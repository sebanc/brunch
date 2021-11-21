/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __USB_HAL_H__
#define __USB_HAL_H__

int usb_init_recv_priv(struct adapter *adapt, u16 ini_in_buf_sz);
void usb_free_recv_priv(struct adapter *adapt, u16 ini_in_buf_sz);
#ifdef CONFIG_FW_C2H_REG
void usb_c2h_hisr_hdl(struct adapter *adapter, u8 *buf);
#endif

u8 rtw_set_hal_ops(struct adapter *adapt);

void rtl8723du_set_hal_ops(struct adapter *adapt);

#endif /* __USB_HAL_H__ */
