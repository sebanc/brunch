// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/kthread.h>

#include "t7xx_common.h"
#include "t7xx_monitor.h"
#include "t7xx_port_config.h"
#include "t7xx_port_proxy.h"

#define MAX_QUEUE_LENGTH 16

static void control_msg_handler(struct t7xx_port *port, struct sk_buff *skb)
{
	int ret = 0;
	struct ctrl_msg_header *ctrl_msg_h;
	struct ccci_fsm_ctl *ctl = fsm_get_entry();

	skb_pull(skb, sizeof(struct ccci_header));
	ctrl_msg_h = (struct ctrl_msg_header *)skb->data;

	switch (ctrl_msg_h->ctrl_msg_id) {
	case CTL_ID_HS2_MSG:
		skb_pull(skb, sizeof(struct ctrl_msg_header));
		if (port->rx_ch == CCCI_CONTROL_RX)
			fsm_append_event(ctl, CCCI_EVENT_MD_HS2,
					 skb->data, ctrl_msg_h->data_length);
		ccci_free_skb(skb);
		break;
	case CTL_ID_MD_EX:
	case CTL_ID_MD_EX_REC_OK:
	case CTL_ID_MD_EX_PASS:
	case CCCI_DRV_VER_ERROR:
		fsm_ee_message_handler(&ctl->ee_ctl, skb);
		ccci_free_skb(skb);
		break;
	case CTL_ID_PORT_ENUM:
		skb_pull(skb, sizeof(struct ctrl_msg_header));
		ret = ccci_port_node_control((void *)skb->data);
		if (ret == 0)
			proxy_send_msg_to_md(CCCI_CONTROL_TX, CTL_ID_PORT_ENUM, 0, 1);
		else
			proxy_send_msg_to_md(CCCI_CONTROL_TX,
					     CTL_ID_PORT_ENUM, PORT_ENUM_VER_DISMATCH, 1);
		break;
	default:
		pr_err("unknown control message to fsm %x\n", ctrl_msg_h->ctrl_msg_id);
		break;
	}
	if (ret)
		pr_err("%s control msg handle error: %d\n", port->name, ret);
}

static int port_ctl_init(struct t7xx_port *port)
{
	port->skb_handler = &control_msg_handler;
	port->thread =
		kthread_run(port_kthread_handler, port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	return 0;
}

static void port_ctl_uninit(struct t7xx_port *port)
{
	unsigned long flags;
	struct sk_buff *skb;

	if (port->thread)
		kthread_stop(port->thread);
	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
		ccci_free_skb(skb);

	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
}

struct port_ops ctl_port_ops = {
	.init = &port_ctl_init,
	.recv_skb = &port_recv_skb,
	.uninit = &port_ctl_uninit,
};
