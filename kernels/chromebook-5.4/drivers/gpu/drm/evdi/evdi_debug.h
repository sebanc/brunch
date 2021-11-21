/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2015 - 2016 DisplayLink (UK) Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#ifndef EVDI_DEBUG_H
#define EVDI_DEBUG_H

#define EVDI_LOGLEVEL_ALWAYS  0
#define EVDI_LOGLEVEL_FATAL   1
#define EVDI_LOGLEVEL_ERROR   2
#define EVDI_LOGLEVEL_WARN    3
#define EVDI_LOGLEVEL_INFO    4
#define EVDI_LOGLEVEL_DEBUG   5
#define EVDI_LOGLEVEL_VERBOSE 6

extern unsigned int evdi_loglevel;

#define EVDI_PRINTK(KERN_LEVEL, lEVEL, FORMAT_STR, ...)	do { \
	if (lEVEL <= evdi_loglevel) {\
		printk(KERN_LEVEL "evdi: " FORMAT_STR, ##__VA_ARGS__); \
	} \
} while (0)

#define EVDI_FATAL(FORMAT_STR, ...) \
	EVDI_PRINTK(KERN_CRIT, EVDI_LOGLEVEL_FATAL,\
		    "[F] %s:%d " FORMAT_STR, __func__, __LINE__, ##__VA_ARGS__)

#define EVDI_ERROR(FORMAT_STR, ...) \
	EVDI_PRINTK(KERN_ERR, EVDI_LOGLEVEL_ERROR,\
		    "[E] %s:%d " FORMAT_STR, __func__, __LINE__, ##__VA_ARGS__)

#define EVDI_WARN(FORMAT_STR, ...) \
	EVDI_PRINTK(KERN_WARNING, EVDI_LOGLEVEL_WARN,\
		    "[W] %s:%d " FORMAT_STR, __func__, __LINE__, ##__VA_ARGS__)

#define EVDI_INFO(FORMAT_STR, ...) \
	EVDI_PRINTK(KERN_DEFAULT, EVDI_LOGLEVEL_INFO,\
		    "[I] " FORMAT_STR, ##__VA_ARGS__)

#define EVDI_DEBUG(FORMAT_STR, ...) \
	EVDI_PRINTK(KERN_DEFAULT, EVDI_LOGLEVEL_DEBUG,\
		    "[D] %s:%d " FORMAT_STR, __func__, __LINE__, ##__VA_ARGS__)

#define EVDI_VERBOSE(FORMAT_STR, ...) \
	EVDI_PRINTK(KERN_DEFAULT, EVDI_LOGLEVEL_VERBOSE,\
		    "[V] %s:%d " FORMAT_STR, __func__, __LINE__, ##__VA_ARGS__)

#define EVDI_CHECKPT() EVDI_VERBOSE("\n")
#define EVDI_ENTER() EVDI_VERBOSE("enter\n")
#define EVDI_EXIT() EVDI_VERBOSE("exit\n")

#endif /* EVDI_DEBUG_H */

