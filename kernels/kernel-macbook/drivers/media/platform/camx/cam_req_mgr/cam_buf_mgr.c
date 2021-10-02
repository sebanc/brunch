// SPDX-License-Identifier: GPL-2.0
/*
 * Spectra camera buffer manager
 *
 * Copyright 2020 Google LLC.
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <media/cam_req_mgr.h>

#include "cam_req_mgr_dev.h"
#include "cam_debug_util.h"
#include "cam_buf_mgr.h"

#define MAX_BUFFER_SIZE (67108864)	/* 64 mb */

struct cmm_dma_buf_attachment {
	struct sg_table *table;
	struct list_head list;
};

/**
 * struct cmm_buf_context - camera buffer manager context
 *
 * @dev:         device context
 * @dev_lock:    lock protecting the tree of nodes
 */
struct cmm_buf_context {
	struct device *dev;
	struct mutex dev_lock;	/* device lock */
};

/**
 * struct cmm_buffer - metadata for a buffer
 *
 * @flags:     buffer specific flags
 * @size:      size of the buffer
 * @b_lock:    protects the camera buffer fields
 * @vaddr:     the kernel mapping if kmap_cnt is not zero
 * @kmap_cnt:  number of times the buffer is mapped to the kernel
 * @sg_table:  the sg table for the buffer
 * @pages      :Array of pointer to the allocated pages
 * @num_pages: The number of the allocated pages
 * @attached:  list of entry attachments
 * @cpu_sync:  Cache synchronization flag
 */
struct cmm_buffer {
	unsigned long flags;
	size_t size;
	struct mutex b_lock;	/* buffer lock */
	void *vaddr;
	int kmap_cnt;
	struct sg_table sg_table;
	struct page **pages;
	size_t num_pages;
	struct list_head attached;
};

static struct cmm_buf_context *idev;
static int _cmm_cbuf_sgt_alloc(struct cmm_buffer *cbuf)
{
	struct sg_table *sgt = &cbuf->sg_table;
	int i, rc;

	cbuf->num_pages = DIV_ROUND_UP(cbuf->size, PAGE_SIZE);
	cbuf->pages = kcalloc(cbuf->num_pages, sizeof(*cbuf->pages),
			      GFP_KERNEL);
	if (!cbuf->pages) {
		CAM_ERR(CAM_MEM, "Cannot allocate pages");
		return -ENOMEM;
	}

	for (i = 0; i < cbuf->num_pages; ++i) {
		cbuf->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!cbuf->pages[i]) {
			CAM_ERR(CAM_MEM, "Cannot allocate page[%d]", i);
			rc = -ENOMEM;
			goto err_free_pages;
		}
	}

	rc = sg_alloc_table_from_pages(sgt, cbuf->pages, cbuf->num_pages, 0,
				       cbuf->size, GFP_KERNEL);
	if (rc) {
		CAM_ERR(CAM_MEM, "Cannot allocate sg table, rc:%d", rc);
		goto err_free_pages;
	}

	sgt->nents = dma_map_sg(idev->dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL);
	if (!sgt->nents)
		goto err_free_table;

	return rc;

err_free_table:
	sg_free_table(sgt);
err_free_pages:
	for (i = 0; i < cbuf->num_pages && cbuf->pages[i]; ++i)
		__free_pages(cbuf->pages[i], 0);

	kfree(cbuf->pages);
	cbuf->num_pages = 0;

	return rc;
}

static void _cmm_cbuf_sgt_free(struct cmm_buffer *cbuf)
{
	int i;

	dma_unmap_sg(idev->dev, cbuf->sg_table.sgl,
		     cbuf->sg_table.orig_nents, DMA_BIDIRECTIONAL);

	sg_free_table(&cbuf->sg_table);

	for (i = 0; i < cbuf->num_pages && cbuf->pages[i]; ++i)
		__free_pages(cbuf->pages[i], 0);

	kfree(cbuf->pages);
	cbuf->num_pages = 0;
}

static int _cmm_cbuf_alloc(struct cmm_buffer *cbuf)
{
	int rc;

	rc = _cmm_cbuf_sgt_alloc(cbuf);
	if (rc)
		CAM_ERR(CAM_MEM, "Cannot allocate cbuf scl table");

	return rc;
}

static void _cmm_cbuf_free(struct cmm_buffer *cbuf)
{
	_cmm_cbuf_sgt_free(cbuf);
}

static struct cmm_buffer *_cmm_cbuf_create(unsigned long len,
					   unsigned long flags)
{
	struct cmm_buffer *cbuf;
	int rc;

	cbuf = kzalloc(sizeof(*cbuf), GFP_KERNEL);
	if (!cbuf)
		return ERR_PTR(-ENOMEM);

	cbuf->size = len;
	cbuf->flags = flags;

	rc = _cmm_cbuf_alloc(cbuf);
	if (rc)
		goto err_buf_free;

	INIT_LIST_HEAD(&cbuf->attached);
	mutex_init(&cbuf->b_lock);

	return cbuf;

err_buf_free:
	kfree(cbuf);

	return ERR_PTR(rc);
}

static void _cmm_cbuf_destroy(struct cmm_buffer *cbuf)
{
	if (cbuf->kmap_cnt > 0) {
		pr_err_once("Likely missing a call to unmap\n");
		vunmap(cbuf->vaddr);
	}

	mutex_destroy(&cbuf->b_lock);
	_cmm_cbuf_free(cbuf);
	kfree(cbuf);
}

static void *_cmm_dma_kmap_get(struct cmm_buffer *cbuf)
{
	pgprot_t pgprot;

	if (cbuf->kmap_cnt++)
		return cbuf->vaddr;

	if (cbuf->flags & CAM_MEM_FLAG_CACHE)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	cbuf->vaddr = vmap(cbuf->pages, cbuf->num_pages, VM_MAP, pgprot);
	if (IS_ERR_OR_NULL(cbuf->vaddr))
		goto err_kmap_dec;

	CAM_DBG(CAM_MEM, "Buffer vmap, map_cnt: %d", cbuf->kmap_cnt);

	return cbuf->vaddr;

err_kmap_dec:
	cbuf->kmap_cnt--;
	return ERR_CAST(cbuf->vaddr);
}

static void _cmm_dma_kmap_put(struct cmm_buffer *cbuf)
{
	if (cbuf->kmap_cnt && --cbuf->kmap_cnt)
		return;

	vunmap(cbuf->vaddr);
	cbuf->vaddr = NULL;

	CAM_DBG(CAM_MEM, "Buffer vunmap, map_cnt: %d", cbuf->kmap_cnt);
}

static struct sg_table *_cmm_dma_sg_table_new(struct sg_table *sgt)
{
	struct sg_table *new_sgt;
	struct scatterlist *scl, *new_scl;
	int rc, i;

	new_sgt = kzalloc(sizeof(*new_sgt), GFP_KERNEL);
	if (!new_sgt)
		return ERR_PTR(-ENOMEM);

	rc = sg_alloc_table(new_sgt, sgt->orig_nents, GFP_KERNEL);
	if (rc)
		goto err_new_sgt_free;

	new_scl = new_sgt->sgl;
	for_each_sg(sgt->sgl, scl, sgt->orig_nents, i) {
		memcpy(new_scl, scl, sizeof(*scl));
		sg_dma_address(new_scl) = 0;
		sg_dma_len(new_scl) = 0;
		new_scl = sg_next(new_scl);
	}

	return new_sgt;

err_new_sgt_free:
	kfree(new_sgt);

	return ERR_PTR(rc);
}

static void _cmm_dma_sg_table_del(struct sg_table *sgt)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static int _op_dma_buf_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct cmm_buffer *cbuf = dmabuf->priv;
	struct cmm_dma_buf_attachment *new_att;
	struct sg_table *sgt;
	int rc;

	new_att = kzalloc(sizeof(*new_att), GFP_KERNEL);
	if (IS_ERR_OR_NULL(new_att))
		return PTR_ERR(new_att);

	sgt = _cmm_dma_sg_table_new(&cbuf->sg_table);
	if (IS_ERR_OR_NULL(sgt)) {
		rc = PTR_ERR(sgt);
		goto err_free_att;
	}

	new_att->table = sgt;
	INIT_LIST_HEAD(&new_att->list);

	attachment->priv = new_att;

	mutex_lock(&cbuf->b_lock);
	list_add(&new_att->list, &cbuf->attached);
	mutex_unlock(&cbuf->b_lock);

	return 0;

err_free_att:
	kfree(new_att);

	return rc;
}

static void _op_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct cmm_dma_buf_attachment *a = attachment->priv;
	struct cmm_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->b_lock);
	list_del(&a->list);
	mutex_unlock(&buffer->b_lock);
	_cmm_dma_sg_table_del(a->table);

	kfree(a);
}

static struct sg_table *_op_dma_buf_map(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct cmm_dma_buf_attachment *a = attachment->priv;
	struct sg_table *table;
	int map_attrs;
	struct cmm_buffer *buffer = attachment->dmabuf->priv;

	table = a->table;

	map_attrs = DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->b_lock);
	table->nents = dma_map_sg_attrs(attachment->dev, table->sgl,
					table->orig_nents, direction,
					map_attrs);
	if (!table->nents) {
		mutex_unlock(&buffer->b_lock);
		return ERR_PTR(-ENOMEM);
	}

	mutex_unlock(&buffer->b_lock);

	return table;
}

static void _op_dma_buf_unmap(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	int map_attrs;
	struct cmm_buffer *buffer = attachment->dmabuf->priv;

	map_attrs = DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->b_lock);
	dma_unmap_sg_attrs(attachment->dev, table->sgl, table->orig_nents,
			   direction, map_attrs);
	mutex_unlock(&buffer->b_lock);
}

static int _op_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct cmm_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i, ret = 0;

	if (!(buffer->flags & CAM_MEM_FLAG_CACHE))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	vma->vm_private_data = buffer;

	mutex_lock(&buffer->b_lock);

	for_each_sg(table->sgl, sg, table->orig_nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				      vma->vm_page_prot);
		if (ret)
			goto err_vm_close;

		addr += len;
		if (addr >= vma->vm_end)
			break;
	}

	mutex_unlock(&buffer->b_lock);

	return ret;

err_vm_close:
	mutex_unlock(&buffer->b_lock);
	CAM_ERR(CAM_MEM, "failure mapping buffer to userspace");

	return ret;
}

static void _op_dma_buf_release(struct dma_buf *dmabuf)
{
	struct cmm_buffer *buffer = dmabuf->priv;

	_cmm_cbuf_destroy(buffer);
	kfree(dmabuf->exp_name);
}

static int _op_dma_buf_vmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct cmm_buffer *buffer = dmabuf->priv;
	void *vaddr = ERR_PTR(-EINVAL);

	mutex_lock(&buffer->b_lock);
	vaddr = _cmm_dma_kmap_get(buffer);
	mutex_unlock(&buffer->b_lock);

	dma_buf_map_set_vaddr(map, vaddr);

	return PTR_ERR_OR_ZERO(vaddr);
}

static void _op_dma_buf_vunmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct cmm_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->b_lock);
	_cmm_dma_kmap_put(buffer);
	mutex_unlock(&buffer->b_lock);
}

static int _op_dma_buf_beg_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	struct cmm_buffer *cbuf = dmabuf->priv;
	int ret = 0;

	if (!(cbuf->flags & CAM_MEM_FLAG_CACHE))
		return 0;

	dma_sync_sg_for_cpu(idev->dev, cbuf->sg_table.sgl,
			    cbuf->sg_table.orig_nents, direction);

	return ret;
}

static int _op_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	struct cmm_buffer *cbuf = dmabuf->priv;
	int ret = 0;

	if (!(cbuf->flags & CAM_MEM_FLAG_CACHE))
		return 0;

	dma_sync_sg_for_device(idev->dev, cbuf->sg_table.sgl,
			       cbuf->sg_table.orig_nents, direction);

	return ret;
}

static const struct dma_buf_ops cmm_dma_buf_ops = {
	.map_dma_buf	  = _op_dma_buf_map,
	.unmap_dma_buf	  = _op_dma_buf_unmap,
	.mmap		  = _op_dma_buf_mmap,
	.release	  = _op_dma_buf_release,
	.attach		  = _op_dma_buf_attach,
	.detach		  = _op_dma_buf_detatch,
	.begin_cpu_access = _op_dma_buf_beg_cpu_access,
	.end_cpu_access   = _op_dma_buf_end_cpu_access,
	.vmap		  = _op_dma_buf_vmap,
	.vunmap		  = _op_dma_buf_vunmap,
};

struct dma_buf *cmm_alloc_buffer(size_t len, unsigned int flags)
{
	struct cmm_buffer *buffer = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;

	len = PAGE_ALIGN(len);
	if (!len) {
		CAM_ERR(CAM_MEM, "Invalid buffer size: %zu", len);
		return ERR_PTR(-EINVAL);
	}

	if (len > MAX_BUFFER_SIZE) {
		CAM_ERR(CAM_MEM, "Buffer size is over limit: %zu", len);
		return ERR_PTR(-EINVAL);
	}

	buffer = _cmm_cbuf_create(len, flags);
	if (IS_ERR_OR_NULL(buffer)) {
		CAM_ERR(CAM_MEM, "Cannot create camera buffer");
		return ERR_CAST(buffer);
	}

	exp_info.ops = &cmm_dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;
	exp_info.exp_name = kasprintf(GFP_KERNEL, "%s", KBUILD_MODNAME);

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		CAM_ERR(CAM_MEM, "Cannot export camera buffer");
		_cmm_cbuf_destroy(buffer);
		kfree(exp_info.exp_name);
		return ERR_CAST(dmabuf);
	}

	return dmabuf;
}

void cmm_free_buffer(struct dma_buf *dmabuf)
{
	if (!dmabuf) {
		CAM_ERR(CAM_MEM, "Invalid argument(s)");
		return;
	}

	dma_buf_put(dmabuf);
}

int cam_buf_mgr_init(struct platform_device *pdev)
{
	idev = kzalloc(sizeof(*idev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(idev)) {
		CAM_ERR(CAM_MEM, "Cannot allocate memory");
		return PTR_ERR(idev);
	}

	idev->dev = &pdev->dev;
	dma_coerce_mask_and_coherent(idev->dev, DMA_BIT_MASK(64));
	mutex_init(&idev->dev_lock);

	return 0;
}

void cam_buf_mgr_exit(struct platform_device *pdev)
{
	if (IS_ERR_OR_NULL(idev)) {
		CAM_ERR(CAM_MEM, "Invalid CBM context");
		return;
	}

	mutex_destroy(&idev->dev_lock);
	kfree(idev);
}
