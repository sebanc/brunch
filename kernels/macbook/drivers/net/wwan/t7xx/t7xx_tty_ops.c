// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include "t7xx_tty_ops.h"
#include "t7xx_port_proxy.h"
#include "t7xx_port_config.h"

static struct tty_ctl_block *tty_ctlb;
static struct tty_port_operations null_ops = {};

static int ccci_tty_open(struct tty_struct *tty, struct file *filp)
{
	int ret = 0;
	int tty_id;
	struct tty_port *pport;
	struct tty_driver *drv;

	drv = tty->driver;
	tty_id = tty->index;
	pport = drv->ports[tty_id];
	if (pport)
		ret = tty_port_open(pport, tty, filp);

	return ret;
}

static void ccci_tty_close(struct tty_struct *tty, struct file *filp)
{
	int tty_id;
	struct tty_port *pport;
	struct tty_driver *drv;

	drv = tty->driver;
	tty_id = tty->index;
	pport = drv->ports[tty_id];
	if (pport)
		tty_port_close(pport, tty, filp);
}

static int ccci_tty_write(struct tty_struct *tty,
			  const unsigned char *buf, int count)
{
	int ret;

	if (!(tty_ctlb && tty_ctlb->driver == tty->driver)) {
		pr_err("tty write err:%d, %p\n", tty->index, tty->driver);
		return -EFAULT;
	}

	ret = tty_ctlb->ccci_ops->send_pkt(tty->index, buf, count);
	if (ret < 0)
		pr_err("tty write err, ret=%d\n", ret);

	return ret;
}

static int ccci_tty_write_room(struct tty_struct *tty)
{
	return CCCI_MTU;
}

static const struct tty_operations ccci_serial_ops = {
	.open = ccci_tty_open,
	.close = ccci_tty_close,
	.write = ccci_tty_write,
	.write_room = ccci_tty_write_room,
};

static int ccci_tty_port_create(int minor, char *port_name)
{
	struct tty_driver *tty_drv;
	struct tty_port *pport;

	tty_drv = tty_ctlb->driver;
	tty_drv->name = port_name;

	pport = kzalloc(sizeof(*pport), GFP_KERNEL);
	if (!pport)
		return -ENOMEM;

	tty_port_init(pport);
	pport->ops = &null_ops;
	tty_port_link_device(pport, tty_drv, minor);
	tty_register_device(tty_drv, minor, NULL);

	return 0;
}

static int ccci_tty_port_destroy(int minor)
{
	struct tty_driver *tty_drv;
	struct tty_port *pport;
	struct tty_struct *tty;

	tty_drv = tty_ctlb->driver;

	pport = tty_drv->ports[minor];
	if (!pport) {
		pr_err("Invalid tty minor:%d\n", minor);
		return -EINVAL;
	}

	tty = tty_port_tty_get(pport);
	if (tty) {
		tty_vhangup(tty);
		tty_kref_put(tty);
	}

	tty_unregister_device(tty_drv, minor);
	tty_port_destroy(pport);
	kfree(pport);
	tty_drv->ports[minor] = NULL;

	return 0;
}

static int tty_ccci_init(struct tty_ccci_ops *ccci_info, struct t7xx_port *port)
{
	struct tty_driver *tty_drv;
	struct tty_ctl_block *ctlb;
	struct port_proxy *port_proxy_ptr;
	int ret, port_nr;

	if (!ccci_info || !port) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	ctlb = kzalloc(sizeof(*ctlb), GFP_KERNEL);
	if (!ctlb)
		return -ENOMEM;

	ctlb->ccci_ops = kzalloc(sizeof(*ctlb->ccci_ops), GFP_KERNEL);
	if (!ctlb->ccci_ops) {
		ret = -ENOMEM;
		goto alloc_fail;
	}

	tty_ctlb = ctlb;
	memcpy(ctlb->ccci_ops, ccci_info, sizeof(struct tty_ccci_ops));
	port_nr = ctlb->ccci_ops->tty_num;

	tty_drv = alloc_tty_driver(port_nr);
	if (!tty_drv) {
		ret = -ENOMEM;
		goto alloc_fail;
	}

	/* init tty driver */
	port_proxy_ptr = port->port_proxy;
	ctlb->driver = tty_drv;
	tty_drv->driver_name = ctlb->ccci_ops->name;
	tty_drv->name = ctlb->ccci_ops->name;
	tty_drv->major = port_proxy_ptr->major;
	tty_drv->minor_start = CCCI_TTY_MINOR_BASE;
	tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	tty_drv->subtype = SERIAL_TYPE_NORMAL;
	tty_drv->flags = TTY_DRIVER_RESET_TERMIOS |
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV |
		TTY_DRIVER_UNNUMBERED_NODE;
	tty_drv->init_termios = tty_std_termios;
	tty_drv->init_termios.c_iflag = 0x00;
	tty_drv->init_termios.c_oflag = 0x00;
	tty_drv->init_termios.c_cflag |= CLOCAL;
	tty_drv->init_termios.c_lflag = 0x00;
	tty_set_operations(tty_drv, &ccci_serial_ops);

	ret = tty_register_driver(tty_drv);
	if (ret < 0) {
		pr_err("Couldn't register tty driver\n");
		ret = -1;
		goto register_tty_fail;
	}

	return ret;

register_tty_fail:
	put_tty_driver(tty_drv);

alloc_fail:
	kfree(ctlb->ccci_ops);
	kfree(ctlb);

return ret;
}

static void tty_ccci_uninit(void)
{
	struct tty_driver *tty_drv;
	struct tty_ctl_block *ctlb;

	ctlb = tty_ctlb;
	if (ctlb) {
		tty_drv = ctlb->driver;
		tty_unregister_driver(tty_drv);
		put_tty_driver(tty_drv);

		kfree(ctlb->ccci_ops);
		kfree(ctlb);
		tty_ctlb = NULL;
	}
}

static int tty_rx_callback(int tty_id, void *data, int len)
{
	int copied = 0;
	struct tty_port *pport;
	struct tty_driver *drv;

	drv = tty_ctlb->driver;
	pport = drv->ports[tty_id];

	if (!pport) {
		pr_err("TTY Port isn't created, the packet is dropped\n");
		return len;
	}

	/* push data to tty port buffer */
	copied = tty_insert_flip_string(pport, data, len);
	if (copied != len)
		pr_err("ccci_tty:%d drop data; skb_len:%d, copied:%d\n",
		       tty_id, len, copied);

	/* trigger port buffer -> line discipline buffer */
	tty_flip_buffer_push(pport);

	return copied;
}

struct tty_dev_ops tty_ops = {
	.tty_driver_status = false,
	.init = &tty_ccci_init,
	.tty_port_create = &ccci_tty_port_create,
	.tty_port_destroy = &ccci_tty_port_destroy,
	.rx_callback = &tty_rx_callback,
	.exit = &tty_ccci_uninit,
};
