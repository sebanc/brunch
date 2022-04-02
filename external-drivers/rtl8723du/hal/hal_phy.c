// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _HAL_PHY_C_

#include <drv_types.h>

/* ********************************************************************************
 *	Constant.
 * ********************************************************************************
 * 2008/11/20 MH For Debug only, RF */
static struct shadow_compare_map RF_Shadow[RF6052_MAX_PATH][RF6052_MAX_REG];

/**
* Function:	PHY_CalculateBitShift
*
* OverView:	Get shifted position of the BitMask
*
* Input:
*			u32		BitMask,
*
* Output:	none
* Return:		u32		Return the shift bit bit position of the mask
*/
u32
PHY_CalculateBitShift(
	u32 BitMask
)
{
	u32 i;

	for (i = 0; i <= 31; i++) {
		if (((BitMask >> i) &  0x1) == 1)
			break;
	}

	return i;
}


/*
 * ==> RF shadow Operation API Code Section!!!
 *
 *-----------------------------------------------------------------------------
 * Function:	PHY_RFShadowRead
 *				PHY_RFShadowWrite
 *				PHY_RFShadowCompare
 *				PHY_RFShadowRecorver
 *				PHY_RFShadowCompareAll
 *				PHY_RFShadowRecorverAll
 *				PHY_RFShadowCompareFlagSet
 *				PHY_RFShadowRecorverFlagSet
 *
 * Overview:	When we set RF register, we must write shadow at first.
 *			When we are running, we must compare shadow abd locate error addr.
 *			Decide to recorver or not.
 *
 * Input:       NONE
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Revised History:
 * When			Who		Remark
 * 11/20/2008	MHC		Create Version 0.
 *
 *---------------------------------------------------------------------------*/
u32 PHY_RFShadowRead(struct adapter *Adapter, enum rf_path eRFPath, u32 Offset)
{
	return	RF_Shadow[eRFPath][Offset].Value;
}	/* PHY_RFShadowRead */

void PHY_RFShadowWrite(struct adapter *Adapter, enum rf_path eRFPath,
		       u32 Offset, u32 Data)
{
	RF_Shadow[eRFPath][Offset].Value = (Data & bRFRegOffsetMask);
	RF_Shadow[eRFPath][Offset].Driver_Write = true;
}	/* PHY_RFShadowWrite */

bool PHY_RFShadowCompare(struct adapter *Adapter, enum rf_path eRFPath,
			 u32 Offset)
{
	u32 reg;
	/* Check if we need to check the register */
	if (RF_Shadow[eRFPath][Offset].Compare) {
		reg = rtw_hal_read_rfreg(Adapter, eRFPath, Offset,
					 bRFRegOffsetMask);
		/* Compare shadow and real rf register for 20bits!! */
		if (RF_Shadow[eRFPath][Offset].Value != reg) {
			/* Locate error position. */
			RF_Shadow[eRFPath][Offset].ErrorOrNot = true;
		}
		return RF_Shadow[eRFPath][Offset].ErrorOrNot ;
	}
	return false;
}	/* PHY_RFShadowCompare */

void PHY_RFShadowRecorver(struct adapter *Adapter, enum rf_path eRFPath,
			  u32 Offset)
{
	/* Check if the address is error */
	if (RF_Shadow[eRFPath][Offset].ErrorOrNot) {
		/* Check if we need to recorver the register. */
		if (RF_Shadow[eRFPath][Offset].Recorver) {
			rtw_hal_write_rfreg(Adapter, eRFPath, Offset, bRFRegOffsetMask,
					    RF_Shadow[eRFPath][Offset].Value);
		}
	}
}	/* PHY_RFShadowRecorver */

void PHY_RFShadowCompareAll(struct adapter *Adapter)
{
	enum rf_path	eRFPath = RF_PATH_A;
	u32		Offset = 0, maxReg = GET_RF6052_REAL_MAX_REG(Adapter);

	for (eRFPath = 0; eRFPath < RF6052_MAX_PATH; eRFPath++) {
		for (Offset = 0; Offset < maxReg; Offset++)
			PHY_RFShadowCompare(Adapter, eRFPath, Offset);
	}
}	/* PHY_RFShadowCompareAll */

void PHY_RFShadowRecorverAll(struct adapter *Adapter)
{
	enum rf_path eRFPath = RF_PATH_A;
	u32 Offset = 0, maxReg = GET_RF6052_REAL_MAX_REG(Adapter);

	for (eRFPath = 0; eRFPath < RF6052_MAX_PATH; eRFPath++) {
		for (Offset = 0; Offset < maxReg; Offset++)
			PHY_RFShadowRecorver(Adapter, eRFPath, Offset);
	}
}	/* PHY_RFShadowRecorverAll */

void PHY_RFShadowCompareFlagSet(struct adapter *Adapter, enum rf_path eRFPath,
				u32 Offset, u8 Type)
{
	/* Set True or False!!! */
	RF_Shadow[eRFPath][Offset].Compare = Type;
}	/* PHY_RFShadowCompareFlagSet */

void PHY_RFShadowRecorverFlagSet(struct adapter *Adapter,
				 enum rf_path eRFPath, u32 Offset, u8 Type)
{
	/* Set True or False!!! */
	RF_Shadow[eRFPath][Offset].Recorver = Type;
}	/* PHY_RFShadowRecorverFlagSet */

void PHY_RFShadowCompareFlagSetAll(struct adapter *Adapter)
{
	enum rf_path eRFPath = RF_PATH_A;
	u32 Offset = 0, maxReg = GET_RF6052_REAL_MAX_REG(Adapter);

	for (eRFPath = 0; eRFPath < RF6052_MAX_PATH; eRFPath++) {
		for (Offset = 0; Offset < maxReg; Offset++) {
			/* 2008/11/20 MH For S3S4 test,
			 * we only check reg 26/27 now!!!!
			 */
			if (Offset != 0x26 && Offset != 0x27)
				PHY_RFShadowCompareFlagSet(Adapter, eRFPath,
							   Offset, false);
			else
				PHY_RFShadowCompareFlagSet(Adapter, eRFPath,
							   Offset, true);
		}
	}
}	/* PHY_RFShadowCompareFlagSetAll */

void PHY_RFShadowRecorverFlagSetAll(struct adapter *Adapter)
{
	enum rf_path eRFPath = RF_PATH_A;
	u32 Offset = 0, maxReg = GET_RF6052_REAL_MAX_REG(Adapter);

	for (eRFPath = 0; eRFPath < RF6052_MAX_PATH; eRFPath++) {
		for (Offset = 0; Offset < maxReg; Offset++) {
			/* 2008/11/20 MH For S3S4 test,
			 * we only check reg 26/27 now!!!!
			 */
			if (Offset != 0x26 && Offset != 0x27)
				PHY_RFShadowRecorverFlagSet(Adapter, eRFPath,
							    Offset, false);
			else
				PHY_RFShadowRecorverFlagSet(Adapter, eRFPath,
							    Offset, true);
		}
	}
}	/* PHY_RFShadowCompareFlagSetAll */

void PHY_RFShadowRefresh(struct adapter *Adapter)
{
	enum rf_path eRFPath = RF_PATH_A;
	u32 Offset = 0, maxReg = GET_RF6052_REAL_MAX_REG(Adapter);

	for (eRFPath = 0; eRFPath < RF6052_MAX_PATH; eRFPath++) {
		for (Offset = 0; Offset < maxReg; Offset++) {
			RF_Shadow[eRFPath][Offset].Value = 0;
			RF_Shadow[eRFPath][Offset].Compare = false;
			RF_Shadow[eRFPath][Offset].Recorver  = false;
			RF_Shadow[eRFPath][Offset].ErrorOrNot = false;
			RF_Shadow[eRFPath][Offset].Driver_Write = false;
		}
	}
}	/* PHY_RFShadowRead */
