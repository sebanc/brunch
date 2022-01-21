// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/kthread.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#include "t7xx_common.h"
#include "t7xx_monitor.h"
#include "t7xx_port_proxy.h"

static void fsm_ee_message_handler(struct sk_buff *skb)
{
	struct ctrl_msg_header *ctrl_msg_h;
	struct ccci_fsm_ctl *ctl;
	enum md_state md_state;
	struct device *dev;

	ctrl_msg_h = (struct ctrl_msg_header *)skb->data;
	md_state = ccci_fsm_get_md_state();
	ctl = fsm_get_entry();
	dev = &ctl->md->mtk_dev->pdev->dev;
	if (md_state != MD_STATE_EXCEPTION) {
		dev_err(dev, "receive invalid MD_EX %x when MD state is %d\n",
			ctrl_msg_h->reserved, md_state);
		return;
	}

	switch (ctrl_msg_h->ctrl_msg_id) {
	case CTL_ID_MD_EX:
		if (ctrl_msg_h->reserved != MD_EX_CHK_ID) {
			dev_err(dev, "receive invalid MD_EX %x\n", ctrl_msg_h->reserved);
		} else {
			port_proxy_send_msg_to_md(CCCI_CONTROL_TX, CTL_ID_MD_EX, MD_EX_CHK_ID);
			fsm_append_event(ctl, CCCI_EVENT_MD_EX, NULL, 0);
		}

		break;

	case CTL_ID_MD_EX_ACK:
		if (ctrl_msg_h->reserved != MD_EX_CHK_ACK_ID)
			dev_err(dev, "receive invalid MD_EX_ACK %x\n", ctrl_msg_h->reserved);
		else
			fsm_append_event(ctl, CCCI_EVENT_MD_EX_REC_OK, NULL, 0);

		break;

	case CTL_ID_MD_EX_PASS:
		fsm_append_event(ctl, CCCI_EVENT_MD_EX_PASS, NULL, 0);
		break;

	case CTL_ID_DRV_VER_ERROR:
		dev_err(dev, "AP/MD driver version mismatch\n");
	}
}

static void control_msg_handler(struct t7xx_port *port, struct sk_buff *skb)
{
	struct ctrl_msg_header *ctrl_msg_h;
	struct ccci_fsm_ctl *ctl;
	int ret = 0;

	skb_pull(skb, sizeof(struct ccci_header));
	ctrl_msg_h = (struct ctrl_msg_header *)skb->data;
	ctl = fsm_get_entry();

	switch (ctrl_msg_h->ctrl_msg_id) {
	case CTL_ID_HS2_MSG:
		skb_pull(skb, sizeof(struct ctrl_msg_header));
		if (port->rx_ch == CCCI_CONTROL_RX)
			fsm_append_event(ctl, CCCI_EVENT_MD_HS2,
					 skb->data, ctrl_msg_h->data_length);
		if (port->rx_ch == CCCI_SAP_CONTROL_RX)
                        fsm_append_event(ctl, CCCI_EVENT_SAP_HS2, \
                                        skb->data, ctrl_msg_h->data_length);

		ccci_free_skb(&port->mtk_dev->pools, skb);
		break;

	case CTL_ID_MD_EX:
	case CTL_ID_MD_EX_ACK:
	case CTL_ID_MD_EX_PASS:
	case CTL_ID_DRV_VER_ERROR:
		fsm_ee_message_handler(skb);
		ccci_free_skb(&port->mtk_dev->pools, skb);
		break;

	case CTL_ID_PORT_ENUM:
		skb_pull(skb, sizeof(struct ctrl_msg_header));
		ret = port_proxy_node_control(port->dev, (struct port_msg *)skb->data);
		if (!ret)
			port_proxy_send_msg_to_md(CCCI_CONTROL_TX, CTL_ID_PORT_ENUM, 0);
		else
			port_proxy_send_msg_to_md(CCCI_CONTROL_TX,
						  CTL_ID_PORT_ENUM, PORT_ENUM_VER_MISMATCH);

		break;

	default:
		dev_err(port->dev, "unknown control message ID to FSM %x\n",
			ctrl_msg_h->ctrl_msg_id);
		break;
	}

	if (ret)
		dev_err(port->dev, "%s control message handle error: %d\n", port->name, ret);
}

static int port_ctl_init(struct t7xx_port *port)
{
	port->skb_handler = &control_msg_handler;
	port->thread = kthread_run(port_kthread_handler, port, "%s", port->name);
	if (IS_ERR(port->thread)) {
		dev_err(port->dev, "failed to start port control thread\n");
		return PTR_ERR(port->thread);
	}

	port->rx_length_th = MAX_CTRL_QUEUE_LENGTH;
	port->skb_from_pool = true;
	return 0;
}

static void port_ctl_uninit(struct t7xx_port *port)
{
	unsigned long flags;
	struct sk_buff *skb;

	if (port->thread)
		kthread_stop(port->thread);

	spin_lock_irqsave(&port->rx_wq.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
		ccci_free_skb(&port->mtk_dev->pools, skb);

	spin_unlock_irqrestore(&port->rx_wq.lock, flags);
}

struct port_ops ctl_port_ops = {
	.init = &port_ctl_init,
	.recv_skb = &port_recv_skb,
	.uninit = &port_ctl_uninit,
};
