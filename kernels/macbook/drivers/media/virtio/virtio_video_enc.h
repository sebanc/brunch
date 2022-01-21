/* SPDX-License-Identifier: GPL-2.0+ */
/* Encoder header for virtio video driver.
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

#ifndef _VIRTIO_VIDEO_ENC_H
#define _VIRTIO_VIDEO_ENC_H

#include "virtio_video.h"

int virtio_video_enc_init(struct video_device *vd);
int virtio_video_enc_init_ctrls(struct virtio_video_stream *stream);
int virtio_video_enc_init_queues(void *priv, struct vb2_queue *src_vq,
				 struct vb2_queue *dst_vq);

#endif /* _VIRTIO_VIDEO_ENC_H */
