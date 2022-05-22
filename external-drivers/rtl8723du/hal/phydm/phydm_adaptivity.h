/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__PHYDMADAPTIVITY_H__
#define    __PHYDMADAPTIVITY_H__

#define ADAPTIVITY_VERSION	"9.5.7"	/*20170627 changed by Kevin, move adapt_igi_up from phydm.h to phydm_adaptivity.h*/

#define pwdb_upper_bound	7
#define dfir_loss	7

enum phydm_adapinfo_e {
	PHYDM_ADAPINFO_CARRIER_SENSE_ENABLE = 0,
	PHYDM_ADAPINFO_DCBACKOFF,
	PHYDM_ADAPINFO_DYNAMICLINKADAPTIVITY,
	PHYDM_ADAPINFO_TH_L2H_INI,
	PHYDM_ADAPINFO_TH_EDCCA_HL_DIFF,
	PHYDM_ADAPINFO_AP_NUM_TH
};

enum phydm_set_lna {
	phydm_disable_lna		= 0,
	phydm_enable_lna		= 1,
};

enum phydm_trx_mux_type {
	phydm_shutdown			= 0,
	phydm_standby_mode		= 1,
	phydm_tx_mode			= 2,
	phydm_rx_mode			= 3
};

enum phydm_mac_edcca_type {
	phydm_ignore_edcca			= 0,
	phydm_dont_ignore_edcca		= 1
};

enum phydm_adaptivity_mode {
	PHYDM_ADAPT_MSG			= 0,
	PHYDM_ADAPT_DEBUG		= 1,
	PHYDM_ADAPT_RESUME		= 2,
	PHYDM_EDCCA_TH_PAUSE	= 3,
	PHYDM_EDCCA_RESUME		= 4
};

struct phydm_adaptivity_struct {
	s8			th_l2h_ini_backup;
	s8			th_edcca_hl_diff_backup;
	s8			igi_base;
	u8			igi_target;
	s8			h2l_lb;
	s8			l2h_lb;
	bool		is_check;
	bool		dynamic_link_adaptivity;
	u8			ap_num_th;
	u8			adajust_igi_level;
	s8			backup_l2h;
	s8			backup_h2l;
	bool			is_stop_edcca;
	u32			adaptivity_dbg_port; /*N:0x208, AC:0x209*/
	u8			debug_mode;
	s8			th_l2h_ini_debug;
	u16			igi_up_bound_lmt_cnt;	/*When igi_up_bound_lmt_cnt !=0, limit IGI upper bound to "adapt_igi_up"*/
	u16			igi_up_bound_lmt_val;	/*max value of igi_up_bound_lmt_cnt*/
	bool		igi_lmt_en;
	u8			adapt_igi_up;
	s8			rvrt_val[2];
	s8			th_l2h;
	s8			th_h2l;
};

void
phydm_pause_edcca(
	void	*p_dm_void,
	bool	is_pasue_edcca
);

void
phydm_check_environment(
	void					*p_dm_void
);

void
phydm_mac_edcca_state(
	void					*p_dm_void,
	enum phydm_mac_edcca_type		state
);

void
phydm_set_edcca_threshold(
	void		*p_dm_void,
	s8		H2L,
	s8		L2H
);

void
phydm_set_trx_mux(
	void			*p_dm_void,
	enum phydm_trx_mux_type			tx_mode,
	enum phydm_trx_mux_type			rx_mode
);

void
phydm_search_pwdb_lower_bound(
	void					*p_dm_void
);

void
phydm_adaptivity_info_init(
	void			*p_dm_void,
	enum phydm_adapinfo_e	cmn_info,
	u32				value
);

void
phydm_adaptivity_init(
	void					*p_dm_void
);

void
phydm_adaptivity(
	void			*p_dm_void
);

void
phydm_set_edcca_threshold_api(
	void	*p_dm_void,
	u8	IGI
);

void
phydm_pause_edcca_work_item_callback(
	void			*p_dm_void
);

void
phydm_resume_edcca_work_item_callback(
	void			*p_dm_void
);

void
phydm_adaptivity_debug(
	void		*p_dm_void,
	u32		*const dm_value,
	u32		*_used,
	char		*output,
	u32		*_out_len
);

void
phydm_set_l2h_th_ini(
	void		*p_dm_void
);

void
phydm_set_forgetting_factor(
	void		*p_dm_void
);

void
phydm_set_pwdb_mode(
	void		*p_dm_void
);

void
phydm_set_edcca_val(
	void			*p_dm_void,
	u32			*val_buf,
	u8			val_len
);

#endif
