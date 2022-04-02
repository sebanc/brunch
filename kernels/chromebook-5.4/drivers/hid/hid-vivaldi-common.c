// SPDX-License-Identifier: GPL-2.0
/*
 * Helpers for ChromeOS HID Vivaldi keyboards
 *
 * Copyright (C) 2022 Google, Inc
 */

#include <linux/export.h>
#include <linux/hid.h>
#include <linux/input/vivaldi-fmap.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#define HID_VD_FN_ROW_PHYSMAP 0x00000001
#define HID_USAGE_FN_ROW_PHYSMAP (HID_UP_GOOGLEVENDOR | HID_VD_FN_ROW_PHYSMAP)

/**
 * vivaldi_hid_feature_mapping - Fill out vivaldi keymap data exposed via HID
 * @data: The vivaldi function keymap
 * @hdev: HID device to parse
 * @field: HID field to parse
 * @usage: HID usage to parse
 */
void vivaldi_hid_feature_mapping(struct vivaldi_data *data,
				 struct hid_device *hdev,
				 struct hid_field *field,
				 struct hid_usage *usage)
{
	struct hid_report *report = field->report;
	int fn_key;
	int ret;
	u32 report_len;
	u8 *report_data, *buf;

	if (field->logical != HID_USAGE_FN_ROW_PHYSMAP ||
	    (usage->hid & HID_USAGE_PAGE) != HID_UP_ORDINAL)
		return;

	fn_key = (usage->hid & HID_USAGE);
	if (fn_key < VIVALDI_MIN_FN_ROW_KEY || fn_key > VIVALDI_MAX_FN_ROW_KEY)
		return;
	if (fn_key > data->num_function_row_keys)
		data->num_function_row_keys = fn_key;

	report_data = buf = hid_alloc_report_buf(report, GFP_KERNEL);
	if (!report_data)
		return;

	report_len = hid_report_len(report);
	if (!report->id) {
		/*
		 * hid_hw_raw_request() will stuff report ID (which will be 0)
		 * into the first byte of the buffer even for unnumbered
		 * reports, so we need to account for this to avoid getting
		 * -EOVERFLOW in return.
		 * Note that hid_alloc_report_buf() adds 7 bytes to the size
		 * so we can safely say that we have space for an extra byte.
		 */
		report_len++;
	}

	ret = hid_hw_raw_request(hdev, report->id, report_data,
				 report_len, HID_FEATURE_REPORT,
				 HID_REQ_GET_REPORT);
	if (ret < 0) {
		dev_warn(&hdev->dev, "failed to fetch feature %d\n",
			 field->report->id);
		goto out;
	}

	if (!report->id) {
		/*
		 * Undo the damage from hid_hw_raw_request() for unnumbered
		 * reports.
		 */
		report_data++;
		report_len--;
	}

	ret = hid_report_raw_event(hdev, HID_FEATURE_REPORT, report_data,
				   report_len, 0);
	if (ret) {
		dev_warn(&hdev->dev, "failed to report feature %d\n",
			 field->report->id);
		goto out;
	}

	data->function_row_physmap[fn_key - VIVALDI_MIN_FN_ROW_KEY] =
	    field->value[usage->usage_index];

out:
	kfree(buf);
}
EXPORT_SYMBOL_GPL(vivaldi_hid_feature_mapping);

MODULE_LICENSE("GPL");
