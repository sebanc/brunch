#include "halmac_88xx_cfg.h"

/**
 * halmac_init_pcie_cfg_88xx() -  init PCIe
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_init_pcie_cfg_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	VOID *pDriver_adapter = NULL;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_INIT_PCIE_CFG);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_init_pcie_cfg_88xx ==========>\n");

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_init_pcie_cfg_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_deinit_pcie_cfg_88xx() - deinit PCIE
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_deinit_pcie_cfg_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	VOID *pDriver_adapter = NULL;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_DEINIT_PCIE_CFG);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_deinit_pcie_cfg_88xx ==========>\n");

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_deinit_pcie_cfg_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_cfg_rx_aggregation_88xx_pcie() - config rx aggregation
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_rx_agg_mode
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_cfg_rx_aggregation_88xx_pcie(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN PHALMAC_RXAGG_CFG phalmac_rxagg_cfg
)
{
	VOID *pDriver_adapter = NULL;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_CFG_RX_AGGREGATION);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_rx_aggregation_88xx_pcie ==========>\n");

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_rx_aggregation_88xx_pcie <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_8_pcie_88xx() - read 1byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u8
halmac_reg_read_8_pcie_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	return PLATFORM_REG_READ_8(pDriver_adapter, halmac_offset);
}

/**
 * halmac_reg_write_8_pcie_88xx() - write 1byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * @halmac_data : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_reg_write_8_pcie_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset,
	IN u8 halmac_data
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_REG_WRITE_8(pDriver_adapter, halmac_offset, halmac_data);

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_16_pcie_88xx() - read 2byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u16
halmac_reg_read_16_pcie_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	return PLATFORM_REG_READ_16(pDriver_adapter, halmac_offset);
}

/**
 * halmac_reg_write_16_pcie_88xx() - write 2byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * @halmac_data : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_reg_write_16_pcie_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset,
	IN u16 halmac_data
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_REG_WRITE_16(pDriver_adapter, halmac_offset, halmac_data);

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_reg_read_32_pcie_88xx() - read 4byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
u32
halmac_reg_read_32_pcie_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	return PLATFORM_REG_READ_32(pDriver_adapter, halmac_offset);
}

/**
 * halmac_reg_write_32_pcie_88xx() - write 4byte register
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_offset : register offset
 * @halmac_data : register value
 * Author : KaiYuan Chang/Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_reg_write_32_pcie_88xx(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN u32 halmac_offset,
	IN u32 halmac_data
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_REG_WRITE_32(pDriver_adapter, halmac_offset, halmac_data);

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_cfg_tx_agg_align_pcie_88xx() -config sdio bus tx agg alignment
 * @pHalmac_adapter : the adapter of halmac
 * @enable : function enable(1)/disable(0)
 * @align_size : sdio bus tx agg alignment size (2^n, n = 3~11)
 * Author : Soar Tu
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_cfg_tx_agg_align_pcie_not_support_88xx(
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


	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_pcie_not_support_88xx ==========>\n");

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_pcie_not_support_88xx not support\n");
	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "halmac_cfg_tx_agg_align_pcie_not_support_88xx <==========\n");

	return HALMAC_RET_SUCCESS;
}

