/*
 * cros_ec_pd - Chrome OS EC Power Delivery Device Driver
 *
 * Copyright (C) 2014 Google, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_MFD_CROS_EC_PD_H
#define __LINUX_MFD_CROS_EC_PD_H

#include <linux/mfd/cros_ec_commands.h>

enum cros_ec_pd_device_type {
	PD_DEVICE_TYPE_NONE = 0,
	PD_DEVICE_TYPE_ZINGER = 1,
	PD_DEVICE_TYPE_DINGDONG = 3,
	PD_DEVICE_TYPE_HOHO = 4,
	PD_DEVICE_TYPE_COUNT,
};

#define USB_VID_GOOGLE 0x18d1

#define USB_PID_DINGDONG 0x5011
#define USB_PID_HOHO     0x5010
#define USB_PID_ZINGER   0x5012

struct cros_ec_pd_firmware_image {
	unsigned int id_major;
	unsigned int id_minor;
	uint16_t usb_vid;
	uint16_t usb_pid;
	char *filename;
	ssize_t rw_image_size;
	uint8_t hash[PD_RW_HASH_SIZE];
	uint8_t (*update_hashes)[][PD_RW_HASH_SIZE];
	int update_hash_count;
};

struct cros_ec_pd_update_data {
	struct device *dev;

	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct notifier_block notifier;

	int num_ports;
	int force_update;
	int is_suspending;
};

#define PD_ID_MAJOR_SHIFT 0
#define PD_ID_MAJOR_MASK  0x03ff
#define PD_ID_MINOR_SHIFT 10
#define PD_ID_MINOR_MASK  0xfc00

#define MAJOR_MINOR_TO_DEV_ID(major, minor) \
	((((major) << PD_ID_MAJOR_SHIFT) & PD_ID_MAJOR_MASK) | \
	(((minor) << PD_ID_MINOR_SHIFT) & PD_ID_MINOR_MASK))

enum cros_ec_pd_find_update_firmware_result {
	PD_DO_UPDATE,
	PD_ALREADY_HAVE_LATEST,
	PD_UNKNOWN_DEVICE,
	PD_UNKNOWN_RW,
};

/* Send 96 bytes per write command when flashing PD device */
#define PD_FLASH_WRITE_STEP 96

/*
 * Wait 2s to start an update check after scheduling. This helps to remove
 * needless extra update checks (ex. if a PD device is reset several times
 * immediately after insertion) and fixes load issues on resume.
 */
#define PD_UPDATE_CHECK_DELAY msecs_to_jiffies(2000)

#endif /* __LINUX_MFD_CROS_EC_H */
