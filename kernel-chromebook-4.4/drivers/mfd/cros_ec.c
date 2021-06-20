/*
 * ChromeOS EC multi-function device
 *
 * Copyright (C) 2012 Google, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * The ChromeOS EC multi function device is used to mux all the requests
 * to the EC device for its multiple features: keyboard controller,
 * battery charging and regulator control, firmware update.
 */

#include <linux/acpi.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/mfd/cros_ec.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/suspend.h>

#include <asm/unaligned.h>

#define CROS_EC_DEV_EC_INDEX 0
#define CROS_EC_DEV_PD_INDEX 1

#ifdef CONFIG_ACPI
#define ACPI_LID_DEVICE      "LID0"

static int ec_wake_gpe = -ENXIO;

/*
 * Installing a GPE handler to indicate to ACPI core
 * that this GPE should stay enabled for lid to work in
 * suspend to idle path
 */

static u32 cros_ec_gpe_handler(acpi_handle gpe_device,
			       u32 gpe_number, void *data)
{
	return ACPI_INTERRUPT_HANDLED | ACPI_REENABLE_GPE;
}

static int cros_ec_get_ec_wake_gpe(struct device *dev)
{

	/*
	 * EC contains cros_ec and LID0 devices
	 * LID0 device includes the EC_WAKE_GPE in PRW
	 * This code goes looks for LID0 acpi device.
	 */
	struct acpi_device *cros_acpi_dev = ACPI_COMPANION(dev);
	struct acpi_device *adev = NULL;
	acpi_handle handle;
	acpi_status status;

	if (!cros_acpi_dev || !cros_acpi_dev->parent ||
	   !cros_acpi_dev->parent->handle)
		return -ENXIO;

	status = acpi_get_handle(cros_acpi_dev->parent->handle,
			ACPI_LID_DEVICE, &handle);
	if (ACPI_FAILURE(status))
		return -ENXIO;

	acpi_bus_get_device(handle, &adev);
	if (!adev)
		return -ENXIO;

	return adev->wakeup.gpe_number;
}

static int cros_ec_install_handler(struct device *dev)
{
	acpi_status status;

	ec_wake_gpe = cros_ec_get_ec_wake_gpe(dev);

	if (ec_wake_gpe < 0)
		return ec_wake_gpe;

	status = acpi_install_gpe_handler(NULL, ec_wake_gpe,
			ACPI_GPE_EDGE_TRIGGERED,
			&cros_ec_gpe_handler, NULL);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	dev_info(dev, "Initialized, GPE = 0x%x\n", ec_wake_gpe);
	return 0;
}
#endif

static struct cros_ec_dev_platform ec_p = {
	.ec_name = CROS_EC_DEV_NAME,
	.cmd_offset = EC_CMD_PASSTHRU_OFFSET(CROS_EC_DEV_EC_INDEX),
};

static int get_next_event_xfer(struct cros_ec_device *ec_dev,
			       struct cros_ec_command *msg,
			       struct ec_response_get_next_event_v1 *event,
			       int version, uint32_t size)
{
	int ret;

	msg->command = EC_CMD_GET_NEXT_EVENT;
	msg->version = version;
	msg->insize = size;

	ret = cros_ec_cmd_xfer(ec_dev, msg);
	if (ret > 0) {
		ec_dev->event_size = ret - 1;
		ec_dev->event_data = *event;
	}

	return ret;
}

int cros_ec_get_next_event(struct cros_ec_device *ec_dev)
{
	struct {
		struct cros_ec_command msg;
		struct ec_response_get_next_event_v1 event;
	} __packed buf;
	struct cros_ec_command *msg = &buf.msg;
	struct ec_response_get_next_event_v1 *event = &buf.event;
	int cmd_version = ec_dev->mkbp_event_supported - 1;
	size_t response_size;
	int ret;

	BUILD_BUG_ON(sizeof(union ec_response_get_next_data_v1) != 16);

begin:
	memset(&buf, 0, sizeof(buf));

	if (ec_dev->suspended) {
		dev_dbg(ec_dev->dev, "Device suspended.\n");
		return -EHOSTDOWN;
	}

	if (cmd_version == 0)
		response_size = sizeof(struct ec_response_get_next_event);
	else
		response_size = sizeof(struct ec_response_get_next_event_v1);

	ret = get_next_event_xfer(ec_dev, msg, event, cmd_version,
				  response_size);

	/*
	 * If the result is EC_RES_INVALID_VERSION, the EC might have
	 * transitioned from RW to RO, so retry using an older version.
	 */
	if (msg->result == EC_RES_INVALID_VERSION && cmd_version > 0) {
		cmd_version = 0;
		goto begin;
	}

	return ret;
}
EXPORT_SYMBOL(cros_ec_get_next_event);

static int cros_ec_get_keyboard_state_event(struct cros_ec_device *ec_dev)
{
	struct {
		struct cros_ec_command msg;
		union ec_response_get_next_data_v1 data;
	} __packed buf;
	union ec_response_get_next_data_v1 *data = &buf.data;
	struct cros_ec_command *msg = &buf.msg;
	int ret;

	memset(&buf, 0, sizeof(buf));

	msg->version = 0;
	msg->command = EC_CMD_MKBP_STATE;
	msg->insize = sizeof(*data);

	ec_dev->event_data.event_type = EC_MKBP_EVENT_KEY_MATRIX;

	ret = cros_ec_cmd_xfer(ec_dev, msg);
	if (ret > 0)
		ec_dev->event_data.data = *data;

	ec_dev->event_size = ret;
	return ret;
}

u32 cros_ec_get_host_event(struct cros_ec_device *ec_dev)
{
	u32 host_event;

	BUG_ON(!ec_dev->mkbp_event_supported);
	if (ec_dev->event_data.event_type != EC_MKBP_EVENT_HOST_EVENT)
		return 0;
	if (ec_dev->event_size != sizeof(host_event)) {
		dev_warn(ec_dev->dev, "Invalid host event size\n");
		return 0;
	}

	host_event = get_unaligned_le32(&ec_dev->event_data.data.host_event);
	return host_event;
}
EXPORT_SYMBOL(cros_ec_get_host_event);

s64 cros_ec_get_time_ns(void)
{
	return ktime_get_boot_ns();
}
EXPORT_SYMBOL(cros_ec_get_time_ns);

static irqreturn_t ec_irq_handler(int irq, void *data) {
	struct cros_ec_device *ec_dev = data;

	ec_dev->last_event_time = cros_ec_get_time_ns();

	return IRQ_WAKE_THREAD;
}

static bool ec_handle_event(struct cros_ec_device *ec_dev)
{
	int wake_event = 1;
	u8 event_type;
	u32 host_event;
	int ret;
	bool ec_has_more_events = false;

	if (ec_dev->mkbp_event_supported) {
		ret = cros_ec_get_next_event(ec_dev);

		if (ret > 0) {
			event_type = ec_dev->event_data.event_type &
				EC_MKBP_EVENT_TYPE_MASK;
			ec_has_more_events =
				ec_dev->event_data.event_type &
					EC_MKBP_HAS_MORE_EVENTS;
			host_event = cros_ec_get_host_event(ec_dev);

			/*
			 * Sensor events need to be parsed by the sensor
			 * sub-device. Defer them, and don't report the
			 * wakeup here.
			 */
			if (event_type == EC_MKBP_EVENT_SENSOR_FIFO)
				wake_event = 0;
			/*
			 * Masked host-events should not count as
			 * wake events.
			 */
			else if (host_event &&
				!(host_event & ec_dev->host_event_wake_mask))
				wake_event = 0;
			/* Consider all other events as wake events. */
			else
				wake_event = 1;
		}
	} else {
		ret = cros_ec_get_keyboard_state_event(ec_dev);
	}

	if (device_may_wakeup(ec_dev->dev) && wake_event)
		pm_wakeup_event(ec_dev->dev, 0);

	if (ret > 0)
		blocking_notifier_call_chain(&ec_dev->event_notifier,
					0, ec_dev);

	return ec_has_more_events;

}

static irqreturn_t ec_irq_thread(int irq, void *data)
{
	struct cros_ec_device *ec_dev = data;
	bool ec_has_more_events;

	do {
		ec_has_more_events = ec_handle_event(ec_dev);
	} while (ec_has_more_events);

	return IRQ_HANDLED;
}

static int cros_ec_sleep_event(struct cros_ec_device *ec_dev, u8 sleep_event)
{
	struct {
		struct cros_ec_command msg;
		struct ec_params_host_sleep_event data;
	} __packed buf;
	struct cros_ec_command *msg = &buf.msg;
	struct ec_params_host_sleep_event *req = &buf.data;

	memset(&buf, 0, sizeof(buf));
	req->sleep_event = sleep_event;
	msg->command = EC_CMD_HOST_SLEEP_EVENT;
	msg->version = 0;
	msg->outsize = sizeof(*req);

	return cros_ec_cmd_xfer(ec_dev, msg);
}

static int cros_ec_ready_event(struct notifier_block *nb,
	unsigned long queued_during_suspend, void *_notify)
{
	struct cros_ec_device *ec_dev = container_of(nb, struct cros_ec_device,
						     notifier_ready);
	u32 host_event = cros_ec_get_host_event(ec_dev);

	if (host_event & EC_HOST_EVENT_MASK(EC_HOST_EVENT_INTERFACE_READY)) {
		mutex_lock(&ec_dev->lock);
		cros_ec_query_all(ec_dev);
		mutex_unlock(&ec_dev->lock);
		return NOTIFY_OK;
	} else {
		return NOTIFY_DONE;
	}
}

static struct cros_ec_dev_platform pd_p = {
	.ec_name = CROS_EC_DEV_PD_NAME,
	.cmd_offset = EC_CMD_PASSTHRU_OFFSET(CROS_EC_DEV_PD_INDEX),
};

static const struct mfd_cell ec_cell = {
	.name = "cros-ec-ctl",
	.platform_data = &ec_p,
	.pdata_size = sizeof(ec_p),
};

static const struct mfd_cell ec_pd_cell = {
	.name = "cros-ec-ctl",
	.platform_data = &pd_p,
	.pdata_size = sizeof(pd_p),
};

int cros_ec_register(struct cros_ec_device *ec_dev)
{
	struct device *dev = ec_dev->dev;
	int err = 0;

	BLOCKING_INIT_NOTIFIER_HEAD(&ec_dev->event_notifier);

	ec_dev->max_request = sizeof(struct ec_params_hello);
	ec_dev->max_response = sizeof(struct ec_response_get_protocol_info);
	ec_dev->max_passthru = 0;

	ec_dev->din = devm_kzalloc(dev, ec_dev->din_size, GFP_KERNEL);
	if (!ec_dev->din)
		return -ENOMEM;

	ec_dev->dout = devm_kzalloc(dev, ec_dev->dout_size, GFP_KERNEL);
	if (!ec_dev->dout)
		return -ENOMEM;

	mutex_init(&ec_dev->lock);

	err = cros_ec_query_all(ec_dev);
	if (err) {
		dev_err(dev, "Cannot identify the EC: error %d\n", err);
		return err;
	}

	if (ec_dev->irq > 0) {
		err = devm_request_threaded_irq(dev, ec_dev->irq,
				ec_irq_handler, ec_irq_thread,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"chromeos-ec", ec_dev);
		if (err) {
			dev_err(dev, "request irq %d: error %d\n",
				ec_dev->irq, err);
			return err;
		}
	}

	err = mfd_add_devices(ec_dev->dev, PLATFORM_DEVID_AUTO, &ec_cell, 1,
			      NULL, ec_dev->irq, NULL);
	if (err) {
		dev_err(dev,
			"Failed to register Embedded Controller subdevice %d\n",
			err);
		return err;
	}

	if (ec_dev->max_passthru) {
		/*
		 * Register a PD device as well on top of this device.
		 * We make the following assumptions:
		 * - behind an EC, we have a pd
		 * - only one device added.
		 * - the EC is responsive at init time (it is not true for a
		 *   sensor hub.
		 */
		err = mfd_add_devices(ec_dev->dev, PLATFORM_DEVID_AUTO,
				      &ec_pd_cell, 1, NULL, ec_dev->irq, NULL);
		if (err) {
			dev_err(dev,
				"Failed to register Power Delivery subdevice %d\n",
				err);
			return err;
		}
	}

	if (IS_ENABLED(CONFIG_OF) && dev->of_node) {
		err = of_platform_populate(dev->of_node, NULL, NULL, dev);
		if (err) {
			mfd_remove_devices(dev);
			dev_err(dev, "Failed to register sub-devices\n");
			return err;
		}
	}

	/*
	 * Clear sleep event - this will fail harmlessly on platforms that
	 * don't implement the sleep event host command.
	 */
	err = cros_ec_sleep_event(ec_dev, 0);
	if (err < 0)
		dev_dbg(ec_dev->dev, "Error %d clearing sleep event to ec",
			err);

	/* Register the notifier for EC_HOST_EVENT_INTERFACE_READY event. */
	ec_dev->notifier_ready.notifier_call = cros_ec_ready_event;
	err = blocking_notifier_chain_register(&ec_dev->event_notifier,
					       &ec_dev->notifier_ready);
	if (err < 0)
		dev_warn(ec_dev->dev, "Failed to register notifier\n");

	dev_info(dev, "Chrome EC device registered\n");

#ifdef CONFIG_ACPI
	cros_ec_install_handler(dev);
#endif
	return 0;
}
EXPORT_SYMBOL(cros_ec_register);

int cros_ec_remove(struct cros_ec_device *ec_dev)
{
	mfd_remove_devices(ec_dev->dev);
#ifdef CONFIG_ACPI
	if (ec_wake_gpe >= 0)
		if (ACPI_FAILURE(acpi_remove_gpe_handler(NULL, ec_wake_gpe,
					&cros_ec_gpe_handler)))
			pr_err("failed to remove gpe handler\n");

#endif
	return 0;
}
EXPORT_SYMBOL(cros_ec_remove);

#ifdef CONFIG_PM_SLEEP
static void cros_ec_clear_gpe(void) {
#ifdef CONFIG_ACPI
		/*
		 * Clearing the GPE status in S0ix flow,
		 * In S3, firmware will clear the status.
		 */
		if (ec_wake_gpe >= 0)
			acpi_clear_gpe(NULL, ec_wake_gpe);
#endif
}

int cros_ec_suspend(struct cros_ec_device *ec_dev)
{
	struct device *dev = ec_dev->dev;
	int ret;
	u8 sleep_event;

	if (!acpi_disabled && !pm_suspend_via_firmware()) {
		sleep_event = HOST_SLEEP_EVENT_S0IX_SUSPEND;
		cros_ec_clear_gpe();
	} else
		sleep_event = HOST_SLEEP_EVENT_S3_SUSPEND;

	ret = cros_ec_sleep_event(ec_dev, sleep_event);
	if (ret < 0)
		dev_dbg(ec_dev->dev, "Error %d sending suspend event to ec",
			ret);

	if (device_may_wakeup(dev))
		ec_dev->wake_enabled = !enable_irq_wake(ec_dev->irq);

	disable_irq(ec_dev->irq);
	ec_dev->was_wake_device = ec_dev->wake_enabled;
	ec_dev->suspended = true;

	return 0;
}
EXPORT_SYMBOL(cros_ec_suspend);

static void cros_ec_report_events_during_suspend(struct cros_ec_device *ec_dev)
{
	while (cros_ec_get_next_event(ec_dev) > 0)
		blocking_notifier_call_chain(&ec_dev->event_notifier,
					     1, ec_dev);
}

int cros_ec_resume(struct cros_ec_device *ec_dev)
{
	int ret;
	u8 sleep_event;

	if (!acpi_disabled && !pm_suspend_via_firmware()) {
		sleep_event = HOST_SLEEP_EVENT_S0IX_RESUME;
		cros_ec_clear_gpe();
	} else
		sleep_event = HOST_SLEEP_EVENT_S3_RESUME;

	ec_dev->suspended = false;
	enable_irq(ec_dev->irq);

	ret = cros_ec_sleep_event(ec_dev, sleep_event);
	if (ret < 0)
		dev_dbg(ec_dev->dev, "Error %d sending resume event to ec",
			ret);

	if (ec_dev->wake_enabled) {
		disable_irq_wake(ec_dev->irq);
		ec_dev->wake_enabled = 0;
	}
	/*
	 * Let the mfd devices know about events that occur during
	 * suspend. This way the clients know what to do with them.
	 */
	cros_ec_report_events_during_suspend(ec_dev);


	return 0;
}
EXPORT_SYMBOL(cros_ec_resume);

#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS EC core driver");
