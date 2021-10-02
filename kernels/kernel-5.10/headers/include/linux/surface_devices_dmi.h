/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Macros to simplify Microsoft Surface devices specific patches.
 */

#ifndef _LINUX_SURFACE_DEVICES_DMI_H_
#define _LINUX_SURFACE_DEVICES_DMI_H_

#include <linux/dmi.h>

#define surface_all_devices \
	{ \
		{ \
			.ident = "Microsoft Surface devices", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_MATCH(DMI_PRODUCT_NAME, "Surface"), \
			}, \
		}, \
		{} \
	}

#define surface_3_devices \
	{ \
		{ \
			.ident = "Microsoft Surface 3", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface 3"), \
			}, \
		}, \
		{} \
	}

#define surface_mwifiex_pcie_devices \
	{ \
		{ \
			.ident = "Microsoft Surface 3", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface 3"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Pro 3", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Pro 3"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Pro 4", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Pro 4"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Pro 2017", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Pro"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Pro 6", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Pro 6"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Book 1", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Book"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Book 2", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Book 2"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Laptop 1", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Laptop"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Laptop 2", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Laptop 2"), \
			}, \
		}, \
		{ \
			.ident = "Microsoft Surface Studio 1", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Studio"), \
			}, \
		}, \
		{} \
	}

#define surface_go_devices \
	{ \
		{ \
			.ident = "Microsoft Surface Go 1", \
			.matches = { \
				DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"), \
				DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Go"), \
			}, \
		}, \
		{} \
	}

#endif /* _LINUX_SURFACE_DEVICES_DMI_H_ */
