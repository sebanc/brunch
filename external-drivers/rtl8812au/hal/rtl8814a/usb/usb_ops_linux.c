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
#define _HCI_OPS_OS_C_

/* #include <drv_types.h> */
#include <rtl8814a_hal.h>

#ifdef CONFIG_SUPPORT_USB_INT
void interrupt_handler_8814au(_adapter *padapter, u16 pkt_len, u8 *pbuf)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct reportpwrstate_parm pwr_rpt;

	if (pkt_len != INTERRUPT_MSG_FORMAT_LEN) {
		RTW_INFO("%s Invalid interrupt content length (%d)!\n", __FUNCTION__, pkt_len);
		return ;
	}

	/* HISR */
	_rtw_memcpy(&(pHalData->IntArray[0]), &(pbuf[USB_INTR_CONTENT_HISR_OFFSET]), 4);
	_rtw_memcpy(&(pHalData->IntArray[1]), &(pbuf[USB_INTR_CONTENT_HISRE_OFFSET]), 4);

#if 0 /*DBG*/
	{
		u32 hisr = 0 , hisr_ex = 0;
		_rtw_memcpy(&hisr, &(pHalData->IntArray[0]), 4);
		hisr = le32_to_cpu(hisr);

		_rtw_memcpy(&hisr_ex, &(pHalData->IntArray[1]), 4);
		hisr_ex = le32_to_cpu(hisr_ex);

		if ((hisr != 0) || (hisr_ex != 0))
			RTW_INFO("===> %s hisr:0x%08x ,hisr_ex:0x%08x\n", __FUNCTION__, hisr, hisr_ex);
	}
#endif

#ifdef CONFIG_LPS_LCLK
	if (pHalData->IntArray[0]  & IMR_CPWM_88E) {
		_rtw_memcpy(&pwr_rpt.state, &(pbuf[USB_INTR_CONTENT_CPWM1_OFFSET]), 1);
		/* _rtw_memcpy(&pwr_rpt.state2, &(pbuf[USB_INTR_CONTENT_CPWM2_OFFSET]), 1); */

		/* 88e's cpwm value only change BIT0, so driver need to add PS_STATE_S2 for LPS flow.		 */
		pwr_rpt.state |= PS_STATE_S2;
		_set_workitem(&(adapter_to_pwrctl(padapter)->cpwm_event));
	}
#endif/* CONFIG_LPS_LCLK */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
	if (pHalData->IntArray[0] & IMR_BCNDMAINT0_88E)
#endif
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
		if (pHalData->IntArray[0] & (IMR_TBDER_88E | IMR_TBDOK_88E))
#endif
		{
			struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
#if 0
			if (pHalData->IntArray[0] & IMR_BCNDMAINT0_88E)
				RTW_INFO("%s: HISR_BCNERLY_INT\n", __func__);
			if (pHalData->IntArray[0] & IMR_TBDOK_88E)
				RTW_INFO("%s: HISR_TXBCNOK\n", __func__);
			if (pHalData->IntArray[0] & IMR_TBDER_88E)
				RTW_INFO("%s: HISR_TXBCNERR\n", __func__);
#endif /* 0 */

			if(check_fwstate(pmlmepriv, WIFI_AP_STATE))
			{
				//send_beacon(padapter);
				if(pmlmepriv->update_bcn == _TRUE)
				{
					//tx_beacon_hdl(padapter, NULL);
					set_tx_beacon_cmd(padapter);
				}
			}
#ifdef CONFIG_CONCURRENT_MODE
			if(check_buddy_fwstate(padapter, WIFI_AP_STATE))
			{
				//send_beacon(padapter);
				if(padapter->pbuddy_adapter->mlmepriv.update_bcn == _TRUE)
				{
					//tx_beacon_hdl(padapter, NULL);
					set_tx_beacon_cmd(padapter->pbuddy_adapter);
				}
			}
#endif

		}
#endif /* CONFIG_INTERRUPT_BASED_TXBCN */




#ifdef DBG_CONFIG_ERROR_DETECT_INT
	if (pHalData->IntArray[1]  & IMR_TXERR_88E)
		RTW_INFO("===> %s Tx Error Flag Interrupt Status\n", __FUNCTION__);
	if (pHalData->IntArray[1]  & IMR_RXERR_88E)
		RTW_INFO("===> %s Rx Error Flag INT Status\n", __FUNCTION__);
	if (pHalData->IntArray[1]  & IMR_TXFOVW_88E)
		RTW_INFO("===> %s Transmit FIFO Overflow\n", __FUNCTION__);
	if (pHalData->IntArray[1]  & IMR_RXFOVW_88E)
		RTW_INFO("===> %s Receive FIFO Overflow\n", __FUNCTION__);
#endif/* DBG_CONFIG_ERROR_DETECT_INT */

#ifdef CONFIG_FW_C2H_REG
	/* C2H Event */
	if (pbuf[0] != 0)
		usb_c2h_hisr_hdl(padapter, pbuf);
#endif
}
#endif /* CONFIG_SUPPORT_USB_INT */
#if 0
int recvbuf2recvframe(PADAPTER padapter, void *ptr)
{
	u8	*pbuf;
	u8	pkt_cnt = 0;
	u32	pkt_offset;
	s32	transfer_len;
	u8				*pphy_status = NULL;
	union recv_frame	*precvframe = NULL;
	struct rx_pkt_attrib	*pattrib = NULL;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	_queue			*pfree_recv_queue = &precvpriv->free_recv_queue;
	_pkt *pskb;

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	pskb = NULL;
	transfer_len = (s32)((struct recv_buf *)ptr)->transfer_len;
	pbuf = ((struct recv_buf *)ptr)->pbuf;
#else
	pskb = (_pkt *)ptr;
	transfer_len = (s32)pskb->len;
	pbuf = pskb->data;
#endif/* CONFIG_USE_USB_BUFFER_ALLOC_RX */


#ifdef CONFIG_USB_RX_AGGREGATION
	pkt_cnt = GET_RX_STATUS_DESC_DMA_AGG_NUM_8814A(pbuf);
#endif

	do {
		precvframe = rtw_alloc_recvframe(pfree_recv_queue);
		if (precvframe == NULL) {
			RTW_INFO("%s()-%d: rtw_alloc_recvframe() failed! RX Drop!\n", __FUNCTION__, __LINE__);
			goto _exit_recvbuf2recvframe;
		}

		_rtw_init_listhead(&precvframe->u.hdr.list);
		precvframe->u.hdr.precvbuf = NULL;	/* can't access the precvbuf for new arch. */
		precvframe->u.hdr.len = 0;

		rtl8814_query_rx_desc_status(precvframe, pbuf);

		pattrib = &precvframe->u.hdr.attrib;

		if ((padapter->registrypriv.mp_mode == 0) && ((pattrib->crc_err) || (pattrib->icv_err))) {
			RTW_INFO("%s: RX Warning! crc_err=%d icv_err=%d, skip!\n", __FUNCTION__, pattrib->crc_err, pattrib->icv_err);

			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

		pkt_offset = RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz + pattrib->pkt_len;

		if ((pattrib->pkt_len <= 0) || (pkt_offset > transfer_len)) {
			RTW_INFO("%s()-%d: RX Warning!,pkt_len<=0 or pkt_offset> transfer_len\n", __FUNCTION__, __LINE__);
			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

#ifdef CONFIG_RX_PACKET_APPEND_FCS
                if (check_fwstate(&padapter->mlmepriv, WIFI_MONITOR_STATE) == _FALSE)
                        if ((pattrib->pkt_rpt_type == NORMAL_RX) && (pHalData->ReceiveConfig & RCR_APPFCS))
                                pattrib->pkt_len -= IEEE80211_FCS_LEN;
#endif
		if (rtw_os_alloc_recvframe(padapter, precvframe,
			(pbuf + pattrib->shift_sz + pattrib->drvinfo_sz + RXDESC_SIZE), pskb) == _FAIL) {
			rtw_free_recvframe(precvframe, pfree_recv_queue);

			goto _exit_recvbuf2recvframe;
		}

		recvframe_put(precvframe, pattrib->pkt_len);
		/* recvframe_pull(precvframe, drvinfo_sz + RXDESC_SIZE); */

		if(pattrib->pkt_rpt_type == NORMAL_RX)//Normal rx packet
		{
			if(pattrib->physt)
				pphy_status = (pbuf + RXDESC_OFFSET);

#ifdef CONFIG_CONCURRENT_MODE
			if(rtw_buddy_adapter_up(padapter))
			{
				if(pre_recv_entry(precvframe, pphy_status) != _SUCCESS)
				{
					RT_TRACE(_module_rtl871x_recv_c_,_drv_err_,
						("recvbuf2recvframe: recv_entry(precvframe) != _SUCCESS\n"));
				}
			}
#endif //CONFIG_CONCURRENT_MODE

			if(pattrib->physt && pphy_status)
				rx_query_phy_status(precvframe, pphy_status);

			if(rtw_recv_entry(precvframe) != _SUCCESS)
			{
				RT_TRACE(_module_rtl871x_recv_c_,_drv_err_,
					("recvbuf2recvframe: rtw_recv_entry(precvframe) != _SUCCESS\n"));
			}

		}
		else{ // pkt_rpt_type == TX_REPORT1-CCX, TX_REPORT2-TX RTP,HIS_REPORT-USB HISR RTP
			if (pattrib->pkt_rpt_type == C2H_PACKET) {
				//RTW_INFO("rx C2H_PACKET \n");
				rtw_hal_c2h_pkt_pre_hdl(padapter,precvframe->u.hdr.rx_data,pattrib->pkt_len);
			}
			rtw_free_recvframe(precvframe, pfree_recv_queue);
		}

#ifdef CONFIG_USB_RX_AGGREGATION
		/* jaguar 8-byte alignment */
		pkt_offset = (u16)_RND8(pkt_offset);
		pkt_cnt--;
		pbuf += pkt_offset;
#endif
		transfer_len -= pkt_offset;
		precvframe = NULL;

	} while (transfer_len > 0);

_exit_recvbuf2recvframe:

	return _SUCCESS;
}
#endif

void rtl8814au_xmit_tasklet(void *priv)
{
	int ret = _FALSE;
	_adapter *padapter = (_adapter*)priv;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;

	while(1)
	{
		if (RTW_CANNOT_TX(padapter))
		{
			RTW_INFO("xmit_tasklet => bDriverStopped or bSurpriseRemoved or bWritePortCancel\n");
			break;
		}

		if (rtw_xmit_ac_blocked(padapter) == _TRUE)
			break;

		ret = rtl8814au_xmitframe_complete(padapter, pxmitpriv, NULL);

		if(ret==_FALSE)
			break;

	}

}

void rtl8814au_set_intf_ops(struct _io_ops	*pops)
{
	_rtw_memset((u8 *)pops, 0, sizeof(struct _io_ops));

	pops->_read8 = &usb_read8;
	pops->_read16 = &usb_read16;
	pops->_read32 = &usb_read32;
	pops->_read_mem = &usb_read_mem;
	pops->_read_port = &usb_read_port;

	pops->_write8 = &usb_write8;
	pops->_write16 = &usb_write16;
	pops->_write32 = &usb_write32;
	pops->_writeN = &usb_writeN;

#ifdef CONFIG_USB_SUPPORT_ASYNC_VDN_REQ
	pops->_write8_async= &usb_async_write8;
	pops->_write16_async = &usb_async_write16;
	pops->_write32_async = &usb_async_write32;
#endif
	pops->_write_mem = &usb_write_mem;
	pops->_write_port = &usb_write_port;

	pops->_read_port_cancel = &usb_read_port_cancel;
	pops->_write_port_cancel = &usb_write_port_cancel;

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	pops->_read_interrupt = &usb_read_interrupt;
#endif

}

void rtl8814au_set_hw_type(struct dvobj_priv *pdvobj)
{
	pdvobj->HardwareType = HARDWARE_TYPE_RTL8814AU;
	RTW_INFO("CHIP TYPE: RTL8814\n");
}
