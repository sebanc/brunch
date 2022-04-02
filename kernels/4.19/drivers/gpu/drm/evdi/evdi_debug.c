// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 - 2016 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>

#include "evdi_debug.h"

unsigned int evdi_loglevel = EVDI_LOGLEVEL_DEBUG;

module_param_named(initial_loglevel, evdi_loglevel, int, 0400);
MODULE_PARM_DESC(initial_loglevel, "Initial log level");
