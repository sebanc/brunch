/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_PORT_CONFIG_H__
#define __T7XX_PORT_CONFIG_H__

#include "t7xx_port.h"

/* port ops mapping */
extern struct port_ops char_port_ops;
extern struct port_ops wwan_sub_port_ops;
extern struct port_ops poller_port_ops;
extern struct port_ops ctl_port_ops;
extern struct port_ops tty_port_ops;
extern struct tty_dev_ops tty_ops;

int port_get_cfg(struct t7xx_port **ports);

#endif /* __T7XX_PORT_CONFIG_H__ */
