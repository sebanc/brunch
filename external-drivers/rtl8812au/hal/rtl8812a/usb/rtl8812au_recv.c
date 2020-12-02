/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
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
 *****************************************************************************/
#define _RTL8812AU_RECV_C_

/* #include <drv_types.h> */
#include <rtl8812a_hal.h>

int	rtl8812au_init_recv_priv(_adapter *padapter)
{
	return usb_init_recv_priv(padapter, INTERRUPT_MSG_FORMAT_LEN);
}

void rtl8812au_free_recv_priv(_adapter *padapter)
{
	usb_free_recv_priv(padapter, INTERRUPT_MSG_FORMAT_LEN);
}
