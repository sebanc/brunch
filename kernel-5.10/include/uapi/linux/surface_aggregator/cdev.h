/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Surface System Aggregator Module (SSAM) user-space EC interface.
 *
 * Definitions, structs, and IOCTLs for the /dev/surface/aggregator misc
 * device. This device provides direct user-space access to the SSAM EC.
 * Intended for debugging and development.
 *
 * Copyright (C) 2020 Maximilian Luz <luzmaximilian@gmail.com>
 */

#ifndef _UAPI_LINUX_SURFACE_AGGREGATOR_CDEV_H
#define _UAPI_LINUX_SURFACE_AGGREGATOR_CDEV_H

#include <linux/ioctl.h>
#include <linux/types.h>

/**
 * struct ssam_cdev_request - Controller request IOCTL argument.
 * @target_category: Target category of the SAM request.
 * @target_id:       Target ID of the SAM request.
 * @command_id:      Command ID of the SAM request.
 * @instance_id:     Instance ID of the SAM request.
 * @flags:           SAM Request flags.
 * @status:          Request status (output).
 * @payload:         Request payload (input data).
 * @payload.data:    Pointer to request payload data.
 * @payload.length:  Length of request payload data (in bytes).
 * @response:        Request response (output data).
 * @response.data:   Pointer to response buffer.
 * @response.length: On input: Capacity of response buffer (in bytes).
 *                   On output: Length of request response (number of bytes
 *                   in the buffer that are actually used).
 */
struct ssam_cdev_request {
	__u8 target_category;
	__u8 target_id;
	__u8 command_id;
	__u8 instance_id;
	__u16 flags;
	__s16 status;

	struct {
		__u64 data;
		__u16 length;
		__u8 __pad[6];
	} payload;

	struct {
		__u64 data;
		__u16 length;
		__u8 __pad[6];
	} response;
} __attribute__((__packed__));

#define SSAM_CDEV_REQUEST	_IOWR(0xA5, 1, struct ssam_cdev_request)

#endif /* _UAPI_LINUX_SURFACE_AGGREGATOR_CDEV_H */
