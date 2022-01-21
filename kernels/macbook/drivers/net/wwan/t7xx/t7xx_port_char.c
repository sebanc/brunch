// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#include "t7xx_common.h"
#include "t7xx_monitor.h"
#include "t7xx_port.h"
#include "t7xx_port_proxy.h"
#include "t7xx_skb_util.h"

static __poll_t port_char_poll(struct file *fp, struct poll_table_struct *poll)
{
	struct t7xx_port *port;
	enum md_state md_state;
	__poll_t mask = 0;

	port = fp->private_data;
	md_state = ccci_fsm_get_md_state();
	poll_wait(fp, &port->rx_wq, poll);

	spin_lock_irq(&port->rx_wq.lock);
	if (!skb_queue_empty(&port->rx_skb_list))
		mask |= EPOLLIN | EPOLLRDNORM;

	spin_unlock_irq(&port->rx_wq.lock);
	if (port_write_room_to_md(port) > 0)
		mask |= EPOLLOUT | EPOLLWRNORM;

	if (port->rx_ch == CCCI_UART1_RX &&
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
	int clear_cnt = 0;

	port = file->private_data;
	/* decrease usage count, so when we ask again,
	 * the packet can be dropped in recv_request.
	 */
	atomic_dec(&port->usage_cnt);

	/* purge RX request list */
	spin_lock_irq(&port->rx_wq.lock);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL) {
		ccci_free_skb(&port->mtk_dev->pools, skb);
		clear_cnt++;
	}

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
			port->name, read_len, count);
		ret = -EFAULT;
	}

	skb_pull(skb, read_len);
	if (full_req_done)
		ccci_free_skb(&port->mtk_dev->pools, skb);

	return ret ? ret : read_len;
}

static ssize_t port_char_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	size_t actual_count, alloc_size, txq_mtu;
	int i, multi_packet = 1;
	struct t7xx_port *port;
	enum md_state md_state;
	struct sk_buff *skb;
	bool blocking;
	int ret;

	blocking = !(file->f_flags & O_NONBLOCK);
	port = file->private_data;
	md_state = ccci_fsm_get_md_state();
	if (md_state == MD_STATE_WAITING_FOR_HS1 || md_state == MD_STATE_WAITING_FOR_HS2) {
		dev_warn(port->dev, "port: %s ch: %d, write fail when md_state: %d\n",
			 port->name, port->tx_ch, md_state);
		return -ENODEV;
	}

	if (port_write_room_to_md(port) <= 0 && !blocking)
		return -EAGAIN;

	txq_mtu = cldma_txq_mtu(port->txq_index);
	if (port->flags & PORT_F_RAW_DATA || port->flags & PORT_F_USER_HEADER) {
		if (port->flags & PORT_F_USER_HEADER && count > txq_mtu) {
			dev_err(port->dev, "packet size: %zu larger than MTU on %s\n",
				count, port->name);
			return -ENOMEM;
		}

		actual_count = count > txq_mtu ? txq_mtu : count;
		alloc_size = actual_count;
	} else {
		actual_count = (count + CCCI_H_ELEN) > txq_mtu ? (txq_mtu - CCCI_H_ELEN) : count;
		alloc_size = actual_count + CCCI_H_ELEN;
		if (count + CCCI_H_ELEN > txq_mtu && (port->tx_ch == CCCI_MBIM_TX ||
						      (port->tx_ch >= CCCI_DSS0_TX &&
						       port->tx_ch <= CCCI_DSS7_TX))) {
			multi_packet = (count + txq_mtu - CCCI_H_ELEN - 1) /
					(txq_mtu - CCCI_H_ELEN);
		}
	}
	mutex_lock(&port->tx_mutex_lock);
	for (i = 0; i < multi_packet; i++) {
		struct ccci_header *ccci_h = NULL;

		if (multi_packet > 1 && multi_packet == i + 1) {
			actual_count = count % (txq_mtu - CCCI_H_ELEN);
			alloc_size = actual_count + CCCI_H_ELEN;
		}

		skb = ccci_alloc_skb_from_pool(&port->mtk_dev->pools, alloc_size, blocking);
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
				 * case, data[0]=CCCI_HEADER_NO_DATA, data[1]=user_data,
				 * ch=tx_channel, reserved=no_use.
				 *
				 * For send ccci_header with additional data case,
				 * data[0]=0, data[1]=data_size, ch=tx_channel,
				 * reserved=user_data.
				 */
				ccci_h = (struct ccci_header *)skb->data;
				if (actual_count == CCCI_H_LEN)
					ccci_h->data[0] = CCCI_HEADER_NO_DATA;
				else
					ccci_h->data[1] = actual_count;

				ccci_h->status &= ~HDR_FLD_CHN;
				ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, port->tx_ch);
			}
		} else {
			/* ccci_header is provided by driver */
			ccci_h = skb_put(skb, CCCI_H_LEN);
			ccci_h->data[0] = 0;
			ccci_h->data[1] = actual_count + CCCI_H_LEN;
			ccci_h->status &= ~HDR_FLD_CHN;
			ccci_h->status |= FIELD_PREP(HDR_FLD_CHN, port->tx_ch);
			ccci_h->reserved = 0;

			ret = copy_from_user(skb_put(skb, actual_count),
					     buf + i * (txq_mtu - CCCI_H_ELEN), actual_count);
		}

		if (ret) {
			ret = -EFAULT;
			goto err_out;
		}

		/* send out */
		port_proxy_set_seq_num(port, ccci_h);
		ret = port_send_skb_to_md(port, skb, blocking);
		if (ret) {
			if (ret == -EBUSY && !blocking)
				ret = -EAGAIN;

			goto err_out;
		} else {
			/* Record the port seq_num after the data is sent to HIF.
			 * Only bits 0-14 are used, thus negating overflow.
			 */
			port->seq_nums[MTK_OUT]++;
		}

		if (multi_packet == 1) {
			mutex_unlock(&port->tx_mutex_lock);
			return actual_count;
		} else if (multi_packet == i + 1) {
			mutex_unlock(&port->tx_mutex_lock);
			return count;
		}
	}

err_out:
	mutex_unlock(&port->tx_mutex_lock);
	dev_err(port->dev, "write error done on %s, size: %zu, ret: %d\n",
		port->name, actual_count, ret);
	ccci_free_skb(&port->mtk_dev->pools, skb);
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

	port->rx_length_th = MAX_RX_QUEUE_LENGTH;
	port->skb_from_pool = true;
	if (port->flags & PORT_F_RX_CHAR_NODE) {
		dev = cdev_alloc();
		if (!dev)
			return -ENOMEM;

		dev->ops = &char_fops;
		dev->owner = THIS_MODULE;
		if (cdev_add(dev, MKDEV(port->major, port->minor_base + port->minor), 1)) {
			kobject_put(&dev->kobj);
			return -ENOMEM;
		}

		if (!(port->flags & PORT_F_RAW_DATA))
			port->flags |= PORT_F_RX_ADJUST_HEADER;

		port->cdev = dev;
	}

	if (port->rx_ch == CCCI_UART2_RX)
		port->flags |= PORT_F_RX_CH_TRAFFIC;

	return 0;
}

static void port_char_uninit(struct t7xx_port *port)
{
	unsigned long flags;
	struct sk_buff *skb;

	if (port->flags & PORT_F_RX_CHAR_NODE && port->cdev) {
		if (port->chn_crt_stat == CCCI_CHAN_ENABLE) {
			port_unregister_device(port->major, port->minor_base + port->minor);
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
		ccci_free_skb(&port->mtk_dev->pools, skb);
	spin_unlock_irqrestore(&port->rx_wq.lock, flags);
}

static int port_char_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	if ((port->flags & PORT_F_RX_CHAR_NODE) && !atomic_read(&port->usage_cnt)) {
		dev_err_ratelimited(port->dev,
				    "port %s is not opened, dropping packets\n", port->name);
		return -ENETDOWN;
	}

	return port_recv_skb(port, skb);
}

static int port_status_update(struct t7xx_port *port)
{
	if (!(port->flags & PORT_F_RX_CHAR_NODE))
		return 0;

	if (port->chan_enable == CCCI_CHAN_ENABLE) {
		int ret;

		port->flags &= ~PORT_F_RX_ALLOW_DROP;
		ret = port_register_device(port->name, port->major,
					   port->minor_base + port->minor);
		if (ret)
			return ret;

		port_proxy_broadcast_state(port, MTK_PORT_STATE_ENABLE);
		spin_lock(&port->port_update_lock);
		port->chn_crt_stat = CCCI_CHAN_ENABLE;
		spin_unlock(&port->port_update_lock);

		return 0;
	}

	port->flags |= PORT_F_RX_ALLOW_DROP;
	port_unregister_device(port->major, port->minor_base + port->minor);
	spin_lock(&port->port_update_lock);
	port->chn_crt_stat = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);
	return port_proxy_broadcast_state(port, MTK_PORT_STATE_DISABLE);
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
		return port_status_update(port);

	return 0;
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
