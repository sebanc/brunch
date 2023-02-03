// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_EFUSE_C_

#include <drv_types.h>
#include <hal_data.h>

#include "../hal/HalEfuseMask8723D_USB.h"

/*------------------------Define local variable------------------------------*/
u8 fakeEfuseBank = {0};
u32 fakeEfuseUsedBytes = {0};
u8 fakeEfuseContent[EFUSE_MAX_HW_SIZE] = {0};
u8 fakeEfuseInitMap[EFUSE_MAX_MAP_LEN] = {0};
u8 fakeEfuseModifiedMap[EFUSE_MAX_MAP_LEN] = {0};

u32 BTEfuseUsedBytes = {0};
u8 BTEfuseContent[EFUSE_MAX_BT_BANK][EFUSE_MAX_HW_SIZE];
u8 BTEfuseInitMap[EFUSE_BT_MAX_MAP_LEN] = {0};
u8 BTEfuseModifiedMap[EFUSE_BT_MAX_MAP_LEN] = {0};

u32 fakeBTEfuseUsedBytes = {0};
u8 fakeBTEfuseContent[EFUSE_MAX_BT_BANK][EFUSE_MAX_HW_SIZE];
u8 fakeBTEfuseInitMap[EFUSE_BT_MAX_MAP_LEN] = {0};
u8 fakeBTEfuseModifiedMap[EFUSE_BT_MAX_MAP_LEN] = {0};

u8 maskfileBuffer[64];
/*------------------------Define local variable------------------------------*/
static bool rtw_file_efuse_IsMasked(struct adapter * pAdapter, u16 Offset)
{
	int r = Offset / 16;
	int c = (Offset % 16) / 2;
	int result = 0;

	if (pAdapter->registrypriv.boffefusemask)
		return false;

	if (c < 4) /* Upper double word */
		result = (maskfileBuffer[r] & (0x10 << c));
	else
		result = (maskfileBuffer[r] & (0x01 << (c - 4)));

	return (result > 0) ? 0 : 1;
}

static bool efuse_IsMasked(struct adapter *pAdapter, u16 Offset)
{
	if (pAdapter->registrypriv.boffefusemask)
		return false;

	return (IS_MASKED(8723D, _MUSB, Offset)) ? true : false;
}

void rtw_efuse_mask_array(struct adapter * pAdapter, u8 *pArray)
{
}

u16 rtw_get_efuse_mask_arraylen(struct adapter * pAdapter)
{
	return 0;
}

static void rtw_mask_map_read(struct adapter * adapt, u16 addr, u16 cnts,
			      u8 *data)
{
	u16 i = 0;

	if (adapt->registrypriv.boffefusemask == 0) {
			for (i = 0; i < cnts; i++) {
				if (adapt->registrypriv.bFileMaskEfuse) {
					/*use file efuse mask.*/
					if (rtw_file_efuse_IsMasked(adapt,
								    addr + i))
						data[i] = 0xff;
				} else {
					if (efuse_IsMasked(adapt, addr + i))
						data[i] = 0xff;
				}
			}
	}
}

u8 rtw_efuse_mask_map_read(struct adapter * adapt, u16 addr, u16 cnts, u8 *data)
{
	u8 ret;
	u16 mapLen;

	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN,
				 (void *)&mapLen, false);
	ret = rtw_efuse_map_read(adapt, addr, cnts , data);

	rtw_mask_map_read(adapt, addr, cnts , data);

	return ret;
}

/* ***********************************************************
 *				Efuse related code
 * *********************************************************** */
static u8 hal_EfuseSwitchToBank(struct adapter *adapt, u8 bank, u8 bPseudoTest)
{
	u8 bRet = false;
	u32 value32 = 0;
#ifdef HAL_EFUSE_MEMORY
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);
	struct efuse_hal * pEfuseHal = &pHalData->EfuseHal;
#endif

	RTW_INFO("%s: Efuse switch bank to %d\n", __func__, bank);
	if (bPseudoTest) {
#ifdef HAL_EFUSE_MEMORY
		pEfuseHal->fakeEfuseBank = bank;
#else
		fakeEfuseBank = bank;
#endif
		bRet = true;
	} else {
		value32 = rtw_read32(adapt, 0x34);
		bRet = true;
		switch (bank) {
		case 0:
			value32 = (value32 & ~EFUSE_SEL_MASK) |
					      EFUSE_SEL(EFUSE_WIFI_SEL_0);
			break;
		case 1:
			value32 = (value32 & ~EFUSE_SEL_MASK) |
					     EFUSE_SEL(EFUSE_BT_SEL_0);
			break;
		case 2:
			value32 = (value32 & ~EFUSE_SEL_MASK) |
					     EFUSE_SEL(EFUSE_BT_SEL_1);
			break;
		case 3:
			value32 = (value32 & ~EFUSE_SEL_MASK) |
					     EFUSE_SEL(EFUSE_BT_SEL_2);
			break;
		default:
			value32 = (value32 & ~EFUSE_SEL_MASK) |
					     EFUSE_SEL(EFUSE_WIFI_SEL_0);
			bRet = false;
			break;
		}
		rtw_write32(adapt, 0x34, value32);
	}
	return bRet;
}

void rtw_efuse_analyze(struct adapter *	adapt, u8 Type, u8 Fake)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	struct efuse_hal *pEfuseHal = &(pHalData->EfuseHal);
	u16 eFuse_Addr = 0;
	u8 offset, wden;
	u16  i, j;
	u8 efuseHeader = 0, efuseExtHdr = 0;
	u8 efuseData[EFUSE_MAX_WORD_UNIT*2] = {0}, dataCnt = 0;
	u16 efuseHeader2Byte = 0;
	u8 *eFuseWord = NULL;
	u8 offset_2_0 = 0;
	u8 pgSectionCnt = 0;
	u8 wd_cnt = 0;
	u8 max_section = 64;
	u16 mapLen = 0, maprawlen = 0;
	bool bExtHeader = false;
	u8 efuseType = EFUSE_WIFI;
	bool bPseudoTest = false;
	u8 bank = 0, startBank = 0, endBank = 1-1;
	bool bCheckNextBank = false;
	u8 protectBytesBank = 0;
	u16 efuse_max = 0;
	u8 ParseEfuseExtHdr, ParseEfuseHeader, ParseOffset, ParseWDEN;
	u8 ParseOffset2_0;

	eFuseWord = rtw_zmalloc(EFUSE_MAX_SECTION_NUM *
				(EFUSE_MAX_WORD_UNIT * 2));
	RTW_INFO("\n");
	if (Type == 0) {
		if (Fake == 0) {
			RTW_INFO("\n\tEFUSE_Analyze Wifi Content\n");
			efuseType = EFUSE_WIFI;
			bPseudoTest = false;
			startBank = 0;
			endBank = 0;
		} else {
			RTW_INFO("\n\tEFUSE_Analyze Wifi Pseudo Content\n");
			efuseType = EFUSE_WIFI;
			bPseudoTest = true;
			startBank = 0;
			endBank = 0;
		}
	} else {
		if (Fake == 0) {
			RTW_INFO("\n\tEFUSE_Analyze BT Content\n");
			efuseType = EFUSE_BT;
			bPseudoTest = false;
			startBank = 1;
			endBank = EFUSE_MAX_BANK - 1;
		} else {
			RTW_INFO("\n\tEFUSE_Analyze BT Pseudo Content\n");
			efuseType = EFUSE_BT;
			bPseudoTest = true;
			startBank = 1;
			endBank = EFUSE_MAX_BANK - 1;
		}
	}

	RTW_INFO("\n\r 1Byte header, [7:4]=offset, [3:0]=word enable\n");
	RTW_INFO("\n\r 2Byte header, header[7:5]=offset[2:0], header[4:0]=0x0F\n");
	RTW_INFO("\n\r 2Byte header, extHeader[7:4]=offset[6:3], extHeader[3:0]=word enable\n");

	EFUSE_GetEfuseDefinition(adapt, efuseType, TYPE_EFUSE_MAP_LEN,
				 (void *)&mapLen, bPseudoTest);
	EFUSE_GetEfuseDefinition(adapt, efuseType, TYPE_EFUSE_MAX_SECTION,
				 (void *)&max_section, bPseudoTest);
	EFUSE_GetEfuseDefinition(adapt, efuseType,
				 TYPE_EFUSE_PROTECT_BYTES_BANK,
				 (void *)&protectBytesBank, bPseudoTest);
	EFUSE_GetEfuseDefinition(adapt, efuseType,
				 TYPE_EFUSE_CONTENT_LEN_BANK,
				 (void *)&efuse_max, bPseudoTest);
	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI,
				 TYPE_EFUSE_REAL_CONTENT_LEN,
				 (void *)&maprawlen, false);
	memset(eFuseWord, 0xff, EFUSE_MAX_SECTION_NUM *
	       (EFUSE_MAX_WORD_UNIT * 2));
	memset(pEfuseHal->fakeEfuseInitMap, 0xff, EFUSE_MAX_MAP_LEN);

	for (bank = startBank; bank <= endBank; bank++) {
		if (!hal_EfuseSwitchToBank(adapt, bank, bPseudoTest)) {
			RTW_INFO("EFUSE_SwitchToBank() Fail!!\n");
			return;
		}

		eFuse_Addr = bank * EFUSE_MAX_BANK_SIZE;

		efuse_OneByteRead(adapt, eFuse_Addr++, &efuseHeader, bPseudoTest);

		if (efuseHeader == 0xFF && bank == startBank && !Fake) {
			RTW_INFO("Non-PGed Efuse\n");
			return;
		}
		RTW_INFO("EFUSE_REAL_CONTENT_LEN = %d\n", maprawlen);

		while ((efuseHeader != 0xFF) && ((efuseType == EFUSE_WIFI &&
		       (eFuse_Addr < maprawlen)) || (efuseType == EFUSE_BT &&
		       (eFuse_Addr < (endBank + 1) * EFUSE_MAX_BANK_SIZE)))) {
			RTW_INFO("Analyzing: Offset: 0x%X\n", eFuse_Addr);
			/* Check PG header for section num.*/
			if (EXT_HEADER(efuseHeader)) {
				bExtHeader = true;
				offset_2_0 = GET_HDR_OFFSET_2_0(efuseHeader);
				efuse_OneByteRead(adapt, eFuse_Addr++,
						  &efuseExtHdr, bPseudoTest);
				if (efuseExtHdr != 0xff) {
					if (ALL_WORDS_DISABLED(efuseExtHdr)) {
						/* Read next pg header*/
						efuse_OneByteRead(adapt,
								  eFuse_Addr++,
								  &efuseHeader,
								  bPseudoTest);
						continue;
					} else {
						offset = ((efuseExtHdr & 0xF0) >> 1) | offset_2_0;
						wden = (efuseExtHdr & 0x0F);
						efuseHeader2Byte = (efuseExtHdr<<8)|efuseHeader;
						RTW_INFO("Find efuseHeader2Byte = 0x%04X, offset=%d, wden=0x%x\n",
										efuseHeader2Byte, offset, wden);
					}
				} else {
					RTW_INFO("Error, efuse[%d]=0xff, efuseExtHdr=0xff\n", eFuse_Addr-1);
					break;
				}
			} else {
				offset = ((efuseHeader >> 4) & 0x0f);
				wden = (efuseHeader & 0x0f);
			}

			memset(efuseData, '\0', EFUSE_MAX_WORD_UNIT * 2);
			dataCnt = 0;

			if (offset < max_section) {
				for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++) {
					/* Check word enable condition in the section	*/
					if (!(wden & (0x01<<i))) {
						if (!((efuseType == EFUSE_WIFI && (eFuse_Addr + 2 < maprawlen)) ||
								(efuseType == EFUSE_BT && (eFuse_Addr + 2 < (endBank + 1) * EFUSE_MAX_BANK_SIZE)))) {
							RTW_INFO("eFuse_Addr exceeds, break\n");
							break;
						}
						efuse_OneByteRead(adapt, eFuse_Addr++, &efuseData[dataCnt++], bPseudoTest);
						eFuseWord[(offset * 8) + (i * 2)] = (efuseData[dataCnt - 1]);
						efuse_OneByteRead(adapt, eFuse_Addr++, &efuseData[dataCnt++], bPseudoTest);
						eFuseWord[(offset * 8) + (i * 2 + 1)] = (efuseData[dataCnt - 1]);
					}
				}
			}

			if (bExtHeader) {
				RTW_INFO("Efuse PG Section (%d) = ", pgSectionCnt);
				RTW_INFO("[ %04X ], [", efuseHeader2Byte);

			} else {
				RTW_INFO("Efuse PG Section (%d) = ", pgSectionCnt);
				RTW_INFO("[ %02X ], [", efuseHeader);
			}

			for (j = 0; j < dataCnt; j++)
				RTW_INFO(" %02X ", efuseData[j]);

			RTW_INFO("]\n");


			if (bExtHeader) {
				ParseEfuseExtHdr = (efuseHeader2Byte & 0xff00) >> 8;
				ParseEfuseHeader = (efuseHeader2Byte & 0xff);
				ParseOffset2_0 = GET_HDR_OFFSET_2_0(ParseEfuseHeader);
				ParseOffset = ((ParseEfuseExtHdr & 0xF0) >> 1) | ParseOffset2_0;
				ParseWDEN = (ParseEfuseExtHdr & 0x0F);
				RTW_INFO("Header=0x%x, ExtHeader=0x%x, ", ParseEfuseHeader, ParseEfuseExtHdr);
			} else {
				ParseEfuseHeader = efuseHeader;
				ParseOffset = ((ParseEfuseHeader >> 4) & 0x0f);
				ParseWDEN = (ParseEfuseHeader & 0x0f);
				RTW_INFO("Header=0x%x, ", ParseEfuseHeader);
			}
			RTW_INFO("offset=0x%x(%d), word enable=0x%x\n", ParseOffset, ParseOffset, ParseWDEN);

			wd_cnt = 0;
			for (i = 0; i < EFUSE_MAX_WORD_UNIT; i++) {
				if (!(wden & (0x01 << i))) {
					RTW_INFO("Map[ %02X ] = %02X %02X\n", ((offset * EFUSE_MAX_WORD_UNIT * 2) + (i * 2)), efuseData[wd_cnt * 2 + 0], efuseData[wd_cnt * 2 + 1]);
					wd_cnt++;
				}
			}

			pgSectionCnt++;
			bExtHeader = false;
			efuse_OneByteRead(adapt, eFuse_Addr++, &efuseHeader, bPseudoTest);
			if (efuseHeader == 0xFF) {
				if ((eFuse_Addr + protectBytesBank) >= efuse_max)
					bCheckNextBank = true;
				else
					bCheckNextBank = false;
			}
		}
		if (!bCheckNextBank) {
			RTW_INFO("Not need to check next bank, eFuse_Addr=%d, protectBytesBank=%d, efuse_max=%d\n",
				eFuse_Addr, protectBytesBank, efuse_max);
			break;
		}
	}
	/* switch bank back to 0 for BT/wifi later use*/
	hal_EfuseSwitchToBank(adapt, 0, bPseudoTest);

	/* 3. Collect 16 sections and 4 word unit into Efuse map.*/
	for (i = 0; i < max_section; i++) {
		for (j = 0; j < EFUSE_MAX_WORD_UNIT; j++) {
			pEfuseHal->fakeEfuseInitMap[(i*8)+(j*2)] = (eFuseWord[(i*8)+(j*2)]);
			pEfuseHal->fakeEfuseInitMap[(i*8)+((j*2)+1)] =  (eFuseWord[(i*8)+((j*2)+1)]);
		}
	}

	RTW_INFO("\n\tEFUSE Analyze Map\n");
	i = 0;
	j = 0;

	for (i = 0; i < mapLen; i++) {
		if (i % 16 == 0)
			RTW_PRINT_SEL(RTW_DBGDUMP, "0x%03x: ", i);
		_RTW_PRINT_SEL(RTW_DBGDUMP, "%02X%s", pEfuseHal->fakeEfuseInitMap[i],
			       ((i + 1) % 16 == 0) ? "\n" : (((i + 1) % 8 == 0) ? "	  " : " ")
			      );
		}
	_RTW_PRINT_SEL(RTW_DBGDUMP, "\n");
	if (eFuseWord)
		rtw_mfree((u8 *)eFuseWord, EFUSE_MAX_SECTION_NUM * (EFUSE_MAX_WORD_UNIT * 2));
}


/* ------------------------------------------------------------------------------ */
#define REG_EFUSE_CTRL		0x0030
#define EFUSE_CTRL			REG_EFUSE_CTRL		/* E-Fuse Control. */
/* ------------------------------------------------------------------------------ */

static void efuse_PreUpdateAction(
	struct adapter *	pAdapter,
	u32 *	BackupRegs)
{
}

static void efuse_PostUpdateAction(
	struct adapter *	pAdapter,
	u32 *	BackupRegs)
{
}


bool
Efuse_Read1ByteFromFakeContent(
		struct adapter *	pAdapter,
		u16 Offset,
	u8 *Value);
bool
Efuse_Read1ByteFromFakeContent(
		struct adapter *	pAdapter,
		u16 Offset,
	u8 *Value)
{
	if (Offset >= EFUSE_MAX_HW_SIZE)
		return false;
	/* DbgPrint("Read fake content, offset = %d\n", Offset); */
	if (fakeEfuseBank == 0)
		*Value = fakeEfuseContent[Offset];
	else
		*Value = fakeBTEfuseContent[fakeEfuseBank - 1][Offset];
	return true;
}

bool
Efuse_Write1ByteToFakeContent(
		struct adapter *	pAdapter,
		u16 Offset,
		u8 Value);
bool
Efuse_Write1ByteToFakeContent(
		struct adapter *	pAdapter,
		u16 Offset,
		u8 Value)
{
	if (Offset >= EFUSE_MAX_HW_SIZE)
		return false;
	if (fakeEfuseBank == 0)
		fakeEfuseContent[Offset] = Value;
	else
		fakeBTEfuseContent[fakeEfuseBank - 1][Offset] = Value;
	return true;
}

/*-----------------------------------------------------------------------------
 * Function:	Efuse_PowerSwitch
 *
 * Overview:	When we want to enable write operation, we should change to
 *				pwr on state. When we stop write, we should switch to 500k mode
 *				and disable LDO 2.5V.
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/17/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
void
Efuse_PowerSwitch(
	struct adapter *	pAdapter,
	u8 bWrite,
	u8 PwrState)
{
	pAdapter->hal_func.EfusePowerSwitch(pAdapter, bWrite, PwrState);
}

void
BTEfuse_PowerSwitch(
	struct adapter *	pAdapter,
	u8 bWrite,
	u8 PwrState)
{
	if (pAdapter->hal_func.BTEfusePowerSwitch)
		pAdapter->hal_func.BTEfusePowerSwitch(pAdapter, bWrite, PwrState);
}

/*-----------------------------------------------------------------------------
 * Function:	efuse_GetCurrentSize
 *
 * Overview:	Get current efuse size!!!
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/16/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
u16
Efuse_GetCurrentSize(
	struct adapter *		pAdapter,
	u8 efuseType,
	bool bPseudoTest)
{
	u16 ret = 0;

	ret = pAdapter->hal_func.EfuseGetCurrentSize(pAdapter, efuseType, bPseudoTest);

	return ret;
}

/*
 *	Description:
 *		Execute E-Fuse read byte operation.
 *		Refered from SD1 Richard.
 *
 *	Assumption:
 *		1. Boot from E-Fuse and successfully auto-load.
 *		2. PASSIVE_LEVEL (USB interface)
 *
 *	Created by Roger, 2008.10.21.
 *   */
void
ReadEFuseByte(
	struct adapter *	Adapter,
	u16 _offset,
	u8 *pbuf,
	bool bPseudoTest)
{
	u32 value32;
	u8 readbyte;
	u16 retry;
	/* unsigned long start=rtw_get_current_time(); */

	if (bPseudoTest) {
		Efuse_Read1ByteFromFakeContent(Adapter, _offset, pbuf);
		return;
	}
	/* Write Address */
	rtw_write8(Adapter, EFUSE_CTRL + 1, (_offset & 0xff));
	readbyte = rtw_read8(Adapter, EFUSE_CTRL + 2);
	rtw_write8(Adapter, EFUSE_CTRL + 2, ((_offset >> 8) & 0x03) | (readbyte & 0xfc));

	/* Write bit 32 0 */
	readbyte = rtw_read8(Adapter, EFUSE_CTRL + 3);
	rtw_write8(Adapter, EFUSE_CTRL + 3, (readbyte & 0x7f));

	/* Check bit 32 read-ready */
	retry = 0;
	value32 = rtw_read32(Adapter, EFUSE_CTRL);
	/* while(!(((value32 >> 24) & 0xff) & 0x80)  && (retry<10)) */
	while (!(((value32 >> 24) & 0xff) & 0x80)  && (retry < 10000)) {
		value32 = rtw_read32(Adapter, EFUSE_CTRL);
		retry++;
	}

	/* 20100205 Joseph: Add delay suggested by SD1 Victor. */
	/* This fix the problem that Efuse read error in high temperature condition. */
	/* Designer says that there shall be some delay after ready bit is set, or the */
	/* result will always stay on last data we read. */
	rtw_udelay_os(50);
	value32 = rtw_read32(Adapter, EFUSE_CTRL);

	*pbuf = (u8)(value32 & 0xff);
	/* RTW_INFO("ReadEFuseByte _offset:%08u, in %d ms\n",_offset ,rtw_get_passing_time_ms(start)); */

}

/*
 *	Description:
 *		1. Execute E-Fuse read byte operation according as map offset and
 *		    save to E-Fuse table.
 *		2. Refered from SD1 Richard.
 *
 *	Assumption:
 *		1. Boot from E-Fuse and successfully auto-load.
 *		2. PASSIVE_LEVEL (USB interface)
 *
 *	Created by Roger, 2008.10.21.
 *
 *	2008/12/12 MH	1. Reorganize code flow and reserve bytes. and add description.
 *					2. Add efuse utilization collect.
 *	2008/12/22 MH	Read Efuse must check if we write section 1 data again!!! Sec1
 *					write addr must be after sec5.
 *   */

void
efuse_ReadEFuse(
	struct adapter *	Adapter,
	u8 efuseType,
	u16 _offset,
	u16 _size_byte,
	u8 *pbuf,
	bool bPseudoTest
);
void
efuse_ReadEFuse(
	struct adapter *	Adapter,
	u8 efuseType,
	u16 _offset,
	u16 _size_byte,
	u8 *pbuf,
	bool bPseudoTest
)
{
	Adapter->hal_func.ReadEFuse(Adapter, efuseType, _offset, _size_byte, pbuf, bPseudoTest);
}

void
EFUSE_GetEfuseDefinition(
		struct adapter *	pAdapter,
		u8 efuseType,
		u8 type,
	void		*pOut,
		bool bPseudoTest
)
{
	pAdapter->hal_func.EFUSEGetEfuseDefinition(pAdapter, efuseType, type, pOut, bPseudoTest);
}


/*  11/16/2008 MH Read one byte from real Efuse. */
u8 efuse_OneByteRead(struct adapter *pAdapter, u16 addr, u8 *data, bool bPseudoTest)
{
	u32 tmpidx = 0;
	u8 bResult;
	u8 readbyte;
	struct hal_com_data	*pHalData = GET_HAL_DATA(pAdapter);
	u8 tmp;

	/* RTW_INFO("===> EFUSE_OneByteRead(), addr = %x\n", addr); */
	/* RTW_INFO("===> EFUSE_OneByteRead() start, 0x34 = 0x%X\n", rtw_read32(pAdapter, EFUSE_TEST)); */

	if (bPseudoTest) {
		bResult = Efuse_Read1ByteFromFakeContent(pAdapter, addr, data);
		return bResult;
	}
	if (IS_CHIP_VENDOR_SMIC(pHalData->version_id)) {
		/* <20130121, Kordan> For SMIC EFUSE specificatoin. */
		/* 0x34[11]: SW force PGMEN input of efuse to high. (for the bank selected by 0x34[9:8])         */
		rtw_write16(pAdapter, 0x34, rtw_read16(pAdapter, 0x34) & (~BIT11));
	}

	/* -----------------e-fuse reg ctrl --------------------------------- */
	/* address			 */
	tmp =  (u8)(addr & 0xff);
	rtw_write8(pAdapter, EFUSE_CTRL + 1, tmp);
	tmp = (u8)((addr >> 8) & 0x03) | (rtw_read8(pAdapter, EFUSE_CTRL + 2) & 0xFC);
	rtw_write8(pAdapter, EFUSE_CTRL + 2, tmp);

	readbyte = rtw_read8(pAdapter, EFUSE_CTRL + 3);
	tmp = readbyte & 0x7f;

	rtw_write8(pAdapter, EFUSE_CTRL + 3, tmp);

	while (!(0x80 & rtw_read8(pAdapter, EFUSE_CTRL + 3)) && (tmpidx < 1000)) {
		rtw_mdelay_os(1);
		tmpidx++;
	}
	if (tmpidx < 1000) {
		*data = rtw_read8(pAdapter, EFUSE_CTRL);
		bResult = true;
	} else {
		*data = 0xff;
		bResult = false;
		RTW_INFO("%s: [ERROR] addr=0x%x bResult=%d time out 1s !!!\n", __func__, addr, bResult);
		RTW_INFO("%s: [ERROR] EFUSE_CTRL =0x%08x !!!\n", __func__, rtw_read32(pAdapter, EFUSE_CTRL));
	}

	return bResult;
}

/*  11/16/2008 MH Write one byte to reald Efuse. */
u8
efuse_OneByteWrite(
	struct adapter *	pAdapter,
	u16 addr,
	u8 data,
	bool bPseudoTest)
{
	u8 tmpidx = 0;
	u8 bResult = false;
	u32 efuseValue = 0;
	struct hal_com_data	*pHalData = GET_HAL_DATA(pAdapter);

	/* RTW_INFO("===> EFUSE_OneByteWrite(), addr = %x data=%x\n", addr, data); */
	/* RTW_INFO("===> EFUSE_OneByteWrite() start, 0x34 = 0x%X\n", rtw_read32(pAdapter, EFUSE_TEST)); */

	if (bPseudoTest) {
		bResult = Efuse_Write1ByteToFakeContent(pAdapter, addr, data);
		return bResult;
	}

	Efuse_PowerSwitch(pAdapter, true, true);

	/* -----------------e-fuse reg ctrl ---------------------------------	 */
	/* address			 */


	efuseValue = rtw_read32(pAdapter, EFUSE_CTRL);
	efuseValue |= (BIT21 | BIT31);
	efuseValue &= ~(0x3FFFF);
	efuseValue |= ((addr << 8 | data) & 0x3FFFF);

	if (IS_CHIP_VENDOR_SMIC(pHalData->version_id)) {
		/* <20130121, Kordan> For SMIC EFUSE specificatoin. */
		/* 0x34[11]: SW force PGMEN input of efuse to high. (for the bank selected by 0x34[9:8]) */
		rtw_write16(pAdapter, 0x34, rtw_read16(pAdapter, 0x34) | (BIT11));
		rtw_write32(pAdapter, EFUSE_CTRL, 0x90600000 | ((addr << 8 | data)));
	} else {
		rtw_write32(pAdapter, EFUSE_CTRL, efuseValue);
	}
	rtw_mdelay_os(1);

	while ((0x80 &  rtw_read8(pAdapter, EFUSE_CTRL + 3)) && (tmpidx < 100)) {
		rtw_mdelay_os(1);
		tmpidx++;
	}

	if (tmpidx < 100)
		bResult = true;
	else {
		bResult = false;
		RTW_INFO("%s: [ERROR] addr=0x%x ,efuseValue=0x%x ,bResult=%d time out 1s !!!\n",
			 __func__, addr, efuseValue, bResult);
		RTW_INFO("%s: [ERROR] EFUSE_CTRL =0x%08x !!!\n", __func__, rtw_read32(pAdapter, EFUSE_CTRL));
	}

	/* disable Efuse program enable */
	if (IS_CHIP_VENDOR_SMIC(pHalData->version_id))
		phy_set_mac_reg(pAdapter, EFUSE_TEST, BIT(11), 0);

	Efuse_PowerSwitch(pAdapter, true, false);

	return bResult;
}

int Efuse_PgPacketRead(struct adapter *	pAdapter, u8 offset,
		   u8 *data,
		   bool bPseudoTest)
{
	int	ret = 0;

	ret =  pAdapter->hal_func.Efuse_PgPacketRead(pAdapter, offset, data, bPseudoTest);

	return ret;
}

int
Efuse_PgPacketWrite(struct adapter *	pAdapter,
		    u8 offset,
		    u8 word_en,
		    u8 *data,
		    bool bPseudoTest)
{
	int ret;

	ret =  pAdapter->hal_func.Efuse_PgPacketWrite(pAdapter, offset, word_en, data, bPseudoTest);

	return ret;
}


static int
Efuse_PgPacketWrite_BT(struct adapter *	pAdapter,
		       u8 offset,
		       u8 word_en,
		       u8 *data,
		       bool bPseudoTest)
{
	int ret;

	ret =  pAdapter->hal_func.Efuse_PgPacketWrite_BT(pAdapter, offset, word_en, data, bPseudoTest);

	return ret;
}


u8
Efuse_WordEnableDataWrite(struct adapter *	pAdapter,
			  u16 efuse_addr,
			  u8 word_en,
			  u8 *data,
			  bool bPseudoTest)
{
	u8 ret = 0;

	ret =  pAdapter->hal_func.Efuse_WordEnableDataWrite(pAdapter, efuse_addr, word_en, data, bPseudoTest);

	return ret;
}

static u8 efuse_read8(struct adapter * adapt, u16 address, u8 *value)
{
	return efuse_OneByteRead(adapt, address, value, false);
}

static u8 efuse_write8(struct adapter * adapt, u16 address, u8 *value)
{
	return efuse_OneByteWrite(adapt, address, *value, false);
}

/*
 * read/wirte raw efuse data
 */
u8 rtw_efuse_access(struct adapter * adapt, u8 bWrite, u16 start_addr, u16 cnts, u8 *data)
{
	int i = 0;
	u16 real_content_len = 0, max_available_size = 0;
	u8 res = _FAIL ;
	u8(*rw8)(struct adapter *, u16, u8 *);
	u32 backupRegs[4] = {0};


	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_REAL_CONTENT_LEN, (void *)&real_content_len, false);
	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, (void *)&max_available_size, false);

	if (start_addr > real_content_len)
		return _FAIL;

	if (bWrite) {
		if ((start_addr + cnts) > max_available_size)
			return _FAIL;
		rw8 = &efuse_write8;
	} else
		rw8 = &efuse_read8;

	efuse_PreUpdateAction(adapt, backupRegs);

	Efuse_PowerSwitch(adapt, bWrite, true);

	/* e-fuse one byte read / write */
	for (i = 0; i < cnts; i++) {
		if (start_addr >= real_content_len) {
			res = _FAIL;
			break;
		}

		res = rw8(adapt, start_addr++, data++);
		if (_FAIL == res)
			break;
	}

	Efuse_PowerSwitch(adapt, bWrite, false);

	efuse_PostUpdateAction(adapt, backupRegs);

	return res;
}
/* ------------------------------------------------------------------------------ */
u16 efuse_GetMaxSize(struct adapter * adapt)
{
	u16 max_size;

	max_size = 0;
	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI , TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, (void *)&max_size, false);
	return max_size;
}
/* ------------------------------------------------------------------------------ */
u8 efuse_GetCurrentSize(struct adapter * adapt, u16 *size)
{
	Efuse_PowerSwitch(adapt, false, true);
	*size = Efuse_GetCurrentSize(adapt, EFUSE_WIFI, false);
	Efuse_PowerSwitch(adapt, false, false);

	return _SUCCESS;
}
/* ------------------------------------------------------------------------------ */
u16 efuse_bt_GetMaxSize(struct adapter * adapt)
{
	u16 max_size;

	max_size = 0;
	EFUSE_GetEfuseDefinition(adapt, EFUSE_BT , TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, (void *)&max_size, false);
	return max_size;
}

u8 efuse_bt_GetCurrentSize(struct adapter * adapt, u16 *size)
{
	Efuse_PowerSwitch(adapt, false, true);
	*size = Efuse_GetCurrentSize(adapt, EFUSE_BT, false);
	Efuse_PowerSwitch(adapt, false, false);

	return _SUCCESS;
}

u8 rtw_efuse_map_read(struct adapter * adapt, u16 addr, u16 cnts, u8 *data)
{
	u16 mapLen = 0;

	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, false);

	if ((addr + cnts) > mapLen)
		return _FAIL;

	Efuse_PowerSwitch(adapt, false, true);

	efuse_ReadEFuse(adapt, EFUSE_WIFI, addr, cnts, data, false);

	Efuse_PowerSwitch(adapt, false, false);

	return _SUCCESS;
}

u8 rtw_BT_efuse_map_read(struct adapter * adapt, u16 addr, u16 cnts, u8 *data)
{
	u16 mapLen = 0;

	EFUSE_GetEfuseDefinition(adapt, EFUSE_BT, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, false);

	if ((addr + cnts) > mapLen)
		return _FAIL;

	Efuse_PowerSwitch(adapt, false, true);

	efuse_ReadEFuse(adapt, EFUSE_BT, addr, cnts, data, false);

	Efuse_PowerSwitch(adapt, false, false);

	return _SUCCESS;
}

/* ------------------------------------------------------------------------------ */
u8 rtw_efuse_map_write(struct adapter * adapt, u16 addr, u16 cnts, u8 *data)
{
#define RT_ASSERT_RET(expr)												\
	if (!(expr)) {															\
		printk("Assertion failed! %s at ......\n", #expr);							\
		printk("      ......%s,%s, line=%d\n",__FILE__, __func__, __LINE__);	\
		return _FAIL;	\
	}

	u8 *efuse = NULL;
	u8 offset, word_en;
	u8 *map = NULL;
	u8 newdata[PGPKT_DATA_SIZE];
	int	i, j, idx, chk_total_byte;
	u8 ret = _SUCCESS;
	u16 mapLen = 0, startAddr = 0, efuse_max_available_len = 0;
	u32 backupRegs[4] = {0};

	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, false);
	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_AVAILABLE_EFUSE_BYTES_TOTAL, &efuse_max_available_len, false);

	if ((addr + cnts) > mapLen)
		return _FAIL;

	RT_ASSERT_RET(PGPKT_DATA_SIZE == 8); /* have to be 8 byte alignment */
	RT_ASSERT_RET((mapLen & 0x7) == 0); /* have to be PGPKT_DATA_SIZE alignment for memcpy */

	efuse = rtw_zmalloc(mapLen);
	if (!efuse)
		return _FAIL;

	map = rtw_zmalloc(mapLen);
	if (!map) {
		rtw_mfree(efuse, mapLen);
		return _FAIL;
	}

	memset(map, 0xFF, mapLen);

	ret = rtw_efuse_map_read(adapt, 0, mapLen, map);
	if (ret == _FAIL)
		goto exit;

	memcpy(efuse , map, mapLen);
	memcpy(efuse + addr, data, cnts);

	if (adapt->registrypriv.boffefusemask == 0) {
		for (i = 0; i < cnts; i++) {
			if (adapt->registrypriv.bFileMaskEfuse) {
				if (rtw_file_efuse_IsMasked(adapt, addr + i))	/*use file efuse mask. */
					efuse[addr + i] = map[addr + i];
			} else {
				if (efuse_IsMasked(adapt, addr + i))
					efuse[addr + i] = map[addr + i];
			}
			RTW_INFO("%s , data[%d] = %x, map[addr+i]= %x\n", __func__, addr + i, efuse[ addr + i], map[addr + i]);
		}
	}
	/*Efuse_PowerSwitch(adapt, true, true);*/

	chk_total_byte = 0;
	idx = 0;
	offset = (addr >> 3);

	while (idx < cnts) {
		word_en = 0xF;
		j = (addr + idx) & 0x7;
		for (i = j; i < PGPKT_DATA_SIZE && idx < cnts; i++, idx++) {
			if (efuse[addr + idx] != map[addr + idx])
				word_en &= ~BIT(i >> 1);
		}

		if (word_en != 0xF) {
			chk_total_byte += Efuse_CalculateWordCnts(word_en) * 2;

			if (offset >= EFUSE_MAX_SECTION_BASE) /* Over EFUSE_MAX_SECTION 16 for 2 ByteHeader */
				chk_total_byte += 2;
			else
				chk_total_byte += 1;
		}

		offset++;
	}

	RTW_INFO("Total PG bytes Count = %d\n", chk_total_byte);
	rtw_hal_get_hwreg(adapt, HW_VAR_EFUSE_BYTES, (u8 *)&startAddr);

	if (startAddr == 0) {
		startAddr = Efuse_GetCurrentSize(adapt, EFUSE_WIFI, false);
		RTW_INFO("%s: Efuse_GetCurrentSize startAddr=%#X\n", __func__, startAddr);
	}
	RTW_DBG("%s: startAddr=%#X\n", __func__, startAddr);

	if ((startAddr + chk_total_byte) >= efuse_max_available_len) {
		RTW_INFO("%s: startAddr(0x%X) + PG data len %d >= efuse_max_available_len(0x%X)\n",
			 __func__, startAddr, chk_total_byte, efuse_max_available_len);
		ret = _FAIL;
		goto exit;
	}

	efuse_PreUpdateAction(adapt, backupRegs);

	idx = 0;
	offset = (addr >> 3);
	while (idx < cnts) {
		word_en = 0xF;
		j = (addr + idx) & 0x7;
		memcpy(newdata, &map[offset << 3], PGPKT_DATA_SIZE);
		for (i = j; i < PGPKT_DATA_SIZE && idx < cnts; i++, idx++) {
			if (efuse[addr + idx] != map[addr + idx]) {
				word_en &= ~BIT(i >> 1);
				newdata[i] = efuse[addr + idx];
			}
		}

		if (word_en != 0xF) {
			ret = Efuse_PgPacketWrite(adapt, offset, word_en, newdata, false);
			RTW_INFO("offset=%x\n", offset);
			RTW_INFO("word_en=%x\n", word_en);

			for (i = 0; i < PGPKT_DATA_SIZE; i++)
				RTW_INFO("data=%x \t", newdata[i]);
			if (ret == _FAIL)
				break;
		}

		offset++;
	}

	/*Efuse_PowerSwitch(adapt, true, false);*/

	efuse_PostUpdateAction(adapt, backupRegs);

exit:

	rtw_mfree(map, mapLen);
	rtw_mfree(efuse, mapLen);

	return ret;
}


u8 rtw_BT_efuse_map_write(struct adapter * adapt, u16 addr, u16 cnts, u8 *data)
{
#define RT_ASSERT_RET(expr)												\
	if (!(expr)) {															\
		printk("Assertion failed! %s at ......\n", #expr);							\
		printk("      ......%s,%s, line=%d\n",__FILE__, __func__, __LINE__);	\
		return _FAIL;	\
	}

	u8 offset, word_en;
	u8 *map;
	u8 newdata[PGPKT_DATA_SIZE];
	int	i = 0, j = 0, idx;
	u8 ret = _SUCCESS;
	u16 mapLen = 0;

	EFUSE_GetEfuseDefinition(adapt, EFUSE_BT, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, false);

	if ((addr + cnts) > mapLen)
		return _FAIL;

	RT_ASSERT_RET(PGPKT_DATA_SIZE == 8); /* have to be 8 byte alignment */
	RT_ASSERT_RET((mapLen & 0x7) == 0); /* have to be PGPKT_DATA_SIZE alignment for memcpy */

	map = rtw_zmalloc(mapLen);
	if (!map)
		return _FAIL;

	ret = rtw_BT_efuse_map_read(adapt, 0, mapLen, map);
	if (ret == _FAIL)
		goto exit;
	RTW_INFO("OFFSET\tVALUE(hex)\n");
	for (i = 0; i < 1024; i += 16) { /* set 512 because the iwpriv's extra size have limit 0x7FF */
		RTW_INFO("0x%03x\t", i);
		for (j = 0; j < 8; j++)
			RTW_INFO("%02X ", map[i + j]);
		RTW_INFO("\t");
		for (; j < 16; j++)
			RTW_INFO("%02X ", map[i + j]);
		RTW_INFO("\n");
	}
	RTW_INFO("\n");
	Efuse_PowerSwitch(adapt, true, true);

	idx = 0;
	offset = (addr >> 3);
	while (idx < cnts) {
		word_en = 0xF;
		j = (addr + idx) & 0x7;
		memcpy(newdata, &map[offset << 3], PGPKT_DATA_SIZE);
		for (i = j; i < PGPKT_DATA_SIZE && idx < cnts; i++, idx++) {
			if (data[idx] != map[addr + idx]) {
				word_en &= ~BIT(i >> 1);
				newdata[i] = data[idx];
			}
		}

		if (word_en != 0xF) {
			RTW_INFO("offset=%x\n", offset);
			RTW_INFO("word_en=%x\n", word_en);
			RTW_INFO("%s: data=", __func__);
			for (i = 0; i < PGPKT_DATA_SIZE; i++)
				RTW_INFO("0x%02X ", newdata[i]);
			RTW_INFO("\n");
			ret = Efuse_PgPacketWrite_BT(adapt, offset, word_en, newdata, false);
			if (ret == _FAIL)
				break;
		}

		offset++;
	}

	Efuse_PowerSwitch(adapt, true, false);

exit:

	rtw_mfree(map, mapLen);

	return ret;
}

/*-----------------------------------------------------------------------------
 * Function:	Efuse_ReadAllMap
 *
 * Overview:	Read All Efuse content
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/11/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
void
Efuse_ReadAllMap(
		struct adapter *	pAdapter,
		u8 efuseType,
	u8 *Efuse,
		bool bPseudoTest);
void
Efuse_ReadAllMap(
		struct adapter *	pAdapter,
		u8 efuseType,
	u8 *Efuse,
		bool bPseudoTest)
{
	u16 mapLen = 0;

	Efuse_PowerSwitch(pAdapter, false, true);

	EFUSE_GetEfuseDefinition(pAdapter, efuseType, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, bPseudoTest);

	efuse_ReadEFuse(pAdapter, efuseType, 0, mapLen, Efuse, bPseudoTest);

	Efuse_PowerSwitch(pAdapter, false, false);
}

/*-----------------------------------------------------------------------------
 * Function:	efuse_ShadowWrite1Byte
 *			efuse_ShadowWrite2Byte
 *			efuse_ShadowWrite4Byte
 *
 * Overview:	Write efuse modify map by one/two/four byte.
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/12/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
#ifdef PLATFORM
static void
efuse_ShadowWrite1Byte(
	struct adapter *	pAdapter,
	u16 Offset,
	u8 Value);
#endif /* PLATFORM */
static void
efuse_ShadowWrite1Byte(
	struct adapter *	pAdapter,
	u16 Offset,
	u8 Value)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(pAdapter);

	pHalData->efuse_eeprom_data[Offset] = Value;

}	/* efuse_ShadowWrite1Byte */

/* ---------------Write Two Bytes */
static void
efuse_ShadowWrite2Byte(
	struct adapter *	pAdapter,
	u16 Offset,
	u16 Value)
{

	struct hal_com_data * pHalData = GET_HAL_DATA(pAdapter);


	pHalData->efuse_eeprom_data[Offset] = Value & 0x00FF;
	pHalData->efuse_eeprom_data[Offset + 1] = Value >> 8;

}	/* efuse_ShadowWrite1Byte */

/* ---------------Write Four Bytes */
static void
efuse_ShadowWrite4Byte(
	struct adapter *	pAdapter,
	u16 Offset,
	u32 Value)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(pAdapter);

	pHalData->efuse_eeprom_data[Offset] = (u8)(Value & 0x000000FF);
	pHalData->efuse_eeprom_data[Offset + 1] = (u8)((Value >> 8) & 0x0000FF);
	pHalData->efuse_eeprom_data[Offset + 2] = (u8)((Value >> 16) & 0x00FF);
	pHalData->efuse_eeprom_data[Offset + 3] = (u8)((Value >> 24) & 0xFF);

}	/* efuse_ShadowWrite1Byte */


/*-----------------------------------------------------------------------------
 * Function:	EFUSE_ShadowWrite
 *
 * Overview:	Write efuse modify map for later update operation to use!!!!!
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/12/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
void
EFUSE_ShadowWrite(
	struct adapter *	pAdapter,
	u8 Type,
	u16 Offset,
	u32 Value);
void
EFUSE_ShadowWrite(
	struct adapter *	pAdapter,
	u8 Type,
	u16 Offset,
	u32 Value)
{
#if (MP_DRIVER == 0)
	return;
#endif
	if (pAdapter->registrypriv.mp_mode == 0)
		return;


	if (Type == 1)
		efuse_ShadowWrite1Byte(pAdapter, Offset, (u8)Value);
	else if (Type == 2)
		efuse_ShadowWrite2Byte(pAdapter, Offset, (u16)Value);
	else if (Type == 4)
		efuse_ShadowWrite4Byte(pAdapter, Offset, (u32)Value);

}	/* EFUSE_ShadowWrite */

void
Efuse_InitSomeVar(
		struct adapter *	pAdapter
);
void
Efuse_InitSomeVar(
		struct adapter *	pAdapter
)
{
	u8 i;

	memset((void *)&fakeEfuseContent[0], 0xff, EFUSE_MAX_HW_SIZE);
	memset((void *)&fakeEfuseInitMap[0], 0xff, EFUSE_MAX_MAP_LEN);
	memset((void *)&fakeEfuseModifiedMap[0], 0xff, EFUSE_MAX_MAP_LEN);

	for (i = 0; i < EFUSE_MAX_BT_BANK; i++)
		memset((void *)&BTEfuseContent[i][0], EFUSE_MAX_HW_SIZE, 0xff);
	memset((void *)&BTEfuseInitMap[0], 0xff, EFUSE_BT_MAX_MAP_LEN);
	memset((void *)&BTEfuseModifiedMap[0], 0xff, EFUSE_BT_MAX_MAP_LEN);

	for (i = 0; i < EFUSE_MAX_BT_BANK; i++)
		memset((void *)&fakeBTEfuseContent[i][0], 0xff, EFUSE_MAX_HW_SIZE);
	memset((void *)&fakeBTEfuseInitMap[0], 0xff, EFUSE_BT_MAX_MAP_LEN);
	memset((void *)&fakeBTEfuseModifiedMap[0], 0xff, EFUSE_BT_MAX_MAP_LEN);
}

/*-----------------------------------------------------------------------------
 * Function:	efuse_ShadowRead1Byte
 *			efuse_ShadowRead2Byte
 *			efuse_ShadowRead4Byte
 *
 * Overview:	Read from efuse init map by one/two/four bytes !!!!!
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/12/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
static void
efuse_ShadowRead1Byte(
	struct adapter *	pAdapter,
	u16 Offset,
	u8 *Value)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(pAdapter);

	*Value = pHalData->efuse_eeprom_data[Offset];

}	/* EFUSE_ShadowRead1Byte */

/* ---------------Read Two Bytes */
static void
efuse_ShadowRead2Byte(
	struct adapter *	pAdapter,
	u16 Offset,
	u16 *Value)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(pAdapter);

	*Value = pHalData->efuse_eeprom_data[Offset];
	*Value |= pHalData->efuse_eeprom_data[Offset + 1] << 8;

}	/* EFUSE_ShadowRead2Byte */

/* ---------------Read Four Bytes */
static void
efuse_ShadowRead4Byte(
	struct adapter *	pAdapter,
	u16 Offset,
	u32 *Value)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(pAdapter);

	*Value = pHalData->efuse_eeprom_data[Offset];
	*Value |= pHalData->efuse_eeprom_data[Offset + 1] << 8;
	*Value |= pHalData->efuse_eeprom_data[Offset + 2] << 16;
	*Value |= pHalData->efuse_eeprom_data[Offset + 3] << 24;

}	/* efuse_ShadowRead4Byte */

/*-----------------------------------------------------------------------------
 * Function:	EFUSE_ShadowRead
 *
 * Overview:	Read from pHalData->efuse_eeprom_data
 *---------------------------------------------------------------------------*/
void
EFUSE_ShadowRead(
		struct adapter *	pAdapter,
		u8 Type,
		u16 Offset,
	u32 *Value)
{
	if (Type == 1)
		efuse_ShadowRead1Byte(pAdapter, Offset, (u8 *)Value);
	else if (Type == 2)
		efuse_ShadowRead2Byte(pAdapter, Offset, (u16 *)Value);
	else if (Type == 4)
		efuse_ShadowRead4Byte(pAdapter, Offset, (u32 *)Value);

}	/* EFUSE_ShadowRead */

/*  11/16/2008 MH Add description. Get current efuse area enabled word!!. */
u8
Efuse_CalculateWordCnts(u8 word_en)
{
	u8 word_cnts = 0;
	if (!(word_en & BIT(0)))
		word_cnts++; /* 0 : write enable */
	if (!(word_en & BIT(1)))
		word_cnts++;
	if (!(word_en & BIT(2)))
		word_cnts++;
	if (!(word_en & BIT(3)))
		word_cnts++;
	return word_cnts;
}

/*-----------------------------------------------------------------------------
 * Function:	efuse_WordEnableDataRead
 *
 * Overview:	Read allowed word in current efuse section data.
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/16/2008	MHC		Create Version 0.
 * 11/21/2008	MHC		Fix Write bug when we only enable late word.
 *
 *---------------------------------------------------------------------------*/
void
efuse_WordEnableDataRead(u8 word_en,
			 u8 *sourdata,
			 u8 *targetdata)
{
	if (!(word_en & BIT(0))) {
		targetdata[0] = sourdata[0];
		targetdata[1] = sourdata[1];
	}
	if (!(word_en & BIT(1))) {
		targetdata[2] = sourdata[2];
		targetdata[3] = sourdata[3];
	}
	if (!(word_en & BIT(2))) {
		targetdata[4] = sourdata[4];
		targetdata[5] = sourdata[5];
	}
	if (!(word_en & BIT(3))) {
		targetdata[6] = sourdata[6];
		targetdata[7] = sourdata[7];
	}
}

/*-----------------------------------------------------------------------------
 * Function:	EFUSE_ShadowMapUpdate
 *
 * Overview:	Transfer current EFUSE content to shadow init and modify map.
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/13/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
void EFUSE_ShadowMapUpdate(
	struct adapter *	pAdapter,
	u8 efuseType,
	bool bPseudoTest)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(pAdapter);
	u16 mapLen = 0;
	EFUSE_GetEfuseDefinition(pAdapter, efuseType, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, bPseudoTest);

	if (pHalData->bautoload_fail_flag) {
		memset(pHalData->efuse_eeprom_data, 0xFF, mapLen);
	} else {
			Efuse_ReadAllMap(pAdapter, efuseType, pHalData->efuse_eeprom_data, bPseudoTest);

	}

	rtw_mask_map_read(pAdapter, 0x00, mapLen, pHalData->efuse_eeprom_data);

	rtw_dump_cur_efuse(pAdapter);
} /* EFUSE_ShadowMapUpdate */

const u8 _mac_hidden_max_bw_to_hal_bw_cap[MAC_HIDDEN_MAX_BW_NUM] = {
	0,
	0,
	(BW_CAP_160M | BW_CAP_80M | BW_CAP_40M | BW_CAP_20M | BW_CAP_10M | BW_CAP_5M),
	(BW_CAP_5M),
	(BW_CAP_10M | BW_CAP_5M),
	(BW_CAP_20M | BW_CAP_10M | BW_CAP_5M),
	(BW_CAP_40M | BW_CAP_20M | BW_CAP_10M | BW_CAP_5M),
	(BW_CAP_80M | BW_CAP_40M | BW_CAP_20M | BW_CAP_10M | BW_CAP_5M),
};

const u8 _mac_hidden_proto_to_hal_proto_cap[MAC_HIDDEN_PROTOCOL_NUM] = {
	0,
	0,
	(PROTO_CAP_11N | PROTO_CAP_11G | PROTO_CAP_11B),
	(PROTO_CAP_11AC | PROTO_CAP_11N | PROTO_CAP_11G | PROTO_CAP_11B),
};

u8 mac_hidden_wl_func_to_hal_wl_func(u8 func)
{
	u8 wl_func = 0;

	if (func & BIT0)
		wl_func |= WL_FUNC_MIRACAST;
	if (func & BIT1)
		wl_func |= WL_FUNC_P2P;
	if (func & BIT2)
		wl_func |= WL_FUNC_TDLS;
	if (func & BIT3)
		wl_func |= WL_FUNC_FTM;

	return wl_func;
}

u8 rtw_efuse_file_read(struct adapter * adapt, u8 *filepatch, u8 *buf, u32 len)
{
	char *ptmpbuf = NULL, *ptr;
	u8 val8;
	u32 count, i, j;
	int err;
	u32 bufsize = 4096;

	ptmpbuf = rtw_zmalloc(bufsize);
	if (!ptmpbuf)
		return false;

	count = rtw_retrieve_from_file(filepatch, ptmpbuf, bufsize);
	if (count <= 90) {
		rtw_mfree(ptmpbuf, bufsize);
		RTW_ERR("%s, filepatch %s, size=%d, FAIL!!\n", __func__, filepatch, count);
		return false;
	}

	i = 0;
	j = 0;
	ptr = ptmpbuf;
	while ((j < len) && (i < count)) {
		if (ptmpbuf[i] == '\0')
			break;
	
		ptr = strpbrk(&ptmpbuf[i], " \t\n\r");
		if (ptr) {
			if (ptr == &ptmpbuf[i]) {
				i++;
				continue;
			}

			/* Add string terminating null */
			*ptr = 0;
		} else {
			ptr = &ptmpbuf[count-1];
		}

		err = sscanf(&ptmpbuf[i], "%hhx", &val8);
		if (err != 1) {
			RTW_WARN("Something wrong to parse efuse file, string=%s\n", &ptmpbuf[i]);
		} else {
			buf[j] = val8;
			RTW_DBG("i=%d, j=%d, 0x%02x\n", i, j, buf[j]);
			j++;
		}

		i = ptr - ptmpbuf + 1;
	}

	rtw_mfree(ptmpbuf, bufsize);
	RTW_INFO("%s, filepatch %s, size=%d, done\n", __func__, filepatch, count);
	return true;
}

#ifdef CONFIG_EFUSE_CONFIG_FILE
u32 rtw_read_efuse_from_file(const char *path, u8 *buf, int map_size)
{
	u32 i;
	u8 c;
	u8 temp[3];
	u8 temp_i;
	u8 end = false;
	u32 ret = _FAIL;

	u8 *file_data = NULL;
	u32 file_size, read_size, pos = 0;
	u8 *map = NULL;

	if (!rtw_is_file_readable_with_size(path, &file_size)) {
		RTW_PRINT("%s %s is not readable\n", __func__, path);
		goto exit;
	}

	file_data = vmalloc(file_size);
	if (!file_data) {
		RTW_ERR("%s vmalloc(%d) fail\n", __func__, file_size);
		goto exit;
	}

	read_size = rtw_retrieve_from_file(path, file_data, file_size);
	if (read_size == 0) {
		RTW_ERR("%s read from %s fail\n", __func__, path);
		goto exit;
	}

	map = vmalloc(map_size);
	if (!map) {
		RTW_ERR("%s vmalloc(%d) fail\n", __func__, map_size);
		goto exit;
	}
	memset(map, 0xff, map_size);

	temp[2] = 0; /* end of string '\0' */

	for (i = 0 ; i < map_size ; i++) {
		temp_i = 0;

		while (1) {
			if (pos >= read_size) {
				end = true;
				break;
			}
			c = file_data[pos++];

			/* bypass spece or eol or null before first hex digit */
			if (temp_i == 0 && (is_eol(c) || is_space(c) || is_null(c)))
				continue;

			if (!IsHexDigit(c)) {
				RTW_ERR("%s invalid 8-bit hex format for offset:0x%03x\n", __func__, i);
				goto exit;
			}

			temp[temp_i++] = c;

			if (temp_i == 2) {
				/* parse value */
				if (sscanf(temp, "%hhx", &map[i]) != 1) {
					RTW_ERR("%s sscanf fail for offset:0x%03x\n", __func__, i);
					goto exit;
				}
				break;
			}
		}

		if (end) {
			if (temp_i != 0) {
				RTW_ERR("%s incomplete 8-bit hex format for offset:0x%03x\n", __func__, i);
				goto exit;
			}
			break;
		}
	}

	RTW_PRINT("efuse file:%s, 0x%03x byte content read\n", path, i);

	memcpy(buf, map, map_size);

	ret = _SUCCESS;

exit:
	if (file_data)
		vfree(file_data);
	if (map)
		vfree(map);

	return ret;
}

u32 rtw_read_macaddr_from_file(const char *path, u8 *buf)
{
	u32 i;
	u8 temp[3];
	u32 ret = _FAIL;
	u8 file_data[17];
	u32 read_size;
	u8 addr[ETH_ALEN];

	if (!rtw_is_file_readable(path)) {
		RTW_PRINT("%s %s is not readable\n", __func__, path);
		goto exit;
	}

	read_size = rtw_retrieve_from_file(path, file_data, 17);
	if (read_size != 17) {
		RTW_ERR("%s read from %s fail\n", __func__, path);
		goto exit;
	}

	temp[2] = 0; /* end of string '\0' */

	for (i = 0 ; i < ETH_ALEN ; i++) {
		if (!IsHexDigit(file_data[i * 3]) || !IsHexDigit(file_data[i * 3 + 1])) {
			RTW_ERR("%s invalid 8-bit hex format for address offset:%u\n", __func__, i);
			goto exit;
		}

		if (i < ETH_ALEN - 1 && file_data[i * 3 + 2] != ':') {
			RTW_ERR("%s invalid separator after address offset:%u\n", __func__, i);
			goto exit;
		}

		temp[0] = file_data[i * 3];
		temp[1] = file_data[i * 3 + 1];
		if (sscanf(temp, "%hhx", &addr[i]) != 1) {
			RTW_ERR("%s sscanf fail for address offset:0x%03x\n", __func__, i);
			goto exit;
		}
	}

	memcpy(buf, addr, ETH_ALEN);

	RTW_PRINT("wifi_mac file: %s\n", path);
#ifdef CONFIG_RTW_DEBUG
	RTW_INFO(MAC_FMT"\n", MAC_ARG(buf));
#endif

	ret = _SUCCESS;

exit:
	return ret;
}
#endif /* CONFIG_EFUSE_CONFIG_FILE */
