/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/bitops.h>
#include <linux/bits.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy.h>

#define RTL821x_PHYSR				0x11
#define RTL821x_PHYSR_DUPLEX			BIT(13)
#define RTL821x_PHYSR_SPEED			GENMASK(15, 14)

#define RTL821x_INER				0x12
#define RTL8211B_INER_INIT			0x6400
#define RTL8211E_INER_LINK_STATUS		BIT(10)
#define RTL8211F_INER_LINK_STATUS		BIT(4)

#define RTL821x_INSR				0x13

#define RTL821x_EXT_PAGE_SELECT			0x1e
#define RTL821x_PAGE_SELECT			0x1f

/* RTL8211E page 5 */
#define RTL8211E_EEE_LED_MODE1			0x05
#define RTL8211E_EEE_LED_MODE2			0x06

/* RTL8211E extension page 44 (0x2c) */
#define RTL8211E_LACR				0x1a
#define RLT8211E_LACR_LEDACTCTRL_SHIFT		4
#define RTL8211E_LCR				0x1c

/* RTL8211E extension page 160 (0xa0) */
#define RTL8211E_SCR				0x1a
#define RTL8211E_SCR_DISABLE_RXC_SSC		BIT(2)

#define LACR_MASK(led)				BIT(4 + (led))
#define LCR_MASK(led)				GENMASK(((led) * 4) + 2,\
							(led) * 4)

#define RTL8211F_INSR				0x1d

#define RTL8211F_TX_DELAY			BIT(8)
#define RTL8211E_TX_DELAY			BIT(1)
#define RTL8211E_RX_DELAY			BIT(2)
#define RTL8211E_MODE_MII_GMII			BIT(3)

#define RTL8201F_ISR				0x1e
#define RTL8201F_IER				0x13

#define RTL8366RB_POWER_SAVE			0x15
#define RTL8366RB_POWER_SAVE_ON			BIT(12)

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

static int rtl821x_read_page(struct phy_device *phydev)
{
	return __phy_read(phydev, RTL821x_PAGE_SELECT);
}

static int rtl821x_write_page(struct phy_device *phydev, int page)
{
	return __phy_write(phydev, RTL821x_PAGE_SELECT, page);
}

static int rtl8211x_select_ext_page(struct phy_device *phydev, int page,
				    int *oldpage)
{
	*oldpage = phy_select_page(phydev, 7);
	if (*oldpage < 0)
		return *oldpage;

	return __phy_write(phydev, RTL821x_EXT_PAGE_SELECT, page);
}

static int rtl8211x_modify_ext_paged(struct phy_device *phydev, int page,
				     u32 regnum, u16 mask, u16 set)
{
	int ret;
	int oldpage;

	ret = rtl8211x_select_ext_page(phydev, page, &oldpage);
	if (ret)
		goto err;

	ret = __phy_modify(phydev, regnum, mask, set);

err:
	return phy_restore_page(phydev, oldpage, ret);
}

static void rtl8211e_disable_eee_led_mode(struct phy_device *phydev)
{
	int oldpage;
	int err = 0;

	oldpage = phy_select_page(phydev, 5);
	if (oldpage < 0)
		goto err;

	/* write magic values to disable EEE LED mode */
	err = __phy_write(phydev, RTL8211E_EEE_LED_MODE1, 0x8b82);
	if (err)
		goto err;

	err = __phy_write(phydev, RTL8211E_EEE_LED_MODE2, 0x052b);

err:
	err = phy_restore_page(phydev, oldpage, err);
	if (err)
		phydev_warn(phydev, "failed to disable EEE LED mode: %d\n",
			    err);
}

static int rtl8211e_config_led(struct phy_device *phydev, int led,
			       struct phy_led_config *cfg)
{
	u16 lacr_bits = 0, lcr_bits = 0;
	int oldpage, ret;

	if (led < 0 || led > 2)
		return -EINVAL;

	switch (cfg->trigger.t) {
	case PHY_LED_TRIGGER_LINK:
		lcr_bits = 7 << (led * 4);
		break;

	case PHY_LED_TRIGGER_LINK_10M:
		lcr_bits = 1 << (led * 4);
		break;

	case PHY_LED_TRIGGER_LINK_100M:
		lcr_bits = 2 << (led * 4);
		break;

	case PHY_LED_TRIGGER_LINK_1G:
		lcr_bits |= 4 << (led * 4);
		break;

	case PHY_LED_TRIGGER_NONE:
		break;

	default:
		return -EOPNOTSUPP;
	}

	if (cfg->trigger.activity)
		lacr_bits = BIT(RLT8211E_LACR_LEDACTCTRL_SHIFT + led);

	rtl8211e_disable_eee_led_mode(phydev);

	ret = rtl8211x_select_ext_page(phydev, 0x2c, &oldpage);
	if (ret)
		goto err;

	ret = __phy_modify(phydev, RTL8211E_LACR, LACR_MASK(led), lacr_bits);
	if (ret)
		goto err;

	ret = __phy_modify(phydev, RTL8211E_LCR, LCR_MASK(led), lcr_bits);

err:
	return phy_restore_page(phydev, oldpage, ret);
}

static int rtl8201_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL8201F_ISR);

	return (err < 0) ? err : 0;
}

static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8211f_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read_paged(phydev, 0xa43, RTL8211F_INSR);

	return (err < 0) ? err : 0;
}

static int rtl8201_config_intr(struct phy_device *phydev)
{
	u16 val;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		val = BIT(13) | BIT(12) | BIT(11);
	else
		val = 0;

	return phy_write_paged(phydev, 0x7, RTL8201F_IER, val);
}

static int rtl8211b_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211B_INER_INIT);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211e_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211E_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211f_config_intr(struct phy_device *phydev)
{
	u16 val;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		val = RTL8211F_INER_LINK_STATUS;
	else
		val = 0;

	return phy_write_paged(phydev, 0xa42, RTL821x_INER, val);
}

static int rtl8211_config_aneg(struct phy_device *phydev)
{
	int ret;

	ret = genphy_config_aneg(phydev);
	if (ret < 0)
		return ret;

	/* Quirk was copied from vendor driver. Unfortunately it includes no
	 * description of the magic numbers.
	 */
	if (phydev->speed == SPEED_100 && phydev->autoneg == AUTONEG_DISABLE) {
		phy_write(phydev, 0x17, 0x2138);
		phy_write(phydev, 0x0e, 0x0260);
	} else {
		phy_write(phydev, 0x17, 0x2108);
		phy_write(phydev, 0x0e, 0x0000);
	}

	return 0;
}

static int rtl8211c_config_init(struct phy_device *phydev)
{
	/* RTL8211C has an issue when operating in Gigabit slave mode */
	phy_set_bits(phydev, MII_CTRL1000,
		     CTL1000_ENABLE_MASTER | CTL1000_AS_MASTER);

	return genphy_config_init(phydev);
}

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int ret;
	u16 val = 0;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	/* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
		val = RTL8211F_TX_DELAY;

	return phy_modify_paged(phydev, 0xd08, 0x11, RTL8211F_TX_DELAY, val);
}

static int rtl8211e_config_init(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	u16 val;
	int ret;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	if (of_property_read_bool(dev->of_node, "realtek,enable-ssc")) {
		ret = rtl8211x_modify_ext_paged(phydev, 0xa0, RTL8211E_SCR,
						RTL8211E_SCR_DISABLE_RXC_SSC,
						0);
		if (ret < 0)
			phydev_warn(phydev, "failed to enable SSC on RXC: %d\n",
				    ret);
	}

	/* enable TX/RX delay for rgmii-* modes, and disable them for rgmii. */
	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		val = 0;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		val = RTL8211E_TX_DELAY | RTL8211E_RX_DELAY;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		val = RTL8211E_RX_DELAY;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		val = RTL8211E_TX_DELAY;
		break;
	default: /* the rest of the modes imply leaving delays as is. */
		return 0;
	}

	/* According to a sample driver there is a 0x1c config register on the
	 * 0xa4 extension page (0x7) layout. It can be used to disable/enable
	 * the RX/TX delays otherwise controlled by RXDLY/TXDLY pins. It can
	 * also be used to customize the whole configuration register:
	 * 8:6 = PHY Address, 5:4 = Auto-Negotiation, 3 = Interface Mode Select,
	 * 2 = RX Delay, 1 = TX Delay, 0 = SELRGV (see original PHY datasheet
	 * for details).
	 */
	return rtl8211x_modify_ext_paged(phydev, 0xa4, 0x1c,
					 RTL8211E_TX_DELAY | RTL8211E_RX_DELAY,
					 val);
}

static int rtl8211b_suspend(struct phy_device *phydev)
{
	phy_write(phydev, MII_MMD_DATA, BIT(9));

	return genphy_suspend(phydev);
}

static int rtl8211b_resume(struct phy_device *phydev)
{
	phy_write(phydev, MII_MMD_DATA, 0);

	return genphy_resume(phydev);
}

static int rtl8366rb_config_init(struct phy_device *phydev)
{
	int ret;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	ret = phy_set_bits(phydev, RTL8366RB_POWER_SAVE,
			   RTL8366RB_POWER_SAVE_ON);
	if (ret) {
		dev_err(&phydev->mdio.dev,
			"error enabling power management\n");
	}

	return ret;
}

static struct phy_driver realtek_drvs[] = {
	{
		.phy_id         = 0x00008201,
		.name           = "RTL8201CP Ethernet",
		.phy_id_mask    = 0x0000ffff,
		.features       = PHY_BASIC_FEATURES,
		.flags          = PHY_HAS_INTERRUPT,
	}, {
		.phy_id		= 0x001cc816,
		.name		= "RTL8201F Fast Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_BASIC_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.ack_interrupt	= &rtl8201_ack_interrupt,
		.config_intr	= &rtl8201_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc910,
		.name		= "RTL8211 Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.config_aneg	= rtl8211_config_aneg,
		.read_mmd	= &genphy_read_mmd_unsupported,
		.write_mmd	= &genphy_write_mmd_unsupported,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc912,
		.name		= "RTL8211B Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211b_config_intr,
		.read_mmd	= &genphy_read_mmd_unsupported,
		.write_mmd	= &genphy_write_mmd_unsupported,
		.suspend	= rtl8211b_suspend,
		.resume		= rtl8211b_resume,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc913,
		.name		= "RTL8211C Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.config_init	= rtl8211c_config_init,
		.read_mmd	= &genphy_read_mmd_unsupported,
		.write_mmd	= &genphy_write_mmd_unsupported,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc914,
		.name		= "RTL8211DN Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.ack_interrupt	= rtl821x_ack_interrupt,
		.config_intr	= rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc915,
		.name		= "RTL8211E Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_init	= &rtl8211e_config_init,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211e_config_intr,
		.config_led	= &rtl8211e_config_led,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc916,
		.name		= "RTL8211F Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_init	= &rtl8211f_config_init,
		.ack_interrupt	= &rtl8211f_ack_interrupt,
		.config_intr	= &rtl8211f_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc961,
		.name		= "RTL8366RB Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_init	= &rtl8366rb_config_init,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	},
};

module_phy_driver(realtek_drvs);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc816, 0x001fffff },
	{ 0x001cc910, 0x001fffff },
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc913, 0x001fffff },
	{ 0x001cc914, 0x001fffff },
	{ 0x001cc915, 0x001fffff },
	{ 0x001cc916, 0x001fffff },
	{ 0x001cc961, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
