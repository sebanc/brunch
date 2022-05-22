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

#include "mp_precomp.h"

#if RT_PLATFORM==PLATFORM_MACOSX
#include "phydm_precomp.h"
#else
#include "../phydm_precomp.h"
#endif
#if (RTL8814A_SUPPORT == 1)


/*---------------------------Define Local Constant---------------------------*/
/* 2010/04/25 MH Define the max tx power tracking tx agc power. */
#define		ODM_TXPWRTRACK_MAX_IDX_8814A		6

/*---------------------------Define Local Constant---------------------------*/

/*3============================================================*/
/* 3 Tx Power Tracking
 *3============================================================*/
#define	REG_A_TX_AGC	0xC94
#define	REG_B_TX_AGC	0xE94
#define	REG_C_TX_AGC	0x1894
#define	REG_D_TX_AGC	0x1A94
#define	TXAGC_BITMASK	(BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25))
#define	REG_A_BBSWING	0xC1C
#define	REG_B_BBSWING	0xE1C
#define	REG_C_BBSWING	0x181C
#define	REG_D_BBSWING	0x1A1C
#define	BBSWING_BITMASK	0xFFE00000


#if 0

u8 check_rf_gain_offset(
	struct dm_struct			*dm,
	enum pwrtrack_method			method,
	u8				rf_path
)
{
	s8	upper_bound = 10, lower_bound = -5;	/*4'b1010 = 10*/
	s8	final_rf_index = 0;
	boolean	is_positive = false;
	u32	bit_mask = 0;
	u8	final_ofdm_swing_index = 0, tx_scaling_upper_bound = 28, tx_scaling_lower_bound = 4;	/*upper bound +2dB, lower bound -10dB*/
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);

	if (method == MIX_MODE) {	/*normal Tx power tracking*/
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "is 8814 MP chip\n");
		bit_mask = BIT(19);
		cali_info->absolute_ofdm_swing_idx[rf_path] = cali_info->absolute_ofdm_swing_idx[rf_path] + cali_info->kfree_offset[rf_path];

		if (cali_info->absolute_ofdm_swing_idx[rf_path] >= 0)				/*check if RF_Index is positive or not*/
			is_positive = true;
		else
			is_positive = false;

		odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, is_positive);

		bit_mask = BIT(18) | BIT(17) | BIT(16) | BIT(15);
		final_rf_index = cali_info->absolute_ofdm_swing_idx[rf_path] / 2;		/*TxBB 1 step equal 1dB, BB swing 1step equal 0.5dB*/

	}

	if (final_rf_index > upper_bound) {		/*Upper bound = 10dB, if more htan upper bound, then move to bb swing max = +2dB*/
		odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, upper_bound);	/*set RF Reg0x55 per path*/

		final_ofdm_swing_index = cali_info->default_ofdm_index + (cali_info->absolute_ofdm_swing_idx[rf_path] - (upper_bound << 1));

		if (final_ofdm_swing_index > tx_scaling_upper_bound)	/*bb swing upper bound = +2dB*/
			final_ofdm_swing_index = tx_scaling_upper_bound;

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			"******path-%d Compensate with TXBB = %d\n", rf_path, upper_bound);

		return final_ofdm_swing_index;
	} else if (final_rf_index < lower_bound) {	/*lower bound = -5dB*/
		odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, (-1) * (lower_bound));	/*set RF Reg0x55 per path*/

		final_ofdm_swing_index = cali_info->default_ofdm_index - ((lower_bound << 1) - cali_info->absolute_ofdm_swing_idx[rf_path]);

		if (final_ofdm_swing_index < tx_scaling_lower_bound)	/*bb swing lower bound = -10dB*/
			final_ofdm_swing_index = tx_scaling_lower_bound;

		return final_ofdm_swing_index;

	} else {	/*normal case*/

		if (is_positive == true)
			odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, final_rf_index);	/*set RF Reg0x55 per path*/
		else
			odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, bit_mask, (-1) * final_rf_index);	/*set RF Reg0x55 per path*/

		final_ofdm_swing_index = cali_info->default_ofdm_index + (cali_info->absolute_ofdm_swing_idx[rf_path]) % 2;
		return final_ofdm_swing_index;
	}

	return false;
}

#endif

u8 get_tssivalue(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path
)
{

	struct _ADAPTER *adapter = dm->adapter;
	struct dm_rf_calibration_struct		*cali_info = &(dm->rf_calibrate_info);
	HAL_DATA_TYPE		*hal_data = GET_HAL_DATA(((PADAPTER)adapter));

	s8				power_by_rate_value = 0;
	u8				tx_num, tssi_value = 0;
	u8				channel  = *dm->channel;
	u8				band_width  = hal_data->CurrentChannelBW;
	u8				tx_rate = 0xFF;
	u8				tx_limit = 0;
	u8				reg_pwr_tbl_sel = 0;

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	PMGNT_INFO			mgnt_info = &(((PADAPTER)adapter)->MgntInfo);

	reg_pwr_tbl_sel = mgnt_info->RegPwrTblSel;
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
	reg_pwr_tbl_sel = adapter->registrypriv.reg_pwr_tbl_sel;
#endif
#endif

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16	rate	 = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = ((PADAPTER)adapter)->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			tx_rate = hw_rate_to_m_rate(dm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8) rate;
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_rate=0x%X\n", __func__, tx_rate);
	tx_num = MgntQuery_NssTxRate(tx_rate);


	if (channel >= 1 && channel <= 14) {
		power_by_rate_value = PHY_GetTxPowerByRateOriginal(adapter, BAND_ON_2_4G, (enum rf_path) rf_path, tx_num, tx_rate);
		tx_limit = PHY_GetTxPowerLimitOriginal(adapter, reg_pwr_tbl_sel, BAND_ON_2_4G, (enum channel_width) band_width, RF_PATH_A, tx_rate, channel);

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s power_by_rate_value=%d   tx_rate=0x%X rf_path=%d   tx_num=%d\n", __func__, power_by_rate_value, tx_rate, rf_path, tx_num);
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_limit=%d   reg_pwr_tbl_sel=0x%X band_width=%d   channel=%d\n", __func__, tx_limit, reg_pwr_tbl_sel, band_width, channel);

		power_by_rate_value = power_by_rate_value > tx_limit ? tx_limit : power_by_rate_value;

		if (IS_CCK_RATE(tx_rate)) {
			switch (rf_path) {
			case RF_PATH_A:
				tssi_value = cali_info->delta_swing_tssi_table_2g_cck_a[power_by_rate_value];
				break;

			case RF_PATH_B:
				tssi_value = cali_info->delta_swing_tssi_table_2g_cck_b[power_by_rate_value];
				break;

			case RF_PATH_C:
				tssi_value = cali_info->delta_swing_tssi_table_2g_cck_c[power_by_rate_value];
				break;

			case RF_PATH_D:
				tssi_value = cali_info->delta_swing_tssi_table_2g_cck_d[power_by_rate_value];
				break;

			default:
				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call:%s Wrong path name!!!\n", __func__);
				break;
			}
		} else {
			switch (rf_path) {
			case RF_PATH_A:
				tssi_value = cali_info->delta_swing_tssi_table_2ga[power_by_rate_value];
				break;

			case RF_PATH_B:
				tssi_value = cali_info->delta_swing_tssi_table_2gb[power_by_rate_value];
				break;

			case RF_PATH_C:
				tssi_value = cali_info->delta_swing_tssi_table_2gc[power_by_rate_value];
				break;

			case RF_PATH_D:
				tssi_value = cali_info->delta_swing_tssi_table_2gd[power_by_rate_value];
				break;

			default:
				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call:%s Wrong path name!!!!\n", __func__);
				break;
			}
		}
	} else if (channel >= 36 && channel <= 64) {
		power_by_rate_value = PHY_GetTxPowerByRateOriginal(adapter, BAND_ON_5G, (enum rf_path) rf_path, tx_num, tx_rate);

		tx_limit = PHY_GetTxPowerLimitOriginal(adapter, reg_pwr_tbl_sel, BAND_ON_5G, (enum channel_width) band_width, RF_PATH_A, tx_rate, channel);

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s power_by_rate_value=%d   tx_rate=0x%X rf_path=%d   tx_num=%d\n", __func__, power_by_rate_value, tx_rate, rf_path, tx_num);
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_limit=%d   reg_pwr_tbl_sel=0x%X band_width=%d   channel=%d\n", __func__, tx_limit, reg_pwr_tbl_sel, band_width, channel);

		power_by_rate_value = power_by_rate_value > tx_limit ? tx_limit : power_by_rate_value;

		switch (rf_path) {
		case RF_PATH_A:
			tssi_value = cali_info->delta_swing_tssi_table_5ga[0][power_by_rate_value];
			break;

		case RF_PATH_B:
			tssi_value = cali_info->delta_swing_tssi_table_5gb[0][power_by_rate_value];
			break;

		case RF_PATH_C:
			tssi_value = cali_info->delta_swing_tssi_table_5gc[0][power_by_rate_value];
			break;

		case RF_PATH_D:
			tssi_value = cali_info->delta_swing_tssi_table_5gd[0][power_by_rate_value];
			break;

		default:
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call:%s Wrong path name!!!!\n", __func__);
			break;
		}
	} else if (channel >= 100 && channel <= 144) {
		power_by_rate_value = PHY_GetTxPowerByRateOriginal(adapter, BAND_ON_5G, (enum rf_path) rf_path, tx_num, tx_rate);

		tx_limit = PHY_GetTxPowerLimitOriginal(adapter, reg_pwr_tbl_sel, BAND_ON_5G, (enum channel_width) band_width, RF_PATH_A, tx_rate, channel);

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s power_by_rate_value=%d   tx_rate=0x%X rf_path=%d   tx_num=%d\n", __func__, power_by_rate_value, tx_rate, rf_path, tx_num);
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_limit=%d   reg_pwr_tbl_sel=0x%X band_width=%d   channel=%d\n", __func__, tx_limit, reg_pwr_tbl_sel, band_width, channel);

		power_by_rate_value = power_by_rate_value > tx_limit ? tx_limit : power_by_rate_value;

		switch (rf_path) {
		case RF_PATH_A:
			tssi_value = cali_info->delta_swing_tssi_table_5ga[1][power_by_rate_value];
			break;

		case RF_PATH_B:
			tssi_value = cali_info->delta_swing_tssi_table_5gb[1][power_by_rate_value];
			break;

		case RF_PATH_C:
			tssi_value = cali_info->delta_swing_tssi_table_5gc[1][power_by_rate_value];
			break;

		case RF_PATH_D:
			tssi_value = cali_info->delta_swing_tssi_table_5gd[1][power_by_rate_value];
			break;

		default:
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call:%s Wrong path name!!!!\n", __func__);
			break;
		}
	} else if (channel >= 149 && channel <= 173) {
		power_by_rate_value = PHY_GetTxPowerByRateOriginal(adapter, BAND_ON_5G, (enum rf_path) rf_path, tx_num, tx_rate);

		tx_limit = PHY_GetTxPowerLimitOriginal(adapter, reg_pwr_tbl_sel, BAND_ON_5G, (enum channel_width) band_width, RF_PATH_A, tx_rate, channel);

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s power_by_rate_value=%d   tx_rate=0x%X rf_path=%d   tx_num=%d\n", __func__, power_by_rate_value, tx_rate, rf_path, tx_num);
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_limit=%d   reg_pwr_tbl_sel=0x%X band_width=%d   channel=%d\n", __func__, tx_limit, reg_pwr_tbl_sel, band_width, channel);

		power_by_rate_value = power_by_rate_value > tx_limit ? tx_limit : power_by_rate_value;

		switch (rf_path) {
		case RF_PATH_A:
			tssi_value = cali_info->delta_swing_tssi_table_5ga[2][power_by_rate_value];
			break;

		case RF_PATH_B:
			tssi_value = cali_info->delta_swing_tssi_table_5gb[2][power_by_rate_value];
			break;

		case RF_PATH_C:
			tssi_value = cali_info->delta_swing_tssi_table_5gc[2][power_by_rate_value];
			break;

		case RF_PATH_D:
			tssi_value = cali_info->delta_swing_tssi_table_5gd[2][power_by_rate_value];
			break;

		default:
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call:%s Wrong path name!!!!\n", __func__);
			break;
		}
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s index=%d   tssi_value=%d\n", __func__, power_by_rate_value, tssi_value);

	return tssi_value;
}

boolean get_tssi_mode_tx_agc_bb_swing_offset(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8					rf_path,
	u32					offset_vaule,
	u8					tx_power_index_offest
)
{
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);

	u8		bb_swing_upper_bound = cali_info->default_ofdm_index + 10;
	u8		bb_swing_lower_bound = 0;
	u8		tx_agc_index = (u8) cali_info->absolute_ofdm_swing_idx[rf_path];
	u8		tx_bb_swing_index = (u8) cali_info->bb_swing_idx_ofdm[rf_path];

	if (tx_power_index_offest > 0XF)
		tx_power_index_offest = 0XF;

	if (tx_agc_index == 0 && tx_bb_swing_index == cali_info->default_ofdm_index) {
		if ((offset_vaule & 0X20) >> 5 == 0) {
			offset_vaule = offset_vaule & 0X1F;

			if (offset_vaule > tx_power_index_offest) {
				tx_agc_index = tx_power_index_offest;
				tx_bb_swing_index = tx_bb_swing_index + (u8) offset_vaule - tx_power_index_offest;

				if (tx_bb_swing_index > bb_swing_upper_bound)
					tx_bb_swing_index = bb_swing_upper_bound;

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"tx_agc_index(0) tx_bb_swing_index(18) +++ ( offset_vaule > 0XF) offset_vaule = 0X%X	TXAGCIndex = 0X%X	tx_bb_swing_index = %d\n", offset_vaule, tx_agc_index, tx_bb_swing_index);
			} else {
				tx_agc_index = (u8) offset_vaule;
				tx_bb_swing_index = cali_info->default_ofdm_index;

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"tx_agc_index(0) tx_bb_swing_index(18) +++ ( offset_vaule <= 0XF) offset_vaule = 0X%X	TXAGCIndex = 0X%X	tx_bb_swing_index = %d\n", offset_vaule, tx_agc_index, tx_bb_swing_index);
			}
		} else {
			tx_agc_index = 0;
			offset_vaule = ((~offset_vaule) + 1) & 0X1F;

			if (tx_bb_swing_index >= (u8) offset_vaule)
				tx_bb_swing_index = tx_bb_swing_index - (u8) offset_vaule;
			else
				tx_bb_swing_index = bb_swing_lower_bound;

			if (tx_bb_swing_index <= bb_swing_lower_bound)
				tx_bb_swing_index = bb_swing_lower_bound;

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"tx_agc_index(0) tx_bb_swing_index(18) --- offset_vaule = 0X%X   TXAGCIndex = 0X%X   tx_bb_swing_index = %d\n", offset_vaule, tx_agc_index, tx_bb_swing_index);
		}

	} else if (tx_agc_index > 0 && tx_bb_swing_index == cali_info->default_ofdm_index) {
		if ((offset_vaule & 0X20) >> 5 == 0) {
			if (offset_vaule > tx_power_index_offest) {
				tx_agc_index = tx_power_index_offest;
				tx_bb_swing_index = tx_bb_swing_index + (u8) offset_vaule - tx_power_index_offest;

				if (tx_bb_swing_index > bb_swing_upper_bound)
					tx_bb_swing_index = bb_swing_upper_bound;
			} else
				tx_agc_index = (u8) offset_vaule;

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"tx_agc_index > 0 tx_bb_swing_index(18) +++ offset_vaule = 0X%X   TXAGCIndex = 0X%X   tx_bb_swing_index = %d\n", offset_vaule, tx_agc_index, tx_bb_swing_index);
		} else {
			tx_agc_index = 0;
			offset_vaule = ((~offset_vaule) + 1) & 0X1F;

			if (tx_bb_swing_index >= (u8) offset_vaule)
				tx_bb_swing_index = tx_bb_swing_index - (u8) offset_vaule;
			else
				tx_bb_swing_index = bb_swing_lower_bound;

			if (tx_bb_swing_index <= bb_swing_lower_bound)
				tx_bb_swing_index = bb_swing_lower_bound;

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"tx_agc_index > 0 tx_bb_swing_index(18)  --- offset_vaule = 0X%X	TXAGCIndex = 0X%X	tx_bb_swing_index = %d\n", offset_vaule, tx_agc_index, tx_bb_swing_index);
		}


	} else if (tx_agc_index > 0 && tx_bb_swing_index > cali_info->default_ofdm_index) {
		if ((offset_vaule & 0X20) >> 5 == 0) {
			tx_agc_index = tx_power_index_offest;
			tx_bb_swing_index = tx_bb_swing_index + (u8) offset_vaule - tx_power_index_offest;

			if (tx_bb_swing_index > bb_swing_upper_bound)
				tx_bb_swing_index = bb_swing_upper_bound;

			if (tx_bb_swing_index < cali_info->default_ofdm_index) {
				tx_agc_index = tx_power_index_offest - (cali_info->default_ofdm_index - tx_bb_swing_index);
				tx_bb_swing_index = cali_info->default_ofdm_index;
			}
		} else {
			tx_agc_index = 0;
			offset_vaule = ((~offset_vaule) + 1) & 0X1F;

			if (tx_bb_swing_index >= (u8) offset_vaule)
				tx_bb_swing_index = cali_info->default_ofdm_index - (u8) offset_vaule;
			else
				tx_bb_swing_index = bb_swing_lower_bound;

			if (tx_bb_swing_index <= bb_swing_lower_bound)
				tx_bb_swing_index = bb_swing_lower_bound;
		}

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"tx_agc_index>0 tx_bb_swing_index>18 --- offset_vaule = 0X%X	 TXAGCIndex = 0X%X	 tx_bb_swing_index = %d	 offset_vaule=%d\n", offset_vaule, tx_agc_index, tx_bb_swing_index, offset_vaule - tx_power_index_offest);

	} else if (tx_agc_index == 0 && tx_bb_swing_index < cali_info->default_ofdm_index) {
		if ((offset_vaule & 0X20) >> 5 == 1) {
			offset_vaule = ((~offset_vaule) + 1) & 0X1F;

			if (tx_bb_swing_index >= (u8) offset_vaule)
				tx_bb_swing_index = tx_bb_swing_index - (u8) offset_vaule;
			else
				tx_bb_swing_index = bb_swing_lower_bound;
		} else {
			offset_vaule = (offset_vaule & 0x1F);
			tx_bb_swing_index = tx_bb_swing_index + (u8) offset_vaule;

			if (tx_bb_swing_index > cali_info->default_ofdm_index) {
				tx_agc_index = tx_bb_swing_index - cali_info->default_ofdm_index;
				tx_bb_swing_index = cali_info->default_ofdm_index;

				if (tx_agc_index > tx_power_index_offest) {
					tx_bb_swing_index = cali_info->default_ofdm_index + (u8)(tx_agc_index) - tx_power_index_offest;
					tx_agc_index = tx_power_index_offest;

					if (tx_bb_swing_index  > bb_swing_upper_bound)
						tx_bb_swing_index = bb_swing_upper_bound;
				}
			}
		}

		if (tx_bb_swing_index <= bb_swing_lower_bound) {
			tx_bb_swing_index = bb_swing_lower_bound;
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s  Path_%d BBSwing Lower Bound\n", __func__, rf_path);
		}

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"tx_agc_index(0) tx_bb_swing_index < 18 offset_vaule = 0X%X	 TXAGCIndex = 0X%X	 tx_bb_swing_index = %d\n", offset_vaule, tx_agc_index, tx_bb_swing_index);

	}

	if ((tx_agc_index == cali_info->absolute_ofdm_swing_idx[rf_path]) && (tx_bb_swing_index == cali_info->bb_swing_idx_ofdm[rf_path]))
		return false;

	else {
		cali_info->absolute_ofdm_swing_idx[rf_path] = tx_agc_index;
		cali_info->bb_swing_idx_ofdm[rf_path] = tx_bb_swing_index;
		return true;
	}

}


void set_tx_agc_bb_swing_offset(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path
)
{

	struct _ADAPTER *adapter = dm->adapter;
	HAL_DATA_TYPE		*hal_data = GET_HAL_DATA(((PADAPTER)adapter));

	u8				tx_rate = 0xFF;
	u8				channel  = *dm->channel;
	u8				band_width  = hal_data->CurrentChannelBW;
	u32				tx_path  = hal_data->AntennaTxPath;

	u8				tssi_value = 0;
	u8				tx_power_index = 0;
	u8				tx_power_index_offest = 0;
	u32				offset_vaule = 0;
	u32				tssi_function = 0;
	u32				txbb_swing = 0;
	u8				tx_bb_swing_index = 0;
	u32				tx_agc_index = 0;
	u32				wait_tx_agc_offset_timer = 0;
	u8				i = 0;
	boolean			rtn = false;

	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16	rate	 = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = ((PADAPTER)adapter)->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			tx_rate = hw_rate_to_m_rate(dm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8) rate;
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_rate=0x%X\n", __func__, tx_rate);

	if (method == TSSI_MODE) {
		switch (rf_path) {
		case RF_PATH_A:
			/*Disable path B TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x65, BIT(10), 0);
			/*Enable path A TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x65, BIT(10), 1);

			/*Read power by rate table to set TSSI value by power and set rf reg 0x65[19:15]*/
			tssi_value = get_tssivalue(dm, TSSI_MODE, (enum rf_path) rf_path);

			odm_set_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15), tssi_value);

			/*Write BB 0xC8C for setting Max. packet (30) of tracking power and the initial value of TXAGC*/
			odm_set_bb_reg(dm, R_0xc8c, BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14), 30);

			odm_set_bb_reg(dm, R_0xc8c, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8), 0);

			/*Write BB TXAGC Initial Power index for EEPROM*/
			tx_power_index = PHY_GetTxPowerIndex_8814A(adapter, (enum rf_path) rf_path, tx_rate, (enum channel_width) band_width, channel);
			odm_set_bb_reg(dm, R_0xc8c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0), tx_power_index);

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s   tx_power_index=%d    rf_path=%d   tx_rate=%d   band_width=%d   channel=%d   0x65[19:15]=0X%X   0x65[11:10]=0X%X\n",
				      __func__,
				odm_get_bb_reg(dm, R_0xc8c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)),
				      rf_path, tx_rate, band_width, channel,
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15)),
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(11) | BIT(10))
				     );

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0xc8c, BIT(7), 0);
			/*Enable path A TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x65, BIT(10), 1);
			/*Enable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0xc8c, BIT(7), 1);

			/*			delay_us(500);*/
			wait_tx_agc_offset_timer = 0;

			while ((odm_get_bb_reg(dm, R_0xd2c, BIT(30)) != 1) && ((tx_path & 8) == 8)) {
				wait_tx_agc_offset_timer++;

				if (wait_tx_agc_offset_timer >= 1000)
					break;
			}

			/*Read the offset value at BB Reg.*/
			offset_vaule = odm_get_bb_reg(dm, R_0xd2c, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24));

			tssi_function = odm_get_bb_reg(dm, R_0xd2c, BIT(30));

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s  0XD2C	 tssi_function[30]=0X%X offset_vaule[29:24]=0X%X	rf_path=%d\n", __func__, tssi_function, offset_vaule, rf_path);

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0xc8c, BIT(7), 0);
			/*Disable path A TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x65, BIT(10), 0);

			if (tssi_function == 1) {
				txbb_swing = odm_get_bb_reg(dm, REG_A_BBSWING, BBSWING_BITMASK);

				for (i = 0; i <= 36; i++) {
					if (txbb_swing == tx_scaling_table_jaguar[i]) {
						cali_info->bb_swing_idx_ofdm[rf_path] = i;
						RF_DBG(dm, DBG_RF_TX_PWR_TRACK," PathA txbb_swing = %d txbb_swing=0X%X\n", tx_bb_swing_index, txbb_swing);
						break;

					} else
						cali_info->bb_swing_idx_ofdm[rf_path] = cali_info->default_ofdm_index;
				}

				cali_info->absolute_ofdm_swing_idx[rf_path] = (u8) odm_get_bb_reg(dm, REG_A_TX_AGC, TXAGC_BITMASK);


				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGCIndex = 0X%X  tx_bb_swing_index = %d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path]);

				tx_power_index_offest = 63 - tx_power_index;

				rtn = get_tssi_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, offset_vaule, tx_power_index_offest);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," tx_agc_index = %d   tx_bb_swing_index = %d rtn=%d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path], rtn);

				if (rtn == true) {
					odm_set_bb_reg(dm, REG_A_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
					odm_set_bb_reg(dm, REG_A_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
				} else
					RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGC And BB Swing are the same path=%d\n", rf_path);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");


				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"  OffsetValue(0XD2C)=0X%X   TXAGC(REG_A_TX_AGC)=0X%X   0XC1C(PathC BBSwing)(%d)=0X%X\n",
					odm_get_bb_reg(dm, R_0xd2c, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24)),
					odm_get_bb_reg(dm, REG_A_TX_AGC, TXAGC_BITMASK),
					cali_info->bb_swing_idx_ofdm[rf_path],
					odm_get_bb_reg(dm, REG_A_BBSWING, BBSWING_BITMASK)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," 0X55[13:9]=0X%X	 0X56=0X%X\n",
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X55, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9)),
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X56, 0XFFFFFFFF)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");

			} else
				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," TSSI does not Calculate Finish\n");

			break;

		case RF_PATH_B:
			/*Disable path A TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x65, BIT(10), 0);
			/*Enable path B TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x65, BIT(10), 1);

			/*Read power by rate table to set TSSI value by power and set rf reg 0x65[19:15]*/
			tssi_value = get_tssivalue(dm, TSSI_MODE, (enum rf_path) rf_path);

			odm_set_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15), tssi_value);

			/*Write BB 0xE8C for setting Max. packet (30) of tracking power and the initial value of TXAGC*/
			odm_set_bb_reg(dm, R_0xe8c, BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14), 30);

			odm_set_bb_reg(dm, R_0xe8c, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8), 0);

			/*Write BB TXAGC Initial Power index for EEPROM*/
			tx_power_index = PHY_GetTxPowerIndex_8814A(adapter, (enum rf_path) rf_path, tx_rate, (enum channel_width) band_width, channel);
			odm_set_bb_reg(dm, R_0xe8c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0), tx_power_index);

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s   tx_power_index=%d    rf_path=%d   tx_rate=%d   band_width=%d   channel=%d   0x65[19:15]=0X%X   0x65[11:10]=0X%X\n",
				      __func__,
				odm_get_bb_reg(dm, R_0xe8c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)),
				      rf_path, tx_rate, band_width, channel,
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15)),
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(11) | BIT(10))
				     );

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0xe8c, BIT(7), 0);
			/*Enable path B TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x65, BIT(10), 1);
			/*Enable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0xe8c, BIT(7), 1);

			/*				delay_us(500);*/
			wait_tx_agc_offset_timer = 0;

			while ((odm_get_bb_reg(dm, R_0xd6c, BIT(30)) != 1) && ((tx_path & 4) == 4)) {
				wait_tx_agc_offset_timer++;

				if (wait_tx_agc_offset_timer >= 1000)
					break;
			}

			/*Read the offset value at BB Reg.*/
			offset_vaule = odm_get_bb_reg(dm, R_0xd6c, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24));

			tssi_function = odm_get_bb_reg(dm, R_0xd6c, BIT(30));

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s  0XD6C	 tssi_function[30]=0X%X offset_vaule[29:24]=0X%X	rf_path=%d\n", __func__, tssi_function, offset_vaule, rf_path);

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0xe8c, BIT(7), 0);
			/*Disable path B TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x65, BIT(10), 0);

			if (tssi_function == 1) {
				txbb_swing = odm_get_bb_reg(dm, REG_B_BBSWING, BBSWING_BITMASK);

				for (i = 0; i <= 36; i++) {
					if (txbb_swing == tx_scaling_table_jaguar[i]) {
						cali_info->bb_swing_idx_ofdm[rf_path] = i;
						RF_DBG(dm, DBG_RF_TX_PWR_TRACK," PathB txbb_swing = %d txbb_swing=0X%X\n", tx_bb_swing_index, txbb_swing);
						break;

					} else
						cali_info->bb_swing_idx_ofdm[rf_path] = cali_info->default_ofdm_index;
				}

				cali_info->absolute_ofdm_swing_idx[rf_path] = (u8) odm_get_bb_reg(dm, REG_B_TX_AGC, TXAGC_BITMASK);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGCIndex = 0X%X  tx_bb_swing_index = %d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path]);

				tx_power_index_offest = 63 - tx_power_index;

				rtn = get_tssi_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, offset_vaule, tx_power_index_offest);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," tx_agc_index = %d   tx_bb_swing_index = %d rtn=%d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path], rtn);

				if (rtn == true) {
					odm_set_bb_reg(dm, REG_B_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
					odm_set_bb_reg(dm, REG_B_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
				} else
					RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGC And BB Swing are the same path=%d\n", rf_path);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");


				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," OffsetValue(0XD6C)=0X%X   TXAGC(REG_B_TX_AGC)=0X%X   0XE1C(PathB BBSwing)(%d)=0X%X\n",
					odm_get_bb_reg(dm, R_0xd6c, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24)),
					odm_get_bb_reg(dm, REG_B_TX_AGC, TXAGC_BITMASK),
					cali_info->bb_swing_idx_ofdm[rf_path],
					odm_get_bb_reg(dm, REG_B_BBSWING, BBSWING_BITMASK)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," 0X55[13:9]=0X%X	 0X56=0X%X\n",
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X55, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9)),
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X56, 0XFFFFFFFF)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");

			} else
				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," TSSI does not Calculate Finish\n");

			break;

		case RF_PATH_C:
			/*Disable path D TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_D, RF_0x65, BIT(10), 0);
			/*Enable path C TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_C, RF_0x65, BIT(10), 1);

			/*Read power by rate table to set TSSI value by power and set rf reg 0x65[19:15]*/
			tssi_value = get_tssivalue(dm, TSSI_MODE, (enum rf_path) rf_path);

			odm_set_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15), tssi_value);

			/*Write BB 0x188C for setting Max. packet (30) of tracking power and the initial value of TXAGC*/
			odm_set_bb_reg(dm, R_0x188c, BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14), 30);

			odm_set_bb_reg(dm, R_0x188c, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8), 0);

			/*Write BB TXAGC Initial Power index for EEPROM*/
			tx_power_index = PHY_GetTxPowerIndex_8814A(adapter, (enum rf_path) rf_path, tx_rate, (enum channel_width) band_width, channel);
			odm_set_bb_reg(dm, R_0x188c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0), tx_power_index);

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s   tx_power_index=%d    rf_path=%d   tx_rate=%d   band_width=%d   channel=%d   0x65[19:15]=0X%X   0x65[11:10]=0X%X\n",
				      __func__,
				odm_get_bb_reg(dm, R_0x188c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)),
				      rf_path, tx_rate, band_width, channel,
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15)),
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(11) | BIT(10))
				     );

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0x188c, BIT(7), 0);
			/*Enable path C TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_C, RF_0x65, BIT(10), 1);
			/*Enable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0x188c, BIT(7), 1);

			/*				delay_us(500);*/
			wait_tx_agc_offset_timer = 0;

			while ((odm_get_bb_reg(dm, R_0xdac, BIT(30)) != 1) && ((tx_path & 2) == 2)) {
				wait_tx_agc_offset_timer++;

				if (wait_tx_agc_offset_timer >= 1000)
					break;
			}

			/*Read the offset value at BB Reg.*/
			offset_vaule = odm_get_bb_reg(dm, R_0xdac, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24));

			tssi_function = odm_get_bb_reg(dm, R_0xdac, BIT(30));

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s  0XDAC	 tssi_function[30]=0X%X offset_vaule[29:24]=0X%X	rf_path=%d\n", __func__, tssi_function, offset_vaule, rf_path);

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0x188c, BIT(7), 0);
			/*Disable path C TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_C, RF_0x65, BIT(10), 0);

			if (tssi_function == 1) {
				txbb_swing = odm_get_bb_reg(dm, REG_C_BBSWING, BBSWING_BITMASK);

				for (i = 0; i <= 36; i++) {
					if (txbb_swing == tx_scaling_table_jaguar[i]) {
						cali_info->bb_swing_idx_ofdm[rf_path] = i;
						RF_DBG(dm, DBG_RF_TX_PWR_TRACK," PathC txbb_swing = %d txbb_swing=0X%X\n", tx_bb_swing_index, txbb_swing);
						break;

					} else
						cali_info->bb_swing_idx_ofdm[rf_path] = cali_info->default_ofdm_index;
				}

				cali_info->absolute_ofdm_swing_idx[rf_path] = (u8) odm_get_bb_reg(dm, REG_C_TX_AGC, TXAGC_BITMASK);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGCIndex = 0X%X  tx_bb_swing_index = %d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path]);

				tx_power_index_offest = 63 - tx_power_index;

				rtn = get_tssi_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, offset_vaule, tx_power_index_offest);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," tx_agc_index = %d   tx_bb_swing_index = %d rtn=%d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path], rtn);

				if (rtn == true) {
					odm_set_bb_reg(dm, REG_C_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
					odm_set_bb_reg(dm, REG_C_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
				} else
					RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGC And BB Swing are the same path=%d\n", rf_path);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");


				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," OffsetValue(0XDAC)=0X%X   TXAGC(REG_C_TX_AGC)=0X%X   0X181C(PathC BBSwing)(%d)=0X%X\n",
					odm_get_bb_reg(dm, R_0xdac, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24)),
					odm_get_bb_reg(dm, REG_C_TX_AGC, TXAGC_BITMASK),
					cali_info->bb_swing_idx_ofdm[rf_path],
					odm_get_bb_reg(dm, REG_C_BBSWING, BBSWING_BITMASK)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," 0X55[13:9]=0X%X	 0X56=0X%X\n",
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X55, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9)),
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X56, 0XFFFFFFFF)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");

			} else
				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," TSSI does not Calculate Finish\n");

			break;


		case RF_PATH_D:
			/*Disable path C TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_C, RF_0x65, BIT(10), 0);
			/*Enable path D TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_D, RF_0x65, BIT(10), 1);

			/*Read power by rate table to set TSSI value by power and set rf reg 0x65[19:15]*/
			tssi_value = get_tssivalue(dm, TSSI_MODE, (enum rf_path) rf_path);

			odm_set_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15), tssi_value);

			/*Write BB 0x1A8C for setting Max. packet (30) of tracking power and the initial value of TXAGC*/
			odm_set_bb_reg(dm, R_0x1a8c, BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14), 30);

			odm_set_bb_reg(dm, R_0x1a8c, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8), 0);

			/*Write BB TXAGC Initial Power index for EEPROM*/
			tx_power_index = PHY_GetTxPowerIndex_8814A(adapter, (enum rf_path) rf_path, tx_rate, (enum channel_width) band_width, channel);
			odm_set_bb_reg(dm, R_0x1a8c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0), tx_power_index);

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s   tx_power_index=%d	  rf_path=%d   tx_rate=%d   band_width=%d	 channel=%d   0x65[19:15]=0X%X	 0x65[11:10]=0X%X\n",
				      __func__,
				odm_get_bb_reg(dm, R_0x1a8c, BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)),
				      rf_path, tx_rate, band_width, channel,
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15)),
				odm_get_rf_reg(dm, (enum rf_path) rf_path, RF_0x65, BIT(11) | BIT(10))
				     );

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0x1a8c, BIT(7), 0);
			/*Enable path D TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_D, RF_0x65, BIT(10), 1);
			/*Enable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0x1a8c, BIT(7), 1);

			/*				delay_us(500);*/
			wait_tx_agc_offset_timer = 0;

			while ((odm_get_bb_reg(dm, R_0xdec, BIT(30)) != 1) && ((tx_path & 1) == 1)) {
				wait_tx_agc_offset_timer++;

				if (wait_tx_agc_offset_timer >= 1000)
					break;
			}

			/*Read the offset value at BB Reg.*/
			offset_vaule = odm_get_bb_reg(dm, R_0xdec, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24));

			tssi_function = odm_get_bb_reg(dm, R_0xdec, BIT(30));

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Call %s  0XDEC  tssi_function[30]=0X%X offset_vaule[29:24]=0X%X	rf_path=%d\n", __func__, tssi_function, offset_vaule, rf_path);

			/*Disable BB TSSI Power Tracking*/
			odm_set_bb_reg(dm, R_0x1a8c, BIT(7), 0);
			/*Disable path D TSSI Circuit*/
			odm_set_rf_reg(dm, RF_PATH_D, RF_0x65, BIT(10), 0);

			if (tssi_function == 1) {
				txbb_swing = odm_get_bb_reg(dm, REG_D_BBSWING, BBSWING_BITMASK);

				for (i = 0; i <= 36; i++) {
					if (txbb_swing == tx_scaling_table_jaguar[i]) {
						cali_info->bb_swing_idx_ofdm[rf_path] = i;
						RF_DBG(dm, DBG_RF_TX_PWR_TRACK," PathD txbb_swing = %d txbb_swing=0X%X\n", tx_bb_swing_index, txbb_swing);
						break;

					} else
						cali_info->bb_swing_idx_ofdm[rf_path] = cali_info->default_ofdm_index;
				}

				cali_info->absolute_ofdm_swing_idx[rf_path] = (u8) odm_get_bb_reg(dm, REG_D_TX_AGC, TXAGC_BITMASK);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGCIndex = 0X%X	tx_bb_swing_index = %d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path]);

				tx_power_index_offest = 63 - tx_power_index;

				rtn = get_tssi_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, offset_vaule, tx_power_index_offest);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," tx_agc_index = %d   tx_bb_swing_index = %d rtn=%d\n", cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path], rtn);

				if (rtn == true) {
					odm_set_bb_reg(dm, REG_D_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
					odm_set_bb_reg(dm, REG_D_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
				} else
					RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"TXAGC And BB Swing are the same path=%d\n", rf_path);

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");


				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," OffsetValue(0XDEC)=0X%X	TXAGC(REG_D_TX_AGC)=0X%X	 0X1A1C(PathD BBSwing)(%d)=0X%X\n",
					odm_get_bb_reg(dm, R_0xdec, BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25) | BIT(24)),
					odm_get_bb_reg(dm, REG_D_TX_AGC, TXAGC_BITMASK),
					cali_info->bb_swing_idx_ofdm[rf_path],
					odm_get_bb_reg(dm, REG_D_BBSWING, BBSWING_BITMASK)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," 0X55[13:9]=0X%X	 0X56=0X%X\n",
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X55, BIT(13) | BIT(12) | BIT(11) | BIT(10) | BIT(9)),
					odm_get_rf_reg(dm, (enum rf_path) rf_path, 0X56, 0XFFFFFFFF)
					     );

				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," ========================================================\n");

			} else
				RF_DBG(dm, DBG_RF_TX_PWR_TRACK," TSSI does not Calculate Finish\n");

			break;


		default:
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Wrong path name!!!!\n");

			break;
		}
	}

}

boolean get_mix_mode_tx_agc_bb_swing_offset(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8					rf_path,
	u8					tx_power_index_offest
)
{
	struct dm_rf_calibration_struct		*cali_info = &(dm->rf_calibrate_info);

	u8	bb_swing_upper_bound = cali_info->default_ofdm_index + 10;
	u8	bb_swing_lower_bound = 0;

	s8	tx_agc_index = 0;
	u8	tx_bb_swing_index = cali_info->default_ofdm_index;

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Path_%d cali_info->absolute_ofdm_swing_idx[rf_path]=%d, tx_power_index_offest=%d\n",
		rf_path, cali_info->absolute_ofdm_swing_idx[rf_path], tx_power_index_offest);

	if (tx_power_index_offest > 0XF)
		tx_power_index_offest = 0XF;

	if (cali_info->absolute_ofdm_swing_idx[rf_path] >= 0 && cali_info->absolute_ofdm_swing_idx[rf_path] <= tx_power_index_offest) {
		tx_agc_index = cali_info->absolute_ofdm_swing_idx[rf_path];
		tx_bb_swing_index = cali_info->default_ofdm_index;
	} else if (cali_info->absolute_ofdm_swing_idx[rf_path] > tx_power_index_offest) {
		tx_agc_index = tx_power_index_offest;
		cali_info->remnant_ofdm_swing_idx[rf_path] = cali_info->absolute_ofdm_swing_idx[rf_path] - tx_power_index_offest;
		tx_bb_swing_index = cali_info->default_ofdm_index + cali_info->remnant_ofdm_swing_idx[rf_path];

		if (tx_bb_swing_index > bb_swing_upper_bound)
			tx_bb_swing_index = bb_swing_upper_bound;
	} else {
		tx_agc_index = 0;

		if (cali_info->default_ofdm_index > (cali_info->absolute_ofdm_swing_idx[rf_path] * (-1)))
			tx_bb_swing_index = cali_info->default_ofdm_index + cali_info->absolute_ofdm_swing_idx[rf_path];
		else
			tx_bb_swing_index = bb_swing_lower_bound;

		if (tx_bb_swing_index <  bb_swing_lower_bound)
			tx_bb_swing_index = bb_swing_lower_bound;
	}

	cali_info->absolute_ofdm_swing_idx[rf_path] = tx_agc_index;
	cali_info->bb_swing_idx_ofdm[rf_path] = tx_bb_swing_index;

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"MixMode Offset Path_%d   cali_info->absolute_ofdm_swing_idx[rf_path]=%d   cali_info->bb_swing_idx_ofdm[rf_path]=%d   tx_power_index_offest=%d\n",
		rf_path, cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path], tx_power_index_offest);

	return true;
}

void power_tracking_by_mix_mode(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path
)
{
	struct _ADAPTER *adapter = dm->adapter;
	HAL_DATA_TYPE		*hal_data = GET_HAL_DATA(((PADAPTER)adapter));
	struct dm_rf_calibration_struct		*cali_info = &(dm->rf_calibrate_info);

	u8				tx_rate = 0xFF;
	u8				channel  = *dm->channel;
	u8				band_width  = hal_data->CurrentChannelBW;
	u8				tx_power_index_offest = 0;
	u8				tx_power_index = 0;

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16	rate	 = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = ((PADAPTER)adapter)->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			tx_rate = hw_rate_to_m_rate(dm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8) rate;
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_rate=0x%X\n", __func__, tx_rate);

	if ((cali_info->power_index_offset[RF_PATH_A] != 0 ||
	     cali_info->power_index_offset[RF_PATH_B] != 0 ||
	     cali_info->power_index_offset[RF_PATH_C] != 0 ||
	     cali_info->power_index_offset[RF_PATH_D] != 0) &&
	    cali_info->txpowertrack_control && (hal_data->eeprom_thermal_meter != 0xff)) {

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"****************Path_%d POWER Tracking MIX MODE**********\n", rf_path);

		tx_power_index = PHY_GetTxPowerIndex_8814A(adapter, (enum rf_path) rf_path, tx_rate, (enum channel_width)band_width, channel);
		tx_power_index_offest = 63 - tx_power_index;

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"cali_info->absolute_ofdm_swing_idx[%d] =%d   tx_power_index=%d\n", rf_path, cali_info->absolute_ofdm_swing_idx[rf_path], tx_power_index);

		switch (rf_path) {
		case RF_PATH_A:
			get_mix_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, tx_power_index_offest);
			odm_set_bb_reg(dm, REG_A_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_A_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		case RF_PATH_B:
			get_mix_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, tx_power_index_offest);
			odm_set_bb_reg(dm, REG_B_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_B_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		case RF_PATH_C:
			get_mix_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, tx_power_index_offest);
			odm_set_bb_reg(dm, REG_C_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_C_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		case RF_PATH_D:
			get_mix_mode_tx_agc_bb_swing_offset(dm, method, (enum rf_path) rf_path, tx_power_index_offest);
			odm_set_bb_reg(dm, REG_D_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_D_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		default:
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Wrong path name!!!!\n");
			break;
		}
	} else
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Power index is the same, eeprom_thermal_meter = 0XFF or txpowertrack_control is Disable !!!!\n");
}

void power_tracking_by_tssi_mode(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path
)
{
	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"****************Path_%d POWER Tracking TSSI_MODE**********\n", rf_path);

	set_tx_agc_bb_swing_offset(dm, TSSI_MODE, (enum rf_path) rf_path);

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"****************Path_%d End POWER Tracking TSSI_MODE**********\n", rf_path);

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "\n");
}



void odm_tx_pwr_track_set_pwr8814a(
	void		*dm_void,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	void		*adapter = dm->adapter;

	u8			channel  = *dm->channel;
	struct dm_rf_calibration_struct		*cali_info = &(dm->rf_calibrate_info);


	/*K-Free*/
#if 0
	s8			txbb_index = 0;
	s8			txbb_upper_bound = 10, txbb_lower_bound = -5;

	txbb_index = cali_info->kfree_offset[rf_path] / 2;

	if (txbb_index > txbb_upper_bound)
		txbb_index = txbb_upper_bound;
	else if (txbb_index < txbb_lower_bound)
		txbb_index = txbb_lower_bound;

	if (txbb_index >= 0) {
		odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, BIT(19), 1);
		odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, (BIT(18) | BIT17 | BIT16 | BIT15), txbb_index);	/*set RF Reg0x55 per path*/
	} else {
		odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, BIT(19), 0);
		odm_set_rf_reg(dm, (enum rf_path) rf_path, REG_RF_TX_GAIN_OFFSET, (BIT(18) | BIT(17) | BIT(16) | BIT(15)), (-1) * txbb_index);
	}

#endif

	if (method == TXAGC) {
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"****************Path_%d POWER Tracking No TXAGC MODE**********\n", rf_path);

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "cali_info->absolute_ofdm_swing_idx[%d] =%d\n",
			rf_path, cali_info->absolute_ofdm_swing_idx[rf_path]);
	} else if (method == TSSI_MODE)
		power_tracking_by_tssi_mode(dm, TSSI_MODE, (enum rf_path) rf_path);
	else if (method == BBSWING) {	/*use for mp driver clean power tracking status*/
		switch (rf_path) {
		case RF_PATH_A:
			odm_set_bb_reg(dm, REG_A_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_A_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		case RF_PATH_B:
			odm_set_bb_reg(dm, REG_B_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_B_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		case RF_PATH_C:
			odm_set_bb_reg(dm, REG_C_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_C_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		case RF_PATH_D:
			odm_set_bb_reg(dm, REG_D_TX_AGC, TXAGC_BITMASK, cali_info->absolute_ofdm_swing_idx[rf_path]);
			odm_set_bb_reg(dm, REG_D_BBSWING, BBSWING_BITMASK, tx_scaling_table_jaguar[cali_info->bb_swing_idx_ofdm[rf_path]]);	/*set BBswing*/
			break;

		default:
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,"Wrong path name!!!!\n");

			break;
		}

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "rf_path=%d   Clear 8814 Power tracking TXAGC=%d  BBSwing=%d\n",
			rf_path, cali_info->absolute_ofdm_swing_idx[rf_path], cali_info->bb_swing_idx_ofdm[rf_path]);

	} else if (method == MIX_MODE)
		power_tracking_by_mix_mode(dm, MIX_MODE, (enum rf_path) rf_path);
	else if (method == MIX_2G_TSSI_5G_MODE) {
		if (channel <= 14)
			power_tracking_by_mix_mode(dm, MIX_MODE, (enum rf_path) rf_path);
		else
			power_tracking_by_tssi_mode(dm, TSSI_MODE, (enum rf_path) rf_path);
	} else if (method == MIX_5G_TSSI_2G_MODE) {
		if (channel <= 14)
			power_tracking_by_tssi_mode(dm, TSSI_MODE, (enum rf_path) rf_path);
		else
			power_tracking_by_mix_mode(dm, MIX_MODE, (enum rf_path) rf_path);
	}
}	/*odm_tx_pwr_track_set_pwr8814a*/


void get_delta_swing_table_8814a(
	void		*dm_void,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	struct _ADAPTER *adapter = dm->adapter;
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);
	u8		tx_rate			= 0xFF;
	u8	channel		 = *dm->channel;

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16	rate	 = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = ((PADAPTER)adapter)->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			tx_rate = hw_rate_to_m_rate(dm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8) rate;
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_rate=0x%X\n", __func__, tx_rate);

	if (1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(tx_rate)) {
			*temperature_up_a   = cali_info->delta_swing_table_idx_2g_cck_a_p;
			*temperature_down_a = cali_info->delta_swing_table_idx_2g_cck_a_n;
			*temperature_up_b   = cali_info->delta_swing_table_idx_2g_cck_b_p;
			*temperature_down_b = cali_info->delta_swing_table_idx_2g_cck_b_n;
		} else {
			*temperature_up_a   = cali_info->delta_swing_table_idx_2ga_p;
			*temperature_down_a = cali_info->delta_swing_table_idx_2ga_n;
			*temperature_up_b   = cali_info->delta_swing_table_idx_2gb_p;
			*temperature_down_b = cali_info->delta_swing_table_idx_2gb_n;
		}
	} else if (36 <= channel && channel <= 64) {
		*temperature_up_a   = cali_info->delta_swing_table_idx_5ga_p[0];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[0];
		*temperature_up_b   = cali_info->delta_swing_table_idx_5gb_p[0];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[0];
	} else if (100 <= channel && channel <= 144) {
		*temperature_up_a   = cali_info->delta_swing_table_idx_5ga_p[1];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[1];
		*temperature_up_b   = cali_info->delta_swing_table_idx_5gb_p[1];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[1];
	} else if (149 <= channel && channel <= 177) {
		*temperature_up_a   = cali_info->delta_swing_table_idx_5ga_p[2];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[2];
		*temperature_up_b   = cali_info->delta_swing_table_idx_5gb_p[2];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[2];
	} else {
		*temperature_up_a   = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_a = (u8 *)delta_swing_table_idx_2ga_n_8188e;
		*temperature_up_b   = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_b = (u8 *)delta_swing_table_idx_2ga_n_8188e;
	}

	return;
}


void get_delta_swing_table_8814a_path_cd(
	void		*dm_void,
	u8 **temperature_up_c,
	u8 **temperature_down_c,
	u8 **temperature_up_d,
	u8 **temperature_down_d
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;
	struct _ADAPTER *adapter = dm->adapter;
	struct dm_rf_calibration_struct	*cali_info = &(dm->rf_calibrate_info);
	u8		tx_rate			= 0xFF;
	u8	channel		 = *dm->channel;

	if (*(dm->mp_mode) == true) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &(adapter->MptCtx);

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		PMPT_CONTEXT p_mpt_ctx = &(adapter->mppriv.mpt_ctx);

		tx_rate = mpt_to_mgnt_rate(p_mpt_ctx->mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16	rate	 = *(dm->forced_data_rate);

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			tx_rate = ((PADAPTER)adapter)->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			tx_rate = hw_rate_to_m_rate(dm->tx_rate);
#endif
		} else   /*force rate*/
			tx_rate = (u8) rate;
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_rate=0x%X\n", __func__, tx_rate);

	if (1 <= channel && channel <= 14) {
		if (IS_CCK_RATE(tx_rate)) {
			*temperature_up_c  = cali_info->delta_swing_table_idx_2g_cck_c_p;
			*temperature_down_c = cali_info->delta_swing_table_idx_2g_cck_c_n;
			*temperature_up_d   = cali_info->delta_swing_table_idx_2g_cck_d_p;
			*temperature_down_d = cali_info->delta_swing_table_idx_2g_cck_d_n;
		} else {
			*temperature_up_c   = cali_info->delta_swing_table_idx_2gc_p;
			*temperature_down_c = cali_info->delta_swing_table_idx_2gc_n;
			*temperature_up_d   = cali_info->delta_swing_table_idx_2gd_p;
			*temperature_down_d = cali_info->delta_swing_table_idx_2gd_n;
		}
	} else if (36 <= channel && channel <= 64) {
		*temperature_up_c   = cali_info->delta_swing_table_idx_5gc_p[0];
		*temperature_down_c = cali_info->delta_swing_table_idx_5gc_n[0];
		*temperature_up_d   = cali_info->delta_swing_table_idx_5gd_p[0];
		*temperature_down_d = cali_info->delta_swing_table_idx_5gd_n[0];
	} else if (100 <= channel && channel <= 144) {
		*temperature_up_c   = cali_info->delta_swing_table_idx_5gc_p[1];
		*temperature_down_c = cali_info->delta_swing_table_idx_5gc_n[1];
		*temperature_up_d   = cali_info->delta_swing_table_idx_5gd_p[1];
		*temperature_down_d = cali_info->delta_swing_table_idx_5gd_n[1];
	} else if (149 <= channel && channel <= 177) {
		*temperature_up_c   = cali_info->delta_swing_table_idx_5gc_p[2];
		*temperature_down_c = cali_info->delta_swing_table_idx_5gc_n[2];
		*temperature_up_d   = cali_info->delta_swing_table_idx_5gd_p[2];
		*temperature_down_d = cali_info->delta_swing_table_idx_5gd_n[2];
	} else {
		*temperature_up_c   = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_c = (u8 *)delta_swing_table_idx_2ga_n_8188e;
		*temperature_up_d   = (u8 *)delta_swing_table_idx_2ga_p_8188e;
		*temperature_down_d = (u8 *)delta_swing_table_idx_2ga_n_8188e;
	}

	return;
}

void configure_txpower_track_8814a(
	struct txpwrtrack_cfg	*config
)
{
	config->swing_table_size_cck = CCK_TABLE_SIZE;
	config->swing_table_size_ofdm = OFDM_TABLE_SIZE;
	config->threshold_iqk = 8;
	config->average_thermal_num = AVG_THERMAL_NUM_8814A;
	config->rf_path_count = MAX_PATH_NUM_8814A;
	config->thermal_reg_addr = RF_T_METER_88E;

	config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8814a;
	config->do_iqk = do_iqk_8814a;
	config->phy_lc_calibrate = halrf_lck_trigger;
	config->get_delta_swing_table = get_delta_swing_table_8814a;
	config->get_delta_swing_table8814only = get_delta_swing_table_8814a_path_cd;
}

void _phy_lc_calibrate_8814a(
	struct dm_struct		*dm
)
{
	u32	lc_cal = 0, cnt;

	/*Check continuous TX and Packet TX*/
	u32	reg0x914 = odm_read_4byte(dm, REG_SINGLE_TONE_CONT_TX_JAGUAR);;

	/*Backup RF reg18.*/
	lc_cal = odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK);

	if ((reg0x914 & 0x70000) == 0)
		odm_write_1byte(dm, REG_TXPAUSE_8812A, 0xFF);

	/*3 3. Read RF reg18*/
	lc_cal = odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK);

	/*3 4. Set LC calibration begin bit15*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal | 0x08000);

	ODM_delay_ms(100);

	for (cnt = 0; cnt < 100; cnt++) {
		if (odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, 0x8000) != 0x1)
			break;

		ODM_delay_ms(10);
	}

	RF_DBG(dm, DBG_RF_IQK, "retry cnt = %d\n", cnt);



	/*3 Restore original situation*/
	if ((reg0x914 & 70000) == 0)
		odm_write_1byte(dm, REG_TXPAUSE_8812A, 0x00);

	/*Recover channel number*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal);

	pr_debug("Call %s\n", __func__);
}

void phy_lc_calibrate_8814a(
	void		*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;

	_phy_lc_calibrate_8814a(dm);
}


#if 0
void _phy_ap_calibrate_8814a(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	s8		delta,
	boolean		is2T
)
{
}

void phy_ap_calibrate_8814a(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	s8		delta
)
{

}

#endif
void phy_dp_calibrate_8814a(
	struct dm_struct	*dm
)
{
}


boolean _phy_query_rf_path_switch_8814a(
	void	*adapter
)
{
	return true;
}


boolean phy_query_rf_path_switch_8814a(
	void	*adapter
)
{

#if DISABLE_BB_RF
	return true;
#endif

	return _phy_query_rf_path_switch_8814a(adapter);
}


void _phy_set_rf_path_switch_8814a(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	boolean		is_main,
	boolean		is2T
)
{
}
void phy_set_rf_path_switch_8814a(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	boolean		is_main
)
{
}




#else	/*(RTL8814A_SUPPORT == 0)*/
void phy_lc_calibrate_8814a(
	struct dm_struct		*dm
) {}

void phy_iq_calibrate_8814a(
	struct dm_struct	*dm,
	boolean is_recovery
) {}
#endif	/* (RTL8814A_SUPPORT == 0)*/
