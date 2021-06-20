/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__PHYDMACS_H__
#define    __PHYDMACS_H__

#define ACS_VERSION	"1.1"	/*20150729 by YuChen*/
#define CLM_VERSION "1.0"

#define ODM_MAX_CHANNEL_2G			14
#define ODM_MAX_CHANNEL_5G			24

/* For phydm_auto_channel_select_setting_ap() */
#define STORE_DEFAULT_NHM_SETTING               0
#define RESTORE_DEFAULT_NHM_SETTING             1
#define ACS_NHM_SETTING                         2

struct _ACS_ {
	bool		is_force_acs_result;
	u8		clean_channel_2g;
	u8		clean_channel_5g;
	u16		channel_info_2g[2][ODM_MAX_CHANNEL_2G];		/* Channel_Info[1]: channel score, Channel_Info[2]:Channel_Scan_Times */
	u16		channel_info_5g[2][ODM_MAX_CHANNEL_5G];
};

void
odm_auto_channel_select_init(
	void			*p_dm_void
);

void
odm_auto_channel_select_reset(
	void			*p_dm_void
);

void
odm_auto_channel_select(
	void			*p_dm_void,
	u8			channel
);

u8
odm_get_auto_channel_select_result(
	void			*p_dm_void,
	u8			band
);

bool
phydm_acs_check(
	void	*p_dm_void
);

#endif  /* #ifndef	__PHYDMACS_H__ */
