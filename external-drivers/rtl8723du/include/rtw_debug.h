/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_DEBUG_H__
#define __RTW_DEBUG_H__

/* driver log level*/
enum {
	_DRV_NONE_ = 0,
	_DRV_ALWAYS_ = 1,
	_DRV_ERR_ = 2,
	_DRV_WARNING_ = 3,
	_DRV_INFO_ = 4,
	_DRV_DEBUG_ = 5,
	_DRV_MAX_ = 6
};

#define DRIVER_PREFIX "RTW: "

	#define RTW_PRINT(x, ...) do {} while (0)
	#define RTW_ERR(x, ...) do {} while (0)
	#define RTW_WARN(x,...) do {} while (0)
	#define RTW_INFO(x,...) do {} while (0)
	#define RTW_DBG(x,...) do {} while (0)
	#define RTW_PRINT_SEL(x,...) do {} while (0)
	#define _RTW_PRINT(x, ...) do {} while (0)
	#define _RTW_ERR(x, ...) do {} while (0)
	#define _RTW_WARN(x,...) do {} while (0)
	#define _RTW_INFO(x,...) do {} while (0)
	#define _RTW_DBG(x,...) do {} while (0)
	#define _RTW_PRINT_SEL(x,...) do {} while (0)

#define RTW_INFO_DUMP(_TitleString, _HexData, _HexDataLen) do {} while (0)
#define RTW_DBG_DUMP(_TitleString, _HexData, _HexDataLen) do {} while (0)
#define RTW_PRINT_DUMP(_TitleString, _HexData, _HexDataLen) do {} while (0)

#define RTW_DBG_EXPR(EXPR) do {} while (0)

#define RTW_DBGDUMP NULL /* 'stream' for _dbgdump */

/* don't use these 3 APIs anymore, will be removed later */
#define RT_TRACE(_Comp, _Level, Fmt) do {} while (0)


#undef _dbgdump
#undef _seqdump

	#define _dbgdump printk
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24))
	#define KERN_CONT
	#endif
	#define _seqdump seq_printf

void RTW_BUF_DUMP_SEL(uint _loglevel, void *sel, u8 *_titlestring,
								bool _idx_show, const u8 *_hexdata, int _hexdatalen);

#ifdef CONFIG_RTW_DEBUG

extern uint rtw_drv_log_level;

#if defined(_dbgdump)

/* with driver-defined prefix */
#undef RTW_PRINT
#define RTW_PRINT(fmt, arg...)     \
	do {\
		if (_DRV_ALWAYS_ <= rtw_drv_log_level) {\
			_dbgdump(DRIVER_PREFIX fmt, ##arg);\
		} \
	} while (0)

#undef RTW_ERR
#define RTW_ERR(fmt, arg...)     \
	do {\
		if (_DRV_ERR_ <= rtw_drv_log_level) {\
			_dbgdump(DRIVER_PREFIX"ERROR " fmt, ##arg);\
		} \
	} while (0)


#undef RTW_WARN
#define RTW_WARN(fmt, arg...)     \
	do {\
		if (_DRV_WARNING_ <= rtw_drv_log_level) {\
			_dbgdump(DRIVER_PREFIX"WARN " fmt, ##arg);\
		} \
	} while (0)

#undef RTW_INFO
#define RTW_INFO(fmt, arg...)     \
	do {\
		if (_DRV_INFO_ <= rtw_drv_log_level) {\
			_dbgdump(DRIVER_PREFIX fmt, ##arg);\
		} \
	} while (0)


#undef RTW_DBG
#define RTW_DBG(fmt, arg...)     \
	do {\
		if (_DRV_DEBUG_ <= rtw_drv_log_level) {\
			_dbgdump(DRIVER_PREFIX fmt, ##arg);\
		} \
	} while (0)

#undef RTW_INFO_DUMP
#define RTW_INFO_DUMP(_TitleString, _HexData, _HexDataLen)	\
	RTW_BUF_DUMP_SEL(_DRV_INFO_, RTW_DBGDUMP, _TitleString, false, _HexData, _HexDataLen)

#undef RTW_DBG_DUMP
#define RTW_DBG_DUMP(_TitleString, _HexData, _HexDataLen)	\
	RTW_BUF_DUMP_SEL(_DRV_DEBUG_, RTW_DBGDUMP, _TitleString, false, _HexData, _HexDataLen)


#undef RTW_PRINT_DUMP
#define RTW_PRINT_DUMP(_TitleString, _HexData, _HexDataLen)	\
	RTW_BUF_DUMP_SEL(_DRV_INFO_, RTW_DBGDUMP, _TitleString, false, _HexData, _HexDataLen)

/* without driver-defined prefix */
#undef _RTW_PRINT
#define _RTW_PRINT(fmt, arg...)     \
	do {\
		if (_DRV_INFO_ <= rtw_drv_log_level) {\
			_dbgdump(KERN_CONT fmt, ##arg);\
		} \
	} while (0)

#undef _RTW_ERR
#define _RTW_ERR(fmt, arg...)     \
	do {\
		if (_DRV_ERR_ <= rtw_drv_log_level) {\
			_dbgdump(KERN_CONT fmt, ##arg);\
		} \
	} while (0)


#undef _RTW_WARN
#define _RTW_WARN(fmt, arg...)     \
	do {\
		if (_DRV_WARNING_ <= rtw_drv_log_level) {\
			_dbgdump(KERN_CONT fmt, ##arg);\
		} \
	} while (0)

#undef _RTW_INFO
#define _RTW_INFO(fmt, arg...)     \
	do {\
		if (_DRV_INFO_ <= rtw_drv_log_level) {\
			_dbgdump(KERN_CONT fmt, ##arg);\
		} \
	} while (0)

#undef _RTW_DBG
#define _RTW_DBG(fmt, arg...)     \
	do {\
		if (_DRV_DEBUG_ <= rtw_drv_log_level) {\
			_dbgdump(KERN_CONT fmt, ##arg);\
		} \
	} while (0)


/* other debug APIs */
#undef RTW_DBG_EXPR
#define RTW_DBG_EXPR(EXPR) do { if (_DRV_DEBUG_ <= rtw_drv_log_level) EXPR; } while (0)

#endif /* defined(_dbgdump) */
#endif /* CONFIG_RTW_DEBUG */


#if defined(_seqdump)
/* dump message to selected 'stream' with driver-defined prefix */
#undef RTW_PRINT_SEL
#define RTW_PRINT_SEL(sel, fmt, arg...) \
	do {\
		if (sel == RTW_DBGDUMP)\
			RTW_PRINT(fmt, ##arg); \
		else {\
			_seqdump(sel, fmt, ##arg) /*rtw_warn_on(1)*/; \
		} \
	} while (0)

/* dump message to selected 'stream' */
#undef _RTW_PRINT_SEL
#define _RTW_PRINT_SEL(sel, fmt, arg...) \
	do {\
		if (sel == RTW_DBGDUMP)\
			_RTW_PRINT(fmt, ##arg); \
		else {\
			_seqdump(sel, fmt, ##arg) /*rtw_warn_on(1)*/; \
		} \
	} while (0)

/* dump message to selected 'stream' */
#undef RTW_DUMP_SEL
#define RTW_DUMP_SEL(sel, _HexData, _HexDataLen) \
	RTW_BUF_DUMP_SEL(_DRV_INFO_, sel, NULL, false, _HexData, _HexDataLen)

#define RTW_MAP_DUMP_SEL(sel, _TitleString, _HexData, _HexDataLen) \
	RTW_BUF_DUMP_SEL(_DRV_INFO_, sel, _TitleString, true, _HexData, _HexDataLen)
#endif /* defined(_seqdump) */


#ifdef CONFIG_DBG_COUNTER
	#define DBG_COUNTER(counter) counter++
#else
	#define DBG_COUNTER(counter)
#endif

void dump_drv_version(void *sel);
void dump_log_level(void *sel);
void dump_drv_cfg(struct seq_file *sel);

void mac_reg_dump(void *sel, struct adapter *adapter);
void bb_reg_dump(void *sel, struct adapter *adapter);
void bb_reg_dump_ex(void *sel, struct adapter *adapter);
void rf_reg_dump(void *sel, struct adapter *adapter);

void rtw_sink_rtp_seq_dbg(struct adapter *adapter, u8 *ehdr_pos);

struct sta_info;
void sta_rx_reorder_ctl_dump(void *sel, struct sta_info *sta);

struct dvobj_priv;
void dump_tx_rate_bmp(void *sel, struct dvobj_priv *dvobj);
void dump_adapters_status(void *sel, struct dvobj_priv *dvobj);

struct sec_cam_ent;
void dump_sec_cam_ent(void *sel, struct sec_cam_ent *ent, int id);
void dump_sec_cam_ent_title(void *sel, u8 has_id);
void dump_sec_cam(void *sel, struct adapter *adapter);
void dump_sec_cam_cache(void *sel, struct adapter *adapter);

#ifdef CONFIG_PROC_DEBUG
ssize_t proc_set_write_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_read_reg(struct seq_file *m, void *v);
ssize_t proc_set_read_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_fwstate(struct seq_file *m, void *v);
int proc_get_sec_info(struct seq_file *m, void *v);
int proc_get_mlmext_state(struct seq_file *m, void *v);
int proc_get_roam_flags(struct seq_file *m, void *v);
ssize_t proc_set_roam_flags(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_roam_param(struct seq_file *m, void *v);
ssize_t proc_set_roam_param(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
ssize_t proc_set_roam_tgt_addr(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
#ifdef CONFIG_RTW_80211R
int proc_get_ft_flags(struct seq_file *m, void *v);
ssize_t proc_set_ft_flags(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
#endif
int proc_get_qos_option(struct seq_file *m, void *v);
int proc_get_ht_option(struct seq_file *m, void *v);
int proc_get_rf_info(struct seq_file *m, void *v);
int proc_get_scan_param(struct seq_file *m, void *v);
ssize_t proc_set_scan_param(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_scan_abort(struct seq_file *m, void *v);
int proc_get_backop_flags_sta(struct seq_file *m, void *v);
ssize_t proc_set_backop_flags_sta(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_backop_flags_ap(struct seq_file *m, void *v);
ssize_t proc_set_backop_flags_ap(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_survey_info(struct seq_file *m, void *v);
ssize_t proc_set_survey_info(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_ap_info(struct seq_file *m, void *v);
ssize_t proc_reset_trx_info(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_trx_info(struct seq_file *m, void *v);
ssize_t proc_set_tx_power_offset(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_tx_power_offset(struct seq_file *m, void *v);
int proc_get_rate_ctl(struct seq_file *m, void *v);
int proc_get_wifi_spec(struct seq_file *m, void *v);
ssize_t proc_set_rate_ctl(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_bw_ctl(struct seq_file *m, void *v);
ssize_t proc_set_bw_ctl(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_bmc_tx_rate(struct seq_file *m, void *v);
ssize_t proc_set_bmc_tx_rate(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_dis_pwt(struct seq_file *m, void *v);
ssize_t proc_set_dis_pwt(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_ps_dbg_info(struct seq_file *m, void *v);
ssize_t proc_set_ps_dbg_info(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

bool rtw_fwdl_test_trigger_chksum_fail(void);
bool rtw_fwdl_test_trigger_wintint_rdy_fail(void);
ssize_t proc_set_fwdl_test_case(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
bool rtw_del_rx_ampdu_test_trigger_no_tx_fail(void);
ssize_t proc_set_del_rx_ampdu_test_case(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
u32 rtw_get_wait_hiq_empty_ms(void);
ssize_t proc_set_wait_hiq_empty(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
void rtw_sta_linking_test_set_start(void);
bool rtw_sta_linking_test_wait_done(void);
bool rtw_sta_linking_test_force_fail(void);
ssize_t proc_set_sta_linking_test(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_rx_stat(struct seq_file *m, void *v);
int proc_get_tx_stat(struct seq_file *m, void *v);
int proc_get_all_sta_info(struct seq_file *m, void *v);

int proc_get_trx_info_debug(struct seq_file *m, void *v);

int proc_get_rx_signal(struct seq_file *m, void *v);
ssize_t proc_set_rx_signal(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_hw_status(struct seq_file *m, void *v);
ssize_t proc_set_hw_status(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_ht_enable(struct seq_file *m, void *v);
ssize_t proc_set_ht_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_bw_mode(struct seq_file *m, void *v);
ssize_t proc_set_bw_mode(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_ampdu_enable(struct seq_file *m, void *v);
ssize_t proc_set_ampdu_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_mac_rptbuf(struct seq_file *m, void *v);

void dump_regsty_rx_ampdu_size_limit(void *sel, struct adapter *adapter);
int proc_get_rx_ampdu(struct seq_file *m, void *v);
ssize_t proc_set_rx_ampdu(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

void rtw_dump_dft_phy_cap(void *sel, struct adapter *adapter);
void rtw_get_dft_phy_cap(void *sel, struct adapter *adapter);
void rtw_dump_drv_phy_cap(void *sel, struct adapter *adapter);

int proc_get_rx_stbc(struct seq_file *m, void *v);
ssize_t proc_set_rx_stbc(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_stbc_cap(struct seq_file *m, void *v);
ssize_t proc_set_stbc_cap(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_ldpc_cap(struct seq_file *m, void *v);
ssize_t proc_set_ldpc_cap(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_rx_ampdu_factor(struct seq_file *m, void *v);
ssize_t proc_set_rx_ampdu_factor(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_tx_max_agg_num(struct seq_file *m, void *v);
ssize_t proc_set_tx_max_agg_num(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_rx_ampdu_density(struct seq_file *m, void *v);
ssize_t proc_set_rx_ampdu_density(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_tx_ampdu_density(struct seq_file *m, void *v);
ssize_t proc_set_tx_ampdu_density(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

#ifdef CONFIG_TX_AMSDU
int proc_get_tx_amsdu(struct seq_file *m, void *v);
ssize_t proc_set_tx_amsdu(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_tx_amsdu_rate(struct seq_file *m, void *v);
ssize_t proc_set_tx_amsdu_rate(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
#endif

int proc_get_en_fwps(struct seq_file *m, void *v);
ssize_t proc_set_en_fwps(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_btcoex_dbg(struct seq_file *m, void *v);
ssize_t proc_set_btcoex_dbg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_btcoex_info(struct seq_file *m, void *v);

int proc_get_odm_adaptivity(struct seq_file *m, void *v);
ssize_t proc_set_odm_adaptivity(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

#ifdef CONFIG_DBG_COUNTER
int proc_get_rx_logs(struct seq_file *m, void *v);
int proc_get_tx_logs(struct seq_file *m, void *v);
int proc_get_int_logs(struct seq_file *m, void *v);
#endif

int proc_get_new_bcn_max(struct seq_file *m, void *v);
ssize_t proc_set_new_bcn_max(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_ps_info(struct seq_file *m, void *v);
int proc_get_monitor(struct seq_file *m, void *v);
ssize_t proc_set_monitor(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

#ifdef CONFIG_IEEE80211W
ssize_t proc_set_tx_sa_query(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_tx_sa_query(struct seq_file *m, void *v);
ssize_t proc_set_tx_deauth(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_tx_deauth(struct seq_file *m, void *v);
ssize_t proc_set_tx_auth(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_tx_auth(struct seq_file *m, void *v);
#endif /* CONFIG_IEEE80211W */

#endif /* CONFIG_PROC_DEBUG */

int proc_get_efuse_map(struct seq_file *m, void *v);
ssize_t proc_set_efuse_map(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_ack_timeout(struct seq_file *m, void *v);
ssize_t proc_set_ack_timeout(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

int proc_get_fw_offload(struct seq_file *m, void *v);
ssize_t proc_set_fw_offload(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);

#ifdef CONFIG_DBG_RF_CAL
int proc_get_iqk_info(struct seq_file *m, void *v);
ssize_t proc_set_iqk(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
int proc_get_lck_info(struct seq_file *m, void *v);
ssize_t proc_set_lck(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data);
#endif /*CONFIG_DBG_RF_CAL*/

#define _drv_always_		1
#define _drv_emerg_			2
#define _drv_alert_			3
#define _drv_crit_			4
#define _drv_err_			5
#define _drv_warning_		6
#define _drv_notice_		7
#define _drv_info_			8
#define _drv_dump_			9
#define _drv_debug_			10

#define _module_rtl871x_xmit_c_		BIT(0)
#define _module_xmit_osdep_c_		BIT(1)
#define _module_rtl871x_recv_c_		BIT(2)
#define _module_recv_osdep_c_		BIT(3)
#define _module_rtl871x_mlme_c_		BIT(4)
#define _module_mlme_osdep_c_		BIT(5)
#define _module_rtl871x_sta_mgt_c_		BIT(6)
#define _module_rtl871x_cmd_c_			BIT(7)
#define _module_cmd_osdep_c_		BIT(8)
#define _module_rtl871x_io_c_				BIT(9)
#define _module_io_osdep_c_		BIT(10)
#define _module_os_intfs_c_			BIT(11)
#define _module_rtl871x_security_c_		BIT(12)
#define _module_rtl871x_eeprom_c_			BIT(13)
#define _module_hal_init_c_		BIT(14)
#define _module_hci_hal_init_c_		BIT(15)
#define _module_rtl871x_ioctl_c_		BIT(16)
#define _module_rtl871x_ioctl_set_c_		BIT(17)
#define _module_rtl871x_ioctl_query_c_	BIT(18)
#define _module_rtl871x_pwrctrl_c_			BIT(19)
#define _module_hci_intfs_c_			BIT(20)
#define _module_hci_ops_c_			BIT(21)
#define _module_osdep_service_c_			BIT(22)
#define _module_mp_			BIT(23)
#define _module_hci_ops_os_c_			BIT(24)
#define _module_rtl871x_ioctl_os_c		BIT(25)
#define _module_rtl8712_cmd_c_		BIT(26)
/* #define _module_efuse_			BIT(27) */
#define	_module_rtl8192c_xmit_c_ BIT(28)
#define _module_hal_xmit_c_	BIT(28)
#define _module_efuse_			BIT(29)
#define _module_rtl8712_recv_c_		BIT(30)
#define _module_rtl8712_led_c_		BIT(31)

#endif /* __RTW_DEBUG_H__ */
