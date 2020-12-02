#include "protocol_bce.h"

#include "audio.h"

static void aaudio_bce_out_queue_completion(struct bce_queue_sq *sq);
static void aaudio_bce_in_queue_completion(struct bce_queue_sq *sq);
static int aaudio_bce_queue_init(struct aaudio_device *dev, struct aaudio_bce_queue *q, const char *name, int direction,
                                 bce_sq_completion cfn);
void aaudio_bce_in_queue_submit_pending(struct aaudio_bce_queue *q, size_t count);

int aaudio_bce_init(struct aaudio_device *dev)
{
    int status;
    struct aaudio_bce *bce = &dev->bcem;
    bce->cq = bce_create_cq(dev->bce, 0x80);
    spin_lock_init(&bce->spinlock);
    if (!bce->cq)
        return -EINVAL;
    if ((status = aaudio_bce_queue_init(dev, &bce->qout, "com.apple.BridgeAudio.IntelToARM", DMA_TO_DEVICE,
            aaudio_bce_out_queue_completion))) {
        return status;
    }
    if ((status = aaudio_bce_queue_init(dev, &bce->qin, "com.apple.BridgeAudio.ARMToIntel", DMA_FROM_DEVICE,
            aaudio_bce_in_queue_completion))) {
        return status;
    }
    aaudio_bce_in_queue_submit_pending(&bce->qin, bce->qin.el_count);
    return 0;
}

int aaudio_bce_queue_init(struct aaudio_device *dev, struct aaudio_bce_queue *q, const char *name, int direction,
        bce_sq_completion cfn)
{
    q->cq = dev->bcem.cq;
    q->el_size = AAUDIO_BCE_QUEUE_ELEMENT_SIZE;
    q->el_count = AAUDIO_BCE_QUEUE_ELEMENT_COUNT;
    /* NOTE: The Apple impl uses 0x80 as the queue size, however we use 21 (in fact 20) to simplify the impl */
    q->sq = bce_create_sq(dev->bce, q->cq, name, (u32) (q->el_count + 1), direction, cfn, dev);
    if (!q->sq)
        return -EINVAL;

    q->data = dma_alloc_coherent(&dev->bce->pci->dev, q->el_size * q->el_count, &q->dma_addr, GFP_KERNEL);
    if (!q->data) {
        bce_destroy_sq(dev->bce, q->sq);
        return -EINVAL;
    }
    return 0;
}

static void aaudio_send_create_tag(struct aaudio_bce *b, int *tagn, char tag[4])
{
    char tag_zero[5];
    b->tag_num = (b->tag_num + 1) % AAUDIO_BCE_QUEUE_TAG_COUNT;
    *tagn = b->tag_num;
    snprintf(tag_zero, 5, "S%03d", b->tag_num);
    *((u32 *) tag) = *((u32 *) tag_zero);
}

int __aaudio_send_prepare(struct aaudio_bce *b, struct aaudio_send_ctx *ctx, char *tag)
{
    int status;
    size_t index;
    void *dptr;
    struct aaudio_msg_header *header;
    if ((status = bce_reserve_submission(b->qout.sq, &ctx->timeout)))
        return status;
    spin_lock_irqsave(&b->spinlock, ctx->irq_flags);
    index = b->qout.data_tail;
    dptr = (u8 *) b->qout.data + index * b->qout.el_size;
    ctx->msg.data = dptr;
    header = dptr;
    if (tag)
        *((u32 *) header->tag) = *((u32 *) tag);
    else
        aaudio_send_create_tag(b, &ctx->tag_n, header->tag);
    return 0;
}

void __aaudio_send(struct aaudio_bce *b, struct aaudio_send_ctx *ctx)
{
    struct bce_qe_submission *s = bce_next_submission(b->qout.sq);
#ifdef DEBUG
    pr_debug("aaudio: Sending command data\n");
    print_hex_dump(KERN_DEBUG, "aaudio:OUT ", DUMP_PREFIX_NONE, 32, 1, ctx->msg.data, ctx->msg.size, true);
#endif
    bce_set_submission_single(s, b->qout.dma_addr + (dma_addr_t) (ctx->msg.data - b->qout.data), ctx->msg.size);
    bce_submit_to_device(b->qout.sq);
    b->qout.data_tail = (b->qout.data_tail + 1) % b->qout.el_count;
    spin_unlock_irqrestore(&b->spinlock, ctx->irq_flags);
}

int __aaudio_send_cmd_sync(struct aaudio_bce *b, struct aaudio_send_ctx *ctx, struct aaudio_msg *reply)
{
    struct aaudio_bce_queue_entry ent;
    DECLARE_COMPLETION_ONSTACK(cmpl);
    ent.msg = reply;
    ent.cmpl = &cmpl;
    b->pending_entries[ctx->tag_n] = &ent;
    __aaudio_send(b, ctx); /* unlocks the spinlock */
    ctx->timeout = wait_for_completion_timeout(&cmpl, ctx->timeout);
    if (ctx->timeout == 0) {
        /* Remove the pending queue entry; this will be normally handled by the completion route but
         * during a timeout it won't */
        spin_lock_irqsave(&b->spinlock, ctx->irq_flags);
        if (b->pending_entries[ctx->tag_n] == &ent)
            b->pending_entries[ctx->tag_n] = NULL;
        spin_unlock_irqrestore(&b->spinlock, ctx->irq_flags);
        return -ETIMEDOUT;
    }
    return 0;
}

static void aaudio_handle_reply(struct aaudio_bce *b, struct aaudio_msg *reply)
{
    const char *tag;
    int tagn;
    unsigned long irq_flags;
    char tag_zero[5];
    struct aaudio_bce_queue_entry *entry;

    tag = ((struct aaudio_msg_header *) reply->data)->tag;
    if (tag[0] != 'S') {
        pr_err("aaudio_handle_reply: Unexpected tag: %.4s\n", tag);
        return;
    }
    *((u32 *) tag_zero) = *((u32 *) tag);
    tag_zero[4] = 0;
    if (kstrtoint(&tag_zero[1], 10, &tagn)) {
        pr_err("aaudio_handle_reply: Tag parse failed: %.4s\n", tag);
        return;
    }

    spin_lock_irqsave(&b->spinlock, irq_flags);
    entry = b->pending_entries[tagn];
    if (entry) {
        if (reply->size < entry->msg->size)
            entry->msg->size = reply->size;
        memcpy(entry->msg->data, reply->data, entry->msg->size);
        complete(entry->cmpl);

        b->pending_entries[tagn] = NULL;
    } else {
        pr_err("aaudio_handle_reply: No queued item found for tag: %.4s\n", tag);
    }
    spin_unlock_irqrestore(&b->spinlock, irq_flags);
}

static void aaudio_bce_out_queue_completion(struct bce_queue_sq *sq)
{
    while (bce_next_completion(sq)) {
        //pr_info("aaudio: Send confirmed\n");
        bce_notify_submission_complete(sq);
    }
}

static void aaudio_bce_in_queue_handle_msg(struct aaudio_device *a, struct aaudio_msg *msg);

static void aaudio_bce_in_queue_completion(struct bce_queue_sq *sq)
{
    struct aaudio_msg msg;
    struct aaudio_device *dev = sq->userdata;
    struct aaudio_bce_queue *q = &dev->bcem.qin;
    struct bce_sq_completion_data *c;
    size_t cnt = 0;

    mb();
    while ((c = bce_next_completion(sq))) {
        msg.data = (u8 *) q->data + q->data_head * q->el_size;
        msg.size = c->data_size;
#ifdef DEBUG
        pr_debug("aaudio: Received command data %llx\n", c->data_size);
        print_hex_dump(KERN_DEBUG, "aaudio:IN ", DUMP_PREFIX_NONE, 32, 1, msg.data, min(msg.size, 128UL), true);
#endif
        aaudio_bce_in_queue_handle_msg(dev, &msg);

        q->data_head = (q->data_head + 1) % q->el_count;

        bce_notify_submission_complete(sq);
        ++cnt;
    }
    aaudio_bce_in_queue_submit_pending(q, cnt);
}

static void aaudio_bce_in_queue_handle_msg(struct aaudio_device *a, struct aaudio_msg *msg)
{
    struct aaudio_msg_header *header = (struct aaudio_msg_header *) msg->data;
    if (msg->size < sizeof(struct aaudio_msg_header)) {
        pr_err("aaudio: Msg size smaller than header (%lx)", msg->size);
        return;
    }
    if (header->type == AAUDIO_MSG_TYPE_RESPONSE) {
        aaudio_handle_reply(&a->bcem, msg);
    } else if (header->type == AAUDIO_MSG_TYPE_COMMAND) {
        aaudio_handle_command(a, msg);
    } else if (header->type == AAUDIO_MSG_TYPE_NOTIFICATION) {
        aaudio_handle_notification(a, msg);
    }
}

void aaudio_bce_in_queue_submit_pending(struct aaudio_bce_queue *q, size_t count)
{
    struct bce_qe_submission *s;
    while (count--) {
        if (bce_reserve_submission(q->sq, NULL)) {
            pr_err("aaudio: Failed to reserve an event queue submission\n");
            break;
        }
        s = bce_next_submission(q->sq);
        bce_set_submission_single(s, q->dma_addr + (dma_addr_t) (q->data_tail * q->el_size), q->el_size);
        q->data_tail = (q->data_tail + 1) % q->el_count;
    }
    bce_submit_to_device(q->sq);
}

struct aaudio_msg aaudio_reply_alloc(void)
{
    struct aaudio_msg ret;
    ret.size = AAUDIO_BCE_QUEUE_ELEMENT_SIZE;
    ret.data = kmalloc(ret.size, GFP_KERNEL);
    return ret;
}

void aaudio_reply_free(struct aaudio_msg *reply)
{
    kfree(reply->data);
}
