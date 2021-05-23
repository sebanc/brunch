/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/******************************************************************************
*                           MUSB.TXT
******************************************************************************/


u16
EFUSE_GetArrayLen_MP_8723D_MUSB(void);

void
EFUSE_GetMaskArray_MP_8723D_MUSB(
	u8 *Array
);

bool
EFUSE_IsAddressMasked_MP_8723D_MUSB(/* TC: Test Chip, MP: MP Chip */
	u16  Offset
);
