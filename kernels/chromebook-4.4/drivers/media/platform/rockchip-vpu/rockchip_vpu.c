/*
 * Rockchip VPU codec driver
 *
 * Copyright (C) 2014 Google, Inc.
 *	Tomasz Figa <tfiga@chromium.org>
 *
 * Based on s5p-mfc driver by Samsung Electronics Co., Ltd.
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "rockchip_vpu_common.h"

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <media/v4l2-event.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include "rockchip_vpu_dec.h"
#include "rockchip_vpu_enc.h"
#include "rockchip_vpu_hw.h"

int rockchip_vpu_debug;
module_param_named(debug, rockchip_vpu_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug,
		 "Debug level - higher value produces more verbose messages");

/*
 * DMA coherent helpers.
 */

int rockchip_vpu_aux_buf_alloc(struct rockchip_vpu_dev *vpu,
			    struct rockchip_vpu_aux_buf *buf, size_t size)
{
	buf->cpu = dma_alloc_coherent(vpu->dev, size, &buf->dma, GFP_KERNEL);
	if (!buf->cpu)
		return -ENOMEM;

	buf->size = size;
	return 0;
}

void rockchip_vpu_aux_buf_free(struct rockchip_vpu_dev *vpu,
			     struct rockchip_vpu_aux_buf *buf)
{
	dma_free_coherent(vpu->dev, buf->size, buf->cpu, buf->dma);

	buf->cpu = NULL;
	buf->dma = 0;
	buf->size = 0;
}

/*
 * Context scheduling.
 */

static void rockchip_vpu_prepare_run(struct rockchip_vpu_ctx *ctx)
{
	if (ctx->run_ops->prepare_run)
		ctx->run_ops->prepare_run(ctx);
}

static void __rockchip_vpu_dequeue_run_locked(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_buf *src, *dst;

	/*
	 * Since ctx was dequeued from ready_ctxs list, we know that it has
	 * at least one buffer in each queue.
	 */
	src = list_first_entry(&ctx->src_queue, struct rockchip_vpu_buf, list);
	dst = list_first_entry(&ctx->dst_queue, struct rockchip_vpu_buf, list);

	list_del_init(&src->list);
	list_del_init(&dst->list);

	ctx->run.src = src;
	ctx->run.dst = dst;
}

static bool rockchip_vpu_ctx_is_flush(struct rockchip_vpu_ctx *ctx)
{
	return ctx->run.src == &ctx->flush_buf;
}

static void rockchip_vpu_flush_done(struct rockchip_vpu_ctx *ctx)
{
	struct vb2_v4l2_buffer *vb2_dst = &ctx->run.dst->b;
	static const struct v4l2_event event = {
		.type = V4L2_EVENT_EOS,
	};
	int i;

	vb2_dst->flags |= V4L2_BUF_FLAG_LAST;
	for (i = 0; i < vb2_dst->vb2_buf.num_planes; i++)
		vb2_set_plane_payload(&vb2_dst->vb2_buf, i, 0);
	vb2_buffer_done(&vb2_dst->vb2_buf, VB2_BUF_STATE_DONE);

	v4l2_event_queue_fh(&ctx->fh, &event);
}

static void rockchip_vpu_try_run(struct rockchip_vpu_dev *dev)
{
	struct rockchip_vpu_ctx *ctx = NULL;
	unsigned long flags;

	vpu_debug_enter();

	spin_lock_irqsave(&dev->irqlock, flags);

	if (list_empty(&dev->ready_ctxs) ||
	    test_bit(VPU_SUSPENDED, &dev->state))
		/* Nothing to do. */
		goto out;

	if (test_and_set_bit(VPU_RUNNING, &dev->state))
		/*
		* The hardware is already running. We will pick another
		* run after we get the notification in rockchip_vpu_run_done().
		*/
		goto out;

	ctx = list_entry(dev->ready_ctxs.next, struct rockchip_vpu_ctx, list);

	list_del_init(&ctx->list);
	__rockchip_vpu_dequeue_run_locked(ctx);

	dev->current_ctx = ctx;

	if (rockchip_vpu_ctx_is_flush(ctx))
		ctx->stopped = true;
	else
		dev->was_decoding = !rockchip_vpu_ctx_is_encoder(ctx);

out:
	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (ctx) {
		if (rockchip_vpu_ctx_is_flush(ctx)) {
			/* Flush is an entirely software operation. */
			rockchip_vpu_run_done(ctx, VB2_BUF_STATE_DONE);
		} else {
			rockchip_vpu_prepare_run(ctx);
			rockchip_vpu_run(ctx);
		}
	}

	vpu_debug_leave();
}

static void __rockchip_vpu_try_context_locked(struct rockchip_vpu_dev *dev,
					    struct rockchip_vpu_ctx *ctx)
{
	if (!list_empty(&ctx->list))
		/* Context already queued. */
		return;

	if (ctx->stopped)
		/* No processing until we are started again. */
		return;

	if (!list_empty(&ctx->dst_queue) && !list_empty(&ctx->src_queue))
		list_add_tail(&ctx->list, &dev->ready_ctxs);
}

void rockchip_vpu_run_done(struct rockchip_vpu_ctx *ctx,
			 enum vb2_buffer_state result)
{
	struct rockchip_vpu_dev *dev = ctx->dev;
	unsigned long flags;

	vpu_debug_enter();

	if (rockchip_vpu_ctx_is_flush(ctx)) {
		rockchip_vpu_flush_done(ctx);
	} else {
		struct vb2_v4l2_buffer *vb2_src = &ctx->run.src->b;
		struct vb2_v4l2_buffer *vb2_dst = &ctx->run.dst->b;

		if (ctx->run_ops->run_done)
			ctx->run_ops->run_done(ctx, result);

		vb2_dst->timestamp = vb2_src->timestamp;
		vb2_buffer_done(&vb2_src->vb2_buf, result);
		vb2_buffer_done(&vb2_dst->vb2_buf, result);
	}

	dev->current_ctx = NULL;
	wake_up_all(&dev->run_wq);

	spin_lock_irqsave(&dev->irqlock, flags);

	__rockchip_vpu_try_context_locked(dev, ctx);
	clear_bit(VPU_RUNNING, &dev->state);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	/* Try scheduling another run to see if we have anything left to do. */
	rockchip_vpu_try_run(dev);

	vpu_debug_leave();
}

void rockchip_vpu_try_context(struct rockchip_vpu_dev *dev,
			    struct rockchip_vpu_ctx *ctx)
{
	unsigned long flags;

	vpu_debug_enter();

	spin_lock_irqsave(&dev->irqlock, flags);

	__rockchip_vpu_try_context_locked(dev, ctx);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	rockchip_vpu_try_run(dev);

	vpu_debug_leave();
}

void write_header(u32 value, u32 *buffer, u32 offset, u32 len)
{
	u32 word = offset / 32;
	u32 bit = offset % 32;

	if (len + bit > 32) {
		u32 len1 = 32 - bit;
		u32 len2 = len + bit - 32;

		buffer[word] &= ~(((1 << len1) - 1) << bit);
		buffer[word] |= value << bit;

		value >>= (32 - bit);
		buffer[word + 1] &= ~((1 << len2) - 1);
		buffer[word + 1] |= value;
	} else {
		buffer[word] &= ~(((1 << len) - 1) << bit);
		buffer[word] |= value << bit;
	}
}

void rockchip_vpu_update_planes(const struct rockchip_vpu_fmt *fmt,
				struct v4l2_pix_format_mplane *pix_fmt_mp)
{
	size_t sizeimage_total = 0;
	int i;

	for (i = 0; i < fmt->num_cplanes; ++i) {
		pix_fmt_mp->plane_fmt[i].bytesperline =
			(pix_fmt_mp->width / fmt->h_subsampling[i])
			* fmt->depth[i] / 8;

		pix_fmt_mp->plane_fmt[i].sizeimage =
			pix_fmt_mp->plane_fmt[i].bytesperline
			* pix_fmt_mp->height / fmt->v_subsampling[i];

		sizeimage_total += pix_fmt_mp->plane_fmt[i].sizeimage;
	}

	if (fmt->num_mplanes == 1)
		pix_fmt_mp->plane_fmt[0].sizeimage = sizeimage_total;
}

/*
 * Control registration.
 */

#define IS_VPU_PRIV(x) ((V4L2_CTRL_ID2CLASS(x) == V4L2_CTRL_CLASS_MPEG) && \
			  V4L2_CTRL_DRIVER_PRIV(x))
#define IS_USER_PRIV(x) ((V4L2_CTRL_ID2CLASS(x) == V4L2_CTRL_CLASS_USER) && \
			  V4L2_CTRL_DRIVER_PRIV(x))

int rockchip_vpu_ctrls_setup(struct rockchip_vpu_ctx *ctx,
			   const struct v4l2_ctrl_ops *ctrl_ops,
			   struct rockchip_vpu_control *controls,
			   unsigned num_ctrls,
			   const char *const *(*get_menu)(u32))
{
	struct v4l2_ctrl_config cfg;
	int i;

	if (num_ctrls > ARRAY_SIZE(ctx->ctrls)) {
		vpu_err("context control array not large enough\n");
		return -ENOSPC;
	}

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, num_ctrls);
	if (ctx->ctrl_handler.error) {
		vpu_err("v4l2_ctrl_handler_init failed\n");
		return ctx->ctrl_handler.error;
	}

	for (i = 0; i < num_ctrls; i++) {
		if (IS_VPU_PRIV(controls[i].id)
		    || IS_USER_PRIV(controls[i].id)
		    || controls[i].type >= V4L2_CTRL_COMPOUND_TYPES) {
			memset(&cfg, 0, sizeof(struct v4l2_ctrl_config));

			cfg.ops = ctrl_ops;
			cfg.id = controls[i].id;
			cfg.min = controls[i].minimum;
			cfg.max = controls[i].maximum;
			cfg.max_stores = controls[i].max_stores;
			cfg.def = controls[i].default_value;
			cfg.name = controls[i].name;
			cfg.type = controls[i].type;
			cfg.elem_size = controls[i].elem_size;
			memcpy(cfg.dims, controls[i].dims, sizeof(cfg.dims));

			if (cfg.type == V4L2_CTRL_TYPE_MENU) {
				cfg.menu_skip_mask = cfg.menu_skip_mask;
				cfg.qmenu = get_menu(cfg.id);
			} else {
				cfg.step = controls[i].step;
			}

			ctx->ctrls[i] = v4l2_ctrl_new_custom(
				&ctx->ctrl_handler, &cfg, NULL);
		} else {
			if (controls[i].type == V4L2_CTRL_TYPE_MENU) {
				ctx->ctrls[i] =
				    v4l2_ctrl_new_std_menu
					(&ctx->ctrl_handler,
					 ctrl_ops,
					 controls[i].id,
					 controls[i].maximum,
					 controls[i].menu_skip_mask,
					 controls[i].
					 default_value);
			} else {
				ctx->ctrls[i] =
				    v4l2_ctrl_new_std(&ctx->ctrl_handler,
						      ctrl_ops,
						      controls[i].id,
						      controls[i].minimum,
						      controls[i].maximum,
						      controls[i].step,
						      controls[i].
						      default_value);
			}
		}

		if (ctx->ctrl_handler.error) {
			vpu_err("Adding control (%d) failed\n", i);
			return ctx->ctrl_handler.error;
		}

		if (controls[i].is_volatile && ctx->ctrls[i])
			ctx->ctrls[i]->flags |= V4L2_CTRL_FLAG_VOLATILE;
		if (controls[i].is_read_only && ctx->ctrls[i])
			ctx->ctrls[i]->flags |= V4L2_CTRL_FLAG_READ_ONLY;
		if (controls[i].can_store && ctx->ctrls[i])
			ctx->ctrls[i]->flags |= V4L2_CTRL_FLAG_CAN_STORE;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_handler);
	ctx->num_ctrls = num_ctrls;
	return 0;
}

void rockchip_vpu_ctrls_delete(struct rockchip_vpu_ctx *ctx)
{
	int i;

	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	for (i = 0; i < ctx->num_ctrls; i++)
		ctx->ctrls[i] = NULL;
}

/*
 * V4L2 file operations.
 */

static int rockchip_vpu_open(struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);
	struct rockchip_vpu_dev *dev = video_drvdata(filp);
	struct rockchip_vpu_ctx *ctx = NULL;
	struct vb2_queue *q;
	int ret = 0;

	/*
	 * We do not need any extra locking here, because we operate only
	 * on local data here, except reading few fields from dev, which
	 * do not change through device's lifetime (which is guaranteed by
	 * reference on module from open()) and V4L2 internal objects (such
	 * as vdev and ctx->fh), which have proper locking done in respective
	 * helper functions used here.
	 */

	vpu_debug_enter();

	/* Allocate memory for context */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto err_leave;
	}

	v4l2_fh_init(&ctx->fh, video_devdata(filp));
	filp->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	ctx->dev = dev;
	INIT_LIST_HEAD(&ctx->src_queue);
	INIT_LIST_HEAD(&ctx->dst_queue);
	INIT_LIST_HEAD(&ctx->list);
	INIT_LIST_HEAD(&ctx->flush_buf.list);

	if (vdev == dev->vfd_enc) {
		/* only for encoder */
		ret = rockchip_vpu_enc_init(ctx);
		if (ret) {
			vpu_err("Failed to initialize encoder context\n");
			goto err_fh_free;
		}
	} else if (vdev == dev->vfd_dec) {
		/* only for decoder */
		ret = rockchip_vpu_dec_init(ctx);
		if (ret) {
			vpu_err("Failed to initialize decoder context\n");
			goto err_fh_free;
		}
	} else {
		ret = -ENOENT;
		goto err_fh_free;
	}
	ctx->fh.ctrl_handler = &ctx->ctrl_handler;

	/* Init videobuf2 queue for CAPTURE */
	q = &ctx->vq_dst;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->drv_priv = &ctx->fh;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->lock = &dev->vpu_mutex;
	q->buf_struct_size = sizeof(struct rockchip_vpu_buf);

	if (vdev == dev->vfd_enc) {
		q->ops = rockchip_get_enc_queue_ops();
	} else if (vdev == dev->vfd_dec) {
		q->ops = rockchip_get_dec_queue_ops();
		q->use_dma_bidirectional = 1;
	}

	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(q);
	if (ret) {
		vpu_err("Failed to initialize videobuf2 queue(capture)\n");
		goto err_enc_dec_exit;
	}

	/* Init videobuf2 queue for OUTPUT */
	q = &ctx->vq_src;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	q->drv_priv = &ctx->fh;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->lock = &dev->vpu_mutex;
	q->buf_struct_size = sizeof(struct rockchip_vpu_buf);

	if (vdev == dev->vfd_enc)
		q->ops = rockchip_get_enc_queue_ops();
	else if (vdev == dev->vfd_dec)
		q->ops = rockchip_get_dec_queue_ops();

	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(q);
	if (ret) {
		vpu_err("Failed to initialize videobuf2 queue(output)\n");
		goto err_vq_dst_release;
	}

	vpu_debug_leave();

	return 0;

err_vq_dst_release:
	vb2_queue_release(&ctx->vq_dst);
err_enc_dec_exit:
	if (vdev == dev->vfd_enc)
		rockchip_vpu_enc_exit(ctx);
	else if (vdev == dev->vfd_dec)
		rockchip_vpu_dec_exit(ctx);
err_fh_free:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
err_leave:
	vpu_debug_leave();

	return ret;
}

static int rockchip_vpu_release(struct file *filp)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(filp->private_data);
	struct video_device *vdev = video_devdata(filp);
	struct rockchip_vpu_dev *dev = ctx->dev;

	/*
	 * No need for extra locking because this was the last reference
	 * to this file.
	 */

	vpu_debug_enter();

	/*
	 * vb2_queue_release() ensures that streaming is stopped, which
	 * in turn means that there are no frames still being processed
	 * by hardware.
	 */
	vb2_queue_release(&ctx->vq_src);
	vb2_queue_release(&ctx->vq_dst);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	if (vdev == dev->vfd_enc)
		rockchip_vpu_enc_exit(ctx);
	else if (vdev == dev->vfd_dec)
		rockchip_vpu_dec_exit(ctx);

	kfree(ctx);

	vpu_debug_leave();

	return 0;
}

static unsigned int rockchip_vpu_poll(struct file *filp,
				    struct poll_table_struct *wait)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(filp->private_data);
	struct vb2_queue *src_q, *dst_q;
	struct vb2_buffer *src_vb = NULL, *dst_vb = NULL;
	unsigned int rc = 0;
	unsigned long flags;

	vpu_debug_enter();

	src_q = &ctx->vq_src;
	dst_q = &ctx->vq_dst;

	/*
	 * There has to be at least one buffer queued on each queued_list, which
	 * means either in driver already or waiting for driver to claim it
	 * and start processing.
	 */
	if ((!vb2_is_streaming(src_q) || list_empty(&src_q->queued_list)) &&
	    (!vb2_is_streaming(dst_q) || list_empty(&dst_q->queued_list))) {
		vpu_debug(0, "src q streaming %d, dst q streaming %d, src list empty(%d), dst list empty(%d)\n",
				src_q->streaming, dst_q->streaming,
				list_empty(&src_q->queued_list),
				list_empty(&dst_q->queued_list));
		return POLLERR;
	}

	poll_wait(filp, &ctx->fh.wait, wait);
	poll_wait(filp, &src_q->done_wq, wait);
	poll_wait(filp, &dst_q->done_wq, wait);

	if (v4l2_event_pending(&ctx->fh))
		rc |= POLLPRI;

	spin_lock_irqsave(&src_q->done_lock, flags);

	if (!list_empty(&src_q->done_list))
		src_vb = list_first_entry(&src_q->done_list, struct vb2_buffer,
						done_entry);

	if (src_vb && (src_vb->state == VB2_BUF_STATE_DONE ||
			src_vb->state == VB2_BUF_STATE_ERROR))
		rc |= POLLOUT | POLLWRNORM;

	spin_unlock_irqrestore(&src_q->done_lock, flags);

	spin_lock_irqsave(&dst_q->done_lock, flags);

	if (!list_empty(&dst_q->done_list))
		dst_vb = list_first_entry(&dst_q->done_list, struct vb2_buffer,
						done_entry);

	if (dst_vb && (dst_vb->state == VB2_BUF_STATE_DONE ||
			dst_vb->state == VB2_BUF_STATE_ERROR))
		rc |= POLLIN | POLLRDNORM;

	spin_unlock_irqrestore(&dst_q->done_lock, flags);

	return rc;
}

static int rockchip_vpu_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct rockchip_vpu_ctx *ctx = fh_to_ctx(filp->private_data);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	int ret;

	vpu_debug_enter();

	if (offset < DST_QUEUE_OFF_BASE) {
		vpu_debug(4, "mmaping source\n");

		ret = vb2_mmap(&ctx->vq_src, vma);
	} else {		/* capture */
		vpu_debug(4, "mmaping destination\n");

		vma->vm_pgoff -= (DST_QUEUE_OFF_BASE >> PAGE_SHIFT);
		ret = vb2_mmap(&ctx->vq_dst, vma);
	}

	vpu_debug_leave();

	return ret;
}

static const struct v4l2_file_operations rockchip_vpu_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_vpu_open,
	.release = rockchip_vpu_release,
	.poll = rockchip_vpu_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = rockchip_vpu_mmap,
};

/*
 * Platform driver.
 */

/* Supported VPU variants. */
static const struct of_device_id of_rockchip_vpu_match[] = {
	{ .compatible = "rockchip,rk3399-vpu", .data = &rk3399_vpu_variant, },
	{ .compatible = "rockchip,rk3399-vdec", .data = &rk3399_vdec_variant, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_rockchip_vpu_match);

static int rockchip_vpu_video_device_register(struct rockchip_vpu_dev *vpu,
					      bool encoder)
{
	const struct of_device_id *match;
	struct video_device *vfd;
	int ret = 0;

	vpu_debug_enter();

	match = of_match_node(of_rockchip_vpu_match, vpu->dev->of_node);

	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&vpu->v4l2_dev, "Failed to allocate video device\n");
		return -ENOMEM;
	}

	vfd->fops = &rockchip_vpu_fops;
	vfd->release = video_device_release;
	vfd->lock = &vpu->vpu_mutex;
	vfd->v4l2_dev = &vpu->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;

	if (encoder) {
		vfd->ioctl_ops = rockchip_get_enc_v4l2_ioctl_ops();
		snprintf(vfd->name, sizeof(vfd->name), "%s-enc",
			 match->compatible);
		vpu->vfd_enc = vfd;
	} else {
		vfd->ioctl_ops = rockchip_get_dec_v4l2_ioctl_ops();
		snprintf(vfd->name, sizeof(vfd->name), "%s-dec",
			 match->compatible);
		vpu->vfd_dec = vfd;
	}

	video_set_drvdata(vfd, vpu);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, 0);
	if (ret) {
		v4l2_err(&vpu->v4l2_dev, "Failed to register video device\n");
		goto err_dev_reg;
	}

	v4l2_info(&vpu->v4l2_dev, "%s registered as video%d\n",
		  vfd->name, vfd->num);

	vpu_debug_leave();

	return 0;

err_dev_reg:
	video_device_release(vfd);
	vpu_debug_leave();

	return ret;
}

static int rockchip_vpu_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct rockchip_vpu_dev *vpu = NULL;
	DEFINE_DMA_ATTRS(attrs_novm);
	DEFINE_DMA_ATTRS(attrs_nohugepage);
	int ret = 0;

	vpu_debug_enter();

	vpu = devm_kzalloc(&pdev->dev, sizeof(*vpu), GFP_KERNEL);
	if (!vpu)
		return -ENOMEM;

	vpu->dev = &pdev->dev;
	vpu->pdev = pdev;
	mutex_init(&vpu->vpu_mutex);
	spin_lock_init(&vpu->irqlock);
	INIT_LIST_HEAD(&vpu->ready_ctxs);
	init_waitqueue_head(&vpu->run_wq);

	match = of_match_node(of_rockchip_vpu_match, pdev->dev.of_node);
	vpu->variant = match->data;

	INIT_DELAYED_WORK(&vpu->watchdog_work, rockchip_vpu_watchdog);

	ret = vpu->variant->hw_probe(vpu);
	if (ret) {
		dev_err(&pdev->dev, "Failed to probe VPU hardware\n");
		goto err_hw_probe;
	}

	vpu->variant->clk_enable(vpu);

	pm_runtime_set_autosuspend_delay(vpu->dev, 100);
	pm_runtime_use_autosuspend(vpu->dev);
	pm_runtime_enable(vpu->dev);

	/*
	 * We'll do mostly sequential access, so sacrifice TLB efficiency for
	 * faster allocation.
	 */
	dma_set_attr(DMA_ATTR_ALLOC_SINGLE_PAGES, &attrs_novm);

	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs_novm);
	vpu->alloc_ctx = vb2_dma_contig_init_ctx_attrs(&pdev->dev,
								&attrs_novm);
	if (IS_ERR(vpu->alloc_ctx)) {
		ret = PTR_ERR(vpu->alloc_ctx);
		goto err_dma_contig;
	}

	dma_set_attr(DMA_ATTR_ALLOC_SINGLE_PAGES, &attrs_nohugepage);
	vpu->alloc_ctx_vm = vb2_dma_contig_init_ctx_attrs(&pdev->dev,
							  &attrs_nohugepage);
	if (IS_ERR(vpu->alloc_ctx_vm)) {
		ret = PTR_ERR(vpu->alloc_ctx_vm);
		goto err_dma_contig_vm;
	}

	ret = v4l2_device_register(&pdev->dev, &vpu->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		goto err_v4l2_dev_reg;
	}

	platform_set_drvdata(pdev, vpu);

	if (vpu->variant->dec_fmts) {
		ret = rockchip_vpu_video_device_register(vpu, false);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to register decoder\n");
			goto err_dec_reg;
		}
	}

	if (vpu->variant->enc_fmts) {
		ret = rockchip_vpu_video_device_register(vpu, true);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to register encoder\n");
			goto err_enc_reg;
		}
	}

	vpu_debug_leave();

	return 0;

err_enc_reg:
	if (vpu->vfd_dec) {
		video_unregister_device(vpu->vfd_dec);
		video_device_release(vpu->vfd_dec);
	}
err_dec_reg:
	v4l2_device_unregister(&vpu->v4l2_dev);
err_v4l2_dev_reg:
	vb2_dma_contig_cleanup_ctx(vpu->alloc_ctx_vm);
err_dma_contig_vm:
	vb2_dma_contig_cleanup_ctx(vpu->alloc_ctx);
err_dma_contig:
	pm_runtime_disable(vpu->dev);
	vpu->variant->clk_disable(vpu);
err_hw_probe:
	pr_debug("%s-- with error\n", __func__);
	vpu_debug_leave();

	return ret;
}

static int rockchip_vpu_remove(struct platform_device *pdev)
{
	struct rockchip_vpu_dev *vpu = platform_get_drvdata(pdev);

	vpu_debug_enter();

	v4l2_info(&vpu->v4l2_dev, "Removing %s\n", pdev->name);

	/*
	 * We are safe here assuming that .remove() got called as
	 * a result of module removal, which guarantees that all
	 * contexts have been released.
	 */

	if (vpu->vfd_enc) {
		video_unregister_device(vpu->vfd_enc);
		video_device_release(vpu->vfd_enc);
	}
	if (vpu->vfd_dec) {
		video_unregister_device(vpu->vfd_dec);
		video_device_release(vpu->vfd_dec);
	}
	v4l2_device_unregister(&vpu->v4l2_dev);
	vb2_dma_contig_cleanup_ctx(vpu->alloc_ctx_vm);
	vb2_dma_contig_cleanup_ctx(vpu->alloc_ctx);
	pm_runtime_disable(vpu->dev);
	vpu->variant->clk_disable(vpu);

	vpu_debug_leave();

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rockchip_vpu_suspend(struct device *dev)
{
	struct rockchip_vpu_dev *vpu = dev_get_drvdata(dev);

	set_bit(VPU_SUSPENDED, &vpu->state);
	wait_event(vpu->run_wq, vpu->current_ctx == NULL);

	return 0;
}

static int rockchip_vpu_resume(struct device *dev)
{
	struct rockchip_vpu_dev *vpu = dev_get_drvdata(dev);

	clear_bit(VPU_SUSPENDED, &vpu->state);
	rockchip_vpu_try_run(vpu);

	return 0;
}
#endif

static const struct dev_pm_ops rockchip_vpu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rockchip_vpu_suspend, rockchip_vpu_resume)
};

static struct platform_driver rockchip_vpu_driver = {
	.probe = rockchip_vpu_probe,
	.remove = rockchip_vpu_remove,
	.driver = {
		   .name = "rockchip-vpu",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(of_rockchip_vpu_match),
		   .pm = &rockchip_vpu_pm_ops,
	},
};
module_platform_driver(rockchip_vpu_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Alpha Lin <Alpha.Lin@Rock-Chips.com>");
MODULE_AUTHOR("Tomasz Figa <tfiga@chromium.org>");
MODULE_DESCRIPTION("Rockchip VPU codec driver");
