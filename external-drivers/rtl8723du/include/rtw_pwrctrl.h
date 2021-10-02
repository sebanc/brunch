/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_PWRCTRL_H_
#define __RTW_PWRCTRL_H_


#define FW_PWR0	0
#define FW_PWR1	1
#define FW_PWR2	2
#define FW_PWR3	3


#define HW_PWR0	7
#define HW_PWR1	6
#define HW_PWR2	2
#define HW_PWR3	0
#define HW_PWR4	8

#define FW_PWRMSK	0x7


#define XMIT_ALIVE	BIT(0)
#define RECV_ALIVE	BIT(1)
#define CMD_ALIVE	BIT(2)
#define EVT_ALIVE	BIT(3)
#define BTCOEX_ALIVE	BIT(4)

enum Power_Mgnt {
	PS_MODE_ACTIVE	= 0	,
	PS_MODE_MIN			,
	PS_MODE_MAX			,
	PS_MODE_DTIM			,	/* PS_MODE_SELF_DEFINED */
	PS_MODE_VOIP			,
	PS_MODE_UAPSD_WMM	,
	PS_MODE_UAPSD			,
	PS_MODE_IBSS			,
	PS_MODE_WWLAN		,
	PM_Radio_Off			,
	PM_Card_Disable		,
	PS_MODE_NUM,
};

enum lps_level {
	LPS_NORMAL = 0,
	LPS_LCLK,
	LPS_PG,
	LPS_LEVEL_MAX,
};

/* BIT[2:0] = HW state
 * BIT[3] = Protocol PS state, 0: register active state, 1: register sleep state
 * BIT[4] = sub-state
 */

#define PS_DPS				BIT(0)
#define PS_LCLK				(PS_DPS)
#define PS_RF_OFF			BIT(1)
#define PS_ALL_ON			BIT(2)
#define PS_ST_ACTIVE		BIT(3)

#define PS_ISR_ENABLE		BIT(4)
#define PS_IMR_ENABLE		BIT(5)
#define PS_ACK				BIT(6)
#define PS_TOGGLE			BIT(7)

#define PS_STATE_MASK		(0x0F)
#define PS_STATE_HW_MASK	(0x07)
#define PS_SEQ_MASK			(0xc0)

#define PS_STATE(x)		(PS_STATE_MASK & (x))
#define PS_STATE_HW(x)	(PS_STATE_HW_MASK & (x))
#define PS_SEQ(x)		(PS_SEQ_MASK & (x))

#define PS_STATE_S0		(PS_DPS)
#define PS_STATE_S1		(PS_LCLK)
#define PS_STATE_S2		(PS_RF_OFF)
#define PS_STATE_S3		(PS_ALL_ON)
#define PS_STATE_S4		((PS_ST_ACTIVE) | (PS_ALL_ON))


#define PS_IS_RF_ON(x)	((x) & (PS_ALL_ON))
#define PS_IS_ACTIVE(x)	((x) & (PS_ST_ACTIVE))
#define CLR_PS_STATE(x)	((x) = ((x) & (0xF0)))


struct reportpwrstate_parm {
	unsigned char mode;
	unsigned char state; /* the CPWM value */
	unsigned short rsvd;
};

static inline void _init_pwrlock(struct semaphore *plock)
{
	sema_init(plock, 1);
}

static inline void _free_pwrlock(struct semaphore *plock)
{
}

static inline void _enter_pwrlock(struct semaphore *plock)
{
	_rtw_down_sema(plock);
}

#define LPS_DELAY_MS	1000 /* 1 sec */

#define EXE_PWR_NONE	0x01
#define EXE_PWR_IPS		0x02
#define EXE_PWR_LPS		0x04

/* RF state. */
enum rt_rf_power_state {
	rf_on,		/* RF is on after RFSleep or RFOff */
	rf_sleep,	/* 802.11 Power Save mode */
	rf_off,		/* HW/SW Radio OFF or Inactive Power Save */
	/* =====Add the new RF state above this line===== */
	rf_max
};

/* RF Off Level for IPS or HW/SW radio off */
#define	RT_RF_OFF_LEVL_ASPM			BIT(0)	/* PCI ASPM */
#define	RT_RF_OFF_LEVL_CLK_REQ		BIT(1)	/* PCI clock request */
#define	RT_RF_OFF_LEVL_PCI_D3			BIT(2)	/* PCI D3 mode */
#define	RT_RF_OFF_LEVL_HALT_NIC		BIT(3)	/* NIC halt, re-initialize hw parameters */
#define	RT_RF_OFF_LEVL_FREE_FW		BIT(4)	/* FW free, re-download the FW */
#define	RT_RF_OFF_LEVL_FW_32K		BIT(5)	/* FW in 32k */
#define	RT_RF_PS_LEVEL_ALWAYS_ASPM	BIT(6)	/* Always enable ASPM and Clock Req in initialization. */
#define	RT_RF_LPS_DISALBE_2R			BIT(30)	/* When LPS is on, disable 2R if no packet is received or transmittd. */
#define	RT_RF_LPS_LEVEL_ASPM			BIT(31)	/* LPS with ASPM */

#define	RT_IN_PS_LEVEL(ppsc, _PS_FLAG)		((ppsc->cur_ps_level & _PS_FLAG) ? true : false)
#define	RT_CLEAR_PS_LEVEL(ppsc, _PS_FLAG)	(ppsc->cur_ps_level &= (~(_PS_FLAG)))
#define	RT_SET_PS_LEVEL(ppsc, _PS_FLAG)		(ppsc->cur_ps_level |= _PS_FLAG)

/* ASPM OSC Control bit, added by Roger, 2013.03.29. */
#define	RT_PCI_ASPM_OSC_IGNORE		0	 /* PCI ASPM ignore OSC control in default */
#define	RT_PCI_ASPM_OSC_ENABLE		BIT0 /* PCI ASPM controlled by OS according to ACPI Spec 5.0 */
#define	RT_PCI_ASPM_OSC_DISABLE		BIT1 /* PCI ASPM controlled by driver or BIOS, i.e., force enable ASPM */


enum _PS_BBRegBackup_ {
	PSBBREG_RF0 = 0,
	PSBBREG_RF1,
	PSBBREG_RF2,
	PSBBREG_AFE0,
	PSBBREG_TOTALCNT
};

enum { /* for ips_mode */
	IPS_NONE = 0,
	IPS_NORMAL,
	IPS_LEVEL_2,
	IPS_NUM
};

/* Design for pwrctrl_priv.ips_deny, 32 bits for 32 reasons at most */
enum ps_deny_reason {
	PS_DENY_DRV_INITIAL = 0,
	PS_DENY_SCAN,
	PS_DENY_JOIN,
	PS_DENY_DISCONNECT,
	PS_DENY_SUSPEND,
	PS_DENY_IOCTL,
	PS_DENY_MGNT_TX,
	PS_DENY_MONITOR_MODE,
	PS_DENY_BEAMFORMING,		/* Beamforming */
	PS_DENY_DRV_REMOVE = 30,
	PS_DENY_OTHERS = 31
};

struct aoac_report {
	u8 iv[8];
	u8 replay_counter_eapol_key[8];
	u8 group_key[32];
	u8 key_index;
	u8 security_type;
	u8 wow_pattern_idx;
	u8 version_info;
	u8 reserved[4];
	u8 rxptk_iv[8];
	u8 rxgtk_iv[4][8];
};

struct pwrctrl_priv {
	struct semaphore	lock;
	struct semaphore	check_32k_lock;
	volatile u8 rpwm; /* requested power state for fw */
	volatile u8 cpwm; /* fw current power state. updated when 1. read from HCPWM 2. driver lowers power level */
	volatile u8 tog; /* toggling */
	volatile u8 cpwm_tog; /* toggling */

	u8	pwr_mode;
	u8	smart_ps;
	u8	bcn_ant_mode;
	u8	dtim;

	u32	alives;
	_workitem cpwm_event;
	_workitem dma_event; /*for handle un-synchronized tx dma*/
	u8	bpower_saving; /* for LPS/IPS */

	u8	b_hw_radio_off;
	u8	reg_rfoff;
	u8	reg_pdnmode; /* powerdown mode */
	u32	rfoff_reason;

	/* RF OFF Level */
	u32	cur_ps_level;
	u32	reg_rfps_level;

	uint	ips_enter_cnts;
	uint	ips_leave_cnts;
	uint	lps_enter_cnts;
	uint	lps_leave_cnts;

	u8	ips_mode;
	u8	ips_org_mode;
	u8	ips_mode_req; /* used to accept the mode setting request, will update to ipsmode later */
	uint bips_processing;
	unsigned long ips_deny_time; /* will deny IPS when system time is smaller than this */
	u8 pre_ips_type;/* 0: default flow, 1: carddisbale flow */

	/* ps_deny: if 0, power save is free to go; otherwise deny all kinds of power save. */
	/* Use enum ps_deny_reason to decide reason. */
	/* Don't access this variable directly without control function, */
	/* and this variable should be protected by lock. */
	u32 ps_deny;

	u8 ps_processing; /* temporarily used to mark whether in rtw_ps_processor */

	u8 fw_psmode_iface_id;
	u8	bLeisurePs;
	u8	LpsIdleCount;
	u8	power_mgnt;
	u8	org_power_mgnt;
	u8	bFwCurrentInPSMode;
	unsigned long	DelayLPSLastTimeStamp;
	int		pnp_current_pwr_state;
	u8		pnp_bstop_trx;

	#ifdef CONFIG_AUTOSUSPEND
	int		ps_flag; /* used by autosuspend */
	u8		bInternalAutoSuspend;
	#endif
	u8		bInSuspend;
	u8		bAutoResume;
	u8		autopm_cnt;
	u8		bSupportRemoteWakeup;
	u8		wowlan_wake_reason;
	u8		wowlan_last_wake_reason;
	u8		wowlan_ap_mode;
	u8		wowlan_mode;
	u8		wowlan_p2p_mode;
	u8		wowlan_pno_enable;
	u8		wowlan_in_resume;

	enum rt_rf_power_state	rf_pwrstate;/* cur power state, only for IPS */
	/* enum rt_rf_power_state	current_rfpwrstate; */
	enum rt_rf_power_state	change_rfpwrstate;

	u8		bHWPowerdown; /* power down mode selection. 0:radio off, 1:power down */
	u8		bHWPwrPindetect; /* come from registrypriv.hwpwrp_detect. enable power down function. 0:disable, 1:enable */
	u8		bkeepfwalive;
	u8		brfoffbyhw;
	unsigned long PS_BBRegBackup[PSBBREG_TOTALCNT];

	u8 lps_level_bk;
	u8 lps_level; /*LPS_NORMAL,LPA_CG,LPS_PG*/
	u8 current_lps_hw_port_id;

	unsigned long radio_on_start_time;
	unsigned long pwr_saving_start_time;
	u32 pwr_saving_time;
	u32 on_time;
	u32 tx_time;
	u32 rx_time;
};

#define rtw_get_ips_mode_req(pwrctl) \
	(pwrctl)->ips_mode_req

#define rtw_ips_mode_req(pwrctl, ips_mode) \
	(pwrctl)->ips_mode_req = (ips_mode)

#define RTW_PWR_STATE_CHK_INTERVAL 2000

#define _rtw_set_pwr_state_check_timer(pwrctl, ms) \
	do { \
		/*RTW_INFO("%s _rtw_set_pwr_state_check_timer(%p, %d)\n", __func__, (pwrctl), (ms));*/ \
		_set_timer(&(pwrctl)->pwr_state_check_timer, (ms)); \
	} while (0)

#define rtw_set_pwr_state_check_timer(__padapt) \
	_rtw_set_pwr_state_check_timer((__padapt), (__padapt)->pwr_state_check_interval)

extern void rtw_init_pwrctrl_priv(struct adapter *adapter);
extern void rtw_free_pwrctrl_priv(struct adapter *adapter);

extern void LeaveAllPowerSaveMode(struct adapter * Adapter);
extern void LeaveAllPowerSaveModeDirect(struct adapter * Adapter);
void _ips_enter(struct adapter *adapt);
void ips_enter(struct adapter *adapt);
int _ips_leave(struct adapter *adapt);
int ips_leave(struct adapter *adapt);

void rtw_ps_processor(struct adapter *adapt);

#ifdef CONFIG_AUTOSUSPEND
int autoresume_enter(struct adapter *adapt);
#endif
#ifdef SUPPORT_HW_RFOFF_DETECTED
enum rt_rf_power_state RfOnOffDetect(IN	struct adapter * pAdapter);
#endif


int rtw_fw_ps_state(struct adapter * adapt);

int LPS_RF_ON_check(struct adapter * adapt, u32 delay_ms);
void LPS_Enter(struct adapter * adapt, const char *msg);
void LPS_Leave(struct adapter * adapt, const char *msg);
void traffic_check_for_leave_lps(struct adapter * adapt, u8 tx, u32 tx_packets);
void rtw_set_ps_mode(struct adapter * adapt, u8 ps_mode, u8 smart_ps, u8 bcn_ant_mode, const char *msg);
void rtw_set_fw_in_ips_mode(struct adapter * adapt, u8 enable);
void rtw_set_rpwm(struct adapter *adapt, u8 val8);
void rtw_wow_lps_level_decide(struct adapter *adapter, u8 wow_en);

#define rtw_is_earlysuspend_registered(pwrpriv) false
#define rtw_is_do_late_resume(pwrpriv) false
#define rtw_set_do_late_resume(pwrpriv, enable) do {} while (0)
#define rtw_register_early_suspend(pwrpriv) do {} while (0)
#define rtw_unregister_early_suspend(pwrpriv) do {} while (0)

u8 rtw_interface_ps_func(struct adapter *adapt, enum hal_intf_ps_func efunc_id, u8 *val);
void rtw_set_ips_deny(struct adapter *adapt, u32 ms);
int _rtw_pwr_wakeup(struct adapter *adapt, u32 ips_deffer_ms, const char *caller);
#define rtw_pwr_wakeup(adapter) _rtw_pwr_wakeup(adapter, RTW_PWR_STATE_CHK_INTERVAL, __func__)
#define rtw_pwr_wakeup_ex(adapter, ips_deffer_ms) _rtw_pwr_wakeup(adapter, ips_deffer_ms, __func__)
int rtw_pm_set_ips(struct adapter *adapt, u8 mode);
int rtw_pm_set_lps(struct adapter *adapt, u8 mode);
int rtw_pm_set_lps_level(struct adapter *adapt, u8 level);

void rtw_ps_deny(struct adapter * adapt, enum ps_deny_reason reason);
void rtw_ps_deny_cancel(struct adapter * adapt, enum ps_deny_reason reason);
u32 rtw_ps_deny_get(struct adapter * adapt);

#endif /* __RTL871X_PWRCTRL_H_ */
