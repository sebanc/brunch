/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __HAL_COMMON_LED_H_
#define __HAL_COMMON_LED_H_

#define NO_LED 0
#define HW_LED 1

#define MSECS(t)        (HZ * ((t) / 1000) + (HZ * ((t) % 1000)) / 1000)

/* ********************************************************************************
 *	LED Behavior Constant.
 * ********************************************************************************
 * Default LED behavior.
 *   */
#define LED_BLINK_NORMAL_INTERVAL	100
#define LED_BLINK_SLOWLY_INTERVAL	200
#define LED_BLINK_LONG_INTERVAL	400
#define LED_INITIAL_INTERVAL		1800

/* LED Customerization */

/* NETTRONIX */
#define LED_BLINK_NORMAL_INTERVAL_NETTRONIX	100
#define LED_BLINK_SLOWLY_INTERVAL_NETTRONIX	2000

/* PORNET */
#define LED_BLINK_SLOWLY_INTERVAL_PORNET	1000
#define LED_BLINK_NORMAL_INTERVAL_PORNET	100
#define LED_BLINK_FAST_INTERVAL_BITLAND		30

/* AzWave. */
#define LED_CM2_BLINK_ON_INTERVAL		250
#define LED_CM2_BLINK_OFF_INTERVAL		4750
#define LED_CM8_BLINK_OFF_INTERVAL		3750	/* for QMI */

/* RunTop */
#define LED_RunTop_BLINK_INTERVAL		300

/* ALPHA */
#define LED_BLINK_NO_LINK_INTERVAL_ALPHA	1000
#define LED_BLINK_NO_LINK_INTERVAL_ALPHA_500MS 500 /* add by ylb 20121012 for customer led for alpha */
#define LED_BLINK_LINK_INTERVAL_ALPHA		500	/* 500 */
#define LED_BLINK_SCAN_INTERVAL_ALPHA		180	/* 150 */
#define LED_BLINK_FASTER_INTERVAL_ALPHA		50
#define LED_BLINK_WPS_SUCESS_INTERVAL_ALPHA	5000

/* 111122 by hpfan: Customized for Xavi */
#define LED_CM11_BLINK_INTERVAL			300
#define LED_CM11_LINK_ON_INTERVEL		3000

/* Netgear */
#define LED_BLINK_LINK_INTERVAL_NETGEAR		500
#define LED_BLINK_LINK_SLOWLY_INTERVAL_NETGEAR	1000

#define LED_WPS_BLINK_OFF_INTERVAL_NETGEAR	100
#define LED_WPS_BLINK_ON_INTERVAL_NETGEAR	500

/* Belkin AC950 */
#define LED_BLINK_LINK_INTERVAL_ON_BELKIN	200
#define LED_BLINK_LINK_INTERVAL_OFF_BELKIN	100
#define LED_BLINK_ERROR_INTERVAL_BELKIN		100

/* by chiyokolin for Azurewave */
#define LED_CM12_BLINK_INTERVAL_5Mbps		160
#define LED_CM12_BLINK_INTERVAL_10Mbps		80
#define LED_CM12_BLINK_INTERVAL_20Mbps		50
#define LED_CM12_BLINK_INTERVAL_40Mbps		40
#define LED_CM12_BLINK_INTERVAL_80Mbps		30
#define LED_CM12_BLINK_INTERVAL_MAXMbps		25

/* Dlink */
#define	LED_BLINK_NO_LINK_INTERVAL		1000
#define	LED_BLINK_LINK_IDEL_INTERVAL		100

#define	LED_BLINK_SCAN_ON_INTERVAL		30
#define	LED_BLINK_SCAN_OFF_INTERVAL		300

#define LED_WPS_BLINK_ON_INTERVAL_DLINK		30
#define LED_WPS_BLINK_OFF_INTERVAL_DLINK			300
#define LED_WPS_BLINK_LINKED_ON_INTERVAL_DLINK			5000

/* ********************************************************************************
 * LED object.
 * ******************************************************************************** */

enum led_ctl_mode {
	LED_CTL_POWER_ON = 1,
	LED_CTL_LINK = 2,
	LED_CTL_NO_LINK = 3,
	LED_CTL_TX = 4,
	LED_CTL_RX = 5,
	LED_CTL_SITE_SURVEY = 6,
	LED_CTL_POWER_OFF = 7,
	LED_CTL_START_TO_LINK = 8,
	LED_CTL_START_WPS = 9,
	LED_CTL_STOP_WPS = 10,
	LED_CTL_START_WPS_BOTTON = 11, /* added for runtop */
	LED_CTL_STOP_WPS_FAIL = 12, /* added for ALPHA	 */
	LED_CTL_STOP_WPS_FAIL_OVERLAP = 13, /* added for BELKIN */
	LED_CTL_CONNECTION_NO_TRANSFER = 14,
};

enum led_state {
	LED_UNKNOWN = 0,
	RTW_LED_ON = 1,
	RTW_LED_OFF = 2,
	LED_BLINK_NORMAL = 3,
	LED_BLINK_SLOWLY = 4,
	LED_BLINK_POWER_ON = 5,
	LED_BLINK_SCAN = 6,	/* LED is blinking during scanning period, the # of times to blink is depend on time for scanning. */
	LED_BLINK_NO_LINK = 7, /* LED is blinking during no link state. */
	LED_BLINK_StartToBlink = 8, /* Customzied for Sercomm Printer Server case */
	LED_BLINK_TXRX = 9,
	LED_BLINK_WPS = 10,	/* LED is blinkg during WPS communication */
	LED_BLINK_WPS_STOP = 11,	/* for ALPHA */
	LED_BLINK_WPS_STOP_OVERLAP = 12,	/* for BELKIN */
	LED_BLINK_RUNTOP = 13,	/* Customized for RunTop */
	LED_BLINK_CAMEO = 14,
	LED_BLINK_XAVI = 15,
	LED_BLINK_ALWAYS_ON = 16,
	LED_BLINK_LINK_IN_PROCESS = 17,  /* Customized for Belkin AC950 */
	LED_BLINK_AUTH_ERROR = 18,  /* Customized for Belkin AC950 */
	LED_BLINK_Azurewave_5Mbps = 19,
	LED_BLINK_Azurewave_10Mbps = 20,
	LED_BLINK_Azurewave_20Mbps = 21,
	LED_BLINK_Azurewave_40Mbps = 22,
	LED_BLINK_Azurewave_80Mbps = 23,
	LED_BLINK_Azurewave_MAXMbps = 24,
	LED_BLINK_LINK_IDEL = 25,
	LED_BLINK_WPS_LINKED = 26,
};

enum led_pin {
	LED_PIN_GPIO0,
	LED_PIN_LED0,
	LED_PIN_LED1,
	LED_PIN_LED2
};

/* ********************************************************************************
 * USB  LED Definition.
 * ******************************************************************************** */

#define IS_LED_WPS_BLINKING(_led_usb)	(((struct led_usb *)_led_usb)->CurrLedState == LED_BLINK_WPS \
		|| ((struct led_usb *)_led_usb)->CurrLedState == LED_BLINK_WPS_STOP \
		|| ((struct led_usb *)_led_usb)->bLedWPSBlinkInProgress)

#define IS_LED_BLINKING(_led_usb)	(((struct led_usb *)_led_usb)->bLedWPSBlinkInProgress \
		|| ((struct led_usb *)_led_usb)->bLedScanBlinkInProgress)

enum led_strategy {
	/* start from 2 */
	SW_LED_MODE0 = 2, /* SW control 1 LED via GPIO0. It is default option. */
	SW_LED_MODE1, /* 2 LEDs, through LED0 and LED1. For ALPHA. */
	SW_LED_MODE2, /* SW control 1 LED via GPIO0, customized for AzWave 8187 minicard. */
	SW_LED_MODE3, /* SW control 1 LED via GPIO0, customized for Sercomm Printer Server case. */
	SW_LED_MODE4, /* for Edimax / Belkin */
	SW_LED_MODE5, /* for Sercomm / Belkin	 */
	SW_LED_MODE6,	/* for 88CU minicard, porting from ce SW_LED_MODE7 */
	SW_LED_MODE7,	/* for Netgear special requirement */
	SW_LED_MODE8, /* for LC */
	SW_LED_MODE9, /* for Belkin AC950 */
	SW_LED_MODE10, /* for Netgear A6200V2 */
	SW_LED_MODE11, /* for Edimax / ASUS */
	SW_LED_MODE12, /* for WNC/NEC */
	SW_LED_MODE13, /* for Netgear A6100, 8811Au */
	SW_LED_MODE14, /* for Buffalo, DNI, 8811Au */
	SW_LED_MODE15, /* for DLINK,  8811Au/8812AU	 */
};


struct led_usb {
	struct adapter *adapt;

	enum led_pin LedPin;	/* Identify how to implement this SW led. */

	enum led_state			CurrLedState; /* Current LED state. */
	bool				bLedOn; /* true if LED is ON, false if LED is OFF. */

	bool				bSWLedCtrl;

	bool				bLedBlinkInProgress; /* true if it is blinking, false o.w.. */
	/* ALPHA, added by chiyoko, 20090106 */
	bool				bLedNoLinkBlinkInProgress;
	bool				bLedLinkBlinkInProgress;
	bool				bLedStartToLinkBlinkInProgress;
	bool				bLedScanBlinkInProgress;
	bool				bLedWPSBlinkInProgress;

	u32					BlinkTimes; /* Number of times to toggle led state for blinking. */
	u8					BlinkCounter; /* Added for turn off overlap led after blinking a while, by page, 20120821 */
	enum led_state			BlinkingLedState; /* Next state for blinking, either LED_ON or LED_OFF are. */

	struct timer_list		BlinkTimer; /* Timer object for led blinking. */

	_workitem			BlinkWorkItem; /* Workitem used by BlinkTimer to manipulate H/W to blink LED.' */
};

void LedControlUSB(
	struct adapter *		Adapter,
	enum led_ctl_mode		LedAction
);

struct led_priv {
	enum led_strategy LedStrategy;
	/* add for led controll */
	struct led_usb			SwLed0;
	struct led_usb			SwLed1;
	struct led_usb			SwLed2;
	u8					bRegUseLed;
	void (*LedControlHandler)(struct adapter *adapt, enum led_ctl_mode LedAction);
	void (*SwLedOn)(struct adapter *adapt, struct led_usb * pLed);
	void (*SwLedOff)(struct adapter *adapt, struct led_usb * pLed);
	/* add for led controll */
};

#define SwLedOn(adapter, pLed) \
	do { \
		if ((adapter)->ledpriv.SwLedOn) \
			(adapter)->ledpriv.SwLedOn((adapter), (pLed)); \
	} while (0)

#define SwLedOff(adapter, pLed) \
	do { \
		if ((adapter)->ledpriv.SwLedOff) \
			(adapter)->ledpriv.SwLedOff((adapter), (pLed)); \
	} while (0)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void BlinkTimerCallback(struct timer_list *t);
#else
void BlinkTimerCallback(void *data);
#endif
void BlinkWorkItemCallback(_workitem *work);

void ResetLedStatus(struct led_usb * pLed);

void
InitLed(
	struct adapter			*adapt,
	struct led_usb *		pLed,
	enum led_pin LedPin
);

void
DeInitLed(
	struct led_usb *		pLed
);

/* hal... */
extern void BlinkHandler(struct led_usb *	pLed);

#define rtw_led_control(adapter, LedAction) \
	do { \
		if ((adapter)->ledpriv.LedControlHandler) \
			(adapter)->ledpriv.LedControlHandler((adapter), (LedAction)); \
	} while (0)

#define rtw_led_get_strategy(adapter) ((adapter)->ledpriv.LedStrategy)

#define IS_NO_LED_STRATEGY(s) ((s) == NO_LED)
#define IS_HW_LED_STRATEGY(s) ((s) == HW_LED)
#define IS_SW_LED_STRATEGY(s) ((s) != NO_LED && (s) != HW_LED)

#endif /*__HAL_COMMON_LED_H_*/

