// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/wwan.h>

#include <net/ipv6.h>

#include "t7xx_hif_dpmaif_rx.h"
#include "t7xx_hif_dpmaif_tx.h"
#include "t7xx_netdev.h"

#define IP_MUX_SESSION_DEFAULT	0
#define SBD_PACKET_TYPE_MASK	GENMASK(7, 4)

static void ccmni_make_etherframe(struct net_device *dev, void *skb_eth_hdr,
				  u8 *mac_addr, unsigned int packet_type)
{
	struct ethhdr *eth_hdr;

	eth_hdr = skb_eth_hdr;
	memcpy(eth_hdr->h_dest, mac_addr, sizeof(eth_hdr->h_dest));
	memset(eth_hdr->h_source, 0, sizeof(eth_hdr->h_source));

	if (packet_type == IPV6_VERSION)
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IPV6);
	else
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IP);
}

static enum txq_type get_txq_type(struct sk_buff *skb)
{
	u32 total_len, payload_len, l4_off;
	bool tcp_syn_fin_rst, is_tcp;
	struct ipv6hdr *ip6h;
	struct tcphdr *tcph;
	struct iphdr *ip4h;
	u32 packet_type;
	__be16 frag_off;

	packet_type = skb->data[0] & SBD_PACKET_TYPE_MASK;
	if (packet_type == IPV6_VERSION) {
		ip6h = (struct ipv6hdr *)skb->data;
		total_len = sizeof(struct ipv6hdr) + ntohs(ip6h->payload_len);
		l4_off = ipv6_skip_exthdr(skb, sizeof(struct ipv6hdr), &ip6h->nexthdr, &frag_off);
		tcph = (struct tcphdr *)(skb->data + l4_off);
		is_tcp = ip6h->nexthdr == IPPROTO_TCP;
		payload_len = total_len - l4_off - (tcph->doff << 2);
	} else if (packet_type == IPV4_VERSION) {
		ip4h = (struct iphdr *)skb->data;
		tcph = (struct tcphdr *)(skb->data + (ip4h->ihl << 2));
		is_tcp = ip4h->protocol == IPPROTO_TCP;
		payload_len = ntohs(ip4h->tot_len) - (ip4h->ihl << 2) - (tcph->doff << 2);
	} else {
		return TXQ_NORMAL;
	}

	tcp_syn_fin_rst = tcph->syn || tcph->fin || tcph->rst;
	if (is_tcp && !payload_len && !tcp_syn_fin_rst)
		return TXQ_FAST;

	return TXQ_NORMAL;
}

static u16 ccmni_select_queue(struct net_device *dev, struct sk_buff *skb,
			      struct net_device *sb_dev)
{
	struct ccmni_instance *ccmni;

	ccmni = netdev_priv(dev);

	if (ccmni->ctlb->capability & NIC_CAP_DATA_ACK_DVD)
		return get_txq_type(skb);

	return TXQ_NORMAL;
}

static int ccmni_open(struct net_device *dev)
{
	struct ccmni_instance *ccmni;

	ccmni = wwan_netdev_drvpriv(dev);
	netif_carrier_on(dev);
	netif_tx_start_all_queues(dev);
	atomic_inc(&ccmni->usage);
	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	struct ccmni_instance *ccmni;

	ccmni = wwan_netdev_drvpriv(dev);

	if (atomic_dec_return(&ccmni->usage) < 0)
		return -EINVAL;

	netif_carrier_off(dev);
	netif_tx_disable(dev);
	return 0;
}

static int ccmni_send_packet(struct ccmni_instance *ccmni, struct sk_buff *skb, enum txq_type txqt)
{
	struct ccmni_ctl_block *ctlb;
	struct ccci_header *ccci_h;
	unsigned int ccmni_idx;

	skb_push(skb, sizeof(struct ccci_header));
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->status &= ~HDR_FLD_CHN;

	ccmni_idx = ccmni->index;
	ccci_h->data[0] = ccmni_idx;
	ccci_h->data[1] = skb->len;
	ccci_h->reserved = 0;

	ctlb = ccmni->ctlb;
	if (dpmaif_tx_send_skb(ctlb->hif_ctrl, txqt, skb)) {
		skb_pull(skb, sizeof(struct ccci_header));
		/* we will reserve header again in the next retry */
		return NETDEV_TX_BUSY;
	}

	return 0;
}

static int ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ccmni_instance *ccmni;
	struct ccmni_ctl_block *ctlb;
	enum txq_type txqt;
	int skb_len;

	ccmni = wwan_netdev_drvpriv(dev);
	ctlb = ccmni->ctlb;
	txqt = TXQ_NORMAL;
	skb_len = skb->len;

	/* If MTU changed or there is no headroom, drop the packet */
	if (skb->len > dev->mtu || skb_headroom(skb) < sizeof(struct ccci_header)) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (ctlb->capability & NIC_CAP_DATA_ACK_DVD)
		txqt = get_txq_type(skb);

	if (ccmni_send_packet(ccmni, skb, txqt)) {
		if (!(ctlb->capability & NIC_CAP_TXBUSY_STOP)) {
			if ((ccmni->tx_busy_cnt[txqt]++) % 100 == 0)
				netdev_notice(dev, "[TX]CCMNI:%d busy:pkt=%ld(ack=%d) cnt=%ld\n",
					      ccmni->index, dev->stats.tx_packets,
					      txqt, ccmni->tx_busy_cnt[txqt]);
		} else {
			ccmni->tx_busy_cnt[txqt]++;
		}

		return NETDEV_TX_BUSY;
	}

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb_len;
	if (ccmni->tx_busy_cnt[txqt] > 10) {
		netdev_notice(dev, "[TX]CCMNI:%d TX busy:tx_pkt=%ld(ack=%d) retries=%ld\n",
			      ccmni->index, dev->stats.tx_packets,
			      txqt, ccmni->tx_busy_cnt[txqt]);
	}
	ccmni->tx_busy_cnt[txqt] = 0;

	return NETDEV_TX_OK;
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
	struct ccmni_instance *ccmni;

	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0)
		netif_tx_wake_all_queues(dev);
}

static const struct net_device_ops ccmni_netdev_ops = {
	.ndo_open	  = ccmni_open,
	.ndo_stop	  = ccmni_close,
	.ndo_start_xmit   = ccmni_start_xmit,
	.ndo_tx_timeout   = ccmni_tx_timeout,
	.ndo_change_mtu   = ccmni_change_mtu,
	.ndo_select_queue = ccmni_select_queue,
};

static void ccmni_start(struct ccmni_ctl_block *ctlb)
{
	struct ccmni_instance *ccmni;
	int i;

	/* carry on the net link */
	for (i = 0; i < ctlb->nic_dev_num; i++) {
		ccmni = ctlb->ccmni_inst[i];
		if (!ccmni)
			continue;

		if (atomic_read(&ccmni->usage) > 0) {
			netif_tx_start_all_queues(ccmni->dev);
			netif_carrier_on(ccmni->dev);
		}
	}
}

static void ccmni_pre_stop(struct ccmni_ctl_block *ctlb)
{
	struct ccmni_instance *ccmni;
	int i;

	/* stop tx */
	for (i = 0; i < ctlb->nic_dev_num; i++) {
		ccmni = ctlb->ccmni_inst[i];
		if (!ccmni)
			continue;

		if (atomic_read(&ccmni->usage) > 0)
			netif_tx_disable(ccmni->dev);
	}
}

static void ccmni_pos_stop(struct ccmni_ctl_block *ctlb)
{
	struct ccmni_instance *ccmni;
	int i;

	/* carry off the net link */
	for (i = 0; i < ctlb->nic_dev_num; i++) {
		ccmni = ctlb->ccmni_inst[i];
		if (!ccmni)
			continue;

		if (atomic_read(&ccmni->usage) > 0)
			netif_carrier_off(ccmni->dev);
	}
}

static void ccmni_wwan_setup(struct net_device *dev)
{
	dev->header_ops = NULL;
	dev->hard_header_len += sizeof(struct ccci_header);

	dev->mtu = 1500;
	dev->max_mtu = CCMNI_MTU_MAX;
	dev->tx_queue_len = CCMNI_TX_QUEUE;
	dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
	/* ccmni is a pure IP device */
	dev->flags = (IFF_POINTOPOINT | IFF_NOARP)
		     & ~(IFF_BROADCAST | IFF_MULTICAST);

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

	/* no need to free again because of free_netdev() */
	dev->priv_destructor = NULL;
	dev->type = ARPHRD_PPP;

	dev->netdev_ops = &ccmni_netdev_ops;
	random_ether_addr((u8 *)dev->dev_addr);
}

static int ccmni_wwan_newlink(void *ctxt, struct net_device *dev, u32 if_id,
			      struct netlink_ext_ack *extack)
{
	struct ccmni_ctl_block *ctlb;
	struct ccmni_instance *ccmni;
	int ret;

	ctlb = ctxt;

	if (if_id >= ARRAY_SIZE(ctlb->ccmni_inst))
		return -EINVAL;

	/* initialize private structure of netdev */
	ccmni = wwan_netdev_drvpriv(dev);
	ccmni->index = if_id;
	ccmni->ctlb = ctlb;
	ccmni->dev = dev;
	atomic_set(&ccmni->usage, 0);
	ctlb->ccmni_inst[if_id] = ccmni;

	ret = register_netdevice(dev);
	if (ret)
		return ret;

	netif_device_attach(dev);
	return 0;
}

static void ccmni_wwan_dellink(void *ctxt, struct net_device *dev, struct list_head *head)
{
	struct ccmni_instance *ccmni;
	struct ccmni_ctl_block *ctlb;
	int if_id;

	ccmni = wwan_netdev_drvpriv(dev);
	ctlb = ctxt;
	if_id = ccmni->index;

	if (if_id >= ARRAY_SIZE(ctlb->ccmni_inst))
		return;

	if (WARN_ON(ctlb->ccmni_inst[if_id] != ccmni))
		return;

	unregister_netdevice(dev);
}

static const struct wwan_ops ccmni_wwan_ops = {
	.priv_size = sizeof(struct ccmni_instance),
	.setup     = ccmni_wwan_setup,
	.newlink   = ccmni_wwan_newlink,
	.dellink   = ccmni_wwan_dellink,
};

static int ccmni_md_state_callback(enum md_state state, void *para)
{
	struct ccmni_ctl_block *ctlb;
	int ret = 0;

	ctlb = para;
	ctlb->md_sta = state;

	switch (state) {
	case MD_STATE_READY:
		ccmni_start(ctlb);
		break;

	case MD_STATE_EXCEPTION:
	case MD_STATE_STOPPED:
		ccmni_pre_stop(ctlb);
		ret = dpmaif_md_state_callback(ctlb->hif_ctrl, state);
		if (ret < 0)
			dev_err(ctlb->hif_ctrl->dev,
				"dpmaif md state callback err, md_sta=%d\n", state);

		ccmni_pos_stop(ctlb);
		break;

	case MD_STATE_WAITING_FOR_HS1:
	case MD_STATE_WAITING_TO_STOP:
		ret = dpmaif_md_state_callback(ctlb->hif_ctrl, state);
		if (ret < 0)
			dev_err(ctlb->hif_ctrl->dev,
				"dpmaif md state callback err, md_sta=%d\n", state);
		break;

	default:
		break;
	}

	return ret;
}

static void init_md_status_notifier(struct ccmni_ctl_block *ctlb)
{
	struct fsm_notifier_block *md_status_notifier;

	md_status_notifier = &ctlb->md_status_notify;
	INIT_LIST_HEAD(&md_status_notifier->entry);
	md_status_notifier->notifier_fn = ccmni_md_state_callback;
	md_status_notifier->data = ctlb;

	fsm_notifier_register(md_status_notifier);
}

static void ccmni_recv_skb(struct mtk_pci_dev *mtk_dev, int netif_id, struct sk_buff *skb)
{
	struct ccmni_instance *ccmni;
	struct net_device *dev;
	int pkt_type, skb_len;

	ccmni = mtk_dev->ccmni_ctlb->ccmni_inst[netif_id];
	if (!ccmni) {
		dev_kfree_skb(skb);
		return;
	}

	dev = ccmni->dev;

	pkt_type = skb->data[0] & SBD_PACKET_TYPE_MASK;
	ccmni_make_etherframe(dev, skb->data - ETH_HLEN, dev->dev_addr, pkt_type);
	skb_set_mac_header(skb, -ETH_HLEN);
	skb_reset_network_header(skb);
	skb->dev = dev;
	if (pkt_type == IPV6_VERSION)
		skb->protocol = htons(ETH_P_IPV6);
	else
		skb->protocol = htons(ETH_P_IP);

	skb_len = skb->len;

	netif_rx_any_context(skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;
}

static void ccmni_queue_tx_irq_notify(struct ccmni_ctl_block *ctlb, int qno)
{
	struct netdev_queue *net_queue;
	struct ccmni_instance *ccmni;

	ccmni = ctlb->ccmni_inst[0];

	if (netif_running(ccmni->dev) && atomic_read(&ccmni->usage) > 0) {
		if (ctlb->capability & NIC_CAP_CCMNI_MQ) {
			net_queue = netdev_get_tx_queue(ccmni->dev, qno);
			if (netif_tx_queue_stopped(net_queue))
				netif_tx_wake_queue(net_queue);
		} else if (netif_queue_stopped(ccmni->dev)) {
			netif_wake_queue(ccmni->dev);
		}
	}
}

static void ccmni_queue_tx_full_notify(struct ccmni_ctl_block *ctlb, int qno)
{
	struct netdev_queue *net_queue;
	struct ccmni_instance *ccmni;

	ccmni = ctlb->ccmni_inst[0];

	if (atomic_read(&ccmni->usage) > 0) {
		dev_err(&ctlb->mtk_dev->pdev->dev, "TX queue %d is full\n", qno);
		if (ctlb->capability & NIC_CAP_CCMNI_MQ) {
			net_queue = netdev_get_tx_queue(ccmni->dev, qno);
			netif_tx_stop_queue(net_queue);
		} else {
			netif_stop_queue(ccmni->dev);
		}
	}
}

static void ccmni_queue_state_notify(struct mtk_pci_dev *mtk_dev,
				     enum dpmaif_txq_state state, int qno)
{
	if (!(mtk_dev->ccmni_ctlb->capability & NIC_CAP_TXBUSY_STOP) ||
	    mtk_dev->ccmni_ctlb->md_sta != MD_STATE_READY ||
	    qno >= TXQ_TYPE_CNT)
		return;

	if (!mtk_dev->ccmni_ctlb->ccmni_inst[0]) {
		dev_warn(&mtk_dev->pdev->dev, "No netdev registered yet\n");
		return;
	}

	if (state == DMPAIF_TXQ_STATE_IRQ)
		ccmni_queue_tx_irq_notify(mtk_dev->ccmni_ctlb, qno);
	else if (state == DMPAIF_TXQ_STATE_FULL)
		ccmni_queue_tx_full_notify(mtk_dev->ccmni_ctlb, qno);
}

int ccmni_init(struct mtk_pci_dev *mtk_dev)
{
	struct ccmni_ctl_block *ctlb;
	int ret;

	ctlb = devm_kzalloc(&mtk_dev->pdev->dev, sizeof(*ctlb), GFP_KERNEL);
	if (!ctlb)
		return -ENOMEM;

	mtk_dev->ccmni_ctlb = ctlb;
	ctlb->mtk_dev = mtk_dev;
	ctlb->callbacks.state_notify = ccmni_queue_state_notify;
	ctlb->callbacks.recv_skb = ccmni_recv_skb;
	ctlb->nic_dev_num = NIC_DEV_DEFAULT;
	ctlb->capability = NIC_CAP_TXBUSY_STOP | NIC_CAP_SGIO |
			   NIC_CAP_DATA_ACK_DVD | NIC_CAP_CCMNI_MQ;

	ctlb->hif_ctrl = dpmaif_hif_init(mtk_dev, &ctlb->callbacks);
	if (!ctlb->hif_ctrl)
		return -ENOMEM;

	/* WWAN core will create a netdev for the default IP MUX channel */
	ret = wwan_register_ops(&ctlb->mtk_dev->pdev->dev, &ccmni_wwan_ops, ctlb,
				IP_MUX_SESSION_DEFAULT);
	if (ret)
		goto error_md;

	init_md_status_notifier(ctlb);

	return 0;

error_md:
	wwan_unregister_ops(&ctlb->mtk_dev->pdev->dev);

	return ret;
}

void ccmni_exit(struct mtk_pci_dev *mtk_dev)
{
	struct ccmni_ctl_block *ctlb;

	ctlb = mtk_dev->ccmni_ctlb;
	/* unregister FSM notifier */
	fsm_notifier_unregister(&ctlb->md_status_notify);
	wwan_unregister_ops(&ctlb->mtk_dev->pdev->dev);
	dpmaif_hif_exit(ctlb->hif_ctrl);
}
