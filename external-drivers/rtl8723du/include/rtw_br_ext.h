/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef _RTW_BR_EXT_H_
#define _RTW_BR_EXT_H_

#define CL_IPV6_PASS	1
#define MACADDRLEN		6
#define _DEBUG_ERR		RTW_INFO
#define _DEBUG_INFO		/* RTW_INFO */
#define DEBUG_WARN		RTW_INFO
#define DEBUG_INFO		/* RTW_INFO */
#define DEBUG_ERR		RTW_INFO
/* #define GET_MY_HWADDR		((GET_MIB(priv))->dot11OperationEntry.hwaddr) */
#define GET_MY_HWADDR(adapt)		(adapter_mac_addr(adapt))

#define NAT25_HASH_BITS		4
#define NAT25_HASH_SIZE		(1 << NAT25_HASH_BITS)
#define NAT25_AGEING_TIME	300

#ifdef CL_IPV6_PASS
	#define MAX_NETWORK_ADDR_LEN	17
#else
	#define MAX_NETWORK_ADDR_LEN	11
#endif

struct nat25_network_db_entry {
	struct nat25_network_db_entry	*next_hash;
	struct nat25_network_db_entry	**pprev_hash;
	atomic_t						use_count;
	unsigned char					macAddr[6];
	unsigned long					ageing_timer;
	unsigned char				networkAddr[MAX_NETWORK_ADDR_LEN];
};

enum NAT25_METHOD {
	NAT25_MIN,
	NAT25_CHECK,
	NAT25_INSERT,
	NAT25_LOOKUP,
	NAT25_PARSE,
	NAT25_MAX
};

struct br_ext_info {
	unsigned int	nat25_disable;
	unsigned int	macclone_enable;
	unsigned int	dhcp_bcst_disable;
	int		addPPPoETag;		/* 1: Add PPPoE relay-SID, 0: disable */
	unsigned char	nat25_dmzMac[MACADDRLEN];
	unsigned int	nat25sc_disable;
};

void nat25_db_cleanup(struct adapter *priv);

#endif /* _RTW_BR_EXT_H_ */
