// SPDX-License-Identifier: GPL-2.0

#include <linux/backlight.h>
#include <linux/err.h>
#include <drm/drm_dp_aux_backlight.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_print.h>

static int drm_dp_aux_brightness_set(struct backlight_device *bd)
{
	struct drm_dp_aux_backlight *pdata = bl_get_data(bd);
	u16 brightness = bd->props.brightness;
	u8 val[2] = { 0x0 };
	int ret = 0;

	if (!pdata->enabled)
		return 0;

	if (bd->props.power != FB_BLANK_UNBLANK ||
	    bd->props.fb_blank != FB_BLANK_UNBLANK ||
	    bd->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		brightness = 0;

	val[0] = brightness >> 8;
	val[1] = brightness & 0xff;
	ret = drm_dp_dpcd_write(pdata->aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB,
				val, sizeof(val));
	if (ret < 0)
		return ret;

	return 0;
}

static int drm_dp_aux_brightness_get(struct backlight_device *bd)
{
	struct drm_dp_aux_backlight *pdata = bl_get_data(bd);
	u8 val[2] = { 0x0 };
	int ret = 0;

	if (!pdata->enabled)
		return 0;

	if (bd->props.power != FB_BLANK_UNBLANK ||
	    bd->props.fb_blank != FB_BLANK_UNBLANK ||
	    bd->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))
		return 0;

	ret = drm_dp_dpcd_read(pdata->aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB,
			       &val, sizeof(val));
	if (ret < 0)
		return ret;

	return (val[0] << 8 | val[1]);
}

static const struct backlight_ops aux_bl_ops = {
	.update_status = drm_dp_aux_brightness_set,
	.get_brightness = drm_dp_aux_brightness_get,
};

/**
 * drm_dp_aux_backlight_enable() - Enable DP aux backlight
 * @aux_bl: the DP aux backlight to enable
 *
 * Returns 0 on success or a negative error code on failure.
 */
int drm_dp_aux_backlight_enable(struct drm_dp_aux_backlight *aux_bl)
{
	u8 val = 0;
	int ret;

	if (!aux_bl || aux_bl->enabled)
		return 0;

	/* Set backlight control mode */
	ret = drm_dp_dpcd_readb(aux_bl->aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER,
				&val);
	if (ret < 0)
		return ret;

	val &= ~DP_EDP_BACKLIGHT_CONTROL_MODE_MASK;
	val |= DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD;
	ret = drm_dp_dpcd_writeb(aux_bl->aux,
				 DP_EDP_BACKLIGHT_MODE_SET_REGISTER,
				 val);
	if (ret < 0)
		return ret;

	/* Enable backlight */
	ret = drm_dp_dpcd_readb(aux_bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER,
				&val);
	if (ret < 0)
		return ret;

	val |= DP_EDP_BACKLIGHT_ENABLE;
	ret = drm_dp_dpcd_writeb(aux_bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER,
				 val);
	if (ret < 0)
		return ret;

	ret = backlight_enable(aux_bl->bd);
	if (ret < 0)
		DRM_DEV_INFO(aux_bl->dev, "failed to enable backlight: %d\n",
			     ret);

	aux_bl->enabled = true;

	return 0;
}
EXPORT_SYMBOL(drm_dp_aux_backlight_enable);

/**
 * drm_dp_aux_backlight_disable() - Disable DP aux backlight
 * @aux_bl: the DP aux backlight to disable
 *
 * Returns 0 on success or a negative error code on failure.
 */
int drm_dp_aux_backlight_disable(struct drm_dp_aux_backlight *aux_bl)
{
	u8 val = 0;
	int ret;

	if (!aux_bl || !aux_bl->enabled)
		return 0;

	ret = backlight_disable(aux_bl->bd);
	if (ret < 0)
		DRM_DEV_INFO(aux_bl->dev, "failed to disable backlight: %d\n",
			     ret);

	ret = drm_dp_dpcd_readb(aux_bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER,
				&val);
	if (ret < 0)
		return ret;

	val &= ~DP_EDP_BACKLIGHT_ENABLE;
	ret = drm_dp_dpcd_writeb(aux_bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER,
				 val);
	if (ret < 0)
		return ret;

	aux_bl->enabled = false;

	return 0;
}
EXPORT_SYMBOL(drm_dp_aux_backlight_disable);

/**
 * drm_dp_aux_backlight_register() - register a DP aux backlight device
 * @name: the name of the backlight device
 * @aux_bl: the DP aux backlight to register
 *
 * Creates and registers a new backlight device that uses DPCD registers
 * on the DisplayPort aux channel to control the brightness of the panel.
 *
 * Returns 0 on success or a negative error code on failure.
 */
int drm_dp_aux_backlight_register(const char *name,
				struct drm_dp_aux_backlight *aux_bl)
{
	struct backlight_properties bl_props = { 0 };
	int max_brightness;

	if (!name || !aux_bl || !aux_bl->aux)
		return -EINVAL;

	max_brightness = 0xffff;

	bl_props.type = BACKLIGHT_RAW;
	bl_props.brightness = max_brightness;
	bl_props.max_brightness = max_brightness;
	aux_bl->bd = devm_backlight_device_register(aux_bl->dev, name,
						  aux_bl->dev, aux_bl,
						  &aux_bl_ops, &bl_props);
	if (IS_ERR(aux_bl->bd)) {
		int ret = PTR_ERR(aux_bl->bd);

		aux_bl->bd = NULL;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_aux_backlight_register);
