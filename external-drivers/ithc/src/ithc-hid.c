// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause

#include "ithc.h"

static int ithc_hid_start(struct hid_device *hdev) { return 0; }
static void ithc_hid_stop(struct hid_device *hdev) { }
static int ithc_hid_open(struct hid_device *hdev) { return 0; }
static void ithc_hid_close(struct hid_device *hdev) { }

static int ithc_hid_parse(struct hid_device *hdev)
{
	struct ithc *ithc = hdev->driver_data;
	const struct ithc_data get_report_desc = { .type = ITHC_DATA_REPORT_DESCRIPTOR };
	WRITE_ONCE(ithc->hid.parse_done, false);
	for (int retries = 0; ; retries++) {
		ithc_log_regs(ithc);
		CHECK_RET(ithc_dma_tx, ithc, &get_report_desc);
		if (wait_event_timeout(ithc->hid.wait_parse, READ_ONCE(ithc->hid.parse_done),
				msecs_to_jiffies(200))) {
			ithc_log_regs(ithc);
			return 0;
		}
		if (retries > 5) {
			ithc_log_regs(ithc);
			pci_err(ithc->pci, "failed to read report descriptor\n");
			return -ETIMEDOUT;
		}
		pci_warn(ithc->pci, "failed to read report descriptor, retrying\n");
	}
}

static int ithc_hid_raw_request(struct hid_device *hdev, unsigned char reportnum, __u8 *buf,
	size_t len, unsigned char rtype, int reqtype)
{
	struct ithc *ithc = hdev->driver_data;
	if (!buf || !len)
		return -EINVAL;

	struct ithc_data d = { .size = len, .data = buf };
	buf[0] = reportnum;

	if (rtype == HID_OUTPUT_REPORT && reqtype == HID_REQ_SET_REPORT) {
		d.type = ITHC_DATA_OUTPUT_REPORT;
		CHECK_RET(ithc_dma_tx, ithc, &d);
		return 0;
	}

	if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_SET_REPORT) {
		d.type = ITHC_DATA_SET_FEATURE;
		CHECK_RET(ithc_dma_tx, ithc, &d);
		return 0;
	}

	if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_GET_REPORT) {
		d.type = ITHC_DATA_GET_FEATURE;
		d.data = &reportnum;
		d.size = 1;

		// Prepare for response.
		mutex_lock(&ithc->hid.get_feature_mutex);
		ithc->hid.get_feature_buf = buf;
		ithc->hid.get_feature_size = len;
		mutex_unlock(&ithc->hid.get_feature_mutex);

		// Transmit 'get feature' request.
		int r = CHECK(ithc_dma_tx, ithc, &d);
		if (!r) {
			r = wait_event_interruptible_timeout(ithc->hid.wait_get_feature,
				!ithc->hid.get_feature_buf, msecs_to_jiffies(1000));
			if (!r)
				r = -ETIMEDOUT;
			else if (r < 0)
				r = -EINTR;
			else
				r = 0;
		}

		// If everything went ok, the buffer has been filled with the response data.
		// Return the response size.
		mutex_lock(&ithc->hid.get_feature_mutex);
		ithc->hid.get_feature_buf = NULL;
		if (!r)
			r = ithc->hid.get_feature_size;
		mutex_unlock(&ithc->hid.get_feature_mutex);
		return r;
	}

	pci_err(ithc->pci, "unhandled hid request %i %i for report id %i\n",
		rtype, reqtype, reportnum);
	return -EINVAL;
}

// FIXME hid_input_report()/hid_parse_report() currently don't take const buffers, so we have to
// cast away the const to avoid a compiler warning...
#define NOCONST(x) ((void *)x)

void ithc_hid_process_data(struct ithc *ithc, struct ithc_data *d)
{
	WARN_ON(!ithc->hid.dev);
	if (!ithc->hid.dev)
		return;

	switch (d->type) {

	case ITHC_DATA_IGNORE:
		return;

	case ITHC_DATA_ERROR:
		CHECK(ithc_reset, ithc);
		return;

	case ITHC_DATA_REPORT_DESCRIPTOR:
		// Response to the report descriptor request sent by ithc_hid_parse().
		CHECK(hid_parse_report, ithc->hid.dev, NOCONST(d->data), d->size);
		WRITE_ONCE(ithc->hid.parse_done, true);
		wake_up(&ithc->hid.wait_parse);
		return;

	case ITHC_DATA_INPUT_REPORT:
	{
		// Standard HID input report.
		int r = hid_input_report(ithc->hid.dev, HID_INPUT_REPORT, NOCONST(d->data), d->size, 1);
		if (r < 0) {
			pci_warn(ithc->pci, "hid_input_report failed with %i (size %u, report ID 0x%02x)\n",
				r, d->size, d->size ? *(u8 *)d->data : 0);
			print_hex_dump_debug(DEVNAME " report: ", DUMP_PREFIX_OFFSET, 32, 1,
				d->data, min(d->size, 0x400u), 0);
		}
		return;
	}

	case ITHC_DATA_GET_FEATURE:
	{
		// Response to a 'get feature' request sent by ithc_hid_raw_request().
		bool done = false;
		mutex_lock(&ithc->hid.get_feature_mutex);
		if (ithc->hid.get_feature_buf) {
			if (d->size < ithc->hid.get_feature_size)
				ithc->hid.get_feature_size = d->size;
			memcpy(ithc->hid.get_feature_buf, d->data, ithc->hid.get_feature_size);
			ithc->hid.get_feature_buf = NULL;
			done = true;
		}
		mutex_unlock(&ithc->hid.get_feature_mutex);
		if (done) {
			wake_up(&ithc->hid.wait_get_feature);
		} else {
			// Received data without a matching request, or the request already
			// timed out. (XXX What's the correct thing to do here?)
			CHECK(hid_input_report, ithc->hid.dev, HID_FEATURE_REPORT,
				NOCONST(d->data), d->size, 1);
		}
		return;
	}

	default:
		pci_err(ithc->pci, "unhandled data type %i\n", d->type);
		return;
	}
}

static struct hid_ll_driver ithc_ll_driver = {
	.start = ithc_hid_start,
	.stop = ithc_hid_stop,
	.open = ithc_hid_open,
	.close = ithc_hid_close,
	.parse = ithc_hid_parse,
	.raw_request = ithc_hid_raw_request,
};

static void ithc_hid_devres_release(struct device *dev, void *res)
{
	struct hid_device **hidm = res;
	if (*hidm)
		hid_destroy_device(*hidm);
}

int ithc_hid_init(struct ithc *ithc)
{
	struct hid_device **hidm = devres_alloc(ithc_hid_devres_release, sizeof(*hidm), GFP_KERNEL);
	if (!hidm)
		return -ENOMEM;
	devres_add(&ithc->pci->dev, hidm);
	struct hid_device *hid = hid_allocate_device();
	if (IS_ERR(hid))
		return PTR_ERR(hid);
	*hidm = hid;

	strscpy(hid->name, DEVFULLNAME, sizeof(hid->name));
	strscpy(hid->phys, ithc->phys, sizeof(hid->phys));
	hid->ll_driver = &ithc_ll_driver;
	hid->bus = BUS_PCI;
	hid->vendor = ithc->vendor_id;
	hid->product = ithc->product_id;
	hid->version = 0x100;
	hid->dev.parent = &ithc->pci->dev;
	hid->driver_data = ithc;

	ithc->hid.dev = hid;

	init_waitqueue_head(&ithc->hid.wait_parse);
	init_waitqueue_head(&ithc->hid.wait_get_feature);
	mutex_init(&ithc->hid.get_feature_mutex);

	return 0;
}

