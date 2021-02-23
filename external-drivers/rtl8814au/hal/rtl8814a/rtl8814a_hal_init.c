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
#define _RTL8814A_HAL_INIT_C_

/* #include <drv_types.h> */
#include <rtl8814a_hal.h>
#include "hal8814a_fw.h"
/* -------------------------------------------------------------------------
 *
 * LLT R/W/Init function
 *
 * ------------------------------------------------------------------------- */
void
Hal_InitEfuseVars_8814A(
		PADAPTER	Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PEFUSE_HAL		pEfuseHal = &(pHalData->EfuseHal);
	u16				*ptr;

#define INIT_EFUSE(var, value)	ptr = (u16 *)&var; *ptr = value

	RTW_INFO("====> %s\n", __func__);
	/* 2 Common */
	INIT_EFUSE(pEfuseHal->WordUnit	     , EFUSE_MAX_WORD_UNIT);
	RTW_INFO("====>pEfuseHal->WordUnit %d\n", pEfuseHal->WordUnit);
	INIT_EFUSE(pEfuseHal->BankSize	     , 512);
	INIT_EFUSE(pEfuseHal->OOBProtectBytes, EFUSE_OOB_PROTECT_BYTES);
	RTW_INFO("====>pEfuseHal->OOBProtectBytes %d\n", pEfuseHal->OOBProtectBytes);
	INIT_EFUSE(pEfuseHal->ProtectBytes   , EFUSE_PROTECT_BYTES_BANK_8814A);
	RTW_INFO("====>pEfuseHal->ProtectBytes %d\n", pEfuseHal->ProtectBytes);
	INIT_EFUSE(pEfuseHal->BankAvailBytes , (pEfuseHal->BankSize - pEfuseHal->OOBProtectBytes));
	INIT_EFUSE(pEfuseHal->TotalBankNum   , EFUSE_MAX_BANK_8814A);
	INIT_EFUSE(pEfuseHal->HeaderRetry    , 0);
	INIT_EFUSE(pEfuseHal->DataRetry      , 0);

	/* 2 Wi-Fi */
	INIT_EFUSE(pEfuseHal->MaxSecNum_WiFi	  , EFUSE_MAX_SECTION_8814A);
	RTW_INFO("====>pEfuseHal->MaxSecNum_WiFi %d\n", pEfuseHal->MaxSecNum_WiFi);
	INIT_EFUSE(pEfuseHal->PhysicalLen_WiFi	  , EFUSE_REAL_CONTENT_LEN_8814A);
	RTW_INFO("====>pEfuseHal->PhysicalLen_WiFi %d\n", pEfuseHal->PhysicalLen_WiFi);
	INIT_EFUSE(pEfuseHal->LogicalLen_WiFi	  , EFUSE_MAP_LEN_8814A);
	RTW_INFO("====>pEfuseHal->LogicalLen_WiFi %d\n", pEfuseHal->LogicalLen_WiFi);
	INIT_EFUSE(pEfuseHal->BankNum_WiFi		 , pEfuseHal->PhysicalLen_WiFi / pEfuseHal->BankSize);
	INIT_EFUSE(pEfuseHal->TotalAvailBytes_WiFi, (pEfuseHal->PhysicalLen_WiFi - (pEfuseHal->TotalBankNum * pEfuseHal->OOBProtectBytes)));

	/* 2 BT */
	INIT_EFUSE(pEfuseHal->MaxSecNum_BT   , 0);
	INIT_EFUSE(pEfuseHal->PhysicalLen_BT , 0);
	INIT_EFUSE(pEfuseHal->LogicalLen_BT  , 0);
	INIT_EFUSE(pEfuseHal->BankNum_BT	 , 0);
	INIT_EFUSE(pEfuseHal->TotalAvailBytes_BT, 0);

	RTW_INFO("%s <====\n", __func__);
}


s32 InitLLTTable8814A(
		PADAPTER	Adapter
)
{
	/* Auto-init LLT table ( Set REG:0x208[BIT0] ) */
	/* Write 1 to enable HW init LLT, driver need polling to 0 meaning init success */
	u8		tmp1byte = 0, testcnt = 0;
	s32	Status = _SUCCESS;

	tmp1byte = rtw_read8(Adapter, REG_AUTO_LLT_8814A);
	rtw_write8(Adapter, REG_AUTO_LLT_8814A, tmp1byte | BIT0);
	while (tmp1byte & BIT0) {
		tmp1byte = rtw_read8(Adapter, REG_AUTO_LLT_8814A);
		rtw_mdelay_os(100);
		testcnt++;
		if (testcnt > 100) {
			Status = _FAIL;
			break;
		}
	}
	return Status;
}

#ifdef CONFIG_WOWLAN
void hal_DetectWoWMode(PADAPTER pAdapter)
{
	adapter_to_pwrctl(pAdapter)->bSupportRemoteWakeup = _TRUE;
}
#endif

void
_FWDownloadEnable_8814A(
		PADAPTER		Adapter,
		BOOLEAN			enable
)
{
	u8	tmp;
	u16	u2Tmp = 0;

	if (enable) {
		/* MCU firmware download enable. */
		u2Tmp = rtw_read16(Adapter, REG_8051FW_CTRL_8814A);
		u2Tmp &= 0x3000;
		u2Tmp &= (~BIT12);
		u2Tmp |= BIT13;
		u2Tmp |= BIT0;
		rtw_write16(Adapter, REG_8051FW_CTRL_8814A, u2Tmp);

		/* Clear Rom DL enable */
		/* tmp = rtw_read8(Adapter, REG_8051FW_CTRL_8814A+2); */ /* modify by gw 20130826(advice by hw) */
		/* rtw_write8(Adapter, REG_8051FW_CTRL_8814A+2, tmp&0xf7); */ /* clear bit3 */
	} else {
		/* MCU firmware download enable. */
		tmp = rtw_read8(Adapter, REG_8051FW_CTRL_8814A);
		rtw_write8(Adapter, REG_8051FW_CTRL_8814A, tmp & 0xfe);
	}
}

#define MAX_REG_BOLCK_SIZE	196

void
_BlockWrite_8814A(
			PADAPTER	Adapter,
			void			*buffer,
			u32			buffSize
)
{
	u32			blockSize_p1 = 4;	/* (Default) Phase #1 : PCI muse use 4-byte write to download FW */
	u32			blockSize_p2 = 8;	/* Phase #2 : Use 8-byte, if Phase#1 use big size to write FW. */
	u32			blockSize_p3 = 1;	/* Phase #3 : Use 1-byte, the remnant of FW image. */
	u32			blockCount_p1 = 0, blockCount_p2 = 0, blockCount_p3 = 0;
	u32			remainSize_p1 = 0, remainSize_p2 = 0;
	u8			*bufferPtr	= (u8 *)buffer;
	u32			i = 0, offset = 0;

#ifdef CONFIG_USB_HCI
	blockSize_p1	= MAX_REG_BOLCK_SIZE; /* Use 196-byte write to download FW */
	/* Small block size will increase USB init speed. But prevent FW download fail */
	/* use 4-Byte instead of 196-Byte to write FW. */
#endif

	/* 3 Phase #1 */
	blockCount_p1 = buffSize / blockSize_p1;
	remainSize_p1 = buffSize % blockSize_p1;



	for (i = 0 ; i < blockCount_p1 ; i++) {
#ifdef CONFIG_USB_HCI
		rtw_writeN(Adapter, (FW_START_ADDRESS + i * blockSize_p1), blockSize_p1, (bufferPtr + i * blockSize_p1));
#else
		rtw_write32(Adapter, (FW_START_ADDRESS + i * blockSize_p1), *((u32 *)(bufferPtr + i * blockSize_p1)));
#endif
	}

	/* 3 Phase #2 */
	if (remainSize_p1) {
		offset = blockCount_p1 * blockSize_p1;

		blockCount_p2 = remainSize_p1 / blockSize_p2;
		remainSize_p2 = remainSize_p1 % blockSize_p2;



#ifdef CONFIG_USB_HCI
		for (i = 0 ; i < blockCount_p2 ; i++)
			rtw_writeN(Adapter, (FW_START_ADDRESS + offset + i * blockSize_p2), blockSize_p2, (bufferPtr + offset + i * blockSize_p2));
#endif
	}

	/* 3 Phase #3 */
	if (remainSize_p2) {
		offset = (blockCount_p1 * blockSize_p1) + (blockCount_p2 * blockSize_p2);

		blockCount_p3 = remainSize_p2 / blockSize_p3;


		for (i = 0 ; i < blockCount_p3 ; i++)
			rtw_write8(Adapter, (FW_START_ADDRESS + offset + i), *(bufferPtr + offset + i));
	}
}

void
_PageWrite_8814A(
			PADAPTER	Adapter,
			u32			page,
			void			*buffer,
			u32			size
)
{
	u8 value8;
	u8 u8Page = (u8)(page & 0x07) ;

	value8 = (rtw_read8(Adapter, REG_8051FW_CTRL_8814A + 2) & 0xF8) | u8Page ;
	rtw_write8(Adapter, REG_8051FW_CTRL_8814A + 2, value8);

	_BlockWrite_8814A(Adapter, buffer, size);
}

void
_FillDummy_8814A(
	u8		*pFwBuf,
	u32		*pFwLen
)
{
	u32	FwLen = *pFwLen;
	u8	remain = (u8)(FwLen % 4);
	remain = (remain == 0) ? 0 : (4 - remain);

	while (remain > 0) {
		pFwBuf[FwLen] = 0;
		FwLen++;
		remain--;
	}

	*pFwLen = FwLen;
}

void
_WriteFW_8814A(
			PADAPTER	Adapter,
			void			*buffer,
			u32			size
)
{
	u32			pageNums, remainSize ;
	u32			page, offset;
	u8			*bufferPtr = (u8 *)buffer;

#ifdef CONFIG_PCI_HCI
	/* 20100120 Joseph: Add for 88CE normal chip. */
	/* Fill in zero to make firmware image to dword alignment. */
	_FillDummy_8814A(bufferPtr, &size);
#endif /* CONFIG_PCI_HCI */

	pageNums = size / MAX_PAGE_SIZE ;

	/* RT_ASSERT((pageNums <= 8), ("Page numbers should not greater then 8\n"));	 */

	remainSize = size % MAX_PAGE_SIZE;

	for (page = 0; page < pageNums;  page++) {
		offset = page * MAX_PAGE_SIZE;
		_PageWrite_8814A(Adapter, page, (bufferPtr + offset), MAX_PAGE_SIZE);
		rtw_udelay_os(2);
	}
	if (remainSize) {
		offset = pageNums * MAX_PAGE_SIZE;
		page = pageNums;
		_PageWrite_8814A(Adapter, page, (bufferPtr + offset), remainSize);
	}
}

void
_3081Disable8814A(
		PADAPTER		Adapter
)
{
	u8	u1bTmp;
	u1bTmp = rtw_read8(Adapter, REG_SYS_FUNC_EN_8814A + 1);
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A + 1, u1bTmp & (~BIT2));


}

void
_3081Enable8814A(
		PADAPTER		Adapter
)
{
	u8	u1bTmp;
	u1bTmp = rtw_read8(Adapter, REG_SYS_FUNC_EN_8814A + 1);
	rtw_write8(Adapter, REG_SYS_FUNC_EN_8814A + 1, u1bTmp | BIT2);
}


/* add by ylb 20130814 for 3081 download FW */
static
BOOLEAN
IDDMADownLoadFW_3081(
			PADAPTER	Adapter,
			u32 source,
			u32 dest,
			u32  length,
			BOOLEAN fs,
			BOOLEAN ls
)
{
	u32 ch0ctrl = (DDMA_CHKSUM_EN | DDMA_CH_OWN);
	u32 cnt;
	u8 tmp;
	/* check if ddma ch0 is idle */
	cnt = 20;
	while (rtw_read32(Adapter, REG_DDMA_CH0CTRL) & DDMA_CH_OWN) {
		rtw_udelay_os(1000);
		cnt--;
		if (cnt == 0) {
			RTW_INFO("IDDMADownLoadFW_3081, line%d: CNT fail\n", __LINE__);
			return _FALSE;
		}
	}
	ch0ctrl |= length & DDMA_LEN_MASK;

	/* check if chksum continuous */
	if (fs == _FALSE)
		ch0ctrl |= DDMA_CH_CHKSUM_CNT;
	rtw_write32(Adapter, REG_DDMA_CH0SA, source);
	rtw_write32(Adapter, REG_DDMA_CH0DA, dest);
	rtw_write32(Adapter, REG_DDMA_CH0CTRL, ch0ctrl);

	cnt = 20;
	while (rtw_read32(Adapter, REG_DDMA_CH0CTRL) & DDMA_CH_OWN) {
		rtw_udelay_os(1000);
		cnt--;
		if (cnt == 0) {
			RTW_INFO("IDDMADownLoadFW_3081, line%d: CNT fail\n", __LINE__);
			return _FALSE;
		}
	}

	/* check checksum */
	if (ls == _TRUE) {
		tmp = rtw_read8(Adapter, REG_8051FW_CTRL_8814A);
		if (0 == (rtw_read32(Adapter, REG_DDMA_CH0CTRL) & DDMA_CHKSUM_FAIL)) {
			/* chksum ok */
			RTW_INFO("Check sum OK\n");
			/* imem */
			if (dest < OCPBASE_DMEM_3081) {
				tmp |= IMEM_DL_RDY;
				rtw_write8(Adapter, REG_8051FW_CTRL_8814A, tmp | IMEM_CHKSUM_OK);
				RTW_INFO("imem check sum tmp %d\n", tmp);
			}
			/* dmem */
			else {
				tmp |= DMEM_DL_RDY;
				rtw_write8(Adapter, REG_8051FW_CTRL_8814A, tmp | DMEM_CHKSUM_OK);
				RTW_INFO("dmem check sum tmp %d\n", tmp);
			}
		} else {
			/* chksum fail */
			RTW_INFO("Check sum fail\n");
			ch0ctrl = rtw_read32(Adapter, REG_DDMA_CH0CTRL);
			rtw_write32(Adapter, REG_DDMA_CH0CTRL, ch0ctrl | DDMA_RST_CHKSUM_STS);

			/* imem */
			if (dest < OCPBASE_DMEM_3081) {
				tmp &= (~IMEM_DL_RDY);
				rtw_write8(Adapter, REG_8051FW_CTRL_8814A, tmp & ~IMEM_CHKSUM_OK);
			}
			/* dmem */
			else {
				tmp &= (~DMEM_DL_RDY);
				rtw_write8(Adapter, REG_8051FW_CTRL_8814A, tmp & ~DMEM_CHKSUM_OK);
			}
			return _FALSE;
		}
	}
	return _TRUE;
}


/* add by ylb 20130814 for 3081 download FW */
static
BOOLEAN
WaitDownLoadRSVDPageOK_3081(
		PADAPTER			Adapter
)
{
	u8	BcnValidReg = 0, TxBcReg = 0;
	u8	count = 0, DLBcnCount = 0;
	BOOLEAN bRetValue = _FALSE;

#if defined(CONFIG_PCI_HCI)
	/* Polling Beacon Queue to send Beacon */
	TxBcReg = rtw_read8(Adapter, REG_MGQ_TXBD_NUM_8814A + 3);
	count = 0;
	while ((count < 20) && (TxBcReg & BIT4)) {
		count++;
		rtw_udelay_os(10);
		TxBcReg = rtw_read8(Adapter, REG_MGQ_TXBD_NUM_8814A + 3);
	}

	rtw_write8(Adapter, REG_MGQ_TXBD_NUM_8814A + 3, TxBcReg | BIT4);
#endif /* #if defined(CONFIG_PCI_HCI) */
	/* check rsvd page download OK. */
	BcnValidReg = rtw_read8(Adapter, REG_FIFOPAGE_CTRL_2_8814A + 1);
	count = 0;
	while (!(BcnValidReg & BIT7) && count < 20) {
		count++;
		rtw_udelay_os(50);
		BcnValidReg = rtw_read8(Adapter, REG_FIFOPAGE_CTRL_2_8814A + 1);
	}

	/* Set 1 to Clear BIT7 by SW */
	if (BcnValidReg & BIT7) {
		rtw_write8(Adapter, REG_FIFOPAGE_CTRL_2_8814A + 1, (BcnValidReg | BIT7));
		bRetValue =  _TRUE;
	} else {
		RTW_INFO("WaitDownLoadRSVDPageOK_3081(): Download RSVD page failed!\n");
		bRetValue = _FALSE;
	}

	return bRetValue;
}


void
SetDownLoadFwRsvdPagePkt_8814A(
		PADAPTER	Adapter,
		void			*buffer,
		u32			len
)
{
	PHAL_DATA_TYPE pHalData;
	struct xmit_frame	*pcmdframe;
	struct xmit_priv	*pxmitpriv;
	struct pkt_attrib	*pattrib;
	/* The desc lengh in Tx packet buffer of 8814A is 40 bytes.	 */
	u16	BufIndex = 0, TxDescOffset = TXDESC_OFFSET;
	u32	TotalPacketLen = len;
	BOOLEAN		bDLOK = FALSE;
	u8	*ReservedPagePacket;

	pHalData = GET_HAL_DATA(Adapter);
	pxmitpriv = &Adapter->xmitpriv;

#ifdef CONFIG_BCN_ICF
	pcmdframe = rtw_alloc_cmdxmitframe(pxmitpriv);
#else
	pcmdframe = alloc_mgtxmitframe(pxmitpriv);
#endif
	if (pcmdframe == NULL)
		return;

	ReservedPagePacket = pcmdframe->buf_addr;

	BufIndex = TxDescOffset;

	TotalPacketLen = len + BufIndex;

	_rtw_memcpy(&ReservedPagePacket[BufIndex], buffer, len);
	/* RTW_INFO("SetFwRsvdPagePkt_8814A(): HW_VAR_SET_TX_CMD: BCN, %p, %d\n", &ReservedPagePacket[BufIndex], len); */

	/* RTW_INFO("SetFwRsvdPagePkt(): TotalPacketLen=%d\n", TotalPacketLen); */

	/* update attribute */
	pattrib = &pcmdframe->attrib;
	update_mgntframe_attrib(Adapter, pattrib);
	pattrib->qsel = QSLT_BEACON;
	pattrib->pktlen = pattrib->last_txcmdsz = TotalPacketLen - TxDescOffset;

	dump_mgntframe(Adapter, pcmdframe);

	/* ReturnGenTempBuffer(pAdapter, pGenBufReservedPagePacket); */
}


void
HalROMDownloadFWRSVDPage8814A(
		PADAPTER	Adapter,
		void			*buffer,
		u32			Len
)
{
	u8	u1bTmp = 0, tmpReg422 = 0;
	u8	BcnValidReg = 0, TxBcReg = 0;
	BOOLEAN	bSendBeacon = _FALSE, bDownLoadRSVDPageOK = _FALSE;
	u8  *pbuffer = buffer;

	BOOLEAN fs = _TRUE, ls = _FALSE;
	u8	FWHeaderSize = 64, PageSize = 128 ;
	u16 RsvdPageNum = 0;
	u32 dmem_pkt_size = 0, iram_pkt_size = 0 , MaxRsvdPageBufSize = 0;
	u32 last_block_size = 0, filesize_ram_block = 0, pkt_offset = 0;
	u32 txpktbuf_bndy = 0;
	u32	BeaconHeaderInTxPacketBuf = 0, MEMOffsetInTxPacketBuf  = 0;
	u8 bcn_ctrl = rtw_read8(Adapter, REG_BCN_CTRL);

	/* Set REG_CR bit 8. DMA beacon by SW. */
	u1bTmp = rtw_read8(Adapter, REG_CR_8814A + 1);
	rtw_write8(Adapter,  REG_CR_8814A + 1, (u1bTmp | BIT0));
	/*RTW_INFO("%s-%d: enable SW BCN, REG_CR=0x%x\n", __func__, __LINE__, rtw_read32(Adapter, REG_CR));*/

	/* Disable Hw protection for a time which revserd for Hw sending beacon. */
	/* Fix download reserved page packet fail that access collision with the protection time. */
	/* 2010.05.11. Added by tynli. 0x550[4]=1, 0x550[3]=0 */
	rtw_write8(Adapter, REG_BCN_CTRL, (bcn_ctrl & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);

	/* Set FWHW_TXQ_CTRL 0x422[6]=0 to tell Hw the packet is not a real beacon frame. 0x422[6]=0 */
	tmpReg422 = rtw_read8(Adapter, REG_FWHW_TXQ_CTRL_8814A + 2);
	rtw_write8(Adapter, REG_FWHW_TXQ_CTRL_8814A + 2,  tmpReg422 & (~BIT6));

	if (tmpReg422 & BIT6) {
		RTW_INFO("HalROMDownloadFWRSVDPage8814A(): There is an Adapter is sending beacon.\n");
		bSendBeacon = _TRUE;
	}

	/* Set The head page of packet of Bcnq */
	rtw_hal_get_def_var(Adapter, HAL_DEF_TX_PAGE_BOUNDARY, (u16 *)&txpktbuf_bndy);
	rtw_write16(Adapter, REG_FIFOPAGE_CTRL_2_8814A, txpktbuf_bndy);

	/* RTW_INFO("HalROMDownloadFWRSVDPage8814A: txpktbuf_bndy=%d\n", txpktbuf_bndy); */

	/* Clear beacon valid check bit. 0x205[7]=1 */
	BcnValidReg = rtw_read8(Adapter, REG_FIFOPAGE_CTRL_2_8814A + 1);
	rtw_write8(Adapter, REG_FIFOPAGE_CTRL_2_8814A + 1, (BcnValidReg | BIT7));

	/* Return Beacon TCB. */
	/* ReturnBeaconQueueTcb_8814A(Adapter); */

	dmem_pkt_size = (u32)GET_FIRMWARE_HDR_TOTAL_DMEM_SZ_3081(pbuffer);
	iram_pkt_size = (u32)GET_FIRMWARE_HDR_IRAM_SZ_3081(pbuffer);
	dmem_pkt_size += (u32)FW_CHKSUM_DUMMY_SZ;
	iram_pkt_size += (u32)FW_CHKSUM_DUMMY_SZ;

	if (dmem_pkt_size + iram_pkt_size + FWHeaderSize != Len) {
		RTW_INFO("ERROR: Fw Hdr size do not match the real fw size!!\n");
		RTW_INFO("dmem_pkt_size = %d, iram_pkt_size = %d,FWHeaderSize = %d, Len = %d!!\n", dmem_pkt_size, iram_pkt_size, FWHeaderSize, Len);
		return;
	}
	RTW_INFO("dmem_pkt_size = %d, iram_pkt_size = %d,FWHeaderSize = %d, Len = %d!!\n", dmem_pkt_size, iram_pkt_size, FWHeaderSize, Len);

	/* download rsvd page.	 */
	/* RsvdPageNum = GetTxBufferRsvdPageNum8814A(Adapter, _FALSE); */
#ifdef CONFIG_BCN_IC
	/* TODO: check tx buffer and DMA size */
	MaxRsvdPageBufSize = MAX_CMDBUF_SZ - TXDESC_OFFSET;
#else
	MaxRsvdPageBufSize = MAX_XMIT_EXTBUF_SZ - TXDESC_OFFSET;/* RsvdPageNum*PageSize - 40 -16 modified for usb; */ /* TX_INFO_SIZE_8814AE; */
#endif
	RTW_INFO("MaxRsvdPageBufSize %d,  Total len %d\n", MaxRsvdPageBufSize, Len);

	BeaconHeaderInTxPacketBuf = txpktbuf_bndy * PageSize;
	MEMOffsetInTxPacketBuf = OCPBASE_TXBUF_3081 + BeaconHeaderInTxPacketBuf + 40;/* TX_INFO_SIZE_8814AE;	 */
	/* download DMEM */
	while (dmem_pkt_size > 0) {
		if (dmem_pkt_size > MaxRsvdPageBufSize) {
			filesize_ram_block = MaxRsvdPageBufSize;
			ls = _FALSE;

			last_block_size = dmem_pkt_size - MaxRsvdPageBufSize;
			if (last_block_size < MaxRsvdPageBufSize) {
				if (((last_block_size + 40) & 0x3F)  == 0)	/* Multiples of 64 */
					filesize_ram_block -= 4;
			}
		} else {
			filesize_ram_block = dmem_pkt_size;
			ls = _TRUE;
		}
		fs = (pkt_offset == 0 ? _TRUE : _FALSE);
		/* Return Beacon TCB. */
		/* ReturnBeaconQueueTcb_8814A(Adapter); */
		/* RTW_INFO("%d packet offset %d dmem_pkt_size %d\n", __LINE__,pkt_offset, dmem_pkt_size); */
		SetDownLoadFwRsvdPagePkt_8814A(Adapter, pbuffer + FWHeaderSize + pkt_offset, filesize_ram_block);
		bDownLoadRSVDPageOK = WaitDownLoadRSVDPageOK_3081(Adapter);
		if (!bDownLoadRSVDPageOK) {
			RTW_INFO("ERROR: DMEM  bDownLoadRSVDPageOK is false!!\n");
			return;
		}

		IDDMADownLoadFW_3081(Adapter, MEMOffsetInTxPacketBuf, OCPBASE_DMEM_3081 + pkt_offset, filesize_ram_block, fs, ls);
		dmem_pkt_size -= filesize_ram_block;
		pkt_offset += filesize_ram_block;
	}

	/* download IRAM */
	pkt_offset = 0;
	while (iram_pkt_size > 0) {
		if (iram_pkt_size > MaxRsvdPageBufSize) {
			filesize_ram_block = MaxRsvdPageBufSize;
			ls = _FALSE;

			last_block_size = iram_pkt_size - MaxRsvdPageBufSize;
			if (last_block_size < MaxRsvdPageBufSize) {
				if (((last_block_size + 40) & 0x3F)  == 0)	/* Multiples of 64 */
					filesize_ram_block -= 4;
			}
		} else {
			filesize_ram_block = iram_pkt_size;
			ls = _TRUE;
		}

		fs = (pkt_offset == 0 ? _TRUE : _FALSE);
		/* Return Beacon TCB. */
		/* ReturnBeaconQueueTcb_8814A(Adapter); */
		/* RTW_INFO("%d packet offset %d iram_pkt_size %d\n", __LINE__,pkt_offset, iram_pkt_size); */
		SetDownLoadFwRsvdPagePkt_8814A(Adapter, pbuffer + Len - iram_pkt_size, filesize_ram_block);

		bDownLoadRSVDPageOK = WaitDownLoadRSVDPageOK_3081(Adapter);
		if (!bDownLoadRSVDPageOK) {
			RTW_INFO("ERROR: IRAM  bDownLoadRSVDPageOK is false!!\n");
			return;
		}

		IDDMADownLoadFW_3081(Adapter, MEMOffsetInTxPacketBuf, OCPBASE_IMEM_3081 + pkt_offset, filesize_ram_block, fs, ls);

		iram_pkt_size -= filesize_ram_block;
		pkt_offset += filesize_ram_block;
	}

	/* restore bcn_ctrl */
	rtw_write8(Adapter, REG_BCN_CTRL, bcn_ctrl);

	/* To make sure that if there exists an adapter which would like to send beacon. */
	/* If exists, the origianl value of 0x422[6] will be 1, we should check this to */
	/* prevent from setting 0x422[6] to 0 after download reserved page, or it will cause */
	/* the beacon cannot be sent by HW. */
	/* 2010.06.23. Added by tynli. */
	if (bSendBeacon)
		rtw_write8(Adapter, REG_FWHW_TXQ_CTRL_8814A + 2, tmpReg422);

	/* Do not enable HW DMA BCN or it will cause Pcie interface hang by timing issue. 2011.11.24. by tynli. */
	/* if(!Adapter->bEnterPnpSleep) */
	{
		/* Clear CR[8] or beacon packet will not be send to TxBuf anymore. */
		u1bTmp = rtw_read8(Adapter, REG_CR_8814A + 1);
		rtw_write8(Adapter, REG_CR_8814A + 1, (u1bTmp & (~BIT0)));
	}

	u1bTmp = rtw_read8(Adapter, REG_8051FW_CTRL_8814A); /* add by gw for flags to show the fw download ok 20130826 */
	if (u1bTmp & DMEM_CHKSUM_OK) {
		if (u1bTmp & IMEM_CHKSUM_OK) {
			u8 tem;
			tem = rtw_read8(Adapter, REG_8051FW_CTRL_8814A + 1);
			rtw_write8(Adapter, REG_8051FW_CTRL_8814A + 1, (tem | BIT6));
		}
	}
}

s32
_FWFreeToGo8814A(
			PADAPTER		Adapter
)
{
	u32	counter = 0;
	u32	value32;

	/* polling CheckSum report */
	do {
		rtw_mdelay_os(50);
		value32 = rtw_read32(Adapter, REG_8051FW_CTRL_8814A);

	} while ((counter++ < 100) && (!(value32 &  CPU_DL_READY)));

	if (counter >= 100) {
		return _FAIL;
	}


	return _SUCCESS;
}


#ifdef CONFIG_FILE_FWIMG
	u8	FwBuffer8814[FW_SIZE];
#endif /* CONFIG_FILE_FWIMG */

s32
FirmwareDownload8814A(
		PADAPTER			Adapter,
		BOOLEAN			bUsedWoWLANFw
)
{
	s32	rtStatus = _SUCCESS;
	u8	write_fw = 0;
	systime fwdl_start_time;
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);

	u8				*pFwImageFileName;
	u8				*pucMappedFile = NULL;
	PRT_FIRMWARE_8814	pFirmware = NULL;
	u8				*pFwHdr = NULL;
	u8				*pFirmwareBuf;
	u32				FirmwareLen;
#ifdef CONFIG_FILE_FWIMG
	u8 *fwfilepath;
#endif
	u32 u32tmp;


	pFirmware = (PRT_FIRMWARE_8814)rtw_zmalloc(sizeof(RT_FIRMWARE_8814));
	if (!pFirmware) {
		rtStatus = _FAIL;
		goto exit;
	}

#ifdef CONFIG_FILE_FWIMG
#ifdef CONFIG_WOWLAN
	if (bUsedWoWLANFw)
		fwfilepath = rtw_fw_wow_file_path;
	else
#endif /* CONFIG_WOWLAN */
	{
		fwfilepath = rtw_fw_file_path;
	}

	if (rtw_is_file_readable(fwfilepath) == _TRUE) {
		RTW_INFO("%s acquire FW from file:%s\n", __FUNCTION__, fwfilepath);
		pFirmware->eFWSource = FW_SOURCE_IMG_FILE;
	} else
#endif /* CONFIG_FILE_FWIMG */
	{
		RTW_INFO("%s fw source from Header\n", __FUNCTION__);
		pFirmware->eFWSource = FW_SOURCE_HEADER_FILE;
	}

	switch (pFirmware->eFWSource) {
	case FW_SOURCE_IMG_FILE:
#ifdef CONFIG_FILE_FWIMG
		rtStatus = rtw_retrieve_from_file(fwfilepath, FwBuffer8814, FW_SIZE);
		pFirmware->ulFwLength = rtStatus >= 0 ? rtStatus : 0;
		pFirmware->szFwBuffer = FwBuffer8814;
		RTW_INFO("%s fw size: %d\n", __FUNCTION__, pFirmware->ulFwLength);
#endif /* CONFIG_FILE_FWIMG */
		break;
	case FW_SOURCE_HEADER_FILE:
		if (bUsedWoWLANFw) {
			#ifdef CONFIG_WOWLAN
			pFirmware->szFwBuffer = array_mp_8814a_fw_wowlan;
			pFirmware->ulFwLength = array_length_mp_8814a_fw_wowlan;
			RTW_INFO("%s fw:%s, size: %d\n", __FUNCTION__, "WoWLAN", pFirmware->ulFwLength);
			#endif /* CONFIG_WOWLAN */
		} else {
			pFirmware->szFwBuffer = array_mp_8814a_fw_nic;
			pFirmware->ulFwLength = array_length_mp_8814a_fw_nic;
			RTW_INFO("%s fw:%s, size: %d\n", __FUNCTION__, "NIC", pFirmware->ulFwLength);
		}
		break;
	}

	if (pFirmware->ulFwLength == 0) {
		RTW_ERR("Firmware size:%u\n", pFirmware->ulFwLength);
		goto exit;
	}

	if ((pFirmware->ulFwLength - 64) > FW_SIZE) {
		rtStatus = _FAIL;
		RTW_ERR("Firmware size:%u exceed %u\n", pFirmware->ulFwLength, FW_SIZE);
		goto exit;
	}

	pFirmwareBuf = pFirmware->szFwBuffer;
	FirmwareLen = pFirmware->ulFwLength;
	pFwHdr = (u8 *)pFirmware->szFwBuffer;

	pHalData->firmware_version = (u16)GET_FIRMWARE_HDR_VERSION_3081(pFwHdr);
	pHalData->firmware_sub_version = (u16)GET_FIRMWARE_HDR_SUB_VER_3081(pFwHdr);
	pHalData->FirmwareSignature = (u16)GET_FIRMWARE_HDR_SIGNATURE_3081(pFwHdr);

	RTW_INFO("%s: fw_ver=%d fw_subver=%d sig=0x%x\n",
		__FUNCTION__, pHalData->firmware_version, pHalData->firmware_sub_version, pHalData->FirmwareSignature);
	fwdl_start_time = rtw_get_current_time();

	_FWDownloadEnable_8814A(Adapter, _TRUE);

	_3081Disable8814A(Adapter);/* add by gw 2013026 for disable mcu core */

	/* DDMA reset, suggest by MAC yodar */
	u32tmp = rtw_read32(Adapter, REG_CPU_DMEM_CON_8814A);
	rtw_write32(Adapter, REG_CPU_DMEM_CON_8814A, u32tmp&(~BIT16));
	rtw_write32(Adapter, REG_CPU_DMEM_CON_8814A, u32tmp | BIT16);

	HalROMDownloadFWRSVDPage8814A(Adapter, pFirmwareBuf, FirmwareLen);

	_3081Enable8814A(Adapter);/* add by gw 2013026 for Enable mcu core */

	_FWDownloadEnable_8814A(Adapter, _FALSE);

	rtStatus = _FWFreeToGo8814A(Adapter);
	if (_SUCCESS != rtStatus)
		goto fwdl_stat;

fwdl_stat:
	RTW_INFO("FWDL %s. write_fw:%u, %dms\n"
		 , (rtStatus == _SUCCESS) ? "success" : "fail"
		 , write_fw
		 , rtw_get_passing_time_ms(fwdl_start_time)
		);

exit:
	if (pFirmware)
		rtw_mfree((u8 *)pFirmware, sizeof(RT_FIRMWARE_8814));

	InitializeFirmwareVars8814(Adapter);

	return rtStatus;
}



void InitializeFirmwareVars8814(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);

	/* Init Fw LPS related. */
	pwrpriv->bFwCurrentInPSMode = _FALSE;

	/* Init H2C cmd.*/
	rtw_write8(padapter, REG_HMETFR_8814A, 0x0f);

	/* Init H2C counter. by tynli. 2009.12.09. */
	pHalData->LastHMEBoxNum = 0;
}

#if 0
/* */
/*  Description: Determine the contents of H2C BT_FW_PATCH Command sent to FW. */
/*  2013.01.23 by tynli */
/*  Porting from 8723B. 2013.04.01 */
/* */
void
SetFwBTFwPatchCmd(
		PADAPTER Adapter,
		u16		FwSize
)
{
	u8		u1BTFwPatchParm[6] = {0};

	RTW_INFO("SetFwBTFwPatchCmd_8821(): FwSize = %d\n", FwSize);

	/* SET_8812_H2CCMD_BT_FW_PATCH_ENABLE(u1BTFwPatchParm, 1); */
	SET_H2CCMD_BT_FW_PATCH_SIZE(u1BTFwPatchParm, FwSize);
	SET_H2CCMD_BT_FW_PATCH_ADDR0(u1BTFwPatchParm, 0);
	SET_H2CCMD_BT_FW_PATCH_ADDR1(u1BTFwPatchParm, 0xa0);
	SET_H2CCMD_BT_FW_PATCH_ADDR2(u1BTFwPatchParm, 0x10);
	SET_H2CCMD_BT_FW_PATCH_ADDR3(u1BTFwPatchParm, 0x80);

	fill_h2c_cmd_8812(Adapter, H2C_BT_FW_PATCH, 6 , u1BTFwPatchParm);
}


int ReservedPage_Compare(PADAPTER Adapter, PRT_MP_FIRMWARE pFirmware, u32 BTPatchSize)
{
	u8 temp, ret, lastBTsz;
	u32 u1bTmp = 0, address_start = 0, count = 0, i = 0;
	u8	*myBTFwBuffer = NULL;

	myBTFwBuffer = rtw_zmalloc(BTPatchSize);
	if (myBTFwBuffer == NULL) {
		RTW_INFO("%s can't be executed due to the failed malloc.\n", __FUNCTION__);
		Adapter->mppriv.bTxBufCkFail = _TRUE;
		return _FALSE;
	}

	temp = rtw_read8(Adapter, 0x209);

	address_start = (temp * 128) / 8;

	rtw_write32(Adapter, 0x140, 0x00000000);
	rtw_write32(Adapter, 0x144, 0x00000000);
	rtw_write32(Adapter, 0x148, 0x00000000);

	rtw_write8(Adapter, 0x106, 0x69);


	for (i = 0; i < (BTPatchSize / 8); i++) {
		rtw_write32(Adapter, 0x140, address_start + 5 + i) ;

		/* polling until reg 0x140[23]=1; */
		do {
			u1bTmp = rtw_read32(Adapter, 0x140);
			if (u1bTmp & BIT(23)) {
				ret = _SUCCESS;
				break;
			}
			count++;
			RTW_INFO("0x140=%x, wait for 10 ms (%d) times.\n", u1bTmp, count);
			rtw_msleep_os(10); /*  10ms */
		} while (!(u1bTmp & BIT(23)) && count < 50);

		myBTFwBuffer[i * 8 + 0] = rtw_read8(Adapter, 0x144);
		myBTFwBuffer[i * 8 + 1] = rtw_read8(Adapter, 0x145);
		myBTFwBuffer[i * 8 + 2] = rtw_read8(Adapter, 0x146);
		myBTFwBuffer[i * 8 + 3] = rtw_read8(Adapter, 0x147);
		myBTFwBuffer[i * 8 + 4] = rtw_read8(Adapter, 0x148);
		myBTFwBuffer[i * 8 + 5] = rtw_read8(Adapter, 0x149);
		myBTFwBuffer[i * 8 + 6] = rtw_read8(Adapter, 0x14a);
		myBTFwBuffer[i * 8 + 7] = rtw_read8(Adapter, 0x14b);
	}

	rtw_write32(Adapter, 0x140, address_start + 5 + BTPatchSize / 8) ;

	lastBTsz = BTPatchSize % 8;

	/* polling until reg 0x140[23]=1; */
	u1bTmp = 0;
	count = 0;
	do {
		u1bTmp = rtw_read32(Adapter, 0x140);
		if (u1bTmp & BIT(23)) {
			ret = _SUCCESS;
			break;
		}
		count++;
		RTW_INFO("0x140=%x, wait for 10 ms (%d) times.\n", u1bTmp, count);
		rtw_msleep_os(10); /*  10ms */
	} while (!(u1bTmp & BIT(23)) && count < 50);

	for (i = 0; i < lastBTsz; i++)
		myBTFwBuffer[(BTPatchSize / 8) * 8 + i] = rtw_read8(Adapter, (0x144 + i));


	for (i = 0; i < BTPatchSize; i++) {
		if (myBTFwBuffer[i] != pFirmware->szFwBuffer[i]) {
			RTW_INFO(" In direct myBTFwBuffer[%d]=%x , pFirmware->szFwBuffer=%x\n", i, myBTFwBuffer[i], pFirmware->szFwBuffer[i]);
			Adapter->mppriv.bTxBufCkFail = _TRUE;
			break;
		}
	}

	if (myBTFwBuffer != NULL)
		rtw_mfree(myBTFwBuffer, BTPatchSize);

	return _TRUE;
}

#ifdef CONFIG_RTL8821A
s32 FirmwareDownloadBT(PADAPTER padapter, PRT_MP_FIRMWARE pFirmware)
{
	s32 rtStatus;
	u8 *pBTFirmwareBuf;
	u32 BTFirmwareLen;
	u8 download_time;
	s8 i;


	rtStatus = _SUCCESS;
	pBTFirmwareBuf = NULL;
	BTFirmwareLen = 0;

	/* */
	/*  Patch BT Fw. Download BT RAM code to Tx packet buffer. */
	/* */
	if (GET_HAL_DATA(padapter)->bBTFWReady) {
		RTW_INFO("%s: BT Firmware is ready!!\n", __FUNCTION__);
		return _FAIL;
	}

#ifdef CONFIG_FILE_FWIMG
	if (rtw_is_file_readable(rtw_fw_mp_bt_file_path) == _TRUE) {
		RTW_INFO("%s: accquire MP BT FW from file:%s\n", __FUNCTION__, rtw_fw_mp_bt_file_path);

		rtStatus = rtw_retrieve_from_file(rtw_fw_mp_bt_file_path, FwBuffer, 0x8000);
		BTFirmwareLen = rtStatus >= 0 ? rtStatus : 0;
		pBTFirmwareBuf = FwBuffer;
	} else
#endif /*  CONFIG_FILE_FWIMG */
	{
#ifdef CONFIG_EMBEDDED_FWIMG
		RTW_INFO("%s: Download MP BT FW from header\n", __FUNCTION__);

		pBTFirmwareBuf = (u8 *)Rtl8821A_BT_MP_Patch_FW;
		BTFirmwareLen = Rtl8812BFwBTImgArrayLength;
		pFirmware->szFwBuffer = pBTFirmwareBuf;
		pFirmware->ulFwLength = BTFirmwareLen;
#endif /*  CONFIG_EMBEDDED_FWIMG */
	}

	RTW_INFO("%s: MP BT Firmware size=%d\n", __FUNCTION__, BTFirmwareLen);

	/*  for h2c cam here should be set to  true */
	GET_HAL_DATA(padapter)->bFWReady = _TRUE;

	download_time = (BTFirmwareLen + 4095) / 4096;
	RTW_INFO("%s: download_time is %d\n", __FUNCTION__, download_time);

	/*  Download BT patch Fw. */
	for (i = (download_time - 1); i >= 0; i--) {
		if (i == (download_time - 1)) {
			rtStatus = _WriteBTFWtoTxPktBuf8812(padapter, pBTFirmwareBuf + (4096 * i), (BTFirmwareLen - (4096 * i)), 1);
			RTW_INFO("%s: start %d, len %d, time 1\n", __FUNCTION__, 4096 * i, BTFirmwareLen - (4096 * i));
		} else {
			rtStatus = _WriteBTFWtoTxPktBuf8812(padapter, pBTFirmwareBuf + (4096 * i), 4096, (download_time - i));
			RTW_INFO("%s: start %d, len 4096, time %d\n", __FUNCTION__, 4096 * i, download_time - i);
		}

		if (rtStatus != _SUCCESS) {
			RTW_INFO("%s: BT Firmware download to Tx packet buffer fail!\n", __FUNCTION__);
			GET_HAL_DATA(padapter)->bBTFWReady = _FALSE;
			return rtStatus;
		}
	}

	ReservedPage_Compare(padapter, pFirmware, BTFirmwareLen);

	GET_HAL_DATA(padapter)->bBTFWReady = _TRUE;
	SetFwBTFwPatchCmd_8821(padapter, (u16)BTFirmwareLen);
	rtStatus = _CheckWLANFwPatchBTFwReady_8821A(padapter);

	RTW_INFO("<===%s: return %s!\n", __FUNCTION__, rtStatus == _SUCCESS ? "SUCCESS" : "FAIL");
	return rtStatus;
}
#endif /* CONFIG_RTL8821A */
#endif

/* ***********************************************************
 *				Efuse related code
 * *********************************************************** */
void
hal_EfuseParseBTCoexistInfo8814A(
		PADAPTER			Adapter,
		u8			*hwinfo,
		BOOLEAN			AutoLoadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			tempval = 0x0;

	if (!AutoLoadFail) {
		tempval = hwinfo[EEPROM_RF_BOARD_OPTION_8814];
		if (((tempval & 0xe0) >> 5) == 0x1) /* [7:5] */
			pHalData->EEPROMBluetoothCoexist = 1;
		else
			pHalData->EEPROMBluetoothCoexist = 0;
		pHalData->EEPROMBluetoothType = BT_RTL8814A;

		tempval = hwinfo[EEPROM_RF_BT_SETTING_8814];
		if (tempval != 0xFF)
			pHalData->EEPROMBluetoothAntNum = (tempval&0x1) ? Ant_x1 : Ant_x2;
	} else {
		pHalData->EEPROMBluetoothCoexist = 0;
		pHalData->EEPROMBluetoothType = BT_RTL8814A;
		pHalData->EEPROMBluetoothAntNum = Ant_x1;
	}

	RTW_INFO("EEPROMBluetoothCoexist = %d, EEPROMBluetoothAntNum = %s\n",
		pHalData->EEPROMBluetoothCoexist, 
			(pHalData->EEPROMBluetoothAntNum == Ant_x1) ? "Ant_x1" : "Ant_x2");
}

void
hal_ReadPROMVersion8814A(
		PADAPTER	Adapter,
		u8	*PROMContent,
		BOOLEAN	AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (AutoloadFail)
		pHalData->EEPROMVersion = EEPROM_Default_Version;
	else {
		pHalData->EEPROMVersion = *(u8 *)&PROMContent[EEPROM_VERSION_8814];
		if (pHalData->EEPROMVersion == 0xFF)
			pHalData->EEPROMVersion = EEPROM_Default_Version;
	}
}

void
hal_ReadTxPowerInfo8814A(
		PADAPTER		Adapter,
		u8				*PROMContent,
		BOOLEAN			AutoLoadFail
)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);

	/* 2010/10/19 MH Add Regulator recognize for CU. */
	if (!AutoLoadFail) {
		struct registry_priv  *registry_par = &Adapter->registrypriv;

		pHalData->EEPROMRegulatory = (PROMContent[EEPROM_RF_BOARD_OPTION_8814] & 0x7);	/* bit0~2 */
		if (PROMContent[EEPROM_RF_BOARD_OPTION_8814] == 0xFF)
			pHalData->EEPROMRegulatory = (EEPROM_DEFAULT_BOARD_OPTION & 0x7);	/* bit0~2 */
	} else
		pHalData->EEPROMRegulatory = 0;

	RTW_INFO("EEPROMRegulatory = 0x%x\n", pHalData->EEPROMRegulatory);

}

void
hal_ReadBoardType8814A(
		PADAPTER	Adapter,
		u8		*PROMContent,
		BOOLEAN		AutoloadFail
)
{


	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		pHalData->InterfaceSel = (PROMContent[EEPROM_RF_BOARD_OPTION_8814] & 0xE0) >> 5;
		if (PROMContent[EEPROM_RF_BOARD_OPTION_8814] == 0xFF)
			pHalData->InterfaceSel = (EEPROM_DEFAULT_BOARD_OPTION & 0xE0) >> 5;
	} else
		pHalData->InterfaceSel = 0;
}

enum rf_type rtl8814a_rfpath_decision(_adapter *adapter)
{
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	struct registry_priv *regpriv = &adapter->registrypriv;
	enum rf_type rf_path = RF_TYPE_MAX;
	u8 trx_antenna;

	if (!hal_data->bautoload_fail_flag) {
		u8 option = hal_data->efuse_eeprom_data[EEPROM_TRX_ANTENNA_OPTION_8814];

		if (option == 0xff) {
			trx_antenna = RF_4T4R;
			RTW_INFO("EEPROM RF set 4T4R\n");
		} else if (option == 0xee) {
			trx_antenna = RF_3T3R;
			RTW_INFO("EEPROM RF set 3T3R\n");
		} else if (option == 0x66) {
			trx_antenna = RF_2T2R;
			RTW_INFO("EEPROM RF set 2T2R\n");
		} else if (option == 0x6f) {
			trx_antenna = RF_2T4R;
			RTW_INFO("EEPROM RF set 2T4R\n");
		} else {
			trx_antenna = RF_2T4R;
			RTW_INFO("unknown EEPROM RF set, default to 2T4R\n");
		}
	} else {
		trx_antenna = RF_2T4R;
		RTW_INFO("AutoloadFail, default to 2T4R\n");
	}

	/* if driver doesn't set rf_path in registry, use the value of EEPROM */
	if (!RF_TYPE_VALID(regpriv->rf_path)) {
		if ((trx_antenna == RF_4T4R || trx_antenna == RF_3T3R)
			#ifdef CONFIG_USB_HCI
			&& IS_SUPER_SPEED_USB(adapter)
			#endif
		)
			rf_path = RF_3T3R;
		else if (trx_antenna == RF_2T4R)
			rf_path = RF_2T4R;
		else {
			rf_path = RF_2T4R;
			RTW_INFO("default rf type: %d\n", rf_path);
		}
	} else {
		rf_path = regpriv->rf_path;
		#ifdef CONFIG_USB_HCI
		if (!IS_SUPER_SPEED_USB(adapter))
			rf_path = RF_2T4R;
		#endif
	}

	RTW_INFO("[8814A] Final rf_config: %d\n", rf_path);

	return rf_path;
}

void
hal_Read_TRX_antenna_8814A(
		PADAPTER	Adapter,
		u8	*PROMContent,
		BOOLEAN		AutoloadFail
)
{
	/* move to rtl8814a_rfpath_decision */
}

void
hal_ReadThermalMeter_8814A(
		PADAPTER	Adapter,
		u8		*PROMContent,
		BOOLEAN	AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	pHalData->eeprom_thermal_meter = 0xff;

	if (!AutoloadFail)
		pHalData->eeprom_thermal_meter = PROMContent[EEPROM_THERMAL_METER_8814];

#if 0 /* ToDo: check with RF */
	else
		pHalData->eeprom_thermal_meter = EEPROM_Default_ThermalMeter_8814A;

	if ((pHalData->eeprom_thermal_meter == 0xff) || (_TRUE == AutoloadFail)) {
		pHalData->odmpriv.RFCalibrateInfo.bAPKThermalMeterIgnore = _TRUE;
		pHalData->eeprom_thermal_meter = EEPROM_Default_ThermalMeter_8814A;
	}
#endif

	/* pHalData->ThermalMeter[0] = pHalData->eeprom_thermal_meter; */
	RTW_INFO("ThermalMeter = 0x%x\n", pHalData->eeprom_thermal_meter);
}


void hal_ReadRemoteWakeup_8814A(
	PADAPTER		padapter,
		u8			*hwinfo,
		BOOLEAN			AutoLoadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	u8 tmpvalue;

	if (AutoLoadFail) {
		pwrctl->bHWPowerdown = _FALSE;
		pwrctl->bSupportRemoteWakeup = _FALSE;
	} else {
		/* decide hw if support remote wakeup function */
		/* if hw supported, 8051 (SIE) will generate WeakUP signal( D+/D- toggle) when autoresume */
#if 0
		/* todo: wowlan should check the efuse again */
#ifdef CONFIG_USB_HCI
		if (IS_HARDWARE_TYPE_8821U(padapter))
			pwrctl->bSupportRemoteWakeup = (hwinfo[EEPROM_USB_OPTIONAL_FUNCTION0_8811AU] & BIT1) ? _TRUE : _FALSE;
		else
			pwrctl->bSupportRemoteWakeup = (hwinfo[EEPROM_USB_OPTIONAL_FUNCTION0] & BIT1) ? _TRUE : _FALSE;
#endif /* CONFIG_USB_HCI */
#endif
		RTW_INFO("%s...bSupportRemoteWakeup(%x)\n", __FUNCTION__, pwrctl->bSupportRemoteWakeup);
	}
}

void
hal_ReadChannelPlan8814A(
		PADAPTER		padapter,
		u8				*hwinfo,
		BOOLEAN			AutoLoadFail
)
{
	hal_com_config_channel_plan(
		padapter
		, hwinfo ? &hwinfo[EEPROM_COUNTRY_CODE_8814] : NULL
		, hwinfo ? hwinfo[EEPROM_ChannelPlan_8814] : 0xFF
		, padapter->registrypriv.alpha2
		, padapter->registrypriv.channel_plan
		, RTW_CHPLAN_REALTEK_DEFINE
		, AutoLoadFail
	);
}

void hal_GetRxGainOffset_8814A(
	PADAPTER	Adapter,
	u8 			*PROMContent,
	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct registry_priv *pregistrypriv = &Adapter->registrypriv;

	pHalData->RxGainOffset[0] = 0;
	pHalData->RxGainOffset[1] = 0;
	pHalData->RxGainOffset[2] = 0;
	pHalData->RxGainOffset[3] = 0;

	if (((pregistrypriv->reg_rxgain_offset_2g != 0) && (pregistrypriv->reg_rxgain_offset_5gl != 0)) &&
	    ((pregistrypriv->reg_rxgain_offset_5gm != 0) && (pregistrypriv->reg_rxgain_offset_5gh != 0))) {

		pHalData->RxGainOffset[0] = pregistrypriv->reg_rxgain_offset_2g;
		pHalData->RxGainOffset[1] = pregistrypriv->reg_rxgain_offset_5gl;
		pHalData->RxGainOffset[2] = pregistrypriv->reg_rxgain_offset_5gm;
		pHalData->RxGainOffset[3] = pregistrypriv->reg_rxgain_offset_5gh;
		RTW_INFO("%s():Use registrypriv 0x%x 0x%x 0x%x 0x%x !!\n", __func__, pregistrypriv->reg_rxgain_offset_2g, pregistrypriv->reg_rxgain_offset_5gl, pregistrypriv->reg_rxgain_offset_5gm,
			 pregistrypriv->reg_rxgain_offset_5gh);

	} else {
		RTW_INFO("%s(): AutoloadFail = %d!!\n", __func__, AutoloadFail);
		pHalData->RxGainOffset[0] = PROMContent[EEPROM_IG_OFFSET_4_CD_2G_8814A];
		pHalData->RxGainOffset[0] |= (PROMContent[EEPROM_IG_OFFSET_4_AB_2G_8814A]) << 8;
		pHalData->RxGainOffset[1] = PROMContent[EEPROM_IG_OFFSET_4_CD_5GL_8814A];
		pHalData->RxGainOffset[1] |= (PROMContent[EEPROM_IG_OFFSET_4_AB_5GL_8814A]) << 8;
		pHalData->RxGainOffset[2] = PROMContent[EEPROM_IG_OFFSET_4_CD_5GM_8814A];
		pHalData->RxGainOffset[2] |= (PROMContent[EEPROM_IG_OFFSET_4_AB_5GM_8814A]) << 8;
		pHalData->RxGainOffset[3] = PROMContent[EEPROM_IG_OFFSET_4_CD_5GH_8814A];
		pHalData->RxGainOffset[3] |= (PROMContent[EEPROM_IG_OFFSET_4_AB_5GH_8814A]) << 8;
	}
	RTW_INFO("hal_GetRxGainOffset_8814A(): RegRxGainOffset_2G = 0x%x!!\n", pHalData->RxGainOffset[0]);
	RTW_INFO("hal_GetRxGainOffset_8814A(): RegRxGainOffset_5GL = 0x%x!!\n", pHalData->RxGainOffset[1]);
	RTW_INFO("hal_GetRxGainOffset_8814A(): RegRxGainOffset_5GM = 0x%x!!\n", pHalData->RxGainOffset[2]);
	RTW_INFO("hal_GetRxGainOffset_8814A(): RegRxGainOffset_5GH = 0x%x!!\n", pHalData->RxGainOffset[3]);
}


void Hal_EfuseParseKFreeData_8814A(
			PADAPTER		Adapter,
			u8				*PROMContent,
			BOOLEAN			AutoloadFail)
{
#ifdef CONFIG_RF_POWER_TRIM

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct kfree_data_t *kfree_data = &pHalData->kfree_data;
	u8 kfreePhydata[KFREE_GAIN_DATA_LENGTH_8814A];
	u32 i = 0, j = 2, chidx = 0, efuseaddr = 0;
	u8 rfpath = 0;

	if (GET_PG_KFREE_ON_8814A(PROMContent) && PROMContent[0xc8] != 0xff)
		kfree_data->flag |= KFREE_FLAG_ON;
	if (GET_PG_KFREE_THERMAL_K_ON_8814A(PROMContent) && PROMContent[0xc8] != 0xff)
		kfree_data->flag |= KFREE_FLAG_THERMAL_K_ON;

	if (Adapter->registrypriv.RegPwrTrimEnable == 1) {
		kfree_data->flag |= KFREE_FLAG_ON;
		kfree_data->flag |= KFREE_FLAG_THERMAL_K_ON;
	}

	_rtw_memset(kfree_data->bb_gain, 0xff, BB_GAIN_NUM * RF_PATH_MAX);

	if (kfree_data->flag & KFREE_FLAG_ON) {

		for (i = 0; i < KFREE_GAIN_DATA_LENGTH_8814A; i++) {
			efuseaddr = PPG_BB_GAIN_2G_TXBA_OFFSET_8814A - i;

			if (efuseaddr <= PPG_BB_GAIN_2G_TXBA_OFFSET_8814A) {
				efuse_OneByteRead(Adapter, efuseaddr, &kfreePhydata[i], _FALSE);
				RTW_INFO("%s,kfreePhydata[%d] = %x\n", __func__, i, kfreePhydata[i]);
			}
		}
		kfree_data->bb_gain[0][RF_PATH_A]
			= (kfreePhydata[0] & PPG_BB_GAIN_2G_TX_OFFSET_MASK);
		kfree_data->bb_gain[0][RF_PATH_B]
			= (kfreePhydata[0] & PPG_BB_GAIN_2G_TXB_OFFSET_MASK) >> 4;
		kfree_data->bb_gain[0][RF_PATH_C]
			= (kfreePhydata[1] & PPG_BB_GAIN_2G_TX_OFFSET_MASK);
		kfree_data->bb_gain[0][RF_PATH_D]
			= (kfreePhydata[1] & PPG_BB_GAIN_2G_TXB_OFFSET_MASK) >> 4;

		for (chidx = 1; chidx <= BB_GAIN_5GHB; chidx++) {
			for (rfpath = RF_PATH_A;  rfpath < RF_PATH_MAX; rfpath++)
				kfree_data->bb_gain[chidx][rfpath] = kfreePhydata[j + rfpath] & PPG_BB_GAIN_5G_TX_OFFSET_MASK;

			j = j + RF_PATH_MAX;
		}
	}

	if (kfree_data->flag & KFREE_FLAG_THERMAL_K_ON)
		pHalData->eeprom_thermal_meter += kfree_data->thermal;

	RTW_INFO("registrypriv.RegPwrTrimEnable = %d\n", Adapter->registrypriv.RegPwrTrimEnable);

	RTW_INFO("kfree flag:%u\n", kfree_data->flag);
	if (Adapter->registrypriv.RegPwrTrimEnable == 1 || kfree_data->flag & KFREE_FLAG_ON) {
		for (chidx = 0 ; chidx <= BB_GAIN_5GHB; chidx++) {
			for (rfpath = RF_PATH_A;  rfpath < RF_PATH_MAX; rfpath++)
				RTW_INFO("bb_gain[%d][%d]= %x\n", chidx, rfpath, kfree_data->bb_gain[chidx][rfpath]);
		}
	}

#endif /*CONFIG_RF_POWER_TRIM */
}


void
hal_EfuseParseXtal_8814A(
		PADAPTER	pAdapter,
		u8			*hwinfo,
		BOOLEAN		AutoLoadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	if (!AutoLoadFail) {
		pHalData->crystal_cap = hwinfo[EEPROM_XTAL_8814];
		if (pHalData->crystal_cap == 0xFF)
			pHalData->crystal_cap = EEPROM_Default_CrystalCap_8814;	 /* what value should 8814 set? */
	} else
		pHalData->crystal_cap = EEPROM_Default_CrystalCap_8814;
	RTW_INFO("crystal_cap: 0x%2x\n", pHalData->crystal_cap);
}

void
hal_ReadAntennaDiversity8814A(
		PADAPTER		pAdapter,
		u8				*PROMContent,
		BOOLEAN			AutoLoadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	pHalData->AntDivCfg = 0;

	RTW_INFO("SWAS: bHwAntDiv = %x, TRxAntDivType = %x\n",
		 pHalData->AntDivCfg, pHalData->TRxAntDivType);
}

void
hal_ReadPAType_8814A(
		PADAPTER	Adapter,
		u8			*PROMContent,
		BOOLEAN		AutoloadFail,
		u8		*pPAType,
		u8		*pLNAType
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8			LNAType_AB, LNAType_CD;

	if (!AutoloadFail) {
		u8			RFEType = PROMContent[EEPROM_RFE_OPTION_8814];

		if (GetRegAmplifierType2G(Adapter) == 0) { /* AUTO */
			*pPAType = ReadLE1Byte(&PROMContent[EEPROM_PA_TYPE_8814]);

			LNAType_AB = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_AB_2G_8814]);
			LNAType_CD = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_CD_2G_8814]);

			if (*pPAType == 0xFF && RFEType == 0xFF)
				pHalData->ExternalPA_2G  = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			else
				pHalData->ExternalPA_2G = (*pPAType & BIT4) ? 1 : 0;

			if (LNAType_AB == 0xFF)
				pHalData->ExternalLNA_2G = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
			else
				pHalData->ExternalLNA_2G = (LNAType_AB & BIT3) ? 1 : 0;

			*pLNAType =	(LNAType_AB & BIT3) << 1 | (LNAType_AB & BIT7) >> 2 |
				(LNAType_CD & BIT3) << 3 | (LNAType_CD & BIT7);
		} else {
			pHalData->ExternalPA_2G  = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			pHalData->ExternalLNA_2G = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
		}

		if (GetRegAmplifierType5G(Adapter) == 0) { /* AUTO */
			LNAType_AB = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_AB_5G_8814]);
			LNAType_CD = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_CD_5G_8814]);

			if (*pPAType == 0xFF && RFEType == 0xFF)
				pHalData->external_pa_5g  = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			else
				pHalData->external_pa_5g = (*pPAType & BIT0) ? 1 : 0;

			if (LNAType_AB == 0xFF)
				pHalData->external_lna_5g = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
			else
				pHalData->external_lna_5g = (LNAType_AB & BIT3) ? 1 : 0;

			(*pLNAType) |=	((LNAType_AB & BIT3) >> 3 | (LNAType_AB & BIT7) >> 6 |
				(LNAType_CD & BIT3) >> 1 | (LNAType_CD & BIT7) >> 4);
		} else {
			pHalData->external_pa_5g  = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_PA_5G)  ? 1 : 0;
			pHalData->external_lna_5g = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_LNA_5G) ? 1 : 0;
		}
	} else {
		pHalData->ExternalPA_2G  = EEPROM_Default_PAType;
		pHalData->external_pa_5g  = 0xFF;
		pHalData->ExternalLNA_2G = EEPROM_Default_LNAType;
		pHalData->external_lna_5g = 0xFF;

		if (GetRegAmplifierType2G(Adapter) == 0) {
			pHalData->ExternalPA_2G  = 0;
			pHalData->ExternalLNA_2G = 0;
		} else {
			pHalData->ExternalPA_2G  = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			pHalData->ExternalLNA_2G = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
		}
		if (GetRegAmplifierType5G(Adapter) == 0) {
			pHalData->external_pa_5g  = 0;
			pHalData->external_lna_5g = 0;
		} else {
			pHalData->external_pa_5g  = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_PA_5G)  ? 1 : 0;
			pHalData->external_lna_5g = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_LNA_5G) ? 1 : 0;
		}
	}
	RTW_INFO("PAType is 0x%x, LNAType is 0x%x\n", *pPAType, *pLNAType);
	RTW_INFO("pHalData->ExternalPA_2G = %d, pHalData->external_pa_5g = %d\n", pHalData->ExternalPA_2G, pHalData->external_pa_5g);
	RTW_INFO("pHalData->ExternalLNA_2G = %d, pHalData->external_lna_5g = %d\n", pHalData->ExternalLNA_2G, pHalData->external_lna_5g);
}

void hal_ReadAmplifierType_8814A(
		PADAPTER		Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	switch (pHalData->rfe_type) {
	case 1:	/* 8814AU */
		pHalData->external_pa_5g = pHalData->external_lna_5g = _TRUE;
		pHalData->TypeAPA = pHalData->TypeALNA = 0;/* APA and ALNA is 0 */
		break;
	case 2:	/* socket board 8814AR and 8194AR */
		pHalData->ExternalPA_2G = pHalData->external_pa_5g = _TRUE;
		pHalData->ExternalLNA_2G = pHalData->external_lna_5g = _TRUE;
		pHalData->TypeAPA = pHalData->TypeALNA = 0x55;/* APA and ALNA is 1 */
		pHalData->TypeGPA = pHalData->TypeGLNA = 0x55;/* GPA and GLNA is 1 */
		break;
	case 3:	/* high power on-board 8814AR and 8194AR */
		pHalData->ExternalPA_2G = pHalData->external_pa_5g = _TRUE;
		pHalData->ExternalLNA_2G = pHalData->external_lna_5g = _TRUE;
		pHalData->TypeAPA = pHalData->TypeALNA = 0xaa;/* APA and ALNA is 2 */
		pHalData->TypeGPA = pHalData->TypeGLNA = 0xaa;/* GPA and GLNA is 2 */
		break;
	case 4:	/* on-board 8814AR and 8194AR */
		pHalData->ExternalPA_2G = pHalData->external_pa_5g = _TRUE;
		pHalData->ExternalLNA_2G = pHalData->external_lna_5g = _TRUE;
		pHalData->TypeAPA = 0x55;/* APA is 1 */
		pHalData->TypeALNA = 0xff; /* ALNA is 3 */
		pHalData->TypeGPA = pHalData->TypeGLNA = 0x55;/* GPA and GLNA is 1 */
		break;
	case 5:
		pHalData->ExternalPA_2G = pHalData->external_pa_5g = _TRUE;
		pHalData->ExternalLNA_2G = pHalData->external_lna_5g = _TRUE;
		pHalData->TypeAPA = 0xaa; /* APA2 */
		pHalData->TypeALNA = 0x5500; /* ALNA4 */
		pHalData->TypeGPA = pHalData->TypeGLNA = 0xaa; /* GPA2,GLNA2 */
		break;
	case 6:
		pHalData->external_lna_5g = _TRUE;
		pHalData->TypeALNA = 0; /* ALNA0 */
		break;
	case 0:
	default:	/* 8814AE */
		break;
	}

	RTW_INFO("pHalData->ExternalPA_2G = %d, pHalData->external_pa_5g = %d\n", pHalData->ExternalPA_2G, pHalData->external_pa_5g);
	RTW_INFO("pHalData->ExternalLNA_2G = %d, pHalData->external_lna_5g = %d\n", pHalData->ExternalLNA_2G, pHalData->external_lna_5g);
	RTW_INFO("pHalData->TypeGPA = 0x%X, pHalData->TypeAPA = 0x%X\n", pHalData->TypeGPA, pHalData->TypeAPA);
	RTW_INFO("pHalData->TypeGLNA = 0x%X, pHalData->TypeALNA = 0x%X\n", pHalData->TypeGLNA, pHalData->TypeALNA);
}


void
hal_ReadRFEType_8814A(
		PADAPTER	Adapter,
		u8			*PROMContent,
		BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		if ((GetRegRFEType(Adapter) != 64) || 0xFF == PROMContent[EEPROM_RFE_OPTION_8814] || PROMContent[EEPROM_RFE_OPTION_8814] & BIT7) {
			if (GetRegRFEType(Adapter) != 64)
				pHalData->rfe_type = GetRegRFEType(Adapter);
			else if (IS_HARDWARE_TYPE_8814AE(Adapter))
				pHalData->rfe_type = 0;
			else if (IS_HARDWARE_TYPE_8814AU(Adapter))
				pHalData->rfe_type = 1;
			hal_ReadAmplifierType_8814A(Adapter);

		} else {
			/* bit7==0 means  RFE type defined by 0xCA[6:0] */
			pHalData->rfe_type = PROMContent[EEPROM_RFE_OPTION_8814] & 0x7F;
			hal_ReadAmplifierType_8814A(Adapter);
		}
	} else {
		if (GetRegRFEType(Adapter) != 64)
			pHalData->rfe_type = GetRegRFEType(Adapter);
		else if (IS_HARDWARE_TYPE_8814AE(Adapter))
			pHalData->rfe_type = 0;
		else if (IS_HARDWARE_TYPE_8814AU(Adapter))
			pHalData->rfe_type = 1;

		hal_ReadAmplifierType_8814A(Adapter);
	}
	RTW_INFO("RFE Type: 0x%2x\n", pHalData->rfe_type);
}

void
hal_ReadPowerTrackingType_8814A(
		PADAPTER	Adapter,
		u8		*PROMContent,
		BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	/*Efuse 0xC8[7:6] TX Power Tracking Mode*/
	u8 PowerTrackingType = ((PROMContent[EEPROM_TX_PWR_CALIBRATE_RATE_8812] & 0xC0) >> 6);

	if (!AutoloadFail) {
		if (GetRegPowerTrackingType(Adapter) != 64)
			pHalData->rf_power_tracking_type = GetRegPowerTrackingType(Adapter);
	} else
		pHalData->rf_power_tracking_type = 0;

	if (PowerTrackingType == 0 || PowerTrackingType == 1 || PowerTrackingType == 2 || PowerTrackingType == 3)
		pHalData->rf_power_tracking_type = PowerTrackingType;
	else
		pHalData->rf_power_tracking_type = 0;

	RTW_INFO("PowerTracking Type: 0x%2x\n", pHalData->rf_power_tracking_type);
}


static void
hal_EfusePowerSwitch8814A(
		PADAPTER	pAdapter,
		u8		bWrite,
		u8		PwrState)
{
	u8	tempval;
	u16	tmpV16;
	u8	EFUSE_ACCESS_ON_8814A = 0x69;
	u8	EFUSE_ACCESS_OFF_8814A = 0x00;

	if (PwrState == _TRUE) {
		rtw_write8(pAdapter, REG_EFUSE_ACCESS, EFUSE_ACCESS_ON_8814A);

		/* Reset: 0x0000h[28], default valid */
		tmpV16 =  PlatformEFIORead2Byte(pAdapter, REG_SYS_FUNC_EN);
		if (!(tmpV16 & FEN_ELDR)) {
			tmpV16 |= FEN_ELDR ;
			rtw_write16(pAdapter, REG_SYS_FUNC_EN, tmpV16);
		}

		/* Clock: Gated(0x0008h[5]) 8M(0x0008h[1]) clock from ANA, default valid */
		tmpV16 = PlatformEFIORead2Byte(pAdapter, REG_SYS_CLKR);
		if ((!(tmpV16 & LOADER_CLK_EN))  || (!(tmpV16 & ANA8M))) {
			tmpV16 |= (LOADER_CLK_EN | ANA8M) ;
			rtw_write16(pAdapter, REG_SYS_CLKR, tmpV16);
		}

		if (bWrite == _TRUE) {
			/* Enable LDO 2.5V before read/write action */
			tempval = rtw_read8(pAdapter, EFUSE_TEST + 3);
			tempval &= 0x0F;
			tempval |= (VOLTAGE_V25 << 4);
			rtw_write8(pAdapter, EFUSE_TEST + 3, (tempval | 0x80));
		}
	} else {
		rtw_write8(pAdapter, REG_EFUSE_ACCESS, EFUSE_ACCESS_OFF_8814A);

		if (bWrite == _TRUE) {
			/* Disable LDO 2.5V after read/write action */
			tempval = rtw_read8(pAdapter, EFUSE_TEST + 3);
			rtw_write8(pAdapter, EFUSE_TEST + 3, (tempval & 0x7F));
		}
	}
}

static void
rtl8814_EfusePowerSwitch(
		PADAPTER	pAdapter,
		u8		bWrite,
		u8		PwrState)
{
	hal_EfusePowerSwitch8814A(pAdapter, bWrite, PwrState);
}

static void
hal_EfuseReadEFuse8814A(
	PADAPTER		Adapter,
	u16			_offset,
	u16			_size_byte,
	u8		*pbuf,
		BOOLEAN	bPseudoTest
)
{
	u8	*efuseTbl = NULL;
	u16	eFuse_Addr = 0;
	u8	offset = 0, wden = 0;
	u16	i, j;
	u16	**eFuseWord = NULL;
	u16	efuse_utilized = 0;
	u8	efuse_usage = 0;
	u8	offset_2_0 = 0;
	u8	efuseHeader = 0, efuseExtHdr = 0, efuseData = 0;

	/*  */
	/* Do NOT excess total size of EFuse table. Added by Roger, 2008.11.10. */
	/*  */
	if ((_offset + _size_byte) > EFUSE_MAP_LEN_8814A) {
		/* total E-Fuse table is 512bytes */
		RTW_INFO("Hal_EfuseReadEFuse8814A(): Invalid offset(%#x) with read bytes(%#x)!!\n", _offset, _size_byte);
		goto exit;
	}

	efuseTbl = (u8 *)rtw_zmalloc(EFUSE_MAP_LEN_8814A);
	if (efuseTbl == NULL) {
		RTW_INFO("%s: alloc efuseTbl fail!\n", __FUNCTION__);
		goto exit;
	}

	eFuseWord = (u16 **)rtw_malloc2d(EFUSE_MAX_SECTION_8814A, EFUSE_MAX_WORD_UNIT_8814A, 2);
	if (eFuseWord == NULL) {
		RTW_INFO("%s: alloc eFuseWord fail!\n", __FUNCTION__);
		goto exit;
	}

	/* 0. Refresh efuse init map as all oxFF. */
	for (i = 0; i < EFUSE_MAX_SECTION_8814A; i++)
		for (j = 0; j < EFUSE_MAX_WORD_UNIT_8814A; j++)
			eFuseWord[i][j] = 0xFFFF;

	/*  */
	/* 1. Read the first byte to check if efuse is empty!!! */
	/*  */
	/*  */
	efuse_OneByteRead(Adapter, eFuse_Addr++, &efuseHeader, bPseudoTest);

	if (efuseHeader != 0xFF)
		efuse_utilized++;
	else {
		RTW_INFO("EFUSE is empty\n");
		efuse_utilized = 0;
		goto exit;
	}
	/* RT_DISP(FEEPROM, EFUSE_READ_ALL, ("Hal_EfuseReadEFuse8814A(): efuse_utilized: %d\n", efuse_utilized)); */

	/*  */
	/* 2. Read real efuse content. Filter PG header and every section data. */
	/*  */
	while ((efuseHeader != 0xFF) && AVAILABLE_EFUSE_ADDR_8814A(eFuse_Addr)) {
		/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("efuse_Addr-%d efuse_data=%x\n", eFuse_Addr-1, *rtemp8)); */

		/* Check PG header for section num. */
		if (EXT_HEADER(efuseHeader)) {	/* extended header */
			offset_2_0 = GET_HDR_OFFSET_2_0(efuseHeader);
			/* RT_DISP(FEEPROM, EFUSE_READ_ALL, ("extended header offset_2_0=%X\n", offset_2_0)); */

			efuse_OneByteRead(Adapter, eFuse_Addr++, &efuseExtHdr, bPseudoTest);

			/* RT_DISP(FEEPROM, EFUSE_READ_ALL, ("efuse[%X]=%X\n", eFuse_Addr-1, efuseExtHdr)); */

			if (efuseExtHdr != 0xff) {
				efuse_utilized++;
				if (ALL_WORDS_DISABLED(efuseExtHdr)) {
					efuse_OneByteRead(Adapter, eFuse_Addr++, &efuseHeader, bPseudoTest);
					if (efuseHeader != 0xff)
						efuse_utilized++;
					break;
				} else {
					offset = ((efuseExtHdr & 0xF0) >> 1) | offset_2_0;
					wden = (efuseExtHdr & 0x0F);
				}
			} else {
				RTW_INFO("Error condition, extended = 0xff\n");
				/* We should handle this condition. */
				break;
			}
		} else {
			offset = ((efuseHeader >> 4) & 0x0f);
			wden = (efuseHeader & 0x0f);
		}

		if (offset < EFUSE_MAX_SECTION_8814A) {
			/* Get word enable value from PG header */
			/* RT_DISP(FEEPROM, EFUSE_READ_ALL, ("Offset-%X Worden=%X\n", offset, wden)); */

			for (i = 0; i < EFUSE_MAX_WORD_UNIT_8814A; i++) {
				/* Check word enable condition in the section				 */
				if (!(wden & (0x01 << i))) {
					efuse_OneByteRead(Adapter, eFuse_Addr++, &efuseData, bPseudoTest);
					/* RT_DISP(FEEPROM, EFUSE_READ_ALL, ("efuse[%X]=%X\n", eFuse_Addr-1, efuseData)); */
					efuse_utilized++;
					eFuseWord[offset][i] = (efuseData & 0xff);

					if (!AVAILABLE_EFUSE_ADDR_8814A(eFuse_Addr))
						break;

					efuse_OneByteRead(Adapter, eFuse_Addr++, &efuseData, bPseudoTest);
					/* RT_DISP(FEEPROM, EFUSE_READ_ALL, ("efuse[%X]=%X\n", eFuse_Addr-1, efuseData)); */
					efuse_utilized++;
					eFuseWord[offset][i] |= (((u16)efuseData << 8) & 0xff00);

					if (!AVAILABLE_EFUSE_ADDR_8814A(eFuse_Addr))
						break;
				}
			}
		} else { /* deal with error offset,skip error data		 */
			RTW_PRINT("invalid offset:0x%02x\n", offset);
			for (i = 0; i < EFUSE_MAX_WORD_UNIT_8814A; i++) {
				/* Check word enable condition in the section				 */
				if (!(wden & 0x01)) {
					eFuse_Addr++;
					efuse_utilized++;
					if (!AVAILABLE_EFUSE_ADDR_8814A(eFuse_Addr))
						break;
					eFuse_Addr++;
					efuse_utilized++;
					if (!AVAILABLE_EFUSE_ADDR_8814A(eFuse_Addr))
						break;
				}
			}
		}
		/* Read next PG header */
		efuse_OneByteRead(Adapter, eFuse_Addr, &efuseHeader, bPseudoTest);
		/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("Addr=%d rtemp 0x%x\n", eFuse_Addr, *rtemp8)); */

		if (efuseHeader != 0xFF) {
			eFuse_Addr++;
			efuse_utilized++;
		}

	}

	/*  */
	/* 3. Collect 16 sections and 4 word unit into Efuse map. */
	/*  */
	for (i = 0; i < EFUSE_MAX_SECTION_8814A; i++) {
		for (j = 0; j < EFUSE_MAX_WORD_UNIT_8814A; j++) {
			efuseTbl[(i * 8) + (j * 2)] = (eFuseWord[i][j] & 0xff);
			efuseTbl[(i * 8) + ((j * 2) + 1)] = ((eFuseWord[i][j] >> 8) & 0xff);
		}
	}

	/* RT_DISP(FEEPROM, EFUSE_READ_ALL, ("Hal_EfuseReadEFuse8814A(): efuse_utilized: %d\n", efuse_utilized)); */

	/*  */
	/* 4. Copy from Efuse map to output pointer memory!!! */
	/*  */
	for (i = 0; i < _size_byte; i++)
		pbuf[i] = efuseTbl[_offset + i];

	/*  */
	/* 5. Calculate Efuse utilization. */
	/*  */
	efuse_usage = (u8)((eFuse_Addr * 100) / EFUSE_REAL_CONTENT_LEN_8814A);
	rtw_hal_set_hwreg(Adapter, HW_VAR_EFUSE_BYTES, (u8 *)&eFuse_Addr);

exit:
	if (efuseTbl)
		rtw_mfree(efuseTbl, EFUSE_MAP_LEN_8814A);

	if (eFuseWord)
		rtw_mfree2d((void *)eFuseWord, EFUSE_MAX_SECTION_8814A, EFUSE_MAX_WORD_UNIT_8814A, sizeof(u16));
}

static void
rtl8814_ReadEFuse(
	PADAPTER	Adapter,
	u8		efuseType,
	u16		_offset,
	u16		_size_byte,
	u8	*pbuf,
		BOOLEAN	bPseudoTest
)
{
	hal_EfuseReadEFuse8814A(Adapter, _offset, _size_byte, pbuf, bPseudoTest);
}

/* Do not support BT */
void
hal_EFUSEGetEfuseDefinition8814A(
			PADAPTER	pAdapter,
			u8		efuseType,
			u8		type,
			void		*pOut
)
{
	switch (type) {
	case TYPE_EFUSE_MAX_SECTION: {
		u8	*pMax_section;
		pMax_section = (u8 *)pOut;
		*pMax_section = EFUSE_MAX_SECTION_8814A;
	}
	break;
	case TYPE_EFUSE_REAL_CONTENT_LEN: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		*pu2Tmp = EFUSE_REAL_CONTENT_LEN_8814A;
	}
	break;
	case TYPE_EFUSE_CONTENT_LEN_BANK: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		*pu2Tmp = EFUSE_REAL_CONTENT_LEN_8814A;
	}
	break;
	case TYPE_AVAILABLE_EFUSE_BYTES_BANK: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		*pu2Tmp = (u16)(EFUSE_REAL_CONTENT_LEN_8814A - EFUSE_OOB_PROTECT_BYTES);
	}
	break;
	case TYPE_AVAILABLE_EFUSE_BYTES_TOTAL: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		*pu2Tmp = (u16)(EFUSE_REAL_CONTENT_LEN_8814A - EFUSE_OOB_PROTECT_BYTES);
	}
	break;
	case TYPE_EFUSE_MAP_LEN: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		*pu2Tmp = (u16)EFUSE_MAP_LEN_8814A;
	}
	break;
	case TYPE_EFUSE_PROTECT_BYTES_BANK: {
		u8 *pu1Tmp;
		pu1Tmp = (u8 *)pOut;
		*pu1Tmp = (u8)(EFUSE_OOB_PROTECT_BYTES);
	}
	break;
	default: {
		u8 *pu1Tmp;
		pu1Tmp = (u8 *)pOut;
		*pu1Tmp = 0;
	}
	break;
	}
}

static void
rtl8814_EFUSE_GetEfuseDefinition(
			PADAPTER	pAdapter,
			u8		efuseType,
			u8		type,
			void		*pOut,
			BOOLEAN		bPseudoTest
)
{
	hal_EFUSEGetEfuseDefinition8814A(pAdapter, efuseType, type, pOut);
}

static u8
hal_EfuseWordEnableDataWrite8814A(PADAPTER	pAdapter,
					u16		efuse_addr,
					u8		word_en,
					u8		*data,
					BOOLEAN		bPseudoTest)
{
	u16 readbackAddr = 0;
	u16 start_addr = efuse_addr;
	u8  badworden = 0x0F;
	u8  readbackData[PGPKT_DATA_SIZE];

	_rtw_memset((void *)readbackData, 0xff, PGPKT_DATA_SIZE);

	RTW_INFO("word_en = %x efuse_addr=%x\n", word_en, efuse_addr);

	if (!(word_en & BIT0)) {
		readbackAddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[0], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[1], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		    IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 0); /* Use 10K Read, Suggested by Morris & Victor */

		efuse_OneByteRead(pAdapter, readbackAddr, &readbackData[0], bPseudoTest);
		efuse_OneByteRead(pAdapter, readbackAddr + 1, &readbackData[1], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
			IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 1); /* Restored to 1.5K Read, Suggested by Morris & Victor */

		if ((data[0] != readbackData[0]) || (data[1] != readbackData[1]))
			badworden &= (~BIT0);
	}
	if (!(word_en & BIT1)) {
		readbackAddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[2], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[3], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		    IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 0); /* Use 10K Read, Suggested by Morris & Victor */

		efuse_OneByteRead(pAdapter, readbackAddr    , &readbackData[2], bPseudoTest);
		efuse_OneByteRead(pAdapter, readbackAddr + 1, &readbackData[3], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		    IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 1); /* Restored to 1.5K Read, Suggested by Morris & Victor */

		if ((data[2] != readbackData[2]) || (data[3] != readbackData[3]))
			badworden &= (~BIT1);
	}
	if (!(word_en & BIT2)) {
		readbackAddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[4], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[5], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		    IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 0); /* Use 10K Read, Suggested by Morris & Victor */

		efuse_OneByteRead(pAdapter, readbackAddr, &readbackData[4], bPseudoTest);
		efuse_OneByteRead(pAdapter, readbackAddr + 1, &readbackData[5], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		    IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 1); /* Restored to 1.5K Read, Suggested by Morris & Victor */

		if ((data[4] != readbackData[4]) || (data[5] != readbackData[5]))
			badworden &= (~BIT2);
	}
	if (!(word_en & BIT3)) {
		readbackAddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[6], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[7], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		    IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 0); /* Use 10K Read, Suggested by Morris & Victor */

		efuse_OneByteRead(pAdapter, readbackAddr, &readbackData[6], bPseudoTest);
		efuse_OneByteRead(pAdapter, readbackAddr + 1, &readbackData[7], bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		    IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 1); /* Restored to 1.5K Read, Suggested by Morris & Victor */

		if ((data[6] != readbackData[6]) || (data[7] != readbackData[7]))
			badworden &= (~BIT3);
	}
	return badworden;
}

static u8
rtl8814_Efuse_WordEnableDataWrite(PADAPTER	pAdapter,
					u16		efuse_addr,
					u8		word_en,
					u8		*data,
					BOOLEAN		bPseudoTest)
{
	u8	ret = 0;

	ret = hal_EfuseWordEnableDataWrite8814A(pAdapter, efuse_addr, word_en, data, bPseudoTest);

	return ret;
}


static u16 hal_EfuseGetCurrentSize_8814A(PADAPTER	pAdapter, BOOLEAN	bPseudoTest)
{
	int			bContinual = _TRUE;

	u16			efuse_addr = 0;
	u8			hoffset = 0, hworden = 0;
	u8			efuse_data, word_cnts = 0;

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	PEFUSE_HAL		pEfuseHal = &(pHalData->EfuseHal);

	RTW_INFO("=======> %s()\n", __func__);

	if (bPseudoTest)
		efuse_addr = (u16)(fakeEfuseUsedBytes);
	else
		rtw_hal_get_hwreg(pAdapter, HW_VAR_EFUSE_BYTES, (u8 *)&efuse_addr);
	/* RTPRINT(FEEPROM, EFUSE_PG, ("hal_EfuseGetCurrentSize_8723A(), start_efuse_addr = %d\n", efuse_addr)); */

	while (bContinual &&
	       efuse_OneByteRead(pAdapter, efuse_addr , &efuse_data, bPseudoTest) &&
	       (efuse_addr  < EFUSE_REAL_CONTENT_LEN_8814A)) {
		if (efuse_data != 0xFF) {
			if ((efuse_data & 0x1F) == 0x0F) {	/* extended header */
				hoffset = efuse_data;
				efuse_addr++;
				efuse_OneByteRead(pAdapter, efuse_addr, &efuse_data, bPseudoTest);
				if ((efuse_data & 0x0F) == 0x0F) {
					efuse_addr++;
					continue;
				} else {
					hoffset = ((hoffset & 0xE0) >> 5) | ((efuse_data & 0xF0) >> 1);
					hworden = efuse_data & 0x0F;
				}
			} else {
				hoffset = (efuse_data >> 4) & 0x0F;
				hworden =  efuse_data & 0x0F;
			}
			RTW_INFO(FUNC_ADPT_FMT": Offset=%d Worden=%#X\n",
				FUNC_ADPT_ARG(pAdapter), hoffset, hworden);
			word_cnts = Efuse_CalculateWordCnts(hworden);
			/* read next header */
			efuse_addr = efuse_addr + (word_cnts * 2) + 1;
		} else
			bContinual = _FALSE ;
	}

	if (bPseudoTest) {
		fakeEfuseUsedBytes = efuse_addr;
		pEfuseHal->fakeEfuseUsedBytes = efuse_addr;
		RTW_INFO("%s(), return %d\n", __func__, pEfuseHal->fakeEfuseUsedBytes);
	} else {
		pEfuseHal->EfuseUsedBytes = efuse_addr;
		pEfuseHal->EfuseUsedPercentage = (u8)((pEfuseHal->EfuseUsedBytes * 100) / pEfuseHal->PhysicalLen_WiFi);
		rtw_hal_set_hwreg(pAdapter, HW_VAR_EFUSE_BYTES, (u8 *)&efuse_addr);
		rtw_hal_set_hwreg(pAdapter, HW_VAR_EFUSE_USAGE, (u8 *)&(pEfuseHal->EfuseUsedPercentage));
		RTW_INFO("%s(), return %d\n", __func__, efuse_addr);
	}

	return efuse_addr;

}

static u16
rtl8814_EfuseGetCurrentSize(
		PADAPTER	pAdapter,
		u8			efuseType,
		BOOLEAN		bPseudoTest)
{
	u16	ret = 0;

	ret = hal_EfuseGetCurrentSize_8814A(pAdapter, bPseudoTest);

	return ret;
}


static int
hal_EfusePgPacketRead_8814A(
		PADAPTER	pAdapter,
		u8			offset,
		u8			*data,
		BOOLEAN		bPseudoTest)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	PEFUSE_HAL		pEfuseHal = &(pHalData->EfuseHal);
	u8 ReadState = PG_STATE_HEADER;

	int bContinual = _TRUE;
	int bDataEmpty = _TRUE ;

	u8 efuse_data, word_cnts = 0;
	u16 efuse_addr = 0;
	u8 hoffset = 0, hworden = 0;
	u8 tmpidx = 0;
	u8 tmpdata[8];
	u8 tmp_header = 0;

	if (data == NULL)
		return _FALSE;
	if (offset >= EFUSE_MAX_SECTION_JAGUAR)
		return _FALSE;

	_rtw_memset((void *)data, 0xff, sizeof(u8) * PGPKT_DATA_SIZE);
	_rtw_memset((void *)tmpdata, 0xff, sizeof(u8) * PGPKT_DATA_SIZE);

	/*  */
	/* <Roger_TODO> Efuse has been pre-programmed dummy 5Bytes at the end of Efuse by CP. */
	/* Skip dummy parts to prevent unexpected data read from Efuse. */
	/* By pass right now. 2009.02.19. */
	/*  */
	while (bContinual && (efuse_addr  < pEfuseHal->PhysicalLen_WiFi)) {
		/* -------  Header Read ------------- */
		if (ReadState & PG_STATE_HEADER) {
			if (efuse_OneByteRead(pAdapter, efuse_addr , &efuse_data, bPseudoTest) && (efuse_data != 0xFF)) {
				if (ALL_WORDS_DISABLED(efuse_data)) {
					tmp_header = efuse_data;
					efuse_addr++;
					efuse_OneByteRead(pAdapter, efuse_addr , &efuse_data, bPseudoTest);
					if ((efuse_data & 0x0F) != 0x0F) {
						hoffset = ((tmp_header & 0xE0) >> 5) | ((efuse_data & 0xF0) >> 1);
						hworden = efuse_data & 0x0F;
					} else {
						efuse_addr++;
						break;
					}

				} else {
					hoffset = (efuse_data >> 4) & 0x0F;
					hworden =  efuse_data & 0x0F;
				}
				word_cnts = Efuse_CalculateWordCnts(hworden);
				bDataEmpty = _TRUE ;

				if (hoffset == offset) {
					for (tmpidx = 0; tmpidx < word_cnts * 2 ; tmpidx++) {
						if (efuse_OneByteRead(pAdapter, efuse_addr + 1 + tmpidx , &efuse_data, bPseudoTest)) {
							tmpdata[tmpidx] = efuse_data;
							if (efuse_data != 0xff)
								bDataEmpty = _FALSE;
						}
					}
					if (bDataEmpty == _FALSE)
						ReadState = PG_STATE_DATA;
					else { /* read next header							 */
						efuse_addr = efuse_addr + (word_cnts * 2) + 1;
						ReadState = PG_STATE_HEADER;
					}
				} else { /* read next header */
					efuse_addr = efuse_addr + (word_cnts * 2) + 1;
					ReadState = PG_STATE_HEADER;
				}

			} else
				bContinual = _FALSE ;
		}
		/* -------  Data section Read ------------- */
		else if (ReadState & PG_STATE_DATA) {
			efuse_WordEnableDataRead(hworden, tmpdata, data);
			efuse_addr = efuse_addr + (word_cnts * 2) + 1;
			ReadState = PG_STATE_HEADER;
		}

	}

	if ((data[0] == 0xff) && (data[1] == 0xff) && (data[2] == 0xff)  && (data[3] == 0xff) &&
	    (data[4] == 0xff) && (data[5] == 0xff) && (data[6] == 0xff)  && (data[7] == 0xff))
		return _FALSE;
	else
		return _TRUE;
}

static int
rtl8814_Efuse_PgPacketRead(PADAPTER	pAdapter,
				u8			offset,
				u8			*data,
				BOOLEAN		bPseudoTest)
{
	int	ret = 0;

	ret = hal_EfusePgPacketRead_8814A(pAdapter, offset, data, bPseudoTest);

	return ret;
}

BOOLEAN
efuse_PgPacketCheck(
		PADAPTER	pAdapter,
		u8		efuseType,
		BOOLEAN		bPseudoTest
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	if (Efuse_GetCurrentSize(pAdapter, efuseType, bPseudoTest) >= (EFUSE_REAL_CONTENT_LEN_8814A - EFUSE_PROTECT_BYTES_BANK_8814A)) {
		RTW_INFO("%s()error: %x >= %x\n", __func__, Efuse_GetCurrentSize(pAdapter, efuseType, bPseudoTest), (EFUSE_REAL_CONTENT_LEN_8814A - EFUSE_PROTECT_BYTES_BANK_8814A));
		return _FALSE;
	}

	return _TRUE;
}

void
efuse_PgPacketConstruct(
		u8			offset,
		u8			word_en,
		u8			*pData,
		PPGPKT_STRUCT	pTargetPkt
)
{
	_rtw_memset((void *)pTargetPkt->data, 0xFF, sizeof(u8) * 8);
	pTargetPkt->offset = offset;
	pTargetPkt->word_en = word_en;
	efuse_WordEnableDataRead(word_en, pData, pTargetPkt->data);
	pTargetPkt->word_cnts = Efuse_CalculateWordCnts(pTargetPkt->word_en);

	RTW_INFO("efuse_PgPacketConstruct(), targetPkt, offset=%d, word_en=0x%x, word_cnts=%d\n", pTargetPkt->offset, pTargetPkt->word_en, pTargetPkt->word_cnts);
}


u16
efuse_PgPacketExceptionHandle(
		PADAPTER		pAdapter,
		u16			ErrOffset
)
{
	RTW_INFO("===> efuse_PgPacketExceptionHandle(), ErrOffset = 0x%X\n", ErrOffset);

	/* ErrOffset is the offset of bad (extension) header. */
	/* if (IS_HARDWARE_TYPE_8812AU(pAdapter)) */
	/* ErrOffset = Hal_EfusePgPacketExceptionHandle_8812A(pAdapter, ErrOffset); */

	RTW_INFO("<=== efuse_PgPacketExceptionHandle(), recovered! Jump to Offset = 0x%X\n", ErrOffset);

	return ErrOffset;
}


BOOLEAN
hal_EfuseCheckIfDatafollowed(
			PADAPTER		pAdapter,
			u8			word_cnts,
			u16			startAddr,
			BOOLEAN			bPseudoTest
)
{
	BOOLEAN		bRet = FALSE;
	u8		i, efuse_data;

	for (i = 0; i < (word_cnts * 2) ; i++) {
		if (efuse_OneByteRead(pAdapter, (startAddr + i) , &efuse_data, bPseudoTest) && (efuse_data != 0xFF))
			bRet = TRUE;
	}

	return bRet;
}

BOOLEAN
hal_EfuseWordEnMatched(
		PPGPKT_STRUCT	pTargetPkt,
		PPGPKT_STRUCT	pCurPkt,
		u8			*pWden
)
{
	u8	match_word_en = 0x0F;	/* default all words are disabled */

	/* check if the same words are enabled both target and current PG packet */
	if (((pTargetPkt->word_en & BIT0) == 0) &&
	    ((pCurPkt->word_en & BIT0) == 0)) {
		match_word_en &= ~BIT0;				/* enable word 0 */
	}
	if (((pTargetPkt->word_en & BIT1) == 0) &&
	    ((pCurPkt->word_en & BIT1) == 0)) {
		match_word_en &= ~BIT1;				/* enable word 1 */
	}
	if (((pTargetPkt->word_en & BIT2) == 0) &&
	    ((pCurPkt->word_en & BIT2) == 0)) {
		match_word_en &= ~BIT2;				/* enable word 2 */
	}
	if (((pTargetPkt->word_en & BIT3) == 0) &&
	    ((pCurPkt->word_en & BIT3) == 0)) {
		match_word_en &= ~BIT3;				/* enable word 3 */
	}

	*pWden = match_word_en;

	if (match_word_en != 0xf)
		return TRUE;
	else
		return FALSE;
}


BOOLEAN
efuse_PgPacketPartialWrite(
			PADAPTER		pAdapter,
			u8			efuseType,
			u16			*pAddr,
			PPGPKT_STRUCT	pTargetPkt,
			BOOLEAN			bPseudoTest
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	PEFUSE_HAL		pEfuseHal = &(pHalData->EfuseHal);
	BOOLEAN			bRet = _FALSE;
	u8			i, efuse_data = 0, cur_header = 0;
	u8			matched_wden = 0, badworden = 0;
	u16			startAddr = 0;
	PGPKT_STRUCT	curPkt;
	u16			max_sec_num = (efuseType == EFUSE_WIFI) ? pEfuseHal->MaxSecNum_WiFi : pEfuseHal->MaxSecNum_BT;
	u16			efuse_max = pEfuseHal->BankSize;
	u16			efuse_max_available_len =
		(efuseType == EFUSE_WIFI) ? pEfuseHal->TotalAvailBytes_WiFi : pEfuseHal->TotalAvailBytes_BT;

	if (bPseudoTest) {
		pEfuseHal->fakeEfuseBank = (efuseType == EFUSE_WIFI) ? 0 : pEfuseHal->fakeEfuseBank;
		Efuse_GetCurrentSize(pAdapter, efuseType, _TRUE);
	}

	/* EFUSE_GetEfuseDefinition(pAdapter, efuseType, TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, &efuse_max_available_len, bPseudoTest); */
	/* EFUSE_GetEfuseDefinition(pAdapter, efuseType, TYPE_EFUSE_CONTENT_LEN_BANK, &efuse_max, bPseudoTest); */

	if (efuseType == EFUSE_WIFI) {
		if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
			startAddr = (u16)pEfuseHal->fakeEfuseUsedBytes;
#else
			startAddr = (u16)fakeEfuseUsedBytes;
#endif

		} else
			rtw_hal_get_hwreg(pAdapter, HW_VAR_EFUSE_BYTES, (u8 *)&startAddr);
	} else {
		if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
			startAddr = (u16)pEfuseHal->fakeBTEfuseUsedBytes;
#else
			startAddr = (u16)fakeBTEfuseUsedBytes;
#endif

		} else
			rtw_hal_get_hwreg(pAdapter, HW_VAR_EFUSE_BT_BYTES, (u8 *)&startAddr);
	}

	startAddr %= efuse_max;
	RTW_INFO("%s: startAddr=%#X\n", __FUNCTION__, startAddr);
	RTW_INFO("efuse_PgPacketPartialWrite(), startAddr = 0x%X\n", startAddr);

	while (1) {
		if (startAddr >= efuse_max_available_len) {
			bRet = _FALSE;
			RTW_INFO("startAddr(%d) >= efuse_max_available_len(%d)\n",
				 startAddr, efuse_max_available_len);
			break;
		}

		if (efuse_OneByteRead(pAdapter, startAddr, &efuse_data, bPseudoTest) && (efuse_data != 0xFF)) {
			if (EXT_HEADER(efuse_data)) {
				cur_header = efuse_data;
				startAddr++;
				efuse_OneByteRead(pAdapter, startAddr, &efuse_data, bPseudoTest);
				if (ALL_WORDS_DISABLED(efuse_data)) {
					u16 recoveredAddr = startAddr;
					recoveredAddr = efuse_PgPacketExceptionHandle(pAdapter, startAddr - 1);

					if (recoveredAddr == (startAddr - 1)) {
						RTW_INFO("Error! All words disabled and the recovery failed, (Addr, Data) = (0x%X, 0x%X)\n",
							 startAddr, efuse_data);
						pAdapter->LastError = ERR_INVALID_DATA;
						break;
					} else {
						startAddr = recoveredAddr;
						RTW_INFO("Bad extension header but recovered => Keep going.\n");
						continue;
					}
				} else {
					curPkt.offset = ((cur_header & 0xE0) >> 5) | ((efuse_data & 0xF0) >> 1);
					curPkt.word_en = efuse_data & 0x0F;
				}
			} else {
				if (ALL_WORDS_DISABLED(efuse_data)) {
					u16 recoveredAddr = startAddr;
					recoveredAddr = efuse_PgPacketExceptionHandle(pAdapter, startAddr);
					if (recoveredAddr != startAddr) {
						efuse_OneByteRead(pAdapter, startAddr, &efuse_data, bPseudoTest);
						RTW_INFO("Bad header but recovered => Read header again.\n");
					}
				}

				cur_header  =  efuse_data;
				curPkt.offset = (cur_header >> 4) & 0x0F;
				curPkt.word_en = cur_header & 0x0F;
			}

			if (curPkt.offset > max_sec_num) {
				pAdapter->LastError = ERR_OUT_OF_RANGE;
				pEfuseHal->Status = ERR_OUT_OF_RANGE;
				bRet = _FALSE;
				break;
			}

			curPkt.word_cnts = Efuse_CalculateWordCnts(curPkt.word_en);
			/* if same header is found but no data followed */
			/* write some part of data followed by the header. */
			if ((curPkt.offset == pTargetPkt->offset) &&
			    (!hal_EfuseCheckIfDatafollowed(pAdapter, curPkt.word_cnts, startAddr + 1, bPseudoTest)) &&
			    hal_EfuseWordEnMatched(pTargetPkt, &curPkt, &matched_wden)) {
				RTW_INFO("Need to partial write data by the previous wrote header\n");
				/* RT_ASSERT(_FALSE, ("Error, Need to partial write data by the previous wrote header!!\n")); */
				/* Here to write partial data */
				badworden = Efuse_WordEnableDataWrite(pAdapter, startAddr + 1, matched_wden, pTargetPkt->data, bPseudoTest);
				if (badworden != 0x0F) {
					u32 PgWriteSuccess = 0;
					/* if write fail on some words, write these bad words again */

					PgWriteSuccess = Efuse_PgPacketWrite(pAdapter, pTargetPkt->offset, badworden, pTargetPkt->data, bPseudoTest);

					if (!PgWriteSuccess) {
						bRet = _FALSE;	/* write fail, return */
						break;
					}
				}
				/* partial write ok, update the target packet for later use		 */
				for (i = 0; i < 4; i++) {
					if ((matched_wden & (0x1 << i)) == 0) {	/* this word has been written */
						pTargetPkt->word_en |= (0x1 << i);	/* disable the word */
					}
				}
				pTargetPkt->word_cnts = Efuse_CalculateWordCnts(pTargetPkt->word_en);
			}
			/* read from next header */
			startAddr = startAddr + (curPkt.word_cnts * 2) + 1;
		} else {
			/* not used header, 0xff */
			*pAddr = startAddr;
			RTW_INFO("Started from unused header offset=%d\n", startAddr);
			bRet = _TRUE;
			break;
		}
	}
	return bRet;
}


BOOLEAN
hal_EfuseFixHeaderProcess(
			PADAPTER			pAdapter,
			u8				efuseType,
			PPGPKT_STRUCT		pFixPkt,
			u16				*pAddr,
			BOOLEAN				bPseudoTest
)
{
	u8	originaldata[8], badworden = 0;
	u16	efuse_addr = *pAddr;
	u32	PgWriteSuccess = 0;

	_rtw_memset((void *)originaldata, 0xff, 8);

	if (Efuse_PgPacketRead(pAdapter, pFixPkt->offset, originaldata, bPseudoTest)) {
		/* check if data exist */
		badworden = Efuse_WordEnableDataWrite(pAdapter, efuse_addr + 1, pFixPkt->word_en, originaldata, bPseudoTest);

		if (badworden != 0xf) {	/* write fail */
			PgWriteSuccess = Efuse_PgPacketWrite(pAdapter, pFixPkt->offset, badworden, originaldata, bPseudoTest);
			if (!PgWriteSuccess)
				return _FALSE;
			else
				efuse_addr = Efuse_GetCurrentSize(pAdapter, efuseType, bPseudoTest);
		} else
			efuse_addr = efuse_addr + (pFixPkt->word_cnts * 2) + 1;
	} else
		efuse_addr = efuse_addr + (pFixPkt->word_cnts * 2) + 1;
	*pAddr = efuse_addr;
	return _TRUE;
}


BOOLEAN
efuse_PgPacketWrite2ByteHeader(
				PADAPTER		pAdapter,
				u8			efuseType,
				u16			*pAddr,
				PPGPKT_STRUCT	pTargetPkt,
				BOOLEAN			bPseudoTest)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	PEFUSE_HAL		pEfuseHal = &(pHalData->EfuseHal);
	BOOLEAN			bRet = _FALSE;
	u16			efuse_addr = *pAddr;
	u8			pg_header = 0, tmp_header = 0, pg_header_temp = 0;
	u8			repeatcnt = 0;
	u16			efuse_max_available_len =
		(efuseType == EFUSE_WIFI) ? pEfuseHal->TotalAvailBytes_WiFi : pEfuseHal->TotalAvailBytes_BT;

	RTW_INFO("Wirte 2byte header\n");


	while (efuse_addr < efuse_max_available_len) {
		pg_header = ((pTargetPkt->offset & 0x07) << 5) | 0x0F;
		RTW_INFO("pg_header = 0x%x\n", pg_header);
		efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
			IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 0); /* Use 10K Read, Suggested by Morris & Victor*/

		efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);

		if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
			IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
			phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 1); /* Use 10K Read, Suggested by Morris & Victor*/

		while (tmp_header == 0xFF || pg_header != tmp_header) {
			if (repeatcnt++ > pEfuseHal->DataRetry) {
				RTW_INFO("Repeat over limit for pg_header!!\n");
				return _FALSE;
			}

			efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);
			efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);
		}

		/* to write ext_header */
		if (tmp_header == pg_header) {
			efuse_addr++;
			pg_header_temp = pg_header;
			pg_header = ((pTargetPkt->offset & 0x78) << 1) | pTargetPkt->word_en;

			efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);

			if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
				IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
				phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 0); /* Use 10K Read, Suggested by Morris & Victor*/

			efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);

			if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
				IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
				phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 1); /* Use 10K Read, Suggested by Morris & Victor*/

			while (tmp_header == 0xFF || pg_header != tmp_header) {
				if (repeatcnt++ > pEfuseHal->DataRetry) {
					RTW_INFO("Repeat over limit for ext_header!!\n");
					return _FALSE;
				}

				efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);
				efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);
			}

			if ((tmp_header & 0x0F) == 0x0F) {	/* word_en PG fail */
				if (repeatcnt++ > pEfuseHal->DataRetry) {
					RTW_INFO("Repeat over limit for word_en!!\n");
					return _FALSE;
				} else {
					efuse_addr++;
					continue;
				}
			} else if (pg_header != tmp_header) {	/* offset PG fail						 */
				PGPKT_STRUCT	fixPkt;
				/* RT_ASSERT(_FALSE, ("Error, efuse_PgPacketWrite2ByteHeader(), offset PG fail, need to cover the existed data!!\n")); */
				RTW_INFO("Error condition for offset PG fail, need to cover the existed data\n");
				fixPkt.offset = ((pg_header_temp & 0xE0) >> 5) | ((tmp_header & 0xF0) >> 1);
				fixPkt.word_en = tmp_header & 0x0F;
				fixPkt.word_cnts = Efuse_CalculateWordCnts(fixPkt.word_en);
				if (!hal_EfuseFixHeaderProcess(pAdapter, efuseType, &fixPkt, &efuse_addr, bPseudoTest))
					return _FALSE;
			} else {
				bRet = _TRUE;
				break;
			}
		} else if ((tmp_header & 0x1F) == 0x0F) {	/* wrong extended header */
			efuse_addr += 2;
			continue;
		}
	}

	*pAddr = efuse_addr;
	return bRet;
}


BOOLEAN
efuse_PgPacketWrite1ByteHeader(
				PADAPTER		pAdapter,
				u8			efuseType,
				u16			*pAddr,
				PPGPKT_STRUCT	pTargetPkt,
				BOOLEAN			bPseudoTest)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);
	PEFUSE_HAL		pEfuseHal = &(pHalData->EfuseHal);
	BOOLEAN			bRet = _FALSE;
	u8			pg_header = 0, tmp_header = 0;
	u16			efuse_addr = *pAddr;
	u8			repeatcnt = 0;

	RTW_INFO("Wirte 1byte header\n");
	pg_header = ((pTargetPkt->offset << 4) & 0xf0) | pTargetPkt->word_en;

	if (IS_HARDWARE_TYPE_8723BE(pAdapter))
		efuse_OneByteWrite(pAdapter, 0x1FF, 00, _FALSE); /* increase current */


	efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);

	if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
		phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 0); /* Use 10K Read, Suggested by Morris & Victor */

	efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);

	if (IS_HARDWARE_TYPE_8723B(pAdapter) || IS_HARDWARE_TYPE_8188E(pAdapter) ||
		IS_HARDWARE_TYPE_8192E(pAdapter) || IS_HARDWARE_TYPE_8703B(pAdapter) || IS_HARDWARE_TYPE_8188F(pAdapter) || IS_HARDWARE_TYPE_8814A(pAdapter))
		phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT26, 1); /* Restored to 1.5K Read, Suggested by Morris & Victor */


	while (tmp_header == 0xFF || pg_header != tmp_header) {
		if (repeatcnt++ > pEfuseHal->HeaderRetry) {
			RTW_INFO("retry %d times fail!!\n", repeatcnt);
			return _FALSE;
		}
		efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);
		efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);
		RTW_INFO("===> efuse_PgPacketWrite1ByteHeader: Keep %d-th retrying, tmp_header = 0x%X\n", repeatcnt, tmp_header);
	}

	if (pg_header == tmp_header)
		bRet = _TRUE;
	else {
		PGPKT_STRUCT	fixPkt;
		/* RT_ASSERT(_FALSE, ("Error, efuse_PgPacketWrite1ByteHeader(), offset PG fail, need to cover the existed data!!\n")); */
		RTW_INFO(" pg_header(0x%X) != tmp_header(0x%X)\n", pg_header, tmp_header);
		RTW_INFO("Error condition for fixed PG packet, need to cover the existed data: (Addr, Data) = (0x%X, 0x%X)\n",
			 efuse_addr, tmp_header);
		fixPkt.offset = (tmp_header >> 4) & 0x0F;
		fixPkt.word_en = tmp_header & 0x0F;
		fixPkt.word_cnts = Efuse_CalculateWordCnts(fixPkt.word_en);
		if (!hal_EfuseFixHeaderProcess(pAdapter, efuseType, &fixPkt, &efuse_addr, bPseudoTest))
			return _FALSE;
	}

	*pAddr = efuse_addr;
	return bRet;
}



BOOLEAN
efuse_PgPacketWriteHeader(
				PADAPTER		pAdapter,
				u8			efuseType,
				u16			*pAddr,
				PPGPKT_STRUCT	pTargetPkt,
				BOOLEAN			bPseudoTest)
{
	BOOLEAN		bRet = _FALSE;

	if (pTargetPkt->offset >= EFUSE_MAX_SECTION_BASE)
		bRet = efuse_PgPacketWrite2ByteHeader(pAdapter, efuseType, pAddr, pTargetPkt, bPseudoTest);
	else
		bRet = efuse_PgPacketWrite1ByteHeader(pAdapter, efuseType, pAddr, pTargetPkt, bPseudoTest);

	return bRet;
}

BOOLEAN
efuse_PgPacketWriteData(
				PADAPTER		pAdapter,
				u8			efuseType,
				u16			*pAddr,
				PPGPKT_STRUCT	pTargetPkt,
				BOOLEAN			bPseudoTest)
{
	BOOLEAN	bRet = _FALSE;
	u16	efuse_addr = *pAddr;
	u8	badworden = 0;
	u32	PgWriteSuccess = 0;

	badworden = 0x0f;
	badworden = Efuse_WordEnableDataWrite(pAdapter, efuse_addr + 1, pTargetPkt->word_en, pTargetPkt->data, bPseudoTest);
	if (badworden == 0x0F) {
		RTW_INFO("efuse_PgPacketWriteData ok!!\n");
		return _TRUE;
	} else {
		/* Reorganize other pg packet */
		/* RT_ASSERT(_FALSE, ("Error, efuse_PgPacketWriteData(), wirte data fail!!\n")); */
		RTW_INFO("efuse_PgPacketWriteData Fail!!\n");

		PgWriteSuccess = Efuse_PgPacketWrite(pAdapter, pTargetPkt->offset, badworden, pTargetPkt->data, bPseudoTest);
		if (!PgWriteSuccess)
			return _FALSE;
		else
			return _TRUE;
	}

	return bRet;
}


int
hal_EfusePgPacketWrite_8814A(PADAPTER	pAdapter,
					u8			offset,
					u8			word_en,
					u8			*pData,
					BOOLEAN		bPseudoTest)
{
	u8 efuseType = EFUSE_WIFI;
	PGPKT_STRUCT	targetPkt;
	u16			startAddr = 0;

	RTW_INFO("===> efuse_PgPacketWrite[%s], offset: 0x%X\n", (efuseType == EFUSE_WIFI) ? "WIFI" : "BT", offset);

	/* 4 [1] Check if the remaining space is available to write. */
	if (!efuse_PgPacketCheck(pAdapter, efuseType, bPseudoTest)) {
		pAdapter->LastError = ERR_WRITE_PROTECT;
		RTW_INFO("efuse_PgPacketCheck(), fail!!\n");
		return _FALSE;
	}

	/* 4 [2] Construct a packet to write: (Data, Offset, and WordEnable) */
	efuse_PgPacketConstruct(offset, word_en, pData, &targetPkt);


	/* 4 [3] Fix headers without data or fix bad headers, and then return the address where to get started. */
	if (!efuse_PgPacketPartialWrite(pAdapter, efuseType, &startAddr, &targetPkt, bPseudoTest)) {
		pAdapter->LastError = ERR_INVALID_DATA;
		RTW_INFO("efuse_PgPacketPartialWrite(), fail!!\n");
		return _FALSE;
	}

	/* 4 [4] Write the (extension) header. */
	if (!efuse_PgPacketWriteHeader(pAdapter, efuseType, &startAddr, &targetPkt, bPseudoTest)) {
		pAdapter->LastError = ERR_IO_FAILURE;
		RTW_INFO("efuse_PgPacketWriteHeader(), fail!!\n");
		return _FALSE;
	}

	/* 4 [5] Write the data. */
	if (!efuse_PgPacketWriteData(pAdapter, efuseType, &startAddr, &targetPkt, bPseudoTest)) {
		pAdapter->LastError = ERR_IO_FAILURE;
		RTW_INFO("efuse_PgPacketWriteData(), fail!!\n");
		return _FALSE;
	}

	RTW_INFO("<=== efuse_PgPacketWrite\n");
	return _TRUE;
}

static int
rtl8814_Efuse_PgPacketWrite(PADAPTER	pAdapter,
					u8			offset,
					u8			word_en,
					u8			*data,
					BOOLEAN		bPseudoTest)
{
	int	ret;

	ret = hal_EfusePgPacketWrite_8814A(pAdapter, offset, word_en, data, bPseudoTest);

	return ret;
}

void InitRDGSetting8814A(PADAPTER padapter)
{
	rtw_write8(padapter, REG_RD_CTRL, 0xFF);
	rtw_write16(padapter, REG_RD_NAV_NXT, 0x200);
	rtw_write8(padapter, REG_RD_RESP_PKT_TH, 0x05);
}

void ReadRFType8814A(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

#if DISABLE_BB_RF
	pHalData->rf_chip = RF_PSEUDO_11N;
#else
	pHalData->rf_chip = RF_6052;
#endif

	/* if (pHalData->rf_type == RF_1T1R){ */
	/*	pHalData->bRFPathRxEnable[0] = _TRUE; */
	/* } */
	/* else {	 */ /* Default unknow type is 2T2r */
	/*	pHalData->bRFPathRxEnable[0] = pHalData->bRFPathRxEnable[1] = _TRUE; */
	/* } */

	if (IsSupported24G(padapter->registrypriv.wireless_mode) &&
	    is_supported_5g(padapter->registrypriv.wireless_mode))
		pHalData->BandSet = BAND_ON_BOTH;
	else if (is_supported_5g(padapter->registrypriv.wireless_mode))
		pHalData->BandSet = BAND_ON_5G;
	else
		pHalData->BandSet = BAND_ON_2_4G;

	/* if(padapter->bInHctTest) */
	/*	pHalData->BandSet = BAND_ON_2_4G; */
}

void rtl8814_start_thread(PADAPTER padapter)
{
}

void rtl8814_stop_thread(PADAPTER padapter)
{
}

void hal_notch_filter_8814(_adapter *adapter, bool enable)
{
	if (enable) {
		RTW_INFO("Enable notch filter\n");
		/* rtw_write8(adapter, rOFDM0_RxDSP+1, rtw_read8(adapter, rOFDM0_RxDSP+1) | BIT1); */
	} else {
		RTW_INFO("Disable notch filter\n");
		/* rtw_write8(adapter, rOFDM0_RxDSP+1, rtw_read8(adapter, rOFDM0_RxDSP+1) & ~BIT1); */
	}
}

u8
GetEEPROMSize8814A(
		PADAPTER	Adapter
)
{
	u8	size = 0;
	u32	curRCR;

	curRCR = rtw_read16(Adapter, REG_SYS_EEPROM_CTRL);
	size = (curRCR & EEPROMSEL) ? 6 : 4; /* 6: EEPROM used is 93C46, 4: boot from E-Fuse. */

	RTW_INFO("EEPROM type is %s\n", size == 4 ? "E-FUSE" : "93C46");
	/* return size; */
	return 4; /* <20120713, Kordan> The default value of HW is 6 ?!! */
}

#if 0
void CheckAutoloadState8812A(PADAPTER padapter)
{

	u8 val8;
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);


	/*  check system boot selection */
	val8 = rtw_read8(padapter, REG_9346CR);
	pHalData->EepromOrEfuse = (val8 & BOOT_FROM_EEPROM) ? _TRUE : _FALSE;
	pHalData->bautoload_fail_flag = (val8 & EEPROM_EN) ? _FALSE : _TRUE;

	RTW_INFO("%s: 9346CR(%#x)=0x%02x, Boot from %s, Autoload %s!\n",
		 __FUNCTION__, REG_9346CR, val8,
		 (pHalData->EepromOrEfuse ? "EEPROM" : "EFUSE"),
		 (pHalData->bautoload_fail_flag ? "Fail" : "OK"));
}
#endif

void
hal_InitPGData_8814A(
		PADAPTER		padapter,
		u8			*PROMContent
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u32			i;
	u16			value16;

	if (_FALSE == pHalData->bautoload_fail_flag) {
		/* autoload OK. */
		/* hal_ReadeFuse_8814A is FW offload read efuse, todo */
		/* #if (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI))  && (MP_DRIVER != 1) */
		/* if(hal_ReadeFuse_8814A(pAdapter) == _FAIL) */
		/* #endif */

		/* Read EFUSE real map to shadow. */
		EFUSE_ShadowMapUpdate(padapter, EFUSE_WIFI, _FALSE);
	} else {
		/* autoload fail */
		RTW_INFO("AutoLoad Fail reported from CR9346!!\n");
		/* update to default value 0xFF */
		EFUSE_ShadowMapUpdate(padapter, EFUSE_WIFI, _FALSE);
	}

#ifdef CONFIG_EFUSE_CONFIG_FILE
	if (check_phy_efuse_tx_power_info_valid(padapter) == _FALSE) {
		if (Hal_readPGDataFromConfigFile(padapter) != _SUCCESS)
			RTW_ERR("invalid phy efuse and read from file fail, will use driver default!!\n");
	}
#endif
}

static void read_chip_version_8814a(PADAPTER Adapter)
{
	u32	value32;
	PHAL_DATA_TYPE	pHalData;
	u8	vdr;

	pHalData = GET_HAL_DATA(Adapter);

	value32 = rtw_read32(Adapter, REG_SYS_CFG);
	RTW_INFO("%s SYS_CFG(0x%X)=0x%08x\n", __FUNCTION__, REG_SYS_CFG, value32);

	pHalData->version_id.ICType = CHIP_8814A;

	pHalData->version_id.ChipType = ((value32 & RTL_ID) ? TEST_CHIP : NORMAL_CHIP);

	pHalData->version_id.RFType = RF_TYPE_3T3R;

	vdr = (value32 & EXT_VENDOR_ID) >> EXT_VENDOR_ID_SHIFT;
	if (vdr == 0x00)
		pHalData->version_id.VendorType = CHIP_VENDOR_TSMC;
	else if (vdr == 0x01)
		pHalData->version_id.VendorType = CHIP_VENDOR_SMIC;
	else if (vdr == 0x02)
		pHalData->version_id.VendorType = CHIP_VENDOR_UMC;

	pHalData->version_id.CUTVersion = (value32 & CHIP_VER_RTL_MASK) >> CHIP_VER_RTL_SHIFT; /* IC version (CUT) */

	pHalData->MultiFunc = RT_MULTI_FUNC_NONE;

#if 1
	dump_chip_info(pHalData->version_id);
#endif

}

void
hal_PatchwithJaguar_8814(
		PADAPTER				Adapter,
		RT_MEDIA_STATUS		MediaStatus
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct mlme_ext_priv	*pmlmeext = &(Adapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	if ((MediaStatus == RT_MEDIA_CONNECT) &&
	    (pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_REALTEK_JAGUAR_BCUTAP)) {
		rtw_write8(Adapter, rVhtlen_Use_Lsig_Jaguar, 0x1);
		rtw_write8(Adapter, REG_TCR + 3, BIT2);
	} else {
		rtw_write8(Adapter, rVhtlen_Use_Lsig_Jaguar, 0x3F);
		rtw_write8(Adapter, REG_TCR + 3, BIT0 | BIT1 | BIT2);
	}


	/*if(	(MediaStatus == RT_MEDIA_CONNECT) &&
		((pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_REALTEK_JAGUAR_BCUTAP) ||
		 (pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_REALTEK_JAGUAR_CCUTAP)))
	{
		rtw_write8(Adapter, rBWIndication_Jaguar + 3,
			(rtw_read8(Adapter, rBWIndication_Jaguar + 3) | BIT2));
	}
	else
	{
		rtw_write8(Adapter, rBWIndication_Jaguar + 3,
			(rtw_read8(Adapter, rBWIndication_Jaguar + 3) & (~BIT2)));
	}*/
}

void init_hal_spec_8814a(_adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);

	hal_spec->ic_name = "rtl8814a";
	hal_spec->macid_num = 128;
	hal_spec->sec_cam_ent_num = 64;
	hal_spec->sec_cap = SEC_CAP_CHK_BMC;

	hal_spec->rfpath_num_2g = 4;
	hal_spec->rfpath_num_5g = 4;
	hal_spec->rf_reg_path_num = 4;
	hal_spec->max_tx_cnt = 3;

	hal_spec->tx_nss_num = 3;
	hal_spec->rx_nss_num = 3;
	hal_spec->band_cap = BAND_CAP_2G | BAND_CAP_5G;
	hal_spec->bw_cap = BW_CAP_20M | BW_CAP_40M | BW_CAP_80M;
	hal_spec->port_num = 5;
	hal_spec->proto_cap = PROTO_CAP_11B | PROTO_CAP_11G | PROTO_CAP_11N | PROTO_CAP_11AC;

	hal_spec->txgi_max = 63;
	hal_spec->txgi_pdbm = 2;

	hal_spec->wl_func = 0
			    | WL_FUNC_P2P
			    | WL_FUNC_MIRACAST
			    | WL_FUNC_TDLS
			    ;

#if CONFIG_TX_AC_LIFETIME
	hal_spec->tx_aclt_unit_factor = 8;
#endif

	hal_spec->pg_txpwr_saddr = 0x10;
	hal_spec->pg_txgi_diff_factor = 1;

	rtw_macid_ctl_init_sleep_reg(adapter_to_macidctl(adapter)
		, REG_MACID_SLEEP
		, REG_MACID_SLEEP_1
		, REG_MACID_SLEEP_2
		, REG_MACID_SLEEP_3);
}

void InitDefaultValue8814A(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData;
	struct pwrctrl_priv *pwrctrlpriv;
	u8 i;

	pHalData = GET_HAL_DATA(padapter);
	pwrctrlpriv = adapter_to_pwrctl(padapter);

	/* init default value */
	pHalData->fw_ractrl = _FALSE;
	if (!pwrctrlpriv->bkeepfwalive)
		pHalData->LastHMEBoxNum = 0;

	/* init dm default value */
	pHalData->bChnlBWInitialized = _FALSE;
	pHalData->bIQKInitialized = _FALSE;

	pHalData->EfuseHal.fakeEfuseBank = 0;
	pHalData->EfuseHal.fakeEfuseUsedBytes = 0;
	_rtw_memset(pHalData->EfuseHal.fakeEfuseContent, 0xFF, EFUSE_MAX_HW_SIZE);
	_rtw_memset(pHalData->EfuseHal.fakeEfuseInitMap, 0xFF, EFUSE_MAX_MAP_LEN);
	_rtw_memset(pHalData->EfuseHal.fakeEfuseModifiedMap, 0xFF, EFUSE_MAX_MAP_LEN);
}

void
_InitBeaconParameters_8814A(
		PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u16 val16;
	u8  val8;

	val8 = DIS_TSF_UDT;
	val16 = val8 | (val8 << 8); /* port0 and port1 */
#ifdef CONFIG_BT_COEXIST
	if (pHalData->EEPROMBluetoothCoexist == 1) {
		/* Enable prot0 beacon function for PSTDMA */
		val16 |= EN_BCN_FUNCTION;
	}
#endif
	rtw_write16(Adapter, REG_BCN_CTRL, val16);

	/* TBTT setup time */
	rtw_write8(Adapter, REG_TBTT_PROHIBIT, TBTT_PROHIBIT_SETUP_TIME);

	/* TBTT hold time: 0x540[19:8] */
	rtw_write8(Adapter, REG_TBTT_PROHIBIT + 1, TBTT_PROHIBIT_HOLD_TIME_STOP_BCN & 0xFF);
	rtw_write8(Adapter, REG_TBTT_PROHIBIT + 2,
		(rtw_read8(Adapter, REG_TBTT_PROHIBIT + 2) & 0xF0) | (TBTT_PROHIBIT_HOLD_TIME_STOP_BCN >> 8));

	rtw_write8(Adapter, REG_DRVERLYINT, DRIVER_EARLY_INT_TIME_8814);/* 5ms */
	rtw_write8(Adapter, REG_BCNDMATIM, BCN_DMA_ATIME_INT_TIME_8814); /* 2ms */

	/* Suggested by designer timchen. Change beacon AIFS to the largest number */
	/* beacause test chip does not contension before sending beacon. by tynli. 2009.11.03 */
	rtw_write16(Adapter, REG_BCNTCFG, 0x4413);

}

static void
_BeaconFunctionEnable(
		PADAPTER		Adapter,
		BOOLEAN			Enable,
		BOOLEAN			Linked
)
{
	rtw_write8(Adapter, REG_BCN_CTRL, (DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_BCNQ_SUB));

	rtw_write8(Adapter, REG_RD_CTRL + 1, 0x6F);
}



void SetBeaconRelatedRegisters8814A(PADAPTER padapter)
{
	u32	value32;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u32 bcn_ctrl_reg			= REG_BCN_CTRL;
	/* reset TSF, enable update TSF, correcting TSF On Beacon */

	/* REG_MBSSID_BCN_SPACE */
	/* REG_BCNDMATIM */
	/* REG_ATIMWND */
	/* REG_TBTT_PROHIBIT */
	/* REG_DRVERLYINT */
	/* REG_BCN_MAX_ERR	 */
	/* REG_BCNTCFG */ /* (0x510) */
	/* REG_DUAL_TSF_RST */
	/* REG_BCN_CTRL  */ /* (0x550) */

#ifdef CONFIG_CONCURRENT_MODE
	if (padapter->hw_port == HW_PORT1)
		bcn_ctrl_reg = REG_BCN_CTRL_1;
#endif

	/* BCN interval */
	rtw_hal_set_hwreg(padapter, HW_VAR_BEACON_INTERVAL, (u8 *)&pmlmeinfo->bcn_interval);

	rtw_write8(padapter, REG_ATIMWND, 0x02);/* 2ms */

	_InitBeaconParameters_8814A(padapter);

	rtw_write8(padapter, REG_SLOT, 0x09);

	value32 = rtw_read32(padapter, REG_TCR);
	value32 &= ~TSFRST;
	rtw_write32(padapter,  REG_TCR, value32);

	value32 |= TSFRST;
	rtw_write32(padapter, REG_TCR, value32);

	/* NOTE: Fix test chip's bug (about contention windows's randomness) */
	rtw_write8(padapter,  REG_RXTSF_OFFSET_CCK, 0x50);
	rtw_write8(padapter, REG_RXTSF_OFFSET_OFDM, 0x50);

	_BeaconFunctionEnable(padapter, _TRUE, _TRUE);

	ResumeTxBeacon(padapter);

	/* rtw_write8(padapter, 0x422, rtw_read8(padapter, 0x422)|BIT(6)); */

	/* rtw_write8(padapter, 0x541, 0xff); */

	/* rtw_write8(padapter, 0x542, rtw_read8(padapter, 0x541)|BIT(0)); */

	rtw_write8(padapter, bcn_ctrl_reg, rtw_read8(padapter, bcn_ctrl_reg) | DIS_BCNQ_SUB);

}

static void hw_var_set_monitor(PADAPTER Adapter, u8 variable, u8 *val)
{
	u32	rcr_bits;
	u16	value_rxfltmap2;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	struct mlme_priv *pmlmepriv = &(Adapter->mlmepriv);

	if (*((u8 *)val) == _HW_STATE_MONITOR_) {

#ifdef CONFIG_CUSTOMER_ALIBABA_GENERAL
		rcr_bits = RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_APWRMGT | RCR_ADF | RCR_AMF | RCR_APP_PHYST_RXFF;
#else
		/* Receive all type */
		rcr_bits = RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_APWRMGT | RCR_ADF | RCR_ACF | RCR_AMF | RCR_APP_PHYST_RXFF;

		/* Append FCS */
		rcr_bits |= RCR_APPFCS;
#endif
#if 0
		/*
		   CRC and ICV packet will drop in recvbuf2recvframe()
		   We no turn on it.
		 */
		rcr_bits |= (RCR_ACRC32 | RCR_AICV);
#endif

		rtw_hal_get_hwreg(Adapter, HW_VAR_RCR, (u8 *)&pHalData->rcr_backup);
		rtw_hal_set_hwreg(Adapter, HW_VAR_RCR, (u8 *)&rcr_bits);

		/* Receive all data frames */
		value_rxfltmap2 = 0xFFFF;
		rtw_write16(Adapter, REG_RXFLTMAP2, value_rxfltmap2);

#if 0
		/* tx pause */
		rtw_write8(padapter, REG_TXPAUSE, 0xFF);
#endif
	} else {
		/* do nothing */
	}

}

static void hw_var_set_opmode(PADAPTER Adapter, u8 variable, u8 *val)
{
	u8	val8;
	u8	mode = *((u8 *)val);
	static u8 isMonitor = _FALSE;

	HAL_DATA_TYPE			*pHalData = GET_HAL_DATA(Adapter);

	if (isMonitor == _TRUE) {
		/* reset RCR from backup */
		rtw_hal_set_hwreg(Adapter, HW_VAR_RCR, (u8 *)&pHalData->rcr_backup);
		rtw_hal_rcr_set_chk_bssid(Adapter, MLME_ACTION_NONE);
		isMonitor = _FALSE;
	}

	if (mode == _HW_STATE_MONITOR_) {
		isMonitor = _TRUE;
		/* set net_type */
		Set_MSR(Adapter, _HW_STATE_NOLINK_);

		hw_var_set_monitor(Adapter, variable, val);
		return;
	}

	rtw_hal_set_hwreg(Adapter, HW_VAR_MAC_ADDR, adapter_mac_addr(Adapter)); /* set mac addr to mac register */

#ifdef CONFIG_CONCURRENT_MODE
	if (Adapter->hw_port == HW_PORT1) {
		/* disable Port1 TSF update */
		rtw_iface_disable_tsf_update(Adapter);

		/* set net_type */
		Set_MSR(Adapter, mode);

		RTW_INFO("%s()-%d mode = %d\n", __FUNCTION__, __LINE__, mode);

		if ((mode == _HW_STATE_STATION_) || (mode == _HW_STATE_NOLINK_)) {
			if (!rtw_mi_get_ap_num(Adapter) && !rtw_mi_get_mesh_num(Adapter)) {
				StopTxBeacon(Adapter);
#ifdef CONFIG_PCI_HCI
				UpdateInterruptMask8814AE(Adapter, 0, 0, RT_BCN_INT_MASKS, 0);
#else /* CONFIG_PCI_HCI */
#ifdef CONFIG_INTERRUPT_BASED_TXBCN

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
				rtw_write8(Adapter, REG_DRVERLYINT, 0x05);/* restore early int time to 5ms */
				UpdateInterruptMask8814AU(Adapter, _TRUE, 0, IMR_BCNDMAINT0_8812);
#endif /* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
				UpdateInterruptMask8814AU(Adapter, _TRUE , 0, (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812));
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif /* CONFIG_PCI_HCI */
			}

			rtw_write8(Adapter, REG_BCN_CTRL_1, DIS_TSF_UDT | DIS_ATIM); /* disable atim wnd and disable beacon function */
			/* rtw_write8(Adapter,REG_BCN_CTRL_1, DIS_TSF_UDT | EN_BCN_FUNCTION); */
		} else if (mode == _HW_STATE_ADHOC_) {
#ifdef RTL8814AE_SW_BCN
			/*Beacon is polled to TXBUF*/
			rtw_write16(Adapter, REG_CR, rtw_read16(Adapter, REG_CR) | BIT(8));
			RTW_INFO("%s-%d: enable SW BCN, REG_CR=0x%x\n", __func__, __LINE__, rtw_read32(Adapter, REG_CR));
#endif
			ResumeTxBeacon(Adapter);
			rtw_write8(Adapter, REG_BCN_CTRL_1, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_BCNQ_SUB);
		} else if (mode == _HW_STATE_AP_) {
#ifdef CONFIG_PCI_HCI
			UpdateInterruptMask8814AE(Adapter, RT_BCN_INT_MASKS, 0, 0, 0);
#else
#ifdef CONFIG_INTERRUPT_BASED_TXBCN
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
			UpdateInterruptMask8814AU(Adapter, _TRUE , IMR_BCNDMAINT0_8812, 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
			UpdateInterruptMask8814AU(Adapter, _TRUE , (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812), 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif

			rtw_write8(Adapter, REG_BCN_CTRL_1, DIS_TSF_UDT | DIS_BCNQ_SUB);

#ifdef RTL8814AE_SW_BCN
			rtw_write16(Adapter, REG_CR, rtw_read16(Adapter, REG_CR) | BIT(8));
			RTW_INFO("%s-%d: enable SW BCN, REG_CR=0x%x\n", __func__, __LINE__, rtw_read32(Adapter, REG_CR));
#endif

			/* enable to rx data frame */
			rtw_write16(Adapter, REG_RXFLTMAP2, 0xFFFF);

			/* Beacon Control related register for first time */
			rtw_write8(Adapter, REG_BCNDMATIM, 0x02); /* 2ms		 */

			/* rtw_write8(Adapter, REG_BCN_MAX_ERR, 0xFF); */
			rtw_write8(Adapter, REG_ATIMWND_1, 0x0c); /* 12ms for port1 */

			rtw_write16(Adapter, REG_TSFTR_SYN_OFFSET, 0x7fff);/* +32767 (~32ms) */

			/* reset TSF2	 */
			rtw_write8(Adapter, REG_DUAL_TSF_RST, BIT(1));

			/* enable BCN1 Function for if2 */
			/* don't enable update TSF1 for if2 (due to TSF update when beacon/probe rsp are received) */
			rtw_write8(Adapter, REG_BCN_CTRL_1, (DIS_TSF_UDT | EN_BCN_FUNCTION | EN_TXBCN_RPT | DIS_BCNQ_SUB));

#ifdef CONFIG_CONCURRENT_MODE
			if (!rtw_mi_buddy_check_mlmeinfo_state(Adapter, WIFI_FW_ASSOC_SUCCESS))
				rtw_write8(Adapter, REG_BCN_CTRL,
					rtw_read8(Adapter, REG_BCN_CTRL) & ~EN_BCN_FUNCTION);
#endif
			/* BCN1 TSF will sync to BCN0 TSF with offset(0x518) if if1_sta linked */
			/* rtw_write8(Adapter, REG_BCN_CTRL_1, rtw_read8(Adapter, REG_BCN_CTRL_1)|BIT(5)); */
			/* rtw_write8(Adapter, REG_DUAL_TSF_RST, BIT(3)); */

			/* dis BCN0 ATIM  WND if if1 is station */
			rtw_write8(Adapter, REG_BCN_CTRL, rtw_read8(Adapter, REG_BCN_CTRL) | DIS_ATIM);

#ifdef CONFIG_TSF_RESET_OFFLOAD
			/* Reset TSF for STA+AP concurrent mode */
			if (DEV_STA_LD_NUM(adapter_to_dvobj(Adapter))) {
				if (rtw_hal_reset_tsf(Adapter, HW_PORT1) == _FAIL)
					RTW_INFO("ERROR! %s()-%d: Reset port1 TSF fail\n",
						 __FUNCTION__, __LINE__);
			}
#endif /* CONFIG_TSF_RESET_OFFLOAD */
		}
	} else /* else for port0 */
#endif /* CONFIG_CONCURRENT_MODE */
	{
#ifdef CONFIG_MI_WITH_MBSSID_CAM /*For Port0 - MBSS CAM*/
		hw_var_set_opmode_mbid(Adapter, mode);
#else
		/* disable Port0 TSF update */
		rtw_iface_disable_tsf_update(Adapter);

		/* set net_type */
		Set_MSR(Adapter, mode);

		RTW_INFO("%s()-%d mode = %d\n", __FUNCTION__, __LINE__, mode);

		if ((mode == _HW_STATE_STATION_) || (mode == _HW_STATE_NOLINK_)) {
#ifdef CONFIG_CONCURRENT_MODE
			if (!rtw_mi_get_ap_num(Adapter) && !rtw_mi_get_mesh_num(Adapter))
#endif /* CONFIG_CONCURRENT_MODE */
			{
				StopTxBeacon(Adapter);
#ifdef CONFIG_PCI_HCI
				UpdateInterruptMask8814AE(Adapter, 0, 0, RT_BCN_INT_MASKS, 0);
#else
#ifdef CONFIG_INTERRUPT_BASED_TXBCN
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
				rtw_write8(Adapter, REG_DRVERLYINT, 0x05);/* restore early int time to 5ms					 */
				UpdateInterruptMask8814AU(Adapter, _TRUE, 0, IMR_BCNDMAINT0_8812);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
				UpdateInterruptMask8814AU(Adapter, _TRUE , 0, (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812));
#endif /* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif
			}

			rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_ATIM); /* disable atim wnd */
			/* rtw_write8(Adapter,REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION); */
		} else if (mode == _HW_STATE_ADHOC_) {
#ifdef RTL8814AE_SW_BCN
			rtw_write16(Adapter, REG_CR, rtw_read16(Adapter, REG_CR) | BIT(8));
			RTW_INFO("%s-%d: enable SW BCN, REG_CR=0x%x\n", __func__, __LINE__, rtw_read32(Adapter, REG_CR));
#endif
			ResumeTxBeacon(Adapter);
			rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_BCNQ_SUB);
		} else if (mode == _HW_STATE_AP_) {
#ifdef CONFIG_PCI_HCI
			UpdateInterruptMask8814AE(Adapter, RT_BCN_INT_MASKS, 0, 0, 0);
#else
#ifdef CONFIG_INTERRUPT_BASED_TXBCN
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
			UpdateInterruptMask8814AU(Adapter, _TRUE , IMR_BCNDMAINT0_8812, 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
			UpdateInterruptMask8814AU(Adapter, _TRUE , (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812), 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif

			rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | DIS_BCNQ_SUB);
			/*Beacon is polled to TXBUF*/
#ifdef RTL8814AE_SW_BCN
			rtw_write16(Adapter, REG_CR, rtw_read16(Adapter, REG_CR) | BIT(8));
			RTW_INFO("%s-%d: enable SW BCN, REG_CR=0x%x\n", __func__, __LINE__, rtw_read32(Adapter, REG_CR));
#endif

			/* enable to rx data frame */
			rtw_write16(Adapter, REG_RXFLTMAP2, 0xFFFF);

			/* Beacon Control related register for first time */
			rtw_write8(Adapter, REG_BCNDMATIM, 0x02); /* 2ms			 */

			/* rtw_write8(Adapter, REG_BCN_MAX_ERR, 0xFF); */
			rtw_write8(Adapter, REG_ATIMWND, 0x0c); /* 12ms */

			rtw_write16(Adapter, REG_TSFTR_SYN_OFFSET, 0x7fff);/* +32767 (~32ms) */

			/* reset TSF */
			rtw_write8(Adapter, REG_DUAL_TSF_RST, BIT(0));

			/* enable BCN0 Function for if1 */
			/* don't enable update TSF0 for if1 (due to TSF update when beacon/probe rsp are received) */
			rtw_write8(Adapter, REG_BCN_CTRL, (DIS_TSF_UDT | EN_BCN_FUNCTION | EN_TXBCN_RPT | DIS_BCNQ_SUB));

#ifdef CONFIG_CONCURRENT_MODE
			if (!rtw_mi_buddy_check_mlmeinfo_state(Adapter, WIFI_FW_ASSOC_SUCCESS))
				rtw_write8(Adapter, REG_BCN_CTRL_1,
					rtw_read8(Adapter, REG_BCN_CTRL_1) & ~EN_BCN_FUNCTION);
#endif

			/* dis BCN1 ATIM  WND if if2 is station */
			rtw_write8(Adapter, REG_BCN_CTRL_1, rtw_read8(Adapter, REG_BCN_CTRL_1) | DIS_ATIM);
#ifdef CONFIG_TSF_RESET_OFFLOAD
			/* Reset TSF for STA+AP concurrent mode */
			if (DEV_STA_LD_NUM(adapter_to_dvobj(Adapter))) {
				if (rtw_hal_reset_tsf(Adapter, HW_PORT0) == _FAIL)
					RTW_INFO("ERROR! %s()-%d: Reset port0 TSF fail\n",
						 __FUNCTION__, __LINE__);
			}
#endif /* CONFIG_TSF_RESET_OFFLOAD */
		}
#endif /* #ifdef CONFIG_MI_WITH_MBSSID_CAM*/
	}
}

static void rtw_store_all_sta_hwseq(_adapter *padapter)
{
	_irqL	 irqL;
	_list	*plist, *phead;
	u8 index;
	u16	hw_seq[NUM_STA];
	u32 shcut_addr = 0;
	struct sta_info *psta;
	struct	sta_priv *pstapriv = &padapter->stapriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);

	_rtw_memset(hw_seq, 0, NUM_STA);

	/* save each HW sequence of mac id from report fifo */
	for (index = 0; index < macid_ctl->num && index < NUM_STA; index++) {
		if (rtw_macid_is_used(macid_ctl, index)) {
			rtw_write16(padapter, 0x140, 0x662 | ((index & BIT5) >> 5));
			shcut_addr = 0x8000;
			shcut_addr = (shcut_addr | ((index & 0x1f) << 7) | (10 << 2)) + 1;

			/* 2 byte un-alignment handling */
			if (shcut_addr%2)
				hw_seq[index] = le16_to_cpu(rtw_read8(padapter, shcut_addr) + (rtw_read8(padapter, shcut_addr + 1) << 8));
			else
				hw_seq[index] = rtw_read16(padapter, shcut_addr);
			/* RTW_INFO("mac_id:%d is used, hw_seq[index]=%d\n", index, hw_seq[index]); */
		}
	}

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);
	for (index = 0; index < NUM_STA; index++) {
		psta = NULL;

		phead = &(pstapriv->sta_hash[index]);
		plist = get_next(phead);

		while ((rtw_end_of_queue_search(phead, plist)) == _FALSE) {
			psta = LIST_CONTAINOR(plist, struct sta_info, hash_list);
			plist = get_next(plist);

			psta->hwseq = hw_seq[psta->cmn.mac_id];
			/* RTW_INFO(" psta->mac_id=%d, psta->hwseq=%d\n" , psta->cmn.mac_id, psta->hwseq); */
		}

	}
	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);


}

static void rtw_restore_all_sta_hwseq(_adapter *padapter)
{
	_irqL	 irqL;
	_list	*plist, *phead;
	u8 index;
	u16	hw_seq[NUM_STA];
	u32 shcut_addr = 0;
	struct sta_info *psta;
	struct	sta_priv *pstapriv = &padapter->stapriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);

	_rtw_memset(hw_seq, 0, NUM_STA);

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);
	for (index = 0; index < NUM_STA; index++) {
		psta = NULL;

		phead = &(pstapriv->sta_hash[index]);
		plist = get_next(phead);

		while ((rtw_end_of_queue_search(phead, plist)) == _FALSE) {
			psta = LIST_CONTAINOR(plist, struct sta_info, hash_list);
			plist = get_next(plist);

			hw_seq[psta->cmn.mac_id] = psta->hwseq;
			/* RTW_INFO(" psta->mac_id=%d, psta->hwseq=%d\n", psta->mac_id, psta->hwseq); */
		}

	}
	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	/* restore each HW sequence of mac id to report fifo */
	for (index = 0; index < macid_ctl->num && index < NUM_STA; index++) {
		if (rtw_macid_is_used(macid_ctl, index)) {
			rtw_write16(padapter, 0x140, 0x662 | ((index & BIT5) >> 5));
			shcut_addr = 0x8000;
			shcut_addr = (shcut_addr | ((index & 0x1f) << 7) | (10 << 2)) + 1;
			rtw_write16(padapter, shcut_addr, hw_seq[index]);
			/* RTW_INFO("mac_id:%d is used, hw_seq[index]=%d\n", index, hw_seq[index]); */
		}
	}


}

u8 SetHwReg8814A(PADAPTER padapter, u8 variable, u8 *pval)
{
	PHAL_DATA_TYPE pHalData;
	u8 ret = _SUCCESS;
	u8 val8;
	u16 val16;
	u32 val32;


	pHalData = GET_HAL_DATA(padapter);

	switch (variable) {
	case HW_VAR_SET_OPMODE:
		hw_var_set_opmode(padapter, variable, pval);
		break;

	case HW_VAR_BASIC_RATE:
		rtw_var_set_basic_rate(padapter, pval);
	break;

	case HW_VAR_TXPAUSE:
		rtw_write8(padapter, REG_TXPAUSE, *pval);
		break;

	case HW_VAR_SLOT_TIME:
		rtw_write8(padapter, REG_SLOT, *pval);
		break;

	case HW_VAR_RESP_SIFS:
		/* SIFS_Timer = 0x0a0a0808; */
		/* RESP_SIFS for CCK */
		rtw_write8(padapter, REG_RESP_SIFS_CCK, pval[0]); /* SIFS_T2T_CCK (0x08) */
		rtw_write8(padapter, REG_RESP_SIFS_CCK + 1, pval[1]); /* SIFS_R2T_CCK(0x08) */
		/* RESP_SIFS for OFDM */
		rtw_write8(padapter, REG_RESP_SIFS_OFDM, pval[2]); /* SIFS_T2T_OFDM (0x0a) */
		rtw_write8(padapter, REG_RESP_SIFS_OFDM + 1, pval[3]); /* SIFS_R2T_OFDM(0x0a) */
		break;

	case HW_VAR_ACK_PREAMBLE: {
		u8 bShortPreamble = *pval;

		/* Joseph marked out for Netgear 3500 TKIP channel 7 issue.(Temporarily) */
		val8 = (pHalData->nCur40MhzPrimeSC) << 5;
		if (bShortPreamble)
			val8 |= 0x80;
		rtw_write8(padapter, REG_RRSR + 2, val8);
	}
	break;

	case HW_VAR_CAM_INVALID_ALL:
		val32 = BIT(31) | BIT(30);
		rtw_write32(padapter, REG_CAMCMD, val32);
		break;

	case HW_VAR_AC_PARAM_VO:
		rtw_write32(padapter, REG_EDCA_VO_PARAM, *(u32 *)pval);
		break;

	case HW_VAR_AC_PARAM_VI:
		rtw_write32(padapter, REG_EDCA_VI_PARAM, *(u32 *)pval);
		break;

	case HW_VAR_AC_PARAM_BE:
		pHalData->ac_param_be = *(u32 *)pval;
		rtw_write32(padapter, REG_EDCA_BE_PARAM, *(u32 *)pval);
		break;

	case HW_VAR_AC_PARAM_BK:
		rtw_write32(padapter, REG_EDCA_BK_PARAM, *(u32 *)pval);
		break;

	case HW_VAR_ACM_CTRL: {
		u8 acm_ctrl;
		u8 AcmCtrl;

		acm_ctrl = *(u8 *)pval;
		AcmCtrl = rtw_read8(padapter, REG_ACMHWCTRL);

		if (acm_ctrl > 1)
			AcmCtrl = AcmCtrl | 0x1;

		if (acm_ctrl & BIT(1))
			AcmCtrl |= AcmHw_VoqEn;
		else
			AcmCtrl &= (~AcmHw_VoqEn);

		if (acm_ctrl & BIT(2))
			AcmCtrl |= AcmHw_ViqEn;
		else
			AcmCtrl &= (~AcmHw_ViqEn);

		if (acm_ctrl & BIT(3))
			AcmCtrl |= AcmHw_BeqEn;
		else
			AcmCtrl &= (~AcmHw_BeqEn);

		RTW_INFO("[HW_VAR_ACM_CTRL] Write 0x%X\n", AcmCtrl);
		rtw_write8(padapter, REG_ACMHWCTRL, AcmCtrl);
	}
	break;
#ifdef CONFIG_80211N_HT
	case HW_VAR_AMPDU_FACTOR: {
		u32	AMPDULen = *(u8 *)pval;


		if (AMPDULen < VHT_AGG_SIZE_256K)
			AMPDULen = (0x2000 << (*((u8 *)pval))) - 1;
		else
			AMPDULen = 0x3ffff;
		rtw_write32(padapter, REG_AMPDU_MAX_LENGTH_8814A, AMPDULen);
		/* RTW_INFO("SetHwReg8814AU(): HW_VAR_AMPDU_FACTOR %x\n" ,AMPDULen); */
	}
	break;
#endif /* CONFIG_80211N_HT */
	case HW_VAR_H2C_FW_PWRMODE: {
		u8 psmode = *pval;
		rtl8814_set_FwPwrMode_cmd(padapter, psmode);
	}
	break;

	case HW_VAR_H2C_FW_JOINBSSRPT:
		rtl8814_set_FwJoinBssReport_cmd(padapter, *pval);
		break;
	case HW_VAR_DL_RSVD_PAGE:
#ifdef CONFIG_BT_COEXIST
		if (pHalData->EEPROMBluetoothCoexist == 1) {
			if (check_fwstate(&padapter->mlmepriv, WIFI_AP_STATE) == _TRUE)
				rtl8814a_download_BTCoex_AP_mode_rsvd_page(padapter);
		}
#endif /* CONFIG_BT_COEXIST */
		break;

#ifdef CONFIG_P2P_PS
	case HW_VAR_H2C_FW_P2P_PS_OFFLOAD:
		rtl8814_set_p2p_ps_offload_cmd(padapter, *pval);
		break;
#endif /* CONFIG_P2P_PS */

	case HW_VAR_EFUSE_USAGE:
		pHalData->EfuseUsedPercentage = *pval;
		break;

	case HW_VAR_EFUSE_BYTES:
		pHalData->EfuseUsedBytes = *(u16 *)pval;
		break;
#if 0
	case HW_VAR_EFUSE_BT_USAGE:
#ifdef HAL_EFUSE_MEMORY
		pHalData->EfuseHal.BTEfuseUsedPercentage = *pval;
#endif /* HAL_EFUSE_MEMORY */
		break;

	case HW_VAR_EFUSE_BT_BYTES:
#ifdef HAL_EFUSE_MEMORY
		pHalData->EfuseHal.BTEfuseUsedBytes = *(u16 *)pval;
#else /* HAL_EFUSE_MEMORY */
		BTEfuseUsedBytes = *(u16 *)pval;
#endif /* HAL_EFUSE_MEMORY */
		break;
#endif /* 0 */
	case HW_VAR_FIFO_CLEARN_UP: {
		struct pwrctrl_priv *pwrpriv;
		u8 trycnt = 100;

		pwrpriv = adapter_to_pwrctl(padapter);

		/* pause tx */
		rtw_write8(padapter, REG_TXPAUSE, 0xff);

		/* keep sn */
		rtw_store_all_sta_hwseq(padapter);

		if (pwrpriv->bkeepfwalive != _TRUE) {
			/* RX DMA stop */
			val32 = rtw_read32(padapter, REG_RXPKT_NUM);
			val32 |= RW_RELEASE_EN;
			rtw_write32(padapter, REG_RXPKT_NUM, val32);
			do {
				val32 = rtw_read32(padapter, REG_RXPKT_NUM);
				val32 &= RXDMA_IDLE;
				if (val32)
					break;
			} while (--trycnt);
			if (trycnt == 0)
				RTW_INFO("[HW_VAR_FIFO_CLEARN_UP] Stop RX DMA failed......\n");

			/* RQPN Load 0 */
			rtw_write16(padapter, REG_RQPN_NPQ, 0x0);
			rtw_write32(padapter, REG_RQPN, 0x80000000);
			rtw_mdelay_os(10);
		}
	}
	break;

	case HW_VAR_RESTORE_HW_SEQ:
		rtw_restore_all_sta_hwseq(padapter);
		break;

	case HW_VAR_CHECK_TXBUF: {
		u8 retry_limit;
		u32 reg_230 = 0, reg_234 = 0, reg_238 = 0, reg_23c = 0, reg_240 = 0;
		u32 init_reg_230 = 0, init_reg_234 = 0, init_reg_238 = 0, init_reg_23c = 0, init_reg_240 = 0;
		systime start = rtw_get_current_time();
		u32 pass_ms;
		int i = 0;

		retry_limit = 0x01;

		val16 = BIT_SRL(retry_limit) | BIT_LRL(retry_limit);
		rtw_write16(padapter, REG_RETRY_LIMIT, val16);

		while (rtw_get_passing_time_ms(start) < 2000
		       && !RTW_CANNOT_RUN(padapter)
		      ) {
			reg_230 = rtw_read32(padapter, REG_FIFOPAGE_INFO_1_8814A);
			reg_234 = rtw_read32(padapter, REG_FIFOPAGE_INFO_2_8814A);
			reg_238 = rtw_read32(padapter, REG_FIFOPAGE_INFO_3_8814A);
			reg_23c = rtw_read32(padapter, REG_FIFOPAGE_INFO_4_8814A);
			reg_240 = rtw_read32(padapter, REG_FIFOPAGE_INFO_5_8814A);

			if (i == 0) {
				init_reg_230 = reg_230;
				init_reg_234 = reg_234;
				init_reg_238 = reg_238;
				init_reg_23c = reg_23c;
				init_reg_240 = reg_240;
			}

			i++;
			if ((((reg_230 & 0x0c) != ((reg_230 >> 16) & 0x0c)) || ((reg_234 & 0x0c) != ((reg_234 >> 16) & 0x0c))
			     || ((reg_238 & 0x0c) != ((reg_238 >> 16) & 0x0c)) || ((reg_23c & 0x0c) != ((reg_23c >> 16) & 0x0c))
			     || ((reg_240 & 0x0c) != ((reg_240 >> 16) & 0x0c)))) {
				/* RTW_INFO("%s: (HW_VAR_CHECK_TXBUF)TXBUF NOT empty - 0x230=0x%08x, 0x234=0x%08x 0x238=0x%08x,"
				" 0x23c=0x%08x, 0x240=0x%08x (%d)\n"
				, __FUNCTION__, reg_230, reg_234, reg_238, reg_23c, reg_240, i); */
				rtw_msleep_os(10);
			} else
				break;
		}

		pass_ms = rtw_get_passing_time_ms(start);

		if (RTW_CANNOT_RUN(padapter))
			RTW_INFO("bDriverStopped or bSurpriseRemoved\n");
		else if (pass_ms >= 2000 || (((reg_230 & 0x0c) != ((reg_230 >> 16) & 0x0c)) || ((reg_234 & 0x0c) != ((reg_234 >> 16) & 0x0c))
			|| ((reg_238 & 0x0c) != ((reg_238 >> 16) & 0x0c)) || ((reg_23c & 0x0c) != ((reg_23c >> 16) & 0x0c))
			|| ((reg_240 & 0x0c) != ((reg_240 >> 16) & 0x0c)))) {
			RTW_PRINT("%s:(HW_VAR_CHECK_TXBUF)NOT empty(%d) in %d ms\n", __func__, i, pass_ms);
			RTW_PRINT("%s:(HW_VAR_CHECK_TXBUF) 0x230=0x%08x, 0x234=0x%08x 0x238=0x%08x, 0x23c=0x%08x, 0x240=0x%08x (0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x)\n", __func__,
				reg_230, reg_234, reg_238, reg_23c, reg_240
				, init_reg_230, init_reg_234, init_reg_238, init_reg_23c, init_reg_240);
			/* rtw_warn_on(1); */
		} else
			RTW_INFO("%s:(HW_VAR_CHECK_TXBUF)TXBUF Empty(%d) in %d ms\n", __FUNCTION__, i, pass_ms);

		retry_limit = RL_VAL_STA;
		val16 = BIT_SRL(retry_limit) | BIT_LRL(retry_limit);
		rtw_write16(padapter, REG_RETRY_LIMIT, val16);
	}

	break;

	case HW_VAR_NAV_UPPER: {
		u32 usNavUpper = *((u32 *)pval);

		if (usNavUpper > HAL_NAV_UPPER_UNIT * 0xFF) {
			RTW_INFO("%s: [HW_VAR_NAV_UPPER] set value(0x%08X us) is larger than (%d * 0xFF)!\n",
				__FUNCTION__, usNavUpper, HAL_NAV_UPPER_UNIT);
			break;
		}

		/* The value of ((usNavUpper + HAL_NAV_UPPER_UNIT - 1) / HAL_NAV_UPPER_UNIT) */
		/* is getting the upper integer. */
		usNavUpper = (usNavUpper + HAL_NAV_UPPER_UNIT - 1) / HAL_NAV_UPPER_UNIT;
		rtw_write8(padapter, REG_NAV_UPPER, (u8)usNavUpper);
	}
	break;

	case HW_VAR_BCN_VALID:
#ifdef CONFIG_CONCURRENT_MODE
		if (padapter->hw_port == HW_PORT1) {
			/* BCN_VALID, BIT31 of REG_FIFOPAGE_CTRL_2_8814A, write 1 to clear, Clear by sw */
			val8 = rtw_read8(padapter, REG_FIFOPAGE_CTRL_2_8814A + 3);
			val8 |= BIT(7);
			rtw_write8(padapter, REG_FIFOPAGE_CTRL_2_8814A + 3, val8);
		} else
#endif
		{
			/* BCN_VALID, BIT15 of REG_FIFOPAGE_CTRL_2_8814A, write 1 to clear, Clear by sw */
			val8 = rtw_read8(padapter, REG_FIFOPAGE_CTRL_2_8814A + 1);
			val8 |= BIT(7);
			rtw_write8(padapter,  REG_FIFOPAGE_CTRL_2_8814A + 1, val8);
		}
		break;

	case HW_VAR_DL_BCN_SEL:
#if 0 /* for MBSSID, so far we don't need this */
#ifdef CONFIG_CONCURRENT_MODE
		if (IS_HARDWARE_TYPE_8821(padapter) && padapter->hw_port == HW_PORT1) {
			/* SW_BCN_SEL - Port1 */
			val8 = rtw_read8(padapter, REG_AUTO_LLT_8814A);
			val8 |= BIT(2);
			rtw_write8(padapter, REG_AUTO_LLT_8814A, val8);
		} else
#endif /* CONFIG_CONCURRENT_MODE */
		{
			/* SW_BCN_SEL - Port0 , BIT_r_EN_BCN_SW_HEAD_SEL */
			val8 = rtw_read8(padapter, REG_AUTO_LLT_8814A);
			val8 &= ~BIT(2);
			rtw_write8(padapter, REG_AUTO_LLT_8814A, val8);
		}
#endif /* for MBSSID, so far we don't need this */
		break;

	case HW_VAR_WIRELESS_MODE: {
		struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
		struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
		struct mlme_ext_info *pmlmeinfo = &pmlmeext->mlmext_info;
		u8	R2T_SIFS = 0, SIFS_Timer = 0;
		u8	wireless_mode = *pval;

		if ((wireless_mode == WIRELESS_11BG) || (wireless_mode == WIRELESS_11G))
			SIFS_Timer = 0xa;
		else
			SIFS_Timer = 0xe;

		/* SIFS for OFDM Data ACK */
		rtw_write8(padapter, REG_SIFS_CTX + 1, SIFS_Timer);
		/* SIFS for OFDM consecutive tx like CTS data! */
		rtw_write8(padapter, REG_SIFS_TRX + 1, SIFS_Timer);

		rtw_write8(padapter, REG_SPEC_SIFS + 1, SIFS_Timer);
		rtw_write8(padapter, REG_MAC_SPEC_SIFS + 1, SIFS_Timer);

		/* 20100719 Joseph: Revise SIFS setting due to Hardware register definition change. */
		rtw_write8(padapter, REG_RESP_SIFS_OFDM + 1, SIFS_Timer);
		rtw_write8(padapter, REG_RESP_SIFS_OFDM, SIFS_Timer);

		/*  */
		/* Adjust R2T SIFS for IOT issue. Add by hpfan 2013.01.25 */
		/* Set R2T SIFS to 0x0a for Atheros IOT. Add by hpfan 2013.02.22 */
		/*  */
		/* Mac has 10 us delay so use 0xa value is enough. */
		R2T_SIFS = 0xa;
#ifdef CONFIG_80211AC_VHT
		if (wireless_mode & WIRELESS_11_5AC &&
		    /* MgntLinkStatusQuery(Adapter)  && */
		    TEST_FLAG(pmlmepriv->vhtpriv.ldpc_cap, LDPC_VHT_ENABLE_RX) &&
		    TEST_FLAG(pmlmepriv->vhtpriv.stbc_cap, STBC_VHT_ENABLE_RX)) {
			if (pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_ATHEROS)
				R2T_SIFS = 0x8;
			else
				R2T_SIFS = 0xa;
		}
#endif /* CONFIG_80211AC_VHT */

		rtw_write8(padapter, REG_RESP_SIFS_OFDM + 1, R2T_SIFS);
	}
	break;

#ifdef CONFIG_BEAMFORMING /*PHYDM_BF - (BEAMFORMING_SUPPORT == 1)*/
	/*add by YuChen for PHYDM-TxBF AutoTest HW Timer*/
	case HW_VAR_HW_REG_TIMER_INIT: {
		HAL_HW_TIMER_TYPE TimerType = (*(PHAL_HW_TIMER_TYPE)pval) >> 16;

		rtw_write8(padapter, 0x164, 1);

		if (TimerType == HAL_TIMER_TXBF)
			rtw_write32(padapter, 0x15C, (*(u16 *)pval));
		else if (TimerType == HAL_TIMER_EARLYMODE)
			rtw_write32(padapter, 0x160, 0x05000190);
		break;
	}
	case HW_VAR_HW_REG_TIMER_START: {
		HAL_HW_TIMER_TYPE TimerType = *(PHAL_HW_TIMER_TYPE)pval;

		if (TimerType == HAL_TIMER_TXBF)
			rtw_write8(padapter, 0x15F, 0x5);
		else if (TimerType == HAL_TIMER_EARLYMODE)
			rtw_write8(padapter, 0x163, 0x5);
		break;
	}
	case HW_VAR_HW_REG_TIMER_RESTART: {
		HAL_HW_TIMER_TYPE TimerType = *(PHAL_HW_TIMER_TYPE)pval;

		if (TimerType == HAL_TIMER_TXBF) {
			rtw_write8(padapter, 0x15F, 0x0);
			rtw_write8(padapter, 0x15F, 0x5);
		} else if (TimerType == HAL_TIMER_EARLYMODE) {
			rtw_write8(padapter, 0x163, 0x0);
			rtw_write8(padapter, 0x163, 0x5);
		}
		break;
	}
	case HW_VAR_HW_REG_TIMER_STOP: {
		HAL_HW_TIMER_TYPE TimerType = *(PHAL_HW_TIMER_TYPE)pval;

		if (TimerType == HAL_TIMER_TXBF)
			rtw_write8(padapter, 0x15F, 0);
		else if (TimerType == HAL_TIMER_EARLYMODE)
			rtw_write8(padapter, 0x163, 0x0);
		break;
	}
#endif/*#ifdef CONFIG_BEAMFORMING*/

#ifdef CONFIG_GPIO_WAKEUP
	case HW_SET_GPIO_WL_CTRL: {
		u8 enable = *pval;
		u8 value = rtw_read8(padapter, 0x4e);
		if (enable && (value & BIT(6))) {
			value &= ~BIT(6);
			rtw_write8(padapter, 0x4e, value);
		} else if (enable == _FALSE) {
			value |= BIT(6);
			rtw_write8(padapter, 0x4e, value);
		}
		RTW_INFO("%s: set WL control, 0x4E=0x%02X\n",
			 __func__, rtw_read8(padapter, 0x4e));
	}
	break;
#endif

#if defined(CONFIG_TDLS) && defined(CONFIG_TDLS_CH_SW)
	case HW_VAR_TDLS_BCN_EARLY_C2H_RPT:
		rtl8814_set_BcnEarly_C2H_Rpt_cmd(padapter, *pval);
		break;
#endif

	default:
		ret = SetHwReg(padapter, variable, pval);
		break;
	}

	return ret;
}

struct qinfo_8814a {
	u32 head:11;
	u32 tail:11;
	u32 empty:1;
	u32 ac:2;
	u32 macid:7;
};

struct bcn_qinfo_8814a {
	u16 head:12;
	u16 rsvd:4;
};

void dump_qinfo_8814a(void *sel, struct qinfo_8814a *info, u32 pkt_num, const char *tag)
{
	/* if (info->pkt_num) */
	RTW_PRINT_SEL(sel, "%shead:0x%02x, tail:0x%02x, pkt_num:%u, macid:%u, ac:%u\n"
		, tag ? tag : "", info->head, info->tail, pkt_num, info->macid, info->ac
		     );
}

void dump_bcn_qinfo_8814a(void *sel, struct bcn_qinfo_8814a *info, u32 pkt_num, const char *tag)
{
	/* if (info->pkt_num) */
	RTW_PRINT_SEL(sel, "%shead:0x%02x, pkt_num:%u\n"
		      , tag ? tag : "", info->head, pkt_num
		     );
}

void dump_mac_qinfo_8814a(void *sel, _adapter *adapter)
{
	u32 q0_info;
	u32 q1_info;
	u32 q2_info;
	u32 q3_info;
	u32 q4_info;
	u32 q5_info;
	u32 q6_info;
	u32 q7_info;
	u32 mg_q_info;
	u32 hi_q_info;
	u16 bcn_q_info;
	u32 q0_q1_info;
	u32 q2_q3_info;
	u32 q4_q5_info;
	u32 q6_q7_info;
	u32 mg_hi_q_info;
	u32 cmd_bcn_q_info;


	q0_info = rtw_read32(adapter, REG_Q0_INFO);
	q1_info = rtw_read32(adapter, REG_Q1_INFO);
	q2_info = rtw_read32(adapter, REG_Q2_INFO);
	q3_info = rtw_read32(adapter, REG_Q3_INFO);
	q4_info = rtw_read32(adapter, REG_Q4_INFO);
	q5_info = rtw_read32(adapter, REG_Q5_INFO);
	q6_info = rtw_read32(adapter, REG_Q6_INFO);
	q7_info = rtw_read32(adapter, REG_Q7_INFO);
	mg_q_info = rtw_read32(adapter, REG_MGQ_INFO);
	hi_q_info = rtw_read32(adapter, REG_HGQ_INFO);
	bcn_q_info = rtw_read16(adapter, REG_BCNQ_INFO);

	q0_q1_info = rtw_read32(adapter, REG_Q0_Q1_INFO_8814A);
	q2_q3_info = rtw_read32(adapter, REG_Q2_Q3_INFO_8814A);
	q4_q5_info = rtw_read32(adapter, REG_Q4_Q5_INFO_8814A);
	q6_q7_info = rtw_read32(adapter, REG_Q6_Q7_INFO_8814A);
	mg_hi_q_info = rtw_read32(adapter, REG_MGQ_HIQ_INFO_8814A);
	cmd_bcn_q_info = rtw_read32(adapter, REG_CMDQ_BCNQ_INFO_8814A);

	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q0_info, q0_q1_info&0xFFF, "Q0 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q1_info, (q0_q1_info>>15)&0xFFF, "Q1 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q2_info, q2_q3_info&0xFFF, "Q2 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q3_info, (q2_q3_info>>15)&0xFFF, "Q3 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q4_info, q4_q5_info&0xFFF, "Q4 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q5_info, (q4_q5_info>>15)&0xFFF, "Q5 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q6_info, q6_q7_info&0xFFF, "Q6 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&q7_info, (q6_q7_info>>15)&0xFFF, "Q7 ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&mg_q_info, mg_hi_q_info&0xFFF, "MG ");
	dump_qinfo_8814a(sel, (struct qinfo_8814a *)&hi_q_info, (mg_hi_q_info>>15)&0xFFF, "HI ");
	dump_bcn_qinfo_8814a(sel, (struct bcn_qinfo_8814a *)&bcn_q_info, cmd_bcn_q_info&0xFFF, "BCN ");
}

static void dump_mac_txfifo_8814a(void *sel, _adapter *adapter)
{
	u32 hpq, lpq, npq, epq, pubq;

	hpq = rtw_read32(adapter, REG_FIFOPAGE_INFO_1_8814A);
	lpq = rtw_read32(adapter, REG_FIFOPAGE_INFO_2_8814A);
	npq = rtw_read32(adapter, REG_FIFOPAGE_INFO_3_8814A);
	epq = rtw_read32(adapter, REG_FIFOPAGE_INFO_4_8814A);
	pubq = rtw_read32(adapter, REG_FIFOPAGE_INFO_5_8814A);

	hpq = (hpq & 0xFFF0000)>>16;
	lpq = (lpq & 0xFFF0000)>>16;
	npq = (npq & 0xFFF0000)>>16;
	epq = (epq & 0xFFF0000)>>16;
	pubq = (pubq & 0xFFF0000)>>16;

	RTW_PRINT_SEL(sel, "Tx: available page num: ");
	if ((hpq == 0xAEA) && (hpq == lpq) && (hpq == pubq))
		RTW_PRINT_SEL(sel, "N/A (reg val = 0xea)\n");
	else
		RTW_PRINT_SEL(sel, "HPQ: %d, LPQ: %d, NPQ: %d, EPQ: %d, PUBQ: %d\n"
			, hpq, lpq, npq, epq, pubq);
}

void rtl8814a_read_wmmedca_reg(PADAPTER adapter, u16 *vo_params, u16 *vi_params, u16 *be_params, u16 *bk_params)
{
	u8 vo_reg_params[4];
	u8 vi_reg_params[4];
	u8 be_reg_params[4];
	u8 bk_reg_params[4];

	GetHwReg8814A(adapter, HW_VAR_AC_PARAM_VO, vo_reg_params);
	GetHwReg8814A(adapter, HW_VAR_AC_PARAM_VI, vi_reg_params);
	GetHwReg8814A(adapter, HW_VAR_AC_PARAM_BE, be_reg_params);
	GetHwReg8814A(adapter, HW_VAR_AC_PARAM_BK, bk_reg_params);

	vo_params[0] = vo_reg_params[0];
	vo_params[1] = vo_reg_params[1] & 0x0F;
	vo_params[2] = (vo_reg_params[1] & 0xF0) >> 4;
	vo_params[3] = ((vo_reg_params[3] << 8) | (vo_reg_params[2])) * 32;

	vi_params[0] = vi_reg_params[0];
	vi_params[1] = vi_reg_params[1] & 0x0F;
	vi_params[2] = (vi_reg_params[1] & 0xF0) >> 4;
	vi_params[3] = ((vi_reg_params[3] << 8) | (vi_reg_params[2])) * 32;

	be_params[0] = be_reg_params[0];
	be_params[1] = be_reg_params[1] & 0x0F;
	be_params[2] = (be_reg_params[1] & 0xF0) >> 4;
	be_params[3] = ((be_reg_params[3] << 8) | (be_reg_params[2])) * 32;

	bk_params[0] = bk_reg_params[0];
	bk_params[1] = bk_reg_params[1] & 0x0F;
	bk_params[2] = (bk_reg_params[1] & 0xF0) >> 4;
	bk_params[3] = ((bk_reg_params[3] << 8) | (bk_reg_params[2])) * 32;

	vo_params[1] = (1 << vo_params[1]) - 1;
	vo_params[2] = (1 << vo_params[2]) - 1;
	vi_params[1] = (1 << vi_params[1]) - 1;
	vi_params[2] = (1 << vi_params[2]) - 1;
	be_params[1] = (1 << be_params[1]) - 1;
	be_params[2] = (1 << be_params[2]) - 1;
	bk_params[1] = (1 << bk_params[1]) - 1;
	bk_params[2] = (1 << bk_params[2]) - 1;
}

void GetHwReg8814A(PADAPTER padapter, u8 variable, u8 *pval)
{
	PHAL_DATA_TYPE pHalData;
	u8 val8;
	u16 val16;
	u32 val32;


	pHalData = GET_HAL_DATA(padapter);

	switch (variable) {
	case HW_VAR_TXPAUSE:
		*pval = rtw_read8(padapter, REG_TXPAUSE);
		break;

	case HW_VAR_BCN_VALID:
#ifdef CONFIG_CONCURRENT_MODE
		if (padapter->hw_port == HW_PORT1) {
			/* BCN_VALID, BIT31 of REG_FIFOPAGE_CTRL_2_8814A, write 1 to clear */
			val8 = rtw_read8(padapter, REG_FIFOPAGE_CTRL_2_8814A + 3);
			*pval = (BIT(7) & val8) ? _TRUE : _FALSE;
		} else
#endif /* CONFIG_CONCURRENT_MODE */
		{
			/* BCN_VALID, BIT15 of REG_FIFOPAGE_CTRL_2_8814A, write 1 to clear */
			val8 = rtw_read8(padapter, REG_FIFOPAGE_CTRL_2_8814A + 1);
			*pval = (BIT(7) & val8) ? _TRUE : _FALSE;
		}
		break;

	case HW_VAR_AC_PARAM_VO:
		val32 = rtw_read32(padapter, REG_EDCA_VO_PARAM);
		pval[0] = val32 & 0xFF;
		pval[1] = (val32 >> 8) & 0xFF;
		pval[2] = (val32 >> 16) & 0xFF;
		pval[3] = (val32 >> 24) & 0x07;
		break;

	case HW_VAR_AC_PARAM_VI:
		val32 = rtw_read32(padapter, REG_EDCA_VI_PARAM);
		pval[0] = val32 & 0xFF;
		pval[1] = (val32 >> 8) & 0xFF;
		pval[2] = (val32 >> 16) & 0xFF;
		pval[3] = (val32 >> 24) & 0x07;
		break;

	case HW_VAR_AC_PARAM_BE:
		val32 = rtw_read32(padapter, REG_EDCA_BE_PARAM);
		pval[0] = val32 & 0xFF;
		pval[1] = (val32 >> 8) & 0xFF;
		pval[2] = (val32 >> 16) & 0xFF;
		pval[3] = (val32 >> 24) & 0x07;
		break;

	case HW_VAR_AC_PARAM_BK:
		val32 = rtw_read32(padapter, REG_EDCA_BK_PARAM);
		pval[0] = val32 & 0xFF;
		pval[1] = (val32 >> 8) & 0xFF;
		pval[2] = (val32 >> 16) & 0xFF;
		pval[3] = (val32 >> 24) & 0x07;
		break;

	case HW_VAR_EFUSE_BYTES: /* To get EFUE total used bytes, added by Roger, 2008.12.22. */
		*(u16 *)pval = pHalData->EfuseUsedBytes;
		break;

	case HW_VAR_CHK_HI_QUEUE_EMPTY:
		val16 = rtw_read16(padapter, REG_TXPKT_EMPTY);
		*pval = (val16 & BIT(10)) ? _TRUE : _FALSE;
		break;
	case HW_VAR_CHK_MGQ_CPU_EMPTY:
			val16 = rtw_read16(padapter, REG_TXPKT_EMPTY);
			*pval = (val16 & BIT(8)) ? _TRUE : _FALSE;
			break;
	case HW_VAR_DUMP_MAC_QUEUE_INFO:
		dump_mac_qinfo_8814a(pval, padapter);
		break;

	case HW_VAR_DUMP_MAC_TXFIFO:
		dump_mac_txfifo_8814a(pval, padapter);
		break;

	default:
		GetHwReg(padapter, variable, pval);
		break;
	}

}

/*
 *	Description:
 *		Change default setting of specified variable.
 */
u8 SetHalDefVar8814A(PADAPTER padapter, HAL_DEF_VARIABLE variable, void *pval)
{
	PHAL_DATA_TYPE pHalData;
	u8 bResult;


	pHalData = GET_HAL_DATA(padapter);
	bResult = _SUCCESS;

	switch (variable) {
	case HAL_DEF_EFUSE_BYTES:
		pHalData->EfuseUsedBytes = *((u16 *)pval);
		break;
	case HAL_DEF_EFUSE_USAGE:
		pHalData->EfuseUsedPercentage = *((u8 *)pval);
		break;
	default:
		bResult = SetHalDefVar(padapter, variable, pval);
		break;
	}

	return bResult;
}

/*
 *	Description:
 *		Query setting of specified variable.
 */
u8 GetHalDefVar8814A(PADAPTER padapter, HAL_DEF_VARIABLE variable, void *pval)
{
	PHAL_DATA_TYPE pHalData;
	u8 bResult;


	pHalData = GET_HAL_DATA(padapter);
	bResult = _SUCCESS;

	switch (variable) {


#ifdef CONFIG_ANTENNA_DIVERSITY
	case HAL_DEF_IS_SUPPORT_ANT_DIV:
		*((u8 *)pval) = (pHalData->AntDivCfg == 0) ? _FALSE : _TRUE;
		break;
#endif /* CONFIG_ANTENNA_DIVERSITY */

	case HAL_DEF_DRVINFO_SZ:
		*((u32 *)pval) = DRVINFO_SZ;
		break;

	case HAL_DEF_MAX_RECVBUF_SZ:
		*((u32 *)pval) = MAX_RECVBUF_SZ;
		break;

	case HAL_DEF_RX_PACKET_OFFSET:
		*((u32 *)pval) = RXDESC_SIZE + DRVINFO_SZ * 8;
		break;

	case HW_VAR_MAX_RX_AMPDU_FACTOR:
		*((u32 *)pval) = MAX_AMPDU_FACTOR_64K;
		break;

	case HW_VAR_BEST_AMPDU_DENSITY:
		*((u32 *)pval) = AMPDU_DENSITY_VALUE_4;
		break;

	case HAL_DEF_TX_LDPC:
		*(u8 *)pval = _TRUE;
		break;

	case HAL_DEF_RX_LDPC:
		*(u8 *)pval = _TRUE;
		break;

	case HAL_DEF_RX_STBC:
		*(u8 *)pval = 1;
		break;

	case HAL_DEF_EXPLICIT_BEAMFORMER:
		if (pHalData->rf_type != RF_1T2R && pHalData->rf_type != RF_1T1R)/*1T?R not support mer*/
			*((PBOOLEAN)pval) = _TRUE;
		else
			*((PBOOLEAN)pval) = _FALSE;
		break;
	case HAL_DEF_EXPLICIT_BEAMFORMEE:
		*((PBOOLEAN)pval) = _TRUE;
		break;

	case HW_DEF_RA_INFO_DUMP:
#if 0
		{
			u8 mac_id = *(u8 *)pval;
			u32 cmd ;
			u32 ra_info1, ra_info2;
			u32 rate_mask1, rate_mask2;
			u8 curr_tx_rate, curr_tx_sgi, hight_rate, lowest_rate;

			RTW_INFO("============ RA status check  Mac_id:%d ===================\n", mac_id);

			cmd = 0x40000100 | mac_id;
			rtw_write32(padapter, REG_HMEBOX_E2_E3_8812, cmd);
			rtw_msleep_os(10);
			ra_info1 = rtw_read32(padapter, REG_RSVD5_8812);
			curr_tx_rate = ra_info1 & 0x7F;
			curr_tx_sgi = (ra_info1 >> 7) & 0x01;
			RTW_INFO("[ ra_info1:0x%08x ] =>cur_tx_rate= %s,cur_sgi:%d, PWRSTS = 0x%02x\n",
				 ra_info1,
				 HDATA_RATE(curr_tx_rate),
				 curr_tx_sgi,
				 (ra_info1 >> 8)  & 0x07);

			cmd = 0x40000400 | mac_id;
			rtw_write32(padapter, REG_HMEBOX_E2_E3_8812, cmd);
			rtw_msleep_os(10);
			ra_info1 = rtw_read32(padapter, REG_RSVD5_8812);
			ra_info2 = rtw_read32(padapter, REG_RSVD6_8812);
			rate_mask1 = rtw_read32(padapter, REG_RSVD7_8812);
			rate_mask2 = rtw_read32(padapter, REG_RSVD8_8812);
			hight_rate = ra_info2 & 0xFF;
			lowest_rate = (ra_info2 >> 8)  & 0xFF;
			RTW_INFO("[ ra_info1:0x%08x ] =>RSSI=%d, BW_setting=0x%02x, DISRA=0x%02x, VHT_EN=0x%02x\n",
				 ra_info1,
				 ra_info1 & 0xFF,
				 (ra_info1 >> 8)  & 0xFF,
				 (ra_info1 >> 16) & 0xFF,
				 (ra_info1 >> 24) & 0xFF);

			RTW_INFO("[ ra_info2:0x%08x ] =>hight_rate=%s, lowest_rate=%s, SGI=0x%02x, RateID=%d\n",
				 ra_info2,
				 HDATA_RATE(hight_rate),
				 HDATA_RATE(lowest_rate),
				 (ra_info2 >> 16) & 0xFF,
				 (ra_info2 >> 24) & 0xFF);
			RTW_INFO("rate_mask2=0x%08x, rate_mask1=0x%08x\n", rate_mask2, rate_mask1);
		}
#else /* 0 */
		RTW_INFO("%s,%d, 8814 need to fix\n", __FUNCTION__, __LINE__);
#endif /* 0 */
		break;

	case HAL_DEF_TX_PAGE_SIZE:
		*(u32 *)pval = PAGE_SIZE_128;
		break;

	case HAL_DEF_TX_PAGE_BOUNDARY:
		if (!padapter->registrypriv.wifi_spec)
			*(u16 *)pval = TX_PAGE_BOUNDARY_8814A;
		else
			*(u16 *)pval = WMM_NORMAL_TX_PAGE_BOUNDARY_8814A;
		break;

	case HAL_DEF_TX_PAGE_BOUNDARY_WOWLAN:
		*(u16 *)pval = TX_PAGE_BOUNDARY_WOWLAN_8814A;
		break;
	case HAL_DEF_EFUSE_BYTES:
		*((u16 *)(pval)) = pHalData->EfuseUsedBytes;
		break;
	case HAL_DEF_EFUSE_USAGE:
		*((u32 *)(pval)) = (pHalData->EfuseUsedPercentage << 16) | (pHalData->EfuseUsedBytes);
		break;
	case HAL_DEF_RX_DMA_SZ_WOW:
		*((u32 *)pval) = RX_DMA_BOUNDARY_8814A + 1;
		break;
	case HAL_DEF_RX_DMA_SZ:
		*((u32 *)pval) = RX_DMA_BOUNDARY_8814A + 1;
		break;
	case HAL_DEF_RX_PAGE_SIZE:
		*((u32 *)pval) = 8;
		break;
	default:
		bResult = GetHalDefVar(padapter, variable, pval);
		break;
	}

	return bResult;
}

#ifdef CONFIG_BT_COEXIST
void rtl8814a_combo_card_WifiOnlyHwInit(PADAPTER pdapter)
{
	u8  u1Tmp;
	RTW_INFO("%s !\n", __FUNCTION__);
	if (IS_HARDWARE_TYPE_8812(pdapter)) {
		/* 0x790[5:0]=0x5 */
		u1Tmp = rtw_read8(pdapter, 0x790);
		u1Tmp = (u1Tmp & 0xb0) | 0x05 ;
		rtw_write8(pdapter, 0x790, u1Tmp);
		/* PTA parameter */
		/* pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x6cc, 0x0); */
		/* pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x6c8, 0xffffff); */
		/* pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x6c4, 0x55555555); */
		/* pBtCoexist->fBtcWrite4Byte(pBtCoexist, 0x6c0, 0x55555555); */
		rtw_write8(pdapter, 0x6cc, 0x0);
		rtw_write32(pdapter, 0x6c8, 0xffffff);
		rtw_write32(pdapter, 0x6c4, 0x55555555);
		rtw_write32(pdapter, 0x6c0, 0x55555555);

		/* coex parameters */
		/* pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x778, 0x3); */
		rtw_write8(pdapter, 0x778, 0x3);

		/* enable counter statistics */
		/* pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x76e, 0xc); */
		rtw_write8(pdapter, 0x76e, 0xc);

		/* enable PTA */
		/* pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x40, 0x20); */
		rtw_write8(pdapter, 0x40, 0x20);

		/* bt clock related */
		/* u1Tmp = pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x4); */
		/* u1Tmp |= BIT7; */
		/* pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x4, u1Tmp); */
		u1Tmp = rtw_read8(pdapter, 0x4);
		u1Tmp |= BIT7;
		rtw_write8(pdapter, 0x4, u1Tmp);

		/* bt clock related */
		/* u1Tmp = pBtCoexist->fBtcRead1Byte(pBtCoexist, 0x7); */
		/* u1Tmp |= BIT1; */
		/* pBtCoexist->fBtcWrite1Byte(pBtCoexist, 0x7, u1Tmp); */
		u1Tmp = rtw_read8(pdapter, 0x7);
		u1Tmp |= BIT1;
		rtw_write8(pdapter, 0x7, u1Tmp);
	}


}
#endif /* CONFIG_BT_COEXIST */

void rtl8814_set_hal_ops(struct hal_ops *pHalFunc)
{
	pHalFunc->dm_init = &rtl8814_init_dm_priv;
	pHalFunc->dm_deinit = &rtl8814_deinit_dm_priv;

	pHalFunc->SetBeaconRelatedRegistersHandler = &SetBeaconRelatedRegisters8814A;

	pHalFunc->read_chip_version = read_chip_version_8814a;

	pHalFunc->set_chnl_bw_handler = &PHY_SetSwChnlBWMode8814;

	pHalFunc->set_tx_power_level_handler = &PHY_SetTxPowerLevel8814;
	pHalFunc->set_tx_power_index_handler = PHY_SetTxPowerIndex_8814A;
	pHalFunc->get_tx_power_index_handler = &PHY_GetTxPowerIndex8814A;

	pHalFunc->hal_dm_watchdog = &rtl8814_HalDmWatchDog;

	pHalFunc->run_thread = &rtl8814_start_thread;
	pHalFunc->cancel_thread = &rtl8814_stop_thread;

	pHalFunc->read_bbreg = &PHY_QueryBBReg8814A;
	pHalFunc->write_bbreg = &PHY_SetBBReg8814A;
	pHalFunc->read_rfreg = &PHY_QueryRFReg8814A;
	pHalFunc->write_rfreg = &PHY_SetRFReg8814A;

	pHalFunc->read_wmmedca_reg = &rtl8814a_read_wmmedca_reg;

	/* Efuse related function */
	pHalFunc->EfusePowerSwitch = &rtl8814_EfusePowerSwitch;
	pHalFunc->ReadEFuse = &rtl8814_ReadEFuse;
	pHalFunc->EFUSEGetEfuseDefinition = &rtl8814_EFUSE_GetEfuseDefinition;
	pHalFunc->EfuseGetCurrentSize = &rtl8814_EfuseGetCurrentSize;
	pHalFunc->Efuse_PgPacketRead = &rtl8814_Efuse_PgPacketRead;
	pHalFunc->Efuse_PgPacketWrite = &rtl8814_Efuse_PgPacketWrite;
	pHalFunc->Efuse_WordEnableDataWrite = &rtl8814_Efuse_WordEnableDataWrite;

#ifdef DBG_CONFIG_ERROR_DETECT
	pHalFunc->sreset_init_value = &sreset_init_value;
	pHalFunc->sreset_reset_value = &sreset_reset_value;
	pHalFunc->silentreset = &sreset_reset;
	pHalFunc->sreset_xmit_status_check = &rtl8814_sreset_xmit_status_check;
	pHalFunc->sreset_linked_status_check  = &rtl8814_sreset_linked_status_check;
	pHalFunc->sreset_get_wifi_status  = &sreset_get_wifi_status;
	pHalFunc->sreset_inprogress = &sreset_inprogress;
#endif /* DBG_CONFIG_ERROR_DETECT */

	pHalFunc->GetHalODMVarHandler = GetHalODMVar;
	pHalFunc->SetHalODMVarHandler = SetHalODMVar;
	pHalFunc->hal_notch_filter = &hal_notch_filter_8814;

	pHalFunc->c2h_handler = c2h_handler_8814a;
	pHalFunc->reqtxrpt = &rtl8814_req_txrpt_cmd;

	pHalFunc->fill_h2c_cmd = &FillH2CCmd_8814;
	pHalFunc->fill_fake_txdesc = &rtl8814a_fill_fake_txdesc;
	pHalFunc->fw_dl = &FirmwareDownload8814A;
	pHalFunc->hal_get_tx_buff_rsvd_page_num = &GetTxBufferRsvdPageNum8814;
}
