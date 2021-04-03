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

#include "halmac_pcie_88xx.h"

#if HALMAC_88XX_SUPPORT

/**
 * init_pcie_cfg_88xx() -  init PCIe
 * @adapter : the adapter of halmac
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
init_pcie_cfg_88xx(struct halmac_adapter *adapter)
{
	return HALMAC_RET_SUCCESS;
}

/**
 * deinit_pcie_cfg_88xx() - deinit PCIE
 * @adapter : the adapter of halmac
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
deinit_pcie_cfg_88xx(struct halmac_adapter *adapter)
{
	return HALMAC_RET_SUCCESS;
}

/**
 * cfg_pcie_rx_agg_88xx() - config rx aggregation
 * @adapter : the adapter of halmac
 * @halmac_rx_agg_mode
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
cfg_pcie_rx_agg_88xx(struct halmac_adapter *adapter,
		     struct halmac_rxagg_cfg *cfg)
{
	return HALMAC_RET_SUCCESS;
}

/**
 * reg_r8_pcie_88xx() - read 1byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u8
reg_r8_pcie_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return PLTFM_REG_R8(offset);
}

/**
 * reg_w8_pcie_88xx() - write 1byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @value : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
reg_w8_pcie_88xx(struct halmac_adapter *adapter, u32 offset, u8 value)
{
	PLTFM_REG_W8(offset, value);

	return HALMAC_RET_SUCCESS;
}

/**
 * reg_r16_pcie_88xx() - read 2byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u16
reg_r16_pcie_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return PLTFM_REG_R16(offset);
}

/**
 * reg_w16_pcie_88xx() - write 2byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @value : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
reg_w16_pcie_88xx(struct halmac_adapter *adapter, u32 offset, u16 value)
{
	PLTFM_REG_W16(offset, value);

	return HALMAC_RET_SUCCESS;
}

/**
 * reg_r32_pcie_88xx() - read 4byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u32
reg_r32_pcie_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return PLTFM_REG_R32(offset);
}

/**
 * reg_w32_pcie_88xx() - write 4byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @value : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
reg_w32_pcie_88xx(struct halmac_adapter *adapter, u32 offset, u32 value)
{
	PLTFM_REG_W32(offset, value);

	return HALMAC_RET_SUCCESS;
}

/**
 * cfg_txagg_pcie_align_88xx() -config sdio bus tx agg alignment
 * @adapter : the adapter of halmac
 * @enable : function enable(1)/disable(0)
 * @align_size : sdio bus tx agg alignment size (2^n, n = 3~11)
 * Author : Soar Tu
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
cfg_txagg_pcie_align_88xx(struct halmac_adapter *adapter, u8 enable,
			  u16 align_size)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * tx_allowed_pcie_88xx() - check tx status
 * @adapter : the adapter of halmac
 * @buf : tx packet, include txdesc
 * @size : tx packet size, include txdesc
 * Author : Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
tx_allowed_pcie_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * pcie_indirect_reg_r32_88xx() - read MAC reg by SDIO reg
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : Soar
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u32
pcie_indirect_reg_r32_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return 0xFFFFFFFF;
}

/**
 * pcie_reg_rn_88xx() - read n byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @size : register value size
 * @value : register value
 * Author : Soar
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
pcie_reg_rn_88xx(struct halmac_adapter *adapter, u32 offset, u32 size,
		 u8 *value)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * set_pcie_bulkout_num_88xx() - inform bulk-out num
 * @adapter : the adapter of halmac
 * @num : usb bulk-out number
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
set_pcie_bulkout_num_88xx(struct halmac_adapter *adapter, u8 num)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * get_pcie_tx_addr_88xx() - get CMD53 addr for the TX packet
 * @adapter : the adapter of halmac
 * @buf : tx packet, include txdesc
 * @size : tx packet size
 * @cmd53_addr : cmd53 addr value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
get_pcie_tx_addr_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
		      u32 *cmd53_addr)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * get_pcie_bulkout_id_88xx() - get bulk out id for the TX packet
 * @adapter : the adapter of halmac
 * @buf : tx packet, include txdesc
 * @size : tx packet size
 * @id : usb bulk-out id
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
get_pcie_bulkout_id_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
			 u8 *id)
{
	return HALMAC_RET_NOT_SUPPORT;
}

enum halmac_ret_status
mdio_write_88xx(struct halmac_adapter *adapter, u8 addr, u16 data, u8 speed)
{
	u8 tmp_u1b = 0;
	u32 cnt = 0;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;
	u8 real_addr = 0;

	HALMAC_REG_W16(REG_MDIO_V1, data);

	real_addr = (addr & 0x1F);
	HALMAC_REG_W8(REG_PCIE_MIX_CFG, real_addr);

	if (speed == HAL_INTF_PHY_PCIE_GEN1) {
		if (addr < 0x20)
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x00);
		else
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x01);
	} else if (speed == HAL_INTF_PHY_PCIE_GEN2) {
		if (addr < 0x20)
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x02);
		else
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x03);
	} else {
		PLTFM_MSG_ERR("[ERR]Error Speed !\n");
	}

	HALMAC_REG_W8_SET(REG_PCIE_MIX_CFG, BIT_MDIO_WFLAG_V1);

	tmp_u1b = HALMAC_REG_R8(REG_PCIE_MIX_CFG) & BIT_MDIO_WFLAG_V1;
	cnt = 20;

	while (tmp_u1b && (cnt != 0)) {
		PLTFM_DELAY_US(10);
		tmp_u1b = HALMAC_REG_R8(REG_PCIE_MIX_CFG) & BIT_MDIO_WFLAG_V1;
		cnt--;
	}

	if (tmp_u1b) {
		PLTFM_MSG_ERR("[ERR]MDIO write fail!\n");
		return HALMAC_RET_FAIL;
	}

	return HALMAC_RET_SUCCESS;
}

u16
mdio_read_88xx(struct halmac_adapter *adapter, u8 addr, u8 speed)
{
	u16 ret = 0;
	u8 tmp_u1b = 0;
	u32 cnt = 0;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;
	u8 real_addr = 0;

	real_addr = (addr & 0x1F);
	HALMAC_REG_W8(REG_PCIE_MIX_CFG, real_addr);

	if (speed == HAL_INTF_PHY_PCIE_GEN1) {
		if (addr < 0x20)
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x00);
		else
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x01);
	} else if (speed == HAL_INTF_PHY_PCIE_GEN2) {
		if (addr < 0x20)
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x02);
		else
			HALMAC_REG_W8(REG_PCIE_MIX_CFG + 3, 0x03);
	} else {
		PLTFM_MSG_ERR("[ERR]Error Speed !\n");
	}

	HALMAC_REG_W8_SET(REG_PCIE_MIX_CFG, BIT_MDIO_RFLAG_V1);

	tmp_u1b = HALMAC_REG_R8(REG_PCIE_MIX_CFG) & BIT_MDIO_RFLAG_V1;
	cnt = 20;
	while (tmp_u1b && (cnt != 0)) {
		PLTFM_DELAY_US(10);
		tmp_u1b = HALMAC_REG_R8(REG_PCIE_MIX_CFG) & BIT_MDIO_RFLAG_V1;
		cnt--;
	}

	if (tmp_u1b) {
		ret  = 0xFFFF;
		PLTFM_MSG_ERR("[ERR]MDIO read fail!\n");
	} else {
		ret = HALMAC_REG_R16(REG_MDIO_V1 + 2);
		PLTFM_MSG_TRACE("[TRACE]Value-R = %x\n", ret);
	}

	return ret;
}

enum halmac_ret_status
dbi_w32_88xx(struct halmac_adapter *adapter, u16 addr, u32 data)
{
	u8 tmp_u1b = 0;
	u32 cnt = 0;
	u16 write_addr = 0;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	HALMAC_REG_W32(REG_DBI_WDATA_V1, data);

	write_addr = ((addr & 0x0ffc) | (0x000F << 12));
	HALMAC_REG_W16(REG_DBI_FLAG_V1, write_addr);

	PLTFM_MSG_TRACE("[TRACE]Addr-W = %x\n", write_addr);

	HALMAC_REG_W8(REG_DBI_FLAG_V1 + 2, 0x01);
	tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);

	cnt = 20;
	while (tmp_u1b && (cnt != 0)) {
		PLTFM_DELAY_US(10);
		tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);
		cnt--;
	}

	if (tmp_u1b) {
		PLTFM_MSG_ERR("[ERR]DBI write fail!\n");
		return HALMAC_RET_FAIL;
	}

	return HALMAC_RET_SUCCESS;
}

u32
dbi_r32_88xx(struct halmac_adapter *adapter, u16 addr)
{
	u16 read_addr = addr & 0x0ffc;
	u8 tmp_u1b = 0;
	u32 cnt = 0;
	u32 ret = 0;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	HALMAC_REG_W16(REG_DBI_FLAG_V1, read_addr);

	HALMAC_REG_W8(REG_DBI_FLAG_V1 + 2, 0x2);
	tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);

	cnt = 20;
	while (tmp_u1b && (cnt != 0)) {
		PLTFM_DELAY_US(10);
		tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);
		cnt--;
	}

	if (tmp_u1b) {
		ret  = 0xFFFF;
		PLTFM_MSG_ERR("[ERR]DBI read fail!\n");
	} else {
		ret = HALMAC_REG_R32(REG_DBI_RDATA_V1);
		PLTFM_MSG_TRACE("[TRACE]Value-R = %x\n", ret);
	}

	return ret;
}

enum halmac_ret_status
dbi_w8_88xx(struct halmac_adapter *adapter, u16 addr, u8 data)
{
	u8 tmp_u1b = 0;
	u32 cnt = 0;
	u16 write_addr = 0;
	u16 remainder = addr & (4 - 1);
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	HALMAC_REG_W8(REG_DBI_WDATA_V1 + remainder, data);

	write_addr = ((addr & 0x0ffc) | (BIT(0) << (remainder + 12)));

	HALMAC_REG_W16(REG_DBI_FLAG_V1, write_addr);

	PLTFM_MSG_TRACE("[TRACE]Addr-W = %x\n", write_addr);

	HALMAC_REG_W8(REG_DBI_FLAG_V1 + 2, 0x01);

	tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);

	cnt = 20;
	while (tmp_u1b && (cnt != 0)) {
		PLTFM_DELAY_US(10);
		tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);
		cnt--;
	}

	if (tmp_u1b) {
		PLTFM_MSG_ERR("[ERR]DBI write fail!\n");
		return HALMAC_RET_FAIL;
	}

	return HALMAC_RET_SUCCESS;
}

u8
dbi_r8_88xx(struct halmac_adapter *adapter, u16 addr)
{
	u16 read_addr = addr & 0x0ffc;
	u8 tmp_u1b = 0;
	u32 cnt = 0;
	u8 ret = 0;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	HALMAC_REG_W16(REG_DBI_FLAG_V1, read_addr);
	HALMAC_REG_W8(REG_DBI_FLAG_V1 + 2, 0x2);

	tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);

	cnt = 20;
	while (tmp_u1b && (cnt != 0)) {
		PLTFM_DELAY_US(10);
		tmp_u1b = HALMAC_REG_R8(REG_DBI_FLAG_V1 + 2);
		cnt--;
	}

	if (tmp_u1b) {
		ret  = 0xFF;
		PLTFM_MSG_ERR("[ERR]DBI read fail!\n");
	} else {
		ret = HALMAC_REG_R8(REG_DBI_RDATA_V1 + (addr & (4 - 1)));
		PLTFM_MSG_TRACE("[TRACE]Value-R = %x\n", ret);
	}

	return ret;
}

enum halmac_ret_status
trxdma_check_idle_88xx(struct halmac_adapter *adapter)
{
	u32 cnt = 0;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	/* Stop Tx & Rx DMA */
	HALMAC_REG_W32_SET(REG_RXPKT_NUM, BIT(18));
	HALMAC_REG_W16_SET(REG_PCIE_CTRL, ~(BIT(15) | BIT(8)));

	/* Stop FW */
	HALMAC_REG_W16_CLR(REG_SYS_FUNC_EN, BIT(10));

	/* Check Tx DMA is idle */
	cnt = 20;
	while ((HALMAC_REG_R8(REG_SYS_CFG5) & BIT(2)) == BIT(2)) {
		PLTFM_DELAY_US(10);
		cnt--;
		if (cnt == 0) {
			PLTFM_MSG_ERR("[ERR]Chk tx idle\n");
			return HALMAC_RET_POWER_OFF_FAIL;
		}
	}

	/* Check Rx DMA is idle */
	cnt = 20;
	while ((HALMAC_REG_R32(REG_RXPKT_NUM) & BIT(17)) != BIT(17)) {
		PLTFM_DELAY_US(10);
		cnt--;
		if (cnt == 0) {
			PLTFM_MSG_ERR("[ERR]Chk rx idle\n");
			return HALMAC_RET_POWER_OFF_FAIL;
		}
	}

	return HALMAC_RET_SUCCESS;
}

void
en_ref_autok_88xx(struct halmac_adapter *adapter, u8 en)
{
	if (en == 1)
		adapter->pcie_refautok_en = 1;
	else
		adapter->pcie_refautok_en = 0;
}

#endif /* HALMAC_88XX_SUPPORT */
