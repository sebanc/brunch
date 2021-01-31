/*
 * Rockchip VPU codec driver
 *
 * Copyright (C) 2014 Google, Inc.
 *	Tomasz Figa <tfiga@chromium.org>
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

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>

/*
 * Hardware control routines.
 */

void rockchip_vpu_power_on(struct rockchip_vpu_dev *vpu)
{
	vpu_debug_enter();

	/* TODO: Clock gating. */

	pm_runtime_get_sync(vpu->dev);

	vpu_debug_leave();
}

static void rockchip_vpu_power_off(struct rockchip_vpu_dev *vpu)
{
	vpu_debug_enter();

	pm_runtime_mark_last_busy(vpu->dev);
	pm_runtime_put_autosuspend(vpu->dev);

	/* TODO: Clock gating. */

	vpu_debug_leave();
}

/*
 * Interrupt handlers.
 */

void rockchip_vpu_irq_done(struct rockchip_vpu_dev *vpu)
{
	struct rockchip_vpu_ctx *ctx = vpu->current_ctx;

	rockchip_vpu_power_off(vpu);
	cancel_delayed_work(&vpu->watchdog_work);

	ctx->hw.codec_ops->done(ctx, VB2_BUF_STATE_DONE);
}

void rockchip_vpu_watchdog(struct work_struct *work)
{
	struct rockchip_vpu_dev *vpu = container_of(to_delayed_work(work),
					struct rockchip_vpu_dev, watchdog_work);
	struct rockchip_vpu_ctx *ctx = vpu->current_ctx;
	unsigned long flags;

	spin_lock_irqsave(&vpu->irqlock, flags);

	ctx->hw.codec_ops->reset(ctx);

	spin_unlock_irqrestore(&vpu->irqlock, flags);

	vpu_err("frame processing timed out!\n");

	rockchip_vpu_power_off(vpu);
	ctx->hw.codec_ops->done(ctx, VB2_BUF_STATE_ERROR);
}

void rockchip_vpu_run(struct rockchip_vpu_ctx *ctx)
{
	ctx->hw.codec_ops->run(ctx);
}

int rockchip_vpu_init(struct rockchip_vpu_ctx *ctx)
{
	enum rockchip_vpu_codec_mode codec_mode;

	if (rockchip_vpu_ctx_is_encoder(ctx))
		codec_mode = ctx->vpu_dst_fmt->codec_mode; /* Encoder */
	else
		codec_mode = ctx->vpu_src_fmt->codec_mode; /* Decoder */

	ctx->hw.codec_ops = &ctx->dev->variant->mode_ops[codec_mode];

	return ctx->hw.codec_ops->init ? ctx->hw.codec_ops->init(ctx) : 0;
}

void rockchip_vpu_deinit(struct rockchip_vpu_ctx *ctx)
{
	if (ctx->hw.codec_ops->exit)
		ctx->hw.codec_ops->exit(ctx);
}
