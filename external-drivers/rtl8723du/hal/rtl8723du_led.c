// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#include "rtl8723d_hal.h"

/*
 * ================================================================================
 * LED object.
 * ================================================================================
 */


/*
 * ================================================================================
 * Prototype of protected function.
 * ================================================================================
 */

/*
 * ================================================================================
 * LED_819xUsb routines.
 * ================================================================================
 */

/*
 * Description:
 * Turn on LED according to LedPin specified.
 */
static void
SwLedOn_8723DU(
	struct adapter * adapt,
	struct led_usb * pLed
)
{
	if (RTW_CANNOT_RUN(adapt))
		return;

	pLed->bLedOn = true;

}


/*
 * Description:
 * Turn off LED according to LedPin specified.
 */
static void
SwLedOff_8723DU(
	struct adapter * adapt,
	struct led_usb * pLed
)
{
	if (RTW_CANNOT_RUN(adapt))
		goto exit;

exit:
	pLed->bLedOn = false;
}

/*
 * ================================================================================
 * Interface to manipulate LED objects.
 * ================================================================================
 */

/*
 * ================================================================================
 * Default LED behavior.
 * ================================================================================
 */

/*
 * Description:
 * Initialize all LED_871x objects.
 */
void
rtl8723du_InitSwLeds(
	struct adapter * adapt
)
{
	struct led_priv *pledpriv = &(adapt->ledpriv);

	pledpriv->LedControlHandler = LedControlUSB;

	pledpriv->SwLedOn = SwLedOn_8723DU;
	pledpriv->SwLedOff = SwLedOff_8723DU;

	InitLed(adapt, &(pledpriv->SwLed0), LED_PIN_LED0);

	InitLed(adapt, &(pledpriv->SwLed1), LED_PIN_LED1);
}


/*
 * Description:
 * DeInitialize all LED_819xUsb objects.
 */
void
rtl8723du_DeInitSwLeds(
	struct adapter * adapt
)
{
	struct led_priv *ledpriv = &(adapt->ledpriv);

	DeInitLed(&(ledpriv->SwLed0));
	DeInitLed(&(ledpriv->SwLed1));
}
