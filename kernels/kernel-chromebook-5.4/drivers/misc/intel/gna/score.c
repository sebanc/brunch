// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2021 Intel Corporation

#include <linux/device.h>
#include <linux/idr.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <uapi/misc/intel/gna.h>

#include "device.h"
#include "mem.h"
#include "request.h"
#include "score.h"

int gna_validate_score_config(struct gna_compute_cfg *compute_cfg,
			      struct gna_file_private *file_priv)
{
	struct gna_private *gna_priv;
	size_t buffers_size;

	gna_priv = file_priv->gna_priv;

	if (compute_cfg->gna_mode > GNA_MODE_XNN) {
		dev_err(gna_dev(gna_priv), "invalid mode\n");
		return -EINVAL;
	}

	if (compute_cfg->layer_count > gna_priv->info.max_layer_count) {
		dev_err(gna_dev(gna_priv), "max layer count exceeded\n");
		return -EINVAL;
	}

	if (compute_cfg->buffer_count == 0) {
		dev_err(gna_dev(gna_priv), "no buffers\n");
		return -EINVAL;
	}

	buffers_size = sizeof(struct gna_buffer) * compute_cfg->buffer_count;
	if (!access_ok(u64_to_user_ptr(compute_cfg->buffers_ptr), buffers_size)) {
		dev_err(gna_dev(gna_priv), "invalid buffers pointer\n");
		return -EINVAL;
	}

	return 0;
}

static int gna_do_patch_memory(struct gna_private *gna_priv, struct gna_memory_object *mo,
			       struct gna_memory_patch *patch, void *vaddr)
{
	size_t size;
	void *dest;
	u64 value;

	value = patch->value;
	size = patch->size;
	dest = (u8 *)vaddr + patch->offset;
	dev_dbg(gna_dev(gna_priv), "patch offset: %llu, size: %zu, value: %llu\n",
		patch->offset, size, value);

	switch (size) {
	case 0:
		return -EFAULT;
	case sizeof(u8):
		*((u8 *)dest) = (u8)value;
		break;
	case sizeof(u16):
		*((u16 *)dest) = (u16)value;
		break;
	case sizeof(u32):
		*((u32 *)dest) = (u32)value;
		break;
	case sizeof(u64):
		*((u64 *)dest) = (u64)value;
		break;
	default:
		// should never happen
		return -EINVAL;
	}

	return 0;
}

static int gna_mem_patch_memory(struct gna_private *gna_priv, struct gna_buffer *buffer)
{
	struct gna_memory_patch *patch;
	struct gna_memory_object *mo;
	void *vaddr;
	int ret = 0;
	u32 i;

	dev_dbg(gna_dev(gna_priv), "memory_id: %llu, patch_count, %llu\n",
		buffer->memory_id, buffer->patch_count);

	mutex_lock(&gna_priv->memidr_lock);
	mo = idr_find(&gna_priv->memory_idr, buffer->memory_id);
	mutex_unlock(&gna_priv->memidr_lock);
	if (!mo)
		return -EINVAL;

	mutex_lock(&mo->page_lock);
	ret = mo->ops->get_pages(mo, buffer->offset, buffer->size);
	mutex_unlock(&mo->page_lock);
	if (ret)
		return ret;

	if (buffer->patch_count) {
		vaddr = vm_map_ram(mo->pages, mo->num_pinned, 0, PAGE_KERNEL);
		if (!vaddr)
			return -ENOMEM;

		patch = (struct gna_memory_patch *)(uintptr_t)buffer->patches_ptr;
		for (i = 0; i < buffer->patch_count; i++, patch++) {
			ret = gna_do_patch_memory(gna_priv, mo, patch, vaddr + buffer->offset);
			if (ret)
				break;
		}

		kvfree((void *)(uintptr_t)buffer->patches_ptr);
		buffer->patches_ptr = 0;
		vm_unmap_ram(vaddr, mo->num_pages);

		if (ret)
			return ret;
	}

	gna_mmu_add(gna_priv, mo);

	return ret;
}

static struct gna_buffer *gna_find_buffer(struct gna_buffer *buffer_list, u32 buffer_count,
					  u32 mmu_offset, u32 *memory_offset)
{
	struct gna_buffer *buffer;
	u32 page_offset;
	u32 memory_size;
	u32 offset;
	u32 i;

	offset = 0;
	for (i = 0; i < buffer_count; i++) {
		buffer = buffer_list + i;
		page_offset = buffer->offset & ~PAGE_MASK;
		memory_size = round_up(page_offset + buffer->size, PAGE_SIZE);
		if (mmu_offset < offset + memory_size) {
			*memory_offset = offset;
			return buffer;
		}
		offset += memory_size;
	}

	return NULL;
}

static int gna_copy_gmm_config(struct gna_private *gna_priv,
			       struct gna_buffer *buffer_list,
			       u32 buffer_count, u32 mmu_offset)
{
	struct gna_hw_descriptor *hwdesc;
	struct gna_memory_object *mo;
	struct gna_mmu_object *mmu;
	struct gna_buffer *buffer;
	u32 memory_offset;
	u32 skip_offset;
	u8 *gmm_desc;
	void *vaddr;

	mmu = &gna_priv->mmu;
	hwdesc = mmu->hwdesc;

	buffer = gna_find_buffer(buffer_list, buffer_count, mmu_offset, &memory_offset);
	if (!buffer) {
		dev_dbg(gna_dev(gna_priv), "buffer not found\n");
		return -EINVAL;
	}

	mutex_lock(&gna_priv->memidr_lock);
	mo = idr_find(&gna_priv->memory_idr, buffer->memory_id);
	mutex_unlock(&gna_priv->memidr_lock);
	if (!mo) {
		dev_dbg(gna_dev(gna_priv), "memory object not found\n");
		return -EFAULT;
	}

	vaddr = vm_map_ram(mo->pages, mo->num_pinned, 0, PAGE_KERNEL);
	if (!vaddr) {
		dev_dbg(gna_dev(gna_priv), "mapping failed\n");
		return -EFAULT;
	}

	skip_offset = round_down(buffer->offset, PAGE_SIZE);
	gmm_desc = (u8 *)vaddr + skip_offset + (mmu_offset - memory_offset);
	memcpy(&hwdesc->xnn_config, gmm_desc, sizeof(struct gna_xnn_descriptor));
	vm_unmap_ram(vaddr, mo->num_pages);

	return 0;
}

int gna_score(struct gna_request *score_request)
{
	struct gna_xnn_descriptor *xnn_config;
	struct gna_compute_cfg *compute_cfg;
	struct gna_private *gna_priv;
	struct gna_memory_object *mo;
	struct gna_mmu_object *mmu;
	struct gna_buffer *buffer;
	bool mo_valid = true;
	u64 buffer_count;
	u32 desc_base;
	int ret;
	u64 i;

	ret = 0;

	gna_priv = score_request->gna_priv;

	mmu = &gna_priv->mmu;
	xnn_config = &mmu->hwdesc->xnn_config;
	compute_cfg = &score_request->compute_cfg;

	buffer = score_request->buffer_list;
	buffer_count = score_request->buffer_count;
	dev_dbg(gna_dev(gna_priv), "buffer count: %llu\n", buffer_count);
	for (i = 0; i < buffer_count; i++, buffer++) {
		dev_dbg(gna_dev(gna_priv), "patch count: %llu\n", buffer->patch_count);
		ret = gna_mem_patch_memory(gna_priv, buffer);
		if (ret)
			goto err_put_pages;
	}

	switch (compute_cfg->gna_mode) {
	case GNA_MODE_XNN:
		dev_dbg(gna_dev(gna_priv), "xNN mode, labase: %d, lacount: %d\n",
			compute_cfg->layer_base, compute_cfg->layer_count);
		xnn_config->labase = compute_cfg->layer_base;
		xnn_config->lacount = compute_cfg->layer_count;
		break;
	case GNA_MODE_GMM:
		dev_dbg(gna_dev(gna_priv), "GMM mode, offset: %d\n", compute_cfg->layer_base);
		ret = gna_copy_gmm_config(gna_priv, score_request->buffer_list,
					  buffer_count, compute_cfg->layer_base);
		if (ret)
			goto err_put_pages_decr;
		break;
	default:
		ret = -EINVAL;
		goto err_put_pages_decr;
	}

	desc_base = (u32)(mmu->hwdesc_dma >> PAGE_SHIFT);
	gna_reg_write(gna_priv, GNA_MMIO_DESBASE, desc_base);

	gna_start_scoring(gna_priv, compute_cfg);

	return 0;

err_put_pages_decr:
	i--;
	buffer--;
err_put_pages:
	do {
		mutex_lock(&gna_priv->memidr_lock);
		mo = idr_find(&gna_priv->memory_idr, buffer->memory_id);
		mutex_unlock(&gna_priv->memidr_lock);
		if (mo) {
			mutex_lock(&mo->page_lock);
			mo->ops->put_pages(mo);
			mutex_unlock(&mo->page_lock);
		} else {
			mo_valid = false;
			dev_warn(gna_dev(gna_priv), "memory object not found %llu\n",
				 buffer->memory_id);
		}
		buffer--;
	} while (i--);

	if (mo_valid) {
		i = score_request->buffer_count;
		while (i--)
			kvfree((void *)(uintptr_t)score_request->buffer_list[i].patches_ptr);
		kvfree(score_request->buffer_list);
	}
	score_request->buffer_list = NULL;
	score_request->buffer_count = 0;

	return ret;
}
