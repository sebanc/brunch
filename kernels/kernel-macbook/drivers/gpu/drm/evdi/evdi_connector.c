// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include "evdi_drm_drv.h"

#if KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE || defined(EL8)
#include <drm/drm_probe_helper.h>
#endif

/*
 * dummy connector to just get EDID,
 * all EVDI appear to have a DVI-D
 */

static int evdi_get_modes(struct drm_connector *connector)
{
	struct evdi_device *evdi = connector->dev->dev_private;
	struct edid *edid = NULL;
	int ret = 0;

	edid = (struct edid *)evdi_painter_get_edid_copy(evdi);

	if (!edid) {
#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE || defined(EL8)
		drm_connector_update_edid_property(connector, NULL);
#else
		drm_mode_connector_update_edid_property(connector, NULL);
#endif
		return 0;
	}

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE || defined(EL8)
	ret = drm_connector_update_edid_property(connector, edid);
#else
	ret = drm_mode_connector_update_edid_property(connector, edid);
#endif

	if (!ret)
		ret = drm_add_edid_modes(connector, edid);
	else
		EVDI_ERROR("Failed to set edid modes! error: %d", ret);

	kfree(edid);
	return ret;
}

static enum drm_mode_status evdi_mode_valid(struct drm_connector *connector,
					    struct drm_display_mode *mode)
{
	struct evdi_device *evdi = connector->dev->dev_private;
	uint32_t mode_area = mode->hdisplay * mode->vdisplay;

	if (evdi->sku_area_limit == 0)
		return MODE_OK;

	if (mode_area > evdi->sku_area_limit) {
		EVDI_WARN("(dev=%d) Mode %dx%d@%d rejected\n",
			evdi->dev_index,
			mode->hdisplay,
			mode->vdisplay,
			drm_mode_vrefresh(mode));
		return MODE_BAD;
	}

	return MODE_OK;
}

static enum drm_connector_status
evdi_detect(struct drm_connector *connector, __always_unused bool force)
{
	struct evdi_device *evdi = connector->dev->dev_private;

	EVDI_CHECKPT();
	if (evdi_painter_is_connected(evdi->painter)) {
		EVDI_DEBUG("(dev=%d) poll connector state: connected\n",
			   evdi->dev_index);
		return connector_status_connected;
	}
	EVDI_DEBUG("(dev=%d) poll connector state: disconnected\n",
		   evdi->dev_index);
	return connector_status_disconnected;
}

static void evdi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static struct drm_encoder *evdi_best_encoder(struct drm_connector *connector)
{
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
	struct drm_encoder *encoder;

	drm_connector_for_each_possible_encoder(connector, encoder) {
		return encoder;
	}

	return NULL;
#else
	return drm_encoder_find(connector->dev,
				NULL,
				connector->encoder_ids[0]);
#endif
}

static struct drm_connector_helper_funcs evdi_connector_helper_funcs = {
	.get_modes = evdi_get_modes,
	.mode_valid = evdi_mode_valid,
	.best_encoder = evdi_best_encoder,
};

static const struct drm_connector_funcs evdi_connector_funcs = {
	.detect = evdi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = evdi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state
};

int evdi_connector_init(struct drm_device *dev, struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct evdi_device *evdi = dev->dev_private;

	connector = kzalloc(sizeof(struct drm_connector), GFP_KERNEL);
	if (!connector)
		return -ENOMEM;

	/* TODO: Initialize connector with actual connector type */
	drm_connector_init(dev, connector, &evdi_connector_funcs,
			   DRM_MODE_CONNECTOR_DVII);
	drm_connector_helper_add(connector, &evdi_connector_helper_funcs);
	connector->polled = DRM_CONNECTOR_POLL_HPD;

	drm_connector_register(connector);

	evdi->conn = connector;

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE  || defined(EL8)
	drm_connector_attach_encoder(connector, encoder);
#else
	drm_mode_connector_attach_encoder(connector, encoder);
#endif
	return 0;
}
