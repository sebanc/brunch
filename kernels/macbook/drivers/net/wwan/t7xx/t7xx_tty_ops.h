/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 *
 * Authors: Haijun Lio <haijun.liu@mediatek.com>
 * Contributors: Amir Hanania <amir.hanania@intel.com>
 *               Moises Veleta <moises.veleta@intel.com>
 *               Sreehari Kancharla <sreehari.kancharla@intel.com>
 */

#ifndef __T7XX_TTY_OPS_H__
#define __T7XX_TTY_OPS_H__

#include <linux/types.h>

#include "t7xx_port_proxy.h"

#define TTY_PORT_MINOR_BASE	250

struct tty_ccci_ops {
	int			tty_num;
	unsigned char		name[16];
	unsigned int		md_ability;
	int			(*send_pkt)(int tty_idx, const void *data, int len);
};

struct tty_ctl_block {
	struct tty_driver	*driver;
	struct tty_ccci_ops	*ccci_ops;
	unsigned int		md_sta;
};

struct tty_dev_ops {
	/* tty port information */
	bool			tty_driver_status;
	atomic_t		port_installed_num;
	int			(*init)(struct tty_ccci_ops *ccci_info, struct t7xx_port *port);
	int			(*tty_port_create)(struct t7xx_port *port, char *port_name);
	int			(*tty_port_destroy)(struct t7xx_port *port);
	int			(*rx_callback)(struct t7xx_port *port, void *data, int len);
	void			(*exit)(void);
};
#endif /* __T7XX_TTY_OPS_H__ */
