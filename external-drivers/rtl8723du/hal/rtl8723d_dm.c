// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * Description:
 *
 * This file is for 92CE/92CU dynamic mechanism only
 *
 *
 * ************************************************************ */
#define _RTL8723D_DM_C_

/* ************************************************************
 * include files
 * ************************************************************ */
#include <rtl8723d_hal.h>

/* ************************************************************
 * Global var
 * ************************************************************ */


#ifdef CONFIG_SUPPORT_HW_WPS_PBC
static void dm_CheckPbcGPIO(struct adapter *adapt)
{
	u8	tmp1byte;
	u8	bPbcPressed = false;

	if (!adapt->registrypriv.hw_wps_pbc)
		return;

	tmp1byte = rtw_read8(adapt, GPIO_IO_SEL);
	tmp1byte |= (HAL_8192C_HW_GPIO_WPS_BIT);
	rtw_write8(adapt, GPIO_IO_SEL, tmp1byte);	/* enable GPIO[2] as output mode */

	tmp1byte &= ~(HAL_8192C_HW_GPIO_WPS_BIT);
	rtw_write8(adapt,  GPIO_IN, tmp1byte);		/* reset the floating voltage level */

	tmp1byte = rtw_read8(adapt, GPIO_IO_SEL);
	tmp1byte &= ~(HAL_8192C_HW_GPIO_WPS_BIT);
	rtw_write8(adapt, GPIO_IO_SEL, tmp1byte);	/* enable GPIO[2] as input mode */

	tmp1byte = rtw_read8(adapt, GPIO_IN);

	if (tmp1byte == 0xff)
		return;

	if (tmp1byte & HAL_8192C_HW_GPIO_WPS_BIT)
		bPbcPressed = true;
	if (bPbcPressed) {
		/* Here we only set bPbcPressed to true */
		/* After trigger PBC, the variable will be set to false */
		RTW_INFO("CheckPbcGPIO - PBC is pressed\n");
		rtw_request_wps_pbc_event(adapt);
	}
}
#endif /* #ifdef CONFIG_SUPPORT_HW_WPS_PBC */

/*
 * Initialize GPIO setting registers
 *   */
static void
dm_InitGPIOSetting(
	struct adapter *	Adapter
)
{
	u8	tmp1byte;

	tmp1byte = rtw_read8(Adapter, REG_GPIO_MUXCFG);
	tmp1byte &= (GPIOSEL_GPIO | ~GPIOSEL_ENBT);

	rtw_write8(Adapter, REG_GPIO_MUXCFG, tmp1byte);
}
/* ************************************************************
 * functions
 * ************************************************************ */
static void Init_ODM_ComInfo_8723d(struct adapter *	Adapter)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(Adapter);
	struct PHY_DM_STRUCT		*pDM_Odm = &(pHalData->odmpriv);
	u8	cut_ver, fab_ver;

	Init_ODM_ComInfo(Adapter);

	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_PACKAGE_TYPE, pHalData->PackageType);

	fab_ver = ODM_TSMC;
	cut_ver = GET_CVID_CUT_VERSION(pHalData->version_id);

	RTW_INFO("%s(): fab_ver=%d cut_ver=%d\n", __func__, fab_ver, cut_ver);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_FAB_VER, fab_ver);
	odm_cmn_info_init(pDM_Odm, ODM_CMNINFO_CUT_VER, cut_ver);
}

void
rtl8723d_InitHalDm(
	struct adapter *	Adapter
)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(Adapter);
	struct PHY_DM_STRUCT		*pDM_Odm = &(pHalData->odmpriv);

	dm_InitGPIOSetting(Adapter);
	odm_dm_init(pDM_Odm);

}

void
rtl8723d_HalDmWatchDog(
	struct adapter *	Adapter
)
{
	bool		bFwCurrentInPSMode = false;
	u8 bFwPSAwake = true;

	if (!rtw_is_hw_init_completed(Adapter))
		goto skip_dm;

	bFwCurrentInPSMode = adapter_to_pwrctl(Adapter)->bFwCurrentInPSMode;
	rtw_hal_get_hwreg(Adapter, HW_VAR_FWLPS_RF_ON, &bFwPSAwake);

	/* Fw is under p2p powersaving mode, driver should stop dynamic mechanism. */
	/* modifed by thomas. 2011.06.11. */
	if (Adapter->wdinfo.p2p_ps_mode)
		bFwPSAwake = false;

	if ((rtw_is_hw_init_completed(Adapter)) &&
	    ((!bFwCurrentInPSMode) && bFwPSAwake))
		rtw_hal_check_rxfifo_full(Adapter);

#ifdef CONFIG_DISABLE_ODM
	goto skip_dm;
#endif
	rtw_phydm_watchdog(Adapter);

skip_dm:

	/* Check GPIO to determine current RF on/off and Pbc status. */
	/* Check Hardware Radio ON/OFF or not */
	/* if(Adapter->MgntInfo.PowerSaveControl.bGpioRfSw) */
	/* { */
	/* RTPRINT(FPWR, PWRHW, ("dm_CheckRfCtrlGPIO\n")); */
	/*	dm_CheckRfCtrlGPIO(Adapter); */
	/* } */
#ifdef CONFIG_SUPPORT_HW_WPS_PBC
	dm_CheckPbcGPIO(Adapter);
#endif
	return;
}

void rtl8723d_init_dm_priv(struct adapter * Adapter)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(Adapter);
	struct PHY_DM_STRUCT		*podmpriv = &pHalData->odmpriv;

	Init_ODM_ComInfo_8723d(Adapter);
	odm_init_all_timers(podmpriv);

}

void rtl8723d_deinit_dm_priv(struct adapter * Adapter)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(Adapter);
	struct PHY_DM_STRUCT		*podmpriv = &pHalData->odmpriv;

	odm_cancel_all_timers(podmpriv);

}
