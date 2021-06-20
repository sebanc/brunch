/*
 * Copyright © 2014 The Chromium OS Authors
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 */

#include <ttm/ttm_page_alloc.h>
#include <drm/drmP.h>
#include "cirrus_drv.h"

struct sg_table *cirrus_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct cirrus_bo *cirrusbo = gem_to_cirrus_bo(obj);
	unsigned long npages = cirrusbo->bo.num_pages;

	return drm_prime_pages_to_sg(cirrusbo->bo.ttm->pages, npages);
}

void *cirrus_gem_prime_vmap(struct drm_gem_object *obj)
{
	struct cirrus_bo *cirrusbo = gem_to_cirrus_bo(obj);
	int ret;

	ret = ttm_bo_kmap(&cirrusbo->bo, 0, cirrusbo->bo.num_pages,
			  &cirrusbo->dma_buf_vmap);
	if (ret)
		return ERR_PTR(ret);

	return cirrusbo->dma_buf_vmap.virtual;
}

void cirrus_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{
	struct cirrus_bo *cirrusbo = gem_to_cirrus_bo(obj);

	ttm_bo_kunmap(&cirrusbo->dma_buf_vmap);
}

int cirrus_gem_prime_pin(struct drm_gem_object *obj)
{
	struct cirrus_bo *cirrusbo = gem_to_cirrus_bo(obj);
	int ret = 0;

	ret = cirrus_bo_reserve(cirrusbo, false);
	if (unlikely(ret != 0))
		goto out;

	ret = cirrus_bo_pin(cirrusbo, TTM_PL_FLAG_SYSTEM, NULL);
	if (ret)
		goto unreserve_out;

	ttm_pool_populate(cirrusbo->bo.ttm);

unreserve_out:
	cirrus_bo_unreserve(cirrusbo);
out:
	return ret;
}

int cirrus_gem_prime_mmap(struct drm_gem_object *obj,
			  struct vm_area_struct *vma)
{
	struct cirrus_bo *cirrusbo = gem_to_cirrus_bo(obj);
	int ret = 0;

	ret = ttm_fbdev_mmap(vma, &cirrusbo->bo);
	vma->vm_pgoff = drm_vma_node_start(&cirrusbo->bo.vma_node);

	return ret;
}
