#ifndef _HALMAC_API_8821C_USB_H_
#define _HALMAC_API_8821C_USB_H_

#include "../../halmac_2_platform.h"
#include "../../halmac_type.h"

extern HALMAC_INTF_PHY_PARA HALMAC_RTL8821C_USB2_PHY[];
extern HALMAC_INTF_PHY_PARA HALMAC_RTL8821C_USB3_PHY[];

HALMAC_RET_STATUS
halmac_mac_power_switch_8821c_usb(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_MAC_POWER halmac_power
);

HALMAC_RET_STATUS
halmac_phy_cfg_8821c_usb(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_INTF_PHY_PLATFORM platform
);

HALMAC_RET_STATUS
halmac_interface_integration_tuning_8821c_usb(
	IN PHALMAC_ADAPTER pHalmac_adapter
);

#endif/* _HALMAC_API_8821C_USB_H_ */
