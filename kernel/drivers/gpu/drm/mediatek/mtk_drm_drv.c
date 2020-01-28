/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: YT SHEN <yt.shen@mediatek.com>
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

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_of.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <drm/mediatek_drm.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"

#define DRIVER_NAME "mediatek"
#define DRIVER_DESC "Mediatek SoC DRM"
#define DRIVER_DATE "20150513"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0

struct mtk_atomic_state {
	struct drm_atomic_state base;
	struct list_head list;
	struct work_struct work;
};

static inline struct mtk_atomic_state *to_mtk_state(struct drm_atomic_state *s)
{
	return container_of(s, struct mtk_atomic_state, base);
}

void mtk_atomic_state_put_queue(struct drm_atomic_state *state)
{
	struct drm_device *drm = state->dev;
	struct mtk_drm_private *mtk_drm = drm->dev_private;
	struct mtk_atomic_state *mtk_state = to_mtk_state(state);
	unsigned long flags;

	spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	list_add_tail(&mtk_state->list, &mtk_drm->unreference.list);
	spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);

	schedule_work(&mtk_drm->unreference.work);
}

static uint32_t mtk_atomic_crtc_mask(struct drm_device *drm,
				     struct drm_atomic_state *state)
{
	uint32_t crtc_mask;
	int i;

	for (i = 0, crtc_mask = 0; i < drm->mode_config.num_crtc; i++) {
		struct drm_crtc *crtc = state->crtcs[i].ptr;

		if (crtc)
			crtc_mask |= (1 << drm_crtc_index(crtc));
	}

	return crtc_mask;
}

/*
 * Block until specified crtcs are no longer pending update, and atomically
 * mark them as pending update.
 */
static int mtk_atomic_get_crtcs(struct drm_device *drm,
				struct drm_atomic_state *state)
{
	struct mtk_drm_private *private = drm->dev_private;
	uint32_t crtc_mask, needs_modeset, has_cursor_plane;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i;
	int ret;

	crtc_mask = mtk_atomic_crtc_mask(drm, state);

	/*
	 * Allow cursor updates unless there is a pending modeset or cursor
	 * plane update.
	 */
	needs_modeset = 0;
	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (drm_atomic_crtc_needs_modeset(crtc_state)) {
			needs_modeset |= (1 << drm_crtc_index(crtc));
			break;
		}
	}

	has_cursor_plane = 0;
	for_each_new_plane_in_state(state, plane, plane_state, i) {
		if (plane_state->crtc && plane == plane_state->crtc->cursor) {
			has_cursor_plane |=
				(1 << drm_crtc_index(plane_state->crtc));
			break;
		}
	}

	/*
	 * Wait for all pending updates to complete for the set of crtcs being
	 * changed in this atomic commit
	 */
	spin_lock(&private->commit.crtcs_event.lock);
	ret = wait_event_interruptible_locked(private->commit.crtcs_event,
			!(private->commit.crtcs & crtc_mask));
	if (ret == 0)
		private->commit.crtcs |= crtc_mask;

	private->commit.flush_for_cursor |= needs_modeset | has_cursor_plane;
	spin_unlock(&private->commit.crtcs_event.lock);

	return ret;
}

/*
 * Mark specified crtcs as no longer pending update.
 */
static void mtk_atomic_put_crtcs(struct drm_device *drm,
				 struct drm_atomic_state *state)
{
	struct mtk_drm_private *private = drm->dev_private;
	uint32_t crtc_mask;

	crtc_mask = mtk_atomic_crtc_mask(drm, state);

	spin_lock(&private->commit.crtcs_event.lock);
	private->commit.crtcs &= ~crtc_mask;
	private->commit.flush_for_cursor &= ~crtc_mask;
	wake_up_all_locked(&private->commit.crtcs_event);
	spin_unlock(&private->commit.crtcs_event.lock);
}

static void mtk_unreference_work(struct work_struct *work)
{
	struct mtk_drm_private *mtk_drm = container_of(work,
			struct mtk_drm_private, unreference.work);
	unsigned long flags;
	struct mtk_atomic_state *state, *tmp;

	/*
	 * framebuffers cannot be unreferenced in atomic context.
	 * Therefore, only hold the spinlock when iterating unreference_list,
	 * and drop it when doing the unreference.
	 */
	spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	list_for_each_entry_safe(state, tmp, &mtk_drm->unreference.list, list) {
		list_del(&state->list);
		spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);
		drm_atomic_state_put(&state->base);
		spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	}
	spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);
}


static void mtk_atomic_schedule(struct drm_device *drm,
				struct drm_atomic_state *state)
{
	struct mtk_atomic_state *mtk_state = to_mtk_state(state);

	schedule_work(&mtk_state->work);
}

static void mtk_atomic_wait_for_fences(struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	int i;

	for_each_new_plane_in_state(state, plane, new_plane_state, i)
		mtk_fb_wait(new_plane_state->fb);
}

static void mtk_atomic_complete(struct drm_device *drm,
				struct drm_atomic_state *state)
{
	struct mtk_drm_private *private = drm->dev_private;
	mtk_atomic_wait_for_fences(state);
	/*
	 * Mediatek drm supports runtime PM, so plane registers cannot be
	 * written when their crtc is disabled.
	 *
	 * The comment for drm_atomic_helper_commit states:
	 *     For drivers supporting runtime PM the recommended sequence is
	 *
	 *     drm_atomic_helper_commit_modeset_disables(dev, state);
	 *     drm_atomic_helper_commit_modeset_enables(dev, state);
	 *     drm_atomic_helper_commit_planes(dev, state,
	 *                                     DRM_PLANE_COMMIT_ACTIVE_ONLY);
	 *
	 * See the kerneldoc entries for these three functions for more details.
	 */
	mutex_lock(&private->hw_lock);
	drm_atomic_helper_commit_modeset_disables(drm, state);
	drm_atomic_helper_commit_modeset_enables(drm, state);
	drm_atomic_helper_commit_planes(drm, state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);
	mutex_unlock(&private->hw_lock);

	drm_atomic_helper_wait_for_vblanks(drm, state);

	drm_atomic_helper_cleanup_planes(drm, state);
	mtk_atomic_put_crtcs(drm, state);

	drm_atomic_state_put(state);
}

static void mtk_atomic_work(struct work_struct *work)
{
	struct mtk_atomic_state *mtk_state = container_of(work,
			struct mtk_atomic_state, work);
	struct drm_atomic_state *state = &mtk_state->base;
	struct drm_device *drm = state->dev;

	mtk_atomic_complete(drm, state);
}

static int mtk_atomic_commit(struct drm_device *drm,
			     struct drm_atomic_state *state,
			     bool async)
{
	int ret;

	ret = drm_atomic_helper_prepare_planes(drm, state);
	if (ret)
		return ret;

	ret = mtk_atomic_get_crtcs(drm, state);
	if (ret) {
		drm_atomic_helper_cleanup_planes(drm, state);
		return ret;
	}

	ret = drm_atomic_helper_swap_state(state, true);
	if (ret) {
		drm_atomic_helper_cleanup_planes(drm, state);
		return ret;
	}

	drm_atomic_state_get(state);
	if (async)
		mtk_atomic_schedule(drm, state);
	else
		mtk_atomic_complete(drm, state);

	return 0;
}

static struct drm_atomic_state *mtk_drm_atomic_state_alloc(
		struct drm_device *dev)
{
	struct mtk_atomic_state *mtk_state;

	mtk_state = kzalloc(sizeof(*mtk_state), GFP_KERNEL);
	if (!mtk_state)
		return NULL;

	if (drm_atomic_state_init(dev, &mtk_state->base) < 0) {
		kfree(mtk_state);
		return NULL;
	}

	INIT_LIST_HEAD(&mtk_state->list);
	INIT_WORK(&mtk_state->work, mtk_atomic_work);

	return &mtk_state->base;
}

static void mtk_drm_atomic_state_free(struct drm_atomic_state *state)
{
	struct mtk_atomic_state *mtk_state = to_mtk_state(state);

	drm_atomic_state_default_release(state);
	kfree(mtk_state);
}

static const struct drm_mode_config_funcs mtk_drm_mode_config_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = mtk_atomic_commit,
	.atomic_state_alloc = mtk_drm_atomic_state_alloc,
	.atomic_state_free = mtk_drm_atomic_state_free
};

static const enum mtk_ddp_comp_id mt2701_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_DSI0,
};

static const enum mtk_ddp_comp_id mt2701_mtk_ddp_ext[] = {
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI0,
};

static const enum mtk_ddp_comp_id mt2712_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt2712_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_PWM1,
};

static const enum mtk_ddp_comp_id mt2712_mtk_ddp_third[] = {
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_DSI3,
	DDP_COMPONENT_PWM2,
};

static const enum mtk_ddp_comp_id mt8173_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt8173_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI0,
};

static const enum mtk_ddp_comp_id mt8183_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL_2L0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR,
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_DITHER,
	DDP_COMPONENT_DSI0,
};

static const enum mtk_ddp_comp_id mt8183_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL_2L1,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI0,
};

static const struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.main_path = mt2701_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt2701_mtk_ddp_main),
	.ext_path = mt2701_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt2701_mtk_ddp_ext),
	.reg_data = &mt2701_mmsys_reg_data,
	.shadow_register = true,
};

static const struct mtk_mmsys_driver_data mt2712_mmsys_driver_data = {
	.main_path = mt2712_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt2712_mtk_ddp_main),
	.ext_path = mt2712_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt2712_mtk_ddp_ext),
	.third_path = mt2712_mtk_ddp_third,
	.third_len = ARRAY_SIZE(mt2712_mtk_ddp_third),
	.reg_data = &mt8173_mmsys_reg_data,
};

static const struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.main_path = mt8173_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt8173_mtk_ddp_main),
	.ext_path = mt8173_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt8173_mtk_ddp_ext),
	.reg_data = &mt8173_mmsys_reg_data,
};

static const struct mtk_mmsys_driver_data mt8183_mmsys_driver_data = {
	.main_path = mt8183_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt8183_mtk_ddp_main),
	.ext_path = mt8183_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt8183_mtk_ddp_ext),
	.reg_data = &mt8183_mmsys_reg_data,
};

static int mtk_drm_kms_init(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;
	struct platform_device *pdev;
	struct device_node *np;
	struct device *dma_dev;
	int ret;

	if (!iommu_present(&platform_bus_type))
		return -EPROBE_DEFER;

	pdev = of_find_device_by_node(private->mutex_node);
	if (!pdev) {
		dev_err(drm->dev, "Waiting for disp-mutex device %pOF\n",
			private->mutex_node);
		of_node_put(private->mutex_node);
		return -EPROBE_DEFER;
	}
	private->mutex_dev = &pdev->dev;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 64;
	drm->mode_config.min_height = 64;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;
	drm->mode_config.funcs = &mtk_drm_mode_config_funcs;

	ret = component_bind_all(drm->dev, drm);
	if (ret)
		goto err_config_cleanup;

	/*
	 * We currently support two fixed data streams, each optional,
	 * and each statically assigned to a crtc:
	 * OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0 ...
	 */
	ret = mtk_drm_crtc_create(drm, private->data->main_path,
				  private->data->main_len);
	if (ret < 0)
		goto err_component_unbind;
	/* ... and OVL1 -> COLOR1 -> GAMMA -> RDMA1 -> DPI0. */
	ret = mtk_drm_crtc_create(drm, private->data->ext_path,
				  private->data->ext_len);
	if (ret < 0)
		goto err_component_unbind;

	ret = mtk_drm_crtc_create(drm, private->data->third_path,
				  private->data->third_len);
	if (ret < 0)
		goto err_component_unbind;

	/* Use OVL device for all DMA memory allocations */
	np = private->comp_node[private->data->main_path[0]] ?:
	     private->comp_node[private->data->ext_path[0]];
	pdev = of_find_device_by_node(np);
	if (!pdev) {
		ret = -ENODEV;
		dev_err(drm->dev, "Need at least one OVL device\n");
		goto err_component_unbind;
	}

	dma_dev = &pdev->dev;
	private->dma_dev = dma_dev;

	/*
	 * Configure the DMA segment size to make sure we get contiguous IOVA
	 * when importing PRIME buffers.
	 */
	if (!dma_dev->dma_parms) {
		private->dma_parms_allocated = true;
		dma_dev->dma_parms =
			devm_kzalloc(drm->dev, sizeof(*dma_dev->dma_parms),
				     GFP_KERNEL);
	}
	if (!dma_dev->dma_parms) {
		ret = -ENOMEM;
		goto err_component_unbind;
	}

	ret = dma_set_max_seg_size(dma_dev, (unsigned int)DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dma_dev, "Failed to set DMA segment size\n");
		goto err_unset_dma_parms;
	}

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	drm->irq_enabled = true;
	ret = drm_vblank_init(drm, MAX_CRTC);
	if (ret < 0)
		goto err_unset_dma_parms;

	drm_kms_helper_poll_init(drm);
	drm_mode_config_reset(drm);

	INIT_WORK(&private->unreference.work, mtk_unreference_work);
	INIT_LIST_HEAD(&private->unreference.list);
	spin_lock_init(&private->unreference.lock);
	init_waitqueue_head(&private->commit.crtcs_event);
	mutex_init(&private->hw_lock);

	return 0;

err_unset_dma_parms:
	if (private->dma_parms_allocated)
		dma_dev->dma_parms = NULL;
err_component_unbind:
	component_unbind_all(drm->dev, drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);

	return ret;
}

static void mtk_drm_kms_deinit(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;

	drm_kms_helper_poll_fini(drm);
	drm_atomic_helper_shutdown(drm);

	if (private->dma_parms_allocated)
		private->dma_dev->dma_parms = NULL;

	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);
}

static const struct drm_ioctl_desc mtk_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MTK_GEM_CREATE, mtk_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GEM_MAP_OFFSET,
			  mtk_gem_map_offset_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
};

static const struct file_operations mtk_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = mtk_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.compat_ioctl = drm_compat_ioctl,
};

/*
 * We need to override this because the device used to import the memory is
 * not dev->dev, as drm_gem_prime_import() expects.
 */
struct drm_gem_object *mtk_drm_gem_prime_import(struct drm_device *dev,
						struct dma_buf *dma_buf)
{
	struct mtk_drm_private *private = dev->dev_private;

	return drm_gem_prime_import_dev(dev, dma_buf, private->dma_dev);
}

static struct drm_driver mtk_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME |
			   DRIVER_ATOMIC | DRIVER_RENDER,

	.gem_free_object_unlocked = mtk_drm_gem_free_object,
	.gem_vm_ops = &drm_gem_cma_vm_ops,
	.dumb_create = mtk_drm_gem_dumb_create,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = drm_gem_prime_export,
	.gem_prime_import = mtk_drm_gem_prime_import,
	.gem_prime_get_sg_table = mtk_gem_prime_get_sg_table,
	.gem_prime_import_sg_table = mtk_gem_prime_import_sg_table,
	.gem_prime_mmap = mtk_drm_gem_mmap_buf,
        .gem_prime_vmap = mtk_drm_gem_vmap_buf,
	.ioctls = mtk_ioctls,
	.num_ioctls = ARRAY_SIZE(mtk_ioctls),
	.fops = &mtk_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int mtk_drm_bind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm;
	int ret;

	drm = drm_dev_alloc(&mtk_drm_driver, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	drm->dev_private = private;
	private->drm = drm;

	ret = mtk_drm_kms_init(drm);
	if (ret < 0)
		goto err_free;

	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto err_deinit;

	return 0;

err_deinit:
	mtk_drm_kms_deinit(drm);
err_free:
	drm_dev_put(drm);
	return ret;
}

static void mtk_drm_unbind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	drm_dev_unregister(private->drm);
	mtk_drm_kms_deinit(private->drm);
	drm_dev_put(private->drm);
	private->num_pipes = 0;
	private->drm = NULL;
}

static const struct component_master_ops mtk_drm_ops = {
	.bind		= mtk_drm_bind,
	.unbind		= mtk_drm_unbind,
};

static const struct of_device_id mtk_ddp_comp_dt_ids[] = {
	{ .compatible = "mediatek,mt2701-disp-ovl",
	  .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt8173-disp-ovl",
	  .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt8183-disp-ovl",
	  .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt8183-disp-ovl-2l",
	  .data = (void *)MTK_DISP_OVL_2L },
	{ .compatible = "mediatek,mt2701-disp-rdma",
	  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt8173-disp-rdma",
	  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt8183-disp-rdma",
	  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt8173-disp-wdma",
	  .data = (void *)MTK_DISP_WDMA },
	{ .compatible = "mediatek,mt8183-disp-ccorr",
	  .data = (void *)MTK_DISP_CCORR },
	{ .compatible = "mediatek,mt2701-disp-color",
	  .data = (void *)MTK_DISP_COLOR },
	{ .compatible = "mediatek,mt8173-disp-color",
	  .data = (void *)MTK_DISP_COLOR },
	{ .compatible = "mediatek,mt8173-disp-aal",
	  .data = (void *)MTK_DISP_AAL},
	{ .compatible = "mediatek,mt8173-disp-gamma",
	  .data = (void *)MTK_DISP_GAMMA, },
	{ .compatible = "mediatek,mt8183-disp-dither",
	  .data = (void *)MTK_DISP_DITHER },
	{ .compatible = "mediatek,mt8173-disp-ufoe",
	  .data = (void *)MTK_DISP_UFOE },
	{ .compatible = "mediatek,mt2701-dsi",
	  .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt8173-dsi",
	  .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt8183-dsi",
	  .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt2701-dpi",
	  .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt8173-dpi",
	  .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt8183-dpi",
	  .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt2701-disp-mutex",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt2712-disp-mutex",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt8173-disp-mutex",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt8183-disp-mutex",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt2701-disp-pwm",
	  .data = (void *)MTK_DISP_BLS },
	{ .compatible = "mediatek,mt8173-disp-pwm",
	  .data = (void *)MTK_DISP_PWM },
	{ .compatible = "mediatek,mt8173-disp-od",
	  .data = (void *)MTK_DISP_OD },
	{ }
};

static int mtk_drm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_drm_private *private;
	struct resource *mem;
	struct device_node *node;
	struct component_match *match = NULL;
	int ret;
	int i;

	private = devm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
	if (!private)
		return -ENOMEM;

	private->data = of_device_get_match_data(dev);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	private->config_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->config_regs)) {
		ret = PTR_ERR(private->config_regs);
		dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
			ret);
		return ret;
	}

	/* Iterate over sibling DISP function blocks */
	for_each_child_of_node(dev->of_node->parent, node) {
		const struct of_device_id *of_id;
		enum mtk_ddp_comp_type comp_type;
		int comp_id;

		of_id = of_match_node(mtk_ddp_comp_dt_ids, node);
		if (!of_id)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(dev, "Skipping disabled component %pOF\n",
				node);
			continue;
		}

		comp_type = (enum mtk_ddp_comp_type)of_id->data;

		if (comp_type == MTK_DISP_MUTEX) {
			private->mutex_node = of_node_get(node);
			continue;
		}

		comp_id = mtk_ddp_comp_get_id(node, comp_type);
		if (comp_id < 0) {
			dev_warn(dev, "Skipping unknown component %pOF\n",
				 node);
			continue;
		}

		private->comp_node[comp_id] = of_node_get(node);

		/*
		 * Currently only the COLOR, OVL, RDMA, DSI, and DPI blocks have
		 * separate component platform drivers and initialize their own
		 * DDP component structure. The others are initialized here.
		 */
		if (comp_type == MTK_DISP_COLOR ||
		    comp_type == MTK_DISP_OVL ||
		    comp_type == MTK_DISP_OVL_2L ||
		    comp_type == MTK_DISP_RDMA ||
		    comp_type == MTK_DSI ||
		    comp_type == MTK_DPI) {
			dev_info(dev, "Adding component match for %pOF\n",
				 node);
			drm_of_component_match_add(dev, &match, compare_of,
						   node);
		} else {
			struct mtk_ddp_comp *comp;

			comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
			if (!comp) {
				ret = -ENOMEM;
				of_node_put(node);
				goto err_node;
			}

			ret = mtk_ddp_comp_init(dev, node, comp, comp_id, NULL);
			if (ret) {
				of_node_put(node);
				goto err_node;
			}

			private->ddp_comp[comp_id] = comp;
		}
	}

	if (!private->mutex_node) {
		dev_err(dev, "Failed to find disp-mutex node\n");
		ret = -ENODEV;
		goto err_node;
	}

	pm_runtime_enable(dev);

	platform_set_drvdata(pdev, private);

	ret = component_master_add_with_match(dev, &mtk_drm_ops, match);
	if (ret)
		goto err_pm;

	return 0;

err_pm:
	pm_runtime_disable(dev);
err_node:
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);
	return ret;
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	int i;

	component_master_del(&pdev->dev, &mtk_drm_ops);
	pm_runtime_disable(&pdev->dev);
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_drm_sys_suspend(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;
	int ret;

	ret = drm_mode_config_helper_suspend(drm);
	DRM_DEBUG_DRIVER("mtk_drm_sys_suspend\n");

	return ret;
}

static int mtk_drm_sys_resume(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;
	int ret;

	ret = drm_mode_config_helper_resume(drm);
	DRM_DEBUG_DRIVER("mtk_drm_sys_resume\n");

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_drm_pm_ops, mtk_drm_sys_suspend,
			 mtk_drm_sys_resume);

static const struct of_device_id mtk_drm_of_ids[] = {
	{ .compatible = "mediatek,mt2701-mmsys",
	  .data = &mt2701_mmsys_driver_data},
	{ .compatible = "mediatek,mt2712-mmsys",
	  .data = &mt2712_mmsys_driver_data},
	{ .compatible = "mediatek,mt8173-mmsys",
	  .data = &mt8173_mmsys_driver_data},
	{ .compatible = "mediatek,mt8183-display",
	  .data = &mt8183_mmsys_driver_data},
	{ }
};

static struct platform_driver mtk_drm_platform_driver = {
	.probe	= mtk_drm_probe,
	.remove	= mtk_drm_remove,
	.driver	= {
		.name	= "mediatek-drm",
		.of_match_table = mtk_drm_of_ids,
		.pm     = &mtk_drm_pm_ops,
	},
};

static struct platform_driver * const mtk_drm_drivers[] = {
	&mtk_ddp_driver,
	&mtk_disp_color_driver,
	&mtk_disp_ovl_driver,
	&mtk_disp_rdma_driver,
	&mtk_dpi_driver,
	&mtk_drm_platform_driver,
	&mtk_mipi_tx_driver,
	&mtk_dsi_driver,
};

static int __init mtk_drm_init(void)
{
	return platform_register_drivers(mtk_drm_drivers,
					 ARRAY_SIZE(mtk_drm_drivers));
}

static void __exit mtk_drm_exit(void)
{
	platform_unregister_drivers(mtk_drm_drivers,
				    ARRAY_SIZE(mtk_drm_drivers));
}

module_init(mtk_drm_init);
module_exit(mtk_drm_exit);

MODULE_AUTHOR("YT SHEN <yt.shen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_LICENSE("GPL v2");
