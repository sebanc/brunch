/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_IP_LINK_H__
#define __MAUSB_IP_LINK_H__

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/inet.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>

#define MAUSB_LINK_BUFF_SIZE	16777216
#define MAUSB_LINK_TOS_LEVEL_EF 0xB8

extern struct miscdevice mausb_host_dev;

enum mausb_link_action {
	MAUSB_LINK_CONNECT	= 0,
	MAUSB_LINK_DISCONNECT	= 1,
	MAUSB_LINK_RECV		= 2,
	MAUSB_LINK_SEND		= 3,
};

enum mausb_channel {
	MAUSB_CTRL_CHANNEL  = 0,
	MAUSB_ISOCH_CHANNEL = 1,
	MAUSB_BULK_CHANNEL  = 2,
	MAUSB_INTR_CHANNEL  = 3,
	MAUSB_MGMT_CHANNEL  = 4,
};

struct mausb_kvec_data_wrapper {
	struct kvec *kvec;
	u32    kvec_num;
	u32    length;
};

struct mausb_ip_recv_ctx {
	u16  left;
	u16  received;
	char *buffer;
	char common_hdr[12] __aligned(4);
};

struct mausb_ip_ctx {
	struct socket		*client_socket;
	union {
		struct sockaddr_in sa_in;
#if IS_ENABLED(CONFIG_IPV6)
		struct sockaddr_in6 sa_in6;
#endif
	} dev_addr_in;
	struct net		*net_ns;
	bool			udp;

	/* Queues to schedule rx work */
	struct workqueue_struct	*recv_workq;
	struct workqueue_struct	*connect_workq;
	struct work_struct	recv_work;
	struct work_struct	connect_work;

	struct mausb_ip_recv_ctx recv_ctx; /* recv buffer */

	enum mausb_channel channel;
	void *ctx;
	/* callback should store task into hpal queue */
	void (*fn_callback)(void *ctx, enum mausb_channel channel,
			    enum mausb_link_action act, int status, void *data);
};

int mausb_init_ip_ctx(struct mausb_ip_ctx **ip_ctx,
		      struct net *net_ns,
		      char ip_addr[INET6_ADDRSTRLEN],
		      u16 port,
		      void *ctx,
		      void (*ctx_callback)(void *ctx,
					   enum mausb_channel channel,
					   enum mausb_link_action act,
					   int status, void *data),
		      enum mausb_channel channel);
int mausb_ip_disconnect(struct mausb_ip_ctx *ip_ctx);
int mausb_ip_send(struct mausb_ip_ctx *ip_ctx,
		  struct mausb_kvec_data_wrapper *wrapper);

void mausb_destroy_ip_ctx(struct mausb_ip_ctx *ip_ctx);
void mausb_ip_connect_async(struct mausb_ip_ctx *ip_ctx);

#endif /* __MAUSB_IP_LINK_H__ */
