/******************************************************************************
 *
 * Copyright(c) 2007 - 2019 Realtek Corporation.
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

#include <drv_types.h>
#include <hal_data.h>
#ifdef CONFIG_RTW_80211K
#include "rtw_rm_fsm.h"
#include "rtw_rm_util.h"

/* 802.11-2012 Table E-1 Operationg classes in United States */
static RT_OPERATING_CLASS RTW_OP_CLASS_US[] = {
	/* 0, OP_CLASS_NULL */	{  0,  0, {}},
	/* 1, OP_CLASS_1 */	{115,  4, {36, 40, 44, 48}},
	/* 2, OP_CLASS_2 */	{118,  4, {52, 56, 60, 64}},
	/* 3, OP_CLASS_3 */	{124,  4, {149, 153, 157, 161}},
	/* 4, OP_CLASS_4 */	{121, 11, {100, 104, 108, 112, 116, 120, 124,
						128, 132, 136, 140}},
	/* 5, OP_CLASS_5 */	{125,  5, {149, 153, 157, 161, 165}},
	/* 6, OP_CLASS_12 */	{ 81, 11, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}}
};

u8 rm_get_ch_set(
	struct rtw_ieee80211_channel *pch_set, u8 op_class, u8 ch_num)
{
	int i,j,sz;
	u8 ch_amount = 0;


	sz = sizeof(RTW_OP_CLASS_US)/sizeof(struct _RT_OPERATING_CLASS);

	if (ch_num != 0) {
		pch_set[0].hw_value = ch_num;
		ch_amount = 1;
		RTW_INFO("RM: meas_ch->hw_value = %u\n", pch_set->hw_value);
		goto done;
	}

	for (i = 0; i < sz; i++) {

		if (RTW_OP_CLASS_US[i].global_op_class == op_class) {

			for (j = 0; j < RTW_OP_CLASS_US[i].Len; j++) {
				pch_set[j].hw_value =
					RTW_OP_CLASS_US[i].Channel[j];
				RTW_INFO("RM: meas_ch[%d].hw_value = %u\n",
					j, pch_set[j].hw_value);
			}
			ch_amount = RTW_OP_CLASS_US[i].Len;
			break;
		}
	}
done:
	return ch_amount;
}

u8 rm_get_oper_class_via_ch(u8 ch)
{
	int i,j,sz;


	sz = sizeof(RTW_OP_CLASS_US)/sizeof(struct _RT_OPERATING_CLASS);

	for (i = 0; i < sz; i++) {
		for (j = 0; j < RTW_OP_CLASS_US[i].Len; j++) {
			if ( ch == RTW_OP_CLASS_US[i].Channel[j]) {
				RTW_INFO("RM: ch %u in oper_calss %u\n",
					ch, RTW_OP_CLASS_US[i].global_op_class);
				return RTW_OP_CLASS_US[i].global_op_class;
				break;
			}
		}
	}
	return 0;
}

int is_wildcard_bssid(u8 *bssid)
{
	int i;
	u8 val8 = 0xff;


	for (i=0;i<6;i++)
		val8 &= bssid[i];

	if (val8 == 0xff)
		return _SUCCESS;
	return _FALSE;
}

u8 translate_dbm_to_rcpi(s8 SignalPower)
{
	/* RCPI = Int{(Power in dBm + 110)*2} for 0dBm > Power > -110dBm
	 *    0	: power <= -110.0 dBm
	 *    1	: power =  -109.5 dBm
	 *    2	: power =  -109.0 dBm
	 */
	return (SignalPower + 110)*2;
}

u8 translate_percentage_to_rcpi(u32 SignalStrengthIndex)
{
	/* Translate to dBm (x=y-100) */
	return translate_dbm_to_rcpi(SignalStrengthIndex - 100);
}

u8 rm_get_bcn_rcpi(struct rm_obj *prm, struct wlan_network *pnetwork)
{
	return translate_percentage_to_rcpi(
		pnetwork->network.PhyInfo.SignalStrength);
}

u8 rm_get_frame_rsni(struct rm_obj *prm, union recv_frame *pframe)
{
	int i;
	u8 val8, snr;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(prm->psta->padapter);

	if (IS_CCK_RATE((hw_rate_to_m_rate(pframe->u.hdr.attrib.data_rate))))
		val8 = 255;
	else {
		snr = 0;
		for (i = 0; i < pHalData->NumTotalRFPath; i++)
			snr += pframe->u.hdr.attrib.phy_info.rx_snr[i];
		snr = snr / pHalData->NumTotalRFPath;
		val8 = (u8)(snr + 10)*2;
	}
	return val8;
}

u8 rm_get_bcn_rsni(struct rm_obj *prm, struct wlan_network *pnetwork)
{
	int i;
	u8 val8, snr;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(prm->psta->padapter);


	if (pnetwork->network.PhyInfo.is_cck_rate) {
		/* current HW doesn't have CCK RSNI */
		/* 255 indicates RSNI is unavailable */
		val8 = 255;
	} else {
		snr = 0;
		for (i = 0; i < pHalData->NumTotalRFPath; i++) {
			snr += pnetwork->network.PhyInfo.rx_snr[i];
		}
		snr = snr / pHalData->NumTotalRFPath;
		val8 = (u8)(snr + 10)*2;
	}
	return val8;
}

char *get_rate_name(enum MGN_RATE rate)
{
	switch(rate) {
	case MGN_1M:
		return "CCK_1M";
		break;
	case MGN_2M:
		return "CCK_2M";
		break;
	case MGN_5_5M:
		return "CCK_5.5M";
		break;
	case MGN_11M:
		return "CCK_11M";
		break;
	case MGN_6M:
		return "OFDM_6M";
		break;
	case MGN_9M:
		return "OFDM_9M";
		break;
	case MGN_12M:
		return "OFDM_12M";
		break;
	case MGN_18M:
		return "OFDM_18M";
		break;
	case MGN_24M:
		return "OFDM_24M";
		break;
	case MGN_36M:
		return "OFDM_36M";
		break;
	case MGN_48M:
		return "OFDM_48M";
		break;
	case MGN_54M:
		return "OFDM_54M";
		break;
	case MGN_MCS0:
		return "HT_MCS0";
		break;
	case MGN_MCS8:
		return "HT_MCS8";
		break;
	case MGN_MCS16:
		return "HT_MCS16";
		break;
	case MGN_MCS24:
		return "HT_MCS24";
		break;
	case MGN_MCS32:
		return "HT_MCS32";
		break;
	case MGN_MCS1:
		return "HT_MCS1";
		break;
	case MGN_MCS9:
		return "HT_MCS9";
		break;
	case MGN_MCS17:
		return "HT_MCS17";
		break;
	case MGN_MCS25:
		return "HT_MCS25";
		break;
	case MGN_MCS2:
		return "HT_MCS2";
		break;
	case MGN_MCS10:
		return "HT_MCS10";
		break;
	case MGN_MCS18:
		return "HT_MCS18";
		break;
	case MGN_MCS26:
		return "HT_MCS26";
		break;
	case MGN_MCS3:
		return "HT_MCS3";
		break;
	case MGN_MCS11:
		return "HT_MCS11";
		break;
	case MGN_MCS19:
		return "HT_MCS19";
		break;
	case MGN_MCS27:
		return "HT_MCS27";
		break;
	case MGN_MCS4:
		return "HT_MCS4";
		break;
	case MGN_MCS12:
		return "HT_MCS12";
		break;
	case MGN_MCS20:
		return "HT_MCS20";
		break;
	case MGN_MCS28:
		return "HT_MCS28";
		break;
	case MGN_MCS5:
		return "HT_MCS5";
		break;
	case MGN_MCS13:
		return "HT_MCS13";
		break;
	case MGN_MCS21:
		return "HT_MCS21";
		break;
	case MGN_MCS29:
		return "HT_MCS29";
		break;
	case MGN_MCS6:
		return "HT_MCS6";
		break;
	case MGN_MCS14:
		return "HT_MCS14";
		break;
	case MGN_MCS22:
		return "HT_MCS22";
		break;
	case MGN_MCS30:
		return "HT_MCS30";
		break;
	case MGN_MCS7:
		return "HT_MCS7";
		break;
	case MGN_MCS15:
		return "HT_MCS15";
		break;
	case MGN_MCS23:
		return "HT_MCS23";
		break;
	case MGN_MCS31:
		return "HT_MCS31";
		break;
	case MGN_VHT1SS_MCS0:
		return "VHT1SS_MCS0";
		break;
	case MGN_VHT2SS_MCS0:
		return "VHT2SS_MCS0";
		break;
	case MGN_VHT3SS_MCS0:
		return "VHT3SS_MCS0";
		break;
	case MGN_VHT4SS_MCS0:
		return "VHT4SS_MCS0";
		break;
	case MGN_VHT1SS_MCS1:
		return "VHT1SS_MCS1";
		break;
	case MGN_VHT2SS_MCS1:
		return "VHT2SS_MCS1";
		break;
	case MGN_VHT3SS_MCS1:
		return "VHT3SS_MCS1";
		break;
	case MGN_VHT4SS_MCS1:
		return "VHT4SS_MCS1";
		break;
	case MGN_VHT1SS_MCS2:
		return "VHT1SS_MCS2";
		break;
	case MGN_VHT2SS_MCS2:
		return "VHT2SS_MCS2";
		break;
	case MGN_VHT3SS_MCS2:
		return "VHT3SS_MCS2";
		break;
	case MGN_VHT4SS_MCS2:
		return "VHT4SS_MCS2";
		break;
	case MGN_VHT1SS_MCS3:
		return "VHT1SS_MCS3";
		break;
	case MGN_VHT2SS_MCS3:
		return "VHT2SS_MCS3";
		break;
	case MGN_VHT3SS_MCS3:
		return "VHT3SS_MCS3";
		break;
	case MGN_VHT4SS_MCS3:
		return "VHT4SS_MCS3";
		break;
	case MGN_VHT1SS_MCS4:
		return "VHT1SS_MCS4";
		break;
	case MGN_VHT2SS_MCS4:
		return "VHT2SS_MCS4";
		break;
	case MGN_VHT3SS_MCS4:
		return "VHT3SS_MCS4";
		break;
	case MGN_VHT4SS_MCS4:
		return "VHT4SS_MCS4";
		break;
	case MGN_VHT1SS_MCS5:
		return "VHT1SS_MCS5";
		break;
	case MGN_VHT2SS_MCS5:
		return "VHT2SS_MCS5";
		break;
	case MGN_VHT3SS_MCS5:
		return "VHT3SS_MCS5";
		break;
	case MGN_VHT4SS_MCS5:
		return "VHT4SS_MCS5";
		break;
	case MGN_VHT1SS_MCS6:
		return "VHT1SS_MCS6";
		break;
	case MGN_VHT2SS_MCS6:
		return "VHT2SS_MCS6";
		break;
	case MGN_VHT3SS_MCS6:
		return "VHT3SS_MCS6";
		break;
	case MGN_VHT4SS_MCS6:
		return "VHT4SS_MCS6";
		break;
	case MGN_VHT1SS_MCS7:
		return "VHT1SS_MCS7";
		break;
	case MGN_VHT2SS_MCS7:
		return "VHT2SS_MCS7";
		break;
	case MGN_VHT3SS_MCS7:
		return "VHT3SS_MCS7";
		break;
	case MGN_VHT4SS_MCS7:
		return "VHT4SS_MCS7";
		break;
	case MGN_VHT1SS_MCS8:
		return "VHT1SS_MCS8";
		break;
	case MGN_VHT2SS_MCS8:
		return "VHT2SS_MCS8";
		break;
	case MGN_VHT3SS_MCS8:
		return "VHT3SS_MCS8";
		break;
	case MGN_VHT4SS_MCS8:
		return "VHT4SS_MCS8";
		break;
	case MGN_VHT1SS_MCS9:
		return "VHT1SS_MCS9";
		break;
	case MGN_VHT2SS_MCS9:
		return "VHT2SS_MCS9";
		break;
	case MGN_VHT3SS_MCS9:
		return "VHT3SS_MCS9";
		break;
	case MGN_VHT4SS_MCS9:
		return "VHT4SS_MCS9";
		break;
	default:
		break;

	}
	return "UNKNOWN";
}

static int get_rate_index(enum MGN_RATE rate)
{
	int rate_num = -1;

	switch(rate) {
	case MGN_1M:
		rate_num = 0;
		break;
	case MGN_2M:
		rate_num = 1;
		break;
	case MGN_5_5M:
		rate_num = 2;
		break;
	case MGN_11M:
		rate_num = 3;
		break;
	case MGN_6M:
		rate_num = 0;
		break;
	case MGN_9M:
		rate_num = 1;
		break;
	case MGN_12M:
		rate_num = 2;
		break;
	case MGN_18M:
		rate_num = 3;
		break;
	case MGN_24M:
		rate_num = 4;
		break;
	case MGN_36M:
		rate_num = 5;
		break;
	case MGN_48M:
		rate_num = 6;
		break;
	case MGN_54M:
		rate_num = 7;
		break;
	case MGN_MCS0:
	case MGN_MCS8:
	case MGN_MCS16:
	case MGN_MCS24:
	case MGN_MCS32:
		rate_num = 0;
		break;
	case MGN_MCS1:
	case MGN_MCS9:
	case MGN_MCS17:
	case MGN_MCS25:
		rate_num = 1;
		break;
	case MGN_MCS2:
	case MGN_MCS10:
	case MGN_MCS18:
	case MGN_MCS26:
		rate_num = 2;
		break;
	case MGN_MCS3:
	case MGN_MCS11:
	case MGN_MCS19:
	case MGN_MCS27:
		rate_num = 3;
		break;
	case MGN_MCS4:
	case MGN_MCS12:
	case MGN_MCS20:
	case MGN_MCS28:
		rate_num = 4;
		break;
	case MGN_MCS5:
	case MGN_MCS13:
	case MGN_MCS21:
	case MGN_MCS29:
		rate_num = 5;
		break;
	case MGN_MCS6:
	case MGN_MCS14:
	case MGN_MCS22:
	case MGN_MCS30:
		rate_num = 6;
		break;
	case MGN_MCS7:
	case MGN_MCS15:
	case MGN_MCS23:
	case MGN_MCS31:
		rate_num = 7;
		break;
	case MGN_VHT1SS_MCS0:
	case MGN_VHT2SS_MCS0:
	case MGN_VHT3SS_MCS0:
	case MGN_VHT4SS_MCS0:
		rate_num = 0;
		break;
	case MGN_VHT1SS_MCS1:
	case MGN_VHT2SS_MCS1:
	case MGN_VHT3SS_MCS1:
	case MGN_VHT4SS_MCS1:
		rate_num = 1;
		break;
	case MGN_VHT1SS_MCS2:
	case MGN_VHT2SS_MCS2:
	case MGN_VHT3SS_MCS2:
	case MGN_VHT4SS_MCS2:
		rate_num = 2;
		break;
	case MGN_VHT1SS_MCS3:
	case MGN_VHT2SS_MCS3:
	case MGN_VHT3SS_MCS3:
	case MGN_VHT4SS_MCS3:
		rate_num = 3;
		break;
	case MGN_VHT1SS_MCS4:
	case MGN_VHT2SS_MCS4:
	case MGN_VHT3SS_MCS4:
	case MGN_VHT4SS_MCS4:
		rate_num = 4;
		break;
	case MGN_VHT1SS_MCS5:
	case MGN_VHT2SS_MCS5:
	case MGN_VHT3SS_MCS5:
	case MGN_VHT4SS_MCS5:
		rate_num = 5;
		break;
	case MGN_VHT1SS_MCS6:
	case MGN_VHT2SS_MCS6:
	case MGN_VHT3SS_MCS6:
	case MGN_VHT4SS_MCS6:
		rate_num = 6;
		break;
	case MGN_VHT1SS_MCS7:
	case MGN_VHT2SS_MCS7:
	case MGN_VHT3SS_MCS7:
	case MGN_VHT4SS_MCS7:
		rate_num = 7;
		break;
	case MGN_VHT1SS_MCS8:
	case MGN_VHT2SS_MCS8:
	case MGN_VHT3SS_MCS8:
	case MGN_VHT4SS_MCS8:
		rate_num = 8;
		break;
	case MGN_VHT1SS_MCS9:
	case MGN_VHT2SS_MCS9:
	case MGN_VHT3SS_MCS9:
	case MGN_VHT4SS_MCS9:
		rate_num = 9;
		break;
	default:
		break;

	}
	return rate_num;
}

/* output: pwr (unit dBm) */
int rm_get_tx_power(PADAPTER adapter, enum rf_path path, enum MGN_RATE rate, s8 *pwr)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	int tx_num, band, bw, ch, n, rs;
	u8 base;
	s8 limt_offset = 127; /* max value of s8 */
	s8 rate_offset;
	s8 powr_offset;
	int rate_pos;


	band = hal_data->current_band_type;
	bw = hal_data->current_channel_bw;
	ch = hal_data->current_channel;

	if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
		return -1;

	if (HAL_IsLegalChannel(adapter, ch) == _FALSE) {
		RTW_INFO("Illegal channel!!\n");
		return -2;
	}

	/* find rs by rate */
	if (IS_CCK_RATE(rate))
		rs = CCK;
	else if (IS_OFDM_RATE(rate))
		rs = OFDM;
	else if (IS_HT1SS_RATE(rate))
		rs = HT_1SS;
	else if (IS_HT2SS_RATE(rate))
		rs = HT_2SS;
	else if (IS_HT3SS_RATE(rate))
		rs = HT_3SS;
	else if (IS_HT4SS_RATE(rate))
		rs = HT_4SS;
	else if (IS_VHT1SS_RATE(rate))
		rs = VHT_1SS;
	else if (IS_VHT2SS_RATE(rate))
		rs = VHT_2SS;
	else if (IS_VHT3SS_RATE(rate))
		rs = VHT_3SS;
	else if (IS_VHT4SS_RATE(rate))
		rs = VHT_4SS;
	else
		return -3;

	tx_num = rate_section_to_tx_num(rs);

	if (tx_num >= hal_spec->tx_nss_num)
		return -4;

	if (band == BAND_ON_5G && IS_CCK_RATE_SECTION(rs))
		return -5;

	if (IS_VHT_RATE_SECTION(rs) && !IS_HARDWARE_TYPE_JAGUAR_ALL(adapter))
		return -6;

	base = PHY_GetTxPowerByRateBase(adapter, band, path, rs);

	/* get power by rate in db */
	rate_pos = get_rate_index(rate);
	if (rate_pos < 0)
		return -7;

	rate_offset = PHY_GetTxPowerByRate(adapter, band, path,
		rates_by_sections[rs].rates[rate_pos]);
#ifdef CONFIG_TXPWR_LIMIT
	limt_offset = PHY_GetTxPowerLimit(adapter, NULL, band, bw,
		path, rates_by_sections[rs].rates[rate_pos], RF_1TX, ch);
#endif
	powr_offset = MIN(rate_offset, limt_offset);

	*pwr = (base + powr_offset) / hal_spec->txgi_pdbm;
#if (RM_MORE_DBG_MSG)
	RTW_INFO("RM: [%s][path_%c] %s base=%d offset=min(%d|%d)=%d (%d,%d)\n",
		band_str(band), rf_path_char(path),
		MGN_RATE_STR(rate),base, rate_offset, limt_offset,
		powr_offset, ((base + rate_offset) / hal_spec->txgi_pdbm), *pwr);
#endif
	return 0;
}

int rm_get_rx_sensitivity(PADAPTER adapter, enum channel_width bw, enum MGN_RATE rate, s8 *pwr)
{
	s8 rx_sensitivity = -110;

	switch(rate) {
	case MGN_1M:
		rx_sensitivity= -101;
		break;
	case MGN_2M:
		rx_sensitivity= -98;
		break;
	case MGN_5_5M:
		rx_sensitivity= -92;
		break;
	case MGN_11M:
		rx_sensitivity= -89;
		break;
	case MGN_6M:
	case MGN_9M:
	case MGN_12M:
		rx_sensitivity = -92;
		break;
	case MGN_18M:
		rx_sensitivity = -90;
		break;
	case MGN_24M:
		rx_sensitivity = -88;
		break;
	case MGN_36M:
		rx_sensitivity = -84;
		break;
	case MGN_48M:
		rx_sensitivity = -79;
		break;
	case MGN_54M:
		rx_sensitivity = -78;
		break;

	case MGN_MCS0:
	case MGN_MCS8:
	case MGN_MCS16:
	case MGN_MCS24:
	case MGN_VHT1SS_MCS0:
	case MGN_VHT2SS_MCS0:
	case MGN_VHT3SS_MCS0:
	case MGN_VHT4SS_MCS0:
		/* BW20 BPSK 1/2 */
		rx_sensitivity = -82;
		break;

	case MGN_MCS1:
	case MGN_MCS9:
	case MGN_MCS17:
	case MGN_MCS25:
	case MGN_VHT1SS_MCS1:
	case MGN_VHT2SS_MCS1:
	case MGN_VHT3SS_MCS1:
	case MGN_VHT4SS_MCS1:
		/* BW20 QPSK 1/2 */
		rx_sensitivity = -79;
		break;

	case MGN_MCS2:
	case MGN_MCS10:
	case MGN_MCS18:
	case MGN_MCS26:
	case MGN_VHT1SS_MCS2:
	case MGN_VHT2SS_MCS2:
	case MGN_VHT3SS_MCS2:
	case MGN_VHT4SS_MCS2:
		/* BW20 QPSK 3/4 */
		rx_sensitivity = -77;
		break;

	case MGN_MCS3:
	case MGN_MCS11:
	case MGN_MCS19:
	case MGN_MCS27:
	case MGN_VHT1SS_MCS3:
	case MGN_VHT2SS_MCS3:
	case MGN_VHT3SS_MCS3:
	case MGN_VHT4SS_MCS3:
		/* BW20 16-QAM 1/2 */
		rx_sensitivity = -74;
		break;

	case MGN_MCS4:
	case MGN_MCS12:
	case MGN_MCS20:
	case MGN_MCS28:
	case MGN_VHT1SS_MCS4:
	case MGN_VHT2SS_MCS4:
	case MGN_VHT3SS_MCS4:
	case MGN_VHT4SS_MCS4:
		/* BW20 16-QAM 3/4 */
		rx_sensitivity = -70;
		break;

	case MGN_MCS5:
	case MGN_MCS13:
	case MGN_MCS21:
	case MGN_MCS29:
	case MGN_VHT1SS_MCS5:
	case MGN_VHT2SS_MCS5:
	case MGN_VHT3SS_MCS5:
	case MGN_VHT4SS_MCS5:
		/* BW20 64-QAM 2/3 */
		rx_sensitivity = -66;
		break;

	case MGN_MCS6:
	case MGN_MCS14:
	case MGN_MCS22:
	case MGN_MCS30:
	case MGN_VHT1SS_MCS6:
	case MGN_VHT2SS_MCS6:
	case MGN_VHT3SS_MCS6:
	case MGN_VHT4SS_MCS6:
		/* BW20 64-QAM 3/4 */
		rx_sensitivity = -65;
		break;

	case MGN_MCS7:
	case MGN_MCS15:
	case MGN_MCS23:
	case MGN_MCS31:
	case MGN_VHT1SS_MCS7:
	case MGN_VHT2SS_MCS7:
	case MGN_VHT3SS_MCS7:
	case MGN_VHT4SS_MCS7:
		/* BW20 64-QAM 5/6 */
		rx_sensitivity = -64;
		break;

	case MGN_VHT1SS_MCS8:
	case MGN_VHT2SS_MCS8:
	case MGN_VHT3SS_MCS8:
	case MGN_VHT4SS_MCS8:
		/* BW20 256-QAM 3/4 */
		rx_sensitivity = -59;
		break;

	case MGN_VHT1SS_MCS9:
	case MGN_VHT2SS_MCS9:
	case MGN_VHT3SS_MCS9:
	case MGN_VHT4SS_MCS9:
		/* BW20 256-QAM 5/6 */
		rx_sensitivity = -57;
		break;

	default:
		return -1;
		break;

	}

	switch(bw) {
	case CHANNEL_WIDTH_20:
		break;
	case CHANNEL_WIDTH_40:
		rx_sensitivity -= 3;
		break;
	case CHANNEL_WIDTH_80:
		rx_sensitivity -= 6;
		break;
	case CHANNEL_WIDTH_160:
		rx_sensitivity -= 9;
		break;
	case CHANNEL_WIDTH_5:
	case CHANNEL_WIDTH_10:
	case CHANNEL_WIDTH_80_80:
	default:
		return -1;
		break;
	}
	*pwr = rx_sensitivity;

	return 0;
}

/* output: path_a max tx power in dBm */
int rm_get_path_a_max_tx_power(_adapter *adapter, s8 *path_a)
{
	struct hal_spec_t *hal_spec = GET_HAL_SPEC(adapter);
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(adapter);
	int path, tx_num, band, bw, ch, n, rs;
	u8 rate_num, base;
	s8 limt_offset = 127; /* max value of s8 */
	s8 rate_offset;
	s8 powr_offset;
	s8 max_pwr[RF_PATH_MAX], pwr;


	band = hal_data->current_band_type;
	bw = hal_data->current_channel_bw;
	ch = hal_data->current_channel;

	for (path = 0; path < RF_PATH_MAX; path++) {
		if (!HAL_SPEC_CHK_RF_PATH(hal_spec, band, path))
			break;

		max_pwr[path] = -127; /* min value of s8 */
#if (RM_MORE_DBG_MSG)
		RTW_INFO("RM: [%s][%c]\n", band_str(band), rf_path_char(path));
#endif
		for (rs = 0; rs < RATE_SECTION_NUM; rs++) {
			tx_num = rate_section_to_tx_num(rs);

			if (tx_num >= hal_spec->tx_nss_num)
				continue;

			if (band == BAND_ON_5G && IS_CCK_RATE_SECTION(rs))
				continue;

			if (IS_VHT_RATE_SECTION(rs) && !IS_HARDWARE_TYPE_JAGUAR_ALL(adapter))
				continue;

			rate_num = rate_section_rate_num(rs);
			base = PHY_GetTxPowerByRateBase(adapter, band, path, rs);

			/* get power by rate in db */
			for (n = rate_num - 1; n >= 0; n--) {
				rate_offset = PHY_GetTxPowerByRate(adapter, band,
					path, rates_by_sections[rs].rates[n]);
#ifdef CONFIG_TXPWR_LIMIT
				limt_offset = PHY_GetTxPowerLimit(adapter, NULL, band, bw,
					path, rates_by_sections[rs].rates[n], RF_1TX, ch);
#endif
				powr_offset = MIN(rate_offset, limt_offset);
				pwr = (base + powr_offset) / hal_spec->txgi_pdbm;
				max_pwr[path] = MAX(max_pwr[path], pwr);
#if (RM_MORE_DBG_MSG)
				RTW_INFO("RM: %2d base=%7s(%2d), min(%2d|%2d)=%2d; (%2d,%2d)\n",
					n, rate_section_str(rs), base,
					rate_offset, limt_offset, powr_offset,
					((base + rate_offset) / hal_spec->txgi_pdbm), pwr);
#endif
			}
		}
	}
#if (RM_MORE_DBG_MSG)
	RTW_INFO("RM: path_a max_pwr=%ddBm\n", max_pwr[0]);
#endif
	*path_a = max_pwr[0];
	return 0;
}

#endif /* CONFIG_RTW_80211K */
