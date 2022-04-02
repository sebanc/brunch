// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 *
 * Authors: Haijun Lio <haijun.liu@mediatek.com>
 * Contributors: Amir Hanania <amir.hanania@intel.com>
 *               Chiranjeevi Rapolu <chiranjeevi.rapolu@intel.com>
 *               Eliot Lee <eliot.lee@intel.com>
 *               Moises Veleta <moises.veleta@intel.com>
 *               Ricardo Martinez<ricardo.martinez@linux.intel.com>
 *               Sreehari Kancharla <sreehari.kancharla@intel.com>
 */

#include <linux/bitfield.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#include "t7xx_common.h"
#include "t7xx_state_monitor.h"
#include "t7xx_port.h"
#include "t7xx_port_proxy.h"

static __poll_t port_char_poll(struct file *fp, struct poll_table_struct *poll)
{
	struct t7xx_port *port;
	struct t7xx_port_static *port_static;
	enum md_state md_state;
	__poll_t mask = 0;

	port = fp->private_data;
	md_state = t7xx_fsm_get_md_state(port->t7xx_dev->md->fsm_ctl);
	poll_wait(fp, &port->rx_wq, poll);

	spin_lock_irq(&port->rx_wq.lock);
	if (!skb_queue_empty(&port->rx_skb_list))
		mask |= EPOLLIN | EPOLLRDNORM;

	spin_unlock_irq(&port->rx_wq.lock);
	if (t7xx_port_write_room_to_md(port) > 0)
		mask |= EPOLLOUT | EPOLLWRNORM;

	port_static = port->port_static;
	if (port_static->rx_ch == PORT_CH_UART1_RX &&
	    md_state != MD_STATE_READY &&
	    md_state != MD_STATE_EXCEPTION) {
		/* notify MD logger to save its log before md_init kills it */
		mask |= EPOLLERR;
		dev_err(port->dev, "poll error for MD logger at state: %d, mask: %u\n",
			md_state, mask);
	}

	return mask;
}

/**
 * port_char_open() - open char port
 * @inode: pointer to inode structure
 * @file: pointer to file structure
 *
 * Open a char port using pre-defined md_ccci_ports structure in port_proxy
 *
 * Return: 0 for success, -EINVAL for failure
 */
static int port_char_open(struct inode *inode, struct file *file)
{
	int major = imajor(inode);
	int minor = iminor(inode);
	struct t7xx_port *port;

	port = port_proxy_get_port(major, minor);

	if (!port)
		return -EINVAL;

	atomic_inc(&port->usage_cnt);
	file->private_data = port;
	return nonseekable_open(inode, file);
}

static int port_char_close(struct inode *inode, struct file *file)
{
	struct t7xx_port *port;
	struct sk_buff *skb;

	port = file->private_data;
	/* decrease usage count, so when we ask again,
	 * the packet can be dropped in recv_request.
	 */
	atomic_dec(&port->usage_cnt);

	/* purge RX request list */
	spin_lock_irq(&port->rx_wq.lock);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
		dev_kfree_skb(skb);

	spin_unlock_irq(&port->rx_wq.lock);
	return 0;
}

static ssize_t port_char_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	bool full_req_done = false;
	struct t7xx_port *port;
	int ret = 0, read_len;
	struct sk_buff *skb;

	port = file->private_data;
	spin_lock_irq(&port->rx_wq.lock);
	if (skb_queue_empty(&port->rx_skb_list)) {
		if (file->f_flags & O_NONBLOCK) {
			spin_unlock_irq(&port->rx_wq.lock);
			return -EAGAIN;
		}

		ret = wait_event_interruptible_locked_irq(port->rx_wq,
							  !skb_queue_empty(&port->rx_skb_list));
		if (ret == -ERESTARTSYS) {
			spin_unlock_irq(&port->rx_wq.lock);
			return -EINTR;
		}
	}

	skb = skb_peek(&port->rx_skb_list);

	if (count >= skb->len) {
		read_len = skb->len;
		full_req_done = true;
		__skb_unlink(skb, &port->rx_skb_list);
	} else {
		read_len = count;
	}

	spin_unlock_irq(&port->rx_wq.lock);
	if (copy_to_user(buf, skb->data, read_len)) {
		dev_err(port->dev, "read on %s, copy to user failed, %d/%zu\n",
			port->port_static->name, read_len, count);
		ret = -EFAULT;
	}

	skb_pull(skb, read_len);
	if (full_req_done)
		dev_kfree_skb(skb);

	return ret ? ret : read_len;
}

static ssize_t port_char_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	size_t actual_count, alloc_size, txq_mtu;
	struct t7xx_port_static *port_static;
	struct cldma_ctrl *md_ctrl;
	int i, multi_packet = 1;
	struct t7xx_port *port;
	enum md_state md_state;
	struct sk_buff *skb;
	bool blocking;
	int ret;

	blocking = !(file->f_flags & O_NONBLOCK);
	port = file->private_data;
	port_static = port->port_static;
	md_state = t7xx_fsm_get_md_state(port->t7xx_dev->md->fsm_ctl);

	if (md_state == MD_STATE_WAITING_FOR_HS1 || md_state == MD_STATE_WAITING_FOR_HS2) {
		dev_warn(port->dev, "port: %s ch: %d, write fail when md_state: %d\n",
			 port_static->name, port_static->tx_ch, md_state);
		return -ENODEV;
	}

	if (t7xx_port_write_room_to_md(port) <= 0 && !blocking)
		return -EAGAIN;

	md_ctrl = port->t7xx_dev->md->md_ctrl[port_static->path_id];

	txq_mtu = cldma_txq_mtu(md_ctrl, port_static->txq_index);
	if (port_static->flags & PORT_F_RAW_DATA || port_static->flags & PORT_F_USER_HEADER) {
		if (port_static->flags & PORT_F_USER_HEADER && count > txq_mtu) {
			dev_err(port->dev, "packet size: %zu larger than MTU on %s\n",
				count, port_static->name);
			return -ENOMEM;
		}

		actual_count = count > txq_mtu ? txq_mtu : count;
		alloc_size = actual_count;
	} else {
		actual_count = (count + CCCI_H_ELEN) > txq_mtu ? (txq_mtu - CCCI_H_ELEN) : count;
		alloc_size = actual_count + CCCI_H_ELEN;
		if (count + CCCI_H_ELEN > txq_mtu && (port_static->tx_ch == PORT_CH_MBIM_TX ||
						      (port_static->tx_ch >= PORT_CH_DSS0_TX &&
						       port_static->tx_ch <= PORT_CH_DSS7_TX))) {
			multi_packet = (count + txq_mtu - CCCI_H_ELEN - 1) /
					(txq_mtu - CCCI_H_ELEN);
		}
	}

	for (i = 0; i < multi_packet; i++) {
		struct ccci_header *ccci_h = NULL;

		if (multi_packet > 1 && multi_packet == i + 1) {
			actual_count = count % (txq_mtu - CCCI_H_ELEN);
			alloc_size = actual_count + CCCI_H_ELEN;
		}

		skb = __dev_alloc_skb(alloc_size, GFP_KERNEL);
		if (!skb) {
			ret = -ENOMEM;
			goto err_out;
		}

		/* Get the user data, no need to validate the data since the driver is just
		 * passing it to the device.
		 */
		if (port->flags & PORT_F_RAW_DATA) {
			ret = copy_from_user(skb_put(skb, actual_count), buf, actual_count);
			if (port->flags & PORT_F_USER_HEADER) {
				/* The ccci_header is provided by user.
				 *
				 * For only sending ccci_header without additional data
				 * case, data[1]=CCCI_HEADER_NO_DATA, data[1]=user_data,
				 * ch=tx_channel, reserved=no_use.
				 *
				 * For send ccci_header with additional data case,
				 * data[0]=0, data[1]=data_size, ch=tx_channel,
				 * reserved=user_data.
				 */
				ccci_h = (struct ccci_header *)skb->data;
				if (actual_count == CCCI_H_LEN)
					ccci_h->packet_header = cpu_to_le32(CCCI_HEADER_NO_DATA);
				else
					ccci_h->packet_len = cpu_to_le32(actual_count);

				ccci_h->status &= cpu_to_le32(~HDR_FLD_CHN);
				ccci_h->status |= cpu_to_le32(FIELD_PREP(HDR_FLD_CHN,
									 port_static->tx_ch));
			}
		} else {
			/* ccci_header is provided by driver */
			ccci_h = skb_put(skb, CCCI_H_LEN);
			ccci_h->packet_header = 0;
			ccci_h->packet_len = cpu_to_le32(actual_count + CCCI_H_LEN);
			ccci_h->status &= cpu_to_le32(~HDR_FLD_CHN);
			ccci_h->status |= cpu_to_le32(FIELD_PREP(HDR_FLD_CHN, port_static->tx_ch));
			ccci_h->ex_msg = 0;

			ret = copy_from_user(skb_put(skb, actual_count),
					     buf + i * (txq_mtu - CCCI_H_ELEN), actual_count);
		}

		if (ret) {
			ret = -EFAULT;
			goto err_out;
		}

		/* send out */
		t7xx_port_proxy_set_tx_seq_num(port, ccci_h);
		//FIXME: Do we need to bring back the blocking option?
		ret = t7xx_port_send_skb_to_md(port, skb);
		if (ret) {
			if (ret == -EBUSY && !blocking)
				ret = -EAGAIN;

			goto err_out;
		} else {
			/* Record the port seq_num after the data is sent to HIF.
			 * Only bits 0-14 are used, thus negating overflow.
			 */
			port->seq_nums[MTK_TX]++;
		}

		if (multi_packet == 1)
			return actual_count;
		else if (multi_packet == i + 1)
			return count;
	}

err_out:
	dev_err(port->dev, "write error done on %s, size: %zu, ret: %d\n",
		port_static->name, actual_count, ret);
	dev_kfree_skb(skb);
	return ret;
}

static const struct file_operations char_fops = {
	.owner = THIS_MODULE,
	.open = &port_char_open,
	.read = &port_char_read,
	.write = &port_char_write,
	.release = &port_char_close,
	.poll = &port_char_poll,
};

static int port_char_init(struct t7xx_port *port)
{
	struct cdev *dev;
	struct t7xx_port_static *port_static = port->port_static;

	port->rx_length_th = RX_QUEUE_MAXLEN;
	if (port->flags & PORT_F_RX_CHAR_NODE) {
		dev = cdev_alloc();
		if (!dev)
			return -ENOMEM;

		dev->ops = &char_fops;
		dev->owner = THIS_MODULE;
		if (cdev_add(dev, MKDEV(port_static->major,
					port_static->minor_base + port_static->minor), 1)) {
			kobject_put(&dev->kobj);
			return -ENOMEM;
		}

		if (!(port->flags & PORT_F_RAW_DATA))
			port->flags |= PORT_F_RX_ADJUST_HEADER;

		port->cdev = dev;
	}

	if (port_static->rx_ch == PORT_CH_UART2_RX)
		port->flags |= PORT_F_RX_CH_TRAFFIC;

	return 0;
}

static void port_char_uninit(struct t7xx_port *port)
{
	unsigned long flags;
	struct sk_buff *skb;
	struct t7xx_port_static *port_static = port->port_static;

	if (port->flags & PORT_F_RX_CHAR_NODE && port->cdev) {
		if (port->chn_crt_stat == CCCI_CHAN_ENABLE) {
			port_unregister_device(port_static->major,
					       port_static->minor_base + port_static->minor);
			spin_lock(&port->port_update_lock);
			port->chn_crt_stat = CCCI_CHAN_DISABLE;
			spin_unlock(&port->port_update_lock);
		}

		cdev_del(port->cdev);
		port->cdev = NULL;
	}

	/* interrupts need to be disabled */
	spin_lock_irqsave(&port->rx_wq.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
		dev_kfree_skb(skb);
	spin_unlock_irqrestore(&port->rx_wq.lock, flags);
}

static int port_char_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	struct t7xx_port_static *port_static = port->port_static;

	if ((port_static->flags & PORT_F_RX_CHAR_NODE) && !atomic_read(&port->usage_cnt)) {
		dev_kfree_skb_any(skb);
		dev_err_ratelimited(port->dev,
				    "port %s is not opened, dropping packets\n", port_static->name);
		return 0;
	}
	return t7xx_port_recv_skb(port, skb);
}

static int port_status_update(struct t7xx_port *port)
{
	struct t7xx_port_static *port_static = port->port_static;

	if (!(port_static->flags & PORT_F_RX_CHAR_NODE) || port->chn_crt_stat == port->chan_enable)
		return 0;

	/* Updating the channel status before the register to diverge the following calls */
	spin_lock(&port->port_update_lock);
	port->chn_crt_stat = !port->chn_crt_stat;
	spin_unlock(&port->port_update_lock);

	if (port->chan_enable == CCCI_CHAN_ENABLE) {
		int ret;

		port_static->flags &= ~PORT_F_RX_ALLOW_DROP;
		ret = port_register_device(port_static->name, port_static->major,
					   port_static->minor_base + port_static->minor);
		if (ret) {
			spin_lock(&port->port_update_lock);
			port->chn_crt_stat = !port->chn_crt_stat;
			spin_unlock(&port->port_update_lock);
			return ret;
		}
		port_proxy_broadcast_state(port, MTK_PORT_STATE_ENABLE);

		return 0;
	}

	port_static->flags |= PORT_F_RX_ALLOW_DROP;
	port_unregister_device(port_static->major, port_static->minor_base + port_static->minor);
	return port_proxy_broadcast_state(port, MTK_PORT_STATE_DISABLE);
}

static int port_char_enable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_ENABLE;
	spin_unlock(&port->port_update_lock);

	return port_status_update(port);
}

static int port_char_disable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);

	return port_status_update(port);
}

static void port_char_md_state_notify(struct t7xx_port *port, unsigned int state)
{
	if (state == MD_STATE_READY)
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
