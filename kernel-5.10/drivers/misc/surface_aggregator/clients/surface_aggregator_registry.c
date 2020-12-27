// SPDX-License-Identifier: GPL-2.0+
/*
 * Surface System Aggregator Module (SSAM) client device registry.
 *
 * Registry for non-platform/non-ACPI SSAM client devices, i.e. devices that
 * cannot be auto-detected. Provides device-hubs for these devices.
 *
 * Copyright (C) 2020 Maximilian Luz <luzmaximilian@gmail.com>
 */

#include <linux/acpi.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/device.h>


/* -- Device registry. ------------------------------------------------------ */

static const struct software_node ssam_node_root = {
	.name = "ssam_platform_hub",
};

static const struct software_node ssam_node_hub_main = {
	.name = "ssam:00:00:01:00:00",
	.parent = &ssam_node_root,
};

static const struct software_node ssam_node_hub_base = {
	.name = "ssam:00:00:02:00:00",
	.parent = &ssam_node_root,
};

static const struct software_node ssam_node_bat_ac = {
	.name = "ssam:01:02:01:01:01",
	.parent = &ssam_node_hub_main,
};

static const struct software_node ssam_node_bat_main = {
	.name = "ssam:01:02:01:01:00",
	.parent = &ssam_node_hub_main,
};

static const struct software_node ssam_node_bat_sb3base = {
	.name = "ssam:01:02:02:01:00",
	.parent = &ssam_node_hub_base,
};

static const struct software_node ssam_node_tmp_perf = {
	.name = "ssam:01:03:01:00:01",
	.parent = &ssam_node_hub_main,
};

static const struct software_node ssam_node_bas_dtx = {
	.name = "ssam:01:11:01:00:00",
	.parent = &ssam_node_hub_main,
};

static const struct software_node ssam_node_hid_main_keyboard = {
	.name = "ssam:01:15:02:01:00",
	.parent = &ssam_node_hub_main,
};

static const struct software_node ssam_node_hid_main_touchpad = {
	.name = "ssam:01:15:02:03:00",
	.parent = &ssam_node_hub_main,
};

static const struct software_node ssam_node_hid_main_iid5 = {
	.name = "ssam:01:15:02:05:00",
	.parent = &ssam_node_hub_main,
};

static const struct software_node ssam_node_hid_base_keyboard = {
	.name = "ssam:01:15:02:01:00",
	.parent = &ssam_node_hub_base,
};

static const struct software_node ssam_node_hid_base_touchpad = {
	.name = "ssam:01:15:02:03:00",
	.parent = &ssam_node_hub_base,
};

static const struct software_node ssam_node_hid_base_iid5 = {
	.name = "ssam:01:15:02:05:00",
	.parent = &ssam_node_hub_base,
};

static const struct software_node ssam_node_hid_base_iid6 = {
	.name = "ssam:01:15:02:06:00",
	.parent = &ssam_node_hub_base,
};

static const struct software_node *ssam_node_group_sb2[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_tmp_perf,
	NULL,
};

static const struct software_node *ssam_node_group_sb3[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_hub_base,
	&ssam_node_tmp_perf,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_bat_sb3base,
	&ssam_node_hid_base_keyboard,
	&ssam_node_hid_base_touchpad,
	&ssam_node_hid_base_iid5,
	&ssam_node_hid_base_iid6,
	&ssam_node_bas_dtx,
	NULL,
};

static const struct software_node *ssam_node_group_sl1[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_tmp_perf,
	NULL,
};

static const struct software_node *ssam_node_group_sl2[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_tmp_perf,
	NULL,
};

static const struct software_node *ssam_node_group_sl3[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_tmp_perf,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	&ssam_node_hid_main_keyboard,
	&ssam_node_hid_main_touchpad,
	&ssam_node_hid_main_iid5,
	NULL,
};

static const struct software_node *ssam_node_group_sp5[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_tmp_perf,
	NULL,
};

static const struct software_node *ssam_node_group_sp6[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_tmp_perf,
	NULL,
};

static const struct software_node *ssam_node_group_sp7[] = {
	&ssam_node_root,
	&ssam_node_hub_main,
	&ssam_node_tmp_perf,
	&ssam_node_bat_ac,
	&ssam_node_bat_main,
	NULL,
};


/* -- Device registry helper functions. ------------------------------------- */

static int ssam_uid_from_string(const char *str, struct ssam_device_uid *uid)
{
	u8 d, tc, tid, iid, fn;
	int n;

	n = sscanf(str, "ssam:%hhx:%hhx:%hhx:%hhx:%hhx", &d, &tc, &tid, &iid, &fn);
	if (n != 5)
		return -EINVAL;

	uid->domain = d;
	uid->category = tc;
	uid->target = tid;
	uid->instance = iid;
	uid->function = fn;

	return 0;
}

static int ssam_hub_remove_devices_fn(struct device *dev, void *data)
{
	if (!is_ssam_device(dev))
		return 0;

	ssam_device_remove(to_ssam_device(dev));
	return 0;
}

static void ssam_hub_remove_devices(struct device *parent)
{
	device_for_each_child_reverse(parent, NULL, ssam_hub_remove_devices_fn);
}

static int ssam_hub_add_device(struct device *parent,
			       struct ssam_controller *ctrl,
			       struct fwnode_handle *node)
{
	struct ssam_device_uid uid;
	struct ssam_device *sdev;
	int status;

	status = ssam_uid_from_string(fwnode_get_name(node), &uid);
	if (status)
		return -ENODEV;

	sdev = ssam_device_alloc(ctrl, uid);
	if (!sdev)
		return -ENOMEM;

	sdev->dev.parent = parent;
	sdev->dev.fwnode = node;

	status = ssam_device_add(sdev);
	if (status)
		ssam_device_put(sdev);

	return status;
}

static int ssam_hub_add_devices(struct device *parent,
				struct ssam_controller *ctrl,
				struct fwnode_handle *node)
{
	struct fwnode_handle *child;
	int status;

	fwnode_for_each_child_node(node, child) {
		status = ssam_hub_add_device(parent, ctrl, child);
		if (status && status != -ENODEV)
			goto err;
	}

	return 0;
err:
	ssam_hub_remove_devices(parent);
	return status;
}


/* -- SSAM main-hub driver. ------------------------------------------------- */

static int ssam_hub_probe(struct ssam_device *sdev)
{
	struct fwnode_handle *node = dev_fwnode(&sdev->dev);

	if (!node)
		return -ENODEV;

	return ssam_hub_add_devices(&sdev->dev, sdev->ctrl, node);
}

static void ssam_hub_remove(struct ssam_device *sdev)
{
	ssam_hub_remove_devices(&sdev->dev);
}

static const struct ssam_device_id ssam_hub_match[] = {
	{ SSAM_VDEV(HUB, 0x01, 0x00, 0x00) },
	{ },
};

static struct ssam_device_driver ssam_hub_driver = {
	.probe = ssam_hub_probe,
	.remove = ssam_hub_remove,
	.match_table = ssam_hub_match,
	.driver = {
		.name = "surface_aggregator_device_hub",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};


/* -- SSAM base-hub driver. ------------------------------------------------- */

enum ssam_base_hub_state {
	SSAM_BASE_HUB_UNINITIALIZED,
	SSAM_BASE_HUB_CONNECTED,
	SSAM_BASE_HUB_DISCONNECTED,
};

struct ssam_base_hub {
	struct ssam_device *sdev;

	struct mutex lock;
	enum ssam_base_hub_state state;

	struct ssam_event_notifier notif;
};

static SSAM_DEFINE_SYNC_REQUEST_R(ssam_bas_query_opmode, u8, {
	.target_category = SSAM_SSH_TC_BAS,
	.target_id       = 0x01,
	.command_id      = 0x0d,
	.instance_id     = 0x00,
});

#define SSAM_BAS_OPMODE_TABLET		0x00
#define SSAM_EVENT_BAS_CID_CONNECTION	0x0c

static int ssam_base_hub_query_state(struct ssam_device *sdev,
				     enum ssam_base_hub_state *state)
{
	u8 opmode;
	int status;

	status = ssam_retry(ssam_bas_query_opmode, sdev->ctrl, &opmode);
	if (status < 0) {
		dev_err(&sdev->dev, "failed to query base state: %d\n", status);
		return status;
	}

	if (opmode != SSAM_BAS_OPMODE_TABLET)
		*state = SSAM_BASE_HUB_CONNECTED;
	else
		*state = SSAM_BASE_HUB_DISCONNECTED;

	return 0;
}

static ssize_t ssam_base_hub_state_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ssam_device *sdev = to_ssam_device(dev);
	struct ssam_base_hub *hub = ssam_device_get_drvdata(sdev);
	bool connected;

	mutex_lock(&hub->lock);
	connected = hub->state == SSAM_BASE_HUB_CONNECTED;
	mutex_unlock(&hub->lock);

	// FIXME: we should use sysfs_emit here, but that's not available on < 5.10
	return scnprintf(buf, PAGE_SIZE, "%d\n", connected);
}

static struct device_attribute ssam_base_hub_attr_state =
	__ATTR(state, 0444, ssam_base_hub_state_show, NULL);

static struct attribute *ssam_base_hub_attrs[] = {
	&ssam_base_hub_attr_state.attr,
	NULL,
};

const struct attribute_group ssam_base_hub_group = {
	.attrs = ssam_base_hub_attrs,
};

static int ssam_base_hub_update(struct ssam_device *sdev,
				enum ssam_base_hub_state new)
{
	struct ssam_base_hub *hub = ssam_device_get_drvdata(sdev);
	struct fwnode_handle *node = dev_fwnode(&sdev->dev);
	int status = 0;

	mutex_lock(&hub->lock);
	if (hub->state == new) {
		mutex_unlock(&hub->lock);
		return 0;
	}
	hub->state = new;

	if (hub->state == SSAM_BASE_HUB_CONNECTED)
		status = ssam_hub_add_devices(&sdev->dev, sdev->ctrl, node);

	if (hub->state != SSAM_BASE_HUB_CONNECTED || status)
		ssam_hub_remove_devices(&sdev->dev);

	mutex_unlock(&hub->lock);

	if (status) {
		dev_err(&sdev->dev, "failed to update base-hub devices: %d\n",
			status);
	}

	return status;
}

static u32 ssam_base_hub_notif(struct ssam_event_notifier *nf,
			       const struct ssam_event *event)
{
	struct ssam_base_hub *hub;
	struct ssam_device *sdev;
	enum ssam_base_hub_state new;

	hub = container_of(nf, struct ssam_base_hub, notif);
	sdev = hub->sdev;

	if (event->command_id != SSAM_EVENT_BAS_CID_CONNECTION)
		return 0;

	if (event->length < 1) {
		dev_err(&sdev->dev, "unexpected payload size: %u\n",
			event->length);
		return 0;
	}

	if (event->data[0])
		new = SSAM_BASE_HUB_CONNECTED;
	else
		new = SSAM_BASE_HUB_DISCONNECTED;

	ssam_base_hub_update(sdev, new);

	/*
	 * Do not return SSAM_NOTIF_HANDLED: The event should be picked up and
	 * consumed by the detachment system driver. We're just a (more or less)
	 * silent observer.
	 */
	return 0;
}

static int __maybe_unused ssam_base_hub_resume(struct device *dev)
{
	struct ssam_device *sdev = to_ssam_device(dev);
	enum ssam_base_hub_state state;
	int status;

	status = ssam_base_hub_query_state(sdev, &state);
	if (status)
		return status;

	return ssam_base_hub_update(sdev, state);
}
static SIMPLE_DEV_PM_OPS(ssam_base_hub_pm_ops, NULL, ssam_base_hub_resume);

static int ssam_base_hub_probe(struct ssam_device *sdev)
{
	enum ssam_base_hub_state state;
	struct ssam_base_hub *hub;
	int status;

	hub = devm_kzalloc(&sdev->dev, sizeof(*hub), GFP_KERNEL);
	if (!hub)
		return -ENOMEM;

	mutex_init(&hub->lock);

	hub->sdev = sdev;
	hub->state = SSAM_BASE_HUB_UNINITIALIZED;

	hub->notif.base.priority = 1000;  /* This notifier should run first. */
	hub->notif.base.fn = ssam_base_hub_notif;
	hub->notif.event.reg = SSAM_EVENT_REGISTRY_SAM;
	hub->notif.event.id.target_category = SSAM_SSH_TC_BAS,
	hub->notif.event.id.instance = 0,
	hub->notif.event.mask = SSAM_EVENT_MASK_NONE;
	hub->notif.event.flags = SSAM_EVENT_SEQUENCED;

	status = ssam_notifier_register(sdev->ctrl, &hub->notif);
	if (status)
		return status;

	ssam_device_set_drvdata(sdev, hub);

	status = ssam_base_hub_query_state(sdev, &state);
	if (status) {
		ssam_notifier_unregister(sdev->ctrl, &hub->notif);
		return status;
	}

	status = ssam_base_hub_update(sdev, state);
	if (status) {
		ssam_notifier_unregister(sdev->ctrl, &hub->notif);
		return status;
	}

	status = sysfs_create_group(&sdev->dev.kobj, &ssam_base_hub_group);
	if (status) {
		ssam_notifier_unregister(sdev->ctrl, &hub->notif);
		ssam_hub_remove_devices(&sdev->dev);
	}

	return status;
}

static void ssam_base_hub_remove(struct ssam_device *sdev)
{
	struct ssam_base_hub *hub = ssam_device_get_drvdata(sdev);

	sysfs_remove_group(&sdev->dev.kobj, &ssam_base_hub_group);

	ssam_notifier_unregister(sdev->ctrl, &hub->notif);
	ssam_hub_remove_devices(&sdev->dev);

	mutex_destroy(&hub->lock);
}

static const struct ssam_device_id ssam_base_hub_match[] = {
	{ SSAM_VDEV(HUB, 0x02, 0x00, 0x00) },
	{ },
};

static struct ssam_device_driver ssam_base_hub_driver = {
	.probe = ssam_base_hub_probe,
	.remove = ssam_base_hub_remove,
	.match_table = ssam_base_hub_match,
	.driver = {
		.name = "surface_aggregator_base_hub",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.pm = &ssam_base_hub_pm_ops,
	},
};


/* -- SSAM platform/meta-hub driver. ---------------------------------------- */

static const struct acpi_device_id ssam_platform_hub_match[] = {
	/* Surface Pro 4, 5, and 6 */
	{ "MSHW0081", (unsigned long)ssam_node_group_sp5 },

	/* Surface Pro 6 (OMBR >= 0x10) */
	{ "MSHW0111", (unsigned long)ssam_node_group_sp6 },

	/* Surface Pro 7 */
	{ "MSHW0116", (unsigned long)ssam_node_group_sp7 },

	/* Surface Book 2 */
	{ "MSHW0107", (unsigned long)ssam_node_group_sb2 },

	/* Surface Book 3 */
	{ "MSHW0117", (unsigned long)ssam_node_group_sb3 },

	/* Surface Laptop 1 */
	{ "MSHW0086", (unsigned long)ssam_node_group_sl1 },

	/* Surface Laptop 2 */
	{ "MSHW0112", (unsigned long)ssam_node_group_sl2 },

	/* Surface Laptop 3 (13", Intel) */
	{ "MSHW0114", (unsigned long)ssam_node_group_sl3 },

	/* Surface Laptop 3 (15", AMD) */
	{ "MSHW0110", (unsigned long)ssam_node_group_sl3 },

	{ },
};
MODULE_DEVICE_TABLE(acpi, ssam_platform_hub_match);

static int ssam_platform_hub_probe(struct platform_device *pdev)
{
	const struct software_node **nodes;
	struct ssam_controller *ctrl;
	struct fwnode_handle *root;
	int status;

	nodes = (const struct software_node **)acpi_device_get_match_data(&pdev->dev);
	if (!nodes)
		return -ENODEV;

	/*
	 * As we're adding the SSAM client devices as children under this device
	 * and not the SSAM controller, we need to add a device link to the
	 * controller to ensure that we remove all of our devices before the
	 * controller is removed. This also guarantees proper ordering for
	 * suspend/resume of the devices on this hub.
	 */
	ctrl = ssam_client_bind(&pdev->dev);
	if (IS_ERR(ctrl))
		return PTR_ERR(ctrl) == -ENODEV ? -EPROBE_DEFER : PTR_ERR(ctrl);

	status = software_node_register_node_group(nodes);
	if (status)
		return status;

	root = software_node_fwnode(&ssam_node_root);
	if (!root)
		return -ENOENT;

	set_secondary_fwnode(&pdev->dev, root);

	status = ssam_hub_add_devices(&pdev->dev, ctrl, root);
	if (status) {
		software_node_unregister_node_group(nodes);
		return status;
	}

	platform_set_drvdata(pdev, nodes);
	return 0;
}

static int ssam_platform_hub_remove(struct platform_device *pdev)
{
	const struct software_node **nodes = platform_get_drvdata(pdev);

	ssam_hub_remove_devices(&pdev->dev);
	set_secondary_fwnode(&pdev->dev, NULL);
	software_node_unregister_node_group(nodes);
	return 0;
}

static struct platform_driver ssam_platform_hub_driver = {
	.probe = ssam_platform_hub_probe,
	.remove = ssam_platform_hub_remove,
	.driver = {
		.name = "surface_aggregator_platform_hub",
		.acpi_match_table = ssam_platform_hub_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};


/* -- Module initialization. ------------------------------------------------ */

static int __init ssam_device_hub_init(void)
{
	int status;

	status = platform_driver_register(&ssam_platform_hub_driver);
	if (status)
		goto err_platform;

	status = ssam_device_driver_register(&ssam_hub_driver);
	if (status)
		goto err_main;

	status = ssam_device_driver_register(&ssam_base_hub_driver);
	if (status)
		goto err_base;

	return 0;

err_base:
	ssam_device_driver_unregister(&ssam_hub_driver);
err_main:
	platform_driver_unregister(&ssam_platform_hub_driver);
err_platform:
	return status;
}
module_init(ssam_device_hub_init);

static void __exit ssam_device_hub_exit(void)
{
	ssam_device_driver_unregister(&ssam_base_hub_driver);
	ssam_device_driver_unregister(&ssam_hub_driver);
	platform_driver_unregister(&ssam_platform_hub_driver);
}
module_exit(ssam_device_hub_exit);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Device-registry for Surface System Aggregator Module");
MODULE_LICENSE("GPL");
