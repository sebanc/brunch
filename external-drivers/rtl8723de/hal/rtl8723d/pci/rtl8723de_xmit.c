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
#define _RTL8723DE_XMIT_C_

/*#include <drv_types.h>*/
#include <rtl8723d_hal.h>

static void _dbg_dump_tx_info(_adapter	*padapter, int frame_tag, u8 *ptxdesc)
{
	u8 bDumpTxPkt;
	u8 bDumpTxDesc = _FALSE;

	rtw_hal_get_def_var(padapter, HAL_DEF_DBG_DUMP_TXPKT, &(bDumpTxPkt));

	if (bDumpTxPkt == 1) {/* dump txdesc for data frame */
		RTW_INFO("dump tx_desc for data frame\n");
		if ((frame_tag & 0x0f) == DATA_FRAMETAG)
			bDumpTxDesc = _TRUE;
	} else if (bDumpTxPkt == 2) {/* dump txdesc for mgnt frame */
		RTW_INFO("dump tx_desc for mgnt frame\n");
		if ((frame_tag & 0x0f) == MGNT_FRAMETAG)
			bDumpTxDesc = _TRUE;
	} else if (bDumpTxPkt == 3) {/* dump early info */
	}

	if (bDumpTxDesc) {
		/* ptxdesc->txdw4 = cpu_to_le32(0x00001006);*/ /*RTS Rate=24M */
		/* ptxdesc->txdw6 = 0x6666f800; */
		RTW_INFO("=====================================\n");
		RTW_INFO("Offset00(0x%08x)\n", *((u32 *)(ptxdesc)));
		RTW_INFO("Offset04(0x%08x)\n", *((u32 *)(ptxdesc + 4)));
		RTW_INFO("Offset08(0x%08x)\n", *((u32 *)(ptxdesc + 8)));
		RTW_INFO("Offset12(0x%08x)\n", *((u32 *)(ptxdesc + 12)));
		RTW_INFO("Offset16(0x%08x)\n", *((u32 *)(ptxdesc + 16)));
		RTW_INFO("Offset20(0x%08x)\n", *((u32 *)(ptxdesc + 20)));
		RTW_INFO("Offset24(0x%08x)\n", *((u32 *)(ptxdesc + 24)));
		RTW_INFO("Offset28(0x%08x)\n", *((u32 *)(ptxdesc + 28)));
		RTW_INFO("Offset32(0x%08x)\n", *((u32 *)(ptxdesc + 32)));
		RTW_INFO("Offset36(0x%08x)\n", *((u32 *)(ptxdesc + 36)));
		RTW_INFO("=====================================\n");
	}

}

s32	rtl8723de_init_xmit_priv(_adapter *padapter)
{
	s32	ret = _SUCCESS;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	_rtw_spinlock_init(&pdvobjpriv->irq_th_lock);

#ifdef PLATFORM_LINUX
	tasklet_init(&pxmitpriv->xmit_tasklet,
		     (void(*)(unsigned long))rtl8723de_xmit_tasklet,
		     (unsigned long)padapter);
#endif

	return ret;
}

void	rtl8723de_free_xmit_priv(_adapter *padapter)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	_rtw_spinlock_free(&pdvobjpriv->irq_th_lock);
}

s32 rtl8723de_enqueue_xmitbuf(struct rtw_tx_ring	*ring, struct xmit_buf *pxmitbuf)
{
	_irqL irqL;
	_queue *ppending_queue = &ring->queue;


	/* RTW_INFO("+enqueue_xmitbuf\n"); */

	if (pxmitbuf == NULL)
		return _FAIL;

	/* _enter_critical(&ppending_queue->lock, &irqL); */

	rtw_list_delete(&pxmitbuf->list);

	rtw_list_insert_tail(&(pxmitbuf->list), get_list_head(ppending_queue));

	ring->qlen++;

	/* RTW_INFO("FREE, free_xmitbuf_cnt=%d\n", pxmitpriv->free_xmitbuf_cnt); */

	/* _exit_critical(&ppending_queue->lock, &irqL); */


	return _SUCCESS;
}

struct xmit_buf *rtl8723de_dequeue_xmitbuf(struct rtw_tx_ring	*ring)
{
	_irqL irqL;
	_list *plist, *phead;
	struct xmit_buf *pxmitbuf =  NULL;
	_queue *ppending_queue = &ring->queue;


	/* _enter_critical(&ppending_queue->lock, &irqL); */

	if (_rtw_queue_empty(ppending_queue) == _TRUE)
		pxmitbuf = NULL;
	else {

		phead = get_list_head(ppending_queue);

		plist = get_next(phead);

		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);

		rtw_list_delete(&(pxmitbuf->list));

		ring->qlen--;
	}

	/* _exit_critical(&ppending_queue->lock, &irqL); */


	return pxmitbuf;
}

static u16 ffaddr2dma(u32 addr)
{
	u16	dma_ctrl;

	switch (addr) {
	case VO_QUEUE_INX:
		dma_ctrl = BIT(3);
		break;
	case VI_QUEUE_INX:
		dma_ctrl = BIT(2);
		break;
	case BE_QUEUE_INX:
		dma_ctrl = BIT(1);
		break;
	case BK_QUEUE_INX:
		dma_ctrl = BIT(0);
		break;
	case BCN_QUEUE_INX:
		dma_ctrl = BIT(4);
		break;
	case MGT_QUEUE_INX:
		dma_ctrl = BIT(6);
		break;
	case HIGH_QUEUE_INX:
		dma_ctrl = BIT(7);
		break;
	default:
		dma_ctrl = 0;
		break;
	}

	return dma_ctrl;
}

/*
 * Fill tx buffer desciptor. Map each buffer address in tx buffer descriptor segment
 * Designed for tx buffer descriptor architecture
 * Input *pmem: pointer to the Tx Buffer Descriptor
 */
static void update_txbufdesc(struct xmit_frame *pxmitframe, u8 *pmem, s32 sz)
{
	_adapter			*padapter = pxmitframe->padapter;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	dma_addr_t		mapping;
	u32				i = 0;
	u16				seg_num = 2 << TX_BUFFER_SEG_NUM; /* 0: 2 seg, 1: 4 seg, 2: 8 seg. */
	u16				page_size_length = 0;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	mapping = pci_map_single(pdvobjpriv->ppcidev, pxmitframe->buf_addr , sz + TX_WIFI_INFO_SIZE, PCI_DMA_TODEVICE);

	/* Calculate page size. Total buffer length including TX_WIFI_INFO and PacketLen */
	page_size_length = (sz + TX_WIFI_INFO_SIZE - 1) / PAGE_SIZE_TX_8723D + 1;

	/* Reset all tx buffer desciprtor content */
	/* -- Reset first element */
	SET_TX_BUFF_DESC_LEN_0_8723D(pmem, 0);
	SET_TX_BUFF_DESC_PSB_8723D(pmem, 0);
	SET_TX_BUFF_DESC_OWN_8723D(pmem, 0);
	/* -- Reset second and other element */
	for (i = 1 ; i < seg_num ; i++) {
		SET_TXBUFFER_DESC_LEN_WITH_OFFSET(pmem, i, 0);
		SET_TXBUFFER_DESC_AMSDU_WITH_OFFSET(pmem, i, 0);
		SET_TXBUFFER_DESC_ADD_LOW_WITH_OFFSET(pmem, i, 0);
	}

	/* Fill buffer length of the first buffer, */
	/* For 8723de, it is required that TX_WIFI_INFO is put in first segment, and the size */
	/*		of the first segment cannot be larger than TX_WIFI_INFO_SIZE. */
	SET_TX_BUFF_DESC_LEN_0_8723D(pmem, TX_WIFI_INFO_SIZE);
	SET_TX_BUFF_DESC_PSB_8723D(pmem, page_size_length);
	SET_TX_BUFF_DESC_ADDR_LOW_0_8723D(pmem, (mapping & DMA_BIT_MASK(32)));
	SET_TX_BUFF_DESC_ADDR_HIGH_0_8723D(pmem, (mapping >> 32));

	/* It is assumed that in linux implementation, packet is coalesced in only one buffer */
	/* Extension mode is not supported here */
	SET_TXBUFFER_DESC_LEN_WITH_OFFSET(pmem, 1, sz);
	SET_TXBUFFER_DESC_AMSDU_WITH_OFFSET(pmem, 1, 0); /* don't using extendsion mode. */
	SET_TXBUFFER_DESC_ADD_LOW_WITH_OFFSET(pmem, 1, ((mapping + TX_WIFI_INFO_SIZE) & DMA_BIT_MASK(32)));
	SET_TXBUFFER_DESC_ADD_HIGT_WITH_OFFSET(pmem, 1, ((mapping + TX_WIFI_INFO_SIZE) >> 32));

#if 0
	buf_desc_debug("TX:%s, tx_buf_desc = 0x%08x\n", __func__, (u32)pmem);
	buf_desc_debug("TX:%s, Offset00(0x%08x)\n", __func__, *((u32 *)(pmem)));
	buf_desc_debug("TX:%s, Offset04(0x%08x)\n", __func__, *((u32 *)(pmem + 4)));
	buf_desc_debug("TX:%s, Offset08(0x%08x)\n", __func__, *((u32 *)(pmem + 8)));
	buf_desc_debug("TX:%s, Offset12(0x%08x)\n", __func__, *((u32 *)(pmem + 12)));
#endif
}

static s32 update_txdesc(struct xmit_frame *pxmitframe)
{
	_adapter			*padapter = pxmitframe->padapter;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	u8				*ptxdesc =  pxmitframe->buf_addr;

	_rtw_memset(ptxdesc, 0, TXDESC_SIZE);

	rtl8723d_update_txdesc(pxmitframe, ptxdesc);

	/* Don't set value before, because rtl8723d_update_txdesc() will clean ptxdesc. */

	/* 4 offset 0 */
	/* SET_TX_DESC_FIRST_SEG_8723D(ptxdesc, 1); */
	/* SET_TX_DESC_LAST_SEG_8723D(ptxdesc, 1); */
	/* SET_TX_DESC_OWN_8723D(ptxdesc, 1); */

	/* SET_TX_DESC_TX_BUFFER_SIZE_8723D(ptxdesc, sz); */
	/* SET_TX_DESC_TX_BUFFER_ADDRESS_8723D(ptxdesc, mapping); */

	/* debug */
	_dbg_dump_tx_info(padapter, pxmitframe->frame_tag, ptxdesc);

	return 0;
}

static u8 *get_txbufdesc(_adapter *padapter, u8 queue_index)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct rtw_tx_ring	*ring;
	u8	*pbufdesc;
	int	idx;

	ring = &pxmitpriv->tx_ring[queue_index];

	if (queue_index != BCN_QUEUE_INX)
		idx = (ring->idx + ring->qlen) % ring->entries;
	else
		idx = 0;

	pbufdesc = (u8 *)&ring->buf_desc[idx];

	if ((queue_index != BCN_QUEUE_INX) && (ring->entries - ring->qlen == 1)) {
		/*
		 * If we fall into this case, we may enlarge checking boundary in
		 * check_nic_enough_desc().
		 */
		RTW_INFO("No more TX desc@%d, ring->idx = %d,idx = %d\n", queue_index, ring->idx, idx);
		return NULL;
	}

	return pbufdesc;
}

u16 get_txbufdesc_idx_hwaddr(u16 ff_hwaddr)
{
	u16	txbuf_desc_addr = REG_BEQ_TXBD_IDX_8723D;

	switch (ff_hwaddr) {
	case BK_QUEUE_INX:
		txbuf_desc_addr = REG_BKQ_TXBD_IDX_8723D;
		break;

	case BE_QUEUE_INX:
		txbuf_desc_addr = REG_BEQ_TXBD_IDX_8723D;
		break;

	case VI_QUEUE_INX:
		txbuf_desc_addr = REG_VIQ_TXBD_IDX_8723D;
		break;

	case VO_QUEUE_INX:
		txbuf_desc_addr = REG_VOQ_TXBD_IDX_8723D;
		break;

	case BCN_QUEUE_INX:
		txbuf_desc_addr = REG_BEQ_TXBD_IDX_8723D;   /*need check*/
		break;

	case TXCMD_QUEUE_INX:
		txbuf_desc_addr = REG_BEQ_TXBD_IDX_8723D;   /*need check*/
		break;

	case MGT_QUEUE_INX:
		txbuf_desc_addr = REG_MGQ_TXBD_IDX_8723D;
		break;

	case HIGH_QUEUE_INX:
		txbuf_desc_addr = REG_HI0Q_TXBD_IDX_8723D;
		break;

	default:
		break;
	}

	return txbuf_desc_addr;
}

/*
  * Update Read/Write pointer
  *			Read pointer is h/w descriptor index
  *		Write pointer is host desciptor index: For tx side, if own bit is set in packet index n,
  *										host pointer (write pointer) point to index n + 1.)
  */
void fill_txbufdesc_own(_adapter *padapter, u8 *txbufdesc, u16 queue_idx, struct rtw_tx_ring *ptxring)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	u16 host_wp;

	if (queue_idx == BCN_QUEUE_INX) {
		/*Adapter->CurrentTXWritePoint[QueueIndex] = Adapter->CurrentTXReadPoint[QueueIndex] =0;*/

		SET_TX_BUFF_DESC_OWN_8723D(txbufdesc, 1);

		/* kick start */
		rtw_write8(padapter, REG_RX_RXBD_NUM_8723D + 1,
			rtw_read8(padapter, REG_RX_RXBD_NUM_8723D + 1) | BIT(4));

		return;
	}

	/* update h/w index
	 * for tx side, if own bit is set in packet index n, host pointer (write pointer) point to index n + 1.
	 */
	/* for current tx packet, enqueue has been ring->qlen++ before.
	 * so, host_wp = ring->idx + ring->qlen.
	 */
	host_wp = (ptxring->idx + ptxring->qlen) % ptxring->entries;
	rtw_write16(padapter, get_txbufdesc_idx_hwaddr(queue_idx), host_wp);
}

s32 rtw_dump_xframe(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	s32 ret = _SUCCESS;
	s32 inner_ret = _SUCCESS;
	_irqL irqL;
	int	t, sz, w_sz, pull = 0;
	u32	ff_hwaddr;
	struct xmit_buf		*pxmitbuf = pxmitframe->pxmitbuf;
	struct pkt_attrib	*pattrib = &pxmitframe->attrib;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct security_priv	*psecuritypriv = &padapter->securitypriv;
	u8 *ptxbufdesc;
	struct rtw_tx_ring *ptx_ring;

	_adapter *pri_adapter = GET_PRIMARY_ADAPTER(padapter);
	struct dvobj_priv *pri_dvobjpriv = adapter_to_dvobj(pri_adapter);


	if ((pxmitframe->frame_tag == DATA_FRAMETAG) &&
	    (pxmitframe->attrib.ether_type != 0x0806) &&
	    (pxmitframe->attrib.ether_type != 0x888e) &&
	    (pxmitframe->attrib.dhcp_pkt != 1))

		rtw_issue_addbareq_cmd(padapter, pxmitframe);

	/* mem_addr = pxmitframe->buf_addr; */


	for (t = 0; t < pattrib->nr_frags; t++) {
		if (inner_ret != _SUCCESS && ret == _SUCCESS)
			ret = _FAIL;

		if (t != (pattrib->nr_frags - 1)) {

			sz = pxmitpriv->frag_len;
			sz = sz - 4 - (psecuritypriv->sw_encrypt ? 0 : pattrib->icv_len);
		} else   /* no frag */
			sz = pattrib->last_txcmdsz;

		ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);

		_enter_critical(&(pri_dvobjpriv->irq_th_lock), &irqL);

		ptxbufdesc = get_txbufdesc(pri_adapter, ff_hwaddr);

		ptx_ring = &(pri_adapter->xmitpriv.tx_ring[ff_hwaddr]);

		if (BCN_QUEUE_INX == ff_hwaddr)
			padapter->xmitpriv.beaconDMAing = _TRUE;

		if (ptxbufdesc == NULL) {
			_exit_critical(&pri_dvobjpriv->irq_th_lock, &irqL);
			rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_TX_DESC_NA);
			rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
			RTW_INFO("##### Tx buffer desc unavailable !#####\n");
			break;
		}
		update_txdesc(pxmitframe);

		/* update_txbufdesc() must be called after update_txdesc().    */
		/* It rely on update_txbufdesc() to map it into non cache memory */
		update_txbufdesc(pxmitframe, ptxbufdesc, sz);

		if (pxmitbuf->buf_tag != XMITBUF_CMD)
			rtl8723de_enqueue_xmitbuf(ptx_ring, pxmitbuf);

		pxmitbuf->len = sz;
		w_sz = sz;

		/* Set descriptor fields before! */
		wmb();

		/*SET_TX_BUFF_DESC_OWN_8723D(ptxbufdesc, 1);*/
		fill_txbufdesc_own(pri_adapter, ptxbufdesc,
				   ff_hwaddr, ptx_ring);

		_exit_critical(&pri_dvobjpriv->irq_th_lock, &irqL);

		rtw_count_tx_stats(padapter, pxmitframe, sz);
		/* RTW_INFO("rtw_write_port, w_sz=%d, sz=%d, txdesc_sz=%d, tid=%d\n", w_sz, sz, w_sz-sz, pattrib->priority); */

		/* mem_addr += w_sz; */

		/* mem_addr = (u8 *)RND4(((SIZE_PTR)(mem_addr))); */

	}

	rtw_free_xmitframe(pxmitpriv, pxmitframe);

	if (ret != _SUCCESS)
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_UNKNOWN);

	return ret;
}

void rtl8723de_xmitframe_resume(_adapter *padapter)
{
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct xmit_frame *pxmitframe = NULL;
	struct xmit_buf	*pxmitbuf = NULL;
	int res = _SUCCESS, xcnt = 0;


	while (1) {
		if (RTW_CANNOT_RUN(padapter)) {
			RTW_INFO("rtl8188ee_xmitframe_resume => bDriverStopped or bSurpriseRemoved\n");
			break;
		}

		pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
		if (!pxmitbuf)
			break;

		pxmitframe =  rtw_dequeue_xframe(pxmitpriv, pxmitpriv->hwxmits, pxmitpriv->hwxmit_entry);

		if (pxmitframe) {
			pxmitframe->pxmitbuf = pxmitbuf;

			pxmitframe->buf_addr = pxmitbuf->pbuf;

			pxmitbuf->priv_data = pxmitframe;

			if ((pxmitframe->frame_tag & 0x0f) == DATA_FRAMETAG) {
				if (pxmitframe->attrib.priority <= 15)  /* TID0~15 */
					res = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);

				rtw_os_xmit_complete(padapter, pxmitframe);/* always return ndis_packet after rtw_xmitframe_coalesce */
			}


			if (res == _SUCCESS)
				rtw_dump_xframe(padapter, pxmitframe);
			else {
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				rtw_free_xmitframe(pxmitpriv, pxmitframe);
			}

			xcnt++;
		} else {
			rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
			break;
		}
	}
}

static u8 check_nic_enough_desc(_adapter *padapter, struct pkt_attrib *pattrib)
{
	u32 prio;

	switch (pattrib->qsel) {
	case 0:
	case 3:
		prio = BE_QUEUE_INX;
		break;
	case 1:
	case 2:
		prio = BK_QUEUE_INX;
		break;
	case 4:
	case 5:
		prio = VI_QUEUE_INX;
		break;
	case 6:
	case 7:
		prio = VO_QUEUE_INX;
		break;
	default:
		prio = BE_QUEUE_INX;
		break;
	}

	return check_tx_desc_resource(padapter, prio);
}

static s32 xmitframe_direct(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	s32 res = _SUCCESS;

	res = rtw_xmitframe_coalesce(padapter, pxmitframe->pkt, pxmitframe);
	if (res == _SUCCESS)
		rtw_dump_xframe(padapter, pxmitframe);

	return res;
}

/*
 * Return
 *	_TRUE	dump packet directly
 *	_FALSE	enqueue packet
 */
static s32 pre_xmitframe(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	_irqL irqL;
	s32 res;
	struct xmit_buf *pxmitbuf = NULL;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;

	_enter_critical_bh(&pxmitpriv->lock, &irqL);

	if ((rtw_txframes_sta_ac_pending(padapter, pattrib) > 0) ||
	    (check_nic_enough_desc(padapter, pattrib) == _FALSE))
		goto enqueue;

	if (rtw_xmit_ac_blocked(padapter) == _TRUE)
		goto enqueue;

	if (DEV_STA_LG_NUM(padapter->dvobj))
		goto enqueue;

	pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
	if (pxmitbuf == NULL)
		goto enqueue;

	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	pxmitframe->pxmitbuf = pxmitbuf;
	pxmitframe->buf_addr = pxmitbuf->pbuf;
	pxmitbuf->priv_data = pxmitframe;

	if (xmitframe_direct(padapter, pxmitframe) != _SUCCESS) {
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
		rtw_free_xmitframe(pxmitpriv, pxmitframe);
	}

	return _TRUE;

enqueue:
	res = rtw_xmitframe_enqueue(padapter, pxmitframe);
	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	if (res != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		/* Trick, make the statistics correct */
		pxmitpriv->tx_pkts--;
		pxmitpriv->tx_drop++;
		return _TRUE;
	}

	return _FALSE;
}

s32 rtl8723de_mgnt_xmit(_adapter *padapter, struct xmit_frame *pmgntframe)
{
	return rtw_dump_xframe(padapter, pmgntframe);
}

/*
 * Return
 *	_TRUE	dump packet directly ok
 *	_FALSE	temporary can't transmit packets to hardware
 */
s32	rtl8723de_hal_xmit(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	return pre_xmitframe(padapter, pxmitframe);
}

s32	rtl8723de_hal_xmitframe_enqueue(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
	s32 err;

	err = rtw_xmitframe_enqueue(padapter, pxmitframe);

	if (err != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		/* Trick, make the statistics correct */
		pxmitpriv->tx_pkts--;
		pxmitpriv->tx_drop++;
	} else {
		if (check_nic_enough_desc(padapter, &pxmitframe->attrib) == _TRUE) {
#ifdef PLATFORM_LINUX
			tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#endif
		}
	}

	return err;
}

#ifdef CONFIG_HOSTAPD_MLME

static void rtl8723de_hostap_mgnt_xmit_cb(struct urb *urb)
{
#ifdef PLATFORM_LINUX
	struct sk_buff *skb = (struct sk_buff *)urb->context;

	/* RTW_INFO("%s\n", __FUNCTION__); */

	rtw_skb_free(skb);
#endif
}

s32 rtl8723de_hostap_mgnt_xmit_entry(_adapter *padapter, _pkt *pkt)
{
#ifdef PLATFORM_LINUX
	u16 fc;
	int rc, len, pipe;
	unsigned int bmcst, tid, qsel;
	struct sk_buff *skb, *pxmit_skb;
	struct urb *urb;
	unsigned char *pxmitbuf;
	struct tx_desc *ptxdesc;
	struct rtw_ieee80211_hdr *tx_hdr;
	struct hostapd_priv *phostapdpriv = padapter->phostapdpriv;
	struct net_device *pnetdev = padapter->pnetdev;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv *pdvobj = adapter_to_dvobj(padapter);


	/* RTW_INFO("%s\n", __FUNCTION__); */

	skb = pkt;

	len = skb->len;
	tx_hdr = (struct rtw_ieee80211_hdr *)(skb->data);
	fc = le16_to_cpu(tx_hdr->frame_ctl);
	bmcst = IS_MCAST(tx_hdr->addr1);

	if ((fc & RTW_IEEE80211_FCTL_FTYPE) != RTW_IEEE80211_FTYPE_MGMT)
		goto _exit;

	pxmit_skb = rtw_skb_alloc(len + TXDESC_SIZE);

	if (!pxmit_skb)
		goto _exit;

	pxmitbuf = pxmit_skb->data;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		goto _exit;

	/* ----- fill tx desc ----- */
	ptxdesc = (struct tx_desc *)pxmitbuf;
	_rtw_memset(ptxdesc, 0, sizeof(*ptxdesc));

	/* offset 0 */
	ptxdesc->txdw0 |= cpu_to_le32(len & 0x0000ffff);
	ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE + OFFSET_SZ) << OFFSET_SHT) & 0x00ff0000); /* default = 32 bytes for TX Desc */
	ptxdesc->txdw0 |= cpu_to_le32(OWN | FSG | LSG);

	if (bmcst)
		ptxdesc->txdw0 |= cpu_to_le32(BIT(24));

	/* offset 4 */
	ptxdesc->txdw1 |= cpu_to_le32(0x00);/* MAC_ID */

	ptxdesc->txdw1 |= cpu_to_le32((0x12 << QSEL_SHT) & 0x00001f00);

	ptxdesc->txdw1 |= cpu_to_le32((0x06 << 16) & 0x000f0000);/* b mode */

	/* offset 8 */

	/* offset 12 */
	ptxdesc->txdw3 |= cpu_to_le32((le16_to_cpu(tx_hdr->seq_ctl) << 16) & 0xffff0000);

	/* offset 16 */
	ptxdesc->txdw4 |= cpu_to_le32(BIT(8));/* driver uses rate */

	/* offset 20 */

	rtl8188e_cal_txdesc_chksum(ptxdesc);
	/* ----- end of fill tx desc ----- */

	/* */
	skb_put(pxmit_skb, len + TXDESC_SIZE);
	pxmitbuf = pxmitbuf + TXDESC_SIZE;
	_rtw_memcpy(pxmitbuf, skb->data, len);

	/* RTW_INFO("mgnt_xmit, len=%x\n", pxmit_skb->len); */


	/* ----- prepare urb for submit ----- */

	/* translate DMA FIFO addr to pipehandle */
	/* pipe = ffaddr2pipehdl(pdvobj, MGT_QUEUE_INX); */
	pipe = usb_sndbulkpipe(pdvobj->pusbdev, pHalData->Queue2EPNum[(u8)MGT_QUEUE_INX] & 0x0f);

	usb_fill_bulk_urb(urb, pdvobj->pusbdev, pipe,
		pxmit_skb->data, pxmit_skb->len, rtl8723de_hostap_mgnt_xmit_cb, pxmit_skb);

	urb->transfer_flags |= URB_ZERO_PACKET;
	usb_anchor_urb(urb, &phostapdpriv->anchored);
	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc < 0) {
		usb_unanchor_urb(urb);
		kfree_skb(skb);
	}
	usb_free_urb(urb);


_exit:

	rtw_skb_free(skb);

#endif

	return 0;

}
#endif
