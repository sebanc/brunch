// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _HAL_USB_C_

#include <drv_types.h>
#include <hal_data.h>

int	usb_init_recv_priv(struct adapter *adapt, u16 ini_in_buf_sz)
{
	struct recv_priv	*precvpriv = &adapt->recvpriv;
	int	i, res = _SUCCESS;
	struct recv_buf *precvbuf;

	tasklet_init(&precvpriv->recv_tasklet,
		     (void(*))usb_recv_tasklet,
		     (unsigned long)adapt);

	/* init recv_buf */
	_rtw_init_queue(&precvpriv->free_recv_buf_queue);
	_rtw_init_queue(&precvpriv->recv_buf_pending_queue);
	/* this is used only when RX_IOBUF is sk_buff */
	skb_queue_head_init(&precvpriv->free_recv_skb_queue);

	RTW_INFO("NR_RECVBUFF: %d\n", NR_RECVBUFF);
	RTW_INFO("MAX_RECVBUF_SZ: %d\n", MAX_RECVBUF_SZ);
	precvpriv->pallocated_recv_buf = rtw_zmalloc(NR_RECVBUFF * sizeof(struct recv_buf) + 4);
	if (!precvpriv->pallocated_recv_buf) {
		res = _FAIL;
		goto exit;
	}

	precvpriv->precv_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(precvpriv->pallocated_recv_buf), 4);

	precvbuf = (struct recv_buf *)precvpriv->precv_buf;

	for (i = 0; i < NR_RECVBUFF ; i++) {
		INIT_LIST_HEAD(&precvbuf->list);

		spin_lock_init(&precvbuf->recvbuf_lock);

		precvbuf->alloc_sz = MAX_RECVBUF_SZ;

		res = rtw_os_recvbuf_resource_alloc(adapt, precvbuf);
		if (res == _FAIL)
			break;

		precvbuf->ref_cnt = 0;
		precvbuf->adapter = adapt;

		/* list_add_tail(&precvbuf->list, &(precvpriv->free_recv_buf_queue.queue)); */

		precvbuf++;
	}

	precvpriv->free_recv_buf_queue_cnt = NR_RECVBUFF;

	skb_queue_head_init(&precvpriv->rx_skb_queue);

#ifdef CONFIG_RX_INDICATE_QUEUE
	memset(&precvpriv->rx_indicate_queue, 0, sizeof(struct ifqueue));
	mtx_init(&precvpriv->rx_indicate_queue.ifq_mtx, "rx_indicate_queue", NULL, MTX_DEF);
#endif /* CONFIG_RX_INDICATE_QUEUE */

	{
		int i;
		SIZE_PTR tmpaddr = 0;
		SIZE_PTR alignment = 0;
		struct sk_buff *pskb = NULL;

		RTW_INFO("NR_PREALLOC_RECV_SKB: %d\n", NR_PREALLOC_RECV_SKB);

		for (i = 0; i < NR_PREALLOC_RECV_SKB; i++) {
			pskb = rtw_skb_alloc(MAX_RECVBUF_SZ + RECVBUFF_ALIGN_SZ);
			if (pskb) {
				pskb->dev = adapt->pnetdev;

				tmpaddr = (SIZE_PTR)pskb->data;
				alignment = tmpaddr & (RECVBUFF_ALIGN_SZ - 1);
				skb_reserve(pskb, (RECVBUFF_ALIGN_SZ - alignment));
				skb_queue_tail(&precvpriv->free_recv_skb_queue, pskb);
			}
		}
	}

exit:

	return res;
}

void usb_free_recv_priv(struct adapter *adapt, u16 ini_in_buf_sz)
{
	int i;
	struct recv_buf *precvbuf;
	struct recv_priv	*precvpriv = &adapt->recvpriv;

	precvbuf = (struct recv_buf *)precvpriv->precv_buf;

	for (i = 0; i < NR_RECVBUFF ; i++) {
		rtw_os_recvbuf_resource_free(adapt, precvbuf);
		precvbuf++;
	}

	if (precvpriv->pallocated_recv_buf)
		rtw_mfree(precvpriv->pallocated_recv_buf, NR_RECVBUFF * sizeof(struct recv_buf) + 4);

	if (skb_queue_len(&precvpriv->rx_skb_queue))
		RTW_WARN("rx_skb_queue not empty\n");

	rtw_skb_queue_purge(&precvpriv->rx_skb_queue);

	if (skb_queue_len(&precvpriv->free_recv_skb_queue))
		RTW_WARN("free_recv_skb_queue not empty, %d\n", skb_queue_len(&precvpriv->free_recv_skb_queue));

	rtw_skb_queue_purge(&precvpriv->free_recv_skb_queue);
}

#ifdef CONFIG_FW_C2H_REG
void usb_c2h_hisr_hdl(struct adapter *adapter, u8 *buf)
{
	u8 *c2h_evt = buf;
	u8 id, seq, plen;
	u8 *payload;

	if (rtw_hal_c2h_reg_hdr_parse(adapter, buf, &id, &seq, &plen, &payload) != _SUCCESS)
		return;

	if (rtw_hal_c2h_id_handle_directly(adapter, id, seq, plen, payload)) {
		/* Handle directly */
		rtw_hal_c2h_handler(adapter, id, seq, plen, payload);

		/* Replace with special pointer to trigger c2h_evt_clear only */
		if (rtw_cbuf_push(adapter->evtpriv.c2h_queue, (void*)&adapter->evtpriv) != _SUCCESS)
			RTW_ERR("%s rtw_cbuf_push fail\n", __func__);
	} else {
		c2h_evt = rtw_malloc(C2H_REG_LEN);
		if (c2h_evt) {
			memcpy(c2h_evt, buf, C2H_REG_LEN);
			if (rtw_cbuf_push(adapter->evtpriv.c2h_queue, (void*)c2h_evt) != _SUCCESS)
				RTW_ERR("%s rtw_cbuf_push fail\n", __func__);
		} else {
			/* Error handling for malloc fail */
			if (rtw_cbuf_push(adapter->evtpriv.c2h_queue, (void*)NULL) != _SUCCESS)
				RTW_ERR("%s rtw_cbuf_push fail\n", __func__);
		}
	}
	_set_workitem(&adapter->evtpriv.c2h_wk);
}
#endif

u8 usb_read8(struct intf_hdl *pintfhdl, u32 addr)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u8 data = 0;


	request = 0x05;
	requesttype = 0x01;/* read_in */
	index = 0;/* n/a */

	wvalue = (u16)(addr & 0x0000ffff);
	len = 1;
	usbctrl_vendorreq(pintfhdl, request, wvalue, index,
			  &data, len, requesttype);


	return data;
}

u16 usb_read16(struct intf_hdl *pintfhdl, u32 addr)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	__le32 data = 0;

	request = 0x05;
	requesttype = 0x01;/* read_in */
	index = 0;/* n/a */

	wvalue = (u16)(addr & 0x0000ffff);
	len = 2;
	usbctrl_vendorreq(pintfhdl, request, wvalue, index,
			  &data, len, requesttype);

	return (u16)(le32_to_cpu(data)&0xffff);
}

u32 usb_read32(struct intf_hdl *pintfhdl, u32 addr)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	__le32 data = 0;


	request = 0x05;
	requesttype = 0x01;/* read_in */
	index = 0;/* n/a */

	wvalue = (u16)(addr & 0x0000ffff);
	len = 4;
	usbctrl_vendorreq(pintfhdl, request, wvalue, index,
			  &data, len, requesttype);

	return le32_to_cpu(data);
}

int usb_write8(struct intf_hdl *pintfhdl, u32 addr, u8 val)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u8 data;
	int ret;


	request = 0x05;
	requesttype = 0x00;/* write_out */
	index = 0;/* n/a */

	wvalue = (u16)(addr & 0x0000ffff);
	len = 1;

	data = val;
	ret = usbctrl_vendorreq(pintfhdl, request, wvalue, index,
				&data, len, requesttype);


	return ret;
}

int usb_write16(struct intf_hdl *pintfhdl, u32 addr, u16 val)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	__le32 data;
	int ret;


	request = 0x05;
	requesttype = 0x00;/* write_out */
	index = 0;/* n/a */

	wvalue = (u16)(addr & 0x0000ffff);
	len = 2;

	data = cpu_to_le32(val & 0x0000ffff);
	ret = usbctrl_vendorreq(pintfhdl, request, wvalue, index,
				&data, len, requesttype);


	return ret;

}

int usb_write32(struct intf_hdl *pintfhdl, u32 addr, u32 val)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	__le32 data;
	int ret;


	request = 0x05;
	requesttype = 0x00;/* write_out */
	index = 0;/* n/a */

	wvalue = (u16)(addr & 0x0000ffff);
	len = 4;
	data = cpu_to_le32(val);
	ret = usbctrl_vendorreq(pintfhdl, request, wvalue, index,
				&data, len, requesttype);


	return ret;

}

int usb_writeN(struct intf_hdl *pintfhdl, u32 addr, u32 length, u8 *pdata)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u8 buf[VENDOR_CMD_MAX_DATA_LEN] = {0};
	int ret;


	request = 0x05;
	requesttype = 0x00;/* write_out */
	index = 0;/* n/a */

	wvalue = (u16)(addr & 0x0000ffff);
	len = length;
	memcpy(buf, pdata, len);
	ret = usbctrl_vendorreq(pintfhdl, request, wvalue, index,
				buf, len, requesttype);


	return ret;
}

void usb_set_intf_ops(struct adapter *adapt, struct _io_ops *pops)
{
	memset((u8 *)pops, 0, sizeof(struct _io_ops));

	pops->_read8 = &usb_read8;
	pops->_read16 = &usb_read16;
	pops->_read32 = &usb_read32;
	pops->_read_mem = &usb_read_mem;
	pops->_read_port = &usb_read_port;

	pops->_write8 = &usb_write8;
	pops->_write16 = &usb_write16;
	pops->_write32 = &usb_write32;
	pops->_writeN = &usb_writeN;

	pops->_write_mem = &usb_write_mem;
	pops->_write_port = &usb_write_port;

	pops->_read_port_cancel = &usb_read_port_cancel;
	pops->_write_port_cancel = &usb_write_port_cancel;
}
