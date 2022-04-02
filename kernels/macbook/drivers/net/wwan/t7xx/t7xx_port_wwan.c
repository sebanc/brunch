// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021-2022, Intel Corporation.
 *
 * Authors:
 *  Amir Hanania <amir.hanania@intel.com>
 *  Chandrashekar Devegowda <chandrashekar.devegowda@intel.com>
 *  Haijun Liu <haijun.liu@mediatek.com>
 *  Moises Veleta <moises.veleta@intel.com>
 *  Ricardo Martinez<ricardo.martinez@linux.intel.com>
 *
 * Contributors:
 *  Andy Shevchenko <andriy.shevchenko@linux.intel.com>
 *  Chiranjeevi Rapolu <chiranjeevi.rapolu@intel.com>
 *  Eliot Lee <eliot.lee@intel.com>
 *  Sreehari Kancharla <sreehari.kancharla@intel.com>
 */

#include <linux/atomic.h>
#include <linux/bitfield.h>
#include <linux/dev_printk.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/minmax.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/wwan.h>

#include "t7xx_common.h"
#include "t7xx_port.h"
#include "t7xx_port_proxy.h"
#include "t7xx_state_monitor.h"

#define CCCI_HEADROOM		128

static int t7xx_port_ctrl_start(struct wwan_port *port)
{
	struct t7xx_port *port_mtk = wwan_port_get_drvdata(port);

	if (atomic_read(&port_mtk->usage_cnt))
		return -EBUSY;

	atomic_inc(&port_mtk->usage_cnt);
	return 0;
}

static void t7xx_port_ctrl_stop(struct wwan_port *port)
{
	struct t7xx_port *port_mtk = wwan_port_get_drvdata(port);

	atomic_dec(&port_mtk->usage_cnt);
}

static int t7xx_port_ctrl_tx(struct wwan_port *port, struct sk_buff *skb)
{
	struct t7xx_port *port_private = wwan_port_get_drvdata(port);
	size_t actual_len, alloc_size, txq_mtu = CLDMA_MTU;
	struct t7xx_port_static *port_static;
	unsigned int len, i, packets;
	struct t7xx_fsm_ctl *ctl;
	enum md_state md_state;

	len = skb->len;
	if (!len || !port_private->rx_length_th || port_private->rx_length_th > RX_QUEUE_MAXLEN ||
	    !port_private->chan_enable || port_private->flags > PORT_F_MAX)
		return -EINVAL;

	port_static = port_private->port_static;
	ctl = port_private->t7xx_dev->md->fsm_ctl;
	md_state = t7xx_fsm_get_md_state(ctl);
	if (md_state == MD_STATE_WAITING_FOR_HS1 || md_state == MD_STATE_WAITING_FOR_HS2) {
		dev_warn(port_private->dev, "Cannot write to %s port when md_state=%d\n",
			 port_static->name, md_state);
		return -ENODEV;
	}

	alloc_size = min_t(size_t, txq_mtu, len + CCCI_HEADROOM);
	actual_len = alloc_size - CCCI_HEADROOM;
	packets = DIV_ROUND_UP(len, txq_mtu - CCCI_HEADROOM);

	for (i = 0; i < packets; i++) {
		struct ccci_header *ccci_h;
		struct sk_buff *skb_ccci;
		int ret;

		if (packets > 1 && packets == i + 1) {
			actual_len = len % (txq_mtu - CCCI_HEADROOM);
			alloc_size = actual_len + CCCI_HEADROOM;
		}

		skb_ccci = __dev_alloc_skb(alloc_size, GFP_KERNEL);
		if (!skb_ccci)
			return -ENOMEM;

		ccci_h = skb_put(skb_ccci, sizeof(*ccci_h));
		t7xx_ccci_header_init(ccci_h, 0, actual_len + sizeof(*ccci_h),
				      port_static->tx_ch, 0);
		skb_put_data(skb_ccci, skb->data + i * (txq_mtu - CCCI_HEADROOM), actual_len);
		t7xx_port_proxy_set_tx_seq_num(port_private, ccci_h);

		ret = t7xx_port_send_skb_to_md(port_private, skb_ccci);
		if (ret) {
			dev_kfree_skb_any(skb_ccci);
			dev_err(port_private->dev, "Write error on %s port, %d\n",
				port_static->name, ret);
			return ret;
		}

		port_private->seq_nums[MTK_TX]++;
	}

	dev_kfree_skb(skb);
	return 0;
}

static const struct wwan_port_ops wwan_ops = {
	.start = t7xx_port_ctrl_start,
	.stop = t7xx_port_ctrl_stop,
	.tx = t7xx_port_ctrl_tx,
};

static int t7xx_port_wwan_init(struct t7xx_port *port)
{
	struct t7xx_port_static *port_static = port->port_static;

	port->rx_length_th = RX_QUEUE_MAXLEN;
	port->flags |= PORT_F_RX_ADJUST_HEADER;

	if (port_static->rx_ch == PORT_CH_UART2_RX)
		port->flags |= PORT_F_RX_CH_TRAFFIC;

	if (!port->chan_enable)
		port->flags |= PORT_F_RX_ALLOW_DROP;

	return 0;
}

static void t7xx_port_wwan_uninit(struct t7xx_port *port)
{
	unsigned long flags;

	if (!port->wwan_port)
		return;

	spin_lock_irqsave(&port->rx_wq.lock, flags);
	port->rx_length_th = 0;
	spin_unlock_irqrestore(&port->rx_wq.lock, flags);

	wwan_remove_port(port->wwan_port);
	port->wwan_port = NULL;
}

static int t7xx_port_wwan_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	struct t7xx_port_static *port_static = port->port_static;

	if (!atomic_read(&port->usage_cnt)) {
		dev_kfree_skb_any(skb);
		dev_err_ratelimited(port->dev, "Port %s is not opened, drop packets\n",
				    port_static->name);
		return 0;
	}

	return t7xx_port_recv_skb(port, skb);
}

static int t7xx_port_wwan_enable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = true;
	port->flags &= ~PORT_F_RX_ALLOW_DROP;
	spin_unlock(&port->port_update_lock);

	return 0;
}

static int t7xx_port_wwan_disable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = false;
	port->flags |= PORT_F_RX_ALLOW_DROP;
	spin_unlock(&port->port_update_lock);

	return 0;
}

static void t7xx_port_wwan_md_state_notify(struct t7xx_port *port, unsigned int state)
{
	struct t7xx_port_static *port_static = port->port_static;

	if (state != MD_STATE_READY)
		return;

	if (!port->wwan_port) {
		port->wwan_port = wwan_create_port(port->dev, port_static->port_type,
						   &wwan_ops, port);
		if (IS_ERR(port->wwan_port))
			dev_err(port->dev, "Unable to create WWWAN port %s", port_static->name);
	}
}

struct port_ops wwan_sub_port_ops = {
	.init = t7xx_port_wwan_init,
	.recv_skb = t7xx_port_wwan_recv_skb,
	.uninit = t7xx_port_wwan_uninit,
	.enable_chl = t7xx_port_wwan_enable_chl,
	.disable_chl = t7xx_port_wwan_disable_chl,
	.md_state_notify = t7xx_port_wwan_md_state_notify,
};
