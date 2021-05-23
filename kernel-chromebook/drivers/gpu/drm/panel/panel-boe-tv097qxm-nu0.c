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

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/videomode.h>

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	const struct display_timing *timings;
	unsigned int num_timings;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;

	u32 bus_format;
};

struct panel_desc_dsi {
	struct panel_desc desc;

	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
};

struct boe {
	struct drm_panel base;
	bool prepared;
	bool enabled;

	const struct panel_desc *desc;

	struct backlight_device *backlight;
	struct regulator *avdd;
	struct regulator *avee;
	struct regulator *vddio;
	struct i2c_adapter *ddc;

	struct gpio_desc *enable_gpio;
};

static inline struct boe *to_boe(struct drm_panel *panel)
{
	return container_of(panel, struct boe, base);
}

static int boe_get_fixed_modes(struct boe *panel)
{
	struct drm_connector *connector = panel->base.connector;
	struct drm_device *drm = panel->base.drm;
	struct drm_display_mode *mode;
	unsigned int i, num = 0;

	if (!panel->desc)
		return 0;

	for (i = 0; i < panel->desc->num_timings; i++) {
		const struct display_timing *dt = &panel->desc->timings[i];
		struct videomode vm;

		videomode_from_timing(dt, &vm);
		mode = drm_mode_create(drm);
		if (!mode) {
			dev_err(drm->dev, "failed to add mode %ux%u\n",
				dt->hactive.typ, dt->vactive.typ);
			continue;
		}

		drm_display_mode_from_videomode(&vm, mode);
		drm_mode_set_name(mode);

		drm_mode_probed_add(connector, mode);
		num++;
	}

	for (i = 0; i < panel->desc->num_modes; i++) {
		const struct drm_display_mode *m = &panel->desc->modes[i];

		mode = drm_mode_duplicate(drm, m);
		if (!mode) {
			dev_err(drm->dev, "failed to add mode %ux%u@%u\n",
				m->hdisplay, m->vdisplay, m->vrefresh);
			continue;
		}

		drm_mode_set_name(mode);

		drm_mode_probed_add(connector, mode);
		num++;
	}

	connector->display_info.bpc = panel->desc->bpc;
	connector->display_info.width_mm = panel->desc->size.width;
	connector->display_info.height_mm = panel->desc->size.height;
	if (panel->desc->bus_format)
		drm_display_info_set_bus_formats(&connector->display_info,
						 &panel->desc->bus_format, 1);

	return num;
}

static int boe_disable(struct drm_panel *panel)
{
	struct boe *p = to_boe(panel);

	if (!p->enabled)
		return 0;

	if (p->backlight) {
		dev_info(panel->dev, "Disabling backlight\n");
		p->backlight->props.power = FB_BLANK_POWERDOWN;
		p->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(p->backlight);
	}

	if (p->desc->delay.disable)
		msleep(p->desc->delay.disable);

	p->enabled = false;

	return 0;
}

static int boe_unprepare(struct drm_panel *panel)
{
	struct boe *p = to_boe(panel);

	if (!p->prepared)
		return 0;

	regulator_disable(p->avee);
	regulator_disable(p->avdd);

	gpiod_set_value_cansleep(p->enable_gpio, 0);

	regulator_disable(p->vddio);

		if (p->desc->delay.unprepare)
		msleep(p->desc->delay.unprepare);

	p->prepared = false;

	return 0;
}

static int boe_prepare(struct drm_panel *panel)
{
	struct boe *p = to_boe(panel);
	int err;

	if (p->prepared)
		return 0;

	err = regulator_enable(p->vddio);
	if (err < 0) {
		dev_err(panel->dev, "failed to enable supply: %d\n", err);
		return err;
	}

	msleep(15);

	gpiod_set_value_cansleep(p->enable_gpio, 1);
	usleep_range(10, 15);
	gpiod_set_value_cansleep(p->enable_gpio, 0);
	usleep_range(10, 15);
	gpiod_set_value_cansleep(p->enable_gpio, 1);

	msleep(20);

	err = regulator_enable(p->avdd);
	if (err)
		goto err_disable_vddio;

	err = regulator_enable(p->avee);
	if (err)
		goto err_disable_avdd;

	if (p->desc->delay.prepare)
		msleep(p->desc->delay.prepare);

	p->prepared = true;

	return 0;

err_disable_avdd:
	regulator_disable(p->avdd);
err_disable_vddio:
	gpiod_set_value_cansleep(p->enable_gpio, 0);
	msleep(20);
	regulator_disable(p->vddio);

	return err;
}

static int boe_enable(struct drm_panel *panel)
{
	struct boe *p = to_boe(panel);

	if (p->enabled)
		return 0;

	if (p->desc->delay.enable)
		msleep(p->desc->delay.enable);

	if (p->backlight) {
		dev_info(panel->dev, "Enabling backlight\n");
		p->backlight->props.state &= ~BL_CORE_FBBLANK;
		p->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(p->backlight);
	}

	p->enabled = true;

	return 0;
}

static int boe_get_modes(struct drm_panel *panel)
{
	struct boe *p = to_boe(panel);
	int num = 0;

	/* probe EDID if a DDC bus is available */
	if (p->ddc) {
		struct edid *edid = drm_get_edid(panel->connector, p->ddc);
		drm_mode_connector_update_edid_property(panel->connector, edid);
		if (edid) {
			num += drm_add_edid_modes(panel->connector, edid);
			kfree(edid);
		}
	}

	/* add hard-coded panel modes */
	num += boe_get_fixed_modes(p);

	return num;
}

static const struct drm_panel_funcs boe_drm_funcs = {
	.disable = boe_disable,
	.unprepare = boe_unprepare,
	.prepare = boe_prepare,
	.enable = boe_enable,
	.get_modes = boe_get_modes,
};

static const struct drm_display_mode boe_tv097qxm_nu0_mode = {
	.clock = 241646,
	.hdisplay = 1536,
	.hsync_start = 1536 + 200,
	.hsync_end = 1536 + 200 + 4,
	.htotal = 1536 + 200 + 4 + 200,
	.vdisplay = 2048,
	.vsync_start = 2048 + 12,
	.vsync_end = 2048 + 12 + 2,
	.vtotal = 2048 + 12 + 2 + 14,
	.vrefresh = 60,
};

static const struct panel_desc_dsi boe_tv097qxm_nu0 = {
	.desc = {
		.modes = &boe_tv097qxm_nu0_mode,
		.num_modes = 1,
		.bpc = 8,
		.size = {
			.width = 147,
			.height = 196,
		},
		.delay = {
			.prepare = 20,
			.enable = 100,
			.disable = 20,
			.unprepare = 20,
		},
	},
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		 MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
		 MIPI_DSI_CLOCK_NON_CONTINUOUS,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 4,
};


static const struct of_device_id panel_boe_dsi_of_match[] = {
	{
		.compatible = "boe,tv097qxm-nu0",
		.data = &boe_tv097qxm_nu0
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, panel_boe_dsi_of_match);

static int panel_boe_probe(struct device *dev, const struct panel_desc *desc)
{
	struct device_node *backlight, *ddc;
	struct boe *panel;
	int err;

	panel = devm_kzalloc(dev, sizeof(struct boe), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->enabled = false;
	panel->prepared = false;
	panel->desc = desc;

	panel->avdd = devm_regulator_get(dev, "avdd");
	if (IS_ERR(panel->avdd)) {
		dev_info(dev, "no avdd regulator found\n");
		return PTR_ERR(panel->avdd);
	}

	panel->avee = devm_regulator_get(dev, "avee");
	if (IS_ERR(panel->avee)) {
		dev_info(dev, "no avee regulator found\n");
		return PTR_ERR(panel->avee);
	}

	panel->vddio = devm_regulator_get(dev, "vddio");
	if (IS_ERR(panel->vddio)) {
		dev_err(dev, "no vddio regulator found\n");
		return PTR_ERR(panel->vddio);
	}

	panel->enable_gpio = devm_gpiod_get_optional(dev, "enable",
						     GPIOD_OUT_LOW);
	if (IS_ERR(panel->enable_gpio)) {
		err = PTR_ERR(panel->enable_gpio);
		dev_err(dev, "failed to request GPIO: %d\n", err);
		return err;
	}

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		panel->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!panel->backlight)
			return -EPROBE_DEFER;
	} else {
		dev_warn(dev, "failed to parse backlight.\n");
	}

	ddc = of_parse_phandle(dev->of_node, "ddc-i2c-bus", 0);
	if (ddc) {
		panel->ddc = of_find_i2c_adapter_by_node(ddc);
		of_node_put(ddc);

		if (!panel->ddc) {
			err = -EPROBE_DEFER;
			goto free_backlight;
		}
	} else {
		dev_warn(dev, "failed to parse ddc-i2c-bus.\n");
	}

	drm_panel_init(&panel->base);
	panel->base.dev = dev;
	panel->base.funcs = &boe_drm_funcs;

	err = drm_panel_add(&panel->base);
	if (err < 0)
		goto free_ddc;

	dev_set_drvdata(dev, panel);

	return 0;

free_ddc:
	if (panel->ddc)
		put_device(&panel->ddc->dev);
free_backlight:
	if (panel->backlight)
		put_device(&panel->backlight->dev);

	return err;
}

static int panel_boe_dsi_probe(struct mipi_dsi_device *dsi)
{
	const struct panel_desc_dsi *desc;
	const struct of_device_id *id;
	int err;

	id = of_match_node(panel_boe_dsi_of_match, dsi->dev.of_node);
	if (!id)
		return -ENODEV;

	desc = id->data;

	err = panel_boe_probe(&dsi->dev, &desc->desc);
	if (err < 0)
		return err;

	dsi->mode_flags = desc->flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

	return mipi_dsi_attach(dsi);
}

static int panel_boe_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct boe *panel = mipi_dsi_get_drvdata(dsi);
	int err;

	err = mipi_dsi_detach(dsi);
	if (err < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", err);

	drm_panel_detach(&panel->base);
	drm_panel_remove(&panel->base);

	boe_disable(&panel->base);

	if (panel->backlight)
		put_device(&panel->backlight->dev);

	return 0;
}

static void panel_boe_dsi_shutdown(struct mipi_dsi_device *dsi)
{
	struct boe *panel = mipi_dsi_get_drvdata(dsi);

	boe_disable(&panel->base);
}

static struct mipi_dsi_driver panel_boe_dsi_driver = {
	.driver = {
		.name = "panel-boe-dsi",
		.of_match_table = panel_boe_dsi_of_match,
	},
	.probe = panel_boe_dsi_probe,
	.remove = panel_boe_dsi_remove,
	.shutdown = panel_boe_dsi_shutdown,
};

module_mipi_dsi_driver(panel_boe_dsi_driver);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_AUTHOR("Shaoming Chen <shaoming.chen@mediatek.com>");
MODULE_DESCRIPTION("BOE TV097QXM-NU0 LCD Panel Driver");
MODULE_LICENSE("GPL v2");
