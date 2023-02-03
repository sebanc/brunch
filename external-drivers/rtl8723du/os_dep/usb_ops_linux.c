// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _USB_OPS_LINUX_C_

#include <drv_types.h>
#include <hal_data.h>
#include <rtw_sreset.h>

struct rtw_async_write_data {
	u8 data[VENDOR_CMD_MAX_DATA_LEN];
	struct usb_ctrlrequest dr;
};

int usbctrl_vendorreq(struct intf_hdl *pintfhdl, u8 request, u16 value, u16 index, void *pdata, u16 len, u8 requesttype)
{
	struct adapter	*adapt = pintfhdl->adapt;
	struct dvobj_priv  *pdvobjpriv = adapter_to_dvobj(adapt);
	struct usb_device *udev = pdvobjpriv->pusbdev;
	unsigned int pipe;
	int status = 0;
	u8 reqtype;
	u8 *pIo_buf;
	int vendorreq_times = 0;

	if (RTW_CANNOT_IO(adapt)) {
		status = -EPERM;
		goto exit;
	}

	if (len > MAX_VENDOR_REQ_CMD_SIZE) {
		RTW_INFO("[%s] Buffer len error ,vendor request failed\n", __func__);
		status = -EINVAL;
		goto exit;
	}

	pIo_buf = pdvobjpriv->usb_vendor_req_buf;

	if (!pIo_buf) {
		RTW_INFO("[%s] pIo_buf is NULL\n", __func__);
		status = -ENOMEM;
		goto exit;
	}

	while (++vendorreq_times <= MAX_USBCTRL_VENDORREQ_TIMES) {
		memset(pIo_buf, 0, len);

		if (requesttype == 0x01) {
			pipe = usb_rcvctrlpipe(udev, 0);/* read_in */
			reqtype =  REALTEK_USB_VENQT_READ;
		} else {
			pipe = usb_sndctrlpipe(udev, 0);/* write_out */
			reqtype =  REALTEK_USB_VENQT_WRITE;
			memcpy(pIo_buf, pdata, len);
		}

		status = rtw_usb_control_msg(udev, pipe, request, reqtype, value, index, pIo_buf, len, RTW_USB_CONTROL_MSG_TIMEOUT);

		if (status == len) {  /* Success this control transfer. */
			rtw_reset_continual_io_error(pdvobjpriv);
			if (requesttype == 0x01) {
				/* For Control read transfer, we have to copy the read data from pIo_buf to pdata. */
				memcpy(pdata, pIo_buf,  len);
			}
		} else { /* error cases */
			RTW_INFO("reg 0x%x, usb %s %u fail, status:%d value=0x%x, vendorreq_times:%d\n"
				, value, (requesttype == 0x01) ? "read" : "write" , len, status, *(u32 *)pdata, vendorreq_times);

			if (status < 0) {
				if (status == (-ESHUTDOWN) || status == -ENODEV)
					rtw_set_surprise_removed(adapt);
			} else { /* status != len && status >= 0 */
				if (status > 0) {
					if (requesttype == 0x01) {
						/* For Control read transfer, we have to copy the read data from pIo_buf to pdata. */
						memcpy(pdata, pIo_buf,  len);
					}
				}
			}

			if (rtw_inc_and_chk_continual_io_error(pdvobjpriv)) {
				rtw_set_surprise_removed(adapt);
				break;
			}

		}

		/* firmware download is checksumed, don't retry */
		if ((value >= FW_START_ADDRESS) || status == len)
			break;

	}

exit:
	return status;

}

unsigned int ffaddr2pipehdl(struct dvobj_priv *pdvobj, u32 addr)
{
	unsigned int pipe = 0, ep_num = 0;
	struct usb_device *pusbd = pdvobj->pusbdev;

	if (addr == RECV_BULK_IN_ADDR)
		pipe = usb_rcvbulkpipe(pusbd, pdvobj->RtInPipe[0]);

	else if (addr == RECV_INT_IN_ADDR)
		pipe = usb_rcvintpipe(pusbd, pdvobj->RtInPipe[1]);

	else if (addr < HW_QUEUE_ENTRY) {
		ep_num = pdvobj->Queue2Pipe[addr];
		pipe = usb_sndbulkpipe(pusbd, ep_num);
	}

	return pipe;
}

struct zero_bulkout_context {
	void *pbuf;
	void *purb;
	void *pirp;
	void *adapt;
};

void usb_read_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
}

void usb_write_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{
}

void usb_read_port_cancel(struct intf_hdl *pintfhdl)
{
	int i;
	struct recv_buf *precvbuf;
	struct adapter	*adapt = pintfhdl->adapt;
	precvbuf = (struct recv_buf *)adapt->recvpriv.precv_buf;

	RTW_INFO("%s\n", __func__);

	for (i = 0; i < NR_RECVBUFF ; i++) {

		if (precvbuf->purb)	 {
			/* RTW_INFO("usb_read_port_cancel : usb_kill_urb\n"); */
			usb_kill_urb(precvbuf->purb);
		}
		precvbuf++;
	}
}

static void usb_write_port_complete(struct urb *purb, struct pt_regs *regs)
{
	unsigned long irqL;
	struct xmit_buf *pxmitbuf = (struct xmit_buf *)purb->context;
	struct adapter	*adapt = pxmitbuf->adapt;
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;

	switch (pxmitbuf->flags) {
	case VO_QUEUE_INX:
		pxmitpriv->voq_cnt--;
		break;
	case VI_QUEUE_INX:
		pxmitpriv->viq_cnt--;
		break;
	case BE_QUEUE_INX:
		pxmitpriv->beq_cnt--;
		break;
	case BK_QUEUE_INX:
		pxmitpriv->bkq_cnt--;
		break;
	default:
		break;
	}
	if (RTW_CANNOT_TX(adapt)) {
		RTW_INFO("%s(): TX Warning! bDriverStopped(%s) OR bSurpriseRemoved(%s) pxmitbuf->buf_tag(%x)\n"
			 , __func__
			 , rtw_is_drv_stopped(adapt) ? "True" : "False"
			 , rtw_is_surprise_removed(adapt) ? "True" : "False"
			 , pxmitbuf->buf_tag);

		goto check_completion;
	}


	if (purb->status == 0) {

	} else {
		RTW_INFO("###=> urb_write_port_complete status(%d)\n", purb->status);
		if ((purb->status == -EPIPE) || (purb->status == -EPROTO)) {
			sreset_set_wifi_error_status(adapt, USB_WRITE_PORT_FAIL);
		} else if (purb->status == -EINPROGRESS) {
			goto check_completion;
		} else if (purb->status == -ENOENT) {
			RTW_INFO("%s: -ENOENT\n", __func__);
			goto check_completion;
		} else if (purb->status == -ECONNRESET) {
			RTW_INFO("%s: -ECONNRESET\n", __func__);
			goto check_completion;
		} else if (purb->status == -ESHUTDOWN) {
			rtw_set_drv_stopped(adapt);
			goto check_completion;
		} else {
			rtw_set_surprise_removed(adapt);
			RTW_INFO("bSurpriseRemoved=true\n");
			goto check_completion;
		}
	}

check_completion:
	_enter_critical(&pxmitpriv->lock_sctx, &irqL);
	rtw_sctx_done_err(&pxmitbuf->sctx,
		purb->status ? RTW_SCTX_DONE_WRITE_PORT_ERR : RTW_SCTX_DONE_SUCCESS);
	_exit_critical(&pxmitpriv->lock_sctx, &irqL);

	rtw_free_xmitbuf(pxmitpriv, pxmitbuf);

	tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
}

u32 usb_write_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{
	unsigned long irqL;
	unsigned int pipe;
	int status;
	u32 ret = _FAIL;
	struct urb *	purb = NULL;
	struct adapter *adapt = (struct adapter *)pintfhdl->adapt;
	struct dvobj_priv	*pdvobj = adapter_to_dvobj(adapt);
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;
	struct xmit_buf *pxmitbuf = (struct xmit_buf *)wmem;
	struct xmit_frame *pxmitframe = (struct xmit_frame *)pxmitbuf->priv_data;
	struct usb_device *pusbd = pdvobj->pusbdev;

	if (RTW_CANNOT_TX(adapt)) {
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_TX_DENY);
		goto exit;
	}

	_enter_critical(&pxmitpriv->lock, &irqL);

	switch (addr) {
	case VO_QUEUE_INX:
		pxmitpriv->voq_cnt++;
		pxmitbuf->flags = VO_QUEUE_INX;
		break;
	case VI_QUEUE_INX:
		pxmitpriv->viq_cnt++;
		pxmitbuf->flags = VI_QUEUE_INX;
		break;
	case BE_QUEUE_INX:
		pxmitpriv->beq_cnt++;
		pxmitbuf->flags = BE_QUEUE_INX;
		break;
	case BK_QUEUE_INX:
		pxmitpriv->bkq_cnt++;
		pxmitbuf->flags = BK_QUEUE_INX;
		break;
	case HIGH_QUEUE_INX:
		pxmitbuf->flags = HIGH_QUEUE_INX;
		break;
	default:
		pxmitbuf->flags = MGT_QUEUE_INX;
		break;
	}

	_exit_critical(&pxmitpriv->lock, &irqL);

	purb	= pxmitbuf->pxmit_urb[0];

	/* translate DMA FIFO addr to pipehandle */
	pipe = ffaddr2pipehdl(pdvobj, addr);

	usb_fill_bulk_urb(purb, pusbd, pipe,
			  pxmitframe->buf_addr, /* = pxmitbuf->pbuf */
			  cnt,
			  usb_write_port_complete,
			  pxmitbuf);/* context is pxmitbuf */

#ifdef USB_PACKET_OFFSET_SZ
#if (USB_PACKET_OFFSET_SZ == 0)
	purb->transfer_flags |= URB_ZERO_PACKET;
#endif
#endif
	status = usb_submit_urb(purb, GFP_ATOMIC);
	if (status) {
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_WRITE_PORT_ERR);
		RTW_INFO("usb_write_port, status=%d\n", status);

		switch (status) {
		case -ENODEV:
			rtw_set_drv_stopped(adapt);
			break;
		default:
			break;
		}
		goto exit;
	}

	ret = _SUCCESS;

	/* Commented by Albert 2009/10/13
	 * We add the URB_ZERO_PACKET flag to urb so that the host will send the zero packet automatically. */
	/*
		if(bwritezero)
		{
			usb_bulkout_zero(pintfhdl, addr);
		}
	*/


exit:
	if (ret != _SUCCESS)
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
	return ret;

}

void usb_write_port_cancel(struct intf_hdl *pintfhdl)
{
	int i, j;
	struct adapter	*adapt = pintfhdl->adapt;
	struct xmit_buf *pxmitbuf = (struct xmit_buf *)adapt->xmitpriv.pxmitbuf;

	RTW_INFO("%s\n", __func__);

	for (i = 0; i < NR_XMITBUFF; i++) {
		for (j = 0; j < 8; j++) {
			if (pxmitbuf->pxmit_urb[j])
				usb_kill_urb(pxmitbuf->pxmit_urb[j]);
		}
		pxmitbuf++;
	}

	pxmitbuf = (struct xmit_buf *)adapt->xmitpriv.pxmit_extbuf;
	for (i = 0; i < NR_XMIT_EXTBUFF ; i++) {
		for (j = 0; j < 8; j++) {
			if (pxmitbuf->pxmit_urb[j])
				usb_kill_urb(pxmitbuf->pxmit_urb[j]);
		}
		pxmitbuf++;
	}
}

static void usb_init_recvbuf(struct adapter *adapt, struct recv_buf *precvbuf)
{

	precvbuf->transfer_len = 0;

	precvbuf->len = 0;

	precvbuf->ref_cnt = 0;

	if (precvbuf->pbuf) {
		precvbuf->pdata = precvbuf->phead = precvbuf->ptail = precvbuf->pbuf;
		precvbuf->pend = precvbuf->pdata + MAX_RECVBUF_SZ;
	}

}

void usb_recv_tasklet(void *priv)
{
	struct sk_buff			*pskb;
	struct adapter		*adapt = (struct adapter *)priv;
	struct recv_priv	*precvpriv = &adapt->recvpriv;
	struct recv_buf	*precvbuf = NULL;

	while (NULL != (pskb = skb_dequeue(&precvpriv->rx_skb_queue))) {
		if (RTW_CANNOT_RUN(adapt)) {
			RTW_INFO("recv_tasklet => bDriverStopped(%s) OR bSurpriseRemoved(%s)\n",
				 rtw_is_drv_stopped(adapt) ?
				 "True" : "False",
				 rtw_is_surprise_removed(adapt) ?
				 "True" : "False");
				dev_kfree_skb_any(pskb);
			break;
		}

		recvbuf2recvframe(adapt, pskb);

		skb_reset_tail_pointer(pskb);
		pskb->len = 0;

		skb_queue_tail(&precvpriv->free_recv_skb_queue, pskb);

		precvbuf = rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue);
		if (precvbuf) {
			precvbuf->pskb = NULL;
			rtw_read_port(adapt, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		}
	}
}

static void usb_read_port_complete(struct urb *purb, struct pt_regs *regs)
{
	struct recv_buf	*precvbuf = (struct recv_buf *)purb->context;
	struct adapter			*adapt = (struct adapter *)precvbuf->adapter;
	struct recv_priv	*precvpriv = &adapt->recvpriv;

	ATOMIC_DEC(&(precvpriv->rx_pending_cnt));

	if (RTW_CANNOT_RX(adapt)) {
		RTW_INFO("%s() RX Warning! bDriverStopped(%s) OR bSurpriseRemoved(%s)\n"
			, __func__
			, rtw_is_drv_stopped(adapt) ? "True" : "False"
			, rtw_is_surprise_removed(adapt) ? "True" : "False");
		goto exit;
	}

	if (purb->status == 0) {

		if ((purb->actual_length > MAX_RECVBUF_SZ) || (purb->actual_length < RXDESC_SIZE)) {
			RTW_INFO("%s()-%d: urb->actual_length:%u, MAX_RECVBUF_SZ:%u, RXDESC_SIZE:%u\n"
				, __func__, __LINE__, purb->actual_length, MAX_RECVBUF_SZ, RXDESC_SIZE);
			rtw_read_port(adapt, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		} else {
			rtw_reset_continual_io_error(adapter_to_dvobj(adapt));

			precvbuf->transfer_len = purb->actual_length;
			skb_put(precvbuf->pskb, purb->actual_length);
			skb_queue_tail(&precvpriv->rx_skb_queue, precvbuf->pskb);

			if (skb_queue_len(&precvpriv->rx_skb_queue) <= 1)
				tasklet_schedule(&precvpriv->recv_tasklet);

			precvbuf->pskb = NULL;
			rtw_read_port(adapt, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		}
	} else {

		RTW_INFO("###=> usb_read_port_complete => urb.status(%d)\n", purb->status);

		if (rtw_inc_and_chk_continual_io_error(adapter_to_dvobj(adapt)))
			rtw_set_surprise_removed(adapt);

		switch (purb->status) {
		case -EINVAL:
		case -EPIPE:
		case -ENODEV:
		case -ESHUTDOWN:
		case -ENOENT:
			rtw_set_drv_stopped(adapt);
			break;
		case -EPROTO:
		case -EILSEQ:
		case -ETIME:
		case -ECOMM:
		case -EOVERFLOW:
			rtw_read_port(adapt, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
			break;
		case -EINPROGRESS:
			RTW_INFO("ERROR: URB IS IN PROGRESS!/n");
			break;
		default:
			break;
		}
	}

exit:
	return;
}

u32 usb_read_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	int err;
	unsigned int pipe;
	u32 ret = _FAIL;
	struct urb * purb = NULL;
	struct recv_buf	*precvbuf = (struct recv_buf *)rmem;
	struct adapter		*adapter = pintfhdl->adapt;
	struct dvobj_priv	*pdvobj = adapter_to_dvobj(adapter);
	struct recv_priv	*precvpriv = &adapter->recvpriv;
	struct usb_device	*pusbd = pdvobj->pusbdev;

	if (RTW_CANNOT_RX(adapter) || (!precvbuf)) {
		goto exit;
	}

	usb_init_recvbuf(adapter, precvbuf);

	if (!precvbuf->pskb) {
		SIZE_PTR tmpaddr = 0;
		SIZE_PTR alignment = 0;

		precvbuf->pskb = skb_dequeue(&precvpriv->free_recv_skb_queue);
		if (NULL != precvbuf->pskb)
			goto recv_buf_hook;

		precvbuf->pskb = rtw_skb_alloc(MAX_RECVBUF_SZ + RECVBUFF_ALIGN_SZ);

		if (!precvbuf->pskb) {
			/* enqueue precvbuf and wait for free skb */
			rtw_enqueue_recvbuf(precvbuf, &precvpriv->recv_buf_pending_queue);
			goto exit;
		}

		tmpaddr = (SIZE_PTR)precvbuf->pskb->data;
		alignment = tmpaddr & (RECVBUFF_ALIGN_SZ - 1);
		skb_reserve(precvbuf->pskb, (RECVBUFF_ALIGN_SZ - alignment));
	}

recv_buf_hook:
	precvbuf->phead = precvbuf->pskb->head;
	precvbuf->pdata = precvbuf->pskb->data;
	precvbuf->ptail = skb_tail_pointer(precvbuf->pskb);
	precvbuf->pend = skb_end_pointer(precvbuf->pskb);
	precvbuf->pbuf = precvbuf->pskb->data;

	purb = precvbuf->purb;

	/* translate DMA FIFO addr to pipehandle */
	pipe = ffaddr2pipehdl(pdvobj, addr);

	usb_fill_bulk_urb(purb, pusbd, pipe,
		precvbuf->pbuf,
		MAX_RECVBUF_SZ,
		usb_read_port_complete,
		precvbuf);

	err = usb_submit_urb(purb, GFP_ATOMIC);
	if (err && err != (-EPERM)) {
		RTW_INFO("cannot submit rx in-token(err = 0x%08x),urb_status = %d\n"
			, err, purb->status);
		goto exit;
	}

	ATOMIC_INC(&(precvpriv->rx_pending_cnt));
	ret = _SUCCESS;

exit:


	return ret;
}
