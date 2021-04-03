/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTL8723D_CMD_H__
#define __RTL8723D_CMD_H__

/* ---------------------------------------------------------------------------------------------------------
 * ----------------------------------    H2C CMD DEFINITION    ------------------------------------------------
 * --------------------------------------------------------------------------------------------------------- */

enum h2c_cmd_8723D {
	/* Common Class: 000 */
	H2C_8723D_RSVD_PAGE = 0x00,
	H2C_8723D_MEDIA_STATUS_RPT = 0x01,
	H2C_8723D_SCAN_ENABLE = 0x02,
	H2C_8723D_KEEP_ALIVE = 0x03,
	H2C_8723D_DISCON_DECISION = 0x04,
	H2C_8723D_PSD_OFFLOAD = 0x05,
	H2C_8723D_AP_OFFLOAD = 0x08,
	H2C_8723D_BCN_RSVDPAGE = 0x09,
	H2C_8723D_PROBERSP_RSVDPAGE = 0x0A,
	H2C_8723D_FCS_RSVDPAGE = 0x10,
	H2C_8723D_FCS_INFO = 0x11,
	H2C_8723D_AP_WOW_GPIO_CTRL = 0x13,

	/* PoweSave Class: 001 */
	H2C_8723D_SET_PWR_MODE = 0x20,
	H2C_8723D_PS_TUNING_PARA = 0x21,
	H2C_8723D_PS_TUNING_PARA2 = 0x22,
	H2C_8723D_P2P_LPS_PARAM = 0x23,
	H2C_8723D_P2P_PS_OFFLOAD = 0x24,
	H2C_8723D_PS_SCAN_ENABLE = 0x25,
	H2C_8723D_SAP_PS_ = 0x26,
	H2C_8723D_INACTIVE_PS_ = 0x27, /* Inactive_PS */
	H2C_8723D_FWLPS_IN_IPS_ = 0x28,

	/* Dynamic Mechanism Class: 010 */
	H2C_8723D_MACID_CFG = 0x40,
	H2C_8723D_TXBF = 0x41,
	H2C_8723D_RSSI_SETTING = 0x42,
	H2C_8723D_AP_REQ_TXRPT = 0x43,
	H2C_8723D_INIT_RATE_COLLECT = 0x44,
	H2C_8723D_RA_PARA_ADJUST = 0x46,

	/* BT Class: 011 */
	H2C_8723D_B_TYPE_TDMA = 0x60,
	H2C_8723D_BT_INFO = 0x61,
	H2C_8723D_FORCE_BT_TXPWR = 0x62,
	H2C_8723D_BT_IGNORE_WLANACT = 0x63,
	H2C_8723D_DAC_SWING_VALUE = 0x64,
	H2C_8723D_ANT_SEL_RSV = 0x65,
	H2C_8723D_WL_OPMODE = 0x66,
	H2C_8723D_BT_MP_OPER = 0x67,
	H2C_8723D_BT_CONTROL = 0x68,
	H2C_8723D_BT_WIFI_CTRL = 0x69,
	H2C_8723D_BT_FW_PATCH = 0x6A,
	H2C_8723D_BT_WLAN_CALIBRATION = 0x6D,

	/* WOWLAN Class: 100 */
	H2C_8723D_WOWLAN = 0x80,
	H2C_8723D_REMOTE_WAKE_CTRL = 0x81,
	H2C_8723D_AOAC_GLOBAL_INFO = 0x82,
	H2C_8723D_AOAC_RSVD_PAGE = 0x83,
	H2C_8723D_AOAC_RSVD_PAGE2 = 0x84,
	H2C_8723D_D0_SCAN_OFFLOAD_CTRL = 0x85,
	H2C_8723D_D0_SCAN_OFFLOAD_INFO = 0x86,
	H2C_8723D_CHNL_SWITCH_OFFLOAD = 0x87,
	H2C_8723D_P2P_OFFLOAD_RSVD_PAGE = 0x8A,
	H2C_8723D_P2P_OFFLOAD = 0x8B,

	H2C_8723D_RESET_TSF = 0xC0,
	H2C_8723D_MAXID,
};

/* ---------------------------------------------------------------------------------------------------------
 * ----------------------------------    H2C CMD CONTENT    --------------------------------------------------
 * ---------------------------------------------------------------------------------------------------------
 * _RSVD_PAGE_CMD_0x00 */
static inline void SET_8723D_H2CCMD_RSVD_PAGE_PROBE_RSP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_RSVD_PAGE_PSPOLL(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_RSVD_PAGE_NULL_DATA(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_RSVD_PAGE_QOS_NULL_DATA(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_RSVD_PAGE_BT_QOS_NULL_DATA(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, GENMASK(7, 0));
}

/* _PWR_MOD_CMD_0x20 */
static inline void SET_8723D_H2CCMD_PWRMODE_PARM_MODE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_PWRMODE_PARM_RLBM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(3, 0));
}

static inline void SET_8723D_H2CCMD_PWRMODE_PARM_SMART_PS(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 4));
}

static inline void SET_8723D_H2CCMD_PWRMODE_PARM_BCN_PASS_TIME(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_PWRMODE_PARM_ALL_QUEUE_UAPSD(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_PWRMODE_PARM_BCN_EARLY_C2H_RPT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, BIT(2));
}

static inline void SET_8723D_H2CCMD_PWRMODE_PARM_PWR_STATE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_PWRMODE_PARM_BYTE5(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 5, __Value, GENMASK(7, 0));
}

static inline void GET_8723D_H2CCMD_PWRMODE_PARM_MODE(u8 *__pH2CCmd)
{
	u8_get_bits(*__pH2CCmd + 0, GENMASK(7, 0));
}

/* _PS_TUNE_PARAM_CMD_0x21 */
static inline void SET_8723D_H2CCMD_PSTUNE_PARM_BCN_TO_LIMIT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_PSTUNE_PARM_DTIM_TIMEOUT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_PSTUNE_PARM_ADOPT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, BIT(0));
}

static inline void SET_8723D_H2CCMD_PSTUNE_PARM_PS_TIMEOUT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 1));
}

static inline void SET_8723D_H2CCMD_PSTUNE_PARM_DTIM_PERIOD(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

/* _MACID_CFG_CMD_0x40 */
static inline void SET_8723D_H2CCMD_MACID_CFG_MACID(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_RAID(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(4, 0));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_SGI_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, BIT(7));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_BW(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(1, 0));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_NO_UPDATE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, BIT(3));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_VHT_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(5, 4));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_DISPT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, BIT(6));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_DISRA(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, BIT(7));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_RATE_MASK0(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_RATE_MASK1(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_RATE_MASK2(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 5, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_MACID_CFG_RATE_MASK3(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 6, __Value, GENMASK(7, 0));
}

/* _RSSI_SETTING_CMD_0x42 */
static inline void SET_8723D_H2CCMD_RSSI_SETTING_MACID(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_RSSI_SETTING_RSSI(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(6, 0));
}

static inline void SET_8723D_H2CCMD_RSSI_SETTING_ULDL_STATE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

/* _AP_REQ_TXRPT_CMD_0x43 */
static inline void SET_8723D_H2CCMD_APREQRPT_PARM_MACID1(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_APREQRPT_PARM_MACID2(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

/* _FORCE_BT_TXPWR_CMD_0x62 */
static inline void SET_8723D_H2CCMD_BT_PWR_IDX(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

/* _FORCE_BT_MP_OPER_CMD_0x67 */
static inline void SET_8723D_H2CCMD_BT_MPOPER_VER(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(3, 0));
}

static inline void SET_8723D_H2CCMD_BT_MPOPER_REQNUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 4));
}

static inline void SET_8723D_H2CCMD_BT_MPOPER_IDX(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_BT_MPOPER_PARAM1(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_BT_MPOPER_PARAM2(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_BT_MPOPER_PARAM3(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, GENMASK(7, 0));
}

/* _BT_FW_PATCH_0x6A */
static inline void SET_8723D_H2CCMD_BT_FW_PATCH_SIZE(u8 *__pH2CCmd, u8 __Value)
{
	le16p_replace_bits((__le16 *)__pH2CCmd + 0, __Value, GENMASK(15, 0));
}

static inline void SET_8723D_H2CCMD_BT_FW_PATCH_ADDR0(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_BT_FW_PATCH_ADDR1(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_BT_FW_PATCH_ADDR2(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, GENMASK(7, 0));
}

static inline void SET_8723D_H2CCMD_BT_FW_PATCH_ADDR3(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 5, __Value, GENMASK(7, 0));
}

/* ---------------------------------------------------------------------------------------------------------
 * -------------------------------------------    Structure    --------------------------------------------------
 * --------------------------------------------------------------------------------------------------------- */


/* ---------------------------------------------------------------------------------------------------------
 * ----------------------------------    Function Statement     --------------------------------------------------
 * --------------------------------------------------------------------------------------------------------- */

/* host message to firmware cmd */
void rtl8723d_set_FwPwrMode_cmd(struct adapter * adapt, u8 Mode);
void rtl8723d_set_FwJoinBssRpt_cmd(struct adapter * adapt, u8 mstatus);
/* int rtl8723d_set_lowpwr_lps_cmd(struct adapter * adapt, u8 enable); */
void rtl8723d_set_FwPsTuneParam_cmd(struct adapter * adapt);
void rtl8723d_download_rsvd_page(struct adapter * adapt, u8 mstatus);
void rtl8723d_download_BTCoex_AP_mode_rsvd_page(struct adapter * adapt);
void rtl8723d_set_p2p_ps_offload_cmd(struct adapter * adapt, u8 p2p_ps_state);

int FillH2CCmd8723D(struct adapter * adapt, u8 ElementID, u32 CmdLen, u8 *pCmdBuffer);
u8 GetTxBufferRsvdPageNum8723D(struct adapter *adapt, bool wowlan);

#endif
