// SPDX-License-Identifier: GPL-2.0-only
/*
 * ANX7688 HDMI->DP bridge driver
 *
 * Copyright 2016 Google LLC
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <drm/drm_bridge.h>

/* Register addresses */
#define VENDOR_ID_REG 0x00
#define DEVICE_ID_REG 0x02

#define FW_VERSION_REG 0x80

#define DP_BANDWIDTH_REG 0x85
#define DP_LANE_COUNT_REG 0x86

#define VENDOR_ID 0x1f29
#define DEVICE_ID 0x7688

/* First supported firmware version (0.85) */
#define MINIMUM_FW_VERSION 0x0085

struct anx7688 {
	struct drm_bridge bridge;
	struct i2c_client *client;
	struct regmap *regmap;

	bool filter;
};

static inline struct anx7688 *bridge_to_anx7688(struct drm_bridge *bridge)
{
	return container_of(bridge, struct anx7688, bridge);
}

static bool anx7688_bridge_mode_fixup(struct drm_bridge *bridge,
				      const struct drm_display_mode *mode,
				      struct drm_display_mode *adjusted_mode)
{
	struct anx7688 *anx7688 = bridge_to_anx7688(bridge);
	u8 regs[2];
	u8 dpbw, lanecount;
	int totalbw, requiredbw;
	int ret;

	if (!anx7688->filter)
		return true;

	/* Read both regs 0x85 (bandwidth) and 0x86 (lane count). */
	ret = regmap_bulk_read(anx7688->regmap, DP_BANDWIDTH_REG, regs, 2);
	if (ret < 0) {
		dev_err(&anx7688->client->dev,
			"Failed to read bandwidth/lane count\n");
		return false;
	}
	dpbw = regs[0];
	lanecount = regs[1];

	/* Maximum 0x19 bandwidth (6.75 Gbps Turbo mode), 2 lanes */
	if (dpbw > 0x19 || lanecount > 2) {
		dev_err(&anx7688->client->dev,
			"Invalid bandwidth/lane count (%02x/%d)\n",
			dpbw, lanecount);
		return false;
	}

	/* Compute available bandwidth (kHz) */
	totalbw = dpbw * lanecount * 270000 * 8 / 10;

	/* Required bandwidth (8 bpc, kHz) */
	requiredbw = mode->clock * 8 * 3;

	dev_dbg(&anx7688->client->dev,
		"DP bandwidth: %d kHz (%02x/%d); mode requires %d Khz\n",
		totalbw, dpbw, lanecount, requiredbw);

	if (totalbw == 0) {
		dev_warn(&anx7688->client->dev,
			 "Bandwidth/lane count are 0, not rejecting modes\n");
		return true;
	}

	return totalbw >= requiredbw;
}

static const struct drm_bridge_funcs anx7688_bridge_funcs = {
	.mode_fixup	= anx7688_bridge_mode_fixup,
};

static const struct regmap_config anx7688_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int anx7688_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct anx7688 *anx7688;
	struct device *dev = &client->dev;
	int ret;
	u8 buffer[4];
	u16 vendor, device, fwversion;

	anx7688 = devm_kzalloc(dev, sizeof(*anx7688), GFP_KERNEL);
	if (!anx7688)
		return -ENOMEM;

#if IS_ENABLED(CONFIG_OF)
	anx7688->bridge.of_node = client->dev.of_node;
#endif

	anx7688->client = client;
	i2c_set_clientdata(client, anx7688);

	anx7688->regmap =
		devm_regmap_init_i2c(client, &anx7688_regmap_config);

	/* Read both vendor and device id (4 bytes). */
	ret = regmap_bulk_read(anx7688->regmap, VENDOR_ID_REG, buffer, 4);
	if (ret) {
		dev_err(dev, "Failed to read chip vendor/device id\n");
		return ret;
	}

	vendor = (u16)buffer[1] << 8 | buffer[0];
	device = (u16)buffer[3] << 8 | buffer[2];
	if (vendor != VENDOR_ID || device != DEVICE_ID) {
		dev_err(dev, "Invalid vendor/device id %04x/%04x\n",
			vendor, device);
		return -ENODEV;
	}

	ret = regmap_bulk_read(anx7688->regmap, FW_VERSION_REG, buffer, 2);
	if (ret) {
		dev_err(&client->dev, "Failed to read firmware version\n");
		return ret;
	}

	fwversion = (u16)buffer[0] << 8 | buffer[1];
	dev_info(dev, "ANX7688 firwmare version %02x.%02x\n",
		 buffer[0], buffer[1]);

	/* FW version >= 0.85 supports bandwidth/lane count registers */
	if (fwversion >= MINIMUM_FW_VERSION) {
		anx7688->filter = true;
	} else {
		/* Warn, but not fail, for backwards compatibility. */
		dev_warn(dev,
			 "Old ANX7688 FW version (%02x.%02x), not filtering\n",
			 buffer[0], buffer[1]);
	}

	anx7688->bridge.funcs = &anx7688_bridge_funcs;
	drm_bridge_add(&anx7688->bridge);

	return 0;
}

static int anx7688_i2c_remove(struct i2c_client *client)
{
	struct anx7688 *anx7688 = i2c_get_clientdata(client);

	drm_bridge_remove(&anx7688->bridge);

	return 0;
}

static const struct i2c_device_id anx7688_id[] = {
	{ "anx7688", 0 },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(i2c, anx7688_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id anx7688_match_table[] = {
	{ .compatible = "analogix,anx7688", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, anx7688_match_table);
#endif

static struct i2c_driver anx7688_driver = {
	.driver = {
		   .name = "anx7688",
		   .of_match_table = of_match_ptr(anx7688_match_table),
		  },
	.probe = anx7688_i2c_probe,
	.remove = anx7688_i2c_remove,
	.id_table = anx7688_id,
};

module_i2c_driver(anx7688_driver);

MODULE_DESCRIPTION("ANX7688 SlimPort Transmitter driver");
MODULE_AUTHOR("Nicolas Boichat <drinkcat@chromium.org>");
MODULE_LICENSE("GPL v2");
