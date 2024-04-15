/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

enum ithc_data_type {
	ITHC_DATA_IGNORE,
	ITHC_DATA_RAW,
	ITHC_DATA_ERROR,
	ITHC_DATA_REPORT_DESCRIPTOR,
	ITHC_DATA_INPUT_REPORT,
	ITHC_DATA_OUTPUT_REPORT,
	ITHC_DATA_GET_FEATURE,
	ITHC_DATA_SET_FEATURE,
};

struct ithc_data {
	enum ithc_data_type type;
	u32 size;
	const void *data;
};

struct ithc_hid {
	struct hid_device *dev;
	bool parse_done;
	wait_queue_head_t wait_parse;
	wait_queue_head_t wait_get_feature;
	struct mutex get_feature_mutex;
	void *get_feature_buf;
	size_t get_feature_size;
};

int ithc_hid_init(struct ithc *ithc);
void ithc_hid_process_data(struct ithc *ithc, struct ithc_data *d);

