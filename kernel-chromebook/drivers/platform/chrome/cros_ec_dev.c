/*
 * cros_ec_dev - expose the Chrome OS Embedded Controller to user-space
 *
 * Copyright (C) 2014 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/fs.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_dark_resume.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "cros_ec_debugfs.h"
#include "cros_ec_dev.h"

#define DRV_NAME "cros-ec-ctl"

/* Device variables */
#define CROS_MAX_DEV 128
static int ec_major;

static const struct attribute_group *cros_ec_groups[] = {
	&cros_ec_attr_group,
	&cros_ec_lightbar_attr_group,
#if IS_ENABLED(CONFIG_MFD_CROS_EC_PD_UPDATE)
	&cros_ec_pd_attr_group,
#endif
#if IS_ENABLED(CONFIG_CHARGER_CROS_USB_PD)
	&cros_usb_pd_charger_attr_group,
#endif
	NULL,
};

static struct class cros_class = {
	.owner          = THIS_MODULE,
	.name           = "chromeos",
	.dev_groups     = cros_ec_groups,
};

/* Basic communication */
static int ec_get_version(struct cros_ec_dev *ec, char *str, int maxlen)
{
	struct ec_response_get_version *resp;
	static const char * const current_image_name[] = {
		"unknown", "read-only", "read-write", "invalid",
	};
	struct cros_ec_command *msg;
	int ret;

	msg = kmalloc(sizeof(*msg) + sizeof(*resp), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->version = 0;
	msg->command = EC_CMD_GET_VERSION + ec->cmd_offset;
	msg->insize = sizeof(*resp);
	msg->outsize = 0;

	ret = cros_ec_cmd_xfer(ec->ec_dev, msg);
	if (ret < 0)
		goto exit;

	if (msg->result != EC_RES_SUCCESS) {
		snprintf(str, maxlen,
			 "%s\nUnknown EC version: EC returned %d\n",
			 CROS_EC_DEV_VERSION, msg->result);
		ret = -EINVAL;
		goto exit;
	}

	resp = (struct ec_response_get_version *)msg->data;
	if (resp->current_image >= ARRAY_SIZE(current_image_name))
		resp->current_image = 3; /* invalid */

	snprintf(str, maxlen, "%s\n%s\n%s\n%s\n", CROS_EC_DEV_VERSION,
		 resp->version_string_ro, resp->version_string_rw,
		 current_image_name[resp->current_image]);

	ret = 0;
exit:
	kfree(msg);
	return ret;
}

struct cros_ec_priv {
	struct cros_ec_dev *ec;
	struct notifier_block notifier;
	struct list_head events;
	wait_queue_head_t wait_event;
	unsigned long event_mask;
	size_t event_len;
};

struct ec_priv_event {
	struct list_head node;
	size_t size;
	uint8_t event_type;
	u8 data[0];
};

/* Arbitrary bounded size for the event queue */
#define MAX_EVENT_LEN PAGE_SIZE

static int ec_device_mkbp_event(struct notifier_block *nb,
	unsigned long queued_during_suspend, void *_notify)
{
	struct cros_ec_priv *priv = container_of(nb, struct cros_ec_priv,
						 notifier);
	struct cros_ec_device *ec_dev = priv->ec->ec_dev;
	struct ec_priv_event *event;
	unsigned long event_bit = 1 << ec_dev->event_data.event_type;
	int total_size = sizeof(struct ec_priv_event) + ec_dev->event_size;

	if (!(event_bit & priv->event_mask) ||
	    (priv->event_len + total_size) > MAX_EVENT_LEN)
		return NOTIFY_DONE;

	event = kzalloc(total_size, GFP_KERNEL);
	if (!event)
		return NOTIFY_DONE;

	event->size = ec_dev->event_size;
	event->event_type = ec_dev->event_data.event_type;
	memcpy(event->data, &ec_dev->event_data.data, ec_dev->event_size);

	spin_lock(&priv->wait_event.lock);
	list_add_tail(&event->node, &priv->events);
	priv->event_len += total_size;
	wake_up_locked(&priv->wait_event);
	spin_unlock(&priv->wait_event.lock);

	return NOTIFY_OK;
}

/* Device file ops */
static int ec_device_open(struct inode *inode, struct file *filp)
{
	struct cros_ec_dev *ec = container_of(inode->i_cdev,
					      struct cros_ec_dev, cdev);
	int retval;
	struct cros_ec_priv *priv = kzalloc(sizeof(struct cros_ec_priv),
					    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->ec = ec;
	filp->private_data = priv;
	INIT_LIST_HEAD(&priv->events);
	init_waitqueue_head(&priv->wait_event);
	nonseekable_open(inode, filp);

	priv->notifier.notifier_call = ec_device_mkbp_event;
	retval = blocking_notifier_chain_register(&ec->ec_dev->event_notifier,
						  &priv->notifier);
	if (retval) {
		dev_err(ec->dev, "failed to register event notifier\n");
		kfree(priv);
	}

	return retval;
}

static unsigned int ec_device_poll(struct file *filp, poll_table *wait)
{
	struct cros_ec_priv *priv = filp->private_data;

	poll_wait(filp, &priv->wait_event, wait);

	if (list_empty(&priv->events))
		return 0;

	return POLLIN | POLLRDNORM;
}

static int ec_device_release(struct inode *inode, struct file *filp)
{
	struct cros_ec_priv *priv = filp->private_data;
	struct cros_ec_dev *ec = priv->ec;
	struct ec_priv_event *evt, *tmp;

	blocking_notifier_chain_unregister(&ec->ec_dev->event_notifier,
					   &priv->notifier);
	list_for_each_entry_safe(evt, tmp, &priv->events, node) {
		list_del(&evt->node);
		kfree(evt);
	}
	kfree(priv);

	return 0;
}

static struct ec_priv_event *ec_fetch_event(struct cros_ec_priv *priv,
					    bool fetch, bool block)
{
	struct ec_priv_event *event;
	int error;

	spin_lock(&priv->wait_event.lock);
	if (!block && list_empty(&priv->events)) {
		event = ERR_PTR(-EWOULDBLOCK);
		goto out;
	}
	if (!fetch) {
		event = NULL;
		goto out;
	}
	error = wait_event_interruptible_locked(priv->wait_event,
						!list_empty(&priv->events));
	if (error) {
		event = ERR_PTR(error);
		goto out;
	}
	event = list_first_entry(&priv->events, struct ec_priv_event, node);
	list_del(&event->node);
	priv->event_len -= event->size + sizeof(struct ec_priv_event);
out:
	spin_unlock(&priv->wait_event.lock);
	return event;
}


static ssize_t ec_device_read(struct file *filp, char __user *buffer,
			      size_t length, loff_t *offset)
{
	struct cros_ec_priv *priv = filp->private_data;
	struct cros_ec_dev *ec = priv->ec;
	char msg[sizeof(struct ec_response_get_version) +
		 sizeof(CROS_EC_DEV_VERSION)];
	size_t count;
	int ret;


	if (priv->event_mask) { /* queued MKBP event */
		struct ec_priv_event *event;

		event = ec_fetch_event(priv, length != 0,
				       !(filp->f_flags & O_NONBLOCK));
		if (IS_ERR(event))
			return PTR_ERR(event);
		/*
		 * length == 0 is special - no IO is done but we check
		 * for error conditions.
		 */
		if (length == 0)
			return 0;

		/* the event is 1 byte of type plus the payload */
		count = min(length, event->size + 1);
		ret = copy_to_user(buffer, &event->event_type, count);
		kfree(event);
		if (ret) /* the copy failed */
			return -EFAULT;
		*offset = count;
		return count;
	}
	/* legacy behavior if no event mask is defined */
	if (*offset != 0)
		return 0;

	ret = ec_get_version(ec, msg, sizeof(msg));
	if (ret)
		return ret;

	count = min(length, strlen(msg));

	if (copy_to_user(buffer, msg, count))
		return -EFAULT;

	*offset = count;
	return count;
}

/* Ioctls */
static long ec_device_ioctl_xcmd(struct cros_ec_dev *ec, void __user *arg)
{
	long ret;
	struct cros_ec_command u_cmd;
	struct cros_ec_command *s_cmd;

	if (copy_from_user(&u_cmd, arg, sizeof(u_cmd)))
		return -EFAULT;

	s_cmd = kmalloc(sizeof(*s_cmd) + max(u_cmd.outsize, u_cmd.insize),
			GFP_KERNEL);
	if (!s_cmd)
		return -ENOMEM;

	if (copy_from_user(s_cmd, arg, sizeof(*s_cmd) + u_cmd.outsize)) {
		ret = -EFAULT;
		goto exit;
	}

	if (u_cmd.outsize != s_cmd->outsize ||
	    u_cmd.insize != s_cmd->insize) {
		ret = -EINVAL;
		goto exit;
	}

	s_cmd->command += ec->cmd_offset;
	ret = cros_ec_cmd_xfer(ec->ec_dev, s_cmd);
	/* Only copy data to userland if data was received. */
	if (ret < 0)
		goto exit;

	if (copy_to_user(arg, s_cmd, sizeof(*s_cmd) + s_cmd->insize))
		ret = -EFAULT;
exit:
	kfree(s_cmd);
	return ret;
}

static long ec_device_ioctl_readmem(struct cros_ec_dev *ec, void __user *arg)
{
	struct cros_ec_device *ec_dev = ec->ec_dev;
	struct cros_ec_readmem s_mem = { };
	long num;

	/* Not every platform supports direct reads */
	if (!ec_dev->cmd_readmem)
		return -ENOTTY;

	if (copy_from_user(&s_mem, arg, sizeof(s_mem)))
		return -EFAULT;

	num = ec_dev->cmd_readmem(ec_dev, s_mem.offset, s_mem.bytes,
				  s_mem.buffer);
	if (num <= 0)
		return num;

	if (copy_to_user((void __user *)arg, &s_mem, sizeof(s_mem)))
		return -EFAULT;

	return num;
}

static long ec_device_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	struct cros_ec_priv *priv = filp->private_data;
	struct cros_ec_dev *ec = priv->ec;

	if (_IOC_TYPE(cmd) != CROS_EC_DEV_IOC)
		return -ENOTTY;

	switch (cmd) {
	case CROS_EC_DEV_IOCXCMD:
		return ec_device_ioctl_xcmd(ec, (void __user *)arg);
	case CROS_EC_DEV_IOCRDMEM:
		return ec_device_ioctl_readmem(ec, (void __user *)arg);
	case CROS_EC_DEV_IOCEVENTMASK:
		priv->event_mask = arg;
		return 0;
	}

	return -ENOTTY;
}

/* Module initialization */
static const struct file_operations fops = {
	.open = ec_device_open,
	.poll = ec_device_poll,
	.release = ec_device_release,
	.read = ec_device_read,
	.unlocked_ioctl = ec_device_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ec_device_ioctl,
#endif
};

static void __remove(struct device *dev)
{
	struct cros_ec_dev *ec = container_of(dev, struct cros_ec_dev,
					      class_dev);
	kfree(ec);
}

static const struct mfd_cell cros_usb_pd_charger_devs[] = {
	{
		.name = "cros-usb-pd-charger",
		.id   = -1,
	},
};

static void cros_ec_usb_pd_charger_register(struct cros_ec_dev *ec)
{
	int ret;

	ret = mfd_add_devices(ec->dev, 0, cros_usb_pd_charger_devs,
			      ARRAY_SIZE(cros_usb_pd_charger_devs),
			      NULL, 0, NULL);
	if (ret)
		dev_err(ec->dev, "failed to add usb-pd-charger device\n");
}


static const struct mfd_cell cros_ec_rtc_devs[] = {
	{
		.name = "cros-ec-rtc",
		.id   = -1,
	},
};

static void cros_ec_rtc_register(struct cros_ec_dev *ec)
{
	int ret;

	ret = mfd_add_devices(ec->dev, 0, cros_ec_rtc_devs,
			      ARRAY_SIZE(cros_ec_rtc_devs),
			      NULL, 0, NULL);
	if (ret)
		dev_err(ec->dev, "failed to add cros-ec-rtc device: %d\n", ret);
}

static const struct mfd_cell cros_ec_cec_devs[] = {
	{
		.name = "cros-ec-cec",
		.id   = -1,
	},
};

static void cros_ec_cec_register(struct cros_ec_dev *ec)
{
	int ret;

	ret = mfd_add_devices(ec->dev, 0, cros_ec_cec_devs,
			      ARRAY_SIZE(cros_ec_cec_devs),
			      NULL, 0, NULL);
	if (ret)
		dev_err(ec->dev, "failed to add cros-ec-cec device: %d\n", ret);
}

static const struct mfd_cell ec_throttler_cells[] = {
	{ .name = "cros-ec-throttler" }
};

static void cros_ec_throttler_register(struct cros_ec_dev *ec)
{
	int ret;

	ret = mfd_add_devices(ec->dev, 0, ec_throttler_cells,
			      ARRAY_SIZE(ec_throttler_cells),
			      NULL, 0, NULL);
	if (ret)
		dev_err(ec->dev,
			"failed to add cros-ec-throttler device: %d\n", ret);
}

static const struct mfd_cell cros_ec_sensorhub_cells[] = {
	{ .name = "cros-ec-sensorhub" }
};

static int ec_device_probe(struct platform_device *pdev)
{
	int retval = -ENOMEM;
	struct device *dev = &pdev->dev;
	struct cros_ec_dev_platform *ec_platform = dev_get_platdata(dev);
	dev_t devno = MKDEV(ec_major, pdev->id);
	struct cros_ec_dev *ec = kzalloc(sizeof(*ec), GFP_KERNEL);

	if (!ec)
		return retval;

	dev_set_drvdata(dev, ec);
	ec->ec_dev = dev_get_drvdata(dev->parent);
	ec->dev = dev;
	ec->cmd_offset = ec_platform->cmd_offset;
	ec->features[0] = -1U; /* Not cached yet */
	ec->features[1] = -1U; /* Not cached yet */
	device_initialize(&ec->class_dev);
	cdev_init(&ec->cdev, &fops);

	/*
	 * ACPI attaches the firmware node of the parent to the platform
	 * devices. If the parent firmware node has a valid wakeup flag, ACPI
	 * marks this platform device also as wake capable.  But this platform
	 * device by itself cannot wake the system up. Mark the wake capability
	 * to false.
	 */
	device_set_wakeup_capable(dev, false);

	/* check whether this is actually a Fingerprint MCU rather than an EC */
	if (cros_ec_check_features(ec, EC_FEATURE_FINGERPRINT)) {
		dev_info(dev, "Fingerprint MCU detected.\n");
		/*
		 * Help userspace differentiating ECs from FP MCU,
		 * regardless of the probing order.
		 */
		ec_platform->ec_name = CROS_EC_DEV_FP_NAME;
	}

	/* check whether this is actually a Touchpad MCU rather than an EC */
	if (cros_ec_check_features(ec, EC_FEATURE_TOUCHPAD)) {
		dev_info(dev, "Touchpad MCU detected.\n");
		/*
		 * Help userspace differentiating ECs from TP MCU,
		 * regardless of the probing order.
		 */
		ec_platform->ec_name = CROS_EC_DEV_TP_NAME;
	}

	/*
	 * Add the character device
	 * Link cdev to the class device to be sure device is not used
	 * before unbinding it.
	 */
	ec->cdev.kobj.parent = &ec->class_dev.kobj;
	retval = cdev_add(&ec->cdev, devno, 1);
	if (retval) {
		dev_err(dev, ": failed to add character device\n");
		goto cdev_add_failed;
	}

	/*
	 * Add the class device
	 * Link to the character device for creating the /dev entry
	 * in devtmpfs.
	 */
	ec->class_dev.devt = ec->cdev.dev;
	ec->class_dev.class = &cros_class;
	ec->class_dev.parent = dev;
	ec->class_dev.release = __remove;

	retval = dev_set_name(&ec->class_dev, "%s", ec_platform->ec_name);
	if (retval) {
		dev_err(dev, "dev_set_name failed => %d\n", retval);
		goto set_named_failed;
	}

	/* check whether this EC instance has the PD charge manager */
	if (cros_ec_check_features(ec, EC_FEATURE_USB_PD))
		cros_ec_usb_pd_charger_register(ec);

	/* check whether this EC is a sensor hub. */
	if (cros_ec_get_sensor_count(ec) > 0) {
		retval = mfd_add_devices(ec->dev, 0, cros_ec_sensorhub_cells,
				ARRAY_SIZE(cros_ec_sensorhub_cells),
				NULL, 0, NULL);
		if (retval)
			dev_err(ec->dev, "failed to add %s subdevice: %d\n",
				cros_ec_sensorhub_cells->name, retval);
	}

	/* check whether this EC instance has RTC host command support */
	if (cros_ec_check_features(ec, EC_FEATURE_RTC))
		cros_ec_rtc_register(ec);

	/* check whether this EC instance has CEC command support */
	if (cros_ec_check_features(ec, EC_FEATURE_CEC))
		cros_ec_cec_register(ec);

	if (IS_ENABLED(CONFIG_CROS_EC_THROTTLER))
		cros_ec_throttler_register(ec);

	/* Take control of the lightbar from the EC. */
	lb_manual_suspend_ctrl(ec, 1);

	/* We can now add the sysfs class, we know which parameter to show */
	retval = device_add(&ec->class_dev);
	if (retval)
		dev_err(dev, "device_register failed => %d\n", retval);

	if (cros_ec_debugfs_init(ec))
		dev_warn(dev, "failed to create debugfs directory\n");

	dev_dark_resume_add_consumer(dev);

	return 0;

set_named_failed:
	dev_set_drvdata(dev, NULL);
	cdev_del(&ec->cdev);
cdev_add_failed:
	kfree(ec);
	return retval;
}

static int ec_device_remove(struct platform_device *pdev)
{
	struct cros_ec_dev *ec = dev_get_drvdata(&pdev->dev);
	dev_dark_resume_remove_consumer(&pdev->dev);

	/* Let the EC take over the lightbar again. */
	lb_manual_suspend_ctrl(ec, 0);

	mfd_remove_devices(ec->dev);
	cros_ec_debugfs_remove(ec);

	cdev_del(&ec->cdev);
	device_unregister(&ec->class_dev);
	return 0;
}

static void ec_device_shutdown(struct platform_device *pdev)
{
	struct cros_ec_dev *ec = dev_get_drvdata(&pdev->dev);

	/* Be sure to clear up debugfs delayed works */
	cros_ec_debugfs_remove(ec);
}

static const struct platform_device_id cros_ec_id[] = {
	{ "cros-ec-ctl", 0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, cros_ec_id);

static int __maybe_unused ec_device_suspend(struct device *dev)
{
	struct cros_ec_dev *ec = dev_get_drvdata(dev);

	cros_ec_debugfs_suspend(ec);

	if (!dev_dark_resume_active(dev))
		lb_suspend(ec);

	return 0;
}

static int __maybe_unused ec_device_resume(struct device *dev)
{
	struct cros_ec_dev *ec = dev_get_drvdata(dev);
	char msg[sizeof(struct ec_response_get_version) +
		 sizeof(CROS_EC_DEV_VERSION)];
	int ret;

	/* Be sure the communication with the EC is reestablished */
	ret = ec_get_version(ec, msg, sizeof(msg));
	if (ret < 0) {
		dev_err(ec->ec_dev->dev, "No EC response at resume: %d\n", ret);
		return 0;
	}
	if (!dev_dark_resume_active(dev))
		lb_resume(ec);

	cros_ec_debugfs_resume(ec);

	return 0;
}

static const struct dev_pm_ops cros_ec_dev_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = ec_device_suspend,
	.resume = ec_device_resume,
#endif
};

static struct platform_driver cros_ec_dev_driver = {
	.driver = {
		.name = DRV_NAME,
		.pm = &cros_ec_dev_pm_ops,
	},
	.probe = ec_device_probe,
	.remove = ec_device_remove,
	.shutdown = ec_device_shutdown,
};

static int __init cros_ec_dev_init(void)
{
	int ret;
	dev_t dev = 0;

	ret  = class_register(&cros_class);
	if (ret) {
		pr_err(CROS_EC_DEV_NAME ": failed to register device class\n");
		return ret;
	}

	/* Get a range of minor numbers (starting with 0) to work with */
	ret = alloc_chrdev_region(&dev, 0, CROS_MAX_DEV, CROS_EC_DEV_NAME);
	if (ret < 0) {
		pr_err(CROS_EC_DEV_NAME ": alloc_chrdev_region() failed\n");
		goto failed_chrdevreg;
	}
	ec_major = MAJOR(dev);

	/* Register the driver */
	ret = platform_driver_register(&cros_ec_dev_driver);
	if (ret < 0) {
		pr_warn(CROS_EC_DEV_NAME ": can't register driver: %d\n", ret);
		goto failed_devreg;
	}
	return 0;

failed_devreg:
	unregister_chrdev_region(MKDEV(ec_major, 0), CROS_MAX_DEV);
failed_chrdevreg:
	class_unregister(&cros_class);
	return ret;
}

static void __exit cros_ec_dev_exit(void)
{
	platform_driver_unregister(&cros_ec_dev_driver);
	unregister_chrdev(ec_major, CROS_EC_DEV_NAME);
	class_unregister(&cros_class);
}

module_init(cros_ec_dev_init);
module_exit(cros_ec_dev_exit);

MODULE_ALIAS("platform:" DRV_NAME);
MODULE_AUTHOR("Bill Richardson <wfrichar@chromium.org>");
MODULE_DESCRIPTION("Userspace interface to the Chrome OS Embedded Controller");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
