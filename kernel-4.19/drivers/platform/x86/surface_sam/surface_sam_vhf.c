/*
 * Virtual HID Framwork (VHF) driver for input events via SAM.
 * Used for keyboard input events on the Surface Laptops.
 */

#include <linux/acpi.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include "surface_sam_ssh.h"


#define USB_VENDOR_ID_MICROSOFT		0x045e
#define USB_DEVICE_ID_MS_VHF		0xf001

#define VHF_INPUT_NAME			"Microsoft Virtual HID Framework Device"

/*
 * Request ID for VHF events. This value is based on the output of the Surface
 * EC and should not be changed.
 */
#define SAM_EVENT_VHF_RQID		0x0001
#define SAM_EVENT_VHF_TC		0x08


struct vhf_evtctx {
	struct device     *dev;
	struct hid_device *hid;
};

struct vhf_drvdata {
	struct vhf_evtctx event_ctx;
};


/*
 * These report descriptors have been extracted from a Surface Book 2.
 * They seems to be similar enough to be usable on the Surface Laptop.
 */
static const u8 vhf_hid_desc[] = {
	// keyboard descriptor (event command ID 0x03)
	0x05, 0x01,             /*  Usage Page (Desktop),                   */
	0x09, 0x06,             /*  Usage (Keyboard),                       */
	0xA1, 0x01,             /*  Collection (Application),               */
	0x85, 0x01,             /*      Report ID (1),                      */
	0x15, 0x00,             /*      Logical Minimum (0),                */
	0x25, 0x01,             /*      Logical Maximum (1),                */
	0x75, 0x01,             /*      Report Size (1),                    */
	0x95, 0x08,             /*      Report Count (8),                   */
	0x05, 0x07,             /*      Usage Page (Keyboard),              */
	0x19, 0xE0,             /*      Usage Minimum (KB Leftcontrol),     */
	0x29, 0xE7,             /*      Usage Maximum (KB Right GUI),       */
	0x81, 0x02,             /*      Input (Variable),                   */
	0x75, 0x08,             /*      Report Size (8),                    */
	0x95, 0x0A,             /*      Report Count (10),                  */
	0x19, 0x00,             /*      Usage Minimum (None),               */
	0x29, 0x91,             /*      Usage Maximum (KB LANG2),           */
	0x26, 0xFF, 0x00,       /*      Logical Maximum (255),              */
	0x81, 0x00,             /*      Input,                              */
	0x05, 0x0C,             /*      Usage Page (Consumer),              */
	0x0A, 0xC0, 0x02,       /*      Usage (02C0h),                      */
	0xA1, 0x02,             /*      Collection (Logical),               */
	0x1A, 0xC1, 0x02,       /*          Usage Minimum (02C1h),          */
	0x2A, 0xC6, 0x02,       /*          Usage Maximum (02C6h),          */
	0x95, 0x06,             /*          Report Count (6),               */
	0xB1, 0x03,             /*          Feature (Constant, Variable),   */
	0xC0,                   /*      End Collection,                     */
	0x05, 0x08,             /*      Usage Page (LED),                   */
	0x19, 0x01,             /*      Usage Minimum (01h),                */
	0x29, 0x03,             /*      Usage Maximum (03h),                */
	0x75, 0x01,             /*      Report Size (1),                    */
	0x95, 0x03,             /*      Report Count (3),                   */
	0x25, 0x01,             /*      Logical Maximum (1),                */
	0x91, 0x02,             /*      Output (Variable),                  */
	0x95, 0x05,             /*      Report Count (5),                   */
	0x91, 0x01,             /*      Output (Constant),                  */
	0xC0,                   /*  End Collection,                         */

	// media key descriptor (event command ID 0x04)
	0x05, 0x0C,             /*  Usage Page (Consumer),                  */
	0x09, 0x01,             /*  Usage (Consumer Control),               */
	0xA1, 0x01,             /*  Collection (Application),               */
	0x85, 0x03,             /*      Report ID (3),                      */
	0x75, 0x10,             /*      Report Size (16),                   */
	0x15, 0x00,             /*      Logical Minimum (0),                */
	0x26, 0xFF, 0x03,       /*      Logical Maximum (1023),             */
	0x19, 0x00,             /*      Usage Minimum (00h),                */
	0x2A, 0xFF, 0x03,       /*      Usage Maximum (03FFh),              */
	0x81, 0x00,             /*      Input,                              */
	0xC0,                   /*  End Collection,                         */
};


static int vhf_hid_start(struct hid_device *hid)
{
	hid_dbg(hid, "%s\n", __func__);
	return 0;
}

static void vhf_hid_stop(struct hid_device *hid)
{
	hid_dbg(hid, "%s\n", __func__);
}

static int vhf_hid_open(struct hid_device *hid)
{
	hid_dbg(hid, "%s\n", __func__);
	return 0;
}

static void vhf_hid_close(struct hid_device *hid)
{
	hid_dbg(hid, "%s\n", __func__);
}

static int vhf_hid_parse(struct hid_device *hid)
{
	return hid_parse_report(hid, (u8 *)vhf_hid_desc, ARRAY_SIZE(vhf_hid_desc));
}

static int vhf_hid_raw_request(struct hid_device *hid, unsigned char reportnum,
			       u8 *buf, size_t len, unsigned char rtype,
			       int reqtype)
{
	hid_dbg(hid, "%s\n", __func__);
	return 0;
}

static int vhf_hid_output_report(struct hid_device *hid, u8 *buf, size_t len)
{
	hid_dbg(hid, "%s\n", __func__);
	print_hex_dump_debug("report:", DUMP_PREFIX_OFFSET, 16, 1, buf, len, false);

	return len;
}

static struct hid_ll_driver vhf_hid_ll_driver = {
	.start         = vhf_hid_start,
	.stop          = vhf_hid_stop,
	.open          = vhf_hid_open,
	.close         = vhf_hid_close,
	.parse         = vhf_hid_parse,
	.raw_request   = vhf_hid_raw_request,
	.output_report = vhf_hid_output_report,
};


static struct hid_device *vhf_create_hid_device(struct platform_device *pdev)
{
	struct hid_device *hid;

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		return hid;
	}

	hid->dev.parent = &pdev->dev;

	hid->bus     = BUS_VIRTUAL;
	hid->vendor  = USB_VENDOR_ID_MICROSOFT;
	hid->product = USB_DEVICE_ID_MS_VHF;

	hid->ll_driver = &vhf_hid_ll_driver;

	sprintf(hid->name, "%s", VHF_INPUT_NAME);

	return hid;
}

static int vhf_event_handler(struct surface_sam_ssh_event *event, void *data)
{
	struct vhf_evtctx *ctx = (struct vhf_evtctx *)data;

	if (event->tc == 0x08 && (event->cid == 0x03 || event->cid == 0x04)) {
		return hid_input_report(ctx->hid, HID_INPUT_REPORT, event->pld, event->len, 1);
	}

	dev_warn(ctx->dev, "unsupported event (tc = %d, cid = %d)\n", event->tc, event->cid);
	return 0;
}

static int surface_sam_vhf_probe(struct platform_device *pdev)
{
	struct vhf_drvdata *drvdata;
	struct hid_device *hid;
	int status;

	// add device link to EC
	status = surface_sam_ssh_consumer_register(&pdev->dev);
	if (status) {
		return status == -ENXIO ? -EPROBE_DEFER : status;
	}

	drvdata = kzalloc(sizeof(struct vhf_drvdata), GFP_KERNEL);
	if (!drvdata) {
		return -ENOMEM;
	}

	hid = vhf_create_hid_device(pdev);
	if (IS_ERR(hid)) {
		status = PTR_ERR(hid);
		goto err_probe_hid;
	}

	status = hid_add_device(hid);
	if (status) {
		goto err_add_hid;
	}

	drvdata->event_ctx.dev = &pdev->dev;
	drvdata->event_ctx.hid = hid;

	platform_set_drvdata(pdev, drvdata);

	status = surface_sam_ssh_set_event_handler(
			SAM_EVENT_VHF_RQID,
	                vhf_event_handler,
			&drvdata->event_ctx);
	if (status) {
		goto err_add_hid;
	}

	status = surface_sam_ssh_enable_event_source(SAM_EVENT_VHF_TC, 0x01, SAM_EVENT_VHF_RQID);
	if (status) {
		goto err_event_source;
	}

	return 0;

err_event_source:
	surface_sam_ssh_remove_event_handler(SAM_EVENT_VHF_RQID);
err_add_hid:
	hid_destroy_device(hid);
	platform_set_drvdata(pdev, NULL);
err_probe_hid:
	kfree(drvdata);
	return status;
}

static int surface_sam_vhf_remove(struct platform_device *pdev)
{
	struct vhf_drvdata *drvdata = platform_get_drvdata(pdev);

	surface_sam_ssh_disable_event_source(SAM_EVENT_VHF_TC, 0x01, SAM_EVENT_VHF_RQID);
	surface_sam_ssh_remove_event_handler(SAM_EVENT_VHF_RQID);

	hid_destroy_device(drvdata->event_ctx.hid);
	kfree(drvdata);

	platform_set_drvdata(pdev, NULL);
	return 0;
}


static const struct acpi_device_id surface_sam_vhf_match[] = {
	{ "MSHW0096" },
	{ },
};
MODULE_DEVICE_TABLE(acpi, surface_sam_vhf_match);

static struct platform_driver surface_sam_vhf = {
	.probe = surface_sam_vhf_probe,
	.remove = surface_sam_vhf_remove,
	.driver = {
		.name = "surface_sam_vhf",
		.acpi_match_table = ACPI_PTR(surface_sam_vhf_match),
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_platform_driver(surface_sam_vhf);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Virtual HID Framework Driver for 5th Generation Surface Devices");
MODULE_LICENSE("GPL v2");
