/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _DRM_DP_AUX_BACKLIGHT_H_
#define _DRM_DP_AUX_BACKLIGHT_H_

#include <linux/backlight.h>
#include <drm/drm_dp_helper.h>

/**
 * struct drm_dp_aux_backlight - DisplayPort aux backlight
 * @dev: the device to register
 * @aux: the DisplayPort aux channel
 * @bd: the backlight device
 * @enabled: true if backlight is enabled else false.
 */
struct drm_dp_aux_backlight {
	struct device *dev;
	struct drm_dp_aux *aux;
	struct backlight_device *bd;
	bool enabled;
};

int drm_dp_aux_backlight_enable(struct drm_dp_aux_backlight *aux_bl);
int drm_dp_aux_backlight_disable(struct drm_dp_aux_backlight *aux_bl);

int drm_dp_aux_backlight_register(const char *name,
				struct drm_dp_aux_backlight *aux_bl);

#endif /* _DRM_DP_AUX_BACKLIGHT_H_ */
