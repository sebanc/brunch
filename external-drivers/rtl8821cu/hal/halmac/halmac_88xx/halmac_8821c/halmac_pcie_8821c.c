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

#include "halmac_pcie_8821c.h"
#include "halmac_pwr_seq_8821c.h"
#include "../halmac_init_88xx.h"
#include "../halmac_common_88xx.h"
#include "../halmac_pcie_88xx.h"
#include "halmac_8821c_cfg.h"

#if HALMAC_8821C_SUPPORT

#define INTF_INTGRA_MINREF	90
#define INTF_INTGRA_HOSTREF	100

static u16
get_target(struct halmac_adapter *adapter);

static enum halmac_ret_status
freerun_delay_us(struct halmac_adapter *adapter, u16 delay);

/**
 * mac_pwr_switch_pcie_8821c() - switch mac power
 * @adapter : the adapter of halmac
 * @pwr : power state
 * Author : KaiYuan Chang / Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
mac_pwr_switch_pcie_8821c(struct halmac_adapter *adapter,
			  enum halmac_mac_power pwr)
{
	u8 value8;
	u8 rpwm;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;
	enum halmac_ret_status status;

	PLTFM_MSG_TRACE("[TRACE]%s ===>\n", __func__);
	PLTFM_MSG_TRACE("[TRACE]pwr = %x\n", pwr);
	PLTFM_MSG_TRACE("[TRACE]8821C pwr seq ver = %s\n",
			HALMAC_8821C_PWR_SEQ_VER);

	adapter->rpwm = HALMAC_REG_R8(REG_PCIE_HRPWM1_V1);

	/* Check FW still exist or not */
	if (HALMAC_REG_R16(REG_MCUFW_CTRL) == 0xC078) {
		/* Leave 32K */
		rpwm = (u8)((adapter->rpwm ^ BIT(7)) & 0x80);
		HALMAC_REG_W8(REG_PCIE_HRPWM1_V1, rpwm);
	}

	value8 = HALMAC_REG_R8(REG_CR);
	if (value8 == 0xEA)
		adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_OFF;
	else
		adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_ON;

	/* Check if power switch is needed */
	if (pwr == HALMAC_MAC_POWER_ON &&
	    adapter->halmac_state.mac_pwr == HALMAC_MAC_POWER_ON) {
		PLTFM_MSG_WARN("[WARN]power state unchange!!\n");
		return HALMAC_RET_PWR_UNCHANGE;
	}

	if (pwr == HALMAC_MAC_POWER_OFF) {
		status = trxdma_check_idle_88xx(adapter);
		if (status != HALMAC_RET_SUCCESS)
			return status;
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

		adapter->halmac_state.mac_pwr = HALMAC_MAC_POWER_ON;
	}

	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_pcie_switch_8821c() - pcie gen1/gen2 switch
 * @adapter : the adapter of halmac
 * @cfg : gen1/gen2 selection
 * Author : KaiYuan Chang / Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
pcie_switch_8821c(struct halmac_adapter *adapter, enum halmac_pcie_cfg cfg)
{
	return HALMAC_RET_SUCCESS;
}

/**
 * phy_cfg_pcie_8821c() - phy config
 * @adapter : the adapter of halmac
 * Author : KaiYuan Chang / Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
phy_cfg_pcie_8821c(struct halmac_adapter *adapter,
		   enum halmac_intf_phy_platform pltfm)
{
	enum halmac_ret_status status = HALMAC_RET_SUCCESS;

	PLTFM_MSG_TRACE("[TRACE]%s ===>\n", __func__);

	status = parse_intf_phy_88xx(adapter, pcie_gen1_phy_param_8821c, pltfm,
				     HAL_INTF_PHY_PCIE_GEN1);

	if (status != HALMAC_RET_SUCCESS)
		return status;

	status = parse_intf_phy_88xx(adapter, pcie_gen2_phy_param_8821c, pltfm,
				     HAL_INTF_PHY_PCIE_GEN2);

	if (status != HALMAC_RET_SUCCESS)
		return status;

	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	return HALMAC_RET_SUCCESS;
}

/**
 * intf_tun_pcie_8821c() - pcie interface fine tuning
 * @adapter : the adapter of halmac
 * Author : Rick Liu
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
intf_tun_pcie_8821c(struct halmac_adapter *adapter)
{
	return HALMAC_RET_SUCCESS;
}

enum halmac_ret_status
auto_refclk_cal_8821c_pcie(struct halmac_adapter *adapter)
{
	u8 bdr_ori;
	u16 tmp_u16;
	u16 div_set;
	u16 mgn_tmp;
	u16 mgn_set;
	u16 tar;
	enum halmac_ret_status status = HALMAC_RET_SUCCESS;
	u8 l1_flag = 0;

#if (INTF_INTGRA_HOSTREF <= INTF_INTGRA_MINREF || 0 >= INTF_INTGRA_MINREF)
	return status;
#endif
	/* Disable L1BD */
	bdr_ori = dbi_r8_88xx(adapter, PCIE_L1_BACKDOOR);
	if (bdr_ori & (BIT(4) | BIT(3))) {
		status = dbi_w8_88xx(adapter, PCIE_L1_BACKDOOR,
				     bdr_ori & ~(BIT(4) | BIT(3)));
		if (status != HALMAC_RET_SUCCESS)
			return status;
		l1_flag = 1;
	}

	/* Disable function */
	tmp_u16 = mdio_read_88xx(adapter, RAC_CTRL_PPR,
				 HAL_INTF_PHY_PCIE_GEN1);
	if (tmp_u16 & BIT(9)) {
		status = mdio_write_88xx(adapter, RAC_CTRL_PPR,
					 tmp_u16 & ~(BIT(9)),
					 HAL_INTF_PHY_PCIE_GEN1);
		if (status != HALMAC_RET_SUCCESS)
			return status;
	}
	if (adapter->pcie_refautok_en == 0) {
		if (l1_flag == 1)
			status = dbi_w8_88xx(adapter, PCIE_L1_BACKDOOR,
					     bdr_ori);
		return status;
	}

	/* Set div */
	tmp_u16 = mdio_read_88xx(adapter, RAC_CTRL_PPR, HAL_INTF_PHY_PCIE_GEN1);
	status = mdio_write_88xx(adapter, RAC_CTRL_PPR,
				 tmp_u16 & ~(BIT(7) | BIT(6)),
				 HAL_INTF_PHY_PCIE_GEN1);
	if (status != HALMAC_RET_SUCCESS)
		return status;

	/*  Obtain div and margin */
	tar = get_target(adapter);
	if (tar == 0xFFFF)
		return HALMAC_RET_FAIL;
	mgn_tmp = tar * INTF_INTGRA_HOSTREF / INTF_INTGRA_MINREF - tar;

	if (mgn_tmp >= 128) {
		div_set = 0x0003;
		mgn_set = 0x000F;
	} else if (mgn_tmp >= 64) {
		div_set = 0x0003;
		mgn_set = mgn_tmp >> 3;
	} else if (mgn_tmp >= 32) {
		div_set = 0x0002;
		mgn_set = mgn_tmp >> 2;
	} else if (mgn_tmp >= 16) {
		div_set = 0x0001;
		mgn_set = mgn_tmp >> 1;
	} else if (mgn_tmp == 0) {
		div_set = 0x0000;
		mgn_set = 0x0001;
	} else {
		div_set = 0x0000;
		mgn_set = mgn_tmp;
	}

	/* Set div, margin, target*/
	tmp_u16 = mdio_read_88xx(adapter, RAC_CTRL_PPR, HAL_INTF_PHY_PCIE_GEN1);
	tmp_u16 = (tmp_u16 & ~(BIT(7) | BIT(6))) | (div_set << 6);
	status = mdio_write_88xx(adapter, RAC_CTRL_PPR,
				 tmp_u16, HAL_INTF_PHY_PCIE_GEN1);
	if (status != HALMAC_RET_SUCCESS)
		return status;
	tar = get_target(adapter);
	if (tar == 0xFFFF)
		return HALMAC_RET_FAIL;
	PLTFM_MSG_TRACE("[TRACE]target = 0x%X, div = 0x%X, margin = 0x%X\n",
			tar, div_set, mgn_set);
	status = mdio_write_88xx(adapter, RAC_SET_PPR,
				 (tar & 0x0FFF) | (mgn_set << 12),
				 HAL_INTF_PHY_PCIE_GEN1);
	if (status != HALMAC_RET_SUCCESS)
		return status;

	/* Enable function */
	tmp_u16 = mdio_read_88xx(adapter, RAC_CTRL_PPR, HAL_INTF_PHY_PCIE_GEN1);
	status = mdio_write_88xx(adapter, RAC_CTRL_PPR, tmp_u16 | BIT(9),
				 HAL_INTF_PHY_PCIE_GEN1);
	if (status != HALMAC_RET_SUCCESS)
		return status;
	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	/* Set L1BD to ori */
	if (l1_flag == 1)
		status = dbi_w8_88xx(adapter, PCIE_L1_BACKDOOR, bdr_ori);

	return status;
}

static u16
get_target(struct halmac_adapter *adapter)
{
	u16 tmp_u16;
	u16 tar;
	enum halmac_ret_status status = HALMAC_RET_SUCCESS;

	/* Enable counter */
	tmp_u16 = mdio_read_88xx(adapter, RAC_CTRL_PPR, HAL_INTF_PHY_PCIE_GEN1);
	status = mdio_write_88xx(adapter, RAC_CTRL_PPR,
				 tmp_u16 | BIT(11), HAL_INTF_PHY_PCIE_GEN1);
	if (status != HALMAC_RET_SUCCESS)
		return 0xFFFF;

	/* Obtain target */
	status = freerun_delay_us(adapter, 300);
	if (status != HALMAC_RET_SUCCESS)
		return 0xFFFF;
	tar = mdio_read_88xx(adapter, RAC_TRG_PPR, HAL_INTF_PHY_PCIE_GEN1);
	if (tar == 0) {
		PLTFM_MSG_ERR("[ERR]Get target failed.\n");
		return 0xFFFF;
	}

	/* Disable counter */
	tmp_u16 = mdio_read_88xx(adapter, RAC_CTRL_PPR, HAL_INTF_PHY_PCIE_GEN1);
	status = mdio_write_88xx(adapter, RAC_CTRL_PPR,
				 tmp_u16 & ~(BIT(11)), HAL_INTF_PHY_PCIE_GEN1);
	if (status != HALMAC_RET_SUCCESS)
		return 0xFFFF;
	return tar;
}

static enum halmac_ret_status
freerun_delay_us(struct halmac_adapter *adapter, u16 delay)
{
	u16 count;
	u8 mc_ori;
	u32 frcnt_ori;
	u32 frcnt_cmp;
	u8 frcnt_onflg = 0;
	u32 cmp_val;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;
	enum halmac_ret_status status = HALMAC_RET_SUCCESS;

	/* Enable free-run counter */
	mc_ori = HALMAC_REG_R8(REG_MISC_CTRL);
	if ((mc_ori & BIT(3)) == 0) {
		status = HALMAC_REG_W8(REG_MISC_CTRL, mc_ori | BIT(3));
		if (status != HALMAC_RET_SUCCESS)
			return status;
		frcnt_onflg = 1;
	}

	/* counting delay */
	count = 20;
	frcnt_ori = HALMAC_REG_R32(REG_FREERUN_CNT);
	PLTFM_MSG_TRACE("[TRACE]free_ori = 0x%X\n", frcnt_ori);
	do {
		PLTFM_DELAY_US(100);
		count--;
		frcnt_cmp = HALMAC_REG_R32(REG_FREERUN_CNT);
		PLTFM_MSG_TRACE("[TRACE]Count=0x%X, free_cmp=0x%X\n"
				, count, frcnt_cmp);
		if (frcnt_cmp >= frcnt_ori)
			cmp_val = frcnt_cmp - frcnt_ori;
		else
			cmp_val = 0xFFFFFFFF - frcnt_ori + frcnt_cmp;
	} while ((count > 0) && (cmp_val < delay));

	/*  Reset freerun counter */
	if (frcnt_onflg != 1)
		return status;

	status = HALMAC_REG_W8(REG_MISC_CTRL, mc_ori);
	if (status != HALMAC_RET_SUCCESS)
		return status;
	status = HALMAC_REG_W8(REG_DUAL_TSF_RST,
			       HALMAC_REG_R8(REG_DUAL_TSF_RST) | BIT(5));
	return status;
}

#endif /* HALMAC_8821C_SUPPORT */
