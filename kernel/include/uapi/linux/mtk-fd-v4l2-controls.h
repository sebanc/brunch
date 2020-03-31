/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
//
// Copyright (c) 2019 MediaTek Inc.
/*
 * For V4L2_CID_MTK_FD_DETECT_POSE, User can set the desired face direction to
 * be detected for each face angle, there are five face angle and 12 directions.
 * Below shows the definition of face angle and face direction,
 * and a recommended usage of for face detection, the more selected directions
 * the longer HW process time needed.
 *
 * enum face_angle {
 *	MTK_FD_FACE_FRONT,
 *	MTK_FD_FACE_RIGHT_50,
 *	MTK_FD_FACE_LEFT_50,
 *	MTK_FD_FACE_RIGHT_90,
 *	MTK_FD_FACE_LEFT_90,
 *	MTK_FD_FACE_ANGLE_NUM,
 * };
 *
 * struct face_direction_def {
 *	__u16 MTK_FD_FACE_DIR_0 : 1,
 *		MTK_FD_FACE_DIR_30 : 1,
 *		MTK_FD_FACE_DIR_60 : 1,
 *		MTK_FD_FACE_DIR_90 : 1,
 *		MTK_FD_FACE_DIR_120 : 1,
 *		MTK_FD_FACE_DIR_150 : 1,
 *		MTK_FD_FACE_DIR_180 : 1,
 *		MTK_FD_FACE_DIR_210 : 1,
 *		MTK_FD_FACE_DIR_240 : 1,
 *		MTK_FD_FACE_DIR_270 : 1,
 *		MTK_FD_FACE_DIR_300 : 1,
 *		MTK_FD_FACE_DIR_330 : 1,
 *		: 4;
 * };
 *
 * Sample usage:
 * u16 face_directions[MTK_FD_FACE_ANGLE_NUM] = {0};
 *
 *	face_directions[MTK_FD_FACE_FRONT] = 0x3ff;
 *
 */

#ifndef __UAPI_MTK_FD_V4L2_CONTROLS_H__
#define __UAPI_MTK_FD_V4L2_CONTROLS_H__

#include <linux/types.h>
#include <linux/v4l2-controls.h>

/* Set the face angle and directions to be detected */
#define V4L2_CID_MTK_FD_DETECT_POSE		(V4L2_CID_USER_MTK_FD_BASE + 1)

/* Set image widths for an input image to be scaled down for face detection */
#define V4L2_CID_MTK_FD_SCALE_DOWN_IMG_WIDTH	(V4L2_CID_USER_MTK_FD_BASE + 2)

/* Set image heights for an input image to be scaled down for face detection */
#define V4L2_CID_MTK_FD_SCALE_DOWN_IMG_HEIGHT	(V4L2_CID_USER_MTK_FD_BASE + 3)

/* Set the length of scale down size array */
#define V4L2_CID_MTK_FD_SCALE_IMG_NUM		(V4L2_CID_USER_MTK_FD_BASE + 4)

/* Set the detection speed, usually reducing accuracy. */
#define V4L2_CID_MTK_FD_DETECT_SPEED		(V4L2_CID_USER_MTK_FD_BASE + 5)

/* Select the detection model or algorithm to be used. */
#define V4L2_CID_MTK_FD_DETECTION_MODEL		(V4L2_CID_USER_MTK_FD_BASE + 6)

/* We reserve 16 controls for this driver. */
#define V4L2_CID_MTK_FD_MAX			16

#endif /* __UAPI_MTK_FD_V4L2_CONTROLS_H__ */
