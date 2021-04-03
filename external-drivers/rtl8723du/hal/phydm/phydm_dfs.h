/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __PHYDM_DFS_H__
#define __PHYDM_DFS_H__

#define DFS_VERSION	"1.1"

/* ============================================================
  Definition
 ============================================================
*/

/*
============================================================
1  structure
 ============================================================
*/

struct _DFS_STATISTICS {
	u8			mask_idx;
	u8			igi_cur;
	u8			igi_pre;
	u8			st_l2h_cur;
	u16			fa_count_pre;
	u16			fa_inc_hist[5];	
	u16			vht_crc_ok_cnt_pre;
	u16			ht_crc_ok_cnt_pre;
	u16			leg_crc_ok_cnt_pre;
	u16			short_pulse_cnt_pre;
	u16			long_pulse_cnt_pre;
	u8			pwdb_th;
	u8			pwdb_th_cur;
	u8			pwdb_scalar_factor;	
	u8			peak_th;
	u8			short_pulse_cnt_th;
	u8			long_pulse_cnt_th;
	u8			peak_window;
	u8			nb2wb_th;
	u8			fa_mask_th;
	u8			det_flag_offset;
	u8			st_l2h_max;
	u8			st_l2h_min;
	u8			mask_hist_checked;
	bool		pulse_flag_hist[5];
	bool		radar_det_mask_hist[5];
	bool		idle_mode;
	bool		force_TP_mode;
	bool		dbg_mode;
	bool		det_print;
	bool		det_print2;
};


/* ============================================================
  enumeration
 ============================================================
*/

enum phydm_dfs_region_domain {
	PHYDM_DFS_DOMAIN_UNKNOWN = 0,
	PHYDM_DFS_DOMAIN_FCC = 1,
	PHYDM_DFS_DOMAIN_MKK = 2,
	PHYDM_DFS_DOMAIN_ETSI = 3,
};

/*
============================================================
  function prototype
============================================================
*/
#if defined(CONFIG_PHYDM_DFS_MASTER)
void phydm_radar_detect_reset(void *p_dm_void);
void phydm_radar_detect_disable(void *p_dm_void);
void phydm_radar_detect_enable(void *p_dm_void);
bool phydm_radar_detect(void *p_dm_void);
void phydm_dfs_parameter_init(void *p_dm_void);
void phydm_dfs_debug(void *p_dm_void, u32 *const argv, u32 *_used, char *output, u32 *_out_len);
#endif /* defined(CONFIG_PHYDM_DFS_MASTER) */

bool 
phydm_dfs_is_meteorology_channel(
	void		*p_dm_void
);

bool
phydm_is_dfs_band(
	void		*p_dm_void
);

bool
phydm_dfs_master_enabled(
	void		*p_dm_void
);

#endif /*#ifndef __PHYDM_DFS_H__ */
