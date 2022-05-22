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
#define _HAL_COM_C_

#include <drv_types.h>
#include "hal_com_h2c.h"

#include "hal_data.h"

//#define CONFIG_GTK_OL_DBG

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
char	rtw_phy_para_file_path[PATH_LENGTH_MAX];
#endif

void dump_chip_info(HAL_VERSION	ChipVersion)
{
	int cnt = 0;
	u8 buf[128]={0};
	
	if (IS_8188E(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8188E_");
	else if (IS_8188F(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8188F_");
	else if (IS_8812_SERIES(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8812_");
	else if (IS_8192E(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8192E_");
	else if (IS_8821_SERIES(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8821_");
	else if (IS_8723B_SERIES(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8723B_");
	else if (IS_8703B_SERIES(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8703B_");
	else if (IS_8814A_SERIES(ChipVersion))
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_8814A_");
	else
		cnt += sprintf((buf+cnt), "Chip Version Info: CHIP_UNKNOWN_");

	cnt += sprintf((buf+cnt), "%s_", IS_NORMAL_CHIP(ChipVersion)?"Normal_Chip":"Test_Chip");
	if(IS_CHIP_VENDOR_TSMC(ChipVersion))
		cnt += sprintf((buf+cnt), "%s_","TSMC");
	else if(IS_CHIP_VENDOR_UMC(ChipVersion))	
		cnt += sprintf((buf+cnt), "%s_","UMC");
	else if(IS_CHIP_VENDOR_SMIC(ChipVersion))
		cnt += sprintf((buf+cnt), "%s_","SMIC");		
	
	if (IS_A_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "A_CUT_");
	else if (IS_B_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "B_CUT_");
	else if (IS_C_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "C_CUT_");
	else if (IS_D_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "D_CUT_");
	else if (IS_E_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "E_CUT_");
	else if (IS_F_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "F_CUT_");
	else if (IS_I_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "I_CUT_");
	else if (IS_J_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "J_CUT_");
	else if (IS_K_CUT(ChipVersion))
		cnt += sprintf((buf+cnt), "K_CUT_");
	else
		cnt += sprintf((buf+cnt), "UNKNOWN_CUT(%d)_", ChipVersion.CUTVersion);

	if(IS_1T1R(ChipVersion)) cnt += sprintf((buf+cnt), "1T1R_");
	else if(IS_1T2R(ChipVersion)) cnt += sprintf((buf+cnt), "1T2R_");
	else if(IS_2T2R(ChipVersion)) cnt += sprintf((buf+cnt), "2T2R_");
	else if(IS_3T3R(ChipVersion)) cnt += sprintf((buf+cnt), "3T3R_");
	else if(IS_3T4R(ChipVersion)) cnt += sprintf((buf+cnt), "3T4R_");
	else if(IS_4T4R(ChipVersion)) cnt += sprintf((buf+cnt), "4T4R_");
	else cnt += sprintf((buf+cnt), "UNKNOWN_RFTYPE(%d)_", ChipVersion.RFType);

	cnt += sprintf((buf+cnt), "RomVer(%d)\n", ChipVersion.ROMVer);

	DBG_871X("%s", buf);
}
void rtw_hal_config_rftype(PADAPTER  padapter)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	
	if (IS_1T1R(pHalData->VersionID)) {
		pHalData->rf_type = RF_1T1R;
		pHalData->NumTotalRFPath = 1;
	}	
	else if (IS_2T2R(pHalData->VersionID)) {
		pHalData->rf_type = RF_2T2R;
		pHalData->NumTotalRFPath = 2;
	}
	else if (IS_1T2R(pHalData->VersionID)) {
		pHalData->rf_type = RF_1T2R;
		pHalData->NumTotalRFPath = 2;
	}
	else if(IS_3T3R(pHalData->VersionID)) {
		pHalData->rf_type = RF_3T3R;
		pHalData->NumTotalRFPath = 3;
	}	
	else if(IS_4T4R(pHalData->VersionID)) {
		pHalData->rf_type = RF_4T4R;
		pHalData->NumTotalRFPath = 4;
	}
	else {
		pHalData->rf_type = RF_1T1R;
		pHalData->NumTotalRFPath = 1;
	}
	
	DBG_871X("%s RF_Type is %d TotalTxPath is %d \n", __FUNCTION__, pHalData->rf_type, pHalData->NumTotalRFPath);
}

#define	EEPROM_CHANNEL_PLAN_BY_HW_MASK	0x80

/*
 * Description:
 * 	Use hardware(efuse), driver parameter(registry) and default channel plan
 * 	to decide which one should be used.
 *
 * Parameters:
 *	padapter			pointer of adapter
 *	hw_alpha2		country code from HW (efuse/eeprom/mapfile)
 *	hw_chplan		channel plan from HW (efuse/eeprom/mapfile)
 *						BIT[7] software configure mode; 0:Enable, 1:disable
 *						BIT[6:0] Channel Plan
 *	sw_alpha2		country code from HW (registry/module param)
 *	sw_chplan		channel plan from SW (registry/module param)
 *	def_chplan		channel plan used when HW/SW both invalid
 *	AutoLoadFail		efuse autoload fail or not
 *
 * Return:
 *	Final channel plan decision
 *
 */
u8 hal_com_config_channel_plan(
	IN	PADAPTER padapter,
	IN	char *hw_alpha2,
	IN	u8 hw_chplan,
	IN	char *sw_alpha2,
	IN	u8 sw_chplan,
	IN	u8 def_chplan,
	IN	BOOLEAN AutoLoadFail
	)
{
	PHAL_DATA_TYPE	pHalData;
	u8 force_hw_chplan = _FALSE;
	int chplan = -1;
	const struct country_chplan *country_ent = NULL, *ent;

	pHalData = GET_HAL_DATA(padapter);

	/* treat 0xFF as invalid value, bypass hw_chplan & force_hw_chplan parsing */
	if (hw_chplan == 0xFF)
		goto chk_hw_country_code;

	if (AutoLoadFail == _TRUE)
		goto chk_sw_config;

	#ifndef CONFIG_FORCE_SW_CHANNEL_PLAN
	if (hw_chplan & EEPROM_CHANNEL_PLAN_BY_HW_MASK)
		force_hw_chplan = _TRUE;
	#endif

	hw_chplan &= (~EEPROM_CHANNEL_PLAN_BY_HW_MASK);

chk_hw_country_code:
	if (hw_alpha2 && !IS_ALPHA2_NO_SPECIFIED(hw_alpha2)) {
		ent = rtw_get_chplan_from_country(hw_alpha2);
		if (ent) {
			/* get chplan from hw country code, by pass hw chplan setting */
			country_ent = ent;
			chplan = ent->chplan;
			goto chk_sw_config;
		} else
			DBG_871X_LEVEL(_drv_always_, "%s unsupported hw_alpha2:\"%c%c\"\n", __func__, hw_alpha2[0], hw_alpha2[1]);
	}

	if (rtw_is_channel_plan_valid(hw_chplan))
		chplan = hw_chplan;
	else if (force_hw_chplan == _TRUE) {
		DBG_871X_LEVEL(_drv_always_, "%s unsupported hw_chplan:0x%02X\n", __func__, hw_chplan);
		/* hw infomaton invalid, refer to sw information */
		force_hw_chplan = _FALSE;
	}

chk_sw_config:
	if (force_hw_chplan == _TRUE)
		goto done;

	if (sw_alpha2 && !IS_ALPHA2_NO_SPECIFIED(sw_alpha2)) {
		ent = rtw_get_chplan_from_country(sw_alpha2);
		if (ent) {
			/* get chplan from sw country code, by pass sw chplan setting */
			country_ent = ent;
			chplan = ent->chplan;
			goto done;
		} else
			DBG_871X_LEVEL(_drv_always_, "%s unsupported sw_alpha2:\"%c%c\"\n", __func__, sw_alpha2[0], sw_alpha2[1]);
	}

	if (rtw_is_channel_plan_valid(sw_chplan)) {
		/* cancel hw_alpha2 because chplan is specified by sw_chplan*/
		country_ent = NULL;
		chplan = sw_chplan;
	} else if (sw_chplan != RTW_CHPLAN_MAX)
		DBG_871X_LEVEL(_drv_always_, "%s unsupported sw_chplan:0x%02X\n", __func__, sw_chplan);

done:
	if (chplan == -1) {
		DBG_871X_LEVEL(_drv_always_, "%s use def_chplan:0x%02X\n", __func__, def_chplan);
		chplan = def_chplan;
	} else if (country_ent) {
		DBG_871X_LEVEL(_drv_always_, "%s country code:\"%c%c\" with chplan:0x%02X\n", __func__
			, country_ent->alpha2[0], country_ent->alpha2[1], country_ent->chplan);
	} else
		DBG_871X_LEVEL(_drv_always_, "%s chplan:0x%02X\n", __func__, chplan);

	padapter->mlmepriv.country_ent = country_ent;
	pHalData->bDisableSWChannelPlan = force_hw_chplan;

	return chplan;
}

BOOLEAN
HAL_IsLegalChannel(
	IN	PADAPTER	Adapter,
	IN	u32			Channel
	)
{
	BOOLEAN bLegalChannel = _TRUE;

	if (Channel > 14) {
		if(IsSupported5G(Adapter->registrypriv.wireless_mode) == _FALSE) {
			bLegalChannel = _FALSE;
			DBG_871X("Channel > 14 but wireless_mode do not support 5G\n");
		}
	} else if ((Channel <= 14) && (Channel >=1)){
		if(IsSupported24G(Adapter->registrypriv.wireless_mode) == _FALSE) {
			bLegalChannel = _FALSE;
			DBG_871X("(Channel <= 14) && (Channel >=1) but wireless_mode do not support 2.4G\n");
		}
	} else {
		bLegalChannel = _FALSE;
		DBG_871X("Channel is Invalid !!!\n");
	}

	return bLegalChannel;
}	

u8	MRateToHwRate(u8 rate)
{
	u8	ret = DESC_RATE1M;
		
	switch(rate)
	{
		case MGN_1M:		    ret = DESC_RATE1M;	break;
		case MGN_2M:		    ret = DESC_RATE2M;	break;
		case MGN_5_5M:		    ret = DESC_RATE5_5M;	break;
		case MGN_11M:		    ret = DESC_RATE11M;	break;
		case MGN_6M:		    ret = DESC_RATE6M;	break;
		case MGN_9M:		    ret = DESC_RATE9M;	break;
		case MGN_12M:		    ret = DESC_RATE12M;	break;
		case MGN_18M:		    ret = DESC_RATE18M;	break;
		case MGN_24M:		    ret = DESC_RATE24M;	break;
		case MGN_36M:		    ret = DESC_RATE36M;	break;
		case MGN_48M:		    ret = DESC_RATE48M;	break;
		case MGN_54M:		    ret = DESC_RATE54M;	break;

		case MGN_MCS0:		    ret = DESC_RATEMCS0;	break;
		case MGN_MCS1:		    ret = DESC_RATEMCS1;	break;
		case MGN_MCS2:		    ret = DESC_RATEMCS2;	break;
		case MGN_MCS3:		    ret = DESC_RATEMCS3;	break;
		case MGN_MCS4:		    ret = DESC_RATEMCS4;	break;
		case MGN_MCS5:		    ret = DESC_RATEMCS5;	break;
		case MGN_MCS6:		    ret = DESC_RATEMCS6;	break;
		case MGN_MCS7:		    ret = DESC_RATEMCS7;	break;
		case MGN_MCS8:		    ret = DESC_RATEMCS8;	break;
		case MGN_MCS9:		    ret = DESC_RATEMCS9;	break;
		case MGN_MCS10:	        ret = DESC_RATEMCS10;	break;
		case MGN_MCS11:	        ret = DESC_RATEMCS11;	break;
		case MGN_MCS12:	        ret = DESC_RATEMCS12;	break;
		case MGN_MCS13:	        ret = DESC_RATEMCS13;	break;
		case MGN_MCS14:	        ret = DESC_RATEMCS14;	break;
		case MGN_MCS15:	        ret = DESC_RATEMCS15;	break;
		case MGN_MCS16:		    ret = DESC_RATEMCS16;	break;
		case MGN_MCS17:		    ret = DESC_RATEMCS17;	break;
		case MGN_MCS18:		    ret = DESC_RATEMCS18;	break;
		case MGN_MCS19:		    ret = DESC_RATEMCS19;	break;
		case MGN_MCS20:	        ret = DESC_RATEMCS20;	break;
		case MGN_MCS21:	        ret = DESC_RATEMCS21;	break;
		case MGN_MCS22:	        ret = DESC_RATEMCS22;	break;
		case MGN_MCS23:	        ret = DESC_RATEMCS23;	break;
		case MGN_MCS24:	        ret = DESC_RATEMCS24;	break;
		case MGN_MCS25:	        ret = DESC_RATEMCS25;	break;
		case MGN_MCS26:		    ret = DESC_RATEMCS26;	break;
		case MGN_MCS27:		    ret = DESC_RATEMCS27;	break;
		case MGN_MCS28:		    ret = DESC_RATEMCS28;	break;
		case MGN_MCS29:		    ret = DESC_RATEMCS29;	break;
		case MGN_MCS30:	        ret = DESC_RATEMCS30;	break;
		case MGN_MCS31:	        ret = DESC_RATEMCS31;	break;

		case MGN_VHT1SS_MCS0:	ret = DESC_RATEVHTSS1MCS0;	break;
		case MGN_VHT1SS_MCS1:	ret = DESC_RATEVHTSS1MCS1;	break;
		case MGN_VHT1SS_MCS2:	ret = DESC_RATEVHTSS1MCS2;	break;
		case MGN_VHT1SS_MCS3:	ret = DESC_RATEVHTSS1MCS3;	break;
		case MGN_VHT1SS_MCS4:	ret = DESC_RATEVHTSS1MCS4;	break;
		case MGN_VHT1SS_MCS5:	ret = DESC_RATEVHTSS1MCS5;	break;
		case MGN_VHT1SS_MCS6:	ret = DESC_RATEVHTSS1MCS6;	break;
		case MGN_VHT1SS_MCS7:	ret = DESC_RATEVHTSS1MCS7;	break;
		case MGN_VHT1SS_MCS8:	ret = DESC_RATEVHTSS1MCS8;	break;
		case MGN_VHT1SS_MCS9:	ret = DESC_RATEVHTSS1MCS9;	break;	
		case MGN_VHT2SS_MCS0:	ret = DESC_RATEVHTSS2MCS0;	break;
		case MGN_VHT2SS_MCS1:	ret = DESC_RATEVHTSS2MCS1;	break;
		case MGN_VHT2SS_MCS2:	ret = DESC_RATEVHTSS2MCS2;	break;
		case MGN_VHT2SS_MCS3:	ret = DESC_RATEVHTSS2MCS3;	break;
		case MGN_VHT2SS_MCS4:	ret = DESC_RATEVHTSS2MCS4;	break;
		case MGN_VHT2SS_MCS5:	ret = DESC_RATEVHTSS2MCS5;	break;
		case MGN_VHT2SS_MCS6:	ret = DESC_RATEVHTSS2MCS6;	break;
		case MGN_VHT2SS_MCS7:	ret = DESC_RATEVHTSS2MCS7;	break;
		case MGN_VHT2SS_MCS8:	ret = DESC_RATEVHTSS2MCS8;	break;
		case MGN_VHT2SS_MCS9:	ret = DESC_RATEVHTSS2MCS9;	break;	
		case MGN_VHT3SS_MCS0:	ret = DESC_RATEVHTSS3MCS0;	break;
		case MGN_VHT3SS_MCS1:	ret = DESC_RATEVHTSS3MCS1;	break;
		case MGN_VHT3SS_MCS2:	ret = DESC_RATEVHTSS3MCS2;	break;
		case MGN_VHT3SS_MCS3:	ret = DESC_RATEVHTSS3MCS3;	break;
		case MGN_VHT3SS_MCS4:	ret = DESC_RATEVHTSS3MCS4;	break;
		case MGN_VHT3SS_MCS5:	ret = DESC_RATEVHTSS3MCS5;	break;
		case MGN_VHT3SS_MCS6:	ret = DESC_RATEVHTSS3MCS6;	break;
		case MGN_VHT3SS_MCS7:	ret = DESC_RATEVHTSS3MCS7;	break;
		case MGN_VHT3SS_MCS8:	ret = DESC_RATEVHTSS3MCS8;	break;
		case MGN_VHT3SS_MCS9:	ret = DESC_RATEVHTSS3MCS9;	break;
		case MGN_VHT4SS_MCS0:	ret = DESC_RATEVHTSS4MCS0;	break;
		case MGN_VHT4SS_MCS1:	ret = DESC_RATEVHTSS4MCS1;	break;
		case MGN_VHT4SS_MCS2:	ret = DESC_RATEVHTSS4MCS2;	break;
		case MGN_VHT4SS_MCS3:	ret = DESC_RATEVHTSS4MCS3;	break;
		case MGN_VHT4SS_MCS4:	ret = DESC_RATEVHTSS4MCS4;	break;
		case MGN_VHT4SS_MCS5:	ret = DESC_RATEVHTSS4MCS5;	break;
		case MGN_VHT4SS_MCS6:	ret = DESC_RATEVHTSS4MCS6;	break;
		case MGN_VHT4SS_MCS7:	ret = DESC_RATEVHTSS4MCS7;	break;
		case MGN_VHT4SS_MCS8:	ret = DESC_RATEVHTSS4MCS8;	break;
		case MGN_VHT4SS_MCS9:	ret = DESC_RATEVHTSS4MCS9;	break;
		default:		break;
	}

	return ret;
}

u8	HwRateToMRate(u8 rate)
{
	u8	ret_rate = MGN_1M;

	switch(rate)
	{
	
		case DESC_RATE1M:		    ret_rate = MGN_1M;		break;
		case DESC_RATE2M:		    ret_rate = MGN_2M;		break;
		case DESC_RATE5_5M:	        ret_rate = MGN_5_5M;	break;
		case DESC_RATE11M:		    ret_rate = MGN_11M;		break;
		case DESC_RATE6M:		    ret_rate = MGN_6M;		break;
		case DESC_RATE9M:		    ret_rate = MGN_9M;		break;
		case DESC_RATE12M:		    ret_rate = MGN_12M;		break;
		case DESC_RATE18M:		    ret_rate = MGN_18M;		break;
		case DESC_RATE24M:		    ret_rate = MGN_24M;		break;
		case DESC_RATE36M:		    ret_rate = MGN_36M;		break;
		case DESC_RATE48M:		    ret_rate = MGN_48M;		break;
		case DESC_RATE54M:		    ret_rate = MGN_54M;		break;			
		case DESC_RATEMCS0:	        ret_rate = MGN_MCS0;	break;
		case DESC_RATEMCS1:	        ret_rate = MGN_MCS1;	break;
		case DESC_RATEMCS2:	        ret_rate = MGN_MCS2;	break;
		case DESC_RATEMCS3:	        ret_rate = MGN_MCS3;	break;
		case DESC_RATEMCS4:	        ret_rate = MGN_MCS4;	break;
		case DESC_RATEMCS5:	        ret_rate = MGN_MCS5;	break;
		case DESC_RATEMCS6:	        ret_rate = MGN_MCS6;	break;
		case DESC_RATEMCS7:	        ret_rate = MGN_MCS7;	break;
		case DESC_RATEMCS8:	        ret_rate = MGN_MCS8;	break;
		case DESC_RATEMCS9:	        ret_rate = MGN_MCS9;	break;
		case DESC_RATEMCS10:	    ret_rate = MGN_MCS10;	break;
		case DESC_RATEMCS11:	    ret_rate = MGN_MCS11;	break;
		case DESC_RATEMCS12:	    ret_rate = MGN_MCS12;	break;
		case DESC_RATEMCS13:	    ret_rate = MGN_MCS13;	break;
		case DESC_RATEMCS14:	    ret_rate = MGN_MCS14;	break;
		case DESC_RATEMCS15:	    ret_rate = MGN_MCS15;	break;
		case DESC_RATEMCS16:	    ret_rate = MGN_MCS16;	break;
		case DESC_RATEMCS17:	    ret_rate = MGN_MCS17;	break;
		case DESC_RATEMCS18:	    ret_rate = MGN_MCS18;	break;
		case DESC_RATEMCS19:	    ret_rate = MGN_MCS19;	break;
		case DESC_RATEMCS20:	    ret_rate = MGN_MCS20;	break;
		case DESC_RATEMCS21:	    ret_rate = MGN_MCS21;	break;
		case DESC_RATEMCS22:	    ret_rate = MGN_MCS22;	break;
		case DESC_RATEMCS23:	    ret_rate = MGN_MCS23;	break;
		case DESC_RATEMCS24:	    ret_rate = MGN_MCS24;	break;
		case DESC_RATEMCS25:	    ret_rate = MGN_MCS25;	break;
		case DESC_RATEMCS26:	    ret_rate = MGN_MCS26;	break;
		case DESC_RATEMCS27:	    ret_rate = MGN_MCS27;	break;
		case DESC_RATEMCS28:	    ret_rate = MGN_MCS28;	break;
		case DESC_RATEMCS29:	    ret_rate = MGN_MCS29;	break;
		case DESC_RATEMCS30:	    ret_rate = MGN_MCS30;	break;
		case DESC_RATEMCS31:	    ret_rate = MGN_MCS31;	break;
		case DESC_RATEVHTSS1MCS0:	ret_rate = MGN_VHT1SS_MCS0;		break;
		case DESC_RATEVHTSS1MCS1:	ret_rate = MGN_VHT1SS_MCS1;		break;
		case DESC_RATEVHTSS1MCS2:	ret_rate = MGN_VHT1SS_MCS2;		break;
		case DESC_RATEVHTSS1MCS3:	ret_rate = MGN_VHT1SS_MCS3;		break;
		case DESC_RATEVHTSS1MCS4:	ret_rate = MGN_VHT1SS_MCS4;		break;
		case DESC_RATEVHTSS1MCS5:	ret_rate = MGN_VHT1SS_MCS5;		break;
		case DESC_RATEVHTSS1MCS6:	ret_rate = MGN_VHT1SS_MCS6;		break;
		case DESC_RATEVHTSS1MCS7:	ret_rate = MGN_VHT1SS_MCS7;		break;
		case DESC_RATEVHTSS1MCS8:	ret_rate = MGN_VHT1SS_MCS8;		break;
		case DESC_RATEVHTSS1MCS9:	ret_rate = MGN_VHT1SS_MCS9;		break;
		case DESC_RATEVHTSS2MCS0:	ret_rate = MGN_VHT2SS_MCS0;		break;
		case DESC_RATEVHTSS2MCS1:	ret_rate = MGN_VHT2SS_MCS1;		break;
		case DESC_RATEVHTSS2MCS2:	ret_rate = MGN_VHT2SS_MCS2;		break;
		case DESC_RATEVHTSS2MCS3:	ret_rate = MGN_VHT2SS_MCS3;		break;
		case DESC_RATEVHTSS2MCS4:	ret_rate = MGN_VHT2SS_MCS4;		break;
		case DESC_RATEVHTSS2MCS5:	ret_rate = MGN_VHT2SS_MCS5;		break;
		case DESC_RATEVHTSS2MCS6:	ret_rate = MGN_VHT2SS_MCS6;		break;
		case DESC_RATEVHTSS2MCS7:	ret_rate = MGN_VHT2SS_MCS7;		break;
		case DESC_RATEVHTSS2MCS8:	ret_rate = MGN_VHT2SS_MCS8;		break;
		case DESC_RATEVHTSS2MCS9:	ret_rate = MGN_VHT2SS_MCS9;		break;				
		case DESC_RATEVHTSS3MCS0:	ret_rate = MGN_VHT3SS_MCS0;		break;
		case DESC_RATEVHTSS3MCS1:	ret_rate = MGN_VHT3SS_MCS1;		break;
		case DESC_RATEVHTSS3MCS2:	ret_rate = MGN_VHT3SS_MCS2;		break;
		case DESC_RATEVHTSS3MCS3:	ret_rate = MGN_VHT3SS_MCS3;		break;
		case DESC_RATEVHTSS3MCS4:	ret_rate = MGN_VHT3SS_MCS4;		break;
		case DESC_RATEVHTSS3MCS5:	ret_rate = MGN_VHT3SS_MCS5;		break;
		case DESC_RATEVHTSS3MCS6:	ret_rate = MGN_VHT3SS_MCS6;		break;
		case DESC_RATEVHTSS3MCS7:	ret_rate = MGN_VHT3SS_MCS7;		break;
		case DESC_RATEVHTSS3MCS8:	ret_rate = MGN_VHT3SS_MCS8;		break;
		case DESC_RATEVHTSS3MCS9:	ret_rate = MGN_VHT3SS_MCS9;		break;				
		case DESC_RATEVHTSS4MCS0:	ret_rate = MGN_VHT4SS_MCS0;		break;
		case DESC_RATEVHTSS4MCS1:	ret_rate = MGN_VHT4SS_MCS1;		break;
		case DESC_RATEVHTSS4MCS2:	ret_rate = MGN_VHT4SS_MCS2;		break;
		case DESC_RATEVHTSS4MCS3:	ret_rate = MGN_VHT4SS_MCS3;		break;
		case DESC_RATEVHTSS4MCS4:	ret_rate = MGN_VHT4SS_MCS4;		break;
		case DESC_RATEVHTSS4MCS5:	ret_rate = MGN_VHT4SS_MCS5;		break;
		case DESC_RATEVHTSS4MCS6:	ret_rate = MGN_VHT4SS_MCS6;		break;
		case DESC_RATEVHTSS4MCS7:	ret_rate = MGN_VHT4SS_MCS7;		break;
		case DESC_RATEVHTSS4MCS8:	ret_rate = MGN_VHT4SS_MCS8;		break;
		case DESC_RATEVHTSS4MCS9:	ret_rate = MGN_VHT4SS_MCS9;		break;				
		
		default:							
			DBG_871X("HwRateToMRate(): Non supported Rate [%x]!!!\n",rate );
			break;
	}

	return ret_rate;
}

void	HalSetBrateCfg(
	IN PADAPTER		Adapter,
	IN u8			*mBratesOS,
	OUT u16			*pBrateCfg)
{
	u8	i, is_brate, brate;

	for(i=0;i<NDIS_802_11_LENGTH_RATES_EX;i++)
	{
		is_brate = mBratesOS[i] & IEEE80211_BASIC_RATE_MASK;
		brate = mBratesOS[i] & 0x7f;
		
		if( is_brate )
		{		
			switch(brate)
			{
				case IEEE80211_CCK_RATE_1MB:	*pBrateCfg |= RATE_1M;	break;
				case IEEE80211_CCK_RATE_2MB:	*pBrateCfg |= RATE_2M;	break;
				case IEEE80211_CCK_RATE_5MB:	*pBrateCfg |= RATE_5_5M;break;
				case IEEE80211_CCK_RATE_11MB:	*pBrateCfg |= RATE_11M;	break;
				case IEEE80211_OFDM_RATE_6MB:	*pBrateCfg |= RATE_6M;	break;
				case IEEE80211_OFDM_RATE_9MB:	*pBrateCfg |= RATE_9M;	break;
				case IEEE80211_OFDM_RATE_12MB:	*pBrateCfg |= RATE_12M;	break;
				case IEEE80211_OFDM_RATE_18MB:	*pBrateCfg |= RATE_18M;	break;
				case IEEE80211_OFDM_RATE_24MB:	*pBrateCfg |= RATE_24M;	break;
				case IEEE80211_OFDM_RATE_36MB:	*pBrateCfg |= RATE_36M;	break;
				case IEEE80211_OFDM_RATE_48MB:	*pBrateCfg |= RATE_48M;	break;
				case IEEE80211_OFDM_RATE_54MB:	*pBrateCfg |= RATE_54M;	break;
			}
		}
	}
}

static VOID
_OneOutPipeMapping(
	IN	PADAPTER	pAdapter
	)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
	pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];//VI
	pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[0];//BE
	pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[0];//BK
	
	pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
	pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
	pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
	pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD
}

static VOID
_TwoOutPipeMapping(
	IN	PADAPTER	pAdapter,
	IN	BOOLEAN	 	bWIFICfg
	)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if(bWIFICfg){ //WMM
		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  0, 	1, 	0, 	1, 	0, 	0, 	0, 	0, 		0	};
		//0:ep_0 num, 1:ep_1 num 
		
		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[1];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[1];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[0];//BK
		
		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD
		
	}
	else{//typical setting

		
		//BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  1, 	1, 	0, 	0, 	0, 	0, 	0, 	0, 		0	};			
		//0:ep_0 num, 1:ep_1 num
		
		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[1];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];//BK
		
		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD	
		
	}
	
}

static VOID _ThreeOutPipeMapping(
	IN	PADAPTER	pAdapter,
	IN	BOOLEAN	 	bWIFICfg
	)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if(bWIFICfg){//for WMM
		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  1, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};
		//0:H, 1:N, 2:L 
		
		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];//BK
		
		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD
		
	}
	else{//typical setting

		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  2, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};			
		//0:H, 1:N, 2:L 
		
		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[2];//BK
		
		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD	
	}

}
static VOID _FourOutPipeMapping(
	IN	PADAPTER	pAdapter,
	IN	BOOLEAN	 	bWIFICfg
	)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if(bWIFICfg){//for WMM
		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  1, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};
		//0:H, 1:N, 2:L ,3:E
		
		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];//BK
		
		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[3];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD
		
	}
	else{//typical setting

		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  2, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};			
		//0:H, 1:N, 2:L 
		
		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[2];//BK
		
		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[3];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD	
	}

}
BOOLEAN
Hal_MappingOutPipe(
	IN	PADAPTER	pAdapter,
	IN	u8		NumOutPipe
	)
{
	struct registry_priv *pregistrypriv = &pAdapter->registrypriv;

	BOOLEAN	 bWIFICfg = (pregistrypriv->wifi_spec) ?_TRUE:_FALSE;
	
	BOOLEAN result = _TRUE;

	switch(NumOutPipe)
	{
		case 2:
			_TwoOutPipeMapping(pAdapter, bWIFICfg);
			break;
		case 3:
		case 4:
			_ThreeOutPipeMapping(pAdapter, bWIFICfg);
			break;			
		case 1:
			_OneOutPipeMapping(pAdapter);
			break;
		default:
			result = _FALSE;
			break;
	}

	return result;
	
}

void hal_init_macaddr(_adapter *adapter)
{
	rtw_hal_set_hwreg(adapter, HW_VAR_MAC_ADDR, adapter_mac_addr(adapter));
#ifdef  CONFIG_CONCURRENT_MODE
	if (adapter->pbuddy_adapter)
		rtw_hal_set_hwreg(adapter->pbuddy_adapter, HW_VAR_MAC_ADDR, adapter_mac_addr(adapter->pbuddy_adapter));
#endif
}

void rtw_init_hal_com_default_value(PADAPTER Adapter)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);

	pHalData->AntDetection = 1;
}

/* 
* C2H event format:
* Field	 TRIGGER		CONTENT	   CMD_SEQ 	CMD_LEN		 CMD_ID
* BITS	 [127:120]	[119:16]      [15:8]		  [7:4]	 	   [3:0]
*/

void c2h_evt_clear(_adapter *adapter)
{
	rtw_write8(adapter, REG_C2HEVT_CLEAR, C2H_EVT_HOST_CLOSE);
}

s32 c2h_evt_read(_adapter *adapter, u8 *buf)
{
	s32 ret = _FAIL;
	struct c2h_evt_hdr *c2h_evt;
	int i;
	u8 trigger;

	if (buf == NULL)
		goto exit;

#if defined (CONFIG_RTL8188E)

	trigger = rtw_read8(adapter, REG_C2HEVT_CLEAR);

	if (trigger == C2H_EVT_HOST_CLOSE) {
		goto exit; /* Not ready */
	} else if (trigger != C2H_EVT_FW_CLOSE) {
		goto clear_evt; /* Not a valid value */
	}

	c2h_evt = (struct c2h_evt_hdr *)buf;

	_rtw_memset(c2h_evt, 0, 16);

	*buf = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL);
	*(buf+1) = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL + 1);	

	RT_PRINT_DATA(_module_hal_init_c_, _drv_info_, "c2h_evt_read(): ",
		&c2h_evt , sizeof(c2h_evt));

	if (0) {
		DBG_871X("%s id:%u, len:%u, seq:%u, trigger:0x%02x\n", __func__
			, c2h_evt->id, c2h_evt->plen, c2h_evt->seq, trigger);
	}

	/* Read the content */
	for (i = 0; i < c2h_evt->plen; i++)
		c2h_evt->payload[i] = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL + 2 + i);

	RT_PRINT_DATA(_module_hal_init_c_, _drv_info_, "c2h_evt_read(): Command Content:\n",
		c2h_evt->payload, c2h_evt->plen);

	ret = _SUCCESS;

clear_evt:
	/* 
	* Clear event to notify FW we have read the command.
	* If this field isn't clear, the FW won't update the next command message.
	*/
	c2h_evt_clear(adapter);
#endif
exit:
	return ret;
}

/* 
* C2H event format:
* Field    TRIGGER    CMD_LEN    CONTENT    CMD_SEQ    CMD_ID
* BITS    [127:120]   [119:112]    [111:16]	     [15:8]         [7:0]
*/
s32 c2h_evt_read_88xx(_adapter *adapter, u8 *buf)
{
	s32 ret = _FAIL;
	struct c2h_evt_hdr_88xx *c2h_evt;
	int i;
	u8 trigger;

	if (buf == NULL)
		goto exit;

#if defined(CONFIG_RTL8812A) || defined(CONFIG_RTL8821A) || defined(CONFIG_RTL8192E) || defined(CONFIG_RTL8723B) || defined(CONFIG_RTL8703B)

	trigger = rtw_read8(adapter, REG_C2HEVT_CLEAR);

	if (trigger == C2H_EVT_HOST_CLOSE) {
		goto exit; /* Not ready */
	} else if (trigger != C2H_EVT_FW_CLOSE) {
		goto clear_evt; /* Not a valid value */
	}

	c2h_evt = (struct c2h_evt_hdr_88xx *)buf;

	_rtw_memset(c2h_evt, 0, 16);

	c2h_evt->id = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL);
	c2h_evt->seq = rtw_read8(adapter, REG_C2HEVT_CMD_SEQ_88XX);
	c2h_evt->plen = rtw_read8(adapter, REG_C2HEVT_CMD_LEN_88XX);

	RT_PRINT_DATA(_module_hal_init_c_, _drv_info_, "c2h_evt_read(): ",
		&c2h_evt , sizeof(c2h_evt));

	if (0) {
		DBG_871X("%s id:%u, len:%u, seq:%u, trigger:0x%02x\n", __func__
			, c2h_evt->id, c2h_evt->plen, c2h_evt->seq, trigger);
	}

	/* Read the content */
	for (i = 0; i < c2h_evt->plen; i++)
		c2h_evt->payload[i] = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL + 2 + i);

	RT_PRINT_DATA(_module_hal_init_c_, _drv_info_, "c2h_evt_read(): Command Content:\n",
		c2h_evt->payload, c2h_evt->plen);

	ret = _SUCCESS;

clear_evt:
	/* 
	* Clear event to notify FW we have read the command.
	* If this field isn't clear, the FW won't update the next command message.
	*/
	c2h_evt_clear(adapter);
#endif
exit:
	return ret;
}

#define	GET_C2H_MAC_HIDDEN_RPT_UUID_X(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 0, 0, 8)
#define	GET_C2H_MAC_HIDDEN_RPT_UUID_Y(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 1, 0, 8)
#define	GET_C2H_MAC_HIDDEN_RPT_UUID_Z(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 2, 0, 5)
#define	GET_C2H_MAC_HIDDEN_RPT_UUID_CRC(_data)			LE_BITS_TO_2BYTE(((u8 *)(_data)) + 2, 5, 11)
#define	GET_C2H_MAC_HIDDEN_RPT_HCI_TYPE(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 4, 0, 4)
#define	GET_C2H_MAC_HIDDEN_RPT_PACKAGE_TYPE(_data)		LE_BITS_TO_1BYTE(((u8 *)(_data)) + 4, 4, 3)
#define	GET_C2H_MAC_HIDDEN_RPT_TR_SWITCH(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 4, 7, 1)
#define	GET_C2H_MAC_HIDDEN_RPT_WL_FUNC(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 5, 0, 4)
#define	GET_C2H_MAC_HIDDEN_RPT_HW_STYPE(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 5, 4, 4)
#define	GET_C2H_MAC_HIDDEN_RPT_BW(_data)				LE_BITS_TO_1BYTE(((u8 *)(_data)) + 6, 0, 3)
#define	GET_C2H_MAC_HIDDEN_RPT_FAB(_data)				LE_BITS_TO_1BYTE(((u8 *)(_data)) + 6, 3, 2)
#define	GET_C2H_MAC_HIDDEN_RPT_ANT_NUM(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 6, 5, 3)
#define	GET_C2H_MAC_HIDDEN_RPT_80211_PROTOCOL(_data)	LE_BITS_TO_1BYTE(((u8 *)(_data)) + 7, 2, 2)
#define	GET_C2H_MAC_HIDDEN_RPT_NIC_ROUTER(_data)		LE_BITS_TO_1BYTE(((u8 *)(_data)) + 7, 6, 2)

#ifndef DBG_C2H_MAC_HIDDEN_RPT_HANDLE
#define DBG_C2H_MAC_HIDDEN_RPT_HANDLE 0
#endif

int c2h_mac_hidden_rpt_hdl(_adapter *adapter, u8 *data, u8 len)
{
	HAL_DATA_TYPE	*hal_data = GET_HAL_DATA(adapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	int ret = _FAIL;

	u32 uuid;
	u8 uuid_x;
	u8 uuid_y;
	u8 uuid_z;
	u16 uuid_crc;

	u8 hci_type;
	u8 package_type;
	u8 tr_switch;
	u8 wl_func;
	u8 hw_stype;
	u8 bw;
	u8 fab;
	u8 ant_num;
	u8 protocol;
	u8 nic;

	int i;

	if (len < MAC_HIDDEN_RPT_LEN) {
		DBG_871X_LEVEL(_drv_warning_, "%s len(%u) < %d\n", __func__, len, MAC_HIDDEN_RPT_LEN);
		goto exit;
	}

	uuid_x = GET_C2H_MAC_HIDDEN_RPT_UUID_X(data);
	uuid_y = GET_C2H_MAC_HIDDEN_RPT_UUID_Y(data);
	uuid_z = GET_C2H_MAC_HIDDEN_RPT_UUID_Z(data);
	uuid_crc = GET_C2H_MAC_HIDDEN_RPT_UUID_CRC(data);

	hci_type = GET_C2H_MAC_HIDDEN_RPT_HCI_TYPE(data);
	package_type = GET_C2H_MAC_HIDDEN_RPT_PACKAGE_TYPE(data);

	tr_switch = GET_C2H_MAC_HIDDEN_RPT_TR_SWITCH(data);

	wl_func = GET_C2H_MAC_HIDDEN_RPT_WL_FUNC(data);
	hw_stype = GET_C2H_MAC_HIDDEN_RPT_HW_STYPE(data);

	bw = GET_C2H_MAC_HIDDEN_RPT_BW(data);
	fab = GET_C2H_MAC_HIDDEN_RPT_FAB(data);
	ant_num = GET_C2H_MAC_HIDDEN_RPT_ANT_NUM(data);

	protocol = GET_C2H_MAC_HIDDEN_RPT_80211_PROTOCOL(data);
	nic = GET_C2H_MAC_HIDDEN_RPT_NIC_ROUTER(data);

	if (DBG_C2H_MAC_HIDDEN_RPT_HANDLE) {
		for (i = 0; i < len; i++)
			DBG_871X("%s: 0x%02X\n", __func__, *(data + i));

		DBG_871X("uuid x:0x%02x y:0x%02x z:0x%x crc:0x%x\n", uuid_x, uuid_y, uuid_z, uuid_crc);
		DBG_871X("hci_type:0x%x\n", hci_type);
		DBG_871X("package_type:0x%x\n", package_type);
		DBG_871X("tr_switch:0x%x\n", tr_switch);
		DBG_871X("wl_func:0x%x\n", wl_func);
		DBG_871X("hw_stype:0x%x\n", hw_stype);
		DBG_871X("bw:0x%x\n", bw);
		DBG_871X("fab:0x%x\n", fab);
		DBG_871X("ant_num:0x%x\n", ant_num);
		DBG_871X("protocol:0x%x\n", protocol);
		DBG_871X("nic:0x%x\n", nic);
	}

	/*
	* NOTICE:
	* for now, the following is common info/format
	* if there is any hal difference need to export
	* some IC dependent code will need to be implement
	*/	hal_data->PackageType = package_type;
	hal_spec->wl_func &= mac_hidden_wl_func_to_hal_wl_func(wl_func);
	hal_spec->bw_cap &= mac_hidden_max_bw_to_hal_bw_cap(bw);
	hal_spec->nss_num = rtw_min(hal_spec->nss_num, ant_num);
	hal_spec->proto_cap &= mac_hidden_proto_to_hal_proto_cap(protocol);

	/* TODO: tr_switch */
	/* TODO: fab */

	ret = _SUCCESS;

exit:
	return ret;
}

int c2h_mac_hidden_rpt_2_hdl(_adapter *adapter, u8 *data, u8 len)
{
	HAL_DATA_TYPE	*hal_data = GET_HAL_DATA(adapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	int ret = _FAIL;

	int i;

	if (len < MAC_HIDDEN_RPT_2_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, MAC_HIDDEN_RPT_2_LEN);
		goto exit;
	}

	if (DBG_C2H_MAC_HIDDEN_RPT_HANDLE) {
		for (i = 0; i < len; i++)
			DBG_871X_LEVEL(_drv_always_, "%s: 0x%02X\n", __func__, *(data + i));
	}

	ret = _SUCCESS;

exit:
	return ret;
}

int hal_read_mac_hidden_rpt(_adapter *adapter)
{
	int ret = _FAIL;
	int ret_fwdl;
	u8 mac_hidden_rpt[MAC_HIDDEN_RPT_LEN + MAC_HIDDEN_RPT_2_LEN] = {0};
	u32 start = rtw_get_current_time();
	u32 cnt = 0;
	u32 timeout_ms = 800;
	u32 min_cnt = 10;
	u8 id = C2H_DEFEATURE_RSVD;
	int i;

#if defined(CONFIG_USB_HCI) || defined(CONFIG_PCI_HCI)
	u8 hci_type = rtw_get_intf_type(adapter);

	if ((hci_type == RTW_USB || hci_type == RTW_PCIE)
		&& !rtw_is_hw_init_completed(adapter))
		rtw_hal_power_on(adapter);
#endif

	/* inform FW mac hidden rpt from reg is needed */
	rtw_write8(adapter, REG_C2HEVT_MSG_NORMAL, C2H_DEFEATURE_RSVD);

	/* download FW */
	ret_fwdl = rtw_hal_fw_dl(adapter, _FALSE);
	if (ret_fwdl != _SUCCESS)
		goto mac_hidden_rpt_hdl;

	/* polling for data ready */
	start = rtw_get_current_time();
	do {
		cnt++;
		id = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL);
		if (id == C2H_MAC_HIDDEN_RPT || RTW_CANNOT_IO(adapter))
			break;
		rtw_msleep_os(10);
	} while (rtw_get_passing_time_ms(start) < timeout_ms || cnt < min_cnt);

	if (id == C2H_MAC_HIDDEN_RPT) {
		/* read data */
		for (i = 0; i < MAC_HIDDEN_RPT_LEN + MAC_HIDDEN_RPT_2_LEN; i++)
			mac_hidden_rpt[i] = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL + 2 + i);
	}
	
	/* inform FW mac hidden rpt has read */
	rtw_write8(adapter, REG_C2HEVT_MSG_NORMAL, C2H_DBG);

mac_hidden_rpt_hdl:
	c2h_mac_hidden_rpt_hdl(adapter, mac_hidden_rpt, MAC_HIDDEN_RPT_LEN);
	c2h_mac_hidden_rpt_2_hdl(adapter, mac_hidden_rpt + MAC_HIDDEN_RPT_LEN, MAC_HIDDEN_RPT_2_LEN);

	if (ret_fwdl == _SUCCESS && id == C2H_MAC_HIDDEN_RPT)
		ret = _SUCCESS;

exit:

#if defined(CONFIG_USB_HCI) || defined(CONFIG_PCI_HCI)
	if ((hci_type == RTW_USB || hci_type == RTW_PCIE)
		&& !rtw_is_hw_init_completed(adapter))
		rtw_hal_power_off(adapter);
#endif

	DBG_871X("%s %s! (%u, %dms), fwdl:%d, id:0x%02x\n", __func__
		, (ret == _SUCCESS)?"OK":"Fail", cnt, rtw_get_passing_time_ms(start), ret_fwdl, id);

	return ret;
}

int c2h_defeature_dbg_hdl(_adapter *adapter, u8 *data, u8 len)
{
	HAL_DATA_TYPE	*hal_data = GET_HAL_DATA(adapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	int ret = _FAIL;

	int i;

	if (len < DEFEATURE_DBG_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, DEFEATURE_DBG_LEN);
		goto exit;
	}

	for (i = 0; i < len; i++)
		DBG_871X_LEVEL(_drv_always_,"%s: 0x%02X\n", __func__, *(data + i));

	ret = _SUCCESS;
	
exit:
	return ret;
}


#ifndef DBG_CUSTOMER_STR_RPT_HANDLE
#define DBG_CUSTOMER_STR_RPT_HANDLE 0
#endif

#ifdef CONFIG_RTW_CUSTOMER_STR
s32 rtw_hal_h2c_customer_str_req(_adapter *adapter)
{
	u8 h2c_data[H2C_CUSTOMER_STR_REQ_LEN] = {0};

	SET_H2CCMD_CUSTOMER_STR_REQ_EN(h2c_data, 1);
	return rtw_hal_fill_h2c_cmd(adapter, H2C_CUSTOMER_STR_REQ, H2C_CUSTOMER_STR_REQ_LEN, h2c_data);
}

#define	C2H_CUSTOMER_STR_RPT_BYTE0(_data)		((u8 *)(_data))
#define	C2H_CUSTOMER_STR_RPT_2_BYTE8(_data)		((u8 *)(_data))

int c2h_customer_str_rpt_hdl(_adapter *adapter, u8 *data, u8 len)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	int ret = _FAIL;
	int i;

	if (len < CUSTOMER_STR_RPT_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, CUSTOMER_STR_RPT_LEN);
		goto exit;
	}

	if (DBG_CUSTOMER_STR_RPT_HANDLE)
		RTW_PRINT_DUMP("customer_str_rpt: ", data, CUSTOMER_STR_RPT_LEN);

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);

	if (dvobj->customer_str_sctx != NULL) {
		if (dvobj->customer_str_sctx->status != RTW_SCTX_SUBMITTED)
			RTW_WARN("%s invalid sctx.status:%d\n", __func__, dvobj->customer_str_sctx->status);
		_rtw_memcpy(dvobj->customer_str,  C2H_CUSTOMER_STR_RPT_BYTE0(data), CUSTOMER_STR_RPT_LEN);
		dvobj->customer_str_sctx->status = RTX_SCTX_CSTR_WAIT_RPT2;
	} else
		RTW_WARN("%s sctx not set\n", __func__);

	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

	ret = _SUCCESS;

exit:
	return ret;
}

int c2h_customer_str_rpt_2_hdl(_adapter *adapter, u8 *data, u8 len)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	int ret = _FAIL;
	int i;

	if (len < CUSTOMER_STR_RPT_2_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, CUSTOMER_STR_RPT_2_LEN);
		goto exit;
	}

	if (DBG_CUSTOMER_STR_RPT_HANDLE)
		RTW_PRINT_DUMP("customer_str_rpt_2: ", data, CUSTOMER_STR_RPT_2_LEN);

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);

	if (dvobj->customer_str_sctx != NULL) {
		if (dvobj->customer_str_sctx->status != RTX_SCTX_CSTR_WAIT_RPT2)
			RTW_WARN("%s rpt not ready\n", __func__);
		_rtw_memcpy(dvobj->customer_str + CUSTOMER_STR_RPT_LEN,  C2H_CUSTOMER_STR_RPT_2_BYTE8(data), CUSTOMER_STR_RPT_2_LEN);
		rtw_sctx_done(&dvobj->customer_str_sctx);
	} else
		RTW_WARN("%s sctx not set\n", __func__);

	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

	ret = _SUCCESS;

exit:
	return ret;
}

/* read customer str */
s32 rtw_hal_customer_str_read(_adapter *adapter, u8 *cs)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct submit_ctx sctx;
	s32 ret = _SUCCESS;

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
	if (dvobj->customer_str_sctx != NULL)
		ret = _FAIL;
	else {
		rtw_sctx_init(&sctx, 2 * 1000);
		dvobj->customer_str_sctx = &sctx;
	}
	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

	if (ret == _FAIL) {
		RTW_WARN("%s another handle ongoing\n", __func__);
		goto exit;
	}

	ret = rtw_customer_str_req_cmd(adapter);
	if (ret != _SUCCESS) {
		RTW_WARN("%s read cmd fail\n", __func__);
		_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
		dvobj->customer_str_sctx = NULL;
		_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);
		goto exit;
	}

	/* wait till rpt done or timeout */
	rtw_sctx_wait(&sctx, __func__);

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
	dvobj->customer_str_sctx = NULL;
	if (sctx.status == RTW_SCTX_DONE_SUCCESS)
		_rtw_memcpy(cs, dvobj->customer_str, RTW_CUSTOMER_STR_LEN);
	else
		ret = _FAIL;
	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

exit:
	return ret;
}

s32 rtw_hal_h2c_customer_str_write(_adapter *adapter, const u8 *cs)
{
	u8 h2c_data_w1[H2C_CUSTOMER_STR_W1_LEN] = {0};
	u8 h2c_data_w2[H2C_CUSTOMER_STR_W2_LEN] = {0};
	u8 h2c_data_w3[H2C_CUSTOMER_STR_W3_LEN] = {0};
	s32 ret;

	SET_H2CCMD_CUSTOMER_STR_W1_EN(h2c_data_w1, 1);
	_rtw_memcpy(H2CCMD_CUSTOMER_STR_W1_BYTE0(h2c_data_w1), cs, 6);

	SET_H2CCMD_CUSTOMER_STR_W2_EN(h2c_data_w2, 1);
	_rtw_memcpy(H2CCMD_CUSTOMER_STR_W2_BYTE6(h2c_data_w2), cs + 6, 6);

	SET_H2CCMD_CUSTOMER_STR_W3_EN(h2c_data_w3, 1);
	_rtw_memcpy(H2CCMD_CUSTOMER_STR_W3_BYTE12(h2c_data_w3), cs + 6 + 6, 4);

	ret = rtw_hal_fill_h2c_cmd(adapter, H2C_CUSTOMER_STR_W1, H2C_CUSTOMER_STR_W1_LEN, h2c_data_w1);
	if (ret != _SUCCESS) {
		RTW_WARN("%s w1 fail\n", __func__);
		goto exit;
	}

	ret = rtw_hal_fill_h2c_cmd(adapter, H2C_CUSTOMER_STR_W2, H2C_CUSTOMER_STR_W2_LEN, h2c_data_w2);
	if (ret != _SUCCESS) {
		RTW_WARN("%s w2 fail\n", __func__);
		goto exit;
	}

	ret = rtw_hal_fill_h2c_cmd(adapter, H2C_CUSTOMER_STR_W3, H2C_CUSTOMER_STR_W3_LEN, h2c_data_w3);
	if (ret != _SUCCESS) {
		RTW_WARN("%s w3 fail\n", __func__);
		goto exit;
	}

exit:
	return ret;
}

/* write customer str and check if value reported is the same as requested */
s32 rtw_hal_customer_str_write(_adapter *adapter, const u8 *cs)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct submit_ctx sctx;
	s32 ret = _SUCCESS;

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
	if (dvobj->customer_str_sctx != NULL)
		ret = _FAIL;
	else {
		rtw_sctx_init(&sctx, 2 * 1000);
		dvobj->customer_str_sctx = &sctx;
	}
	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

	if (ret == _FAIL) {
		RTW_WARN("%s another handle ongoing\n", __func__);
		goto exit;
	}

	ret = rtw_customer_str_write_cmd(adapter, cs);
	if (ret != _SUCCESS) {
		RTW_WARN("%s write cmd fail\n", __func__);
		_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
		dvobj->customer_str_sctx = NULL;
		_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);
		goto exit;
	}

	ret = rtw_customer_str_req_cmd(adapter);
	if (ret != _SUCCESS) {
		RTW_WARN("%s read cmd fail\n", __func__);
		_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
		dvobj->customer_str_sctx = NULL;
		_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);
		goto exit;
	}

	/* wait till rpt done or timeout */
	rtw_sctx_wait(&sctx, __func__);

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
	dvobj->customer_str_sctx = NULL;
	if (sctx.status == RTW_SCTX_DONE_SUCCESS) {
		if (_rtw_memcmp(cs, dvobj->customer_str, RTW_CUSTOMER_STR_LEN) != _TRUE) {
			RTW_WARN("%s read back check fail\n", __func__);
			RTW_INFO_DUMP("write req: ", cs, RTW_CUSTOMER_STR_LEN);
			RTW_INFO_DUMP("read back: ", dvobj->customer_str, RTW_CUSTOMER_STR_LEN);
			ret = _FAIL;
		}
	} else
		ret = _FAIL;
	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

exit:
	return ret;
}
#endif /* CONFIG_RTW_CUSTOMER_STR */


u8  rtw_hal_networktype_to_raid(_adapter *adapter, struct sta_info *psta)
{
	if(IS_NEW_GENERATION_IC(adapter)){
		return networktype_to_raid_ex(adapter,psta);
	}
	else{
		return networktype_to_raid(adapter,psta);
	}

}
u8 rtw_get_mgntframe_raid(_adapter *adapter,unsigned char network_type)
{	

	u8 raid;
	if(IS_NEW_GENERATION_IC(adapter)){
		
		raid = (network_type & WIRELESS_11B)	?RATEID_IDX_B
											:RATEID_IDX_G;		
	}
	else{
		raid = (network_type & WIRELESS_11B)	?RATR_INX_WIRELESS_B
											:RATR_INX_WIRELESS_G;		
	}	
	return raid;
}

void rtw_hal_update_sta_rate_mask(PADAPTER padapter, struct sta_info *psta)
{
	u8	i, rf_type, limit;
	u64	tx_ra_bitmap;

	if(psta == NULL)
	{
		return;
	}

	tx_ra_bitmap = 0;

	//b/g mode ra_bitmap  
	for (i=0; i<sizeof(psta->bssrateset); i++)
	{
		if (psta->bssrateset[i])
			tx_ra_bitmap |= rtw_get_bit_value_from_ieee_value(psta->bssrateset[i]&0x7f);
	}

#ifdef CONFIG_80211N_HT
#ifdef CONFIG_80211AC_VHT
	//AC mode ra_bitmap
	if(psta->vhtpriv.vht_option) 
	{
		tx_ra_bitmap |= (rtw_vht_rate_to_bitmap(psta->vhtpriv.vht_mcs_map) << 12);
	}
	else
#endif //CONFIG_80211AC_VHT
	{
		//n mode ra_bitmap
		if(psta->htpriv.ht_option)
		{
			rf_type = RF_1T1R;
			rtw_hal_get_hwreg(padapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
			if(rf_type == RF_2T2R)
				limit=16;// 2R
			else if(rf_type == RF_3T3R)
				limit=24;// 3R
			else
				limit=8;//  1R


			/* Handling SMPS mode for AP MODE only*/
			if (check_fwstate(&padapter->mlmepriv, WIFI_AP_STATE) == _TRUE) {
				/*0:static SMPS, 1:dynamic SMPS, 3:SMPS disabled, 2:reserved*/
				if (psta->htpriv.smps_cap == 0 || psta->htpriv.smps_cap == 1) {
					/*operate with only one active receive chain // 11n-MCS rate <= MSC7*/
					limit = 8;/*  1R*/
				}
			}

			for (i=0; i<limit; i++) {
				if (psta->htpriv.ht_cap.supp_mcs_set[i/8] & BIT(i%8))
					tx_ra_bitmap |= BIT(i+12);
			}
		}
	}
#endif //CONFIG_80211N_HT
	DBG_871X("supp_mcs_set = %02x, %02x, %02x, rf_type=%d, tx_ra_bitmap=%016llx\n"
	, psta->htpriv.ht_cap.supp_mcs_set[0], psta->htpriv.ht_cap.supp_mcs_set[1], psta->htpriv.ht_cap.supp_mcs_set[2], rf_type, tx_ra_bitmap);
	psta->ra_mask = tx_ra_bitmap;
	psta->init_rate = get_highest_rate_idx(tx_ra_bitmap)&0x3f;
}

#ifndef SEC_CAM_ACCESS_TIMEOUT_MS
	#define SEC_CAM_ACCESS_TIMEOUT_MS 200
#endif

#ifndef DBG_SEC_CAM_ACCESS
	#define DBG_SEC_CAM_ACCESS 0
#endif

u32 rtw_sec_read_cam(_adapter *adapter, u8 addr)
{
	_mutex *mutex = &adapter_to_dvobj(adapter)->cam_ctl.sec_cam_access_mutex;
	u32 rdata;
	u32 cnt = 0;
	u32 start = 0, end = 0;
	u8 timeout = 0;
	u8 sr = 0;

	_enter_critical_mutex(mutex, NULL);

	rtw_write32(adapter, REG_CAMCMD, CAM_POLLINIG | addr);

	start = rtw_get_current_time();
	while (1) {
		if (rtw_is_surprise_removed(adapter)) {
			sr = 1;
			break;
		}

		cnt++;
		if (0 == (rtw_read32(adapter, REG_CAMCMD) & CAM_POLLINIG))
			break;

		if (rtw_get_passing_time_ms(start) > SEC_CAM_ACCESS_TIMEOUT_MS) {
			timeout = 1;
			break;
		}
	}
	end = rtw_get_current_time();

	rdata = rtw_read32(adapter, REG_CAMREAD);

	_exit_critical_mutex(mutex, NULL);

	if (DBG_SEC_CAM_ACCESS || timeout) {
		DBG_871X(FUNC_ADPT_FMT" addr:0x%02x, rdata:0x%08x, to:%u, polling:%u, %d ms\n"
			, FUNC_ADPT_ARG(adapter), addr, rdata, timeout, cnt, rtw_get_time_interval_ms(start, end));
	}

	return rdata;
}

void rtw_sec_write_cam(_adapter *adapter, u8 addr, u32 wdata)
{
	_mutex *mutex = &adapter_to_dvobj(adapter)->cam_ctl.sec_cam_access_mutex;
	u32 cnt = 0;
	u32 start = 0, end = 0;
	u8 timeout = 0;
	u8 sr = 0;

	_enter_critical_mutex(mutex, NULL);

	rtw_write32(adapter, REG_CAMWRITE, wdata);
	rtw_write32(adapter, REG_CAMCMD, CAM_POLLINIG | CAM_WRITE | addr);

	start = rtw_get_current_time();
	while (1) {
		if (rtw_is_surprise_removed(adapter)) {
			sr = 1;
			break;
		}

		cnt++;
		if (0 == (rtw_read32(adapter, REG_CAMCMD) & CAM_POLLINIG))
			break;

		if (rtw_get_passing_time_ms(start) > SEC_CAM_ACCESS_TIMEOUT_MS) {
			timeout = 1;
			break;
		}
	}
	end = rtw_get_current_time();

	_exit_critical_mutex(mutex, NULL);

	if (DBG_SEC_CAM_ACCESS || timeout) {
		DBG_871X(FUNC_ADPT_FMT" addr:0x%02x, wdata:0x%08x, to:%u, polling:%u, %d ms\n"
			, FUNC_ADPT_ARG(adapter), addr, wdata, timeout, cnt, rtw_get_time_interval_ms(start, end));
	}
}

void rtw_sec_read_cam_ent(_adapter *adapter, u8 id, u8 *ctrl, u8 *mac, u8 *key)
{
	unsigned int val, addr;
	u8 i;
	u32 rdata;
	u8 begin = 0;
	u8 end = 5; /* TODO: consider other key length accordingly */

	if (!ctrl && !mac && !key) {
		rtw_warn_on(1);
		goto exit;
	}

	/* TODO: check id range */

	if (!ctrl && !mac)
		begin = 2; /* read from key */

	if (!key && !mac)
		end = 0; /* read to ctrl */
	else if (!key)
		end = 2; /* read to mac */

	for (i = begin; i <= end; i++) {
		rdata = rtw_sec_read_cam(adapter, (id << 3) | i);

		switch (i) {
		case 0:
			if (ctrl)
				_rtw_memcpy(ctrl, (u8 *)(&rdata), 2);
			if (mac)
				_rtw_memcpy(mac, ((u8 *)(&rdata)) + 2, 2);
			break;
		case 1:
			if (mac)
				_rtw_memcpy(mac + 2, (u8 *)(&rdata), 4);
			break;
		default:
			if (key)
				_rtw_memcpy(key + (i - 2) * 4, (u8 *)(&rdata), 4);
			break;
		}
	}

exit:
	return;
}


void rtw_sec_write_cam_ent(_adapter *adapter, u8 id, u16 ctrl, u8 *mac, u8 *key)
{
	unsigned int i;
	int j;
	u8 addr;
	u32 wdata;

	/* TODO: consider other key length accordingly */
#if 0
	switch ((ctrl & 0x1c) >> 2) {
	case _WEP40_:
	case _TKIP_
	case _AES_
	case _WEP104_

	}
#else
	j = 7;
#endif

	for (; j >= 0; j--) {
		switch (j) {
		case 0:
			wdata = (ctrl | (mac[0] << 16) | (mac[1] << 24));
			break;
		case 1:
			wdata = (mac[2] | (mac[3] << 8) | (mac[4] << 16) | (mac[5] << 24));
			break;
		case 6:
		case 7:
			wdata = 0;
			break;
		default:
			i = (j - 2) << 2;
			wdata = (key[i] | (key[i + 1] << 8) | (key[i + 2] << 16) | (key[i + 3] << 24));
			break;
		}

		addr = (id << 3) + j;

		rtw_sec_write_cam(adapter, addr, wdata);
	}
}

bool rtw_sec_read_cam_is_gk(_adapter *adapter, u8 id)
{
	bool res;
	u16 ctrl;

	rtw_sec_read_cam_ent(adapter, id, (u8 *)&ctrl, NULL, NULL);

	res = (ctrl & BIT6) ? _TRUE : _FALSE;
	return res;
}

void hw_var_port_switch(_adapter *adapter)
{
#ifdef CONFIG_CONCURRENT_MODE
#ifdef CONFIG_RUNTIME_PORT_SWITCH
/*
0x102: MSR
0x550: REG_BCN_CTRL
0x551: REG_BCN_CTRL_1
0x55A: REG_ATIMWND
0x560: REG_TSFTR
0x568: REG_TSFTR1
0x570: REG_ATIMWND_1
0x610: REG_MACID
0x618: REG_BSSID
0x700: REG_MACID1
0x708: REG_BSSID1
*/

	int i;
	u8 msr;
	u8 bcn_ctrl;
	u8 bcn_ctrl_1;
	u8 atimwnd[2];
	u8 atimwnd_1[2];
	u8 tsftr[8];
	u8 tsftr_1[8];
	u8 macid[6];
	u8 bssid[6];
	u8 macid_1[6];
	u8 bssid_1[6];

	u8 iface_type;

	msr = rtw_read8(adapter, MSR);
	bcn_ctrl = rtw_read8(adapter, REG_BCN_CTRL);
	bcn_ctrl_1 = rtw_read8(adapter, REG_BCN_CTRL_1);

	for (i=0; i<2; i++)
		atimwnd[i] = rtw_read8(adapter, REG_ATIMWND+i);
	for (i=0; i<2; i++)
		atimwnd_1[i] = rtw_read8(adapter, REG_ATIMWND_1+i);

	for (i=0; i<8; i++)
		tsftr[i] = rtw_read8(adapter, REG_TSFTR+i);
	for (i=0; i<8; i++)
		tsftr_1[i] = rtw_read8(adapter, REG_TSFTR1+i);

	for (i=0; i<6; i++)
		macid[i] = rtw_read8(adapter, REG_MACID+i);

	for (i=0; i<6; i++)
		bssid[i] = rtw_read8(adapter, REG_BSSID+i);

	for (i=0; i<6; i++)
		macid_1[i] = rtw_read8(adapter, REG_MACID1+i);

	for (i=0; i<6; i++)
		bssid_1[i] = rtw_read8(adapter, REG_BSSID1+i);

#ifdef DBG_RUNTIME_PORT_SWITCH
	DBG_871X(FUNC_ADPT_FMT" before switch\n"
		"msr:0x%02x\n"
		"bcn_ctrl:0x%02x\n"
		"bcn_ctrl_1:0x%02x\n"
		"atimwnd:0x%04x\n"
		"atimwnd_1:0x%04x\n"
		"tsftr:%llu\n"
		"tsftr1:%llu\n"
		"macid:"MAC_FMT"\n"
		"bssid:"MAC_FMT"\n"
		"macid_1:"MAC_FMT"\n"
		"bssid_1:"MAC_FMT"\n"
		, FUNC_ADPT_ARG(adapter)
		, msr
		, bcn_ctrl
		, bcn_ctrl_1
		, *((u16*)atimwnd)
		, *((u16*)atimwnd_1)
		, *((u64*)tsftr)
		, *((u64*)tsftr_1)
		, MAC_ARG(macid)
		, MAC_ARG(bssid)
		, MAC_ARG(macid_1)
		, MAC_ARG(bssid_1)
	);
#endif /* DBG_RUNTIME_PORT_SWITCH */

	/* disable bcn function, disable update TSF  */
	rtw_write8(adapter, REG_BCN_CTRL, (bcn_ctrl & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);
	rtw_write8(adapter, REG_BCN_CTRL_1, (bcn_ctrl_1 & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);

	/* switch msr */
	msr = (msr&0xf0) |((msr&0x03) << 2) | ((msr&0x0c) >> 2);
	rtw_write8(adapter, MSR, msr);

	/* write port0 */
	rtw_write8(adapter, REG_BCN_CTRL, bcn_ctrl_1 & ~EN_BCN_FUNCTION);
	for (i=0; i<2; i++)
		rtw_write8(adapter, REG_ATIMWND+i, atimwnd_1[i]);
	for (i=0; i<8; i++)
		rtw_write8(adapter, REG_TSFTR+i, tsftr_1[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_MACID+i, macid_1[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_BSSID+i, bssid_1[i]);

	/* write port1 */
	rtw_write8(adapter, REG_BCN_CTRL_1, bcn_ctrl & ~EN_BCN_FUNCTION);
	for (i=0; i<2; i++)
		rtw_write8(adapter, REG_ATIMWND_1+1, atimwnd[i]);
	for (i=0; i<8; i++)
		rtw_write8(adapter, REG_TSFTR1+i, tsftr[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_MACID1+i, macid[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_BSSID1+i, bssid[i]);

	/* write bcn ctl */
#ifdef CONFIG_BT_COEXIST
#if defined(CONFIG_RTL8723B) || defined(CONFIG_RTL8703B)
	// always enable port0 beacon function for PSTDMA
	bcn_ctrl_1 |= EN_BCN_FUNCTION;
	// always disable port1 beacon function for PSTDMA
	bcn_ctrl &= ~EN_BCN_FUNCTION;
#endif
#endif
	rtw_write8(adapter, REG_BCN_CTRL, bcn_ctrl_1);
	rtw_write8(adapter, REG_BCN_CTRL_1, bcn_ctrl);

	if (adapter->iface_type == IFACE_PORT0) {
		adapter->iface_type = IFACE_PORT1;
		adapter->pbuddy_adapter->iface_type = IFACE_PORT0;
		DBG_871X_LEVEL(_drv_always_, "port switch - port0("ADPT_FMT"), port1("ADPT_FMT")\n",
			ADPT_ARG(adapter->pbuddy_adapter), ADPT_ARG(adapter));
	} else {
		adapter->iface_type = IFACE_PORT0;
		adapter->pbuddy_adapter->iface_type = IFACE_PORT1;
		DBG_871X_LEVEL(_drv_always_, "port switch - port0("ADPT_FMT"), port1("ADPT_FMT")\n",
			ADPT_ARG(adapter), ADPT_ARG(adapter->pbuddy_adapter));
	}

#ifdef DBG_RUNTIME_PORT_SWITCH
	msr = rtw_read8(adapter, MSR);
	bcn_ctrl = rtw_read8(adapter, REG_BCN_CTRL);
	bcn_ctrl_1 = rtw_read8(adapter, REG_BCN_CTRL_1);

	for (i=0; i<2; i++)
		atimwnd[i] = rtw_read8(adapter, REG_ATIMWND+i);
	for (i=0; i<2; i++)
		atimwnd_1[i] = rtw_read8(adapter, REG_ATIMWND_1+i);

	for (i=0; i<8; i++)
		tsftr[i] = rtw_read8(adapter, REG_TSFTR+i);
	for (i=0; i<8; i++)
		tsftr_1[i] = rtw_read8(adapter, REG_TSFTR1+i);

	for (i=0; i<6; i++)
		macid[i] = rtw_read8(adapter, REG_MACID+i);

	for (i=0; i<6; i++)
		bssid[i] = rtw_read8(adapter, REG_BSSID+i);

	for (i=0; i<6; i++)
		macid_1[i] = rtw_read8(adapter, REG_MACID1+i);

	for (i=0; i<6; i++)
		bssid_1[i] = rtw_read8(adapter, REG_BSSID1+i);

	DBG_871X(FUNC_ADPT_FMT" after switch\n"
		"msr:0x%02x\n"
		"bcn_ctrl:0x%02x\n"
		"bcn_ctrl_1:0x%02x\n"
		"atimwnd:%u\n"
		"atimwnd_1:%u\n"
		"tsftr:%llu\n"
		"tsftr1:%llu\n"
		"macid:"MAC_FMT"\n"
		"bssid:"MAC_FMT"\n"
		"macid_1:"MAC_FMT"\n"
		"bssid_1:"MAC_FMT"\n"
		, FUNC_ADPT_ARG(adapter)
		, msr
		, bcn_ctrl
		, bcn_ctrl_1
		, *((u16*)atimwnd)
		, *((u16*)atimwnd_1)
		, *((u64*)tsftr)
		, *((u64*)tsftr_1)
		, MAC_ARG(macid)
		, MAC_ARG(bssid)
		, MAC_ARG(macid_1)
		, MAC_ARG(bssid_1)
	);
#endif /* DBG_RUNTIME_PORT_SWITCH */

#endif /* CONFIG_RUNTIME_PORT_SWITCH */
#endif /* CONFIG_CONCURRENT_MODE */
}

const char * const _h2c_msr_role_str[] = {
	"RSVD",
	"STA",
	"AP",
	"GC",
	"GO",
	"TDLS",
	"ADHOC",
	"INVALID",
};

/*
* rtw_hal_set_FwMediaStatusRpt_cmd -
*
* @adapter:
* @opmode:  0:disconnect, 1:connect
* @miracast: 0:it's not in miracast scenario. 1:it's in miracast scenario
* @miracast_sink: 0:source. 1:sink
* @role: The role of this macid. 0:rsvd. 1:STA. 2:AP. 3:GC. 4:GO. 5:TDLS
* @macid:
* @macid_ind:  0:update Media Status to macid.  1:update Media Status from macid to macid_end
* @macid_end:
*/
s32 rtw_hal_set_FwMediaStatusRpt_cmd(_adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid, bool macid_ind, u8 macid_end)
{
	struct macid_ctl_t *macid_ctl = &adapter->dvobj->macid_ctl;
	u8 parm[H2C_MEDIA_STATUS_RPT_LEN] = {0};
	int i;
	s32 ret;

	SET_H2CCMD_MSRRPT_PARM_OPMODE(parm, opmode);
	SET_H2CCMD_MSRRPT_PARM_MACID_IND(parm, macid_ind);
	SET_H2CCMD_MSRRPT_PARM_MIRACAST(parm, miracast);
	SET_H2CCMD_MSRRPT_PARM_MIRACAST_SINK(parm, miracast_sink);
	SET_H2CCMD_MSRRPT_PARM_ROLE(parm, role);
	SET_H2CCMD_MSRRPT_PARM_MACID(parm, macid);
	SET_H2CCMD_MSRRPT_PARM_MACID_END(parm, macid_end);

	RT_PRINT_DATA(_module_hal_init_c_, _drv_always_, "MediaStatusRpt parm:", parm, H2C_MEDIA_STATUS_RPT_LEN);

	ret = rtw_hal_fill_h2c_cmd(adapter, H2C_MEDIA_STATUS_RPT, H2C_MEDIA_STATUS_RPT_LEN, parm);
	if (ret != _SUCCESS)
		goto exit;

#if defined(CONFIG_RTL8188E)
	if (rtw_get_chip_type(adapter) == RTL8188E) {
		HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);

		/* 8188E FW doesn't set macid no link, driver does it by self */
		if (opmode)
			rtw_hal_set_hwreg(adapter, HW_VAR_MACID_LINK, &macid);
		else
			rtw_hal_set_hwreg(adapter, HW_VAR_MACID_NOLINK, &macid);

		/* for 8188E RA */
		#if (RATE_ADAPTIVE_SUPPORT == 1)
		if (hal_data->fw_ractrl == _FALSE) {
			u8 max_macid;

			max_macid = rtw_search_max_mac_id(adapter);
			rtw_hal_set_hwreg(adapter, HW_VAR_TX_RPT_MAX_MACID, &max_macid);
		}
		#endif
	}
#endif

#if defined(CONFIG_RTL8812A) || defined(CONFIG_RTL8821A)
	/* TODO: this should move to IOT issue area */
	if (rtw_get_chip_type(adapter) == RTL8812
		|| rtw_get_chip_type(adapter) == RTL8821
	) {
		if (MLME_IS_STA(adapter))
			Hal_PatchwithJaguar_8812(adapter, opmode);
	}
#endif

	SET_H2CCMD_MSRRPT_PARM_MACID_IND(parm, 0);
	if (macid_ind == 0)
		macid_end = macid;

	for (i = macid; macid <= macid_end; macid++)
		rtw_macid_ctl_set_h2c_msr(macid_ctl, macid, parm[0]);

exit:
	return ret;
}

inline s32 rtw_hal_set_FwMediaStatusRpt_single_cmd(_adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid)
{
	return rtw_hal_set_FwMediaStatusRpt_cmd(adapter, opmode, miracast, miracast_sink, role, macid, 0, 0);
}

inline s32 rtw_hal_set_FwMediaStatusRpt_range_cmd(_adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid, u8 macid_end)
{
	return rtw_hal_set_FwMediaStatusRpt_cmd(adapter, opmode, miracast, miracast_sink, role, macid, 1, macid_end);
}

void rtw_hal_set_FwRsvdPage_cmd(PADAPTER padapter, PRSVDPAGE_LOC rsvdpageloc)
{
	struct	hal_ops *pHalFunc = &padapter->HalFunc;
	u8	u1H2CRsvdPageParm[H2C_RSVDPAGE_LOC_LEN]={0};
	u8	ret = 0;

	DBG_871X("RsvdPageLoc: ProbeRsp=%d PsPoll=%d Null=%d QoSNull=%d BTNull=%d\n",
		rsvdpageloc->LocProbeRsp, rsvdpageloc->LocPsPoll,
		rsvdpageloc->LocNullData, rsvdpageloc->LocQosNull,
		rsvdpageloc->LocBTQosNull);

	SET_H2CCMD_RSVDPAGE_LOC_PROBE_RSP(u1H2CRsvdPageParm, rsvdpageloc->LocProbeRsp);
	SET_H2CCMD_RSVDPAGE_LOC_PSPOLL(u1H2CRsvdPageParm, rsvdpageloc->LocPsPoll);
	SET_H2CCMD_RSVDPAGE_LOC_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocNullData);
	SET_H2CCMD_RSVDPAGE_LOC_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocQosNull);
	SET_H2CCMD_RSVDPAGE_LOC_BT_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocBTQosNull);

	ret = rtw_hal_fill_h2c_cmd(padapter,
				H2C_RSVD_PAGE,
				H2C_RSVDPAGE_LOC_LEN,
				u1H2CRsvdPageParm);

}

#ifdef CONFIG_GPIO_WAKEUP
void rtw_hal_switch_gpio_wl_ctrl(_adapter *padapter, u8 index, u8 enable)
{
	/*
	* Switch GPIO_13, GPIO_14 to wlan control, or pull GPIO_13,14 MUST fail.
	* It happended at 8723B/8192E/8821A. New IC will check multi function GPIO,
	* and implement HAL function.
	* TODO: GPIO_8 multi function?
	*/
	if (index == 13 || index == 14)
		rtw_hal_set_hwreg(padapter, HW_SET_GPIO_WL_CTRL, (u8 *)(&enable));
}

void rtw_hal_set_output_gpio(_adapter *padapter, u8 index, u8 outputval)
{
	if ( index <= 7 ) {
		/* config GPIO mode */
		rtw_write8(padapter, REG_GPIO_PIN_CTRL + 3,
				rtw_read8(padapter, REG_GPIO_PIN_CTRL + 3) & ~BIT(index) );

		/* config GPIO Sel */
		/* 0: input */
		/* 1: output */
		rtw_write8(padapter, REG_GPIO_PIN_CTRL + 2,
				rtw_read8(padapter, REG_GPIO_PIN_CTRL + 2) | BIT(index));

		/* set output value */
		if ( outputval ) {
			rtw_write8(padapter, REG_GPIO_PIN_CTRL + 1,
					rtw_read8(padapter, REG_GPIO_PIN_CTRL + 1) | BIT(index));
		} else {
			rtw_write8(padapter, REG_GPIO_PIN_CTRL + 1,
					rtw_read8(padapter, REG_GPIO_PIN_CTRL + 1) & ~BIT(index));
		}
	} else if (index <= 15){
		/* 88C Series: */
		/* index: 11~8 transform to 3~0 */
		/* 8723 Series: */
		/* index: 12~8 transform to 4~0 */

		index -= 8;

		/* config GPIO mode */
		rtw_write8(padapter, REG_GPIO_PIN_CTRL_2 + 3,
				rtw_read8(padapter, REG_GPIO_PIN_CTRL_2 + 3) & ~BIT(index) );

		/* config GPIO Sel */
		/* 0: input */
		/* 1: output */
		rtw_write8(padapter, REG_GPIO_PIN_CTRL_2 + 2,
				rtw_read8(padapter, REG_GPIO_PIN_CTRL_2 + 2) | BIT(index));

		/* set output value */
		if ( outputval ) {
			rtw_write8(padapter, REG_GPIO_PIN_CTRL_2 + 1,
					rtw_read8(padapter, REG_GPIO_PIN_CTRL_2 + 1) | BIT(index));
		} else {
			rtw_write8(padapter, REG_GPIO_PIN_CTRL_2 + 1,
					rtw_read8(padapter, REG_GPIO_PIN_CTRL_2 + 1) & ~BIT(index));
		}
	} else {
		DBG_871X("%s: invalid GPIO%d=%d\n",
				__FUNCTION__, index, outputval);
	}
}
#endif

void rtw_hal_set_FwAoacRsvdPage_cmd(PADAPTER padapter, PRSVDPAGE_LOC rsvdpageloc)
{
	struct	hal_ops *pHalFunc = &padapter->HalFunc;
	struct	pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);
	struct	mlme_priv *pmlmepriv = &padapter->mlmepriv;
	u8	res = 0, count = 0, ret = 0;
}

static void rtw_hal_construct_beacon(_adapter *padapter,
		u8 *pframe, u32 *pLength)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	u16					*fctrl;
	u32					rate_len, pktlen;
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX		*cur_network = &(pmlmeinfo->network);
	u8	bc_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


	//DBG_871X("%s\n", __FUNCTION__);

	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	fctrl = &(pwlanhdr->frame_ctl);
	*(fctrl) = 0;

	_rtw_memcpy(pwlanhdr->addr1, bc_addr, ETH_ALEN);
	_rtw_memcpy(pwlanhdr->addr2, adapter_mac_addr(padapter), ETH_ALEN);
	_rtw_memcpy(pwlanhdr->addr3, get_my_bssid(cur_network), ETH_ALEN);

	SetSeqNum(pwlanhdr, 0/*pmlmeext->mgnt_seq*/);
	//pmlmeext->mgnt_seq++;
	SetFrameSubType(pframe, WIFI_BEACON);

	pframe += sizeof(struct rtw_ieee80211_hdr_3addr);
	pktlen = sizeof (struct rtw_ieee80211_hdr_3addr);

	//timestamp will be inserted by hardware
	pframe += 8;
	pktlen += 8;

	// beacon interval: 2 bytes
	_rtw_memcpy(pframe, (unsigned char *)(rtw_get_beacon_interval_from_ie(cur_network->IEs)), 2);

	pframe += 2;
	pktlen += 2;

	// capability info: 2 bytes
	_rtw_memcpy(pframe, (unsigned char *)(rtw_get_capability_from_ie(cur_network->IEs)), 2);

	pframe += 2;
	pktlen += 2;

	if( (pmlmeinfo->state&0x03) == WIFI_FW_AP_STATE)
	{
		//DBG_871X("ie len=%d\n", cur_network->IELength);
		pktlen += cur_network->IELength - sizeof(NDIS_802_11_FIXED_IEs);
		_rtw_memcpy(pframe, cur_network->IEs+sizeof(NDIS_802_11_FIXED_IEs), pktlen);

		goto _ConstructBeacon;
	}

	//below for ad-hoc mode

	// SSID
	pframe = rtw_set_ie(pframe, _SSID_IE_, cur_network->Ssid.SsidLength, cur_network->Ssid.Ssid, &pktlen);

	// supported rates...
	rate_len = rtw_get_rateset_len(cur_network->SupportedRates);
	pframe = rtw_set_ie(pframe, _SUPPORTEDRATES_IE_, ((rate_len > 8)? 8: rate_len), cur_network->SupportedRates, &pktlen);

	// DS parameter set
	pframe = rtw_set_ie(pframe, _DSSET_IE_, 1, (unsigned char *)&(cur_network->Configuration.DSConfig), &pktlen);

	if( (pmlmeinfo->state&0x03) == WIFI_FW_ADHOC_STATE)
	{
		u32 ATIMWindow;
		// IBSS Parameter Set...
		//ATIMWindow = cur->Configuration.ATIMWindow;
		ATIMWindow = 0;
		pframe = rtw_set_ie(pframe, _IBSS_PARA_IE_, 2, (unsigned char *)(&ATIMWindow), &pktlen);
	}


	//todo: ERP IE


	// EXTERNDED SUPPORTED RATE
	if (rate_len > 8)
	{
		pframe = rtw_set_ie(pframe, _EXT_SUPPORTEDRATES_IE_, (rate_len - 8), (cur_network->SupportedRates + 8), &pktlen);
	}


	//todo:HT for adhoc

_ConstructBeacon:

	if ((pktlen + TXDESC_SIZE) > 512)
	{
		DBG_871X("beacon frame too large\n");
		return;
	}

	*pLength = pktlen;

	//DBG_871X("%s bcn_sz=%d\n", __FUNCTION__, pktlen);

}

static void rtw_hal_construct_PSPoll(_adapter *padapter,
		u8 *pframe, u32 *pLength)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	u16					*fctrl;
	u32					pktlen;
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	//DBG_871X("%s\n", __FUNCTION__);

	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	// Frame control.
	fctrl = &(pwlanhdr->frame_ctl);
	*(fctrl) = 0;
	SetPwrMgt(fctrl);
	SetFrameSubType(pframe, WIFI_PSPOLL);

	// AID.
	SetDuration(pframe, (pmlmeinfo->aid | 0xc000));

	// BSSID.
	_rtw_memcpy(pwlanhdr->addr1, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);

	// TA.
	_rtw_memcpy(pwlanhdr->addr2, adapter_mac_addr(padapter), ETH_ALEN);

	*pLength = 16;
}

static void rtw_hal_construct_NullFunctionData(
	PADAPTER padapter,
	u8		*pframe,
	u32		*pLength,
	u8		*StaAddr,
	u8		bQoS,
	u8		AC,
	u8		bEosp,
	u8		bForcePowerSave)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	u16						*fctrl;
	u32						pktlen;
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct wlan_network		*cur_network = &pmlmepriv->cur_network;
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);


	//DBG_871X("%s:%d\n", __FUNCTION__, bForcePowerSave);

	pwlanhdr = (struct rtw_ieee80211_hdr*)pframe;

	fctrl = &pwlanhdr->frame_ctl;
	*(fctrl) = 0;
	if (bForcePowerSave)
	{
		SetPwrMgt(fctrl);
	}

	switch(cur_network->network.InfrastructureMode)
	{
		case Ndis802_11Infrastructure:
			SetToDs(fctrl);
			_rtw_memcpy(pwlanhdr->addr1, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);
			_rtw_memcpy(pwlanhdr->addr2, adapter_mac_addr(padapter), ETH_ALEN);
			_rtw_memcpy(pwlanhdr->addr3, StaAddr, ETH_ALEN);
			break;
		case Ndis802_11APMode:
			SetFrDs(fctrl);
			_rtw_memcpy(pwlanhdr->addr1, StaAddr, ETH_ALEN);
			_rtw_memcpy(pwlanhdr->addr2, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);
			_rtw_memcpy(pwlanhdr->addr3, adapter_mac_addr(padapter), ETH_ALEN);
			break;
		case Ndis802_11IBSS:
		default:
			_rtw_memcpy(pwlanhdr->addr1, StaAddr, ETH_ALEN);
			_rtw_memcpy(pwlanhdr->addr2, adapter_mac_addr(padapter), ETH_ALEN);
			_rtw_memcpy(pwlanhdr->addr3, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);
			break;
	}

	SetSeqNum(pwlanhdr, 0);

	if (bQoS == _TRUE) {
		struct rtw_ieee80211_hdr_3addr_qos *pwlanqoshdr;

		SetFrameSubType(pframe, WIFI_QOS_DATA_NULL);

		pwlanqoshdr = (struct rtw_ieee80211_hdr_3addr_qos*)pframe;
		SetPriority(&pwlanqoshdr->qc, AC);
		SetEOSP(&pwlanqoshdr->qc, bEosp);

		pktlen = sizeof(struct rtw_ieee80211_hdr_3addr_qos);
	} else {
		SetFrameSubType(pframe, WIFI_DATA_NULL);

		pktlen = sizeof(struct rtw_ieee80211_hdr_3addr);
	}

	*pLength = pktlen;
}

void rtw_hal_construct_ProbeRsp(_adapter *padapter, u8 *pframe, u32 *pLength,
		u8 *StaAddr, BOOLEAN bHideSSID)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	u16					*fctrl;
	u8					*mac, *bssid;
	u32					pktlen;
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX  *cur_network = &(pmlmeinfo->network);

	/*DBG_871X("%s\n", __FUNCTION__);*/

	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	mac = adapter_mac_addr(padapter);
	bssid = cur_network->MacAddress;

	fctrl = &(pwlanhdr->frame_ctl);
	*(fctrl) = 0;
	_rtw_memcpy(pwlanhdr->addr1, StaAddr, ETH_ALEN);
	_rtw_memcpy(pwlanhdr->addr2, mac, ETH_ALEN);
	_rtw_memcpy(pwlanhdr->addr3, bssid, ETH_ALEN);

	SetSeqNum(pwlanhdr, 0);
	SetFrameSubType(fctrl, WIFI_PROBERSP);

	pktlen = sizeof(struct rtw_ieee80211_hdr_3addr);
	pframe += pktlen;

	if (cur_network->IELength > MAX_IE_SZ)
		return;

	_rtw_memcpy(pframe, cur_network->IEs, cur_network->IELength);
	pframe += cur_network->IELength;
	pktlen += cur_network->IELength;

	*pLength = pktlen;
}

/*
 * Description: Fill the reserved packets that FW will use to RSVD page.
 *			Now we just send 4 types packet to rsvd page.
 *			(1)Beacon, (2)Ps-poll, (3)Null data, (4)ProbeRsp.
 * Input:
 * finished - FALSE:At the first time we will send all the packets as a large packet to Hw,
 *		    so we need to set the packet length to total lengh.
 *	      TRUE: At the second time, we should send the first packet (default:beacon)
 *		    to Hw again and set the lengh in descriptor to the real beacon lengh.
 * 2009.10.15 by tynli.
 *
 * Page Size = 128: 8188e, 8723a/b, 8192c/d,  
 * Page Size = 256: 8192e, 8821a
 * Page Size = 512: 8812a
 */

void rtw_hal_set_fw_rsvd_page(_adapter* adapter, bool finished)
{
	PHAL_DATA_TYPE pHalData;
	struct xmit_frame	*pcmdframe;
	struct pkt_attrib	*pattrib;
	struct xmit_priv	*pxmitpriv;
	struct mlme_ext_priv	*pmlmeext;
	struct mlme_ext_info	*pmlmeinfo;
	struct pwrctrl_priv *pwrctl;
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct hal_ops *pHalFunc = &adapter->HalFunc;
	u32	BeaconLength = 0, ProbeRspLength = 0, PSPollLength = 0;
	u32	NullDataLength = 0, QosNullLength = 0, BTQosNullLength = 0;
	u32	ProbeReqLength = 0, NullFunctionDataLength = 0;
	u8	TxDescLen = TXDESC_SIZE, TxDescOffset = TXDESC_OFFSET;
	u8	TotalPageNum = 0 , CurtPktPageNum = 0 , RsvdPageNum = 0;
	u8	*ReservedPagePacket;
	u16	BufIndex = 0;
	u32	TotalPacketLen = 0, MaxRsvdPageBufSize = 0, PageSize = 0;
	RSVDPAGE_LOC	RsvdPageLoc;

#ifdef DBG_CONFIG_ERROR_DETECT
	struct sreset_priv *psrtpriv;
#endif /* DBG_CONFIG_ERROR_DETECT */


	pHalData = GET_HAL_DATA(adapter);
#ifdef DBG_CONFIG_ERROR_DETECT
	psrtpriv = &pHalData->srestpriv;
#endif
	pxmitpriv = &adapter->xmitpriv;
	pmlmeext = &adapter->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;
	pwrctl = adapter_to_pwrctl(adapter);

	rtw_hal_get_def_var(adapter, HAL_DEF_TX_PAGE_SIZE, (u8 *)&PageSize);
	
	if (PageSize == 0) {
		DBG_871X("[Error]: %s, PageSize is zero!!\n", __func__);
		return;
	}

	if (pwrctl->wowlan_mode == _TRUE || pwrctl->wowlan_ap_mode == _TRUE)
		RsvdPageNum = rtw_hal_get_txbuff_rsvd_page_num(adapter, _TRUE);
	else
		RsvdPageNum = rtw_hal_get_txbuff_rsvd_page_num(adapter, _FALSE);
	
	DBG_871X("%s PageSize: %d, RsvdPageNUm: %d\n",__func__, PageSize, RsvdPageNum);
	
	MaxRsvdPageBufSize = RsvdPageNum*PageSize;

	if (MaxRsvdPageBufSize > MAX_CMDBUF_SZ) {
		DBG_871X("%s MaxRsvdPageBufSize(%d) is larger than MAX_CMDBUF_SZ(%d)",
			__func__, MaxRsvdPageBufSize, MAX_CMDBUF_SZ);
		rtw_warn_on(1);
		return;
	}
	
	pcmdframe = rtw_alloc_cmdxmitframe(pxmitpriv);
	
	if (pcmdframe == NULL) {
		DBG_871X("%s: alloc ReservedPagePacket fail!\n", __FUNCTION__);
		return;
	}

	ReservedPagePacket = pcmdframe->buf_addr;
	_rtw_memset(&RsvdPageLoc, 0, sizeof(RSVDPAGE_LOC));

	/* beacon * 2 pages */
	BufIndex = TxDescOffset;
	rtw_hal_construct_beacon(adapter,
			&ReservedPagePacket[BufIndex], &BeaconLength);

	/*
	* When we count the first page size, we need to reserve description size for the RSVD
	* packet, it will be filled in front of the packet in TXPKTBUF.
	*/
	CurtPktPageNum = (u8)PageNum((TxDescLen + BeaconLength), PageSize);
	/* If we don't add 1 more page, ARP offload function will fail at 8723bs.*/
	if (CurtPktPageNum == 1) 
		CurtPktPageNum += 1;

	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum*PageSize);

	if (pwrctl->wowlan_ap_mode == _TRUE) {
		/* (4) probe response*/
		RsvdPageLoc.LocProbeRsp = TotalPageNum;
		rtw_hal_construct_ProbeRsp(
			adapter, &ReservedPagePacket[BufIndex],
			&ProbeRspLength,
			get_my_bssid(&pmlmeinfo->network), _FALSE);
		rtw_hal_fill_fake_txdesc(adapter,
			&ReservedPagePacket[BufIndex-TxDescLen],
			ProbeRspLength, _FALSE, _FALSE, _FALSE);

		CurtPktPageNum = (u8)PageNum(TxDescLen + ProbeRspLength, PageSize);
		TotalPageNum += CurtPktPageNum;
		TotalPacketLen = BufIndex + ProbeRspLength;
		BufIndex += (CurtPktPageNum*PageSize);
		goto download_page;
	}

	/* ps-poll * 1 page */
	RsvdPageLoc.LocPsPoll = TotalPageNum;
	DBG_871X("LocPsPoll: %d\n", RsvdPageLoc.LocPsPoll);
	rtw_hal_construct_PSPoll(adapter,
			&ReservedPagePacket[BufIndex], &PSPollLength);
	rtw_hal_fill_fake_txdesc(adapter,
			&ReservedPagePacket[BufIndex-TxDescLen],
			PSPollLength, _TRUE, _FALSE, _FALSE);

	CurtPktPageNum = (u8)PageNum((TxDescLen + PSPollLength), PageSize);

	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum*PageSize);

#ifdef CONFIG_BT_COEXIST
	/* BT Qos null data * 1 page */
	RsvdPageLoc.LocBTQosNull = TotalPageNum;
	DBG_871X("LocBTQosNull: %d\n", RsvdPageLoc.LocBTQosNull);
	rtw_hal_construct_NullFunctionData(
			adapter,
			&ReservedPagePacket[BufIndex],
			&BTQosNullLength,
			get_my_bssid(&pmlmeinfo->network),
			_TRUE, 0, 0, _FALSE);
	rtw_hal_fill_fake_txdesc(adapter,
			&ReservedPagePacket[BufIndex-TxDescLen],
			BTQosNullLength, _FALSE, _TRUE, _FALSE);

	CurtPktPageNum = (u8)PageNum(TxDescLen + BTQosNullLength, PageSize);

	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum*PageSize);
#endif /* CONFIG_BT_COEXIT */

	/* null data * 1 page */
	RsvdPageLoc.LocNullData = TotalPageNum;
	DBG_871X("LocNullData: %d\n", RsvdPageLoc.LocNullData);
	rtw_hal_construct_NullFunctionData(
			adapter,
			&ReservedPagePacket[BufIndex],
			&NullDataLength,
			get_my_bssid(&pmlmeinfo->network),
			_FALSE, 0, 0, _FALSE);
	rtw_hal_fill_fake_txdesc(adapter,
			&ReservedPagePacket[BufIndex-TxDescLen],
			NullDataLength, _FALSE, _FALSE, _FALSE);

	CurtPktPageNum = (u8)PageNum(TxDescLen + NullDataLength, PageSize);

	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum*PageSize);

	//Qos null data * 1 page
	RsvdPageLoc.LocQosNull = TotalPageNum;
	DBG_871X("LocQosNull: %d\n", RsvdPageLoc.LocQosNull);
	rtw_hal_construct_NullFunctionData(
			adapter,
			&ReservedPagePacket[BufIndex],
			&QosNullLength,
			get_my_bssid(&pmlmeinfo->network),
			_TRUE, 0, 0, _FALSE);
	rtw_hal_fill_fake_txdesc(adapter,
			&ReservedPagePacket[BufIndex-TxDescLen],
			QosNullLength, _FALSE, _FALSE, _FALSE);

	CurtPktPageNum = (u8)PageNum(TxDescLen + QosNullLength, PageSize);

	TotalPageNum += CurtPktPageNum;

	TotalPacketLen = BufIndex + QosNullLength;

	BufIndex += (CurtPktPageNum*PageSize);

download_page:
	/* DBG_871X("%s BufIndex(%d), TxDescLen(%d), PageSize(%d)\n",__func__, BufIndex, TxDescLen, PageSize);*/
	DBG_871X("%s PageNum(%d), pktlen(%d)\n",
				__func__, TotalPageNum, TotalPacketLen);

	if (TotalPacketLen > MaxRsvdPageBufSize) {
		DBG_871X("%s(ERROR): rsvd page size is not enough!!TotalPacketLen %d, MaxRsvdPageBufSize %d\n",
				__FUNCTION__, TotalPacketLen,MaxRsvdPageBufSize);
		rtw_warn_on(1);
		goto error;
	} else {
		/* update attribute */
		pattrib = &pcmdframe->attrib;
		update_mgntframe_attrib(adapter, pattrib);
		pattrib->qsel = QSLT_BEACON;
		pattrib->pktlen = TotalPacketLen - TxDescOffset;
		pattrib->last_txcmdsz = TotalPacketLen - TxDescOffset;
#ifdef CONFIG_PCI_HCI
		dump_mgntframe(adapter, pcmdframe);
#else
		dump_mgntframe_and_wait(adapter, pcmdframe, 100);
#endif
	}

	DBG_871X("%s: Set RSVD page location to Fw ,TotalPacketLen(%d), TotalPageNum(%d)\n",
			__func__,TotalPacketLen,TotalPageNum);

	if(check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE) {
		rtw_hal_set_FwRsvdPage_cmd(adapter, &RsvdPageLoc);
		if (pwrctl->wowlan_mode == _TRUE)
			rtw_hal_set_FwAoacRsvdPage_cmd(adapter, &RsvdPageLoc);
	} else if (pwrctl->wowlan_pno_enable) {
#ifdef CONFIG_PNO_SUPPORT
		rtw_hal_set_FwAoacRsvdPage_cmd(adapter, &RsvdPageLoc);
		if(pwrctl->pno_in_resume)
			rtw_hal_set_scan_offload_info_cmd(adapter,
					&RsvdPageLoc, 0);
		else
			rtw_hal_set_scan_offload_info_cmd(adapter,
					&RsvdPageLoc, 1);
#endif /* CONFIG_PNO_SUPPORT */
	}
	return;
error:
	rtw_free_xmitframe(pxmitpriv, pcmdframe);
}

#ifdef CONFIG_TDLS
#ifdef CONFIG_TDLS_CH_SW
s32 rtw_hal_ch_sw_oper_offload(_adapter *padapter, u8 channel, u8 channel_offset, u16 bwmode)
{
	PHAL_DATA_TYPE	pHalData =  GET_HAL_DATA(padapter);
	u8 ch_sw_h2c_buf[4] = {0x00, 0x00, 0x00, 0x00};


	SET_H2CCMD_CH_SW_OPER_OFFLOAD_CH_NUM(ch_sw_h2c_buf, channel);	
	SET_H2CCMD_CH_SW_OPER_OFFLOAD_BW_MODE(ch_sw_h2c_buf, bwmode);
	switch (bwmode) {
		case CHANNEL_WIDTH_40:
			SET_H2CCMD_CH_SW_OPER_OFFLOAD_BW_40M_SC(ch_sw_h2c_buf, channel_offset);	
			break;
		case CHANNEL_WIDTH_80:
			SET_H2CCMD_CH_SW_OPER_OFFLOAD_BW_80M_SC(ch_sw_h2c_buf, channel_offset);
			break;
		case CHANNEL_WIDTH_20:	
		default:
			break;
	}
	SET_H2CCMD_CH_SW_OPER_OFFLOAD_RFE_TYPE(ch_sw_h2c_buf, pHalData->RFEType);

	return rtw_hal_fill_h2c_cmd(padapter, H2C_CHNL_SWITCH_OPER_OFFLOAD, sizeof(ch_sw_h2c_buf), ch_sw_h2c_buf);
}
#endif
#endif

void SetHwReg(_adapter *adapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
_func_enter_;

	switch (variable) {
		case HW_VAR_PORT_SWITCH:
			hw_var_port_switch(adapter);
			break;
		case HW_VAR_INIT_RTS_RATE:
		{
			u16 brate_cfg = *((u16*)val);
			u8 rate_index = 0;
			HAL_VERSION *hal_ver = &hal_data->VersionID;

			if (IS_8188E(*hal_ver)) {

				while (brate_cfg > 0x1) {
					brate_cfg = (brate_cfg >> 1);
					rate_index++;
				}
				rtw_write8(adapter, REG_INIRTS_RATE_SEL, rate_index);
			} else {
				rtw_warn_on(1);
			}
		}
			break;
		case HW_VAR_SEC_CFG:
		{
			#if defined(CONFIG_CONCURRENT_MODE) && !defined(DYNAMIC_CAMID_ALLOC)
			// enable tx enc and rx dec engine, and no key search for MC/BC
			rtw_write8(adapter, REG_SECCFG, SCR_NoSKMC|SCR_RxDecEnable|SCR_TxEncEnable);
			#elif defined(DYNAMIC_CAMID_ALLOC)
			u16 reg_scr_ori;
			u16 reg_scr;

			reg_scr = reg_scr_ori = rtw_read16(adapter, REG_SECCFG);
			reg_scr |= (SCR_CHK_KEYID|SCR_RxDecEnable|SCR_TxEncEnable);

			if (_rtw_camctl_chk_cap(adapter, SEC_CAP_CHK_BMC))
				reg_scr |= SCR_CHK_BMC;

			if (_rtw_camctl_chk_flags(adapter, SEC_STATUS_STA_PK_GK_CONFLICT_DIS_BMC_SEARCH))
				reg_scr |= SCR_NoSKMC;

			if (reg_scr != reg_scr_ori)
				rtw_write16(adapter, REG_SECCFG, reg_scr);
			#else
			rtw_write8(adapter, REG_SECCFG, *((u8*)val));
			#endif
		}
			break;
		case HW_VAR_SEC_DK_CFG:
		{
			struct security_priv *sec = &adapter->securitypriv;
			u8 reg_scr = rtw_read8(adapter, REG_SECCFG);

			if (val) /* Enable default key related setting */
			{
				reg_scr |= SCR_TXBCUSEDK;
				if (sec->dot11AuthAlgrthm != dot11AuthAlgrthm_8021X)
					reg_scr |= (SCR_RxUseDK|SCR_TxUseDK);
			}
			else /* Disable default key related setting */
			{
				reg_scr &= ~(SCR_RXBCUSEDK|SCR_TXBCUSEDK|SCR_RxUseDK|SCR_TxUseDK);
			}

			rtw_write8(adapter, REG_SECCFG, reg_scr);
		}
			break;

		case HW_VAR_ASIX_IOT:
			// enable  ASIX IOT function
			if (*((u8*)val) == _TRUE) {
				// 0xa2e[0]=0 (disable rake receiver)
				rtw_write8(adapter, rCCK0_FalseAlarmReport+2, 
						rtw_read8(adapter, rCCK0_FalseAlarmReport+2) & ~(BIT0));
				//  0xa1c=0xa0 (reset channel estimation if signal quality is bad)
				rtw_write8(adapter, rCCK0_DSPParameter2, 0xa0);
			} else {
			// restore reg:0xa2e,   reg:0xa1c
				rtw_write8(adapter, rCCK0_FalseAlarmReport+2, 
						rtw_read8(adapter, rCCK0_FalseAlarmReport+2)|(BIT0));
				rtw_write8(adapter, rCCK0_DSPParameter2, 0x00);
			}
			break;
		default:
			if (0)
				DBG_871X_LEVEL(_drv_always_, FUNC_ADPT_FMT" variable(%d) not defined!\n",
					FUNC_ADPT_ARG(adapter), variable);
			break;
	}

_func_exit_;
}

void GetHwReg(_adapter *adapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);

_func_enter_;

	switch (variable) {
	case HW_VAR_BASIC_RATE:
		*((u16*)val) = hal_data->BasicRateSet;
		break;
	case HW_VAR_RF_TYPE:
		*((u8*)val) = hal_data->rf_type;
		break;
	case HW_VAR_DO_IQK:
		*val = hal_data->bNeedIQK;
		break;
	case HW_VAR_CH_SW_NEED_TO_TAKE_CARE_IQK_INFO:
		if (hal_is_band_support(adapter, BAND_ON_5G))	
			*val = _TRUE;
		else
			*val = _FALSE;
		
		break;
	default:
		if (0)
			DBG_871X_LEVEL(_drv_always_, FUNC_ADPT_FMT" variable(%d) not defined!\n",
				FUNC_ADPT_ARG(adapter), variable);
		break;
	}

_func_exit_;
}

u8
SetHalDefVar(_adapter *adapter, HAL_DEF_VARIABLE variable, void *value)
{	
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	u8 bResult = _SUCCESS;

	switch(variable) {

	case HAL_DEF_DBG_DUMP_RXPKT:
		hal_data->bDumpRxPkt = *((u8*)value);
		break;
	case HAL_DEF_DBG_DUMP_TXPKT:
		hal_data->bDumpTxPkt = *((u8*)value);
		break;
	case HAL_DEF_ANT_DETECT:
		hal_data->AntDetection = *((u8 *)value);
		break;
	case HAL_DEF_DBG_DIS_PWT:
		hal_data->bDisableTXPowerTraining = *((u8*)value);
		break;	
	default:
		DBG_871X_LEVEL(_drv_always_, "%s: [WARNING] HAL_DEF_VARIABLE(%d) not defined!\n", __FUNCTION__, variable);
		bResult = _FAIL;
		break;
	}

	return bResult;
}

#ifdef CONFIG_BEAMFORMING
u8 rtw_hal_query_txbfer_rf_num(_adapter *adapter)
{
	struct registry_priv	*pregistrypriv = &adapter->registrypriv;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	
	if ((pregistrypriv->beamformer_rf_num) && (IS_HARDWARE_TYPE_8814AE(adapter) || IS_HARDWARE_TYPE_8814AU(adapter) || IS_HARDWARE_TYPE_8822BU(adapter)))
		return pregistrypriv->beamformer_rf_num;
	else if (IS_HARDWARE_TYPE_8814AE(adapter)
/*
#if defined(CONFIG_USB_HCI) 
	||  (IS_HARDWARE_TYPE_8814AU(adapter) && (pUsbModeMech->CurUsbMode == 2 || pUsbModeMech->HubUsbMode == 2)) //for USB3.0
#endif
*/
	) {	
		/*BF cap provided by Yu Chen, Sean, 2015, 01 */
		if (hal_data->rf_type == RF_3T3R) 
			return 2;
		else if (hal_data->rf_type == RF_4T4R)
			return 3;
		else 
			return 1;
	} else
		return 1;
	
}
u8 rtw_hal_query_txbfee_rf_num(_adapter *adapter)
{
	struct registry_priv		*pregistrypriv = &adapter->registrypriv;
	struct mlme_ext_priv	*pmlmeext = &adapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	
	if ((pregistrypriv->beamformee_rf_num) && (IS_HARDWARE_TYPE_8814AE(adapter) || IS_HARDWARE_TYPE_8814AU(adapter) || IS_HARDWARE_TYPE_8822BU(adapter)))
		return pregistrypriv->beamformee_rf_num;
	else if (IS_HARDWARE_TYPE_8814AE(adapter) || IS_HARDWARE_TYPE_8814AU(adapter)) {
		if (pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_BROADCOM)		
			return 2;		
		else
			return 2;/*TODO: May be 3 in the future, by ChenYu. */
	} else
		return 1;
		
}
#endif

u8
GetHalDefVar(_adapter *adapter, HAL_DEF_VARIABLE variable, void *value)
{
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	u8 bResult = _SUCCESS;

	switch(variable) {
		case HAL_DEF_UNDERCORATEDSMOOTHEDPWDB:
			{
				struct mlme_priv *pmlmepriv;
				struct sta_priv *pstapriv;
				struct sta_info *psta;

				pmlmepriv = &adapter->mlmepriv;
				pstapriv = &adapter->stapriv;
				psta = rtw_get_stainfo(pstapriv, pmlmepriv->cur_network.network.MacAddress);
				if (psta)
				{
					*((int*)value) = psta->rssi_stat.UndecoratedSmoothedPWDB;     
				}
			}
			break;
		case HAL_DEF_DBG_DUMP_RXPKT:
			*((u8*)value) = hal_data->bDumpRxPkt;
			break;
		case HAL_DEF_DBG_DUMP_TXPKT:
			*((u8*)value) = hal_data->bDumpTxPkt;
			break;
		case HAL_DEF_ANT_DETECT:
			*((u8 *)value) = hal_data->AntDetection;
			break;
		case HAL_DEF_MACID_SLEEP:
			*(u8*)value = _FALSE;
			break;
		case HAL_DEF_TX_PAGE_SIZE:
			*(( u32*)value) = PAGE_SIZE_128;
			break;
		case HAL_DEF_DBG_DIS_PWT:
			*(u8*)value = hal_data->bDisableTXPowerTraining;
			break;
#ifdef CONFIG_BEAMFORMING
		case HAL_DEF_BEAMFORMER_CAP:
			*(u8 *)value = rtw_hal_query_txbfer_rf_num(adapter);
			break;
		case HAL_DEF_BEAMFORMEE_CAP:
			*(u8 *)value = rtw_hal_query_txbfee_rf_num(adapter);
			break;
#endif
		default:
			DBG_871X_LEVEL(_drv_always_, "%s: [WARNING] HAL_DEF_VARIABLE(%d) not defined!\n", __FUNCTION__, variable);
			bResult = _FAIL;
			break;
	}

	return bResult;
}

void SetHalODMVar(
	PADAPTER				Adapter,
	HAL_ODM_VARIABLE		eVariable,
	PVOID					pValue1,
	BOOLEAN					bSet)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PDM_ODM_T podmpriv = &pHalData->odmpriv;
	//_irqL irqL;
	switch(eVariable){
		case HAL_ODM_STA_INFO:
			{	
				struct sta_info *psta = (struct sta_info *)pValue1;				
				if(bSet){
					DBG_8192C("### Set STA_(%d) info ###\n",psta->mac_id);
					ODM_CmnInfoPtrArrayHook(podmpriv, ODM_CMNINFO_STA_STATUS,psta->mac_id,psta);
				}
				else{
					DBG_8192C("### Clean STA_(%d) info ###\n",psta->mac_id);
					//_enter_critical_bh(&pHalData->odm_stainfo_lock, &irqL);
					ODM_CmnInfoPtrArrayHook(podmpriv, ODM_CMNINFO_STA_STATUS,psta->mac_id,NULL);
					
					//_exit_critical_bh(&pHalData->odm_stainfo_lock, &irqL);
			            }
			}
			break;
		case HAL_ODM_P2P_STATE:		
				ODM_CmnInfoUpdate(podmpriv,ODM_CMNINFO_WIFI_DIRECT,bSet);
			break;
		case HAL_ODM_WIFI_DISPLAY_STATE:
				ODM_CmnInfoUpdate(podmpriv,ODM_CMNINFO_WIFI_DISPLAY,bSet);
			break;
		case HAL_ODM_REGULATION:
				ODM_CmnInfoInit(podmpriv, ODM_CMNINFO_DOMAIN_CODE_2G, pHalData->Regulation2_4G);
				ODM_CmnInfoInit(podmpriv, ODM_CMNINFO_DOMAIN_CODE_5G, pHalData->Regulation5G);
			break;
#if defined(CONFIG_SIGNAL_DISPLAY_DBM) && defined(CONFIG_BACKGROUND_NOISE_MONITOR)		
		case HAL_ODM_NOISE_MONITOR:
			{
				struct noise_info *pinfo = (struct noise_info *)pValue1;

				#ifdef DBG_NOISE_MONITOR
				DBG_8192C("### Noise monitor chan(%d)-bPauseDIG:%d,IGIValue:0x%02x,max_time:%d (ms) ###\n",
					pinfo->chan,pinfo->bPauseDIG,pinfo->IGIValue,pinfo->max_time);
				#endif
				
				pHalData->noise[pinfo->chan] = ODM_InbandNoise_Monitor(podmpriv,pinfo->bPauseDIG,pinfo->IGIValue,pinfo->max_time);				
				DBG_871X("chan_%d, noise = %d (dBm)\n",pinfo->chan,pHalData->noise[pinfo->chan]);
				#ifdef DBG_NOISE_MONITOR
				DBG_871X("noise_a = %d, noise_b = %d  noise_all:%d \n", 
					podmpriv->noise_level.noise[ODM_RF_PATH_A], 
					podmpriv->noise_level.noise[ODM_RF_PATH_B],
					podmpriv->noise_level.noise_all);						
				#endif
			}
			break;
#endif/*#ifdef CONFIG_BACKGROUND_NOISE_MONITOR*/

		case HAL_ODM_INITIAL_GAIN:
			{
				u8 rx_gain = *((u8 *)(pValue1));
				/*printk("rx_gain:%x\n",rx_gain);*/
				if (rx_gain == 0xff) {/*restore rx gain*/
					/*ODM_Write_DIG(podmpriv,pDigTable->BackupIGValue);*/
					odm_PauseDIG(podmpriv, PHYDM_RESUME, PHYDM_PAUSE_LEVEL_0, rx_gain);
				} else {
					/*pDigTable->BackupIGValue = pDigTable->CurIGValue;*/
					/*ODM_Write_DIG(podmpriv,rx_gain);*/
					odm_PauseDIG(podmpriv, PHYDM_PAUSE, PHYDM_PAUSE_LEVEL_0, rx_gain);
				}
			}
			break;		
		case HAL_ODM_FA_CNT_DUMP:
			if (*((u8 *)pValue1))
				podmpriv->DebugComponents |= (ODM_COMP_DIG | ODM_COMP_FA_CNT);
			else
				podmpriv->DebugComponents &= ~(ODM_COMP_DIG | ODM_COMP_FA_CNT);
			break;
		case HAL_ODM_DBG_FLAG:
			ODM_CmnInfoUpdate(podmpriv, ODM_CMNINFO_DBG_COMP, *((u8Byte *)pValue1));
			break;
		case HAL_ODM_DBG_LEVEL:
			ODM_CmnInfoUpdate(podmpriv, ODM_CMNINFO_DBG_LEVEL, *((u4Byte *)pValue1));
			break;
		case HAL_ODM_RX_INFO_DUMP:
		{
			PFALSE_ALARM_STATISTICS FalseAlmCnt = (PFALSE_ALARM_STATISTICS)PhyDM_Get_Structure(podmpriv , PHYDM_FALSEALMCNT);
			pDIG_T	pDM_DigTable = &podmpriv->DM_DigTable;
			void *sel;

			sel = pValue1;

			DBG_871X_SEL(sel , "============ Rx Info dump ===================\n");
			DBG_871X_SEL(sel , "bLinked = %d, RSSI_Min = %d(%%), CurrentIGI = 0x%x\n", podmpriv->bLinked, podmpriv->RSSI_Min, pDM_DigTable->CurIGValue);
			DBG_871X_SEL(sel , "Cnt_Cck_fail = %d, Cnt_Ofdm_fail = %d, Total False Alarm = %d\n", FalseAlmCnt->Cnt_Cck_fail, FalseAlmCnt->Cnt_Ofdm_fail, FalseAlmCnt->Cnt_all);

			if (podmpriv->bLinked) {
					DBG_871X_SEL(sel , "RxRate = %s", HDATA_RATE(podmpriv->RxRate));
					DBG_871X_SEL(sel , " RSSI_A = %d(%%), RSSI_B = %d(%%)\n", podmpriv->RSSI_A, podmpriv->RSSI_B);
				#ifdef DBG_RX_SIGNAL_DISPLAY_RAW_DATA
						rtw_dump_raw_rssi_info(Adapter, sel);
				#endif
			}
		}		
		break;

#ifdef CONFIG_AUTO_CHNL_SEL_NHM
		case HAL_ODM_AUTO_CHNL_SEL:
		{
			ACS_OP	acs_op = *(ACS_OP *)pValue1;

			rtw_phydm_func_set(Adapter, ODM_BB_NHM_CNT);

			if (ACS_INIT == acs_op) {
				#ifdef DBG_AUTO_CHNL_SEL_NHM
				DBG_871X("[ACS-"ADPT_FMT"] HAL_ODM_AUTO_CHNL_SEL: ACS_INIT\n", ADPT_ARG(Adapter));
				#endif
				odm_AutoChannelSelectInit(podmpriv); 
			} else if (ACS_RESET == acs_op) {
				/* Reset statistics for auto channel selection mechanism.*/
				#ifdef DBG_AUTO_CHNL_SEL_NHM
				DBG_871X("[ACS-"ADPT_FMT"] HAL_ODM_AUTO_CHNL_SEL: ACS_RESET\n", ADPT_ARG(Adapter));
				#endif
				odm_AutoChannelSelectReset(podmpriv);
				
			} else if (ACS_SELECT == acs_op) {
				/* Collect NHM measurement result after current channel */
				#ifdef DBG_AUTO_CHNL_SEL_NHM
				DBG_871X("[ACS-"ADPT_FMT"] HAL_ODM_AUTO_CHNL_SEL: ACS_SELECT, CH(%d)\n", ADPT_ARG(Adapter), rtw_get_acs_channel(Adapter));
				#endif
				odm_AutoChannelSelect(podmpriv, rtw_get_acs_channel(Adapter));
			} else 
				DBG_871X("[ACS-"ADPT_FMT"] HAL_ODM_AUTO_CHNL_SEL: Unexpected OP\n", ADPT_ARG(Adapter));

		}
		break;
#endif
#ifdef CONFIG_ANTENNA_DIVERSITY
		case HAL_ODM_ANTDIV_SELECT:
		{
			u8	antenna = (*(u8 *)pValue1);
			
			/*switch antenna*/
			ODM_UpdateRxIdleAnt(&pHalData->odmpriv, antenna);
			/*DBG_871X("==> HAL_ODM_ANTDIV_SELECT, Ant_(%s)\n", (antenna == MAIN_ANT) ? "MAIN_ANT" : "AUX_ANT");*/

		}
		break;
#endif

		default:
			break;
	}
}	

void GetHalODMVar(	
	PADAPTER				Adapter,
	HAL_ODM_VARIABLE		eVariable,
	PVOID					pValue1,
	PVOID					pValue2)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	PDM_ODM_T podmpriv = &pHalData->odmpriv;
	
	switch (eVariable) {
#if defined(CONFIG_SIGNAL_DISPLAY_DBM) && defined(CONFIG_BACKGROUND_NOISE_MONITOR)
	case HAL_ODM_NOISE_MONITOR:
		{
			u8 chan = *(u8 *)pValue1;
			*(s16 *)pValue2 = pHalData->noise[chan];
			#ifdef DBG_NOISE_MONITOR
			DBG_8192C("### Noise monitor chan(%d)-noise:%d (dBm) ###\n",
				chan, pHalData->noise[chan]);
			#endif
		}
		break;
#endif/*#ifdef CONFIG_BACKGROUND_NOISE_MONITOR*/
	case HAL_ODM_DBG_FLAG:
		*((u8Byte *)pValue1) = podmpriv->DebugComponents;
		break;
	case HAL_ODM_DBG_LEVEL:
		*((u4Byte *)pValue1) = podmpriv->DebugLevel;
		break;

#ifdef CONFIG_AUTO_CHNL_SEL_NHM
	case HAL_ODM_AUTO_CHNL_SEL:
		{
			#ifdef DBG_AUTO_CHNL_SEL_NHM
			DBG_871X("[ACS-"ADPT_FMT"] HAL_ODM_AUTO_CHNL_SEL: GET_BEST_CHAN\n", ADPT_ARG(Adapter));
			#endif	
			/* Retrieve better channel from NHM mechanism	*/
			if (IsSupported24G(Adapter->registrypriv.wireless_mode)) 
				*((u8 *)(pValue1)) = ODM_GetAutoChannelSelectResult(podmpriv, BAND_ON_2_4G);
			if (IsSupported5G(Adapter->registrypriv.wireless_mode)) 
				*((u8 *)(pValue2)) = ODM_GetAutoChannelSelectResult(podmpriv, BAND_ON_5G);
		}
		break;
#endif
#ifdef CONFIG_ANTENNA_DIVERSITY
	case HAL_ODM_ANTDIV_SELECT:
		{
			pFAT_T	pDM_FatTable = &podmpriv->DM_FatTable;
			*((u8 *)pValue1) = pDM_FatTable->RxIdleAnt;
		}
		break;
#endif
	case HAL_ODM_INITIAL_GAIN:
		{
			pDIG_T pDM_DigTable = &podmpriv->DM_DigTable;
			*((u8 *)pValue1) = pDM_DigTable->CurIGValue;
		}
		break;
	default:
		break;
	}
}


u32 rtw_phydm_ability_ops(_adapter *adapter, HAL_PHYDM_OPS ops, u32 ability)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(adapter);
	PDM_ODM_T podmpriv = &pHalData->odmpriv;
	u32 result = 0;
	
	switch (ops) {
	case HAL_PHYDM_DIS_ALL_FUNC:
		podmpriv->SupportAbility = DYNAMIC_FUNC_DISABLE;
		break;
	case HAL_PHYDM_FUNC_SET:
		podmpriv->SupportAbility |= ability;
		break;
	case HAL_PHYDM_FUNC_CLR:
		podmpriv->SupportAbility &= ~(ability);
		break;			
	case HAL_PHYDM_ABILITY_BK:
		/* dm flag backup*/
		podmpriv->BK_SupportAbility = podmpriv->SupportAbility;
		break;
	case HAL_PHYDM_ABILITY_RESTORE:
		/* restore dm flag */
		podmpriv->SupportAbility = podmpriv->BK_SupportAbility;
		break;
	case HAL_PHYDM_ABILITY_SET:
		podmpriv->SupportAbility = ability;
		break;
	case HAL_PHYDM_ABILITY_GET:
		result = podmpriv->SupportAbility;
		break;
	}
	return result;
}


BOOLEAN 
eqNByte(
	u8*	str1,
	u8*	str2,
	u32	num
	)
{
	if(num==0)
		return _FALSE;
	while(num>0)
	{
		num--;
		if(str1[num]!=str2[num])
			return _FALSE;
	}
	return _TRUE;
}

//
//	Description:
//		Translate a character to hex digit.
//
u32
MapCharToHexDigit(
	IN		char		chTmp
)
{
	if(chTmp >= '0' && chTmp <= '9')
		return (chTmp - '0');
	else if(chTmp >= 'a' && chTmp <= 'f')
		return (10 + (chTmp - 'a'));
	else if(chTmp >= 'A' && chTmp <= 'F') 
		return (10 + (chTmp - 'A'));
	else
		return 0;	
}



//
//	Description:
//		Parse hex number from the string pucStr.
//
BOOLEAN 
GetHexValueFromString(
	IN		char*			szStr,
	IN OUT	u32*			pu4bVal,
	IN OUT	u32*			pu4bMove
)
{
	char*		szScan = szStr;

	// Check input parameter.
	if(szStr == NULL || pu4bVal == NULL || pu4bMove == NULL)
	{
		DBG_871X("GetHexValueFromString(): Invalid inpur argumetns! szStr: %p, pu4bVal: %p, pu4bMove: %p\n", szStr, pu4bVal, pu4bMove);
		return _FALSE;
	}

	// Initialize output.
	*pu4bMove = 0;
	*pu4bVal = 0;

	// Skip leading space.
	while(	*szScan != '\0' && 
			(*szScan == ' ' || *szScan == '\t') )
	{
		szScan++;
		(*pu4bMove)++;
	}

	// Skip leading '0x' or '0X'.
	if(*szScan == '0' && (*(szScan+1) == 'x' || *(szScan+1) == 'X'))
	{
		szScan += 2;
		(*pu4bMove) += 2;
	}	

	// Check if szScan is now pointer to a character for hex digit, 
	// if not, it means this is not a valid hex number.
	if(!IsHexDigit(*szScan))
	{
		return _FALSE;
	}

	// Parse each digit.
	do
	{
		(*pu4bVal) <<= 4;
		*pu4bVal += MapCharToHexDigit(*szScan);

		szScan++;
		(*pu4bMove)++;
	} while(IsHexDigit(*szScan));

	return _TRUE;
}

BOOLEAN 
GetFractionValueFromString(
	IN		char*			szStr,
	IN OUT	u8*				pInteger,
	IN OUT	u8*				pFraction,
	IN OUT	u32*			pu4bMove
)
{
	char	*szScan = szStr;

	// Initialize output.
	*pu4bMove = 0;
	*pInteger = 0;
	*pFraction = 0;

	// Skip leading space.
	while (	*szScan != '\0' && 	(*szScan == ' ' || *szScan == '\t') ) {
		++szScan;
		++(*pu4bMove);
	}

	// Parse each digit.
	do {
		(*pInteger) *= 10;
		*pInteger += ( *szScan - '0' );

		++szScan;
		++(*pu4bMove);

		if ( *szScan == '.' ) 
		{
			++szScan;
			++(*pu4bMove);
			
			if ( *szScan < '0' || *szScan > '9' )
				return _FALSE;
			else {
				*pFraction = *szScan - '0';
				++szScan;
				++(*pu4bMove);
				return _TRUE;
			}
		}
	} while(*szScan >= '0' && *szScan <= '9');

	return _TRUE;
}

//
//	Description:
//		Return TRUE if szStr is comment out with leading "//".
//
BOOLEAN
IsCommentString(
	IN		char			*szStr
)
{
	if(*szStr == '/' && *(szStr+1) == '/')
	{
		return _TRUE;
	}
	else
	{
		return _FALSE;
	}
}

BOOLEAN
GetU1ByteIntegerFromStringInDecimal(
	IN		char*	Str,
	IN OUT	u8*		pInt
	)
{
	u16 i = 0;
	*pInt = 0;

	while ( Str[i] != '\0' )
	{
		if ( Str[i] >= '0' && Str[i] <= '9' )
		{
			*pInt *= 10;
			*pInt += ( Str[i] - '0' );
		}
		else
		{
			return _FALSE;
		}
		++i;
	}

	return _TRUE;
}

// <20121004, Kordan> For example, 
// ParseQualifiedString(inString, 0, outString, '[', ']') gets "Kordan" from a string "Hello [Kordan]".
// If RightQualifier does not exist, it will hang on in the while loop
BOOLEAN 
ParseQualifiedString(
    IN		char*	In, 
    IN OUT	u32*	Start, 
    OUT		char*	Out, 
    IN		char		LeftQualifier, 
    IN		char		RightQualifier
    )
{
	u32	i = 0, j = 0;
	char	c = In[(*Start)++];

	if (c != LeftQualifier)
		return _FALSE;

	i = (*Start);
	while ((c = In[(*Start)++]) != RightQualifier) 
		; // find ']'
	j = (*Start) - 2;
	strncpy((char *)Out, (const char*)(In+i), j-i+1);

	return _TRUE;
}

BOOLEAN
isAllSpaceOrTab(
	u8*	data,
	u8	size
	)
{
	u8	cnt = 0, NumOfSpaceAndTab = 0;

	while( size > cnt )
	{
		if ( data[cnt] == ' ' || data[cnt] == '\t' || data[cnt] == '\0' )
			++NumOfSpaceAndTab;

		++cnt;
	}

	return size == NumOfSpaceAndTab;
}


void rtw_hal_check_rxfifo_full(_adapter *adapter)
{
	struct dvobj_priv *psdpriv = adapter->dvobj;
	struct debug_priv *pdbgpriv = &psdpriv->drv_dbg;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(adapter);
	int save_cnt=_FALSE;
	
	//switch counter to RX fifo
	if (IS_8188E(pHalData->VersionID) || IS_8188F(pHalData->VersionID)
		|| IS_8812_SERIES(pHalData->VersionID) || IS_8821_SERIES(pHalData->VersionID)
		|| IS_8723B_SERIES(pHalData->VersionID) || IS_8192E(pHalData->VersionID) || IS_8703B_SERIES(pHalData->VersionID))
	{
		rtw_write8(adapter, REG_RXERR_RPT+3, rtw_read8(adapter, REG_RXERR_RPT+3)|0xa0);
		save_cnt = _TRUE;
	}
	else 
	{
		//todo: other chips 
	}
	
		
	if (save_cnt) {
		pdbgpriv->dbg_rx_fifo_last_overflow = pdbgpriv->dbg_rx_fifo_curr_overflow;
		pdbgpriv->dbg_rx_fifo_curr_overflow = rtw_read16(adapter, REG_RXERR_RPT);
		pdbgpriv->dbg_rx_fifo_diff_overflow = pdbgpriv->dbg_rx_fifo_curr_overflow-pdbgpriv->dbg_rx_fifo_last_overflow;
	} else {
		/* special value to indicate no implementation */
		pdbgpriv->dbg_rx_fifo_last_overflow = 1;
		pdbgpriv->dbg_rx_fifo_curr_overflow = 1;
		pdbgpriv->dbg_rx_fifo_diff_overflow = 1;
	}
}

void linked_info_dump(_adapter *padapter,u8 benable)
{			
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(padapter);

	if(padapter->bLinkInfoDump == benable)
		return;
	
	DBG_871X("%s %s \n",__FUNCTION__,(benable)?"enable":"disable");
										
	if(benable){
		#ifdef CONFIG_LPS
		pwrctrlpriv->org_power_mgnt = pwrctrlpriv->power_mgnt;//keep org value
		rtw_pm_set_lps(padapter,PS_MODE_ACTIVE);
		#endif	
								
		#ifdef CONFIG_IPS	
		pwrctrlpriv->ips_org_mode = pwrctrlpriv->ips_mode;//keep org value
		rtw_pm_set_ips(padapter,IPS_NONE);
		#endif	
	}
	else{
		#ifdef CONFIG_IPS		
		rtw_pm_set_ips(padapter, pwrctrlpriv->ips_org_mode);
		#endif // CONFIG_IPS

		#ifdef CONFIG_LPS	
		rtw_pm_set_lps(padapter, pwrctrlpriv->org_power_mgnt );
		#endif // CONFIG_LPS
	}
	padapter->bLinkInfoDump = benable ;	
}

#ifdef DBG_RX_SIGNAL_DISPLAY_RAW_DATA
void rtw_get_raw_rssi_info(void *sel, _adapter *padapter)
{
	u8 isCCKrate,rf_path;
	PHAL_DATA_TYPE	pHalData =  GET_HAL_DATA(padapter);
	struct rx_raw_rssi *psample_pkt_rssi = &padapter->recvpriv.raw_rssi_info;
	
	DBG_871X_SEL_NL(sel,"RxRate = %s, PWDBALL = %d(%%), rx_pwr_all = %d(dBm)\n", 
			HDATA_RATE(psample_pkt_rssi->data_rate), psample_pkt_rssi->pwdball, psample_pkt_rssi->pwr_all);
	isCCKrate = (psample_pkt_rssi->data_rate <= DESC_RATE11M)?TRUE :FALSE;

	if(isCCKrate)
		psample_pkt_rssi->mimo_signal_strength[0] = psample_pkt_rssi->pwdball;
		
	for(rf_path = 0;rf_path<pHalData->NumTotalRFPath;rf_path++)
	{
		DBG_871X_SEL_NL(sel, "RF_PATH_%d=>signal_strength:%d(%%),signal_quality:%d(%%)\n"
			, rf_path, psample_pkt_rssi->mimo_signal_strength[rf_path], psample_pkt_rssi->mimo_signal_quality[rf_path]);
		
		if(!isCCKrate){
			DBG_871X_SEL_NL(sel,"\trx_ofdm_pwr:%d(dBm),rx_ofdm_snr:%d(dB)\n",
			psample_pkt_rssi->ofdm_pwr[rf_path],psample_pkt_rssi->ofdm_snr[rf_path]);
		}
	}	
}

void rtw_dump_raw_rssi_info(_adapter *padapter, void *sel)
{
	u8 isCCKrate,rf_path;
	PHAL_DATA_TYPE	pHalData =  GET_HAL_DATA(padapter);
	struct rx_raw_rssi *psample_pkt_rssi = &padapter->recvpriv.raw_rssi_info;
	DBG_871X_SEL(sel, "============ RAW Rx Info dump ===================\n");
	DBG_871X_SEL(sel, "RxRate = %s, PWDBALL = %d(%%), rx_pwr_all = %d(dBm)\n", HDATA_RATE(psample_pkt_rssi->data_rate), psample_pkt_rssi->pwdball, psample_pkt_rssi->pwr_all);

	isCCKrate = (psample_pkt_rssi->data_rate <= DESC_RATE11M)?TRUE :FALSE;

	if(isCCKrate)
		psample_pkt_rssi->mimo_signal_strength[0] = psample_pkt_rssi->pwdball;
		
	for(rf_path = 0;rf_path<pHalData->NumTotalRFPath;rf_path++)
	{
		DBG_871X_SEL(sel , "RF_PATH_%d=>signal_strength:%d(%%),signal_quality:%d(%%)"
			, rf_path, psample_pkt_rssi->mimo_signal_strength[rf_path], psample_pkt_rssi->mimo_signal_quality[rf_path]);
		
		if (!isCCKrate)
			DBG_871X_SEL(sel , ",rx_ofdm_pwr:%d(dBm),rx_ofdm_snr:%d(dB)\n", psample_pkt_rssi->ofdm_pwr[rf_path], psample_pkt_rssi->ofdm_snr[rf_path]);
		else
			DBG_871X_SEL(sel , "\n");

	}	
}

void rtw_store_phy_info(_adapter *padapter, union recv_frame *prframe)	
{
	u8 isCCKrate,rf_path;
	PHAL_DATA_TYPE	pHalData =  GET_HAL_DATA(padapter);
	struct rx_pkt_attrib *pattrib = &prframe->u.hdr.attrib;

	PODM_PHY_INFO_T pPhyInfo  = (PODM_PHY_INFO_T)(&pattrib->phy_info);
	struct rx_raw_rssi *psample_pkt_rssi = &padapter->recvpriv.raw_rssi_info;
	
	psample_pkt_rssi->data_rate = pattrib->data_rate;
	isCCKrate = (pattrib->data_rate <= DESC_RATE11M)?TRUE :FALSE;
	
	psample_pkt_rssi->pwdball = pPhyInfo->RxPWDBAll;
	psample_pkt_rssi->pwr_all = pPhyInfo->RecvSignalPower;

	for(rf_path = 0;rf_path<pHalData->NumTotalRFPath;rf_path++)
	{		
		psample_pkt_rssi->mimo_signal_strength[rf_path] = pPhyInfo->RxMIMOSignalStrength[rf_path];
		psample_pkt_rssi->mimo_signal_quality[rf_path] = pPhyInfo->RxMIMOSignalQuality[rf_path];
		if(!isCCKrate){
			psample_pkt_rssi->ofdm_pwr[rf_path] = pPhyInfo->RxPwr[rf_path];
			psample_pkt_rssi->ofdm_snr[rf_path] = pPhyInfo->RxSNR[rf_path];		
		}
	}
}
#endif

int check_phy_efuse_tx_power_info_valid(PADAPTER padapter) {
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);
	u8* pContent = pHalData->efuse_eeprom_data;
	int index = 0;
	u16 tx_index_offset = 0x0000;

	switch (rtw_get_chip_type(padapter)) {
		case RTL8723B:
			tx_index_offset = EEPROM_TX_PWR_INX_8723B;
		break;
		case RTL8703B:
			tx_index_offset = EEPROM_TX_PWR_INX_8703B;
		break;
		case RTL8188E:
			tx_index_offset = EEPROM_TX_PWR_INX_88E;
		break;
		case RTL8188F:
			tx_index_offset = EEPROM_TX_PWR_INX_8188F;
		break;
		case RTL8192E:
			tx_index_offset = EEPROM_TX_PWR_INX_8192E;
		break;
		case RTL8821:
			tx_index_offset = EEPROM_TX_PWR_INX_8821;
		break;
		case RTL8812:
			tx_index_offset = EEPROM_TX_PWR_INX_8812;
		break;
		case RTL8814A:
			tx_index_offset = EEPROM_TX_PWR_INX_8814;
		break;
		default:
			tx_index_offset = 0x0010;
		break;
	}

	/* TODO: chacking length by ICs */
	for (index = 0 ; index < 11 ; index++) {
		if (pContent[tx_index_offset + index] == 0xFF)
			return _FALSE;
	}
	return _TRUE;
}

int hal_efuse_macaddr_offset(_adapter *adapter)
{
	u8 interface_type = 0;
	int addr_offset = -1;

	interface_type = rtw_get_intf_type(adapter);

	switch (rtw_get_chip_type(adapter)) {
#ifdef CONFIG_RTL8723B
	case RTL8723B:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8723BU;
		else if (interface_type == RTW_SDIO)
			addr_offset = EEPROM_MAC_ADDR_8723BS;
		else if (interface_type == RTW_PCIE)
			addr_offset = EEPROM_MAC_ADDR_8723BE;
		break;
#endif
#ifdef CONFIG_RTL8703B
	case RTL8703B:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8703BU;
		else if (interface_type == RTW_SDIO)
			addr_offset = EEPROM_MAC_ADDR_8703BS;
	break;
#endif
#ifdef CONFIG_RTL8188E
	case RTL8188E:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_88EU;
		else if (interface_type == RTW_SDIO)
			addr_offset = EEPROM_MAC_ADDR_88ES;
		else if (interface_type == RTW_PCIE)
			addr_offset = EEPROM_MAC_ADDR_88EE;
		break;
#endif
#ifdef CONFIG_RTL8188F
	case RTL8188F:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8188FU;
		else if (interface_type == RTW_SDIO)
			addr_offset = EEPROM_MAC_ADDR_8188FS;
		break;
#endif
#ifdef CONFIG_RTL8812A
	case RTL8812:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8812AU;
		else if (interface_type == RTW_PCIE)
			addr_offset = EEPROM_MAC_ADDR_8812AE;
		break;
#endif
#ifdef CONFIG_RTL8821A
	case RTL8821:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8821AU;
		else if (interface_type == RTW_SDIO)
			addr_offset = EEPROM_MAC_ADDR_8821AS;
		else if (interface_type == RTW_PCIE)
			addr_offset = EEPROM_MAC_ADDR_8821AE;
		break;
#endif
#ifdef CONFIG_RTL8192E
	case RTL8192E:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8192EU;
		else if (interface_type == RTW_SDIO)
			addr_offset = EEPROM_MAC_ADDR_8192ES;
		else if (interface_type == RTW_PCIE)
			addr_offset = EEPROM_MAC_ADDR_8192EE;
		break;
#endif
#ifdef CONFIG_RTL8814A
	case RTL8814A:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8814AU;
		else if (interface_type == RTW_PCIE)
			addr_offset = EEPROM_MAC_ADDR_8814AE;
		break;
#endif
	}

	if (addr_offset == -1) {
		DBG_871X_LEVEL(_drv_err_, "%s: unknown combination - chip_type:%u, interface:%u\n"
			, __func__, rtw_get_chip_type(adapter), rtw_get_intf_type(adapter));
	}

	return addr_offset;
}

int Hal_GetPhyEfuseMACAddr(PADAPTER padapter, u8 *mac_addr)
{
	int ret = _FAIL;
	int addr_offset;

	addr_offset = hal_efuse_macaddr_offset(padapter);
	if (addr_offset == -1)
		goto exit;

	ret = rtw_efuse_map_read(padapter, addr_offset, ETH_ALEN, mac_addr);

exit:
	return ret;
}

#ifdef CONFIG_EFUSE_CONFIG_FILE
u32 Hal_readPGDataFromConfigFile(PADAPTER padapter)
{
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(padapter);
	u32 ret;

	ret = rtw_read_efuse_from_file(EFUSE_MAP_PATH, hal_data->efuse_eeprom_data);
	hal_data->efuse_file_status = ((ret == _FAIL) ? EFUSE_FILE_FAILED : EFUSE_FILE_LOADED);

	return ret;
}

u32 Hal_ReadMACAddrFromFile(PADAPTER padapter, u8 *mac_addr)
{
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(padapter);
	u32 ret = _FAIL;

	if (rtw_read_macaddr_from_file(WIFIMAC_PATH, mac_addr) == _SUCCESS
		&& rtw_check_invalid_mac_address(mac_addr, _TRUE) == _FALSE
	) {
		hal_data->macaddr_file_status = MACADDR_FILE_LOADED;
		ret = _SUCCESS;
	} else {
		hal_data->macaddr_file_status = MACADDR_FILE_FAILED;
	}

	return ret;
}
#endif /* CONFIG_EFUSE_CONFIG_FILE */

int hal_config_macaddr(_adapter *adapter, bool autoload_fail)
{
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	u8 addr[ETH_ALEN];
	int addr_offset = hal_efuse_macaddr_offset(adapter);
	u8 *hw_addr = NULL;
	int ret = _SUCCESS;

	if (autoload_fail)
		goto bypass_hw_pg;

	if (addr_offset != -1)
		hw_addr = &hal_data->efuse_eeprom_data[addr_offset];

#ifdef CONFIG_EFUSE_CONFIG_FILE
	/* if the hw_addr is written by efuse file, set to NULL */
	if (hal_data->efuse_file_status == EFUSE_FILE_LOADED)
		hw_addr = NULL;
#endif

	if (!hw_addr) {
		/* try getting hw pg data */
		if (Hal_GetPhyEfuseMACAddr(adapter, addr) == _SUCCESS)
			hw_addr = addr;
	}

	/* check hw pg data */
	if (hw_addr && rtw_check_invalid_mac_address(hw_addr, _TRUE) == _FALSE) {
		_rtw_memcpy(hal_data->EEPROMMACAddr, hw_addr, ETH_ALEN);
		goto exit;
	}
	
bypass_hw_pg:

#ifdef CONFIG_EFUSE_CONFIG_FILE
	/* check wifi mac file */
	if (Hal_ReadMACAddrFromFile(adapter, addr) == _SUCCESS) {
		_rtw_memcpy(hal_data->EEPROMMACAddr, addr, ETH_ALEN);
		goto exit;
	}
#endif

	_rtw_memset(hal_data->EEPROMMACAddr, 0, ETH_ALEN);
	ret = _FAIL;

exit:
	return ret;
}

#ifdef CONFIG_RF_POWER_TRIM
u32 Array_kfreemap[] = { 
0x08,0xe,
0x06,0xc,
0x04,0xa,
0x02,0x8,
0x00,0x6,
0x03,0x4,
0x05,0x2,
0x07,0x0,
0x09,0x0,
0x0c,0x0,
};

void rtw_bb_rf_gain_offset(_adapter *padapter)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct registry_priv  *registry_par = &padapter->registrypriv;
	struct kfree_data_t *kfree_data = &pHalData->kfree_data;
	u8		value = pHalData->EEPROMRFGainOffset;
	u8		tmp = 0x3e;
	u32		res, i = 0;
	u4Byte		ArrayLen	= sizeof(Array_kfreemap)/sizeof(u32);
	pu4Byte		Array	= Array_kfreemap;
	u4Byte		v1 = 0, v2 = 0, GainValue = 0, target = 0;

	if (registry_par->RegPwrTrimEnable == 2) {
		DBG_871X("Registry kfree default force disable.\n");
		return;
	}

#if defined(CONFIG_RTL8723B)
	if (value & BIT4 || (registry_par->RegPwrTrimEnable == 1)) {
		DBG_871X("Offset RF Gain.\n");
		DBG_871X("Offset RF Gain.  pHalData->EEPROMRFGainVal=0x%x\n",pHalData->EEPROMRFGainVal);
		
		if(pHalData->EEPROMRFGainVal != 0xff){

			if(pHalData->ant_path == ODM_RF_PATH_A) {
				GainValue=(pHalData->EEPROMRFGainVal & 0x0f);
				
			} else {
				GainValue=(pHalData->EEPROMRFGainVal & 0xf0)>>4;
			}
			DBG_871X("Ant PATH_%d GainValue Offset = 0x%x\n",(pHalData->ant_path == ODM_RF_PATH_A) ? (ODM_RF_PATH_A) : (ODM_RF_PATH_B),GainValue);
			
			for (i = 0; i < ArrayLen; i += 2 )
			{
				//DBG_871X("ArrayLen in =%d ,Array 1 =0x%x ,Array2 =0x%x \n",i,Array[i],Array[i]+1);
				v1 = Array[i];
				v2 = Array[i+1];
				 if ( v1 == GainValue ) {
						DBG_871X("Offset RF Gain. got v1 =0x%x ,v2 =0x%x \n",v1,v2);
						target=v2;
						break;
				 }
			}	 
			DBG_871X("pHalData->EEPROMRFGainVal=0x%x ,Gain offset Target Value=0x%x\n",pHalData->EEPROMRFGainVal,target);

			res = rtw_hal_read_rfreg(padapter, RF_PATH_A, 0x7f, 0xffffffff);
			DBG_871X("Offset RF Gain. before reg 0x7f=0x%08x\n",res);
			PHY_SetRFReg(padapter, RF_PATH_A, REG_RF_BB_GAIN_OFFSET, BIT18|BIT17|BIT16|BIT15, target);
			res = rtw_hal_read_rfreg(padapter, RF_PATH_A, 0x7f, 0xffffffff);

			DBG_871X("Offset RF Gain. After reg 0x7f=0x%08x\n",res);
			
		}else {

			DBG_871X("Offset RF Gain.  pHalData->EEPROMRFGainVal=0x%x	!= 0xff, didn't run Kfree\n",pHalData->EEPROMRFGainVal);
		}
	} else {
		DBG_871X("Using the default RF gain.\n");
	}

#elif defined(CONFIG_RTL8188E)
	if (value & BIT4 || (registry_par->RegPwrTrimEnable == 1)) {
		DBG_871X("8188ES Offset RF Gain.\n");
		DBG_871X("8188ES Offset RF Gain. EEPROMRFGainVal=0x%x\n",
				pHalData->EEPROMRFGainVal);

		if (pHalData->EEPROMRFGainVal != 0xff) {
			res = rtw_hal_read_rfreg(padapter, RF_PATH_A,
					REG_RF_BB_GAIN_OFFSET, 0xffffffff);

			DBG_871X("Offset RF Gain. reg 0x55=0x%x\n",res);
			res &= 0xfff87fff;

			res |= (pHalData->EEPROMRFGainVal & 0x0f) << 15;
			DBG_871X("Offset RF Gain. res=0x%x\n",res);

			rtw_hal_write_rfreg(padapter, RF_PATH_A,
					REG_RF_BB_GAIN_OFFSET,
					RF_GAIN_OFFSET_MASK, res);
		} else {
			DBG_871X("Offset RF Gain. EEPROMRFGainVal=0x%x == 0xff, didn't run Kfree\n",
					pHalData->EEPROMRFGainVal);
		}
	} else {
		DBG_871X("Using the default RF gain.\n");
	}
#else
	/* TODO: call this when channel switch */
	if (kfree_data->flag & KFREE_FLAG_ON)
		rtw_rf_apply_tx_gain_offset(padapter, 6); /* input ch6 to select BB_GAIN_2G */
#endif
	
}
#endif /*CONFIG_RF_POWER_TRIM */

bool kfree_data_is_bb_gain_empty(struct kfree_data_t *data)
{
#ifdef CONFIG_RF_POWER_TRIM
	int i, j;

	for (i = 0; i < BB_GAIN_NUM; i++)
		for (j = 0; j < RF_PATH_MAX; j++)
			if (data->bb_gain[i][j] != 0)
				return 0;
#endif
	return 1;
}

#ifdef CONFIG_USB_RX_AGGREGATION
void rtw_set_usb_agg_by_mode_normal(_adapter *padapter, u8 cur_wireless_mode)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	if(cur_wireless_mode < WIRELESS_11_24N 
		&& cur_wireless_mode > 0) //ABG mode
	{
#ifdef CONFIG_PREALLOC_RX_SKB_BUFFER
		u32 remainder = 0;
		u8 quotient = 0;

		remainder = MAX_RECVBUF_SZ % (4*1024); 
		quotient = (u8)(MAX_RECVBUF_SZ >> 12); 
		
		if (quotient > 5) {
			pHalData->RegAcUsbDmaSize = 0x6;
			pHalData->RegAcUsbDmaTime = 0x10;
		} else {
			if (remainder >= 2048) {
				pHalData->RegAcUsbDmaSize = quotient;
				pHalData->RegAcUsbDmaTime = 0x10;
			} else {
				pHalData->RegAcUsbDmaSize = (quotient-1);
				pHalData->RegAcUsbDmaTime = 0x10;
			}
		}
#else /* !CONFIG_PREALLOC_RX_SKB_BUFFER */
		if(0x6 != pHalData->RegAcUsbDmaSize || 0x10 !=pHalData->RegAcUsbDmaTime)
		{
			pHalData->RegAcUsbDmaSize = 0x6;
			pHalData->RegAcUsbDmaTime = 0x10;
			rtw_write16(padapter, REG_RXDMA_AGG_PG_TH,
				pHalData->RegAcUsbDmaSize | (pHalData->RegAcUsbDmaTime<<8));
		}
#endif /* CONFIG_PREALLOC_RX_SKB_BUFFER */
					
	}
	else if(cur_wireless_mode >= WIRELESS_11_24N
			&& cur_wireless_mode <= WIRELESS_MODE_MAX)//N AC mode
	{
#ifdef CONFIG_PREALLOC_RX_SKB_BUFFER
		u32 remainder = 0;
		u8 quotient = 0;

		remainder = MAX_RECVBUF_SZ % (4*1024); 
		quotient = (u8)(MAX_RECVBUF_SZ >> 12); 
		
		if (quotient > 5) {
			pHalData->RegAcUsbDmaSize = 0x5;
			pHalData->RegAcUsbDmaTime = 0x20;
		} else {
			if (remainder >= 2048) {
				pHalData->RegAcUsbDmaSize = quotient;
				pHalData->RegAcUsbDmaTime = 0x10;
			} else {
				pHalData->RegAcUsbDmaSize = (quotient-1);
				pHalData->RegAcUsbDmaTime = 0x10;
			}
		}
#else /* !CONFIG_PREALLOC_RX_SKB_BUFFER */
		if(0x5 != pHalData->RegAcUsbDmaSize || 0x20 !=pHalData->RegAcUsbDmaTime)
		{
			pHalData->RegAcUsbDmaSize = 0x5;
			pHalData->RegAcUsbDmaTime = 0x20;
			rtw_write16(padapter, REG_RXDMA_AGG_PG_TH,
				pHalData->RegAcUsbDmaSize | (pHalData->RegAcUsbDmaTime<<8));
		}
#endif /* CONFIG_PREALLOC_RX_SKB_BUFFER */

	}
	else
	{
		/* DBG_871X("%s: Unknow wireless mode(0x%x)\n",__func__,padapter->mlmeextpriv.cur_wireless_mode); */
	}
}

void rtw_set_usb_agg_by_mode_customer(_adapter *padapter, u8 cur_wireless_mode, u8 UsbDmaSize, u8 Legacy_UsbDmaSize)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	if (cur_wireless_mode < WIRELESS_11_24N
		&& cur_wireless_mode > 0) { /* ABG mode */
		if (Legacy_UsbDmaSize != pHalData->RegAcUsbDmaSize
			|| 0x10 != pHalData->RegAcUsbDmaTime) {
			pHalData->RegAcUsbDmaSize = Legacy_UsbDmaSize;
			pHalData->RegAcUsbDmaTime = 0x10;
			rtw_write16(padapter, REG_RXDMA_AGG_PG_TH,
				pHalData->RegAcUsbDmaSize | (pHalData->RegAcUsbDmaTime<<8));
		}
	} else if (cur_wireless_mode >= WIRELESS_11_24N
				&& cur_wireless_mode <= WIRELESS_MODE_MAX) { /* N AC mode */
		if (UsbDmaSize != pHalData->RegAcUsbDmaSize
			|| 0x20 != pHalData->RegAcUsbDmaTime) {
			pHalData->RegAcUsbDmaSize = UsbDmaSize;
			pHalData->RegAcUsbDmaTime = 0x20;
			rtw_write16(padapter, REG_RXDMA_AGG_PG_TH,
				pHalData->RegAcUsbDmaSize | (pHalData->RegAcUsbDmaTime<<8));
		}
	} else {
		/* DBG_871X("%s: Unknown wireless mode(0x%x)\n",__func__,padapter->mlmeextpriv.cur_wireless_mode); */
	}
}

void rtw_set_usb_agg_by_mode(_adapter *padapter, u8 cur_wireless_mode)
{
#ifdef CONFIG_PLATFORM_NOVATEK_NT72668
	rtw_set_usb_agg_by_mode_customer(padapter, cur_wireless_mode, 0x3, 0x3);
	return;
#endif /* CONFIG_PLATFORM_NOVATEK_NT72668 */

	rtw_set_usb_agg_by_mode_normal(padapter, cur_wireless_mode);
}
#endif //CONFIG_USB_RX_AGGREGATION

//To avoid RX affect TX throughput
void dm_DynamicUsbTxAgg(_adapter *padapter, u8 from_timer)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct mlme_priv		*pmlmepriv = &(padapter->mlmepriv);
	struct mlme_ext_priv	*pmlmeextpriv = &(padapter->mlmeextpriv);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	u8 cur_wireless_mode = pmlmeextpriv->cur_wireless_mode;
#ifdef CONFIG_CONCURRENT_MODE
	struct mlme_ext_priv	*pbuddymlmeextpriv = &(padapter->pbuddy_adapter->mlmeextpriv);
#endif //CONFIG_CONCURRENT_MODE

#ifdef CONFIG_USB_RX_AGGREGATION	
	if(IS_HARDWARE_TYPE_8821U(padapter) )//|| IS_HARDWARE_TYPE_8192EU(padapter))
	{
		//This AGG_PH_TH only for UsbRxAggMode == USB_RX_AGG_USB
		if((pHalData->UsbRxAggMode == USB_RX_AGG_USB) && (check_fwstate(pmlmepriv, _FW_LINKED)== _TRUE))
		{
			if(pdvobjpriv->traffic_stat.cur_tx_tp > 2 && pdvobjpriv->traffic_stat.cur_rx_tp < 30)
				rtw_write16(padapter , REG_RXDMA_AGG_PG_TH , 0x1010);
			else if (pdvobjpriv->traffic_stat.last_tx_bytes > 220000 && pdvobjpriv->traffic_stat.cur_rx_tp < 30)
				rtw_write16(padapter , REG_RXDMA_AGG_PG_TH , 0x1006);			
			else
				rtw_write16(padapter, REG_RXDMA_AGG_PG_TH,0x2005); //dmc agg th 20K
			
			//DBG_871X("TX_TP=%u, RX_TP=%u \n", pdvobjpriv->traffic_stat.cur_tx_tp, pdvobjpriv->traffic_stat.cur_rx_tp);
		}
	}
	else if(IS_HARDWARE_TYPE_8812(padapter))
	{
#ifdef CONFIG_CONCURRENT_MODE
		if(rtw_linked_check(padapter) == _TRUE && rtw_linked_check(padapter->pbuddy_adapter) == _TRUE)
		{
			if(pbuddymlmeextpriv->cur_wireless_mode >= pmlmeextpriv->cur_wireless_mode)
				cur_wireless_mode = pbuddymlmeextpriv->cur_wireless_mode;
			else
				cur_wireless_mode = pmlmeextpriv->cur_wireless_mode;

			rtw_set_usb_agg_by_mode(padapter,cur_wireless_mode);
		}
		else if (rtw_linked_check(padapter) == _TRUE && rtw_linked_check(padapter->pbuddy_adapter) == _FALSE)
		{
			rtw_set_usb_agg_by_mode(padapter,cur_wireless_mode);
		}
#else //!CONFIG_CONCURRENT_MODE
		rtw_set_usb_agg_by_mode(padapter,cur_wireless_mode);
#endif //CONFIG_CONCURRENT_MODE
#ifdef CONFIG_PLATFORM_NOVATEK_NT72668
	} else {
		rtw_set_usb_agg_by_mode(padapter, cur_wireless_mode);
#endif /* CONFIG_PLATFORM_NOVATEK_NT72668 */
	}
#endif
}

//bus-agg check for SoftAP mode
inline u8 rtw_hal_busagg_qsel_check(_adapter *padapter,u8 pre_qsel,u8 next_qsel)
{
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	u8 chk_rst = _SUCCESS;
	
	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return chk_rst;

	//if((pre_qsel == 0xFF)||(next_qsel== 0xFF)) 
	//	return chk_rst;
	
	if(	((pre_qsel == QSLT_HIGH)||((next_qsel== QSLT_HIGH))) 
			&& (pre_qsel != next_qsel )){
			//DBG_871X("### bus-agg break cause of qsel misatch, pre_qsel=0x%02x,next_qsel=0x%02x ###\n",
			//	pre_qsel,next_qsel);
			chk_rst = _FAIL;
		}
	return chk_rst;
}

/*
 * Description:
 * dump_TX_FIFO: This is only used to dump TX_FIFO for debug WoW mode offload
 * contant.
 *
 * Input:
 * adapter: adapter pointer.
 * page_num: The max. page number that user want to dump. 
 * page_size: page size of each page. eg. 128 bytes, 256 bytes, 512byte.
 */
void dump_TX_FIFO(_adapter* padapter, u8 page_num, u16 page_size){

	int i;
	u8 val = 0;
	u8 base = 0;
	u32 addr = 0;
	u32 count = (page_size / 8);

	if (page_num <= 0) {
		DBG_871X("!!%s: incorrect input page_num paramter!\n", __func__);
		return;
	}

	if (page_size < 128 || page_size > 512) {
		DBG_871X("!!%s: incorrect input page_size paramter!\n", __func__);
		return;
	}

	DBG_871X("+%s+\n", __func__);
	val = rtw_read8(padapter, 0x106);
	rtw_write8(padapter, 0x106, 0x69);
	DBG_871X("0x106: 0x%02x\n", val);
	base = rtw_read8(padapter, 0x209);
	DBG_871X("0x209: 0x%02x\n", base);

	addr = ((base) * page_size)/8;
	for (i = 0 ; i < page_num * count ; i+=2) {
		rtw_write32(padapter, 0x140, addr + i);
		printk(" %08x %08x ", rtw_read32(padapter, 0x144), rtw_read32(padapter, 0x148));
		rtw_write32(padapter, 0x140, addr + i + 1);
		printk(" %08x %08x \n", rtw_read32(padapter, 0x144), rtw_read32(padapter, 0x148));
	}
}

#ifdef CONFIG_GPIO_API
u8 rtw_hal_get_gpio(_adapter* adapter, u8 gpio_num)
{
	u8 value;
	u8 direction;	
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapter);

	rtw_ps_deny(adapter, PS_DENY_IOCTL);

	DBG_871X("rf_pwrstate=0x%02x\n", pwrpriv->rf_pwrstate);
	LeaveAllPowerSaveModeDirect(adapter);

	/* Read GPIO Direction */
	direction = (rtw_read8(adapter,REG_GPIO_PIN_CTRL + 2) & BIT(gpio_num)) >> gpio_num;

	/* According the direction to read register value */
	if( direction )
		value =  (rtw_read8(adapter, REG_GPIO_PIN_CTRL + 1)& BIT(gpio_num)) >> gpio_num;
	else
		value =  (rtw_read8(adapter, REG_GPIO_PIN_CTRL)& BIT(gpio_num)) >> gpio_num;

	rtw_ps_deny_cancel(adapter, PS_DENY_IOCTL);
	DBG_871X("%s direction=%d value=%d\n",__FUNCTION__,direction,value);

	return value;
}

int  rtw_hal_set_gpio_output_value(_adapter* adapter, u8 gpio_num, bool isHigh)
{
	u8 direction = 0;
	u8 res = -1;
	if (IS_HARDWARE_TYPE_8188E(adapter)){
		/* Check GPIO is 4~7 */
		if( gpio_num > 7 || gpio_num < 4)
		{
			DBG_871X("%s The gpio number does not included 4~7.\n",__FUNCTION__);
			return -1;
		}
	}	
	
	rtw_ps_deny(adapter, PS_DENY_IOCTL);

	LeaveAllPowerSaveModeDirect(adapter);

	/* Read GPIO direction */
	direction = (rtw_read8(adapter,REG_GPIO_PIN_CTRL + 2) & BIT(gpio_num)) >> gpio_num;

	/* If GPIO is output direction, setting value. */
	if( direction )
	{
		if(isHigh)
			rtw_write8(adapter, REG_GPIO_PIN_CTRL + 1, rtw_read8(adapter, REG_GPIO_PIN_CTRL + 1) | BIT(gpio_num));
		else
			rtw_write8(adapter, REG_GPIO_PIN_CTRL + 1, rtw_read8(adapter, REG_GPIO_PIN_CTRL + 1) & ~BIT(gpio_num));

		DBG_871X("%s Set gpio %x[%d]=%d\n",__FUNCTION__,REG_GPIO_PIN_CTRL+1,gpio_num,isHigh );
		res = 0;
	}
	else
	{
		DBG_871X("%s The gpio is input,not be set!\n",__FUNCTION__);
		res = -1;
	}

	rtw_ps_deny_cancel(adapter, PS_DENY_IOCTL);
	return res;
}

int rtw_hal_config_gpio(_adapter* adapter, u8 gpio_num, bool isOutput)
{
	if (IS_HARDWARE_TYPE_8188E(adapter)){
		if( gpio_num > 7 || gpio_num < 4)
		{
			DBG_871X("%s The gpio number does not included 4~7.\n",__FUNCTION__);
			return -1;
		}
	}	

	DBG_871X("%s gpio_num =%d direction=%d\n",__FUNCTION__,gpio_num,isOutput);

	rtw_ps_deny(adapter, PS_DENY_IOCTL);

	LeaveAllPowerSaveModeDirect(adapter);

	if( isOutput )
	{
		rtw_write8(adapter, REG_GPIO_PIN_CTRL + 2, rtw_read8(adapter, REG_GPIO_PIN_CTRL + 2) | BIT(gpio_num));
	}
	else
	{
		rtw_write8(adapter, REG_GPIO_PIN_CTRL + 2, rtw_read8(adapter, REG_GPIO_PIN_CTRL + 2) & ~BIT(gpio_num));
	}

	rtw_ps_deny_cancel(adapter, PS_DENY_IOCTL);

	return 0;
}
int rtw_hal_register_gpio_interrupt(_adapter* adapter, int gpio_num, void(*callback)(u8 level))
{
	u8 value;
	u8 direction;
	PHAL_DATA_TYPE phal = GET_HAL_DATA(adapter);

	if (IS_HARDWARE_TYPE_8188E(adapter)){	
		if(gpio_num > 7 || gpio_num < 4)
		{
			DBG_871X_LEVEL(_drv_always_, "%s The gpio number does not included 4~7.\n",__FUNCTION__);
			return -1;
		}
	}

	rtw_ps_deny(adapter, PS_DENY_IOCTL);

	LeaveAllPowerSaveModeDirect(adapter);

	/* Read GPIO direction */
	direction = (rtw_read8(adapter,REG_GPIO_PIN_CTRL + 2) & BIT(gpio_num)) >> gpio_num;
	if(direction){
		DBG_871X_LEVEL(_drv_always_, "%s Can't register output gpio as interrupt.\n",__FUNCTION__);
		return -1;
	}

	/* Config GPIO Mode */
	rtw_write8(adapter, REG_GPIO_PIN_CTRL + 3, rtw_read8(adapter, REG_GPIO_PIN_CTRL + 3) | BIT(gpio_num));	

	/* Register GPIO interrupt handler*/
	adapter->gpiointpriv.callback[gpio_num] = callback;
	
	/* Set GPIO interrupt mode, 0:positive edge, 1:negative edge */
	value = rtw_read8(adapter, REG_GPIO_PIN_CTRL) & BIT(gpio_num);
	adapter->gpiointpriv.interrupt_mode = rtw_read8(adapter, REG_HSIMR + 2)^value;
	rtw_write8(adapter, REG_GPIO_INTM, adapter->gpiointpriv.interrupt_mode);
	
	/* Enable GPIO interrupt */
	adapter->gpiointpriv.interrupt_enable_mask = rtw_read8(adapter, REG_HSIMR + 2) | BIT(gpio_num);
	rtw_write8(adapter, REG_HSIMR + 2, adapter->gpiointpriv.interrupt_enable_mask);

	rtw_hal_update_hisr_hsisr_ind(adapter, 1);
	
	rtw_ps_deny_cancel(adapter, PS_DENY_IOCTL);

	return 0;
}
int rtw_hal_disable_gpio_interrupt(_adapter* adapter, int gpio_num)
{
	u8 value;
	u8 direction;
	PHAL_DATA_TYPE phal = GET_HAL_DATA(adapter);

	if (IS_HARDWARE_TYPE_8188E(adapter)){
		if(gpio_num > 7 || gpio_num < 4)
		{
			DBG_871X("%s The gpio number does not included 4~7.\n",__FUNCTION__);
			return -1;
		}
	}

	rtw_ps_deny(adapter, PS_DENY_IOCTL);

	LeaveAllPowerSaveModeDirect(adapter);

	/* Config GPIO Mode */
	rtw_write8(adapter, REG_GPIO_PIN_CTRL + 3, rtw_read8(adapter, REG_GPIO_PIN_CTRL + 3) &~ BIT(gpio_num));	

	/* Unregister GPIO interrupt handler*/
	adapter->gpiointpriv.callback[gpio_num] = NULL;

	/* Reset GPIO interrupt mode, 0:positive edge, 1:negative edge */
	adapter->gpiointpriv.interrupt_mode = rtw_read8(adapter, REG_GPIO_INTM) &~ BIT(gpio_num);
	rtw_write8(adapter, REG_GPIO_INTM, 0x00);
	
	/* Disable GPIO interrupt */
	adapter->gpiointpriv.interrupt_enable_mask = rtw_read8(adapter, REG_HSIMR + 2) &~ BIT(gpio_num);
	rtw_write8(adapter, REG_HSIMR + 2, adapter->gpiointpriv.interrupt_enable_mask);

	if(!adapter->gpiointpriv.interrupt_enable_mask)
		rtw_hal_update_hisr_hsisr_ind(adapter, 0);
	
	rtw_ps_deny_cancel(adapter, PS_DENY_IOCTL);

	return 0;
}
#endif

s8 rtw_hal_ch_sw_iqk_info_search(_adapter* padapter, u8 central_chnl, u8 bw_mode) {
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u8 i;

	for (i = 0; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++) {
		if ((pHalData->iqk_reg_backup[i].central_chnl != 0)) {
			if ((pHalData->iqk_reg_backup[i].central_chnl == central_chnl)
				&& (pHalData->iqk_reg_backup[i].bw_mode == bw_mode)) {
				return i;
			}	
		}
	}

	return -1;
}

void rtw_hal_ch_sw_iqk_info_backup(_adapter* padapter) {
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	s8 res;
	u8 i;

	/* If it's an existed record, overwrite it */
	res = rtw_hal_ch_sw_iqk_info_search(padapter, pHalData->CurrentChannel, pHalData->CurrentChannelBW);
	if ((res >= 0) && (res < MAX_IQK_INFO_BACKUP_CHNL_NUM)) {
		rtw_hal_set_hwreg(padapter, HW_VAR_CH_SW_IQK_INFO_BACKUP, (u8*)&(pHalData->iqk_reg_backup[res]));
		return;
	}		

	/* Search for the empty record to use */
	for (i = 0; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++) {
		if (pHalData->iqk_reg_backup[i].central_chnl == 0) {
			rtw_hal_set_hwreg(padapter, HW_VAR_CH_SW_IQK_INFO_BACKUP, (u8*)&(pHalData->iqk_reg_backup[i]));
			return;
		}	
	}

	/* Else, overwrite the oldest record */
	for (i = 1; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++) {
		_rtw_memcpy(&(pHalData->iqk_reg_backup[i - 1]), &(pHalData->iqk_reg_backup[i]), sizeof(struct hal_iqk_reg_backup));
	}
	rtw_hal_set_hwreg(padapter, HW_VAR_CH_SW_IQK_INFO_BACKUP, (u8*)&(pHalData->iqk_reg_backup[MAX_IQK_INFO_BACKUP_CHNL_NUM - 1]));
}

void rtw_hal_ch_sw_iqk_info_restore(_adapter* padapter, u8 ch_sw_use_case) {
	rtw_hal_set_hwreg(padapter, HW_VAR_CH_SW_IQK_INFO_RESTORE, &ch_sw_use_case);
}

void rtw_dump_mac_rx_counters(_adapter* padapter,struct dbg_rx_counter *rx_counter)
{
	u32	mac_cck_ok=0, mac_ofdm_ok=0, mac_ht_ok=0, mac_vht_ok=0;
	u32	mac_cck_err=0, mac_ofdm_err=0, mac_ht_err=0, mac_vht_err=0;
	u32	mac_cck_fa=0, mac_ofdm_fa=0, mac_ht_fa=0;
	u32	DropPacket=0;
	
	if(!rx_counter){
		rtw_warn_on(1);
		return;
	}

	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x3);
	mac_cck_ok	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	  
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x0);
	mac_ofdm_ok	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	 
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x6);
	mac_ht_ok	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	
	mac_vht_ok	= 0;	
	if (IS_HARDWARE_TYPE_JAGUAR(padapter) || IS_HARDWARE_TYPE_JAGUAR2(padapter)) {
		PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x0);
		PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT26, 0x1);
		mac_vht_ok	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	 
	}	
		
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x4);
	mac_cck_err	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x1);
	mac_ofdm_err	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x7);
	mac_ht_err	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]		
	mac_vht_err	= 0;
	if (IS_HARDWARE_TYPE_JAGUAR(padapter) || IS_HARDWARE_TYPE_JAGUAR2(padapter)) {
		PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x1);
		PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT26, 0x1);
		mac_vht_err	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	 
	}

	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x5);
	mac_cck_fa	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x2);
	mac_ofdm_fa	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]	
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT28|BIT29|BIT30|BIT31, 0x9);
	mac_ht_fa	= PHY_QueryMacReg(padapter, REG_RXERR_RPT, bMaskLWord);// [15:0]		
	
	//Mac_DropPacket
	rtw_write32(padapter, REG_RXERR_RPT, (rtw_read32(padapter, REG_RXERR_RPT)& 0x0FFFFFFF)| Mac_DropPacket);
	DropPacket = rtw_read32(padapter, REG_RXERR_RPT)& 0x0000FFFF;

	rx_counter->rx_pkt_ok = mac_cck_ok+mac_ofdm_ok+mac_ht_ok+mac_vht_ok;
	rx_counter->rx_pkt_crc_error = mac_cck_err+mac_ofdm_err+mac_ht_err+mac_vht_err;
	rx_counter->rx_cck_fa = mac_cck_fa;
	rx_counter->rx_ofdm_fa = mac_ofdm_fa;
	rx_counter->rx_ht_fa = mac_ht_fa;
	rx_counter->rx_pkt_drop = DropPacket;
}
void rtw_reset_mac_rx_counters(_adapter* padapter)
{

	if (IS_HARDWARE_TYPE_8703B(padapter) || IS_HARDWARE_TYPE_8188F(padapter))
		PHY_SetMacReg(padapter, 0x608, BIT19, 0x1); /* If no packet rx, MaxRx clock be gating ,BIT_DISGCLK bit19 set 1 for fix*/	

	//reset mac counter
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT27, 0x1); 
	PHY_SetMacReg(padapter, REG_RXERR_RPT, BIT27, 0x0);
}

void rtw_dump_phy_rx_counters(_adapter* padapter,struct dbg_rx_counter *rx_counter)
{
	u32 cckok=0,cckcrc=0,ofdmok=0,ofdmcrc=0,htok=0,htcrc=0,OFDM_FA=0,CCK_FA=0,vht_ok=0,vht_err=0;
	if(!rx_counter){
		rtw_warn_on(1);
		return;
	}
	if (IS_HARDWARE_TYPE_JAGUAR(padapter) || IS_HARDWARE_TYPE_JAGUAR2(padapter)){
		cckok	= PHY_QueryBBReg(padapter, 0xF04, 0x3FFF);	     // [13:0] 
		ofdmok	= PHY_QueryBBReg(padapter, 0xF14, 0x3FFF);	     // [13:0] 
		htok		= PHY_QueryBBReg(padapter, 0xF10, 0x3FFF);     // [13:0]
		vht_ok	= PHY_QueryBBReg(padapter, 0xF0C, 0x3FFF);     // [13:0]
		cckcrc	= PHY_QueryBBReg(padapter, 0xF04, 0x3FFF0000); // [29:16]	
		ofdmcrc	= PHY_QueryBBReg(padapter, 0xF14, 0x3FFF0000); // [29:16]
		htcrc	= PHY_QueryBBReg(padapter, 0xF10, 0x3FFF0000); // [29:16]
		vht_err	= PHY_QueryBBReg(padapter, 0xF0C, 0x3FFF0000); // [29:16]
		CCK_FA	= PHY_QueryBBReg(padapter, 0xA5C, bMaskLWord);
		OFDM_FA	= PHY_QueryBBReg(padapter, 0xF48, bMaskLWord);
	} 
	else
	{
		cckok	= PHY_QueryBBReg(padapter, 0xF88, bMaskDWord);
		ofdmok	= PHY_QueryBBReg(padapter, 0xF94, bMaskLWord);
		htok		= PHY_QueryBBReg(padapter, 0xF90, bMaskLWord);
		vht_ok	= 0;
		cckcrc	= PHY_QueryBBReg(padapter, 0xF84, bMaskDWord);
		ofdmcrc	= PHY_QueryBBReg(padapter, 0xF94, bMaskHWord);
		htcrc	= PHY_QueryBBReg(padapter, 0xF90, bMaskHWord);
		vht_err	= 0;
		OFDM_FA = PHY_QueryBBReg(padapter, 0xCF0, bMaskLWord) + PHY_QueryBBReg(padapter, 0xCF2, bMaskLWord) +
			PHY_QueryBBReg(padapter, 0xDA2, bMaskLWord) + PHY_QueryBBReg(padapter, 0xDA4, bMaskLWord) +
			PHY_QueryBBReg(padapter, 0xDA6, bMaskLWord) + PHY_QueryBBReg(padapter, 0xDA8, bMaskLWord);
	
		CCK_FA=(rtw_read8(padapter, 0xA5B )<<8 ) | (rtw_read8(padapter, 0xA5C));
	}
	
	rx_counter->rx_pkt_ok = cckok+ofdmok+htok+vht_ok;
	rx_counter->rx_pkt_crc_error = cckcrc+ofdmcrc+htcrc+vht_err;
	rx_counter->rx_ofdm_fa = OFDM_FA;
	rx_counter->rx_cck_fa = CCK_FA;
	
}

void rtw_reset_phy_trx_ok_counters(_adapter *padapter)
{
	if (IS_HARDWARE_TYPE_JAGUAR(padapter) || IS_HARDWARE_TYPE_JAGUAR2(padapter)) {
		PHY_SetBBReg(padapter, 0xB58, BIT0, 0x1);
		PHY_SetBBReg(padapter, 0xB58, BIT0, 0x0);
	}
}
void rtw_reset_phy_rx_counters(_adapter *padapter)
{
	//reset phy counter
	if (IS_HARDWARE_TYPE_JAGUAR(padapter) || IS_HARDWARE_TYPE_JAGUAR2(padapter))
	{
		rtw_reset_phy_trx_ok_counters(padapter);

		PHY_SetBBReg(padapter, 0x9A4, BIT17, 0x1);//reset  OFDA FA counter
		PHY_SetBBReg(padapter, 0x9A4, BIT17, 0x0);
			
		PHY_SetBBReg(padapter, 0xA2C, BIT15, 0x0);//reset  CCK FA counter
		PHY_SetBBReg(padapter, 0xA2C, BIT15, 0x1);
	}
	else
	{
		PHY_SetBBReg(padapter, 0xF14, BIT16, 0x1);
		rtw_msleep_os(10);
		PHY_SetBBReg(padapter, 0xF14, BIT16, 0x0);
		
		PHY_SetBBReg(padapter, 0xD00, BIT27, 0x1);//reset  OFDA FA counter
		PHY_SetBBReg(padapter, 0xC0C, BIT31, 0x1);//reset  OFDA FA counter
		PHY_SetBBReg(padapter, 0xD00, BIT27, 0x0);
		PHY_SetBBReg(padapter, 0xC0C, BIT31, 0x0);
			
		PHY_SetBBReg(padapter, 0xA2C, BIT15, 0x0);//reset  CCK FA counter
		PHY_SetBBReg(padapter, 0xA2C, BIT15, 0x1);
	}
}
#ifdef DBG_RX_COUNTER_DUMP
void rtw_dump_drv_rx_counters(_adapter* padapter,struct dbg_rx_counter *rx_counter)
{
	struct recv_priv *precvpriv = &padapter->recvpriv;
	if(!rx_counter){
		rtw_warn_on(1);
		return;
	}
	rx_counter->rx_pkt_ok = padapter->drv_rx_cnt_ok;
	rx_counter->rx_pkt_crc_error = padapter->drv_rx_cnt_crcerror;
	rx_counter->rx_pkt_drop = precvpriv->rx_drop - padapter->drv_rx_cnt_drop;	
}
void rtw_reset_drv_rx_counters(_adapter* padapter)
{
	struct recv_priv *precvpriv = &padapter->recvpriv;
	padapter->drv_rx_cnt_ok = 0;
	padapter->drv_rx_cnt_crcerror = 0;
	padapter->drv_rx_cnt_drop = precvpriv->rx_drop;
}
void rtw_dump_phy_rxcnts_preprocess(_adapter* padapter,u8 rx_cnt_mode)
{
	u8 initialgain;
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(padapter);
	
	if((!(padapter->dump_rx_cnt_mode& DUMP_PHY_RX_COUNTER)) && (rx_cnt_mode & DUMP_PHY_RX_COUNTER))
	{
		/*initialgain = pDigTable->CurIGValue;*/
		rtw_hal_get_odm_var(padapter, HAL_ODM_INITIAL_GAIN, &initialgain, NULL);
		DBG_871X("%s CurIGValue:0x%02x\n",__FUNCTION__,initialgain);
		rtw_hal_set_odm_var(padapter, HAL_ODM_INITIAL_GAIN, &initialgain, _FALSE);
		/*disable dynamic functions, such as high power, DIG*/
		rtw_phydm_ability_backup(padapter);
		rtw_phydm_func_clr(padapter, (ODM_BB_DIG|ODM_BB_FA_CNT));
	}
	else if((padapter->dump_rx_cnt_mode& DUMP_PHY_RX_COUNTER) &&(!(rx_cnt_mode & DUMP_PHY_RX_COUNTER )))
	{
		//turn on phy-dynamic functions
		rtw_phydm_ability_restore(padapter);
		initialgain = 0xff; //restore RX GAIN
		rtw_hal_set_odm_var(padapter, HAL_ODM_INITIAL_GAIN, &initialgain, _FALSE);
		
	}
}
	
void rtw_dump_rx_counters(_adapter* padapter)
{
	struct dbg_rx_counter rx_counter;

	if( padapter->dump_rx_cnt_mode & DUMP_DRV_RX_COUNTER ){
		_rtw_memset(&rx_counter,0,sizeof(struct dbg_rx_counter));
		rtw_dump_drv_rx_counters(padapter,&rx_counter);
		DBG_871X( "Drv Received packet OK:%d CRC error:%d Drop Packets: %d\n",
					rx_counter.rx_pkt_ok,rx_counter.rx_pkt_crc_error,rx_counter.rx_pkt_drop);		
		rtw_reset_drv_rx_counters(padapter);		
	}
		
	if( padapter->dump_rx_cnt_mode & DUMP_MAC_RX_COUNTER ){
		_rtw_memset(&rx_counter,0,sizeof(struct dbg_rx_counter));
		rtw_dump_mac_rx_counters(padapter,&rx_counter);
		DBG_871X( "Mac Received packet OK:%d CRC error:%d FA Counter: %d Drop Packets: %d\n",
					rx_counter.rx_pkt_ok,rx_counter.rx_pkt_crc_error,
					rx_counter.rx_cck_fa+rx_counter.rx_ofdm_fa+rx_counter.rx_ht_fa,
					rx_counter.rx_pkt_drop);			
		rtw_reset_mac_rx_counters(padapter);
	}

	if(padapter->dump_rx_cnt_mode & DUMP_PHY_RX_COUNTER ){		
		_rtw_memset(&rx_counter,0,sizeof(struct dbg_rx_counter));		
		rtw_dump_phy_rx_counters(padapter,&rx_counter);
		//DBG_871X("%s: OFDM_FA =%d\n", __FUNCTION__, rx_counter.rx_ofdm_fa);
		//DBG_871X("%s: CCK_FA =%d\n", __FUNCTION__, rx_counter.rx_cck_fa);
		DBG_871X("Phy Received packet OK:%d CRC error:%d FA Counter: %d\n",rx_counter.rx_pkt_ok,rx_counter.rx_pkt_crc_error,
		rx_counter.rx_ofdm_fa+rx_counter.rx_cck_fa);
		rtw_reset_phy_rx_counters(padapter);	
	}
}
#endif
void rtw_get_noise(_adapter* padapter)
{
#if defined(CONFIG_SIGNAL_DISPLAY_DBM) && defined(CONFIG_BACKGROUND_NOISE_MONITOR)
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct noise_info info;
	if(rtw_linked_check(padapter)){
		info.bPauseDIG = _TRUE;
		info.IGIValue = 0x1e;
		info.max_time = 100;//ms
		info.chan = pmlmeext->cur_channel ;//rtw_get_oper_ch(padapter);
		rtw_ps_deny(padapter, PS_DENY_IOCTL);
		LeaveAllPowerSaveModeDirect(padapter);

		rtw_hal_set_odm_var(padapter, HAL_ODM_NOISE_MONITOR,&info, _FALSE);	
		//ODM_InbandNoise_Monitor(podmpriv,_TRUE,0x20,100);
		rtw_ps_deny_cancel(padapter, PS_DENY_IOCTL);
		rtw_hal_get_odm_var(padapter, HAL_ODM_NOISE_MONITOR,&(info.chan), &(padapter->recvpriv.noise));	
		#ifdef DBG_NOISE_MONITOR
		DBG_871X("chan:%d,noise_level:%d\n",info.chan,padapter->recvpriv.noise);
		#endif
	}
#endif		

}

u8 rtw_get_current_tx_rate(_adapter *padapter, u8 macid)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	PDM_ODM_T		pDM_Odm = &pHalData->odmpriv;
	pRA_T			pRA_Table = &pDM_Odm->DM_RA_Table;
	u8 rate_id = 0;

#if (RATE_ADAPTIVE_SUPPORT == 1)
	rate_id = ODM_RA_GetDecisionRate_8188E(pDM_Odm, macid);
#else
	rate_id = (pRA_Table->link_tx_rate[macid]) & 0x7f;
#endif

	return rate_id;

}

#ifdef CONFIG_FW_C2H_DEBUG

/*	C2H RX package original is 128.
if enable CONFIG_FW_C2H_DEBUG, it should increase to 256.
 C2H FW debug message:
 without aggregate:
 {C2H_CmdID,Seq,SubID,Len,Content[0~n]}
 Content[0~n]={'a','b','c',...,'z','\n'}
 with aggregate:
 {C2H_CmdID,Seq,SubID,Len,Content[0~n]}
 Content[0~n]={'a','b','c',...,'z','\n',Extend C2H pkt 2...}
 Extend C2H pkt 2={C2H CmdID,Seq,SubID,Len,Content = {'a','b','c',...,'z','\n'}}
 Author: Isaac	*/

void Debug_FwC2H(PADAPTER padapter, u8 *pdata, u8 len)
{
	int i = 0;
	int cnt = 0, total_length = 0;
	u8 buf[128] = {0};
	u8 more_data = _FALSE;
	u8 *nextdata = NULL;
	u8 test = 0;

	u8 data_len;
	u8 seq_no;

	nextdata = pdata;
	do {
		data_len = *(nextdata + 1);
		seq_no = *(nextdata + 2);

		for (i = 0 ; i < data_len - 2 ; i++) {
			cnt += sprintf((buf+cnt), "%c", nextdata[3 + i]);

			if (nextdata[3 + i] == 0x0a && nextdata[4 + i] == 0xff)
				more_data = _TRUE;
			else if (nextdata[3 + i] == 0x0a && nextdata[4 + i] != 0xff)
				more_data = _FALSE;
		}

		DBG_871X("[RTKFW, SEQ=%d]: %s", seq_no, buf);
		data_len += 3;
		total_length += data_len;

		if (more_data == _TRUE) {
			_rtw_memset(buf, '\0', 128);
			cnt = 0;
			nextdata = (pdata + total_length);
		}
	} while (more_data == _TRUE);
}
#endif /*CONFIG_FW_C2H_DEBUG*/
void update_IOT_info(_adapter *padapter)
{
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	
	switch (pmlmeinfo->assoc_AP_vendor)
	{
		case HT_IOT_PEER_MARVELL:
			pmlmeinfo->turboMode_cts2self = 1;
			pmlmeinfo->turboMode_rtsen = 0;
			break;
		
		case HT_IOT_PEER_RALINK:
			pmlmeinfo->turboMode_cts2self = 0;
			pmlmeinfo->turboMode_rtsen = 1;
			//disable high power			
			rtw_phydm_func_clr(padapter, ODM_BB_DYNAMIC_TXPWR);
			break;
		case HT_IOT_PEER_REALTEK:
			//rtw_write16(padapter, 0x4cc, 0xffff);
			//rtw_write16(padapter, 0x546, 0x01c0);
			//disable high power			
			rtw_phydm_func_clr(padapter, ODM_BB_DYNAMIC_TXPWR);
			break;
		default:
			pmlmeinfo->turboMode_cts2self = 0;
			pmlmeinfo->turboMode_rtsen = 1;
			break;	
	}
	
}
#ifdef CONFIG_AUTO_CHNL_SEL_NHM
void rtw_acs_start(_adapter *padapter, bool bStart)
{	
	if (_TRUE == bStart) {
		ACS_OP acs_op = ACS_INIT;
		
		rtw_hal_set_odm_var(padapter, HAL_ODM_AUTO_CHNL_SEL, &acs_op, _TRUE);
		rtw_set_acs_channel(padapter, 0);
		SET_ACS_STATE(padapter, ACS_ENABLE);		
	} else {		
		SET_ACS_STATE(padapter, ACS_DISABLE);
		#ifdef DBG_AUTO_CHNL_SEL_NHM
		if (1) {
			u8 best_24g_ch = 0;
			u8 best_5g_ch = 0;
			
			rtw_hal_get_odm_var(padapter, HAL_ODM_AUTO_CHNL_SEL, &(best_24g_ch), &(best_5g_ch));
			DBG_871X("[ACS-"ADPT_FMT"] Best 2.4G CH:%u\n", ADPT_ARG(padapter), best_24g_ch);
			DBG_871X("[ACS-"ADPT_FMT"] Best 5G CH:%u\n", ADPT_ARG(padapter), best_5g_ch);
		}
		#endif
	}
}
#endif

/* TODO: merge with phydm, see odm_SetCrystalCap() */
void hal_set_crystal_cap(_adapter *adapter, u8 crystal_cap)
{
	crystal_cap = crystal_cap & 0x3F;

	switch (rtw_get_chip_type(adapter)) {
#if defined(CONFIG_RTL8188E) || defined(CONFIG_RTL8188F)
	case RTL8188E:
	case RTL8188F:
		/* write 0x24[16:11] = 0x24[22:17] = CrystalCap */
		PHY_SetBBReg(adapter, REG_AFE_XTAL_CTRL, 0x007FF800, (crystal_cap | (crystal_cap << 6)));
		break;
#endif
#if defined(CONFIG_RTL8812A)
	case RTL8812:
		/* write 0x2C[30:25] = 0x2C[24:19] = CrystalCap */
		PHY_SetBBReg(adapter, REG_MAC_PHY_CTRL, 0x7FF80000, (crystal_cap | (crystal_cap << 6)));
		break;
#endif
#if defined(CONFIG_RTL8723B) || defined(CONFIG_RTL8703B) || defined(CONFIG_RTL8821A) || defined(CONFIG_RTL8192E)
	case RTL8723B:
	case RTL8703B:
	case RTL8821:
	case RTL8192E:
		/* write 0x2C[23:18] = 0x2C[17:12] = CrystalCap */
		PHY_SetBBReg(adapter, REG_MAC_PHY_CTRL, 0x00FFF000, (crystal_cap | (crystal_cap << 6)));
		break;
#endif
#if defined(CONFIG_RTL8814A)
	case RTL8814A:
		/* write 0x2C[26:21] = 0x2C[20:15] = CrystalCap*/
		PHY_SetBBReg(adapter, REG_MAC_PHY_CTRL, 0x07FF8000, (crystal_cap | (crystal_cap << 6)));
		break;
#endif
#if defined(CONFIG_RTL8821B) || defined(CONFIG_RTL8822B)
	case RTL8821B:
	case RTL8822B:
		/* write 0x28[6:1] = 0x24[30:25] = CrystalCap */
		crystal_cap = crystal_cap & 0x3F;
		PHY_SetBBReg(adapter, REG_AFE_XTAL_CTRL, 0x7E000000, crystal_cap);
		PHY_SetBBReg(adapter, REG_AFE_PLL_CTRL, 0x7E, crystal_cap);
		break;
#endif
	default:
		rtw_warn_on(1);
	}
}

int hal_spec_init(_adapter *adapter)
{
	u8 interface_type = 0;
	int ret = _SUCCESS;

	interface_type = rtw_get_intf_type(adapter);

	switch (rtw_get_chip_type(adapter)) {
#ifdef CONFIG_RTL8723B
	case RTL8723B:
		init_hal_spec_8723b(adapter);
		break;
#endif
#ifdef CONFIG_RTL8703B
	case RTL8703B:
		init_hal_spec_8703b(adapter);
		break;
#endif
#ifdef CONFIG_RTL8188E
	case RTL8188E:
		init_hal_spec_8188e(adapter);
		break;
#endif
#ifdef CONFIG_RTL8188F
	case RTL8188F:
		init_hal_spec_8188f(adapter);
		break;
#endif
#ifdef CONFIG_RTL8812A
	case RTL8812:
		init_hal_spec_8812a(adapter);
		break;
#endif
#ifdef CONFIG_RTL8821A
	case RTL8821:
		init_hal_spec_8821a(adapter);
		break;
#endif
#ifdef CONFIG_RTL8192E
	case RTL8192E:
		init_hal_spec_8192e(adapter);
		break;
#endif
#ifdef CONFIG_RTL8814A
	case RTL8814A:
		init_hal_spec_8814a(adapter);
		break;
#endif
	default:
		DBG_871X_LEVEL(_drv_err_, "%s: unknown chip_type:%u\n"
			, __func__, rtw_get_chip_type(adapter));
		ret = _FAIL;
		break;
	}

	return ret;
}

static const char * const _band_cap_str[] = {
	/* BIT0 */"2G",
	/* BIT1 */"5G",
};

static const char * const _bw_cap_str[] = {
	/* BIT0 */"5M",
	/* BIT1 */"10M",
	/* BIT2 */"20M",
	/* BIT3 */"40M",
	/* BIT4 */"80M",
	/* BIT5 */"160M",
	/* BIT6 */"80_80M",
};

static const char * const _proto_cap_str[] = {
	/* BIT0 */"b",
	/* BIT1 */"g",
	/* BIT2 */"n",
	/* BIT3 */"ac",
};

static const char * const _wl_func_str[] = {
	/* BIT0 */"P2P",
	/* BIT1 */"MIRACAST",
	/* BIT2 */"TDLS",
	/* BIT3 */"FTM",
};

void dump_hal_spec(void *sel, _adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	int i;

	DBG_871X_SEL_NL(sel, "macid_num:%u\n", hal_spec->macid_num);
	DBG_871X_SEL_NL(sel, "sec_cap:0x%02x\n", hal_spec->sec_cap);
	DBG_871X_SEL_NL(sel, "sec_cam_ent_num:%u\n", hal_spec->sec_cam_ent_num);
	DBG_871X_SEL_NL(sel, "nss_num:%u\n", hal_spec->nss_num);

	DBG_871X_SEL_NL(sel, "band_cap:");
	for (i = 0; i < BAND_CAP_BIT_NUM; i++) {
		if (((hal_spec->band_cap) >> i) & BIT0 && _band_cap_str[i])
			DBG_871X_SEL(sel, "%s ", _band_cap_str[i]);
	}
	DBG_871X_SEL(sel, "\n");

	DBG_871X_SEL_NL(sel, "bw_cap:");
	for (i = 0; i < BW_CAP_BIT_NUM; i++) {
		if (((hal_spec->bw_cap) >> i) & BIT0 && _bw_cap_str[i])
			DBG_871X_SEL(sel, "%s ", _bw_cap_str[i]);
	}
	DBG_871X_SEL(sel, "\n");

	DBG_871X_SEL_NL(sel, "proto_cap:");
	for (i = 0; i < PROTO_CAP_BIT_NUM; i++) {
		if (((hal_spec->proto_cap) >> i) & BIT0 && _proto_cap_str[i])
			DBG_871X_SEL(sel, "%s ", _proto_cap_str[i]);
	}
	DBG_871X_SEL(sel, "\n");

	DBG_871X_SEL_NL(sel, "wl_func:");
	for (i = 0; i < WL_FUNC_BIT_NUM; i++) {
		if (((hal_spec->wl_func) >> i) & BIT0 && _wl_func_str[i])
			DBG_871X_SEL(sel, "%s ", _wl_func_str[i]);
	}
	DBG_871X_SEL(sel, "\n");
}

inline bool hal_chk_band_cap(_adapter *adapter, u8 cap)
{
	return (GET_HAL_SPEC(adapter)->band_cap & cap);
}

inline bool hal_chk_bw_cap(_adapter *adapter, u8 cap)
{
	return (GET_HAL_SPEC(adapter)->bw_cap & cap);
}

inline bool hal_chk_proto_cap(_adapter *adapter, u8 cap)
{
	return (GET_HAL_SPEC(adapter)->proto_cap & cap);
}

inline bool hal_chk_wl_func(_adapter *adapter, u8 func)
{
	return (GET_HAL_SPEC(adapter)->wl_func & func);
}

inline bool hal_is_band_support(_adapter *adapter, u8 band)
{
	return (GET_HAL_SPEC(adapter)->band_cap & band_to_band_cap(band));
}

inline bool hal_is_bw_support(_adapter *adapter, u8 bw)
{
	return (GET_HAL_SPEC(adapter)->bw_cap & ch_width_to_bw_cap(bw));
}

inline bool hal_is_wireless_mode_support(_adapter *adapter, u8 mode)
{
	u8 proto_cap = GET_HAL_SPEC(adapter)->proto_cap;

	if (mode == WIRELESS_11B)
		if ((proto_cap & PROTO_CAP_11B) && hal_chk_band_cap(adapter, BAND_CAP_2G))
			return 1;

	if (mode == WIRELESS_11G)
		if ((proto_cap & PROTO_CAP_11G) && hal_chk_band_cap(adapter, BAND_CAP_2G))
			return 1;

	if (mode == WIRELESS_11A)
		if ((proto_cap & PROTO_CAP_11G) && hal_chk_band_cap(adapter, BAND_CAP_5G))
			return 1;

	if (mode == WIRELESS_11_24N)
		if ((proto_cap & PROTO_CAP_11N) && hal_chk_band_cap(adapter, BAND_CAP_2G))
			return 1;

	if (mode == WIRELESS_11_5N)
		if ((proto_cap & PROTO_CAP_11N) && hal_chk_band_cap(adapter, BAND_CAP_5G))
			return 1;

	if (mode == WIRELESS_11AC)
		if ((proto_cap & PROTO_CAP_11AC) && hal_chk_band_cap(adapter, BAND_CAP_5G))
			return 1;

	return 0;
}

/*
* hal_largest_bw - starting from in_bw, get largest bw supported by HAL
* @adapter:
* @in_bw: starting bw, value of CHANNEL_WIDTH
*
* Returns: value of CHANNEL_WIDTH
*/
u8 hal_largest_bw(_adapter *adapter, u8 in_bw)
{
	for (; in_bw > CHANNEL_WIDTH_20; in_bw--) {
		if (hal_is_bw_support(adapter, in_bw))
			break;
	}

	if (!hal_is_bw_support(adapter, in_bw))
		rtw_warn_on(1);

	return in_bw;
}

#ifdef CONFIG_ANTENNA_DIVERSITY
u8	rtw_hal_antdiv_before_linked(_adapter *padapter)
{		
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u8 cur_ant, change_ant;

	if (!pHalData->AntDivCfg)
		return _FALSE;

	if (pHalData->sw_antdiv_bl_state == 0) {
		pHalData->sw_antdiv_bl_state = 1;

		rtw_hal_get_odm_var(padapter, HAL_ODM_ANTDIV_SELECT, &cur_ant, NULL);
		change_ant = (cur_ant == MAIN_ANT) ? AUX_ANT : MAIN_ANT;
	
		return rtw_antenna_select_cmd(padapter, change_ant, _FALSE);
	}

	pHalData->sw_antdiv_bl_state = 0;
	return _FALSE;
}

void	rtw_hal_antdiv_rssi_compared(_adapter *padapter, WLAN_BSSID_EX *dst, WLAN_BSSID_EX *src)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
		
	if (pHalData->AntDivCfg) {
		/*DBG_871X("update_network=> org-RSSI(%d), new-RSSI(%d)\n", dst->Rssi, src->Rssi);*/
		/*select optimum_antenna for before linked =>For antenna diversity*/
		if (dst->Rssi >=  src->Rssi) {/*keep org parameter*/
			src->Rssi = dst->Rssi;
			src->PhyInfo.Optimum_antenna = dst->PhyInfo.Optimum_antenna;
		}
	}
}
#endif

