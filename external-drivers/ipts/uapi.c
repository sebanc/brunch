// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Dorian Stoll
 *
 * Linux driver for Intel Precise Touch & Stylus
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/types.h>

#include "context.h"
#include "control.h"
#include "protocol.h"
#include "uapi.h"

struct ipts_uapi uapi;

static ssize_t ipts_uapi_read(struct file *file, char __user *buf,
		size_t count, loff_t *offset)
{
	int buffer;
	int maxbytes;
	struct ipts_context *ipts = uapi.ipts;

	buffer = MINOR(file->f_path.dentry->d_inode->i_rdev);

	if (!ipts || ipts->status != IPTS_HOST_STATUS_STARTED)
		return -ENODEV;

	maxbytes = ipts->device_info.data_size - *offset;
	if (maxbytes <= 0 || count > maxbytes)
		return -EINVAL;

	if (copy_to_user(buf, ipts->data[buffer].address + *offset, count))
		return -EFAULT;

	return count;
}

static long ipts_uapi_ioctl_get_device_ready(struct ipts_context *ipts,
		unsigned long arg)
{
	void __user *buffer = (void __user *)arg;
	u8 ready = ipts->status == IPTS_HOST_STATUS_STARTED;

	if (copy_to_user(buffer, &ready, sizeof(u8)))
		return -EFAULT;

	return 0;
}

static long ipts_uapi_ioctl_get_device_info(struct ipts_context *ipts,
		unsigned long arg)
{
	struct ipts_device_info info;
	void __user *buffer = (void __user *)arg;

	if (ipts->status != IPTS_HOST_STATUS_STARTED)
		return -ENODEV;

	info.vendor = ipts->device_info.vendor_id;
	info.product = ipts->device_info.device_id;
	info.version = ipts->device_info.fw_rev;
	info.buffer_size = ipts->device_info.data_size;
	info.max_contacts = ipts->device_info.max_contacts;

	if (copy_to_user(buffer, &info, sizeof(struct ipts_device_info)))
		return -EFAULT;

	return 0;
}

static long ipts_uapi_ioctl_get_doorbell(struct ipts_context *ipts,
		unsigned long arg)
{
	void __user *buffer = (void __user *)arg;

	if (ipts->status != IPTS_HOST_STATUS_STARTED)
		return -ENODEV;

	if (copy_to_user(buffer, ipts->doorbell.address, sizeof(u32)))
		return -EFAULT;

	return 0;
}

static long ipts_uapi_ioctl_send_feedback(struct ipts_context *ipts,
		struct file *file)
{
	int ret;
	struct ipts_feedback_cmd cmd;

	if (ipts->status != IPTS_HOST_STATUS_STARTED)
		return -ENODEV;

	memset(&cmd, 0, sizeof(struct ipts_feedback_cmd));
	cmd.buffer = MINOR(file->f_path.dentry->d_inode->i_rdev);

	ret = ipts_control_send(ipts, IPTS_CMD_FEEDBACK,
				&cmd, sizeof(struct ipts_feedback_cmd));

	if (ret)
		return -EFAULT;

	return 0;
}

static long ipts_uapi_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct ipts_context *ipts = uapi.ipts;

	if (!ipts)
		return -ENODEV;

	switch (cmd) {
	case IPTS_IOCTL_GET_DEVICE_READY:
		return ipts_uapi_ioctl_get_device_ready(ipts, arg);
	case IPTS_IOCTL_GET_DEVICE_INFO:
		return ipts_uapi_ioctl_get_device_info(ipts, arg);
	case IPTS_IOCTL_GET_DOORBELL:
		return ipts_uapi_ioctl_get_doorbell(ipts, arg);
	case IPTS_IOCTL_SEND_FEEDBACK:
		return ipts_uapi_ioctl_send_feedback(ipts, file);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations ipts_uapi_fops = {
	.owner = THIS_MODULE,
	.read = ipts_uapi_read,
	.unlocked_ioctl = ipts_uapi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ipts_uapi_ioctl,
#endif
};

void ipts_uapi_link(struct ipts_context *ipts)
{
	uapi.ipts = ipts;
}

void ipts_uapi_unlink(void)
{
	uapi.ipts = NULL;
}

int ipts_uapi_init(void)
{
	int i, major;

	alloc_chrdev_region(&uapi.dev, 0, IPTS_BUFFERS, "ipts");
	uapi.class = class_create(THIS_MODULE, "ipts");

	major = MAJOR(uapi.dev);

	cdev_init(&uapi.cdev, &ipts_uapi_fops);
	uapi.cdev.owner = THIS_MODULE;
	cdev_add(&uapi.cdev, MKDEV(major, 0), IPTS_BUFFERS);

	for (i = 0; i < IPTS_BUFFERS; i++) {
		device_create(uapi.class, NULL,
				MKDEV(major, i), NULL, "ipts/%d", i);
	}

	return 0;
}

void ipts_uapi_free(void)
{
	int i;
	int major;

	major = MAJOR(uapi.dev);

	for (i = 0; i < IPTS_BUFFERS; i++)
		device_destroy(uapi.class, MKDEV(major, i));

	cdev_del(&uapi.cdev);

	unregister_chrdev_region(MKDEV(major, 0), MINORMASK);
	class_destroy(uapi.class);
}

