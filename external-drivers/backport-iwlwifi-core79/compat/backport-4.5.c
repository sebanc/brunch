/*
 * Copyright(c) 2015 Hauke Mehrtens <hauke@hauke-m.de>
 *
 * Backport functionality introduced in Linux 4.5.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/io.h>

int led_set_brightness_sync(struct led_classdev *led_cdev,
			    enum led_brightness value)
{
	if (led_cdev->blink_delay_on || led_cdev->blink_delay_off)
		return -EBUSY;

	led_cdev->brightness = min(value, led_cdev->max_brightness);

	if (led_cdev->flags & LED_SUSPENDED)
		return 0;

	if (led_cdev->brightness_set_sync)
		return led_cdev->brightness_set_sync(led_cdev,
							 led_cdev->brightness);
	return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(led_set_brightness_sync);

/**
 * no_seek_end_llseek - llseek implementation for fixed-sized devices
 * @file:	file structure to seek on
 * @offset:	file offset to seek to
 * @whence:	type of seek
 *
 */
loff_t no_seek_end_llseek(struct file *file, loff_t offset, int whence)
{
	switch (whence) {
	case SEEK_SET: case SEEK_CUR:
		return generic_file_llseek_size(file, offset, whence,
						~0ULL, 0);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(no_seek_end_llseek);

/**
 * memdup_user_nul - duplicate memory region from user space and NUL-terminate
 *
 * @src: source address in user space
 * @len: number of bytes to copy
 *
 * Returns an ERR_PTR() on failure.
 */
void *memdup_user_nul(const void __user *src, size_t len)
{
	char *p;

	/*
	 * Always use GFP_KERNEL, since copy_from_user() can sleep and
	 * cause pagefault, which makes it pointless to use GFP_NOFS
	 * or GFP_ATOMIC.
	 */
	p = kmalloc(len + 1, GFP_KERNEL);
	if (!p)
		return ERR_PTR(-ENOMEM);

	if (copy_from_user(p, src, len)) {
		kfree(p);
		return ERR_PTR(-EFAULT);
	}
	p[len] = '\0';

	return p;
}
EXPORT_SYMBOL_GPL(memdup_user_nul);

void phy_attached_info(struct phy_device *phydev)
{
	phy_attached_print(phydev, NULL);
}
EXPORT_SYMBOL_GPL(phy_attached_info);

#define ATTACHED_FMT "attached PHY driver [%s] (mii_bus:phy_addr=%s, irq=%d)"
void phy_attached_print(struct phy_device *phydev, const char *fmt, ...)
{
	if (!fmt) {
		dev_info(&phydev->dev, ATTACHED_FMT "\n",
			 phydev->drv->name, phydev_name(phydev),
			 phydev->irq);
	} else {
		va_list ap;

		dev_info(&phydev->dev, ATTACHED_FMT,
			 phydev->drv->name, phydev_name(phydev),
			 phydev->irq);

		va_start(ap, fmt);
		vprintk(fmt, ap);
		va_end(ap);
	}
}
EXPORT_SYMBOL_GPL(phy_attached_print);

static void devm_led_trigger_release(struct device *dev, void *res)
{
	led_trigger_unregister(*(struct led_trigger **)res);
}

int devm_led_trigger_register(struct device *dev,
			      struct led_trigger *trig)
{
	struct led_trigger **dr;
	int rc;

	dr = devres_alloc(devm_led_trigger_release, sizeof(*dr),
			  GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	*dr = trig;

	rc = led_trigger_register(trig);
	if (rc)
		devres_free(dr);
	else
		devres_add(dev, dr);

	return rc;
}
EXPORT_SYMBOL_GPL(devm_led_trigger_register);

/**
 * __ioread32_copy - copy data from MMIO space, in 32-bit units
 * @to: destination (must be 32-bit aligned)
 * @from: source, in MMIO space (must be 32-bit aligned)
 * @count: number of 32-bit quantities to copy
 *
 * Copy data from MMIO space to kernel space, in units of 32 bits at a
 * time.  Order of access is not guaranteed, nor is a memory barrier
 * performed afterwards.
 */
void __ioread32_copy(void *to, const void __iomem *from, size_t count)
{
	u32 *dst = to;
	const u32 __iomem *src = from;
	const u32 __iomem *end = src + count;

	while (src < end)
		*dst++ = __raw_readl(src++);
}
EXPORT_SYMBOL_GPL(__ioread32_copy);
