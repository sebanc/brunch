// SPDX-License-Identifier: GPL-2.0+
/*
 * Surface performance-mode driver.
 *
 * Provides a user-space interface for the performance mode control provided
 * by the Surface System Aggregator Module (SSAM), influencing cooling
 * behavior of the device and potentially managing power limits.
 *
 * Copyright (C) 2019-2021 Maximilian Luz <luzmaximilian@gmail.com>
 */

#include <asm/unaligned.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include <linux/surface_aggregator/device.h>

enum sam_perf_mode {
	SAM_PERF_MODE_NORMAL  = 1,
	SAM_PERF_MODE_BATTERY = 2,
	SAM_PERF_MODE_PERF1   = 3,
	SAM_PERF_MODE_PERF2   = 4,

	__SAM_PERF_MODE__MIN  = 1,
	__SAM_PERF_MODE__MAX  = 4,
};

struct ssam_perf_info {
	__le32 mode;
	__le16 unknown1;
	__le16 unknown2;
} __packed;

SSAM_DEFINE_SYNC_REQUEST_CL_R(ssam_tmp_perf_mode_get, struct ssam_perf_info, {
	.target_category = SSAM_SSH_TC_TMP,
	.command_id      = 0x02,
});

SSAM_DEFINE_SYNC_REQUEST_CL_W(__ssam_tmp_perf_mode_set, __le32, {
	.target_category = SSAM_SSH_TC_TMP,
	.command_id      = 0x03,
});

static int ssam_tmp_perf_mode_set(struct ssam_device *sdev, u32 mode)
{
	__le32 mode_le = cpu_to_le32(mode);

	if (mode < __SAM_PERF_MODE__MIN || mode > __SAM_PERF_MODE__MAX)
		return -EINVAL;

	return ssam_retry(__ssam_tmp_perf_mode_set, sdev, &mode_le);
}

static ssize_t perf_mode_show(struct device *dev, struct device_attribute *attr,
			      char *data)
{
	struct ssam_device *sdev = to_ssam_device(dev);
	struct ssam_perf_info info;
	int status;

	status = ssam_retry(ssam_tmp_perf_mode_get, sdev, &info);
	if (status) {
		dev_err(dev, "failed to get current performance mode: %d\n",
			status);
		return -EIO;
	}

	return sprintf(data, "%d\n", le32_to_cpu(info.mode));
}

static ssize_t perf_mode_store(struct device *dev, struct device_attribute *attr,
			       const char *data, size_t count)
{
	struct ssam_device *sdev = to_ssam_device(dev);
	int perf_mode;
	int status;

	status = kstrtoint(data, 0, &perf_mode);
	if (status < 0)
		return status;

	status = ssam_tmp_perf_mode_set(sdev, perf_mode);
	if (status < 0)
		return status;

	return count;
}

static const DEVICE_ATTR_RW(perf_mode);

static int surface_sam_sid_perfmode_probe(struct ssam_device *sdev)
{
	return sysfs_create_file(&sdev->dev.kobj, &dev_attr_perf_mode.attr);
}

static void surface_sam_sid_perfmode_remove(struct ssam_device *sdev)
{
	sysfs_remove_file(&sdev->dev.kobj, &dev_attr_perf_mode.attr);
}

static const struct ssam_device_id ssam_perfmode_match[] = {
	{ SSAM_SDEV(TMP, 0x01, 0x00, 0x01) },
	{ },
};
MODULE_DEVICE_TABLE(ssam, ssam_perfmode_match);

static struct ssam_device_driver surface_sam_sid_perfmode = {
	.probe = surface_sam_sid_perfmode_probe,
	.remove = surface_sam_sid_perfmode_remove,
	.match_table = ssam_perfmode_match,
	.driver = {
		.name = "surface_performance_mode",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_ssam_device_driver(surface_sam_sid_perfmode);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Performance mode interface for Surface System Aggregator Module");
MODULE_LICENSE("GPL");
