#ifndef BCE_VHCI_QUEUE_H
#define BCE_VHCI_QUEUE_H

#include <linux/completion.h>
#include "../queue.h"

#define VHCI_EVENT_QUEUE_EL_COUNT 256
#define VHCI_EVENT_PENDING_COUNT 32

struct bce_vhci;
struct bce_vhci_event_queue;

enum bce_vhci_message_status {
    BCE_VHCI_SUCCESS = 1,
    BCE_VHCI_ERROR = 2,
    BCE_VHCI_USB_PIPE_STALL = 3,
    BCE_VHCI_ABORT = 4,
    BCE_VHCI_BAD_ARGUMENT = 5,
    BCE_VHCI_OVERRUN = 6,
    BCE_VHCI_INTERNAL_ERROR = 7,
    BCE_VHCI_NO_POWER = 8,
    BCE_VHCI_UNSUPPORTED = 9
};
struct bce_vhci_message {
    u16 cmd;
    u16 status; // bce_vhci_message_status
    u32 param1;
    u64 param2;
};

struct bce_vhci_message_queue {
    struct bce_queue_cq *cq;
    struct bce_queue_sq *sq;
    struct bce_vhci_message *data;
    dma_addr_t dma_addr;
};
typedef void (*bce_vhci_event_queue_callback)(struct bce_vhci_event_queue *q, struct bce_vhci_message *msg);
struct bce_vhci_event_queue {
    struct bce_vhci *vhci;
    struct bce_queue_sq *sq;
    struct bce_vhci_message *data;
    dma_addr_t dma_addr;
    bce_vhci_event_queue_callback cb;
    struct completion queue_empty_completion;
};
struct bce_vhci_command_queue_completion {
    struct bce_vhci_message *result;
    struct completion completion;
};
struct bce_vhci_command_queue {
    struct bce_vhci_message_queue *mq;
    struct bce_vhci_command_queue_completion completion;
    struct spinlock completion_lock;
    struct mutex mutex;
};

int bce_vhci_message_queue_create(struct bce_vhci *vhci, struct bce_vhci_message_queue *ret, const char *name);
void bce_vhci_message_queue_destroy(struct bce_vhci *vhci, struct bce_vhci_message_queue *q);
void bce_vhci_message_queue_write(struct bce_vhci_message_queue *q, struct bce_vhci_message *req);

int __bce_vhci_event_queue_create(struct bce_vhci *vhci, struct bce_vhci_event_queue *ret, const char *name,
        bce_sq_completion compl);
int bce_vhci_event_queue_create(struct bce_vhci *vhci, struct bce_vhci_event_queue *ret, const char *name,
        bce_vhci_event_queue_callback cb);
void bce_vhci_event_queue_destroy(struct bce_vhci *vhci, struct bce_vhci_event_queue *q);
void bce_vhci_event_queue_submit_pending(struct bce_vhci_event_queue *q, size_t count);
void bce_vhci_event_queue_pause(struct bce_vhci_event_queue *q);
void bce_vhci_event_queue_resume(struct bce_vhci_event_queue *q);

void bce_vhci_command_queue_create(struct bce_vhci_command_queue *ret, struct bce_vhci_message_queue *mq);
void bce_vhci_command_queue_destroy(struct bce_vhci_command_queue *cq);
int bce_vhci_command_queue_execute(struct bce_vhci_command_queue *cq, struct bce_vhci_message *req,
        struct bce_vhci_message *res, unsigned long timeout);
void bce_vhci_command_queue_deliver_completion(struct bce_vhci_command_queue *cq, struct bce_vhci_message *msg);

#endif //BCE_VHCI_QUEUE_H
