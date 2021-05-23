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
#define _RTL8814AU_LED_C_

//#include <drv_types.h>
#include <rtl8814a_hal.h>
#ifdef CONFIG_RTW_SW_LED
//================================================================================
// LED object.
//================================================================================


//================================================================================
//	Prototype of protected function.
//================================================================================


//================================================================================
// LED_819xUsb routines. 
//================================================================================

//
//	Description:
//		Turn on LED according to LedPin specified.
//
static void
SwLedOn_8814AU(
	PADAPTER		padapter, 
	PLED_USB		pLed
)
{
	u32 LedGpioCfg;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;

	LedGpioCfg = rtw_read32(padapter , REG_GPIO_PIN_CTRL_2); /* 0x60. In 8814AU, the name should be REG_GPIO_EXT_CTRL */
	switch (pLed->LedPin) {
	case LED_PIN_LED0:
		LedGpioCfg |= (BIT16 | BIT17 | BIT21 | BIT22);	/* config as gpo */
		LedGpioCfg &= ~(BIT8 | BIT9 | BIT13 | BIT14);	/* set gpo value */
		LedGpioCfg &= ~(BIT0 | BIT1 | BIT5 | BIT6);		/* set gpi value. TBD: may not need this */
		rtw_write32(padapter , REG_GPIO_PIN_CTRL_2 , LedGpioCfg);
		break;
	default:
		break;
	}
	pLed->bLedOn = _TRUE;
}


//
//	Description:
//		Turn off LED according to LedPin specified.
//
static void
SwLedOff_8814AU(
	PADAPTER		padapter, 
	PLED_USB		pLed
)
{
	u32 LedGpioCfg;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;
	LedGpioCfg = rtw_read32(padapter , REG_GPIO_PIN_CTRL_2); /* 0x60. In 8814AU, the name should be REG_GPIO_EXT_CTRL */
	switch (pLed->LedPin) {
	case LED_PIN_LED0:
		LedGpioCfg |= (BIT16 | BIT17 | BIT21 | BIT22);	/* config as gpo */
		LedGpioCfg |= (BIT8 | BIT9 | BIT13 | BIT14);	/* set gpo output value */
		rtw_write32(padapter , REG_GPIO_PIN_CTRL_2 , LedGpioCfg);
		break;
	default:
		break;
	}

	pLed->bLedOn = _FALSE;
}

//================================================================================
// Interface to manipulate LED objects.
//================================================================================


//================================================================================
// Default LED behavior.
//================================================================================

//
//	Description:
//		Initialize all LED_871x objects.
//
void
rtl8814au_InitSwLeds(
	_adapter	*padapter
	)
{
	struct led_priv *pledpriv = adapter_to_led(padapter);

	pledpriv->LedControlHandler = LedControlUSB;

	pledpriv->SwLedOn = SwLedOn_8814AU;
	pledpriv->SwLedOff = SwLedOff_8814AU;

	InitLed(padapter, &(pledpriv->SwLed0), LED_PIN_LED0);

	InitLed(padapter, &(pledpriv->SwLed1), LED_PIN_LED1);

	InitLed(padapter, &(pledpriv->SwLed2), LED_PIN_LED2);
}


//
//	Description:
//		DeInitialize all LED_819xUsb objects.
//
void
rtl8814au_DeInitSwLeds(
	_adapter	*padapter
	)
{
	struct led_priv	*ledpriv = adapter_to_led(padapter);

	DeInitLed( &(ledpriv->SwLed0) );
	DeInitLed( &(ledpriv->SwLed1) );
	DeInitLed( &(ledpriv->SwLed2) );
}
#endif
