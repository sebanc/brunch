/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2005-2014, 2018 Intel Corporation
 */
#ifndef __user_infc_h__
#define __user_infc_h__

#include "iwl-tm-gnl.h"
#include "iwl-tm-infc.h"
#include "xvt.h"

/*
 * iwl_xvt_user_send_notif masks the usage of iwl-tm-gnl
 * If there is a need in replacing the interface, it
 * should be done only here.
 */
static inline int iwl_xvt_user_send_notif(struct iwl_xvt *xvt, u32 cmd,
					  void *data, u32 size, gfp_t flags)
{
	int err;
	IWL_DEBUG_INFO(xvt, "send user notification: cmd=0x%x, size=%d\n",
		       cmd, size);
	err = iwl_tm_gnl_send_msg(xvt->trans, cmd, false, data, size, flags);

	WARN_ONCE(err, "failed to send notification to user, err %d\n", err);
	return err;
}

void iwl_xvt_send_user_rx_notif(struct iwl_xvt *xvt,
				struct iwl_rx_cmd_buffer *rxb);

int iwl_xvt_user_cmd_execute(struct iwl_testmode *testmode, u32 cmd,
			     struct iwl_tm_data *data_in,
			     struct iwl_tm_data *data_out, bool *supported_cmd);

#endif
