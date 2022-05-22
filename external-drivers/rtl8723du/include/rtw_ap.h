/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_AP_H_
#define __RTW_AP_H_

/* external function */
extern void rtw_indicate_sta_assoc_event(struct adapter *adapt, struct sta_info *psta);
extern void rtw_indicate_sta_disassoc_event(struct adapter *adapt, struct sta_info *psta);

void init_mlme_ap_info(struct adapter *adapt);
void free_mlme_ap_info(struct adapter *adapt);
u8 rtw_set_tim_ie(u8 dtim_cnt, u8 dtim_period
	, const u8 *tim_bmp, u8 tim_bmp_len, u8 *tim_ie);
/* void update_BCNTIM(struct adapter *adapt); */
void rtw_add_bcn_ie(struct adapter *adapt, struct wlan_bssid_ex *pnetwork, u8 index, u8 *data, u8 len);
void rtw_remove_bcn_ie(struct adapter *adapt, struct wlan_bssid_ex *pnetwork, u8 index);
void _update_beacon(struct adapter *adapt, u8 ie_id, u8 *oui, u8 tx, const char *tag);
#define update_beacon(adapter, ie_id, oui, tx) _update_beacon((adapter), (ie_id), (oui), (tx), __func__)

void rtw_ap_update_sta_ra_info(struct adapter *adapt, struct sta_info *psta);

void expire_timeout_chk(struct adapter *adapt);
void update_sta_info_apmode(struct adapter *adapt, struct sta_info *psta);
void rtw_start_bss_hdl_after_chbw_decided(struct adapter *adapter);
void start_bss_network(struct adapter *adapt, struct createbss_parm *parm);
int rtw_check_beacon_data(struct adapter *adapt, u8 *pbuf,  int len);
void rtw_ap_restore_network(struct adapter *adapt);

#if CONFIG_RTW_MACADDR_ACL
void rtw_set_macaddr_acl(struct adapter *adapter, int mode);
int rtw_acl_add_sta(struct adapter *adapter, const u8 *addr);
int rtw_acl_remove_sta(struct adapter *adapter, const u8 *addr);
#endif /* CONFIG_RTW_MACADDR_ACL */

u8 rtw_ap_set_pairwise_key(struct adapter *adapt, struct sta_info *psta);
int rtw_ap_set_group_key(struct adapter *adapt, u8 *key, u8 alg, int keyid);
int rtw_ap_set_wep_key(struct adapter *adapt, u8 *key, u8 keylen, int keyid, u8 set_tx);

void associated_clients_update(struct adapter *adapt, u8 updated, u32 sta_info_type);
void bss_cap_update_on_sta_join(struct adapter *adapt, struct sta_info *psta);
u8 bss_cap_update_on_sta_leave(struct adapter *adapt, struct sta_info *psta);
void sta_info_update(struct adapter *adapt, struct sta_info *psta);
void ap_sta_info_defer_update(struct adapter *adapt, struct sta_info *psta);
u8 ap_free_sta(struct adapter *adapt, struct sta_info *psta, bool active, u16 reason, bool enqueue);
int rtw_sta_flush(struct adapter *adapt, bool enqueue);
int rtw_ap_inform_ch_switch(struct adapter *adapt, u8 new_ch, u8 ch_offset);
void start_ap_mode(struct adapter *adapt);
void stop_ap_mode(struct adapter *adapt);

void rtw_ap_update_bss_chbw(struct adapter *adapter, struct wlan_bssid_ex *bss, u8 ch, u8 bw, u8 offset);
bool rtw_ap_chbw_decision(struct adapter *adapter, s16 req_ch, s8 req_bw, s8 req_offset, u8 *ch, u8 *bw, u8 *offset, u8 *chbw_allow);

void rtw_ap_parse_sta_capability(struct adapter *adapter, struct sta_info *sta, u8 *cap);
u16 rtw_ap_parse_sta_supported_rates(struct adapter *adapter, struct sta_info *sta, u8 *tlv_ies, u16 tlv_ies_len);
u16 rtw_ap_parse_sta_security_ie(struct adapter *adapter, struct sta_info *sta, struct rtw_ieee802_11_elems *elems);
void rtw_ap_parse_sta_wmm_ie(struct adapter *adapter, struct sta_info *sta, u8 *tlv_ies, u16 tlv_ies_len);
void rtw_ap_parse_sta_ht_ie(struct adapter *adapter, struct sta_info *sta, struct rtw_ieee802_11_elems *elems);
void rtw_ap_parse_sta_vht_ie(struct adapter *adapter, struct sta_info *sta, struct rtw_ieee802_11_elems *elems);

void update_bmc_sta(struct adapter *adapt);

#ifdef CONFIG_BMC_TX_RATE_SELECT
void rtw_update_bmc_sta_tx_rate(struct adapter *adapter);
#endif

void rtw_process_ht_action_smps(struct adapter *adapt, u8 *ta, u8 ctrl_field);
void rtw_process_public_act_bsscoex(struct adapter *adapt, u8 *pframe, uint frame_len);
int rtw_ht_operation_update(struct adapter *adapt);
u8 rtw_ap_sta_linking_state_check(struct adapter *adapter);

#endif /*__RTW_AP_H_*/
