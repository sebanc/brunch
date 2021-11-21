// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "phydm_precomp.h"

/* ******************************************************
 * when antenna test utility is on or some testing need to disable antenna diversity
 * call this function to disable all ODM related mechanisms which will switch antenna.
 * ****************************************************** */
void
odm_stop_antenna_switch_dm(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	/* disable ODM antenna diversity */
	p_dm->support_ability &= ~ODM_BB_ANT_DIV;
	odm_ant_div_on_off(p_dm, ANTDIV_OFF);
	odm_tx_by_tx_desc_or_reg(p_dm, TX_BY_REG);
	PHYDM_DBG(p_dm, DBG_ANT_DIV, ("STOP Antenna Diversity\n"));
}

void
phydm_enable_antenna_diversity(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	p_dm->support_ability |= ODM_BB_ANT_DIV;
	p_dm->antdiv_select = 0;
	PHYDM_DBG(p_dm, DBG_ANT_DIV, ("AntDiv is enabled & Re-Init AntDiv\n"));
	odm_antenna_diversity_init(p_dm);
}

void
odm_set_ant_config(
	void	*p_dm_void,
	u8		ant_setting	/* 0=A, 1=B, 2=C, .... */
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	if (p_dm->support_ic_type == ODM_RTL8723B) {
		if (ant_setting == 0)		/* ant A*/
			odm_set_bb_reg(p_dm, 0x948, MASKDWORD, 0x00000000);
		else if (ant_setting == 1)
			odm_set_bb_reg(p_dm, 0x948, MASKDWORD, 0x00000280);
	} else if (p_dm->support_ic_type == ODM_RTL8723D) {
		if (ant_setting == 0)		/* ant A*/
			odm_set_bb_reg(p_dm, 0x948, MASKLWORD, 0x0000);
		else if (ant_setting == 1)
			odm_set_bb_reg(p_dm, 0x948, MASKLWORD, 0x0280);
	}
}

/* ****************************************************** */


void
odm_sw_ant_div_rest_after_link(
	void		*p_dm_void
)
{
}

void
odm_ant_div_on_off(
	void		*p_dm_void,
	u8		swch
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct phydm_fat_struct	*p_dm_fat_table = &p_dm->dm_fat_table;

	if (p_dm_fat_table->ant_div_on_off != swch) {
		if (p_dm->ant_div_type == S0S1_SW_ANTDIV)
			return;

		if (p_dm->support_ic_type & ODM_N_ANTDIV_SUPPORT) {
			PHYDM_DBG(p_dm, DBG_ANT_DIV, ("(( Turn %s )) N-Series HW-AntDiv block\n", (swch == ANTDIV_ON) ? "ON" : "OFF"));
			odm_set_bb_reg(p_dm, 0xc50, BIT(7), swch);
			odm_set_bb_reg(p_dm, 0xa00, BIT(15), swch);

			/*Mingzhi 2017-05-08*/
			if (p_dm->support_ic_type == ODM_RTL8723D) {
				if (swch == ANTDIV_ON) {
					odm_set_bb_reg(p_dm, 0xce0, BIT(1), 1);
					odm_set_bb_reg(p_dm, 0x948, BIT(6), 1);          /*1:HW ctrl  0:SW ctrl*/
					}
				else{
					odm_set_bb_reg(p_dm, 0xce0, BIT(1), 0);
					odm_set_bb_reg(p_dm, 0x948, BIT(6), 0);          /*1:HW ctrl  0:SW ctrl*/
				}
			}			
		} else if (p_dm->support_ic_type & ODM_AC_ANTDIV_SUPPORT) {
			PHYDM_DBG(p_dm, DBG_ANT_DIV, ("(( Turn %s )) AC-Series HW-AntDiv block\n", (swch == ANTDIV_ON) ? "ON" : "OFF"));
			if (p_dm->support_ic_type & ODM_RTL8812) {
				odm_set_bb_reg(p_dm, 0xc50, BIT(7), swch); /* OFDM AntDiv function block enable */
				odm_set_bb_reg(p_dm, 0xa00, BIT(15), swch); /* CCK AntDiv function block enable */
			} else {
				odm_set_bb_reg(p_dm, 0x8D4, BIT(24), swch); /* OFDM AntDiv function block enable */

				if ((p_dm->cut_version >= ODM_CUT_C) && (p_dm->support_ic_type == ODM_RTL8821) && (p_dm->ant_div_type != S0S1_SW_ANTDIV)) {
					PHYDM_DBG(p_dm, DBG_ANT_DIV, ("(( Turn %s )) CCK HW-AntDiv block\n", (swch == ANTDIV_ON) ? "ON" : "OFF"));
					odm_set_bb_reg(p_dm, 0x800, BIT(25), swch);
					odm_set_bb_reg(p_dm, 0xA00, BIT(15), swch); /* CCK AntDiv function block enable */
				} else if (p_dm->support_ic_type == ODM_RTL8821C) {
					PHYDM_DBG(p_dm, DBG_ANT_DIV, ("(( Turn %s )) CCK HW-AntDiv block\n", (swch == ANTDIV_ON) ? "ON" : "OFF"));
					odm_set_bb_reg(p_dm, 0x800, BIT(25), swch);
					odm_set_bb_reg(p_dm, 0xA00, BIT(15), swch); /* CCK AntDiv function block enable */
				}
			}
		}
	}
	p_dm_fat_table->ant_div_on_off = swch;

}

void
odm_tx_by_tx_desc_or_reg(
	void		*p_dm_void,
	u8			swch
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct phydm_fat_struct	*p_dm_fat_table = &p_dm->dm_fat_table;
	u8 enable;

	if (p_dm_fat_table->b_fix_tx_ant == NO_FIX_TX_ANT)
		enable = (swch == TX_BY_DESC) ? 1 : 0;
	else
		enable = 0;/*Force TX by Reg*/

	if (p_dm->ant_div_type != CGCS_RX_HW_ANTDIV) {
		if (p_dm->support_ic_type & ODM_N_ANTDIV_SUPPORT)
			odm_set_bb_reg(p_dm, 0x80c, BIT(21), enable);
		else if (p_dm->support_ic_type & ODM_AC_ANTDIV_SUPPORT)
			odm_set_bb_reg(p_dm, 0x900, BIT(18), enable);

		PHYDM_DBG(p_dm, DBG_ANT_DIV, ("[AntDiv] TX_Ant_BY (( %s ))\n", (enable == TX_BY_DESC) ? "DESC" : "REG"));
	}
}

void
odm_ant_div_reset(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (p_dm->ant_div_type == S0S1_SW_ANTDIV) {
#ifdef CONFIG_S0S1_SW_ANTENNA_DIVERSITY
		odm_s0s1_sw_ant_div_reset(p_dm);
#endif
	}

}

void
odm_antenna_diversity_init(
	void		*p_dm_void
)
{
}

void
odm_antenna_diversity(
	void		*p_dm_void
)
{
}
