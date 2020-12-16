// SPDX-License-Identifier: GPL-2.0+
/* Driver for virtio video device.
 *
 * Copyright 2019 OpenSynergy GmbH.
 *
 * Based on drivers/gpu/drm/virtio/virtgpu_vq.c
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "virtio_video.h"

#define MAX_INLINE_CMD_SIZE   298
#define MAX_INLINE_RESP_SIZE  298
#define VBUFFER_SIZE          (sizeof(struct virtio_video_vbuffer) \
			       + MAX_INLINE_CMD_SIZE		   \
			       + MAX_INLINE_RESP_SIZE)

void virtio_video_resource_id_get(struct virtio_video *vv, uint32_t *id)
{
	int handle;

	idr_preload(GFP_KERNEL);
	spin_lock(&vv->resource_idr_lock);
	handle = idr_alloc(&vv->resource_idr, NULL, 1, 0, GFP_NOWAIT);
	spin_unlock(&vv->resource_idr_lock);
	idr_preload_end();
	*id = handle;
}

void virtio_video_resource_id_put(struct virtio_video *vv, uint32_t id)
{
	spin_lock(&vv->resource_idr_lock);
	idr_remove(&vv->resource_idr, id);
	spin_unlock(&vv->resource_idr_lock);
}

void virtio_video_stream_id_get(struct virtio_video *vv,
				struct virtio_video_stream *stream,
				uint32_t *id)
{
	int handle;

	idr_preload(GFP_KERNEL);
	spin_lock(&vv->stream_idr_lock);
	handle = idr_alloc(&vv->stream_idr, stream, 1, 0, 0);
	spin_unlock(&vv->stream_idr_lock);
	idr_preload_end();
	*id = handle;
}

void virtio_video_stream_id_put(struct virtio_video *vv, uint32_t id)
{
	spin_lock(&vv->stream_idr_lock);
	idr_remove(&vv->stream_idr, id);
	spin_unlock(&vv->stream_idr_lock);
}

void virtio_video_cmd_ack(struct virtqueue *vq)
{
	struct virtio_video *vv = vq->vdev->priv;

	schedule_work(&vv->commandq.dequeue_work);
}

void virtio_video_event_ack(struct virtqueue *vq)
{
	struct virtio_video *vv = vq->vdev->priv;

	schedule_work(&vv->eventq.dequeue_work);
}

static struct virtio_video_vbuffer *
virtio_video_get_vbuf(struct virtio_video *vv, int size,
		      int resp_size, void *resp_buf,
		      virtio_video_resp_cb resp_cb)
{
	struct virtio_video_vbuffer *vbuf;

	vbuf = kmem_cache_alloc(vv->vbufs, GFP_KERNEL);
	if (!vbuf)
		return ERR_PTR(-ENOMEM);
	memset(vbuf, 0, VBUFFER_SIZE);

	BUG_ON(size > MAX_INLINE_CMD_SIZE);
	vbuf->buf = (void *)vbuf + sizeof(*vbuf);
	vbuf->size = size;

	vbuf->resp_cb = resp_cb;
	vbuf->resp_size = resp_size;
	if (resp_size <= MAX_INLINE_RESP_SIZE && !resp_buf)
		vbuf->resp_buf = (void *)vbuf->buf + size;
	else
		vbuf->resp_buf = resp_buf;
	BUG_ON(!vbuf->resp_buf);

	return vbuf;
}

static void free_vbuf(struct virtio_video *vv,
		      struct virtio_video_vbuffer *vbuf)
{
	if (!vbuf->resp_cb &&
	    vbuf->resp_size > MAX_INLINE_RESP_SIZE)
		kfree(vbuf->resp_buf);
	kfree(vbuf->data_buf);
	kmem_cache_free(vv->vbufs, vbuf);
}

static void reclaim_vbufs(struct virtqueue *vq, struct list_head *reclaim_list)
{
	struct virtio_video_vbuffer *vbuf;
	unsigned int len;
	struct virtio_video *vv = vq->vdev->priv;
	int freed = 0;

	while ((vbuf = virtqueue_get_buf(vq, &len))) {
		list_add_tail(&vbuf->list, reclaim_list);
		freed++;
	}
	if (freed == 0)
		v4l2_dbg(1, vv->debug, &vv->v4l2_dev,
			 "zero vbufs reclaimed\n");
}

static void detach_vbufs(struct virtqueue *vq, struct list_head *detach_list)
{
	struct virtio_video_vbuffer *vbuf;

	while ((vbuf = virtqueue_detach_unused_buf(vq)) != NULL)
		list_add_tail(&vbuf->list, detach_list);
}

static void virtio_video_deatch_vbufs(struct virtio_video *vv)
{
	struct list_head detach_list;
	struct virtio_video_vbuffer *entry, *tmp;

	INIT_LIST_HEAD(&detach_list);

	detach_vbufs(vv->eventq.vq, &detach_list);
	detach_vbufs(vv->commandq.vq, &detach_list);

	if (list_empty(&detach_list))
		return;

	list_for_each_entry_safe(entry, tmp, &detach_list, list) {
		list_del(&entry->list);
		free_vbuf(vv, entry);
	}
}

int virtio_video_alloc_vbufs(struct virtio_video *vv)
{
	vv->vbufs =
		kmem_cache_create("virtio-video-vbufs", VBUFFER_SIZE,
				  __alignof__(struct virtio_video_vbuffer), 0,
				  NULL);
	if (!vv->vbufs)
		return -ENOMEM;

	return 0;
}

void virtio_video_free_vbufs(struct virtio_video *vv)
{
	virtio_video_deatch_vbufs(vv);
	kmem_cache_destroy(vv->vbufs);
	vv->vbufs = NULL;
}

static void *virtio_video_alloc_req(struct virtio_video *vv,
				    struct virtio_video_vbuffer **vbuffer_p,
				    int size)
{
	struct virtio_video_vbuffer *vbuf;

	vbuf = virtio_video_get_vbuf(vv, size,
				     sizeof(struct virtio_video_cmd_hdr),
				     NULL, NULL);
	if (IS_ERR(vbuf)) {
		*vbuffer_p = NULL;
		return ERR_CAST(vbuf);
	}
	*vbuffer_p = vbuf;

	return vbuf->buf;
}

static void *
virtio_video_alloc_req_resp(struct virtio_video *vv,
			    virtio_video_resp_cb cb,
			    struct virtio_video_vbuffer **vbuffer_p,
			    int req_size, int resp_size,
			    void *resp_buf)
{
	struct virtio_video_vbuffer *vbuf;

	vbuf = virtio_video_get_vbuf(vv, req_size, resp_size, resp_buf, cb);
	if (IS_ERR(vbuf)) {
		*vbuffer_p = NULL;
		return ERR_CAST(vbuf);
	}
	*vbuffer_p = vbuf;

	return vbuf->buf;
}

void virtio_video_dequeue_cmd_func(struct work_struct *work)
{
	struct virtio_video *vv =
		container_of(work, struct virtio_video,
			     commandq.dequeue_work);
	struct list_head reclaim_list;
	struct virtio_video_vbuffer *entry, *tmp;
	struct virtio_video_cmd_hdr *resp;

	INIT_LIST_HEAD(&reclaim_list);
	spin_lock(&vv->commandq.qlock);
	do {
		virtqueue_disable_cb(vv->commandq.vq);
		reclaim_vbufs(vv->commandq.vq, &reclaim_list);

	} while (!virtqueue_enable_cb(vv->commandq.vq));
	spin_unlock(&vv->commandq.qlock);

	list_for_each_entry_safe(entry, tmp, &reclaim_list, list) {
		resp = (struct virtio_video_cmd_hdr *)entry->resp_buf;
		if (resp->type >=
		    cpu_to_le32(VIRTIO_VIDEO_RESP_ERR_INVALID_OPERATION))
			v4l2_dbg(1, vv->debug, &vv->v4l2_dev,
				 "response 0x%x\n", le32_to_cpu(resp->type));
		if (entry->resp_cb)
			entry->resp_cb(vv, entry);

		list_del(&entry->list);
		free_vbuf(vv, entry);
	}
	wake_up(&vv->commandq.ack_queue);
}

void virtio_video_dequeue_event_func(struct work_struct *work)
{
	struct virtio_video *vv =
		container_of(work, struct virtio_video,
			     eventq.dequeue_work);
	struct list_head reclaim_list;
	struct virtio_video_vbuffer *entry, *tmp;

	INIT_LIST_HEAD(&reclaim_list);
	spin_lock(&vv->eventq.qlock);
	do {
		virtqueue_disable_cb(vv->eventq.vq);
		reclaim_vbufs(vv->eventq.vq, &reclaim_list);

	} while (!virtqueue_enable_cb(vv->eventq.vq));
	spin_unlock(&vv->eventq.qlock);

	list_for_each_entry_safe(entry, tmp, &reclaim_list, list) {
		entry->resp_cb(vv, entry);
		list_del(&entry->list);
	}
	wake_up(&vv->eventq.ack_queue);
}

static int
virtio_video_queue_cmd_buffer_locked(struct virtio_video *vv,
				      struct virtio_video_vbuffer *vbuf)
{
	struct virtqueue *vq = vv->commandq.vq;
	struct scatterlist *sgs[3], vreq, vout, vresp;
	int outcnt = 0, incnt = 0;
	int ret;

	if (!vv->vq_ready)
		return -ENODEV;

	sg_init_one(&vreq, vbuf->buf, vbuf->size);
	sgs[outcnt + incnt] = &vreq;
	outcnt++;

	if (vbuf->data_size) {
		sg_init_one(&vout, vbuf->data_buf, vbuf->data_size);
		sgs[outcnt + incnt] = &vout;
		outcnt++;
	}

	if (vbuf->resp_size) {
		sg_init_one(&vresp, vbuf->resp_buf, vbuf->resp_size);
		sgs[outcnt + incnt] = &vresp;
		incnt++;
	}

retry:
	ret = virtqueue_add_sgs(vq, sgs, outcnt, incnt, vbuf, GFP_ATOMIC);
	if (ret == -ENOSPC) {
		spin_unlock(&vv->commandq.qlock);
		wait_event(vv->commandq.ack_queue, vq->num_free);
		spin_lock(&vv->commandq.qlock);
		goto retry;
	} else {
		virtqueue_kick(vq);
	}

	return ret;
}

static int virtio_video_queue_cmd_buffer(struct virtio_video *vv,
					  struct virtio_video_vbuffer *vbuf)
{
	int ret;

	spin_lock(&vv->commandq.qlock);
	ret = virtio_video_queue_cmd_buffer_locked(vv, vbuf);
	spin_unlock(&vv->commandq.qlock);

	return ret;
}

static int virtio_video_queue_event_buffer(struct virtio_video *vv,
					   struct virtio_video_vbuffer *vbuf)
{
	int ret;
	struct scatterlist vresp;
	struct virtqueue *vq = vv->eventq.vq;

	spin_lock(&vv->eventq.qlock);
	sg_init_one(&vresp, vbuf->resp_buf, vbuf->resp_size);
	ret = virtqueue_add_inbuf(vq, &vresp, 1, vbuf, GFP_ATOMIC);
	spin_unlock(&vv->eventq.qlock);
	if (ret)
		return ret;

	virtqueue_kick(vq);

	return 0;
}

static void virtio_video_event_cb(struct virtio_video *vv,
				  struct virtio_video_vbuffer *vbuf)
{
	int ret;
	struct virtio_video_stream *stream;
	struct virtio_video_event *event =
		(struct virtio_video_event *)vbuf->resp_buf;

	stream = idr_find(&vv->stream_idr, event->stream_id);
	if (!stream) {
		v4l2_warn(&vv->v4l2_dev, "no stream %u found for event\n",
			  event->stream_id);
		return;
	}

	switch (le32_to_cpu(event->event_type)) {
	case VIRTIO_VIDEO_EVENT_DECODER_RESOLUTION_CHANGED:
		virtio_video_cmd_get_params(vv, stream,
					   VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);
		virtio_video_queue_res_chg_event(stream);
		if (stream->state == STREAM_STATE_INIT) {
			stream->state = STREAM_STATE_METADATA;
			wake_up(&vv->wq);
		}
		break;
	default:
		v4l2_warn(&vv->v4l2_dev, "failed to queue event buffer\n");
		break;
	}

	memset(vbuf->resp_buf, 0, vbuf->resp_size);
	ret = virtio_video_queue_event_buffer(vv, vbuf);
	if (ret)
		v4l2_warn(&vv->v4l2_dev, "queue event buffer failed\n");
}

int virtio_video_alloc_events(struct virtio_video *vv, size_t num)
{
	int ret;
	size_t i;
	struct virtio_video_vbuffer *vbuf;

	for (i = 0; i < num; i++) {
		vbuf = virtio_video_get_vbuf(vv, 0,
					     sizeof(struct virtio_video_event),
					     NULL, virtio_video_event_cb);
		if (IS_ERR(vbuf))
			return PTR_ERR(vbuf);

		ret = virtio_video_queue_event_buffer(vv, vbuf);
		if (ret)
			return ret;
	}

	return 0;
}

int virtio_video_cmd_stream_create(struct virtio_video *vv, uint32_t stream_id,
				   enum virtio_video_format format,
				   const char *tag)
{
	struct virtio_video_stream_create *req_p;
	struct virtio_video_vbuffer *vbuf;
	int resource_type;

	switch (vv->res_type) {
	case RESOURCE_TYPE_GUEST_PAGES:
		resource_type = VIRTIO_VIDEO_MEM_TYPE_GUEST_PAGES;
		break;
	case RESOURCE_TYPE_VIRTIO_OBJECT:
		resource_type = VIRTIO_VIDEO_MEM_TYPE_VIRTIO_OBJECT;
		break;
	default:
		return -EINVAL;
	}

	req_p = virtio_video_alloc_req(vv, &vbuf, sizeof(*req_p));
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_STREAM_CREATE);
	req_p->hdr.stream_id = cpu_to_le32(stream_id);
	req_p->coded_format = cpu_to_le32(format);
	req_p->in_mem_type = cpu_to_le32(resource_type);
	req_p->out_mem_type = cpu_to_le32(resource_type);

	strncpy(req_p->tag, tag, sizeof(req_p->tag) - 1);
	req_p->tag[sizeof(req_p->tag) - 1] = 0;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

int virtio_video_cmd_stream_destroy(struct virtio_video *vv, uint32_t stream_id)
{
	struct virtio_video_stream_destroy *req_p;
	struct virtio_video_vbuffer *vbuf;

	req_p = virtio_video_alloc_req(vv, &vbuf, sizeof(*req_p));
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_STREAM_DESTROY);
	req_p->hdr.stream_id = cpu_to_le32(stream_id);

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

int virtio_video_cmd_stream_drain(struct virtio_video *vv, uint32_t stream_id)
{
	struct virtio_video_stream_drain *req_p;
	struct virtio_video_vbuffer *vbuf;

	req_p = virtio_video_alloc_req(vv, &vbuf, sizeof(*req_p));
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_STREAM_DRAIN);
	req_p->hdr.stream_id = cpu_to_le32(stream_id);

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

static void virtio_video_cmd_resource_create_core(
	struct virtio_video *vv, struct virtio_video_resource_create *req_p,
	uint32_t stream_id, uint32_t resource_id, uint32_t queue_type,
	unsigned int num_planes)
{
	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_RESOURCE_CREATE);
	req_p->hdr.stream_id = cpu_to_le32(stream_id);
	req_p->resource_id = cpu_to_le32(resource_id);
	req_p->queue_type = cpu_to_le32(queue_type);
	req_p->num_planes = cpu_to_le32(num_planes);
}

int virtio_video_cmd_resource_create_page(
	struct virtio_video *vv, uint32_t stream_id, uint32_t resource_id,
	uint32_t queue_type, unsigned int num_planes, unsigned int *num_entries,
	struct virtio_video_mem_entry *ents)
{
	struct virtio_video_resource_create *req_p;
	struct virtio_video_vbuffer *vbuf;
	unsigned int nents = 0;
	int i;

	req_p = virtio_video_alloc_req(vv, &vbuf, sizeof(*req_p));
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	virtio_video_cmd_resource_create_core(vv, req_p, stream_id, resource_id,
					      queue_type, num_planes);

	for (i = 0; i < num_planes; i++) {
		nents += num_entries[i];
		req_p->num_entries[i] = cpu_to_le32(num_entries[i]);
	}

	vbuf->data_buf = ents;
	vbuf->data_size = sizeof(*ents) * nents;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

int virtio_video_cmd_resource_create_object(
	struct virtio_video *vv, uint32_t stream_id, uint32_t resource_id,
	uint32_t queue_type, unsigned int num_planes, struct vb2_plane *planes,
	struct virtio_video_object_entry *ents)
{
	struct virtio_video_resource_create *req_p;
	struct virtio_video_vbuffer *vbuf;
	int i;

	req_p = virtio_video_alloc_req(vv, &vbuf, sizeof(*req_p));
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	virtio_video_cmd_resource_create_core(vv, req_p, stream_id, resource_id,
					      queue_type, num_planes);

	req_p->planes_layout =
		cpu_to_le32(VIRTIO_VIDEO_PLANES_LAYOUT_SINGLE_BUFFER);
	for (i = 0; i < num_planes; i++)
		req_p->plane_offsets[i] = planes[i].data_offset;

	vbuf->data_buf = ents;
	vbuf->data_size = sizeof(*ents);

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

static void
virtio_video_cmd_resource_destroy_all_cb(struct virtio_video *vv,
					 struct virtio_video_vbuffer *vbuf)
{
	struct virtio_video_stream *stream = vbuf->priv;
	struct virtio_video_resource_destroy_all *req_p =
		(struct virtio_video_resource_destroy_all *)vbuf->buf;

	switch (le32_to_cpu(req_p->queue_type)) {
	case VIRTIO_VIDEO_QUEUE_TYPE_INPUT:
		stream->src_destroyed = true;
		break;
	case VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT:
		stream->dst_destroyed = true;
		break;
	default:
		v4l2_err(&vv->v4l2_dev, "invalid queue type: %u\n",
			 req_p->queue_type);
		return;
	}

	wake_up(&vv->wq);
}

int virtio_video_cmd_resource_destroy_all(struct virtio_video *vv,
					  struct virtio_video_stream *stream,
					  enum virtio_video_queue_type qtype)
{
	struct virtio_video_resource_destroy_all *req_p;
	struct virtio_video_vbuffer *vbuf;

	req_p = virtio_video_alloc_req_resp
		(vv, &virtio_video_cmd_resource_destroy_all_cb,
		 &vbuf, sizeof(*req_p),
		 sizeof(struct virtio_video_cmd_hdr), NULL);
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_RESOURCE_DESTROY_ALL);
	req_p->hdr.stream_id = cpu_to_le32(stream->stream_id);
	req_p->queue_type = cpu_to_le32(qtype);

	vbuf->priv = stream;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

static void
virtio_video_cmd_resource_queue_cb(struct virtio_video *vv,
				   struct virtio_video_vbuffer *vbuf)
{
	uint32_t flags, bytesused;
	uint64_t timestamp;
	struct virtio_video_buffer *virtio_vb = vbuf->priv;
	struct virtio_video_resource_queue_resp *resp =
		(struct virtio_video_resource_queue_resp *)vbuf->resp_buf;

	flags = le32_to_cpu(resp->flags);
	bytesused = le32_to_cpu(resp->size);
	timestamp = le64_to_cpu(resp->timestamp);

	virtio_video_buf_done(virtio_vb, flags, timestamp, bytesused);
}

int virtio_video_cmd_resource_queue(struct virtio_video *vv, uint32_t stream_id,
				    struct virtio_video_buffer *virtio_vb,
				    uint32_t data_size[],
				    uint8_t num_data_size, uint32_t queue_type)
{
	uint8_t i;
	struct virtio_video_resource_queue *req_p;
	struct virtio_video_resource_queue_resp *resp_p;
	struct virtio_video_vbuffer *vbuf;
	size_t resp_size = sizeof(struct virtio_video_resource_queue_resp);

	req_p = virtio_video_alloc_req_resp(vv,
					    &virtio_video_cmd_resource_queue_cb,
					    &vbuf, sizeof(*req_p), resp_size,
					    NULL);
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_RESOURCE_QUEUE);
	req_p->hdr.stream_id = cpu_to_le32(stream_id);
	req_p->queue_type = cpu_to_le32(queue_type);
	req_p->resource_id = cpu_to_le32(virtio_vb->resource_id);
	req_p->num_data_sizes = num_data_size;
	req_p->timestamp =
		cpu_to_le64(virtio_vb->v4l2_m2m_vb.vb.vb2_buf.timestamp);

	for (i = 0; i < num_data_size; ++i)
		req_p->data_sizes[i] = cpu_to_le32(data_size[i]);

	resp_p = (struct virtio_video_resource_queue_resp *)vbuf->resp_buf;
	memset(resp_p, 0, sizeof(*resp_p));

	vbuf->priv = virtio_vb;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

static void
virtio_video_cmd_queue_clear_cb(struct virtio_video *vv,
				struct virtio_video_vbuffer *vbuf)
{
	struct virtio_video_stream *stream = vbuf->priv;
	struct virtio_video_queue_clear *req_p =
		(struct virtio_video_queue_clear *)vbuf->buf;

	if (le32_to_cpu(req_p->queue_type) == VIRTIO_VIDEO_QUEUE_TYPE_INPUT)
		stream->src_cleared = true;
	else
		stream->dst_cleared = true;

	wake_up(&vv->wq);
}

int virtio_video_cmd_queue_clear(struct virtio_video *vv,
				 struct virtio_video_stream *stream,
				 uint32_t queue_type)
{
	struct virtio_video_queue_clear *req_p;
	struct virtio_video_vbuffer *vbuf;

	req_p = virtio_video_alloc_req_resp
		(vv, &virtio_video_cmd_queue_clear_cb,
		 &vbuf, sizeof(*req_p),
		 sizeof(struct virtio_video_cmd_hdr), NULL);
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_QUEUE_CLEAR);
	req_p->hdr.stream_id = cpu_to_le32(stream->stream_id);
	req_p->queue_type = cpu_to_le32(queue_type);

	vbuf->priv = stream;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

static void
virtio_video_query_caps_cb(struct virtio_video *vv,
			   struct virtio_video_vbuffer *vbuf)
{
	bool *got_resp_p = vbuf->priv;
	*got_resp_p = true;
	wake_up(&vv->wq);
}

int virtio_video_query_capability(struct virtio_video *vv, void *resp_buf,
				  size_t resp_size, uint32_t queue_type)
{
	struct virtio_video_query_capability *req_p = NULL;
	struct virtio_video_vbuffer *vbuf = NULL;

	if (!vv || !resp_buf)
		return -1;

	req_p = virtio_video_alloc_req_resp(vv, &virtio_video_query_caps_cb,
					    &vbuf, sizeof(*req_p), resp_size,
					    resp_buf);
	if (IS_ERR(req_p))
		return -1;

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_QUERY_CAPABILITY);
	req_p->queue_type = cpu_to_le32(queue_type);

	vbuf->priv = &vv->got_caps;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

int virtio_video_query_control_level(struct virtio_video *vv, void *resp_buf,
				     size_t resp_size, uint32_t format)
{
	struct virtio_video_query_control *req_p = NULL;
	struct virtio_video_query_control_level *ctrl_l = NULL;
	struct virtio_video_vbuffer *vbuf = NULL;
	uint32_t req_size = 0;

	if (!vv || !resp_buf)
		return -1;

	req_size = sizeof(struct virtio_video_query_control) +
		sizeof(struct virtio_video_query_control_level);

	req_p = virtio_video_alloc_req_resp(vv, &virtio_video_query_caps_cb,
					    &vbuf, req_size, resp_size,
					    resp_buf);
	if (IS_ERR(req_p))
		return -1;

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_QUERY_CONTROL);
	req_p->control = cpu_to_le32(VIRTIO_VIDEO_CONTROL_LEVEL);
	ctrl_l = (void *)((char *)req_p +
			  sizeof(struct virtio_video_query_control));
	ctrl_l->format = cpu_to_le32(format);

	vbuf->priv = &vv->got_control;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

int virtio_video_query_control_profile(struct virtio_video *vv, void *resp_buf,
				       size_t resp_size, uint32_t format)
{
	struct virtio_video_query_control *req_p = NULL;
	struct virtio_video_query_control_profile *ctrl_p = NULL;
	struct virtio_video_vbuffer *vbuf = NULL;
	uint32_t req_size = 0;

	if (!vv || !resp_buf)
		return -1;

	req_size = sizeof(struct virtio_video_query_control) +
		sizeof(struct virtio_video_query_control_profile);

	req_p = virtio_video_alloc_req_resp(vv, &virtio_video_query_caps_cb,
					    &vbuf, req_size, resp_size,
					    resp_buf);
	if (IS_ERR(req_p))
		return -1;

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_QUERY_CONTROL);
	req_p->control = cpu_to_le32(VIRTIO_VIDEO_CONTROL_PROFILE);
	ctrl_p = (void *)((char *)req_p +
			  sizeof(struct virtio_video_query_control));
	ctrl_p->format = cpu_to_le32(format);

	vbuf->priv = &vv->got_control;

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

static void
virtio_video_cmd_get_params_cb(struct virtio_video *vv,
			       struct virtio_video_vbuffer *vbuf)
{
	int i;
	struct virtio_video_get_params_resp *resp =
		(struct virtio_video_get_params_resp *)vbuf->resp_buf;
	struct virtio_video_params *params = &resp->params;
	struct virtio_video_stream *stream = vbuf->priv;
	enum virtio_video_queue_type queue_type;
	struct video_format_info *format_info = NULL;

	queue_type = le32_to_cpu(params->queue_type);
	if (queue_type == VIRTIO_VIDEO_QUEUE_TYPE_INPUT)
		format_info = &stream->in_info;
	else
		format_info = &stream->out_info;

	if (!format_info)
		return;

	format_info->frame_rate = le32_to_cpu(params->frame_rate);
	format_info->frame_width = le32_to_cpu(params->frame_width);
	format_info->frame_height = le32_to_cpu(params->frame_height);
	format_info->min_buffers = le32_to_cpu(params->min_buffers);
	format_info->max_buffers = le32_to_cpu(params->max_buffers);
	format_info->fourcc_format =
		virtio_video_format_to_v4l2(le32_to_cpu(params->format));

	format_info->crop.top = le32_to_cpu(params->crop.top);
	format_info->crop.left = le32_to_cpu(params->crop.left);
	format_info->crop.width = le32_to_cpu(params->crop.width);
	format_info->crop.height = le32_to_cpu(params->crop.height);

	format_info->num_planes = le32_to_cpu(params->num_planes);
	for (i = 0; i < le32_to_cpu(params->num_planes); i++) {
		struct virtio_video_plane_format *plane_formats =
						 &params->plane_formats[i];
		struct video_plane_format *plane_format =
						 &format_info->plane_format[i];

		plane_format->plane_size =
				 le32_to_cpu(plane_formats->plane_size);
		plane_format->stride = le32_to_cpu(plane_formats->stride);
	}

	format_info->is_updated = true;
	wake_up(&vv->wq);
}

int virtio_video_cmd_get_params(struct virtio_video *vv,
			       struct virtio_video_stream *stream,
			       uint32_t queue_type)
{
	int ret;
	struct virtio_video_get_params *req_p = NULL;
	struct virtio_video_vbuffer *vbuf = NULL;
	struct virtio_video_get_params_resp *resp_p;
	struct video_format_info *format_info = NULL;
	size_t resp_size = sizeof(struct virtio_video_get_params_resp);

	if (!vv || !stream)
		return -1;

	req_p = virtio_video_alloc_req_resp(vv,
					&virtio_video_cmd_get_params_cb,
					&vbuf, sizeof(*req_p), resp_size,
					NULL);

	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_GET_PARAMS);
	req_p->hdr.stream_id = cpu_to_le32(stream->stream_id);
	req_p->queue_type = cpu_to_le32(queue_type);

	resp_p = (struct virtio_video_get_params_resp *)vbuf->resp_buf;
	memset(resp_p, 0, sizeof(*resp_p));

	if (req_p->queue_type == VIRTIO_VIDEO_QUEUE_TYPE_INPUT)
		format_info = &stream->in_info;
	else
		format_info = &stream->out_info;

	format_info->is_updated = false;

	vbuf->priv = stream;
	ret = virtio_video_queue_cmd_buffer(vv, vbuf);
	if (ret)
		return ret;

	ret = wait_event_timeout(vv->wq,
				 format_info->is_updated, 5 * HZ);
	if (ret == 0) {
		v4l2_err(&vv->v4l2_dev, "timed out waiting for get_params\n");
		return -1;
	}
	return 0;
}

int
virtio_video_cmd_set_params(struct virtio_video *vv,
			    struct virtio_video_stream *stream,
			    struct video_format_info *format_info,
			    uint32_t queue_type)
{
	int i;
	struct virtio_video_set_params *req_p;
	struct virtio_video_vbuffer *vbuf;

	req_p = virtio_video_alloc_req(vv, &vbuf, sizeof(*req_p));
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_SET_PARAMS);
	req_p->hdr.stream_id = cpu_to_le32(stream->stream_id);
	req_p->params.queue_type = cpu_to_le32(queue_type);
	req_p->params.frame_rate = cpu_to_le32(format_info->frame_rate);
	req_p->params.frame_width = cpu_to_le32(format_info->frame_width);
	req_p->params.frame_height = cpu_to_le32(format_info->frame_height);
	req_p->params.format = virtio_video_v4l2_format_to_virtio(
				 cpu_to_le32(format_info->fourcc_format));
	req_p->params.min_buffers = cpu_to_le32(format_info->min_buffers);
	req_p->params.max_buffers = cpu_to_le32(format_info->max_buffers);
	req_p->params.num_planes = cpu_to_le32(format_info->num_planes);

	for (i = 0; i < format_info->num_planes; i++) {
		struct virtio_video_plane_format *plane_formats =
			&req_p->params.plane_formats[i];
		struct video_plane_format *plane_format =
			&format_info->plane_format[i];
		plane_formats->plane_size =
				 cpu_to_le32(plane_format->plane_size);
		plane_formats->stride = cpu_to_le32(plane_format->stride);
	}

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}

static void
virtio_video_cmd_get_ctrl_profile_cb(struct virtio_video *vv,
				     struct virtio_video_vbuffer *vbuf)
{
	struct virtio_video_get_control_resp *resp =
		(struct virtio_video_get_control_resp *)vbuf->resp_buf;
	struct virtio_video_control_val_profile *resp_p = NULL;
	struct virtio_video_stream *stream = vbuf->priv;
	struct video_control_info *control = &stream->control;

	if (!control)
		return;

	resp_p = (void *)((char *) resp +
			  sizeof(struct virtio_video_get_control_resp));

	control->profile = le32_to_cpu(resp_p->profile);
	control->is_updated = true;
	wake_up(&vv->wq);
}

static void
virtio_video_cmd_get_ctrl_level_cb(struct virtio_video *vv,
				   struct virtio_video_vbuffer *vbuf)
{
	struct virtio_video_get_control_resp *resp =
		(struct virtio_video_get_control_resp *)vbuf->resp_buf;
	struct virtio_video_control_val_level *resp_p = NULL;
	struct virtio_video_stream *stream = vbuf->priv;
	struct video_control_info *control = &stream->control;

	if (!control)
		return;

	resp_p = (void *)((char *)resp +
			  sizeof(struct virtio_video_get_control_resp));

	control->level = le32_to_cpu(resp_p->level);
	control->is_updated = true;
	wake_up(&vv->wq);
}

static void
virtio_video_cmd_get_ctrl_bitrate_cb(struct virtio_video *vv,
				     struct virtio_video_vbuffer *vbuf)
{
	struct virtio_video_get_control_resp *resp =
		(struct virtio_video_get_control_resp *)vbuf->resp_buf;
	struct virtio_video_control_val_bitrate *resp_p = NULL;
	struct virtio_video_stream *stream = vbuf->priv;
	struct video_control_info *control = &stream->control;

	if (!control)
		return;

	resp_p = (void *)((char *) resp +
			  sizeof(struct virtio_video_get_control_resp));

	control->bitrate = le32_to_cpu(resp_p->bitrate);
	control->is_updated = true;
	wake_up(&vv->wq);
}

int virtio_video_cmd_get_control(struct virtio_video *vv,
				 struct virtio_video_stream *stream,
				 uint32_t virtio_ctrl)
{
	int ret = 0;
	struct virtio_video_get_control *req_p = NULL;
	struct virtio_video_get_control_resp *resp_p = NULL;
	struct virtio_video_vbuffer *vbuf = NULL;
	size_t resp_size = sizeof(struct virtio_video_get_control_resp);
	virtio_video_resp_cb cb;

	if (!vv)
		return -1;

	switch (virtio_ctrl) {
	case VIRTIO_VIDEO_CONTROL_PROFILE:
		resp_size += sizeof(struct virtio_video_control_val_profile);
		cb = &virtio_video_cmd_get_ctrl_profile_cb;
		break;
	case VIRTIO_VIDEO_CONTROL_LEVEL:
		resp_size += sizeof(struct virtio_video_control_val_level);
		cb = &virtio_video_cmd_get_ctrl_level_cb;
		break;
	case VIRTIO_VIDEO_CONTROL_BITRATE:
		resp_size += sizeof(struct virtio_video_control_val_bitrate);
		cb = &virtio_video_cmd_get_ctrl_bitrate_cb;
		break;
	default:
		return -1;
	}

	req_p = virtio_video_alloc_req_resp(vv, cb, &vbuf,
					    sizeof(*req_p), resp_size, NULL);
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_GET_CONTROL);
	req_p->hdr.stream_id = cpu_to_le32(stream->stream_id);
	req_p->control = cpu_to_le32(virtio_ctrl);

	resp_p = (struct virtio_video_get_control_resp *)vbuf->resp_buf;
	memset(resp_p, 0, resp_size);

	stream->control.is_updated = false;

	vbuf->priv = stream;
	ret = virtio_video_queue_cmd_buffer(vv, vbuf);
	if (ret)
		return ret;

	ret = wait_event_timeout(vv->wq, stream->control.is_updated, 5 * HZ);
	if (ret == 0) {
		v4l2_err(&vv->v4l2_dev, "timed out waiting for get_params\n");
		return -1;
	}
	return 0;
}

int virtio_video_cmd_set_control(struct virtio_video *vv, uint32_t stream_id,
				 uint32_t control, uint32_t value)
{
	struct virtio_video_set_control *req_p = NULL;
	struct virtio_video_vbuffer *vbuf = NULL;
	struct virtio_video_control_val_level *ctrl_l = NULL;
	struct virtio_video_control_val_profile *ctrl_p = NULL;
	struct virtio_video_control_val_bitrate *ctrl_b = NULL;
	size_t size;

	if (!vv || value == 0)
		return -EINVAL;

	switch (control) {
	case VIRTIO_VIDEO_CONTROL_PROFILE:
		size = sizeof(struct virtio_video_control_val_profile);
		break;
	case VIRTIO_VIDEO_CONTROL_LEVEL:
		size = sizeof(struct virtio_video_control_val_level);
		break;
	case VIRTIO_VIDEO_CONTROL_BITRATE:
		size = sizeof(struct virtio_video_control_val_bitrate);
		break;
	default:
		return -1;
	}

	req_p = virtio_video_alloc_req(vv, &vbuf, size + sizeof(*req_p));
	if (IS_ERR(req_p))
		return PTR_ERR(req_p);

	req_p->hdr.type = cpu_to_le32(VIRTIO_VIDEO_CMD_SET_CONTROL);
	req_p->hdr.stream_id = cpu_to_le32(stream_id);
	req_p->control = cpu_to_le32(control);

	switch (control) {
	case VIRTIO_VIDEO_CONTROL_PROFILE:
		ctrl_p = (void *)((char *)req_p +
				  sizeof(struct virtio_video_set_control));
		ctrl_p->profile = cpu_to_le32(value);
		break;
	case VIRTIO_VIDEO_CONTROL_LEVEL:
		ctrl_l = (void *)((char *)req_p +
				 sizeof(struct virtio_video_set_control));
		ctrl_l->level = cpu_to_le32(value);
		break;
	case VIRTIO_VIDEO_CONTROL_BITRATE:
		ctrl_b = (void *)((char *)req_p +
				 sizeof(struct virtio_video_set_control));
		ctrl_b->bitrate = cpu_to_le32(value);
		break;
	}

	return virtio_video_queue_cmd_buffer(vv, vbuf);
}
