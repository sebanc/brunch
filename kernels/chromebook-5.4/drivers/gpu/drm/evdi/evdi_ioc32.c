// SPDX-License-Identifier: GPL-2.0-only
/**
 * evdi_ioc32.c
 *
 * Copyright (c) 2016 The Chromium OS Authors
 * Copyright (c) 2018 DisplayLink (UK) Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/compat.h>

#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <uapi/drm/evdi_drm.h>

#include "evdi_drv.h"

struct drm_evdi_connect32 {
	int32_t connected;
	int32_t dev_index;
	uint32_t edid_ptr32;
	uint32_t edid_length;
	uint32_t sku_area_limit;
};

struct drm_evdi_grabpix32 {
	uint32_t mode;
	int32_t buf_width;
	int32_t buf_height;
	int32_t buf_byte_stride;
	uint32_t buffer_ptr32;
	int32_t num_rects;
	uint32_t rects_ptr32;
};

static int compat_evdi_connect(struct file *file,
				unsigned int __always_unused cmd,
				unsigned long arg)
{
	struct drm_evdi_connect32 req32;
	struct drm_evdi_connect __user *request;

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32)))
		return -EFAULT;

	request = compat_alloc_user_space(sizeof(*request));
	if (!access_ok(request, sizeof(*request))
	    || __put_user(req32.connected, &request->connected)
	    || __put_user(req32.dev_index, &request->dev_index)
	    || __put_user((void __user *)(unsigned long)req32.edid_ptr32,
			  &request->edid)
	    || __put_user(req32.edid_length, &request->edid_length)
	    || __put_user(req32.sku_area_limit, &request->sku_area_limit))
		return -EFAULT;

	return drm_ioctl(file, DRM_IOCTL_EVDI_CONNECT,
			 (unsigned long)request);
}

static int compat_evdi_grabpix(struct file *file,
				unsigned int __always_unused cmd,
				unsigned long arg)
{
	struct drm_evdi_grabpix32 req32;
	struct drm_evdi_grabpix __user *request;

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32)))
		return -EFAULT;

	request = compat_alloc_user_space(sizeof(*request));
	if (!access_ok(request, sizeof(*request))
	    || __put_user(req32.mode, &request->mode)
	    || __put_user(req32.buf_width, &request->buf_width)
	    || __put_user(req32.buf_height, &request->buf_height)
	    || __put_user(req32.buf_byte_stride, &request->buf_byte_stride)
	    || __put_user((void __user *)(unsigned long)req32.buffer_ptr32,
			  &request->buffer)
	    || __put_user(req32.num_rects, &request->num_rects)
	    || __put_user((void __user *)(unsigned long)req32.rects_ptr32,
			  &request->rects))
		return -EFAULT;

	return drm_ioctl(file, DRM_IOCTL_EVDI_GRABPIX,
			 (unsigned long)request);
}

static drm_ioctl_compat_t *evdi_compat_ioctls[] = {
	[DRM_EVDI_CONNECT] = compat_evdi_connect,
	[DRM_EVDI_GRABPIX] = compat_evdi_grabpix,
};

/**
 * Called whenever a 32-bit process running under a 64-bit kernel
 * performs an ioctl on /dev/dri/card<n>.
 *
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument.
 * \return zero on success or negative number on failure.
 */
long evdi_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	drm_ioctl_compat_t *fn = NULL;
	int ret;

	if (nr < DRM_COMMAND_BASE || nr >= DRM_COMMAND_END)
		return drm_compat_ioctl(filp, cmd, arg);

	if (nr < DRM_COMMAND_BASE + ARRAY_SIZE(evdi_compat_ioctls))
		fn = evdi_compat_ioctls[nr - DRM_COMMAND_BASE];

	if (fn != NULL)
		ret = (*fn) (filp, cmd, arg);
	else
		ret = drm_ioctl(filp, cmd, arg);

	return ret;
}
