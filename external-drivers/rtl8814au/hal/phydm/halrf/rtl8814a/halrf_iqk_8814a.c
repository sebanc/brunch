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
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
#if RT_PLATFORM == PLATFORM_MACOSX
#include "phydm_precomp.h"
#else
#include "../phydm_precomp.h"
#endif
#else
#include "../../phydm_precomp.h"
#endif

#if (RTL8814A_SUPPORT == 1)

/*---------------------------Define Local Constant---------------------------*/

/*---------------------------Define Local Constant---------------------------*/

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
void do_iqk_8814a(void *dm_void, u8 delta_thermal_index, u8 thermal_value,
		  u8 threshold)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	odm_reset_iqk_result(dm);
	dm->rf_calibrate_info.thermal_value_iqk = thermal_value;
	halrf_iqk_trigger(dm, false);
}
#else
/*Originally config->do_iqk is hooked phy_iq_calibrate_8814a, but do_iqk_8814a and phy_iq_calibrate_8814a have different arguments*/
void do_iqk_8814a(void *dm_void, u8 delta_thermal_index, u8 thermal_value,
		  u8 threshold)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	boolean is_recovery = (boolean)delta_thermal_index;

	halrf_iqk_trigger(dm, is_recovery);
}
#endif
/* 1 7.	IQK */

void _iqk_iqk_fail_report_8814a(struct dm_struct *dm)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 i, j;

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 4; j++) {
			if (iqk_info->iqk_fail[i][j])
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK]please check S%d %s\n", j,
				       (i == 0) ? "TXIQK" : "RXIQK");
#else
				panic_printk("[IQK]please check S%d %s\n", j, (i == 0) ? "TXIQK" : "RXIQK");
#endif
		}
	}
}

void _iqk_backup_mac_bb_8814a(struct dm_struct *dm, u32 *MAC_backup,
			      u32 *BB_backup, u32 *backup_mac_reg,
			      u32 *backup_bb_reg)
{
	u32 i;
	/* save MACBB default value */
	for (i = 0; i < MAC_REG_NUM_8814; i++)
		MAC_backup[i] = odm_read_4byte(dm, backup_mac_reg[i]);
	for (i = 0; i < BB_REG_NUM_8814; i++)
		BB_backup[i] = odm_read_4byte(dm, backup_bb_reg[i]);

	RF_DBG(dm, DBG_RF_IQK, "BackupMacBB Success!!!!\n");
}

void _iqk_backup_rf_8814a(struct dm_struct *dm, u32 RF_backup[][4],
			  u32 *backup_rf_reg)
{
	u32 i;
	/* Save RF Parameters */
	for (i = 0; i < RF_REG_NUM_8814; i++) {
		RF_backup[i][RF_PATH_A] = odm_get_rf_reg(dm, RF_PATH_A, backup_rf_reg[i], RFREGOFFSETMASK);
		RF_backup[i][RF_PATH_B] = odm_get_rf_reg(dm, RF_PATH_B, backup_rf_reg[i], RFREGOFFSETMASK);
		RF_backup[i][RF_PATH_C] = odm_get_rf_reg(dm, RF_PATH_C, backup_rf_reg[i], RFREGOFFSETMASK);
		RF_backup[i][RF_PATH_D] = odm_get_rf_reg(dm, RF_PATH_D, backup_rf_reg[i], RFREGOFFSETMASK);
	}

	RF_DBG(dm, DBG_RF_IQK, "BackupRF Success!!!!\n");
}

void _iqk_afe_setting_8814a(struct dm_struct *dm, boolean do_iqk)
{
	if (do_iqk) {
		/* IQK AFE setting RX_WAIT_CCA mode */
		odm_write_4byte(dm, 0xc60, 0x0e808003);
		odm_write_4byte(dm, 0xe60, 0x0e808003);
		odm_write_4byte(dm, 0x1860, 0x0e808003);
		odm_write_4byte(dm, 0x1a60, 0x0e808003);
		odm_set_bb_reg(dm, R_0x90c, BIT(13), 0x1);

		odm_set_bb_reg(dm, R_0x764, BIT(10) | BIT(9), 0x3);
		odm_set_bb_reg(dm, R_0x764, BIT(10) | BIT(9), 0x0);

		odm_set_bb_reg(dm, R_0x804, BIT(2), 0x1);
		odm_set_bb_reg(dm, R_0x804, BIT(2), 0x0);

		RF_DBG(dm, DBG_RF_IQK, "AFE IQK mode Success!!!!\n");
	} else {
		odm_write_4byte(dm, 0xc60, 0x07808003);
		odm_write_4byte(dm, 0xe60, 0x07808003);
		odm_write_4byte(dm, 0x1860, 0x07808003);
		odm_write_4byte(dm, 0x1a60, 0x07808003);
		odm_set_bb_reg(dm, R_0x90c, BIT(13), 0x1);

		odm_set_bb_reg(dm, R_0x764, BIT(10) | BIT(9), 0x3);
		odm_set_bb_reg(dm, R_0x764, BIT(10) | BIT(9), 0x0);

		odm_set_bb_reg(dm, R_0x804, BIT(2), 0x1);
		odm_set_bb_reg(dm, R_0x804, BIT(2), 0x0);

		RF_DBG(dm, DBG_RF_IQK, "AFE Normal mode Success!!!!\n");
	}
}

void _iqk_restore_mac_bb_8814a(struct dm_struct *dm, u32 *MAC_backup,
			       u32 *BB_backup, u32 *backup_mac_reg,
			       u32 *backup_bb_reg)
{
	u32 i;
	/* Reload MacBB Parameters */
	for (i = 0; i < MAC_REG_NUM_8814; i++)
		odm_write_4byte(dm, backup_mac_reg[i], MAC_backup[i]);
	for (i = 0; i < BB_REG_NUM_8814; i++)
		odm_write_4byte(dm, backup_bb_reg[i], BB_backup[i]);
	RF_DBG(dm, DBG_RF_IQK, "RestoreMacBB Success!!!!\n");
}

void _iqk_restore_rf_8814a(struct dm_struct *dm, u32 *backup_rf_reg,
			   u32 RF_backup[][4])
{
	u32 i;

	odm_set_rf_reg(dm, RF_PATH_A, RF_0xef, RFREGOFFSETMASK, 0x0);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0xef, RFREGOFFSETMASK, 0x0);
	odm_set_rf_reg(dm, RF_PATH_C, RF_0xef, RFREGOFFSETMASK, 0x0);
	odm_set_rf_reg(dm, RF_PATH_D, RF_0xef, RFREGOFFSETMASK, 0x0);

	odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, 0x88001);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, 0x88001);
	odm_set_rf_reg(dm, RF_PATH_C, RF_0x8f, RFREGOFFSETMASK, 0x88001);
	odm_set_rf_reg(dm, RF_PATH_D, RF_0x8f, RFREGOFFSETMASK, 0x88001);

	for (i = 0; i < RF_REG_NUM_8814; i++) {
		odm_set_rf_reg(dm, RF_PATH_A, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i][RF_PATH_A]);
		odm_set_rf_reg(dm, RF_PATH_B, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i][RF_PATH_B]);
		odm_set_rf_reg(dm, RF_PATH_C, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i][RF_PATH_C]);
		odm_set_rf_reg(dm, RF_PATH_D, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i][RF_PATH_D]);
	}

	RF_DBG(dm, DBG_RF_IQK, "RestoreRF Success!!!!\n");
}

void phy_reset_iqk_result_8814a(struct dm_struct *dm)
{
	odm_write_4byte(dm, 0x1b00, 0xf8000000);
	odm_write_4byte(dm, 0x1b38, 0x20000000);
	odm_write_4byte(dm, 0x1b00, 0xf8000002);
	odm_write_4byte(dm, 0x1b38, 0x20000000);
	odm_write_4byte(dm, 0x1b00, 0xf8000004);
	odm_write_4byte(dm, 0x1b38, 0x20000000);
	odm_write_4byte(dm, 0x1b00, 0xf8000006);
	odm_write_4byte(dm, 0x1b38, 0x20000000);
	odm_write_4byte(dm, 0xc10, 0x100);
	odm_write_4byte(dm, 0xe10, 0x100);
	odm_write_4byte(dm, 0x1810, 0x100);
	odm_write_4byte(dm, 0x1a10, 0x100);
}

void _iqk_reset_nctl_8814a(struct dm_struct *dm)
{
	odm_write_4byte(dm, 0x1b00, 0xf8000000);
	odm_write_4byte(dm, 0x1b80, 0x00000006);
	odm_write_4byte(dm, 0x1b00, 0xf8000000);
	odm_write_4byte(dm, 0x1b80, 0x00000002);
	RF_DBG(dm, DBG_RF_IQK, "ResetNCTL Success!!!!\n");
}

void _iqk_configure_mac_8814a(struct dm_struct *dm)
{
	/* ========MAC register setting======== */
	odm_write_1byte(dm, 0x522, 0x3f);
	odm_set_bb_reg(dm, R_0x550, BIT(11) | BIT(3), 0x0);
	odm_write_1byte(dm, 0x808, 0x00); /*		RX ante off */
	odm_set_bb_reg(dm, R_0x838, 0xf, 0xe); /*		CCA off */
	odm_set_bb_reg(dm, R_0xa14, BIT(9) | BIT(8), 0x3); /*		CCK RX path off */
	odm_write_4byte(dm, 0xcb0, 0x77777777);
	odm_write_4byte(dm, 0xeb0, 0x77777777);
	odm_write_4byte(dm, 0x18b4, 0x77777777);
	odm_write_4byte(dm, 0x1ab4, 0x77777777);
	odm_set_bb_reg(dm, R_0x1abc, 0x0ff00000, 0x77);
	odm_set_bb_reg(dm, R_0x910, BIT(23) | BIT(22), 0x0);
	/*by YN*/
	odm_set_bb_reg(dm, R_0xcbc, 0xf, 0x0);
}

void _lok_one_shot(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 path = 0, delay_count = 0, ii;
	boolean LOK_notready = false;
	u32 LOK_temp1 = 0, LOK_temp2 = 0;

	RF_DBG(dm, DBG_RF_IQK, "============ LOK ============\n");
	for (path = 0; path <= 3; path++) {
		RF_DBG(dm, DBG_RF_IQK, "==========S%d LOK ==========\n", path);

		odm_set_bb_reg(dm, R_0x9a4, BIT(21) | BIT(20), path); /* ADC Clock source */
		odm_write_4byte(dm, 0x1b00, (0xf8000001 | (1 << (4 + path)))); /* LOK: CMD ID = 0  {0xf8000011, 0xf8000021, 0xf8000041, 0xf8000081} */
		ODM_delay_ms(LOK_delay);
		delay_count = 0;
		LOK_notready = true;

		while (LOK_notready) {
			LOK_notready = (boolean)odm_get_bb_reg(dm, R_0x1b00, BIT(0));
			ODM_delay_ms(1);
			delay_count++;
			if (delay_count >= 10) {
				RF_DBG(dm, DBG_RF_IQK, "S%d LOK timeout!!!\n",
				       path);

				_iqk_reset_nctl_8814a(dm);
				break;
			}
		}
		RF_DBG(dm, DBG_RF_IQK, "S%d ==> delay_count = 0x%x\n", path,
		       delay_count);

		if (!LOK_notready) {
			odm_write_4byte(dm, 0x1b00, 0xf8000000 | (path << 1));
			odm_write_4byte(dm, 0x1bd4, 0x003f0001);
			LOK_temp2 = (odm_get_bb_reg(dm, R_0x1bfc, 0x003e0000) + 0x10) & 0x1f;
			LOK_temp1 = (odm_get_bb_reg(dm, R_0x1bfc, 0x0000003e) + 0x10) & 0x1f;

			for (ii = 1; ii < 5; ii++) {
				LOK_temp1 = LOK_temp1 + ((LOK_temp1 & BIT(4 - ii)) << (ii * 2));
				LOK_temp2 = LOK_temp2 + ((LOK_temp2 & BIT(4 - ii)) << (ii * 2));
			}
			RF_DBG(dm, DBG_RF_IQK,
			       "LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n",
			       LOK_temp1 >> 4, LOK_temp2 >> 4);

			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8, 0x07c00, LOK_temp1 >> 4);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8, 0xf8000, LOK_temp2 >> 4);

			RF_DBG(dm, DBG_RF_IQK, "==>S%d fill LOK\n", path);

		} else {
			RF_DBG(dm, DBG_RF_IQK, "==>S%d LOK Fail!!!\n", path);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x8, RFREGOFFSETMASK, 0x08400);
		}
		iqk_info->lok_fail[path] = LOK_notready;
	}
	RF_DBG(dm, DBG_RF_IQK,
	       "LOK0_notready = %d, LOK1_notready = %d, LOK2_notready = %d, LOK3_notready = %d\n",
	       iqk_info->lok_fail[0], iqk_info->lok_fail[1],
	       iqk_info->lok_fail[2], iqk_info->lok_fail[3]);
}

void _iqk_one_shot(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 path = 0, delay_count = 0, cal_retry = 0, idx;
	boolean notready = true, fail = true;
	u32 IQK_CMD = 0x0;
	u16 iqk_apply[4] = {0xc94, 0xe94, 0x1894, 0x1a94};

	for (idx = 0; idx <= 1; idx++) { /* ii = 0:TXK, 1: RXK */

		if (idx == TX_IQK) {
			RF_DBG(dm, DBG_RF_IQK,
			       "============ WBTXIQK ============\n");
		} else if (idx == RX_IQK) {
			RF_DBG(dm, DBG_RF_IQK,
			       "============ WBRXIQK ============\n");
		}

		for (path = 0; path <= 3; path++) {
			RF_DBG(dm, DBG_RF_IQK, "==========S%d IQK ==========\n",
			       path);
			cal_retry = 0;
			fail = true;
			while (fail) {
				odm_set_bb_reg(dm, R_0x9a4, BIT(21) | BIT(20), path);
				if (idx == TX_IQK) {
					IQK_CMD = (0xf8000001 | (*dm->band_width + 3) << 8 | (1 << (4 + path)));

					RF_DBG(dm, DBG_RF_IQK, "TXK_Trigger = 0x%x\n", IQK_CMD);
					/*{0xf8000311, 0xf8000321, 0xf8000341, 0xf8000381} ==> 20 WBTXK (CMD = 3)*/
					/*{0xf8000411, 0xf8000421, 0xf8000441, 0xf8000481} ==> 40 WBTXK (CMD = 4)*/
					/*{0xf8000511, 0xf8000521, 0xf8000541, 0xf8000581} ==> 80 WBTXK (CMD = 5)*/
				} else if (idx == RX_IQK) {
					if (*dm->band_type == ODM_BAND_2_4G) {
						odm_set_rf_reg(dm, (enum rf_path)path, RF_0xdf, BIT(11), 0x1);
						odm_set_rf_reg(dm, (enum rf_path)path, RF_0x56, 0xfffff, 0x51ce1);
						switch (path) {
						case 0:
						case 1:
							odm_write_4byte(dm, 0xeb0, 0x54775477);
							break;
						case 2:
							odm_write_4byte(dm, 0x18b4, 0x54775477);
							break;
						case 3:
							odm_write_4byte(dm, 0x1abc, 0x75400000);
							odm_write_4byte(dm, 0x1ab4, 0x77777777);
							break;
						}
					}
					IQK_CMD = (0xf8000001 | (9 - *dm->band_width) << 8 | (1 << (4 + path)));
					RF_DBG(dm, DBG_RF_IQK, "TXK_Trigger = 0x%x\n", IQK_CMD);
					/*{0xf8000911, 0xf8000921, 0xf8000941, 0xf8000981} ==> 20 WBRXK (CMD = 9)*/
					/*{0xf8000811, 0xf8000821, 0xf8000841, 0xf8000881} ==> 40 WBRXK (CMD = 8)*/
					/*{0xf8000711, 0xf8000721, 0xf8000741, 0xf8000781} ==> 80 WBRXK (CMD = 7)*/
				}

				odm_write_4byte(dm, 0x1b00, IQK_CMD);
				ODM_delay_ms(WBIQK_delay);

				delay_count = 0;
				notready = true;
				while (notready) {
					notready = (boolean)odm_get_bb_reg(dm, R_0x1b00, BIT(0));
					if (!notready) {
						fail = (boolean)odm_get_bb_reg(dm, R_0x1b08, BIT(26));
						break;
					}
					ODM_delay_ms(1);
					delay_count++;
					if (delay_count >= 20) {
						RF_DBG(dm, DBG_RF_IQK, "S%d IQK timeout!!!\n", path);
						_iqk_reset_nctl_8814a(dm);
						break;
					}
				}
				if (fail)
					cal_retry++;
				if (cal_retry > 3)
					break;
			}
			RF_DBG(dm, DBG_RF_IQK, "S%d ==> 0x1b00 = 0x%x\n", path,
			       odm_read_4byte(dm, 0x1b00));
			RF_DBG(dm, DBG_RF_IQK, "S%d ==> 0x1b08 = 0x%x\n", path,
			       odm_read_4byte(dm, 0x1b08));
			RF_DBG(dm, DBG_RF_IQK,
			       "S%d ==> delay_count = 0x%x, cal_retry = %x\n",
			       path, delay_count, cal_retry);

			odm_write_4byte(dm, 0x1b00, 0xf8000000 | (path << 1));
			if (!fail) {
				if (idx == TX_IQK)
					iqk_info->iqc_matrix[idx][path] = odm_read_4byte(dm, 0x1b38);
				else if (idx == RX_IQK) {
					odm_write_4byte(dm, 0x1b3c, 0x20000000);
					iqk_info->iqc_matrix[idx][path] = odm_read_4byte(dm, 0x1b3c);
				}
				RF_DBG(dm, DBG_RF_IQK, "S%d_IQC = 0x%x\n", path,
				       iqk_info->iqc_matrix[idx][path]);
			}

			if (idx == RX_IQK) {
				if (iqk_info->iqk_fail[TX_IQK][path] == false) /* TXIQK success in RXIQK */
					odm_write_4byte(dm, 0x1b38, iqk_info->iqc_matrix[TX_IQK][path]);
				else
					odm_set_bb_reg(dm, iqk_apply[path], BIT(0), 0x0);

				if (fail)
					odm_set_bb_reg(dm, iqk_apply[path], (BIT(11) | BIT(10)), 0x0);

				if (*dm->band_type == ODM_BAND_2_4G)
					odm_set_rf_reg(dm, (enum rf_path)path, RF_0xdf, BIT(11), 0x0);
			}

			iqk_info->iqk_fail[idx][path] = fail;
		}
		RF_DBG(dm, DBG_RF_IQK,
		       "IQK0_fail = %d, IQK1_fail = %d, IQK2_fail = %d, IQK3_fail = %d\n",
		       iqk_info->iqk_fail[idx][0], iqk_info->iqk_fail[idx][1],
		       iqk_info->iqk_fail[idx][2], iqk_info->iqk_fail[idx][3]);
	}
}

void _iqk_tx_8814a(struct dm_struct *dm, u8 chnl_idx)
{
	RF_DBG(dm, DBG_RF_IQK, "band_width = %d, ExtPA2G = %d\n",
	       *dm->band_width, dm->ext_pa);
	RF_DBG(dm, DBG_RF_IQK, "Interface = %d, band_type = %d\n",
	       dm->support_interface, *dm->band_type);
	RF_DBG(dm, DBG_RF_IQK, "cut_version = %x\n", dm->cut_version);

	odm_set_rf_reg(dm, RF_PATH_A, RF_0x58, BIT(19), 0x1);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x58, BIT(19), 0x1);
	odm_set_rf_reg(dm, RF_PATH_C, RF_0x58, BIT(19), 0x1);
	odm_set_rf_reg(dm, RF_PATH_D, RF_0x58, BIT(19), 0x1);

	odm_set_bb_reg(dm, R_0xc94, (BIT(11) | BIT(10) | BIT(0)), 0x401);
	odm_set_bb_reg(dm, R_0xe94, (BIT(11) | BIT(10) | BIT(0)), 0x401);
	odm_set_bb_reg(dm, R_0x1894, (BIT(11) | BIT(10) | BIT(0)), 0x401);
	odm_set_bb_reg(dm, R_0x1a94, (BIT(11) | BIT(10) | BIT(0)), 0x401);

	if (*dm->band_type == ODM_BAND_5G)
		odm_write_4byte(dm, 0x1b00, 0xf8000ff1);
	else
		odm_write_4byte(dm, 0x1b00, 0xf8000ef1);

	ODM_delay_ms(1);

	odm_write_4byte(dm, 0x810, 0x20101063);
	odm_write_4byte(dm, 0x90c, 0x0B00C000);

	_lok_one_shot(dm);
	_iqk_one_shot(dm);
}

void phy_iq_calibrate_8814a_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u8 ii, jj;
	static boolean firstrun = true;
	if (firstrun) {
		firstrun = false;
		RF_DBG(dm, DBG_RF_IQK, "=====>%s\n", __func__);
		for (jj = 0; jj < 2; jj++) {
			for (ii = 0; ii < NUM; ii++) {
				iqk_info->lok_fail[ii] = true;
				iqk_info->iqk_fail[jj][ii] = true;
				iqk_info->iqc_matrix[jj][ii] = 0x20000000;
			}
		}
	}
}

void _phy_iq_calibrate_8814a(struct dm_struct *dm, u8 channel)
{
	struct dm_iqk_info *iqk_info = &dm->IQK_info;
	u32 MAC_backup[MAC_REG_NUM_8814], BB_backup[BB_REG_NUM_8814], RF_backup[RF_REG_NUM_8814][4];
	u32 backup_mac_reg[MAC_REG_NUM_8814] = {0x520, 0x550};
	u32 backup_bb_reg[BB_REG_NUM_8814] = {0xa14, 0x808, 0x838, 0x90c, 0x810, 0xcb0, 0xeb0,
					      0x18b4, 0x1ab4, 0x1abc, 0x9a4, 0x764, 0xcbc, 0x910};
	u32 backup_rf_reg[RF_REG_NUM_8814] = {0x0};
	u8 chnl_idx = odm_get_right_chnl_place_for_iqk(channel);

	iqk_info->iqk_times++;

	_iqk_backup_mac_bb_8814a(dm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg);
	_iqk_afe_setting_8814a(dm, true);
	_iqk_backup_rf_8814a(dm, RF_backup, backup_rf_reg);
	_iqk_configure_mac_8814a(dm);
	_iqk_tx_8814a(dm, chnl_idx);
	_iqk_reset_nctl_8814a(dm); /* for 3-wire to  BB use */
	_iqk_afe_setting_8814a(dm, false);
	_iqk_restore_mac_bb_8814a(dm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg);
	_iqk_restore_rf_8814a(dm, backup_rf_reg, RF_backup);
}

/*IQK version:0xf*/
/*do not bypass path A IQK result*/
void phy_iq_calibrate_8814a(void *dm_void, boolean is_recovery)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _hal_rf_ *rf = &(dm->rf_table);

	phy_iq_calibrate_8814a_init(dm);
	_phy_iq_calibrate_8814a(dm, *dm->channel);
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	_iqk_iqk_fail_report_8814a(dm);
#endif
}

#endif
