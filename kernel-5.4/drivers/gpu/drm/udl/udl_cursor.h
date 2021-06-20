/*
 * udl_cursor.h
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

#ifndef _UDL_CURSOR_H_
#define _UDL_CURSOR_H_

#include <linux/module.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>

struct udl_cursor;
struct udl_cursor_hline {
	uint32_t *buffer;
	int width;
	int offset;
};

extern int udl_cursor_alloc(struct udl_cursor **cursor);
extern void udl_cursor_free(struct udl_cursor *cursor);
extern void udl_cursor_copy(struct udl_cursor *dst, struct udl_cursor *src);
extern bool udl_cursor_enabled(struct udl_cursor *cursor);
extern void udl_cursor_get_hline(struct udl_cursor *cursor, int x, int y,
		struct udl_cursor_hline *hline);
extern int udl_cursor_set(struct drm_crtc *crtc, struct drm_file *file,
		uint32_t handle, uint32_t width, uint32_t height,
		struct udl_cursor *cursor);
extern int udl_cursor_move(struct drm_crtc *crtc, int x, int y,
		struct udl_cursor *cursor);

#endif
