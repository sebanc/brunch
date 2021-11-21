// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/wwan.h>
#include <net/ipv6.h>

#include "t7xx_hif_dpmaif_rx.h"
#include "t7xx_hif_dpmaif_tx.h"
#include "t7xx_netdev.h"

/* IP MUX channel range */
#define IP_MUX_SESSION_START	0
#define IP_MUX_SESSION_END	7

/* Default IP MUX channel */
#define IP_MUX_SESSION_DEFAULT	0

static void ccmni_enable_napi(struct ccmni_ctl_block *ctlb)
{
	int i;

	if (atomic_cmpxchg(&ctlb->napi_enable, 0, 1) == 0) {
		for (i = 0; i < RXQ_NUM; i++) {
			napi_enable(ctlb->napi[i]);
			napi_schedule(ctlb->napi[i]);
		}
	}
}

static void ccmni_disable_napi(struct ccmni_ctl_block *ctlb)
{
	int i;

	if (atomic_cmpxchg(&ctlb->napi_enable, 1, 0) == 1) {
		for (i = 0; i < RXQ_NUM; i++) {
			napi_synchronize(ctlb->napi[i]);
			napi_disable(ctlb->napi[i]);
		}
	}
}

static void ccmni_make_etherframe(struct net_device *dev,
				  void *skb_eth_hdr, unsigned char *mac_addr,
				  unsigned int packet_type)
{
	struct ethhdr *eth_hdr = skb_eth_hdr;

	memcpy(eth_hdr->h_dest, mac_addr, sizeof(eth_hdr->h_dest));
	memset(eth_hdr->h_source, 0, sizeof(eth_hdr->h_source));

	if (packet_type == IPV6_VERSION)
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IPV6);
	else
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IP);
}

static bool is_ack_skb(struct sk_buff *skb)
{
	u32 packet_type;
	struct tcphdr *tcph;
	u32 total_len, l4_off;
	u8 nexthdr;
	__be16 frag_off;
	struct ipv6hdr *ip6h;
	struct iphdr *ip4h;

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		ip6h = (struct ipv6hdr *)skb->data;
		total_len = sizeof(struct ipv6hdr) + ntohs(ip6h->payload_len);
		nexthdr = ip6h->nexthdr;
		l4_off = ipv6_skip_exthdr(skb, sizeof(struct ipv6hdr),
					  &nexthdr, &frag_off);
		tcph = (struct tcphdr *)(skb->data + l4_off);

		if (nexthdr == IPPROTO_TCP && !tcph->syn && !tcph->fin &&
		    !tcph->rst && ((total_len - l4_off) == (tcph->doff << 2)))
			return true;
	} else if (packet_type == IPV4_VERSION) {
		ip4h = (struct iphdr *)skb->data;

		tcph = (struct tcphdr *)(skb->data + (ip4h->ihl << 2));
		if (ip4h->protocol == IPPROTO_TCP &&
		    !tcph->syn && !tcph->fin && !tcph->rst &&
		    (ntohs(ip4h->tot_len) == (ip4h->ihl << 2) + (tcph->doff << 2)))
			return true;
	}

	return false;
}

/********************netdev register functions********************/
static u16 ccmni_select_queue(struct net_device *dev, struct sk_buff *skb,
			      struct net_device *sb_dev)

{
	u16 queue_id = CCMNI_TXQ_NORMAL;
	struct ccmni_instance *ccmni = (struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ctlb = ccmni->ctlb;

	if (ctlb->capability & NIC_CAP_DATA_ACK_DVD) {
		if (is_ack_skb(skb))
			queue_id = CCMNI_TXQ_FAST;
	}

	return queue_id;
}

static int ccmni_open(struct net_device *dev)
{
	struct ccmni_instance *ccmni = wwan_netdev_drvpriv(dev);
	struct ccmni_ctl_block *ccmni_ctl = ccmni->ctlb;
	struct ccmni_instance *ccmni_tmp;

	if (!ccmni_ctl) {
		netdev_err(dev, "%s_Open: MD ctlb is NULL\n", dev->name);
		return -EINVAL;
	}

	netif_carrier_on(dev);
	netif_tx_start_all_queues(dev);

	if (ccmni_ctl->capability & NIC_CAP_NAPI) {
		if (!atomic_read(&ccmni_ctl->napi_usr_refcnt)) {
			ccmni_enable_napi(ccmni_ctl);
			atomic_set(&ccmni_ctl->napi_usr_refcnt, 1);
		} else {
			atomic_inc(&ccmni_ctl->napi_usr_refcnt);
		}
	}

	atomic_inc(&ccmni->usage);
	ccmni_tmp = ccmni_ctl->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp)
		atomic_set(&ccmni_tmp->usage, atomic_read(&ccmni->usage));

	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	struct ccmni_instance *ccmni = wwan_netdev_drvpriv(dev);
	struct ccmni_ctl_block *ccmni_ctl = ccmni->ctlb;
	struct ccmni_instance *ccmni_tmp;

	if (!ccmni_ctl) {
		netdev_err(dev, "%s_Close: MD ctlb is NULL\n",
			   dev->name);
		return -EINVAL;
	}

	if (atomic_read(&ccmni->usage) <= 0)
		return 0;

	if (ccmni_ctl->capability & NIC_CAP_NAPI) {
		if (atomic_dec_and_test(&ccmni_ctl->napi_usr_refcnt))
			ccmni_disable_napi(ccmni_ctl);
	}

	atomic_dec(&ccmni->usage);
	ccmni_tmp = ccmni_ctl->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp)
		atomic_set(&ccmni_tmp->usage, atomic_read(&ccmni->usage));

	netif_carrier_off(dev);
	netif_tx_disable(dev);

	return 0;
}

static int ccmni_send_packet(struct ccmni_instance *ccmni,
			     void *data, int is_ack)
{
	struct ccci_header *ccci_h;
	struct sk_buff *skb = (struct sk_buff *)data;
	unsigned char tx_qno;
	unsigned int ccmni_idx = ccmni->index;
	struct ccmni_ctl_block *ctlb = ccmni->ctlb;

	skb_push(skb, sizeof(struct ccci_header));
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->channel = 0;
	ccci_h->data[0] = ccmni_idx;
	/* as skb->len already included ccci_header after skb_push */
	ccci_h->data[1] = skb->len;
	ccci_h->reserved = 0;

	/* select the HIF TXQ */
	if (is_ack)
		tx_qno = HIF_ACK_QUEUE;
	else
		tx_qno = HIF_NORMAL_QUEUE;

	if (dpmaif_tx_send_skb(ctlb->hif_ctrl, tx_qno, skb, 0)) {
		skb_pull(skb, sizeof(struct ccci_header));
		/* undo header, in next retry, we'll reserve header again */
		return NETDEV_TX_BUSY;
	}

	return 0;
}

static int ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int skb_len = skb->len;
	struct ccmni_instance *ccmni = wwan_netdev_drvpriv(dev);
	struct ccmni_ctl_block *ctlb = ccmni->ctlb;
	unsigned int is_ack = 0;

	/* If MTU changed or no headroom, drop the packet */
	if (skb->len > dev->mtu || skb_headroom(skb) < sizeof(struct ccci_header)) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (ctlb->capability & NIC_CAP_DATA_ACK_DVD)
		is_ack = is_ack_skb(skb);

	if (ccmni_send_packet(ccmni, skb, is_ack) == NETDEV_TX_BUSY)
		goto tx_busy;

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb_len;
	if (ccmni->tx_busy_cnt[is_ack] > 10) {
		netdev_notice(dev, "[TX]CCMNI:%d TX busy:tx_pkt=%ld(ack=%d) retries=%ld\n",
			      ccmni->index, dev->stats.tx_packets,
			      is_ack, ccmni->tx_busy_cnt[is_ack]);
	}
	ccmni->tx_busy_cnt[is_ack] = 0;

	return NETDEV_TX_OK;

tx_busy:
	if (!(ctlb->capability & NIC_CAP_TXBUSY_STOP)) {
		if ((ccmni->tx_busy_cnt[is_ack]++) % 100 == 0)
			netdev_notice(dev, "[TX]CCMNI:%d TX busy:tx_pkt=%ld(ack=%d) retries=%ld\n",
				      ccmni->index, dev->stats.tx_packets,
				      is_ack, ccmni->tx_busy_cnt[is_ack]);
	} else {
		ccmni->tx_busy_cnt[is_ack]++;
	}

	return NETDEV_TX_BUSY;
}

static int ccmni_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu > CCMNI_MTU_MAX)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static void ccmni_tx_timeout(struct net_device *dev, unsigned int __always_unused txqueue)
{
	struct ccmni_instance *ccmni = (struct ccmni_instance *)netdev_priv(dev);

	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0)
		netif_tx_wake_all_queues(dev);
}

static const struct net_device_ops ccmni_netdev_ops = {
	.ndo_open	= ccmni_open,
	.ndo_stop	= ccmni_close,
	.ndo_start_xmit = ccmni_start_xmit,
	.ndo_tx_timeout = ccmni_tx_timeout,
	.ndo_change_mtu = ccmni_change_mtu,
	.ndo_select_queue = ccmni_select_queue,
};

/***********ccmni driver register ccci function***********/
static void ccmni_inst_init(struct ccmni_instance *ccmni, struct net_device *dev)
{
	int i;

	ccmni->dev = dev;
	atomic_set(&ccmni->usage, 0);

	for (i = 0; i < 2; i++) {
		ccmni->tx_seq_num[i] = 0;
		ccmni->tx_full_cnt[i] = 0;
		ccmni->tx_irq_cnt[i] = 0;
		ccmni->tx_full_tick[i] = 0;
		ccmni->flags[i] &= ~CCMNI_TX_PRINT_F;
	}
	ccmni->rx_seq_num = 0;
	ccmni->rx_gro_cnt = 0;
}

static void ccmni_start(struct ccmni_ctl_block *ctlb)
{
	struct ccmni_instance *ccmni;
	unsigned char i;

	/* carry on the net link */
	for (i = 0; i < ctlb->nic_dev_num; i++) {
		ccmni = ctlb->ccmni_inst[i];
		if (!ccmni) {
			dev_err(ctlb->hif_ctrl->dev, "ccmni%d is null\n", i);
			continue;
		}

		if (atomic_read(&ccmni->usage) > 0) {
			netif_tx_start_all_queues(ccmni->dev);
			netif_carrier_on(ccmni->dev);
		}
	}

	/* enable the NAPI */
	if ((ctlb->capability & NIC_CAP_NAPI) &&
	    atomic_read(&ctlb->napi_usr_refcnt)) {
		ccmni_enable_napi(ctlb);
	}
}

static void ccmni_pre_stop(struct ccmni_ctl_block *ctlb)
{
	struct ccmni_instance *ccmni;
	unsigned char i;

	/* stop tx */
	for (i = 0; i < ctlb->nic_dev_num; i++) {
		ccmni = ctlb->ccmni_inst[i];
		if (!ccmni) {
			dev_err(ctlb->hif_ctrl->dev, "ccmni%d is null\n", i);
			continue;
		}
		if (atomic_read(&ccmni->usage) > 0)
			netif_tx_disable(ccmni->dev);
	}
}

static void ccmni_pos_stop(struct ccmni_ctl_block *ctlb)
{
	struct ccmni_instance *ccmni;
	unsigned char i;

	if ((ctlb->capability & NIC_CAP_NAPI) &&
	    atomic_read(&ctlb->napi_usr_refcnt)) {
		ccmni_disable_napi(ctlb);
	}

	/* carry off the net link */
	for (i = 0; i < ctlb->nic_dev_num; i++) {
		ccmni = ctlb->ccmni_inst[i];
		if (!ccmni) {
			dev_err(ctlb->hif_ctrl->dev, "ccmni%d is null\n", i);
			continue;
		}

		if (atomic_read(&ccmni->usage) > 0)
			netif_carrier_off(ccmni->dev);
	}
}

static void ccmni_wwan_setup(struct net_device *dev)
{
	dev->header_ops = NULL;
	dev->hard_header_len += sizeof(struct ccci_header);

	dev->mtu = CCMNI_MTU;
	dev->max_mtu = CCMNI_MTU_MAX;
	dev->tx_queue_len = CCMNI_TX_QUEUE;
	dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
	/* ccmni is a pure IP device */
	dev->flags = IFF_NOARP & ~(IFF_BROADCAST | IFF_MULTICAST);

	/* not supporting VLAN */
	dev->features = NETIF_F_VLAN_CHALLENGED;

	dev->features |= NETIF_F_SG;
	dev->hw_features |= NETIF_F_SG;

	/* uplink checksum offload */
	dev->features |= NETIF_F_HW_CSUM;
	dev->hw_features |= NETIF_F_HW_CSUM;

	/* downlink checksum offload */
	dev->features |= NETIF_F_RXCSUM;
	dev->hw_features |= NETIF_F_RXCSUM;

	dev->addr_len = ETH_ALEN;

	/* use kernel default free_netdev() function */
	dev->needs_free_netdev = true;

	/* do not need to free again, because of free_netdev() */
	dev->priv_destructor = NULL;

	dev->netdev_ops = &ccmni_netdev_ops;
	random_ether_addr((u8 *)dev->dev_addr);
}

static __maybe_unused int init_netdev_napi(struct ccmni_ctl_block *ctlb)
{
	int i, j;

	/* one HW, but shared with multiple net devices,
	 * so add a dummy device for NAPI.
	 */
	init_dummy_netdev(&ctlb->dummy_dev);

	atomic_set(&ctlb->napi_usr_refcnt, 0);
	atomic_set(&ctlb->napi_enable, 0);
	ctlb->napi_poll_weigh = NIC_NAPI_POLL_BUDGET; /* default budget 64 */
	ctlb->napi_poll = dpmaif_napi_rx_poll;

	/* init all HW NAPI */
	for (i = 0; i < RXQ_NUM; i++) {
		ctlb->napi[i] = dpmaif_get_napi(ctlb->hif_ctrl, i);
		if (ctlb->napi[i]) {
			netif_napi_add(&ctlb->dummy_dev, ctlb->napi[i],
				       ctlb->napi_poll,
				       ctlb->napi_poll_weigh);
		} else {
			dev_err(&ctlb->mtk_dev->pdev->dev,
				"dpmaif_get_napi failed: napi[%d]\n", i);
			goto add_napi_fail;
		}
	}

	return 0;

add_napi_fail:
	for (j = i - 1; j >= 0; j--)
		netif_napi_del(ctlb->napi[j]);

	return -EFAULT;
}

static __maybe_unused void uninit_netdev_napi(struct ccmni_ctl_block *ctlb)
{
	int i;

	/* release the NAPI */
	if ((ctlb->capability & NIC_CAP_NAPI)) {
		for (i = 0; i < RXQ_NUM; i++) {
			if (!ctlb->napi[i])
				continue;
			netif_napi_del(ctlb->napi[i]);
			ctlb->napi[i] = NULL;
		}
	}
}

static int ccmni_wwan_newlink(void *ctxt, struct net_device *dev, u32 if_id,
			      struct netlink_ext_ack *extack)
{
	struct ccmni_ctl_block *ctlb = ctxt;
	struct ccmni_instance *ccmni;
	int ret;

	if (if_id < IP_MUX_SESSION_START ||
	    if_id >= ARRAY_SIZE(ctlb->ccmni_inst))
		return -EINVAL;

	/* initialize private structure of netdev */
	ccmni = wwan_netdev_drvpriv(dev);
	ccmni->index = if_id;
	ccmni->ctlb = ctlb;
	ccmni_inst_init(ccmni, dev);
	ctlb->ccmni_inst[if_id] = ccmni;

	ret = register_netdevice(dev);
	if (ret)
		return ret;

	netif_device_attach(dev);

	return 0;
}

static void ccmni_wwan_dellink(void *ctxt, struct net_device *dev, struct list_head *head)
{
	struct ccmni_instance *ccmni = wwan_netdev_drvpriv(dev);
	struct ccmni_ctl_block *ctlb = ctxt;
	int if_id = ccmni->index;

	if (if_id < IP_MUX_SESSION_START ||
	    if_id >= ARRAY_SIZE(ctlb->ccmni_inst))
		return;

	if (WARN_ON(ctlb->ccmni_inst[if_id] != ccmni))
		return;

	unregister_netdevice(dev);
}

static const struct wwan_ops ccmni_wwan_ops = {
	.priv_size = sizeof(struct ccmni_instance),
	.setup = ccmni_wwan_setup,
	.newlink = ccmni_wwan_newlink,
	.dellink = ccmni_wwan_dellink,
};

static int ccmni_md_state_callback(enum MD_STATE state, void *para)
{
	struct ccmni_ctl_block *ctlb = (struct ccmni_ctl_block *)para;
	int ret = 0;

	if (!ctlb) {
		pr_err("Invalid input para, md_sta=%d\n", state);
		return -EINVAL;
	}

	ctlb->md_sta = state;

	switch (state) {
	case BOOT_WAITING_FOR_HS1:
		/* hif layer */
		ret = dpmaif_md_state_callback(ctlb->hif_ctrl, state);
		if (ret < 0)
			netdev_err(&ctlb->dummy_dev, "dpmaif md state callback err, md_sta=%d\n",
				   state);
		break;
	case READY:
		ccmni_start(ctlb);
		break;
	case EXCEPTION:
		ccmni_pre_stop(ctlb);
		ret = dpmaif_md_state_callback(ctlb->hif_ctrl, state);
		if (ret < 0)
			netdev_err(&ctlb->dummy_dev, "dpmaif md state callback err, md_sta=%d\n",
				   state);
		ccmni_pos_stop(ctlb);
		break;
	case STOPPED:
		ccmni_pre_stop(ctlb);
		/* hif layer */
		ret = dpmaif_md_state_callback(ctlb->hif_ctrl, state);
		if (ret < 0)
			netdev_err(&ctlb->dummy_dev, "dpmaif md state callback err, md_sta=%d\n",
				   state);
		ccmni_pos_stop(ctlb);
		break;
	case WAITING_TO_STOP:
		/* hif layer */
		ret = dpmaif_md_state_callback(ctlb->hif_ctrl, state);
		if (ret < 0)
			netdev_err(&ctlb->dummy_dev, "dpmaif md state callback err, md_sta=%d\n",
				   state);
		break;
	default:
		break;
	}

	return ret;
}

static int init_md_status_notifier(struct ccmni_ctl_block *ctlb)
{
	struct fsm_notifier_block *md_status_notifier = &ctlb->md_status_notify;
	int ret;

	INIT_LIST_HEAD(&md_status_notifier->entry);
	md_status_notifier->notifier_fn = ccmni_md_state_callback;
	md_status_notifier->data = ctlb;

	ret = fsm_notifier_register(md_status_notifier);
	if (ret < 0)
		netdev_err(&ctlb->dummy_dev, "ccmni fsm_notifier_register fail\n");

	return ret;
}

static int uninit_md_status_notifier(struct ccmni_ctl_block *ctlb)
{
	int ret;

	ret = fsm_notifier_unregister(&ctlb->md_status_notify);
	if (ret < 0)
		netdev_err(&ctlb->dummy_dev, "ccmni fsm_notifier_unregister fail\n");

	return ret;
}

unsigned int ccmni_get_nic_cap(struct ccmni_ctl_block *ctlb)
{
	if (!ctlb) {
		pr_err("ctlb NULL err\n");
		return 0;
	}

	return ctlb->capability;
}

int ccmni_init(struct mtk_pci_dev *mtk_dev)
{
	struct ccmni_ctl_block *ctlb;
	int ret;

	ctlb = kzalloc(sizeof(*ctlb), GFP_KERNEL);
	if (!ctlb)
		return -ENOMEM;

	mtk_dev->ccmni_ctlb = ctlb;
	ctlb->mtk_dev = mtk_dev;
	ctlb->capability = NIC_CAP_TXBUSY_STOP | NIC_CAP_SGIO |
		NIC_CAP_DATA_ACK_DVD | NIC_CAP_CCMNI_MQ;
	ctlb->nic_dev_created = false;

	ret = dpmaif_hif_init(ctlb);
	if (ret < 0) {
		dev_err(&mtk_dev->pdev->dev, "init dpmaif hif fail\n");
		goto alloc_mem_fail;
	}

	ctlb->nic_dev_num = NIC_DEV_DEFAULT;
	if (ctlb->nic_dev_num > NIC_DEV_MAX) {
		dev_err(&mtk_dev->pdev->dev,
			"The NIC devices number err, please check(MAX = 20)\n");
		ret = -1;
		goto alloc_mem_fail;
	}

	/* WWAN core will create a netdev for the default IP MUX channel */
	ret = wwan_register_ops(&ctlb->mtk_dev->pdev->dev, &ccmni_wwan_ops, ctlb,
				IP_MUX_SESSION_DEFAULT);
	if (ret)
		goto alloc_mem_fail;

	ret = init_md_status_notifier(ctlb);
	if (ret < 0) {
		dev_err(&mtk_dev->pdev->dev, "initial FSM notifier fail\n");
		goto wwan_register_fail;
	}

	return 0;

wwan_register_fail:
	wwan_unregister_ops(&ctlb->mtk_dev->pdev->dev);
alloc_mem_fail:
	kfree(ctlb);
	return ret;
}

void ccmni_exit(struct mtk_pci_dev *mtk_dev)
{
	struct ccmni_ctl_block *ctlb;

	ctlb = mtk_dev->ccmni_ctlb;
	if (!ctlb) {
		dev_err(&mtk_dev->pdev->dev, "The mtk_dev ctlb is NULL err\n");
		return;
	}

	/* uninit fsm notifier */
	uninit_md_status_notifier(ctlb);

	wwan_unregister_ops(&ctlb->mtk_dev->pdev->dev);

	dpmaif_hif_exit(ctlb);

	kfree(ctlb);
	mtk_dev->ccmni_ctlb = NULL;
}

static bool is_skb_gro(struct sk_buff *skb)
{
	u32 packet_type;

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV4_VERSION &&
	    ip_hdr(skb)->protocol == IPPROTO_TCP)
		return true;
	else if (packet_type == IPV6_VERSION &&
		 ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
		return true;

	return false;
}

int ccmni_rx_callback(struct ccmni_ctl_block *ctlb, int ccmni_idx,
		      struct sk_buff *skb, struct napi_struct *napi)
{
	struct ccmni_instance *ccmni;
	struct net_device *dev;
	int pkt_type, skb_len, is_gro;

	if (!ctlb) {
		pr_err("Invalid CCMNI%d ctrl/ops struct\n", ccmni_idx);
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ccmni = ctlb->ccmni_inst[ccmni_idx];
	if (!ccmni) {
		dev_kfree_skb(skb);
		return 0;
	}

	dev = ccmni->dev;

	pkt_type = skb->data[0] & 0xF0;
	ccmni_make_etherframe(dev, skb->data - ETH_HLEN, dev->dev_addr, pkt_type);
	skb_set_mac_header(skb, -ETH_HLEN);
	skb_reset_network_header(skb);
	skb->dev = dev;
	if (pkt_type == IPV6_VERSION)
		skb->protocol = htons(ETH_P_IPV6);
	else
		skb->protocol = htons(ETH_P_IP);

	skb_len = skb->len;

	is_gro = is_skb_gro(skb);
	if (ctlb->capability & NIC_CAP_NAPI) {
		if (is_gro && napi)
			napi_gro_receive(napi, skb);
		else if (napi)
			netif_receive_skb(skb);
		else
			netif_rx_ni(skb);
	} else {
		if (!in_interrupt())
			netif_rx_ni(skb);
		else
			netif_rx(skb);
	}
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;

	return 0;
}

void ccmni_queue_state_notify(struct ccmni_ctl_block *ctlb,
			      enum HIF_STATE state, int qno)
{
	int ccmni_idx;
	struct ccmni_instance *ccmni;
	struct ccmni_instance *ccmni_tmp;
	struct net_device *dev;
	struct netdev_queue *net_queue;
	int is_ack = 0;

	if (!ctlb) {
		pr_err("Invalid ccmni ctrl when hif_sta=%d\n", state);
		return;
	}

	if (ctlb->md_sta != READY)
		return;

	/* check ack packet: HIF_ACK_QUEUE <->  CCMNI_TXQ_FAST */
	if (qno == HIF_ACK_QUEUE)
		is_ack = 1;

	/* notify ONLY default nic */
	for (ccmni_idx = 0; ccmni_idx < 1; ccmni_idx++) {
		ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
		if (!ccmni_tmp) {
			pr_err("ccmni%d is null\n", ccmni_idx);
			continue;
		}

		dev = ccmni_tmp->dev;
		ccmni = wwan_netdev_drvpriv(dev);

		switch (state) {
		case TX_IRQ:
			if (netif_running(ccmni->dev) &&
			    atomic_read(&ccmni->usage) > 0) {
				if (ctlb->capability & NIC_CAP_CCMNI_MQ) {
					if (is_ack)
						net_queue = netdev_get_tx_queue(ccmni->dev,
										CCMNI_TXQ_FAST);
					else
						net_queue = netdev_get_tx_queue(ccmni->dev,
										CCMNI_TXQ_NORMAL);
					if (netif_tx_queue_stopped(net_queue))
						netif_tx_wake_queue(net_queue);
				} else {
					is_ack = 0;
					if (netif_queue_stopped(ccmni->dev))
						netif_wake_queue(ccmni->dev);
				}
				ccmni->tx_irq_cnt[is_ack]++;
				if ((ccmni->flags[is_ack] & CCMNI_TX_PRINT_F) ||
				    time_after(jiffies,	ccmni->tx_irq_tick[is_ack] + 2))
					ccmni->flags[is_ack] &= ~CCMNI_TX_PRINT_F;
			}
			break;
		case TX_FULL:
			if (atomic_read(&ccmni->usage) > 0) {
				if (ctlb->capability & NIC_CAP_CCMNI_MQ) {
					if (is_ack)
						net_queue = netdev_get_tx_queue(ccmni->dev,
										CCMNI_TXQ_FAST);
					else
						net_queue = netdev_get_tx_queue(ccmni->dev,
										CCMNI_TXQ_NORMAL);
					netif_tx_stop_queue(net_queue);
				} else {
					is_ack = 0;
					netif_stop_queue(ccmni->dev);
				}
				ccmni->tx_full_cnt[is_ack]++;
				ccmni->tx_irq_tick[is_ack] = jiffies;
				if (time_after(jiffies,
					       ccmni->tx_full_tick[is_ack] + 4)) {
					ccmni->tx_full_tick[is_ack] = jiffies;
					ccmni->flags[is_ack] |= CCMNI_TX_PRINT_F;
				}
			}
			break;
		default:
			break;
		}
	}
}

