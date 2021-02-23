/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#ifndef _IPTS_UAPI_H_
#define _IPTS_UAPI_H_

#include <linux/types.h>

#include "context.h"

struct ipts_uapi {
	dev_t dev;
	struct class *class;
	struct cdev cdev;

	struct ipts_context *ipts;
};

struct ipts_device_info {
	__u16 vendor;
	__u16 product;
	__u32 version;
	__u32 buffer_size;
	__u8 max_contacts;

	/* For future expansion */
	__u8 reserved[19];
};

#define IPTS_IOCTL_GET_DEVICE_READY _IOR(0x86, 0x01, __u8)
#define IPTS_IOCTL_GET_DEVICE_INFO  _IOR(0x86, 0x02, struct ipts_device_info)
#define IPTS_IOCTL_GET_DOORBELL	    _IOR(0x86, 0x03, __u32)
#define IPTS_IOCTL_SEND_FEEDBACK    _IO(0x86, 0x04)
#define IPTS_IOCTL_SEND_RESET	    _IO(0x86, 0x05)

void ipts_uapi_link(struct ipts_context *ipts);
void ipts_uapi_unlink(void);

int ipts_uapi_init(void);
void ipts_uapi_free(void);

#endif /* _IPTS_UAPI_H_ */
