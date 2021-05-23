/*
 *  Wayland Virtio Driver
 *  Copyright (C) 2017 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

/*
 * Virtio Wayland (virtio_wl or virtwl) is a virtual device that allows a guest
 * virtual machine to use a wayland server on the host transparently (to the
 * host).  This is done by proxying the wayland protocol socket stream verbatim
 * between the host and guest over 2 (recv and send) virtio queues. The guest
 * can request new wayland server connections to give each guest wayland client
 * a different server context. Each host connection's file descriptor is exposed
 * to the guest as a virtual file descriptor (VFD). Additionally, the guest can
 * request shared memory file descriptors which are also exposed as VFDs. These
 * shared memory VFDs are directly writable by the guest via device memory
 * injected by the host. Each VFD is sendable along a connection context VFD and
 * will appear as ancillary data to the wayland server, just like a message from
 * an ordinary wayland client. When the wayland server sends a shared memory
 * file descriptor to the client (such as when sending a keymap), a VFD is
 * allocated by the device automatically and its memory is injected into as
 * device memory.
 *
 * This driver is intended to be paired with the `virtwl_guest_proxy` program
 * which is run in the guest system and acts like a wayland server. It accepts
 * wayland client connections and converts their socket messages to ioctl
 * messages exposed by this driver via the `/dev/wl` device file. While it would
 * be possible to expose a unix stream socket from this driver, the user space
 * helper is much cleaner to write.
 */

#include <linux/anon_inodes.h>
#include <linux/cdev.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/scatterlist.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/virtio.h>
#include <linux/virtio_wl.h>

#include <drm/virtio_drm.h>
#include <uapi/linux/dma-buf.h>

#ifdef CONFIG_DRM_VIRTIO_GPU
#define SEND_VIRTGPU_RESOURCES
#endif

#define VFD_ILLEGAL_SIGN_BIT 0x80000000
#define VFD_HOST_VFD_ID_BIT 0x40000000

struct virtwl_vfd_qentry {
	struct list_head list;
	struct virtio_wl_ctrl_hdr *hdr;
	unsigned int len; /* total byte length of ctrl_vfd_* + vfds + data */
	unsigned int vfd_offset; /* int offset into vfds */
	unsigned int data_offset; /* byte offset into data */
};

struct virtwl_vfd {
	struct kobject kobj;
	struct mutex lock;

	struct virtwl_info *vi;
	uint32_t id;
	uint32_t flags;
	uint64_t pfn;
	uint32_t size;
	bool hungup;

	struct list_head in_queue; /* list of virtwl_vfd_qentry */
	wait_queue_head_t in_waitq;
};

struct virtwl_info {
	dev_t dev_num;
	struct device *dev;
	struct class *class;
	struct cdev cdev;

	struct mutex vq_locks[VIRTWL_QUEUE_COUNT];
	struct virtqueue *vqs[VIRTWL_QUEUE_COUNT];
	struct work_struct in_vq_work;
	struct work_struct out_vq_work;

	wait_queue_head_t out_waitq;

	struct mutex vfds_lock;
	struct idr vfds;
};

static struct virtwl_vfd *virtwl_vfd_alloc(struct virtwl_info *vi);
static void virtwl_vfd_free(struct virtwl_vfd *vfd);

static const struct file_operations virtwl_vfd_fops;

static int virtwl_resp_err(unsigned int type)
{
	switch (type) {
	case VIRTIO_WL_RESP_OK:
	case VIRTIO_WL_RESP_VFD_NEW:
	case VIRTIO_WL_RESP_VFD_NEW_DMABUF:
		return 0;
	case VIRTIO_WL_RESP_ERR:
		return -ENODEV; /* Device is no longer reliable */
	case VIRTIO_WL_RESP_OUT_OF_MEMORY:
		return -ENOMEM;
	case VIRTIO_WL_RESP_INVALID_ID:
		return -ENOENT;
	case VIRTIO_WL_RESP_INVALID_TYPE:
		return -EINVAL;
	case VIRTIO_WL_RESP_INVALID_FLAGS:
		return -EPERM;
	case VIRTIO_WL_RESP_INVALID_CMD:
		return -ENOTTY;
	default:
		return -EPROTO;
	}
}

static int vq_return_inbuf_locked(struct virtqueue *vq, void *buffer)
{
	int ret;
	struct scatterlist sg[1];

	sg_init_one(sg, buffer, PAGE_SIZE);

	ret = virtqueue_add_inbuf(vq, sg, 1, buffer, GFP_KERNEL);
	if (ret) {
		pr_warn("virtwl: failed to give inbuf to host: %d\n", ret);
		return ret;
	}

	return 0;
}

static int vq_queue_out(struct virtwl_info *vi, struct scatterlist *out_sg,
			struct scatterlist *in_sg,
			struct completion *finish_completion,
			bool nonblock)
{
	struct virtqueue *vq = vi->vqs[VIRTWL_VQ_OUT];
	struct mutex *vq_lock = &vi->vq_locks[VIRTWL_VQ_OUT];
	struct scatterlist *sgs[] = { out_sg, in_sg };
	int ret = 0;

	mutex_lock(vq_lock);
	while ((ret = virtqueue_add_sgs(vq, sgs, 1, 1, finish_completion,
					GFP_KERNEL)) == -ENOSPC) {
		mutex_unlock(vq_lock);
		if (nonblock)
			return -EAGAIN;
		if (!wait_event_timeout(vi->out_waitq, vq->num_free > 0, HZ))
			return -EBUSY;
		mutex_lock(vq_lock);
	}
	if (!ret)
		virtqueue_kick(vq);
	mutex_unlock(vq_lock);

	return ret;
}

static int vq_fill_locked(struct virtqueue *vq)
{
	void *buffer;
	int ret = 0;

	while (vq->num_free > 0) {
		buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!buffer) {
			ret = -ENOMEM;
			goto clear_queue;
		}

		ret = vq_return_inbuf_locked(vq, buffer);
		if (ret)
			goto clear_queue;
	}

	return 0;

clear_queue:
	while ((buffer = virtqueue_detach_unused_buf(vq)))
		kfree(buffer);
	return ret;
}

static bool vq_handle_new(struct virtwl_info *vi,
			  struct virtio_wl_ctrl_vfd_new *new, unsigned int len)
{
	struct virtwl_vfd *vfd;
	u32 id = new->vfd_id;
	int ret;

	if (id == 0)
		return true; /* return the inbuf to vq */

	if (!(id & VFD_HOST_VFD_ID_BIT) || (id & VFD_ILLEGAL_SIGN_BIT)) {
		pr_warn("virtwl: received a vfd with invalid id: %u\n", id);
		return true; /* return the inbuf to vq */
	}

	vfd = virtwl_vfd_alloc(vi);
	if (!vfd)
		return true; /* return the inbuf to vq */

	mutex_lock(&vi->vfds_lock);
	ret = idr_alloc(&vi->vfds, vfd, id, id + 1, GFP_KERNEL);
	mutex_unlock(&vi->vfds_lock);

	if (ret <= 0) {
		virtwl_vfd_free(vfd);
		pr_warn("virtwl: failed to place received vfd: %d\n", ret);
		return true; /* return the inbuf to vq */
	}

	vfd->id = id;
	vfd->size = new->size;
	vfd->pfn = new->pfn;
	vfd->flags = new->flags;

	return true; /* return the inbuf to vq */
}

static bool vq_handle_recv(struct virtwl_info *vi,
			   struct virtio_wl_ctrl_vfd_recv *recv,
			   unsigned int len)
{
	struct virtwl_vfd *vfd;
	struct virtwl_vfd_qentry *qentry;

	mutex_lock(&vi->vfds_lock);
	vfd = idr_find(&vi->vfds, recv->vfd_id);
	if (vfd)
		mutex_lock(&vfd->lock);
	mutex_unlock(&vi->vfds_lock);

	if (!vfd) {
		pr_warn("virtwl: recv for unknown vfd_id %u\n", recv->vfd_id);
		return true; /* return the inbuf to vq */
	}

	qentry = kzalloc(sizeof(*qentry), GFP_KERNEL);
	if (!qentry) {
		mutex_unlock(&vfd->lock);
		pr_warn("virtwl: failed to allocate qentry for vfd\n");
		return true; /* return the inbuf to vq */
	}

	qentry->hdr = &recv->hdr;
	qentry->len = len;

	list_add_tail(&qentry->list, &vfd->in_queue);
	wake_up_interruptible_all(&vfd->in_waitq);
	mutex_unlock(&vfd->lock);

	return false; /* no return the inbuf to vq */
}

static bool vq_handle_hup(struct virtwl_info *vi,
			   struct virtio_wl_ctrl_vfd *vfd_hup,
			   unsigned int len)
{
	struct virtwl_vfd *vfd;

	mutex_lock(&vi->vfds_lock);
	vfd = idr_find(&vi->vfds, vfd_hup->vfd_id);
	if (vfd)
		mutex_lock(&vfd->lock);
	mutex_unlock(&vi->vfds_lock);

	if (!vfd) {
		pr_warn("virtwl: hup for unknown vfd_id %u\n", vfd_hup->vfd_id);
		return true; /* return the inbuf to vq */
	}

	if (vfd->hungup)
		pr_warn("virtwl: hup for hungup vfd_id %u\n", vfd_hup->vfd_id);

	vfd->hungup = true;
	wake_up_interruptible_all(&vfd->in_waitq);
	mutex_unlock(&vfd->lock);

	return true;
}

static bool vq_dispatch_hdr(struct virtwl_info *vi, unsigned int len,
			    struct virtio_wl_ctrl_hdr *hdr)
{
	struct virtqueue *vq = vi->vqs[VIRTWL_VQ_IN];
	struct mutex *vq_lock = &vi->vq_locks[VIRTWL_VQ_IN];
	bool return_vq = true;
	int ret;

	switch (hdr->type) {
	case VIRTIO_WL_CMD_VFD_NEW:
		return_vq = vq_handle_new(vi,
					  (struct virtio_wl_ctrl_vfd_new *)hdr,
					  len);
		break;
	case VIRTIO_WL_CMD_VFD_RECV:
		return_vq = vq_handle_recv(vi,
			(struct virtio_wl_ctrl_vfd_recv *)hdr, len);
		break;
	case VIRTIO_WL_CMD_VFD_HUP:
		return_vq = vq_handle_hup(vi, (struct virtio_wl_ctrl_vfd *)hdr,
					  len);
		break;
	default:
		pr_warn("virtwl: unhandled ctrl command: %u\n", hdr->type);
		break;
	}

	if (!return_vq)
		return false; /* no kick the vq */

	mutex_lock(vq_lock);
	ret = vq_return_inbuf_locked(vq, hdr);
	mutex_unlock(vq_lock);
	if (ret) {
		pr_warn("virtwl: failed to return inbuf to host: %d\n", ret);
		kfree(hdr);
	}

	return true; /* kick the vq */
}

static void vq_in_work_handler(struct work_struct *work)
{
	struct virtwl_info *vi = container_of(work, struct virtwl_info,
					      in_vq_work);
	struct virtqueue *vq = vi->vqs[VIRTWL_VQ_IN];
	struct mutex *vq_lock = &vi->vq_locks[VIRTWL_VQ_IN];
	void *buffer;
	unsigned int len;
	bool kick_vq = false;

	mutex_lock(vq_lock);
	while ((buffer = virtqueue_get_buf(vq, &len)) != NULL) {
		struct virtio_wl_ctrl_hdr *hdr = buffer;

		mutex_unlock(vq_lock);
		kick_vq |= vq_dispatch_hdr(vi, len, hdr);
		mutex_lock(vq_lock);
	}
	mutex_unlock(vq_lock);

	if (kick_vq)
		virtqueue_kick(vq);
}

static void vq_out_work_handler(struct work_struct *work)
{
	struct virtwl_info *vi = container_of(work, struct virtwl_info,
					      out_vq_work);
	struct virtqueue *vq = vi->vqs[VIRTWL_VQ_OUT];
	struct mutex *vq_lock = &vi->vq_locks[VIRTWL_VQ_OUT];
	unsigned int len;
	struct completion *finish_completion;
	bool wake_waitq = false;

	mutex_lock(vq_lock);
	while ((finish_completion = virtqueue_get_buf(vq, &len)) != NULL) {
		wake_waitq = true;
		complete(finish_completion);
	}
	mutex_unlock(vq_lock);

	if (wake_waitq)
		wake_up_interruptible_all(&vi->out_waitq);
}

static void vq_in_cb(struct virtqueue *vq)
{
	struct virtwl_info *vi = vq->vdev->priv;

	schedule_work(&vi->in_vq_work);
}

static void vq_out_cb(struct virtqueue *vq)
{
	struct virtwl_info *vi = vq->vdev->priv;

	schedule_work(&vi->out_vq_work);
}

static struct virtwl_vfd *virtwl_vfd_alloc(struct virtwl_info *vi)
{
	struct virtwl_vfd *vfd = kzalloc(sizeof(struct virtwl_vfd), GFP_KERNEL);

	if (!vfd)
		return ERR_PTR(-ENOMEM);

	vfd->vi = vi;

	mutex_init(&vfd->lock);
	INIT_LIST_HEAD(&vfd->in_queue);
	init_waitqueue_head(&vfd->in_waitq);

	return vfd;
}

static int virtwl_vfd_file_flags(struct virtwl_vfd *vfd)
{
	int flags = 0;
	int rw_mask = VIRTIO_WL_VFD_WRITE | VIRTIO_WL_VFD_READ;

	if ((vfd->flags & rw_mask) == rw_mask)
		flags |= O_RDWR;
	else if (vfd->flags & VIRTIO_WL_VFD_WRITE)
		flags |= O_WRONLY;
	else if (vfd->flags & VIRTIO_WL_VFD_READ)
		flags |= O_RDONLY;
	if (vfd->pfn)
		flags |= O_RDWR;
	return flags;
}

/* Locks the vfd and unlinks its id from vi */
static void virtwl_vfd_lock_unlink(struct virtwl_vfd *vfd)
{
	struct virtwl_info *vi = vfd->vi;

	/* this order is important to avoid deadlock */
	mutex_lock(&vi->vfds_lock);
	mutex_lock(&vfd->lock);
	idr_remove(&vi->vfds, vfd->id);
	mutex_unlock(&vfd->lock);
	mutex_unlock(&vi->vfds_lock);
}

/*
 * Only used to free a vfd that is not referenced any place else and contains
 * no queed virtio buffers. This must not be called while vfd is included in a
 * vi->vfd.
 */
static void virtwl_vfd_free(struct virtwl_vfd *vfd)
{
	kfree(vfd);
}

/*
 * Thread safe and also removes vfd from vi as well as any queued virtio buffers
 */
static void virtwl_vfd_remove(struct virtwl_vfd *vfd)
{
	struct virtwl_info *vi = vfd->vi;
	struct virtqueue *vq = vi->vqs[VIRTWL_VQ_IN];
	struct mutex *vq_lock = &vi->vq_locks[VIRTWL_VQ_IN];
	struct virtwl_vfd_qentry *qentry, *next;

	virtwl_vfd_lock_unlink(vfd);

	mutex_lock(vq_lock);
	list_for_each_entry_safe(qentry, next, &vfd->in_queue, list) {
		vq_return_inbuf_locked(vq, qentry->hdr);
		list_del(&qentry->list);
		kfree(qentry);
	}
	mutex_unlock(vq_lock);
	virtqueue_kick(vq);

	virtwl_vfd_free(vfd);
}

static void vfd_qentry_free_if_empty(struct virtwl_vfd *vfd,
				     struct virtwl_vfd_qentry *qentry)
{
	struct virtwl_info *vi = vfd->vi;
	struct virtqueue *vq = vi->vqs[VIRTWL_VQ_IN];
	struct mutex *vq_lock = &vi->vq_locks[VIRTWL_VQ_IN];

	if (qentry->hdr->type == VIRTIO_WL_CMD_VFD_RECV) {
		struct virtio_wl_ctrl_vfd_recv *recv =
			(struct virtio_wl_ctrl_vfd_recv *)qentry->hdr;
		ssize_t data_len =
			(ssize_t)qentry->len - (ssize_t)sizeof(*recv) -
			(ssize_t)recv->vfd_count * (ssize_t)sizeof(__le32);

		if (qentry->vfd_offset < recv->vfd_count)
			return;

		if ((s64)qentry->data_offset < data_len)
			return;
	}

	mutex_lock(vq_lock);
	vq_return_inbuf_locked(vq, qentry->hdr);
	mutex_unlock(vq_lock);
	list_del(&qentry->list);
	kfree(qentry);
	virtqueue_kick(vq);
}

static ssize_t vfd_out_locked(struct virtwl_vfd *vfd, char __user *buffer,
			      size_t len)
{
	struct virtwl_vfd_qentry *qentry, *next;
	ssize_t read_count = 0;

	list_for_each_entry_safe(qentry, next, &vfd->in_queue, list) {
		struct virtio_wl_ctrl_vfd_recv *recv =
			(struct virtio_wl_ctrl_vfd_recv *)qentry->hdr;
		size_t recv_offset = sizeof(*recv) + recv->vfd_count *
				     sizeof(__le32) + qentry->data_offset;
		u8 *buf = (u8 *)recv + recv_offset;
		ssize_t to_read = (ssize_t)qentry->len - (ssize_t)recv_offset;

		if (qentry->hdr->type != VIRTIO_WL_CMD_VFD_RECV)
			continue;

		if ((to_read + read_count) > len)
			to_read = len - read_count;

		if (copy_to_user(buffer + read_count, buf, to_read)) {
			read_count = -EFAULT;
			break;
		}

		read_count += to_read;

		qentry->data_offset += to_read;
		vfd_qentry_free_if_empty(vfd, qentry);

		if (read_count >= len)
			break;
	}

	return read_count;
}

/* must hold both vfd->lock and vi->vfds_lock */
static size_t vfd_out_vfds_locked(struct virtwl_vfd *vfd,
				  struct virtwl_vfd **vfds, size_t count)
{
	struct virtwl_info *vi = vfd->vi;
	struct virtwl_vfd_qentry *qentry, *next;
	size_t i;
	size_t read_count = 0;

	list_for_each_entry_safe(qentry, next, &vfd->in_queue, list) {
		struct virtio_wl_ctrl_vfd_recv *recv =
			(struct virtio_wl_ctrl_vfd_recv *)qentry->hdr;
		size_t vfd_offset = sizeof(*recv) + qentry->vfd_offset *
				    sizeof(__le32);
		__le32 *vfds_le = (__le32 *)((void *)recv + vfd_offset);
		ssize_t vfds_to_read = recv->vfd_count - qentry->vfd_offset;

		if (read_count >= count)
			break;
		if (vfds_to_read <= 0)
			continue;
		if (qentry->hdr->type != VIRTIO_WL_CMD_VFD_RECV)
			continue;

		if ((vfds_to_read + read_count) > count)
			vfds_to_read = count - read_count;

		for (i = 0; i < vfds_to_read; i++) {
			uint32_t vfd_id = le32_to_cpu(vfds_le[i]);
			vfds[read_count] = idr_find(&vi->vfds, vfd_id);
			if (vfds[read_count]) {
				read_count++;
			} else {
				pr_warn("virtwl: received a vfd with unrecognized id: %u\n",
					vfd_id);
			}
			qentry->vfd_offset++;
		}

		vfd_qentry_free_if_empty(vfd, qentry);
	}

	return read_count;
}

/* this can only be called if the caller has unique ownership of the vfd */
static int do_vfd_close(struct virtwl_vfd *vfd)
{
	struct virtio_wl_ctrl_vfd *ctrl_close;
	struct virtwl_info *vi = vfd->vi;
	struct completion finish_completion;
	struct scatterlist out_sg;
	struct scatterlist in_sg;
	int ret = 0;

	ctrl_close = kzalloc(sizeof(*ctrl_close), GFP_KERNEL);
	if (!ctrl_close)
		return -ENOMEM;

	ctrl_close->hdr.type = VIRTIO_WL_CMD_VFD_CLOSE;
	ctrl_close->vfd_id = vfd->id;

	sg_init_one(&out_sg, &ctrl_close->hdr,
		    sizeof(struct virtio_wl_ctrl_vfd));
	sg_init_one(&in_sg, &ctrl_close->hdr,
		    sizeof(struct virtio_wl_ctrl_hdr));

	init_completion(&finish_completion);
	ret = vq_queue_out(vi, &out_sg, &in_sg, &finish_completion,
			   false /* block */);
	if (ret) {
		pr_warn("virtwl: failed to queue close vfd id %u: %d\n",
			vfd->id,
			ret);
		goto free_ctrl_close;
	}

	wait_for_completion(&finish_completion);
	virtwl_vfd_remove(vfd);

free_ctrl_close:
	kfree(ctrl_close);
	return ret;
}

static ssize_t virtwl_vfd_recv(struct file *filp, char __user *buffer,
			       size_t len, struct virtwl_vfd **vfds,
			       size_t *vfd_count)
{
	struct virtwl_vfd *vfd = filp->private_data;
	struct virtwl_info *vi = vfd->vi;
	ssize_t read_count = 0;
	size_t vfd_read_count = 0;
	bool force_to_wait = false;

	mutex_lock(&vi->vfds_lock);
	mutex_lock(&vfd->lock);

	while (read_count == 0 && vfd_read_count == 0) {
		while (force_to_wait || list_empty(&vfd->in_queue)) {
			force_to_wait = false;
			if (vfd->hungup)
				goto out_unlock;

			mutex_unlock(&vfd->lock);
			mutex_unlock(&vi->vfds_lock);
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;

			if (wait_event_interruptible(vfd->in_waitq,
				!list_empty(&vfd->in_queue) || vfd->hungup))
				return -ERESTARTSYS;

			mutex_lock(&vi->vfds_lock);
			mutex_lock(&vfd->lock);
		}

		read_count = vfd_out_locked(vfd, buffer, len);
		if (read_count < 0)
			goto out_unlock;
		if (vfds && vfd_count && *vfd_count)
			vfd_read_count = vfd_out_vfds_locked(vfd, vfds,
							     *vfd_count);
		else if (read_count == 0 && !list_empty(&vfd->in_queue))
			/*
			 * Indicates a corner case where the in_queue has ONLY
			 * incoming VFDs but the caller has given us no space to
			 * store them. We force a wait for more activity on the
			 * in_queue to prevent busy waiting.
			 */
			force_to_wait = true;
	}

out_unlock:
	mutex_unlock(&vfd->lock);
	mutex_unlock(&vi->vfds_lock);
	if (vfd_count)
		*vfd_count = vfd_read_count;
	return read_count;
}

static int encode_vfd_ids(struct virtwl_vfd **vfds, size_t vfd_count,
			  __le32 *vfd_ids)
{
	size_t i;

	for (i = 0; i < vfd_count; i++) {
		if (vfds[i])
			vfd_ids[i] = cpu_to_le32(vfds[i]->id);
		else
			return -EBADFD;
	}
	return 0;
}

#ifdef SEND_VIRTGPU_RESOURCES
static int encode_vfd_ids_foreign(struct virtwl_vfd **vfds,
				  struct dma_buf **virtgpu_dma_bufs,
				  size_t vfd_count,
				  struct virtio_wl_ctrl_vfd_send_vfd *vfd_ids)
{
	size_t i;
	int ret;

	for (i = 0; i < vfd_count; i++) {
		if (vfds[i]) {
			vfd_ids[i].kind = VIRTIO_WL_CTRL_VFD_SEND_KIND_LOCAL;
			vfd_ids[i].id = cpu_to_le32(vfds[i]->id);
		} else if (virtgpu_dma_bufs[i]) {
			ret = virtio_gpu_dma_buf_to_handle(virtgpu_dma_bufs[i],
							   false,
							   &vfd_ids[i].id);
			if (ret)
				return ret;
			vfd_ids[i].kind = VIRTIO_WL_CTRL_VFD_SEND_KIND_VIRTGPU;
		} else {
			return -EBADFD;
		}
	}
	return 0;
}
#endif

static int virtwl_vfd_send(struct file *filp, const char __user *buffer,
					       u32 len, int *vfd_fds)
{
	struct virtwl_vfd *vfd = filp->private_data;
	struct virtwl_info *vi = vfd->vi;
	struct fd vfd_files[VIRTWL_SEND_MAX_ALLOCS] = { { 0 } };
	struct virtwl_vfd *vfds[VIRTWL_SEND_MAX_ALLOCS] = { 0 };
#ifdef SEND_VIRTGPU_RESOURCES
	struct dma_buf *virtgpu_dma_bufs[VIRTWL_SEND_MAX_ALLOCS] = { 0 };
	bool foreign_id = false;
#endif
	size_t vfd_count = 0;
	size_t vfd_ids_size;
	size_t ctrl_send_size;
	struct virtio_wl_ctrl_vfd_send *ctrl_send;
	u8 *vfd_ids;
	u8 *out_buffer;
	struct completion finish_completion;
	struct scatterlist out_sg;
	struct scatterlist in_sg;
	int ret;
	int i;

	if (vfd_fds) {
		for (i = 0; i < VIRTWL_SEND_MAX_ALLOCS; i++) {
			struct fd vfd_file;
			int fd = vfd_fds[i];

			if (fd < 0)
				break;

			vfd_file = fdget(vfd_fds[i]);
			if (!vfd_file.file) {
				ret = -EBADFD;
				goto put_files;
			}

			if (vfd_file.file->f_op == &virtwl_vfd_fops) {
				vfd_files[i] = vfd_file;

				vfds[i] = vfd_file.file->private_data;
				if (vfds[i] && vfds[i]->id) {
					vfd_count++;
					continue;
				}

				ret = -EINVAL;
				goto put_files;
			} else {
				struct dma_buf *dma_buf = ERR_PTR(-EINVAL);
#ifdef SEND_VIRTGPU_RESOURCES
				dma_buf = dma_buf_get(vfd_fds[i]);
				if (!IS_ERR(dma_buf)) {
					fdput(vfd_file);
					virtgpu_dma_bufs[i] = dma_buf;
					foreign_id = true;
					vfd_count++;
					continue;
				}
#endif
				fdput(vfd_file);
				ret = PTR_ERR(dma_buf);
				goto put_files;
			}
		}
	}

	/* Empty writes always succeed. */
	if (len == 0 && vfd_count == 0)
		return 0;

	vfd_ids_size = vfd_count * sizeof(__le32);
#ifdef SEND_VIRTGPU_RESOURCES
	if (foreign_id) {
		vfd_ids_size = vfd_count *
			       sizeof(struct virtio_wl_ctrl_vfd_send_vfd);
	}
#endif
	ctrl_send_size = sizeof(*ctrl_send) + vfd_ids_size + len;
	ctrl_send = kzalloc(ctrl_send_size, GFP_KERNEL);
	if (!ctrl_send) {
		ret = -ENOMEM;
		goto put_files;
	}

	vfd_ids = (u8 *)ctrl_send + sizeof(*ctrl_send);
	out_buffer = (u8 *)ctrl_send + ctrl_send_size - len;

	ctrl_send->hdr.type = VIRTIO_WL_CMD_VFD_SEND;
#ifdef SEND_VIRTGPU_RESOURCES
	if (foreign_id) {
		ctrl_send->hdr.type = VIRTIO_WL_CMD_VFD_SEND_FOREIGN_ID;
		ret = encode_vfd_ids_foreign(vfds, virtgpu_dma_bufs, vfd_count,
			(struct virtio_wl_ctrl_vfd_send_vfd *)vfd_ids);
	} else {
		ret = encode_vfd_ids(vfds, vfd_count, (__le32 *)vfd_ids);
	}
#else
	ret = encode_vfd_ids(vfds, vfd_count, (__le32 *)vfd_ids);
#endif
	if (ret)
		goto free_ctrl_send;
	ctrl_send->vfd_id = vfd->id;
	ctrl_send->vfd_count = vfd_count;

	if (copy_from_user(out_buffer, buffer, len)) {
		ret = -EFAULT;
		goto free_ctrl_send;
	}

	init_completion(&finish_completion);
	sg_init_one(&out_sg, ctrl_send, ctrl_send_size);
	sg_init_one(&in_sg, ctrl_send, sizeof(struct virtio_wl_ctrl_hdr));

	ret = vq_queue_out(vi, &out_sg, &in_sg, &finish_completion,
				       filp->f_flags & O_NONBLOCK);
	if (ret)
		goto free_ctrl_send;

	wait_for_completion(&finish_completion);

	ret = virtwl_resp_err(ctrl_send->hdr.type);

free_ctrl_send:
	kfree(ctrl_send);
put_files:
	for (i = 0; i < VIRTWL_SEND_MAX_ALLOCS; i++) {
		if (vfd_files[i].file)
			fdput(vfd_files[i]);
#ifdef SEND_VIRTGPU_RESOURCES
		if (virtgpu_dma_bufs[i])
			dma_buf_put(virtgpu_dma_bufs[i]);
#endif
	}
	return ret;
}

static int virtwl_vfd_dmabuf_sync(struct file *filp, u32 flags)
{
	struct virtio_wl_ctrl_vfd_dmabuf_sync *ctrl_dmabuf_sync;
	struct virtwl_vfd *vfd = filp->private_data;
	struct virtwl_info *vi = vfd->vi;
	struct completion finish_completion;
	struct scatterlist out_sg;
	struct scatterlist in_sg;
	int ret = 0;

	ctrl_dmabuf_sync = kzalloc(sizeof(*ctrl_dmabuf_sync), GFP_KERNEL);
	if (!ctrl_dmabuf_sync)
		return -ENOMEM;

	ctrl_dmabuf_sync->hdr.type = VIRTIO_WL_CMD_VFD_DMABUF_SYNC;
	ctrl_dmabuf_sync->vfd_id = vfd->id;
	ctrl_dmabuf_sync->flags = flags;

	sg_init_one(&out_sg, &ctrl_dmabuf_sync->hdr,
		    sizeof(struct virtio_wl_ctrl_vfd_dmabuf_sync));
	sg_init_one(&in_sg, &ctrl_dmabuf_sync->hdr,
		    sizeof(struct virtio_wl_ctrl_hdr));

	init_completion(&finish_completion);
	ret = vq_queue_out(vi, &out_sg, &in_sg, &finish_completion,
			   false /* block */);
	if (ret) {
		pr_warn("virtwl: failed to queue dmabuf sync vfd id %u: %d\n",
			vfd->id,
			ret);
		goto free_ctrl_dmabuf_sync;
	}

	wait_for_completion(&finish_completion);

free_ctrl_dmabuf_sync:
	kfree(ctrl_dmabuf_sync);
	return ret;
}

static ssize_t virtwl_vfd_read(struct file *filp, char __user *buffer,
			       size_t size, loff_t *pos)
{
	return virtwl_vfd_recv(filp, buffer, size, NULL, NULL);
}

static ssize_t virtwl_vfd_write(struct file *filp, const char __user *buffer,
				size_t size, loff_t *pos)
{
	int ret = 0;

	if (size > U32_MAX)
		size = U32_MAX;

	ret = virtwl_vfd_send(filp, buffer, size, NULL);
	if (ret)
		return ret;

	return size;
}

static int virtwl_vfd_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct virtwl_vfd *vfd = filp->private_data;
	unsigned long vm_size = vma->vm_end - vma->vm_start;
	int ret = 0;

	mutex_lock(&vfd->lock);

	if (!vfd->pfn) {
		ret = -EACCES;
		goto out_unlock;
	}

	if (vm_size + (vma->vm_pgoff << PAGE_SHIFT) > PAGE_ALIGN(vfd->size)) {
		ret = -EINVAL;
		goto out_unlock;
	}

	ret = io_remap_pfn_range(vma, vma->vm_start, vfd->pfn, vm_size,
				 vma->vm_page_prot);
	if (ret)
		goto out_unlock;

	vma->vm_flags |= VM_PFNMAP | VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

out_unlock:
	mutex_unlock(&vfd->lock);
	return ret;
}

static unsigned int virtwl_vfd_poll(struct file *filp,
				    struct poll_table_struct *wait)
{
	struct virtwl_vfd *vfd = filp->private_data;
	struct virtwl_info *vi = vfd->vi;
	unsigned int mask = 0;

	mutex_lock(&vi->vq_locks[VIRTWL_VQ_OUT]);
	poll_wait(filp, &vi->out_waitq, wait);
	if (vi->vqs[VIRTWL_VQ_OUT]->num_free)
		mask |= POLLOUT | POLLWRNORM;
	mutex_unlock(&vi->vq_locks[VIRTWL_VQ_OUT]);

	mutex_lock(&vfd->lock);
	poll_wait(filp, &vfd->in_waitq, wait);
	if (!list_empty(&vfd->in_queue))
		mask |= POLLIN | POLLRDNORM;
	if (vfd->hungup)
		mask |= POLLHUP;
	mutex_unlock(&vfd->lock);

	return mask;
}

static int virtwl_vfd_release(struct inode *inodep, struct file *filp)
{
	struct virtwl_vfd *vfd = filp->private_data;
	uint32_t vfd_id = vfd->id;
	int ret;

	/*
	 * If release is called, filp must be out of references and we have the
	 * last reference.
	 */
	ret = do_vfd_close(vfd);
	if (ret)
		pr_warn("virtwl: failed to release vfd id %u: %d\n", vfd_id,
			ret);
	return 0;
}

static int virtwl_open(struct inode *inodep, struct file *filp)
{
	struct virtwl_info *vi = container_of(inodep->i_cdev,
					      struct virtwl_info, cdev);

	filp->private_data = vi;

	return 0;
}

static struct virtwl_vfd *do_new(struct virtwl_info *vi,
				 struct virtwl_ioctl_new *ioctl_new,
				 size_t ioctl_new_size, bool nonblock)
{
	struct virtio_wl_ctrl_vfd_new *ctrl_new;
	struct virtwl_vfd *vfd;
	struct completion finish_completion;
	struct scatterlist out_sg;
	struct scatterlist in_sg;
	int ret = 0;

	if (ioctl_new->type != VIRTWL_IOCTL_NEW_CTX &&
		ioctl_new->type != VIRTWL_IOCTL_NEW_ALLOC &&
		ioctl_new->type != VIRTWL_IOCTL_NEW_PIPE_READ &&
		ioctl_new->type != VIRTWL_IOCTL_NEW_PIPE_WRITE &&
		ioctl_new->type != VIRTWL_IOCTL_NEW_DMABUF)
		return ERR_PTR(-EINVAL);

	ctrl_new = kzalloc(sizeof(*ctrl_new), GFP_KERNEL);
	if (!ctrl_new)
		return ERR_PTR(-ENOMEM);

	vfd = virtwl_vfd_alloc(vi);
	if (!vfd) {
		ret = -ENOMEM;
		goto free_ctrl_new;
	}

	mutex_lock(&vi->vfds_lock);
	/*
	 * Take the lock before adding it to the vfds list where others might
	 * reference it.
	 */
	mutex_lock(&vfd->lock);
	ret = idr_alloc(&vi->vfds, vfd, 1, VIRTWL_MAX_ALLOC, GFP_KERNEL);
	mutex_unlock(&vi->vfds_lock);
	if (ret <= 0)
		goto remove_vfd;

	vfd->id = ret;
	ret = 0;

	ctrl_new->vfd_id = vfd->id;
	switch (ioctl_new->type) {
	case VIRTWL_IOCTL_NEW_CTX:
		ctrl_new->hdr.type = VIRTIO_WL_CMD_VFD_NEW_CTX;
		ctrl_new->flags = VIRTIO_WL_VFD_WRITE | VIRTIO_WL_VFD_READ;
		break;
	case VIRTWL_IOCTL_NEW_ALLOC:
		ctrl_new->hdr.type = VIRTIO_WL_CMD_VFD_NEW;
		ctrl_new->size = PAGE_ALIGN(ioctl_new->size);
		break;
	case VIRTWL_IOCTL_NEW_PIPE_READ:
		ctrl_new->hdr.type = VIRTIO_WL_CMD_VFD_NEW_PIPE;
		ctrl_new->flags = VIRTIO_WL_VFD_READ;
		break;
	case VIRTWL_IOCTL_NEW_PIPE_WRITE:
		ctrl_new->hdr.type = VIRTIO_WL_CMD_VFD_NEW_PIPE;
		ctrl_new->flags = VIRTIO_WL_VFD_WRITE;
		break;
	case VIRTWL_IOCTL_NEW_DMABUF:
		/* Make sure ioctl_new contains enough data for NEW_DMABUF. */
		if (ioctl_new_size == sizeof(*ioctl_new)) {
			ctrl_new->hdr.type = VIRTIO_WL_CMD_VFD_NEW_DMABUF;
			/* FIXME: convert from host byte order. */
			memcpy(&ctrl_new->dmabuf, &ioctl_new->dmabuf,
			       sizeof(ioctl_new->dmabuf));
			break;
		}
		/* fall-through */
	default:
		ret = -EINVAL;
		goto remove_vfd;
	}

	init_completion(&finish_completion);
	sg_init_one(&out_sg, ctrl_new, sizeof(*ctrl_new));
	sg_init_one(&in_sg, ctrl_new, sizeof(*ctrl_new));

	ret = vq_queue_out(vi, &out_sg, &in_sg, &finish_completion, nonblock);
	if (ret)
		goto remove_vfd;

	wait_for_completion(&finish_completion);

	ret = virtwl_resp_err(ctrl_new->hdr.type);
	if (ret)
		goto remove_vfd;

	vfd->size = ctrl_new->size;
	vfd->pfn = ctrl_new->pfn;
	vfd->flags = ctrl_new->flags;

	mutex_unlock(&vfd->lock);

	if (ioctl_new->type == VIRTWL_IOCTL_NEW_DMABUF) {
		/* FIXME: convert to host byte order. */
		memcpy(&ioctl_new->dmabuf, &ctrl_new->dmabuf,
		       sizeof(ctrl_new->dmabuf));
	}

	kfree(ctrl_new);
	return vfd;

remove_vfd:
	/*
	 * unlock the vfd to avoid deadlock when unlinking it
	 * or freeing a held lock
	 */
	mutex_unlock(&vfd->lock);
	/* this is safe since the id cannot change after the vfd is created */
	if (vfd->id)
		virtwl_vfd_lock_unlink(vfd);
	virtwl_vfd_free(vfd);
free_ctrl_new:
	kfree(ctrl_new);
	return ERR_PTR(ret);
}

static long virtwl_ioctl_send(struct file *filp, void __user *ptr)
{
	struct virtwl_ioctl_txn ioctl_send;
	void __user *user_data = ptr + sizeof(struct virtwl_ioctl_txn);
	int ret;

	ret = copy_from_user(&ioctl_send, ptr, sizeof(struct virtwl_ioctl_txn));
	if (ret)
		return -EFAULT;

	/* Early check for user error; do_send still uses copy_from_user. */
	ret = !access_ok(user_data, ioctl_send.len);
	if (ret)
		return -EFAULT;

	return virtwl_vfd_send(filp, user_data, ioctl_send.len, ioctl_send.fds);
}

static long virtwl_ioctl_recv(struct file *filp, void __user *ptr)
{
	struct virtwl_ioctl_txn ioctl_recv;
	void __user *user_data = ptr + sizeof(struct virtwl_ioctl_txn);
	int __user *user_fds = (int __user *)ptr;
	size_t vfd_count = VIRTWL_SEND_MAX_ALLOCS;
	struct virtwl_vfd *vfds[VIRTWL_SEND_MAX_ALLOCS] = { 0 };
	int fds[VIRTWL_SEND_MAX_ALLOCS];
	size_t i;
	int ret = 0;

	for (i = 0; i < VIRTWL_SEND_MAX_ALLOCS; i++)
		fds[i] = -1;

	ret = copy_from_user(&ioctl_recv, ptr, sizeof(struct virtwl_ioctl_txn));
	if (ret)
		return -EFAULT;

	/* Early check for user error. */
	ret = !access_ok(user_data, ioctl_recv.len);
	if (ret)
		return -EFAULT;

	ret = virtwl_vfd_recv(filp, user_data, ioctl_recv.len, vfds,
			      &vfd_count);
	if (ret < 0)
		return ret;

	ret = copy_to_user(&((struct virtwl_ioctl_txn __user *)ptr)->len, &ret,
			   sizeof(ioctl_recv.len));
	if (ret) {
		ret = -EFAULT;
		goto free_vfds;
	}

	for (i = 0; i < vfd_count; i++) {
		ret = anon_inode_getfd("[virtwl_vfd]", &virtwl_vfd_fops,
				       vfds[i], virtwl_vfd_file_flags(vfds[i])
				       | O_CLOEXEC);
		if (ret < 0)
			goto free_vfds;

		vfds[i] = NULL;
		fds[i] = ret;
	}

	ret = copy_to_user(user_fds, fds, sizeof(int) * VIRTWL_SEND_MAX_ALLOCS);
	if (ret) {
		ret = -EFAULT;
		goto free_vfds;
	}

	return 0;

free_vfds:
	for (i = 0; i < vfd_count; i++) {
		if (vfds[i])
			do_vfd_close(vfds[i]);
		if (fds[i] >= 0)
			__close_fd(current->files, fds[i]);
	}
	return ret;
}

static long virtwl_ioctl_dmabuf_sync(struct file *filp, void __user *ptr)
{
	struct virtwl_ioctl_dmabuf_sync ioctl_dmabuf_sync;
	int ret;

	ret = copy_from_user(&ioctl_dmabuf_sync, ptr,
			     sizeof(struct virtwl_ioctl_dmabuf_sync));
	if (ret)
		return -EFAULT;

	if (ioctl_dmabuf_sync.flags & ~DMA_BUF_SYNC_VALID_FLAGS_MASK)
		return -EINVAL;

	return virtwl_vfd_dmabuf_sync(filp, ioctl_dmabuf_sync.flags);
}

static long virtwl_vfd_ioctl(struct file *filp, unsigned int cmd,
			     void __user *ptr)
{
	switch (cmd) {
	case VIRTWL_IOCTL_SEND:
		return virtwl_ioctl_send(filp, ptr);
	case VIRTWL_IOCTL_RECV:
		return virtwl_ioctl_recv(filp, ptr);
	case VIRTWL_IOCTL_DMABUF_SYNC:
		return virtwl_ioctl_dmabuf_sync(filp, ptr);
	default:
		return -ENOTTY;
	}
}

static long virtwl_ioctl_new(struct file *filp, void __user *ptr,
			     size_t in_size)
{
	struct virtwl_info *vi = filp->private_data;
	struct virtwl_vfd *vfd;
	struct virtwl_ioctl_new ioctl_new = {};
	size_t size = min(in_size, sizeof(ioctl_new));
	int ret;

	/* Early check for user error. */
	ret = !access_ok(ptr, size);
	if (ret)
		return -EFAULT;

	ret = copy_from_user(&ioctl_new, ptr, size);
	if (ret)
		return -EFAULT;

	vfd = do_new(vi, &ioctl_new, size, filp->f_flags & O_NONBLOCK);
	if (IS_ERR(vfd))
		return PTR_ERR(vfd);

	ret = anon_inode_getfd("[virtwl_vfd]", &virtwl_vfd_fops, vfd,
			       virtwl_vfd_file_flags(vfd) | O_CLOEXEC);
	if (ret < 0) {
		do_vfd_close(vfd);
		return ret;
	}

	ioctl_new.fd = ret;
	ret = copy_to_user(ptr, &ioctl_new, size);
	if (ret) {
		/* The release operation will handle freeing this alloc */
		ksys_close(ioctl_new.fd);
		return -EFAULT;
	}

	return 0;
}

static long virtwl_ioctl_ptr(struct file *filp, unsigned int cmd,
			     void __user *ptr)
{
	if (filp->f_op == &virtwl_vfd_fops)
		return virtwl_vfd_ioctl(filp, cmd, ptr);

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(VIRTWL_IOCTL_NEW):
		return virtwl_ioctl_new(filp, ptr, _IOC_SIZE(cmd));
	default:
		return -ENOTTY;
	}
}

static long virtwl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return virtwl_ioctl_ptr(filp, cmd, (void __user *)arg);
}

#ifdef CONFIG_COMPAT
static long virtwl_ioctl_compat(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	return virtwl_ioctl_ptr(filp, cmd, compat_ptr(arg));
}
#else
#define virtwl_ioctl_compat NULL
#endif

static int virtwl_release(struct inode *inodep, struct file *filp)
{
	return 0;
}

static const struct file_operations virtwl_fops = {
	.open = virtwl_open,
	.unlocked_ioctl = virtwl_ioctl,
	.compat_ioctl = virtwl_ioctl_compat,
	.release = virtwl_release,
};

static const struct file_operations virtwl_vfd_fops = {
	.read = virtwl_vfd_read,
	.write = virtwl_vfd_write,
	.mmap = virtwl_vfd_mmap,
	.poll = virtwl_vfd_poll,
	.unlocked_ioctl = virtwl_ioctl,
	.compat_ioctl = virtwl_ioctl_compat,
	.release = virtwl_vfd_release,
};

static int probe_common(struct virtio_device *vdev)
{
	int i;
	int ret;
	struct virtwl_info *vi = NULL;
	vq_callback_t *vq_callbacks[] = { vq_in_cb, vq_out_cb };
	static const char * const vq_names[] = { "in", "out" };

	vi = kzalloc(sizeof(struct virtwl_info), GFP_KERNEL);
	if (!vi)
		return -ENOMEM;

	vdev->priv = vi;

	ret = alloc_chrdev_region(&vi->dev_num, 0, 1, "wl");
	if (ret) {
		ret = -ENOMEM;
		pr_warn("virtwl: failed to allocate wl chrdev region: %d\n",
			ret);
		goto free_vi;
	}

	vi->class = class_create(THIS_MODULE, "wl");
	if (IS_ERR(vi->class)) {
		ret = PTR_ERR(vi->class);
		pr_warn("virtwl: failed to create wl class: %d\n", ret);
		goto unregister_region;

	}

	vi->dev = device_create(vi->class, NULL, vi->dev_num, vi, "wl%d", 0);
	if (IS_ERR(vi->dev)) {
		ret = PTR_ERR(vi->dev);
		pr_warn("virtwl: failed to create wl0 device: %d\n", ret);
		goto destroy_class;
	}

	cdev_init(&vi->cdev, &virtwl_fops);
	ret = cdev_add(&vi->cdev, vi->dev_num, 1);
	if (ret) {
		pr_warn("virtwl: failed to add virtio wayland character device to system: %d\n",
			ret);
		goto destroy_device;
	}

	for (i = 0; i < VIRTWL_QUEUE_COUNT; i++)
		mutex_init(&vi->vq_locks[i]);

	ret = virtio_find_vqs(vdev, VIRTWL_QUEUE_COUNT, vi->vqs, vq_callbacks,
			      vq_names, NULL);
	if (ret) {
		pr_warn("virtwl: failed to find virtio wayland queues: %d\n",
			ret);
		goto del_cdev;
	}

	INIT_WORK(&vi->in_vq_work, vq_in_work_handler);
	INIT_WORK(&vi->out_vq_work, vq_out_work_handler);
	init_waitqueue_head(&vi->out_waitq);

	mutex_init(&vi->vfds_lock);
	idr_init(&vi->vfds);

	/* lock is unneeded as we have unique ownership */
	ret = vq_fill_locked(vi->vqs[VIRTWL_VQ_IN]);
	if (ret) {
		pr_warn("virtwl: failed to fill in virtqueue: %d", ret);
		goto del_cdev;
	}

	virtio_device_ready(vdev);
	virtqueue_kick(vi->vqs[VIRTWL_VQ_IN]);


	return 0;

del_cdev:
	cdev_del(&vi->cdev);
destroy_device:
	put_device(vi->dev);
destroy_class:
	class_destroy(vi->class);
unregister_region:
	unregister_chrdev_region(vi->dev_num, 0);
free_vi:
	kfree(vi);
	return ret;
}

static void remove_common(struct virtio_device *vdev)
{
	struct virtwl_info *vi = vdev->priv;

	cdev_del(&vi->cdev);
	put_device(vi->dev);
	class_destroy(vi->class);
	unregister_chrdev_region(vi->dev_num, 0);
	kfree(vi);
}

static int virtwl_probe(struct virtio_device *vdev)
{
	return probe_common(vdev);
}

static void virtwl_remove(struct virtio_device *vdev)
{
	remove_common(vdev);
}

static void virtwl_scan(struct virtio_device *vdev)
{
}


static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_WL, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features_legacy[] = {
	VIRTIO_WL_F_TRANS_FLAGS
};

static unsigned int features[] = {
	VIRTIO_WL_F_TRANS_FLAGS
};

static struct virtio_driver virtio_wl_driver = {
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
	.id_table =	id_table,
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.feature_table_legacy = features_legacy,
	.feature_table_size_legacy = ARRAY_SIZE(features_legacy),
	.probe =	virtwl_probe,
	.remove =	virtwl_remove,
	.scan =		virtwl_scan,
};

module_virtio_driver(virtio_wl_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio wayland driver");
MODULE_LICENSE("GPL");
