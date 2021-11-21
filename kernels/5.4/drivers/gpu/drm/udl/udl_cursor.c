/*
 * udl_cursor.c
 *
 * Copyright (c) 2015 The Chromium OS Authors
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

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include "udl_cursor.h"
#include "udl_drv.h"

#define UDL_CURSOR_W 64
#define UDL_CURSOR_H 64
#define UDL_CURSOR_BUF (UDL_CURSOR_W * UDL_CURSOR_H)

/*
 * UDL drm cursor private structure.
 */
struct udl_cursor {
	uint32_t buffer[UDL_CURSOR_BUF];
	bool enabled;
	int x;
	int y;
};

int udl_cursor_alloc(struct udl_cursor **cursor)
{
	struct udl_cursor *new_cursor = kzalloc(sizeof(struct udl_cursor),
		GFP_KERNEL);
	if (!new_cursor)
		return -ENOMEM;
	*cursor = new_cursor;
	return 0;
}

void udl_cursor_free(struct udl_cursor *cursor)
{
	BUG_ON(!cursor);
	kfree(cursor);
}

void udl_cursor_copy(struct udl_cursor *dst, struct udl_cursor *src)
{
	memcpy(dst, src, sizeof(struct udl_cursor));
}

bool udl_cursor_enabled(struct udl_cursor *cursor)
{
	return cursor->enabled;
}

void udl_cursor_get_hline(struct udl_cursor *cursor, int x, int y,
		struct udl_cursor_hline *hline)
{
	if (!cursor || !cursor->enabled ||
		x >= cursor->x + UDL_CURSOR_W ||
		y < cursor->y || y >= cursor->y + UDL_CURSOR_H) {
		hline->buffer = NULL;
		return;
	}

	hline->buffer = &cursor->buffer[UDL_CURSOR_W * (y - cursor->y)];
	hline->width = UDL_CURSOR_W;
	hline->offset = x - cursor->x;
}

/*
 * Return pre-computed cursor blend value defined as:
 * R: 5 bits (bit 0:4)
 * G: 6 bits (bit 5:10)
 * B: 5 bits (bit 11:15)
 * A: 7 bits (bit 16:22)
 */
static uint32_t cursor_blend_val32(uint32_t pix)
{
	/* range of alpha_scaled is 0..64 */
	uint32_t alpha_scaled = ((pix >> 24) * 65) >> 8;
	return ((pix >> 3) & 0x1f) |
		((pix >> 5) & 0x7e0) |
		((pix >> 8) & 0xf800) |
		(alpha_scaled << 16);
}

static int udl_cursor_download(struct udl_cursor *cursor,
		struct drm_gem_object *obj)
{
	struct udl_gem_object *udl_gem_obj = to_udl_bo(obj);
	uint32_t *src_ptr, *dst_ptr;
	size_t i;

	int ret = udl_gem_vmap(udl_gem_obj);
	if (ret != 0) {
		DRM_ERROR("failed to vmap cursor\n");
		return ret;
	}

	src_ptr = udl_gem_obj->vmapping;
	dst_ptr = cursor->buffer;
	for (i = 0; i < UDL_CURSOR_BUF; ++i)
		dst_ptr[i] = cursor_blend_val32(le32_to_cpu(src_ptr[i]));
	return 0;
}

int udl_cursor_set(struct drm_crtc *crtc, struct drm_file *file,
		uint32_t handle, uint32_t width, uint32_t height,
		struct udl_cursor *cursor)
{
	if (handle) {
		struct drm_gem_object *obj;
		int err;
		/* Currently we only support 64x64 cursors */
		if (width != UDL_CURSOR_W || height != UDL_CURSOR_H) {
			DRM_ERROR("we currently only support %dx%d cursors\n",
					UDL_CURSOR_W, UDL_CURSOR_H);
			return -EINVAL;
		}
		obj = drm_gem_object_lookup(file, handle);
		if (!obj) {
			DRM_ERROR("failed to lookup gem object.\n");
			return -EINVAL;
		}
		err = udl_cursor_download(cursor, obj);
		drm_gem_object_put(obj);
		if (err != 0) {
			DRM_ERROR("failed to copy cursor.\n");
			return err;
		}
		cursor->enabled = true;
	} else {
		cursor->enabled = false;
	}

	return 0;
}

int udl_cursor_move(struct drm_crtc *crtc, int x, int y,
		struct udl_cursor *cursor)
{
	cursor->x = x;
	cursor->y = y;
	return 0;
}
