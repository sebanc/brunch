/******************************************************************************
 *
 * Copyright(c) 2007 - 2020 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _RTW_NLRTW_C_

#include <drv_types.h>
#include "nlrtw.h"

#ifdef CONFIG_RTW_NLRTW

#include <net/netlink.h>
#include <net/genetlink.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
#include <uapi/linux/netlink.h>
#endif


enum nlrtw_cmds {
	NLRTW_CMD_UNSPEC,

	NLRTW_CMD_CHANNEL_UTILIZATION,

	__NLRTW_CMD_AFTER_LAST,
	NLRTW_CMD_MAX = __NLRTW_CMD_AFTER_LAST - 1
};

enum nlrtw_attrs {
	NLRTW_ATTR_UNSPEC,

	NLRTW_ATTR_WIPHY_NAME,
	NLRTW_ATTR_CHANNEL_UTILIZATIONS,
	NLRTW_ATTR_CHANNEL_UTILIZATION_THRESHOLD,

	__NLRTW_ATTR_AFTER_LAST,
	NUM_NLRTW_ATTR = __NLRTW_ATTR_AFTER_LAST,
	NLRTW_ATTR_MAX = __NLRTW_ATTR_AFTER_LAST - 1
};

enum nlrtw_ch_util_attrs {
	__NLRTW_ATTR_CHANNEL_UTILIZATION_INVALID,

	NLRTW_ATTR_CHANNEL_UTILIZATION_VALUE,
	NLRTW_ATTR_CHANNEL_UTILIZATION_BSSID,

	__NLRTW_ATTR_CHANNEL_UTILIZATION_AFTER_LAST,
	NUM_NLRTW_ATTR_CHANNEL_UTILIZATION = __NLRTW_ATTR_CHANNEL_UTILIZATION_AFTER_LAST,
	NLRTW_ATTR_CHANNEL_UTILIZATION_MAX = __NLRTW_ATTR_CHANNEL_UTILIZATION_AFTER_LAST - 1
};

static int nlrtw_ch_util_set(struct sk_buff *skb, struct genl_info *info)
{
	unsigned int msg;

	if (!info->attrs[NLRTW_ATTR_CHANNEL_UTILIZATION_THRESHOLD])
		return -EINVAL;
	msg = nla_get_u8(info->attrs[NLRTW_ATTR_CHANNEL_UTILIZATION_THRESHOLD]);

	return 0;
}

static struct nla_policy nlrtw_genl_policy[NUM_NLRTW_ATTR] = {
	[NLRTW_ATTR_CHANNEL_UTILIZATION_THRESHOLD] = { .type = NLA_U8 },
};

static struct genl_ops nlrtw_genl_ops[] = {
	{
		.cmd = NLRTW_CMD_CHANNEL_UTILIZATION,
		.flags = 0,
		.policy = nlrtw_genl_policy,
		.doit = nlrtw_ch_util_set,
		.dumpit = NULL,
	},
};

enum nlrtw_multicast_groups {
	NLRTW_MCGRP_DEFAULT,
};
static struct genl_multicast_group nlrtw_genl_mcgrp[] = {
	[NLRTW_MCGRP_DEFAULT] = { .name = "nlrtw_default" },
};

/* family definition */
static struct genl_family nlrtw_genl_family = {
	.hdrsize = 0,
	.name = "nlrtw_"DRV_NAME,
	.version = 1,
	.maxattr = NLRTW_ATTR_MAX,

	.netnsok = true,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 12)
	.module = THIS_MODULE,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	.ops = nlrtw_genl_ops,
	.n_ops = ARRAY_SIZE(nlrtw_genl_ops),
	.mcgrps = nlrtw_genl_mcgrp,
	.n_mcgrps = ARRAY_SIZE(nlrtw_genl_mcgrp),
#endif
};

static inline int nlrtw_multicast(const struct genl_family *family,
				  struct sk_buff *skb, u32 portid,
				  unsigned int group, gfp_t flags)
{
	int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	ret = genlmsg_multicast(&nlrtw_genl_family, skb, portid, group, flags);
#else
	ret = genlmsg_multicast(skb, portid, nlrtw_genl_mcgrp[group].id, flags);
#endif
	return ret;
}

int rtw_nlrtw_ch_util_rpt(_adapter *adapter, u8 n_rpts, u8 *val, u8 **mac_addr)
{
	struct sk_buff *skb = NULL;
	void *msg_header = NULL;
	struct nlattr *nl_ch_util, *nl_ch_utils;
	struct wiphy *wiphy;
	u8 i;
	int ret;

	wiphy = adapter_to_wiphy(adapter);
	if (!wiphy)
		return -EINVAL;

	/* allocate memory */
	skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!skb) {
		nlmsg_free(skb);
		return -ENOMEM;
	}

	/* create the message headers */
	msg_header = genlmsg_put(skb, 0, 0, &nlrtw_genl_family, 0,
				 NLRTW_CMD_CHANNEL_UTILIZATION);
	if (!msg_header) {
		ret = -ENOMEM;
		goto err_out;
	}

	/* add attributes */
	ret = nla_put_string(skb, NLRTW_ATTR_WIPHY_NAME, wiphy_name(wiphy));

	nl_ch_utils = nla_nest_start(skb, NLRTW_ATTR_CHANNEL_UTILIZATIONS);
	if (!nl_ch_utils) {
		ret = -EMSGSIZE;
		goto err_out;
	}

	for (i = 0; i < n_rpts; i++) {
		nl_ch_util = nla_nest_start(skb, i);
		if (!nl_ch_util) {
			ret = -EMSGSIZE;
			goto err_out;
		}

		ret = nla_put(skb, NLRTW_ATTR_CHANNEL_UTILIZATION_BSSID, ETH_ALEN, *(mac_addr + i));
		if (ret != 0)
			goto err_out;

		ret = nla_put_u8(skb, NLRTW_ATTR_CHANNEL_UTILIZATION_VALUE, *(val + i));
		if (ret != 0)
			goto err_out;

		nla_nest_end(skb, nl_ch_util);
	}

	nla_nest_end(skb, nl_ch_utils);

	/* finalize the message */
	genlmsg_end(skb, msg_header);

	ret = nlrtw_multicast(&nlrtw_genl_family, skb, 0, NLRTW_MCGRP_DEFAULT, GFP_KERNEL);
	if (ret == -ESRCH) {
		RTW_INFO("[%s] return ESRCH(No such process)."
			 " Maybe no process waits for this msg\n", __func__);
		return ret;
	} else if (ret != 0) {
		RTW_INFO("[%s] ret = %d\n", __func__, ret);
		return ret;
	}

	return 0;
err_out:
	nlmsg_free(skb);
	return ret;
}

int rtw_nlrtw_init(void)
{
	int err;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
	err = genl_register_family(&nlrtw_genl_family);
	if (err)
		return err;
#else
	err = genl_register_family_with_ops(&nlrtw_genl_family, nlrtw_genl_ops, ARRAY_SIZE(nlrtw_genl_ops));
	if (err)
		return err;

	err = genl_register_mc_group(&nlrtw_genl_family, &nlrtw_genl_mcgrp[0]);
	if (err) {
		genl_unregister_family(&nlrtw_genl_family);
		return err;
	}
#endif
	RTW_INFO("[%s] %s\n", __func__, nlrtw_genl_family.name);
	return 0;
}

int rtw_nlrtw_deinit(void)
{
	int err;

	err = genl_unregister_family(&nlrtw_genl_family);
	RTW_INFO("[%s] err = %d\n", __func__, err);

	return err;
}
#endif /* CONFIG_RTW_NLRTW */
