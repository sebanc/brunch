// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "t7xx_hif_cldma.h"
#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_port_proxy.h"
#include "t7xx_skb_util.h"

#define MTK_DEV_NAME				"MTK_WWAN_M80"

#define PORT_NETLINK_MSG_MAX_PAYLOAD		32
#define PORT_NOTIFY_PROTOCOL			NETLINK_USERSOCK
#define PORT_STATE_BROADCAST_GROUP		21
#define CHECK_RX_SEQ_MASK			0x7fff
#define DATA_AT_CMD_Q				5

/* port->minor is configured in-sequence, but when we use it in code
 * it should be unique among all ports for addressing.
 */
#define TTY_IPC_MINOR_BASE			100
#define TTY_PORT_MINOR_BASE			250
#define TTY_PORT_MINOR_INVALID			-1

static struct port_proxy *port_prox;
static struct class *dev_class;

#define for_each_proxy_port(i, p, proxy)	\
	for (i = 0, (p) = &(proxy)->ports[i];	\
	     i < (proxy)->port_number;		\
	     i++, (p) = &(proxy)->ports[i])

static struct t7xx_port md_ccci_ports[] = {
	{CCCI_MD_LOG_TX, CCCI_MD_LOG_RX, 7, 7, 7, 7, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 2, "ttyCMdLog", WWAN_PORT_AT},
	{CCCI_LB_IT_TX, CCCI_LB_IT_RX, 0, 0, 0xff, 0xff, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 3, "ccci_lb_it",},
	{CCCI_MIPC_TX, CCCI_MIPC_RX, 2, 2, 0, 0, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &tty_port_ops, 1, "ttyCMIPC0",},
	{CCCI_MBIM_TX, CCCI_MBIM_RX, 2, 2, 0, 0, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &wwan_sub_port_ops, 10, "ttyCMBIM0", WWAN_PORT_MBIM},
	/*{CCCI_SAP_GNSS_TX, CCCI_SAP_GNSS_RX, 0, 0, 0, 0, ID_CLDMA0,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 6, "ccci_sap_gnss",},*/
	{CCCI_SAP_GNSS_TX, CCCI_SAP_GNSS_RX, 0, 0, 0, 0, ID_CLDMA0, 
	PORT_F_RX_CHAR_NODE, &wwan_sub_port_ops, 0, "ccci_sap_gnss", WWAN_PORT_AT},
	{CCCI_UART2_TX, CCCI_UART2_RX, DATA_AT_CMD_Q, DATA_AT_CMD_Q, 0xff,
	 0xff, ID_CLDMA1, PORT_F_RX_CHAR_NODE, &wwan_sub_port_ops, 0, "ttyC0", WWAN_PORT_AT},
   	{CCCI_SAP_META_TX, CCCI_SAP_META_RX, 1, 1, 0, 0, ID_CLDMA0,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 7, "ccci_sap_meta",},
	{CCCI_SAP_LOG_TX, CCCI_SAP_LOG_RX, 2, 2, 0, 0, ID_CLDMA0, 
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 8, "ccci_sap_log",},
    	{CCCI_SAP_ADB_TX, CCCI_SAP_ADB_RX, 3, 3, 0, 0, ID_CLDMA0, 
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 9, "ccci_sap_adb",},
	{CCCI_UART1_TX, CCCI_UART1_RX, 1, 1, 1, 1, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 11, "ttyCMdMeta",},
	{CCCI_DSS0_TX, CCCI_DSS0_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 13, "ttyCMBIMDSS0",},
	{CCCI_DSS1_TX, CCCI_DSS1_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 14, "ttyCMBIMDSS1",},
	{CCCI_DSS2_TX, CCCI_DSS2_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 15, "ttyCMBIMDSS2",},
	{CCCI_DSS3_TX, CCCI_DSS3_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 16, "ttyCMBIMDSS3",},
	{CCCI_DSS4_TX, CCCI_DSS4_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 17, "ttyCMBIMDSS4",},
	{CCCI_DSS5_TX, CCCI_DSS5_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 18, "ttyCMBIMDSS5",},
	{CCCI_DSS6_TX, CCCI_DSS6_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 19, "ttyCMBIMDSS6",},
	{CCCI_DSS7_TX, CCCI_DSS7_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_RX_CHAR_NODE, &char_port_ops, 20, "ttyCMBIMDSS7",},
	{CCCI_CONTROL_TX, CCCI_CONTROL_RX, 0, 0, 0, 0, ID_CLDMA1,
	 0, &ctl_port_ops, 0xff, "ccci_ctrl",},
    	{0xFFFF, 0xFFFF, 0, 0, 0, 0, ID_CLDMA0, PORT_F_RX_CHAR_NODE|PORT_F_RAW_DATA,
		&char_port_ops, 1, "brom_download",},
	{CCCI_SAP_CONTROL_TX, CCCI_SAP_CONTROL_RX, 0, 0, 0, 0, ID_CLDMA0, 0,
                &ctl_port_ops, 0xFF, "ccci_sap_ctrl",},
};

static int port_netlink_send_msg(struct t7xx_port *port, int grp,
				 const char *buf, size_t len)
{
	struct port_proxy *pprox;
	struct sk_buff *nl_skb;
	struct nlmsghdr *nlh;

	nl_skb = nlmsg_new(len, GFP_KERNEL);
	if (!nl_skb)
		return -ENOMEM;

	nlh = nlmsg_put(nl_skb, 0, 1, NLMSG_DONE, len, 0);
	if (!nlh) {
		dev_err(port->dev, "could not release netlink\n");
		nlmsg_free(nl_skb);
		return -EFAULT;
	}

	/* Add new netlink message to the skb
	 * after checking if header+payload
	 * can be handled.
	 */
	memcpy(nlmsg_data(nlh), buf, len);

	pprox = port->port_proxy;
	return netlink_broadcast(pprox->netlink_sock, nl_skb,
				 0, grp, GFP_KERNEL);
}

static int port_netlink_init(void)
{
	port_prox->netlink_sock = netlink_kernel_create(&init_net, PORT_NOTIFY_PROTOCOL, NULL);

	if (!port_prox->netlink_sock) {
		dev_err(port_prox->dev, "failed to create netlink socket\n");
		return -ENOMEM;
	}

	return 0;
}

static void port_netlink_uninit(void)
{
	if (port_prox->netlink_sock) {
		netlink_kernel_release(port_prox->netlink_sock);
		port_prox->netlink_sock = NULL;
	}
}

static struct t7xx_port *proxy_get_port(int minor, enum ccci_ch ch)
{
	struct t7xx_port *port;
	int i;

	if (!port_prox)
		return NULL;

	for_each_proxy_port(i, port, port_prox) {
		if (minor >= 0 && port->minor == minor)
			return port;

		if (ch != CCCI_INVALID_CH_ID && (port->rx_ch == ch || port->tx_ch == ch))
			return port;
	}

	return NULL;
}

struct t7xx_port *port_proxy_get_port(int major, int minor)
{
	if (port_prox && port_prox->major == major)
		return proxy_get_port(minor, CCCI_INVALID_CH_ID);

	return NULL;
}

static inline struct t7xx_port *port_get_by_ch(enum ccci_ch ch)
{
	return proxy_get_port(TTY_PORT_MINOR_INVALID, ch);
}

/* Sequence numbering to track for lost packets */
void port_proxy_set_seq_num(struct t7xx_port *port, struct ccci_header *ccci_h)
{
	if (ccci_h && port) {
		ccci_h->status &= ~HDR_FLD_SEQ;
		ccci_h->status |= FIELD_PREP(HDR_FLD_SEQ, port->seq_nums[MTK_OUT]);
		ccci_h->status &= ~HDR_FLD_AST;
		ccci_h->status |= FIELD_PREP(HDR_FLD_AST, 1);
	}
}

static u16 port_check_rx_seq_num(struct t7xx_port *port, struct ccci_header *ccci_h)
{
	u16 channel, seq_num, assert_bit;

	channel = FIELD_GET(HDR_FLD_CHN, ccci_h->status);
	seq_num = FIELD_GET(HDR_FLD_SEQ, ccci_h->status);
	assert_bit = FIELD_GET(HDR_FLD_AST, ccci_h->status);
	if (assert_bit && port->seq_nums[MTK_IN] &&
	    ((seq_num - port->seq_nums[MTK_IN]) & CHECK_RX_SEQ_MASK) != 1) {
		dev_err(port->dev, "channel %d seq number out-of-order %d->%d (data: %X, %X)\n",
			channel, seq_num, port->seq_nums[MTK_IN],
			ccci_h->data[0], ccci_h->data[1]);
	}

	return seq_num;
}

void port_proxy_reset(struct device *dev)
{
	struct t7xx_port *port;
	int i;

	if (!port_prox) {
		dev_err(dev, "invalid port proxy\n");
		return;
	}

	for_each_proxy_port(i, port, port_prox) {
		port->seq_nums[MTK_IN] = -1;
		port->seq_nums[MTK_OUT] = 0;
	}
}

static inline int port_get_queue_no(struct t7xx_port *port)
{
	return ccci_fsm_get_md_state() == MD_STATE_EXCEPTION ?
		port->txq_exp_index : port->txq_index;
}

static inline void port_struct_init(struct t7xx_port *port)
{
	INIT_LIST_HEAD(&port->entry);
	INIT_LIST_HEAD(&port->queue_entry);
	skb_queue_head_init(&port->rx_skb_list);
	init_waitqueue_head(&port->rx_wq);
	port->seq_nums[MTK_IN] = -1;
	port->seq_nums[MTK_OUT] = 0;
	atomic_set(&port->usage_cnt, 0);
	port->port_proxy = port_prox;
}

static void port_adjust_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h;

	ccci_h = (struct ccci_header *)skb->data;
	if (port->flags & PORT_F_USER_HEADER) { /* header provide by user */
		/* CCCI_MON_CH should fall in here, as header must be
		 * send to md_init.
		 */
		if (ccci_h->data[0] == CCCI_HEADER_NO_DATA) {
			if (skb->len > sizeof(struct ccci_header)) {
				dev_err_ratelimited(port->dev,
						    "recv unexpected data for %s, skb->len=%d\n",
						    port->name, skb->len);
				skb_trim(skb, sizeof(struct ccci_header));
			}
		}
	} else {
		/* remove CCCI header */
		skb_pull(skb, sizeof(struct ccci_header));
	}
}

/**
 * port_recv_skb() - receive skb from modem or HIF
 * @port: port to use
 * @skb: skb to use
 *
 * Used to receive native HIF RX data,
 * which have same RX receive flow.
 *
 * Return: 0 for success or error code
 */
int port_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	unsigned long flags;

	spin_lock_irqsave(&port->rx_wq.lock, flags);
	if (port->rx_skb_list.qlen < port->rx_length_th) {
		struct ccci_header *ccci_h = (struct ccci_header *)skb->data;

		port->flags &= ~PORT_F_RX_FULLED;
		if (port->flags & PORT_F_RX_ADJUST_HEADER)
			port_adjust_skb(port, skb);

		if (!(port->flags & PORT_F_RAW_DATA) &&
		    FIELD_GET(HDR_FLD_CHN, ccci_h->status) == CCCI_STATUS_RX) {
			port->skb_handler(port, skb);
		} else {
			if (port->mtk_wwan_port)
				wwan_port_rx(port->mtk_wwan_port, skb);
			else
				__skb_queue_tail(&port->rx_skb_list, skb);
		}

		spin_unlock_irqrestore(&port->rx_wq.lock, flags);
		wake_up_all(&port->rx_wq);
		return 0;
	}

	port->flags |= PORT_F_RX_FULLED;
	spin_unlock_irqrestore(&port->rx_wq.lock, flags);
	if (port->flags & PORT_F_RX_ALLOW_DROP) {
		dev_err(port->dev, "port %s RX full, drop packet\n", port->name);
		return -ENETDOWN;
	}

	return -ENOBUFS;
}

/**
 * port_kthread_handler() - kthread handler for specific port
 * @arg: port pointer
 *
 * Receive native HIF RX data,
 * which have same RX receive flow.
 *
 * Return: Always 0 to kthread_run
 */
int port_kthread_handler(void *arg)
{
	while (!kthread_should_stop()) {
		struct t7xx_port *port = arg;
		struct sk_buff *skb;
		unsigned long flags;

		spin_lock_irqsave(&port->rx_wq.lock, flags);
		if (skb_queue_empty(&port->rx_skb_list) &&
		    wait_event_interruptible_locked_irq(port->rx_wq,
							!skb_queue_empty(&port->rx_skb_list) ||
							kthread_should_stop())) {
			spin_unlock_irqrestore(&port->rx_wq.lock, flags);
			continue;
		}

		if (kthread_should_stop()) {
			spin_unlock_irqrestore(&port->rx_wq.lock, flags);
			break;
		}

		skb = __skb_dequeue(&port->rx_skb_list);
		spin_unlock_irqrestore(&port->rx_wq.lock, flags);

		if (port->skb_handler)
			port->skb_handler(port, skb);
	}

	return 0;
}

int port_write_room_to_md(struct t7xx_port *port)
{
	return cldma_write_room(port->path_id, port_get_queue_no(port));
}

int port_proxy_send_skb(struct t7xx_port *port, struct sk_buff *skb, bool from_pool)
{
	struct ccci_header *ccci_h;
	unsigned char tx_qno;
	int ret;

	ccci_h = (struct ccci_header *)(skb->data);
	tx_qno = port_get_queue_no(port);
	port_proxy_set_seq_num(port, (struct ccci_header *)ccci_h);
	ret = cldma_send_skb(port->path_id, tx_qno, skb, from_pool, true);
	if (ret) {
		dev_err(port->dev, "failed to send skb, error: %d\n", ret);
	} else {
		/* Record the port seq_num after the data is sent to HIF.
		 * Only bits 0-14 are used, thus negating overflow.
		 */
		port->seq_nums[MTK_OUT]++;
	}

	return ret;
}

int port_send_skb_to_md(struct t7xx_port *port, struct sk_buff *skb, bool blocking)
{
	enum md_state md_state;
	unsigned int fsm_state;

	md_state = ccci_fsm_get_md_state();
	fsm_state = ccci_fsm_get_current_state();
	if (fsm_state != CCCI_FSM_PRE_START) {
		if (md_state == MD_STATE_WAITING_FOR_HS1 ||
		    md_state == MD_STATE_WAITING_FOR_HS2) {
			return -ENODEV;
		}

		if (md_state == MD_STATE_EXCEPTION &&
		    port->tx_ch != CCCI_MD_LOG_TX &&
		    port->tx_ch != CCCI_UART1_TX) {
			return -ETXTBSY;
		}

		if (md_state == MD_STATE_STOPPED ||
		    md_state == MD_STATE_WAITING_TO_STOP ||
		    md_state == MD_STATE_INVALID) {
			pr_info("%s: md_state = %d fsm_state = %d \n", __func__, md_state, fsm_state);
			return -ENODEV;
		}
	}

	return cldma_send_skb(port->path_id, port_get_queue_no(port),
				   skb, port->skb_from_pool, blocking);
}

static void proxy_setup_channel_mapping(void)
{
	struct t7xx_port *port;
	int i, j;

	/* init RX_CH=>port list mapping */
	for (i = 0; i < ARRAY_SIZE(port_prox->rx_ch_ports); i++)
		INIT_LIST_HEAD(&port_prox->rx_ch_ports[i]);
	/* init queue_id=>port list mapping per HIF */
	for (j = 0; j < ARRAY_SIZE(port_prox->queue_ports); j++) {
		for (i = 0; i < ARRAY_SIZE(port_prox->queue_ports[j]); i++)
			INIT_LIST_HEAD(&port_prox->queue_ports[j][i]);
	}

	/* setup port mapping */
	for_each_proxy_port(i, port, port_prox) {
		/* setup RX_CH=>port list mapping */
		list_add_tail(&port->entry,
			      &port_prox->rx_ch_ports[port->rx_ch & CCCI_CH_ID_MASK]);
		/* setup QUEUE_ID=>port list mapping */
		list_add_tail(&port->queue_entry,
			      &port_prox->queue_ports[port->path_id][port->rxq_index]);
	}
}

/* inject CCCI message to modem */
void port_proxy_send_msg_to_md(int ch, unsigned int msg, unsigned int resv)
{
	struct ctrl_msg_header *ctrl_msg_h;
	struct ccci_header *ccci_h;
	struct t7xx_port *port;
	struct sk_buff *skb;
	int ret;

	port = port_get_by_ch(ch);
	if (!port)
		return;

	skb = ccci_alloc_skb_from_pool(&port->mtk_dev->pools, sizeof(struct ccci_header),
				       GFS_BLOCKING);
	if (!skb)
		return;

	if (ch == CCCI_CONTROL_TX) {
		ccci_h = (struct ccci_header *)(skb->data);
		ccci_h->data[0] = CCCI_HEADER_NO_DATA;
		ccci_h->data[1] = sizeof(struct ctrl_msg_header) + CCCI_H_LEN;
		ccci_h->status &= ~HDR_FLD_CHN;
		ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, ch);
		ccci_h->reserved = 0;
		ctrl_msg_h = (struct ctrl_msg_header *)(skb->data + CCCI_H_LEN);
		ctrl_msg_h->data_length = 0;
		ctrl_msg_h->reserved = resv;
		ctrl_msg_h->ctrl_msg_id = msg;
		skb_put(skb, CCCI_H_LEN + sizeof(struct ctrl_msg_header));
	} else {
		ccci_h = skb_put(skb, sizeof(struct ccci_header));
		ccci_h->data[0] = CCCI_HEADER_NO_DATA;
		ccci_h->data[1] = msg;
		ccci_h->status &= ~HDR_FLD_CHN;
		ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, ch);
		ccci_h->reserved = resv;
	}

	ret = port_proxy_send_skb(port, skb, port->skb_from_pool);
	if (ret) {
		dev_err(port->dev, "port%s send to MD fail\n", port->name);
		ccci_free_skb(&port->mtk_dev->pools, skb);
	}
}

/**
 * port_proxy_recv_skb_from_q() - receive raw data from dedicated queue
 * @queue: CLDMA queue
 * @skb: socket buffer
 *
 * Return: 0 for success or error code for drops
 */
static int port_proxy_recv_skb_from_q(struct cldma_queue *queue, struct sk_buff *skb)
{
	struct t7xx_port *port;
	int ret = 0;

	port = port_prox->dedicated_ports[queue->hif_id][queue->index];

	if (skb && port->ops->recv_skb)
		ret = port->ops->recv_skb(port, skb);

	if (ret < 0 && ret != -ENOBUFS) {
		dev_err(port->dev, "drop on RX ch %d, ret %d\n", port->rx_ch, ret);
		ccci_free_skb(&port->mtk_dev->pools, skb);
		return -ENETDOWN;
	}

	return ret;
}

/**
 * port_proxy_dispatch_recv_skb() - dispatch received skb
 * @queue: CLDMA queue
 * @skb: socket buffer
 *
 * If recv_request returns 0 or -ENETDOWN, then it's the port's duty
 * to free the request and the caller should no longer reference the request.
 * If recv_request returns any other error, caller should free the request.
 *
 * Return: 0 or greater for success, or negative error code
 */
static int port_proxy_dispatch_recv_skb(struct cldma_queue *queue, struct sk_buff *skb)
{
	struct list_head *port_list;
	struct ccci_header *ccci_h;
	struct t7xx_port *port;
	u16 seq_num, channel;
	int ret = -ENETDOWN;

	if (!skb)
		return -EINVAL;

	ccci_h = (struct ccci_header *)skb->data;
	channel = FIELD_GET(HDR_FLD_CHN, ccci_h->status);

	if (channel >= CCCI_MAX_CH_NUM ||
	    ccci_fsm_get_md_state() == MD_STATE_INVALID)
		goto err_exit;

	port_list = &port_prox->rx_ch_ports[channel & CCCI_CH_ID_MASK];
	list_for_each_entry(port, port_list, entry) {
		if (queue->hif_id != port->path_id || channel != port->rx_ch)
			continue;

		/* Multi-cast is not supported, because one port may be freed
		 * and can modify this request before another port can process it.
		 * However we still can use req->state to achieve some kind of
		 * multi-cast if needed.
		 */
		if (port->ops->recv_skb) {
			seq_num = port_check_rx_seq_num(port, ccci_h);
			ret = port->ops->recv_skb(port, skb);
			/* If the packet is stored to RX buffer
			 * successfully or drop, the sequence
			 * num will be updated.
			 */
			if (ret == -ENOBUFS)
				return ret;

			port->seq_nums[MTK_IN] = seq_num;
		}

		break;
	}

err_exit:
	if (ret < 0) {
		struct skb_pools *pools;

		pools = &queue->md->mtk_dev->pools;
		ccci_free_skb(pools, skb);
		return -ENETDOWN;
	}

	return 0;
}

static int port_proxy_recv_skb(struct cldma_queue *queue, struct sk_buff *skb)
{
	if (queue->q_type == CLDMA_SHARED_Q)
		return port_proxy_dispatch_recv_skb(queue, skb);

	return port_proxy_recv_skb_from_q(queue, skb);
}

/**
 * port_proxy_md_status_notify() - notify all ports of state
 * @state: state
 *
 * Called by ccci_fsm,
 * Used to dispatch modem status for all ports,
 * which want to know MD state transition.
 */
void port_proxy_md_status_notify(unsigned int state)
{
	struct t7xx_port *port;
	int i;

	if (!port_prox)
		return;

	for_each_proxy_port(i, port, port_prox)
		if (port->ops->md_state_notify)
			port->ops->md_state_notify(port, state);
}

static int proxy_register_char_dev(void)
{
	dev_t dev = 0;
	int ret;

	if (port_prox->major) {
		dev = MKDEV(port_prox->major, port_prox->minor_base);
		ret = register_chrdev_region(dev, TTY_IPC_MINOR_BASE, MTK_DEV_NAME);
	} else {
		ret = alloc_chrdev_region(&dev, port_prox->minor_base,
					  TTY_IPC_MINOR_BASE, MTK_DEV_NAME);
		if (ret)
			dev_err(port_prox->dev, "failed to alloc chrdev region, ret=%d\n", ret);

		port_prox->major = MAJOR(dev);
	}

	return ret;
}

static void proxy_init_all_ports(struct mtk_modem *md)
{
	struct t7xx_port *port;
	int i;

	for_each_proxy_port(i, port, port_prox) {
		port_struct_init(port);
		if (port->tx_ch == CCCI_CONTROL_TX) {
			port_prox->ctl_port = port;
			md->core_md.ctl_port = port;
		}

		if (port->tx_ch == CCCI_SAP_CONTROL_TX) {
                        md->core_sap.ctl_port = port;
		}

		port->major = port_prox->major;
		port->minor_base = port_prox->minor_base;
		port->mtk_dev = md->mtk_dev;
		port->dev = &md->mtk_dev->pdev->dev;
		spin_lock_init(&port->port_update_lock);
		spin_lock(&port->port_update_lock);
		mutex_init(&port->tx_mutex_lock);
		if (port->flags & PORT_F_CHAR_NODE_SHOW)
			port->chan_enable = CCCI_CHAN_ENABLE;
		else
			port->chan_enable = CCCI_CHAN_DISABLE;

		port->chn_crt_stat = CCCI_CHAN_DISABLE;
		spin_unlock(&port->port_update_lock);
		if (port->ops->init)
			port->ops->init(port);

		if (port->flags & PORT_F_RAW_DATA)
			port_prox->dedicated_ports[port->path_id][port->rxq_index] = port;
	}

	proxy_setup_channel_mapping();
}

static int proxy_alloc(struct mtk_modem *md)
{
	int ret;

	port_prox = devm_kzalloc(&md->mtk_dev->pdev->dev, sizeof(*port_prox), GFP_KERNEL);
	if (!port_prox)
		return -ENOMEM;

	ret = proxy_register_char_dev();
	if (ret)
		return ret;

	port_prox->dev = &md->mtk_dev->pdev->dev;
	port_prox->ports = md_ccci_ports;
	port_prox->port_number = ARRAY_SIZE(md_ccci_ports);
	proxy_init_all_ports(md);

	return 0;
};

struct t7xx_port *port_get_by_minor(int minor)
{
	return proxy_get_port(minor, CCCI_INVALID_CH_ID);
}

struct t7xx_port *port_get_by_name(char *port_name)
{
	struct t7xx_port *port;
	int i;

	if (!port_prox)
		return NULL;

	for_each_proxy_port(i, port, port_prox)
		if (!strncmp(port->name, port_name, strlen(port->name)))
			return port;

	return NULL;
}

int port_register_device(const char *name, int major, int minor)
{
	struct device *dev;

	dev = device_create(dev_class, NULL, MKDEV(major, minor), NULL, "%s", name);

	return IS_ERR(dev) ? PTR_ERR(dev) : 0;
}

void port_unregister_device(int major, int minor)
{
	device_destroy(dev_class, MKDEV(major, minor));
}

int port_proxy_broadcast_state(struct t7xx_port *port, int state)
{
	char msg[PORT_NETLINK_MSG_MAX_PAYLOAD];

	if (state >= MTK_PORT_STATE_INVALID)
		return -EINVAL;

	switch (state) {
	case MTK_PORT_STATE_ENABLE:
		snprintf(msg, PORT_NETLINK_MSG_MAX_PAYLOAD, "enable %s", port->name);
		break;

	case MTK_PORT_STATE_DISABLE:
		snprintf(msg, PORT_NETLINK_MSG_MAX_PAYLOAD, "disable %s", port->name);
		break;

	default:
		snprintf(msg, PORT_NETLINK_MSG_MAX_PAYLOAD, "invalid operation");
		break;
	}

	return port_netlink_send_msg(port, PORT_STATE_BROADCAST_GROUP, msg, strlen(msg) + 1);
}

/**
 * port_proxy_init() - init ports
 * @md: modem
 *
 * Called by CCCI modem,
 * used to create all CCCI port instances.
 *
 * Return: 0 for success or error
 */
int port_proxy_init(struct mtk_modem *md)
{
	int ret;

	dev_class = class_create(THIS_MODULE, "ccci_node");
	if (IS_ERR(dev_class))
		return PTR_ERR(dev_class);

	ret = proxy_alloc(md);
	if (ret)
		goto err_proxy;

	ret = port_netlink_init();
	if (ret)
		goto err_netlink;

	cldma_set_recv_skb(ID_CLDMA1, port_proxy_recv_skb);
	cldma_set_recv_skb(ID_CLDMA0, port_proxy_recv_skb);

	return 0;

err_proxy:
	class_destroy(dev_class);
err_netlink:
	port_proxy_uninit();

	return ret;
}

void port_proxy_uninit(void)
{
	struct t7xx_port *port;
	int i;

	for_each_proxy_port(i, port, port_prox)
		if (port->ops->uninit)
			port->ops->uninit(port);

	unregister_chrdev_region(MKDEV(port_prox->major, port_prox->minor_base),
				 TTY_IPC_MINOR_BASE);
	port_netlink_uninit();

	class_destroy(dev_class);
}

/**
 * port_proxy_node_control() - create/remove node
 * @dev: device
 * @port_msg: message
 *
 * Used to control create/remove device node.
 *
 * Return: 0 for success or error
 */
int port_proxy_node_control(struct device *dev, struct port_msg *port_msg)
{
	struct t7xx_port *port;
	unsigned int ports, i;
	unsigned int version;

	version = FIELD_GET(PORT_MSG_VERSION, port_msg->info);
	if (version != PORT_ENUM_VER ||
	    port_msg->head_pattern != PORT_ENUM_HEAD_PATTERN ||
	    port_msg->tail_pattern != PORT_ENUM_TAIL_PATTERN) {
		dev_err(dev, "port message enumeration invalid %x:%x:%x\n",
			version, port_msg->head_pattern, port_msg->tail_pattern);
		return -EFAULT;
	}

	ports = FIELD_GET(PORT_MSG_PRT_CNT, port_msg->info);

	for (i = 0; i < ports; i++) {
		u32 *port_info = (u32 *)(port_msg->data + sizeof(*port_info) * i);
		unsigned int en_flag = FIELD_GET(PORT_INFO_ENFLG, *port_info);
		unsigned int ch_id = FIELD_GET(PORT_INFO_CH_ID, *port_info);

		port = port_get_by_ch(ch_id);

		if (!port) {
			dev_warn(dev, "Port:%x not found\n", ch_id);
			continue;
		}

		if (ccci_fsm_get_md_state() == MD_STATE_READY) {
			if (en_flag == CCCI_CHAN_ENABLE) {
				if (port->ops->enable_chl)
					port->ops->enable_chl(port);
			} else {
				if (port->ops->disable_chl)
					port->ops->disable_chl(port);
			}
		} else {
			port->chan_enable = en_flag;
		}
	}

	return 0;
}
