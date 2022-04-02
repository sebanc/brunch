#ifndef BCE_QUEUE_H
#define BCE_QUEUE_H

#include <linux/completion.h>
#include <linux/pci.h>

#define BCE_CMD_SIZE 0x40

struct apple_bce_device;

enum bce_queue_type {
    BCE_QUEUE_CQ, BCE_QUEUE_SQ
};
struct bce_queue {
    int qid;
    int type;
};
struct bce_queue_cq {
    int qid;
    int type;
    u32 el_count;
    dma_addr_t dma_handle;
    void *data;

    u32 index;
};
struct bce_queue_sq;
typedef void (*bce_sq_completion)(struct bce_queue_sq *q);
struct bce_sq_completion_data {
    u32 status;
    u64 data_size;
    u64 result;
};
struct bce_queue_sq {
    int qid;
    int type;
    u32 el_size;
    u32 el_count;
    dma_addr_t dma_handle;
    void *data;
    void *userdata;
    void __iomem *reg_mem_dma;

    atomic_t available_commands;
    struct completion available_command_completion;
    atomic_t available_command_completion_waiting_count;
    u32 head, tail;

    u32 completion_cidx, completion_tail;
    struct bce_sq_completion_data *completion_data;
    bool has_pending_completions;
    bce_sq_completion completion;
};

struct bce_queue_cmdq_result_el {
    struct completion cmpl;
    u32 status;
    u64 result;
};
struct bce_queue_cmdq {
    struct bce_queue_sq *sq;
    struct spinlock lck;
    struct bce_queue_cmdq_result_el **tres;
};

struct bce_queue_memcfg {
    u16 qid;
    u16 el_count;
    u16 vector_or_cq;
    u16 _pad;
    u64 addr;
    u64 length;
};

enum bce_qe_completion_status {
    BCE_COMPLETION_SUCCESS = 0,
    BCE_COMPLETION_ERROR = 1,
    BCE_COMPLETION_ABORTED = 2,
    BCE_COMPLETION_NO_SPACE = 3,
    BCE_COMPLETION_OVERRUN = 4
};
enum bce_qe_completion_flags {
    BCE_COMPLETION_FLAG_PENDING = 0x8000
};
struct bce_qe_completion {
    u64 result;
    u64 data_size;
    u16 qid;
    u16 completion_index;
    u16 status; // bce_qe_completion_status
    u16 flags;  // bce_qe_completion_flags
};

struct bce_qe_submission {
    u64 length;
    u64 addr;

    u64 segl_addr;
    u64 segl_length;
};

enum bce_cmdq_command {
    BCE_CMD_REGISTER_MEMORY_QUEUE = 0x20,
    BCE_CMD_UNREGISTER_MEMORY_QUEUE = 0x30,
    BCE_CMD_FLUSH_MEMORY_QUEUE = 0x40,
    BCE_CMD_SET_MEMORY_QUEUE_PROPERTY = 0x50
};
struct bce_cmdq_simple_memory_queue_cmd {
    u16 cmd; // bce_cmdq_command
    u16 flags;
    u16 qid;
};
struct bce_cmdq_register_memory_queue_cmd {
    u16 cmd; // bce_cmdq_command
    u16 flags;
    u16 qid;
    u16 _pad;
    u16 el_count;
    u16 vector_or_cq;
    u16 _pad2;
    u16 name_len;
    char name[0x20];
    u64 addr;
    u64 length;
};

static __always_inline void *bce_sq_element(struct bce_queue_sq *q, int i) {
    return (void *) ((u8 *) q->data + q->el_size * i);
}
static __always_inline void *bce_cq_element(struct bce_queue_cq *q, int i) {
    return (void *) ((struct bce_qe_completion *) q->data + i);
}

static __always_inline struct bce_sq_completion_data *bce_next_completion(struct bce_queue_sq *sq) {
    struct bce_sq_completion_data *res;
    rmb();
    if (sq->completion_cidx == sq->completion_tail)
        return NULL;
    res = &sq->completion_data[sq->completion_cidx];
    sq->completion_cidx = (sq->completion_cidx + 1) % sq->el_count;
    return res;
}

struct bce_queue_cq *bce_alloc_cq(struct apple_bce_device *dev, int qid, u32 el_count);
void bce_get_cq_memcfg(struct bce_queue_cq *cq, struct bce_queue_memcfg *cfg);
void bce_free_cq(struct apple_bce_device *dev, struct bce_queue_cq *cq);
void bce_handle_cq_completions(struct apple_bce_device *dev, struct bce_queue_cq *cq);

struct bce_queue_sq *bce_alloc_sq(struct apple_bce_device *dev, int qid, u32 el_size, u32 el_count,
        bce_sq_completion compl, void *userdata);
void bce_get_sq_memcfg(struct bce_queue_sq *sq, struct bce_queue_cq *cq, struct bce_queue_memcfg *cfg);
void bce_free_sq(struct apple_bce_device *dev, struct bce_queue_sq *sq);
int bce_reserve_submission(struct bce_queue_sq *sq, unsigned long *timeout);
void bce_cancel_submission_reservation(struct bce_queue_sq *sq);
void *bce_next_submission(struct bce_queue_sq *sq);
void bce_submit_to_device(struct bce_queue_sq *sq);
void bce_notify_submission_complete(struct bce_queue_sq *sq);

void bce_set_submission_single(struct bce_qe_submission *element, dma_addr_t addr, size_t size);

struct bce_queue_cmdq *bce_alloc_cmdq(struct apple_bce_device *dev, int qid, u32 el_count);
void bce_free_cmdq(struct apple_bce_device *dev, struct bce_queue_cmdq *cmdq);

u32 bce_cmd_register_queue(struct bce_queue_cmdq *cmdq, struct bce_queue_memcfg *cfg, const char *name, bool isdirout);
u32 bce_cmd_unregister_memory_queue(struct bce_queue_cmdq *cmdq, u16 qid);
u32 bce_cmd_flush_memory_queue(struct bce_queue_cmdq *cmdq, u16 qid);


/* User API - Creates and registers the queue */

struct bce_queue_cq *bce_create_cq(struct apple_bce_device *dev, u32 el_count);
struct bce_queue_sq *bce_create_sq(struct apple_bce_device *dev, struct bce_queue_cq *cq, const char *name, u32 el_count,
        int direction, bce_sq_completion compl, void *userdata);
void bce_destroy_cq(struct apple_bce_device *dev, struct bce_queue_cq *cq);
void bce_destroy_sq(struct apple_bce_device *dev, struct bce_queue_sq *sq);

#endif //BCEDRIVER_MAILBOX_H
