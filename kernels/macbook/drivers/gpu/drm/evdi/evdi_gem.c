// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
#include <drm/drmP.h>
#endif
#include "evdi_drm_drv.h"
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <drm/drm_cache.h>


#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static const struct vm_operations_struct evdi_gem_vm_ops = {
	.fault = evdi_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static struct drm_gem_object_funcs gem_obj_funcs = {
	.free = evdi_gem_free_object,
	.vm_ops = &evdi_gem_vm_ops,
	.export = drm_gem_prime_export,
	.get_sg_table = evdi_prime_get_sg_table,
};
#endif

uint32_t evdi_gem_object_handle_lookup(struct drm_file *filp,
				       struct drm_gem_object *obj)
{
	uint32_t it_handle = 0;
	struct drm_gem_object *it_obj = NULL;

	spin_lock(&filp->table_lock);
	idr_for_each_entry(&filp->object_idr, it_obj, it_handle) {
		if (it_obj == obj)
			break;
	}
	spin_unlock(&filp->table_lock);

	if (!it_obj)
		it_handle = 0;

	return it_handle;
}

struct evdi_gem_object *evdi_gem_alloc_object(struct drm_device *dev,
					      size_t size)
{
	struct evdi_gem_object *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj == NULL)
		return NULL;

	if (drm_gem_object_init(dev, &obj->base, size) != 0) {
		kfree(obj);
		return NULL;
	}

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE || defined(EL8)
	dma_resv_init(&obj->_resv);
#else
	reservation_object_init(&obj->_resv);
#endif
	obj->resv = &obj->_resv;

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	obj->base.funcs = &gem_obj_funcs;
#endif

	return obj;
}

static int
evdi_gem_create(struct drm_file *file,
		struct drm_device *dev, uint64_t size, uint32_t *handle_p)
{
	struct evdi_gem_object *obj;
	int ret;
	u32 handle;

	size = roundup(size, PAGE_SIZE);

	obj = evdi_gem_alloc_object(dev, size);
	if (obj == NULL)
		return -ENOMEM;

	ret = drm_gem_handle_create(file, &obj->base, &handle);
	if (ret) {
		drm_gem_object_release(&obj->base);
		kfree(obj);
		return ret;
	}
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
	drm_gem_object_put(&obj->base);
#else
	drm_gem_object_put_unlocked(&obj->base);
#endif
	*handle_p = handle;
	return 0;
}

static int evdi_align_pitch(int width, int cpp)
{
	int aligned = width;
	int pitch_mask = 0;

	switch (cpp) {
	case 1:
		pitch_mask = 255;
		break;
	case 2:
		pitch_mask = 127;
		break;
	case 3:
	case 4:
		pitch_mask = 63;
		break;
	}

	aligned += pitch_mask;
	aligned &= ~pitch_mask;
	return aligned * cpp;
}

int evdi_dumb_create(struct drm_file *file,
		     struct drm_device *dev, struct drm_mode_create_dumb *args)
{
	args->pitch = evdi_align_pitch(args->width, DIV_ROUND_UP(args->bpp, 8));

	args->size = args->pitch * args->height;
	return evdi_gem_create(file, dev, args->size, &args->handle);
}

int evdi_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_flags |= VM_MIXEDMAP;

	return ret;
}

#if KERNEL_VERSION(4, 17, 0) <= LINUX_VERSION_CODE
vm_fault_t evdi_gem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
#else
int evdi_gem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
#endif
	struct evdi_gem_object *obj = to_evdi_bo(vma->vm_private_data);
	struct page *page;
	unsigned int page_offset;
	int ret = 0;

	page_offset = (vmf->address - vma->vm_start) >> PAGE_SHIFT;

	if (!obj->pages)
		return VM_FAULT_SIGBUS;

	page = obj->pages[page_offset];
	ret = vm_insert_page(vma, vmf->address, page);
	switch (ret) {
	case -EAGAIN:
	case 0:
	case -ERESTARTSYS:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
	return VM_FAULT_SIGBUS;
}

static int evdi_gem_get_pages(struct evdi_gem_object *obj,
			      __always_unused gfp_t gfpmask)
{
	struct page **pages;

	if (obj->pages)
		return 0;

	pages = drm_gem_get_pages(&obj->base);

	if (IS_ERR(pages))
		return PTR_ERR(pages);

	obj->pages = pages;

#if defined(CONFIG_X86)
	drm_clflush_pages(obj->pages, obj->base.size / PAGE_SIZE);
#endif

	return 0;
}

static void evdi_gem_put_pages(struct evdi_gem_object *obj)
{
	if (obj->base.import_attach) {
		kvfree(obj->pages);
		obj->pages = NULL;
		return;
	}

	drm_gem_put_pages(&obj->base, obj->pages, false, false);
	obj->pages = NULL;
}

int evdi_gem_vmap(struct evdi_gem_object *obj)
{
	int page_count = obj->base.size / PAGE_SIZE;
	int ret;

	if (obj->base.import_attach) {
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		struct dma_buf_map map;

		ret = dma_buf_vmap(obj->base.import_attach->dmabuf, &map);
		if (ret)
			return -ENOMEM;
		obj->vmapping = map.vaddr;
#else
		obj->vmapping = dma_buf_vmap(obj->base.import_attach->dmabuf);
		if (!obj->vmapping)
			return -ENOMEM;
#endif
		return 0;
	}

	ret = evdi_gem_get_pages(obj, GFP_KERNEL);
	if (ret)
		return ret;

	obj->vmapping = vmap(obj->pages, page_count, 0, PAGE_KERNEL);
	if (!obj->vmapping)
		return -ENOMEM;
	return 0;
}

void evdi_gem_vunmap(struct evdi_gem_object *obj)
{
	if (obj->base.import_attach) {
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(obj->vmapping);

		dma_buf_vunmap(obj->base.import_attach->dmabuf, &map);
#else
		dma_buf_vunmap(obj->base.import_attach->dmabuf, obj->vmapping);
#endif
		obj->vmapping = NULL;
		return;
	}

	if (obj->vmapping) {
		vunmap(obj->vmapping);
		obj->vmapping = NULL;
	}

	evdi_gem_put_pages(obj);
}

void evdi_gem_free_object(struct drm_gem_object *gem_obj)
{
	struct evdi_gem_object *obj = to_evdi_bo(gem_obj);

	if (obj->vmapping)
		evdi_gem_vunmap(obj);

	if (gem_obj->import_attach)
		drm_prime_gem_destroy(gem_obj, obj->sg);

	if (obj->pages)
		evdi_gem_put_pages(obj);

	if (gem_obj->dev->vma_offset_manager)
		drm_gem_free_mmap_offset(gem_obj);
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE || defined(EL8)
	dma_resv_fini(&obj->_resv);
#else
	reservation_object_fini(&obj->_resv);
#endif
	obj->resv = NULL;
}

/*
 * the dumb interface doesn't work with the GEM straight MMAP
 * interface, it expects to do MMAP on the drm fd, like normal
 */
int evdi_gem_mmap(struct drm_file *file,
		  struct drm_device *dev, uint32_t handle, uint64_t *offset)
{
	struct evdi_gem_object *gobj;
	struct drm_gem_object *obj;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);
	obj = drm_gem_object_lookup(file, handle);
	if (obj == NULL) {
		ret = -ENOENT;
		goto unlock;
	}
	gobj = to_evdi_bo(obj);

	ret = evdi_gem_get_pages(gobj, GFP_KERNEL);
	if (ret)
		goto out;

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&gobj->base.vma_node);

 out:
	drm_gem_object_put(&gobj->base);
 unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

struct drm_gem_object *
evdi_prime_import_sg_table(struct drm_device *dev,
			   struct dma_buf_attachment *attach,
			   struct sg_table *sg)
{
	struct evdi_gem_object *obj;
	int npages;

	obj = evdi_gem_alloc_object(dev, attach->dmabuf->size);
	if (IS_ERR(obj))
		return ERR_CAST(obj);

	npages = PAGE_ALIGN(attach->dmabuf->size) / PAGE_SIZE;
	DRM_DEBUG_PRIME("Importing %d pages\n", npages);
	obj->pages = kvmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!obj->pages) {
		evdi_gem_free_object(&obj->base);
		return ERR_PTR(-ENOMEM);
	}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	drm_prime_sg_to_page_array(sg, obj->pages, npages);
#else
	drm_prime_sg_to_page_addr_arrays(sg, obj->pages, NULL, npages);
#endif
	obj->sg = sg;
	return &obj->base;
}

struct sg_table *evdi_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct evdi_gem_object *bo = to_evdi_bo(obj);
	#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		return drm_prime_pages_to_sg(obj->dev, bo->pages, bo->base.size >> PAGE_SHIFT);
	#else
		return drm_prime_pages_to_sg(bo->pages, bo->base.size >> PAGE_SHIFT);
	#endif
}

