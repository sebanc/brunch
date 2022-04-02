/******************************************************************************
 *
 * Copyright(c) 2016 - 2018 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/

#ifndef _HALMAC_PCIE_88XX_H_
#define _HALMAC_PCIE_88XX_H_

#include "../halmac_api.h"

#if HALMAC_88XX_SUPPORT

enum halmac_ret_status
init_pcie_cfg_88xx(struct halmac_adapter *adapter);

enum halmac_ret_status
deinit_pcie_cfg_88xx(struct halmac_adapter *adapter);

enum halmac_ret_status
cfg_pcie_rx_agg_88xx(struct halmac_adapter *adapter,
		     struct halmac_rxagg_cfg *cfg);

u8
reg_r8_pcie_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
reg_w8_pcie_88xx(struct halmac_adapter *adapter, u32 offset, u8 value);

u16
reg_r16_pcie_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
reg_w16_pcie_88xx(struct halmac_adapter *adapter, u32 offset, u16 value);

u32
reg_r32_pcie_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
reg_w32_pcie_88xx(struct halmac_adapter *adapter, u32 offset, u32 value);

enum halmac_ret_status
cfg_txagg_pcie_align_88xx(struct halmac_adapter *adapter, u8 enable,
			  u16 align_size);

enum halmac_ret_status
tx_allowed_pcie_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size);

u32
pcie_indirect_reg_r32_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
pcie_reg_rn_88xx(struct halmac_adapter *adapter, u32 offset, u32 size,
		 u8 *value);

enum halmac_ret_status
set_pcie_bulkout_num_88xx(struct halmac_adapter *adapter, u8 num);

enum halmac_ret_status
get_pcie_tx_addr_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
		      u32 *cmd53_addr);

enum halmac_ret_status
get_pcie_bulkout_id_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
			 u8 *id);

enum halmac_ret_status
mdio_write_88xx(struct halmac_adapter *adapter, u8 addr, u16 data, u8 speed);

u16
mdio_read_88xx(struct halmac_adapter *adapter, u8 addr, u8 speed);

enum halmac_ret_status
dbi_w32_88xx(struct halmac_adapter *adapter, u16 addr, u32 data);

u32
dbi_r32_88xx(struct halmac_adapter *adapter, u16 addr);

enum halmac_ret_status
dbi_w8_88xx(struct halmac_adapter *adapter, u16 addr, u8 data);

u8
dbi_r8_88xx(struct halmac_adapter *adapter, u16 addr);

enum halmac_ret_status
trxdma_check_idle_88xx(struct halmac_adapter *adapter);

void
en_ref_autok_88xx(struct halmac_adapter *dapter, u8 en);

#endif /* HALMAC_88XX_SUPPORT */

#endif/* _HALMAC_PCIE_88XX_H_ */
