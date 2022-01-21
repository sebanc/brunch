// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/wwan.h>
#include <linux/bitfield.h>

#include "t7xx_common.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_monitor.h"
#include "t7xx_port_proxy.h"
#include "t7xx_skb_util.h"

static int mtk_port_ctrl_start(struct wwan_port *port)
{
	struct t7xx_port *port_mtk;

	port_mtk = wwan_port_get_drvdata(port);

	if (atomic_read(&port_mtk->usage_cnt))
		return -EBUSY;

	atomic_inc(&port_mtk->usage_cnt);
	return 0;
}

static void mtk_port_ctrl_stop(struct wwan_port *port)
{
	struct t7xx_port *port_mtk;

	port_mtk = wwan_port_get_drvdata(port);

	atomic_dec(&port_mtk->usage_cnt);
}

static int mtk_port_ctrl_tx(struct wwan_port *port, struct sk_buff *skb)
{
	size_t actual_count = 0, alloc_size = 0, txq_mtu = 0;
	struct sk_buff *skb_ccci = NULL;
	struct t7xx_port *port_ccci;
	int i, multi_packet = 1;
	enum md_state md_state;
	unsigned int count;
	int ret = 0;

	count = skb->len;
	if (!count)
		return -EINVAL;

	port_ccci = wwan_port_get_drvdata(port);
	md_state = ccci_fsm_get_md_state();
	if (md_state == MD_STATE_WAITING_FOR_HS1 || md_state == MD_STATE_WAITING_FOR_HS2) {
		dev_warn(port_ccci->dev, "port %s ch%d write fail when md_state=%d!\n",
			 port_ccci->name, port_ccci->tx_ch, md_state);
		return -ENODEV;
	}

	txq_mtu = CLDMA_TXQ_MTU;

	if (port_ccci->flags & PORT_F_RAW_DATA || port_ccci->flags & PORT_F_USER_HEADER) {
		if (port_ccci->flags & PORT_F_USER_HEADER && count > txq_mtu) {
			dev_err(port_ccci->dev, "reject packet(size=%u), larger than MTU on %s\n",
				count, port_ccci->name);
			return -ENOMEM;
		}

		alloc_size = min_t(size_t, txq_mtu, count);
		actual_count = alloc_size;
	} else {
		alloc_size = min_t(size_t, txq_mtu, count + CCCI_H_ELEN);
		actual_count = alloc_size - CCCI_H_ELEN;
		if (count + CCCI_H_ELEN > txq_mtu &&
		    (port_ccci->tx_ch == CCCI_MBIM_TX ||
		     (port_ccci->tx_ch >= CCCI_DSS0_TX && port_ccci->tx_ch <= CCCI_DSS7_TX)))
			multi_packet = DIV_ROUND_UP(count, txq_mtu - CCCI_H_ELEN);
	}

	for (i = 0; i < multi_packet; i++) {
		struct ccci_header *ccci_h = NULL;

		if (multi_packet > 1 && multi_packet == i + 1) {
			actual_count = count % (txq_mtu - CCCI_H_ELEN);
			alloc_size = actual_count + CCCI_H_ELEN;
		}

		skb_ccci = ccci_alloc_skb_from_pool(&port_ccci->mtk_dev->pools, alloc_size,
						    GFS_BLOCKING);
		if (!skb_ccci)
			return -ENOMEM;

		if (port_ccci->flags & PORT_F_RAW_DATA) {
			memcpy(skb_put(skb_ccci, actual_count), skb->data, actual_count);

			if (port_ccci->flags & PORT_F_USER_HEADER) {
				/* The ccci_header is provided by user.
				 *
				 * For only send ccci_header without additional data
				 * case, data[0]=CCCI_HEADER_NO_DATA, data[1]=user_data,
				 * ch=tx_channel, reserved=no_use.
				 *
				 * For send ccci_header
				 * with additional data case, data[0]=0,
				 * data[1]=data_size, ch=tx_channel,
				 * reserved=user_data.
				 */
				ccci_h = (struct ccci_header *)skb->data;
				if (actual_count == CCCI_H_LEN)
					ccci_h->data[0] = CCCI_HEADER_NO_DATA;
				else
					ccci_h->data[1] = actual_count;

				ccci_h->status &= ~HDR_FLD_CHN;
				ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, port_ccci->tx_ch);
			}
		} else {
			/* ccci_header is provided by driver */
			ccci_h = skb_put(skb_ccci, CCCI_H_LEN);
			ccci_h->data[0] = 0;
			ccci_h->data[1] = actual_count + CCCI_H_LEN;
			ccci_h->status &= ~HDR_FLD_CHN;
			ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, port_ccci->tx_ch);
			ccci_h->reserved = 0;

			memcpy(skb_put(skb_ccci, actual_count),
			       skb->data + i * (txq_mtu - CCCI_H_ELEN), actual_count);
		}

		port_proxy_set_seq_num(port_ccci, ccci_h);
		ret = port_send_skb_to_md(port_ccci, skb_ccci, true);
		if (ret)
			goto err_out;

		/* record the port seq_num after the data is sent to HIF */
		port_ccci->seq_nums[MTK_OUT]++;

		if (multi_packet == 1)
			return actual_count;
		else if (multi_packet == i + 1)
			return count;
	}

err_out:
	if (ret != -ENOMEM) {
		dev_err(port_ccci->dev, "write error done on %s, size=%zu, ret=%d\n",
			port_ccci->name, actual_count, ret);
		ccci_free_skb(&port_ccci->mtk_dev->pools, skb_ccci);
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
	port->rx_length_th = MAX_RX_QUEUE_LENGTH;
	port->skb_from_pool = true;

	if (!(port->flags & PORT_F_RAW_DATA))
		port->flags |= PORT_F_RX_ADJUST_HEADER;

	if (port->rx_ch == CCCI_UART2_RX)
		port->flags |= PORT_F_RX_CH_TRAFFIC;

	if (port->mtk_port_type != WWAN_PORT_UNKNOWN) {
		port->mtk_wwan_port =
			wwan_create_port(port->dev, port->mtk_port_type, &mtk_wwan_port_ops, port);
		if (IS_ERR(port->mtk_wwan_port))
			return PTR_ERR(port->mtk_wwan_port);
	} else {
		port->mtk_wwan_port = NULL;
	}

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
	if (port->flags & PORT_F_RX_CHAR_NODE) {
		if (!atomic_read(&port->usage_cnt)) {
			dev_err_ratelimited(port->dev, "port %s is not opened, drop packets\n",
					    port->name);
			return -ENETDOWN;
		}
	}

	return port_recv_skb(port, skb);
}

static int port_status_update(struct t7xx_port *port)
{
	if (port->flags & PORT_F_RX_CHAR_NODE) {
		if (port->chan_enable == CCCI_CHAN_ENABLE) {
			port->flags &= ~PORT_F_RX_ALLOW_DROP;
		} else {
			port->flags |= PORT_F_RX_ALLOW_DROP;
			spin_lock(&port->port_update_lock);
			port->chn_crt_stat = CCCI_CHAN_DISABLE;
			spin_unlock(&port->port_update_lock);
			return port_proxy_broadcast_state(port, MTK_PORT_STATE_DISABLE);
		}
	}

	return 0;
}

static int port_wwan_enable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_ENABLE;
	spin_unlock(&port->port_update_lock);

	if (port->chn_crt_stat != port->chan_enable)
		return port_status_update(port);

	return 0;
}

static int port_wwan_disable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);

	if (port->chn_crt_stat != port->chan_enable)
		return port_status_update(port);

	return 0;
}

static void port_wwan_md_state_notify(struct t7xx_port *port, unsigned int state)
{
	if (state == MD_STATE_READY)
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
