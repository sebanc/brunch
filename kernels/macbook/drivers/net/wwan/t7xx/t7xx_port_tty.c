// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/atomic.h>
#include <linux/bitfield.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "t7xx_common.h"
#include "t7xx_port_proxy.h"
#include "t7xx_skb_util.h"
#include "t7xx_tty_ops.h"

#define TTY_PORT_NAME_BASE		"ttyC"
#define TTY_PORT_NR			32

static int ccci_tty_send_pkt(int tty_port_idx, const void *data, int len)
{
	struct ccci_header *ccci_h = NULL;
	int actual_count, alloc_size;
	int ret, header_len = 0;
	struct t7xx_port *port;
	struct sk_buff *skb;

	port = port_get_by_minor(tty_port_idx + TTY_PORT_MINOR_BASE);
	if (!port)
		return -ENXIO;

	if (port->flags & PORT_F_RAW_DATA) {
		actual_count = len > CLDMA_TXQ_MTU ? CLDMA_TXQ_MTU : len;
		alloc_size = actual_count;
	} else {
		/* get skb info */
		header_len = sizeof(struct ccci_header);
		actual_count = len > CCCI_MTU ? CCCI_MTU : len;
		alloc_size = actual_count + header_len;
	}

	skb = ccci_alloc_skb_from_pool(&port->mtk_dev->pools, alloc_size, GFS_BLOCKING);
	if (skb) {
		if (!(port->flags & PORT_F_RAW_DATA)) {
			ccci_h = skb_put(skb, sizeof(struct ccci_header));
			ccci_h->data[0] = 0;
			ccci_h->data[1] = actual_count + header_len;
			ccci_h->status &= ~HDR_FLD_CHN;
			ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, port->tx_ch);
			ccci_h->reserved = 0;
		}
	} else {
		return -ENOMEM;
	}

	memcpy(skb_put(skb, actual_count), data, actual_count);

	/* send data */
	port_proxy_set_seq_num(port, ccci_h);
	ret = port_send_skb_to_md(port, skb, true);
	if (ret) {
		dev_err(port->dev, "failed to send skb to md, ret = %d\n", ret);
		return ret;
	}

	/* Record the port seq_num after the data is sent to HIF.
	 * Only bits 0-14 are used, thus negating overflow.
	 */
	port->seq_nums[MTK_OUT]++;

	return actual_count;
}

static struct tty_ccci_ops mtk_tty_ops = {
	.tty_num = TTY_PORT_NR,
	.name = TTY_PORT_NAME_BASE,
	.md_ability = 0,
	.send_pkt = ccci_tty_send_pkt,
};

static int port_tty_init(struct t7xx_port *port)
{
	/* mapping the minor number to tty dev idx */
	port->minor += TTY_PORT_MINOR_BASE;

	/* init the tty driver */
	if (!tty_ops.tty_driver_status) {
		tty_ops.tty_driver_status = true;
		atomic_set(&tty_ops.port_installed_num, 0);
		tty_ops.init(&mtk_tty_ops, port);
	}

	return 0;
}

static int port_tty_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	int actual_recv_len;

	/* get skb data */
	if (!(port->flags & PORT_F_RAW_DATA))
		skb_pull(skb, sizeof(struct ccci_header));

	/* send data to tty driver. */
	actual_recv_len = tty_ops.rx_callback(port, skb->data, skb->len);

	if (actual_recv_len != skb->len) {
		dev_err(port->dev, "ccci port[%s] recv skb fail\n", port->name);
		skb_push(skb, sizeof(struct ccci_header));
		return -ENOBUFS;
	}

	ccci_free_skb(&port->mtk_dev->pools, skb);
	return 0;
}

static void port_tty_md_state_notify(struct t7xx_port *port, unsigned int state)
{
	if (state != MD_STATE_READY || port->chan_enable != CCCI_CHAN_ENABLE)
		return;

	port->flags &= ~PORT_F_RX_ALLOW_DROP;
	/* create a tty port */
	tty_ops.tty_port_create(port, port->name);
	atomic_inc(&tty_ops.port_installed_num);
	spin_lock(&port->port_update_lock);
	port->chn_crt_stat = CCCI_CHAN_ENABLE;
	spin_unlock(&port->port_update_lock);
}

static void port_tty_uninit(struct t7xx_port *port)
{
	port->minor -= TTY_PORT_MINOR_BASE;

	if (port->chn_crt_stat != CCCI_CHAN_ENABLE)
		return;

	/* destroy tty port */
	tty_ops.tty_port_destroy(port);
	spin_lock(&port->port_update_lock);
	port->chn_crt_stat = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);

	/* CCCI tty driver exit */
	if (atomic_dec_and_test(&tty_ops.port_installed_num) && tty_ops.exit) {
		tty_ops.exit();
		tty_ops.tty_driver_status = false;
	}
}

static int port_tty_enable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_ENABLE;
	spin_unlock(&port->port_update_lock);
	if (port->chn_crt_stat != port->chan_enable) {
		port->flags &= ~PORT_F_RX_ALLOW_DROP;
		/* create a tty port */
		tty_ops.tty_port_create(port, port->name);
		spin_lock(&port->port_update_lock);
		port->chn_crt_stat = CCCI_CHAN_ENABLE;
		spin_unlock(&port->port_update_lock);
		atomic_inc(&tty_ops.port_installed_num);
	}

	return 0;
}

static int port_tty_disable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);
	return 0;
}

struct port_ops tty_port_ops = {
	.init = &port_tty_init,
	.recv_skb = &port_tty_recv_skb,
	.md_state_notify = &port_tty_md_state_notify,
	.uninit = &port_tty_uninit,
	.enable_chl = &port_tty_enable_chl,
	.disable_chl = &port_tty_disable_chl,
};
