// SPDX-License-Identifier: GPL-2.0+
/* Driver for virtio video device.
 *
 * Copyright 2019 OpenSynergy GmbH.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "virtio_video.h"

struct virtio_video_convert_table {
	uint32_t virtio_value;
	uint32_t v4l2_value;
};

static struct virtio_video_convert_table level_table[] = {
	{ VIRTIO_VIDEO_LEVEL_H264_1_0, V4L2_MPEG_VIDEO_H264_LEVEL_1_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_1_1, V4L2_MPEG_VIDEO_H264_LEVEL_1_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_1_2, V4L2_MPEG_VIDEO_H264_LEVEL_1_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_1_3, V4L2_MPEG_VIDEO_H264_LEVEL_1_3 },
	{ VIRTIO_VIDEO_LEVEL_H264_2_0, V4L2_MPEG_VIDEO_H264_LEVEL_2_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_2_1, V4L2_MPEG_VIDEO_H264_LEVEL_2_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_2_2, V4L2_MPEG_VIDEO_H264_LEVEL_2_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_3_0, V4L2_MPEG_VIDEO_H264_LEVEL_3_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_3_1, V4L2_MPEG_VIDEO_H264_LEVEL_3_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_3_2, V4L2_MPEG_VIDEO_H264_LEVEL_3_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_4_0, V4L2_MPEG_VIDEO_H264_LEVEL_4_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_4_1, V4L2_MPEG_VIDEO_H264_LEVEL_4_1 },
	{ VIRTIO_VIDEO_LEVEL_H264_4_2, V4L2_MPEG_VIDEO_H264_LEVEL_4_2 },
	{ VIRTIO_VIDEO_LEVEL_H264_5_0, V4L2_MPEG_VIDEO_H264_LEVEL_5_0 },
	{ VIRTIO_VIDEO_LEVEL_H264_5_1, V4L2_MPEG_VIDEO_H264_LEVEL_5_1 },
	{ 0 },
};

uint32_t virtio_video_level_to_v4l2(uint32_t level)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(level_table); idx++) {
		if (level_table[idx].virtio_value == level)
			return level_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_level_to_virtio(uint32_t v4l2_level)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(level_table); idx++) {
		if (level_table[idx].v4l2_value == v4l2_level)
			return level_table[idx].virtio_value;
	}

	return 0;
}

static struct virtio_video_convert_table profile_table[] = {
	{ VIRTIO_VIDEO_PROFILE_H264_BASELINE,
		V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE },
	{ VIRTIO_VIDEO_PROFILE_H264_MAIN, V4L2_MPEG_VIDEO_H264_PROFILE_MAIN },
	{ VIRTIO_VIDEO_PROFILE_H264_EXTENDED,
		V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED },
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH, V4L2_MPEG_VIDEO_H264_PROFILE_HIGH },
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH10PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10 },
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH422PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422},
	{ VIRTIO_VIDEO_PROFILE_H264_HIGH444PREDICTIVEPROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE },
	{ VIRTIO_VIDEO_PROFILE_H264_SCALABLEBASELINE,
		V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_BASELINE },
	{ VIRTIO_VIDEO_PROFILE_H264_SCALABLEHIGH,
		V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_HIGH },
	{ VIRTIO_VIDEO_PROFILE_H264_STEREOHIGH,
		V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH },
	{ VIRTIO_VIDEO_PROFILE_H264_MULTIVIEWHIGH,
		V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH },
	{ 0 },
};

uint32_t virtio_video_profile_to_v4l2(uint32_t profile)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(profile_table); idx++) {
		if (profile_table[idx].virtio_value == profile)
			return profile_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_profile_to_virtio(uint32_t v4l2_profile)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(profile_table); idx++) {
		if (profile_table[idx].v4l2_value == v4l2_profile)
			return profile_table[idx].virtio_value;
	}

	return 0;
}

static struct virtio_video_convert_table format_table[] = {
	{ VIRTIO_VIDEO_FORMAT_ARGB8888, V4L2_PIX_FMT_ARGB32 },
	{ VIRTIO_VIDEO_FORMAT_BGRA8888, V4L2_PIX_FMT_ABGR32 },
	{ VIRTIO_VIDEO_FORMAT_NV12, V4L2_PIX_FMT_NV12 },
	{ VIRTIO_VIDEO_FORMAT_YUV420, V4L2_PIX_FMT_YUV420 },
	{ VIRTIO_VIDEO_FORMAT_YVU420, V4L2_PIX_FMT_YVU420 },
	{ VIRTIO_VIDEO_FORMAT_MPEG2, V4L2_PIX_FMT_MPEG2 },
	{ VIRTIO_VIDEO_FORMAT_MPEG4, V4L2_PIX_FMT_MPEG4 },
	{ VIRTIO_VIDEO_FORMAT_H264, V4L2_PIX_FMT_H264 },
	{ VIRTIO_VIDEO_FORMAT_HEVC, V4L2_PIX_FMT_HEVC },
	{ VIRTIO_VIDEO_FORMAT_VP8, V4L2_PIX_FMT_VP8 },
	{ VIRTIO_VIDEO_FORMAT_VP9, V4L2_PIX_FMT_VP9 },
	{ 0 },
};

uint32_t virtio_video_format_to_v4l2(uint32_t format)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(format_table); idx++) {
		if (format_table[idx].virtio_value == format)
			return format_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_format_to_virtio(uint32_t v4l2_format)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(format_table); idx++) {
		if (format_table[idx].v4l2_value == v4l2_format)
			return format_table[idx].virtio_value;
	}

	return 0;
}

static struct virtio_video_convert_table control_table[] = {
	{ VIRTIO_VIDEO_CONTROL_BITRATE, V4L2_CID_MPEG_VIDEO_BITRATE },
	{ VIRTIO_VIDEO_CONTROL_PROFILE, V4L2_CID_MPEG_VIDEO_H264_PROFILE },
	{ VIRTIO_VIDEO_CONTROL_LEVEL, V4L2_CID_MPEG_VIDEO_H264_LEVEL },
	{ 0 },
};

uint32_t virtio_video_control_to_v4l2(uint32_t control)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(control_table); idx++) {
		if (control_table[idx].virtio_value == control)
			return control_table[idx].v4l2_value;
	}

	return 0;
}

uint32_t virtio_video_v4l2_control_to_virtio(uint32_t v4l2_control)
{
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(control_table); idx++) {
		if (control_table[idx].v4l2_value == v4l2_control)
			return control_table[idx].virtio_value;
	}

	return 0;
}

struct video_format *find_video_format(struct list_head *fmts_list,
				       uint32_t format)
{
	struct video_format *fmt = NULL;

	list_for_each_entry(fmt, fmts_list, formats_list_entry) {
		if (fmt->desc.format == format)
			return fmt;
	}

	return NULL;
}

void virtio_video_format_from_info(struct video_format_info *info,
				   struct v4l2_pix_format_mplane *pix_mp)
{
	int i;

	pix_mp->width = info->frame_width;
	pix_mp->height = info->frame_height;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->colorspace = V4L2_COLORSPACE_REC709;
	pix_mp->xfer_func = 0;
	pix_mp->ycbcr_enc = 0;
	pix_mp->quantization = 0;
	memset(pix_mp->reserved, 0, sizeof(pix_mp->reserved));
	memset(pix_mp->plane_fmt[0].reserved, 0,
	       sizeof(pix_mp->plane_fmt[0].reserved));

	pix_mp->num_planes = info->num_planes;
	pix_mp->pixelformat = info->fourcc_format;

	for (i = 0; i < info->num_planes; i++) {
		pix_mp->plane_fmt[i].bytesperline =
					 info->plane_format[i].stride;
		pix_mp->plane_fmt[i].sizeimage =
					 info->plane_format[i].plane_size;
	}
}

void virtio_video_format_fill_default_info(struct video_format_info *dst_info,
					  struct video_format_info *src_info)
{
	memcpy(dst_info, src_info, sizeof(*dst_info));
}
