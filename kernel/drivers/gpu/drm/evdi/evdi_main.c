// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/version.h>
#include <linux/platform_device.h>
#if KERNEL_VERSION(5, 5, 0) > LINUX_VERSION_CODE
#include <drm/drmP.h>
#endif
#include "evdi_drv.h"
#include "evdi_cursor.h"

#if KERNEL_VERSION(5, 1, 0) <= LINUX_VERSION_CODE
#include <drm/drm_probe_helper.h>
#endif

int evdi_driver_setup(struct drm_device *dev)
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

	ret =  evdi_cursor_init(&evdi->cursor);
	if (ret)
		goto err;

	EVDI_CHECKPT();
	evdi_modeset_init(dev);

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
	EVDI_ERROR("%d\n", ret);
	if (evdi->cursor)
		evdi_cursor_free(evdi->cursor);
	kfree(evdi);
	return ret;
}

#if KERNEL_VERSION(4, 12, 0) > LINUX_VERSION_CODE
int evdi_driver_load(struct drm_device *dev,
		     __always_unused unsigned long flags)
{
	return evdi_driver_setup(dev);
}
#endif

#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
int evdi_driver_unload(struct drm_device *dev)
#else
void evdi_driver_unload(struct drm_device *dev)
#endif
{
	struct evdi_device *evdi = dev->dev_private;

	EVDI_CHECKPT();

#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
	drm_vblank_cleanup(dev);
#endif

	drm_kms_helper_poll_fini(dev);

#if KERNEL_VERSION(4, 8, 0) <= LINUX_VERSION_CODE

#elif KERNEL_VERSION(4, 7, 0) <= LINUX_VERSION_CODE
	drm_connector_unregister_all(dev);
#else
	drm_connector_unplug_all(dev);
#endif
#ifdef CONFIG_FB
	evdi_fbdev_unplug(dev);
#endif /* CONFIG_FB */
	if (evdi->cursor)
		evdi_cursor_free(evdi->cursor);
	evdi_painter_cleanup(evdi);
#ifdef CONFIG_FB
	evdi_fbdev_cleanup(dev);
#endif /* CONFIG_FB */
	evdi_modeset_cleanup(dev);

	kfree(evdi);
#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static void evdi_driver_close(struct drm_device *drm_dev, struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;

	EVDI_CHECKPT();
	if (evdi)
		evdi_painter_close(evdi, file);
}

void evdi_driver_preclose(struct drm_device *drm_dev, struct drm_file *file)
{
	evdi_driver_close(drm_dev, file);
}

void evdi_driver_postclose(struct drm_device *drm_dev, struct drm_file *file)
{
	struct evdi_device *evdi = drm_dev->dev_private;

	EVDI_DEBUG("(dev=%d) Process tries to close us, postclose\n",
		   evdi ? evdi->dev_index : -1);
	evdi_log_process();

	evdi_driver_close(drm_dev, file);
}

