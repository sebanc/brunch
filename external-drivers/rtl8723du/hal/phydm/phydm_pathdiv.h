/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__PHYDMPATHDIV_H__
#define    __PHYDMPATHDIV_H__

#define PATHDIV_VERSION	"3.1" /*2015.07.29 by YuChen*/

void
phydm_c2h_dtp_handler(
	void	*p_dm_void,
	u8   *cmd_buf,
	u8	cmd_len
);

void
phydm_path_diversity_init(
	void	*p_dm_void
);

void
odm_path_diversity(
	void	*p_dm_void
);

void
odm_pathdiv_debug(
	void		*p_dm_void,
	u32		*const dm_value,
	u32		*_used,
	char		*output,
	u32		*_out_len
);

#endif		 /* #ifndef  __ODMPATHDIV_H__ */
