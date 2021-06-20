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

/*Image2HeaderVersion: 2.19*/
#include "mp_precomp.h"
#include "../phydm_precomp.h"

#if (RTL8814A_SUPPORT == 1)
static BOOLEAN
CheckPositive(
	struct dm_struct    *pDM_Odm,
	u32  Condition1,
	u32  Condition2,
	u32  Condition3,
	u32  Condition4
)
{
	u1Byte    _BoardType = ((pDM_Odm->board_type & BIT4) >> 4) << 0 | /* _GLNA*/
				((pDM_Odm->board_type & BIT3) >> 3) << 1 | /* _GPA*/ 
				((pDM_Odm->board_type & BIT7) >> 7) << 2 | /* _ALNA*/
				((pDM_Odm->board_type & BIT6) >> 6) << 3 | /* _APA */
				((pDM_Odm->board_type & BIT2) >> 2) << 4;  /* _BT*/  

	u4Byte	cond1   = Condition1, cond2 = Condition2, cond3 = Condition3, cond4 = Condition4;
	u4Byte    driver1 = pDM_Odm->cut_version       << 24 | 
				(pDM_Odm->support_interface & 0xF0) << 16 | 
				pDM_Odm->support_platform  << 16 | 
				pDM_Odm->package_type      << 12 | 
				(pDM_Odm->support_interface & 0x0F) << 8  |
				_BoardType;

	u4Byte    driver2 = (pDM_Odm->type_glna & 0xFF) <<  0 |  
				(pDM_Odm->type_gpa & 0xFF)  <<  8 | 
				(pDM_Odm->type_alna & 0xFF) << 16 | 
				(pDM_Odm->type_apa & 0xFF)  << 24; 

u4Byte    driver3 = 0;

	u4Byte    driver4 = (pDM_Odm->type_glna & 0xFF00) >>  8 |
				(pDM_Odm->type_gpa & 0xFF00) |
				(pDM_Odm->type_alna & 0xFF00) << 8 |
				(pDM_Odm->type_apa & 0xFF00)  << 16;

	PHYDM_DBG(pDM_Odm, ODM_COMP_INIT,  
	"===> CheckPositive (cond1, cond2, cond3, cond4) = (0x%X 0x%X 0x%X 0x%X)\n", cond1, cond2, cond3, cond4);
	PHYDM_DBG(pDM_Odm, ODM_COMP_INIT,  
	"===> CheckPositive (driver1, driver2, driver3, driver4) = (0x%X 0x%X 0x%X 0x%X)\n", driver1, driver2, driver3, driver4);

	PHYDM_DBG(pDM_Odm, ODM_COMP_INIT,  
	"	(Platform, Interface) = (0x%X, 0x%X)\n", pDM_Odm->support_platform, pDM_Odm->support_interface);
	PHYDM_DBG(pDM_Odm, ODM_COMP_INIT,  
	"	(Board, Package) = (0x%X, 0x%X)\n", pDM_Odm->board_type, pDM_Odm->package_type);


	/*============== Value Defined Check ===============*/
	/*QFN Type [15:12] and Cut Version [27:24] need to do value check*/
	
	if (((cond1 & 0x0000F000) != 0) && ((cond1 & 0x0000F000) != (driver1 & 0x0000F000)))
		return FALSE;
	if (((cond1 & 0x0F000000) != 0) && ((cond1 & 0x0F000000) != (driver1 & 0x0F000000)))
		return FALSE;

	/*=============== Bit Defined Check ================*/
	/* We don't care [31:28] */

	cond1   &= 0x00FF0FFF; 
	driver1 &= 0x00FF0FFF; 

	if ((cond1 & driver1) == cond1) {
		u4Byte bitMask = 0;

		if ((cond1 & 0x0F) == 0) /* BoardType is DONTCARE*/
			return TRUE;

		if ((cond1 & BIT0) != 0) /*GLNA*/
			bitMask |= 0x000000FF;
		if ((cond1 & BIT1) != 0) /*GPA*/
			bitMask |= 0x0000FF00;
		if ((cond1 & BIT2) != 0) /*ALNA*/
			bitMask |= 0x00FF0000;
		if ((cond1 & BIT3) != 0) /*APA*/
			bitMask |= 0xFF000000;

		if (((cond2 & bitMask) == (driver2 & bitMask)) && ((cond4 & bitMask) == (driver4 & bitMask)))  /* BoardType of each RF path is matched*/
			return TRUE;
		else
			return FALSE;
	} else
		return FALSE;
}
static BOOLEAN
CheckNegative(
	struct dm_struct    *pDM_Odm,
	u32  Condition1,
	u32  Condition2
)
{
	return TRUE;
}

/******************************************************************************
*                           MAC_REG.TXT
******************************************************************************/

u4Byte Array_MP_8814A_MAC_REG[] = { 
		0x010, 0x0000007C,
		0x014, 0x000000DB,
		0x016, 0x00000002,
		0x073, 0x00000010,
		0x420, 0x00000080,
		0x421, 0x0000000F,
		0x428, 0x0000000A,
		0x429, 0x00000010,
		0x430, 0x00000000,
		0x431, 0x00000000,
		0x432, 0x00000000,
		0x433, 0x00000001,
		0x434, 0x00000004,
		0x435, 0x00000005,
		0x436, 0x00000007,
		0x437, 0x00000008,
		0x43C, 0x00000004,
		0x43D, 0x00000005,
		0x43E, 0x00000007,
		0x43F, 0x00000008,
		0x440, 0x0000005D,
		0x441, 0x00000001,
		0x442, 0x00000000,
		0x444, 0x00000010,
		0x445, 0x000000F0,
		0x446, 0x00000001,
		0x447, 0x000000FE,
		0x448, 0x00000000,
		0x449, 0x00000000,
		0x44A, 0x00000000,
		0x44B, 0x00000040,
		0x44C, 0x00000010,
		0x44D, 0x000000F0,
		0x44E, 0x0000003F,
		0x44F, 0x00000000,
		0x450, 0x00000000,
		0x451, 0x00000000,
		0x452, 0x00000000,
		0x453, 0x00000040,
		0x45E, 0x00000004,
		0x49C, 0x00000010,
		0x49D, 0x000000F0,
		0x49E, 0x00000000,
		0x49F, 0x00000006,
		0x4A0, 0x000000E0,
		0x4A1, 0x00000003,
		0x4A2, 0x00000000,
		0x4A3, 0x00000040,
		0x4A4, 0x00000015,
		0x4A5, 0x000000F0,
		0x4A6, 0x00000000,
		0x4A7, 0x00000006,
		0x4A8, 0x000000E0,
		0x4A9, 0x00000000,
		0x4AA, 0x00000000,
		0x4AB, 0x00000000,
		0x7DA, 0x00000008,
		0x1448, 0x00000006,
		0x144A, 0x00000006,
		0x144C, 0x00000006,
		0x144E, 0x00000006,
		0x4C8, 0x000000FF,
		0x4C9, 0x00000008,
		0x4CA, 0x0000003C,
		0x4CB, 0x0000003C,
		0x4CC, 0x000000FF,
		0x4CD, 0x000000FF,
		0x4CE, 0x00000001,
		0x4CF, 0x00000008,
		0x500, 0x00000026,
		0x501, 0x000000A2,
		0x502, 0x0000002F,
		0x503, 0x00000000,
		0x504, 0x00000028,
		0x505, 0x000000A3,
		0x506, 0x0000005E,
		0x507, 0x00000000,
		0x508, 0x0000002B,
		0x509, 0x000000A4,
		0x50A, 0x0000005E,
		0x50B, 0x00000000,
		0x50C, 0x0000004F,
		0x50D, 0x000000A4,
		0x50E, 0x00000000,
		0x50F, 0x00000000,
		0x512, 0x0000001C,
		0x514, 0x0000000A,
		0x516, 0x0000000A,
		0x521, 0x0000002F,
		0x525, 0x0000004F,
		0x550, 0x00000010,
		0x551, 0x00000010,
		0x559, 0x00000002,
		0x55C, 0x00000064,
		0x55D, 0x000000FF,
		0x577, 0x00000003,
		0x5BE, 0x00000064,
		0x604, 0x00000001,
		0x605, 0x00000030,
		0x607, 0x00000001,
		0x608, 0x0000000E,
		0x609, 0x0000002A,
		0x60A, 0x00000000,
		0x60C, 0x00000018,
		0x60D, 0x00000050,
		0x6A0, 0x000000FF,
		0x6A1, 0x000000FF,
		0x6A2, 0x000000FF,
		0x6A3, 0x000000FF,
		0x6A4, 0x000000FF,
		0x6A5, 0x000000FF,
		0x6DE, 0x00000084,
		0x620, 0x000000FF,
		0x621, 0x000000FF,
		0x622, 0x000000FF,
		0x623, 0x000000FF,
		0x624, 0x000000FF,
		0x625, 0x000000FF,
		0x626, 0x000000FF,
		0x627, 0x000000FF,
		0x638, 0x00000064,
		0x63C, 0x0000000A,
		0x63D, 0x0000000A,
		0x63E, 0x0000000E,
		0x63F, 0x0000000E,
		0x640, 0x00000040,
		0x642, 0x00000040,
		0x643, 0x00000000,
		0x652, 0x000000C8,
		0x66E, 0x00000005,
		0x700, 0x00000021,
		0x701, 0x00000043,
		0x702, 0x00000065,
		0x703, 0x00000087,
		0x708, 0x00000021,
		0x709, 0x00000043,
		0x70A, 0x00000065,
		0x70B, 0x00000087,
		0x718, 0x00000040,
		0x7D5, 0x000000BC,
		0x7D8, 0x00000028,
		0x7D9, 0x00000000,
		0x7DA, 0x0000000B,

};

void
odm_read_and_config_mp_8814a_mac_reg(
	struct	dm_struct *  pDM_Odm
)
{
	u4Byte     i         = 0;
	u1Byte     cCond;
	BOOLEAN bMatched = TRUE, bSkipped = FALSE;
	u4Byte     ArrayLen    = sizeof(Array_MP_8814A_MAC_REG)/sizeof(u4Byte);
	pu4Byte    Array       = Array_MP_8814A_MAC_REG;
	
	u4Byte	v1 = 0, v2 = 0, pre_v1 = 0, pre_v2 = 0;

	PHYDM_DBG(pDM_Odm, ODM_COMP_INIT, "===> ODM_ReadAndConfig_MP_8814A_MAC_REG\n");

	while ((i + 1) < ArrayLen) {
		v1 = Array[i];
		v2 = Array[i + 1];

		if (v1 & (BIT31 | BIT30)) {/*positive & negative condition*/
			if (v1 & BIT31) {/* positive condition*/
				cCond  = (u1Byte)((v1 & (BIT29|BIT28)) >> 28);
				if (cCond == COND_ENDIF) {/*end*/
					bMatched = TRUE;
					bSkipped = FALSE;
					PHYDM_DBG(pDM_Odm, ODM_COMP_INIT, "ENDIF\n");
				} else if (cCond == COND_ELSE) { /*else*/
					bMatched = bSkipped?FALSE:TRUE;
					PHYDM_DBG(pDM_Odm, ODM_COMP_INIT, "ELSE\n");
				} else {/*if , else if*/
					pre_v1 = v1;
					pre_v2 = v2;
					PHYDM_DBG(pDM_Odm, ODM_COMP_INIT, "IF or ELSE IF\n");
				}
			} else if (v1 & BIT30) { /*negative condition*/
				if (bSkipped == FALSE) {
					if (CheckPositive(pDM_Odm, pre_v1, pre_v2, v1, v2)) {
						bMatched = TRUE;
						bSkipped = TRUE;
					} else {
						bMatched = FALSE;
						bSkipped = FALSE;
					}
				} else
					bMatched = FALSE;
			}
		} else {
			if (bMatched)
				odm_ConfigMAC_8814A(pDM_Odm, v1, (u1Byte)v2);
		}
		i = i + 2;
	}
}

u4Byte
odm_get_version_mp_8814a_mac_reg(void)
{
	   return 85;
}

#endif /* end of HWIMG_SUPPORT*/

