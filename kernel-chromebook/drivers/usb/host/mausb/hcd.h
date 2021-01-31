/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#ifndef __MAUSB_HCD_H__
#define __MAUSB_HCD_H__

#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "hpal.h"

#define DRIVER_NAME "mausb_host"
#define DEVICE_NAME "mausb_host_hcd"

#define NUMBER_OF_PORTS		15
/* Response Timeout in ms (MA-USB v1.0a, Table 43) */
#define RESPONSE_TIMEOUT_MS	5000

/* MA-USB v1.0a, Table 12 */
enum mausb_device_type {
	USBDEVICE = 0,
	USB20HUB  = 1,
	USB30HUB  = 2,
};

/* MA-USB v1.0a, Table 15 */
enum mausb_device_speed {
	LOW_SPEED	 = 0,
	FULL_SPEED	 = 1,
	HIGH_SPEED	 = 2,
	SUPER_SPEED	 = 3,
	SUPER_SPEED_PLUS = 4,
};

struct mausb_hcd {
	spinlock_t	lock;	/* Protect HCD during URB processing */
	u8		connected_ports;

	struct rb_root	mausb_urbs;
	struct hub_ctx	*hcd_ss_ctx;
	struct hub_ctx	*hcd_hs_ctx;
	struct notifier_block power_state_listener;
};

struct mausb_dev {
	u32		port_status;
	struct rb_root	usb_devices;
	u8		dev_speed;
	void		*ma_dev;
};

struct hub_ctx {
	struct usb_hcd	 *hcd;
	struct mausb_dev ma_devs[NUMBER_OF_PORTS];
};

int mausb_host_driver_init(void);
void mausb_host_driver_deinit(void);

void mausb_port_has_changed(const enum mausb_device_type device_type,
			    const enum mausb_device_speed device_speed,
			    void *ma_dev);
void mausb_hcd_disconnect(const u8 port_number,
			  const enum mausb_device_type device_type,
			  const enum mausb_device_speed device_speed);

#define PORT_C_MASK \
		((USB_PORT_STAT_C_CONNECTION \
		| USB_PORT_STAT_C_ENABLE \
		| USB_PORT_STAT_C_SUSPEND \
		| USB_PORT_STAT_C_OVERCURRENT \
		| USB_PORT_STAT_C_RESET) << 16)

/* USB 2.0 specification chapter 11.24.2.7.1 table 11-21 page 427 */
#define MAUSB_PORT_20_STATUS_CONNECT         0x0001
#define MAUSB_PORT_20_STATUS_ENABLE          0x0002
#define MAUSB_PORT_20_STATUS_SUSPEND         0x0004
#define MAUSB_PORT_20_STATUS_OVER_CURRENT    0x0008
#define MAUSB_PORT_20_STATUS_RESET           0x0010
#define MAUSB_PORT_20_STATUS_POWER           0x0100
#define MAUSB_PORT_20_STATUS_LOW_SPEED       0x0200
#define MAUSB_PORT_20_STATUS_HIGH_SPEED      0x0400

/* USB 3.2 specification chapter 10.16.2.6.1 table 10-13 page 440 */
#define MAUSB_PORT_30_STATUS_CONNECT              0x0001
#define MAUSB_PORT_30_STATUS_ENABLE               0x0002
#define MAUSB_PORT_30_STATUS_OVER_CURRENT         0x0008
#define MAUSB_PORT_30_STATUS_RESET                0x0010
#define MAUSB_PORT_30_LINK_STATE_U0               0x0000
#define MAUSB_PORT_30_LINK_STATE_U1               0x0020
#define MAUSB_PORT_30_LINK_STATE_U2               0x0040
#define MAUSB_PORT_30_LINK_STATE_U3               0x0060
#define MAUSB_PORT_30_LINK_STATE_DISABLED         0x0080
#define MAUSB_PORT_30_LINK_STATE_RX_DETECT        0x00A0
#define MAUSB_PORT_30_LINK_STATE_INACTIVE         0x00C0
#define MAUSB_PORT_30_LINK_STATE_POLLING          0x00E0
#define MAUSB_PORT_30_LINK_STATE_RECOVERY         0x0100
#define MAUSB_PORT_30_LINK_STATE_HOT_RESET        0x0120
#define MAUSB_PORT_30_LINK_STATE_COMPLIANCE_MODE  0x0140
#define MAUSB_PORT_30_LINK_STATE_LOOPBACK         0x0160
#define MAUSB_PORT_30_STATUS_POWER                0x0200
#define MAUSB_PORT_30_STATUS_SUPER_SPEED          0x0400
#define MAUSB_PORT_30_CLEAR_LINK_STATE            0xFE1F

/* USB 3.2 specification chapter 10.16.2.6.2 table 10-14 page 443 */
#define MAUSB_CHANGE_PORT_30_STATUS_CONNECT              0x010000
#define MAUSB_CHANGE_PORT_30_STATUS_OVER_CURRENT         0x080000
#define MAUSB_CHANGE_PORT_30_STATUS_RESET                0x100000
#define MAUSB_CHANGE_PORT_30_BH_STATUS_RESET             0x200000
#define MAUSB_CHANGE_PORT_30_LINK_STATE                  0x400000
#define MAUSB_CHANGE_PORT_30_CONFIG_ERROR                0x800000

/* USB 3.2 specification chapter 10.16.2.4 table 10-10 page 438 */
#define MAUSB_HUB_30_POWER_GOOD              0x00
#define MAUSB_HUB_30_LOCAL_POWER_SOURCE_LOST 0x01
#define MAUSB_HUB_30_OVER_CURRENT            0x02

/* USB 3.2 specification chapter 10.16.2.4 table 10-11 page 438 */
#define MAUSB_CHANGE_HUB_30_LOCAL_POWER_SOURCE_LOST  0x10000
#define MAUSB_CHANGE_HUB_30_OVER_CURRENT             0x20000

#define DEV_HANDLE_NOT_ASSIGNED	-1

struct mausb_usb_device_ctx {
	s32		dev_handle;
	bool		addressed;
	void		*dev_addr;
	struct rb_node	rb_node;
};

struct mausb_endpoint_ctx {
	u16	ep_handle;
	u16	dev_handle;
	void	*ma_dev;
	struct mausb_usb_device_ctx *usb_device_ctx;
};

struct mausb_urb_ctx {
	struct urb		*urb;
	struct mausb_data_iter	iterator;
	struct rb_node		rb_node;
	struct work_struct	work;
};

int mausb_hcd_create_and_add(struct device *dev);
void mausb_hcd_urb_complete(struct urb *urb, u32 actual_length, int status);
void mausb_clear_hcd_madev(u8 port_number);

#endif /* __MAUSB_HCD_H__ */
