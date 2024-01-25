/******************************************************************************
 *
 * Copyright(c) 2016 - 2019 Realtek Corporation. All rights reserved.
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

#include "halmac_usb_88xx.h"

#if (HALMAC_88XX_SUPPORT && HALMAC_USB_SUPPORT)

enum usb_burst_size {
	USB_BURST_SIZE_3_0 = 0x0,
	USB_BURST_SIZE_2_0_HS = 0x1,
	USB_BURST_SIZE_2_0_FS = 0x2,
	USB_BURST_SIZE_2_0_OTHERS = 0x3,
	USB_BURST_SIZE_UNDEFINE = 0x7F,
};

#define USB_PHY_PAGE0			0x9B
#define USB_PHY_PAGE1			0xBB

/**
 * init_usb_cfg_88xx() - init USB
 * @adapter : the adapter of halmac
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
init_usb_cfg_88xx(struct halmac_adapter *adapter)
{
	u8 value8 = 0;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	PLTFM_MSG_TRACE("[TRACE]%s ===>\n", __func__);

	value8 |= (BIT_DMA_MODE | (0x3 << BIT_SHIFT_BURST_CNT));

	if (HALMAC_REG_R8(REG_SYS_CFG2 + 3) == 0x20) {
		 /* usb3.0 */
		value8 |= (USB_BURST_SIZE_3_0 << BIT_SHIFT_BURST_SIZE);
	} else {
		if ((HALMAC_REG_R8(REG_USB_USBSTAT) & 0x3) == 0x1)/* usb2.0 */
			value8 |= USB_BURST_SIZE_2_0_HS << BIT_SHIFT_BURST_SIZE;
		else /* usb1.1 */
			value8 |= USB_BURST_SIZE_2_0_FS << BIT_SHIFT_BURST_SIZE;
	}

	HALMAC_REG_W8(REG_RXDMA_MODE, value8);
	HALMAC_REG_W16_SET(REG_TXDMA_OFFSET_CHK, BIT_DROP_DATA_EN);

	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	return HALMAC_RET_SUCCESS;
}

/**
 * deinit_usb_cfg_88xx() - deinit USB
 * @adapter : the adapter of halmac
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
deinit_usb_cfg_88xx(struct halmac_adapter *adapter)
{
	return HALMAC_RET_SUCCESS;
}

/**
 * cfg_usb_rx_agg_88xx() - config rx aggregation
 * @adapter : the adapter of halmac
 * @halmac_rx_agg_mode
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
cfg_usb_rx_agg_88xx(struct halmac_adapter *adapter,
		    struct halmac_rxagg_cfg *cfg)
{
	u8 dma_usb_agg;
	u8 size;
	u8 timeout;
	u8 agg_enable;
	u32 value32;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	PLTFM_MSG_TRACE("[TRACE]%s ===>\n", __func__);

	dma_usb_agg = HALMAC_REG_R8(REG_RXDMA_AGG_PG_TH + 3);
	agg_enable = HALMAC_REG_R8(REG_TXDMA_PQ_MAP);

	switch (cfg->mode) {
	case HALMAC_RX_AGG_MODE_NONE:
		agg_enable &= ~BIT_RXDMA_AGG_EN;
		break;
	case HALMAC_RX_AGG_MODE_DMA:
		agg_enable |= BIT_RXDMA_AGG_EN;
		dma_usb_agg |= BIT(7);
		break;

	case HALMAC_RX_AGG_MODE_USB:
		agg_enable |= BIT_RXDMA_AGG_EN;
		dma_usb_agg &= ~BIT(7);
		break;
	default:
		PLTFM_MSG_ERR("[ERR]unsupported mode\n");
		agg_enable &= ~BIT_RXDMA_AGG_EN;
		break;
	}

	if (cfg->threshold.drv_define == 0) {
		if (HALMAC_REG_R8(REG_SYS_CFG2 + 3) == 0x20) {
			/* usb3.0 */
			size = 0x5;
			timeout = 0xA;
		} else {
			/* usb2.0 */
			size = 0x5;
			timeout = 0x20;
		}
	} else {
		size = cfg->threshold.size;
		timeout = cfg->threshold.timeout;
	}

	value32 = HALMAC_REG_R32(REG_RXDMA_AGG_PG_TH);
	if (cfg->threshold.size_limit_en == 0)
		HALMAC_REG_W32(REG_RXDMA_AGG_PG_TH, value32 & ~BIT_EN_PRE_CALC);
	else
		HALMAC_REG_W32(REG_RXDMA_AGG_PG_TH, value32 | BIT_EN_PRE_CALC);

	HALMAC_REG_W8(REG_TXDMA_PQ_MAP, agg_enable);
	HALMAC_REG_W8(REG_RXDMA_AGG_PG_TH + 3, dma_usb_agg);
	HALMAC_REG_W16(REG_RXDMA_AGG_PG_TH,
		       (u16)(size | (timeout << BIT_SHIFT_DMA_AGG_TO_V1)));

	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	return HALMAC_RET_SUCCESS;
}

/**
 * reg_r8_usb_88xx() - read 1byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u8
reg_r8_usb_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return PLTFM_REG_R8(offset);
}

/**
 * reg_w8_usb_88xx() - write 1byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @value : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
reg_w8_usb_88xx(struct halmac_adapter *adapter, u32 offset, u8 value)
{
	PLTFM_REG_W8(offset, value);

	return HALMAC_RET_SUCCESS;
}

/**
 * reg_r16_usb_88xx() - read 2byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u16
reg_r16_usb_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return PLTFM_REG_R16(offset);
}

/**
 * reg_w16_usb_88xx() - write 2byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @value : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
reg_w16_usb_88xx(struct halmac_adapter *adapter, u32 offset, u16 value)
{
	PLTFM_REG_W16(offset, value);

	return HALMAC_RET_SUCCESS;
}

/**
 * reg_r32_usb_88xx() - read 4byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u32
reg_r32_usb_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return PLTFM_REG_R32(offset);
}

/**
 * reg_w32_usb_88xx() - write 4byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @value : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
reg_w32_usb_88xx(struct halmac_adapter *adapter, u32 offset, u32 value)
{
	PLTFM_REG_W32(offset, value);

	return HALMAC_RET_SUCCESS;
}

/**
 * set_usb_bulkout_num_88xx() - inform bulk-out num
 * @adapter : the adapter of halmac
 * @bulkout_num : usb bulk-out number
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
set_usb_bulkout_num_88xx(struct halmac_adapter *adapter, u8 num)
{
	adapter->bulkout_num = num;

	return HALMAC_RET_SUCCESS;
}

/**
 * get_usb_bulkout_id_88xx() - get bulk out id for the TX packet
 * @adapter : the adapter of halmac
 * @buf : tx packet, include txdesc
 * @size : tx packet size
 * @id : usb bulk-out id
 * Author : KaiYuan Chang
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
get_usb_bulkout_id_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
			u8 *id)
{
	enum halmac_qsel queue_sel;
	enum halmac_dma_mapping dma_mapping;

	PLTFM_MSG_TRACE("[TRACE]%s ===>\n", __func__);

	if (!buf) {
		PLTFM_MSG_ERR("[ERR]buf is NULL!!\n");
		return HALMAC_RET_DATA_BUF_NULL;
	}

	if (size == 0) {
		PLTFM_MSG_ERR("[ERR]size is 0!!\n");
		return HALMAC_RET_DATA_SIZE_INCORRECT;
	}

	queue_sel = (enum halmac_qsel)GET_TX_DESC_QSEL(buf);

	switch (queue_sel) {
	case HALMAC_QSEL_VO:
	case HALMAC_QSEL_VO_V2:
		dma_mapping = adapter->pq_map[HALMAC_PQ_MAP_VO];
		break;
	case HALMAC_QSEL_VI:
	case HALMAC_QSEL_VI_V2:
		dma_mapping = adapter->pq_map[HALMAC_PQ_MAP_VI];
		break;
	case HALMAC_QSEL_BE:
	case HALMAC_QSEL_BE_V2:
		dma_mapping = adapter->pq_map[HALMAC_PQ_MAP_BE];
		break;
	case HALMAC_QSEL_BK:
	case HALMAC_QSEL_BK_V2:
		dma_mapping = adapter->pq_map[HALMAC_PQ_MAP_BK];
		break;
	case HALMAC_QSEL_MGNT:
		dma_mapping = adapter->pq_map[HALMAC_PQ_MAP_MG];
		break;
	case HALMAC_QSEL_HIGH:
	case HALMAC_QSEL_BCN:
	case HALMAC_QSEL_CMD:
		dma_mapping = HALMAC_DMA_MAPPING_HIGH;
		break;
	default:
		PLTFM_MSG_ERR("[ERR]Qsel is out of range\n");
		return HALMAC_RET_QSEL_INCORRECT;
	}

	switch (dma_mapping) {
	case HALMAC_DMA_MAPPING_HIGH:
		*id = 0;
		break;
	case HALMAC_DMA_MAPPING_NORMAL:
		*id = 1;
		break;
	case HALMAC_DMA_MAPPING_LOW:
		*id = 2;
		break;
	case HALMAC_DMA_MAPPING_EXTRA:
		*id = 3;
		break;
	default:
		PLTFM_MSG_ERR("[ERR]out of range\n");
		return HALMAC_RET_DMA_MAP_INCORRECT;
	}

	PLTFM_MSG_TRACE("[TRACE]%s <===\n", __func__);

	return HALMAC_RET_SUCCESS;
}

/**
 * cfg_txagg_usb_align_88xx() -config sdio bus tx agg alignment
 * @adapter : the adapter of halmac
 * @enable : function enable(1)/disable(0)
 * @align_size : sdio bus tx agg alignment size (2^n, n = 3~11)
 * Author : Soar Tu
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
cfg_txagg_usb_align_88xx(struct halmac_adapter *adapter, u8 enable,
			 u16 align_size)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * tx_allowed_usb_88xx() - check tx status
 * @adapter : the adapter of halmac
 * @buf : tx packet, include txdesc
 * @size : tx packet size, include txdesc
 * Author : Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
tx_allowed_usb_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * usb_indirect_reg_r32_88xx() - read MAC reg by SDIO reg
 * @adapter : the adapter of halmac
 * @offset : register offset
 * Author : Soar
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
u32
usb_indirect_reg_r32_88xx(struct halmac_adapter *adapter, u32 offset)
{
	return 0xFFFFFFFF;
}

/**
 * usb_reg_rn_88xx() - read n byte register
 * @adapter : the adapter of halmac
 * @offset : register offset
 * @size : register value size
 * @value : register value
 * Author : Soar
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
usb_reg_rn_88xx(struct halmac_adapter *adapter, u32 offset, u32 size,
		u8 *value)
{
	return HALMAC_RET_NOT_SUPPORT;
}

/**
 * get_usb_tx_addr_88xx() - get CMD53 addr for the TX packet
 * @adapter : the adapter of halmac
 * @buf : tx packet, include txdesc
 * @size : tx packet size
 * @pcmd53_addr : cmd53 addr value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : enum halmac_ret_status
 * More details of status code can be found in prototype document
 */
enum halmac_ret_status
get_usb_tx_addr_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
		     u32 *cmd53_addr)
{
	return HALMAC_RET_NOT_SUPPORT;
}

enum halmac_ret_status
set_usb_mode_88xx(struct halmac_adapter *adapter, enum halmac_usb_mode mode)
{
	u32 usb_tmp;
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;
	enum halmac_usb_mode cur_mode;

	cur_mode = (HALMAC_REG_R8(REG_SYS_CFG2 + 3) == 0x20) ?
					HALMAC_USB_MODE_U3 : HALMAC_USB_MODE_U2;

	/* check if HW supports usb2_usb3 switch */
	usb_tmp = HALMAC_REG_R32(REG_PAD_CTRL2);
	if (0 == (BIT_GET_USB23_SW_MODE_V1(usb_tmp) |
	    (usb_tmp & BIT_USB3_USB2_TRANSITION))) {
		PLTFM_MSG_ERR("[ERR]u2/u3 switch\n");
		return HALMAC_RET_USB2_3_SWITCH_UNSUPPORT;
	}

	if (mode == cur_mode) {
		PLTFM_MSG_ERR("[ERR]u2/u3 unchange\n");
		return HALMAC_RET_USB_MODE_UNCHANGE;
	}

	/* Enable IO wrapper timeout */
	if (adapter->chip_id == HALMAC_CHIP_ID_8822B ||
	    adapter->chip_id == HALMAC_CHIP_ID_8821C)
		HALMAC_REG_W8_CLR(REG_SW_MDIO + 3, BIT(0));

	usb_tmp &= ~(BIT_USB23_SW_MODE_V1(0x3));

	if (mode == HALMAC_USB_MODE_U2)
		HALMAC_REG_W32(REG_PAD_CTRL2,
			       usb_tmp |
			       BIT_USB23_SW_MODE_V1(HALMAC_USB_MODE_U2) |
			       BIT_RSM_EN_V1);
	else
		HALMAC_REG_W32(REG_PAD_CTRL2,
			       usb_tmp |
			       BIT_USB23_SW_MODE_V1(HALMAC_USB_MODE_U3) |
			       BIT_RSM_EN_V1);

	HALMAC_REG_W8(REG_PAD_CTRL2 + 1, 4);
	HALMAC_REG_W16_SET(REG_SYS_PW_CTRL, BIT_APFM_OFFMAC);
	PLTFM_DELAY_US(1000);
	HALMAC_REG_W32_SET(REG_PAD_CTRL2, BIT_NO_PDN_CHIPOFF_V1);

	return HALMAC_RET_SUCCESS;
}

enum halmac_ret_status
usbphy_write_88xx(struct halmac_adapter *adapter, u8 addr, u16 data, u8 speed)
{
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;

	if (speed == HAL_INTF_PHY_USB3) {
		HALMAC_REG_W8(0xff0d, (u8)data);
		HALMAC_REG_W8(0xff0e, (u8)(data >> 8));
		HALMAC_REG_W8(0xff0c, addr | BIT(7));
	} else if (speed == HAL_INTF_PHY_USB2) {
		HALMAC_REG_W8(0xfe41, (u8)data);
		HALMAC_REG_W8(0xfe40, addr);
		HALMAC_REG_W8(0xfe42, 0x81);
	} else {
		PLTFM_MSG_ERR("[ERR]Error USB Speed !\n");
		return HALMAC_RET_NOT_SUPPORT;
	}

	return HALMAC_RET_SUCCESS;
}

u16
usbphy_read_88xx(struct halmac_adapter *adapter, u8 addr, u8 speed)
{
	struct halmac_api *api = (struct halmac_api *)adapter->halmac_api;
	u16 value = 0;

	if (speed == HAL_INTF_PHY_USB3) {
		HALMAC_REG_W8(0xff0c, addr | BIT(6));
		value = (u16)(HALMAC_REG_R32(0xff0c) >> 8);
	} else if (speed == HAL_INTF_PHY_USB2) {
		if (addr >= 0xE0)
			addr -= 0x20;
		if (addr >= 0xC0 && addr <= 0xDF) {
			HALMAC_REG_W8(0xfe40, addr);
			HALMAC_REG_W8(0xfe42, 0x81);
			value = HALMAC_REG_R8(0xfe43);
		} else {
			PLTFM_MSG_ERR("[ERR]phy offset\n");
			return HALMAC_RET_NOT_SUPPORT;
		}
	} else {
		PLTFM_MSG_ERR("[ERR]usb speed !\n");
		return HALMAC_RET_NOT_SUPPORT;
	}

	return value;
}

enum halmac_ret_status
en_ref_autok_usb_88xx(struct halmac_adapter *adapter, u8 en)
{
	return HALMAC_RET_NOT_SUPPORT;
}
enum halmac_ret_status
usb_page_switch_88xx(struct halmac_adapter *adapter, u8 speed, u8 page)
{
	if (speed == HAL_INTF_PHY_USB3)
		return HALMAC_RET_SUCCESS;
	if (page == 0)
		usbphy_write_88xx(adapter, USB_REG_PAGE, USB_PHY_PAGE0, speed);
	else
		usbphy_write_88xx(adapter, USB_REG_PAGE, USB_PHY_PAGE1, speed);

	return HALMAC_RET_SUCCESS;
}
#endif /* HALMAC_88XX_SUPPORT */
