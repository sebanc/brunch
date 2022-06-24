/******************************************************************************
 *
 * Copyright(c) 2016 - 2017 Realtek Corporation.
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
#define _RTL8821CU_LED_C_

#include <drv_types.h>		/* PADAPTER */
#include <hal_data.h>		/* PHAL_DATA_TYPE */
#include <hal_com_led.h>	/* PLED_USB */
#include "../../hal_halmac.h" /* HALMAC API */
#ifdef CONFIG_RTW_SW_LED

/*
 * =============================================================================
 * LED object.
 * =============================================================================
 */


/*
 * =============================================================================
 * Prototype of protected function.
 * =============================================================================
 */

/*
 * =============================================================================
 * LED routines.
 * =============================================================================
 */

/*
 * Description:
 * Turn on LED according to LedPin specified.
 */
static void swledon(PADAPTER padapter, PLED_USB led)
{
	PHAL_DATA_TYPE hal = GET_HAL_DATA(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;

	switch (led->LedPin) {
	case LED_PIN_GPIO0:
		break;
	case LED_PIN_LED0:
	case LED_PIN_LED1:
	case LED_PIN_LED2:
	default:
		if (padapter->registrypriv.led_ctrl == 0) {
			rtw_halmac_led_switch(adapter_to_dvobj(padapter), 0);
		} else {
			rtw_halmac_led_switch(adapter_to_dvobj(padapter), 1);
		}
		break;
	}

	if (padapter->registrypriv.led_ctrl == 0) {
		led->bLedOn = _FALSE;
	} else {
		led->bLedOn = _TRUE;
	}
}


/*
 * Description:
 * Turn off LED according to LedPin specified.
 */
static void swledoff(PADAPTER padapter, PLED_USB led)
{
	PHAL_DATA_TYPE hal = GET_HAL_DATA(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;

	switch (led->LedPin) {
	case LED_PIN_GPIO0:
		break;
	case LED_PIN_LED0:
	case LED_PIN_LED1:
	case LED_PIN_LED2:
	default:
		if (padapter->registrypriv.led_ctrl <= 1) {
			rtw_halmac_led_switch(adapter_to_dvobj(padapter), 0);
		} else {
			rtw_halmac_led_switch(adapter_to_dvobj(padapter), 1);
		}
		break;
	}

	if (padapter->registrypriv.led_ctrl <= 1) {
		led->bLedOn = _FALSE;
	} else {
		led->bLedOn = _TRUE;
	}
}

/*
 * =============================================================================
 * Interface to manipulate LED objects.
 * =============================================================================
 */

/*
 * =============================================================================
 * Default LED behavior.
 * =============================================================================
 */

/*
 * Description:
 * Initialize all LED_871x objects.
 */
void rtl8821cu_initswleds(PADAPTER padapter)
{
	struct led_priv *ledpriv = adapter_to_led(padapter);
	u8 enable = 1;
	u8 mode = 3;

	ledpriv->LedControlHandler = LedControlUSB;
	ledpriv->SwLedOn = swledon;
	ledpriv->SwLedOff = swledoff;

	InitLed(padapter, &(ledpriv->SwLed0), LED_PIN_LED0);
	InitLed(padapter, &(ledpriv->SwLed1), LED_PIN_LED1);
	InitLed(padapter, &(ledpriv->SwLed2), LED_PIN_LED2);

}

/*
 * Description:
 * DeInitialize all LED_819xUsb objects.
 */
void rtl8821cu_deinitswleds(PADAPTER padapter)
{
	struct led_priv *ledpriv = adapter_to_led(padapter);
	u8 enable = 0;
	u8 mode = 3;

	DeInitLed(&(ledpriv->SwLed0));
	DeInitLed(&(ledpriv->SwLed1));
	DeInitLed(&(ledpriv->SwLed2));

}
#endif
