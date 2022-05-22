/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */


#ifndef	__PHYDM_PRIMARYCCA_H__
#define	__PHYDM_PRIMARYCCA_H__

#define PRIMARYCCA_VERSION	"1.0"  /*2017.03.23, Dino*/

/*============================================================*/
/*Definition */
/*============================================================*/

#define	SECOND_CH_AT_LSB	2	/*primary CH @ MSB,  SD4: HAL_PRIME_CHNL_OFFSET_UPPER*/
#define	SECOND_CH_AT_USB	1	/*primary CH @ LSB,   SD4: HAL_PRIME_CHNL_OFFSET_LOWER*/
#define	OFDMCCA_TH	500
#define	bw_ind_bias		500
#define	PRI_CCA_MONITOR_TIME	30

#ifdef PHYDM_PRIMARY_CCA

/*============================================================*/
/*structure and define*/
/*============================================================*/
enum	primary_cca_ch_position {  /*N-series REG0xc6c[8:7]*/
	MF_USC_LSC = 0,
	MF_LSC = 1,
	MF_USC = 2
};

struct phydm_pricca_struct {

	#if (RTL8188E_SUPPORT == 1) || (RTL8192E_SUPPORT == 1)
	u8	pri_cca_flag;
	u8	intf_flag;
	u8	intf_type;
	u8	monitor_flag;
	u8	ch_offset;
	#endif
	u8	dup_rts_flag;
	u8	cca_th_40m_bkp; /*c84[31:28]*/
	enum channel_width	pre_bw;
	u8	pri_cca_is_become_linked;
	u8	mf_state;
};

/*============================================================*/
/*function prototype*/
/*============================================================*/

#endif /*#ifdef PHYDM_PRIMARY_CCA*/


bool
odm_dynamic_primary_cca_dup_rts(
	void			*p_dm_void
);

void
phydm_primary_cca_init(
	void			*p_dm_void
);

void
phydm_primary_cca(
	void			*p_dm_void
);


#endif /*#ifndef	__PHYDM_PRIMARYCCA_H__*/
