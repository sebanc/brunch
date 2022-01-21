/* SPDX-License-Identifier: GPL-2.0-only
 * evdi_platform_drv.h
 *
 * Copyright (c) 2020 DisplayLink (UK) Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _EVDI_PLATFORM_DRV_H_
#define _EVDI_PLATFORM_DRV_H_

struct device;
struct platform_device_info;

#define DRIVER_NAME   "evdi"
#define DRIVER_DESC   "Extensible Virtual Display Interface"
#define DRIVER_DATE   "20210126"

#define DRIVER_MAJOR 1
#define DRIVER_MINOR 9
#define DRIVER_PATCH 1

void evdi_platform_remove_all_devices(struct device *device);
int evdi_platform_device_count(struct device *device);
int evdi_platform_add_devices(struct device *device, unsigned int val);
int evdi_platform_device_add(struct device *device, struct device *parent);

#endif

