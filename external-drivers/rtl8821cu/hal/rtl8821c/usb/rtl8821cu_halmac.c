/******************************************************************************
 *
 * Copyright(c) 2016 - 2018 Realtek Corporation.
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
#define _RTL8821CU_HALMAC_C_

#include <drv_types.h>		/* struct dvobj_priv and etc. */
#include "../../hal_halmac.h"
#include "../rtl8821c.h"	 /* rtl8821c_cal_txdesc_chksum() */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)) || (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18))
#define usb_write_port_complete_not_xmitframe(purb, regs)	usb_write_port_complete_not_xmitframe(purb)
#endif
static void usb_write_port_complete_not_xmitframe(struct urb *purb, struct pt_regs *regs)
{

	if (purb->status == 0) {
		;
	} else {
		RTW_INFO("###=> urb_write_port_complete status(%d)\n", purb->status);
		if ((purb->status == -EPIPE) || (purb->status == -EPROTO)) {
			;
		} else if (purb->status == -EINPROGRESS) {
			goto check_completion;

		} else if (purb->status == -ENOENT) {
			RTW_INFO("%s: -ENOENT\n", __func__);
			goto check_completion;

		} else if (purb->status == -ECONNRESET) {
			RTW_INFO("%s: -ECONNRESET\n", __func__);
			goto check_completion;

		} else if (purb->status == -ESHUTDOWN) {
			RTW_INFO("%s: -ESHUTDOWN\n", __func__);
			goto check_completion;
		} else {
			goto check_completion;
		}
	}

check_completion:
	rtw_mfree(purb->transfer_buffer, purb->transfer_buffer_length);
	usb_free_urb(purb);
}

static u32 usb_write_port_not_xmitframe(struct dvobj_priv *d, u8 addr, u32 cnt, u8 *wmem)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	unsigned int pipe;
	int status;
	u32 ret = _FAIL, bwritezero = _FALSE;
	PURB	purb = NULL;
	struct dvobj_priv	*pdvobj = adapter_to_dvobj(padapter);
	struct usb_device *pusbd = pdvobj->pusbdev;


	purb	= usb_alloc_urb(0, GFP_KERNEL);
	if (purb == NULL) {
		RTW_ERR("purb == NULL\n");
		return _FAIL;
	}

	/* translate DMA FIFO addr to pipehandle */
	pipe = ffaddr2pipehdl(pdvobj, addr);


	usb_fill_bulk_urb(purb, pusbd, pipe,
			  wmem,
			  cnt,
			  usb_write_port_complete_not_xmitframe,
			  padapter);


#ifdef USB_PACKET_OFFSET_SZ
#if (USB_PACKET_OFFSET_SZ == 0)
	purb->transfer_flags |= URB_ZERO_PACKET;
#endif
#endif

#if 0
	if (bwritezero)
		purb->transfer_flags |= URB_ZERO_PACKET;
#endif

	status = usb_submit_urb(purb, GFP_ATOMIC);
	if (!status) {
		#ifdef DBG_CONFIG_ERROR_DETECT
		{
			HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

			pHalData->srestpriv.last_tx_time = rtw_get_current_time();
		}
		#endif
	} else {
		RTW_INFO("usb_write_port, status=%d\n", status);

		switch (status) {
		case -ENODEV:
			break;
		default:
			break;
		}
		goto exit;
	}

	ret = _SUCCESS;

exit:
	if (ret != _SUCCESS)
		usb_free_urb(purb);

	return ret;
}

static u8 usb_write_data_not_xmitframe(void *d, u8 *pBuf, u32 size, u8 qsel)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u32 desclen, len;
	u8 *buf;
	u8 ret;
	u8 addr;
	u8 add_pkt_offset = 0;


	rtw_halmac_get_tx_desc_size(pobj, &desclen);
	len = desclen + size;

	if (qsel == HALMAC_TXDESC_QSEL_BEACON) {

		if (len % pHalData->UsbBulkOutSize == 0)
			add_pkt_offset = 1;

		if (add_pkt_offset == 1)
			len = len+PACKET_OFFSET_SZ;

		buf = rtw_zmalloc(len);
		if (!buf)
			return _FALSE;

		if (add_pkt_offset == 1)
			_rtw_memcpy(buf + desclen + PACKET_OFFSET_SZ , pBuf, size);
		else
			_rtw_memcpy(buf + desclen, pBuf, size);

		SET_TX_DESC_TXPKTSIZE_8821C(buf, size);
		if (add_pkt_offset == 1) {
			SET_TX_DESC_OFFSET_8821C(buf, desclen+PACKET_OFFSET_SZ);
			SET_TX_DESC_PKT_OFFSET_8821C(buf, 1);
		} else
			SET_TX_DESC_OFFSET_8821C(buf, desclen);
	} else if (qsel == HALMAC_TXDESC_QSEL_H2C_CMD){

		buf = rtw_zmalloc(len);
		if (!buf)
			return _FALSE;

		_rtw_memcpy(buf + desclen, pBuf, size);

		SET_TX_DESC_TXPKTSIZE_8821C(buf, size);
	} else {

		RTW_ERR("%s: qsel may be error(%d)\n", __func__, qsel);

		return _FALSE;
	}

	SET_TX_DESC_QSEL_8821C(buf, qsel);
	rtl8821c_cal_txdesc_chksum(padapter, buf);

	addr = rtw_halmac_usb_get_bulkout_id(d, buf, len);
	ret = usb_write_port_not_xmitframe(d, addr, len , buf);
	if (_SUCCESS == ret) {
		ret = _TRUE;
	} else {
		RTW_ERR("%s , rtl8821cu_simple_write_port fail\n", __func__);
		rtw_mfree(buf , len);
		ret = _FALSE;
	}

	return ret;

}

static u8 usb_write_data_rsvd_page_normal(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct xmit_frame	*pcmdframe = NULL;
	struct pkt_attrib	*pattrib = NULL;
	u8 txdesoffset = 0;
	u8 *buf = NULL;

	if (size + TXDESC_OFFSET > MAX_CMDBUF_SZ) {
		RTW_INFO("%s: total buffer size(%d) > MAX_CMDSZE(%d)\n"
			 , __func__, size + TXDESC_OFFSET, MAX_CMDSZ);
		return _FALSE;
	}

	pcmdframe = rtw_alloc_cmdxmitframe(pxmitpriv);

	if (pcmdframe == NULL) {
		RTW_INFO("%s: alloc cmd frame fail!\n", __func__);
		return _FALSE;
	}

	txdesoffset = TXDESC_OFFSET;
	buf = pcmdframe->buf_addr;
	_rtw_memcpy((buf + txdesoffset), pBuf, size); /* shift desclen */

	/* update attribute */
	pattrib = &pcmdframe->attrib;
	update_mgntframe_attrib(padapter, pattrib);
	pattrib->qsel = HALMAC_TXDESC_QSEL_BEACON;
	pattrib->pktlen = size;
	pattrib->last_txcmdsz = size;

	/* fill tx desc in dump_mgntframe */
	dump_mgntframe(padapter, pcmdframe);

	return _TRUE;
}

static u8 usb_write_data_h2c_normal(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct xmit_frame	*pcmdframe = NULL;
	struct pkt_attrib	*pattrib = NULL;
	u32 desc_size = 0;
	u8 txdesoffset = 0;
	u8 *buf = NULL;

	if (size + TXDESC_OFFSET > MAX_XMIT_EXTBUF_SZ) {
		/* RTW_INFO("%s: total buffer size(%d) > MAX_XMIT_EXTBUF_SZ(%d)\n" */
		/*	, __func__, size + TXDESC_OFFSET, MAX_XMIT_EXTBUF_SZ); */
		RTW_INFO("%s: total buffer size(%d) > MAX_XMIT_EXTBUF_SZ(%d)\n"
			 , __func__, size + TXDESC_OFFSET, MAX_XMIT_EXTBUF_SZ);
		return _FALSE;
	}

	pcmdframe = alloc_mgtxmitframe(pxmitpriv);

	if (pcmdframe == NULL) {
		/* RTW_INFO("%s: alloc cmd frame fail!\n", __func__); */
		RTW_INFO("%s: alloc cmd frame fail!\n", __func__);
		return _FALSE;
	}

	rtw_halmac_get_tx_desc_size(pobj, &desc_size);
	txdesoffset = desc_size;
	buf = pcmdframe->buf_addr;
	_rtw_memcpy(buf + txdesoffset, pBuf, size); /* shift desclen */

	/* update attribute */
	pattrib = &pcmdframe->attrib;
	update_mgntframe_attrib(padapter, pattrib);
	pattrib->qsel = HALMAC_TXDESC_QSEL_H2C_CMD;
	pattrib->pktlen = size;
	pattrib->last_txcmdsz = size;

	/* fill tx desc in dump_mgntframe */
	dump_mgntframe(padapter, pcmdframe);

	return _TRUE;

}

static u8 usb_write_data_rsvd_page(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u8 ret;

	if (pHalData->not_xmitframe_fw_dl)
		ret = usb_write_data_not_xmitframe(d , pBuf , size, HALMAC_TXDESC_QSEL_BEACON);
	else
		ret = usb_write_data_rsvd_page_normal(d , pBuf , size);

	if (ret == _TRUE)
		return 1;
	return 0;
}

static u8 usb_write_data_h2c(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u8 ret;

	if (pHalData->not_xmitframe_fw_dl)
		ret = usb_write_data_not_xmitframe(d , pBuf , size, HALMAC_TXDESC_QSEL_H2C_CMD);
	else
		ret = usb_write_data_h2c_normal(d , pBuf , size);

	if (ret == _TRUE)
		return 1;
	return 0;
}

int rtl8821cu_halmac_init_adapter(PADAPTER padapter)
{
	struct dvobj_priv *d;
	struct halmac_platform_api *api;
	int err;


	d = adapter_to_dvobj(padapter);
	api = &rtw_halmac_platform_api;
	api->SEND_RSVD_PAGE = usb_write_data_rsvd_page;
	api->SEND_H2C_PKT = usb_write_data_h2c;

	err = rtw_halmac_init_adapter(d, api);

	return err;
}
