#ifndef BCE_QUEUE_DMA_H
#define BCE_QUEUE_DMA_H

#include <linux/pci.h>

struct bce_qe_submission;

struct bce_segment_list_header {
    u64 element_count;
    u64 data_size;

    u64 next_segl_addr;
    u64 next_segl_length;
};
struct bce_segment_list_element {
    u64 addr;
    u64 length;
};

struct bce_segment_list_element_hostinfo {
    struct bce_segment_list_element_hostinfo *next;
    void *page_start;
    size_t page_count;
    dma_addr_t dma_start;
};


struct bce_dma_buffer {
    enum dma_data_direction direction;
    struct sg_table scatterlist;
    struct bce_segment_list_element_hostinfo *seglist_hostinfo;
};

/* NOTE: Takes ownership of the sg_table if it succeeds. Ownership is not transferred on failure. */
int bce_map_dma_buffer(struct device *dev, struct bce_dma_buffer *buf, struct sg_table scatterlist,
        enum dma_data_direction dir);

/* Creates a buffer from virtual memory (vmalloc) */
int bce_map_dma_buffer_vm(struct device *dev, struct bce_dma_buffer *buf, void *data, size_t len,
        enum dma_data_direction dir);

/* Creates a buffer from kernel memory (kmalloc) */
int bce_map_dma_buffer_km(struct device *dev, struct bce_dma_buffer *buf, void *data, size_t len,
                          enum dma_data_direction dir);

void bce_unmap_dma_buffer(struct device *dev, struct bce_dma_buffer *buf);

int bce_set_submission_buf(struct bce_qe_submission *element, struct bce_dma_buffer *buf, size_t offset, size_t length);

#endif //BCE_QUEUE_DMA_H
