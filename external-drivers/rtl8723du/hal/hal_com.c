// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _HAL_COM_C_

#include <drv_types.h>
#include "hal_com_h2c.h"

#include "hal_data.h"

void rtw_dump_fw_info(void *sel, struct adapter *adapter)
{
	struct hal_com_data	*hal_data = NULL;

	if (!adapter)
		return;

	hal_data = GET_HAL_DATA(adapter);
	if (hal_data->bFWReady)
		RTW_PRINT_SEL(sel, "FW VER -%d.%d\n", hal_data->firmware_version, hal_data->firmware_sub_version);
	else
		RTW_PRINT_SEL(sel, "FW not ready\n");
}

/* #define CONFIG_GTK_OL_DBG */

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	char	rtw_phy_para_file_path[PATH_LENGTH_MAX];
#endif

void dump_chip_info(struct hal_version	ChipVersion)
{
	int cnt = 0;
	u8 buf[128] = {0};

	if (IS_8188E(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8188E_");
	else if (IS_8188F(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8188F_");
	else if (IS_8812_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8812_");
	else if (IS_8192E(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8192E_");
	else if (IS_8821_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8821_");
	else if (IS_8723B_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8723B_");
	else if (IS_8703B_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8703B_");
	else if (IS_8723D_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8723D_");
	else if (IS_8814A_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8814A_");
	else if (IS_8822B_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8822B_");
	else if (IS_8821C_SERIES(ChipVersion))
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_8821C_");
	else
		cnt += sprintf((buf + cnt), "Chip Version Info: CHIP_UNKNOWN_");

	cnt += sprintf((buf + cnt), "%s_", IS_NORMAL_CHIP(ChipVersion) ? "Normal_Chip" : "Test_Chip");
	if (IS_CHIP_VENDOR_TSMC(ChipVersion))
		cnt += sprintf((buf + cnt), "%s_", "TSMC");
	else if (IS_CHIP_VENDOR_UMC(ChipVersion))
		cnt += sprintf((buf + cnt), "%s_", "UMC");
	else if (IS_CHIP_VENDOR_SMIC(ChipVersion))
		cnt += sprintf((buf + cnt), "%s_", "SMIC");

	if (IS_A_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "A_CUT_");
	else if (IS_B_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "B_CUT_");
	else if (IS_C_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "C_CUT_");
	else if (IS_D_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "D_CUT_");
	else if (IS_E_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "E_CUT_");
	else if (IS_F_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "F_CUT_");
	else if (IS_I_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "I_CUT_");
	else if (IS_J_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "J_CUT_");
	else if (IS_K_CUT(ChipVersion))
		cnt += sprintf((buf + cnt), "K_CUT_");
	else
		cnt += sprintf((buf + cnt), "UNKNOWN_CUT(%d)_", ChipVersion.CUTVersion);

	if (IS_1T1R(ChipVersion))
		cnt += sprintf((buf + cnt), "1T1R_");
	else if (IS_1T2R(ChipVersion))
		cnt += sprintf((buf + cnt), "1T2R_");
	else if (IS_2T2R(ChipVersion))
		cnt += sprintf((buf + cnt), "2T2R_");
	else if (IS_3T3R(ChipVersion))
		cnt += sprintf((buf + cnt), "3T3R_");
	else if (IS_3T4R(ChipVersion))
		cnt += sprintf((buf + cnt), "3T4R_");
	else if (IS_4T4R(ChipVersion))
		cnt += sprintf((buf + cnt), "4T4R_");
	else
		cnt += sprintf((buf + cnt), "UNKNOWN_RFTYPE(%d)_", ChipVersion.RFType);

	cnt += sprintf((buf + cnt), "RomVer(%d)\n", ChipVersion.ROMVer);

	RTW_INFO("%s", buf);
}
void rtw_hal_config_rftype(struct adapter *  adapt)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);

	if (IS_1T1R(pHalData->version_id)) {
		pHalData->rf_type = RF_1T1R;
		pHalData->NumTotalRFPath = 1;
	} else if (IS_2T2R(pHalData->version_id)) {
		pHalData->rf_type = RF_2T2R;
		pHalData->NumTotalRFPath = 2;
	} else if (IS_1T2R(pHalData->version_id)) {
		pHalData->rf_type = RF_1T2R;
		pHalData->NumTotalRFPath = 2;
	} else if (IS_3T3R(pHalData->version_id)) {
		pHalData->rf_type = RF_3T3R;
		pHalData->NumTotalRFPath = 3;
	} else if (IS_4T4R(pHalData->version_id)) {
		pHalData->rf_type = RF_4T4R;
		pHalData->NumTotalRFPath = 4;
	} else {
		pHalData->rf_type = RF_1T1R;
		pHalData->NumTotalRFPath = 1;
	}

	RTW_INFO("%s RF_Type is %d TotalTxPath is %d\n", __func__, pHalData->rf_type, pHalData->NumTotalRFPath);
}

#define	EEPROM_CHANNEL_PLAN_BY_HW_MASK	0x80

/*
 * Description:
 *	Use hardware(efuse), driver parameter(registry) and default channel plan
 *	to decide which one should be used.
 *
 * Parameters:
 *	adapt			pointer of adapter
 *	hw_alpha2		country code from HW (efuse/eeprom/mapfile)
 *	hw_chplan		channel plan from HW (efuse/eeprom/mapfile)
 *						BIT[7] software configure mode; 0:Enable, 1:disable
 *						BIT[6:0] Channel Plan
 *	sw_alpha2		country code from HW (registry/module param)
 *	sw_chplan		channel plan from SW (registry/module param)
 *	def_chplan		channel plan used when HW/SW both invalid
 *	AutoLoadFail		efuse autoload fail or not
 *
 */
void hal_com_config_channel_plan(
	struct adapter * adapt,
	char *hw_alpha2,
	u8 hw_chplan,
	char *sw_alpha2,
	u8 sw_chplan,
	u8 def_chplan,
	bool AutoLoadFail
)
{
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapt);
	struct hal_com_data *	pHalData;
	u8 force_hw_chplan = false;
	int chplan = -1;
	const struct country_chplan *country_ent = NULL, *ent;

	pHalData = GET_HAL_DATA(adapt);

	/* treat 0xFF as invalid value, bypass hw_chplan & force_hw_chplan parsing */
	if (hw_chplan == 0xFF)
		goto chk_hw_country_code;

	if (AutoLoadFail)
		goto chk_sw_config;

#ifndef CONFIG_FORCE_SW_CHANNEL_PLAN
	if (hw_chplan & EEPROM_CHANNEL_PLAN_BY_HW_MASK)
		force_hw_chplan = true;
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
			RTW_PRINT("%s unsupported hw_alpha2:\"%c%c\"\n", __func__, hw_alpha2[0], hw_alpha2[1]);
	}

	if (rtw_is_channel_plan_valid(hw_chplan))
		chplan = hw_chplan;
	else if (force_hw_chplan) {
		RTW_PRINT("%s unsupported hw_chplan:0x%02X\n", __func__, hw_chplan);
		/* hw infomaton invalid, refer to sw information */
		force_hw_chplan = false;
	}

chk_sw_config:
	if (force_hw_chplan)
		goto done;

	if (sw_alpha2 && !IS_ALPHA2_NO_SPECIFIED(sw_alpha2)) {
		ent = rtw_get_chplan_from_country(sw_alpha2);
		if (ent) {
			/* get chplan from sw country code, by pass sw chplan setting */
			country_ent = ent;
			chplan = ent->chplan;
			goto done;
		} else
			RTW_PRINT("%s unsupported sw_alpha2:\"%c%c\"\n", __func__, sw_alpha2[0], sw_alpha2[1]);
	}

	if (rtw_is_channel_plan_valid(sw_chplan)) {
		/* cancel hw_alpha2 because chplan is specified by sw_chplan*/
		country_ent = NULL;
		chplan = sw_chplan;
	} else if (sw_chplan != RTW_CHPLAN_UNSPECIFIED)
		RTW_PRINT("%s unsupported sw_chplan:0x%02X\n", __func__, sw_chplan);

done:
	if (chplan == -1) {
		RTW_PRINT("%s use def_chplan:0x%02X\n", __func__, def_chplan);
		chplan = def_chplan;
	} else if (country_ent) {
		RTW_PRINT("%s country code:\"%c%c\" with chplan:0x%02X\n", __func__
			, country_ent->alpha2[0], country_ent->alpha2[1], country_ent->chplan);
	} else
		RTW_PRINT("%s chplan:0x%02X\n", __func__, chplan);

	rfctl->country_ent = country_ent;
	rfctl->ChannelPlan = chplan;
	pHalData->bDisableSWChannelPlan = force_hw_chplan;
}

bool
HAL_IsLegalChannel(
	struct adapter *	Adapter,
	u32			Channel
)
{
	bool bLegalChannel = true;

	if (Channel > 14) {
		if (!is_supported_5g(Adapter->registrypriv.wireless_mode)) {
			bLegalChannel = false;
			RTW_INFO("Channel > 14 but wireless_mode do not support 5G\n");
		}
	} else if ((Channel <= 14) && (Channel >= 1)) {
		if (!IsSupported24G(Adapter->registrypriv.wireless_mode)) {
			bLegalChannel = false;
			RTW_INFO("(Channel <= 14) && (Channel >=1) but wireless_mode do not support 2.4G\n");
		}
	} else {
		bLegalChannel = false;
		RTW_INFO("Channel is Invalid !!!\n");
	}

	return bLegalChannel;
}

u8	MRateToHwRate(u8 rate)
{
	u8	ret = DESC_RATE1M;

	switch (rate) {
	case MGN_1M:
		ret = DESC_RATE1M;
		break;
	case MGN_2M:
		ret = DESC_RATE2M;
		break;
	case MGN_5_5M:
		ret = DESC_RATE5_5M;
		break;
	case MGN_11M:
		ret = DESC_RATE11M;
		break;
	case MGN_6M:
		ret = DESC_RATE6M;
		break;
	case MGN_9M:
		ret = DESC_RATE9M;
		break;
	case MGN_12M:
		ret = DESC_RATE12M;
		break;
	case MGN_18M:
		ret = DESC_RATE18M;
		break;
	case MGN_24M:
		ret = DESC_RATE24M;
		break;
	case MGN_36M:
		ret = DESC_RATE36M;
		break;
	case MGN_48M:
		ret = DESC_RATE48M;
		break;
	case MGN_54M:
		ret = DESC_RATE54M;
		break;

	case MGN_MCS0:
		ret = DESC_RATEMCS0;
		break;
	case MGN_MCS1:
		ret = DESC_RATEMCS1;
		break;
	case MGN_MCS2:
		ret = DESC_RATEMCS2;
		break;
	case MGN_MCS3:
		ret = DESC_RATEMCS3;
		break;
	case MGN_MCS4:
		ret = DESC_RATEMCS4;
		break;
	case MGN_MCS5:
		ret = DESC_RATEMCS5;
		break;
	case MGN_MCS6:
		ret = DESC_RATEMCS6;
		break;
	case MGN_MCS7:
		ret = DESC_RATEMCS7;
		break;
	case MGN_MCS8:
		ret = DESC_RATEMCS8;
		break;
	case MGN_MCS9:
		ret = DESC_RATEMCS9;
		break;
	case MGN_MCS10:
		ret = DESC_RATEMCS10;
		break;
	case MGN_MCS11:
		ret = DESC_RATEMCS11;
		break;
	case MGN_MCS12:
		ret = DESC_RATEMCS12;
		break;
	case MGN_MCS13:
		ret = DESC_RATEMCS13;
		break;
	case MGN_MCS14:
		ret = DESC_RATEMCS14;
		break;
	case MGN_MCS15:
		ret = DESC_RATEMCS15;
		break;
	case MGN_MCS16:
		ret = DESC_RATEMCS16;
		break;
	case MGN_MCS17:
		ret = DESC_RATEMCS17;
		break;
	case MGN_MCS18:
		ret = DESC_RATEMCS18;
		break;
	case MGN_MCS19:
		ret = DESC_RATEMCS19;
		break;
	case MGN_MCS20:
		ret = DESC_RATEMCS20;
		break;
	case MGN_MCS21:
		ret = DESC_RATEMCS21;
		break;
	case MGN_MCS22:
		ret = DESC_RATEMCS22;
		break;
	case MGN_MCS23:
		ret = DESC_RATEMCS23;
		break;
	case MGN_MCS24:
		ret = DESC_RATEMCS24;
		break;
	case MGN_MCS25:
		ret = DESC_RATEMCS25;
		break;
	case MGN_MCS26:
		ret = DESC_RATEMCS26;
		break;
	case MGN_MCS27:
		ret = DESC_RATEMCS27;
		break;
	case MGN_MCS28:
		ret = DESC_RATEMCS28;
		break;
	case MGN_MCS29:
		ret = DESC_RATEMCS29;
		break;
	case MGN_MCS30:
		ret = DESC_RATEMCS30;
		break;
	case MGN_MCS31:
		ret = DESC_RATEMCS31;
		break;

	case MGN_VHT1SS_MCS0:
		ret = DESC_RATEVHTSS1MCS0;
		break;
	case MGN_VHT1SS_MCS1:
		ret = DESC_RATEVHTSS1MCS1;
		break;
	case MGN_VHT1SS_MCS2:
		ret = DESC_RATEVHTSS1MCS2;
		break;
	case MGN_VHT1SS_MCS3:
		ret = DESC_RATEVHTSS1MCS3;
		break;
	case MGN_VHT1SS_MCS4:
		ret = DESC_RATEVHTSS1MCS4;
		break;
	case MGN_VHT1SS_MCS5:
		ret = DESC_RATEVHTSS1MCS5;
		break;
	case MGN_VHT1SS_MCS6:
		ret = DESC_RATEVHTSS1MCS6;
		break;
	case MGN_VHT1SS_MCS7:
		ret = DESC_RATEVHTSS1MCS7;
		break;
	case MGN_VHT1SS_MCS8:
		ret = DESC_RATEVHTSS1MCS8;
		break;
	case MGN_VHT1SS_MCS9:
		ret = DESC_RATEVHTSS1MCS9;
		break;
	case MGN_VHT2SS_MCS0:
		ret = DESC_RATEVHTSS2MCS0;
		break;
	case MGN_VHT2SS_MCS1:
		ret = DESC_RATEVHTSS2MCS1;
		break;
	case MGN_VHT2SS_MCS2:
		ret = DESC_RATEVHTSS2MCS2;
		break;
	case MGN_VHT2SS_MCS3:
		ret = DESC_RATEVHTSS2MCS3;
		break;
	case MGN_VHT2SS_MCS4:
		ret = DESC_RATEVHTSS2MCS4;
		break;
	case MGN_VHT2SS_MCS5:
		ret = DESC_RATEVHTSS2MCS5;
		break;
	case MGN_VHT2SS_MCS6:
		ret = DESC_RATEVHTSS2MCS6;
		break;
	case MGN_VHT2SS_MCS7:
		ret = DESC_RATEVHTSS2MCS7;
		break;
	case MGN_VHT2SS_MCS8:
		ret = DESC_RATEVHTSS2MCS8;
		break;
	case MGN_VHT2SS_MCS9:
		ret = DESC_RATEVHTSS2MCS9;
		break;
	case MGN_VHT3SS_MCS0:
		ret = DESC_RATEVHTSS3MCS0;
		break;
	case MGN_VHT3SS_MCS1:
		ret = DESC_RATEVHTSS3MCS1;
		break;
	case MGN_VHT3SS_MCS2:
		ret = DESC_RATEVHTSS3MCS2;
		break;
	case MGN_VHT3SS_MCS3:
		ret = DESC_RATEVHTSS3MCS3;
		break;
	case MGN_VHT3SS_MCS4:
		ret = DESC_RATEVHTSS3MCS4;
		break;
	case MGN_VHT3SS_MCS5:
		ret = DESC_RATEVHTSS3MCS5;
		break;
	case MGN_VHT3SS_MCS6:
		ret = DESC_RATEVHTSS3MCS6;
		break;
	case MGN_VHT3SS_MCS7:
		ret = DESC_RATEVHTSS3MCS7;
		break;
	case MGN_VHT3SS_MCS8:
		ret = DESC_RATEVHTSS3MCS8;
		break;
	case MGN_VHT3SS_MCS9:
		ret = DESC_RATEVHTSS3MCS9;
		break;
	case MGN_VHT4SS_MCS0:
		ret = DESC_RATEVHTSS4MCS0;
		break;
	case MGN_VHT4SS_MCS1:
		ret = DESC_RATEVHTSS4MCS1;
		break;
	case MGN_VHT4SS_MCS2:
		ret = DESC_RATEVHTSS4MCS2;
		break;
	case MGN_VHT4SS_MCS3:
		ret = DESC_RATEVHTSS4MCS3;
		break;
	case MGN_VHT4SS_MCS4:
		ret = DESC_RATEVHTSS4MCS4;
		break;
	case MGN_VHT4SS_MCS5:
		ret = DESC_RATEVHTSS4MCS5;
		break;
	case MGN_VHT4SS_MCS6:
		ret = DESC_RATEVHTSS4MCS6;
		break;
	case MGN_VHT4SS_MCS7:
		ret = DESC_RATEVHTSS4MCS7;
		break;
	case MGN_VHT4SS_MCS8:
		ret = DESC_RATEVHTSS4MCS8;
		break;
	case MGN_VHT4SS_MCS9:
		ret = DESC_RATEVHTSS4MCS9;
		break;
	default:
		break;
	}

	return ret;
}

u8	hw_rate_to_m_rate(u8 rate)
{
	u8	ret_rate = MGN_1M;

	switch (rate) {

	case DESC_RATE1M:
		ret_rate = MGN_1M;
		break;
	case DESC_RATE2M:
		ret_rate = MGN_2M;
		break;
	case DESC_RATE5_5M:
		ret_rate = MGN_5_5M;
		break;
	case DESC_RATE11M:
		ret_rate = MGN_11M;
		break;
	case DESC_RATE6M:
		ret_rate = MGN_6M;
		break;
	case DESC_RATE9M:
		ret_rate = MGN_9M;
		break;
	case DESC_RATE12M:
		ret_rate = MGN_12M;
		break;
	case DESC_RATE18M:
		ret_rate = MGN_18M;
		break;
	case DESC_RATE24M:
		ret_rate = MGN_24M;
		break;
	case DESC_RATE36M:
		ret_rate = MGN_36M;
		break;
	case DESC_RATE48M:
		ret_rate = MGN_48M;
		break;
	case DESC_RATE54M:
		ret_rate = MGN_54M;
		break;
	case DESC_RATEMCS0:
		ret_rate = MGN_MCS0;
		break;
	case DESC_RATEMCS1:
		ret_rate = MGN_MCS1;
		break;
	case DESC_RATEMCS2:
		ret_rate = MGN_MCS2;
		break;
	case DESC_RATEMCS3:
		ret_rate = MGN_MCS3;
		break;
	case DESC_RATEMCS4:
		ret_rate = MGN_MCS4;
		break;
	case DESC_RATEMCS5:
		ret_rate = MGN_MCS5;
		break;
	case DESC_RATEMCS6:
		ret_rate = MGN_MCS6;
		break;
	case DESC_RATEMCS7:
		ret_rate = MGN_MCS7;
		break;
	case DESC_RATEMCS8:
		ret_rate = MGN_MCS8;
		break;
	case DESC_RATEMCS9:
		ret_rate = MGN_MCS9;
		break;
	case DESC_RATEMCS10:
		ret_rate = MGN_MCS10;
		break;
	case DESC_RATEMCS11:
		ret_rate = MGN_MCS11;
		break;
	case DESC_RATEMCS12:
		ret_rate = MGN_MCS12;
		break;
	case DESC_RATEMCS13:
		ret_rate = MGN_MCS13;
		break;
	case DESC_RATEMCS14:
		ret_rate = MGN_MCS14;
		break;
	case DESC_RATEMCS15:
		ret_rate = MGN_MCS15;
		break;
	case DESC_RATEMCS16:
		ret_rate = MGN_MCS16;
		break;
	case DESC_RATEMCS17:
		ret_rate = MGN_MCS17;
		break;
	case DESC_RATEMCS18:
		ret_rate = MGN_MCS18;
		break;
	case DESC_RATEMCS19:
		ret_rate = MGN_MCS19;
		break;
	case DESC_RATEMCS20:
		ret_rate = MGN_MCS20;
		break;
	case DESC_RATEMCS21:
		ret_rate = MGN_MCS21;
		break;
	case DESC_RATEMCS22:
		ret_rate = MGN_MCS22;
		break;
	case DESC_RATEMCS23:
		ret_rate = MGN_MCS23;
		break;
	case DESC_RATEMCS24:
		ret_rate = MGN_MCS24;
		break;
	case DESC_RATEMCS25:
		ret_rate = MGN_MCS25;
		break;
	case DESC_RATEMCS26:
		ret_rate = MGN_MCS26;
		break;
	case DESC_RATEMCS27:
		ret_rate = MGN_MCS27;
		break;
	case DESC_RATEMCS28:
		ret_rate = MGN_MCS28;
		break;
	case DESC_RATEMCS29:
		ret_rate = MGN_MCS29;
		break;
	case DESC_RATEMCS30:
		ret_rate = MGN_MCS30;
		break;
	case DESC_RATEMCS31:
		ret_rate = MGN_MCS31;
		break;
	case DESC_RATEVHTSS1MCS0:
		ret_rate = MGN_VHT1SS_MCS0;
		break;
	case DESC_RATEVHTSS1MCS1:
		ret_rate = MGN_VHT1SS_MCS1;
		break;
	case DESC_RATEVHTSS1MCS2:
		ret_rate = MGN_VHT1SS_MCS2;
		break;
	case DESC_RATEVHTSS1MCS3:
		ret_rate = MGN_VHT1SS_MCS3;
		break;
	case DESC_RATEVHTSS1MCS4:
		ret_rate = MGN_VHT1SS_MCS4;
		break;
	case DESC_RATEVHTSS1MCS5:
		ret_rate = MGN_VHT1SS_MCS5;
		break;
	case DESC_RATEVHTSS1MCS6:
		ret_rate = MGN_VHT1SS_MCS6;
		break;
	case DESC_RATEVHTSS1MCS7:
		ret_rate = MGN_VHT1SS_MCS7;
		break;
	case DESC_RATEVHTSS1MCS8:
		ret_rate = MGN_VHT1SS_MCS8;
		break;
	case DESC_RATEVHTSS1MCS9:
		ret_rate = MGN_VHT1SS_MCS9;
		break;
	case DESC_RATEVHTSS2MCS0:
		ret_rate = MGN_VHT2SS_MCS0;
		break;
	case DESC_RATEVHTSS2MCS1:
		ret_rate = MGN_VHT2SS_MCS1;
		break;
	case DESC_RATEVHTSS2MCS2:
		ret_rate = MGN_VHT2SS_MCS2;
		break;
	case DESC_RATEVHTSS2MCS3:
		ret_rate = MGN_VHT2SS_MCS3;
		break;
	case DESC_RATEVHTSS2MCS4:
		ret_rate = MGN_VHT2SS_MCS4;
		break;
	case DESC_RATEVHTSS2MCS5:
		ret_rate = MGN_VHT2SS_MCS5;
		break;
	case DESC_RATEVHTSS2MCS6:
		ret_rate = MGN_VHT2SS_MCS6;
		break;
	case DESC_RATEVHTSS2MCS7:
		ret_rate = MGN_VHT2SS_MCS7;
		break;
	case DESC_RATEVHTSS2MCS8:
		ret_rate = MGN_VHT2SS_MCS8;
		break;
	case DESC_RATEVHTSS2MCS9:
		ret_rate = MGN_VHT2SS_MCS9;
		break;
	case DESC_RATEVHTSS3MCS0:
		ret_rate = MGN_VHT3SS_MCS0;
		break;
	case DESC_RATEVHTSS3MCS1:
		ret_rate = MGN_VHT3SS_MCS1;
		break;
	case DESC_RATEVHTSS3MCS2:
		ret_rate = MGN_VHT3SS_MCS2;
		break;
	case DESC_RATEVHTSS3MCS3:
		ret_rate = MGN_VHT3SS_MCS3;
		break;
	case DESC_RATEVHTSS3MCS4:
		ret_rate = MGN_VHT3SS_MCS4;
		break;
	case DESC_RATEVHTSS3MCS5:
		ret_rate = MGN_VHT3SS_MCS5;
		break;
	case DESC_RATEVHTSS3MCS6:
		ret_rate = MGN_VHT3SS_MCS6;
		break;
	case DESC_RATEVHTSS3MCS7:
		ret_rate = MGN_VHT3SS_MCS7;
		break;
	case DESC_RATEVHTSS3MCS8:
		ret_rate = MGN_VHT3SS_MCS8;
		break;
	case DESC_RATEVHTSS3MCS9:
		ret_rate = MGN_VHT3SS_MCS9;
		break;
	case DESC_RATEVHTSS4MCS0:
		ret_rate = MGN_VHT4SS_MCS0;
		break;
	case DESC_RATEVHTSS4MCS1:
		ret_rate = MGN_VHT4SS_MCS1;
		break;
	case DESC_RATEVHTSS4MCS2:
		ret_rate = MGN_VHT4SS_MCS2;
		break;
	case DESC_RATEVHTSS4MCS3:
		ret_rate = MGN_VHT4SS_MCS3;
		break;
	case DESC_RATEVHTSS4MCS4:
		ret_rate = MGN_VHT4SS_MCS4;
		break;
	case DESC_RATEVHTSS4MCS5:
		ret_rate = MGN_VHT4SS_MCS5;
		break;
	case DESC_RATEVHTSS4MCS6:
		ret_rate = MGN_VHT4SS_MCS6;
		break;
	case DESC_RATEVHTSS4MCS7:
		ret_rate = MGN_VHT4SS_MCS7;
		break;
	case DESC_RATEVHTSS4MCS8:
		ret_rate = MGN_VHT4SS_MCS8;
		break;
	case DESC_RATEVHTSS4MCS9:
		ret_rate = MGN_VHT4SS_MCS9;
		break;

	default:
		RTW_INFO("hw_rate_to_m_rate(): Non supported Rate [%x]!!!\n", rate);
		break;
	}

	return ret_rate;
}

void	HalSetBrateCfg(
	struct adapter *		Adapter,
	u8			*mBratesOS,
	u16			*pBrateCfg)
{
	u8	i, is_brate, brate;

	for (i = 0; i < NDIS_802_11_LENGTH_RATES_EX; i++) {
		is_brate = mBratesOS[i] & IEEE80211_BASIC_RATE_MASK;
		brate = mBratesOS[i] & 0x7f;

		if (is_brate) {
			switch (brate) {
			case IEEE80211_CCK_RATE_1MB:
				*pBrateCfg |= RATE_1M;
				break;
			case IEEE80211_CCK_RATE_2MB:
				*pBrateCfg |= RATE_2M;
				break;
			case IEEE80211_CCK_RATE_5MB:
				*pBrateCfg |= RATE_5_5M;
				break;
			case IEEE80211_CCK_RATE_11MB:
				*pBrateCfg |= RATE_11M;
				break;
			case IEEE80211_OFDM_RATE_6MB:
				*pBrateCfg |= RATE_6M;
				break;
			case IEEE80211_OFDM_RATE_9MB:
				*pBrateCfg |= RATE_9M;
				break;
			case IEEE80211_OFDM_RATE_12MB:
				*pBrateCfg |= RATE_12M;
				break;
			case IEEE80211_OFDM_RATE_18MB:
				*pBrateCfg |= RATE_18M;
				break;
			case IEEE80211_OFDM_RATE_24MB:
				*pBrateCfg |= RATE_24M;
				break;
			case IEEE80211_OFDM_RATE_36MB:
				*pBrateCfg |= RATE_36M;
				break;
			case IEEE80211_OFDM_RATE_48MB:
				*pBrateCfg |= RATE_48M;
				break;
			case IEEE80211_OFDM_RATE_54MB:
				*pBrateCfg |= RATE_54M;
				break;
			}
		}
	}
}

static void
_OneOutPipeMapping(
	struct adapter *	pAdapter
)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];/* VO */
	pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];/* VI */
	pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[0];/* BE */
	pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[0];/* BK */

	pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];/* BCN */
	pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];/* MGT */
	pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];/* HIGH */
	pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];/* TXCMD */
}

static void
_TwoOutPipeMapping(
	struct adapter *	pAdapter,
	bool		bWIFICfg
)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if (bWIFICfg) { /* WMM */

		/*	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA  */
		/* {  0, 	1, 	0, 	1, 	0, 	0, 	0, 	0, 		0	}; */
		/* 0:ep_0 num, 1:ep_1 num */

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[1];/* VO */
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];/* VI */
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[1];/* BE */
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[0];/* BK */

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];/* BCN */
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];/* MGT */
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];/* HIGH */
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];/* TXCMD */

	} else { /* typical setting */


		/* BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA */
		/* {  1, 	1, 	0, 	0, 	0, 	0, 	0, 	0, 		0	};			 */
		/* 0:ep_0 num, 1:ep_1 num */

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];/* VO */
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];/* VI */
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[1];/* BE */
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];/* BK */

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];/* BCN */
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];/* MGT */
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];/* HIGH */
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];/* TXCMD	 */

	}

}

static void _ThreeOutPipeMapping(
	struct adapter *	pAdapter,
	bool		bWIFICfg
)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if (bWIFICfg) { /* for WMM */

		/*	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA  */
		/* {  1, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	}; */
		/* 0:H, 1:N, 2:L */

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];/* VO */
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];/* VI */
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];/* BE */
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];/* BK */

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];/* BCN */
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];/* MGT */
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];/* HIGH */
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];/* TXCMD */

	} else { /* typical setting */


		/*	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA  */
		/* {  2, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};			 */
		/* 0:H, 1:N, 2:L */

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];/* VO */
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];/* VI */
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];/* BE */
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[2];/* BK */

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];/* BCN */
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];/* MGT */
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];/* HIGH */
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];/* TXCMD	 */
	}

}

bool
Hal_MappingOutPipe(
	struct adapter *	pAdapter,
	u8		NumOutPipe
)
{
	struct registry_priv *pregistrypriv = &pAdapter->registrypriv;

	bool	 bWIFICfg = (pregistrypriv->wifi_spec) ? true : false;

	bool result = true;

	switch (NumOutPipe) {
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
		result = false;
		break;
	}

	return result;

}

void rtw_hal_reqtxrpt(struct adapter *adapt, u8 macid)
{
	if (adapt->hal_func.reqtxrpt)
		adapt->hal_func.reqtxrpt(adapt, macid);
}

void rtw_hal_dump_macaddr(void *sel, struct adapter *adapter)
{
	int i;
	struct adapter *iface;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	u8 mac_addr[ETH_ALEN];

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_mbid_cam_dump(sel, __func__, adapter);
#else
	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		if (iface) {
			rtw_hal_get_hwreg(iface, HW_VAR_MAC_ADDR, mac_addr);
			RTW_PRINT_SEL(sel, ADPT_FMT"- hw port(%d) mac_addr ="MAC_FMT"\n",
				ADPT_ARG(iface), iface->hw_port, MAC_ARG(mac_addr));
		}
	}
#endif
}

void rtw_restore_mac_addr(struct adapter *adapter)
{
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_mbid_cam_restore(adapter);
#else
	int i;
	struct adapter *iface;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		if (iface)
			rtw_hal_set_hwreg(iface, HW_VAR_MAC_ADDR, adapter_mac_addr(iface));
	}
#endif
	rtw_hal_dump_macaddr(RTW_DBGDUMP, adapter);
}

void rtw_init_hal_com_default_value(struct adapter * Adapter)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(Adapter);
	struct registry_priv *regsty = adapter_to_regsty(Adapter);

	pHalData->AntDetection = 1;
	pHalData->antenna_test = false;
	pHalData->RegIQKFWOffload = regsty->iqk_fw_offload;
	pHalData->ch_switch_offload = regsty->ch_switch_offload;
}

#ifdef CONFIG_FW_C2H_REG
void c2h_evt_clear(struct adapter *adapter)
{
	rtw_write8(adapter, REG_C2HEVT_CLEAR, C2H_EVT_HOST_CLOSE);
}

int c2h_evt_read_88xx(struct adapter *adapter, u8 *buf)
{
	int ret = _FAIL;
	int i;
	u8 trigger;

	if (!buf)
		goto exit;

	trigger = rtw_read8(adapter, REG_C2HEVT_CLEAR);

	if (trigger == C2H_EVT_HOST_CLOSE) {
		goto exit; /* Not ready */
	} else if (trigger != C2H_EVT_FW_CLOSE) {
		goto clear_evt; /* Not a valid value */
	}

	memset(buf, 0, C2H_REG_LEN);

	/* Read ID, LEN, SEQ */
	SET_C2H_ID_88XX(buf, rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL));
	SET_C2H_SEQ_88XX(buf, rtw_read8(adapter, REG_C2HEVT_CMD_SEQ_88XX));
	SET_C2H_PLEN_88XX(buf, rtw_read8(adapter, REG_C2HEVT_CMD_LEN_88XX));

	/* Read the content */
	for (i = 0; i < C2H_PLEN_88XX(buf); i++)
		*(C2H_PAYLOAD_88XX(buf) + i) = rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL + 2 + i);

	RTW_DBG_DUMP("payload: ", C2H_PAYLOAD_88XX(buf), C2H_PLEN_88XX(buf));

	ret = _SUCCESS;

clear_evt:
	/*
	* Clear event to notify FW we have read the command.
	* If this field isn't clear, the FW won't update the next command message.
	*/
	c2h_evt_clear(adapter);

exit:
	return ret;
}
#endif /* CONFIG_FW_C2H_REG */

#ifndef DBG_C2H_PKT_PRE_HDL
#define DBG_C2H_PKT_PRE_HDL 0
#endif
#ifndef DBG_C2H_PKT_HDL
#define DBG_C2H_PKT_HDL 0
#endif
void rtw_hal_c2h_pkt_pre_hdl(struct adapter *adapter, u8 *buf, u16 len)
{
	u8 parse_fail = 0;
	u8 hdl_here = 0;
	int ret = _FAIL;
	u8 id, seq, plen;
	u8 *payload;

	if (rtw_hal_c2h_pkt_hdr_parse(adapter, buf, len, &id, &seq, &plen, &payload) != _SUCCESS) {
		parse_fail = 1;
		goto exit;
	}

	hdl_here = rtw_hal_c2h_id_handle_directly(adapter, id, seq, plen, payload) ? 1 : 0;
	if (hdl_here) 
		ret = rtw_hal_c2h_handler(adapter, id, seq, plen, payload);
	else
		ret = rtw_c2h_packet_wk_cmd(adapter, buf, len);

exit:
	if (parse_fail)
		RTW_ERR("%s parse fail, buf=%p, len=:%u\n", __func__, buf, len);
	else if (ret != _SUCCESS || DBG_C2H_PKT_PRE_HDL > 0) {
		RTW_PRINT("%s: id=0x%02x, seq=%u, plen=%u, %s %s\n", __func__, id, seq, plen
			, hdl_here ? "handle" : "enqueue"
			, ret == _SUCCESS ? "ok" : "fail"
		);
		if (DBG_C2H_PKT_PRE_HDL >= 2)
			RTW_PRINT_DUMP("dump: ", buf, len);
	}
}

void rtw_hal_c2h_pkt_hdl(struct adapter *adapter, u8 *buf, u16 len)
{
	u8 parse_fail = 0;
	u8 bypass = 0;
	int ret = _FAIL;
	u8 id, seq, plen;
	u8 *payload;

	if (rtw_hal_c2h_pkt_hdr_parse(adapter, buf, len, &id, &seq, &plen, &payload) != _SUCCESS) {
		parse_fail = 1;
		goto exit;
	}

	ret = rtw_hal_c2h_handler(adapter, id, seq, plen, payload);

exit:
	if (parse_fail)
		RTW_ERR("%s parse fail, buf=%p, len=:%u\n", __func__, buf, len);
	else if (ret != _SUCCESS || bypass || DBG_C2H_PKT_HDL > 0) {
		RTW_PRINT("%s: id=0x%02x, seq=%u, plen=%u, %s %s\n", __func__, id, seq, plen
			, !bypass ? "handle" : "bypass"
			, ret == _SUCCESS ? "ok" : "fail"
		);
		if (DBG_C2H_PKT_HDL >= 2)
			RTW_PRINT_DUMP("dump: ", buf, len);
	}
}

void c2h_iqk_offload(struct adapter *adapter, u8 *data, u8 len)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	struct submit_ctx *iqk_sctx = &hal_data->iqk_sctx;

	RTW_INFO("IQK offload finish in %dms\n", rtw_get_passing_time_ms(iqk_sctx->submit_time));
	rtw_sctx_done(&iqk_sctx);
}

int c2h_iqk_offload_wait(struct adapter *adapter, u32 timeout_ms)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	struct submit_ctx *iqk_sctx = &hal_data->iqk_sctx;

	iqk_sctx->submit_time = rtw_get_current_time();
	iqk_sctx->timeout_ms = timeout_ms;
	iqk_sctx->status = RTW_SCTX_SUBMITTED;

	return rtw_sctx_wait(iqk_sctx, __func__);
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
#define	GET_C2H_MAC_HIDDEN_RPT_ANT_NUM(_data)			LE_BITS_TO_1BYTE(((u8 *)(_data)) + 6, 5, 3)
#define	GET_C2H_MAC_HIDDEN_RPT_80211_PROTOCOL(_data)	LE_BITS_TO_1BYTE(((u8 *)(_data)) + 7, 2, 2)
#define	GET_C2H_MAC_HIDDEN_RPT_NIC_ROUTER(_data)		LE_BITS_TO_1BYTE(((u8 *)(_data)) + 7, 6, 2)

#ifndef DBG_C2H_MAC_HIDDEN_RPT_HANDLE
#define DBG_C2H_MAC_HIDDEN_RPT_HANDLE 0
#endif

int c2h_mac_hidden_rpt_hdl(struct adapter *adapter, u8 *data, u8 len)
{
	struct hal_com_data	*hal_data = GET_HAL_DATA(adapter);
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	int ret = _FAIL;
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
	u8 ant_num;
	u8 protocol;
	u8 nic;

	int i;

	if (len < MAC_HIDDEN_RPT_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, MAC_HIDDEN_RPT_LEN);
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
	ant_num = GET_C2H_MAC_HIDDEN_RPT_ANT_NUM(data);

	protocol = GET_C2H_MAC_HIDDEN_RPT_80211_PROTOCOL(data);
	nic = GET_C2H_MAC_HIDDEN_RPT_NIC_ROUTER(data);

	if (DBG_C2H_MAC_HIDDEN_RPT_HANDLE) {
		for (i = 0; i < len; i++)
			RTW_PRINT("%s: 0x%02X\n", __func__, *(data + i));

		RTW_PRINT("uuid x:0x%02x y:0x%02x z:0x%x crc:0x%x\n", uuid_x, uuid_y, uuid_z, uuid_crc);
		RTW_PRINT("hci_type:0x%x\n", hci_type);
		RTW_PRINT("package_type:0x%x\n", package_type);
		RTW_PRINT("tr_switch:0x%x\n", tr_switch);
		RTW_PRINT("wl_func:0x%x\n", wl_func);
		RTW_PRINT("hw_stype:0x%x\n", hw_stype);
		RTW_PRINT("bw:0x%x\n", bw);
		RTW_PRINT("ant_num:0x%x\n", ant_num);
		RTW_PRINT("protocol:0x%x\n", protocol);
		RTW_PRINT("nic:0x%x\n", nic);
	}

	/*
	* NOTICE:
	* for now, the following is common info/format
	* if there is any hal difference need to export
	* some IC dependent code will need to be implement
	*/
	hal_data->PackageType = package_type;
	hal_spec->wl_func &= mac_hidden_wl_func_to_hal_wl_func(wl_func);
	hal_spec->bw_cap &= mac_hidden_max_bw_to_hal_bw_cap(bw);
	hal_spec->tx_nss_num = rtw_min(hal_spec->tx_nss_num, ant_num);
	hal_spec->rx_nss_num = rtw_min(hal_spec->rx_nss_num, ant_num);
	hal_spec->proto_cap &= mac_hidden_proto_to_hal_proto_cap(protocol);
	hal_spec->hci_type = hci_type;

	/* TODO: tr_switch */

	ret = _SUCCESS;

exit:
	return ret;
}

int c2h_mac_hidden_rpt_2_hdl(struct adapter *adapter, u8 *data, u8 len)
{
	int ret = _FAIL;
	int i;

	if (len < MAC_HIDDEN_RPT_2_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, MAC_HIDDEN_RPT_2_LEN);
		goto exit;
	}

	if (DBG_C2H_MAC_HIDDEN_RPT_HANDLE) {
		for (i = 0; i < len; i++)
			RTW_PRINT("%s: 0x%02X\n", __func__, *(data + i));
	}

	ret = _SUCCESS;

exit:
	return ret;
}

static int c2h_mac_hidden_rpt_bt_info_hdl(struct adapter *adapter, u8 val)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);

	hal_spec->bt_ft_ver = (val == 0xff) ? 0 : 1;

	RTW_INFO("%s bt_ft_ver: %d\n", __func__, hal_spec->bt_ft_ver);

	return _SUCCESS;
}

int hal_read_mac_hidden_rpt(struct adapter *adapter)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapter);
	int ret = _FAIL;
	int ret_fwdl;
	u8 mac_hidden_rpt[MAC_HIDDEN_RPT_LEN + MAC_HIDDEN_RPT_2_LEN] = {0};
	unsigned long start = rtw_get_current_time();
	u32 cnt = 0;
	u32 timeout_ms = 800;
	u32 min_cnt = 10;
	u8 id = C2H_DEFEATURE_RSVD;
	int i;

	u8 hci_type = rtw_get_intf_type(adapter);

	if ((hci_type == RTW_USB || hci_type == RTW_PCIE)
		&& !rtw_is_hw_init_completed(adapter))
		rtw_hal_power_on(adapter);

	/* inform FW mac hidden rpt from reg is needed */
	rtw_write8(adapter, REG_C2HEVT_MSG_NORMAL, C2H_DEFEATURE_RSVD);

	/* download FW */
	pHalData->not_xmitframe_fw_dl = 1;
	ret_fwdl = rtw_hal_fw_dl(adapter, false);
	pHalData->not_xmitframe_fw_dl = 0;
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
	c2h_mac_hidden_rpt_bt_info_hdl(adapter, rtw_read8(adapter, REG_C2HEVT_MSG_NORMAL + 1));

	if (ret_fwdl == _SUCCESS && id == C2H_MAC_HIDDEN_RPT)
		ret = _SUCCESS;
	if ((hci_type == RTW_USB || hci_type == RTW_PCIE)
		&& !rtw_is_hw_init_completed(adapter))
		rtw_hal_power_off(adapter);
	RTW_INFO("%s %s! (%u, %dms), fwdl:%d, id:0x%02x\n", __func__
		, (ret == _SUCCESS) ? "OK" : "Fail", cnt, rtw_get_passing_time_ms(start), ret_fwdl, id);
	return ret;
}

int c2h_defeature_dbg_hdl(struct adapter *adapter, u8 *data, u8 len)
{
	int ret = _FAIL;
	int i;

	if (len < DEFEATURE_DBG_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, DEFEATURE_DBG_LEN);
		goto exit;
	}

	for (i = 0; i < len; i++)
		RTW_PRINT("%s: 0x%02X\n", __func__, *(data + i));

	ret = _SUCCESS;
	
exit:
	return ret;
}

#ifndef DBG_CUSTOMER_STR_RPT_HANDLE
#define DBG_CUSTOMER_STR_RPT_HANDLE 0
#endif

int rtw_hal_h2c_customer_str_req(struct adapter *adapter)
{
	u8 h2c_data[H2C_CUSTOMER_STR_REQ_LEN] = {0};

	SET_H2CCMD_CUSTOMER_STR_REQ_EN(h2c_data, 1);
	return rtw_hal_fill_h2c_cmd(adapter, H2C_CUSTOMER_STR_REQ, H2C_CUSTOMER_STR_REQ_LEN, h2c_data);
}

#define	C2H_CUSTOMER_STR_RPT_BYTE0(_data)		((u8 *)(_data))
#define	C2H_CUSTOMER_STR_RPT_2_BYTE8(_data)		((u8 *)(_data))

int c2h_customer_str_rpt_hdl(struct adapter *adapter, u8 *data, u8 len)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	int ret = _FAIL;

	if (len < CUSTOMER_STR_RPT_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, CUSTOMER_STR_RPT_LEN);
		goto exit;
	}

	if (DBG_CUSTOMER_STR_RPT_HANDLE)
		RTW_PRINT_DUMP("customer_str_rpt: ", data, CUSTOMER_STR_RPT_LEN);

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);

	if (dvobj->customer_str_sctx) {
		if (dvobj->customer_str_sctx->status != RTW_SCTX_SUBMITTED)
			RTW_WARN("%s invalid sctx.status:%d\n", __func__, dvobj->customer_str_sctx->status);
		memcpy(dvobj->customer_str,  C2H_CUSTOMER_STR_RPT_BYTE0(data), CUSTOMER_STR_RPT_LEN);
		dvobj->customer_str_sctx->status = RTX_SCTX_CSTR_WAIT_RPT2;
	} else
		RTW_WARN("%s sctx not set\n", __func__);

	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

	ret = _SUCCESS;

exit:
	return ret;
}

int c2h_customer_str_rpt_2_hdl(struct adapter *adapter, u8 *data, u8 len)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	int ret = _FAIL;

	if (len < CUSTOMER_STR_RPT_2_LEN) {
		RTW_WARN("%s len(%u) < %d\n", __func__, len, CUSTOMER_STR_RPT_2_LEN);
		goto exit;
	}

	if (DBG_CUSTOMER_STR_RPT_HANDLE)
		RTW_PRINT_DUMP("customer_str_rpt_2: ", data, CUSTOMER_STR_RPT_2_LEN);

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);

	if (dvobj->customer_str_sctx) {
		if (dvobj->customer_str_sctx->status != RTX_SCTX_CSTR_WAIT_RPT2)
			RTW_WARN("%s rpt not ready\n", __func__);
		memcpy(dvobj->customer_str + CUSTOMER_STR_RPT_LEN,  C2H_CUSTOMER_STR_RPT_2_BYTE8(data), CUSTOMER_STR_RPT_2_LEN);
		rtw_sctx_done(&dvobj->customer_str_sctx);
	} else
		RTW_WARN("%s sctx not set\n", __func__);

	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

	ret = _SUCCESS;

exit:
	return ret;
}

/* read customer str */
int rtw_hal_customer_str_read(struct adapter *adapter, u8 *cs)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct submit_ctx sctx;
	int ret = _SUCCESS;

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
	if (dvobj->customer_str_sctx)
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
		memcpy(cs, dvobj->customer_str, RTW_CUSTOMER_STR_LEN);
	else
		ret = _FAIL;
	_exit_critical_mutex(&dvobj->customer_str_mutex, NULL);

exit:
	return ret;
}

int rtw_hal_h2c_customer_str_write(struct adapter *adapter, const u8 *cs)
{
	u8 h2c_data_w1[H2C_CUSTOMER_STR_W1_LEN] = {0};
	u8 h2c_data_w2[H2C_CUSTOMER_STR_W2_LEN] = {0};
	u8 h2c_data_w3[H2C_CUSTOMER_STR_W3_LEN] = {0};
	int ret;

	SET_H2CCMD_CUSTOMER_STR_W1_EN(h2c_data_w1, 1);
	memcpy(H2CCMD_CUSTOMER_STR_W1_BYTE0(h2c_data_w1), cs, 6);

	SET_H2CCMD_CUSTOMER_STR_W2_EN(h2c_data_w2, 1);
	memcpy(H2CCMD_CUSTOMER_STR_W2_BYTE6(h2c_data_w2), cs + 6, 6);

	SET_H2CCMD_CUSTOMER_STR_W3_EN(h2c_data_w3, 1);
	memcpy(H2CCMD_CUSTOMER_STR_W3_BYTE12(h2c_data_w3), cs + 6 + 6, 4);

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
int rtw_hal_customer_str_write(struct adapter *adapter, const u8 *cs)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct submit_ctx sctx;
	int ret = _SUCCESS;

	_enter_critical_mutex(&dvobj->customer_str_mutex, NULL);
	if (dvobj->customer_str_sctx) {
		ret = _FAIL;
	} else {
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
		if (memcmp(cs, dvobj->customer_str, RTW_CUSTOMER_STR_LEN)) {
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

void rtw_hal_update_sta_wset(struct adapter *adapter, struct sta_info *psta)
{
	u8 w_set = 0;

	if (psta->wireless_mode & WIRELESS_11B)
		w_set |= WIRELESS_CCK;

	if ((psta->wireless_mode & WIRELESS_11G) || (psta->wireless_mode & WIRELESS_11A))
		w_set |= WIRELESS_OFDM;

	if (psta->wireless_mode & WIRELESS_11_24N)
		w_set |= WIRELESS_HT;

	if ((psta->wireless_mode & WIRELESS_11AC) || (psta->wireless_mode & WIRELESS_11_5N))
		w_set |= WIRELESS_VHT;

	psta->cmn.support_wireless_set = w_set;
}

static void rtw_hal_update_sta_mimo_type(struct adapter *adapter, struct sta_info *psta)
{
	s8 tx_nss, rx_nss;

	tx_nss = rtw_get_sta_tx_nss(adapter, psta);
	rx_nss =  rtw_get_sta_rx_nss(adapter, psta);
	if ((tx_nss == 1) && (rx_nss == 1))
		psta->cmn.mimo_type = RF_1T1R;
	else if ((tx_nss == 1) && (rx_nss == 2))
		psta->cmn.mimo_type = RF_1T2R;
	else if ((tx_nss == 2) && (rx_nss == 2))
		psta->cmn.mimo_type = RF_2T2R;
	else if ((tx_nss == 2) && (rx_nss == 3))
		psta->cmn.mimo_type = RF_2T3R;
	else if ((tx_nss == 2) && (rx_nss == 4))
		psta->cmn.mimo_type = RF_2T4R;
	else if ((tx_nss == 3) && (rx_nss == 3))
		psta->cmn.mimo_type = RF_3T3R;
	else if ((tx_nss == 3) && (rx_nss == 4))
		psta->cmn.mimo_type = RF_3T4R;
	else if ((tx_nss == 4) && (rx_nss == 4))
		psta->cmn.mimo_type = RF_4T4R;
	else
		rtw_warn_on(1);

	RTW_INFO("STA - MAC_ID:%d, Tx - %d SS, Rx - %d SS\n",
			psta->cmn.mac_id, tx_nss, rx_nss);
}

static void rtw_hal_update_sta_smps_cap(struct adapter *adapter, struct sta_info *psta)
{
	/*Spatial Multiplexing Power Save*/
	psta->cmn.sm_ps = SM_PS_DISABLE;

	RTW_INFO("STA - MAC_ID:%d, SM_PS %d\n",
			psta->cmn.mac_id, psta->cmn.sm_ps);
}

u8 rtw_get_mgntframe_raid(struct adapter *adapter, unsigned char network_type)
{
	return (network_type & WIRELESS_11B) ? RATR_INX_WIRELESS_B :
					       RATR_INX_WIRELESS_G;
}

static void rtw_hal_update_sta_rate_mask(struct adapter * adapt, struct sta_info *psta)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapt);
	u8 i, rf_type, tx_nss;
	u64 tx_ra_bitmap = 0;

	if (!psta)
		return;

	/* b/g mode ra_bitmap  */
	for (i = 0; i < sizeof(psta->bssrateset); i++) {
		if (psta->bssrateset[i])
			tx_ra_bitmap |= rtw_get_bit_value_from_ieee_value(psta->bssrateset[i] & 0x7f);
	}

	rtw_hal_get_hwreg(adapt, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	tx_nss = rtw_min(rf_type_to_rf_tx_cnt(rf_type), hal_spec->tx_nss_num);
	if (psta->htpriv.ht_option) {
		/* n mode ra_bitmap */

		/* Handling SMPS mode for AP MODE only*/
		if (check_fwstate(&adapt->mlmepriv, WIFI_AP_STATE)) {
			/*0:static SMPS, 1:dynamic SMPS, 3:SMPS disabled, 2:reserved*/
			if (psta->htpriv.smps_cap == 0 || psta->htpriv.smps_cap == 1) {
				/*operate with only one active receive chain // 11n-MCS rate <= MSC7*/
				tx_nss = rtw_min(tx_nss, 1);
			}
		}

		tx_ra_bitmap |= (rtw_ht_mcs_set_to_bitmap(psta->htpriv.ht_cap.supp_mcs_set, tx_nss) << 12);
	}
	psta->cmn.ra_info.ramask = tx_ra_bitmap;
	psta->init_rate = get_highest_rate_idx(tx_ra_bitmap) & 0x3f;
}

void rtw_hal_update_sta_ra_info(struct adapter * adapt, struct sta_info *psta)
{
	rtw_hal_update_sta_mimo_type(adapt, psta);
	rtw_hal_update_sta_smps_cap(adapt, psta);
	rtw_hal_update_sta_rate_mask(adapt, psta);
}

#ifndef SEC_CAM_ACCESS_TIMEOUT_MS
	#define SEC_CAM_ACCESS_TIMEOUT_MS 200
#endif

#ifndef DBG_SEC_CAM_ACCESS
	#define DBG_SEC_CAM_ACCESS 0
#endif

u32 rtw_sec_read_cam(struct adapter *adapter, u8 addr)
{
	_mutex *mutex = &adapter_to_dvobj(adapter)->cam_ctl.sec_cam_access_mutex;
	u32 rdata;
	u32 cnt = 0;
	unsigned long start = 0, end = 0;
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
		RTW_INFO(FUNC_ADPT_FMT" addr:0x%02x, rdata:0x%08x, to:%u, polling:%u, %d ms\n"
			, FUNC_ADPT_ARG(adapter), addr, rdata, timeout, cnt, rtw_get_time_interval_ms(start, end));
	}

	return rdata;
}

void rtw_sec_write_cam(struct adapter *adapter, u8 addr, u32 wdata)
{
	_mutex *mutex = &adapter_to_dvobj(adapter)->cam_ctl.sec_cam_access_mutex;
	u32 cnt = 0;
	unsigned long start = 0, end = 0;
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
		RTW_INFO(FUNC_ADPT_FMT" addr:0x%02x, wdata:0x%08x, to:%u, polling:%u, %d ms\n"
			, FUNC_ADPT_ARG(adapter), addr, wdata, timeout, cnt, rtw_get_time_interval_ms(start, end));
	}
}

void rtw_sec_read_cam_ent(struct adapter *adapter, u8 id, u8 *ctrl, u8 *mac, u8 *key)
{
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
				memcpy(ctrl, (u8 *)(&rdata), 2);
			if (mac)
				memcpy(mac, ((u8 *)(&rdata)) + 2, 2);
			break;
		case 1:
			if (mac)
				memcpy(mac + 2, (u8 *)(&rdata), 4);
			break;
		default:
			if (key)
				memcpy(key + (i - 2) * 4, (u8 *)(&rdata), 4);
			break;
		}
	}

exit:
	return;
}


void rtw_sec_write_cam_ent(struct adapter *adapter, u8 id, u16 ctrl, u8 *mac, u8 *key)
{
	unsigned int i;
	int j;
	u8 addr;
	u32 wdata;

	j = 7;

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

void rtw_sec_clr_cam_ent(struct adapter *adapter, u8 id)
{
	u8 addr;

	addr = (id << 3);
	rtw_sec_write_cam(adapter, addr, 0);
}

bool rtw_sec_read_cam_is_gk(struct adapter *adapter, u8 id)
{
	bool res;
	u16 ctrl;

	rtw_sec_read_cam_ent(adapter, id, (u8 *)&ctrl, NULL, NULL);

	res = (ctrl & BIT6) ? true : false;
	return res;
}
#ifdef CONFIG_MI_WITH_MBSSID_CAM
void rtw_mbid_cam_init(struct dvobj_priv *dvobj)
{
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	spin_lock_init(&mbid_cam_ctl->lock);
	mbid_cam_ctl->bitmap = 0;
	ATOMIC_SET(&mbid_cam_ctl->mbid_entry_num, 0);
	memset(&dvobj->mbid_cam_cache, 0, sizeof(dvobj->mbid_cam_cache));
}

void rtw_mbid_cam_deinit(struct dvobj_priv *dvobj)
{
}

void rtw_mbid_cam_reset(struct adapter *adapter)
{
	unsigned long irqL;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	mbid_cam_ctl->bitmap = 0;
	memset(&dvobj->mbid_cam_cache, 0, sizeof(dvobj->mbid_cam_cache));
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);

	ATOMIC_SET(&mbid_cam_ctl->mbid_entry_num, 0);
}
static u8 _rtw_mbid_cam_search_by_macaddr(struct adapter *adapter, u8 *mac_addr)
{
	u8 i;
	u8 cam_id = INVALID_CAM_ID;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	for (i = 0; i < TOTAL_MBID_CAM_NUM; i++) {
		if (mac_addr && !memcmp(dvobj->mbid_cam_cache[i].mac_addr, mac_addr, ETH_ALEN)) {
			cam_id = i;
			break;
		}
	}

	RTW_INFO("%s mac:"MAC_FMT" - cam_id:%d\n", __func__, MAC_ARG(mac_addr), cam_id);
	return cam_id;
}

u8 rtw_mbid_cam_search_by_macaddr(struct adapter *adapter, u8 *mac_addr)
{
	unsigned long irqL;

	u8 cam_id = INVALID_CAM_ID;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	cam_id = _rtw_mbid_cam_search_by_macaddr(adapter, mac_addr);
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);

	return cam_id;
}
static u8 _rtw_mbid_cam_search_by_ifaceid(struct adapter *adapter, u8 iface_id)
{
	u8 i;
	u8 cam_id = INVALID_CAM_ID;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	for (i = 0; i < TOTAL_MBID_CAM_NUM; i++) {
		if (iface_id == dvobj->mbid_cam_cache[i].iface_id) {
			cam_id = i;
			break;
		}
	}
	if (cam_id != INVALID_CAM_ID)
		RTW_INFO("%s iface_id:%d mac:"MAC_FMT" - cam_id:%d\n",
			__func__, iface_id, MAC_ARG(dvobj->mbid_cam_cache[cam_id].mac_addr), cam_id);

	return cam_id;
}

u8 rtw_mbid_cam_search_by_ifaceid(struct adapter *adapter, u8 iface_id)
{
	unsigned long irqL;
	u8 cam_id = INVALID_CAM_ID;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	cam_id = _rtw_mbid_cam_search_by_ifaceid(adapter, iface_id);
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);

	return cam_id;
}
u8 rtw_get_max_mbid_cam_id(struct adapter *adapter)
{
	unsigned long irqL;
	s8 i;
	u8 cam_id = INVALID_CAM_ID;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	for (i = (TOTAL_MBID_CAM_NUM - 1); i >= 0; i--) {
		if (mbid_cam_ctl->bitmap & BIT(i)) {
			cam_id = i;
			break;
		}
	}
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);
	/*RTW_INFO("%s max cam_id:%d\n", __func__, cam_id);*/
	return cam_id;
}

inline u8 rtw_get_mbid_cam_entry_num(struct adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	return ATOMIC_READ(&mbid_cam_ctl->mbid_entry_num);
}

static inline void mbid_cam_cache_init(struct adapter *adapter, struct mbid_cam_cache *pmbid_cam, u8 *mac_addr)
{
	if (adapter && pmbid_cam && mac_addr) {
		memcpy(pmbid_cam->mac_addr, mac_addr, ETH_ALEN);
		pmbid_cam->iface_id = adapter->iface_id;
	}
}
static inline void mbid_cam_cache_clr(struct mbid_cam_cache *pmbid_cam)
{
	if (pmbid_cam) {
		memset(pmbid_cam->mac_addr, 0, ETH_ALEN);
		pmbid_cam->iface_id = CONFIG_IFACE_NUMBER;
	}
}

u8 rtw_mbid_camid_alloc(struct adapter *adapter, u8 *mac_addr)
{
	unsigned long irqL;
	u8 cam_id = INVALID_CAM_ID, i;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;
	u8 entry_num = ATOMIC_READ(&mbid_cam_ctl->mbid_entry_num);

	if (entry_num >= TOTAL_MBID_CAM_NUM) {
		RTW_INFO(FUNC_ADPT_FMT" failed !! MBSSID number :%d over TOTAL_CAM_ENTRY(8)\n", FUNC_ADPT_ARG(adapter), entry_num);
		rtw_warn_on(1);
	}

	if (INVALID_CAM_ID != rtw_mbid_cam_search_by_macaddr(adapter, mac_addr))
		goto exit;

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	for (i = 0; i < TOTAL_MBID_CAM_NUM; i++) {
		if (!(mbid_cam_ctl->bitmap & BIT(i))) {
			mbid_cam_ctl->bitmap |= BIT(i);
			cam_id = i;
			break;
		}
	}
	if ((cam_id != INVALID_CAM_ID) && (mac_addr))
		mbid_cam_cache_init(adapter, &dvobj->mbid_cam_cache[cam_id], mac_addr);
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);

	if (cam_id != INVALID_CAM_ID) {
		ATOMIC_INC(&mbid_cam_ctl->mbid_entry_num);
		RTW_INFO("%s mac:"MAC_FMT" - cam_id:%d\n", __func__, MAC_ARG(mac_addr), cam_id);
#ifdef CONFIG_MI_WITH_MBSSID_CAM
		rtw_mbid_cam_cache_dump(RTW_DBGDUMP, __func__, adapter);
#endif
	} else
		RTW_INFO("%s [WARN] "MAC_FMT" - invalid cam_id:%d\n", __func__, MAC_ARG(mac_addr), cam_id);
exit:
	return cam_id;
}

u8 rtw_mbid_cam_info_change(struct adapter *adapter, u8 *mac_addr)
{
	unsigned long irqL;
	u8 entry_id = INVALID_CAM_ID;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	entry_id = _rtw_mbid_cam_search_by_ifaceid(adapter, adapter->iface_id);
	if (entry_id != INVALID_CAM_ID)
		mbid_cam_cache_init(adapter, &dvobj->mbid_cam_cache[entry_id], mac_addr);

	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);

	return entry_id;
}

u8 rtw_mbid_cam_assign(struct adapter *adapter, u8 *mac_addr, u8 camid)
{
	unsigned long irqL;
	u8 ret = false;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	if ((camid >= TOTAL_MBID_CAM_NUM) || (camid == INVALID_CAM_ID)) {
		RTW_INFO(FUNC_ADPT_FMT" failed !! invlaid mbid_canid :%d\n", FUNC_ADPT_ARG(adapter), camid);
		rtw_warn_on(1);
	}
	if (INVALID_CAM_ID != rtw_mbid_cam_search_by_macaddr(adapter, mac_addr))
		goto exit;

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	if (!(mbid_cam_ctl->bitmap & BIT(camid))) {
		if (mac_addr) {
			mbid_cam_ctl->bitmap |= BIT(camid);
			mbid_cam_cache_init(adapter, &dvobj->mbid_cam_cache[camid], mac_addr);
			ret = true;
		}
	}
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);

	if (ret) {
		ATOMIC_INC(&mbid_cam_ctl->mbid_entry_num);
		RTW_INFO("%s mac:"MAC_FMT" - cam_id:%d\n", __func__, MAC_ARG(mac_addr), camid);
#ifdef CONFIG_MI_WITH_MBSSID_CAM
		rtw_mbid_cam_cache_dump(RTW_DBGDUMP, __func__, adapter);
#endif
	} else
		RTW_INFO("%s  [WARN] mac:"MAC_FMT" - cam_id:%d assigned failed\n", __func__, MAC_ARG(mac_addr), camid);

exit:
	return ret;
}

void rtw_mbid_camid_clean(struct adapter *adapter, u8 mbss_canid)
{
	unsigned long irqL;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	if ((mbss_canid >= TOTAL_MBID_CAM_NUM) || (mbss_canid == INVALID_CAM_ID)) {
		RTW_INFO(FUNC_ADPT_FMT" failed !! invlaid mbid_canid :%d\n", FUNC_ADPT_ARG(adapter), mbss_canid);
		rtw_warn_on(1);
	}
	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	mbid_cam_cache_clr(&dvobj->mbid_cam_cache[mbss_canid]);
	mbid_cam_ctl->bitmap &= (~BIT(mbss_canid));
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);
	ATOMIC_DEC(&mbid_cam_ctl->mbid_entry_num);
	RTW_INFO("%s - cam_id:%d\n", __func__, mbss_canid);
}
int rtw_mbid_cam_cache_dump(void *sel, const char *fun_name, struct adapter *adapter)
{
	unsigned long irqL;
	u8 i;
	struct adapter *iface;
	u8 iface_id;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;
	u8 entry_num = ATOMIC_READ(&mbid_cam_ctl->mbid_entry_num);
	u8 max_cam_id = rtw_get_max_mbid_cam_id(adapter);

	RTW_PRINT_SEL(sel, "== MBSSID CAM DUMP (%s)==\n", fun_name);

	_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);
	RTW_PRINT_SEL(sel, "Entry numbers:%d, max_camid:%d, bitmap:0x%08x\n", entry_num, max_cam_id, mbid_cam_ctl->bitmap);
	for (i = 0; i < TOTAL_MBID_CAM_NUM; i++) {
		RTW_PRINT_SEL(sel, "CAM_ID = %d\t", i);

		if (mbid_cam_ctl->bitmap & BIT(i)) {
			iface_id = dvobj->mbid_cam_cache[i].iface_id;
			RTW_PRINT_SEL(sel, "IF_ID:%d\t", iface_id);
			RTW_PRINT_SEL(sel, "MAC Addr:"MAC_FMT"\t", MAC_ARG(dvobj->mbid_cam_cache[i].mac_addr));

			iface = dvobj->adapters[iface_id];
			if (iface) {
				if (MLME_IS_STA(iface))
					RTW_PRINT_SEL(sel, "ROLE:%s\n", "STA");
				else if (MLME_IS_AP(iface))
					RTW_PRINT_SEL(sel, "ROLE:%s\n", "AP");
				else if (MLME_IS_MESH(iface))
					RTW_PRINT_SEL(sel, "ROLE:%s\n", "MESH");
				else
					RTW_PRINT_SEL(sel, "ROLE:%s\n", "NONE");
			}

		} else
			RTW_PRINT_SEL(sel, "N/A\n");
	}
	_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);
	return 0;
}

static void read_mbssid_cam(struct adapter *adapt, u8 cam_addr, u8 *mac)
{
	u8 poll = 1;
	u8 cam_ready = false;
	u32 cam_data1 = 0;
	u16 cam_data2 = 0;

	if (RTW_CANNOT_RUN(adapt))
		return;

	rtw_write32(adapt, REG_MBIDCAMCFG_2, BIT_MBIDCAM_POLL | ((cam_addr & MBIDCAM_ADDR_MASK) << MBIDCAM_ADDR_SHIFT));

	do {
		if (0 == (rtw_read32(adapt, REG_MBIDCAMCFG_2) & BIT_MBIDCAM_POLL)) {
			cam_ready = true;
			break;
		}
		poll++;
	} while ((poll % 10) != 0 && !RTW_CANNOT_RUN(adapt));

	if (cam_ready) {
		cam_data1 = rtw_read32(adapt, REG_MBIDCAMCFG_1);
		mac[0] = cam_data1 & 0xFF;
		mac[1] = (cam_data1 >> 8) & 0xFF;
		mac[2] = (cam_data1 >> 16) & 0xFF;
		mac[3] = (cam_data1 >> 24) & 0xFF;

		cam_data2 = rtw_read16(adapt, REG_MBIDCAMCFG_2);
		mac[4] = cam_data2 & 0xFF;
		mac[5] = (cam_data2 >> 8) & 0xFF;
	}

}
int rtw_mbid_cam_dump(void *sel, const char *fun_name, struct adapter *adapter)
{
	/*unsigned long irqL;*/
	u8 i;
	u8 mac_addr[ETH_ALEN];

	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

	RTW_PRINT_SEL(sel, "\n== MBSSID HW-CAM DUMP (%s)==\n", fun_name);

	/*_enter_critical_bh(&mbid_cam_ctl->lock, &irqL);*/
	for (i = 0; i < TOTAL_MBID_CAM_NUM; i++) {
		RTW_PRINT_SEL(sel, "CAM_ID = %d\t", i);
		memset(mac_addr, 0, ETH_ALEN);
		read_mbssid_cam(adapter, i, mac_addr);
		RTW_PRINT_SEL(sel, "MAC Addr:"MAC_FMT"\n", MAC_ARG(mac_addr));
	}
	/*_exit_critical_bh(&mbid_cam_ctl->lock, &irqL);*/
	return 0;
}

static void write_mbssid_cam(struct adapter *adapt, u8 cam_addr, u8 *mac)
{
	u32	cam_val[2] = {0};

	cam_val[0] = (mac[3] << 24) | (mac[2] << 16) | (mac[1] << 8) | mac[0];
	cam_val[1] = ((cam_addr & MBIDCAM_ADDR_MASK) << MBIDCAM_ADDR_SHIFT)  | (mac[5] << 8) | mac[4];

	rtw_hal_set_hwreg(adapt, HW_VAR_MBSSID_CAM_WRITE, (u8 *)cam_val);
}

static void clear_mbssid_cam(struct adapter *adapt, u8 cam_addr)
{
	rtw_hal_set_hwreg(adapt, HW_VAR_MBSSID_CAM_CLEAR, &cam_addr);
}
static void enable_mbssid_cam(struct adapter *adapter)
{
	u8 max_cam_id = rtw_get_max_mbid_cam_id(adapter);
	/*enable MBSSID*/
	rtw_hal_rcr_add(adapter, RCR_ENMBID);
	if (max_cam_id != INVALID_CAM_ID) {
		rtw_write8(adapter, REG_MBID_NUM,
			((rtw_read8(adapter, REG_MBID_NUM) & 0xF8) | ((max_cam_id -1) & 0x07)));
	}
}
void rtw_mbid_cam_restore(struct adapter *adapter)
{
	u8 i;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct mbid_cam_ctl_t *mbid_cam_ctl = &dvobj->mbid_cam_ctl;

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_mbid_cam_cache_dump(RTW_DBGDUMP, __func__, adapter);
#endif

	for (i = 0; i < TOTAL_MBID_CAM_NUM; i++) {
		if (mbid_cam_ctl->bitmap & BIT(i)) {
			write_mbssid_cam(adapter, i, dvobj->mbid_cam_cache[i].mac_addr);
			RTW_INFO("%s - cam_id:%d => mac:"MAC_FMT"\n", __func__, i, MAC_ARG(dvobj->mbid_cam_cache[i].mac_addr));
		}
	}
	enable_mbssid_cam(adapter);
}
#endif /*CONFIG_MI_WITH_MBSSID_CAM*/

#ifdef CONFIG_MI_WITH_MBSSID_CAM
void rtw_hal_set_macaddr_mbid(struct adapter *adapter, u8 *mac_addr)
{
	/*
		MBID entry_id = 0~7 ,for IFACE_ID0 ~ IFACE_IDx
	*/
	u8 entry_id = rtw_mbid_camid_alloc(adapter, mac_addr);

	if (entry_id != INVALID_CAM_ID) {
		write_mbssid_cam(adapter, entry_id, mac_addr);
		enable_mbssid_cam(adapter);
	}
}

void rtw_hal_change_macaddr_mbid(struct adapter *adapter, u8 *mac_addr)
{
	u8 idx = 0;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	u8 entry_id;

	if (!mac_addr) {
		rtw_warn_on(1);
		return;
	}


	entry_id = rtw_mbid_cam_info_change(adapter, mac_addr);

	if (entry_id != INVALID_CAM_ID)
		write_mbssid_cam(adapter, entry_id, mac_addr);
}

#endif/*#ifdef CONFIG_MI_WITH_MBSSID_CAM*/

static void rtw_hal_set_macaddr_port(struct adapter *adapter, u8 *val)
{
	u8 idx = 0;
	u32 reg_macid = 0;

	if (!val)
		return;

	RTW_INFO("%s "ADPT_FMT"- hw port(%d) mac_addr ="MAC_FMT"\n",  __func__,
		 ADPT_ARG(adapter), adapter->hw_port, MAC_ARG(val));

	switch (adapter->hw_port) {
	case HW_PORT0:
	default:
		reg_macid = REG_MACID;
		break;
	case HW_PORT1:
		reg_macid = REG_MACID1;
		break;
	}

	for (idx = 0; idx < 6; idx++)
		rtw_write8(GET_PRIMARY_ADAPTER(adapter), (reg_macid + idx), val[idx]);
}

static void rtw_hal_get_macaddr_port(struct adapter *adapter, u8 *mac_addr)
{
	u8 idx = 0;
	u32 reg_macid = 0;

	if (!mac_addr)
		return;

	memset(mac_addr, 0, ETH_ALEN);
	switch (adapter->hw_port) {
	case HW_PORT0:
	default:
		reg_macid = REG_MACID;
		break;
	case HW_PORT1:
		reg_macid = REG_MACID1;
		break;
	}

	for (idx = 0; idx < 6; idx++)
		mac_addr[idx] = rtw_read8(GET_PRIMARY_ADAPTER(adapter), (reg_macid + idx));

	RTW_INFO("%s "ADPT_FMT"- hw port(%d) mac_addr ="MAC_FMT"\n",  __func__,
		 ADPT_ARG(adapter), adapter->hw_port, MAC_ARG(mac_addr));
}

static void rtw_hal_set_bssid(struct adapter *adapter, u8 *val)
{
	u8	idx = 0;
	u32 reg_bssid = 0;

	switch (adapter->hw_port) {
	case HW_PORT0:
	default:
		reg_bssid = REG_BSSID;
		break;
	case HW_PORT1:
		reg_bssid = REG_BSSID1;
		break;
	}

	for (idx = 0 ; idx < 6; idx++)
		rtw_write8(adapter, (reg_bssid + idx), val[idx]);

	RTW_INFO("%s "ADPT_FMT"- hw port -%d BSSID: "MAC_FMT"\n", __func__, ADPT_ARG(adapter), adapter->hw_port, MAC_ARG(val));
}

static inline u8 hw_var_rcr_config(struct adapter *adapter, u32 rcr)
{
	int err;

	err = rtw_write32(adapter, REG_RCR, rcr);
	if (err == _SUCCESS)
		GET_HAL_DATA(adapter)->ReceiveConfig = rcr;
	return err;
}

static inline u8 hw_var_rcr_get(struct adapter *adapter, u32 *rcr)
{
	u32 v32;

	v32 = rtw_read32(adapter, REG_RCR);
	if (rcr)
		*rcr = v32;
	GET_HAL_DATA(adapter)->ReceiveConfig = v32;
	return _SUCCESS;
}

/* only check SW RCR variable */
inline u8 rtw_hal_rcr_check(struct adapter *adapter, u32 check_bit)
{
	struct hal_com_data * hal;
	u32 rcr;

	hal = GET_HAL_DATA(adapter);

	rcr = hal->ReceiveConfig;
	if ((rcr & check_bit) == check_bit)
		return 1;

	return 0;
}

inline u8 rtw_hal_rcr_add(struct adapter *adapter, u32 add)
{
	struct hal_com_data * hal;
	u32 rcr;
	u8 ret = _SUCCESS;

	hal = GET_HAL_DATA(adapter);

	rtw_hal_get_hwreg(adapter, HW_VAR_RCR, (u8 *)&rcr);
	rcr |= add;
	if (rcr != hal->ReceiveConfig)
		ret = rtw_hal_set_hwreg(adapter, HW_VAR_RCR, (u8 *)&rcr);

	return ret;
}

inline u8 rtw_hal_rcr_clear(struct adapter *adapter, u32 clear)
{
	struct hal_com_data * hal;
	u32 rcr;
	u8 ret = _SUCCESS;

	hal = GET_HAL_DATA(adapter);

	rtw_hal_get_hwreg(adapter, HW_VAR_RCR, (u8 *)&rcr);
	rcr &= ~clear;
	if (rcr != hal->ReceiveConfig)
		ret = rtw_hal_set_hwreg(adapter, HW_VAR_RCR, (u8 *)&rcr);

	return ret;
}

void rtw_hal_rcr_set_chk_bssid(struct adapter *adapter, u8 self_action)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u32 rcr, rcr_new;
	struct mi_state mstate, mstate_s;

	rtw_hal_get_hwreg(adapter, HW_VAR_RCR, (u8 *)&rcr);
	rcr_new = rcr;

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rcr_new &= ~(RCR_CBSSID_BCN | RCR_CBSSID_DATA);
#else
	rtw_mi_status_no_self(adapter, &mstate);
	rtw_mi_status_no_others(adapter, &mstate_s);

	/* only adjust parameters interested */
	switch (self_action) {
	case MLME_SCAN_ENTER:
		mstate_s.scan_num = 1;
		mstate_s.scan_enter_num = 1;
		break;
	case MLME_SCAN_DONE:
		mstate_s.scan_enter_num = 0;
		break;
	case MLME_AP_STARTED:
		mstate_s.ap_num = 1;
		break;
	case MLME_AP_STOPPED:
		mstate_s.ap_num = 0;
		mstate_s.ld_ap_num = 0;
		break;
#ifdef CONFIG_RTW_MESH
	case MLME_MESH_STARTED:
		mstate_s.mesh_num = 1;
		break;
	case MLME_MESH_STOPPED:
		mstate_s.mesh_num = 0;
		mstate_s.ld_mesh_num = 0;
		break;
#endif
	case MLME_ACTION_NONE:
	case MLME_STA_CONNECTING:
	case MLME_ADHOC_STARTED:
		/* caller without effect of decision */
		break;
	default:
		rtw_warn_on(1);
	};

	rtw_mi_status_merge(&mstate, &mstate_s);

	if (MSTATE_AP_NUM(&mstate) || MSTATE_MESH_NUM(&mstate) || MSTATE_TDLS_LD_NUM(&mstate)
		|| hal_data->in_cta_test
	)
		rcr_new &= ~RCR_CBSSID_DATA;
	else
		rcr_new |= RCR_CBSSID_DATA;

	if ((MSTATE_AP_NUM(&mstate) && adapter->registrypriv.wifi_spec) /* for 11n Logo 4.2.31/4.2.32 */
		|| MSTATE_MESH_NUM(&mstate)
		|| MSTATE_SCAN_ENTER_NUM(&mstate)
		|| hal_data->in_cta_test
	)
		rcr_new &= ~RCR_CBSSID_BCN;	
	else
		rcr_new |= RCR_CBSSID_BCN;
#endif /* CONFIG_MI_WITH_MBSSID_CAM */

	if (rcr != rcr_new)
		rtw_hal_set_hwreg(adapter, HW_VAR_RCR, (u8 *)&rcr_new);
}

static void hw_var_set_rcr_am(struct adapter *adapter, u8 enable)
{
	u32 rcr = RCR_AM;

	if (enable)
		rtw_hal_rcr_add(adapter, rcr);
	else
		rtw_hal_rcr_clear(adapter, rcr);
}

static void rtw_hal_get_msr(struct adapter *adapter, u8 *net_type)
{
	switch (adapter->hw_port) {
	case HW_PORT0:
		/*REG_CR - BIT[17:16]-Network Type for port 1*/
		*net_type = rtw_read8(adapter, MSR) & 0x03;
		break;
	case HW_PORT1:
		/*REG_CR - BIT[19:18]-Network Type for port 1*/
		*net_type = (rtw_read8(adapter, MSR) & 0x0C) >> 2;
		break;
	default:
		RTW_INFO("[WARN] "ADPT_FMT"- invalid hw port -%d\n",
			 ADPT_ARG(adapter), adapter->hw_port);
		rtw_warn_on(1);
		break;
	}
}

#if defined(CONFIG_MI_WITH_MBSSID_CAM) && defined(CONFIG_MI_WITH_MBSSID_CAM) /*For 2 hw ports - 88E/92E/8812/8821/8723B*/
static u8 rtw_hal_net_type_decision(struct adapter *adapter, u8 net_type)
{
	if ((adapter->hw_port == HW_PORT0) && (rtw_get_mbid_cam_entry_num(adapter))) {
		if (net_type != _HW_STATE_NOLINK_)
			return _HW_STATE_AP_;
	}
	return net_type;
}
#endif
static void rtw_hal_set_msr(struct adapter *adapter, u8 net_type)
{
	u8 val8 = 0;

	switch (adapter->hw_port) {
	case HW_PORT0:
		#if defined(CONFIG_MI_WITH_MBSSID_CAM) && defined(CONFIG_MI_WITH_MBSSID_CAM)
		net_type = rtw_hal_net_type_decision(adapter, net_type);
		#endif
		/*REG_CR - BIT[17:16]-Network Type for port 0*/
		val8 = rtw_read8(adapter, MSR) & 0x0C;
		val8 |= net_type;
		rtw_write8(adapter, MSR, val8);
		break;
	case HW_PORT1:
		/*REG_CR - BIT[19:18]-Network Type for port 1*/
		val8 = rtw_read8(adapter, MSR) & 0x03;
		val8 |= net_type << 2;
		rtw_write8(adapter, MSR, val8);
		break;
	default:
		RTW_INFO("[WARN] "ADPT_FMT"- invalid hw port -%d\n",
			 ADPT_ARG(adapter), adapter->hw_port);
		rtw_warn_on(1);
		break;
	}
}

static void hw_var_set_bcn_interval(struct adapter *a, u16 interval)
{
	RTW_ERR(FUNC_ADPT_FMT ": Not implemented yet!!\n", FUNC_ADPT_ARG(a));
	rtw_warn_on(1);
}

void hw_var_port_switch(struct adapter *adapter)
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

	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct adapter *iface = NULL;

	msr = rtw_read8(adapter, MSR);
	bcn_ctrl = rtw_read8(adapter, REG_BCN_CTRL);
	bcn_ctrl_1 = rtw_read8(adapter, REG_BCN_CTRL_1);

	for (i = 0; i < 2; i++)
		atimwnd[i] = rtw_read8(adapter, REG_ATIMWND + i);
	for (i = 0; i < 2; i++)
		atimwnd_1[i] = rtw_read8(adapter, REG_ATIMWND_1 + i);

	for (i = 0; i < 8; i++)
		tsftr[i] = rtw_read8(adapter, REG_TSFTR + i);
	for (i = 0; i < 8; i++)
		tsftr_1[i] = rtw_read8(adapter, REG_TSFTR1 + i);

	for (i = 0; i < 6; i++)
		macid[i] = rtw_read8(adapter, REG_MACID + i);

	for (i = 0; i < 6; i++)
		bssid[i] = rtw_read8(adapter, REG_BSSID + i);

	for (i = 0; i < 6; i++)
		macid_1[i] = rtw_read8(adapter, REG_MACID1 + i);

	for (i = 0; i < 6; i++)
		bssid_1[i] = rtw_read8(adapter, REG_BSSID1 + i);

	/* disable bcn function, disable update TSF */
	rtw_write8(adapter, REG_BCN_CTRL, (bcn_ctrl & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);
	rtw_write8(adapter, REG_BCN_CTRL_1, (bcn_ctrl_1 & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);

	/* switch msr */
	msr = (msr & 0xf0) | ((msr & 0x03) << 2) | ((msr & 0x0c) >> 2);
	rtw_write8(adapter, MSR, msr);

	/* write port0 */
	rtw_write8(adapter, REG_BCN_CTRL, bcn_ctrl_1 & ~EN_BCN_FUNCTION);
	for (i = 0; i < 2; i++)
		rtw_write8(adapter, REG_ATIMWND + i, atimwnd_1[i]);
	for (i = 0; i < 8; i++)
		rtw_write8(adapter, REG_TSFTR + i, tsftr_1[i]);
	for (i = 0; i < 6; i++)
		rtw_write8(adapter, REG_MACID + i, macid_1[i]);
	for (i = 0; i < 6; i++)
		rtw_write8(adapter, REG_BSSID + i, bssid_1[i]);

	/* write port1 */
	rtw_write8(adapter, REG_BCN_CTRL_1, bcn_ctrl & ~EN_BCN_FUNCTION);
	for (i = 0; i < 2; i++)
		rtw_write8(adapter, REG_ATIMWND_1 + i, atimwnd[i]);
	for (i = 0; i < 8; i++)
		rtw_write8(adapter, REG_TSFTR1 + i, tsftr[i]);
	for (i = 0; i < 6; i++)
		rtw_write8(adapter, REG_MACID1 + i, macid[i]);
	for (i = 0; i < 6; i++)
		rtw_write8(adapter, REG_BSSID1 + i, bssid[i]);

	/* write bcn ctl */
	/* always enable port0 beacon function for PSTDMA */
	bcn_ctrl_1 |= EN_BCN_FUNCTION;
	/* always disable port1 beacon function for PSTDMA */
	rtw_write8(adapter, REG_BCN_CTRL, bcn_ctrl_1);
	rtw_write8(adapter, REG_BCN_CTRL_1, bcn_ctrl);

	if (adapter->iface_id == IFACE_ID0)
		iface = dvobj->adapters[IFACE_ID1];
	else if (adapter->iface_id == IFACE_ID1)
		iface = dvobj->adapters[IFACE_ID0];


	if (adapter->hw_port == HW_PORT0) {
		adapter->hw_port = HW_PORT1;
		iface->hw_port = HW_PORT0;
		RTW_PRINT("port switch - port0("ADPT_FMT"), port1("ADPT_FMT")\n",
			  ADPT_ARG(iface), ADPT_ARG(adapter));
	} else {
		adapter->hw_port = HW_PORT0;
		iface->hw_port = HW_PORT1;
		RTW_PRINT("port switch - port0("ADPT_FMT"), port1("ADPT_FMT")\n",
			  ADPT_ARG(adapter), ADPT_ARG(iface));
	}

#endif /* CONFIG_RUNTIME_PORT_SWITCH */
#endif /* CONFIG_CONCURRENT_MODE */
}

const char *const _h2c_msr_role_str[] = {
	"RSVD",
	"STA",
	"AP",
	"GC",
	"GO",
	"TDLS",
	"ADHOC",
	"MESH",
	"INVALID",
};

#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
int rtw_hal_set_default_port_id_cmd(struct adapter *adapter, u8 mac_id)
{
	int ret = _SUCCESS;
	u8 parm[H2C_DEFAULT_PORT_ID_LEN] = {0};
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	SET_H2CCMD_DFTPID_PORT_ID(parm, adapter->hw_port);
	SET_H2CCMD_DFTPID_MAC_ID(parm, mac_id);

	RTW_DBG_DUMP("DFT port id parm:", parm, H2C_DEFAULT_PORT_ID_LEN);
	RTW_INFO("%s port_id :%d, mad_id:%d\n", __func__, adapter->hw_port, mac_id);

	ret = rtw_hal_fill_h2c_cmd(adapter, H2C_DEFAULT_PORT_ID, H2C_DEFAULT_PORT_ID_LEN, parm);
	dvobj->default_port_id = adapter->hw_port;

	return ret;
}
int rtw_set_default_port_id(struct adapter *adapter)
{
	int ret = _SUCCESS;
	struct sta_info		*psta;
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	if (adapter->hw_port == dvobj->default_port_id)
		return ret;

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
		psta = rtw_get_stainfo(&adapter->stapriv, get_bssid(pmlmepriv));
		if (psta)
			ret = rtw_hal_set_default_port_id_cmd(adapter, psta->cmn.mac_id);
	} else if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {

	} else {
	}

	return ret;
}
int rtw_set_ps_rsvd_page(struct adapter *adapter)
{
	int ret = _SUCCESS;
	u16 media_status_rpt = RT_MEDIA_CONNECT;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	if (adapter->hw_port == dvobj->default_port_id)
		return ret;

	rtw_hal_set_hwreg(adapter, HW_VAR_H2C_FW_JOINBSSRPT,
			  (u8 *)&media_status_rpt);

	return ret;
}

#endif

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
int rtw_hal_set_FwMediaStatusRpt_cmd(struct adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid, bool macid_ind, u8 macid_end)
{
	struct macid_ctl_t *macid_ctl = &adapter->dvobj->macid_ctl;
	u8 parm[H2C_MEDIA_STATUS_RPT_LEN] = {0};
	int i;
	int ret;

	SET_H2CCMD_MSRRPT_PARM_OPMODE(parm, opmode);
	SET_H2CCMD_MSRRPT_PARM_MACID_IND(parm, macid_ind);
	SET_H2CCMD_MSRRPT_PARM_MIRACAST(parm, miracast);
	SET_H2CCMD_MSRRPT_PARM_MIRACAST_SINK(parm, miracast_sink);
	SET_H2CCMD_MSRRPT_PARM_ROLE(parm, role);
	SET_H2CCMD_MSRRPT_PARM_MACID(parm, macid);
	SET_H2CCMD_MSRRPT_PARM_MACID_END(parm, macid_end);
#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
	SET_H2CCMD_MSRRPT_PARM_PORT_NUM(parm, adapter->hw_port);
#endif
	RTW_DBG_DUMP("MediaStatusRpt parm:", parm, H2C_MEDIA_STATUS_RPT_LEN);

	ret = rtw_hal_fill_h2c_cmd(adapter, H2C_MEDIA_STATUS_RPT, H2C_MEDIA_STATUS_RPT_LEN, parm);
	if (ret != _SUCCESS)
		goto exit;

	SET_H2CCMD_MSRRPT_PARM_MACID_IND(parm, 0);
	if (macid_ind == 0)
		macid_end = macid;

	for (i = macid; macid <= macid_end; macid++) {
		rtw_macid_ctl_set_h2c_msr(macid_ctl, macid, parm[0]);
		if (!opmode) {
			rtw_macid_ctl_set_bw(macid_ctl, macid, CHANNEL_WIDTH_20);
			rtw_macid_ctl_set_vht_en(macid_ctl, macid, 0);
			rtw_macid_ctl_set_rate_bmp0(macid_ctl, macid, 0);
			rtw_macid_ctl_set_rate_bmp1(macid_ctl, macid, 0);
		}
	}
	if (!opmode)
		rtw_update_tx_rate_bmp(adapter_to_dvobj(adapter));

exit:
	return ret;
}

inline int rtw_hal_set_FwMediaStatusRpt_single_cmd(struct adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid)
{
	return rtw_hal_set_FwMediaStatusRpt_cmd(adapter, opmode, miracast, miracast_sink, role, macid, 0, 0);
}

inline int rtw_hal_set_FwMediaStatusRpt_range_cmd(struct adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid, u8 macid_end)
{
	return rtw_hal_set_FwMediaStatusRpt_cmd(adapter, opmode, miracast, miracast_sink, role, macid, 1, macid_end);
}

static void rtw_hal_set_FwRsvdPage_cmd(struct adapter * adapt, struct rsvd_page * rsvdpageloc)
{
	u8	u1H2CRsvdPageParm[H2C_RSVDPAGE_LOC_LEN] = {0};
	u8	ret = 0;

	RTW_INFO("RsvdPageLoc: ProbeRsp=%d PsPoll=%d Null=%d QoSNull=%d BTNull=%d\n",
		 rsvdpageloc->LocProbeRsp, rsvdpageloc->LocPsPoll,
		 rsvdpageloc->LocNullData, rsvdpageloc->LocQosNull,
		 rsvdpageloc->LocBTQosNull);

	SET_H2CCMD_RSVD_PAGE_PROBE_RSP(u1H2CRsvdPageParm, rsvdpageloc->LocProbeRsp);
	SET_H2CCMD_RSVD_PAGE_PSPOLL(u1H2CRsvdPageParm, rsvdpageloc->LocPsPoll);
	SET_H2CCMD_RSVD_PAGE_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocNullData);
	SET_H2CCMD_RSVD_PAGE_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocQosNull);
	SET_H2CCMD_RSVD_PAGE_BT_QOS_NULL_DATA(u1H2CRsvdPageParm, rsvdpageloc->LocBTQosNull);

	ret = rtw_hal_fill_h2c_cmd(adapt,
				   H2C_RSVD_PAGE,
				   H2C_RSVDPAGE_LOC_LEN,
				   u1H2CRsvdPageParm);

}

int rtw_hal_get_rsvd_page(struct adapter *adapter, u32 page_offset,
	u32 page_num, u8 *buffer, u32 buffer_size)
{
	u32 addr = 0, size = 0, count = 0;
	u32 page_size = 0, data_low = 0, data_high = 0;
	u16 txbndy = 0, offset = 0;
	u8 i = 0;
	bool rst = false;

	rtw_hal_get_def_var(adapter, HAL_DEF_TX_PAGE_SIZE, &page_size);

	addr = page_offset * page_size;
	size = page_num * page_size;

	if (buffer_size < size) {
		RTW_ERR("%s buffer_size(%d) < get page total size(%d)\n",
			__func__, buffer_size, size);
		return rst;
	}
	txbndy = rtw_read8(adapter, REG_TDECTRL + 1);

	offset = (txbndy + page_offset) * page_size / 8;
	count = (buffer_size / 8) + 1;

	rtw_write8(adapter, REG_PKT_BUFF_ACCESS_CTRL, 0x69);

	for (i = 0 ; i < count ; i++) {
		rtw_write32(adapter, REG_PKTBUF_DBG_CTRL, offset + i);
		data_low = rtw_read32(adapter, REG_PKTBUF_DBG_DATA_L);
		data_high = rtw_read32(adapter, REG_PKTBUF_DBG_DATA_H);
		memcpy(buffer + (i * 8),
			&data_low, sizeof(data_low));
		memcpy(buffer + ((i * 8) + 4),
			&data_high, sizeof(data_high));
	}
	rtw_write8(adapter, REG_PKT_BUFF_ACCESS_CTRL, 0x0);
	rst = true;

	return rst;
}

void rtw_dump_rsvd_page(void *sel, struct adapter *adapter, u8 page_offset, u8 page_num)
{
	u32 page_size = 0;
	u8 *buffer = NULL;
	u32 buf_size = 0;

	if (page_num == 0)
		return;

	RTW_PRINT_SEL(sel, "======= RSVD PAGE DUMP =======\n");
	RTW_PRINT_SEL(sel, "page_offset:%d, page_num:%d\n", page_offset, page_num);

	rtw_hal_get_def_var(adapter, HAL_DEF_TX_PAGE_SIZE, &page_size);
	if (page_size) {
		buf_size = page_size * page_num;
		buffer = vzalloc(buf_size);

		if (buffer) {
			rtw_hal_get_rsvd_page(adapter, page_offset, page_num, buffer, buf_size);
			RTW_DUMP_SEL(sel, buffer, buf_size);
			vfree(buffer);
		} else
			RTW_PRINT_SEL(sel, "ERROR - rsvd_buf mem allocate failed\n");
	} else
			RTW_PRINT_SEL(sel, "ERROR - Tx page size is zero ??\n");

	RTW_PRINT_SEL(sel, "==========================\n");
}

void rtw_hal_construct_beacon(struct adapter *adapt,
				     u8 *pframe, u32 *pLength)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	__le16 *fctrl;
	u32 rate_len, pktlen;
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex		*cur_network = &(pmlmeinfo->network);
	u8	bc_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


	/* RTW_INFO("%s\n", __func__); */

	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	fctrl = &(pwlanhdr->frame_ctl);
	*(fctrl) = 0;

	memcpy(pwlanhdr->addr1, bc_addr, ETH_ALEN);
	memcpy(pwlanhdr->addr2, adapter_mac_addr(adapt), ETH_ALEN);
	memcpy(pwlanhdr->addr3, get_my_bssid(cur_network), ETH_ALEN);

	SetSeqNum(pwlanhdr, 0/*pmlmeext->mgnt_seq*/);
	/* pmlmeext->mgnt_seq++; */
	set_frame_sub_type(pframe, WIFI_BEACON);

	pframe += sizeof(struct rtw_ieee80211_hdr_3addr);
	pktlen = sizeof(struct rtw_ieee80211_hdr_3addr);

	/* timestamp will be inserted by hardware */
	pframe += 8;
	pktlen += 8;

	/* beacon interval: 2 bytes */
	memcpy(pframe, (unsigned char *)(rtw_get_beacon_interval_from_ie(cur_network->IEs)), 2);

	pframe += 2;
	pktlen += 2;

	/* capability info: 2 bytes */
	memcpy(pframe, (unsigned char *)(rtw_get_capability_from_ie(cur_network->IEs)), 2);

	pframe += 2;
	pktlen += 2;

	if ((pmlmeinfo->state & 0x03) == WIFI_FW_AP_STATE) {
		/* RTW_INFO("ie len=%d\n", cur_network->IELength); */
		pktlen += cur_network->IELength - sizeof(struct ndis_802_11_fixed_ies);
		memcpy(pframe, cur_network->IEs + sizeof(struct ndis_802_11_fixed_ies), pktlen);

		goto _ConstructBeacon;
	}

	/* below for ad-hoc mode */

	/* SSID */
	pframe = rtw_set_ie(pframe, _SSID_IE_, cur_network->Ssid.SsidLength, cur_network->Ssid.Ssid, &pktlen);

	/* supported rates... */
	rate_len = rtw_get_rateset_len(cur_network->SupportedRates);
	pframe = rtw_set_ie(pframe, _SUPPORTEDRATES_IE_, ((rate_len > 8) ? 8 : rate_len), cur_network->SupportedRates, &pktlen);

	/* DS parameter set */
	pframe = rtw_set_ie(pframe, _DSSET_IE_, 1, (unsigned char *)&(cur_network->Configuration.DSConfig), &pktlen);

	if ((pmlmeinfo->state & 0x03) == WIFI_FW_ADHOC_STATE) {
		u32 ATIMWindow;
		/* IBSS Parameter Set... */
		/* ATIMWindow = cur->Configuration.ATIMWindow; */
		ATIMWindow = 0;
		pframe = rtw_set_ie(pframe, _IBSS_PARA_IE_, 2, (unsigned char *)(&ATIMWindow), &pktlen);
	}


	/* todo: ERP IE */


	/* EXTERNDED SUPPORTED RATE */
	if (rate_len > 8)
		pframe = rtw_set_ie(pframe, _EXT_SUPPORTEDRATES_IE_, (rate_len - 8), (cur_network->SupportedRates + 8), &pktlen);


	/* todo:HT for adhoc */

_ConstructBeacon:

	if ((pktlen + TXDESC_SIZE) > 512) {
		RTW_INFO("beacon frame too large\n");
		return;
	}

	*pLength = pktlen;

	/* RTW_INFO("%s bcn_sz=%d\n", __func__, pktlen); */

}

static void rtw_hal_construct_PSPoll(struct adapter *adapt,
				     u8 *pframe, u32 *pLength)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	__le16 *fctrl;
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	/* RTW_INFO("%s\n", __func__); */

	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	/* Frame control. */
	fctrl = &(pwlanhdr->frame_ctl);
	*(fctrl) = 0;
	SetPwrMgt(fctrl);
	set_frame_sub_type(pframe, WIFI_PSPOLL);

	/* AID. */
	set_duration(pframe, (pmlmeinfo->aid | 0xc000));

	/* BSSID. */
	memcpy(pwlanhdr->addr1, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);

	/* TA. */
	memcpy(pwlanhdr->addr2, adapter_mac_addr(adapt), ETH_ALEN);

	*pLength = 16;
}

void rtw_hal_construct_NullFunctionData(
	struct adapter * adapt,
	u8		*pframe,
	u32		*pLength,
	u8		*StaAddr,
	u8		bQoS,
	u8		AC,
	u8		bEosp,
	u8		bForcePowerSave)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	__le16 *fctrl;
	u32 pktlen;
	struct mlme_priv		*pmlmepriv = &adapt->mlmepriv;
	struct wlan_network		*cur_network = &pmlmepriv->cur_network;
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u8 bssid[ETH_ALEN];

	/* RTW_INFO("%s:%d\n", __func__, bForcePowerSave); */

	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	fctrl = &pwlanhdr->frame_ctl;
	*(fctrl) = 0;
	if (bForcePowerSave)
		SetPwrMgt(fctrl);

	if (!StaAddr) {
		memcpy(bssid, adapter_mac_addr(adapt), ETH_ALEN);
		StaAddr = bssid;
	}

	switch (cur_network->network.InfrastructureMode) {
	case Ndis802_11Infrastructure:
		SetToDs(fctrl);
		memcpy(pwlanhdr->addr1, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);
		memcpy(pwlanhdr->addr2, adapter_mac_addr(adapt), ETH_ALEN);
		memcpy(pwlanhdr->addr3, StaAddr, ETH_ALEN);
		break;
	case Ndis802_11APMode:
		SetFrDs(fctrl);
		memcpy(pwlanhdr->addr1, StaAddr, ETH_ALEN);
		memcpy(pwlanhdr->addr2, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);
		memcpy(pwlanhdr->addr3, adapter_mac_addr(adapt), ETH_ALEN);
		break;
	case Ndis802_11IBSS:
	default:
		memcpy(pwlanhdr->addr1, StaAddr, ETH_ALEN);
		memcpy(pwlanhdr->addr2, adapter_mac_addr(adapt), ETH_ALEN);
		memcpy(pwlanhdr->addr3, get_my_bssid(&(pmlmeinfo->network)), ETH_ALEN);
		break;
	}

	SetSeqNum(pwlanhdr, 0);
	set_duration(pwlanhdr, 0);

	if (bQoS) {
		struct rtw_ieee80211_hdr_3addr_qos *pwlanqoshdr;

		set_frame_sub_type(pframe, WIFI_QOS_DATA_NULL);

		pwlanqoshdr = (struct rtw_ieee80211_hdr_3addr_qos *)pframe;
		SetPriority(&pwlanqoshdr->qc, AC);
		SetEOSP(&pwlanqoshdr->qc, bEosp);

		pktlen = sizeof(struct rtw_ieee80211_hdr_3addr_qos);
	} else {
		set_frame_sub_type(pframe, WIFI_DATA_NULL);

		pktlen = sizeof(struct rtw_ieee80211_hdr_3addr);
	}

	*pLength = pktlen;
}

static void rtw_hal_construct_ProbeRsp(struct adapter *adapt, u8 *pframe, u32 *pLength,
				u8 *StaAddr, bool bHideSSID)
{
	struct rtw_ieee80211_hdr	*pwlanhdr;
	__le16 *fctrl;
	u8 *mac, *bssid;
	u32 pktlen;
	struct mlme_ext_priv	*pmlmeext = &(adapt->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct wlan_bssid_ex  *cur_network = &(pmlmeinfo->network);

	/*RTW_INFO("%s\n", __func__);*/

	pwlanhdr = (struct rtw_ieee80211_hdr *)pframe;

	mac = adapter_mac_addr(adapt);
	bssid = cur_network->MacAddress;

	fctrl = &(pwlanhdr->frame_ctl);
	*(fctrl) = 0;
	memcpy(pwlanhdr->addr1, StaAddr, ETH_ALEN);
	memcpy(pwlanhdr->addr2, mac, ETH_ALEN);
	memcpy(pwlanhdr->addr3, bssid, ETH_ALEN);

	SetSeqNum(pwlanhdr, 0);
	set_frame_sub_type(fctrl, WIFI_PROBERSP);

	pktlen = sizeof(struct rtw_ieee80211_hdr_3addr);
	pframe += pktlen;

	if (cur_network->IELength > MAX_IE_SZ)
		return;

	memcpy(pframe, cur_network->IEs, cur_network->IELength);
	pframe += cur_network->IELength;
	pktlen += cur_network->IELength;

	*pLength = pktlen;
}

/*
 * Description: Fill the reserved packets that FW will use to RSVD page.
 *			Now we just send 4 types packet to rsvd page.
 *			(1)Beacon, (2)Ps-poll, (3)Null data, (4)ProbeRsp.
 * Input:
 * finished - false:At the first time we will send all the packets as a large packet to Hw,
 *		    so we need to set the packet length to total lengh.
 *	      true: At the second time, we should send the first packet (default:beacon)
 *		    to Hw again and set the lengh in descriptor to the real beacon lengh.
 * page_num - The amount of reserved page which driver need.
 *	      If this is not NULL, this function doesn't real download reserved
 *	      page, but just count the number of reserved page.
 *
 * 2009.10.15 by tynli.
 * 2017.06.20 modified by Lucas.
 *
 * Page Size = 128: 8188e, 8723a/b, 8192c/d,
 * Page Size = 256: 8192e, 8821a
 * Page Size = 512: 8812a
 */

static void _rtw_hal_set_fw_rsvd_page(struct adapter *adapter, bool finished, u8 *page_num)
{
	struct hal_com_data * pHalData;
	struct xmit_frame	*pcmdframe = NULL;
	struct pkt_attrib	*pattrib;
	struct xmit_priv	*pxmitpriv;
	struct mlme_ext_priv	*pmlmeext;
	struct mlme_ext_info	*pmlmeinfo;
	struct pwrctrl_priv *pwrctl;
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	u32	BeaconLength = 0, ProbeRspLength = 0, PSPollLength = 0;
	u32	NullDataLength = 0, QosNullLength = 0, BTQosNullLength = 0;
	u8	TxDescLen = TXDESC_SIZE, TxDescOffset = TXDESC_OFFSET;
	u8	TotalPageNum = 0 , CurtPktPageNum = 0 , RsvdPageNum = 0;
	u8	*ReservedPagePacket;
	u16	BufIndex = 0;
	u32	TotalPacketLen = 0, MaxRsvdPageBufSize = 0, PageSize = 0;
	struct rsvd_page	RsvdPageLoc;

	pHalData = GET_HAL_DATA(adapter);
	pxmitpriv = &adapter->xmitpriv;
	pmlmeext = &adapter->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;
	pwrctl = adapter_to_pwrctl(adapter);

	rtw_hal_get_def_var(adapter, HAL_DEF_TX_PAGE_SIZE, (u8 *)&PageSize);

	if (PageSize == 0) {
		RTW_INFO("[Error]: %s, PageSize is zero!!\n", __func__);
		return;
	}

	/* Prepare ReservedPagePacket */
	if (page_num) {
		ReservedPagePacket = rtw_zmalloc(MAX_CMDBUF_SZ);
		if (!ReservedPagePacket) {
			RTW_WARN("%s: alloc ReservedPagePacket fail!\n", __func__);
			*page_num = 0xFF;
			return;
		}
	} else {
		if (pwrctl->wowlan_mode || pwrctl->wowlan_ap_mode)
			RsvdPageNum = rtw_hal_get_txbuff_rsvd_page_num(adapter, true);
		else
			RsvdPageNum = rtw_hal_get_txbuff_rsvd_page_num(adapter, false);

		RTW_INFO("%s PageSize: %d, RsvdPageNUm: %d\n", __func__, PageSize, RsvdPageNum);

		MaxRsvdPageBufSize = RsvdPageNum * PageSize;
		if (MaxRsvdPageBufSize > MAX_CMDBUF_SZ) {
			RTW_ERR("%s MaxRsvdPageBufSize(%d) is larger than MAX_CMDBUF_SZ(%d)",
				 __func__, MaxRsvdPageBufSize, MAX_CMDBUF_SZ);
			rtw_warn_on(1);
			return;
		}

		pcmdframe = rtw_alloc_cmdxmitframe(pxmitpriv);
		if (!pcmdframe) {
			RTW_ERR("%s: alloc ReservedPagePacket fail!\n", __func__);
			return;
		}

		ReservedPagePacket = pcmdframe->buf_addr;
	}

	memset(&RsvdPageLoc, 0, sizeof(struct rsvd_page));

	/* beacon * 1 pages */
	BufIndex = TxDescOffset;
	rtw_hal_construct_beacon(adapter,
				 &ReservedPagePacket[BufIndex], &BeaconLength);

	/*
	* When we count the first page size, we need to reserve description size for the RSVD
	* packet, it will be filled in front of the packet in TXPKTBUF.
	*/
	CurtPktPageNum = (u8)PageNum((TxDescLen + BeaconLength), PageSize);

	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum * PageSize);

	if (pwrctl->wowlan_ap_mode) {
		/* (4) probe response*/
		RsvdPageLoc.LocProbeRsp = TotalPageNum;
		rtw_hal_construct_ProbeRsp(
			adapter, &ReservedPagePacket[BufIndex],
			&ProbeRspLength,
			get_my_bssid(&pmlmeinfo->network), false);
		rtw_hal_fill_fake_txdesc(adapter,
				 &ReservedPagePacket[BufIndex - TxDescLen],
				 ProbeRspLength, false, false, false);

		CurtPktPageNum = (u8)PageNum(TxDescLen + ProbeRspLength, PageSize);
		TotalPageNum += CurtPktPageNum;
		TotalPacketLen = BufIndex + ProbeRspLength;
		BufIndex += (CurtPktPageNum * PageSize);
		goto download_page;
	}

	/* ps-poll * 1 page */
	RsvdPageLoc.LocPsPoll = TotalPageNum;
	RTW_INFO("LocPsPoll: %d\n", RsvdPageLoc.LocPsPoll);
	rtw_hal_construct_PSPoll(adapter,
				 &ReservedPagePacket[BufIndex], &PSPollLength);
	rtw_hal_fill_fake_txdesc(adapter,
				 &ReservedPagePacket[BufIndex - TxDescLen],
				 PSPollLength, true, false, false);

	CurtPktPageNum = (u8)PageNum((TxDescLen + PSPollLength), PageSize);

	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum * PageSize);

	if (!pwrctl->wowlan_mode ||
		pwrctl->wowlan_in_resume) {
		/* BT Qos null data * 1 page */
		RsvdPageLoc.LocBTQosNull = TotalPageNum;

		RTW_INFO("LocBTQosNull: %d\n", RsvdPageLoc.LocBTQosNull);

		rtw_hal_construct_NullFunctionData(adapter,
					&ReservedPagePacket[BufIndex],
					&BTQosNullLength,
					get_my_bssid(&pmlmeinfo->network),
					true, 0, 0, false);

		rtw_hal_fill_fake_txdesc(adapter,
				&ReservedPagePacket[BufIndex - TxDescLen],
				BTQosNullLength, false, true, false);

		CurtPktPageNum = (u8)PageNum(TxDescLen + BTQosNullLength,
					     PageSize);

		TotalPageNum += CurtPktPageNum;
		BufIndex += (CurtPktPageNum * PageSize);
	}

	/* null data * 1 page */
	RsvdPageLoc.LocNullData = TotalPageNum;
	RTW_INFO("LocNullData: %d\n", RsvdPageLoc.LocNullData);
	rtw_hal_construct_NullFunctionData(
		adapter,
		&ReservedPagePacket[BufIndex],
		&NullDataLength,
		get_my_bssid(&pmlmeinfo->network),
		false, 0, 0, false);
	rtw_hal_fill_fake_txdesc(adapter,
			 &ReservedPagePacket[BufIndex - TxDescLen],
			 NullDataLength, false, false, false);

	CurtPktPageNum = (u8)PageNum(TxDescLen + NullDataLength, PageSize);

	TotalPageNum += CurtPktPageNum;

	BufIndex += (CurtPktPageNum * PageSize);

	if (!pwrctl->wowlan_mode ||
		pwrctl->wowlan_in_resume) {
		/* Qos null data * 1 page */
		RsvdPageLoc.LocQosNull = TotalPageNum;
		RTW_INFO("LocQosNull: %d\n", RsvdPageLoc.LocQosNull);
		rtw_hal_construct_NullFunctionData(adapter,
					&ReservedPagePacket[BufIndex],
					&QosNullLength,
					get_my_bssid(&pmlmeinfo->network),
					true, 0, 0, false);
		rtw_hal_fill_fake_txdesc(adapter,
				 &ReservedPagePacket[BufIndex - TxDescLen],
				 QosNullLength, false, false, false);

		CurtPktPageNum = (u8)PageNum(TxDescLen + QosNullLength,
					     PageSize);

		TotalPageNum += CurtPktPageNum;

		BufIndex += (CurtPktPageNum * PageSize);
	}

	TotalPacketLen = BufIndex + QosNullLength;

	TotalPacketLen -= TxDescOffset;

download_page:
	if (page_num) {
		*page_num = TotalPageNum;
		rtw_mfree(ReservedPagePacket, MAX_CMDBUF_SZ);
		ReservedPagePacket = NULL;
		return;
	}

	RTW_INFO("%s PageNum(%d), pktlen(%d)\n",
		 __func__, TotalPageNum, TotalPacketLen);

	if (TotalPacketLen > MaxRsvdPageBufSize) {
		RTW_ERR("%s(ERROR): rsvd page size is not enough!!TotalPacketLen %d, MaxRsvdPageBufSize %d\n",
			 __func__, TotalPacketLen, MaxRsvdPageBufSize);
		rtw_warn_on(1);
		goto error;
	} else {
		/* update attribute */
		pattrib = &pcmdframe->attrib;
		update_mgntframe_attrib(adapter, pattrib);
		pattrib->qsel = QSLT_BEACON;
		pattrib->pktlen = TotalPacketLen;
		pattrib->last_txcmdsz = TotalPacketLen;
		dump_mgntframe_and_wait(adapter, pcmdframe, 100);
	}

	RTW_INFO("%s: Set RSVD page location to Fw ,TotalPacketLen(%d), TotalPageNum(%d)\n",
		 __func__, TotalPacketLen, TotalPageNum);
	if (check_fwstate(pmlmepriv, _FW_LINKED)) {
		rtw_hal_set_FwRsvdPage_cmd(adapter, &RsvdPageLoc);
	}

	return;
error:
	rtw_free_xmitframe(pxmitpriv, pcmdframe);
}

void rtw_hal_set_fw_rsvd_page(struct adapter *adapter, bool finished)
{
	_rtw_hal_set_fw_rsvd_page(adapter, finished, NULL);
}

/**
 * rtw_hal_get_rsvd_page_num() - Get needed reserved page number
 * @adapter:	struct adapter*
 *
 * Caculate needed reserved page number.
 * In different state would get different number, for example normal mode and
 * WOW mode would need different reserved page size.
 *
 * Return the number of reserved page which driver need.
 */
u8 rtw_hal_get_rsvd_page_num(struct adapter *adapter)
{
	u8 num = 0;


	_rtw_hal_set_fw_rsvd_page(adapter, false, &num);

	return num;
}

static void hw_var_set_mlme_sitesurvey(struct adapter *adapter, u8 variable, u8 *val)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u16 value_rxfltmap2;
	int i;
	struct adapter *iface;

	/* not to receive data frame */
	value_rxfltmap2 = 0;

	if (*((u8 *)val)) { /* under sitesurvey */
		/*
		* 1. configure REG_RXFLTMAP2
		* 2. disable TSF update &  buddy TSF update to avoid updating wrong TSF due to clear RCR_CBSSID_BCN
		* 3. config RCR to receive different BSSID BCN or probe rsp
		*/
		rtw_write16(adapter, REG_RXFLTMAP2, value_rxfltmap2);

#ifdef CONFIG_MI_WITH_MBSSID_CAM
		/*do nothing~~*/
#else

		/* disable update TSF */
		for (i = 0; i < dvobj->iface_nums; i++) {
			iface = dvobj->adapters[i];
			if (!iface)
				continue;

			if (rtw_linked_check(iface)
				&& !MLME_IS_AP(iface) && !MLME_IS_MESH(iface)
			) {
				if (iface->hw_port == HW_PORT1)
					rtw_write8(iface, REG_BCN_CTRL_1, rtw_read8(iface, REG_BCN_CTRL_1) | DIS_TSF_UDT);
				else
					rtw_write8(iface, REG_BCN_CTRL, rtw_read8(iface, REG_BCN_CTRL) | DIS_TSF_UDT);

				iface->mlmeextpriv.en_hw_update_tsf = false;
			}
		}
#endif /* CONFIG_MI_WITH_MBSSID_CAM */

		rtw_hal_rcr_set_chk_bssid(adapter, MLME_SCAN_ENTER);

		/* Save orignal RRSR setting. needed? */
		hal_data->RegRRSR = rtw_read16(adapter, REG_RRSR);

		if (rtw_mi_get_ap_num(adapter) || rtw_mi_get_mesh_num(adapter))
			StopTxBeacon(adapter);
	} else { /* sitesurvey done */
		/*
		* 1. enable rx data frame
		* 2. config RCR not to receive different BSSID BCN or probe rsp
		* 3. doesn't enable TSF update &  buddy TSF right now to avoid HW conflict
		*	 so, we enable TSF update when rx first BCN after sitesurvey done
		*/
		if (rtw_mi_check_fwstate(adapter, _FW_LINKED | WIFI_AP_STATE | WIFI_MESH_STATE)) {
			/* enable to rx data frame */
			rtw_write16(adapter, REG_RXFLTMAP2, 0xFFFF);
		}

		rtw_hal_rcr_set_chk_bssid(adapter, MLME_SCAN_DONE);

#ifdef CONFIG_MI_WITH_MBSSID_CAM
		/*if ((rtw_mi_get_assoced_sta_num(adapter) == 1) && (!rtw_mi_check_status(adapter, MI_AP_MODE)))
			rtw_write8(adapter, REG_BCN_CTRL, rtw_read8(adapter, REG_BCN_CTRL)&(~DIS_TSF_UDT));*/
#else

		for (i = 0; i < dvobj->iface_nums; i++) {
			iface = dvobj->adapters[i];
			if (!iface)
				continue;
			if (rtw_linked_check(iface)
				&& !MLME_IS_AP(iface) && !MLME_IS_MESH(iface)
			) {
				/* enable HW TSF update when recive beacon*/
				/*if (iface->hw_port == HW_PORT1)
					rtw_write8(iface, REG_BCN_CTRL_1, rtw_read8(iface, REG_BCN_CTRL_1)&(~(DIS_TSF_UDT)));
				else
					rtw_write8(iface, REG_BCN_CTRL, rtw_read8(iface, REG_BCN_CTRL)&(~(DIS_TSF_UDT)));
				*/
				iface->mlmeextpriv.en_hw_update_tsf = true;
			}
		}
#endif /* CONFIG_MI_WITH_MBSSID_CAM */

		/* Restore orignal RRSR setting. needed? */
		rtw_write16(adapter, REG_RRSR, hal_data->RegRRSR);

		if (rtw_mi_get_ap_num(adapter) || rtw_mi_get_mesh_num(adapter)) {
			ResumeTxBeacon(adapter);
			rtw_mi_tx_beacon_hdl(adapter);
		}
	}
}

#ifdef CONFIG_TSF_RESET_OFFLOAD
static int rtw_hal_h2c_reset_tsf(struct adapter *adapter, u8 reset_port)
{
	u8 buf[2];
	int ret;

	if (reset_port == HW_PORT0) {
		buf[0] = 0x1;
		buf[1] = 0;
	} else {
		buf[0] = 0x0;
		buf[1] = 0x1;
	}

	ret = rtw_hal_fill_h2c_cmd(adapter, H2C_RESET_TSF, 2, buf);

	return ret;
}

int rtw_hal_reset_tsf(struct adapter *adapter, u8 reset_port)
{
	u8 reset_cnt_before = 0, reset_cnt_after = 0, loop_cnt = 0;
	u32 reg_reset_tsf_cnt = (reset_port == HW_PORT0) ?
				REG_FW_RESET_TSF_CNT_0 : REG_FW_RESET_TSF_CNT_1;
	int ret;

	/* site survey will cause reset tsf fail */
	rtw_mi_buddy_scan_abort(adapter, false);
	reset_cnt_after = reset_cnt_before = rtw_read8(adapter, reg_reset_tsf_cnt);
	ret = rtw_hal_h2c_reset_tsf(adapter, reset_port);
	if (ret != _SUCCESS)
		return ret;

	while ((reset_cnt_after == reset_cnt_before) && (loop_cnt < 10)) {
		rtw_msleep_os(100);
		loop_cnt++;
		reset_cnt_after = rtw_read8(adapter, reg_reset_tsf_cnt);
	}

	return (loop_cnt >= 10) ? _FAIL : _SUCCESS;
}
#endif /* CONFIG_TSF_RESET_OFFLOAD */

static void rtw_hal_set_hw_update_tsf(struct adapter * adapt)
{
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;

#if defined(CONFIG_MI_WITH_MBSSID_CAM)
	RTW_INFO("[Warn] %s "ADPT_FMT" enter func\n", __func__, ADPT_ARG(adapt));
	rtw_warn_on(1);
	return;
#endif

	if (!pmlmeext->en_hw_update_tsf)
		return;

	/* check RCR */
	if (!rtw_hal_rcr_check(adapt, RCR_CBSSID_BCN))
		return;

	/* enable hw update tsf function for non-AP and non-Mesh */
	if (rtw_linked_check(adapt)
		&& !MLME_IS_AP(adapt) && !MLME_IS_MESH(adapt)
	) {
#ifdef CONFIG_CONCURRENT_MODE
		if (adapt->hw_port == HW_PORT1)
			rtw_write8(adapt, REG_BCN_CTRL_1, rtw_read8(adapt, REG_BCN_CTRL_1) & (~DIS_TSF_UDT));
		else
#endif
			rtw_write8(adapt, REG_BCN_CTRL, rtw_read8(adapt, REG_BCN_CTRL) & (~DIS_TSF_UDT));
	}
	pmlmeext->en_hw_update_tsf = false;
}

static void hw_var_set_correct_tsf(struct adapter *adapter)
{
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	/*do nothing*/
#else
	u64 tsf;
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;
	struct mlme_ext_info *mlmeinfo = &(mlmeext->mlmext_info);

	tsf = mlmeext->TSFValue - rtw_modular64(mlmeext->TSFValue, (mlmeinfo->bcn_interval * 1024)) - 1024; /*us*/

	if ((mlmeinfo->state & 0x03) == WIFI_FW_ADHOC_STATE
		|| (mlmeinfo->state & 0x03) == WIFI_FW_AP_STATE)
		StopTxBeacon(adapter);

	rtw_hal_correct_tsf(adapter, adapter->hw_port, tsf);

#ifdef CONFIG_CONCURRENT_MODE
	/* Update buddy port's TSF if it is SoftAP/Mesh for beacon TX issue! */
	if ((mlmeinfo->state & 0x03) == WIFI_FW_STATION_STATE
		&& (rtw_mi_get_ap_num(adapter) || rtw_mi_get_mesh_num(adapter))
	) {
		struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
		int i;
		struct adapter *iface;

		for (i = 0; i < dvobj->iface_nums; i++) {
			iface = dvobj->adapters[i];
			if (!iface)
				continue;
			if (iface == adapter)
				continue;

			if ((MLME_IS_AP(iface) || MLME_IS_MESH(iface))
				&& check_fwstate(&iface->mlmepriv, WIFI_ASOC_STATE)
			) {
				rtw_hal_correct_tsf(iface, iface->hw_port, tsf);
				#ifdef CONFIG_TSF_RESET_OFFLOAD
				if (rtw_hal_reset_tsf(iface, iface->hw_port) == _FAIL)
					RTW_INFO("%s-[ERROR] "ADPT_FMT" Reset port%d TSF fail\n"
						, __func__, ADPT_ARG(iface), iface->hw_port);
				#endif	/* CONFIG_TSF_RESET_OFFLOAD*/
			}
		}
	}
#endif /* CONFIG_CONCURRENT_MODE */

	if ((mlmeinfo->state & 0x03) == WIFI_FW_ADHOC_STATE
		|| (mlmeinfo->state & 0x03) == WIFI_FW_AP_STATE)
		ResumeTxBeacon(adapter);

#endif /*CONFIG_MI_WITH_MBSSID_CAM*/
}

#if defined(CONFIG_FW_MULTI_PORT_SUPPORT)
/* For multi-port support, driver needs to inform the port ID to FW for btc operations */
int rtw_hal_set_wifi_port_id_cmd(struct adapter *adapter)
{
	u8 port_id = 0;
	u8 h2c_buf[H2C_BTC_WL_PORT_ID_LEN] = {0};

	SET_H2CCMD_BTC_WL_PORT_ID(h2c_buf, adapter->hw_port);
	return rtw_hal_fill_h2c_cmd(adapter, H2C_BTC_WL_PORT_ID, H2C_BTC_WL_PORT_ID_LEN, h2c_buf);
}
#endif

u8 SetHwReg(struct adapter *adapter, u8 variable, u8 *val)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 ret = _SUCCESS;

	switch (variable) {
	case HW_VAR_MEDIA_STATUS: {
		u8 net_type = *((u8 *)val);

		rtw_hal_set_msr(adapter, net_type);
	}
	break;
	case HW_VAR_MAC_ADDR:
#ifdef CONFIG_MI_WITH_MBSSID_CAM
		rtw_hal_set_macaddr_mbid(adapter, val);
#else
		rtw_hal_set_macaddr_port(adapter, val);
#endif
		break;
	case HW_VAR_BSSID:
		rtw_hal_set_bssid(adapter, val);
		break;
	case HW_VAR_RCR:
		ret = hw_var_rcr_config(adapter, *((u32 *)val));
		break;
	case HW_VAR_ON_RCR_AM:
		hw_var_set_rcr_am(adapter, 1);
		break;
	case HW_VAR_OFF_RCR_AM:
		hw_var_set_rcr_am(adapter, 0);
		break;
	case HW_VAR_BEACON_INTERVAL:
		hw_var_set_bcn_interval(adapter, *(u16 *)val);
		break;
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	case HW_VAR_MBSSID_CAM_WRITE: {
		u32	cmd = 0;
		u32	*cam_val = (u32 *)val;

		rtw_write32(adapter, REG_MBIDCAMCFG_1, cam_val[0]);
		cmd = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | BIT_MBIDCAM_VALID | cam_val[1];
		rtw_write32(adapter, REG_MBIDCAMCFG_2, cmd);
	}
		break;
	case HW_VAR_MBSSID_CAM_CLEAR: {
		u32 cmd;
		u8 entry_id = *(u8 *)val;

		rtw_write32(adapter, REG_MBIDCAMCFG_1, 0);

		cmd = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | ((entry_id & MBIDCAM_ADDR_MASK) << MBIDCAM_ADDR_SHIFT);
		rtw_write32(adapter, REG_MBIDCAMCFG_2, cmd);
	}
		break;
	case HW_VAR_RCR_MBSSID_EN:
		if (*((u8 *)val))
			rtw_hal_rcr_add(adapter, RCR_ENMBID);
		else
			rtw_hal_rcr_clear(adapter, RCR_ENMBID);
		break;
#endif
	case HW_VAR_PORT_SWITCH:
		hw_var_port_switch(adapter);
		break;
	case HW_VAR_INIT_RTS_RATE: {
		u16 brate_cfg = *((u16 *)val);
		u8 rate_index = 0;
		struct hal_version *hal_ver = &hal_data->version_id;

		if (IS_8188E(*hal_ver)) {

			while (brate_cfg > 0x1) {
				brate_cfg = (brate_cfg >> 1);
				rate_index++;
			}
			rtw_write8(adapter, REG_INIRTS_RATE_SEL, rate_index);
		} else
			rtw_warn_on(1);
	}
		break;
	case HW_VAR_SEC_CFG: {
		u16 reg_scr_ori;
		u16 reg_scr;

		reg_scr = reg_scr_ori = rtw_read16(adapter, REG_SECCFG);
		reg_scr |= (SCR_CHK_KEYID | SCR_RxDecEnable | SCR_TxEncEnable);

		if (_rtw_camctl_chk_cap(adapter, SEC_CAP_CHK_BMC))
			reg_scr |= SCR_CHK_BMC;

		if (_rtw_camctl_chk_flags(adapter, SEC_STATUS_STA_PK_GK_CONFLICT_DIS_BMC_SEARCH))
			reg_scr |= SCR_NoSKMC;

		if (reg_scr != reg_scr_ori)
			rtw_write16(adapter, REG_SECCFG, reg_scr);
	}
		break;
	case HW_VAR_SEC_DK_CFG: {
		struct security_priv *sec = &adapter->securitypriv;
		u8 reg_scr = rtw_read8(adapter, REG_SECCFG);

		if (val) { /* Enable default key related setting */
			reg_scr |= SCR_TXBCUSEDK;
			if (sec->dot11AuthAlgrthm != dot11AuthAlgrthm_8021X)
				reg_scr |= (SCR_RxUseDK | SCR_TxUseDK);
		} else /* Disable default key related setting */
			reg_scr &= ~(SCR_RXBCUSEDK | SCR_TXBCUSEDK | SCR_RxUseDK | SCR_TxUseDK);

		rtw_write8(adapter, REG_SECCFG, reg_scr);
	}
		break;

	case HW_VAR_ASIX_IOT:
		/* enable  ASIX IOT function */
		if (*((u8 *)val)) {
			/* 0xa2e[0]=0 (disable rake receiver) */
			rtw_write8(adapter, rCCK0_FalseAlarmReport + 2,
				rtw_read8(adapter, rCCK0_FalseAlarmReport + 2) & ~(BIT0));
			/* 0xa1c=0xa0 (reset channel estimation if signal quality is bad) */
			rtw_write8(adapter, rCCK0_DSPParameter2, 0xa0);
		} else {
			/* restore reg:0xa2e,   reg:0xa1c */
			rtw_write8(adapter, rCCK0_FalseAlarmReport + 2,
				rtw_read8(adapter, rCCK0_FalseAlarmReport + 2) | (BIT0));
			rtw_write8(adapter, rCCK0_DSPParameter2, 0x00);
		}
		break;
	case HW_VAR_MLME_SITESURVEY:
		hw_var_set_mlme_sitesurvey(adapter, variable, val);
		if (hal_data->EEPROMBluetoothCoexist == 1)
			rtw_btcoex_ScanNotify(adapter, *val ? true : false);
		break;
	case HW_VAR_EN_HW_UPDATE_TSF:
		rtw_hal_set_hw_update_tsf(adapter);
		break;
	case HW_VAR_CORRECT_TSF:
		hw_var_set_correct_tsf(adapter);
		break;
	case HW_VAR_APFM_ON_MAC:
		hal_data->bMacPwrCtrlOn = *val;
		RTW_INFO("%s: bMacPwrCtrlOn=%d\n", __func__, hal_data->bMacPwrCtrlOn);
		break;
	case HW_VAR_ENABLE_RX_BAR:
		if (*val) {
			/* enable RX BAR */
			u16 val16 = rtw_read16(adapter, REG_RXFLTMAP1);

			val16 |= BIT(8);
			rtw_write16(adapter, REG_RXFLTMAP1, val16);
		} else {
			/* disable RX BAR */
			u16 val16 = rtw_read16(adapter, REG_RXFLTMAP1);

			val16 &= (~BIT(8));
			rtw_write16(adapter, REG_RXFLTMAP1, val16);
		}
		RTW_INFO("[HW_VAR_ENABLE_RX_BAR] 0x%02X=0x%02X\n",
			REG_RXFLTMAP1, rtw_read16(adapter, REG_RXFLTMAP1));
		break;
	default:
		break;
	}

	return ret;
}

void GetHwReg(struct adapter *adapter, u8 variable, u8 *val)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u64 val64;


	switch (variable) {
	case HW_VAR_MAC_ADDR:
		rtw_hal_get_macaddr_port(adapter, val);
		break;
	case HW_VAR_BASIC_RATE:
		*((u16 *)val) = hal_data->BasicRateSet;
		break;
	case HW_VAR_RF_TYPE:
		*((u8 *)val) = hal_data->rf_type;
		break;
	case HW_VAR_MEDIA_STATUS:
		rtw_hal_get_msr(adapter, val);
		break;
	case HW_VAR_DO_IQK:
		*val = hal_data->bNeedIQK;
		break;
	case HW_VAR_CH_SW_NEED_TO_TAKE_CARE_IQK_INFO:
		if (hal_is_band_support(adapter, BAND_ON_5G))
			*val = true;
		else
			*val = false;
		break;
	case HW_VAR_APFM_ON_MAC:
		*val = hal_data->bMacPwrCtrlOn;
		break;
	case HW_VAR_RCR:
		hw_var_rcr_get(adapter, (u32 *)val);
		break;
	case HW_VAR_FWLPS_RF_ON:
		/* When we halt NIC, we should check if FW LPS is leave. */
		if (rtw_is_surprise_removed(adapter)
			|| (adapter_to_pwrctl(adapter)->rf_pwrstate == rf_off)
		) {
			/*
			 * If it is in HW/SW Radio OFF or IPS state,
			 * we do not check Fw LPS Leave,
			 * because Fw is unload.
			 */
			*val = true;
		} else {
			u32 rcr = 0;

			rtw_hal_get_hwreg(adapter, HW_VAR_RCR, (u8 *)&rcr);
			if (rcr & (RCR_UC_MD_EN | RCR_BC_MD_EN | RCR_TIM_PARSER_EN))
				*val = false;
			else
				*val = true;
		}
		break;

	case HW_VAR_TSF:
		/* read and save HIGH 32bits TSF value */
		val64 = rtw_read32(adapter, REG_TSFTR+4);
		val64 = val64 << 32;

		/* read and save LOW 32bits TSF value */
		val64 |= rtw_read32(adapter, REG_TSFTR);
		*((u64*)val) = val64;
		break;

	default:
		break;
	}

}

static u32 _get_page_size(struct adapter *a)
{
	return PAGE_SIZE_128;
}

u8
SetHalDefVar(struct adapter *adapter, enum hal_def_variable variable, void *value)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 bResult = _SUCCESS;

	switch (variable) {

	case HAL_DEF_DBG_DUMP_RXPKT:
		hal_data->bDumpRxPkt = *((u8 *)value);
		break;
	case HAL_DEF_DBG_DUMP_TXPKT:
		hal_data->bDumpTxPkt = *((u8 *)value);
		break;
	case HAL_DEF_ANT_DETECT:
		hal_data->AntDetection = *((u8 *)value);
		break;
	case HAL_DEF_DBG_DIS_PWT:
		hal_data->bDisableTXPowerTraining = *((u8 *)value);
		break;
	default:
		RTW_PRINT("%s: [WARNING] hal_def_variable(%d) not defined!\n", __func__, variable);
		bResult = _FAIL;
		break;
	}

	return bResult;
}

u8
GetHalDefVar(struct adapter *adapter, enum hal_def_variable variable, void *value)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	u8 bResult = _SUCCESS;

	switch (variable) {
	case HAL_DEF_UNDERCORATEDSMOOTHEDPWDB: {
		struct mlme_priv *pmlmepriv;
		struct sta_priv *pstapriv;
		struct sta_info *psta;

		pmlmepriv = &adapter->mlmepriv;
		pstapriv = &adapter->stapriv;
		psta = rtw_get_stainfo(pstapriv, pmlmepriv->cur_network.network.MacAddress);
		if (psta)
			*((int *)value) = psta->cmn.rssi_stat.rssi;
	}
	break;
	case HAL_DEF_DBG_DUMP_RXPKT:
		*((u8 *)value) = hal_data->bDumpRxPkt;
		break;
	case HAL_DEF_DBG_DUMP_TXPKT:
		*((u8 *)value) = hal_data->bDumpTxPkt;
		break;
	case HAL_DEF_ANT_DETECT:
		*((u8 *)value) = hal_data->AntDetection;
		break;
	case HAL_DEF_TX_PAGE_SIZE:
		*((u32 *)value) = _get_page_size(adapter);
		break;
	case HAL_DEF_DBG_DIS_PWT:
		*(u8 *)value = hal_data->bDisableTXPowerTraining;
		break;
	case HAL_DEF_EXPLICIT_BEAMFORMER:
	case HAL_DEF_EXPLICIT_BEAMFORMEE:
	case HAL_DEF_VHT_MU_BEAMFORMER:
	case HAL_DEF_VHT_MU_BEAMFORMEE:
		*(u8 *)value = false;
		break;
	default:
		RTW_PRINT("%s: [WARNING] hal_def_variable(%d) not defined!\n", __func__, variable);
		bResult = _FAIL;
		break;
	}

	return bResult;
}


bool
eqNByte(
	u8	*str1,
	u8	*str2,
	u32	num
)
{
	if (num == 0)
		return false;
	while (num > 0) {
		num--;
		if (str1[num] != str2[num])
			return false;
	}
	return true;
}

/*
 *	Description:
 *		Translate a character to hex digit.
 *   */
u32
MapCharToHexDigit(
	char		chTmp
)
{
	if (chTmp >= '0' && chTmp <= '9')
		return chTmp - '0';
	else if (chTmp >= 'a' && chTmp <= 'f')
		return 10 + (chTmp - 'a');
	else if (chTmp >= 'A' && chTmp <= 'F')
		return 10 + (chTmp - 'A');
	else
		return 0;
}



/*
 *	Description:
 *		Parse hex number from the string pucStr.
 *   */
bool
GetHexValueFromString(
	char			*szStr,
	u32			*pu4bVal,
	u32			*pu4bMove
)
{
	char		*szScan = szStr;

	/* Check input parameter. */
	if (!szStr || !pu4bVal || !pu4bMove) {
		RTW_INFO("GetHexValueFromString(): Invalid input argumetns! szStr: %p, pu4bVal: %p, pu4bMove: %p\n", szStr, pu4bVal, pu4bMove);
		return false;
	}

	/* Initialize output. */
	*pu4bMove = 0;
	*pu4bVal = 0;

	/* Skip leading space. */
	while (*szScan != '\0' &&
	       (*szScan == ' ' || *szScan == '\t')) {
		szScan++;
		(*pu4bMove)++;
	}

	/* Skip leading '0x' or '0X'. */
	if (*szScan == '0' && (*(szScan + 1) == 'x' || *(szScan + 1) == 'X')) {
		szScan += 2;
		(*pu4bMove) += 2;
	}

	/* Check if szScan is now pointer to a character for hex digit, */
	/* if not, it means this is not a valid hex number. */
	if (!IsHexDigit(*szScan))
		return false;

	/* Parse each digit. */
	do {
		(*pu4bVal) <<= 4;
		*pu4bVal += MapCharToHexDigit(*szScan);

		szScan++;
		(*pu4bMove)++;
	} while (IsHexDigit(*szScan));

	return true;
}

bool
GetFractionValueFromString(
	char			*szStr,
	u8				*pInteger,
	u8				*pFraction,
	u32			*pu4bMove
)
{
	char	*szScan = szStr;

	/* Initialize output. */
	*pu4bMove = 0;
	*pInteger = 0;
	*pFraction = 0;

	/* Skip leading space. */
	while (*szScan != '\0' &&	(*szScan == ' ' || *szScan == '\t')) {
		++szScan;
		++(*pu4bMove);
	}

	/* Parse each digit. */
	do {
		(*pInteger) *= 10;
		*pInteger += (*szScan - '0');

		++szScan;
		++(*pu4bMove);

		if (*szScan == '.') {
			++szScan;
			++(*pu4bMove);

			if (*szScan < '0' || *szScan > '9')
				return false;
			else {
				*pFraction = *szScan - '0';
				++szScan;
				++(*pu4bMove);
				return true;
			}
		}
	} while (*szScan >= '0' && *szScan <= '9');

	return true;
}

/*
 *	Description:
 * Return true if szStr is comment out with leading " */ /* ".
 *   */
bool
IsCommentString(
	char			*szStr
)
{
	if (*szStr == '/' && *(szStr + 1) == '/')
		return true;
	else
		return false;
}

bool
GetU1ByteIntegerFromStringInDecimal(
	char	*Str,
	u8		*pInt
)
{
	u16 i = 0;
	*pInt = 0;

	while (Str[i] != '\0') {
		if (Str[i] >= '0' && Str[i] <= '9') {
			*pInt *= 10;
			*pInt += (Str[i] - '0');
		} else
			return false;
		++i;
	}

	return true;
}

/* <20121004, Kordan> For example,
 * ParseQualifiedString(inString, 0, outString, '[', ']') gets "Kordan" from a string "Hello [Kordan]".
 * If RightQualifier does not exist, it will hang on in the while loop */
bool
ParseQualifiedString(
	char	*In,
	u32	*Start,
	char	*Out,
	char		LeftQualifier,
	char		RightQualifier
)
{
	u32	i = 0, j = 0;
	char	c = In[(*Start)++];

	if (c != LeftQualifier)
		return false;

	i = (*Start);
	c = In[(*Start)++];
	while (c != RightQualifier && c != '\0')
		c = In[(*Start)++];

	if (c == '\0')
		return false;

	j = (*Start) - 2;
	strncpy((char *)Out, (const char *)(In + i), j - i + 1);

	return true;
}

bool
isAllSpaceOrTab(
	u8	*data,
	u8	size
)
{
	u8	cnt = 0, NumOfSpaceAndTab = 0;

	while (size > cnt) {
		if (data[cnt] == ' ' || data[cnt] == '\t' || data[cnt] == '\0')
			++NumOfSpaceAndTab;

		++cnt;
	}

	return size == NumOfSpaceAndTab;
}


void rtw_hal_check_rxfifo_full(struct adapter *adapter)
{
	struct dvobj_priv *psdpriv = adapter->dvobj;
	struct debug_priv *pdbgpriv = &psdpriv->drv_dbg;
	struct hal_com_data *pHalData = GET_HAL_DATA(adapter);
	struct registry_priv *regsty = &adapter->registrypriv;
	int save_cnt = false;

	if (regsty->check_hw_status == 1) {
		/* switch counter to RX fifo */
		if (IS_8188E(pHalData->version_id) ||
		    IS_8188F(pHalData->version_id) ||
		    IS_8812_SERIES(pHalData->version_id) ||
		    IS_8821_SERIES(pHalData->version_id) ||
		    IS_8723B_SERIES(pHalData->version_id) ||
		    IS_8192E(pHalData->version_id) ||
		    IS_8703B_SERIES(pHalData->version_id) ||
		    IS_8723D_SERIES(pHalData->version_id)) {
			rtw_write8(adapter, REG_RXERR_RPT + 3, rtw_read8(adapter, REG_RXERR_RPT + 3) | 0xa0);
			save_cnt = true;
		} else {
			/* todo: other chips */
		}


		if (save_cnt) {
			pdbgpriv->dbg_rx_fifo_last_overflow = pdbgpriv->dbg_rx_fifo_curr_overflow;
			pdbgpriv->dbg_rx_fifo_curr_overflow = rtw_read16(adapter, REG_RXERR_RPT);
			pdbgpriv->dbg_rx_fifo_diff_overflow = pdbgpriv->dbg_rx_fifo_curr_overflow - pdbgpriv->dbg_rx_fifo_last_overflow;
		} else {
			/* special value to indicate no implementation */
			pdbgpriv->dbg_rx_fifo_last_overflow = 1;
			pdbgpriv->dbg_rx_fifo_curr_overflow = 1;
			pdbgpriv->dbg_rx_fifo_diff_overflow = 1;
		}
	}
}

void linked_info_dump(struct adapter *adapt, u8 benable)
{
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

	if (adapt->bLinkInfoDump == benable)
		return;

	RTW_INFO("%s %s\n", __func__, (benable) ? "enable" : "disable");

	if (benable) {
		pwrctrlpriv->org_power_mgnt = pwrctrlpriv->power_mgnt;/* keep org value */
		rtw_pm_set_lps(adapt, PS_MODE_ACTIVE);

		pwrctrlpriv->ips_org_mode = pwrctrlpriv->ips_mode;/* keep org value */
		rtw_pm_set_ips(adapt, IPS_NONE);
	} else {
		rtw_pm_set_ips(adapt, pwrctrlpriv->ips_org_mode);

		rtw_pm_set_lps(adapt, pwrctrlpriv->org_power_mgnt);
	}
	adapt->bLinkInfoDump = benable ;
}

void rtw_store_phy_info(struct adapter *adapt, union recv_frame *prframe)
{
	u8 isCCKrate, rf_path , dframe_type;
	u8 *ptr;
	struct recv_priv *precvpriv = &(adapt->recvpriv);
	struct hal_com_data *	pHalData =  GET_HAL_DATA(adapt);
	struct rx_pkt_attrib *pattrib = &prframe->u.hdr.attrib;
	struct phydm_phyinfo_struct *p_phy_info = &pattrib->phy_info;
	struct rx_raw_rssi *psample_pkt_rssi = &adapt->recvpriv.raw_rssi_info;
	psample_pkt_rssi->data_rate = pattrib->data_rate;
	ptr = prframe->u.hdr.rx_data;
	dframe_type = GetFrameType(ptr);

	if (precvpriv->store_law_data_flag) {
		isCCKrate = (pattrib->data_rate <= DESC_RATE11M) ? true : false;

		psample_pkt_rssi->pwdball = p_phy_info->rx_pwdb_all;
		psample_pkt_rssi->pwr_all = p_phy_info->recv_signal_power;

		for (rf_path = 0; rf_path < pHalData->NumTotalRFPath; rf_path++) {
			psample_pkt_rssi->mimo_signal_strength[rf_path] = p_phy_info->rx_mimo_signal_strength[rf_path];
			psample_pkt_rssi->mimo_signal_quality[rf_path] = p_phy_info->rx_mimo_signal_quality[rf_path];
			if (!isCCKrate) {
				psample_pkt_rssi->ofdm_pwr[rf_path] = p_phy_info->rx_pwr[rf_path];
				psample_pkt_rssi->ofdm_snr[rf_path] = p_phy_info->rx_snr[rf_path];
			}
		}
	}

}


int check_phy_efuse_tx_power_info_valid(struct adapter * adapt)
{
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);
	u8 *pContent = pHalData->efuse_eeprom_data;
	int index = 0;
	u16 tx_index_offset = 0x0000;

	switch (rtw_get_chip_type(adapt)) {
	case RTL8723B:
		tx_index_offset = EEPROM_TX_PWR_INX_8723B;
		break;
	case RTL8703B:
		tx_index_offset = EEPROM_TX_PWR_INX_8703B;
		break;
	case RTL8723D:
		tx_index_offset = EEPROM_TX_PWR_INX_8723D;
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
	case RTL8822B:
		tx_index_offset = EEPROM_TX_PWR_INX_8822B;
		break;
	case RTL8821C:
		tx_index_offset = EEPROM_TX_PWR_INX_8821C;
		break;
	default:
		tx_index_offset = 0x0010;
		break;
	}

	/* TODO: chacking length by ICs */
	for (index = 0 ; index < 11 ; index++) {
		if (pContent[tx_index_offset + index] == 0xFF)
			return false;
	}
	return true;
}

int hal_efuse_macaddr_offset(struct adapter *adapter)
{
	u8 interface_type = 0;
	int addr_offset = -1;

	interface_type = rtw_get_intf_type(adapter);

	switch (rtw_get_chip_type(adapter)) {
	case RTL8723D:
		if (interface_type == RTW_USB)
			addr_offset = EEPROM_MAC_ADDR_8723DU;
		else if (interface_type == RTW_SDIO)
			addr_offset = EEPROM_MAC_ADDR_8723DS;
		else if (interface_type == RTW_PCIE)
			addr_offset = EEPROM_MAC_ADDR_8723DE;
		break;
	}

	if (addr_offset == -1) {
		RTW_ERR("%s: unknown combination - chip_type:%u, interface:%u\n"
			, __func__, rtw_get_chip_type(adapter), rtw_get_intf_type(adapter));
	}

	return addr_offset;
}

int Hal_GetPhyEfuseMACAddr(struct adapter * adapt, u8 *mac_addr)
{
	int ret = _FAIL;
	int addr_offset;

	addr_offset = hal_efuse_macaddr_offset(adapt);
	if (addr_offset == -1)
		goto exit;

	ret = rtw_efuse_map_read(adapt, addr_offset, ETH_ALEN, mac_addr);

exit:
	return ret;
}

void rtw_dump_cur_efuse(struct adapter * adapt)
{
	int mapsize =0;
	struct hal_com_data *hal_data = GET_HAL_DATA(adapt);

	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN , (void *)&mapsize, false);

	if (mapsize <= 0 || mapsize > EEPROM_MAX_SIZE) {
		RTW_ERR("wrong map size %d\n", mapsize);
		return;
	}

#ifdef CONFIG_RTW_DEBUG
	if (hal_data->efuse_file_status == EFUSE_FILE_LOADED)
		RTW_MAP_DUMP_SEL(RTW_DBGDUMP, "EFUSE FILE", hal_data->efuse_eeprom_data, mapsize);
	else
		RTW_MAP_DUMP_SEL(RTW_DBGDUMP, "HW EFUSE", hal_data->efuse_eeprom_data, mapsize);
#endif
}


#ifdef CONFIG_EFUSE_CONFIG_FILE
u32 Hal_readPGDataFromConfigFile(struct adapter * adapt)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapt);
	u32 ret = false;
	u32 maplen = 0;

	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN , (void *)&maplen, false);

	if (maplen < 256 || maplen > EEPROM_MAX_SIZE) {
		RTW_ERR("eFuse length error :%d\n", maplen);
		return false;
	}	

	ret = rtw_read_efuse_from_file(EFUSE_MAP_PATH, hal_data->efuse_eeprom_data, maplen);

	hal_data->efuse_file_status = ((ret == _FAIL) ? EFUSE_FILE_FAILED : EFUSE_FILE_LOADED);

	if (hal_data->efuse_file_status == EFUSE_FILE_LOADED)
		rtw_dump_cur_efuse(adapt);

	return ret;
}

u32 Hal_ReadMACAddrFromFile(struct adapter * adapt, u8 *mac_addr)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapt);
	u32 ret = _FAIL;

	if (rtw_read_macaddr_from_file(WIFIMAC_PATH, mac_addr) == _SUCCESS
		&& !rtw_check_invalid_mac_address(mac_addr, true)
	) {
		hal_data->macaddr_file_status = MACADDR_FILE_LOADED;
		ret = _SUCCESS;
	} else
		hal_data->macaddr_file_status = MACADDR_FILE_FAILED;

	return ret;
}
#endif /* CONFIG_EFUSE_CONFIG_FILE */

int hal_config_macaddr(struct adapter *adapter, bool autoload_fail)
{
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
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
	if (hw_addr && !rtw_check_invalid_mac_address(hw_addr, true)) {
		memcpy(hal_data->EEPROMMACAddr, hw_addr, ETH_ALEN);
		goto exit;
	}

bypass_hw_pg:

#ifdef CONFIG_EFUSE_CONFIG_FILE
	/* check wifi mac file */
	if (Hal_ReadMACAddrFromFile(adapter, addr) == _SUCCESS) {
		memcpy(hal_data->EEPROMMACAddr, addr, ETH_ALEN);
		goto exit;
	}
#endif

	memset(hal_data->EEPROMMACAddr, 0, ETH_ALEN);
	ret = _FAIL;

exit:
	return ret;
}

void rtw_bb_rf_gain_offset(struct adapter *adapt)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(adapt);
	struct registry_priv  *registry_par = &adapt->registrypriv;
	struct kfree_data_t *kfree_data = &pHalData->kfree_data;

	if (registry_par->RegPwrTrimEnable == 2) {
		RTW_INFO("Registry kfree default force disable.\n");
		return;
	}

	/* TODO: call this when channel switch */
	if (kfree_data->flag & KFREE_FLAG_ON)
		rtw_rf_apply_tx_gain_offset(adapt, 6); /* input ch6 to select BB_GAIN_2G */
}
bool kfree_data_is_bb_gain_empty(struct kfree_data_t *data)
{
	int i, j;

	for (i = 0; i < BB_GAIN_NUM; i++)
		for (j = 0; j < RF_PATH_MAX; j++)
			if (data->bb_gain[i][j] != 0)
				return 0;
	return 1;
}

/* To avoid RX affect TX throughput */
void dm_DynamicUsbTxAgg(struct adapter *adapt, u8 from_timer)
{
}

/* bus-agg check for SoftAP mode */
inline u8 rtw_hal_busagg_qsel_check(struct adapter *adapt, u8 pre_qsel, u8 next_qsel)
{
	u8 chk_rst = _SUCCESS;

	if (!MLME_IS_AP(adapt) && !MLME_IS_MESH(adapt))
		return chk_rst;

	if (((pre_qsel == QSLT_HIGH) || ((next_qsel == QSLT_HIGH))) &&
	    (pre_qsel != next_qsel)) {
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
void dump_TX_FIFO(struct adapter *adapt, u8 page_num, u16 page_size)
{

	int i;
	u8 val = 0;
	u8 base = 0;
	u32 addr = 0;
	u32 count = (page_size / 8);

	if (page_num <= 0) {
		RTW_INFO("!!%s: incorrect input page_num paramter!\n", __func__);
		return;
	}

	if (page_size < 128 || page_size > 512) {
		RTW_INFO("!!%s: incorrect input page_size paramter!\n", __func__);
		return;
	}

	RTW_INFO("+%s+\n", __func__);
	val = rtw_read8(adapt, 0x106);
	rtw_write8(adapt, 0x106, 0x69);
	RTW_INFO("0x106: 0x%02x\n", val);
	base = rtw_read8(adapt, 0x209);
	RTW_INFO("0x209: 0x%02x\n", base);

	addr = ((base)*page_size) / 8;
	for (i = 0 ; i < page_num * count ; i += 2) {
		rtw_write32(adapt, 0x140, addr + i);
		printk(" %08x %08x ", rtw_read32(adapt, 0x144), rtw_read32(adapt, 0x148));
		rtw_write32(adapt, 0x140, addr + i + 1);
		printk(" %08x %08x\n", rtw_read32(adapt, 0x144), rtw_read32(adapt, 0x148));
	}
}

s8 rtw_hal_ch_sw_iqk_info_search(struct adapter *adapt, u8 central_chnl, u8 bw_mode)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	u8 i;

	for (i = 0; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++) {
		if ((pHalData->iqk_reg_backup[i].central_chnl != 0)) {
			if ((pHalData->iqk_reg_backup[i].central_chnl == central_chnl)
			    && (pHalData->iqk_reg_backup[i].bw_mode == bw_mode))
				return i;
		}
	}

	return -1;
}

void rtw_hal_ch_sw_iqk_info_backup(struct adapter *adapt)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapt);
	s8 res;
	u8 i;

	/* If it's an existed record, overwrite it */
	res = rtw_hal_ch_sw_iqk_info_search(adapt, pHalData->current_channel, pHalData->current_channel_bw);
	if ((res >= 0) && (res < MAX_IQK_INFO_BACKUP_CHNL_NUM)) {
		rtw_hal_set_hwreg(adapt, HW_VAR_CH_SW_IQK_INFO_BACKUP, (u8 *)&(pHalData->iqk_reg_backup[res]));
		return;
	}

	/* Search for the empty record to use */
	for (i = 0; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++) {
		if (pHalData->iqk_reg_backup[i].central_chnl == 0) {
			rtw_hal_set_hwreg(adapt, HW_VAR_CH_SW_IQK_INFO_BACKUP, (u8 *)&(pHalData->iqk_reg_backup[i]));
			return;
		}
	}

	/* Else, overwrite the oldest record */
	for (i = 1; i < MAX_IQK_INFO_BACKUP_CHNL_NUM; i++)
		memcpy(&(pHalData->iqk_reg_backup[i - 1]), &(pHalData->iqk_reg_backup[i]), sizeof(struct hal_iqk_reg_backup));

	rtw_hal_set_hwreg(adapt, HW_VAR_CH_SW_IQK_INFO_BACKUP, (u8 *)&(pHalData->iqk_reg_backup[MAX_IQK_INFO_BACKUP_CHNL_NUM - 1]));
}

void rtw_hal_ch_sw_iqk_info_restore(struct adapter *adapt, u8 ch_sw_use_case)
{
	rtw_hal_set_hwreg(adapt, HW_VAR_CH_SW_IQK_INFO_RESTORE, &ch_sw_use_case);
}

void rtw_dump_mac_rx_counters(struct adapter *adapt, struct dbg_rx_counter *rx_counter)
{
	u32	mac_cck_ok = 0, mac_ofdm_ok = 0, mac_ht_ok = 0, mac_vht_ok = 0;
	u32	mac_cck_err = 0, mac_ofdm_err = 0, mac_ht_err = 0, mac_vht_err = 0;
	u32	mac_cck_fa = 0, mac_ofdm_fa = 0, mac_ht_fa = 0;
	u32	DropPacket = 0;

	if (!rx_counter) {
		rtw_warn_on(1);
		return;
	}
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x3);
	mac_cck_ok	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]	  */
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x0);
	mac_ofdm_ok	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]	 */
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x6);
	mac_ht_ok	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]	 */
	mac_vht_ok	= 0;
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x4);
	mac_cck_err	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]	 */
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x1);
	mac_ofdm_err	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]	 */
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x7);
	mac_ht_err	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]		 */
	mac_vht_err	= 0;

	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x5);
	mac_cck_fa	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]	 */
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x2);
	mac_ofdm_fa	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]	 */
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT28 | BIT29 | BIT30 | BIT31, 0x9);
	mac_ht_fa	= phy_query_mac_reg(adapt, REG_RXERR_RPT, bMaskLWord);/* [15:0]		 */

	/* Mac_DropPacket */
	rtw_write32(adapt, REG_RXERR_RPT, (rtw_read32(adapt, REG_RXERR_RPT) & 0x0FFFFFFF) | Mac_DropPacket);
	DropPacket = rtw_read32(adapt, REG_RXERR_RPT) & 0x0000FFFF;

	rx_counter->rx_pkt_ok = mac_cck_ok + mac_ofdm_ok + mac_ht_ok + mac_vht_ok;
	rx_counter->rx_pkt_crc_error = mac_cck_err + mac_ofdm_err + mac_ht_err + mac_vht_err;
	rx_counter->rx_cck_fa = mac_cck_fa;
	rx_counter->rx_ofdm_fa = mac_ofdm_fa;
	rx_counter->rx_ht_fa = mac_ht_fa;
	rx_counter->rx_pkt_drop = DropPacket;
}
void rtw_reset_mac_rx_counters(struct adapter *adapt)
{

	/* If no packet rx, MaxRx clock be gating ,BIT_DISGCLK bit19 set 1 for fix*/
	phy_set_mac_reg(adapt, REG_RCR, BIT19, 0x1);

	/* reset mac counter */
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT27, 0x1);
	phy_set_mac_reg(adapt, REG_RXERR_RPT, BIT27, 0x0);
}

void rtw_dump_phy_rx_counters(struct adapter *adapt, struct dbg_rx_counter *rx_counter)
{
	u32 cckok = 0, cckcrc = 0, ofdmok = 0, ofdmcrc = 0, htok = 0, htcrc = 0, OFDM_FA = 0, CCK_FA = 0, vht_ok = 0, vht_err = 0;
	if (!rx_counter) {
		rtw_warn_on(1);
		return;
	}
	cckok	= phy_query_bb_reg(adapt, 0xF88, bMaskDWord);
	ofdmok	= phy_query_bb_reg(adapt, 0xF94, bMaskLWord);
	htok		= phy_query_bb_reg(adapt, 0xF90, bMaskLWord);
	vht_ok	= 0;
	cckcrc	= phy_query_bb_reg(adapt, 0xF84, bMaskDWord);
	ofdmcrc	= phy_query_bb_reg(adapt, 0xF94, bMaskHWord);
	htcrc	= phy_query_bb_reg(adapt, 0xF90, bMaskHWord);
	vht_err	= 0;
	OFDM_FA = phy_query_bb_reg(adapt, 0xCF0, bMaskLWord) + phy_query_bb_reg(adapt, 0xCF2, bMaskLWord) +
		phy_query_bb_reg(adapt, 0xDA2, bMaskLWord) + phy_query_bb_reg(adapt, 0xDA4, bMaskLWord) +
		phy_query_bb_reg(adapt, 0xDA6, bMaskLWord) + phy_query_bb_reg(adapt, 0xDA8, bMaskLWord);

	CCK_FA = (rtw_read8(adapt, 0xA5B) << 8) | (rtw_read8(adapt, 0xA5C));

	rx_counter->rx_pkt_ok = cckok + ofdmok + htok + vht_ok;
	rx_counter->rx_pkt_crc_error = cckcrc + ofdmcrc + htcrc + vht_err;
	rx_counter->rx_ofdm_fa = OFDM_FA;
	rx_counter->rx_cck_fa = CCK_FA;

}

void rtw_reset_phy_trx_ok_counters(struct adapter *adapt)
{
}

void rtw_reset_phy_rx_counters(struct adapter *adapt)
{
	phy_set_bb_reg(adapt, 0xF14, BIT16, 0x1);
	rtw_msleep_os(10);
	phy_set_bb_reg(adapt, 0xF14, BIT16, 0x0);

	phy_set_bb_reg(adapt, 0xD00, BIT27, 0x1);/* reset  OFDA FA counter */
	phy_set_bb_reg(adapt, 0xC0C, BIT31, 0x1);/* reset  OFDA FA counter */
	phy_set_bb_reg(adapt, 0xD00, BIT27, 0x0);
	phy_set_bb_reg(adapt, 0xC0C, BIT31, 0x0);

	phy_set_bb_reg(adapt, 0xA2C, BIT15, 0x0);/* reset  CCK FA counter */
	phy_set_bb_reg(adapt, 0xA2C, BIT15, 0x1);
}

u8 rtw_get_current_tx_sgi(struct adapter *adapt, struct sta_info *psta)
{
	u8 curr_tx_sgi = 0;
	struct ra_sta_info *ra_info;

	if (!psta)
		return curr_tx_sgi;

	ra_info = &psta->cmn.ra_info;
	curr_tx_sgi = ((ra_info->curr_tx_rate) & 0x80) >> 7;

	return curr_tx_sgi;
}
u8 rtw_get_current_tx_rate(struct adapter *adapt, struct sta_info *psta)
{
	u8 rate_id = 0;
	struct ra_sta_info *ra_info;

	if (!psta)
		return rate_id;

	ra_info = &psta->cmn.ra_info;
	rate_id = ra_info->curr_tx_rate & 0x7f;

	return rate_id;
}

void update_IOT_info(struct adapter *adapt)
{
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	switch (pmlmeinfo->assoc_AP_vendor) {
	case HT_IOT_PEER_MARVELL:
		pmlmeinfo->turboMode_cts2self = 1;
		pmlmeinfo->turboMode_rtsen = 0;
		break;

	case HT_IOT_PEER_RALINK:
		pmlmeinfo->turboMode_cts2self = 0;
		pmlmeinfo->turboMode_rtsen = 1;
		break;
	case HT_IOT_PEER_REALTEK:
		/* rtw_write16(adapt, 0x4cc, 0xffff); */
		/* rtw_write16(adapt, 0x546, 0x01c0); */
		break;
	default:
		pmlmeinfo->turboMode_cts2self = 0;
		pmlmeinfo->turboMode_rtsen = 1;
		break;
	}

}

/* TODO: merge with phydm, see odm_SetCrystalCap() */
void hal_set_crystal_cap(struct adapter *adapter, u8 crystal_cap)
{
	crystal_cap = crystal_cap & 0x3F;

	switch (rtw_get_chip_type(adapter)) {
	case RTL8723B:
	case RTL8703B:
	case RTL8723D:
	case RTL8821:
	case RTL8192E:
		/* write 0x2C[23:18] = 0x2C[17:12] = CrystalCap */
		phy_set_bb_reg(adapter, REG_MAC_PHY_CTRL, 0x00FFF000, (crystal_cap | (crystal_cap << 6)));
		break;
	default:
		rtw_warn_on(1);
	}
}

int hal_spec_init(struct adapter *adapter)
{
	u8 interface_type = 0;
	int ret = _SUCCESS;

	interface_type = rtw_get_intf_type(adapter);

	switch (rtw_get_chip_type(adapter)) {
	case RTL8723D:
		init_hal_spec_8723d(adapter);
		break;
	default:
		RTW_ERR("%s: unknown chip_type:%u\n"
			, __func__, rtw_get_chip_type(adapter));
		ret = _FAIL;
		break;
	}

	return ret;
}

static const char *const _band_cap_str[] = {
	/* BIT0 */"2G",
	/* BIT1 */"5G",
};

static const char *const _bw_cap_str[] = {
	/* BIT0 */"5M",
	/* BIT1 */"10M",
	/* BIT2 */"20M",
	/* BIT3 */"40M",
	/* BIT4 */"80M",
	/* BIT5 */"160M",
	/* BIT6 */"80_80M",
};

static const char *const _proto_cap_str[] = {
	/* BIT0 */"b",
	/* BIT1 */"g",
	/* BIT2 */"n",
	/* BIT3 */"ac",
};

static const char *const _wl_func_str[] = {
	/* BIT0 */"P2P",
	/* BIT1 */"MIRACAST",
	/* BIT2 */"TDLS",
	/* BIT3 */"FTM",
};

void dump_hal_spec(void *sel, struct adapter *adapter)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	int i;

	RTW_PRINT_SEL(sel, "macid_num:%u\n", hal_spec->macid_num);
	RTW_PRINT_SEL(sel, "sec_cap:0x%02x\n", hal_spec->sec_cap);
	RTW_PRINT_SEL(sel, "sec_cam_ent_num:%u\n", hal_spec->sec_cam_ent_num);
	RTW_PRINT_SEL(sel, "rfpath_num_2g:%u\n", hal_spec->rfpath_num_2g);
	RTW_PRINT_SEL(sel, "rfpath_num_5g:%u\n", hal_spec->rfpath_num_5g);
	RTW_PRINT_SEL(sel, "max_tx_cnt:%u\n", hal_spec->max_tx_cnt);
	RTW_PRINT_SEL(sel, "tx_nss_num:%u\n", hal_spec->tx_nss_num);
	RTW_PRINT_SEL(sel, "rx_nss_num:%u\n", hal_spec->rx_nss_num);

	RTW_PRINT_SEL(sel, "band_cap:");
	for (i = 0; i < BAND_CAP_BIT_NUM; i++) {
		if (((hal_spec->band_cap) >> i) & BIT0 && _band_cap_str[i])
			_RTW_PRINT_SEL(sel, "%s ", _band_cap_str[i]);
	}
	_RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "bw_cap:");
	for (i = 0; i < BW_CAP_BIT_NUM; i++) {
		if (((hal_spec->bw_cap) >> i) & BIT0 && _bw_cap_str[i])
			_RTW_PRINT_SEL(sel, "%s ", _bw_cap_str[i]);
	}
	_RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "proto_cap:");
	for (i = 0; i < PROTO_CAP_BIT_NUM; i++) {
		if (((hal_spec->proto_cap) >> i) & BIT0 && _proto_cap_str[i])
			_RTW_PRINT_SEL(sel, "%s ", _proto_cap_str[i]);
	}
	_RTW_PRINT_SEL(sel, "\n");

	RTW_PRINT_SEL(sel, "wl_func:");
	for (i = 0; i < WL_FUNC_BIT_NUM; i++) {
		if (((hal_spec->wl_func) >> i) & BIT0 && _wl_func_str[i])
			_RTW_PRINT_SEL(sel, "%s ", _wl_func_str[i]);
	}
	_RTW_PRINT_SEL(sel, "\n");
}

inline bool hal_chk_band_cap(struct adapter *adapter, u8 cap)
{
	return GET_HAL_SPEC(adapter)->band_cap & cap;
}

inline bool hal_chk_bw_cap(struct adapter *adapter, u8 cap)
{
	return GET_HAL_SPEC(adapter)->bw_cap & cap;
}

inline bool hal_chk_proto_cap(struct adapter *adapter, u8 cap)
{
	return GET_HAL_SPEC(adapter)->proto_cap & cap;
}

inline bool hal_chk_wl_func(struct adapter *adapter, u8 func)
{
	return GET_HAL_SPEC(adapter)->wl_func & func;
}

inline bool hal_is_band_support(struct adapter *adapter, u8 band)
{
	return GET_HAL_SPEC(adapter)->band_cap & band_to_band_cap(band);
}

inline bool hal_is_bw_support(struct adapter *adapter, u8 bw)
{
	return GET_HAL_SPEC(adapter)->bw_cap & ch_width_to_bw_cap(bw);
}

inline bool hal_is_wireless_mode_support(struct adapter *adapter, u8 mode)
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
* @in_bw: starting bw, value of enum channel_width
*
* Returns: value of enum channel_width
*/
u8 hal_largest_bw(struct adapter *adapter, u8 in_bw)
{
	for (; in_bw > CHANNEL_WIDTH_20; in_bw--) {
		if (hal_is_bw_support(adapter, in_bw))
			break;
	}

	if (!hal_is_bw_support(adapter, in_bw))
		rtw_warn_on(1);

	return in_bw;
}

void rtw_hal_correct_tsf(struct adapter *adapt, u8 hw_port, u64 tsf)
{
	if (hw_port == HW_PORT0) {
		/*disable related TSF function*/
		rtw_write8(adapt, REG_BCN_CTRL, rtw_read8(adapt, REG_BCN_CTRL) & (~EN_BCN_FUNCTION));

		rtw_write32(adapt, REG_TSFTR, tsf);
		rtw_write32(adapt, REG_TSFTR + 4, tsf >> 32);

		/*enable related TSF function*/
		rtw_write8(adapt, REG_BCN_CTRL, rtw_read8(adapt, REG_BCN_CTRL) | EN_BCN_FUNCTION);
	} else if (hw_port == HW_PORT1) {
		/*disable related TSF function*/
		rtw_write8(adapt, REG_BCN_CTRL_1, rtw_read8(adapt, REG_BCN_CTRL_1) & (~EN_BCN_FUNCTION));

		rtw_write32(adapt, REG_TSFTR1, tsf);
		rtw_write32(adapt, REG_TSFTR1 + 4, tsf >> 32);

		/*enable related TSF function*/
		rtw_write8(adapt, REG_BCN_CTRL_1, rtw_read8(adapt, REG_BCN_CTRL_1) | EN_BCN_FUNCTION);
	} else
		RTW_INFO("%s-[WARN] "ADPT_FMT" invalid hw_port:%d\n", __func__, ADPT_ARG(adapt), hw_port);
}

void ResumeTxBeacon(struct adapter *adapt)
{
#ifdef CONFIG_MI_WITH_MBSSID_CAM
#else
	rtw_write8(adapt, REG_FWHW_TXQ_CTRL + 2,
		rtw_read8(adapt, REG_FWHW_TXQ_CTRL + 2) | BIT(6));

	/* TBTT hold time: 0x540[19:8] */
	rtw_write8(adapt, REG_TBTT_PROHIBIT + 1, TBTT_PROHIBIT_HOLD_TIME & 0xFF);
	rtw_write8(adapt, REG_TBTT_PROHIBIT + 2,
		(rtw_read8(adapt, REG_TBTT_PROHIBIT + 2) & 0xF0) | (TBTT_PROHIBIT_HOLD_TIME >> 8));
#endif
}

void StopTxBeacon(struct adapter *adapt)
{
#ifdef CONFIG_MI_WITH_MBSSID_CAM
#else
	rtw_write8(adapt, REG_FWHW_TXQ_CTRL + 2,
		rtw_read8(adapt, REG_FWHW_TXQ_CTRL + 2) & (~BIT6));

	/* TBTT hold time: 0x540[19:8] */
	rtw_write8(adapt, REG_TBTT_PROHIBIT + 1, TBTT_PROHIBIT_HOLD_TIME_STOP_BCN & 0xFF);
	rtw_write8(adapt, REG_TBTT_PROHIBIT + 2,
		(rtw_read8(adapt, REG_TBTT_PROHIBIT + 2) & 0xF0) | (TBTT_PROHIBIT_HOLD_TIME_STOP_BCN >> 8));
#endif
}

#ifdef CONFIG_MI_WITH_MBSSID_CAM /*HW port0 - MBSS*/
void hw_var_set_opmode_mbid(struct adapter *Adapter, u8 mode)
{
	RTW_INFO("%s()-"ADPT_FMT" mode = %d\n", __func__, ADPT_ARG(Adapter), mode);

	rtw_hal_rcr_set_chk_bssid(Adapter, MLME_ACTION_NONE);

	/* disable Port0 TSF update*/
	rtw_write8(Adapter, REG_BCN_CTRL, rtw_read8(Adapter, REG_BCN_CTRL) | DIS_TSF_UDT);

	/* set net_type */
	Set_MSR(Adapter, mode);

	if ((mode == _HW_STATE_STATION_) || (mode == _HW_STATE_NOLINK_)) {
		if (!rtw_mi_get_ap_num(Adapter) && !rtw_mi_get_mesh_num(Adapter))
			StopTxBeacon(Adapter);

		rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_ATIM);/*disable atim wnd*/
	} else if (mode == _HW_STATE_ADHOC_) {
		ResumeTxBeacon(Adapter);
		rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_BCNQ_SUB);

	} else if (mode == _HW_STATE_AP_) {
		ResumeTxBeacon(Adapter);

		rtw_write8(Adapter, REG_BCN_CTRL, DIS_TSF_UDT | DIS_BCNQ_SUB);

		/*enable to rx data frame*/
		rtw_write16(Adapter, REG_RXFLTMAP2, 0xFFFF);

		/*Beacon Control related register for first time*/
		rtw_write8(Adapter, REG_BCNDMATIM, 0x02); /* 2ms */

		/*rtw_write8(Adapter, REG_BCN_MAX_ERR, 0xFF);*/
		rtw_write8(Adapter, REG_ATIMWND, 0x0c); /* 12ms */
		rtw_write16(Adapter, REG_BCNTCFG, 0x00);

		rtw_write16(Adapter, REG_TSFTR_SYN_OFFSET, 0x7fff);/* +32767 (~32ms) */

		/*reset TSF*/
		rtw_write8(Adapter, REG_DUAL_TSF_RST, BIT(0));

		/*enable BCN0 Function for if1*/
		/*don't enable update TSF0 for if1 (due to TSF update when beacon,probe rsp are received)*/
		rtw_write8(Adapter, REG_BCN_CTRL, (DIS_TSF_UDT | EN_BCN_FUNCTION | EN_TXBCN_RPT | DIS_BCNQ_SUB));
		#ifdef CONFIG_BCN_XMIT_PROTECT
		rtw_write8(Adapter, REG_CCK_CHECK, rtw_read8(Adapter, REG_CCK_CHECK) | BIT_EN_BCN_PKT_REL);
		#endif
	}
}
#endif

#ifdef CONFIG_PHY_CAPABILITY_QUERY
void rtw_dump_phy_cap_by_phydmapi(void *sel, struct adapter *adapter)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapter);
	struct phy_spec_t *phy_spec = &pHalData->phy_spec;

	RTW_PRINT_SEL(sel, "[PHY SPEC] TRx Capability : 0x%08x\n", phy_spec->trx_cap);
	RTW_PRINT_SEL(sel, "[PHY SPEC] Tx Stream Num Index : %d\n", (phy_spec->trx_cap >> 24) & 0xFF); /*Tx Stream Num Index [31:24]*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] Rx Stream Num Index : %d\n", (phy_spec->trx_cap >> 16) & 0xFF); /*Rx Stream Num Index [23:16]*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] Tx Path Num Index : %d\n", (phy_spec->trx_cap >> 8) & 0xFF);/*Tx Path Num Index	[15:8]*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] Rx Path Num Index : %d\n\n", (phy_spec->trx_cap & 0xFF));/*Rx Path Num Index	[7:0]*/

	RTW_PRINT_SEL(sel, "[PHY SPEC] STBC Capability : 0x%08x\n", phy_spec->stbc_cap);
	RTW_PRINT_SEL(sel, "[PHY SPEC] VHT STBC Tx : %s\n", ((phy_spec->stbc_cap >> 24) & 0xFF) ? "Supported" : "N/A"); /*VHT STBC Tx [31:24]*/
	/*VHT STBC Rx [23:16]
	0 = not support
	1 = support for 1 spatial stream
	2 = support for 1 or 2 spatial streams
	3 = support for 1 or 2 or 3 spatial streams
	4 = support for 1 or 2 or 3 or 4 spatial streams*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] VHT STBC Rx :%d\n", ((phy_spec->stbc_cap >> 16) & 0xFF));
	RTW_PRINT_SEL(sel, "[PHY SPEC] HT STBC Tx : %s\n", ((phy_spec->stbc_cap >> 8) & 0xFF) ? "Supported" : "N/A"); /*HT STBC Tx [15:8]*/
	/*HT STBC Rx [7:0]
	0 = not support
	1 = support for 1 spatial stream
	2 = support for 1 or 2 spatial streams
	3 = support for 1 or 2 or 3 spatial streams*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] HT STBC Rx : %d\n\n", (phy_spec->stbc_cap & 0xFF));

	RTW_PRINT_SEL(sel, "[PHY SPEC] LDPC Capability : 0x%08x\n", phy_spec->ldpc_cap);
	RTW_PRINT_SEL(sel, "[PHY SPEC] VHT LDPC Tx : %s\n", ((phy_spec->ldpc_cap >> 24) & 0xFF) ? "Supported" : "N/A"); /*VHT LDPC Tx [31:24]*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] VHT LDPC Rx : %s\n", ((phy_spec->ldpc_cap >> 16) & 0xFF) ? "Supported" : "N/A"); /*VHT LDPC Rx [23:16]*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] HT LDPC Tx : %s\n", ((phy_spec->ldpc_cap >> 8) & 0xFF) ? "Supported" : "N/A"); /*HT LDPC Tx [15:8]*/
	RTW_PRINT_SEL(sel, "[PHY SPEC] HT LDPC Rx : %s\n\n", (phy_spec->ldpc_cap & 0xFF) ? "Supported" : "N/A"); /*HT LDPC Rx [7:0]*/
}
#else
static void rtw_dump_phy_cap_by_hal(void *sel, struct adapter *adapter)
{
	u8 phy_cap = false;

	/* STBC */
	rtw_hal_get_def_var(adapter, HAL_DEF_TX_STBC, (u8 *)&phy_cap);
	RTW_PRINT_SEL(sel, "[HAL] STBC Tx : %s\n", (phy_cap) ? "Supported" : "N/A");

	phy_cap = false;
	rtw_hal_get_def_var(adapter, HAL_DEF_RX_STBC, (u8 *)&phy_cap);
	RTW_PRINT_SEL(sel, "[HAL] STBC Rx : %s\n\n", (phy_cap) ? "Supported" : "N/A");

	/* LDPC support */
	phy_cap = false;
	rtw_hal_get_def_var(adapter, HAL_DEF_TX_LDPC, (u8 *)&phy_cap);
	RTW_PRINT_SEL(sel, "[HAL] LDPC Tx : %s\n", (phy_cap) ? "Supported" : "N/A");

	phy_cap = false;
	rtw_hal_get_def_var(adapter, HAL_DEF_RX_LDPC, (u8 *)&phy_cap);
	RTW_PRINT_SEL(sel, "[HAL] LDPC Rx : %s\n\n", (phy_cap) ? "Supported" : "N/A");
}
#endif
void rtw_dump_phy_cap(void *sel, struct adapter *adapter)
{
	RTW_PRINT_SEL(sel, "\n ======== PHY Capability ========\n");
#ifdef CONFIG_PHY_CAPABILITY_QUERY
	rtw_dump_phy_cap_by_phydmapi(sel, adapter);
#else
	rtw_dump_phy_cap_by_hal(sel, adapter);
#endif
}

inline s16 translate_dbm_to_percentage(s16 signal)
{
	if ((signal <= -100) || (signal >= 20))
		return	0;
	else if (signal >= 0)
		return	100;
	else
		return 100 + signal;
}

#ifdef RTW_CHANNEL_SWITCH_OFFLOAD
void rtw_hal_switch_chnl_and_set_bw_offload(struct adapter *adapter, u8 central_ch, u8 pri_ch_idx, u8 bw)
{
	u8 h2c[H2C_SINGLE_CHANNELSWITCH_V2_LEN] = {0};
	struct hal_com_data * hal;
	struct submit_ctx *chsw_sctx;

	hal = GET_HAL_DATA(adapter);
	chsw_sctx = &hal->chsw_sctx;

	SET_H2CCMD_SINGLE_CH_SWITCH_V2_CENTRAL_CH_NUM(h2c, central_ch);
	SET_H2CCMD_SINGLE_CH_SWITCH_V2_PRIMARY_CH_IDX(h2c, pri_ch_idx);
	SET_H2CCMD_SINGLE_CH_SWITCH_V2_BW(h2c, bw);

	rtw_sctx_init(chsw_sctx, 100);
	rtw_hal_fill_h2c_cmd(adapter, H2C_SINGLE_CHANNELSWITCH_V2, H2C_SINGLE_CHANNELSWITCH_V2_LEN, h2c);
	rtw_sctx_wait(chsw_sctx, __func__);
}
#endif /* RTW_CHANNEL_SWITCH_OFFLOAD */
