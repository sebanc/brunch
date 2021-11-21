/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PORT_H__
#define __T7XX_PORT_H__

#include <linux/skbuff.h>
#include <linux/wwan.h>

#include "t7xx_hif_cldma.h"

#define PORT_F_ALLOW_DROP	BIT(0)	/* packet will be dropped if port's Rx buffer full */
#define PORT_F_RX_FULLED	BIT(1)	/* rx buffer has been full once */
#define PORT_F_USER_HEADER	BIT(2)	/* CCCI header will be provided by user, but not by CCCI */
#define PORT_F_RX_EXCLUSIVE	BIT(3)	/* Rx queue only has this one port */
#define PORT_F_ADJUST_HEADER	BIT(4)	/* Check whether need remove ccci header while recv skb */
#define PORT_F_CH_TRAFFIC	BIT(5)	/* Enable port channel traffic */
#define PORT_F_WITH_CHAR_NODE	BIT(7)	/* Need export char dev node for userspace */
#define PORT_F_CHAR_NODE_SHOW	BIT(10)	/* char dev node is shown to userspace at default */

/* reused for net tx, Data queue, same bit as RX_FULLED */
#define PORT_F_TX_DATA_FULLED	BIT(1)
#define PORT_F_TX_ACK_FULLED	BIT(8)
#define PORT_F_RAW_DATA		BIT(9)

struct t7xx_port;

struct port_ops {
	/* must-have */
	int (*init)(struct t7xx_port *port);
	int (*recv_skb)(struct t7xx_port *port, struct sk_buff *skb);
	/* optional */
	int (*recv_match)(struct t7xx_port *port, struct sk_buff *skb);
	void (*md_state_notify)(struct t7xx_port *port, unsigned int md_state);
	void (*uninit)(struct t7xx_port *port);
	int (*enable_chl)(struct t7xx_port *port);
	int (*disable_chl)(struct t7xx_port *port);
};

typedef void (*port_skb_handler)(struct t7xx_port *port, struct sk_buff *skb);

struct t7xx_port {
	/* don't change the sequence unless you modified modem drivers as well */
	enum ccci_ch tx_ch;
	enum ccci_ch rx_ch;
	unsigned char txq_index;
	unsigned char rxq_index;
	unsigned char txq_exp_index;
	unsigned char rxq_exp_index;
	enum cldma_id path_id;
	unsigned int flags;
	struct port_ops *ops;
	/* device node related */
	unsigned int minor;
	char *name;

	enum wwan_port_type mtk_port_type;
	struct wwan_port *mtk_wwan_port;
	struct device *dev;

	/* un-initialized in definition, always put them at the end */
	short seq_nums[2];
	struct port_proxy *port_proxy;
	atomic_t usage_cnt;
	struct list_head entry;
	struct list_head queue_entry;
	unsigned int major; /* dynamic alloc */
	unsigned int minor_base;
	/* Tx and Rx flows are asymmetric since ports are multiplexed on
	 * queues. Tx: data blocks are sent directly to a queue. Each port
	 * does not maintain a Tx list; instead, they only provide a wait_queue_head
	 * for blocking writes. Rx: Each port uses a Rx list to hold packets,
	 * allowing the modem to dispatch Rx packet as quickly as possible.
	 */
	struct sk_buff_head rx_skb_list;
	unsigned char skb_from_pool;
	spinlock_t port_update_lock;	/* protects port configuration */
	wait_queue_head_t rx_wq;	/* for uplayer user */
	int rx_length_th;
	unsigned int tx_busy_count;
	unsigned int rx_busy_count;
	port_skb_handler skb_handler;
	unsigned char chan_enable;
	unsigned char chn_crt_stat;
	struct cdev *cdev;
	struct task_struct *thread;
};

/*****************************************************************************/
/* API Region called by ccci port object */
/*****************************************************************************/
/* This API used to some port create kthread handle,
 * which have same kthread handle flow.
 */
int port_kthread_handler(void *arg);
/* This API used to some port to receive native HIF RX data,
 * which have same RX receive flow.
 */
int port_recv_skb(struct t7xx_port *port, struct sk_buff *skb);

int port_write_room_to_md(struct t7xx_port *port);
struct t7xx_port *port_get_by_minor(int minor);
struct t7xx_port *port_get_by_name(char *port_name);
int port_send_skb_to_md(struct t7xx_port *port,
			struct sk_buff *skb, int blocking);
int port_dev_open(struct inode *inode, struct file *file);
int port_dev_close(struct inode *inode, struct file *file);
ssize_t port_dev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
ssize_t port_dev_write(struct file *file, const char __user *buf,
		       size_t count, loff_t *ppos);

#endif /* __T7XX_PORT_H__ */
