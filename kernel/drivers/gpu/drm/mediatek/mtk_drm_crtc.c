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

#include <asm/barrier.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"

/**
 * struct mtk_drm_crtc - MediaTek specific crtc structure.
 * @base: crtc object.
 * @enabled: records whether crtc_enable succeeded
 * @planes: array of 4 drm_plane structures, one for each overlay plane
 * @pending_planes: whether any plane has pending changes to be applied
 * @config_regs: memory mapped mmsys configuration register space
 * @mutex: handle to one of the ten disp_mutex streams
 * @ddp_comp_nr: number of components in ddp_comp
 * @ddp_comp: array of pointers the mtk_ddp_comp structures used by this crtc
 */
struct mtk_drm_crtc {
	struct drm_crtc			base;
	bool				enabled;

	bool				pending_needs_vblank;
	struct drm_pending_vblank_event	*event;

	struct drm_plane		*planes;
	unsigned int			layer_nr;
	bool				pending_planes;
	bool                            cursor_update;

	struct cmdq_client		*cmdq_client;
	u32				cmdq_event;

	void __iomem			*config_regs;
	const struct mtk_mmsys_reg_data *mmsys_reg_data;
	struct mtk_disp_mutex		*mutex;
	unsigned int			ddp_comp_nr;
	struct mtk_ddp_comp		**ddp_comp;

	struct drm_crtc_state		*old_crtc_state;
};

struct mtk_crtc_state {
	struct drm_crtc_state		base;

	bool				pending_config;
	unsigned int			pending_width;
	unsigned int			pending_height;
	unsigned int			pending_vrefresh;
	struct cmdq_pkt			*cmdq_handle;
};

struct mtk_cmdq_cb_data {
	struct drm_crtc_state		*state;
	struct cmdq_pkt			*cmdq_handle;
};

static inline struct mtk_drm_crtc *to_mtk_crtc(struct drm_crtc *c)
{
	return container_of(c, struct mtk_drm_crtc, base);
}

static inline struct mtk_crtc_state *to_mtk_crtc_state(struct drm_crtc_state *s)
{
	return container_of(s, struct mtk_crtc_state, base);
}

static void mtk_drm_crtc_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	drm_crtc_send_vblank_event(crtc, mtk_crtc->event);
	drm_crtc_vblank_put(crtc);
	mtk_crtc->event = NULL;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

static void mtk_drm_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	drm_crtc_handle_vblank(&mtk_crtc->base);
	if (mtk_crtc->pending_needs_vblank) {
		mtk_drm_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}
}

static void mtk_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	mtk_disp_mutex_put(mtk_crtc->mutex);

	drm_crtc_cleanup(crtc);
}

static void mtk_drm_crtc_reset(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state;

	if (crtc->state) {
		__drm_atomic_helper_crtc_destroy_state(crtc->state);

		state = to_mtk_crtc_state(crtc->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		crtc->state = &state->base;
	}

	state->base.crtc = crtc;
}

static struct drm_crtc_state *mtk_drm_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	WARN_ON(state->base.crtc != crtc);
	state->base.crtc = crtc;

	return &state->base;
}

static void mtk_drm_crtc_destroy_state(struct drm_crtc *crtc,
				       struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_mtk_crtc_state(state));
}

static bool mtk_drm_crtc_mode_fixup(struct drm_crtc *crtc,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	/* Nothing to do here, but this callback is mandatory. */
	return true;
}

static void mtk_drm_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);

	state->pending_width = crtc->mode.hdisplay;
	state->pending_height = crtc->mode.vdisplay;
	state->pending_vrefresh = crtc->mode.vrefresh;
	wmb();	/* Make sure the above parameters are set before update */
	state->pending_config = true;
}

static int mtk_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];

	mtk_ddp_comp_enable_vblank(comp, &mtk_crtc->base);

	return 0;
}

static void mtk_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];

	mtk_ddp_comp_disable_vblank(comp);
}

static int mtk_crtc_ddp_clk_enable(struct mtk_drm_crtc *mtk_crtc)
{
	int ret;
	int i;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++) {
		ret = clk_prepare_enable(mtk_crtc->ddp_comp[i]->clk);
		if (ret) {
			DRM_ERROR("Failed to enable clock %d: %d\n", i, ret);
			goto err;
		}
	}

	return 0;
err:
	while (--i >= 0)
		clk_disable_unprepare(mtk_crtc->ddp_comp[i]->clk);
	return ret;
}

static void mtk_crtc_ddp_clk_disable(struct mtk_drm_crtc *mtk_crtc)
{
	int i;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
		clk_disable_unprepare(mtk_crtc->ddp_comp[i]->clk);
}

static void ddp_cmdq_cursor_cb(struct cmdq_cb_data data)
{

#if IS_ENABLED(CONFIG_MTK_CMDQ)
	struct mtk_cmdq_cb_data *cb_data = data.data;

	DRM_DEBUG_DRIVER("%s\n", __func__);

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
#endif

}

static void ddp_cmdq_cb(struct cmdq_cb_data data)
{

#if IS_ENABLED(CONFIG_MTK_CMDQ)
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct drm_crtc_state *crtc_state = cb_data->state;
	struct drm_atomic_state *atomic_state = crtc_state->state;
	struct drm_crtc *crtc = crtc_state->crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	DRM_DEBUG_DRIVER("%s\n", __func__);

	if (mtk_crtc->pending_needs_vblank) {
		/* cmdq_vblank_event must be read after cmdq_needs_event */
		smp_rmb();

		mtk_drm_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}
	mtk_atomic_state_put_queue(atomic_state);
	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
#endif

}

static int mtk_crtc_ddp_hw_init(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_connector_list_iter conn_iter;
	unsigned int width, height, vrefresh, bpc = MTK_MAX_BPC;
	int ret;
	int i;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	if (WARN_ON(!crtc->state))
		return -EINVAL;

	width = crtc->state->adjusted_mode.hdisplay;
	height = crtc->state->adjusted_mode.vdisplay;
	vrefresh = crtc->state->adjusted_mode.vrefresh;

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;

		drm_connector_list_iter_begin(crtc->dev, &conn_iter);
		drm_for_each_connector_iter(connector, &conn_iter) {
			if (connector->encoder != encoder)
				continue;
			if (connector->display_info.bpc != 0 &&
			    bpc > connector->display_info.bpc)
				bpc = connector->display_info.bpc;
		}
		drm_connector_list_iter_end(&conn_iter);
	}

	ret = pm_runtime_get_sync(crtc->dev->dev);
	if (ret < 0) {
		DRM_ERROR("Failed to enable power domain: %d\n", ret);
		return ret;
	}

	ret = mtk_disp_mutex_prepare(mtk_crtc->mutex);
	if (ret < 0) {
		DRM_ERROR("Failed to enable mutex clock: %d\n", ret);
		goto err_pm_runtime_put;
	}

	ret = mtk_crtc_ddp_clk_enable(mtk_crtc);
	if (ret < 0) {
		DRM_ERROR("Failed to enable component clocks: %d\n", ret);
		goto err_mutex_unprepare;
	}

	DRM_DEBUG_DRIVER("mediatek_ddp_ddp_path_setup\n");
	for (i = 0; i < mtk_crtc->ddp_comp_nr - 1; i++) {
		mtk_ddp_add_comp_to_path(mtk_crtc->config_regs,
					 mtk_crtc->mmsys_reg_data,
					 mtk_crtc->ddp_comp[i]->id,
					 mtk_crtc->ddp_comp[i + 1]->id);
		mtk_disp_mutex_add_comp(mtk_crtc->mutex,
					mtk_crtc->ddp_comp[i]->id);
	}
	mtk_disp_mutex_add_comp(mtk_crtc->mutex, mtk_crtc->ddp_comp[i]->id);
	mtk_disp_mutex_enable(mtk_crtc->mutex);

	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++) {
		struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[i];
		enum mtk_ddp_comp_id prev;

		if (i > 0)
			prev = mtk_crtc->ddp_comp[i - 1]->id;
		else
			prev = DDP_COMPONENT_ID_MAX;

		if (prev == DDP_COMPONENT_OVL0)
			mtk_ddp_comp_bgclr_in_on(comp);

		mtk_ddp_comp_config(comp, width, height,
				    vrefresh, bpc, NULL);
		mtk_ddp_comp_start(comp);
	}

	/* Initially configure all planes */
	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i];
		struct mtk_plane_state *plane_state;
		struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];
		unsigned int comp_layer_nr = mtk_ddp_comp_layer_nr(comp);
		unsigned int local_layer;

		plane_state = to_mtk_plane_state(plane->state);

		if (i >= comp_layer_nr) {
			comp = mtk_crtc->ddp_comp[1];
			local_layer = i - comp_layer_nr;
		} else
			local_layer = i;
		mtk_ddp_comp_layer_config(comp, local_layer,
					  plane_state, NULL);
	}

	return 0;

err_mutex_unprepare:
	mtk_disp_mutex_unprepare(mtk_crtc->mutex);
err_pm_runtime_put:
	pm_runtime_put(crtc->dev->dev);
	return ret;
}

static void mtk_crtc_ddp_hw_fini(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_device *drm = mtk_crtc->base.dev;
	int i;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
		mtk_ddp_comp_stop(mtk_crtc->ddp_comp[i]);
	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
		mtk_disp_mutex_remove_comp(mtk_crtc->mutex,
					   mtk_crtc->ddp_comp[i]->id);
	mtk_disp_mutex_disable(mtk_crtc->mutex);
	for (i = 0; i < mtk_crtc->ddp_comp_nr - 1; i++) {
		mtk_ddp_comp_bgclr_in_off(mtk_crtc->ddp_comp[i]);
		mtk_ddp_remove_comp_from_path(mtk_crtc->config_regs,
					      mtk_crtc->mmsys_reg_data,
					      mtk_crtc->ddp_comp[i]->id,
					      mtk_crtc->ddp_comp[i + 1]->id);
		mtk_disp_mutex_remove_comp(mtk_crtc->mutex,
					   mtk_crtc->ddp_comp[i]->id);
	}
	mtk_disp_mutex_remove_comp(mtk_crtc->mutex, mtk_crtc->ddp_comp[i]->id);
	mtk_crtc_ddp_clk_disable(mtk_crtc);
	mtk_disp_mutex_unprepare(mtk_crtc->mutex);

	pm_runtime_put(drm->dev);
}

static void mtk_crtc_ddp_config(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_atomic_state *atomic_state = mtk_crtc->old_crtc_state->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(mtk_crtc->base.state);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];
	unsigned int i;
	unsigned int comp_layer_nr = mtk_ddp_comp_layer_nr(comp);
	unsigned int local_layer;

	/*
	 * TODO: instead of updating the registers here, we should prepare
	 * working registers in atomic_commit and let the hardware command
	 * queue update module registers on vblank.
	 */
	if (state->pending_config) {
		mtk_ddp_comp_config(comp, state->pending_width,
				    state->pending_height,
				    state->pending_vrefresh, 0, NULL);

		state->pending_config = false;
	}

	if (mtk_crtc->pending_planes) {
		for (i = 0; i < mtk_crtc->layer_nr; i++) {
			struct drm_plane *plane = &mtk_crtc->planes[i];
			struct mtk_plane_state *plane_state;

			plane_state = to_mtk_plane_state(plane->state);

			if (plane_state->pending.config) {
				if (i >= comp_layer_nr) {
					comp = mtk_crtc->ddp_comp[1];
					local_layer = i - comp_layer_nr;
				} else
					local_layer = i;

				mtk_ddp_comp_layer_config(comp, local_layer,
							  plane_state, NULL);
				plane_state->pending.config = false;
			}
		}
		mtk_crtc->pending_planes = false;
		if (!mtk_crtc->cursor_update)
			mtk_atomic_state_put_queue(atomic_state);
		mtk_crtc->cursor_update = false;
	}
}

void mtk_drm_crtc_cursor_update(struct drm_crtc *crtc, struct drm_plane *plane,
				struct drm_plane_state *plane_state)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	const struct drm_plane_helper_funcs *plane_helper_funcs =
			plane->helper_private;
	int i;

	if (!mtk_crtc->enabled)
		return;

	mutex_lock(&priv->hw_lock);
	if (IS_ENABLED(CONFIG_MTK_CMDQ) && mtk_crtc->cmdq_client) {
		struct mtk_crtc_state *mtk_crtc_state =
				to_mtk_crtc_state(crtc->state);
		struct mtk_cmdq_cb_data *cb_data;

		mtk_crtc_state->cmdq_handle =
				cmdq_pkt_create(mtk_crtc->cmdq_client,
						PAGE_SIZE);
		cmdq_pkt_clear_event(mtk_crtc_state->cmdq_handle,
				     mtk_crtc->cmdq_event);
		cmdq_pkt_wfe(mtk_crtc_state->cmdq_handle, mtk_crtc->cmdq_event);
		plane_helper_funcs->atomic_update(plane, plane_state);
		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		cb_data->cmdq_handle = mtk_crtc_state->cmdq_handle;
		cmdq_pkt_flush_async(mtk_crtc_state->cmdq_handle,
				     ddp_cmdq_cursor_cb, cb_data);
	} else {
		plane_helper_funcs->atomic_update(plane, plane_state);

		for (i = 0; i < mtk_crtc->layer_nr; i++) {
			struct drm_plane *plane = &mtk_crtc->planes[i];
			struct mtk_plane_state *plane_state;

			plane_state = to_mtk_plane_state(plane->state);
			if (plane_state->pending.dirty) {
				plane_state->pending.config = true;
				plane_state->pending.dirty = false;
			}
		}
		mtk_crtc->pending_planes = true;
		mtk_crtc->cursor_update = true;
		if (priv->data->shadow_register) {
			mtk_disp_mutex_acquire(mtk_crtc->mutex);
			mtk_crtc_ddp_config(crtc);
			mtk_disp_mutex_release(mtk_crtc->mutex);
		}
	}
	mutex_unlock(&priv->hw_lock);
}

void mtk_drm_crtc_plane_update(struct drm_crtc *crtc, struct drm_plane *plane,
			       struct mtk_plane_state *state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *mtk_crtc_state = to_mtk_crtc_state(crtc_state);
	struct cmdq_pkt *cmdq_handle = mtk_crtc_state->cmdq_handle;
	unsigned int comp_layer_nr = mtk_ddp_comp_layer_nr(comp);
	unsigned int local_layer;
	unsigned int plane_index = plane - mtk_crtc->planes;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	if (mtk_crtc->cmdq_client) {
		if (plane_index >= comp_layer_nr) {
			comp = mtk_crtc->ddp_comp[1];
			local_layer = plane_index - comp_layer_nr;
		} else {
			local_layer = plane_index;
		}
		mtk_ddp_comp_layer_config(comp, local_layer, state,
					  cmdq_handle);
	}
}

static void mtk_drm_crtc_atomic_enable(struct drm_crtc *crtc,
				       struct drm_crtc_state *old_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];
	int ret;

	DRM_DEBUG_DRIVER("%s %d\n", __func__, crtc->base.id);

	ret = pm_runtime_get_sync(comp->dev);
	if (ret < 0)
		DRM_DEV_ERROR(comp->dev, "Failed to enable power domain: %d\n",
			      ret);

	ret = mtk_crtc_ddp_hw_init(mtk_crtc);
	if (ret) {
		pm_runtime_put(comp->dev);
		return;
	}

	drm_crtc_vblank_on(crtc);
	mtk_crtc->enabled = true;
}

static void mtk_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					struct drm_crtc_state *old_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];
	int i, ret;

	DRM_DEBUG_DRIVER("%s %d\n", __func__, crtc->base.id);
	if (!mtk_crtc->enabled)
		return;

	/* Set all pending plane state to disabled */
	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i];
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		plane_state->pending.enable = false;
		plane_state->pending.config = true;
	}
	mtk_crtc->pending_planes = true;

	/* Wait for planes to be disabled */
	drm_crtc_wait_one_vblank(crtc);

	drm_crtc_vblank_off(crtc);
	mtk_crtc_ddp_hw_fini(mtk_crtc);

	mtk_crtc->enabled = false;

	ret = pm_runtime_put(comp->dev);
	if (ret < 0)
		DRM_DEV_ERROR(comp->dev, "Failed to disable power domain: %d\n",
			      ret);
}

static void mtk_drm_crtc_atomic_begin(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->event && state->base.event)
		DRM_ERROR("new event while there is still a pending event\n");

	if (state->base.event) {
		state->base.event->pipe = drm_crtc_index(crtc);
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		mtk_crtc->event = state->base.event;
		state->base.event = NULL;
		/* Make sure the above parameter is set before update */
		smp_wmb();
		mtk_crtc->pending_needs_vblank = true;
	}
	if (IS_ENABLED(CONFIG_MTK_CMDQ) && mtk_crtc->cmdq_client) {
		state->cmdq_handle = cmdq_pkt_create(mtk_crtc->cmdq_client,
						     PAGE_SIZE);
		cmdq_pkt_clear_event(state->cmdq_handle, mtk_crtc->cmdq_event);
		cmdq_pkt_wfe(state->cmdq_handle, mtk_crtc->cmdq_event);
	}
}

static void mtk_drm_crtc_atomic_flush(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state)
{
	struct drm_atomic_state *old_atomic_state = old_crtc_state->state;
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc_state);
	struct cmdq_pkt *cmdq_handle = state->cmdq_handle;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_cmdq_cb_data *cb_data;
	unsigned int pending_planes = 0;
	int i;

	DRM_DEBUG_DRIVER("[CRTC:%u] [STATE:%p(%p)->%p(%p)]\n", crtc->base.id,
			 old_crtc_state, old_crtc_state->state,
			 crtc_state, crtc_state->state);

	if (IS_ENABLED(CONFIG_MTK_CMDQ) && mtk_crtc->cmdq_client) {
		drm_atomic_state_get(old_atomic_state);
		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		cb_data->state = old_crtc_state;
		cb_data->cmdq_handle = cmdq_handle;
		cmdq_pkt_flush_async(cmdq_handle, ddp_cmdq_cb, cb_data);

		return;
	}

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i];
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		if (plane_state->pending.dirty) {
			plane_state->pending.config = true;
			plane_state->pending.dirty = false;
			pending_planes |= BIT(i);
		}
	}

	if (pending_planes) {
		mtk_crtc->pending_planes = true;
		drm_atomic_state_get(old_atomic_state);
		mtk_crtc->old_crtc_state = old_crtc_state;
	}

	if (crtc->state->color_mgmt_changed)
		for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
			mtk_ddp_gamma_set(mtk_crtc->ddp_comp[i],
					  crtc->state, NULL);

	if (priv->data->shadow_register) {
		mtk_disp_mutex_acquire(mtk_crtc->mutex);
		mtk_crtc_ddp_config(crtc);
		mtk_disp_mutex_release(mtk_crtc->mutex);
	}
}

static const struct drm_crtc_funcs mtk_crtc_funcs = {
	.set_config		= drm_atomic_helper_set_config,
	.page_flip		= drm_atomic_helper_page_flip,
	.destroy		= mtk_drm_crtc_destroy,
	.reset			= mtk_drm_crtc_reset,
	.atomic_duplicate_state	= mtk_drm_crtc_duplicate_state,
	.atomic_destroy_state	= mtk_drm_crtc_destroy_state,
	.gamma_set		= drm_atomic_helper_legacy_gamma_set,
	.enable_vblank		= mtk_drm_crtc_enable_vblank,
	.disable_vblank		= mtk_drm_crtc_disable_vblank,
};

static const struct drm_crtc_helper_funcs mtk_crtc_helper_funcs = {
	.mode_fixup	= mtk_drm_crtc_mode_fixup,
	.mode_set_nofb	= mtk_drm_crtc_mode_set_nofb,
	.atomic_begin	= mtk_drm_crtc_atomic_begin,
	.atomic_flush	= mtk_drm_crtc_atomic_flush,
	.atomic_enable	= mtk_drm_crtc_atomic_enable,
	.atomic_disable	= mtk_drm_crtc_atomic_disable,
};

static int mtk_drm_crtc_init(struct drm_device *drm,
			     struct mtk_drm_crtc *mtk_crtc,
			     struct drm_plane *primary,
			     struct drm_plane *cursor, unsigned int pipe)
{
	int ret;

	ret = drm_crtc_init_with_planes(drm, &mtk_crtc->base, primary, cursor,
					&mtk_crtc_funcs, NULL);
	if (ret)
		goto err_cleanup_crtc;

	drm_crtc_helper_add(&mtk_crtc->base, &mtk_crtc_helper_funcs);

	return 0;

err_cleanup_crtc:
	drm_crtc_cleanup(&mtk_crtc->base);
	return ret;
}

void mtk_crtc_ddp_irq(struct drm_crtc *crtc, struct mtk_ddp_comp *comp)
{

	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (mtk_crtc->cmdq_client) {
		drm_crtc_handle_vblank(crtc);
	} else {
		if (!priv->data->shadow_register)
			mtk_crtc_ddp_config(crtc);
		mtk_drm_finish_page_flip(mtk_crtc);
	}
}

int mtk_drm_crtc_create(struct drm_device *drm_dev,
			const enum mtk_ddp_comp_id *path, unsigned int path_len)
{
	struct mtk_drm_private *priv = drm_dev->dev_private;
	struct device *dev = drm_dev->dev;
	struct mtk_drm_crtc *mtk_crtc;
	enum drm_plane_type type;
	unsigned int zpos;
	int pipe = priv->num_pipes;
	int ret;
	int i;

	if (!path)
		return 0;

	for (i = 0; i < path_len; i++) {
		enum mtk_ddp_comp_id comp_id = path[i];
		struct device_node *node;

		node = priv->comp_node[comp_id];
		if (!node) {
			dev_info(dev,
				 "Not creating crtc %d because component %d is disabled or missing\n",
				 pipe, comp_id);
			return 0;
		}
	}

	mtk_crtc = devm_kzalloc(dev, sizeof(*mtk_crtc), GFP_KERNEL);
	if (!mtk_crtc)
		return -ENOMEM;

	mtk_crtc->config_regs = priv->config_regs;
	mtk_crtc->mmsys_reg_data = priv->data->reg_data;
	mtk_crtc->ddp_comp_nr = path_len;
	mtk_crtc->ddp_comp = devm_kmalloc_array(dev, mtk_crtc->ddp_comp_nr,
						sizeof(*mtk_crtc->ddp_comp),
						GFP_KERNEL);
	if (!mtk_crtc->ddp_comp)
		return -ENOMEM;

	mtk_crtc->mutex = mtk_disp_mutex_get(priv->mutex_dev, pipe);
	if (IS_ERR(mtk_crtc->mutex)) {
		ret = PTR_ERR(mtk_crtc->mutex);
		dev_err(dev, "Failed to get mutex: %d\n", ret);
		return ret;
	}

	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++) {
		enum mtk_ddp_comp_id comp_id = path[i];
		struct mtk_ddp_comp *comp;
		struct device_node *node;

		node = priv->comp_node[comp_id];
		comp = priv->ddp_comp[comp_id];
		if (!comp) {
			dev_err(dev, "Component %pOF not initialized\n", node);
			ret = -ENODEV;
			return ret;
		}

		mtk_crtc->ddp_comp[i] = comp;
	}

	mtk_crtc->layer_nr = mtk_ddp_comp_layer_nr(mtk_crtc->ddp_comp[0]);
	if (mtk_crtc->ddp_comp_nr > 1) {
		struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[1];

		if (comp->funcs->bgclr_in_on)
			mtk_crtc->layer_nr += mtk_ddp_comp_layer_nr(comp);
	}
	mtk_crtc->planes = devm_kcalloc(dev, mtk_crtc->layer_nr,
					sizeof(struct drm_plane),
					GFP_KERNEL);

	for (zpos = 0; zpos < mtk_crtc->layer_nr; zpos++) {
		type = (zpos == 0) ? DRM_PLANE_TYPE_PRIMARY :
				(zpos == 1) ? DRM_PLANE_TYPE_CURSOR :
						DRM_PLANE_TYPE_OVERLAY;
		ret = mtk_plane_init(drm_dev, &mtk_crtc->planes[zpos],
				     BIT(pipe), type);
		if (ret)
			return ret;
	}

	ret = mtk_drm_crtc_init(drm_dev, mtk_crtc, &mtk_crtc->planes[0],
				mtk_crtc->layer_nr > 1 ? &mtk_crtc->planes[1] :
				NULL, pipe);
	if (ret < 0)
		return ret;
	drm_mode_crtc_set_gamma_size(&mtk_crtc->base, MTK_LUT_SIZE);
	drm_crtc_enable_color_mgmt(&mtk_crtc->base, 0, false, MTK_LUT_SIZE);
	priv->num_pipes++;

	if (IS_ENABLED(CONFIG_MTK_CMDQ)) {
		mtk_crtc->cmdq_client = cmdq_mbox_create(dev,
				drm_crtc_index(&mtk_crtc->base), 2000);
		of_property_read_u32_index(dev->of_node, "mediatek,gce-events",
					   drm_crtc_index(&mtk_crtc->base),
					   &mtk_crtc->cmdq_event);
		if (IS_ERR(mtk_crtc->cmdq_client)) {
			dev_dbg(dev, "mtk_crtc %d failed to create mailbox client, writing register by CPU now\n",
				drm_crtc_index(&mtk_crtc->base));
			mtk_crtc->cmdq_client = NULL;
		}
	}

	return 0;
}
