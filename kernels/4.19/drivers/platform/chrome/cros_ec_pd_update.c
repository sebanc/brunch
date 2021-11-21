/*
 * cros_ec_pd_update - Chrome OS EC Power Delivery Device FW Update Driver
 *
 * Copyright (C) 2014 Google, Inc
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
 * This driver communicates with a Chrome OS PD device and performs tasks
 * related to auto-updating its firmware.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_pd_update.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_data/cros_usbpd_notify.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

/*
 * Driver loaded when a Chrome OS PD device is found.
 */
#define DRV_NAME "cros-ec-pd-update"

struct cros_ec_dev *cros_ec_pd_ec;
EXPORT_SYMBOL_GPL(cros_ec_pd_ec);

/* Allow disabling of the update for testing purposes */
static int disable;

/*
 * $DEVICE_known_update_hashes - A list of old known RW hashes from which we
 * wish to upgrade. When cros_ec_pd_firmware_images is updated, the old hash
 * should probably be added here. The latest hash currently in
 * cros_ec_pd_firmware_images should NOT appear here.
 */
static uint8_t zinger_known_update_hashes[][PD_RW_HASH_SIZE] = {
	/* zinger_v1.7.509-e5bffd3.bin */
	{ 0x02, 0xad, 0x4c, 0x95, 0x25,
	  0x89, 0xe5, 0xe7, 0x1e, 0xc6,
	  0xaf, 0x9c, 0x0e, 0xaa, 0xbb,
	  0x6c, 0xa7, 0x52, 0x8c, 0x3a },
	/* zinger_v1.7.262-9a5b8f4.bin */
	{ 0x05, 0x94, 0xb8, 0x97, 0x8a,
	  0x9a, 0xa0, 0x0a, 0x71, 0x07,
	  0x37, 0xba, 0x8f, 0x4c, 0x01,
	  0xe6, 0x45, 0x6d, 0xb0, 0x01 },
};

static uint8_t dingdong_known_update_hashes[][PD_RW_HASH_SIZE] = {
	/* dingdong_v1.7.575-96b74f1.bin devid: 3.2 */
	{ 0x64, 0xdb, 0x4e, 0x86, 0xd6,
	  0x7d, 0x7a, 0xce, 0x41, 0xfd,
	  0x09, 0x3b, 0xd4, 0x8b, 0x3f,
	  0x1f, 0xba, 0x73, 0xcb, 0x73 },
	/* dingdong_v1.7.489-8533e9d.bin devid: 3.2 */
	{ 0x53, 0x20, 0x21, 0x34, 0xc2,
	  0xee, 0x2f, 0x07, 0xbb, 0x24,
	  0x94, 0xab, 0xbe, 0x1f, 0xee,
	  0xf2, 0xb3, 0x7e, 0xff, 0x23 },
	/* dingdong_v1.7.317-b0bb7c9.bin devid: 3.1 */
	{ 0x0f, 0x1e, 0x93, 0x9f, 0xbc,
	  0x23, 0x0a, 0x3f, 0x4f, 0x35,
	  0xf8, 0xfe, 0xd8, 0xa9, 0x71,
	  0x8f, 0xef, 0x15, 0xc8, 0xea },
};

static uint8_t hoho_known_update_hashes[][PD_RW_HASH_SIZE] = {
	/* hoho_v1.7.575-96b74f1.bin devid: 4.2 */
	{ 0x4b, 0x3d, 0x8b, 0xba, 0x8a,
	  0x62, 0xae, 0x4f, 0x64, 0xd2,
	  0x0f, 0x96, 0xf9, 0x4e, 0xc7,
	  0xf6, 0x6a, 0x19, 0x84, 0x1c },
	/* hoho_v1.7.489-8533e9d.bin devid: 4.2 */
	{ 0xac, 0x00, 0xc1, 0x4c, 0x3a,
	  0x77, 0xa6, 0x1f, 0xf9, 0xd5,
	  0x59, 0x3a, 0x56, 0x06, 0x5c,
	  0x86, 0x09, 0xe0, 0x03, 0xb3 },
	/* hoho_v1.7.317-b0bb7c9.bin devid:4.1 */
	{ 0x98, 0x19, 0xa6, 0x6b, 0x61,
	  0x1f, 0x28, 0xba, 0xde, 0x80,
	  0xa3, 0x88, 0x95, 0x67, 0x57,
	  0xa2, 0x98, 0xe4, 0xf1, 0x62 },
};

const struct cros_ec_pd_firmware_image cros_ec_pd_firmware_images[] = {
	/* PD_DEVICE_TYPE_ZINGER */
	{
		.id_major = PD_DEVICE_TYPE_ZINGER,
		.id_minor = 1,
		.usb_vid = USB_VID_GOOGLE,
		.usb_pid = USB_PID_ZINGER,
		.filename = "cros-pd/zinger_v1.7.539-91a0fa2.bin",
		.rw_image_size = (16 * 1024),
		.hash = { 0x3b, 0x2e, 0xe3, 0xf6, 0x1e,
			  0x6a, 0x1d, 0x49, 0xd3, 0x1c,
			  0xf5, 0x77, 0x5e, 0xa7, 0x19,
			  0xdb, 0xde, 0xcd, 0xaa, 0xc2 },
		.update_hashes = &zinger_known_update_hashes,
		.update_hash_count = ARRAY_SIZE(zinger_known_update_hashes),
	},
	{
		.id_major = PD_DEVICE_TYPE_DINGDONG,
		.id_minor = 2,
		.usb_vid = USB_VID_GOOGLE,
		.usb_pid = USB_PID_DINGDONG,
		.filename = "cros-pd/dingdong_v1.7.684-69498dd.bin",
		.rw_image_size = (64 * 1024),
		.hash = { 0xe6, 0x97, 0x90, 0xd9, 0xe5,
			  0x01, 0x15, 0x22, 0xee, 0x1c,
			  0x7e, 0x4d, 0x6c, 0x54, 0x78,
			  0xd4, 0x7a, 0xa7, 0xda, 0x1d },
		.update_hashes = &dingdong_known_update_hashes,
		.update_hash_count = ARRAY_SIZE(dingdong_known_update_hashes),
	},
	{
		.id_major = PD_DEVICE_TYPE_DINGDONG,
		.id_minor = 1,
		.usb_vid = USB_VID_GOOGLE,
		.usb_pid = USB_PID_DINGDONG,
		.filename = "cros-pd/dingdong_v1.7.684-69498dd.bin",
		.rw_image_size = (64 * 1024),
		.hash = { 0xe6, 0x97, 0x90, 0xd9, 0xe5,
			  0x01, 0x15, 0x22, 0xee, 0x1c,
			  0x7e, 0x4d, 0x6c, 0x54, 0x78,
			  0xd4, 0x7a, 0xa7, 0xda, 0x1d },
		.update_hashes = &dingdong_known_update_hashes,
		.update_hash_count = ARRAY_SIZE(dingdong_known_update_hashes),
	},
	{
		.id_major = PD_DEVICE_TYPE_HOHO,
		.id_minor = 2,
		.usb_vid = USB_VID_GOOGLE,
		.usb_pid = USB_PID_HOHO,
		.filename = "cros-pd/hoho_v1.7.684-69498dd.bin",
		.rw_image_size = (64 * 1024),
		.hash = { 0x43, 0x1b, 0x4e, 0x20, 0xe8,
			  0x38, 0xdd, 0x29, 0x42, 0xbd,
			  0x6d, 0xfc, 0x13, 0xf2, 0xb2,
			  0x46, 0xa6, 0xf4, 0x98, 0x08 },
		.update_hashes = &hoho_known_update_hashes,
		.update_hash_count = ARRAY_SIZE(hoho_known_update_hashes),
	},
	{
		.id_major = PD_DEVICE_TYPE_HOHO,
		.id_minor = 1,
		.usb_vid = USB_VID_GOOGLE,
		.usb_pid = USB_PID_HOHO,
		.filename = "cros-pd/hoho_v1.7.684-69498dd.bin",
		.rw_image_size = (64 * 1024),
		.hash = { 0x43, 0x1b, 0x4e, 0x20, 0xe8,
			  0x38, 0xdd, 0x29, 0x42, 0xbd,
			  0x6d, 0xfc, 0x13, 0xf2, 0xb2,
			  0x46, 0xa6, 0xf4, 0x98, 0x08 },
		.update_hashes = &hoho_known_update_hashes,
		.update_hash_count = ARRAY_SIZE(hoho_known_update_hashes),
	},
	{
		/* Empty image for termination. */
	},
};
EXPORT_SYMBOL_GPL(cros_ec_pd_firmware_images);

/**
 * cros_ec_pd_command - Send a command to the EC. Returns 0 on success,
 * <0 on failure.
 *
 * @dev: PD device
 * @pd_dev: EC PD device
 * @command: EC command
 * @outdata: EC command output data
 * @outsize: Size of outdata
 * @indata: EC command input data
 * @insize: Size of indata
 */
static int cros_ec_pd_command(struct device *dev,
			      struct cros_ec_dev *pd_dev,
			      int command,
			      uint8_t *outdata,
			      int outsize,
			      uint8_t *indata,
			      int insize)
{
	int ret;
	struct cros_ec_command *msg;

	msg = kzalloc(sizeof(*msg) + max(insize, outsize), GFP_KERNEL);
	if (!msg)
		return -EC_RES_ERROR;

	msg->command = command | pd_dev->cmd_offset;
	msg->outsize = outsize;
	msg->insize = insize;

	if (outsize)
		memcpy(msg->data, outdata, outsize);

	ret = cros_ec_cmd_xfer_status(pd_dev->ec_dev, msg);
	if (ret < 0)
		goto error;

	if (insize)
		memcpy(indata, msg->data, insize);
	ret = EC_RES_SUCCESS;
error:
	kfree(msg);
	return ret;
}

/**
 * cros_ec_pd_enter_gfu - Enter GFU alternate mode.
 * Returns 0 if ec command successful <0 on failure.
 *
 * Note, doesn't guarantee entry.
 *
 * @dev: PD device
 * @pd_dev: EC PD device
 * @port: Port # on device
 */
static int cros_ec_pd_enter_gfu(struct device *dev, struct cros_ec_dev *pd_dev,
				int port)
{
	int rv;
	struct ec_params_usb_pd_set_mode_request set_mode_request;

	set_mode_request.port = port;
	set_mode_request.svid = USB_VID_GOOGLE;
	/* TODO(tbroch) Will GFU always be '1'? */
	set_mode_request.opos = 1;
	set_mode_request.cmd = PD_ENTER_MODE;
	rv = cros_ec_pd_command(dev, pd_dev, EC_CMD_USB_PD_SET_AMODE,
				(uint8_t *)&set_mode_request,
				sizeof(set_mode_request),
				NULL, 0);
	if (!rv)
		/* Allow time to enter GFU mode */
		msleep(500);

	return rv;
}

int cros_ec_pd_get_status(
		struct device *dev,
		struct cros_ec_dev *pd_dev,
		int port,
		struct ec_params_usb_pd_rw_hash_entry *hash_entry,
		struct ec_params_usb_pd_discovery_entry *discovery_entry)
{
	struct ec_params_usb_pd_info_request info_request;
	int ret;

	info_request.port = port;
	ret = cros_ec_pd_command(dev, pd_dev, EC_CMD_USB_PD_DEV_INFO,
				 (uint8_t *)&info_request, sizeof(info_request),
				 (uint8_t *)hash_entry, sizeof(*hash_entry));
	/* Skip getting USB discovery data if no device present on port */
	if (ret < 0 || hash_entry->dev_id == PD_DEVICE_TYPE_NONE)
		return ret;

	return cros_ec_pd_command(dev, pd_dev, EC_CMD_USB_PD_DISCOVERY,
				  (uint8_t *)&info_request,
				  sizeof(info_request),
				  (uint8_t *)discovery_entry,
				  sizeof(*discovery_entry));
}
EXPORT_SYMBOL_GPL(cros_ec_pd_get_status);

/**
 * cros_ec_pd_send_hash_entry - Inform the EC of a PD devices for which we
 * have firmware available. EC typically will not store more than four hashes.
 * Returns 0 on success, <0 on failure.
 *
 * @dev: PD device
 * @pd_dev: EC PD device
 * @fw: FW update image to inform the EC of
 */
static int cros_ec_pd_send_hash_entry(struct device *dev,
				      struct cros_ec_dev *pd_dev,
				      const struct cros_ec_pd_firmware_image
						   *fw)
{
	struct ec_params_usb_pd_rw_hash_entry hash_entry;

	hash_entry.dev_id = MAJOR_MINOR_TO_DEV_ID(fw->id_major, fw->id_minor);
	memcpy(hash_entry.dev_rw_hash, fw->hash, PD_RW_HASH_SIZE);

	return cros_ec_pd_command(dev, pd_dev, EC_CMD_USB_PD_RW_HASH_ENTRY,
				  (uint8_t *)&hash_entry, sizeof(hash_entry),
				  NULL, 0);
}

/**
 * cros_ec_pd_send_fw_update_cmd - Send update-related EC command.
 * Returns 0 on success, <0 on failure.
 *
 * @dev: PD device
 * @pd_dev: EC PD device
 * @pd_cmd: fw_update command
 */
static int cros_ec_pd_send_fw_update_cmd(struct device *dev,
					 struct cros_ec_dev *pd_dev,
					 struct ec_params_usb_pd_fw_update
						*pd_cmd)
{
	return cros_ec_pd_command(dev, pd_dev, EC_CMD_USB_PD_FW_UPDATE,
				  (uint8_t *)pd_cmd,
				  pd_cmd->size + sizeof(*pd_cmd),
				  NULL, 0);
}

/**
 * cros_ec_pd_get_num_ports - Get number of EC charge ports.
 * Returns 0 on success, <0 on failure.
 *
 * @dev: PD device
 * @pd_dev: EC PD device
 * @num_ports: Holds number of ports, on command success
 */
static int cros_ec_pd_get_num_ports(struct device *dev,
				    struct cros_ec_dev *pd_dev,
				    int *num_ports)
{
	struct ec_response_usb_pd_ports resp;
	int ret;

	ret = cros_ec_pd_command(dev, pd_dev, EC_CMD_USB_PD_PORTS,
				 NULL, 0,
				 (uint8_t *)&resp, sizeof(resp));
	if (ret == EC_RES_SUCCESS)
		*num_ports = resp.num_ports;
	return ret;
}


/**
 * cros_ec_pd_fw_update - Send EC_CMD_USB_PD_FW_UPDATE command to perform
 * update-related operation.
 * Returns 0 on success, <0 on failure.
 *
 * @dev: PD device
 * @pd_dev: EC PD device
 * @fw: RW FW update file
 * @port: Port# to which update device is attached
 */
static int cros_ec_pd_fw_update(struct cros_ec_pd_update_data *drv_data,
				struct cros_ec_dev *pd_dev,
				const struct firmware *fw,
				uint8_t port)
{
	uint8_t cmd_buf[sizeof(struct ec_params_usb_pd_fw_update) +
			PD_FLASH_WRITE_STEP];
	struct ec_params_usb_pd_fw_update *pd_cmd =
		(struct ec_params_usb_pd_fw_update *)cmd_buf;
	uint8_t *pd_cmd_data = cmd_buf + sizeof(*pd_cmd);
	struct device *dev = drv_data->dev;
	int i, ret;

	if (drv_data->is_suspending)
		return -EBUSY;

	/* Common port */
	pd_cmd->port = port;

	/* Erase signature */
	pd_cmd->cmd = USB_PD_FW_ERASE_SIG;
	pd_cmd->size = 0;
	ret = cros_ec_pd_send_fw_update_cmd(dev, pd_dev, pd_cmd);
	if (ret < 0) {
		dev_err(dev,
			"Unable to clear Port%d PD signature (err:%d)\n",
			port, ret);
		return ret;
	}

	/* Reboot PD */
	pd_cmd->cmd = USB_PD_FW_REBOOT;
	pd_cmd->size = 0;
	ret = cros_ec_pd_send_fw_update_cmd(dev, pd_dev, pd_cmd);
	if (ret < 0) {
		dev_err(dev, "Unable to reboot Port%d PD (err:%d)\n",
			port, ret);
		return ret;
	}

	/*
	 * Wait for the charger to reboot.
	 * TODO(shawnn): Instead of waiting for a fixed period of time, wait
	 * to receive an interrupt that signals the charger is back online.
	 */
	msleep(4000);

	if (drv_data->is_suspending)
		return -EBUSY;

	/*
	 * Force re-entry into GFU mode for USBPD devices that don't enter
	 * it by default.
	 */
	ret = cros_ec_pd_enter_gfu(dev, pd_dev, port);
	if (ret < 0)
		dev_warn(dev, "Unable to enter GFU (err:%d)\n", ret);

	/* Erase RW flash */
	pd_cmd->cmd = USB_PD_FW_FLASH_ERASE;
	pd_cmd->size = 0;
	ret = cros_ec_pd_send_fw_update_cmd(dev, pd_dev, pd_cmd);
	if (ret < 0) {
		dev_err(dev, "Unable to erase Port%d PD RW flash (err:%d)\n",
			port, ret);
		return ret;
	}

	/* Wait 3 seconds for the PD peripheral to finalize RW erase */
	msleep(3000);

	/* Write RW flash */
	pd_cmd->cmd = USB_PD_FW_FLASH_WRITE;
	for (i = 0; i < fw->size; i += PD_FLASH_WRITE_STEP) {
		if (drv_data->is_suspending)
			return -EBUSY;
		pd_cmd->size = min(fw->size - i, (size_t)PD_FLASH_WRITE_STEP);
		memcpy(pd_cmd_data, fw->data + i, pd_cmd->size);
		ret = cros_ec_pd_send_fw_update_cmd(dev, pd_dev, pd_cmd);
		if (ret < 0) {
			dev_err(dev,
				"Unable to write Port%d PD RW flash (err:%d)\n",
				port, ret);
			return ret;
		}
	}

	/* Wait 100ms to guarantee that writes finish */
	msleep(100);

	/* Reboot PD into new RW */
	pd_cmd->cmd = USB_PD_FW_REBOOT;
	pd_cmd->size = 0;
	ret = cros_ec_pd_send_fw_update_cmd(dev, pd_dev, pd_cmd);
	if (ret < 0) {
		dev_err(dev,
			"Unable to reboot Port%d PD post-flash (err:%d)\n",
			port, ret);
		return ret;
	}

	return 0;
}

/**
 * cros_ec_find_update_firmware - Search firmware image table for an image
 * matching the passed attributes, then decide whether an update should
 * be performed.
 * Returns PD_DO_UPDATE if an update should be performed, and writes the
 * cros_ec_pd_firmware_image pointer to update_image.
 * Returns reason for not updating otherwise.
 *
 * @dev: PD device
 * @hash_entry: Pre-filled hash entry struct for matching
 * @discovery_entry: Pre-filled discovery entry struct for matching
 * @update_image: Stores update firmware image on success
 */
static enum cros_ec_pd_find_update_firmware_result cros_ec_find_update_firmware(
	struct device *dev,
	struct ec_params_usb_pd_rw_hash_entry *hash_entry,
	struct ec_params_usb_pd_discovery_entry *discovery_entry,
	const struct cros_ec_pd_firmware_image **update_image)
{
	const struct cros_ec_pd_firmware_image *img;
	int i;

	if (hash_entry->dev_id == PD_DEVICE_TYPE_NONE)
		return PD_UNKNOWN_DEVICE;

	/*
	 * Search for a matching firmware update image.
	 * TODO(shawnn): Replace sequential table search with modified binary
	 * search on major / minor.
	 */
	for (i = 0; cros_ec_pd_firmware_images[i].rw_image_size > 0; i++) {
		img = &cros_ec_pd_firmware_images[i];
		if (MAJOR_MINOR_TO_DEV_ID(img->id_major, img->id_minor)
					  == hash_entry->dev_id &&
		    img->usb_vid == discovery_entry->vid &&
		    img->usb_pid == discovery_entry->pid)
			break;
	}
	*update_image = img;

	if (cros_ec_pd_firmware_images[i].rw_image_size == 0)
		return PD_UNKNOWN_DEVICE;

	if (!memcmp(hash_entry->dev_rw_hash, img->hash, PD_RW_HASH_SIZE)) {
		if (hash_entry->current_image != EC_IMAGE_RW)
			/*
			 * As signature isn't factored into the hash if we've
			 * previously updated RW but subsequently invalidate
			 * signature we can get into this situation.  Need to
			 * reflash.
			 */
			return PD_DO_UPDATE;
		/* Device is already updated */
		return PD_ALREADY_HAVE_LATEST;
	}

	/* Always update if PD device is stuck in RO. */
	if (hash_entry->current_image != EC_IMAGE_RW) {
		dev_info(dev, "Updating FW since PD dev is in RO\n");
		return PD_DO_UPDATE;
	}

	dev_info(dev, "Considering upgrade from existing RW: %x %x %x %x\n",
		 hash_entry->dev_rw_hash[0],
		 hash_entry->dev_rw_hash[1],
		 hash_entry->dev_rw_hash[2],
		 hash_entry->dev_rw_hash[3]);

	/* Verify RW is a known update image so we don't roll-back. */
	for (i = 0; i < img->update_hash_count; ++i)
		if (memcmp(hash_entry->dev_rw_hash,
			   (*img->update_hashes)[i],
			   PD_RW_HASH_SIZE) == 0) {
			dev_info(dev, "Updating FW since RW is known\n");
			return PD_DO_UPDATE;
		}

	dev_info(dev, "Skipping FW update since RW is unknown\n");
	return PD_UNKNOWN_RW;
}

/**
 * cros_ec_pd_get_host_event_status - Get host event status and return.  If
 * failure return 0.
 *
 * @dev: PD device
 * @pd_dev: EC PD device
 */
static uint32_t cros_ec_pd_get_host_event_status(struct device *dev,
						 struct cros_ec_dev *pd_dev)
{
	int ret;
	struct ec_response_host_event_status host_event_status;

	/* Check for host events on EC. */
	ret = cros_ec_pd_command(dev, pd_dev, EC_CMD_PD_HOST_EVENT_STATUS,
				 NULL, 0,
				 (uint8_t *)&host_event_status,
				 sizeof(host_event_status));
	if (ret) {
		dev_err(dev, "Can't get host event status (err: %d)\n", ret);
		return 0;
	}
	dev_dbg(dev, "Got host event status %x\n", host_event_status.status);
	return host_event_status.status;
}

/**
 * cros_ec_pd_update_check - Probe the status of attached PD devices and kick
 * off an RW firmware update if needed. This is run as a deferred task on
 * module load, resume, and when an ACPI event is received (typically on
 * PD device insertion).
 *
 * @work: Delayed work pointer
 */
static void cros_ec_pd_update_check(struct work_struct *work)
{
	const struct cros_ec_pd_firmware_image *img;
	const struct firmware *fw;
	struct ec_params_usb_pd_rw_hash_entry hash_entry;
	struct ec_params_usb_pd_discovery_entry discovery_entry;
	struct cros_ec_pd_update_data *drv_data =
		container_of(to_delayed_work(work),
		struct cros_ec_pd_update_data, work);
	struct device *dev = drv_data->dev;
	struct power_supply *charger;
	enum cros_ec_pd_find_update_firmware_result result;
	int ret, port;

	if (disable) {
		dev_info(dev, "Update is disabled\n");
		return;
	}

	dev_dbg(dev, "Checking for updates\n");

	/* Force GFU entry for devices not in GFU by default. */
	for (port = 0; port < drv_data->num_ports; ++port) {
		dev_dbg(dev, "Considering GFU entry on C%d\n", port);
		ret = cros_ec_pd_get_status(dev, cros_ec_pd_ec,
					    port, &hash_entry,
					    &discovery_entry);
		if (ret || (hash_entry.dev_id == PD_DEVICE_TYPE_NONE)) {
			dev_dbg(dev, "Forcing GFU entry on C%d\n", port);
			cros_ec_pd_enter_gfu(dev, cros_ec_pd_ec, port);
		}
	}

	/*
	 * Override status received from EC if update is forced, such as
	 * after power-on or after resume.
	 */
	mutex_lock(&drv_data->lock);
	if (drv_data->force_update) {
		drv_data->pd_status =
			PD_EVENT_POWER_CHANGE | PD_EVENT_UPDATE_DEVICE;
		drv_data->force_update = 0;
	}

	/*
	 * If there is an EC based charger, send a notification to it to
	 * trigger a refresh of the power supply state.
	 */
	charger = cros_ec_pd_ec->ec_dev->charger;
	if ((drv_data->pd_status & PD_EVENT_POWER_CHANGE) && charger)
		charger->desc->external_power_changed(charger);

	if (!(drv_data->pd_status & PD_EVENT_UPDATE_DEVICE)) {
		drv_data->pd_status = 0;
		mutex_unlock(&drv_data->lock);
		return;
	}

	drv_data->pd_status = 0;
	mutex_unlock(&drv_data->lock);

	/* Received notification, send command to check on PD status. */
	for (port = 0; port < drv_data->num_ports; ++port) {
		/* Don't try to update if we're going to suspend. */
		if (drv_data->is_suspending)
			return;

		ret = cros_ec_pd_get_status(dev, cros_ec_pd_ec,
					    port, &hash_entry,
					    &discovery_entry);
		if (ret < 0) {
			dev_err(dev,
				"Can't get Port%d device status (err:%d)\n",
				port, ret);
			return;
		}

		result = cros_ec_find_update_firmware(dev,
						      &hash_entry,
						      &discovery_entry,
						      &img);
		dev_dbg(dev, "Find Port%d FW result: %d\n", port, result);

		switch (result) {
		case PD_DO_UPDATE:
			if (request_firmware(&fw, img->filename, dev)) {
				dev_err(dev,
					"Error, Port%d can't load file %s\n",
					port, img->filename);
				break;
			}

			if (fw->size != img->rw_image_size) {
				dev_err(dev,
					"Port%d FW file %s size %zd != %zd\n",
					port, img->filename, fw->size,
					img->rw_image_size);
				goto done;
			}

			/* Update firmware */
			dev_info(dev, "Updating Port%d RW to %s\n", port,
				 img->filename);
			ret = cros_ec_pd_fw_update(drv_data, cros_ec_pd_ec, fw,
						   port);
			dev_info(dev,
				 "Port%d FW update completed with status %d\n",
				  port, ret);
done:
			release_firmware(fw);
			break;
		case PD_ALREADY_HAVE_LATEST:
			/*
			 * Device already has latest firmare. Send hash entry
			 * to EC so we don't get subsequent FW update requests.
			 */
			dev_info(dev, "Port%d FW is already up-to-date %s\n",
				 port, img->filename);
			cros_ec_pd_send_hash_entry(dev, cros_ec_pd_ec, img);
			break;
		case PD_UNKNOWN_DEVICE:
		case PD_UNKNOWN_RW:
			/* Unknown PD device or RW -- don't update FW */
			break;
		}
	}
}

/**
 * cros_ec_pd_notify - Called upon receiving a PD MCU event (typically
 * due to PD device insertion). Queue a delayed task to check if a PD
 * device FW update is necessary.
 */
static void cros_ec_pd_notify(struct device *dev, u32 event)
{
	struct cros_ec_pd_update_data *drv_data =
		(struct cros_ec_pd_update_data *)
		dev_get_drvdata(dev);

	if (drv_data) {
		mutex_lock(&drv_data->lock);
		if (event == 0)
			drv_data->pd_status =
				cros_ec_pd_get_host_event_status(dev,
								 cros_ec_pd_ec);
		else
			drv_data->pd_status = event;
		mutex_unlock(&drv_data->lock);
		queue_delayed_work(drv_data->workqueue, &drv_data->work,
				   PD_UPDATE_CHECK_DELAY);
	} else {
		dev_warn(dev, "PD notification skipped due to missing drv_data\n");
	}
}

static ssize_t disable_firmware_update(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct cros_ec_pd_update_data *drv_data;

	ret = sscanf(buf, "%i", &val);
	if (ret != 1)
		return -EINVAL;

	disable = !!val;
	dev_info(dev, "FW update is %sabled\n", disable ? "dis" : "en");

	drv_data = (struct cros_ec_pd_update_data *)dev_get_drvdata(dev);

	/* If re-enabled then force update */
	if (!disable && drv_data) {
		drv_data->force_update = 1;
		queue_delayed_work(drv_data->workqueue, &drv_data->work,
				   PD_UPDATE_CHECK_DELAY);
	}

	return count;
}

static DEVICE_ATTR(disable, 0200, NULL, disable_firmware_update);

static struct attribute *pd_attrs[] = {
	&dev_attr_disable.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pd);

static int cros_ec_pd_add(struct device *dev)
{
	struct cros_ec_pd_update_data *drv_data;
	int ret, i;

	/* If cros_ec_pd_ec is not initialized, try again later */
	if (!cros_ec_pd_ec)
		return -EPROBE_DEFER;

	drv_data =
		devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	mutex_init(&drv_data->lock);

	drv_data->dev = dev;
	INIT_DELAYED_WORK(&drv_data->work, cros_ec_pd_update_check);
	drv_data->workqueue =
		create_singlethread_workqueue("cros_ec_pd_update");
	if (cros_ec_pd_get_num_ports(drv_data->dev,
				     cros_ec_pd_ec,
				     &drv_data->num_ports) < 0) {
		dev_err(drv_data->dev, "Can't get num_ports\n");
		return -EINVAL;
	}
	drv_data->force_update = 1;
	drv_data->is_suspending = 0;
	dev_set_drvdata(dev, drv_data);
	ret = sysfs_create_groups(&dev->kobj, pd_groups);
	if (ret) {
		dev_err(dev, "failed to create sysfs attributes: %d\n", ret);
		return ret;
	}

	/*
	 * Send list of update FW hashes to PD MCU.
	 * TODO(crosbug.com/p/35510): This won't scale past four update
	 * devices. Find a better solution once we get there.
	 */
	for (i = 0; cros_ec_pd_firmware_images[i].rw_image_size > 0; i++)
		cros_ec_pd_send_hash_entry(drv_data->dev,
					   cros_ec_pd_ec,
					   &cros_ec_pd_firmware_images[i]);

	queue_delayed_work(drv_data->workqueue, &drv_data->work,
		PD_UPDATE_CHECK_DELAY);
	return 0;
}

static int cros_ec_pd_resume(struct device *dev)
{
	struct cros_ec_pd_update_data *drv_data =
		(struct cros_ec_pd_update_data *)dev_get_drvdata(dev);

	if (drv_data) {
		drv_data->force_update = 1;
		drv_data->is_suspending = 0;
		queue_delayed_work(drv_data->workqueue, &drv_data->work,
			PD_UPDATE_CHECK_DELAY);
	}
	return 0;
}

static int cros_ec_pd_remove(struct device *dev)
{
	struct cros_ec_pd_update_data *drv_data =
		(struct cros_ec_pd_update_data *)
		dev_get_drvdata(dev);

	if (drv_data) {
		drv_data->is_suspending = 1;
		cancel_delayed_work_sync(&drv_data->work);
		mutex_destroy(&drv_data->lock);
	}


	return 0;
}

static int cros_ec_pd_suspend(struct device *dev)
{
	struct cros_ec_pd_update_data *drv_data =
		(struct cros_ec_pd_update_data *)dev_get_drvdata(dev);

	if (drv_data) {
		drv_data->is_suspending = 1;
		cancel_delayed_work_sync(&drv_data->work);
		disable = 0;
	}
	return 0;
}

static SIMPLE_DEV_PM_OPS(cros_ec_pd_pm,
	cros_ec_pd_suspend, cros_ec_pd_resume);

static int _ec_pd_notify(struct notifier_block *nb,
	unsigned long host_event, void *_notify)
{
	struct cros_ec_pd_update_data *drv_data;
	struct device *dev;

	drv_data = container_of(nb, struct cros_ec_pd_update_data, notifier);
	dev = drv_data->dev;

	cros_ec_pd_notify(dev, host_event);
	return NOTIFY_OK;
}

static int plat_cros_ec_pd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_pd_update_data *drv_data =
		(struct cros_ec_pd_update_data *)dev_get_drvdata(dev);
	int ret;

	ret = cros_ec_pd_add(dev);
	if (ret < 0)
		return ret;

	drv_data = (struct cros_ec_pd_update_data *)dev_get_drvdata(dev);
	/* Get PD events from the EC */
	drv_data->notifier.notifier_call = _ec_pd_notify;
	ret = cros_usbpd_register_notify(&drv_data->notifier);
	if (ret < 0)
		dev_warn(dev, "failed to register notifier\n");

	return 0;
}

static int plat_cros_ec_pd_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_pd_update_data *drv_data =
		(struct cros_ec_pd_update_data *)dev_get_drvdata(dev);

	cros_usbpd_unregister_notify(&drv_data->notifier);

	return cros_ec_pd_remove(dev);
}

static struct platform_driver cros_ec_pd_driver = {
	.driver = {
		.name  = DRV_NAME,
		.pm = &cros_ec_pd_pm,
	},
	.remove  = plat_cros_ec_pd_remove,
	.probe   = plat_cros_ec_pd_probe,
};

module_platform_driver(cros_ec_pd_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS power device FW update driver");
MODULE_ALIAS("platform:" DRV_NAME);
