// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _USB_OPS_C_

#include <rtl8723d_hal.h>

int recvbuf2recvframe(struct adapter * adapt, void *ptr)
{
	u8 *pbuf;
	u32 pkt_offset;
	int transfer_len;
	u8 *pdata;
	union recv_frame *precvframe = NULL;
	struct rx_pkt_attrib *pattrib = NULL;
	struct hal_com_data * pHalData;
	struct recv_priv *precvpriv;
	struct __queue *pfree_recv_queue;
	struct sk_buff *pskb;


	pHalData = GET_HAL_DATA(adapt);
	precvpriv = &adapt->recvpriv;
	pfree_recv_queue = &precvpriv->free_recv_queue;

	pskb = (struct sk_buff *)ptr;
	transfer_len = (int)pskb->len;
	pbuf = pskb->data;

	do {
		precvframe = rtw_alloc_recvframe(pfree_recv_queue);
		if (!precvframe) {
			RTW_INFO("%s: rtw_alloc_recvframe() failed! RX Drop!\n", __func__);
			goto _exit_recvbuf2recvframe;
		}

		if (transfer_len > 1500)
			INIT_LIST_HEAD(&precvframe->u.hdr.list);
		precvframe->u.hdr.precvbuf = NULL;  /* can't access the precvbuf for new arch. */
		precvframe->u.hdr.len = 0;

		rtl8723d_query_rx_desc_status(precvframe, pbuf);

		pattrib = &precvframe->u.hdr.attrib;

		if ((adapt->registrypriv.mp_mode == 0)
		    && ((pattrib->crc_err) || (pattrib->icv_err))) {
			RTW_INFO("%s: RX Warning! crc_err=%d icv_err=%d, skip!\n",
				 __func__, pattrib->crc_err, pattrib->icv_err);

			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

		pkt_offset = RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz + le16_to_cpu(pattrib->pkt_len);
		if ((le16_to_cpu(pattrib->pkt_len) <= 0) || (pkt_offset > transfer_len)) {
			RTW_INFO("%s: RX Error! pkt_len=%d pkt_offset=%d transfer_len=%d\n",
				__func__, pattrib->pkt_len, pkt_offset, transfer_len);

			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

#ifdef CONFIG_RX_PACKET_APPEND_FCS
		if (!check_fwstate(&adapt->mlmepriv, WIFI_MONITOR_STATE))
			if ((pattrib->pkt_rpt_type == NORMAL_RX) && rtw_hal_rcr_check(adapt, RCR_APPFCS))
				pattrib->pkt_len -= IEEE80211_FCS_LEN;
#endif

		pdata = pbuf + RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz;
		if (rtw_os_alloc_recvframe(adapt, precvframe, pdata, pskb) == _FAIL) {
			RTW_INFO("%s: RX Error! rtw_os_alloc_recvframe FAIL!\n", __func__);

			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

		recvframe_put(precvframe, pattrib->pkt_len);

		if (pattrib->pkt_rpt_type == NORMAL_RX)
			pre_recv_entry(precvframe, pattrib->physt ? (pbuf + RXDESC_OFFSET) : NULL);
		else {
			if (pattrib->pkt_rpt_type == C2H_PACKET)
				rtw_hal_c2h_pkt_pre_hdl(adapt, precvframe->u.hdr.rx_data, le16_to_cpu(pattrib->pkt_len));
			else {
				RTW_INFO("%s: [WARNNING] RX type(%d) not be handled!\n",
					 __func__, pattrib->pkt_rpt_type);
			}
			rtw_free_recvframe(precvframe, pfree_recv_queue);
		}
		transfer_len -= pkt_offset;
		precvframe = NULL;
	} while (transfer_len > 0);

_exit_recvbuf2recvframe:

	return _SUCCESS;
}

void rtl8723du_xmit_tasklet(void *priv)
{
	int ret = false;
	struct adapter *adapt = (struct adapter *)priv;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	while (1) {
		if (RTW_CANNOT_TX(adapt)) {
			RTW_INFO("xmit_tasklet => bDriverStopped or bSurpriseRemoved or bWritePortCancel\n");
			break;
		}

		if (rtw_xmit_ac_blocked(adapt))
			break;

		ret = rtl8723du_xmitframe_complete(adapt, pxmitpriv, NULL);

		if (!ret)
			break;
	}
}

void rtl8723du_set_hw_type(struct dvobj_priv *pdvobj)
{
	pdvobj->HardwareType = HARDWARE_TYPE_RTL8723DU;
	RTW_INFO("CHIP TYPE: RTL8723DU\n");
}
