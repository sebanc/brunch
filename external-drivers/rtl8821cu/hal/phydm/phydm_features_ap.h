/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/

#ifndef	__PHYDM_FEATURES_AP_H__
#define __PHYDM_FEATURES_AP_H__

#if (RTL8822B_SUPPORT == 1 || RTL8812A_SUPPORT == 1 || RTL8197F_SUPPORT == 1 || RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	#define DYN_ANT_WEIGHTING_SUPPORT
#endif

#if (RTL8822B_SUPPORT == 1 || RTL8821C_SUPPORT == 1)
	#define FAHM_SUPPORT
#endif
	#define NHM_SUPPORT
	#define CLM_SUPPORT

#if (RTL8822B_SUPPORT == 1)
	/*#define PHYDM_PHYSTAUS_SMP_MODE*/
#endif

#if (RTL8197F_SUPPORT == 1)
	/*#define PHYDM_TDMA_DIG_SUPPORT*/
#endif

#if (RTL8197F_SUPPORT == 1) || (RTL8822B_SUPPORT == 1)
	/*#define PHYDM_LNA_SAT_CHK_SUPPORT*/
	#ifdef PHYDM_LNA_SAT_CHK_SUPPORT

		#if (RTL8197F_SUPPORT == 1)
		/*#define PHYDM_LNA_SAT_CHK_SUPPORT_TYPE1*/
		#endif

		#if (RTL8822B_SUPPORT == 1)
		/*#define PHYDM_LNA_SAT_CHK_TYPE2*/
		#endif
	#endif
#endif

#if (RTL8822B_SUPPORT == 1)
	/*#define PHYDM_POWER_TRAINING_SUPPORT*/
#endif

#if (RTL8822B_SUPPORT == 1)
	#define PHYDM_TXA_CALIBRATION
#endif

#if (RTL8188E_SUPPORT == 1) || (RTL8197F_SUPPORT == 1) || (RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	#define	PHYDM_PRIMARY_CCA
#endif

#if (RTL8188F_SUPPORT == 1 || RTL8710B_SUPPORT == 1 || RTL8821C_SUPPORT == 1 || RTL8822B_SUPPORT == 1 || RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	#define	PHYDM_DC_CANCELLATION
#endif

#if (RTL8822B_SUPPORT == 1 || RTL8197F_SUPPORT == 1 || RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	#define	CONFIG_ADAPTIVE_SOML
#endif

#if (RTL8812A_SUPPORT == 1 || RTL8821A_SUPPORT == 1 || RTL8881A_SUPPORT == 1 || RTL8192E_SUPPORT == 1 || RTL8723B_SUPPORT == 1)
	/*#define	CONFIG_RA_FW_DBG_CODE*/
#endif

#if (RTL8192F_SUPPORT == 1)
	/*#define	CONFIG_8912F_SPUR_CALIBRATION*/
#endif

#ifdef CONFIG_SUPPORT_DYNAMIC_TXPWR
#define CONFIG_DYNAMIC_TX_TWR
#endif
#define PHYDM_DIG_MODE_DECISION_SUPPORT
/*#define	CONFIG_PSD_TOOL*/
#define PHYDM_SUPPORT_CCKPD
#define PHYDM_SUPPORT_ADAPTIVITY
#define RA_MASK_PHYDMLIZE_AP
/*#define	CONFIG_PATH_DIVERSITY*/
/*#define	CONFIG_RA_DYNAMIC_RTY_LIMIT*/
/*#define	CONFIG_RA_DYNAMIC_RATE_ID*/
#define	CONFIG_BB_TXBF_API
/*#define	ODM_CONFIG_BT_COEXIST*/
#define	PHYDM_3RD_REFORM_RA_MASK
#define	PHYDM_3RD_REFORM_RSSI_MONOTOR
#define	PHYDM_SUPPORT_RSSI_MONITOR
#if !defined(CONFIG_DISABLE_PHYDM_DEBUG_FUNCTION)
	#define CONFIG_PHYDM_DEBUG_FUNCTION
#endif

/* [ Configure Antenna Diversity ] */
#if (RTL8188F_SUPPORT == 1)
	#ifdef CONFIG_ANTENNA_DIVERSITY
		#define CONFIG_PHYDM_ANTENNA_DIVERSITY
		#define CONFIG_S0S1_SW_ANTENNA_DIVERSITY
	#endif
#endif

#if defined(CONFIG_RTL_8881A_ANT_SWITCH) || defined(CONFIG_SLOT_0_ANT_SWITCH) || defined(CONFIG_SLOT_1_ANT_SWITCH) || defined(CONFIG_RTL_8197F_ANT_SWITCH)
	#define CONFIG_PHYDM_ANTENNA_DIVERSITY
	#define ODM_EVM_ENHANCE_ANTDIV
	#define SKIP_EVM_ANTDIV_TRAINING_PATCH

	/*----------*/
	#ifdef CONFIG_NO_2G_DIVERSITY_8197F
		#define CONFIG_NO_2G_DIVERSITY
	#elif defined(CONFIG_2G_CGCS_RX_DIVERSITY_8197F)
		#define CONFIG_2G_CGCS_RX_DIVERSITY
	#elif defined(CONFIG_2G_CG_TRX_DIVERSITY_8197F)
		#define CONFIG_2G_CG_TRX_DIVERSITY
	#endif

	#if (!defined(CONFIG_NO_2G_DIVERSITY) && !defined(CONFIG_2G5G_CG_TRX_DIVERSITY_8881A) && !defined(CONFIG_2G_CGCS_RX_DIVERSITY) && !defined(CONFIG_2G_CG_TRX_DIVERSITY) && !defined(CONFIG_2G_CG_SMART_ANT_DIVERSITY))
		#define CONFIG_NO_2G_DIVERSITY
	#endif

	#ifdef CONFIG_NO_5G_DIVERSITY_8881A
		#define CONFIG_NO_5G_DIVERSITY
	#elif defined(CONFIG_5G_CGCS_RX_DIVERSITY_8881A)
		#define CONFIG_5G_CGCS_RX_DIVERSITY
	#elif defined(CONFIG_5G_CG_TRX_DIVERSITY_8881A)
		#define CONFIG_5G_CG_TRX_DIVERSITY
	#elif defined(CONFIG_2G5G_CG_TRX_DIVERSITY_8881A)
		#define CONFIG_2G5G_CG_TRX_DIVERSITY
	#endif
	#if (!defined(CONFIG_NO_5G_DIVERSITY) && !defined(CONFIG_5G_CGCS_RX_DIVERSITY) && !defined(CONFIG_5G_CG_TRX_DIVERSITY) && !defined(CONFIG_2G5G_CG_TRX_DIVERSITY) && !defined(CONFIG_5G_CG_SMART_ANT_DIVERSITY))
		#define CONFIG_NO_5G_DIVERSITY
	#endif
	/*----------*/
	#if (defined(CONFIG_NO_2G_DIVERSITY) && defined(CONFIG_NO_5G_DIVERSITY))
		#define CONFIG_NOT_SUPPORT_ANTDIV
	#elif (!defined(CONFIG_NO_2G_DIVERSITY) && defined(CONFIG_NO_5G_DIVERSITY))
		#define CONFIG_2G_SUPPORT_ANTDIV
	#elif (defined(CONFIG_NO_2G_DIVERSITY) && !defined(CONFIG_NO_5G_DIVERSITY))
		#define CONFIG_5G_SUPPORT_ANTDIV
	#elif ((!defined(CONFIG_NO_2G_DIVERSITY) && !defined(CONFIG_NO_5G_DIVERSITY)) || defined(CONFIG_2G5G_CG_TRX_DIVERSITY))
			#define CONFIG_2G5G_SUPPORT_ANTDIV
	#endif
		/*----------*/
#endif /*Antenna Diveristy*/

/*[SmartAntenna]*/
/*#define	CONFIG_SMART_ANTENNA*/
#ifdef CONFIG_SMART_ANTENNA
	/*#define	CONFIG_CUMITEK_SMART_ANTENNA*/
#endif
/* --------------------------------------------------*/

#endif
