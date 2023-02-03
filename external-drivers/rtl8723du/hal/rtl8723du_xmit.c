// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTL8723DU_XMIT_C_

#include <rtl8723d_hal.h>

void _dbg_dump_tx_info(struct adapter * adapt, int frame_tag, struct tx_desc *ptxdesc)
{
	u8 bDumpTxPkt;
	u8 bDumpTxDesc = false;

	rtw_hal_get_def_var(adapt, HAL_DEF_DBG_DUMP_TXPKT, &(bDumpTxPkt));

	if (bDumpTxPkt == 1) { /* dump txdesc for data frame */
		RTW_INFO("dump tx_desc for data frame\n");
		if ((frame_tag & 0x0f) == DATA_FRAMETAG)
			bDumpTxDesc = true;
	} else if (bDumpTxPkt == 2) { /* dump txdesc for mgnt frame */
		RTW_INFO("dump tx_desc for mgnt frame\n");
		if ((frame_tag & 0x0f) == MGNT_FRAMETAG)
			bDumpTxDesc = true;
	} else if (bDumpTxPkt == 3) { /* dump early info */
	}

	if (bDumpTxDesc) {
		RTW_INFO("=====================================\n");
		RTW_INFO("Offset00(0x%08x)\n", ptxdesc->txdw0);
		RTW_INFO("Offset04(0x%08x)\n", ptxdesc->txdw1);
		RTW_INFO("Offset08(0x%08x)\n", ptxdesc->txdw2);
		RTW_INFO("Offset12(0x%08x)\n", ptxdesc->txdw3);
		RTW_INFO("Offset16(0x%08x)\n", ptxdesc->txdw4);
		RTW_INFO("Offset20(0x%08x)\n", ptxdesc->txdw5);
		RTW_INFO("Offset24(0x%08x)\n", ptxdesc->txdw6);
		RTW_INFO("Offset28(0x%08x)\n", ptxdesc->txdw7);
#if defined(TXDESC_40_BYTES) || defined(TXDESC_64_BYTES)
		RTW_INFO("Offset32(0x%08x)\n", ptxdesc->txdw8);
		RTW_INFO("Offset36(0x%08x)\n", ptxdesc->txdw9);
#endif
#if defined(TXDESC_64_BYTES)
		RTW_INFO("Offset40(0x%08x)\n", ptxdesc->txdw10);
		RTW_INFO("Offset44(0x%08x)\n", ptxdesc->txdw11);
		RTW_INFO("Offset48(0x%08x)\n", ptxdesc->txdw12);
		RTW_INFO("Offset52(0x%08x)\n", ptxdesc->txdw13);
		RTW_INFO("Offset56(0x%08x)\n", ptxdesc->txdw14);
		RTW_INFO("Offset58(0x%08x)\n", ptxdesc->txdw15);
#endif
		RTW_INFO("=====================================\n");
	}
}

int rtl8723du_init_xmit_priv(struct adapter * adapt)
{
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	tasklet_init(&pxmitpriv->xmit_tasklet,
		     (void (*))rtl8723du_xmit_tasklet,
		     (unsigned long)adapt);
	return _SUCCESS;
}

void rtl8723du_free_xmit_priv(struct adapter * adapt)
{
}

static int urb_zero_packet_chk(struct adapter * adapt, int sz)
{
	u8 blnSetTxDescOffset;
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	blnSetTxDescOffset = (((sz + TXDESC_SIZE) % pHalData->UsbBulkOutSize) == 0) ? 1 : 0;

	return blnSetTxDescOffset;
}

static int update_txdesc(struct xmit_frame *pxmitframe, u8 *pmem, int sz, u8 bagg_pkt)
{
	int pull = 0;

	struct adapter * adapt = pxmitframe->adapt;
	struct tx_desc *ptxdesc = (struct tx_desc *)pmem;

	if ((PACKET_OFFSET_SZ != 0)
	    && (false == bagg_pkt)
	    && (urb_zero_packet_chk(adapt, sz) == 0)) {
		ptxdesc = (struct tx_desc *)(pmem + PACKET_OFFSET_SZ);
		pull = 1;
		pxmitframe->pkt_offset--;
	}

	memset(ptxdesc, 0, sizeof(struct tx_desc));

	rtl8723d_update_txdesc(pxmitframe, (u8 *)ptxdesc);
	_dbg_dump_tx_info(adapt, pxmitframe->frame_tag, ptxdesc);
	return pull;
}

static int rtw_dump_xframe(struct adapter * adapt, struct xmit_frame *pxmitframe)
{
	int ret = _SUCCESS;
	int inner_ret = _SUCCESS;
	int t, sz, w_sz, pull = 0;
	u8 *mem_addr;
	u32 ff_hwaddr;
	struct xmit_buf *pxmitbuf = pxmitframe->pxmitbuf;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	struct security_priv *psecuritypriv = &adapt->securitypriv;

	if ((pxmitframe->frame_tag == DATA_FRAMETAG) &&
	    (pxmitframe->attrib.ether_type != 0x0806) &&
	    (pxmitframe->attrib.ether_type != 0x888e) &&
	    (pxmitframe->attrib.dhcp_pkt != 1))
		rtw_issue_addbareq_cmd(adapt, pxmitframe);

	mem_addr = pxmitframe->buf_addr;


	for (t = 0; t < pattrib->nr_frags; t++) {
		if (inner_ret != _SUCCESS && ret == _SUCCESS)
			ret = _FAIL;

		if (t != (pattrib->nr_frags - 1)) {

			sz = pxmitpriv->frag_len;
			sz = sz - 4 - (psecuritypriv->sw_encrypt ? 0 : pattrib->icv_len);
		} else /* no frag */
			sz = pattrib->last_txcmdsz;

		pull = update_txdesc(pxmitframe, mem_addr, sz, false);
		/* rtl8723d_update_txdesc(pxmitframe, mem_addr+PACKET_OFFSET_SZ); */

		if (pull) {
			mem_addr += PACKET_OFFSET_SZ; /* pull txdesc head */

			/* pxmitbuf->pbuf = mem_addr; */
			pxmitframe->buf_addr = mem_addr;

			w_sz = sz + TXDESC_SIZE;
		} else
			w_sz = sz + TXDESC_SIZE + PACKET_OFFSET_SZ;

		ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);
		inner_ret = rtw_write_port(adapt, ff_hwaddr, w_sz, (unsigned char *)pxmitbuf);
		rtw_count_tx_stats(adapt, pxmitframe, sz);

		mem_addr += w_sz;

		mem_addr = (u8 *)RND4(((SIZE_PTR)(mem_addr)));
	}

	rtw_free_xmitframe(pxmitpriv, pxmitframe);

	if (ret != _SUCCESS)
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_UNKNOWN);

	return ret;
}

int rtl8723du_xmitframe_complete(struct adapter * adapt, struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf)
{

	struct hw_xmit *phwxmits;
	int hwentry;
	struct xmit_frame *pxmitframe = NULL;
	int res = _SUCCESS, xcnt = 0;

	phwxmits = pxmitpriv->hwxmits;
	hwentry = pxmitpriv->hwxmit_entry;


	if (!pxmitbuf) {
		pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
		if (!pxmitbuf)
			return false;
	}


	do {
		pxmitframe = rtw_dequeue_xframe(pxmitpriv, phwxmits, hwentry);

		if (pxmitframe) {
			pxmitframe->pxmitbuf = pxmitbuf;

			pxmitframe->buf_addr = pxmitbuf->pbuf;

			pxmitbuf->priv_data = pxmitframe;

			if ((pxmitframe->frame_tag & 0x0f) == DATA_FRAMETAG) {
				if (pxmitframe->attrib.priority <= 15) /* TID0~15 */
					res = rtw_xmitframe_coalesce(adapt, pxmitframe->pkt, pxmitframe);

				rtw_os_xmit_complete(adapt, pxmitframe); /* always return ndis_packet after rtw_xmitframe_coalesce */
			}




			if (res == _SUCCESS)
				rtw_dump_xframe(adapt, pxmitframe);
			else {
				rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
				rtw_free_xmitframe(pxmitpriv, pxmitframe);
			}

			xcnt++;

		} else {
			rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
			return false;
		}

		break;

	} while (0/*xcnt < (NR_XMITFRAME >> 3)*/);

	return true;

}

static int xmitframe_direct(struct adapter * adapt, struct xmit_frame *pxmitframe)
{
	int res = _SUCCESS;


	res = rtw_xmitframe_coalesce(adapt, pxmitframe->pkt, pxmitframe);
	if (res == _SUCCESS)
		rtw_dump_xframe(adapt, pxmitframe);

	return res;
}

/*
 * Return
 *	true	dump packet directly
 *	false	enqueue packet
 */
static int pre_xmitframe(struct adapter * adapt, struct xmit_frame *pxmitframe)
{
	unsigned long irqL;
	int res;
	struct xmit_buf *pxmitbuf = NULL;
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;

	_enter_critical_bh(&pxmitpriv->lock, &irqL);

	if (rtw_txframes_sta_ac_pending(adapt, pattrib) > 0)
		goto enqueue;

	if (rtw_xmit_ac_blocked(adapt))
		goto enqueue;

	if (DEV_STA_LG_NUM(adapt->dvobj))
		goto enqueue;

	pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
	if (!pxmitbuf)
		goto enqueue;

	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	pxmitframe->pxmitbuf = pxmitbuf;
	pxmitframe->buf_addr = pxmitbuf->pbuf;
	pxmitbuf->priv_data = pxmitframe;

	if (xmitframe_direct(adapt, pxmitframe) != _SUCCESS) {
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
		rtw_free_xmitframe(pxmitpriv, pxmitframe);
	}

	return true;

enqueue:
	res = rtw_xmitframe_enqueue(adapt, pxmitframe);
	_exit_critical_bh(&pxmitpriv->lock, &irqL);

	if (res != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		pxmitpriv->tx_drop++;
		return true;
	}

	return false;
}

int rtl8723du_mgnt_xmit(struct adapter * adapt, struct xmit_frame *pmgntframe)
{
	return rtw_dump_xframe(adapt, pmgntframe);
}

/*
 * Return
 *	true	dump packet directly ok
 *	false	temporary can't transmit packets to hardware
 */
int rtl8723du_hal_xmit(struct adapter * adapt, struct xmit_frame *pxmitframe)
{
	return pre_xmitframe(adapt, pxmitframe);
}

int rtl8723du_hal_xmitframe_enqueue(struct adapter * adapt, struct xmit_frame *pxmitframe)
{
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	int err;

	err = rtw_xmitframe_enqueue(adapt, pxmitframe);
	if (err != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		pxmitpriv->tx_drop++;
	} else {
		tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
	}

	return err;

}

void rtl8723d_cal_txdesc_chksum(struct tx_desc *ptxdesc)
{
	__le16 *usPtr = (__le16 *)ptxdesc;
	u32 count;
	u32 index;
	u16 checksum = 0;


	/* Clear first */
	ptxdesc->txdw7 &= cpu_to_le32(0xffff0000);

	/*
	 * checksume is always calculated by first 32 bytes,
	 * and it doesn't depend on TX DESC length.
	 * Thomas,Lucas@SD4,20130515
	 */
	count = 16;
	for (index = 0; index < count; index++)
		checksum ^= le16_to_cpu(*(usPtr + index));

	/* avoid zero checksum make tx hang */
	checksum = ~checksum;

	ptxdesc->txdw7 |= cpu_to_le32(checksum & 0x0000ffff);
}
