// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":port_wwan: " fmt

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/wwan.h>

#include "t7xx_buffer_manager.h"
#include "t7xx_common.h"
#include "t7xx_monitor.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_port_config.h"
#include "t7xx_port_proxy.h"

#define MAX_QUEUE_LENGTH 32

static int mtk_port_ctrl_start(struct wwan_port *port)
{
	struct t7xx_port *port_mtk = wwan_port_get_drvdata(port);
	int ret = 0;

	if (strcmp(port_mtk->name, "brom_download") != 0) {
		if (atomic_read(&port_mtk->usage_cnt))
			ret = -EBUSY;
	}
	atomic_inc(&port_mtk->usage_cnt);
	return ret;
}

static void mtk_port_ctrl_stop(struct wwan_port *port)
{
	struct t7xx_port *port_mtk = wwan_port_get_drvdata(port);

	atomic_dec(&port_mtk->usage_cnt);
}

static int mtk_port_ctrl_tx(struct wwan_port *port, struct sk_buff *skb)
{
	struct t7xx_port *port_ccci = wwan_port_get_drvdata(port);
	unsigned char blocking = 0x01;
	struct sk_buff *skb_ccci = NULL;
	struct ccci_header *ccci_h = NULL;
	size_t actual_count = 0, alloc_size = 0, txq_mtu = 0;
	int ret = 0;
	int md_state;
	int i, multi_packet = 1;
	unsigned int count = skb->len;

	if (count == 0)
		return -EINVAL;

	md_state = ccci_fsm_get_md_state();
	if (md_state == BOOT_WAITING_FOR_HS1 || md_state == BOOT_WAITING_FOR_HS2) {
		pr_err("port %s ch%d write fail when md_state=%d!\n",
		       port_ccci->name, port_ccci->tx_ch, md_state);
		return -ENODEV;
	}

	if (port_write_room_to_md(port_ccci) <= 0 && !blocking)
		return -EAGAIN;

	txq_mtu = ccci_cldma_txq_mtu(port_ccci->txq_index);

	if (port_ccci->flags & PORT_F_RAW_DATA || port_ccci->flags & PORT_F_USER_HEADER) {
		if (port_ccci->flags & PORT_F_USER_HEADER && count > txq_mtu) {
			pr_err("reject packet(size=%u), larger than MTU on %s\n",
			       count, port_ccci->name);
			ret = -ENOMEM;
			goto err_out;
		} else {
			alloc_size = min_t(size_t, txq_mtu, count);
			actual_count = alloc_size;
		}
	} else {
		alloc_size = min_t(size_t, txq_mtu, count + CCCI_H_ELEN);
		actual_count = alloc_size - CCCI_H_ELEN;
		if ((count + CCCI_H_ELEN) > txq_mtu &&
		    (port_ccci->tx_ch == CCCI_MBIM_TX ||
		     (port_ccci->tx_ch >= CCCI_DSS0_TX && port_ccci->tx_ch <= CCCI_DSS7_TX))) {
			multi_packet = DIV_ROUND_UP(count, txq_mtu - CCCI_H_ELEN);
		}
	}
	/* loop start */
	for (i = 0; i < multi_packet; i++) {
		if (multi_packet > 1 && multi_packet == i + 1) {
			actual_count = count % (txq_mtu - CCCI_H_ELEN);
			alloc_size = actual_count + CCCI_H_ELEN;
		}
		skb_ccci = ccci_alloc_skb(alloc_size, 1, blocking);
		if (skb_ccci) {
			if (port_ccci->flags & PORT_F_RAW_DATA) {
				/* get user data */
				memcpy(skb_put(skb_ccci, actual_count),
				       skb->data, actual_count);

				if (port_ccci->flags & PORT_F_USER_HEADER) {
				/* ccci_header provided by user,
				 * For only send ccci_header without additional data
				 * case, data[0]=CCCI_MAGIC_NUM, data[1]=user_data,
				 * ch=tx_channel, reserved=no_use For send ccci_header
				 * with additional data case, data[0]=0,
				 * data[1]=data_size, ch=tx_channel, reserved=user_data
				 */
					ccci_h = (struct ccci_header *)skb->data;
					if (actual_count == CCCI_H_LEN)
						ccci_h->data[0] = CCCI_MAGIC_NUM;
					else
						ccci_h->data[1] = actual_count;

					ccci_h->channel = port_ccci->tx_ch;
				}
			} else {
				/* ccci_header is provided by driver */
				ccci_h = (struct ccci_header *)skb_put(skb_ccci, CCCI_H_LEN);
				ccci_h->data[0] = 0;
				ccci_h->data[1] = actual_count + CCCI_H_LEN;
				ccci_h->channel = port_ccci->tx_ch;
				ccci_h->reserved = 0;

				/* get user data */
				memcpy(skb_put(skb_ccci, actual_count),
				       skb->data + i * (txq_mtu - CCCI_H_ELEN), actual_count);
			}
			/* send out */
			port_inc_tx_seq_num(port_ccci, ccci_h);
			ret = port_send_skb_to_md(port_ccci, skb_ccci, blocking);
			if (ret) {
				if (ret == -EBUSY && !blocking)
					ret = -EAGAIN;
				goto err_out;
			} else {
				/* record the port seq_num after the data is sent to HIF */
				port_ccci->seq_nums[MTK_OUT]++;
			}
			if (multi_packet == 1)
				return actual_count;
			else if (multi_packet == i + 1)
				return count;
		} else {
			/* consider this case as non-blocking */
			return -EBUSY;
		}
	} /* loop end */

err_out:
	if (ret != ENOMEM) {
		pr_err("write error done on %s, size=%zu, ret=%d\n", port_ccci->name, actual_count,
		       ret);
		ccci_free_skb(skb_ccci);
		kfree_skb(skb);
	}
	return ret;
}

static const struct wwan_port_ops mtk_wwan_port_ops = {
	.start = mtk_port_ctrl_start,
	.stop = mtk_port_ctrl_stop,
	.tx = mtk_port_ctrl_tx,
};

static int port_wwan_init(struct t7xx_port *port)
{
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;

	if (!(port->flags & PORT_F_RAW_DATA))
		port->flags |= PORT_F_ADJUST_HEADER;
	if (port->rx_ch == CCCI_UART2_RX)
		port->flags |= PORT_F_CH_TRAFFIC;
	if (port->mtk_port_type != WWAN_PORT_UNKNOWN)
		port->mtk_wwan_port =
			wwan_create_port(port->dev, port->mtk_port_type, &mtk_wwan_port_ops, port);
	else
		port->mtk_wwan_port = NULL;

	return 0;
}

static void port_wwan_uninit(struct t7xx_port *port)
{
	if (port->mtk_wwan_port) {
		if (port->chn_crt_stat == CCCI_CHAN_ENABLE) {
			spin_lock(&port->port_update_lock);
			port->chn_crt_stat = CCCI_CHAN_DISABLE;
			spin_unlock(&port->port_update_lock);
		}
		wwan_remove_port(port->mtk_wwan_port);
		port->mtk_wwan_port = NULL;
	}
}

static int port_wwan_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		if (!atomic_read(&port->usage_cnt)) {
			pr_err("port %s is not opened, drop packets\n", port->name);
			return -EDROPPACKET;
		}
	}
	return port_recv_skb(port, skb);
}

static inline int port_status_update(struct t7xx_port *port)
{
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		if (port->chan_enable == CCCI_CHAN_ENABLE) {
			pr_debug("port %s is created\n", port->name);
			port->flags &= ~PORT_F_ALLOW_DROP;
		} else {
			pr_debug("port %s is removed\n", port->name);
			port->flags |= PORT_F_ALLOW_DROP;
			spin_lock(&port->port_update_lock);
			port->chn_crt_stat = CCCI_CHAN_DISABLE;
			spin_unlock(&port->port_update_lock);
			port_broadcast_state(port, PORT_STATE_DISABLE);
		}
	}
	return 0;
}

static int port_wwan_enable_chl(struct t7xx_port *port)
{
	int err = 0;

	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_ENABLE;
	spin_unlock(&port->port_update_lock);

	if (port->chn_crt_stat != port->chan_enable)
		err = port_status_update(port);

	return err;
}

static int port_wwan_disable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);

	if (port->chn_crt_stat != port->chan_enable)
		port_status_update(port);

	return 0;
}

static void port_wwan_md_state_notify(struct t7xx_port *port, unsigned int state)
{
	if (state == READY)
		port_status_update(port);
}

struct port_ops wwan_sub_port_ops = {
	.init = &port_wwan_init,
	.recv_skb = &port_wwan_recv_skb,
	.uninit = &port_wwan_uninit,
	.enable_chl = &port_wwan_enable_chl,
	.disable_chl = &port_wwan_disable_chl,
	.md_state_notify = &port_wwan_md_state_notify,
};
