/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "phydm_precomp.h"

void phydm_dynamic_ant_weighting(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

#ifdef DYN_ANT_WEIGHTING_SUPPORT
	#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & (ODM_RTL8197F))
		phydm_dynamic_ant_weighting_8197f(dm);
	#endif

	#if (RTL8812A_SUPPORT == 1)
	if (dm->support_ic_type & (ODM_RTL8812)) {
		phydm_dynamic_ant_weighting_8812a(dm);
	}
	#endif

	#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type & (ODM_RTL8822B))
		phydm_dynamic_ant_weighting_8822b(dm);
	#endif
#endif
}

#ifdef DYN_ANT_WEIGHTING_SUPPORT
void phydm_dyn_ant_weight_dbg(
	void *dm_void,
	char input[][16],
	u32 *_used,
	char *output,
	u32 *_out_len,
	u32 input_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	char help[] = "-h";
	u32 var1[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;

	if ((strcmp(input[1], help) == 0)) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "echo dis_dym_ant_weighting {0/1}\n");

	} else {
		PHYDM_SSCANF(input[1], DCMD_DECIMAL, &var1[0]);

		if (var1[0] == 1) {
			dm->is_disable_dym_ant_weighting = 1;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Disable dyn-ant-weighting\n");
		} else {
			dm->is_disable_dym_ant_weighting = 0;
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Enable dyn-ant-weighting\n");
		}
	}
	*_used = used;
	*_out_len = out_len;
}
#endif

void phydm_iq_gen_en(void *dm_void)
{
#ifdef PHYDM_COMPILE_IC_2SS
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 i;

#if (ODM_IC_11AC_SERIES_SUPPORT)
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		for (i = RF_PATH_A; i <= RF_PATH_B; i++) {
			odm_set_rf_reg(dm, (enum rf_path)i, RF_0xef, BIT(19), 0x1); /*RF mode table write enable*/
			odm_set_rf_reg(dm, (enum rf_path)i, RF_0x33, 0xF, 3); /*Select RX mode*/
			odm_set_rf_reg(dm, (enum rf_path)i, RF_0x3e, 0xfffff, 0x00036); /*Set Table data*/
			odm_set_rf_reg(dm, (enum rf_path)i, RF_0x3f, 0xfffff, 0x5AFCE); /*Set Table data*/
			odm_set_rf_reg(dm, (enum rf_path)i, RF_0xef, BIT(19), 0x0); /*RF mode table write disable*/
		}
	}
#endif
#endif
}

void phydm_dis_cdd(void *dm_void)
{
#ifdef PHYDM_COMPILE_IC_2SS
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	#if (ODM_IC_11AC_SERIES_SUPPORT)
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		odm_set_bb_reg(dm, R_0x808, 0x3ffff00, 0);
		odm_set_bb_reg(dm, R_0x9ac, 0x1fff, 0);
		odm_set_bb_reg(dm, R_0x9ac, BIT(13), 1);
	}
	#endif
#endif
}

void phydm_pathb_q_matrix_rotate_en(void *dm_void)
{
#ifdef PHYDM_COMPILE_IC_2SS
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	#if (ODM_IC_11AC_SERIES_SUPPORT)
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		phydm_iq_gen_en(dm);

		#ifdef PHYDM_COMMON_API_SUPPORT
		if (phydm_api_trx_mode(dm, BB_PATH_AB, BB_PATH_AB, true) == false)
			return;
		#endif

		phydm_dis_cdd(dm);
		odm_set_bb_reg(dm, R_0x195c, MASKDWORD, 0x40000); /*Set Q matrix r_v11 =1*/
		phydm_pathb_q_matrix_rotate(dm, 0);
		odm_set_bb_reg(dm, R_0x191c, BIT(7), 1); /*Set Q matrix enable*/
	}
	#endif
#endif
}

void phydm_pathb_q_matrix_rotate(void *dm_void, u16 phase_idx)
{
#ifdef PHYDM_COMPILE_IC_2SS
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 phase_table_0[12] = {0x40000, 0x376CF, 0x20000, 0x00000, 0xFE0000, 0xFC8930,
				 0xFC0000, 0xFC8930, 0xFDFFFF, 0x000000, 0x020000, 0x0376CF};
	u32 phase_table_1[12] = {0x00000, 0x1FFFF, 0x376CF, 0x40000, 0x0376CF, 0x01FFFF,
				 0x000000, 0xFDFFFF, 0xFC8930, 0xFC0000, 0xFC8930, 0xFDFFFF};

	if (phase_idx >= 12) {
		PHYDM_DBG(dm, ODM_COMP_API, "Phase Set Error: %d\n", phase_idx);
		return;
	}

	#if (ODM_IC_11AC_SERIES_SUPPORT)
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		odm_set_bb_reg(dm, R_0x1954, 0xffffff, phase_table_0[phase_idx]); /*Set Q matrix r_v21*/
		odm_set_bb_reg(dm, R_0x1950, 0xffffff, phase_table_1[phase_idx]);
	}
	#endif
#endif
}

void phydm_init_trx_antenna_setting(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (dm->support_ic_type & (ODM_RTL8814A)) {
		u8 rx_ant = 0, tx_ant = 0;

		rx_ant = (u8)odm_get_bb_reg(dm, ODM_REG(BB_RX_PATH, dm), ODM_BIT(BB_RX_PATH, dm));
		tx_ant = (u8)odm_get_bb_reg(dm, ODM_REG(BB_TX_PATH, dm), ODM_BIT(BB_TX_PATH, dm));
		dm->tx_ant_status = (tx_ant & 0xf);
		dm->rx_ant_status = (rx_ant & 0xf);
	} else if (dm->support_ic_type & (ODM_RTL8723D | ODM_RTL8821C | ODM_RTL8710B)) { /* JJ ADD 20161014 */
		dm->tx_ant_status = 0x1;
		dm->rx_ant_status = 0x1;

	} else if (dm->support_ic_type & (ODM_RTL8192F)) { /* Mingzhi ADD 20170907 */
		u8 rx_ant = 0, tx_ant = 0;

		rx_ant = (u8)odm_get_bb_reg(dm, R_0x90c, BIT0 | BIT1);
		tx_ant = (u8)odm_get_bb_reg(dm, R_0xc04, BIT0 | BIT1);
		dm->tx_ant_status = (tx_ant & 0x3);
		dm->rx_ant_status = (rx_ant & 0x3);
	}
}

void phydm_config_ofdm_tx_path(void *dm_void, u32 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
#if ((RTL8192E_SUPPORT == 1) || (RTL8812A_SUPPORT == 1))
	u8 ofdm_tx_path = 0x33;
#endif

	if (dm->support_ic_type & (ODM_RTL8192E)) {
		#if (RTL8192E_SUPPORT == 1)
		if (path == BB_PATH_A) {
			odm_set_bb_reg(dm, R_0x90c, MASKDWORD, 0x81121111);
			/**/
		} else if (path == BB_PATH_B) {
			odm_set_bb_reg(dm, R_0x90c, MASKDWORD, 0x82221222);
			/**/
		} else if (path == BB_PATH_AB) {
			odm_set_bb_reg(dm, R_0x90c, MASKDWORD, 0x83321333);
			/**/
		}
		#endif
	} else if (dm->support_ic_type & (ODM_RTL8812)) {
		#if (RTL8812A_SUPPORT == 1)
		if (path == BB_PATH_A) {
			ofdm_tx_path = 0x11;
			/**/
		} else if (path == BB_PATH_B) {
			ofdm_tx_path = 0x22;
			/**/
		} else if (path == BB_PATH_AB) {
			ofdm_tx_path = 0x33;
			/**/
		}

		odm_set_bb_reg(dm, R_0x80c, 0xff00, ofdm_tx_path);
		#endif
	}
}

void phydm_config_ofdm_rx_path(void *dm_void, u32 path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 ofdm_rx_path = 0;

	if (dm->support_ic_type & (ODM_RTL8192E)) {
#if (RTL8192E_SUPPORT == 1)
		if (path == BB_PATH_A) {
			ofdm_rx_path = 1;
			/**/
		} else if (path == BB_PATH_B) {
			ofdm_rx_path = 2;
			/**/
		} else if (path == BB_PATH_AB) {
			ofdm_rx_path = 3;
			/**/
		}

		odm_set_bb_reg(dm, R_0xc04, 0xff, (((ofdm_rx_path) << 4) | ofdm_rx_path));
		odm_set_bb_reg(dm, R_0xd04, 0xf, ofdm_rx_path);
#endif
	}
#if (RTL8812A_SUPPORT || RTL8822B_SUPPORT)
	else if (dm->support_ic_type & (ODM_RTL8812 | ODM_RTL8822B)) {
		if (path == BB_PATH_A) {
			ofdm_rx_path = 1;
			/**/
		} else if (path == BB_PATH_B) {
			ofdm_rx_path = 2;
			/**/
		} else if (path == BB_PATH_AB) {
			ofdm_rx_path = 3;
			/**/
		}

		odm_set_bb_reg(dm, R_0x808, MASKBYTE0, ((ofdm_rx_path << 4) | ofdm_rx_path));
	}
#endif
}

void phydm_config_cck_rx_antenna_init(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	if (dm->support_ic_type & ODM_IC_1SS)
		return;

	/*CCK 2R CCA parameters*/
	odm_set_bb_reg(dm, R_0xa00, BIT(15), 0x0); /*Disable antenna diversity*/
	odm_set_bb_reg(dm, R_0xa70, BIT(7), 0); /*Concurrent CCA at LSB & USB*/
	odm_set_bb_reg(dm, R_0xa74, BIT(8), 0); /*RX path diversity enable*/
	odm_set_bb_reg(dm, R_0xa14, BIT(7), 0); /*r_en_mrc_antsel*/
	odm_set_bb_reg(dm, R_0xa20, (BIT(5) | BIT(4)), 1); /*MBC weighting*/

	if (dm->support_ic_type & (ODM_RTL8192E | ODM_RTL8197F | ODM_RTL8192F)) { /*jj add 20170822*/
		odm_set_bb_reg(dm, R_0xa08, BIT(28), 1); /*r_cck_2nd_sel_eco*/
		/**/
	} else if (dm->support_ic_type & ODM_RTL8814A) {
		odm_set_bb_reg(dm, R_0xa84, BIT(28), 1); /*2R CCA only*/
		/**/
	}
#endif
}

void phydm_config_cck_rx_path(void *dm_void, enum bb_path path)
{
#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 path_div_select = 0;
	u8 cck_path[2] = {0};
	u8 en_2R_path = 0;
	u8 en_2R_mrc = 0;
	u8 i = 0, j = 0;
	u8 num_enable_path = 0;
	u8 cck_mrc_max_path = 2;

	if (dm->support_ic_type & ODM_IC_1SS)
		return;

	for (i = 0; i < 4; i++) {
		if (path & BIT(i)) { /*ex: PHYDM_ABCD*/
			num_enable_path++;
			cck_path[j] = i;
			j++;
		}
		if (num_enable_path >= cck_mrc_max_path)
			break;
	}

	if (num_enable_path > 1) {
		path_div_select = 1;
		en_2R_path = 1;
		en_2R_mrc = 1;
	} else {
		path_div_select = 0;
		en_2R_path = 0;
		en_2R_mrc = 0;
	}

	odm_set_bb_reg(dm, R_0xa04, (BIT(27) | BIT(26)), cck_path[0]); /*CCK_1 input signal path*/
	odm_set_bb_reg(dm, R_0xa04, (BIT(25) | BIT(24)), cck_path[1]); /*CCK_2 input signal path*/
	odm_set_bb_reg(dm, R_0xa74, BIT(8), path_div_select); /*enable Rx path diversity*/
	odm_set_bb_reg(dm, R_0xa2c, BIT(18), en_2R_path); /*enable 2R Rx path*/
	odm_set_bb_reg(dm, R_0xa2c, BIT(22), en_2R_mrc); /*enable 2R MRC*/
	if (dm->support_ic_type & ODM_RTL8192F) {
		if (path == BB_PATH_A) {
			odm_set_bb_reg(dm, R_0xa04, (BIT(27) | BIT(26)), 0);	/*CCK_1 input signal path*/
			odm_set_bb_reg(dm, R_0xa04, (BIT(25) | BIT(24)), 0);	/*CCK_2 input signal path*/
			odm_set_bb_reg(dm, R_0xa74, BIT(8), 0); /*enable Rx path diversity*/
			odm_set_bb_reg(dm, R_0xa2c, (BIT(18) | BIT(17)), 0); /*enable 2R Rx path*/
			odm_set_bb_reg(dm, R_0xa2c, (BIT(22) | BIT(21)), 0); /*enable 2R MRC*/
		} else if (path == BB_PATH_B) {/*for DC cancellation*/
			odm_set_bb_reg(dm, R_0xa04, (BIT(27) | BIT(26)), 1);	/*CCK_1 input signal path*/
			odm_set_bb_reg(dm, R_0xa04, (BIT(25) | BIT(24)), 1);	/*CCK_2 input signal path*/
			odm_set_bb_reg(dm, R_0xa74, BIT(8), 0); /*enable Rx path diversity*/
			odm_set_bb_reg(dm, R_0xa2c, (BIT(18) | BIT(17)), 0); /*enable 2R Rx path*/
			odm_set_bb_reg(dm, R_0xa2c, (BIT(22) | BIT(21)), 0); /*enable 2R MRC*/
		} else if (path == BB_PATH_AB) {
			odm_set_bb_reg(dm, R_0xa04, (BIT(27) | BIT(26)), 0);	/*CCK_1 input signal path*/
			odm_set_bb_reg(dm, R_0xa04, (BIT(25) | BIT(24)), 1);	/*CCK_2 input signal path*/
			odm_set_bb_reg(dm, R_0xa74, BIT(8), 1); /*enable Rx path diversity*/
			odm_set_bb_reg(dm, R_0xa2c, (BIT(18) | BIT(17)), 1); /*enable 2R Rx path*/
			odm_set_bb_reg(dm, R_0xa2c, (BIT(22) | BIT(21)), 1); /*enable 2R MRC*/
		}
	}

#endif
}

void phydm_config_cck_tx_path(void *dm_void, enum bb_path path)
{
#if (defined(PHYDM_COMPILE_ABOVE_2SS))
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (path == BB_PATH_A)
		odm_set_bb_reg(dm, R_0xa04, 0xf0000000, 0x8);
	else if (path == BB_PATH_B)
		odm_set_bb_reg(dm, R_0xa04, 0xf0000000, 0x4);
	else if (path == BB_PATH_AB)
		odm_set_bb_reg(dm, R_0xa04, 0xf0000000, 0xc);
#endif
}

void phydm_config_trx_path(void *dm_void, u32 *const dm_value, u32 *_used,
			   char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;

#if 0
	dm_value[0]: 0:CCK, 1:OFDM
	dm_value[1]: 1:TX, 2:RX
	dm_value[2]: 1:path_A, 2:path_B, 3:path_AB
#endif

	/* CCK */
	if (dm_value[0] == 0) {
		if (dm_value[1] == 1) { /*TX*/
			if (dm_value[2] == 1)
				phydm_config_cck_tx_path(dm, BB_PATH_A);
			else if (dm_value[2] == 2)
				phydm_config_cck_tx_path(dm, BB_PATH_B);
			else if (dm_value[2] == 3)
				phydm_config_cck_tx_path(dm, BB_PATH_AB);
		} else if (dm_value[1] == 2) { /*RX*/

			phydm_config_cck_rx_antenna_init(dm);

			if (dm_value[2] == 1)
				phydm_config_cck_rx_path(dm, BB_PATH_A);
			else if (dm_value[2] == 2)
				phydm_config_cck_rx_path(dm, BB_PATH_B);
			else if (dm_value[2] == 3)
				phydm_config_cck_rx_path(dm, BB_PATH_AB);
			}
		}
	/* OFDM */
	else if (dm_value[0] == 1) {
		if (dm_value[1] == 1) { /*TX*/
			phydm_config_ofdm_tx_path(dm, dm_value[2]);
			/**/
		} else if (dm_value[1] == 2) { /*RX*/
			phydm_config_ofdm_rx_path(dm, dm_value[2]);
			/**/
		}
	}

	PDM_SNPF(out_len, used, output + used, out_len - used,
		 "PHYDM Set path [%s] [%s] = [%s%s%s%s]\n",
		 (dm_value[0] == 1) ? "OFDM" : "CCK",
		 (dm_value[1] == 1) ? "TX" : "RX",
		 (dm_value[2] & 0x1) ? "A" : "", (dm_value[2] & 0x2) ? "B" : "",
		 (dm_value[2] & 0x4) ? "C" : "",
		 (dm_value[2] & 0x8) ? "D" : "");
}

void phydm_tx_2path(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	PHYDM_DBG(dm, ODM_COMP_API, "%s ======>\n", __func__);

#if (defined(PHYDM_COMPILE_IC_2SS))
	if (dm->support_ic_type & ODM_IC_2SS) {
		#if (RTL8822B_SUPPORT == 1 || RTL8192F_SUPPORT == 1 || RTL8197F_SUPPORT == 1)
		if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8197F | ODM_RTL8192F))
			phydm_api_trx_mode(dm, BB_PATH_AB, (enum bb_path)dm->rx_ant_status, true);
		#endif

		#if (RTL8812A_SUPPORT == 1 || RTL8192E_SUPPORT == 1)
		if (dm->support_ic_type & (ODM_RTL8812 | ODM_RTL8192E)) {
			phydm_config_cck_tx_path(dm, BB_PATH_AB);
			phydm_config_ofdm_tx_path(dm, BB_PATH_AB);
		}
		#endif
	}

#endif
}

void phydm_stop_3_wire(void *dm_void, u8 set_type)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (set_type == PHYDM_SET) {
		/*[Stop 3-wires]*/
		if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
			odm_set_bb_reg(dm, R_0xc00, 0xf, 0x4); /*	hardware 3-wire off */
			odm_set_bb_reg(dm, R_0xe00, 0xf, 0x4); /*	hardware 3-wire off */
		} else {
			odm_set_bb_reg(dm, R_0x88c, 0xf00000, 0xf); /* 3 wire Disable    88c[23:20]=0xf */
		}

	} else { /*if (set_type == PHYDM_REVERT)*/

		/*[Start 3-wires]*/
		if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
			odm_set_bb_reg(dm, R_0xc00, 0xf, 0x7); /*	hardware 3-wire on */
			odm_set_bb_reg(dm, R_0xe00, 0xf, 0x7); /*	hardware 3-wire on */
		} else {
			odm_set_bb_reg(dm, R_0x88c, 0xf00000, 0x0); /* 3 wire enable 88c[23:20]=0x0 */
		}
	}
}

u8 phydm_stop_ic_trx(void *dm_void, u8 set_type)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_api_stuc *api = &dm->api_table;
	u32 i;
	u8 trx_idle_success = false;
	u32 dbg_port_value = 0;

	if (set_type == PHYDM_SET) {
		/*[Stop TRX]---------------------------------------------------------------------*/
		if (phydm_set_bb_dbg_port(dm, DBGPORT_PRI_3, 0x0) == false) /*set debug port to 0x0*/
			return PHYDM_SET_FAIL;

		for (i = 0; i < 10000; i++) {
			dbg_port_value = phydm_get_bb_dbg_port_val(dm);
			if ((dbg_port_value & (BIT(17) | BIT(3))) == 0) /* PHYTXON && CCA_all */ {
				PHYDM_DBG(dm, ODM_COMP_API,
					  "Stop trx wait for (%d) times\n", i);

				trx_idle_success = true;
				break;
			}
		}
		phydm_release_bb_dbg_port(dm);

		if (trx_idle_success) {
			api->tx_queue_bitmap = (u8)odm_get_bb_reg(dm, R_0x520, 0xff0000);

			odm_set_bb_reg(dm, R_0x520, 0xff0000, 0xff); /*pause all TX queue*/

			if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
				odm_set_bb_reg(dm, R_0x808, BIT(28), 0); /*disable CCK block*/
				odm_set_bb_reg(dm, R_0x838, BIT(1), 1); /*disable OFDM RX CCA*/
			} else {
				/*TBD*/
				odm_set_bb_reg(dm, R_0x800, BIT(24), 0); /* disable whole CCK block */

				api->rx_iqc_reg_1 = odm_get_bb_reg(dm, R_0xc14, MASKDWORD);
				api->rx_iqc_reg_2 = odm_get_bb_reg(dm, R_0xc1c, MASKDWORD);

				odm_set_bb_reg(dm, R_0xc14, MASKDWORD, 0x0); /* [ Set IQK Matrix = 0 ] equivalent to [ Turn off CCA] */
				odm_set_bb_reg(dm, R_0xc1c, MASKDWORD, 0x0);
			}

		} else {
			return PHYDM_SET_FAIL;
		}

		return PHYDM_SET_SUCCESS;

	} else { /*if (set_type == PHYDM_REVERT)*/

		odm_set_bb_reg(dm, R_0x520, 0xff0000, (u32)(api->tx_queue_bitmap)); /*Release all TX queue*/

		if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
			odm_set_bb_reg(dm, R_0x808, BIT(28), 1); /*enable CCK block*/
			odm_set_bb_reg(dm, R_0x838, BIT(1), 0); /*enable OFDM RX CCA*/
		} else {
			/*TBD*/
			odm_set_bb_reg(dm, R_0x800, BIT(24), 1); /* enable whole CCK block */

			odm_set_bb_reg(dm, R_0xc14, MASKDWORD, api->rx_iqc_reg_1); /* [ Set IQK Matrix = 0 ] equivalent to [ Turn off CCA] */
			odm_set_bb_reg(dm, R_0xc1c, MASKDWORD, api->rx_iqc_reg_2);
		}

		return PHYDM_SET_SUCCESS;
	}
}

void phydm_set_ext_switch(void *dm_void, u32 *const dm_value, u32 *_used,
			  char *output, u32 *_out_len)
{
#if (RTL8821A_SUPPORT == 1) || (RTL8881A_SUPPORT == 1)
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 ext_ant_switch = dm_value[0];

	if (dm->support_ic_type & (ODM_RTL8821 | ODM_RTL8881A)) {
		/*Output Pin Settings*/

		/*select DPDT_P and DPDT_N as output pin*/
		odm_set_mac_reg(dm, R_0x4c, BIT(23), 0);

		/*by WLAN control*/
		odm_set_mac_reg(dm, R_0x4c, BIT(24), 1);

		/*DPDT_N = 1b'0*/ /*DPDT_P = 1b'0*/
		odm_set_bb_reg(dm, R_0xcb4, 0xFF, 77);

		if (ext_ant_switch == MAIN_ANT) {
			odm_set_bb_reg(dm, R_0xcb4, (BIT(29) | BIT(28)), 1);
			PHYDM_DBG(dm, ODM_COMP_API, "8821A ant swh=2b'01\n");
		} else if (ext_ant_switch == AUX_ANT) {
			odm_set_bb_reg(dm, R_0xcb4, BIT(29) | BIT(28), 2);
			PHYDM_DBG(dm, ODM_COMP_API, "*8821A ant swh=2b'10\n");
		}
	}
#endif
}

void phydm_csi_mask_enable(void *dm_void, u32 enable)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 reg_value = 0;

	reg_value = (enable == FUNC_ENABLE) ? 1 : 0;

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		odm_set_bb_reg(dm, R_0xd2c, BIT(28), reg_value);
		PHYDM_DBG(dm, ODM_COMP_API,
			  "Enable CSI Mask:  Reg 0xD2C[28] = ((0x%x))\n",
			  reg_value);

	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		odm_set_bb_reg(dm, R_0x874, BIT(0), reg_value);
		PHYDM_DBG(dm, ODM_COMP_API,
			  "Enable CSI Mask:  Reg 0x874[0] = ((0x%x))\n",
			  reg_value);
	}
}

void phydm_clean_all_csi_mask(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		odm_set_bb_reg(dm, R_0xd40, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0xd44, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0xd48, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0xd4c, MASKDWORD, 0);

	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		odm_set_bb_reg(dm, R_0x880, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0x884, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0x888, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0x88c, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0x890, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0x894, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0x898, MASKDWORD, 0);
		odm_set_bb_reg(dm, R_0x89c, MASKDWORD, 0);
	}
}

void phydm_set_csi_mask_reg(void *dm_void, u32 tone_idx_tmp, u8 tone_direction)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 byte_offset, bit_offset;
	u32 target_reg;
	u8 reg_tmp_value;
	u32 tone_num = 64;
	u32 tone_num_shift = 0;
	u32 csi_mask_reg_p = 0, csi_mask_reg_n = 0;

	/* calculate real tone idx*/
	if ((tone_idx_tmp % 10) >= 5)
		tone_idx_tmp += 10;

	tone_idx_tmp = (tone_idx_tmp / 10);

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		tone_num = 64;
		csi_mask_reg_p = 0xD40;
		csi_mask_reg_n = 0xD48;

	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		tone_num = 128;
		csi_mask_reg_p = 0x880;
		csi_mask_reg_n = 0x890;
	}

	if (tone_direction == FREQ_POSITIVE) {
		if (tone_idx_tmp >= (tone_num - 1))
			tone_idx_tmp = (tone_num - 1);

		byte_offset = (u8)(tone_idx_tmp >> 3);
		bit_offset = (u8)(tone_idx_tmp & 0x7);
		target_reg = csi_mask_reg_p + byte_offset;

	} else {
		tone_num_shift = tone_num;

		if (tone_idx_tmp >= tone_num)
			tone_idx_tmp = tone_num;

		tone_idx_tmp = tone_num - tone_idx_tmp;

		byte_offset = (u8)(tone_idx_tmp >> 3);
		bit_offset = (u8)(tone_idx_tmp & 0x7);
		target_reg = csi_mask_reg_n + byte_offset;
	}

	reg_tmp_value = odm_read_1byte(dm, target_reg);
	PHYDM_DBG(dm, ODM_COMP_API,
		  "Pre Mask tone idx[%d]:  Reg0x%x = ((0x%x))\n",
		  (tone_idx_tmp + tone_num_shift), target_reg, reg_tmp_value);
	reg_tmp_value |= BIT(bit_offset);
	odm_write_1byte(dm, target_reg, reg_tmp_value);
	PHYDM_DBG(dm, ODM_COMP_API,
		  "New Mask tone idx[%d]:  Reg0x%x = ((0x%x))\n",
		  (tone_idx_tmp + tone_num_shift), target_reg, reg_tmp_value);
}

void phydm_set_nbi_reg(void *dm_void, u32 tone_idx_tmp, u32 bw)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 nbi_table_128[NBI_TABLE_SIZE_128] = {25, 55, 85, 115, 135, 155, 185, 205, 225, 245, /*1~10*/ /*tone_idx X 10*/
						 265, 285, 305, 335, 355, 375, 395, 415, 435, 455, /*11~20*/
						 485, 505, 525, 555, 585, 615, 635}; /*21~27*/

	u32 nbi_table_256[NBI_TABLE_SIZE_256] = {25, 55, 85, 115, 135, 155, 175, 195, 225, 245, /*1~10*/
						 265, 285, 305, 325, 345, 365, 385, 405, 425, 445, /*11~20*/
						 465, 485, 505, 525, 545, 565, 585, 605, 625, 645, /*21~30*/
						 665, 695, 715, 735, 755, 775, 795, 815, 835, 855, /*31~40*/
						 875, 895, 915, 935, 955, 975, 995, 1015, 1035, 1055, /*41~50*/
						 1085, 1105, 1125, 1145, 1175, 1195, 1225, 1255, 1275}; /*51~59*/

	u32 reg_idx = 0;
	u32 i;
	u8 nbi_table_idx = FFT_128_TYPE;

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		nbi_table_idx = FFT_128_TYPE;
	} else if (dm->support_ic_type & ODM_IC_11AC_1_SERIES) {
		nbi_table_idx = FFT_256_TYPE;
	} else if (dm->support_ic_type & ODM_IC_11AC_2_SERIES) {
		if (bw == 80)
			nbi_table_idx = FFT_256_TYPE;
		else /*20M, 40M*/
			nbi_table_idx = FFT_128_TYPE;
	}

	if (nbi_table_idx == FFT_128_TYPE) {
		for (i = 0; i < NBI_TABLE_SIZE_128; i++) {
			if (tone_idx_tmp < nbi_table_128[i]) {
				reg_idx = i + 1;
				break;
			}
		}

	} else if (nbi_table_idx == FFT_256_TYPE) {
		for (i = 0; i < NBI_TABLE_SIZE_256; i++) {
			if (tone_idx_tmp < nbi_table_256[i]) {
				reg_idx = i + 1;
				break;
			}
		}
	}

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		odm_set_bb_reg(dm, R_0xc40, 0x1f000000, reg_idx);
		PHYDM_DBG(dm, ODM_COMP_API,
			  "Set tone idx:  Reg0xC40[28:24] = ((0x%x))\n",
			  reg_idx);
		/**/
	} else {
		odm_set_bb_reg(dm, R_0x87c, 0xfc000, reg_idx);
		PHYDM_DBG(dm, ODM_COMP_API,
			  "Set tone idx: Reg0x87C[19:14] = ((0x%x))\n",
			  reg_idx);
		/**/
	}
}

void phydm_nbi_enable(void *dm_void, u32 enable)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 val = 0;

	val = (enable == FUNC_ENABLE) ? 1 : 0;

	PHYDM_DBG(dm, ODM_COMP_API, "Enable NBI=%d\n", val);

	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (dm->support_ic_type & (ODM_RTL8192F | ODM_RTL8197F)) {
			val = (enable == FUNC_ENABLE) ? 0xf : 0;
			odm_set_bb_reg(dm, R_0xc50, 0xf000000, val);
		} else {
			odm_set_bb_reg(dm, R_0xc40, BIT(9), val);
		}
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8821C)) {
			odm_set_bb_reg(dm, R_0x87c, BIT(13), val);
			odm_set_bb_reg(dm, R_0xc20, BIT(28), val);
			if (dm->rf_type > RF_1T1R)
				odm_set_bb_reg(dm, R_0xe20, BIT(28), val);
		} else {
			odm_set_bb_reg(dm, R_0x87c, BIT(13), val);
		}
	}
}

u8 phydm_calculate_fc(void *dm_void, u32 channel, u32 bw, u32 second_ch,
		      u32 *fc_in)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 fc = *fc_in;
	u32 start_ch_per_40m[NUM_START_CH_40M] = {36, 44, 52, 60, 100, 108, 116, 124, 132, 140, 149, 157, 165, 173};
	u32 start_ch_per_80m[NUM_START_CH_80M] = {36, 52, 100, 116, 132, 149, 165};
	u32 *start_ch = &start_ch_per_40m[0];
	u32 num_start_channel = NUM_START_CH_40M;
	u32 channel_offset = 0;
	u32 i;

	/*2.4G*/
	if (channel <= 14 && channel > 0) {
		if (bw == 80)
			return PHYDM_SET_FAIL;

		fc = 2412 + (channel - 1) * 5;

		if (bw == 40 && second_ch == PHYDM_ABOVE) {
			if (channel >= 10) {
				PHYDM_DBG(dm, ODM_COMP_API,
					  "CH = ((%d)), Scnd_CH = ((%d)) Error setting\n",
					  channel, second_ch);
				return PHYDM_SET_FAIL;
			}
			fc += 10;
		} else if (bw == 40 && (second_ch == PHYDM_BELOW)) {
			if (channel <= 2) {
				PHYDM_DBG(dm, ODM_COMP_API,
					  "CH = ((%d)), Scnd_CH = ((%d)) Error setting\n",
					  channel, second_ch);
				return PHYDM_SET_FAIL;
			}
			fc -= 10;
		}
	}
	/*5G*/
	else if (channel >= 36 && channel <= 177) {
		if (bw != 20) {
			if (bw == 40) {
				num_start_channel = NUM_START_CH_40M;
				start_ch = &start_ch_per_40m[0];
				channel_offset = CH_OFFSET_40M;
			} else if (bw == 80) {
				num_start_channel = NUM_START_CH_80M;
				start_ch = &start_ch_per_80m[0];
				channel_offset = CH_OFFSET_80M;
			}

			for (i = 0; i < (num_start_channel - 1); i++) {
				if (channel < start_ch[i + 1]) {
					channel = start_ch[i] + channel_offset;
					break;
				}
			}
			PHYDM_DBG(dm, ODM_COMP_API, "Mod_CH = ((%d))\n",
				  channel);
		}

		fc = 5180 + (channel - 36) * 5;

	} else {
		PHYDM_DBG(dm, ODM_COMP_API, "CH = ((%d)) Error setting\n",
			  channel);
		return PHYDM_SET_FAIL;
	}

	*fc_in = fc;

	return PHYDM_SET_SUCCESS;
}

u8 phydm_calculate_intf_distance(void *dm_void, u32 bw, u32 fc,
				 u32 f_interference, u32 *tone_idx_tmp_in)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 bw_up, bw_low;
	u32 int_distance;
	u32 tone_idx_tmp;
	u8 set_result = PHYDM_SET_NO_NEED;

	bw_up = fc + bw / 2;
	bw_low = fc - bw / 2;

	PHYDM_DBG(dm, ODM_COMP_API,
		  "[f_l, fc, fh] = [ %d, %d, %d ], f_int = ((%d))\n", bw_low,
		  fc, bw_up, f_interference);

	if (f_interference >= bw_low && f_interference <= bw_up) {
		int_distance = (fc >= f_interference) ? (fc - f_interference) : (f_interference - fc);
		tone_idx_tmp = (int_distance << 5); /* =10*(int_distance /0.3125) */
		PHYDM_DBG(dm, ODM_COMP_API,
			  "int_distance = ((%d MHz)) Mhz, tone_idx_tmp = ((%d.%d))\n",
			  int_distance, (tone_idx_tmp / 10),
			  (tone_idx_tmp % 10));
		*tone_idx_tmp_in = tone_idx_tmp;
		set_result = PHYDM_SET_SUCCESS;
	}

	return set_result;
}

u8 phydm_csi_mask_setting(void *dm_void, u32 enable, u32 channel, u32 bw,
			  u32 f_interference, u32 second_ch)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 fc = 2412;
	u8 tone_direction;
	u32 tone_idx_tmp;
	u8 set_result = PHYDM_SET_SUCCESS;

	if (enable == FUNC_DISABLE) {
		set_result = PHYDM_SET_SUCCESS;
		phydm_clean_all_csi_mask(dm);

	} else {
		PHYDM_DBG(dm, ODM_COMP_API,
			  "[Set CSI MASK_] CH = ((%d)), BW = ((%d)), f_intf = ((%d)), Scnd_CH = ((%s))\n",
			  channel, bw, f_interference,
			  (((bw == 20) || (channel > 14)) ? "Don't care" :
			  (second_ch == PHYDM_ABOVE) ? "H" : "L"));

		/*calculate fc*/
		if (phydm_calculate_fc(dm, channel, bw, second_ch, &fc) == PHYDM_SET_FAIL) {
			set_result = PHYDM_SET_FAIL;
		} else {
			/*calculate interference distance*/
			if (phydm_calculate_intf_distance(dm, bw, fc, f_interference, &tone_idx_tmp) == PHYDM_SET_SUCCESS) {
				tone_direction = (f_interference >= fc) ? FREQ_POSITIVE : FREQ_NEGATIVE;
				phydm_set_csi_mask_reg(dm, tone_idx_tmp, tone_direction);
				set_result = PHYDM_SET_SUCCESS;
			} else {
				set_result = PHYDM_SET_NO_NEED;
		}
	}
	}

	if (set_result == PHYDM_SET_SUCCESS)
		phydm_csi_mask_enable(dm, enable);
	else
		phydm_csi_mask_enable(dm, FUNC_DISABLE);

	return set_result;
}

u8 phydm_nbi_setting(void *dm_void, u32 enable, u32 channel, u32 bw,
		     u32 f_interference, u32 second_ch)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 fc = 2412;
	u32 tone_idx_tmp;
	u8 set_result = PHYDM_SET_SUCCESS;

	if (enable == FUNC_DISABLE) {
		set_result = PHYDM_SET_SUCCESS;
	} else {
		PHYDM_DBG(dm, ODM_COMP_API,
			  "[Set NBI] CH = ((%d)), BW = ((%d)), f_intf = ((%d)), Scnd_CH = ((%s))\n",
			  channel, bw, f_interference,
			  (((second_ch == PHYDM_DONT_CARE) || (bw == 20) ||
			  (channel > 14)) ? "Don't care" :
			  (second_ch == PHYDM_ABOVE) ? "H" : "L"));

		/*calculate fc*/
		if (phydm_calculate_fc(dm, channel, bw, second_ch, &fc) == PHYDM_SET_FAIL) {
			set_result = PHYDM_SET_FAIL;
		} else {
			/*calculate interference distance*/
			if (phydm_calculate_intf_distance(dm, bw, fc, f_interference, &tone_idx_tmp) == PHYDM_SET_SUCCESS) {
				phydm_set_nbi_reg(dm, tone_idx_tmp, bw);
				set_result = PHYDM_SET_SUCCESS;
			} else {
				set_result = PHYDM_SET_NO_NEED;
		}
	}
	}

	if (set_result == PHYDM_SET_SUCCESS)
		phydm_nbi_enable(dm, enable);
	else
		phydm_nbi_enable(dm, FUNC_DISABLE);

	return set_result;
}

void phydm_api_debug(void *dm_void, u32 function_map, u32 *const dm_value,
		     u32 *_used, char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u32 channel = dm_value[1];
	u32 bw = dm_value[2];
	u32 f_interference = dm_value[3];
	u32 second_ch = dm_value[4];
	u8 set_result = 0;

	/*PHYDM_API_NBI*/
	/*-------------------------------------------------------------------------------------------------------------------------------*/
	if (function_map == PHYDM_API_NBI) {
		if (dm_value[0] == 100) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "[HELP-NBI]  EN(on=1, off=2)   CH   BW(20/40/80)  f_intf(Mhz)    Scnd_CH(L=1, H=2)\n");
			return;

		} else if (dm_value[0] == FUNC_ENABLE) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "[Enable NBI] CH = ((%d)), BW = ((%d)), f_intf = ((%d)), Scnd_CH = ((%s))\n",
				 channel, bw, f_interference,
				 ((second_ch == PHYDM_DONT_CARE) ||
				 (bw == 20) || (channel > 14)) ? "Don't care" :
				 ((second_ch == PHYDM_ABOVE) ? "H" : "L"));
			set_result = phydm_nbi_setting(dm, FUNC_ENABLE, channel, bw, f_interference, second_ch);

		} else if (dm_value[0] == FUNC_DISABLE) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "[Disable NBI]\n");
			set_result = phydm_nbi_setting(dm, FUNC_DISABLE, channel, bw, f_interference, second_ch);

		} else {
			set_result = PHYDM_SET_FAIL;
		}
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "[NBI set result: %s]\n",
			 (set_result == PHYDM_SET_SUCCESS) ? "Success" :
			 ((set_result == PHYDM_SET_NO_NEED) ? "No need" :
			 "Error"));
	}

	/*PHYDM_CSI_MASK*/
	/*-------------------------------------------------------------------------------------------------------------------------------*/
	else if (function_map == PHYDM_API_CSI_MASK) {
		if (dm_value[0] == 100) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "[HELP-CSI MASK]  EN(on=1, off=2)   CH   BW(20/40/80)  f_intf(Mhz)    Scnd_CH(L=1, H=2)\n");
			return;

		} else if (dm_value[0] == FUNC_ENABLE) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "[Enable CSI MASK] CH = ((%d)), BW = ((%d)), f_intf = ((%d)), Scnd_CH = ((%s))\n",
				 channel, bw, f_interference,
				 (channel > 14) ? "Don't care" :
				 (((second_ch == PHYDM_DONT_CARE) ||
				 (bw == 20) || (channel > 14)) ? "H" : "L"));
			set_result = phydm_csi_mask_setting(dm, FUNC_ENABLE, channel, bw, f_interference, second_ch);

		} else if (dm_value[0] == FUNC_DISABLE) {
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "[Disable CSI MASK]\n");
			set_result = phydm_csi_mask_setting(dm, FUNC_DISABLE, channel, bw, f_interference, second_ch);

		} else {
			set_result = PHYDM_SET_FAIL;
		}
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "[CSI MASK set result: %s]\n",
			 (set_result == PHYDM_SET_SUCCESS) ? "Success" :
			 ((set_result == PHYDM_SET_NO_NEED) ? "No need" :
			 "Error"));
	}
	*_used = used;
	*_out_len = out_len;
}

void phydm_stop_ck320(void *dm_void, u8 enable)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 reg_value = enable ? 1 : 0;

	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		odm_set_bb_reg(dm, R_0x8b4, BIT(6), reg_value);
		/**/
	} else {
		if (dm->support_ic_type & ODM_IC_N_2SS) { /*N-2SS*/
			odm_set_bb_reg(dm, R_0x87c, BIT(29), reg_value);
			/**/
		} else { /*N-1SS*/
			odm_set_bb_reg(dm, R_0x87c, BIT(31), reg_value);
			/**/
		}
	}
}

boolean
phydm_set_bb_txagc_offset(void *dm_void, s8 power_offset, /*(unit: dB)*/
			  u8 add_half_db /*(+0.5 dB)*/)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	s8 power_idx = power_offset * 2;
	boolean set_success = false;

	PHYDM_DBG(dm, ODM_COMP_API, "power_offset=%d, add_half_db =%d\n",
		  power_offset, add_half_db);

	#if ODM_IC_11AC_SERIES_SUPPORT
	if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		if (power_offset > -16 || power_offset < 15) {
			if (add_half_db)
				power_idx += 1;

			power_idx &= 0x3f;

			PHYDM_DBG(dm, ODM_COMP_API, "Reg_idx =0x%x\n",
				  power_idx);
			odm_set_bb_reg(dm, R_0x8b4, 0x3f, power_idx);
			set_success = true;
		} else {
			pr_debug("[Warning] TX AGC Offset Setting error!");
		}
	}
	#endif

	#if ODM_IC_11N_SERIES_SUPPORT
	if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		if (power_offset > -8 || power_offset < 7) {
			if (add_half_db)
				power_idx += 1;

			power_idx &= 0x1f;

			PHYDM_DBG(dm, ODM_COMP_API, "Reg_idx =0x%x\n",
				  power_idx);
			odm_set_bb_reg(dm, R_0x80c, 0x1f00, power_idx); /*r_txagc_offset_a*/
			odm_set_bb_reg(dm, R_0x80c, 0x3e000, power_idx); /*r_txagc_offset_b*/
			set_success = true;
		} else {
			pr_debug("[Warning] TX AGC Offset Setting error!");
		}
	}
	#endif

	return set_success;
}

#ifdef PHYDM_COMMON_API_SUPPORT
boolean
phydm_api_set_txagc(void *dm_void, u32 power_index, enum rf_path path,
		    u8 hw_rate, boolean is_single_rate)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	boolean ret = false;
	#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	u8 i;
	#endif

#if ((RTL8822B_SUPPORT == 1) || (RTL8821C_SUPPORT == 1) || (RTL8195B_SUPPORT == 1))
	if (dm->support_ic_type & (ODM_RTL8822B | ODM_RTL8821C | ODM_RTL8195B)) {
		if (is_single_rate) {
			#if (RTL8822B_SUPPORT == 1)
			if (dm->support_ic_type == ODM_RTL8822B)
				ret = phydm_write_txagc_1byte_8822b(dm, power_index, path, hw_rate);
			#endif

			#if (RTL8821C_SUPPORT == 1)
			if (dm->support_ic_type == ODM_RTL8821C)
				ret = phydm_write_txagc_1byte_8821c(dm, power_index, path, hw_rate);
			#endif

			#if (RTL8195B_SUPPORT == 1)
			if (dm->support_ic_type == ODM_RTL8195B)
				ret = phydm_write_txagc_1byte_8195b(dm, power_index, path, hw_rate);
			#endif

			#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			set_current_tx_agc(dm->priv, path, hw_rate, (u8)power_index);
			#endif

		} else {
			#if (RTL8822B_SUPPORT == 1)
			if (dm->support_ic_type == ODM_RTL8822B)
				ret = config_phydm_write_txagc_8822b(dm, power_index, path, hw_rate);
			#endif

			#if (RTL8821C_SUPPORT == 1)
			if (dm->support_ic_type == ODM_RTL8821C)
				ret = config_phydm_write_txagc_8821c(dm, power_index, path, hw_rate);
			#endif

			#if (RTL8195B_SUPPORT == 1)
			if (dm->support_ic_type == ODM_RTL8195B)
				ret = config_phydm_write_txagc_8195b(dm, power_index, path, hw_rate);
			#endif

			#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
			for (i = 0; i < 4; i++)
				set_current_tx_agc(dm->priv, path, (hw_rate + i), (u8)power_index);
			#endif
		}
	}
#endif

#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8197F)
		ret = config_phydm_write_txagc_8197f(dm, power_index, path, hw_rate);
#endif

/*jj add 20170822*/
#if (RTL8192F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8192F)
		ret = config_phydm_write_txagc_8192f(dm, power_index, path, hw_rate);
#endif
	return ret;
}

u8 phydm_api_get_txagc(void *dm_void, enum rf_path path, u8 hw_rate)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 ret = 0;

#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8822B)
		ret = config_phydm_read_txagc_8822b(dm, path, hw_rate);
#endif

#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8197F)
		ret = config_phydm_read_txagc_8197f(dm, path, hw_rate);
#endif

#if (RTL8821C_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8821C)
		ret = config_phydm_read_txagc_8821c(dm, path, hw_rate);
#endif

#if (RTL8195B_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8195B)
		ret = config_phydm_read_txagc_8195b(dm, path, hw_rate);
#endif

/*jj add 20170822*/
#if (RTL8192F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8192F)
		ret = config_phydm_read_txagc_8192f(dm, path, hw_rate);
#endif
	return ret;
}

boolean
phydm_api_switch_bw_channel(void *dm_void, u8 central_ch, u8 primary_ch_idx,
			    enum channel_width bandwidth)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	boolean ret = false;

#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8822B)
		ret = config_phydm_switch_channel_bw_8822b(dm, central_ch, primary_ch_idx, bandwidth);
#endif

#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8197F)
		ret = config_phydm_switch_channel_bw_8197f(dm, central_ch, primary_ch_idx, bandwidth);
#endif

#if (RTL8821C_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8821C)
		ret = config_phydm_switch_channel_bw_8821c(dm, central_ch, primary_ch_idx, bandwidth);
#endif

/*jj add 20170822*/
#if (RTL8192F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8192F)
		ret = config_phydm_switch_channel_bw_8192f(dm, central_ch, primary_ch_idx, bandwidth);
#endif
	return ret;
}

boolean
phydm_api_trx_mode(void *dm_void, enum bb_path tx_path, enum bb_path rx_path,
		   boolean is_tx2_path)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	boolean ret = false;

#if (RTL8822B_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8822B)
		ret = config_phydm_trx_mode_8822b(dm, tx_path, rx_path, is_tx2_path);
#endif

#if (RTL8197F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8197F)
		ret = config_phydm_trx_mode_8197f(dm, tx_path, rx_path, is_tx2_path);
#endif

/*jj add 20170822*/
#if (RTL8192F_SUPPORT == 1)
	if (dm->support_ic_type & ODM_RTL8192F)
		ret = config_phydm_trx_mode_8192f(dm, tx_path, rx_path, is_tx2_path);
#endif
	return ret;
}
#else
u8 config_phydm_read_txagc_n(
	void *dm_void,
	enum rf_path path,
	u8 hw_rate)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 read_back_data = INVALID_TXAGC_DATA;
	u32 reg_txagc;
	u32 reg_mask;
	/* This function is for 92E/88E etc... */
	/* Input need to be HW rate index, not driver rate index!!!! */

	/* Error handling */
	if (path > RF_PATH_B || hw_rate > ODM_RATEMCS15) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s: unsupported path (%d)\n",
			  __func__, path);
		return INVALID_TXAGC_DATA;
	}

	if (path == RF_PATH_A) {
		switch (hw_rate) {
		case ODM_RATE1M:
			reg_txagc = R_0xe08;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATE2M:
			reg_txagc = R_0x86c;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATE5_5M:
			reg_txagc = R_0x86c;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATE11M:
			reg_txagc = R_0x86c;
			reg_mask = 0x7f000000;
			break;

		case ODM_RATE6M:
			reg_txagc = R_0xe00;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATE9M:
			reg_txagc = R_0xe00;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATE12M:
			reg_txagc = R_0xe00;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATE18M:
			reg_txagc = R_0xe00;
			reg_mask = 0x7f000000;
			break;
		case ODM_RATE24M:
			reg_txagc = R_0xe04;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATE36M:
			reg_txagc = R_0xe04;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATE48M:
			reg_txagc = R_0xe04;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATE54M:
			reg_txagc = R_0xe04;
			reg_mask = 0x7f000000;
			break;

		case ODM_RATEMCS0:
			reg_txagc = R_0xe10;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS1:
			reg_txagc = R_0xe10;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS2:
			reg_txagc = R_0xe10;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS3:
			reg_txagc = R_0xe10;
			reg_mask = 0x7f000000;
			break;
		case ODM_RATEMCS4:
			reg_txagc = R_0xe14;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS5:
			reg_txagc = R_0xe14;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS6:
			reg_txagc = R_0xe14;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS7:
			reg_txagc = R_0xe14;
			reg_mask = 0x7f000000;
			break;

		case ODM_RATEMCS8:
			reg_txagc = R_0xe18;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS9:
			reg_txagc = R_0xe18;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS10:
			reg_txagc = R_0xe18;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS11:
			reg_txagc = R_0xe18;
			reg_mask = 0x7f000000;
			break;
		case ODM_RATEMCS12:
			reg_txagc = R_0xe1c;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS13:
			reg_txagc = R_0xe1c;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS14:
			reg_txagc = R_0xe1c;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS15:
			reg_txagc = R_0xe1c;
			reg_mask = 0x7f000000;
			break;

		default:
			PHYDM_DBG(dm, ODM_PHY_CONFIG, "Invalid HWrate!\n");
			break;
		}
	} else if (path == RF_PATH_B) {
		switch (hw_rate) {
		case ODM_RATE1M:
			reg_txagc = R_0x838;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATE2M:
			reg_txagc = R_0x838;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATE5_5M:
			reg_txagc = R_0x838;
			reg_mask = 0x7f000000;
			break;
		case ODM_RATE11M:
			reg_txagc = R_0x86c;
			reg_mask = 0x0000007f;
			break;

		case ODM_RATE6M:
			reg_txagc = R_0x830;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATE9M:
			reg_txagc = R_0x830;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATE12M:
			reg_txagc = R_0x830;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATE18M:
			reg_txagc = R_0x830;
			reg_mask = 0x7f000000;
			break;
		case ODM_RATE24M:
			reg_txagc = R_0x834;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATE36M:
			reg_txagc = R_0x834;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATE48M:
			reg_txagc = R_0x834;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATE54M:
			reg_txagc = R_0x834;
			reg_mask = 0x7f000000;
			break;

		case ODM_RATEMCS0:
			reg_txagc = R_0x83c;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS1:
			reg_txagc = R_0x83c;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS2:
			reg_txagc = R_0x83c;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS3:
			reg_txagc = R_0x83c;
			reg_mask = 0x7f000000;
			break;
		case ODM_RATEMCS4:
			reg_txagc = R_0x848;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS5:
			reg_txagc = R_0x848;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS6:
			reg_txagc = R_0x848;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS7:
			reg_txagc = R_0x848;
			reg_mask = 0x7f000000;
			break;

		case ODM_RATEMCS8:
			reg_txagc = R_0x84c;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS9:
			reg_txagc = R_0x84c;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS10:
			reg_txagc = R_0x84c;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS11:
			reg_txagc = R_0x84c;
			reg_mask = 0x7f000000;
			break;
		case ODM_RATEMCS12:
			reg_txagc = R_0x868;
			reg_mask = 0x0000007f;
			break;
		case ODM_RATEMCS13:
			reg_txagc = R_0x868;
			reg_mask = 0x00007f00;
			break;
		case ODM_RATEMCS14:
			reg_txagc = R_0x868;
			reg_mask = 0x007f0000;
			break;
		case ODM_RATEMCS15:
			reg_txagc = R_0x868;
			reg_mask = 0x7f000000;
			break;

		default:
			PHYDM_DBG(dm, ODM_PHY_CONFIG, "Invalid HWrate!\n");
			break;
		}
	} else
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Invalid RF path!!\n");
	read_back_data = (u8)odm_get_bb_reg(dm, reg_txagc, reg_mask);
	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s: path-%d rate index 0x%x = 0x%x\n",
		  __func__, path, hw_rate, read_back_data);
	return read_back_data;
}
#endif

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
void phydm_normal_driver_rx_sniffer(
	struct dm_struct *dm,
	u8 *desc,
	PRT_RFD_STATUS rt_rfd_status,
	u8 *drv_info,
	u8 phy_status)
{
#if (defined(CONFIG_PHYDM_RX_SNIFFER_PARSING))
	u32 *msg;
	u16 seq_num;
	struct phydm_fat_struct *fat_tab = &dm->dm_fat_table;

	if (rt_rfd_status->packet_report_type != NORMAL_RX)
		return;

	if (!dm->is_linked) {
		if (rt_rfd_status->is_hw_error)
			return;
	}

	if (!(fat_tab->fat_state == FAT_TRAINING_STATE))
		return;

	if (phy_status == true) {
		if (dm->rx_pkt_type == type_block_ack || dm->rx_pkt_type == type_rts || dm->rx_pkt_type == type_cts)
			seq_num = 0;
		else
			seq_num = rt_rfd_status->seq_num;

		PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, "%04d , %01s, rate=0x%02x, L=%04d , %s , %s",
			    seq_num,
			    /*rt_rfd_status->mac_id,*/
			    ((rt_rfd_status->is_crc) ? "C" : (rt_rfd_status->is_ampdu) ? "A" : "_"),
			    rt_rfd_status->data_rate,
			    rt_rfd_status->length,
			    ((rt_rfd_status->band_width == 0) ? "20M" : ((rt_rfd_status->band_width == 1) ? "40M" : "80M")),
			    ((rt_rfd_status->is_ldpc) ? "LDP" : "BCC"));

		if (dm->rx_pkt_type == type_asoc_req) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "AS_REQ");
			/**/
		} else if (dm->rx_pkt_type == type_asoc_rsp) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "AS_RSP");
			/**/
		} else if (dm->rx_pkt_type == type_probe_req) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "PR_REQ");
			/**/
		} else if (dm->rx_pkt_type == type_probe_rsp) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "PR_RSP");
			/**/
		} else if (dm->rx_pkt_type == type_deauth) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "DEAUTH");
			/**/
		} else if (dm->rx_pkt_type == type_beacon) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "BEACON");
			/**/
		} else if (dm->rx_pkt_type == type_block_ack_req) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "BA_REQ");
			/**/
		} else if (dm->rx_pkt_type == type_rts) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "__RTS_");
			/**/
		} else if (dm->rx_pkt_type == type_cts) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "__CTS_");
			/**/
		} else if (dm->rx_pkt_type == type_ack) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "__ACK_");
			/**/
		} else if (dm->rx_pkt_type == type_block_ack) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "__BA__");
			/**/
		} else if (dm->rx_pkt_type == type_data) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "_DATA_");
			/**/
		} else if (dm->rx_pkt_type == type_data_ack) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "Data_Ack");
			/**/
		} else if (dm->rx_pkt_type == type_qos_data) {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [%s]", "QoS_Data");
			/**/
		} else {
			PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [0x%x]", dm->rx_pkt_type);
			/**/
		}

		PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , [RSSI=%d,%d,%d,%d ]",
			    dm->rssi_a,
			    dm->rssi_b,
			    dm->rssi_c,
			    dm->rssi_d);

		msg = (u32 *)drv_info;

		PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, " , P-STS[28:0]=%08x-%08x-%08x-%08x-%08x-%08x-%08x\n",
			    msg[6], msg[5], msg[4], msg[3], msg[2], msg[1], msg[1]);
	} else {
		PHYDM_DBG_F(dm, ODM_COMP_SNIFFER, "%04d , %01s, rate=0x%02x, L=%04d , %s , %s\n",
			    rt_rfd_status->seq_num,
			    /*rt_rfd_status->mac_id,*/
			    ((rt_rfd_status->is_crc) ? "C" : (rt_rfd_status->is_ampdu) ? "A" : "_"),
			    rt_rfd_status->data_rate,
			    rt_rfd_status->length,
			    ((rt_rfd_status->band_width == 0) ? "20M" : ((rt_rfd_status->band_width == 1) ? "40M" : "80M")),
			    ((rt_rfd_status->is_ldpc) ? "LDP" : "BCC"));
	}

#endif
}
#endif
