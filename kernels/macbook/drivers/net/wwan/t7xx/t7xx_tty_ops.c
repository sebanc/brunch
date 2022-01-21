// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include "t7xx_port_proxy.h"
#include "t7xx_tty_ops.h"

#define GET_TTY_IDX(p)		((p)->minor - TTY_PORT_MINOR_BASE)

static struct tty_ctl_block *tty_ctlb;
static const struct tty_port_operations null_ops = {};

static int ccci_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct tty_port *pport;
	int ret = 0;

	pport = tty->driver->ports[tty->index];
	if (pport)
		ret = tty_port_open(pport, tty, filp);

	return ret;
}

static void ccci_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct tty_port *pport;

	pport = tty->driver->ports[tty->index];
	if (pport)
		tty_port_close(pport, tty, filp);
}

static int ccci_tty_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	if (!(tty_ctlb && tty_ctlb->driver == tty->driver))
		return -EFAULT;

	return tty_ctlb->ccci_ops->send_pkt(tty->index, buf, count);
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

static int ccci_tty_port_create(struct t7xx_port *port, char *port_name)
{
	struct tty_driver *tty_drv;
	struct tty_port *pport;
	int minor = GET_TTY_IDX(port);

	tty_drv = tty_ctlb->driver;
	tty_drv->name = port_name;

	pport = devm_kzalloc(port->dev, sizeof(*pport), GFP_KERNEL);
	if (!pport)
		return -ENOMEM;

	tty_port_init(pport);
	pport->ops = &null_ops;
	tty_port_link_device(pport, tty_drv, minor);
	tty_register_device(tty_drv, minor, NULL);
	return 0;
}

static int ccci_tty_port_destroy(struct t7xx_port *port)
{
	struct tty_driver *tty_drv;
	struct tty_port *pport;
	struct tty_struct *tty;
	int minor = port->minor;

	tty_drv = tty_ctlb->driver;

	pport = tty_drv->ports[minor];
	if (!pport) {
		dev_err(port->dev, "Invalid tty minor:%d\n", minor);
		return -EINVAL;
	}

	tty = tty_port_tty_get(pport);
	if (tty) {
		tty_vhangup(tty);
		tty_kref_put(tty);
	}

	tty_unregister_device(tty_drv, minor);
	tty_port_destroy(pport);
	tty_drv->ports[minor] = NULL;
	return 0;
}

static int tty_ccci_init(struct tty_ccci_ops *ccci_info, struct t7xx_port *port)
{
	struct port_proxy *port_proxy_ptr;
	struct tty_driver *tty_drv;
	struct tty_ctl_block *ctlb;
	int ret, port_nr;

	ctlb = devm_kzalloc(port->dev, sizeof(*ctlb), GFP_KERNEL);
	if (!ctlb)
		return -ENOMEM;

	ctlb->ccci_ops = devm_kzalloc(port->dev, sizeof(*ctlb->ccci_ops), GFP_KERNEL);
	if (!ctlb->ccci_ops)
		return -ENOMEM;

	tty_ctlb = ctlb;
	memcpy(ctlb->ccci_ops, ccci_info, sizeof(struct tty_ccci_ops));
	port_nr = ctlb->ccci_ops->tty_num;

	tty_drv = tty_alloc_driver(port_nr, 0);
	if (IS_ERR(tty_drv))
		return -ENOMEM;

	/* init tty driver */
	port_proxy_ptr = port->port_proxy;
	ctlb->driver = tty_drv;
	tty_drv->driver_name = ctlb->ccci_ops->name;
	tty_drv->name = ctlb->ccci_ops->name;
	tty_drv->major = port_proxy_ptr->major;
	tty_drv->minor_start = TTY_PORT_MINOR_BASE;
	tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	tty_drv->subtype = SERIAL_TYPE_NORMAL;
	tty_drv->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW |
			 TTY_DRIVER_DYNAMIC_DEV | TTY_DRIVER_UNNUMBERED_NODE;
	tty_drv->init_termios = tty_std_termios;
	tty_drv->init_termios.c_iflag = 0;
	tty_drv->init_termios.c_oflag = 0;
	tty_drv->init_termios.c_cflag |= CLOCAL;
	tty_drv->init_termios.c_lflag = 0;
	tty_set_operations(tty_drv, &ccci_serial_ops);

	ret = tty_register_driver(tty_drv);
	if (ret < 0) {
		dev_err(port->dev, "Could not register tty driver\n");
		tty_driver_kref_put(tty_drv);
	}

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
		tty_driver_kref_put(tty_drv);
		tty_ctlb = NULL;
	}
}

static int tty_rx_callback(struct t7xx_port *port, void *data, int len)
{
	struct tty_port *pport;
	struct tty_driver *drv;
	int tty_id = GET_TTY_IDX(port);
	int copied = 0;

	drv = tty_ctlb->driver;
	pport = drv->ports[tty_id];

	if (!pport) {
		dev_err(port->dev, "tty port isn't created, the packet is dropped\n");
		return len;
	}

	/* push data to tty port buffer */
	copied = tty_insert_flip_string(pport, data, len);

	/* trigger port buffer -> line discipline buffer */
	tty_flip_buffer_push(pport);
	return copied;
}

struct tty_dev_ops tty_ops = {
	.init = &tty_ccci_init,
	.tty_port_create = &ccci_tty_port_create,
	.tty_port_destroy = &ccci_tty_port_destroy,
	.rx_callback = &tty_rx_callback,
	.exit = &tty_ccci_uninit,
};
