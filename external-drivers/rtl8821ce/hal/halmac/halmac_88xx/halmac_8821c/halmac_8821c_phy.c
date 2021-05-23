#include "../halmac_88xx_cfg.h"
#include "halmac_8821c_cfg.h"

/**
 * ============ip sel item list============
 * HALMAC_IP_SEL_INTF_PHY
 *	USB2 : usb2 phy, 1byte value
 *	USB3 : usb3 phy, 2byte value
 *	PCIE1 : pcie gen1 mdio, 2byte value
 *	PCIE2 : pcie gen2 mdio, 2byte value
 * HALMAC_IP_SEL_MAC
 *	USB2, USB3, PCIE1, PCIE2 : mac ip, 1byte value
 * HALMAC_IP_SEL_PCIE_DBI
 *	USB2 USB3 : none
 *	PCIE1, PCIE2 : pcie dbi, 1byte value
 */

HALMAC_INTF_PHY_PARA HALMAC_RTL8821C_USB2_PHY[] = {
	/* {offset, value, ip sel, cut mask, platform mask} */
	{0xFFFF, 0x00, HALMAC_IP_SEL_INTF_PHY, HALMAC_INTF_PHY_CUT_ALL, HALMAC_INTF_PHY_PLATFORM_ALL},
};

HALMAC_INTF_PHY_PARA HALMAC_RTL8821C_USB3_PHY[] = {
	/* {offset, value, cut mask, platform mask} */
	{0xFFFF, 0x0000, HALMAC_IP_SEL_INTF_PHY, HALMAC_INTF_PHY_CUT_ALL, HALMAC_INTF_PHY_PLATFORM_ALL},
};

HALMAC_INTF_PHY_PARA HALMAC_RTL8821C_PCIE_PHY_GEN1[] = {
	/* {offset, value, ip sel, cut mask, platform mask} */
	{0xFFFF, 0x0000, HALMAC_IP_SEL_INTF_PHY, HALMAC_INTF_PHY_CUT_ALL, HALMAC_INTF_PHY_PLATFORM_ALL},
};

HALMAC_INTF_PHY_PARA HALMAC_RTL8821C_PCIE_PHY_GEN2[] = {
	/* {offset, value, ip sel, cut mask, platform mask} */
	{0xFFFF, 0x0000, HALMAC_IP_SEL_INTF_PHY, HALMAC_INTF_PHY_CUT_ALL, HALMAC_INTF_PHY_PLATFORM_ALL},
};
