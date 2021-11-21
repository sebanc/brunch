// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/cdev.h>
#include <linux/poll.h>

#include "t7xx_buffer_manager.h"
#include "t7xx_monitor.h"
#include "t7xx_port_proxy.h"
#include "t7xx_port.h"
#include "t7xx_port_config.h"

#define MAX_QUEUE_LENGTH 32

static __poll_t port_char_dev_poll(struct file *fp, struct poll_table_struct *poll)
{
	struct t7xx_port *port = fp->private_data;
	__poll_t mask = 0;
	int md_state = ccci_fsm_get_md_state();

	poll_wait(fp, &port->rx_wq, poll);

	if (!skb_queue_empty(&port->rx_skb_list))
		mask |= EPOLLIN | EPOLLRDNORM;
	if (port_write_room_to_md(port) > 0)
		mask |= EPOLLOUT | EPOLLWRNORM;
	if (port->rx_ch == CCCI_UART1_RX &&
	    md_state != READY &&
	    md_state != EXCEPTION) {
		/* notify MD logger to save its log before md_init kills it */
		mask |= EPOLLERR;
		pr_err("poll error for MD logger at state %d, mask=%u\n", md_state, mask);
	}

	return mask;
}

static const struct file_operations char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &port_dev_open, /* use default API */
	.read = &port_dev_read, /* use default API */
	.write = &port_dev_write, /* use default API */
	.release = &port_dev_close,/* use default API */
	.poll = &port_char_dev_poll,/* use port char self API */
};

static int port_char_init(struct t7xx_port *port)
{
	struct cdev *dev;
	int ret = 0;

	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		dev = cdev_alloc();
		if (!dev)
			return -ENOMEM;
		dev->ops = &char_dev_fops;
		dev->owner = THIS_MODULE;
		ret = cdev_add(dev, MKDEV(port->major, port->minor_base + port->minor), 1);
		if (ret)
			kobject_put(&dev->kobj);
		if (!(port->flags & PORT_F_RAW_DATA))
			port->flags |= PORT_F_ADJUST_HEADER;
		port->cdev = dev;
	}

	if (port->rx_ch == CCCI_UART2_RX)
		port->flags |= PORT_F_CH_TRAFFIC;

	return ret;
}

static void port_char_uninit(struct t7xx_port *port)
{
	unsigned long flags;
	struct sk_buff *skb;

	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		if (port->cdev) {
			if (port->chn_crt_stat == CCCI_CHAN_ENABLE) {
				ccci_unregister_dev_node(port->major,
							 port->minor_base + port->minor);
				spin_lock(&port->port_update_lock);
				port->chn_crt_stat = CCCI_CHAN_DISABLE;
				spin_unlock(&port->port_update_lock);
			}
			cdev_del(port->cdev);
			port->cdev = NULL;
		}
	}

	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
		ccci_free_skb(skb);

	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
}

static int port_char_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		if (!atomic_read(&port->usage_cnt)) {
			pr_err("port %s is not opened, drop packets\n", port->name);
			return -EDROPPACKET;
		}
	}

	return port_recv_skb(port, skb);
}

static int port_status_update(struct t7xx_port *port)
{
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		if (port->chan_enable == CCCI_CHAN_ENABLE) {
			int ret;

			port->flags &= ~PORT_F_ALLOW_DROP;
			ret = ccci_register_dev_node(port->name, port->major,
						     port->minor_base + port->minor);
			if (!ret) {
				port_broadcast_state(port, PORT_STATE_ENABLE);
				spin_lock(&port->port_update_lock);
				port->chn_crt_stat = CCCI_CHAN_ENABLE;
				spin_unlock(&port->port_update_lock);
			}
			return ret;
		}
		port->flags |= PORT_F_ALLOW_DROP;
		ccci_unregister_dev_node(port->major,
					 port->minor_base + port->minor);
		spin_lock(&port->port_update_lock);
		port->chn_crt_stat = CCCI_CHAN_DISABLE;
		spin_unlock(&port->port_update_lock);
		port_broadcast_state(port, PORT_STATE_DISABLE);
	}

	return 0;
}

static int port_char_enable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_ENABLE;
	spin_unlock(&port->port_update_lock);
	if (port->chn_crt_stat != port->chan_enable)
		return port_status_update(port);

	return 0;
}

static int port_char_disable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);
	if (port->chn_crt_stat != port->chan_enable)
		port_status_update(port);

	return 0;
}

static void port_char_md_state_notify(struct t7xx_port *port, unsigned int state)
{
	if (state == READY)
		port_status_update(port);
}

struct port_ops char_port_ops = {
	.init = &port_char_init,
	.recv_skb = &port_char_recv_skb,
	.uninit = &port_char_uninit,
	.enable_chl = &port_char_enable_chl,
	.disable_chl = &port_char_disable_chl,
	.md_state_notify = &port_char_md_state_notify,
};
