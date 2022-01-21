/*
 * Copyright (c) 2014 Google, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Power supply driver for ChromeOS EC based USB PD Charger.
 */

#include <linux/fs.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mfd/cros_ec.h>
#include <linux/mfd/cros_ec_commands.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/rtc.h>
#include <linux/slab.h>

#include <linux/mfd/cros_ec.h>
#include <linux/mfd/cros_ec_commands.h>

#define CROS_USB_PD_MAX_LOG_ENTRIES	30

#define CROS_USB_PD_LOG_UPDATE_DELAY msecs_to_jiffies(60000)
#define CROS_USB_PD_CACHE_UPDATE_DELAY msecs_to_jiffies(500)

/* Buffer + macro for building PDLOG string */
#define BUF_SIZE 80
#define APPEND_STRING(buf, len, str, ...) ((len) += \
	snprintf((buf) + (len), max(BUF_SIZE - (len), 0), (str), ##__VA_ARGS__))

#define CHARGER_DIR_NAME		"CROS_USB_PD_CHARGER%d"
#define CHARGER_DIR_NAME_LENGTH		sizeof(CHARGER_DIR_NAME)
#define DRV_NAME "cros-usb-pd-charger"

#define MANUFACTURER_MODEL_LENGTH	32

struct port_data {
	int port_number;
	char name[CHARGER_DIR_NAME_LENGTH];
	char manufacturer[MANUFACTURER_MODEL_LENGTH];
	char model_name[MANUFACTURER_MODEL_LENGTH];
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	int psy_type;
	int psy_online;
	int psy_status;
	int psy_current_max;
	int psy_voltage_max_design;
	int psy_voltage_now;
	int psy_power_max;
	struct charger_data *charger;
	unsigned long last_update;
};

struct charger_data {
	struct device *dev;
	struct cros_ec_dev *ec_dev;
	struct cros_ec_device *ec_device;
	int num_charger_ports;
	int num_registered_psy;
	struct port_data *ports[EC_USB_PD_MAX_PORTS];
	struct delayed_work log_work;
	struct workqueue_struct *log_workqueue;
	struct notifier_block notifier;
	bool suspended;
};

#define EC_MAX_IN_SIZE EC_PROTO2_MAX_REQUEST_SIZE
#define EC_MAX_OUT_SIZE EC_PROTO2_MAX_RESPONSE_SIZE
uint8_t ec_inbuf[EC_MAX_IN_SIZE];
uint8_t ec_outbuf[EC_MAX_OUT_SIZE];

static enum power_supply_property cros_usb_pd_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

/* Current external voltage/current limit in mV/mA. Default to none. */
static uint16_t ext_voltage_lim = EC_POWER_LIMIT_NONE;
static uint16_t ext_current_lim = EC_POWER_LIMIT_NONE;

static int ec_command(struct charger_data *charger, int version, int command,
		      uint8_t *outdata, int outsize, uint8_t *indata,
		      int insize)
{
	struct cros_ec_device *ec_device = charger->ec_device;
	struct cros_ec_dev *ec_dev = charger->ec_dev;
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + max(insize, outsize), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->version = version;
	msg->command = ec_dev->cmd_offset + command;
	msg->outsize = outsize;
	msg->insize = insize;

	if (outsize)
		memcpy(msg->data, outdata, outsize);

	ret = cros_ec_cmd_xfer_status(ec_device, msg);
	if (ret > 0 && insize)
		memcpy(indata, msg->data, insize);

	kfree(msg);
	return ret;
}

static int set_ec_usb_pd_override_ports(struct charger_data *charger,
					int port_num)
{
	struct device *dev = charger->dev;
	struct ec_params_charge_port_override req;
	int ret;

	req.override_port = port_num;

	ret = ec_command(charger, 0, EC_CMD_PD_CHARGE_PORT_OVERRIDE,
			 (uint8_t *)&req, sizeof(req),
			 NULL, 0);
	if (ret < 0) {
		dev_info(dev, "Port Override command returned 0x%x\n", ret);
		return -EINVAL;
	}

	return 0;
}

static int get_ec_num_ports(struct charger_data *charger, int *num_ports)
{
	struct device *dev = charger->dev;
	struct ec_response_usb_pd_ports *resp =
	    (struct ec_response_usb_pd_ports *)ec_inbuf;
	int ret;

	*num_ports = 0;
	ret = ec_command(charger, 0, EC_CMD_USB_PD_PORTS,
			 NULL, 0, ec_inbuf, EC_MAX_IN_SIZE);
	if (ret < 0) {
		dev_err(dev, "Unable to query PD ports (err:0x%x)\n", ret);
		return ret;
	}
	*num_ports = resp->num_ports;
	dev_dbg(dev, "Num ports = %d\n", *num_ports);

	return 0;
}

static int get_ec_usb_pd_discovery_info(struct port_data *port)
{
	struct charger_data *charger = port->charger;
	struct device *dev = charger->dev;
	struct ec_params_usb_pd_info_request req;
	struct ec_params_usb_pd_discovery_entry resp;
	int ret;

	req.port = port->port_number;

	ret = ec_command(charger, 0, EC_CMD_USB_PD_DISCOVERY,
			 (uint8_t *)&req, sizeof(req),
			 (uint8_t *)&resp, sizeof(resp));
	if (ret < 0) {
		dev_err(dev, "Unable to query Discovery info (err:0x%x)\n",
			 ret);
		return -EINVAL;
	}

	dev_dbg(dev, "Port %d: VID = 0x%x, PID=0x%x, PTYPE=0x%x\n",
		port->port_number, resp.vid, resp.pid, resp.ptype);

	snprintf(port->manufacturer, MANUFACTURER_MODEL_LENGTH, "%x", resp.vid);
	snprintf(port->model_name, MANUFACTURER_MODEL_LENGTH, "%x", resp.pid);

	return 0;
}

static int get_ec_usb_pd_power_info(struct port_data *port)
{
	struct charger_data *charger = port->charger;
	struct device *dev = charger->dev;
	struct ec_params_usb_pd_power_info req;
	struct ec_response_usb_pd_power_info resp;
	char role_str[80];
	int ret, last_psy_status, last_psy_type;

	req.port = port->port_number;
	ret = ec_command(charger, 0, EC_CMD_USB_PD_POWER_INFO,
			 (uint8_t *)&req, sizeof(req),
			 (uint8_t *)&resp, sizeof(resp));
	if (ret < 0) {
		dev_err(dev, "Unable to query PD power info (err:0x%x)\n", ret);
		return -EINVAL;
	}

	last_psy_status = port->psy_status;
	last_psy_type = port->psy_type;

	switch (resp.role) {
	case USB_PD_PORT_POWER_DISCONNECTED:
		snprintf(role_str, sizeof(role_str), "%s", "DISCONNECTED");
		port->psy_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		port->psy_online = 0;
		break;
	case USB_PD_PORT_POWER_SOURCE:
		snprintf(role_str, sizeof(role_str), "%s", "SOURCE");
		port->psy_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		port->psy_online = 0;
		break;
	case USB_PD_PORT_POWER_SINK:
		snprintf(role_str, sizeof(role_str), "%s", "SINK");
		port->psy_status = POWER_SUPPLY_STATUS_CHARGING;
		port->psy_online = 1;
		break;
	case USB_PD_PORT_POWER_SINK_NOT_CHARGING:
		snprintf(role_str, sizeof(role_str), "%s", "NOT CHARGING");
		port->psy_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		port->psy_online = 1;
		break;
	default:
		snprintf(role_str, sizeof(role_str), "%s", "UNKNOWN");
		dev_err(dev, "Unknown role %d\n", resp.role);
		break;
	}

	port->psy_voltage_max_design = resp.meas.voltage_max;
	port->psy_voltage_now = resp.meas.voltage_now;
	port->psy_current_max = resp.meas.current_max;
	port->psy_power_max = resp.max_power;

	switch (resp.type) {
	case USB_CHG_TYPE_BC12_SDP:
	case USB_CHG_TYPE_VBUS:
		port->psy_type = POWER_SUPPLY_TYPE_USB;
		break;
	case USB_CHG_TYPE_NONE:
		/*
		 * For dual-role devices when we are a source, the firmware
		 * reports the type as NONE. Report such chargers as type
		 * USB_PD_DRP.
		 */
		if (resp.role == USB_PD_PORT_POWER_SOURCE && resp.dualrole)
			port->psy_type = POWER_SUPPLY_TYPE_USB_PD_DRP;
		else
			port->psy_type = POWER_SUPPLY_TYPE_USB;
		break;
	case USB_CHG_TYPE_OTHER:
	case USB_CHG_TYPE_PROPRIETARY:
		port->psy_type = POWER_SUPPLY_TYPE_APPLE_BRICK_ID;
		break;
	case USB_CHG_TYPE_C:
		port->psy_type = POWER_SUPPLY_TYPE_USB_TYPE_C;
		break;
	case USB_CHG_TYPE_BC12_DCP:
		port->psy_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case USB_CHG_TYPE_BC12_CDP:
		port->psy_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case USB_CHG_TYPE_PD:
		if (resp.dualrole)
			port->psy_type = POWER_SUPPLY_TYPE_USB_PD_DRP;
		else
			port->psy_type = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case USB_CHG_TYPE_UNKNOWN:
		/*
		 * While the EC is trying to determine the type of charger that
		 * has been plugged in, it will report the charger type as
		 * unknown. Treat this case as a dedicated charger since we
		 * don't know any better just yet. Additionally since the power
		 * capabilities are unknown, report the max current and voltage
		 * as zero.
		 */
		port->psy_type = POWER_SUPPLY_TYPE_MAINS;
		port->psy_voltage_max_design = 0;
		port->psy_current_max = 0;
		break;
	default:
		dev_err(dev, "Port %d: default case!\n",
			port->port_number);
		port->psy_type = POWER_SUPPLY_TYPE_USB;
	}

	port->psy_desc.type = port->psy_type;

	dev_dbg(dev,
		"Port %d: %s type=%d=vmax=%d vnow=%d cmax=%d clim=%d pmax=%d\n",
		port->port_number, role_str, resp.type,
		resp.meas.voltage_max, resp.meas.voltage_now,
		resp.meas.current_max, resp.meas.current_lim,
		resp.max_power);

	/*
	 * If power supply type or status changed, explicitly call
	 * power_supply_changed. This results in udev event getting generated
	 * and allows user mode apps to react quicker instead of waiting for
	 * their next poll of power supply status.
	 */
	if (last_psy_type != port->psy_type ||
	    last_psy_status != port->psy_status) {
		dev_dbg(dev, "Port %d: Calling power_supply_changed\n",
			port->port_number);
		power_supply_changed(port->psy);
	}

	return 0;
}

static int get_ec_port_status(struct port_data *port, bool ratelimit)
{
	int ret;

	if (ratelimit &&
	    time_is_after_jiffies(port->last_update +
				  CROS_USB_PD_CACHE_UPDATE_DELAY))
		return 0;

	ret = get_ec_usb_pd_power_info(port);
	if (ret < 0)
		return ret;

	ret = get_ec_usb_pd_discovery_info(port);
	port->last_update = jiffies;

	return ret;
}

static void cros_usb_pd_charger_power_changed(struct power_supply *psy)
{
	struct port_data *port = power_supply_get_drvdata(psy);
	struct charger_data *charger = port->charger;
	struct device *dev = charger->dev;
	int i;

	dev_dbg(dev, "cros_usb_pd_charger_power_changed\n");
	for (i = 0; i < charger->num_registered_psy; i++) {
		get_ec_port_status(charger->ports[i], false);
	}
}

static int cros_usb_pd_charger_get_prop(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct port_data *port = power_supply_get_drvdata(psy);
	struct charger_data *charger = port->charger;
	struct device *dev = charger->dev;
	struct cros_ec_device *ec_device = charger->ec_device;
	int ret;


	/* Only refresh ec_port_status for dynamic properties */
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		/*
		 * If mkbp_event_supported, then we can be assured that
		 * the driver's state for the online property is consistent
		 * with the hardware. However, if we aren't event driven,
		 * the optimization before to skip an ec_port_status get
		 * and only returned cached values of the online property will
		 * cause a delay in detecting a cable attach until one of the
		 * other properties are read.
		 *
		 * Allow an ec_port_status refresh for online property check
		 * if we're not already online to check for plug events if
		 * not mkbp_event_supported.
		 */
		if (ec_device->mkbp_event_supported || port->psy_online)
			break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		ret = get_ec_port_status(port, true);
		if (ret < 0) {
			dev_err(dev, "Failed to get port status (err:0x%x)\n",
				ret);
			return -EINVAL;
		}
		break;
	default:
		break;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = port->psy_online;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = port->psy_status;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = port->psy_current_max * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = port->psy_voltage_max_design * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* TODO: change this to voltage_now. */
		val->intval = port->psy_voltage_now * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		/* TODO: send a TBD host command to the EC. */
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = port->model_name;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = port->manufacturer;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cros_usb_pd_charger_set_prop(struct power_supply *psy,
				    enum power_supply_property psp,
				    const union power_supply_propval *val)
{
	struct port_data *port = power_supply_get_drvdata(psy);
	struct charger_data *charger = port->charger;
	int port_number;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		/*
		 * A value of -1 implies switching to battery as the power
		 * source. Any other value implies using this port as the
		 * power source.
		 */
		port_number = val->intval;
		if (port_number != -1)
			port_number = port->port_number;
		return set_ec_usb_pd_override_ports(charger, port_number);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int cros_usb_pd_charger_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static void cros_usb_pd_print_log_entry(struct ec_response_pd_log *r,
					ktime_t tstamp)
{
	static const char * const fault_names[] = {
		"---", "OCP", "fast OCP", "OVP", "Discharge"
	};
	static const char * const role_names[] = {
		"Disconnected", "SRC", "SNK", "SNK (not charging)"
	};
	static const char * const chg_type_names[] = {
		"None", "PD", "Type-C", "Proprietary",
		"DCP", "CDP", "SDP", "Other", "VBUS"
	};
	int i;
	int role_idx, type_idx;
	const char *fault, *role, *chg_type;
	struct usb_chg_measures *meas;
	struct mcdp_info *minfo;
	struct rtc_time rt;
	s64 msecs;
	int len = 0;
	char buf[BUF_SIZE + 1];

	/* the timestamp is the number of 1024th of seconds in the past */
	tstamp = ktime_sub_us(tstamp,
		 (uint64_t)r->timestamp << PD_LOG_TIMESTAMP_SHIFT);
	rt = rtc_ktime_to_tm(tstamp);

	switch (r->type) {
	case PD_EVENT_MCU_CHARGE:
		if (r->data & CHARGE_FLAGS_OVERRIDE)
			APPEND_STRING(buf, len, "override ");
		if (r->data & CHARGE_FLAGS_DELAYED_OVERRIDE)
			APPEND_STRING(buf, len, "pending_override ");
		role_idx = r->data & CHARGE_FLAGS_ROLE_MASK;
		role = role_idx < ARRAY_SIZE(role_names) ?
			role_names[role_idx] : "Unknown";
		type_idx = (r->data & CHARGE_FLAGS_TYPE_MASK)
			 >> CHARGE_FLAGS_TYPE_SHIFT;
		chg_type = type_idx < ARRAY_SIZE(chg_type_names) ?
			chg_type_names[type_idx] : "???";
		meas = (struct usb_chg_measures *)r->payload;

		if ((role_idx == USB_PD_PORT_POWER_DISCONNECTED) ||
		    (role_idx == USB_PD_PORT_POWER_SOURCE)) {
			APPEND_STRING(buf, len, "%s", role);

			if ((role_idx == USB_PD_PORT_POWER_SOURCE) &&
			    (meas->current_max))
				APPEND_STRING(buf, len, " %dmA",
					      meas->current_max);

			break;
		}

		APPEND_STRING(buf, len, "%s %s %s %dmV max %dmV / %dmA", role,
			r->data & CHARGE_FLAGS_DUAL_ROLE ? "DRP" : "Charger",
			chg_type,
			meas->voltage_now,
			meas->voltage_max,
			meas->current_max);
		break;
	case PD_EVENT_ACC_RW_FAIL:
		APPEND_STRING(buf, len, "RW signature check failed");
		break;
	case PD_EVENT_PS_FAULT:
		fault = r->data < ARRAY_SIZE(fault_names) ? fault_names[r->data]
							  : "???";
		APPEND_STRING(buf, len, "Power supply fault: %s", fault);
		break;
	case PD_EVENT_VIDEO_DP_MODE:
		APPEND_STRING(buf, len, "DP mode %sabled",
			      (r->data == 1) ? "en" : "dis");
		break;
	case PD_EVENT_VIDEO_CODEC:
		minfo = (struct mcdp_info *)r->payload;
		APPEND_STRING(buf, len,
			      "HDMI info: family:%04x chipid:%04x "
			      "irom:%d.%d.%d fw:%d.%d.%d",
			      MCDP_FAMILY(minfo->family),
			      MCDP_CHIPID(minfo->chipid),
			      minfo->irom.major, minfo->irom.minor,
			      minfo->irom.build, minfo->fw.major,
			      minfo->fw.minor, minfo->fw.build);
		break;
	default:
		APPEND_STRING(buf, len,
			"Event %02x (%04x) [", r->type, r->data);
		for (i = 0; i < PD_LOG_SIZE(r->size_port); i++)
			APPEND_STRING(buf, len, "%02x ", r->payload[i]);
		APPEND_STRING(buf, len, "]");
		break;
	}

	msecs = ktime_to_ms(tstamp);
	do_div(msecs, MSEC_PER_SEC);
	pr_info("PDLOG %d/%02d/%02d %02d:%02d:%02d.%03lld P%d %s\n",
		rt.tm_year + 1900, rt.tm_mon + 1, rt.tm_mday,
		rt.tm_hour, rt.tm_min, rt.tm_sec, msecs,
		PD_LOG_PORT(r->size_port), buf);
}

static void cros_usb_pd_log_check(struct work_struct *work)
{
	struct charger_data *charger = container_of(to_delayed_work(work),
		struct charger_data, log_work);
	struct device *dev = charger->dev;
	union {
		struct ec_response_pd_log r;
		uint32_t words[8]; /* space for the payload */
	} u;
	int ret;
	int entries = 0;
	ktime_t now;

	if (charger->suspended) {
		dev_dbg(dev, "Suspended...bailing out\n");
		return;
	}

	while (entries++ < CROS_USB_PD_MAX_LOG_ENTRIES) {
		ret = ec_command(charger, 0, EC_CMD_PD_GET_LOG_ENTRY,
				 NULL, 0, (uint8_t *)&u, sizeof(u));
		now = ktime_get_real();
		if (ret < 0) {
			dev_dbg(dev, "Cannot get PD log %d\n", ret);
			break;
		}
		if (u.r.type == PD_EVENT_NO_ENTRY)
			break;

		cros_usb_pd_print_log_entry(&u.r, now);
	}

	queue_delayed_work(charger->log_workqueue, &charger->log_work,
		CROS_USB_PD_LOG_UPDATE_DELAY);
}

static int cros_usb_pd_ec_event(struct notifier_block *nb,
	unsigned long queued_during_suspend, void *_notify)
{
	struct charger_data *charger;
	struct device *dev;
	struct cros_ec_device *ec_device;
	u32 host_event;

	charger = container_of(nb, struct charger_data, notifier);
	dev = charger->dev;
	ec_device = charger->ec_device;

	host_event = cros_ec_get_host_event(ec_device);
	if (host_event & EC_HOST_EVENT_MASK(EC_HOST_EVENT_PD_MCU)) {
		cros_usb_pd_charger_power_changed(charger->ports[0]->psy);
		return NOTIFY_OK;
	} else {
		return NOTIFY_DONE;
	}
}

static char *charger_supplied_to[] = {"cros-usb_pd-charger"};

static int cros_usb_pd_charger_probe(struct platform_device *pd)
{
	struct device *dev = &pd->dev;
	struct cros_ec_dev *ec_dev = dev_get_drvdata(pd->dev.parent);
	struct cros_ec_device *ec_device;
	struct charger_data *charger;
	struct port_data *port;
	struct power_supply_desc *psy_desc;
	struct power_supply *psy;
	int i;
	int ret = -EINVAL;

	dev_dbg(dev, "cros_usb_pd_charger_probe\n");
	if (!ec_dev) {
		dev_err(dev, "No EC dev found\n");
		return -EINVAL;
	}

	ec_device = ec_dev->ec_dev;
	if (!ec_device) {
		dev_err(dev, "No EC device found.\n");
		return -EINVAL;
	}

	charger = devm_kzalloc(dev, sizeof(struct charger_data),
				    GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->dev = dev;
	charger->ec_dev = ec_dev;
	charger->ec_device = ec_device;

	platform_set_drvdata(pd, charger);

	if ((get_ec_num_ports(charger, &charger->num_charger_ports) < 0) ||
	    !charger->num_charger_ports) {
		/*
		 * This can happen on a system that doesn't support USB PD.
		 * Log a message, but no need to warn.
		 */
		dev_info(dev, "No charging ports found\n");
		ret = -ENODEV;
		goto fail_nowarn;
	}

	for (i = 0; i < charger->num_charger_ports; i++) {
		struct power_supply_config psy_cfg = {};

		port = devm_kzalloc(dev, sizeof(struct port_data), GFP_KERNEL);
		if (!port) {
			dev_err(dev, "Failed to alloc port structure\n");
			ret = -ENOMEM;
			goto fail;
		}

		port->charger = charger;
		port->port_number = i;
		sprintf(port->name, CHARGER_DIR_NAME, i);

		psy_desc = &port->psy_desc;
		psy_desc->name = port->name;
		psy_desc->type = POWER_SUPPLY_TYPE_USB;
		psy_desc->get_property = cros_usb_pd_charger_get_prop;
		psy_desc->set_property = cros_usb_pd_charger_set_prop;
		psy_desc->property_is_writeable = cros_usb_pd_charger_is_writeable;
		psy_desc->external_power_changed = cros_usb_pd_charger_power_changed;
		psy_desc->properties = cros_usb_pd_charger_props;
		psy_desc->num_properties = ARRAY_SIZE(cros_usb_pd_charger_props);

		psy_cfg.drv_data = port;
		psy_cfg.supplied_to = charger_supplied_to;
		psy_cfg.num_supplicants = ARRAY_SIZE(charger_supplied_to);

		psy = power_supply_register_no_ws(dev, psy_desc, &psy_cfg);
		if (IS_ERR(psy)) {
			dev_err(dev, "Failed to register power supply\n");
			continue;
		}
		port->psy = psy;

		charger->ports[charger->num_registered_psy++] = port;
		ec_device->charger = psy;
	}

	if (!charger->num_registered_psy) {
		ret = -ENODEV;
		dev_err(dev, "No power supplies registered\n");
		goto fail;
	}

	if (ec_device->mkbp_event_supported) {
		/* Get PD events from the EC */
		charger->notifier.notifier_call = cros_usb_pd_ec_event;
		ret = blocking_notifier_chain_register(&ec_device->event_notifier,
						       &charger->notifier);
		if (ret < 0)
			dev_warn(dev, "failed to register notifier\n");
	}

	/* Retrieve PD event logs periodically */
	INIT_DELAYED_WORK(&charger->log_work, cros_usb_pd_log_check);
	charger->log_workqueue =
		create_singlethread_workqueue("cros_usb_pd_log");
	queue_delayed_work(charger->log_workqueue, &charger->log_work,
		CROS_USB_PD_LOG_UPDATE_DELAY);

	return 0;

fail:
	WARN(1, "%s: Failing probe (err:0x%x)\n", dev_name(dev), ret);

fail_nowarn:
	if (charger) {
		ec_device->charger = NULL;
		for (i = 0; i < charger->num_registered_psy; i++) {
			port = charger->ports[i];
			power_supply_unregister(port->psy);
			devm_kfree(dev, port);
		}
		platform_set_drvdata(pd, NULL);
		devm_kfree(dev, charger);
	}

	dev_info(dev, "Failing probe (err:0x%x)\n", ret);
	return ret;
}

static int cros_usb_pd_charger_remove(struct platform_device *pd)
{
	struct charger_data *charger = platform_get_drvdata(pd);
	struct cros_ec_device *ec_device = charger->ec_device;
	struct device *dev = charger->dev;
	struct port_data *port;
	int i;

	if (charger) {
		ec_device->charger = NULL;
		for (i = 0; i < charger->num_registered_psy; i++) {
			port = charger->ports[i];
			power_supply_unregister(port->psy);
			devm_kfree(dev, port);
		}
		flush_delayed_work(&charger->log_work);
		platform_set_drvdata(pd, NULL);
		devm_kfree(dev, charger);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int cros_usb_pd_charger_resume(struct device *dev)
{
	struct charger_data *charger = dev_get_drvdata(dev);
	int i;

	if (!charger)
		return 0;

	charger->suspended = false;

	dev_dbg(dev, "cros_usb_pd_charger_resume: updating power supplies\n");
	for (i = 0; i < charger->num_registered_psy; i++) {
		power_supply_changed(charger->ports[i]->psy);
		charger->ports[i]->last_update =
				jiffies - CROS_USB_PD_CACHE_UPDATE_DELAY;
	}
	queue_delayed_work(charger->log_workqueue, &charger->log_work,
		CROS_USB_PD_LOG_UPDATE_DELAY);

	return 0;
}

static int cros_usb_pd_charger_suspend(struct device *dev)
{
	struct charger_data *charger = dev_get_drvdata(dev);

	charger->suspended = true;

	if (charger)
		flush_delayed_work(&charger->log_work);
	return 0;
}
#endif

static int set_ec_ext_power_limit(struct cros_ec_dev *ec,
				  uint16_t current_lim, uint16_t voltage_lim)
{
	struct ec_params_external_power_limit_v1 *req;
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + sizeof(*req), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	req = (struct ec_params_external_power_limit_v1 *)msg->data;

	req->current_lim = current_lim;
	req->voltage_lim = voltage_lim;

	msg->version = 1;
	msg->command = ec->cmd_offset + EC_CMD_EXTERNAL_POWER_LIMIT;
	msg->outsize = sizeof(*req);

	ret = cros_ec_cmd_xfer_status(ec->ec_dev, msg);
	if (ret < 0) {
		dev_warn(ec->dev,
			 "External power limit command returned 0x%x\n",
			 ret);
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	kfree(msg);
	return ret;
}

static ssize_t get_ec_ext_current_lim(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", ext_current_lim);
}

static ssize_t set_ec_ext_current_lim(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	uint16_t tmp_val;
	struct cros_ec_dev *ec = container_of(
			dev, struct cros_ec_dev, class_dev);

	if (kstrtou16(buf, 0, &tmp_val) < 0)
		return -EINVAL;

	ret = set_ec_ext_power_limit(ec, tmp_val, ext_voltage_lim);
	if (ret < 0)
		return ret;

	ext_current_lim = tmp_val;
	if (ext_current_lim == EC_POWER_LIMIT_NONE)
		dev_info(ec->dev, "External current limit removed\n");
	else
		dev_info(ec->dev, "External current limit set to %dmA\n",
			 ext_current_lim);

	return count;
}

static ssize_t get_ec_ext_voltage_lim(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", ext_voltage_lim);
}

static ssize_t set_ec_ext_voltage_lim(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	uint16_t tmp_val;

	struct cros_ec_dev *ec = container_of(
			dev, struct cros_ec_dev, class_dev);

	if (kstrtou16(buf, 0, &tmp_val) < 0)
		return -EINVAL;

	ret = set_ec_ext_power_limit(ec, ext_current_lim, tmp_val);
	if (ret < 0)
		return ret;

	ext_voltage_lim = tmp_val;
	if (ext_voltage_lim == EC_POWER_LIMIT_NONE)
		dev_info(ec->dev, "External voltage limit removed\n");
	else
		dev_info(ec->dev, "External voltage limit set to %dmV\n",
			 ext_voltage_lim);

	return count;
}

static DEVICE_ATTR(ext_current_lim, S_IWUSR | S_IWGRP | S_IRUGO,
		   get_ec_ext_current_lim,
		   set_ec_ext_current_lim);
static DEVICE_ATTR(ext_voltage_lim, S_IWUSR | S_IWGRP | S_IRUGO,
		   get_ec_ext_voltage_lim,
		   set_ec_ext_voltage_lim);

static struct attribute *__ext_power_cmds_attrs[] = {
	&dev_attr_ext_current_lim.attr,
	&dev_attr_ext_voltage_lim.attr,
	NULL,
};

struct attribute_group cros_usb_pd_charger_attr_group = {
	.name = "usb-pd-charger",
	.attrs = __ext_power_cmds_attrs,
};
EXPORT_SYMBOL(cros_usb_pd_charger_attr_group);

static SIMPLE_DEV_PM_OPS(cros_usb_pd_charger_pm_ops,
	cros_usb_pd_charger_suspend, cros_usb_pd_charger_resume);

static struct platform_driver cros_usb_pd_charger_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &cros_usb_pd_charger_pm_ops,
	},
	.probe = cros_usb_pd_charger_probe,
	.remove = cros_usb_pd_charger_remove,
};

module_platform_driver(cros_usb_pd_charger_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Chrome USB PD charger");
MODULE_ALIAS("platform:" DRV_NAME);
