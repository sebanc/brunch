/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __USB_OPS_H_
#define __USB_OPS_H_


#define REALTEK_USB_VENQT_READ		0xC0
#define REALTEK_USB_VENQT_WRITE	0x40
#define REALTEK_USB_VENQT_CMD_REQ	0x05
#define REALTEK_USB_VENQT_CMD_IDX	0x00
#define REALTEK_USB_IN_INT_EP_IDX	1

enum {
	VENDOR_WRITE = 0x00,
	VENDOR_READ = 0x01,
};
#define ALIGNMENT_UNIT				16
#define MAX_VENDOR_REQ_CMD_SIZE	254		/* 8188cu SIE Support */
#define MAX_USB_IO_CTL_SIZE		(MAX_VENDOR_REQ_CMD_SIZE + ALIGNMENT_UNIT)

#include <usb_ops_linux.h>

void usb_set_intf_ops(struct adapter *adapt, struct _io_ops *pops);

void rtl8723du_set_hw_type(struct dvobj_priv *pdvobj);
void rtl8723du_set_intf_ops(struct _io_ops *pops);
void rtl8723du_recv_tasklet(void *priv);
void rtl8723du_xmit_tasklet(void *priv);

enum RTW_USB_SPEED {
	RTW_USB_SPEED_UNKNOWN	= 0,
	RTW_USB_SPEED_1_1	= 1,
	RTW_USB_SPEED_2		= 2,
	RTW_USB_SPEED_3		= 3,
};

#define IS_FULL_SPEED_USB(Adapter)	(adapter_to_dvobj(Adapter)->usb_speed == RTW_USB_SPEED_1_1)
#define IS_HIGH_SPEED_USB(Adapter)	(adapter_to_dvobj(Adapter)->usb_speed == RTW_USB_SPEED_2)
#define IS_SUPER_SPEED_USB(Adapter)	(adapter_to_dvobj(Adapter)->usb_speed == RTW_USB_SPEED_3)

#define USB_SUPER_SPEED_BULK_SIZE	1024	/* usb 3.0 */
#define USB_HIGH_SPEED_BULK_SIZE	512		/* usb 2.0 */
#define USB_FULL_SPEED_BULK_SIZE	64		/* usb 1.1 */

static inline u8 rtw_usb_bulk_size_boundary(struct adapter *adapt, int buf_len)
{
	u8 rst = true;

	if (IS_SUPER_SPEED_USB(adapt))
		rst = (0 == (buf_len) % USB_SUPER_SPEED_BULK_SIZE) ? true : false;
	else if (IS_HIGH_SPEED_USB(adapt))
		rst = (0 == (buf_len) % USB_HIGH_SPEED_BULK_SIZE) ? true : false;
	else
		rst = (0 == (buf_len) % USB_FULL_SPEED_BULK_SIZE) ? true : false;
	return rst;
}


#endif /* __USB_OPS_H_ */
