/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PORT_PROXY_H__
#define __T7XX_PORT_PROXY_H__

#include <linux/bits.h>
#include <linux/device.h>
#include <linux/types.h>
#include <net/sock.h>

#include "t7xx_common.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_modem_ops.h"
#include "t7xx_port.h"

/* CCCI logic channel enable & disable flag */
#define CCCI_CHAN_ENABLE	1
#define CCCI_CHAN_DISABLE	0

#define MTK_MAX_QUEUE_NUM	16
#define MTK_PORT_STATE_ENABLE	0
#define MTK_PORT_STATE_DISABLE	1
#define MTK_PORT_STATE_INVALID	2

#define MAX_RX_QUEUE_LENGTH	32
#define MAX_CTRL_QUEUE_LENGTH	16

#define CCCI_MTU		3568 /* 3.5kB -16 */
#define CLDMA_TXQ_MTU		MTK_SKB_4K

struct port_proxy {
	int			port_number;
	unsigned int		major;
	unsigned int		minor_base;
	struct t7xx_port	*ports;
	struct t7xx_port	*ctl_port;
	struct t7xx_port	*dedicated_ports[CLDMA_NUM][MTK_MAX_QUEUE_NUM];
	/* port list of each RX channel, for RX dispatching */
	struct list_head	rx_ch_ports[CCCI_MAX_CH_ID];
	/* port list of each queue for receiving queue status dispatching */
	struct list_head	queue_ports[CLDMA_NUM][MTK_MAX_QUEUE_NUM];
	struct sock		*netlink_sock;
	struct device		*dev;
};

struct ctrl_msg_header {
	u32			ctrl_msg_id;
	u32			reserved;
	u32			data_length;
	u8			data[0];
};

struct port_msg {
	u32			head_pattern;
	u32			info;
	u32			tail_pattern;
	u8			data[0]; /* port set info */
};

#define PORT_INFO_RSRVD		GENMASK(31, 16)
#define PORT_INFO_ENFLG		GENMASK(15, 15)
#define PORT_INFO_CH_ID		GENMASK(14, 0)

#define PORT_MSG_VERSION	GENMASK(31, 16)
#define PORT_MSG_PRT_CNT	GENMASK(15, 0)

#define PORT_ENUM_VER		0
#define PORT_ENUM_HEAD_PATTERN	0x5a5a5a5a
#define PORT_ENUM_TAIL_PATTERN	0xa5a5a5a5
#define PORT_ENUM_VER_MISMATCH	0x00657272

/* port operations mapping */
extern struct port_ops char_port_ops;
extern struct port_ops wwan_sub_port_ops;
extern struct port_ops ctl_port_ops;
extern struct port_ops tty_port_ops;
extern struct tty_dev_ops tty_ops;

int port_proxy_send_skb(struct t7xx_port *port, struct sk_buff *skb, bool from_pool);
void port_proxy_set_seq_num(struct t7xx_port *port, struct ccci_header *ccci_h);
int port_proxy_node_control(struct device *dev, struct port_msg *port_msg);
void port_proxy_reset(struct device *dev);
int port_proxy_broadcast_state(struct t7xx_port *port, int state);
void port_proxy_send_msg_to_md(int ch, unsigned int msg, unsigned int resv);
void port_proxy_uninit(void);
int port_proxy_init(struct mtk_modem *md);
void port_proxy_md_status_notify(unsigned int state);
struct t7xx_port *port_proxy_get_port(int major, int minor);

#endif /* __T7XX_PORT_PROXY_H__ */
