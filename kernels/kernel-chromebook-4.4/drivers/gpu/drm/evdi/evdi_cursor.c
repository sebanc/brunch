/*
 * evdi_cursor.c
 *
 * Copyright (c) 2016 The Chromium OS Authors
 * Copyright (c) 2016 - 2017 DisplayLink (UK) Ltd.
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
#include <linux/compiler.h>
#include <linux/mutex.h>

#include "evdi_cursor.h"
#include "evdi_drv.h"

/*
 * EVDI drm cursor private structure.
 */
struct evdi_cursor {
	bool enabled;
	int32_t x;
	int32_t y;
	uint32_t width;
	uint32_t height;
	int32_t hot_x;
	int32_t hot_y;
	uint32_t pixel_format;
	uint32_t stride;
	struct evdi_gem_object *obj;
	struct mutex lock;
};

static void evdi_cursor_set_gem(struct evdi_cursor *cursor,
				struct evdi_gem_object *obj)
{
	if (obj)
		drm_gem_object_reference(&obj->base);
	if (cursor->obj)
		drm_gem_object_unreference_unlocked(&cursor->obj->base);

	cursor->obj = obj;
}

struct evdi_gem_object *evdi_cursor_gem(struct evdi_cursor *cursor)
{
	return cursor->obj;
}

int evdi_cursor_init(struct evdi_cursor **cursor)
{
	if (WARN_ON(*cursor))
		return -EINVAL;

	*cursor = kzalloc(sizeof(struct evdi_cursor), GFP_KERNEL);
	if (*cursor) {
		mutex_init(&(*cursor)->lock);
		return 0;
	} else {
		return -ENOMEM;
	}
}

void evdi_cursor_lock(struct evdi_cursor *cursor)
{
	mutex_lock(&cursor->lock);
}

void evdi_cursor_unlock(struct evdi_cursor *cursor)
{
	mutex_unlock(&cursor->lock);
}

void evdi_cursor_free(struct evdi_cursor *cursor)
{
	if (WARN_ON(!cursor))
		return;
	evdi_cursor_set_gem(cursor, NULL);
	kfree(cursor);
}

bool evdi_cursor_enabled(struct evdi_cursor *cursor)
{
	return cursor->enabled;
}

void evdi_cursor_enable(struct evdi_cursor *cursor, bool enable)
{
	evdi_cursor_lock(cursor);
	cursor->enabled = enable;
	if (!enable)
		evdi_cursor_set_gem(cursor, NULL);
	evdi_cursor_unlock(cursor);
}

void evdi_cursor_set(struct evdi_cursor *cursor,
		     struct evdi_gem_object *obj,
		     uint32_t width, uint32_t height,
		     int32_t hot_x, int32_t hot_y,
		     uint32_t pixel_format, uint32_t stride)
{
	int err = 0;

	evdi_cursor_lock(cursor);
	if (obj && !obj->vmapping)
		err = evdi_gem_vmap(obj);

	if (err != 0) {
		EVDI_ERROR("Failed to map cursor.\n");
		obj = NULL;
	}

	cursor->enabled = obj != NULL;
	cursor->width = width;
	cursor->height = height;
	cursor->hot_x = hot_x;
	cursor->hot_y = hot_y;
	cursor->pixel_format = pixel_format;
	cursor->stride = stride;
	evdi_cursor_set_gem(cursor, obj);

	evdi_cursor_unlock(cursor);
}

void evdi_cursor_move(struct evdi_cursor *cursor, int32_t x, int32_t y)
{
	evdi_cursor_lock(cursor);
	cursor->x = x;
	cursor->y = y;
	evdi_cursor_unlock(cursor);
}

static inline uint32_t blend_component(uint32_t pixel,
				  uint32_t blend,
				  uint32_t alpha)
{
	uint32_t pre_blend = (pixel * (255 - alpha) + blend * alpha);

	return (pre_blend + ((pre_blend + 1) << 8)) >> 16;
}

static inline uint32_t blend_alpha(const uint32_t pixel_val32,
				uint32_t blend_val32)
{
	uint32_t alpha = (blend_val32 >> 24);

	return blend_component(pixel_val32 & 0xff,
			       blend_val32 & 0xff, alpha) |
			blend_component((pixel_val32 & 0xff00) >> 8,
				(blend_val32 & 0xff00) >> 8, alpha) << 8 |
			blend_component((pixel_val32 & 0xff0000) >> 16,
				(blend_val32 & 0xff0000) >> 16, alpha) << 16;
}

static int evdi_cursor_compose_pixel(char __user *buffer,
				     int const cursor_value,
				     int const fb_value,
				     int cmd_offset)
{
	int const composed_value = blend_alpha(fb_value, cursor_value);

	return copy_to_user(buffer + cmd_offset, &composed_value, 4);
}

int evdi_cursor_compose_and_copy(struct evdi_cursor *cursor,
				 struct evdi_framebuffer *ufb,
				 char __user *buffer,
				 int buf_byte_stride)
{
	int x, y;
	struct drm_framebuffer *fb = &ufb->base;
	const int h_cursor_w = cursor->width >> 1;
	const int h_cursor_h = cursor->height >> 1;
	uint32_t *cursor_buffer = NULL;
	uint32_t bytespp = 0;

	if (!cursor->enabled)
		return 0;

	if (!cursor->obj)
		return -EINVAL;

	if (!cursor->obj->vmapping)
		return -EINVAL;

	bytespp = evdi_fb_get_bpp(cursor->pixel_format);
	bytespp = DIV_ROUND_UP(bytespp, 8);
	if (bytespp != 4) {
		EVDI_ERROR("Unsupported cursor format bpp=%u\n", bytespp);
		return -EINVAL;
	}

	if (cursor->width * cursor->height * bytespp >
	    cursor->obj->base.size){
		EVDI_ERROR("Wrong cursor size\n");
		return -EINVAL;
	}

	cursor_buffer = (uint32_t *)cursor->obj->vmapping;

	for (y = -h_cursor_h; y < h_cursor_h; ++y) {
		for (x = -h_cursor_w; x < h_cursor_w; ++x) {
			uint32_t curs_val;
			int *fbsrc;
			int fb_value;
			int cmd_offset;
			int cursor_pix;
			int const mouse_pix_x = cursor->x + x + h_cursor_w;
			int const mouse_pix_y = cursor->y + y + h_cursor_h;
			bool const is_pix_sane =
				mouse_pix_x >= 0 &&
				mouse_pix_y >= 0 &&
				mouse_pix_x < fb->width &&
				mouse_pix_y < fb->height;

			if (!is_pix_sane)
				continue;

			cursor_pix = h_cursor_w+x +
				    (h_cursor_h+y)*cursor->width;
			curs_val = le32_to_cpu(cursor_buffer[cursor_pix]);
			fbsrc = (int *)ufb->obj->vmapping;
			fb_value = *(fbsrc + ((fb->pitches[0]>>2) *
						  mouse_pix_y + mouse_pix_x));
			cmd_offset = (buf_byte_stride * mouse_pix_y) +
						       (mouse_pix_x * bytespp);
			if (evdi_cursor_compose_pixel(buffer,
						      curs_val,
						      fb_value,
						      cmd_offset)) {
				EVDI_ERROR("Failed to compose cursor pixel\n");
				return -EFAULT;
			}
		}
	}

	return 0;
}

void evdi_cursor_position(struct evdi_cursor *cursor, int32_t *x, int32_t *y)
{
	*x = cursor->x;
	*y = cursor->y;
}

void evdi_cursor_hotpoint(struct evdi_cursor *cursor,
			  int32_t *hot_x, int32_t *hot_y)
{
	*hot_x = cursor->hot_x;
	*hot_y = cursor->hot_y;
}

void evdi_cursor_size(struct evdi_cursor *cursor,
		      uint32_t *width, uint32_t *height)
{
	*width = cursor->width;
	*height = cursor->height;
}

void evdi_cursor_format(struct evdi_cursor *cursor, uint32_t *format)
{
	*format = cursor->pixel_format;
}

void evdi_cursor_stride(struct evdi_cursor *cursor, uint32_t *stride)
{
	*stride = cursor->stride;
}

