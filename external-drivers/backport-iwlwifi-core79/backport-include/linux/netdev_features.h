#ifndef __BACKPORT_NETDEV_FEATURES_H
#define __BACKPORT_NETDEV_FEATURES_H

#include <linux/version.h>
#include_next <linux/netdev_features.h>

/* this was renamed in commit 53692b1de :  sctp: Rename NETIF_F_SCTP_CSUM to NETIF_F_SCTP_CRC */
#ifndef NETIF_F_SCTP_CRC
#define NETIF_F_SCTP_CRC __NETIF_F(SCTP_CSUM)
#endif

#endif /* __BACKPORT_NETDEV_FEATURES_H */
