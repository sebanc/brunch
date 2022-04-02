#include "queue.h"
#include "apple_bce.h"

#define REG_DOORBELL_BASE 0x44000

struct bce_queue_cq *bce_alloc_cq(struct apple_bce_device *dev, int qid, u32 el_count)
{
    struct bce_queue_cq *q;
    q = kzalloc(sizeof(struct bce_queue_cq), GFP_KERNEL);
    q->qid = qid;
    q->type = BCE_QUEUE_CQ;
    q->el_count = el_count;
    q->data = dma_alloc_coherent(&dev->pci->dev, el_count * sizeof(struct bce_qe_completion),
            &q->dma_handle, GFP_KERNEL);
    if (!q->data) {
        pr_err("DMA queue memory alloc failed\n");
        kfree(q);
        return NULL;
    }
    return q;
}

void bce_get_cq_memcfg(struct bce_queue_cq *cq, struct bce_queue_memcfg *cfg)
{
    cfg->qid = (u16) cq->qid;
    cfg->el_count = (u16) cq->el_count;
    cfg->vector_or_cq = 0;
    cfg->_pad = 0;
    cfg->addr = cq->dma_handle;
    cfg->length = cq->el_count * sizeof(struct bce_qe_completion);
}

void bce_free_cq(struct apple_bce_device *dev, struct bce_queue_cq *cq)
{
    dma_free_coherent(&dev->pci->dev, cq->el_count * sizeof(struct bce_qe_completion), cq->data, cq->dma_handle);
    kfree(cq);
}

static void bce_handle_cq_completion(struct apple_bce_device *dev, struct bce_qe_completion *e, size_t *ce)
{
    struct bce_queue *target;
    struct bce_queue_sq *target_sq;
    struct bce_sq_completion_data *cmpl;
    if (e->qid >= BCE_MAX_QUEUE_COUNT) {
        pr_err("Device sent a response for qid (%u) >= BCE_MAX_QUEUE_COUNT\n", e->qid);
        return;
    }
    target = dev->queues[e->qid];
    if (!target || target->type != BCE_QUEUE_SQ) {
        pr_err("Device sent a response for qid (%u), which does not exist\n", e->qid);
        return;
    }
    target_sq = (struct bce_queue_sq *) target;
    if (target_sq->completion_tail != e->completion_index) {
        pr_err("Completion index mismatch; this is likely going to make this driver unusable\n");
        return;
    }
    if (!target_sq->has_pending_completions) {
        target_sq->has_pending_completions = true;
        dev->int_sq_list[(*ce)++] = target_sq;
    }
    cmpl = &target_sq->completion_data[e->completion_index];
    cmpl->status = e->status;
    cmpl->data_size = e->data_size;
    cmpl->result = e->result;
    wmb();
    target_sq->completion_tail = (target_sq->completion_tail + 1) % target_sq->el_count;
}

void bce_handle_cq_completions(struct apple_bce_device *dev, struct bce_queue_cq *cq)
{
    size_t ce = 0;
    struct bce_qe_completion *e;
    struct bce_queue_sq *sq;
    e = bce_cq_element(cq, cq->index);
    if (!(e->flags & BCE_COMPLETION_FLAG_PENDING))
        return;
    mb();
    while (true) {
        e = bce_cq_element(cq, cq->index);
        if (!(e->flags & BCE_COMPLETION_FLAG_PENDING))
            break;
        // pr_info("apple-bce: compl: %i: %i %llx %llx", e->qid, e->status, e->data_size, e->result);
        bce_handle_cq_completion(dev, e, &ce);
        e->flags = 0;
        cq->index = (cq->index + 1) % cq->el_count;
    }
    mb();
    iowrite32(cq->index, (u32 *) ((u8 *) dev->reg_mem_dma +  REG_DOORBELL_BASE) + cq->qid);
    while (ce) {
        --ce;
        sq = dev->int_sq_list[ce];
        sq->completion(sq);
        sq->has_pending_completions = false;
    }
}


struct bce_queue_sq *bce_alloc_sq(struct apple_bce_device *dev, int qid, u32 el_size, u32 el_count,
        bce_sq_completion compl, void *userdata)
{
    struct bce_queue_sq *q;
    q = kzalloc(sizeof(struct bce_queue_sq), GFP_KERNEL);
    q->qid = qid;
    q->type = BCE_QUEUE_SQ;
    q->el_size = el_size;
    q->el_count = el_count;
    q->data = dma_alloc_coherent(&dev->pci->dev, el_count * el_size,
                                 &q->dma_handle, GFP_KERNEL);
    q->completion = compl;
    q->userdata = userdata;
    q->completion_data = kzalloc(sizeof(struct bce_sq_completion_data) * el_count, GFP_KERNEL);
    q->reg_mem_dma = dev->reg_mem_dma;
    atomic_set(&q->available_commands, el_count - 1);
    init_completion(&q->available_command_completion);
    atomic_set(&q->available_command_completion_waiting_count, 0);
    if (!q->data) {
        pr_err("DMA queue memory alloc failed\n");
        kfree(q);
        return NULL;
    }
    return q;
}

void bce_get_sq_memcfg(struct bce_queue_sq *sq, struct bce_queue_cq *cq, struct bce_queue_memcfg *cfg)
{
    cfg->qid = (u16) sq->qid;
    cfg->el_count = (u16) sq->el_count;
    cfg->vector_or_cq = (u16) cq->qid;
    cfg->_pad = 0;
    cfg->addr = sq->dma_handle;
    cfg->length = sq->el_count * sq->el_size;
}

void bce_free_sq(struct apple_bce_device *dev, struct bce_queue_sq *sq)
{
    dma_free_coherent(&dev->pci->dev, sq->el_count * sq->el_size, sq->data, sq->dma_handle);
    kfree(sq);
}

int bce_reserve_submission(struct bce_queue_sq *sq, unsigned long *timeout)
{
    while (atomic_dec_if_positive(&sq->available_commands) < 0) {
        if (!timeout || !*timeout)
            return -EAGAIN;
        atomic_inc(&sq->available_command_completion_waiting_count);
        *timeout = wait_for_completion_timeout(&sq->available_command_completion, *timeout);
        if (!*timeout) {
            if (atomic_dec_if_positive(&sq->available_command_completion_waiting_count) < 0)
                try_wait_for_completion(&sq->available_command_completion); /* consume the pending completion */
        }
    }
    return 0;
}

void bce_cancel_submission_reservation(struct bce_queue_sq *sq)
{
    atomic_inc(&sq->available_commands);
}

void *bce_next_submission(struct bce_queue_sq *sq)
{
    void *ret = bce_sq_element(sq, sq->tail);
    sq->tail = (sq->tail + 1) % sq->el_count;
    return ret;
}

void bce_submit_to_device(struct bce_queue_sq *sq)
{
    mb();
    iowrite32(sq->tail, (u32 *) ((u8 *) sq->reg_mem_dma +  REG_DOORBELL_BASE) + sq->qid);
}

void bce_notify_submission_complete(struct bce_queue_sq *sq)
{
    sq->head = (sq->head + 1) % sq->el_count;
    atomic_inc(&sq->available_commands);
    if (atomic_dec_if_positive(&sq->available_command_completion_waiting_count) >= 0) {
        complete(&sq->available_command_completion);
    }
}

void bce_set_submission_single(struct bce_qe_submission *element, dma_addr_t addr, size_t size)
{
    element->addr = addr;
    element->length = size;
    element->segl_addr = element->segl_length = 0;
}

static void bce_cmdq_completion(struct bce_queue_sq *q);

struct bce_queue_cmdq *bce_alloc_cmdq(struct apple_bce_device *dev, int qid, u32 el_count)
{
    struct bce_queue_cmdq *q;
    q = kzalloc(sizeof(struct bce_queue_cmdq), GFP_KERNEL);
    q->sq = bce_alloc_sq(dev, qid, BCE_CMD_SIZE, el_count, bce_cmdq_completion, q);
    if (!q->sq) {
        kfree(q);
        return NULL;
    }
    spin_lock_init(&q->lck);
    q->tres = kzalloc(sizeof(struct bce_queue_cmdq_result_el*) * el_count, GFP_KERNEL);
    if (!q->tres) {
        kfree(q);
        return NULL;
    }
    return q;
}

void bce_free_cmdq(struct apple_bce_device *dev, struct bce_queue_cmdq *cmdq)
{
    bce_free_sq(dev, cmdq->sq);
    kfree(cmdq->tres);
    kfree(cmdq);
}

void bce_cmdq_completion(struct bce_queue_sq *q)
{
    struct bce_queue_cmdq_result_el *el;
    struct bce_queue_cmdq *cmdq = q->userdata;
    struct bce_sq_completion_data *result;

    spin_lock(&cmdq->lck);
    while ((result = bce_next_completion(q))) {
        el = cmdq->tres[cmdq->sq->head];
        if (el) {
            el->result = result->result;
            el->status = result->status;
            mb();
            complete(&el->cmpl);
        } else {
            pr_err("apple-bce: Unexpected command queue completion\n");
        }
        cmdq->tres[cmdq->sq->head] = NULL;
        bce_notify_submission_complete(q);
    }
    spin_unlock(&cmdq->lck);
}

static __always_inline void *bce_cmd_start(struct bce_queue_cmdq *cmdq, struct bce_queue_cmdq_result_el *res)
{
    void *ret;
    unsigned long timeout;
    init_completion(&res->cmpl);
    mb();

    timeout = msecs_to_jiffies(1000L * 60 * 5); /* wait for up to ~5 minutes */
    if (bce_reserve_submission(cmdq->sq, &timeout))
        return NULL;

    spin_lock(&cmdq->lck);
    cmdq->tres[cmdq->sq->tail] = res;
    ret = bce_next_submission(cmdq->sq);
    return ret;
}

static __always_inline void bce_cmd_finish(struct bce_queue_cmdq *cmdq, struct bce_queue_cmdq_result_el *res)
{
    bce_submit_to_device(cmdq->sq);
    spin_unlock(&cmdq->lck);

    wait_for_completion(&res->cmpl);
    mb();
}

u32 bce_cmd_register_queue(struct bce_queue_cmdq *cmdq, struct bce_queue_memcfg *cfg, const char *name, bool isdirout)
{
    struct bce_queue_cmdq_result_el res;
    struct bce_cmdq_register_memory_queue_cmd *cmd = bce_cmd_start(cmdq, &res);
    if (!cmd)
        return (u32) -1;
    cmd->cmd = BCE_CMD_REGISTER_MEMORY_QUEUE;
    cmd->flags = (u16) ((name ? 2 : 0) | (isdirout ? 1 : 0));
    cmd->qid = cfg->qid;
    cmd->el_count = cfg->el_count;
    cmd->vector_or_cq = cfg->vector_or_cq;
    memset(cmd->name, 0, sizeof(cmd->name));
    if (name) {
        cmd->name_len = (u16) min(strlen(name), (size_t) sizeof(cmd->name));
        memcpy(cmd->name, name, cmd->name_len);
    } else {
        cmd->name_len = 0;
    }
    cmd->addr = cfg->addr;
    cmd->length = cfg->length;

    bce_cmd_finish(cmdq, &res);
    return res.status;
}

u32 bce_cmd_unregister_memory_queue(struct bce_queue_cmdq *cmdq, u16 qid)
{
    struct bce_queue_cmdq_result_el res;
    struct bce_cmdq_simple_memory_queue_cmd *cmd = bce_cmd_start(cmdq, &res);
    if (!cmd)
        return (u32) -1;
    cmd->cmd = BCE_CMD_UNREGISTER_MEMORY_QUEUE;
    cmd->flags = 0;
    cmd->qid = qid;
    bce_cmd_finish(cmdq, &res);
    return res.status;
}

u32 bce_cmd_flush_memory_queue(struct bce_queue_cmdq *cmdq, u16 qid)
{
    struct bce_queue_cmdq_result_el res;
    struct bce_cmdq_simple_memory_queue_cmd *cmd = bce_cmd_start(cmdq, &res);
    if (!cmd)
        return (u32) -1;
    cmd->cmd = BCE_CMD_FLUSH_MEMORY_QUEUE;
    cmd->flags = 0;
    cmd->qid = qid;
    bce_cmd_finish(cmdq, &res);
    return res.status;
}


struct bce_queue_cq *bce_create_cq(struct apple_bce_device *dev, u32 el_count)
{
    struct bce_queue_cq *cq;
    struct bce_queue_memcfg cfg;
    int qid = ida_simple_get(&dev->queue_ida, BCE_QUEUE_USER_MIN, BCE_QUEUE_USER_MAX, GFP_KERNEL);
    if (qid < 0)
        return NULL;
    cq = bce_alloc_cq(dev, qid, el_count);
    if (!cq)
        return NULL;
    bce_get_cq_memcfg(cq, &cfg);
    if (bce_cmd_register_queue(dev->cmd_cmdq, &cfg, NULL, false) != 0) {
        pr_err("apple-bce: CQ registration failed (%i)", qid);
        bce_free_cq(dev, cq);
        ida_simple_remove(&dev->queue_ida, (uint) qid);
        return NULL;
    }
    dev->queues[qid] = (struct bce_queue *) cq;
    return cq;
}

struct bce_queue_sq *bce_create_sq(struct apple_bce_device *dev, struct bce_queue_cq *cq, const char *name, u32 el_count,
        int direction, bce_sq_completion compl, void *userdata)
{
    struct bce_queue_sq *sq;
    struct bce_queue_memcfg cfg;
    int qid;
    if (cq == NULL)
        return NULL; /* cq can not be null */
    if (name == NULL)
        return NULL; /* name can not be null */
    if (direction != DMA_TO_DEVICE && direction != DMA_FROM_DEVICE)
        return NULL; /* unsupported direction */
    qid = ida_simple_get(&dev->queue_ida, BCE_QUEUE_USER_MIN, BCE_QUEUE_USER_MAX, GFP_KERNEL);
    if (qid < 0)
        return NULL;
    sq = bce_alloc_sq(dev, qid, sizeof(struct bce_qe_submission), el_count, compl, userdata);
    if (!sq)
        return NULL;
    bce_get_sq_memcfg(sq, cq, &cfg);
    if (bce_cmd_register_queue(dev->cmd_cmdq, &cfg, name, direction != DMA_FROM_DEVICE) != 0) {
        pr_err("apple-bce: SQ registration failed (%i)", qid);
        bce_free_sq(dev, sq);
        ida_simple_remove(&dev->queue_ida, (uint) qid);
        return NULL;
    }
    spin_lock(&dev->queues_lock);
    dev->queues[qid] = (struct bce_queue *) sq;
    spin_unlock(&dev->queues_lock);
    return sq;
}

void bce_destroy_cq(struct apple_bce_device *dev, struct bce_queue_cq *cq)
{
    if (!dev->is_being_removed && bce_cmd_unregister_memory_queue(dev->cmd_cmdq, (u16) cq->qid))
        pr_err("apple-bce: CQ unregister failed");
    spin_lock(&dev->queues_lock);
    dev->queues[cq->qid] = NULL;
    spin_unlock(&dev->queues_lock);
    ida_simple_remove(&dev->queue_ida, (uint) cq->qid);
    bce_free_cq(dev, cq);
}

void bce_destroy_sq(struct apple_bce_device *dev, struct bce_queue_sq *sq)
{
    if (!dev->is_being_removed && bce_cmd_unregister_memory_queue(dev->cmd_cmdq, (u16) sq->qid))
        pr_err("apple-bce: CQ unregister failed");
    spin_lock(&dev->queues_lock);
    dev->queues[sq->qid] = NULL;
    spin_unlock(&dev->queues_lock);
    ida_simple_remove(&dev->queue_ida, (uint) sq->qid);
    bce_free_sq(dev, sq);
}