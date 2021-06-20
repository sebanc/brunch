/*
 * Copyright (c) 2016 MediaTek Inc.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <asm/unaligned.h>
#include <drm/drm_panel.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>

#define PAGE1_VSTART		0x6b
#define PAGE2_SPI_CFG3		0x82
#define I2C_TO_SPI_RESET	0x20
#define PAGE2_ROMADD_BYTE1	0x8e
#define PAGE2_ROMADD_BYTE2	0x8f
#define PAGE2_SWSPI_WDATA	0x90
#define PAGE2_SWSPI_RDATA	0x91
#define PAGE2_SWSPI_LEN		0x92
#define PAGE2_SWSPI_CTL		0x93
#define TRIGGER_NO_READBACK	0x05
#define TRIGGER_READBACK	0x01
#define PAGE2_SPI_STATUS	0x9e
#define SPI_READY		0x0c
#define PAGE2_GPIO_L		0xa6
#define PAGE2_GPIO_H		0xa7
#define PS_GPIO9		BIT(1)
#define PAGE2_IROM_CTRL		0xb0
#define IROM_ENABLE		0xc0
#define IROM_DISABLE		0x80
#define PAGE2_SW_RESET		0xbc
#define SPI_SW_RESET		BIT(7)
#define MPU_SW_RESET		BIT(6)
#define PAGE2_ENCTLSPI_WR	0xda
#define PAGE2_I2C_BYPASS	0xea
#define I2C_BYPASS_EN		0xd0
#define PAGE2_MCS_EN		0xf3
#define MCS_EN			BIT(0)
#define PAGE3_SET_ADD		0xfe
#define PAGE3_SET_VAL		0xff
#define VDO_CTL_ADD		0x13
#define VDO_DIS			0x18
#define VDO_EN			0x1c
#define PAGE4_REV_L		0xf0
#define PAGE4_REV_H		0xf1
#define PAGE4_CHIP_L		0xf2
#define PAGE4_CHIP_H		0xf3

#define PAGE0_DP_CNTL	0
#define PAGE1_VDO_BDG	1
#define PAGE2_TOP_CNTL	2
#define PAGE3_DSI_CNTL1	3
#define PAGE4_MIPI_PHY	4
#define PAGE5_VPLL	5
#define PAGE6_DSI_CNTL2	6
#define PAGE7_SPI_CNTL	7
#define MAX_DEVS		0x8

/* Firmware */
#define PS_FW_NAME		"ps864x_fw.bin"

#define FW_CHIP_ID_OFFSET	0
#define FW_VERSION_OFFSET	2
#define EDID_I2C_ADDR		0x50

#define WRITE_STATUS_REG_CMD	0x01
#define READ_STATUS_REG_CMD	0x05
#define BUSY			BIT(0)
#define CLEAR_ALL_PROTECT	0x00
#define BLK_PROTECT_BITS	0x0c
#define STATUS_REG_PROTECT	BIT(7)
#define WRITE_ENABLE_CMD	0x06
#define CHIP_ERASE_CMD		0xc7

struct ps8640_info {
	u8 family_id;
	u8 variant_id;
	u16 version;
};

struct ps8640 {
	struct drm_connector connector;
	struct drm_bridge bridge;
	struct edid *edid;
	struct mipi_dsi_device *dsi;
	struct i2c_client *page[MAX_DEVS];
	struct regulator_bulk_data supplies[2];
	struct drm_panel *panel;
	struct gpio_desc *gpio_reset;
	struct gpio_desc *gpio_power_down;
	struct gpio_desc *gpio_mode_sel;
	bool enabled;

	/* firmware file info */
	struct ps8640_info info;
	bool in_fw_update;
	/* for firmware update protect */
	struct mutex fw_mutex;
};

static const u8 enc_ctrl_code[6] = { 0xaa, 0x55, 0x50, 0x41, 0x52, 0x44 };
static const u8 hw_chip_id[4] = { 0x00, 0x0a, 0x00, 0x30 };

static inline struct ps8640 *bridge_to_ps8640(struct drm_bridge *e)
{
	return container_of(e, struct ps8640, bridge);
}

static inline struct ps8640 *connector_to_ps8640(struct drm_connector *e)
{
	return container_of(e, struct ps8640, connector);
}

static int ps8640_read(struct i2c_client *client, u8 reg, u8 *data,
		       u16 data_len)
{
	int ret;
	struct i2c_msg msgs[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		},
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = data_len,
		 .buf = data,
		}
	};

	ret = i2c_transfer(client->adapter, msgs, 2);

	if (ret == 2)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int ps8640_write_bytes(struct i2c_client *client, const u8 *data,
			      u16 data_len)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = data_len;
	msg.buf = (u8 *)data;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret == 1)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int ps8640_write_byte(struct i2c_client *client, u8 reg,  u8 data)
{
	u8 buf[] = { reg, data };

	return ps8640_write_bytes(client, buf, sizeof(buf));
}

static void ps8640_get_mcu_fw_version(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE5_VPLL];
	u8 fw_ver[2];

	ps8640_read(client, 0x4, fw_ver, sizeof(fw_ver));
	ps_bridge->info.version = (fw_ver[0] << 8) | fw_ver[1];

	DRM_INFO_ONCE("ps8640 rom fw version %d.%d\n", fw_ver[0], fw_ver[1]);
}

static int ps8640_bridge_unmute(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE3_DSI_CNTL1];
	u8 vdo_ctrl_buf[3] = { PAGE3_SET_ADD, VDO_CTL_ADD, VDO_EN };

	return ps8640_write_bytes(client, vdo_ctrl_buf, sizeof(vdo_ctrl_buf));
}

static int ps8640_bridge_mute(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE3_DSI_CNTL1];
	u8 vdo_ctrl_buf[3] = { PAGE3_SET_ADD, VDO_CTL_ADD, VDO_DIS };

	return ps8640_write_bytes(client, vdo_ctrl_buf, sizeof(vdo_ctrl_buf));
}

static void ps8640_pre_enable(struct drm_bridge *bridge)
{
	struct ps8640 *ps_bridge = bridge_to_ps8640(bridge);
	struct i2c_client *client = ps_bridge->page[PAGE2_TOP_CNTL];
	struct i2c_client *page1 = ps_bridge->page[PAGE1_VDO_BDG];
	int err;
	u8 set_vdo_done, mcs_en, vstart;
	ktime_t timeout;

	if (ps_bridge->in_fw_update)
		return;

	if (ps_bridge->enabled)
		return;

	err = drm_panel_prepare(ps_bridge->panel);
	if (err < 0) {
		DRM_ERROR("failed to prepare panel: %d\n", err);
		return;
	}

	err = regulator_bulk_enable(ARRAY_SIZE(ps_bridge->supplies),
				    ps_bridge->supplies);
	if (err < 0) {
		DRM_ERROR("cannot enable regulators %d\n", err);
		goto err_panel_unprepare;
	}

	gpiod_set_value(ps_bridge->gpio_power_down, 1);
	gpiod_set_value(ps_bridge->gpio_reset, 0);
	usleep_range(2000, 2500);
	gpiod_set_value(ps_bridge->gpio_reset, 1);

	/*
	 * Wait for the ps8640 embed mcu ready
	 * First wait 200ms and then check the mcu ready flag every 20ms
	 */
	msleep(200);

	timeout = ktime_add_ms(ktime_get(), 200);
	for (;;) {
		err = ps8640_read(client, PAGE2_GPIO_H, &set_vdo_done, 1);
		if (err < 0) {
			DRM_ERROR("failed read PAGE2_GPIO_H: %d\n", err);
			goto err_regulators_disable;
		}
		if ((set_vdo_done & PS_GPIO9) == PS_GPIO9)
			break;
		if (ktime_compare(ktime_get(), timeout) > 0)
			break;
		msleep(20);
	}

	msleep(50);

	ps8640_read(page1, PAGE1_VSTART, &vstart, 1);
	DRM_INFO("PS8640 PAGE1.0x6B = 0x%x\n", vstart);

	/**
	 * The Manufacturer Command Set (MCS) is a device dependent interface
	 * intended for factory programming of the display module default
	 * parameters. Once the display module is configured, the MCS shall be
	 * disabled by the manufacturer. Once disabled, all MCS commands are
	 * ignored by the display interface.
	 */
	ps8640_read(client, PAGE2_MCS_EN, &mcs_en, 1);
	ps8640_write_byte(client, PAGE2_MCS_EN, mcs_en & ~MCS_EN);

	if (ps_bridge->info.version == 0)
		ps8640_get_mcu_fw_version(ps_bridge);

	err = ps8640_bridge_unmute(ps_bridge);
	if (err)
		DRM_ERROR("failed to enable unmutevideo: %d\n", err);
	/* Switch access edp panel's edid through i2c */
	ps8640_write_byte(client, PAGE2_I2C_BYPASS, I2C_BYPASS_EN);
	ps_bridge->enabled = true;

	return;

err_regulators_disable:
	regulator_bulk_disable(ARRAY_SIZE(ps_bridge->supplies),
			       ps_bridge->supplies);
err_panel_unprepare:
	drm_panel_unprepare(ps_bridge->panel);
}

static void ps8640_enable(struct drm_bridge *bridge)
{
	struct ps8640 *ps_bridge = bridge_to_ps8640(bridge);
	int err;

	err = drm_panel_enable(ps_bridge->panel);
	if (err < 0)
		DRM_ERROR("failed to enable panel: %d\n", err);
}

static void ps8640_disable(struct drm_bridge *bridge)
{
	struct ps8640 *ps_bridge = bridge_to_ps8640(bridge);
	int err;

	err = drm_panel_disable(ps_bridge->panel);
	if (err < 0)
		DRM_ERROR("failed to disable panel: %d\n", err);
}

static void ps8640_post_disable(struct drm_bridge *bridge)
{
	struct ps8640 *ps_bridge = bridge_to_ps8640(bridge);
	int err;

	if (ps_bridge->in_fw_update)
		return;

	if (!ps_bridge->enabled)
		return;

	ps_bridge->enabled = false;

	err = ps8640_bridge_mute(ps_bridge);
	if (err < 0)
		DRM_ERROR("failed to unmutevideo: %d\n", err);

	gpiod_set_value(ps_bridge->gpio_reset, 0);
	gpiod_set_value(ps_bridge->gpio_power_down, 0);
	err = regulator_bulk_disable(ARRAY_SIZE(ps_bridge->supplies),
				     ps_bridge->supplies);
	if (err < 0)
		DRM_ERROR("cannot disable regulators %d\n", err);

	err = drm_panel_unprepare(ps_bridge->panel);
	if (err)
		DRM_ERROR("failed to unprepare panel: %d\n", err);
}

static int ps8640_get_modes(struct drm_connector *connector)
{
	struct ps8640 *ps_bridge = connector_to_ps8640(connector);
	struct edid *edid;
	int num_modes = 0;
	bool power_off;

	if (ps_bridge->edid)
		return drm_add_edid_modes(connector, ps_bridge->edid);

	power_off = !ps_bridge->enabled;
	ps8640_pre_enable(&ps_bridge->bridge);

	edid = drm_get_edid(connector, ps_bridge->page[0]->adapter);
	if (!edid)
		goto out;

	ps_bridge->edid = edid;
	drm_connector_update_edid_property(connector, ps_bridge->edid);
	num_modes = drm_add_edid_modes(connector, ps_bridge->edid);

out:
	if (power_off)
		ps8640_post_disable(&ps_bridge->bridge);

	return num_modes;
}

static const struct drm_connector_helper_funcs ps8640_connector_helper_funcs = {
	.get_modes = ps8640_get_modes,
};

static enum drm_connector_status ps8640_detect(struct drm_connector *connector,
					       bool force)
{
	return connector_status_connected;
}

static const struct drm_connector_funcs ps8640_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = ps8640_detect,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

int ps8640_bridge_attach(struct drm_bridge *bridge)
{
	struct ps8640 *ps_bridge = bridge_to_ps8640(bridge);
	struct device *dev = &ps_bridge->page[0]->dev;
	struct device_node *in_ep, *dsi_node = NULL;
	struct mipi_dsi_device *dsi;
	struct mipi_dsi_host *host = NULL;
	int ret;
	const struct mipi_dsi_device_info info = { .type = "ps8640",
						   .channel = 0,
						   .node = NULL,
						 };

	ret = drm_connector_init(bridge->dev, &ps_bridge->connector,
				 &ps8640_connector_funcs,
				 DRM_MODE_CONNECTOR_eDP);

	if (ret) {
		DRM_ERROR("Failed to initialize connector with drm: %d\n", ret);
		return ret;
	}

	drm_connector_helper_add(&ps_bridge->connector,
				 &ps8640_connector_helper_funcs);

	ps_bridge->connector.dpms = DRM_MODE_DPMS_ON;
	drm_connector_attach_encoder(&ps_bridge->connector,
					  bridge->encoder);

	if (ps_bridge->panel)
		drm_panel_attach(ps_bridge->panel, &ps_bridge->connector);

	/* port@0 is ps8640 dsi input port */
	in_ep = of_graph_get_endpoint_by_regs(dev->of_node, 0, -1);
	if (in_ep) {
		dsi_node = of_graph_get_remote_port_parent(in_ep);
		of_node_put(in_ep);
	}

	if (dsi_node) {
		host = of_find_mipi_dsi_host_by_node(dsi_node);
		of_node_put(dsi_node);
		if (!host) {
			ret = -ENODEV;
			goto err;
		}
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) {
		dev_err(dev, "failed to create dsi device\n");
		ret = PTR_ERR(dsi);
		goto err;
	}

	ps_bridge->dsi = dsi;

	dsi->host = host;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
				     MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;
	ret = mipi_dsi_attach(dsi);
	if (ret)
		goto err_dsi_attach;

	return 0;

err_dsi_attach:
	mipi_dsi_device_unregister(dsi);
err:
	if (ps_bridge->panel)
		drm_panel_detach(ps_bridge->panel);
	drm_connector_cleanup(&ps_bridge->connector);
	return ret;
}

static const struct drm_bridge_funcs ps8640_bridge_funcs = {
	.attach = ps8640_bridge_attach,
	.disable = ps8640_disable,
	.post_disable = ps8640_post_disable,
	.pre_enable = ps8640_pre_enable,
	.enable = ps8640_enable,
};

/* Firmware Version is returned as Major.Minor */
static ssize_t ps8640_fw_version_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ps8640 *ps_bridge = dev_get_drvdata(dev);
	struct ps8640_info *info = &ps_bridge->info;

	return scnprintf(buf, PAGE_SIZE, "%u.%u\n", info->version >> 8,
			 info->version & 0xff);
}

/* Hardware Version is returned as FamilyID.VariantID */
static ssize_t ps8640_hw_version_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct ps8640 *ps_bridge = dev_get_drvdata(dev);
	struct ps8640_info *info = &ps_bridge->info;

	return scnprintf(buf, PAGE_SIZE, "ps%u.%u\n", info->family_id,
			 info->variant_id);
}

static int ps8640_spi_send_cmd(struct ps8640 *ps_bridge, u8 *cmd, u8 cmd_len)
{
	struct i2c_client *client = ps_bridge->page[PAGE2_TOP_CNTL];
	u8 i, buf[3] = { PAGE2_SWSPI_LEN, cmd_len - 1, TRIGGER_NO_READBACK };
	int ret;

	ret = ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_ENABLE);
	if (ret)
		goto err;

	/* write command in write port */
	for (i = 0; i < cmd_len; i++) {
		ret = ps8640_write_byte(client, PAGE2_SWSPI_WDATA, cmd[i]);
		if (ret)
			goto err_irom_disable;
	}

	ret = ps8640_write_bytes(client, buf, sizeof(buf));
	if (ret)
		goto err_irom_disable;

	ret = ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_DISABLE);
	if (ret)
		goto err;

	return 0;
err_irom_disable:
	ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_DISABLE);
err:
	dev_err(&client->dev, "send command err: %d\n", ret);
	return ret;
}

static int ps8640_wait_spi_ready(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE2_TOP_CNTL];
	u8 spi_rdy_st;
	ktime_t timeout;

	timeout = ktime_add_ms(ktime_get(), 200);
	for (;;) {
		ps8640_read(client, PAGE2_SPI_STATUS, &spi_rdy_st, 1);
		if ((spi_rdy_st & SPI_READY) != SPI_READY)
			break;

		if (ktime_compare(ktime_get(), timeout) > 0) {
			dev_err(&client->dev, "wait spi ready timeout\n");
			return -EBUSY;
		}

		msleep(20);
	}

	return 0;
}

static int ps8640_wait_spi_nobusy(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE2_TOP_CNTL];
	u8 spi_status, buf[3] = { PAGE2_SWSPI_LEN, 0, TRIGGER_READBACK };
	int ret;
	ktime_t timeout;

	timeout = ktime_add_ms(ktime_get(), 500);
	for (;;) {
		/* 0x05 RDSR; Read-Status-Register */
		ret = ps8640_write_byte(client, PAGE2_SWSPI_WDATA,
					READ_STATUS_REG_CMD);
		if (ret)
			goto err_send_cmd_exit;

		ret = ps8640_write_bytes(client, buf, 3);
		if (ret)
			goto err_send_cmd_exit;

		/* delay for cmd send */
		usleep_range(300, 500);
		/* wait for SPI ROM until not busy */
		ret = ps8640_read(client, PAGE2_SWSPI_RDATA, &spi_status, 1);
		if (ret)
			goto err_send_cmd_exit;

		if (!(spi_status & BUSY))
			break;

		if (ktime_compare(ktime_get(), timeout) > 0) {
			dev_err(&client->dev, "wait spi no busy timeout: %d\n",
				ret);
			return -EBUSY;
		}
	}

	return 0;

err_send_cmd_exit:
	dev_err(&client->dev, "send command err: %d\n", ret);
	return ret;
}

static int ps8640_wait_rom_idle(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE0_DP_CNTL];
	int ret;

	ret = ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_ENABLE);
	if (ret)
		goto exit;

	ret = ps8640_wait_spi_ready(ps_bridge);
	if (ret)
		goto err_spi;

	ret = ps8640_wait_spi_nobusy(ps_bridge);
	if (ret)
		goto err_spi;

	ret = ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_DISABLE);
	if (ret)
		goto exit;

	return 0;

err_spi:
	ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_DISABLE);
exit:
	dev_err(&client->dev, "wait ps8640 rom idle fail: %d\n", ret);

	return ret;
}

static int ps8640_spi_dl_mode(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE2_TOP_CNTL];
	int ret;

	/* switch ps8640 mode to spi dl mode */
	if (ps_bridge->gpio_mode_sel)
		gpiod_set_value(ps_bridge->gpio_mode_sel, 0);

	/* reset spi interface */
	ret = ps8640_write_byte(client, PAGE2_SW_RESET,
				SPI_SW_RESET | MPU_SW_RESET);
	if (ret)
		goto exit;

	ret = ps8640_write_byte(client, PAGE2_SW_RESET, MPU_SW_RESET);
	if (ret)
		goto exit;

	return 0;

exit:
	dev_err(&client->dev, "fail reset spi interface: %d\n", ret);

	return ret;
}

static int ps8640_rom_prepare(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE2_TOP_CNTL];
	struct device *dev = &client->dev;
	u8 i, cmd[2];
	int ret;

	cmd[0] = WRITE_ENABLE_CMD;
	ret = ps8640_spi_send_cmd(ps_bridge, cmd, 1);
	if (ret) {
		dev_err(dev, "failed enable-write-status-register: %d\n", ret);
		return ret;
	}

	cmd[0] = WRITE_STATUS_REG_CMD;
	cmd[1] = CLEAR_ALL_PROTECT;
	ret = ps8640_spi_send_cmd(ps_bridge, cmd, 2);
	if (ret) {
		dev_err(dev, "fail disable all protection: %d\n", ret);
		return ret;
	}

	/* wait for SPI module ready */
	ret = ps8640_wait_rom_idle(ps_bridge);
	if (ret) {
		dev_err(dev, "fail wait rom idle: %d\n", ret);
		return ret;
	}

	ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_ENABLE);
	for (i = 0; i < ARRAY_SIZE(enc_ctrl_code); i++)
		ps8640_write_byte(client, PAGE2_ENCTLSPI_WR, enc_ctrl_code[i]);
	ps8640_write_byte(client, PAGE2_IROM_CTRL, IROM_DISABLE);

	/* Enable-Write-Status-Register */
	cmd[0] = WRITE_ENABLE_CMD;
	ret = ps8640_spi_send_cmd(ps_bridge, cmd, 1);
	if (ret) {
		dev_err(dev, "fail enable-write-status-register: %d\n", ret);
		return ret;
	}

	/* chip erase command */
	cmd[0] = CHIP_ERASE_CMD;
	ret = ps8640_spi_send_cmd(ps_bridge, cmd, 1);
	if (ret) {
		dev_err(dev, "fail disable all protection: %d\n", ret);
		return ret;
	}

	ret = ps8640_wait_rom_idle(ps_bridge);
	if (ret) {
		dev_err(dev, "fail wait rom idle: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ps8640_check_chip_id(struct ps8640 *ps_bridge)
{
	struct i2c_client *client = ps_bridge->page[PAGE4_MIPI_PHY];
	u8 buf[4];

	ps8640_read(client, PAGE4_REV_L, buf, 4);
	return memcmp(buf, hw_chip_id, sizeof(buf));
}

static int ps8640_validate_firmware(struct ps8640 *ps_bridge,
				    const struct firmware *fw)
{
	struct i2c_client *client = ps_bridge->page[0];
	u16 fw_chip_id;

	/*
	 * Get the chip_id from the firmware. Make sure that it is the
	 * right controller to do the firmware and config update.
	 */
	fw_chip_id = get_unaligned_le16(fw->data + FW_CHIP_ID_OFFSET);

	if (fw_chip_id != 0x8640 && ps8640_check_chip_id(ps_bridge) == 0) {
		dev_err(&client->dev,
			"chip id mismatch: fw 0x%x vs. chip 0x8640\n",
			fw_chip_id);
		return -EINVAL;
	}

	return 0;
}

static int ps8640_write_rom(struct ps8640 *ps_bridge, const struct firmware *fw)
{
	struct i2c_client *client = ps_bridge->page[PAGE0_DP_CNTL];
	struct device *dev = &client->dev;
	struct i2c_client *client2 = ps_bridge->page[PAGE2_TOP_CNTL];
	struct i2c_client *client7 = ps_bridge->page[PAGE7_SPI_CNTL];
	size_t pos, cpy_len;
	u8 buf[257];
	int ret;

	ps8640_write_byte(client2, PAGE2_SPI_CFG3, I2C_TO_SPI_RESET);
	msleep(100);
	ps8640_write_byte(client2, PAGE2_SPI_CFG3, 0x00);

	for (pos = 0; pos < fw->size; pos += cpy_len) {
		buf[0] = PAGE2_ROMADD_BYTE1;
		buf[1] = pos >> 8;
		buf[2] = pos >> 16;
		ret = ps8640_write_bytes(client2, buf, 3);
		if (ret)
			goto error;
		cpy_len = fw->size >= 256 + pos ? 256 : fw->size - pos;
		buf[0] = 0;
		memcpy(buf + 1, fw->data + pos, cpy_len);
		ret = ps8640_write_bytes(client7, buf, cpy_len + 1);
		if (ret)
			goto error;

		dev_dbg(dev, "fw update completed %zu / %zu bytes\n", pos,
			fw->size);
	}
	return 0;

error:
	dev_err(dev, "failed write external flash, %d\n", ret);
	return ret;
}

static int ps8640_spi_normal_mode(struct ps8640 *ps_bridge)
{
	u8 cmd[2];
	struct i2c_client *client = ps_bridge->page[PAGE2_TOP_CNTL];

	/* Enable-Write-Status-Register */
	cmd[0] = WRITE_ENABLE_CMD;
	ps8640_spi_send_cmd(ps_bridge, cmd, 1);

	/* protect BPL/BP0/BP1 */
	cmd[0] = WRITE_STATUS_REG_CMD;
	cmd[1] = BLK_PROTECT_BITS | STATUS_REG_PROTECT;
	ps8640_spi_send_cmd(ps_bridge, cmd, 2);

	/* wait for SPI rom ready */
	ps8640_wait_rom_idle(ps_bridge);

	/* disable PS8640 mapping function */
	ps8640_write_byte(client, PAGE2_ENCTLSPI_WR, 0x00);

	if (ps_bridge->gpio_mode_sel)
		gpiod_set_value(ps_bridge->gpio_mode_sel, 1);
	return 0;
}

static int ps8640_enter_bl(struct ps8640 *ps_bridge)
{
	ps_bridge->in_fw_update = true;
	return ps8640_spi_dl_mode(ps_bridge);
}

static void ps8640_exit_bl(struct ps8640 *ps_bridge, const struct firmware *fw)
{
	ps8640_spi_normal_mode(ps_bridge);
	ps_bridge->in_fw_update = false;
}

static int ps8640_load_fw(struct ps8640 *ps_bridge, const struct firmware *fw)
{
	struct i2c_client *client = ps_bridge->page[PAGE0_DP_CNTL];
	struct device *dev = &client->dev;
	int ret;
	bool ps8640_status_backup = ps_bridge->enabled;

	ret = ps8640_validate_firmware(ps_bridge, fw);
	if (ret)
		return ret;

	mutex_lock(&ps_bridge->fw_mutex);
	if (!ps_bridge->in_fw_update) {
		if (!ps8640_status_backup)
			ps8640_pre_enable(&ps_bridge->bridge);

		ret = ps8640_enter_bl(ps_bridge);
		if (ret)
			goto exit;
	}

	ret = ps8640_rom_prepare(ps_bridge);
	if (ret)
		goto exit;

	ret = ps8640_write_rom(ps_bridge, fw);

exit:
	if (ret)
		dev_err(dev, "Failed to load firmware, %d\n", ret);

	ps8640_exit_bl(ps_bridge, fw);
	if (!ps8640_status_backup)
		ps8640_post_disable(&ps_bridge->bridge);
	mutex_unlock(&ps_bridge->fw_mutex);
	return ret;
}

static ssize_t ps8640_update_fw_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ps8640 *ps_bridge = i2c_get_clientdata(client);
	const struct firmware *fw;
	int error;

	error = request_firmware(&fw, PS_FW_NAME, dev);
	if (error) {
		dev_err(dev, "Unable to open firmware %s: %d\n",
			PS_FW_NAME, error);
		return error;
	}

	error = ps8640_load_fw(ps_bridge, fw);
	if (error)
		dev_err(dev, "The firmware update failed(%d)\n", error);
	else
		dev_info(dev, "The firmware update succeeded\n");

	release_firmware(fw);
	return error ? error : count;
}

static DEVICE_ATTR(fw_version, S_IRUGO, ps8640_fw_version_show, NULL);
static DEVICE_ATTR(hw_version, S_IRUGO, ps8640_hw_version_show, NULL);
static DEVICE_ATTR(update_fw, S_IWUSR, NULL, ps8640_update_fw_store);

static struct attribute *ps8640_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_update_fw.attr,
	NULL
};

static const struct attribute_group ps8640_attr_group = {
	.attrs = ps8640_attrs,
};

static void ps8640_remove_sysfs_group(void *data)
{
	struct ps8640 *ps_bridge = data;

	sysfs_remove_group(&ps_bridge->page[0]->dev.kobj, &ps8640_attr_group);
}

static int ps8640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ps8640 *ps_bridge;
	struct device_node *np = dev->of_node;
	struct device_node *port, *out_ep;
	struct device_node *panel_node = NULL;
	int ret;
	u32 i;

	ps_bridge = devm_kzalloc(dev, sizeof(*ps_bridge), GFP_KERNEL);
	if (!ps_bridge)
		return -ENOMEM;

	/* port@1 is ps8640 output port */
	port = of_graph_get_port_by_id(np, 1);
	if (port) {
		out_ep = of_get_child_by_name(port, "endpoint");
		of_node_put(port);
		if (out_ep) {
			panel_node = of_graph_get_remote_port_parent(out_ep);
			of_node_put(out_ep);
		}
	}
	if (panel_node) {
		ps_bridge->panel = of_drm_find_panel(panel_node);
		of_node_put(panel_node);
		if (IS_ERR_OR_NULL(ps_bridge->panel))
			return -EPROBE_DEFER;
	}

	mutex_init(&ps_bridge->fw_mutex);
	ps_bridge->supplies[0].supply = "vdd33";
	ps_bridge->supplies[1].supply = "vdd12";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ps_bridge->supplies),
				      ps_bridge->supplies);
	if (ret) {
		dev_info(dev, "failed to get regulators: %d\n", ret);
		return ret;
	}

	ps_bridge->gpio_mode_sel = devm_gpiod_get_optional(&client->dev,
							     "mode-sel",
							     GPIOD_OUT_HIGH);
	if (IS_ERR(ps_bridge->gpio_mode_sel)) {
		ret = PTR_ERR(ps_bridge->gpio_mode_sel);
		dev_err(dev, "cannot get mode-sel %d\n", ret);
		return ret;
	}

	ps_bridge->gpio_power_down = devm_gpiod_get(&client->dev, "sleep",
					       GPIOD_OUT_LOW);
	if (IS_ERR(ps_bridge->gpio_power_down)) {
		ret = PTR_ERR(ps_bridge->gpio_power_down);
		dev_err(dev, "cannot get sleep: %d\n", ret);
		return ret;
	}

	/*
	 * Request the reset pin low to avoid the bridge being
	 * initialized prematurely
	 */
	ps_bridge->gpio_reset = devm_gpiod_get(&client->dev, "reset",
					       GPIOD_OUT_LOW);
	if (IS_ERR(ps_bridge->gpio_reset)) {
		ret = PTR_ERR(ps_bridge->gpio_reset);
		dev_err(dev, "cannot get reset: %d\n", ret);
		return ret;
	}

	ps_bridge->bridge.funcs = &ps8640_bridge_funcs;
	ps_bridge->bridge.of_node = dev->of_node;

	ps_bridge->page[0] = client;

	/*
	 * ps8640 uses multiple addresses, use dummy devices for them
	 * page[0]: for DP control
	 * page[1]: for VIDEO Bridge
	 * page[2]: for control top
	 * page[3]: for DSI Link Control1
	 * page[4]: for MIPI Phy
	 * page[5]: for VPLL
	 * page[6]: for DSI Link Control2
	 * page[7]: for spi rom mapping
	 */
	for (i = 1; i < MAX_DEVS; i++) {
		ps_bridge->page[i] = i2c_new_dummy(client->adapter,
						   client->addr + i);
		if (!ps_bridge->page[i]) {
			dev_err(dev, "failed i2c dummy device, address%02x\n",
				client->addr + i);
			ret = -EBUSY;
			goto exit_dummy;
		}
	}
	i2c_set_clientdata(client, ps_bridge);

	ret = sysfs_create_group(&client->dev.kobj, &ps8640_attr_group);
	if (ret) {
		dev_err(dev, "failed to create sysfs entries: %d\n", ret);
		goto exit_dummy;
	}

	ret = devm_add_action(dev, ps8640_remove_sysfs_group, ps_bridge);
	if (ret) {
		dev_err(dev, "failed to add sysfs cleanup action: %d\n", ret);
		goto exit_remove_sysfs;
	}

	drm_bridge_add(&ps_bridge->bridge);
	return 0;

exit_remove_sysfs:
	sysfs_remove_group(&ps_bridge->page[0]->dev.kobj, &ps8640_attr_group);
exit_dummy:
	while (--i)
		i2c_unregister_device(ps_bridge->page[i]);
	return ret;
}

static int ps8640_remove(struct i2c_client *client)
{
	struct ps8640 *ps_bridge = i2c_get_clientdata(client);
	int i = MAX_DEVS;

	drm_bridge_remove(&ps_bridge->bridge);
	sysfs_remove_group(&ps_bridge->page[0]->dev.kobj, &ps8640_attr_group);
	while (--i)
		i2c_unregister_device(ps_bridge->page[i]);
	kfree(ps_bridge->edid);
	ps_bridge->edid = NULL;

	return 0;
}

static const struct i2c_device_id ps8640_i2c_table[] = {
	{ "ps8640", 0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(i2c, ps8640_i2c_table);

static const struct of_device_id ps8640_match[] = {
	{ .compatible = "parade,ps8640" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ps8640_match);

static struct i2c_driver ps8640_driver = {
	.id_table = ps8640_i2c_table,
	.probe = ps8640_probe,
	.remove = ps8640_remove,
	.driver = {
		.name = "ps8640",
		.of_match_table = ps8640_match,
	},
};
module_i2c_driver(ps8640_driver);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_AUTHOR("CK Hu <ck.hu@mediatek.com>");
MODULE_DESCRIPTION("PARADE ps8640 DSI-eDP converter driver");
MODULE_LICENSE("GPL v2");
