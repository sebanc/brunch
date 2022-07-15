#define PRD_SIZE_MASK            0xffffff
#define PRD_END_FLAG             0x1000000
struct ithc_phys_region_desc {
	u64 addr; // physical addr/1024
	u32 size; // num bytes, PRD_END_FLAG marks last prd for data split over multiple prds
	u32 unused;
};

#define DMA_RX_CODE_INPUT_REPORT          3
#define DMA_RX_CODE_FEATURE_REPORT        4
#define DMA_RX_CODE_REPORT_DESCRIPTOR     5
#define DMA_RX_CODE_RESET                 7
struct ithc_dma_rx_header {
	u32 code;
	u32 data_size;
	u32 _unknown[14];
};

#define DMA_TX_CODE_SET_FEATURE           3
#define DMA_TX_CODE_GET_FEATURE           4
#define DMA_TX_CODE_OUTPUT_REPORT         5
#define DMA_TX_CODE_GET_REPORT_DESCRIPTOR 7
struct ithc_dma_tx_header {
	u32 code;
	u32 data_size;
};

#define HID_REPORT_ID_SINGLETOUCH 0x40
struct ithc_hid_report_singletouch {
	u8 report_id;
	u8 button;
	u16 x;
	u16 y;
};

#define HID_FEATURE_ID_MULTITOUCH 5
struct ithc_hid_feature_multitouch {
	u8 feature_id;
	u8 enable;
};

struct ithc_dma_prd_buffer {
	void *addr;
	dma_addr_t dma_addr;
	u32 size;
	u32 num_pages; // per data buffer
	enum dma_data_direction dir;
};

struct ithc_dma_data_buffer {
	void *addr;
	struct sg_table *sgt;
	int active_idx;
	u32 data_size;
};

struct ithc_dma_tx {
	struct mutex mutex;
	u32 max_size;
	struct ithc_dma_prd_buffer prds;
	struct ithc_dma_data_buffer buf;
};

struct ithc_dma_rx {
	struct mutex mutex;
	u32 num_received;
	loff_t pos;
	struct ithc_api *api;
	struct ithc_dma_prd_buffer prds;
	struct ithc_dma_data_buffer bufs[NUM_RX_ALLOC];
	wait_queue_head_t wait;
};

int ithc_dma_rx_init(struct ithc *ithc, u8 channel, const char *devname);
void ithc_dma_rx_enable(struct ithc *ithc, u8 channel);
int ithc_dma_tx_init(struct ithc *ithc);
int ithc_dma_rx(struct ithc *ithc, u8 channel);
int ithc_dma_tx(struct ithc *ithc, u32 cmdcode, u32 datasize, void *cmddata);
void *ithc_data_kmap(struct ithc_dma_data_buffer *b);
void ithc_data_kunmap(struct ithc_dma_data_buffer *b, void *p);
int ithc_set_multitouch(struct ithc *ithc, bool enable);

