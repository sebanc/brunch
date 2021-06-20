// SPDX-License-Identifier: GPL-2.0+
/* Driver for virtio video device.
 *
 * Copyright 2019 OpenSynergy GmbH.
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

#include <linux/dma-buf.h>
#include <linux/virtio_dma_buf.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-sg.h>

#include "virtio_video.h"
#include "virtio_video_dec.h"
#include "virtio_video_enc.h"

int virtio_video_queue_setup(struct vb2_queue *vq, unsigned int *num_buffers,
			     unsigned int *num_planes, unsigned int sizes[],
			     struct device *alloc_devs[])
{
	int i;
	struct virtio_video_stream *stream = vb2_get_drv_priv(vq);
	struct video_format_info *p_info;

	if (*num_planes)
		return 0;

	if (V4L2_TYPE_IS_OUTPUT(vq->type))
		p_info = &stream->in_info;
	else
		p_info = &stream->out_info;

	*num_planes = p_info->num_planes;

	for (i = 0; i < p_info->num_planes; i++)
		sizes[i] = p_info->plane_format[i].plane_size;

	return 0;
}

static int virtio_video_get_dma_buf_id(struct virtio_video_device *vvd,
			  struct vb2_buffer *vb, uuid_t *uuid)
{
	/**
	 * For multiplanar formats, we assume all planes are on one DMA buffer.
	 */
	if (vb->planes[0].dbuf) {
		return virtio_dma_buf_get_uuid(vb->planes[0].dbuf, uuid);
	} else {
		return -EINVAL;
	}
}

static int virtio_video_send_resource_create_object(struct vb2_buffer *vb,
						    uint32_t resource_id,
						    uuid_t uuid)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video *vv = to_virtio_vd(stream->video_dev)->vv;
	struct virtio_video_buffer *virtio_vb = to_virtio_vb(vb);
	struct vb2_buffer *cur_vb;
	struct virtio_video_object_entry *ent;
	int queue_type;
	int ret;
	bool *destroyed;

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		queue_type = VIRTIO_VIDEO_QUEUE_TYPE_INPUT;
		destroyed = &stream->src_destroyed;
	} else {
		queue_type = VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT;
		destroyed = &stream->dst_destroyed;
	}

	ent = kcalloc(1, sizeof(*ent), GFP_KERNEL);
	uuid_copy((uuid_t *) &ent->uuid, &uuid);

	ret = virtio_video_cmd_resource_create_object(vv, stream->stream_id,
						      resource_id,
						      queue_type,
						      vb->num_planes,
						      vb->planes, ent);
	if (ret) {
		kfree(ent);
		return ret;
	}

	/**
	 * If the given uuid was previously used in another entry, invalidate
	 * it because the uuid must be tied with only one resource_id.
	 */
	list_for_each_entry(cur_vb, &vb->vb2_queue->queued_list,
			    queued_entry) {
		struct virtio_video_buffer *cur_vvb =
			to_virtio_vb(cur_vb);

		if (uuid_equal(&uuid, &cur_vvb->uuid))
			cur_vvb->uuid = uuid_null;
	}

	virtio_vb->resource_id = resource_id;
	virtio_vb->uuid = uuid;
	*destroyed = false;

	return 0;
}

static int virtio_video_buf_init_guest_pages(struct vb2_buffer *vb)
{
	int ret = 0;
	unsigned int i, j;
	struct scatterlist *sg;
	struct virtio_video_mem_entry *ents;
	uint32_t num_ents[VIRTIO_VIDEO_MAX_PLANES];
	struct sg_table *sgt[VIRTIO_VIDEO_MAX_PLANES];
	uint32_t resource_id, nents = 0;
	struct vb2_queue *vq = vb->vb2_queue;
	enum v4l2_buf_type queue_type = vq->type;
	struct virtio_video_stream *stream = vb2_get_drv_priv(vq);
	struct virtio_video_buffer *virtio_vb = to_virtio_vb(vb);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;

	virtio_video_resource_id_get(vv, &resource_id);

	if (vv->supp_non_contig) {
		for (i = 0; i < vb->num_planes; i++) {
			sgt[i] = vb2_dma_sg_plane_desc(vb, i);
			nents += sgt[i]->nents;
		}

		ents = kcalloc(nents, sizeof(*ents), GFP_KERNEL);
		if (!ents)
			return -ENOMEM;

		for (i = 0; i < vb->num_planes; ++i) {
			for_each_sg(sgt[i]->sgl, sg, sgt[i]->nents, j) {
				ents[j].addr = cpu_to_le64(vv->use_dma_api
							   ? sg_dma_address(sg)
							   : sg_phys(sg));
				ents[j].length = cpu_to_le32(sg->length);
			}
			num_ents[i] = sgt[i]->nents;
		}
	} else {
		nents = vb->num_planes;

		ents = kcalloc(nents, sizeof(*ents), GFP_KERNEL);
		if (!ents)
			return -ENOMEM;

		for (i = 0; i < vb->num_planes; ++i) {
			ents[i].addr =
				cpu_to_le64(vb2_dma_contig_plane_dma_addr(vb,
									  i));
			ents[i].length = cpu_to_le32(vb->planes[i].length);
			num_ents[i] = 1;
		}
	}

	v4l2_dbg(1, vv->debug, &vv->v4l2_dev, "mem entries:\n");
	if (vv->debug >= 1) {
		for (i = 0; i < nents; i++)
			pr_debug("\t%03i: addr=%llx length=%u\n", i,
					ents[i].addr, ents[i].length);
	}

	ret = virtio_video_cmd_resource_create_page(
		vv, stream->stream_id, resource_id,
		to_virtio_queue_type(queue_type), vb->num_planes, num_ents,
		ents);
	if (ret) {
		virtio_video_resource_id_put(vvd->vv, resource_id);
		kfree(ents);

		return ret;
	}

	virtio_vb->queued = false;
	virtio_vb->resource_id = resource_id;

	return 0;
}

static int virtio_video_buf_init_virtio_object(struct vb2_buffer *vb)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video_buffer *virtio_vb = to_virtio_vb(vb);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	int ret;
	uint32_t resource_id;
	uuid_t uuid;

	ret = virtio_video_get_dma_buf_id(vvd, vb, &uuid);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to get DMA-buf handle");
		return ret;
	}
	virtio_video_resource_id_get(vv, &resource_id);

	ret = virtio_video_send_resource_create_object(vb, resource_id, uuid);
	if (ret) {
		virtio_video_resource_id_put(vv, resource_id);
		return ret;
	}

	virtio_vb->queued = false;

	return 0;
}

int virtio_video_buf_init(struct vb2_buffer *vb)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;

	switch (vv->res_type) {
	case RESOURCE_TYPE_GUEST_PAGES:
		return virtio_video_buf_init_guest_pages(vb);
	case RESOURCE_TYPE_VIRTIO_OBJECT:
		return virtio_video_buf_init_virtio_object(vb);
	default:
		return -EINVAL;
	}
}

int virtio_video_buf_prepare(struct vb2_buffer *vb)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video_buffer *virtio_vb = to_virtio_vb(vb);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	uuid_t uuid;
	int ret;

	if (vv->res_type != RESOURCE_TYPE_VIRTIO_OBJECT)
		return 0;

	ret = virtio_video_get_dma_buf_id(vvd, vb, &uuid);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to get DMA-buf handle");
		return ret;
	}

	/**
	 * If a user gave a different object as a buffer from the previous
	 * one, send RESOURCE_CREATE again to register the object.
	 */
	if (!uuid_equal(&uuid, &virtio_vb->uuid)) {
		ret = virtio_video_send_resource_create_object(
			vb, virtio_vb->resource_id, uuid);
		if (ret)
			return ret;
	}

	return ret;
}

void virtio_video_buf_cleanup(struct vb2_buffer *vb)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video_buffer *virtio_vb = to_virtio_vb(vb);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;

	virtio_video_resource_id_put(vv, virtio_vb->resource_id);
}

int virtio_video_querycap(struct file *file, void *fh,
			  struct v4l2_capability *cap)
{
	struct video_device *video_dev = video_devdata(file);

	strncpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
	strncpy(cap->card, video_dev->name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "virtio:%s",
		 video_dev->name);

	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

int virtio_video_enum_framesizes(struct file *file, void *fh,
				 struct v4l2_frmsizeenum *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format *fmt = NULL;
	struct video_format_frame *frm = NULL;
	struct virtio_video_format_frame *frame = NULL;
	int idx = f->index;

	fmt = find_video_format(&vvd->input_fmt_list, f->pixel_format);
	if (fmt == NULL)
		fmt = find_video_format(&vvd->output_fmt_list, f->pixel_format);
	if (fmt == NULL)
		return -EINVAL;

	if (idx >= fmt->desc.num_frames)
		return -EINVAL;

	frm = &fmt->frames[idx];
	frame = &frm->frame;

	if (frame->width.min == frame->width.max &&
	    frame->height.min == frame->height.max) {
		f->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		f->discrete.width = frame->width.min;
		f->discrete.height = frame->height.min;
		return 0;
	}

	f->type = V4L2_FRMSIZE_TYPE_CONTINUOUS;
	f->stepwise.min_width = frame->width.min;
	f->stepwise.max_width = frame->width.max;
	f->stepwise.min_height = frame->height.min;
	f->stepwise.max_height = frame->height.max;
	f->stepwise.step_width = frame->width.step;
	f->stepwise.step_height = frame->height.step;
	return 0;
}

static bool in_stepped_interval(uint32_t int_start, uint32_t int_end,
				uint32_t step, uint32_t point)
{
	if (point < int_start || point > int_end)
		return false;

	if (step == 0 && int_start == int_end && int_start == point)
		return true;

	if (step != 0 && (point - int_start) % step == 0)
		return true;

	return false;
}

int virtio_video_enum_framemintervals(struct file *file, void *fh,
				      struct v4l2_frmivalenum *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format *fmt = NULL;
	struct video_format_frame *frm = NULL;
	struct virtio_video_format_frame *frame = NULL;
	struct virtio_video_format_range *frate = NULL;
	int idx = f->index;
	int f_idx = 0;

	fmt = find_video_format(&vvd->input_fmt_list, f->pixel_format);
	if (fmt == NULL)
		fmt = find_video_format(&vvd->output_fmt_list, f->pixel_format);
	if (fmt == NULL)
		return -EINVAL;

	for (f_idx = 0; f_idx <= fmt->desc.num_frames; f_idx++) {
		frm = &fmt->frames[f_idx];
		frame = &frm->frame;
		if (in_stepped_interval(frame->width.min, frame->width.max,
					frame->width.step, f->width) &&
		   in_stepped_interval(frame->height.min, frame->height.max,
					frame->height.step, f->height))
			break;
	}

	if (frame == NULL || f->index >= frame->num_rates)
		return -EINVAL;

	frate = &frm->frame_rates[idx];
	if (frate->max == frate->min) {
		f->type = V4L2_FRMIVAL_TYPE_DISCRETE;
		f->discrete.numerator = 1;
		f->discrete.denominator = frate->max;
	} else {
		f->stepwise.min.numerator = 1;
		f->stepwise.min.denominator = frate->max;
		f->stepwise.max.numerator = 1;
		f->stepwise.max.denominator = frate->min;
		f->stepwise.step.numerator = 1;
		f->stepwise.step.denominator = frate->step;
		if (frate->step == 1)
			f->type = V4L2_FRMIVAL_TYPE_CONTINUOUS;
		else
			f->type = V4L2_FRMIVAL_TYPE_STEPWISE;
	}
	return 0;
}


int virtio_video_g_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct video_format_info *info;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct virtio_video_stream *stream = file2stream(file);

	if (!V4L2_TYPE_IS_OUTPUT(f->type))
		info = &stream->out_info;
	else
		info = &stream->in_info;

	virtio_video_format_from_info(info, pix_mp);

	return 0;
}

int virtio_video_s_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	int i, ret;
	struct virtio_video_stream *stream = file2stream(file);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct video_format_info info;
	struct video_format_info *p_info;
	uint32_t queue;

	ret = virtio_video_try_fmt(stream, f);
	if (ret)
		return ret;

	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		virtio_video_format_fill_default_info(&info, &stream->in_info);
		queue = VIRTIO_VIDEO_QUEUE_TYPE_INPUT;
	} else {
		virtio_video_format_fill_default_info(&info, &stream->out_info);
		queue = VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT;
	}

	info.frame_width = pix_mp->width;
	info.frame_height = pix_mp->height;
	info.num_planes = pix_mp->num_planes;
	info.fourcc_format = pix_mp->pixelformat;

	for (i = 0; i < info.num_planes; i++) {
		info.plane_format[i].stride =
					 pix_mp->plane_fmt[i].bytesperline;
		info.plane_format[i].plane_size =
					 pix_mp->plane_fmt[i].sizeimage;
	}

	virtio_video_cmd_set_params(vv, stream, &info, queue);
	virtio_video_cmd_get_params(vv, stream, VIRTIO_VIDEO_QUEUE_TYPE_INPUT);
	virtio_video_cmd_get_params(vv, stream, VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);

	if (V4L2_TYPE_IS_OUTPUT(f->type))
		p_info = &stream->in_info;
	else
		p_info = &stream->out_info;

	virtio_video_format_from_info(p_info, pix_mp);

	return 0;
}

int virtio_video_g_selection(struct file *file, void *fh,
			 struct v4l2_selection *sel)
{
	struct video_format_info *info = NULL;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);

	switch (vvd->type) {
	case VIRTIO_VIDEO_DEVICE_ENCODER:
		if (!V4L2_TYPE_IS_OUTPUT(sel->type))
			return -EINVAL;
		info = &stream->in_info;
		break;
	case VIRTIO_VIDEO_DEVICE_DECODER:
		if (V4L2_TYPE_IS_OUTPUT(sel->type))
			return -EINVAL;
		info = &stream->out_info;
		break;
	default:
		v4l2_err(&vvd->vv->v4l2_dev, "unsupported device type\n");
		return -EINVAL;
	}

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.width = info->frame_width;
		sel->r.height = info->frame_height;
		break;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		sel->r.left = info->crop.left;
		sel->r.top = info->crop.top;
		sel->r.width = info->crop.width;
		sel->r.height = info->crop.height;
		break;
	default:
		v4l2_dbg(1, vvd->vv->debug, &vvd->vv->v4l2_dev,
			 "unsupported/invalid selection target: %d\n",
			sel->target);
		return -EINVAL;
	}

	return 0;
}

int virtio_video_s_selection(struct file *file, void *fh,
			     struct v4l2_selection *sel)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	int ret;

	stream->out_info.crop.top = sel->r.top;
	stream->out_info.crop.left = sel->r.left;
	stream->out_info.crop.width = sel->r.width;
	stream->out_info.crop.height = sel->r.height;

	ret = virtio_video_cmd_set_params(vv, stream,  &stream->out_info,
					   VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);
	if (ret < 0)
		return -EINVAL;

	/* Get actual selection that was set */
	return virtio_video_cmd_get_params(vv, stream,
					   VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);
}

int virtio_video_try_fmt(struct virtio_video_stream *stream,
			 struct v4l2_format *f)
{
	int i, idx = 0;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format *fmt = NULL;
	bool found = false;
	struct video_format_frame *frm = NULL;
	struct virtio_video_format_frame *frame = NULL;

	if (V4L2_TYPE_IS_OUTPUT(f->type))
		fmt = find_video_format(&vvd->input_fmt_list,
					pix_mp->pixelformat);
	else
		fmt = find_video_format(&vvd->output_fmt_list,
					pix_mp->pixelformat);

	if (!fmt) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			virtio_video_format_from_info(&stream->out_info,
						      pix_mp);
		else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			virtio_video_format_from_info(&stream->in_info,
						      pix_mp);
		else
			return -EINVAL;
		return 0;
	}

	/* For coded formats whose metadata are in steram */
	if (pix_mp->width == 0 && pix_mp->height == 0)  {
		stream->current_frame = &fmt->frames[0];
		return 0;
	}

	for (i = 0; i < fmt->desc.num_frames && !found; i++) {
		frm = &fmt->frames[i];
		frame = &frm->frame;
		if (!within_range(frame->width.min, pix_mp->width,
				  frame->width.max))
			continue;

		if (!within_range(frame->height.min, pix_mp->height,
				  frame->height.max))
			continue;
		idx = i;
		/*
		 * Try to find a more suitable frame size. Go with the current
		 * one otherwise.
		 */
		if (needs_alignment(pix_mp->width, frame->width.step))
			continue;

		if (needs_alignment(pix_mp->height, frame->height.step))
			continue;

		stream->current_frame = frm;
		found = true;
	}

	if (!found) {
		frm = &fmt->frames[idx];
		frame = &frm->frame;
		pix_mp->width = clamp(pix_mp->width, frame->width.min,
				      frame->width.max);
		if (frame->width.step != 0)
			pix_mp->width = ALIGN(pix_mp->width, frame->width.step);

		pix_mp->height = clamp(pix_mp->height, frame->height.min,
				       frame->height.max);
		if (frame->height.step != 0)
			pix_mp->height = ALIGN(pix_mp->height,
					       frame->height.step);
		stream->current_frame = frm;
	}

	return 0;
}

static int virtio_video_queue_free(struct virtio_video *vv,
			  struct virtio_video_stream *stream,
			  enum v4l2_buf_type type)
{
	int ret;
	uint32_t queue_type = to_virtio_queue_type(type);
	const bool *destroyed = V4L2_TYPE_IS_OUTPUT(type) ?
		&stream->src_destroyed : &stream->dst_destroyed;

	ret = virtio_video_cmd_resource_destroy_all(vv, stream,
						    queue_type);
	if (ret) {
		v4l2_warn(&vv->v4l2_dev,
			  "failed to destroy resources\n");
		return ret;
	}

	ret = wait_event_timeout(vv->wq, *destroyed, 5 * HZ);
	if (ret == 0) {
		v4l2_err(&vv->v4l2_dev, "timed out waiting for resource destruction for %s\n",
			 V4L2_TYPE_IS_OUTPUT(type) ? "OUTPUT" : "CAPTURE");
		return -EINVAL;
	}

	return 0;
}

int virtio_video_reqbufs(struct file *file, void *priv,
			 struct v4l2_requestbuffers *rb)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct v4l2_m2m_ctx *m2m_ctx = stream->fh.m2m_ctx;
	struct vb2_queue *vq = v4l2_m2m_get_vq(m2m_ctx, rb->type);
	struct virtio_video_device *vvd = video_drvdata(file);

	if (rb->count == 0)
		virtio_video_queue_free(vvd->vv, stream, vq->type);

	return v4l2_m2m_reqbufs(file, m2m_ctx, rb);
}

int virtio_video_subscribe_event(struct v4l2_fh *fh,
				 const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	default:
		return -EINVAL;
	}
}

void virtio_video_queue_eos_event(struct virtio_video_stream *stream)
{
	static const struct v4l2_event eos_event = {
		.type = V4L2_EVENT_EOS
	};

	v4l2_event_queue_fh(&stream->fh, &eos_event);
}

void virtio_video_queue_res_chg_event(struct virtio_video_stream *stream)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
			V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	v4l2_event_queue_fh(&stream->fh, &ev_src_ch);
}

void virtio_video_mark_drain_complete(struct virtio_video_stream *stream,
				      struct vb2_v4l2_buffer *v4l2_vb)
{
	struct vb2_buffer *vb2_buf;

	v4l2_vb->flags |= V4L2_BUF_FLAG_LAST;

	vb2_buf = &v4l2_vb->vb2_buf;
	vb2_buf->planes[0].bytesused = 0;

	v4l2_m2m_buf_done(v4l2_vb, VB2_BUF_STATE_DONE);
	stream->state = STREAM_STATE_STOPPED;
}

void virtio_video_buf_done(struct virtio_video_buffer *virtio_vb,
			   uint32_t flags, uint64_t timestamp, uint32_t size)
{
	int i;
	enum vb2_buffer_state done_state = VB2_BUF_STATE_DONE;
	struct vb2_v4l2_buffer *v4l2_vb = &virtio_vb->v4l2_m2m_vb.vb;
	struct vb2_buffer *vb = &v4l2_vb->vb2_buf;
	struct vb2_queue *vb2_queue = vb->vb2_queue;
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb2_queue);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format_info *p_info;

	virtio_vb->queued = false;

	if (flags & VIRTIO_VIDEO_BUFFER_FLAG_ERR)
		done_state = VB2_BUF_STATE_ERROR;

	if (flags & VIRTIO_VIDEO_BUFFER_FLAG_IFRAME)
		v4l2_vb->flags |= V4L2_BUF_FLAG_KEYFRAME;

	if (flags & VIRTIO_VIDEO_BUFFER_FLAG_BFRAME)
		v4l2_vb->flags |= V4L2_BUF_FLAG_BFRAME;

	if (flags & VIRTIO_VIDEO_BUFFER_FLAG_PFRAME)
		v4l2_vb->flags |= V4L2_BUF_FLAG_PFRAME;

	if (flags & VIRTIO_VIDEO_BUFFER_FLAG_EOS) {
		v4l2_vb->flags |= V4L2_BUF_FLAG_LAST;
		stream->state = STREAM_STATE_STOPPED;
		virtio_video_queue_eos_event(stream);
	}

	/*
	 * If the host notifies an error or EOS with a buffer flag,
	 * the driver must set |bytesused| to 0.
	 *
	 * TODO(b/151810591): Though crosvm virtio-video device returns an
	 * empty buffer with EOS flag, the currecnt virtio-video protocol
	 * (v3 RFC) doesn't provides a way of knowing whether an EOS buffer
	 * is empty or not.
	 * So, we are assuming that EOS buffer is always empty. Once the
	 * protocol is updated, we should update this implementation based
	 * on the wrong assumption.
	 */
	if ((flags & VIRTIO_VIDEO_BUFFER_FLAG_ERR) ||
	    (flags & VIRTIO_VIDEO_BUFFER_FLAG_EOS)) {
		vb->planes[0].bytesused = 0;
		v4l2_m2m_buf_done(v4l2_vb, done_state);
		return;
	}

	if (!V4L2_TYPE_IS_OUTPUT(vb2_queue->type)) {
		switch (vvd->type) {
		case VIRTIO_VIDEO_DEVICE_ENCODER:
			vb->planes[0].bytesused = size;
			break;
		case VIRTIO_VIDEO_DEVICE_DECODER:
			p_info = &stream->out_info;
			for (i = 0; i < p_info->num_planes; i++)
				vb->planes[i].bytesused =
					p_info->plane_format[i].plane_size;
			break;
		}

		vb->timestamp = timestamp;
	}

	v4l2_m2m_buf_done(v4l2_vb, done_state);
}

static void virtio_video_worker(struct work_struct *work)
{
	unsigned int i;
	int ret;
	struct vb2_buffer *vb2_buf;
	struct vb2_v4l2_buffer *src_vb, *dst_vb;
	struct virtio_video_buffer *virtio_vb;
	struct virtio_video_stream *stream = work2stream(work);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct vb2_queue *src_vq =
		v4l2_m2m_get_vq(stream->fh.m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	struct vb2_queue *dst_vq =
		v4l2_m2m_get_vq(stream->fh.m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	struct virtio_video *vv = vvd->vv;
	uint32_t data_size[VB2_MAX_PLANES] = {0};

	mutex_lock(dst_vq->lock);
	for (;;) {
		dst_vb = v4l2_m2m_next_dst_buf(stream->fh.m2m_ctx);
		if (dst_vb == NULL)
			break;

		vb2_buf = &dst_vb->vb2_buf;
		virtio_vb = to_virtio_vb(vb2_buf);

		for (i = 0; i < vb2_buf->num_planes; ++i)
			data_size[i] = vb2_buf->planes[i].bytesused;

		ret = virtio_video_cmd_resource_queue
			(vv, stream->stream_id, virtio_vb, data_size,
			 vb2_buf->num_planes, VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);
		if (ret) {
			v4l2_err(&vv->v4l2_dev,
				  "failed to queue a dst buffer\n");
			v4l2_m2m_job_finish(vvd->m2m_dev, stream->fh.m2m_ctx);
			mutex_unlock(dst_vq->lock);
			return;
		}

		virtio_vb->queued = true;
		stream->dst_cleared = false;
		dst_vb = v4l2_m2m_dst_buf_remove(stream->fh.m2m_ctx);
	}
	mutex_unlock(dst_vq->lock);

	mutex_lock(src_vq->lock);
	for (;;) {
		if (stream->state == STREAM_STATE_DRAIN)
			break;

		src_vb = v4l2_m2m_next_src_buf(stream->fh.m2m_ctx);
		if (src_vb == NULL)
			break;

		vb2_buf = &src_vb->vb2_buf;
		virtio_vb = to_virtio_vb(vb2_buf);

		for (i = 0; i < vb2_buf->num_planes; ++i)
			data_size[i] = vb2_buf->planes[i].bytesused;

		ret = virtio_video_cmd_resource_queue
			(vv, stream->stream_id, virtio_vb, data_size,
			 vb2_buf->num_planes, VIRTIO_VIDEO_QUEUE_TYPE_INPUT);
		if (ret) {
			v4l2_err(&vv->v4l2_dev,
				  "failed to queue an src buffer\n");
			v4l2_m2m_job_finish(vvd->m2m_dev, stream->fh.m2m_ctx);
			mutex_unlock(src_vq->lock);
			return;
		}

		virtio_vb->queued = true;
		stream->src_cleared = false;
		src_vb = v4l2_m2m_src_buf_remove(stream->fh.m2m_ctx);
	}
	mutex_unlock(src_vq->lock);

	v4l2_m2m_job_finish(vvd->m2m_dev, stream->fh.m2m_ctx);
}

static int virtio_video_device_open(struct file *file)
{
	int ret;
	uint32_t stream_id;
	char name[TASK_COMM_LEN];
	struct virtio_video_stream *stream;
	struct video_format *default_fmt;
	enum virtio_video_format format;
	struct video_device *video_dev = video_devdata(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;

	switch (vvd->type) {
	case VIRTIO_VIDEO_DEVICE_ENCODER:
		default_fmt = list_first_entry_or_null(&vvd->output_fmt_list,
						       struct video_format,
						       formats_list_entry);
		break;
	case VIRTIO_VIDEO_DEVICE_DECODER:
		default_fmt = list_first_entry_or_null(&vvd->input_fmt_list,
						       struct video_format,
						       formats_list_entry);
		break;
	default:
		v4l2_err(&vv->v4l2_dev, "unsupported device type\n");
		return -EIO;
	}

	if (!default_fmt) {
		v4l2_err(&vv->v4l2_dev, "device failed to start\n");
		return -EIO;
	}

	stream = kzalloc(sizeof(*stream), GFP_KERNEL);
	if (!stream)
		return -ENOMEM;

	get_task_comm(name, current);
	format = virtio_video_v4l2_format_to_virtio(default_fmt->desc.format);
	virtio_video_stream_id_get(vv, stream, &stream_id);
	ret = virtio_video_cmd_stream_create(vv, stream_id, format, name);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to create stream\n");
		goto err_stream_create;
	}

	stream->video_dev = video_dev;
	stream->stream_id = stream_id;
	stream->state = STREAM_STATE_IDLE;
	stream->src_destroyed = true;
	stream->dst_destroyed = true;

	ret = virtio_video_cmd_get_params(vv, stream,
					  VIRTIO_VIDEO_QUEUE_TYPE_INPUT);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to get stream in params\n");
		goto err_init_ctrls;
	}

	ret = virtio_video_cmd_get_params(vv, stream,
					  VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to get stream out params\n");
		goto err_init_ctrls;
	}

	ret = virtio_video_cmd_get_control(vv, stream,
					   VIRTIO_VIDEO_CONTROL_PROFILE);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to get stream profile\n");
		goto err_init_ctrls;
	}

	ret = virtio_video_cmd_get_control(vv, stream,
					   VIRTIO_VIDEO_CONTROL_LEVEL);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to get stream level\n");
		goto err_init_ctrls;
	}

	ret = virtio_video_cmd_get_control(vv, stream,
					   VIRTIO_VIDEO_CONTROL_BITRATE);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to get stream bitrate\n");
		goto err_init_ctrls;
	}

	mutex_init(&stream->vq_mutex);
	INIT_WORK(&stream->work, virtio_video_worker);
	v4l2_fh_init(&stream->fh, video_dev);
	stream->fh.ctrl_handler = &stream->ctrl_handler;

	switch (vvd->type) {
	case VIRTIO_VIDEO_DEVICE_ENCODER:
		stream->fh.m2m_ctx =
			v4l2_m2m_ctx_init(vvd->m2m_dev, stream,
					  &virtio_video_enc_init_queues);
		break;
	case VIRTIO_VIDEO_DEVICE_DECODER:
		stream->fh.m2m_ctx =
			v4l2_m2m_ctx_init(vvd->m2m_dev, stream,
					  &virtio_video_dec_init_queues);
		break;
	default:
		v4l2_err(&vv->v4l2_dev, "unsupported device type\n");
		goto err_stream_create;
	}

	v4l2_m2m_set_src_buffered(stream->fh.m2m_ctx, true);
	v4l2_m2m_set_dst_buffered(stream->fh.m2m_ctx, true);
	file->private_data = &stream->fh;
	v4l2_fh_add(&stream->fh);

	switch (vvd->type) {
	case VIRTIO_VIDEO_DEVICE_ENCODER:
		ret = virtio_video_enc_init_ctrls(stream);
		break;
	case VIRTIO_VIDEO_DEVICE_DECODER:
		ret = virtio_video_dec_init_ctrls(stream);
		break;
	default:
		ret = 0;
		break;
	}

	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to init controls\n");
		goto err_init_ctrls;
	}
	return 0;

err_init_ctrls:
	v4l2_fh_del(&stream->fh);
	v4l2_fh_exit(&stream->fh);
	mutex_lock(video_dev->lock);
	v4l2_m2m_ctx_release(stream->fh.m2m_ctx);
	mutex_unlock(video_dev->lock);
err_stream_create:
	virtio_video_stream_id_put(vv, stream_id);
	kfree(stream);

	return ret;
}

static int virtio_video_device_release(struct file *file)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct video_device *video_dev = video_devdata(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;

	v4l2_fh_del(&stream->fh);
	v4l2_fh_exit(&stream->fh);
	mutex_lock(video_dev->lock);
	v4l2_m2m_ctx_release(stream->fh.m2m_ctx);
	mutex_unlock(video_dev->lock);

	virtio_video_cmd_stream_destroy(vv, stream->stream_id);
	virtio_video_stream_id_put(vv, stream->stream_id);

	v4l2_ctrl_handler_free(&stream->ctrl_handler);
	kfree(stream);

	return 0;
}

static const struct v4l2_file_operations virtio_video_device_fops = {
	.owner		= THIS_MODULE,
	.open		= virtio_video_device_open,
	.release	= virtio_video_device_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static void virtio_video_device_run(void *priv)
{
	struct virtio_video_stream *stream = priv;
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);

	queue_work(vvd->workqueue, &stream->work);
}

static int virtio_video_device_job_ready(void *priv)
{
	struct virtio_video_stream *stream = priv;

	if (stream->state == STREAM_STATE_STOPPED)
		return 0;

	if (v4l2_m2m_num_src_bufs_ready(stream->fh.m2m_ctx) > 0 ||
	    v4l2_m2m_num_dst_bufs_ready(stream->fh.m2m_ctx) > 0)
		return 1;

	return 0;
}

static void virtio_video_device_job_abort(void *priv)
{
	struct virtio_video_stream *stream = priv;
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);

	v4l2_m2m_job_finish(vvd->m2m_dev, stream->fh.m2m_ctx);
}

static const struct v4l2_m2m_ops virtio_video_device_m2m_ops = {
	.device_run	= virtio_video_device_run,
	.job_ready	= virtio_video_device_job_ready,
	.job_abort	= virtio_video_device_job_abort,
};

static int virtio_video_device_register(struct virtio_video_device *vvd)
{
	int ret = 0;
	struct video_device *vd = NULL;
	struct virtio_video *vv = NULL;

	if (!vvd)
		return -EINVAL;

	vd = &vvd->video_dev;
	vv = vvd->vv;

	switch (vvd->type) {
	case VIRTIO_VIDEO_DEVICE_ENCODER:
		ret = virtio_video_enc_init(vd);
		break;
	case VIRTIO_VIDEO_DEVICE_DECODER:
		ret = virtio_video_dec_init(vd);
		break;
	default:
		v4l2_err(&vv->v4l2_dev, "unknown device type\n");
		return -EINVAL;
	}

	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to init device\n");
		return ret;
	}

	ret = video_register_device(vd, VFL_TYPE_VIDEO, 0);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to register video device\n");
		return ret;
	}

	vvd->workqueue = alloc_ordered_workqueue(vd->name,
						 WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!vvd->workqueue) {
		v4l2_err(&vv->v4l2_dev, "failed to create a workqueue");
		video_unregister_device(vd);
		return -ENOMEM;
	}

	list_add(&vvd->devices_list_entry, &vv->devices_list);
	v4l2_info(&vv->v4l2_dev, "Device '%s' registered as /dev/video%d\n",
		  vd->name, vd->num);

	return 0;
}

static void virtio_video_device_unregister(struct virtio_video_device *vvd)
{
	if (!vvd)
		return;

	list_del(&vvd->devices_list_entry);
	flush_workqueue(vvd->workqueue);
	destroy_workqueue(vvd->workqueue);
	video_unregister_device(&vvd->video_dev);
}

static struct virtio_video_device *
virtio_video_device_create(struct virtio_video *vv)
{
	struct device *dev = NULL;
	struct video_device *vd = NULL;
	struct v4l2_m2m_dev *m2m_dev = NULL;
	struct virtio_video_device *vvd = NULL;

	if (!vv)
		return ERR_PTR(-EINVAL);

	dev = &vv->vdev->dev;

	vvd = devm_kzalloc(dev, sizeof(*vvd), GFP_KERNEL);
	if (!vvd)
		return ERR_PTR(-ENOMEM);

	m2m_dev = v4l2_m2m_init(&virtio_video_device_m2m_ops);
	if (IS_ERR(m2m_dev)) {
		v4l2_err(&vv->v4l2_dev, "failed to init m2m device\n");
		goto err;
	}

	vvd->vv = vv;
	vvd->m2m_dev = m2m_dev;
	mutex_init(&vvd->video_dev_mutex);
	vd = &vvd->video_dev;
	vd->lock = &vvd->video_dev_mutex;
	vd->v4l2_dev = &vv->v4l2_dev;
	vd->vfl_dir = VFL_DIR_M2M;
	vd->ioctl_ops = NULL;
	vd->fops = &virtio_video_device_fops;
	vd->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M_MPLANE;
	vd->release = video_device_release_empty;

	/* Use the selection API instead */
	v4l2_disable_ioctl(vd, VIDIOC_CROPCAP);
	v4l2_disable_ioctl(vd, VIDIOC_G_CROP);
	v4l2_disable_ioctl(vd, VIDIOC_S_CROP);

	video_set_drvdata(vd, vvd);

	INIT_LIST_HEAD(&vvd->input_fmt_list);
	INIT_LIST_HEAD(&vvd->output_fmt_list);
	INIT_LIST_HEAD(&vvd->controls_fmt_list);

	vvd->num_output_fmts = 0;
	vvd->num_input_fmts = 0;

	switch (vv->vdev->id.device) {
	case VIRTIO_ID_VIDEO_ENC:
		vvd->type = VIRTIO_VIDEO_DEVICE_ENCODER;
		break;
	case VIRTIO_ID_VIDEO_DEC:
	default:
		vvd->type = VIRTIO_VIDEO_DEVICE_DECODER;
		break;
	}

	return vvd;

err:
	devm_kfree(dev, vvd);

	return ERR_CAST(m2m_dev);
}

static void virtio_video_device_destroy(struct virtio_video_device *vvd)
{
	if (!vvd)
		return;

	v4l2_m2m_release(vvd->m2m_dev);
	devm_kfree(&vvd->vv->vdev->dev, vvd);
}

int virtio_video_device_init(struct virtio_video *vv,
			     void *input_buf, void *output_buf)
{
	int ret = 0;
	struct virtio_video_device *vvd = NULL;

	if (!vv || !input_buf || !output_buf)
		return -EINVAL;


	vvd = virtio_video_device_create(vv);
	if (IS_ERR(vvd)) {
		v4l2_err(&vv->v4l2_dev,
			 "failed to create virtio video device\n");
		ret = PTR_ERR(vvd);
		goto failed;
	}

	ret = virtio_video_parse_virtio_capability(vvd, input_buf, output_buf);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to parse a function\n");
		virtio_video_device_destroy(vvd);
		ret = -EINVAL;
		goto failed;
	}

	ret = virtio_video_parse_virtio_control(vvd);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to query controls\n");
		virtio_video_clean_capability(vvd);
		virtio_video_device_destroy(vvd);
		goto failed;
	}

	ret = virtio_video_device_register(vvd);
	if (ret) {
		v4l2_err(&vv->v4l2_dev,
			 "failed to init virtio video device\n");
		virtio_video_clean_control(vvd);
		virtio_video_clean_capability(vvd);
		virtio_video_device_destroy(vvd);
		goto failed;
	}

	return 0;

failed:
	virtio_video_device_deinit(vv);

	return ret;
}

void virtio_video_device_deinit(struct virtio_video *vv)
{
	struct virtio_video_device *vvd = NULL, *tmp = NULL;

	list_for_each_entry_safe(vvd, tmp, &vv->devices_list,
				 devices_list_entry) {
		virtio_video_device_unregister(vvd);
		virtio_video_clean_control(vvd);
		virtio_video_clean_capability(vvd);
		virtio_video_device_destroy(vvd);
	}
}
