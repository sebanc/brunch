// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 - 2016 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include "evdi_drv.h"

static ssize_t frame_count_show(struct device *dev,
				__always_unused struct device_attribute *attr,
				char *buf)
{
	struct drm_minor *drm_minor = dev_get_drvdata(dev);
	struct drm_device *drm_dev = drm_minor->dev;
	struct evdi_device *evdi = drm_dev->dev_private;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			atomic_read(&evdi->frame_count));
}

static struct device_attribute evdi_device_attributes[] = {
	__ATTR_RO(frame_count),
};

void evdi_stats_init(struct evdi_device *evdi)
{
	int i, retval;

	DRM_INFO("evdi: %s\n", __func__);
	atomic_set(&evdi->frame_count, 0);
	for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++) {
		retval =
		    device_create_file(evdi->ddev->primary->kdev,
				       &evdi_device_attributes[i]);
		if (retval)
			DRM_ERROR("evdi: device_create_file failed %d\n",
				  retval);
	}
}

void evdi_stats_cleanup(struct evdi_device *evdi)
{
	int i;

	DRM_INFO("evdi: %s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(evdi_device_attributes); i++)
		device_remove_file(evdi->ddev->primary->kdev,
				   &evdi_device_attributes[i]);
}
