/*
 * Copyright(c) 2017 Hauke Mehrtens <hauke@hauke-m.de>
 *
 * Backport functionality introduced in Linux 4.10.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/page_ref.h>
#include <linux/gfp.h>

#if LINUX_VERSION_IS_GEQ(4,6,0)
#if LINUX_VERSION_IS_LESS(4,7,0)
static bool ethtool_convert_link_mode_to_legacy_u32(u32 *legacy_u32,
						    const unsigned long *src)
{
	bool retval = true;

	/* TODO: following test will soon always be true */
	if (__ETHTOOL_LINK_MODE_MASK_NBITS > 32) {
		__ETHTOOL_DECLARE_LINK_MODE_MASK(ext);

		bitmap_zero(ext, __ETHTOOL_LINK_MODE_MASK_NBITS);
		bitmap_fill(ext, 32);
		bitmap_complement(ext, ext, __ETHTOOL_LINK_MODE_MASK_NBITS);
		if (bitmap_intersects(ext, src,
				      __ETHTOOL_LINK_MODE_MASK_NBITS)) {
			/* src mask goes beyond bit 31 */
			retval = false;
		}
	}
	*legacy_u32 = src[0];
	return retval;
}

static void ethtool_convert_legacy_u32_to_link_mode(unsigned long *dst,
						    u32 legacy_u32)
{
	bitmap_zero(dst, __ETHTOOL_LINK_MODE_MASK_NBITS);
	dst[0] = legacy_u32;
}
#endif

static u32 mii_get_an(struct mii_if_info *mii, u16 addr)
{
	int advert;

	advert = mii->mdio_read(mii->dev, mii->phy_id, addr);

	return mii_lpa_to_ethtool_lpa_t(advert);
}

/**
 * mii_ethtool_set_link_ksettings - set settings that are specified in @cmd
 * @mii: MII interfaces
 * @cmd: requested ethtool_link_ksettings
 *
 * Returns 0 for success, negative on error.
 */
int mii_ethtool_set_link_ksettings(struct mii_if_info *mii,
				   const struct ethtool_link_ksettings *cmd)
{
	struct net_device *dev = mii->dev;
	u32 speed = cmd->base.speed;

	if (speed != SPEED_10 &&
	    speed != SPEED_100 &&
	    speed != SPEED_1000)
		return -EINVAL;
	if (cmd->base.duplex != DUPLEX_HALF && cmd->base.duplex != DUPLEX_FULL)
		return -EINVAL;
	if (cmd->base.port != PORT_MII)
		return -EINVAL;
	if (cmd->base.phy_address != mii->phy_id)
		return -EINVAL;
	if (cmd->base.autoneg != AUTONEG_DISABLE &&
	    cmd->base.autoneg != AUTONEG_ENABLE)
		return -EINVAL;
	if ((speed == SPEED_1000) && (!mii->supports_gmii))
		return -EINVAL;

	/* ignore supported, maxtxpkt, maxrxpkt */

	if (cmd->base.autoneg == AUTONEG_ENABLE) {
		u32 bmcr, advert, tmp;
		u32 advert2 = 0, tmp2 = 0;
		u32 advertising;

		ethtool_convert_link_mode_to_legacy_u32(
			&advertising, cmd->link_modes.advertising);

		if ((advertising & (ADVERTISED_10baseT_Half |
				    ADVERTISED_10baseT_Full |
				    ADVERTISED_100baseT_Half |
				    ADVERTISED_100baseT_Full |
				    ADVERTISED_1000baseT_Half |
				    ADVERTISED_1000baseT_Full)) == 0)
			return -EINVAL;

		/* advertise only what has been requested */
		advert = mii->mdio_read(dev, mii->phy_id, MII_ADVERTISE);
		tmp = advert & ~(ADVERTISE_ALL | ADVERTISE_100BASE4);
		if (mii->supports_gmii) {
			advert2 = mii->mdio_read(dev, mii->phy_id,
						 MII_CTRL1000);
			tmp2 = advert2 &
				~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
		}
		tmp |= ethtool_adv_to_mii_adv_t(advertising);

		if (mii->supports_gmii)
			tmp2 |= ethtool_adv_to_mii_ctrl1000_t(advertising);
		if (advert != tmp) {
			mii->mdio_write(dev, mii->phy_id, MII_ADVERTISE, tmp);
			mii->advertising = tmp;
		}
		if ((mii->supports_gmii) && (advert2 != tmp2))
			mii->mdio_write(dev, mii->phy_id, MII_CTRL1000, tmp2);

		/* turn on autonegotiation, and force a renegotiate */
		bmcr = mii->mdio_read(dev, mii->phy_id, MII_BMCR);
		bmcr |= (BMCR_ANENABLE | BMCR_ANRESTART);
		mii->mdio_write(dev, mii->phy_id, MII_BMCR, bmcr);

		mii->force_media = 0;
	} else {
		u32 bmcr, tmp;

		/* turn off auto negotiation, set speed and duplexity */
		bmcr = mii->mdio_read(dev, mii->phy_id, MII_BMCR);
		tmp = bmcr & ~(BMCR_ANENABLE | BMCR_SPEED100 |
			       BMCR_SPEED1000 | BMCR_FULLDPLX);
		if (speed == SPEED_1000)
			tmp |= BMCR_SPEED1000;
		else if (speed == SPEED_100)
			tmp |= BMCR_SPEED100;
		if (cmd->base.duplex == DUPLEX_FULL) {
			tmp |= BMCR_FULLDPLX;
			mii->full_duplex = 1;
		} else {
			mii->full_duplex = 0;
		}
		if (bmcr != tmp)
			mii->mdio_write(dev, mii->phy_id, MII_BMCR, tmp);

		mii->force_media = 1;
	}
	return 0;
}
EXPORT_SYMBOL(mii_ethtool_set_link_ksettings);


/**
 * mii_ethtool_get_link_ksettings - get settings that are specified in @cmd
 * @mii: MII interface
 * @cmd: requested ethtool_link_ksettings
 *
 * The @cmd parameter is expected to have been cleared before calling
 * mii_ethtool_get_link_ksettings().
 *
 * Returns 0 for success, negative on error.
 */
int mii_ethtool_get_link_ksettings(struct mii_if_info *mii,
				   struct ethtool_link_ksettings *cmd)
{
	struct net_device *dev = mii->dev;
	u16 bmcr, bmsr, ctrl1000 = 0, stat1000 = 0;
	u32 nego, supported, advertising, lp_advertising;

	supported = (SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full |
		     SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full |
		     SUPPORTED_Autoneg | SUPPORTED_TP | SUPPORTED_MII);
	if (mii->supports_gmii)
		supported |= SUPPORTED_1000baseT_Half |
			SUPPORTED_1000baseT_Full;

	/* only supports twisted-pair */
	cmd->base.port = PORT_MII;

	/* this isn't fully supported at higher layers */
	cmd->base.phy_address = mii->phy_id;
	cmd->base.mdio_support = ETH_MDIO_SUPPORTS_C22;

	advertising = ADVERTISED_TP | ADVERTISED_MII;

	bmcr = mii->mdio_read(dev, mii->phy_id, MII_BMCR);
	bmsr = mii->mdio_read(dev, mii->phy_id, MII_BMSR);
	if (mii->supports_gmii) {
		ctrl1000 = mii->mdio_read(dev, mii->phy_id, MII_CTRL1000);
		stat1000 = mii->mdio_read(dev, mii->phy_id, MII_STAT1000);
	}
	if (bmcr & BMCR_ANENABLE) {
		advertising |= ADVERTISED_Autoneg;
		cmd->base.autoneg = AUTONEG_ENABLE;

		advertising |= mii_get_an(mii, MII_ADVERTISE);
		if (mii->supports_gmii)
			advertising |= mii_ctrl1000_to_ethtool_adv_t(ctrl1000);

		if (bmsr & BMSR_ANEGCOMPLETE) {
			lp_advertising = mii_get_an(mii, MII_LPA);
			lp_advertising |=
					mii_stat1000_to_ethtool_lpa_t(stat1000);
		} else {
			lp_advertising = 0;
		}

		nego = advertising & lp_advertising;

		if (nego & (ADVERTISED_1000baseT_Full |
			    ADVERTISED_1000baseT_Half)) {
			cmd->base.speed = SPEED_1000;
			cmd->base.duplex = !!(nego & ADVERTISED_1000baseT_Full);
		} else if (nego & (ADVERTISED_100baseT_Full |
				   ADVERTISED_100baseT_Half)) {
			cmd->base.speed = SPEED_100;
			cmd->base.duplex = !!(nego & ADVERTISED_100baseT_Full);
		} else {
			cmd->base.speed = SPEED_10;
			cmd->base.duplex = !!(nego & ADVERTISED_10baseT_Full);
		}
	} else {
		cmd->base.autoneg = AUTONEG_DISABLE;

		cmd->base.speed = ((bmcr & BMCR_SPEED1000 &&
				    (bmcr & BMCR_SPEED100) == 0) ?
				   SPEED_1000 :
				   ((bmcr & BMCR_SPEED100) ?
				    SPEED_100 : SPEED_10));
		cmd->base.duplex = (bmcr & BMCR_FULLDPLX) ?
			DUPLEX_FULL : DUPLEX_HALF;

		lp_advertising = 0;
	}

	mii->full_duplex = cmd->base.duplex;

	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.supported,
						supported);
	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.advertising,
						advertising);
	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.lp_advertising,
						lp_advertising);

	/* ignore maxtxpkt, maxrxpkt for now */

	return 0;
}
EXPORT_SYMBOL(mii_ethtool_get_link_ksettings);
#endif /* LINUX_VERSION_IS_GEQ(4,6,0) */

void __page_frag_cache_drain(struct page *page, unsigned int count)
{
	VM_BUG_ON_PAGE(page_ref_count(page) == 0, page);

	if (page_ref_sub_and_test(page, count)) {
		unsigned int order = compound_order(page);

		/*
		 * __free_pages_ok() is not exported so call
		 * __free_pages() which decrements the ref counter
		 * and increment the ref counter before.
		 */
		page_ref_inc(page);
		__free_pages(page, order);
	}
}
EXPORT_SYMBOL_GPL(__page_frag_cache_drain);
