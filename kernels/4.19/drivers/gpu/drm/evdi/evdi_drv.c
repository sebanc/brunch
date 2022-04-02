// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc_helper.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb.h>

#include "evdi_drv.h"
#include <uapi/drm/evdi_drm.h>
#include "evdi_debug.h"
#include "evdi_cursor.h"

MODULE_AUTHOR("DisplayLink (UK) Ltd.");
MODULE_DESCRIPTION("Extensible Virtual Display Interface");
MODULE_LICENSE("GPL");

#define EVDI_DEVICE_COUNT_MAX 16
#define MAX_EVDI_USB_ADDR 10

static struct evdi_context {
	struct device *root_dev;
	unsigned int dev_count;
	struct platform_device *devices[EVDI_DEVICE_COUNT_MAX];
	struct notifier_block usb_notifier;
	struct mutex lock;
} evdi_context;

#define evdi_context_lock(ctx) \
		mutex_lock(&ctx->lock)

#define evdi_context_unlock(ctx) \
		mutex_unlock(&ctx->lock)


struct evdi_platform_device_data {
	struct drm_device *drm_dev;
	struct device *parent;
	bool symlinked;
};

static void
evdi_platform_device_unlink_if_linked_with(struct platform_device *pdev,
					   struct device *parent);

static int evdi_platform_drv_usb(__always_unused struct notifier_block *nb,
		unsigned long action,
		void *data)
{
	struct usb_device *usb_dev = (struct usb_device *)(data);
	struct platform_device *pdev;
	int i = 0;

	if (!usb_dev)
		return 0;
	if (action != BUS_NOTIFY_DEL_DEVICE)
		return 0;

	evdi_context_lock((&evdi_context));
	for (i = 0; i < EVDI_DEVICE_COUNT_MAX; ++i) {
		pdev = evdi_context.devices[i];
		if (!pdev)
			continue;
		evdi_platform_device_unlink_if_linked_with(pdev, &usb_dev->dev);
		if (pdev->dev.parent == &usb_dev->dev) {
			EVDI_INFO("Parent USB removed. Removing evdi.%d\n", i);
			platform_device_unregister(pdev);
			evdi_context.dev_count--;
			evdi_context.devices[i] = NULL;
		}
	}
	evdi_context_unlock((&evdi_context));

	return 0;
}

static int evdi_context_get_free_idx(struct evdi_context *ctx)
{
	int i;

	for (i = 0; i < EVDI_DEVICE_COUNT_MAX; ++i) {
		if (ctx->devices[i] == NULL)
			return i;
	}
	return -ENOMEM;
}

static bool evdi_platform_device_is_free(struct platform_device *pdev)
{
	struct evdi_platform_device_data *data =
		(struct evdi_platform_device_data *)platform_get_drvdata(pdev);
	struct evdi_device *evdi = data->drm_dev->dev_private;

	if (evdi && !evdi_painter_is_connected(evdi) &&
	    !data->symlinked)
		return true;
	return false;
}

static void evdi_platform_device_link(struct platform_device *pdev,
				      struct device *parent)
{
	struct evdi_platform_device_data *data = NULL;
	int ret = 0;

	if (!parent || !pdev)
		return;

	data = (struct evdi_platform_device_data *)platform_get_drvdata(pdev);
	if (!evdi_platform_device_is_free(pdev)) {
		EVDI_FATAL("Device is already attached can't symlink again\n");
		return;
	}

	ret = sysfs_create_link(&pdev->dev.kobj, &parent->kobj, "device");
	if (ret) {
		EVDI_FATAL("Failed to create sysfs link to parent device\n");
	} else {
		data->symlinked = true;
		data->parent = parent;
	}
}

static void evdi_platform_device_unlink_if_linked_with(struct platform_device *pdev,
				struct device *parent)
{
	struct evdi_platform_device_data *data =
		(struct evdi_platform_device_data *)platform_get_drvdata(pdev);

	if (parent && data->parent == parent) {
		sysfs_remove_link(&pdev->dev.kobj, "device");
		data->symlinked = false;
		data->parent = NULL;
		EVDI_INFO("Detached from parent device\n");
	}
}

static struct drm_driver driver;

struct drm_ioctl_desc evdi_painter_ioctls[] = {
	DRM_IOCTL_DEF_DRV(EVDI_CONNECT, evdi_painter_connect_ioctl,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_REQUEST_UPDATE,
				evdi_painter_request_update_ioctl,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_GRABPIX, evdi_painter_grabpix_ioctl,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_DDCCI_RESPONSE,
				evdi_painter_ddcci_response_ioctl,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(EVDI_ENABLE_CURSOR_EVENTS,
			    evdi_painter_enable_cursor_events_ioctl,
				DRM_UNLOCKED),
};

static const struct vm_operations_struct evdi_gem_vm_ops = {
	.fault = evdi_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct file_operations evdi_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.mmap = evdi_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.unlocked_ioctl = drm_ioctl,
	.release = drm_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = evdi_compat_ioctl,
#endif
	.llseek = noop_llseek,
};

static int evdi_enable_vblank(__always_unused struct drm_device *dev,
	__always_unused unsigned int pipe)
{
	return 1;
}

static void evdi_disable_vblank(__always_unused struct drm_device *dev,
	__always_unused unsigned int pipe)
{
}

static struct drm_driver driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME |
	DRIVER_ATOMIC,
	.load = evdi_driver_load,
	.unload = evdi_driver_unload,
	.preclose = evdi_driver_preclose,

	/* gem hooks */
	.gem_free_object = evdi_gem_free_object,
	.gem_vm_ops = &evdi_gem_vm_ops,

	.dumb_create = evdi_dumb_create,
	.dumb_map_offset = evdi_gem_mmap,
	.dumb_destroy = drm_gem_dumb_destroy,

	.ioctls = evdi_painter_ioctls,
	.num_ioctls = ARRAY_SIZE(evdi_painter_ioctls),

	.fops = &evdi_driver_fops,

	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_import = evdi_gem_prime_import,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.gem_prime_export = evdi_gem_prime_export,

	.enable_vblank = evdi_enable_vblank,
	.disable_vblank = evdi_disable_vblank,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static struct platform_device *evdi_platform_drv_get_free_device(
				struct evdi_context *ctx)
{
	int i;
	struct platform_device *pdev = NULL;

	for (i = 0; i < EVDI_DEVICE_COUNT_MAX; ++i) {
		pdev = ctx->devices[i];
		if (pdev && evdi_platform_device_is_free(pdev))
			return pdev;
	}
	return NULL;
}

static struct platform_device *evdi_platform_drv_create_new_device(
				struct evdi_context *ctx)
{
	struct platform_device_info pdevinfo = {
		.parent = NULL,
		.name = "evdi",
		.id = evdi_context_get_free_idx(ctx),
		.res = NULL,
		.num_res = 0,
		.data = NULL,
		.size_data = 0,
		.dma_mask = DMA_BIT_MASK(32),
	};

	if (pdevinfo.id < 0 || ctx->dev_count >= EVDI_DEVICE_COUNT_MAX) {
		EVDI_ERROR("Evdi device add failed. Too many devices.\n");
		return ERR_PTR(-EINVAL);
	}

	ctx->devices[pdevinfo.id] = platform_device_register_full(&pdevinfo);
	if (dma_set_mask(&evdi_context.devices[evdi_context.dev_count]->dev,
			 DMA_BIT_MASK(64))) {
		EVDI_DEBUG("Unable to change dma mask to 64 bit. ");
		EVDI_DEBUG("Sticking with 32 bit\n");
	}

	ctx->dev_count++;
	return ctx->devices[pdevinfo.id];
}

static int evdi_add_device(struct evdi_context *ctx, struct device *parent)
{
	struct platform_device *pdev = NULL;

	evdi_context_lock(ctx);
	if (parent)
		pdev = evdi_platform_drv_get_free_device(ctx);

	if (IS_ERR_OR_NULL(pdev))
		pdev = evdi_platform_drv_create_new_device(ctx);
	evdi_context_unlock(ctx);

	if (IS_ERR_OR_NULL(pdev))
		return -EINVAL;

	evdi_platform_device_link(pdev, parent);
	return 0;
}

static int evdi_drm_device_setup(struct drm_device *dev)
{
	struct platform_device *platdev = NULL;
	struct evdi_device *evdi;
	int ret;

	EVDI_CHECKPT();
	evdi = kzalloc(sizeof(struct evdi_device), GFP_KERNEL);
	if (!evdi)
		return -ENOMEM;

	evdi->ddev = dev;
	dev->dev_private = evdi;

	ret =	evdi_cursor_init(&evdi->cursor);
	if (ret)
		goto err;

	EVDI_CHECKPT();
	evdi_modeset_init(dev);

	if (ret)
		goto err;

#ifdef CONFIG_FB
	ret = evdi_fbdev_init(dev);
	if (ret)
		goto err;
#endif /* CONFIG_FB */

	ret = drm_vblank_init(dev, 1);
	if (ret)
		goto err_fb;

	ret = evdi_painter_init(evdi);
	if (ret)
		goto err_fb;

	drm_kms_helper_poll_init(dev);

	platdev = to_platform_device(dev->dev);
	platform_set_drvdata(platdev, dev);

	return 0;

err_fb:
#ifdef CONFIG_FB
	evdi_fbdev_cleanup(dev);
#endif /* CONFIG_FB */
err:
	kfree(evdi);
	EVDI_ERROR("%d\n", ret);
	if (evdi->cursor)
		evdi_cursor_free(evdi->cursor);
	return ret;
}

static int evdi_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct drm_device *dev = NULL;
	struct evdi_platform_device_data *data =
		kzalloc(sizeof(struct evdi_platform_device_data), GFP_KERNEL);

	EVDI_CHECKPT();

	dev = drm_dev_alloc(&driver, &pdev->dev);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ret = evdi_drm_device_setup(dev);
	if (ret)
		goto err_free;

	ret = drm_dev_register(dev, 0);
	if (ret)
		goto err_free;
	else {
		data->drm_dev = dev;
		data->symlinked = false;
		platform_set_drvdata(pdev, data);
	}

	return 0;

err_free:
	kfree(data);
	drm_dev_unref(dev);
	return ret;
}

static int evdi_platform_remove(struct platform_device *pdev)
{
	struct evdi_platform_device_data *data =
		(struct evdi_platform_device_data *)platform_get_drvdata(pdev);

	EVDI_CHECKPT();

	drm_dev_unplug(data->drm_dev);
	kfree(data);

	return 0;
}

static void evdi_remove_all(void)
{
	int i;

	EVDI_DEBUG("removing all evdi devices\n");
	evdi_context_lock((&evdi_context));
	for (i = 0; i < EVDI_DEVICE_COUNT_MAX; ++i) {
		if (evdi_context.devices[i]) {
			EVDI_DEBUG("removing evdi %d\n", i);

			platform_device_unregister(evdi_context.devices[i]);
			evdi_context.dev_count--;
			evdi_context.devices[i] = NULL;
		}
	}
	evdi_context.dev_count = 0;
	evdi_context_unlock((&evdi_context));
}

static struct platform_driver evdi_platform_driver = {
	.probe = evdi_platform_probe,
	.remove = evdi_platform_remove,
	.driver = {
			 .name = "evdi",
			 .mod_name = KBUILD_MODNAME,
			 .owner = THIS_MODULE,
	}
};

static ssize_t version_show(__always_unused struct device *dev,
				__always_unused struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u.%u.%u\n", DRIVER_MAJOR,
			DRIVER_MINOR, DRIVER_PATCHLEVEL);
}

static ssize_t count_show(__always_unused struct device *dev,
				__always_unused struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", evdi_context.dev_count);
}

struct evdi_usb_addr {
	int addr[MAX_EVDI_USB_ADDR];
	int len;
	struct usb_device *usb;
};

static int evdi_platform_device_attach(struct device *device,
		struct evdi_usb_addr *parent_addr);

static ssize_t add_device_with_usb_path(struct device *dev,
			 const char *buf, size_t count)
{
	char *usb_path = kstrdup(buf, GFP_KERNEL);
	char *temp_path = usb_path;
	char *bus_token;
	char *usb_token;
	char *usb_token_copy = NULL;
	char *token;
	char *bus;
	char *port;
	struct evdi_usb_addr usb_addr;

	if (!usb_path)
		return -ENOMEM;

	memset(&usb_addr, 0, sizeof(usb_addr));
	temp_path = strnstr(temp_path, "usb:", count);
	if (!temp_path)
		goto err_parse_usb_path;

	temp_path = strim(temp_path);

	bus_token = strsep(&temp_path, ":");
	if (!bus_token)
		goto err_parse_usb_path;

	usb_token = strsep(&temp_path, ":");
	if (!usb_token)
		goto err_parse_usb_path;

	/* Separate trailing ':*' from usb_token */
	strsep(&temp_path, ":");

	token = usb_token_copy = kstrdup(usb_token, GFP_KERNEL);
	bus = strsep(&token, "-");
	if (!bus)
		goto err_parse_usb_path;
	if (kstrtouint(bus, 10, &usb_addr.addr[usb_addr.len++]))
		goto err_parse_usb_path;

	do {
		port = strsep(&token, ".");
		if (!port)
			goto err_parse_usb_path;
		if (kstrtouint(port, 10, &usb_addr.addr[usb_addr.len++]))
			goto err_parse_usb_path;
	} while (token && port && usb_addr.len < MAX_EVDI_USB_ADDR);

	if (evdi_platform_device_attach(dev, &usb_addr) != 0) {
		EVDI_ERROR("Unable to attach to: %s\n", buf);
		kfree(usb_path);
		kfree(usb_token_copy);
		return -EINVAL;
	}

	EVDI_INFO("Attaching to %s:%s\n", bus_token, usb_token);
	kfree(usb_path);
	kfree(usb_token_copy);
	return count;

err_parse_usb_path:
	EVDI_ERROR("Unable to parse usb path: %s", buf);
	kfree(usb_path);
	kfree(usb_token_copy);
	return -EINVAL;
}

static int find_usb_device_at_path(struct usb_device *usb, void *data)
{
	struct evdi_usb_addr *find_path = (struct evdi_usb_addr *)(data);
	struct usb_device *pdev = usb;
	int port = 0;
	int i;

	i = find_path->len - 1;
	while (pdev != NULL && i >= 0 && i < MAX_EVDI_USB_ADDR) {
		port = pdev->portnum;
		if (port == 0)
			port = pdev->bus->busnum;

		if (port != find_path->addr[i])
			return 0;

		if (pdev->parent == NULL && i == 0) {
			find_path->usb = usb;
			return 1;
		}
		pdev = pdev->parent;
		i--;
	}

	return 0;
}

int evdi_platform_device_attach(struct device *device,
		struct evdi_usb_addr *parent_addr)
{
	struct device *parent = NULL;

	if (!parent_addr)
		return -EINVAL;

	if (!usb_for_each_dev(parent_addr, find_usb_device_at_path) ||
	    !parent_addr->usb)
		return -EINVAL;

	parent = &parent_addr->usb->dev;
	return evdi_add_device(&evdi_context, parent);
}

static ssize_t add_store(struct device *dev,
			 __always_unused struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int val;

	if (strnstr(buf, "usb:", count))
		return add_device_with_usb_path(dev, buf, count);

	if (kstrtouint(buf, 10, &val)) {
		EVDI_ERROR("Invalid device count \"%s\"\n", buf);
		return -EINVAL;
	}
	if (val == 0) {
		EVDI_WARN("Adding 0 devices has no effect\n");
		return count;
	}
	if (val > EVDI_DEVICE_COUNT_MAX - evdi_context.dev_count) {
		EVDI_ERROR("Evdi device add failed. Too many devices.\n");
		return -EINVAL;
	}

	EVDI_DEBUG("Increasing device count to %u\n",
			evdi_context.dev_count + val);
	while (val-- && evdi_add_device(&evdi_context, NULL) == 0)
		;

	return count;
}

static ssize_t remove_all_store(__always_unused struct device *dev,
				__always_unused struct device_attribute *attr,
				__always_unused const char *buf,
				size_t count)
{
	evdi_remove_all();
	return count;
}

static ssize_t loglevel_show(__always_unused struct device *dev,
				__always_unused struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", evdi_loglevel);
}

static ssize_t loglevel_store(__always_unused struct device *dev,
				__always_unused struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val)) {
		EVDI_ERROR("Unable to parse %u\n", val);
		return -EINVAL;
	}
	if (val > EVDI_LOGLEVEL_VERBOSE) {
		EVDI_ERROR("Invalid loglevel %u\n", val);
		return -EINVAL;
	}

	EVDI_INFO("Setting loglevel to %u\n", val);
	evdi_loglevel = val;
	return count;
}

static struct device_attribute evdi_device_attributes[] = {
	__ATTR_RO(count),
	__ATTR_RO(version),
	__ATTR_RW(loglevel),
	__ATTR_WO(add),
	__ATTR_WO(remove_all)
};

static int __init evdi_init(void)
{
	int i;

	EVDI_INFO("Initialising logging on level %u\n", evdi_loglevel);
	EVDI_INFO("Atomic driver:%s",
		(driver.driver_features & DRIVER_ATOMIC) ? "yes" : "no");
	evdi_context.root_dev = root_device_register("evdi");
	evdi_context.usb_notifier.notifier_call = evdi_platform_drv_usb;
	mutex_init(&evdi_context.lock);

	usb_register_notify(&evdi_context.usb_notifier);
	if (!PTR_RET(evdi_context.root_dev))
		for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++) {
			device_create_file(evdi_context.root_dev,
						 &evdi_device_attributes[i]);
		}

	return platform_driver_register(&evdi_platform_driver);
}

static void __exit evdi_exit(void)
{
	int i;

	EVDI_CHECKPT();
	evdi_remove_all();

	usb_unregister_notify(&evdi_context.usb_notifier);
	platform_driver_unregister(&evdi_platform_driver);

	if (!PTR_RET(evdi_context.root_dev)) {
		for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++) {
			device_remove_file(evdi_context.root_dev,
						 &evdi_device_attributes[i]);
		}
		root_device_unregister(evdi_context.root_dev);
	}
}

module_init(evdi_init);
module_exit(evdi_exit);
