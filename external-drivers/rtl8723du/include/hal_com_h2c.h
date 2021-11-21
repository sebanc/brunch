/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __COMMON_H2C_H__
#define __COMMON_H2C_H__

#include <linux/bitfield.h>

/* ---------------------------------------------------------------------------------------------------------
 * ----------------------------------    H2C CMD DEFINITION    ------------------------------------------------
 * ---------------------------------------------------------------------------------------------------------
 * 88e, 8723b, 8812, 8821, 92e use the same FW code base */
enum h2c_cmd {
	/* Common Class: 000 */
	H2C_RSVD_PAGE = 0x00,
	H2C_MEDIA_STATUS_RPT = 0x01,
	H2C_SCAN_ENABLE = 0x02,
	H2C_KEEP_ALIVE = 0x03,
	H2C_DISCON_DECISION = 0x04,
	H2C_PSD_OFFLOAD = 0x05,
	H2C_CUSTOMER_STR_REQ = 0x06,
	H2C_AP_OFFLOAD = 0x08,
	H2C_BCN_RSVDPAGE = 0x09,
	H2C_PROBERSP_RSVDPAGE = 0x0A,
	H2C_FCS_RSVDPAGE = 0x10,
	H2C_FCS_INFO = 0x11,
	H2C_AP_WOW_GPIO_CTRL = 0x13,
	H2C_CHNL_SWITCH_OPER_OFFLOAD = 0x1C,
	H2C_SINGLE_CHANNELSWITCH_V2 = 0x1D,

	/* PoweSave Class: 001 */
	H2C_SET_PWR_MODE = 0x20,
	H2C_PS_TUNING_PARA = 0x21,
	H2C_PS_TUNING_PARA2 = 0x22,
	H2C_P2P_LPS_PARAM = 0x23,
	H2C_P2P_PS_OFFLOAD = 0x24,
	H2C_PS_SCAN_ENABLE = 0x25,
	H2C_SAP_PS_ = 0x26,
	H2C_INACTIVE_PS_ = 0x27, /* Inactive_PS */
	H2C_FWLPS_IN_IPS_ = 0x28,

#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
	H2C_DEFAULT_PORT_ID = 0x2C,
#endif
	/* Dynamic Mechanism Class: 010 */
	H2C_MACID_CFG = 0x40,
	H2C_TXBF = 0x41,
	H2C_RSSI_SETTING = 0x42,
	H2C_AP_REQ_TXRPT = 0x43,
	H2C_INIT_RATE_COLLECT = 0x44,
	H2C_IQ_CALIBRATION	= 0x45,

	H2C_RA_MASK_3SS = 0x46,/* for 8814A */
	H2C_RA_PARA_ADJUST = 0x47,
	H2C_DYNAMIC_TX_PATH = 0x48,/* for 8814A */

	H2C_FW_TRACE_EN = 0x49,

	/* BT Class: 011 */
	H2C_B_TYPE_TDMA = 0x60,
	H2C_BT_INFO = 0x61,
	H2C_FORCE_BT_TXPWR = 0x62,
	H2C_BT_IGNORE_WLANACT = 0x63,
	H2C_DAC_SWING_VALUE = 0x64,
	H2C_ANT_SEL_RSV = 0x65,
	H2C_WL_OPMODE = 0x66,
	H2C_BT_MP_OPER = 0x67,
	H2C_BT_CONTROL = 0x68,
	H2C_BT_WIFI_CTRL = 0x69,
	H2C_BT_FW_PATCH = 0x6A,
#if defined(CONFIG_FW_MULTI_PORT_SUPPORT)
	H2C_BTC_WL_PORT_ID = 0x71,
#endif
	/* WOWLAN Class: 100 */
	H2C_WOWLAN = 0x80,
	H2C_REMOTE_WAKE_CTRL = 0x81,
	H2C_AOAC_GLOBAL_INFO = 0x82,
	H2C_AOAC_RSVD_PAGE = 0x83,
	H2C_AOAC_RSVD_PAGE2 = 0x84,
	H2C_D0_SCAN_OFFLOAD_CTRL = 0x85,
	H2C_D0_SCAN_OFFLOAD_INFO = 0x86,
	H2C_CHNL_SWITCH_OFFLOAD = 0x87,
	H2C_AOAC_RSVDPAGE3 = 0x88,
	H2C_P2P_OFFLOAD_RSVD_PAGE = 0x8A,
	H2C_P2P_OFFLOAD = 0x8B,

	H2C_RESET_TSF = 0xC0,
	H2C_BCNHWSEQ = 0xC5,
	H2C_CUSTOMER_STR_W1 = 0xC6,
	H2C_CUSTOMER_STR_W2 = 0xC7,
	H2C_CUSTOMER_STR_W3 = 0xC8,
	H2C_MAXID,
};

#define H2C_INACTIVE_PS_LEN		3
#define H2C_RSVDPAGE_LOC_LEN		5
#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
#define H2C_DEFAULT_PORT_ID_LEN		2
#define H2C_MEDIA_STATUS_RPT_LEN		4
#else
#define H2C_MEDIA_STATUS_RPT_LEN		3
#endif
#define H2C_KEEP_ALIVE_CTRL_LEN	2
#define H2C_DISCON_DECISION_LEN		3
#define H2C_AP_OFFLOAD_LEN		3
#define H2C_AP_WOW_GPIO_CTRL_LEN	4
#define H2C_AP_PS_LEN			2
#define H2C_PWRMODE_LEN			7
#define H2C_PSTUNEPARAM_LEN			4
#define H2C_MACID_CFG_LEN		7
#define H2C_BTMP_OPER_LEN			5
#define H2C_WOWLAN_LEN			6
#define H2C_REMOTE_WAKE_CTRL_LEN	3
#define H2C_AOAC_GLOBAL_INFO_LEN	2
#define H2C_AOAC_RSVDPAGE_LOC_LEN	7
#define H2C_SCAN_OFFLOAD_CTRL_LEN	4
#define H2C_BT_FW_PATCH_LEN			6
#define H2C_RSSI_SETTING_LEN		4
#define H2C_AP_REQ_TXRPT_LEN		3
#define H2C_FORCE_BT_TXPWR_LEN		3
#define H2C_BCN_RSVDPAGE_LEN		5
#define H2C_PROBERSP_RSVDPAGE_LEN	5
#define H2C_P2PRSVDPAGE_LOC_LEN		5
#define H2C_P2P_OFFLOAD_LEN	3

#if defined(CONFIG_FW_MULTI_PORT_SUPPORT)
#define H2C_BTC_WL_PORT_ID_LEN	1
#endif
#define H2C_SINGLE_CHANNELSWITCH_V2_LEN 2

#define eq_mac_addr(a, b)						(((a)[0] == (b)[0] && (a)[1] == (b)[1] && (a)[2] == (b)[2] && (a)[3] == (b)[3] && (a)[4] == (b)[4] && (a)[5] == (b)[5]) ? 1 : 0)
#define cp_mac_addr(des, src)					((des)[0] = (src)[0], (des)[1] = (src)[1], (des)[2] = (src)[2], (des)[3] = (src)[3], (des)[4] = (src)[4], (des)[5] = (src)[5])
#define cpIpAddr(des, src)					((des)[0] = (src)[0], (des)[1] = (src)[1], (des)[2] = (src)[2], (des)[3] = (src)[3])


/* _RSVD_PAGE_CMD_0x00 */
static inline void SET_H2CCMD_RSVD_PAGE_PROBE_RSP(u8 *__pH2CCmd, u8 __Value)
{
	*(__pH2CCmd) = __Value;
}

static inline void SET_H2CCMD_RSVD_PAGE_PSPOLL(u8 *__pH2CCmd, u8 __Value)
{
	*(__pH2CCmd + 1) = __Value;
}

static inline void SET_H2CCMD_RSVD_PAGE_NULL_DATA(u8 *__pH2CCmd, u8 __Value)
{
	*(__pH2CCmd + 2) = __Value;
}

static inline void SET_H2CCMD_RSVD_PAGE_QOS_NULL_DATA(u8 *__pH2CCmd, u8 __Value)
{
	*(__pH2CCmd + 3) = __Value;
}

static inline void SET_H2CCMD_RSVD_PAGE_BT_QOS_NULL_DATA(u8 *__pH2CCmd, u8 __Value)
{
	*(__pH2CCmd + 4) = __Value;
}

/* _MEDIA_STATUS_RPT_PARM_CMD_0x01 */
static inline void SET_H2CCMD_MSRRPT_PARM_OPMODE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

static inline void SET_H2CCMD_MSRRPT_PARM_MACID_IND(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(1));
}

static inline void SET_H2CCMD_MSRRPT_PARM_MIRACAST(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(2));
}

static inline void SET_H2CCMD_MSRRPT_PARM_MIRACAST_SINK(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(3));
}

static inline void SET_H2CCMD_MSRRPT_PARM_ROLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(7, 4));
}

static inline void SET_H2CCMD_MSRRPT_PARM_MACID(u8 *__pH2CCmd, u8 __Value)
{
	*(__pH2CCmd + 1) = __Value;
}

static inline void SET_H2CCMD_MSRRPT_PARM_MACID_END(u8 *__pH2CCmd, u8 __Value)
{
	*(__pH2CCmd + 2) = __Value;
}

static inline void SET_H2CCMD_MSRRPT_PARM_PORT_NUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(2, 0));
}

static inline int GET_H2CCMD_MSRRPT_PARM_OPMODE(u8 *__pH2CCmd)
{
	return u8_get_bits(*__pH2CCmd, BIT(0));
}

static inline int GET_H2CCMD_MSRRPT_PARM_MIRACAST(u8 *__pH2CCmd)
{
	return u8_get_bits(*__pH2CCmd, BIT(2));
}

static inline int GET_H2CCMD_MSRRPT_PARM_MIRACAST_SINK(u8 *__pH2CCmd)
{
	return u8_get_bits(*__pH2CCmd, BIT(3));
}

static inline int GET_H2CCMD_MSRRPT_PARM_ROLE(u8 *__pH2CCmd)
{
	return u8_get_bits(*__pH2CCmd, GENMASK(7, 4));
}

#define H2C_MSR_ROLE_RSVD	0
#define H2C_MSR_ROLE_STA	1
#define H2C_MSR_ROLE_AP		2
#define H2C_MSR_ROLE_GC		3
#define H2C_MSR_ROLE_GO		4
#define H2C_MSR_ROLE_TDLS	5
#define H2C_MSR_ROLE_ADHOC	6
#define H2C_MSR_ROLE_MESH	7
#define H2C_MSR_ROLE_MAX	8

extern const char *const _h2c_msr_role_str[];
#define h2c_msr_role_str(role) (((role) >= H2C_MSR_ROLE_MAX) ? _h2c_msr_role_str[H2C_MSR_ROLE_MAX] : _h2c_msr_role_str[(role)])

#define H2C_MSR_FMT "%s %s%s"
#define H2C_MSR_ARG(h2c_msr) \
	GET_H2CCMD_MSRRPT_PARM_OPMODE((h2c_msr)) ? " C" : "", \
	h2c_msr_role_str(GET_H2CCMD_MSRRPT_PARM_ROLE((h2c_msr))), \
	GET_H2CCMD_MSRRPT_PARM_MIRACAST((h2c_msr)) ? (GET_H2CCMD_MSRRPT_PARM_MIRACAST_SINK((h2c_msr)) ? " MSINK" : " MSRC") : ""

int rtw_hal_set_FwMediaStatusRpt_cmd(struct adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid, bool macid_ind, u8 macid_end);
int rtw_hal_set_FwMediaStatusRpt_single_cmd(struct adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid);
int rtw_hal_set_FwMediaStatusRpt_range_cmd(struct adapter *adapter, bool opmode, bool miracast, bool miracast_sink, u8 role, u8 macid, u8 macid_end);

/* _KEEP_ALIVE_CMD_0x03 */
static inline void SET_H2CCMD_KEEPALIVE_PARM_ENABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

static inline void SET_H2CCMD_KEEPALIVE_PARM_ADOPT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(1));
}

static inline void SET_H2CCMD_KEEPALIVE_PARM_PKT_TYPE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(2));
}

static inline void SET_H2CCMD_KEEPALIVE_PARM_PORT_NUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(5, 3));
}

static inline void SET_H2CCMD_KEEPALIVE_PARM_CHECK_PERIOD(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

/* _DISCONNECT_DECISION_CMD_0x04 */
static inline void SET_H2CCMD_DISCONDECISION_PARM_ENABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

static inline void SET_H2CCMD_DISCONDECISION_PARM_ADOPT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(1));
}

static inline void SET_H2CCMD_DISCONDECISION_PORT_NUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(6, 4));
}

static inline void SET_H2CCMD_DISCONDECISION_PARM_CHECK_PERIOD(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_DISCONDECISION_PARM_TRY_PKT_NUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

#define RTW_CUSTOMER_STR_LEN 16
#define RTW_CUSTOMER_STR_FMT "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"
#define RTW_CUSTOMER_STR_ARG(x) ((u8 *)(x))[0], ((u8 *)(x))[1], ((u8 *)(x))[2], ((u8 *)(x))[3], ((u8 *)(x))[4], ((u8 *)(x))[5], \
	((u8 *)(x))[6], ((u8 *)(x))[7], ((u8 *)(x))[8], ((u8 *)(x))[9], ((u8 *)(x))[10], ((u8 *)(x))[11], \
	((u8 *)(x))[12], ((u8 *)(x))[13], ((u8 *)(x))[14], ((u8 *)(x))[15]

/* H2C_CUSTOMER_STR_REQ  0x06 */
#define H2C_CUSTOMER_STR_REQ_LEN 1
static inline void SET_H2CCMD_CUSTOMER_STR_REQ_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

int rtw_hal_h2c_customer_str_req(struct adapter *adapter);
int rtw_hal_customer_str_read(struct adapter *adapter, u8 *cs);

/* H2C_CUSTOMER_STR_W1 0xC6 */
#define H2C_CUSTOMER_STR_W1_LEN 7
static inline void SET_H2CCMD_CUSTOMER_STR_W1_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

#define H2CCMD_CUSTOMER_STR_W1_BYTE0(__pH2CCmd)				(((u8 *)(__pH2CCmd)) + 1)

/* H2C_CUSTOMER_STR_W2 0xC7 */
#define H2C_CUSTOMER_STR_W2_LEN 7
static inline void SET_H2CCMD_CUSTOMER_STR_W2_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

#define H2CCMD_CUSTOMER_STR_W2_BYTE6(__pH2CCmd)				(((u8 *)(__pH2CCmd)) + 1)

/* H2C_CUSTOMER_STR_W3 0xC8 */
#define H2C_CUSTOMER_STR_W3_LEN 5
static inline void SET_H2CCMD_CUSTOMER_STR_W3_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

#define H2CCMD_CUSTOMER_STR_W3_BYTE12(__pH2CCmd)			(((u8 *)(__pH2CCmd)) + 1)
int rtw_hal_h2c_customer_str_write(struct adapter *adapter, const u8 *cs);
int rtw_hal_customer_str_write(struct adapter *adapter, const u8 *cs);

/* _AP_Offload 0x08 */
static inline void SET_H2CCMD_AP_WOWLAN_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(7, 0));
}

/* _BCN_RsvdPage	0x09 */
static inline void SET_H2CCMD_AP_WOWLAN_RSVD_PAGE_BCN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(7, 0));
}

/* _Probersp_RsvdPage 0x0a */
static inline void SET_H2CCMD_AP_WOWLAN_RSVD_PAGE_ProbeRsp(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(7, 0));
}

/* _Probersp_RsvdPage 0x13 */
static inline void SET_H2CCMD_AP_WOW_GPIO_CTRL_INDEX(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(3, 0));
}

static inline void SET_H2CCMD_AP_WOW_GPIO_CTRL_C2H_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(4));
}

static inline void SET_H2CCMD_AP_WOW_GPIO_CTRL_PLUS(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(5));
}

static inline void SET_H2CCMD_AP_WOW_GPIO_CTRL_HIGH_ACTIVE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(6));
}

static inline void SET_H2CCMD_AP_WOW_GPIO_CTRL_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(7));
}

static inline void SET_H2CCMD_AP_WOW_GPIO_CTRL_DURATION(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AP_WOW_GPIO_CTRL_C2H_DURATION(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

/* _AP_PS 0x26 */
static inline void SET_H2CCMD_AP_WOW_PS_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

static inline void SET_H2CCMD_AP_WOW_PS_32K_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(1));
}

static inline void SET_H2CCMD_AP_WOW_PS_RF(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(2));
}

static inline void SET_H2CCMD_AP_WOW_PS_DURATION(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
/* DEFAULT PORT ID 0x2C*/
static inline void SET_H2CCMD_DFTPID_PORT_ID(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_DFTPID_MAC_ID(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}
#endif

/* CHNL SWITCH OPER OFFLOAD 0x1C */
static inline void SET_H2CCMD_CH_SW_OPER_OFFLOAD_CH_NUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_CH_SW_OPER_OFFLOAD_BW_MODE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(1, 0));
}

static inline void SET_H2CCMD_CH_SW_OPER_OFFLOAD_BW_40M_SC(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(4, 2));
}

static inline void SET_H2CCMD_CH_SW_OPER_OFFLOAD_BW_80M_SC(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 5));
}

static inline void SET_H2CCMD_CH_SW_OPER_OFFLOAD_RFE_TYPE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(3, 0));
}

/* H2C_SINGLE_CHANNELSWITCH_V2 = 0x1D */
static inline void SET_H2CCMD_SINGLE_CH_SWITCH_V2_CENTRAL_CH_NUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_SINGLE_CH_SWITCH_V2_PRIMARY_CH_IDX(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(3, 0));
}

static inline void SET_H2CCMD_SINGLE_CH_SWITCH_V2_BW(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 4));
}

#if defined(CONFIG_FW_MULTI_PORT_SUPPORT)
#static inline voidSET_H2CCMD_BTC_WL_PORT_ID(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, GENMASK(3, 0));
}
#endif

/* _WoWLAN PARAM_CMD_0x80 */
static inline void SET_H2CCMD_WOWLAN_FUNC_ENABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(0));
}

static inline void SET_H2CCMD_WOWLAN_PATTERN_MATCH_ENABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(1));
}

static inline void SET_H2CCMD_WOWLAN_MAGIC_PKT_ENABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(2));
}

static inline void SET_H2CCMD_WOWLAN_UNICAST_PKT_ENABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(3));
}

static inline void SET_H2CCMD_WOWLAN_ALL_PKT_DROP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(4));
}

static inline void SET_H2CCMD_WOWLAN_GPIO_ACTIVE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd, __Value, BIT(5));
}

static inline void SET_H2CCMD_WOWLAN_REKEY_WAKE_UP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(6));
}

static inline void SET_H2CCMD_WOWLAN_DISCONNECT_WAKE_UP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(7));
}

static inline void SET_H2CCMD_WOWLAN_GPIONUM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_WOWLAN_DATAPIN_WAKE_UP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, BIT(7));
}

static inline void SET_H2CCMD_WOWLAN_GPIO_DURATION(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_WOWLAN_GPIO_PULSE_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, BIT(0));
}

static inline void SET_H2CCMD_WOWLAN_GPIO_PULSE_COUNT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 1));
}

static inline void SET_H2CCMD_WOWLAN_DISABLE_UPHY(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, BIT(0));
}

static inline void SET_H2CCMD_WOWLAN_HST2DEV_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, BIT(1));
}

static inline void SET_H2CCMD_WOWLAN_GPIO_DURATION_MS(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, BIT(2));
}

static inline void SET_H2CCMD_WOWLAN_CHANGE_UNIT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, BIT(2));
}

static inline void SET_H2CCMD_WOWLAN_UNIT_FOR_UPHY_DISABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, BIT(3));
}

static inline void SET_H2CCMD_WOWLAN_TAKE_PDN_UPHY_DIS_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, BIT(4));
}

static inline void SET_H2CCMD_WOWLAN_GPIO_INPUT_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, BIT(5));
}

/* _REMOTE_WAKEUP_CMD_0x81 */
static inline void SET_H2CCMD_REMOTE_WAKECTRL_ENABLE(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(0));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_ARP_OFFLOAD_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(1));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_NDP_OFFLOAD_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(2));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_GTK_OFFLOAD_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(3));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_NLO_OFFLOAD_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(4));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_FW_UNICAST_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, BIT(7));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_P2P_OFFLAD_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, BIT(0));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_NBNS_FILTER_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, BIT(2));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_TKIP_OFFLOAD_EN(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, BIT(3));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_ARP_ACTION(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, BIT(0));
}

static inline void SET_H2CCMD_REMOTE_WAKE_CTRL_FW_PARSING_UNTIL_WAKEUP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, BIT(4));
}

/* AOAC_GLOBAL_INFO_0x82 */
static inline void SET_H2CCMD_AOAC_GLOBAL_INFO_PAIRWISE_ENC_ALG(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AOAC_GLOBAL_INFO_GROUP_ENC_ALG(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

/* AOAC_RSVD_PAGE_0x83 */
static inline void SET_H2CCMD_AOAC_RSVD_PAGE_REMOTE_WAKE_CTRL_INFO(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 0, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AOAC_RSVD_PAGE_ARP_RSP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AOAC_RSVD_PAGE_NEIGHBOR_ADV(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 2, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AOAC_RSVD_PAGE_GTK_RSP(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 3, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AOAC_RSVD_PAGE_GTK_INFO(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 4, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AOAC_RSVD_PAGE_GTK_EXT_MEM(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 5, __Value, GENMASK(7, 0));
}

static inline void SET_H2CCMD_AOAC_RSVD_PAGE_NDP_INFO(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 6, __Value, GENMASK(7, 0));
}

/* AOAC_RSVDPAGE_2_0x84 */

/* AOAC_RSVDPAGE_3_0x88 */
static inline void SET_H2CCMD_AOAC_RSVD_PAGE_AOAC_REPORT(u8 *__pH2CCmd, u8 __Value)
{
	u8p_replace_bits(__pH2CCmd + 1, __Value, GENMASK(7, 0));
}

/* ---------------------------------------------------------------------------------------------------------
 * -------------------------------------------    Structure    --------------------------------------------------
 * --------------------------------------------------------------------------------------------------------- */
struct rsvd_page {
	u8 LocProbeRsp;
	u8 LocPsPoll;
	u8 LocNullData;
	u8 LocQosNull;
	u8 LocBTQosNull;
	u8 LocApOffloadBCN;
};

void dump_TX_FIFO(struct adapter * adapt, u8 page_num, u16 page_size);
u8 rtw_hal_set_fw_media_status_cmd(struct adapter *adapter, u8 mstatus, u8 macid);

#endif

