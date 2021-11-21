/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author:Mark Yao <mark.yao@rock-chips.com>
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

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_plane_helper.h>

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/wait.h>

#include <linux/reset.h>
#include <linux/delay.h>

#include <soc/rockchip/rk3399_dmc.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_gem.h"
#include "rockchip_drm_fb.h"
#include "rockchip_drm_psr.h"
#include "rockchip_drm_vop.h"

#define VOP_WIN_SET(x, win, name, v) \
		vop_reg_set(vop, &win->phy->name, win->base, ~0, v, #name)
#define VOP_SCL_SET(x, win, name, v) \
		vop_reg_set(vop, &win->phy->scl->name, win->base, ~0, v, #name)
#define VOP_SCL_SET_EXT(x, win, name, v) \
		vop_reg_set(vop, &win->phy->scl->ext->name, \
			    win->base, ~0, v, #name)
#define VOP_AFBDC_SET(x, name, v) \
		if (vop->data->afbdc) \
			vop_reg_set(vop, &vop->data->afbdc->name, 0, ~0, v, #name)

#define VOP_WIN_YUV2YUV_SET(x, win_yuv2yuv, name, v)     \
		if (win_yuv2yuv->name.mask) \
			vop_reg_set(vop, &win_yuv2yuv->name, 0, ~0, v, #name)

#define VOP_WIN_YUV2YUV_COEFFICIENT_SET(x, win_yuv2yuv, name, v)     \
		if (win_yuv2yuv->phy->name.mask) \
			vop_reg_set(vop, &win_yuv2yuv->phy->name, win_yuv2yuv->base, ~0, v, #name)

#define VOP_INTR_SET_MASK(vop, name, mask, v) \
		vop_reg_set(vop, &vop->data->intr->name, 0, mask, v, #name)

#define VOP_REG_SET(vop, group, name, v) \
		    vop_reg_set(vop, &vop->data->group->name, 0, ~0, v, #name)

#define VOP_INTR_SET_TYPE(vop, name, type, v) \
	do { \
		int i, reg = 0, mask = 0; \
		for (i = 0; i < vop->data->intr->nintrs; i++) { \
			if (vop->data->intr->intrs[i] & type) { \
				reg |= (v) << i; \
				mask |= 1 << i; \
			} \
		} \
		VOP_INTR_SET_MASK(vop, name, mask, reg); \
	} while (0)
#define VOP_INTR_GET_TYPE(vop, name, type) \
		vop_get_intr_type(vop, &vop->data->intr->name, type)

#define VOP_WIN_GET(x, win, name) \
		vop_read_reg(x, win->offset, win->phy->name)

#define VOP_WIN_GET_YRGBADDR(vop, win) \
		vop_readl(vop, win->base + win->phy->yrgb_mst.offset)

#define VOP_WIN_TO_INDEX(vop_win) \
	((vop_win) - (vop_win)->vop->win)

#define to_vop(x) container_of(x, struct vop, crtc)
#define to_vop_win(x) container_of(x, struct vop_win, base)

#define REFLECT_Y_MASK (DRM_REFLECT_Y | DRM_ROTATE_0)

/*
 * The coefficients of the following matrix are all fixed points.
 * The format is S2.10 for the 3x3 part of the matrix, and S9.12 for the offsets.
 * They are all represented in two's complement.
 */
static const uint32_t bt601_yuv2rgb[] = {
	0x4A8, 0x0,    0x662,
	0x4A8, 0x1E6F, 0x1CBF,
	0x4A8, 0x812,  0x0,
	0x321168, 0x0877CF, 0x2EB127
};

enum vop_pending {
	VOP_PENDING_FB_UNREF,
};

struct vop_win {
	struct drm_plane base;
	const struct vop_win_data *data;
	const struct vop_win_yuv2yuv_data *yuv2yuv_data;
	struct vop *vop;
};

struct vop {
	struct drm_crtc crtc;
	struct device *dev;
	struct drm_device *drm_dev;
	bool is_enabled;

	struct vop_win *afbdc_win;
	unsigned int win_enabled;

	struct completion dsp_hold_completion;

	/* protected by dev->event_lock */
	struct drm_pending_vblank_event *event;

	struct drm_flip_work fb_unref_work;
	unsigned long pending;

	ktime_t line_flag_timestamp;
	atomic_t line_flag_wait_count;
	wait_queue_head_t line_flag_wq;
	struct notifier_block dmc_nb;

	const struct vop_data *data;

	uint32_t *regsbak;
	void __iomem *regs;

	/* physical map length of vop register */
	uint32_t len;

	/* one time only one process allowed to config the register */
	spinlock_t reg_lock;
	/* lock vop irq reg */
	spinlock_t irq_lock;

	unsigned int irq;

	/* vop AHP clk */
	struct clk *hclk;
	/* vop dclk */
	struct clk *dclk;
	/* vop share memory frequency */
	struct clk *aclk;

	/* vop dclk reset */
	struct reset_control *dclk_rst;

	struct vop_win win[];
};

static inline void vop_writel(struct vop *vop, uint32_t offset, uint32_t v)
{
	writel(v, vop->regs + offset);
	vop->regsbak[offset >> 2] = v;
}

static inline uint32_t vop_readl(struct vop *vop, uint32_t offset)
{
	return readl(vop->regs + offset);
}

static inline uint32_t vop_read_reg(struct vop *vop, uint32_t base,
				    const struct vop_reg *reg)
{
	return (vop_readl(vop, base + reg->offset) >> reg->shift) & reg->mask;
}

static void vop_reg_set(struct vop *vop, const struct vop_reg *reg,
			uint32_t _offset, uint32_t _mask, uint32_t v,
			const char *reg_name)
{
	int offset, mask, shift;

	if (!reg || !reg->mask) {
		dev_dbg(vop->dev, "Warning: not support %s\n", reg_name);
		return;
	}

	offset = reg->offset + _offset;
	mask = reg->mask & _mask;
	shift = reg->shift;

	if (reg->write_mask) {
		v = ((v << shift) & 0xffff) | (mask << (shift + 16));
	} else {
		uint32_t cached_val = vop->regsbak[offset >> 2];

		v = (cached_val & ~(mask << shift)) | (v << shift);
		vop->regsbak[offset >> 2] = v;
	}

	if (reg->relaxed)
		writel_relaxed(v, vop->regs + offset);
	else
		writel(v, vop->regs + offset);
}

static inline uint32_t vop_get_intr_type(struct vop *vop,
					 const struct vop_reg *reg, int type)
{
	uint32_t i, ret = 0;
	uint32_t regs = vop_read_reg(vop, 0, reg);

	for (i = 0; i < vop->data->intr->nintrs; i++) {
		if ((type & vop->data->intr->intrs[i]) && (regs & 1 << i))
			ret |= vop->data->intr->intrs[i];
	}

	return ret;
}

static inline void vop_cfg_done(struct vop *vop)
{
	VOP_REG_SET(vop, common, cfg_done, 1);
}

static bool has_rb_swapped(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_BGR565:
		return true;
	default:
		return false;
	}
}

static int vop_convert_afbdc_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return AFBDC_FMT_U8U8U8U8;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		return AFBDC_FMT_U8U8U8;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		return AFBDC_FMT_RGB565;
	default:
		DRM_ERROR("unsupported afbdc format[%08x]\n", format);
		return -EINVAL;
	}
}

static enum vop_data_format vop_convert_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return VOP_FMT_ARGB8888;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		return VOP_FMT_RGB888;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		return VOP_FMT_RGB565;
	case DRM_FORMAT_NV12:
		return VOP_FMT_YUV420SP;
	case DRM_FORMAT_NV16:
		return VOP_FMT_YUV422SP;
	case DRM_FORMAT_NV24:
		return VOP_FMT_YUV444SP;
	default:
		DRM_ERROR("unsupported format[%08x]\n", format);
		return -EINVAL;
	}
}

static bool is_yuv_support(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV24:
		return true;
	default:
		return false;
	}
}

static bool is_alpha_support(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
		return true;
	default:
		return false;
	}
}

static uint16_t scl_vop_cal_scale(enum scale_mode mode, uint32_t src,
				  uint32_t dst, bool is_horizontal,
				  int vsu_mode, int *vskiplines)
{
	uint16_t val = 1 << SCL_FT_DEFAULT_FIXPOINT_SHIFT;

	if (vskiplines)
		*vskiplines = 0;

	if (is_horizontal) {
		if (mode == SCALE_UP)
			val = GET_SCL_FT_BIC(src, dst);
		else if (mode == SCALE_DOWN)
			val = GET_SCL_FT_BILI_DN(src, dst);
	} else {
		if (mode == SCALE_UP) {
			if (vsu_mode == SCALE_UP_BIL)
				val = GET_SCL_FT_BILI_UP(src, dst);
			else
				val = GET_SCL_FT_BIC(src, dst);
		} else if (mode == SCALE_DOWN) {
			if (vskiplines) {
				*vskiplines = scl_get_vskiplines(src, dst);
				val = scl_get_bili_dn_vskip(src, dst,
							    *vskiplines);
			} else {
				val = GET_SCL_FT_BILI_DN(src, dst);
			}
		}
	}

	return val;
}

static void scl_vop_cal_scl_fac(struct vop *vop, const struct vop_win_data *win,
			     uint32_t src_w, uint32_t src_h, uint32_t dst_w,
			     uint32_t dst_h, uint32_t pixel_format)
{
	uint16_t yrgb_hor_scl_mode, yrgb_ver_scl_mode;
	uint16_t cbcr_hor_scl_mode = SCALE_NONE;
	uint16_t cbcr_ver_scl_mode = SCALE_NONE;
	int hsub = drm_format_horz_chroma_subsampling(pixel_format);
	int vsub = drm_format_vert_chroma_subsampling(pixel_format);
	bool is_yuv = is_yuv_support(pixel_format);
	uint16_t cbcr_src_w = src_w / hsub;
	uint16_t cbcr_src_h = src_h / vsub;
	uint16_t vsu_mode;
	uint16_t lb_mode;
	uint32_t val;
	int vskiplines;

	if (dst_w > 3840) {
		DRM_DEV_ERROR(vop->dev, "Maximum dst width (3840) exceeded\n");
		return;
	}

	if (!win->phy->scl->ext) {
		VOP_SCL_SET(vop, win, scale_yrgb_x,
			    scl_cal_scale2(src_w, dst_w));
		VOP_SCL_SET(vop, win, scale_yrgb_y,
			    scl_cal_scale2(src_h, dst_h));
		if (is_yuv) {
			VOP_SCL_SET(vop, win, scale_cbcr_x,
				    scl_cal_scale2(cbcr_src_w, dst_w));
			VOP_SCL_SET(vop, win, scale_cbcr_y,
				    scl_cal_scale2(cbcr_src_h, dst_h));
		}
		return;
	}

	yrgb_hor_scl_mode = scl_get_scl_mode(src_w, dst_w);
	yrgb_ver_scl_mode = scl_get_scl_mode(src_h, dst_h);

	if (is_yuv) {
		cbcr_hor_scl_mode = scl_get_scl_mode(cbcr_src_w, dst_w);
		cbcr_ver_scl_mode = scl_get_scl_mode(cbcr_src_h, dst_h);
		if (cbcr_hor_scl_mode == SCALE_DOWN)
			lb_mode = scl_vop_cal_lb_mode(dst_w, true);
		else
			lb_mode = scl_vop_cal_lb_mode(cbcr_src_w, true);
	} else {
		if (yrgb_hor_scl_mode == SCALE_DOWN)
			lb_mode = scl_vop_cal_lb_mode(dst_w, false);
		else
			lb_mode = scl_vop_cal_lb_mode(src_w, false);
	}

	VOP_SCL_SET_EXT(vop, win, lb_mode, lb_mode);
	if (lb_mode == LB_RGB_3840X2) {
		if (yrgb_ver_scl_mode != SCALE_NONE) {
			DRM_DEV_ERROR(vop->dev, "not allow yrgb ver scale\n");
			return;
		}
		if (cbcr_ver_scl_mode != SCALE_NONE) {
			DRM_DEV_ERROR(vop->dev, "not allow cbcr ver scale\n");
			return;
		}
		vsu_mode = SCALE_UP_BIL;
	} else if (lb_mode == LB_RGB_2560X4) {
		vsu_mode = SCALE_UP_BIL;
	} else {
		vsu_mode = SCALE_UP_BIC;
	}

	val = scl_vop_cal_scale(yrgb_hor_scl_mode, src_w, dst_w,
				true, 0, NULL);
	VOP_SCL_SET(vop, win, scale_yrgb_x, val);
	val = scl_vop_cal_scale(yrgb_ver_scl_mode, src_h, dst_h,
				false, vsu_mode, &vskiplines);
	VOP_SCL_SET(vop, win, scale_yrgb_y, val);

	VOP_SCL_SET_EXT(vop, win, vsd_yrgb_gt4, vskiplines == 4);
	VOP_SCL_SET_EXT(vop, win, vsd_yrgb_gt2, vskiplines == 2);

	VOP_SCL_SET_EXT(vop, win, yrgb_hor_scl_mode, yrgb_hor_scl_mode);
	VOP_SCL_SET_EXT(vop, win, yrgb_ver_scl_mode, yrgb_ver_scl_mode);
	VOP_SCL_SET_EXT(vop, win, yrgb_hsd_mode, SCALE_DOWN_BIL);
	VOP_SCL_SET_EXT(vop, win, yrgb_vsd_mode, SCALE_DOWN_BIL);
	VOP_SCL_SET_EXT(vop, win, yrgb_vsu_mode, vsu_mode);
	if (is_yuv) {
		val = scl_vop_cal_scale(cbcr_hor_scl_mode, cbcr_src_w,
					dst_w, true, 0, NULL);
		VOP_SCL_SET(vop, win, scale_cbcr_x, val);
		val = scl_vop_cal_scale(cbcr_ver_scl_mode, cbcr_src_h,
					dst_h, false, vsu_mode, &vskiplines);
		VOP_SCL_SET(vop, win, scale_cbcr_y, val);

		VOP_SCL_SET_EXT(vop, win, vsd_cbcr_gt4, vskiplines == 4);
		VOP_SCL_SET_EXT(vop, win, vsd_cbcr_gt2, vskiplines == 2);
		VOP_SCL_SET_EXT(vop, win, cbcr_hor_scl_mode, cbcr_hor_scl_mode);
		VOP_SCL_SET_EXT(vop, win, cbcr_ver_scl_mode, cbcr_ver_scl_mode);
		VOP_SCL_SET_EXT(vop, win, cbcr_hsd_mode, SCALE_DOWN_BIL);
		VOP_SCL_SET_EXT(vop, win, cbcr_vsd_mode, SCALE_DOWN_BIL);
		VOP_SCL_SET_EXT(vop, win, cbcr_vsu_mode, vsu_mode);
	}
}

static void vop_dsp_hold_valid_irq_enable(struct vop *vop)
{
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, clear, DSP_HOLD_VALID_INTR, 1);
	VOP_INTR_SET_TYPE(vop, enable, DSP_HOLD_VALID_INTR, 1);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static void vop_dsp_hold_valid_irq_disable(struct vop *vop)
{
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, enable, DSP_HOLD_VALID_INTR, 0);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

/*
 * (1) each frame starts at the start of the Vsync pulse which is signaled by
 *     the "FRAME_SYNC" interrupt.
 * (2) the active data region of each frame ends at dsp_vact_end
 * (3) we should program this same number (dsp_vact_end) into dsp_line_frag_num,
 *      to get "LINE_FLAG" interrupt at the end of the active on screen data.
 *
 * VOP_INTR_CTRL0.dsp_line_frag_num = VOP_DSP_VACT_ST_END.dsp_vact_end
 * Interrupts
 * LINE_FLAG -------------------------------+
 * FRAME_SYNC ----+                         |
 *                |                         |
 *                v                         v
 *                | Vsync | Vbp |  Vactive  | Vfp |
 *                        ^     ^           ^     ^
 *                        |     |           |     |
 *                        |     |           |     |
 * dsp_vs_end ------------+     |           |     |   VOP_DSP_VTOTAL_VS_END
 * dsp_vact_start --------------+           |     |   VOP_DSP_VACT_ST_END
 * dsp_vact_end ----------------------------+     |   VOP_DSP_VACT_ST_END
 * dsp_total -------------------------------------+   VOP_DSP_VTOTAL_VS_END
 */
static void vop_line_flag_irq_get(struct vop *vop)
{
	unsigned long flags;

	if (atomic_inc_return(&vop->line_flag_wait_count) != 1)
		return;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, clear, LINE_FLAG_INTR, 1);
	VOP_INTR_SET_TYPE(vop, enable, LINE_FLAG_INTR, 1);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static void vop_line_flag_irq_put(struct vop *vop)
{
	unsigned long flags;

	if (!atomic_dec_and_test(&vop->line_flag_wait_count))
		return;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, enable, LINE_FLAG_INTR, 0);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static int vop_enable(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	int ret, i;

	if (vop->is_enabled)
		return 0;

	ret = pm_runtime_get_sync(vop->dev);
	if (ret < 0) {
		dev_err(vop->dev, "failed to get pm runtime: %d\n", ret);
		return ret;
	}

	ret = clk_enable(vop->hclk);
	if (WARN_ON(ret < 0))
		goto err_put_pm_runtime;

	ret = clk_enable(vop->dclk);
	if (WARN_ON(ret < 0))
		goto err_disable_hclk;

	ret = clk_enable(vop->aclk);
	if (WARN_ON(ret < 0))
		goto err_disable_dclk;

	/*
	 * Slave iommu shares power, irq and clock with vop.  It was associated
	 * automatically with this master device via common driver code.
	 * Now that we have enabled the clock we attach it to the shared drm
	 * mapping.
	 */
	ret = rockchip_drm_dma_attach_device(vop->drm_dev, vop->dev);
	if (ret) {
		dev_err(vop->dev, "failed to attach dma mapping, %d\n", ret);
		goto err_disable_aclk;
	}

	memcpy(vop->regs, vop->regsbak, vop->len);
	/*
	 * We need to make sure that all windows are disabled before we
	 * enable the crtc. Otherwise we might try to scan from a destroyed
	 * buffer later.
	 */
	for (i = 0; i < vop->data->win_size; i++) {
		struct vop_win *vop_win = &vop->win[i];
		const struct vop_win_data *win = vop_win->data;

		spin_lock(&vop->reg_lock);
		VOP_WIN_SET(vop, win, enable, 0);
		vop->win_enabled &= ~BIT(i);
		spin_unlock(&vop->reg_lock);
	}

	if (vop->data->afbdc) {
		VOP_AFBDC_SET(vop, enable, 0);
		vop->afbdc_win = NULL;
	}

	vop_cfg_done(vop);

	/*
	 * At here, vop clock & iommu is enable, R/W vop regs would be safe.
	 */
	vop->is_enabled = true;

	spin_lock(&vop->reg_lock);

	VOP_REG_SET(vop, common, standby, 1);

	spin_unlock(&vop->reg_lock);

	enable_irq(vop->irq);

	drm_crtc_vblank_on(crtc);

	return 0;

err_disable_aclk:
	clk_disable(vop->aclk);
err_disable_dclk:
	clk_disable(vop->dclk);
err_disable_hclk:
	clk_disable(vop->hclk);
err_put_pm_runtime:
	pm_runtime_put_sync(vop->dev);
	return ret;
}

void rockchip_drm_set_win_enabled(struct drm_crtc *crtc, bool enabled)
{
	struct vop *vop = to_vop(crtc);
	int i;

	spin_lock(&vop->reg_lock);

	for (i = 0; i < vop->data->win_size; i++) {
		struct vop_win *vop_win = &vop->win[i];
		const struct vop_win_data *win = vop_win->data;

		VOP_WIN_SET(vop, win, enable,
			    enabled && (vop->win_enabled & BIT(i)));
	}

	VOP_AFBDC_SET(vop, enable, enabled && vop->afbdc_win != NULL);

	vop_cfg_done(vop);

	spin_unlock(&vop->reg_lock);
}
EXPORT_SYMBOL(rockchip_drm_set_win_enabled);

static void vop_crtc_disable(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	struct rockchip_drm_private *priv = crtc->dev->dev_private;

	if (!vop->is_enabled)
		return;

	rockchip_dmcfreq_unregister_clk_sync_nb(priv->devfreq, &vop->dmc_nb);

	drm_crtc_vblank_off(crtc);

	/*
	 * Vop standby will take effect at end of current frame,
	 * if dsp hold valid irq happen, it means standby complete.
	 *
	 * we must wait standby complete when we want to disable aclk,
	 * if not, memory bus maybe dead.
	 */
	reinit_completion(&vop->dsp_hold_completion);
	vop_dsp_hold_valid_irq_enable(vop);

	spin_lock(&vop->reg_lock);

	VOP_REG_SET(vop, common, standby, 1);

	spin_unlock(&vop->reg_lock);

	wait_for_completion(&vop->dsp_hold_completion);

	vop_dsp_hold_valid_irq_disable(vop);

	disable_irq(vop->irq);

	vop->is_enabled = false;

	/*
	 * vop standby complete, so iommu detach is safe.
	 */
	rockchip_drm_dma_detach_device(vop->drm_dev, vop->dev);

	clk_disable(vop->dclk);
	clk_disable(vop->aclk);
	clk_disable(vop->hclk);
	pm_runtime_put(vop->dev);
}

static void vop_plane_destroy(struct drm_plane *plane)
{
	drm_plane_cleanup(plane);
}

static int vop_plane_atomic_check(struct drm_plane *plane,
			   struct drm_plane_state *state)
{
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_framebuffer *fb = state->fb;
	struct vop_win *vop_win = to_vop_win(plane);
	const struct vop_win_data *win = vop_win->data;
	int ret;
	struct drm_rect clip;
	int min_scale = win->phy->scl ? FRAC_16_16(1, 8) :
					DRM_PLANE_HELPER_NO_SCALING;
	int max_scale = win->phy->scl ? FRAC_16_16(8, 1) :
					DRM_PLANE_HELPER_NO_SCALING;

	if (!crtc || !fb)
		return 0;

	if (state->state)
		crtc_state = drm_atomic_get_existing_crtc_state(state->state,
								crtc);
	else /* Special case for asynchronous cursor updates. */
		crtc_state = plane->crtc->state;
	if (WARN_ON(!crtc_state))
		return -EINVAL;

	clip.x1 = 0;
	clip.y1 = 0;
	clip.x2 = crtc_state->adjusted_mode.hdisplay;
	clip.y2 = crtc_state->adjusted_mode.vdisplay;

	ret = drm_plane_helper_check_state(state, &clip,
					   min_scale, max_scale,
					   true, true);
	if (ret)
		return ret;

	if (!state->visible)
		return 0;

	ret = vop_convert_format(fb->format->format);
	if (ret < 0)
		return ret;

	/*
	 * Src.x1 can be odd when do clip, but yuv plane start point
	 * need align with 2 pixel.
	 */
	if (is_yuv_support(fb->format->format) && ((state->src.x1 >> 16) % 2)) {
		DRM_ERROR("Invalid Source: Yuv format not support odd xpos\n");
		return -EINVAL;
	}

	if (is_yuv_support(fb->format->format) && (state->rotation != DRM_ROTATE_0))
		return -EINVAL;

	if (state->ctm) {
		if (state->ctm->length != sizeof(struct drm_color_ctm)) {
			DRM_ERROR("Invalid PLANE_CTM blob size.\n");
			return -EINVAL;
		}
	}

	if (state->rotation & ~REFLECT_Y_MASK)
		return -EINVAL;

	if (fb->modifier == DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC) {
		struct vop *vop = to_vop(crtc);

		if (!vop->data->afbdc) {
			DRM_ERROR("vop does not support AFBDC\n");
			return -EINVAL;
		}

		ret = vop_convert_afbdc_format(fb->format->format);
		if (ret < 0)
			return ret;

		if (state->src.x1 || state->src.y1) {
			DRM_ERROR("afbdc does not support offset display\n");
			DRM_ERROR("xpos=%d, ypos=%d, offset=%d\n",
				  state->src.x1, state->src.y1, fb->offsets[0]);
			return -EINVAL;
		}

		if (state->rotation != DRM_ROTATE_0) {
			DRM_ERROR("afbdc does not support rotation\n");
			DRM_ERROR("rotation=%d\n", state->rotation);
			return -EINVAL;
		}
	}

	return 0;
}

static void vop_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct vop_win *vop_win = to_vop_win(plane);
	const struct vop_win_data *win = vop_win->data;
	struct vop *vop = to_vop(old_state->crtc);

	if (!old_state->crtc)
		return;

	spin_lock(&vop->reg_lock);

	if (vop->afbdc_win == vop_win)
		vop->afbdc_win = NULL;

	VOP_WIN_SET(vop, win, enable, 0);
	vop->win_enabled &= ~BIT(VOP_WIN_TO_INDEX(vop_win));

	spin_unlock(&vop->reg_lock);
}

static void vop_plane_atomic_update(struct drm_plane *plane,
		struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_crtc *crtc = state->crtc;
	struct vop_win *vop_win = to_vop_win(plane);
	const struct vop_win_data *win = vop_win->data;
	const struct vop_win_yuv2yuv_data *win_yuv2yuv = vop_win->yuv2yuv_data;
	struct vop *vop = to_vop(state->crtc);
	struct drm_framebuffer *fb = state->fb;
	unsigned int actual_w, actual_h;
	unsigned int dsp_stx, dsp_sty;
	uint32_t act_info, dsp_info, dsp_st;
	struct drm_rect *src = &state->src;
	struct drm_rect *dest = &state->dst;
	struct drm_gem_object *obj, *uv_obj;
	struct rockchip_gem_object *rk_obj, *rk_uv_obj;
	unsigned long offset;
	dma_addr_t dma_addr;
	uint32_t val;
	bool rb_swap;
	int format;
	int is_yuv = is_yuv_support(fb->format->format);
	int win_index = VOP_WIN_TO_INDEX(vop_win);
	int i;

	/*
	 * can't update plane when vop is disabled.
	 */
	if (!crtc)
		return;

	if (WARN_ON(!vop->is_enabled))
		return;

	if (vop->afbdc_win == vop_win)
		vop->afbdc_win = NULL;

	if (!state->visible) {
		vop_plane_atomic_disable(plane, old_state);
		return;
	}

	obj = rockchip_fb_get_gem_obj(fb, 0);
	rk_obj = to_rockchip_obj(obj);

	actual_w = drm_rect_width(src) >> 16;
	actual_h = drm_rect_height(src) >> 16;
	act_info = (actual_h - 1) << 16 | ((actual_w - 1) & 0xffff);

	dsp_info = (drm_rect_height(dest) - 1) << 16;
	dsp_info |= (drm_rect_width(dest) - 1) & 0xffff;

	dsp_stx = dest->x1 + crtc->mode.htotal - crtc->mode.hsync_start;
	dsp_sty = dest->y1 + crtc->mode.vtotal - crtc->mode.vsync_start;
	dsp_st = dsp_sty << 16 | (dsp_stx & 0xffff);

	offset = (src->x1 >> 16) * fb->format->cpp[0];
	offset += (src->y1 >> 16) * fb->pitches[0];
	dma_addr = rk_obj->dma_addr + offset + fb->offsets[0];
	/* For rotation, move dma_addr to the beginning of the last line. */
	if (state->rotation == REFLECT_Y_MASK)
		dma_addr += (actual_h - 1) * fb->pitches[0];

	format = vop_convert_format(fb->format->format);

	spin_lock(&vop->reg_lock);

	if (fb->modifier == DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC) {
		int afbdc_format = vop_convert_afbdc_format(fb->format->format);

		VOP_AFBDC_SET(vop, format, afbdc_format | 1 << 4);
		VOP_AFBDC_SET(vop, hreg_block_split, 0);
		VOP_AFBDC_SET(vop, win_sel, win_index);
		VOP_AFBDC_SET(vop, hdr_ptr, dma_addr);
		VOP_AFBDC_SET(vop, pic_size, act_info);

		vop->afbdc_win = vop_win;
	}

	VOP_WIN_SET(vop, win, format, format);
	VOP_WIN_SET(vop, win, yrgb_vir, DIV_ROUND_UP(fb->pitches[0], 4));
	VOP_WIN_SET(vop, win, yrgb_mst, dma_addr);
	VOP_WIN_SET(vop, win, y_mir_en,	state->rotation == REFLECT_Y_MASK);

	VOP_WIN_YUV2YUV_SET(vop, win_yuv2yuv, y2r_en, is_yuv);

	if (is_yuv) {
		int hsub = drm_format_horz_chroma_subsampling(fb->format->format);
		int vsub = drm_format_vert_chroma_subsampling(fb->format->format);
		int bpp = fb->format->cpp[1];

		uv_obj = rockchip_fb_get_gem_obj(fb, 1);
		rk_uv_obj = to_rockchip_obj(uv_obj);

		offset = (src->x1 >> 16) * bpp / hsub;
		offset += (src->y1 >> 16) * fb->pitches[1] / vsub;

		dma_addr = rk_uv_obj->dma_addr + offset + fb->offsets[1];
		VOP_WIN_SET(vop, win, uv_vir, DIV_ROUND_UP(fb->pitches[1], 4));
		VOP_WIN_SET(vop, win, uv_mst, dma_addr);

		for (i = 0; i < NUM_YUV2YUV_COEFFICIENTS; i++) {
			VOP_WIN_YUV2YUV_COEFFICIENT_SET(vop,
							win_yuv2yuv,
							y2r_coefficients[i],
							bt601_yuv2rgb[i]);
		}
	}

	VOP_WIN_YUV2YUV_SET(vop, win_yuv2yuv, r2r_en, !!state->ctm);
	if (state->ctm) {
		struct drm_color_ctm *color_ctm =
			(struct drm_color_ctm*)state->ctm->data;
		/*
		 * Convert matrix values from sign and fixed point U31.32 to
		 * two's complement S3.10.
		 */
		for (i = 0; i < 9; i++) {
			uint64_t input = color_ctm->matrix[i];
			uint64_t abs_coeff = GENMASK_ULL(62, 0) & input;
			s64 val = (input & BIT_ULL(63)) ?
				-(abs_coeff >> 22) : (abs_coeff >> 22);
			val = clamp_val(val, -4 << 10, (4 << 10) - 1);
			VOP_WIN_YUV2YUV_COEFFICIENT_SET(vop,
							win_yuv2yuv,
							r2r_coefficients[i],
							val);
		}
	}

	if (win->phy->scl)
		scl_vop_cal_scl_fac(vop, win, actual_w, actual_h,
				    drm_rect_width(dest), drm_rect_height(dest),
				    fb->format->format);

	VOP_WIN_SET(vop, win, act_info, act_info);
	VOP_WIN_SET(vop, win, dsp_info, dsp_info);
	VOP_WIN_SET(vop, win, dsp_st, dsp_st);

	rb_swap = has_rb_swapped(fb->format->format);
	VOP_WIN_SET(vop, win, rb_swap, rb_swap);

	if (is_alpha_support(fb->format->format)) {
		VOP_WIN_SET(vop, win, dst_alpha_ctl,
			    DST_FACTOR_M0(ALPHA_SRC_INVERSE));
		val = SRC_ALPHA_EN(1) | SRC_COLOR_M0(ALPHA_SRC_PRE_MUL) |
			SRC_ALPHA_M0(ALPHA_STRAIGHT) |
			SRC_BLEND_M0(ALPHA_PER_PIX) |
			SRC_ALPHA_CAL_M0(ALPHA_NO_SATURATION) |
			SRC_FACTOR_M0(ALPHA_ONE);
		VOP_WIN_SET(vop, win, src_alpha_ctl, val);
	} else {
		VOP_WIN_SET(vop, win, src_alpha_ctl, SRC_ALPHA_EN(0));
	}

	VOP_WIN_SET(vop, win, enable, 1);
	vop->win_enabled |= BIT(win_index);

	spin_unlock(&vop->reg_lock);
}

static const struct drm_plane_helper_funcs plane_helper_funcs = {
	.atomic_check = vop_plane_atomic_check,
	.atomic_update = vop_plane_atomic_update,
	.atomic_disable = vop_plane_atomic_disable,
};

static bool vop_fs_irq_is_pending(struct vop *vop)
{
	return VOP_INTR_GET_TYPE(vop, status, FS_INTR);
}

static void vop_wait_for_irq_handler(struct vop *vop)
{
	bool pending;
	int ret;

	/*
	 * Spin until frame start interrupt status bit goes low, which means
	 * that interrupt handler was invoked and cleared it. The timeout of
	 * 10 msecs is really too long, but it is just a safety measure if
	 * something goes really wrong. The wait will only happen in the very
	 * unlikely case of a vblank happening exactly at the same time and
	 * shouldn't exceed microseconds range.
	 */
	ret = readx_poll_timeout_atomic(vop_fs_irq_is_pending, vop, pending,
					!pending, 0, 10 * 1000);
	if (ret)
		DRM_DEV_ERROR(vop->dev, "VOP vblank IRQ stuck for 10 ms\n");

	synchronize_irq(vop->irq);
}

static int vop_cursor_update(struct drm_plane *plane,
			     struct drm_crtc *crtc,
			     struct drm_framebuffer *fb,
			     int crtc_x, int crtc_y,
			     unsigned int crtc_w, unsigned int crtc_h,
			     uint32_t src_x, uint32_t src_y,
			     uint32_t src_w, uint32_t src_h)
{
	struct drm_device *dev = crtc->dev;
	struct vop *vop = to_vop(crtc);
	struct rockchip_drm_private *private = dev->dev_private;
	struct rockchip_atomic_commit *commit = &private->commit;
	const struct drm_plane_helper_funcs *funcs;
	struct drm_plane_state *plane_state;
	int ret;

	funcs = plane->helper_private;

	mutex_lock(&commit->lock);

	/*
	 * We need to wait for any commits that perform a modeset or update
	 * the cursor plane using the atomic API directly.
	 */
	if (commit->needs_modeset || commit->has_cursor_plane)
		flush_kthread_worker(&commit->worker);

	plane_state = plane->funcs->atomic_duplicate_state(plane);

	plane_state->crtc_x = crtc_x;
	plane_state->crtc_y = crtc_y;
	plane_state->crtc_h = crtc_h;
	plane_state->crtc_w = crtc_w;
	plane_state->src_x = src_x;
	plane_state->src_y = src_y;
	plane_state->src_h = src_h;
	plane_state->src_w = src_w;

	drm_atomic_set_fb_for_plane(plane_state, fb);

	if (funcs && funcs->atomic_check) {
		ret = funcs->atomic_check(plane, plane_state);
		if (ret)
			goto err_destroy;
	}

	swap(plane_state, plane->state);

	if (vop->is_enabled) {
		struct drm_encoder *encoder;

		drm_for_each_encoder_mask(encoder, dev,
					  crtc->state->encoder_mask)
			rockchip_drm_psr_inhibit_get(encoder);

		mutex_lock(&commit->hw_lock);

		if (fb)
			funcs->atomic_update(plane, plane_state);
		else
			funcs->atomic_disable(plane, plane_state);

		spin_lock(&vop->reg_lock);
		vop_cfg_done(vop);
		spin_unlock(&vop->reg_lock);

		if (plane_state->fb && plane_state->fb != plane->state->fb) {
			vop_wait_for_irq_handler(vop);

			WARN_ON(drm_crtc_vblank_get(crtc) != 0);
			drm_framebuffer_reference(plane_state->fb);
			drm_flip_work_queue(&vop->fb_unref_work,
					    plane_state->fb);
			set_bit(VOP_PENDING_FB_UNREF, &vop->pending);
		}

		mutex_unlock(&commit->hw_lock);

		drm_for_each_encoder_mask(encoder, dev,
					  crtc->state->encoder_mask)
			rockchip_drm_psr_inhibit_put(encoder);
	}

err_destroy:
	mutex_unlock(&commit->lock);

	plane->funcs->atomic_destroy_state(plane, plane_state);

	return ret;
}

static int vop_plane_update(struct drm_plane *plane,
			    struct drm_crtc *crtc,
			    struct drm_framebuffer *fb,
			    int crtc_x, int crtc_y,
			    unsigned int crtc_w, unsigned int crtc_h,
			    uint32_t src_x, uint32_t src_y,
			    uint32_t src_w, uint32_t src_h)
{
	if (crtc && plane == crtc->cursor && plane->state->crtc == crtc)
		return vop_cursor_update(plane, crtc, fb,
					 crtc_x, crtc_y, crtc_w, crtc_h,
					 src_x, src_y, src_w, src_h);

	return drm_atomic_helper_update_plane(plane, crtc, fb,
					      crtc_x, crtc_y, crtc_w, crtc_h,
					      src_x, src_y, src_w, src_h);
}

static bool vop_plane_format_mod_supported(struct drm_plane *plane,
					   uint32_t format,
					   uint64_t modifier)
{
	switch (modifier) {
	case DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC:
		switch (format) {
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_ABGR8888:
			return true;
		default:
			return false;
		}
	default:
		return true;
	}
}

static const struct drm_plane_funcs vop_plane_funcs = {
	.update_plane	= vop_plane_update,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = vop_plane_destroy,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
	.format_mod_supported = vop_plane_format_mod_supported,
};

static int vop_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return -EPERM;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, clear, FS_INTR, 1);
	VOP_INTR_SET_TYPE(vop, enable, FS_INTR, 1);

	spin_unlock_irqrestore(&vop->irq_lock, flags);

	return 0;
}

static void vop_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	unsigned long flags;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock_irqsave(&vop->irq_lock, flags);

	VOP_INTR_SET_TYPE(vop, enable, FS_INTR, 0);

	spin_unlock_irqrestore(&vop->irq_lock, flags);
}

static void vop_crtc_cancel_pending_vblank(struct drm_crtc *crtc,
					   struct drm_file *file_priv)
{
	struct drm_device *drm = crtc->dev;
	struct vop *vop = to_vop(crtc);
	struct drm_pending_vblank_event *e;
	unsigned long flags;

	spin_lock_irqsave(&drm->event_lock, flags);
	e = vop->event;
	if (!e) {
		spin_unlock_irqrestore(&drm->event_lock, flags);
		return;
	}

	if (e->base.file_priv != file_priv) {
		spin_unlock_irqrestore(&drm->event_lock, flags);
		return;
	}

	vop->event = NULL;
	drm_crtc_vblank_put(crtc);

	spin_unlock_irqrestore(&drm->event_lock, flags);

	drm_event_cancel_free(drm, &e->base);
}

static const struct rockchip_crtc_funcs private_crtc_funcs = {
	.enable_vblank = vop_crtc_enable_vblank,
	.disable_vblank = vop_crtc_disable_vblank,
	.cancel_pending_vblank = vop_crtc_cancel_pending_vblank,
};

static bool vop_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct vop *vop = to_vop(crtc);

	adjusted_mode->clock =
		DIV_ROUND_UP(clk_round_rate(vop->dclk, mode->clock * 1000),
			     1000);

	return true;
}

static void vop_crtc_enable(struct drm_crtc *crtc)
{
	struct vop *vop = to_vop(crtc);
	const struct vop_data *vop_data = vop->data;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	struct rockchip_drm_private *priv = crtc->dev->dev_private;
	u16 hsync_len = adjusted_mode->hsync_end - adjusted_mode->hsync_start;
	u16 hdisplay = adjusted_mode->hdisplay;
	u16 htotal = adjusted_mode->htotal;
	u16 hact_st = adjusted_mode->htotal - adjusted_mode->hsync_start;
	u16 hact_end = hact_st + hdisplay;
	u16 vdisplay = adjusted_mode->vdisplay;
	u16 vtotal = adjusted_mode->vtotal;
	u16 vsync_len = adjusted_mode->vsync_end - adjusted_mode->vsync_start;
	u16 vact_st = adjusted_mode->vtotal - adjusted_mode->vsync_start;
	u16 vact_end = vact_st + vdisplay;
	uint32_t pin_pol, val;
	int ret;

	ret = vop_enable(crtc);
	if (ret) {
		DRM_DEV_ERROR(vop->dev, "Failed to enable vop (%d)\n", ret);
		return;
	}

	pin_pol = BIT(DCLK_INVERT);
	pin_pol |= (adjusted_mode->flags & DRM_MODE_FLAG_PHSYNC) ?
		   BIT(HSYNC_POSITIVE) : 0;
	pin_pol |= (adjusted_mode->flags & DRM_MODE_FLAG_PVSYNC) ?
		   BIT(VSYNC_POSITIVE) : 0;
	VOP_REG_SET(vop, output, pin_pol, pin_pol);
	VOP_REG_SET(vop, output, mipi_dual_channel_en, 0);

	switch (s->output_type) {
	case DRM_MODE_CONNECTOR_LVDS:
		VOP_REG_SET(vop, output, rgb_en, 1);
		VOP_REG_SET(vop, output, rgb_pin_pol, pin_pol);
		break;
	case DRM_MODE_CONNECTOR_eDP:
		VOP_REG_SET(vop, output, edp_pin_pol, pin_pol);
		VOP_REG_SET(vop, output, edp_en, 1);
		break;
	case DRM_MODE_CONNECTOR_HDMIA:
		VOP_REG_SET(vop, output, hdmi_pin_pol, pin_pol);
		VOP_REG_SET(vop, output, hdmi_en, 1);
		break;
	case DRM_MODE_CONNECTOR_DSI:
		VOP_REG_SET(vop, output, mipi_pin_pol, pin_pol);
		VOP_REG_SET(vop, output, mipi_en, 1);
		VOP_REG_SET(vop, output, mipi_dual_channel_en,
			   !!(s->output_flags & ROCKCHIP_OUTPUT_DSI_DUAL_CHANNEL));
		break;
	case DRM_MODE_CONNECTOR_DisplayPort:
		pin_pol &= ~BIT(DCLK_INVERT);
		VOP_REG_SET(vop, output, dp_pin_pol, pin_pol);
		VOP_REG_SET(vop, output, dp_en, 1);
		break;
	default:
		DRM_DEV_ERROR(vop->dev, "unsupported connector_type [%d]\n",
			      s->output_type);
	}
	if (s->output_mode == ROCKCHIP_OUT_MODE_AAAA && s->output_bpc == 8)
		VOP_REG_SET(vop, common, pre_dither_down, 1);
	else
		VOP_REG_SET(vop, common, pre_dither_down, 0);

	/*
	 * if vop is not support RGB10 output, need force RGB10 to RGB888.
	 */
	if (s->output_mode == ROCKCHIP_OUT_MODE_AAAA &&
	    !(vop_data->feature & VOP_FEATURE_OUTPUT_RGB10))
		s->output_mode = ROCKCHIP_OUT_MODE_P888;
	VOP_REG_SET(vop, common, out_mode, s->output_mode);

	VOP_REG_SET(vop, modeset, htotal_pw, (htotal << 16) | hsync_len);
	val = hact_st << 16;
	val |= hact_end;
	VOP_REG_SET(vop, modeset, hact_st_end, val);
	VOP_REG_SET(vop, modeset, hpost_st_end, val);

	VOP_REG_SET(vop, modeset, vtotal_pw, (vtotal << 16) | vsync_len);
	val = vact_st << 16;
	val |= vact_end;
	VOP_REG_SET(vop, modeset, vact_st_end, val);
	VOP_REG_SET(vop, modeset, vpost_st_end, val);

	VOP_REG_SET(vop, intr, line_flag_num[0], vact_end);

	clk_set_rate(vop->dclk, adjusted_mode->clock * 1000);

	VOP_REG_SET(vop, common, standby, 0);
	rockchip_dmcfreq_register_clk_sync_nb(priv->devfreq, &vop->dmc_nb);
}

static int dmc_notify(struct notifier_block *nb,
		      unsigned long action,
		      void *data)
{
	struct vop *vop = container_of(nb, struct vop, dmc_nb);
	struct drm_crtc *crtc = &vop->crtc;
	ktime_t *timeout = data;
	int ret;

	if (WARN_ON(!vop->is_enabled))
		return NOTIFY_BAD;

	ret = rockchip_drm_wait_vact_end(crtc, 100);
	*timeout = ktime_add_ns(vop->line_flag_timestamp,
				rockchip_drm_get_vblank_ns(&crtc->mode));
	if (ret) {
		dev_err(vop->dev, "%s: Line flag interrupt did not arrive\n",
			__func__);
		return NOTIFY_BAD;
	}

	return NOTIFY_STOP;
}

static int vop_crtc_atomic_check(struct drm_crtc *crtc,
				 struct drm_crtc_state *state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(state);
	uint32_t vblank_ns;

	vblank_ns = rockchip_drm_get_vblank_ns(&state->adjusted_mode);
	if (state->active && vblank_ns < DMC_MIN_VBLANK_NS)
		s->needs_dmcfreq_block = true;

	return 0;
}

static void vop_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_crtc_state)
{
	struct drm_atomic_state *old_state = old_crtc_state->state;
	struct drm_plane_state *old_plane_state;
	struct drm_device *drm = crtc->dev;
	struct vop *vop = to_vop(crtc);
	struct drm_plane *plane;
	unsigned long flags;
	int i;

	if (WARN_ON(!vop->is_enabled))
		return;

	spin_lock(&vop->reg_lock);

	VOP_AFBDC_SET(vop, enable, vop->afbdc_win != NULL);
	vop_cfg_done(vop);

	spin_unlock(&vop->reg_lock);

	/*
	 * There is a (rather unlikely) possibility that a vblank interrupt
	 * fired before we set the cfg_done bit. To avoid spuriously
	 * signalling flip completion we need to wait for it to finish.
	 */
	vop_wait_for_irq_handler(vop);

	spin_lock_irqsave(&drm->event_lock, flags);
	if (crtc->state->event) {
		vop->event = crtc->state->event;
		crtc->state->event = NULL;
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
	}
	spin_unlock_irqrestore(&drm->event_lock, flags);

	for_each_plane_in_state(old_state, plane, old_plane_state, i) {
		if (!old_plane_state->fb)
			continue;

		if (old_plane_state->fb == plane->state->fb)
			continue;

		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		drm_framebuffer_reference(old_plane_state->fb);
		drm_flip_work_queue(&vop->fb_unref_work, old_plane_state->fb);
		set_bit(VOP_PENDING_FB_UNREF, &vop->pending);
	}
}

static const struct drm_crtc_helper_funcs vop_crtc_helper_funcs = {
	.enable = vop_crtc_enable,
	.disable = vop_crtc_disable,
	.mode_fixup = vop_crtc_mode_fixup,
	.atomic_check = vop_crtc_atomic_check,
	.atomic_flush = vop_crtc_atomic_flush,
};

static void vop_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

static void vop_crtc_reset(struct drm_crtc *crtc)
{
	if (crtc->state)
		__drm_atomic_helper_crtc_destroy_state(crtc, crtc->state);
	kfree(crtc->state);

	crtc->state = kzalloc(sizeof(struct rockchip_crtc_state), GFP_KERNEL);
	if (crtc->state)
		crtc->state->crtc = crtc;
}

static struct drm_crtc_state *vop_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct rockchip_crtc_state *rockchip_state;

	rockchip_state = kzalloc(sizeof(*rockchip_state), GFP_KERNEL);
	if (!rockchip_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &rockchip_state->base);
	return &rockchip_state->base;
}

static void vop_crtc_destroy_state(struct drm_crtc *crtc,
				   struct drm_crtc_state *state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(crtc, &s->base);
	kfree(s);
}

static const struct drm_crtc_funcs vop_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = vop_crtc_destroy,
	.reset = vop_crtc_reset,
	.atomic_duplicate_state = vop_crtc_duplicate_state,
	.atomic_destroy_state = vop_crtc_destroy_state,
};

static void vop_fb_unref_worker(struct drm_flip_work *work, void *val)
{
	struct vop *vop = container_of(work, struct vop, fb_unref_work);
	struct drm_framebuffer *fb = val;

	drm_crtc_vblank_put(&vop->crtc);
	drm_framebuffer_unreference(fb);
}

static void vop_handle_vblank(struct vop *vop)
{
	struct drm_device *drm = vop->drm_dev;
	struct drm_crtc *crtc = &vop->crtc;
	unsigned long flags;

	spin_lock_irqsave(&drm->event_lock, flags);
	if (vop->event) {
		drm_crtc_send_vblank_event(crtc, vop->event);
		drm_crtc_vblank_put(crtc);
		vop->event = NULL;
	}
	spin_unlock_irqrestore(&drm->event_lock, flags);

	if (test_and_clear_bit(VOP_PENDING_FB_UNREF, &vop->pending))
		drm_flip_work_commit(&vop->fb_unref_work, system_unbound_wq);
}

static irqreturn_t vop_isr(int irq, void *data)
{
	struct vop *vop = data;
	struct drm_crtc *crtc = &vop->crtc;
	uint32_t active_irqs;
	unsigned long flags;
	int ret = IRQ_NONE;

	/*
	 * interrupt register has interrupt status, enable and clear bits, we
	 * must hold irq_lock to avoid a race with enable/disable_vblank().
	*/
	spin_lock_irqsave(&vop->irq_lock, flags);

	active_irqs = VOP_INTR_GET_TYPE(vop, status, INTR_MASK);
	/* Clear all active interrupt sources */
	if (active_irqs)
		VOP_INTR_SET_TYPE(vop, clear, active_irqs, 1);

	spin_unlock_irqrestore(&vop->irq_lock, flags);

	/* This is expected for vop iommu irqs, since the irq is shared */
	if (!active_irqs)
		return IRQ_NONE;

	if (active_irqs & DSP_HOLD_VALID_INTR) {
		complete(&vop->dsp_hold_completion);
		active_irqs &= ~DSP_HOLD_VALID_INTR;
		ret = IRQ_HANDLED;
	}

	if (active_irqs & LINE_FLAG_INTR) {
		vop->line_flag_timestamp = ktime_get();
		wake_up_all(&vop->line_flag_wq);
		active_irqs &= ~LINE_FLAG_INTR;
		ret = IRQ_HANDLED;
	}

	if (active_irqs & FS_INTR) {
		drm_crtc_handle_vblank(crtc);
		vop_handle_vblank(vop);
		active_irqs &= ~FS_INTR;
		ret = IRQ_HANDLED;
	}

	/* Unhandled irqs are spurious. */
	if (active_irqs)
		DRM_DEV_ERROR(vop->dev, "Unknown VOP IRQs: %#02x\n",
			      active_irqs);

	return ret;
}

static int vop_create_crtc(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;
	struct device *dev = vop->dev;
	struct drm_device *drm_dev = vop->drm_dev;
	struct drm_plane *primary = NULL, *cursor = NULL, *plane, *tmp;
	struct drm_crtc *crtc = &vop->crtc;
	struct device_node *port;
	struct drm_property *rotation_property =
		drm_dev->mode_config.rotation_property;
	int ret;
	int i;

	/*
	 * Create drm_plane for primary and cursor planes first, since we need
	 * to pass them to drm_crtc_init_with_planes, which sets the
	 * "possible_crtcs" to the newly initialized crtc.
	 */
	for (i = 0; i < vop_data->win_size; i++) {
		struct vop_win *vop_win = &vop->win[i];
		const struct vop_win_data *win_data = vop_win->data;

		if (win_data->type != DRM_PLANE_TYPE_PRIMARY &&
		    win_data->type != DRM_PLANE_TYPE_CURSOR)
			continue;

		ret = drm_universal_plane_init(vop->drm_dev, &vop_win->base,
					       0, &vop_plane_funcs,
					       win_data->phy->data_formats,
					       win_data->phy->nformats,
					       win_data->phy->format_modifiers,
					       win_data->type, NULL);
		if (ret) {
			DRM_DEV_ERROR(vop->dev, "failed to init plane %d\n",
				      ret);
			goto err_cleanup_planes;
		}

		plane = &vop_win->base;
		drm_plane_helper_add(plane, &plane_helper_funcs);
		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			primary = plane;
		else if (plane->type == DRM_PLANE_TYPE_CURSOR)
			cursor = plane;
	}

	ret = drm_crtc_init_with_planes(drm_dev, crtc, primary, cursor,
					&vop_crtc_funcs, NULL);
	drm_plane_enable_color_mgmt(cursor, true);

	drm_object_attach_property(&primary->base,
				   rotation_property,
				   DRM_ROTATE_0);

	drm_plane_enable_color_mgmt(primary, true);
	if (ret)
		goto err_cleanup_planes;

	drm_crtc_helper_add(crtc, &vop_crtc_helper_funcs);

	/*
	 * Create drm_planes for overlay windows with possible_crtcs restricted
	 * to the newly created crtc.
	 */
	for (i = 0; i < vop_data->win_size; i++) {
		struct vop_win *vop_win = &vop->win[i];
		const struct vop_win_data *win_data = vop_win->data;
		unsigned long possible_crtcs = 1 << drm_crtc_index(crtc);

		if (win_data->type != DRM_PLANE_TYPE_OVERLAY)
			continue;

		ret = drm_universal_plane_init(vop->drm_dev, &vop_win->base,
					       possible_crtcs,
					       &vop_plane_funcs,
					       win_data->phy->data_formats,
					       win_data->phy->nformats,
					       win_data->phy->format_modifiers,
					       win_data->type, NULL);
		if (ret) {
			DRM_DEV_ERROR(vop->dev, "failed to init overlay %d\n",
				      ret);
			goto err_cleanup_crtc;
		}
		drm_plane_helper_add(&vop_win->base, &plane_helper_funcs);
		drm_object_attach_property(&vop_win->base.base,
					   rotation_property,
					   DRM_ROTATE_0);
		drm_plane_enable_color_mgmt(&vop_win->base, true);
	}

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		DRM_DEV_ERROR(vop->dev, "no port node found in %s\n",
			      dev->of_node->full_name);
		ret = -ENOENT;
		goto err_cleanup_crtc;
	}

	drm_flip_work_init(&vop->fb_unref_work, "fb_unref",
			   vop_fb_unref_worker);

	init_completion(&vop->dsp_hold_completion);
	init_waitqueue_head(&vop->line_flag_wq);
	atomic_set(&vop->line_flag_wait_count, 0);
	crtc->port = port;
	rockchip_register_crtc_funcs(crtc, &private_crtc_funcs);

	return 0;

err_cleanup_crtc:
	drm_crtc_cleanup(crtc);
err_cleanup_planes:
	list_for_each_entry_safe(plane, tmp, &drm_dev->mode_config.plane_list,
				 head)
		drm_plane_cleanup(plane);
	return ret;
}

static void vop_destroy_crtc(struct vop *vop)
{
	struct drm_crtc *crtc = &vop->crtc;
	struct drm_device *drm_dev = vop->drm_dev;
	struct drm_plane *plane, *tmp;

	rockchip_unregister_crtc_funcs(crtc);
	of_node_put(crtc->port);

	/*
	 * We need to cleanup the planes now.  Why?
	 *
	 * The planes are "&vop->win[i].base".  That means the memory is
	 * all part of the big "struct vop" chunk of memory.  That memory
	 * was devm allocated and associated with this component.  We need to
	 * free it ourselves before vop_unbind() finishes.
	 */
	list_for_each_entry_safe(plane, tmp, &drm_dev->mode_config.plane_list,
				 head)
		vop_plane_destroy(plane);

	/*
	 * Destroy CRTC after vop_plane_destroy() since vop_disable_plane()
	 * references the CRTC.
	 */
	drm_crtc_cleanup(crtc);
	drm_flip_work_cleanup(&vop->fb_unref_work);
}

static int vop_initial(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;
	struct reset_control *ahb_rst, *axi_rst;
	int i, ret;

	vop->hclk = devm_clk_get(vop->dev, "hclk_vop");
	if (IS_ERR(vop->hclk)) {
		dev_err(vop->dev, "failed to get hclk source\n");
		return PTR_ERR(vop->hclk);
	}
	vop->aclk = devm_clk_get(vop->dev, "aclk_vop");
	if (IS_ERR(vop->aclk)) {
		dev_err(vop->dev, "failed to get aclk source\n");
		return PTR_ERR(vop->aclk);
	}
	vop->dclk = devm_clk_get(vop->dev, "dclk_vop");
	if (IS_ERR(vop->dclk)) {
		dev_err(vop->dev, "failed to get dclk source\n");
		return PTR_ERR(vop->dclk);
	}

	ret = pm_runtime_get_sync(vop->dev);
	if (ret < 0) {
		dev_err(vop->dev, "failed to get pm runtime: %d\n", ret);
		return ret;
	}

	ret = clk_prepare(vop->dclk);
	if (ret < 0) {
		dev_err(vop->dev, "failed to prepare dclk\n");
		goto err_put_pm_runtime;
	}

	/* Enable both the hclk and aclk to setup the vop */
	ret = clk_prepare_enable(vop->hclk);
	if (ret < 0) {
		dev_err(vop->dev, "failed to prepare/enable hclk\n");
		goto err_unprepare_dclk;
	}

	ret = clk_prepare_enable(vop->aclk);
	if (ret < 0) {
		dev_err(vop->dev, "failed to prepare/enable aclk\n");
		goto err_disable_hclk;
	}

	/*
	 * do aclk_reset, reset all vop registers.
	 */
	axi_rst = devm_reset_control_get(vop->dev, "axi");
	if (IS_ERR(axi_rst)) {
		dev_err(vop->dev, "failed to get axi reset\n");
		ret = PTR_ERR(axi_rst);
		goto err_disable_aclk;
	}
	reset_control_assert(axi_rst);
	usleep_range(10, 20);
	reset_control_deassert(axi_rst);

	/*
	 * do hclk_reset, reset all vop registers.
	 */
	ahb_rst = devm_reset_control_get(vop->dev, "ahb");
	if (IS_ERR(ahb_rst)) {
		dev_err(vop->dev, "failed to get ahb reset\n");
		ret = PTR_ERR(ahb_rst);
		goto err_disable_aclk;
	}
	reset_control_assert(ahb_rst);
	usleep_range(10, 20);
	reset_control_deassert(ahb_rst);

	memcpy(vop->regsbak, vop->regs, vop->len);

	VOP_REG_SET(vop, misc, global_regdone_en, 1);
	VOP_REG_SET(vop, common, dsp_blank, 0);

	for (i = 0; i < vop_data->win_size; i++) {
		const struct vop_win_data *win = &vop_data->win[i];

		VOP_WIN_SET(vop, win, enable, 0);
		VOP_WIN_SET(vop, win, gate, 1);
		vop->win_enabled &= ~BIT(i);
	}

	vop_cfg_done(vop);

	/*
	 * do dclk_reset, let all config take affect.
	 */
	vop->dclk_rst = devm_reset_control_get(vop->dev, "dclk");
	if (IS_ERR(vop->dclk_rst)) {
		dev_err(vop->dev, "failed to get dclk reset\n");
		ret = PTR_ERR(vop->dclk_rst);
		goto err_disable_aclk;
	}
	reset_control_assert(vop->dclk_rst);
	usleep_range(10, 20);
	reset_control_deassert(vop->dclk_rst);

	clk_disable(vop->hclk);
	clk_disable(vop->aclk);

	vop->is_enabled = false;

	pm_runtime_put_sync(vop->dev);

	return 0;

err_disable_aclk:
	clk_disable_unprepare(vop->aclk);
err_disable_hclk:
	clk_disable_unprepare(vop->hclk);
err_unprepare_dclk:
	clk_unprepare(vop->dclk);
err_put_pm_runtime:
	pm_runtime_put_sync(vop->dev);
	return ret;
}

/*
 * Initialize the vop->win array elements.
 */
static void vop_win_init(struct vop *vop)
{
	const struct vop_data *vop_data = vop->data;
	unsigned int i;

	for (i = 0; i < vop_data->win_size; i++) {
		struct vop_win *vop_win = &vop->win[i];
		const struct vop_win_data *win_data = &vop_data->win[i];

		vop_win->data = win_data;
		vop_win->vop = vop;
		vop_win->yuv2yuv_data = &vop_data->win_yuv2yuv[i];
	}
}

/**
 * rockchip_drm_wait_vact_end
 * @crtc: CRTC to enable line flag
 * @mstimeout: millisecond for timeout
 *
 * Wait for vact_end line flag irq or timeout.
 * Must be called from a context that is mutually exclusive with VOP
 * enable/disable.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int rockchip_drm_wait_vact_end(struct drm_crtc *crtc, unsigned int mstimeout)
{
	struct vop *vop = to_vop(crtc);
	unsigned long jiffies_left;
	DEFINE_WAIT(wait);

	if (!crtc || !vop->is_enabled)
		return -ENODEV;

	if (mstimeout <= 0)
		return -EINVAL;

	vop_line_flag_irq_get(vop);

	prepare_to_wait(&vop->line_flag_wq, &wait, TASK_UNINTERRUPTIBLE);
	jiffies_left = schedule_timeout(msecs_to_jiffies(mstimeout));
	finish_wait(&vop->line_flag_wq, &wait);

	vop_line_flag_irq_put(vop);

	if (jiffies_left == 0) {
		dev_err(vop->dev, "Timeout waiting for IRQ\n");
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_wait_vact_end);

static int vop_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	const struct vop_data *vop_data;
	struct drm_device *drm_dev = data;
	struct vop *vop;
	struct resource *res;
	size_t alloc_size;
	int ret, irq;

	vop_data = of_device_get_match_data(dev);
	if (!vop_data)
		return -ENODEV;

	/* Allocate vop struct and its vop_win array */
	alloc_size = sizeof(*vop) + sizeof(*vop->win) * vop_data->win_size;
	vop = devm_kzalloc(dev, alloc_size, GFP_KERNEL);
	if (!vop)
		return -ENOMEM;

	vop->dev = dev;
	vop->data = vop_data;
	vop->drm_dev = drm_dev;
	dev_set_drvdata(dev, vop);

	vop_win_init(vop);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	vop->len = resource_size(res);
	vop->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(vop->regs))
		return PTR_ERR(vop->regs);

	vop->regsbak = devm_kzalloc(dev, vop->len, GFP_KERNEL);
	if (!vop->regsbak)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "cannot find irq for vop\n");
		return irq;
	}
	vop->irq = (unsigned int)irq;

	spin_lock_init(&vop->reg_lock);
	spin_lock_init(&vop->irq_lock);
	vop->dmc_nb.notifier_call = dmc_notify;

	ret = devm_request_irq(dev, vop->irq, vop_isr,
			       IRQF_SHARED, dev_name(dev), vop);
	if (ret)
		return ret;

	/* IRQ is initially disabled; it gets enabled in power_on */
	disable_irq(vop->irq);

	ret = vop_create_crtc(vop);
	if (ret)
		goto err_enable_irq;

	pm_runtime_enable(&pdev->dev);

	ret = vop_initial(vop);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot initial vop dev - err %d\n", ret);
		goto err_disable_pm_runtime;
	}

	return 0;

err_disable_pm_runtime:
	pm_runtime_disable(&pdev->dev);
	vop_destroy_crtc(vop);
err_enable_irq:
	enable_irq(vop->irq); /* To balance out the disable_irq above */
	return ret;
}

static void vop_unbind(struct device *dev, struct device *master, void *data)
{
	struct vop *vop = dev_get_drvdata(dev);

	pm_runtime_disable(dev);
	vop_destroy_crtc(vop);

	clk_unprepare(vop->aclk);
	clk_unprepare(vop->hclk);
	clk_unprepare(vop->dclk);
}

const struct component_ops vop_component_ops = {
	.bind = vop_bind,
	.unbind = vop_unbind,
};
EXPORT_SYMBOL_GPL(vop_component_ops);
