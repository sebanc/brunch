/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _HCI_OPS_OS_C_

/*#include <drv_types.h>*/
#include <rtl8723d_hal.h>

static int rtl8723de_init_rx_ring(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	struct net_device	*dev = padapter->pnetdev;
	dma_addr_t *mapping = NULL;
	struct sk_buff *skb = NULL;
	u8	*rx_bufdesc = NULL;
	int	i, rx_queue_idx;


	/* rx_queue_idx 0:RX_MPDU_QUEUE */
	/* rx_queue_idx 1:RX_CMD_QUEUE */
	for (rx_queue_idx = 0; rx_queue_idx < 1/*RX_MAX_QUEUE*/; rx_queue_idx++) {
		precvpriv->rx_ring[rx_queue_idx].buf_desc =
			pci_alloc_consistent(pdev,
			sizeof(*precvpriv->rx_ring[rx_queue_idx].buf_desc) * precvpriv->rxringcount,
				     &precvpriv->rx_ring[rx_queue_idx].dma);

		if (!precvpriv->rx_ring[rx_queue_idx].buf_desc
		    || (unsigned long)precvpriv->rx_ring[rx_queue_idx].buf_desc & 0xFF) {
			RTW_INFO("Cannot allocate RX ring\n");
			return _FAIL;
		}

		_rtw_memset(precvpriv->rx_ring[rx_queue_idx].buf_desc, 0, sizeof(*precvpriv->rx_ring[rx_queue_idx].buf_desc) * precvpriv->rxringcount);
		precvpriv->rx_ring[rx_queue_idx].idx = 0;

		for (i = 0; i < precvpriv->rxringcount; i++) {
			skb = rtw_skb_alloc(precvpriv->rxbuffersize);

			if (!skb) {
				RTW_INFO("Cannot allocate skb for RX ring\n");
				return _FAIL;
			}

			rx_bufdesc = (u8 *)(&precvpriv->rx_ring[rx_queue_idx].buf_desc[i]);

			precvpriv->rx_ring[rx_queue_idx].rx_buf[i] = skb;

			mapping = (dma_addr_t *)skb->cb;

			/* just set skb->cb to mapping addr for pci_unmap_single use */
			*mapping = pci_map_single(pdev, skb_tail_pointer(skb),
						  precvpriv->rxbuffersize,
						  PCI_DMA_FROMDEVICE);

			/* Reset FS, LS, Total len */
			SET_RX_BUFFER_DESC_LS_8723D(rx_bufdesc, 0);
			SET_RX_BUFFER_DESC_FS_8723D(rx_bufdesc, 0);
			SET_RX_BUFFER_DESC_TOTAL_LENGTH_8723D(rx_bufdesc, 0);
			SET_RX_BUFFER_DESC_DATA_LENGTH_8723D(rx_bufdesc, precvpriv->rxbuffersize);
			SET_RX_BUFFER_PHYSICAL_LOW_8723D(rx_bufdesc, ((*mapping) & DMA_BIT_MASK(32)));
			SET_RX_BUFFER_PHYSICAL_HIGH_8723D(rx_bufdesc, ((*mapping) >> 32));
		}
	}


	return _SUCCESS;
}

static void rtl8723de_free_rx_ring(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	int i, rx_queue_idx;


	/* rx_queue_idx 0:RX_MPDU_QUEUE */
	/* rx_queue_idx 1:RX_CMD_QUEUE */
	for (rx_queue_idx = 0; rx_queue_idx < 1/*RX_MAX_QUEUE*/; rx_queue_idx++) {
		for (i = 0; i < precvpriv->rxringcount; i++) {
			struct sk_buff *skb = precvpriv->rx_ring[rx_queue_idx].rx_buf[i];

			if (!skb)
				continue;

			pci_unmap_single(pdev,
					 *((dma_addr_t *) skb->cb),
					 precvpriv->rxbuffersize,
					 PCI_DMA_FROMDEVICE);
			rtw_skb_free(skb);
		}

		pci_free_consistent(pdev,
			    sizeof(*precvpriv->rx_ring[rx_queue_idx].buf_desc) *
				    precvpriv->rxringcount,
				    precvpriv->rx_ring[rx_queue_idx].buf_desc,
				    precvpriv->rx_ring[rx_queue_idx].dma);
		precvpriv->rx_ring[rx_queue_idx].buf_desc = NULL;
	}

}


static int rtl8723de_init_tx_ring(_adapter *padapter, unsigned int prio, unsigned int entries)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	struct tx_buf_desc	*ring;
	dma_addr_t		dma;
	int	i;


	/* RTW_INFO("%s entries num:%d\n", __func__, entries); */
	ring = pci_alloc_consistent(pdev, sizeof(*ring) * entries, &dma);
	if (!ring || (unsigned long)ring & 0xFF) {
		RTW_INFO("Cannot allocate TX ring (prio = %d)\n", prio);
		return _FAIL;
	}

	_rtw_memset(ring, 0, sizeof(*ring) * entries);
	pxmitpriv->tx_ring[prio].buf_desc = ring;
	pxmitpriv->tx_ring[prio].dma = dma;
	pxmitpriv->tx_ring[prio].idx = 0;
	pxmitpriv->tx_ring[prio].entries = entries;
	_rtw_init_queue(&pxmitpriv->tx_ring[prio].queue);
	pxmitpriv->tx_ring[prio].qlen = 0;
	pxmitpriv->tx_ring[prio].hw_rp_cache = 0;

	/* RTW_INFO("%s queue:%d, ring_addr:%p\n", __func__, prio, ring); */


	return _SUCCESS;
}

static void rtl8723de_free_tx_ring(_adapter *padapter, unsigned int prio)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pci_dev	*pdev = pdvobjpriv->ppcidev;
	struct rtw_tx_ring *ring = &pxmitpriv->tx_ring[prio];
	u8				*tx_bufdesc;
	struct xmit_buf	*pxmitbuf;


	while (ring->qlen) {
		tx_bufdesc = (u8 *)(&ring->buf_desc[ring->idx]);

		SET_TX_BUFF_DESC_OWN_8723D(tx_bufdesc, 0);

		if (prio != BCN_QUEUE_INX)
			ring->idx = (ring->idx + 1) % ring->entries;

		pxmitbuf = rtl8723de_dequeue_xmitbuf(ring);
		if (pxmitbuf) {
			dma_addr_t mapping;

			mapping = GET_TX_BUFF_DESC_ADDR_LOW_0_8723D(tx_bufdesc);
			mapping |= (dma_addr_t)GET_TX_BUFF_DESC_ADDR_HIGH_0_8723D(tx_bufdesc) << 32;

			pci_unmap_single(pdev, mapping, pxmitbuf->len, PCI_DMA_TODEVICE);
			rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
		} else
			RTW_INFO("%s(): qlen(%d) is not zero, but have xmitbuf in pending queue\n", __func__, ring->qlen);
	}

	pci_free_consistent(pdev, sizeof(*ring->buf_desc) * ring->entries, ring->buf_desc, ring->dma);
	ring->buf_desc = NULL;

}

static void init_desc_ring_var(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	u8 i = 0;

	for (i = 0; i < HW_QUEUE_ENTRY; i++)
		pxmitpriv->txringcount[i] = TX_BD_NUM;

	/* we just alloc 2 desc for beacon queue, */
	/* because we just need first desc in hw beacon. */
	pxmitpriv->txringcount[BCN_QUEUE_INX] = 2;

	/* BE queue need more descriptor for performance consideration */
	/* or, No more tx desc will happen, and may cause mac80211 mem leakage. */
	/* if(!padapter->registrypriv.wifi_spec) */
	/*	pxmitpriv->txringcount[BE_QUEUE_INX] = TXDESC_NUM_BE_QUEUE; */
#ifdef CONFIG_CONCURRENT_MODE
	pxmitpriv->txringcount[BE_QUEUE_INX] *= 2;
#endif

	pxmitpriv->txringcount[TXCMD_QUEUE_INX] = 2;

	precvpriv->rxbuffersize = MAX_RECVBUF_SZ;	/* 2048;*/ /*1024; */
	precvpriv->rxringcount = PCI_MAX_RX_COUNT;	/* 128; */
}


u32 rtl8723de_init_desc_ring(_adapter *padapter)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	int	i, ret = _SUCCESS;


	init_desc_ring_var(padapter);

	ret = rtl8723de_init_rx_ring(padapter);
	if (ret == _FAIL)
		return ret;

	/* general process for other queue */
	for (i = 0; i < PCI_MAX_TX_QUEUE_COUNT; i++) {
		ret = rtl8723de_init_tx_ring(padapter, i, pxmitpriv->txringcount[i]);
		if (ret == _FAIL)
			goto err_free_rings;
	}

	return ret;

err_free_rings:

	rtl8723de_free_rx_ring(padapter);

	for (i = 0; i < PCI_MAX_TX_QUEUE_COUNT; i++)
		if (pxmitpriv->tx_ring[i].buf_desc)
			rtl8723de_free_tx_ring(padapter, i);


	return ret;
}

u32 rtl8723de_free_desc_ring(_adapter *padapter)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	u32 i;


	/* free rx rings */
	rtl8723de_free_rx_ring(padapter);

	/* free tx rings */
	for (i = 0; i < PCI_MAX_TX_QUEUE_COUNT; i++)
		rtl8723de_free_tx_ring(padapter, i);


	return _SUCCESS;
}

void rtl8723de_reset_desc_ring(_adapter *padapter)
{
	_irqL	irqL;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct xmit_buf	*pxmitbuf = NULL;
	u8	*tx_bufdesc/*, *rx_bufdesc*/;
	int	i, rx_queue_idx;

	for (rx_queue_idx = 0; rx_queue_idx < 1; rx_queue_idx++) {
		if (precvpriv->rx_ring[rx_queue_idx].buf_desc) {
#if 0
			rx_bufdesc = NULL;
			for (i = 0; i < precvpriv->rxringcount; i++) {
				rx_bufdesc = (u8 *)(&precvpriv->rx_ring[rx_queue_idx].buf_desc[i]);
				/*SET_RX_BUFFER_DESC_OWN_8723D(rx_bufdesc, 1);*/
			}
#endif
			precvpriv->rx_ring[rx_queue_idx].idx = 0;
		}
	}

	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);
	for (i = 0; i < PCI_MAX_TX_QUEUE_COUNT; i++) {
		if (pxmitpriv->tx_ring[i].buf_desc) {
			struct rtw_tx_ring *ring = &pxmitpriv->tx_ring[i];

			while (ring->qlen) {
				tx_bufdesc = (u8 *)(&ring->buf_desc[ring->idx]);

				SET_TX_BUFF_DESC_OWN_8723D(tx_bufdesc, 0);

				if (i != BCN_QUEUE_INX)
					ring->idx = (ring->idx + 1) % ring->entries;

				pxmitbuf = rtl8723de_dequeue_xmitbuf(ring);
				if (pxmitbuf) {
					dma_addr_t mapping;

					mapping = GET_TX_BUFF_DESC_ADDR_LOW_0_8723D(tx_bufdesc);
					mapping |= (dma_addr_t)GET_TX_BUFF_DESC_ADDR_HIGH_0_8723D(tx_bufdesc) << 32;

					pci_unmap_single(pdvobjpriv->ppcidev, mapping, pxmitbuf->len, PCI_DMA_TODEVICE);
					rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				} else
					RTW_INFO("%s(): qlen(%d) is not zero, but have xmitbuf in pending queue\n", __func__, ring->qlen);
			}
			ring->idx = 0;
			ring->hw_rp_cache = 0;
		}
	}
	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);

	/* Reset read/write point */
	rtw_write32(padapter, REG_BD_RW_PTR_CLR_8723D, 0XFFFFFFFF);
}

static void rtl8723de_xmit_beacon(PADAPTER Adapter)
{
#if defined(CONFIG_AP_MODE) && defined(CONFIG_NATIVEAP_MLME)
	struct mlme_priv *pmlmepriv = &Adapter->mlmepriv;

	if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		/* send_beacon(Adapter); */
		if (pmlmepriv->update_bcn == _TRUE) {
			WLAN_BSSID_EX	*cur_network = &Adapter->mlmeextpriv.mlmext_info.network;

			/* prevent beacon IE length is 0! */
			if (cur_network->IELength < _FIXED_IE_LENGTH_) {
				/* RTW_INFO( "%s:%d cur_network->IELength=%u\n", __FUNCTION__, __LINE__, cur_network->IELength ); */
				return;
			}

			tx_beacon_hdl(Adapter, NULL);
		}
	}
#endif
}

void rtl8723de_prepare_bcn_tasklet(void *priv)
{
	_adapter	*padapter = (_adapter *)priv;

	rtl8723de_xmit_beacon(padapter);
}

u8 check_tx_desc_resource(_adapter *padapter, int prio)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct rtw_tx_ring	*ring;

	ring = &pxmitpriv->tx_ring[prio];

	/* for now we reserve two free descriptor as a safety boundary */
	/* between the tail and the head */
	if ((ring->entries - ring->qlen) >= 3)
		return _TRUE;

	/* RTW_INFO("do not have enough desc for Tx\n"); */
	return _FALSE;
}

static u32 rtl8723de_check_txdesc_closed(PADAPTER Adapter, u32 queue_idx,
		struct rtw_tx_ring *ring)
{
	/*
	 * hw_rp_cache is used to reduce REG access.
	 */
	u32	tmp32;

	/* bcn queue should not enter this function */
	if (queue_idx == BCN_QUEUE_INX)
		return _TRUE;

	/* qlen == 0 --> don't need to process */
	if (ring->qlen == 0)
		return _FALSE;

	/* sw_rp == hw_rp_cache --> sync hw_rp */
	if (ring->idx == ring->hw_rp_cache) {

		tmp32 = rtw_read32(Adapter, get_txbufdesc_idx_hwaddr(queue_idx));

		ring->hw_rp_cache = (tmp32 >> 16) & 0x0FFF;
	}

	/* check if need to handle TXOK */
	if (ring->idx == ring->hw_rp_cache)
		return _FALSE;

	return _TRUE;
}

static void rtl8723de_tx_isr(PADAPTER Adapter, int prio)
{
	struct xmit_priv	*pxmitpriv = &Adapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);
	struct rtw_tx_ring	*ring = &pxmitpriv->tx_ring[prio];
	struct xmit_buf	*pxmitbuf;
	u8	*tx_bufdesc;

	while (ring->qlen) {
		tx_bufdesc = (u8 *)&ring->buf_desc[ring->idx];

		/* beacon packet will only use the first descriptor defautly, */
		/* and the OWN may not be cleared by the hardware */
		if (prio != BCN_QUEUE_INX) {
			if (!rtl8723de_check_txdesc_closed(Adapter, prio, ring))
				break;

			ring->idx = (ring->idx + 1) % ring->entries;
		} else if (prio == BCN_QUEUE_INX)
			SET_TX_BUFF_DESC_OWN_8723D(tx_bufdesc, 0);

		pxmitbuf = rtl8723de_dequeue_xmitbuf(ring);
		if (pxmitbuf) {
			dma_addr_t mapping;

			mapping = GET_TX_BUFF_DESC_ADDR_LOW_0_8723D(tx_bufdesc);
			mapping |= (dma_addr_t)GET_TX_BUFF_DESC_ADDR_HIGH_0_8723D(tx_bufdesc) << 32;

			pci_unmap_single(pdvobjpriv->ppcidev, mapping, pxmitbuf->len, PCI_DMA_TODEVICE);
			rtw_sctx_done(&pxmitbuf->sctx);
			rtw_free_xmitbuf(&(pxmitbuf->padapter->xmitpriv), pxmitbuf);
		} else
			RTW_INFO("%s(): qlen(%d) is not zero, but have xmitbuf in pending queue\n", __func__, ring->qlen);
	}

	if ((prio != BCN_QUEUE_INX) && check_tx_desc_resource(Adapter, prio)
	    && rtw_xmit_ac_blocked(Adapter) != _TRUE
	   )
		rtw_mi_xmit_tasklet_schedule(Adapter);
}


s32	rtl8723de_interrupt(PADAPTER Adapter)
{
	_irqL	irqL;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(Adapter);
	struct xmit_priv	*pxmitpriv = &Adapter->xmitpriv;
	int	ret = _SUCCESS;

	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);

	/* read ISR: 4/8bytes */
	if (InterruptRecognized8723DE(Adapter) == _FALSE) {
		ret = _FAIL;
		goto done;
	}

	/* <1> beacon related */
	if (pHalData->IntArray[0] & IMR_TXBCN0OK_8723D) {
		/*Modify for MI temporary,this processor cannot apply to multi-ap*/
		PADAPTER bcn_adapter = rtw_mi_get_ap_adapter(Adapter);
		if (bcn_adapter->xmitpriv.beaconDMAing) {
			bcn_adapter->xmitpriv.beaconDMAing = _FAIL;
			rtl8723de_tx_isr(Adapter, BCN_QUEUE_INX);
		}
	}

	if (pHalData->IntArray[0] & IMR_TXBCN0ERR_8723D) {
		/*Modify for MI temporary,this processor cannot apply to multi-ap*/
		PADAPTER bcn_adapter = rtw_mi_get_ap_adapter(Adapter);
		if (bcn_adapter->xmitpriv.beaconDMAing) {
			bcn_adapter->xmitpriv.beaconDMAing = _FAIL;
			rtl8723de_tx_isr(Adapter, BCN_QUEUE_INX);
		}
	}

	if (pHalData->IntArray[0] & IMR_BCNDERR0_8723D) {
		/* Release resource and re-transmit beacon to HW */
		struct tasklet_struct  *bcn_tasklet;
		/*Modify for MI temporary,this processor cannot apply to multi-ap*/
		PADAPTER bcn_adapter = rtw_mi_get_ap_adapter(Adapter);
		rtl8723de_tx_isr(Adapter, BCN_QUEUE_INX);
		bcn_adapter->mlmepriv.update_bcn = _TRUE;
		bcn_tasklet = &bcn_adapter->recvpriv.irq_prepare_beacon_tasklet;
		tasklet_hi_schedule(bcn_tasklet);
	}

	if (pHalData->IntArray[0] & IMR_BCNDMAINT0_8723D) {
		struct tasklet_struct  *bcn_tasklet;
		/*Modify for MI temporary,this processor cannot apply to multi-ap*/
		PADAPTER bcn_adapter = rtw_mi_get_ap_adapter(Adapter);

		bcn_tasklet = &bcn_adapter->recvpriv.irq_prepare_beacon_tasklet;
		tasklet_hi_schedule(bcn_tasklet);
	}

	/* <2> Rx related */
	if ((pHalData->IntArray[0] & (IMR_ROK_8723D | IMR_RDU_8723D)) || (pHalData->IntArray[1] & IMR_RXFOVW_8723D)) {
		pHalData->IntrMask[0] &= (~(IMR_ROK_8723D | IMR_RDU_8723D));
		pHalData->IntrMask[1] &= (~IMR_RXFOVW_8723D);
		rtw_write32(Adapter, REG_HIMR0_8723D, pHalData->IntrMask[0]);
		rtw_write32(Adapter, REG_HIMR1_8723D, pHalData->IntrMask[1]);
		tasklet_hi_schedule(&Adapter->recvpriv.recv_tasklet);
	}

	/* <3> Tx related */
	if (pHalData->IntArray[1] & IMR_TXFOVW_8723D) {
		/* */
		/* RTW_INFO("IMR_TXFOVW!\n"); */
	}

	/* if (pisr_content->IntArray[0] & IMR_TX_MASK) { */
	/*	tasklet_hi_schedule(&pxmitpriv->xmit_tasklet); */
	/* } */

	if (pHalData->IntArray[0] & IMR_MGNTDOK_8723D) {
		/* RTW_INFO("Manage ok interrupt!\n"); */
		rtl8723de_tx_isr(Adapter, MGT_QUEUE_INX);
	}

	if (pHalData->IntArray[0] & IMR_HIGHDOK_8723D) {
		/* RTW_INFO("HIGH_QUEUE ok interrupt!\n"); */
		rtl8723de_tx_isr(Adapter, HIGH_QUEUE_INX);
	}

	if (pHalData->IntArray[0] & IMR_BKDOK_8723D) {
		/* RTW_INFO("BK Tx OK interrupt!\n"); */
		rtl8723de_tx_isr(Adapter, BK_QUEUE_INX);
	}

	if (pHalData->IntArray[0] & IMR_BEDOK_8723D) {
		/* RTW_INFO("BE TX OK interrupt!\n"); */
		rtl8723de_tx_isr(Adapter, BE_QUEUE_INX);
	}

	if (pHalData->IntArray[0] & IMR_VIDOK_8723D) {
		/* RTW_INFO("VI TX OK interrupt!\n"); */
		rtl8723de_tx_isr(Adapter, VI_QUEUE_INX);
	}

	if (pHalData->IntArray[0] & IMR_VODOK_8723D) {
		/* RTW_INFO("Vo TX OK interrupt!\n"); */
		rtl8723de_tx_isr(Adapter, VO_QUEUE_INX);
	}

done:

	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);

	return ret;
}

/*
  * Wait until rx data is ready
  *		return value: _SUCCESS if Rx packet is ready, _FAIL if not ready
  */

static u32
rtl8723de_wait_rxrdy(
	_adapter			*padapter,
	u8				*rx_bufdesc,
	u16				rx_queue_idx
)
{

	struct recv_priv	*precvpriv = &padapter->recvpriv;
	u8 first_seg, last_seg;
#ifdef USING_RX_TAG
	u16 rx_tag;
#else
	u16 total_len;
#endif
	u16 read_cnt = 0;
	u16 status = _SUCCESS;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	if (rx_bufdesc == NULL)
		return _FAIL;

#ifdef USING_RX_TAG
	rx_tag = (u2Byte)GET_RX_BUFFER_DESC_RX_TAG_8723D(rx_bufdesc);
#else
	total_len = (u2Byte)GET_RX_BUFFER_DESC_TOTAL_LENGTH_8723D(rx_bufdesc);
#endif
	first_seg = (u1Byte)GET_RX_BUFFER_DESC_FS_8723D(rx_bufdesc);
	last_seg = (u1Byte)GET_RX_BUFFER_DESC_LS_8723D(rx_bufdesc);

	/*buf_desc_debug("RX:%s enter: rx_buf_desc addr = %p, total_len=%d, first_seg=%d, last_seg=%d, read_cnt %d, index %d, address %p\n",\
					__FUNCTION__, (u8 *)&precvpriv->rx_ring[rx_queue_idx].desc[precvpriv->rx_ring[rx_queue_idx].idx], total_len, first_seg, last_seg, read_cnt, precvpriv->rx_ring[rx_queue_idx].idx, rx_bufdesc);*/

#if defined(USING_RX_TAG)
	while (rx_tag != pHalData->RxTag) {

		read_cnt++;

		rx_tag = (u2Byte)GET_RX_BUFFER_DESC_RX_TAG_8723D(rx_bufdesc);

		first_seg = (u1Byte)GET_RX_BUFFER_DESC_FS_8723D(rx_bufdesc);

		last_seg = (u1Byte)GET_RX_BUFFER_DESC_LS_8723D(rx_bufdesc);


		if (read_cnt > 100) {
			RTW_INFO("RX:%s(%d) Before sync RxTag:sw=%X,hw=%X\n", __func__, __LINE__, pHalData->RxTag, rx_tag);
			break;
		}
	}

	pHalData->RxTag = (rx_tag + 1) & 0x1FFF;	/* rx tag occpies 13 bits [28:16] */
#else
	while (total_len == 0 && first_seg == 0 && last_seg == 0) {

		read_cnt++;

		total_len = (u2Byte)GET_RX_BUFFER_DESC_TOTAL_LENGTH_8723D(rx_bufdesc);

		first_seg = (u1Byte)GET_RX_BUFFER_DESC_FS_8723D(rx_bufdesc);

		last_seg = (u1Byte)GET_RX_BUFFER_DESC_LS_8723D(rx_bufdesc);

		if (read_cnt > 20) {
			status = _FAIL;
			break;
		}
	}
#endif

	/*
	buf_desc_debug("RX:%s exit total_len=%d, rx_tag = %d, first_seg=%d, last_seg=%d, read_cnt %d\n",\
						__FUNCTION__, total_len, pHalData->RxTag, first_seg, last_seg, read_cnt);
	*/

	return status;
}

/*
  * Check the number of rxdec to be handled between
  *		"index of RX queue descriptor maintained by host (write pointer)" and
  *   "index of RX queue descriptor maintained by hardware (read pointer)"
  */
static u16 rtl8723de_check_rxdesc_remain(_adapter *padapter, struct rtw_rx_ring *prxring)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	u16	desc_idx_hw_wp, desc_idx_host_rp;
	u32	tmp_4bytes;

	int num_rxdesc_to_handle;

	tmp_4bytes = rtw_read32(padapter, REG_RXQ_TXBD_IDX_8723D);
	desc_idx_hw_wp = (u16)((tmp_4bytes >> 16) & 0x7ff);
	desc_idx_host_rp = (u16)(tmp_4bytes & 0x7ff);

	/* make sure driver does not handle packet if hardware pointer keeps in zero */
	/*	in initial state */
	/*buf_desc_debug("RX:%s(%d) reg_value %x\n", __FUNCTION__, __LINE__, tmp_4bytes);*/

	num_rxdesc_to_handle = desc_idx_hw_wp - desc_idx_host_rp;

	if (num_rxdesc_to_handle == 0)
		return 0;	/* No RX */

	if (num_rxdesc_to_handle < 0)	/* wrap-around case */
		num_rxdesc_to_handle += RX_BD_NUM;

	if (prxring->idx != desc_idx_host_rp) {
		prxring->idx = desc_idx_host_rp;
		RTW_INFO("RX: %s(%d) host_rp is inconsistent: sw=%X, hw=%X\n",
			 __func__, __LINE__, prxring->idx, desc_idx_host_rp);
	}

	/* buf_desc_debug("RX:%s  reg_value %x,  last_desc_idx_hw = %x, current hw idx %x, current host idx %x, remain desc to handle = %d\n",\
					__FUNCTION__, tmp_4bytes, last_idx_hw, desc_idx_hw, desc_idx_host, num_rxdesc_to_handle); */

	return (u16)num_rxdesc_to_handle;
}

static void rtl8723de_rx_mpdu(_adapter *padapter)
{
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	_queue			*pfree_recv_queue = &precvpriv->free_recv_queue;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(padapter);
	union recv_frame	*precvframe = NULL;
	u8				*pphy_info = NULL;
	struct rx_pkt_attrib	*pattrib = NULL;
	int	rx_queue_idx = RX_MPDU_QUEUE;
	u16	remaing_rxdesc = 0;
	struct rtw_rx_ring *prxring;

	prxring = &precvpriv->rx_ring[rx_queue_idx];

	remaing_rxdesc = rtl8723de_check_rxdesc_remain(padapter, prxring);

	/* RX NORMAL PKT */
	while (remaing_rxdesc) {
		u8	*rx_bufdesc = (u8 *)&prxring->buf_desc[prxring->idx];/* rx descriptor */
		struct sk_buff *skb = prxring->rx_buf[prxring->idx];/* rx pkt */

		if (rtl8723de_wait_rxrdy(padapter, rx_bufdesc, rx_queue_idx) != _SUCCESS)
			RTW_INFO("RX:%s(%d) packet not ready\n", __func__, __LINE__);

		{
			precvframe = rtw_alloc_recvframe(pfree_recv_queue);
			if (precvframe == NULL) {
				RTW_INFO("%s: precvframe==NULL\n", __func__);
				goto done;
			}

			_rtw_init_listhead(&precvframe->u.hdr.list);
			precvframe->u.hdr.len = 0;

			pci_unmap_single(pdvobjpriv->ppcidev,
					 *((dma_addr_t *)skb->cb),
					 precvpriv->rxbuffersize,
					 PCI_DMA_FROMDEVICE);

			rtl8723d_query_rx_desc_status(precvframe, skb->data);
			pattrib = &precvframe->u.hdr.attrib;

#ifdef CONFIG_RX_PACKET_APPEND_FCS
			if (check_fwstate(&padapter->mlmepriv, WIFI_MONITOR_STATE) == _FALSE)
				if ((pattrib->pkt_rpt_type == NORMAL_RX) && (pHalData->ReceiveConfig & RCR_APPFCS))
					pattrib->pkt_len -= IEEE80211_FCS_LEN;
#endif

			if (rtw_os_alloc_recvframe(padapter, precvframe,
				(skb->data + RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz), skb) == _FAIL) {
				rtw_free_recvframe(precvframe, &precvpriv->free_recv_queue);
				RTW_INFO("rtl8723de_rx_mpdu:can not allocate memory for skb copy\n");
				*((dma_addr_t *) skb->cb) = pci_map_single(pdvobjpriv->ppcidev, skb_tail_pointer(skb), precvpriv->rxbuffersize, PCI_DMA_FROMDEVICE);
				goto done;
			}

			recvframe_put(precvframe, pattrib->pkt_len);

			/* Normal rx packet */
			if (pattrib->pkt_rpt_type == NORMAL_RX) {
				if (pattrib->physt)
					pphy_info = (u8 *)(skb->data) + RXDESC_SIZE;
#ifdef CONFIG_CONCURRENT_MODE
				pre_recv_entry(precvframe, pphy_info);
#endif

				if (pattrib->physt && pphy_info)
					rx_query_phy_status(precvframe, pphy_info);

				rtw_recv_entry(precvframe);
			} else {
#ifdef CONFIG_FW_C2H_PKT
				if (pattrib->pkt_rpt_type == C2H_PACKET)
					rtw_hal_c2h_pkt_pre_hdl(padapter, precvframe->u.hdr.rx_data, pattrib->pkt_len);
				else {
					RTW_INFO("%s: [WARNNING] RX type(%d) not be handled!\n",
						__func__, pattrib->pkt_rpt_type);
				}
#endif
				rtw_free_recvframe(precvframe, pfree_recv_queue);
			}

			*((dma_addr_t *) skb->cb) = pci_map_single(pdvobjpriv->ppcidev, skb_tail_pointer(skb), precvpriv->rxbuffersize, PCI_DMA_FROMDEVICE);
		}
done:

		SET_RX_BUFFER_PHYSICAL_LOW_8723D(rx_bufdesc, (*((dma_addr_t *)skb->cb) & DMA_BIT_MASK(32)));
		SET_RX_BUFFER_PHYSICAL_HIGH_8723D(rx_bufdesc, (*((dma_addr_t *)skb->cb) >> 32));
		SET_RX_BUFFER_DESC_DATA_LENGTH_8723D(rx_bufdesc, precvpriv->rxbuffersize);

		prxring->idx = (prxring->idx + 1) % precvpriv->rxringcount;
		rtw_write16(padapter, REG_RXQ_TXBD_IDX_8723D, prxring->idx);

		remaing_rxdesc--;
	}

}

void rtl8723de_recv_tasklet(void *priv)
{
	_irqL	irqL;
	_adapter	*padapter = (_adapter *)priv;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	rtl8723de_rx_mpdu(padapter);
	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);
	pHalData->IntrMask[0] |= (IMR_ROK_8723D | IMR_RDU_8723D);
	pHalData->IntrMask[1] |= IMR_RXFOVW_8723D;
	rtw_write32(padapter, REG_HIMR0_8723D, pHalData->IntrMask[0]);
	rtw_write32(padapter, REG_HIMR1_8723D, pHalData->IntrMask[1]);
	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);
}

static u8 pci_read8(struct intf_hdl *pintfhdl, u32 addr)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;

	/*	printk("%s, addr=%08x,  val=%02x\n", __func__, addr,  readb((u8 *)pdvobjpriv->pci_mem_start + addr)); */
	return 0xff & readb((u8 *)pdvobjpriv->pci_mem_start + addr);
}

static u16 pci_read16(struct intf_hdl *pintfhdl, u32 addr)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;
	/*	printk("%s, addr=%08x,  val=%04x\n", __func__, addr,  readw((u8 *)pdvobjpriv->pci_mem_start + addr)); */
	return readw((u8 *)pdvobjpriv->pci_mem_start + addr);
}

static u32 pci_read32(struct intf_hdl *pintfhdl, u32 addr)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;

	/*	printk("%s, addr=%08x,  val=%08x\n", __func__, addr,  readl((u8 *)pdvobjpriv->pci_mem_start + addr)); */
	return readl((u8 *)pdvobjpriv->pci_mem_start + addr);
}

/* 2009.12.23. by tynli. Suggested by SD1 victorh. For ASPM hang on AMD and Nvidia.
 * 20100212 Tynli: Do read IO operation after write for all PCI bridge suggested by SD1.
 * Origianally this is only for INTEL. */
static int pci_write8(struct intf_hdl *pintfhdl, u32 addr, u8 val)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;

	writeb(val, (u8 *)pdvobjpriv->pci_mem_start + addr);
	/* readb((u8 *)pdvobjpriv->pci_mem_start + addr); */
	return 1;
}

static int pci_write16(struct intf_hdl *pintfhdl, u32 addr, u16 val)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;

	writew(val, (u8 *)pdvobjpriv->pci_mem_start + addr);
	/* readw((u8 *)pdvobjpriv->pci_mem_start + addr); */
	return 2;
}

static int pci_write32(struct intf_hdl *pintfhdl, u32 addr, u32 val)
{
	struct dvobj_priv  *pdvobjpriv = (struct dvobj_priv *)pintfhdl->pintf_dev;

	writel(val, (u8 *)pdvobjpriv->pci_mem_start + addr);
	/* readl((u8 *)pdvobjpriv->pci_mem_start + addr); */
	return 4;
}


static void pci_read_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{

}

static void pci_write_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{

}

static u32 pci_read_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	return 0;
}

void rtl8723de_xmit_tasklet(void *priv)
{
	/* _irqL irqL; */
	_adapter *padapter = (_adapter *)priv;
	/* struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter); */
	/* PRT_ISR_CONTENT	pisr_content = &pdvobjpriv->isr_content; */

#if 1
	/* try to deal with the pending packets */
	rtl8723de_xmitframe_resume(padapter);
#else
	_enter_critical(&pdvobjpriv->irq_th_lock, &irqL);

	if (pisr_content->IntArray[0] & IMR_BCNDERR0_88E) {
		/* RTW_INFO("beacon interrupt!\n"); */
		rtl8188ee_tx_isr(padapter, BCN_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_MGNTDOK) {
		/* RTW_INFO("Manage ok interrupt!\n"); */
		rtl8188ee_tx_isr(padapter, MGT_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_HIGHDOK) {
		/* RTW_INFO("HIGH_QUEUE ok interrupt!\n"); */
		rtl8188ee_tx_isr(padapter, HIGH_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_BKDOK) {
		/* RTW_INFO("BK Tx OK interrupt!\n"); */
		rtl8188ee_tx_isr(padapter, BK_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_BEDOK) {
		/* RTW_INFO("BE TX OK interrupt!\n"); */
		rtl8188ee_tx_isr(padapter, BE_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_VIDOK) {
		/* RTW_INFO("VI TX OK interrupt!\n"); */
		rtl8188ee_tx_isr(padapter, VI_QUEUE_INX);
	}

	if (pisr_content->IntArray[0] & IMR_VODOK) {
		/* RTW_INFO("Vo TX OK interrupt!\n"); */
		rtl8188ee_tx_isr(padapter, VO_QUEUE_INX);
	}

	_exit_critical(&pdvobjpriv->irq_th_lock, &irqL);

	if (check_fwstate(&padapter->mlmepriv, _FW_UNDER_SURVEY) != _TRUE) {
		/* try to deal with the pending packets */
		rtl8723de_xmitframe_resume(padapter);
	}
#endif

}

static u32 pci_write_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{
	_adapter			*padapter = (_adapter *)pintfhdl->padapter;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0))
	netif_trans_update(padapter->pnetdev);
#else
	padapter->pnetdev->trans_start = jiffies;
#endif

	return 0;
}

void rtl8723de_set_intf_ops(struct _io_ops	*pops)
{

	_rtw_memset((u8 *)pops, 0, sizeof(struct _io_ops));

	pops->_read8 = &pci_read8;
	pops->_read16 = &pci_read16;
	pops->_read32 = &pci_read32;

	pops->_read_mem = &pci_read_mem;
	pops->_read_port = &pci_read_port;

	pops->_write8 = &pci_write8;
	pops->_write16 = &pci_write16;
	pops->_write32 = &pci_write32;

	pops->_write_mem = &pci_write_mem;
	pops->_write_port = &pci_write_port;


}
