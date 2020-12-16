/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef I2C_HID_H
#define I2C_HID_H

#include <linux/i2c.h>

#ifdef CONFIG_DMI
struct i2c_hid_desc *i2c_hid_get_dmi_i2c_hid_desc_override(uint8_t *i2c_name);
char *i2c_hid_get_dmi_hid_report_desc_override(uint8_t *i2c_name,
					       unsigned int *size);
#else
static inline struct i2c_hid_desc
		   *i2c_hid_get_dmi_i2c_hid_desc_override(uint8_t *i2c_name)
{ return NULL; }
static inline char *i2c_hid_get_dmi_hid_report_desc_override(uint8_t *i2c_name,
							     unsigned int *size)
{ return NULL; }
#endif

/**
 * struct i2chid_subclass_data - Data passed from subclass to the core.
 *
 * @power_up_device: do sequencing to power up the device.
 * @power_down_device: do sequencing to power down the device.
 */
struct i2chid_subclass_data {
	int (*power_up_device)(struct i2chid_subclass_data *subclass);
	void (*power_down_device)(struct i2chid_subclass_data *subclass);
};

int i2c_hid_core_probe(struct i2c_client *client,
		       struct i2chid_subclass_data *subclass,
		       u16 hid_descriptor_address);
int i2c_hid_core_remove(struct i2c_client *client);

void i2c_hid_core_shutdown(struct i2c_client *client);

#ifdef CONFIG_PM_SLEEP
int i2c_hid_core_suspend(struct device *dev);
int i2c_hid_core_resume(struct device *dev);
#endif

#endif
