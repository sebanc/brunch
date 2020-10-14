/*
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "intel_guc_submission.h"
#include "i915_drv.h"

ipts_i915_t intel_ipts;

int intel_ipts_map_buffer(ipts_mapbuffer_t *mapbuf)
{
	struct drm_i915_gem_object *gem_obj = NULL;
	struct i915_vma *vma = NULL;
	ipts_object_t *obj = NULL;
	int ret = 0;

	obj = kzalloc(sizeof(ipts_object_t), GFP_KERNEL);
	if (!obj) {
		DRM_ERROR("intel_ipts: Failed creating buffer\n");
		return -ENOMEM;
	}

	gem_obj = i915_gem_object_create(intel_ipts.to_i915, roundup(mapbuf->size, PAGE_SIZE));
	if (gem_obj == NULL) {
		DRM_ERROR("intel_ipts: Failed creating GEM object\n");
		ret = -ENOMEM;
		goto err_obj;
	}
	obj->gem_obj = gem_obj;

	if (mapbuf->flags & 0x01) {
		ret = i915_gem_object_attach_phys(gem_obj, PAGE_SIZE);
		if (ret) {
			DRM_ERROR("intel_ipts: Failed attaching GEM object : %d\n", ret);
			goto err_gem;
		}
		obj->cpu_addr = gem_obj->phys_handle->vaddr;
	}

	vma = i915_vma_instance(gem_obj, &intel_ipts.ipts_context->gem_context->ppgtt->vm, NULL);
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		DRM_ERROR("intel_ipts: Failed creating VMA\n");
		goto err_gem;
	}
	obj->vma = vma;

	ret = i915_vma_pin(vma, 0, PAGE_SIZE, PIN_USER);
	if (ret) {
		DRM_ERROR("intel_ipts: Failed pinning VMA\n");
		goto err_vma_close;
	}

	if (!gem_obj->phys_handle) {
		obj->cpu_addr = i915_gem_object_pin_map(gem_obj, I915_MAP_WB);
		if (IS_ERR(obj->cpu_addr)) {
			ret = PTR_ERR(obj->cpu_addr);
			DRM_ERROR("intel_ipts: Failed pinning GEM object : %d\n", ret);
			goto err_vma_unpin;
		}
	}

	mapbuf->buf_handle = (u64)obj;
	mapbuf->cpu_addr = (void*)obj->cpu_addr;
	mapbuf->gfx_addr = (void*)vma->node.start;
	mapbuf->phy_addr = (obj->gem_obj->phys_handle) ? (u64)obj->gem_obj->phys_handle->busaddr : 0;

	spin_lock(&intel_ipts.buffers.lock);
	list_add_tail(&obj->list, &intel_ipts.buffers.list);
	spin_unlock(&intel_ipts.buffers.lock);

	return 0;

err_vma_unpin:
	i915_vma_unpin(obj->vma);

err_vma_close:
	i915_vma_close(obj->vma);

err_gem:
	i915_gem_object_put(obj->gem_obj);

err_obj:
	kfree(obj);

	return ret;
}
EXPORT_SYMBOL_GPL(intel_ipts_map_buffer);

int intel_ipts_unmap_buffer(uint64_t buf_handle)
{
	ipts_object_t* obj = (ipts_object_t*)buf_handle;

	spin_lock(&intel_ipts.buffers.lock);
	list_del(&obj->list);
	spin_unlock(&intel_ipts.buffers.lock);

	if (!obj->gem_obj->phys_handle)
		i915_gem_object_unpin_pages(obj->gem_obj);
	
	i915_vma_unpin(obj->vma);

	i915_vma_close(obj->vma);

	i915_gem_object_put(obj->gem_obj);

	kfree(obj);

	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipts_unmap_buffer);

static void intel_ipts_get_wq_info(ipts_wq_info_t *wq_info)
{
	struct guc_process_desc *desc;
	void *base = NULL;
	u64 phy_base = 0;

	base = intel_ipts.ipts_client->vaddr;
	desc = (struct guc_process_desc *)((u64)base + intel_ipts.ipts_client->proc_desc_offset);

	desc->wq_base_addr = (u64)base + GUC_DB_SIZE;
	desc->db_base_addr = (u64)base + intel_ipts.ipts_client->doorbell_offset;

	phy_base = sg_dma_address(intel_ipts.ipts_client->vma->pages->sgl);

	wq_info->db_addr = desc->db_base_addr;
	wq_info->db_phy_addr = phy_base + intel_ipts.ipts_client->doorbell_offset;
	wq_info->db_cookie_offset = offsetof(struct guc_doorbell_info, cookie);
	wq_info->wq_addr = desc->wq_base_addr;
	wq_info->wq_phy_addr = phy_base + GUC_DB_SIZE;
	wq_info->wq_head_addr = (u64)&desc->head;
	wq_info->wq_head_phy_addr = phy_base + intel_ipts.ipts_client->proc_desc_offset +
					offsetof(struct guc_process_desc, head);
	wq_info->wq_tail_addr = (u64)&desc->tail;
	wq_info->wq_tail_phy_addr = phy_base + intel_ipts.ipts_client->proc_desc_offset +
					offsetof(struct guc_process_desc, tail);
	wq_info->wq_size = desc->wq_size_bytes;
}

static void raw_data_work_func(struct work_struct *work)
{
	if (intel_ipts.handle_processed_data)
		intel_ipts.handle_processed_data(intel_ipts.ipts);
}

void intel_ipts_notify_handle_processed_data(void)
{
	if (intel_ipts.ipts && intel_ipts.ipts->state == IPTS_STA_RAW_DATA_STARTED)
		schedule_work(&intel_ipts.raw_data_work);
}

int intel_ipts_connect(ipts_info_t *ipts, void *intel_ipts_handle_processed_data)
{
	if (!intel_ipts.initialized)
		return -ENODEV;

	intel_ipts.ipts = ipts;
	intel_ipts.ipts->hardware_id = intel_ipts.hardware_id;
	intel_ipts.handle_processed_data = intel_ipts_handle_processed_data;

	intel_ipts_get_wq_info(&intel_ipts.ipts->resources.wq_info);

	INIT_WORK(&intel_ipts.raw_data_work, raw_data_work_func);

	spin_lock_init(&intel_ipts.buffers.lock);
	INIT_LIST_HEAD(&intel_ipts.buffers.list);

	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipts_connect);

void intel_ipts_disconnect(void)
{
	destroy_work_on_stack(&intel_ipts.raw_data_work);

	intel_ipts.handle_processed_data = NULL;
	intel_ipts.ipts = NULL;
}
EXPORT_SYMBOL_GPL(intel_ipts_disconnect);
