/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_DRM_DDP_COMP_H
#define MTK_DRM_DDP_COMP_H

#include <linux/io.h>
#include "mtk_drm_plane.h"

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct drm_crtc_state;

enum mtk_ddp_comp_type {
	MTK_DISP_OVL,
	MTK_DISP_RDMA,
	MTK_DISP_WDMA,
	MTK_DISP_COLOR,
	MTK_DISP_AAL,
	MTK_DISP_GAMMA,
	MTK_DISP_UFOE,
	MTK_DSI,
	MTK_DPI,
	MTK_DISP_PWM,
	MTK_DISP_MUTEX,
	MTK_DISP_OD,
	MTK_DISP_SPLIT,
	MTK_DDP_COMP_TYPE_MAX,
};

enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_SPLIT0,
	DDP_COMPONENT_SPLIT1,
	DDP_COMPONENT_ID_MAX,
};

struct mtk_ddp_comp;
struct cmdq_rec;
struct mtk_ddp_comp_funcs {
	void (*config)(struct mtk_ddp_comp *comp, unsigned int w,
		       unsigned int h, unsigned int vrefresh, unsigned int bpc,
		       struct cmdq_rec *handle);
	void (*prepare)(struct mtk_ddp_comp *comp);
	void (*unprepare)(struct mtk_ddp_comp *comp);
	void (*start)(struct mtk_ddp_comp *comp, struct cmdq_rec *handle);
	void (*stop)(struct mtk_ddp_comp *comp, struct cmdq_rec *handle);
	void (*enable_vblank)(struct mtk_ddp_comp *comp, struct drm_crtc *crtc,
			      struct cmdq_rec *handle);
	void (*disable_vblank)(struct mtk_ddp_comp *comp,
			       struct cmdq_rec *handle);
	void (*layer_on)(struct mtk_ddp_comp *comp, unsigned int idx,
			 struct cmdq_rec *handle);
	void (*layer_off)(struct mtk_ddp_comp *comp, unsigned int idx,
			  struct cmdq_rec *handle);
	void (*layer_config)(struct mtk_ddp_comp *comp, unsigned int idx,
			     struct mtk_plane_pending_state *pending,
			     struct cmdq_rec *handle);
	void (*gamma_set)(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state,
			  struct cmdq_rec *handle);
};

struct mtk_ddp_comp {
	struct clk *clk;
	void __iomem *regs;
	int irq;
	struct device *larb_dev;
	enum mtk_ddp_comp_id id;
	enum mtk_ddp_comp_type type;
	const struct mtk_ddp_comp_funcs *funcs;
	bool dual_dsi_mode;
};

static inline void mtk_ddp_comp_config(struct mtk_ddp_comp *comp,
				       unsigned int w, unsigned int h,
				       unsigned int vrefresh, unsigned int bpc,
				       struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->config)
		comp->funcs->config(comp, w, h, vrefresh, bpc, handle);
}

static inline void mtk_ddp_comp_prepare(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->prepare)
		comp->funcs->prepare(comp);
}

static inline void mtk_ddp_comp_unprepare(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->unprepare)
		comp->funcs->unprepare(comp);
}

static inline void mtk_ddp_comp_start(struct mtk_ddp_comp *comp,
				      struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->start)
		comp->funcs->start(comp, handle);
}

static inline void mtk_ddp_comp_stop(struct mtk_ddp_comp *comp,
				     struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->stop)
		comp->funcs->stop(comp, handle);
}

static inline void mtk_ddp_comp_enable_vblank(struct mtk_ddp_comp *comp,
					      struct drm_crtc *crtc,
					      struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->enable_vblank)
		comp->funcs->enable_vblank(comp, crtc, handle);
}

static inline void mtk_ddp_comp_disable_vblank(struct mtk_ddp_comp *comp,
					       struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->disable_vblank)
		comp->funcs->disable_vblank(comp, handle);
}

static inline void mtk_ddp_comp_layer_on(struct mtk_ddp_comp *comp,
					 unsigned int idx,
					 struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->layer_on)
		comp->funcs->layer_on(comp, idx, handle);
}

static inline void mtk_ddp_comp_layer_off(struct mtk_ddp_comp *comp,
					  unsigned int idx,
					  struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->layer_off)
		comp->funcs->layer_off(comp, idx, handle);
}

static inline void mtk_ddp_comp_layer_config(struct mtk_ddp_comp *comp,
					     unsigned int idx,
					     struct mtk_plane_pending_state *pending,
					     struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->layer_config)
		comp->funcs->layer_config(comp, idx, pending, handle);
}

static inline void mtk_ddp_gamma_set(struct mtk_ddp_comp *comp,
				     struct drm_crtc_state *state,
				     struct cmdq_rec *handle)
{
	if (comp->funcs && comp->funcs->gamma_set)
		comp->funcs->gamma_set(comp, state, handle);
}

int mtk_ddp_comp_get_id(struct device_node *node,
			enum mtk_ddp_comp_type comp_type);
int mtk_ddp_comp_init(struct device *dev, struct device_node *comp_node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs);
int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_ddp_comp_set_dual_dsi_mode(struct mtk_ddp_comp *comp,
				    bool dual_dsi_mode);

#endif /* MTK_DRM_DDP_COMP_H */
