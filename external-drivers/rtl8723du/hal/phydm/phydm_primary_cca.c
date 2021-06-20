// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */
#include "mp_precomp.h"
#include "phydm_precomp.h"
#ifdef PHYDM_PRIMARY_CCA

void
phydm_write_dynamic_cca(
	void			*p_dm_void,
	u8			curr_mf_state
	
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct phydm_pricca_struct	*primary_cca = &(p_dm->dm_pri_cca);

	if (primary_cca->mf_state != curr_mf_state) {

		if (p_dm->support_ic_type & ODM_IC_11N_SERIES) {
			
			if (curr_mf_state == MF_USC_LSC) {
				odm_set_bb_reg(p_dm, 0xc6c, BIT(8) | BIT(7), MF_USC_LSC);
				odm_set_bb_reg(p_dm, 0xc84, 0xf0000000, primary_cca->cca_th_40m_bkp); /*40M OFDM MF CCA threshold*/
			} else {
				odm_set_bb_reg(p_dm, 0xc6c, BIT(8) | BIT(7), curr_mf_state);
				odm_set_bb_reg(p_dm, 0xc84, 0xf0000000, 0); /*40M OFDM MF CCA threshold*/
			}
		}
		
		primary_cca->mf_state = curr_mf_state;
		PHYDM_DBG(p_dm, DBG_PRI_CCA,
			("Set CCA at ((%s SB)), 0xc6c[8:7]=((%d))\n", ((curr_mf_state == MF_USC_LSC)?"D":((curr_mf_state == MF_LSC)?"L":"U")), curr_mf_state));
	}
}

void
phydm_primary_cca_reset(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct phydm_pricca_struct	*primary_cca = &(p_dm->dm_pri_cca);

	PHYDM_DBG(p_dm, DBG_PRI_CCA, ("[PriCCA] Reset\n"));
	primary_cca->mf_state = 0xff;
	primary_cca->pre_bw = (enum channel_width)0xff;
	phydm_write_dynamic_cca(p_dm, MF_USC_LSC);
}

void
phydm_primary_cca_11n(
	void			*p_dm_void
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct phydm_pricca_struct	*p_primary_cca = &(p_dm->dm_pri_cca);
	enum channel_width	curr_bw = (enum channel_width)(*(p_dm->p_band_width));

	if (!(p_dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;
	
	if (!p_dm->is_linked) { /* is_linked==False */
		PHYDM_DBG(p_dm, DBG_PRI_CCA, ("[PriCCA][No Link!!!]\n"));

		if (p_primary_cca->pri_cca_is_become_linked) {
			phydm_primary_cca_reset(p_dm);
			p_primary_cca->pri_cca_is_become_linked = p_dm->is_linked;
		}
		return;
		
	} else {
		if (!p_primary_cca->pri_cca_is_become_linked) {
			PHYDM_DBG(p_dm, DBG_PRI_CCA, ("[PriCCA][Linked !!!]\n"));
			p_primary_cca->pri_cca_is_become_linked = p_dm->is_linked;
		}
	}
	
	if (curr_bw != p_primary_cca->pre_bw) {

		PHYDM_DBG(p_dm, DBG_PRI_CCA, ("[Primary CCA] start ==>\n"));
		p_primary_cca->pre_bw = curr_bw;

		if (curr_bw == CHANNEL_WIDTH_40) {
			
			if ((*(p_dm->p_sec_ch_offset)) == SECOND_CH_AT_LSB) {/* Primary CH @ upper sideband*/
				
				PHYDM_DBG(p_dm, DBG_PRI_CCA, ("BW40M, Primary CH at USB\n"));
				phydm_write_dynamic_cca(p_dm, MF_USC);
				
			} else {	/*Primary CH @ lower sideband*/
				
				PHYDM_DBG(p_dm, DBG_PRI_CCA, ("BW40M, Primary CH at LSB\n"));
				phydm_write_dynamic_cca(p_dm, MF_LSC);
			}
		} else {
		
			PHYDM_DBG(p_dm, DBG_PRI_CCA, ("Not BW40M, USB + LSB\n"));
			phydm_primary_cca_reset(p_dm);
		}
	}
}

#endif

bool
odm_dynamic_primary_cca_dup_rts(
	void			*p_dm_void
)
{
#ifdef PHYDM_PRIMARY_CCA
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct phydm_pricca_struct		*primary_cca = &(p_dm->dm_pri_cca);

	return	primary_cca->dup_rts_flag;
#else
	return	0;	
#endif
}

void
phydm_primary_cca_init(
	void			*p_dm_void
)
{
#ifdef PHYDM_PRIMARY_CCA
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct phydm_pricca_struct		*primary_cca = &(p_dm->dm_pri_cca);

	if (!(p_dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	PHYDM_DBG(p_dm, DBG_PRI_CCA, ("[PriCCA] Init ==>\n"));
	primary_cca->mf_state = 0xff;
	primary_cca->pre_bw = (enum channel_width)0xff;
	
	if (p_dm->support_ic_type & ODM_IC_11N_SERIES)
		primary_cca->cca_th_40m_bkp = (u8)odm_get_bb_reg(p_dm, 0xc84, 0xf0000000);
#endif
}

void
phydm_primary_cca(
	void			*p_dm_void
)
{
#ifdef PHYDM_PRIMARY_CCA
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;

	if (!(p_dm->support_ic_type & ODM_IC_11N_SERIES))
		return;

	if (!(p_dm->support_ability & ODM_BB_PRIMARY_CCA))
		return;

	phydm_primary_cca_11n(p_dm);

#endif
}


