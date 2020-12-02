#include "mailbox.h"
#include <linux/atomic.h>
#include "pci.h"

#define REG_MBOX_OUT_BASE 0x820
#define REG_MBOX_REPLY_COUNTER 0x108
#define REG_MBOX_REPLY_BASE 0x810
#define REG_TIMESTAMP_BASE 0xC000

#define BCE_MBOX_TIMEOUT_MS 200

void bce_mailbox_init(struct bce_mailbox *mb, void __iomem *reg_mb)
{
    mb->reg_mb = reg_mb;
    init_completion(&mb->mb_completion);
}

int bce_mailbox_send(struct bce_mailbox *mb, u64 msg, u64* recv)
{
    u32 __iomem *regb;

    if (atomic_cmpxchg(&mb->mb_status, 0, 1) != 0) {
        return -EEXIST; // We don't support two messages at once
    }
    reinit_completion(&mb->mb_completion);

    pr_debug("bce_mailbox_send: %llx\n", msg);
    regb = (u32*) ((u8*) mb->reg_mb + REG_MBOX_OUT_BASE);
    iowrite32((u32) msg, regb);
    iowrite32((u32) (msg >> 32), regb + 1);
    iowrite32(0, regb + 2);
    iowrite32(0, regb + 3);

    wait_for_completion_timeout(&mb->mb_completion, msecs_to_jiffies(BCE_MBOX_TIMEOUT_MS));
    if (atomic_read(&mb->mb_status) != 2) { // Didn't get the reply
        atomic_set(&mb->mb_status, 0);
        return -ETIMEDOUT;
    }

    *recv = mb->mb_result;
    pr_debug("bce_mailbox_send: reply %llx\n", *recv);

    atomic_set(&mb->mb_status, 0);
    return 0;
}

static int bce_mailbox_retrive_response(struct bce_mailbox *mb)
{
    u32 __iomem *regb;
    u32 lo, hi;
    int count, counter;
    u32 res = ioread32((u8*) mb->reg_mb + REG_MBOX_REPLY_COUNTER);
    count = (res >> 20) & 0xf;
    counter = count;
    pr_debug("bce_mailbox_retrive_response count=%i\n", count);
    while (counter--) {
        regb = (u32*) ((u8*) mb->reg_mb + REG_MBOX_REPLY_BASE);
        lo = ioread32(regb);
        hi = ioread32(regb + 1);
        ioread32(regb + 2);
        ioread32(regb + 3);
        pr_debug("bce_mailbox_retrive_response %llx\n", ((u64) hi << 32) | lo);
        mb->mb_result = ((u64) hi << 32) | lo;
    }
    return count > 0 ? 0 : -ENODATA;
}

int bce_mailbox_handle_interrupt(struct bce_mailbox *mb)
{
    int status = bce_mailbox_retrive_response(mb);
    if (!status) {
        atomic_set(&mb->mb_status, 2);
        complete(&mb->mb_completion);
    }
    return status;
}

static void bc_send_timestamp(struct timer_list *tl);

void bce_timestamp_init(struct bce_timestamp *ts, void __iomem *reg)
{
    u32 __iomem *regb;

    spin_lock_init(&ts->stop_sl);
    ts->stopped = false;

    ts->reg = reg;

    regb = (u32*) ((u8*) ts->reg + REG_TIMESTAMP_BASE);

    ioread32(regb);
    mb();

    timer_setup(&ts->timer, bc_send_timestamp, 0);
}

void bce_timestamp_start(struct bce_timestamp *ts, bool is_initial)
{
    unsigned long flags;
    u32 __iomem *regb = (u32*) ((u8*) ts->reg + REG_TIMESTAMP_BASE);

    if (is_initial) {
        iowrite32((u32) -4, regb + 2);
        iowrite32((u32) -1, regb);
    } else {
        iowrite32((u32) -3, regb + 2);
        iowrite32((u32) -1, regb);
    }

    spin_lock_irqsave(&ts->stop_sl, flags);
    ts->stopped = false;
    spin_unlock_irqrestore(&ts->stop_sl, flags);
    mod_timer(&ts->timer, jiffies + msecs_to_jiffies(150));
}

void bce_timestamp_stop(struct bce_timestamp *ts)
{
    unsigned long flags;
    u32 __iomem *regb = (u32*) ((u8*) ts->reg + REG_TIMESTAMP_BASE);

    spin_lock_irqsave(&ts->stop_sl, flags);
    ts->stopped = true;
    spin_unlock_irqrestore(&ts->stop_sl, flags);
    del_timer_sync(&ts->timer);

    iowrite32((u32) -2, regb + 2);
    iowrite32((u32) -1, regb);
}

static void bc_send_timestamp(struct timer_list *tl)
{
    struct bce_timestamp *ts;
    unsigned long flags;
    u32 __iomem *regb;
    ktime_t bt;

    ts = container_of(tl, struct bce_timestamp, timer);
    regb = (u32*) ((u8*) ts->reg + REG_TIMESTAMP_BASE);
    local_irq_save(flags);
    ioread32(regb + 2);
    mb();
    bt = ktime_get_boottime();
    iowrite32((u32) bt, regb + 2);
    iowrite32((u32) (bt >> 32), regb);

    spin_lock(&ts->stop_sl);
    if (!ts->stopped)
        mod_timer(&ts->timer, jiffies + msecs_to_jiffies(150));
    spin_unlock(&ts->stop_sl);
    local_irq_restore(flags);
}