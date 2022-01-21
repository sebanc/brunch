/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2014 Intel Corporation
 */
#ifndef __iwl_dnt_dispatch_h__
#define __iwl_dnt_dispatch_h__

#include "iwl-debug.h"
#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-config.h"


/**
 * iwl_dnt_dispatch_pull - pulling debug data.
 */
int iwl_dnt_dispatch_pull(struct iwl_trans *trans, u8 *buffer, u32 buffer_size,
			  u32 input);

int iwl_dnt_dispatch_collect_ucode_message(struct iwl_trans *trans,
					   struct iwl_rx_cmd_buffer *rxb);

void iwl_dnt_dispatch_free(struct iwl_dnt *dnt, struct iwl_trans *trans);

struct dnt_collect_db *iwl_dnt_dispatch_allocate_collect_db(
							struct iwl_dnt *dnt);

void iwl_dnt_dispatch_handle_nic_err(struct iwl_trans *trans);
#endif
