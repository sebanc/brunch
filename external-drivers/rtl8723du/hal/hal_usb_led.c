// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#include <drv_types.h>
#include <hal_data.h>

/*
 *	Description:
 *		Implementation of LED blinking behavior.
 *		It toggle off LED and schedule corresponding timer if necessary.
 *   */
static void
SwLedBlink(
	struct led_usb *			pLed
)
{
	struct adapter			*adapt = pLed->adapt;
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	u8				bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(adapt, pLed);
	} else {
		SwLedOff(adapt, pLed);
	}

	/* Determine if we shall change LED state again. */
	pLed->BlinkTimes--;
	switch (pLed->CurrLedState) {

	case LED_BLINK_NORMAL:
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		break;

	case LED_BLINK_StartToBlink:
		if (check_fwstate(pmlmepriv, _FW_LINKED) && check_fwstate(pmlmepriv, WIFI_STATION_STATE))
			bStopBlinking = true;
		if (check_fwstate(pmlmepriv, _FW_LINKED) &&
		    (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) || check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)))
			bStopBlinking = true;
		else if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		break;

	case LED_BLINK_WPS:
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		break;


	default:
		bStopBlinking = true;
		break;

	}

	if (bStopBlinking) {
		if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
			SwLedOff(adapt, pLed);
		else if ((check_fwstate(pmlmepriv, _FW_LINKED)) && (!pLed->bLedOn))
			SwLedOn(adapt, pLed);
		else if ((!check_fwstate(pmlmepriv, _FW_LINKED)) &&  pLed->bLedOn)
			SwLedOff(adapt, pLed);

		pLed->BlinkTimes = 0;
		pLed->bLedBlinkInProgress = false;
	} else {
		/* Assign LED state to toggle. */
		if (pLed->BlinkingLedState == RTW_LED_ON)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;

		/* Schedule a timer to toggle LED state. */
		switch (pLed->CurrLedState) {
		case LED_BLINK_NORMAL:
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			break;

		case LED_BLINK_SLOWLY:
		case LED_BLINK_StartToBlink:
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			break;

		case LED_BLINK_WPS: {
			if (pLed->BlinkingLedState == RTW_LED_ON)
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LONG_INTERVAL);
			else
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LONG_INTERVAL);
		}
		break;

		default:
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			break;
		}
	}
}

static void
SwLedBlink1(
	struct led_usb *			pLed
)
{
	struct adapter				*adapt = pLed->adapt;
	struct hal_com_data *		pHalData = GET_HAL_DATA(adapt);
	struct led_priv		*ledpriv = &(adapt->ledpriv);
	struct mlme_priv		*pmlmepriv = &(adapt->mlmepriv);
	struct led_usb *			pLed1 = &(ledpriv->SwLed1);
	u8					bStopBlinking = false;

	u32 uLedBlinkNoLinkInterval = LED_BLINK_NO_LINK_INTERVAL_ALPHA; /* add by ylb 20121012 for customer led for alpha */
	if (pHalData->CustomerID == RT_CID_819x_ALPHA_Dlink)
		uLedBlinkNoLinkInterval = LED_BLINK_NO_LINK_INTERVAL_ALPHA_500MS;

	if (pHalData->CustomerID == RT_CID_819x_CAMEO)
		pLed = &(ledpriv->SwLed1);

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(adapt, pLed);
	} else {
		SwLedOff(adapt, pLed);
	}


	if (pHalData->CustomerID == RT_CID_DEFAULT) {
		if (check_fwstate(pmlmepriv, _FW_LINKED)) {
			if (!pLed1->bSWLedCtrl) {
				SwLedOn(adapt, pLed1);
				pLed1->bSWLedCtrl = true;
			} else if (!pLed1->bLedOn)
				SwLedOn(adapt, pLed1);
		} else {
			if (!pLed1->bSWLedCtrl) {
				SwLedOff(adapt, pLed1);
				pLed1->bSWLedCtrl = true;
			} else if (pLed1->bLedOn)
				SwLedOff(adapt, pLed1);
		}
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_SLOWLY:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), uLedBlinkNoLinkInterval);/* change by ylb 20121012 for customer led for alpha */
		break;

	case LED_BLINK_NORMAL:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
		break;

	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->bLedLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_NORMAL;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);

			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), uLedBlinkNoLinkInterval);
			}
			pLed->bLedScanBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->bLedLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_NORMAL;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), uLedBlinkNoLinkInterval);
			}
			pLed->BlinkTimes = 0;
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_WPS:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		break;

	case LED_BLINK_WPS_STOP:	/* WPS success */
		if (pLed->BlinkingLedState == RTW_LED_ON) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_WPS_SUCESS_INTERVAL_ALPHA);
			bStopBlinking = false;
		} else
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				pLed->bLedLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_NORMAL;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
			}
			pLed->bLedWPSBlinkInProgress = false;
		}
		break;

	default:
		break;
	}

}

static void
SwLedBlink2(
	struct led_usb *			pLed
)
{
	struct adapter				*adapt = pLed->adapt;
	struct mlme_priv		*pmlmepriv = &(adapt->mlmepriv);
	u8					bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(adapt, pLed);
	} else {
		SwLedOff(adapt, pLed);
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				SwLedOn(adapt, pLed);

			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				SwLedOff(adapt, pLed);
			}
			pLed->bLedScanBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				SwLedOn(adapt, pLed);

			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				SwLedOff(adapt, pLed);
			}
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	default:
		break;
	}

}

static void
SwLedBlink3(
	struct led_usb *			pLed
)
{
	struct adapter			*adapt = pLed->adapt;
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	u8				bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(adapt, pLed);
	} else {
		if (pLed->CurrLedState != LED_BLINK_WPS_STOP)
			SwLedOff(adapt, pLed);
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				if (!pLed->bLedOn)
					SwLedOn(adapt, pLed);

			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				if (pLed->bLedOn)
					SwLedOff(adapt, pLed);

			}
			pLed->bLedScanBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;

				if (!pLed->bLedOn)
					SwLedOn(adapt, pLed);

			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;

				if (pLed->bLedOn)
					SwLedOff(adapt, pLed);


			}
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_WPS:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		break;

	case LED_BLINK_WPS_STOP:	/* WPS success */
		if (pLed->BlinkingLedState == RTW_LED_ON) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_WPS_SUCESS_INTERVAL_ALPHA);
			bStopBlinking = false;
		} else
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on)
				SwLedOff(adapt, pLed);
			else {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				SwLedOn(adapt, pLed);
			}
			pLed->bLedWPSBlinkInProgress = false;
		}
		break;


	default:
		break;
	}

}


static void
SwLedBlink4(
	struct led_usb *			pLed
)
{
	struct adapter			*adapt = pLed->adapt;
	struct led_priv	*ledpriv = &(adapt->ledpriv);
	struct led_usb *		pLed1 = &(ledpriv->SwLed1);
	u8				bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(adapt, pLed);
	} else {
		SwLedOff(adapt, pLed);
	}

	if (!pLed1->bLedWPSBlinkInProgress && pLed1->BlinkingLedState == LED_UNKNOWN) {
		pLed1->BlinkingLedState = RTW_LED_OFF;
		pLed1->CurrLedState = RTW_LED_OFF;
		SwLedOff(adapt, pLed1);
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_SLOWLY:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		break;

	case LED_BLINK_StartToBlink:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = false;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(adapt, pLed);
			else {
				pLed->bLedNoLinkBlinkInProgress = false;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}
			pLed->bLedScanBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(adapt, pLed);
			else {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(adapt, pLed);
			else {

				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_WPS:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_BLINK_WPS_STOP:	/* WPS authentication fail */
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;

		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		break;

	case LED_BLINK_WPS_STOP_OVERLAP:	/* WPS session overlap */
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0) {
			if (pLed->bLedOn)
				pLed->BlinkTimes = 1;
			else
				bStopBlinking = true;
		}

		if (bStopBlinking) {
			pLed->BlinkTimes = 10;
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
		} else {
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;

			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_BLINK_ALWAYS_ON:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(adapt, pLed);
			else {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS) {
				SwLedOff(adapt, pLed);
			} else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	default:
		break;
	}



}

static void
SwLedBlink5(
	struct led_usb *			pLed
)
{
	struct adapter			*adapt = pLed->adapt;
	u8				bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(adapt, pLed);
	} else {
		SwLedOff(adapt, pLed);
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				if (pLed->bLedOn)
					SwLedOff(adapt, pLed);
			} else {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				if (!pLed->bLedOn)
					_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}

			pLed->bLedScanBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
			}
		}
		break;


	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				if (pLed->bLedOn)
					SwLedOff(adapt, pLed);
			} else {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				if (!pLed->bLedOn)
					_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}

			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(adapt, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	default:
		break;
	}



}

static void
SwLedBlink6(
	struct led_usb *			pLed
)
{
	struct adapter			*adapt = pLed->adapt;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(adapt, pLed);
	} else {
		SwLedOff(adapt, pLed);
	}

}

static void
SwLedBlink7(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);
	bool bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		if (pLed->CurrLedState != LED_BLINK_WPS_STOP)
			SwLedOff(Adapter, pLed);
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on)
				SwLedOff(Adapter, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				if (!pLed->bLedOn)
					SwLedOn(Adapter, pLed);

			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				if (pLed->bLedOn)
					SwLedOff(Adapter, pLed);

			}
			pLed->bLedScanBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on)
				SwLedOff(Adapter, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
			}
		}
		break;

	case LED_BLINK_WPS:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
		break;

	case LED_BLINK_WPS_STOP:	/* WPS success */
		if (pLed->BlinkingLedState == RTW_LED_ON) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
			bStopBlinking = false;
		} else
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on)
				SwLedOff(Adapter, pLed);
			else {
				pLed->CurrLedState = RTW_LED_ON;
				pLed->BlinkingLedState = RTW_LED_ON;
				SwLedOn(Adapter, pLed);
			}
			pLed->bLedWPSBlinkInProgress = false;
		}
		break;


	default:
		break;
	}


}

static void
SwLedBlink8(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		SwLedOff(Adapter, pLed);
	}


}

/* page added for Belkin AC950. 20120813 */
static void
SwLedBlink9(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);
	bool bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		SwLedOff(Adapter, pLed);
	}
	switch (pLed->CurrLedState) {
	case RTW_LED_ON:
		SwLedOn(Adapter, pLed);
		break;
	case RTW_LED_OFF:
		SwLedOff(Adapter, pLed);
		break;
	case LED_BLINK_SLOWLY:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		break;

	case LED_BLINK_StartToBlink:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on)
				SwLedOff(Adapter, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->bLedLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
			} else if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}
			pLed->BlinkTimes = 0;
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_WPS:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_BLINK_WPS_STOP:	/* WPS authentication fail */
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;

		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		break;

	case LED_BLINK_WPS_STOP_OVERLAP:	/* WPS session overlap		 */
		pLed->BlinkTimes--;
		pLed->BlinkCounter--;
		if (pLed->BlinkCounter == 0) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			pLed->CurrLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		} else {
			if (pLed->BlinkTimes == 0) {
				if (pLed->bLedOn)
					pLed->BlinkTimes = 1;
				else
					bStopBlinking = true;
			}

			if (bStopBlinking) {
				pLed->BlinkTimes = 10;
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
			} else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			}
		}
		break;

	case LED_BLINK_ALWAYS_ON:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on &&
			    adapter_to_pwrctl(Adapter)->rfoff_reason >
			    RF_CHANGE_BY_PS) {
				SwLedOff(Adapter, pLed);
			} else {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on &&
			    adapter_to_pwrctl(Adapter)->rfoff_reason >
			    RF_CHANGE_BY_PS) {
				SwLedOff(Adapter, pLed);
			} else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;
	case LED_BLINK_LINK_IN_PROCESS:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ON_BELKIN);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_OFF_BELKIN);
		}
		break;

	case LED_BLINK_AUTH_ERROR:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (!bStopBlinking) {
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_ERROR_INTERVAL_BELKIN);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_ERROR_INTERVAL_BELKIN);
			}
		} else {
			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_ERROR_INTERVAL_BELKIN);
		}
		break;

	default:
		break;
	}

}

/* page added for Netgear A6200V2. 20120827 */
static void
SwLedBlink10(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);
	bool bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		SwLedOff(Adapter, pLed);
	}


	switch (pLed->CurrLedState) {
	case RTW_LED_ON:
		SwLedOn(Adapter, pLed);
		break;

	case RTW_LED_OFF:
		SwLedOff(Adapter, pLed);
		break;

	case LED_BLINK_SLOWLY:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		break;

	case LED_BLINK_StartToBlink:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_BLINK_SCAN:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on)
				SwLedOff(Adapter, pLed);
			else if (check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->bLedNoLinkBlinkInProgress = false;
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}
			pLed->BlinkTimes = 0;
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				if (pLed->bLedOn) {
					pLed->BlinkingLedState = RTW_LED_OFF;
					_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
				} else {
					pLed->BlinkingLedState = RTW_LED_ON;
					_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_SLOWLY_INTERVAL_NETGEAR + LED_BLINK_LINK_INTERVAL_NETGEAR);
				}
			}
		}
		break;

	case LED_BLINK_WPS:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL + LED_BLINK_LINK_INTERVAL_NETGEAR);
		}
		break;

	case LED_BLINK_WPS_STOP:	/* WPS authentication fail */
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;

		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		break;

	case LED_BLINK_WPS_STOP_OVERLAP:	/* WPS session overlap */
		pLed->BlinkTimes--;
		pLed->BlinkCounter--;
		if (pLed->BlinkCounter == 0) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			pLed->CurrLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		} else {
			if (pLed->BlinkTimes == 0) {
				if (pLed->bLedOn)
					pLed->BlinkTimes = 1;
				else
					bStopBlinking = true;
			}

			if (bStopBlinking) {
				pLed->BlinkTimes = 10;
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
			} else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;

				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			}
		}
		break;

	case LED_BLINK_ALWAYS_ON:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS) {
				SwLedOff(Adapter, pLed);
			} else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	case LED_BLINK_LINK_IN_PROCESS:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ON_BELKIN);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_OFF_BELKIN);
		}
		break;

	case LED_BLINK_AUTH_ERROR:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (!bStopBlinking) {
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_ERROR_INTERVAL_BELKIN);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_ERROR_INTERVAL_BELKIN);
			}
		} else {
			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_ERROR_INTERVAL_BELKIN);
		}
		break;

	default:
		break;
	}


}

static void
SwLedBlink11(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);
	bool bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		SwLedOff(Adapter, pLed);
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_TXRX:
		if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
			SwLedOff(Adapter, pLed);
		else {
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		}

		break;

	case LED_BLINK_WPS:
		if (pLed->BlinkTimes == 5) {
			SwLedOn(Adapter, pLed);
			_set_timer(&(pLed->BlinkTimer), LED_CM11_LINK_ON_INTERVEL);
		} else {
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_CM11_BLINK_INTERVAL);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_CM11_BLINK_INTERVAL);
			}
		}
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking)
			pLed->BlinkTimes = 5;
		break;

	case LED_BLINK_WPS_STOP:	/* WPS authentication fail */
		if (check_fwstate(pmlmepriv, _FW_LINKED)) {
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		} else {
			pLed->CurrLedState = RTW_LED_ON;
			pLed->BlinkingLedState = RTW_LED_ON;
			SwLedOn(Adapter, pLed);
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	default:
		break;
	}

}

static void
SwLedBlink12(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	bool bStopBlinking = false;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		SwLedOff(Adapter, pLed);
	}

	switch (pLed->CurrLedState) {
	case LED_BLINK_SLOWLY:
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		break;

	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;

		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS) {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				if (pLed->bLedOn)
					SwLedOff(Adapter, pLed);
			} else {
				pLed->bLedNoLinkBlinkInProgress = true;
				pLed->CurrLedState = LED_BLINK_SLOWLY;
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
			}

			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	default:
		break;
	}



}

static void
SwLedBlink13(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	static u8	LinkBlinkCnt = 0;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		if (pLed->CurrLedState != LED_BLINK_WPS_STOP)
			SwLedOff(Adapter, pLed);
	}
	switch (pLed->CurrLedState) {
	case LED_BLINK_LINK_IN_PROCESS:
		if (!pLed->bLedWPSBlinkInProgress)
			LinkBlinkCnt++;

		if (LinkBlinkCnt > 15) {
			LinkBlinkCnt = 0;
			pLed->bLedBlinkInProgress = false;
			break;
		}
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 500);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 500);
		}

		break;

	case LED_BLINK_WPS:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_ON_INTERVAL_NETGEAR);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_OFF_INTERVAL_NETGEAR);
		}

		break;

	case LED_BLINK_WPS_STOP:	/* WPS success */
		SwLedOff(Adapter, pLed);
		pLed->bLedWPSBlinkInProgress = false;
		break;

	default:
		LinkBlinkCnt = 0;
		break;
	}


}

static void
SwLedBlink14(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	bool bStopBlinking = false;
	static u8	LinkBlinkCnt = 0;

	/* Change LED according to BlinkingLedState specified. */
	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		if (pLed->CurrLedState != LED_BLINK_WPS_STOP)
			SwLedOff(Adapter, pLed);
	}
	switch (pLed->CurrLedState) {
	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else
				SwLedOn(Adapter, pLed);
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}

		break;

	default:
		LinkBlinkCnt = 0;
		break;
	}

}

static void
SwLedBlink15(
	struct led_usb *			pLed
)
{
	struct adapter * Adapter = pLed->adapt;
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);
	bool bStopBlinking = false;
	static u8	LinkBlinkCnt = 0;
	/* Change LED according to BlinkingLedState specified. */

	if (pLed->BlinkingLedState == RTW_LED_ON) {
		SwLedOn(Adapter, pLed);
	} else {
		if (pLed->CurrLedState != LED_BLINK_WPS_STOP)
			SwLedOff(Adapter, pLed);
	}
	switch (pLed->CurrLedState) {
	case LED_BLINK_WPS:
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_ON_INTERVAL_DLINK);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_OFF_INTERVAL_DLINK);
		}
		break;

	case LED_BLINK_WPS_STOP:	/* WPS success */

		if (pLed->BlinkingLedState == RTW_LED_OFF) {
			pLed->bLedWPSBlinkInProgress = false;
			return;
		}

		pLed->CurrLedState = LED_BLINK_WPS_STOP;
		pLed->BlinkingLedState = RTW_LED_OFF;

		_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_LINKED_ON_INTERVAL_DLINK);
		break;

	case LED_BLINK_NO_LINK: {
		static bool		bLedOn = true;
		if (bLedOn) {
			bLedOn = false;
			pLed->BlinkingLedState = RTW_LED_OFF;
		} else {
			bLedOn = true;
			pLed->BlinkingLedState = RTW_LED_ON;
		}
		pLed->bLedBlinkInProgress = true;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL);
	}
	break;

	case LED_BLINK_LINK_IDEL: {
		static bool		bLedOn = true;
		if (bLedOn) {
			bLedOn = false;
			pLed->BlinkingLedState = RTW_LED_OFF;
		} else {
			bLedOn = true;
			pLed->BlinkingLedState = RTW_LED_ON;

		}
		pLed->bLedBlinkInProgress = true;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_IDEL_INTERVAL);
	}
	break;

	case LED_BLINK_SCAN: {
		static u8	BlinkTime = 0;
		if (BlinkTime % 2 == 0)
			pLed->BlinkingLedState = RTW_LED_ON;
		else
			pLed->BlinkingLedState = RTW_LED_OFF;
		BlinkTime++;

		if (BlinkTime < 24) {
			pLed->bLedBlinkInProgress = true;

			if (pLed->BlinkingLedState == RTW_LED_ON)
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_OFF_INTERVAL);
			else
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_ON_INTERVAL);
		} else {
			/* if(pLed->OLDLedState ==LED_NO_LINK_BLINK) */
			if (!check_fwstate(pmlmepriv, _FW_LINKED)) {
				pLed->CurrLedState = LED_BLINK_NO_LINK;
				pLed->BlinkingLedState = RTW_LED_ON;

				_set_timer(&(pLed->BlinkTimer), 100);
			}
			BlinkTime = 0;
		}
	}
	break;

	case LED_BLINK_TXRX:
		pLed->BlinkTimes--;
		if (pLed->BlinkTimes == 0)
			bStopBlinking = true;
		if (bStopBlinking) {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else
				SwLedOn(Adapter, pLed);
			pLed->bLedBlinkInProgress = false;
		} else {
			if (adapter_to_pwrctl(Adapter)->rf_pwrstate != rf_on && adapter_to_pwrctl(Adapter)->rfoff_reason > RF_CHANGE_BY_PS)
				SwLedOff(Adapter, pLed);
			else {
				if (pLed->bLedOn)
					pLed->BlinkingLedState = RTW_LED_OFF;
				else
					pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	default:
		LinkBlinkCnt = 0;
		break;
	}

}

/*
 *	Description:
 *		Handler function of LED Blinking.
 *		We dispatch acture LED blink action according to LedStrategy.
 *   */
void BlinkHandler(struct led_usb * pLed)
{
	struct adapter		*adapt = pLed->adapt;
	struct led_priv	*ledpriv = &(adapt->ledpriv);

	/* RTW_INFO("%s (%s:%d)\n",__func__, current->comm, current->pid); */

	if (RTW_CANNOT_RUN(adapt) || (!rtw_is_hw_init_completed(adapt))) {
		/*RTW_INFO("%s bDriverStopped:%s, bSurpriseRemoved:%s\n"
		, __func__
		, rtw_is_drv_stopped(adapt)?"True":"False"
		, rtw_is_surprise_removed(adapt)?"True":"False" );*/
		return;
	}

	switch (ledpriv->LedStrategy) {
	case SW_LED_MODE0:
		SwLedBlink(pLed);
		break;

	case SW_LED_MODE1:
		SwLedBlink1(pLed);
		break;

	case SW_LED_MODE2:
		SwLedBlink2(pLed);
		break;

	case SW_LED_MODE3:
		SwLedBlink3(pLed);
		break;

	case SW_LED_MODE4:
		SwLedBlink4(pLed);
		break;

	case SW_LED_MODE5:
		SwLedBlink5(pLed);
		break;

	case SW_LED_MODE6:
		SwLedBlink6(pLed);
		break;

	case SW_LED_MODE7:
		SwLedBlink7(pLed);
		break;

	case SW_LED_MODE8:
		SwLedBlink8(pLed);
		break;

	case SW_LED_MODE9:
		SwLedBlink9(pLed);
		break;

	case SW_LED_MODE10:
		SwLedBlink10(pLed);
		break;

	case SW_LED_MODE11:
		SwLedBlink11(pLed);
		break;

	case SW_LED_MODE12:
		SwLedBlink12(pLed);
		break;

	case SW_LED_MODE13:
		SwLedBlink13(pLed);
		break;

	case SW_LED_MODE14:
		SwLedBlink14(pLed);
		break;

	case SW_LED_MODE15:
		SwLedBlink15(pLed);
		break;

	default:
		/* SwLedBlink(pLed); */
		break;
	}
}

/*
 *	Description:
 *		Callback function of LED BlinkTimer,
 *		it just schedules to corresponding BlinkWorkItem/led_blink_hdl
 *   */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void BlinkTimerCallback(struct timer_list *t)
#else
void BlinkTimerCallback(void *data)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	struct led_usb *	 pLed = from_timer(pLed, t, BlinkTimer);
#else
	struct led_usb *	 pLed = (struct led_usb *)data;
#endif
	struct adapter		*adapt = pLed->adapt;

	/* RTW_INFO("%s\n", __func__); */

	if (RTW_CANNOT_RUN(adapt) || (!rtw_is_hw_init_completed(adapt))) {
		/*RTW_INFO("%s bDriverStopped:%s, bSurpriseRemoved:%s\n"
			, __func__
			, rtw_is_drv_stopped(adapt)?"True":"False"
			, rtw_is_surprise_removed(adapt)?"True":"False" );*/
		return;
	}

	rtw_led_blink_cmd(adapt, (void *)pLed);
}

/*
 *	Description:
 *		Callback function of LED BlinkWorkItem.
 *		We dispatch acture LED blink action according to LedStrategy.
 *   */
void BlinkWorkItemCallback(_workitem *work)
{
	struct led_usb *	 pLed = container_of(work, struct led_usb, BlinkWorkItem);
	BlinkHandler(pLed);
}

static void
SwLedControlMode0(
	struct adapter		*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(adapt->ledpriv);
	struct led_usb *	pLed = &(ledpriv->SwLed1);

	/* Decide led state */
	switch (LedAction) {
	case LED_CTL_TX:
	case LED_CTL_RX:
		if (!pLed->bLedBlinkInProgress) {
			pLed->bLedBlinkInProgress = true;

			pLed->CurrLedState = LED_BLINK_NORMAL;
			pLed->BlinkTimes = 2;

			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
		}
		break;

	case LED_CTL_START_TO_LINK:
		if (!pLed->bLedBlinkInProgress) {
			pLed->bLedBlinkInProgress = true;

			pLed->CurrLedState = LED_BLINK_StartToBlink;
			pLed->BlinkTimes = 24;

			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
		} else
			pLed->CurrLedState = LED_BLINK_StartToBlink;
		break;

	case LED_CTL_LINK:
		pLed->CurrLedState = RTW_LED_ON;
		if (!pLed->bLedBlinkInProgress) {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_NO_LINK:
		pLed->CurrLedState = RTW_LED_OFF;
		if (!pLed->bLedBlinkInProgress) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		SwLedOff(adapt, pLed);
		break;

	case LED_CTL_START_WPS:
		if (!pLed->bLedBlinkInProgress || pLed->CurrLedState == RTW_LED_ON) {
			pLed->bLedBlinkInProgress = true;

			pLed->CurrLedState = LED_BLINK_WPS;
			pLed->BlinkTimes = 20;

			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LONG_INTERVAL);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_LONG_INTERVAL);
			}
		}
		break;

	case LED_CTL_STOP_WPS:
		if (pLed->bLedBlinkInProgress) {
			pLed->CurrLedState = RTW_LED_OFF;
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		break;


	default:
		break;
	}


}

/* ALPHA, added by chiyoko, 20090106 */
static void
SwLedControlMode1(
	struct adapter		*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv		*ledpriv = &(adapt->ledpriv);
	struct led_usb *			pLed = &(ledpriv->SwLed0);
	struct mlme_priv		*pmlmepriv = &(adapt->mlmepriv);
	struct hal_com_data *		pHalData = GET_HAL_DATA(adapt);

	u32 uLedBlinkNoLinkInterval = LED_BLINK_NO_LINK_INTERVAL_ALPHA; /* add by ylb 20121012 for customer led for alpha */
	if (pHalData->CustomerID == RT_CID_819x_ALPHA_Dlink)
		uLedBlinkNoLinkInterval = LED_BLINK_NO_LINK_INTERVAL_ALPHA_500MS;

	if (pHalData->CustomerID == RT_CID_819x_CAMEO)
		pLed = &(ledpriv->SwLed1);

	switch (LedAction) {
	case LED_CTL_POWER_ON:
	case LED_CTL_START_TO_LINK:
	case LED_CTL_NO_LINK:
		if (!pLed->bLedNoLinkBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}

			pLed->bLedNoLinkBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SLOWLY;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), uLedBlinkNoLinkInterval);/* change by ylb 20121012 for customer led for alpha */
		}
		break;

	case LED_CTL_LINK:
		if (!pLed->bLedLinkBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedLinkBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_NORMAL;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_SITE_SURVEY:
		if ((pmlmepriv->LinkDetectInfo.bBusyTraffic) && (check_fwstate(pmlmepriv, _FW_LINKED)))
			;
		else if (!pLed->bLedScanBlinkInProgress) {
			if (IS_LED_WPS_BLINKING(pLed))
				return;

			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 24;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;

			if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on && adapter_to_pwrctl(adapt)->rfoff_reason == RF_CHANGE_BY_IPS)
				_set_timer(&(pLed->BlinkTimer), LED_INITIAL_INTERVAL);
			else
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);

		}
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if (!pLed->bLedBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedLinkBlinkInProgress = false;
			}
			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (!pLed->bLedWPSBlinkInProgress) {
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedScanBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedScanBlinkInProgress = false;
			}
			pLed->bLedWPSBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_WPS;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		}
		break;


	case LED_CTL_STOP_WPS:
		if (pLed->bLedNoLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedNoLinkBlinkInProgress = false;
		}
		if (pLed->bLedLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedLinkBlinkInProgress = false;
		}
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress)
			_cancel_timer_ex(&(pLed->BlinkTimer));
		else
			pLed->bLedWPSBlinkInProgress = true;

		pLed->CurrLedState = LED_BLINK_WPS_STOP;
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_WPS_SUCESS_INTERVAL_ALPHA);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_STOP_WPS_FAIL:
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->bLedNoLinkBlinkInProgress = true;
		pLed->CurrLedState = LED_BLINK_SLOWLY;
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), uLedBlinkNoLinkInterval);/* change by ylb 20121012 for customer led for alpha */
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		if (pLed->bLedNoLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedNoLinkBlinkInProgress = false;
		}
		if (pLed->bLedLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedLinkBlinkInProgress = false;
		}
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}

		SwLedOff(adapt, pLed);
		break;

	default:
		break;

	}

}

/* Arcadyan/Sitecom , added by chiyoko, 20090216 */
static void
SwLedControlMode2(
	struct adapter				*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(adapt->ledpriv);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct led_usb *		pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_SITE_SURVEY:
		if (pmlmepriv->LinkDetectInfo.bBusyTraffic)
			;
		else if (!pLed->bLedScanBlinkInProgress) {
			if (IS_LED_WPS_BLINKING(pLed))
				return;

			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 24;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if ((!pLed->bLedBlinkInProgress) && (check_fwstate(pmlmepriv, _FW_LINKED))) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;

			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_LINK:
		pLed->CurrLedState = RTW_LED_ON;
		pLed->BlinkingLedState = RTW_LED_ON;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}

		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (!pLed->bLedWPSBlinkInProgress) {
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedScanBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedScanBlinkInProgress = false;
			}
			pLed->bLedWPSBlinkInProgress = true;
			pLed->CurrLedState = RTW_LED_ON;
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_STOP_WPS:
		pLed->bLedWPSBlinkInProgress = false;
		if (adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on) {
			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 0);
		} else {
			pLed->CurrLedState = RTW_LED_ON;
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_STOP_WPS_FAIL:
		pLed->bLedWPSBlinkInProgress = false;
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_TO_LINK:
	case LED_CTL_NO_LINK:
		if (!IS_LED_BLINKING(pLed)) {
			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		SwLedOff(adapt, pLed);
		break;

	default:
		break;

	}

}

/* COREGA, added by chiyoko, 20090316 */
static void
SwLedControlMode3(
	struct adapter				*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(adapt->ledpriv);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct led_usb *		pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_SITE_SURVEY:
		if (pmlmepriv->LinkDetectInfo.bBusyTraffic)
			;
		else if (!pLed->bLedScanBlinkInProgress) {
			if (IS_LED_WPS_BLINKING(pLed))
				return;

			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 24;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if ((!pLed->bLedBlinkInProgress) && (check_fwstate(pmlmepriv, _FW_LINKED))) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;

			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_LINK:
		if (IS_LED_WPS_BLINKING(pLed))
			return;

		pLed->CurrLedState = RTW_LED_ON;
		pLed->BlinkingLedState = RTW_LED_ON;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}

		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (!pLed->bLedWPSBlinkInProgress) {
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedScanBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedScanBlinkInProgress = false;
			}
			pLed->bLedWPSBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_WPS;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_STOP_WPS:
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		} else
			pLed->bLedWPSBlinkInProgress = true;

		pLed->CurrLedState = LED_BLINK_WPS_STOP;
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_WPS_SUCESS_INTERVAL_ALPHA);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 0);
		}

		break;

	case LED_CTL_STOP_WPS_FAIL:
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_TO_LINK:
	case LED_CTL_NO_LINK:
		if (!IS_LED_BLINKING(pLed)) {
			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		SwLedOff(adapt, pLed);
		break;

	default:
		break;

	}

}


/* Edimax-Belkin, added by chiyoko, 20090413 */
static void
SwLedControlMode4(
	struct adapter				*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(adapt->ledpriv);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct led_usb *		pLed = &(ledpriv->SwLed0);
	struct led_usb *		pLed1 = &(ledpriv->SwLed1);

	switch (LedAction) {
	case LED_CTL_START_TO_LINK:
		if (pLed1->bLedWPSBlinkInProgress) {
			pLed1->bLedWPSBlinkInProgress = false;
			_cancel_timer_ex(&(pLed1->BlinkTimer));

			pLed1->BlinkingLedState = RTW_LED_OFF;
			pLed1->CurrLedState = RTW_LED_OFF;

			if (pLed1->bLedOn)
				_set_timer(&(pLed->BlinkTimer), 0);
		}

		if (!pLed->bLedStartToLinkBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}

			pLed->bLedStartToLinkBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_StartToBlink;
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			}
		}
		break;

	case LED_CTL_LINK:
	case LED_CTL_NO_LINK:
		/* LED1 settings */
		if (LedAction == LED_CTL_LINK) {
			if (pLed1->bLedWPSBlinkInProgress) {
				pLed1->bLedWPSBlinkInProgress = false;
				_cancel_timer_ex(&(pLed1->BlinkTimer));

				pLed1->BlinkingLedState = RTW_LED_OFF;
				pLed1->CurrLedState = RTW_LED_OFF;

				if (pLed1->bLedOn)
					_set_timer(&(pLed->BlinkTimer), 0);
			}
		}

		if (!pLed->bLedNoLinkBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}

			pLed->bLedNoLinkBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SLOWLY;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;

			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_SITE_SURVEY:
		if ((pmlmepriv->LinkDetectInfo.bBusyTraffic) && (check_fwstate(pmlmepriv, _FW_LINKED)))
			;
		else if (!pLed->bLedScanBlinkInProgress) {
			if (IS_LED_WPS_BLINKING(pLed))
				return;

			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 24;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if (!pLed->bLedBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (pLed1->bLedWPSBlinkInProgress) {
			pLed1->bLedWPSBlinkInProgress = false;
			_cancel_timer_ex(&(pLed1->BlinkTimer));

			pLed1->BlinkingLedState = RTW_LED_OFF;
			pLed1->CurrLedState = RTW_LED_OFF;

			if (pLed1->bLedOn)
				_set_timer(&(pLed->BlinkTimer), 0);
		}

		if (!pLed->bLedWPSBlinkInProgress) {
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedScanBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedScanBlinkInProgress = false;
			}
			pLed->bLedWPSBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_WPS;
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_SLOWLY_INTERVAL);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);
			}
		}
		break;

	case LED_CTL_STOP_WPS:	/* WPS connect success */
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->bLedNoLinkBlinkInProgress = true;
		pLed->CurrLedState = LED_BLINK_SLOWLY;
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);

		break;

	case LED_CTL_STOP_WPS_FAIL:		/* WPS authentication fail */
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->bLedNoLinkBlinkInProgress = true;
		pLed->CurrLedState = LED_BLINK_SLOWLY;
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);

		/* LED1 settings */
		if (pLed1->bLedWPSBlinkInProgress)
			_cancel_timer_ex(&(pLed1->BlinkTimer));
		else
			pLed1->bLedWPSBlinkInProgress = true;

		pLed1->CurrLedState = LED_BLINK_WPS_STOP;
		if (pLed1->bLedOn)
			pLed1->BlinkingLedState = RTW_LED_OFF;
		else
			pLed1->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);

		break;

	case LED_CTL_STOP_WPS_FAIL_OVERLAP:	/* WPS session overlap */
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->bLedNoLinkBlinkInProgress = true;
		pLed->CurrLedState = LED_BLINK_SLOWLY;
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);

		/* LED1 settings */
		if (pLed1->bLedWPSBlinkInProgress)
			_cancel_timer_ex(&(pLed1->BlinkTimer));
		else
			pLed1->bLedWPSBlinkInProgress = true;

		pLed1->CurrLedState = LED_BLINK_WPS_STOP_OVERLAP;
		pLed1->BlinkTimes = 10;
		if (pLed1->bLedOn)
			pLed1->BlinkingLedState = RTW_LED_OFF;
		else
			pLed1->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_NORMAL_INTERVAL);

		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;

		if (pLed->bLedNoLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedNoLinkBlinkInProgress = false;
		}
		if (pLed->bLedLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedLinkBlinkInProgress = false;
		}
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		if (pLed->bLedStartToLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedStartToLinkBlinkInProgress = false;
		}

		if (pLed1->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed1->BlinkTimer));
			pLed1->bLedWPSBlinkInProgress = false;
		}

		pLed1->BlinkingLedState = LED_UNKNOWN;
		SwLedOff(adapt, pLed);
		SwLedOff(adapt, pLed1);
		break;

	case LED_CTL_CONNECTION_NO_TRANSFER:
		if (!pLed->bLedBlinkInProgress) {
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			pLed->bLedBlinkInProgress = true;

			pLed->CurrLedState = LED_BLINK_ALWAYS_ON;
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		}
		break;

	default:
		break;

	}

}



/* Sercomm-Belkin, added by chiyoko, 20090415 */
static void
SwLedControlMode5(
	struct adapter				*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(adapt->ledpriv);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapt);
	struct led_usb *		pLed = &(ledpriv->SwLed0);

	if (pHalData->CustomerID == RT_CID_819x_CAMEO)
		pLed = &(ledpriv->SwLed1);

	switch (LedAction) {
	case LED_CTL_POWER_ON:
	case LED_CTL_NO_LINK:
	case LED_CTL_LINK:	/* solid blue */
		pLed->CurrLedState = RTW_LED_ON;
		pLed->BlinkingLedState = RTW_LED_ON;

		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_SITE_SURVEY:
		if ((pmlmepriv->LinkDetectInfo.bBusyTraffic) && (check_fwstate(pmlmepriv, _FW_LINKED)))
			;
		else if (!pLed->bLedScanBlinkInProgress) {
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 24;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if (!pLed->bLedBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN)
				return;
			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;

		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}

		SwLedOff(adapt, pLed);
		break;

	default:
		break;

	}

}

/* WNC-Corega, added by chiyoko, 20090902 */
static void
SwLedControlMode6(
	struct adapter				*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(adapt->ledpriv);
	struct led_usb *	pLed0 = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_POWER_ON:
	case LED_CTL_LINK:
	case LED_CTL_NO_LINK:
		_cancel_timer_ex(&(pLed0->BlinkTimer));
		pLed0->CurrLedState = RTW_LED_ON;
		pLed0->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed0->BlinkTimer), 0);
		break;

	case LED_CTL_POWER_OFF:
		SwLedOff(adapt, pLed0);
		break;

	default:
		break;
	}

}

/* Netgear, added by sinda, 2011/11/11 */
static void
SwLedControlMode7(
	struct adapter *			 Adapter,
	enum led_ctl_mode		 LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct mlme_priv	*pmlmepriv = &Adapter->mlmepriv;
	struct led_usb *	pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_SITE_SURVEY:
		if (pmlmepriv->LinkDetectInfo.bBusyTraffic)
			;
		else if (!pLed->bLedScanBlinkInProgress) {
			if (IS_LED_WPS_BLINKING(pLed))
				return;

			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 6;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
		}
		break;

	case LED_CTL_LINK:
		if (IS_LED_WPS_BLINKING(pLed))
			return;

		pLed->CurrLedState = RTW_LED_ON;
		pLed->BlinkingLedState = RTW_LED_ON;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}

		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (!pLed->bLedWPSBlinkInProgress) {
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedScanBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedScanBlinkInProgress = false;
			}
			pLed->bLedWPSBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_WPS;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
		}
		break;

	case LED_CTL_STOP_WPS:
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		} else
			pLed->bLedWPSBlinkInProgress = true;

		pLed->CurrLedState = LED_BLINK_WPS_STOP;
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_LINK_INTERVAL_NETGEAR);
		} else {
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 0);
		}

		break;


	case LED_CTL_STOP_WPS_FAIL:
	case LED_CTL_STOP_WPS_FAIL_OVERLAP:	/* WPS session overlap			 */
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_TO_LINK:
	case LED_CTL_NO_LINK:
		if (!IS_LED_BLINKING(pLed)) {
			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_POWER_OFF:
	case LED_CTL_POWER_ON:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	default:
		break;

	}

}

static void
SwLedControlMode8(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct led_usb *	pLed0 = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_LINK:
		_cancel_timer_ex(&(pLed0->BlinkTimer));
		pLed0->CurrLedState = RTW_LED_ON;
		pLed0->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed0->BlinkTimer), 0);
		break;

	case LED_CTL_NO_LINK:
		_cancel_timer_ex(&(pLed0->BlinkTimer));
		pLed0->CurrLedState = RTW_LED_OFF;
		pLed0->BlinkingLedState = RTW_LED_OFF;
		_set_timer(&(pLed0->BlinkTimer), 0);
		break;

	case LED_CTL_POWER_OFF:
		SwLedOff(Adapter, pLed0);
		break;

	default:
		break;
	}


}

/* page added for Belkin AC950, 20120813 */
static void
SwLedControlMode9(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct mlme_priv	*pmlmepriv = &Adapter->mlmepriv;
	struct led_usb *	pLed = &(ledpriv->SwLed0);
	struct led_usb *	pLed1 = &(ledpriv->SwLed1);
	struct led_usb *	pLed2 = &(ledpriv->SwLed2);
	bool  bWPSOverLap = false;
	/* RTW_INFO("LedAction=%d\n", LedAction); */
	switch (LedAction) {
	case LED_CTL_START_TO_LINK:
		if (!pLed2->bLedBlinkInProgress) {
			pLed2->bLedBlinkInProgress = true;
			pLed2->BlinkingLedState = RTW_LED_ON;
			pLed2->CurrLedState = LED_BLINK_LINK_IN_PROCESS;

			_set_timer(&(pLed2->BlinkTimer), 0);
		}
		break;

	case LED_CTL_LINK:
	case LED_CTL_NO_LINK:
		/* LED1 settings */
		if (LedAction == LED_CTL_NO_LINK) {
			pLed1->CurrLedState = RTW_LED_OFF;
			pLed1->BlinkingLedState = RTW_LED_OFF;
			if (pLed1->bLedOn)
				_set_timer(&(pLed1->BlinkTimer), 0);
		} else {
			pLed1->CurrLedState = RTW_LED_OFF;
			pLed1->BlinkingLedState = RTW_LED_OFF;
			if (pLed1->bLedOn)
				_set_timer(&(pLed1->BlinkTimer), 0);
		}

		/* LED2 settings */
		if (LedAction == LED_CTL_LINK) {
			if (Adapter->securitypriv.dot11PrivacyAlgrthm != _NO_PRIVACY_) {
				if (pLed2->bLedBlinkInProgress) {
					_cancel_timer_ex(&(pLed2->BlinkTimer));
					pLed2->bLedBlinkInProgress = false;
				}
				pLed2->CurrLedState = RTW_LED_ON;
				pLed2->bLedNoLinkBlinkInProgress = true;
				if (!pLed2->bLedOn)
					_set_timer(&(pLed2->BlinkTimer), 0);
			} else {
				if (!pLed2->bLedWPSBlinkInProgress) {
					pLed2->CurrLedState = RTW_LED_OFF;
					pLed2->BlinkingLedState = RTW_LED_OFF;
					if (pLed2->bLedOn)
						_set_timer(&(pLed2->BlinkTimer), 0);
				}
			}
		} else { /* NO_LINK */
			if (!pLed2->bLedWPSBlinkInProgress) {
				pLed2->CurrLedState = RTW_LED_OFF;
				pLed2->BlinkingLedState = RTW_LED_OFF;
				if (pLed2->bLedOn)
					_set_timer(&(pLed2->BlinkTimer), 0);
			}
		}

		/* LED0 settings			 */
		if (!pLed->bLedNoLinkBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}

			pLed->bLedNoLinkBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SLOWLY;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		}

		break;

	case LED_CTL_SITE_SURVEY:
		if ((pmlmepriv->LinkDetectInfo.bBusyTraffic) && (check_fwstate(pmlmepriv, _FW_LINKED)))
			;
		else { /* if(pLed->bLedScanBlinkInProgress ==false) */
			if (IS_LED_WPS_BLINKING(pLed))
				return;

			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 24;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);

		}
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if (!pLed->bLedBlinkInProgress) {
			if (pLed->CurrLedState == LED_BLINK_SCAN || IS_LED_WPS_BLINKING(pLed))
				return;
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		pLed2->bLedBlinkInProgress = true;
		pLed2->BlinkingLedState = RTW_LED_ON;
		pLed2->CurrLedState = LED_BLINK_LINK_IN_PROCESS;
		pLed2->bLedWPSBlinkInProgress = true;

		_set_timer(&(pLed2->BlinkTimer), 500);

		break;

	case LED_CTL_STOP_WPS:	/* WPS connect success	 */
		/* LED2 settings */
		if (pLed2->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed2->BlinkTimer));
			pLed2->bLedBlinkInProgress = false;
			pLed2->bLedWPSBlinkInProgress = false;
		}
		pLed2->CurrLedState = RTW_LED_ON;
		pLed2->bLedNoLinkBlinkInProgress = true;
		if (!pLed2->bLedOn)
			_set_timer(&(pLed2->BlinkTimer), 0);

		/* LED1 settings */
		_cancel_timer_ex(&(pLed1->BlinkTimer));
		pLed1->CurrLedState = RTW_LED_OFF;
		pLed1->BlinkingLedState = RTW_LED_OFF;
		if (pLed1->bLedOn)
			_set_timer(&(pLed1->BlinkTimer), 0);


		break;

	case LED_CTL_STOP_WPS_FAIL:		/* WPS authentication fail	 */
		/* LED1 settings */
		pLed1->CurrLedState = LED_BLINK_AUTH_ERROR;
		pLed1->BlinkTimes = 50;
		if (pLed1->bLedOn)
			pLed1->BlinkingLedState = RTW_LED_OFF;
		else
			pLed1->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed1->BlinkTimer), 0);

		/* LED2 settings */
		pLed2->CurrLedState = RTW_LED_OFF;
		pLed2->BlinkingLedState = RTW_LED_OFF;
		pLed2->bLedWPSBlinkInProgress = false;
		if (pLed2->bLedOn)
			_set_timer(&(pLed2->BlinkTimer), 0);
		break;
	case LED_CTL_STOP_WPS_FAIL_OVERLAP:	/* WPS session overlap */
		/* LED1 settings */
		bWPSOverLap = true;
		pLed1->CurrLedState = LED_BLINK_WPS_STOP_OVERLAP;
		pLed1->BlinkTimes = 10;
		pLed1->BlinkCounter = 50;
		if (pLed1->bLedOn)
			pLed1->BlinkingLedState = RTW_LED_OFF;
		else
			pLed1->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed1->BlinkTimer), 0);

		/* LED2 settings */
		pLed2->CurrLedState = RTW_LED_OFF;
		pLed2->BlinkingLedState = RTW_LED_OFF;
		pLed2->bLedWPSBlinkInProgress = false;
		if (pLed2->bLedOn)
			_set_timer(&(pLed2->BlinkTimer), 0);

		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;

		if (pLed->bLedNoLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedNoLinkBlinkInProgress = false;
		}
		if (pLed->bLedLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedLinkBlinkInProgress = false;
		}
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		if (pLed->bLedStartToLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedStartToLinkBlinkInProgress = false;
		}

		if (pLed1->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed1->BlinkTimer));
			pLed1->bLedWPSBlinkInProgress = false;
		}


		pLed1->BlinkingLedState = LED_UNKNOWN;
		SwLedOff(Adapter, pLed);
		SwLedOff(Adapter, pLed1);
		break;

	case LED_CTL_CONNECTION_NO_TRANSFER:
		if (!pLed->bLedBlinkInProgress) {
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			pLed->bLedBlinkInProgress = true;

			pLed->CurrLedState = LED_BLINK_ALWAYS_ON;
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		}
		break;

	default:
		break;

	}

}

/* page added for Netgear A6200V2, 20120827 */
static void
SwLedControlMode10(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct mlme_priv	*pmlmepriv = &Adapter->mlmepriv;
	struct led_usb *	pLed = &(ledpriv->SwLed0);
	struct led_usb *	pLed1 = &(ledpriv->SwLed1);

	switch (LedAction) {
	case LED_CTL_START_TO_LINK:
		if (!pLed1->bLedBlinkInProgress) {
			pLed1->bLedBlinkInProgress = true;
			pLed1->BlinkingLedState = RTW_LED_ON;
			pLed1->CurrLedState = LED_BLINK_LINK_IN_PROCESS;

			_set_timer(&(pLed1->BlinkTimer), 0);
		}
		break;

	case LED_CTL_LINK:
	case LED_CTL_NO_LINK:
		if (LedAction == LED_CTL_LINK) {
			if (pLed->bLedWPSBlinkInProgress || pLed1->bLedWPSBlinkInProgress)
				;
			else {
				if (pHalData->current_band_type == BAND_ON_2_4G)
					/* LED0 settings */
				{
					pLed->CurrLedState = RTW_LED_ON;
					pLed->BlinkingLedState = RTW_LED_ON;
					if (pLed->bLedBlinkInProgress) {
						_cancel_timer_ex(&(pLed->BlinkTimer));
						pLed->bLedBlinkInProgress = false;
					}
					_set_timer(&(pLed->BlinkTimer), 0);

					pLed1->CurrLedState = RTW_LED_OFF;
					pLed1->BlinkingLedState = RTW_LED_OFF;
					_set_timer(&(pLed1->BlinkTimer), 0);
				} else if (pHalData->current_band_type == BAND_ON_5G)
					/* LED1 settings */
				{
					pLed1->CurrLedState = RTW_LED_ON;
					pLed1->BlinkingLedState = RTW_LED_ON;
					if (pLed1->bLedBlinkInProgress) {
						_cancel_timer_ex(&(pLed1->BlinkTimer));
						pLed1->bLedBlinkInProgress = false;
					}
					_set_timer(&(pLed1->BlinkTimer), 0);

					pLed->CurrLedState = RTW_LED_OFF;
					pLed->BlinkingLedState = RTW_LED_OFF;
					_set_timer(&(pLed->BlinkTimer), 0);
				}
			}
		} else if (LedAction == LED_CTL_NO_LINK) { /* TODO by page */
			if (pLed->bLedWPSBlinkInProgress || pLed1->bLedWPSBlinkInProgress)
				;
			else {
				pLed->CurrLedState = RTW_LED_OFF;
				pLed->BlinkingLedState = RTW_LED_OFF;
				if (pLed->bLedOn)
					_set_timer(&(pLed->BlinkTimer), 0);

				pLed1->CurrLedState = RTW_LED_OFF;
				pLed1->BlinkingLedState = RTW_LED_OFF;
				if (pLed1->bLedOn)
					_set_timer(&(pLed1->BlinkTimer), 0);
			}
		}

		break;

	case LED_CTL_SITE_SURVEY:
		if (check_fwstate(pmlmepriv, _FW_LINKED))
			;                                                                  /* don't blink when media connect */
		else { /* if(pLed->bLedScanBlinkInProgress ==false) */
			if (IS_LED_WPS_BLINKING(pLed) || IS_LED_WPS_BLINKING(pLed1))
				return;

			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			pLed->bLedScanBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SCAN;
			pLed->BlinkTimes = 12;
			pLed->BlinkingLedState = LED_BLINK_SCAN;
			_set_timer(&(pLed->BlinkTimer), 0);

			if (pLed1->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed1->BlinkTimer));
				pLed1->bLedNoLinkBlinkInProgress = false;
			}
			if (pLed1->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed1->BlinkTimer));
				pLed1->bLedBlinkInProgress = false;
			}
			pLed1->bLedScanBlinkInProgress = true;
			pLed1->CurrLedState = LED_BLINK_SCAN;
			pLed1->BlinkTimes = 12;
			pLed1->BlinkingLedState = LED_BLINK_SCAN;
			_set_timer(&(pLed1->BlinkTimer), LED_BLINK_LINK_SLOWLY_INTERVAL_NETGEAR);

		}
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		/* LED0 settings */
		if (!pLed->bLedBlinkInProgress) {
			pLed->bLedBlinkInProgress = true;
			pLed->bLedWPSBlinkInProgress = true;
			pLed->BlinkingLedState = LED_BLINK_WPS;
			pLed->CurrLedState = LED_BLINK_WPS;
			_set_timer(&(pLed->BlinkTimer), 0);
		}

		/* LED1 settings */
		if (!pLed1->bLedBlinkInProgress) {
			pLed1->bLedBlinkInProgress = true;
			pLed1->bLedWPSBlinkInProgress = true;
			pLed1->BlinkingLedState = LED_BLINK_WPS;
			pLed1->CurrLedState = LED_BLINK_WPS;
			_set_timer(&(pLed1->BlinkTimer), LED_BLINK_NORMAL_INTERVAL + LED_BLINK_LINK_INTERVAL_NETGEAR);
		}


		break;

	case LED_CTL_STOP_WPS:	/* WPS connect success */
		if (pHalData->current_band_type == BAND_ON_2_4G)
			/* LED0 settings */
		{
			pLed->bLedWPSBlinkInProgress = false;
			pLed->CurrLedState = RTW_LED_ON;
			pLed->BlinkingLedState = RTW_LED_ON;
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			_set_timer(&(pLed->BlinkTimer), 0);

			pLed1->CurrLedState = RTW_LED_OFF;
			pLed1->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed1->BlinkTimer), 0);
		} else if (pHalData->current_band_type == BAND_ON_5G)
			/* LED1 settings */
		{
			pLed1->bLedWPSBlinkInProgress = false;
			pLed1->CurrLedState = RTW_LED_ON;
			pLed1->BlinkingLedState = RTW_LED_ON;
			if (pLed1->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed1->BlinkTimer));
				pLed1->bLedBlinkInProgress = false;
			}
			_set_timer(&(pLed1->BlinkTimer), 0);

			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 0);
		}

		break;

	case LED_CTL_STOP_WPS_FAIL:		/* WPS authentication fail	 */
		/* LED1 settings */
		pLed1->bLedWPSBlinkInProgress = false;
		pLed1->CurrLedState = RTW_LED_OFF;
		pLed1->BlinkingLedState = RTW_LED_OFF;
		_set_timer(&(pLed1->BlinkTimer), 0);

		/* LED0 settings */
		pLed->bLedWPSBlinkInProgress = false;
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		if (pLed->bLedOn)
			_set_timer(&(pLed->BlinkTimer), 0);

		break;


	default:
		break;

	}

}

/* Edimax-ASUS, added by Page, 20121221 */
static void
SwLedControlMode11(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct led_usb *	pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_POWER_ON:
	case LED_CTL_START_TO_LINK:
	case LED_CTL_NO_LINK:
		pLed->CurrLedState = RTW_LED_ON;
		pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_LINK:
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		pLed->bLedBlinkInProgress = true;
		pLed->CurrLedState = LED_BLINK_TXRX;
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_SCAN_INTERVAL_ALPHA);
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		pLed->bLedWPSBlinkInProgress = true;
		pLed->bLedBlinkInProgress = true;
		pLed->CurrLedState = LED_BLINK_WPS;
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		pLed->BlinkTimes = 5;
		_set_timer(&(pLed->BlinkTimer), 0);

		break;


	case LED_CTL_STOP_WPS:
	case LED_CTL_STOP_WPS_FAIL:
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		pLed->CurrLedState = LED_BLINK_WPS_STOP;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;

		if (pLed->bLedNoLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedNoLinkBlinkInProgress = false;
		}
		if (pLed->bLedLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedLinkBlinkInProgress = false;
		}
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}

		SwLedOff(Adapter, pLed);
		break;

	default:
		break;

	}

}

/* page added for NEC */

static void
SwLedControlMode12(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct led_usb *	pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_POWER_ON:
	case LED_CTL_NO_LINK:
	case LED_CTL_LINK:
	case LED_CTL_SITE_SURVEY:

		if (!pLed->bLedNoLinkBlinkInProgress) {
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}

			pLed->bLedNoLinkBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_SLOWLY;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_NO_LINK_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if (!pLed->bLedBlinkInProgress) {
			if (pLed->bLedNoLinkBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedNoLinkBlinkInProgress = false;
			}
			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn)
				pLed->BlinkingLedState = RTW_LED_OFF;
			else
				pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		}
		break;

	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;

		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}

		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}

		if (pLed->bLedNoLinkBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedNoLinkBlinkInProgress = false;
		}

		SwLedOff(Adapter, pLed);
		break;

	default:
		break;

	}

}

/* Maddest add for NETGEAR R6100 */

static void
SwLedControlMode13(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct led_usb *	pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_LINK:
		if (pLed->bLedWPSBlinkInProgress)
			return;


		pLed->CurrLedState = RTW_LED_ON;
		pLed->BlinkingLedState = RTW_LED_ON;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}

		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (!pLed->bLedWPSBlinkInProgress) {
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedScanBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedScanBlinkInProgress = false;
			}
			pLed->bLedWPSBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_WPS;
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_OFF_INTERVAL_NETGEAR);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_ON_INTERVAL_NETGEAR);
			}
		}
		break;

	case LED_CTL_STOP_WPS:
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		} else
			pLed->bLedWPSBlinkInProgress = true;

		pLed->bLedWPSBlinkInProgress = false;
		pLed->CurrLedState = LED_BLINK_WPS_STOP;
		if (pLed->bLedOn) {
			pLed->BlinkingLedState = RTW_LED_OFF;

			_set_timer(&(pLed->BlinkTimer), 0);
		}

		break;


	case LED_CTL_STOP_WPS_FAIL:
	case LED_CTL_STOP_WPS_FAIL_OVERLAP: /* WPS session overlap */
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_START_TO_LINK:
		if ((!pLed->bLedBlinkInProgress) && (!pLed->bLedWPSBlinkInProgress)) {
			pLed->bLedBlinkInProgress = true;
			pLed->BlinkingLedState = RTW_LED_ON;
			pLed->CurrLedState = LED_BLINK_LINK_IN_PROCESS;

			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_NO_LINK:

		if (pLed->bLedWPSBlinkInProgress)
			return;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		/* if(!IS_LED_BLINKING(pLed)) */
		{
			pLed->CurrLedState = RTW_LED_OFF;
			pLed->BlinkingLedState = RTW_LED_OFF;
			_set_timer(&(pLed->BlinkTimer), 0);
		}
		break;

	case LED_CTL_POWER_OFF:
	case LED_CTL_POWER_ON:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		if (LedAction == LED_CTL_POWER_ON)
			_set_timer(&(pLed->BlinkTimer), 0);
		else
			SwLedOff(Adapter, pLed);
		break;

	default:
		break;

	}


}

/* Maddest add for DNI Buffalo */

static void
SwLedControlMode14(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct led_usb *	pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_POWER_OFF:
		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		SwLedOff(Adapter, pLed);
		break;

	case LED_CTL_POWER_ON:
		SwLedOn(Adapter, pLed);
		break;

	case LED_CTL_LINK:
	case LED_CTL_NO_LINK:
		break;
	case LED_CTL_TX:
	case LED_CTL_RX:
		if (!pLed->bLedBlinkInProgress) {
			pLed->bLedBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_TXRX;
			pLed->BlinkTimes = 2;
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
			}
		}
		break;

	default:
		break;
	}
}

/* Maddest add for Dlink */

static void
SwLedControlMode15(
	struct adapter *			Adapter,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(Adapter->ledpriv);
	struct mlme_priv	*pmlmepriv = &Adapter->mlmepriv;
	struct led_usb *	pLed = &(ledpriv->SwLed0);

	switch (LedAction) {
	case LED_CTL_START_WPS: /* wait until xinpin finish */
	case LED_CTL_START_WPS_BOTTON:
		if (!pLed->bLedWPSBlinkInProgress) {
			if (pLed->bLedBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedBlinkInProgress = false;
			}
			if (pLed->bLedScanBlinkInProgress) {
				_cancel_timer_ex(&(pLed->BlinkTimer));
				pLed->bLedScanBlinkInProgress = false;
			}
			pLed->bLedWPSBlinkInProgress = true;
			pLed->CurrLedState = LED_BLINK_WPS;
			if (pLed->bLedOn) {
				pLed->BlinkingLedState = RTW_LED_OFF;
				_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_OFF_INTERVAL_NETGEAR);
			} else {
				pLed->BlinkingLedState = RTW_LED_ON;
				_set_timer(&(pLed->BlinkTimer), LED_WPS_BLINK_ON_INTERVAL_NETGEAR);
			}
		}
		break;

	case LED_CTL_STOP_WPS:
		if (pLed->bLedWPSBlinkInProgress)
			_cancel_timer_ex(&(pLed->BlinkTimer));

		pLed->CurrLedState = LED_BLINK_WPS_STOP;
		pLed->BlinkingLedState = RTW_LED_ON;

		_set_timer(&(pLed->BlinkTimer), 0);

		break;

	case LED_CTL_STOP_WPS_FAIL:
	case LED_CTL_STOP_WPS_FAIL_OVERLAP: /* WPS session overlap */
		if (pLed->bLedWPSBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedWPSBlinkInProgress = false;
		}

		pLed->CurrLedState = RTW_LED_OFF;
		pLed->BlinkingLedState = RTW_LED_OFF;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_NO_LINK:
		if (pLed->bLedWPSBlinkInProgress)
			return;

		/*if(Adapter->securitypriv.dot11PrivacyAlgrthm > _NO_PRIVACY_)
		{
			if(SecIsTxKeyInstalled(Adapter, pMgntInfo->Bssid))
			{
			}
			else
			{
				if(pMgntInfo->LEDAssocState ==LED_ASSOC_SECURITY_BEGIN)
					return;
			}
		}*/

		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		if (pLed->bLedScanBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedScanBlinkInProgress = false;
		}
		/* if(!IS_LED_BLINKING(pLed)) */
		{
			pLed->CurrLedState = LED_BLINK_NO_LINK;
			pLed->BlinkingLedState = RTW_LED_ON;
			_set_timer(&(pLed->BlinkTimer), 30);
		}
		break;

	case LED_CTL_LINK:

		if (pLed->bLedWPSBlinkInProgress)
			return;

		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}

		pLed->CurrLedState = LED_BLINK_LINK_IDEL;
		pLed->BlinkingLedState = RTW_LED_ON;

		_set_timer(&(pLed->BlinkTimer), 30);
		break;

	case LED_CTL_SITE_SURVEY:
		if (check_fwstate(pmlmepriv, _FW_LINKED))
			return;

		if (pLed->bLedWPSBlinkInProgress)
			return;

		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}
		pLed->CurrLedState = LED_BLINK_SCAN;
		pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), 0);
		break;

	case LED_CTL_TX:
	case LED_CTL_RX:
		if (pLed->bLedWPSBlinkInProgress)
			return;

		if (pLed->bLedBlinkInProgress) {
			_cancel_timer_ex(&(pLed->BlinkTimer));
			pLed->bLedBlinkInProgress = false;
		}

		pLed->bLedBlinkInProgress = true;
		pLed->CurrLedState = LED_BLINK_TXRX;
		pLed->BlinkTimes = 2;
		if (pLed->bLedOn)
			pLed->BlinkingLedState = RTW_LED_OFF;
		else
			pLed->BlinkingLedState = RTW_LED_ON;
		_set_timer(&(pLed->BlinkTimer), LED_BLINK_FASTER_INTERVAL_ALPHA);
		break;

	default:
		break;
	}
}

void
LedControlUSB(
	struct adapter				*adapt,
	enum led_ctl_mode		LedAction
)
{
	struct led_priv	*ledpriv = &(adapt->ledpriv);

#if (MP_DRIVER == 1)
	if (adapt->registrypriv.mp_mode == 1)
		return;
#endif

	if (RTW_CANNOT_RUN(adapt) || (!rtw_is_hw_init_completed(adapt))) {
		/*RTW_INFO("%s bDriverStopped:%s, bSurpriseRemoved:%s\n"
		, __func__
		, rtw_is_drv_stopped(adapt)?"True":"False"
		, rtw_is_surprise_removed(adapt)?"True":"False" );*/
		return;
	}

	if (!ledpriv->bRegUseLed)
		return;

	/* if(priv->bInHctTest) */
	/*	return; */

#ifdef CONFIG_CONCURRENT_MODE
	/* Only do led action for PRIMARY_ADAPTER */
	if (adapt->adapter_type != PRIMARY_ADAPTER)
		return;
#endif

	if ((adapter_to_pwrctl(adapt)->rf_pwrstate != rf_on &&
	     adapter_to_pwrctl(adapt)->rfoff_reason > RF_CHANGE_BY_PS) &&
	    (LedAction == LED_CTL_TX || LedAction == LED_CTL_RX ||
	     LedAction == LED_CTL_SITE_SURVEY ||
	     LedAction == LED_CTL_LINK ||
	     LedAction == LED_CTL_NO_LINK ||
	     LedAction == LED_CTL_POWER_ON))
		return;

	switch (ledpriv->LedStrategy) {
	case SW_LED_MODE0:
		SwLedControlMode0(adapt, LedAction);
		break;

	case SW_LED_MODE1:
		SwLedControlMode1(adapt, LedAction);
		break;

	case SW_LED_MODE2:
		SwLedControlMode2(adapt, LedAction);
		break;

	case SW_LED_MODE3:
		SwLedControlMode3(adapt, LedAction);
		break;

	case SW_LED_MODE4:
		SwLedControlMode4(adapt, LedAction);
		break;

	case SW_LED_MODE5:
		SwLedControlMode5(adapt, LedAction);
		break;

	case SW_LED_MODE6:
		SwLedControlMode6(adapt, LedAction);
		break;

	case SW_LED_MODE7:
		SwLedControlMode7(adapt, LedAction);
		break;

	case SW_LED_MODE8:
		SwLedControlMode8(adapt, LedAction);
		break;

	case SW_LED_MODE9:
		SwLedControlMode9(adapt, LedAction);
		break;

	case SW_LED_MODE10:
		SwLedControlMode10(adapt, LedAction);
		break;

	case SW_LED_MODE11:
		SwLedControlMode11(adapt, LedAction);
		break;

	case SW_LED_MODE12:
		SwLedControlMode12(adapt, LedAction);
		break;

	case SW_LED_MODE13:
		SwLedControlMode13(adapt, LedAction);
		break;

	case SW_LED_MODE14:
		SwLedControlMode14(adapt, LedAction);
		break;

	case SW_LED_MODE15:
		SwLedControlMode15(adapt, LedAction);
		break;

	default:
		break;
	}

}

/*
 *	Description:
 *		Reset status of LED_871x object.
 *   */
void ResetLedStatus(struct led_usb * pLed)
{

	pLed->CurrLedState = RTW_LED_OFF; /* Current LED state. */
	pLed->bLedOn = false; /* true if LED is ON, false if LED is OFF. */

	pLed->bLedBlinkInProgress = false; /* true if it is blinking, false o.w.. */
	pLed->bLedWPSBlinkInProgress = false;

	pLed->BlinkTimes = 0; /* Number of times to toggle led state for blinking. */
	pLed->BlinkCounter = 0;
	pLed->BlinkingLedState = LED_UNKNOWN; /* Next state for blinking, either RTW_LED_ON or RTW_LED_OFF are. */

	pLed->bLedNoLinkBlinkInProgress = false;
	pLed->bLedLinkBlinkInProgress = false;
	pLed->bLedStartToLinkBlinkInProgress = false;
	pLed->bLedScanBlinkInProgress = false;
}

/*
*	Description:
*		Initialize an LED_871x object.
*   */
void
InitLed(
	struct adapter			*adapt,
	struct led_usb *		pLed,
	enum led_pin		LedPin
)
{
	pLed->adapt = adapt;
	pLed->LedPin = LedPin;

	ResetLedStatus(pLed);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	timer_setup(&pLed->BlinkTimer, BlinkTimerCallback, 0);
#else
	rtw_init_timer(&(pLed->BlinkTimer), adapt, BlinkTimerCallback, pLed);
#endif
	_init_workitem(&(pLed->BlinkWorkItem), BlinkWorkItemCallback, pLed);
}


/*
 *	Description:
 *		DeInitialize an LED_871x object.
 *   */
void
DeInitLed(
	struct led_usb *		pLed
)
{
	_cancel_workitem_sync(&(pLed->BlinkWorkItem));
	_cancel_timer_ex(&(pLed->BlinkTimer));
	ResetLedStatus(pLed);
}
