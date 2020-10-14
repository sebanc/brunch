#include "halmac_88xx_cfg.h"

HALMAC_RET_STATUS
halmac_dump_efuse_fw_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
);

HALMAC_RET_STATUS
halmac_dump_efuse_drv_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
);

HALMAC_RET_STATUS
halmac_update_eeprom_mask_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	INOUT PHALMAC_PG_EFUSE_INFO pPg_efuse_info,
	OUT u8 *pEeprom_mask_updated
);

HALMAC_RET_STATUS
halmac_check_efuse_enough_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PG_EFUSE_INFO pPg_efuse_info,
	IN u8 *pEeprom_mask_updated
);

HALMAC_RET_STATUS
halmac_program_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PG_EFUSE_INFO pPg_efuse_info,
	IN u8 *pEeprom_mask_updated
);

HALMAC_RET_STATUS
halmac_pwr_sub_seq_parer_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 cut,
	IN u8 fab,
	IN u8 intf,
	IN PHALMAC_WLAN_PWR_CFG pPwr_sub_seq_cfg
);

HALMAC_RET_STATUS
halmac_parse_c2h_debug_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_scan_status_rpt_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_psd_data_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_efuse_data_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);


HALMAC_RET_STATUS
halmac_parse_h2c_ack_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_physical_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_enqueue_para_buff_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PHY_PARAMETER_INFO para_info,
	IN u8 *pCurr_buff_wptr,
	OUT u8 *pEnd_cmd
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_phy_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_cfg_para_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_gen_cfg_para_h2c_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pH2c_buff
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_update_packet_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);
HALMAC_RET_STATUS
halmac_parse_h2c_ack_update_datapack_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_run_datapack_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_channel_switch_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_iqk_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

HALMAC_RET_STATUS
halmac_parse_h2c_ack_power_tracking_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
);

VOID
halmac_init_offload_feature_state_machine_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	PHALMAC_STATE pState = &(pHalmac_adapter->halmac_state);

	pState->efuse_state_set.efuse_cmd_construct_state = HALMAC_EFUSE_CMD_CONSTRUCT_IDLE;
	pState->efuse_state_set.process_status = HALMAC_CMD_PROCESS_IDLE;
	pState->efuse_state_set.seq_num = pHalmac_adapter->h2c_packet_seq;

	pState->cfg_para_state_set.cfg_para_cmd_construct_state = HALMAC_CFG_PARA_CMD_CONSTRUCT_IDLE;
	pState->cfg_para_state_set.process_status = HALMAC_CMD_PROCESS_IDLE;
	pState->cfg_para_state_set.seq_num = pHalmac_adapter->h2c_packet_seq;

	pState->scan_state_set.scan_cmd_construct_state = HALMAC_SCAN_CMD_CONSTRUCT_IDLE;
	pState->scan_state_set.process_status = HALMAC_CMD_PROCESS_IDLE;
	pState->scan_state_set.seq_num = pHalmac_adapter->h2c_packet_seq;

	pState->update_packet_set.process_status = HALMAC_CMD_PROCESS_IDLE;
	pState->update_packet_set.seq_num = pHalmac_adapter->h2c_packet_seq;

	pState->iqk_set.process_status = HALMAC_CMD_PROCESS_IDLE;
	pState->iqk_set.seq_num = pHalmac_adapter->h2c_packet_seq;

	pState->power_tracking_set.process_status = HALMAC_CMD_PROCESS_IDLE;
	pState->power_tracking_set.seq_num = pHalmac_adapter->h2c_packet_seq;

	pState->psd_set.process_status = HALMAC_CMD_PROCESS_IDLE;
	pState->psd_set.seq_num = pHalmac_adapter->h2c_packet_seq;
	pState->psd_set.data_size = 0;
	pState->psd_set.segment_size = 0;
	pState->psd_set.pData = NULL;
}

HALMAC_RET_STATUS
halmac_dump_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_EFUSE_READ_CFG cfg
)
{
	u32 chk_h2c_init;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
	HALMAC_CMD_PROCESS_STATUS *pProcess_status = &(pHalmac_adapter->halmac_state.efuse_state_set.process_status);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	*pProcess_status = HALMAC_CMD_PROCESS_SENDING;

	if (HALMAC_RET_SUCCESS != halmac_transition_efuse_state_88xx(pHalmac_adapter, HALMAC_EFUSE_CMD_CONSTRUCT_H2C_SENT))
		return HALMAC_RET_ERROR_STATE;

	if (HALMAC_EFUSE_R_AUTO == cfg) {
		chk_h2c_init = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_PKT_READADDR);
		if (HALMAC_DLFW_NONE == pHalmac_adapter->halmac_state.dlfw_state || 0 == chk_h2c_init)
			status = halmac_dump_efuse_drv_88xx(pHalmac_adapter);
		else
			status = halmac_dump_efuse_fw_88xx(pHalmac_adapter);
	} else if (HALMAC_EFUSE_R_FW == cfg) {
		status = halmac_dump_efuse_fw_88xx(pHalmac_adapter);
	} else {
		status = halmac_dump_efuse_drv_88xx(pHalmac_adapter);
	}

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "halmac_read_efuse error = %x\n", status);
		return status;
	}

	return status;
}

HALMAC_RET_STATUS
halmac_func_read_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 offset,
	IN u32 size,
	OUT u8 *pEfuse_map
)
{
	VOID *pDriver_adapter = NULL;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	if (NULL == pEfuse_map) {
		PLATFORM_MSG_PRINT(pHalmac_adapter->pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "Malloc for dump efuse map error\n");
		return HALMAC_RET_NULL_POINTER;
	}

	if (_TRUE == pHalmac_adapter->hal_efuse_map_valid)
		PLATFORM_RTL_MEMCPY(pDriver_adapter, pEfuse_map, pHalmac_adapter->pHalEfuse_map + offset, size);
	else
	if (HALMAC_RET_SUCCESS != halmac_read_hw_efuse_88xx(pHalmac_adapter, offset, size, pEfuse_map))
		return HALMAC_RET_EFUSE_R_FAIL;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_read_hw_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 offset,
	IN u32 size,
	OUT u8 *pEfuse_map
)
{
	u8 value8;
	u32 value32;
	u32 address;
	u32 tmp32, counter;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	/* Read efuse no need 2.5V LDO */
	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 3);
	if (value8 & BIT(7))
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 3, (u8)(value8 & ~(BIT(7))));

	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_EFUSE_CTRL);

	for (address = offset; address < offset + size; address++) {
		value32 = value32 & ~((BIT_MASK_EF_DATA) | (BIT_MASK_EF_ADDR << BIT_SHIFT_EF_ADDR));
		value32 = value32 | ((address & BIT_MASK_EF_ADDR) << BIT_SHIFT_EF_ADDR);
		HALMAC_REG_WRITE_32(pHalmac_adapter, REG_EFUSE_CTRL, value32 & (~BIT_EF_FLAG));

		counter = 1000000;
		do {
			PLATFORM_RTL_DELAY_US(pDriver_adapter, 1);
			tmp32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_EFUSE_CTRL);
			counter--;
			if (0 == counter) {
				PLATFORM_MSG_PRINT(pHalmac_adapter->pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "HALMAC_RET_EFUSE_R_FAIL\n");
				return HALMAC_RET_EFUSE_R_FAIL;
			}
		} while (0 == (tmp32 & BIT_EF_FLAG));

		*(pEfuse_map + address - offset) = (u8)(tmp32 & BIT_MASK_EF_DATA);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_dump_efuse_drv_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u8 *pEfuse_map = NULL;
	u32 efuse_size;
	VOID *pDriver_adapter = NULL;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	efuse_size = pHalmac_adapter->hw_config_info.efuse_size;

	if (NULL == pHalmac_adapter->pHalEfuse_map) {
		pHalmac_adapter->pHalEfuse_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, efuse_size);
		if (NULL == pHalmac_adapter->pHalEfuse_map) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate efuse map Fail!!\n");
			return HALMAC_RET_MALLOC_FAIL;
		}
	}

	pEfuse_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, efuse_size);
	if (NULL == pEfuse_map) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate local efuse map Fail!!\n");
		return HALMAC_RET_MALLOC_FAIL;
	}

	if (HALMAC_RET_SUCCESS != halmac_read_hw_efuse_88xx(pHalmac_adapter, 0, efuse_size, pEfuse_map)) {
		PLATFORM_RTL_FREE(pDriver_adapter, pEfuse_map, efuse_size);
		return HALMAC_RET_EFUSE_R_FAIL;
	}

	PLATFORM_MUTEX_LOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));
	PLATFORM_RTL_MEMCPY(pDriver_adapter, pHalmac_adapter->pHalEfuse_map, pEfuse_map, efuse_size);
	pHalmac_adapter->hal_efuse_map_valid = _TRUE;
	PLATFORM_MUTEX_UNLOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));

	PLATFORM_RTL_FREE(pDriver_adapter, pEfuse_map, efuse_size);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_dump_efuse_fw_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u32 eeprom_size = pHalmac_adapter->hw_config_info.eeprom_size;
	u16 h2c_seq_mum = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	h2c_header_info.sub_cmd_id = SUB_CMD_ID_DUMP_PHYSICAL_EFUSE;
	h2c_header_info.content_size = 0;
	h2c_header_info.ack = _TRUE;
	halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);
	pHalmac_adapter->halmac_state.efuse_state_set.seq_num = h2c_seq_mum;

	if (NULL == pHalmac_adapter->pHalEfuse_map) {
		pHalmac_adapter->pHalEfuse_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, pHalmac_adapter->hw_config_info.efuse_size);
		if (NULL == pHalmac_adapter->pHalEfuse_map) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac allocate efuse map Fail!!\n");
			return HALMAC_RET_MALLOC_FAIL;
		}
	}

	if (_FALSE == pHalmac_adapter->hal_efuse_map_valid) {
		status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);
		if (HALMAC_RET_SUCCESS != status) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_read_efuse_fw Fail = %x!!\n", status);
			return status;
		}
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_func_write_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 offset,
	IN u8 value
)
{
	const u8 wite_protect_code = 0x69;
	u32 value32, tmp32, counter;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MUTEX_LOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));
	pHalmac_adapter->hal_efuse_map_valid = _FALSE;
	PLATFORM_MUTEX_UNLOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PMC_DBG_CTRL2 + 3, wite_protect_code);

	/* Enable 2.5V LDO */
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 3, (u8)(HALMAC_REG_READ_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 3) | BIT(7)));

	value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_EFUSE_CTRL);
	value32 = value32 & ~((BIT_MASK_EF_DATA) | (BIT_MASK_EF_ADDR << BIT_SHIFT_EF_ADDR));
	value32 = value32 | ((offset & BIT_MASK_EF_ADDR) << BIT_SHIFT_EF_ADDR) | (value & BIT_MASK_EF_DATA);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_EFUSE_CTRL, value32 | BIT_EF_FLAG);

	counter = 1000000;
	do {
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 1);
		tmp32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_EFUSE_CTRL);
		counter--;
		if (0 == counter) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_write_efuse Fail !!\n");
			return HALMAC_RET_EFUSE_W_FAIL;
		}
	} while (BIT_EF_FLAG == (tmp32 & BIT_EF_FLAG));

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PMC_DBG_CTRL2 + 3, 0x00);

	/* Disable 2.5V LDO */
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 3, (u8)(HALMAC_REG_READ_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 3) & ~(BIT(7))));

	return HALMAC_RET_SUCCESS;
}


HALMAC_RET_STATUS
halmac_func_switch_efuse_bank_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_EFUSE_BANK efuse_bank
)
{
	u8 reg_value;
	PHALMAC_API pHalmac_api;

	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_transition_efuse_state_88xx(pHalmac_adapter, HALMAC_EFUSE_CMD_CONSTRUCT_BUSY))
		return HALMAC_RET_ERROR_STATE;

	reg_value = HALMAC_REG_READ_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 1);

	if (efuse_bank == (reg_value & (BIT(0) | BIT(1))))
		return HALMAC_RET_SUCCESS;

	reg_value &= ~(BIT(0) | BIT(1));
	reg_value |= efuse_bank;
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 1, reg_value);

	if ((HALMAC_REG_READ_8(pHalmac_adapter, REG_LDO_EFUSE_CTRL + 1) & (BIT(0) | BIT(1))) != efuse_bank)
		return HALMAC_RET_SWITCH_EFUSE_BANK_FAIL;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_eeprom_parser_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pPhysical_efuse_map,
	OUT u8 *pLogical_efuse_map
)
{
	u8 j;
	u8 value8;
	u8 block_index;
	u8 valid_word_enable, word_enable;
	u8 efuse_read_header, efuse_read_header2 = 0;
	u32 eeprom_index;
	u32 efuse_index = 0;
	u32 eeprom_size = pHalmac_adapter->hw_config_info.eeprom_size;
	VOID *pDriver_adapter = NULL;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_RTL_MEMSET(pDriver_adapter, pLogical_efuse_map, 0xFF, eeprom_size);

	do {
		value8 = *(pPhysical_efuse_map + efuse_index);
		efuse_read_header = value8;

		if ((efuse_read_header & 0x1f) == 0x0f) {
			efuse_index++;
			value8 = *(pPhysical_efuse_map + efuse_index);
			efuse_read_header2 = value8;
			block_index = ((efuse_read_header2 & 0xF0) >> 1) | ((efuse_read_header >> 5) & 0x07);
			word_enable = efuse_read_header2 & 0x0F;
		} else {
			block_index = (efuse_read_header & 0xF0) >> 4;
			word_enable = efuse_read_header & 0x0F;
		}

		if (efuse_read_header == 0xff)
			break;

		efuse_index++;

		if (efuse_index >= pHalmac_adapter->hw_config_info.efuse_size - HALMAC_PROTECTED_EFUSE_SIZE_88XX - 1)
			return HALMAC_RET_EEPROM_PARSING_FAIL;

		for (j = 0; j < 4; j++) {
			valid_word_enable = (u8)((~(word_enable >> j)) & BIT(0));
			if (valid_word_enable == 1) {
				eeprom_index = (block_index << 3) + (j << 1);

				if ((eeprom_index + 1) > eeprom_size) {
					PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "Error: EEPROM addr exceeds eeprom_size:0x%X, at eFuse 0x%X\n", eeprom_size, efuse_index - 1);
					if ((efuse_read_header & 0x1f) == 0x0f)
						PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "Error: EEPROM header: 0x%X, 0x%X,\n", efuse_read_header, efuse_read_header2);
					else
						PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "Error: EEPROM header: 0x%X,\n", efuse_read_header);

					return HALMAC_RET_EEPROM_PARSING_FAIL;
				} else {
					value8 = *(pPhysical_efuse_map + efuse_index);
					*(pLogical_efuse_map + eeprom_index) = value8;

					eeprom_index++;
					efuse_index++;

					if (efuse_index > pHalmac_adapter->hw_config_info.efuse_size - HALMAC_PROTECTED_EFUSE_SIZE_88XX - 1)
						return HALMAC_RET_EEPROM_PARSING_FAIL;

					value8 = *(pPhysical_efuse_map + efuse_index);
					*(pLogical_efuse_map + eeprom_index) = value8;

					efuse_index++;

					if (efuse_index > pHalmac_adapter->hw_config_info.efuse_size - HALMAC_PROTECTED_EFUSE_SIZE_88XX)
						return HALMAC_RET_EEPROM_PARSING_FAIL;
				}
			}
		}
	} while (1);

	pHalmac_adapter->efuse_end = efuse_index;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_read_logical_efuse_map_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pMap
)
{
	u8 *pEfuse_map = NULL;
	u32 efuse_size;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	efuse_size = pHalmac_adapter->hw_config_info.efuse_size;

	if (_FALSE == pHalmac_adapter->hal_efuse_map_valid) {
		pEfuse_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, efuse_size);
		if (NULL == pEfuse_map) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate local efuse map Fail!!\n");
			return HALMAC_RET_MALLOC_FAIL;
		}

		status = halmac_func_read_efuse_88xx(pHalmac_adapter, 0, efuse_size, pEfuse_map);
		if (HALMAC_RET_SUCCESS != status) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac_read_efuse error = %x\n", status);
			PLATFORM_RTL_FREE(pDriver_adapter, pEfuse_map, efuse_size);
			return status;
		}

		if (NULL == pHalmac_adapter->pHalEfuse_map) {
			pHalmac_adapter->pHalEfuse_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, efuse_size);
			if (NULL == pHalmac_adapter->pHalEfuse_map) {
				PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate efuse map Fail!!\n");
				PLATFORM_RTL_FREE(pDriver_adapter, pEfuse_map, efuse_size);
				return HALMAC_RET_MALLOC_FAIL;
			}
		}

		PLATFORM_MUTEX_LOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));
		PLATFORM_RTL_MEMCPY(pDriver_adapter, pHalmac_adapter->pHalEfuse_map, pEfuse_map, efuse_size);
		pHalmac_adapter->hal_efuse_map_valid = _TRUE;
		PLATFORM_MUTEX_UNLOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));

		PLATFORM_RTL_FREE(pDriver_adapter, pEfuse_map, efuse_size);
	}

	if (HALMAC_RET_SUCCESS != halmac_eeprom_parser_88xx(pHalmac_adapter, pHalmac_adapter->pHalEfuse_map, pMap))
		return HALMAC_RET_EEPROM_PARSING_FAIL;

	return status;
}

HALMAC_RET_STATUS
halmac_func_write_logical_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 offset,
	IN u8 value
)
{
	u8 pg_efuse_byte1, pg_efuse_byte2;
	u8 pg_block, pg_block_index;
	u8 pg_efuse_header, pg_efuse_header2;
	u8 *pEeprom_map = NULL;
	u32 eeprom_size = pHalmac_adapter->hw_config_info.eeprom_size;
	u32 efuse_end, pg_efuse_num;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	pEeprom_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, eeprom_size);
	if (NULL == pEeprom_map) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate local eeprom map Fail!!\n");
		return HALMAC_RET_MALLOC_FAIL;
	}
	PLATFORM_RTL_MEMSET(pDriver_adapter, pEeprom_map, 0xFF, eeprom_size);

	status = halmac_read_logical_efuse_map_88xx(pHalmac_adapter, pEeprom_map);
	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac_read_logical_efuse_map_88xx error = %x\n", status);
		PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
		return status;
	}

	if (*(pEeprom_map + offset) != value) {
		efuse_end = pHalmac_adapter->efuse_end;
		pg_block = (u8)(offset >> 3);
		pg_block_index = (u8)((offset & (8 - 1)) >> 1);

		if (offset > 0x7f) {
			pg_efuse_header = (((pg_block & 0x07) << 5) & 0xE0) | 0x0F;
			pg_efuse_header2 = (u8)(((pg_block & 0x78) << 1) + ((0x1 << pg_block_index) ^ 0x0F));
		} else {
			pg_efuse_header = (u8)((pg_block << 4) + ((0x01 << pg_block_index) ^ 0x0F));
		}

		if ((offset & 1) == 0) {
			pg_efuse_byte1 = value;
			pg_efuse_byte2 = *(pEeprom_map + offset + 1);
		} else {
			pg_efuse_byte1 = *(pEeprom_map + offset - 1);
			pg_efuse_byte2 = value;
		}

		if (offset > 0x7f) {
			pg_efuse_num = 4;
			if (pHalmac_adapter->hw_config_info.efuse_size <= (pg_efuse_num + HALMAC_PROTECTED_EFUSE_SIZE_88XX + pHalmac_adapter->efuse_end)) {
				PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
				return HALMAC_RET_EFUSE_NOT_ENOUGH;
			}
			halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end, pg_efuse_header);
			halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 1, pg_efuse_header2);
			halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 2, pg_efuse_byte1);
			status = halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 3, pg_efuse_byte2);
		} else {
			pg_efuse_num = 3;
			if (pHalmac_adapter->hw_config_info.efuse_size <= (pg_efuse_num + HALMAC_PROTECTED_EFUSE_SIZE_88XX + pHalmac_adapter->efuse_end)) {
				PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
				return HALMAC_RET_EFUSE_NOT_ENOUGH;
			}
			halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end, pg_efuse_header);
			halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 1, pg_efuse_byte1);
			status = halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 2, pg_efuse_byte2);
		}

		if (HALMAC_RET_SUCCESS != status) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac_write_logical_efuse error = %x\n", status);
			PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
			return status;
		}
	}

	PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_func_pg_efuse_by_map_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PG_EFUSE_INFO pPg_efuse_info,
	IN HALMAC_EFUSE_READ_CFG cfg
)
{
	u8 *pEeprom_mask_updated = NULL;
	u32 eeprom_mask_size = pHalmac_adapter->hw_config_info.eeprom_size >> 4;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pEeprom_mask_updated = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, eeprom_mask_size);
	if (NULL == pEeprom_mask_updated) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate local eeprom map Fail!!\n");
		return HALMAC_RET_MALLOC_FAIL;
	}
	PLATFORM_RTL_MEMSET(pDriver_adapter, pEeprom_mask_updated, 0x00, eeprom_mask_size);

	status = halmac_update_eeprom_mask_88xx(pHalmac_adapter, pPg_efuse_info, pEeprom_mask_updated);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac_update_eeprom_mask_88xx error = %x\n", status);
		PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_mask_updated, eeprom_mask_size);
		return status;
	}

	status = halmac_check_efuse_enough_88xx(pHalmac_adapter, pPg_efuse_info, pEeprom_mask_updated);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac_check_efuse_enough_88xx error = %x\n", status);
		PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_mask_updated, eeprom_mask_size);
		return status;
	}

	status = halmac_program_efuse_88xx(pHalmac_adapter, pPg_efuse_info, pEeprom_mask_updated);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac_program_efuse_88xx error = %x\n", status);
		PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_mask_updated, eeprom_mask_size);
		return status;
	}

	PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_mask_updated, eeprom_mask_size);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_update_eeprom_mask_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	INOUT PHALMAC_PG_EFUSE_INFO	pPg_efuse_info,
	OUT u8 *pEeprom_mask_updated
)
{
	u8 *pEeprom_map = NULL;
	u32 eeprom_size = pHalmac_adapter->hw_config_info.eeprom_size;
	u8 *pEeprom_map_pg, *pEeprom_mask;
	u16 i, j;
	u16 map_byte_offset, mask_byte_offset;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	VOID *pDriver_adapter = NULL;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	pEeprom_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, eeprom_size);
	if (NULL == pEeprom_map) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate local eeprom map Fail!!\n");
		return HALMAC_RET_MALLOC_FAIL;
	}
	PLATFORM_RTL_MEMSET(pDriver_adapter, pEeprom_map, 0xFF, eeprom_size);

	PLATFORM_RTL_MEMSET(pDriver_adapter, pEeprom_mask_updated, 0x00, pPg_efuse_info->efuse_mask_size);

	status = halmac_read_logical_efuse_map_88xx(pHalmac_adapter, pEeprom_map);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
		return status;
	}

	pEeprom_map_pg = pPg_efuse_info->pEfuse_map;
	pEeprom_mask = pPg_efuse_info->pEfuse_mask;


	for (i = 0; i < pPg_efuse_info->efuse_mask_size; i++)
		*(pEeprom_mask_updated + i) = *(pEeprom_mask + i);

	for (i = 0; i < pPg_efuse_info->efuse_map_size; i = i + 16) {
		for (j = 0; j < 16; j = j + 2) {
			map_byte_offset = i + j;
			mask_byte_offset = i >> 4;
			if (*(pEeprom_map_pg + map_byte_offset) == *(pEeprom_map + map_byte_offset)) {
				if (*(pEeprom_map_pg + map_byte_offset + 1) == *(pEeprom_map + map_byte_offset + 1)) {
					switch (j) {
					case 0:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(4) ^ 0xFF);
						break;
					case 2:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(5) ^ 0xFF);
						break;
					case 4:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(6) ^ 0xFF);
						break;
					case 6:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(7) ^ 0xFF);
						break;
					case 8:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(0) ^ 0xFF);
						break;
					case 10:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(1) ^ 0xFF);
						break;
					case 12:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(2) ^ 0xFF);
						break;
					case 14:
						*(pEeprom_mask_updated + mask_byte_offset) = *(pEeprom_mask_updated + mask_byte_offset) & (BIT(3) ^ 0xFF);
						break;
					default:
						break;
					}
				}
			}
		}
	}

	PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);

	return status;
}

HALMAC_RET_STATUS
halmac_check_efuse_enough_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PG_EFUSE_INFO pPg_efuse_info,
	IN u8 *pEeprom_mask_updated
)
{
	u8 pre_word_enb, word_enb;
	u8 pg_efuse_header, pg_efuse_header2;
	u8 pg_block;
	u16 i, j;
	u32 efuse_end;
	u32 tmp_eeprom_offset, pg_efuse_num = 0;

	efuse_end = pHalmac_adapter->efuse_end;

	for (i = 0; i < pPg_efuse_info->efuse_map_size; i = i + 8) {
		tmp_eeprom_offset = i;

		if ((tmp_eeprom_offset & 7) > 0) {
			pre_word_enb = (*(pEeprom_mask_updated + (i >> 4)) & 0x0F);
			word_enb = pre_word_enb ^ 0x0F;
		} else {
			pre_word_enb = (*(pEeprom_mask_updated + (i >> 4)) >> 4);
			word_enb = pre_word_enb ^ 0x0F;
		}

		pg_block = (u8)(tmp_eeprom_offset >> 3);

		if (pre_word_enb > 0) {
			if (tmp_eeprom_offset > 0x7f) {
				pg_efuse_header = (((pg_block & 0x07) << 5) & 0xE0) | 0x0F;
				pg_efuse_header2 = (u8)(((pg_block & 0x78) << 1) + word_enb);
			} else {
				pg_efuse_header = (u8)((pg_block << 4) + word_enb);
			}

			if (tmp_eeprom_offset > 0x7f) {
				pg_efuse_num++;
				pg_efuse_num++;
				efuse_end = efuse_end + 2;
				for (j = 0; j < 4; j++) {
					if (((pre_word_enb >> j) & 0x1) > 0) {
						pg_efuse_num++;
						pg_efuse_num++;
						efuse_end = efuse_end + 2;
					}
				}
			} else {
				pg_efuse_num++;
				efuse_end = efuse_end + 1;
				for (j = 0; j < 4; j++) {
					if (((pre_word_enb >> j) & 0x1) > 0) {
						pg_efuse_num++;
						pg_efuse_num++;
						efuse_end = efuse_end + 2;
					}
				}
			}
		}
	}

	if (pHalmac_adapter->hw_config_info.efuse_size <= (pg_efuse_num + HALMAC_PROTECTED_EFUSE_SIZE_88XX + pHalmac_adapter->efuse_end))
		return HALMAC_RET_EFUSE_NOT_ENOUGH;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_program_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PG_EFUSE_INFO pPg_efuse_info,
	IN u8 *pEeprom_mask_updated
)
{
	u8 pre_word_enb, word_enb;
	u8 pg_efuse_header, pg_efuse_header2;
	u8 pg_block;
	u16 i, j;
	u32 efuse_end;
	u32 tmp_eeprom_offset;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	efuse_end = pHalmac_adapter->efuse_end;

	for (i = 0; i < pPg_efuse_info->efuse_map_size; i = i + 8) {
		tmp_eeprom_offset = i;

		if (((tmp_eeprom_offset >> 3) & 1) > 0) {
			pre_word_enb = (*(pEeprom_mask_updated + (i >> 4)) & 0x0F);
			word_enb = pre_word_enb ^ 0x0F;
		} else {
			pre_word_enb = (*(pEeprom_mask_updated + (i >> 4)) >> 4);
			word_enb = pre_word_enb ^ 0x0F;
		}

		pg_block = (u8)(tmp_eeprom_offset >> 3);

		if (pre_word_enb > 0) {
			if (tmp_eeprom_offset > 0x7f) {
				pg_efuse_header = (((pg_block & 0x07) << 5) & 0xE0) | 0x0F;
				pg_efuse_header2 = (u8)(((pg_block & 0x78) << 1) + word_enb);
			} else {
				pg_efuse_header = (u8)((pg_block << 4) + word_enb);
			}

			if (tmp_eeprom_offset > 0x7f) {
				halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end, pg_efuse_header);
				status = halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 1, pg_efuse_header2);
				efuse_end = efuse_end + 2;
				for (j = 0; j < 4; j++) {
					if (((pre_word_enb >> j) & 0x1) > 0) {
						halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end, *(pPg_efuse_info->pEfuse_map + tmp_eeprom_offset + (j << 1)));
						status = halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 1, *(pPg_efuse_info->pEfuse_map + tmp_eeprom_offset + (j << 1) + 1));
						efuse_end = efuse_end + 2;
					}
				}
			} else {
				status = halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end, pg_efuse_header);
				efuse_end = efuse_end + 1;
				for (j = 0; j < 4; j++) {
					if (((pre_word_enb >> j) & 0x1) > 0) {
						halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end, *(pPg_efuse_info->pEfuse_map + tmp_eeprom_offset + (j << 1)));
						status = halmac_func_write_efuse_88xx(pHalmac_adapter, efuse_end + 1, *(pPg_efuse_info->pEfuse_map + tmp_eeprom_offset + (j << 1) + 1));
						efuse_end = efuse_end + 2;
					}
				}
			}
		}
	}

	return status;
}

HALMAC_RET_STATUS
halmac_update_fw_info_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pHamacl_fw,
	IN u32 halmac_fw_size
)
{
	PHALMAC_FW_VERSION pFw_info = &(pHalmac_adapter->fw_version);

	pFw_info->version = rtk_le16_to_cpu(*((u16 *)(pHamacl_fw + HALMAC_FWHDR_OFFSET_VERSION_88XX)));
	pFw_info->sub_version = *(pHamacl_fw + HALMAC_FWHDR_OFFSET_SUBVERSION_88XX);
	pFw_info->sub_index = *(pHamacl_fw + HALMAC_FWHDR_OFFSET_SUBINDEX_88XX);
	pFw_info->h2c_version = (u16)rtk_le32_to_cpu(*((u32 *)(pHamacl_fw + HALMAC_FWHDR_OFFSET_H2C_FORMAT_VER_88XX)));
	pFw_info->build_time.month = *(pHamacl_fw + HALMAC_FWHDR_OFFSET_MONTH_88XX);
	pFw_info->build_time.date = *(pHamacl_fw + HALMAC_FWHDR_OFFSET_DATE_88XX);
	pFw_info->build_time.hour = *(pHamacl_fw + HALMAC_FWHDR_OFFSET_HOUR_88XX);
	pFw_info->build_time.min = *(pHamacl_fw + HALMAC_FWHDR_OFFSET_MIN_88XX);
	pFw_info->build_time.year = rtk_le16_to_cpu(*((u16 *)(pHamacl_fw + HALMAC_FWHDR_OFFSET_YEAR_88XX)));

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_dlfw_to_mem_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pRam_code,
	IN u32 dest,
	IN u32 code_size
)
{
	u8 *pCode_ptr;
	u8 first_part;
	u32 mem_offset;
	u32 pkt_size_tmp, send_pkt_size;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	pCode_ptr = pRam_code;
	mem_offset = 0;
	first_part = 1;
	pkt_size_tmp = code_size;

	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_DDMA_CH0CTRL, HALMAC_REG_READ_32(pHalmac_adapter, REG_DDMA_CH0CTRL) | BIT_DDMACH0_RESET_CHKSUM_STS);

	while (0 != pkt_size_tmp) {
		if (pkt_size_tmp >= pHalmac_adapter->max_download_size)
			send_pkt_size = pHalmac_adapter->max_download_size;
		else
			send_pkt_size = pkt_size_tmp;

		if (HALMAC_RET_SUCCESS != halmac_send_fwpkt_88xx(pHalmac_adapter, pCode_ptr + mem_offset, send_pkt_size)) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_send_fwpkt_88xx fail!!");
			return HALMAC_RET_DLFW_FAIL;
		}

		if (HALMAC_RET_SUCCESS != halmac_iddma_dlfw_88xx(pHalmac_adapter, HALMAC_OCPBASE_TXBUF_88XX + pHalmac_adapter->hw_config_info.txdesc_size,
			    dest + mem_offset, send_pkt_size, first_part)) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_iddma_dlfw_88xx fail!!");
			return HALMAC_RET_DLFW_FAIL;
		}

		first_part = 0;
		mem_offset += send_pkt_size;
		pkt_size_tmp -= send_pkt_size;
	}

	if (HALMAC_RET_SUCCESS != halmac_check_fw_chksum_88xx(pHalmac_adapter, dest)) {
		PLATFORM_MSG_PRINT(pHalmac_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_check_fw_chksum_88xx fail!!");
		return HALMAC_RET_DLFW_FAIL;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_send_fwpkt_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pRam_code,
	IN u32 code_size
)
{
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	if (HALMAC_RET_SUCCESS != halmac_download_rsvd_page_88xx(pHalmac_adapter, pRam_code, code_size)) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_FW, HALMAC_DBG_ERR, "PLATFORM_SEND_RSVD_PAGE 0 error!!\n");
		return HALMAC_RET_DL_RSVD_PAGE_FAIL;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_iddma_dlfw_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 source,
	IN u32 dest,
	IN u32 length,
	IN u8 first
)
{
	u32 counter;
	u32 ch0_control = (u32)(BIT_DDMACH0_CHKSUM_EN | BIT_DDMACH0_OWN);
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	counter = HALMC_DDMA_POLLING_COUNT;
	while (HALMAC_REG_READ_32(pHalmac_adapter, REG_DDMA_CH0CTRL) & BIT_DDMACH0_OWN) {
		counter--;
		if (0 == counter) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_FW, HALMAC_DBG_ERR, "halmac_iddma_dlfw_88xx error-1!!\n");
			return HALMAC_RET_DDMA_FAIL;
		}
	}

	ch0_control |= (length & BIT_MASK_DDMACH0_DLEN);
	if (0 == first)
		ch0_control |= BIT_DDMACH0_CHKSUM_CONT;

	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_DDMA_CH0SA, source);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_DDMA_CH0DA, dest);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_DDMA_CH0CTRL, ch0_control);

	counter = HALMC_DDMA_POLLING_COUNT;
	while (HALMAC_REG_READ_32(pHalmac_adapter, REG_DDMA_CH0CTRL) & BIT_DDMACH0_OWN) {
		counter--;
		if (0 == counter) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_FW, HALMAC_DBG_ERR, "halmac_iddma_dlfw_88xx error-2!!\n");
			return HALMAC_RET_DDMA_FAIL;
		}
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_check_fw_chksum_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 memory_address
)
{
	u8 mcu_fw_ctrl;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	mcu_fw_ctrl = HALMAC_REG_READ_8(pHalmac_adapter, REG_MCUFW_CTRL);

	if (HALMAC_REG_READ_32(pHalmac_adapter, REG_DDMA_CH0CTRL) & BIT_DDMACH0_CHKSUM_STS) {
		if (memory_address < HALMAC_OCPBASE_DMEM_88XX) {
			mcu_fw_ctrl |= BIT_IMEM_DW_OK;
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_MCUFW_CTRL, (u8)(mcu_fw_ctrl & ~(BIT_IMEM_CHKSUM_OK)));
		} else {
			mcu_fw_ctrl |= BIT_DMEM_DW_OK;
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_MCUFW_CTRL, (u8)(mcu_fw_ctrl & ~(BIT_DMEM_CHKSUM_OK)));
		}

		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_FW, HALMAC_DBG_ERR, "halmac_check_fw_chksum_88xx error!!\n");

		return HALMAC_RET_FW_CHECKSUM_FAIL;
	} else {
		if (memory_address < HALMAC_OCPBASE_DMEM_88XX) {
			mcu_fw_ctrl |= BIT_IMEM_DW_OK;
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_MCUFW_CTRL, (u8)(mcu_fw_ctrl | BIT_IMEM_CHKSUM_OK));
		} else {
			mcu_fw_ctrl |= BIT_DMEM_DW_OK;
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_MCUFW_CTRL, (u8)(mcu_fw_ctrl | BIT_DMEM_CHKSUM_OK));
		}

		return HALMAC_RET_SUCCESS;
	}
}

HALMAC_RET_STATUS
halmac_dlfw_end_flow_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u8 value8;
	u32 counter;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	PHALMAC_API pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_TXDMA_STATUS, BIT(2));

	/* Check IMEM & DMEM checksum is OK or not */
	if (0x50 == (HALMAC_REG_READ_8(pHalmac_adapter, REG_MCUFW_CTRL) & 0x50))
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_MCUFW_CTRL, (u16)(HALMAC_REG_READ_16(pHalmac_adapter, REG_MCUFW_CTRL) | BIT_FW_DW_RDY));
	else
		return HALMAC_RET_DLFW_FAIL;

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_MCUFW_CTRL, (u8)(HALMAC_REG_READ_8(pHalmac_adapter, REG_MCUFW_CTRL) & ~(BIT(0))));

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_RSV_CTRL + 1);
	value8 = (u8)(value8 | BIT(0));
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_RSV_CTRL + 1, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_FUNC_EN + 1);
	value8 = (u8)(value8 | BIT(2));
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_SYS_FUNC_EN + 1, value8); /* Release MCU reset */
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "Download Finish, Reset CPU\n");

	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_WL2LTECOEX_INDIRECT_ACCESS_CTRL_V1, 0xC00F0038);

	counter = 10000;
	while (0xC078 != HALMAC_REG_READ_16(pHalmac_adapter, REG_MCUFW_CTRL)) {
		if (counter == 0) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "Check 0x80 = 0xC078 fail\n");
			if (0xFAAAAA00 == (HALMAC_REG_READ_32(pHalmac_adapter, REG_FW_DBG7) & 0xFFFFFF00))
				PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "Key fail\n");
			return HALMAC_RET_DLFW_FAIL;
		}
		counter--;
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 50);
	}

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "Check 0x80 = 0xC078 counter = %d\n", counter);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_free_dl_fw_end_flow_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u32 counter;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	PHALMAC_API pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	counter = 100;
	while (0 != HALMAC_REG_READ_8(pHalmac_adapter, REG_HMETFR + 3)) {
		counter--;
		if (0 == counter) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "[ERR]0x1CF != 0\n");
			return HALMAC_RET_DLFW_FAIL;
		}
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 50);
	}

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_HMETFR + 3, ID_INFORM_DLEMEM_RDY);

	counter = 10000;
	while (ID_INFORM_DLEMEM_RDY != HALMAC_REG_READ_8(pHalmac_adapter, REG_C2HEVT_3 + 3)) {
		counter--;
		if (0 == counter) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "[ERR]0x1AF != 0x80\n");
			return HALMAC_RET_DLFW_FAIL;
		}
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 50);
	}

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_C2HEVT_3 + 3, 0);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_pwr_seq_parser_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 cut,
	IN u8 fab,
	IN u8 intf,
	IN PHALMAC_WLAN_PWR_CFG *ppPwr_seq_cfg
)
{
	u32 seq_idx = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
	PHALMAC_WLAN_PWR_CFG pSeq_cmd;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	do {
		pSeq_cmd = &(*ppPwr_seq_cfg[seq_idx]);

		if (NULL == pSeq_cmd)
			break;

		status = halmac_pwr_sub_seq_parer_88xx(pHalmac_adapter, cut, fab, intf, pSeq_cmd);
		if (HALMAC_RET_SUCCESS != status) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "[Err]pwr sub seq parser fail, status = 0x%X!\n", status);
			return status;
		}

		seq_idx++;
	} while (1);

	return status;
}

HALMAC_RET_STATUS
halmac_pwr_sub_seq_parer_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 cut,
	IN u8 fab,
	IN u8 intf,
	IN PHALMAC_WLAN_PWR_CFG pPwr_sub_seq_cfg
)
{
	u8 value, flag;
	u8 polling_bit;
	u32 offset;
	u32 polling_count;
	static u32 poll_to_static, poll_long_static;
	VOID *pDriver_adapter = NULL;
	PHALMAC_WLAN_PWR_CFG pSub_seq_cmd;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	pSub_seq_cmd = pPwr_sub_seq_cfg;

	do {
		if ((pSub_seq_cmd->interface_msk & intf) && (pSub_seq_cmd->fab_msk & fab) && (pSub_seq_cmd->cut_msk & cut)) {
			switch (pSub_seq_cmd->cmd) {
			case HALMAC_PWR_CMD_WRITE:
				if (pSub_seq_cmd->base == HALMAC_PWR_BASEADDR_SDIO)
					offset = pSub_seq_cmd->offset | SDIO_LOCAL_OFFSET;
				else
					offset = pSub_seq_cmd->offset;

				value = HALMAC_REG_READ_8(pHalmac_adapter, offset);
				value = (u8)(value & (u8)(~(pSub_seq_cmd->msk)));
				value = (u8)(value | (u8)(pSub_seq_cmd->value & pSub_seq_cmd->msk));

				HALMAC_REG_WRITE_8(pHalmac_adapter, offset, value);
				break;
			case HALMAC_PWR_CMD_POLLING:
				polling_bit = 0;
				polling_count = HALMAC_POLLING_READY_TIMEOUT_COUNT;
				flag = 0;


				if (pSub_seq_cmd->base == HALMAC_PWR_BASEADDR_SDIO)
					offset = pSub_seq_cmd->offset | SDIO_LOCAL_OFFSET;
				else
					offset = pSub_seq_cmd->offset;

				do {
					polling_count--;
					value = HALMAC_REG_READ_8(pHalmac_adapter, offset);
					value = (u8)(value & pSub_seq_cmd->msk);

					if (value == (pSub_seq_cmd->value & pSub_seq_cmd->msk)) {
						polling_bit = 1;
					} else {
						if (0 == polling_count) {
							if (HALMAC_INTERFACE_PCIE == pHalmac_adapter->halmac_interface && 0 == flag) {
								/* For PCIE + USB package poll power bit timeout issue */
								poll_to_static++;
								PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_WARN, "[WARN]PCIE polling timeout : %d!!\n", poll_to_static);
								HALMAC_REG_WRITE_8(pHalmac_adapter, REG_SYS_PW_CTRL, HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_PW_CTRL) | BIT(3));
								HALMAC_REG_WRITE_8(pHalmac_adapter, REG_SYS_PW_CTRL, HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_PW_CTRL) & ~BIT(3));
								polling_bit = 0;
								polling_count = HALMAC_POLLING_READY_TIMEOUT_COUNT;
								flag = 1;
							} else {
								PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_ERR, "[ERR]Pwr cmd polling timeout!!\n");
								PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_ERR, "[ERR]Pwr cmd offset : %X!!\n", pSub_seq_cmd->offset);
								PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_ERR, "[ERR]Pwr cmd value : %X!!\n", pSub_seq_cmd->value);
								PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_ERR, "[ERR]Pwr cmd msk : %X!!\n", pSub_seq_cmd->msk);
								PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_ERR, "[ERR]Read offset = %X value = %X!!\n", offset, value);
								return HALMAC_RET_PWRSEQ_POLLING_FAIL;
							}
						} else {
							PLATFORM_RTL_DELAY_US(pDriver_adapter, 50);
						}
					}
				} while (!polling_bit);
				break;
			case HALMAC_PWR_CMD_DELAY:
				if (pSub_seq_cmd->value == HALMAC_PWRSEQ_DELAY_US)
					PLATFORM_RTL_DELAY_US(pDriver_adapter, pSub_seq_cmd->offset);
				else
					PLATFORM_RTL_DELAY_US(pDriver_adapter, 1000 * pSub_seq_cmd->offset);
				break;
			case HALMAC_PWR_CMD_READ:
				break;
			case HALMAC_PWR_CMD_END:
				return HALMAC_RET_SUCCESS;
			default:
				return HALMAC_RET_PWRSEQ_CMD_INCORRECT;
			}
		}
		pSub_seq_cmd++;
	} while (1);


	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_get_h2c_buff_free_space_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u32 hw_wptr, fw_rptr;
	PHALMAC_API pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	hw_wptr = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_PKT_WRITEADDR) & BIT_MASK_H2C_WR_ADDR;
	fw_rptr = HALMAC_REG_READ_32(pHalmac_adapter, REG_H2C_PKT_READADDR) & BIT_MASK_H2C_READ_ADDR;

	if (hw_wptr >= fw_rptr)
		pHalmac_adapter->h2c_buf_free_space = pHalmac_adapter->h2c_buff_size - (hw_wptr - fw_rptr);
	else
		pHalmac_adapter->h2c_buf_free_space = fw_rptr - hw_wptr;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_send_h2c_pkt_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pHal_h2c_cmd,
	IN u32 size,
	IN u8 ack
)
{
	u32 counter = 100;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	while (pHalmac_adapter->h2c_buf_free_space <= HALMAC_H2C_CMD_SIZE_UNIT_88XX) {
		halmac_get_h2c_buff_free_space_88xx(pHalmac_adapter);
		counter--;
		if (0 == counter) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "h2c free space is not enough!!\n");
			return HALMAC_RET_H2C_SPACE_FULL;
		}
	}

	/* Send TxDesc + H2C_CMD */
	if (_FALSE == PLATFORM_SEND_H2C_PKT(pDriver_adapter, pHal_h2c_cmd, size)) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "Send H2C_CMD pkt error!!\n");
		return HALMAC_RET_SEND_H2C_FAIL;
	}

	pHalmac_adapter->h2c_buf_free_space -= HALMAC_H2C_CMD_SIZE_UNIT_88XX;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "H2C free space : %d\n", pHalmac_adapter->h2c_buf_free_space);

	return status;
}

HALMAC_RET_STATUS
halmac_download_rsvd_page_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pHal_buf,
	IN u32 size
)
{
	u8 restore[3];
	u8 value8;
	u32 counter;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (0 == size) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "Rsvd page packet size is zero!!\n");
		return HALMAC_RET_ZERO_LEN_RSVD_PACKET;
	}

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_FIFOPAGE_CTRL_2 + 1);
	value8 = (u8)(value8 | BIT(7));
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FIFOPAGE_CTRL_2 + 1, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_CR + 1);
	restore[0] = value8;
	value8 = (u8)(value8 | BIT(0));
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_CR + 1, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_BCN_CTRL);
	restore[1] = value8;
	value8 = (u8)((value8 & ~(BIT(3))) | BIT(4));
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_BCN_CTRL, value8);

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_FWHW_TXQ_CTRL + 2);
	restore[2] = value8;
	value8 = (u8)(value8 & ~(BIT(6)));
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FWHW_TXQ_CTRL + 2, value8);

	if (_FALSE == PLATFORM_SEND_RSVD_PAGE(pDriver_adapter, pHal_buf, size)) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "PLATFORM_SEND_RSVD_PAGE 1 error!!\n");
		status = HALMAC_RET_DL_RSVD_PAGE_FAIL;
	}

	/* Check Bcn_Valid_Bit */
	counter = 1000;
	while (!(HALMAC_REG_READ_8(pHalmac_adapter, REG_FIFOPAGE_CTRL_2 + 1) & BIT(7))) {
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 10);
		counter--;
		if (0 == counter) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "Polling Bcn_Valid_Fail error!!\n");
			status = HALMAC_RET_POLLING_BCN_VALID_FAIL;
			break;
		}
	}

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_FIFOPAGE_CTRL_2 + 1);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FIFOPAGE_CTRL_2 + 1, (value8 | BIT(7)));

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_FWHW_TXQ_CTRL + 2, restore[2]);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_BCN_CTRL, restore[1]);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_CR + 1, restore[0]);

	return status;
}

HALMAC_RET_STATUS
halmac_set_h2c_header_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT u8 *pHal_h2c_hdr,
	IN u16 *seq,
	IN u8 ack
)
{
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_set_h2c_header_88xx!!\n");

	H2C_CMD_HEADER_SET_CATEGORY(pHal_h2c_hdr, 0x00);
	H2C_CMD_HEADER_SET_TOTAL_LEN(pHal_h2c_hdr, 16);

	PLATFORM_MUTEX_LOCK(pDriver_adapter, &(pHalmac_adapter->h2c_seq_mutex));
	H2C_CMD_HEADER_SET_SEQ_NUM(pHal_h2c_hdr, pHalmac_adapter->h2c_packet_seq);
	*seq = pHalmac_adapter->h2c_packet_seq;
	pHalmac_adapter->h2c_packet_seq++;
	PLATFORM_MUTEX_UNLOCK(pDriver_adapter, &(pHalmac_adapter->h2c_seq_mutex));

	if (_TRUE == ack)
		H2C_CMD_HEADER_SET_ACK(pHal_h2c_hdr, _TRUE);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_set_fw_offload_h2c_header_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT u8 *pHal_h2c_hdr,
	IN PHALMAC_H2C_HEADER_INFO pH2c_header_info,
	OUT u16 *pSeq_num
)
{
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_set_fw_offload_h2c_header_88xx!!\n");

	FW_OFFLOAD_H2C_SET_TOTAL_LEN(pHal_h2c_hdr, 8 + pH2c_header_info->content_size);
	FW_OFFLOAD_H2C_SET_SUB_CMD_ID(pHal_h2c_hdr, pH2c_header_info->sub_cmd_id);

	FW_OFFLOAD_H2C_SET_CATEGORY(pHal_h2c_hdr, 0x01);
	FW_OFFLOAD_H2C_SET_CMD_ID(pHal_h2c_hdr, 0xFF);

	PLATFORM_MUTEX_LOCK(pDriver_adapter, &(pHalmac_adapter->h2c_seq_mutex));
	FW_OFFLOAD_H2C_SET_SEQ_NUM(pHal_h2c_hdr, pHalmac_adapter->h2c_packet_seq);
	*pSeq_num = pHalmac_adapter->h2c_packet_seq;
	pHalmac_adapter->h2c_packet_seq++;
	PLATFORM_MUTEX_UNLOCK(pDriver_adapter, &(pHalmac_adapter->h2c_seq_mutex));

	if (_TRUE == pH2c_header_info->ack)
		FW_OFFLOAD_H2C_SET_ACK(pHal_h2c_hdr, _TRUE);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_send_h2c_set_pwr_mode_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_FWLPS_OPTION pHal_FwLps_Opt
)
{
	u8 h2c_buff[HALMAC_H2C_CMD_SIZE_88XX];
	u8 *pH2c_header, *pH2c_cmd;
	u16 seq = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_h2c_set_pwr_mode_88xx!!\n");

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pH2c_header = h2c_buff;
	pH2c_cmd = pH2c_header + HALMAC_H2C_CMD_HDR_SIZE_88XX;

	PLATFORM_RTL_MEMSET(pDriver_adapter, h2c_buff, 0x00, HALMAC_H2C_CMD_SIZE_88XX);

	SET_PWR_MODE_SET_CMD_ID(pH2c_cmd, CMD_ID_SET_PWR_MODE);
	SET_PWR_MODE_SET_CLASS(pH2c_cmd, CLASS_SET_PWR_MODE);
	SET_PWR_MODE_SET_MODE(pH2c_cmd, pHal_FwLps_Opt->mode);
	SET_PWR_MODE_SET_CLK_REQUEST(pH2c_cmd, pHal_FwLps_Opt->clk_request);
	SET_PWR_MODE_SET_RLBM(pH2c_cmd, pHal_FwLps_Opt->rlbm);
	SET_PWR_MODE_SET_SMART_PS(pH2c_cmd, pHal_FwLps_Opt->smart_ps);
	SET_PWR_MODE_SET_AWAKE_INTERVAL(pH2c_cmd, pHal_FwLps_Opt->awake_interval);
	SET_PWR_MODE_SET_B_ALL_QUEUE_UAPSD(pH2c_cmd, pHal_FwLps_Opt->all_queue_uapsd);
	SET_PWR_MODE_SET_PWR_STATE(pH2c_cmd, pHal_FwLps_Opt->pwr_state);
	SET_PWR_MODE_SET_ANT_AUTO_SWITCH(pH2c_cmd, pHal_FwLps_Opt->ant_auto_switch);
	SET_PWR_MODE_SET_PS_ALLOW_BT_HIGH_PRIORITY(pH2c_cmd, pHal_FwLps_Opt->ps_allow_bt_high_Priority);
	SET_PWR_MODE_SET_PROTECT_BCN(pH2c_cmd, pHal_FwLps_Opt->protect_bcn);
	SET_PWR_MODE_SET_SILENCE_PERIOD(pH2c_cmd, pHal_FwLps_Opt->silence_period);
	SET_PWR_MODE_SET_FAST_BT_CONNECT(pH2c_cmd, pHal_FwLps_Opt->fast_bt_connect);
	SET_PWR_MODE_SET_TWO_ANTENNA_EN(pH2c_cmd, pHal_FwLps_Opt->two_antenna_en);
	SET_PWR_MODE_SET_ADOPT_USER_SETTING(pH2c_cmd, pHal_FwLps_Opt->adopt_user_Setting);
	SET_PWR_MODE_SET_DRV_BCN_EARLY_SHIFT(pH2c_cmd, pHal_FwLps_Opt->drv_bcn_early_shift);

	halmac_set_h2c_header_88xx(pHalmac_adapter, pH2c_header, &seq, _TRUE);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, h2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_set_pwr_mode_88xx Fail = %x!!\n", status);
		return status;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_func_send_original_h2c_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *original_h2c,
	IN u16 *seq,
	IN u8 ack
)
{
	u8 H2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u8 *pH2c_header, *pH2c_cmd;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_original_h2c ==========>\n");

	pH2c_header = H2c_buff;
	pH2c_cmd = pH2c_header + HALMAC_H2C_CMD_HDR_SIZE_88XX;
	PLATFORM_RTL_MEMCPY(pDriver_adapter, pH2c_cmd, original_h2c, 8); /* Original H2C 8 byte */

	halmac_set_h2c_header_88xx(pHalmac_adapter, pH2c_header, seq, ack);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, H2c_buff, HALMAC_H2C_CMD_SIZE_88XX, ack);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_original_h2c Fail = %x!!\n", status);
		return status;
	}

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_original_h2c <==========\n");

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_media_status_rpt_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 op_mode,
	IN u8 mac_id_ind,
	IN u8 mac_id,
	IN u8 mac_id_end
)
{
	u8 H2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u8 *pH2c_header, *pH2c_cmd;
	u16 seq = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_h2c_set_pwr_mode_88xx!!\n");

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pH2c_header = H2c_buff;
	pH2c_cmd = pH2c_header + HALMAC_H2C_CMD_HDR_SIZE_88XX;

	PLATFORM_RTL_MEMSET(pDriver_adapter, H2c_buff, 0x00, HALMAC_H2C_CMD_SIZE_88XX);

	MEDIA_STATUS_RPT_SET_CMD_ID(pH2c_cmd, CMD_ID_MEDIA_STATUS_RPT);
	MEDIA_STATUS_RPT_SET_CLASS(pH2c_cmd, CLASS_MEDIA_STATUS_RPT);
	MEDIA_STATUS_RPT_SET_OP_MODE(pH2c_cmd, op_mode);
	MEDIA_STATUS_RPT_SET_MACID_IN(pH2c_cmd, mac_id_ind);
	MEDIA_STATUS_RPT_SET_MACID(pH2c_cmd, mac_id);
	MEDIA_STATUS_RPT_SET_MACID_END(pH2c_cmd, mac_id_end);

	halmac_set_h2c_header_88xx(pHalmac_adapter, pH2c_header, &seq, _TRUE);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, H2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_media_status_rpt_88xx Fail = %x!!\n", status);
		return status;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_send_h2c_update_packet_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_PACKET_ID	pkt_id,
	IN u8 *pkt,
	IN u32 pkt_size
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u16 h2c_seq_mum = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS ret_status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));

	ret_status = halmac_download_rsvd_page_88xx(pHalmac_adapter, pkt, pkt_size);

	if (HALMAC_RET_SUCCESS != ret_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_download_rsvd_page_88xx Fail = %x!!\n", ret_status);
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));
		return ret_status;
	}

	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));

	UPDATE_PACKET_SET_SIZE(pH2c_buff, pkt_size + pHalmac_adapter->hw_config_info.txdesc_size);
	UPDATE_PACKET_SET_PACKET_ID(pH2c_buff, pkt_id);
	UPDATE_PACKET_SET_PACKET_LOC(pH2c_buff, pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy - pHalmac_adapter->txff_allocation.rsvd_pg_bndy);

	h2c_header_info.sub_cmd_id = SUB_CMD_ID_UPDATE_PACKET;
	h2c_header_info.content_size = 8;
	h2c_header_info.ack = _TRUE;
	halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);
	pHalmac_adapter->halmac_state.update_packet_set.seq_num = h2c_seq_mum;

	ret_status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

	if (HALMAC_RET_SUCCESS != ret_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_update_packet_88xx Fail = %x!!\n", ret_status);
		return ret_status;
	}

	return ret_status;
}

HALMAC_RET_STATUS
halmac_send_h2c_phy_parameter_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PHY_PARAMETER_INFO para_info,
	IN u8 full_fifo
)
{
	u8 drv_trigger_send = _FALSE;
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u16 h2c_seq_mum = 0;
	u32 info_size = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
	PHALMAC_CONFIG_PARA_INFO pConfig_para_info;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;
	pConfig_para_info = &(pHalmac_adapter->config_para_info);

	/* PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_h2c_phy_parameter_88xx!!\n"); */

	if (NULL == pConfig_para_info->pCfg_para_buf) {
		if (_TRUE == full_fifo)
			pConfig_para_info->para_buf_size = HALMAC_EXTRA_INFO_BUFF_SIZE_FULL_FIFO_88XX;
		else
			pConfig_para_info->para_buf_size = HALMAC_EXTRA_INFO_BUFF_SIZE_88XX;

		pConfig_para_info->pCfg_para_buf = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, pConfig_para_info->para_buf_size);

		if (NULL != pConfig_para_info->pCfg_para_buf) {
			PLATFORM_RTL_MEMSET(pDriver_adapter, pConfig_para_info->pCfg_para_buf, 0x00, pConfig_para_info->para_buf_size);
			pConfig_para_info->full_fifo_mode = full_fifo;
			pConfig_para_info->pPara_buf_w = pConfig_para_info->pCfg_para_buf;
			pConfig_para_info->para_num = 0;
			pConfig_para_info->avai_para_buf_size = pConfig_para_info->para_buf_size;
			pConfig_para_info->value_accumulation = 0;
			pConfig_para_info->offset_accumulation = 0;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "Allocate pCfg_para_buf fail!!\n");
			return HALMAC_RET_MALLOC_FAIL;
		}
	}

	if (HALMAC_RET_SUCCESS != halmac_transition_cfg_para_state_88xx(pHalmac_adapter, HALMAC_CFG_PARA_CMD_CONSTRUCT_CONSTRUCTING))
		return HALMAC_RET_ERROR_STATE;

	halmac_enqueue_para_buff_88xx(pHalmac_adapter, para_info, pConfig_para_info->pPara_buf_w, &drv_trigger_send);

	if (HALMAC_PARAMETER_CMD_END != para_info->cmd_id) {
		pConfig_para_info->para_num++;
		pConfig_para_info->pPara_buf_w += HALMAC_FW_OFFLOAD_CMD_SIZE_88XX;
		pConfig_para_info->avai_para_buf_size = pConfig_para_info->avai_para_buf_size - HALMAC_FW_OFFLOAD_CMD_SIZE_88XX;
	}

	if (((pConfig_para_info->avai_para_buf_size - pHalmac_adapter->hw_config_info.txdesc_size) > HALMAC_FW_OFFLOAD_CMD_SIZE_88XX) &&
	    (_FALSE == drv_trigger_send)) {
		return HALMAC_RET_SUCCESS;
	} else {
		if (0 == pConfig_para_info->para_num) {
			PLATFORM_RTL_FREE(pDriver_adapter, pConfig_para_info->pCfg_para_buf, pConfig_para_info->para_buf_size);
			pConfig_para_info->pCfg_para_buf = NULL;
			pConfig_para_info->pPara_buf_w = NULL;
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_WARN, "no cfg parameter element!!\n");

			if (HALMAC_RET_SUCCESS != halmac_transition_cfg_para_state_88xx(pHalmac_adapter, HALMAC_CFG_PARA_CMD_CONSTRUCT_IDLE))
				return HALMAC_RET_ERROR_STATE;

			return HALMAC_RET_SUCCESS;
		}

		if (HALMAC_RET_SUCCESS != halmac_transition_cfg_para_state_88xx(pHalmac_adapter, HALMAC_CFG_PARA_CMD_CONSTRUCT_H2C_SENT))
			return HALMAC_RET_ERROR_STATE;

		pHalmac_adapter->halmac_state.cfg_para_state_set.process_status = HALMAC_CMD_PROCESS_SENDING;

		if (_TRUE == pConfig_para_info->full_fifo_mode)
			HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, 0);
		else
			HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));

		info_size = pConfig_para_info->para_num * HALMAC_FW_OFFLOAD_CMD_SIZE_88XX;

		status = halmac_download_rsvd_page_88xx(pHalmac_adapter, (u8 *)pConfig_para_info->pCfg_para_buf, info_size);

		if (HALMAC_RET_SUCCESS != status) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_download_rsvd_page_88xx Fail!!\n");
		} else {
			halmac_gen_cfg_para_h2c_88xx(pHalmac_adapter, pH2c_buff);

			h2c_header_info.sub_cmd_id = SUB_CMD_ID_CFG_PARAMETER;
			h2c_header_info.content_size = 4;
			h2c_header_info.ack = _TRUE;
			halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);

			pHalmac_adapter->halmac_state.cfg_para_state_set.seq_num = h2c_seq_mum;

			status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

			if (HALMAC_RET_SUCCESS != status)
				PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail!!\n");

			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "config parameter time = %d\n", HALMAC_REG_READ_32(pHalmac_adapter, REG_FW_DBG6));
		}

		PLATFORM_RTL_FREE(pDriver_adapter, pConfig_para_info->pCfg_para_buf, pConfig_para_info->para_buf_size);
		pConfig_para_info->pCfg_para_buf = NULL;
		pConfig_para_info->pPara_buf_w = NULL;

		/* Restore bcn head */
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));

		if (HALMAC_RET_SUCCESS != halmac_transition_cfg_para_state_88xx(pHalmac_adapter, HALMAC_CFG_PARA_CMD_CONSTRUCT_IDLE))
			return HALMAC_RET_ERROR_STATE;
	}

	if (_FALSE == drv_trigger_send) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "Buffer full trigger sending H2C!!\n");
		return HALMAC_RET_PARA_SENDING;
	}

	return status;
}

HALMAC_RET_STATUS
halmac_enqueue_para_buff_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_PHY_PARAMETER_INFO para_info,
	IN u8 *pCurr_buff_wptr,
	OUT u8 *pEnd_cmd
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_CONFIG_PARA_INFO pConfig_para_info = &(pHalmac_adapter->config_para_info);

	*pEnd_cmd = _FALSE;

	PHY_PARAMETER_INFO_SET_LENGTH(pCurr_buff_wptr, HALMAC_FW_OFFLOAD_CMD_SIZE_88XX);
	PHY_PARAMETER_INFO_SET_IO_CMD(pCurr_buff_wptr, para_info->cmd_id);

	switch (para_info->cmd_id) {
	case HALMAC_PARAMETER_CMD_BB_W8:
	case HALMAC_PARAMETER_CMD_BB_W16:
	case HALMAC_PARAMETER_CMD_BB_W32:
	case HALMAC_PARAMETER_CMD_MAC_W8:
	case HALMAC_PARAMETER_CMD_MAC_W16:
	case HALMAC_PARAMETER_CMD_MAC_W32:
		PHY_PARAMETER_INFO_SET_IO_ADDR(pCurr_buff_wptr, para_info->content.MAC_REG_W.offset);
		PHY_PARAMETER_INFO_SET_DATA(pCurr_buff_wptr, para_info->content.MAC_REG_W.value);
		PHY_PARAMETER_INFO_SET_MASK(pCurr_buff_wptr, para_info->content.MAC_REG_W.msk);
		PHY_PARAMETER_INFO_SET_MSK_EN(pCurr_buff_wptr, para_info->content.MAC_REG_W.msk_en);
		pConfig_para_info->value_accumulation += para_info->content.MAC_REG_W.value;
		pConfig_para_info->offset_accumulation += para_info->content.MAC_REG_W.offset;
		break;
	case HALMAC_PARAMETER_CMD_RF_W:
		PHY_PARAMETER_INFO_SET_RF_ADDR(pCurr_buff_wptr, para_info->content.RF_REG_W.offset); /*In rf register, the address is only 1 byte*/
		PHY_PARAMETER_INFO_SET_RF_PATH(pCurr_buff_wptr, para_info->content.RF_REG_W.rf_path);
		PHY_PARAMETER_INFO_SET_DATA(pCurr_buff_wptr, para_info->content.RF_REG_W.value);
		PHY_PARAMETER_INFO_SET_MASK(pCurr_buff_wptr, para_info->content.RF_REG_W.msk);
		PHY_PARAMETER_INFO_SET_MSK_EN(pCurr_buff_wptr, para_info->content.RF_REG_W.msk_en);
		pConfig_para_info->value_accumulation += para_info->content.RF_REG_W.value;
		pConfig_para_info->offset_accumulation += (para_info->content.RF_REG_W.offset + (para_info->content.RF_REG_W.rf_path << 8));
		break;
	case HALMAC_PARAMETER_CMD_DELAY_US:
	case HALMAC_PARAMETER_CMD_DELAY_MS:
		PHY_PARAMETER_INFO_SET_DELAY_VALUE(pCurr_buff_wptr, para_info->content.DELAY_TIME.delay_time);
		break;
	case HALMAC_PARAMETER_CMD_END:
		*pEnd_cmd = _TRUE;
		break;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, " halmac_send_h2c_phy_parameter_88xx illegal cmd_id!!\n");
		break;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_gen_cfg_para_h2c_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pH2c_buff
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_CONFIG_PARA_INFO pConfig_para_info = &(pHalmac_adapter->config_para_info);

	CFG_PARAMETER_SET_NUM(pH2c_buff, pConfig_para_info->para_num);

	if (_TRUE == pConfig_para_info->full_fifo_mode) {
		CFG_PARAMETER_SET_INIT_CASE(pH2c_buff, 0x1);
		CFG_PARAMETER_SET_PHY_PARAMETER_LOC(pH2c_buff, 0);
	} else {
		CFG_PARAMETER_SET_INIT_CASE(pH2c_buff, 0x0);
		CFG_PARAMETER_SET_PHY_PARAMETER_LOC(pH2c_buff, pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy - pHalmac_adapter->txff_allocation.rsvd_pg_bndy);
	}

	return HALMAC_RET_SUCCESS;
}
#if 0
HALMAC_RET_STATUS
halmac_send_h2c_update_datapack_88xx(
	IN PHALMAC_ADAPTER		pHalmac_adapter,
	IN HALMAC_DATA_TYPE		halmac_data_type,
	IN PHALMAC_PHY_PARAMETER_INFO	para_info
)
{
	u8 drv_trigger_send = _FALSE;
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u8 *pCurr_buf_w;
	u16 h2c_seq_mum = 0;
	u32 info_size = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	PHALMAC_CONFIG_PARA_INFO pConfig_para_info;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;
	pConfig_para_info = &(pHalmac_adapter->config_para_info);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_h2c_phy_parameter_88xx!!\n");

	if (NULL == pConfig_para_info->pCfg_para_buf) {/*Buff null, allocate memory according to use mode*/
		/*else, only 4k reserved page is used*/
		pConfig_para_info->para_buf_size = HALMAC_EXTRA_INFO_BUFF_SIZE_88XX;
		/* pConfig_para_info->datapack_segment =0; */


		pConfig_para_info->pCfg_para_buf = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, pConfig_para_info->para_buf_size);
		if (NULL != pConfig_para_info->pCfg_para_buf) {
			/*Reset buffer parameter*/
			PLATFORM_RTL_MEMSET(pDriver_adapter, pConfig_para_info->pCfg_para_buf, 0x00, pConfig_para_info->para_buf_size);
			/* pConfig_para_info->full_fifo_mode = full_fifo; */
			pConfig_para_info->data_type = halmac_data_type;
			pConfig_para_info->pPara_buf_w = pConfig_para_info->pCfg_para_buf;
			pConfig_para_info->para_num = 0;
			pConfig_para_info->avai_para_buf_size = pConfig_para_info->para_buf_size;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "Allocate pCfg_para_buf fail!!\n");
			return HALMAC_RET_MALLOC_FAIL;
		}
	}

	pCurr_buf_w = pConfig_para_info->pPara_buf_w;

	/*Start fill buffer content*/
	PHY_PARAMETER_INFO_SET_LENGTH(pCurr_buf_w, HALMAC_FW_OFFLOAD_CMD_SIZE_88XX);/* Each element is 12 Byte */
	PHY_PARAMETER_INFO_SET_IO_CMD(pCurr_buf_w, para_info->cmd_id);

	switch (para_info->cmd_id) {
	case HALMAC_PARAMETER_CMD_BB_W8:
	case HALMAC_PARAMETER_CMD_BB_W16:
	case HALMAC_PARAMETER_CMD_BB_W32:
	case HALMAC_PARAMETER_CMD_MAC_W8:
	case HALMAC_PARAMETER_CMD_MAC_W16:
	case HALMAC_PARAMETER_CMD_MAC_W32:
		PHY_PARAMETER_INFO_SET_IO_ADDR(pCurr_buf_w, para_info->content.MAC_REG_W.offset);
		PHY_PARAMETER_INFO_SET_DATA(pCurr_buf_w, para_info->content.MAC_REG_W.value);
		PHY_PARAMETER_INFO_SET_MASK(pCurr_buf_w, para_info->content.MAC_REG_W.msk);
		PHY_PARAMETER_INFO_SET_MSK_EN(pCurr_buf_w, para_info->content.MAC_REG_W.msk_en);
		break;
	case HALMAC_PARAMETER_CMD_RF_W:
		PHY_PARAMETER_INFO_SET_RF_ADDR(pCurr_buf_w, para_info->content.RF_REG_W.offset);        /* In rf register, the address is only 1 byte */
		PHY_PARAMETER_INFO_SET_RF_PATH(pCurr_buf_w, para_info->content.RF_REG_W.rf_path);
		PHY_PARAMETER_INFO_SET_DATA(pCurr_buf_w, para_info->content.RF_REG_W.value);
		PHY_PARAMETER_INFO_SET_MASK(pCurr_buf_w, para_info->content.RF_REG_W.msk);
		PHY_PARAMETER_INFO_SET_MSK_EN(pCurr_buf_w, para_info->content.MAC_REG_W.msk_en);
		break;
	case HALMAC_PARAMETER_CMD_DELAY_US:
	case HALMAC_PARAMETER_CMD_DELAY_MS:
		PHY_PARAMETER_INFO_SET_DELAY_VALUE(pCurr_buf_w, para_info->content.DELAY_TIME.delay_time);
		break;

	case HALMAC_PARAMETER_CMD_END:
		/* PHY_PARAMETER_INFO_SET_MSK_EN(pHalmac_adapter->pPara_buf_w, 1); */
		drv_trigger_send = _TRUE;
		break;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "illegal cmd_id!!\n");
		/* return _FALSE; */
		break;
	}

	/*Update parameter buffer variable*/
	if (HALMAC_PARAMETER_CMD_END != para_info->cmd_id) {
		pConfig_para_info->para_num++;
		pConfig_para_info->pPara_buf_w += HALMAC_FW_OFFLOAD_CMD_SIZE_88XX;
		pConfig_para_info->avai_para_buf_size = pConfig_para_info->avai_para_buf_size - HALMAC_FW_OFFLOAD_CMD_SIZE_88XX;
	}

	if (((pConfig_para_info->avai_para_buf_size - pHalmac_adapter->hw_config_info.txdesc_size) > HALMAC_FW_OFFLOAD_CMD_SIZE_88XX) && (_FALSE == drv_trigger_send)) {
		/*There are still space for parameter cmd, and driver does not trigger it to send, so keep it in buffer temporarily*/
		return HALMAC_RET_SUCCESS_ENQUEUE;
	} else {
		/*There is no space or driver trigger it to send*/

		/*Update the bcn head(dma)*/
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->h2c_extra_info_boundary & BIT_MASK_BCN_HEAD_1_V1));

		/* Download to reserved page */
		info_size = pConfig_para_info->para_num * HALMAC_FW_OFFLOAD_CMD_SIZE_88XX;
		status = halmac_download_rsvd_page_88xx(pHalmac_adapter, (u8 *)pConfig_para_info->pCfg_para_buf, info_size);
		if (HALMAC_RET_SUCCESS != status) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_download_rsvd_page_88xx Fail!!\n");
		} else {/*download rsvd page ok, send h2c packet to fw*/
			/* Construct H2C Content */
			UPDATE_DATAPACK_SET_SIZE(pH2c_buff, pConfig_para_info->para_num * HALMAC_FW_OFFLOAD_CMD_SIZE_88XX);
			UPDATE_DATAPACK_SET_DATAPACK_ID(pH2c_buff, pConfig_para_info->data_type);
			UPDATE_DATAPACK_SET_DATAPACK_LOC(pH2c_buff, pHalmac_adapter->h2c_extra_info_boundary - pHalmac_adapter->Tx_boundary);
			UPDATE_DATAPACK_SET_DATAPACK_SEGMENT(pH2c_buff, pConfig_para_info->datapack_segment);
			UPDATE_DATAPACK_SET_END_SEGMENT(pH2c_buff, drv_trigger_send);

			/* Fill in H2C Header */
			h2c_header_info.sub_cmd_id = SUB_CMD_ID_UPDATE_DATAPACK;
			h2c_header_info.content_size = 8;
			h2c_header_info.ack = _TRUE;
			halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);

			/* Send H2C Cmd Packet */
			status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);
			if (HALMAC_RET_SUCCESS != status)
				PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail!!\n");
		}

		PLATFORM_RTL_FREE(pDriver_adapter, pConfig_para_info->pCfg_para_buf, pConfig_para_info->para_buf_size);
		if (_TRUE == drv_trigger_send)
			pConfig_para_info->datapack_segment = 0;
		else
			pConfig_para_info->datapack_segment++;

		pConfig_para_info->pCfg_para_buf = NULL;
		pConfig_para_info->pPara_buf_w = NULL;
		pConfig_para_info->para_num = 0;
		pConfig_para_info->avai_para_buf_size = 0;

		/*Restore Register after FW handle the H2C packet*/

		/*only set bcn head back*/
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->Tx_boundary & BIT_MASK_BCN_HEAD_1_V1));
	}

	return status;
}
#endif
HALMAC_RET_STATUS
halmac_send_h2c_run_datapack_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_DATA_TYPE halmac_data_type
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u16 h2c_seq_mum = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_h2c_run_datapack_88xx!!\n");

	RUN_DATAPACK_SET_DATAPACK_ID(pH2c_buff, halmac_data_type);

	h2c_header_info.sub_cmd_id = SUB_CMD_ID_RUN_DATAPACK;
	h2c_header_info.content_size = 4;
	h2c_header_info.ack = _TRUE;
	halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail = %x!!\n", status);
		return status;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_send_bt_coex_cmd_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pBt_buf,
	IN u32 bt_size,
	IN u8 ack
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u16 h2c_seq_mum = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_bt_coex_cmd_88xx!!\n");

	PLATFORM_RTL_MEMCPY(pDriver_adapter, pH2c_buff + 8, pBt_buf, bt_size);

	h2c_header_info.sub_cmd_id = SUB_CMD_ID_BT_COEX;
	h2c_header_info.content_size = (u16)bt_size;
	h2c_header_info.ack = ack;
	halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, ack);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail = %x!!\n", status);
		return status;
	}

	return HALMAC_RET_SUCCESS;
}


HALMAC_RET_STATUS
halmac_func_ctrl_ch_switch_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_CH_SWITCH_OPTION pCs_option
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u16 h2c_seq_mum = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
	HALMAC_CMD_PROCESS_STATUS *pProcess_status = &(pHalmac_adapter->halmac_state.scan_state_set.process_status);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_ctrl_ch_switch!!\n");

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_transition_scan_state_88xx(pHalmac_adapter, HALMAC_SCAN_CMD_CONSTRUCT_H2C_SENT))
		return HALMAC_RET_ERROR_STATE;

	*pProcess_status = HALMAC_CMD_PROCESS_SENDING;

	if (0 != pCs_option->switch_en) {
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));

		status = halmac_download_rsvd_page_88xx(pHalmac_adapter, pHalmac_adapter->ch_sw_info.ch_info_buf, pHalmac_adapter->ch_sw_info.total_size);

		if (HALMAC_RET_SUCCESS != status) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "halmac_download_rsvd_page_88xx Fail = %x!!\n", status);
			HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));
			return status;
		}

		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));
	}

	CHANNEL_SWITCH_SET_SWITCH_START(pH2c_buff, pCs_option->switch_en);
	CHANNEL_SWITCH_SET_CHANNEL_NUM(pH2c_buff, pHalmac_adapter->ch_sw_info.ch_num);
	CHANNEL_SWITCH_SET_CHANNEL_INFO_LOC(pH2c_buff, pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy - pHalmac_adapter->txff_allocation.rsvd_pg_bndy);
	CHANNEL_SWITCH_SET_DEST_CH_EN(pH2c_buff, pCs_option->dest_ch_en);
	CHANNEL_SWITCH_SET_DEST_CH(pH2c_buff, pCs_option->dest_ch);
	CHANNEL_SWITCH_SET_PRI_CH_IDX(pH2c_buff, pCs_option->dest_pri_ch_idx);
	CHANNEL_SWITCH_SET_ABSOLUTE_TIME(pH2c_buff, pCs_option->absolute_time_en);
	CHANNEL_SWITCH_SET_TSF_LOW(pH2c_buff, pCs_option->tsf_low);
	CHANNEL_SWITCH_SET_PERIODIC_OPTION(pH2c_buff, pCs_option->periodic_option);
	CHANNEL_SWITCH_SET_NORMAL_CYCLE(pH2c_buff, pCs_option->normal_cycle);
	CHANNEL_SWITCH_SET_NORMAL_PERIOD(pH2c_buff, pCs_option->normal_period);
	CHANNEL_SWITCH_SET_SLOW_PERIOD(pH2c_buff, pCs_option->phase_2_period);
	CHANNEL_SWITCH_SET_CHANNEL_INFO_SIZE(pH2c_buff, pHalmac_adapter->ch_sw_info.total_size);

	h2c_header_info.sub_cmd_id = SUB_CMD_ID_CHANNEL_SWITCH;
	h2c_header_info.content_size = 20;
	h2c_header_info.ack = _TRUE;
	halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);
	pHalmac_adapter->halmac_state.scan_state_set.seq_num = h2c_seq_mum;

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

	if (HALMAC_RET_SUCCESS != status)
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail = %x!!\n", status);

	PLATFORM_RTL_FREE(pDriver_adapter, pHalmac_adapter->ch_sw_info.ch_info_buf, pHalmac_adapter->ch_sw_info.buf_size);
	pHalmac_adapter->ch_sw_info.ch_info_buf = NULL;
	pHalmac_adapter->ch_sw_info.ch_info_buf_w = NULL;
	pHalmac_adapter->ch_sw_info.extra_info_en = 0;
	pHalmac_adapter->ch_sw_info.buf_size = 0;
	pHalmac_adapter->ch_sw_info.avai_buf_size = 0;
	pHalmac_adapter->ch_sw_info.total_size = 0;
	pHalmac_adapter->ch_sw_info.ch_num = 0;

	if (HALMAC_RET_SUCCESS != halmac_transition_scan_state_88xx(pHalmac_adapter, HALMAC_SCAN_CMD_CONSTRUCT_IDLE))
		return HALMAC_RET_ERROR_STATE;

	return status;
}

HALMAC_RET_STATUS
halmac_func_send_general_info_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_GENERAL_INFO pGeneral_info
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u16 h2c_seq_mum = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_general_info!!\n");

	GENERAL_INFO_SET_REF_TYPE(pH2c_buff, pGeneral_info->rfe_type);
	GENERAL_INFO_SET_RF_TYPE(pH2c_buff, pGeneral_info->rf_type);
	GENERAL_INFO_SET_FW_TX_BOUNDARY(pH2c_buff, pHalmac_adapter->txff_allocation.rsvd_fw_txbuff_pg_bndy - pHalmac_adapter->txff_allocation.rsvd_pg_bndy);

	h2c_header_info.sub_cmd_id = SUB_CMD_ID_GENERAL_INFO;
	h2c_header_info.content_size = 4;
	h2c_header_info.ack = _FALSE;
	halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

	if (HALMAC_RET_SUCCESS != status)
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail = %x!!\n", status);

	return status;
}

HALMAC_RET_STATUS
halmac_send_h2c_update_bcn_parse_info_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_BCN_IE_INFO pBcn_ie_info
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u16 h2c_seq_mum = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_H2C_HEADER_INFO h2c_header_info;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_h2c_update_bcn_parse_info_88xx!!\n");

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	UPDATE_BEACON_PARSING_INFO_SET_FUNC_EN(pH2c_buff, pBcn_ie_info->func_en);
	UPDATE_BEACON_PARSING_INFO_SET_SIZE_TH(pH2c_buff, pBcn_ie_info->size_th);
	UPDATE_BEACON_PARSING_INFO_SET_TIMEOUT(pH2c_buff, pBcn_ie_info->timeout);

	UPDATE_BEACON_PARSING_INFO_SET_IE_ID_BMP_0(pH2c_buff, (u32)(pBcn_ie_info->ie_bmp[0]));
	UPDATE_BEACON_PARSING_INFO_SET_IE_ID_BMP_1(pH2c_buff, (u32)(pBcn_ie_info->ie_bmp[1]));
	UPDATE_BEACON_PARSING_INFO_SET_IE_ID_BMP_2(pH2c_buff, (u32)(pBcn_ie_info->ie_bmp[2]));
	UPDATE_BEACON_PARSING_INFO_SET_IE_ID_BMP_3(pH2c_buff, (u32)(pBcn_ie_info->ie_bmp[3]));
	UPDATE_BEACON_PARSING_INFO_SET_IE_ID_BMP_4(pH2c_buff, (u32)(pBcn_ie_info->ie_bmp[4]));

	h2c_header_info.sub_cmd_id = SUB_CMD_ID_UPDATE_BEACON_PARSING_INFO;
	h2c_header_info.content_size = 24;
	h2c_header_info.ack = _TRUE;
	halmac_set_fw_offload_h2c_header_88xx(pHalmac_adapter, pH2c_buff, &h2c_header_info, &h2c_seq_mum);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _TRUE);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail =%x !!\n", status);
		return status;
	}

	return status;
}

HALMAC_RET_STATUS
halmac_send_h2c_ps_tuning_para_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u8 pH2c_buff[HALMAC_H2C_CMD_SIZE_88XX] = { 0 };
	u8 *pH2c_header, *pH2c_cmd;
	u16 seq = 0;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_send_h2c_ps_tuning_para_88xx!!\n");

	pH2c_header = pH2c_buff;
	pH2c_cmd = pH2c_header + HALMAC_H2C_CMD_HDR_SIZE_88XX;

	halmac_set_h2c_header_88xx(pHalmac_adapter, pH2c_header, &seq, _FALSE);

	status = halmac_send_h2c_pkt_88xx(pHalmac_adapter, pH2c_buff, HALMAC_H2C_CMD_SIZE_88XX, _FALSE);

	if (HALMAC_RET_SUCCESS != status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "halmac_send_h2c_pkt_88xx Fail = %x!!\n", status);
		return status;
	}

	return status;
}

HALMAC_RET_STATUS
halmac_parse_c2h_packet_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *halmac_buf,
	IN u32 halmac_size
)
{
	u8 c2h_cmd, c2h_sub_cmd_id;
	u8 *pC2h_buf = halmac_buf + pHalmac_adapter->hw_config_info.rxdesc_size;
	u32 c2h_size = halmac_size - pHalmac_adapter->hw_config_info.rxdesc_size;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	/* PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "halmac_parse_c2h_packet_88xx!!\n"); */

	c2h_cmd = (u8)C2H_HDR_GET_CMD_ID(pC2h_buf);

	/* FW offload C2H cmd is 0xFF */
	if (0xFF != c2h_cmd) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "C2H_PKT not for FwOffloadC2HFormat!!\n");
		return HALMAC_RET_C2H_NOT_HANDLED;
	}

	/* Get C2H sub cmd ID */
	c2h_sub_cmd_id = (u8)C2H_HDR_GET_C2H_SUB_CMD_ID(pC2h_buf);

	switch (c2h_sub_cmd_id) {
	case C2H_SUB_CMD_ID_C2H_DBG:
		status = halmac_parse_c2h_debug_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
		break;
	case C2H_SUB_CMD_ID_H2C_ACK_HDR:
		status = halmac_parse_h2c_ack_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
		break;
	case C2H_SUB_CMD_ID_BT_COEX_INFO:
		status = HALMAC_RET_C2H_NOT_HANDLED;
		break;
	case C2H_SUB_CMD_ID_SCAN_STATUS_RPT:
		status = halmac_parse_scan_status_rpt_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
		break;
	case C2H_SUB_CMD_ID_PSD_DATA:
		status = halmac_parse_psd_data_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
		break;

	case C2H_SUB_CMD_ID_EFUSE_DATA:
		status = halmac_parse_efuse_data_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
		break;
	default:
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_WARN, "c2h_sub_cmd_id switch case out of boundary!!\n");
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_WARN, "[ERR]c2h pkt : %.8X %.8X!!\n", *(u32 *)pC2h_buf, *(u32 *)(pC2h_buf + 4));
		status = HALMAC_RET_C2H_NOT_HANDLED;
		break;
	}

	return status;
}

HALMAC_RET_STATUS
halmac_parse_c2h_debug_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	VOID *pDriver_adapter = NULL;
	u8 *pC2h_buf_local = (u8 *)NULL;
	u32 c2h_size_local = 0;
	u8 dbg_content_length = 0;
	u8 dbg_seq_num = 0;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pC2h_buf_local = pC2h_buf;
	c2h_size_local = c2h_size;

	dbg_content_length = (u8)C2H_HDR_GET_LEN((u8 *)pC2h_buf_local);

	if (dbg_content_length > C2H_DBG_CONTENT_MAX_LENGTH) {
		return HALMAC_RET_SUCCESS;
	} else {
		*(pC2h_buf_local + C2H_DBG_HEADER_LENGTH + dbg_content_length - 2) = '\n';
		dbg_seq_num = (u8)(*(pC2h_buf_local + C2H_DBG_HEADER_LENGTH));
		PLATFORM_MSG_PRINT(pDriver_adapter,  HALMAC_MSG_H2C,  HALMAC_DBG_TRACE,  "[RTKFW, SEQ=%d]: %s",  dbg_seq_num,  (char *)(pC2h_buf_local + C2H_DBG_HEADER_LENGTH + 1));
	}

	return HALMAC_RET_SUCCESS;
}


HALMAC_RET_STATUS
halmac_parse_scan_status_rpt_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_return_code;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status;

	h2c_return_code = (u8)SCAN_STATUS_RPT_GET_H2C_RETURN_CODE(pC2h_buf);
	process_status = (HALMAC_H2C_RETURN_SUCCESS == (HALMAC_H2C_RETURN_CODE)h2c_return_code) ? HALMAC_CMD_PROCESS_DONE : HALMAC_CMD_PROCESS_ERROR;

	PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_CHANNEL_SWITCH, process_status, NULL, 0);

	pHalmac_adapter->halmac_state.scan_state_set.process_status = process_status;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]scan status : %X\n", process_status);

	return HALMAC_RET_SUCCESS;
}


HALMAC_RET_STATUS
halmac_parse_psd_data_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 segment_id = 0, segment_size = 0, h2c_seq = 0;
	u16 total_size;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status;
	PHALMAC_PSD_STATE_SET pPsd_set = &(pHalmac_adapter->halmac_state.psd_set);

	h2c_seq = (u8)PSD_DATA_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]Seq num : h2c -> %d c2h -> %d\n", pPsd_set->seq_num, h2c_seq);
	if (h2c_seq != pPsd_set->seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Seq num mismactch : h2c -> %d c2h -> %d\n", pPsd_set->seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pPsd_set->process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	total_size = (u16)PSD_DATA_GET_TOTAL_SIZE(pC2h_buf);
	segment_id = (u8)PSD_DATA_GET_SEGMENT_ID(pC2h_buf);
	segment_size = (u8)PSD_DATA_GET_SEGMENT_SIZE(pC2h_buf);
	pPsd_set->data_size = total_size;

	if (NULL == pPsd_set->pData)
		pPsd_set->pData = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, pPsd_set->data_size);

	if (0 == segment_id)
		pPsd_set->segment_size = segment_size;

	PLATFORM_RTL_MEMCPY(pDriver_adapter, pPsd_set->pData + segment_id * pPsd_set->segment_size, pC2h_buf + HALMAC_C2H_DATA_OFFSET_88XX, segment_size);

	if (_FALSE == PSD_DATA_GET_END_SEGMENT(pC2h_buf))
		return HALMAC_RET_SUCCESS;

	process_status = HALMAC_CMD_PROCESS_DONE;
	pPsd_set->process_status = process_status;

	PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_PSD, process_status, pPsd_set->pData, pPsd_set->data_size);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_efuse_data_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 segment_id = 0, segment_size = 0, h2c_seq = 0;
	u8 *pEeprom_map = NULL;
	u32 eeprom_size = pHalmac_adapter->hw_config_info.eeprom_size;
	u8 h2c_return_code = 0;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status;

	h2c_seq = (u8)EFUSE_DATA_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]Seq num : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.efuse_state_set.seq_num, h2c_seq);
	if (h2c_seq != pHalmac_adapter->halmac_state.efuse_state_set.seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Seq num mismactch : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.efuse_state_set.seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pHalmac_adapter->halmac_state.efuse_state_set.process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	segment_id = (u8)EFUSE_DATA_GET_SEGMENT_ID(pC2h_buf);
	segment_size = (u8)EFUSE_DATA_GET_SEGMENT_SIZE(pC2h_buf);
	if (0 == segment_id)
		pHalmac_adapter->efuse_segment_size = segment_size;

	pEeprom_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, eeprom_size);
	if (NULL == pEeprom_map) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate local eeprom map Fail!!\n");
		return HALMAC_RET_MALLOC_FAIL;
	}
	PLATFORM_RTL_MEMSET(pDriver_adapter, pEeprom_map, 0xFF, eeprom_size);

	PLATFORM_MUTEX_LOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));
	PLATFORM_RTL_MEMCPY(pDriver_adapter, pHalmac_adapter->pHalEfuse_map + segment_id * pHalmac_adapter->efuse_segment_size,
		pC2h_buf + HALMAC_C2H_DATA_OFFSET_88XX, segment_size);
	PLATFORM_MUTEX_UNLOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));

	if (_FALSE == EFUSE_DATA_GET_END_SEGMENT(pC2h_buf)) {
		PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
		return HALMAC_RET_SUCCESS;
	}

	h2c_return_code = pHalmac_adapter->halmac_state.efuse_state_set.fw_return_code;

	if (HALMAC_H2C_RETURN_SUCCESS == (HALMAC_H2C_RETURN_CODE)h2c_return_code) {
		process_status = HALMAC_CMD_PROCESS_DONE;
		pHalmac_adapter->halmac_state.efuse_state_set.process_status = process_status;

		PLATFORM_MUTEX_LOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));
		pHalmac_adapter->hal_efuse_map_valid = _TRUE;
		PLATFORM_MUTEX_UNLOCK(pDriver_adapter, &(pHalmac_adapter->EfuseMutex));

		if (1 == pHalmac_adapter->event_trigger.physical_efuse_map) {
			PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_DUMP_PHYSICAL_EFUSE, process_status, pHalmac_adapter->pHalEfuse_map, pHalmac_adapter->hw_config_info.efuse_size);
			pHalmac_adapter->event_trigger.physical_efuse_map = 0;
		}

		if (1 == pHalmac_adapter->event_trigger.logical_efuse_map) {
			if (HALMAC_RET_SUCCESS != halmac_eeprom_parser_88xx(pHalmac_adapter, pHalmac_adapter->pHalEfuse_map, pEeprom_map)) {
				PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
				return HALMAC_RET_EEPROM_PARSING_FAIL;
			}
			PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_DUMP_LOGICAL_EFUSE, process_status, pEeprom_map, eeprom_size);
			pHalmac_adapter->event_trigger.logical_efuse_map = 0;
		}
	} else {
		process_status = HALMAC_CMD_PROCESS_ERROR;
		pHalmac_adapter->halmac_state.efuse_state_set.process_status = process_status;

		if (1 == pHalmac_adapter->event_trigger.physical_efuse_map) {
			PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_DUMP_PHYSICAL_EFUSE, process_status, &(pHalmac_adapter->halmac_state.efuse_state_set.fw_return_code), 1);
			pHalmac_adapter->event_trigger.physical_efuse_map = 0;
		}

		if (1 == pHalmac_adapter->event_trigger.logical_efuse_map) {
			if (HALMAC_RET_SUCCESS != halmac_eeprom_parser_88xx(pHalmac_adapter, pHalmac_adapter->pHalEfuse_map, pEeprom_map)) {
				PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
				return HALMAC_RET_EEPROM_PARSING_FAIL;
			}
			PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_DUMP_LOGICAL_EFUSE, process_status, &(pHalmac_adapter->halmac_state.efuse_state_set.fw_return_code), 1);
			pHalmac_adapter->event_trigger.logical_efuse_map = 0;
		}
	}

	PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_h2c_ack_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_cmd_id, h2c_sub_cmd_id;
	u8 h2c_seq = 0, offset = 0, shift = 0;
	u8 h2c_return_code;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status = HALMAC_CMD_PROCESS_UNDEFINE;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;


	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "Ack for C2H!!\n");

	h2c_return_code = (u8)H2C_ACK_HDR_GET_H2C_RETURN_CODE(pC2h_buf);
	if (HALMAC_H2C_RETURN_SUCCESS != (HALMAC_H2C_RETURN_CODE)h2c_return_code)
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "C2H_PKT Status Error!! Status = %d\n", h2c_return_code);

	h2c_cmd_id = (u8)H2C_ACK_HDR_GET_H2C_CMD_ID(pC2h_buf);

	if (0xFF != h2c_cmd_id) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "original h2c ack is not handled!!\n");
		status = HALMAC_RET_C2H_NOT_HANDLED;
	} else {
		h2c_sub_cmd_id = (u8)H2C_ACK_HDR_GET_H2C_SUB_CMD_ID(pC2h_buf);

		switch (h2c_sub_cmd_id) {
		case H2C_SUB_CMD_ID_DUMP_PHYSICAL_EFUSE_ACK:
			status = halmac_parse_h2c_ack_phy_efuse_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_CFG_PARAMETER_ACK:
			status = halmac_parse_h2c_ack_cfg_para_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_UPDATE_PACKET_ACK:
			status = halmac_parse_h2c_ack_update_packet_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_UPDATE_DATAPACK_ACK:
			status = halmac_parse_h2c_ack_update_datapack_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_RUN_DATAPACK_ACK:
			status = halmac_parse_h2c_ack_run_datapack_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_CHANNEL_SWITCH_ACK:
			status = halmac_parse_h2c_ack_channel_switch_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_IQK_ACK:
			status = halmac_parse_h2c_ack_iqk_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_POWER_TRACKING_ACK:
			status = halmac_parse_h2c_ack_power_tracking_88xx(pHalmac_adapter, pC2h_buf, c2h_size);
			break;
		case H2C_SUB_CMD_ID_PSD_ACK:
			break;
		default:
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_WARN, "h2c_sub_cmd_id switch case out of boundary!!\n");
			status = HALMAC_RET_C2H_NOT_HANDLED;
			break;
		}
	}

	return status;
}

HALMAC_RET_STATUS
halmac_parse_h2c_ack_phy_efuse_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_seq = 0;
	u8 h2c_return_code;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	h2c_seq = (u8)H2C_ACK_HDR_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]Seq num : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.efuse_state_set.seq_num, h2c_seq);
	if (h2c_seq != pHalmac_adapter->halmac_state.efuse_state_set.seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Seq num mismactch : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.efuse_state_set.seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pHalmac_adapter->halmac_state.efuse_state_set.process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	h2c_return_code = (u8)H2C_ACK_HDR_GET_H2C_RETURN_CODE(pC2h_buf);
	pHalmac_adapter->halmac_state.efuse_state_set.fw_return_code = h2c_return_code;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_h2c_ack_cfg_para_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_seq = 0;
	u8 h2c_return_code;
	u32 offset_accu = 0, value_accu = 0;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status = HALMAC_CMD_PROCESS_UNDEFINE;

	h2c_seq = (u8)H2C_ACK_HDR_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "Seq num : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.cfg_para_state_set.seq_num, h2c_seq);
	if (h2c_seq != pHalmac_adapter->halmac_state.cfg_para_state_set.seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "Seq num mismactch : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.cfg_para_state_set.seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pHalmac_adapter->halmac_state.cfg_para_state_set.process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	h2c_return_code = (u8)H2C_ACK_HDR_GET_H2C_RETURN_CODE(pC2h_buf);
	pHalmac_adapter->halmac_state.cfg_para_state_set.fw_return_code = h2c_return_code;
	offset_accu = CFG_PARAMETER_ACK_GET_OFFSET_ACCUMULATION(pC2h_buf);
	value_accu = CFG_PARAMETER_ACK_GET_VALUE_ACCUMULATION(pC2h_buf);

	if ((offset_accu != pHalmac_adapter->config_para_info.offset_accumulation) || (value_accu != pHalmac_adapter->config_para_info.value_accumulation)) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[C2H]offset_accu : %x, value_accu : %x!!\n", offset_accu, value_accu);
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[Adapter]offset_accu : %x, value_accu : %x!!\n", pHalmac_adapter->config_para_info.offset_accumulation, pHalmac_adapter->config_para_info.value_accumulation);
		process_status = HALMAC_CMD_PROCESS_ERROR;
	}

	if ((HALMAC_H2C_RETURN_SUCCESS == (HALMAC_H2C_RETURN_CODE)h2c_return_code) && (HALMAC_CMD_PROCESS_ERROR != process_status)) {
		process_status = HALMAC_CMD_PROCESS_DONE;
		pHalmac_adapter->halmac_state.cfg_para_state_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_CFG_PARA, process_status, NULL, 0);
	} else {
		process_status = HALMAC_CMD_PROCESS_ERROR;
		pHalmac_adapter->halmac_state.cfg_para_state_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_CFG_PARA, process_status, &(pHalmac_adapter->halmac_state.cfg_para_state_set.fw_return_code), 1);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_h2c_ack_update_packet_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_seq = 0;
	u8 h2c_return_code;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status;

	h2c_seq = (u8)H2C_ACK_HDR_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]Seq num : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.update_packet_set.seq_num, h2c_seq);
	if (h2c_seq != pHalmac_adapter->halmac_state.update_packet_set.seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Seq num mismactch : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.update_packet_set.seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pHalmac_adapter->halmac_state.update_packet_set.process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	h2c_return_code = (u8)H2C_ACK_HDR_GET_H2C_RETURN_CODE(pC2h_buf);
	pHalmac_adapter->halmac_state.update_packet_set.fw_return_code = h2c_return_code;

	if (HALMAC_H2C_RETURN_SUCCESS == (HALMAC_H2C_RETURN_CODE)h2c_return_code) {
		process_status = HALMAC_CMD_PROCESS_DONE;
		pHalmac_adapter->halmac_state.update_packet_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_UPDATE_PACKET, process_status, NULL, 0);
	} else {
		process_status = HALMAC_CMD_PROCESS_ERROR;
		pHalmac_adapter->halmac_state.update_packet_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_UPDATE_PACKET, process_status, &(pHalmac_adapter->halmac_state.update_packet_set.fw_return_code), 1);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_h2c_ack_update_datapack_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status = HALMAC_CMD_PROCESS_UNDEFINE;

	PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_UPDATE_DATAPACK, process_status, NULL, 0);

	return HALMAC_RET_SUCCESS;
}


HALMAC_RET_STATUS
halmac_parse_h2c_ack_run_datapack_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status = HALMAC_CMD_PROCESS_UNDEFINE;

	PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_RUN_DATAPACK, process_status, NULL, 0);

	return HALMAC_RET_SUCCESS;
}


HALMAC_RET_STATUS
halmac_parse_h2c_ack_channel_switch_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_seq = 0;
	u8 h2c_return_code;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status;

	h2c_seq = (u8)H2C_ACK_HDR_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]Seq num : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.scan_state_set.seq_num, h2c_seq);
	if (h2c_seq != pHalmac_adapter->halmac_state.scan_state_set.seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Seq num mismactch : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.scan_state_set.seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pHalmac_adapter->halmac_state.scan_state_set.process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	h2c_return_code = (u8)H2C_ACK_HDR_GET_H2C_RETURN_CODE(pC2h_buf);
	pHalmac_adapter->halmac_state.scan_state_set.fw_return_code = h2c_return_code;

	if (HALMAC_H2C_RETURN_SUCCESS == (HALMAC_H2C_RETURN_CODE)h2c_return_code) {
		process_status = HALMAC_CMD_PROCESS_RCVD;
		pHalmac_adapter->halmac_state.scan_state_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_CHANNEL_SWITCH, process_status, NULL, 0);
	} else {
		process_status = HALMAC_CMD_PROCESS_ERROR;
		pHalmac_adapter->halmac_state.scan_state_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_CHANNEL_SWITCH, process_status, &(pHalmac_adapter->halmac_state.scan_state_set.fw_return_code), 1);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_h2c_ack_iqk_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_seq = 0;
	u8 h2c_return_code;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status;

	h2c_seq = (u8)H2C_ACK_HDR_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]Seq num : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.iqk_set.seq_num, h2c_seq);
	if (h2c_seq != pHalmac_adapter->halmac_state.iqk_set.seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Seq num mismactch : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.iqk_set.seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pHalmac_adapter->halmac_state.iqk_set.process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	h2c_return_code = (u8)H2C_ACK_HDR_GET_H2C_RETURN_CODE(pC2h_buf);
	pHalmac_adapter->halmac_state.iqk_set.fw_return_code = h2c_return_code;

	if (HALMAC_H2C_RETURN_SUCCESS == (HALMAC_H2C_RETURN_CODE)h2c_return_code) {
		process_status = HALMAC_CMD_PROCESS_DONE;
		pHalmac_adapter->halmac_state.iqk_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_IQK, process_status, NULL, 0);
	} else {
		process_status = HALMAC_CMD_PROCESS_ERROR;
		pHalmac_adapter->halmac_state.iqk_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_IQK, process_status, &(pHalmac_adapter->halmac_state.iqk_set.fw_return_code), 1);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_h2c_ack_power_tracking_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 *pC2h_buf,
	IN u32 c2h_size
)
{
	u8 h2c_seq = 0;
	u8 h2c_return_code;
	VOID *pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	HALMAC_CMD_PROCESS_STATUS process_status;

	h2c_seq = (u8)H2C_ACK_HDR_GET_H2C_SEQ(pC2h_buf);
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_TRACE, "[TRACE]Seq num : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.power_tracking_set.seq_num, h2c_seq);
	if (h2c_seq != pHalmac_adapter->halmac_state.power_tracking_set.seq_num) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Seq num mismactch : h2c -> %d c2h -> %d\n", pHalmac_adapter->halmac_state.power_tracking_set.seq_num, h2c_seq);
		return HALMAC_RET_SUCCESS;
	}

	if (HALMAC_CMD_PROCESS_SENDING != pHalmac_adapter->halmac_state.power_tracking_set.process_status) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "[ERR]Not in HALMAC_CMD_PROCESS_SENDING\n");
		return HALMAC_RET_SUCCESS;
	}

	h2c_return_code = (u8)H2C_ACK_HDR_GET_H2C_RETURN_CODE(pC2h_buf);
	pHalmac_adapter->halmac_state.power_tracking_set.fw_return_code = h2c_return_code;

	if (HALMAC_H2C_RETURN_SUCCESS == (HALMAC_H2C_RETURN_CODE)h2c_return_code) {
		process_status = HALMAC_CMD_PROCESS_DONE;
		pHalmac_adapter->halmac_state.power_tracking_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_POWER_TRACKING, process_status, NULL, 0);
	} else {
		process_status = HALMAC_CMD_PROCESS_ERROR;
		pHalmac_adapter->halmac_state.power_tracking_set.process_status = process_status;
		PLATFORM_EVENT_INDICATION(pDriver_adapter, HALMAC_FEATURE_POWER_TRACKING, process_status, &(pHalmac_adapter->halmac_state.power_tracking_set.fw_return_code), 1);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_convert_to_sdio_bus_offset_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	INOUT u32 *halmac_offset
)
{
	VOID *pDriver_adapter = NULL;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	switch ((*halmac_offset) & 0xFFFF0000) {
	case WLAN_IOREG_OFFSET:
		*halmac_offset = (HALMAC_SDIO_CMD_ADDR_MAC_REG << 13) | (*halmac_offset & HALMAC_WLAN_MAC_REG_MSK);
		break;
	case SDIO_LOCAL_OFFSET:
		*halmac_offset = (HALMAC_SDIO_CMD_ADDR_SDIO_REG << 13) | (*halmac_offset & HALMAC_SDIO_LOCAL_MSK);
		break;
	default:
		*halmac_offset = 0xFFFFFFFF;
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "Unknown base address!!\n");
		return HALMAC_RET_CONVERT_SDIO_OFFSET_FAIL;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_update_sdio_free_page_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u32 free_page = 0, free_page2 = 0, free_page3 = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	PHALMAC_SDIO_FREE_SPACE pSdio_free_space;
	u8 data[12] = {0};
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_update_sdio_free_page_88xx ==========>\n");

	pSdio_free_space = &(pHalmac_adapter->sdio_free_space);
	/*need to use HALMAC_REG_READ_N, 20160316, Soar*/
	HALMAC_REG_SDIO_CMD53_READ_N(pHalmac_adapter, REG_SDIO_FREE_TXPG, 12, data);
/*
	free_page = HALMAC_REG_READ_32(pHalmac_adapter, REG_SDIO_FREE_TXPG);
	free_page2 = HALMAC_REG_READ_32(pHalmac_adapter, REG_SDIO_FREE_TXPG2);
	free_page3 = HALMAC_REG_READ_32(pHalmac_adapter, REG_SDIO_OQT_FREE_TXPG_V1);
*/
	free_page = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	free_page2 = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
	free_page3 = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);

	pSdio_free_space->high_queue_number = (u16)BIT_GET_HIQ_FREEPG_V1(free_page);
	pSdio_free_space->normal_queue_number = (u16)BIT_GET_MID_FREEPG_V1(free_page);
	pSdio_free_space->low_queue_number = (u16)BIT_GET_LOW_FREEPG_V1(free_page2);
	pSdio_free_space->public_queue_number = (u16)BIT_GET_PUB_FREEPG_V1(free_page2);
	pSdio_free_space->extra_queue_number = (u16)BIT_GET_EXQ_FREEPG_V1(free_page3);
	pSdio_free_space->ac_oqt_number = (u8)((free_page3 >> 16) & 0xFF);
	pSdio_free_space->non_ac_oqt_number = (u8)((free_page3 >> 24) & 0xFF);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_update_sdio_free_page_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_update_oqt_free_space_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	PHALMAC_SDIO_FREE_SPACE pSdio_free_space;
	u8 value;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_update_oqt_free_space_88xx ==========>\n");

	pSdio_free_space = &(pHalmac_adapter->sdio_free_space);

	pSdio_free_space->ac_oqt_number = HALMAC_REG_READ_8(pHalmac_adapter, REG_SDIO_OQT_FREE_TXPG_V1 + 2);
	/* pSdio_free_space->non_ac_oqt_number = (u8)BIT_GET_NOAC_OQT_FREEPG_V1(oqt_free_page); */
	pSdio_free_space->ac_empty = 0;
	value = HALMAC_REG_READ_8(pHalmac_adapter, REG_TXPKT_EMPTY);
	while (value > 0) {
		value = value & (value - 1);
		pSdio_free_space->ac_empty++;
	};

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_update_oqt_free_space_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

HALMAC_EFUSE_CMD_CONSTRUCT_STATE
halmac_query_efuse_curr_state_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	return pHalmac_adapter->halmac_state.efuse_state_set.efuse_cmd_construct_state;
}

HALMAC_RET_STATUS
halmac_transition_efuse_state_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_EFUSE_CMD_CONSTRUCT_STATE dest_state
)
{
	PHALMAC_EFUSE_STATE_SET pEfuse_state = &(pHalmac_adapter->halmac_state.efuse_state_set);

	if ((HALMAC_EFUSE_CMD_CONSTRUCT_IDLE != pEfuse_state->efuse_cmd_construct_state)
	    && (HALMAC_EFUSE_CMD_CONSTRUCT_BUSY != pEfuse_state->efuse_cmd_construct_state)
	    && (HALMAC_EFUSE_CMD_CONSTRUCT_H2C_SENT != pEfuse_state->efuse_cmd_construct_state))
		return HALMAC_RET_ERROR_STATE;

	if (pEfuse_state->efuse_cmd_construct_state == dest_state)
		return HALMAC_RET_ERROR_STATE;

	if (HALMAC_EFUSE_CMD_CONSTRUCT_BUSY == dest_state) {
		if (HALMAC_EFUSE_CMD_CONSTRUCT_H2C_SENT == pEfuse_state->efuse_cmd_construct_state)
			return HALMAC_RET_ERROR_STATE;
	} else if (HALMAC_EFUSE_CMD_CONSTRUCT_H2C_SENT == dest_state) {
		if (HALMAC_EFUSE_CMD_CONSTRUCT_IDLE == pEfuse_state->efuse_cmd_construct_state)
			return HALMAC_RET_ERROR_STATE;
	}

	pEfuse_state->efuse_cmd_construct_state = dest_state;

	return HALMAC_RET_SUCCESS;
}

HALMAC_CFG_PARA_CMD_CONSTRUCT_STATE
halmac_query_cfg_para_curr_state_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	return pHalmac_adapter->halmac_state.cfg_para_state_set.cfg_para_cmd_construct_state;
}

HALMAC_RET_STATUS
halmac_transition_cfg_para_state_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_CFG_PARA_CMD_CONSTRUCT_STATE dest_state
)
{
	PHALMAC_CFG_PARA_STATE_SET pCfg_para = &(pHalmac_adapter->halmac_state.cfg_para_state_set);

	if ((HALMAC_CFG_PARA_CMD_CONSTRUCT_IDLE != pCfg_para->cfg_para_cmd_construct_state) &&
	    (HALMAC_CFG_PARA_CMD_CONSTRUCT_CONSTRUCTING != pCfg_para->cfg_para_cmd_construct_state) &&
	    (HALMAC_CFG_PARA_CMD_CONSTRUCT_H2C_SENT != pCfg_para->cfg_para_cmd_construct_state))
		return HALMAC_RET_ERROR_STATE;

	if (HALMAC_CFG_PARA_CMD_CONSTRUCT_IDLE == dest_state) {
		if (HALMAC_CFG_PARA_CMD_CONSTRUCT_CONSTRUCTING == pCfg_para->cfg_para_cmd_construct_state)
			return HALMAC_RET_ERROR_STATE;
	} else if (HALMAC_CFG_PARA_CMD_CONSTRUCT_CONSTRUCTING == dest_state) {
		if (HALMAC_CFG_PARA_CMD_CONSTRUCT_H2C_SENT == pCfg_para->cfg_para_cmd_construct_state)
			return HALMAC_RET_ERROR_STATE;
	} else if (HALMAC_CFG_PARA_CMD_CONSTRUCT_H2C_SENT == dest_state) {
		if ((HALMAC_CFG_PARA_CMD_CONSTRUCT_IDLE == pCfg_para->cfg_para_cmd_construct_state)
		    || (HALMAC_CFG_PARA_CMD_CONSTRUCT_H2C_SENT == pCfg_para->cfg_para_cmd_construct_state))
			return HALMAC_RET_ERROR_STATE;
	}

	pCfg_para->cfg_para_cmd_construct_state = dest_state;

	return HALMAC_RET_SUCCESS;
}

HALMAC_SCAN_CMD_CONSTRUCT_STATE
halmac_query_scan_curr_state_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	return pHalmac_adapter->halmac_state.scan_state_set.scan_cmd_construct_state;
}

HALMAC_RET_STATUS
halmac_transition_scan_state_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_SCAN_CMD_CONSTRUCT_STATE dest_state
)
{
	PHALMAC_SCAN_STATE_SET pScan = &(pHalmac_adapter->halmac_state.scan_state_set);

	if (pScan->scan_cmd_construct_state > HALMAC_SCAN_CMD_CONSTRUCT_H2C_SENT)
		return HALMAC_RET_ERROR_STATE;

	if (HALMAC_SCAN_CMD_CONSTRUCT_IDLE == dest_state) {
		if ((HALMAC_SCAN_CMD_CONSTRUCT_BUFFER_CLEARED == pScan->scan_cmd_construct_state) ||
		    (HALMAC_SCAN_CMD_CONSTRUCT_CONSTRUCTING == pScan->scan_cmd_construct_state))
			return HALMAC_RET_ERROR_STATE;
	} else if (HALMAC_SCAN_CMD_CONSTRUCT_BUFFER_CLEARED == dest_state) {
		if (HALMAC_SCAN_CMD_CONSTRUCT_H2C_SENT == pScan->scan_cmd_construct_state)
			return HALMAC_RET_ERROR_STATE;
	} else if (HALMAC_SCAN_CMD_CONSTRUCT_CONSTRUCTING == dest_state) {
		if ((HALMAC_SCAN_CMD_CONSTRUCT_IDLE == pScan->scan_cmd_construct_state) ||
		    (HALMAC_SCAN_CMD_CONSTRUCT_H2C_SENT == pScan->scan_cmd_construct_state))
			return HALMAC_RET_ERROR_STATE;
	} else if (HALMAC_SCAN_CMD_CONSTRUCT_H2C_SENT == dest_state) {
		if ((HALMAC_SCAN_CMD_CONSTRUCT_CONSTRUCTING != pScan->scan_cmd_construct_state) &&
		    (HALMAC_SCAN_CMD_CONSTRUCT_BUFFER_CLEARED != pScan->scan_cmd_construct_state))
			return HALMAC_RET_ERROR_STATE;
	}

	pScan->scan_cmd_construct_state = dest_state;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_cfg_para_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	PHALMAC_CFG_PARA_STATE_SET pCfg_para_state_set = &(pHalmac_adapter->halmac_state.cfg_para_state_set);

	*pProcess_status = pCfg_para_state_set->process_status;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_dump_physical_efuse_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_EFUSE_STATE_SET pEfuse_state_set = &(pHalmac_adapter->halmac_state.efuse_state_set);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	*pProcess_status = pEfuse_state_set->process_status;

	if (NULL == data)
		return HALMAC_RET_NULL_POINTER;

	if (NULL == size)
		return HALMAC_RET_NULL_POINTER;

	if (HALMAC_CMD_PROCESS_DONE == *pProcess_status) {
		if (*size < pHalmac_adapter->hw_config_info.efuse_size) {
			*size = pHalmac_adapter->hw_config_info.efuse_size;
			return HALMAC_RET_BUFFER_TOO_SMALL;
		}

		*size = pHalmac_adapter->hw_config_info.efuse_size;
		PLATFORM_RTL_MEMCPY(pDriver_adapter, data, pHalmac_adapter->pHalEfuse_map, *size);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_dump_logical_efuse_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	u8 *pEeprom_map = NULL;
	u32 eeprom_size = pHalmac_adapter->hw_config_info.eeprom_size;
	VOID *pDriver_adapter = NULL;
	PHALMAC_EFUSE_STATE_SET pEfuse_state_set = &(pHalmac_adapter->halmac_state.efuse_state_set);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	*pProcess_status = pEfuse_state_set->process_status;

	if (NULL == data)
		return HALMAC_RET_NULL_POINTER;

	if (NULL == size)
		return HALMAC_RET_NULL_POINTER;

	if (HALMAC_CMD_PROCESS_DONE == *pProcess_status) {
		if (*size < eeprom_size) {
			*size = eeprom_size;
			return HALMAC_RET_BUFFER_TOO_SMALL;
		}

		*size = eeprom_size;

		pEeprom_map = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, eeprom_size);
		if (NULL == pEeprom_map) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_EFUSE, HALMAC_DBG_ERR, "[ERR]halmac allocate local eeprom map Fail!!\n");
			return HALMAC_RET_MALLOC_FAIL;
		}
		PLATFORM_RTL_MEMSET(pDriver_adapter, pEeprom_map, 0xFF, eeprom_size);

		if (HALMAC_RET_SUCCESS != halmac_eeprom_parser_88xx(pHalmac_adapter, pHalmac_adapter->pHalEfuse_map, pEeprom_map)) {
			PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
			return HALMAC_RET_EEPROM_PARSING_FAIL;
		}

		PLATFORM_RTL_MEMCPY(pDriver_adapter, data, pEeprom_map, *size);

		PLATFORM_RTL_FREE(pDriver_adapter, pEeprom_map, eeprom_size);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_channel_switch_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	PHALMAC_SCAN_STATE_SET pScan_state_set = &(pHalmac_adapter->halmac_state.scan_state_set);

	*pProcess_status = pScan_state_set->process_status;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_update_packet_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	PHALMAC_UPDATE_PACKET_STATE_SET pUpdate_packet_set = &(pHalmac_adapter->halmac_state.update_packet_set);

	*pProcess_status = pUpdate_packet_set->process_status;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_iqk_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	PHALMAC_IQK_STATE_SET pIqk_set = &(pHalmac_adapter->halmac_state.iqk_set);

	*pProcess_status = pIqk_set->process_status;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_power_tracking_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	PHALMAC_POWER_TRACKING_STATE_SET pPower_tracking_state_set = &(pHalmac_adapter->halmac_state.power_tracking_set);;

	*pProcess_status = pPower_tracking_state_set->process_status;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_query_psd_status_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	OUT HALMAC_CMD_PROCESS_STATUS *pProcess_status,
	INOUT u8 *data,
	INOUT u32 *size
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_PSD_STATE_SET pPsd_set = &(pHalmac_adapter->halmac_state.psd_set);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	*pProcess_status = pPsd_set->process_status;

	if (NULL == data)
		return HALMAC_RET_NULL_POINTER;

	if (NULL == size)
		return HALMAC_RET_NULL_POINTER;

	if (HALMAC_CMD_PROCESS_DONE == *pProcess_status) {
		if (*size < pPsd_set->data_size) {
			*size = pPsd_set->data_size;
			return HALMAC_RET_BUFFER_TOO_SMALL;
		}

		*size = pPsd_set->data_size;
		PLATFORM_RTL_MEMCPY(pDriver_adapter, data, pPsd_set->pData, *size);
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_verify_io_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u8 value8, wvalue8;
	u32 value32, value32_2, wvalue32;
	u32 halmac_offset;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS ret_status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	if (HALMAC_INTERFACE_SDIO == pHalmac_adapter->halmac_interface) {
		halmac_offset = REG_PAGE5_DUMMY;
		if (0 == (halmac_offset & 0xFFFF0000))
			halmac_offset |= WLAN_IOREG_OFFSET;

		ret_status = halmac_convert_to_sdio_bus_offset_88xx(pHalmac_adapter, &halmac_offset);

		/* Verify CMD52 R/W */
		wvalue8 = 0xab;
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset, wvalue8);

		value8 = PLATFORM_SDIO_CMD52_READ(pDriver_adapter, halmac_offset);

		if (value8 != wvalue8) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "cmd52 r/w fail write = %X read = %X\n", wvalue8, value8);
			ret_status = HALMAC_RET_PLATFORM_API_INCORRECT;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "cmd52 r/w ok\n");
		}

		/* Verify CMD53 R/W */
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset, 0xaa);
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset + 1, 0xbb);
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset + 2, 0xcc);
		PLATFORM_SDIO_CMD52_WRITE(pDriver_adapter, halmac_offset + 3, 0xdd);

		value32 = PLATFORM_SDIO_CMD53_READ_32(pDriver_adapter, halmac_offset);

		if (0xddccbbaa != value32) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "cmd53 r fail : read = %X\n");
			ret_status = HALMAC_RET_PLATFORM_API_INCORRECT;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "cmd53 r ok\n");
		}

		wvalue32 = 0x11223344;
		PLATFORM_SDIO_CMD53_WRITE_32(pDriver_adapter, halmac_offset, wvalue32);

		value32 = PLATFORM_SDIO_CMD53_READ_32(pDriver_adapter, halmac_offset);

		if (value32 != wvalue32) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "cmd53 w fail\n");
			ret_status = HALMAC_RET_PLATFORM_API_INCORRECT;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "cmd53 w ok\n");
		}

		value32 = PLATFORM_SDIO_CMD53_READ_32(pDriver_adapter, halmac_offset + 2); /* value32 should be 0x33441122 */

		wvalue32 = 0x11225566;
		PLATFORM_SDIO_CMD53_WRITE_32(pDriver_adapter, halmac_offset, wvalue32);

		value32_2 = PLATFORM_SDIO_CMD53_READ_32(pDriver_adapter, halmac_offset + 2); /* value32 should be 0x55661122 */
		if (value32_2 == value32) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "cmd52 is used for HAL_SDIO_CMD53_READ_32\n");
			ret_status = HALMAC_RET_PLATFORM_API_INCORRECT;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "cmd53 is correctly used\n");
		}
	} else {
		wvalue32 = 0x77665511;
		PLATFORM_REG_WRITE_32(pDriver_adapter, REG_PAGE5_DUMMY, wvalue32);

		value32 = PLATFORM_REG_READ_32(pDriver_adapter, REG_PAGE5_DUMMY);
		if (value32 != wvalue32) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "reg rw\n");
			ret_status = HALMAC_RET_PLATFORM_API_INCORRECT;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "reg rw ok\n");
		}
	}

	return ret_status;
}

HALMAC_RET_STATUS
halmac_verify_send_rsvd_page_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u8 *rsvd_buf = NULL;
	u8 *rsvd_page = NULL;
	u32 i;
	u32 h2c_pkt_verify_size = 64, h2c_pkt_verify_payload = 0xab;
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS ret_status = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	rsvd_buf = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, h2c_pkt_verify_size);

	if (NULL == rsvd_buf) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "[ERR]rsvd buffer malloc fail!!\n");
		return HALMAC_RET_MALLOC_FAIL;
	}

	PLATFORM_RTL_MEMSET(pDriver_adapter, rsvd_buf, (u8)h2c_pkt_verify_payload, h2c_pkt_verify_size);

	ret_status = halmac_download_rsvd_page_88xx(pHalmac_adapter, rsvd_buf, h2c_pkt_verify_size);

	if (HALMAC_RET_SUCCESS != ret_status) {
		PLATFORM_RTL_FREE(pDriver_adapter, rsvd_buf, h2c_pkt_verify_size);
		return ret_status;
	}

	rsvd_page = (u8 *)PLATFORM_RTL_MALLOC(pDriver_adapter, h2c_pkt_verify_size + pHalmac_adapter->hw_config_info.txdesc_size);

	if (NULL == rsvd_page) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "[ERR]rsvd page malloc fail!!\n");
		PLATFORM_RTL_FREE(pDriver_adapter, rsvd_buf, h2c_pkt_verify_size);
		return HALMAC_RET_MALLOC_FAIL;
	}

	PLATFORM_RTL_MEMSET(pDriver_adapter, rsvd_page, 0x00, h2c_pkt_verify_size + pHalmac_adapter->hw_config_info.txdesc_size);

	ret_status = halmac_dump_fifo_88xx(pHalmac_adapter, HAL_FIFO_SEL_RSVD_PAGE, 0, h2c_pkt_verify_size + pHalmac_adapter->hw_config_info.txdesc_size, rsvd_page);

	if (HALMAC_RET_SUCCESS != ret_status) {
		PLATFORM_RTL_FREE(pDriver_adapter, rsvd_buf, h2c_pkt_verify_size);
		PLATFORM_RTL_FREE(pDriver_adapter, rsvd_page, h2c_pkt_verify_size + pHalmac_adapter->hw_config_info.txdesc_size);
		return ret_status;
	}

	for (i = 0; i < h2c_pkt_verify_size; i++) {
		if (*(rsvd_buf + i) != *(rsvd_page + (i + pHalmac_adapter->hw_config_info.txdesc_size))) {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "[ERR]Compare RSVD page Fail\n");
			ret_status = HALMAC_RET_PLATFORM_API_INCORRECT;
		}
	}

	PLATFORM_RTL_FREE(pDriver_adapter, rsvd_buf, h2c_pkt_verify_size);
	PLATFORM_RTL_FREE(pDriver_adapter, rsvd_page, h2c_pkt_verify_size + pHalmac_adapter->hw_config_info.txdesc_size);

	return ret_status;
}

VOID
halmac_power_save_cb_88xx(
	IN VOID *CbData
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_ADAPTER pHalmac_adapter = (PHALMAC_ADAPTER)NULL;

	pHalmac_adapter = (PHALMAC_ADAPTER)CbData;
	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_power_save_cb_88xx\n");
}

HALMAC_RET_STATUS
halmac_buffer_read_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 offset,
	IN u32 size,
	IN HAL_FIFO_SEL halmac_fifo_sel,
	OUT u8 *pFifo_map
)
{
	u32 start_page, value_read;
	u32 i, counter = 0, residue;
	PHALMAC_API pHalmac_api;

	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (HAL_FIFO_SEL_RSVD_PAGE == halmac_fifo_sel)
		offset = offset + (pHalmac_adapter->txff_allocation.rsvd_pg_bndy << HALMAC_TX_PAGE_SIZE_2_POWER_88XX);

	start_page = offset >> 12;
	residue = offset & (4096 - 1);

	if ((HAL_FIFO_SEL_TX == halmac_fifo_sel) || (HAL_FIFO_SEL_RSVD_PAGE == halmac_fifo_sel))
		start_page += 0x780;
	else if (HAL_FIFO_SEL_RX == halmac_fifo_sel)
		start_page += 0x700;
	else if (HAL_FIFO_SEL_REPORT == halmac_fifo_sel)
		start_page += 0x660;
	else if (HAL_FIFO_SEL_LLT == halmac_fifo_sel)
		start_page += 0x650;
	else
		return HALMAC_RET_NOT_SUPPORT;

	value_read = HALMAC_REG_READ_16(pHalmac_adapter, REG_PKTBUF_DBG_CTRL);

	do {
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_PKTBUF_DBG_CTRL, (u16)(start_page | (value_read & 0xF000)));

		for (i = 0x8000 + residue; i <= 0x8FFF; i += 4) {
			*(u32 *)(pFifo_map + counter) = HALMAC_REG_READ_32(pHalmac_adapter, i);
			*(u32 *)(pFifo_map + counter) = rtk_le32_to_cpu(*(u32 *)(pFifo_map + counter));
			counter += 4;
			if (size == counter)
				goto HALMAC_BUF_READ_OK;
		}

		residue = 0;
		start_page++;
	} while (1);

HALMAC_BUF_READ_OK:
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_PKTBUF_DBG_CTRL, (u16)value_read);

	return HALMAC_RET_SUCCESS;
}

VOID
halmac_restore_mac_register_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_RESTORE_INFO pRestore_info,
	IN u32 restore_num
)
{
	u8 value_length;
	u32 i;
	u32 mac_register;
	u32 mac_value;
	PHALMAC_API pHalmac_api;
	PHALMAC_RESTORE_INFO pCurr_restore_info = pRestore_info;

	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	for (i = 0; i < restore_num; i++) {
		mac_register = pCurr_restore_info->mac_register;
		mac_value = pCurr_restore_info->value;
		value_length = pCurr_restore_info->length;

		if (1 == value_length)
			HALMAC_REG_WRITE_8(pHalmac_adapter, mac_register, (u8)mac_value);
		else if (2 == value_length)
			HALMAC_REG_WRITE_16(pHalmac_adapter, mac_register, (u16)mac_value);
		else if (4 == value_length)
			HALMAC_REG_WRITE_32(pHalmac_adapter, mac_register, mac_value);

		pCurr_restore_info++;
	}
}

VOID
halmac_api_record_id_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_API_ID api_id
)
{

}

HALMAC_RET_STATUS
halmac_set_usb_mode_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_USB_MODE usb_mode
)
{
	u32 usb_temp;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	HALMAC_USB_MODE current_usb_mode;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	current_usb_mode = (HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_CFG2 + 3) == 0x20) ? HALMAC_USB_MODE_U3 : HALMAC_USB_MODE_U2;

	/*check if HW supports usb2_usb3 swtich*/
	usb_temp = HALMAC_REG_READ_32(pHalmac_adapter, REG_PAD_CTRL2);
	if (_FALSE == (BIT_GET_USB23_SW_MODE_V1(usb_temp) | (usb_temp & BIT_USB3_USB2_TRANSITION))) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "HALMAC_HW_USB_MODE usb mode HW unsupport\n");
		return HALMAC_RET_USB2_3_SWITCH_UNSUPPORT;
	}

	if (usb_mode == current_usb_mode) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_H2C, HALMAC_DBG_ERR, "HALMAC_HW_USB_MODE usb mode unchange\n");
		return HALMAC_RET_USB_MODE_UNCHANGE;
	}

	usb_temp &= ~(BIT_USB23_SW_MODE_V1(0x3));

	if (HALMAC_USB_MODE_U2 == usb_mode) {
		/* usb3 to usb2 */
		HALMAC_REG_WRITE_32(pHalmac_adapter, REG_PAD_CTRL2, usb_temp | BIT_USB23_SW_MODE_V1(HALMAC_USB_MODE_U2) | BIT_RSM_EN_V1);
	} else {
		/* usb2 to usb3 */
		HALMAC_REG_WRITE_32(pHalmac_adapter, REG_PAD_CTRL2, usb_temp | BIT_USB23_SW_MODE_V1(HALMAC_USB_MODE_U3) | BIT_RSM_EN_V1);
	}

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PAD_CTRL2 + 1, 4); /* set counter down timer 4x64 ms */
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_SYS_PW_CTRL, HALMAC_REG_READ_16(pHalmac_adapter, REG_SYS_PW_CTRL) | BIT_APFM_OFFMAC);
	PLATFORM_RTL_DELAY_US(pDriver_adapter, 1000);
	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_PAD_CTRL2, HALMAC_REG_READ_32(pHalmac_adapter, REG_PAD_CTRL2) | BIT_NO_PDN_CHIPOFF_V1);

	return HALMAC_RET_SUCCESS;
}

VOID
halmac_enable_bb_rf_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 enable
)
{
	u8 value8;
	u32 value32;
	PHALMAC_API pHalmac_api;

	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (1 == enable) {
		value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_FUNC_EN);
		value8 = value8 | BIT(0) | BIT(1);
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_SYS_FUNC_EN, value8);

		value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_RF_CTRL);
		value8 = value8 | BIT(0) | BIT(1) | BIT(2);
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_RF_CTRL, value8);

		value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_WLRF1);
		value32 = value32 | BIT(24) | BIT(25) | BIT(26);
		HALMAC_REG_WRITE_32(pHalmac_adapter, REG_WLRF1, value32);
	} else {
		value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_FUNC_EN);
		value8 = value8 & (~(BIT(0) | BIT(1)));
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_SYS_FUNC_EN, value8);

		value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_RF_CTRL);
		value8 = value8 & (~(BIT(0) | BIT(1) | BIT(2)));
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_RF_CTRL, value8);

		value32 = HALMAC_REG_READ_32(pHalmac_adapter, REG_WLRF1);
		value32 = value32 & (~(BIT(24) | BIT(25) | BIT(26)));
		HALMAC_REG_WRITE_32(pHalmac_adapter, REG_WLRF1, value32);
	}
}

VOID
halmac_config_sdio_tx_page_threshold_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_TX_PAGE_THRESHOLD_INFO pThreshold_info
)
{
	PHALMAC_API pHalmac_api;

	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	switch (pThreshold_info->dma_queue_sel) {
	case HALMAC_MAP2_HQ:
			HALMAC_REG_WRITE_32(pHalmac_adapter, REG_TQPNT1, pThreshold_info->threshold);
		break;
	case HALMAC_MAP2_NQ:
			HALMAC_REG_WRITE_32(pHalmac_adapter, REG_TQPNT2, pThreshold_info->threshold);
		break;
	case HALMAC_MAP2_LQ:
			HALMAC_REG_WRITE_32(pHalmac_adapter, REG_TQPNT3, pThreshold_info->threshold);
		break;
	case HALMAC_MAP2_EXQ:
			HALMAC_REG_WRITE_32(pHalmac_adapter, REG_TQPNT4, pThreshold_info->threshold);
		break;
	default:
		break;
	}
}

VOID
halmac_config_ampdu_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_AMPDU_CONFIG pAmpdu_config
)
{
	PHALMAC_API pHalmac_api;

	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PROT_MODE_CTRL + 2, pAmpdu_config->max_agg_num);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PROT_MODE_CTRL + 3, pAmpdu_config->max_agg_num);
};

HALMAC_RET_STATUS
halmac_check_oqt_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 tx_agg_num,
	IN u8 *pHalmac_buf
)
{
	u32 counter = 10;

	/*S0, S1 are not allowed to use, 0x4E4[0] should be 0. Soar 20160323*/
	/*no need to check non_ac_oqt_number. HI and MGQ blocked will cause protocal issue before H_OQT being full*/
	switch ((HALMAC_QUEUE_SELECT)GET_TX_DESC_QSEL(pHalmac_buf)) {
	case HALMAC_QUEUE_SELECT_VO:
	case HALMAC_QUEUE_SELECT_VO_V2:
	case HALMAC_QUEUE_SELECT_VI:
	case HALMAC_QUEUE_SELECT_VI_V2:
	case HALMAC_QUEUE_SELECT_BE:
	case HALMAC_QUEUE_SELECT_BE_V2:
	case HALMAC_QUEUE_SELECT_BK:
	case HALMAC_QUEUE_SELECT_BK_V2:
		counter = 10;
		do {
			if (pHalmac_adapter->sdio_free_space.ac_empty > 0) {
				pHalmac_adapter->sdio_free_space.ac_empty -= 1;
				break;
			}

			if (pHalmac_adapter->sdio_free_space.ac_oqt_number >= tx_agg_num) {
				pHalmac_adapter->sdio_free_space.ac_oqt_number -= (u8)tx_agg_num;
				break;
			}

			halmac_update_oqt_free_space_88xx(pHalmac_adapter);

			counter--;
			if (0 == counter)
				return HALMAC_RET_OQT_NOT_ENOUGH;
		} while (1);
		break;
	default:
		break;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_rqpn_parser_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_TRX_MODE halmac_trx_mode,
	IN PHALMAC_RQPN pRqpn_table
)
{
	u8 search_flag;
	u32 i;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	search_flag = 0;
	for (i = 0; i < HALMAC_TRX_MODE_MAX; i++) {
		if (halmac_trx_mode == pRqpn_table[i].mode) {
			pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VO] = pRqpn_table[i].dma_map_vo;
			pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VI] = pRqpn_table[i].dma_map_vi;
			pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BE] = pRqpn_table[i].dma_map_be;
			pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BK] = pRqpn_table[i].dma_map_bk;
			pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_MG] = pRqpn_table[i].dma_map_mg;
			pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_HI] = pRqpn_table[i].dma_map_hi;
			search_flag = 1;
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_rqpn_parser_88xx done\n");
			break;
		}
	}

	if (0 == search_flag) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "HALMAC_RET_TRX_MODE_NOT_SUPPORT 1 switch case not support\n");
		return HALMAC_RET_TRX_MODE_NOT_SUPPORT;
	}

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_pg_num_parser_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_TRX_MODE halmac_trx_mode,
	IN PHALMAC_PG_NUM pPg_num_table
)
{
	u8 search_flag;
	u16 HPQ_num = 0, LPQ_Nnum = 0, NPQ_num = 0, GAPQ_num = 0;
	u16 EXPQ_num = 0, PUBQ_num = 0;
	u32 i = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	search_flag = 0;
	for (i = 0; i < HALMAC_TRX_MODE_MAX; i++) {
		if (halmac_trx_mode == pPg_num_table[i].mode) {
			HPQ_num = pPg_num_table[i].hq_num;
			LPQ_Nnum = pPg_num_table[i].lq_num;
			NPQ_num = pPg_num_table[i].nq_num;
			EXPQ_num = pPg_num_table[i].exq_num;
			GAPQ_num = pPg_num_table[i].gap_num;
			PUBQ_num = pHalmac_adapter->txff_allocation.ac_q_pg_num - HPQ_num - LPQ_Nnum - NPQ_num - EXPQ_num - GAPQ_num;
			search_flag = 1;
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_pg_num_parser_88xx done\n");
			break;
		}
	}

	if (0 == search_flag) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "HALMAC_RET_TRX_MODE_NOT_SUPPORT 1 switch case not support\n");
		return HALMAC_RET_TRX_MODE_NOT_SUPPORT;
	}

	if (pHalmac_adapter->txff_allocation.ac_q_pg_num < HPQ_num + LPQ_Nnum + NPQ_num + EXPQ_num + GAPQ_num)
		return HALMAC_RET_CFG_TXFIFO_PAGE_FAIL;

	pHalmac_adapter->txff_allocation.high_queue_pg_num = HPQ_num;
	pHalmac_adapter->txff_allocation.low_queue_pg_num = LPQ_Nnum;
	pHalmac_adapter->txff_allocation.normal_queue_pg_num = NPQ_num;
	pHalmac_adapter->txff_allocation.extra_queue_pg_num = EXPQ_num;
	pHalmac_adapter->txff_allocation.pub_queue_pg_num = PUBQ_num;

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_parse_intf_phy_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_INTF_PHY_PARA pIntf_phy_para,
	IN HALMAC_INTF_PHY_PLATFORM platform,
	IN HAL_INTF_PHY intf_phy
)
{
	u16 value;
	u16 curr_cut;
	u16 offset;
	u16 ip_sel;
	PHALMAC_INTF_PHY_PARA pCurr_phy_para;
	PHALMAC_API pHalmac_api;
	VOID *pDriver_adapter = NULL;
	u8 result = HALMAC_RET_SUCCESS;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	switch (pHalmac_adapter->chip_version) {
	case HALMAC_CHIP_VER_A_CUT:
		curr_cut = (u16)HALMAC_INTF_PHY_CUT_A;
		break;
	case HALMAC_CHIP_VER_B_CUT:
		curr_cut = (u16)HALMAC_INTF_PHY_CUT_B;
		break;
	case HALMAC_CHIP_VER_C_CUT:
		curr_cut = (u16)HALMAC_INTF_PHY_CUT_C;
		break;
	case HALMAC_CHIP_VER_D_CUT:
		curr_cut = (u16)HALMAC_INTF_PHY_CUT_D;
		break;
	case HALMAC_CHIP_VER_E_CUT:
		curr_cut = (u16)HALMAC_INTF_PHY_CUT_E;
		break;
	case HALMAC_CHIP_VER_F_CUT:
		curr_cut = (u16)HALMAC_INTF_PHY_CUT_F;
		break;
	case HALMAC_CHIP_VER_TEST:
		curr_cut = (u16)HALMAC_INTF_PHY_CUT_TESTCHIP;
		break;
	default:
		return HALMAC_RET_FAIL;
	}

	pCurr_phy_para = pIntf_phy_para;

	do {
		if ((pCurr_phy_para->cut & curr_cut) && (pCurr_phy_para->plaform & (u16)platform)) {
			offset =  pCurr_phy_para->offset;
			value = pCurr_phy_para->value;
			ip_sel = pCurr_phy_para->ip_sel;

			if (0xFFFF == offset)
				break;

			if (HALMAC_IP_SEL_MAC == ip_sel) {
				HALMAC_REG_WRITE_8(pHalmac_adapter, (u32)offset, (u8)value);
			} else if (HAL_INTF_PHY_USB2 == intf_phy) {

				result = halmac_usbphy_write_88xx(pHalmac_adapter, (u8)offset, value, HAL_INTF_PHY_USB2);

				if (HALMAC_RET_SUCCESS != result)
					PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_USB, HALMAC_DBG_ERR, "[ERR]Write USB2PHY fail!\n");

			} else if (HAL_INTF_PHY_USB3 == intf_phy) {

				result = halmac_usbphy_write_88xx(pHalmac_adapter, (u8)offset, value, HAL_INTF_PHY_USB3);

				if (HALMAC_RET_SUCCESS != result)
					PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_USB, HALMAC_DBG_ERR, "[ERR]Write USB3PHY fail!\n");

			} else if (HAL_INTF_PHY_PCIE_GEN1 == intf_phy) {

				if (HALMAC_IP_SEL_INTF_PHY == ip_sel)
					result = halmac_mdio_write_88xx(pHalmac_adapter, (u8)offset, value, HAL_INTF_PHY_PCIE_GEN1);
				else
					result = halmac_dbi_write8_88xx(pHalmac_adapter, offset, (u8)value);

				if (HALMAC_RET_SUCCESS != result)
					PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_MDIO, HALMAC_DBG_ERR, "[ERR]MDIO write GEN1 fail!\n");

			} else if (HAL_INTF_PHY_PCIE_GEN2 == intf_phy) {

				if (HALMAC_IP_SEL_INTF_PHY == ip_sel)
					result = halmac_mdio_write_88xx(pHalmac_adapter, (u8)offset, value, HAL_INTF_PHY_PCIE_GEN2);
				else
					result = halmac_dbi_write8_88xx(pHalmac_adapter, offset, (u8)value);

				if (HALMAC_RET_SUCCESS != result)
					PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_MDIO, HALMAC_DBG_ERR, "[ERR]MDIO write GEN2 fail!\n");
			} else {
				PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_COMMON, HALMAC_DBG_ERR, "[ERR]Parse intf phy cfg error!\n");
			}

		}
		pCurr_phy_para++;
	} while (1);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_dbi_write32_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u16 addr,
	IN u32 data
)
{
	u8 tmp_u1b = 0;
	u32 count = 0;
	u16 write_addr = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_DBI_WDATA_V1, data);

	write_addr = ((addr & 0x0ffc) | (0x000F << 12));
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_DBI_FLAG_V1, write_addr);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_TRACE, "WriteAddr = %x\n", write_addr);

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2, 0x01);
	tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2);

	count = 20;
	while (tmp_u1b && (count != 0)) {
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 10);
		tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2);
		count--;
	}

	if (tmp_u1b) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_ERR, "DBI write fail!\n");
		return HALMAC_RET_FAIL;
	} else {
		return HALMAC_RET_SUCCESS;
	}

}

u32
halmac_dbi_read32_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u16 addr
)
{
	u16 read_addr = addr & 0x0ffc;
	u8 tmp_u1b = 0;
	u32 count = 0;
	u32 ret = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api; ;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;


	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_DBI_FLAG_V1, read_addr);

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2, 0x2);
	tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2);

	count = 20;
	while (tmp_u1b && (count != 0)) {
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 10);
		tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2);
		count--;
	}

	if (tmp_u1b) {
		ret  = 0xFFFF;
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_ERR, "DBI read fail!\n");
	} else {
		ret = HALMAC_REG_READ_32(pHalmac_adapter, REG_DBI_RDATA_V1);
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_TRACE, "Read Value = %x\n", ret);
	}

	return ret;

}


HALMAC_RET_STATUS
halmac_dbi_write8_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u16 addr,
	IN u8 data
)
{
	u8 tmp_u1b = 0;
	u32 count = 0;
	u16 write_addr = 0;
	u16 remainder = addr & (4 - 1);
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api; ;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_DBI_WDATA_V1 + remainder, data);

	write_addr = ((addr & 0x0ffc) | (BIT(0) << (remainder + 12)));

	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_DBI_FLAG_V1, write_addr);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_TRACE, "WriteAddr = %x\n", write_addr);

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2, 0x01);

	tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2);

	count = 20;
	while (tmp_u1b && (count != 0)) {
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 10);
		tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1+2);
		count--;
	}

	if (tmp_u1b) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_ERR, "DBI write fail!\n");
		return HALMAC_RET_FAIL;
	} else {

		return HALMAC_RET_SUCCESS;
	}

}

u8
halmac_dbi_read8_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u16 addr
)
{
	u16 read_addr = addr & 0x0ffc;
	u8 tmp_u1b = 0;
	u32 count = 0;
	u8 ret = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api; ;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;


	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_DBI_FLAG_V1, read_addr);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2, 0x2);

	tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2);

	count = 20;
	while (tmp_u1b && (count != 0)) {
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 10);
		tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_FLAG_V1 + 2);
		count--;
	}

	if (tmp_u1b) {
		ret  = 0xFF;
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_ERR, "DBI read fail!\n");
	} else {
		ret = HALMAC_REG_READ_8(pHalmac_adapter, REG_DBI_RDATA_V1 + (addr & (4 - 1)));
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_DBI, HALMAC_DBG_TRACE, "Read Value = %x\n", ret);
	}

	return ret;

}


HALMAC_RET_STATUS
halmac_mdio_write_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 addr,
	IN u16 data,
	IN u8 speed
)
{
	u8 tmp_u1b = 0;
	u32 count = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	u8 real_addr = 0;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_MDIO_V1, data);

	/* address : 5bit */
	real_addr = (addr & 0x1F);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG, real_addr);

	if (HAL_INTF_PHY_PCIE_GEN1 == speed) {

		/* GEN1 page 0 */
		if (addr < 0x20) {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b00 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x00);

		/* GEN1 page 1 */
		} else {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b01 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x01);
		}

	} else if (HAL_INTF_PHY_PCIE_GEN2 == speed) {

		/* GEN2 page 0 */
		if (addr < 0x20) {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b10 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x02);

		/* GEN2 page 1 */
		} else {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b11 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x03);
		}
	} else {

		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_MDIO, HALMAC_DBG_ERR, "Error Speed !\n");
	}

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG, HALMAC_REG_READ_8(pHalmac_adapter, REG_PCIE_MIX_CFG) | BIT_MDIO_WFLAG_V1);

	tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_PCIE_MIX_CFG) & BIT_MDIO_WFLAG_V1 ;
	count = 20;

	while (tmp_u1b && (count != 0)) {
		PLATFORM_RTL_DELAY_US(pDriver_adapter, 10);
		tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_PCIE_MIX_CFG) & BIT_MDIO_WFLAG_V1;
		count--;
	}

	if (tmp_u1b) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_MDIO, HALMAC_DBG_ERR, "MDIO write fail!\n");
		return HALMAC_RET_FAIL;
	} else {

		return HALMAC_RET_SUCCESS;
	}

}



u16
halmac_mdio_read_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 addr,
	IN u8 speed

)
{
	u16 ret = 0;
	u8 tmp_u1b = 0;
	u32 count = 0;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	u8 real_addr = 0;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	/* address : 5bit */
	real_addr = (addr & 0x1F);
	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG, real_addr);

	if (HAL_INTF_PHY_PCIE_GEN1 == speed) {
		/* GEN1 page 0 */
		if (addr < 0x20) {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b00 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x00);

		/* GEN1 page 1 */
		} else {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b01 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x01);
		}

	} else if (HAL_INTF_PHY_PCIE_GEN2 == speed) {

		/* GEN2 page 0 */
		if (addr < 0x20) {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b10 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x02);

		/* GEN2 page 1 */
		} else {

			/* select MDIO PHY Addr : reg 0x3F8[28:24]=5'b11 */
			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG + 3, 0x03);
		}
	} else {

		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_MDIO, HALMAC_DBG_ERR, "Error Speed !\n");
	}

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_PCIE_MIX_CFG, HALMAC_REG_READ_8(pHalmac_adapter, REG_PCIE_MIX_CFG) | BIT_MDIO_RFLAG_V1);

	tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_PCIE_MIX_CFG) & BIT_MDIO_RFLAG_V1 ;
	count = 20;

	while (tmp_u1b && (count != 0)) {

		PLATFORM_RTL_DELAY_US(pDriver_adapter, 10);
		tmp_u1b = HALMAC_REG_READ_8(pHalmac_adapter, REG_PCIE_MIX_CFG) & BIT_MDIO_RFLAG_V1;
		count--;
	}

	if (tmp_u1b) {
		ret  = 0xFFFF;
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_MDIO, HALMAC_DBG_ERR, "MDIO read fail!\n");

	} else {
		ret = HALMAC_REG_READ_16(pHalmac_adapter, REG_MDIO_V1+2);
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_MDIO, HALMAC_DBG_TRACE, "Read Value = %x\n", ret);
	}

	return ret;
}

HALMAC_RET_STATUS
halmac_usbphy_write_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 addr,
	IN u16 data,
	IN u8 speed
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (HAL_INTF_PHY_USB3 == speed) {
		HALMAC_REG_WRITE_8(pHalmac_adapter, 0xff0d, (u8)data);
		HALMAC_REG_WRITE_8(pHalmac_adapter, 0xff0e, (u8)(data>>8));
		HALMAC_REG_WRITE_8(pHalmac_adapter, 0xff0c, addr|BIT(7));
	} else if (HAL_INTF_PHY_USB2 == speed) {
		HALMAC_REG_WRITE_8(pHalmac_adapter, 0xfe41, (u8)data);
		HALMAC_REG_WRITE_8(pHalmac_adapter, 0xfe40, addr);
		HALMAC_REG_WRITE_8(pHalmac_adapter, 0xfe42, 0x81);
	} else {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_USB, HALMAC_DBG_ERR, "[ERR]Error USB Speed !\n");
		return HALMAC_RET_NOT_SUPPORT;
	}

	return HALMAC_RET_SUCCESS;
}

u16
halmac_usbphy_read_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u8 addr,
	IN u8 speed
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;
	u16 value = 0;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (HAL_INTF_PHY_USB3 == speed) {
		HALMAC_REG_WRITE_8(pHalmac_adapter, 0xff0c, addr|BIT(6));
		value = (u16)(HALMAC_REG_READ_32(pHalmac_adapter, 0xff0c)>>8);
	} else if (HAL_INTF_PHY_USB2 == speed) {
		if ((addr >= 0xE0) && (addr <= 0xFF))
			addr -= 0x20;
		if ((addr >= 0xC0) && (addr <= 0xDF)) {
			HALMAC_REG_WRITE_8(pHalmac_adapter, 0xfe40, addr);
			HALMAC_REG_WRITE_8(pHalmac_adapter, 0xfe42, 0x81);
			value = HALMAC_REG_READ_8(pHalmac_adapter, 0xfe43);
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_USB, HALMAC_DBG_ERR, "[ERR]Error USB2PHY offset!\n");
			return HALMAC_RET_NOT_SUPPORT;
		}
	} else {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_USB, HALMAC_DBG_ERR, "[ERR]Error USB Speed !\n");
		return HALMAC_RET_NOT_SUPPORT;
	}

	return value;
}

