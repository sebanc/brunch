// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTL8723DU_RECV_C_

#include <rtl8723d_hal.h>

int rtl8723du_init_recv_priv(struct adapter * adapt)
{
	return usb_init_recv_priv(adapt, USB_INTR_CONTENT_LENGTH);
}

void rtl8723du_free_recv_priv(struct adapter * adapt)
{
	usb_free_recv_priv(adapt, USB_INTR_CONTENT_LENGTH);
}
