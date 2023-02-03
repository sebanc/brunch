/******************************************************************************
 *
 * Copyright(c) 2016 - 2017 Realtek Corporation.
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
#define _RTL8821CE_RECV_C_

#include <drv_types.h>		/* PADAPTER and etc. */
#include <hal_data.h>		/* HAL_DATA_TYPE */
#include "../../hal_halmac.h"	/* rtw_halmac_get_rx_desc_size() */
#include "../rtl8821c.h"
#include "rtl8821ce.h"

/* Debug Buffer Descriptor Ring */

/*#define BUF_DESC_DEBUG*/
#ifdef BUF_DESC_DEBUG
#define buf_desc_debug(...) RTW_INFO("BUF_DESC:" __VA_ARGS__)
#else
#define buf_desc_debug(...)  do {} while (0)
#endif

/*
 * Wait until rx data is ready
 *	return value: _SUCCESS if Rx packet is ready, _FAIL if not ready
 */

static u32 rtl8821ce_wait_rxrdy(_adapter *padapter,
				u8 *rx_bd, u16 rx_q_idx)
{
	struct recv_priv *r_priv = &padapter->recvpriv;
	u8 first_seg = 0, last_seg = 0;
	u16 total_len = 0, read_cnt = 0;

	static BOOLEAN start_rx = _FALSE;
	u16 status = _SUCCESS;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);

	if (rx_bd == NULL)
		return _FAIL;

	total_len = (u2Byte)GET_RX_BD_TOTALRXPKTSIZE(rx_bd);
	first_seg = (u1Byte)GET_RX_BD_FS(rx_bd);
	last_seg = (u1Byte)GET_RX_BD_LS(rx_bd);

	buf_desc_debug("RX:%s enter: rx_bd addr = %p, total_len=%d, first_seg=%d, last_seg=%d, read_cnt %d, index %d, address %p\n",
		       __func__,
		(u8 *)&r_priv->rx_ring[rx_q_idx].desc[r_priv->rx_ring[rx_q_idx].idx],
		       total_len, first_seg, last_seg, read_cnt,
		       r_priv->rx_ring[rx_q_idx].idx, rx_bd);

#if defined(USING_RX_TAG)
	/* Rx Tag not ported */
	if (!start_rx) {
		start_rx = _TRUE;
		pHalData->RxTag = 1;
	} else {
		while (total_len != (pHalData->RxTag + 1)) {

			read_cnt++;

			total_len = (u2Byte)GET_RX_BD_TOTALRXPKTSIZE(rx_bd);
			first_seg = (u1Byte)GET_RX_BD_FS(rx_bd);
			last_seg = (u1Byte)GET_RX_BD_LS(rx_bd);

			if (read_cnt > 10000) {
				pHalData->RxTag = total_len;
				break;
			}

			if (total_len == 0 && pHalData->RxTag == 0x1fff)
				break;
		}
		pHalData->RxTag = total_len;
	}
#else
	while (total_len == 0) {
		read_cnt++;

		total_len = (u2Byte) GET_RX_BD_TOTALRXPKTSIZE(rx_bd);
		first_seg = (u1Byte) GET_RX_BD_FS(rx_bd);
		last_seg = (u1Byte) GET_RX_BD_LS(rx_bd);

		if (read_cnt > 20) {
			status = _FAIL;
			break;
		}
	}
#endif

	buf_desc_debug("RX:%s exit total_len=%d, rx_tag = %d, first_seg=%d, last_seg=%d, read_cnt %d\n",
		       __func__, total_len, pHalData->RxTag,
		       first_seg, last_seg, read_cnt);

	return status;
}

/*
 * Check the number of rxdec to be handled between
 *   "index of RX queue descriptor maintained by host (write pointer)" and
 *   "index of RX queue descriptor maintained by hardware (read pointer)"
 */
static u16 rtl8821ce_check_rxdesc_remain(_adapter *padapter, int rx_queue_idx)
{
	struct recv_priv *r_priv = &padapter->recvpriv;
	u16 desc_idx_hw = 0, desc_idx_host = 0, num_rxdesc_to_handle = 0;
	u32 tmp_4bytes = 0;
	static BOOLEAN	start_rx = FALSE;


	tmp_4bytes = rtw_read32(padapter, REG_RXQ_RXBD_IDX_8821C);
	desc_idx_hw = (u16)((tmp_4bytes >> 16) & 0x7ff);
	desc_idx_host = (u16)(tmp_4bytes & 0x7ff);

	/*
	 * make sure driver does not handle packet if hardware pointer
	 * keeps in zero in initial state
	 */
	buf_desc_debug("RX:%s(%d) reg_value %x\n", __func__, __LINE__,
		       tmp_4bytes);

	if (desc_idx_hw > 0)
		start_rx = TRUE;

	if (!start_rx)
		return 0;

	if (desc_idx_hw < desc_idx_host)
		/* hw idx is turn around */
		num_rxdesc_to_handle = RX_BD_NUM_8821CE - desc_idx_host + desc_idx_hw;
	else
		num_rxdesc_to_handle = desc_idx_hw - desc_idx_host;

	if (num_rxdesc_to_handle == 0)
		return 0;

	r_priv->rx_ring[rx_queue_idx].idx = desc_idx_host;

	buf_desc_debug("RX:%s reg_val %x, hw_idx %x, host_idx %x, desc to handle = %d\n",
		__func__, tmp_4bytes, desc_idx_hw, desc_idx_host, num_rxdesc_to_handle);

	return num_rxdesc_to_handle;
}

static void rtl8821ce_rx_mpdu(_adapter *padapter)
{
	struct recv_priv *r_priv = &padapter->recvpriv;
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(padapter);
	_queue *pfree_recv_queue = &r_priv->free_recv_queue;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	union recv_frame *precvframe = NULL;
	struct rx_pkt_attrib *pattrib = NULL;
	int rx_q_idx = RX_MPDU_QUEUE;
	u32 count = r_priv->rxringcount;
	u16 remaing_rxdesc = 0;
	u8 *rx_bd;
	struct sk_buff *skb;
	u32 desc_size;

	rtw_halmac_get_rx_desc_size(adapter_to_dvobj(padapter), &desc_size);

	/* RX NORMAL PKT */

	remaing_rxdesc = rtl8821ce_check_rxdesc_remain(padapter, rx_q_idx);
	while (remaing_rxdesc) {

		/* rx descriptor */
		rx_bd = (u8 *)&r_priv->rx_ring[rx_q_idx].buf_desc[r_priv->rx_ring[rx_q_idx].idx];

		/* rx packet */
		skb = r_priv->rx_ring[rx_q_idx].rx_buf[r_priv->rx_ring[rx_q_idx].idx];

		buf_desc_debug("RX:%s(%d), rx_bd addr = %x, total_len = %d, ring idx = %d\n",
			       __func__, __LINE__, (u32)rx_bd,
			       GET_RX_BD_TOTALRXPKTSIZE(rx_bd),
			       r_priv->rx_ring[rx_q_idx].idx);

		buf_desc_debug("RX:%s(%d), skb(rx_buf)=%x, buf addr(virtual = %x, phisycal = %x)\n",
			       __func__, __LINE__, (u32)skb,
			       (u32)(skb_tail_pointer(skb)),
			       GET_RX_BD_PHYSICAL_ADDR_LOW(rx_bd));

		/* wait until packet is ready. this operation is similar to
		 * check own bit and should be called before pci_unmap_single
		 * which release memory mapping
		 */

		if (rtl8821ce_wait_rxrdy(padapter, rx_bd, rx_q_idx) !=
		    _SUCCESS)
			buf_desc_debug("RX:%s(%d) packet not ready\n",
				       __func__, __LINE__);

		{
			precvframe = rtw_alloc_recvframe(pfree_recv_queue);

			if (precvframe == NULL)
				goto done;

			_rtw_init_listhead(&precvframe->u.hdr.list);
			precvframe->u.hdr.len = 0;

			#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
			pci_unmap_single(pdvobjpriv->ppcidev,
					 *((dma_addr_t *)skb->cb),
					 r_priv->rxbuffersize,
					 PCI_DMA_FROMDEVICE);
			#else
			dma_unmap_single(&(pdvobjpriv->ppcidev)->dev,
					 *((dma_addr_t *)skb->cb),
					 r_priv->rxbuffersize,
					 DMA_FROM_DEVICE);
			#endif


			rtl8821c_query_rx_desc(precvframe, skb->data);
			pattrib = &precvframe->u.hdr.attrib;

#ifdef CONFIG_RX_PACKET_APPEND_FCS
			{
				struct mlme_priv *mlmepriv =
						&padapter->mlmepriv;

				if (check_fwstate(mlmepriv,
						  WIFI_MONITOR_STATE) == _FALSE)
					if (pattrib->pkt_rpt_type == NORMAL_RX)
						pattrib->pkt_len -=
							IEEE80211_FCS_LEN;
			}
#endif

			buf_desc_debug("RX:%s(%d), pkt_len = %d, pattrib->drvinfo_sz = %d, pattrib->qos = %d, pattrib->shift_sz = %d\n",
				       __func__, __LINE__, pattrib->pkt_len,
				       pattrib->drvinfo_sz, pattrib->qos,
				       pattrib->shift_sz);

			if (rtw_os_alloc_recvframe(padapter, precvframe,
				   (skb->data + desc_size +
				    pattrib->drvinfo_sz + pattrib->shift_sz),
						   skb) == _FAIL) {

				rtw_free_recvframe(precvframe,
						   &r_priv->free_recv_queue);

				RTW_INFO("rtl8821ce_rx_mpdu:can't allocate memory for skb copy\n");
				*((dma_addr_t *) skb->cb) =
					#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
					pci_map_single(pdvobjpriv->ppcidev,
						       skb_tail_pointer(skb),
						       r_priv->rxbuffersize,
						       PCI_DMA_FROMDEVICE);
					#else
					dma_map_single(&(pdvobjpriv->ppcidev)->dev,
						       skb_tail_pointer(skb),
						       r_priv->rxbuffersize,
						       DMA_FROM_DEVICE);
					#endif
				goto done;
			}

			recvframe_put(precvframe, pattrib->pkt_len);

			if (pattrib->pkt_rpt_type == NORMAL_RX) {
				/* Normal rx packet */
				pre_recv_entry(precvframe, pattrib->physt ? ((u8 *)(skb->data) + desc_size) : NULL);

			} else {
				if (pattrib->pkt_rpt_type == C2H_PACKET)
					c2h_pre_handler_rtl8821c(padapter, skb->data, desc_size + pattrib->pkt_len);

				rtw_free_recvframe(precvframe, pfree_recv_queue);
			}
			*((dma_addr_t *) skb->cb) =
				#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
				pci_map_single(pdvobjpriv->ppcidev,
					       skb_tail_pointer(skb),
					       r_priv->rxbuffersize,
					       PCI_DMA_FROMDEVICE);
				#else
				dma_map_single(&(pdvobjpriv->ppcidev)->dev,
					       skb_tail_pointer(skb),
					       r_priv->rxbuffersize,
					       DMA_FROM_DEVICE);
				#endif
		}
done:


		SET_RX_BD_PHYSICAL_ADDR_LOW(rx_bd, *((dma_addr_t *)skb->cb));
#ifdef CONFIG_64BIT_DMA
		SET_RX_BD_PHYSICAL_ADDR_HIGH(rx_bd, (*((dma_addr_t *)skb->cb)>>32));
#endif
		SET_RX_BD_RXBUFFSIZE(rx_bd, r_priv->rxbuffersize);

		r_priv->rx_ring[rx_q_idx].idx =
			(r_priv->rx_ring[rx_q_idx].idx + 1) %
			r_priv->rxringcount;

		rtw_write16(padapter, REG_RXQ_RXBD_IDX,
			    r_priv->rx_ring[rx_q_idx].idx);

		buf_desc_debug("RX:%s(%d) reg_value %x\n", __func__, __LINE__,
			       rtw_read32(padapter, REG_RXQ_RXBD_IDX));

		remaing_rxdesc--;
	}
}

static void rtl8821ce_recv_tasklet(void *priv)
{
	_irqL	irqL;
	_adapter	*padapter = (_adapter *)priv;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	rtl8821ce_rx_mpdu(padapter);
	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);
	pHalData->IntrMask[0] |= (BIT_RXOK_MSK_8821C | BIT_RDU_MSK_8821C);
	pHalData->IntrMask[1] |= BIT_FOVW_MSK_8821C;
	rtw_write32(padapter, REG_HIMR0_8821C, pHalData->IntrMask[0]);
	rtw_write32(padapter, REG_HIMR1_8821C, pHalData->IntrMask[1]);
	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);
}

static void rtl8821ce_xmit_beacon(PADAPTER Adapter)
{
#if defined(CONFIG_AP_MODE) && defined(CONFIG_NATIVEAP_MLME)
	struct mlme_priv *pmlmepriv = &Adapter->mlmepriv;

	if (MLME_IS_AP(Adapter) || MLME_IS_MESH(Adapter)) {
		/* send_beacon(Adapter); */
		if (pmlmepriv->update_bcn == _TRUE)
			tx_beacon_hdl(Adapter, NULL);
	}
#endif
}

static void rtl8821ce_prepare_bcn_tasklet(void *priv)
{
	_adapter *padapter = (_adapter *)priv;

	rtl8821ce_xmit_beacon(padapter);
}

s32 rtl8821ce_init_recv_priv(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	s32	ret = _SUCCESS;


#ifdef PLATFORM_LINUX
	tasklet_init(&precvpriv->recv_tasklet,
		     (void(*)(unsigned long))rtl8821ce_recv_tasklet,
		     (unsigned long)padapter);

	tasklet_init(&precvpriv->irq_prepare_beacon_tasklet,
		     (void(*)(unsigned long))rtl8821ce_prepare_bcn_tasklet,
		     (unsigned long)padapter);
#endif


	return ret;
}

void rtl8821ce_free_recv_priv(_adapter *padapter)
{

}

int rtl8821ce_init_rxbd_ring(_adapter *padapter)
{
	struct recv_priv	*r_priv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	struct net_device	*dev = padapter->pnetdev;
	dma_addr_t *mapping = NULL;
	struct sk_buff *skb = NULL;
	u8	*rx_desc = NULL;
	int	i, rx_queue_idx;


	/* rx_queue_idx 0:RX_MPDU_QUEUE */
	/* rx_queue_idx 1:RX_CMD_QUEUE */
	for (rx_queue_idx = 0; rx_queue_idx < 1; rx_queue_idx++) {
		r_priv->rx_ring[rx_queue_idx].buf_desc =
			#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
			pci_alloc_consistent(pdev,
			     sizeof(*r_priv->rx_ring[rx_queue_idx].buf_desc) *
					     r_priv->rxringcount,
				     &r_priv->rx_ring[rx_queue_idx].dma);
			#else
			dma_alloc_coherent(&pdev->dev,
			     sizeof(*r_priv->rx_ring[rx_queue_idx].buf_desc) *
					     r_priv->rxringcount,
						&r_priv->rx_ring[rx_queue_idx].dma,
				    GFP_KERNEL);
			#endif

		if (!r_priv->rx_ring[rx_queue_idx].buf_desc ||
		    (unsigned long)r_priv->rx_ring[rx_queue_idx].buf_desc &
		    0xFF) {
			RTW_INFO("Cannot allocate RX ring\n");
			return _FAIL;
		}

		_rtw_memset(r_priv->rx_ring[rx_queue_idx].buf_desc, 0,
			    sizeof(*r_priv->rx_ring[rx_queue_idx].buf_desc) *
			    r_priv->rxringcount);
		r_priv->rx_ring[rx_queue_idx].idx = 0;

		for (i = 0; i < r_priv->rxringcount; i++) {
			skb = dev_alloc_skb(r_priv->rxbuffersize);
			if (!skb) {
				RTW_INFO("Cannot allocate skb for RX ring\n");
				return _FAIL;
			}

			rx_desc =
				(u8 *)(&r_priv->rx_ring[rx_queue_idx].buf_desc[i]);
			r_priv->rx_ring[rx_queue_idx].rx_buf[i] = skb;
			mapping = (dma_addr_t *)skb->cb;

			/* just set skb->cb to mapping addr
			 * for pci_unmap_single use
			 */
			#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
			*mapping = pci_map_single(pdev, skb_tail_pointer(skb),
					r_priv->rxbuffersize,
					PCI_DMA_FROMDEVICE);
			#else
			*mapping = dma_map_single(&pdev->dev, skb_tail_pointer(skb),
					r_priv->rxbuffersize,
					DMA_FROM_DEVICE);
			#endif


			/* Reset FS, LS, Total len */
			SET_RX_BD_LS(rx_desc, 0);
			SET_RX_BD_FS(rx_desc, 0);
			SET_RX_BD_TOTALRXPKTSIZE(rx_desc, 0);
			SET_RX_BD_RXBUFFSIZE(rx_desc, r_priv->rxbuffersize);
			SET_RX_BD_PHYSICAL_ADDR_LOW(rx_desc, *mapping);
#ifdef CONFIG_64BIT_DMA
			SET_RX_BD_PHYSICAL_ADDR_HIGH(rx_desc, *mapping >> 32);
#endif

			buf_desc_debug("RX:rx buffer desc addr[%d] = %x, skb(rx_buf) = %x, buffer addr (virtual = %x, physical = %x)\n",
				i, (u32)&r_priv->rx_ring[rx_queue_idx].buf_desc[i],
				(u32)r_priv->rx_ring[rx_queue_idx].rx_buf[i],
				(u32)(skb_tail_pointer(skb)), (u32)(*mapping));
		}
	}


	return _SUCCESS;
}

void rtl8821ce_free_rxbd_ring(_adapter *padapter)
{
	struct recv_priv *r_priv = &padapter->recvpriv;
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev *pdev = pdvobjpriv->ppcidev;
	int i, rx_queue_idx;


	/* rx_queue_idx 0:RX_MPDU_QUEUE */
	/* rx_queue_idx 1:RX_CMD_QUEUE */
	for (rx_queue_idx = 0; rx_queue_idx < 1; rx_queue_idx++) {
		for (i = 0; i < r_priv->rxringcount; i++) {
			struct sk_buff *skb;

			skb = r_priv->rx_ring[rx_queue_idx].rx_buf[i];

			if (!skb)
				continue;

			#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
			pci_unmap_single(pdev,
					 *((dma_addr_t *) skb->cb),
					 r_priv->rxbuffersize,
					 PCI_DMA_FROMDEVICE);
			#else
			dma_unmap_single(&pdev->dev,
					 *((dma_addr_t *) skb->cb),
					 r_priv->rxbuffersize,
					 DMA_FROM_DEVICE);
			#endif
			kfree_skb(skb);
		}

		#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
		pci_free_consistent(pdev,
			    sizeof(*r_priv->rx_ring[rx_queue_idx].buf_desc) *
				    r_priv->rxringcount,
				    r_priv->rx_ring[rx_queue_idx].buf_desc,
				    r_priv->rx_ring[rx_queue_idx].dma);
		#else
		dma_free_coherent(&pdev->dev,
			sizeof(*r_priv->rx_ring[rx_queue_idx].buf_desc) *
			r_priv->rxringcount,
			r_priv->rx_ring[rx_queue_idx].buf_desc,
			r_priv->rx_ring[rx_queue_idx].dma);
		#endif
		r_priv->rx_ring[rx_queue_idx].buf_desc = NULL;
	}

}
