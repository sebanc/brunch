// SPDX-License-Identifier: GPL-2.0+
/* Decoder for virtio video device.
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

#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>

#include "virtio_video.h"
#include "virtio_video_dec.h"

static void virtio_video_dec_buf_queue(struct vb2_buffer *vb)
{
	int i, ret;
	struct vb2_buffer *src_buf;
	struct vb2_v4l2_buffer *src_vb;
	struct virtio_video_buffer *virtio_vb;
	uint32_t data_size[VB2_MAX_PLANES] = {0};
	struct vb2_v4l2_buffer *v4l2_vb = to_vb2_v4l2_buffer(vb);
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;

	v4l2_m2m_buf_queue(stream->fh.m2m_ctx, v4l2_vb);

	if ((stream->state != STREAM_STATE_INIT) ||
	    !V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
		return;

	src_vb = v4l2_m2m_next_src_buf(stream->fh.m2m_ctx);
	if (!src_vb) {
		v4l2_err(&vv->v4l2_dev, "no src buf during initialization\n");
		return;
	}

	src_buf = &src_vb->vb2_buf;
	for (i = 0; i < src_buf->num_planes; ++i)
		data_size[i] = src_buf->planes[i].bytesused;

	virtio_vb = to_virtio_vb(src_buf);

	ret = virtio_video_cmd_resource_queue(vv, stream->stream_id,
					      virtio_vb, data_size,
					      src_buf->num_planes,
					      VIRTIO_VIDEO_QUEUE_TYPE_INPUT);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to queue an src buffer\n");
		return;
	}

	virtio_vb->queued = true;
	stream->src_cleared = false;
	src_vb = v4l2_m2m_src_buf_remove(stream->fh.m2m_ctx);
}

static int virtio_video_dec_start_streaming(struct vb2_queue *vq,
					    unsigned int count)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vq);

	if (!V4L2_TYPE_IS_OUTPUT(vq->type) &&
	    stream->state >= STREAM_STATE_INIT)
		stream->state = STREAM_STATE_RUNNING;

	return 0;
}

static void virtio_video_dec_stop_streaming(struct vb2_queue *vq)
{
	int ret, queue_type;
	bool *cleared;
	bool is_v4l2_output = V4L2_TYPE_IS_OUTPUT(vq->type);
	struct virtio_video_stream *stream = vb2_get_drv_priv(vq);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct vb2_v4l2_buffer *v4l2_vb;

	if (is_v4l2_output) {
		cleared = &stream->src_cleared;
		queue_type = VIRTIO_VIDEO_QUEUE_TYPE_INPUT;
	} else {
		cleared = &stream->dst_cleared;
		queue_type = VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT;
	}

	ret = virtio_video_cmd_queue_clear(vv, stream, queue_type);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to clear queue\n");
		return;
	}

	ret = wait_event_timeout(vv->wq, *cleared, 5 * HZ);
	if (ret == 0) {
		v4l2_err(&vv->v4l2_dev, "timed out waiting for queue clear\n");
		return;
	}

	for (;;) {
		if (is_v4l2_output)
			v4l2_vb = v4l2_m2m_src_buf_remove(stream->fh.m2m_ctx);
		else
			v4l2_vb = v4l2_m2m_dst_buf_remove(stream->fh.m2m_ctx);
		if (!v4l2_vb)
			break;
		v4l2_m2m_buf_done(v4l2_vb, VB2_BUF_STATE_ERROR);
	}
}

static const struct vb2_ops virtio_video_dec_qops = {
	.queue_setup	 = virtio_video_queue_setup,
	.buf_init	 = virtio_video_buf_init,
	.buf_prepare	 = virtio_video_buf_prepare,
	.buf_cleanup	 = virtio_video_buf_cleanup,
	.buf_queue	 = virtio_video_dec_buf_queue,
	.start_streaming = virtio_video_dec_start_streaming,
	.stop_streaming  = virtio_video_dec_stop_streaming,
	.wait_prepare	 = vb2_ops_wait_prepare,
	.wait_finish	 = vb2_ops_wait_finish,
};

static int virtio_video_dec_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct virtio_video_stream *stream = ctrl2stream(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (stream->state >= STREAM_STATE_METADATA)
			ctrl->val = stream->out_info.min_buffers;
		else
			ctrl->val = 0;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops virtio_video_dec_ctrl_ops = {
	.g_volatile_ctrl	= virtio_video_dec_g_volatile_ctrl,
};

int virtio_video_dec_init_ctrls(struct virtio_video_stream *stream)
{
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&stream->ctrl_handler, 1);

	ctrl = v4l2_ctrl_new_std(&stream->ctrl_handler,
				&virtio_video_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				MIN_BUFS_MIN, MIN_BUFS_MAX, MIN_BUFS_STEP,
				MIN_BUFS_DEF);

	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (stream->ctrl_handler.error) {
		int err = stream->ctrl_handler.error;

		v4l2_ctrl_handler_free(&stream->ctrl_handler);
		return err;
	}

	v4l2_ctrl_handler_setup(&stream->ctrl_handler);

	return 0;
}

int virtio_video_dec_init_queues(void *priv, struct vb2_queue *src_vq,
				 struct vb2_queue *dst_vq)
{
	int ret;
	struct virtio_video_stream *stream = priv;
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct device *dev = vv->v4l2_dev.dev;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->drv_priv = stream;
	src_vq->buf_struct_size = sizeof(struct virtio_video_buffer);
	src_vq->ops = &virtio_video_dec_qops;
	src_vq->mem_ops = virtio_video_mem_ops(vv);
	src_vq->min_buffers_needed = stream->in_info.min_buffers;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &stream->vq_mutex;
	src_vq->gfp_flags = virtio_video_gfp_flags(vv);
	src_vq->dev = dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->drv_priv = stream;
	dst_vq->buf_struct_size = sizeof(struct virtio_video_buffer);
	dst_vq->ops = &virtio_video_dec_qops;
	dst_vq->mem_ops = virtio_video_mem_ops(vv);
	dst_vq->min_buffers_needed = stream->out_info.min_buffers;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &stream->vq_mutex;
	dst_vq->gfp_flags = virtio_video_gfp_flags(vv);
	dst_vq->dev = dev;

	return vb2_queue_init(dst_vq);
}

static int virtio_video_try_decoder_cmd(struct file *file, void *fh,
					struct v4l2_decoder_cmd *cmd)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;

	if (stream->state == STREAM_STATE_DRAIN)
		return -EBUSY;

	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
	case V4L2_DEC_CMD_START:
		if (cmd->flags != 0) {
			v4l2_err(&vv->v4l2_dev, "flags=%u are not supported",
				 cmd->flags);
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int virtio_video_decoder_cmd(struct file *file, void *fh,
				    struct v4l2_decoder_cmd *cmd)
{
	int ret;
	struct vb2_queue *src_vq, *dst_vq;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;
	int current_state;

	ret = virtio_video_try_decoder_cmd(file, fh, cmd);
	if (ret < 0)
		return ret;

	dst_vq = v4l2_m2m_get_vq(stream->fh.m2m_ctx,
				 V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

	switch (cmd->cmd) {
	case V4L2_DEC_CMD_START:
		vb2_clear_last_buffer_dequeued(dst_vq);
		if (stream->state == STREAM_STATE_STOPPED) {
			stream->state = STREAM_STATE_RUNNING;
		} else {
			v4l2_warn(&vv->v4l2_dev, "state(%d) is not STOPPED\n",
				  stream->state);
		}
		break;
	case V4L2_DEC_CMD_STOP:
		src_vq = v4l2_m2m_get_vq(stream->fh.m2m_ctx,
					 V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

		if (!vb2_is_streaming(src_vq)) {
			v4l2_dbg(1, vv->debug,
				 &vv->v4l2_dev, "output is not streaming\n");
			return 0;
		}

		if (!vb2_is_streaming(dst_vq)) {
			v4l2_dbg(1, vv->debug,
				 &vv->v4l2_dev, "capture is not streaming\n");
			return 0;
		}

		current_state = stream->state;
		stream->state = STREAM_STATE_DRAIN;
		ret = virtio_video_cmd_stream_drain(vv, stream->stream_id);
		if (ret) {
			stream->state = current_state;
			v4l2_err(&vv->v4l2_dev, "failed to drain stream\n");
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int virtio_video_dec_enum_fmt_vid_cap(struct file *file, void *fh,
					     struct v4l2_fmtdesc *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format_info *info = NULL;
	struct video_format *fmt = NULL;
	unsigned long input_mask = 0;
	int idx = 0, bit_num = 0;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	if (f->index >= vvd->num_output_fmts)
		return -EINVAL;

	info = &stream->in_info;
	list_for_each_entry(fmt, &vvd->input_fmt_list, formats_list_entry) {
		if (info->fourcc_format == fmt->desc.format) {
			input_mask = fmt->desc.mask;
			break;
		}
	}

	if (input_mask == 0)
		return -EINVAL;

	list_for_each_entry(fmt, &vvd->output_fmt_list, formats_list_entry) {
		if (test_bit(bit_num, &input_mask)) {
			if (f->index == idx) {
				f->pixelformat = fmt->desc.format;
				return 0;
			}
			idx++;
		}
		bit_num++;
	}
	return -EINVAL;
}


static int virtio_video_dec_enum_fmt_vid_out(struct file *file, void *fh,
					     struct v4l2_fmtdesc *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format *fmt = NULL;
	int idx = 0;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	if (f->index >= vvd->num_input_fmts)
		return -EINVAL;

	list_for_each_entry(fmt, &vvd->input_fmt_list, formats_list_entry) {
		if (f->index == idx) {
			f->pixelformat = fmt->desc.format;
			return 0;
		}
		idx++;
	}
	return -EINVAL;
}

static int virtio_video_dec_s_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	int ret;
	struct virtio_video_stream *stream = file2stream(file);

	ret = virtio_video_s_fmt(file, fh, f);
	if (ret)
		return ret;

	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		if (stream->state == STREAM_STATE_IDLE)
			stream->state = STREAM_STATE_INIT;
	}

	return 0;
}

static const struct v4l2_ioctl_ops virtio_video_dec_ioctl_ops = {
	.vidioc_querycap	= virtio_video_querycap,

	.vidioc_enum_fmt_vid_cap = virtio_video_dec_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_cap	= virtio_video_dec_s_fmt,

	.vidioc_g_fmt_vid_cap_mplane	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane	= virtio_video_dec_s_fmt,

	.vidioc_enum_fmt_vid_out = virtio_video_dec_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_out	= virtio_video_dec_s_fmt,

	.vidioc_g_fmt_vid_out_mplane	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_out_mplane	= virtio_video_dec_s_fmt,

	.vidioc_g_selection = virtio_video_g_selection,
	.vidioc_s_selection = virtio_video_s_selection,

	.vidioc_try_decoder_cmd	= virtio_video_try_decoder_cmd,
	.vidioc_decoder_cmd	= virtio_video_decoder_cmd,
	.vidioc_enum_frameintervals = virtio_video_enum_framemintervals,
	.vidioc_enum_framesizes = virtio_video_enum_framesizes,

	.vidioc_reqbufs		= virtio_video_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf		= v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf		= v4l2_m2m_ioctl_dqbuf,
	.vidioc_prepare_buf	= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_create_bufs	= v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,

	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,

	.vidioc_subscribe_event = virtio_video_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

int virtio_video_dec_init(struct video_device *vd)
{
	vd->ioctl_ops = &virtio_video_dec_ioctl_ops;
	strscpy(vd->name, "stateful-decoder", sizeof(vd->name));

	return 0;
}
