// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2018-2019  Realtek Corporation
 */

#include "main.h"
#include "fw.h"
#include "wow.h"
#include "reg.h"
#include "debug.h"
#include "mac.h"
#include "ps.h"

static void rtw_wow_resume_wakeup_reason(struct rtw_dev *rtwdev)
{
	u8 reason;

	reason = rtw_read8(rtwdev, REG_WOWLAN_WAKE_REASON);

	if (reason == RTW_WOW_RSN_RX_DEAUTH)
		rtw_dbg(rtwdev, RTW_DBG_WOW, "WOW: Rx deauth\n");
	else if (reason == RTW_WOW_RSN_DISCONNECT)
		rtw_dbg(rtwdev, RTW_DBG_WOW, "WOW: AP is off\n");
	else if (reason == RTW_WOW_RSN_RX_MAGIC_PKT)
		rtw_dbg(rtwdev, RTW_DBG_WOW, "WOW: Rx magic packet\n");
	else if (reason == RTW_WOW_RSN_RX_GTK_REKEY)
		rtw_dbg(rtwdev, RTW_DBG_WOW, "WOW: Rx gtk rekey\n");
	else if (reason == RTW_WOW_RSN_RX_PATTERN_MATCH)
		rtw_dbg(rtwdev, RTW_DBG_WOW, "WOW: Rx pattern match packet\n");
	else if (reason == RTW_WOW_RSN_RX_NLO)
		rtw_dbg(rtwdev, RTW_DBG_WOW, "Rx NLO\n");
	else
		rtw_warn(rtwdev, "Unknown wakeup reason %x\n", reason);
}

static void rtw_wow_bb_stop(struct rtw_dev *rtwdev, u8 *txpause)
{
	if (!(rtw_read32_mask(rtwdev, REG_BCNQ_INFO, BIT_MGQ_CPU_EMPTY) != 1))
		rtw_warn(rtwdev, "Wrong status of MGQ_CPU empty!\n");

	*txpause = rtw_read8(rtwdev, REG_TXPAUSE);
	rtw_write8(rtwdev, REG_TXPAUSE, 0xff);
	rtw_write8_clr(rtwdev, REG_SYS_FUNC_EN, BIT_FEN_BB_RSTB);
}

static void rtw_wow_bb_start(struct rtw_dev *rtwdev, u8 *txpause)
{
	rtw_write8_set(rtwdev, REG_SYS_FUNC_EN, BIT_FEN_BB_RSTB);
	rtw_write8(rtwdev, REG_TXPAUSE, *txpause);
}

static bool rtw_wow_check_fw_status(struct rtw_dev *rtwdev, bool wow_enable)
{
	bool res = false;

	/* wait 100ms for wow firmware to start or stop */
	msleep(100);
	if (wow_enable) {
		if (rtw_read32_mask(rtwdev, REG_MCUTST_II, 0xff000000) == 0)
			res = true;
	} else {
		if (rtw_read32_mask(rtwdev, REG_FE1IMR, BIT_FS_RXDONE) == 0 &&
		    rtw_read32_mask(rtwdev, REG_RXPKT_NUM, BIT_RW_RELEASE) == 0)
			res = true;
	}

	if (!res)
		rtw_err(rtwdev, "failed to check wow status %s\n",
			wow_enable ? "enabled" : "disabled");

	return res;
}

static void rtw_wow_fw_security_type_iter(struct ieee80211_hw *hw,
					  struct ieee80211_vif *vif,
					  struct ieee80211_sta *sta,
					  struct ieee80211_key_conf *key,
					  void *data)
{
	struct rtw_fw_key_type_iter_data *iter_data = data;
	struct rtw_dev *rtwdev = hw->priv;
	u8 hw_key_type;

	if (vif != rtwdev->wow.wow_vif)
		return;

	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		hw_key_type = RTW_CAM_WEP40;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		hw_key_type = RTW_CAM_WEP104;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		hw_key_type = RTW_CAM_TKIP;
		key->flags |= IEEE80211_KEY_FLAG_GENERATE_MMIC;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		hw_key_type = RTW_CAM_AES;
		key->flags |= IEEE80211_KEY_FLAG_SW_MGMT_TX;
		break;
	default:
		rtw_warn(rtwdev, "Unsupported key type");
		hw_key_type = 0;
		break;
	}

	if (sta)
		iter_data->pairwise_key_type = hw_key_type;
	else
		iter_data->group_key_type = hw_key_type;
}

static void rtw_wow_fw_security_type(struct rtw_dev *rtwdev)
{
	struct rtw_fw_key_type_iter_data data = {};
	struct ieee80211_vif *wow_vif = rtwdev->wow.wow_vif;

	data.rtwdev = rtwdev;

	rtw_iterate_keys(rtwdev, wow_vif,
			 rtw_wow_fw_security_type_iter, &data);

	rtw_dbg(rtwdev, RTW_DBG_WOW, "pairwise_key: %d group_key: %d\n",
		data.pairwise_key_type, data.group_key_type);

	rtw_fw_set_aoac_global_info_cmd(rtwdev, data.pairwise_key_type,
					data.group_key_type);
}

static void __rtw_wow_pattern_write_cam(struct rtw_dev *rtwdev, u8 addr,
					u32 wdata)
{
	bool ret;

	rtw_write32(rtwdev, REG_WKFMCAM_RWD, wdata);
	rtw_write32(rtwdev, REG_WKFMCAM_CMD, BIT_WKFCAM_POLLING_V1 |
		    BIT_WKFCAM_WE | BIT_WKFCAM_ADDR_V2(addr));

	ret = check_hw_ready(rtwdev, REG_WKFMCAM_CMD, BIT_WKFCAM_POLLING_V1, 0);
	if (!ret)
		rtw_err(rtwdev, "failed to write pattern cam\n");
}

static void __rtw_wow_pattern_write_cam_ent(struct rtw_dev *rtwdev, u8 id,
					    struct  rtw_wow_pattern *context)
{
	int i;
	u8 addr;
	u32 wdata;

	for (i = 0; i < RTW_MAX_PATTERN_MASK_SIZE / 4; i++) {
		addr = (id << 3) + i;
		wdata = context->mask[i * 4];
		wdata |= context->mask[i * 4 + 1] << 8;
		wdata |= context->mask[i * 4 + 2] << 16;
		wdata |= context->mask[i * 4 + 3] << 24;
		__rtw_wow_pattern_write_cam(rtwdev, addr, wdata);
	}

	wdata = context->crc;
	addr = (id << 3) + RTW_MAX_PATTERN_MASK_SIZE / 4;

	switch (context->type) {
	case RTW_PATTERN_BROADCAST:
		wdata |= BIT_BC | BIT_VALID;
		break;
	case RTW_PATTERN_MULTICAST:
		wdata |= BIT_MC | BIT_VALID;
		break;
	case RTW_PATTERN_UNICAST:
		wdata |= BIT_UC | BIT_VALID;
		break;
	default:
		break;
	}
	__rtw_wow_pattern_write_cam(rtwdev, addr, wdata);
}

/* RTK internal CRC16 for Pattern Cam */
static u16 __rtw_cal_crc16(u8 data, u16 crc)
{
	u8 shift_in, data_bit;
	u8 crc_bit4, crc_bit11, crc_bit15;
	u16 crc_result;
	int index;

	for (index = 0; index < 8; index++) {
		crc_bit15 = ((crc & BIT(15)) ? 1 : 0);
		data_bit = (data & (BIT(0) << index) ? 1 : 0);
		shift_in = crc_bit15 ^ data_bit;

		crc_result = crc << 1;

		if (shift_in == 0)
			crc_result &= (~BIT(0));
		else
			crc_result |= BIT(0);

		crc_bit11 = ((crc & BIT(11)) ? 1 : 0) ^ shift_in;

		if (crc_bit11 == 0)
			crc_result &= (~BIT(12));
		else
			crc_result |= BIT(12);

		crc_bit4 = ((crc & BIT(4)) ? 1 : 0) ^ shift_in;

		if (crc_bit4 == 0)
			crc_result &= (~BIT(5));
		else
			crc_result |= BIT(5);

		crc = crc_result;
	}
	return crc;
}

static u16 rtw_calc_crc(u8 *pdata, int length)
{
	u16 crc = 0xffff;
	int i;

	for (i = 0; i < length; i++)
		crc = __rtw_cal_crc16(pdata[i], crc);

	/* get 1' complement */
	return ~crc;
}

static void rtw_wow_pattern_generate(struct rtw_dev *rtwdev,
				     struct rtw_vif *rtwvif,
				     struct cfg80211_pkt_pattern *patterns,
				     struct rtw_wow_pattern *rtw_pattern)
{
	int len = 0;
	const u8 *mask;
	const u8 *pattern;
	u8 mask_hw[RTW_MAX_PATTERN_MASK_SIZE] = {0};
	u8 content[RTW_MAX_PATTERN_SIZE] = {0};
	u8 mac_addr[ETH_ALEN] = {0};
	u8 mask_len = 0;
	u16 count = 0;
	int i;

	pattern = patterns->pattern;
	len = patterns->pattern_len;
	mask = patterns->mask;

	ether_addr_copy(mac_addr, rtwvif->mac_addr);
	memset(rtw_pattern, 0, sizeof(struct rtw_wow_pattern));

	mask_len = DIV_ROUND_UP(len, 8);

	if (is_broadcast_ether_addr(pattern))
		rtw_pattern->type = RTW_PATTERN_BROADCAST;
	else if (is_multicast_ether_addr(pattern))
		rtw_pattern->type = RTW_PATTERN_MULTICAST;
	else if (ether_addr_equal(pattern, mac_addr))
		rtw_pattern->type = RTW_PATTERN_UNICAST;
	else
		rtw_pattern->type = RTW_PATTERN_INVALID;

	/* translate mask from os to mask for hw
	 * pattern from OS uses 'ethenet frame', like this:
	 * |    6   |    6   |   2  |     20    |  Variable  |  4  |
	 * |--------+--------+------+-----------+------------+-----|
	 * |    802.3 Mac Header    | IP Header | TCP Packet | FCS |
	 * |   DA   |   SA   | Type |
	 *
	 * BUT, packet catched by our HW is in '802.11 frame', begin from LLC
	 * |     24 or 30      |    6   |   2  |     20    |  Variable  |  4  |
	 * |-------------------+--------+------+-----------+------------+-----|
	 * | 802.11 MAC Header |       LLC     | IP Header | TCP Packet | FCS |
	 *		       | Others | Tpye |
	 *
	 * Therefore, we need translate mask_from_OS to mask_to_hw.
	 * We should left-shift mask by 6 bits, then set the new bit[0~5] = 0,
	 * because new mask[0~5] means 'SA', but our HW packet begins from LLC,
	 * bit[0~5] corresponds to first 6 Bytes in LLC, they just don't match.
	 */

	/* Shift 6 bits */
	for (i = 0; i < mask_len - 1; i++) {
		mask_hw[i] = mask[i] >> 6;
		mask_hw[i] |= (mask[i + 1] & 0x3F) << 2;
	}

	mask_hw[i] = (mask[i] >> 6) & 0x3F;
	/* Set bit 0-5 to zero */
	mask_hw[0] &= ~(0x3F);

	memcpy(rtw_pattern->mask, mask_hw, RTW_MAX_PATTERN_MASK_SIZE);

	/* To get the wake up pattern from the mask.
	 * We do not count first 12 bits which means
	 * DA[6] and SA[6] in the pattern to match HW design.
	 */
	count = 0;
	for (i = 12; i < len; i++) {
		if ((mask[i / 8] >> (i % 8)) & 0x01) {
			content[count] = pattern[i];
			count++;
		}
	}

	rtw_pattern->crc = rtw_calc_crc(content, count);

	if (rtw_pattern->crc != 0) {
		if (rtw_pattern->type == RTW_PATTERN_INVALID)
			rtw_pattern->type = RTW_PATTERN_VALID;
	}
}

static void __rtw_wow_pattern_clear_cam(struct rtw_dev *rtwdev)
{
	bool ret;

	rtw_write32(rtwdev, REG_WKFMCAM_CMD, BIT_WKFCAM_POLLING_V1 |
		    BIT_WKFCAM_CLR_V1);

	ret = check_hw_ready(rtwdev, REG_WKFMCAM_CMD, BIT_WKFCAM_POLLING_V1, 0);
	if (!ret)
		rtw_err(rtwdev, "failed to clean pattern cam\n");
}

static void rtw_wow_enable_pattern(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	struct rtw_wow_pattern *rtw_pattern = rtw_wow->patterns;
	int i = 0;

	for (i = 0; i < rtw_wow->pattern_cnt; i++)
		__rtw_wow_pattern_write_cam_ent(rtwdev, i, rtw_pattern + i);
}

static void rtw_wow_disable_pattern(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;

	__rtw_wow_pattern_clear_cam(rtwdev);

	rtw_wow->pattern_cnt = 0;
	memset(rtw_wow->patterns, 0, sizeof(rtw_wow->patterns));
}

static void rtw_wow_fw_enable(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;

	if (rtw_wow->suspend_mode == RTW_SUSPEND_LINKED) {
		rtw_send_rsvd_page_h2c(rtwdev);
		rtw_wow_enable_pattern(rtwdev);
		rtw_wow_fw_security_type(rtwdev);
		rtw_fw_set_disconnect_decision_cmd(rtwdev, true);
		rtw_fw_set_keep_alive_cmd(rtwdev, true);
	} else {
		rtw_fw_set_nlo_info(rtwdev, true);
		rtw_fw_update_pkt_probe_req(rtwdev, NULL);
		rtw_fw_channel_switch(rtwdev, true);
	}

	rtw_fw_set_wowlan_ctrl_cmd(rtwdev, true);
	rtw_fw_set_remote_wake_ctrl_cmd(rtwdev, true);
	rtw_wow_check_fw_status(rtwdev, true);
}

static void rtw_wow_fw_disable(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;

	if (rtw_wow->suspend_mode == RTW_SUSPEND_LINKED) {
		rtw_fw_set_disconnect_decision_cmd(rtwdev, false);
		rtw_fw_set_keep_alive_cmd(rtwdev, false);
		rtw_wow_disable_pattern(rtwdev);
	} else {
		rtw_fw_channel_switch(rtwdev, false);
		rtw_fw_set_nlo_info(rtwdev, false);
	}

	rtw_fw_set_wowlan_ctrl_cmd(rtwdev, false);
	rtw_fw_set_remote_wake_ctrl_cmd(rtwdev, false);
	rtw_wow_check_fw_status(rtwdev, false);
}

static void rtw_wow_avoid_reset_mac(struct rtw_dev *rtwdev)
{
	/* When resuming from wowlan mode, some host issue signal
	 * (PCIE: PREST, USB: SE0RST) to device, and lead to reset
	 * mac core. If it happens, the connection to AP will be lost.
	 * Setting REG_RSV_CTRL Register can avoid this process.
	 */
	switch (rtw_hci_type(rtwdev)) {
	case RTW_HCI_TYPE_PCIE:
	case RTW_HCI_TYPE_USB:
		rtw_write8(rtwdev, REG_RSV_CTRL, BIT_WLOCK_1C_B6);
		rtw_write8(rtwdev, REG_RSV_CTRL,
			   BIT_WLOCK_1C_B6 | BIT_R_DIS_PRST);
		break;
	default:
		rtw_warn(rtwdev, "Unsupported hci type to disable reset MAC");
		break;
	}
}

static void rtw_wow_fw_media_status_iter(void *data, struct ieee80211_sta *sta)
{
	struct rtw_sta_info *si = (struct rtw_sta_info *)sta->drv_priv;
	struct rtw_fw_media_status_iter_data *iter_data = data;
	struct rtw_dev *rtwdev = iter_data->rtwdev;

	rtw_fw_media_status_report(rtwdev, si->mac_id, iter_data->connect);
}

static void rtw_wow_fw_media_status(struct rtw_dev *rtwdev, bool connect)
{
	struct rtw_fw_media_status_iter_data data;

	data.rtwdev = rtwdev;
	data.connect = connect;

	rtw_iterate_stas_atomic(rtwdev, rtw_wow_fw_media_status_iter, &data);
}

static int rtw_wow_dl_fw_rsvd(struct rtw_dev *rtwdev)
{
	struct ieee80211_vif *wow_vif = rtwdev->wow.wow_vif;

	rtw_fw_config_rsvd_page(rtwdev);

	return rtw_fw_download_rsvd_page(rtwdev, wow_vif);
}

static int rtw_wow_swap_fw(struct rtw_dev *rtwdev, bool wow)
{
	struct rtw_fw_state *fw;
	int ret;

	if (wow)
		fw = &rtwdev->wow_fw;
	else
		fw = &rtwdev->fw;

	rtw_hci_setup(rtwdev);

	ret = rtw_download_firmware(rtwdev, fw);
	if (ret) {
		rtw_err(rtwdev, "failed to download %s firmware\n",
			wow ? "wow" : "normal");
		return ret;
	}
	rtw_wow_fw_media_status(rtwdev, true);

	return rtw_wow_dl_fw_rsvd(rtwdev);
}

static void rtw_wow_set_param(struct rtw_dev *rtwdev,
			      struct cfg80211_wowlan *wowlan)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	struct rtw_wow_pattern *rtw_pattern = rtw_wow->patterns;
	struct ieee80211_vif *wow_vif = rtw_wow->wow_vif;
	struct rtw_vif *rtwvif = (struct rtw_vif *)wow_vif->drv_priv;
	int i;

	rtw_dbg(rtwdev, RTW_DBG_WOW, "WOW: Pattern Count %d\n",
		wowlan->n_patterns);

	rtw_wow->any = wowlan->any;
	rtw_wow->disconnect = wowlan->disconnect;
	rtw_wow->magic_pkt = wowlan->magic_pkt;
	rtw_wow->gtk_rekey_failure = wowlan->gtk_rekey_failure;

	if (!wowlan->n_patterns || !wowlan->patterns)
		return;

	rtw_wow->pattern_cnt = wowlan->n_patterns;
	for (i = 0; i < wowlan->n_patterns; i++)
		rtw_wow_pattern_generate(rtwdev, rtwvif, wowlan->patterns + i,
					 rtw_pattern + i);
}

static void rtw_wow_suspend_normal_mode(struct rtw_dev *rtwdev,
					u8 *txpause_status)
{
	rtw_hci_stop(rtwdev);

	/* wait 100ms for wow firmware to stop TX */
	msleep(100);
	rtw_wow_bb_stop(rtwdev, txpause_status);

	rtw_hci_setup(rtwdev);
}

static void rtw_wow_sleep(struct rtw_dev *rtwdev, u8 *txpause_status)
{
	rtw_wow_fw_enable(rtwdev);
	rtw_wow_bb_start(rtwdev, txpause_status);
	rtw_wow_avoid_reset_mac(rtwdev);
}

static int rtw_wow_suspend_start(struct rtw_dev *rtwdev)
{
	int ret = 0;
	u8 txpause_status;

	rtw_wow_suspend_normal_mode(rtwdev, &txpause_status);

	ret = rtw_wow_swap_fw(rtwdev, true);
	if (ret)
		return ret;

	rtw_wow_sleep(rtwdev, &txpause_status);

	return ret;
}

static int rtw_wow_suspend_linked(struct rtw_dev *rtwdev,
				  struct cfg80211_wowlan *wowlan)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	struct ieee80211_vif *wow_vif = rtw_wow->wow_vif;
	struct rtw_vif *rtwvif = (struct rtw_vif *)wow_vif->drv_priv;
	int ret = 0;

	mutex_lock(&rtwdev->mutex);

	cancel_delayed_work_sync(&rtwdev->watch_dog_work);
	cancel_delayed_work_sync(&rtwdev->lps_work);
	cancel_work_sync(&rtwdev->c2h_work);

	rtw_leave_lps(rtwdev);

	rtw_wow_set_param(rtwdev, wowlan);
	rtw_wow->suspend_mode = RTW_SUSPEND_LINKED;

	ret = rtw_wow_suspend_start(rtwdev);
	if (ret) {
		rtw_wow->suspend_mode = RTW_SUSPEND_IDLE;
		goto unlock;
	}

	rtw_enter_lps(rtwdev, rtwvif->port);

unlock:
	mutex_unlock(&rtwdev->mutex);
	return ret;
}

static int rtw_wow_suspend_no_link(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	int ret;

	mutex_lock(&rtwdev->mutex);

	cancel_delayed_work_sync(&rtwdev->watch_dog_work);
	cancel_work_sync(&rtwdev->c2h_work);

	if (test_bit(RTW_FLAG_INACTIVE_PS, rtwdev->flags)) {
		rtw_leave_ips(rtwdev);
		rtw_wow->enter_ips = true;
	}

	rtw_wow->suspend_mode = RTW_SUSPEND_NO_LINK;

	ret = rtw_wow_suspend_start(rtwdev);
	if (ret && rtw_wow->enter_ips) {
		rtw_wow->suspend_mode = RTW_SUSPEND_IDLE;
		rtw_enter_ips(rtwdev);
		rtw_wow->enter_ips = false;
		goto unlock;
	}

	/* firmware enter/leave deep ps if deep ps is supported */
	if (rtw_fw_lps_deep_mode)
		set_bit(RTW_FLAG_LEISURE_PS_DEEP, rtwdev->flags);

unlock:
	mutex_unlock(&rtwdev->mutex);
	return ret;
}

static void rtw_wow_awake(struct rtw_dev *rtwdev, u8 *txpause_status)
{
	rtw_wow_fw_disable(rtwdev);
	rtw_wow_bb_stop(rtwdev, txpause_status);
	rtw_hci_setup(rtwdev);
}

static void rtw_wow_resume_normal_mode(struct rtw_dev *rtwdev,
				       u8 *txpause_status)
{
	rtw_wow_bb_start(rtwdev, txpause_status);
	rtw_hci_start(rtwdev);
	ieee80211_queue_delayed_work(rtwdev->hw, &rtwdev->watch_dog_work,
				     RTW_WATCH_DOG_DELAY_TIME);
}

static int rtw_wow_resume_start(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	u8 txpause_status;
	int ret = 0;

	rtw_wow_awake(rtwdev, &txpause_status);

	ret = rtw_wow_swap_fw(rtwdev, false);
	if (ret)
		return ret;

	rtw_wow_resume_normal_mode(rtwdev, &txpause_status);

	rtw_wow->wow_vif = NULL;
	rtw_wow->suspend_mode = RTW_SUSPEND_IDLE;

	return ret;
}

static int rtw_wow_resume_linked(struct rtw_dev *rtwdev)
{
	int ret = 0;

	mutex_lock(&rtwdev->mutex);

	rtw_leave_lps(rtwdev);

	rtw_wow_resume_wakeup_reason(rtwdev);

	ret = rtw_wow_resume_start(rtwdev);

	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static int rtw_wow_resume_no_link(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	int ret = 0;

	mutex_lock(&rtwdev->mutex);

	if (rtw_fw_lps_deep_mode)
		rtw_leave_lps_deep(rtwdev);

	rtw_wow_resume_wakeup_reason(rtwdev);

	ret = rtw_wow_resume_start(rtwdev);

	if (rtw_wow->enter_ips) {
		rtw_enter_ips(rtwdev);
		rtw_wow->enter_ips = false;
	}

	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static void rtw_wow_suspend_iter(void *data, u8 *mac, struct ieee80211_vif *vif)
{
	struct rtw_dev *rtwdev = data;
	struct rtw_vif *rtwvif = (struct rtw_vif *)vif->drv_priv;

	if (rtwdev->wow.wow_vif || vif->type != NL80211_IFTYPE_STATION)
		return;

	if (rtwvif->net_type == RTW_NET_MGD_LINKED)
		rtwdev->wow.wow_vif = vif;
}

int rtw_wow_suspend(struct rtw_dev *rtwdev, struct cfg80211_wowlan *wowlan)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;

	if (rtw_wow->pno_req.inited && rtw_wow->wow_vif)
		return rtw_wow_suspend_no_link(rtwdev);

	rtw_iterate_vifs_atomic(rtwdev, rtw_wow_suspend_iter, rtwdev);
	if (rtw_wow->wow_vif)
		return rtw_wow_suspend_linked(rtwdev, wowlan);

	return -EPERM;
}

int rtw_wow_resume(struct rtw_dev *rtwdev)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	int ret = 0;

	if (rtw_wow->suspend_mode == RTW_SUSPEND_LINKED)
		ret = rtw_wow_resume_linked(rtwdev);
	else if (rtw_wow->suspend_mode == RTW_SUSPEND_NO_LINK)
		ret = rtw_wow_resume_no_link(rtwdev);

	return ret;
}

int rtw_wow_store_scan_param(struct rtw_dev *rtwdev, struct ieee80211_vif *vif,
			     struct cfg80211_sched_scan_request *req)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	struct rtw_pno_request *rtw_pno_req = &rtw_wow->pno_req;
	int size;
	int i;

	if (rtw_pno_req->inited)
		return -EBUSY;

	if (!req->n_match_sets || !req->match_sets)
		return -EINVAL;

	size = sizeof(struct cfg80211_match_set) * req->n_match_sets;
	rtw_pno_req->match_sets = kmalloc(size, GFP_KERNEL);

	if (!rtw_pno_req->match_sets)
		return -ENOMEM;

	rtw_pno_req->match_set_cnt = req->n_match_sets;
	memcpy(rtw_pno_req->match_sets, req->match_sets, size);

	size = sizeof(struct ieee80211_channel) * req->n_channels;
	rtw_pno_req->channels = kmalloc(size, GFP_KERNEL);

	if (!rtw_pno_req->channels) {
		kfree(rtw_pno_req->match_sets);
		return -ENOMEM;
	}

	rtw_pno_req->channel_cnt = req->n_channels;

	for (i = 0 ; i < rtw_pno_req->channel_cnt; i++) {
		memcpy(rtw_pno_req->channels + i, *(req->channels + i),
		       sizeof(struct ieee80211_channel));
	}

	rtw_wow->wow_vif = vif;
	rtw_pno_req->inited = true;

	return 0;
}

int rtw_wow_clear_scan_param(struct rtw_dev *rtwdev, struct ieee80211_vif *vif)
{
	struct rtw_wow_param *rtw_wow = &rtwdev->wow;
	struct rtw_pno_request *pno_req = &rtw_wow->pno_req;

	if (!pno_req->inited)
		return -EINVAL;

	kfree(pno_req->match_sets);
	kfree(pno_req->channels);
	pno_req->match_set_cnt = 0;
	pno_req->channel_cnt = 0;
	pno_req->inited = false;

	return 0;
}
