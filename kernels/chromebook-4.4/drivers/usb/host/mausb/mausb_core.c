// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#include <linux/module.h>

#include "hcd.h"
#include "mausb_address.h"
#include "utils.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DisplayLink (UK) Ltd.");

static struct mausb_device_address	device_address;
static int				mausb_device_disconnect_param;
static u16				madev_addr;
static u8				mausb_client_connect_param;
static u8				mausb_client_disconnect_param;

static int mausb_client_connect(const char *value,
				const struct kernel_param *kp)
{
	unsigned long flags;

	spin_lock_irqsave(&mss.lock, flags);
	if (mss.client_connected) {
		dev_err(mausb_host_dev.this_device, "MA-USB client is already connected");
		spin_unlock_irqrestore(&mss.lock, flags);
		return -EEXIST;
	}
	/* Save heartbeat client information */
	mss.client_connected = true;
	mss.missed_heartbeats = 0;
	reinit_completion(&mss.client_stopped);
	spin_unlock_irqrestore(&mss.lock, flags);
	/* Start hearbeat timer */
	mod_timer(&mss.heartbeat_timer,
		  jiffies + msecs_to_jiffies(MAUSB_HEARTBEAT_TIMEOUT_MS));

	return 0;
}

static int mausb_client_disconnect(const char *value,
				   const struct kernel_param *kp)
{
	unsigned long flags;
	struct mausb_device *dev = NULL;

	spin_lock_irqsave(&mss.lock, flags);
	if (!mss.client_connected) {
		dev_err(mausb_host_dev.this_device, "MA-USB client is not connected");
		spin_unlock_irqrestore(&mss.lock, flags);
		return -ENODEV;
	}
	spin_unlock_irqrestore(&mss.lock, flags);

	/* Stop heartbeat timer */
	del_timer_sync(&mss.heartbeat_timer);

	/* Clear heartbeat client information */
	spin_lock_irqsave(&mss.lock, flags);
	mss.client_connected = false;
	mss.missed_heartbeats = 0;
	list_for_each_entry(dev, &mss.madev_list, list_entry) {
		dev_vdbg(mausb_host_dev.this_device, "Enqueue heartbeat_work madev_addr=%x",
			 dev->madev_addr);
		queue_work(dev->workq, &dev->heartbeat_work);
	}
	complete(&mss.client_stopped);
	spin_unlock_irqrestore(&mss.lock, flags);

	return 0;
}

static int mausb_device_connect(const char *value,
				const struct kernel_param *kp)
{
	int status = 0;

	if (!value)
		return -EINVAL;

	if (strlen(value) <= INET6_ADDRSTRLEN) {
		strcpy(device_address.ip.address, value);
		dev_info(mausb_host_dev.this_device, "Processing '%s' address",
			 device_address.ip.address);
	} else {
		dev_err(mausb_host_dev.this_device, "Invalid IP format");
		return 0;
	}
	status = mausb_initiate_dev_connection(device_address, madev_addr);
	memset(&device_address, 0, sizeof(device_address));

	return status;
}

static int mausb_device_disconnect(const char *value,
				   const struct kernel_param *kp)
{
	u8 dev_address = 0;
	int status = 0;
	unsigned long flags;
	struct mausb_device *dev = NULL;

	if (!value)
		return -EINVAL;

	status = kstrtou8(value, 0, &dev_address);
	if (status < 0)
		return -EINVAL;

	spin_lock_irqsave(&mss.lock, flags);

	dev = mausb_get_dev_from_addr_unsafe(dev_address);
	if (dev)
		queue_work(dev->workq, &dev->hcd_disconnect_work);

	spin_unlock_irqrestore(&mss.lock, flags);

	return 0;
}

static const struct kernel_param_ops mausb_device_connect_ops = {
	.set = mausb_device_connect
};

static const struct kernel_param_ops mausb_device_disconnect_ops = {
	.set = mausb_device_disconnect
};

static const struct kernel_param_ops mausb_client_connect_ops = {
	.set = mausb_client_connect
};

static const struct kernel_param_ops mausb_client_disconnect_ops = {
	.set = mausb_client_disconnect
};

module_param_named(mgmt, device_address.ip.port.management, ushort, 0664);
MODULE_PARM_DESC(mgmt, "MA-USB management port");
module_param_named(ctrl, device_address.ip.port.control, ushort, 0664);
MODULE_PARM_DESC(ctrl, "MA-USB control port");
module_param_named(bulk, device_address.ip.port.bulk, ushort, 0664);
MODULE_PARM_DESC(bulk, "MA-USB bulk port");
module_param_named(isoch, device_address.ip.port.isochronous, ushort, 0664);
MODULE_PARM_DESC(isoch, "MA-USB isochronous port");
module_param_named(madev_addr, madev_addr, ushort, 0664);
MODULE_PARM_DESC(madev_addr, "MA-USB device address");

module_param_cb(client_connect, &mausb_client_connect_ops,
		&mausb_client_connect_param, 0664);
module_param_cb(client_disconnect, &mausb_client_disconnect_ops,
		&mausb_client_disconnect_param, 0664);
module_param_cb(ip, &mausb_device_connect_ops,
		device_address.ip.address, 0664);
module_param_cb(disconnect, &mausb_device_disconnect_ops,
		&mausb_device_disconnect_param, 0664);

static int mausb_host_init(void)
{
	int status = mausb_host_dev_register();

	if (status < 0)
		return status;

	status = mausb_host_driver_init();
	if (status < 0)
		goto cleanup_dev;

	status = mausb_register_power_state_listener();
	if (status < 0)
		goto cleanup;

	mausb_initialize_mss();

	return 0;

cleanup:
	mausb_host_driver_deinit();
cleanup_dev:
	mausb_host_dev_deregister();
	return status;
}

static void mausb_host_exit(void)
{
	mausb_unregister_power_state_listener();
	mausb_deinitialize_mss();
	mausb_host_driver_deinit();
	mausb_host_dev_deregister();
}

module_init(mausb_host_init);
module_exit(mausb_host_exit);
