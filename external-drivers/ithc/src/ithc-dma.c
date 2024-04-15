// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause

#include "ithc.h"

// The THC uses tables of PRDs (physical region descriptors) to describe the TX and RX data buffers.
// Each PRD contains the DMA address and size of a block of DMA memory, and some status flags.
// This allows each data buffer to consist of multiple non-contiguous blocks of memory.

static int ithc_dma_prd_alloc(struct ithc *ithc, struct ithc_dma_prd_buffer *p,
	unsigned int num_buffers, unsigned int num_pages, enum dma_data_direction dir)
{
	p->num_pages = num_pages;
	p->dir = dir;
	// We allocate enough space to have one PRD per data buffer page, however if the data
	// buffer pages happen to be contiguous, we can describe the buffer using fewer PRDs, so
	// some will remain unused (which is fine).
	p->size = round_up(num_buffers * num_pages * sizeof(struct ithc_phys_region_desc), PAGE_SIZE);
	p->addr = dmam_alloc_coherent(&ithc->pci->dev, p->size, &p->dma_addr, GFP_KERNEL);
	if (!p->addr)
		return -ENOMEM;
	if (p->dma_addr & (PAGE_SIZE - 1))
		return -EFAULT;
	return 0;
}

// Devres managed sg_table wrapper.
struct ithc_sg_table {
	void *addr;
	struct sg_table sgt;
	enum dma_data_direction dir;
};
static void ithc_dma_sgtable_free(struct sg_table *sgt)
{
	struct scatterlist *sg;
	int i;
	for_each_sgtable_sg(sgt, sg, i) {
		struct page *p = sg_page(sg);
		if (p)
			__free_page(p);
	}
	sg_free_table(sgt);
}
static void ithc_dma_data_devres_release(struct device *dev, void *res)
{
	struct ithc_sg_table *sgt = res;
	if (sgt->addr)
		vunmap(sgt->addr);
	dma_unmap_sgtable(dev, &sgt->sgt, sgt->dir, 0);
	ithc_dma_sgtable_free(&sgt->sgt);
}

static int ithc_dma_data_alloc(struct ithc *ithc, struct ithc_dma_prd_buffer *prds,
	struct ithc_dma_data_buffer *b)
{
	// We don't use dma_alloc_coherent() for data buffers, because they don't have to be
	// coherent (they are unidirectional) or contiguous (we can use one PRD per page).
	// We could use dma_alloc_noncontiguous(), however this still always allocates a single
	// DMA mapped segment, which is more restrictive than what we need.
	// Instead we use an sg_table of individually allocated pages.
	struct page *pages[16];
	if (prds->num_pages == 0 || prds->num_pages > ARRAY_SIZE(pages))
		return -EINVAL;
	b->active_idx = -1;
	struct ithc_sg_table *sgt = devres_alloc(
		ithc_dma_data_devres_release, sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;
	sgt->dir = prds->dir;

	if (!sg_alloc_table(&sgt->sgt, prds->num_pages, GFP_KERNEL)) {
		struct scatterlist *sg;
		int i;
		bool ok = true;
		for_each_sgtable_sg(&sgt->sgt, sg, i) {
			// NOTE: don't need __GFP_DMA for PCI DMA
			struct page *p = pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
			if (!p) {
				ok = false;
				break;
			}
			sg_set_page(sg, p, PAGE_SIZE, 0);
		}
		if (ok && !dma_map_sgtable(&ithc->pci->dev, &sgt->sgt, prds->dir, 0)) {
			devres_add(&ithc->pci->dev, sgt);
			b->sgt = &sgt->sgt;
			b->addr = sgt->addr = vmap(pages, prds->num_pages, 0, PAGE_KERNEL);
			if (!b->addr)
				return -ENOMEM;
			return 0;
		}
		ithc_dma_sgtable_free(&sgt->sgt);
	}
	devres_free(sgt);
	return -ENOMEM;
}

static int ithc_dma_data_buffer_put(struct ithc *ithc, struct ithc_dma_prd_buffer *prds,
	struct ithc_dma_data_buffer *b, unsigned int idx)
{
	// Give a buffer to the THC.
	struct ithc_phys_region_desc *prd = prds->addr;
	prd += idx * prds->num_pages;
	if (b->active_idx >= 0) {
		pci_err(ithc->pci, "buffer already active\n");
		return -EINVAL;
	}
	b->active_idx = idx;
	if (prds->dir == DMA_TO_DEVICE) {
		// TX buffer: Caller should have already filled the data buffer, so just fill
		// the PRD and flush.
		// (TODO: Support multi-page TX buffers. So far no device seems to use or need
		// these though.)
		if (b->data_size > PAGE_SIZE)
			return -EINVAL;
		prd->addr = sg_dma_address(b->sgt->sgl) >> 10;
		prd->size = b->data_size | PRD_FLAG_END;
		flush_kernel_vmap_range(b->addr, b->data_size);
	} else if (prds->dir == DMA_FROM_DEVICE) {
		// RX buffer: Reset PRDs.
		struct scatterlist *sg;
		int i;
		for_each_sgtable_dma_sg(b->sgt, sg, i) {
			prd->addr = sg_dma_address(sg) >> 10;
			prd->size = sg_dma_len(sg);
			prd++;
		}
		prd[-1].size |= PRD_FLAG_END;
	}
	dma_wmb(); // for the prds
	dma_sync_sgtable_for_device(&ithc->pci->dev, b->sgt, prds->dir);
	return 0;
}

static int ithc_dma_data_buffer_get(struct ithc *ithc, struct ithc_dma_prd_buffer *prds,
	struct ithc_dma_data_buffer *b, unsigned int idx)
{
	// Take a buffer from the THC.
	struct ithc_phys_region_desc *prd = prds->addr;
	prd += idx * prds->num_pages;
	// This is purely a sanity check. We don't strictly need the idx parameter for this
	// function, because it should always be the same as active_idx, unless we have a bug.
	if (b->active_idx != idx) {
		pci_err(ithc->pci, "wrong buffer index\n");
		return -EINVAL;
	}
	b->active_idx = -1;
	if (prds->dir == DMA_FROM_DEVICE) {
		// RX buffer: Calculate actual received data size from PRDs.
		dma_rmb(); // for the prds
		b->data_size = 0;
		struct scatterlist *sg;
		int i;
		for_each_sgtable_dma_sg(b->sgt, sg, i) {
			unsigned int size = prd->size;
			b->data_size += size & PRD_SIZE_MASK;
			if (size & PRD_FLAG_END)
				break;
			if ((size & PRD_SIZE_MASK) != sg_dma_len(sg)) {
				pci_err(ithc->pci, "truncated prd\n");
				break;
			}
			prd++;
		}
		invalidate_kernel_vmap_range(b->addr, b->data_size);
	}
	dma_sync_sgtable_for_cpu(&ithc->pci->dev, b->sgt, prds->dir);
	return 0;
}

int ithc_dma_rx_init(struct ithc *ithc, u8 channel)
{
	struct ithc_dma_rx *rx = &ithc->dma_rx[channel];
	mutex_init(&rx->mutex);

	// Allocate buffers.
	unsigned int num_pages = (ithc->max_rx_size + PAGE_SIZE - 1) / PAGE_SIZE;
	pci_dbg(ithc->pci, "allocating rx buffers: num = %u, size = %u, pages = %u\n",
		NUM_RX_BUF, ithc->max_rx_size, num_pages);
	CHECK_RET(ithc_dma_prd_alloc, ithc, &rx->prds, NUM_RX_BUF, num_pages, DMA_FROM_DEVICE);
	for (unsigned int i = 0; i < NUM_RX_BUF; i++)
		CHECK_RET(ithc_dma_data_alloc, ithc, &rx->prds, &rx->bufs[i]);

	// Init registers.
	writeb(DMA_RX_CONTROL2_RESET, &ithc->regs->dma_rx[channel].control2);
	lo_hi_writeq(rx->prds.dma_addr, &ithc->regs->dma_rx[channel].addr);
	writeb(NUM_RX_BUF - 1, &ithc->regs->dma_rx[channel].num_bufs);
	writeb(num_pages - 1, &ithc->regs->dma_rx[channel].num_prds);
	u8 head = readb(&ithc->regs->dma_rx[channel].head);
	if (head) {
		pci_err(ithc->pci, "head is nonzero (%u)\n", head);
		return -EIO;
	}

	// Init buffers.
	for (unsigned int i = 0; i < NUM_RX_BUF; i++)
		CHECK_RET(ithc_dma_data_buffer_put, ithc, &rx->prds, &rx->bufs[i], i);

	writeb(head ^ DMA_RX_WRAP_FLAG, &ithc->regs->dma_rx[channel].tail);
	return 0;
}

void ithc_dma_rx_enable(struct ithc *ithc, u8 channel)
{
	bitsb_set(&ithc->regs->dma_rx[channel].control,
		DMA_RX_CONTROL_ENABLE | DMA_RX_CONTROL_IRQ_ERROR | DMA_RX_CONTROL_IRQ_DATA);
	CHECK(waitl, ithc, &ithc->regs->dma_rx[channel].status,
		DMA_RX_STATUS_ENABLED, DMA_RX_STATUS_ENABLED);
}

int ithc_dma_tx_init(struct ithc *ithc)
{
	struct ithc_dma_tx *tx = &ithc->dma_tx;
	mutex_init(&tx->mutex);

	// Allocate buffers.
	unsigned int num_pages = (ithc->max_tx_size + PAGE_SIZE - 1) / PAGE_SIZE;
	pci_dbg(ithc->pci, "allocating tx buffers: size = %u, pages = %u\n",
		ithc->max_tx_size, num_pages);
	CHECK_RET(ithc_dma_prd_alloc, ithc, &tx->prds, 1, num_pages, DMA_TO_DEVICE);
	CHECK_RET(ithc_dma_data_alloc, ithc, &tx->prds, &tx->buf);

	// Init registers.
	lo_hi_writeq(tx->prds.dma_addr, &ithc->regs->dma_tx.addr);
	writeb(num_pages - 1, &ithc->regs->dma_tx.num_prds);

	// Init buffers.
	CHECK_RET(ithc_dma_data_buffer_put, ithc, &ithc->dma_tx.prds, &ithc->dma_tx.buf, 0);
	return 0;
}

static int ithc_dma_rx_unlocked(struct ithc *ithc, u8 channel)
{
	// Process all filled RX buffers from the ringbuffer.
	struct ithc_dma_rx *rx = &ithc->dma_rx[channel];
	unsigned int n = rx->num_received;
	u8 head_wrap = readb(&ithc->regs->dma_rx[channel].head);
	while (1) {
		u8 tail = n % NUM_RX_BUF;
		u8 tail_wrap = tail | ((n / NUM_RX_BUF) & 1 ? 0 : DMA_RX_WRAP_FLAG);
		writeb(tail_wrap, &ithc->regs->dma_rx[channel].tail);
		// ringbuffer is full if tail_wrap == head_wrap
		// ringbuffer is empty if tail_wrap == head_wrap ^ WRAP_FLAG
		if (tail_wrap == (head_wrap ^ DMA_RX_WRAP_FLAG))
			return 0;

		// take the buffer that the device just filled
		struct ithc_dma_data_buffer *b = &rx->bufs[n % NUM_RX_BUF];
		CHECK_RET(ithc_dma_data_buffer_get, ithc, &rx->prds, b, tail);
		rx->num_received = ++n;

		// process data
		struct ithc_data d;
		if ((ithc->use_quickspi ? ithc_quickspi_decode_rx : ithc_legacy_decode_rx)
			(ithc, b->addr, b->data_size, &d) < 0) {
			pci_err(ithc->pci, "invalid dma rx data! channel %u, buffer %u, size %u: %*ph\n",
				channel, tail, b->data_size, min((int)b->data_size, 64), b->addr);
			print_hex_dump_debug(DEVNAME " data: ", DUMP_PREFIX_OFFSET, 32, 1,
				b->addr, min(b->data_size, 0x400u), 0);
		} else {
			ithc_hid_process_data(ithc, &d);
		}

		// give the buffer back to the device
		CHECK_RET(ithc_dma_data_buffer_put, ithc, &rx->prds, b, tail);
	}
}
int ithc_dma_rx(struct ithc *ithc, u8 channel)
{
	struct ithc_dma_rx *rx = &ithc->dma_rx[channel];
	mutex_lock(&rx->mutex);
	int ret = ithc_dma_rx_unlocked(ithc, channel);
	mutex_unlock(&rx->mutex);
	return ret;
}

static int ithc_dma_tx_unlocked(struct ithc *ithc, const struct ithc_data *data)
{
	// Send a single TX buffer to the THC.
	pci_dbg(ithc->pci, "dma tx data type %u, size %u\n", data->type, data->size);
	CHECK_RET(ithc_dma_data_buffer_get, ithc, &ithc->dma_tx.prds, &ithc->dma_tx.buf, 0);

	// Fill the TX buffer with header and data.
	ssize_t sz;
	if (data->type == ITHC_DATA_RAW) {
		sz = min(data->size, ithc->max_tx_size);
		memcpy(ithc->dma_tx.buf.addr, data->data, sz);
	} else {
		sz = (ithc->use_quickspi ? ithc_quickspi_encode_tx : ithc_legacy_encode_tx)
			(ithc, data, ithc->dma_tx.buf.addr, ithc->max_tx_size);
	}
	ithc->dma_tx.buf.data_size = sz < 0 ? 0 : sz;
	CHECK_RET(ithc_dma_data_buffer_put, ithc, &ithc->dma_tx.prds, &ithc->dma_tx.buf, 0);
	if (sz < 0) {
		pci_err(ithc->pci, "failed to encode tx data type %i, size %u, error %i\n",
			data->type, data->size, (int)sz);
		return -EINVAL;
	}

	// Let the THC process the buffer.
	bitsb_set(&ithc->regs->dma_tx.control, DMA_TX_CONTROL_SEND);
	CHECK_RET(waitb, ithc, &ithc->regs->dma_tx.control, DMA_TX_CONTROL_SEND, 0);
	writel(DMA_TX_STATUS_DONE, &ithc->regs->dma_tx.status);
	return 0;
}
int ithc_dma_tx(struct ithc *ithc, const struct ithc_data *data)
{
	mutex_lock(&ithc->dma_tx.mutex);
	int ret = ithc_dma_tx_unlocked(ithc, data);
	mutex_unlock(&ithc->dma_tx.mutex);
	return ret;
}

