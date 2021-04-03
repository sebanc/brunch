// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2018 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <drm/drmP.h>
#include "evdi_drv.h"
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>

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

	dma_resv_init(&obj->_resv);
	obj->resv = &obj->_resv;

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

	drm_gem_object_put_unlocked(&obj->base);
	*handle_p = handle;
	return 0;
}

int evdi_dumb_create(struct drm_file *file,
		     struct drm_device *dev, struct drm_mode_create_dumb *args)
{
	args->pitch = args->width * DIV_ROUND_UP(args->bpp, 8);
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

vm_fault_t evdi_gem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct evdi_gem_object *obj = to_evdi_bo(vma->vm_private_data);
	struct page *page;
	unsigned int page_offset;
	int ret = 0;

	page_offset = (vmf->address - vma->vm_start) >> PAGE_SHIFT;

	if (!obj->pages)
		return VM_FAULT_SIGBUS;

	page = obj->pages[page_offset];
	ret = vm_insert_page(vma, (unsigned long)vmf->address, page);
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

static int evdi_gem_get_pages(struct evdi_gem_object *obj)
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
	if (obj->pages)
		drm_gem_put_pages(&obj->base, obj->pages, false, false);
	obj->pages = NULL;
}

int evdi_gem_vmap(struct evdi_gem_object *obj)
{
	int page_count = obj->base.size / PAGE_SIZE;
	int ret;

	if (obj->base.import_attach) {
		obj->vmapping = dma_buf_vmap(obj->base.import_attach->dmabuf);
		if (!obj->vmapping)
			return -ENOMEM;
		return 0;
	}

	ret = evdi_gem_get_pages(obj);
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
		dma_buf_vunmap(obj->base.import_attach->dmabuf, obj->vmapping);
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

	if (gem_obj->import_attach) {
		drm_prime_gem_destroy(gem_obj, obj->sg);
		put_device(gem_obj->dev->dev);
	}

	if (obj->pages)
		evdi_gem_put_pages(obj);

	if (gem_obj->dev->vma_offset_manager)
		drm_gem_free_mmap_offset(gem_obj);

	dma_resv_init(&obj->_resv);
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

	ret = evdi_gem_get_pages(gobj);
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

static int evdi_prime_create(struct drm_device *dev,
			     size_t size,
			     struct sg_table *sg,
			     struct evdi_gem_object **obj_p)
{
	struct evdi_gem_object *obj;
	int npages;

	npages = size / PAGE_SIZE;

	*obj_p = NULL;
	obj = evdi_gem_alloc_object(dev, npages * PAGE_SIZE);
	if (!obj)
		return -ENOMEM;

	obj->sg = sg;
	obj->pages = kvmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (obj->pages == NULL) {
		DRM_ERROR("obj pages is NULL %d\n", npages);
		return -ENOMEM;
	}

	drm_prime_sg_to_page_addr_arrays(sg, obj->pages, NULL, npages);

	*obj_p = obj;
	return 0;
}

struct evdi_drm_dmabuf_attachment {
	struct sg_table sgt;
	enum dma_data_direction dir;
	bool is_mapped;
};

static int evdi_attach_dma_buf(__always_unused struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attach)
{
	struct evdi_drm_dmabuf_attachment *evdi_attach;

	evdi_attach = kzalloc(sizeof(*evdi_attach), GFP_KERNEL);
	if (!evdi_attach)
		return -ENOMEM;

	evdi_attach->dir = DMA_NONE;
	attach->priv = evdi_attach;

	return 0;
}

static void evdi_detach_dma_buf(__always_unused struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct evdi_drm_dmabuf_attachment *evdi_attach = attach->priv;
	struct sg_table *sgt;

	if (!evdi_attach)
		return;

	sgt = &evdi_attach->sgt;

	if (evdi_attach->dir != DMA_NONE)
		dma_unmap_sg(attach->dev, sgt->sgl, sgt->nents,
			     evdi_attach->dir);

	sg_free_table(sgt);
	kfree(evdi_attach);
	attach->priv = NULL;
}

static struct sg_table *evdi_map_dma_buf(struct dma_buf_attachment *attach,
					 enum dma_data_direction dir)
{
	struct evdi_drm_dmabuf_attachment *evdi_attach = attach->priv;
	struct evdi_gem_object *obj = to_evdi_bo(attach->dmabuf->priv);
	struct drm_device *dev = obj->base.dev;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt = NULL;
	unsigned int i;
	int page_count;
	int nents, ret;

	DRM_DEBUG_PRIME("[DEV:%s] size:%zd dir=%d\n", dev_name(attach->dev),
			attach->dmabuf->size, dir);

	/* just return current sgt if already requested. */
	if (evdi_attach->dir == dir && evdi_attach->is_mapped)
		return &evdi_attach->sgt;

	if (!obj->pages) {
		ret = evdi_gem_get_pages(obj);
		if (ret) {
			DRM_ERROR("failed to map pages.\n");
			return ERR_PTR(ret);
		}
	}

	page_count = obj->base.size / PAGE_SIZE;
	obj->sg = drm_prime_pages_to_sg(obj->pages, page_count);
	if (IS_ERR(obj->sg)) {
		DRM_ERROR("failed to allocate sgt.\n");
		return ERR_CAST(obj->sg);
	}

	sgt = &evdi_attach->sgt;

	ret = sg_alloc_table(sgt, obj->sg->orig_nents, GFP_KERNEL);
	if (ret) {
		DRM_ERROR("failed to alloc sgt.\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_lock(&dev->struct_mutex);

	rd = obj->sg->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	if (dir != DMA_NONE) {
		nents = dma_map_sg(attach->dev, sgt->sgl, sgt->orig_nents, dir);
		if (!nents) {
			DRM_ERROR("failed to map sgl with iommu.\n");
			sg_free_table(sgt);
			sgt = ERR_PTR(-EIO);
			goto err_unlock;
		}
	}

	evdi_attach->is_mapped = true;
	evdi_attach->dir = dir;
	attach->priv = evdi_attach;

 err_unlock:
	mutex_unlock(&dev->struct_mutex);
	return sgt;
}

static void evdi_unmap_dma_buf(
			__always_unused struct dma_buf_attachment *attach,
			__always_unused struct sg_table *sgt,
			__always_unused enum dma_data_direction dir)
{
}

static void *evdi_dmabuf_kmap(__always_unused struct dma_buf *dma_buf,
			__always_unused unsigned long page_num)
{
	return NULL;
}

static void evdi_dmabuf_kunmap(
			__always_unused struct dma_buf *dma_buf,
			__always_unused unsigned long page_num,
			__always_unused void *addr)
{
}

static int evdi_dmabuf_mmap(__always_unused struct dma_buf *dma_buf,
			__always_unused struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct dma_buf_ops evdi_dmabuf_ops = {

	.attach = evdi_attach_dma_buf,
	.detach = evdi_detach_dma_buf,
	.map_dma_buf = evdi_map_dma_buf,
	.unmap_dma_buf = evdi_unmap_dma_buf,
	.map = evdi_dmabuf_kmap,
	.unmap = evdi_dmabuf_kunmap,
	.mmap = evdi_dmabuf_mmap,
	.release = drm_gem_dmabuf_release,
};

struct drm_gem_object *evdi_gem_prime_import(struct drm_device *dev,
					     struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	struct evdi_gem_object *uobj;
	int ret;

	/* check if our object */
	if (dma_buf->ops == &evdi_dmabuf_ops) {
		uobj = to_evdi_bo(dma_buf->priv);
		if (uobj->base.dev == dev) {
			drm_gem_object_get(&uobj->base);
			return &uobj->base;
		}
	}

	/* need to attach */
	get_device(dev->dev);
	attach = dma_buf_attach(dma_buf, dev->dev);
	if (IS_ERR(attach)) {
		put_device(dev->dev);
		return ERR_CAST(attach);
	}

	get_dma_buf(dma_buf);

	sg = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sg)) {
		ret = PTR_ERR(sg);
		goto fail_detach;
	}

	ret = evdi_prime_create(dev, dma_buf->size, sg, &uobj);
	if (ret)
		goto fail_unmap;

	uobj->base.import_attach = attach;
	uobj->resv = attach->dmabuf->resv;

	return &uobj->base;

 fail_unmap:
	dma_buf_unmap_attachment(attach, sg, DMA_BIDIRECTIONAL);
 fail_detach:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);
	put_device(dev->dev);
	return ERR_PTR(ret);
}

struct dma_buf *evdi_gem_prime_export(struct drm_gem_object *obj, int flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct evdi_gem_object *evdi_obj = to_evdi_bo(obj);

	exp_info.exp_name = "evdi",
	exp_info.ops = &evdi_dmabuf_ops,
	exp_info.size = obj->size,
	exp_info.flags = flags,
	exp_info.resv = evdi_obj->resv,
	exp_info.priv = obj;

	return drm_gem_dmabuf_export(obj->dev, &exp_info);
}
