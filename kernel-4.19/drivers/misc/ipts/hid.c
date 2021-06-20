/*
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/firmware.h>
#include <linux/intel-ipts.h>

#include "hid.h"
#include "msgs.h"

static int ipts_hid_get_hid_descriptor(ipts_info_t *ipts, u8 **desc, int *size)
{
	u8 *buf;
	int hid_size = 0, ret = 0;
	const struct firmware *vendor_desc = NULL;
	char fw_path[256];

	if (!strcmp(ipts->hardware_id, "MSHW0079"))
		snprintf(fw_path, 256, "intel/ipts/SurfaceTouchServicingDescriptor%s.bin", "MSHW0137");	
	else
		snprintf(fw_path, 256, "intel/ipts/SurfaceTouchServicingDescriptor%s.bin", ipts->hardware_id);
	ret = request_firmware(&vendor_desc, fw_path, &ipts->cldev->dev);
	if (ret) {
		dev_err(&ipts->cldev->dev, "error in reading HID Vendor Descriptor\n");
		return ret;
	}

	hid_size = vendor_desc->size;
	dev_dbg(&ipts->cldev->dev, "hid size = %d\n", hid_size);
	buf = vmalloc(hid_size);
	if (buf == NULL) {
		ret = -ENOMEM;
		goto no_mem;
	}

	memcpy(buf, vendor_desc->data, vendor_desc->size);
	release_firmware(vendor_desc);

	*desc = buf;
	*size = hid_size;

	return 0;
no_mem :
	if (vendor_desc)
		release_firmware(vendor_desc);

	return ret;
}

static int ipts_hid_parse(struct hid_device *hid)
{
	ipts_info_t *ipts = hid->driver_data;
	int ret = 0, size;
	u8 *buf;

	dev_dbg(&ipts->cldev->dev, "ipts_hid_parse() start\n");
	ret = ipts_hid_get_hid_descriptor(ipts, &buf, &size);
	if (ret != 0) {
		dev_err(&ipts->cldev->dev, "ipts_hid_ipts_get_hid_descriptor ret %d\n", ret);
		return -EIO;
	}

	ret = hid_parse_report(hid, buf, size);
	vfree(buf);
	if (ret) {
		dev_err(&ipts->cldev->dev, "hid_parse_report error : %d\n", ret);
		goto out;
	}

out:
	return ret;
}

static int ipts_hid_start(struct hid_device *hid)
{
	return 0;
}

static void ipts_hid_stop(struct hid_device *hid)
{
	return;
}

static int ipts_hid_open(struct hid_device *hid)
{
	return 0;
}

static void ipts_hid_close(struct hid_device *hid)
{
	return;
}

static int ipts_hid_raw_request(struct hid_device *hid,
				unsigned char report_number, __u8 *buf,
				size_t count, unsigned char report_type,
				int reqtype)
{
	ipts_info_t *ipts = hid->driver_data;
	u32 fb_data_type;

	dev_dbg(&ipts->cldev->dev, "hid raw request => report %d, request %d\n",
						 (int)report_type, reqtype);

	if (report_type != HID_FEATURE_REPORT)
		return 0;

	switch (reqtype) {
		case HID_REQ_GET_REPORT:
			fb_data_type = TOUCH_FEEDBACK_DATA_TYPE_GET_FEATURES;
			break;
		case HID_REQ_SET_REPORT:
			fb_data_type = TOUCH_FEEDBACK_DATA_TYPE_SET_FEATURES;
			break;
		default:
			dev_err(&ipts->cldev->dev, "raw request not supprted: %d\n", reqtype);
			return -EIO;
	}

	return ipts_hid_send_hid2me_feedback(ipts, fb_data_type, buf, count);
}

static int ipts_hid_output_report(struct hid_device *hid,
					__u8 *buf, size_t count)
{
	ipts_info_t *ipts = hid->driver_data;
	u32 fb_data_type;

	dev_dbg(&ipts->cldev->dev, "hid output report\n");

	fb_data_type = TOUCH_FEEDBACK_DATA_TYPE_OUTPUT_REPORT;

	return ipts_hid_send_hid2me_feedback(ipts, fb_data_type, buf, count);
}

static struct hid_ll_driver ipts_hid_ll_driver = {
	.parse = ipts_hid_parse,
	.start = ipts_hid_start,
	.stop = ipts_hid_stop,
	.open = ipts_hid_open,
	.close = ipts_hid_close,
	.raw_request = ipts_hid_raw_request,
	.output_report = ipts_hid_output_report,
};

int ipts_hid_init(ipts_info_t *ipts)
{
	int ret = 0;
	struct hid_device *hid;

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		ret = PTR_ERR(hid);
		goto err_dev;
	}

	hid->driver_data = ipts;
	hid->ll_driver = &ipts_hid_ll_driver;
	hid->dev.parent = &ipts->cldev->dev;
	hid->bus = 0x44; // MEI
	hid->version = ipts->device_info.fw_rev;
	hid->vendor = ipts->device_info.vendor_id;
	hid->product = ipts->device_info.device_id;

	snprintf(hid->name, sizeof(hid->name), "%s", "Intel Precision Touch and Stylus");
	strlcpy(hid->phys, dev_name(&ipts->cldev->dev), sizeof(hid->phys));

	ret = hid_add_device(hid);
	if (ret) {
		if (ret != -ENODEV)
			dev_err(&ipts->cldev->dev, "can't add hid device: %d\n", ret);
		goto err_mem_free;
	}

	ipts->hid = hid;

	return 0;

err_mem_free:
	hid_destroy_device(hid);
err_dev:
	return ret;
}

void ipts_hid_fini(ipts_info_t *ipts)
{
	if (ipts->hid) {
		hid_destroy_device(ipts->hid);
		ipts->hid = NULL;
	}
}
