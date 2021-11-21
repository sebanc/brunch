/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_MAUSB_ADDRESS_H__
#define __MAUSB_MAUSB_ADDRESS_H__

#include <linux/inet.h>
#include <linux/types.h>

struct mausb_device_address {
	u8 link_type;
	struct {
		char address[INET6_ADDRSTRLEN];
		u8 number_of_ports;
		struct {
			u16 management;
			u16 control;
			u16 bulk;
			u16 interrupt;
			u16 isochronous;
		} port;
	} ip;
};

#endif /* __MAUSB_MAUSB_ADDRESS_H__ */
