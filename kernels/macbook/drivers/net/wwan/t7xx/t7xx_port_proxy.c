// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "t7xx_dev_node.h"
#include "t7xx_buffer_manager.h"
#include "t7xx_modem_ops.h"
#include "t7xx_monitor.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_port_proxy.h"
#include "t7xx_port_config.h"

#define CCCI_DEV_NAME "CCCI_PORT"
#define PORT_NETLINK_MSG_MAX_PAYLOAD		64
#define PORT_NOTIFY_PROTOCOL			NETLINK_USERSOCK
#define PORT_STATE_BROADCAST_GROUP		21
#define BUFF_LEN				20
#define CHECK_RX_SEQ_MASK			0x7FFF

static struct port_proxy *port_prox;

static ssize_t port_add_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "ccci port add\n");
}

static ssize_t port_add_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct t7xx_port *port;
	char arr_buf[BUFF_LEN] = {0};
	char *port_buf;
	int ret;
	int i;

	if (strlen(buf) >= BUFF_LEN) {
		pr_err("Input string too large: %zu >= %d\n", strlen(buf), BUFF_LEN);
		return 0;
	}

	strncpy(arr_buf, buf, BUFF_LEN - 1);
	port_buf = arr_buf;
	port_buf = strsep(&port_buf, "\r\n");
	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		ret = strcmp(port->name, port_buf);
		if (ret == 0 && port->ops->enable_chl)
			port->ops->enable_chl(port);
	}
	return count;
}

static ssize_t port_remove_show(struct device *dev, struct device_attribute *attr,
				char *buf)

{
	return sprintf(buf, "ccci port remove\n");
}

static ssize_t port_remove_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct t7xx_port *port;
	char arr_buf[BUFF_LEN] = {0};
	char *port_buf;
	int ret;
	int i;

	if (strlen(buf) >= BUFF_LEN) {
		pr_err("Input string too large: %lu >= %d\n", strlen(buf), BUFF_LEN);
		return 0;
	}

	strncpy(arr_buf, buf, BUFF_LEN - 1);
	port_buf = arr_buf;
	port_buf = strsep(&port_buf, "\r\n");
	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		ret = strcmp(port->name, port_buf);
		if (ret == 0 && port->ops->disable_chl)
			port->ops->disable_chl(port);
	}

	return count;
}

static ssize_t port_status_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct t7xx_port *port;
	int count = 0;
	int i;

	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		if (port->chn_crt_stat)
			count += snprintf((buf + count), (count + 32),
				"%s, Enable\n", port->name);
		else
			count += snprintf((buf + count), (count + 32),
				"%s, Disable\n", port->name);
	}

	return count;
}

static int port_netlink_send_msg(struct t7xx_port *port, int grp,
				 const char *buf, int len)
{
	struct port_proxy *pprox;
	struct sk_buff *nl_skb;
	struct nlmsghdr *nlh;
	int err;

	nl_skb = nlmsg_new(len, GFP_KERNEL);
	if (!nl_skb) {
		pr_err("Could not alloc netlink msg\n");
		return -EFAULT;
	}

	nlh = nlmsg_put(nl_skb, 0, 1, NLMSG_DONE, len, 0);
	if (!nlh) {
		pr_err("Could not release netlink\n");
		nlmsg_free(nl_skb);
		return -EFAULT;
	}

	memcpy(nlmsg_data(nlh), buf, len);

	pprox = port->port_proxy;
	err = netlink_broadcast(pprox->netlink_sock, nl_skb,
				0, grp, GFP_KERNEL);
	return err;
}

static int port_netlink_init(void)
{
	port_prox->netlink_sock = netlink_kernel_create(&init_net,
							PORT_NOTIFY_PROTOCOL,
							NULL);

	if (!port_prox->netlink_sock) {
		pr_err("failed to create netlink socket\n");
		return -EAGAIN;
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

static DEVICE_ATTR(port_remove, 0660, &port_remove_show, &port_remove_store);
static DEVICE_ATTR(port_status, 0660, &port_status_show, NULL);
static DEVICE_ATTR(port_add, 0660, &port_add_show, &port_add_store);

static struct attribute *ccci_ports_default_attrs[] = {
	&dev_attr_port_add.attr,
	&dev_attr_port_remove.attr,
	&dev_attr_port_status.attr,
	NULL
};

static struct attribute_group ccci_port_group = {
	.name = "ports",
	.attrs = ccci_ports_default_attrs,
};

static struct t7xx_port *proxy_get_port(int minor, enum ccci_ch ch)
{
	struct t7xx_port *port;
	int i;

	if (!port_prox)
		return NULL;

	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		if (minor >= 0 && port->minor == minor)
			return port;
		if (ch != CCCI_INVALID_CH_ID &&
		    (port->rx_ch == ch || port->tx_ch == ch))
			return port;
	}
	return NULL;
}

static inline void proxy_set_critical_user(int user_id, int enabled)
{
	if (enabled)
		port_prox->critical_user_active |= BIT(user_id);
	else
		port_prox->critical_user_active &= ~BIT(user_id);
}

static inline int proxy_get_critical_user(int user_id)
{
	return (port_prox->critical_user_active & BIT(user_id)) >> user_id;
}

static struct t7xx_port *port_get_by_node(int major, int minor)
{
	if (port_prox && port_prox->major == major)
		return proxy_get_port(minor, CCCI_INVALID_CH_ID);

	return NULL;
}

/* Sequence number help to check lose packets */
void port_inc_tx_seq_num(struct t7xx_port *port, struct ccci_header *ccci_h)
{
	if (ccci_h && port) {
		ccci_h->seq_num = port->seq_nums[MTK_OUT];
		ccci_h->assert_bit = 1;
	}
}

static inline void port_update_rx_seq_num(struct t7xx_port *port, u16 seq_num)
{
	port->seq_nums[MTK_IN] = seq_num;
}

static u16 port_check_rx_seq_num(struct t7xx_port *port, struct ccci_header *ccci_h)
{
	u16 channel, seq_num, assert_bit;

	channel = ccci_h->channel;
	seq_num = ccci_h->seq_num;
	assert_bit = ccci_h->assert_bit;
	if (assert_bit && port->seq_nums[MTK_IN] &&
	    ((seq_num - port->seq_nums[MTK_IN]) & CHECK_RX_SEQ_MASK) != 1) {
		pr_err("channel %d seq number out-of-order %d->%d (data: %X, %X)\n",
		       channel, seq_num, port->seq_nums[MTK_IN],
		       ccci_h->data[0], ccci_h->data[1]);
	}

	return seq_num;
}

void port_reset(void)
{
	struct t7xx_port *port;
	int port_index;

	if (!port_prox) {
		pr_crit("invalid port proxy\n");
		return;
	}

	for (port_index = 0; port_index < port_prox->port_number; port_index++) {
		port = port_prox->ports + port_index;
		port->tx_busy_count = 0;
		port->rx_busy_count = 0;
		port->seq_nums[MTK_IN] = -1;
		port->seq_nums[MTK_OUT] = 0;
	}
}

static void port_user_register(struct t7xx_port *port)
{
	int rx_ch = port->rx_ch;

	if (rx_ch == CCCI_UART2_RX)
		proxy_set_critical_user(CRIT_USR_MUXD, 1);
	if (rx_ch == CCCI_MD_LOG_RX)
		proxy_set_critical_user(CRIT_USR_MDLOG, 1);
	if (rx_ch == CCCI_UART1_RX)
		proxy_set_critical_user(CRIT_USR_META, 1);
}

static void port_user_unregister(struct t7xx_port *port)
{
	int rx_ch = port->rx_ch;

	if (!port_prox) {
		pr_err("proxy has been free\n");
		return;
	}

	if (rx_ch == CCCI_UART2_RX)
		proxy_set_critical_user(CRIT_USR_MUXD, 0);
	if (rx_ch == CCCI_MD_LOG_RX)
		proxy_set_critical_user(CRIT_USR_MDLOG, 0);
	if (rx_ch == CCCI_UART1_RX)
		proxy_set_critical_user(CRIT_USR_META, 0);
}

static inline int port_get_queue_no(struct t7xx_port *port)
{
	return ccci_fsm_get_md_state() == EXCEPTION ? port->txq_exp_index : port->txq_index;
}

/* REGION: default char device operation definition for node,
 * which export node for userspace
 */
int port_dev_open(struct inode *inode, struct file *file)
{
	int major = imajor(inode);
	int minor = iminor(inode);
	struct t7xx_port *port;

	port = port_get_by_node(major, minor);
	if (!port) {
		pr_err("Invalid Arguments. port is null");
		return -EINVAL;
	}

	atomic_inc(&port->usage_cnt);
	file->private_data = port;
	nonseekable_open(inode, file);
	port_user_register(port);
	return 0;
}

int port_dev_close(struct inode *inode, struct file *file)
{
	struct t7xx_port *port = file->private_data;
	struct sk_buff *skb;
	unsigned long flags;
	int clear_cnt = 0;

	/* 0. decrease usage count, so when we ask more,
	 * the packet can be dropped in recv_request
	 */
	atomic_dec(&port->usage_cnt);
	/* 1. purge Rx request list */
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL) {
		ccci_free_skb(skb);
		clear_cnt++;
	}
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	port_user_unregister(port);

	return 0;
}

ssize_t port_dev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct t7xx_port *port = file->private_data;
	struct sk_buff *skb;
	int ret = 0, read_len = 0, full_req_done = 0;
	unsigned long flags;
	int not_empty;

read_start:

	if (skb_queue_empty(&port->rx_skb_list)) {
		if (!(file->f_flags & O_NONBLOCK)) {
			spin_lock_irq(&port->rx_wq.lock);
			not_empty = !skb_queue_empty(&port->rx_skb_list);
			ret = wait_event_interruptible_locked_irq(port->rx_wq,
								  not_empty);
			spin_unlock_irq(&port->rx_wq.lock);
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}

	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	if (skb_queue_empty(&port->rx_skb_list)) {
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		if (!(file->f_flags & O_NONBLOCK)) {
			goto read_start;
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}

	skb = skb_peek(&port->rx_skb_list);
	read_len = skb->len;

	if (count >= read_len) {
		full_req_done = 1;
		__skb_unlink(skb, &port->rx_skb_list);
	} else {
		read_len = count;
	}
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (copy_to_user(buf, skb->data, read_len)) {
		pr_err("read on %s, copy to user failed, %d/%zu\n",
		       port->name, read_len, count);
		ret = -EFAULT;
	}
	skb_pull(skb, read_len);
	if (full_req_done)
		ccci_free_skb(skb);

 exit:
	return ret ? ret : read_len;
}

ssize_t port_dev_write(struct file *file,
		       const char __user *buf, size_t count, loff_t *ppos)
{
	struct t7xx_port *port = file->private_data;
	struct ccci_header *ccci_h = NULL;
	struct sk_buff *skb;
	unsigned char blocking = !(file->f_flags & O_NONBLOCK);
	size_t actual_count, alloc_size, txq_mtu;
	int i, multi_packet = 1;
	int md_state;
	int ret;

	if (count == 0)
		return -EINVAL;

	md_state = ccci_fsm_get_md_state();
	if (md_state == BOOT_WAITING_FOR_HS1 ||
	    md_state == BOOT_WAITING_FOR_HS2) {
		pr_err("port %s ch%d write fail when md_state=%d\n",
		       port->name, port->tx_ch, md_state);
		return -ENODEV;
	}

	if (port_write_room_to_md(port) <= 0 && !blocking)
		return -EAGAIN;

	txq_mtu = ccci_cldma_txq_mtu(port->txq_index);
	if (port->flags & PORT_F_RAW_DATA || port->flags & PORT_F_USER_HEADER) {
		if (port->flags & PORT_F_USER_HEADER && count > txq_mtu) {
			pr_err("packet size %zu larger than MTU on %s\n",
			       count, port->name);
			return -ENOMEM;
		}
		actual_count = count > txq_mtu ? txq_mtu : count;
		alloc_size = actual_count;
	} else {
		actual_count = (count + CCCI_H_ELEN) > txq_mtu ?
			(txq_mtu - CCCI_H_ELEN) : count;
		alloc_size = actual_count + CCCI_H_ELEN;
		if ((count + CCCI_H_ELEN) > txq_mtu && (port->tx_ch == CCCI_MBIM_TX ||
							(port->tx_ch >= CCCI_DSS0_TX &&
							 port->tx_ch <= CCCI_DSS7_TX))) {
			multi_packet = (count + txq_mtu - CCCI_H_ELEN - 1) /
					(txq_mtu - CCCI_H_ELEN);
		}
	}
	/* loop start */
	for (i = 0; i < multi_packet; i++) {
		if (multi_packet > 1 && multi_packet == i + 1) {
			actual_count = count % (txq_mtu - CCCI_H_ELEN);
			alloc_size = actual_count + CCCI_H_ELEN;
	}
	skb = ccci_alloc_skb(alloc_size, 1, blocking);
	if (!skb)
		return -EBUSY; /* consider this case as non-blocking */

	if (port->flags & PORT_F_RAW_DATA) {
		ret = copy_from_user(skb_put(skb, actual_count),
				     buf, actual_count);
		if (port->flags & PORT_F_USER_HEADER) {
		/* ccci_header provided by user,
		 * For only send ccci_header without additional data
		 * case, data[0]=CCCI_MAGIC_NUM, data[1]=user_data,
		 * ch=tx_channel, reserved=no_use For send ccci_header
		 * with additional data case, data[0]=0,
		 * data[1]=data_size, ch=tx_channel, reserved=user_data
		 */
			ccci_h = (struct ccci_header *)skb->data;
			if (actual_count == CCCI_H_LEN)
				ccci_h->data[0] = CCCI_MAGIC_NUM;
			else
				ccci_h->data[1] = actual_count;

			ccci_h->channel = port->tx_ch;
		}
	} else {
		/* ccci_header is provided by driver */
		ccci_h = (struct ccci_header *)skb_put(skb, CCCI_H_LEN);
		ccci_h->data[0] = 0;
		ccci_h->data[1] = actual_count + CCCI_H_LEN;
		ccci_h->channel = port->tx_ch;
		ccci_h->reserved = 0;

		ret = copy_from_user(skb_put(skb, actual_count),
				     buf + i * (txq_mtu - CCCI_H_ELEN), actual_count);
	}
	if (ret) {
		pr_err("copy_from_user error\n");
		goto err_out;
	}

	/* send out */
	port_inc_tx_seq_num(port, ccci_h);
	ret = port_send_skb_to_md(port, skb, blocking);
	if (ret) {
		if (ret == -EBUSY && !blocking)
			ret = -EAGAIN;
		goto err_out;
	} else {
		/* record the port seq_num after the data is sent to HIF*/
		port->seq_nums[MTK_OUT]++;
	}
	if (multi_packet == 1)
		return actual_count;
	else if (multi_packet == i + 1)
		return count;
	/* consider this case as non-blocking */
	return -EBUSY;

err_out:
	pr_err("write error done on %s, size=%zu, ret=%d\n",
	       port->name, actual_count, ret);
	ccci_free_skb(skb);
	}
	return ret;
}

/*****************************************************************************/
/* REGION: port common API implementation,*/
/* these APIs are valuable for every port */
/*****************************************************************************/
static inline void port_struct_init(struct t7xx_port *port)
{
	INIT_LIST_HEAD(&port->entry);
	INIT_LIST_HEAD(&port->queue_entry);
	skb_queue_head_init(&port->rx_skb_list);
	init_waitqueue_head(&port->rx_wq);
	port->tx_busy_count = 0;
	port->rx_busy_count = 0;
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
		 * send to md_init
		 */
		if (ccci_h->data[0] == CCCI_MAGIC_NUM) {
			if (skb->len > sizeof(struct ccci_header)) {
				pr_err("recv unexpected data for %s, skb->len=%d\n",
				       port->name, skb->len);
				skb_trim(skb, sizeof(struct ccci_header));
			}
		}
	} else {
		/* remove CCCI header */
		skb_pull(skb, sizeof(struct ccci_header));
	}
}

/* This API is used to receive raw data from dedicated queue */
int port_recv_skb_from_dedicatedQ(enum cldma_id hif_id,
				  unsigned char qno, struct sk_buff *skb)
{
	struct t7xx_port *port;
	int ret = 0;

	port = port_prox->dedicated_ports[hif_id][qno];

	if (skb && port->ops->recv_skb)
		ret = port->ops->recv_skb(port, skb);

	if (ret < 0) {
		if (ret != -ERXFULL) {
			pr_err("drop on rx ch %d, ret %d\n", port->rx_ch, ret);
			ccci_free_skb(skb);
			ret = -EDROPPACKET;
		}
	}
	return ret;
}

/* This API is the common API for port to receive skb from modem or HIF */
int port_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	unsigned long flags;

	spin_lock_irqsave(&port->rx_skb_list.lock, flags);

	if (port->rx_skb_list.qlen < port->rx_length_th) {
		port->flags &= ~PORT_F_RX_FULLED;
		if (port->flags & PORT_F_ADJUST_HEADER)
			port_adjust_skb(port, skb);
		if (!(port->flags & PORT_F_RAW_DATA) &&
		    ccci_h->channel == CCCI_STATUS_RX) {
			port->skb_handler(port, skb);
		} else {
			if (port->mtk_wwan_port)
				wwan_port_rx(port->mtk_wwan_port, skb);
			else
				__skb_queue_tail(&port->rx_skb_list, skb);
		}
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		spin_lock_irqsave(&port->rx_wq.lock, flags);
		wake_up_all_locked(&port->rx_wq);
		spin_unlock_irqrestore(&port->rx_wq.lock, flags);

		return 0;
	}

	port->flags |= PORT_F_RX_FULLED;
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (port->flags & PORT_F_ALLOW_DROP) {
		pr_err("port %s Rx full, drop packet\n", port->name);
		goto drop;
	} else {
		return -ERXFULL;
	}

 drop:
	return -EDROPPACKET;
}

int port_kthread_handler(void *arg)
{
	struct t7xx_port *port = arg;
	struct sk_buff *skb;
	unsigned long flags;
	int ret;

	while (1) {
		if (skb_queue_empty(&port->rx_skb_list)) {
			ret = wait_event_interruptible(port->rx_wq,
						       (!skb_queue_empty(&port->rx_skb_list) ||
							kthread_should_stop()));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;

		/* 1. dequeue */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		skb = __skb_dequeue(&port->rx_skb_list);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);

		/* 2. process port skb */
		if (port->skb_handler)
			port->skb_handler(port, skb);
	}
	return 0;
}

int port_write_room_to_md(struct t7xx_port *port)
{
	return ccci_cldma_write_room(port->path_id, port_get_queue_no(port));
}

int ccci_port_send_hif(struct t7xx_port *port, struct sk_buff *skb,
		       unsigned char from_pool, unsigned char blocking)
{
	struct ccci_header *ccci_h;
	unsigned char tx_qno;
	int ret;

	ccci_h = (struct ccci_header *)(skb->data);
	tx_qno = port_get_queue_no(port);
	port_inc_tx_seq_num(port, ccci_h);
	ret = ccci_cldma_send_skb(port->path_id, tx_qno, skb, from_pool, blocking);
	if (ret)
		pr_err("failed to send skb,ret = %d\n", ret);
	else
		/* record the port seq_num after the data is sent to HIF */
		port->seq_nums[MTK_OUT]++;

	return ret;
}

int port_send_skb_to_md(struct t7xx_port *port, struct sk_buff *skb, int blocking)
{
	unsigned int md_state, fsm_state;
	int tx_qno;
	int err;

	md_state = ccci_fsm_get_md_state();
	fsm_state = ccci_fsm_get_current_state();
	if (fsm_state != CCCI_FSM_PRE_STARTING) {
		if (md_state == BOOT_WAITING_FOR_HS1 ||
		    md_state == BOOT_WAITING_FOR_HS2) {
			err = -ENODEV;
			goto exit;
		}
		if (md_state == EXCEPTION &&
		    port->tx_ch != CCCI_MD_LOG_TX &&
		    port->tx_ch != CCCI_UART1_TX) {
			err = -ETXTBSY;
			goto exit;
		}
		if (md_state == STOPPED ||
		    md_state == WAITING_TO_STOP ||
		    md_state == INVALID) {
			err = -ENODEV;
			goto exit;
		}
	}
	tx_qno = port_get_queue_no(port);

	err = ccci_cldma_send_skb(port->path_id, tx_qno, skb, port->skb_from_pool, blocking);
	if (err)
		goto exit;

	return 0;

exit:
	pr_err("failed to send packet via port:%s, ch:%d, md state:%d\n",
	       port->name, port->tx_ch, md_state);
	return err;
}

/*****************************************************************************/
/* REGION: port_proxy class method implementation */
/*****************************************************************************/
static void proxy_setup_channel_mapping(void)
{
	struct t7xx_port *port;
	int i, hif;

	/* Init RX_CH=>port list mapping */
	for (i = 0; i < ARRAY_SIZE(port_prox->rx_ch_ports); i++)
		INIT_LIST_HEAD(&port_prox->rx_ch_ports[i]);
	/* Init queue_id=>port list mapping per HIF */
	for (hif = 0; hif < ARRAY_SIZE(port_prox->queue_ports); hif++) {
		for (i = 0; i < ARRAY_SIZE(port_prox->queue_ports[hif]); i++)
			INIT_LIST_HEAD(&port_prox->queue_ports[hif][i]);
	}

	/* setup port mapping */
	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		/* setup RX_CH=>port list mapping */
		list_add_tail(&port->entry,
			      &port_prox->rx_ch_ports[port->rx_ch & CCCI_CH_ID_MASK]);
		/* setup QUEUE_ID=>port list mapping */
		list_add_tail(&port->queue_entry,
			      &port_prox->queue_ports[port->path_id][port->rxq_index]);
	}
}

/* kernel inject CCCI message to modem. */
void proxy_send_msg_to_md(int ch, unsigned int msg, unsigned int resv, int blocking)
{
	struct ctrl_msg_header *ctrl_msg_h;
	struct ccci_header *ccci_h;
	struct t7xx_port *port;
	struct sk_buff *skb;
	int ret;

	if (!port_prox) {
		pr_err("port_prox is NULL\n");
		return;
	}

	port = proxy_get_port(-1, ch);
	if (!port)
		return;

	skb = ccci_alloc_skb(sizeof(struct ccci_header),
			     port->skb_from_pool, blocking);
	if (!skb)
		return;

	if (ch == CCCI_CONTROL_TX) {
		ccci_h = (struct ccci_header *)(skb->data);
		ccci_h->data[0] = CCCI_MAGIC_NUM;
		ccci_h->data[1] = sizeof(struct ctrl_msg_header) + CCCI_H_LEN;
		ccci_h->channel = ch;
		ccci_h->reserved = 0;
		ctrl_msg_h = (struct ctrl_msg_header *)(skb->data + CCCI_H_LEN);
		ctrl_msg_h->data_length = 0;
		ctrl_msg_h->reserved = resv;
		ctrl_msg_h->ctrl_msg_id = msg;
		skb_put(skb, CCCI_H_LEN + sizeof(struct ctrl_msg_header));
	} else {
		ccci_h = (struct ccci_header *)skb_put(skb,
			sizeof(struct ccci_header));
		ccci_h->data[0] = CCCI_MAGIC_NUM;
		ccci_h->data[1] = msg;
		ccci_h->channel = ch;
		ccci_h->reserved = resv;
	}

	ret = ccci_port_send_hif(port, skb, port->skb_from_pool, blocking);
	if (ret) {
		pr_err("port%s send to md fail\n", port->name);
		ccci_free_skb(skb);
	}
}

/* if recv_request returns 0 or -EDROPPACKET, then it's port's duty
 * to free the request, and caller should NOT reference the request any more
 * but if it returns other error, caller should be responsible to
 * free the request.
 */
int proxy_dispatch_recv_skb(enum cldma_id hif_id, struct sk_buff *skb)
{
	struct list_head *port_list;
	struct ccci_header *ccci_h;
	struct t7xx_port *port;
	u16 seq_num, channel;
	char matched = '\0';
	int ret = -EINVAL;

	if (!skb)
		return -EINVAL;

	ccci_h = (struct ccci_header *)skb->data;
	channel = ccci_h->channel;

	if (channel >= CCCI_MAX_CH_NUM)
		goto err_exit;

	if (ccci_fsm_get_md_state() == INVALID) {
		ret = -ENODEV;
		goto err_exit;
	}
	/* bit 15~12 is used as peer_id */
	port_list = &port_prox->rx_ch_ports[channel & CCCI_CH_ID_MASK];
	list_for_each_entry(port, port_list, entry) {
		/* Multi-cast is not supported, because one port may be freed
		 * and can modify this request before another port can process it.
		 * However we still can use req->state to achieve some kind of
		 * multi-cast if needed.
		 */
		matched = (hif_id == port->path_id) &&
			  ((!port->ops->recv_match) ?
			   (channel == port->rx_ch) : port->ops->recv_match(port, skb));
		if (matched) {
			if (port->ops->recv_skb) {
				seq_num = port_check_rx_seq_num(port, ccci_h);
				ret = port->ops->recv_skb(port, skb);
				/* If the packet is stored to rx buffer
				 * successfully or drop, the sequence
				 * num will be updated
				 */
				if (ret != -ERXFULL) {
					port_update_rx_seq_num(port, seq_num);
				} else {
					port->rx_busy_count++;
					return ret;
				}
			} else {
				pr_err("port->ops->recv_skb is null\n");
				ret = -EINVAL;
			}
			break;
		}
	}

err_exit:
	if (ret < 0) {
		ccci_free_skb(skb);
		ret = -EDROPPACKET;
	}

	return ret;
}

static void proxy_dispatch_md_status(unsigned int state)
{
	struct t7xx_port *port;
	int i;

	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		if (port->ops->md_state_notify)
			port->ops->md_state_notify(port, state);
	}
}

static int proxy_register_char_dev(void)
{
	char tmp_buf[30];
	dev_t dev = 0;
	int ret;

	snprintf(tmp_buf, 30, CCCI_DEV_NAME);
	if (port_prox->major) {
		dev = MKDEV(port_prox->major, port_prox->minor_base);
		ret = register_chrdev_region(dev, CCCI_IPC_MINOR_BASE, tmp_buf);
	} else {
		ret = alloc_chrdev_region(&dev, port_prox->minor_base,
					  CCCI_IPC_MINOR_BASE, tmp_buf);
		if (ret)
			pr_err("failed to alloc chrdev region,ret=%d\n", ret);
		port_prox->major = MAJOR(dev);
	}
	return ret;
}

static void proxy_init_all_ports(struct ccci_modem *md)
{
	struct t7xx_port *port;
	int i;

	if (!md) {
		pr_err("modem sys is null\n");
		return;
	}

	/* init port */
	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		port_struct_init(port);
		if (port->tx_ch == CCCI_CONTROL_TX) {
			port_prox->ctl_port = port;
			md->core_md.ctl_port = port;
		}

		port->major = port_prox->major;
		port->minor_base = port_prox->minor_base;
		port->dev = &md->mtk_dev->pdev->dev;
		spin_lock_init(&port->port_update_lock);
		spin_lock(&port->port_update_lock);
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

static void proxy_alloc(struct ccci_modem *md)
{
	int ret;

	/* Allocate port_proxy obj and set all member zero */
	port_prox = kzalloc(sizeof(*port_prox), GFP_KERNEL);
	if (!port_prox)
		return;

	ret = proxy_register_char_dev();
	if (ret)
		goto err_register;
	port_prox->port_number = port_get_cfg(&port_prox->ports);
	if (port_prox->port_number > 0 && port_prox->ports)
		proxy_init_all_ports(md);
	else
		ret = -1;

err_register:
	if (ret) {
		kfree(port_prox);
		port_prox = NULL;
		pr_err("fail to get md port config,ret=%d\n", ret);
	}
	return;
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

	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		if (strncmp(port->name, port_name, strlen(port->name)) == 0)
			return port;
	}
	return NULL;
}

int port_broadcast_state(struct t7xx_port *port, int state)
{
	char msg[PORT_NETLINK_MSG_MAX_PAYLOAD];
	int err;

	if (state >= PORT_STATE_INVALID)
		return -EINVAL;

	switch (state) {
	case PORT_STATE_ENABLE:
		snprintf(msg, PORT_NETLINK_MSG_MAX_PAYLOAD,
			 "enable %s", port->name);
		break;

	case PORT_STATE_DISABLE:
		snprintf(msg, PORT_NETLINK_MSG_MAX_PAYLOAD,
			 "disable %s", port->name);
		break;

	default:
		snprintf(msg, PORT_NETLINK_MSG_MAX_PAYLOAD,
			 "invalid operation");
		break;
	}

	err = port_netlink_send_msg(port, PORT_STATE_BROADCAST_GROUP,
				    msg, strlen(msg) + 1);
	return err;
}

/* This API is called by ccci modem,
 * and used to create all ccci port instances
 */
int ccci_port_init(struct ccci_modem *md)
{
	struct device *dev;
	int err;

	dev = &md->mtk_dev->pdev->dev;
	proxy_alloc(md);
	if (!port_prox) {
		dev_err(dev, "failed to alloc port_proxy\n");
		return -ENOMEM;
	}

	err = port_netlink_init();
	if (err)
		dev_err(dev, "failed to init netlink\n");
	err = devm_device_add_group(dev, &ccci_port_group);
	if (err) {
		dev_err(dev, "failed to init port proxy sysfs: %d\n", err);
		return err;
	}
	return 0;
}

int ccci_port_uninit(void)
{
	struct t7xx_port *port;
	int i;

	if (!port_prox) {
		pr_notice("port_status notify: proxy not initiated\n");
		return -EFAULT;
	}

	/* port uninit */
	for (i = 0; i < port_prox->port_number; i++) {
		port = port_prox->ports + i;
		if (port->ops->uninit)
			port->ops->uninit(port);
	}
	/* free the char device number */
	unregister_chrdev_region(MKDEV(port_prox->major, port_prox->minor_base),
				 CCCI_IPC_MINOR_BASE);
	port_netlink_uninit();
	kfree_sensitive(port_prox);
	port_prox = NULL;
	return 0;
}

/* This API is called by ccci_fsm,
 * and used to dispatch modem status for all ports,
 * which want to know md state transition.
 */
void ccci_port_md_status_notify(unsigned int state)
{
	if (!port_prox)
		pr_notice("port_status notify: proxy not initiated\n");
	else
		proxy_dispatch_md_status(state);
}

/* This API is called by ccci fsm,
 * and used to get critical user status.
 */
int ccci_port_get_critical_user(unsigned int user_id)
{
	return proxy_get_critical_user(user_id);
}

/* This API is used to control create/remove device node */
int ccci_port_node_control(void *data)
{
	struct port_msg *port_msg = (struct port_msg *)data;
	struct port_info *port_info;
	struct t7xx_port *port;
	u16 port_cnt, port_index;

	if (port_msg->head_pattern != PORT_ENUM_HEAD_PATTERN ||
	    port_msg->tail_pattern != PORT_ENUM_TAIL_PATTERN) {
		pr_err("port enumeration info pattern is error(0x%x, 0x%x)\n",
		       port_msg->head_pattern, port_msg->tail_pattern);
		return -EFAULT;
	}
	if (port_msg->version != PORT_ENUM_VER) {
		pr_err("port enumeration info version doesn't match(0x%x)\n",
		       port_msg->version);
		return -EINVAL;
	}
	port_cnt = port_msg->port_count;
	for (port_index = 0; port_index < port_cnt; port_index++) {
		port_info = (struct port_info *)(port_msg->data +
						 (sizeof(struct port_info) * port_index));
		if (!port_info) {
			pr_err("the port info is NULL, the index %d\n", port_index);
			return -EFAULT;
		}
		port = proxy_get_port(-1, port_info->ch_id);
		if (!port) {
			pr_err("the port 0x%X isn't found\n", port_info->ch_id);
			continue;
		}
		if (ccci_fsm_get_md_state() == READY) {
			if (port_info->en_flag == CCCI_CHAN_ENABLE) {
				if (port->ops->enable_chl)
					port->ops->enable_chl(port);
			} else {
				if (port->ops->disable_chl)
					port->ops->disable_chl(port);
			}
		} else {
			port->chan_enable = port_info->en_flag;
		}
	}
	return 0;
}
