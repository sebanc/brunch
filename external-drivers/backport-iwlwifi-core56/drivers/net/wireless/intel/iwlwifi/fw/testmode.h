/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * Copyright(c) 2018        Intel Corporation
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
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * Copyright(c) 2018        Intel Corporation
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

#ifndef __IWL_TESTMODE_H__
#define __IWL_TESTMODE_H__

#ifdef CPTCFG_NL80211_TESTMODE
/**
 * enum iwl_testmode_attrs - testmode attributes inside
 *	NL80211_ATTR_TESTDATA
 * @IWL_TM_ATTR_UNSPEC: (invalid attribute)
 * @IWL_TM_ATTR_CMD: sub command, see &enum iwl_testmode_commands (u32)
 * @IWL_TM_ATTR_NOA_DURATION: requested NoA duration (u32)
 * @IWL_TM_ATTR_BEACON_FILTER_STATE: beacon filter state (0 or 1, u32)
 * @NUM_IWL_TM_ATTRS: number of attributes in the enum
 * @IWL_TM_ATTR_MAX: max amount of attributes
 */
enum iwl_testmode_attrs {
	IWL_TM_ATTR_UNSPEC,
	IWL_TM_ATTR_CMD,
	IWL_TM_ATTR_NOA_DURATION,
	IWL_TM_ATTR_BEACON_FILTER_STATE,

	/* keep last */
	NUM_IWL_TM_ATTRS,
	IWL_TM_ATTR_MAX = NUM_IWL_TM_ATTRS - 1,
};

/**
 * enum iwl_testmode_commands - trans testmode commands
 * @IWL_TM_CMD_SET_NOA: set NoA on GO vif for testing
 * @IWL_TM_CMD_SET_BEACON_FILTER: turn beacon filtering off/on
 */
enum iwl_testmode_commands {
	IWL_TM_CMD_SET_NOA,
	IWL_TM_CMD_SET_BEACON_FILTER,
};
#endif

#ifdef CPTCFG_IWLWIFI_DEVICE_TESTMODE
struct iwl_host_cmd;
struct iwl_rx_cmd_buffer;

struct iwl_testmode {
	struct iwl_trans *trans;
	const struct iwl_fw *fw;
	/* the mutex of the op_mode */
	struct mutex *mutex;
	void *op_mode;
	int (*send_hcmd)(void *op_mode, struct iwl_host_cmd *host_cmd);
	u32 fw_major_ver;
	u32 fw_minor_ver;
};

/**
 * iwl_tm_data - A data packet for testmode usages
 * @data:   Pointer to be casted to relevant data type
 *          (According to usage)
 * @len:    Size of data in bytes
 *
 * This data structure is used for sending/receiving data packets
 * between internal testmode interfaces
 */
struct iwl_tm_data {
	void *data;
	u32 len;
};

void iwl_tm_init(struct iwl_trans *trans, const struct iwl_fw *fw,
		 struct mutex *mutex, void *op_mode);

void iwl_tm_set_fw_ver(struct iwl_trans *trans, u32 fw_major_ver,
		       u32 fw_minor_var);

int iwl_tm_execute_cmd(struct iwl_testmode *testmode, u32 cmd,
		       struct iwl_tm_data *data_in,
		       struct iwl_tm_data *data_out);

#define ADDR_IN_AL_MSK (0x80000000)
#define GET_AL_ADDR(ofs) (ofs & ~(ADDR_IN_AL_MSK))
#define IS_AL_ADDR(ofs) (!!(ofs & (ADDR_IN_AL_MSK)))
#endif

#endif /* __IWL_TESTMODE_H__ */
