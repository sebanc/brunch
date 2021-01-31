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

#ifndef	__PHYDM_FEATURES_WIN_H__
#define __PHYDM_FEATURES_WIN_H__

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

/*#define PHYDM_TDMA_DIG_SUPPORT*/

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

#if (RTL8822B_SUPPORT == 1 || RTL8192F_SUPPORT == 1)
	#define	PHYDM_POWER_TRAINING_SUPPORT
#endif

#if (RTL8822B_SUPPORT == 1)
	#define	PHYDM_TXA_CALIBRATION
#endif

#if (RTL8188E_SUPPORT == 1 || RTL8192E_SUPPORT == 1)
	#define	PHYDM_PRIMARY_CCA
#endif

#if (RTL8188F_SUPPORT == 1 || RTL8710B_SUPPORT == 1 || RTL8821C_SUPPORT == 1 || RTL8822B_SUPPORT == 1 || RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	#define	PHYDM_DC_CANCELLATION
#endif

#if (RTL8822B_SUPPORT == 1 || RTL8197F_SUPPORT == 1 || RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	/*#define	CONFIG_ADAPTIVE_SOML*/
#endif

#if (RTL8192F_SUPPORT == 1)/*jj add 20170822*/
	#define	CONFIG_ADAPTIVE_SOML
#endif

#if (RTL8192F_SUPPORT == 1)
	#define	CONFIG_8912F_SPUR_CALIBRATION
#endif

/*Antenna Diversity*/
#define	CONFIG_PHYDM_ANTENNA_DIVERSITY
#ifdef CONFIG_PHYDM_ANTENNA_DIVERSITY

	#if (RTL8723B_SUPPORT == 1) || (RTL8821A_SUPPORT == 1) || (RTL8188F_SUPPORT == 1) || (RTL8821C_SUPPORT == 1)
		#define	CONFIG_S0S1_SW_ANTENNA_DIVERSITY
	#endif

	#if (RTL8822B_SUPPORT == 1)
		/*#define	ODM_EVM_ENHANCE_ANTDIV*/
		/*#define	CONFIG_2T3R_ANTENNA*/
	#endif

	/* --[SmtAnt]-----------------------------------------*/
	#if (RTL8821A_SUPPORT == 1)
		/*#define	CONFIG_HL_SMART_ANTENNA_TYPE1*/
		#define	CONFIG_FAT_PATCH
	#endif
	
	#if (RTL8822B_SUPPORT == 1)
		/*#define CONFIG_HL_SMART_ANTENNA_TYPE2*/
	#endif
	
	#if (defined(CONFIG_HL_SMART_ANTENNA_TYPE1) || defined(CONFIG_HL_SMART_ANTENNA_TYPE2))
		#define	CONFIG_HL_SMART_ANTENNA
	#endif

	/* --------------------------------------------------*/

#endif

/*[SmartAntenna]*/
#define	CONFIG_SMART_ANTENNA
#ifdef CONFIG_SMART_ANTENNA
		/*#define	CONFIG_CUMITEK_SMART_ANTENNA*/
#endif
	/* --------------------------------------------------*/

#if (RTL8188E_SUPPORT == 1 || RTL8192E_SUPPORT == 1)
	#define	CONFIG_RECEIVER_BLOCKING
#endif

#if (RTL8812A_SUPPORT == 1 || RTL8821A_SUPPORT == 1 || RTL8881A_SUPPORT == 1 || RTL8192E_SUPPORT == 1 || RTL8723B_SUPPORT == 1)
	#define	CONFIG_RA_FW_DBG_CODE
#endif

/* #ifdef CONFIG_SUPPORT_DYNAMIC_TXPWR */
#define CONFIG_DYNAMIC_TX_TWR
/* #endif */
/*#define PHYDM_DIG_MODE_DECISION_SUPPORT	*/
#define	CONFIG_PSD_TOOL
#define PHYDM_SUPPORT_ADAPTIVITY
#define	PHYDM_SUPPORT_CCKPD
#define	RA_MASK_PHYDMLIZE_WIN
/*#define	CONFIG_PATH_DIVERSITY*/
/*#define	CONFIG_RA_DYNAMIC_RTY_LIMIT*/
#define CONFIG_ANT_DETECTION
#define	CONFIG_BB_TXBF_API
#define	ODM_CONFIG_BT_COEXIST
#define	PHYDM_3RD_REFORM_RA_MASK
#define	PHYDM_3RD_REFORM_RSSI_MONOTOR
#define	CONFIG_PHYDM_DFS_MASTER
#define	PHYDM_SUPPORT_RSSI_MONITOR
#define	PHYDM_AUTO_DEGBUG
#define CONFIG_PHYDM_DEBUG_FUNCTION

#endif
