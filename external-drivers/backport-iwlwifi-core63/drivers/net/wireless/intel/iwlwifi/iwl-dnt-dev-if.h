/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2014 Intel Corporation
 * Copyright (C) 2014 Intel Mobile Communications GmbH
 */
#ifndef __iwl_dnt_dev_if_h__
#define __iwl_dnt_dev_if_h__

#include "iwl-drv.h"
#include "iwl-trans.h"
#include "iwl-op-mode.h"
#include "iwl-config.h"

struct iwl_dnt;

#define DNT_LDBG_CMD_SIZE	80
#define DNT_MARBH_BUF_SIZE	(0x3cff * sizeof(u32))
#define DNT_SMEM_BUF_SIZE	(0x18004)

#define DNT_CHUNK_SIZE 512

/* marbh access types */
enum {
	ACCESS_TYPE_DIRECT = 0,
	ACCESS_TYPE_INDIRECT,
};

/**
 * iwl_dnt_dev_if_configure_monitor - configure monitor.
 *
 * configure the correct monitor configuration - depends on the monitor mode.
 */
int iwl_dnt_dev_if_configure_monitor(struct iwl_dnt *dnt,
				     struct iwl_trans *trans);

/**
 * iwl_dnt_dev_if_retrieve_monitor_data - retrieve monitor data.
 *
 * retrieve monitor data - depends on the monitor mode.
 * Note: monitor must be stopped in order to retrieve data.
 */
int iwl_dnt_dev_if_retrieve_monitor_data(struct iwl_dnt *dnt,
					 struct iwl_trans *trans, u8 *buffer,
					 u32 buffer_size);
/**
 * iwl_dnt_dev_if_start_monitor - start monitor data.
 *
 * starts monitor - sends command to start monitor.
 */
int iwl_dnt_dev_if_start_monitor(struct iwl_dnt *dnt,
				 struct iwl_trans *trans);
/**
 * iwl_dnt_dev_if_set_log_level - set ucode messages log level.
 */
int iwl_dnt_dev_if_set_log_level(struct iwl_dnt *dnt,
				 struct iwl_trans *trans);

int iwl_dnt_dev_if_read_sram(struct iwl_dnt *dnt, struct iwl_trans *trans);

int iwl_dnt_dev_if_read_rx(struct iwl_dnt *dnt, struct iwl_trans *trans);

#endif
