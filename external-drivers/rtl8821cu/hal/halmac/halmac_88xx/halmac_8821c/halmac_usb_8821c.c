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

#include "halmac_usb_8821c.h"
#include "halmac_pwr_seq_8821c.h"
#include "../halmac_init_88xx.h"
#include "../halmac_common_88xx.h"

#if HALMAC_8821C_SUPPORT

/**
 * mac_pwr_switch_usb_8821c() - switch mac power
 * @adapter : the adapter of halmac
 * @pwr : power state
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
mac_pwr_switch_usb_8821c(struct halmac_adapter *adapter,
			 enum halmac_mac_power pwr)
{
	u8 value8;
	u8 rpwm;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	PLTFM_MSG_TRACE("[TRACE]%s\n", __func__);
	PLTFM_MSG_TRACE("[TRACE]%x\n", pwr);
	PLTFM_MSG_TRACE("[TRACE]8821C pwr seq ver = %s\n",
			HALMAC_8821C_PWR_SEQ_VER);

	adapter->rpwm = HALMAC_REG_R8(0xFE58);

	/* Check FW still exist or not */
	if (HALMAC_REG_R16(REG_MCUFW_CTRL) == 0xC078) {
		/* Leave 32K */
		rpwm = (u8)((adapter->rpwm ^ BIT(7)) & 0x80);
		HALMAC_REG_W8(0xFE58, rpwm);
	}

	value8 = HALMAC_REG_R8(REG_CR);
	if (value8 == 0xEA) {
		adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_OFF;
	} else {
		if (BIT(0) == (HALMAC_REG_R8(REG_SYS_STATUS1 + 1) & BIT(0)))
			adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_OFF;
		else
			adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_ON;
	}

	/*Check if power switch is needed*/
	if (pwr == HALMAC_MAC_POWER_ON &&
	    adapter->halmac_state.mac_pwr == HALMAC_MAC_POWER_ON) {
		PLTFM_MSG_WARN("[WARN]power state unchange!!\n");
		return HALMAC_RET_PWR_UNCHANGE;
	}

	if (pwr == HALMAC_MAC_POWER_OFF) {
		if (pwr_seq_parser_88xx(adapter, card_dis_flow_8821c) !=
		    HALMAC_RET_SUCCESS) {
			PLTFM_MSG_ERR("[ERR]Handle power off cmd error\n");
			return HALMAC_RET_POWER_OFF_FAIL;
		}

		adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_OFF;
		adapter->halmac_state.dlfw_state = HALMAC_DLFW_NONE;
		init_adapter_dynamic_param_88xx(adapter);
	} else {
		if (pwr_seq_parser_88xx(adapter, card_en_flow_8821c) !=
		    HALMAC_RET_SUCCESS) {
			PLTFM_MSG_ERR("[ERR]Handle power on cmd error\n");
			return HALMAC_RET_POWER_ON_FAIL;
		}

		HALMAC_REG_W8_CLR(REG_SYS_STATUS1 + 1, BIT(0));

		if ((HALMAC_REG_R8(REG_SW_MDIO + 3) & BIT(0)) == BIT(0))
			PLTFM_MSG_ALWAYS("[ALWAYS]shall R reg twice!!\n");

		adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_ON;
	}

	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	return HALMAC_RET_SUCCESS;
}

/**
 * phy_cfg_usb_8821c() - phy config
 * @adapter : the adapter of halmac
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
phy_cfg_usb_8821c(struct halmac_adapter *adapter,
		  enum halmac_intf_phy_platform pltfm)
{
	enum halmac_ret_status status = HALMAC_RET_SUCCESS;

	PLTFM_MSG_TRACE("[TRACE]%s ===>\n", __func__);

	status = parse_intf_phy_88xx(adapter, usb2_phy_param_8821c, pltfm,
				     HAL_INTF_PHY_USB2);

	if (status != HALMAC_RET_SUCCESS)
		return status;

	status = parse_intf_phy_88xx(adapter, usb3_phy_param_8821c, pltfm,
				     HAL_INTF_PHY_USB3);

	if (status != HALMAC_RET_SUCCESS)
		return status;

	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_pcie_switch_8821c() - pcie gen1/gen2 switch
 * @adapter : the adapter of halmac
 * @pcie_cfg : gen1/gen2 selection
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
pcie_switch_usb_8821c(struct halmac_adapter *adapter, enum halmac_pcie_cfg cfg)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * intf_tun_usb_8821c() - usb interface fine tuning
 * @adapter : the adapter of halmac
 * Author : Ivan
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
intf_tun_usb_8821c(struct halmac_adapter *adapter)
{
	return HALMAC_RET_SUCCESS;
}

#endif /* HALMAC_8821C_SUPPORT */
