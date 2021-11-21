/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_P2P_H_
#define __RTW_P2P_H_


u32 build_beacon_p2p_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_probe_resp_p2p_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_prov_disc_request_p2p_ie(struct wifidirect_info *pwdinfo, u8 *pbuf, u8 *pssid, u8 ussidlen, u8 *pdev_raddr);
u32 build_assoc_resp_p2p_ie(struct wifidirect_info *pwdinfo, u8 *pbuf, u8 status_code);
u32 build_deauth_p2p_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
int rtw_init_wifi_display_info(struct adapter *adapt);
void rtw_wfd_enable(struct adapter *adapter, bool on);
void rtw_wfd_set_ctrl_port(struct adapter *adapter, u16 port);
void rtw_tdls_wfd_enable(struct adapter *adapter, bool on);

u32 build_probe_req_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_probe_resp_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf, u8 tunneled);
u32 build_beacon_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_nego_req_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_nego_resp_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_nego_confirm_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_invitation_req_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_invitation_resp_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_assoc_req_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_assoc_resp_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_provdisc_req_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);
u32 build_provdisc_resp_wfd_ie(struct wifidirect_info *pwdinfo, u8 *pbuf);

u32 rtw_append_beacon_wfd_ie(struct adapter *adapter, u8 *pbuf);
u32 rtw_append_probe_req_wfd_ie(struct adapter *adapter, u8 *pbuf);
u32 rtw_append_probe_resp_wfd_ie(struct adapter *adapter, u8 *pbuf);
u32 rtw_append_assoc_req_wfd_ie(struct adapter *adapter, u8 *pbuf);
u32 rtw_append_assoc_resp_wfd_ie(struct adapter *adapter, u8 *pbuf);

void rtw_xframe_chk_wfd_ie(struct xmit_frame *xframe);

u32 process_probe_req_p2p_ie(struct wifidirect_info *pwdinfo, u8 *pframe, uint len);
u32 process_assoc_req_p2p_ie(struct wifidirect_info *pwdinfo, u8 *pframe, uint len, struct sta_info *psta);
u32 process_p2p_devdisc_req(struct wifidirect_info *pwdinfo, u8 *pframe, uint len);
u32 process_p2p_devdisc_resp(struct wifidirect_info *pwdinfo, u8 *pframe, uint len);
u8 process_p2p_provdisc_req(struct wifidirect_info *pwdinfo,  u8 *pframe, uint len);
u8 process_p2p_provdisc_resp(struct wifidirect_info *pwdinfo,  u8 *pframe);
u8 process_p2p_group_negotation_req(struct wifidirect_info *pwdinfo, u8 *pframe, uint len);
u8 process_p2p_group_negotation_resp(struct wifidirect_info *pwdinfo, u8 *pframe, uint len);
u8 process_p2p_group_negotation_confirm(struct wifidirect_info *pwdinfo, u8 *pframe, uint len);
u8 process_p2p_presence_req(struct wifidirect_info *pwdinfo, u8 *pframe, uint len);
int process_p2p_cross_connect_ie(struct adapter * adapt, u8 *IEs, u32 IELength);

int p2p_protocol_wk_hdl(struct adapter *adapt, int intCmdType, u8 *buf);

void	process_p2p_ps_ie(struct adapter * adapt, u8 *IEs, u32 IELength);
void	p2p_ps_wk_hdl(struct adapter *adapt, u8 p2p_ps_state);
u8	p2p_ps_wk_cmd(struct adapter *adapt, u8 p2p_ps_state, u8 enqueue);

u8 roch_stay_in_cur_chan(struct adapter *adapt);
void rtw_init_cfg80211_wifidirect_info(struct adapter	*adapt);
int rtw_p2p_check_frames(struct adapter *adapt, const u8 *buf, u32 len, u8 tx);
void reset_global_wifidirect_info(struct adapter *adapt);
void rtw_init_wifidirect_timers(struct adapter *adapt);
void rtw_init_wifidirect_addrs(struct adapter *adapt, u8 *dev_addr, u8 *iface_addr);
void init_wifidirect_info(struct adapter *adapt, enum P2P_ROLE role);
int rtw_p2p_enable(struct adapter *adapt, enum P2P_ROLE role);

static inline void _rtw_p2p_set_state(struct wifidirect_info *wdinfo, enum P2P_STATE state)
{
	if (wdinfo->p2p_state != state) {
		/* wdinfo->pre_p2p_state = wdinfo->p2p_state; */
		wdinfo->p2p_state = state;
	}
}
static inline void _rtw_p2p_set_pre_state(struct wifidirect_info *wdinfo, enum P2P_STATE state)
{
	if (wdinfo->pre_p2p_state != state)
		wdinfo->pre_p2p_state = state;
}

static inline void _rtw_p2p_set_role(struct wifidirect_info *wdinfo, enum P2P_ROLE role)
{
	if (wdinfo->role != role)
		wdinfo->role = role;
}
static inline int _rtw_p2p_state(struct wifidirect_info *wdinfo)
{
	return wdinfo->p2p_state;
}
static inline int _rtw_p2p_pre_state(struct wifidirect_info *wdinfo)
{
	return wdinfo->pre_p2p_state;
}
static inline int _rtw_p2p_role(struct wifidirect_info *wdinfo)
{
	return wdinfo->role;
}
static inline bool _rtw_p2p_chk_state(struct wifidirect_info *wdinfo, enum P2P_STATE state)
{
	return wdinfo->p2p_state == state;
}
static inline bool _rtw_p2p_chk_role(struct wifidirect_info *wdinfo, enum P2P_ROLE role)
{
	return wdinfo->role == role;
}

#define rtw_p2p_set_state(wdinfo, state) _rtw_p2p_set_state(wdinfo, state)
#define rtw_p2p_set_pre_state(wdinfo, state) _rtw_p2p_set_pre_state(wdinfo, state)
#define rtw_p2p_set_role(wdinfo, role) _rtw_p2p_set_role(wdinfo, role)

#define rtw_p2p_state(wdinfo) _rtw_p2p_state(wdinfo)
#define rtw_p2p_pre_state(wdinfo) _rtw_p2p_pre_state(wdinfo)
#define rtw_p2p_role(wdinfo) _rtw_p2p_role(wdinfo)
#define rtw_p2p_chk_state(wdinfo, state) _rtw_p2p_chk_state(wdinfo, state)
#define rtw_p2p_chk_role(wdinfo, role) _rtw_p2p_chk_role(wdinfo, role)

#define rtw_p2p_findphase_ex_set(wdinfo, value) \
	(wdinfo)->find_phase_state_exchange_cnt = (value)

/* is this find phase exchange for social channel scan? */
#define rtw_p2p_findphase_ex_is_social(wdinfo)   \
	(wdinfo)->find_phase_state_exchange_cnt >= P2P_FINDPHASE_EX_SOCIAL_FIRST

/* should we need find phase exchange anymore? */
#define rtw_p2p_findphase_ex_is_needed(wdinfo) \
	((wdinfo)->find_phase_state_exchange_cnt < P2P_FINDPHASE_EX_MAX && \
	 (wdinfo)->find_phase_state_exchange_cnt != P2P_FINDPHASE_EX_NONE && \
	 !(wdinfo)->rx_invitereq_info.scan_op_ch_only && \
	 !(wdinfo)->p2p_info.scan_op_ch_only)

#endif
