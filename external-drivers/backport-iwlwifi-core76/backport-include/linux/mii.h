#ifndef __BACKPORT_LINUX_MII_H
#define __BACKPORT_LINUX_MII_H
#include_next <linux/mii.h>
#include <linux/version.h>


#if LINUX_VERSION_IN_RANGE(4,6,0, 4,11,0)
extern int mii_ethtool_get_link_ksettings(
	struct mii_if_info *mii, struct ethtool_link_ksettings *cmd);
extern int mii_ethtool_set_link_ksettings(
	struct mii_if_info *mii, const struct ethtool_link_ksettings *cmd);
#endif /* LINUX_VERSION_IN_RANGE(4,6,0, 4,11,0) */

#endif /* __BACKPORT_LINUX_MII_H */
