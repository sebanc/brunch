/******************************************************************************
 *
 * Copyright(c) 2015 - 2017 Realtek Corporation.
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
#define _RTL8822BU_OPS_C_

#include <drv_types.h>			/* PADAPTER, basic_types.h and etc. */
#include <hal_intf.h>			/* struct hal_ops */
#include <hal_data.h>			/* HAL_DATA_TYPE */
#include "../../hal_halmac.h"		/* register define */
#include "../rtl8822b.h"		/* 8822b hal common define. rtl8822bu_init_default_value ...*/
#include "rtl8822bu.h"			/* 8822bu functions */

#ifdef CONFIG_SUPPORT_USB_INT
static void rtl8822bu_interrupt_handler(PADAPTER padapter, u16 pkt_len, u8 *pbuf)
{	
	}
#endif /* CONFIG_SUPPORT_USB_INT */

void rtl8822bu_set_hw_type(struct dvobj_priv *pdvobj)
{
	pdvobj->HardwareType = HARDWARE_TYPE_RTL8822BU;
	RTW_INFO("CHIP TYPE: RTL8822B\n");
}

static u8 sethwreg(PADAPTER padapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	struct registry_priv *registry_par = &padapter->registrypriv;
	int status = 0;
	u8 ret = _SUCCESS;

	switch (variable) {
	case HW_VAR_RXDMA_AGG_PG_TH:
#ifdef CONFIG_USB_RX_AGGREGATION
		if (pdvobjpriv->traffic_stat.cur_tx_tp < 1 && pdvobjpriv->traffic_stat.cur_rx_tp < 1) {
			/* for low traffic, do not usb AGGREGATION */
			pHalData->rxagg_usb_timeout = 0x01;
			pHalData->rxagg_usb_size = 0x01;

		} else {
#ifdef CONFIG_PLATFORM_NOVATEK_NT72668
			pHalData->rxagg_usb_timeout = 0x20;
			pHalData->rxagg_usb_size = 0x03;
#elif defined(CONFIG_PLATFORM_HISILICON)
			/* use 16k to workaround for HISILICON platform */
			pHalData->rxagg_usb_timeout = 8;
			pHalData->rxagg_usb_size = 3;
#else
			/* default setting */
			pHalData->rxagg_usb_timeout = 0x20;
			pHalData->rxagg_usb_size = 0x05;
#endif
		}
		rtw_halmac_rx_agg_switch(pdvobjpriv, _TRUE);
#if 0
		RTW_INFO("\n==========RAFFIC_STATISTIC==============\n");
		RTW_INFO("cur_tx_bytes:%lld\n", pdvobjpriv->traffic_stat.cur_tx_bytes);
		RTW_INFO("cur_rx_bytes:%lld\n", pdvobjpriv->traffic_stat.cur_rx_bytes);

		RTW_INFO("last_tx_bytes:%lld\n", pdvobjpriv->traffic_stat.last_tx_bytes);
		RTW_INFO("last_rx_bytes:%lld\n", pdvobjpriv->traffic_stat.last_rx_bytes);

		RTW_INFO("cur_tx_tp:%d\n", pdvobjpriv->traffic_stat.cur_tx_tp);
		RTW_INFO("cur_rx_tp:%d\n", pdvobjpriv->traffic_stat.cur_rx_tp);
		RTW_INFO("\n========================\n");
#endif
#endif
		break;
	case HW_VAR_SET_RPWM:
#ifdef CONFIG_LPS_LCLK
		{
			u8	ps_state = *((u8 *)val);

			/*rpwm value only use BIT0(clock bit) ,BIT6(Ack bit), and BIT7(Toggle bit) for 88e.
			BIT0 value - 1: 32k, 0:40MHz.
			BIT6 value - 1: report cpwm value after success set, 0:do not report.
			BIT7 value - Toggle bit change.
			modify by Thomas. 2012/4/2.*/
			ps_state = ps_state & 0xC1;
			/* RTW_INFO("##### Change RPWM value to = %x for switch clk #####\n", ps_state); */
			rtw_write8(padapter, REG_USB_HRPWM_8822B, ps_state);
		}
#endif
		break;
	case HW_VAR_AMPDU_MAX_TIME:
		rtw_write8(padapter, REG_AMPDU_MAX_TIME_V1_8822B, 0x70);
		break;
	case HW_VAR_USB_MODE:
		/* U2 to U3 */
		if (registry_par->switch_usb_mode == 1) {
			if (IS_HIGH_SPEED_USB(padapter)) {
				status = rtw_halmac_switch_usb_mode(adapter_to_dvobj(padapter), RTW_USB_SPEED_3);
				if (status)
					*val = _TRUE;
			}
		} else if (registry_par->switch_usb_mode == 2) {
			/* U3 to U2 */
			if (IS_SUPER_SPEED_USB(padapter)) {
				status = rtw_halmac_switch_usb_mode(adapter_to_dvobj(padapter), RTW_USB_SPEED_2);
				if (status)
					*val = _TRUE;
			}
		}
		break;
	default:
		ret = rtl8822b_sethwreg(padapter, variable, val);
		break;
	}

	return ret;
}

static void gethwreg(PADAPTER padapter, u8 variable, u8 *val)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);

	switch (variable) {
	case HW_VAR_CPWM:
#ifdef CONFIG_LPS_LCLK
		*val = rtw_read8(padapter, REG_USB_HCPWM_8822B);
		/* RTW_INFO("##### REG_USB_HCPWM(0x%02x) = 0x%02x #####\n", REG_USB_HCPWM_8822B, *val); */
#endif /* CONFIG_LPS_LCLK */
		break;
	case HW_VAR_RPWM_TOG:
#ifdef CONFIG_LPS_LCLK
		*val = rtw_read8(padapter, REG_USB_HRPWM_8822B);
		*val &= BIT_TOGGLE_8822B;
#endif /* CONFIG_LPS_LCLK */
		break;

	default:
		rtl8822b_gethwreg(padapter, variable, val);
		break;
	}

}

/*
	Description:
		Change default setting of specified variable.
*/
static u8 sethaldefvar(PADAPTER padapter, HAL_DEF_VARIABLE eVariable, void *pValue)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u8 bResult = _SUCCESS;

	switch (eVariable) {
	default:
		rtl8822b_sethaldefvar(padapter, eVariable, pValue);
		break;
	}

	return bResult;
}

/*
	Description:
		Query setting of specified variable.
*/
static u8 gethaldefvar(PADAPTER	padapter, HAL_DEF_VARIABLE eVariable, void *pValue)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u8 bResult = _SUCCESS;

	switch (eVariable) {
	case HW_VAR_MAX_RX_AMPDU_FACTOR:
		*(HT_CAP_AMPDU_FACTOR *)pValue = MAX_AMPDU_FACTOR_64K;
		break;
	default:
		bResult = rtl8822b_gethaldefvar(padapter, eVariable, pValue);
		break;
	}

	return bResult;
}

static void rtl8822bu_init_default_value(PADAPTER	padapter)
{
	rtl8822b_init_default_value(padapter);
}

static u8 rtl8822bu_ps_func(PADAPTER padapter, HAL_INTF_PS_FUNC efunc_id, u8 *val)
{
	u8 bResult = _TRUE;

	switch (efunc_id) {

#if defined(CONFIG_AUTOSUSPEND) && defined(SUPPORT_HW_RFOFF_DETECTED)
	case HAL_USB_SELECT_SUSPEND:
		break;
#endif /* CONFIG_AUTOSUSPEND && SUPPORT_HW_RFOFF_DETECTED */

	default:
		break;
	}
	return bResult;
}

#ifdef CONFIG_RTW_LED
static void read_ledsetting(PADAPTER adapter)
{
	struct led_priv *ledpriv = adapter_to_led(adapter);

#ifdef CONFIG_RTW_SW_LED
	PHAL_DATA_TYPE hal;
	
	hal = GET_HAL_DATA(adapter);
	ledpriv->bRegUseLed = _TRUE;

	switch (hal->EEPROMCustomerID) {
	default:
		hal->CustomerID = RT_CID_DEFAULT;
		break;
	}

	switch (hal->CustomerID) {
	case RT_CID_DEFAULT:
	default:
		ledpriv->LedStrategy = SW_LED_MODE9;
		break;
	}
#else /* HW LED */
	ledpriv->LedStrategy = HW_LED;
#endif /* CONFIG_RTW_SW_LED */
}
#endif /* CONFIG_RTW_LED */
 

/*
 * Description:
 *	Collect all hardware information, fill "HAL_DATA_TYPE".
 *	Sometimes this would be used to read MAC address.
 *	This function will do
 *	1. Read Efuse/EEPROM to initialize
 *	2. Read registers to initialize
 *	3. Other vaiables initialization
 */
static u8 read_adapter_info(PADAPTER padapter)
{
	u8 ret = _FAIL;

	/*
	 * 1. Read Efuse/EEPROM to initialize
	 */
	if (rtl8822b_read_efuse(padapter) != _SUCCESS)
		goto exit;

	/*
	 * 2. Read registers to initialize
	 */

	/*
	 * 3. Other Initialization
	 */

#ifdef CONFIG_RTW_LED
	read_ledsetting(padapter);
#endif /* CONFIG_RTW_LED */

	ret = _SUCCESS;

exit:
	return ret;
}


void rtl8822bu_set_hal_ops(PADAPTER padapter)
{
	struct hal_ops *ops;
	int err;


	err = rtl8822bu_halmac_init_adapter(padapter);
	if (err) {
		RTW_INFO("%s: [ERROR]HALMAC initialize FAIL!\n", __func__);
		return;
	}

	rtl8822b_set_hal_ops(padapter);

	ops = &padapter->hal_func;

	ops->hal_init = rtl8822bu_init;
	ops->hal_deinit = rtl8822bu_deinit;

	ops->inirp_init = rtl8822bu_inirp_init;
	ops->inirp_deinit = rtl8822bu_inirp_deinit;

	ops->init_xmit_priv = rtl8822bu_init_xmit_priv;
	ops->free_xmit_priv = rtl8822bu_free_xmit_priv;

	ops->init_recv_priv = rtl8822bu_init_recv_priv;
	ops->free_recv_priv = rtl8822bu_free_recv_priv;
#ifdef CONFIG_RTW_SW_LED
	ops->InitSwLeds = rtl8822bu_initswleds;
	ops->DeInitSwLeds = rtl8822bu_deinitswleds;
#endif

	ops->init_default_value = rtl8822bu_init_default_value;
	ops->intf_chip_configure = rtl8822bu_interface_configure;
	ops->read_adapter_info = read_adapter_info;

	ops->set_hw_reg_handler = sethwreg;
	ops->GetHwRegHandler = gethwreg;
	ops->get_hal_def_var_handler = gethaldefvar;
	ops->SetHalDefVarHandler = sethaldefvar;


	ops->hal_xmit = rtl8822bu_hal_xmit;
	ops->mgnt_xmit = rtl8822bu_mgnt_xmit;
	ops->hal_xmitframe_enqueue = rtl8822bu_hal_xmitframe_enqueue;

#ifdef CONFIG_HOSTAPD_MLME
	ops->hostap_mgnt_xmit_entry = rtl8822bu_hostap_mgnt_xmit_entry;
#endif
	ops->interface_ps_func = rtl8822bu_ps_func;
#ifdef CONFIG_XMIT_THREAD_MODE
	ops->xmit_thread_handler = rtl8822bu_xmit_buf_handler;
#endif
#ifdef CONFIG_SUPPORT_USB_INT
	ops->interrupt_handler = rtl8822bu_interrupt_handler;
#endif


}
