// SPDX-License-Identifier: GPL-2.0

#include <linux/backlight.h>
#include <linux/err.h>
#include <drm/drm_dp_aux_backlight.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_print.h>

/**
 * drm_dp_aux_backlight_enable() - Enable DP aux backlight
 * @aux_bl: the DP aux backlight to enable
 *
 * Returns 0 on success or a negative error code on failure.
 */
static int drm_dp_aux_backlight_enable(struct drm_dp_aux_backlight *aux_bl)
{
	u8 val = 0;
	int ret;

	/* Set backlight control mode */
	ret = drm_dp_dpcd_readb(aux_bl->aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER,
				&val);
	if (ret < 0)
		return ret;

	val &= ~DP_EDP_BACKLIGHT_CONTROL_MODE_MASK;
	val |= DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD;
	ret = drm_dp_dpcd_writeb(aux_bl->aux,
				 DP_EDP_BACKLIGHT_MODE_SET_REGISTER, val);
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

	return 0;
}

/**
 * drm_dp_aux_backlight_disable() - Disable DP aux backlight
 * @aux_bl: the DP aux backlight to disable
 *
 * Returns 0 on success or a negative error code on failure.
 */
static int drm_dp_aux_backlight_disable(struct drm_dp_aux_backlight *aux_bl)
{
	u8 val = 0;
	int ret;

	ret = drm_dp_dpcd_readb(aux_bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER,
				&val);
	if (ret < 0)
		return ret;

	val &= ~DP_EDP_BACKLIGHT_ENABLE;
	ret = drm_dp_dpcd_writeb(aux_bl->aux, DP_EDP_DISPLAY_CONTROL_REGISTER,
				 val);
	if (ret < 0)
		return ret;

	return 0;
}

static int drm_dp_aux_brightness_set(struct backlight_device *bd)
{
	struct drm_dp_aux_backlight *pdata = bl_get_data(bd);
	u16 brightness = bd->props.brightness;
	u8 val[2] = { 0x0 };
	int ret = 0;
	bool is_blank;

	is_blank = bd->props.power != FB_BLANK_UNBLANK ||
		   bd->props.fb_blank != FB_BLANK_UNBLANK ||
		   bd->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK);

	if (is_blank) {
		if (!pdata->enabled)
			return 0;

		ret = drm_dp_aux_backlight_disable(pdata);
		if (ret)
			return ret;
		pdata->enabled = false;
	} else {
		/* Set the brightness before turning on the backlight. */
		val[0] = brightness >> 8;
		val[1] = brightness & 0xff;
		ret = drm_dp_dpcd_write(pdata->aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB,
					val, sizeof(val));
		if (ret < 0)
			return ret;

		/* If we're already enabled then we're done */
		if (pdata->enabled)
			return 0;

		ret = drm_dp_aux_backlight_enable(pdata);
		if (ret)
			return ret;
		pdata->enabled = true;
	}

	return 0;
}

static const struct backlight_ops aux_bl_ops = {
	.update_status = drm_dp_aux_brightness_set,
};

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

	/* Should start out officially "disabled" */
	backlight_disable(aux_bl->bd);

	return 0;
}
EXPORT_SYMBOL(drm_dp_aux_backlight_register);
