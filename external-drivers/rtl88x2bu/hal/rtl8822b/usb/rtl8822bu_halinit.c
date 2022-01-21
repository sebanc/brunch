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
#define _RTL8822BU_HALINIT_C_

#include <hal_data.h>			/* HAL DATA */
#include "../../hal_halmac.h"		/* HALMAC API */
#include "../rtl8822b.h"		/* rtl8822b hal common define */
#include "rtl8822bu.h"

#ifndef CONFIG_USB_HCI

	#error "CONFIG_USB_HCI shall be on!\n"

#endif

#ifdef CONFIG_FWLPS_IN_IPS
u8 rtl8822bu_fw_ips_init(_adapter *padapter)
{
	struct sreset_priv *psrtpriv = &GET_HAL_DATA(padapter)->srestpriv;
	struct debug_priv *pdbgpriv = &adapter_to_dvobj(padapter)->drv_dbg;
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);

	if (pwrctl->bips_processing == _TRUE && psrtpriv->silent_reset_inprogress == _FALSE
		&& GET_HAL_DATA(padapter)->bFWReady == _TRUE && pwrctl->pre_ips_type == 0) {
		systime start_time;
		u8 cpwm_orig, cpwm_now, rpwm;
		u8 bMacPwrCtrlOn = _TRUE;

		RTW_INFO("%s: Leaving FW_IPS\n", __func__);
#ifdef CONFIG_LPS_LCLK
		/* for polling cpwm */
		cpwm_orig = 0;
		rtw_hal_get_hwreg(padapter, HW_VAR_CPWM, &cpwm_orig);

		/* set rpwm */
		rtw_hal_get_hwreg(padapter, HW_VAR_RPWM_TOG, &rpwm);
		rpwm += 0x80;
		rpwm |= PS_ACK;
		rtw_hal_set_hwreg(padapter, HW_VAR_SET_RPWM, (u8 *)(&rpwm));


		RTW_INFO("%s: write rpwm=%02x\n", __func__, rpwm);

		pwrctl->tog = (rpwm + 0x80) & 0x80;

		/* do polling cpwm */
		start_time = rtw_get_current_time();
		do {

			rtw_mdelay_os(1);

			rtw_hal_get_hwreg(padapter, HW_VAR_CPWM, &cpwm_now);
			if ((cpwm_orig ^ cpwm_now) & 0x80) {
				#ifdef DBG_CHECK_FW_PS_STATE
				RTW_INFO("%s: polling cpwm ok when leaving IPS in FWLPS state, cpwm_orig=%02x, cpwm_now=%02x, 0x100=0x%x\n"
					, __func__, cpwm_orig, cpwm_now, rtw_read8(padapter, REG_CR));
				#endif /* DBG_CHECK_FW_PS_STATE */
				break;
			}

			if (rtw_get_passing_time_ms(start_time) > 100) {
				RTW_INFO("%s: polling cpwm timeout when leaving IPS in FWLPS state, cpwm_orig=%02x, cpwm_now=%02x, 0x100=0x%x\n",
					__func__, cpwm_orig, cpwm_now, rtw_read8(padapter, REG_CR));
				break;
			}
		} while (1);
#endif /* CONFIG_LPS_LCLK */
		rtl8822b_set_FwPwrModeInIPS_cmd(padapter, 0);

		rtw_hal_set_hwreg(padapter, HW_VAR_APFM_ON_MAC, &bMacPwrCtrlOn);
#ifdef CONFIG_LPS_LCLK
		#ifdef DBG_CHECK_FW_PS_STATE
		if (rtw_fw_ps_state(padapter) == _FAIL) {
			RTW_INFO("after hal init, fw ps state in 32k\n");
			pdbgpriv->dbg_ips_drvopen_fail_cnt++;
		}
		#endif /* DBG_CHECK_FW_PS_STATE */
#endif /* CONFIG_LPS_LCLK */
		return _SUCCESS;
	}
	return _FAIL;
}

u8 rtl8822bu_fw_ips_deinit(_adapter *padapter)
{
	struct sreset_priv *psrtpriv =  &GET_HAL_DATA(padapter)->srestpriv;
	struct debug_priv *pdbgpriv = &adapter_to_dvobj(padapter)->drv_dbg;
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);

	if (pwrctl->bips_processing == _TRUE && psrtpriv->silent_reset_inprogress == _FALSE
		&& GET_HAL_DATA(padapter)->bFWReady == _TRUE && padapter->netif_up == _TRUE) {
		int cnt = 0;
		u8 val8 = 0, rpwm;

		RTW_INFO("%s: issue H2C to FW when entering IPS\n", __func__);

		rtl8822b_set_FwPwrModeInIPS_cmd(padapter, 0x1);
#ifdef CONFIG_LPS_LCLK
		/* poll 0x1cc to make sure H2C command already finished by FW; MAC_0x1cc=0 means H2C done by FW. */
		do {
			val8 = rtw_read8(padapter, REG_HMETFR);
			cnt++;
			RTW_INFO("%s  polling REG_HMETFR=0x%x, cnt=%d\n", __func__, val8, cnt);
			rtw_mdelay_os(10);
		} while (cnt < 100 && (val8 != 0));

		/* H2C done, enter 32k */
		if (val8 == 0) {
			/* set rpwm to enter 32k */
			rtw_hal_get_hwreg(padapter, HW_VAR_RPWM_TOG, &rpwm);
			rpwm += 0x80;
			rpwm |= BIT_SYS_CLK_8822B;
			rtw_hal_set_hwreg(padapter, HW_VAR_SET_RPWM, (u8 *)(&rpwm));
			RTW_INFO("%s: write rpwm=%02x\n", __func__, rpwm);
			pwrctl->tog = (val8 + 0x80) & 0x80;

			cnt = val8 = 0;
			do {
				val8 = rtw_read8(padapter, REG_CR);
				cnt++;
				RTW_INFO("%s  polling 0x100=0x%x, cnt=%d\n", __func__, val8, cnt);
				rtw_mdelay_os(10);
			} while (cnt < 100 && (val8 != 0xEA));

			#ifdef DBG_CHECK_FW_PS_STATE
			if (val8 != 0xEA)
				RTW_INFO("MAC_1C0=%08x, MAC_1C4=%08x, MAC_1C8=%08x, MAC_1CC=%08x\n"
					, rtw_read32(padapter, 0x1c0), rtw_read32(padapter, 0x1c4)
					, rtw_read32(padapter, 0x1c8), rtw_read32(padapter, REG_HMETFR));
			#endif /* DBG_CHECK_FW_PS_STATE */
		} else {
			RTW_INFO("MAC_1C0=%08x, MAC_1C4=%08x, MAC_1C8=%08x, MAC_1CC=%08x\n"
				, rtw_read32(padapter, 0x1c0), rtw_read32(padapter, 0x1c4)
				, rtw_read32(padapter, 0x1c8), rtw_read32(padapter, REG_HMETFR));
		}

		RTW_INFO("polling done when entering IPS, check result : 0x100=0x%x, cnt=%d, MAC_1cc=0x%02x\n"
			, rtw_read8(padapter, REG_CR), cnt, rtw_read8(padapter, REG_HMETFR));

		pwrctl->pre_ips_type = 0;
#endif /* CONFIG_LPS_LCLK */
		return _SUCCESS;
	}

	pdbgpriv->dbg_carddisable_cnt++;
	pwrctl->pre_ips_type = 1;

	return _FAIL;

}

#endif

#ifdef CONFIG_RTW_LED
static void init_hwled(PADAPTER adapter, u8 enable)
{
	u8 mode = 0;
	struct led_priv *ledpriv = adapter_to_led(adapter);

	if (ledpriv->LedStrategy != HW_LED)
		return;

	rtw_halmac_led_cfg(adapter_to_dvobj(adapter), enable, mode);
}
#endif

static void hal_init_misc(PADAPTER adapter)
{
#ifdef CONFIG_RTW_LED
	struct led_priv *ledpriv = adapter_to_led(adapter);

	init_hwled(adapter, 1);
#ifdef CONFIG_RTW_SW_LED
	if (ledpriv->bRegUseLed == _TRUE)
		rtw_halmac_led_cfg(adapter_to_dvobj(adapter), _TRUE, 3);
#endif
#endif /* CONFIG_RTW_LED */

}

u32 rtl8822bu_init(PADAPTER padapter)
{
	u8 status = _SUCCESS;
	systime init_start_time = rtw_get_current_time();

#ifdef CONFIG_FWLPS_IN_IPS
	if (_SUCCESS == rtl8822bu_fw_ips_init(padapter))
		goto exit;
#endif

	rtl8822b_init(padapter);

	hal_init_misc(padapter);

#ifdef CONFIG_FWLPS_IN_IPS
exit:
#endif
	RTW_INFO("%s in %dms\n", __func__, rtw_get_passing_time_ms(init_start_time));
	return status;
}

static void hal_deinit_misc(PADAPTER adapter)
{
#ifdef CONFIG_RTW_LED
	struct led_priv *ledpriv = adapter_to_led(adapter);

	init_hwled(adapter, 0);
#ifdef CONFIG_RTW_SW_LED
	if (ledpriv->bRegUseLed == _TRUE)
		rtw_halmac_led_cfg(adapter_to_dvobj(adapter), _FALSE, 3);
#endif
#endif /* CONFIG_RTW_LED */
}

u32 rtl8822bu_deinit(PADAPTER padapter)
{
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv *pobj_priv = adapter_to_dvobj(padapter);
	u8 status = _TRUE;

	RTW_INFO("==> %s\n", __func__);

#ifdef CONFIG_FWLPS_IN_IPS
	if (_SUCCESS == rtl8822bu_fw_ips_deinit(padapter))
		goto exit;
#endif

	hal_deinit_misc(padapter);
	status = rtl8822b_deinit(padapter);
	if (status == _FALSE) {
		RTW_INFO("%s: rtl8822b_hal_deinit fail\n", __func__);
		return _FAIL;
	}

#ifdef CONFIG_FWLPS_IN_IPS
exit:
#endif
	RTW_INFO("%s <==\n", __func__);
	return _SUCCESS;
}


u32 rtl8822bu_inirp_init(PADAPTER padapter)
{
	u8 i, status;
	struct recv_buf *precvbuf;
	struct dvobj_priv *pdev = adapter_to_dvobj(padapter);
	struct intf_hdl *pintfhdl = &padapter->iopriv.intf;
	struct recv_priv *precvpriv = &(padapter->recvpriv);
	u32(*_read_port)(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *pmem);
#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u32(*_read_interrupt)(struct intf_hdl *pintfhdl, u32 addr);
#endif

#ifdef CONFIG_FWLPS_IN_IPS
	/* Do not sumbit urb repeat */
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(padapter);

	if (pwrctl->bips_processing == _TRUE) {
		status = _SUCCESS;
		goto exit;
	}
#endif /* CONFIG_FWLPS_IN_IPS */

	_read_port = pintfhdl->io_ops._read_port;

	status = _SUCCESS;


	precvpriv->ff_hwaddr = RECV_BULK_IN_ADDR;

	/* issue Rx irp to receive data */
	precvbuf = (struct recv_buf *)precvpriv->precv_buf;
	for (i = 0; i < NR_RECVBUFF; i++) {
		if (_read_port(pintfhdl, precvpriv->ff_hwaddr, 0, (u8 *)precvbuf) == _FALSE) {
			status = _FAIL;
			goto exit;
		}

		precvbuf++;
		precvpriv->free_recv_buf_queue_cnt--;
	}

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	if (pdev->RtInPipe[REALTEK_USB_IN_INT_EP_IDX] != 0x05) {
		status = _FAIL;
		RTW_INFO("%s =>Warning !! Have not USB Int-IN pipe, RtIntInPipe(%d)!!!\n", __func__, pdev->RtInPipe[REALTEK_USB_IN_INT_EP_IDX]);
		goto exit;
	}
	_read_interrupt = pintfhdl->io_ops._read_interrupt;
	if (_read_interrupt(pintfhdl, RECV_INT_IN_ADDR) == _FALSE) {
		status = _FAIL;
	}
#endif

exit:



	return status;

}

u32 rtl8822bu_inirp_deinit(PADAPTER padapter)
{

	rtw_read_port_cancel(padapter);


	return _SUCCESS;
}

void rtl8822bu_update_interrupt_mask(PADAPTER padapter, u8 bHIMR0 , u32 AddMSR, u32 RemoveMSR)
{
	HAL_DATA_TYPE *pHalData;
	u32 *himr;

	pHalData = GET_HAL_DATA(padapter);

	if (bHIMR0)
		himr = &(pHalData->IntrMask[0]);
	else
		himr = &(pHalData->IntrMask[1]);

	if (AddMSR)
		*himr |= AddMSR;

	if (RemoveMSR)
		*himr &= (~RemoveMSR);

	if (bHIMR0)
		rtw_write32(padapter, REG_HIMR0_8822B, *himr);
	else
		rtw_write32(padapter, REG_HIMR1_8822B, *himr);

}

static void config_chip_out_EP(PADAPTER padapter, u8 NumOutPipe)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);


	pHalData->OutEpQueueSel = 0;
	pHalData->OutEpNumber = 0;

	switch (NumOutPipe) {
	case 4:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 4;
		break;
	case 3:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 3;
		break;
	case 2:
		pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_NQ;
		pHalData->OutEpNumber = 2;
		break;
	case  1:
		pHalData->OutEpQueueSel = TX_SELE_HQ;
		pHalData->OutEpNumber = 1;
		break;
	default:
		break;
	}

	RTW_INFO("%s OutEpQueueSel(0x%02x), OutEpNumber(%d)\n", __func__, pHalData->OutEpQueueSel, pHalData->OutEpNumber);

}

static u8 usb_set_queue_pipe_mapping(PADAPTER padapter, u8 NumInPipe, u8 NumOutPipe)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	u8 result = _FALSE;

	config_chip_out_EP(padapter, NumOutPipe);

	/* Normal chip with one IN and one OUT doesn't have interrupt IN EP. */
	if (1 == pHalData->OutEpNumber) {
		if (1 != NumInPipe)
			return result;
	}

	result = Hal_MappingOutPipe(padapter, NumOutPipe);

	return result;

}

void rtl8822bu_interface_configure(PADAPTER padapter)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(padapter);
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(padapter);

	if (IS_SUPER_SPEED_USB(padapter))
		pHalData->UsbBulkOutSize = USB_SUPER_SPEED_BULK_SIZE;
	else if (IS_HIGH_SPEED_USB(padapter))
		pHalData->UsbBulkOutSize = USB_HIGH_SPEED_BULK_SIZE;
	else
		pHalData->UsbBulkOutSize = USB_FULL_SPEED_BULK_SIZE;

#ifdef CONFIG_USB_TX_AGGREGATION
	/* according to value defined by halmac */
	pHalData->UsbTxAggMode		= 1;
	rtw_halmac_usb_get_txagg_desc_num(pdvobjpriv, &pHalData->UsbTxAggDescNum);
#endif /* CONFIG_USB_TX_AGGREGATION */

#ifdef CONFIG_USB_RX_AGGREGATION
	/* according to value defined by halmac */
	pHalData->rxagg_mode = RX_AGG_USB;
#ifdef CONFIG_PLATFORM_NOVATEK_NT72668
	pHalData->rxagg_usb_size = 0x03;
	pHalData->rxagg_usb_timeout = 0x20;
#elif defined(CONFIG_PLATFORM_HISILICON)
	 /* use 16k to workaround for HISILICON platform */
	pHalData->rxagg_usb_size = 3;
	pHalData->rxagg_usb_timeout = 8;
#endif /* CONFIG_PLATFORM_NOVATEK_NT72668 */
#endif /* CONFIG_USB_RX_AGGREGATION */

	usb_set_queue_pipe_mapping(padapter,
			   pdvobjpriv->RtNumInPipes, pdvobjpriv->RtNumOutPipes);
}
