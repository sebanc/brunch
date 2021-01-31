/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
//============================================================
// Description:
//
// This file is for 92CE/92CU dynamic mechanism only
//
//
//============================================================
#define _RTL8814A_DM_C_

//============================================================
// include files
//============================================================
//#include <drv_types.h>
#include <rtl8814a_hal.h>

//============================================================
// Global var
//============================================================

static VOID
dm_CheckProtection(
	IN	PADAPTER	Adapter
	)
{
#if 0
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);
	u1Byte			CurRate, RateThreshold;

	if(pMgntInfo->pHTInfo->bCurBW40MHz)
		RateThreshold = MGN_MCS1;
	else
		RateThreshold = MGN_MCS3;

	if(Adapter->TxStats.CurrentInitTxRate <= RateThreshold) {
		pMgntInfo->bDmDisableProtect = TRUE;
		DbgPrint("Forced disable protect: %x\n", Adapter->TxStats.CurrentInitTxRate);
	} else {
		pMgntInfo->bDmDisableProtect = FALSE;
		DbgPrint("Enable protect: %x\n", Adapter->TxStats.CurrentInitTxRate);
	}
#endif
}

#ifdef CONFIG_SUPPORT_HW_WPS_PBC
static void dm_CheckPbcGPIO(_adapter *padapter)
{
	u8	tmp1byte;
	u8	bPbcPressed = _FALSE;

	if(!padapter->registrypriv.hw_wps_pbc)
		return;

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)

	tmp1byte = rtw_read8(padapter, REG_GPIO_EXT_CTRL_8814A);
	//DBG_871X("CheckPbcGPIO - %x\n", tmp1byte);

	if (tmp1byte == 0xff)
		return ;
	else if (tmp1byte & BIT3) {
		// Here we only set bPbcPressed to TRUE. After trigger PBC, the variable will be set to FALSE
		DBG_871X("CheckPbcGPIO - PBC is pressed\n");
		bPbcPressed = _TRUE;
	}

#endif

	if (_TRUE == bPbcPressed) {
		/* Here we only set bPbcPressed to true */
		/* After trigger PBC, the variable will be set to false */
		RTW_INFO("CheckPbcGPIO - PBC is pressed\n");

		rtw_request_wps_pbc_event(padapter);
	}
}
#endif /* #ifdef CONFIG_SUPPORT_HW_WPS_PBC */

#ifdef CONFIG_PCI_HCI
/*
 *	Description:
 *		Perform interrupt migration dynamically to reduce CPU utilization.
 *
 *	Assumption:
 *		1. Do not enable migration under WIFI test.
 *
 *	Created by Roger, 2010.03.05.
 *   */
VOID
dm_InterruptMigration(
	IN	PADAPTER	Adapter
)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);
	BOOLEAN			bCurrentIntMt, bCurrentACIntDisable;
	BOOLEAN			IntMtToSet = _FALSE;
	BOOLEAN			ACIntToSet = _FALSE;

	/* Retrieve current interrupt migration and Tx four ACs IMR settings first. */
	bCurrentIntMt = pHalData->bInterruptMigration;
	bCurrentACIntDisable = pHalData->bDisableTxInt;

	/*  */
	/* <Roger_Notes> Currently we use busy traffic for reference instead of RxIntOK counts to prevent non-linear Rx statistics */
	/* when interrupt migration is set before. 2010.03.05. */
	/*  */
	if (!Adapter->registrypriv.wifi_spec &&
	    (check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE) &&
	    pmlmepriv->LinkDetectInfo.bHigherBusyTraffic) {
		IntMtToSet = _TRUE;

		/* To check whether we should disable Tx interrupt or not. */
		if (pmlmepriv->LinkDetectInfo.bHigherBusyRxTraffic)
			ACIntToSet = _TRUE;
	}

	/* Update current settings. */
	if (bCurrentIntMt != IntMtToSet) {
		RTW_INFO("%s(): Update interrrupt migration(%d)\n", __FUNCTION__, IntMtToSet);
		if (IntMtToSet) {
			/*  */
			/* <Roger_Notes> Set interrrupt migration timer and corresponging Tx/Rx counter. */
			/* timer 25ns*0xfa0=100us for 0xf packets. */
			/* 2010.03.05. */
			/*  */
			rtw_write32(Adapter, REG_INT_MIG, 0xff000fa0);/* 0x306:Rx, 0x307:Tx */
			pHalData->bInterruptMigration = IntMtToSet;
		} else {
			/* Reset all interrupt migration settings. */
			rtw_write32(Adapter, REG_INT_MIG, 0);
			pHalData->bInterruptMigration = IntMtToSet;
		}
	}

#if 0
	if (bCurrentACIntDisable != ACIntToSet) {
		RTW_INFO("%s(): Update AC interrrupt(%d)\n", __FUNCTION__, ACIntToSet);
		if (ACIntToSet) { /*  Disable four ACs interrupts. */
			/* */
			/*  <Roger_Notes> Disable VO, VI, BE and BK four AC interrupts to gain more efficient CPU utilization. */
			/*  When extremely highly Rx OK occurs, we will disable Tx interrupts. */
			/*  2010.03.05. */
			/* */
			UpdateInterruptMask8192CE(Adapter, 0, RT_AC_INT_MASKS);
			pHalData->bDisableTxInt = ACIntToSet;
		} else { /*  Enable four ACs interrupts. */
			UpdateInterruptMask8192CE(Adapter, RT_AC_INT_MASKS, 0);
			pHalData->bDisableTxInt = ACIntToSet;
		}
	}
#endif

}

#endif

/*
 * Initialize GPIO setting registers
 *   */
static void
dm_InitGPIOSetting(
	IN	PADAPTER	Adapter
)
{
	PHAL_DATA_TYPE		pHalData = GET_HAL_DATA(Adapter);

	u8	tmp1byte;

	tmp1byte = rtw_read8(Adapter, REG_GPIO_MUXCFG);
	tmp1byte &= (GPIOSEL_GPIO | ~GPIOSEL_ENBT);

	rtw_write8(Adapter, REG_GPIO_MUXCFG, tmp1byte);
}

/* ************************************************************
 * functions
 * ************************************************************ */
static void Init_ODM_ComInfo_8814(PADAPTER	Adapter)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		*pDM_Odm = &(pHalData->odmpriv);
	u8	cut_ver, fab_ver;

	Init_ODM_ComInfo(Adapter);

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_IC_TYPE, ODM_RTL8814A);

	fab_ver = ODM_TSMC;
	if(IS_A_CUT(pHalData->version_id))
		cut_ver = ODM_CUT_A;
	else if(IS_B_CUT(pHalData->version_id))
		cut_ver = ODM_CUT_B;
	else if(IS_C_CUT(pHalData->version_id))
		cut_ver = ODM_CUT_C;
	else if(IS_D_CUT(pHalData->version_id))
		cut_ver = ODM_CUT_D;
	else if(IS_E_CUT(pHalData->version_id))
		cut_ver = ODM_CUT_E;
	else
		cut_ver = ODM_CUT_A;

	odm_cmn_info_init(pDM_Odm,ODM_CMNINFO_FAB_VER,fab_ver);
	odm_cmn_info_init(pDM_Odm,ODM_CMNINFO_CUT_VER,cut_ver);

 	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_RF_ANTENNA_TYPE, pHalData->TRxAntDivType);

	//odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_IQKFWOFFLOAD, pHalData->RegIQKFWOffload);

}

void
rtl8814_InitHalDm(
	IN	PADAPTER	Adapter
	)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		*		pDM_Odm = &(pHalData->odmpriv);
	u8	i;

#ifdef CONFIG_USB_HCI
	dm_InitGPIOSetting(Adapter);
#endif //CONFIG_USB_HCI

	odm_dm_init(pDM_Odm);

	//Adapter->fix_rate = 0xFF;

}

VOID
rtl8814_HalDmWatchDog(
	IN	PADAPTER	Adapter
	)
{
	BOOLEAN		bFwCurrentInPSMode = _FALSE;
	BOOLEAN		bFwPSAwake = _TRUE;
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		*pDM_Odm = &(pHalData->odmpriv);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(Adapter);
	u8 in_lps = _FALSE;

	if (!rtw_is_hw_init_completed(Adapter))
		goto skip_dm;

#ifdef CONFIG_LPS
	bFwCurrentInPSMode = adapter_to_pwrctl(Adapter)->bFwCurrentInPSMode;
	rtw_hal_get_hwreg(Adapter, HW_VAR_FWLPS_RF_ON, &bFwPSAwake);
#endif

#ifdef CONFIG_P2P_PS
	/* Fw is under p2p powersaving mode, driver should stop dynamic mechanism. */
	/* modifed by thomas. 2011.06.11. */
	if (Adapter->wdinfo.p2p_ps_mode)
		bFwPSAwake = _FALSE;
#endif /* CONFIG_P2P_PS */

	if ((rtw_is_hw_init_completed(Adapter))
	    && ((!bFwCurrentInPSMode) && bFwPSAwake)) {

		rtw_hal_check_rxfifo_full(Adapter);
		/*  */
		/* Dynamically switch RTS/CTS protection. */
		/*  */
		/* dm_CheckProtection(Adapter); */

#ifdef CONFIG_PCI_HCI
		/* 20100630 Joseph: Disable Interrupt Migration mechanism temporarily because it degrades Rx throughput. */
		/* Tx Migration settings. */
		/* dm_InterruptMigration(Adapter); */

		/* if(Adapter->HalFunc.TxCheckStuckHandler(Adapter)) */
		/*	PlatformScheduleWorkItem(&(GET_HAL_DATA(Adapter)->HalResetWorkItem)); */
#endif

	}

#ifdef CONFIG_DISABLE_ODM
	goto skip_dm;
#endif
#ifdef CONFIG_LPS
	if (pwrpriv->bLeisurePs && bFwCurrentInPSMode && pwrpriv->pwr_mode != PS_MODE_ACTIVE)
		in_lps = _TRUE;
#endif
	rtw_phydm_watchdog(Adapter, in_lps);

skip_dm:

#ifdef CONFIG_SUPPORT_HW_WPS_PBC
	/* Check GPIO to determine current Pbc status. */
	dm_CheckPbcGPIO(Adapter);
#endif

	return;
}

void rtl8814_init_dm_priv(IN PADAPTER Adapter)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		*podmpriv = &pHalData->odmpriv;

	/* _rtw_spinlock_init(&(pHalData->odm_stainfo_lock)); */

#ifndef CONFIG_IQK_PA_OFF /* FW has no IQK PA OFF option yet, don't offload */
	#ifdef CONFIG_BT_COEXIST
	/* firmware size issue, btcoex fw doesn't support IQK offload */
	if (pHalData->EEPROMBluetoothCoexist == _FALSE)
	#endif
	{
		pHalData->RegIQKFWOffload = 1;
		rtw_sctx_init(&pHalData->iqk_sctx, 0);
	}
#endif

	Init_ODM_ComInfo_8814(Adapter);
	odm_init_all_timers(podmpriv );
	//PHYDM_InitDebugSetting(podmpriv);

	pHalData->CurrentTxPwrIdx = 20;

}

void rtl8814_deinit_dm_priv(IN PADAPTER Adapter)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		*podmpriv = &pHalData->odmpriv;
	/* _rtw_spinlock_free(&pHalData->odm_stainfo_lock); */
	odm_cancel_all_timers(podmpriv);
}


#ifdef CONFIG_ANTENNA_DIVERSITY
// Add new function to reset the state of antenna diversity before link.
//
// Compare RSSI for deciding antenna
void	AntDivCompare8814(PADAPTER Adapter, WLAN_BSSID_EX *dst, WLAN_BSSID_EX *src)
{
	//PADAPTER Adapter = pDM_Odm->Adapter ;

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	if (0 != pHalData->AntDivCfg) {
		//DBG_8192C("update_network=> orgRSSI(%d)(%d),newRSSI(%d)(%d)\n",dst->Rssi,query_rx_pwr_percentage(dst->Rssi),
		//	src->Rssi,query_rx_pwr_percentage(src->Rssi));
		//select optimum_antenna for before linked =>For antenna diversity
		if (dst->Rssi >=  src->Rssi )//keep org parameter
		{
			src->Rssi = dst->Rssi;
			src->PhyInfo.Optimum_antenna = dst->PhyInfo.Optimum_antenna;
		}
	}
}

// Add new function to reset the state of antenna diversity before link.
u8 AntDivBeforeLink8814(PADAPTER Adapter )
{

	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct dm_struct		* 	pDM_Odm =&pHalData->odmpriv;
	SWAT_T		*pDM_SWAT_Table = &pDM_Odm->DM_SWAT_Table;
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);

	// Condition that does not need to use antenna diversity.
	if (pHalData->AntDivCfg==0) {
		//DBG_8192C("odm_AntDivBeforeLink8192C(): No AntDiv Mechanism.\n");
		return _FALSE;
	}

	if (check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE) {
		return _FALSE;
	}


	if (pDM_SWAT_Table->SWAS_NoLink_State == 0) {
		//switch channel
		pDM_SWAT_Table->SWAS_NoLink_State = 1;
		pDM_SWAT_Table->CurAntenna = (pDM_SWAT_Table->CurAntenna==MAIN_ANT)?AUX_ANT:MAIN_ANT;

		//PHY_SetBBReg(Adapter, rFPGA0_XA_RFInterfaceOE, 0x300, pDM_SWAT_Table->CurAntenna);
		rtw_antenna_select_cmd(Adapter, pDM_SWAT_Table->CurAntenna, _FALSE);
		//DBG_8192C("%s change antenna to ANT_( %s ).....\n",__FUNCTION__, (pDM_SWAT_Table->CurAntenna==MAIN_ANT)?"MAIN":"AUX");
		return _TRUE;
	} else {
		pDM_SWAT_Table->SWAS_NoLink_State = 0;
		return _FALSE;
	}

}
#endif //CONFIG_ANTENNA_DIVERSITY

