/******************************************************************************
 *
 * Copyright(c) 2016 - 2018 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/

#ifndef _HALMAC_PCIE_8821C_H_
#define _HALMAC_PCIE_8821C_H_

#include "../../halmac_api.h"

#if HALMAC_8821C_SUPPORT

extern struct halmac_intf_phy_para pcie_gen1_phy_param_8821c[];
extern struct halmac_intf_phy_para pcie_gen2_phy_param_8821c[];

enum halmac_ret_status
mac_pwr_switch_pcie_8821c(struct halmac_adapter *adapter,
			  enum halmac_mac_power pwr);

enum halmac_ret_status
pcie_switch_8821c(struct halmac_adapter *adapter, enum halmac_pcie_cfg cfg);

enum halmac_ret_status
phy_cfg_pcie_8821c(struct halmac_adapter *adapter,
		   enum halmac_intf_phy_platform pltfm);

enum halmac_ret_status
intf_tun_pcie_8821c(struct halmac_adapter *adapter);

enum halmac_ret_status
auto_refclk_cal_8821c_pcie(struct halmac_adapter *adapter);

#endif /* HALMAC_8821C_SUPPORT */

#endif/* _HALMAC_PCIE_8821C_H_ */
