/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author: Lin Huang <hl@rock-chips.com>
 */

#ifndef __SOC_ROCKCHIP_PHY_TYPEC_H
#define __SOC_ROCKCHIP_PHY_TYPEC_H

struct usb3phy_reg {
	u32 offset;
	u32 enable_bit;
	u32 write_enable;
};

/**
 * struct rockchip_usb3phy_port_cfg: usb3-phy port configuration.
 * @reg: the base address for usb3-phy config.
 * @typec_conn_dir: the register of type-c connector direction.
 * @usb3tousb2_en: the register of type-c force usb2 to usb2 enable.
 * @external_psm: the register of type-c phy external psm clock.
 * @pipe_status: the register of type-c phy pipe status.
 * @usb3_host_disable: the register of type-c usb3 host disable.
 * @usb3_host_port: the register of type-c usb3 host port.
 * @uphy_dp_sel: the register of type-c phy DP select control.
 */
struct rockchip_usb3phy_port_cfg {
	unsigned int reg;
	struct usb3phy_reg typec_conn_dir;
	struct usb3phy_reg usb3tousb2_en;
	struct usb3phy_reg external_psm;
	struct usb3phy_reg pipe_status;
	struct usb3phy_reg usb3_host_disable;
	struct usb3phy_reg usb3_host_port;
	struct usb3phy_reg uphy_dp_sel;
};

struct phy_config {
	int swing;
	int pe;
};

struct rockchip_typec_phy {
	struct device *dev;
	void __iomem *base;
	struct extcon_dev *extcon;
	struct regmap *grf_regs;
	struct clk *clk_core;
	struct clk *clk_ref;
	struct reset_control *uphy_rst;
	struct reset_control *pipe_rst;
	struct reset_control *tcphy_rst;
	const struct rockchip_usb3phy_port_cfg *port_cfgs;
	/* mutex to protect access to individual PHYs */
	struct mutex lock;
	bool flip;
	u8 mode;
	struct phy_config config[3][4];
	int (*typec_phy_config)(struct phy *phy, int link_rate,
				int lanes, u8 swing, u8 pre_emp);
};

#endif
