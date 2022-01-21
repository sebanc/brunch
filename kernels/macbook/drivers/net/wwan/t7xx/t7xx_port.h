/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PORT_H__
#define __T7XX_PORT_H__

#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/wwan.h>
#include <linux/types.h>

#include "t7xx_hif_cldma.h"

#define PORT_F_RX_ALLOW_DROP	BIT(0)	/* packet will be dropped if port's RX buffer full */
#define PORT_F_RX_FULLED	BIT(1)	/* RX buffer has been full once */
#define PORT_F_USER_HEADER	BIT(2)	/* CCCI header will be provided by user, but not by CCCI */
#define PORT_F_RX_EXCLUSIVE	BIT(3)	/* RX queue only has this one port */
#define PORT_F_RX_ADJUST_HEADER	BIT(4)	/* check whether need remove CCCI header while recv skb */
#define PORT_F_RX_CH_TRAFFIC	BIT(5)	/* enable port channel traffic */
#define PORT_F_RX_CHAR_NODE	BIT(7)	/* need export char dev node for userspace */
#define PORT_F_CHAR_NODE_SHOW	BIT(10)	/* char dev node is shown to userspace at default */

/* reused for net TX, Data queue, same bit as RX_FULLED */
#define PORT_F_TX_DATA_FULLED	BIT(1)
#define PORT_F_TX_ACK_FULLED	BIT(8)
#define PORT_F_RAW_DATA		BIT(9)

#define CCCI_MAX_CH_ID		0xff /* RX channel ID should NOT be >= this!! */
#define CCCI_CH_ID_MASK		0xff

/* Channel ID and Message ID definitions.
 * The channel number consists of peer_id(15:12) , channel_id(11:0)
 * peer_id:
 * 0:reserved, 1: to sAP, 2: to MD
 */
enum ccci_ch {

	/* to sAP */
        CCCI_SAP_CONTROL_RX = 0X1000,
        CCCI_SAP_CONTROL_TX = 0X1001,
        CCCI_SAP_GNSS_RX = 0x1004,
        CCCI_SAP_GNSS_TX = 0x1005,
        CCCI_SAP_META_RX = 0x1006,
        CCCI_SAP_META_TX = 0x1007,
        CCCI_SAP_LOG_RX = 0x1008,
        CCCI_SAP_LOG_TX = 0x1009,
        CCCI_SAP_ADB_RX = 0x100a,
        CCCI_SAP_ADB_TX = 0x100b,

	/* to MD */
	CCCI_CONTROL_RX = 0x2000,
	CCCI_CONTROL_TX = 0x2001,
	CCCI_SYSTEM_RX = 0x2002,
	CCCI_SYSTEM_TX = 0x2003,
	CCCI_UART1_RX = 0x2006,			/* META */
	CCCI_UART1_RX_ACK = 0x2007,
	CCCI_UART1_TX = 0x2008,
	CCCI_UART1_TX_ACK = 0x2009,
	CCCI_UART2_RX = 0x200a,			/* AT */
	CCCI_UART2_RX_ACK = 0x200b,
	CCCI_UART2_TX = 0x200c,
	CCCI_UART2_TX_ACK = 0x200d,
	CCCI_MD_LOG_RX = 0x202a,		/* MD logging */
	CCCI_MD_LOG_TX = 0x202b,
	CCCI_LB_IT_RX = 0x203e,			/* loop back test */
	CCCI_LB_IT_TX = 0x203f,
	CCCI_STATUS_RX = 0x2043,		/* status polling */
	CCCI_STATUS_TX = 0x2044,
	CCCI_MIPC_RX = 0x20ce,			/* MIPC */
	CCCI_MIPC_TX = 0x20cf,
	CCCI_MBIM_RX = 0x20d0,
	CCCI_MBIM_TX = 0x20d1,
	CCCI_DSS0_RX = 0x20d2,
	CCCI_DSS0_TX = 0x20d3,
	CCCI_DSS1_RX = 0x20d4,
	CCCI_DSS1_TX = 0x20d5,
	CCCI_DSS2_RX = 0x20d6,
	CCCI_DSS2_TX = 0x20d7,
	CCCI_DSS3_RX = 0x20d8,
	CCCI_DSS3_TX = 0x20d9,
	CCCI_DSS4_RX = 0x20da,
	CCCI_DSS4_TX = 0x20db,
	CCCI_DSS5_RX = 0x20dc,
	CCCI_DSS5_TX = 0x20dd,
	CCCI_DSS6_RX = 0x20de,
	CCCI_DSS6_TX = 0x20df,
	CCCI_DSS7_RX = 0x20e0,
	CCCI_DSS7_TX = 0x20e1,
	CCCI_MAX_CH_NUM,
	CCCI_MONITOR_CH_ID = GENMASK(31, 28), /* for MD init */
	CCCI_INVALID_CH_ID = GENMASK(15, 0),
};

struct t7xx_port;
struct port_ops {
	int (*init)(struct t7xx_port *port);
	int (*recv_skb)(struct t7xx_port *port, struct sk_buff *skb);
	void (*md_state_notify)(struct t7xx_port *port, unsigned int md_state);
	void (*uninit)(struct t7xx_port *port);
	int (*enable_chl)(struct t7xx_port *port);
	int (*disable_chl)(struct t7xx_port *port);
};

typedef void (*port_skb_handler)(struct t7xx_port *port, struct sk_buff *skb);

struct t7xx_port {
	/* members used for initialization, do not change the order */
	enum ccci_ch		tx_ch;
	enum ccci_ch		rx_ch;
	unsigned char		txq_index;
	unsigned char		rxq_index;
	unsigned char		txq_exp_index;
	unsigned char		rxq_exp_index;
	enum cldma_id		path_id;
	unsigned int		flags;
	struct port_ops		*ops;
	unsigned int		minor;
	char			*name;
	enum wwan_port_type	mtk_port_type;
	struct wwan_port	*mtk_wwan_port;
	struct mtk_pci_dev	*mtk_dev;
	struct device		*dev;

	/* un-initialized in definition, always put them at the end */
	short			seq_nums[2];
	struct port_proxy	*port_proxy;
	atomic_t		usage_cnt;
	struct			list_head entry;
	struct			list_head queue_entry;
	unsigned int		major;
	unsigned int		minor_base;
	/* TX and RX flows are asymmetric since ports are multiplexed on
	 * queues.
	 *
	 * TX: data blocks are sent directly to a queue. Each port
	 * does not maintain a TX list; instead, they only provide
	 * a wait_queue_head for blocking writes.
	 *
	 * RX: Each port uses a RX list to hold packets,
	 * allowing the modem to dispatch RX packet as quickly as possible.
	 */
	struct sk_buff_head	rx_skb_list;
	bool			skb_from_pool;
	spinlock_t		port_update_lock; /* protects port configuration */
	wait_queue_head_t	rx_wq;
	int			rx_length_th;
	port_skb_handler	skb_handler;
	unsigned char		chan_enable;
	unsigned char		chn_crt_stat;
	struct cdev		*cdev;
	struct task_struct	*thread;
	struct mutex		tx_mutex_lock; /* protects the seq number operation */
};

int port_kthread_handler(void *arg);
int port_recv_skb(struct t7xx_port *port, struct sk_buff *skb);
int port_write_room_to_md(struct t7xx_port *port);
struct t7xx_port *port_get_by_minor(int minor);
struct t7xx_port *port_get_by_name(char *port_name);
int port_send_skb_to_md(struct t7xx_port *port, struct sk_buff *skb, bool blocking);
int port_register_device(const char *name, int major, int minor);
void port_unregister_device(int major, int minor);

#endif /* __T7XX_PORT_H__ */
