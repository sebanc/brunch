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

		if (!crtc)
			continue;
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
	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		if (crtc && drm_atomic_crtc_needs_modeset(crtc_state)) {
			needs_modeset |= (1 << drm_crtc_index(crtc));
			break;
		}
	}

	has_cursor_plane = 0;
	for_each_plane_in_state(state, plane, plane_state, i) {
		if (crtc && plane->crtc && plane == plane->crtc->cursor) {
			has_cursor_plane |= (1 << drm_crtc_index(crtc));
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
	struct drm_plane_state *plane_state;
	int i;

	for_each_plane_in_state(state, plane, plane_state, i)
		mtk_fb_wait(plane->state->fb);
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
	 *     drm_atomic_helper_commit_planes(dev, state, true);
	 *
	 * See the kerneldoc entries for these three functions for more details.
	 */
	mutex_lock(&private->hw_lock);
	drm_atomic_helper_commit_modeset_disables(drm, state);
	drm_atomic_helper_commit_modeset_enables(drm, state);
	drm_atomic_helper_commit_planes(drm, state, true);
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

	drm_atomic_helper_swap_state(state, true);

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

static const enum mtk_ddp_comp_id mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_AAL,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mtk_ddp_split_dsi[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_AAL,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_SPLIT1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI0,
};

static int mtk_drm_kms_init(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;
	struct platform_device *pdev;
	struct device_node *np;
	const enum mtk_ddp_comp_id *dsi_stream;
	int dsi_stream_len;
	bool dual_dsi_mode;
	int ret;

	if (!iommu_present(&platform_bus_type))
		return -EPROBE_DEFER;

	pdev = of_find_device_by_node(private->mutex_node);
	if (!pdev) {
		dev_err(drm->dev, "Waiting for disp-mutex device %s\n",
			private->mutex_node->full_name);
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
	 * and each statically assigned to a crtc.
	 *
	 * By default, UFOE directly drives DSI0:
	 * OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0 ...
	 *
	 * However, if the dsi0 node has "mediatek,dual-dsi-mode" set,
	 * UFOE will use SPLIT1 to dual drive both DSI0 and DSI1:
	 * OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> SPLIT1 -> DSI0 ...
	 *                                                       \-> DSI1 ...
	 */
	np = private->comp_node[DDP_COMPONENT_DSI0];
	if (np && of_property_read_bool(np, "mediatek,dual-dsi-mode")) {
		dsi_stream = mtk_ddp_split_dsi;
		dsi_stream_len = ARRAY_SIZE(mtk_ddp_split_dsi);
		dual_dsi_mode = true;
	} else {
		dsi_stream = mtk_ddp_main;
		dsi_stream_len = ARRAY_SIZE(mtk_ddp_main);
		dual_dsi_mode = false;
	}
	ret = mtk_drm_crtc_create(drm, dsi_stream, dsi_stream_len,
				  dual_dsi_mode);
	if (ret < 0)
		goto err_component_unbind;

	/*
	 * The DPI stream is directly driven from RDMA1:
	 * ... and OVL1 -> COLOR1 -> GAMMA -> RDMA1 -> DPI0
	 */
	ret = mtk_drm_crtc_create(drm, mtk_ddp_ext, ARRAY_SIZE(mtk_ddp_ext),
				  false);
	if (ret < 0)
		goto err_component_unbind;

	/* Use OVL device for all DMA memory allocations */
	np = private->comp_node[dsi_stream[0]] ?:
	     private->comp_node[mtk_ddp_ext[0]];
	pdev = of_find_device_by_node(np);
	if (!pdev) {
		ret = -ENODEV;
		dev_err(drm->dev, "Need at least one OVL device\n");
		goto err_component_unbind;
	}

	private->dma_dev = &pdev->dev;

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	drm->irq_enabled = true;
	ret = drm_vblank_init(drm, MAX_CRTC);
	if (ret < 0)
		goto err_component_unbind;

	drm->vblank_disable_allowed = true;
	drm_kms_helper_poll_init(drm);
	drm_mode_config_reset(drm);

	INIT_WORK(&private->unreference.work, mtk_unreference_work);
	INIT_LIST_HEAD(&private->unreference.list);
	spin_lock_init(&private->unreference.lock);
	init_waitqueue_head(&private->commit.crtcs_event);
	mutex_init(&private->hw_lock);

	return 0;

err_component_unbind:
	component_unbind_all(drm->dev, drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);

	return ret;
}

static void mtk_drm_kms_deinit(struct drm_device *drm)
{
	drm_kms_helper_poll_fini(drm);

	drm_vblank_cleanup(drm);
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

static struct drm_driver mtk_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME |
			   DRIVER_ATOMIC | DRIVER_RENDER,

	.get_vblank_counter = drm_vblank_no_hw_counter,
	.enable_vblank = mtk_drm_crtc_enable_vblank,
	.disable_vblank = mtk_drm_crtc_disable_vblank,

	.gem_free_object = mtk_drm_gem_free_object,
	.gem_vm_ops = &drm_gem_cma_vm_ops,
	.dumb_create = mtk_drm_gem_dumb_create,
	.dumb_map_offset = mtk_drm_gem_dumb_map_offset,
	.dumb_destroy = drm_gem_dumb_destroy,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = drm_gem_prime_export,
	.gem_prime_import = drm_gem_prime_import,
	.gem_prime_get_sg_table = mtk_gem_prime_get_sg_table,
	.gem_prime_import_sg_table = mtk_gem_prime_import_sg_table,
	.gem_prime_mmap = mtk_drm_gem_mmap_buf,
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
	if (!drm)
		return -ENOMEM;

	drm_dev_set_unique(drm, dev_name(dev));

	drm->dev_private = private;
	private->drm = drm;

	ret = mtk_drm_kms_init(drm);
	if (ret < 0)
		goto err_free;

	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto err_deinit;

	ret = drm_connector_register_all(drm);
	if (ret < 0)
		goto err_unregister;

	return 0;

err_unregister:
	drm_dev_unregister(drm);
err_deinit:
	mtk_drm_kms_deinit(drm);
err_free:
	drm_dev_unref(drm);
	return ret;
}

static void mtk_drm_unbind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	drm_put_dev(private->drm);
	private->drm = NULL;
}

static const struct component_master_ops mtk_drm_ops = {
	.bind		= mtk_drm_bind,
	.unbind		= mtk_drm_unbind,
};

static const struct of_device_id mtk_ddp_comp_dt_ids[] = {
	{ .compatible = "mediatek,mt8173-disp-ovl",   .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt8173-disp-rdma",  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt8173-disp-wdma",  .data = (void *)MTK_DISP_WDMA },
	{ .compatible = "mediatek,mt8173-disp-color", .data = (void *)MTK_DISP_COLOR },
	{ .compatible = "mediatek,mt8173-disp-aal",   .data = (void *)MTK_DISP_AAL},
	{ .compatible = "mediatek,mt8173-disp-gamma", .data = (void *)MTK_DISP_GAMMA, },
	{ .compatible = "mediatek,mt8173-disp-ufoe",  .data = (void *)MTK_DISP_UFOE },
	{ .compatible = "mediatek,mt8173-dsi",        .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt8173-dpi",        .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt8173-disp-mutex", .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt8173-disp-pwm",   .data = (void *)MTK_DISP_PWM },
	{ .compatible = "mediatek,mt8173-disp-od",    .data = (void *)MTK_DISP_OD },
	{ .compatible = "mediatek,mt8173-disp-split", .data = (void *)MTK_DISP_SPLIT },
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

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	private->config_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->config_regs)) {
		ret = PTR_ERR(private->config_regs);
		dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
			ret);
		return ret;
	}

	node = of_parse_phandle(dev->of_node, "mediatek,gce", 0);
	if (!node) {
		dev_err(dev, "Failed to get gce node\n");
		return -EINVAL;
	}

	private->gce_pdev = of_find_device_by_node(node);
	of_node_put(node);
	if (!private->gce_pdev) {
		dev_err(dev, "Failed to find gce platform device\n");
		return -EINVAL;
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
			dev_dbg(dev, "Skipping disabled component %s\n",
				node->full_name);
			continue;
		}

		comp_type = (enum mtk_ddp_comp_type)of_id->data;

		if (comp_type == MTK_DISP_MUTEX) {
			private->mutex_node = of_node_get(node);
			continue;
		}

		comp_id = mtk_ddp_comp_get_id(node, comp_type);
		if (comp_id < 0) {
			dev_warn(dev, "Skipping unknown component %s\n",
				 node->full_name);
			continue;
		}

		private->comp_node[comp_id] = of_node_get(node);

		/*
		 * Currently only the OVL, RDMA, DSI, and DPI blocks have
		 * separate component platform drivers and initialize their own
		 * DDP component structure. The others are initialized here.
		 */
		if (comp_type == MTK_DISP_OVL ||
		    comp_type == MTK_DISP_RDMA ||
		    comp_type == MTK_DSI ||
		    comp_type == MTK_DPI) {
			dev_info(dev, "Adding component match for %s\n",
				 node->full_name);
			component_match_add(dev, &match, compare_of, node);
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
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++) {
		of_node_put(private->comp_node[i]);
		if (private->ddp_comp[i]) {
			put_device(private->ddp_comp[i]->larb_dev);
			private->ddp_comp[i] = NULL;
		}
	}
	return ret;
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	struct drm_device *drm = private->drm;
	int i;

	drm_connector_unregister_all(drm);
	drm_dev_unregister(drm);
	mtk_drm_kms_deinit(drm);
	drm_dev_unref(drm);

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

	drm_kms_helper_poll_disable(drm);

	private->suspend_state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(private->suspend_state)) {
		drm_kms_helper_poll_enable(drm);
		return PTR_ERR(private->suspend_state);
	}

	DRM_DEBUG_DRIVER("mtk_drm_sys_suspend\n");
	return 0;
}

static int mtk_drm_sys_resume(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;

	drm_atomic_helper_resume(drm, private->suspend_state);
	drm_kms_helper_poll_enable(drm);

	DRM_DEBUG_DRIVER("mtk_drm_sys_resume\n");
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_drm_pm_ops, mtk_drm_sys_suspend,
			 mtk_drm_sys_resume);

static const struct of_device_id mtk_drm_of_ids[] = {
	{ .compatible = "mediatek,mt8173-mmsys", },
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
	&mtk_disp_ovl_driver,
	&mtk_disp_rdma_driver,
	&mtk_dpi_driver,
	&mtk_drm_platform_driver,
	&mtk_dsi_driver,
	&mtk_mipi_tx_driver,
};

static int __init mtk_drm_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_drm_drivers); i++) {
		ret = platform_driver_register(mtk_drm_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
			       mtk_drm_drivers[i]->driver.name, ret);
			goto err;
		}
	}

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_drm_drivers[i]);

	return ret;
}

static void __exit mtk_drm_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_drm_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_drm_drivers[i]);
}

module_init(mtk_drm_init);
module_exit(mtk_drm_exit);

MODULE_AUTHOR("YT SHEN <yt.shen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_LICENSE("GPL v2");
