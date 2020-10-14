#include "../halmac_88xx_cfg.h"
#include "halmac_8821c_cfg.h"

#define CLKCAL_CTRL_PHYPARA		0x00
#define CLKCAL_SET_PHYPARA		0x20
#define CLKCAL_TRG_VAL_PHYPARA	0x21

static HALMAC_RET_STATUS
halmac_auto_refclk_cal_8821c_pcie(
	IN PHALMAC_ADAPTER pHalmac_adapter
);

/**
 * halmac_mac_power_switch_8821c_pcie() - switch mac power
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_power : power state
 * Author : KaiYuan Chang / Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_mac_power_switch_8821c_pcie(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_MAC_POWER	halmac_power
)
{
	u8 interface_mask;
	u8 value8;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_MAC_POWER_SWITCH);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_mac_power_switch_88xx_pcie halmac_power =  %x ==========>\n", halmac_power);
	interface_mask = HALMAC_PWR_INTF_PCI_MSK;

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_CR);
	if (0xEA == value8)
		pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_OFF;
	else
		pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_ON;

	/* Check if power switch is needed */
	if (halmac_power == HALMAC_MAC_POWER_ON && pHalmac_adapter->halmac_state.mac_power == HALMAC_MAC_POWER_ON) {
		PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_WARN, "halmac_mac_power_switch power state unchange!\n");
		return HALMAC_RET_PWR_UNCHANGE;
	} else {
		if (HALMAC_MAC_POWER_OFF == halmac_power) {
			if (HALMAC_RET_SUCCESS != halmac_pwr_seq_parser_88xx(pHalmac_adapter, HALMAC_PWR_CUT_ALL_MSK, HALMAC_PWR_FAB_TSMC_MSK,
				    interface_mask, halmac_8821c_card_disable_flow)) {
				PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_ERR, "Handle power off cmd error\n");
				return HALMAC_RET_POWER_OFF_FAIL;
			}

			pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_OFF;
			pHalmac_adapter->halmac_state.ps_state = HALMAC_PS_STATE_UNDEFINE;
			pHalmac_adapter->halmac_state.dlfw_state = HALMAC_DLFW_NONE;
			halmac_init_adapter_dynamic_para_88xx(pHalmac_adapter);
		} else {
			if (HALMAC_RET_SUCCESS != halmac_pwr_seq_parser_88xx(pHalmac_adapter, HALMAC_PWR_CUT_ALL_MSK, HALMAC_PWR_FAB_TSMC_MSK,
				    interface_mask, halmac_8821c_card_enable_flow)) {
				PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_ERR, "Handle power on cmd error\n");
				return HALMAC_RET_POWER_ON_FAIL;
			}

			pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_ON;
			pHalmac_adapter->halmac_state.ps_state = HALMAC_PS_STATE_ACT;
		}
	}

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_mac_power_switch_88xx_pcie <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_pcie_switch_8821c() - pcie gen1/gen2 switch
 * @pHalmac_adapter : the adapter of halmac
 * @pcie_cfg : gen1/gen2 selection
 * Author : KaiYuan Chang / Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_pcie_switch_8821c(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_PCIE_CFG	pcie_cfg
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_PCIE_SWITCH);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_pcie_switch_8821c ==========>\n");


	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_pcie_switch_8821c <==========\n");

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_pcie_switch_8821c_nc(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_PCIE_CFG	pcie_cfg
)
{
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_PCIE_SWITCH);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_pcie_switch_8821c_nc ==========>\n");

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_pcie_switch_8821c_nc <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_phy_cfg_8821c_pcie() - phy config
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang / Ivan Lin
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_phy_cfg_8821c_pcie(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_INTF_PHY_PLATFORM platform
)
{
	VOID *pDriver_adapter = NULL;
	HALMAC_RET_STATUS status = HALMAC_RET_SUCCESS;
	PHALMAC_API pHalmac_api;

	if (HALMAC_RET_SUCCESS != halmac_adapter_validate(pHalmac_adapter))
		return HALMAC_RET_ADAPTER_INVALID;

	if (HALMAC_RET_SUCCESS != halmac_api_validate(pHalmac_adapter))
		return HALMAC_RET_API_INVALID;

	halmac_api_record_id_88xx(pHalmac_adapter, HALMAC_API_PHY_CFG);

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_phy_cfg ==========>\n");

	status = halmac_parse_intf_phy_88xx(pHalmac_adapter, HALMAC_RTL8821C_PCIE_PHY_GEN1, platform, HAL_INTF_PHY_PCIE_GEN1);

	if (HALMAC_RET_SUCCESS != status)
		return status;

	status = halmac_parse_intf_phy_88xx(pHalmac_adapter, HALMAC_RTL8821C_PCIE_PHY_GEN2, platform, HAL_INTF_PHY_PCIE_GEN2);

	if (HALMAC_RET_SUCCESS != status)
		return status;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_phy_cfg <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_interface_integration_tuning_8821c_pcie() - pcie interface fine tuning
 * @pHalmac_adapter : the adapter of halmac
 * Author : Rick Liu
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_interface_integration_tuning_8821c_pcie(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	HALMAC_RET_STATUS status;

	status = halmac_auto_refclk_cal_8821c_pcie(pHalmac_adapter);

	return status;
}

static HALMAC_RET_STATUS
halmac_auto_refclk_cal_8821c_pcie(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	u16 read_tmp;
	u16 cal_target;
	u16 margin;
	HALMAC_RET_STATUS status;
	VOID *pDriver_adapter = NULL;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;

	/* Set reference clock div number at 0x00[7:6] */
	read_tmp = halmac_mdio_read_88xx(pHalmac_adapter, CLKCAL_CTRL_PHYPARA, HAL_INTF_PHY_PCIE_GEN1);
	/* status = halmac_mdio_write_88xx(pHalmac_adapter, CLKCAL_CTRL_PHYPARA, read_tmp & ~(BIT(6) | BIT(7)), HAL_INTF_PHY_PCIE_GEN1); */
	/* status = halmac_mdio_write_88xx(pHalmac_adapter, CLKCAL_CTRL_PHYPARA, read_tmp & ~(BIT(6)) | BIT(7), HAL_INTF_PHY_PCIE_GEN1); */
	status = halmac_mdio_write_88xx(pHalmac_adapter, CLKCAL_CTRL_PHYPARA, read_tmp | BIT(7) | BIT(6), HAL_INTF_PHY_PCIE_GEN1);
	if (HALMAC_RET_SUCCESS != status)
		return status;

	/* Set 0x00[11] to count 1T of reference clock and read target value at 0x21[11:0] */
	read_tmp = halmac_mdio_read_88xx(pHalmac_adapter, CLKCAL_CTRL_PHYPARA, HAL_INTF_PHY_PCIE_GEN1);
	status = halmac_mdio_write_88xx(pHalmac_adapter, CLKCAL_CTRL_PHYPARA, read_tmp | BIT(11), HAL_INTF_PHY_PCIE_GEN1);
	if (HALMAC_RET_SUCCESS != status)
		return status;
	PLATFORM_RTL_DELAY_US(pDriver_adapter, 22);
	cal_target = halmac_mdio_read_88xx(pHalmac_adapter, CLKCAL_TRG_VAL_PHYPARA, HAL_INTF_PHY_PCIE_GEN1);

	/* Set calibration target at 0x20[11:0] and margin at 0x20[15:12] */
	margin = 0x0003;
	status = halmac_mdio_write_88xx(pHalmac_adapter, CLKCAL_SET_PHYPARA, (cal_target & 0x0FFF) | (margin << 12), HAL_INTF_PHY_PCIE_GEN1);
	if (HALMAC_RET_SUCCESS != status)
		return status;

	/* Turn on calibration mechanium at 0x00[9] */
	status = halmac_mdio_write_88xx(pHalmac_adapter, CLKCAL_CTRL_PHYPARA, read_tmp | BIT(9), HAL_INTF_PHY_PCIE_GEN1);

	return HALMAC_RET_SUCCESS;
}
