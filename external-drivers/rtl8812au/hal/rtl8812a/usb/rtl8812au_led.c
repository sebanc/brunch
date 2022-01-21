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
#define _RTL8812AU_LED_C_

/* #include <drv_types.h> */
#include <rtl8812a_hal.h>
#ifdef CONFIG_RTW_SW_LED

/* ********************************************************************************
 * LED object.
 * ******************************************************************************** */


/* ********************************************************************************
 *	Prototype of protected function.
 * ******************************************************************************** */


/* ********************************************************************************
 * LED_819xUsb routines.
 * ******************************************************************************** */

/*
 *	Description:
 *		Turn on LED according to LedPin specified.
 *   */
static void
SwLedOn_8812AU(
	PADAPTER		padapter,
	PLED_USB		pLed
)
{
	u8	LedCfg;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;

	if (RT_GetInterfaceSelection(padapter) == INTF_SEL2_MINICARD ||
	    RT_GetInterfaceSelection(padapter) == INTF_SEL3_USB_Solo ||
	    RT_GetInterfaceSelection(padapter) == INTF_SEL4_USB_Combo) {
		LedCfg = rtw_read8(padapter, REG_LEDCFG2);
		switch (pLed->LedPin) {
		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:
			LedCfg = rtw_read8(padapter, REG_LEDCFG2);
			rtw_write8(padapter, REG_LEDCFG2, (LedCfg & 0xf0) | BIT5 | BIT6); /* SW control led0 on. */
			break;

		case LED_PIN_LED1:
			rtw_write8(padapter, REG_LEDCFG2, (LedCfg & 0x0f) | BIT5); /* SW control led1 on. */
			break;

		default:
			break;
		}
	} else {
		switch (pLed->LedPin) {
		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:
			if (pHalData->AntDivCfg == 0) {
				LedCfg = rtw_read8(padapter, REG_LEDCFG0);
				rtw_write8(padapter, REG_LEDCFG0, (LedCfg & 0x70) | BIT5); /* SW control led0 on. */
			} else {
				LedCfg = rtw_read8(padapter, REG_LEDCFG2) & 0xe0;
				rtw_write8(padapter, REG_LEDCFG2, LedCfg | BIT7 | BIT6 | BIT5); /* SW control led0 on. */
			}
			break;

		case LED_PIN_LED1:
			LedCfg = rtw_read8(padapter, (REG_LEDCFG1));
			rtw_write8(padapter, (REG_LEDCFG1), (LedCfg & 0x70) | BIT5); /* SW control led1 on. */
			break;

		case LED_PIN_LED2:
			LedCfg = rtw_read8(padapter, (REG_LEDCFG2));
			rtw_write8(padapter, (REG_LEDCFG2), (LedCfg & 0x70) | BIT5); /* SW control led1 on. */
			break;

		default:
			break;
		}
	}

	pLed->bLedOn = _TRUE;
}


/*
 *	Description:
 *		Turn off LED according to LedPin specified.
 *   */
static void
SwLedOff_8812AU(
	PADAPTER		padapter,
	PLED_USB		pLed
)
{
	u8	LedCfg;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);

	if (RTW_CANNOT_RUN(padapter))
		return;

	if (RT_GetInterfaceSelection(padapter) == INTF_SEL2_MINICARD ||
	    RT_GetInterfaceSelection(padapter) == INTF_SEL3_USB_Solo ||
	    RT_GetInterfaceSelection(padapter) == INTF_SEL4_USB_Combo) {
		LedCfg = rtw_read8(padapter, REG_LEDCFG2);

		/* 2009/10/23 MH Issau eed to move the LED GPIO from bit  0 to bit3. */
		/* 2009/10/26 MH Issau if tyhe device is 8c DID is 0x8176, we need to enable bit6 to */
		/* enable GPIO8 for controlling LED.	 */
		/* 2010/07/02 Supprt Open-drain arrangement for controlling the LED. Added by Roger. */
		/*  */
		switch (pLed->LedPin) {

		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:
			if (pHalData->bLedOpenDrain == _TRUE) {
				LedCfg &= 0x90; /* Set to software control.				 */
				rtw_write8(padapter, REG_LEDCFG2, (LedCfg | BIT3));
				LedCfg = rtw_read8(padapter, REG_MAC_PINMUX_CFG);
				LedCfg &= 0xFE;
				rtw_write8(padapter, REG_MAC_PINMUX_CFG, LedCfg);
			} else
				rtw_write8(padapter, REG_LEDCFG2, (LedCfg | BIT3 | BIT5 | BIT6));
			break;

		case LED_PIN_LED1:
			LedCfg &= 0x0f; /* Set to software control. */
			rtw_write8(padapter, REG_LEDCFG2, (LedCfg | BIT3));
			break;

		default:
			break;
		}
	} else {
		switch (pLed->LedPin) {
		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:
			if (pHalData->AntDivCfg == 0) {
				LedCfg = rtw_read8(padapter, REG_LEDCFG0);
				LedCfg &= 0x70; /* Set to software control. */
				rtw_write8(padapter, REG_LEDCFG0, (LedCfg | BIT3 | BIT5));
			} else {
				LedCfg = rtw_read8(padapter, REG_LEDCFG2);
				LedCfg &= 0xe0; /* Set to software control.			 */
				rtw_write8(padapter, REG_LEDCFG2, (LedCfg | BIT3 | BIT7 | BIT6 | BIT5));
			}
			break;

		case LED_PIN_LED1:
			LedCfg = rtw_read8(padapter, REG_LEDCFG1);
			LedCfg &= 0x70; /* Set to software control. */
			rtw_write8(padapter, REG_LEDCFG1, (LedCfg | BIT3 | BIT5));
			break;

		case LED_PIN_LED2:
			LedCfg = rtw_read8(padapter, REG_LEDCFG2);
			LedCfg &= 0x70; /* Set to software control. */
			rtw_write8(padapter, REG_LEDCFG2, (LedCfg | BIT3 | BIT5));
			break;

		default:
			break;
		}
	}

	pLed->bLedOn = _FALSE;
}

/*
 *	Description:
 *		Turn on LED according to LedPin specified.
 *   */
static void
SwLedOn_8821AU(
	PADAPTER		Adapter,
	PLED_USB		pLed
)
{
	u8	LedCfg;
	u32	gpio8_cfg;

	if (RTW_CANNOT_RUN(Adapter))
		return;


	if (RT_GetInterfaceSelection(Adapter) == INTF_SEL2_MINICARD ||
	    RT_GetInterfaceSelection(Adapter) == INTF_SEL3_USB_Solo ||
	    RT_GetInterfaceSelection(Adapter) == INTF_SEL4_USB_Combo) {
		LedCfg = rtw_read8(Adapter, REG_LEDCFG2);
		switch (pLed->LedPin) {
		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:

			LedCfg = rtw_read8(Adapter, REG_LEDCFG2);
			rtw_write8(Adapter, REG_LEDCFG2, (LedCfg & 0xf0) | BIT5 | BIT6); /* SW control led0 on. */
			break;

		case LED_PIN_LED1:
			rtw_write8(Adapter, REG_LEDCFG2, (LedCfg & 0x0f) | BIT5); /* SW control led1 on. */
			break;

		default:
			break;

		}
	} else {
		switch (pLed->LedPin) {
		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:
		case LED_PIN_LED1:
		case LED_PIN_LED2:
			if (IS_HARDWARE_TYPE_8821U(Adapter)) {
				LedCfg = rtw_read8(Adapter, REG_LEDCFG2) & 0xc0; /* Keep 0x4c [22:23] */
				gpio8_cfg = rtw_read32(Adapter, REG_GPIO_PIN_CTRL_2);
				/*0x4c[21] = 0, 0x60[24] = 0, 0x60[16] = 1, 0x60[8] = 0 , use 0x60[8]  control LED , suggert by SD1 Jong*/
				rtw_write8(Adapter, REG_LEDCFG2, LedCfg);
				rtw_write32(Adapter, REG_GPIO_PIN_CTRL_2 , ((gpio8_cfg | BIT16) & (~BIT8) & (~BIT24)));
				/* RTW_INFO("8821 SwLedOn LED2 0x%x\n", rtw_read32(Adapter, REG_LEDCFG0)); */
			}

			break;

		default:
			break;
		}
	}
	pLed->bLedOn = _TRUE;
}


/*
 *	Description:
 *		Turn off LED according to LedPin specified.
 *   */
static void
SwLedOff_8821AU(
	PADAPTER		Adapter,
	PLED_USB		pLed
)
{
	HAL_DATA_TYPE *pHalData = GET_HAL_DATA(Adapter);
	u8	LedCfg;
	u32	gpio8_cfg;

	if (RTW_CANNOT_RUN(Adapter))
		return;

	if (RT_GetInterfaceSelection(Adapter) == INTF_SEL2_MINICARD ||
	    RT_GetInterfaceSelection(Adapter) == INTF_SEL3_USB_Solo ||
	    RT_GetInterfaceSelection(Adapter) == INTF_SEL4_USB_Combo) {
		LedCfg = rtw_read8(Adapter, REG_LEDCFG2);

		/* 2009/10/23 MH Issau eed to move the LED GPIO from bit  0 to bit3. */
		/* 2009/10/26 MH Issau if tyhe device is 8c DID is 0x8176, we need to enable bit6 to */
		/* enable GPIO8 for controlling LED.	 */
		/* 2010/07/02 Supprt Open-drain arrangement for controlling the LED. Added by Roger. */
		/*  */
		switch (pLed->LedPin) {

		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:
			if (pHalData->bLedOpenDrain == _TRUE) {
				LedCfg &= 0x90; /* Set to software control.				 */
				rtw_write8(Adapter, REG_LEDCFG2, (LedCfg | BIT3));
				LedCfg = rtw_read8(Adapter, REG_MAC_PINMUX_CFG);
				LedCfg &= 0xFE;
				rtw_write8(Adapter, REG_MAC_PINMUX_CFG, LedCfg);
			} else
				rtw_write8(Adapter, REG_LEDCFG2, (LedCfg | BIT3 | BIT5 | BIT6));
			break;

		case LED_PIN_LED1:
			LedCfg &= 0x0f; /* Set to software control. */
			rtw_write8(Adapter, REG_LEDCFG2, (LedCfg | BIT3));
			break;

		default:
			break;
		}
	} else {
		switch (pLed->LedPin) {
		case LED_PIN_GPIO0:
			break;

		case LED_PIN_LED0:
		case LED_PIN_LED1:
		case LED_PIN_LED2:
			if (IS_HARDWARE_TYPE_8821U(Adapter)) {
				/*0x4c[21] = 0, 0x60[24] = 0, 0x60[16] = 1, 0x60[8] = 1 , use 0x60[8]  control LED ,  suggert by SD1 Jong*/
				gpio8_cfg = rtw_read32(Adapter, REG_GPIO_PIN_CTRL_2);
				rtw_write32(Adapter, REG_GPIO_PIN_CTRL_2 , ((gpio8_cfg | BIT16 | BIT8) & (~BIT24)));
				/* RTW_INFO("8821 SwLedOn LED2 0x%x\n", rtw_read32(Adapter, REG_LEDCFG0)); */
			}

			break;


		default:
			break;
		}
	}

	pLed->bLedOn = _FALSE;
}


/* ********************************************************************************
 * Interface to manipulate LED objects.
 * ******************************************************************************** */


/* ********************************************************************************
 * Default LED behavior.
 * ******************************************************************************** */

/*
 *	Description:
 *		Initialize all LED_871x objects.
 *   */
void
rtl8812au_InitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv *pledpriv = adapter_to_led(padapter);

	pledpriv->LedControlHandler = LedControlUSB;

	if (IS_HARDWARE_TYPE_8812(padapter)) {
		pledpriv->SwLedOn = SwLedOn_8812AU;
		pledpriv->SwLedOff = SwLedOff_8812AU;
	} else {
		pledpriv->SwLedOn = SwLedOn_8821AU;
		pledpriv->SwLedOff = SwLedOff_8821AU;
	}

	InitLed(padapter, &(pledpriv->SwLed0), LED_PIN_LED0);

	InitLed(padapter, &(pledpriv->SwLed1), LED_PIN_LED1);

	InitLed(padapter, &(pledpriv->SwLed2), LED_PIN_LED2);
}


/*
 *	Description:
 *		DeInitialize all LED_819xUsb objects.
 *   */
void
rtl8812au_DeInitSwLeds(
	_adapter	*padapter
)
{
	struct led_priv	*ledpriv = adapter_to_led(padapter);

	DeInitLed(&(ledpriv->SwLed0));
	DeInitLed(&(ledpriv->SwLed1));
	DeInitLed(&(ledpriv->SwLed2));
}
#endif
