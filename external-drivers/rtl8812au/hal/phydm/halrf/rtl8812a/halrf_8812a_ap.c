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
#include "../../phydm_precomp.h"



/*---------------------------Define Local Constant---------------------------*/
/* 2010/04/25 MH Define the max tx power tracking tx agc power. */
#define		ODM_TXPWRTRACK_MAX_IDX8812A		6

/*---------------------------Define Local Constant---------------------------*/


/* 3============================================================
 * 3 Tx Power Tracking
 * 3============================================================ */
void halrf_rf_lna_setting_8812a(
	struct dm_struct	*dm,
	enum halrf_lna_set type
)
{
	/*phydm_disable_lna*/
	if (type == HALRF_LNA_DISABLE) {
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x30, 0xfffff, 0x18000);	/*select Rx mode*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x31, 0xfffff, 0x3f7ff);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x32, 0xfffff, 0xc22bf);	/*disable LNA*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x0);
		if (dm->rf_type > RF_1T1R) {
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x1);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x30, 0xfffff, 0x18000);	/*select Rx mode*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x31, 0xfffff, 0x3f7ff);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x32, 0xfffff, 0xc22bf);	/*disable LNA*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x0);
		}
	} else if (type == HALRF_LNA_ENABLE) {
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x30, 0xfffff, 0x18000);	/*select Rx mode*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x31, 0xfffff, 0x3f7ff);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x32, 0xfffff, 0xc26bf);	/*disable LNA*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, 0x80000, 0x0);
		if (dm->rf_type > RF_1T1R) {
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x1);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x30, 0xfffff, 0x18000);	/*select Rx mode*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x31, 0xfffff, 0x3f7ff);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x32, 0xfffff, 0xc26bf);	/*disable LNA*/
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, 0x80000, 0x0);
		}
	}
}


#if 0



/* new element A = element D x X */

/* new element C = element D x Y */


void do_iqk_8812a(
	void		*dm_void,
	u8		delta_thermal_index,
	u8		thermal_value,
	u8		threshold
)
{
	struct dm_struct	*dm = (struct dm_struct *)dm_void;
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	void		*adapter = dm->adapter;
	HAL_DATA_TYPE	*hal_data = GET_HAL_DATA(adapter);
#endif

	odm_reset_iqk_result(dm);

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)
#if USE_WORKITEM
	platform_acquire_mutex(&hal_data->mx_chnl_bw_control);
#else
	platform_acquire_spin_lock(adapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
#endif
#elif ((DEV_BUS_TYPE == RT_USB_INTERFACE) || (DEV_BUS_TYPE == RT_SDIO_INTERFACE))
	platform_acquire_mutex(&hal_data->mx_chnl_bw_control);
#endif
#endif


	dm->rf_calibrate_info.thermal_value_iqk = thermal_value;
	phy_iq_calibrate_8812a(adapter, false);


#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)
#if USE_WORKITEM
	platform_release_mutex(&hal_data->mx_chnl_bw_control);
#else
	platform_release_spin_lock(adapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
#endif
#elif ((DEV_BUS_TYPE == RT_USB_INTERFACE) || (DEV_BUS_TYPE == RT_SDIO_INTERFACE))
	platform_release_mutex(&hal_data->mx_chnl_bw_control);
#endif
#endif
}

/*-----------------------------------------------------------------------------
 * Function:	odm_TxPwrTrackSetPwr88E()
 *
 * Overview:	88E change all channel tx power accordign to flag.
 *				OFDM & CCK are all different.
 *
 * Input:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	04/23/2012	MHC		Create version 0.
 *
 *---------------------------------------------------------------------------*/
void
odm_tx_pwr_track_set_pwr8812a(
	struct dm_struct			*dm,
	enum pwrtrack_method	method,
	u8				rf_path,
	u8				channel_mapped_index
)
{
	if (method == TXAGC) {
		u8	cck_power_level[MAX_TX_COUNT], ofdm_power_level[MAX_TX_COUNT];
		u8	bw20_power_level[MAX_TX_COUNT], bw40_power_level[MAX_TX_COUNT];
		u8	rf = 0;
		u32	pwr = 0, tx_agc = 0;
		void *adapter = dm->adapter;

		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "odm_TxPwrTrackSetPwr88E CH=%d\n", *(dm->channel));
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))

		pwr = phy_query_bb_reg(adapter, REG_TX_AGC_A_RATE18_06, 0xFF);
		pwr += (dm->bb_swing_idx_cck - dm->bb_swing_idx_cck_base);
		tx_agc = (pwr << 16) | (pwr << 8) | (pwr);
		phy_set_bb_reg(adapter, REG_TX_AGC_A_CCK_1_MCS32, MASKBYTE1, tx_agc);
		phy_set_bb_reg(adapter, REG_TX_AGC_B_CCK_11_A_CCK_2_11, 0xffffff00, tx_agc);
		RTPRINT(FPHY, PHY_TXPWR, ("odm_tx_pwr_track_set_pwr88_e: CCK Tx-rf(A) Power = 0x%x\n", tx_agc));

		pwr = phy_query_bb_reg(adapter, REG_TX_AGC_A_RATE18_06, 0xFF);
		pwr += (dm->bb_swing_idx_ofdm[RF_PATH_A] - dm->bb_swing_idx_ofdm_base);
		tx_agc |= ((pwr << 24) | (pwr << 16) | (pwr << 8) | pwr);
		phy_set_bb_reg(adapter, REG_TX_AGC_A_RATE18_06, MASKDWORD, tx_agc);
		phy_set_bb_reg(adapter, REG_TX_AGC_A_RATE54_24, MASKDWORD, tx_agc);
		phy_set_bb_reg(adapter, REG_TX_AGC_A_MCS03_MCS00, MASKDWORD, tx_agc);
		phy_set_bb_reg(adapter, REG_TX_AGC_A_MCS07_MCS04, MASKDWORD, tx_agc);
		phy_set_bb_reg(adapter, REG_TX_AGC_A_MCS11_MCS08, MASKDWORD, tx_agc);
		phy_set_bb_reg(adapter, REG_TX_AGC_A_MCS15_MCS12, MASKDWORD, tx_agc);
		RTPRINT(FPHY, PHY_TXPWR, ("odm_tx_pwr_track_set_pwr88_e: OFDM Tx-rf(A) Power = 0x%x\n", tx_agc));
#endif
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
		phy_rf6052_set_cck_tx_power(dm->priv, *(dm->channel));
		phy_rf6052_set_ofdm_tx_power(dm->priv, *(dm->channel));
#endif

	} else if (method == BBSWING) {
		/* Adjust BB swing by OFDM IQ matrix */
		if (rf_path == RF_PATH_A)
			odm_set_bb_reg(dm, REG_A_TX_SCALE_JAGUAR, MASKDWORD, dm->bb_swing_idx_ofdm[RF_PATH_A]);
		else if (rf_path == RF_PATH_B)
			odm_set_bb_reg(dm, REG_B_TX_SCALE_JAGUAR, MASKDWORD, dm->bb_swing_idx_ofdm[RF_PATH_B]);
	} else
		return;
}	/* odm_TxPwrTrackSetPwr88E */

void configure_txpower_track_8812a(
	struct txpwrtrack_cfg	*config
)
{
	config->swing_table_size_cck = CCK_TABLE_SIZE;
	config->swing_table_size_ofdm = OFDM_TABLE_SIZE;
	config->threshold_iqk = 8;
	config->average_thermal_num = AVG_THERMAL_NUM_8812A;
	config->rf_path_count = 2;
	config->thermal_reg_addr = RF_T_METER_8812A;

	config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8812a;
	config->do_iqk = do_iqk_8812a;
	config->phy_lc_calibrate = phy_lc_calibrate_8812a;
}

#endif

/* 1 7.	IQK */
#define MAX_TOLERANCE		5
#define IQK_DELAY_TIME		1		/* ms */

u8			/* bit0 = 1 => Tx OK, bit1 = 1 => Rx OK */
phy_path_a_iqk_8812a(
	struct dm_struct		*dm,
	boolean		config_path_b
)
{
	u32 reg_eac, reg_e94, reg_e9c, reg_ea4;
	u8 result = 0x00;
	RF_DBG(dm, DBG_RF_IQK, "path A IQK!\n");

	/* 1 Tx IQK */
	/* path-A IQK setting */
	RF_DBG(dm, DBG_RF_IQK, "path-A IQK setting!\n");
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x10008c1c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x30008c1c);
	odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x8214032a);
	odm_set_bb_reg(dm, REG_RX_IQK_PI_A, MASKDWORD, 0x28160000);

	/* LO calibration setting */
	RF_DBG(dm, DBG_RF_IQK, "LO calibration setting!\n");
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x00462911);

	/* One shot, path A LOK & IQK */
	RF_DBG(dm, DBG_RF_IQK, "One shot, path A LOK & IQK!\n");
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf9000000);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8000000);

	/* delay x ms */
	RF_DBG(dm, DBG_RF_IQK, "delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_8812A);
	/* platform_stall_execution(IQK_DELAY_TIME_8812A*1000); */
	ODM_delay_ms(IQK_DELAY_TIME_8812A);

	/* Check failed */
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xeac = 0x%x\n", reg_eac);
	reg_e94 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xe94 = 0x%x\n", reg_e94);
	reg_e9c = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xe9c = 0x%x\n", reg_e9c);
	reg_ea4 = odm_get_bb_reg(dm, REG_RX_POWER_BEFORE_IQK_A_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xea4 = 0x%x\n", reg_ea4);

	if (!(reg_eac & BIT(28)) &&
	    (((reg_e94 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_e9c & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else							/* if Tx not OK, ignore Rx */
		return result;

#if 0
	if (!(reg_eac & BIT(27)) &&		/* if Tx is OK, check whether Rx is OK */
	    (((reg_ea4 & 0x03FF0000) >> 16) != 0x132) &&
	    (((reg_eac & 0x03FF0000) >> 16) != 0x36))
		result |= 0x02;
	else
		RTPRINT(FINIT, INIT_IQK, ("path A Rx IQK fail!!\n"));
#endif

	return result;


}

u8			/* bit0 = 1 => Tx OK, bit1 = 1 => Rx OK */
phy_path_a_rx_iqk_8812a(
	struct dm_struct		*dm,
	boolean		config_path_b
)
{
	u32 reg_eac, reg_e94, reg_e9c, reg_ea4, u4tmp;
	u8 result = 0x00;
	RF_DBG(dm, DBG_RF_IQK, "path A Rx IQK!\n");

	/* 1 Get TXIMR setting */
	/* modify RXIQK mode table */
	RF_DBG(dm, DBG_RF_IQK, "path-A Rx IQK modify RXIQK mode table!\n");
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0);
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, RFREGOFFSETMASK, 0x800a0);
	odm_set_rf_reg(dm, RF_PATH_A, RF_RCK_OS, RFREGOFFSETMASK, 0x30000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G1, RFREGOFFSETMASK, 0x0000f);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G2, RFREGOFFSETMASK, 0xf117B);
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	/* IQK setting */
	odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, 0x01007c00);
	odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x81004800);

	/* path-A IQK setting */
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x10008c1c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x30008c1c);
	odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x82160804);
	odm_set_bb_reg(dm, REG_RX_IQK_PI_A, MASKDWORD, 0x28160000);

	/* LO calibration setting */
	RF_DBG(dm, DBG_RF_IQK, "LO calibration setting!\n");
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x0046a911);

	/* One shot, path A LOK & IQK */
	RF_DBG(dm, DBG_RF_IQK, "One shot, path A LOK & IQK!\n");
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf9000000);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8000000);

	/* delay x ms */
	RF_DBG(dm, DBG_RF_IQK, "delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_8812A);
	/* platform_stall_execution(IQK_DELAY_TIME_8812A*1000); */
	ODM_delay_ms(IQK_DELAY_TIME_8812A);


	/* Check failed */
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xeac = 0x%x\n", reg_eac);
	reg_e94 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xe94 = 0x%x\n", reg_e94);
	reg_e9c = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xe9c = 0x%x\n", reg_e9c);

	if (!(reg_eac & BIT(28)) &&
	    (((reg_e94 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_e9c & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else							/* if Tx not OK, ignore Rx */
		return result;

	u4tmp = 0x80007C00 | (reg_e94 & 0x3FF0000)  | ((reg_e9c & 0x3FF0000) >> 16);
	odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, u4tmp);
	RF_DBG(dm, DBG_RF_IQK, "0xe40 = 0x%x u4tmp = 0x%x\n", odm_get_bb_reg(dm, REG_TX_IQK, MASKDWORD), u4tmp);


	/* 1 RX IQK */
	/* modify RXIQK mode table */
	RF_DBG(dm, DBG_RF_IQK, "path-A Rx IQK modify RXIQK mode table 2!\n");
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0);
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, RFREGOFFSETMASK, 0x800a0);
	odm_set_rf_reg(dm, RF_PATH_A, RF_RCK_OS, RFREGOFFSETMASK, 0x30000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G1, RFREGOFFSETMASK, 0x0000f);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G2, RFREGOFFSETMASK, 0xf7ffa);
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	/* IQK setting */
	odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x01004800);

	/* path-A IQK setting */
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x30008c1c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x10008c1c);
	odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x82160c05);
	odm_set_bb_reg(dm, REG_RX_IQK_PI_A, MASKDWORD, 0x28160c05);

	/* LO calibration setting */
	RF_DBG(dm, DBG_RF_IQK, "LO calibration setting!\n");
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x0046a911);

	/* One shot, path A LOK & IQK */
	RF_DBG(dm, DBG_RF_IQK, "One shot, path A LOK & IQK!\n");
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf9000000);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8000000);

	/* delay x ms */
	RF_DBG(dm, DBG_RF_IQK, "delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_8812A);
	/* platform_stall_execution(IQK_DELAY_TIME_8812A*1000); */
	ODM_delay_ms(IQK_DELAY_TIME_8812A);

	/* Check failed */
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xeac = 0x%x\n", reg_eac);
	reg_e94 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xe94 = 0x%x\n", reg_e94);
	reg_e9c = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xe9c = 0x%x\n", reg_e9c);
	reg_ea4 = odm_get_bb_reg(dm, REG_RX_POWER_BEFORE_IQK_A_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xea4 = 0x%x\n", reg_ea4);

#if 0
	if (!(reg_eac & BIT(28)) &&
	    (((reg_e94 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_e9c & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else							/* if Tx not OK, ignore Rx */
		return result;
#endif

	if (!(reg_eac & BIT(27)) &&		/* if Tx is OK, check whether Rx is OK */
	    (((reg_ea4 & 0x03FF0000) >> 16) != 0x132) &&
	    (((reg_eac & 0x03FF0000) >> 16) != 0x36))
		result |= 0x02;
	else
		RF_DBG(dm, DBG_RF_IQK, "path A Rx IQK fail!!\n");

	return result;


}

u8				/* bit0 = 1 => Tx OK, bit1 = 1 => Rx OK */
phy_path_b_iqk_8812a(
	struct dm_struct		*dm
)
{
	u32 reg_eac, reg_eb4, reg_ebc, reg_ec4, reg_ecc;
	u8	result = 0x00;
	RF_DBG(dm, DBG_RF_IQK, "path B IQK!\n");

	/* One shot, path B LOK & IQK */
	RF_DBG(dm, DBG_RF_IQK, "One shot, path A LOK & IQK!\n");
	odm_set_bb_reg(dm, REG_IQK_AGC_CONT, MASKDWORD, 0x00000002);
	odm_set_bb_reg(dm, REG_IQK_AGC_CONT, MASKDWORD, 0x00000000);

	/* delay x ms */
	RF_DBG(dm, DBG_RF_IQK, "delay %d ms for One shot, path B LOK & IQK.\n", IQK_DELAY_TIME_8812A);
	/* platform_stall_execution(IQK_DELAY_TIME_8812A*1000); */
	ODM_delay_ms(IQK_DELAY_TIME_8812A);

	/* Check failed */
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xeac = 0x%x\n", reg_eac);
	reg_eb4 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_B, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xeb4 = 0x%x\n", reg_eb4);
	reg_ebc = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_B, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xebc = 0x%x\n", reg_ebc);
	reg_ec4 = odm_get_bb_reg(dm, REG_RX_POWER_BEFORE_IQK_B_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xec4 = 0x%x\n", reg_ec4);
	reg_ecc = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_B_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "0xecc = 0x%x\n", reg_ecc);

	if (!(reg_eac & BIT(31)) &&
	    (((reg_eb4 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_ebc & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else
		return result;

	if (!(reg_eac & BIT(30)) &&
	    (((reg_ec4 & 0x03FF0000) >> 16) != 0x132) &&
	    (((reg_ecc & 0x03FF0000) >> 16) != 0x36))
		result |= 0x02;
	else
		RF_DBG(dm, DBG_RF_IQK, "path B Rx IQK fail!!\n");


	return result;

}

void
_phy_path_a_fill_iqk_matrix_8812a(
	struct dm_struct		*dm,
	boolean	is_iqk_ok,
	s32		result[][8],
	u8		final_candidate,
	boolean		is_tx_only
)
{
	u32	oldval_0, X, TX0_A, reg;
	s32	Y, TX0_C;
	RF_DBG(dm, DBG_RF_IQK, "path A IQ Calibration %s !\n", (is_iqk_ok) ? "Success" : "Failed");

	if (final_candidate == 0xFF)
		return;

	else if (is_iqk_ok) {
		oldval_0 = (odm_get_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, MASKDWORD) >> 22) & 0x3FF;

		X = result[final_candidate][0];
		if ((X & 0x00000200) != 0)
			X = X | 0xFFFFFC00;
		TX0_A = (X * oldval_0) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "X = 0x%x, TX0_A = 0x%x, oldval_0 0x%x\n", X, TX0_A, oldval_0);
		odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, 0x3FF, TX0_A);

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(31), ((X * oldval_0 >> 7) & 0x1));

		Y = result[final_candidate][1];
		if ((Y & 0x00000200) != 0)
			Y = Y | 0xFFFFFC00;


		TX0_C = (Y * oldval_0) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "Y = 0x%x, TX = 0x%x\n", (u32)Y, (u32)TX0_C);
		odm_set_bb_reg(dm, REG_OFDM_0_XC_TX_AFE, 0xF0000000, ((TX0_C & 0x3C0) >> 6));
		odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, 0x003F0000, (TX0_C & 0x3F));

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(29), ((Y * oldval_0 >> 7) & 0x1));

		if (is_tx_only) {
			RF_DBG(dm, DBG_RF_IQK, "_phy_path_a_fill_iqk_matrix_8812a only Tx OK\n");
			return;
		}

		reg = result[final_candidate][2];
#if (DM_ODM_SUPPORT_TYPE == ODM_AP)
		if (RTL_ABS(reg, 0x100) >= 16)
			reg = 0x100;
#endif
		odm_set_bb_reg(dm, REG_OFDM_0_XA_RX_IQ_IMBALANCE, 0x3FF, reg);

		reg = result[final_candidate][3] & 0x3F;
		odm_set_bb_reg(dm, REG_OFDM_0_XA_RX_IQ_IMBALANCE, 0xFC00, reg);

		reg = (result[final_candidate][3] >> 6) & 0xF;
		odm_set_bb_reg(dm, REG_OFDM_0_RX_IQ_EXT_ANTA, 0xF0000000, reg);
	}
}

void
_phy_path_b_fill_iqk_matrix_8812a(
	struct dm_struct		*dm,
	boolean	is_iqk_ok,
	s32		result[][8],
	u8		final_candidate,
	boolean		is_tx_only			/* do Tx only */
)
{
	u32	oldval_1, X, TX1_A, reg;
	s32	Y, TX1_C;
	RF_DBG(dm, DBG_RF_IQK, "path B IQ Calibration %s !\n", (is_iqk_ok) ? "Success" : "Failed");

	if (final_candidate == 0xFF)
		return;

	else if (is_iqk_ok) {
		oldval_1 = (odm_get_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, MASKDWORD) >> 22) & 0x3FF;

		X = result[final_candidate][4];
		if ((X & 0x00000200) != 0)
			X = X | 0xFFFFFC00;
		TX1_A = (X * oldval_1) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "X = 0x%x, TX1_A = 0x%x\n", X, TX1_A);
		odm_set_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, 0x3FF, TX1_A);

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(27), ((X * oldval_1 >> 7) & 0x1));

		Y = result[final_candidate][5];
		if ((Y & 0x00000200) != 0)
			Y = Y | 0xFFFFFC00;

		TX1_C = (Y * oldval_1) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "Y = 0x%x, TX1_C = 0x%x\n", (u32)Y, (u32)TX1_C);
		odm_set_bb_reg(dm, REG_OFDM_0_XD_TX_AFE, 0xF0000000, ((TX1_C & 0x3C0) >> 6));
		odm_set_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, 0x003F0000, (TX1_C & 0x3F));

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(25), ((Y * oldval_1 >> 7) & 0x1));

		if (is_tx_only)
			return;

		reg = result[final_candidate][6];
		odm_set_bb_reg(dm, REG_OFDM_0_XB_RX_IQ_IMBALANCE, 0x3FF, reg);

		reg = result[final_candidate][7] & 0x3F;
		odm_set_bb_reg(dm, REG_OFDM_0_XB_RX_IQ_IMBALANCE, 0xFC00, reg);

		reg = (result[final_candidate][7] >> 6) & 0xF;
		odm_set_bb_reg(dm, REG_OFDM_0_AGC_RSSI_TABLE, 0x0000F000, reg);
	}
}

void
_phy_save_adda_registers_8812a(
	struct dm_struct		*dm,
	u32		*adda_reg,
	u32		*adda_backup,
	u32		register_num
)
{
	u32	i;

	RF_DBG(dm, DBG_RF_IQK, "Save ADDA parameters.\n");
	for (i = 0 ; i < register_num ; i++)
		adda_backup[i] = odm_get_bb_reg(dm, adda_reg[i], MASKDWORD);
}


void
_phy_save_mac_registers_8812a(
	struct dm_struct		*dm,
	u32		*mac_reg,
	u32		*mac_backup
)
{
	u32	i;
	RF_DBG(dm, DBG_RF_IQK, "Save MAC parameters.\n");
	for (i = 0 ; i < (IQK_MAC_REG_NUM - 1); i++)
		mac_backup[i] = odm_read_1byte(dm, mac_reg[i]);
	mac_backup[i] = odm_read_4byte(dm, mac_reg[i]);

}


void
_phy_reload_adda_registers_8812a(
	struct dm_struct		*dm,
	u32		*adda_reg,
	u32		*adda_backup,
	u32		regiester_num
)
{
	u32	i;

	RF_DBG(dm, DBG_RF_IQK, "Reload ADDA power saving parameters !\n");
	for (i = 0 ; i < regiester_num; i++)
		odm_set_bb_reg(dm, adda_reg[i], MASKDWORD, adda_backup[i]);
}

void
_phy_reload_mac_registers_8812a(
	struct dm_struct		*dm,
	u32		*mac_reg,
	u32		*mac_backup
)
{
	u32	i;
	RF_DBG(dm, DBG_RF_IQK, "Reload MAC parameters !\n");
	for (i = 0 ; i < (IQK_MAC_REG_NUM - 1); i++)
		odm_write_1byte(dm, mac_reg[i], (u8)mac_backup[i]);
	odm_write_4byte(dm, mac_reg[i], mac_backup[i]);
}


void
_phy_path_adda_on_8812a(
	struct dm_struct		*dm,
	u32		*adda_reg,
	boolean		is_path_a_on,
	boolean		is2T
)
{
	u32	path_on;
	u32	i;
	RF_DBG(dm, DBG_RF_IQK, "ADDA ON.\n");

	path_on = is_path_a_on ? 0x04db25a4 : 0x0b1b25a4;
	if (false == is2T) {
		path_on = 0x0bdb25a0;
		odm_set_bb_reg(dm, adda_reg[0], MASKDWORD, 0x0b1b25a0);
	} else
		odm_set_bb_reg(dm, adda_reg[0], MASKDWORD, path_on);

	for (i = 1 ; i < IQK_ADDA_REG_NUM ; i++)
		odm_set_bb_reg(dm, adda_reg[i], MASKDWORD, path_on);

}

void
_phy_mac_setting_calibration_8812a(
	struct dm_struct		*dm,
	u32		*mac_reg,
	u32		*mac_backup
)
{
	u32	i = 0;
	RF_DBG(dm, DBG_RF_IQK, "MAC settings for Calibration.\n");

	odm_write_1byte(dm, mac_reg[i], 0x3F);

	for (i = 1 ; i < (IQK_MAC_REG_NUM - 1); i++)
		odm_write_1byte(dm, mac_reg[i], (u8)(mac_backup[i] & (~BIT(3))));
	odm_write_1byte(dm, mac_reg[i], (u8)(mac_backup[i] & (~BIT(5))));

}

void
_phy_path_a_stand_by_8812a(
	struct dm_struct		*dm
)
{
	RF_DBG(dm, DBG_RF_IQK, "path-A standby mode!\n");

	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x0);
	odm_set_bb_reg(dm, R_0x840, MASKDWORD, 0x00010000);
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);
}

void
_phy_pi_mode_switch_8812a(
	struct dm_struct		*dm,
	boolean		pi_mode
)
{
	u32	mode;
	RF_DBG(dm, DBG_RF_IQK, "BB Switch to %s mode!\n", (pi_mode ? "PI" : "SI"));

	mode = pi_mode ? 0x01000100 : 0x01000000;
	odm_set_bb_reg(dm, REG_FPGA0_XA_HSSI_PARAMETER1, MASKDWORD, mode);
	odm_set_bb_reg(dm, REG_FPGA0_XB_HSSI_PARAMETER1, MASKDWORD, mode);
}

boolean
phy_simularity_compare_8812a(
	struct dm_struct		*dm,
	s32		result[][8],
	u8		 c1,
	u8		 c2
)
{
	u32		i, j, diff, simularity_bit_map, bound = 0;
	u8		final_candidate[2] = {0xFF, 0xFF};	/* for path A and path B */
	boolean		is_result = true;
	boolean		is2T = 0;

	if (is2T)
		bound = 8;
	else
		bound = 4;

	RF_DBG(dm, DBG_RF_IQK, "===> IQK:phy_simularity_compare_8812a c1 %d c2 %d!!!\n", c1, c2);


	simularity_bit_map = 0;

	for (i = 0; i < bound; i++) {
		diff = (result[c1][i] > result[c2][i]) ? (result[c1][i] - result[c2][i]) : (result[c2][i] - result[c1][i]);
		if (diff > MAX_TOLERANCE) {
			RF_DBG(dm, DBG_RF_IQK, "IQK:phy_simularity_compare_8812a differnece overflow index %d compare1 0x%x compare2 0x%x!!!\n",  i, (u32)result[c1][i], (u32)result[c2][i]);

			if ((i == 2 || i == 6) && !simularity_bit_map) {
				if (result[c1][i] + result[c1][i + 1] == 0)
					final_candidate[(i / 4)] = c2;
				else if (result[c2][i] + result[c2][i + 1] == 0)
					final_candidate[(i / 4)] = c1;
				else
					simularity_bit_map = simularity_bit_map | (1 << i);
			} else
				simularity_bit_map = simularity_bit_map | (1 << i);
		}
	}

	RF_DBG(dm, DBG_RF_IQK, "IQK:phy_simularity_compare_8812a simularity_bit_map   %d !!!\n", simularity_bit_map);

	if (simularity_bit_map == 0) {
		for (i = 0; i < (bound / 4); i++) {
			if (final_candidate[i] != 0xFF) {
				for (j = i * 4; j < (i + 1) * 4 - 2; j++)
					result[3][j] = result[final_candidate[i]][j];
				is_result = false;
			}
		}
		return is_result;
	} else if (!(simularity_bit_map & 0x0F)) {		/* path A OK */
		for (i = 0; i < 4; i++)
			result[3][i] = result[c1][i];
		return false;
	} else if (!(simularity_bit_map & 0xF0) && is2T) {	/* path B OK */
		for (i = 4; i < 8; i++)
			result[3][i] = result[c1][i];
		return false;
	} else
		return false;

}
#if 0
	#define BW_20M	0
	#define	BW_40M  1
	#define	BW_80M	2
#endif

void _iqk_rx_fill_iqc_8812a(
	struct dm_struct			*dm,
	enum rf_path	path,
	unsigned int			RX_X,
	unsigned int			RX_Y
)
{
	switch (path) {
	case RF_PATH_A:
	{
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
		odm_set_bb_reg(dm, R_0xc10, 0x000003ff, RX_X >> 1);
		odm_set_bb_reg(dm, R_0xc10, 0x03ff0000, RX_Y >> 1);
		RF_DBG(dm, DBG_RF_IQK, "RX_X = %x;;RX_Y = %x ====>fill to IQC\n", RX_X >> 1 & 0x000003ff, RX_Y >> 1 & 0x000003ff);
		RF_DBG(dm, DBG_RF_IQK, "0xc10 = %x ====>fill to IQC\n", odm_read_4byte(dm, 0xc10));
	}
	break;
	case RF_PATH_B:
	{
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
		odm_set_bb_reg(dm, R_0xe10, 0x000003ff, RX_X >> 1);
		odm_set_bb_reg(dm, R_0xe10, 0x03ff0000, RX_Y >> 1);
		RF_DBG(dm, DBG_RF_IQK, "RX_X = %x;;RX_Y = %x====>fill to IQC\n ", RX_X >> 1 & 0x000003ff, RX_Y >> 1 & 0x000003ff);
		RF_DBG(dm, DBG_RF_IQK, "0xe10 = %x====>fill to IQC\n", odm_read_4byte(dm, 0xe10));
	}
	break;
	default:
		break;
	};
}

void _iqk_tx_fill_iqc_8812a(
	struct dm_struct			*dm,
	enum rf_path	path,
	unsigned int			TX_X,
	unsigned int			TX_Y
)
{
	switch (path) {
	case RF_PATH_A:
	{
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
		odm_write_4byte(dm, 0xc90, 0x00000080);
		odm_write_4byte(dm, 0xcc4, 0x20040000);
		odm_write_4byte(dm, 0xcc8, 0x20000000);
		odm_set_bb_reg(dm, R_0xccc, 0x000007ff, TX_Y);
		odm_set_bb_reg(dm, R_0xcd4, 0x000007ff, TX_X);
		RF_DBG(dm, DBG_RF_IQK, "TX_X = %x;;TX_Y = %x =====> fill to IQC\n", TX_X & 0x000007ff, TX_Y & 0x000007ff);
		RF_DBG(dm, DBG_RF_IQK, "0xcd4 = %x;;0xccc = %x ====>fill to IQC\n", odm_get_bb_reg(dm, R_0xcd4, 0x000007ff), odm_get_bb_reg(dm, R_0xccc, 0x000007ff));
	}
	break;
	case RF_PATH_B:
	{
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
		odm_write_4byte(dm, 0xe90, 0x00000080);
		odm_write_4byte(dm, 0xec4, 0x20040000);
		odm_write_4byte(dm, 0xec8, 0x20000000);
		odm_set_bb_reg(dm, R_0xecc, 0x000007ff, TX_Y);
		odm_set_bb_reg(dm, R_0xed4, 0x000007ff, TX_X);
		RF_DBG(dm, DBG_RF_IQK, "TX_X = %x;;TX_Y = %x =====> fill to IQC\n", TX_X & 0x000007ff, TX_Y & 0x000007ff);
		RF_DBG(dm, DBG_RF_IQK, "0xed4 = %x;;0xecc = %x ====>fill to IQC\n", odm_get_bb_reg(dm, R_0xed4, 0x000007ff), odm_get_bb_reg(dm, R_0xecc, 0x000007ff));
	}
	break;
	default:
		break;
	};
}

void _iqk_backup_mac_bb_8812a(
	struct dm_struct		*dm,
	u32		*MACBB_backup,
	u32		*backup_macbb_reg,
	u32		MACBB_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* save MACBB default value */
	for (i = 0; i < MACBB_NUM; i++)
		MACBB_backup[i] = odm_read_4byte(dm, backup_macbb_reg[i]);

	RF_DBG(dm, DBG_RF_IQK, "BackupMacBB Success!!!!\n");
}
void _iqk_backup_rf_8812a(
	struct dm_struct		*dm,
	u32		*RFA_backup,
	u32		*RFB_backup,
	u32		*backup_rf_reg,
	u32		RF_NUM
)
{

	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Save RF Parameters */
	for (i = 0; i < RF_NUM; i++) {
		RFA_backup[i] = odm_get_rf_reg(dm, RF_PATH_A, backup_rf_reg[i], MASKDWORD);
		RFB_backup[i] = odm_get_rf_reg(dm, RF_PATH_B, backup_rf_reg[i], MASKDWORD);
	}
	RF_DBG(dm, DBG_RF_IQK, "BackupRF Success!!!!\n");
}
void _iqk_backup_afe_8812a(
	struct dm_struct		*dm,
	u32		*AFE_backup,
	u32		*backup_afe_reg,
	u32		AFE_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Save AFE Parameters */
	for (i = 0; i < AFE_NUM; i++)
		AFE_backup[i] = odm_read_4byte(dm, backup_afe_reg[i]);
	RF_DBG(dm, DBG_RF_IQK, "BackupAFE Success!!!!\n");
}
void _iqk_restore_mac_bb_8812a(
	struct dm_struct		*dm,
	u32		*MACBB_backup,
	u32		*backup_macbb_reg,
	u32		MACBB_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Reload MacBB Parameters */
	for (i = 0; i < MACBB_NUM; i++)
		odm_write_4byte(dm, backup_macbb_reg[i], MACBB_backup[i]);
	RF_DBG(dm, DBG_RF_IQK, "RestoreMacBB Success!!!!\n");
}
void _iqk_restore_rf_8812a(
	struct dm_struct			*dm,
	enum rf_path	path,
	u32			*backup_rf_reg,
	u32			*RF_backup,
	u32			RF_REG_NUM
)
{
	u32 i;

	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	for (i = 0; i < RF_REG_NUM; i++)
		odm_set_rf_reg(dm, (enum rf_path)path, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i]);
	odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x0);

	switch (path) {
	case RF_PATH_A:
	{
		RF_DBG(dm, DBG_RF_IQK, "RestoreRF path A Success!!!!\n");
	}
	break;
	case RF_PATH_B:
	{
		RF_DBG(dm, DBG_RF_IQK, "RestoreRF path B Success!!!!\n");
	}
	break;
	default:
		break;
	}
}
void _iqk_restore_afe_8812a(
	struct dm_struct		*dm,
	u32		*AFE_backup,
	u32		*backup_afe_reg,
	u32		AFE_NUM
)
{
	u32 i;
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	/* Reload AFE Parameters */
	for (i = 0; i < AFE_NUM; i++)
		odm_write_4byte(dm, backup_afe_reg[i], AFE_backup[i]);
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1); /* [31] = 1 --> Page C1 */
	odm_write_4byte(dm, 0xc80, 0x0);
	odm_write_4byte(dm, 0xc84, 0x0);
	odm_write_4byte(dm, 0xc88, 0x0);
	odm_write_4byte(dm, 0xc8c, 0x3c000000);
	odm_write_4byte(dm, 0xcb8, 0x0);
	odm_write_4byte(dm, 0xe80, 0x0);
	odm_write_4byte(dm, 0xe84, 0x0);
	odm_write_4byte(dm, 0xe88, 0x0);
	odm_write_4byte(dm, 0xe8c, 0x3c000000);
	odm_write_4byte(dm, 0xeb8, 0x0);
	RF_DBG(dm, DBG_RF_IQK, "RestoreAFE Success!!!!\n");
}


void _iqk_configure_mac_8812a(
	struct dm_struct		*dm
)
{
	/* ========MAC register setting======== */
	odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0); /* [31] = 0 --> Page C */
	odm_write_1byte(dm, 0x522, 0x3f);
	odm_set_bb_reg(dm, R_0x550, BIT(11) | BIT(3), 0x0);
	odm_set_bb_reg(dm, R_0x808, BIT(28), 0x0);	/*		CCK Off */
	odm_write_1byte(dm, 0x808, 0x00);		/*		RX ante off */
	odm_set_bb_reg(dm, R_0x838, 0xf, 0xc);		/*		CCA off */
}

#define cal_num 3

void _iqk_tx_8812a(
	struct dm_struct		*dm,
	enum rf_path path,
	u8 chnl_idx
)
{
	u32		TX_fail, RX_fail, delay_count, IQK_ready, cal_retry, cal = 0, temp_reg65;
	int			TX_X = 0, TX_Y = 0, RX_X = 0, RX_Y = 0, tx_average = 0, rx_average = 0;
	int			TX_X0[cal_num], TX_Y0[cal_num], RX_X0[cal_num], RX_Y0[cal_num];
	boolean	TX0IQKOK = false, RX0IQKOK = false;
	int			TX_X1[cal_num], TX_Y1[cal_num], RX_X1[cal_num], RX_Y1[cal_num];
	boolean	TX1IQKOK = false, RX1IQKOK = false, VDF_enable = false;
	int			i, k, VDF_Y[3], VDF_X[3], tx_dt[3], rx_dt[3], ii, dx = 0, dy = 0, TX_finish = 0, RX_finish = 0;
	struct dm_rf_calibration_struct  *cali_info = &(dm->rf_calibrate_info);
	struct rtl8192cd_priv		*priv = dm->priv;

	dm->priv->pshare->IQK_total_cnt++;

	RF_DBG(dm, DBG_RF_IQK, "band_width = %d ext_pa = %d pBand = %d\n", *dm->band_width, dm->ext_pa, *dm->band_type);

	if (*dm->band_width == 2)
		VDF_enable = true;

	temp_reg65 = odm_get_rf_reg(dm, (enum rf_path)path, RF_0x65, MASKDWORD);

	switch (path) {
	case RF_PATH_A:
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);

		/* Port 0 DAC/ADC on*/
		odm_write_4byte(dm, 0xc60, 0x77777777);
		odm_write_4byte(dm, 0xc64, 0x77777777);

		/*Port 1 DAC/ADC off*/
		odm_write_4byte(dm, 0xe60, 0x00000000);
		odm_write_4byte(dm, 0xe64, 0x00000000);

		odm_write_4byte(dm, 0xc68, 0x19791979);
		odm_set_bb_reg(dm, R_0xc00, 0xf, 0x4);

		/*DAC/ADC sampling rate (160 MHz)*/
		odm_set_bb_reg(dm, R_0xc5c, BIT(26) | BIT(25) | BIT(24), 0x7);

		odm_set_bb_reg(dm, R_0xcb0, 0x00ff0000, 0x77);
		odm_set_bb_reg(dm, R_0xcb4, 0x03000000, 0x0);
		break;
	case RF_PATH_B:
		odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
		/*Port 0 DAC/ADC off*/
		odm_write_4byte(dm, 0xc60, 0x00000000);
		odm_write_4byte(dm, 0xc64, 0x00000000);

		/*Port 1 DAC/ADC on*/
		odm_write_4byte(dm, 0xe60, 0x77777777);
		odm_write_4byte(dm, 0xe64, 0x77777777);

		odm_write_4byte(dm, 0xe68, 0x19791979);

		odm_set_bb_reg(dm, R_0xe00, 0xf, 0x4);

		/*DAC/ADC sampling rate (160 MHz)*/
		odm_set_bb_reg(dm, R_0xe5c, BIT(26) | BIT(25) | BIT(24), 0x7);
		odm_set_bb_reg(dm, R_0xeb0, 0x00ff0000, 0x77);
		odm_set_bb_reg(dm, R_0xeb4, 0x03000000, 0x0);
		break;
	default:
		break;
	}

	while (cal < cal_num) {
		switch (path) {
		case RF_PATH_A:
		{
			/*======pathA TX IQK ======*/
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x80002);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x30, RFREGOFFSETMASK, 0x20000);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x31, RFREGOFFSETMASK, 0x3fffd);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x32, RFREGOFFSETMASK, 0xfe83f);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x65, RFREGOFFSETMASK, 0x931d5);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREGOFFSETMASK, 0x8a001);
			odm_write_4byte(dm, 0x90c, 0x00008000);
			odm_write_4byte(dm, 0xb00, 0x03000100);
			odm_set_bb_reg(dm, R_0xc94, BIT(0), 0x1);
			odm_write_4byte(dm, 0x978, 0x29002000);/*TX (X,Y)*/
			odm_write_4byte(dm, 0x97c, 0xa9002000);/*RX (X,Y)*/
			odm_write_4byte(dm, 0x984, 0x00462910);/*[0]:AGC_en, [15]:idac_K_Mask*/

			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);

			if (dm->ext_pa)
				odm_write_4byte(dm, 0xc88, 0x821403e3);
			else
				odm_write_4byte(dm, 0xc88, 0x821403f1);

			if (*dm->band_type == ODM_BAND_5G)
				odm_write_4byte(dm, 0xc8c, 0x68163e96);
			else
				odm_write_4byte(dm, 0xc8c, 0x28163e96);

			if (VDF_enable == 1) {
				/*====== pathA VDF TX IQK ======*/
				RF_DBG(dm, DBG_RF_IQK, "TXVDF Start\n");
				for (k = 0; k <= 2; k++) {
					switch (k) {
					case 0:
						odm_write_4byte(dm, 0xc80, 0x18008c38);/*TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16*/
						odm_write_4byte(dm, 0xc84, 0x38008c38);/*RX_Tone_idx[9:0], RxK_Mask[29]*/
						odm_write_4byte(dm, 0x984, 0x00462910);/*[0]:AGC_en, [15]:idac_K_Mask*/
						odm_set_bb_reg(dm, R_0xce8, BIT(31), 0x0);
						break;
					case 1:
						odm_set_bb_reg(dm, R_0xc80, BIT(28), 0x0);
						odm_set_bb_reg(dm, R_0xc84, BIT(28), 0x0);
						odm_write_4byte(dm, 0x984, 0x0046a910);/*[0]:AGC_en, [15]:idac_K_Mask*/
						break;
					case 2:
						RF_DBG(dm, DBG_RF_IQK, "VDF_Y[1] = %x;;;VDF_Y[0] = %x\n", VDF_Y[1] >> 21 & 0x00007ff, VDF_Y[0] >> 21 & 0x00007ff);
						RF_DBG(dm, DBG_RF_IQK, "VDF_X[1] = %x;;;VDF_X[0] = %x\n", VDF_X[1] >> 21 & 0x00007ff, VDF_X[0] >> 21 & 0x00007ff);
						tx_dt[cal] = (VDF_Y[1] >> 20) - (VDF_Y[0] >> 20);
						RF_DBG(dm, DBG_RF_IQK, "tx_dt = %d\n", tx_dt[cal]);
						tx_dt[cal] = ((16 * tx_dt[cal]) * 10000 / 15708);
						tx_dt[cal] = (tx_dt[cal] >> 1) + (tx_dt[cal] & BIT(0));
						odm_write_4byte(dm, 0xc80, 0x18008c20);
						odm_write_4byte(dm, 0xc84, 0x38008c20);
						odm_set_bb_reg(dm, R_0xce8, BIT(31), 0x1);
						odm_set_bb_reg(dm, R_0xce8, 0x3fff0000, tx_dt[cal] & 0x00003fff);
						break;
					default:
						break;
					}

					odm_write_4byte(dm, 0xcb8, 0x00100000);/*cb8[20] SI/PI*/
					cal_retry = 0;
					while (1) {
						/*one shot*/
						odm_write_4byte(dm, 0x980, 0xfa000000);
						odm_write_4byte(dm, 0x980, 0xf8000000);
						delay_ms(10);
						odm_write_4byte(dm, 0xcb8, 0x00000000);
						delay_count = 0;
						while (1) {
							IQK_ready = odm_get_bb_reg(dm, R_0xd00, BIT(10));
							if (IQK_ready || (delay_count > 20))
								break;
							delay_ms(1);
							delay_count++;
						}

						if (delay_count < 20) {
							/*============pathA VDF TXIQK Check==============*/
							TX_fail = odm_get_bb_reg(dm, R_0xd00, BIT(12));
							if (~TX_fail) {
								odm_write_4byte(dm, 0xcb8, 0x02000000);
								VDF_X[k] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
								odm_write_4byte(dm, 0xcb8, 0x04000000);
								VDF_Y[k] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
								TX0IQKOK = true;
								break;
							} else {
								TX0IQKOK = false;
								cal_retry++;
								if (cal_retry == 10)
									break;
							}
						} else {
							TX0IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					}
				}
				RF_DBG(dm, DBG_RF_IQK, "TXA_VDF_cal_retry = %d\n", cal_retry);
				TX_X0[cal] = VDF_X[k - 1];
				TX_Y0[cal] = VDF_Y[k - 1];
			} else {

				/*====== pathA TX IQK ======*/
				odm_write_4byte(dm, 0xc80, 0x18008c10);/*TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16*/
				odm_write_4byte(dm, 0xc84, 0x38008c10);/*RX_Tone_idx[9:0], RxK_Mask[29]*/
				odm_write_4byte(dm, 0xce8, 0x00000000);
				odm_write_4byte(dm, 0xcb8, 0x00100000);
				cal_retry = 0;
				while (1) {
					odm_write_4byte(dm, 0x980, 0xfa000000);
					odm_write_4byte(dm, 0x980, 0xf8000000);
					delay_ms(10); /* delay 25ms */
					odm_write_4byte(dm, 0xcb8, 0x00000000);
					delay_count = 0;
					while (1) {
						IQK_ready = odm_get_bb_reg(dm, R_0xd00, BIT(10));
						if (IQK_ready || (delay_count > 20))
							break;
						delay_ms(1);
						delay_count++;
					}

					if (delay_count < 20) {
						/*============pathA TXIQK Check==============*/
						TX_fail = odm_get_bb_reg(dm, R_0xd00, BIT(12));
						if (~TX_fail) {
							odm_write_4byte(dm, 0xcb8, 0x02000000);
							TX_X0[cal] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
							odm_write_4byte(dm, 0xcb8, 0x04000000);
							TX_Y0[cal] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
							TX0IQKOK = true;
#if 0
							odm_write_4byte(dm, 0xcb8, 0x01000000);
							reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
							odm_write_4byte(dm, 0xcb8, 0x02000000);
							reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
							image_power = (reg2 << 32) + reg1;
							dbg_print("Before PW = %d\n", image_power);
							odm_write_4byte(dm, 0xcb8, 0x03000000);
							reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
							odm_write_4byte(dm, 0xcb8, 0x04000000);
							reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
							image_power = (reg2 << 32) + reg1;
							dbg_print("After PW = %d\n", image_power);
#endif
							break;
						} else {
							TX0IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					} else {
						TX0IQKOK = false;
						cal_retry++;
						if (cal_retry == 10)
							break;
					}
				}

				RF_DBG(dm, DBG_RF_IQK, "TXA_cal_retry = %d\n", cal_retry);
			}

			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x58, 0x7fe00, odm_get_rf_reg(dm, (enum rf_path)path, RF_0x8, 0xffc00));
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
			if (TX0IQKOK == false)
				break;


			/*======pathA VDF RX IQK ======*/
			if (VDF_enable == 1) {
				odm_set_bb_reg(dm, R_0xce8, BIT(31), 0x0);/*TX VDF Disable*/
				RF_DBG(dm, DBG_RF_IQK, "RXVDF Start\n");

				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x80000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x30, RFREGOFFSETMASK, 0x30000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x31, RFREGOFFSETMASK, 0x3f7ff);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x32, RFREGOFFSETMASK, 0xfe7bf);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREGOFFSETMASK, 0x88001);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x65, RFREGOFFSETMASK, 0x931d0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x00000);
				odm_set_bb_reg(dm, R_0x978, BIT(31), 0x1);
				odm_set_bb_reg(dm, R_0x97c, BIT(31), 0x0);
				odm_write_4byte(dm, 0x984, 0x0046a911);

				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
				odm_write_4byte(dm, 0xc88, 0x02140119);
				odm_write_4byte(dm, 0xc8c, 0x28161420);

				for (k = 0; k <= 2; k++) {
					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
					odm_set_bb_reg(dm, R_0x978, 0x03FF8000, (VDF_X[k]) >> 21 & 0x000007ff);
					odm_set_bb_reg(dm, R_0x978, 0x000007FF, (VDF_Y[k]) >> 21 & 0x000007ff);

					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
					switch (k) {
					case 0:
						odm_write_4byte(dm, 0xc80, 0x38008c38);/*TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16*/
						odm_write_4byte(dm, 0xc84, 0x18008c38);/*RX_Tone_idx[9:0], RxK_Mask[29]*/
						odm_set_bb_reg(dm, R_0xce8, BIT(30), 0x0);
						break;
					case 1:
						odm_write_4byte(dm, 0xc80, 0x28008c38);
						odm_write_4byte(dm, 0xc84, 0x08008c38);
						break;
					case 2:
						RF_DBG(dm, DBG_RF_IQK, "VDF_Y[1] = %x;;;VDF_Y[0] = %x\n", VDF_Y[1] >> 21 & 0x00007ff, VDF_Y[0] >> 21 & 0x00007ff);
						RF_DBG(dm, DBG_RF_IQK, "VDF_X[1] = %x;;;VDF_X[0] = %x\n", VDF_X[1] >> 21 & 0x00007ff, VDF_X[0] >> 21 & 0x00007ff);
						rx_dt[cal] = (VDF_Y[1] >> 20) - (VDF_Y[0] >> 20);
						RF_DBG(dm, DBG_RF_IQK, "rx_dt = %d\n", rx_dt[cal]);
						rx_dt[cal] = ((16 * rx_dt[cal]) * 10000 / 13823);
						rx_dt[cal] = (rx_dt[cal] >> 1) + (rx_dt[cal] & BIT(0));
						odm_write_4byte(dm, 0xc80, 0x38008c20);
						odm_write_4byte(dm, 0xc84, 0x18008c20);
						odm_set_bb_reg(dm, R_0xce8, 0x00003fff, rx_dt[cal] & 0x00003fff);
						break;
					default:
						break;
					}

					if (k == 2)
						odm_set_bb_reg(dm, R_0xce8, BIT(30), 0x1);  /*RX VDF Enable*/

					odm_write_4byte(dm, 0xcb8, 0x00100000);/*cb8[20] N SI/PI*/
					cal_retry = 0;
					while (1) {
						/*one shot*/
						odm_write_4byte(dm, 0x980, 0xfa000000);
						odm_write_4byte(dm, 0x980, 0xf8000000);
						delay_ms(10);
						odm_write_4byte(dm, 0xcb8, 0x00000000);
						delay_count = 0;
						while (1) {
							IQK_ready = odm_get_bb_reg(dm, R_0xd00, BIT(10));
							if (IQK_ready || (delay_count > 20))
								break;
							delay_ms(1);
							delay_count++;
						}

						odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
						RF_DBG(dm, DBG_RF_IQK, "==== A VDF: path A RF0 = 0x%x ====\n",
							odm_get_rf_reg(dm, (enum rf_path)0, RF_0x0, RFREGOFFSETMASK));
						RF_DBG(dm, DBG_RF_IQK, "==== A VDF: path B RF0 = 0x%x ====\n",
							odm_get_rf_reg(dm, (enum rf_path)1, RF_0x0, RFREGOFFSETMASK));
						odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);

						if (delay_count < 20) {
							/*============pathA VDF RXIQK Check==============*/
							RX_fail = odm_get_bb_reg(dm, R_0xd00, BIT(11));
							if (RX_fail == 0) {
								odm_write_4byte(dm, 0xcb8, 0x06000000);
								VDF_X[k] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
								odm_write_4byte(dm, 0xcb8, 0x08000000);
								VDF_Y[k] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
								RX0IQKOK = true;
								break;
							} else {
								odm_set_bb_reg(dm, R_0xc10, 0x000003ff, 0x200 >> 1);
								odm_set_bb_reg(dm, R_0xc10, 0x03ff0000, 0x0 >> 1);
								RX0IQKOK = false;
								cal_retry++;
								if (cal_retry == 10)
									break;
							}
						} else {
							RX0IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					}
				}
				RF_DBG(dm, DBG_RF_IQK, "RXA_VDF_cal_retry = %d\n", cal_retry);
				RX_X0[cal] = VDF_X[k - 1] ;
				RX_Y0[cal] = VDF_Y[k - 1];
				odm_set_bb_reg(dm, R_0xce8, BIT(31), 0x1);    /*TX VDF Enable*/
			} else {
				/*====== pathA RX IQK ======*/
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x80000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x30, RFREGOFFSETMASK, 0x30000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x31, RFREGOFFSETMASK, 0x3f7ff);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x32, RFREGOFFSETMASK, 0xfe7bf);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREGOFFSETMASK, 0x88001);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x65, RFREGOFFSETMASK, 0x931d0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x00000);
				odm_set_bb_reg(dm, R_0x978, 0x03FF8000, (TX_X0[cal]) >> 21 & 0x000007ff);
				odm_set_bb_reg(dm, R_0x978, 0x000007FF, (TX_Y0[cal]) >> 21 & 0x000007ff);
				odm_set_bb_reg(dm, R_0x978, BIT(31), 0x1);
				odm_set_bb_reg(dm, R_0x97c, BIT(31), 0x0);
				odm_write_4byte(dm, 0x90c, 0x00008000);
				odm_write_4byte(dm, 0x984, 0x0046a911);

				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
				odm_write_4byte(dm, 0xc80, 0x38008c10);
				odm_write_4byte(dm, 0xc84, 0x18008c10);
				odm_write_4byte(dm, 0xc88, 0x02140119);
				odm_write_4byte(dm, 0xc8c, 0x28161420);
				odm_write_4byte(dm, 0xcb8, 0x00100000);
				cal_retry = 0;
				while (1) {
					/*one shot*/
					odm_write_4byte(dm, 0x980, 0xfa000000);
					odm_write_4byte(dm, 0x980, 0xf8000000);
					delay_ms(10);
					odm_write_4byte(dm, 0xcb8, 0x00000000);
					delay_count = 0;
					while (1) {
						IQK_ready = odm_get_bb_reg(dm, R_0xd00, BIT(10));
						if (IQK_ready || (delay_count > 20))
							break;
						else {
							delay_ms(1);
							delay_count++;
						}
					}

					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
					RF_DBG(dm, DBG_RF_IQK, "==== A: path A RF0 = 0x%x ====\n",
						odm_get_rf_reg(dm, (enum rf_path)0, RF_0x0, RFREGOFFSETMASK));
					RF_DBG(dm, DBG_RF_IQK, "==== A: path B RF0 = 0x%x ====\n",
						odm_get_rf_reg(dm, (enum rf_path)1, RF_0x0, RFREGOFFSETMASK));
					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);

					if (delay_count < 20) {
						/*============pathA RXIQK Check==============*/
						RX_fail = odm_get_bb_reg(dm, R_0xd00, BIT(11));
						if (RX_fail == 0) {
							odm_write_4byte(dm, 0xcb8, 0x06000000);
							RX_X0[cal] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
							odm_write_4byte(dm, 0xcb8, 0x08000000);
							RX_Y0[cal] = odm_get_bb_reg(dm, R_0xd00, 0x07ff0000) << 21;
							RX0IQKOK = true;
#if 0
							odm_write_4byte(dm, 0xcb8, 0x05000000);
							reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
							odm_write_4byte(dm, 0xcb8, 0x06000000);
							reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
							image_power = (reg2 << 32) + reg1;
							RF_DBG(dm, DBG_RF_IQK, "Before PW = %d\n", image_power);
							odm_write_4byte(dm, 0xcb8, 0x07000000);
							reg1 = odm_get_bb_reg(dm, R_0xd00, 0xffffffff);
							odm_write_4byte(dm, 0xcb8, 0x08000000);
							reg2 = odm_get_bb_reg(dm, R_0xd00, 0x0000001f);
							image_power = (reg2 << 32) + reg1;
							RF_DBG(dm, DBG_RF_IQK, "After PW = %d\n", image_power);
#endif
							break;
						} else {
							odm_set_bb_reg(dm, R_0xc10, 0x000003ff, 0x200 >> 1);
							odm_set_bb_reg(dm, R_0xc10, 0x03ff0000, 0x0 >> 1);
							RX0IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					} else {
						RX0IQKOK = false;
						cal_retry++;
						if (cal_retry == 10)
							break;
					}
				}
				RF_DBG(dm, DBG_RF_IQK, "RXA_cal_retry = %d\n", cal_retry);
			}
			if (TX0IQKOK)
				tx_average++;
			if (RX0IQKOK)
				rx_average++;
		}
		break;

		case RF_PATH_B:
		{
			/*path-B TX/RX IQK*/
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x80002);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x30, RFREGOFFSETMASK, 0x20000);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x31, RFREGOFFSETMASK, 0x3fffd);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x32, RFREGOFFSETMASK, 0xfe83f);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x65, RFREGOFFSETMASK, 0x931d5);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREGOFFSETMASK, 0x8a001);
			odm_write_4byte(dm, 0x90c, 0x00008000);
			odm_write_4byte(dm, 0xb00, 0x03000100);
			odm_set_bb_reg(dm, R_0xe94, BIT(0), 0x1);
			odm_write_4byte(dm, 0x978, 0x29002000);/*TX (X,Y)*/
			odm_write_4byte(dm, 0x97c, 0xa9002000);/*RX (X,Y)*/
			odm_write_4byte(dm, 0x984, 0x00462910);/*[0]:AGC_en, [15]:idac_K_Mask*/

			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
			if (dm->ext_pa)
				odm_write_4byte(dm, 0xe88, 0x821403e3);
			else
				odm_write_4byte(dm, 0xe88, 0x821403f1);

			if (*dm->band_type == ODM_BAND_5G)
				odm_write_4byte(dm, 0xe8c, 0x68163e96);
			else
				odm_write_4byte(dm, 0xe8c, 0x28163e96);

			if (VDF_enable == 1) {
				/*============pathB VDF TXIQK==============*/
				for (k = 0; k <= 2; k++) {
					switch (k) {
					case 0:
						/*one shot*/
						odm_write_4byte(dm, 0xe80, 0x18008c38);/*TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16*/
						odm_write_4byte(dm, 0xe84, 0x38008c38);/*RX_Tone_idx[9:0], RxK_Mask[29]*/
						odm_write_4byte(dm, 0x984, 0x00462910);
						odm_set_bb_reg(dm, R_0xee8, BIT(31), 0x0);
						break;
					case 1:
						odm_set_bb_reg(dm, R_0xe80, BIT(28), 0x0);
						odm_set_bb_reg(dm, R_0xe84, BIT(28), 0x0);
						odm_write_4byte(dm, 0x984, 0x0046a910);
						odm_set_bb_reg(dm, R_0xee8, BIT(31), 0x0);
						break;
					case 2:
						RF_DBG(dm, DBG_RF_IQK, "VDF_Y[1] = %x;;;VDF_Y[0] = %x\n", VDF_Y[1] >> 21 & 0x00007ff, VDF_Y[0] >> 21 & 0x00007ff);
						RF_DBG(dm, DBG_RF_IQK, "VDF_X[1] = %x;;;VDF_X[0] = %x\n", VDF_X[1] >> 21 & 0x00007ff, VDF_X[0] >> 21 & 0x00007ff);
						tx_dt[cal] = (VDF_Y[1] >> 20) - (VDF_Y[0] >> 20);
						RF_DBG(dm, DBG_RF_IQK, "tx_dt = %d\n", tx_dt[cal]);
						tx_dt[cal] = ((16 * tx_dt[cal]) * 10000 / 15708);
						tx_dt[cal] = (tx_dt[cal] >> 1) + (tx_dt[cal] & BIT(0));
						odm_write_4byte(dm, 0xe80, 0x18008c20);
						odm_write_4byte(dm, 0xe84, 0x38008c20);
						odm_set_bb_reg(dm, R_0xee8, BIT(31), 0x1);
						odm_set_bb_reg(dm, R_0xee8, 0x3fff0000, tx_dt[cal] & 0x00003fff);
						break;
					default:
						break;
					}

					odm_write_4byte(dm, 0xeb8, 0x00100000);
					cal_retry = 0;
					while (1) {
						/*one shot*/
						odm_write_4byte(dm, 0x980, 0xfa000000);
						odm_write_4byte(dm, 0x980, 0xf8000000);
						delay_ms(10);
						odm_write_4byte(dm, 0xeb8, 0x00000000);
						delay_count = 0;
						while (1) {
							IQK_ready = odm_get_bb_reg(dm, R_0xd40, BIT(10));
							if (IQK_ready || (delay_count > 20))
								break;
							else {
								delay_ms(1);
								delay_count++;
							}
						}

						if (delay_count < 20) {
							/*============pathB VDF TXIQK Check==============*/
							TX_fail = odm_get_bb_reg(dm, R_0xd40, BIT(12));

							if (~TX_fail) {
								odm_write_4byte(dm, 0xeb8, 0x02000000);
								VDF_X[k] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
								odm_write_4byte(dm, 0xeb8, 0x04000000);
								VDF_Y[k] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
								TX1IQKOK = true;
								break;
							} else {
								TX1IQKOK = false;
								cal_retry++;
								if (cal_retry == 10)
									break;
							}
						} else {
							TX1IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					}
				}
				RF_DBG(dm, DBG_RF_IQK, "TXB_VDF_cal_retry = %d\n", cal_retry);
				TX_X1[cal] = VDF_X[k - 1] ;
				TX_Y1[cal] = VDF_Y[k - 1];
			} else {
				/*============pathB TXIQK==============*/
				odm_write_4byte(dm, 0xe80, 0x18008c10);
				odm_write_4byte(dm, 0xe84, 0x38008c10);
				odm_write_4byte(dm, 0xee8, 0x00000000);
				odm_write_4byte(dm, 0xeb8, 0x00100000);
				cal_retry = 0;
				while (1) {
					/*one shot*/
					odm_write_4byte(dm, 0x980, 0xfa000000);
					odm_write_4byte(dm, 0x980, 0xf8000000);
					delay_ms(10);
					odm_write_4byte(dm, 0xeb8, 0x00000000);
					delay_count = 0;
					while (1) {
						IQK_ready = odm_get_bb_reg(dm, R_0xd40, BIT(10));
						if (IQK_ready || (delay_count > 20))
							break;
						delay_ms(1);
						delay_count++;
					}

					if (delay_count < 20) {
						/*============pathB TXIQK Check==============*/
						TX_fail = odm_get_bb_reg(dm, R_0xd40, BIT(12));
						if (~TX_fail) {
							odm_write_4byte(dm, 0xeb8, 0x02000000);
							TX_X1[cal] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
							odm_write_4byte(dm, 0xeb8, 0x04000000);
							TX_Y1[cal] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
							TX1IQKOK = true;
#if 0
							int			reg1 = 0, reg2 = 0, image_power = 0;
							odm_write_4byte(dm, 0xeb8, 0x01000000);
							reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
							odm_write_4byte(dm, 0xeb8, 0x02000000);
							reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
							image_power = (reg2 << 32) + reg1;
							dbg_print("Before PW = %d\n", image_power);
							odm_write_4byte(dm, 0xeb8, 0x03000000);
							reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
							odm_write_4byte(dm, 0xeb8, 0x04000000);
							reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
							image_power = (reg2 << 32) + reg1;
							dbg_print("After PW = %d\n", image_power);
#endif
							break;
						} else {
							TX1IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					} else {
						TX1IQKOK = false;
						cal_retry++;
						if (cal_retry == 10)
							break;
					}
				}
				RF_DBG(dm, DBG_RF_IQK, "TXB_cal_retry = %d\n", cal_retry);
			}

			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x58, 0x7fe00, odm_get_rf_reg(dm, (enum rf_path)path, RF_0x8, 0xffc00));
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);

			if (TX1IQKOK == false)
				break;


			/*======pathB VDF RX IQK ======*/
			if (VDF_enable == 1) {
				odm_set_bb_reg(dm, R_0xee8, BIT(31), 0x0);/*TX VDF Disable*/
				RF_DBG(dm, DBG_RF_IQK, "RXVDF Start\n");

				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x80000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x30, RFREGOFFSETMASK, 0x30000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x31, RFREGOFFSETMASK, 0x3f7ff);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x32, RFREGOFFSETMASK, 0xfe7bf);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREGOFFSETMASK, 0x88001);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x65, RFREGOFFSETMASK, 0x931d0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x00000);

				odm_set_bb_reg(dm, R_0x978, BIT(31), 0x1);
				odm_set_bb_reg(dm, R_0x97c, BIT(31), 0x0);
				odm_write_4byte(dm, 0x984, 0x0046a911);
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
				odm_write_4byte(dm, 0xe88, 0x02140119);
				odm_write_4byte(dm, 0xe8c, 0x28161420);

				for (k = 0; k <= 2; k++) {
					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
					odm_set_bb_reg(dm, R_0x978, 0x03FF8000, (VDF_X[k]) >> 21 & 0x000007ff);
					odm_set_bb_reg(dm, R_0x978, 0x000007FF, (VDF_Y[k]) >> 21 & 0x000007ff);
					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);

					switch (k) {
					case 0:
						odm_write_4byte(dm, 0xe80, 0x38008c38);/*TX_Tone_idx[9:0], TxK_Mask[29] TX_Tone = 16*/
						odm_write_4byte(dm, 0xe84, 0x18008c38);/*RX_Tone_idx[9:0], RxK_Mask[29]*/
						odm_set_bb_reg(dm, R_0xee8, BIT(30), 0x0);
						break;
					case 1:
						odm_write_4byte(dm, 0xe80, 0x28008c38);
						odm_write_4byte(dm, 0xe84, 0x08008c38);
						odm_set_bb_reg(dm, R_0xee8, BIT(30), 0x0);
						break;
					case 2:
						RF_DBG(dm, DBG_RF_IQK, "VDF_Y[1] = %x;;;VDF_Y[0] = %x\n", VDF_Y[1] >> 21 & 0x00007ff, VDF_Y[0] >> 21 & 0x00007ff);
						RF_DBG(dm, DBG_RF_IQK, "VDF_X[1] = %x;;;VDF_X[0] = %x\n", VDF_X[1] >> 21 & 0x00007ff, VDF_X[0] >> 21 & 0x00007ff);
						rx_dt[cal] = (VDF_Y[1] >> 20) - (VDF_Y[0] >> 20);
						RF_DBG(dm, DBG_RF_IQK, "rx_dt = %d\n", rx_dt[cal]);
						rx_dt[cal] = ((16 * rx_dt[cal]) * 10000 / 13823);
						rx_dt[cal] = (rx_dt[cal] >> 1) + (rx_dt[cal] & BIT(0));
						odm_write_4byte(dm, 0xe80, 0x38008c20);
						odm_write_4byte(dm, 0xe84, 0x18008c20);
						odm_set_bb_reg(dm, R_0xee8, 0x00003fff, rx_dt[cal] & 0x00003fff);
						break;
					default:
						break;
					}


					if (k == 2)
						odm_set_bb_reg(dm, R_0xee8, BIT(30), 0x1);  /*RX VDF Enable*/

					odm_write_4byte(dm, 0xeb8, 0x00100000);

					cal_retry = 0;
					while (1) {
						/*one shot*/
						odm_write_4byte(dm, 0x980, 0xfa000000);
						odm_write_4byte(dm, 0x980, 0xf8000000);
						delay_ms(10);
						odm_write_4byte(dm, 0xeb8, 0x00000000);
						delay_count = 0;
						while (1) {
							IQK_ready = odm_get_bb_reg(dm, R_0xd40, BIT(10));
							if (IQK_ready || (delay_count > 20))
								break;
							delay_ms(1);
							delay_count++;
						}

						odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
						RF_DBG(dm, DBG_RF_IQK, "==== B VDF: path A RF0 = 0x%x ====\n",
							odm_get_rf_reg(dm, (enum rf_path)0, RF_0x0, RFREGOFFSETMASK));
						RF_DBG(dm, DBG_RF_IQK, "==== B VDF: path B RF0 = 0x%x ====\n",
							odm_get_rf_reg(dm, (enum rf_path)1, RF_0x0, RFREGOFFSETMASK));
						odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);

						if (delay_count < 20) {
							/*============pathB VDF RXIQK Check==============*/
							RX_fail = odm_get_bb_reg(dm, R_0xd40, BIT(11));
							if (RX_fail == 0) {
								odm_write_4byte(dm, 0xeb8, 0x06000000);
								VDF_X[k] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
								odm_write_4byte(dm, 0xeb8, 0x08000000);
								VDF_Y[k] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
								RX1IQKOK = true;
								break;
							} else {
								odm_set_bb_reg(dm, R_0xe10, 0x000003ff, 0x200 >> 1);
								odm_set_bb_reg(dm, R_0xe10, 0x03ff0000, 0x0 >> 1);
								RX1IQKOK = false;
								cal_retry++;
								if (cal_retry == 10)
									break;
							}
						} else {
							RX1IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					}
				}
				RF_DBG(dm, DBG_RF_IQK, "RXB_VDF_cal_retry = %d\n", cal_retry);
				RX_X1[cal] = VDF_X[k - 1] ;
				RX_Y1[cal] = VDF_Y[k - 1];
				odm_set_bb_reg(dm, R_0xee8, BIT(31), 0x1);    /*TX VDF Enable*/
			} else {
				/*============pathB RXIQK==============*/
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x80000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x30, RFREGOFFSETMASK, 0x30000);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x31, RFREGOFFSETMASK, 0x3f7ff);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x32, RFREGOFFSETMASK, 0xfe7bf);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8f, RFREGOFFSETMASK, 0x88001);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0x65, RFREGOFFSETMASK, 0x931d0);
				odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, RFREGOFFSETMASK, 0x00000);

				odm_set_bb_reg(dm, R_0x978, 0x03FF8000, (TX_X1[cal]) >> 21 & 0x000007ff);
				odm_set_bb_reg(dm, R_0x978, 0x000007FF, (TX_Y1[cal]) >> 21 & 0x000007ff);
				odm_set_bb_reg(dm, R_0x978, BIT(31), 0x1);
				odm_set_bb_reg(dm, R_0x97c, BIT(31), 0x0);
				odm_write_4byte(dm, 0x90c, 0x00008000);
				odm_write_4byte(dm, 0x984, 0x0046a911);

				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
				odm_write_4byte(dm, 0xe80, 0x38008c15);
				odm_write_4byte(dm, 0xe84, 0x18008c15);
				odm_write_4byte(dm, 0xe88, 0x02140119);
				odm_write_4byte(dm, 0xe8c, 0x28161420);
				odm_write_4byte(dm, 0xeb8, 0x00100000);

				cal_retry = 0;
				while (1) {
					/*one shot*/
					odm_write_4byte(dm, 0x980, 0xfa000000);
					odm_write_4byte(dm, 0x980, 0xf8000000);
					delay_ms(10);
					odm_write_4byte(dm, 0xeb8, 0x00000000);
					delay_count = 0;
					while (1) {
						IQK_ready = odm_get_bb_reg(dm, R_0xd40, BIT(10));
						if (IQK_ready || (delay_count > 20))
							break;
						delay_ms(1);
						delay_count++;
					}

					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
					RF_DBG(dm, DBG_RF_IQK, "==== B: path A RF0 = 0x%x ====\n",
						odm_get_rf_reg(dm, (enum rf_path)0, RF_0x0, RFREGOFFSETMASK));
					RF_DBG(dm, DBG_RF_IQK, "==== B: path B RF0 = 0x%x ====\n",
						odm_get_rf_reg(dm, (enum rf_path)1, RF_0x0, RFREGOFFSETMASK));
					odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);

					if (delay_count < 20) {
						/*============pathB RXIQK Check==============*/
						RX_fail = odm_get_bb_reg(dm, R_0xd40, BIT(11));
						if (RX_fail == 0) {
							odm_write_4byte(dm, 0xeb8, 0x06000000);
							RX_X1[cal] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
							odm_write_4byte(dm, 0xeb8, 0x08000000);
							RX_Y1[cal] = odm_get_bb_reg(dm, R_0xd40, 0x07ff0000) << 21;
							RX1IQKOK = true;
							break;
						} else {
							odm_set_bb_reg(dm, R_0xe10, 0x000003ff, 0x200 >> 1);
							odm_set_bb_reg(dm, R_0xe10, 0x03ff0000, 0x0 >> 1);
							RX1IQKOK = false;
							cal_retry++;
							if (cal_retry == 10)
								break;
						}
					} else {
						RX1IQKOK = false;
						cal_retry++;
						if (cal_retry == 10)
							break;
					}
				}
#if 0
				odm_write_4byte(dm, 0xeb8, 0x05000000);
				reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
				odm_write_4byte(dm, 0xeb8, 0x06000000);
				reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
				image_power = (reg2 << 32) + reg1;
				RF_DBG(dm, DBG_RF_IQK, "Before PW = %d\n", image_power);

				odm_write_4byte(dm, 0xeb8, 0x07000000);
				reg1 = odm_get_bb_reg(dm, R_0xd40, 0xffffffff);
				odm_write_4byte(dm, 0xeb8, 0x08000000);
				reg2 = odm_get_bb_reg(dm, R_0xd40, 0x0000001f);
				image_power = (reg2 << 32) + reg1;
				RF_DBG(dm, DBG_RF_IQK, "After PW = %d\n", image_power);
#endif

				RF_DBG(dm, DBG_RF_IQK, "RXB_cal_retry = %d\n", cal_retry);
			}
			if (RX1IQKOK)
				rx_average++;
			if (TX1IQKOK)
				tx_average++;
		}
		break;
		default:
			break;
		}
		cal++;
	}
	/*FillIQK Result*/
	switch (path) {
	case RF_PATH_A:
		RF_DBG(dm, DBG_RF_IQK, "========Path_A =======\n");
		if (tx_average == 0) {
			_iqk_tx_fill_iqc_8812a(dm, path, 0x200, 0x0);
			break;
		}

		for (i = 0; i < tx_average; i++)
			RF_DBG(dm, DBG_RF_IQK, "TX_X0[%d] = %x ;; TX_Y0[%d] = %x\n", i, (TX_X0[i]) >> 21 & 0x000007ff, i, (TX_Y0[i]) >> 21 & 0x000007ff);

		for (i = 0; i < tx_average; i++) {
			for (ii = i + 1; ii < tx_average; ii++) {
				dx = (TX_X0[i] >> 21) - (TX_X0[ii] >> 21);
				if (dx < 4 && dx > -4) {
					dy = (TX_Y0[i] >> 21) - (TX_Y0[ii] >> 21);
					if (dy < 4 && dy > -4) {
						TX_X = ((TX_X0[i] >> 21) + (TX_X0[ii] >> 21)) / 2;
						TX_Y = ((TX_Y0[i] >> 21) + (TX_Y0[ii] >> 21)) / 2;
						if (*dm->band_width == 2)
							tx_dt[0] = (tx_dt[i] + tx_dt[ii]) / 2;
						TX_finish = 1;
						break;
					}
				}
			}
			if (TX_finish == 1)
				break;
		}

		if (*dm->band_width == 2) {
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
			odm_set_bb_reg(dm, R_0xce8, 0x3fff0000, tx_dt[0] & 0x00003fff);
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
		}
		if (TX_finish == 1)
			_iqk_tx_fill_iqc_8812a(dm, path, TX_X, TX_Y);
		else
			_iqk_tx_fill_iqc_8812a(dm, path, 0x200, 0x0);

		if (rx_average == 0) {
			_iqk_rx_fill_iqc_8812a(dm, path, 0x200, 0x0);
			break;
		}

		for (i = 0; i < rx_average; i++)
			RF_DBG(dm, DBG_RF_IQK, "RX_X0[%d] = %x ;; RX_Y0[%d] = %x\n", i, (RX_X0[i]) >> 21 & 0x000007ff, i, (RX_Y0[i]) >> 21 & 0x000007ff);

		for (i = 0; i < rx_average; i++) {
			for (ii = i + 1; ii < rx_average; ii++) {
				dx = (RX_X0[i] >> 21) - (RX_X0[ii] >> 21);
				if (dx < 4 && dx > -4) {
					dy = (RX_Y0[i] >> 21) - (RX_Y0[ii] >> 21);
					if (dy < 4 && dy > -4) {
						RX_X = ((RX_X0[i] >> 21) + (RX_X0[ii] >> 21)) / 2;
						RX_Y = ((RX_Y0[i] >> 21) + (RX_Y0[ii] >> 21)) / 2;
						if (*dm->band_width == 2)
							rx_dt[0] = (rx_dt[i] + rx_dt[ii]) / 2;
						RX_finish = 1;
						break;
					}
				}
			}
			if (RX_finish == 1)
				break;
		}

		if (*dm->band_width == 2) {
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
			odm_set_bb_reg(dm, R_0xce8, 0x00003fff, rx_dt[0] & 0x00003fff);
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
		}

		if (RX_finish == 1)
			_iqk_rx_fill_iqc_8812a(dm, path, RX_X, RX_Y);
		else
			_iqk_rx_fill_iqc_8812a(dm, path, 0x200, 0x0);

		if (TX_finish && RX_finish) {
			cali_info->is_need_iqk = false;
			cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][0] = ((TX_X & 0x000007ff) << 16) + (TX_Y & 0x000007ff);	/* path A TX */
			cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][1] = ((RX_X & 0x000007ff) << 16) + (RX_Y & 0x000007ff);	/* path A RX */

			if (*dm->band_width == 2) {
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
				cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][4] = odm_read_4byte(dm, 0xce8);	/* path B VDF */
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
			}
		}
		break;
	case RF_PATH_B:
		RF_DBG(dm, DBG_RF_IQK, "========Path_B =======\n");
		if (tx_average == 0) {
			_iqk_tx_fill_iqc_8812a(dm, path, 0x200, 0x0);
			break;
		}

		for (i = 0; i < tx_average; i++)
			RF_DBG(dm, DBG_RF_IQK, "TX_X1[%d] = %x ;; TX_Y1[%d] = %x\n", i, (TX_X1[i]) >> 21 & 0x000007ff, i, (TX_Y1[i]) >> 21 & 0x000007ff);

		for (i = 0; i < tx_average; i++) {
			for (ii = i + 1; ii < tx_average; ii++) {
				dx = (TX_X1[i] >> 21) - (TX_X1[ii] >> 21);
				if (dx < 4 && dx > -4) {
					dy = (TX_Y1[i] >> 21) - (TX_Y1[ii] >> 21);
					if (dy < 4 && dy > -4) {
						TX_X = ((TX_X1[i] >> 21) + (TX_X1[ii] >> 21)) / 2;
						TX_Y = ((TX_Y1[i] >> 21) + (TX_Y1[ii] >> 21)) / 2;
						if (*dm->band_width == 2)
							tx_dt[0] = (tx_dt[i] + tx_dt[ii]) / 2;
						TX_finish = 1;
						break;
					}
				}
			}
			if (TX_finish == 1)
				break;
		}

		if (*dm->band_width == 2) {
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
			odm_set_bb_reg(dm, R_0xee8, 0x3fff0000, tx_dt[0] & 0x00003fff);
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
		}

		if (TX_finish == 1)
			_iqk_tx_fill_iqc_8812a(dm, path, TX_X, TX_Y);
		else
			_iqk_tx_fill_iqc_8812a(dm, path, 0x200, 0x0);

		if (rx_average == 0) {
			_iqk_rx_fill_iqc_8812a(dm, path, 0x200, 0x0);
			break;
		}

		for (i = 0; i < rx_average; i++)
			RF_DBG(dm, DBG_RF_IQK, "RX_X1[%d] = %x ;; RX_Y1[%d] = %x\n", i, (RX_X1[i]) >> 21 & 0x000007ff, i, (RX_Y1[i]) >> 21 & 0x000007ff);

		for (i = 0; i < rx_average; i++) {
			for (ii = i + 1; ii < rx_average; ii++) {
				dx = (RX_X1[i] >> 21) - (RX_X1[ii] >> 21);
				if (dx < 4 && dx > -4) {
					dy = (RX_Y1[i] >> 21) - (RX_Y1[ii] >> 21);
					if (dy < 4 && dy > -4) {
						RX_X = ((RX_X1[i] >> 21) + (RX_X1[ii] >> 21)) / 2;
						RX_Y = ((RX_Y1[i] >> 21) + (RX_Y1[ii] >> 21)) / 2;
						if (*dm->band_width == 2)
							rx_dt[0] = (rx_dt[i] + rx_dt[ii]) / 2;
						RX_finish = 1;
						break;
					}
				}
			}
			if (RX_finish == 1)
				break;
		}

		if (*dm->band_width == 2) {
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
			odm_set_bb_reg(dm, R_0xee8, 0x00003fff, rx_dt[0] & 0x00003fff);
			odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
		}

		if (RX_finish == 1)
			_iqk_rx_fill_iqc_8812a(dm, path, RX_X, RX_Y);
		else
			_iqk_rx_fill_iqc_8812a(dm, path, 0x200, 0x0);

		if (TX_finish && RX_finish) {
			cali_info->is_need_iqk = false;
			cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][2] = ((TX_X & 0x000007ff) << 16) + (TX_Y & 0x000007ff);	/* path B TX */
			cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][3] = ((RX_X & 0x000007ff) << 16) + (RX_Y & 0x000007ff);	/* path B RX */

			if (*dm->band_width == 2) {
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x1);
				cali_info->iqk_matrix_reg_setting[chnl_idx].value[*dm->band_width][5] = odm_read_4byte(dm, 0xee8);	/* path B VDF */
				odm_set_bb_reg(dm, R_0x82c, BIT(31), 0x0);
			}
		}
		break;
	default:
		break;
	}

	if (!TX_finish && !RX_finish)
		priv->pshare->IQK_fail_cnt++;

#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	if (!TX0IQKOK)
		panic_printk("[IQK] please check S0 TXIQK\n");
	if (!RX0IQKOK)
		panic_printk("[IQK] please check S0 RXIQK\n");
	if (!TX1IQKOK)
		panic_printk("[IQK] please check S1 TXIQK\n");
	if (!RX1IQKOK)
		panic_printk("[IQK] please check S1 RXIQK\n");
#endif
}


#define MACBB_REG_NUM 9
#define AFE_REG_NUM 14
#define RF_REG_NUM 3

/*IQK: v1.1*/
/*1.remove 0x8c4 setting*/
/*2.add IQK debug message*/
void
_phy_iq_calibrate_8812a(
	struct dm_struct		*dm,
	u8		channel
)
{
	u32	MACBB_backup[MACBB_REG_NUM], AFE_backup[AFE_REG_NUM], RFA_backup[RF_REG_NUM], RFB_backup[RF_REG_NUM];
	u32	backup_macbb_reg[MACBB_REG_NUM] = {0x520, 0x550, 0x808, 0x838, 0x90c, 0xb00, 0xc00, 0xe00, 0x82c};
	u32	backup_afe_reg[AFE_REG_NUM] = {0xc5c, 0xc60, 0xc64, 0xc68, 0xcb8, 0xcb0, 0xcb4, 0xe5c, 0xe60, 0xe64,
					       0xe68, 0xeb8, 0xeb0, 0xeb4
					  };
	u32	backup_rf_reg[RF_REG_NUM] = {0x65, 0x8f, 0x0};
	u8	chnl_idx = odm_get_right_chnl_place_for_iqk(channel);

	_iqk_backup_mac_bb_8812a(dm, MACBB_backup, backup_macbb_reg, MACBB_REG_NUM);
	_iqk_backup_afe_8812a(dm, AFE_backup, backup_afe_reg, AFE_REG_NUM);
	_iqk_backup_rf_8812a(dm, RFA_backup, RFB_backup, backup_rf_reg, RF_REG_NUM);

	_iqk_configure_mac_8812a(dm);
	_iqk_tx_8812a(dm, RF_PATH_A, chnl_idx);
	_iqk_restore_rf_8812a(dm, RF_PATH_A, backup_rf_reg, RFA_backup, RF_REG_NUM);

	_iqk_tx_8812a(dm, RF_PATH_B, chnl_idx);
	_iqk_restore_rf_8812a(dm, RF_PATH_B, backup_rf_reg, RFB_backup, RF_REG_NUM);

	_iqk_restore_afe_8812a(dm, AFE_backup, backup_afe_reg, AFE_REG_NUM);
	_iqk_restore_mac_bb_8812a(dm, MACBB_backup, backup_macbb_reg, MACBB_REG_NUM);

	/* _IQK_Exit_8812A(dm); */
	/* _IQK_TX_CheckResult_8812A */
}


void
_phy_lc_calibrate_8812a(
	struct dm_struct		*dm,
	boolean		is2T
)
{
	u8	tmp_reg;
	u32	rf_amode = 0, rf_bmode = 0, lc_cal;
	/* Check continuous TX and Packet TX */
	tmp_reg = odm_read_1byte(dm, 0xd03);

	if ((tmp_reg & 0x70) != 0)			/* Deal with contisuous TX case */
		odm_write_1byte(dm, 0xd03, tmp_reg & 0x8F);	/* disable all continuous TX */
	else							/* Deal with Packet TX case */
		odm_write_1byte(dm, REG_TXPAUSE, 0xFF);			/* block all queues */

	if ((tmp_reg & 0x70) != 0) {
		/* 1. Read original RF mode */
		/* path-A */
		rf_amode = odm_get_rf_reg(dm, RF_PATH_A, RF_AC, MASK12BITS);

		/* path-B */
		if (is2T)
			rf_bmode = odm_get_rf_reg(dm, RF_PATH_B, RF_AC, MASK12BITS);

		/* 2. Set RF mode = standby mode */
		/* path-A */
		odm_set_rf_reg(dm, RF_PATH_A, RF_AC, MASK12BITS, (rf_amode & 0x8FFFF) | 0x10000);

		/* path-B */
		if (is2T)
			odm_set_rf_reg(dm, RF_PATH_B, RF_AC, MASK12BITS, (rf_bmode & 0x8FFFF) | 0x10000);
	}

	/* 3. Read RF reg18 */
	lc_cal = odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, MASK12BITS);

	/* 4. Set LC calibration begin	bit15 */
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, MASK12BITS, lc_cal | 0x08000);

	ODM_delay_ms(100);


	/* Restore original situation */
	if ((tmp_reg & 0x70) != 0) {	/* Deal with contisuous TX case */
		/* path-A */
		odm_write_1byte(dm, 0xd03, tmp_reg);
		odm_set_rf_reg(dm, RF_PATH_A, RF_AC, MASK12BITS, rf_amode);

		/* path-B */
		if (is2T)
			odm_set_rf_reg(dm, RF_PATH_B, RF_AC, MASK12BITS, rf_bmode);
	} else /* Deal with Packet TX case */
		odm_write_1byte(dm, REG_TXPAUSE, 0x00);
}


#if 1 /* FOR_8812_IQK */
void
phy_iq_calibrate_8812a(
	void		*dm_void,
	boolean	is_recovery
)
{
	struct dm_struct	*dm = (struct dm_struct *)dm_void;
	
	_phy_iq_calibrate_8812a(dm, *dm->channel);
}
#endif


void
phy_lc_calibrate_8812a(
	void		*dm_void
)
{
	struct dm_struct		*dm = (struct dm_struct *)dm_void;

	_phy_lc_calibrate_8812a(dm, true);
}


void _phy_set_rf_path_switch_8812a(
	struct dm_struct		*dm,
	boolean		is_main,
	boolean		is2T
)
{
	if (is2T) {	/* 92C */
		if (is_main)
			odm_set_bb_reg(dm, REG_FPGA0_XB_RF_INTERFACE_OE, BIT(5) | BIT(6), 0x1);	/* 92C_Path_A */
		else
			odm_set_bb_reg(dm, REG_FPGA0_XB_RF_INTERFACE_OE, BIT(5) | BIT(6), 0x2);	/* BT */
	} else {		/* 88C */

		if (is_main)
			odm_set_bb_reg(dm, REG_FPGA0_XA_RF_INTERFACE_OE, BIT(8) | BIT(9), 0x2);	/* Main */
		else
			odm_set_bb_reg(dm, REG_FPGA0_XA_RF_INTERFACE_OE, BIT(8) | BIT(9), 0x1);	/* Aux */
	}
}


void phy_set_rf_path_switch_8812a(
	struct dm_struct		*dm,
	boolean		is_main
)
{
	/* HAL_DATA_TYPE	*hal_data = GET_HAL_DATA(adapter); */

#ifdef DISABLE_BB_RF
	return;
#endif

	{
		/* For 88C 1T1R */
		_phy_set_rf_path_switch_8812a(dm, is_main, false);
	}
}
