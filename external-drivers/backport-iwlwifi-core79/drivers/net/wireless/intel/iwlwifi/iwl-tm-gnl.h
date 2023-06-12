/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2010-2014, 2018 Intel Corporation
 * Copyright (C) 2013-2014 Intel Mobile Communications GmbH
 */
#ifndef __IWL_TM_GNL_H__
#define __IWL_TM_GNL_H__

#include <linux/types.h>

#include "fw/testmode.h"

struct iwl_test_trace {
	u32 size;
	u8 *cpu_addr;
	dma_addr_t dma_addr;
	bool enabled;
};

struct iwl_test {
	struct iwl_test_trace trace;
	bool notify;
};


/**
 * struct iwl_tm_gnl_dev - Devices data base
 * @list:	  Linked list to all devices
 * @trans:	  Pointer to the owning transport
 * @dev_name:	  Pointer to the device name
 * @cmd_handlers: Operation mode specific command handlers.
 *
 * Used to retrieve a device op mode pointer.
 * Device identifier it's name.
 */
struct iwl_tm_gnl_dev {
	struct list_head list;
	struct iwl_test tst;
	struct iwl_dnt *dnt;
	struct iwl_trans *trans;
	const char *dev_name;
	u32 nl_events_portid;
};

int iwl_tm_gnl_send_msg(struct iwl_trans *trans, u32 cmd, bool check_notify,
			void *data_out, u32 data_len, gfp_t flags);

void iwl_tm_gnl_add(struct iwl_trans *trans);
void iwl_tm_gnl_remove(struct iwl_trans *trans);

int iwl_tm_gnl_init(void);
int iwl_tm_gnl_exit(void);
void iwl_tm_gnl_send_rx(struct iwl_trans *trans, struct iwl_rx_cmd_buffer *rxb);
#endif
