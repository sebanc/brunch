// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _OS_INTFS_C_

#include <drv_types.h>
#include <hal_data.h>
#include <rtw_debug.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek Wireless Lan Driver");
MODULE_AUTHOR("Realtek Semiconductor Corp.");
MODULE_VERSION(DRIVERVERSION);

/* module param defaults */
static int rtw_chip_version = 0x00;
static int rtw_rfintfs = HWPI;
static int rtw_lbkmode = 0;/* RTL8712_AIR_TRX; */


static int rtw_network_mode = Ndis802_11IBSS;/* Ndis802_11Infrastructure; */ /* infra, ad-hoc, auto */
/* struct ndis_802_11_ssid	ssid; */
static int rtw_channel = 1;/* ad-hoc support requirement */
static int rtw_wireless_mode = WIRELESS_MODE_MAX;
static int rtw_vrtl_carrier_sense = AUTO_VCS;
static int rtw_vcs_type = RTS_CTS;
static int rtw_rts_thresh = 2347;
static int rtw_frag_thresh = 2346;
static int rtw_preamble = PREAMBLE_LONG;/* long, short, auto */
static int rtw_scan_mode = 1;/* active, passive */
static int rtw_adhoc_tx_pwr = 1;
static int rtw_soft_ap = 0;
/* int smart_ps = 1; */
static 	int rtw_power_mgnt = PS_MODE_MAX;
static	int rtw_ips_mode = IPS_NORMAL;

static 	int rtw_lps_level = LPS_NORMAL; /*USB default LPS level*/

module_param(rtw_ips_mode, int, 0644);
MODULE_PARM_DESC(rtw_ips_mode, "The default IPS mode");

module_param(rtw_lps_level, int, 0644);
MODULE_PARM_DESC(rtw_lps_level, "The default LPS level");

/* LPS: 
 * rtw_smart_ps = 0 => TX: pwr bit = 1, RX: PS_Poll
 * rtw_smart_ps = 1 => TX: pwr bit = 0, RX: PS_Poll
 * rtw_smart_ps = 2 => TX: pwr bit = 0, RX: NullData with pwr bit = 0
*/
static int rtw_smart_ps = 2;

static int rtw_check_fw_ps = 1;

static int rtw_usb_rxagg_mode = 2;/* RX_AGG_DMA=1, RX_AGG_USB=2 */
module_param(rtw_usb_rxagg_mode, int, 0644);

static int rtw_dynamic_agg_enable = 1;
module_param(rtw_dynamic_agg_enable, int, 0644);

/* set log level when inserting driver module, default log level is _DRV_INFO_ = 4,
* please refer to "How_to_set_driver_debug_log_level.doc" to set the available level.
*/
uint rtw_drv_log_level = _DRV_NONE_;
module_param(rtw_drv_log_level, uint, 0644);
MODULE_PARM_DESC(rtw_drv_log_level, "set log level when insert driver module, default log level is _DRV_NONE_ = 0");

static int rtw_radio_enable = 1;
static int rtw_long_retry_lmt = 7;
static int rtw_short_retry_lmt = 7;
static int rtw_busy_thresh = 40;
/* int qos_enable = 0; */ /* * */
static int rtw_ack_policy = NORMAL_ACK;

static int rtw_mp_mode = 0;

static int rtw_software_encrypt = 0;
static int rtw_software_decrypt = 0;

static int rtw_acm_method = 0;/* 0:By SW 1:By HW. */

static int rtw_wmm_enable = 1;/* default is set to enable the wmm. */

static int rtw_pwrtrim_enable = 0; /* Default Enalbe  power trim by efuse config */

static uint rtw_tx_bw_mode = 0x21;
module_param(rtw_tx_bw_mode, uint, 0644);
MODULE_PARM_DESC(rtw_tx_bw_mode, "The max tx bw for 2.4G and 5G. format is the same as rtw_bw_mode");

int rtw_ht_enable = 1;
/* 0: 20 MHz, 1: 40 MHz, 2: 80 MHz, 3: 160MHz, 4: 80+80MHz
* 2.4G use bit 0 ~ 3, 5G use bit 4 ~ 7
* 0x21 means enable 2.4G 40MHz & 5G 80MHz */
#ifdef CONFIG_RTW_CUSTOMIZE_BWMODE
int rtw_bw_mode = CONFIG_RTW_CUSTOMIZE_BWMODE;
#else
int rtw_bw_mode = 0x21;
#endif
int rtw_ampdu_enable = 1;/* for enable tx_ampdu , */ /* 0: disable, 0x1:enable */
static int rtw_rx_stbc = 1;/* 0: disable, bit(0):enable 2.4g, bit(1):enable 5g, default is set to enable 2.4GHZ for IOT issue with bufflao's AP at 5GHZ */
static int rtw_rx_ampdu_amsdu;/* 0: disabled, 1:enabled, 2:auto . There is an IOT issu with DLINK DIR-629 when the flag turn on */
/*
* 2: Follow the AMSDU filed in ADDBA Resp. (Deault)
* 0: Force the AMSDU filed in ADDBA Resp. to be disabled.
* 1: Force the AMSDU filed in ADDBA Resp. to be enabled.
*/
static int rtw_tx_ampdu_amsdu = 2;

static uint rtw_rx_ampdu_sz_limit_1ss[4] = CONFIG_RTW_RX_AMPDU_SZ_LIMIT_1SS;
static uint rtw_rx_ampdu_sz_limit_1ss_num = 0;
module_param_array(rtw_rx_ampdu_sz_limit_1ss, uint, &rtw_rx_ampdu_sz_limit_1ss_num, 0644);
MODULE_PARM_DESC(rtw_rx_ampdu_sz_limit_1ss, "RX AMPDU size limit for 1SS link of each BW, 0xFF: no limitation");

static uint rtw_rx_ampdu_sz_limit_2ss[4] = CONFIG_RTW_RX_AMPDU_SZ_LIMIT_2SS;
static uint rtw_rx_ampdu_sz_limit_2ss_num = 0;
module_param_array(rtw_rx_ampdu_sz_limit_2ss, uint, &rtw_rx_ampdu_sz_limit_2ss_num, 0644);
MODULE_PARM_DESC(rtw_rx_ampdu_sz_limit_2ss, "RX AMPDU size limit for 2SS link of each BW, 0xFF: no limitation");

static uint rtw_rx_ampdu_sz_limit_3ss[4] = CONFIG_RTW_RX_AMPDU_SZ_LIMIT_3SS;
static uint rtw_rx_ampdu_sz_limit_3ss_num = 0;
module_param_array(rtw_rx_ampdu_sz_limit_3ss, uint, &rtw_rx_ampdu_sz_limit_3ss_num, 0644);
MODULE_PARM_DESC(rtw_rx_ampdu_sz_limit_3ss, "RX AMPDU size limit for 3SS link of each BW, 0xFF: no limitation");

static uint rtw_rx_ampdu_sz_limit_4ss[4] = CONFIG_RTW_RX_AMPDU_SZ_LIMIT_4SS;
static uint rtw_rx_ampdu_sz_limit_4ss_num = 0;
module_param_array(rtw_rx_ampdu_sz_limit_4ss, uint, &rtw_rx_ampdu_sz_limit_4ss_num, 0644);
MODULE_PARM_DESC(rtw_rx_ampdu_sz_limit_4ss, "RX AMPDU size limit for 4SS link of each BW, 0xFF: no limitation");

/* Short GI support Bit Map
* BIT0 - 20MHz, 0: non-support, 1: support
* BIT1 - 40MHz, 0: non-support, 1: support
* BIT2 - 80MHz, 0: non-support, 1: support
* BIT3 - 160MHz, 0: non-support, 1: support */
static int rtw_short_gi = 0xf;
/* BIT0: Enable VHT LDPC Rx, BIT1: Enable VHT LDPC Tx, BIT4: Enable HT LDPC Rx, BIT5: Enable HT LDPC Tx */
static int rtw_ldpc_cap = 0x33;
/* BIT0: Enable VHT STBC Rx, BIT1: Enable VHT STBC Tx, BIT4: Enable HT STBC Rx, BIT5: Enable HT STBC Tx */
static int rtw_stbc_cap = 0x13;
/*
* BIT0: Enable VHT SU Beamformer
* BIT1: Enable VHT SU Beamformee
* BIT2: Enable VHT MU Beamformer, depend on VHT SU Beamformer
* BIT3: Enable VHT MU Beamformee, depend on VHT SU Beamformee
* BIT4: Enable HT Beamformer
* BIT5: Enable HT Beamformee
*/
static int rtw_beamform_cap = BIT(1) | BIT(3);
static int rtw_bfer_rf_number = 0; /*BeamformerCapRfNum Rf path number, 0 for auto, others for manual*/
static int rtw_bfee_rf_number = 0; /*BeamformeeCapRfNum  Rf path number, 0 for auto, others for manual*/

static int rtw_lowrate_two_xmit = 1;/* Use 2 path Tx to transmit MCS0~7 and legacy mode */

static int rtw_rf_config = RF_TYPE_MAX;
module_param(rtw_rf_config, int, 0644);

/* 0: not check in watch dog, 1: check in watch dog  */
static int rtw_check_hw_status = 0;

static int rtw_low_power = 0;
static 	int rtw_wifi_spec = 0;

static int rtw_special_rf_path = 0; /* 0: 2T2R ,1: only turn on path A 1T1R */

static char rtw_country_unspecified[] = {0xFF, 0xFF, 0x00};
static char *rtw_country_code = rtw_country_unspecified;
module_param(rtw_country_code, charp, 0644);
MODULE_PARM_DESC(rtw_country_code, "The default country code (in alpha2)");

static int rtw_channel_plan = CONFIG_RTW_CHPLAN;
module_param(rtw_channel_plan, int, 0644);
MODULE_PARM_DESC(rtw_channel_plan, "The default chplan ID when rtw_alpha2 is not specified or valid");

static uint rtw_excl_chs[MAX_CHANNEL_NUM] = CONFIG_RTW_EXCL_CHS;
static int rtw_excl_chs_num = 0;
module_param_array(rtw_excl_chs, uint, &rtw_excl_chs_num, 0644);
MODULE_PARM_DESC(rtw_excl_chs, "exclusive channel array");

/*if concurrent softap + p2p(GO) is needed, this param lets p2p response full channel list.
But Softap must be SHUT DOWN once P2P decide to set up connection and become a GO.*/
static 	int rtw_full_ch_in_p2p_handshake = 0; /* reply only softap channel*/

static int rtw_btcoex_enable = 2;
module_param(rtw_btcoex_enable, int, 0644);
MODULE_PARM_DESC(rtw_btcoex_enable, "BT co-existence on/off, 0:off, 1:on, 2:by efuse");

static int rtw_ant_num = 0;
module_param(rtw_ant_num, int, 0644);
MODULE_PARM_DESC(rtw_ant_num, "Antenna number setting, 0:by efuse");

static int rtw_bt_iso = 2;/* 0:Low, 1:High, 2:From Efuse */
static int rtw_bt_sco = 3;/* 0:Idle, 1:None-SCO, 2:SCO, 3:From Counter, 4.Busy, 5.OtherBusy */
static int rtw_bt_ampdu = 1 ; /* 0:Disable BT control A-MPDU, 1:Enable BT control A-MPDU. */

static int rtw_AcceptAddbaReq = true;/* 0:Reject AP's Add BA req, 1:Accept AP's Add BA req. */

static int rtw_antdiv_cfg = 2; /* 0:OFF , 1:ON, 2:decide by Efuse config */
static int rtw_antdiv_type = 0
	; /* 0:decide by efuse  1: for 88EE, 1Tx and 1RxCG are diversity.(2 Ant with SPDT), 2:  for 88EE, 1Tx and 2Rx are diversity.( 2 Ant, Tx and RxCG are both on aux port, RxCS is on main port ), 3: for 88EE, 1Tx and 1RxCG are fixed.(1Ant, Tx and RxCG are both on aux port) */

static int rtw_drv_ant_band_switch = 1; /* 0:OFF , 1:ON, Driver control antenna band switch*/

static int rtw_single_ant_path; /*0:main ant , 1:aux ant , Fixed single antenna path, default main ant*/

/* 0: doesn't switch, 1: switch from usb2.0 to usb 3.0 2: switch from usb3.0 to usb 2.0 */
static int rtw_switch_usb_mode = 0;

static int rtw_enusbss = 0;/* 0:disable,1:enable */

static int rtw_hwpdn_mode = 2; /* 0:disable,1:enable,2: by EFUSE config */

static int rtw_hwpwrp_detect = 0; /* HW power  ping detect 0:disable , 1:enable */

static int rtw_hw_wps_pbc = 1;

int rtw_mc2u_disable = 0;

static int rtw_80211d = 0;

#ifdef CONFIG_PCI_ASPM
/* CLK_REQ:BIT0 L0s:BIT1 ASPM_L1:BIT2 L1Off:BIT3*/
int	rtw_pci_aspm_enable = 0x5;
#endif

#ifdef CONFIG_QOS_OPTIMIZATION
int rtw_qos_opt_enable = 1; /* 0: disable,1:enable */
#else
static int rtw_qos_opt_enable = 0; /* 0: disable,1:enable */
#endif
module_param(rtw_qos_opt_enable, int, 0644);

static char *ifname = "wlan%d";
module_param(ifname, charp, 0644);
MODULE_PARM_DESC(ifname, "The default name to allocate for first interface");

static 	char *if2name = "wlan%d";
module_param(if2name, charp, 0644);
MODULE_PARM_DESC(if2name, "The default name to allocate for second interface");

char *rtw_initmac = NULL;  /* temp mac address if users want to use instead of the mac address in Efuse */

#ifdef CONFIG_CONCURRENT_MODE

	#if (CONFIG_IFACE_NUMBER > 2)
		int rtw_virtual_iface_num = CONFIG_IFACE_NUMBER - 1;
		module_param(rtw_virtual_iface_num, int, 0644);
	#else
		int rtw_virtual_iface_num = 1;
	#endif

#endif
static u8 rtw_bmc_tx_rate = MGN_UNKNOWN;
module_param(rtw_pwrtrim_enable, int, 0644);
module_param(rtw_initmac, charp, 0644);
module_param(rtw_special_rf_path, int, 0644);
module_param(rtw_chip_version, int, 0644);
module_param(rtw_rfintfs, int, 0644);
module_param(rtw_lbkmode, int, 0644);
module_param(rtw_network_mode, int, 0644);
module_param(rtw_channel, int, 0644);
module_param(rtw_mp_mode, int, 0644);
module_param(rtw_wmm_enable, int, 0644);
module_param(rtw_vrtl_carrier_sense, int, 0644);
module_param(rtw_vcs_type, int, 0644);
module_param(rtw_busy_thresh, int, 0644);

module_param(rtw_ht_enable, int, 0644);
module_param(rtw_bw_mode, int, 0644);
module_param(rtw_ampdu_enable, int, 0644);
module_param(rtw_rx_stbc, int, 0644);
module_param(rtw_rx_ampdu_amsdu, int, 0644);
module_param(rtw_tx_ampdu_amsdu, int, 0644);

module_param(rtw_lowrate_two_xmit, int, 0644);

module_param(rtw_power_mgnt, int, 0644);
module_param(rtw_smart_ps, int, 0644);
module_param(rtw_low_power, int, 0644);
module_param(rtw_wifi_spec, int, 0644);

module_param(rtw_full_ch_in_p2p_handshake, int, 0644);
module_param(rtw_antdiv_cfg, int, 0644);
module_param(rtw_antdiv_type, int, 0644);

module_param(rtw_drv_ant_band_switch, int, 0644);
module_param(rtw_single_ant_path, int, 0644);

module_param(rtw_switch_usb_mode, int, 0644);

module_param(rtw_enusbss, int, 0644);
module_param(rtw_hwpdn_mode, int, 0644);
module_param(rtw_hwpwrp_detect, int, 0644);

module_param(rtw_hw_wps_pbc, int, 0644);
module_param(rtw_check_hw_status, int, 0644);

static uint rtw_max_roaming_times = 2;
module_param(rtw_max_roaming_times, uint, 0644);
MODULE_PARM_DESC(rtw_max_roaming_times, "The max roaming times to try");

#ifdef CONFIG_FILE_FWIMG
char *rtw_fw_file_path = "/system/etc/firmware/rtlwifi/FW_NIC.BIN";
module_param(rtw_fw_file_path, charp, 0644);
MODULE_PARM_DESC(rtw_fw_file_path, "The path of fw image");

char *rtw_fw_wow_file_path = "/system/etc/firmware/rtlwifi/FW_WoWLAN.BIN";
module_param(rtw_fw_wow_file_path, charp, 0644);
MODULE_PARM_DESC(rtw_fw_wow_file_path, "The path of fw for Wake on Wireless image");
#endif /* CONFIG_FILE_FWIMG */

module_param(rtw_mc2u_disable, int, 0644);

module_param(rtw_80211d, int, 0644);
MODULE_PARM_DESC(rtw_80211d, "Enable 802.11d mechanism");

#ifdef CONFIG_ADVANCE_OTA
/*	BIT(0): OTA continuous rotated test within low RSSI,1R CCA in path B
	BIT(1) & BIT(2): OTA continuous rotated test with low high RSSI */
/* Experimental environment: shielding room with half of absorber and 2~3 rotation per minute */
int rtw_advnace_ota;
module_param(rtw_advnace_ota, int, 0644);
#endif

static uint rtw_notch_filter = RTW_NOTCH_FILTER;
module_param(rtw_notch_filter, uint, 0644);
MODULE_PARM_DESC(rtw_notch_filter, "0:Disable, 1:Enable, 2:Enable only for P2P");

static uint rtw_hiq_filter = CONFIG_RTW_HIQ_FILTER;
module_param(rtw_hiq_filter, uint, 0644);
MODULE_PARM_DESC(rtw_hiq_filter, "0:allow all, 1:allow special, 2:deny all");

static uint rtw_adaptivity_en = CONFIG_RTW_ADAPTIVITY_EN;
module_param(rtw_adaptivity_en, uint, 0644);
MODULE_PARM_DESC(rtw_adaptivity_en, "0:disable, 1:enable");

static uint rtw_adaptivity_mode = CONFIG_RTW_ADAPTIVITY_MODE;
module_param(rtw_adaptivity_mode, uint, 0644);
MODULE_PARM_DESC(rtw_adaptivity_mode, "0:normal, 1:carrier sense");

static uint rtw_adaptivity_dml = CONFIG_RTW_ADAPTIVITY_DML;
module_param(rtw_adaptivity_dml, uint, 0644);
MODULE_PARM_DESC(rtw_adaptivity_dml, "0:disable, 1:enable");

static uint rtw_adaptivity_dc_backoff = CONFIG_RTW_ADAPTIVITY_DC_BACKOFF;
module_param(rtw_adaptivity_dc_backoff, uint, 0644);
MODULE_PARM_DESC(rtw_adaptivity_dc_backoff, "DC backoff for Adaptivity");

static int rtw_adaptivity_th_l2h_ini = CONFIG_RTW_ADAPTIVITY_TH_L2H_INI;
module_param(rtw_adaptivity_th_l2h_ini, int, 0644);
MODULE_PARM_DESC(rtw_adaptivity_th_l2h_ini, "th_l2h_ini for Adaptivity");

static int rtw_adaptivity_th_edcca_hl_diff = CONFIG_RTW_ADAPTIVITY_TH_EDCCA_HL_DIFF;
module_param(rtw_adaptivity_th_edcca_hl_diff, int, 0644);
MODULE_PARM_DESC(rtw_adaptivity_th_edcca_hl_diff, "th_edcca_hl_diff for Adaptivity");

static uint rtw_amplifier_type_2g = CONFIG_RTW_AMPLIFIER_TYPE_2G;
module_param(rtw_amplifier_type_2g, uint, 0644);
MODULE_PARM_DESC(rtw_amplifier_type_2g, "BIT3:2G ext-PA, BIT4:2G ext-LNA");

static uint rtw_amplifier_type_5g = CONFIG_RTW_AMPLIFIER_TYPE_5G;
module_param(rtw_amplifier_type_5g, uint, 0644);
MODULE_PARM_DESC(rtw_amplifier_type_5g, "BIT6:5G ext-PA, BIT7:5G ext-LNA");

static uint rtw_RFE_type = CONFIG_RTW_RFE_TYPE;
module_param(rtw_RFE_type, uint, 0644);
MODULE_PARM_DESC(rtw_RFE_type, "default init value:64");

static uint rtw_powertracking_type = 64;
module_param(rtw_powertracking_type, uint, 0644);
MODULE_PARM_DESC(rtw_powertracking_type, "default init value:64");

static uint rtw_GLNA_type = CONFIG_RTW_GLNA_TYPE;
module_param(rtw_GLNA_type, uint, 0644);
MODULE_PARM_DESC(rtw_GLNA_type, "default init value:0");

static uint rtw_TxBBSwing_2G = 0xFF;
module_param(rtw_TxBBSwing_2G, uint, 0644);
MODULE_PARM_DESC(rtw_TxBBSwing_2G, "default init value:0xFF");

static uint rtw_TxBBSwing_5G = 0xFF;
module_param(rtw_TxBBSwing_5G, uint, 0644);
MODULE_PARM_DESC(rtw_TxBBSwing_5G, "default init value:0xFF");

static uint rtw_OffEfuseMask = 0;
module_param(rtw_OffEfuseMask, uint, 0644);
MODULE_PARM_DESC(rtw_OffEfuseMask, "default open Efuse Mask value:0");

static uint rtw_FileMaskEfuse = 0;
module_param(rtw_FileMaskEfuse, uint, 0644);
MODULE_PARM_DESC(rtw_FileMaskEfuse, "default drv Mask Efuse value:0");

static uint rtw_rxgain_offset_2g = 0;
module_param(rtw_rxgain_offset_2g, uint, 0644);
MODULE_PARM_DESC(rtw_rxgain_offset_2g, "default RF Gain 2G Offset value:0");

static int rtw_rxgain_offset_5gl = 0;
module_param(rtw_rxgain_offset_5gl, uint, 0644);
MODULE_PARM_DESC(rtw_rxgain_offset_5gl, "default RF Gain 5GL Offset value:0");

static uint rtw_rxgain_offset_5gm = 0;
module_param(rtw_rxgain_offset_5gm, uint, 0644);
MODULE_PARM_DESC(rtw_rxgain_offset_5gm, "default RF Gain 5GM Offset value:0");

static uint rtw_rxgain_offset_5gh = 0;
module_param(rtw_rxgain_offset_5gh, uint, 0644);
MODULE_PARM_DESC(rtw_rxgain_offset_5gm, "default RF Gain 5GL Offset value:0");

static uint rtw_pll_ref_clk_sel = CONFIG_RTW_PLL_REF_CLK_SEL;
module_param(rtw_pll_ref_clk_sel, uint, 0644);
MODULE_PARM_DESC(rtw_pll_ref_clk_sel, "force pll_ref_clk_sel, 0xF:use autoload value");

static int rtw_tx_pwr_by_rate = CONFIG_TXPWR_BY_RATE_EN;
module_param(rtw_tx_pwr_by_rate, int, 0644);
MODULE_PARM_DESC(rtw_tx_pwr_by_rate, "0:Disable, 1:Enable, 2: Depend on efuse");

#ifdef CONFIG_TXPWR_LIMIT
int rtw_tx_pwr_lmt_enable = CONFIG_TXPWR_LIMIT_EN;
module_param(rtw_tx_pwr_lmt_enable, int, 0644);
MODULE_PARM_DESC(rtw_tx_pwr_lmt_enable, "0:Disable, 1:Enable, 2: Depend on efuse");
#endif

static int rtw_target_tx_pwr_2g_a[RATE_SECTION_NUM] = CONFIG_RTW_TARGET_TX_PWR_2G_A;
static int rtw_target_tx_pwr_2g_a_num = 0;
module_param_array(rtw_target_tx_pwr_2g_a, int, &rtw_target_tx_pwr_2g_a_num, 0644);
MODULE_PARM_DESC(rtw_target_tx_pwr_2g_a, "2.4G target tx power (unit:dBm) of RF path A for each rate section, should match the real calibrate power, -1: undefined");

static int rtw_target_tx_pwr_2g_b[RATE_SECTION_NUM] = CONFIG_RTW_TARGET_TX_PWR_2G_B;
static int rtw_target_tx_pwr_2g_b_num = 0;
module_param_array(rtw_target_tx_pwr_2g_b, int, &rtw_target_tx_pwr_2g_b_num, 0644);
MODULE_PARM_DESC(rtw_target_tx_pwr_2g_b, "2.4G target tx power (unit:dBm) of RF path B for each rate section, should match the real calibrate power, -1: undefined");

static int rtw_target_tx_pwr_2g_c[RATE_SECTION_NUM] = CONFIG_RTW_TARGET_TX_PWR_2G_C;
static int rtw_target_tx_pwr_2g_c_num = 0;
module_param_array(rtw_target_tx_pwr_2g_c, int, &rtw_target_tx_pwr_2g_c_num, 0644);
MODULE_PARM_DESC(rtw_target_tx_pwr_2g_c, "2.4G target tx power (unit:dBm) of RF path C for each rate section, should match the real calibrate power, -1: undefined");

static int rtw_target_tx_pwr_2g_d[RATE_SECTION_NUM] = CONFIG_RTW_TARGET_TX_PWR_2G_D;
static int rtw_target_tx_pwr_2g_d_num = 0;
module_param_array(rtw_target_tx_pwr_2g_d, int, &rtw_target_tx_pwr_2g_d_num, 0644);
MODULE_PARM_DESC(rtw_target_tx_pwr_2g_d, "2.4G target tx power (unit:dBm) of RF path D for each rate section, should match the real calibrate power, -1: undefined");

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
char *rtw_phy_file_path = REALTEK_CONFIG_PATH;
module_param(rtw_phy_file_path, charp, 0644);
MODULE_PARM_DESC(rtw_phy_file_path, "The path of phy parameter");
/* PHY FILE Bit Map
* BIT0 - MAC,				0: non-support, 1: support
* BIT1 - BB,					0: non-support, 1: support
* BIT2 - BB_PG,				0: non-support, 1: support
* BIT3 - BB_MP,				0: non-support, 1: support
* BIT4 - RF,					0: non-support, 1: support
* BIT5 - RF_TXPWR_TRACK,	0: non-support, 1: support
* BIT6 - RF_TXPWR_LMT,		0: non-support, 1: support */
static int rtw_load_phy_file = (BIT2 | BIT6);
module_param(rtw_load_phy_file, int, 0644);
MODULE_PARM_DESC(rtw_load_phy_file, "PHY File Bit Map");
static int rtw_decrypt_phy_file = 0;
module_param(rtw_decrypt_phy_file, int, 0644);
MODULE_PARM_DESC(rtw_decrypt_phy_file, "Enable Decrypt PHY File");
#endif

int _netdev_open(struct net_device *pnetdev);
int netdev_open(struct net_device *pnetdev);
static int netdev_close(struct net_device *pnetdev);

#ifdef CONFIG_RTW_NAPI
/*following setting should define NAPI in Makefile
enable napi only = 1, disable napi = 0*/
static int rtw_en_napi = 1;
module_param(rtw_en_napi, int, 0644);
#ifdef CONFIG_RTW_NAPI_DYNAMIC
int rtw_napi_threshold = 100; /* unit: Mbps */
module_param(rtw_napi_threshold, int, 0644);
#endif /* CONFIG_RTW_NAPI_DYNAMIC */
/*following setting should define GRO in Makefile
enable gro = 1, disable gro = 0*/
static int rtw_en_gro = 1;
module_param(rtw_en_gro, int, 0644);
#endif /* CONFIG_RTW_NAPI */

#ifdef RTW_IQK_FW_OFFLOAD
static int rtw_iqk_fw_offload = 1;
#else
static int rtw_iqk_fw_offload;
#endif /* RTW_IQK_FW_OFFLOAD */
module_param(rtw_iqk_fw_offload, int, 0644);

#ifdef RTW_CHANNEL_SWITCH_OFFLOAD
int rtw_ch_switch_offload = 1;
#else
static int rtw_ch_switch_offload;
#endif /* RTW_IQK_FW_OFFLOAD */
module_param(rtw_ch_switch_offload, int, 0644);

#ifdef CONFIG_FW_OFFLOAD_PARAM_INIT
int rtw_fw_param_init = 1;
module_param(rtw_fw_param_init, int, 0644);
#endif

static void rtw_regsty_load_target_tx_power(struct registry_priv *regsty)
{
	int path, rs;
	int *target_tx_pwr;

	for (path = RF_PATH_A; path < RF_PATH_MAX; path++) {
		if (path == RF_PATH_A)
			target_tx_pwr = rtw_target_tx_pwr_2g_a;
		else if (path == RF_PATH_B)
			target_tx_pwr = rtw_target_tx_pwr_2g_b;
		else if (path == RF_PATH_C)
			target_tx_pwr = rtw_target_tx_pwr_2g_c;
		else if (path == RF_PATH_D)
			target_tx_pwr = rtw_target_tx_pwr_2g_d;

		for (rs = CCK; rs < RATE_SECTION_NUM; rs++)
			regsty->target_tx_pwr_2g[path][rs] = target_tx_pwr[rs];
	}
}

inline void rtw_regsty_load_excl_chs(struct registry_priv *regsty)
{
	int i;
	int ch_num = 0;

	for (i = 0; i < MAX_CHANNEL_NUM; i++)
		if (((u8)rtw_excl_chs[i]) != 0)
			regsty->excl_chs[ch_num++] = (u8)rtw_excl_chs[i];

	if (ch_num < MAX_CHANNEL_NUM)
		regsty->excl_chs[ch_num] = 0;
}

inline void rtw_regsty_init_rx_ampdu_sz_limit(struct registry_priv *regsty)
{
	int i, j;
	uint *sz_limit;

	for (i = 0; i < 4; i++) {
		if (i == 0)
			sz_limit = rtw_rx_ampdu_sz_limit_1ss;
		else if (i == 1)
			sz_limit = rtw_rx_ampdu_sz_limit_2ss;
		else if (i == 2)
			sz_limit = rtw_rx_ampdu_sz_limit_3ss;
		else if (i == 3)
			sz_limit = rtw_rx_ampdu_sz_limit_4ss;

		for (j = 0; j < 4; j++)
			regsty->rx_ampdu_sz_limit_by_nss_bw[i][j] = sz_limit[j];
	}
}

uint loadparam(struct adapter *adapt)
{
	uint status = _SUCCESS;
	struct registry_priv  *registry_par = &adapt->registrypriv;


#ifdef CONFIG_RTW_DEBUG
	if (rtw_drv_log_level >= _DRV_MAX_)
		rtw_drv_log_level = _DRV_DEBUG_;
#endif

	registry_par->chip_version = (u8)rtw_chip_version;
	registry_par->rfintfs = (u8)rtw_rfintfs;
	registry_par->lbkmode = (u8)rtw_lbkmode;
	/* registry_par->hci = (u8)hci; */
	registry_par->network_mode  = (u8)rtw_network_mode;

	memcpy(registry_par->ssid.Ssid, "ANY", 3);
	registry_par->ssid.SsidLength = 3;

	registry_par->channel = (u8)rtw_channel;
	registry_par->wireless_mode = (u8)rtw_wireless_mode;

	if (IsSupported24G(registry_par->wireless_mode) && (!is_supported_5g(registry_par->wireless_mode))
	    && (registry_par->channel > 14))
		registry_par->channel = 1;
	else if (is_supported_5g(registry_par->wireless_mode) && (!IsSupported24G(registry_par->wireless_mode))
		 && (registry_par->channel <= 14))
		registry_par->channel = 36;

	registry_par->vrtl_carrier_sense = (u8)rtw_vrtl_carrier_sense ;
	registry_par->vcs_type = (u8)rtw_vcs_type;
	registry_par->rts_thresh = (u16)rtw_rts_thresh;
	registry_par->frag_thresh = (u16)rtw_frag_thresh;
	registry_par->preamble = (u8)rtw_preamble;
	registry_par->scan_mode = (u8)rtw_scan_mode;
	registry_par->adhoc_tx_pwr = (u8)rtw_adhoc_tx_pwr;
	registry_par->soft_ap = (u8)rtw_soft_ap;
	registry_par->smart_ps = (u8)rtw_smart_ps;
	registry_par->check_fw_ps = (u8)rtw_check_fw_ps;
	registry_par->power_mgnt = (u8)rtw_power_mgnt;
	registry_par->ips_mode = (u8)rtw_ips_mode;
	registry_par->lps_level = (u8)rtw_lps_level;
	registry_par->radio_enable = (u8)rtw_radio_enable;
	registry_par->long_retry_lmt = (u8)rtw_long_retry_lmt;
	registry_par->short_retry_lmt = (u8)rtw_short_retry_lmt;
	registry_par->busy_thresh = (u16)rtw_busy_thresh;
	/* registry_par->qos_enable = (u8)rtw_qos_enable; */
	registry_par->ack_policy = (u8)rtw_ack_policy;
	registry_par->mp_mode = (u8)rtw_mp_mode;
	registry_par->software_encrypt = (u8)rtw_software_encrypt;
	registry_par->software_decrypt = (u8)rtw_software_decrypt;

	registry_par->acm_method = (u8)rtw_acm_method;
	registry_par->usb_rxagg_mode = (u8)rtw_usb_rxagg_mode;
	registry_par->dynamic_agg_enable = (u8)rtw_dynamic_agg_enable;

	/* WMM */
	registry_par->wmm_enable = (u8)rtw_wmm_enable;

	registry_par->RegPwrTrimEnable = (u8)rtw_pwrtrim_enable;

	registry_par->tx_bw_mode = (u8)rtw_tx_bw_mode;

	registry_par->ht_enable = (u8)rtw_ht_enable;
	registry_par->bw_mode = (u8)rtw_bw_mode;
	registry_par->ampdu_enable = (u8)rtw_ampdu_enable;
	registry_par->rx_stbc = (u8)rtw_rx_stbc;
	registry_par->rx_ampdu_amsdu = (u8)rtw_rx_ampdu_amsdu;
	registry_par->tx_ampdu_amsdu = (u8)rtw_tx_ampdu_amsdu;
	registry_par->short_gi = (u8)rtw_short_gi;
	registry_par->ldpc_cap = (u8)rtw_ldpc_cap;
	registry_par->stbc_cap = (u8)rtw_stbc_cap;
	registry_par->beamform_cap = (u8)rtw_beamform_cap;
	registry_par->beamformer_rf_num = (u8)rtw_bfer_rf_number;
	registry_par->beamformee_rf_num = (u8)rtw_bfee_rf_number;
	rtw_regsty_init_rx_ampdu_sz_limit(registry_par);

	registry_par->lowrate_two_xmit = (u8)rtw_lowrate_two_xmit;
	registry_par->rf_config = (u8)rtw_rf_config;
	registry_par->low_power = (u8)rtw_low_power;

	registry_par->check_hw_status = (u8)rtw_check_hw_status;

	registry_par->wifi_spec = (u8)rtw_wifi_spec;

	if (strlen(rtw_country_code) != 2 ||
	    !is_alpha(rtw_country_code[0]) ||
	    !is_alpha(rtw_country_code[1])) {
		if (rtw_country_code != rtw_country_unspecified)
			RTW_ERR("%s discard rtw_country_code not in alpha2\n", __func__);
		memset(registry_par->alpha2, 0xFF, 2);
	} else {
		memcpy(registry_par->alpha2, rtw_country_code, 2);
	}
	registry_par->channel_plan = (u8)rtw_channel_plan;
	rtw_regsty_load_excl_chs(registry_par);

	registry_par->special_rf_path = (u8)rtw_special_rf_path;

	registry_par->full_ch_in_p2p_handshake = (u8)rtw_full_ch_in_p2p_handshake;
	registry_par->btcoex = (u8)rtw_btcoex_enable;
	registry_par->bt_iso = (u8)rtw_bt_iso;
	registry_par->bt_sco = (u8)rtw_bt_sco;
	registry_par->bt_ampdu = (u8)rtw_bt_ampdu;
	registry_par->ant_num = (u8)rtw_ant_num;
	registry_par->single_ant_path = (u8) rtw_single_ant_path;

	registry_par->bAcceptAddbaReq = (u8)rtw_AcceptAddbaReq;

	registry_par->antdiv_cfg = (u8)rtw_antdiv_cfg;
	registry_par->antdiv_type = (u8)rtw_antdiv_type;

	registry_par->drv_ant_band_switch = (u8) rtw_drv_ant_band_switch;

	registry_par->switch_usb_mode = (u8)rtw_switch_usb_mode;

#ifdef CONFIG_AUTOSUSPEND
	registry_par->usbss_enable = (u8)rtw_enusbss;/* 0:disable,1:enable */
#endif
#ifdef SUPPORT_HW_RFOFF_DETECTED
	registry_par->hwpdn_mode = (u8)rtw_hwpdn_mode;/* 0:disable,1:enable,2:by EFUSE config */
	registry_par->hwpwrp_detect = (u8)rtw_hwpwrp_detect;/* 0:disable,1:enable */
#endif

	registry_par->hw_wps_pbc = (u8)rtw_hw_wps_pbc;

	registry_par->max_roaming_times = (u8)rtw_max_roaming_times;
#ifdef CONFIG_INTEL_WIDI
	registry_par->max_roaming_times = (u8)rtw_max_roaming_times + 2;
#endif /* CONFIG_INTEL_WIDI */

	registry_par->enable80211d = (u8)rtw_80211d;

	snprintf(registry_par->ifname, 16, "%s", ifname);
	snprintf(registry_par->if2name, 16, "%s", if2name);

	registry_par->notch_filter = (u8)rtw_notch_filter;

#ifdef CONFIG_CONCURRENT_MODE
	registry_par->virtual_iface_num = (u8)rtw_virtual_iface_num;
#endif
	registry_par->pll_ref_clk_sel = (u8)rtw_pll_ref_clk_sel;

#ifdef CONFIG_TXPWR_LIMIT
	registry_par->RegEnableTxPowerLimit = (u8)rtw_tx_pwr_lmt_enable;
#endif
	registry_par->RegEnableTxPowerByRate = (u8)rtw_tx_pwr_by_rate;

	rtw_regsty_load_target_tx_power(registry_par);

	registry_par->TxBBSwing_2G = (s8)rtw_TxBBSwing_2G;
	registry_par->TxBBSwing_5G = (s8)rtw_TxBBSwing_5G;
	registry_par->bEn_RFE = 1;
	registry_par->RFE_Type = (u8)rtw_RFE_type;
	registry_par->PowerTracking_Type = (u8)rtw_powertracking_type;
	registry_par->AmplifierType_2G = (u8)rtw_amplifier_type_2g;
	registry_par->AmplifierType_5G = (u8)rtw_amplifier_type_5g;
	registry_par->GLNA_Type = (u8)rtw_GLNA_type;
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	registry_par->load_phy_file = (u8)rtw_load_phy_file;
	registry_par->RegDecryptCustomFile = (u8)rtw_decrypt_phy_file;
#endif
	registry_par->qos_opt_enable = (u8)rtw_qos_opt_enable;

	registry_par->hiq_filter = (u8)rtw_hiq_filter;

	registry_par->adaptivity_en = (u8)rtw_adaptivity_en;
	registry_par->adaptivity_mode = (u8)rtw_adaptivity_mode;
	registry_par->adaptivity_dml = (u8)rtw_adaptivity_dml;
	registry_par->adaptivity_dc_backoff = (u8)rtw_adaptivity_dc_backoff;
	registry_par->adaptivity_th_l2h_ini = (s8)rtw_adaptivity_th_l2h_ini;
	registry_par->adaptivity_th_edcca_hl_diff = (s8)rtw_adaptivity_th_edcca_hl_diff;

	registry_par->boffefusemask = (u8)rtw_OffEfuseMask;
	registry_par->bFileMaskEfuse = (u8)rtw_FileMaskEfuse;

	registry_par->reg_rxgain_offset_2g = (u32) rtw_rxgain_offset_2g;
	registry_par->reg_rxgain_offset_5gl = (u32) rtw_rxgain_offset_5gl;
	registry_par->reg_rxgain_offset_5gm = (u32) rtw_rxgain_offset_5gm;
	registry_par->reg_rxgain_offset_5gh = (u32) rtw_rxgain_offset_5gh;

#ifdef CONFIG_RTW_NAPI
	registry_par->en_napi = (u8)rtw_en_napi;
#ifdef CONFIG_RTW_NAPI_DYNAMIC
	registry_par->napi_threshold = (u32)rtw_napi_threshold;
#endif /* CONFIG_RTW_NAPI_DYNAMIC */
	registry_par->en_gro = (u8)rtw_en_gro;
	if (!registry_par->en_napi && registry_par->en_gro) {
		registry_par->en_gro = 0;
		RTW_WARN("Disable GRO because NAPI is not enabled\n");
	}
#endif /* CONFIG_RTW_NAPI */

	registry_par->iqk_fw_offload = (u8)rtw_iqk_fw_offload;
	registry_par->ch_switch_offload = (u8)rtw_ch_switch_offload;

#ifdef CONFIG_ADVANCE_OTA
	registry_par->adv_ota = rtw_advnace_ota;
#endif
#ifdef CONFIG_FW_OFFLOAD_PARAM_INIT
	registry_par->fw_param_init = rtw_fw_param_init;
#endif
	registry_par->bmc_tx_rate = rtw_bmc_tx_rate;
	return status;
}

/**
 * rtw_net_set_mac_address
 * This callback function is used for the Media Access Control address
 * of each net_device needs to be changed.
 *
 * Arguments:
 * @pnetdev: net_device pointer.
 * @addr: new MAC address.
 *
 * Return:
 * ret = 0: Permit to change net_device's MAC address.
 * ret = -1 (Default): Operation not permitted.
 *
 * Auther: Arvin Liu
 * Date: 2015/05/29
 */
static int rtw_net_set_mac_address(struct net_device *pnetdev, void *addr)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct sockaddr *sa = (struct sockaddr *)addr;
	int ret = -1;

	/* only the net_device is in down state to permit modifying mac addr */
	if ((pnetdev->flags & IFF_UP)) {
		RTW_INFO(FUNC_ADPT_FMT": The net_device's is not in down state\n"
			 , FUNC_ADPT_ARG(adapt));

		return ret;
	}

	/* if the net_device is linked, it's not permit to modify mac addr */
	if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING) ||
	    check_fwstate(pmlmepriv, _FW_LINKED) ||
	    check_fwstate(pmlmepriv, _FW_UNDER_SURVEY)) {
		RTW_INFO(FUNC_ADPT_FMT": The net_device's is not idle currently\n"
			 , FUNC_ADPT_ARG(adapt));

		return ret;
	}

	/* check whether the input mac address is valid to permit modifying mac addr */
	if (rtw_check_invalid_mac_address(sa->sa_data, false)) {
		RTW_INFO(FUNC_ADPT_FMT": Invalid Mac Addr for "MAC_FMT"\n"
			 , FUNC_ADPT_ARG(adapt), MAC_ARG(sa->sa_data));

		return ret;
	}

	memcpy(adapter_mac_addr(adapt), sa->sa_data, ETH_ALEN); /* set mac addr to adapter */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	memcpy(pnetdev->dev_addr, sa->sa_data, ETH_ALEN); /* set mac addr to net_device */
#else
	dev_addr_set(pnetdev, sa->sa_data);
#endif

	rtw_ps_deny(adapt, PS_DENY_IOCTL);
	LeaveAllPowerSaveModeDirect(adapt); /* leave PS mode for guaranteeing to access hw register successfully */
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_hal_change_macaddr_mbid(adapt, sa->sa_data);
#else
	rtw_hal_set_hwreg(adapt, HW_VAR_MAC_ADDR, sa->sa_data); /* set mac addr to mac register */
#endif
	rtw_ps_deny_cancel(adapt, PS_DENY_IOCTL);

	RTW_INFO(FUNC_ADPT_FMT": Set Mac Addr to "MAC_FMT" Successfully\n"
		 , FUNC_ADPT_ARG(adapt), MAC_ARG(sa->sa_data));

	ret = 0;

	return ret;
}

static struct net_device_stats *rtw_net_get_stats(struct net_device *pnetdev)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct xmit_priv *pxmitpriv = &(adapt->xmitpriv);
	struct recv_priv *precvpriv = &(adapt->recvpriv);

	adapt->stats.tx_packets = pxmitpriv->tx_pkts;/* pxmitpriv->tx_pkts++; */
	adapt->stats.rx_packets = precvpriv->rx_pkts;/* precvpriv->rx_pkts++; */
	adapt->stats.tx_dropped = pxmitpriv->tx_drop;
	adapt->stats.rx_dropped = precvpriv->rx_drop;
	adapt->stats.tx_bytes = pxmitpriv->tx_bytes;
	adapt->stats.rx_bytes = precvpriv->rx_bytes;

	return &adapt->stats;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
/*
 * AC to queue mapping
 *
 * AC_VO -> queue 0
 * AC_VI -> queue 1
 * AC_BE -> queue 2
 * AC_BK -> queue 3
 */
static const u16 rtw_1d_to_queue[8] = { 2, 3, 3, 2, 1, 1, 0, 0 };

/* Given a data frame determine the 802.1p/1d tag to use. */
static unsigned int rtw_classify8021d(struct sk_buff *skb)
{
	unsigned int dscp;

	/* skb->priority values from 256->263 are magic values to
	 * directly indicate a specific 802.1d priority.  This is used
	 * to allow 802.1d priority to be passed directly in from VLAN
	 * tags, etc.
	 */
	if (skb->priority >= 256 && skb->priority <= 263)
		return skb->priority - 256;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		dscp = ip_hdr(skb)->tos & 0xfc;
		break;
	default:
		return 0;
	}

	return dscp >> 5;
}

static u16 rtw_select_queue(struct net_device *dev, struct sk_buff *skb,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
			    struct net_device *sb_dev
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
			    struct net_device *sb_dev,
                            select_queue_fallback_t fallback
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
 			    void *unused,
                            select_queue_fallback_t fallback
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
			    void *accel_priv
#endif
)
{
	struct adapter	*adapt = rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;

	skb->priority = rtw_classify8021d(skb);

	if (pmlmepriv->acm_mask != 0)
		skb->priority = qos_acm(pmlmepriv->acm_mask, skb->priority);

	return rtw_1d_to_queue[skb->priority];
}

u16 rtw_recv_select_queue(struct sk_buff *skb)
{
	struct iphdr *piphdr;
	unsigned int dscp;
	__be16	eth_type;
	u32 priority;
	u8 *pdata = skb->data;

	memcpy(&eth_type, pdata + (ETH_ALEN << 1), 2);

	switch (eth_type) {
	case htons(ETH_P_IP):

		piphdr = (struct iphdr *)(pdata + ETH_HLEN);

		dscp = piphdr->tos & 0xfc;

		priority = dscp >> 5;

		break;
	default:
		priority = 0;
	}

	return rtw_1d_to_queue[priority];

}

#endif

static u8 is_rtw_ndev(struct net_device *ndev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
	return ndev->netdev_ops
		&& ndev->netdev_ops->ndo_do_ioctl
		&& ndev->netdev_ops->ndo_do_ioctl == rtw_ioctl;
#else
	return ndev->do_ioctl
		&& ndev->do_ioctl == rtw_ioctl;
#endif
}

static int rtw_ndev_notifier_call(struct notifier_block *nb, unsigned long state, void *ptr)
{
	struct net_device *ndev;

	if (!ptr)
		return NOTIFY_DONE;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
	ndev = netdev_notifier_info_to_dev(ptr);
#else
	ndev = ptr;
#endif

	if (!ndev)
		return NOTIFY_DONE;

	if (!is_rtw_ndev(ndev))
		return NOTIFY_DONE;

	RTW_INFO(FUNC_NDEV_FMT" state:%lu\n", FUNC_NDEV_ARG(ndev), state);

	switch (state) {
	case NETDEV_CHANGENAME:
		rtw_adapter_proc_replace(ndev);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block rtw_ndev_notifier = {
	.notifier_call = rtw_ndev_notifier_call,
};

int rtw_ndev_notifier_register(void)
{
	return register_netdevice_notifier(&rtw_ndev_notifier);
}

void rtw_ndev_notifier_unregister(void)
{
	unregister_netdevice_notifier(&rtw_ndev_notifier);
}

static int rtw_ndev_init(struct net_device *dev)
{
	struct adapter *adapter = rtw_netdev_priv(dev);

	RTW_PRINT(FUNC_ADPT_FMT" if%d mac_addr="MAC_FMT"\n"
		, FUNC_ADPT_ARG(adapter), (adapter->iface_id + 1), MAC_ARG(dev->dev_addr));
	strncpy(adapter->old_ifname, dev->name, IFNAMSIZ);
	adapter->old_ifname[IFNAMSIZ - 1] = '\0';
	rtw_adapter_proc_init(dev);

	return 0;
}

static void rtw_ndev_uninit(struct net_device *dev)
{
	struct adapter *adapter = rtw_netdev_priv(dev);

	RTW_PRINT(FUNC_ADPT_FMT" if%d\n"
		  , FUNC_ADPT_ARG(adapter), (adapter->iface_id + 1));
	rtw_adapter_proc_deinit(dev);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
static const struct net_device_ops rtw_netdev_ops = {
	.ndo_init = rtw_ndev_init,
	.ndo_uninit = rtw_ndev_uninit,
	.ndo_open = netdev_open,
	.ndo_stop = netdev_close,
	.ndo_start_xmit = rtw_xmit_entry,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	.ndo_select_queue	= rtw_select_queue,
#endif
	.ndo_set_mac_address = rtw_net_set_mac_address,
	.ndo_get_stats = rtw_net_get_stats,
	.ndo_do_ioctl = rtw_ioctl,
};
#endif

int rtw_init_netdev_name(struct net_device *pnetdev, const char *ifname)
{
	if (dev_alloc_name(pnetdev, ifname) < 0)
		RTW_ERR("dev_alloc_name, fail!\n");
	rtw_netif_carrier_off(pnetdev);

	return 0;
}

static void rtw_hook_if_ops(struct net_device *ndev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
	ndev->netdev_ops = &rtw_netdev_ops;
#else
	ndev->init = rtw_ndev_init;
	ndev->uninit = rtw_ndev_uninit;
	ndev->open = _netdev_open;
	ndev->stop = netdev_close;
	ndev->hard_start_xmit = rtw_xmit_entry;
	ndev->set_mac_address = rtw_net_set_mac_address;
	ndev->get_stats = rtw_net_get_stats;
	ndev->do_ioctl = rtw_ioctl;
#endif
}

#ifdef CONFIG_CONCURRENT_MODE
static void rtw_hook_vir_if_ops(struct net_device *ndev);
#endif
struct net_device *rtw_init_netdev(struct adapter *old_adapt)
{
	struct adapter *adapt;
	struct net_device *pnetdev;

	if (old_adapt) {
		rtw_os_ndev_free(old_adapt);
		pnetdev = rtw_alloc_etherdev_with_old_priv(sizeof(struct adapter), (void *)old_adapt);
	} else
		pnetdev = rtw_alloc_etherdev(sizeof(struct adapter));

	if (!pnetdev)
		return NULL;

	adapt = rtw_netdev_priv(pnetdev);
	adapt->pnetdev = pnetdev;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	SET_MODULE_OWNER(pnetdev);
#endif

	rtw_hook_if_ops(pnetdev);
#ifdef CONFIG_CONCURRENT_MODE
	if (!is_primary_adapter(adapt))
		rtw_hook_vir_if_ops(pnetdev);
#endif /* CONFIG_CONCURRENT_MODE */

#ifdef CONFIG_TCP_CSUM_OFFLOAD_TX
	pnetdev->features |= NETIF_F_IP_CSUM;
#endif

	/* pnetdev->tx_timeout = NULL; */
	pnetdev->watchdog_timeo = HZ * 3; /* 3 second timeout */

#ifdef CONFIG_WIRELESS_EXT
	pnetdev->wireless_handlers = (struct iw_handler_def *)&rtw_handlers_def;
#endif

#ifdef WIRELESS_SPY
	/* priv->wireless_data.spy_data = &priv->spy_data; */
	/* pnetdev->wireless_data = &priv->wireless_data; */
#endif

	return pnetdev;
}

static int rtw_os_ndev_alloc(struct adapter *adapter)
{
	int ret = _FAIL;
	struct net_device *ndev = NULL;

	ndev = rtw_init_netdev(adapter);
	if (!ndev) {
		rtw_warn_on(1);
		goto exit;
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 0)
	SET_NETDEV_DEV(ndev, dvobj_to_dev(adapter_to_dvobj(adapter)));
#endif

	if (rtw_cfg80211_ndev_res_alloc(adapter) != _SUCCESS) {
		rtw_warn_on(1);
		goto free_ndev;
	}
	ret = _SUCCESS;

free_ndev:
	if (ret != _SUCCESS && ndev)
		rtw_free_netdev(ndev);
exit:
	return ret;
}

void rtw_os_ndev_free(struct adapter *adapter)
{
	rtw_cfg80211_ndev_res_free(adapter);
	if (adapter->pnetdev) {
		rtw_free_netdev(adapter->pnetdev);
		adapter->pnetdev = NULL;
	}
}

static int rtw_os_ndev_register(struct adapter *adapter, const char *name)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	int ret = _SUCCESS;
	struct net_device *ndev = adapter->pnetdev;
	u8 rtnl_lock_needed = rtw_rtnl_lock_needed(dvobj);

#ifdef CONFIG_RTW_NAPI
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1,0)
	netif_napi_add(ndev, &adapter->napi, rtw_recv_napi_poll, RTL_NAPI_WEIGHT);
#else
	netif_napi_add(ndev, &adapter->napi, rtw_recv_napi_poll);
#endif
#endif /* CONFIG_RTW_NAPI */

	if (rtw_cfg80211_ndev_res_register(adapter) != _SUCCESS) {
		rtw_warn_on(1);
		ret = _FAIL;
		goto exit;
	}
	/* alloc netdev name */
	rtw_init_netdev_name(ndev, name);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	memcpy(ndev->dev_addr, adapter_mac_addr(adapter), ETH_ALEN);
#else
	dev_addr_set(ndev, adapter_mac_addr(adapter));
#endif

	/* Tell the network stack we exist */

	if (rtnl_lock_needed)
		ret = (register_netdev(ndev) == 0) ? _SUCCESS : _FAIL;
	else
		ret = (register_netdevice(ndev) == 0) ? _SUCCESS : _FAIL;

	if (ret == _SUCCESS)
		adapter->registered = 1;
	else
		RTW_INFO(FUNC_NDEV_FMT" if%d Failed!\n", FUNC_NDEV_ARG(ndev), (adapter->iface_id + 1));

	if (ret != _SUCCESS) {
		rtw_cfg80211_ndev_res_unregister(adapter);
		#if !defined(RTW_SINGLE_WIPHY)
		rtw_wiphy_unregister(adapter_to_wiphy(adapter));
		#endif
	}

exit:
#ifdef CONFIG_RTW_NAPI
	if (ret != _SUCCESS)
		netif_napi_del(&adapter->napi);
#endif /* CONFIG_RTW_NAPI */

	return ret;
}

void rtw_os_ndev_unregister(struct adapter *adapter)
{
	struct net_device *netdev = NULL;

	if (!adapter || adapter->registered == 0)
		return;

	adapter->ndev_unregistering = 1;

	netdev = adapter->pnetdev;

	rtw_cfg80211_ndev_res_unregister(adapter);

	if ((adapter->DriverState != DRIVER_DISAPPEAR) && netdev) {
		struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
		u8 rtnl_lock_needed = rtw_rtnl_lock_needed(dvobj);

		if (rtnl_lock_needed)
			unregister_netdev(netdev);
		else
			unregister_netdevice(netdev);
	}

#if !defined(RTW_SINGLE_WIPHY)
	rtw_wiphy_unregister(adapter_to_wiphy(adapter));
#endif
#ifdef CONFIG_RTW_NAPI
	if (adapter->napi_state == NAPI_ENABLE) {
		napi_disable(&adapter->napi);
		adapter->napi_state = NAPI_DISABLE;
	}
	netif_napi_del(&adapter->napi);
#endif /* CONFIG_RTW_NAPI */

	adapter->registered = 0;
	adapter->ndev_unregistering = 0;
}

/**
 * rtw_os_ndev_init - Allocate and register OS layer net device and relating structures for @adapter
 * @adapter: the adapter on which this function applies
 * @name: the requesting net device name
 *
 * Returns:
 * _SUCCESS or _FAIL
 */
int rtw_os_ndev_init(struct adapter *adapter, const char *name)
{
	int ret = _FAIL;

	if (rtw_os_ndev_alloc(adapter) != _SUCCESS)
		goto exit;

	if (rtw_os_ndev_register(adapter, name) != _SUCCESS)
		goto os_ndev_free;

	ret = _SUCCESS;

os_ndev_free:
	if (ret != _SUCCESS)
		rtw_os_ndev_free(adapter);
exit:
	return ret;
}

/**
 * rtw_os_ndev_deinit - Unregister and free OS layer net device and relating structures for @adapter
 * @adapter: the adapter on which this function applies
 */
void rtw_os_ndev_deinit(struct adapter *adapter)
{
	rtw_os_ndev_unregister(adapter);
	rtw_os_ndev_free(adapter);
}

static int rtw_os_ndevs_alloc(struct dvobj_priv *dvobj)
{
	int i, status = _SUCCESS;
	struct adapter *adapter;

	if (rtw_cfg80211_dev_res_alloc(dvobj) != _SUCCESS) {
		rtw_warn_on(1);
		status = _FAIL;
		goto exit;
	}

	for (i = 0; i < dvobj->iface_nums; i++) {

		if (i >= CONFIG_IFACE_NUMBER) {
			RTW_ERR("%s %d >= CONFIG_IFACE_NUMBER(%d)\n", __func__, i, CONFIG_IFACE_NUMBER);
			rtw_warn_on(1);
			continue;
		}

		adapter = dvobj->adapters[i];
		if (adapter && !adapter->pnetdev) {

			#ifdef CONFIG_RTW_DYNAMIC_NDEV
			if (!is_primary_adapter(adapter))
				continue;
			#endif

			status = rtw_os_ndev_alloc(adapter);
			if (status != _SUCCESS) {
				rtw_warn_on(1);
				break;
			}
		}
	}

	if (status != _SUCCESS) {
		for (; i >= 0; i--) {
			adapter = dvobj->adapters[i];
			if (adapter && adapter->pnetdev)
				rtw_os_ndev_free(adapter);
		}
	}

	if (status != _SUCCESS)
		rtw_cfg80211_dev_res_free(dvobj);
exit:
	return status;
}

static void rtw_os_ndevs_free(struct dvobj_priv *dvobj)
{
	int i;
	struct adapter *adapter = NULL;

	for (i = 0; i < dvobj->iface_nums; i++) {

		if (i >= CONFIG_IFACE_NUMBER) {
			RTW_ERR("%s %d >= CONFIG_IFACE_NUMBER(%d)\n", __func__, i, CONFIG_IFACE_NUMBER);
			rtw_warn_on(1);
			continue;
		}

		adapter = dvobj->adapters[i];

		if (!adapter)
			continue;

		rtw_os_ndev_free(adapter);
	}

	rtw_cfg80211_dev_res_free(dvobj);
}

u32 rtw_start_drv_threads(struct adapter *adapt)
{
	u32 _status = _SUCCESS;

	if (is_primary_adapter(adapt)) {
		if (!adapt->cmdThread) {
			RTW_INFO(FUNC_ADPT_FMT " start RTW_CMD_THREAD\n", FUNC_ADPT_ARG(adapt));
			adapt->cmdThread = kthread_run(rtw_cmd_thread, adapt, "RTW_CMD_THREAD");
			if (IS_ERR(adapt->cmdThread)) {
				adapt->cmdThread = NULL;
				_status = _FAIL;
			}
			else
				_rtw_down_sema(&adapt->cmdpriv.start_cmdthread_sema); /* wait for cmd_thread to run */
		}
	}


#ifdef CONFIG_EVENT_THREAD_MODE
	if (!adapt->evtThread) {
		RTW_INFO(FUNC_ADPT_FMT " start RTW_EVENT_THREAD\n", FUNC_ADPT_ARG(adapt));
		adapt->evtThread = kthread_run(event_thread, adapt, "RTW_EVENT_THREAD");
		if (IS_ERR(adapt->evtThread)) {
			adapt->evtThread = NULL;
			_status = _FAIL;
		}
	}
#endif

	rtw_hal_start_thread(adapt);
	return _status;

}

void rtw_stop_drv_threads(struct adapter *adapt)
{

	if (is_primary_adapter(adapt))
		rtw_stop_cmd_thread(adapt);

#ifdef CONFIG_EVENT_THREAD_MODE
	if (adapt->evtThread) {
		up(&adapt->evtpriv.evt_notify);
		rtw_thread_stop(adapt->evtThread);
		adapt->evtThread = NULL;
	}
#endif

	rtw_hal_stop_thread(adapt);
}

u8 rtw_init_default_value(struct adapter *adapt);
u8 rtw_init_default_value(struct adapter *adapt)
{
	u8 ret  = _SUCCESS;
	struct registry_priv *pregistrypriv = &adapt->registrypriv;
	struct xmit_priv	*pxmitpriv = &adapt->xmitpriv;
	struct security_priv *psecuritypriv = &adapt->securitypriv;

	/* xmit_priv */
	pxmitpriv->vcs_setting = pregistrypriv->vrtl_carrier_sense;
	pxmitpriv->vcs = pregistrypriv->vcs_type;
	pxmitpriv->vcs_type = pregistrypriv->vcs_type;
	/* pxmitpriv->rts_thresh = pregistrypriv->rts_thresh; */
	pxmitpriv->frag_len = pregistrypriv->frag_thresh;

	/* security_priv */
	/* rtw_get_encrypt_decrypt_from_registrypriv(adapt); */
	psecuritypriv->binstallGrpkey = _FAIL;
#ifdef CONFIG_GTK_OL
	psecuritypriv->binstallKCK_KEK = _FAIL;
#endif /* CONFIG_GTK_OL */
	psecuritypriv->sw_encrypt = pregistrypriv->software_encrypt;
	psecuritypriv->sw_decrypt = pregistrypriv->software_decrypt;

	psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_Open; /* open system */
	psecuritypriv->dot11PrivacyAlgrthm = _NO_PRIVACY_;

	psecuritypriv->dot11PrivacyKeyIndex = 0;

	psecuritypriv->dot118021XGrpPrivacy = _NO_PRIVACY_;
	psecuritypriv->dot118021XGrpKeyid = 1;

	psecuritypriv->ndisauthtype = Ndis802_11AuthModeOpen;
	psecuritypriv->ndisencryptstatus = Ndis802_11WEPDisabled;
#ifdef CONFIG_CONCURRENT_MODE
	psecuritypriv->dot118021x_bmc_cam_id = INVALID_SEC_MAC_CAM_ID;
#endif


	/* pwrctrl_priv */


	/* registry_priv */
	rtw_init_registrypriv_dev_network(adapt);
	rtw_update_registrypriv_dev_network(adapt);


	/* hal_priv */
	rtw_hal_def_value_init(adapt);

	/* misc. */
	RTW_ENABLE_FUNC(adapt, DF_RX_BIT);
	RTW_ENABLE_FUNC(adapt, DF_TX_BIT);
	adapt->bLinkInfoDump = 0;
	adapt->bNotifyChannelChange = false;
	adapt->bShowGetP2PState = 1;

	/* for debug purpose */
	adapt->fix_rate = 0xFF;
	adapt->data_fb = 0;
	adapt->fix_bw = 0xFF;
	adapt->power_offset = 0;
	adapt->rsvd_page_offset = 0;
	adapt->rsvd_page_num = 0;
	adapt->bmc_tx_rate = pregistrypriv->bmc_tx_rate;
	adapt->driver_tx_bw_mode = pregistrypriv->tx_bw_mode;

	adapt->driver_ampdu_spacing = 0xFF;
	adapt->driver_rx_ampdu_factor =  0xFF;
	adapt->driver_rx_ampdu_spacing = 0xFF;
	adapt->fix_rx_ampdu_accept = RX_AMPDU_ACCEPT_INVALID;
	adapt->fix_rx_ampdu_size = RX_AMPDU_SIZE_INVALID;
#ifdef CONFIG_TX_AMSDU
	adapt->tx_amsdu = 2;
	adapt->tx_amsdu_rate = 400;
#endif
	adapt->driver_tx_max_agg_num = 0xFF;
#ifdef CONFIG_RTW_NAPI
	adapt->napi_state = NAPI_DISABLE;
#endif
	adapt->tsf.sync_port =  MAX_HW_PORT;
	adapt->tsf.offset = 0;

	return ret;
}

struct dvobj_priv *devobj_init(void)
{
	struct dvobj_priv *pdvobj = NULL;

	pdvobj = (struct dvobj_priv *)rtw_zmalloc(sizeof(*pdvobj));
	if (!pdvobj)
		return NULL;

	_rtw_mutex_init(&pdvobj->hw_init_mutex);
	_rtw_mutex_init(&pdvobj->h2c_fwcmd_mutex);
	_rtw_mutex_init(&pdvobj->setch_mutex);
	_rtw_mutex_init(&pdvobj->setbw_mutex);
	_rtw_mutex_init(&pdvobj->rf_read_reg_mutex);

	_rtw_mutex_init(&pdvobj->customer_str_mutex);
	memset(pdvobj->customer_str, 0xFF, RTW_CUSTOMER_STR_LEN);

	pdvobj->processing_dev_remove = false;

	ATOMIC_SET(&pdvobj->disable_func, 0);

	rtw_macid_ctl_init(&pdvobj->macid_ctl);
	spin_lock_init(&pdvobj->cam_ctl.lock);
	_rtw_mutex_init(&pdvobj->cam_ctl.sec_cam_access_mutex);
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_mbid_cam_init(pdvobj);
#endif

	pdvobj->nr_ap_if = 0;
	pdvobj->inter_bcn_space = DEFAULT_BCN_INTERVAL; /* default value is equal to the default beacon_interval (100ms) */
	_rtw_init_queue(&pdvobj->ap_if_q);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
	timer_setup(&pdvobj->dynamic_chk_timer, rtw_dynamic_check_timer_handler, 0);
#else
	rtw_init_timer(&(pdvobj->dynamic_chk_timer), NULL, rtw_dynamic_check_timer_handler, pdvobj);
#endif

#ifdef CONFIG_RTW_NAPI_DYNAMIC
	pdvobj->en_napi_dynamic = 0;
#endif /* CONFIG_RTW_NAPI_DYNAMIC */


	return pdvobj;

}

void devobj_deinit(struct dvobj_priv *pdvobj)
{
	if (!pdvobj)
		return;

	/* TODO: use rtw_os_ndevs_deinit instead at the first stage of driver's dev deinit function */
	rtw_cfg80211_dev_res_free(pdvobj);

	_rtw_mutex_free(&pdvobj->hw_init_mutex);
	_rtw_mutex_free(&pdvobj->h2c_fwcmd_mutex);

	_rtw_mutex_free(&pdvobj->customer_str_mutex);

	_rtw_mutex_free(&pdvobj->setch_mutex);
	_rtw_mutex_free(&pdvobj->setbw_mutex);
	_rtw_mutex_free(&pdvobj->rf_read_reg_mutex);

	rtw_macid_ctl_deinit(&pdvobj->macid_ctl);
	_rtw_mutex_free(&pdvobj->cam_ctl.sec_cam_access_mutex);

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_mbid_cam_deinit(pdvobj);
#endif
	rtw_mfree((u8 *)pdvobj, sizeof(*pdvobj));
}

inline u8 rtw_rtnl_lock_needed(struct dvobj_priv *dvobj)
{
	if (dvobj->rtnl_lock_holder && dvobj->rtnl_lock_holder == current)
		return 0;
	return 1;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26))
static inline int rtnl_is_locked(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17))
	if (unlikely(rtnl_trylock())) {
		rtnl_unlock();
#else
	if (unlikely(down_trylock(&rtnl_sem) == 0)) {
		up(&rtnl_sem);
#endif
		return 0;
	}
	return 1;
}
#endif

inline void rtw_set_rtnl_lock_holder(struct dvobj_priv *dvobj, void * thd_hdl)
{
	rtw_warn_on(!rtnl_is_locked());

	if (!thd_hdl || rtnl_is_locked())
		dvobj->rtnl_lock_holder = thd_hdl;

	if (dvobj->rtnl_lock_holder && 0)
		RTW_INFO("rtnl_lock_holder: %s:%d\n", current->comm, current->pid);
}

u8 rtw_reset_drv_sw(struct adapter *adapt)
{
	u8	ret8 = _SUCCESS;
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;

	/* hal_priv */
	if (is_primary_adapter(adapt))
		rtw_hal_def_value_init(adapt);

	RTW_ENABLE_FUNC(adapt, DF_RX_BIT);
	RTW_ENABLE_FUNC(adapt, DF_TX_BIT);

	adapt->tsf.sync_port =  MAX_HW_PORT;
	adapt->tsf.offset = 0;

	adapt->bLinkInfoDump = 0;

	adapt->xmitpriv.tx_pkts = 0;
	adapt->recvpriv.rx_pkts = 0;

	pmlmepriv->LinkDetectInfo.bBusyTraffic = false;

	/* pmlmepriv->LinkDetectInfo.TrafficBusyState = false; */
	pmlmepriv->LinkDetectInfo.TrafficTransitionCount = 0;
	pmlmepriv->LinkDetectInfo.LowPowerTransitionCount = 0;

	_clr_fwstate_(pmlmepriv, _FW_UNDER_SURVEY | _FW_UNDER_LINKING);

#ifdef CONFIG_AUTOSUSPEND
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 34))
	adapter_to_dvobj(adapt)->pusbdev->autosuspend_disabled = 1;/* autosuspend disabled by the user */
#endif
#endif

	adapt->pwr_state_check_cnts = 0;

	/* mlmeextpriv */
	mlmeext_set_scan_state(&adapt->mlmeextpriv, SCAN_DISABLE);

	rtw_set_signal_stat_timer(&adapt->recvpriv);

	return ret8;
}


u8 rtw_init_drv_sw(struct adapter *adapt)
{
	u8	ret8 = _SUCCESS;

	INIT_LIST_HEAD(&adapt->list);

	ret8 = rtw_init_default_value(adapt);

	if ((rtw_init_cmd_priv(&adapt->cmdpriv)) == _FAIL) {
		ret8 = _FAIL;
		goto exit;
	}

	adapt->cmdpriv.adapt = adapt;

	if ((rtw_init_evt_priv(&adapt->evtpriv)) == _FAIL) {
		ret8 = _FAIL;
		goto exit;
	}

	if (is_primary_adapter(adapt))
		rtw_rfctl_init(adapt);

	if (rtw_init_mlme_priv(adapt) == _FAIL) {
		ret8 = _FAIL;
		goto exit;
	}

	rtw_init_wifidirect_timers(adapt);
	init_wifidirect_info(adapt, P2P_ROLE_DISABLE);
	reset_global_wifidirect_info(adapt);
	rtw_init_cfg80211_wifidirect_info(adapt);
	if (rtw_init_wifi_display_info(adapt) == _FAIL)
		RTW_ERR("Can't init init_wifi_display_info\n");

	if (init_mlme_ext_priv(adapt) == _FAIL) {
		ret8 = _FAIL;
		goto exit;
	}

	if (_rtw_init_xmit_priv(&adapt->xmitpriv, adapt) == _FAIL) {
		RTW_INFO("Can't _rtw_init_xmit_priv\n");
		ret8 = _FAIL;
		goto exit;
	}

	if (_rtw_init_recv_priv(&adapt->recvpriv, adapt) == _FAIL) {
		RTW_INFO("Can't _rtw_init_recv_priv\n");
		ret8 = _FAIL;
		goto exit;
	}
	/* add for CONFIG_IEEE80211W, none 11w also can use */
	spin_lock_init(&adapt->security_key_mutex);

	/* We don't need to memset adapt->XXX to zero, because adapter is allocated by vzalloc(). */
	/* memset((unsigned char *)&adapt->securitypriv, 0, sizeof (struct security_priv)); */

	if (_rtw_init_sta_priv(&adapt->stapriv) == _FAIL) {
		RTW_INFO("Can't _rtw_init_sta_priv\n");
		ret8 = _FAIL;
		goto exit;
	}

	adapt->setband = WIFI_FREQUENCY_BAND_AUTO;
	adapt->fix_rate = 0xFF;
	adapt->power_offset = 0;
	adapt->rsvd_page_offset = 0;
	adapt->rsvd_page_num = 0;

	adapt->data_fb = 0;
	adapt->fix_rx_ampdu_accept = RX_AMPDU_ACCEPT_INVALID;
	adapt->fix_rx_ampdu_size = RX_AMPDU_SIZE_INVALID;
	rtw_init_bcmc_stainfo(adapt);

	rtw_init_pwrctrl_priv(adapt);

	/* memset((u8 *)&adapt->qospriv, 0, sizeof (struct qos_priv)); */ /* move to mlme_priv */

	rtw_hal_dm_init(adapt);
	rtw_hal_sw_led_init(adapt);

#ifdef CONFIG_INTEL_WIDI
	if (rtw_init_intel_widi(adapt) == _FAIL) {
		RTW_INFO("Can't rtw_init_intel_widi\n");
		ret8 = _FAIL;
		goto exit;
	}
#endif /* CONFIG_INTEL_WIDI */

	spin_lock_init(&adapt->br_ext_lock);

exit:



	return ret8;

}

void rtw_cancel_all_timer(struct adapter *adapt)
{

	_cancel_timer_ex(&adapt->mlmepriv.assoc_timer);

	_cancel_timer_ex(&adapt->mlmepriv.scan_to_timer);

	_cancel_timer_ex(&adapter_to_dvobj(adapt)->dynamic_chk_timer);
	/* cancel sw led timer */
	rtw_hal_sw_led_deinit(adapt);
	_cancel_timer_ex(&adapt->pwr_state_check_timer);

#ifdef CONFIG_TX_AMSDU
	_cancel_timer_ex(&adapt->xmitpriv.amsdu_bk_timer);
	_cancel_timer_ex(&adapt->xmitpriv.amsdu_be_timer);
	_cancel_timer_ex(&adapt->xmitpriv.amsdu_vo_timer);
	_cancel_timer_ex(&adapt->xmitpriv.amsdu_vi_timer);
#endif

	_cancel_timer_ex(&adapt->cfg80211_wdinfo.remain_on_ch_timer);

	_cancel_timer_ex(&adapt->mlmepriv.set_scan_deny_timer);
	rtw_clear_scan_deny(adapt);
	_cancel_timer_ex(&adapt->recvpriv.signal_stat_timer);

	/* cancel dm timer */
	rtw_hal_dm_deinit(adapt);

#ifdef CONFIG_PLATFORM_FS_MX61
	msleep(50);
#endif
}

u8 rtw_free_drv_sw(struct adapter *adapt)
{
	/* we can call rtw_p2p_enable here, but: */
	/* 1. rtw_p2p_enable may have IO operation */
	/* 2. rtw_p2p_enable is bundled with wext interface */
	struct wifidirect_info *pwdinfo = &adapt->wdinfo;
	if (!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE)) {
		_cancel_timer_ex(&pwdinfo->find_phase_timer);
		_cancel_timer_ex(&pwdinfo->restore_p2p_state_timer);
		_cancel_timer_ex(&pwdinfo->pre_tx_scan_timer);
		#ifdef CONFIG_CONCURRENT_MODE
		_cancel_timer_ex(&pwdinfo->ap_p2p_switch_timer);
		#endif /* CONFIG_CONCURRENT_MODE */
		rtw_p2p_set_state(pwdinfo, P2P_STATE_NONE);
	}
#ifdef CONFIG_INTEL_WIDI
	rtw_free_intel_widi(adapt);
#endif /* CONFIG_INTEL_WIDI */

	free_mlme_ext_priv(&adapt->mlmeextpriv);

	rtw_free_cmd_priv(&adapt->cmdpriv);

	rtw_free_evt_priv(&adapt->evtpriv);

	rtw_free_mlme_priv(&adapt->mlmepriv);

	if (is_primary_adapter(adapt))
		rtw_rfctl_deinit(adapt);

	/* free_io_queue(adapt); */

	_rtw_free_xmit_priv(&adapt->xmitpriv);

	_rtw_free_sta_priv(&adapt->stapriv); /* will free bcmc_stainfo here */

	_rtw_free_recv_priv(&adapt->recvpriv);

	rtw_free_pwrctrl_priv(adapt);

	/* rtw_mfree((void *)adapt, sizeof (adapt)); */

	rtw_hal_free_data(adapt);


	/* free the old_pnetdev */
	if (adapt->rereg_nd_name_priv.old_pnetdev) {
		free_netdev(adapt->rereg_nd_name_priv.old_pnetdev);
		adapt->rereg_nd_name_priv.old_pnetdev = NULL;
	}

	return _SUCCESS;

}
void rtw_intf_start(struct adapter *adapter)
{
	if (adapter->intf_start)
		adapter->intf_start(adapter);
}
void rtw_intf_stop(struct adapter *adapter)
{
	if (adapter->intf_stop)
		adapter->intf_stop(adapter);
}

#ifdef CONFIG_CONCURRENT_MODE
int _netdev_vir_if_open(struct net_device *pnetdev)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct adapter *primary_adapt = GET_PRIMARY_ADAPTER(adapt);

	RTW_INFO(FUNC_NDEV_FMT" , bup=%d\n", FUNC_NDEV_ARG(pnetdev), adapt->bup);

	if (!primary_adapt)
		goto _netdev_virtual_iface_open_error;

	if (!primary_adapt->bup || !rtw_is_hw_init_completed(primary_adapt))
		_netdev_open(primary_adapt->pnetdev);

	if (!adapt->bup && primary_adapt->bup &&
	    rtw_is_hw_init_completed(primary_adapt)) {
	}

	if (!adapt->bup) {
		if (rtw_start_drv_threads(adapt) == _FAIL)
			goto _netdev_virtual_iface_open_error;
	}

#ifdef CONFIG_RTW_NAPI
	if (adapt->napi_state == NAPI_DISABLE) {
		napi_enable(&adapt->napi);
		adapt->napi_state = NAPI_ENABLE;
	}
#endif

	rtw_cfg80211_init_wiphy(adapt);
	rtw_cfg80211_init_wdev_data(adapt);

	adapt->bup = true;

	adapt->net_closed = false;

	rtw_netif_wake_queue(pnetdev);

	RTW_INFO(FUNC_NDEV_FMT" (bup=%d) exit\n", FUNC_NDEV_ARG(pnetdev), adapt->bup);

	return 0;

_netdev_virtual_iface_open_error:

	adapt->bup = false;

#ifdef CONFIG_RTW_NAPI
	if(adapt->napi_state == NAPI_ENABLE) {
		napi_disable(&adapt->napi);
		adapt->napi_state = NAPI_DISABLE;
	}
#endif

	rtw_netif_carrier_off(pnetdev);
	rtw_netif_stop_queue(pnetdev);

	return -1;

}

int netdev_vir_if_open(struct net_device *pnetdev)
{
	int ret;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);

	_enter_critical_mutex(&(adapter_to_dvobj(adapt)->hw_init_mutex), NULL);
	ret = _netdev_vir_if_open(pnetdev);
	_exit_critical_mutex(&(adapter_to_dvobj(adapt)->hw_init_mutex), NULL);

	return ret;
}

static int netdev_vir_if_close(struct net_device *pnetdev)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;

	RTW_INFO(FUNC_NDEV_FMT" , bup=%d\n", FUNC_NDEV_ARG(pnetdev), adapt->bup);
	adapt->net_closed = true;
	pmlmepriv->LinkDetectInfo.bBusyTraffic = false;

	if (pnetdev)
		rtw_netif_stop_queue(pnetdev);

	if (!rtw_p2p_chk_role(&adapt->wdinfo, P2P_ROLE_DISABLE))
		rtw_p2p_enable(adapt, P2P_ROLE_DISABLE);

	rtw_scan_abort(adapt);
	rtw_cfg80211_wait_scan_req_empty(adapt, 200);
	adapter_wdev_data(adapt)->bandroid_scan = false;

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
static const struct net_device_ops rtw_netdev_vir_if_ops = {
	.ndo_init = rtw_ndev_init,
	.ndo_uninit = rtw_ndev_uninit,
	.ndo_open = netdev_vir_if_open,
	.ndo_stop = netdev_vir_if_close,
	.ndo_start_xmit = rtw_xmit_entry,
	.ndo_set_mac_address = rtw_net_set_mac_address,
	.ndo_get_stats = rtw_net_get_stats,
	.ndo_do_ioctl = rtw_ioctl,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	.ndo_select_queue	= rtw_select_queue,
#endif
};
#endif

static void rtw_hook_vir_if_ops(struct net_device *ndev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
	ndev->netdev_ops = &rtw_netdev_vir_if_ops;
#else
	ndev->init = rtw_ndev_init;
	ndev->uninit = rtw_ndev_uninit;
	ndev->open = netdev_vir_if_open;
	ndev->stop = netdev_vir_if_close;
	ndev->set_mac_address = rtw_net_set_mac_address;
#endif
}
struct adapter *rtw_drv_add_vir_if(struct adapter *primary_adapt,
	void (*set_intf_ops)(struct adapter *primary_adapt, struct _io_ops *pops))
{
	int res = _FAIL;
	struct adapter *adapt = NULL;
	struct dvobj_priv *pdvobjpriv;
	u8 mac[ETH_ALEN];

	/****** init adapter ******/
	adapt = (struct adapter *)vzalloc(sizeof(*adapt));
	if (!adapt)
		goto exit;

	if (loadparam(adapt) != _SUCCESS)
		goto free_adapter;

	memcpy(adapt, primary_adapt, sizeof(struct adapter));

	/*  */
	adapt->bup = false;
	adapt->net_closed = true;
	adapt->dir_dev = NULL;
	adapt->dir_odm = NULL;

	/*set adapter_type/iface type*/
	adapt->isprimary = false;
	adapt->adapter_type = VIRTUAL_ADAPTER;

#ifdef CONFIG_MI_WITH_MBSSID_CAM
	adapt->hw_port = HW_PORT0;
#else
	adapt->hw_port = HW_PORT1;
#endif


	/****** hook vir if into dvobj ******/
	pdvobjpriv = adapter_to_dvobj(adapt);
	adapt->iface_id = pdvobjpriv->iface_nums;
	pdvobjpriv->adapters[pdvobjpriv->iface_nums++] = adapt;

	adapt->intf_start = primary_adapt->intf_start;
	adapt->intf_stop = primary_adapt->intf_stop;

	/* step init_io_priv */
	if ((rtw_init_io_priv(adapt, set_intf_ops)) == _FAIL) {
		goto free_adapter;
	}

	/*init drv data*/
	if (rtw_init_drv_sw(adapt) != _SUCCESS)
		goto free_drv_sw;


	/*get mac address from primary_adapt*/
	memcpy(mac, adapter_mac_addr(primary_adapt), ETH_ALEN);

	/*
	* If the BIT1 is 0, the address is universally administered.
	* If it is 1, the address is locally administered
	*/
	mac[0] |= BIT(1);
	if (adapt->iface_id > IFACE_ID1)
		mac[4] ^= BIT(adapt->iface_id);

	memcpy(adapter_mac_addr(adapt), mac, ETH_ALEN);
	/* update mac-address to mbsid-cam cache*/
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_mbid_camid_alloc(adapt, adapter_mac_addr(adapt));
#endif
	RTW_INFO("%s if%d mac_addr : "MAC_FMT"\n", __func__, adapt->iface_id + 1, MAC_ARG(adapter_mac_addr(adapt)));
	rtw_init_wifidirect_addrs(adapt, adapter_mac_addr(adapt), adapter_mac_addr(adapt));
	res = _SUCCESS;

free_drv_sw:
	if (res != _SUCCESS && adapt)
		rtw_free_drv_sw(adapt);
free_adapter:
	if (res != _SUCCESS && adapt) {
		vfree(adapt);
		adapt = NULL;
	}
exit:
	return adapt;
}

void rtw_drv_stop_vir_if(struct adapter *adapt)
{
	struct net_device *pnetdev = NULL;
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;

	if (!adapt)
		return;

	pnetdev = adapt->pnetdev;

	if (check_fwstate(pmlmepriv, _FW_LINKED))
		rtw_disassoc_cmd(adapt, 0, RTW_CMDF_DIRECTLY);

	if (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt)) {
		free_mlme_ap_info(adapt);
	}

	if (adapt->bup) {
		if (adapt->xmitpriv.ack_tx)
			rtw_ack_tx_done(&adapt->xmitpriv, RTW_SCTX_DONE_DRV_STOP);
		rtw_intf_stop(adapt);
		rtw_stop_drv_threads(adapt);
		adapt->bup = false;
	}
	/* cancel timer after thread stop */
	rtw_cancel_all_timer(adapt);
}

void rtw_drv_free_vir_if(struct adapter *adapt)
{
	if (!adapt)
		return;

	RTW_INFO(FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapt));
	rtw_free_drv_sw(adapt);

	/* TODO: use rtw_os_ndevs_deinit instead at the first stage of driver's dev deinit function */
	rtw_os_ndev_free(adapt);

	vfree(adapt);
}

void rtw_drv_stop_vir_ifaces(struct dvobj_priv *dvobj)
{
	int i;

	for (i = VIF_START_ID; i < dvobj->iface_nums; i++)
		rtw_drv_stop_vir_if(dvobj->adapters[i]);
}

void rtw_drv_free_vir_ifaces(struct dvobj_priv *dvobj)
{
	int i;

	for (i = VIF_START_ID; i < dvobj->iface_nums; i++)
		rtw_drv_free_vir_if(dvobj->adapters[i]);
}

void rtw_drv_del_vir_if(struct adapter *adapt)
{
	rtw_drv_stop_vir_if(adapt);
	rtw_drv_free_vir_if(adapt);
}

void rtw_drv_del_vir_ifaces(struct adapter *primary_adapt)
{
	int i;
	struct dvobj_priv *dvobj = primary_adapt->dvobj;

	for (i = VIF_START_ID; i < dvobj->iface_nums; i++)
		rtw_drv_del_vir_if(dvobj->adapters[i]);

}

#endif /*end of CONFIG_CONCURRENT_MODE*/

/* IPv4, IPv6 IP addr notifier */
static int rtw_inetaddr_notifier_call(struct notifier_block *nb,
				      unsigned long action, void *data)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *)data;
	struct net_device *ndev;
	struct mlme_ext_priv *pmlmeext = NULL;
	struct mlme_ext_info *pmlmeinfo = NULL;
	struct adapter *adapter = NULL;

	if (!ifa || !ifa->ifa_dev || !ifa->ifa_dev->dev)
		return NOTIFY_DONE;

	ndev = ifa->ifa_dev->dev;

	if (!is_rtw_ndev(ndev))
		return NOTIFY_DONE;

	adapter = (struct adapter *)rtw_netdev_priv(ifa->ifa_dev->dev);

	if (!adapter)
		return NOTIFY_DONE;

	pmlmeext = &adapter->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;

	switch (action) {
	case NETDEV_UP:
		memcpy(pmlmeinfo->ip_addr, &ifa->ifa_address,
					RTW_IP_ADDR_LEN);
		RTW_DBG("%s[%s]: up IP: %pI4\n", __func__,
					ifa->ifa_label, pmlmeinfo->ip_addr);
	break;
	case NETDEV_DOWN:
		memset(pmlmeinfo->ip_addr, 0, RTW_IP_ADDR_LEN);
		RTW_DBG("%s[%s]: down IP: %pI4\n", __func__,
					ifa->ifa_label, pmlmeinfo->ip_addr);
	break;
	default:
		RTW_DBG("%s: default action\n", __func__);
	break;
	}
	return NOTIFY_DONE;
}

#ifdef CONFIG_IPV6
static int rtw_inet6addr_notifier_call(struct notifier_block *nb,
				       unsigned long action, void *data)
{
	struct inet6_ifaddr *inet6_ifa = data;
	struct net_device *ndev;
	struct pwrctrl_priv *pwrctl = NULL;
	struct mlme_ext_priv *pmlmeext = NULL;
	struct mlme_ext_info *pmlmeinfo = NULL;
	struct adapter *adapter = NULL;

	if (!inet6_ifa || !inet6_ifa->idev || !inet6_ifa->idev->dev)
		return NOTIFY_DONE;

	ndev = inet6_ifa->idev->dev;

	if (!is_rtw_ndev(ndev))
		return NOTIFY_DONE;

	adapter = (struct adapter *)rtw_netdev_priv(inet6_ifa->idev->dev);

	if (!adapter)
		return NOTIFY_DONE;

	pmlmeext =  &adapter->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;
	pwrctl = adapter_to_pwrctl(adapter);

	pmlmeext = &adapter->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;

	switch (action) {
	case NETDEV_UP:
		memcpy(pmlmeinfo->ip6_addr, &inet6_ifa->addr,
					RTW_IPv6_ADDR_LEN);
		RTW_DBG("%s: up IPv6 addrs: %pI6\n", __func__,
					pmlmeinfo->ip6_addr);
			break;
	case NETDEV_DOWN:
		memset(pmlmeinfo->ip6_addr, 0, RTW_IPv6_ADDR_LEN);
		RTW_DBG("%s: down IPv6 addrs: %pI6\n", __func__,
					pmlmeinfo->ip6_addr);
		break;
	default:
		RTW_DBG("%s: default action\n", __func__);
		break;
	}
	return NOTIFY_DONE;
}
#endif

static struct notifier_block rtw_inetaddr_notifier = {
	.notifier_call = rtw_inetaddr_notifier_call
};

#ifdef CONFIG_IPV6
static struct notifier_block rtw_inet6addr_notifier = {
	.notifier_call = rtw_inet6addr_notifier_call
};
#endif

void rtw_inetaddr_notifier_register(void)
{
	RTW_INFO("%s\n", __func__);
	register_inetaddr_notifier(&rtw_inetaddr_notifier);
#ifdef CONFIG_IPV6
	register_inet6addr_notifier(&rtw_inet6addr_notifier);
#endif
}

void rtw_inetaddr_notifier_unregister(void)
{
	RTW_INFO("%s\n", __func__);
	unregister_inetaddr_notifier(&rtw_inetaddr_notifier);
#ifdef CONFIG_IPV6
	unregister_inet6addr_notifier(&rtw_inet6addr_notifier);
#endif
}

static int rtw_os_ndevs_register(struct dvobj_priv *dvobj)
{
	int i, status = _SUCCESS;
	struct registry_priv *regsty = dvobj_to_regsty(dvobj);
	struct adapter *adapter;

	if (rtw_cfg80211_dev_res_register(dvobj) != _SUCCESS) {
		rtw_warn_on(1);
		status = _FAIL;
		goto exit;
	}

	for (i = 0; i < dvobj->iface_nums; i++) {

		if (i >= CONFIG_IFACE_NUMBER) {
			RTW_ERR("%s %d >= CONFIG_IFACE_NUMBER(%d)\n", __func__, i, CONFIG_IFACE_NUMBER);
			rtw_warn_on(1);
			continue;
		}

		adapter = dvobj->adapters[i];
		if (adapter) {
			char *name;

			#ifdef CONFIG_RTW_DYNAMIC_NDEV
			if (!is_primary_adapter(adapter))
				continue;
			#endif

			if (adapter->iface_id == IFACE_ID0)
				name = regsty->ifname;
			else if (adapter->iface_id == IFACE_ID1)
				name = regsty->if2name;
			else
				name = "wlan%d";

			status = rtw_os_ndev_register(adapter, name);

			if (status != _SUCCESS) {
				rtw_warn_on(1);
				break;
			}
		}
	}

	if (status != _SUCCESS) {
		for (; i >= 0; i--) {
			adapter = dvobj->adapters[i];
			if (adapter)
				rtw_os_ndev_unregister(adapter);
		}
	}

	if (status != _SUCCESS)
		rtw_cfg80211_dev_res_unregister(dvobj);
exit:
	return status;
}

void rtw_os_ndevs_unregister(struct dvobj_priv *dvobj)
{
	int i;
	struct adapter *adapter = NULL;

	for (i = 0; i < dvobj->iface_nums; i++) {
		adapter = dvobj->adapters[i];

		if (!adapter)
			continue;

		rtw_os_ndev_unregister(adapter);
	}

	rtw_cfg80211_dev_res_unregister(dvobj);
}

/**
 * rtw_os_ndevs_init - Allocate and register OS layer net devices and relating structures for @dvobj
 * @dvobj: the dvobj on which this function applies
 *
 * Returns:
 * _SUCCESS or _FAIL
 */
int rtw_os_ndevs_init(struct dvobj_priv *dvobj)
{
	int ret = _FAIL;

	if (rtw_os_ndevs_alloc(dvobj) != _SUCCESS)
		goto exit;

	if (rtw_os_ndevs_register(dvobj) != _SUCCESS)
		goto os_ndevs_free;

	ret = _SUCCESS;

os_ndevs_free:
	if (ret != _SUCCESS)
		rtw_os_ndevs_free(dvobj);
exit:
	return ret;
}

/**
 * rtw_os_ndevs_deinit - Unregister and free OS layer net devices and relating structures for @dvobj
 * @dvobj: the dvobj on which this function applies
 */
void rtw_os_ndevs_deinit(struct dvobj_priv *dvobj)
{
	rtw_os_ndevs_unregister(dvobj);
	rtw_os_ndevs_free(dvobj);
}

void netdev_br_init(struct net_device *netdev)
{
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(netdev);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35))
	rcu_read_lock();
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)) */

	/* if(check_fwstate(pmlmepriv, WIFI_STATION_STATE|WIFI_ADHOC_STATE)) */
	{
		/* struct net_bridge	*br = netdev->br_port->br; */ /* ->dev->dev_addr; */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
		if (netdev->br_port)
#else   /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
		if (rcu_dereference(adapter->pnetdev->rx_handler_data))
#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
		{
			struct net_device *br_netdev;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24))
			br_netdev = dev_get_by_name(CONFIG_BR_EXT_BRNAME);
#else	/* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)) */
			struct net *devnet = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26))
			devnet = netdev->nd_net;
#else	/* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)) */
			devnet = dev_net(netdev);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)) */

			br_netdev = dev_get_by_name(devnet, CONFIG_BR_EXT_BRNAME);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)) */

			if (br_netdev) {
				memcpy(adapter->br_mac, br_netdev->dev_addr, ETH_ALEN);
				dev_put(br_netdev);
			} else
				printk("%s()-%d: dev_get_by_name(%s) failed!", __func__, __LINE__, CONFIG_BR_EXT_BRNAME);
		}

		adapter->ethBrExtInfo.addPPPoETag = 1;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35))
	rcu_read_unlock();
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)) */
}

int _netdev_open(struct net_device *pnetdev)
{
	uint status;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

	RTW_INFO(FUNC_NDEV_FMT" , bup=%d\n", FUNC_NDEV_ARG(pnetdev), adapt->bup);

	adapt->netif_up = true;

	#ifdef CONFIG_AUTOSUSPEND
	if (pwrctrlpriv->ps_flag) {
		adapt->net_closed = false;
		goto netdev_open_normal_process;
	}
	#endif

	if (!adapt->bup) {
		rtw_clr_surprise_removed(adapt);
		rtw_clr_drv_stopped(adapt);

		status = rtw_hal_init(adapt);
		if (status == _FAIL) {
			goto netdev_open_error;
		}
		RTW_INFO("MAC Address = "MAC_FMT"\n", MAC_ARG(pnetdev->dev_addr));

		status = rtw_start_drv_threads(adapt);
		if (status == _FAIL) {
			RTW_INFO("Initialize driver software resource Failed!\n");
			goto netdev_open_error;
		}

#ifdef CONFIG_RTW_NAPI
		if(adapt->napi_state == NAPI_DISABLE) {
			napi_enable(&adapt->napi);
			adapt->napi_state = NAPI_ENABLE;
		}
#endif

		rtw_intf_start(adapt);

		rtw_cfg80211_init_wiphy(adapt);
		rtw_cfg80211_init_wdev_data(adapt);

		rtw_led_control(adapt, LED_CTL_NO_LINK);

		adapt->bup = true;
		pwrctrlpriv->bips_processing = false;
	}
	adapt->net_closed = false;

	_set_timer(&adapter_to_dvobj(adapt)->dynamic_chk_timer, 2000);

	rtw_set_pwr_state_check_timer(adapt);

	/* rtw_netif_carrier_on(pnetdev); */ /* call this func when rtw_joinbss_event_callback return success */
	rtw_netif_wake_queue(pnetdev);

	netdev_br_init(pnetdev);
#ifdef CONFIG_CONCURRENT_MODE
	{
		struct adapter *sec_adapter = adapter_to_dvobj(adapt)->adapters[IFACE_ID1];

		#ifndef CONFIG_RTW_DYNAMIC_NDEV
		if (sec_adapter && (!sec_adapter->bup))
			_netdev_vir_if_open(sec_adapter->pnetdev);
		#endif
	}
#endif

	pwrctrlpriv->radio_on_start_time = rtw_get_current_time();
	pwrctrlpriv->pwr_saving_start_time = rtw_get_current_time();
	pwrctrlpriv->pwr_saving_time = 0;
	pwrctrlpriv->on_time = 0;
	pwrctrlpriv->tx_time = 0;
	pwrctrlpriv->rx_time = 0;

	RTW_INFO("-871x_drv - drv_open, bup=%d\n", adapt->bup);

	return 0;

netdev_open_error:

	adapt->bup = false;

#ifdef CONFIG_RTW_NAPI
	if(adapt->napi_state == NAPI_ENABLE) {
		napi_disable(&adapt->napi);
		adapt->napi_state = NAPI_DISABLE;
	}
#endif

	rtw_netif_carrier_off(pnetdev);
	rtw_netif_stop_queue(pnetdev);

	RTW_INFO("-871x_drv - drv_open fail, bup=%d\n", adapt->bup);

	return -1;

}

int netdev_open(struct net_device *pnetdev)
{
	int ret = false;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(adapt);

	if (pwrctrlpriv->bInSuspend) {
		RTW_INFO(" [WARN] "ADPT_FMT" %s  failed, bInSuspend=%d\n", ADPT_ARG(adapt), __func__, pwrctrlpriv->bInSuspend);
		return 0;
	}

	_enter_critical_mutex(&(adapter_to_dvobj(adapt)->hw_init_mutex), NULL);
	if (is_primary_adapter(adapt))
		ret = _netdev_open(pnetdev);
#ifdef CONFIG_CONCURRENT_MODE
	else
		ret = _netdev_vir_if_open(pnetdev);
#endif
	_exit_critical_mutex(&(adapter_to_dvobj(adapt)->hw_init_mutex), NULL);

	return ret;
}

static int  ips_netdrv_open(struct adapter *adapt)
{
	int status = _SUCCESS;
	/* struct pwrctrl_priv	*pwrpriv = adapter_to_pwrctl(adapt); */

	adapt->net_closed = false;

	RTW_INFO("===> %s.........\n", __func__);


	rtw_clr_drv_stopped(adapt);
	/* adapt->bup = true; */

	status = rtw_hal_init(adapt);
	if (status == _FAIL) {
		goto netdev_open_error;
	}
	rtw_intf_start(adapt);

	rtw_set_pwr_state_check_timer(adapt);
	_set_timer(&adapter_to_dvobj(adapt)->dynamic_chk_timer, 2000);

	return _SUCCESS;

netdev_open_error:
	/* adapt->bup = false; */
	RTW_INFO("-ips_netdrv_open - drv_open failure, bup=%d\n", adapt->bup);

	return _FAIL;
}

int rtw_ips_pwr_up(struct adapter *adapt)
{
	int result;
	unsigned long start_time = rtw_get_current_time();
	RTW_INFO("===>  rtw_ips_pwr_up..............\n");
	rtw_reset_drv_sw(adapt);

	result = ips_netdrv_open(adapt);

	rtw_led_control(adapt, LED_CTL_NO_LINK);

	RTW_INFO("<===  rtw_ips_pwr_up.............. in %dms\n", rtw_get_passing_time_ms(start_time));
	return result;

}

void rtw_ips_pwr_down(struct adapter *adapt)
{
	unsigned long start_time = rtw_get_current_time();
	RTW_INFO("===> rtw_ips_pwr_down...................\n");

	adapt->net_closed = true;

	rtw_ips_dev_unload(adapt);
	RTW_INFO("<=== rtw_ips_pwr_down..................... in %dms\n", rtw_get_passing_time_ms(start_time));
}

void rtw_ips_dev_unload(struct adapter *adapt)
{
	RTW_INFO("====> %s...\n", __func__);
	rtw_hal_set_hwreg(adapt, HW_VAR_FIFO_CLEARN_UP, NULL);
	rtw_intf_stop(adapt);

	if (!rtw_is_surprise_removed(adapt))
		rtw_hal_deinit(adapt);
}

static int pm_netdev_open(struct net_device *pnetdev, u8 bnormal)
{
	int status = 0;

	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);

	if (bnormal) {
		_enter_critical_mutex(&(adapter_to_dvobj(adapt)->hw_init_mutex), NULL);
		status = _netdev_open(pnetdev);
		_exit_critical_mutex(&(adapter_to_dvobj(adapt)->hw_init_mutex), NULL);
	} else {
		status = (_SUCCESS == ips_netdrv_open(adapt)) ? (0) : (-1);
	}
	return status;
}

static int netdev_close(struct net_device *pnetdev)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(pnetdev);
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;

	RTW_INFO(FUNC_NDEV_FMT" , bup=%d\n", FUNC_NDEV_ARG(pnetdev), adapt->bup);
	#ifdef CONFIG_AUTOSUSPEND
	if (pwrctl->bInternalAutoSuspend) {
		/* rtw_pwr_wakeup(adapt); */
		if (pwrctl->rf_pwrstate == rf_off)
			pwrctl->ps_flag = true;
	}
	#endif
	adapt->net_closed = true;
	adapt->netif_up = false;
	pmlmepriv->LinkDetectInfo.bBusyTraffic = false;

	if (pwrctl->rf_pwrstate == rf_on) {
		RTW_INFO("(2)871x_drv - drv_close, bup=%d, hw_init_completed=%s\n", adapt->bup, rtw_is_hw_init_completed(adapt) ? "true" : "false");

		/* s1. */
		if (pnetdev)
			rtw_netif_stop_queue(pnetdev);

		/* s2. */
		LeaveAllPowerSaveMode(adapt);
		rtw_disassoc_cmd(adapt, 500, RTW_CMDF_DIRECTLY);
		/* s2-2.  indicate disconnect to os */
		rtw_indicate_disconnect(adapt, 0, false);
		/* s2-3. */
		rtw_free_assoc_resources(adapt, 1);
		/* s2-4. */
		rtw_free_network_queue(adapt, true);
		/* Close LED */
		rtw_led_control(adapt, LED_CTL_POWER_OFF);
	}
	nat25_db_cleanup(adapt);

	if (!rtw_p2p_chk_role(&adapt->wdinfo, P2P_ROLE_DISABLE))
		rtw_p2p_enable(adapt, P2P_ROLE_DISABLE);

	rtw_scan_abort(adapt);
	rtw_cfg80211_wait_scan_req_empty(adapt, 200);
	adapter_wdev_data(adapt)->bandroid_scan = false;

	RTW_INFO("-871x_drv - drv_close, bup=%d\n", adapt->bup);

	return 0;
}

void rtw_ndev_destructor(struct net_device *ndev)
{
	RTW_INFO(FUNC_NDEV_FMT"\n", FUNC_NDEV_ARG(ndev));

	if (ndev->ieee80211_ptr)
		rtw_mfree((u8 *)ndev->ieee80211_ptr, sizeof(struct wireless_dev));
	free_netdev(ndev);
}

#ifdef CONFIG_ARP_KEEP_ALIVE
struct route_info {
	struct in_addr dst_addr;
	struct in_addr src_addr;
	struct in_addr gateway;
	unsigned int dev_index;
};

static void parse_routes(struct nlmsghdr *nl_hdr, struct route_info *rt_info)
{
	struct rtmsg *rt_msg;
	struct rtattr *rt_attr;
	int rt_len;

	rt_msg = (struct rtmsg *) NLMSG_DATA(nl_hdr);
	if ((rt_msg->rtm_family != AF_INET) || (rt_msg->rtm_table != RT_TABLE_MAIN))
		return;

	rt_attr = (struct rtattr *) RTM_RTA(rt_msg);
	rt_len = RTM_PAYLOAD(nl_hdr);

	for (; RTA_OK(rt_attr, rt_len); rt_attr = RTA_NEXT(rt_attr, rt_len)) {
		switch (rt_attr->rta_type) {
		case RTA_OIF:
			rt_info->dev_index = *(int *) RTA_DATA(rt_attr);
			break;
		case RTA_GATEWAY:
			rt_info->gateway.s_addr = *(u_int *) RTA_DATA(rt_attr);
			break;
		case RTA_PREFSRC:
			rt_info->src_addr.s_addr = *(u_int *) RTA_DATA(rt_attr);
			break;
		case RTA_DST:
			rt_info->dst_addr.s_addr = *(u_int *) RTA_DATA(rt_attr);
			break;
		}
	}
}

static int route_dump(u32 *gw_addr , int *gw_index)
{
	int err = 0;
	struct socket *sock;
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;
	struct msghdr msg;
	struct iovec iov;
	struct sockaddr_nl nladdr;
	mm_segment_t oldfs;
	char *pg;
	int size = 0;

	err = sock_create(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE, &sock);
	if (err) {
		printk(": Could not create a datagram socket, error = %d\n", -ENXIO);
		return err;
	}

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = sizeof(req);
	req.nlh.nlmsg_type = RTM_GETROUTE;
	req.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.g.rtgen_family = AF_INET;

	iov.iov_base = &req;
	iov.iov_len = sizeof(req);

	msg.msg_name = &nladdr;
	msg.msg_namelen = sizeof(nladdr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	/* referece:sock_xmit in kernel code
	 * WRITE for sock_sendmsg, READ for sock_recvmsg
	 * third parameter for msg_iovlen
	 * last parameter for iov_len
	 */
	iov_iter_init(&msg.msg_iter, WRITE, &iov, 1, sizeof(req));
#else
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
#endif
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = MSG_DONTWAIT;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
	err = sock_sendmsg(sock, &msg);
#else
	err = sock_sendmsg(sock, &msg, sizeof(req));
#endif
	set_fs(oldfs);

	if (err < 0)
		goto out_sock;

	pg = (char *) __get_free_page(GFP_KERNEL);
	if (!pg) {
		err = -ENOMEM;
		goto out_sock;
	}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
restart:
#endif

	for (;;) {
		struct nlmsghdr *h;

		iov.iov_base = pg;
		iov.iov_len = PAGE_SIZE;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
		iov_iter_init(&msg.msg_iter, READ, &iov, 1, PAGE_SIZE);
#endif

		oldfs = get_fs();
		set_fs(KERNEL_DS);
		err = sock_recvmsg(sock, &msg, PAGE_SIZE, MSG_DONTWAIT);
		set_fs(oldfs);

		if (err < 0)
			goto out_sock_pg;

		if (msg.msg_flags & MSG_TRUNC) {
			err = -ENOBUFS;
			goto out_sock_pg;
		}

		h = (struct nlmsghdr *) pg;

		while (NLMSG_OK(h, err)) {
			struct route_info rt_info;
			if (h->nlmsg_type == NLMSG_DONE) {
				err = 0;
				goto done;
			}

			if (h->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *errm = (struct nlmsgerr *) NLMSG_DATA(h);
				err = errm->error;
				printk("NLMSG error: %d\n", errm->error);
				goto done;
			}

			if (h->nlmsg_type == RTM_GETROUTE)
				printk("RTM_GETROUTE: NLMSG: %d\n", h->nlmsg_type);
			if (h->nlmsg_type != RTM_NEWROUTE) {
				printk("NLMSG: %d\n", h->nlmsg_type);
				err = -EINVAL;
				goto done;
			}

			memset(&rt_info, 0, sizeof(struct route_info));
			parse_routes(h, &rt_info);
			if (!rt_info.dst_addr.s_addr && rt_info.gateway.s_addr && rt_info.dev_index) {
				*gw_addr = rt_info.gateway.s_addr;
				*gw_index = rt_info.dev_index;

			}
			h = NLMSG_NEXT(h, err);
		}

		if (err) {
			printk("!!!Remnant of size %d %d %d\n", err, h->nlmsg_len, h->nlmsg_type);
			err = -EINVAL;
			break;
		}
	}

done:
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	if (!err && req.g.rtgen_family == AF_INET) {
		req.g.rtgen_family = AF_INET6;

		iov.iov_base = &req;
		iov.iov_len = sizeof(req);

		msg.msg_name = &nladdr;
		msg.msg_namelen = sizeof(nladdr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
		iov_iter_init(&msg.msg_iter, WRITE, &iov, 1, sizeof(req));
#else
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
#endif
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = MSG_DONTWAIT;

		oldfs = get_fs();
		set_fs(KERNEL_DS);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
		err = sock_sendmsg(sock, &msg);
#else
		err = sock_sendmsg(sock, &msg, sizeof(req));
#endif
		set_fs(oldfs);

		if (err > 0)
			goto restart;
	}
#endif

out_sock_pg:
	free_page((unsigned long) pg);

out_sock:
	sock_release(sock);
	return err;
}

static int arp_query(unsigned char *haddr, u32 paddr,
		     struct net_device *dev)
{
	struct neighbour *neighbor_entry;
	int	ret = 0;

	neighbor_entry = neigh_lookup(&arp_tbl, &paddr, dev);

	if (neighbor_entry) {
		neighbor_entry->used = jiffies;
		if (neighbor_entry->nud_state & NUD_VALID) {
			memcpy(haddr, neighbor_entry->ha, dev->addr_len);
			ret = 1;
		}
		neigh_release(neighbor_entry);
	}
	return ret;
}

static int get_defaultgw(u32 *ip_addr , char mac[])
{
	int gw_index = 0; /* oif device index */
	struct net_device *gw_dev = NULL; /* oif device */

	route_dump(ip_addr, &gw_index);

	if (!(*ip_addr) || !gw_index) {
		/* RTW_INFO("No default GW\n"); */
		return -1;
	}

	gw_dev = dev_get_by_index(&init_net, gw_index);

	if (!gw_dev) {
		/* RTW_INFO("get Oif Device Fail\n"); */
		return -1;
	}

	if (!arp_query(mac, *ip_addr, gw_dev)) {
		/* RTW_INFO( "arp query failed\n"); */
		dev_put(gw_dev);
		return -1;

	}
	dev_put(gw_dev);

	return 0;
}

int	rtw_gw_addr_query(struct adapter *adapt)
{
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);
	u32 gw_addr = 0; /* default gw address */
	unsigned char gw_mac[32] = {0}; /* default gw mac */
	int i;
	int res;

	res = get_defaultgw(&gw_addr, gw_mac);
	if (!res) {
		pmlmepriv->gw_ip[0] = gw_addr & 0xff;
		pmlmepriv->gw_ip[1] = (gw_addr & 0xff00) >> 8;
		pmlmepriv->gw_ip[2] = (gw_addr & 0xff0000) >> 16;
		pmlmepriv->gw_ip[3] = (gw_addr & 0xff000000) >> 24;
		memcpy(pmlmepriv->gw_mac_addr, gw_mac, 6);
		RTW_INFO("%s Gateway Mac:\t" MAC_FMT "\n", __func__, MAC_ARG(pmlmepriv->gw_mac_addr));
		RTW_INFO("%s Gateway IP:\t" IP_FMT "\n", __func__, IP_ARG(pmlmepriv->gw_ip));
	} else
		RTW_INFO("Get Gateway IP/MAC fail!\n");

	return res;
}
#endif

void rtw_dev_unload(struct adapter * adapt)
{
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);
	struct dvobj_priv *pobjpriv = adapt->dvobj;
	struct debug_priv *pdbgpriv = &pobjpriv->drv_dbg;
	struct cmd_priv *pcmdpriv = &adapt->cmdpriv;

	if (adapt->bup) {
		RTW_INFO("==> "FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapt));

		rtw_set_drv_stopped(adapt);
		if (adapt->xmitpriv.ack_tx)
			rtw_ack_tx_done(&adapt->xmitpriv, RTW_SCTX_DONE_DRV_STOP);
		rtw_intf_stop(adapt);

		#ifdef CONFIG_AUTOSUSPEND
		if (!pwrctl->bInternalAutoSuspend)
		#endif
		{
			rtw_stop_drv_threads(adapt);

			if (ATOMIC_READ(&(pcmdpriv->cmdthd_running))) {
				RTW_ERR("cmd_thread not stop !!\n");
				rtw_warn_on(1);
			}
		}
		/* check the status of IPS */
		if (rtw_hal_check_ips_status(adapt) || pwrctl->rf_pwrstate == rf_off) { /* check HW status and SW state */
			RTW_PRINT("%s: driver in IPS-FWLPS\n", __func__);
			pdbgpriv->dbg_dev_unload_inIPS_cnt++;
		} else
			RTW_PRINT("%s: driver not in IPS\n", __func__);

		if (!rtw_is_surprise_removed(adapt)) {
			rtw_btcoex_IpsNotify(adapt, pwrctl->ips_mode_req);
			rtw_hal_deinit(adapt);
			rtw_set_surprise_removed(adapt);
		}

		adapt->bup = false;

		RTW_INFO("<== "FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapt));
	} else {
		RTW_INFO("%s: bup==false\n", __func__);
	}
	rtw_cancel_all_timer(adapt);
}

int rtw_suspend_free_assoc_resource(struct adapter *adapt)
{
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;
	struct wifidirect_info	*pwdinfo = &adapt->wdinfo;

	RTW_INFO("==> "FUNC_ADPT_FMT" entry....\n", FUNC_ADPT_ARG(adapt));

	if (rtw_chk_roam_flags(adapt, RTW_ROAM_ON_RESUME)) {
		if (check_fwstate(pmlmepriv, WIFI_STATION_STATE) &&
		    check_fwstate(pmlmepriv, _FW_LINKED) &&
		    rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE)) {
			RTW_INFO("%s %s(" MAC_FMT "), length:%d assoc_ssid.length:%d\n", __func__,
				pmlmepriv->cur_network.network.Ssid.Ssid,
				MAC_ARG(pmlmepriv->cur_network.network.MacAddress),
				pmlmepriv->cur_network.network.Ssid.SsidLength,
				pmlmepriv->assoc_ssid.SsidLength);
			rtw_set_to_roam(adapt, 1);
		}
	}

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE) && check_fwstate(pmlmepriv, _FW_LINKED)) {
		rtw_disassoc_cmd(adapt, 0, RTW_CMDF_DIRECTLY);
		/* s2-2.  indicate disconnect to os */
		rtw_indicate_disconnect(adapt, 0, false);
	}
	else if (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt))
		rtw_sta_flush(adapt, true);

	/* s2-3. */
	rtw_free_assoc_resources(adapt, 1);

	/* s2-4. */
#ifdef CONFIG_AUTOSUSPEND
	if (is_primary_adapter(adapt) && (!adapter_to_pwrctl(adapt)->bInternalAutoSuspend))
#endif
		rtw_free_network_queue(adapt, true);

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY)) {
		RTW_PRINT("%s: fw_under_survey\n", __func__);
		rtw_indicate_scan_done(adapt, 1);
		clr_fwstate(pmlmepriv, _FW_UNDER_SURVEY);
	}

	if (check_fwstate(pmlmepriv, _FW_UNDER_LINKING)) {
		RTW_PRINT("%s: fw_under_linking\n", __func__);
		rtw_indicate_disconnect(adapt, 0, false);
	}

	RTW_INFO("<== "FUNC_ADPT_FMT" exit....\n", FUNC_ADPT_ARG(adapt));
	return _SUCCESS;
}

static int rtw_suspend_normal(struct adapter *adapt)
{
	int ret = _SUCCESS;

	RTW_INFO("==> "FUNC_ADPT_FMT" entry....\n", FUNC_ADPT_ARG(adapt));

	rtw_btcoex_SuspendNotify(adapt, BTCOEX_SUSPEND_STATE_SUSPEND);
	rtw_mi_netif_caroff_qstop(adapt);

	rtw_mi_suspend_free_assoc_resource(adapt);

	rtw_led_control(adapt, LED_CTL_POWER_OFF);

	if ((rtw_hal_check_ips_status(adapt))
	    || (adapter_to_pwrctl(adapt)->rf_pwrstate == rf_off))
		RTW_PRINT("%s: ### ERROR #### driver in IPS ####ERROR###!!!\n", __func__);


#ifdef CONFIG_CONCURRENT_MODE
	rtw_set_drv_stopped(adapt);	/*for stop thread*/
	rtw_stop_cmd_thread(adapt);
	rtw_drv_stop_vir_ifaces(adapter_to_dvobj(adapt));
#endif
	rtw_dev_unload(adapt);

	RTW_INFO("<== "FUNC_ADPT_FMT" exit....\n", FUNC_ADPT_ARG(adapt));
	return ret;
}

int rtw_suspend_common(struct adapter *adapt)
{
	struct dvobj_priv *dvobj = adapt->dvobj;
	struct debug_priv *pdbgpriv = &dvobj->drv_dbg;
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);
	int ret = 0;
	unsigned long start_time = rtw_get_current_time();

	RTW_PRINT(" suspend start\n");
	RTW_INFO("==> %s (%s:%d)\n", __func__, current->comm, current->pid);

	pdbgpriv->dbg_suspend_cnt++;

	pwrpriv->bInSuspend = true;

	while (pwrpriv->bips_processing)
		rtw_msleep_os(1);

	if ((!adapt->bup) || RTW_CANNOT_RUN(adapt)) {
		RTW_INFO("%s bup=%d bDriverStopped=%s bSurpriseRemoved = %s\n", __func__
			 , adapt->bup
			 , rtw_is_drv_stopped(adapt) ? "True" : "False"
			, rtw_is_surprise_removed(adapt) ? "True" : "False");
		pdbgpriv->dbg_suspend_error_cnt++;
		goto exit;
	}
	rtw_ps_deny(adapt, PS_DENY_SUSPEND);

	rtw_mi_cancel_all_timer(adapt);
	LeaveAllPowerSaveModeDirect(adapt);

	rtw_ps_deny_cancel(adapt, PS_DENY_SUSPEND);

	if (!rtw_mi_check_status(adapt, MI_AP_MODE)) {
		rtw_suspend_normal(adapt);
	} else if (rtw_mi_check_status(adapt, MI_AP_MODE)) {
		rtw_suspend_normal(adapt);
	}


	RTW_PRINT("rtw suspend success in %d ms\n",
		  rtw_get_passing_time_ms(start_time));

exit:
	RTW_INFO("<===  %s return %d.............. in %dms\n", __func__
		 , ret, rtw_get_passing_time_ms(start_time));

	return ret;
}

static void rtw_mi_resume_process_normal(struct adapter *adapt)
{
	int i;
	struct adapter *iface;
	struct mlme_priv *pmlmepriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		if ((iface) && rtw_is_adapter_up(iface)) {
			pmlmepriv = &iface->mlmepriv;

			if (check_fwstate(pmlmepriv, WIFI_STATION_STATE)) {
				RTW_INFO(FUNC_ADPT_FMT" fwstate:0x%08x - WIFI_STATION_STATE\n", FUNC_ADPT_ARG(iface), get_fwstate(pmlmepriv));

				if (rtw_chk_roam_flags(iface, RTW_ROAM_ON_RESUME))
					rtw_roaming(iface, NULL);

			} else if (MLME_IS_AP(iface) || MLME_IS_MESH(iface)) {
				RTW_INFO(FUNC_ADPT_FMT" %s\n", FUNC_ADPT_ARG(iface), MLME_IS_AP(iface) ? "AP" : "MESH");
				rtw_ap_restore_network(iface);
			} else if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE))
				RTW_INFO(FUNC_ADPT_FMT" fwstate:0x%08x - WIFI_ADHOC_STATE\n", FUNC_ADPT_ARG(iface), get_fwstate(pmlmepriv));
			else
				RTW_INFO(FUNC_ADPT_FMT" fwstate:0x%08x - ???\n", FUNC_ADPT_ARG(iface), get_fwstate(pmlmepriv));
		}
	}
}

static int rtw_resume_process_normal(struct adapter *adapt)
{
	struct net_device *pnetdev;
	struct pwrctrl_priv *pwrpriv;
	struct dvobj_priv *psdpriv;
	struct debug_priv *pdbgpriv;

	int ret = _SUCCESS;

	if (!adapt) {
		ret = -1;
		goto exit;
	}

	pnetdev = adapt->pnetdev;
	pwrpriv = adapter_to_pwrctl(adapt);
	psdpriv = adapt->dvobj;
	pdbgpriv = &psdpriv->drv_dbg;

	RTW_INFO("==> "FUNC_ADPT_FMT" entry....\n", FUNC_ADPT_ARG(adapt));

	rtw_clr_surprise_removed(adapt);
	rtw_hal_disable_interrupt(adapt);

	rtw_mi_reset_drv_sw(adapt);

	pwrpriv->bkeepfwalive = false;

	RTW_INFO("bkeepfwalive(%x)\n", pwrpriv->bkeepfwalive);
	if (pm_netdev_open(pnetdev, true) != 0) {
		ret = -1;
		pdbgpriv->dbg_resume_error_cnt++;
		goto exit;
	}

	rtw_mi_netif_caron_qstart(adapt);

	if (adapt->pid[1] != 0) {
		RTW_INFO("pid[1]:%d\n", adapt->pid[1]);
		rtw_signal_process(adapt->pid[1], SIGUSR2);
	}

	rtw_btcoex_SuspendNotify(adapt, BTCOEX_SUSPEND_STATE_RESUME);

	rtw_mi_resume_process_normal(adapt);

	RTW_INFO("<== "FUNC_ADPT_FMT" exit....\n", FUNC_ADPT_ARG(adapt));

exit:
	return ret;
}

int rtw_resume_common(struct adapter *adapt)
{
	int ret = 0;
	unsigned long start_time = rtw_get_current_time();
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);

	if (!pwrpriv->bInSuspend)
		return 0;

	RTW_PRINT("resume start\n");
	RTW_INFO("==> %s (%s:%d)\n", __func__, current->comm, current->pid);

	if (!rtw_mi_check_status(adapt, WIFI_AP_STATE)) {
		rtw_resume_process_normal(adapt);
	} else if (rtw_mi_check_status(adapt, WIFI_AP_STATE)) {
		rtw_resume_process_normal(adapt);
	}

	if (pwrpriv) {
		pwrpriv->bInSuspend = false;
		pwrpriv->wowlan_in_resume = false;
	}
	RTW_PRINT("%s:%d in %d ms\n", __func__ , ret,
		  rtw_get_passing_time_ms(start_time));


	return ret;
}
