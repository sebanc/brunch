// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>

#include "evdi_params.h"
#include "evdi_debug.h"

unsigned int evdi_loglevel __read_mostly = EVDI_LOGLEVEL_DEBUG;
unsigned short int evdi_initial_device_count __read_mostly;

module_param_named(initial_loglevel, evdi_loglevel, int, 0400);
MODULE_PARM_DESC(initial_loglevel, "Initial log level");

module_param_named(initial_device_count,
		   evdi_initial_device_count, ushort, 0644);
MODULE_PARM_DESC(initial_device_count, "Initial DRM device count (default: 0)");
