// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#include <linux/kernel.h>
#include <linux/module.h>

#include "t7xx_dev_node.h"
#include "t7xx_buffer_manager.h"

static void *dev_class;

int ccci_register_dev_node(const char *name, int major_id, int minor)
{
	int ret = 0;
	dev_t dev_n;
	struct device *dev;

	dev_n = MKDEV(major_id, minor);
	dev = device_create(dev_class, NULL, dev_n, NULL, "%s", name);

	if (IS_ERR(dev))
		ret = PTR_ERR(dev);

	return ret;
}

void ccci_unregister_dev_node(int major_id, int minor)
{
	device_destroy(dev_class, MKDEV(major_id, minor));
}

int ccci_init(void)
{
	dev_class = class_create(THIS_MODULE, "ccci_node");
	ccci_subsys_bm_init();

	return 0;
}

void ccci_uninit(void)
{
	ccci_subsys_bm_uninit();

	if (dev_class) {
		class_destroy(dev_class);
		dev_class = NULL;
	}
}
