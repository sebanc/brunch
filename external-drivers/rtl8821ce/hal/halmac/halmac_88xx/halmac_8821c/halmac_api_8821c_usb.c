#include "../halmac_88xx_cfg.h"
#include "halmac_8821c_cfg.h"

/**
 * halmac_mac_power_switch_8821c_usb() - switch mac power
 * @pHalmac_adapter : the adapter of halmac
 * @halmac_power : power state
 * Author : KaiYuan Chang
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_mac_power_switch_8821c_usb(
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

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_mac_power_switch_88xx_usb halmac_power = %x ==========>\n", halmac_power);

	interface_mask = HALMAC_PWR_INTF_USB_MSK;

	value8 = HALMAC_REG_READ_8(pHalmac_adapter, REG_CR);
	if (0xEA == value8) {
		pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_OFF;
	} else {
		if (BIT(0) == (HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_STATUS1 + 1) & BIT(0)))
			pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_OFF;
		else
			pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_ON;
	}

	/*Check if power switch is needed*/
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

			HALMAC_REG_WRITE_8(pHalmac_adapter, REG_SYS_STATUS1 + 1, HALMAC_REG_READ_8(pHalmac_adapter, REG_SYS_STATUS1 + 1) & ~(BIT(0)));

			pHalmac_adapter->halmac_state.mac_power = HALMAC_MAC_POWER_ON;
			pHalmac_adapter->halmac_state.ps_state = HALMAC_PS_STATE_ACT;
		}
	}

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_mac_power_switch_88xx_usb <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_phy_cfg_8821c_usb() - phy config
 * @pHalmac_adapter : the adapter of halmac
 * Author : KaiYuan Chang
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_phy_cfg_8821c_usb(
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

	status = halmac_parse_intf_phy_88xx(pHalmac_adapter, HALMAC_RTL8821C_USB2_PHY, platform, HAL_INTF_PHY_USB2);

	if (HALMAC_RET_SUCCESS != status)
		return status;

	status = halmac_parse_intf_phy_88xx(pHalmac_adapter, HALMAC_RTL8821C_USB3_PHY, platform, HAL_INTF_PHY_USB3);

	if (HALMAC_RET_SUCCESS != status)
		return status;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_PWR, HALMAC_DBG_TRACE, "halmac_phy_cfg <==========\n");

	return HALMAC_RET_SUCCESS;
}

/**
 * halmac_interface_integration_tuning_8821c_usb() - usb interface fine tuning
 * @pHalmac_adapter : the adapter of halmac
 * Author : Ivan
 * Return : HALMAC_RET_STATUS
 * More details of status code can be found in prototype document
 */
HALMAC_RET_STATUS
halmac_interface_integration_tuning_8821c_usb(
	IN PHALMAC_ADAPTER pHalmac_adapter
)
{
	return HALMAC_RET_SUCCESS;
}

