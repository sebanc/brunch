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

#include "drv_types.h"
#include <rtl8723d_hal.h>

/* ================================================================================
 * Prototype of protected function.
 * ================================================================================ */

/* ================================================================================
 * LED_819xUsb routines.
 * ================================================================================ */

/*
 *	Description:
 *		Turn on LED according to LedPin specified.
 */
VOID
SwLedOn_8723DE(
	IN	PADAPTER		Adapter,
	IN	PLED_PCIE		pLed
)
{
	u16	LedReg = REG_LEDCFG0;
	u8	LedCfg = 0;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*ledpriv = &(Adapter->ledpriv);

	if (RTW_CANNOT_RUN(Adapter))
		return;

	pLed->bLedOn = _TRUE;
}


/*
 *	Description:
 *		Turn off LED according to LedPin specified.
 */
VOID
SwLedOff_8723DE(
	IN	PADAPTER		Adapter,
	IN	PLED_PCIE		pLed
)
{
	u16	LedReg = REG_LEDCFG0;
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*ledpriv = &(Adapter->ledpriv);

	if (RTW_CANNOT_RUN(Adapter))
		return;

	pLed->bLedOn = _FALSE;
}


/*
 *	Description:
 *		Initialize all LED_871x objects.
 */
void
rtl8723de_InitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv *pledpriv = &(padapter->ledpriv);

	pledpriv->LedControlHandler = LedControlPCIE;

	pledpriv->SwLedOn = SwLedOn_8723DE;
	pledpriv->SwLedOff = SwLedOff_8723DE;

	InitLed(padapter, &(pledpriv->SwLed0), LED_PIN_LED0);

	InitLed(padapter, &(pledpriv->SwLed1), LED_PIN_LED1);
}


/*
 *	Description:
 *		DeInitialize all LED_819xUsb objects.
 */
void
rtl8723de_DeInitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv	*ledpriv = &(padapter->ledpriv);

	DeInitLed(&(ledpriv->SwLed0));
	DeInitLed(&(ledpriv->SwLed1));
}
