#include "halmac_88xx_cfg.h"

/**
 * halmac_init_sdio_cfg_88xx() - init SDIO
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_init_sdio_cfg_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	u8 data[16] = {0};

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_INIT_SDIO_CFG);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_init_sdio_cfg_88xx ==========>\n");

	HALMAC_REG_READ_32(pHalmac_adapter, REG_SDIO_FREE_TXPG);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_SDIO_TX_CTRL, 0x00000000);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_init_sdio_cfg_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_deinit_sdio_cfg_88xx() - deinit SDIO
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_deinit_sdio_cfg_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	VOID *pDriver_adapter = NULL;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_DEINIT_SDIO_CFG);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_deinit_sdio_cfg_88xx ==========>\n");

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_deinit_sdio_cfg_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_cfg_rx_aggregation_88xx_sdio() - config rx aggregation
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_rx_agg_mode
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_cfg_rx_aggregation_88xx_sdio(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_RXAGG_CFG phalmac_rxagg_cfg
)
{
	u8 value8;
	u8 size = 0, timeout = 0, agg_enable = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_CFG_RX_AGGREGATION);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_rx_aggregation_88xx_sdio ==========>\n");

	agg_enable = HALMAC_REG_READ_8(pHalmac_adapter, REG_TXDMA_PQ_MAP);

	switch (phalmac_rxagg_cfg->mode) {
	case HALMAC_RX_AGG_MODE_NONE:
		agg_enable &= ~(BIT_RXDMA_AGG_EN);
		break;
	case HALMAC_RX_AGG_MODE_DMA:
	case HALMAC_RX_AGG_MODE_USB:
		agg_enable |= BIT_RXDMA_AGG_EN;
		break;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_cfg_rx_aggregation_88xx_usb switch case not support\n");
		agg_enable &= ~BIT_RXDMA_AGG_EN;
		break;
	}

	if (_FALSE == phalmac_rxagg_cfg->threshold.drv_define) {
		size = 0xFF;
		timeout = 0x01;
	} else {
		size = phalmac_rxagg_cfg->threshold.size;
		timeout = phalmac_rxagg_cfg->threshold.timeout;
	}

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_TXDMA_PQ_MAP, agg_enable);
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_RXDMA_AGG_PG_TH, (u16)(size | (timeout << BIT_SHIFT_DMA_AGG_TO_V1)));

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_RXDMA_MODE);
	if (0 != (agg_enable & BIT_RXDMA_AGG_EN))
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_RXDMA_MODE, value8 | BIT_DMA_MODE);
	else
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_RXDMA_MODE, value8 & ~(BIT_DMA_MODE));

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_rx_aggregation_88xx_sdio <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_8_sdio_88xx() - read 1byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u8
halmac_reg_read_8_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset
)
{
	u8 value8;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (0 == (halmac_offset & 0xFFFF0000))
		halmac_offset |= WLAN_IOREG_OFFSET;

	status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_reg_read_8_sdio_88xx error = %x\n", status);
		return status;
	}

	value8 = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset);

	return value8;
}

/**
 * halmac_reg_write_8_sdio_88xx() - write 1byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * @halmac_data : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_reg_write_8_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset,
	IN u8 halmac_data
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (0 == (halmac_offset & 0xFFFF0000))
		halmac_offset |= WLAN_IOREG_OFFSET;

	status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_reg_write_8_sdio_88xx error = %x\n", status);
		return status;
	}

	PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset, halmac_data);

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_16_sdio_88xx() - read 2byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u16
halmac_reg_read_16_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
	u32 halmac_offset_old = 0;

	union {
		u16	word;
		u8	byte[2];
	} value16 = { 0x0000 };

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	halmac_offset_old = halmac_offset;

	if (0 == (halmac_offset & 0xFFFF0000))
		halmac_offset |= WLAN_IOREG_OFFSET;

	status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_reg_read_16_sdio_88xx error = %x\n", status);
		return status;
	}

	if ((HALMAC_MAC_POWER_OFF == pHalmac_adapter->halmac_state.mac_power) || (0 != (halmac_offset & (2 - 1))) ||
		(HALMAC_SDIO_CMD53_4BYTE_MODE_RW == pHalmac_adapter->sdio_cmd53_4byte) || (HALMAC_SDIO_CMD53_4BYTE_MODE_R == pHalmac_adapter->sdio_cmd53_4byte)) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_COMMON, HALMAC_DBG_TRACE, "[WARN]use cmd52, offset = %x\n", halmac_offset);
		value16.byte[0] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset);
		value16.byte[1] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 1);
		value16.word = rtk_le16_to_cpu(value16.word);
	} else {
		if (pHalmac_adapter->sdio_hw_info.io_hi_speed_flag != 0) {
			if ((halmac_offset_old & 0xffffef00) == 0x00000000) {
				value16.byte[0] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset);
				value16.byte[1] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 1);
				value16.word = rtk_le16_to_cpu(value16.word);
			} else {
				value16.word = PLATFORM_SDIO_CMD53_READ_16(pDriver_adapter, halmac_offset);
			}
		} else {
			value16.word = PLATFORM_SDIO_CMD53_READ_16(pDriver_adapter, halmac_offset);
		}
	}

	return value16.word;
}

/**
 * halmac_reg_write_16_sdio_88xx() - write 2byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * @halmac_data : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_reg_write_16_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset,
	IN u16 halmac_data
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (0 == (halmac_offset & 0xFFFF0000))
		halmac_offset |= WLAN_IOREG_OFFSET;

	status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_reg_write_16_sdio_88xx error = %x\n", status);
		return status;
	}

	if ((HALMAC_MAC_POWER_OFF == pHalmac_adapter->halmac_state.mac_power) || (0 != (halmac_offset & (2 - 1))) ||
		(HALMAC_SDIO_CMD53_4BYTE_MODE_RW == pHalmac_adapter->sdio_cmd53_4byte) || (HALMAC_SDIO_CMD53_4BYTE_MODE_W == pHalmac_adapter->sdio_cmd53_4byte)) {
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset, (u8)(halmac_data & 0xFF));
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset + 1, (u8)((halmac_data & 0xFF00) >> 8));
	} else {
		PLATFORM_SDIO_CMD53_WRITE_16(pDriver_adapter, halmac_offset, halmac_data);
	}

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_32_sdio_88xx() - read 4byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u32
halmac_reg_read_32_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
    u32 halmac_offset_old = 0;

	union {
		u32	dword;
		u8	byte[4];
	} value32 = { 0x00000000 };

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

    halmac_offset_old = halmac_offset;

	if (0 == (halmac_offset & 0xFFFF0000))
		halmac_offset |= WLAN_IOREG_OFFSET;

	status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);
	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_reg_read_32_sdio_88xx error = %x\n", status);
		return status;
	}

	if (HALMAC_MAC_POWER_OFF == pHalmac_adapter->halmac_state.mac_power || 0 != (halmac_offset & (4 - 1))) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_COMMON, HALMAC_DBG_TRACE, "[WARN]use cmd52, offset = %x\n", halmac_offset);
		value32.byte[0] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset);
		value32.byte[1] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 1);
		value32.byte[2] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 2);
		value32.byte[3] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 3);
		value32.dword = rtk_le32_to_cpu(value32.dword);
	} else {
		if (pHalmac_adapter->sdio_hw_info.io_hi_speed_flag != 0) {
			if ((halmac_offset_old & 0xffffef00) == 0x00000000) {
				value32.byte[0] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset);
				value32.byte[1] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 1);
				value32.byte[2] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 2);
				value32.byte[3] = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset + 3);
				value32.dword = rtk_le32_to_cpu(value32.dword);
			} else {
				value32.dword = PLATFORM_SDIO_CMD53_READ_32(pDriver_adapter, halmac_offset);
			}
		} else {
			value32.dword = PLATFORM_SDIO_CMD53_READ_32(pDriver_adapter, halmac_offset);
		}
	}

	return value32.dword;
}

/**
 * halmac_reg_write_32_sdio_88xx() - write 4byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * @halmac_data : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_reg_write_32_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset,
	IN u32 halmac_data
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (0 == (halmac_offset & 0xFFFF0000))
		halmac_offset |= WLAN_IOREG_OFFSET;

	status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_reg_write_32_sdio_88xx error = %x\n", status);
		return status;
	}

	if (HALMAC_MAC_POWER_OFF == pHalmac_adapter->halmac_state.mac_power || 0 != (halmac_offset & (4 - 1))) {
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset, (u8)(halmac_data & 0xFF));
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset + 1, (u8)((halmac_data & 0xFF00) >> 8));
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset + 2, (u8)((halmac_data & 0xFF0000) >> 16));
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset + 3, (u8)((halmac_data & 0xFF000000) >> 24));
	} else {
		PLATFORM_SDIO_CMD53_WRITE_32(pDriver_adapter, halmac_offset, halmac_data);
	}

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_nbyte_sdio_88xx() - read n byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * @halmac_size : register value size
 * @halmac_data : register value
 * Author : Soar
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u8
halmac_reg_read_nbyte_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset,
	IN u32 halmac_size,
	OUT u8 *halmac_data
)
{
	u8 rtemp = 0xFF;
	u32 counter = 1000;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	union {
		u32	dword;
		u8	byte[4];
	} value32 = { 0x00000000 };

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (0 == (halmac_offset & 0xFFFF0000)) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_offset error = 0x%x\n", halmac_offset);
		return HALMAC_RET_FAIL;
	}

	status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);
	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_reg_read_nbyte_sdio_88xx error = %x\n", status);
		return status;
	}

	if (HALMAC_MAC_POWER_OFF == pHalmac_adapter->halmac_state.mac_power) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_state error = 0x%x\n", pHalmac_adapter->halmac_state.mac_power);
		return HALMAC_RET_FAIL;
	}

	PLATFORM_SDIO_CMD53_READ_N(pDriver_adapter, halmac_offset, halmac_size, halmac_data);

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_get_sdio_tx_addr_sdio_88xx() - get CMD53 addr for the TX packet
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_buf : tx packet, include txdesc
 * @halmac_size : tx packet size
 * @pcmd53_addr : cmd53 addr value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_get_sdio_tx_addr_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *halmac_buf,
	IN u32 halmac_size,
	OUT u32 *pcmd53_addr
)
{
	u32 four_byte_len;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_QUEUE_SELECT queue_sel;
	HALMAC_DMA_MAPPING dma_mapping;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_GET_SDIO_TX_ADDR);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_get_sdio_tx_addr_88xx ==========>\n");

	if (NULL == halmac_buf) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_buf is NULL!!\n");
		return HALMAC_RET_DATA_BUF_NULL;
	}

	if (0 == halmac_size) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_size is 0!!\n");
		return HALMAC_RET_DATA_SIZE_INCORRECT;
	}

	queue_sel = (HALMAC_QUEUE_SELECT)GET_TX_DESC_QSEL(halmac_buf);

	switch (queue_sel) {
	case HALMAC_QUEUE_SELECT_VO:
	case HALMAC_QUEUE_SELECT_VO_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VO];
		break;
	case HALMAC_QUEUE_SELECT_VI:
	case HALMAC_QUEUE_SELECT_VI_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VI];
		break;
	case HALMAC_QUEUE_SELECT_BE:
	case HALMAC_QUEUE_SELECT_BE_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BE];
		break;
	case HALMAC_QUEUE_SELECT_BK:
	case HALMAC_QUEUE_SELECT_BK_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BK];
		break;
	case HALMAC_QUEUE_SELECT_MGNT:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_MG];
		break;
	case HALMAC_QUEUE_SELECT_HIGH:
	case HALMAC_QUEUE_SELECT_BCN:
	case HALMAC_QUEUE_SELECT_CMD:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_HI];
		break;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "Qsel is out of range\n");
		return HALMAC_RET_QSEL_INCORRECT;
	}

	four_byte_len = (halmac_size >> 2) + ((halmac_size & (4 - 1)) ? 1 : 0);

	switch (dma_mapping) {
	case HALMAC_DMA_MAPPING_HIGH:
		*pcmd53_addr = HALMAC_SDIO_CMD_ADDR_TXFF_HIGH;
		break;
	case HALMAC_DMA_MAPPING_NORMAL:
		*pcmd53_addr = HALMAC_SDIO_CMD_ADDR_TXFF_NORMAL;
		break;
	case HALMAC_DMA_MAPPING_LOW:
		*pcmd53_addr = HALMAC_SDIO_CMD_ADDR_TXFF_LOW;
		break;
	case HALMAC_DMA_MAPPING_EXTRA:
		*pcmd53_addr = HALMAC_SDIO_CMD_ADDR_TXFF_EXTRA;
		break;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "DmaMapping is out of range\n");
		return HALMAC_RET_DMA_MAP_INCORRECT;
	}

	*pcmd53_addr = (*pcmd53_addr << 13) | (four_byte_len & HALMAC_SDIO_4BYTE_LEN_MASK);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_get_sdio_tx_addr_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_cfg_tx_agg_align_sdio_88xx() -config sdio bus tx agg alignment
 * @pHalmac_adapter : the adapter of halmac
 * @enable : function enable(1)/disable(0)
 * @align_size : sdio bus tx agg alignment size (2^n, n = 3~11)
 * Author : Soar Tu
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_cfg_tx_agg_align_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 enable,
	IN u16 align_size
)
{
	PHALMAC_API pHalmac_api;
	VOID *pDriver_adapter = NULL;
	u8 i, align_size_ok = 0;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_CFG_TX_AGG_ALIGN);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;


	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_sdio_88xx ==========>\n");

	if ((align_size & 0xF000) != 0) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "Align size is out of range\n");
		return HALMAC_RET_FAIL;
	}

	for (i = 3; i <= 11; i++) {
		if (align_size == 1 << i) {
			align_size_ok = 1;
			break;
		}
	}
	if (align_size_ok == 0) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "Align size is not 2^3 ~ 2^11\n");
		return HALMAC_RET_FAIL;
	}

	/*Keep sdio tx agg alignment size for driver query*/
	pHalmac_adapter->hw_config_info.tx_align_size = align_size;

	if (enable)
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_RQPN_CTRL_2, 0x8000 | align_size);
	else
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_RQPN_CTRL_2, align_size);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_sdio_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_cfg_tx_agg_align_sdio_not_support_88xx(
	IN PHALMAC_ADAPTER	pHalmac_adapter,
	IN u8	enable,
	IN u16	align_size
)
{
	PHALMAC_API pHalmac_api;
	VOID *pDriver_adapter = NULL;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_CFG_TX_AGG_ALIGN);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;


	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_sdio_not_support_88xx ==========>\n");

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_sdio_not_support_88xx not support\n");
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_sdio_not_support_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_tx_allowed_sdio_88xx() - check tx status
 * @pHalmac_adapter : the adapter of halmac
 * @pHalmac_buf : tx packet, include txdesc
 * @halmac_size : tx packet size, include txdesc
 * Author : Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_tx_allowed_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pHalmac_buf,
	IN u32 halmac_size
)
{
	u8 *pCurr_packet;
	u16 *pCurr_free_space;
	u32 i, counter;
	u32 tx_agg_num, packet_size = 0;
	u32 tx_required_page_num, total_required_page_num = 0;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
	VOID *pDriver_adapter = NULL;
	HALMAC_DMA_MAPPING dma_mapping;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_TX_ALLOWED_SDIO);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_tx_allowed_sdio_88xx ==========>\n");

	tx_agg_num = GET_TX_DESC_DMA_TXAGG_NUM(pHalmac_buf);
	pCurr_packet = pHalmac_buf;

	tx_agg_num = (tx_agg_num == 0) ? 1 : tx_agg_num;

	switch ((HALMAC_QUEUE_SELECT)GET_TX_DESC_QSEL(pCurr_packet)) {
	case HALMAC_QUEUE_SELECT_VO:
	case HALMAC_QUEUE_SELECT_VO_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VO];
		break;
	case HALMAC_QUEUE_SELECT_VI:
	case HALMAC_QUEUE_SELECT_VI_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VI];
		break;
	case HALMAC_QUEUE_SELECT_BE:
	case HALMAC_QUEUE_SELECT_BE_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BE];
		break;
	case HALMAC_QUEUE_SELECT_BK:
	case HALMAC_QUEUE_SELECT_BK_V2:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BK];
		break;
	case HALMAC_QUEUE_SELECT_MGNT:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_MG];
		break;
	case HALMAC_QUEUE_SELECT_HIGH:
		dma_mapping = pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_HI];
		break;
	case HALMAC_QUEUE_SELECT_BCN:
	case HALMAC_QUEUE_SELECT_CMD:
		return HALMAC_RET_SUCCESS;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "Qsel is out of range\n");
		return HALMAC_RET_QSEL_INCORRECT;
	}

	switch (dma_mapping) {
	case HALMAC_DMA_MAPPING_HIGH:
		pCurr_free_space = &(pHalmac_adapter->sdio_free_space.high_queue_number);
		break;
	case HALMAC_DMA_MAPPING_NORMAL:
		pCurr_free_space = &(pHalmac_adapter->sdio_free_space.normal_queue_number);
		break;
	case HALMAC_DMA_MAPPING_LOW:
		pCurr_free_space = &(pHalmac_adapter->sdio_free_space.low_queue_number);
		break;
	case HALMAC_DMA_MAPPING_EXTRA:
		pCurr_free_space = &(pHalmac_adapter->sdio_free_space.extra_queue_number);
		break;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "DmaMapping is out of range\n");
		return HALMAC_RET_DMA_MAP_INCORRECT;
	}

	for (i = 0; i < tx_agg_num; i++) {
		packet_size = GET_TX_DESC_TXPKTSIZE(pCurr_packet) + GET_TX_DESC_OFFSET(pCurr_packet) + (GET_TX_DESC_PKT_OFFSET(pCurr_packet) << 3);
		tx_required_page_num = (packet_size >> pHalmac_adapter->hw_config_info.page_size_2_power) + ((packet_size & (pHalmac_adapter->hw_config_info.page_size - 1)) ? 1 : 0);
		total_required_page_num += tx_required_page_num;

		packet_size = HALMAC_ALIGN(packet_size, 8);

		pCurr_packet += packet_size;
	}

	counter = 10;
	do {
		if ((u32)(*pCurr_free_space + pHalmac_adapter->sdio_free_space.public_queue_number) > total_required_page_num) {
			if (*pCurr_free_space >= total_required_page_num) {
				*pCurr_free_space -= (u16)total_required_page_num;
			} else {
				pHalmac_adapter->sdio_free_space.public_queue_number -= (u16)(total_required_page_num - *pCurr_free_space);
				*pCurr_free_space = 0;
			}

			status = halmac_check_oqt_88xx(pHalmac_adapter, tx_agg_num, pHalmac_buf);

			if (HALMAC_RET_SUCCESS != status)
				return status;

			break;
		} else {
			halmac_update_sdio_free_page_88xx(pHalmac_adapter);
		}

		counter--;
		if (0 == counter)
			return HALMAC_RET_FREE_SPACE_NOT_ENOUGH;
	} while (1);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_tx_allowed_sdio_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_indirect_32_sdio_88xx() - read MAC reg by SDIO reg
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * Author : Soar
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u32
halmac_reg_read_indirect_32_sdio_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset
)
{
	u8 rtemp;
	u32 counter = 1000;
	VOID *pDriver_adapter = NULL;

	union {
		u32	dword;
		u8	byte[4];
	} value32 = { 0x00000000 };

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_SDIO_CMD53_WRITE_32(pDriver_adapter, (HALMAC_SDIO_CMD_ADDR_SDIO_REG << 13) | (REG_SDIO_INDIRECT_REG_CFG & HALMAC_SDIO_LOCAL_MSK), halmac_offset | BIT(19) | BIT(17));

	do {
		rtemp = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, (HALMAC_SDIO_CMD_ADDR_SDIO_REG << 13) | ((REG_SDIO_INDIRECT_REG_CFG + 2) & HALMAC_SDIO_LOCAL_MSK));
		counter--;
	} while (((rtemp & BIT(4)) != 0) && (counter > 0));

	value32.dword = PLATFORM_SDIO_CMD53_READ_32(pDriver_adapter, (HALMAC_SDIO_CMD_ADDR_SDIO_REG << 13) | (REG_SDIO_INDIRECT_REG_DATA & HALMAC_SDIO_LOCAL_MSK));

	return value32.dword;
}


/**
 * halmac_sdio_cmd53_4byte_88xx() - cmd53 only for 4byte len register IO
 * @pHalmac_adapter : the adapter of halmac
 * @enable : 1->CMD53 only use in 4byte reg, 0 : No limitation
 * Author : Ivan Lin/KaiYuan Chang
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_sdio_cmd53_4byte_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_SDIO_CMD53_4BYTE_MODE cmd53_4byte_mode
)
{
	if (halmac_adapter_validate(pHalmac_adapter) != HALMAC_RET_SUCCESS)
		return HALMAC_RET_ADAPTER_INVALID;

	if (halmac_api_validate(pHalmac_adapter) != HALMAC_RET_SUCCESS)
		return HALMAC_RET_API_INVALID;

	if (pHalmac_adapter->halmac_interface != HALMAC_INTERFACE_SDIO)
		return HALMAC_RET_WRONG_INTF;

	pHalmac_adapter->sdio_cmd53_4byte = cmd53_4byte_mode;

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_sdio_hw_info_88xx() - info sdio hw info
 * @pHalmac_adapter : the adapter of halmac
 * @HALMAC_SDIO_CMD53_4BYTE_MODE :
 * clock_speed : sdio bus clock. Unit -> MHz
 * spec_ver : sdio spec version
 * Author : Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_sdio_hw_info_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_SDIO_HW_INFO pSdio_hw_info
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "[TRACE]halmac_sdio_hw_info_88xx ==========>\n");

	if (halmac_adapter_validate(pHalmac_adapter) != HALMAC_RET_SUCCESS)
		return HALMAC_RET_ADAPTER_INVALID;

	if (halmac_api_validate(pHalmac_adapter) != HALMAC_RET_SUCCESS)
		return HALMAC_RET_API_INVALID;

	if (pHalmac_adapter->halmac_interface != HALMAC_INTERFACE_SDIO)
		return HALMAC_RET_WRONG_INTF;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "[TRACE]SDIO hw clock : %d, spec : %d\n", pSdio_hw_info->clock_speed, pSdio_hw_info->spec_ver);

	if (pSdio_hw_info->clock_speed > HALMAC_SDIO_CLOCK_SPEED_MAX_88XX)
		return HALMAC_RET_SDIO_CLOCK_ERR;

	if (pSdio_hw_info->clock_speed > HALMAC_SDIO_CLK_THRESHOLD_88XX)
		pHalmac_adapter->sdio_hw_info.io_hi_speed_flag = 1;

	pHalmac_adapter->sdio_hw_info.clock_speed = pSdio_hw_info->clock_speed;
	pHalmac_adapter->sdio_hw_info.spec_ver = pSdio_hw_info->spec_ver;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "[TRACE]halmac_sdio_hw_info_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}
