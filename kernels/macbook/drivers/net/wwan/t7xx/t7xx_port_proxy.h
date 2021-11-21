/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PORT_PROXY_H__
#define __T7XX_PORT_PROXY_H__

#include <net/sock.h>

#include "t7xx_common.h"
#include "t7xx_port.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_modem_ops.h"

/* ccci logic channel enable & disable flag */
#define CCCI_CHAN_ENABLE	1
#define CCCI_CHAN_DISABLE	0

#define MAX_QUEUE_NUM		16
#define PORT_STATE_ENABLE	0
#define PORT_STATE_DISABLE	1
#define PORT_STATE_INVALID	2

#define CCCI_MTU            3568 /* 3.5kB -16 */

/* error number definition */
#define EDROPPACKET	216
#define ERXFULL		217

enum {
	CRIT_USR_MUXD = 1,
	CRIT_USR_MDLOG,
	CRIT_USR_META,
};

struct port_proxy {
	int			port_number;
	unsigned int		major;
	unsigned int		minor_base;
	unsigned int		critical_user_active;
	/* do NOT use this manner, otherwise spinlock inside private_data
	 * will trigger alignment exception
	 */
	struct t7xx_port		*ports;
	struct t7xx_port		*ctl_port;
	struct t7xx_port		*dedicated_ports[CLDMA_NUM][MAX_QUEUE_NUM];
	/* port list of each Rx channel, for Rx dispatching */
	struct list_head	rx_ch_ports[CCCI_MAX_CH_ID];
	/* port list of each queue for receiving queue status dispatching */
	struct list_head	queue_ports[CLDMA_NUM][MAX_QUEUE_NUM];
	struct sock		*netlink_sock;
};

struct ctrl_msg_header {
	u32 ctrl_msg_id;
	u32 reserved;
	u32 data_length;
	u8 data[0];
} __packed;

struct port_info {
	u32 ch_id:15;
	u32 en_flag:1; /* 1:enable, 0:disable */
	u32 reserved:16; /* no use now */
} __packed;

struct port_msg {
	u32 head_pattern;
	u32 port_count:16;
	u32 version:16;
	u32 tail_pattern;
	u8 data[0]; /* port set info */
} __packed;

#define PORT_ENUM_VER 0
#define PORT_ENUM_HEAD_PATTERN 0x5a5a5a5a
#define PORT_ENUM_TAIL_PATTERN 0xa5a5a5a5
#define PORT_ENUM_VER_DISMATCH 0x00657272

int port_recv_skb_from_dedicatedQ(enum cldma_id hif_id, unsigned char qno,
				  struct sk_buff *skb);
int ccci_port_send_hif(struct t7xx_port *port, struct sk_buff *skb,
		       unsigned char from_pool, unsigned char blocking);
void port_inc_tx_seq_num(struct t7xx_port *port, struct ccci_header *ccci_h);
int ccci_port_node_control(void *data);
void port_reset(void);
int port_broadcast_state(struct t7xx_port *port, int state);
void proxy_send_msg_to_md(int ch, unsigned int msg, unsigned int resv, int blocking);
int proxy_dispatch_recv_skb(enum cldma_id hif_id, struct sk_buff *skb);

/* This API is called by ccci_modem,
 * and used to create all ccci port instance
 */
int ccci_port_init(struct ccci_modem *md);

int ccci_port_uninit(void);

/* This API is called by ccci_fsm,
 * and used to dispatch modem status for related port
 */
void ccci_port_md_status_notify(unsigned int state);

/* This API is called by ccci fsm,
 * and used to get critical user status.
 */
int ccci_port_get_critical_user(unsigned int user_id);

/* This API is called by ccci fsm,
 * and used to send a ccci msg for modem.
 */
int ccci_port_send_msg_to_md(int ch, unsigned int msg,
			     unsigned int resv, int blocking);

#endif /* __T7XX_PORT_PROXY_H__ */
