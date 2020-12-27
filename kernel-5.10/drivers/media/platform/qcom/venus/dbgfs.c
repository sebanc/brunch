// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Linaro Ltd.
 */

#include <linux/debugfs.h>

#include "core.h"

static int trigger_ssr_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t trigger_ssr_write(struct file *filp, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct venus_core *core = filp->private_data;
	u32 ssr_type;
	int ret;

	ret = kstrtou32_from_user(buf, count, 4, &ssr_type);
	if (ret)
		return ret;

	ret = hfi_core_trigger_ssr(core, ssr_type);
	if (ret < 0)
		return ret;

	return count;
}

static const struct file_operations ssr_fops = {
	.open = trigger_ssr_open,
	.write = trigger_ssr_write,
};

void venus_dbgfs_init(struct venus_core *core)
{
	core->root = debugfs_create_dir("venus", NULL);
	debugfs_create_x32("fw_level", 0644, core->root, &venus_fw_debug);
	debugfs_create_file("trigger_ssr", 0200, core->root, core, &ssr_fops);
}

void venus_dbgfs_deinit(struct venus_core *core)
{
	debugfs_remove_recursive(core->root);
}
