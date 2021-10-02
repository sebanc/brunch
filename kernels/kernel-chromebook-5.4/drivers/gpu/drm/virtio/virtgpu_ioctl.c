/*
 * Copyright (C) 2015 Red Hat, Inc.
 * All Rights Reserved.
 *
 * Authors:
 *    Dave Airlie
 *    Alon Levy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/sync_file.h>

#include <drm/drm_file.h>
#include <drm/ttm/ttm_execbuf_util.h>
#include <drm/virtgpu_drm.h>

#include "virtgpu_drv.h"

#define VIRTGPU_BLOB_FLAG_MASK (VIRTGPU_BLOB_FLAG_MAPPABLE | \
				VIRTGPU_BLOB_FLAG_SHAREABLE | \
				VIRTGPU_BLOB_FLAG_CROSS_DEVICE)

static void convert_to_hw_box(struct virtio_gpu_box *dst,
			      const struct drm_virtgpu_3d_box *src)
{
	dst->x = cpu_to_le32(src->x);
	dst->y = cpu_to_le32(src->y);
	dst->z = cpu_to_le32(src->z);
	dst->w = cpu_to_le32(src->w);
	dst->h = cpu_to_le32(src->h);
	dst->d = cpu_to_le32(src->d);
}

static int virtio_gpu_map_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct drm_virtgpu_map *virtio_gpu_map = data;

	return virtio_gpu_mode_dumb_mmap(file_priv, vgdev->ddev,
					 virtio_gpu_map->handle,
					 &virtio_gpu_map->offset);
}

int virtio_gpu_object_list_validate(struct ww_acquire_ctx *ticket,
				    struct list_head *head)
{
	struct ttm_operation_ctx ctx = { false, false };
	struct ttm_validate_buffer *buf;
	struct ttm_buffer_object *bo;
	struct virtio_gpu_object *qobj;
	int ret;

	ret = ttm_eu_reserve_buffers(ticket, head, true, NULL, true);
	if (ret != 0)
		return ret;

	list_for_each_entry(buf, head, head) {
		bo = buf->bo;
		qobj = container_of(bo, struct virtio_gpu_object, tbo);
		ret = ttm_bo_validate(bo, &qobj->placement, &ctx);
		if (ret) {
			ttm_eu_backoff_reservation(ticket, head);
			return ret;
		}
	}
	return 0;
}

void virtio_gpu_unref_list(struct list_head *head)
{
	struct ttm_validate_buffer *buf;
	struct ttm_buffer_object *bo;
	struct virtio_gpu_object *qobj;

	list_for_each_entry(buf, head, head) {
		bo = buf->bo;
		qobj = container_of(bo, struct virtio_gpu_object, tbo);

		drm_gem_object_put(&qobj->gem_base);
	}
}

/* A context was already created when the device file was .open() by userspace,
 * so it is destroyed, and recreated with the same ctx_id, but now contains
 * meaningful context_init flags.
 */
static void virtio_gpu_create_context(struct virtio_gpu_device *vgdev,
				      struct virtio_gpu_fpriv *vfpriv)
{
	char dbgname[TASK_COMM_LEN];

	virtio_gpu_cmd_context_destroy(vgdev, vfpriv->ctx_id);

	get_task_comm(dbgname, current);
	virtio_gpu_cmd_context_create(vgdev, vfpriv->ctx_id,
				      vfpriv->context_init, strlen(dbgname),
				      dbgname);
}

/*
 * Usage of execbuffer:
 * Relocations need to take into account the full VIRTIO_GPUDrawable size.
 * However, the command as passed from user space must *not* contain the initial
 * VIRTIO_GPUReleaseInfo struct (first XXX bytes)
 */
static int virtio_gpu_execbuffer_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *drm_file)
{
	struct drm_virtgpu_execbuffer *exbuf = data;
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct virtio_gpu_fpriv *vfpriv = drm_file->driver_priv;
	struct drm_gem_object *gobj;
	struct virtio_gpu_fence *out_fence;
	struct virtio_gpu_object *qobj;
	int ret;
	uint32_t *bo_handles = NULL;
	void __user *user_bo_handles = NULL;
	struct list_head validate_list;
	struct ttm_validate_buffer *buflist = NULL;
	int i;
	struct ww_acquire_ctx ticket;
	struct sync_file *sync_file;
	int in_fence_fd = exbuf->fence_fd;
	int out_fence_fd = -1;
	void *buf;

	if (vgdev->has_virgl_3d == false)
		return -ENOSYS;

	if ((exbuf->flags & ~VIRTGPU_EXECBUF_FLAGS))
		return -EINVAL;

	exbuf->fence_fd = -1;

	if (exbuf->flags & VIRTGPU_EXECBUF_FENCE_FD_IN) {
		struct dma_fence *in_fence;

		in_fence = sync_file_get_fence(in_fence_fd);

		if (!in_fence)
			return -EINVAL;

		/*
		 * Wait if the fence is from a foreign context, or if the fence
		 * array contains any fence from a foreign context.
		 */
		ret = 0;
		if (!dma_fence_match_context(in_fence, vgdev->fence_drv.context))
			ret = dma_fence_wait(in_fence, true);

		dma_fence_put(in_fence);
		if (ret)
			return ret;
	}

	if (exbuf->flags & VIRTGPU_EXECBUF_FENCE_FD_OUT) {
		out_fence_fd = get_unused_fd_flags(O_CLOEXEC);
		if (out_fence_fd < 0)
			return out_fence_fd;
	}

	INIT_LIST_HEAD(&validate_list);
	if (exbuf->num_bo_handles) {

		bo_handles = kvmalloc_array(exbuf->num_bo_handles,
					   sizeof(uint32_t), GFP_KERNEL);
		buflist = kvmalloc_array(exbuf->num_bo_handles,
					   sizeof(struct ttm_validate_buffer),
					   GFP_KERNEL | __GFP_ZERO);
		if (!bo_handles || !buflist) {
			ret = -ENOMEM;
			goto out_unused_fd;
		}

		user_bo_handles = u64_to_user_ptr(exbuf->bo_handles);
		if (copy_from_user(bo_handles, user_bo_handles,
				   exbuf->num_bo_handles * sizeof(uint32_t))) {
			ret = -EFAULT;
			goto out_unused_fd;
		}

		for (i = 0; i < exbuf->num_bo_handles; i++) {
			gobj = drm_gem_object_lookup(drm_file, bo_handles[i]);
			if (!gobj) {
				ret = -ENOENT;
				goto out_unused_fd;
			}

			qobj = gem_to_virtio_gpu_obj(gobj);
			buflist[i].bo = &qobj->tbo;

			list_add(&buflist[i].head, &validate_list);
		}
		kvfree(bo_handles);
		bo_handles = NULL;
	}

	ret = virtio_gpu_object_list_validate(&ticket, &validate_list);
	if (ret)
		goto out_free;

	buf = vmemdup_user(u64_to_user_ptr(exbuf->command), exbuf->size);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto out_unresv;
	}

	out_fence = virtio_gpu_fence_alloc(vgdev);
	if(!out_fence) {
		ret = -ENOMEM;
		goto out_memdup;
	}

	if (out_fence_fd >= 0) {
		sync_file = sync_file_create(&out_fence->f);
		if (!sync_file) {
			dma_fence_put(&out_fence->f);
			ret = -ENOMEM;
			goto out_memdup;
		}

		exbuf->fence_fd = out_fence_fd;
		fd_install(out_fence_fd, sync_file->file);
	}

	virtio_gpu_cmd_submit(vgdev, buf, exbuf->size,
			      vfpriv->ctx_id, out_fence);

	ttm_eu_fence_buffer_objects(&ticket, &validate_list, &out_fence->f);

	/* fence the command bo */
	virtio_gpu_unref_list(&validate_list);
	kvfree(buflist);
	dma_fence_put(&out_fence->f);
	return 0;

out_memdup:
	kvfree(buf);
out_unresv:
	ttm_eu_backoff_reservation(&ticket, &validate_list);
out_free:
	virtio_gpu_unref_list(&validate_list);
out_unused_fd:
	kvfree(bo_handles);
	kvfree(buflist);

	if (out_fence_fd >= 0)
		put_unused_fd(out_fence_fd);

	return ret;
}

static int virtio_gpu_getparam_ioctl(struct drm_device *dev, void *data,
				     struct drm_file *file_priv)
{
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct drm_virtgpu_getparam *param = data;
	int value;

	switch (param->param) {
	case VIRTGPU_PARAM_3D_FEATURES:
		value = vgdev->has_virgl_3d == true ? 1 : 0;
		break;
	case VIRTGPU_PARAM_CAPSET_QUERY_FIX:
		value = 1;
		break;
	case VIRTGPU_PARAM_RESOURCE_BLOB:
		value = vgdev->has_resource_blob == true ? 1 : 0;
		break;
	case VIRTGPU_PARAM_HOST_VISIBLE:
		value = vgdev->has_host_visible == true ? 1 : 0;
		break;
	case VIRTGPU_PARAM_CROSS_DEVICE:
		value = vgdev->has_resource_assign_uuid == true ? 1 : 0;
		break;
	case VIRTGPU_PARAM_CONTEXT_INIT:
		value = vgdev->has_context_init == true ? 1 : 0;
		break;
	case VIRTGPU_PARAM_SUPPORTED_CAPSET_IDs:
		value = vgdev->capset_id_mask;
		break;
	default:
		return -EINVAL;
	}
	if (copy_to_user(u64_to_user_ptr(param->value), &value, sizeof(int)))
		return -EFAULT;

	return 0;
}

static int virtio_gpu_resource_create_ioctl(struct drm_device *dev, void *data,
					    struct drm_file *file_priv)
{
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct drm_virtgpu_resource_create *rc = data;
	struct virtio_gpu_fence *fence;
	int ret;
	struct virtio_gpu_object *qobj;
	struct drm_gem_object *obj;
	uint32_t handle = 0;
	struct virtio_gpu_object_params params = { 0 };

	if (vgdev->has_virgl_3d == false) {
		if (rc->depth > 1)
			return -EINVAL;
		if (rc->nr_samples > 1)
			return -EINVAL;
		if (rc->last_level > 1)
			return -EINVAL;
		if (rc->target != 2)
			return -EINVAL;
		if (rc->array_size > 1)
			return -EINVAL;
	}

	params.format = rc->format;
	params.width = rc->width;
	params.height = rc->height;
	params.size = rc->size;
	if (vgdev->has_virgl_3d) {
		params.virgl = true;
		params.target = rc->target;
		params.bind = rc->bind;
		params.depth = rc->depth;
		params.array_size = rc->array_size;
		params.last_level = rc->last_level;
		params.nr_samples = rc->nr_samples;
		params.flags = rc->flags;
	}
	/* allocate a single page size object */
	if (params.size == 0)
		params.size = PAGE_SIZE;

	fence = virtio_gpu_fence_alloc(vgdev);
	if (!fence)
		return -ENOMEM;
	qobj = virtio_gpu_alloc_object(dev, &params, fence);
	dma_fence_put(&fence->f);
	if (IS_ERR(qobj))
		return PTR_ERR(qobj);
	obj = &qobj->gem_base;

	ret = drm_gem_handle_create(file_priv, obj, &handle);
	if (ret) {
		drm_gem_object_release(obj);
		return ret;
	}
	drm_gem_object_put(obj);

	rc->res_handle = qobj->hw_res_handle; /* similiar to a VM address */
	rc->bo_handle = handle;
	return 0;
}

static int virtio_gpu_resource_info_cros_ioctl(struct drm_device *dev,
					       void *data,
					       struct drm_file *file_priv)
{
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct drm_virtgpu_resource_info_cros *ri = data;
	struct drm_gem_object *gobj = NULL;
	struct virtio_gpu_object *qobj = NULL;
	int ret = 0;

	if (ri->type != VIRTGPU_RESOURCE_INFO_TYPE_DEFAULT &&
	    ri->type != VIRTGPU_RESOURCE_INFO_TYPE_EXTENDED)
		return -EINVAL;

	gobj = drm_gem_object_lookup(file_priv, ri->bo_handle);
	if (gobj == NULL)
		return -ENOENT;

	qobj = gem_to_virtio_gpu_obj(gobj);

	ri->res_handle = qobj->hw_res_handle;
	ri->size = qobj->gem_base.size;

	if (ri->type == VIRTGPU_RESOURCE_INFO_TYPE_DEFAULT) {
		ri->blob_mem = qobj->blob_mem;
		goto out;
	} else if (qobj->blob_mem == VIRTGPU_BLOB_MEM_GUEST) {
		ret = -EINVAL;
		goto out;
	}

	ri->stride = 0;
	if (qobj->blob_mem)
		goto out;

	if (!qobj->create_callback_done) {
		ret = wait_event_interruptible(vgdev->resp_wq,
					       qobj->create_callback_done);
		if (ret)
			goto out;
	}

	if (qobj->num_planes) {
		int i;

		ri->num_planes = qobj->num_planes;
		for (i = 0; i < qobj->num_planes; i++) {
			ri->strides[i] = qobj->strides[i];
			ri->offsets[i] = qobj->offsets[i];
		}
	}

	ri->format_modifier = qobj->format_modifier;
out:
	drm_gem_object_put(gobj);
	return ret;
}

static int virtio_gpu_transfer_from_host_ioctl(struct drm_device *dev,
					       void *data,
					       struct drm_file *file)
{
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct virtio_gpu_fpriv *vfpriv = file->driver_priv;
	struct drm_virtgpu_3d_transfer_from_host *args = data;
	struct ttm_operation_ctx ctx = { true, false };
	struct drm_gem_object *gobj = NULL;
	struct virtio_gpu_object *qobj = NULL;
	struct virtio_gpu_fence *fence;
	int ret;
	u32 offset = args->offset;
	struct virtio_gpu_box box;

	if (vgdev->has_virgl_3d == false)
		return -ENOSYS;

	gobj = drm_gem_object_lookup(file, args->bo_handle);
	if (gobj == NULL)
		return -ENOENT;

	qobj = gem_to_virtio_gpu_obj(gobj);

	ret = virtio_gpu_object_reserve(qobj, false);
	if (ret)
		goto out;

	ret = ttm_bo_validate(&qobj->tbo, &qobj->placement, &ctx);
	if (unlikely(ret))
		goto out_unres;

	convert_to_hw_box(&box, &args->box);

	fence = virtio_gpu_fence_alloc(vgdev);
	if (!fence) {
		ret = -ENOMEM;
		goto out_unres;
	}
	virtio_gpu_cmd_transfer_from_host_3d
		(vgdev, qobj->hw_res_handle,
		 vfpriv->ctx_id, offset, args->level,
		 &box, fence);
	dma_resv_add_excl_fence(qobj->tbo.base.resv,
					  &fence->f);

	dma_fence_put(&fence->f);
out_unres:
	virtio_gpu_object_unreserve(qobj);
out:
	drm_gem_object_put(gobj);
	return ret;
}

static int virtio_gpu_transfer_to_host_ioctl(struct drm_device *dev, void *data,
					     struct drm_file *file)
{
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct virtio_gpu_fpriv *vfpriv = file->driver_priv;
	struct drm_virtgpu_3d_transfer_to_host *args = data;
	struct ttm_operation_ctx ctx = { true, false };
	struct drm_gem_object *gobj = NULL;
	struct virtio_gpu_object *qobj = NULL;
	struct virtio_gpu_fence *fence;
	struct virtio_gpu_box box;
	int ret;
	u32 offset = args->offset;

	gobj = drm_gem_object_lookup(file, args->bo_handle);
	if (gobj == NULL)
		return -ENOENT;

	qobj = gem_to_virtio_gpu_obj(gobj);

	ret = virtio_gpu_object_reserve(qobj, false);
	if (ret)
		goto out;

	ret = ttm_bo_validate(&qobj->tbo, &qobj->placement, &ctx);
	if (unlikely(ret))
		goto out_unres;

	convert_to_hw_box(&box, &args->box);
	if (!vgdev->has_virgl_3d) {
		virtio_gpu_cmd_transfer_to_host_2d
			(vgdev, qobj, offset,
			 box.w, box.h, box.x, box.y, NULL);
	} else {
		fence = virtio_gpu_fence_alloc(vgdev);
		if (!fence) {
			ret = -ENOMEM;
			goto out_unres;
		}
		virtio_gpu_cmd_transfer_to_host_3d
			(vgdev, qobj,
			 vfpriv ? vfpriv->ctx_id : 0, offset,
			 args->level, &box, fence);
		dma_resv_add_excl_fence(qobj->tbo.base.resv,
						  &fence->f);
		dma_fence_put(&fence->f);
	}

out_unres:
	virtio_gpu_object_unreserve(qobj);
out:
	drm_gem_object_put(gobj);
	return ret;
}

static int virtio_gpu_wait_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file)
{
	struct drm_virtgpu_3d_wait *args = data;
	struct drm_gem_object *gobj = NULL;
	struct virtio_gpu_object *qobj = NULL;
	int ret;
	bool nowait = false;

	gobj = drm_gem_object_lookup(file, args->handle);
	if (gobj == NULL)
		return -ENOENT;

	qobj = gem_to_virtio_gpu_obj(gobj);

	if (args->flags & VIRTGPU_WAIT_NOWAIT)
		nowait = true;
	ret = virtio_gpu_object_wait(qobj, nowait);

	drm_gem_object_put(gobj);
	return ret;
}

static int virtio_gpu_get_caps_ioctl(struct drm_device *dev,
				void *data, struct drm_file *file)
{
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct drm_virtgpu_get_caps *args = data;
	unsigned size, host_caps_size;
	int i;
	int found_valid = -1;
	int ret;
	struct virtio_gpu_drv_cap_cache *cache_ent;
	void *ptr;

	if (vgdev->num_capsets == 0)
		return -ENOSYS;

	/* don't allow userspace to pass 0 */
	if (args->size == 0)
		return -EINVAL;

	spin_lock(&vgdev->display_info_lock);
	for (i = 0; i < vgdev->num_capsets; i++) {
		if (vgdev->capsets[i].id == args->cap_set_id) {
			if (vgdev->capsets[i].max_version >= args->cap_set_ver) {
				found_valid = i;
				break;
			}
		}
	}

	if (found_valid == -1) {
		spin_unlock(&vgdev->display_info_lock);
		return -EINVAL;
	}

	host_caps_size = vgdev->capsets[found_valid].max_size;
	/* only copy to user the minimum of the host caps size or the guest caps size */
	size = min(args->size, host_caps_size);

	list_for_each_entry(cache_ent, &vgdev->cap_cache, head) {
		if (cache_ent->id == args->cap_set_id &&
		    cache_ent->version == args->cap_set_ver) {
			spin_unlock(&vgdev->display_info_lock);
			goto copy_exit;
		}
	}
	spin_unlock(&vgdev->display_info_lock);

	/* not in cache - need to talk to hw */
	virtio_gpu_cmd_get_capset(vgdev, found_valid, args->cap_set_ver,
				  &cache_ent);

copy_exit:
	ret = wait_event_timeout(vgdev->resp_wq,
				 atomic_read(&cache_ent->is_valid), 5 * HZ);
	if (!ret)
		return -EBUSY;

	/* is_valid check must proceed before copy of the cache entry. */
	smp_rmb();

	ptr = cache_ent->caps_cache;

	if (copy_to_user(u64_to_user_ptr(args->addr), ptr, size))
		return -EFAULT;

	return 0;
}

static int virtio_gpu_resource_create_blob_ioctl(struct drm_device *dev,
				void *data, struct drm_file *file)
{
	int ret, si, nents;
	uint32_t handle = 0;
	uint32_t ctx_id;
	struct scatterlist *sg;
	struct virtio_gpu_object *obj;
	struct virtio_gpu_fence *fence;
	struct virtio_gpu_mem_entry *ents;
	struct drm_virtgpu_resource_create_blob *rc_blob = data;
	struct virtio_gpu_object_params params = { 0 };
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct virtio_gpu_fpriv *vfpriv = file->driver_priv;
	bool use_dma_api = !virtio_has_iommu_quirk(vgdev->vdev);
	bool mappable = rc_blob->blob_flags & VIRTGPU_BLOB_FLAG_MAPPABLE;
	bool has_guest = (rc_blob->blob_mem == VIRTGPU_BLOB_MEM_GUEST ||
	                  rc_blob->blob_mem == VIRTGPU_BLOB_MEM_HOST3D_GUEST);

	params.size = rc_blob->size;
	params.blob_mem = rc_blob->blob_mem;
	params.blob = true;
	ctx_id = vfpriv ? vfpriv->ctx_id : 0;

	if (rc_blob->blob_flags & ~VIRTGPU_BLOB_FLAG_MASK ||
	    !rc_blob->blob_flags) {
		return -EINVAL;
	}

	if (rc_blob->blob_flags & VIRTGPU_BLOB_FLAG_CROSS_DEVICE &&
	    !vgdev->has_resource_assign_uuid)
		return -EINVAL;

	if (rc_blob->cmd_size) {
		void *buf;
	        /* gets freed when the ring has consumed it */
		buf = memdup_user(u64_to_user_ptr(rc_blob->cmd),
				  rc_blob->cmd_size);

		if (IS_ERR(buf))
			return PTR_ERR(buf);

		virtio_gpu_cmd_submit(vgdev, buf, rc_blob->cmd_size,
				      ctx_id, NULL);
	}

	obj = virtio_gpu_alloc_object(dev, &params, NULL);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	if (!obj->pages) {
                ret = virtio_gpu_object_get_sg_table(vgdev, obj);
                if (ret)
			goto err_free_obj;
        }

	if (!has_guest) {
		nents = 0;
	} else if (use_dma_api) {
                obj->mapped = dma_map_sg(vgdev->vdev->dev.parent,
                                         obj->pages->sgl, obj->pages->nents,
                                         DMA_TO_DEVICE);
                nents = obj->mapped;
        } else {
                nents = obj->pages->nents;
        }

	/* gets freed when the ring has consumed it */
	ents = kzalloc(nents * sizeof(struct virtio_gpu_mem_entry), GFP_KERNEL);
	if (!ents) {
		ret = -ENOMEM;
		goto err_free_obj;
	}

	if (has_guest) {
		for_each_sg(obj->pages->sgl, sg, nents, si) {
			ents[si].addr = cpu_to_le64(use_dma_api
						    ? sg_dma_address(sg)
						    : sg_phys(sg));
			ents[si].length = cpu_to_le32(sg->length);
			ents[si].padding = 0;
		}
	}

	fence = virtio_gpu_fence_alloc(vgdev);
	if (!fence) {
		ret = -ENOMEM;
		goto err_free_ents;
	}

	if (has_guest) {
		virtio_gpu_cmd_resource_create_blob_guest(vgdev, obj, ctx_id,
							  rc_blob->blob_mem,
							  rc_blob->blob_flags,
							  rc_blob->blob_id,
							  rc_blob->size,
							  nents, ents);
	} else {
		virtio_gpu_cmd_resource_create_blob(vgdev, obj, ctx_id,
						    rc_blob->blob_mem,
						    rc_blob->blob_flags,
						    rc_blob->blob_id,
						    rc_blob->size,
						    nents, ents);
	}

	ret = drm_gem_handle_create(file, &obj->gem_base, &handle);
	if (ret)
		goto err_fence_put;

	if (!has_guest && mappable) {
		virtio_gpu_cmd_map(vgdev, obj, obj->tbo.offset, fence);
		obj->mapped = 1;
	}

	ret = virtio_gpu_object_reserve(obj, false);
	if (ret)
		goto err_unmap;

	dma_resv_add_excl_fence(obj->tbo.base.resv, &fence->f);

	virtio_gpu_object_unreserve(obj);
	dma_fence_put(&fence->f);
	drm_gem_object_put(&obj->gem_base);

	rc_blob->res_handle = obj->hw_res_handle;
	rc_blob->bo_handle = handle;
	return 0;

err_unmap:
	if (obj->mapped)
		virtio_gpu_cmd_unmap(vgdev, obj->hw_res_handle);
	drm_gem_handle_delete(file, handle);
err_fence_put:
	dma_fence_put(&fence->f);
err_free_ents:
	kfree(ents);
err_free_obj:
	drm_gem_object_release(&obj->gem_base);
	return ret;
}

static int virtio_gpu_context_init_ioctl(struct drm_device *dev,
					 void *data, struct drm_file *file)
{
	int ret = 0;
	uint32_t num_params, i, param, value;
	size_t len;
	struct drm_virtgpu_context_set_param *ctx_set_params;
	struct virtio_gpu_device *vgdev = dev->dev_private;
	struct virtio_gpu_fpriv *vfpriv = file->driver_priv;
	struct drm_virtgpu_context_init *args = data;

	num_params = args->num_params;
	len = num_params * sizeof(struct drm_virtgpu_context_set_param);

	if (!vgdev->has_context_init || !vgdev->has_virgl_3d)
		return -EINVAL;

	/* Number of unique parameters supported at this time. */
	if (num_params > 1)
		return -EINVAL;

	ctx_set_params = memdup_user(u64_to_user_ptr(args->ctx_set_params),
				     len);

	if (IS_ERR(ctx_set_params))
		return PTR_ERR(ctx_set_params);

	for (i = 0; i < num_params; i++) {
		param = ctx_set_params[i].param;
		value = ctx_set_params[i].value;

		switch (param) {
		case VIRTGPU_CONTEXT_PARAM_CAPSET_ID:
			if (value > MAX_CAPSET_ID)
				return -EINVAL;

			if ((vgdev->capset_id_mask & (1 << value)) == 0)
				return -EINVAL;

			/* Context capset ID already set */
			if (vfpriv->context_init &
			    VIRTIO_GPU_CONTEXT_INIT_CAPSET_ID_MASK)
				return -EINVAL;

			vfpriv->context_init |= value;
			break;
		default:
			return -EINVAL;
		}
	}

	virtio_gpu_create_context(vgdev, vfpriv);

	return ret;
}

struct drm_ioctl_desc virtio_gpu_ioctls[DRM_VIRTIO_NUM_IOCTLS] = {
	DRM_IOCTL_DEF_DRV(VIRTGPU_MAP, virtio_gpu_map_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_EXECBUFFER, virtio_gpu_execbuffer_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_GETPARAM, virtio_gpu_getparam_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_RESOURCE_CREATE,
			  virtio_gpu_resource_create_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_RESOURCE_INFO_CROS,
			  virtio_gpu_resource_info_cros_ioctl,
			  DRM_RENDER_ALLOW),

	/* make transfer async to the main ring? - no sure, can we
	 * thread these in the underlying GL
	 */
	DRM_IOCTL_DEF_DRV(VIRTGPU_TRANSFER_FROM_HOST,
			  virtio_gpu_transfer_from_host_ioctl,
			  DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(VIRTGPU_TRANSFER_TO_HOST,
			  virtio_gpu_transfer_to_host_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_WAIT, virtio_gpu_wait_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_GET_CAPS, virtio_gpu_get_caps_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_RESOURCE_CREATE_BLOB,
			  virtio_gpu_resource_create_blob_ioctl,
			  DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF_DRV(VIRTGPU_CONTEXT_INIT,
			  virtio_gpu_context_init_ioctl,
			  DRM_RENDER_ALLOW)
	};
