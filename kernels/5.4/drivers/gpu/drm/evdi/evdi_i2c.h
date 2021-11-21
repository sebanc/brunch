/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_I2C_H
#define EVDI_I2C_H

#include <linux/module.h>
#include <linux/i2c.h>

int evdi_i2c_add(struct i2c_adapter *adapter,
		struct device *parent,
		void *ddev);
void evdi_i2c_remove(struct i2c_adapter *adapter);

#endif  /* EVDI_I2C_H */
