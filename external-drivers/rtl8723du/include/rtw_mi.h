/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_MI_H_
#define __RTW_MI_H_

void rtw_mi_update_union_chan_inf(struct adapter *adapter, u8 ch, u8 offset , u8 bw);
u8 rtw_mi_stayin_union_ch_chk(struct adapter *adapter);
u8 rtw_mi_stayin_union_band_chk(struct adapter *adapter);
int rtw_mi_get_ch_setting_union(struct adapter *adapter, u8 *ch, u8 *bw, u8 *offset);
int rtw_mi_get_ch_setting_union_no_self(struct adapter *adapter, u8 *ch, u8 *bw, u8 *offset);

struct mi_state {
	u8 sta_num;			/* WIFI_STATION_STATE */
	u8 ld_sta_num;		/* WIFI_STATION_STATE && _FW_LINKED */
	u8 lg_sta_num;		/* WIFI_STATION_STATE && _FW_UNDER_LINKING */
	u8 ap_num;			/* WIFI_AP_STATE && _FW_LINKED */
	u8 starting_ap_num;	/*WIFI_FW_AP_STATE*/
	u8 ld_ap_num;		/* WIFI_AP_STATE && _FW_LINKED && asoc_sta_count > 2 */
	u8 adhoc_num;		/* (WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE) && _FW_LINKED */
	u8 ld_adhoc_num;	/* (WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE) && _FW_LINKED && asoc_sta_count > 2 */
#ifdef CONFIG_RTW_MESH
	u8 mesh_num;		/* WIFI_MESH_STATE &&  _FW_LINKED */
	u8 ld_mesh_num;		/* WIFI_MESH_STATE &&  _FW_LINKED && asoc_sta_count > 2 */
#endif
	u8 scan_num;		/* WIFI_SITE_MONITOR */
	u8 scan_enter_num;	/* WIFI_SITE_MONITOR && !SCAN_DISABLE && !SCAN_BACK_OP */
	u8 uwps_num;		/* WIFI_UNDER_WPS */
	u8 roch_num;
	u8 mgmt_tx_num;
	u8 union_ch;
	u8 union_bw;
	u8 union_offset;
};

#define MSTATE_STA_NUM(_mstate)			((_mstate)->sta_num)
#define MSTATE_STA_LD_NUM(_mstate)		((_mstate)->ld_sta_num)
#define MSTATE_STA_LG_NUM(_mstate)		((_mstate)->lg_sta_num)

#define MSTATE_TDLS_LD_NUM(_mstate)		0

#define MSTATE_AP_NUM(_mstate)			((_mstate)->ap_num)
#define MSTATE_AP_STARTING_NUM(_mstate)	((_mstate)->starting_ap_num)
#define MSTATE_AP_LD_NUM(_mstate)		((_mstate)->ld_ap_num)

#define MSTATE_ADHOC_NUM(_mstate)		((_mstate)->adhoc_num)
#define MSTATE_ADHOC_LD_NUM(_mstate)	((_mstate)->ld_adhoc_num)

#ifdef CONFIG_RTW_MESH
#define MSTATE_MESH_NUM(_mstate)		((_mstate)->mesh_num)
#define MSTATE_MESH_LD_NUM(_mstate)		((_mstate)->ld_mesh_num)
#else
#define MSTATE_MESH_NUM(_mstate)		0
#define MSTATE_MESH_LD_NUM(_mstate)		0
#endif

#define MSTATE_SCAN_NUM(_mstate)		((_mstate)->scan_num)
#define MSTATE_SCAN_ENTER_NUM(_mstate)	((_mstate)->scan_enter_num)
#define MSTATE_WPS_NUM(_mstate)			((_mstate)->uwps_num)

#define MSTATE_ROCH_NUM(_mstate)		((_mstate)->roch_num)
#define MSTATE_MGMT_TX_NUM(_mstate)		((_mstate)->mgmt_tx_num)
#define MSTATE_U_CH(_mstate)			((_mstate)->union_ch)
#define MSTATE_U_BW(_mstate)			((_mstate)->union_bw)
#define MSTATE_U_OFFSET(_mstate)		((_mstate)->union_offset)

#define rtw_mi_get_union_chan(adapter)	adapter_to_dvobj(adapter)->iface_state.union_ch
#define rtw_mi_get_union_bw(adapter)		adapter_to_dvobj(adapter)->iface_state.union_bw
#define rtw_mi_get_union_offset(adapter)	adapter_to_dvobj(adapter)->iface_state.union_offset

#define rtw_mi_get_assoced_sta_num(adapter)	DEV_STA_LD_NUM(adapter_to_dvobj(adapter))
#define rtw_mi_get_ap_num(adapter)			DEV_AP_NUM(adapter_to_dvobj(adapter))
#define rtw_mi_get_mesh_num(adapter)		DEV_MESH_NUM(adapter_to_dvobj(adapter))

/* For now, not return union_ch/bw/offset */
void rtw_mi_status(struct adapter *adapter, struct mi_state *mstate);
void rtw_mi_status_no_self(struct adapter *adapter, struct mi_state *mstate);
void rtw_mi_status_no_others(struct adapter *adapter, struct mi_state *mstate);

/* For now, not handle union_ch/bw/offset */
void rtw_mi_status_merge(struct mi_state *d, struct mi_state *a);

void rtw_mi_update_iface_status(struct mlme_priv *pmlmepriv, int state);

u8 rtw_mi_netif_stop_queue(struct adapter *adapt);
u8 rtw_mi_buddy_netif_stop_queue(struct adapter *adapt);

u8 rtw_mi_netif_wake_queue(struct adapter *adapt);
u8 rtw_mi_buddy_netif_wake_queue(struct adapter *adapt);

u8 rtw_mi_netif_carrier_on(struct adapter *adapt);
u8 rtw_mi_buddy_netif_carrier_on(struct adapter *adapt);
u8 rtw_mi_netif_carrier_off(struct adapter *adapt);
u8 rtw_mi_buddy_netif_carrier_off(struct adapter *adapt);

u8 rtw_mi_netif_caroff_qstop(struct adapter *adapt);
u8 rtw_mi_buddy_netif_caroff_qstop(struct adapter *adapt);
u8 rtw_mi_netif_caron_qstart(struct adapter *adapt);
u8 rtw_mi_buddy_netif_caron_qstart(struct adapter *adapt);

void rtw_mi_scan_abort(struct adapter *adapter, bool bwait);
void rtw_mi_buddy_scan_abort(struct adapter *adapter, bool bwait);
u32 rtw_mi_start_drv_threads(struct adapter *adapter);
u32 rtw_mi_buddy_start_drv_threads(struct adapter *adapter);
void rtw_mi_stop_drv_threads(struct adapter *adapter);
void rtw_mi_buddy_stop_drv_threads(struct adapter *adapter);
void rtw_mi_cancel_all_timer(struct adapter *adapter);
void rtw_mi_buddy_cancel_all_timer(struct adapter *adapter);
void rtw_mi_reset_drv_sw(struct adapter *adapter);
void rtw_mi_buddy_reset_drv_sw(struct adapter *adapter);

extern void rtw_intf_start(struct adapter *adapter);
extern void rtw_intf_stop(struct adapter *adapter);
void rtw_mi_intf_start(struct adapter *adapter);
void rtw_mi_buddy_intf_start(struct adapter *adapter);
void rtw_mi_intf_stop(struct adapter *adapter);
void rtw_mi_buddy_intf_stop(struct adapter *adapter);

void rtw_mi_suspend_free_assoc_resource(struct adapter *adapter);
void rtw_mi_buddy_suspend_free_assoc_resource(struct adapter *adapter);

void rtw_mi_set_scan_deny(struct adapter *adapter, u32 ms);
void rtw_mi_buddy_set_scan_deny(struct adapter *adapter, u32 ms);

u8 rtw_mi_is_scan_deny(struct adapter *adapter);
u8 rtw_mi_buddy_is_scan_deny(struct adapter *adapter);

void rtw_mi_beacon_update(struct adapter *adapt);
void rtw_mi_buddy_beacon_update(struct adapter *adapt);

void rtw_mi_hal_dump_macaddr(struct adapter *adapt);
void rtw_mi_buddy_hal_dump_macaddr(struct adapter *adapt);

u8 rtw_mi_busy_traffic_check(struct adapter *adapt, bool check_sc_interval);
u8 rtw_mi_buddy_busy_traffic_check(struct adapter *adapt, bool check_sc_interval);

u8 rtw_mi_check_mlmeinfo_state(struct adapter *adapt, u32 state);
u8 rtw_mi_buddy_check_mlmeinfo_state(struct adapter *adapt, u32 state);

u8 rtw_mi_check_fwstate(struct adapter *adapt, int state);
u8 rtw_mi_buddy_check_fwstate(struct adapter *adapt, int state);
enum {
	MI_LINKED,
	MI_ASSOC,
	MI_UNDER_WPS,
	MI_AP_MODE,
	MI_AP_ASSOC,
	MI_ADHOC,
	MI_ADHOC_ASSOC,
	MI_MESH,
	MI_MESH_ASSOC,
	MI_STA_NOLINK, /* this is misleading, but not used now */
	MI_STA_LINKED,
	MI_STA_LINKING,
};
u8 rtw_mi_check_status(struct adapter *adapter, u8 type);

void dump_dvobj_mi_status(void *sel, const char *fun_name, struct adapter *adapter);
void dump_mi_status(void *sel, struct dvobj_priv *dvobj);

u8 rtw_mi_traffic_statistics(struct adapter *adapt);
u8 rtw_mi_check_miracast_enabled(struct adapter *adapt);

void rtw_mi_adapter_reset(struct adapter *adapt);
void rtw_mi_buddy_adapter_reset(struct adapter *adapt);

u8 rtw_mi_dynamic_check_timer_handlder(struct adapter *adapt);
u8 rtw_mi_buddy_dynamic_check_timer_handlder(struct adapter *adapt);

u8 rtw_mi_dev_unload(struct adapter *adapt);
u8 rtw_mi_buddy_dev_unload(struct adapter *adapt);

extern void rtw_iface_dynamic_chk_wk_hdl(struct adapter *adapt);
u8 rtw_mi_dynamic_chk_wk_hdl(struct adapter *adapt);
u8 rtw_mi_buddy_dynamic_chk_wk_hdl(struct adapter *adapt);

u8 rtw_mi_os_xmit_schedule(struct adapter *adapt);
u8 rtw_mi_buddy_os_xmit_schedule(struct adapter *adapt);

u8 rtw_mi_report_survey_event(struct adapter *adapt, union recv_frame *precv_frame);
u8 rtw_mi_buddy_report_survey_event(struct adapter *adapt, union recv_frame *precv_frame);

extern void sreset_start_adapter(struct adapter *adapt);
extern void sreset_stop_adapter(struct adapter *adapt);
u8 rtw_mi_sreset_adapter_hdl(struct adapter *adapt, u8 bstart);
u8 rtw_mi_buddy_sreset_adapter_hdl(struct adapter *adapt, u8 bstart);

u8 rtw_mi_tx_beacon_hdl(struct adapter *adapt);
u8 rtw_mi_buddy_tx_beacon_hdl(struct adapter *adapt);

u8 rtw_mi_set_tx_beacon_cmd(struct adapter *adapt);
u8 rtw_mi_buddy_set_tx_beacon_cmd(struct adapter *adapt);

u8 rtw_mi_p2p_chk_state(struct adapter *adapt, enum P2P_STATE p2p_state);
u8 rtw_mi_buddy_p2p_chk_state(struct adapter *adapt, enum P2P_STATE p2p_state);
u8 rtw_mi_stay_in_p2p_mode(struct adapter *adapt);
u8 rtw_mi_buddy_stay_in_p2p_mode(struct adapter *adapt);

struct adapter *rtw_get_iface_by_id(struct adapter *adapt, u8 iface_id);
struct adapter *rtw_get_iface_by_macddr(struct adapter *adapt, u8 *mac_addr);
struct adapter *rtw_get_iface_by_hwport(struct adapter *adapt, u8 hw_port);

void rtw_mi_buddy_clone_bcmc_packet(struct adapter *adapt, union recv_frame *precvframe, u8 *pphy_status);

void rtw_mi_update_ap_bmc_camid(struct adapter *adapt, u8 camid_a, u8 camid_b);

#endif /*__RTW_MI_H_*/
