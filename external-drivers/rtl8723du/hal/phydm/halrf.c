// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "phydm_precomp.h"

void halrf_basic_profile(void *p_dm_void, u32 *_used, char *output,
			 u32 *_out_len)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u32 used = *_used;
	u32 out_len = *_out_len;

	/* HAL RF version List */
	PHYDM_SNPRINTF((output + used, out_len - used, "%-35s\n", "% HAL RF version %"));
	PHYDM_SNPRINTF((output + used, out_len - used, "  %-35s: %s\n", "Power Tracking", HALRF_POWRTRACKING_VER));
	PHYDM_SNPRINTF((output + used, out_len - used, "  %-35s: %s\n", "IQK", HALRF_IQK_VER));
	PHYDM_SNPRINTF((output + used, out_len - used, "  %-35s: %s\n", "LCK", HALRF_LCK_VER));
	PHYDM_SNPRINTF((output + used, out_len - used, "  %-35s: %s\n", "DPK", HALRF_DPK_VER));

	*_used = used;
	*_out_len = out_len;
}

void halrf_rf_lna_setting(void *p_dm_void, enum phydm_lna_set type)
{
}

void halrf_support_ability_debug(void *p_dm_void, char input[][16], u32 *_used,
				 char *output, u32 *_out_len)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);
	u32	dm_value[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8	i;

	for (i = 0; i < 5; i++) {
		PHYDM_SSCANF(input[i + 1], DCMD_DECIMAL, &dm_value[i]);
	}
	
	PHYDM_SNPRINTF((output + used, out_len - used, "\n%s\n", "================================"));
	if (dm_value[0] == 100) {
		PHYDM_SNPRINTF((output + used, out_len - used, "[RF Supportability]\n"));
		PHYDM_SNPRINTF((output + used, out_len - used, "%s\n", "================================"));
		PHYDM_SNPRINTF((output + used, out_len - used, "00. (( %s ))Power Tracking\n", ((p_rf->rf_supportability & HAL_RF_TX_PWR_TRACK) ? ("V") : ("."))));
		PHYDM_SNPRINTF((output + used, out_len - used, "01. (( %s ))IQK\n", ((p_rf->rf_supportability & HAL_RF_IQK) ? ("V") : ("."))));
		PHYDM_SNPRINTF((output + used, out_len - used, "02. (( %s ))LCK\n", ((p_rf->rf_supportability & HAL_RF_LCK) ? ("V") : ("."))));
		PHYDM_SNPRINTF((output + used, out_len - used, "03. (( %s ))DPK\n", ((p_rf->rf_supportability & HAL_RF_DPK) ? ("V") : ("."))));
		PHYDM_SNPRINTF((output + used, out_len - used, "04. (( %s ))HAL_RF_TXGAPK\n", ((p_rf->rf_supportability & HAL_RF_TXGAPK) ? ("V") : ("."))));
		PHYDM_SNPRINTF((output + used, out_len - used, "%s\n", "================================"));		
	} else {
		if (dm_value[1] == 1) { /* enable */
			p_rf->rf_supportability |= BIT(dm_value[0]) ;
		} else if (dm_value[1] == 2) /* disable */
			p_rf->rf_supportability &= ~(BIT(dm_value[0])) ;
		else {
			PHYDM_SNPRINTF((output + used, out_len - used, "%s\n", "[Warning!!!]  1:enable,  2:disable"));
		}
	}
	PHYDM_SNPRINTF((output + used, out_len - used, "Curr-RF_supportability =  0x%x\n", p_rf->rf_supportability));
	PHYDM_SNPRINTF((output + used, out_len - used, "%s\n", "================================"));

	*_used = used;
	*_out_len = out_len;
}

void halrf_cmn_info_init(void *p_dm_void, enum halrf_cmninfo_init_e cmn_info,
			 u32 value)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);

	switch	(cmn_info) {
	case	HALRF_CMNINFO_EEPROM_THERMAL_VALUE:
		p_rf->eeprom_thermal = (u8)value;
		break;
	case	HALRF_CMNINFO_FW_VER:
		p_rf->fw_ver = (u32)value;
		break;
	default:
		break;
	}
}


void halrf_cmn_info_hook(void *p_dm_void, enum halrf_cmninfo_hook_e cmn_info,
			 void *p_value)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);
	
	switch	(cmn_info) {
	case	HALRF_CMNINFO_CON_TX:
		p_rf->p_is_con_tx = (bool *)p_value;
		break;
	case	HALRF_CMNINFO_SINGLE_TONE:
		p_rf->p_is_single_tone = (bool *)p_value;		
		break;
	case	HALRF_CMNINFO_CARRIER_SUPPRESSION:
		p_rf->p_is_carrier_suppresion = (bool *)p_value;		
		break;
	case	HALRF_CMNINFO_MP_RATE_INDEX:
		p_rf->p_mp_rate_index = (u8 *)p_value;
		break;
	default:
		/*do nothing*/
		break;
	}
}

void halrf_cmn_info_set(void *p_dm_void, u32 cmn_info, u64 value)
{
	/* This init variable may be changed in run time. */
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);
	
	switch	(cmn_info) {

		case	HALRF_CMNINFO_ABILITY:
			p_rf->rf_supportability = (u32)value;
			break;

		case	HALRF_CMNINFO_DPK_EN:
			p_rf->dpk_en = (u8)value;
			break;
		case HALRF_CMNINFO_RFK_FORBIDDEN :
			p_dm->IQK_info.rfk_forbidden = (bool)value;
			break;
		case HALRF_CMNINFO_RATE_INDEX:
			p_rf->p_rate_index = (u32)value;
			break;
		default:
			/* do nothing */
			break;
	}
}

u64 halrf_cmn_info_get(void *p_dm_void, u32 cmn_info)
{
	/* This init variable may be changed in run time. */
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);
	u64	return_value = 0;
	
	switch	(cmn_info) {
	case	HALRF_CMNINFO_ABILITY:
		return_value = (u32)p_rf->rf_supportability;
		break;
	case HALRF_CMNINFO_RFK_FORBIDDEN :
		return_value = p_dm->IQK_info.rfk_forbidden;
		break;
	default:
		break;
	}
	return	return_value;
}

static void halrf_supportability_init_mp(void *p_dm_void)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);

	switch (p_dm->support_ic_type) {
	case ODM_RTL8814B:
		break;
	default:
		p_rf->rf_supportability = 
			HAL_RF_TX_PWR_TRACK	|
			HAL_RF_IQK		|
			HAL_RF_LCK		|
			0;
		break;
	}

	ODM_RT_TRACE(p_dm, ODM_COMP_INIT, ODM_DBG_LOUD, ("IC = ((0x%x)), RF_Supportability Init MP = ((0x%x))\n",
		     p_dm->support_ic_type, p_rf->rf_supportability));
}

void halrf_supportability_init(void *p_dm_void)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);

	switch (p_dm->support_ic_type) {
	case ODM_RTL8814B:
		break;
	default:
		p_rf->rf_supportability = 
			HAL_RF_TX_PWR_TRACK	|
			HAL_RF_IQK		|
			HAL_RF_LCK		|
			0;
		break;
	}

	ODM_RT_TRACE(p_dm, ODM_COMP_INIT, ODM_DBG_LOUD, ("IC = ((0x%x)), RF_Supportability Init = ((0x%x))\n",
		     p_dm->support_ic_type, p_rf->rf_supportability));
}

void halrf_watchdog(void *p_dm_void)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	phydm_rf_watchdog(p_dm);
}

void halrf_iqk_trigger(void *p_dm_void, bool is_recovery)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION		*p_iqk_info = &p_dm->IQK_info;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);
	u64 start_time;

	if ((p_dm->p_mp_mode) && (p_rf->p_is_con_tx) &&
	    (p_rf->p_is_single_tone) && (p_rf->p_is_carrier_suppresion))
		if (*(p_dm->p_mp_mode) && ((*(p_rf->p_is_con_tx) ||
		    *(p_rf->p_is_single_tone) ||
		    *(p_rf->p_is_carrier_suppresion))))
			return;

	if (!(p_rf->rf_supportability & HAL_RF_IQK))
		return;
#if DISABLE_BB_RF
	return;
#endif

	if (p_iqk_info->rfk_forbidden)
		return;

	if (!p_dm->rf_calibrate_info.is_iqk_in_progress) {
		odm_acquire_spin_lock(p_dm, RT_IQK_SPINLOCK);
		p_dm->rf_calibrate_info.is_iqk_in_progress = true;
		odm_release_spin_lock(p_dm, RT_IQK_SPINLOCK);
		start_time = odm_get_current_time(p_dm);
		switch (p_dm->support_ic_type) {
		case ODM_RTL8723D:
			phy_iq_calibrate_8723d(p_dm, is_recovery);
			break;
		default:
			break;
		}
		p_dm->rf_calibrate_info.iqk_progressing_time = odm_get_progressing_time(p_dm, start_time);
		ODM_RT_TRACE(p_dm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			     ("[IQK]IQK progressing_time = %lld ms\n",
			      p_dm->rf_calibrate_info.iqk_progressing_time));

		odm_acquire_spin_lock(p_dm, RT_IQK_SPINLOCK);
		p_dm->rf_calibrate_info.is_iqk_in_progress = false;
		odm_release_spin_lock(p_dm, RT_IQK_SPINLOCK);
	} else {
		ODM_RT_TRACE(p_dm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			     ("== Return the IQK CMD, because RFKs in Progress ==\n"));
	}
}

void halrf_lck_trigger(void *p_dm_void)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION		*p_iqk_info = &p_dm->IQK_info;
	struct _hal_rf_				*p_rf = &(p_dm->rf_table);
	u64 start_time;
	
	if ((p_dm->p_mp_mode) && (p_rf->p_is_con_tx) &&
	    (p_rf->p_is_single_tone) && (p_rf->p_is_carrier_suppresion))
		if (*(p_dm->p_mp_mode) && ((*(p_rf->p_is_con_tx) ||
		    *(p_rf->p_is_single_tone) ||
		    *(p_rf->p_is_carrier_suppresion))))
			return;
	if (!(p_rf->rf_supportability & HAL_RF_LCK))
		return;
#if DISABLE_BB_RF
		return;
#endif
	if (p_iqk_info->rfk_forbidden)
		return;
	while (*(p_dm->p_is_scan_in_process)) {
	       ODM_RT_TRACE(p_dm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			    ("[LCK]scan is in process, bypass LCK\n"));
		return;
	}

	if (!p_dm->rf_calibrate_info.is_lck_in_progress) {
		odm_acquire_spin_lock(p_dm, RT_IQK_SPINLOCK);
		p_dm->rf_calibrate_info.is_lck_in_progress = true;
		odm_release_spin_lock(p_dm, RT_IQK_SPINLOCK);
		start_time = odm_get_current_time(p_dm);
		switch (p_dm->support_ic_type) {
		case ODM_RTL8723D:
			phy_lc_calibrate_8723d(p_dm);
			break;
		default:
			break;
		}
		p_dm->rf_calibrate_info.lck_progressing_time =
			 odm_get_progressing_time(p_dm, start_time);
		ODM_RT_TRACE(p_dm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			     ("[IQK]LCK progressing_time = %lld ms\n",
			      p_dm->rf_calibrate_info.lck_progressing_time));
		odm_acquire_spin_lock(p_dm, RT_IQK_SPINLOCK);
		p_dm->rf_calibrate_info.is_lck_in_progress = false;
		odm_release_spin_lock(p_dm, RT_IQK_SPINLOCK);		
	} else {
		ODM_RT_TRACE(p_dm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			     ("== Return the LCK CMD, because RFK is in Progress ==\n"));
	}
}

void halrf_init(void *p_dm_void)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	
	ODM_RT_TRACE(p_dm, ODM_COMP_INIT, ODM_DBG_LOUD, ("HALRF_Init\n"));

	if (*(p_dm->p_mp_mode))
		halrf_supportability_init_mp(p_dm);
	else
		halrf_supportability_init(p_dm);
}
