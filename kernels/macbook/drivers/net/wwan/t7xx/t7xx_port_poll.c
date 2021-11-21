// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include "t7xx_monitor.h"
#include "t7xx_port.h"
#include "t7xx_port_config.h"

#define MAX_QUEUE_LENGTH 16

static void status_msg_handler(struct t7xx_port *port, struct sk_buff *skb)
{
	int ret;

	ret = ccci_fsm_recv_status_packet(skb);
	if (ret)
		pr_err("%s status poller gotten error: %d\n", port->name, ret);
}

static int port_poller_init(struct t7xx_port *port)
{
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	port->skb_handler = &status_msg_handler;
	return 0;
}

struct port_ops poller_port_ops = {
	.init = &port_poller_init,
	.recv_skb = &port_recv_skb,
};
