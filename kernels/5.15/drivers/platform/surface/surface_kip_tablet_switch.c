// SPDX-License-Identifier: GPL-2.0+
/*
 * Surface System Aggregator Module (SSAM) tablet mode switch via KIP
 * subsystem.
 *
 * Copyright (C) 2021 Maximilian Luz <luzmaximilian@gmail.com>
 */

#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/device.h>

#define SSAM_EVENT_KIP_CID_LID_STATE		0x1d

enum ssam_kip_lid_state {
	SSAM_KIP_LID_STATE_DISCONNECTED   = 0x01,
	SSAM_KIP_LID_STATE_CLOSED         = 0x02,
	SSAM_KIP_LID_STATE_LAPTOP         = 0x03,
	SSAM_KIP_LID_STATE_FOLDED_CANVAS  = 0x04,
	SSAM_KIP_LID_STATE_FOLDED_BACK    = 0x05,
};

struct ssam_kip_sw {
	struct ssam_device *sdev;

	enum ssam_kip_lid_state state;
	struct work_struct update_work;
	struct input_dev *mode_switch;

	struct ssam_event_notifier notif;
};

SSAM_DEFINE_SYNC_REQUEST_R(__ssam_kip_get_lid_state, u8, {
	.target_category = SSAM_SSH_TC_KIP,
	.target_id       = 0x01,
	.command_id      = 0x1d,
	.instance_id     = 0x00,
});

static int ssam_kip_get_lid_state(struct ssam_kip_sw *sw, enum ssam_kip_lid_state *state)
{
	int status;
	u8 raw;

	status = ssam_retry(__ssam_kip_get_lid_state, sw->sdev->ctrl, &raw);
	if (status < 0) {
		dev_err(&sw->sdev->dev, "failed to query KIP lid state: %d\n", status);
		return status;
	}

	*state = raw;
	return 0;
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ssam_kip_sw *sw = dev_get_drvdata(dev);
	const char *state;

	switch (sw->state) {
	case SSAM_KIP_LID_STATE_DISCONNECTED:
		state = "disconnected";
		break;

	case SSAM_KIP_LID_STATE_CLOSED:
		state = "closed";
		break;

	case SSAM_KIP_LID_STATE_LAPTOP:
		state = "laptop";
		break;

	case SSAM_KIP_LID_STATE_FOLDED_CANVAS:
		state = "folded-canvas";
		break;

	case SSAM_KIP_LID_STATE_FOLDED_BACK:
		state = "folded-back";
		break;

	default:
		state = "<unknown>";
		dev_warn(dev, "unknown KIP lid state: %d\n", sw->state);
		break;
	}

	return sysfs_emit(buf, "%s\n", state);
}
static DEVICE_ATTR_RO(state);

static struct attribute *ssam_kip_sw_attrs[] = {
	&dev_attr_state.attr,
	NULL,
};

static const struct attribute_group ssam_kip_sw_group = {
	.attrs = ssam_kip_sw_attrs,
};

static void ssam_kip_sw_update_workfn(struct work_struct *work)
{
	struct ssam_kip_sw *sw = container_of(work, struct ssam_kip_sw, update_work);
	enum ssam_kip_lid_state state;
	int tablet, status;

	status = ssam_kip_get_lid_state(sw, &state);
	if (status)
		return;

	if (sw->state == state)
		return;
	sw->state = state;

	/* Send SW_TABLET_MODE event. */
	tablet = state != SSAM_KIP_LID_STATE_LAPTOP;
	input_report_switch(sw->mode_switch, SW_TABLET_MODE, tablet);
	input_sync(sw->mode_switch);
}

static u32 ssam_kip_sw_notif(struct ssam_event_notifier *nf, const struct ssam_event *event)
{
	struct ssam_kip_sw *sw = container_of(nf, struct ssam_kip_sw, notif);

	if (event->command_id != SSAM_EVENT_KIP_CID_LID_STATE)
		return 0;	/* Return "unhandled". */

	if (event->length < 1) {
		dev_err(&sw->sdev->dev, "unexpected payload size: %u\n", event->length);
		return 0;
	}

	schedule_work(&sw->update_work);
	return SSAM_NOTIF_HANDLED;
}

static int __maybe_unused ssam_kip_sw_resume(struct device *dev)
{
	struct ssam_kip_sw *sw = dev_get_drvdata(dev);

	schedule_work(&sw->update_work);
	return 0;
}
static SIMPLE_DEV_PM_OPS(ssam_kip_sw_pm_ops, NULL, ssam_kip_sw_resume);

static int ssam_kip_sw_probe(struct ssam_device *sdev)
{
	struct ssam_kip_sw *sw;
	int tablet, status;

	sw = devm_kzalloc(&sdev->dev, sizeof(*sw), GFP_KERNEL);
	if (!sw)
		return -ENOMEM;

	sw->sdev = sdev;
	INIT_WORK(&sw->update_work, ssam_kip_sw_update_workfn);

	ssam_device_set_drvdata(sdev, sw);

	/* Get initial state. */
	status = ssam_kip_get_lid_state(sw, &sw->state);
	if (status)
		return status;

	/* Set up tablet mode switch. */
	sw->mode_switch = devm_input_allocate_device(&sdev->dev);
	if (!sw->mode_switch)
		return -ENOMEM;

	sw->mode_switch->name = "Microsoft Surface KIP Tablet Mode Switch";
	sw->mode_switch->phys = "ssam/01:0e:01:00:01/input0";
	sw->mode_switch->id.bustype = BUS_HOST;
	sw->mode_switch->dev.parent = &sdev->dev;

	tablet = sw->state != SSAM_KIP_LID_STATE_LAPTOP;
	input_set_capability(sw->mode_switch, EV_SW, SW_TABLET_MODE);
	input_report_switch(sw->mode_switch, SW_TABLET_MODE, tablet);

	status = input_register_device(sw->mode_switch);
	if (status)
		return status;

	/* Set up notifier. */
	sw->notif.base.priority = 0;
	sw->notif.base.fn = ssam_kip_sw_notif;
	sw->notif.event.reg = SSAM_EVENT_REGISTRY_SAM;
	sw->notif.event.id.target_category = SSAM_SSH_TC_KIP,
	sw->notif.event.id.instance = 0,
	sw->notif.event.mask = SSAM_EVENT_MASK_TARGET;
	sw->notif.event.flags = SSAM_EVENT_SEQUENCED;

	status = ssam_device_notifier_register(sdev, &sw->notif);
	if (status)
		return status;

	status = sysfs_create_group(&sdev->dev.kobj, &ssam_kip_sw_group);
	if (status)
		goto err;

	/* We might have missed events during setup, so check again. */
	schedule_work(&sw->update_work);
	return 0;

err:
	ssam_device_notifier_unregister(sdev, &sw->notif);
	cancel_work_sync(&sw->update_work);
	return status;
}

static void ssam_kip_sw_remove(struct ssam_device *sdev)
{
	struct ssam_kip_sw *sw = ssam_device_get_drvdata(sdev);

	sysfs_remove_group(&sdev->dev.kobj, &ssam_kip_sw_group);

	ssam_device_notifier_unregister(sdev, &sw->notif);
	cancel_work_sync(&sw->update_work);
}

static const struct ssam_device_id ssam_kip_sw_match[] = {
	{ SSAM_SDEV(KIP, 0x01, 0x00, 0x01) },
	{ },
};
MODULE_DEVICE_TABLE(ssam, ssam_kip_sw_match);

static struct ssam_device_driver ssam_kip_sw_driver = {
	.probe = ssam_kip_sw_probe,
	.remove = ssam_kip_sw_remove,
	.match_table = ssam_kip_sw_match,
	.driver = {
		.name = "surface_kip_tablet_mode_switch",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.pm = &ssam_kip_sw_pm_ops,
	},
};
module_ssam_device_driver(ssam_kip_sw_driver);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Tablet mode switch driver for Surface devices using KIP subsystem");
MODULE_LICENSE("GPL");
