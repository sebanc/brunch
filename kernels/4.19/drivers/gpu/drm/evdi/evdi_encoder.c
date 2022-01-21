// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2018 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include "evdi_drv.h"

/* dummy encoder */
static void evdi_enc_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
	kfree(encoder);
}

static const struct drm_encoder_funcs evdi_enc_funcs = {
	.destroy = evdi_enc_destroy,
};

struct drm_encoder *evdi_encoder_init(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	int ret = 0;

	encoder = kzalloc(sizeof(struct drm_encoder), GFP_KERNEL);
	if (!encoder)
		goto err;

	ret = drm_encoder_init(dev, encoder, &evdi_enc_funcs,
			       DRM_MODE_ENCODER_TMDS, NULL);
	if (ret) {
		EVDI_ERROR("Failed to initialize encoder: %d\n", ret);
		goto err_encoder;
	}

	encoder->possible_crtcs = 1;
	return encoder;

err_encoder:
	kfree(encoder);
err:
	return NULL;
}
