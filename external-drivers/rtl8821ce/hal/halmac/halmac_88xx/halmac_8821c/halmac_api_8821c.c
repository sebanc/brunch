#include "halmac_8821c_cfg.h"
#include "halmac_func_8821c.h"
#include "../halmac_func_88xx.h"


/**
 * halmac_mount_api_8821c() - attach functions to function pointer
 * @pHalmac_adapter
 *
 * SD1 internal use
 *
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 */
HALMAC_RET_STATUS
halmac_mount_api_8821c(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	PHALMAC_API pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	pHalmac_adapter->chip_id = HALMAC_CHIP_ID_8821C;
	pHalmac_adapter->hw_config_info.efuse_size = HALMAC_EFUSE_SIZE_8821C;
	pHalmac_adapter->hw_config_info.eeprom_size = HALMAC_EEPROM_SIZE_8821C;
	pHalmac_adapter->hw_config_info.bt_efuse_size = HALMAC_BT_EFUSE_SIZE_8821C;
	pHalmac_adapter->hw_config_info.cam_entry_num = HALMAC_SECURITY_CAM_ENTRY_NUM_8821C;
	pHalmac_adapter->hw_config_info.txdesc_size = HALMAC_TX_DESC_SIZE_8821C;
	pHalmac_adapter->hw_config_info.rxdesc_size = HALMAC_RX_DESC_SIZE_8821C;
	pHalmac_adapter->hw_config_info.tx_fifo_size = HALMAC_TX_FIFO_SIZE_8821C;
	pHalmac_adapter->hw_config_info.rx_fifo_size = HALMAC_RX_FIFO_SIZE_8821C;
	pHalmac_adapter->hw_config_info.page_size = HALMAC_TX_PAGE_SIZE_8821C;
	pHalmac_adapter->hw_config_info.tx_align_size = HALMAC_TX_ALIGN_SIZE_8821C;
	pHalmac_adapter->hw_config_info.page_size_2_power = HALMAC_TX_PAGE_SIZE_2_POWER_8821C;

	pHalmac_adapter->txff_allocation.rsvd_drv_pg_num = HALMAC_RSVD_DRV_PGNUM_8821C;

#if HALMAC_8821C_SUPPORT
	pHalmac_api->halmac_init_trx_cfg = halmac_init_trx_cfg_8821C;
		pHalmac_api->halmac_init_protocol_cfg = halmac_init_protocol_cfg_8821c;
	pHalmac_api->halmac_init_h2c = halmac_init_h2c_8821c;

	if (HALMAC_INTERFACE_SDIO == pHalmac_adapter->halmac_interface) {
		pHalmac_api->halmac_tx_allowed_sdio = halmac_tx_allowed_sdio_88xx;
		pHalmac_api->halmac_cfg_tx_agg_align = halmac_cfg_tx_agg_align_sdio_88xx;
		pHalmac_api->halmac_mac_power_switch = halmac_mac_power_switch_8821c_sdio;
		pHalmac_api->halmac_phy_cfg = halmac_phy_cfg_8821c_sdio;
		pHalmac_api->halmac_interface_integration_tuning = halmac_interface_integration_tuning_8821c_sdio;
	} else if (HALMAC_INTERFACE_USB == pHalmac_adapter->halmac_interface) {
		pHalmac_api->halmac_mac_power_switch = halmac_mac_power_switch_8821c_usb;
		pHalmac_api->halmac_cfg_tx_agg_align = halmac_cfg_tx_agg_align_usb_not_support_88xx;
		pHalmac_api->halmac_phy_cfg = halmac_phy_cfg_8821c_usb;
		pHalmac_api->halmac_interface_integration_tuning = halmac_interface_integration_tuning_8821c_usb;
	} else if (HALMAC_INTERFACE_PCIE == pHalmac_adapter->halmac_interface) {
		pHalmac_api->halmac_mac_power_switch = halmac_mac_power_switch_8821c_pcie;
		pHalmac_api->halmac_cfg_tx_agg_align = halmac_cfg_tx_agg_align_pcie_not_support_88xx;
		pHalmac_api->halmac_pcie_switch = halmac_pcie_switch_8821c;
		pHalmac_api->halmac_phy_cfg = halmac_phy_cfg_8821c_pcie;
		pHalmac_api->halmac_interface_integration_tuning = halmac_interface_integration_tuning_8821c_pcie;
	} else {
		pHalmac_api->halmac_pcie_switch = halmac_pcie_switch_8821c_nc;
	}
#endif

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_init_trx_cfg_8821c() - config trx dma register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_trx_mode : trx mode selection
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_init_trx_cfg_8821C(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_TRX_MODE halmac_trx_mode
)
{
	u8 value8;
	u32 value32;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_INIT_TRX_CFG);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;
	pHalmac_adapter->trx_mode = halmac_trx_mode;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_init_trx_cfg ==========>halmac_trx_mode = %d\n", halmac_trx_mode);

	status = halmac_txdma_queue_mapping_8821c(pHalmac_adapter, halmac_trx_mode);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_txdma_queue_mapping fail!\n");
		return status;
	}

	value8 = 0;
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_CR, value8);
	value8 = HALMAC_CR_TRX_ENABLE_8821C;
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_CR, value8);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_H2CQ_CSR, BIT(31));

	status = halmac_priority_queue_config_8821c(pHalmac_adapter, halmac_trx_mode);
	if (HALMAC_RX_FIFO_EXPANDING_MODE_DISABLE != pHalmac_adapter->txff_allocation.rx_fifo_expanding_mode)
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_RX_DRVINFO_SZ, HALMAC_RX_DESC_DUMMY_SIZE_MAX_88XX >> 3);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_txdma_queue_mapping fail!\n");
		return status;
	}

	/* Config H2C packet buffer */
	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_HEAD);
	/* value32 = (value32 & 0xFFFC0000) | HALMAC_RSVD_H2C_QUEUE_BOUNDARY_8821C; */
	/* value32 = (value32 & 0xFFFC0000) | pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy * HALMAC_TX_PAGE_SIZE_8821C; */
	value32 = (value32 & 0xFFFC0000) | (pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy << HALMAC_TX_PAGE_SIZE_2_POWER_8821C);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_H2C_HEAD, value32);

	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_READ_ADDR);
	/* value32 = (value32 & 0xFFFC0000) | HALMAC_RSVD_H2C_QUEUE_BOUNDARY_8821C; */
	value32 = (value32 & 0xFFFC0000) | (pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy << HALMAC_TX_PAGE_SIZE_2_POWER_8821C);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_H2C_READ_ADDR, value32);

	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_TAIL);
	/* value32 = (value32 & 0xFFFC0000) | (HALMAC_RSVD_H2C_QUEUE_BOUNDARY_8821C + HALMAC_RSVD_H2C_QUEUE_SIZE_8821C); */
	value32 = (value32 & 0xFFFC0000) | ((pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy << HALMAC_TX_PAGE_SIZE_2_POWER_8821C) + (HALMAC_RSVD_H2C_QUEUE_PGNUM_8821C << HALMAC_TX_PAGE_SIZE_2_POWER_8821C));
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_H2C_TAIL, value32);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_H2C_INFO);
	value8 = (u8)((value8 & 0xFC) | 0x01);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_H2C_INFO, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_H2C_INFO);
	value8 = (u8)((value8 & 0xFB) | 0x04);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_H2C_INFO, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_TXDMA_OFFSET_CHK + 1);
	value8 = (u8)((value8 & 0x7f) | 0x80);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_TXDMA_OFFSET_CHK + 1, value8);

	pHalmac_adapter->h2c_buff_size = HALMAC_RSVD_H2C_QUEUE_PGNUM_8821C << HALMAC_TX_PAGE_SIZE_2_POWER_8821C;
	halmac_get_h2c_buff_free_space_88xx(pHalmac_adapter);

	if (pHalmac_adapter->h2c_buff_size != pHalmac_adapter->h2c_buf_free_space) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "get h2c free space error!\n");
		return HALMAC_RET_GET_H2C_SPACE_ERR;
	}

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_init_trx_cfg <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_init_protocol_cfg_8821c() - config protocol register
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_init_protocol_cfg_8821c(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u32 max_agg_num, max_rts_agg_num;
	u32 value32;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "[TRACE]halmac_init_protocol_cfg_8821c ==========>\n");

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_AMPDU_MAX_TIME_V1, HALMAC_AMPDU_MAX_TIME_8821C);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_TX_HANG_CTRL, BIT_EN_EOF_V1);

	max_agg_num = HALMAC_PROT_MAX_AGG_PKT_LIMIT_8821C;
	max_rts_agg_num = HALMAC_PROT_RTS_MAX_AGG_PKT_LIMIT_8821C;

	if (HALMAC_INTERFACE_SDIO == pHalmac_adapter->halmac_interface) {
		max_agg_num = HALMAC_PROT_MAX_AGG_PKT_LIMIT_8821C_SDIO;
		max_rts_agg_num = HALMAC_PROT_RTS_MAX_AGG_PKT_LIMIT_8821C_SDIO;
	}

	value32 = HALMAC_PROT_RTS_LEN_TH_8821C | (HALMAC_PROT_RTS_TX_TIME_TH_8821C << 8) | (max_agg_num << 16) | (max_rts_agg_num << 24);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_PROT_MODE_CTRL, value32);

	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_BAR_MODE_CTRL + 2, HALMAC_BAR_RETRY_LIMIT_8821C | HALMAC_RA_TRY_RATE_AGG_LIMIT_8821C << 8);

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FAST_EDCA_VOVI_SETTING, HALMAC_FAST_EDCA_VO_TH_8821C);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FAST_EDCA_VOVI_SETTING + 2, HALMAC_FAST_EDCA_VI_TH_8821C);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FAST_EDCA_BEBK_SETTING, HALMAC_FAST_EDCA_BE_TH_8821C);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FAST_EDCA_BEBK_SETTING + 2, HALMAC_FAST_EDCA_BK_TH_8821C);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "[TRACE]halmac_init_protocol_cfg_8821c <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_init_h2c_8821c() - config h2c packet buffer
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_init_h2c_8821c(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u8 value8;
	u32 value32;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	value8 = 0;
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_CR, value8);
	value8 = HALMAC_CR_TRX_ENABLE_8821C;
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_CR, value8);

	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_HEAD);
	/* value32 = (value32 & 0xFFFC0000) | HALMAC_RSVD_H2C_QUEUE_BOUNDARY_8821C; */
	value32 = (value32 & 0xFFFC0000) | (pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy << HALMAC_TX_PAGE_SIZE_2_POWER_8821C);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_H2C_HEAD, value32);

	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_READ_ADDR);
	/* value32 = (value32 & 0xFFFC0000) | HALMAC_RSVD_H2C_QUEUE_BOUNDARY_8821C; */
	value32 = (value32 & 0xFFFC0000) | (pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy << HALMAC_TX_PAGE_SIZE_2_POWER_8821C);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_H2C_READ_ADDR, value32);

	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_TAIL);
	/* value32 = (value32 & 0xFFFC0000) | (HALMAC_RSVD_H2C_QUEUE_BOUNDARY_8821C + HALMAC_RSVD_H2C_QUEUE_SIZE_8821C); */
	value32 = (value32 & 0xFFFC0000) | ((pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy << HALMAC_TX_PAGE_SIZE_2_POWER_8821C) + (HALMAC_RSVD_H2C_QUEUE_PGNUM_8821C << HALMAC_TX_PAGE_SIZE_2_POWER_8821C));
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_H2C_TAIL, value32);
	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_H2C_INFO);
	value8 = (u8)((value8 & 0xFC) | 0x01);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_H2C_INFO, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_H2C_INFO);
	value8 = (u8)((value8 & 0xFB) | 0x04);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_H2C_INFO, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_TXDMA_OFFSET_CHK + 1);
	value8 = (u8)((value8 & 0x7f) | 0x80);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_TXDMA_OFFSET_CHK + 1, value8);

	pHalmac_adapter->h2c_buff_size = (HALMAC_RSVD_H2C_QUEUE_PGNUM_8821C << HALMAC_TX_PAGE_SIZE_2_POWER_8821C);
	halmac_get_h2c_buff_free_space_88xx(pHalmac_adapter);

	if (pHalmac_adapter->h2c_buff_size != pHalmac_adapter->h2c_buf_free_space) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "get h2c free space error!\n");
		return HALMAC_RET_GET_H2C_SPACE_ERR;
	}

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "h2c free space : %d\n", pHalmac_adapter->h2c_buf_free_space);

	return HALMAC_RET_SUCCESS;
}
