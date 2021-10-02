#ifndef AAUDIO_PROTOCOL_BCE_H
#define AAUDIO_PROTOCOL_BCE_H

#include "protocol.h"
#include "../queue.h"

#define AAUDIO_BCE_QUEUE_ELEMENT_SIZE 0x1000
#define AAUDIO_BCE_QUEUE_ELEMENT_COUNT 20

#define AAUDIO_BCE_QUEUE_TAG_COUNT 1000

struct aaudio_device;

struct aaudio_bce_queue_entry {
    struct aaudio_msg *msg;
    struct completion *cmpl;
};
struct aaudio_bce_queue {
    struct bce_queue_cq *cq;
    struct bce_queue_sq *sq;
    void *data;
    dma_addr_t dma_addr;
    size_t data_head, data_tail;
    size_t el_size, el_count;
};
struct aaudio_bce {
    struct bce_queue_cq *cq;
    struct aaudio_bce_queue qin;
    struct aaudio_bce_queue qout;
    int tag_num;
    struct aaudio_bce_queue_entry *pending_entries[AAUDIO_BCE_QUEUE_TAG_COUNT];
    struct spinlock spinlock;
};

struct aaudio_send_ctx {
    int status;
    int tag_n;
    unsigned long irq_flags;
    struct aaudio_msg msg;
    unsigned long timeout;
};

int aaudio_bce_init(struct aaudio_device *dev);
int __aaudio_send_prepare(struct aaudio_bce *b, struct aaudio_send_ctx *ctx, char *tag);
void __aaudio_send(struct aaudio_bce *b, struct aaudio_send_ctx *ctx);
int __aaudio_send_cmd_sync(struct aaudio_bce *b, struct aaudio_send_ctx *ctx, struct aaudio_msg *reply);

#define aaudio_send_with_tag(a, ctx, tag, tout, fn, ...) ({ \
    (ctx)->timeout = msecs_to_jiffies(tout); \
    (ctx)->status = __aaudio_send_prepare(&(a)->bcem, (ctx), (tag)); \
    if (!(ctx)->status) { \
        fn(&(ctx)->msg, ##__VA_ARGS__); \
        __aaudio_send(&(a)->bcem, (ctx)); \
    } \
    (ctx)->status; \
})
#define aaudio_send(a, ctx, tout, fn, ...) aaudio_send_with_tag(a, ctx, NULL, tout, fn, ##__VA_ARGS__)

#define aaudio_send_cmd_sync(a, ctx, reply, tout, fn, ...) ({ \
    (ctx)->timeout = msecs_to_jiffies(tout); \
    (ctx)->status = __aaudio_send_prepare(&(a)->bcem, (ctx), NULL); \
    if (!(ctx)->status) { \
        fn(&(ctx)->msg, ##__VA_ARGS__); \
        (ctx)->status = __aaudio_send_cmd_sync(&(a)->bcem, (ctx), (reply)); \
    } \
    (ctx)->status; \
})

struct aaudio_msg aaudio_reply_alloc(void);
void aaudio_reply_free(struct aaudio_msg *reply);

#endif //AAUDIO_PROTOCOL_BCE_H
