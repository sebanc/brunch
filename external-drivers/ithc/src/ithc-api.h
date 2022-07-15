struct ithc_api {
	struct ithc *ithc;
	struct ithc_dma_rx *rx;
	struct miscdevice m;
	atomic_t open_count;
};

struct ithc_api_header {
	u8 hdr_size;
	u8 reserved[3];
	u32 msg_num;
	u32 size;
};

int ithc_api_init(struct ithc *ithc, struct ithc_dma_rx *rx, const char *name);

