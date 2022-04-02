// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/debugfs.h>
#include <linux/relay.h>
#include <linux/skbuff.h>
#include <linux/wwan.h>

#include "t7xx_common.h"
#include "t7xx_port.h"
#include "t7xx_port_proxy.h"
#include "t7xx_state_monitor.h"

#define T7XX_TRC_SUB_BUFF_SIZE		131072
#define T7XX_TRC_N_SUB_BUFF		32
#define T7XX_TRC_FILE_PERM		0660

struct t7xx_trace {
	struct rchan			*t7xx_rchan;
	struct dentry			*ctrl_file;
};

static struct dentry *t7xx_trace_create_buf_file_handler(const char *filename,
							 struct dentry *parent,
							 umode_t mode,
							 struct rchan_buf *buf,
							 int *is_global)
{
	*is_global = 1;
	return debugfs_create_file(filename, mode, parent, buf,
				   &relay_file_operations);
}

static int t7xx_trace_remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);
	return 0;
}

static int t7xx_trace_subbuf_start_handler(struct rchan_buf *buf, void *subbuf,
					   void *prev_subbuf,
					   size_t prev_padding)
{
	if (relay_buf_full(buf)) {
		pr_err_ratelimited("Relay_buf full dropping traces");
		return 0;
	}

	return 1;
}

static struct rchan_callbacks relay_callbacks = {
	.subbuf_start = t7xx_trace_subbuf_start_handler,
	.create_buf_file = t7xx_trace_create_buf_file_handler,
	.remove_buf_file = t7xx_trace_remove_buf_file_handler,
};

static ssize_t t7xx_port_trace_write(struct file *file, const char __user *buf,
				     size_t len, loff_t *ppos)
{
	struct t7xx_port *port = file->private_data;
	size_t actual_len, alloc_size, txq_mtu;
	struct t7xx_port_static *port_static;
	struct cldma_ctrl *md_ctrl;
	struct ccci_header *ccci_h;
	enum md_state md_state;
	struct sk_buff *skb;
	int ret;

	port_static = port->port_static;
	md_state = t7xx_fsm_get_md_state(port->t7xx_dev->md->fsm_ctl);

	if (md_state == MD_STATE_WAITING_FOR_HS1 || md_state == MD_STATE_WAITING_FOR_HS2) {
		dev_warn(port->dev, "port: %s ch: %d, write fail when md_state: %d\n",
			 port_static->name, port_static->tx_ch, md_state);
		return -ENODEV;
	}

	if (t7xx_port_write_room_to_md(port) <= 0)
		return -EAGAIN;

	md_ctrl = port->t7xx_dev->md->md_ctrl[port_static->path_id];

	txq_mtu = cldma_txq_mtu(md_ctrl, port_static->txq_index);
	alloc_size = min_t(size_t, txq_mtu, len + CCCI_H_ELEN);
	actual_len = alloc_size - CCCI_H_ELEN;

	skb = __dev_alloc_skb(alloc_size, GFP_KERNEL);
	if (!skb) {
		ret = -ENOMEM;
		goto err_out;
	}

	ccci_h = skb_put(skb, CCCI_H_LEN);
	t7xx_ccci_header_init(ccci_h, 0, actual_len + CCCI_H_LEN, port_static->tx_ch, 0);

	ret = copy_from_user(skb_put(skb, actual_len), buf, actual_len);
	if (ret)
		goto err_out;

	t7xx_port_proxy_set_tx_seq_num(port, ccci_h);

	ret = t7xx_port_send_skb_to_md(port, skb);
	if (ret)
		goto err_out;

	port->seq_nums[MTK_TX]++;

	return actual_len;

err_out:
	dev_err(port->dev, "write error done on %s, size: %zu, ret: %d\n",
		port_static->name, actual_len, ret);
	dev_kfree_skb(skb);
	return ret;
}

static const struct file_operations t7xx_trace_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = t7xx_port_trace_write,
};

static int t7xx_trace_port_init(struct t7xx_port *port)
{
	struct dentry *debugfs_pdev = wwan_get_debugfs_dir(port->dev);

	if (IS_ERR(debugfs_pdev))
		debugfs_pdev = NULL; // WWAN_DEBUGFS not working

	port->debugfs_dir = debugfs_create_dir(KBUILD_MODNAME, debugfs_pdev);
	if (IS_ERR_OR_NULL(port->debugfs_dir))
		return -ENOMEM;

	port->trace = devm_kzalloc(port->dev, sizeof(*port->trace), GFP_KERNEL);
	if (!port->trace)
		goto err_debugfs_dir;

	port->trace->ctrl_file = debugfs_create_file("mdlog_ctrl",
						     T7XX_TRC_FILE_PERM,
						     port->debugfs_dir,
						     port,
						     &t7xx_trace_fops);
	if (!port->trace->ctrl_file)
		goto err_debugfs_dir;

	port->trace->t7xx_rchan = relay_open("relay_ch",
					     port->debugfs_dir,
					     T7XX_TRC_SUB_BUFF_SIZE,
					     T7XX_TRC_N_SUB_BUFF,
					     &relay_callbacks, NULL);
	if (!port->trace->t7xx_rchan)
		goto err_debugfs_dir;

	return 0;

err_debugfs_dir:
	debugfs_remove_recursive(port->debugfs_dir);
	return -ENOMEM;
}

static void t7xx_trace_port_uninit(struct t7xx_port *port)
{
	struct t7xx_trace *trace = port->trace;

	relay_close(trace->t7xx_rchan);
	debugfs_remove_recursive(port->debugfs_dir);
}

static int t7xx_trace_port_recv_skb(struct t7xx_port *port, struct sk_buff *skb)
{
	struct t7xx_trace *t7xx_trace = port->trace;

	if (!t7xx_trace->t7xx_rchan)
		return -EINVAL;

	relay_write(t7xx_trace->t7xx_rchan, skb->data, skb->len);
	dev_kfree_skb(skb);
	return 0;
}

struct port_ops t7xx_trace_port_ops = {
	.init =	t7xx_trace_port_init,
	.recv_skb = t7xx_trace_port_recv_skb,
	.uninit = t7xx_trace_port_uninit,
};
