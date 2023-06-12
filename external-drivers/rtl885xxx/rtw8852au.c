// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2018-2021  Realtek Corporation
 */

#include <linux/module.h>
#include <linux/usb.h>
#include "usb.h"
#include "main.h"
#include "rtw8852a.h"

static const struct usb_device_id rtw_8822au_id_table[] = {
        /*=== Realtek demoboard ===*/
        {USB_DEVICE_AND_INTERFACE_INFO(USB_VENDOR_ID_REALTEK, 0x8832, 0xff, 0xff, 0xff),
	 .driver_info = (kernel_ulong_t)&rtw89_8852au_info,
        {USB_DEVICE_AND_INTERFACE_INFO(USB_VENDOR_ID_REALTEK, 0x885A, 0xff, 0xff, 0xff),
	 .driver_info = (kernel_ulong_t)&rtw89_8852au_info},
        {USB_DEVICE_AND_INTERFACE_INFO(USB_VENDOR_ID_REALTEK, 0x885C, 0xff, 0xff, 0xff),
	 .driver_info = (kernel_ulong_t)&rtw89_8852au_info},

        /*=== ASUS USB-AX56 =======*/
        {USB_DEVICE_AND_INTERFACE_INFO(USB_VENDOR_ID_ASUS, 0x1997, 0xff, 0xff, 0xff),
	 .driver_info = (kernel_ulong_t)&rtw89_8852au_info},

        /*=== BUFFALO WI-U3-1200AX2(/N) =======*/
        {USB_DEVICE_AND_INTERFACE_INFO(USB_VENDOR_ID_BUFFALO, 0x0312, 0xff, 0xff, 0xff),
	 .driver_info = (kernel_ulong_t)&rtw89_8852au_info},
        
        /*=== D-Link DWA-X1850 ====*/
        {USB_DEVICE_AND_INTERFACE_INFO(USB_VENDOR_ID_DLINK, 0x3321, 0xff, 0xff, 0xff),
	 .driver_info = (kernel_ulong_t)&rtw89_8852au_info},
	{},
};
MODULE_DEVICE_TABLE(usb, rtw89_8852au_id_table);

static int rtw8852au_probe(struct usb_interface *intf,
			   const struct usb_device_id *id)
{
	return rtw_usb_probe(intf, id);
}

static struct usb_driver rtw89_8852au_driver = {
	.name = "rtw89_8852au",
	.id_table = rtw89_8852au_id_table,
	.probe = rtw8822au_probe,
	.disconnect = rtw89_usb_disconnect,
};
module_usb_driver(rtw89_8852a_driver);

MODULE_AUTHOR("Realtek Corporation");
MODULE_DESCRIPTION("Realtek 802.11ax wireless 8852AU driver");
MODULE_LICENSE("Dual BSD/GPL");
