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

#ifndef _HALMAC_USB_88XX_H_
#define _HALMAC_USB_88XX_H_

#include "../halmac_api.h"

#if HALMAC_88XX_SUPPORT

enum halmac_ret_status
init_usb_cfg_88xx(struct halmac_adapter *adapter);

enum halmac_ret_status
deinit_usb_cfg_88xx(struct halmac_adapter *adapter);

enum halmac_ret_status
cfg_usb_rx_agg_88xx(struct halmac_adapter *adapter,
		    struct halmac_rxagg_cfg *cfg);

u8
reg_r8_usb_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
reg_w8_usb_88xx(struct halmac_adapter *adapter, u32 offset, u8 value);

u16
reg_r16_usb_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
reg_w16_usb_88xx(struct halmac_adapter *adapter, u32 offset, u16 value);

u32
reg_r32_usb_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
reg_w32_usb_88xx(struct halmac_adapter *adapter, u32 offset, u32 value);

enum halmac_ret_status
set_usb_bulkout_num_88xx(struct halmac_adapter *adapter, u8 num);

enum halmac_ret_status
get_usb_bulkout_id_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
			u8 *id);

enum halmac_ret_status
cfg_txagg_usb_align_88xx(struct halmac_adapter *adapter, u8 enable,
			 u16 align_size);

enum halmac_ret_status
tx_allowed_usb_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size);

u32
usb_indirect_reg_r32_88xx(struct halmac_adapter *adapter, u32 offset);

enum halmac_ret_status
usb_reg_rn_88xx(struct halmac_adapter *adapter, u32 offset, u32 size,
		u8 *value);

enum halmac_ret_status
get_usb_tx_addr_88xx(struct halmac_adapter *adapter, u8 *buf, u32 size,
		     u32 *cmd53_addr);

enum halmac_ret_status
set_usb_mode_88xx(struct halmac_adapter *adapter, enum halmac_usb_mode mode);

enum halmac_ret_status
usbphy_write_88xx(struct halmac_adapter *adapter, u8 addr, u16 data, u8 speed);

u16
usbphy_read_88xx(struct halmac_adapter *adapter, u8 addr, u8 speed);

#endif /* HALMAC_88XX_SUPPORT */

#endif/* _HALMAC_API_88XX_USB_H_ */
