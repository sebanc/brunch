#include "ithc.h"

static int ithc_dma_prd_alloc(struct ithc *ithc, struct ithc_dma_prd_buffer *p, unsigned num_buffers, unsigned num_pages, enum dma_data_direction dir) {
	p->num_pages = num_pages;
	p->dir = dir;
	p->size = round_up(num_buffers * num_pages * sizeof(struct ithc_phys_region_desc), PAGE_SIZE);
	p->addr = dmam_alloc_coherent(&ithc->pci->dev, p->size, &p->dma_addr, GFP_KERNEL);
	if (!p->addr) return -ENOMEM;
	if (p->dma_addr & (PAGE_SIZE - 1)) return -EFAULT;
	return 0;
}

struct ithc_sg_table {
	void *addr;
	struct sg_table sgt;
	enum dma_data_direction dir;
};
static void ithc_dma_sgtable_free(struct sg_table *sgt) {
	struct scatterlist *sg;
	int i;
	for_each_sgtable_sg(sgt, sg, i) {
		struct page *p = sg_page(sg);
		if (p) __free_page(p);
	}
	sg_free_table(sgt);
}
static void ithc_dma_data_devres_release(struct device *dev, void *res) {
	struct ithc_sg_table *sgt = res;
	if (sgt->addr) vunmap(sgt->addr);
	dma_unmap_sgtable(dev, &sgt->sgt, sgt->dir, 0);
	ithc_dma_sgtable_free(&sgt->sgt);
}

static int ithc_dma_data_alloc(struct ithc* ithc, struct ithc_dma_prd_buffer *prds, struct ithc_dma_data_buffer *b) {
	// We don't use dma_alloc_coherent for data buffers, because they don't have to be contiguous (we can use one PRD per page) or coherent (they are unidirectional).
	// Instead we use an sg_table of individually allocated pages (5.13 has dma_alloc_noncontiguous for this, but we'd like to support 5.10 for now).
	struct page *pages[16];
	if (prds->num_pages == 0 || prds->num_pages > ARRAY_SIZE(pages)) return -EINVAL;
	b->active_idx = -1;
	struct ithc_sg_table *sgt = devres_alloc(ithc_dma_data_devres_release, sizeof *sgt, GFP_KERNEL);
	if (!sgt) return -ENOMEM;
	sgt->dir = prds->dir;
	if (!sg_alloc_table(&sgt->sgt, prds->num_pages, GFP_KERNEL)) {
		struct scatterlist *sg;
		int i;
		bool ok = true;
		for_each_sgtable_sg(&sgt->sgt, sg, i) {
			struct page *p = pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO); // don't need __GFP_DMA for PCI DMA
			if (!p) { ok = false; break; }
			sg_set_page(sg, p, PAGE_SIZE, 0);
		}
		if (ok && !dma_map_sgtable(&ithc->pci->dev, &sgt->sgt, prds->dir, 0)) {
			devres_add(&ithc->pci->dev, sgt);
			b->sgt = &sgt->sgt;
			b->addr = sgt->addr = vmap(pages, prds->num_pages, 0, PAGE_KERNEL);
			if (!b->addr) return -ENOMEM;
			return 0;
		}
		ithc_dma_sgtable_free(&sgt->sgt);
	}
	devres_free(sgt);
	return -ENOMEM;
}

static int ithc_dma_data_buffer_put(struct ithc *ithc, struct ithc_dma_prd_buffer *prds, struct ithc_dma_data_buffer *b, unsigned idx) {
	struct ithc_phys_region_desc *prd = prds->addr;
	prd += idx * prds->num_pages;
	if (b->active_idx >= 0) { pci_err(ithc->pci, "buffer already active\n"); return -EINVAL; }
	b->active_idx = idx;
	if (prds->dir == DMA_TO_DEVICE) {
		if (b->data_size > PAGE_SIZE) return -EINVAL;
		prd->addr = sg_dma_address(b->sgt->sgl) >> 10;
		prd->size = b->data_size | PRD_END_FLAG;
		flush_kernel_vmap_range(b->addr, b->data_size);
	} else if (prds->dir == DMA_FROM_DEVICE) {
		struct scatterlist *sg;
		int i;
		for_each_sgtable_dma_sg(b->sgt, sg, i) {
			prd->addr = sg_dma_address(sg) >> 10;
			prd->size = sg_dma_len(sg);
			prd++;
		}
		prd[-1].size |= PRD_END_FLAG;
	}
	dma_wmb(); // for the prds
	dma_sync_sgtable_for_device(&ithc->pci->dev, b->sgt, prds->dir);
	return 0;
}

static int ithc_dma_data_buffer_get(struct ithc *ithc, struct ithc_dma_prd_buffer *prds, struct ithc_dma_data_buffer *b, unsigned idx) {
	struct ithc_phys_region_desc *prd = prds->addr;
	prd += idx * prds->num_pages;
	if (b->active_idx != idx) { pci_err(ithc->pci, "wrong buffer index\n"); return -EINVAL; }
	b->active_idx = -1;
	if (prds->dir == DMA_FROM_DEVICE) {
		dma_rmb(); // for the prds
		b->data_size = 0;
		struct scatterlist *sg;
		int i;
		for_each_sgtable_dma_sg(b->sgt, sg, i) {
			unsigned size = prd->size;
			b->data_size += size & PRD_SIZE_MASK;
			if (size & PRD_END_FLAG) break;
			if ((size & PRD_SIZE_MASK) != sg_dma_len(sg)) { pci_err(ithc->pci, "truncated prd\n"); break; }
			prd++;
		}
		invalidate_kernel_vmap_range(b->addr, b->data_size);
	}
	dma_sync_sgtable_for_cpu(&ithc->pci->dev, b->sgt, prds->dir);
	return 0;
}

int ithc_dma_rx_init(struct ithc *ithc, u8 channel, const char *devname) {
	struct ithc_dma_rx *rx = &ithc->dma_rx[channel];
	mutex_init(&rx->mutex);
	init_waitqueue_head(&rx->wait);
	u32 buf_size = DEVCFG_DMA_RX_SIZE(ithc->config.dma_buf_sizes);
	unsigned num_pages = (buf_size + PAGE_SIZE - 1) / PAGE_SIZE;
	pci_dbg(ithc->pci, "allocating rx buffers: num = %u, size = %u, pages = %u\n", NUM_RX_ALLOC, buf_size, num_pages);
	CHECK_RET(ithc_dma_prd_alloc, ithc, &rx->prds, NUM_RX_ALLOC, num_pages, DMA_FROM_DEVICE);
	for (unsigned i = 0; i < NUM_RX_ALLOC; i++)
		CHECK_RET(ithc_dma_data_alloc, ithc, &rx->prds, &rx->bufs[i]);
	writeb(DMA_RX_CONTROL2_RESET, &ithc->regs->dma_rx[channel].control2);
	lo_hi_writeq(rx->prds.dma_addr, &ithc->regs->dma_rx[channel].addr);
	writeb(NUM_RX_DEV - 1, &ithc->regs->dma_rx[channel].num_bufs);
	writeb(num_pages - 1, &ithc->regs->dma_rx[channel].num_prds);
	u8 head = readb(&ithc->regs->dma_rx[channel].head);
	if (head) { pci_err(ithc->pci, "head is nonzero (%u)\n", head); return -EIO; }
	for (unsigned i = 0; i < NUM_RX_DEV; i++)
		CHECK_RET(ithc_dma_data_buffer_put, ithc, &rx->prds, &rx->bufs[i], i);
	writeb(head ^ DMA_RX_WRAP_FLAG, &ithc->regs->dma_rx[channel].tail);
	ithc_api_init(ithc, rx, devname);
	return 0;
}
void ithc_dma_rx_enable(struct ithc *ithc, u8 channel) {
	bitsb_set(&ithc->regs->dma_rx[channel].control, DMA_RX_CONTROL_ENABLE | DMA_RX_CONTROL_IRQ_ERROR | DMA_RX_CONTROL_IRQ_DATA);
}

int ithc_dma_tx_init(struct ithc *ithc) {
	struct ithc_dma_tx *tx = &ithc->dma_tx;
	mutex_init(&tx->mutex);
	tx->max_size = DEVCFG_DMA_TX_SIZE(ithc->config.dma_buf_sizes);
	unsigned num_pages = (tx->max_size + PAGE_SIZE - 1) / PAGE_SIZE;
	pci_dbg(ithc->pci, "allocating tx buffers: size = %u, pages = %u\n", tx->max_size, num_pages);
	CHECK_RET(ithc_dma_prd_alloc, ithc, &tx->prds, 1, num_pages, DMA_TO_DEVICE);
	CHECK_RET(ithc_dma_data_alloc, ithc, &tx->prds, &tx->buf);
	lo_hi_writeq(tx->prds.dma_addr, &ithc->regs->dma_tx.addr);
	writeb(num_pages - 1, &ithc->regs->dma_tx.num_prds);
	CHECK_RET(ithc_dma_data_buffer_put, ithc, &ithc->dma_tx.prds, &ithc->dma_tx.buf, 0);
	return 0;
}

static int ithc_dma_rx_process_buf(struct ithc *ithc, struct ithc_dma_data_buffer *data, u8 channel, u8 buf) {
	if (buf >= NUM_RX_DEV) {
		pci_err(ithc->pci, "invalid dma ringbuffer index\n");
		return -EINVAL;
	}
	ithc_set_active(ithc);
	u32 len = data->data_size;
	struct ithc_dma_rx_header *hdr = data->addr;
	u8 *hiddata = (void *)(hdr + 1);
	struct ithc_hid_report_singletouch *st = (void *)hiddata;
	if (len >= sizeof *hdr && hdr->code == DMA_RX_CODE_RESET) {
		CHECK(ithc_reset, ithc);
	} else if (len < sizeof *hdr || len != sizeof *hdr + hdr->data_size) {
		if (hdr->code == DMA_RX_CODE_INPUT_REPORT) {
			// When the CPU enters a low power state during DMA, we can get truncated messages.
			// Typically this will be a single touch HID report that is only 1 byte, or a multitouch report that is 257 bytes.
			// See also ithc_set_active().
		} else {
			pci_err(ithc->pci, "invalid dma rx data! channel %u, buffer %u, size %u, code %u, data size %u\n", channel, buf, len, hdr->code, hdr->data_size);
			print_hex_dump_debug(DEVNAME " data: ", DUMP_PREFIX_OFFSET, 32, 1, hdr, min(len, 0x400u), 0);
		}
	} else if (ithc->input && hdr->code == DMA_RX_CODE_INPUT_REPORT && hdr->data_size == sizeof *st && st->report_id == HID_REPORT_ID_SINGLETOUCH) {
		input_report_key(ithc->input, BTN_TOUCH, st->button);
		input_report_abs(ithc->input, ABS_X, st->x);
		input_report_abs(ithc->input, ABS_Y, st->y);
		input_sync(ithc->input);
	} else if (ithc->hid && hdr->code == DMA_RX_CODE_REPORT_DESCRIPTOR && hdr->data_size > 8) {
		CHECK(hid_parse_report, ithc->hid, hiddata + 8, hdr->data_size - 8);
		WRITE_ONCE(ithc->hid_parse_done, true);
		wake_up(&ithc->wait_hid_parse);
	} else if (ithc->hid && hdr->code == DMA_RX_CODE_INPUT_REPORT) {
		CHECK(hid_input_report, ithc->hid, HID_INPUT_REPORT, hiddata, hdr->data_size, 1);
	} else if (ithc->hid && hdr->code == DMA_RX_CODE_FEATURE_REPORT) {
		bool done = false;
		mutex_lock(&ithc->hid_get_feature_mutex);
		if (ithc->hid_get_feature_buf) {
			if (hdr->data_size < ithc->hid_get_feature_size) ithc->hid_get_feature_size = hdr->data_size;
			memcpy(ithc->hid_get_feature_buf, hiddata, ithc->hid_get_feature_size);
			ithc->hid_get_feature_buf = NULL;
			done = true;
		}
		mutex_unlock(&ithc->hid_get_feature_mutex);
		if (done) wake_up(&ithc->wait_hid_get_feature);
		else CHECK(hid_input_report, ithc->hid, HID_FEATURE_REPORT, hiddata, hdr->data_size, 1);
	} else {
		pci_dbg(ithc->pci, "unhandled dma rx data! channel %u, buffer %u, size %u, code %u\n", channel, buf, len, hdr->code);
		print_hex_dump_debug(DEVNAME " data: ", DUMP_PREFIX_OFFSET, 32, 1, hdr, min(len, 0x400u), 0);
	}
	return 0;
}

static int ithc_dma_rx_unlocked(struct ithc *ithc, u8 channel) {
	struct ithc_dma_rx *rx = &ithc->dma_rx[channel];
	// buf idx tail..tail+NUM_RX_DEV-1 are mapped as dev idx tail%NUM_RX_DEV..
	// tail+NUM_RX_DEV = oldest unmapped buffer
	unsigned n = rx->num_received;
	u8 head_wrap = readb(&ithc->regs->dma_rx[channel].head);
	while (1) {
		u8 tail = n % NUM_RX_DEV;
		u8 tail_wrap = tail | ((n / NUM_RX_DEV) & 1 ? 0 : DMA_RX_WRAP_FLAG);
		writeb(tail_wrap, &ithc->regs->dma_rx[channel].tail);
		// ringbuffer is full if tail_wrap == head_wrap
		// ringbuffer is empty if tail_wrap == head_wrap ^ WRAP_FLAG
		if (tail_wrap == (head_wrap ^ DMA_RX_WRAP_FLAG)) return 0;

		// take the buffer that the device just filled
		struct ithc_dma_data_buffer *b = &rx->bufs[n % NUM_RX_ALLOC];
		CHECK_RET(ithc_dma_data_buffer_get, ithc, &rx->prds, b, tail);
		// give the oldest buffer we have back to the device to replace the buffer we took
		CHECK_RET(ithc_dma_data_buffer_put, ithc, &rx->prds, &rx->bufs[(n + NUM_RX_DEV) % NUM_RX_ALLOC], tail);
		rx->num_received = ++n;
		WRITE_ONCE(rx->pos, READ_ONCE(rx->pos) + sizeof(struct ithc_api_header) + b->data_size);

		// process data
		CHECK(ithc_dma_rx_process_buf, ithc, b, channel, tail);

		wake_up(&rx->wait);
	}
}
int ithc_dma_rx(struct ithc *ithc, u8 channel) {
	struct ithc_dma_rx *rx = &ithc->dma_rx[channel];
	mutex_lock(&rx->mutex);
	int ret = ithc_dma_rx_unlocked(ithc, channel);
	mutex_unlock(&rx->mutex);
	return ret;
}

static int ithc_dma_tx_unlocked(struct ithc *ithc, u32 cmdcode, u32 datasize, void *data) {
	pci_dbg(ithc->pci, "dma tx command %u, size %u\n", cmdcode, datasize);
	struct ithc_dma_tx_header *hdr;
	u8 padding = datasize & 3 ? 4 - (datasize & 3) : 0;
	unsigned fullsize = sizeof *hdr + datasize + padding;
	if (fullsize > ithc->dma_tx.max_size || fullsize > PAGE_SIZE) return -EINVAL;
	CHECK_RET(ithc_dma_data_buffer_get, ithc, &ithc->dma_tx.prds, &ithc->dma_tx.buf, 0);

	ithc->dma_tx.buf.data_size = fullsize;
	hdr = ithc->dma_tx.buf.addr;
	hdr->code = cmdcode;
	hdr->data_size = datasize;
	u8 *dest = (void *)(hdr + 1);
	memcpy(dest, data, datasize);
	dest += datasize;
	for (u8 p = 0; p < padding; p++) *dest++ = 0;
	CHECK_RET(ithc_dma_data_buffer_put, ithc, &ithc->dma_tx.prds, &ithc->dma_tx.buf, 0);

	bitsb_set(&ithc->regs->dma_tx.control, DMA_TX_CONTROL_SEND);
	CHECK_RET(waitb, ithc, &ithc->regs->dma_tx.control, DMA_TX_CONTROL_SEND, 0);
	writel(DMA_TX_STATUS_DONE, &ithc->regs->dma_tx.status);
	return 0;
}
int ithc_dma_tx(struct ithc *ithc, u32 cmdcode, u32 datasize, void *data) {
	mutex_lock(&ithc->dma_tx.mutex);
	int ret = ithc_dma_tx_unlocked(ithc, cmdcode, datasize, data);
	mutex_unlock(&ithc->dma_tx.mutex);
	return ret;
}

int ithc_set_multitouch(struct ithc *ithc, bool enable) {
	pci_info(ithc->pci, "%s multi-touch mode\n", enable ? "enabling" : "disabling");
	struct ithc_hid_feature_multitouch data = { .feature_id = HID_FEATURE_ID_MULTITOUCH, .enable = enable };
	return ithc_dma_tx(ithc, DMA_TX_CODE_SET_FEATURE, sizeof data, &data);
}

