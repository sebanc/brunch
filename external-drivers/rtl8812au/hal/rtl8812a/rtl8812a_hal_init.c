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
#define _RTL8812A_HAL_INIT_C_

/* #include <drv_types.h> */
#include <rtl8812a_hal.h>
#ifdef CONFIG_RTL8812A
#include "hal8812a_fw.h"
#endif
#ifdef CONFIG_RTL8821A
#include "hal8821a_fw.h"
#endif
/* -------------------------------------------------------------------------
 *
 * LLT R/W/Init function
 *
 * ------------------------------------------------------------------------- */
s32
_LLTWrite_8812A(
	IN	PADAPTER	Adapter,
	IN	u32			address,
	IN	u32			data
)
{
	u32	status = _SUCCESS;
	s32	count = 0;
	u32	value = _LLT_INIT_ADDR(address) | _LLT_INIT_DATA(data) | _LLT_OP(_LLT_WRITE_ACCESS);

	rtw_write32(Adapter, REG_LLT_INIT, value);

	/* polling */
	do {
		value = rtw_read32(Adapter, REG_LLT_INIT);
		if (_LLT_NO_ACTIVE == _LLT_OP_VALUE(value))
			break;

		if (count > POLLING_LLT_THRESHOLD) {
			status = _FAIL;
			break;
		}
	} while (++count);

	return status;
}

static u8
_LLTRead_8812A(
	IN	PADAPTER	Adapter,
	IN	u32			address
)
{
	u32	count = 0;
	u32	value = _LLT_INIT_ADDR(address) | _LLT_OP(_LLT_READ_ACCESS);
	u16	LLTReg = REG_LLT_INIT;


	rtw_write32(Adapter, LLTReg, value);

	/* polling and get value */
	do {
		value = rtw_read32(Adapter, LLTReg);
		if (_LLT_NO_ACTIVE == _LLT_OP_VALUE(value))
			return (u8)value;

		if (count > POLLING_LLT_THRESHOLD) {
			break;
		}
	} while (++count);

	return 0xFF;
}

s32 InitLLTTable8812A(PADAPTER padapter, u8 txpktbuf_bndy)
{
	s32	status = _FAIL;
	u32	i;
	u32	Last_Entry_Of_TxPktBuf = LAST_ENTRY_OF_TX_PKT_BUFFER_8812;
	HAL_DATA_TYPE *pHalData	= GET_HAL_DATA(padapter);

	for (i = 0; i < (txpktbuf_bndy - 1); i++) {
		status = _LLTWrite_8812A(padapter, i, i + 1);
		if (_SUCCESS != status)
			return status;
	}

	/* end of list */
	status = _LLTWrite_8812A(padapter, (txpktbuf_bndy - 1), 0xFF);
	if (_SUCCESS != status)
		return status;

	/* Make the other pages as ring buffer */
	/* This ring buffer is used as beacon buffer if we config this MAC as two MAC transfer. */
	/* Otherwise used as local loopback buffer. */
	for (i = txpktbuf_bndy; i < Last_Entry_Of_TxPktBuf; i++) {
		status = _LLTWrite_8812A(padapter, i, (i + 1));
		if (_SUCCESS != status)
			return status;
	}

	/* Let last entry point to the start entry of ring buffer */
	status = _LLTWrite_8812A(padapter, Last_Entry_Of_TxPktBuf, txpktbuf_bndy);
	if (_SUCCESS != status)
		return status;

	return status;
}

BOOLEAN HalDetectPwrDownMode8812(PADAPTER Adapter)
{
	u8 tmpvalue = 0;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(Adapter);

	EFUSE_ShadowRead(Adapter, 1, EEPROM_RF_FEATURE_OPTION_8812, (u32 *)&tmpvalue);

	/* 2010/08/25 MH INF priority > PDN Efuse value. */
	if (tmpvalue & BIT(4) && pwrctrlpriv->reg_pdnmode)
		pHalData->pwrdown = _TRUE;
	else
		pHalData->pwrdown = _FALSE;

	RTW_INFO("HalDetectPwrDownMode(): PDN=%d\n", pHalData->pwrdown);

	return pHalData->pwrdown;
}	/* HalDetectPwrDownMode */

#if defined(CONFIG_WOWLAN) || defined(CONFIG_AP_WOWLAN)
void Hal_DetectWoWMode(PADAPTER pAdapter)
{
	adapter_to_pwrctl(pAdapter)->bSupportRemoteWakeup = _TRUE;
}
#endif

static VOID
_FWDownloadEnable_8812(
	IN	PADAPTER		padapter,
	IN	BOOLEAN			enable
)
{
	u8	tmp;

	if (enable) {
		/* MCU firmware download enable. */
		tmp = rtw_read8(padapter, REG_MCUFWDL);
		rtw_write8(padapter, REG_MCUFWDL, tmp | 0x01);

		/* 8051 reset */
		tmp = rtw_read8(padapter, REG_MCUFWDL + 2);
		rtw_write8(padapter, REG_MCUFWDL + 2, tmp & 0xf7);
	} else {

		/* MCU firmware download disable. */
		tmp = rtw_read8(padapter, REG_MCUFWDL);
		rtw_write8(padapter, REG_MCUFWDL, tmp & 0xfe);
	}
}
#define MAX_REG_BOLCK_SIZE	196
static int
_BlockWrite_8812(
	IN		PADAPTER		padapter,
	IN		PVOID		buffer,
	IN		u32			buffSize
)
{
	int ret = _SUCCESS;

	u32			blockSize_p1 = 4;	/* (Default) Phase #1 : PCI muse use 4-byte write to download FW */
	u32			blockSize_p2 = 8;	/* Phase #2 : Use 8-byte, if Phase#1 use big size to write FW. */
	u32			blockSize_p3 = 1;	/* Phase #3 : Use 1-byte, the remnant of FW image. */
	u32			blockCount_p1 = 0, blockCount_p2 = 0, blockCount_p3 = 0;
	u32			remainSize_p1 = 0, remainSize_p2 = 0;
	u8			*bufferPtr	= (u8 *)buffer;
	u32			i = 0, offset = 0;
#ifdef CONFIG_PCI_HCI
	u8			remainFW[4] = {0, 0, 0, 0};
	u8			*p = NULL;
#endif

#ifdef CONFIG_USB_HCI
	blockSize_p1 = MAX_REG_BOLCK_SIZE;
#endif

	/* 3 Phase #1 */
	blockCount_p1 = buffSize / blockSize_p1;
	remainSize_p1 = buffSize % blockSize_p1;



	for (i = 0; i < blockCount_p1; i++) {
#ifdef CONFIG_USB_HCI
		ret = rtw_writeN(padapter, (FW_START_ADDRESS + i * blockSize_p1), blockSize_p1, (bufferPtr + i * blockSize_p1));
#else
		ret = rtw_write32(padapter, (FW_START_ADDRESS + i * blockSize_p1), le32_to_cpu(*((u32 *)(bufferPtr + i * blockSize_p1))));
#endif

		if (ret == _FAIL)
			goto exit;
	}

#ifdef CONFIG_PCI_HCI
	p = (u8 *)((u32 *)(bufferPtr + blockCount_p1 * blockSize_p1));
	if (remainSize_p1) {
		switch (remainSize_p1) {
		case 0:
			break;
		case 3:
			remainFW[2] = *(p + 2);
		case 2:
			remainFW[1] = *(p + 1);
		case 1:
			remainFW[0] = *(p);
			ret = rtw_write32(padapter, (FW_START_ADDRESS + blockCount_p1 * blockSize_p1),
					  le32_to_cpu(*(u32 *)remainFW));
		}
		return ret;
	}
#endif

	/* 3 Phase #2 */
	if (remainSize_p1) {
		offset = blockCount_p1 * blockSize_p1;

		blockCount_p2 = remainSize_p1 / blockSize_p2;
		remainSize_p2 = remainSize_p1 % blockSize_p2;



#ifdef CONFIG_USB_HCI
		for (i = 0; i < blockCount_p2; i++) {
			ret = rtw_writeN(padapter, (FW_START_ADDRESS + offset + i * blockSize_p2), blockSize_p2, (bufferPtr + offset + i * blockSize_p2));

			if (ret == _FAIL)
				goto exit;
		}
#endif
	}

	/* 3 Phase #3 */
	if (remainSize_p2) {
		offset = (blockCount_p1 * blockSize_p1) + (blockCount_p2 * blockSize_p2);

		blockCount_p3 = remainSize_p2 / blockSize_p3;


		for (i = 0 ; i < blockCount_p3 ; i++) {
			ret = rtw_write8(padapter, (FW_START_ADDRESS + offset + i), *(bufferPtr + offset + i));

			if (ret == _FAIL)
				goto exit;
		}
	}

exit:
	return ret;
}

static int
_PageWrite_8812(
	IN		PADAPTER	padapter,
	IN		u32			page,
	IN		PVOID		buffer,
	IN		u32			size
)
{
	u8 value8;
	u8 u8Page = (u8)(page & 0x07) ;

	value8 = (rtw_read8(padapter, REG_MCUFWDL + 2) & 0xF8) | u8Page ;
	rtw_write8(padapter, REG_MCUFWDL + 2, value8);

	return _BlockWrite_8812(padapter, buffer, size);
}

static VOID
_FillDummy_8812(
	u8		*pFwBuf,
	u32	*pFwLen
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

static int
_WriteFW_8812(
	IN		PADAPTER		padapter,
	IN		PVOID			buffer,
	IN		u32			size
)
{
	/* Since we need dynamic decide method of dwonload fw, so we call this function to get chip version. */
	int	ret = _SUCCESS;
	u32	pageNums, remainSize ;
	u32	page, offset;
	u8	*bufferPtr = (u8 *)buffer;

#ifdef CONFIG_PCI_HCI
	/* 20100120 Joseph: Add for 88CE normal chip. */
	/* Fill in zero to make firmware image to dword alignment.
	*		_FillDummy(bufferPtr, &size); */
#endif

	pageNums = size / MAX_DLFW_PAGE_SIZE ;
	/* RT_ASSERT((pageNums <= 4), ("Page numbers should not greater then 4\n")); */
	remainSize = size % MAX_DLFW_PAGE_SIZE;

	for (page = 0; page < pageNums; page++) {
		offset = page * MAX_DLFW_PAGE_SIZE;
		ret = _PageWrite_8812(padapter, page, bufferPtr + offset, MAX_DLFW_PAGE_SIZE);

		if (ret == _FAIL)
			goto exit;
	}
	if (remainSize) {
		offset = pageNums * MAX_DLFW_PAGE_SIZE;
		page = pageNums;
		ret = _PageWrite_8812(padapter, page, bufferPtr + offset, remainSize);

		if (ret == _FAIL)
			goto exit;

	}

exit:
	return ret;
}

void _8051Reset8812(PADAPTER padapter)
{
	u8 u1bTmp, u1bTmp2;

	/* Reset MCU IO Wrapper- sugggest by SD1-Gimmy */
	if (IS_HARDWARE_TYPE_8812(padapter)) {
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL);
		rtw_write8(padapter, REG_RSV_CTRL, u1bTmp2 & (~BIT1));
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL + 1);
		rtw_write8(padapter, REG_RSV_CTRL + 1, u1bTmp2 & (~BIT3));
	} else if (IS_HARDWARE_TYPE_8821(padapter)) {
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL);
		rtw_write8(padapter, REG_RSV_CTRL, u1bTmp2 & (~BIT1));
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL + 1);
		rtw_write8(padapter, REG_RSV_CTRL + 1, u1bTmp2 & (~BIT0));
	}

	u1bTmp = rtw_read8(padapter, REG_SYS_FUNC_EN + 1);
	rtw_write8(padapter, REG_SYS_FUNC_EN + 1, u1bTmp & (~BIT2));

	/* Enable MCU IO Wrapper */
	if (IS_HARDWARE_TYPE_8812(padapter)) {
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL);
		rtw_write8(padapter, REG_RSV_CTRL, u1bTmp2 & (~BIT1));
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL + 1);
		rtw_write8(padapter, REG_RSV_CTRL + 1, u1bTmp2 | (BIT3));
	} else if (IS_HARDWARE_TYPE_8821(padapter)) {
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL);
		rtw_write8(padapter, REG_RSV_CTRL, u1bTmp2 & (~BIT1));
		u1bTmp2 = rtw_read8(padapter, REG_RSV_CTRL + 1);
		rtw_write8(padapter, REG_RSV_CTRL + 1, u1bTmp2 | (BIT0));
	}

	rtw_write8(padapter, REG_SYS_FUNC_EN + 1, u1bTmp | (BIT2));

	RTW_INFO("=====> _8051Reset8812(): 8051 reset success .\n");
}

static s32 polling_fwdl_chksum(_adapter *adapter, u32 min_cnt, u32 timeout_ms)
{
	s32 ret = _FAIL;
	u32 value32;
	systime start = rtw_get_current_time();
	u32 cnt = 0;

	/* polling CheckSum report */
	do {
		cnt++;
		value32 = rtw_read32(adapter, REG_MCUFWDL);
		if (value32 & FWDL_ChkSum_rpt || RTW_CANNOT_IO(adapter))
			break;
		rtw_yield_os();
	} while (rtw_get_passing_time_ms(start) < timeout_ms || cnt < min_cnt);

	if (!(value32 & FWDL_ChkSum_rpt))
		goto exit;

	if (rtw_fwdl_test_trigger_chksum_fail())
		goto exit;

	ret = _SUCCESS;

exit:
	RTW_INFO("%s: Checksum report %s! (%u, %dms), REG_MCUFWDL:0x%08x\n", __FUNCTION__
		, (ret == _SUCCESS) ? "OK" : "Fail", cnt, rtw_get_passing_time_ms(start), value32);

	return ret;
}

static s32 _FWFreeToGo8812(_adapter *adapter, u32 min_cnt, u32 timeout_ms)
{
	s32 ret = _FAIL;
	u32	value32;
	systime start = rtw_get_current_time();
	u32 cnt = 0;

	value32 = rtw_read32(adapter, REG_MCUFWDL);
	value32 |= MCUFWDL_RDY;
	value32 &= ~WINTINI_RDY;
	rtw_write32(adapter, REG_MCUFWDL, value32);

	_8051Reset8812(adapter);

	/*  polling for FW ready */
	do {
		cnt++;
		value32 = rtw_read32(adapter, REG_MCUFWDL);
		if (value32 & WINTINI_RDY || RTW_CANNOT_IO(adapter))
			break;
		rtw_yield_os();
	} while (rtw_get_passing_time_ms(start) < timeout_ms || cnt < min_cnt);

	if (!(value32 & WINTINI_RDY))
		goto exit;

	if (rtw_fwdl_test_trigger_wintint_rdy_fail())
		goto exit;

	ret = _SUCCESS;

exit:
	RTW_INFO("%s: Polling FW ready %s! (%u, %dms), REG_MCUFWDL:0x%08x\n", __FUNCTION__
		, (ret == _SUCCESS) ? "OK" : "Fail", cnt, rtw_get_passing_time_ms(start), value32);

	return ret;
}

#ifdef CONFIG_FILE_FWIMG
	u8 FwBuffer8812[FW_SIZE_8812];
	u8 FwBuffer[FW_SIZE_8812];
#endif /* CONFIG_FILE_FWIMG */

s32
FirmwareDownload8812(
	IN	PADAPTER			Adapter,
	IN	BOOLEAN			bUsedWoWLANFw
)
{
	s32	rtStatus = _SUCCESS;
	u8	write_fw = 0;
	systime fwdl_start_time;
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(Adapter);
	u8				*pFwImageFileName;
	u8				*pucMappedFile = NULL;
	PRT_FIRMWARE_8812	pFirmware = NULL;
	u8				*pFwHdr = NULL;
	u8				*pFirmwareBuf;
	u32				FirmwareLen;


	pFirmware = (PRT_FIRMWARE_8812)rtw_zmalloc(sizeof(RT_FIRMWARE_8812));
	if (!pFirmware) {
		rtStatus = _FAIL;
		goto exit;
	}

#ifdef CONFIG_FILE_FWIMG
#ifdef CONFIG_WOWLAN
	if (bUsedWoWLANFw && rtw_is_file_readable(rtw_fw_wow_file_path) == _TRUE) {
		RTW_INFO("%s acquire FW from file:%s\n", __FUNCTION__, rtw_fw_wow_file_path);
		pFirmware->eFWSource = FW_SOURCE_IMG_FILE;
	} else
#endif
		if (rtw_is_file_readable(rtw_fw_file_path) == _TRUE) {
			RTW_INFO("%s acquire FW from file:%s\n", __FUNCTION__, rtw_fw_file_path);
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
#ifdef CONFIG_WOWLAN
		if (bUsedWoWLANFw)
			rtStatus = rtw_retrieve_from_file(rtw_fw_wow_file_path, FwBuffer8812, FW_SIZE_8812);
		else
#endif
			rtStatus = rtw_retrieve_from_file(rtw_fw_file_path, FwBuffer8812, FW_SIZE_8812);
		pFirmware->ulFwLength = rtStatus >= 0 ? rtStatus : 0;
		pFirmware->szFwBuffer = FwBuffer8812;
#endif /* CONFIG_FILE_FWIMG */
		break;
	case FW_SOURCE_HEADER_FILE:
		if (bUsedWoWLANFw) {
		#ifdef CONFIG_WOWLAN
			if (pwrpriv->wowlan_mode) {
#ifdef CONFIG_RTL8812A
				if (IS_HARDWARE_TYPE_8812(Adapter)) {
				pFirmware->szFwBuffer = array_mp_8812a_fw_wowlan;
				pFirmware->ulFwLength = array_length_mp_8812a_fw_wowlan;
				}
#endif
#ifdef CONFIG_RTL8821A
				if (IS_HARDWARE_TYPE_8821(Adapter)) {
				pFirmware->szFwBuffer = array_mp_8821a_fw_wowlan;
				pFirmware->ulFwLength = array_length_mp_8821a_fw_wowlan;
				}
#endif
				RTW_INFO("%s fw:%s, size: %d\n", __func__, "WoWLAN", pFirmware->ulFwLength);

			}
		#endif /* CONFIG_WOWLAN */

		#ifdef CONFIG_AP_WOWLAN
			if (pwrpriv->wowlan_ap_mode) {
#ifdef CONFIG_RTL8812A
				if (IS_HARDWARE_TYPE_8812(Adapter)) {
				pFirmware->szFwBuffer = array_mp_8812a_fw_ap;
				pFirmware->ulFwLength = array_length_mp_8812a_fw_ap;
				}
#endif
#ifdef CONFIG_RTL8821A
				if (IS_HARDWARE_TYPE_8821(Adapter)) {
				pFirmware->szFwBuffer = array_mp_8821a_fw_ap;
				pFirmware->ulFwLength = array_length_mp_8821a_fw_ap;
				}
#endif

				RTW_INFO("%s fw: %s, size: %d\n", __func__, "AP_WoWLAN", pFirmware->ulFwLength);
			}
		#endif /* CONFIG_AP_WOWLAN */
		} else {
#ifdef CONFIG_BT_COEXIST
			if (pHalData->EEPROMBluetoothCoexist == _TRUE) {

#ifdef CONFIG_RTL8812A
				if (IS_HARDWARE_TYPE_8812(pAdapter)) {
				pFirmware->szFwBuffer = array_mp_8812a_fw_nic_bt;
				pFirmware->ulFwLength = array_length_mp_8812a_fw_nic_bt;
				}
#endif
#ifdef CONFIG_RTL8821A
				if (IS_HARDWARE_TYPE_8821(pAdapter)) {
				pFirmware->szFwBuffer = array_mp_8821a_fw_nic_bt;
				pFirmware->ulFwLength = array_length_mp_8821a_fw_nic_bt;
				}
#endif

				RTW_INFO("%s fw:%s, size: %d\n", __FUNCTION__, "NIC-BTCOEX", pFirmware->ulFwLength);
			} else
#endif /* CONFIG_BT_COEXIST */
			{

#ifdef CONFIG_RTL8812A
				if (IS_HARDWARE_TYPE_8812(Adapter)) {
				pFirmware->szFwBuffer = array_mp_8812a_fw_nic;
				pFirmware->ulFwLength = array_length_mp_8812a_fw_nic;
				}
#endif
#ifdef CONFIG_RTL8821A
				if (IS_HARDWARE_TYPE_8821(Adapter)) {
				pFirmware->szFwBuffer = array_mp_8821a_fw_nic;
				pFirmware->ulFwLength = array_length_mp_8821a_fw_nic;
				}
#endif

				RTW_INFO("%s fw:%s, size: %d\n", __FUNCTION__, "NIC", pFirmware->ulFwLength);
			}
		}
		break;
	}

	if ((pFirmware->ulFwLength - 32) > FW_SIZE_8812) {
		rtStatus = _FAIL;
		RTW_ERR("Firmware size:%u exceed %u\n", pFirmware->ulFwLength, FW_SIZE_8812);
		goto exit;
	}

	pFirmwareBuf = pFirmware->szFwBuffer;
	FirmwareLen = pFirmware->ulFwLength;
	pFwHdr = (u8 *)pFirmware->szFwBuffer;

	pHalData->firmware_version = (u16)GET_FIRMWARE_HDR_VERSION_8812(pFwHdr);
	pHalData->firmware_sub_version = (u16)GET_FIRMWARE_HDR_SUB_VER_8812(pFwHdr);
	pHalData->FirmwareSignature = (u16)GET_FIRMWARE_HDR_SIGNATURE_8812(pFwHdr);

	RTW_INFO("%s: fw_ver=%d fw_subver=%d sig=0x%x\n",
		__FUNCTION__, pHalData->firmware_version, pHalData->firmware_sub_version, pHalData->FirmwareSignature);

	if (IS_FW_HEADER_EXIST_8812(pFwHdr) || IS_FW_HEADER_EXIST_8821(pFwHdr)) {
		/* Shift 32 bytes for FW header */
		pFirmwareBuf = pFirmwareBuf + 32;
		FirmwareLen = FirmwareLen - 32;
	}

	/* Suggested by Filen. If 8051 is running in RAM code, driver should inform Fw to reset by itself, */
	/* or it will cause download Fw fail. 2010.02.01. by tynli. */
	if (rtw_read8(Adapter, REG_MCUFWDL) & BIT7) { /* 8051 RAM code */
		rtw_write8(Adapter, REG_MCUFWDL, 0x00);
		_8051Reset8812(Adapter);
	}

	_FWDownloadEnable_8812(Adapter, _TRUE);
	fwdl_start_time = rtw_get_current_time();
	while (!RTW_CANNOT_IO(Adapter)
	       && (write_fw++ < 3 || rtw_get_passing_time_ms(fwdl_start_time) < 500)) {
		/* reset FWDL chksum */
		rtw_write8(Adapter, REG_MCUFWDL, rtw_read8(Adapter, REG_MCUFWDL) | FWDL_ChkSum_rpt);

		rtStatus = _WriteFW_8812(Adapter, pFirmwareBuf, FirmwareLen);
		if (rtStatus != _SUCCESS)
			continue;

		rtStatus = polling_fwdl_chksum(Adapter, 5, 50);
		if (rtStatus == _SUCCESS)
			break;
	}
	_FWDownloadEnable_8812(Adapter, _FALSE);
	if (_SUCCESS != rtStatus)
		goto fwdl_stat;

	rtStatus = _FWFreeToGo8812(Adapter, 10, 200);
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
		rtw_mfree((u8 *)pFirmware, sizeof(RT_FIRMWARE_8812));

	InitializeFirmwareVars8812(Adapter);

	return rtStatus;
}


void InitializeFirmwareVars8812(PADAPTER padapter)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);

	/* Init Fw LPS related. */
	pwrpriv->bFwCurrentInPSMode = _FALSE;

	/* Init H2C cmd. */
	rtw_write8(padapter, REG_HMETFR, 0x0f);

	/* Init H2C counter. by tynli. 2009.12.09. */
	pHalData->LastHMEBoxNum = 0;
}


/*
 * Description: Determine the contents of H2C BT_FW_PATCH Command sent to FW.
 * 2013.01.23 by tynli
 * Porting from 8723B. 2013.04.01
 *   */
VOID
SetFwBTFwPatchCmd_8821(
	IN PADAPTER Adapter,
	IN u2Byte		FwSize
)
{
	u1Byte		u1BTFwPatchParm[6] = {0};

	RTW_INFO("SetFwBTFwPatchCmd_8821(): FwSize = %d\n", FwSize);

	/* SET_8812_H2CCMD_BT_FW_PATCH_ENABLE(u1BTFwPatchParm, 1); */
	SET_8812_H2CCMD_BT_FW_PATCH_SIZE(u1BTFwPatchParm, FwSize);
	SET_8812_H2CCMD_BT_FW_PATCH_ADDR0(u1BTFwPatchParm, 0);
	SET_8812_H2CCMD_BT_FW_PATCH_ADDR1(u1BTFwPatchParm, 0xa0);
	SET_8812_H2CCMD_BT_FW_PATCH_ADDR2(u1BTFwPatchParm, 0x10);
	SET_8812_H2CCMD_BT_FW_PATCH_ADDR3(u1BTFwPatchParm, 0x80);

	fill_h2c_cmd_8812(Adapter, H2C_8812_BT_FW_PATCH, 6 , u1BTFwPatchParm);
}

#ifdef CONFIG_MP_INCLUDED
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
			rtw_msleep_os(10); /* 10ms */
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
		rtw_msleep_os(10); /* 10ms */
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
#endif

/* ***********************************************************
 *				Efuse related code
 * *********************************************************** */
VOID
Hal_EfuseParseBTCoexistInfo8812A(
	IN PADAPTER			Adapter,
	IN u8				*hwinfo,
	IN BOOLEAN			AutoLoadFail
)
{
	struct registry_priv *regsty = &Adapter->registrypriv;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	u8 tmp_u8;
	u32 tmp_u32;

	if (IS_HARDWARE_TYPE_8812(Adapter)) {

		pHalData->EEPROMBluetoothType = BT_RTL8812A;

		if (!AutoLoadFail) {
			tmp_u8 = hwinfo[EEPROM_RF_BOARD_OPTION_8812];
			if (((tmp_u8 & 0xe0) >> 5) == 0x1) /* [7:5] */
				pHalData->EEPROMBluetoothCoexist = _TRUE;
			else
				pHalData->EEPROMBluetoothCoexist = _FALSE;

			tmp_u8 = hwinfo[EEPROM_RF_BT_SETTING_8812];
			pHalData->EEPROMBluetoothAntNum = (tmp_u8 & 0x1); /* bit [0] */
		} else {
			pHalData->EEPROMBluetoothCoexist = _FALSE;
			pHalData->EEPROMBluetoothAntNum = Ant_x1;
		}
	} else if (IS_HARDWARE_TYPE_8821(Adapter)) {

		pHalData->EEPROMBluetoothType = BT_RTL8821;

		if (!AutoLoadFail) {
			tmp_u32 = rtw_read32(Adapter, REG_MULTI_FUNC_CTRL);
			if (tmp_u32 & BT_FUNC_EN)
				pHalData->EEPROMBluetoothCoexist = _TRUE;
			else
				pHalData->EEPROMBluetoothCoexist = _FALSE;

			tmp_u8 = hwinfo[EEPROM_RF_BT_SETTING_8821];
			pHalData->EEPROMBluetoothAntNum = (tmp_u8 & 0x1); /* bit [0] */
		} else {
			pHalData->EEPROMBluetoothCoexist = _FALSE;
			pHalData->EEPROMBluetoothAntNum = Ant_x2;
		}

#ifdef CONFIG_BTCOEX_FORCE_CSR
		pHalData->EEPROMBluetoothType = BT_CSR_BC8;
		pHalData->EEPROMBluetoothCoexist = _TRUE;
		pHalData->EEPROMBluetoothAntNum = Ant_x2;
#endif
	} else
		rtw_warn_on(1);

#ifdef CONFIG_BT_COEXIST
	if (_TRUE == pHalData->EEPROMBluetoothCoexist && IS_HARDWARE_TYPE_8812(Adapter)) {
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
		if (!hal_btcoex_AntIsolationConfig_ParaFile(Adapter , PHY_FILE_WIFI_ANT_ISOLATION)) {
#endif
			pHalData->EEPROMBluetoothCoexist = _TRUE;
			hal_btcoex_SetAntIsolationType(Adapter, 0);

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
		}
#endif
	}

	if (regsty->ant_num != 0) {
		if (regsty->ant_num == 1)
			pHalData->EEPROMBluetoothAntNum = Ant_x1;
		else if (regsty->ant_num == 2)
			pHalData->EEPROMBluetoothAntNum = Ant_x2;
		else
			RTW_WARN("%s: Discard invalid driver defined antenna number(%d)!\n"
				 , __func__, regsty->ant_num);
	}

	if (regsty->single_ant_path == 0)
		pHalData->ant_path = 0;
	else if (regsty->single_ant_path == 1)
		pHalData->ant_path = 1;
	else
		RTW_WARN("%s: Discard invalid driver defined antenna path(%d)!\n"
				 , __func__, regsty->single_ant_path);


	if (regsty->btcoex != 2)
		pHalData->EEPROMBluetoothCoexist = regsty->btcoex ? _TRUE : _FALSE;

	/*Add by YiWei , btcoex 1 ant module , ant band switch by btcoex , driver have to disable  ant band switch feature*/
	if (pHalData->EEPROMBluetoothCoexist == 1 && pHalData->EEPROMBluetoothAntNum == Ant_x1)
		regsty->drv_ant_band_switch = 0;

	RTW_INFO("%s: BTCoexist=%s, AntNum=%d, AntPath=%d\n", __func__, pHalData->EEPROMBluetoothCoexist == _TRUE ? "Enable" : "Disable"
		 , pHalData->EEPROMBluetoothAntNum == Ant_x2 ? 2 : 1, pHalData->ant_path);
#endif /* CONFIG_BT_COEXIST */
}

void
Hal_EfuseParseIDCode8812A(
	IN	PADAPTER	padapter,
	IN	u8			*hwinfo
)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);
	u16			EEPROMId;


	/* Checl 0x8129 again for making sure autoload status!! */
	EEPROMId = ReadLE2Byte(&hwinfo[0]);
	if (EEPROMId != RTL_EEPROM_ID) {
		RTW_INFO("EEPROM ID(%#x) is invalid!!\n", EEPROMId);
		pHalData->bautoload_fail_flag = _TRUE;
	} else
		pHalData->bautoload_fail_flag = _FALSE;

	RTW_INFO("EEPROM ID=0x%04x\n", EEPROMId);
}

VOID
Hal_ReadPROMVersion8812A(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN	AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (AutoloadFail)
		pHalData->EEPROMVersion = EEPROM_Default_Version;
	else {
		if (IS_HARDWARE_TYPE_8812(Adapter))
			pHalData->EEPROMVersion = *(u8 *)&PROMContent[EEPROM_VERSION_8812];
		else
			pHalData->EEPROMVersion = *(u8 *)&PROMContent[EEPROM_VERSION_8821];
		if (pHalData->EEPROMVersion == 0xFF)
			pHalData->EEPROMVersion = EEPROM_Default_Version;
	}
	/* RTW_INFO("pHalData->EEPROMVersion is 0x%x\n", pHalData->EEPROMVersion); */
}

void
Hal_ReadTxPowerInfo8812A(
	IN	PADAPTER		Adapter,
	IN	u8				*PROMContent,
	IN	BOOLEAN			AutoLoadFail
)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	TxPowerInfo24G pwrInfo24G;
	TxPowerInfo5G pwrInfo5G;

	hal_load_txpwr_info(Adapter, &pwrInfo24G, &pwrInfo5G, PROMContent);

	/* 2010/10/19 MH Add Regulator recognize for CU. */
	if (!AutoLoadFail) {
		struct registry_priv  *registry_par = &Adapter->registrypriv;


		if (PROMContent[EEPROM_RF_BOARD_OPTION_8812] == 0xFF)
			pHalData->EEPROMRegulatory = (EEPROM_DEFAULT_BOARD_OPTION & 0x7);	/* bit0~2 */
		else
			pHalData->EEPROMRegulatory = (PROMContent[EEPROM_RF_BOARD_OPTION_8812] & 0x7);	/* bit0~2 */

	} else
		pHalData->EEPROMRegulatory = 0;
	RTW_INFO("EEPROMRegulatory = 0x%x\n", pHalData->EEPROMRegulatory);

}

VOID
Hal_ReadBoardType8812A(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		pHalData->InterfaceSel = (PROMContent[EEPROM_RF_BOARD_OPTION_8812] & 0xE0) >> 5;
		if (PROMContent[EEPROM_RF_BOARD_OPTION_8812] == 0xFF)
			pHalData->InterfaceSel = (EEPROM_DEFAULT_BOARD_OPTION & 0xE0) >> 5;
	} else
		pHalData->InterfaceSel = 0;
	RTW_INFO("Board Type: 0x%2x\n", pHalData->InterfaceSel);

}

VOID
Hal_ReadThermalMeter_8812A(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN	AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	/* u8	tempval; */

	/*  */
	/* ThermalMeter from EEPROM */
	/*  */
	if (!AutoloadFail)
		pHalData->eeprom_thermal_meter = PROMContent[EEPROM_THERMAL_METER_8812];
	else
		pHalData->eeprom_thermal_meter = EEPROM_Default_ThermalMeter_8812;
	/* pHalData->eeprom_thermal_meter = (tempval&0x1f);	 */ /* [4:0] */

	if (pHalData->eeprom_thermal_meter == 0xff || AutoloadFail) {
		pHalData->odmpriv.rf_calibrate_info.is_apk_thermal_meter_ignore = _TRUE;
		pHalData->eeprom_thermal_meter = 0xFF;
	}

	/* pHalData->ThermalMeter[0] = pHalData->eeprom_thermal_meter;	 */
	RTW_INFO("ThermalMeter = 0x%x\n", pHalData->eeprom_thermal_meter);
}

void Hal_ReadRemoteWakeup_8812A(
	PADAPTER		padapter,
	IN	u8			*hwinfo,
	IN	BOOLEAN			AutoLoadFail
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
#ifdef CONFIG_USB_HCI
		if (IS_HARDWARE_TYPE_8821U(padapter))
			pwrctl->bSupportRemoteWakeup = (hwinfo[EEPROM_USB_OPTIONAL_FUNCTION0_8811AU] & BIT1) ? _TRUE : _FALSE;
		else
			pwrctl->bSupportRemoteWakeup = (hwinfo[EEPROM_USB_OPTIONAL_FUNCTION0] & BIT1) ? _TRUE : _FALSE;
#endif /* CONFIG_USB_HCI */

		RTW_INFO("%s...bSupportRemoteWakeup(%x)\n", __FUNCTION__, pwrctl->bSupportRemoteWakeup);
	}
}

VOID
Hal_ReadChannelPlan8812A(
	IN	PADAPTER		padapter,
	IN	u8				*hwinfo,
	IN	BOOLEAN			AutoLoadFail
)
{
	hal_com_config_channel_plan(
		padapter
		, hwinfo ? &hwinfo[EEPROM_COUNTRY_CODE_8812] : NULL
		, hwinfo ? hwinfo[EEPROM_ChannelPlan_8812] : 0xFF
		, padapter->registrypriv.alpha2
		, padapter->registrypriv.channel_plan
		, RTW_CHPLAN_REALTEK_DEFINE
		, AutoLoadFail
	);
}

VOID
Hal_EfuseParseXtal_8812A(
	IN	PADAPTER	pAdapter,
	IN	u8			*hwinfo,
	IN	BOOLEAN		AutoLoadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	if (!AutoLoadFail) {
		pHalData->crystal_cap = hwinfo[EEPROM_XTAL_8812];
		if (pHalData->crystal_cap == 0xFF)
			pHalData->crystal_cap = EEPROM_Default_CrystalCap_8812;	 /* what value should 8812 set? */
	} else
		pHalData->crystal_cap = EEPROM_Default_CrystalCap_8812;
	RTW_INFO("crystal_cap: 0x%2x\n", pHalData->crystal_cap);
}

/* for both 8812A and 8821A */
VOID
Hal_ReadAntennaDiversity8812A(
	IN	PADAPTER		pAdapter,
	IN	u8				*PROMContent,
	IN	BOOLEAN			AutoLoadFail
)
{
#ifdef CONFIG_ANTENNA_DIVERSITY
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(pAdapter);
	struct registry_priv *registry_par = &pAdapter->registrypriv;

	if (!AutoLoadFail) {
		if (registry_par->antdiv_cfg == 2) {
			if (PROMContent[EEPROM_RF_BOARD_OPTION_8812] == 0xFF)
				pHalData->AntDivCfg = (EEPROM_DEFAULT_BOARD_OPTION & BIT3) ? 1 : 0;
			else
				pHalData->AntDivCfg = (PROMContent[EEPROM_RF_BOARD_OPTION_8812] & BIT3) ? 1 : 0;
		} else {
			/* by registry */
			pHalData->AntDivCfg = registry_par->antdiv_cfg;
		}

		if (registry_par->antdiv_type == 0)
			pHalData->TRxAntDivType = PROMContent[EEPROM_RF_ANTENNA_OPT_8812];
		else
			pHalData->TRxAntDivType = registry_par->antdiv_type;

	} else {
		/* Disable */
		pHalData->AntDivCfg = 0;
	}

#ifdef CONFIG_BT_COEXIST
	if (hal_btcoex_1Ant(pAdapter)) {
		pHalData->AntDivCfg = 0;
		/*Add by YiWei , btcoex 1 ant module , ant band switch by btcoex , driver have to disable  ant band switch feature*/
		pAdapter->registrypriv.drv_ant_band_switch = 0;
	}
#endif

	RTW_INFO("[ANTDIV] AntDivCfg=%d, TRxAntDivType=0x%02x\n", pHalData->AntDivCfg, pHalData->TRxAntDivType);
#endif
}

VOID
hal_ReadPAType_8812A(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		if (GetRegAmplifierType2G(Adapter) == 0) { /* AUTO */
			pHalData->PAType_2G = ReadLE1Byte(&PROMContent[EEPROM_PA_TYPE_8812AU]);
			pHalData->LNAType_2G = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_2G_8812AU]);
			if (pHalData->PAType_2G == 0xFF)
				pHalData->PAType_2G = 0;
			if (pHalData->LNAType_2G == 0xFF)
				pHalData->LNAType_2G = 0;

			pHalData->ExternalPA_2G = ((pHalData->PAType_2G & BIT5) && (pHalData->PAType_2G & BIT4)) ? 1 : 0;
			pHalData->ExternalLNA_2G = ((pHalData->LNAType_2G & BIT7) && (pHalData->LNAType_2G & BIT3)) ? 1 : 0;
		} else {
			pHalData->ExternalPA_2G  = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			pHalData->ExternalLNA_2G = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
		}

		if (GetRegAmplifierType5G(Adapter) == 0) { /* AUTO */
			pHalData->PAType_5G = ReadLE1Byte(&PROMContent[EEPROM_PA_TYPE_8812AU]);
			pHalData->LNAType_5G = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_5G_8812AU]);
			if (pHalData->PAType_5G == 0xFF)
				pHalData->PAType_5G = 0;
			if (pHalData->LNAType_5G == 0xFF)
				pHalData->LNAType_5G = 0;

			pHalData->external_pa_5g = ((pHalData->PAType_5G & BIT1) && (pHalData->PAType_5G & BIT0)) ? 1 : 0;
			pHalData->external_lna_5g = ((pHalData->LNAType_5G & BIT7) && (pHalData->LNAType_5G & BIT3)) ? 1 : 0;
		} else {
			pHalData->external_pa_5g  = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_PA_5G)  ? 1 : 0;
			pHalData->external_lna_5g = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_LNA_5G) ? 1 : 0;
		}
	} else {
		pHalData->ExternalPA_2G  = EEPROM_Default_PAType;
		pHalData->external_pa_5g  = 0xFF;
		pHalData->ExternalLNA_2G = EEPROM_Default_LNAType;
		pHalData->external_lna_5g = 0xFF;

		if (GetRegAmplifierType2G(Adapter) == 0) { /* AUTO */
			pHalData->ExternalPA_2G  = 0;
			pHalData->ExternalLNA_2G = 0;
		} else {
			pHalData->ExternalPA_2G  = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			pHalData->ExternalLNA_2G = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
		}
		if (GetRegAmplifierType5G(Adapter) == 0) { /* AUTO */
			pHalData->external_pa_5g  = 0;
			pHalData->external_lna_5g = 0;
		} else {
			pHalData->external_pa_5g  = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_PA_5G)  ? 1 : 0;
			pHalData->external_lna_5g = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_LNA_5G) ? 1 : 0;
		}
	}
	RTW_INFO("pHalData->PAType_2G is 0x%x, pHalData->ExternalPA_2G = %d\n", pHalData->PAType_2G, pHalData->ExternalPA_2G);
	RTW_INFO("pHalData->PAType_5G is 0x%x, pHalData->external_pa_5g = %d\n", pHalData->PAType_5G, pHalData->external_pa_5g);
	RTW_INFO("pHalData->LNAType_2G is 0x%x, pHalData->ExternalLNA_2G = %d\n", pHalData->LNAType_2G, pHalData->ExternalLNA_2G);
	RTW_INFO("pHalData->LNAType_5G is 0x%x, pHalData->external_lna_5g = %d\n", pHalData->LNAType_5G, pHalData->external_lna_5g);
}

VOID
Hal_ReadAmplifierType_8812A(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	u8 extTypePA_2G_A  = (PROMContent[0xBD] & BIT2)      >> 2; /* 0xBD[2] */
	u8 extTypePA_2G_B  = (PROMContent[0xBD] & BIT6)      >> 6; /* 0xBD[6] */
	u8 extTypePA_5G_A  = (PROMContent[0xBF] & BIT2)      >> 2; /* 0xBF[2] */
	u8 extTypePA_5G_B  = (PROMContent[0xBF] & BIT6)      >> 6; /* 0xBF[6] */
	u8 extTypeLNA_2G_A = (PROMContent[0xBD] & (BIT1 | BIT0)) >> 0; /* 0xBD[1:0] */
	u8 extTypeLNA_2G_B = (PROMContent[0xBD] & (BIT5 | BIT4)) >> 4; /* 0xBD[5:4] */
	u8 extTypeLNA_5G_A = (PROMContent[0xBF] & (BIT1 | BIT0)) >> 0; /* 0xBF[1:0] */
	u8 extTypeLNA_5G_B = (PROMContent[0xBF] & (BIT5 | BIT4)) >> 4; /* 0xBF[5:4] */

	hal_ReadPAType_8812A(Adapter, PROMContent, AutoloadFail);

	if ((pHalData->PAType_2G & (BIT5 | BIT4)) == (BIT5 | BIT4)) /* [2.4G] Path A and B are both extPA */
		pHalData->TypeGPA  = extTypePA_2G_B  << 2 | extTypePA_2G_A;

	if ((pHalData->PAType_5G & (BIT1 | BIT0)) == (BIT1 | BIT0)) /* [5G] Path A and B are both extPA */
		pHalData->TypeAPA  = extTypePA_5G_B  << 2 | extTypePA_5G_A;

	if ((pHalData->LNAType_2G & (BIT7 | BIT3)) == (BIT7 | BIT3)) /* [2.4G] Path A and B are both extLNA */
		pHalData->TypeGLNA = extTypeLNA_2G_B << 2 | extTypeLNA_2G_A;

	if ((pHalData->LNAType_5G & (BIT7 | BIT3)) == (BIT7 | BIT3)) /* [5G] Path A and B are both extLNA */
		pHalData->TypeALNA = extTypeLNA_5G_B << 2 | extTypeLNA_5G_A;

	RTW_INFO("pHalData->TypeGPA = 0x%X\n", pHalData->TypeGPA);
	RTW_INFO("pHalData->TypeAPA = 0x%X\n", pHalData->TypeAPA);
	RTW_INFO("pHalData->TypeGLNA = 0x%X\n", pHalData->TypeGLNA);
	RTW_INFO("pHalData->TypeALNA = 0x%X\n", pHalData->TypeALNA);
}

VOID
Hal_ReadPAType_8821A(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		if (GetRegAmplifierType2G(Adapter) == 0) { /* AUTO */
			pHalData->PAType_2G = ReadLE1Byte(&PROMContent[EEPROM_PA_TYPE_8812AU]);
			pHalData->LNAType_2G = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_2G_8812AU]);
			if (pHalData->PAType_2G == 0xFF)
				pHalData->PAType_2G = 0;
			if (pHalData->LNAType_2G == 0xFF)
				pHalData->LNAType_2G = 0;

			pHalData->ExternalPA_2G = (pHalData->PAType_2G & BIT4) ? 1 : 0;
			pHalData->ExternalLNA_2G = (pHalData->LNAType_2G & BIT3) ? 1 : 0;
		} else {
			pHalData->ExternalPA_2G  = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			pHalData->ExternalLNA_2G = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
		}

		if (GetRegAmplifierType5G(Adapter) == 0) { /* AUTO */
			pHalData->PAType_5G = ReadLE1Byte(&PROMContent[EEPROM_PA_TYPE_8812AU]);
			pHalData->LNAType_5G = ReadLE1Byte(&PROMContent[EEPROM_LNA_TYPE_5G_8812AU]);
			if (pHalData->PAType_5G == 0xFF)
				pHalData->PAType_5G = 0;
			if (pHalData->LNAType_5G == 0xFF)
				pHalData->LNAType_5G = 0;

			pHalData->external_pa_5g = (pHalData->PAType_5G & BIT0) ? 1 : 0;
			pHalData->external_lna_5g = (pHalData->LNAType_5G & BIT3) ? 1 : 0;
		} else {
			pHalData->external_pa_5g  = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_PA_5G)  ? 1 : 0;
			pHalData->external_lna_5g = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_LNA_5G) ? 1 : 0;
		}
	} else {
		pHalData->ExternalPA_2G  = EEPROM_Default_PAType;
		pHalData->external_pa_5g  = 0xFF;
		pHalData->ExternalLNA_2G = EEPROM_Default_LNAType;
		pHalData->external_lna_5g = 0xFF;

		if (GetRegAmplifierType2G(Adapter) == 0) { /* AUTO */
			pHalData->ExternalPA_2G  = 0;
			pHalData->ExternalLNA_2G = 0;
		} else {
			pHalData->ExternalPA_2G  = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_PA)  ? 1 : 0;
			pHalData->ExternalLNA_2G = (GetRegAmplifierType2G(Adapter) & ODM_BOARD_EXT_LNA) ? 1 : 0;
		}
		if (GetRegAmplifierType5G(Adapter) == 0) { /* AUTO */
			pHalData->external_pa_5g  = 0;
			pHalData->external_lna_5g = 0;
		} else {
			pHalData->external_pa_5g  = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_PA_5G)  ? 1 : 0;
			pHalData->external_lna_5g = (GetRegAmplifierType5G(Adapter) & ODM_BOARD_EXT_LNA_5G) ? 1 : 0;
		}
	}
	RTW_INFO("pHalData->PAType_2G is 0x%x, pHalData->ExternalPA_2G = %d\n", pHalData->PAType_2G, pHalData->ExternalPA_2G);
	RTW_INFO("pHalData->PAType_5G is 0x%x, pHalData->external_pa_5g = %d\n", pHalData->PAType_5G, pHalData->external_pa_5g);
	RTW_INFO("pHalData->LNAType_2G is 0x%x, pHalData->ExternalLNA_2G = %d\n", pHalData->LNAType_2G, pHalData->ExternalLNA_2G);
	RTW_INFO("pHalData->LNAType_5G is 0x%x, pHalData->external_lna_5g = %d\n", pHalData->LNAType_5G, pHalData->external_lna_5g);
}

VOID
Hal_ReadRFEType_8812A(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);

	if (!AutoloadFail) {
		if ((GetRegRFEType(Adapter) != 64) || 0xFF == PROMContent[EEPROM_RFE_OPTION_8812]) {
			if (GetRegRFEType(Adapter) != 64)
				pHalData->rfe_type = GetRegRFEType(Adapter);
			else {
				if (IS_HARDWARE_TYPE_8812AU(Adapter))
					pHalData->rfe_type = 0;
				else if (IS_HARDWARE_TYPE_8812E(Adapter))
					pHalData->rfe_type = 2;
				else
					pHalData->rfe_type = EEPROM_DEFAULT_RFE_OPTION;
			}

		} else if (PROMContent[EEPROM_RFE_OPTION_8812] & BIT7) {
			if (pHalData->external_lna_5g) {
				if (pHalData->external_pa_5g) {
					if (pHalData->ExternalLNA_2G && pHalData->ExternalPA_2G)
						pHalData->rfe_type = 3;
					else
						pHalData->rfe_type = 0;
				} else
					pHalData->rfe_type = 2;
			} else
				pHalData->rfe_type = 4;
		} else {
			pHalData->rfe_type = PROMContent[EEPROM_RFE_OPTION_8812] & 0x3F;

			/* 2013/03/19 MH Due to othe customer already use incorrect EFUSE map */
			/* to for their product. We need to add workaround to prevent to modify */
			/* spec and notify all customer to revise the IC 0xca content. After */
			/* discussing with Willis an YN, revise driver code to prevent. */
			if (pHalData->rfe_type == 4 &&
			    (pHalData->external_pa_5g == _TRUE || pHalData->ExternalPA_2G == _TRUE ||
			     pHalData->external_lna_5g == _TRUE || pHalData->ExternalLNA_2G == _TRUE)) {
				if (IS_HARDWARE_TYPE_8812AU(Adapter))
					pHalData->rfe_type = 0;
				else if (IS_HARDWARE_TYPE_8812E(Adapter))
					pHalData->rfe_type = 2;
			}
		}
	} else {
		if (GetRegRFEType(Adapter) != 64)
			pHalData->rfe_type = GetRegRFEType(Adapter);
		else {
			if (IS_HARDWARE_TYPE_8812AU(Adapter))
				pHalData->rfe_type = 0;
			else if (IS_HARDWARE_TYPE_8812E(Adapter))
				pHalData->rfe_type = 2;
			else
				pHalData->rfe_type = EEPROM_DEFAULT_RFE_OPTION;
		}
	}

	RTW_INFO("RFE Type: 0x%2x\n", pHalData->rfe_type);
}

void Hal_EfuseParseKFreeData_8821A(
	IN		PADAPTER	Adapter,
	IN		u8		*PROMContent,
	IN		BOOLEAN		AutoloadFail)
{
#ifdef CONFIG_RF_POWER_TRIM

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct kfree_data_t *kfree_data = &pHalData->kfree_data;
	u8 pg_pwrtrim = 0xFF, pg_pwrtrim_5g_a = 0xFF, pg_pwrtrim_5g_lb1 = 0xFF, 
					pg_pwrtrim_5g_lb2 = 0xFF, pg_pwrtrim_5g_mb1 = 0xFF, pg_pwrtrim_5g_mb2 = 0xFF, pg_pwrtrim_5g_hb = 0xFF, pg_therm = 0xFF;

	if ((Adapter->registrypriv.RegPwrTrimEnable == 1) || !AutoloadFail) {

		efuse_OneByteRead(Adapter, PPG_BB_GAIN_2G_TXA_OFFSET_8821A, &pg_pwrtrim, _FALSE);
		efuse_OneByteRead(Adapter, PPG_BB_GAIN_5GLB1_TXA_OFFSET_8821A, &pg_pwrtrim_5g_lb1, _FALSE);
		efuse_OneByteRead(Adapter, PPG_BB_GAIN_5GLB2_TXA_OFFSET_8821A, &pg_pwrtrim_5g_lb2, _FALSE);
		efuse_OneByteRead(Adapter, PPG_BB_GAIN_5GMB1_TXA_OFFSET_8821A, &pg_pwrtrim_5g_mb1, _FALSE);
		efuse_OneByteRead(Adapter, PPG_BB_GAIN_5GMB2_TXA_OFFSET_8821A, &pg_pwrtrim_5g_mb2, _FALSE);
		efuse_OneByteRead(Adapter, PPG_BB_GAIN_5GHB_TXA_OFFSET_8821A, &pg_pwrtrim_5g_hb, _FALSE);
		efuse_OneByteRead(Adapter, PPG_THERMAL_OFFSET_8821A, &pg_therm, _FALSE);

		kfree_data->bb_gain[BB_GAIN_2G][RF_PATH_A]
			= KFREE_BB_GAIN_2G_TX_OFFSET(pg_pwrtrim & PPG_BB_GAIN_2G_TX_OFFSET_MASK);
		kfree_data->bb_gain[BB_GAIN_5GLB1][RF_PATH_A]
			= KFREE_BB_GAIN_5G_TX_OFFSET(pg_pwrtrim_5g_lb1 & PPG_BB_GAIN_5G_TX_OFFSET_MASK);
		kfree_data->bb_gain[BB_GAIN_5GLB2][RF_PATH_A]
			= KFREE_BB_GAIN_5G_TX_OFFSET(pg_pwrtrim_5g_lb2 & PPG_BB_GAIN_5G_TX_OFFSET_MASK);
		kfree_data->bb_gain[BB_GAIN_5GMB1][RF_PATH_A]
			= KFREE_BB_GAIN_5G_TX_OFFSET(pg_pwrtrim_5g_mb1 & PPG_BB_GAIN_5G_TX_OFFSET_MASK);
		kfree_data->bb_gain[BB_GAIN_5GMB2][RF_PATH_A]
			= KFREE_BB_GAIN_5G_TX_OFFSET(pg_pwrtrim_5g_mb2) & PPG_BB_GAIN_5G_TX_OFFSET_MASK);
		kfree_data->bb_gain[BB_GAIN_5GHB][RF_PATH_A]
			= KFREE_BB_GAIN_5G_TX_OFFSET(pg_pwrtrim_5g_hb & PPG_BB_GAIN_5G_TX_OFFSET_MASK);
		kfree_data->thermal
			= KFREE_THERMAL_OFFSET(pg_therm & PPG_THERMAL_OFFSET_MASK);
	}

	if (!AutoloadFail) {
		if (GET_PG_KFREE_ON_8821A(PROMContent))
			kfree_data->flag |= KFREE_FLAG_ON;
		if (GET_PG_KFREE_THERMAL_K_ON_8821A(PROMContent))
			kfree_data->flag |= KFREE_FLAG_THERMAL_K_ON;
	} else {
		if (!kfree_data_is_bb_gain_empty(kfree_data))
			kfree_data->flag |= KFREE_FLAG_ON;
		if (kfree_data->thermal != 0)
			kfree_data->flag |= KFREE_FLAG_THERMAL_K_ON;
	}

	if (Adapter->registrypriv.RegPwrTrimEnable == 1) {
		kfree_data->flag |= KFREE_FLAG_ON;
		kfree_data->flag |= KFREE_FLAG_THERMAL_K_ON;
	}

	if (kfree_data->flag & KFREE_FLAG_THERMAL_K_ON)
		pHalData->eeprom_thermal_meter += kfree_data->thermal;

	RTW_INFO("kfree flag:0x%02x\n", kfree_data->flag);
	if (Adapter->registrypriv.RegPwrTrimEnable || kfree_data->flag & KFREE_FLAG_ON) {
		int i;

		RTW_PRINT_SEL(RTW_DBGDUMP, "bb_gain:");
		for (i = 0; i < BB_GAIN_NUM; i++)
			_RTW_PRINT_SEL(RTW_DBGDUMP, "%d ", kfree_data->bb_gain[i][RF_PATH_A]);
		_RTW_PRINT_SEL(RTW_DBGDUMP, "\n");
	}
	if (Adapter->registrypriv.RegPwrTrimEnable || kfree_data->flag & KFREE_FLAG_THERMAL_K_ON)
		RTW_INFO("thermal:%d\n", kfree_data->thermal);

#endif /*CONFIG_RF_POWER_TRIM */
}

/*
 * 2013/04/15 MH Add 8812AU- VL/VS/VN for different board type.
 *   */
VOID
hal_ReadUsbType_8812AU(
	IN	PADAPTER	Adapter,
	IN	u8			*PROMContent,
	IN	BOOLEAN		AutoloadFail
)
{
	/* if (IS_HARDWARE_TYPE_8812AU(Adapter) && Adapter->UsbModeMechanism.RegForcedUsbMode == 5) */
	{
		PHAL_DATA_TYPE pHalData = GET_HAL_DATA(Adapter);
		struct hal_spec_t *hal_spc = GET_HAL_SPEC(Adapter);
		u8	reg_tmp, i, j, antenna = 0, wmode = 0;
		/* Read anenna type from EFUSE 1019/1018 */
		for (i = 0; i < 2; i++) {
			/*
			  Check efuse address 1019
			  Check efuse address 1018
			*/
			efuse_OneByteRead(Adapter, 1019 - i, &reg_tmp, _FALSE);
			/*
			  CHeck bit 7-5
			  Check bit 3-1
			*/
			if (((reg_tmp >> 5) & 0x7) != 0) {
				antenna = ((reg_tmp >> 5) & 0x7);
				break;
			} else if ((reg_tmp >> 1 & 0x07) != 0) {
				antenna = ((reg_tmp >> 1) & 0x07);
				break;
			}


		}

		/* Read anenna type from EFUSE 1021/1020 */
		for (i = 0; i < 2; i++) {
			/*
			  Check efuse address 1021
			  Check efuse address 1020
			*/
			efuse_OneByteRead(Adapter, 1021 - i, &reg_tmp, _FALSE);

			/* CHeck bit 3-2 */
			if (((reg_tmp >> 2) & 0x3) != 0) {
				wmode = ((reg_tmp >> 2) & 0x3);
				break;
			}


		}

		RTW_INFO("%s: antenna=%d, wmode=%d\n", __func__, antenna, wmode);
		/* Antenna == 1 WMODE = 3 RTL8812AU-VL 11AC + USB2.0 Mode */
		if (antenna == 1) {
			/* Config 8812AU as 1*1 mode AC mode. */
			pHalData->rf_type = RF_1T1R;
			/* UsbModeSwitch_SetUsbModeMechOn(Adapter, FALSE); */
			/* pHalData->EFUSEHidden = EFUSE_HIDDEN_812AU_VL; */
			RTW_INFO("%s(): EFUSE_HIDDEN_812AU_VL\n", __FUNCTION__);
		} else if (antenna == 2) {
			if (wmode == 3) {
				if (PROMContent[EEPROM_USB_MODE_8812] == 0x2) {
					/* RTL8812AU Normal Mode. No further action. */
					/* pHalData->EFUSEHidden = EFUSE_HIDDEN_812AU; */
					RTW_INFO("%s(): EFUSE_HIDDEN_812AU\n", __FUNCTION__);
				} else {
					/* Antenna == 2 WMODE = 3 RTL8812AU-VS 11AC + USB2.0 Mode */
					/* Driver will not support USB automatic switch */
					/* UsbModeSwitch_SetUsbModeMechOn(Adapter, FALSE); */
					/* pHalData->EFUSEHidden = EFUSE_HIDDEN_812AU_VS; */
					RTW_INFO("%s(): EFUSE_HIDDEN_8812AU_VS\n", __func__);
				}
			} else if (wmode == 2) {
				/* Antenna == 2 WMODE = 2 RTL8812AU-VN 11N only + USB2.0 Mode */
				/* UsbModeSwitch_SetUsbModeMechOn(Adapter, FALSE); */
				/* pHalData->EFUSEHidden = EFUSE_HIDDEN_812AU_VN; */
				RTW_INFO("%s(): EFUSE_HIDDEN_8812AU_VN\n", __func__);
				hal_spc->proto_cap &= ~PROTO_CAP_11AC;
			}
		}
	}
}

enum {
	VOLTAGE_V25						= 0x03,
	LDOE25_SHIFT						= 28 ,
};

static VOID
Hal_EfusePowerSwitch8812A(
	IN	PADAPTER	pAdapter,
	IN	u8		bWrite,
	IN	u8		PwrState)
{
	u8	tempval;
	u16	tmpV16;
#define EFUSE_ACCESS_ON_JAGUAR 0x69
#define EFUSE_ACCESS_OFF_JAGUAR 0x00
	if (PwrState == _TRUE) {
		rtw_write8(pAdapter, REG_EFUSE_BURN_GNT_8812, EFUSE_ACCESS_ON_JAGUAR);

		/* 1.2V Power: From VDDON with Power Cut(0x0000h[15]), defualt valid */
		tmpV16 = rtw_read16(pAdapter, REG_SYS_ISO_CTRL);
		if (!(tmpV16 & PWC_EV12V)) {
			tmpV16 |= PWC_EV12V ;
			/* rtw_write16(pAdapter,REG_SYS_ISO_CTRL,tmpV16); */
		}
		/* Reset: 0x0000h[28], default valid */
		tmpV16 =  rtw_read16(pAdapter, REG_SYS_FUNC_EN);
		if (!(tmpV16 & FEN_ELDR)) {
			tmpV16 |= FEN_ELDR ;
			rtw_write16(pAdapter, REG_SYS_FUNC_EN, tmpV16);
		}

		/* Clock: Gated(0x0008h[5]) 8M(0x0008h[1]) clock from ANA, default valid */
		tmpV16 = rtw_read16(pAdapter, REG_SYS_CLKR);
		if ((!(tmpV16 & LOADER_CLK_EN))  || (!(tmpV16 & ANA8M))) {
			tmpV16 |= (LOADER_CLK_EN | ANA8M) ;
			rtw_write16(pAdapter, REG_SYS_CLKR, tmpV16);
		}

		if (bWrite == _TRUE) {
			/* Enable LDO 2.5V before read/write action */
			tempval = rtw_read8(pAdapter, EFUSE_TEST + 3);
			tempval &= ~(BIT3 | BIT4 | BIT5 | BIT6);
			tempval |= (VOLTAGE_V25 << 3);
			tempval |= BIT7;
			rtw_write8(pAdapter, EFUSE_TEST + 3, tempval);
		}
	} else {
		rtw_write8(pAdapter, REG_EFUSE_BURN_GNT_8812, EFUSE_ACCESS_OFF_JAGUAR);

		if (bWrite == _TRUE) {
			/* Disable LDO 2.5V after read/write action */
			tempval = rtw_read8(pAdapter, EFUSE_TEST + 3);
			rtw_write8(pAdapter, EFUSE_TEST + 3, (tempval & 0x7F));
		}
	}

}

static BOOLEAN
Hal_EfuseSwitchToBank8812A(
	IN		PADAPTER	pAdapter,
	IN		u1Byte		bank,
	IN		BOOLEAN		bPseudoTest
)
{
	return _FALSE;
}

static VOID
Hal_EfuseReadEFuse8812A(
	PADAPTER		Adapter,
	u16			_offset,
	u16			_size_byte,
	u8		*pbuf,
	IN	BOOLEAN	bPseudoTest
)
{
	u8	*efuseTbl = NULL;
	u8	rtemp8[1];
	u16	eFuse_Addr = 0;
	u8	offset, wren;
	u16	i, j;
	u16	**eFuseWord = NULL;
	u16	efuse_utilized = 0;
	u8	efuse_usage = 0;
	u8	u1temp = 0;

	/*  */
	/* Do NOT excess total size of EFuse table. Added by Roger, 2008.11.10. */
	/*  */
	if ((_offset + _size_byte) > EFUSE_MAP_LEN_JAGUAR) {
		/* total E-Fuse table is 512bytes */
		RTW_INFO("Hal_EfuseReadEFuse8812A(): Invalid offset(%#x) with read bytes(%#x)!!\n", _offset, _size_byte);
		goto exit;
	}

	efuseTbl = (u8 *)rtw_zmalloc(EFUSE_MAP_LEN_JAGUAR);
	if (efuseTbl == NULL) {
		RTW_INFO("%s: alloc efuseTbl fail!\n", __FUNCTION__);
		goto exit;
	}

	eFuseWord = (u16 **)rtw_malloc2d(EFUSE_MAX_SECTION_JAGUAR, EFUSE_MAX_WORD_UNIT, 2);
	if (eFuseWord == NULL) {
		RTW_INFO("%s: alloc eFuseWord fail!\n", __FUNCTION__);
		goto exit;
	}

	/* 0. Refresh efuse init map as all oxFF. */
	for (i = 0; i < EFUSE_MAX_SECTION_JAGUAR; i++)
		for (j = 0; j < EFUSE_MAX_WORD_UNIT; j++)
			eFuseWord[i][j] = 0xFFFF;

	/*  */
	/* 1. Read the first byte to check if efuse is empty!!! */
	/*  */
	/*  */
	ReadEFuseByte(Adapter, eFuse_Addr, rtemp8, bPseudoTest);
	if (*rtemp8 != 0xFF) {
		efuse_utilized++;
		/* RTW_INFO("efuse_Addr-%d efuse_data=%x\n", eFuse_Addr, *rtemp8); */
		eFuse_Addr++;
	} else {
		RTW_INFO("EFUSE is empty efuse_Addr-%d efuse_data=%x\n", eFuse_Addr, *rtemp8);
		goto exit;
	}


	/*  */
	/* 2. Read real efuse content. Filter PG header and every section data. */
	/*  */
	while ((*rtemp8 != 0xFF) && (eFuse_Addr < EFUSE_REAL_CONTENT_LEN_JAGUAR)) {
		/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("efuse_Addr-%d efuse_data=%x\n", eFuse_Addr-1, *rtemp8)); */

		/* Check PG header for section num. */
		if ((*rtemp8 & 0x1F) == 0x0F) {	/* extended header */
			u1temp = ((*rtemp8 & 0xE0) >> 5);
			/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("extended header u1temp=%x *rtemp&0xE0 0x%x\n", u1temp, *rtemp8 & 0xE0)); */

			/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("extended header u1temp=%x\n", u1temp)); */

			ReadEFuseByte(Adapter, eFuse_Addr, rtemp8, bPseudoTest);

			/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("extended header efuse_Addr-%d efuse_data=%x\n", eFuse_Addr, *rtemp8));	 */

			if ((*rtemp8 & 0x0F) == 0x0F) {
				eFuse_Addr++;
				ReadEFuseByte(Adapter, eFuse_Addr, rtemp8, bPseudoTest);

				if (*rtemp8 != 0xFF && (eFuse_Addr < EFUSE_REAL_CONTENT_LEN_JAGUAR))
					eFuse_Addr++;
				continue;
			} else {
				offset = ((*rtemp8 & 0xF0) >> 1) | u1temp;
				wren = (*rtemp8 & 0x0F);
				eFuse_Addr++;
			}
		} else {
			offset = ((*rtemp8 >> 4) & 0x0f);
			wren = (*rtemp8 & 0x0f);
		}

		if (offset < EFUSE_MAX_SECTION_JAGUAR) {
			/* Get word enable value from PG header */
			/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("Offset-%d Worden=%x\n", offset, wren)); */

			for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++) {
				/* Check word enable condition in the section				 */
				if (!(wren & 0x01)) {
					/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("Addr=%d\n", eFuse_Addr)); */
					ReadEFuseByte(Adapter, eFuse_Addr, rtemp8, bPseudoTest);
					eFuse_Addr++;
					efuse_utilized++;
					eFuseWord[offset][i] = (*rtemp8 & 0xff);


					if (eFuse_Addr >= EFUSE_REAL_CONTENT_LEN_JAGUAR)
						break;

					/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("Addr=%d", eFuse_Addr)); */
					ReadEFuseByte(Adapter, eFuse_Addr, rtemp8, bPseudoTest);
					eFuse_Addr++;

					efuse_utilized++;
					eFuseWord[offset][i] |= (((u2Byte)*rtemp8 << 8) & 0xff00);

					if (eFuse_Addr >= EFUSE_REAL_CONTENT_LEN_JAGUAR)
						break;
				}

				wren >>= 1;

			}
		} else { /* deal with error offset,skip error data		 */
			RTW_PRINT("invalid offset:0x%02x\n", offset);
			for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++) {
				/* Check word enable condition in the section				 */
				if (!(wren & 0x01)) {
					eFuse_Addr++;
					efuse_utilized++;
					if (eFuse_Addr >= EFUSE_REAL_CONTENT_LEN_JAGUAR)
						break;
					eFuse_Addr++;
					efuse_utilized++;
					if (eFuse_Addr >= EFUSE_REAL_CONTENT_LEN_JAGUAR)
						break;
				}
			}
		}
		/* Read next PG header */
		ReadEFuseByte(Adapter, eFuse_Addr, rtemp8, bPseudoTest);
		/* RTPRINT(FEEPROM, EFUSE_READ_ALL, ("Addr=%d rtemp 0x%x\n", eFuse_Addr, *rtemp8)); */

		if (*rtemp8 != 0xFF && (eFuse_Addr < EFUSE_REAL_CONTENT_LEN_JAGUAR)) {
			efuse_utilized++;
			eFuse_Addr++;
		}
	}

	/*  */
	/* 3. Collect 16 sections and 4 word unit into Efuse map. */
	/*  */
	for (i = 0; i < EFUSE_MAX_SECTION_JAGUAR; i++) {
		for (j = 0; j < EFUSE_MAX_WORD_UNIT; j++) {
			efuseTbl[(i * 8) + (j * 2)] = (eFuseWord[i][j] & 0xff);
			efuseTbl[(i * 8) + ((j * 2) + 1)] = ((eFuseWord[i][j] >> 8) & 0xff);
		}
	}


	/*  */
	/* 4. Copy from Efuse map to output pointer memory!!! */
	/*  */
	for (i = 0; i < _size_byte; i++)
		pbuf[i] = efuseTbl[_offset + i];

	/*  */
	/* 5. Calculate Efuse utilization. */
	/*  */
	efuse_usage = (u1Byte)((eFuse_Addr * 100) / EFUSE_REAL_CONTENT_LEN_JAGUAR);
	rtw_hal_set_hwreg(Adapter, HW_VAR_EFUSE_BYTES, (u8 *)&eFuse_Addr);
	RTW_INFO("%s: eFuse_Addr offset(%#x) !!\n", __FUNCTION__, eFuse_Addr);

exit:
	if (efuseTbl)
		rtw_mfree(efuseTbl, EFUSE_MAP_LEN_JAGUAR);

	if (eFuseWord)
		rtw_mfree2d((void *)eFuseWord, EFUSE_MAX_SECTION_JAGUAR, EFUSE_MAX_WORD_UNIT, sizeof(u16));
}

/* ***********************************************************
 *				Efuse related code
 * *********************************************************** */
static u8
hal_EfuseSwitchToBank(
	PADAPTER	padapter,
	u8			bank,
	u8			bPseudoTest)
{
	u8 bRet = _FALSE;
	u32 value32 = 0;
#ifdef HAL_EFUSE_MEMORY
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);
	PEFUSE_HAL pEfuseHal = &pHalData->EfuseHal;
#endif


	RTW_INFO("%s: Efuse switch bank to %d\n", __FUNCTION__, bank);
	if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
		pEfuseHal->fakeEfuseBank = bank;
#else
		fakeEfuseBank = bank;
#endif
		bRet = _TRUE;
	} else {
		value32 = rtw_read32(padapter, EFUSE_TEST);
		bRet = _TRUE;
		switch (bank) {
		case 0:
			value32 = (value32 & ~EFUSE_SEL_MASK) | EFUSE_SEL(EFUSE_WIFI_SEL_0);
			break;
		case 1:
			value32 = (value32 & ~EFUSE_SEL_MASK) | EFUSE_SEL(EFUSE_BT_SEL_0);
			break;
		case 2:
			value32 = (value32 & ~EFUSE_SEL_MASK) | EFUSE_SEL(EFUSE_BT_SEL_1);
			break;
		case 3:
			value32 = (value32 & ~EFUSE_SEL_MASK) | EFUSE_SEL(EFUSE_BT_SEL_2);
			break;
		default:
			value32 = (value32 & ~EFUSE_SEL_MASK) | EFUSE_SEL(EFUSE_WIFI_SEL_0);
			bRet = _FALSE;
			break;
		}
		rtw_write32(padapter, EFUSE_TEST, value32);
	}

	return bRet;
}


static VOID
hal_ReadEFuse_BT(
	PADAPTER	padapter,
	u16			_offset,
	u16			_size_byte,
	u8			*pbuf,
	u8			bPseudoTest
)
{
#ifdef HAL_EFUSE_MEMORY
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	PEFUSE_HAL		pEfuseHal = &pHalData->EfuseHal;
#endif
	u8	*efuseTbl;
	u8	bank;
	u16	eFuse_Addr;
	u8	efuseHeader, efuseExtHdr, efuseData;
	u8	offset, wden;
	u16	i, total, used;
	u8	efuse_usage;


	/* */
	/* Do NOT excess total size of EFuse table. Added by Roger, 2008.11.10. */
	/* */
	if ((_offset + _size_byte) > EFUSE_BT_MAP_LEN) {
		RTW_INFO("%s: Invalid offset(%#x) with read bytes(%#x)!!\n", __FUNCTION__, _offset, _size_byte);
		return;
	}

	efuseTbl = rtw_malloc(EFUSE_BT_MAP_LEN);
	if (efuseTbl == NULL) {
		RTW_INFO("%s: efuseTbl malloc fail!\n", __FUNCTION__);
		return;
	}
	/* 0xff will be efuse default value instead of 0x00. */
	_rtw_memset(efuseTbl, 0xFF, EFUSE_BT_MAP_LEN);

	total = 0;
	EFUSE_GetEfuseDefinition(padapter, EFUSE_BT, TYPE_AVAILABLE_EFUSE_BYTES_BANK, &total, bPseudoTest);

	for (bank = 1; bank < 3; bank++) { /* 8821a BT 2 bank 512 size */
		if (hal_EfuseSwitchToBank(padapter, bank, bPseudoTest) == _FALSE) {
			RTW_INFO("%s: hal_EfuseSwitchToBank Fail!!\n", __FUNCTION__);
			goto exit;
		}

		eFuse_Addr = 0;

		while (AVAILABLE_EFUSE_ADDR(eFuse_Addr)) {
			/* ReadEFuseByte(padapter, eFuse_Addr++, &efuseHeader, bPseudoTest); */
			efuse_OneByteRead(padapter, eFuse_Addr++, &efuseHeader, bPseudoTest);
			if (efuseHeader == 0xFF)
				break;
			RTW_INFO("%s: efuse[%#X]=0x%02x (header)\n", __FUNCTION__, (((bank - 1) * EFUSE_BT_REAL_CONTENT_LEN) + eFuse_Addr - 1), efuseHeader);

			/* Check PG header for section num. */
			if (EXT_HEADER(efuseHeader)) {	/* extended header */
				offset = GET_HDR_OFFSET_2_0(efuseHeader);
				RTW_INFO("%s: extended header offset_2_0=0x%X\n", __FUNCTION__, offset);

				/* ReadEFuseByte(padapter, eFuse_Addr++, &efuseExtHdr, bPseudoTest); */
				efuse_OneByteRead(padapter, eFuse_Addr++, &efuseExtHdr, bPseudoTest);
				RTW_INFO("%s: efuse[%#X]=0x%02x (ext header)\n", __FUNCTION__, (((bank - 1) * EFUSE_BT_REAL_CONTENT_LEN) + eFuse_Addr - 1), efuseExtHdr);
				if (ALL_WORDS_DISABLED(efuseExtHdr))
					continue;

				offset |= ((efuseExtHdr & 0xF0) >> 1);
				wden = (efuseExtHdr & 0x0F);
			} else {
				offset = ((efuseHeader >> 4) & 0x0f);
				wden = (efuseHeader & 0x0f);
			}

			if (offset < EFUSE_BT_MAX_SECTION) {
				u16 addr;

				/* Get word enable value from PG header */
				RTW_INFO("%s: Offset=%d Worden=%#X\n", __FUNCTION__, offset, wden);

				addr = offset * PGPKT_DATA_SIZE;
				for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++) {
					/* Check word enable condition in the section */
					if (!(wden & (0x01 << i))) {
						efuseData = 0;
						/* ReadEFuseByte(padapter, eFuse_Addr++, &efuseData, bPseudoTest); */
						efuse_OneByteRead(padapter, eFuse_Addr++, &efuseData, bPseudoTest);
						RTW_INFO("%s: efuse[%#X]=0x%02X\n", __FUNCTION__, eFuse_Addr - 1, efuseData);
						efuseTbl[addr] = efuseData;

						efuseData = 0;
						/* ReadEFuseByte(padapter, eFuse_Addr++, &efuseData, bPseudoTest); */
						efuse_OneByteRead(padapter, eFuse_Addr++, &efuseData, bPseudoTest);
						RTW_INFO("%s: efuse[%#X]=0x%02X\n", __FUNCTION__, eFuse_Addr - 1, efuseData);
						efuseTbl[addr + 1] = efuseData;
					}
					addr += 2;
				}
			} else {
				RTW_INFO("%s: offset(%d) is illegal!!\n", __FUNCTION__, offset);
				eFuse_Addr += Efuse_CalculateWordCnts(wden) * 2;
			}
		}

		if ((eFuse_Addr - 1) < total) {
			RTW_INFO("%s: bank(%d) data end at %#x\n", __FUNCTION__, bank, eFuse_Addr - 1);
			break;
		}
	}

	/* switch bank back to bank 0 for later BT and wifi use. */
	hal_EfuseSwitchToBank(padapter, 0, bPseudoTest);

	/* Copy from Efuse map to output pointer memory!!! */
	for (i = 0; i < _size_byte; i++)
		pbuf[i] = efuseTbl[_offset + i];

	/* */
	/* Calculate Efuse utilization. */
	/* */
	EFUSE_GetEfuseDefinition(padapter, EFUSE_BT, TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, &total, bPseudoTest);
	used = (EFUSE_BT_REAL_BANK_CONTENT_LEN * (bank - 1)) + eFuse_Addr - 1;
	RTW_INFO("%s: bank(%d) data end at %#x ,used =%d\n", __FUNCTION__, bank, eFuse_Addr - 1, used);
	efuse_usage = (u8)((used * 100) / total);
	if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
		pEfuseHal->fakeBTEfuseUsedBytes = used;
#else
		fakeBTEfuseUsedBytes = used;
#endif
	} else {
		rtw_hal_set_hwreg(padapter, HW_VAR_EFUSE_BT_BYTES, (u8 *)&used);
		rtw_hal_set_hwreg(padapter, HW_VAR_EFUSE_BT_USAGE, (u8 *)&efuse_usage);
	}

exit:
	if (efuseTbl)
		rtw_mfree(efuseTbl, EFUSE_BT_MAP_LEN);
}


static VOID
rtl8812_ReadEFuse(
	PADAPTER	Adapter,
	u8		efuseType,
	u16		_offset,
	u16		_size_byte,
	u8	*pbuf,
	IN	BOOLEAN	bPseudoTest
)
{
	if (efuseType == EFUSE_WIFI)
		Hal_EfuseReadEFuse8812A(Adapter, _offset, _size_byte, pbuf, bPseudoTest);
	else
		hal_ReadEFuse_BT(Adapter, _offset, _size_byte, pbuf, bPseudoTest);
}

/* Do not support BT */
VOID
Hal_EFUSEGetEfuseDefinition8812A(
	IN		PADAPTER	pAdapter,
	IN		u1Byte		efuseType,
	IN		u1Byte		type,
	OUT		PVOID		pOut
)
{
	switch (type) {
	case TYPE_EFUSE_MAX_SECTION: {
		u8	*pMax_section;
		pMax_section = (u8 *)pOut;
		if (efuseType == EFUSE_WIFI)
			*pMax_section = EFUSE_MAX_SECTION_JAGUAR;
		else
			*pMax_section = EFUSE_BT_MAX_SECTION;
	}
		break;
	case TYPE_EFUSE_REAL_CONTENT_LEN: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		if (efuseType == EFUSE_WIFI)
			*pu2Tmp = EFUSE_REAL_CONTENT_LEN_JAGUAR;
		else
			*pu2Tmp = EFUSE_BT_REAL_CONTENT_LEN;
	}
		break;
	case TYPE_EFUSE_CONTENT_LEN_BANK: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		if (efuseType == EFUSE_WIFI)
			*pu2Tmp = EFUSE_REAL_CONTENT_LEN_JAGUAR;
		else
			*pu2Tmp = EFUSE_BT_REAL_BANK_CONTENT_LEN;

	}
		break;
	case TYPE_AVAILABLE_EFUSE_BYTES_BANK: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		if (efuseType == EFUSE_WIFI)
			*pu2Tmp = (u16)(EFUSE_REAL_CONTENT_LEN_JAGUAR - EFUSE_OOB_PROTECT_BYTES_JAGUAR);
		else
			*pu2Tmp = (EFUSE_BT_REAL_BANK_CONTENT_LEN - EFUSE_PROTECT_BYTES_BANK);
	}
		break;
	case TYPE_AVAILABLE_EFUSE_BYTES_TOTAL: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		if (efuseType == EFUSE_WIFI)
			*pu2Tmp = (u16)(EFUSE_REAL_CONTENT_LEN_JAGUAR - EFUSE_OOB_PROTECT_BYTES_JAGUAR);
		else
			*pu2Tmp = (EFUSE_BT_REAL_CONTENT_LEN - (EFUSE_PROTECT_BYTES_BANK * 3));
	}
		break;
	case TYPE_EFUSE_MAP_LEN: {
		u16 *pu2Tmp;
		pu2Tmp = (u16 *)pOut;
		if (efuseType == EFUSE_WIFI)
			*pu2Tmp = (u16)EFUSE_MAP_LEN_JAGUAR;
		else
			*pu2Tmp = EFUSE_BT_MAP_LEN;
	}
		break;
	case TYPE_EFUSE_PROTECT_BYTES_BANK: {
		u8 *pu1Tmp;
		pu1Tmp = (u8 *)pOut;
		if (efuseType == EFUSE_WIFI)
			*pu1Tmp = (u8)(EFUSE_OOB_PROTECT_BYTES_JAGUAR);
		else
			*pu1Tmp = EFUSE_PROTECT_BYTES_BANK;
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
VOID
Hal_EFUSEGetEfuseDefinition_Pseudo8812A(
	IN		PADAPTER	pAdapter,
	IN		u8			efuseType,
	IN		u8			type,
	OUT		PVOID		pOut
)
{
	switch (type) {
	case TYPE_EFUSE_MAX_SECTION: {
		u8		*pMax_section;
		pMax_section = (pu1Byte)pOut;
		*pMax_section = EFUSE_MAX_SECTION_JAGUAR;
	}
		break;
	case TYPE_EFUSE_REAL_CONTENT_LEN: {
		u16 *pu2Tmp;
		pu2Tmp = (pu2Byte)pOut;
		*pu2Tmp = EFUSE_REAL_CONTENT_LEN_JAGUAR;
	}
		break;
	case TYPE_EFUSE_CONTENT_LEN_BANK: {
		u16 *pu2Tmp;
		pu2Tmp = (pu2Byte)pOut;
		*pu2Tmp = EFUSE_REAL_CONTENT_LEN_JAGUAR;
	}
		break;
	case TYPE_AVAILABLE_EFUSE_BYTES_BANK: {
		u16 *pu2Tmp;
		pu2Tmp = (pu2Byte)pOut;
		*pu2Tmp = (u2Byte)(EFUSE_REAL_CONTENT_LEN_JAGUAR - EFUSE_OOB_PROTECT_BYTES_JAGUAR);
	}
		break;
	case TYPE_AVAILABLE_EFUSE_BYTES_TOTAL: {
		u16 *pu2Tmp;
		pu2Tmp = (pu2Byte)pOut;
		*pu2Tmp = (u2Byte)(EFUSE_REAL_CONTENT_LEN_JAGUAR - EFUSE_OOB_PROTECT_BYTES_JAGUAR);
	}
		break;
	case TYPE_EFUSE_MAP_LEN: {
		u16 *pu2Tmp;
		pu2Tmp = (pu2Byte)pOut;
		*pu2Tmp = (u2Byte)EFUSE_MAP_LEN_JAGUAR;
	}
		break;
	case TYPE_EFUSE_PROTECT_BYTES_BANK: {
		u8 *pu1Tmp;
		pu1Tmp = (u8 *)pOut;
		*pu1Tmp = (u8)(EFUSE_OOB_PROTECT_BYTES_JAGUAR);
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


static VOID
rtl8812_EFUSE_GetEfuseDefinition(
	IN		PADAPTER	pAdapter,
	IN		u8		efuseType,
	IN		u8		type,
	OUT		void		*pOut,
	IN		BOOLEAN		bPseudoTest
)
{
	if (bPseudoTest)
		Hal_EFUSEGetEfuseDefinition_Pseudo8812A(pAdapter, efuseType, type, pOut);
	else
		Hal_EFUSEGetEfuseDefinition8812A(pAdapter, efuseType, type, pOut);
}

static u8
Hal_EfuseWordEnableDataWrite8812A(IN	PADAPTER	pAdapter,
				  IN	u16		efuse_addr,
				  IN	u8		word_en,
				  IN	u8		*data,
				  IN	BOOLEAN		bPseudoTest)
{
	u16	tmpaddr = 0;
	u16	start_addr = efuse_addr;
	u8	badworden = 0x0F;
	u8	tmpdata[8];

	_rtw_memset((PVOID)tmpdata, 0xff, PGPKT_DATA_SIZE);

	if (!(word_en & BIT0)) {
		tmpaddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[0], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[1], bPseudoTest);

		efuse_OneByteRead(pAdapter, tmpaddr, &tmpdata[0], bPseudoTest);
		efuse_OneByteRead(pAdapter, tmpaddr + 1, &tmpdata[1], bPseudoTest);
		if ((data[0] != tmpdata[0]) || (data[1] != tmpdata[1]))
			badworden &= (~BIT0);
	}
	if (!(word_en & BIT1)) {
		tmpaddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[2], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[3], bPseudoTest);

		efuse_OneByteRead(pAdapter, tmpaddr    , &tmpdata[2], bPseudoTest);
		efuse_OneByteRead(pAdapter, tmpaddr + 1, &tmpdata[3], bPseudoTest);
		if ((data[2] != tmpdata[2]) || (data[3] != tmpdata[3]))
			badworden &= (~BIT1);
	}
	if (!(word_en & BIT2)) {
		tmpaddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[4], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[5], bPseudoTest);

		efuse_OneByteRead(pAdapter, tmpaddr, &tmpdata[4], bPseudoTest);
		efuse_OneByteRead(pAdapter, tmpaddr + 1, &tmpdata[5], bPseudoTest);
		if ((data[4] != tmpdata[4]) || (data[5] != tmpdata[5]))
			badworden &= (~BIT2);
	}
	if (!(word_en & BIT3)) {
		tmpaddr = start_addr;
		efuse_OneByteWrite(pAdapter, start_addr++, data[6], bPseudoTest);
		efuse_OneByteWrite(pAdapter, start_addr++, data[7], bPseudoTest);

		efuse_OneByteRead(pAdapter, tmpaddr, &tmpdata[6], bPseudoTest);
		efuse_OneByteRead(pAdapter, tmpaddr + 1, &tmpdata[7], bPseudoTest);
		if ((data[6] != tmpdata[6]) || (data[7] != tmpdata[7]))
			badworden &= (~BIT3);
	}
	return badworden;
}

static u8
rtl8812_Efuse_WordEnableDataWrite(IN	PADAPTER	pAdapter,
				  IN	u16		efuse_addr,
				  IN	u8		word_en,
				  IN	u8		*data,
				  IN	BOOLEAN		bPseudoTest)
{
	u8	ret = 0;

	ret = Hal_EfuseWordEnableDataWrite8812A(pAdapter, efuse_addr, word_en, data, bPseudoTest);

	return ret;
}


static u16
hal_EfuseGetCurrentSize_8812A(IN	PADAPTER	pAdapter,
			      IN		BOOLEAN			bPseudoTest)
{
	int	bContinual = _TRUE;

	u16	efuse_addr = 0;
	u8	hoffset = 0, hworden = 0;
	u8	efuse_data, word_cnts = 0;

	if (bPseudoTest)
		efuse_addr = (u16)(fakeEfuseUsedBytes);
	else
		rtw_hal_get_hwreg(pAdapter, HW_VAR_EFUSE_BYTES, (u8 *)&efuse_addr);
	RTW_INFO("%s(), start_efuse_addr = %d\n", __func__, efuse_addr);

	while (bContinual &&
	       efuse_OneByteRead(pAdapter, efuse_addr , &efuse_data, bPseudoTest) &&
	       (efuse_addr  < EFUSE_REAL_CONTENT_LEN_JAGUAR)) {
		if (efuse_data != 0xFF) {
			if ((efuse_data & 0x1F) == 0x0F) {	/* extended header */
				hoffset = efuse_data;
				efuse_addr++;
				efuse_OneByteRead(pAdapter, efuse_addr , &efuse_data, bPseudoTest);
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
			word_cnts = Efuse_CalculateWordCnts(hworden);
			/* read next header */
			efuse_addr = efuse_addr + (word_cnts * 2) + 1;
		} else
			bContinual = _FALSE ;
	}

	if (bPseudoTest) {
		fakeEfuseUsedBytes = efuse_addr;
		RTW_INFO("%s(), return %d\n", __func__, fakeEfuseUsedBytes);
	} else {
		rtw_hal_set_hwreg(pAdapter, HW_VAR_EFUSE_BYTES, (u8 *)&efuse_addr);
		RTW_INFO("%s(), return %d\n", __func__, efuse_addr);
	}

	return efuse_addr;
}

static u16
hal_EfuseGetCurrentSize_BT(
	PADAPTER	padapter,
	u8			bPseudoTest)
{
#ifdef HAL_EFUSE_MEMORY
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	PEFUSE_HAL		pEfuseHal = &pHalData->EfuseHal;
#endif
	u16 btusedbytes;
	u16	efuse_addr;
	u8	bank, startBank;
	u8	hoffset = 0, hworden = 0;
	u8	efuse_data, word_cnts = 0;
	u16	retU2 = 0;
	u8 bContinual = _TRUE;


	if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
		btusedbytes = pEfuseHal->fakeBTEfuseUsedBytes;
#else
		btusedbytes = fakeBTEfuseUsedBytes;
#endif
	} else {
		btusedbytes = 0;
		rtw_hal_get_hwreg(padapter, HW_VAR_EFUSE_BT_BYTES, (u8 *)&btusedbytes);
	}
	efuse_addr = (u16)((btusedbytes % EFUSE_BT_REAL_BANK_CONTENT_LEN));
	startBank = (u8)(1 + (btusedbytes / EFUSE_BT_REAL_BANK_CONTENT_LEN));

	RTW_INFO("%s: start from bank=%d addr=0x%X\n", __FUNCTION__, startBank, efuse_addr);

	EFUSE_GetEfuseDefinition(padapter, EFUSE_BT, TYPE_AVAILABLE_EFUSE_BYTES_BANK, &retU2, bPseudoTest);

	for (bank = startBank; bank < 3; bank++) {
		if (hal_EfuseSwitchToBank(padapter, bank, bPseudoTest) == _FALSE) {
			RTW_INFO(KERN_ERR "%s: switch bank(%d) Fail!!\n", __FUNCTION__, bank);
			/* bank = EFUSE_MAX_BANK; */
			break;
		}

		/* only when bank is switched we have to reset the efuse_addr. */
		if (bank != startBank)
			efuse_addr = 0;

		while (AVAILABLE_EFUSE_ADDR(efuse_addr)) {
			if (efuse_OneByteRead(padapter, efuse_addr, &efuse_data, bPseudoTest) == _FALSE) {
				RTW_INFO(KERN_ERR "%s: efuse_OneByteRead Fail! addr=0x%X !!\n", __FUNCTION__, efuse_addr);
				/* bank = EFUSE_MAX_BANK; */
				break;
			}
			RTW_INFO("%s: efuse_OneByteRead ! addr=0x%X !efuse_data=0x%X! bank =%d\n", __FUNCTION__, efuse_addr, efuse_data, bank);

			if (efuse_data == 0xFF)
				break;

			if (EXT_HEADER(efuse_data)) {
				hoffset = GET_HDR_OFFSET_2_0(efuse_data);
				efuse_addr++;
				efuse_OneByteRead(padapter, efuse_addr, &efuse_data, bPseudoTest);
				RTW_INFO("%s: efuse_OneByteRead EXT_HEADER ! addr=0x%X !efuse_data=0x%X! bank =%d\n", __FUNCTION__, efuse_addr, efuse_data, bank);

				if (ALL_WORDS_DISABLED(efuse_data)) {
					efuse_addr++;
					continue;
				}

				/*				hoffset = ((hoffset & 0xE0) >> 5) | ((efuse_data & 0xF0) >> 1); */
				hoffset |= ((efuse_data & 0xF0) >> 1);
				hworden = efuse_data & 0x0F;
			} else {
				hoffset = (efuse_data >> 4) & 0x0F;
				hworden =  efuse_data & 0x0F;
			}

			RTW_INFO(FUNC_ADPT_FMT": Offset=%d Worden=%#X\n",
				 FUNC_ADPT_ARG(padapter), hoffset, hworden);

			word_cnts = Efuse_CalculateWordCnts(hworden);
			/* read next header */
			efuse_addr += (word_cnts * 2) + 1;
		}
		/* Check if we need to check next bank efuse */
		if (efuse_addr < retU2)
			break;/* don't need to check next bank. */
	}

	retU2 = ((bank - 1) * EFUSE_BT_REAL_BANK_CONTENT_LEN) + efuse_addr;
	if (bPseudoTest) {
		pEfuseHal->fakeBTEfuseUsedBytes = retU2;
		/* RT_DISP(FEEPROM, EFUSE_PG, ("Hal_EfuseGetCurrentSize_BT92C(), already use %u bytes\n", pEfuseHal->fakeBTEfuseUsedBytes)); */
	} else {
		pEfuseHal->BTEfuseUsedBytes = retU2;
		/* RT_DISP(FEEPROM, EFUSE_PG, ("Hal_EfuseGetCurrentSize_BT92C(), already use %u bytes\n", pEfuseHal->BTEfuseUsedBytes)); */
	}

	RTW_INFO("%s: CurrentSize=%d\n", __FUNCTION__, retU2);
	return retU2;
}


static u16
rtl8812_EfuseGetCurrentSize(
	IN	PADAPTER	pAdapter,
	IN	u8			efuseType,
	IN	BOOLEAN		bPseudoTest)
{
	u16	ret = 0;

	if (efuseType == EFUSE_WIFI)
		ret = hal_EfuseGetCurrentSize_8812A(pAdapter, bPseudoTest);
	else
		ret = hal_EfuseGetCurrentSize_BT(pAdapter, bPseudoTest);

	return ret;
}


static int
hal_EfusePgPacketRead_8812A(
	IN	PADAPTER	pAdapter,
	IN	u8			offset,
	IN	u8			*data,
	IN	BOOLEAN		bPseudoTest)
{
	u8	ReadState = PG_STATE_HEADER;

	int	bContinual = _TRUE;
	int	bDataEmpty = _TRUE ;

	u8	efuse_data, word_cnts = 0;
	u16	efuse_addr = 0;
	u8	hoffset = 0, hworden = 0;
	u8	tmpidx = 0;
	u8	tmpdata[8];
	u8	max_section = 0;
	u8	tmp_header = 0;

	if (data == NULL)
		return _FALSE;
	if (offset > EFUSE_MAX_SECTION_JAGUAR)
		return _FALSE;

	_rtw_memset((PVOID)data, 0xff, sizeof(u8) * PGPKT_DATA_SIZE);
	_rtw_memset((PVOID)tmpdata, 0xff, sizeof(u8) * PGPKT_DATA_SIZE);


	/*  */
	/* <Roger_TODO> Efuse has been pre-programmed dummy 5Bytes at the end of Efuse by CP. */
	/* Skip dummy parts to prevent unexpected data read from Efuse. */
	/* By pass right now. 2009.02.19. */
	/*  */
	while (bContinual && (efuse_addr  < EFUSE_REAL_CONTENT_LEN_JAGUAR)) {
		/* -------  Header Read ------------- */
		if (ReadState & PG_STATE_HEADER) {
			if (efuse_OneByteRead(pAdapter, efuse_addr , &efuse_data, bPseudoTest) && (efuse_data != 0xFF)) {
				if (EXT_HEADER(efuse_data)) {
					tmp_header = efuse_data;
					efuse_addr++;
					efuse_OneByteRead(pAdapter, efuse_addr , &efuse_data, bPseudoTest);
					if (!ALL_WORDS_DISABLED(efuse_data)) {
						hoffset = ((tmp_header & 0xE0) >> 5) | ((efuse_data & 0xF0) >> 1);
						hworden = efuse_data & 0x0F;
					} else {
						RTW_INFO("Error, All words disabled\n");
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
					else { /* read next header */
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
rtl8812_Efuse_PgPacketRead(IN	PADAPTER	pAdapter,
			   IN	u8			offset,
			   IN	u8			*data,
			   IN	BOOLEAN		bPseudoTest)
{
	int	ret = 0;

	ret = hal_EfusePgPacketRead_8812A(pAdapter, offset, data, bPseudoTest);

	return ret;
}


BOOLEAN
hal_EfuseFixHeaderProcess(
	IN		PADAPTER			pAdapter,
	IN		u8				efuseType,
	IN		PPGPKT_STRUCT		pFixPkt,
	IN		u16				*pAddr,
	IN		BOOLEAN				bPseudoTest
)
{
	u8	originaldata[8], badworden = 0;
	u16	efuse_addr = *pAddr;
	u32	PgWriteSuccess = 0;

	_rtw_memset((PVOID)originaldata, 0xff, 8);

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


static u8
hal_EfusePgPacketWrite2ByteHeader(
	PADAPTER		padapter,
	u8				efuseType,
	u16				*pAddr,
	PPGPKT_STRUCT	pTargetPkt,
	u8				bPseudoTest)
{
	u16	efuse_addr, efuse_max_available_len = 0;
	u8	pg_header = 0, tmp_header = 0, pg_header_temp = 0;
	u8	repeatcnt = 0;


	/*	RTW_INFO("%s\n", __FUNCTION__); */
	EFUSE_GetEfuseDefinition(padapter, efuseType, TYPE_AVAILABLE_EFUSE_BYTES_BANK, &efuse_max_available_len, bPseudoTest);

	efuse_addr = *pAddr;

	if (efuse_addr >= efuse_max_available_len) {
		RTW_INFO("%s: addr(%d) over available(%d)!!\n", __FUNCTION__, efuse_addr, efuse_max_available_len);
		return _FALSE;
	}

	while (efuse_addr < efuse_max_available_len) {
		pg_header = ((pTargetPkt->offset & 0x07) << 5) | 0x0F;
		efuse_OneByteWrite(padapter, efuse_addr, pg_header, bPseudoTest);
		efuse_OneByteRead(padapter, efuse_addr, &tmp_header, bPseudoTest);


		while (tmp_header == 0xFF || pg_header != tmp_header) {
			if (repeatcnt++ > EFUSE_REPEAT_THRESHOLD_) {
				RTW_INFO("%s, Repeat over limit for pg_header!!\n", __FUNCTION__);
				return _FALSE;
			}

			efuse_OneByteWrite(padapter, efuse_addr, pg_header, bPseudoTest);
			efuse_OneByteRead(padapter, efuse_addr, &tmp_header, bPseudoTest);
		}

		/*to write ext_header*/
		if (tmp_header == pg_header) {
			efuse_addr++;
			pg_header_temp = pg_header;
			pg_header = ((pTargetPkt->offset & 0x78) << 1) | pTargetPkt->word_en;

			efuse_OneByteWrite(padapter, efuse_addr, pg_header, bPseudoTest);
			efuse_OneByteRead(padapter, efuse_addr, &tmp_header, bPseudoTest);


			while (tmp_header == 0xFF || pg_header != tmp_header) {
				if (repeatcnt++ > EFUSE_REPEAT_THRESHOLD_) {
					RTW_INFO("%s, Repeat over limit for ext_header!!\n", __FUNCTION__);
					return _FALSE;
				}

				efuse_OneByteWrite(padapter, efuse_addr, pg_header, bPseudoTest);
				efuse_OneByteRead(padapter, efuse_addr, &tmp_header, bPseudoTest);
			}

			if ((tmp_header & 0x0F) == 0x0F) {
				if (repeatcnt++ > EFUSE_REPEAT_THRESHOLD_) {
					RTW_INFO("Repeat over limit for word_en!!\n");
					return _FALSE;

				} else {
					efuse_addr++;
					continue;
				}
			} else if (pg_header != tmp_header) {
				PGPKT_STRUCT	fixPkt;

				RTW_ERR("Error, efuse_PgPacketWrite2ByteHeader(), offset PG fail, need to cover the existed data!!\n");
				RTW_ERR("Error condition for offset PG fail, need to cover the existed data\n");
				fixPkt.offset = ((pg_header_temp & 0xE0) >> 5) | ((tmp_header & 0xF0) >> 1);
				fixPkt.word_en = tmp_header & 0x0F;
				fixPkt.word_cnts = Efuse_CalculateWordCnts(fixPkt.word_en);
				if (!hal_EfuseFixHeaderProcess(padapter, efuseType, &fixPkt, &efuse_addr, bPseudoTest))
					return _FALSE;
			} else
				break;
		} else if ((tmp_header & 0x1F) == 0x0F) {/*wrong extended header*/
			efuse_addr += 2;
			continue;
		}
	}

	*pAddr = efuse_addr;

	return _TRUE;
}


static u8
hal_EfusePgPacketWrite1ByteHeader(
	PADAPTER		pAdapter,
	u8				efuseType,
	u16				*pAddr,
	PPGPKT_STRUCT	pTargetPkt,
	u8				bPseudoTest)
{
	u8	bRet = _FALSE;
	u8	pg_header = 0, tmp_header = 0;
	u16	efuse_addr = *pAddr;
	u8	repeatcnt = 0;


	RTW_INFO("%s\n", __func__);
	pg_header = ((pTargetPkt->offset << 4) & 0xf0) | pTargetPkt->word_en;

	efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);

	efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);

	while (tmp_header == 0xFF || pg_header != tmp_header) {
		if (repeatcnt++ > EFUSE_REPEAT_THRESHOLD_) {
			RTW_INFO("retry %d times fail!!\n", repeatcnt);
			return FALSE;
		}
		efuse_OneByteWrite(pAdapter, efuse_addr, pg_header, bPseudoTest);
		efuse_OneByteRead(pAdapter, efuse_addr, &tmp_header, bPseudoTest);
		RTW_INFO("===> %s: Keep %d-th retrying, tmp_header = 0x%X\n", __func__, repeatcnt, tmp_header);
	}

	if (tmp_header != pg_header) {
		PGPKT_STRUCT	fixPkt;

		RTW_INFO("Error, %s(), offset PG fail, need to cover the existed data!!\n", __func__);
		RTW_INFO("pg_header(0x%X) != tmp_header(0x%X)\n", pg_header, tmp_header);
		RTW_INFO("Error condition for fixed PG packet, need to cover the existed data: (Addr, Data) = (0x%X, 0x%X)\n",
							efuse_addr, tmp_header);
		fixPkt.offset = (tmp_header>>4) & 0x0F;
		fixPkt.word_en = tmp_header & 0x0F;
		fixPkt.word_cnts = Efuse_CalculateWordCnts(fixPkt.word_en);
		if (!hal_EfuseFixHeaderProcess(pAdapter, efuseType, &fixPkt, &efuse_addr, bPseudoTest))
			return FALSE;
	}

	*pAddr = efuse_addr;

	return _TRUE;
}


static u8
hal_EfusePgPacketWriteData(
	PADAPTER		pAdapter,
	u8				efuseType,
	u16				*pAddr,
	PPGPKT_STRUCT	pTargetPkt,
	u8				bPseudoTest)
{
	u16	efuse_addr;
	u8	badworden;


	efuse_addr = *pAddr;
	badworden = rtl8812_Efuse_WordEnableDataWrite(pAdapter, efuse_addr + 1, pTargetPkt->word_en, pTargetPkt->data, bPseudoTest);
	if (badworden != 0x0F) {
		RTW_INFO("%s: Fail!!\n", __FUNCTION__);
		return _FALSE;
	}

	/*	RTW_INFO("%s: ok\n", __FUNCTION__); */
	return _TRUE;
}

BOOLEAN efuse_PgPacketCheck(
	PADAPTER	pAdapter,
	u8		efuseType,
	BOOLEAN		bPseudoTest
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);

	if (Efuse_GetCurrentSize(pAdapter, efuseType, bPseudoTest) >= (EFUSE_REAL_CONTENT_LEN_JAGUAR - EFUSE_OOB_PROTECT_BYTES_JAGUAR)) {
		RTW_INFO("%s()error: %x >= %x\n", __func__, Efuse_GetCurrentSize(pAdapter, efuseType, bPseudoTest), (EFUSE_REAL_CONTENT_LEN_JAGUAR - EFUSE_OOB_PROTECT_BYTES_JAGUAR));
		return _FALSE;
	}

	return _TRUE;
}


VOID
efuse_PgPacketConstruct(
	IN	    u8			offset,
	IN	    u8			word_en,
	IN	    u8			*pData,
	IN OUT	PPGPKT_STRUCT	pTargetPkt
)
{
	_rtw_memset((PVOID)pTargetPkt->data, 0xFF, sizeof(u8) * 8);
	pTargetPkt->offset = offset;
	pTargetPkt->word_en = word_en;
	efuse_WordEnableDataRead(word_en, pData, pTargetPkt->data);
	pTargetPkt->word_cnts = Efuse_CalculateWordCnts(pTargetPkt->word_en);

	RTW_INFO("efuse_PgPacketConstruct(), targetPkt, offset=%d, word_en=0x%x, word_cnts=%d\n", pTargetPkt->offset, pTargetPkt->word_en, pTargetPkt->word_cnts);
}

u2Byte
Hal_EfusePgPacketExceptionHandle_8812A(
		PADAPTER	pAdapter,
		u2Byte		ErrOffset
		)
{
	BOOLEAN bPseudoTest = FALSE;
	u8 next = 0, next_next = 0, data = 0, i = 0, header = 0;
	u8 s = 0;
	u2Byte offset = ErrOffset;

	efuse_OneByteRead(pAdapter, offset, &header, bPseudoTest);

	if (EXT_HEADER(header)) {
		s = ((header & 0xF0) >> 4);

		/* Skip bad word enable to look two bytes ahead and determine if recoverable.*/
		offset += 1;
		efuse_OneByteRead(pAdapter, offset+1, &next, bPseudoTest);
		efuse_OneByteRead(pAdapter, offset+2, &next_next, bPseudoTest);
		if (next == 0xFF && next_next == 0xFF) {/* Have enough space to make fake data to recover bad header.*/
			switch (s) {
			case 0x0: case 0x2: case 0x4: case 0x6:
			case 0x8: case 0xA: case 0xC:
					for (i = 0; i < 3; ++i) {
						efuse_OneByteWrite(pAdapter, offset, 0x27, bPseudoTest);
						efuse_OneByteRead(pAdapter, offset, &data, bPseudoTest);
						if (data == 0x27)
							break;
					}
					break;
			case 0xE:
					for (i = 0; i < 3; ++i) {
						efuse_OneByteWrite(pAdapter, offset, 0x17, bPseudoTest);
						efuse_OneByteRead(pAdapter, offset, &data, bPseudoTest);
						if (data == 0x17)
							break;

					break;
			default:
					break;
			}
			efuse_OneByteWrite(pAdapter, offset+1, 0xFF, bPseudoTest);
			efuse_OneByteWrite(pAdapter, offset+2, 0xFF, bPseudoTest);
			offset += 3;
			ErrOffset = offset;
		}
	} else {/* 1-Byte header*/
		if (ALL_WORDS_DISABLED(header)) {
			u8 next = 0;

			efuse_OneByteRead(pAdapter, offset+1, &next, bPseudoTest);
			if (next == 0xFF) {/* Have enough space to make fake data to recover bad header.*/
				header = (header & 0xF0) | 0x7;
				for (i = 0; i < 3; ++i) {
					efuse_OneByteWrite(pAdapter, offset, header, bPseudoTest);
					efuse_OneByteRead(pAdapter, offset, &data, bPseudoTest);
					if (data == header)
						break;
				}
				efuse_OneByteWrite(pAdapter, offset+1, 0xFF, bPseudoTest);
				efuse_OneByteWrite(pAdapter, offset+2, 0xFF, bPseudoTest);
				offset += 2;
				ErrOffset = offset;
				}
		}
	}
	}

	return ErrOffset;
}


BOOLEAN hal_EfuseCheckIfDatafollowed(
	IN		PADAPTER		pAdapter,
	IN		u8			word_cnts,
	IN		u16			startAddr,
	IN		BOOLEAN			bPseudoTest
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
	IN	PPGPKT_STRUCT	pTargetPkt,
	IN	PPGPKT_STRUCT	pCurPkt,
	IN	u8			*pWden
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
	IN	    PADAPTER		pAdapter,
	IN	    u8			efuseType,
	IN OUT	u16			*pAddr,
	IN	    PPGPKT_STRUCT	pTargetPkt,
	IN	    BOOLEAN			bPseudoTest
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

	RTW_INFO("efuse_PgPacketPartialWrite()\n");

	if (bPseudoTest) {
		pEfuseHal->fakeEfuseBank = (efuseType == EFUSE_WIFI) ? 0 : pEfuseHal->fakeEfuseBank;
		Efuse_GetCurrentSize(pAdapter, efuseType, _TRUE);
	}

	 EFUSE_GetEfuseDefinition(pAdapter, efuseType, TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, &efuse_max_available_len, bPseudoTest);
	 EFUSE_GetEfuseDefinition(pAdapter, efuseType, TYPE_EFUSE_CONTENT_LEN_BANK, &efuse_max, bPseudoTest);

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

					recoveredAddr = Hal_EfusePgPacketExceptionHandle_8812A(pAdapter, startAddr - 1);

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

					recoveredAddr = Hal_EfusePgPacketExceptionHandle_8812A(pAdapter, startAddr);
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

BOOLEAN	efuse_PgPacketWriteHeader(
	PADAPTER		pAdapter,
	u8			efuseType,
	u16			*pAddr,
	PPGPKT_STRUCT	pTargetPkt,
	BOOLEAN			bPseudoTest)
{
	BOOLEAN		bRet = _FALSE;

	if (pTargetPkt->offset >= EFUSE_MAX_SECTION_BASE)
		bRet = hal_EfusePgPacketWrite2ByteHeader(pAdapter, efuseType, pAddr, pTargetPkt, bPseudoTest);
	else
		bRet = hal_EfusePgPacketWrite1ByteHeader(pAdapter, efuseType, pAddr, pTargetPkt, bPseudoTest);

	return bRet;
}

int
hal_EfusePgPacketWrite_8812A(IN	PADAPTER	pAdapter,
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
	if (!hal_EfusePgPacketWriteData(pAdapter, efuseType, &startAddr, &targetPkt, bPseudoTest)) {
		pAdapter->LastError = ERR_IO_FAILURE;
		RTW_INFO("efuse_PgPacketWriteData(), fail!!\n");
		return _FALSE;
	}

	RTW_INFO("<=== efuse_PgPacketWrite\n");
	return _TRUE;
}

static VOID
rtl8812_EfusePowerSwitch(
	IN	PADAPTER	pAdapter,
	IN	u8		bWrite,
	IN	u8		PwrState)
{

	Hal_EfusePowerSwitch8812A(pAdapter, bWrite, PwrState);

}

static int
rtl8812_Efuse_PgPacketWrite(IN	PADAPTER	pAdapter,
			    IN	u8			offset,
			    IN	u8			word_en,
			    IN	u8			*data,
			    IN	BOOLEAN		bPseudoTest)
{
	int		ret;

	ret = hal_EfusePgPacketWrite_8812A(pAdapter, offset, word_en, data, bPseudoTest);

	return ret;
}

static u8
hal_EfusePgCheckAvailableAddr(
	PADAPTER	pAdapter,
	u8			efuseType,
	u8		bPseudoTest)
{
	u16	max_available = 0;
	u16 current_size;


	EFUSE_GetEfuseDefinition(pAdapter, efuseType, TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, &max_available, bPseudoTest);
	/*	RTW_INFO("%s: max_available=%d\n", __FUNCTION__, max_available); */

	current_size = Efuse_GetCurrentSize(pAdapter, efuseType, bPseudoTest);
	if (current_size >= max_available) {
		RTW_INFO("%s: Error!! current_size(%d)>max_available(%d)\n", __FUNCTION__, current_size, max_available);
		return _FALSE;
	}
	return _TRUE;
}

static void
hal_EfuseConstructPGPkt(
	u8				offset,
	u8				word_en,
	u8				*pData,
	PPGPKT_STRUCT	pTargetPkt)
{
	_rtw_memset(pTargetPkt->data, 0xFF, PGPKT_DATA_SIZE);
	pTargetPkt->offset = offset;
	pTargetPkt->word_en = word_en;
	efuse_WordEnableDataRead(word_en, pData, pTargetPkt->data);
	pTargetPkt->word_cnts = Efuse_CalculateWordCnts(pTargetPkt->word_en);
}

static u8
hal_EfusePartialWriteCheck(
	PADAPTER		padapter,
	u8				efuseType,
	u16				*pAddr,
	PPGPKT_STRUCT	pTargetPkt,
	u8				bPseudoTest)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);
	PEFUSE_HAL		pEfuseHal = &pHalData->EfuseHal;
	u8	bRet = _FALSE;
	u16	startAddr = 0, efuse_max_available_len = 0, efuse_max = 0;
	u8	efuse_data = 0;

	EFUSE_GetEfuseDefinition(padapter, efuseType, TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, &efuse_max_available_len, bPseudoTest);
	EFUSE_GetEfuseDefinition(padapter, efuseType, TYPE_EFUSE_CONTENT_LEN_BANK, &efuse_max, bPseudoTest);

	if (efuseType == EFUSE_WIFI) {
		if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
			startAddr = (u16)pEfuseHal->fakeEfuseUsedBytes;
#else
			startAddr = (u16)fakeEfuseUsedBytes;
#endif
		} else
			rtw_hal_get_hwreg(padapter, HW_VAR_EFUSE_BYTES, (u8 *)&startAddr);
	} else {
		if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
			startAddr = (u16)pEfuseHal->fakeBTEfuseUsedBytes;
#else
			startAddr = (u16)fakeBTEfuseUsedBytes;
#endif
		} else
			rtw_hal_get_hwreg(padapter, HW_VAR_EFUSE_BT_BYTES, (u8 *)&startAddr);
	}
	startAddr %= efuse_max;
	RTW_INFO("%s: startAddr=%#X\n", __FUNCTION__, startAddr);

	while (1) {
		if (startAddr >= efuse_max_available_len) {
			bRet = _FALSE;
			RTW_INFO("%s: startAddr(%d) >= efuse_max_available_len(%d)\n",
				__FUNCTION__, startAddr, efuse_max_available_len);
			break;
		}

		if (efuse_OneByteRead(padapter, startAddr, &efuse_data, bPseudoTest) && (efuse_data != 0xFF)) {
			bRet = _FALSE;
			RTW_INFO("%s: Something Wrong! last bytes(%#X=0x%02X) is not 0xFF\n",
				 __FUNCTION__, startAddr, efuse_data);
			break;
		} else {
			/* not used header, 0xff */
			*pAddr = startAddr;
			/* RTW_INFO("%s: Started from unused header offset=%d\n", __FUNCTION__, startAddr)); */
			bRet = _TRUE;
			break;
		}
	}

	return bRet;
}

static u8
hal_EfusePgPacketWriteHeader(
	PADAPTER		padapter,
	u8				efuseType,
	u16				*pAddr,
	PPGPKT_STRUCT	pTargetPkt,
	u8				bPseudoTest)
{
	u8 bRet = _FALSE;

	if (pTargetPkt->offset >= EFUSE_MAX_SECTION_BASE)
		bRet = hal_EfusePgPacketWrite2ByteHeader(padapter, efuseType, pAddr, pTargetPkt, bPseudoTest);
	else
		bRet = hal_EfusePgPacketWrite1ByteHeader(padapter, efuseType, pAddr, pTargetPkt, bPseudoTest);

	return bRet;
}


static u8
Hal_EfusePgPacketWrite_BT(
	PADAPTER	pAdapter,
	u8			offset,
	u8			word_en,
	u8			*pData,
	u8			bPseudoTest)
{
	PGPKT_STRUCT targetPkt;
	u16 startAddr = 0;
	u8 efuseType = EFUSE_BT;

	if (!hal_EfusePgCheckAvailableAddr(pAdapter, efuseType, bPseudoTest))
		return _FALSE;

	hal_EfuseConstructPGPkt(offset, word_en, pData, &targetPkt);

	if (!hal_EfusePartialWriteCheck(pAdapter, efuseType, &startAddr, &targetPkt, bPseudoTest))
		return _FALSE;

	if (!hal_EfusePgPacketWriteHeader(pAdapter, efuseType, &startAddr, &targetPkt, bPseudoTest))
		return _FALSE;

	if (!hal_EfusePgPacketWriteData(pAdapter, efuseType, &startAddr, &targetPkt, bPseudoTest))
		return _FALSE;

	return _TRUE;
}


void InitRDGSetting8812A(PADAPTER padapter)
{
	rtw_write8(padapter, REG_RD_CTRL, 0xFF);
	rtw_write16(padapter, REG_RD_NAV_NXT, 0x200);
	rtw_write8(padapter, REG_RD_RESP_PKT_TH, 0x05);
}

void ReadRFType8812A(PADAPTER padapter)
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


void rtl8812_start_thread(PADAPTER padapter)
{
}

void rtl8812_stop_thread(PADAPTER padapter)
{
}

void hal_notch_filter_8812(_adapter *adapter, bool enable)
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
GetEEPROMSize8812A(
	IN	PADAPTER	Adapter
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

void CheckAutoloadState8812A(PADAPTER padapter)
{
	u8 val8;
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

	/* check system boot selection */
	val8 = rtw_read8(padapter, REG_9346CR);
	pHalData->EepromOrEfuse = (val8 & BOOT_FROM_EEPROM) ? _TRUE : _FALSE;
	pHalData->bautoload_fail_flag = (val8 & EEPROM_EN) ? _FALSE : _TRUE;

	RTW_INFO("%s: 9346CR(%#x)=0x%02x, Boot from %s, Autoload %s!\n",
		 __FUNCTION__, REG_9346CR, val8,
		 (pHalData->EepromOrEfuse ? "EEPROM" : "EFUSE"),
		 (pHalData->bautoload_fail_flag ? "Fail" : "OK"));
}

void InitPGData8812A(PADAPTER padapter)
{
	u32 i;
	u16 val16;
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

	if (_FALSE == pHalData->bautoload_fail_flag) {
		/* autoload OK. */
		if (is_boot_from_eeprom(padapter)) {
			/* Read all Content from EEPROM or EFUSE. */
			for (i = 0; i < HWSET_MAX_SIZE_JAGUAR; i += 2) {
				/* val16 = EF2Byte(ReadEEprom(pAdapter, (u2Byte) (i>>1))); */
				/* *((u16*)(&PROMContent[i])) = val16; */
			}
		} else {
			/* Read EFUSE real map to shadow. */
			EFUSE_ShadowMapUpdate(padapter, EFUSE_WIFI, _FALSE);
		}
	} else {
		/* update to default value 0xFF */
		if (!is_boot_from_eeprom(padapter))
			EFUSE_ShadowMapUpdate(padapter, EFUSE_WIFI, _FALSE);
	}
#ifdef CONFIG_EFUSE_CONFIG_FILE
	if (check_phy_efuse_tx_power_info_valid(padapter) == _FALSE) {
		if (Hal_readPGDataFromConfigFile(padapter) != _SUCCESS)
			RTW_ERR("invalid phy efuse and read from file fail, will use driver default!!\n");
	}
#endif
}

static void read_chip_version_8812a(PADAPTER Adapter)
{
	u32	value32;
	PHAL_DATA_TYPE	pHalData;
	pHalData = GET_HAL_DATA(Adapter);

	value32 = rtw_read32(Adapter, REG_SYS_CFG);
	RTW_INFO("%s SYS_CFG(0x%X)=0x%08x\n", __FUNCTION__, REG_SYS_CFG, value32);

	if (IS_HARDWARE_TYPE_8812(Adapter))
		pHalData->version_id.ICType = CHIP_8812;
	else
		pHalData->version_id.ICType = CHIP_8821;

	pHalData->version_id.ChipType = ((value32 & RTL_ID) ? TEST_CHIP : NORMAL_CHIP);

	if (IS_HARDWARE_TYPE_8812(Adapter))
		pHalData->version_id.RFType = RF_TYPE_2T2R; /* RF_2T2R; */
	else
		pHalData->version_id.RFType = RF_TYPE_1T1R; /* RF_1T1R; */

	if (Adapter->registrypriv.special_rf_path == 1)
		pHalData->version_id.RFType = RF_TYPE_1T1R;	/* RF_1T1R; */

	if (IS_HARDWARE_TYPE_8812(Adapter))
		pHalData->version_id.VendorType = ((value32 & VENDOR_ID) ? CHIP_VENDOR_UMC : CHIP_VENDOR_TSMC);
	else {
		u32 vendor;

		vendor = (value32 & EXT_VENDOR_ID) >> EXT_VENDOR_ID_SHIFT;
		switch (vendor) {
		case 0:
			vendor = CHIP_VENDOR_TSMC;
			break;
		case 1:
			vendor = CHIP_VENDOR_SMIC;
			break;
		case 2:
			vendor = CHIP_VENDOR_UMC;
			break;
		}
		pHalData->version_id.VendorType = vendor;
	}
	pHalData->version_id.CUTVersion = (value32 & CHIP_VER_RTL_MASK) >> CHIP_VER_RTL_SHIFT; /* IC version (CUT) */
	if (IS_HARDWARE_TYPE_8812(Adapter))
		pHalData->version_id.CUTVersion += 1;

	/* value32 = rtw_read32(Adapter, REG_GPIO_OUTSTS); */
	pHalData->version_id.ROMVer = 0;	/* ROM code version. */

	/* For multi-function consideration. Added by Roger, 2010.10.06. */
	pHalData->MultiFunc = RT_MULTI_FUNC_NONE;
	value32 = rtw_read32(Adapter, REG_MULTI_FUNC_CTRL);
	pHalData->MultiFunc |= ((value32 & WL_FUNC_EN) ? RT_MULTI_FUNC_WIFI : 0);
	pHalData->MultiFunc |= ((value32 & BT_FUNC_EN) ? RT_MULTI_FUNC_BT : 0);
	pHalData->PolarityCtl = ((value32 & WL_HWPDN_SL) ? RT_POLARITY_HIGH_ACT : RT_POLARITY_LOW_ACT);

	rtw_hal_config_rftype(Adapter);
#if 1
	dump_chip_info(pHalData->version_id);
#endif

}

VOID
Hal_PatchwithJaguar_8812(
	IN PADAPTER				Adapter,
	IN RT_MEDIA_STATUS		MediaStatus
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

		if (rtw_get_chip_type(Adapter) == RTL8812)
			rtw_write8(Adapter, REG_TCR + 3, BIT0 | BIT1 | BIT2);
		else
			/*
			* 20150707 yiwei.sun
			* set 0x604[24] = 0 , to fix 11ac vht 80Mhz mode  mcs 8 , 9 udp pkt lose issue
			* suggest by MAC team Jong & Scott
			*/
			rtw_write8(Adapter , REG_TCR + 3 , BIT1 | BIT2);
	}


	if ((MediaStatus == RT_MEDIA_CONNECT) &&
	    ((pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_REALTEK_JAGUAR_BCUTAP) ||
	     (pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_REALTEK_JAGUAR_CCUTAP))) {
		rtw_write8(Adapter, rBWIndication_Jaguar + 3,
			(rtw_read8(Adapter, rBWIndication_Jaguar + 3) | BIT2));
	} else {
		rtw_write8(Adapter, rBWIndication_Jaguar + 3,
			(rtw_read8(Adapter, rBWIndication_Jaguar + 3) & (~BIT2)));
	}
}

#ifdef CONFIG_RTL8821A
void init_hal_spec_8821a(_adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);

	hal_spec->ic_name = "rtl8821a";
	hal_spec->macid_num = 128;
	hal_spec->sec_cam_ent_num = 64;
	hal_spec->sec_cap = 0;
	hal_spec->rfpath_num_2g = 1;
	hal_spec->rfpath_num_5g = 1;
	hal_spec->txgi_max = 63;
	hal_spec->txgi_pdbm = 2;
	hal_spec->max_tx_cnt = 1;
	hal_spec->tx_nss_num = 1;
	hal_spec->rx_nss_num = 1;
	hal_spec->band_cap = BAND_CAP_2G | BAND_CAP_5G;
	hal_spec->bw_cap = BW_CAP_20M | BW_CAP_40M | BW_CAP_80M;
	hal_spec->port_num = 2;
	hal_spec->proto_cap = PROTO_CAP_11B | PROTO_CAP_11G | PROTO_CAP_11N | PROTO_CAP_11AC;

	hal_spec->wl_func = 0
			    | WL_FUNC_P2P
			    | WL_FUNC_MIRACAST
			    | WL_FUNC_TDLS
			    ;

	hal_spec->pg_txpwr_saddr = 0x10;
	hal_spec->pg_txgi_diff_factor = 1;

	rtw_macid_ctl_init_sleep_reg(adapter_to_macidctl(adapter)
		, REG_MACID_SLEEP
		, REG_MACID_SLEEP_1
		, REG_MACID_SLEEP_2
		, REG_MACID_SLEEP_3);

}
#endif /* CONFIG_RTL8821A */

void init_hal_spec_8812a(_adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);

	hal_spec->ic_name = "rtl8812a";
	hal_spec->macid_num = 128;
	hal_spec->sec_cam_ent_num = 64;
	hal_spec->sec_cap = 0;
	hal_spec->rfpath_num_2g = 2;
	hal_spec->rfpath_num_5g = 2;
	hal_spec->txgi_max = 63;
	hal_spec->txgi_pdbm = 2;
	hal_spec->max_tx_cnt = 2;
	hal_spec->tx_nss_num = 2;
	hal_spec->rx_nss_num = 2;
	hal_spec->band_cap = BAND_CAP_2G | BAND_CAP_5G;
	hal_spec->bw_cap = BW_CAP_20M | BW_CAP_40M | BW_CAP_80M;
	hal_spec->port_num = 2;
	hal_spec->proto_cap = PROTO_CAP_11B | PROTO_CAP_11G | PROTO_CAP_11N | PROTO_CAP_11AC;

	hal_spec->wl_func = 0
			    | WL_FUNC_P2P
			    | WL_FUNC_MIRACAST
			    | WL_FUNC_TDLS
			    ;

	hal_spec->pg_txpwr_saddr = 0x10;
	hal_spec->pg_txgi_diff_factor = 1;

	rtw_macid_ctl_init_sleep_reg(adapter_to_macidctl(adapter)
		, REG_MACID_SLEEP
		, REG_MACID_SLEEP_1
		, REG_MACID_SLEEP_2
		, REG_MACID_SLEEP_3);
}

void InitDefaultValue8821A(PADAPTER padapter)
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

VOID
_InitBeaconParameters_8812A(
	IN  PADAPTER Adapter
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

	rtw_write8(Adapter, REG_DRVERLYINT, DRIVER_EARLY_INT_TIME_8812);/* 5ms */
	rtw_write8(Adapter, REG_BCNDMATIM, BCN_DMA_ATIME_INT_TIME_8812); /* 2ms */

	/* Suggested by designer timchen. Change beacon AIFS to the largest number */
	/* beacause test chip does not contension before sending beacon. by tynli. 2009.11.03 */
	rtw_write16(Adapter, REG_BCNTCFG, 0x4413);

}

static VOID
_BeaconFunctionEnable(
	IN	PADAPTER		Adapter,
	IN	BOOLEAN			Enable,
	IN	BOOLEAN			Linked
)
{
	rtw_write8(Adapter, REG_BCN_CTRL, (DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_BCNQ_SUB));

	rtw_write8(Adapter, REG_RD_CTRL + 1, 0x6F);
}


void SetBeaconRelatedRegisters8812A(PADAPTER padapter)
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

	_InitBeaconParameters_8812A(padapter);

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

#ifdef CONFIG_BEAMFORMING

#if (BEAMFORMING_SUPPORT == 0)
VOID
SetBeamformingCLK_8812(
	IN	PADAPTER			Adapter
)
{
	struct pwrctrl_priv	*pwrpriv = adapter_to_pwrctl(Adapter);
	u16	u2btmp;
	u8	Count = 0, u1btmp;

	RTW_INFO(" ==>%s\n", __FUNCTION__);

	if (rtw_mi_check_fwstate(Adapter, _FW_UNDER_SURVEY)) {
		RTW_INFO(" <==%s return by Scan\n", __FUNCTION__);
		return;
	}

	/* Stop Usb TxDMA */
	rtw_write_port_cancel(Adapter);

#ifdef CONFIG_PCI_HCI
	/* Stop PCIe TxDMA */
	rtw_write8(Adapter, REG_PCIE_CTRL_REG + 1, 0xFE);
#endif

	/* Wait TXFF empty */
	for (Count = 0; Count < 100; Count++) {
		u2btmp = rtw_read16(Adapter, REG_TXPKT_EMPTY);
		u2btmp = u2btmp & 0xfff;
		if (u2btmp != 0xfff) {
			rtw_mdelay_os(10);
			continue;
		} else
			break;
	}

	RTW_INFO(" Tx Empty count %d\n", Count);

	/* TX pause */
	rtw_write8(Adapter, REG_TXPAUSE, 0xFF);

	/* Wait TX State Machine OK */
	for (Count = 0; Count < 100; Count++) {
		if (rtw_read32(Adapter, REG_SCH_TXCMD_8812A) != 0)
			continue;
		else
			break;
	}

	RTW_INFO(" Tx Status count %d\n", Count);

	/* Stop RX DMA path */
	u1btmp = rtw_read8(Adapter, REG_RXDMA_CONTROL_8812A);
	rtw_write8(Adapter, REG_RXDMA_CONTROL_8812A, u1btmp | BIT2);

	for (Count = 0; Count < 100; Count++) {
		u1btmp = rtw_read8(Adapter, REG_RXDMA_CONTROL_8812A);
		if (u1btmp & BIT1)
			break;
		else
			rtw_mdelay_os(10);
	}

	RTW_INFO(" Rx Empty count %d\n", Count);

	/* Disable clock */
	rtw_write8(Adapter, REG_SYS_CLKR + 1, 0xf0);
	/* Disable 320M */
	rtw_write8(Adapter, REG_AFE_PLL_CTRL + 3, 0x8);
	/* Enable 320M */
	rtw_write8(Adapter, REG_AFE_PLL_CTRL + 3, 0xa);
	/* Enable clock */
	rtw_write8(Adapter, REG_SYS_CLKR + 1, 0xfc);

	/* Release Tx pause */
	rtw_write8(Adapter, REG_TXPAUSE, 0);

	/* Enable RX DMA path */
	u1btmp = rtw_read8(Adapter, REG_RXDMA_CONTROL_8812A);
	rtw_write8(Adapter, REG_RXDMA_CONTROL_8812A, u1btmp & (~BIT2));

	/* Start Usb TxDMA */
	RTW_ENABLE_FUNC(Adapter, DF_TX_BIT);
	RTW_INFO("%s\n", __FUNCTION__);

	RTW_INFO("<==%s\n", __FUNCTION__);
}

VOID
SetBeamformRfMode_8812(
	IN PADAPTER				Adapter,
	IN struct beamforming_info	*pBeamInfo
)
{
	BOOLEAN				bSelfBeamformer = _FALSE;
	BOOLEAN				bSelfBeamformee = _FALSE;
	BEAMFORMING_CAP	BeamformCap = BEAMFORMING_CAP_NONE;

	BeamformCap = beamforming_get_beamform_cap(pBeamInfo);

	if (BeamformCap == pBeamInfo->beamforming_cap)
		return;
	else
		pBeamInfo->beamforming_cap = BeamformCap;

	if (GET_RF_TYPE(Adapter) == RF_1T1R)
		return;

	bSelfBeamformer = BeamformCap & BEAMFORMER_CAP;
	bSelfBeamformee = BeamformCap & BEAMFORMEE_CAP;

	phy_set_rf_reg(Adapter, RF_PATH_A, RF_WeLut_Jaguar, 0x80000, 0x1); /* RF Mode table write enable */
	phy_set_rf_reg(Adapter, RF_PATH_B, RF_WeLut_Jaguar, 0x80000, 0x1); /* RF Mode table write enable */

	if (bSelfBeamformer) {
		/* Paath_A */
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_ModeTableAddr, 0x78000, 0x3); /* Select RX mode */
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_ModeTableData0, 0xfffff, 0x3F7FF); /* Set Table data */
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_ModeTableData1, 0xfffff, 0xE26BF); /* Enable TXIQGEN in RX mode */
		/* Path_B */
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_ModeTableAddr, 0x78000, 0x3); /* Select RX mode */
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_ModeTableData0, 0xfffff, 0x3F7FF); /* Set Table data */
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_ModeTableData1, 0xfffff, 0xE26BF); /* Enable TXIQGEN in RX mode */
	} else {
		/* Paath_A */
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_ModeTableAddr, 0x78000, 0x3); /* Select RX mode */
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_ModeTableData0, 0xfffff, 0x3F7FF); /* Set Table data */
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_ModeTableData1, 0xfffff, 0xC26BF); /* Disable TXIQGEN in RX mode */
		/* Path_B */
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_ModeTableAddr, 0x78000, 0x3); /* Select RX mode */
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_ModeTableData0, 0xfffff, 0x3F7FF); /* Set Table data */
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_ModeTableData1, 0xfffff, 0xC26BF); /* Disable TXIQGEN in RX mode */
	}

	phy_set_rf_reg(Adapter, RF_PATH_A, RF_WeLut_Jaguar, 0x80000, 0x0); /* RF Mode table write disable */
	phy_set_rf_reg(Adapter, RF_PATH_B, RF_WeLut_Jaguar, 0x80000, 0x0); /* RF Mode table write disable */

	if (bSelfBeamformer)
		phy_set_bb_reg(Adapter, rTxPath_Jaguar, bMaskByte1, 0x33);
	else
		phy_set_bb_reg(Adapter, rTxPath_Jaguar, bMaskByte1, 0x11);
}



VOID
SetBeamformEnter_8812(
	IN PADAPTER				Adapter,
	IN u8					Idx
)
{
	u8	i = 0;
	u32	CSI_Param;
	HAL_DATA_TYPE			*pHalData = GET_HAL_DATA(Adapter);
	struct mlme_priv			*pmlmepriv = &(Adapter->mlmepriv);
	struct beamforming_info	*pBeamInfo = GET_BEAMFORM_INFO(pmlmepriv);
	struct beamforming_entry	BeamformEntry = pBeamInfo->beamforming_entry[Idx];
	u16	STAid = 0;

	SetBeamformRfMode_8812(Adapter, pBeamInfo);

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) || check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))
		STAid = BeamformEntry.mac_id;
	else
		STAid = BeamformEntry.p_aid;

	/* Sounding protocol control */
	rtw_write8(Adapter, REG_SND_PTCL_CTRL_8812A, 0xCB);

	/* MAC addresss/Partial AID of Beamformer */
	if (Idx == 0) {
		for (i = 0; i < ETH_ALEN ; i++)
			rtw_write8(Adapter, (REG_BFMER0_INFO_8812A + i), BeamformEntry.mac_addr[i]);
		/* CSI report use legacy ofdm so don't need to fill P_AID.*/
		/*rtw_write16(Adapter, REG_BFMER0_INFO_8812A+6, BeamformEntry.P_AID);*/
	} else {
		for (i = 0; i < ETH_ALEN ; i++)
			rtw_write8(Adapter, (REG_BFMER1_INFO_8812A + i), BeamformEntry.mac_addr[i]);
		/* CSI report use legacy ofdm so don't need to fill P_AID.*/
		/*rtw_write16(Adapter, REG_BFMER1_INFO_8812A+6, BeamformEntry.P_AID);*/
	}

	/* CSI report parameters of Beamformee */
	if ((BeamformEntry.beamforming_entry_cap & BEAMFORMEE_CAP_VHT_SU) ||
	    (BeamformEntry.beamforming_entry_cap & BEAMFORMER_CAP_VHT_SU)) {
		if (pHalData->rf_type == RF_2T2R)
			CSI_Param = 0x01090109;
		else
			CSI_Param = 0x01080108;
	} else {
		if (pHalData->rf_type == RF_2T2R)
			CSI_Param = 0x03090309;
		else
			CSI_Param = 0x03080308;
	}

	if (pHalData->rf_type == RF_2T2R)
		rtw_write32(Adapter, 0x9B4, 0x00000000);	/* Nc =2 */
	else
		rtw_write32(Adapter, 0x9B4, 0x01081008); /* Nc =1 */

	rtw_write32(Adapter, REG_CSI_RPT_PARAM_BW20_8812A, CSI_Param);
	rtw_write32(Adapter, REG_CSI_RPT_PARAM_BW40_8812A, CSI_Param);
	rtw_write32(Adapter, REG_CSI_RPT_PARAM_BW80_8812A, CSI_Param);

	/* P_AID of Beamformee & enable NDPA transmission & enable NDPA interrupt */
	if (Idx == 0) {
		rtw_write16(Adapter, REG_TXBF_CTRL_8812A, STAid);
		rtw_write8(Adapter, REG_TXBF_CTRL_8812A + 3, rtw_read8(Adapter, REG_TXBF_CTRL_8812A + 3) | BIT4 | BIT6 | BIT7);
	} else
		rtw_write16(Adapter, REG_TXBF_CTRL_8812A + 2, STAid | BIT12 | BIT14 | BIT15);

	/* CSI report parameters of Beamformee */
	if (Idx == 0) {
		/* Get BIT24 & BIT25*/
		u8	tmp = rtw_read8(Adapter, REG_BFMEE_SEL_8812A + 3) & 0x3;

		rtw_write8(Adapter, REG_BFMEE_SEL_8812A + 3, tmp | 0x60);
		rtw_write16(Adapter, REG_BFMEE_SEL_8812A, STAid | BIT9);
	} else {
		/* Set BIT25 */
		rtw_write16(Adapter, REG_BFMEE_SEL_8812A + 2, STAid | (0xE2 << 8));
	}

	/* Timeout value for MAC to leave NDP_RX_standby_state (60 us, Test chip) (80 us,  MP chip) */
	rtw_write8(Adapter, REG_SND_PTCL_CTRL_8812A + 3, 0x50);

	beamforming_notify(Adapter);
}


VOID
SetBeamformLeave_8812(
	IN PADAPTER				Adapter,
	IN u8					Idx
)
{
	struct beamforming_info	*pBeamInfo = GET_BEAMFORM_INFO(&(Adapter->mlmepriv));

	SetBeamformRfMode_8812(Adapter, pBeamInfo);

	/*	Clear P_AID of Beamformee
	*	Clear MAC addresss of Beamformer
	*	Clear Associated Bfmee Sel
	*/
	if (pBeamInfo->beamforming_cap == BEAMFORMING_CAP_NONE)
		rtw_write8(Adapter, REG_SND_PTCL_CTRL_8812A, 0xC8);

	if (Idx == 0) {
		rtw_write16(Adapter, REG_TXBF_CTRL_8812A, 0);

		rtw_write32(Adapter, REG_BFMER0_INFO_8812A, 0);
		rtw_write16(Adapter, REG_BFMER0_INFO_8812A + 4, 0);

		rtw_write16(Adapter, REG_BFMEE_SEL_8812A, 0);
	} else {
		rtw_write16(Adapter, REG_TXBF_CTRL_8812A + 2, rtw_read16(Adapter, REG_TXBF_CTRL_8812A + 2) & 0xF000);

		rtw_write32(Adapter, REG_BFMER1_INFO_8812A, 0);
		rtw_write16(Adapter, REG_BFMER1_INFO_8812A + 4, 0);

		rtw_write16(Adapter, REG_BFMEE_SEL_8812A + 2, rtw_read16(Adapter, REG_BFMEE_SEL_8812A + 2) & 0x60);
	}
}


VOID
SetBeamformStatus_8812(
	IN PADAPTER				Adapter,
	IN u8					Idx
)
{
	u16	BeamCtrlVal;
	u32	BeamCtrlReg;
	struct mlme_priv			*pmlmepriv = &(Adapter->mlmepriv);
	struct beamforming_info	*pBeamInfo = GET_BEAMFORM_INFO(pmlmepriv);
	struct beamforming_entry	BeamformEntry = pBeamInfo->beamforming_entry[Idx];

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) || check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))
		BeamCtrlVal = BeamformEntry.mac_id;
	else
		BeamCtrlVal = BeamformEntry.p_aid;

	if (Idx == 0)
		BeamCtrlReg = REG_TXBF_CTRL_8812A;
	else {
		BeamCtrlReg = REG_TXBF_CTRL_8812A + 2;
		BeamCtrlVal |= BIT12 | BIT14 | BIT15;
	}

	if (BeamformEntry.beamforming_entry_state == BEAMFORMING_ENTRY_STATE_PROGRESSED) {
		if (BeamformEntry.sound_bw == CHANNEL_WIDTH_20)
			BeamCtrlVal |= BIT9;
		else if (BeamformEntry.sound_bw == CHANNEL_WIDTH_40)
			BeamCtrlVal |= BIT10;
		else if (BeamformEntry.sound_bw == CHANNEL_WIDTH_80)
			BeamCtrlVal |= BIT11;
	} else
		BeamCtrlVal &= ~(BIT9 | BIT10 | BIT11);

	rtw_write16(Adapter, BeamCtrlReg, BeamCtrlVal);

	RTW_INFO("%s Idx %d BeamCtrlReg %x BeamCtrlVal %x\n", __FUNCTION__, Idx, BeamCtrlReg, BeamCtrlVal);
}


VOID
SetBeamformFwTxBFCmd_8812(
	IN	PADAPTER	Adapter
)
{
	u8	Idx, Period0 = 0, Period1 = 0;
	u8	PageNum0 = 0xFF, PageNum1 = 0xFF;
	u8	u1TxBFParm[3] = {0};

	struct mlme_priv			*pmlmepriv = &(Adapter->mlmepriv);
	struct beamforming_info	*pBeamInfo = GET_BEAMFORM_INFO(pmlmepriv);

	for (Idx = 0; Idx < BEAMFORMING_ENTRY_NUM; Idx++) {
		if (pBeamInfo->beamforming_entry[Idx].beamforming_entry_state == BEAMFORMING_ENTRY_STATE_PROGRESSED) {
			if (Idx == 0) {
				if (pBeamInfo->beamforming_entry[Idx].bSound)
					PageNum0 = 0xFE;
				else
					PageNum0 = 0xFF; /* stop sounding */
				Period0 = (u8)(pBeamInfo->beamforming_entry[Idx].sound_period);
			} else if (Idx == 1) {
				if (pBeamInfo->beamforming_entry[Idx].bSound)
					PageNum1 = 0xFE;
				else
					PageNum1 = 0xFF; /* stop sounding */
				Period1 = (u8)(pBeamInfo->beamforming_entry[Idx].sound_period);
			}
		}
	}

	u1TxBFParm[0] = PageNum0;
	u1TxBFParm[1] = PageNum1;
	u1TxBFParm[2] = (Period1 << 4) | Period0;
	fill_h2c_cmd_8812(Adapter, H2C_8812_TxBF, 3, u1TxBFParm);

	RTW_INFO("%s PageNum0 = %d Period0 = %d\n", __FUNCTION__, PageNum0, Period0);
	RTW_INFO("PageNum1 = %d Period1 %d\n", PageNum1, Period1);
}


VOID
SetBeamformDownloadNDPA_8812(
	IN	PADAPTER			Adapter,
	IN	u8					Idx
)
{
	u8	u1bTmp = 0, tmpReg422 = 0, Head_Page;
	u8	BcnValidReg = 0, count = 0, DLBcnCount = 0;
	BOOLEAN			bSendBeacon = _FALSE;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u8	TxPageBndy = LAST_ENTRY_OF_TX_PKT_BUFFER_8812; /* default reseved 1 page for the IC type which is undefined. */
	struct beamforming_info	*pBeamInfo = GET_BEAMFORM_INFO(&(Adapter->mlmepriv));
	struct beamforming_entry	*pBeamEntry = pBeamInfo->beamforming_entry + Idx;

	/* pHalData->bFwDwRsvdPageInProgress = _TRUE; */

	if (Idx == 0)
		Head_Page = 0xFE;
	else
		Head_Page = 0xFE;

	rtw_hal_get_def_var(Adapter, HAL_DEF_TX_PAGE_BOUNDARY, (u8 *)&TxPageBndy);

	/* Set REG_CR bit 8. DMA beacon by SW. */
	u1bTmp = rtw_read8(Adapter, REG_CR + 1);
	rtw_write8(Adapter,  REG_CR + 1, (u1bTmp | BIT0));

	rtw_write8(Adapter, REG_CR + 1,
		rtw_read8(Adapter, REG_CR + 1) | BIT0);

	/* Set FWHW_TXQ_CTRL 0x422[6]=0 to tell Hw the packet is not a real beacon frame. */
	tmpReg422 = rtw_read8(Adapter, REG_FWHW_TXQ_CTRL + 2);
	rtw_write8(Adapter, REG_FWHW_TXQ_CTRL + 2,  tmpReg422 & (~BIT6));

	if (tmpReg422 & BIT6) {
		RTW_INFO("SetBeamformDownloadNDPA_8812(): There is an Adapter is sending beacon.\n");
		bSendBeacon = _TRUE;
	}

	/*	TDECTRL[15:8] 0x209[7:0] = 0xF6	Beacon Head for TXDMA */
	rtw_write8(Adapter, REG_TDECTRL + 1, Head_Page);

	do {
		/* Clear beacon valid check bit. */
		BcnValidReg = rtw_read8(Adapter, REG_TDECTRL + 2);
		rtw_write8(Adapter, REG_TDECTRL + 2, (BcnValidReg | BIT0));

		/* download NDPA rsvd page. */
		if (pBeamEntry->beamforming_entry_cap & BEAMFORMER_CAP_VHT_SU)
			beamforming_send_vht_ndpa_packet(Adapter, pBeamEntry->mac_addr, pBeamEntry->aid, pBeamEntry->sound_bw, BCN_QUEUE_INX);
		else
			beamforming_send_ht_ndpa_packet(Adapter, pBeamEntry->mac_addr, pBeamEntry->sound_bw, BCN_QUEUE_INX);

		/* check rsvd page download OK. */
		BcnValidReg = rtw_read8(Adapter, REG_TDECTRL + 2);
		count = 0;
		while (!(BcnValidReg & BIT0) && count < 20) {
			count++;
			rtw_udelay_os(10);
			BcnValidReg = rtw_read8(Adapter, REG_TDECTRL + 2);
		}
		DLBcnCount++;
	} while (!(BcnValidReg & BIT0) && DLBcnCount < 5);

	if (!(BcnValidReg & BIT0))
		RTW_INFO("%s Download RSVD page failed!\n", __FUNCTION__);

	/*	TDECTRL[15:8] 0x209[7:0] = 0xF6	Beacon Head for TXDMA */
	rtw_write8(Adapter, REG_TDECTRL + 1, TxPageBndy);

	/* To make sure that if there exists an adapter which would like to send beacon. */
	/* If exists, the origianl value of 0x422[6] will be 1, we should check this to */
	/* prevent from setting 0x422[6] to 0 after download reserved page, or it will cause */
	/* the beacon cannot be sent by HW. */
	/* 2010.06.23. Added by tynli. */
	if (bSendBeacon)
		rtw_write8(Adapter, REG_FWHW_TXQ_CTRL + 2, tmpReg422);

	/* Do not enable HW DMA BCN or it will cause Pcie interface hang by timing issue. 2011.11.24. by tynli. */
	/* Clear CR[8] or beacon packet will not be send to TxBuf anymore. */
	u1bTmp = rtw_read8(Adapter, REG_CR + 1);
	rtw_write8(Adapter, REG_CR + 1, (u1bTmp & (~BIT0)));

	pBeamEntry->beamforming_entry_state = BEAMFORMING_ENTRY_STATE_PROGRESSED;

	/* pHalData->bFwDwRsvdPageInProgress = _FALSE; */
}

VOID
SetBeamformFwTxBF_8812(
	IN	PADAPTER			Adapter,
	IN	u8					Idx
)
{
	struct beamforming_info	*pBeamInfo = GET_BEAMFORM_INFO(&(Adapter->mlmepriv));
	struct beamforming_entry	*pBeamEntry = pBeamInfo->beamforming_entry + Idx;

	if (pBeamEntry->beamforming_entry_state == BEAMFORMING_ENTRY_STATE_PROGRESSING)
		SetBeamformDownloadNDPA_8812(Adapter, Idx);

	SetBeamformFwTxBFCmd_8812(Adapter);
}


VOID
SetBeamformPatch_8812(
	IN	PADAPTER			Adapter,
	IN	u8					Operation
)
{
	HAL_DATA_TYPE			*pHalData = GET_HAL_DATA(Adapter);
	struct beamforming_info	*pBeamInfo = GET_BEAMFORM_INFO(&(Adapter->mlmepriv));

	if (pBeamInfo->beamforming_cap == BEAMFORMING_CAP_NONE)
		return;

	/*if(Operation == SCAN_OPT_BACKUP_BAND0)
	{
		rtw_write8(Adapter, REG_SND_PTCL_CTRL_8812A, 0xC8);
	}
	else if(Operation == SCAN_OPT_RESTORE)
	{
		rtw_write8(Adapter, REG_SND_PTCL_CTRL_8812A, 0xCB);
	}*/
}
#endif /*#if (BEAMFORMING_SUPPORT == 0) - for drv beamforming*/
#endif /*#ifdef CONFIG_BEAMFORMING*/

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
				UpdateInterruptMask8812AE(Adapter, 0, 0, RT_BCN_INT_MASKS, 0);
#else
#ifdef CONFIG_INTERRUPT_BASED_TXBCN

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
				rtw_write8(Adapter, REG_DRVERLYINT, 0x05);/* restore early int time to 5ms */
				UpdateInterruptMask8812AU(Adapter, _TRUE, 0, IMR_BCNDMAINT0_8812);
#endif /* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
				UpdateInterruptMask8812AU(Adapter, _TRUE , 0, (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812));
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif
			}

			rtw_write8(Adapter, REG_BCN_CTRL_1, DIS_TSF_UDT | DIS_ATIM); /* disable atim wnd and disable beacon function */
			/* rtw_write8(Adapter,REG_BCN_CTRL_1, DIS_TSF_UDT | EN_BCN_FUNCTION); */
		} else if (mode == _HW_STATE_ADHOC_) {
			ResumeTxBeacon(Adapter);
			rtw_write8(Adapter, REG_BCN_CTRL_1, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_BCNQ_SUB);
		} else if (mode == _HW_STATE_AP_) {
#ifdef CONFIG_PCI_HCI
			UpdateInterruptMask8812AE(Adapter, RT_BCN_INT_MASKS, 0, 0, 0);
#else
#ifdef CONFIG_INTERRUPT_BASED_TXBCN
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
			UpdateInterruptMask8812AU(Adapter, _TRUE , IMR_BCNDMAINT0_8812, 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
			UpdateInterruptMask8812AU(Adapter, _TRUE , (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812), 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif

			rtw_write8(Adapter, REG_BCN_CTRL_1, DIS_TSF_UDT | DIS_BCNQ_SUB);

#ifdef CONFIG_PCI_HCI
			/*Beacon is polled to TXBUF SWBCN*/
			rtw_write32(Adapter, REG_CR, rtw_read32(Adapter, REG_CR) | BIT(8));
			RTW_INFO("CR:SWBCN %x\n", rtw_read32(Adapter, 0x100));
#endif

			/* enable to rx data frame */
			rtw_write16(Adapter, REG_RXFLTMAP2, 0xFFFF);

			/* Beacon Control related register for first time */
			rtw_write8(Adapter, REG_BCNDMATIM, 0x02); /* 2ms		 */

			/* rtw_write8(Adapter, REG_BCN_MAX_ERR, 0xFF); */
			rtw_write8(Adapter, REG_ATIMWND_1, 0x0a); /* 10ms for port1 */

			rtw_write16(Adapter, REG_TSFTR_SYN_OFFSET, 0x7fff);/* +32767 (~32ms) */

			/* reset TSF2	 */
			rtw_write8(Adapter, REG_DUAL_TSF_RST, BIT(1));

			/* enable BCN1 Function for if2 */
			/* don't enable update TSF1 for if2 (due to TSF update when beacon/probe rsp are received) */
			rtw_write8(Adapter, REG_BCN_CTRL_1, (DIS_TSF_UDT | EN_BCN_FUNCTION | EN_TXBCN_RPT | DIS_BCNQ_SUB));

			if (IS_HARDWARE_TYPE_8821(Adapter)) {
				/* select BCN on port 1 */
				rtw_write8(Adapter, REG_CCK_CHECK_8812,	 rtw_read8(Adapter, REG_CCK_CHECK_8812) | BIT(5));
			}

			if (!rtw_mi_buddy_check_mlmeinfo_state(Adapter, WIFI_FW_ASSOC_SUCCESS))
				rtw_write8(Adapter, REG_BCN_CTRL,
					rtw_read8(Adapter, REG_BCN_CTRL) & ~EN_BCN_FUNCTION);

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

#ifdef CONFIG_MI_WITH_MBSSID_CAM
		hw_var_set_opmode_mbid(Adapter, mode);
#else
		/* disable Port0 TSF update */
		rtw_iface_disable_tsf_update(Adapter);

		/* set net_type */
		Set_MSR(Adapter, mode);

		RTW_INFO("%s()-%d mode = %d\n", __func__, __LINE__, mode);

		if ((mode == _HW_STATE_STATION_) || (mode == _HW_STATE_NOLINK_)) {
#ifdef CONFIG_CONCURRENT_MODE
			if (!rtw_mi_get_ap_num(Adapter) && !rtw_mi_get_mesh_num(Adapter))
#endif /* CONFIG_CONCURRENT_MODE */
			{
				StopTxBeacon(Adapter);
#ifdef CONFIG_PCI_HCI
				UpdateInterruptMask8812AE(Adapter, 0, 0, RT_BCN_INT_MASKS, 0);
#else
#ifdef CONFIG_INTERRUPT_BASED_TXBCN
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
				rtw_write8(Adapter, REG_DRVERLYINT, 0x05);/* restore early int time to 5ms					 */
				UpdateInterruptMask8812AU(Adapter, _TRUE, 0, IMR_BCNDMAINT0_8812);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
				UpdateInterruptMask8812AU(Adapter, _TRUE , 0, (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812));
#endif /* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif
			}

			rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_ATIM); /* disable atim wnd */
			/* rtw_write8(Adapter,REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION); */
		} else if (mode == _HW_STATE_ADHOC_) {
			ResumeTxBeacon(Adapter);
			rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_BCNQ_SUB);
		} else if (mode == _HW_STATE_AP_) {
#ifdef CONFIG_PCI_HCI
			UpdateInterruptMask8812AE(Adapter, RT_BCN_INT_MASKS, 0, 0, 0);
#else
#ifdef CONFIG_INTERRUPT_BASED_TXBCN
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
			UpdateInterruptMask8812AU(Adapter, _TRUE , IMR_BCNDMAINT0_8812, 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
			UpdateInterruptMask8812AU(Adapter, _TRUE , (IMR_TXBCN0ERR_8812 | IMR_TXBCN0OK_8812), 0);
#endif/* CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR */

#endif /* CONFIG_INTERRUPT_BASED_TXBCN */
#endif

			rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | DIS_BCNQ_SUB);
#ifdef CONFIG_PCI_HCI
			/*Beacon is polled to TXBUF SWBCN*/
			rtw_write32(Adapter, REG_CR, rtw_read32(Adapter, REG_CR) | BIT(8));
			RTW_INFO("CR:SWBCN %x\n", rtw_read32(Adapter, 0x100));
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

			if (IS_HARDWARE_TYPE_8821(Adapter)) {
				/* select BCN on port 0 */
				rtw_write8(Adapter, REG_CCK_CHECK_8812,	rtw_read8(Adapter, REG_CCK_CHECK_8812) & (~BIT(5)));
			}

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
#endif /*#ifdef CONFIG_MI_WITH_MBSSID_CAM*/
	}
}

static void hw_var_backup_IQK_val(PADAPTER padapter, struct hal_iqk_reg_backup *pRecPosForBkp)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);

	pRecPosForBkp->central_chnl = pHalData->current_channel;
	pRecPosForBkp->bw_mode = pHalData->current_channel_bw;

	if (1)
		RTW_INFO(FUNC_ADPT_FMT": central_chnl:%d, bw_mode:%d\n"
			, FUNC_ADPT_ARG(padapter)
			, pRecPosForBkp->central_chnl
			, pRecPosForBkp->bw_mode
			);

	/* According to phydm code (phy_IQCalibrate_8821A) & (phy_IQCalibrate_8812A) */
	if (IS_HARDWARE_TYPE_8821(padapter)) {
		/* for RF_PATH_A Tx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x1);
		pRecPosForBkp->reg_backup[RF_PATH_A][0] = rtw_read32(padapter, 0xCCC);
		pRecPosForBkp->reg_backup[RF_PATH_A][1] = rtw_read32(padapter, 0xCD4);
		/* for RF_PATH_A Rx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x0);
		pRecPosForBkp->reg_backup[RF_PATH_A][2] = rtw_read32(padapter, 0xC10);
	} else if (IS_HARDWARE_TYPE_8812(padapter)) {
		/* for RF_PATH_A Tx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x1);
		pRecPosForBkp->reg_backup[RF_PATH_A][0] = rtw_read32(padapter, 0xCCC);
		pRecPosForBkp->reg_backup[RF_PATH_A][1] = rtw_read32(padapter, 0xCD4);
		/* for RF_PATH_A Rx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x0);
		pRecPosForBkp->reg_backup[RF_PATH_A][2] = rtw_read32(padapter, 0xC10);
		/* for RF_PATH_B Tx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x1);
		pRecPosForBkp->reg_backup[RF_PATH_B][0] = rtw_read32(padapter, 0xECC);
		pRecPosForBkp->reg_backup[RF_PATH_B][1] = rtw_read32(padapter, 0xED4);
		/* for RF_PATH_B Rx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x0);
		pRecPosForBkp->reg_backup[RF_PATH_B][2] = rtw_read32(padapter, 0xE10);
	}
}

#ifdef CONFIG_TDLS
#ifdef CONFIG_TDLS_CH_SW
static void hw_var_restore_IQK_val_TDLS(PADAPTER padapter)
{
	u8 i;
	u8 central_chnl_now = 0;
	u8 bw_mode_now = 0;
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u8 RecPosForRetore;
	u8 MatchedRec = _FALSE;

	central_chnl_now = rtw_get_center_ch(dvobj->oper_channel, dvobj->oper_bwmode, dvobj->oper_ch_offset);
	bw_mode_now = dvobj->oper_bwmode;

	/* To search for the matched record */
	for (i = 0; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++) {
		if ((pHalData->iqk_reg_backup[i].central_chnl == central_chnl_now) &&
		    (pHalData->iqk_reg_backup[i].bw_mode == bw_mode_now)) {
			MatchedRec = _TRUE;
			RecPosForRetore = i;
			break;
		}
	}

	if (MatchedRec == _FALSE)
		return;

	if (IS_HARDWARE_TYPE_8821(padapter)) {
		/* for RF_PATH_A Tx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x1);
		phy_set_bb_reg(padapter, 0xCCC, 0x000007FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][0]);
		phy_set_bb_reg(padapter, 0xCD4, 0x000007FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][1]);
		/* for RF_PATH_A Rx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x0);
		phy_set_bb_reg(padapter, 0xC10, 0x03FF03FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][2]);
	} else if (IS_HARDWARE_TYPE_8812(padapter)) {
		/* for RF_PATH_A Tx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x1);
		phy_set_bb_reg(padapter, 0xCCC, 0x000007FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][0]);
		phy_set_bb_reg(padapter, 0xCD4, 0x000007FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][1]);
		/* for RF_PATH_A Rx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x0);
		phy_set_bb_reg(padapter, 0xC10, 0x03FF03FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][2]);
		/* for RF_PATH_B Tx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x1);
		phy_set_bb_reg(padapter, 0xECC, 0x000007FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[1][0]);
		phy_set_bb_reg(padapter, 0xED4, 0x000007FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[1][1]);
		/* for RF_PATH_B Rx */
		phy_set_bb_reg(padapter, 0x82C, BIT(31), 0x0);
		phy_set_bb_reg(padapter, 0xE10, 0x03FF03FF, pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[1][2]);
	}
}
#endif
#endif

#ifdef CONFIG_MCC_MODE
static void hw_var_restore_IQK_val_MCC(PADAPTER padapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	struct mcc_adapter_priv *pmccadapriv = &padapter->mcc_adapterpriv;
	u8 i = 0;
	u8 central_chnl_now = 0, bw_mode_now = 0;
	u8 chidx = pmlmeext->cur_channel;
	u8 bw = pmlmeext->cur_bwmode;
	u8 bw_offset = pmlmeext->cur_ch_offset;
	u8 RecPosForRetore = 0;
	u8 MatchedRec = _FALSE;

	central_chnl_now = rtw_get_center_ch(chidx, bw, bw_offset);
	bw_mode_now = bw;

	/* To search for the matched record */
	for (i = 0; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++) {
		if ((pHalData->iqk_reg_backup[i].central_chnl == central_chnl_now) &&
			(pHalData->iqk_reg_backup[i].bw_mode == bw_mode_now)) {
			MatchedRec = _TRUE;
			RecPosForRetore = i;
			break;
		}
	}

	if (MatchedRec == _FALSE) {
		rtw_warn_on(1);
		RTW_INFO("[MCC] "FUNC_ADPT_FMT" not found fit IQK value from IQK table(%d,%d)\n"
			, FUNC_ADPT_ARG(padapter), central_chnl_now, bw_mode_now);
		return;
	}

	/* According to phydm code(phy_IQCalibrate_8821A)  & document  */
	if (IS_HARDWARE_TYPE_8821(padapter)) {
		/* 8821A only one RF PATH */
		/* transform register value to TX_X & TX_Y */
		pmccadapriv->mcc_iqk_arr[RF_PATH_A].TX_X =
			(u16)(pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[RF_PATH_A][1] & 0x000007ff);
		pmccadapriv->mcc_iqk_arr[RF_PATH_A].TX_Y =
			(u16)(pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[RF_PATH_A][0] & 0x000007ff);

		if (0)
			RTW_INFO(FUNC_ADPT_FMT": central_chnl = %d, bw_mode = %d, rf_path = %d, TX_X = 0x%02x, TX_Y = 0x%02x\n"
			, FUNC_ADPT_ARG(padapter)
			, central_chnl_now
			, bw_mode_now
			, RF_PATH_A
			, pmccadapriv->mcc_iqk_arr[RF_PATH_A].TX_X
			, pmccadapriv->mcc_iqk_arr[RF_PATH_A].TX_Y
			);

		/* transform register value to RX_X & RX_Y */
		pmccadapriv->mcc_iqk_arr[RF_PATH_A].RX_X =
					(u16) ((pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][2] & 0x000003ff) << 1);
		pmccadapriv->mcc_iqk_arr[RF_PATH_A].RX_Y =
			(u16) (((pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[0][2] & 0x03ff0000)>>16) << 1);

		if (0)
			RTW_INFO(FUNC_ADPT_FMT": central_chnl = %d, bw_mode = %d, rf_path = %d, TX_X = 0x%02x, TX_Y = 0x%02x\n"
			, FUNC_ADPT_ARG(padapter)
			, central_chnl_now
			, bw_mode_now
			, RF_PATH_A
			, pmccadapriv->mcc_iqk_arr[RF_PATH_A].RX_X
			, pmccadapriv->mcc_iqk_arr[RF_PATH_A].RX_Y
			);

	}else if (IS_HARDWARE_TYPE_8812(padapter)) {
		/* 8812A has two RF PATH at most */
		u8 total_rf_path = pHalData->NumTotalRFPath;
		u8 rf_path = 0;

		for (rf_path = 0; rf_path < total_rf_path; rf_path++) {
			/* transform register value to TX_X & TX_Y */
			pmccadapriv->mcc_iqk_arr[rf_path].TX_X =
				(u16)(pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[rf_path][1] & 0x000007ff);
			pmccadapriv->mcc_iqk_arr[rf_path].TX_Y =
				(u16)(pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[rf_path][0] & 0x000007ff);

			if (1)
				RTW_INFO(FUNC_ADPT_FMT": central_chnl = %d, bw_mode = %d, rf_path = %d, TX_X = 0x%02x, TX_Y = 0x%02x\n"
				, FUNC_ADPT_ARG(padapter)
				, central_chnl_now
				, bw_mode_now
				, rf_path
				, pmccadapriv->mcc_iqk_arr[rf_path].TX_X
				, pmccadapriv->mcc_iqk_arr[rf_path].TX_Y
				);

			/* transform register value to RX_X & RX_Y */
			pmccadapriv->mcc_iqk_arr[rf_path].RX_X =
						(u16) ((pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[rf_path][2] & 0x000003ff) << 1);
			pmccadapriv->mcc_iqk_arr[rf_path].RX_Y =
				(u16) (((pHalData->iqk_reg_backup[RecPosForRetore].reg_backup[rf_path][2] & 0x03ff0000)>>16) << 1);

			if (1)
				RTW_INFO(FUNC_ADPT_FMT": central_chnl = %d, bw_mode = %d, rf_path = %d, RX_X = 0x%02x, RX_Y = 0x%02x\n"
				, FUNC_ADPT_ARG(padapter)
				, central_chnl_now
				, bw_mode_now
				, rf_path
				, pmccadapriv->mcc_iqk_arr[rf_path].RX_X
				, pmccadapriv->mcc_iqk_arr[rf_path].RX_Y
				);
		}
	}
}
#endif /* CONFIG_MCC_MODE */

u8 SetHwReg8812A(PADAPTER padapter, u8 variable, u8 *pval)
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

	case HW_VAR_BASIC_RATE: {
		struct mlme_ext_info *mlmext_info = &padapter->mlmeextpriv.mlmext_info;
		u16 input_b = 0, masked = 0, ioted = 0, BrateCfg = 0;
		u16 rrsr_2g_force_mask = RRSR_CCK_RATES;
		u16 rrsr_2g_allow_mask = (RRSR_24M | RRSR_12M | RRSR_6M | RRSR_CCK_RATES);
		u16 rrsr_5g_force_mask = (RRSR_6M);
		u16 rrsr_5g_allow_mask = (RRSR_OFDM_RATES);

		HalSetBrateCfg(padapter, pval, &BrateCfg);
		input_b = BrateCfg;

		/* apply force and allow mask */
		if (pHalData->current_band_type == BAND_ON_2_4G) {
			BrateCfg |= rrsr_2g_force_mask;
			BrateCfg &= rrsr_2g_allow_mask;
		} else { /* 5G */
			BrateCfg |= rrsr_5g_force_mask;
			BrateCfg &= rrsr_5g_allow_mask;
		}
		masked = BrateCfg;

		/* IOT consideration */
		if (mlmext_info->assoc_AP_vendor == HT_IOT_PEER_CISCO) {
			/* if peer is cisco and didn't use ofdm rate, we enable 6M ack */
			if ((BrateCfg & (RRSR_24M | RRSR_12M | RRSR_6M)) == 0)
				BrateCfg |= RRSR_6M;
		}
		ioted = BrateCfg;

		pHalData->BasicRateSet = BrateCfg;

		RTW_INFO("HW_VAR_BASIC_RATE: %#x->%#x->%#x\n", input_b, masked, ioted);

		/* Set RRSR rate table. */
		rtw_write16(padapter, REG_RRSR, BrateCfg);
		rtw_write8(padapter, REG_RRSR + 2, rtw_read8(padapter, REG_RRSR + 2) & 0xf0);
	}
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

	case HW_VAR_CAM_EMPTY_ENTRY: {
		u8 ucIndex = *pval;
		u8 i;
		u32	ulCommand = 0;
		u32	ulContent = 0;
		u32	ulEncAlgo = CAM_AES;

		for (i = 0; i < CAM_CONTENT_COUNT; i++) {
			/* filled id in CAM config 2 byte */
			if (i == 0) {
				ulContent |= (ucIndex & 0x03) | ((u16)(ulEncAlgo) << 2);
				/* ulContent |= CAM_VALID; */
			} else
				ulContent = 0;
			/* polling bit, and No Write enable, and address */
			ulCommand = CAM_CONTENT_COUNT * ucIndex + i;
			ulCommand = ulCommand | CAM_POLLINIG | CAM_WRITE;
			/* write content 0 is equall to mark invalid */
			rtw_write32(padapter, WCAMI, ulContent);  /* delay_ms(40); */
			rtw_write32(padapter, RWCAM, ulCommand);  /* delay_ms(40); */
		}
	}
		break;

	case HW_VAR_CAM_INVALID_ALL:
		val32 = BIT(31) | BIT(30);
		rtw_write32(padapter, RWCAM, val32);
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

		if (IS_HARDWARE_TYPE_8812(padapter)) {
			if (AMPDULen < VHT_AGG_SIZE_128K)
				AMPDULen = (0x2000 << *(u8 *)pval) - 1;
			else
				AMPDULen = 0x1ffff;
		} else if (IS_HARDWARE_TYPE_8821(padapter)) {
			if (AMPDULen < HT_AGG_SIZE_64K)
				AMPDULen = (0x2000 << *(u8 *)pval) - 1;
			else
				AMPDULen = 0xffff;
		}
		AMPDULen |= BIT(31);
		rtw_write32(padapter, REG_AMPDU_MAX_LENGTH_8812, AMPDULen);
	}
	break;
#endif /* CONFIG_80211N_HT */
#if 0
	case HW_VAR_RXDMA_AGG_PG_TH:
		rtw_write8(padapter, REG_RXDMA_AGG_PG_TH, *pval);
		break;
#endif
	case HW_VAR_H2C_FW_PWRMODE: {
		u8 psmode = *pval;

		rtl8812_set_FwPwrMode_cmd(padapter, psmode);
	}
	break;

	case HW_VAR_H2C_FW_JOINBSSRPT:
		rtl8812_set_FwJoinBssReport_cmd(padapter, *pval);
		break;
	case HW_VAR_DL_RSVD_PAGE:
#ifdef CONFIG_BT_COEXIST
		if (pHalData->EEPROMBluetoothCoexist == 1) {
			if (check_fwstate(&padapter->mlmepriv, WIFI_AP_STATE) == _TRUE)
				rtl8812a_download_BTCoex_AP_mode_rsvd_page(padapter);
		}
#endif /* CONFIG_BT_COEXIST */
		break;

#ifdef CONFIG_P2P_PS
	case HW_VAR_H2C_FW_P2P_PS_OFFLOAD:
		rtl8812_set_p2p_ps_offload_cmd(padapter, *pval);
		break;
#endif /* CONFIG_P2P_PS */

	case HW_VAR_EFUSE_USAGE:
		pHalData->EfuseUsedPercentage = *pval;
		break;

	case HW_VAR_EFUSE_BYTES:
		pHalData->EfuseUsedBytes = *(u16 *)pval;
		break;
	case HW_VAR_EFUSE_BT_USAGE:
#ifdef HAL_EFUSE_MEMORY
		pHalData->EfuseHal.BTEfuseUsedPercentage = *pval;
#endif
		break;

	case HW_VAR_EFUSE_BT_BYTES:
#ifdef HAL_EFUSE_MEMORY
		pHalData->EfuseHal.BTEfuseUsedBytes = *(u16 *)pval;
#else
		BTEfuseUsedBytes = *(u16 *)pval;
#endif
		break;
	case HW_VAR_FIFO_CLEARN_UP: {
		struct pwrctrl_priv *pwrpriv;
		u8 trycnt = 100;

		pwrpriv = adapter_to_pwrctl(padapter);

		/* pause tx */
		rtw_write8(padapter, REG_TXPAUSE, 0xff);

		/* keep sn */
		padapter->xmitpriv.nqos_ssn = rtw_read16(padapter, REG_NQOS_SEQ);

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
		/* restore Sequence No. */
		rtw_write8(padapter, 0x4dc, padapter->xmitpriv.nqos_ssn);
		break;
	case HW_VAR_CHECK_TXBUF: {
		u8 retry_limit;
		u32 reg_200 = 0, reg_204 = 0;
		u32 init_reg_200 = 0, init_reg_204 = 0;
		systime start = rtw_get_current_time();
		u32 pass_ms;
		int i = 0;

		retry_limit = 0x01;

		val16 = BIT_SRL(retry_limit) | BIT_LRL(retry_limit);
		rtw_write16(padapter, REG_RETRY_LIMIT, val16);

		while (rtw_get_passing_time_ms(start) < 2000
		       && !RTW_CANNOT_RUN(padapter)
		      ) {
			reg_200 = rtw_read32(padapter, 0x200);
			reg_204 = rtw_read32(padapter, 0x204);

			if (i == 0) {
				init_reg_200 = reg_200;
				init_reg_204 = reg_204;
			}

			i++;
			if ((reg_200 & 0x00ffffff) != (reg_204 & 0x00ffffff)) {
				/* RTW_INFO("%s: (HW_VAR_CHECK_TXBUF)TXBUF NOT empty - 0x204=0x%x, 0x200=0x%x (%d)\n", __FUNCTION__, reg_204, reg_200, i); */
				rtw_msleep_os(10);
			} else
				break;
		}

		pass_ms = rtw_get_passing_time_ms(start);

		if (RTW_CANNOT_RUN(padapter))
			;
		else if (pass_ms >= 2000 || (reg_200 & 0x00ffffff) != (reg_204 & 0x00ffffff)) {
			RTW_PRINT("%s:(HW_VAR_CHECK_TXBUF)NOT empty(%d) in %d ms\n", __FUNCTION__, i, pass_ms);
			RTW_PRINT("%s:(HW_VAR_CHECK_TXBUF)0x200=0x%08x, 0x204=0x%08x (0x%08x, 0x%08x)\n",
				__FUNCTION__, reg_200, reg_204, init_reg_200, init_reg_204);
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
		if (IS_HARDWARE_TYPE_8821(padapter) && padapter->hw_port == HW_PORT1) {
			val8 = rtw_read8(padapter, REG_DWBCN1_CTRL_8812 + 2);
			val8 |= BIT(0);
			rtw_write8(padapter, REG_DWBCN1_CTRL_8812 + 2, val8);
		} else
#endif
		{
			/* BCN_VALID, BIT16 of REG_TDECTRL = BIT0 of REG_TDECTRL+2, write 1 to clear, Clear by sw */
			val8 = rtw_read8(padapter, REG_TDECTRL + 2);
			val8 |= BIT(0);
			rtw_write8(padapter, REG_TDECTRL + 2, val8);
		}
		break;

	case HW_VAR_DL_BCN_SEL:
#ifdef CONFIG_CONCURRENT_MODE
		if (IS_HARDWARE_TYPE_8821(padapter) && padapter->hw_port == HW_PORT1) {
			/* SW_BCN_SEL - Port1 */
			val8 = rtw_read8(padapter, REG_DWBCN1_CTRL_8812 + 2);
			val8 |= BIT(4);
			rtw_write8(padapter, REG_DWBCN1_CTRL_8812 + 2, val8);
		} else
#endif
		{
			/* SW_BCN_SEL - Port0 */
			val8 = rtw_read8(padapter, REG_DWBCN1_CTRL_8812 + 2);
			val8 &= ~BIT(4);
			rtw_write8(padapter, REG_DWBCN1_CTRL_8812 + 2, val8);
		}
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

#ifdef CONFIG_BEAMFORMING

#if (BEAMFORMING_SUPPORT == 1) /*add by YuChen for PHYDM -TxBF AutoTest HW Timer 8812A*/
	case HW_VAR_HW_REG_TIMER_INIT: {
		HAL_HW_TIMER_TYPE TimerType = (*(PHAL_HW_TIMER_TYPE)pval) >> 16;

		rtw_write8(padapter, 0x164, 1);

		if (TimerType == HAL_TIMER_TXBF)
			rtw_write16(padapter, 0x15C, (*(pu2Byte)pval));
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
		HAL_HW_TIMER_TYPE TimerType = *(PHAL_HW_TIMER_TYPE)pval & 0xffff;

		if (TimerType == HAL_TIMER_TXBF) {
			u4Byte Reg15C = (*(pu4Byte)pval) >> 16 | BIT24 | BIT26;

			rtw_write8(padapter, 0x15F, 0x0);
			rtw_write32(padapter, 0x15F, Reg15C);
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

#else /*(BEAMFORMING_SUPPORT == 0) - for drv beamforming*/

	case HW_VAR_SOUNDING_ENTER:
		SetBeamformEnter_8812(padapter, *pval);
		break;

	case HW_VAR_SOUNDING_LEAVE:
		SetBeamformLeave_8812(padapter, *pval);
		break;

	case HW_VAR_SOUNDING_RATE:
		rtw_write8(padapter, REG_NDPA_OPT_CTRL_8812A, (MRateToHwRate(pval[1]) << 2 | pval[0]));
		break;

	case HW_VAR_SOUNDING_STATUS:
		SetBeamformStatus_8812(padapter, *pval);
		break;

	case HW_VAR_SOUNDING_FW_NDPA:
		SetBeamformFwTxBF_8812(padapter, *pval);
		break;

	case HW_VAR_SOUNDING_CLK:
		SetBeamformingCLK_8812(padapter);
		break;
#endif

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

	case HW_VAR_CH_SW_IQK_INFO_BACKUP:
		hw_var_backup_IQK_val(padapter, (struct hal_iqk_reg_backup *)pval);
		break;

	case HW_VAR_CH_SW_IQK_INFO_RESTORE:
		switch (*pval) {
		case CH_SW_USE_CASE_TDLS:
#ifdef CONFIG_TDLS
#ifdef CONFIG_TDLS_CH_SW
			hw_var_restore_IQK_val_TDLS(padapter);
#endif
#endif
			break;
		case CH_SW_USE_CASE_MCC:
#ifdef CONFIG_MCC_MODE
			hw_var_restore_IQK_val_MCC(padapter);
#endif /* CONFIG_MCC_MODE */
			break;
		}
		break;

#if defined(CONFIG_TDLS) && defined(CONFIG_TDLS_CH_SW)
	case HW_VAR_TDLS_BCN_EARLY_C2H_RPT:
		rtl8812_set_BcnEarly_C2H_Rpt_cmd(padapter, *pval);
		break;
#endif

	default:
		ret = SetHwReg(padapter, variable, pval);
		break;
	}

	return ret;
}

struct qinfo_8812a {
	u32 head:8;
	u32 pkt_num:7;
	u32 tail:8;
	u32 ac:2;
	u32 macid:7;
};

struct bcn_qinfo_8812a {
	u16 head:8;
	u16 pkt_num:8;
};

void dump_qinfo_8812a(void *sel, struct qinfo_8812a *info, const char *tag)
{
	/* if (info->pkt_num) */
	RTW_PRINT_SEL(sel, "%shead:0x%02x, tail:0x%02x, pkt_num:%u, macid:%u, ac:%u\n"
		, tag ? tag : "", info->head, info->tail, info->pkt_num, info->macid, info->ac
		     );
}

void dump_bcn_qinfo_8812a(void *sel, struct bcn_qinfo_8812a *info, const char *tag)
{
	/* if (info->pkt_num) */
	RTW_PRINT_SEL(sel, "%shead:0x%02x, pkt_num:%u\n"
		      , tag ? tag : "", info->head, info->pkt_num
		     );
}

void dump_mac_qinfo_8812a(void *sel, _adapter *adapter)
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

	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q0_info, "Q0 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q1_info, "Q1 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q2_info, "Q2 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q3_info, "Q3 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q4_info, "Q4 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q5_info, "Q5 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q6_info, "Q6 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&q7_info, "Q7 ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&mg_q_info, "MG ");
	dump_qinfo_8812a(sel, (struct qinfo_8812a *)&hi_q_info, "HI ");
	dump_bcn_qinfo_8812a(sel, (struct bcn_qinfo_8812a *)&bcn_q_info, "BCN ");
}

static void dump_mac_txfifo_8812a(void *sel, _adapter *adapter)
{
	u32 rqpn, rqpn_npq;
	u32 hpq, lpq, npq, epq, pubq;

	rqpn = rtw_read32(adapter, REG_FIFOPAGE);
	rqpn_npq = rtw_read32(adapter, REG_RQPN_NPQ);

	hpq = (rqpn & 0xFF);
	lpq = ((rqpn & 0xFF00)>>8);
	pubq = ((rqpn & 0xFF0000)>>16);
	npq = ((rqpn_npq & 0xFF00)>>8);
	epq = ((rqpn_npq & 0xFF000000)>>24);

	RTW_PRINT_SEL(sel, "Tx: available page num: ");
	if ((hpq == 0xEA) && (hpq == lpq) && (hpq == pubq))
		RTW_PRINT_SEL(sel, "N/A (reg val = 0xea)\n");
	else {
		/* 8821A have EPQ, 8812A have no EPQ. */
		if (IS_HARDWARE_TYPE_8821(adapter))
			RTW_PRINT_SEL(sel, "HPQ: %d, LPQ: %d, NPQ: %d, EPQ: %d, PUBQ: %d\n"
				, hpq, lpq, npq, epq, pubq);
		else
			RTW_PRINT_SEL(sel, "HPQ: %d, LPQ: %d, NPQ: %d, PUBQ: %d\n"
				, hpq, lpq, npq, pubq);
	}
}

void rtl8812a_read_wmmedca_reg(PADAPTER adapter, u16 *vo_params, u16 *vi_params, u16 *be_params, u16 *bk_params)
{
	u8 vo_reg_params[4];
	u8 vi_reg_params[4];
	u8 be_reg_params[4];
	u8 bk_reg_params[4];

	GetHwReg8812A(adapter, HW_VAR_AC_PARAM_VO, vo_reg_params);
	GetHwReg8812A(adapter, HW_VAR_AC_PARAM_VI, vi_reg_params);
	GetHwReg8812A(adapter, HW_VAR_AC_PARAM_BE, be_reg_params);
	GetHwReg8812A(adapter, HW_VAR_AC_PARAM_BK, bk_reg_params);

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

void GetHwReg8812A(PADAPTER padapter, u8 variable, u8 *pval)
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
		if (IS_HARDWARE_TYPE_8821(padapter) && padapter->hw_port == HW_PORT1) {
			val8 = rtw_read8(padapter, REG_DWBCN1_CTRL_8812 + 2);
			*pval = (BIT(0) & val8) ? _TRUE : _FALSE;
		} else
#endif
		{
			/* BCN_VALID, BIT16 of REG_TDECTRL = BIT0 of REG_TDECTRL+2 */
			val8 = rtw_read8(padapter, REG_TDECTRL + 2);
			*pval = (BIT(0) & val8) ? _TRUE : _FALSE;
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
	case HW_VAR_EFUSE_BT_USAGE:
		#ifdef HAL_EFUSE_MEMORY
		*pval = pHalData->EfuseHal.BTEfuseUsedPercentage;
		#endif
	break;	
	case HW_VAR_EFUSE_BT_BYTES:
		#ifdef HAL_EFUSE_MEMORY
		*((u16 *)pval) = pHalData->EfuseHal.BTEfuseUsedBytes;
		#else
		*((u16 *)pval) = BTEfuseUsedBytes;
		#endif
	break;

	case HW_VAR_CHK_HI_QUEUE_EMPTY:
		val16 = rtw_read16(padapter, REG_TXPKT_EMPTY);
		*pval = (val16 & BIT(10)) ? _TRUE : _FALSE;
		break;
	case HW_VAR_CHK_MGQ_CPU_EMPTY:
		val16 = rtw_read16(padapter, REG_TXPKT_EMPTY);
		*pval = (val16 & BIT(8)) ? _TRUE : _FALSE;
		break;
#ifdef CONFIG_WOWLAN
	case HW_VAR_SYS_CLKR:
		*pval = rtw_read8(padapter, REG_SYS_CLKR);
		break;
#endif

	case HW_VAR_DUMP_MAC_QUEUE_INFO:
		dump_mac_qinfo_8812a(pval, padapter);
		break;

	case HW_VAR_DUMP_MAC_TXFIFO:
		dump_mac_txfifo_8812a(pval, padapter);
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
u8 SetHalDefVar8812A(PADAPTER padapter, HAL_DEF_VARIABLE variable, void *pval)
{
	PHAL_DATA_TYPE pHalData;
	u8 bResult;


	pHalData = GET_HAL_DATA(padapter);
	bResult = _SUCCESS;

	switch (variable) {
	default:
		bResult = SetHalDefVar(padapter, variable, pval);
		break;
	}

	return bResult;
}

void hal_ra_info_dump(_adapter *padapter , void *sel)
{
	int i;
	u8 mac_id;
	u32 cmd;
	u32 ra_info1, ra_info2, bw_set;
	u32 rate_mask1, rate_mask2;
	u8 curr_tx_rate, curr_tx_sgi, hight_rate, lowest_rate;
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	HAL_DATA_TYPE *HalData = GET_HAL_DATA(padapter);

	for (i = 0; i < macid_ctl->num; i++) {
		if (rtw_macid_is_used(macid_ctl, i) && !rtw_macid_is_bmc(macid_ctl, i)) {

			mac_id = (u8) i;
			_RTW_PRINT_SEL(sel , "============ RA status check  Mac_id:%d ===================\n", mac_id);

			cmd = 0x40000100 | mac_id;
			rtw_write32(padapter, REG_HMEBOX_E2_E3_8812, cmd);
			rtw_msleep_os(10);
			ra_info1 = rtw_read32(padapter, REG_RSVD5_8812);
			curr_tx_sgi = rtw_get_current_tx_sgi(padapter, macid_ctl->sta[mac_id]);
			curr_tx_rate = rtw_get_current_tx_rate(padapter, macid_ctl->sta[mac_id]);

			_RTW_PRINT_SEL(sel , "[ ra_info1:0x%08x ] =>cur_tx_rate= %s,cur_sgi:%d\n",
				       ra_info1,
				       HDATA_RATE(curr_tx_rate),
				       curr_tx_sgi);
			_RTW_PRINT_SEL(sel , "[ ra_info1:0x%08x ] =>PWRSTS = 0x%02x\n", ra_info1, (ra_info1 >> 8)  & 0x07);

			cmd = 0x40000400 | mac_id;
			rtw_write32(padapter, REG_HMEBOX_E2_E3_8812, cmd);
			rtw_msleep_os(10);
			ra_info1 = rtw_read32(padapter, REG_RSVD5_8812);
			ra_info2 = rtw_read32(padapter, REG_RSVD6_8812);
			rate_mask1 = rtw_read32(padapter, REG_RSVD7_8812);
			rate_mask2 = rtw_read32(padapter, REG_RSVD8_8812);
			hight_rate = ra_info2 & 0xFF;
			lowest_rate = (ra_info2 >> 8)  & 0xFF;
			bw_set = (ra_info1 >> 8)  & 0xFF;

			_RTW_PRINT_SEL(sel , "[ ra_info1:0x%08x ] => VHT_EN=0x%02x, ", ra_info1, (ra_info1 >> 24) & 0xFF);

			switch (bw_set) {

			case CHANNEL_WIDTH_20:
				_RTW_PRINT_SEL(sel , "BW_setting=20M\n");
				break;

			case CHANNEL_WIDTH_40:
				_RTW_PRINT_SEL(sel , "BW_setting=40M\n");
				break;

			case CHANNEL_WIDTH_80:
				_RTW_PRINT_SEL(sel , "BW_setting=80M\n");
				break;

			case CHANNEL_WIDTH_160:
				_RTW_PRINT_SEL(sel , "BW_setting=160M\n");
				break;

			default:
				_RTW_PRINT_SEL(sel , "BW_setting=0x%02x\n", bw_set);
				break;

			}

			_RTW_PRINT_SEL(sel , "[ ra_info1:0x%08x ] =>RSSI=%d, DISRA=0x%02x\n", ra_info1, ra_info1 & 0xFF, (ra_info1 >> 16) & 0xFF);
			_RTW_PRINT_SEL(sel , "[ ra_info2:0x%08x ] =>hight_rate=%s, lowest_rate=%s, SGI=0x%02x, RateID=%d\n",
				       ra_info2,
				       HDATA_RATE(hight_rate),
				       HDATA_RATE(lowest_rate),
				       (ra_info2 >> 16) & 0xFF,
				       (ra_info2 >> 24) & 0xFF);
			_RTW_PRINT_SEL(sel , "rate_mask2=0x%08x, rate_mask1=0x%08x\n", rate_mask2, rate_mask1);


		}
	}
}
/*
 *	Description:
 *		Query setting of specified variable.
 */
u8 GetHalDefVar8812A(PADAPTER padapter, HAL_DEF_VARIABLE variable, void *pval)
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
#endif

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
		*((u32 *)pval) = AMPDU_DENSITY_VALUE_7;
		break;

	case HAL_DEF_TX_LDPC:
		if (IS_VENDOR_8812A_C_CUT(padapter))
			/* IOT issue, default disable */
			*(u8 *)pval = _FALSE;
		else if (IS_HARDWARE_TYPE_8821(padapter))
			*(u8 *)pval = _TRUE;
		else
			*(u8 *)pval = _FALSE;
		break;

	case HAL_DEF_RX_LDPC:
		if (IS_VENDOR_8812A_C_CUT(padapter))
			/* IOT issue, default disable */
			*(u8 *)pval = _FALSE;
		else if (IS_HARDWARE_TYPE_8821(padapter))
			*(u8 *)pval = _FALSE;
		else
			*(u8 *)pval = _FALSE;
		break;

	case HAL_DEF_TX_STBC:
		if (pHalData->rf_type == RF_1T2R || pHalData->rf_type == RF_1T1R)
			*(u8 *)pval = 0;
		else
			*(u8 *)pval = 1;
		break;

	case HAL_DEF_RX_STBC:
		*(u8 *)pval = 1;
		break;

	case HAL_DEF_EXPLICIT_BEAMFORMER:
		if (pHalData->rf_type == RF_2T2R)
			*((PBOOLEAN)pval) = _TRUE;
		else
			*((PBOOLEAN)pval) = _FALSE;
		break;

	case HAL_DEF_EXPLICIT_BEAMFORMEE:
		*((PBOOLEAN)pval) = _TRUE;
		break;

	case HW_DEF_RA_INFO_DUMP:
		hal_ra_info_dump(padapter, pval);
		break;

	case HAL_DEF_TX_PAGE_SIZE:
		if (IS_HARDWARE_TYPE_8812(padapter))
			*(u32 *)pval = PAGE_SIZE_512;
		else
			*(u32 *)pval = PAGE_SIZE_256;
		break;

	case HAL_DEF_TX_PAGE_BOUNDARY:
		if (!padapter->registrypriv.wifi_spec) {
			if (IS_HARDWARE_TYPE_8812(padapter))
				*(u8 *)pval = TX_PAGE_BOUNDARY_8812;
			else
				*(u8 *)pval = TX_PAGE_BOUNDARY_8821;
		} else {
			if (IS_HARDWARE_TYPE_8812(padapter))
				*(u8 *)pval = WMM_NORMAL_TX_PAGE_BOUNDARY_8812;
			else
				*(u8 *)pval = WMM_NORMAL_TX_PAGE_BOUNDARY_8821;
		}
		break;

	case HAL_DEF_TX_PAGE_BOUNDARY_WOWLAN:
		*(u8 *)pval = TX_PAGE_BOUNDARY_WOWLAN_8812;
		break;
	case HAL_DEF_RX_DMA_SZ_WOW:
		if (IS_HARDWARE_TYPE_8821(padapter))
			*(u32 *)pval = MAX_RX_DMA_BUFFER_SIZE_8821 - RESV_FMWF;
		else
			*(u32 *)pval = MAX_RX_DMA_BUFFER_SIZE_8812 - RESV_FMWF;
		break;
	case HAL_DEF_RX_DMA_SZ:
		if (IS_HARDWARE_TYPE_8821(padapter))
			*(u32 *)pval = RX_DMA_BOUNDARY_8821 + 1;
		else
			*(u32 *)pval = RX_DMA_BOUNDARY_8812 + 1;
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
void rtl8812a_combo_card_WifiOnlyHwInit(PADAPTER pdapter)
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

void rtl8812_set_hal_ops(struct hal_ops *pHalFunc)
{
	pHalFunc->dm_init = &rtl8812_init_dm_priv;
	pHalFunc->dm_deinit = &rtl8812_deinit_dm_priv;

	pHalFunc->SetBeaconRelatedRegistersHandler = &SetBeaconRelatedRegisters8812A;

	pHalFunc->read_chip_version = read_chip_version_8812a;

	pHalFunc->set_chnl_bw_handler = &PHY_SetSwChnlBWMode8812;

	pHalFunc->set_tx_power_level_handler = &PHY_SetTxPowerLevel8812;
	pHalFunc->get_tx_power_level_handler = &PHY_GetTxPowerLevel8812;

	pHalFunc->set_tx_power_index_handler = PHY_SetTxPowerIndex_8812A;
	pHalFunc->get_tx_power_index_handler = PHY_GetTxPowerIndex_8812A;

	pHalFunc->hal_dm_watchdog = &rtl8812_HalDmWatchDog;

	pHalFunc->run_thread = &rtl8812_start_thread;
	pHalFunc->cancel_thread = &rtl8812_stop_thread;

	pHalFunc->read_bbreg = &PHY_QueryBBReg8812;
	pHalFunc->write_bbreg = &PHY_SetBBReg8812;
	pHalFunc->read_rfreg = &PHY_QueryRFReg8812;
	pHalFunc->write_rfreg = &PHY_SetRFReg8812;

	pHalFunc->read_wmmedca_reg = &rtl8812a_read_wmmedca_reg;

	/* Efuse related function */
	pHalFunc->EfusePowerSwitch = &rtl8812_EfusePowerSwitch;
	pHalFunc->ReadEFuse = &rtl8812_ReadEFuse;
	pHalFunc->EFUSEGetEfuseDefinition = &rtl8812_EFUSE_GetEfuseDefinition;
	pHalFunc->EfuseGetCurrentSize = &rtl8812_EfuseGetCurrentSize;
	pHalFunc->Efuse_PgPacketRead = &rtl8812_Efuse_PgPacketRead;
	pHalFunc->Efuse_PgPacketWrite = &rtl8812_Efuse_PgPacketWrite;
	pHalFunc->Efuse_WordEnableDataWrite = &rtl8812_Efuse_WordEnableDataWrite;
	pHalFunc->Efuse_PgPacketWrite_BT = &Hal_EfusePgPacketWrite_BT;

#ifdef DBG_CONFIG_ERROR_DETECT
	pHalFunc->sreset_init_value = &sreset_init_value;
	pHalFunc->sreset_reset_value = &sreset_reset_value;
	pHalFunc->silentreset = &sreset_reset;
	pHalFunc->sreset_xmit_status_check = &rtl8812_sreset_xmit_status_check;
	pHalFunc->sreset_linked_status_check  = &rtl8812_sreset_linked_status_check;
	pHalFunc->sreset_get_wifi_status  = &sreset_get_wifi_status;
	pHalFunc->sreset_inprogress = &sreset_inprogress;
#endif /* DBG_CONFIG_ERROR_DETECT */

	pHalFunc->GetHalODMVarHandler = GetHalODMVar;
	pHalFunc->SetHalODMVarHandler = SetHalODMVar;
	pHalFunc->hal_notch_filter = &hal_notch_filter_8812;

	pHalFunc->c2h_handler = c2h_handler_8812a;

	pHalFunc->fill_h2c_cmd = &fill_h2c_cmd_8812;
	pHalFunc->fill_fake_txdesc = &rtl8812a_fill_fake_txdesc;
	pHalFunc->fw_dl = &FirmwareDownload8812;
	pHalFunc->hal_get_tx_buff_rsvd_page_num = &GetTxBufferRsvdPageNum8812;
}
