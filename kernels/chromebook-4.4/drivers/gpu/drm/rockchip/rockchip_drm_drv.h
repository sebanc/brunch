/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author:Mark Yao <mark.yao@rock-chips.com>
 *
 * based on exynos_drm_drv.h
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

#ifndef _ROCKCHIP_DRM_DRV_H
#define _ROCKCHIP_DRM_DRV_H

#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem.h>

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/component.h>

#define ROCKCHIP_MAX_FB_BUFFER	3
#define ROCKCHIP_MAX_CONNECTOR	2
#define ROCKCHIP_MAX_CRTC	2

struct drm_device;
struct drm_connector;
struct iommu_domain;

/*
 * Rockchip drm private crtc funcs.
 * @enable_vblank: enable crtc vblank irq.
 * @disable_vblank: disable crtc vblank irq.
 */
struct rockchip_crtc_funcs {
	int (*enable_vblank)(struct drm_crtc *crtc);
	void (*disable_vblank)(struct drm_crtc *crtc);
	void (*cancel_pending_vblank)(struct drm_crtc *crtc, struct drm_file *file_priv);
};

struct rockchip_atomic_commit {
	/* kthread worker for non-blocking commit completion */
	struct kthread_work work;
	struct kthread_worker worker;
	struct task_struct *thread;

	struct drm_atomic_state *state;
	struct drm_device *dev;
	struct mutex lock;
	struct mutex hw_lock;
	bool needs_modeset;
	bool has_cursor_plane;
};

struct rockchip_crtc_state {
	struct drm_crtc_state base;
	int output_type;
	int output_mode;
	int output_flags;
	int output_bpc;
	bool needs_dmcfreq_block;
};
#define to_rockchip_crtc_state(s) \
		container_of(s, struct rockchip_crtc_state, base)

/*
 * Rockchip drm private structure.
 *
 * @crtc: array of enabled CRTCs, used to map from "pipe" to drm_crtc.
 * @num_pipe: number of pipes for this device.
 * @mm_lock: protect drm_mm on multi-threads.
 */
struct rockchip_drm_private {
	struct drm_fb_helper fbdev_helper;
	struct drm_gem_object *fbdev_bo;
	const struct rockchip_crtc_funcs *crtc_funcs[ROCKCHIP_MAX_CRTC];

	struct rockchip_atomic_commit commit;
	struct iommu_domain *domain;

	struct mutex mm_lock;
	struct drm_mm mm;

	struct list_head psr_list;
	struct mutex psr_list_lock;

	struct devfreq *devfreq;
	struct devfreq_event_dev *devfreq_event_dev;
	struct drm_atomic_state *state;
};

uint32_t rockchip_drm_get_vblank_ns(struct drm_display_mode *mode);
void rockchip_drm_atomic_work(struct kthread_work *work);
int rockchip_register_crtc_funcs(struct drm_crtc *crtc,
				 const struct rockchip_crtc_funcs *crtc_funcs);
void rockchip_unregister_crtc_funcs(struct drm_crtc *crtc);
int rockchip_drm_dma_attach_device(struct drm_device *drm_dev,
				   struct device *dev);
void rockchip_drm_dma_detach_device(struct drm_device *drm_dev,
				    struct device *dev);
int rockchip_drm_wait_vact_end(struct drm_crtc *crtc, unsigned int mstimeout);

void rockchip_drm_set_win_enabled(struct drm_crtc *ctrc, bool enabled);

#ifdef CONFIG_ROCKCHIP_CDN_DP
extern bool is_connector_cdn_dp(struct drm_connector *connector);
extern void cdn_dp_hdcp_atomic_disable(struct drm_connector *connector);
extern void cdn_dp_hdcp_atomic_enable(struct drm_connector *connector);
#else
static inline bool is_connector_cdn_dp(struct drm_connector *encoder)
{
	return false;
}
static inline void cdn_dp_hdcp_atomic_disable(struct drm_connector *connector)
{
}
static inline void cdn_dp_hdcp_atomic_enable(struct drm_connector *connector)
{
}
#endif

extern struct platform_driver cdn_dp_driver;
extern struct platform_driver dw_hdmi_rockchip_pltfm_driver;
extern struct platform_driver dw_mipi_dsi_rockchip_driver;
extern struct platform_driver inno_hdmi_driver;
extern struct platform_driver rockchip_dp_driver;
extern struct platform_driver vop_platform_driver;
#endif /* _ROCKCHIP_DRM_DRV_H_ */
