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
#define _RTL8821CE_HALMAC_C_
#include <drv_types.h>		/* struct dvobj_priv and etc. */
#include "../../hal_halmac.h"
#include "rtl8821ce.h"

static u8 pci_write_port_not_xmitframe(void *d,  u32 size, u8 *pBuf, u8 qsel)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	struct pci_dev *pdev = pobj->ppcidev;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	u32 page_size = 0;
	u8 *txbd;
#ifdef  CONFIG_64BIT
	u64 txbd_dma;
#else
	dma_addr_t txbd_dma;
#endif
	u8 ret = _SUCCESS;
	dma_addr_t mapping;

	u16 seg_num = 2 << TX_BUFFER_SEG_NUM;

	u16 page_size_length = 0;
#ifdef CONFIG_64BIT_DMA	
	u16 tmp = rtw_read16(padapter, REG_RX_RXBD_NUM_8821C);
	/* using 64bit */	
	rtw_write16(padapter, REG_RX_RXBD_NUM_8821C, tmp | 0x8000);
#endif

	rtw_hal_get_def_var(padapter, HAL_DEF_TX_PAGE_SIZE, &page_size);

	/* map TX DESC buf_addr (including TX DESC + tx data) */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	mapping = pci_map_single(pdev, pBuf,
		size+TX_WIFI_INFO_SIZE, PCI_DMA_TODEVICE);
	#else
	mapping = dma_map_single(&pdev->dev, pBuf,
		size+TX_WIFI_INFO_SIZE, DMA_TO_DEVICE);
	#endif


	/* Calculate page size.
	 * Total buffer length including TX_WIFI_INFO and PacketLen
	 */
	page_size_length =
		(size + TX_WIFI_INFO_SIZE) / page_size;

	if (((size + TX_WIFI_INFO_SIZE) % page_size) > 0)
		page_size_length++;

	#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	txbd = pci_alloc_consistent(pdev,
		sizeof(struct tx_buf_desc), &txbd_dma);
	#else
	txbd = dma_alloc_coherent(&pdev->dev,
		sizeof(struct tx_buf_desc), &txbd_dma, GFP_KERNEL);
	#endif

	if (!txbd) {
		#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
		pci_unmap_single(pdev, mapping,
			size + TX_WIFI_INFO_SIZE, PCI_DMA_FROMDEVICE);
		#else
		dma_unmap_single(&pdev->dev, mapping,
			size + TX_WIFI_INFO_SIZE, DMA_FROM_DEVICE);
		#endif

		return _FALSE;
	}
	if (qsel == HALMAC_TXDESC_QSEL_H2C_CMD) {
		rtw_write32(padapter, REG_H2CQ_TXBD_DESA_8821C,
			txbd_dma & DMA_BIT_MASK(32));
	#ifdef CONFIG_64BIT_DMA	
		rtw_write32(padapter, REG_H2CQ_TXBD_DESA_8821C + 4,
			txbd_dma >> 32);
	#endif

		rtw_write32(padapter, REG_H2CQ_TXBD_NUM_8821C,
			2 | ((RTL8821CE_SEG_NUM << 12) & 0x3000));
	} else {
		/* Set BCN BD Reg */
		rtw_write32(padapter, REG_BCNQ_TXBD_DESA_8821C,
			txbd_dma & DMA_BIT_MASK(32));
	#ifdef CONFIG_64BIT_DMA	
		rtw_write32(padapter, REG_BCNQ_TXBD_DESA_8821C + 4,
			txbd_dma >> 32);
	#endif
	}

	/*
	 * Reset all tx buffer desciprtor content
	 * -- Reset first element
	 */
	_rtw_memset(txbd, 0, sizeof(struct tx_buf_desc));

	/*
	 * Fill buffer length of the first buffer,
	 * For 8821ce, it is required that TX_WIFI_INFO is put in first segment,
	 * and the size of the first segment cannot be larger than
	 * TX_WIFI_INFO_SIZE.
	 */
	SET_TX_BD_TX_BUFF_SIZE0(txbd, TX_WIFI_INFO_SIZE);
	SET_TX_BD_PSB(txbd, page_size_length);
	/* starting addr of TXDESC */
	SET_TX_BD_PHYSICAL_ADDR0_LOW(txbd, mapping);
#ifdef CONFIG_64BIT_DMA 
	SET_TX_BD_PHYSICAL_ADDR0_HIGH(txbd, mapping>>32);
#endif

	/*
	 * It is assumed that in linux implementation, packet is coalesced
	 * in only one buffer. Extension mode is not supported here
	 */
	SET_TXBUFFER_DESC_LEN_WITH_OFFSET(txbd, 1, size);

	SET_TXBUFFER_DESC_ADD_LOW_WITH_OFFSET(txbd, 1,
				      mapping + TX_WIFI_INFO_SIZE); /* pkt */

#ifdef CONFIG_64BIT_DMA 
	SET_TXBUFFER_DESC_ADD_HIGH_WITH_OFFSET(txbd, 1,
				      (mapping + TX_WIFI_INFO_SIZE) >> 32); /* pkt */
#endif


	/* Need Comment */
	wmb();

	if (qsel == HALMAC_TXDESC_QSEL_H2C_CMD)
		rtw_write16(padapter, REG_H2CQ_TXBD_IDX, 1);
	else {
		/* fill_txbd_own*/
		SET_TX_BD_OWN(txbd, 1);
		/* kick start */
		rtw_write8(padapter, REG_RX_RXBD_NUM + 1,
			rtw_read8(padapter, REG_RX_RXBD_NUM + 1) | BIT(4));
	}

	udelay(100);

	#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	pci_free_consistent(pdev, sizeof(struct tx_buf_desc), txbd, txbd_dma);
	#else
	dma_free_coherent(&pdev->dev, sizeof(struct tx_buf_desc), txbd, txbd_dma);
	#endif

	#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	pci_unmap_single(pdev, mapping,
			size + TX_WIFI_INFO_SIZE,	 PCI_DMA_FROMDEVICE);
	#else
	dma_unmap_single(&pdev->dev, mapping,
			size + TX_WIFI_INFO_SIZE,	 DMA_FROM_DEVICE);
	#endif

	return ret;

}

static u8 pci_write_data_not_xmitframe(void *d, u8 *pBuf, u32 size, u8 qsel)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	struct halmac_adapter *halmac = dvobj_to_halmac((struct dvobj_priv *)d);
	struct halmac_api *api = HALMAC_GET_API(halmac);
	u32 desclen = 0;
	u8 *buf = NULL;
	u8 ret = _FALSE;

	if ((size + TXDESC_OFFSET) > MAX_CMDBUF_SZ) {
		RTW_INFO("%s: total buffer size(%d) > MAX_CMDBUF_SZ(%d)\n"
			, __func__, size + TXDESC_OFFSET, MAX_CMDBUF_SZ);
		return _FALSE;
	}

	rtw_halmac_get_tx_desc_size(pobj, &desclen);

	buf = rtw_zmalloc(desclen + size);

	if (!buf) {
		RTW_INFO("%s: rtw_zmalloc for rsvd Fail\n", __func__);
		return _FALSE;
	}

	_rtw_memcpy(buf + desclen, pBuf, size);

	SET_TX_DESC_TXPKTSIZE_8821C(buf, size);

	/* TX_DESC is not included in the data,
	 * driver needs to fill in the TX_DESC with qsel=h2c
	 * Offset in TX_DESC should be set to 0.
	 */
	if (qsel == HALMAC_TXDESC_QSEL_H2C_CMD)
		SET_TX_DESC_OFFSET_8821C(buf, 0);
	else
		SET_TX_DESC_OFFSET_8821C(buf, desclen);

	SET_TX_DESC_QSEL_8821C(buf, qsel);

	api->halmac_fill_txdesc_checksum(halmac, buf);

	ret = pci_write_port_not_xmitframe(d, size, buf, qsel);


	if (ret == _SUCCESS)
		ret = _TRUE;
	else
		ret = _FALSE;

	rtw_mfree(buf, desclen + size);

	return _TRUE;
}

static u8 pci_write_data_rsvd_page_xmitframe(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	struct xmit_priv        *pxmitpriv = &padapter->xmitpriv;
	struct xmit_frame       *pcmdframe = NULL;
	struct pkt_attrib       *pattrib = NULL;
	u32 desclen = 0;
	struct rtw_tx_ring *ring = &pxmitpriv->tx_ring[BCN_QUEUE_INX];
	struct pci_dev *pdev = pobj->ppcidev;
	struct xmit_buf       	*pxmitbuf = NULL;
	u8 DLBcnCount = 0;
	u32 poll = 0;
	u8 *txbd;
	bool bcn_valid = _FALSE;
	u8 *txdesc = NULL;
	dma_addr_t mapping;

	if (size + TXDESC_OFFSET > MAX_CMDBUF_SZ) {
		RTW_INFO("%s: total buffer size(%d) > MAX_CMDBUF_SZ(%d)\n"
			, __func__, size + TXDESC_OFFSET, MAX_CMDBUF_SZ);
		return _FALSE;
	}

	pcmdframe = rtw_alloc_cmdxmitframe(pxmitpriv);

	if (pcmdframe == NULL) {
		RTW_INFO("%s: alloc ReservedPagePacket fail!\n", __func__);
		return _FALSE;
	}

        pxmitbuf = pcmdframe->pxmitbuf;
	rtw_halmac_get_tx_desc_size(pobj, &desclen);
	txdesc = pcmdframe->buf_addr;

	_rtw_memcpy((txdesc + desclen), pBuf, size); /* shift desclen */

	/* update attribute */
	pattrib = &pcmdframe->attrib;
	update_mgntframe_attrib(padapter, pattrib);
	pattrib->qsel = QSLT_BEACON;
	pattrib->pktlen = size;
	pattrib->last_txcmdsz = size;

	/* Clear beacon valid check bit. */
	rtw_hal_set_hwreg(padapter, HW_VAR_BCN_VALID, NULL);

	dump_mgntframe(padapter, pcmdframe);

	DLBcnCount = 0;
	poll = 0;
	do {
		DLBcnCount++;
		do {
			rtw_yield_os();
			/* does rsvd page download OK. */
			rtw_hal_get_hwreg(padapter,
				HW_VAR_BCN_VALID,(u8 *)(&bcn_valid));
			poll++;
		} while (!bcn_valid && (poll % 10) != 0 && !RTW_CANNOT_RUN(padapter));
	} while (!bcn_valid && DLBcnCount <= 100 && !RTW_CANNOT_RUN(padapter));

	txbd = (u8 *)(&ring->buf_desc[0]);

	mapping = GET_TX_BD_PHYSICAL_ADDR0_LOW(txbd);
	#ifdef CONFIG_64BIT_DMA
		mapping |= (dma_addr_t)GET_TX_BD_PHYSICAL_ADDR0_HIGH(txbd) << 32;
	#endif

	/*To patch*/

	#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0))
	pci_unmap_single(pdev, mapping,	pxmitbuf->len, PCI_DMA_TODEVICE);
	#else
	dma_unmap_single(&pdev->dev, mapping,	pxmitbuf->len, DMA_TO_DEVICE);
	#endif

	return _TRUE;
}

static u8 pci_write_data_h2c_normal(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	struct halmac_adapter *halmac = dvobj_to_halmac((struct dvobj_priv *)d);
	struct xmit_priv        *pxmitpriv = &padapter->xmitpriv;
	struct xmit_frame       *pcmdframe = NULL;
	struct pkt_attrib       *pattrib = NULL;
	struct halmac_api *api;
	u32 desclen;
	u8 *buf;

	if (size + TXDESC_OFFSET > MAX_XMIT_EXTBUF_SZ) {
		RTW_INFO("%s: total buffer size(%d) > MAX_XMIT_EXTBUF_SZ(%d)\n"
			, __func__, size + TXDESC_OFFSET, MAX_XMIT_EXTBUF_SZ);
		return _FALSE;
	}

	pcmdframe = alloc_mgtxmitframe(pxmitpriv);

	if (pcmdframe == NULL) {
		RTW_INFO("%s: alloc ReservedPagePacket fail!\n", __func__);
		return _FALSE;
	}

	api = HALMAC_GET_API(halmac);

	rtw_halmac_get_tx_desc_size(pobj, &desclen);
	buf = pcmdframe->buf_addr;
	_rtw_memcpy(buf + desclen, pBuf, size); /* shift desclen */

	SET_TX_DESC_TXPKTSIZE_8821C(buf, size);
	SET_TX_DESC_OFFSET_8821C(buf, 0);
	SET_TX_DESC_QSEL_8821C(buf, HALMAC_TXDESC_QSEL_H2C_CMD);
	SET_TX_DESC_TXDESC_CHECKSUM_8821C(buf, 0);
	api->halmac_fill_txdesc_checksum(halmac, buf);

	/* update attribute */
	pattrib = &pcmdframe->attrib;
	update_mgntframe_attrib(padapter, pattrib);
	pattrib->qsel = QSLT_CMD;
	pattrib->pktlen = size;
	pattrib->last_txcmdsz = size;

	/* fill tx desc in dump_mgntframe */
	dump_mgntframe(padapter, pcmdframe);

	return _TRUE;
}

static u8 pci_write_data_rsvd_page(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u8 ret;

	if (pHalData->not_xmitframe_fw_dl)
		ret = pci_write_data_not_xmitframe(d, pBuf, size, HALMAC_TXDESC_QSEL_BEACON);
	else
		ret = pci_write_data_rsvd_page_xmitframe(d, pBuf, size);

	if (ret == _TRUE)
		return 1;
	return 0;
}

static u8 pci_write_data_h2c(void *d, u8 *pBuf, u32 size)
{
	struct dvobj_priv *pobj = (struct dvobj_priv *)d;
	PADAPTER padapter = dvobj_get_primary_adapter(pobj);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u8 ret;

	if (pHalData->not_xmitframe_fw_dl)
		ret = pci_write_data_not_xmitframe(d, pBuf, size, HALMAC_TXDESC_QSEL_H2C_CMD);
	else
		ret = pci_write_data_h2c_normal(d, pBuf, size);

	if (ret == _TRUE)
		return 1;
	return 0;
}

int rtl8821ce_halmac_init_adapter(PADAPTER padapter)
{
	struct dvobj_priv *d;
	struct halmac_platform_api *api;
	int err;

#ifdef CONFIG_64BIT_DMA	
	if (adapter_to_dvobj(padapter)->bdma64)
		PlatformEnableDMA64(padapter);
#endif

	d = adapter_to_dvobj(padapter);
	api = &rtw_halmac_platform_api;
	api->SEND_RSVD_PAGE = pci_write_data_rsvd_page;
	api->SEND_H2C_PKT = pci_write_data_h2c;

	err = rtw_halmac_init_adapter(d, api);

	return err;
}
