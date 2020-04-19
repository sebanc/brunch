/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 - 2019 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2017 Intel Deutschland GmbH
 * Copyright (C) 2018 - 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#include "xvt.h"
#include "fw/dbg.h"

#define XVT_DEBUGFS_WRITE_WRAPPER(name, buflen, argtype)		\
static ssize_t _iwl_dbgfs_##name##_write(struct file *file,		\
					 const char __user *user_buf,	\
					 size_t count, loff_t *ppos)	\
{									\
	argtype *arg = file->private_data;				\
	char buf[buflen] = {};						\
	size_t buf_size = min(count, sizeof(buf) -  1);			\
									\
	if (copy_from_user(buf, user_buf, buf_size))			\
		return -EFAULT;						\
									\
	return iwl_dbgfs_##name##_write(arg, buf, buf_size, ppos);	\
}

#define _XVT_DEBUGFS_WRITE_FILE_OPS(name, buflen, argtype)		\
XVT_DEBUGFS_WRITE_WRAPPER(name, buflen, argtype)			\
static const struct file_operations iwl_dbgfs_##name##_ops = {		\
	.write = _iwl_dbgfs_##name##_write,				\
	.open = simple_open,						\
	.llseek = generic_file_llseek,					\
}

#define XVT_DEBUGFS_WRITE_FILE_OPS(name, bufsz) \
	_XVT_DEBUGFS_WRITE_FILE_OPS(name, bufsz, struct iwl_xvt)

#define XVT_DEBUGFS_ADD_FILE_ALIAS(alias, name, parent, mode) do {	\
	if (!debugfs_create_file(alias, mode, parent, xvt,	\
				 &iwl_dbgfs_##name##_ops))	\
		goto err;					\
	} while (0)

#define XVT_DEBUGFS_ADD_FILE(name, parent, mode) \
	XVT_DEBUGFS_ADD_FILE_ALIAS(#name, name, parent, mode)

static ssize_t iwl_dbgfs_fw_dbg_collect_write(struct iwl_xvt *xvt,
					      char *buf, size_t count,
					      loff_t *ppos)
{
	if (!(xvt->state == IWL_XVT_STATE_OPERATIONAL && xvt->fw_running))
		return -EIO;

	if (count == 0)
		return 0;

	iwl_dbg_tlv_time_point(&xvt->fwrt, IWL_FW_INI_TIME_POINT_USER_TRIGGER,
			       NULL);

	iwl_fw_dbg_collect(&xvt->fwrt, FW_DBG_TRIGGER_USER, buf,
			   (count - 1), NULL);

	return count;
}

static ssize_t iwl_dbgfs_fw_restart_write(struct iwl_xvt *xvt, char *buf,
					  size_t count, loff_t *ppos)
{
	int __maybe_unused ret;

	if (!xvt->fw_running)
		return -EIO;

	mutex_lock(&xvt->mutex);

	/* Take the return value, though failure is expected, for compilation */
	ret = iwl_xvt_send_cmd_pdu(xvt, REPLY_ERROR, 0, 0, NULL);

	mutex_unlock(&xvt->mutex);

	return count;
}

/* Device wide debugfs entries */
XVT_DEBUGFS_WRITE_FILE_OPS(fw_dbg_collect, 64);
XVT_DEBUGFS_WRITE_FILE_OPS(fw_restart, 10);

#ifdef CPTCFG_IWLWIFI_DEBUGFS
int iwl_xvt_dbgfs_register(struct iwl_xvt *xvt, struct dentry *dbgfs_dir)
{
	xvt->debugfs_dir = dbgfs_dir;

	XVT_DEBUGFS_ADD_FILE(fw_dbg_collect, xvt->debugfs_dir, S_IWUSR);
	XVT_DEBUGFS_ADD_FILE(fw_restart, xvt->debugfs_dir, S_IWUSR);

	return 0;
err:
	IWL_ERR(xvt, "Can't create the xvt debugfs directory\n");
	return -ENOMEM;
}
#endif /* CPTCFG_IWLWIFI_DEBUGFS */
