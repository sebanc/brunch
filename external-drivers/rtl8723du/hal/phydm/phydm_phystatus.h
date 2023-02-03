/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__PHYDM_PHYSTATUS_H__
#define	__PHYDM_PHYSTATUS_H__


/*--------------------------Define -------------------------------------------*/
#define	CCK_RSSI_INIT_COUNT 5

#define	RA_RSSI_STATE_INIT	0
#define	RA_RSSI_STATE_SEND	1
#define	RA_RSSI_STATE_HOLD	2

#define	CFO_HW_RPT_2_MHZ(val) ((val<<1) + (val>>1))
/* ((X* 3125)  / 10)>>7 = (X*10)>>2 = X*2.5 = X<<1 + X>>1  */

/* ************************************************************
 * structure and define
 * ************************************************************ */

__PACK struct _phy_rx_agc_info {
#if defined(__LITTLE_ENDIAN)
	u8	gain: 7, trsw: 1;
#else
	u8	trsw: 1, gain: 7;
#endif
};

__PACK struct _phy_status_rpt_8192cd {
	struct _phy_rx_agc_info path_agc[2];
	u8	ch_corr[2];
	u8	cck_sig_qual_ofdm_pwdb_all;
	u8	cck_agc_rpt_ofdm_cfosho_a;
	u8	cck_rpt_b_ofdm_cfosho_b;
	u8	rsvd_1;/*ch_corr_msb;*/
	u8	noise_power_db_msb;
	s8	path_cfotail[2];
	u8	pcts_mask[2];
	s8	stream_rxevm[2];
	u8	path_rxsnr[2];
	u8	noise_power_db_lsb;
	u8	rsvd_2[3];
	u8	stream_csi[2];
	u8	stream_target_csi[2];
	s8	sig_evm;
	u8	rsvd_3;

#if defined(__LITTLE_ENDIAN)
	u8	antsel_rx_keep_2: 1;	/*ex_intf_flg:1;*/
	u8	sgi_en: 1;
	u8	rxsc: 2;
	u8	idle_long: 1;
	u8	r_ant_train_en: 1;
	u8	ant_sel_b: 1;
	u8	ant_sel: 1;
#else	/* __BIG_ENDIAN */
	u8	ant_sel: 1;
	u8	ant_sel_b: 1;
	u8	r_ant_train_en: 1;
	u8	idle_long: 1;
	u8	rxsc: 2;
	u8	sgi_en: 1;
	u8	antsel_rx_keep_2: 1;/*ex_intf_flg:1;*/
#endif
};

struct _phy_status_rpt_8812 {
	/*	DWORD 0*/
	u8			gain_trsw[2];							/*path-A and path-B {TRSW, gain[6:0] }*/
	u8			chl_num_LSB;							/*channel number[7:0]*/
#if defined(__LITTLE_ENDIAN)
	u8			chl_num_MSB: 2;							/*channel number[9:8]*/
	u8			sub_chnl: 4;								/*sub-channel location[3:0]*/
	u8			r_RFMOD: 2;								/*RF mode[1:0]*/
#else	/*__BIG_ENDIAN	*/
	u8			r_RFMOD: 2;
	u8			sub_chnl: 4;
	u8			chl_num_MSB: 2;
#endif

	/*	DWORD 1*/
	u8			pwdb_all;								/*CCK signal quality / OFDM pwdb all*/
	s8			cfosho[2];		/*DW1 byte 1 DW1 byte2	CCK AGC report and CCK_BB_Power / OFDM path-A and path-B short CFO*/
#if defined(__LITTLE_ENDIAN)
	/*this should be checked again because the definition of 8812 and 8814 is different*/
	/*	u8			r_cck_rx_enable_pathc:2;					cck rx enable pathc[1:0]*/
	/*	u8			cck_rx_path:4;							cck rx path[3:0]*/
	u8			resvd_0: 6;
	u8			bt_RF_ch_MSB: 2;						/*8812A:2'b0			8814A: bt rf channel keep[7:6]*/
#else	/*_BIG_ENDIAN_*/
	u8			bt_RF_ch_MSB: 2;
	u8			resvd_0: 6;
#endif

	/*	DWORD 2*/
#if defined(__LITTLE_ENDIAN)
	u8			ant_div_sw_a: 1;							/*8812A: ant_div_sw_a    8814A: 1'b0*/
	u8			ant_div_sw_b: 1;							/*8812A: ant_div_sw_b    8814A: 1'b0*/
	u8			bt_RF_ch_LSB: 6;						/*8812A: 6'b0                   8814A: bt rf channel keep[5:0]*/
#else	/*_BIG_ENDIAN_	*/
	u8			bt_RF_ch_LSB: 6;
	u8			ant_div_sw_b: 1;
	u8			ant_div_sw_a: 1;
#endif
	s8			cfotail[2];		   /*DW2 byte 1 DW2 byte 2	path-A and path-B CFO tail*/
	u8			PCTS_MSK_RPT_0;						/*PCTS mask report[7:0]*/
	u8			PCTS_MSK_RPT_1;						/*PCTS mask report[15:8]*/

	/*	DWORD 3*/
	s8			rxevm[2];	         /*DW3 byte 1 DW3 byte 2	stream 1 and stream 2 RX EVM*/
	s8			rxsnr[2];	         /*DW3 byte 3 DW4 byte 0	path-A and path-B RX SNR*/

	/*	DWORD 4*/
	u8			PCTS_MSK_RPT_2;						/*PCTS mask report[23:16]*/
#if defined(__LITTLE_ENDIAN)
	u8			PCTS_MSK_RPT_3: 6;						/*PCTS mask report[29:24]*/
	u8			pcts_rpt_valid: 1;							/*pcts_rpt_valid*/
	u8			resvd_1: 1;								/*1'b0*/
#else	/*_BIG_ENDIAN_*/
	u8			resvd_1: 1;
	u8			pcts_rpt_valid: 1;
	u8			PCTS_MSK_RPT_3: 6;
#endif
	s8			rxevm_cd[2];	   /*DW 4 byte 3 DW5 byte 0  8812A: 16'b0	8814A: stream 3 and stream 4 RX EVM*/

	/*	DWORD 5*/
	u8			csi_current[2];	   /*DW5 byte 1 DW5 byte 2	8812A: stream 1 and 2 CSI	8814A:  path-C and path-D RX SNR*/
	u8			gain_trsw_cd[2];	   /*DW5 byte 3 DW6 byte 0	path-C and path-D {TRSW, gain[6:0] }*/

	/*	DWORD 6*/
	s8			sigevm;									/*signal field EVM*/
#if defined(__LITTLE_ENDIAN)
	u8			antidx_antc: 3;							/*8812A: 3'b0		8814A: antidx_antc[2:0]*/
	u8			antidx_antd: 3;							/*8812A: 3'b0		8814A: antidx_antd[2:0]*/
	u8			dpdt_ctrl_keep: 1;						/*8812A: 1'b0		8814A: dpdt_ctrl_keep*/
	u8			GNT_BT_keep: 1;							/*8812A: 1'b0		8814A: GNT_BT_keep*/
#else	/*_BIG_ENDIAN_*/
	u8			GNT_BT_keep: 1;
	u8			dpdt_ctrl_keep: 1;
	u8			antidx_antd: 3;
	u8			antidx_antc: 3;
#endif
#if defined(__LITTLE_ENDIAN)
	u8			antidx_anta: 3;							/*antidx_anta[2:0]*/
	u8			antidx_antb: 3;							/*antidx_antb[2:0]*/
	u8			hw_antsw_occur: 2;								/*1'b0*/
#else	/*_BIG_ENDIAN_*/
	u8			hw_antsw_occur: 2;
	u8			antidx_antb: 3;
	u8			antidx_anta: 3;
#endif
};


__PACK struct _phy_status_rpt_jaguar2_type0 {
	/* DW0 */
	u8		page_num;
	u8		pwdb;
#if defined(__LITTLE_ENDIAN)
	u8		gain: 6;
	u8		rsvd_0: 1;
	u8		trsw: 1;
#else
	u8		trsw: 1;
	u8		rsvd_0: 1;
	u8		gain: 6;
#endif
	u8		rsvd_1;

	/* DW1 */
	u8		rsvd_2;
#if defined(__LITTLE_ENDIAN)
	u8		rxsc: 4;
	u8		agc_table: 4;
#else
	u8		agc_table: 4;
	u8		rxsc: 4;
#endif
	u8		channel;
	u8		band;

	/* DW2 */
	u16		length;
#if defined(__LITTLE_ENDIAN)
	u8		antidx_a: 3;
	u8		antidx_b: 3;
	u8		rsvd_3: 2;
	u8		antidx_c: 3;
	u8		antidx_d: 3;
	u8		rsvd_4:2;
#else
	u8		rsvd_3: 2;
	u8		antidx_b: 3;
	u8		antidx_a: 3;
	u8		rsvd_4:2;
	u8		antidx_d: 3;
	u8		antidx_c: 3;
#endif

	/* DW3 */
	u8		signal_quality;
#if defined(__LITTLE_ENDIAN)
	u8		vga:5;
	u8		lna_l:3;
	u8		bb_power:6;
	u8		rsvd_9:1;
	u8		lna_h:1;
#else
	u8		lna_l:3;
	u8		vga:5;
	u8		lna_h:1;
	u8		rsvd_9:1;
	u8		bb_power:6;
#endif
	u8		rsvd_5;

	/* DW4 */
	u32		rsvd_6;

	/* DW5 */
	u32		rsvd_7;

	/* DW6 */
	u32		rsvd_8;
};

__PACK struct _phy_status_rpt_jaguar2_type1 {
	/* DW0 and DW1 */
	u8		page_num;
	u8		pwdb[4];
#if defined(__LITTLE_ENDIAN)
	u8		l_rxsc: 4;
	u8		ht_rxsc: 4;
#else
	u8		ht_rxsc: 4;
	u8		l_rxsc: 4;
#endif
	u8		channel;
#if defined(__LITTLE_ENDIAN)
	u8		band: 2;
	u8		rsvd_0: 1;
	u8		hw_antsw_occu: 1;
	u8		gnt_bt: 1;
	u8		ldpc: 1;
	u8		stbc: 1;
	u8		beamformed: 1;
#else
	u8		beamformed: 1;
	u8		stbc: 1;
	u8		ldpc: 1;
	u8		gnt_bt: 1;
	u8		hw_antsw_occu: 1;
	u8		rsvd_0: 1;
	u8		band: 2;
#endif

	/* DW2 */
	u16		lsig_length;
#if defined(__LITTLE_ENDIAN)
	u8		antidx_a: 3;
	u8		antidx_b: 3;
	u8		rsvd_1: 2;
	u8		antidx_c: 3;
	u8		antidx_d: 3;
	u8		rsvd_2: 2;
#else
	u8		rsvd_1: 2;
	u8		antidx_b: 3;
	u8		antidx_a: 3;
	u8		rsvd_2: 2;
	u8		antidx_d: 3;
	u8		antidx_c: 3;
#endif

	/* DW3 */
	u8		paid;
#if defined(__LITTLE_ENDIAN)
	u8		paid_msb: 1;
	u8		gid: 6;
	u8		rsvd_3: 1;
#else
	u8		rsvd_3: 1;
	u8		gid: 6;
	u8		paid_msb: 1;
#endif
	u8		intf_pos;
#if defined(__LITTLE_ENDIAN)
	u8		intf_pos_msb: 1;
	u8		rsvd_4: 2;
	u8		nb_intf_flag: 1;
	u8		rf_mode: 2;
	u8		rsvd_5: 2;
#else
	u8		rsvd_5: 2;
	u8		rf_mode: 2;
	u8		nb_intf_flag: 1;
	u8		rsvd_4: 2;
	u8		intf_pos_msb: 1;
#endif

	/* DW4 */
	s8		rxevm[4];			/* s(8,1) */

	/* DW5 */
	s8		cfo_tail[4];			/* s(8,7) */

	/* DW6 */
	s8		rxsnr[4];			/* s(8,1) */
};

__PACK struct _phy_status_rpt_jaguar2_type2 {
	/* DW0 ane DW1 */
	u8		page_num;
	u8		pwdb[4];
#if defined(__LITTLE_ENDIAN)
	u8		l_rxsc: 4;
	u8		ht_rxsc: 4;
#else
	u8		ht_rxsc: 4;
	u8		l_rxsc: 4;
#endif
	u8		channel;
#if defined(__LITTLE_ENDIAN)
	u8		band: 2;
	u8		rsvd_0: 1;
	u8		hw_antsw_occu: 1;
	u8		gnt_bt: 1;
	u8		ldpc: 1;
	u8		stbc: 1;
	u8		beamformed: 1;
#else
	u8		beamformed: 1;
	u8		stbc: 1;
	u8		ldpc: 1;
	u8		gnt_bt: 1;
	u8		hw_antsw_occu: 1;
	u8		rsvd_0: 1;
	u8		band: 2;
#endif

	/* DW2 */
#if defined(__LITTLE_ENDIAN)
	u8		shift_l_map: 6;
	u8		rsvd_1: 2;
#else
	u8		rsvd_1: 2;
	u8		shift_l_map: 6;
#endif
	u8		cnt_pw2cca;
#if defined(__LITTLE_ENDIAN)
	u8		agc_table_a: 4;
	u8		agc_table_b: 4;
	u8		agc_table_c: 4;
	u8		agc_table_d: 4;
#else
	u8		agc_table_b: 4;
	u8		agc_table_a: 4;
	u8		agc_table_d: 4;
	u8		agc_table_c: 4;
#endif

	/* DW3 ~ DW6*/
	u8		cnt_cca2agc_rdy;
#if defined(__LITTLE_ENDIAN)
	u8		gain_a: 6;
	u8		rsvd_2: 1;
	u8		trsw_a: 1;
	u8		gain_b: 6;
	u8		rsvd_3: 1;
	u8		trsw_b: 1;
	u8		gain_c: 6;
	u8		rsvd_4: 1;
	u8		trsw_c: 1;
	u8		gain_d: 6;
	u8		rsvd_5: 1;
	u8		trsw_d: 1;
	u8		aagc_step_a: 2;
	u8		aagc_step_b: 2;
	u8		aagc_step_c: 2;
	u8		aagc_step_d: 2;
#else
	u8		trsw_a: 1;
	u8		rsvd_2: 1;
	u8		gain_a: 6;
	u8		trsw_b: 1;
	u8		rsvd_3: 1;
	u8		gain_b: 6;
	u8		trsw_c: 1;
	u8		rsvd_4: 1;
	u8		gain_c: 6;
	u8		trsw_d: 1;
	u8		rsvd_5: 1;
	u8		gain_d: 6;
	u8		aagc_step_d: 2;
	u8		aagc_step_c: 2;
	u8		aagc_step_b: 2;
	u8		aagc_step_a: 2;
#endif
	u8		ht_aagc_gain[4];
	u8		dagc_gain[4];
#if defined(__LITTLE_ENDIAN)
	u8		counter: 6;
	u8		rsvd_6: 2;
	u8		syn_count: 5;
	u8		rsvd_7:3;
#else
	u8		rsvd_6: 2;
	u8		counter: 6;
	u8		rsvd_7:3;
	u8		syn_count: 5;
#endif
};
/*==============================================*/

void
phydm_rx_phy_status_new_type(
	void						*p_dm_void,
	u8						*p_phy_status,
	struct phydm_perpkt_info_struct			*p_pktinfo,
	struct phydm_phyinfo_struct			*p_phy_info
);

bool
phydm_query_is_mu_api(
	struct PHY_DM_STRUCT			*p_phydm,
	u8								ppdu_idx,
	u8								*p_data_rate,
	u8								*p_gid
);

void
phydm_reset_phystatus_avg(
	struct PHY_DM_STRUCT	*p_dm
);

void
phydm_reset_phystatus_statistic(
	struct PHY_DM_STRUCT	*p_dm
);

void
phydm_reset_rssi_for_dm(
	struct PHY_DM_STRUCT	*p_dm,
	u8		station_id
);

void
phydm_get_cck_rssi_table_from_reg(
	struct PHY_DM_STRUCT	*p_dm
);

u8
phydm_rate_to_num_ss(
	struct PHY_DM_STRUCT		*p_dm,
	u8			data_rate
);

s32
phydm_signal_scale_mapping(
	struct PHY_DM_STRUCT *p_dm,
	s32 curr_sig
);

void
odm_phy_status_query(
	struct PHY_DM_STRUCT					*p_dm,
	struct phydm_phyinfo_struct			*p_phy_info,
	u8						*p_phy_status,
	struct phydm_perpkt_info_struct			*p_pktinfo
);

void
phydm_rx_phy_status_init(
	void			*p_dm_void
);

#endif /*#ifndef	__HALHWOUTSRC_H__*/
