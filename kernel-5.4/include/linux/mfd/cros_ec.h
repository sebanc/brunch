/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * ChromeOS EC multi-function device
 *
 * Copyright (C) 2012 Google, Inc
 */

#ifndef __LINUX_MFD_CROS_EC_H
#define __LINUX_MFD_CROS_EC_H

#include <linux/device.h>

/**
 * struct cros_ec_dev - ChromeOS EC device entry point.
 * @class_dev: Device structure used in sysfs.
 * @ec_dev: cros_ec_device structure to talk to the physical device.
 * @dev: Pointer to the platform device.
 * @debug_info: cros_ec_debugfs structure for debugging information.
 * @has_kb_wake_angle: True if at least 2 accelerometer are connected to the EC.
 * @cmd_offset: Offset to apply for each command.
 * @features: Features supported by the EC.
 */
struct cros_ec_dev {
	struct device class_dev;
	struct cros_ec_device *ec_dev;
	struct device *dev;
	struct cros_ec_debugfs *debug_info;
	bool has_kb_wake_angle;
	u16 cmd_offset;
	u32 features[2];
};

#define to_cros_ec_dev(dev)  container_of(dev, struct cros_ec_dev, class_dev)

/**
 * cros_ec_check_features - Test for the presence of EC features
 *
 * Call this function to test whether the ChromeOS EC supports a feature.
 *
 * @ec_dev: EC device
 * @msg: One of ec_feature_code values
 * @return: 1 if supported, 0 if not
 */
int cros_ec_check_features(struct cros_ec_dev *ec, int feature);

#endif /* __LINUX_MFD_CROS_EC_H */
