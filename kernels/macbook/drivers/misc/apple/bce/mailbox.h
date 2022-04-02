#ifndef BCE_MAILBOX_H
#define BCE_MAILBOX_H

#include <linux/completion.h>
#include <linux/pci.h>
#include <linux/timer.h>

struct bce_mailbox {
    void __iomem *reg_mb;

    atomic_t mb_status; // possible statuses: 0 (no msg), 1 (has active msg), 2 (got reply)
    struct completion mb_completion;
    uint64_t mb_result;
};

enum bce_message_type {
    BCE_MB_REGISTER_COMMAND_SQ = 0x7,            // to-device
    BCE_MB_REGISTER_COMMAND_CQ = 0x8,            // to-device
    BCE_MB_REGISTER_COMMAND_QUEUE_REPLY = 0xB,   // to-host
    BCE_MB_SET_FW_PROTOCOL_VERSION = 0xC,        // both
    BCE_MB_SLEEP_NO_STATE = 0x14,                // to-device
    BCE_MB_RESTORE_NO_STATE = 0x15,              // to-device
    BCE_MB_SAVE_STATE_AND_SLEEP = 0x17,          // to-device
    BCE_MB_RESTORE_STATE_AND_WAKE = 0x18,        // to-device
    BCE_MB_SAVE_STATE_AND_SLEEP_FAILURE = 0x19,  // from-device
    BCE_MB_SAVE_RESTORE_STATE_COMPLETE = 0x1A,   // from-device
};

#define BCE_MB_MSG(type, value) (((u64) (type) << 58) | ((value) & 0x3FFFFFFFFFFFFFFLL))
#define BCE_MB_TYPE(v) ((u32) (v >> 58))
#define BCE_MB_VALUE(v) (v & 0x3FFFFFFFFFFFFFFLL)

void bce_mailbox_init(struct bce_mailbox *mb, void __iomem *reg_mb);

int bce_mailbox_send(struct bce_mailbox *mb, u64 msg, u64* recv);

int bce_mailbox_handle_interrupt(struct bce_mailbox *mb);


struct bce_timestamp {
    void __iomem *reg;
    struct timer_list timer;
    struct spinlock stop_sl;
    bool stopped;
};

void bce_timestamp_init(struct bce_timestamp *ts, void __iomem *reg);

void bce_timestamp_start(struct bce_timestamp *ts, bool is_initial);

void bce_timestamp_stop(struct bce_timestamp *ts);

#endif //BCEDRIVER_MAILBOX_H
