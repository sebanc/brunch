// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#define _RTW_DEBUG_C_

#include <drv_types.h>
#include <hal_data.h>

#ifdef CONFIG_RTW_DEBUG
static const char *rtw_log_level_str[] = {
	"_DRV_NONE_ = 0",
	"_DRV_ALWAYS_ = 1",
	"_DRV_ERR_ = 2",
	"_DRV_WARNING_ = 3",
	"_DRV_INFO_ = 4",
	"_DRV_DEBUG_ = 5",
	"_DRV_MAX_ = 6",
};
#endif

#ifdef CONFIG_DEBUG_RTL871X
	u64 GlobalDebugComponents = 0;
#endif /* CONFIG_DEBUG_RTL871X */

void dump_drv_version(void *sel)
{
	RTW_PRINT_SEL(sel, "%s %s\n", DRV_NAME, DRIVERVERSION);
}

void dump_drv_cfg(struct seq_file *sel)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
	char *kernel_version = utsname()->release;

	RTW_PRINT_SEL(sel, "\nKernel Version: %s\n", kernel_version);
#endif

	RTW_PRINT_SEL(sel, "Driver Version: %s\n", DRIVERVERSION);
	RTW_PRINT_SEL(sel, "------------------------------------------------\n");
	RTW_PRINT_SEL(sel, "CFG80211\n");
	RTW_PRINT_SEL(sel, "RTW_USE_CFG80211_STA_EVENT\n");
	#ifdef CONFIG_RADIO_WORK
	RTW_PRINT_SEL(sel, "CONFIG_RADIO_WORK\n");
	#endif

	RTW_PRINT_SEL(sel, "DBG:%d\n", DBG);
#ifdef CONFIG_RTW_DEBUG
	RTW_PRINT_SEL(sel, "CONFIG_RTW_DEBUG\n");
#endif

#ifdef CONFIG_CONCURRENT_MODE
	RTW_PRINT_SEL(sel, "CONFIG_CONCURRENT_MODE\n");
#endif

	RTW_PRINT_SEL(sel, "CONFIG_POWER_SAVING\n");

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	RTW_PRINT_SEL(sel, "LOAD_PHY_PARA_FROM_FILE - REALTEK_CONFIG_PATH=%s\n", REALTEK_CONFIG_PATH);
	#if defined(REALTEK_CONFIG_PATH_WITH_IC_NAME_FOLDER)
	RTW_PRINT_SEL(sel, "LOAD_PHY_PARA_FROM_FILE - REALTEK_CONFIG_PATH_WITH_IC_NAME_FOLDER\n");
	#endif

/* configurations about TX power */
#ifdef CONFIG_CALIBRATE_TX_POWER_BY_REGULATORY
	RTW_PRINT_SEL(sel, "CONFIG_CALIBRATE_TX_POWER_BY_REGULATORY\n");
#endif
#endif
	RTW_PRINT_SEL(sel, "CONFIG_TXPWR_BY_RATE_EN=%d\n", CONFIG_TXPWR_BY_RATE_EN);
	RTW_PRINT_SEL(sel, "CONFIG_TXPWR_LIMIT_EN=%d\n", CONFIG_TXPWR_LIMIT_EN);


#ifdef CONFIG_DISABLE_ODM
	RTW_PRINT_SEL(sel, "CONFIG_DISABLE_ODM\n");
#endif

	RTW_PRINT_SEL(sel, "CONFIG_MINIMAL_MEMORY_USAGE\n");

	RTW_PRINT_SEL(sel, "CONFIG_RTW_ADAPTIVITY_EN = %d\n", CONFIG_RTW_ADAPTIVITY_EN);
#if (CONFIG_RTW_ADAPTIVITY_EN)
	RTW_PRINT_SEL(sel, "ADAPTIVITY_MODE = %s\n", (CONFIG_RTW_ADAPTIVITY_MODE) ? "carrier_sense" : "normal");
#endif

#ifdef CONFIG_RTW_80211R
	RTW_PRINT_SEL(sel, "CONFIG_RTW_80211R\n");
#endif

#ifdef CONFIG_RTW_WIFI_HAL
	RTW_PRINT_SEL(sel, "CONFIG_RTW_WIFI_HAL\n");
#endif
	RTW_PRINT_SEL(sel, "CONFIG_PREALLOC_RECV_SKB\n");

	RTW_PRINT_SEL(sel, "\n=== XMIT-INFO ===\n");
	RTW_PRINT_SEL(sel, "NR_XMITFRAME = %d\n", NR_XMITFRAME);
	RTW_PRINT_SEL(sel, "NR_XMITBUFF = %d\n", NR_XMITBUFF);
	RTW_PRINT_SEL(sel, "MAX_XMITBUF_SZ = %d\n", MAX_XMITBUF_SZ);
	RTW_PRINT_SEL(sel, "NR_XMIT_EXTBUFF = %d\n", NR_XMIT_EXTBUFF);
	RTW_PRINT_SEL(sel, "MAX_XMIT_EXTBUF_SZ = %d\n", MAX_XMIT_EXTBUF_SZ);
	RTW_PRINT_SEL(sel, "MAX_CMDBUF_SZ = %d\n", MAX_CMDBUF_SZ);

	RTW_PRINT_SEL(sel, "\n=== RECV-INFO ===\n");
	RTW_PRINT_SEL(sel, "NR_RECVFRAME = %d\n", NR_RECVFRAME);
	RTW_PRINT_SEL(sel, "NR_RECVBUFF = %d\n", NR_RECVBUFF);
	RTW_PRINT_SEL(sel, "MAX_RECVBUF_SZ = %d\n", MAX_RECVBUF_SZ);

}

void dump_log_level(void *sel)
{
#ifdef CONFIG_RTW_DEBUG
	int i;

	RTW_PRINT_SEL(sel, "drv_log_level:%d\n", rtw_drv_log_level);
	for (i = 0; i <= _DRV_MAX_; i++) {
		if (rtw_log_level_str[i])
			RTW_PRINT_SEL(sel, "%c %s = %d\n",
				(rtw_drv_log_level == i) ? '+' : ' ', rtw_log_level_str[i], i);
	}
#else
	RTW_PRINT_SEL(sel, "CONFIG_RTW_DEBUG is disabled\n");
#endif
}

void mac_reg_dump(void *sel, struct adapter *adapter)
{
	int i, j = 1;

	RTW_PRINT_SEL(sel, "======= MAC REG =======\n");

	for (i = 0x0; i < 0x800; i += 4) {
		if (j % 4 == 1)
			RTW_PRINT_SEL(sel, "0x%04x", i);
		_RTW_PRINT_SEL(sel, " 0x%08x ", rtw_read32(adapter, i));
		if ((j++) % 4 == 0)
			_RTW_PRINT_SEL(sel, "\n");
	}
}

void bb_reg_dump(void *sel, struct adapter *adapter)
{
	int i, j = 1;

	RTW_PRINT_SEL(sel, "======= BB REG =======\n");
	for (i = 0x800; i < 0x1000; i += 4) {
		if (j % 4 == 1)
			RTW_PRINT_SEL(sel, "0x%04x", i);
		_RTW_PRINT_SEL(sel, " 0x%08x ", rtw_read32(adapter, i));
		if ((j++) % 4 == 0)
			_RTW_PRINT_SEL(sel, "\n");
	}
}

void bb_reg_dump_ex(void *sel, struct adapter *adapter)
{
	int i;

	RTW_PRINT_SEL(sel, "======= BB REG =======\n");
	for (i = 0x800; i < 0x1000; i += 4) {
		RTW_PRINT_SEL(sel, "0x%04x", i);
		_RTW_PRINT_SEL(sel, " 0x%08x ", rtw_read32(adapter, i));
		_RTW_PRINT_SEL(sel, "\n");
	}
}

void rf_reg_dump(void *sel, struct adapter *adapter)
{
	int i, j = 1, path;
	u32 value;
	u8 rf_type = 0;
	u8 path_nums = 0;

	rtw_hal_get_hwreg(adapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	if ((RF_1T2R == rf_type) || (RF_1T1R == rf_type))
		path_nums = 1;
	else
		path_nums = 2;

	RTW_PRINT_SEL(sel, "======= RF REG =======\n");

	for (path = 0; path < path_nums; path++) {
		RTW_PRINT_SEL(sel, "RF_Path(%x)\n", path);
		for (i = 0; i < 0x100; i++) {
			value = rtw_hal_read_rfreg(adapter, path, i, 0xffffffff);
			if (j % 4 == 1)
				RTW_PRINT_SEL(sel, "0x%02x ", i);
			_RTW_PRINT_SEL(sel, " 0x%08x ", value);
			if ((j++) % 4 == 0)
				_RTW_PRINT_SEL(sel, "\n");
		}
	}
}

void rtw_sink_rtp_seq_dbg(struct adapter *adapter, u8 *ehdr_pos)
{
	struct recv_priv *precvpriv = &(adapter->recvpriv);
	if (precvpriv->sink_udpport > 0) {
		if (*((__be16 *)(ehdr_pos + 0x24)) == cpu_to_be16(precvpriv->sink_udpport)) {
			precvpriv->pre_rtp_rxseq = precvpriv->cur_rtp_rxseq;
			precvpriv->cur_rtp_rxseq = be16_to_cpu(*((__be16 *)(ehdr_pos + 0x2C)));
			if (precvpriv->pre_rtp_rxseq + 1 != precvpriv->cur_rtp_rxseq)
				RTW_INFO("%s : RTP Seq num from %d to %d\n", __func__, precvpriv->pre_rtp_rxseq, precvpriv->cur_rtp_rxseq);
		}
	}
}

void sta_rx_reorder_ctl_dump(void *sel, struct sta_info *sta)
{
	struct recv_reorder_ctrl *reorder_ctl;
	int i;

	for (i = 0; i < 16; i++) {
		reorder_ctl = &sta->recvreorder_ctrl[i];
		if (reorder_ctl->ampdu_size != RX_AMPDU_SIZE_INVALID ||
		    le16_to_cpu(reorder_ctl->indicate_seq) != 0xFFFF) {
			RTW_PRINT_SEL(sel, "tid=%d, enable=%d, ampdu_size=%u, indicate_seq=%u\n"
				, i, reorder_ctl->enable, reorder_ctl->ampdu_size, reorder_ctl->indicate_seq
				     );
		}
	}
}

void dump_tx_rate_bmp(void *sel, struct dvobj_priv *dvobj)
{
	struct adapter *adapter = dvobj_get_primary_adapter(dvobj);
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(dvobj);
	u8 bw;

	RTW_PRINT_SEL(sel, "%-6s", "bw");
	if (hal_chk_proto_cap(adapter, PROTO_CAP_11AC))
		_RTW_PRINT_SEL(sel, " %-11s", "vht");

	_RTW_PRINT_SEL(sel, " %-11s %-4s %-3s\n", "ht", "ofdm", "cck");

	for (bw = CHANNEL_WIDTH_20; bw <= CHANNEL_WIDTH_160; bw++) {
		if (!hal_is_bw_support(adapter, bw))
			continue;

		RTW_PRINT_SEL(sel, "%6s", ch_width_str(bw));
		if (hal_chk_proto_cap(adapter, PROTO_CAP_11AC)) {
			_RTW_PRINT_SEL(sel, " %03x %03x %03x"
				, RATE_BMP_GET_VHT_3SS(rfctl->rate_bmp_vht_by_bw[bw])
				, RATE_BMP_GET_VHT_2SS(rfctl->rate_bmp_vht_by_bw[bw])
				, RATE_BMP_GET_VHT_1SS(rfctl->rate_bmp_vht_by_bw[bw])
			);
		}

		_RTW_PRINT_SEL(sel, " %02x %02x %02x %02x"
			, bw <= CHANNEL_WIDTH_40 ? RATE_BMP_GET_HT_4SS(rfctl->rate_bmp_ht_by_bw[bw]) : 0
			, bw <= CHANNEL_WIDTH_40 ? RATE_BMP_GET_HT_3SS(rfctl->rate_bmp_ht_by_bw[bw]) : 0
			, bw <= CHANNEL_WIDTH_40 ? RATE_BMP_GET_HT_2SS(rfctl->rate_bmp_ht_by_bw[bw]) : 0
			, bw <= CHANNEL_WIDTH_40 ? RATE_BMP_GET_HT_1SS(rfctl->rate_bmp_ht_by_bw[bw]) : 0
		);

		_RTW_PRINT_SEL(sel, "  %03x   %01x\n"
			, bw <= CHANNEL_WIDTH_20 ? RATE_BMP_GET_OFDM(rfctl->rate_bmp_cck_ofdm) : 0
			, bw <= CHANNEL_WIDTH_20 ? RATE_BMP_GET_CCK(rfctl->rate_bmp_cck_ofdm) : 0
		);
	}
}

void dump_adapters_status(void *sel, struct dvobj_priv *dvobj)
{
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(dvobj);
	int i;
	struct adapter *iface;
	u8 u_ch, u_bw, u_offset;

	dump_mi_status(sel, dvobj);

#ifdef CONFIG_FW_MULTI_PORT_SUPPORT
	RTW_PRINT_SEL(sel, "default port id:%d\n\n", dvobj->default_port_id);
#endif /* CONFIG_FW_MULTI_PORT_SUPPORT */

	RTW_PRINT_SEL(sel, "dev status:%s%s\n\n"
		, dev_is_surprise_removed(dvobj) ? " SR" : ""
		, dev_is_drv_stopped(dvobj) ? " DS" : ""
	);

#define P2P_INFO_TITLE_FMT	" %-3s %-4s"
#define P2P_INFO_TITLE_ARG	, "lch", "p2ps"
#define P2P_INFO_VALUE_FMT	" %3u %c%3u"
#define P2P_INFO_VALUE_ARG	, iface->wdinfo.listen_channel, iface->wdev_data.p2p_enabled ? 'e' : ' ', rtw_p2p_state(&iface->wdinfo)
#define P2P_INFO_DASH		"---------"

	RTW_PRINT_SEL(sel, "%-2s %-15s %c %-3s %-3s %-3s %-17s %-4s %-7s"
		P2P_INFO_TITLE_FMT
		" %s\n"
		, "id", "ifname", ' ', "bup", "nup", "ncd", "macaddr", "port", "ch"
		P2P_INFO_TITLE_ARG
		, "status");

	RTW_PRINT_SEL(sel, "---------------------------------------------------------------"
		P2P_INFO_DASH
		"-------\n");

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->adapters[i];
		if (iface) {
			RTW_PRINT_SEL(sel, "%2d %-15s %c %3u %3u %3u "MAC_FMT" %4hhu %3u,%u,%u"
				P2P_INFO_VALUE_FMT
				" "MLME_STATE_FMT"\n"
				, i, iface->registered ? ADPT_ARG(iface) : NULL
				, iface->registered ? 'R' : ' '
				, iface->bup
				, iface->netif_up
				, iface->net_closed
				, MAC_ARG(adapter_mac_addr(iface))
				, get_hw_port(iface)
				, iface->mlmeextpriv.cur_channel
				, iface->mlmeextpriv.cur_bwmode
				, iface->mlmeextpriv.cur_ch_offset
				P2P_INFO_VALUE_ARG
				, MLME_STATE_ARG(iface)
			);
		}
	}

	RTW_PRINT_SEL(sel, "---------------------------------------------------------------"
		P2P_INFO_DASH
		"-------\n");

	rtw_mi_get_ch_setting_union(dvobj_get_primary_adapter(dvobj), &u_ch, &u_bw, &u_offset);
	RTW_PRINT_SEL(sel, "%55s %3u,%u,%u\n"
		, "union:"
		, u_ch, u_bw, u_offset
	);

	RTW_PRINT_SEL(sel, "%55s %3u,%u,%u offch_state:%d\n"
		, "oper:"
		, dvobj->oper_channel
		, dvobj->oper_bwmode
		, dvobj->oper_ch_offset
		, rfctl->offch_state
	);

}

#define SEC_CAM_ENT_ID_TITLE_FMT "%-2s"
#define SEC_CAM_ENT_ID_TITLE_ARG "id"
#define SEC_CAM_ENT_ID_VALUE_FMT "%2u"
#define SEC_CAM_ENT_ID_VALUE_ARG(id) (id)

#define SEC_CAM_ENT_TITLE_FMT "%-6s %-17s %-32s %-3s %-7s %-2s %-2s %-5s"
#define SEC_CAM_ENT_TITLE_ARG "ctrl", "addr", "key", "kid", "type", "MK", "GK", "valid"
#define SEC_CAM_ENT_VALUE_FMT "0x%04x "MAC_FMT" "KEY_FMT" %3u %-7s %2u %2u %5u"
#define SEC_CAM_ENT_VALUE_ARG(ent) \
	(ent)->ctrl \
	, MAC_ARG((ent)->mac) \
	, KEY_ARG((ent)->key) \
	, ((ent)->ctrl) & 0x03 \
	, security_type_str((((ent)->ctrl) >> 2) & 0x07) \
	, (((ent)->ctrl) >> 5) & 0x01 \
	, (((ent)->ctrl) >> 6) & 0x01 \
	, (((ent)->ctrl) >> 15) & 0x01

void dump_sec_cam_ent(void *sel, struct sec_cam_ent *ent, int id)
{
	if (id >= 0) {
		RTW_PRINT_SEL(sel, SEC_CAM_ENT_ID_VALUE_FMT " " SEC_CAM_ENT_VALUE_FMT"\n"
			, SEC_CAM_ENT_ID_VALUE_ARG(id), SEC_CAM_ENT_VALUE_ARG(ent));
	} else
		RTW_PRINT_SEL(sel, SEC_CAM_ENT_VALUE_FMT"\n", SEC_CAM_ENT_VALUE_ARG(ent));
}

void dump_sec_cam_ent_title(void *sel, u8 has_id)
{
	if (has_id) {
		RTW_PRINT_SEL(sel, SEC_CAM_ENT_ID_TITLE_FMT " " SEC_CAM_ENT_TITLE_FMT"\n"
			, SEC_CAM_ENT_ID_TITLE_ARG, SEC_CAM_ENT_TITLE_ARG);
	} else
		RTW_PRINT_SEL(sel, SEC_CAM_ENT_TITLE_FMT"\n", SEC_CAM_ENT_TITLE_ARG);
}

void dump_sec_cam(void *sel, struct adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	struct sec_cam_ent ent;
	int i;

	RTW_PRINT_SEL(sel, "HW sec cam:\n");
	dump_sec_cam_ent_title(sel, 1);
	for (i = 0; i < cam_ctl->num; i++) {
		rtw_sec_read_cam_ent(adapter, i, (u8 *)(&ent.ctrl), ent.mac, ent.key);
		dump_sec_cam_ent(sel , &ent, i);
	}
}

void dump_sec_cam_cache(void *sel, struct adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	int i;

	RTW_PRINT_SEL(sel, "SW sec cam cache:\n");
	dump_sec_cam_ent_title(sel, 1);
	for (i = 0; i < cam_ctl->num; i++) {
		if (dvobj->cam_cache[i].ctrl != 0)
			dump_sec_cam_ent(sel, &dvobj->cam_cache[i], i);
	}

}

#ifdef CONFIG_PROC_DEBUG
ssize_t proc_set_write_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 addr, val, len;

	if (count < 3) {
		RTW_INFO("argument size is less than 3\n");
		return -EFAULT;
	}

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%x %x %x", &addr, &val, &len);

		if (num !=  3) {
			RTW_INFO("invalid write_reg parameter!\n");
			return count;
		}

		switch (len) {
		case 1:
			rtw_write8(adapt, addr, (u8)val);
			break;
		case 2:
			rtw_write16(adapt, addr, (u16)val);
			break;
		case 4:
			rtw_write32(adapt, addr, val);
			break;
		default:
			RTW_INFO("error write length=%d", len);
			break;
		}

	}

	return count;

}

static u32 proc_get_read_addr = 0xeeeeeeee;
static u32 proc_get_read_len = 0x4;

int proc_get_read_reg(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	if (proc_get_read_addr == 0xeeeeeeee) {
		RTW_PRINT_SEL(m, "address not initialized\n");
		return 0;
	}

	switch (proc_get_read_len) {
	case 1:
		RTW_PRINT_SEL(m, "rtw_read8(0x%x)=0x%x\n", proc_get_read_addr, rtw_read8(adapt, proc_get_read_addr));
		break;
	case 2:
		RTW_PRINT_SEL(m, "rtw_read16(0x%x)=0x%x\n", proc_get_read_addr, rtw_read16(adapt, proc_get_read_addr));
		break;
	case 4:
		RTW_PRINT_SEL(m, "rtw_read32(0x%x)=0x%x\n", proc_get_read_addr, rtw_read32(adapt, proc_get_read_addr));
		break;
	default:
		RTW_PRINT_SEL(m, "error read length=%d\n", proc_get_read_len);
		break;
	}

	return 0;
}

ssize_t proc_set_read_reg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[16];
	u32 addr, len;

	if (count < 2) {
		RTW_INFO("argument size is less than 2\n");
		return -EFAULT;
	}

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%x %x", &addr, &len);

		if (num !=  2) {
			RTW_INFO("invalid read_reg parameter!\n");
			return count;
		}

		proc_get_read_addr = addr;

		proc_get_read_len = len;
	}

	return count;

}

int proc_get_rx_stat(struct seq_file *m, void *v)
{
	unsigned long	 irqL;
	struct list_head	*plist, *phead;
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct sta_info *psta = NULL;
	struct stainfo_stats	*pstats = NULL;
	struct sta_priv		*pstapriv = &(adapter->stapriv);
	u32 i, j;
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u8 null_addr[ETH_ALEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);
	for (i = 0; i < NUM_STA; i++) {
		phead = &(pstapriv->sta_hash[i]);
		plist = get_next(phead);
		while ((!rtw_end_of_queue_search(phead, plist))) {
			psta = container_of(plist, struct sta_info, hash_list);
			plist = get_next(plist);
			pstats = &psta->sta_stats;

			if (!pstats)
				continue;
			if ((memcmp(psta->cmn.mac_addr, bc_addr, 6))
				&& (memcmp(psta->cmn.mac_addr, null_addr, 6))
				&& (memcmp(psta->cmn.mac_addr, adapter_mac_addr(adapter), 6))) {
				RTW_PRINT_SEL(m, "MAC :\t\t"MAC_FMT "\n", MAC_ARG(psta->cmn.mac_addr));
				RTW_PRINT_SEL(m, "data_rx_cnt :\t%llu\n", sta_rx_data_uc_pkts(psta) - pstats->last_rx_data_uc_pkts);
				pstats->last_rx_data_uc_pkts = sta_rx_data_uc_pkts(psta);
				RTW_PRINT_SEL(m, "duplicate_cnt :\t%u\n", pstats->duplicate_cnt);
				pstats->duplicate_cnt = 0;
				RTW_PRINT_SEL(m, "rx_per_rate_cnt :\n");

				for (j = 0; j < 0x60; j++) {
					RTW_PRINT_SEL(m, "%08u  ", pstats->rxratecnt[j]);
					pstats->rxratecnt[j] = 0;
					if ((j%8) == 7)
						RTW_PRINT_SEL(m, "\n");
				}
				RTW_PRINT_SEL(m, "\n");
			}
		}
	}
	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);
	return 0;
}

int proc_get_tx_stat(struct seq_file *m, void *v)
{
	unsigned long	irqL;
	struct list_head	*plist, *phead;
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct sta_info *psta = NULL, *sta_rec[NUM_STA];
	struct stainfo_stats	*pstats = NULL;
	struct sta_priv	*pstapriv = &(adapter->stapriv);
	u32 i, macid_rec_idx = 0;
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u8 null_addr[ETH_ALEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	struct submit_ctx gotc2h;

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);
	for (i = 0; i < NUM_STA; i++) {
		sta_rec[i] = NULL;
		phead = &(pstapriv->sta_hash[i]);
		plist = get_next(phead);
		while ((!rtw_end_of_queue_search(phead, plist))) {
			psta = container_of(plist, struct sta_info, hash_list);
			plist = get_next(plist);
			if ((memcmp(psta->cmn.mac_addr, bc_addr, 6)) &&
			    (memcmp(psta->cmn.mac_addr, null_addr, 6)) &&
			    (memcmp(psta->cmn.mac_addr, adapter_mac_addr(adapter), 6))) {
				sta_rec[macid_rec_idx++] = psta;
			}
		}
	}
	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);
	for (i = 0; i < macid_rec_idx; i++) {
		pstats = &(sta_rec[i]->sta_stats);
		if (!pstats)
			continue;
		pstapriv->c2h_sta = sta_rec[i];
		rtw_hal_reqtxrpt(adapter, sta_rec[i]->cmn.mac_id);
		rtw_sctx_init(&gotc2h, 60);
		pstapriv->gotc2h = &gotc2h;
		if (rtw_sctx_wait(&gotc2h, __func__)) {
			RTW_PRINT_SEL(m, "MAC :\t\t"MAC_FMT "\n", MAC_ARG(sta_rec[i]->cmn.mac_addr));
			RTW_PRINT_SEL(m, "data_sent_cnt :\t%u\n", pstats->tx_ok_cnt + pstats->tx_fail_cnt);
			RTW_PRINT_SEL(m, "success_cnt :\t%u\n", pstats->tx_ok_cnt);
			RTW_PRINT_SEL(m, "failure_cnt :\t%u\n", pstats->tx_fail_cnt);
			RTW_PRINT_SEL(m, "retry_cnt :\t%u\n\n", pstats->tx_retry_cnt);
		} else {
			RTW_PRINT_SEL(m, "Warming : Query timeout, operation abort!!\n");
			RTW_PRINT_SEL(m, "\n");
			pstapriv->c2h_sta = NULL;
			break;
		}
	}
	return 0;
}

int proc_get_fwstate(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	RTW_PRINT_SEL(m, "fwstate=0x%x\n", get_fwstate(pmlmepriv));

	return 0;
}

int proc_get_sec_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct security_priv *sec = &adapt->securitypriv;

	RTW_PRINT_SEL(m, "auth_alg=0x%x, enc_alg=0x%x, auth_type=0x%x, enc_type=0x%x\n",
		sec->dot11AuthAlgrthm, sec->dot11PrivacyAlgrthm,
		sec->ndisauthtype, sec->ndisencryptstatus);

	RTW_PRINT_SEL(m, "hw_decrypted=%d\n", sec->hw_decrypted);

	RTW_PRINT_SEL(m, "wep_sw_enc_cnt=%llu, %llu, %llu\n"
		, sec->wep_sw_enc_cnt_bc , sec->wep_sw_enc_cnt_mc, sec->wep_sw_enc_cnt_uc);
	RTW_PRINT_SEL(m, "wep_sw_dec_cnt=%llu, %llu, %llu\n"
		, sec->wep_sw_dec_cnt_bc , sec->wep_sw_dec_cnt_mc, sec->wep_sw_dec_cnt_uc);

	RTW_PRINT_SEL(m, "tkip_sw_enc_cnt=%llu, %llu, %llu\n"
		, sec->tkip_sw_enc_cnt_bc , sec->tkip_sw_enc_cnt_mc, sec->tkip_sw_enc_cnt_uc);
	RTW_PRINT_SEL(m, "tkip_sw_dec_cnt=%llu, %llu, %llu\n"
		, sec->tkip_sw_dec_cnt_bc , sec->tkip_sw_dec_cnt_mc, sec->tkip_sw_dec_cnt_uc);

	RTW_PRINT_SEL(m, "aes_sw_enc_cnt=%llu, %llu, %llu\n"
		, sec->aes_sw_enc_cnt_bc , sec->aes_sw_enc_cnt_mc, sec->aes_sw_enc_cnt_uc);
	RTW_PRINT_SEL(m, "aes_sw_dec_cnt=%llu, %llu, %llu\n"
		, sec->aes_sw_dec_cnt_bc , sec->aes_sw_dec_cnt_mc, sec->aes_sw_dec_cnt_uc);
	return 0;
}

int proc_get_mlmext_state(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);

	RTW_PRINT_SEL(m, "pmlmeinfo->state=0x%x\n", pmlmeinfo->state);

	return 0;
}

int proc_get_roam_flags(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m, "0x%02x\n", rtw_roam_flags(adapter));

	return 0;
}

ssize_t proc_set_roam_flags(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);

	char tmp[32];
	u8 flags;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx", &flags);

		if (num == 1)
			rtw_assign_roam_flags(adapter, flags);
	}

	return count;

}

int proc_get_roam_param(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *mlme = &adapter->mlmepriv;

	RTW_PRINT_SEL(m, "%12s %12s %11s %14s\n", "rssi_diff_th", "scanr_exp_ms", "scan_int_ms", "rssi_threshold");
	RTW_PRINT_SEL(m, "%-12u %-12u %-11u %-14u\n"
		, mlme->roam_rssi_diff_th
		, mlme->roam_scanr_exp_ms
		, mlme->roam_scan_int_ms
		, mlme->roam_rssi_threshold
	);

	return 0;
}

ssize_t proc_set_roam_param(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *mlme = &adapter->mlmepriv;

	char tmp[32];
	u8 rssi_diff_th;
	u32 scanr_exp_ms;
	u32 scan_int_ms;
	u8 rssi_threshold;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhu %u %u %hhu", &rssi_diff_th, &scanr_exp_ms, &scan_int_ms, &rssi_threshold);

		if (num >= 1)
			mlme->roam_rssi_diff_th = rssi_diff_th;
		if (num >= 2)
			mlme->roam_scanr_exp_ms = scanr_exp_ms;
		if (num >= 3)
			mlme->roam_scan_int_ms = scan_int_ms;
		if (num >= 4)
			mlme->roam_rssi_threshold = rssi_threshold;
	}

	return count;

}

ssize_t proc_set_roam_tgt_addr(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);

	char tmp[32];
	u8 addr[ETH_ALEN];

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", addr, addr + 1, addr + 2, addr + 3, addr + 4, addr + 5);
		if (num == 6)
			memcpy(adapter->mlmepriv.roam_tgt_addr, addr, ETH_ALEN);

		RTW_INFO("set roam_tgt_addr to "MAC_FMT"\n", MAC_ARG(adapter->mlmepriv.roam_tgt_addr));
	}

	return count;
}

#ifdef CONFIG_RTW_80211R
ssize_t proc_set_ft_flags(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);

	char tmp[32];
	u8 flags;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx", &flags);

		if (num == 1)
			adapter->mlmepriv.ft_roam.ft_flags = flags;
	}

	return count;

}

int proc_get_ft_flags(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m, "0x%02x\n", adapter->mlmepriv.ft_roam.ft_flags);

	return 0;
}
#endif

int proc_get_qos_option(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	RTW_PRINT_SEL(m, "qos_option=%d\n", pmlmepriv->qospriv.qos_option);

	return 0;
}

int proc_get_ht_option(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	RTW_PRINT_SEL(m, "ht_option=%d\n", pmlmepriv->htpriv.ht_option);

	return 0;
}

int proc_get_rf_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;

	RTW_PRINT_SEL(m, "cur_ch=%d, cur_bw=%d, cur_ch_offet=%d\n",
		pmlmeext->cur_channel, pmlmeext->cur_bwmode, pmlmeext->cur_ch_offset);

	RTW_PRINT_SEL(m, "oper_ch=%d, oper_bw=%d, oper_ch_offet=%d\n",
		rtw_get_oper_ch(adapt), rtw_get_oper_bw(adapt),  rtw_get_oper_choffset(adapt));

	return 0;
}

int proc_get_scan_param(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;
	struct ss_res *ss = &mlmeext->sitesurvey_res;

#define SCAN_PARAM_TITLE_FMT "%10s"
#define SCAN_PARAM_VALUE_FMT "%-10u"
#define SCAN_PARAM_TITLE_ARG , "scan_ch_ms"
#define SCAN_PARAM_VALUE_ARG , ss->scan_ch_ms
#define SCAN_PARAM_TITLE_FMT_HT " %15s %13s"
#define SCAN_PARAM_VALUE_FMT_HT " %-15u %-13u"
#define SCAN_PARAM_TITLE_ARG_HT , "rx_ampdu_accept", "rx_ampdu_size"
#define SCAN_PARAM_VALUE_ARG_HT , ss->rx_ampdu_accept, ss->rx_ampdu_size
#define SCAN_PARAM_TITLE_FMT_BACKOP " %9s %12s"
#define SCAN_PARAM_VALUE_FMT_BACKOP " %-9u %-12u"
#define SCAN_PARAM_TITLE_ARG_BACKOP , "backop_ms", "scan_cnt_max"
#define SCAN_PARAM_VALUE_ARG_BACKOP , ss->backop_ms, ss->scan_cnt_max

	RTW_PRINT_SEL(m,
		SCAN_PARAM_TITLE_FMT
		SCAN_PARAM_TITLE_FMT_HT
		SCAN_PARAM_TITLE_FMT_BACKOP
		"\n"
		SCAN_PARAM_TITLE_ARG
		SCAN_PARAM_TITLE_ARG_HT
		SCAN_PARAM_TITLE_ARG_BACKOP
	);

	RTW_PRINT_SEL(m,
		SCAN_PARAM_VALUE_FMT
		SCAN_PARAM_VALUE_FMT_HT
		SCAN_PARAM_VALUE_FMT_BACKOP
		"\n"
		SCAN_PARAM_VALUE_ARG
		SCAN_PARAM_VALUE_ARG_HT
		SCAN_PARAM_VALUE_ARG_BACKOP
	);

	return 0;
}

ssize_t proc_set_scan_param(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;
	struct ss_res *ss = &mlmeext->sitesurvey_res;

	char tmp[32] = {0};

	u16 scan_ch_ms;
#define SCAN_PARAM_INPUT_FMT "%hu"
#define SCAN_PARAM_INPUT_ARG , &scan_ch_ms
	u8 rx_ampdu_accept;
	u8 rx_ampdu_size;
#define SCAN_PARAM_INPUT_FMT_HT " %hhu %hhu"
#define SCAN_PARAM_INPUT_ARG_HT , &rx_ampdu_accept, &rx_ampdu_size
	u16 backop_ms;
	u8 scan_cnt_max;
#define SCAN_PARAM_INPUT_FMT_BACKOP " %hu %hhu"
#define SCAN_PARAM_INPUT_ARG_BACKOP , &backop_ms, &scan_cnt_max

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp,
			SCAN_PARAM_INPUT_FMT
			SCAN_PARAM_INPUT_FMT_HT
			SCAN_PARAM_INPUT_FMT_BACKOP
			SCAN_PARAM_INPUT_ARG
			SCAN_PARAM_INPUT_ARG_HT
			SCAN_PARAM_INPUT_ARG_BACKOP
		);

		if (num-- > 0)
			ss->scan_ch_ms = scan_ch_ms;
		if (num-- > 0)
			ss->rx_ampdu_accept = rx_ampdu_accept;
		if (num-- > 0)
			ss->rx_ampdu_size = rx_ampdu_size;
		if (num-- > 0)
			ss->backop_ms = backop_ms;
		if (num-- > 0)
			ss->scan_cnt_max = scan_cnt_max;
	}

	return count;
}

int proc_get_scan_abort(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	u32 pass_ms;

	pass_ms = rtw_scan_abort_timeout(adapter, 10000);

	RTW_PRINT_SEL(m, "%u\n", pass_ms);

	return 0;
}

int proc_get_backop_flags_sta(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;

	RTW_PRINT_SEL(m, "0x%02x\n", mlmeext_scan_backop_flags_sta(mlmeext));

	return 0;
}

ssize_t proc_set_backop_flags_sta(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;

	char tmp[32];
	u8 flags;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx", &flags);

		if (num == 1)
			mlmeext_assign_scan_backop_flags_sta(mlmeext, flags);
	}

	return count;
}

int proc_get_backop_flags_ap(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;

	RTW_PRINT_SEL(m, "0x%02x\n", mlmeext_scan_backop_flags_ap(mlmeext));

	return 0;
}

ssize_t proc_set_backop_flags_ap(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv *mlmeext = &adapter->mlmeextpriv;

	char tmp[32];
	u8 flags;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx", &flags);

		if (num == 1)
			mlmeext_assign_scan_backop_flags_ap(mlmeext, flags);
	}

	return count;
}

int proc_get_survey_info(struct seq_file *m, void *v)
{
	unsigned long irqL;
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct __queue	*queue	= &(pmlmepriv->scanned_queue);
	struct wlan_network	*pnetwork = NULL;
	struct list_head	*plist, *phead;
	int notify_signal;
	s16 notify_noise = 0;
	u16  index = 0, ie_cap = 0;
	unsigned char *ie_wpa = NULL, *ie_wpa2 = NULL, *ie_wps = NULL;
	unsigned char *ie_p2p = NULL, *ssid = NULL;
	char flag_str[64];
	int ielen = 0;
	u32 wpsielen = 0;

	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);
	phead = get_list_head(queue);
	if (!phead)
		goto _exit;
	plist = get_next(phead);
	if (!plist)
		goto _exit;

	RTW_PRINT_SEL(m, "%5s  %-17s  %3s  %-3s  %-4s  %-4s  %5s  %32s  %32s\n", "index", "bssid", "ch", "RSSI", "SdBm", "Noise", "age", "flag", "ssid");
	while (1) {
		if (rtw_end_of_queue_search(phead, plist))
			break;

		pnetwork = container_of(plist, struct wlan_network, list);
		if (!pnetwork)
			break;

		if (check_fwstate(pmlmepriv, _FW_LINKED) &&
		    is_same_network(&pmlmepriv->cur_network.network, &pnetwork->network, 0)) {
			notify_signal = translate_percentage_to_dbm(adapt->recvpriv.signal_strength);/* dbm */
		} else {
			notify_signal = translate_percentage_to_dbm(pnetwork->network.PhyInfo.SignalStrength);/* dbm */
		}

		ie_wpa = rtw_get_wpa_ie(&pnetwork->network.IEs[12], &ielen, pnetwork->network.IELength - 12);
		ie_wpa2 = rtw_get_wpa2_ie(&pnetwork->network.IEs[12], &ielen, pnetwork->network.IELength - 12);
		ie_cap = rtw_get_capability(&pnetwork->network);
		ie_wps = rtw_get_wps_ie(&pnetwork->network.IEs[12], pnetwork->network.IELength - 12, NULL, &wpsielen);
		ie_p2p = rtw_get_p2p_ie(&pnetwork->network.IEs[12], pnetwork->network.IELength - 12, NULL, &ielen);
		ssid = pnetwork->network.Ssid.Ssid;
		sprintf(flag_str, "%s%s%s%s%s%s%s",
			(ie_wpa) ? "[WPA]" : "",
			(ie_wpa2) ? "[WPA2]" : "",
			(!ie_wpa && !ie_wpa && ie_cap & BIT(4)) ? "[WEP]" : "",
			(ie_wps) ? "[WPS]" : "",
			(pnetwork->network.InfrastructureMode == Ndis802_11IBSS) ? "[IBSS]" : "",
			(ie_cap & BIT(0)) ? "[ESS]" : "",
			(ie_p2p) ? "[P2P]" : "");
		RTW_PRINT_SEL(m, "%5d  "MAC_FMT"  %3d  %3d  %4d  %4d    %5d  %32s  %32s\n",
			      ++index,
			      MAC_ARG(pnetwork->network.MacAddress),
			      pnetwork->network.Configuration.DSConfig,
			      (int)pnetwork->network.Rssi,
			      notify_signal,
			      notify_noise,
			rtw_get_passing_time_ms(pnetwork->last_scanned),
			      flag_str,
			      pnetwork->network.Ssid.Ssid);
		plist = get_next(plist);
	}
_exit:
	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	return 0;
}

ssize_t proc_set_survey_info(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	u8 _status = false;

	if (count < 1)
		return -EFAULT;
	if (rtw_is_scan_deny(adapt)) {
		RTW_INFO(FUNC_ADPT_FMT  ": scan deny\n", FUNC_ADPT_ARG(adapt));
		goto exit;
	}

	rtw_ps_deny(adapt, PS_DENY_SCAN);
	if (_FAIL == rtw_pwr_wakeup(adapt))
		goto cancel_ps_deny;

	if (!rtw_is_adapter_up(adapt)) {
		RTW_INFO("scan abort!! adapter cannot use\n");
		goto cancel_ps_deny;
	}

	if (rtw_mi_busy_traffic_check(adapt, false)) {
		RTW_INFO("scan abort!! BusyTraffic\n");
		goto cancel_ps_deny;
	}

	if (check_fwstate(pmlmepriv, WIFI_AP_STATE) && check_fwstate(pmlmepriv, WIFI_UNDER_WPS)) {
		RTW_INFO("scan abort!! AP mode process WPS\n");
		goto cancel_ps_deny;
	}
	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY | _FW_UNDER_LINKING)) {
		RTW_INFO("scan abort!! fwstate=0x%x\n", pmlmepriv->fw_state);
		goto cancel_ps_deny;
	}

#ifdef CONFIG_CONCURRENT_MODE
	if (rtw_mi_buddy_check_fwstate(adapt,
		       _FW_UNDER_SURVEY | _FW_UNDER_LINKING | WIFI_UNDER_WPS)) {
		RTW_INFO("scan abort!! buddy_fwstate check failed\n");
		goto cancel_ps_deny;
	}
#endif
	_status = rtw_set_802_11_bssid_list_scan(adapt, NULL);

cancel_ps_deny:
	rtw_ps_deny_cancel(adapt, PS_DENY_SCAN);
exit:
	return count;
}

int proc_get_ap_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct sta_info *psta;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &adapt->mlmeextpriv;
	struct wlan_network *cur_network = &(pmlmepriv->cur_network);
	struct sta_priv *pstapriv = &adapt->stapriv;

	psta = rtw_get_stainfo(pstapriv, cur_network->network.MacAddress);
	if (psta) {
		RTW_PRINT_SEL(m, "SSID=%s\n", cur_network->network.Ssid.Ssid);
		RTW_PRINT_SEL(m, "sta's macaddr:" MAC_FMT "\n", MAC_ARG(psta->cmn.mac_addr));
		RTW_PRINT_SEL(m, "cur_channel=%d, cur_bwmode=%d, cur_ch_offset=%d\n", pmlmeext->cur_channel, pmlmeext->cur_bwmode, pmlmeext->cur_ch_offset);
		RTW_PRINT_SEL(m, "wireless_mode=0x%x, rtsen=%d, cts2slef=%d\n", psta->wireless_mode, psta->rtsen, psta->cts2self);
		RTW_PRINT_SEL(m, "state=0x%x, aid=%d, macid=%d, raid=%d\n",
			psta->state, psta->cmn.aid, psta->cmn.mac_id, psta->cmn.ra_info.rate_id);
		RTW_PRINT_SEL(m, "qos_en=%d, ht_en=%d, init_rate=%d\n", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
		RTW_PRINT_SEL(m, "bwmode=%d, ch_offset=%d, sgi_20m=%d,sgi_40m=%d\n"
			, psta->cmn.bw_mode, psta->htpriv.ch_offset, psta->htpriv.sgi_20m, psta->htpriv.sgi_40m);
		RTW_PRINT_SEL(m, "ampdu_enable = %d\n", psta->htpriv.ampdu_enable);
		RTW_PRINT_SEL(m, "agg_enable_bitmap=%x, candidate_tid_bitmap=%x\n", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);
		RTW_PRINT_SEL(m, "ldpc_cap=0x%x, stbc_cap=0x%x, beamform_cap=0x%x\n", psta->htpriv.ldpc_cap, psta->htpriv.stbc_cap, psta->htpriv.beamform_cap);
		sta_rx_reorder_ctl_dump(m, psta);
	} else
		RTW_PRINT_SEL(m, "can't get sta's macaddr, cur_network's macaddr:" MAC_FMT "\n", MAC_ARG(cur_network->network.MacAddress));

	return 0;
}

ssize_t proc_reset_trx_info(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct recv_priv  *precvpriv = &adapt->recvpriv;
	char cmd[32] = {0};
	u8 cnt = 0;

	if (count > sizeof(cmd)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(cmd, buffer, count)) {
		sscanf(cmd, "%hhx", &cnt);
		if (0 == cnt) {
			precvpriv->dbg_rx_ampdu_drop_count = 0;
			precvpriv->dbg_rx_ampdu_forced_indicate_count = 0;
			precvpriv->dbg_rx_ampdu_loss_count = 0;
			precvpriv->dbg_rx_dup_mgt_frame_drop_count = 0;
			precvpriv->dbg_rx_ampdu_window_shift_cnt = 0;
			precvpriv->dbg_rx_conflic_mac_addr_cnt = 0;
			precvpriv->dbg_rx_drop_count = 0;
		}
	}

	return count;
}

int proc_get_trx_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	int i;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	struct recv_priv  *precvpriv = &adapt->recvpriv;
	struct hw_xmit *phwxmit;

	dump_os_queue(m, adapt);

	RTW_PRINT_SEL(m, "free_xmitbuf_cnt=%d, free_xmitframe_cnt=%d\n"
		, pxmitpriv->free_xmitbuf_cnt, pxmitpriv->free_xmitframe_cnt);
	RTW_PRINT_SEL(m, "free_ext_xmitbuf_cnt=%d, free_xframe_ext_cnt=%d\n"
		, pxmitpriv->free_xmit_extbuf_cnt, pxmitpriv->free_xframe_ext_cnt);
	RTW_PRINT_SEL(m, "free_recvframe_cnt=%d\n"
		      , precvpriv->free_recvframe_cnt);

	for (i = 0; i < 4; i++) {
		phwxmit = pxmitpriv->hwxmits + i;
		RTW_PRINT_SEL(m, "%d, hwq.accnt=%d\n", i, phwxmit->accnt);
	}

	rtw_hal_get_hwreg(adapt, HW_VAR_DUMP_MAC_TXFIFO, (u8 *)m);

	RTW_PRINT_SEL(m, "rx_urb_pending_cn=%d\n", ATOMIC_READ(&(precvpriv->rx_pending_cnt)));

	dump_rx_bh_tk(m, &GET_PRIMARY_ADAPTER(adapt)->recvpriv);

	/* Folowing are RX info */
	RTW_PRINT_SEL(m, "RX: Count of Packets dropped by Driver: %llu\n", (unsigned long long)precvpriv->dbg_rx_drop_count);
	/* Counts of packets whose seq_num is less than preorder_ctrl->indicate_seq, Ex delay, retransmission, redundant packets and so on */
	RTW_PRINT_SEL(m, "Rx: Counts of Packets Whose Seq_Num Less Than Reorder Control Seq_Num: %llu\n", (unsigned long long)precvpriv->dbg_rx_ampdu_drop_count);
	/* How many times the Rx Reorder Timer is triggered. */
	RTW_PRINT_SEL(m, "Rx: Reorder Time-out Trigger Counts: %llu\n", (unsigned long long)precvpriv->dbg_rx_ampdu_forced_indicate_count);
	/* Total counts of packets loss */
	RTW_PRINT_SEL(m, "Rx: Packet Loss Counts: %llu\n", (unsigned long long)precvpriv->dbg_rx_ampdu_loss_count);
	RTW_PRINT_SEL(m, "Rx: Duplicate Management Frame Drop Count: %llu\n", (unsigned long long)precvpriv->dbg_rx_dup_mgt_frame_drop_count);
	RTW_PRINT_SEL(m, "Rx: AMPDU BA window shift Count: %llu\n", (unsigned long long)precvpriv->dbg_rx_ampdu_window_shift_cnt);
	/*The same mac addr counts*/
	RTW_PRINT_SEL(m, "Rx: Conflict MAC Address Frames Count: %llu\n", (unsigned long long)precvpriv->dbg_rx_conflic_mac_addr_cnt);
	return 0;
}

int proc_get_dis_pwt(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 dis_pwt = 0;
	rtw_hal_get_def_var(adapt, HAL_DEF_DBG_DIS_PWT, &(dis_pwt));
	RTW_PRINT_SEL(m, " Tx Power training mode:%s\n", (dis_pwt) ? "Disable" : "Enable");
	return 0;
}
ssize_t proc_set_dis_pwt(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[4] = {0};
	u8 dis_pwt = 0;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx", &dis_pwt);

		RTW_INFO("Set Tx Power training mode:%s\n", (dis_pwt) ? "Disable" : "Enable");
		if (num >= 1)
			rtw_hal_set_def_var(adapt, HAL_DEF_DBG_DIS_PWT, &(dis_pwt));
	}
	return count;
}

int proc_get_rate_ctl(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	u8 data_rate = 0, sgi = 0, data_fb = 0;

	if (adapter->fix_rate != 0xff) {
		data_rate = adapter->fix_rate & 0x7F;
		sgi = adapter->fix_rate >> 7;
		data_fb = adapter->data_fb ? 1 : 0;
		RTW_PRINT_SEL(m, "FIXED %s%s%s\n"
			, HDATA_RATE(data_rate)
			, data_rate > DESC_RATE54M ? (sgi ? " SGI" : " LGI") : ""
			, data_fb ? " FB" : ""
		);
		RTW_PRINT_SEL(m, "0x%02x %u\n", adapter->fix_rate, adapter->data_fb);
	} else
		RTW_PRINT_SEL(m, "RA\n");

	return 0;
}

ssize_t proc_set_rate_ctl(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct hal_com_data *hal_data = GET_HAL_DATA(adapter);
	char tmp[32];
	u8 fix_rate;
	u8 data_fb;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx %hhu", &fix_rate, &data_fb);

		if (num >= 1) {
			u8 fix_rate_ori = adapter->fix_rate;

			adapter->fix_rate = fix_rate;
			if (fix_rate == 0xFF)
				hal_data->ForcedDataRate = 0;
			else
				hal_data->ForcedDataRate = hw_rate_to_m_rate(fix_rate & 0x7F);

			if (adapter->fix_bw != 0xFF && fix_rate_ori != fix_rate)
				rtw_update_tx_rate_bmp(adapter_to_dvobj(adapter));
		}
		if (num >= 2)
			adapter->data_fb = data_fb ? 1 : 0;
	}

	return count;
}

int proc_get_bmc_tx_rate(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);

	if (!MLME_IS_AP(adapter) && !MLME_IS_MESH(adapter)) {
		RTW_PRINT_SEL(m, "[ERROR] Not in SoftAP/Mesh mode !!\n");
		return 0;
	}

	RTW_PRINT_SEL(m, " BMC Tx rate - %s\n", MGN_RATE_STR(adapter->bmc_tx_rate));
	return 0;
}

ssize_t proc_set_bmc_tx_rate(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u8 bmc_tx_rate;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhx", &bmc_tx_rate);

		if (num >= 1)
			/*adapter->bmc_tx_rate = hw_rate_to_m_rate(bmc_tx_rate);*/
			adapter->bmc_tx_rate = bmc_tx_rate;
	}

	return count;
}

int proc_get_tx_power_offset(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m, "Tx power offset - %u\n", adapter->power_offset);
	return 0;
}

ssize_t proc_set_tx_power_offset(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u8 power_offset = 0;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhu", &power_offset);

		if (num >= 1) {
			if (power_offset > 5)
				power_offset = 0;

			adapter->power_offset = power_offset;
		}
	}

	return count;
}

int proc_get_bw_ctl(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	u8 data_bw = 0;

	if (adapter->fix_bw != 0xff) {
		data_bw = adapter->fix_bw;
		RTW_PRINT_SEL(m, "FIXED %s\n", ch_width_str(data_bw));
	} else
		RTW_PRINT_SEL(m, "Auto\n");

	return 0;
}

ssize_t proc_set_bw_ctl(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u8 fix_bw;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhu", &fix_bw);

		if (num >= 1) {
			u8 fix_bw_ori = adapter->fix_bw;

			adapter->fix_bw = fix_bw;

			if (adapter->fix_rate != 0xFF && fix_bw_ori != fix_bw)
				rtw_update_tx_rate_bmp(adapter_to_dvobj(adapter));
		}
	}

	return count;
}

static u8 fwdl_test_chksum_fail = 0;
static u8 fwdl_test_wintint_rdy_fail = 0;

bool rtw_fwdl_test_trigger_chksum_fail(void)
{
	if (fwdl_test_chksum_fail) {
		RTW_PRINT("fwdl test case: trigger chksum_fail\n");
		fwdl_test_chksum_fail--;
		return true;
	}
	return false;
}

bool rtw_fwdl_test_trigger_wintint_rdy_fail(void)
{
	if (fwdl_test_wintint_rdy_fail) {
		RTW_PRINT("fwdl test case: trigger wintint_rdy_fail\n");
		fwdl_test_wintint_rdy_fail--;
		return true;
	}
	return false;
}

ssize_t proc_set_fwdl_test_case(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count))
		sscanf(tmp, "%hhu %hhu", &fwdl_test_chksum_fail, &fwdl_test_wintint_rdy_fail);
	return count;
}

static u8 del_rx_ampdu_test_no_tx_fail = 0;

bool rtw_del_rx_ampdu_test_trigger_no_tx_fail(void)
{
	if (del_rx_ampdu_test_no_tx_fail) {
		RTW_PRINT("del_rx_ampdu test case: trigger no_tx_fail\n");
		del_rx_ampdu_test_no_tx_fail--;
		return true;
	}
	return false;
}

ssize_t proc_set_del_rx_ampdu_test_case(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}
	if (buffer && !copy_from_user(tmp, buffer, count))
		sscanf(tmp, "%hhu", &del_rx_ampdu_test_no_tx_fail);

	return count;
}

static u32 g_wait_hiq_empty_ms = 0;

u32 rtw_get_wait_hiq_empty_ms(void)
{
	return g_wait_hiq_empty_ms;
}

ssize_t proc_set_wait_hiq_empty(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];
	int num;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count))
		num = sscanf(tmp, "%u", &g_wait_hiq_empty_ms);

	return count;
}

static unsigned long sta_linking_test_start_time = 0;
static u32 sta_linking_test_wait_ms = 0;
static u8 sta_linking_test_force_fail = 0;

void rtw_sta_linking_test_set_start(void)
{
	sta_linking_test_start_time = rtw_get_current_time();
}

bool rtw_sta_linking_test_wait_done(void)
{
	return rtw_get_passing_time_ms(sta_linking_test_start_time) >= sta_linking_test_wait_ms;
}

bool rtw_sta_linking_test_force_fail(void)
{
	return sta_linking_test_force_fail;
}

ssize_t proc_set_sta_linking_test(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		u32 wait_ms = 0;
		u8 force_fail = 0;
		int num = sscanf(tmp, "%u %hhu", &wait_ms, &force_fail);

		if (num >= 1)
			sta_linking_test_wait_ms = wait_ms;
		if (num >= 2)
			sta_linking_test_force_fail = force_fail;
	}

	return count;
}

int proc_get_ps_dbg_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapt->dvobj;
	struct debug_priv *pdbgpriv = &dvobj->drv_dbg;

	RTW_PRINT_SEL(m, "dbg_sdio_alloc_irq_cnt=%d\n", pdbgpriv->dbg_sdio_alloc_irq_cnt);
	RTW_PRINT_SEL(m, "dbg_sdio_free_irq_cnt=%d\n", pdbgpriv->dbg_sdio_free_irq_cnt);
	RTW_PRINT_SEL(m, "dbg_sdio_alloc_irq_error_cnt=%d\n", pdbgpriv->dbg_sdio_alloc_irq_error_cnt);
	RTW_PRINT_SEL(m, "dbg_sdio_free_irq_error_cnt=%d\n", pdbgpriv->dbg_sdio_free_irq_error_cnt);
	RTW_PRINT_SEL(m, "dbg_sdio_init_error_cnt=%d\n", pdbgpriv->dbg_sdio_init_error_cnt);
	RTW_PRINT_SEL(m, "dbg_sdio_deinit_error_cnt=%d\n", pdbgpriv->dbg_sdio_deinit_error_cnt);
	RTW_PRINT_SEL(m, "dbg_suspend_error_cnt=%d\n", pdbgpriv->dbg_suspend_error_cnt);
	RTW_PRINT_SEL(m, "dbg_suspend_cnt=%d\n", pdbgpriv->dbg_suspend_cnt);
	RTW_PRINT_SEL(m, "dbg_resume_cnt=%d\n", pdbgpriv->dbg_resume_cnt);
	RTW_PRINT_SEL(m, "dbg_resume_error_cnt=%d\n", pdbgpriv->dbg_resume_error_cnt);
	RTW_PRINT_SEL(m, "dbg_deinit_fail_cnt=%d\n", pdbgpriv->dbg_deinit_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_carddisable_cnt=%d\n", pdbgpriv->dbg_carddisable_cnt);
	RTW_PRINT_SEL(m, "dbg_ps_insuspend_cnt=%d\n", pdbgpriv->dbg_ps_insuspend_cnt);
	RTW_PRINT_SEL(m, "dbg_dev_unload_inIPS_cnt=%d\n", pdbgpriv->dbg_dev_unload_inIPS_cnt);
	RTW_PRINT_SEL(m, "dbg_scan_pwr_state_cnt=%d\n", pdbgpriv->dbg_scan_pwr_state_cnt);
	RTW_PRINT_SEL(m, "dbg_downloadfw_pwr_state_cnt=%d\n", pdbgpriv->dbg_downloadfw_pwr_state_cnt);
	RTW_PRINT_SEL(m, "dbg_carddisable_error_cnt=%d\n", pdbgpriv->dbg_carddisable_error_cnt);
	RTW_PRINT_SEL(m, "dbg_fw_read_ps_state_fail_cnt=%d\n", pdbgpriv->dbg_fw_read_ps_state_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_leave_ips_fail_cnt=%d\n", pdbgpriv->dbg_leave_ips_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_leave_lps_fail_cnt=%d\n", pdbgpriv->dbg_leave_lps_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_h2c_leave32k_fail_cnt=%d\n", pdbgpriv->dbg_h2c_leave32k_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_diswow_dload_fw_fail_cnt=%d\n", pdbgpriv->dbg_diswow_dload_fw_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_enwow_dload_fw_fail_cnt=%d\n", pdbgpriv->dbg_enwow_dload_fw_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_ips_drvopen_fail_cnt=%d\n", pdbgpriv->dbg_ips_drvopen_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_poll_fail_cnt=%d\n", pdbgpriv->dbg_poll_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_rpwm_toogle_cnt=%d\n", pdbgpriv->dbg_rpwm_toogle_cnt);
	RTW_PRINT_SEL(m, "dbg_rpwm_timeout_fail_cnt=%d\n", pdbgpriv->dbg_rpwm_timeout_fail_cnt);
	RTW_PRINT_SEL(m, "dbg_sreset_cnt=%d\n", pdbgpriv->dbg_sreset_cnt);
	RTW_PRINT_SEL(m, "dbg_fw_mem_dl_error_cnt=%d\n", pdbgpriv->dbg_fw_mem_dl_error_cnt);

	return 0;
}
ssize_t proc_set_ps_dbg_info(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapter->dvobj;
	struct debug_priv *pdbgpriv = &dvobj->drv_dbg;
	char tmp[32];
	u8 ps_dbg_cmd_id;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%hhx", &ps_dbg_cmd_id);
		if (ps_dbg_cmd_id == 1) /*Clean all*/
			memset(pdbgpriv, 0, sizeof(struct debug_priv));
	}
	return count;
}

#ifdef CONFIG_DBG_COUNTER
int proc_get_rx_logs(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct rx_logs *rx_logs = &adapt->rx_logs;

	RTW_PRINT_SEL(m,
		      "intf_rx=%d\n"
		      "intf_rx_err_recvframe=%d\n"
		      "intf_rx_err_skb=%d\n"
		      "intf_rx_report=%d\n"
		      "core_rx=%d\n"
		      "core_rx_pre=%d\n"
		      "core_rx_pre_ver_err=%d\n"
		      "core_rx_pre_mgmt=%d\n"
		      "core_rx_pre_mgmt_err_80211w=%d\n"
		      "core_rx_pre_mgmt_err=%d\n"
		      "core_rx_pre_ctrl=%d\n"
		      "core_rx_pre_ctrl_err=%d\n"
		      "core_rx_pre_data=%d\n"
		      "core_rx_pre_data_wapi_seq_err=%d\n"
		      "core_rx_pre_data_wapi_key_err=%d\n"
		      "core_rx_pre_data_handled=%d\n"
		      "core_rx_pre_data_err=%d\n"
		      "core_rx_pre_data_unknown=%d\n"
		      "core_rx_pre_unknown=%d\n"
		      "core_rx_enqueue=%d\n"
		      "core_rx_dequeue=%d\n"
		      "core_rx_post=%d\n"
		      "core_rx_post_decrypt=%d\n"
		      "core_rx_post_decrypt_wep=%d\n"
		      "core_rx_post_decrypt_tkip=%d\n"
		      "core_rx_post_decrypt_aes=%d\n"
		      "core_rx_post_decrypt_wapi=%d\n"
		      "core_rx_post_decrypt_hw=%d\n"
		      "core_rx_post_decrypt_unknown=%d\n"
		      "core_rx_post_decrypt_err=%d\n"
		      "core_rx_post_defrag_err=%d\n"
		      "core_rx_post_portctrl_err=%d\n"
		      "core_rx_post_indicate=%d\n"
		      "core_rx_post_indicate_in_oder=%d\n"
		      "core_rx_post_indicate_reoder=%d\n"
		      "core_rx_post_indicate_err=%d\n"
		      "os_indicate=%d\n"
		      "os_indicate_ap_mcast=%d\n"
		      "os_indicate_ap_forward=%d\n"
		      "os_indicate_ap_self=%d\n"
		      "os_indicate_err=%d\n"
		      "os_netif_ok=%d\n"
		      "os_netif_err=%d\n",
		      rx_logs->intf_rx,
		      rx_logs->intf_rx_err_recvframe,
		      rx_logs->intf_rx_err_skb,
		      rx_logs->intf_rx_report,
		      rx_logs->core_rx,
		      rx_logs->core_rx_pre,
		      rx_logs->core_rx_pre_ver_err,
		      rx_logs->core_rx_pre_mgmt,
		      rx_logs->core_rx_pre_mgmt_err_80211w,
		      rx_logs->core_rx_pre_mgmt_err,
		      rx_logs->core_rx_pre_ctrl,
		      rx_logs->core_rx_pre_ctrl_err,
		      rx_logs->core_rx_pre_data,
		      rx_logs->core_rx_pre_data_wapi_seq_err,
		      rx_logs->core_rx_pre_data_wapi_key_err,
		      rx_logs->core_rx_pre_data_handled,
		      rx_logs->core_rx_pre_data_err,
		      rx_logs->core_rx_pre_data_unknown,
		      rx_logs->core_rx_pre_unknown,
		      rx_logs->core_rx_enqueue,
		      rx_logs->core_rx_dequeue,
		      rx_logs->core_rx_post,
		      rx_logs->core_rx_post_decrypt,
		      rx_logs->core_rx_post_decrypt_wep,
		      rx_logs->core_rx_post_decrypt_tkip,
		      rx_logs->core_rx_post_decrypt_aes,
		      rx_logs->core_rx_post_decrypt_wapi,
		      rx_logs->core_rx_post_decrypt_hw,
		      rx_logs->core_rx_post_decrypt_unknown,
		      rx_logs->core_rx_post_decrypt_err,
		      rx_logs->core_rx_post_defrag_err,
		      rx_logs->core_rx_post_portctrl_err,
		      rx_logs->core_rx_post_indicate,
		      rx_logs->core_rx_post_indicate_in_oder,
		      rx_logs->core_rx_post_indicate_reoder,
		      rx_logs->core_rx_post_indicate_err,
		      rx_logs->os_indicate,
		      rx_logs->os_indicate_ap_mcast,
		      rx_logs->os_indicate_ap_forward,
		      rx_logs->os_indicate_ap_self,
		      rx_logs->os_indicate_err,
		      rx_logs->os_netif_ok,
		      rx_logs->os_netif_err
		     );

	return 0;
}

int proc_get_tx_logs(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct tx_logs *tx_logs = &adapt->tx_logs;

	RTW_PRINT_SEL(m,
		      "os_tx=%d\n"
		      "os_tx_err_up=%d\n"
		      "os_tx_err_xmit=%d\n"
		      "os_tx_m2u=%d\n"
		      "os_tx_m2u_ignore_fw_linked=%d\n"
		      "os_tx_m2u_ignore_self=%d\n"
		      "os_tx_m2u_entry=%d\n"
		      "os_tx_m2u_entry_err_xmit=%d\n"
		      "os_tx_m2u_entry_err_skb=%d\n"
		      "os_tx_m2u_stop=%d\n"
		      "core_tx=%d\n"
		      "core_tx_err_pxmitframe=%d\n"
		      "core_tx_err_brtx=%d\n"
		      "core_tx_upd_attrib=%d\n"
		      "core_tx_upd_attrib_adhoc=%d\n"
		      "core_tx_upd_attrib_sta=%d\n"
		      "core_tx_upd_attrib_ap=%d\n"
		      "core_tx_upd_attrib_unknown=%d\n"
		      "core_tx_upd_attrib_dhcp=%d\n"
		      "core_tx_upd_attrib_icmp=%d\n"
		      "core_tx_upd_attrib_active=%d\n"
		      "core_tx_upd_attrib_err_ucast_sta=%d\n"
		      "core_tx_upd_attrib_err_ucast_ap_link=%d\n"
		      "core_tx_upd_attrib_err_sta=%d\n"
		      "core_tx_upd_attrib_err_link=%d\n"
		      "core_tx_upd_attrib_err_sec=%d\n"
		      "core_tx_ap_enqueue_warn_fwstate=%d\n"
		      "core_tx_ap_enqueue_warn_sta=%d\n"
		      "core_tx_ap_enqueue_warn_nosta=%d\n"
		      "core_tx_ap_enqueue_warn_link=%d\n"
		      "core_tx_ap_enqueue_warn_trigger=%d\n"
		      "core_tx_ap_enqueue_mcast=%d\n"
		      "core_tx_ap_enqueue_ucast=%d\n"
		      "core_tx_ap_enqueue=%d\n"
		      "intf_tx=%d\n"
		      "intf_tx_pending_ac=%d\n"
		      "intf_tx_pending_fw_under_survey=%d\n"
		      "intf_tx_pending_fw_under_linking=%d\n"
		      "intf_tx_pending_xmitbuf=%d\n"
		      "intf_tx_enqueue=%d\n"
		      "core_tx_enqueue=%d\n"
		      "core_tx_enqueue_class=%d\n"
		      "core_tx_enqueue_class_err_sta=%d\n"
		      "core_tx_enqueue_class_err_nosta=%d\n"
		      "core_tx_enqueue_class_err_fwlink=%d\n"
		      "intf_tx_direct=%d\n"
		      "intf_tx_direct_err_coalesce=%d\n"
		      "intf_tx_dequeue=%d\n"
		      "intf_tx_dequeue_err_coalesce=%d\n"
		      "intf_tx_dump_xframe=%d\n"
		      "intf_tx_dump_xframe_err_txdesc=%d\n"
		      "intf_tx_dump_xframe_err_port=%d\n",
		      tx_logs->os_tx,
		      tx_logs->os_tx_err_up,
		      tx_logs->os_tx_err_xmit,
		      tx_logs->os_tx_m2u,
		      tx_logs->os_tx_m2u_ignore_fw_linked,
		      tx_logs->os_tx_m2u_ignore_self,
		      tx_logs->os_tx_m2u_entry,
		      tx_logs->os_tx_m2u_entry_err_xmit,
		      tx_logs->os_tx_m2u_entry_err_skb,
		      tx_logs->os_tx_m2u_stop,
		      tx_logs->core_tx,
		      tx_logs->core_tx_err_pxmitframe,
		      tx_logs->core_tx_err_brtx,
		      tx_logs->core_tx_upd_attrib,
		      tx_logs->core_tx_upd_attrib_adhoc,
		      tx_logs->core_tx_upd_attrib_sta,
		      tx_logs->core_tx_upd_attrib_ap,
		      tx_logs->core_tx_upd_attrib_unknown,
		      tx_logs->core_tx_upd_attrib_dhcp,
		      tx_logs->core_tx_upd_attrib_icmp,
		      tx_logs->core_tx_upd_attrib_active,
		      tx_logs->core_tx_upd_attrib_err_ucast_sta,
		      tx_logs->core_tx_upd_attrib_err_ucast_ap_link,
		      tx_logs->core_tx_upd_attrib_err_sta,
		      tx_logs->core_tx_upd_attrib_err_link,
		      tx_logs->core_tx_upd_attrib_err_sec,
		      tx_logs->core_tx_ap_enqueue_warn_fwstate,
		      tx_logs->core_tx_ap_enqueue_warn_sta,
		      tx_logs->core_tx_ap_enqueue_warn_nosta,
		      tx_logs->core_tx_ap_enqueue_warn_link,
		      tx_logs->core_tx_ap_enqueue_warn_trigger,
		      tx_logs->core_tx_ap_enqueue_mcast,
		      tx_logs->core_tx_ap_enqueue_ucast,
		      tx_logs->core_tx_ap_enqueue,
		      tx_logs->intf_tx,
		      tx_logs->intf_tx_pending_ac,
		      tx_logs->intf_tx_pending_fw_under_survey,
		      tx_logs->intf_tx_pending_fw_under_linking,
		      tx_logs->intf_tx_pending_xmitbuf,
		      tx_logs->intf_tx_enqueue,
		      tx_logs->core_tx_enqueue,
		      tx_logs->core_tx_enqueue_class,
		      tx_logs->core_tx_enqueue_class_err_sta,
		      tx_logs->core_tx_enqueue_class_err_nosta,
		      tx_logs->core_tx_enqueue_class_err_fwlink,
		      tx_logs->intf_tx_direct,
		      tx_logs->intf_tx_direct_err_coalesce,
		      tx_logs->intf_tx_dequeue,
		      tx_logs->intf_tx_dequeue_err_coalesce,
		      tx_logs->intf_tx_dump_xframe,
		      tx_logs->intf_tx_dump_xframe_err_txdesc,
		      tx_logs->intf_tx_dump_xframe_err_port
		     );

	return 0;
}

int proc_get_int_logs(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m,
		      "all=%d\n"
		      "err=%d\n"
		      "tbdok=%d\n"
		      "tbder=%d\n"
		      "bcnderr=%d\n"
		      "bcndma=%d\n"
		      "bcndma_e=%d\n"
		      "rx=%d\n"
		      "rx_rdu=%d\n"
		      "rx_fovw=%d\n"
		      "txfovw=%d\n"
		      "mgntok=%d\n"
		      "highdok=%d\n"
		      "bkdok=%d\n"
		      "bedok=%d\n"
		      "vidok=%d\n"
		      "vodok=%d\n",
		      adapt->int_logs.all,
		      adapt->int_logs.err,
		      adapt->int_logs.tbdok,
		      adapt->int_logs.tbder,
		      adapt->int_logs.bcnderr,
		      adapt->int_logs.bcndma,
		      adapt->int_logs.bcndma_e,
		      adapt->int_logs.rx,
		      adapt->int_logs.rx_rdu,
		      adapt->int_logs.rx_fovw,
		      adapt->int_logs.txfovw,
		      adapt->int_logs.mgntok,
		      adapt->int_logs.highdok,
		      adapt->int_logs.bkdok,
		      adapt->int_logs.bedok,
		      adapt->int_logs.vidok,
		      adapt->int_logs.vodok
		     );

	return 0;
}

#endif /* CONFIG_DBG_COUNTER */

int proc_get_hw_status(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapt->dvobj;
	struct debug_priv *pdbgpriv = &dvobj->drv_dbg;
	struct registry_priv *regsty = dvobj_to_regsty(dvobj);

	if (regsty->check_hw_status == 0)
		RTW_PRINT_SEL(m, "RX FIFO full count: not check in watch dog\n");
	else if (pdbgpriv->dbg_rx_fifo_last_overflow == 1
	    && pdbgpriv->dbg_rx_fifo_curr_overflow == 1
	    && pdbgpriv->dbg_rx_fifo_diff_overflow == 1
	   )
		RTW_PRINT_SEL(m, "RX FIFO full count: no implementation\n");
	else {
		RTW_PRINT_SEL(m, "RX FIFO full count: last_time=%llu, current_time=%llu, differential=%llu\n"
			, pdbgpriv->dbg_rx_fifo_last_overflow, pdbgpriv->dbg_rx_fifo_curr_overflow, pdbgpriv->dbg_rx_fifo_diff_overflow);
	}

	return 0;
}

ssize_t proc_set_hw_status(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapt->dvobj;
	struct registry_priv *regsty = dvobj_to_regsty(dvobj);
	char tmp[32];
	u32 enable;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%d ", &enable);
		if (regsty && enable <= 1) {
			regsty->check_hw_status = enable;
			RTW_INFO("check_hw_status=%d\n", regsty->check_hw_status);
		}
	}

	return count;
}

int proc_get_trx_info_debug(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	/*============  tx info ============	*/
	rtw_hal_get_def_var(adapt, HW_DEF_RA_INFO_DUMP, m);

	/*============  rx info ============	*/
	rtw_hal_set_odm_var(adapt, HAL_ODM_RX_INFO_DUMP, m, false);


	return 0;
}

int proc_get_rx_signal(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m, "rssi:%d\n", adapt->recvpriv.rssi);
	RTW_PRINT_SEL(m, "signal_strength:%u\n", adapt->recvpriv.signal_strength);
	RTW_PRINT_SEL(m, "signal_qual:%u\n", adapt->recvpriv.signal_qual);
	return 0;
}

ssize_t proc_set_rx_signal(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 is_signal_dbg, signal_strength;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%u %u", &is_signal_dbg, &signal_strength);

		is_signal_dbg = is_signal_dbg == 0 ? 0 : 1;

		if (is_signal_dbg && num != 2)
			return count;

		signal_strength = signal_strength > 100 ? 100 : signal_strength;

		adapt->recvpriv.is_signal_dbg = is_signal_dbg;
		adapt->recvpriv.signal_strength_dbg = signal_strength;

		if (is_signal_dbg)
			RTW_INFO("set %s %u\n", "DBG_SIGNAL_STRENGTH", signal_strength);
		else
			RTW_INFO("set %s\n", "HW_SIGNAL_STRENGTH");

	}

	return count;

}

int proc_get_ht_enable(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	if (pregpriv)
		RTW_PRINT_SEL(m, "%d\n", pregpriv->ht_enable);

	return 0;
}

ssize_t proc_set_ht_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%d ", &mode);
		if (pregpriv && mode < 2) {
			pregpriv->ht_enable = mode;
			RTW_INFO("ht_enable=%d\n", pregpriv->ht_enable);
		}
	}
	return count;
}

int proc_get_bw_mode(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	if (pregpriv)
		RTW_PRINT_SEL(m, "0x%02x\n", pregpriv->bw_mode);
	return 0;
}

ssize_t proc_set_bw_mode(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	char tmp[32];
	u32 mode;
	u8 bw_2g;
	u8 bw_5g;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}
	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%x ", &mode);
		bw_5g = mode >> 4;
		bw_2g = mode & 0x0f;

		if (pregpriv && bw_2g <= 4 && bw_5g <= 4) {

			pregpriv->bw_mode = mode;
			printk("bw_mode=0x%x\n", mode);
		}
	}
	return count;
}

int proc_get_ampdu_enable(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	if (pregpriv)
		RTW_PRINT_SEL(m, "%d\n", pregpriv->ampdu_enable);

	return 0;
}

ssize_t proc_set_ampdu_enable(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%d ", &mode);
		if (pregpriv && mode < 2) {
			pregpriv->ampdu_enable = mode;
			printk("ampdu_enable=%d\n", mode);
		}

	}
	return count;
}

int proc_get_mac_rptbuf(struct seq_file *m, void *v)
{
	return 0;
}

void dump_regsty_rx_ampdu_size_limit(void *sel, struct adapter *adapter)
{
	struct registry_priv *regsty = adapter_to_regsty(adapter);
	int i;

	RTW_PRINT_SEL(sel, "%-3s %-3s %-3s %-3s %-4s\n"
		, "", "20M", "40M", "80M", "160M");
	for (i = 0; i < 4; i++)
		RTW_PRINT_SEL(sel, "%dSS %3u %3u %3u %4u\n", i + 1
			, regsty->rx_ampdu_sz_limit_by_nss_bw[i][0]
			, regsty->rx_ampdu_sz_limit_by_nss_bw[i][1]
			, regsty->rx_ampdu_sz_limit_by_nss_bw[i][2]
			, regsty->rx_ampdu_sz_limit_by_nss_bw[i][3]);
}

int proc_get_rx_ampdu(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	_RTW_PRINT_SEL(m, "accept: ");
	if (adapt->fix_rx_ampdu_accept == RX_AMPDU_ACCEPT_INVALID)
		RTW_PRINT_SEL(m, "%u%s\n", rtw_rx_ampdu_is_accept(adapt), "(auto)");
	else
		RTW_PRINT_SEL(m, "%u%s\n", adapt->fix_rx_ampdu_accept, "(fixed)");

	_RTW_PRINT_SEL(m, "size: ");
	if (adapt->fix_rx_ampdu_size == RX_AMPDU_SIZE_INVALID) {
		RTW_PRINT_SEL(m, "%u%s\n", rtw_rx_ampdu_size(adapt), "(auto) with conditional limit:");
		dump_regsty_rx_ampdu_size_limit(m, adapt);
	} else
		RTW_PRINT_SEL(m, "%u%s\n", adapt->fix_rx_ampdu_size, "(fixed)");
	RTW_PRINT_SEL(m, "\n");

	RTW_PRINT_SEL(m, "%19s %17s\n", "fix_rx_ampdu_accept", "fix_rx_ampdu_size");

	_RTW_PRINT_SEL(m, "%-19d %-17u\n"
		, adapt->fix_rx_ampdu_accept
		, adapt->fix_rx_ampdu_size);

	return 0;
}

ssize_t proc_set_rx_ampdu(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u8 accept;
	u8 size;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhu %hhu", &accept, &size);

		if (num >= 1)
			rtw_rx_ampdu_set_accept(adapt, accept, RX_AMPDU_DRV_FIXED);
		if (num >= 2)
			rtw_rx_ampdu_set_size(adapt, size, RX_AMPDU_DRV_FIXED);

		rtw_rx_ampdu_apply(adapt);
	}
	return count;
}

int proc_get_rx_ampdu_factor(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	if (adapt)
		RTW_PRINT_SEL(m, "rx ampdu factor = %x\n", adapt->driver_rx_ampdu_factor);

	return 0;
}

ssize_t proc_set_rx_ampdu_factor(struct file *file, const char __user *buffer
				 , size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 factor;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%d ", &factor);

		if (adapt && (num == 1)) {
			RTW_INFO("adapt->driver_rx_ampdu_factor = %x\n", factor);

			if (factor  > 0x03)
				adapt->driver_rx_ampdu_factor = 0xFF;
			else
				adapt->driver_rx_ampdu_factor = factor;
		}
	}

	return count;
}

int proc_get_tx_max_agg_num(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	if (adapt)
		RTW_PRINT_SEL(m, "tx max AMPDU num = 0x%02x\n", adapt->driver_tx_max_agg_num);

	return 0;
}

ssize_t proc_set_tx_max_agg_num(struct file *file, const char __user *buffer
				 , size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u8 agg_num;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%hhx ", &agg_num);

		if (adapt && (num == 1)) {
			RTW_INFO("adapt->driver_tx_max_agg_num = 0x%02x\n", agg_num);

			adapt->driver_tx_max_agg_num = agg_num;
		}
	}

	return count;
}

int proc_get_rx_ampdu_density(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	if (adapt)
		RTW_PRINT_SEL(m, "rx ampdu densityg = %x\n", adapt->driver_rx_ampdu_spacing);

	return 0;
}

ssize_t proc_set_rx_ampdu_density(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 density;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%d ", &density);

		if (adapt && (num == 1)) {
			RTW_INFO("adapt->driver_rx_ampdu_spacing = %x\n", density);

			if (density > 0x07)
				adapt->driver_rx_ampdu_spacing = 0xFF;
			else
				adapt->driver_rx_ampdu_spacing = density;
		}
	}

	return count;
}

int proc_get_tx_ampdu_density(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);


	if (adapt)
		RTW_PRINT_SEL(m, "tx ampdu density = %x\n", adapt->driver_ampdu_spacing);

	return 0;
}

ssize_t proc_set_tx_ampdu_density(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 density;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%d ", &density);

		if (adapt && (num == 1)) {
			RTW_INFO("adapt->driver_ampdu_spacing = %x\n", density);

			if (density > 0x07)
				adapt->driver_ampdu_spacing = 0xFF;
			else
				adapt->driver_ampdu_spacing = density;
		}
	}

	return count;
}

#ifdef CONFIG_TX_AMSDU
int proc_get_tx_amsdu(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;

	if (adapt)
	{
		RTW_PRINT_SEL(m, "tx amsdu = %d\n", adapt->tx_amsdu);
		RTW_PRINT_SEL(m, "amsdu set timer conut = %u\n", pxmitpriv->amsdu_debug_set_timer);
		RTW_PRINT_SEL(m, "amsdu  time out count = %u\n", pxmitpriv->amsdu_debug_timeout);
		RTW_PRINT_SEL(m, "amsdu coalesce one count = %u\n", pxmitpriv->amsdu_debug_coalesce_one);
		RTW_PRINT_SEL(m, "amsdu coalesce two count = %u\n", pxmitpriv->amsdu_debug_coalesce_two);
	}

	return 0;
}

ssize_t proc_set_tx_amsdu(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct xmit_priv *pxmitpriv = &adapt->xmitpriv;
	char tmp[32];
	u32 amsdu;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%d ", &amsdu);

		if (adapt && (num == 1)) {
			RTW_INFO("adapt->tx_amsdu = %x\n", amsdu);

			if (amsdu > 3)
				adapt->tx_amsdu = 0;
			else if(amsdu == 3)
			{
				pxmitpriv->amsdu_debug_set_timer = 0;
				pxmitpriv->amsdu_debug_timeout = 0;
				pxmitpriv->amsdu_debug_coalesce_one = 0;
				pxmitpriv->amsdu_debug_coalesce_two = 0;
			}
			else
				adapt->tx_amsdu = amsdu;
		}
	}

	return count;
}

int proc_get_tx_amsdu_rate(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	if (adapt)
		RTW_PRINT_SEL(m, "tx amsdu rate = %d Mbps\n", adapt->tx_amsdu_rate);

	return 0;
}

ssize_t proc_set_tx_amsdu_rate(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 amsdu_rate;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {

		int num = sscanf(tmp, "%d ", &amsdu_rate);

		if (adapt && (num == 1)) {
			RTW_INFO("adapt->tx_amsdu_rate = %x\n", amsdu_rate);
			adapt->tx_amsdu_rate = amsdu_rate;
		}
	}

	return count;
}
#endif /* CONFIG_TX_AMSDU */

int proc_get_en_fwps(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	if (pregpriv)
		RTW_PRINT_SEL(m, "check_fw_ps = %d , 1:enable get FW PS state , 0: disable get FW PS state\n"
			      , pregpriv->check_fw_ps);

	return 0;
}

ssize_t proc_set_en_fwps(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%d ", &mode);
		if (pregpriv &&  mode < 2) {
			pregpriv->check_fw_ps = mode;
			RTW_INFO("pregpriv->check_fw_ps=%d\n", pregpriv->check_fw_ps);
		}
	}
	return count;
}

void rtw_dump_dft_phy_cap(void *sel, struct adapter *adapter)
{
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct ht_priv	*phtpriv = &pmlmepriv->htpriv;

	RTW_PRINT_SEL(sel, "[DFT CAP] HT STBC Tx : %s\n", (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX)) ? "V" : "X");
	RTW_PRINT_SEL(sel, "[DFT CAP] HT STBC Rx : %s\n\n", (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_RX)) ? "V" : "X");

	RTW_PRINT_SEL(sel, "[DFT CAP] HT LDPC Tx : %s\n", (TEST_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_TX)) ? "V" : "X");
	RTW_PRINT_SEL(sel, "[DFT CAP] HT LDPC Rx : %s\n\n", (TEST_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_RX)) ? "V" : "X");
}

void rtw_get_dft_phy_cap(void *sel, struct adapter *adapter)
{
	RTW_PRINT_SEL(sel, "\n ======== PHY CAP protocol ========\n");
	rtw_ht_use_default_setting(adapter);
	rtw_dump_dft_phy_cap(sel, adapter);
}

void rtw_dump_drv_phy_cap(void *sel, struct adapter *adapter)
{
	struct registry_priv	*pregistry_priv = &adapter->registrypriv;

	RTW_PRINT_SEL(sel, "\n ======== DRV's configuration ========\n");
	RTW_PRINT_SEL(sel, "[DRV CAP] STBC Capability : 0x%02x\n", pregistry_priv->stbc_cap);
	RTW_PRINT_SEL(sel, "[DRV CAP] VHT STBC Tx : %s\n", (TEST_FLAG(pregistry_priv->stbc_cap, BIT1)) ? "V" : "X"); /*BIT1: Enable VHT STBC Tx*/
	RTW_PRINT_SEL(sel, "[DRV CAP] VHT STBC Rx : %s\n", (TEST_FLAG(pregistry_priv->stbc_cap, BIT0)) ? "V" : "X"); /*BIT0: Enable VHT STBC Rx*/
	RTW_PRINT_SEL(sel, "[DRV CAP] HT STBC Tx : %s\n", (TEST_FLAG(pregistry_priv->stbc_cap, BIT5)) ? "V" : "X"); /*BIT5: Enable HT STBC Tx*/
	RTW_PRINT_SEL(sel, "[DRV CAP] HT STBC Rx : %s\n\n", (TEST_FLAG(pregistry_priv->stbc_cap, BIT4)) ? "V" : "X"); /*BIT4: Enable HT STBC Rx*/

	RTW_PRINT_SEL(sel, "[DRV CAP] LDPC Capability : 0x%02x\n", pregistry_priv->ldpc_cap);
	RTW_PRINT_SEL(sel, "[DRV CAP] VHT LDPC Tx : %s\n", (TEST_FLAG(pregistry_priv->ldpc_cap, BIT1)) ? "V" : "X"); /*BIT1: Enable VHT LDPC Tx*/
	RTW_PRINT_SEL(sel, "[DRV CAP] VHT LDPC Rx : %s\n", (TEST_FLAG(pregistry_priv->ldpc_cap, BIT0)) ? "V" : "X"); /*BIT0: Enable VHT LDPC Rx*/
	RTW_PRINT_SEL(sel, "[DRV CAP] HT LDPC Tx : %s\n", (TEST_FLAG(pregistry_priv->ldpc_cap, BIT5)) ? "V" : "X"); /*BIT5: Enable HT LDPC Tx*/
	RTW_PRINT_SEL(sel, "[DRV CAP] HT LDPC Rx : %s\n\n", (TEST_FLAG(pregistry_priv->ldpc_cap, BIT4)) ? "V" : "X"); /*BIT4: Enable HT LDPC Rx*/
}

int proc_get_stbc_cap(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	if (pregpriv)
		RTW_PRINT_SEL(m, "0x%02x\n", pregpriv->stbc_cap);

	return 0;
}

ssize_t proc_set_stbc_cap(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%d ", &mode);
		if (pregpriv) {
			pregpriv->stbc_cap = mode;
			RTW_INFO("stbc_cap = 0x%02x\n", mode);
		}
	}
	return count;
}
int proc_get_rx_stbc(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	if (pregpriv)
		RTW_PRINT_SEL(m, "%d\n", pregpriv->rx_stbc);

	return 0;
}

ssize_t proc_set_rx_stbc(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%d ", &mode);
		if (pregpriv && (mode == 0 || mode == 1 || mode == 2 || mode == 3)) {
			pregpriv->rx_stbc = mode;
			printk("rx_stbc=%d\n", mode);
		}
	}
	return count;
}

int proc_get_ldpc_cap(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;

	if (pregpriv)
		RTW_PRINT_SEL(m, "0x%02x\n", pregpriv->ldpc_cap);

	return 0;
}

ssize_t proc_set_ldpc_cap(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &adapt->registrypriv;
	char tmp[32];
	u32 mode;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		sscanf(tmp, "%d ", &mode);
		if (pregpriv) {
			pregpriv->ldpc_cap = mode;
			RTW_INFO("ldpc_cap = 0x%02x\n", mode);
		}
	}
	return count;
}

int proc_get_all_sta_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	unsigned long irqL;
	struct sta_info *psta;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct sta_priv *pstapriv = &adapt->stapriv;
	int i;
	struct list_head	*plist, *phead;

	RTW_MAP_DUMP_SEL(m, "sta_dz_bitmap=", pstapriv->sta_dz_bitmap, pstapriv->aid_bmp_len);
	RTW_MAP_DUMP_SEL(m, "tim_bitmap=", pstapriv->tim_bitmap, pstapriv->aid_bmp_len);

	_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	for (i = 0; i < NUM_STA; i++) {
		phead = &(pstapriv->sta_hash[i]);
		plist = get_next(phead);

		while ((!rtw_end_of_queue_search(phead, plist))) {
			psta = container_of(plist, struct sta_info, hash_list);

			plist = get_next(plist);

			/* if(extra_arg == psta->cmn.aid) */
			{
				RTW_PRINT_SEL(m, "==============================\n");
				RTW_PRINT_SEL(m, "sta's macaddr:" MAC_FMT "\n", MAC_ARG(psta->cmn.mac_addr));
				RTW_PRINT_SEL(m, "rtsen=%d, cts2slef=%d\n", psta->rtsen, psta->cts2self);
				RTW_PRINT_SEL(m, "state=0x%x, aid=%d, macid=%d, raid=%d\n",
					psta->state, psta->cmn.aid, psta->cmn.mac_id, psta->cmn.ra_info.rate_id);
				RTW_PRINT_SEL(m, "qos_en=%d, ht_en=%d, init_rate=%d\n", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
				RTW_PRINT_SEL(m, "bwmode=%d, ch_offset=%d, sgi_20m=%d,sgi_40m=%d\n"
					, psta->cmn.bw_mode, psta->htpriv.ch_offset, psta->htpriv.sgi_20m, psta->htpriv.sgi_40m);
				RTW_PRINT_SEL(m, "ampdu_enable = %d\n", psta->htpriv.ampdu_enable);
				RTW_PRINT_SEL(m, "tx_amsdu_enable = %d\n", psta->htpriv.tx_amsdu_enable);
				RTW_PRINT_SEL(m, "agg_enable_bitmap=%x, candidate_tid_bitmap=%x\n", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);
				RTW_PRINT_SEL(m, "sleepq_len=%d\n", psta->sleepq_len);
				RTW_PRINT_SEL(m, "sta_xmitpriv.vo_q_qcnt=%d\n", psta->sta_xmitpriv.vo_q.qcnt);
				RTW_PRINT_SEL(m, "sta_xmitpriv.vi_q_qcnt=%d\n", psta->sta_xmitpriv.vi_q.qcnt);
				RTW_PRINT_SEL(m, "sta_xmitpriv.be_q_qcnt=%d\n", psta->sta_xmitpriv.be_q.qcnt);
				RTW_PRINT_SEL(m, "sta_xmitpriv.bk_q_qcnt=%d\n", psta->sta_xmitpriv.bk_q.qcnt);

				RTW_PRINT_SEL(m, "capability=0x%x\n", psta->capability);
				RTW_PRINT_SEL(m, "flags=0x%x\n", psta->flags);
				RTW_PRINT_SEL(m, "wpa_psk=0x%x\n", psta->wpa_psk);
				RTW_PRINT_SEL(m, "wpa2_group_cipher=0x%x\n", psta->wpa2_group_cipher);
				RTW_PRINT_SEL(m, "wpa2_pairwise_cipher=0x%x\n", psta->wpa2_pairwise_cipher);
				RTW_PRINT_SEL(m, "qos_info=0x%x\n", psta->qos_info);
				RTW_PRINT_SEL(m, "dot118021XPrivacy=0x%x\n", psta->dot118021XPrivacy);

				sta_rx_reorder_ctl_dump(m, psta);

				RTW_PRINT_SEL(m, "rx_data_uc_pkts=%llu\n", sta_rx_data_uc_pkts(psta));
				RTW_PRINT_SEL(m, "rx_data_mc_pkts=%llu\n", psta->sta_stats.rx_data_mc_pkts);
				RTW_PRINT_SEL(m, "rx_data_bc_pkts=%llu\n", psta->sta_stats.rx_data_bc_pkts);
				RTW_PRINT_SEL(m, "rx_uc_bytes=%llu\n", sta_rx_uc_bytes(psta));
				RTW_PRINT_SEL(m, "rx_mc_bytes=%llu\n", psta->sta_stats.rx_mc_bytes);
				RTW_PRINT_SEL(m, "rx_bc_bytes=%llu\n", psta->sta_stats.rx_bc_bytes);
				RTW_PRINT_SEL(m, "rx_avg_tp =%d (Bps)\n", psta->cmn.rx_moving_average_tp);

				RTW_PRINT_SEL(m, "tx_data_pkts=%llu\n", psta->sta_stats.tx_pkts);
				RTW_PRINT_SEL(m, "tx_bytes=%llu\n", psta->sta_stats.tx_bytes);
				RTW_PRINT_SEL(m, "tx_avg_tp =%d (MBps)\n", psta->cmn.tx_moving_average_tp);
				dump_st_ctl(m, &psta->st_ctl);

				if (STA_OP_WFD_MODE(psta))
					RTW_PRINT_SEL(m, "op_wfd_mode:0x%02x\n", STA_OP_WFD_MODE(psta));

				RTW_PRINT_SEL(m, "==============================\n");
			}

		}

	}

	_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);

	return 0;
}

int proc_get_btcoex_dbg(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter * adapt;
	char buf[512] = {0};
	adapt = (struct adapter *)rtw_netdev_priv(dev);

	rtw_btcoex_GetDBG(adapt, buf, 512);

	_RTW_PRINT_SEL(m, "%s", buf);

	return 0;
}

ssize_t proc_set_btcoex_dbg(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter * adapt;
	u8 tmp[80] = {0};
	u32 module[2] = {0};
	u32 num;

	adapt = (struct adapter *)rtw_netdev_priv(dev);
	if (!buffer) {
		RTW_INFO(FUNC_ADPT_FMT ": input buffer is NULL!\n",
			 FUNC_ADPT_ARG(adapt));

		return -EFAULT;
	}
	if (count < 1) {
		RTW_INFO(FUNC_ADPT_FMT ": input length is 0!\n",
			 FUNC_ADPT_ARG(adapt));

		return -EFAULT;
	}
	num = count;
	if (num > (sizeof(tmp) - 1))
		num = (sizeof(tmp) - 1);

	if (copy_from_user(tmp, buffer, num)) {
		RTW_INFO(FUNC_ADPT_FMT ": copy buffer from user space FAIL!\n",
			 FUNC_ADPT_ARG(adapt));

		return -EFAULT;
	}

	num = sscanf(tmp, "%x %x", module, module + 1);
	if (1 == num) {
		if (0 == module[0])
			memset(module, 0, sizeof(module));
		else
			memset(module, 0xFF, sizeof(module));
	} else if (2 != num) {
		RTW_INFO(FUNC_ADPT_FMT ": input(\"%s\") format incorrect!\n",
			 FUNC_ADPT_ARG(adapt), tmp);

		if (0 == num)
			return -EFAULT;
	}

	RTW_INFO(FUNC_ADPT_FMT ": input 0x%08X 0x%08X\n",
		 FUNC_ADPT_ARG(adapt), module[0], module[1]);
	rtw_btcoex_SetDBG(adapt, module);

	return count;
}

int proc_get_btcoex_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter * adapt;
	const u32 bufsize = 30 * 100;
	u8 *pbuf = NULL;

	adapt = (struct adapter *)rtw_netdev_priv(dev);

	pbuf = rtw_zmalloc(bufsize);
	if (!pbuf)
		return -ENOMEM;

	rtw_btcoex_DisplayBtCoexInfo(adapt, pbuf, bufsize);

	_RTW_PRINT_SEL(m, "%s\n", pbuf);

	rtw_mfree(pbuf, bufsize);

	return 0;
}

int proc_get_new_bcn_max(struct seq_file *m, void *v)
{
	RTW_PRINT_SEL(m, "%d", new_bcn_max);
	return 0;
}

ssize_t proc_set_new_bcn_max(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];
	extern int new_bcn_max;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count))
		sscanf(tmp, "%d ", &new_bcn_max);

	return count;
}

int proc_get_ps_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	u8 ips_mode = pwrpriv->ips_mode;
	u8 lps_mode = pwrpriv->power_mgnt;
	u8 lps_level = pwrpriv->lps_level;
	char *str = "";

	RTW_PRINT_SEL(m, "======Power Saving Info:======\n");
	RTW_PRINT_SEL(m, "*IPS:\n");

	if (ips_mode == IPS_NORMAL) {
#ifdef CONFIG_FWLPS_IN_IPS
		str = "FW_LPS_IN_IPS";
#else
		str = "Card Disable";
#endif
	} else if (ips_mode == IPS_NONE)
		str = "NO IPS";
	else if (ips_mode == IPS_LEVEL_2)
		str = "IPS_LEVEL_2";
	else
		str = "invalid ips_mode";

	RTW_PRINT_SEL(m, " IPS mode: %s\n", str);
	RTW_PRINT_SEL(m, " IPS enter count:%d, IPS leave count:%d\n",
		      pwrpriv->ips_enter_cnts, pwrpriv->ips_leave_cnts);
	RTW_PRINT_SEL(m, "------------------------------\n");
	RTW_PRINT_SEL(m, "*LPS:\n");

	if (lps_mode == PS_MODE_ACTIVE)
		str = "NO LPS";
	else if (lps_mode == PS_MODE_MIN)
		str = "MIN";
	else if (lps_mode == PS_MODE_MAX)
		str = "MAX";
	else if (lps_mode == PS_MODE_DTIM)
		str = "DTIM";
	else
		sprintf(str, "%d", lps_mode);

	RTW_PRINT_SEL(m, " LPS mode: %s\n", str);

	if (pwrpriv->dtim != 0)
		RTW_PRINT_SEL(m, " DTIM: %d\n", pwrpriv->dtim);
	RTW_PRINT_SEL(m, " LPS enter count:%d, LPS leave count:%d\n",
		      pwrpriv->lps_enter_cnts, pwrpriv->lps_leave_cnts);

	if (lps_level == LPS_LCLK)
		str = "LPS_LCLK";
	else if  (lps_level == LPS_PG)
		str = "LPS_PG";
	else
		str = "LPS_NORMAL";
	RTW_PRINT_SEL(m, " LPS level: %s\n", str);

	RTW_PRINT_SEL(m, "=============================\n");
	return 0;
}

int proc_get_monitor(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);

	if (WIFI_MONITOR_STATE == get_fwstate(pmlmepriv)) {
		RTW_PRINT_SEL(m, "Monitor mode : Enable\n");

		RTW_PRINT_SEL(m, "ch=%d, ch_offset=%d, bw=%d\n",
			rtw_get_oper_ch(adapt), rtw_get_oper_choffset(adapt), rtw_get_oper_bw(adapt));
	} else
		RTW_PRINT_SEL(m, "Monitor mode : Disable\n");

	return 0;
}

ssize_t proc_set_monitor(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 target_chan, target_offset, target_bw;

	if (count < 3) {
		RTW_INFO("argument size is less than 3\n");
		return -EFAULT;
	}

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%hhu %hhu %hhu", &target_chan, &target_offset, &target_bw);

		if (num != 3) {
			RTW_INFO("invalid write_reg parameter!\n");
			return count;
		}

		adapt->mlmeextpriv.cur_channel  = target_chan;
		set_channel_bwmode(adapt, target_chan, target_offset, target_bw);
	}

	return count;
}

#include <hal_data.h>
int proc_get_efuse_map(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct hal_com_data * pHalData = GET_HAL_DATA(adapt);
	struct pwrctrl_priv *pwrctrlpriv  = adapter_to_pwrctl(adapt);
	struct efuse_hal * pEfuseHal = &pHalData->EfuseHal;
	int i, j;
	u8 ips_mode = IPS_NUM;
	u16 mapLen;

	EFUSE_GetEfuseDefinition(adapt, EFUSE_WIFI, TYPE_EFUSE_MAP_LEN, (void *)&mapLen, false);
	if (mapLen > EFUSE_MAX_MAP_LEN)
		mapLen = EFUSE_MAX_MAP_LEN;

	ips_mode = pwrctrlpriv->ips_mode;
	rtw_pm_set_ips(adapt, IPS_NONE);

	if (pHalData->efuse_file_status == EFUSE_FILE_LOADED) {
		RTW_PRINT_SEL(m, "File eFuse Map loaded! file path:%s\nDriver eFuse Map From File\n", EFUSE_MAP_PATH);
		if (pHalData->bautoload_fail_flag)
			RTW_PRINT_SEL(m, "File Autoload fail!!!\n");
	} else if (pHalData->efuse_file_status ==  EFUSE_FILE_FAILED) {
		RTW_PRINT_SEL(m, "Open File eFuse Map Fail ! file path:%s\nDriver eFuse Map From Default\n", EFUSE_MAP_PATH);
		if (pHalData->bautoload_fail_flag)
			RTW_PRINT_SEL(m, "HW Autoload fail!!!\n");
	} else {
		RTW_PRINT_SEL(m, "Driver eFuse Map From HW\n");
		if (pHalData->bautoload_fail_flag)
			RTW_PRINT_SEL(m, "HW Autoload fail!!!\n");
	}
	for (i = 0; i < mapLen; i += 16) {
		RTW_PRINT_SEL(m, "0x%02x\t", i);
		for (j = 0; j < 8; j++)
			RTW_PRINT_SEL(m, "%02X ", pHalData->efuse_eeprom_data[i + j]);
		RTW_PRINT_SEL(m, "\t");
		for (; j < 16; j++)
			RTW_PRINT_SEL(m, "%02X ", pHalData->efuse_eeprom_data[i + j]);
		RTW_PRINT_SEL(m, "\n");
	}

	if (rtw_efuse_map_read(adapt, 0, mapLen, pEfuseHal->fakeEfuseInitMap) == _FAIL) {
		RTW_PRINT_SEL(m, "WARN - Read Realmap Failed\n");
		return 0;
	}

	RTW_PRINT_SEL(m, "\n");
	RTW_PRINT_SEL(m, "HW eFuse Map\n");
	for (i = 0; i < mapLen; i += 16) {
		RTW_PRINT_SEL(m, "0x%02x\t", i);
		for (j = 0; j < 8; j++)
			RTW_PRINT_SEL(m, "%02X ", pEfuseHal->fakeEfuseInitMap[i + j]);
		RTW_PRINT_SEL(m, "\t");
		for (; j < 16; j++)
			RTW_PRINT_SEL(m, "%02X ", pEfuseHal->fakeEfuseInitMap[i + j]);
		RTW_PRINT_SEL(m, "\n");
	}

	rtw_pm_set_ips(adapt, ips_mode);

	return 0;
}

ssize_t proc_set_efuse_map(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	return count;
}

#ifdef CONFIG_IEEE80211W
ssize_t proc_set_tx_sa_query(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct	sta_priv *pstapriv = &adapt->stapriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	struct sta_info *psta;
	struct list_head	*plist, *phead;
	unsigned long	 irqL;
	char tmp[16];
	u8	mac_addr[NUM_STA][ETH_ALEN];
	u32 key_type;
	u8 index;

	if (count > 2) {
		RTW_INFO("argument size is more than 2\n");
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {
		int num = sscanf(tmp, "%x", &key_type);

		if (num !=  1) {
			RTW_INFO("invalid read_reg parameter!\n");
			return count;
		}
		RTW_INFO("0: set sa query request , key_type=%d\n", key_type);
	}

	if ((check_fwstate(pmlmepriv, WIFI_STATION_STATE))
	    && (check_fwstate(pmlmepriv, _FW_LINKED)) && SEC_IS_BIP_KEY_INSTALLED(&adapt->securitypriv)) {
		RTW_INFO("STA:"MAC_FMT"\n", MAC_ARG(get_my_bssid(&(pmlmeinfo->network))));
		/* TX unicast sa_query to AP */
		issue_action_SA_Query(adapt, get_my_bssid(&(pmlmeinfo->network)), 0, 0, (u8)key_type);
	} else if (check_fwstate(pmlmepriv, WIFI_AP_STATE) && SEC_IS_BIP_KEY_INSTALLED(&adapt->securitypriv)) {
		/* TX unicast sa_query to every client STA */
		_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);
		for (index = 0; index < NUM_STA; index++) {
			psta = NULL;

			phead = &(pstapriv->sta_hash[index]);
			plist = get_next(phead);

			while ((!rtw_end_of_queue_search(phead, plist))) {
				psta = container_of(plist, struct sta_info, hash_list);
				plist = get_next(plist);
				memcpy(&mac_addr[psta->cmn.mac_id][0], psta->cmn.mac_addr, ETH_ALEN);
			}
		}
		_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);

		for (index = 0; index < macid_ctl->num && index < NUM_STA; index++) {
			if (rtw_macid_is_used(macid_ctl, index) && !rtw_macid_is_bmc(macid_ctl, index)) {
				if (memcmp(get_my_bssid(&(pmlmeinfo->network)), &mac_addr[index][0], ETH_ALEN)
				    && !IS_MCAST(&mac_addr[index][0])) {
					issue_action_SA_Query(adapt, &mac_addr[index][0], 0, 0, (u8)key_type);
					RTW_INFO("STA[%u]:"MAC_FMT"\n", index , MAC_ARG(&mac_addr[index][0]));
				}
			}
		}
	}

	return count;
}

int proc_get_tx_sa_query(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m, "%s\n", __func__);
	return 0;
}

ssize_t proc_set_tx_deauth(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct	sta_priv *pstapriv = &adapt->stapriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	struct sta_info *psta;
	struct list_head	*plist, *phead;
	unsigned long	 irqL;
	char tmp[16];
	u8	mac_addr[NUM_STA][ETH_ALEN];
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u32 key_type;
	u8 index;


	if (count > 2) {
		RTW_INFO("argument size is more than 2\n");
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {
		int num = sscanf(tmp, "%x", &key_type);

		if (num !=  1) {
			RTW_INFO("invalid read_reg parameter!\n");
			return count;
		}
		RTW_INFO("key_type=%d\n", key_type);
	}
	if (key_type < 0 || key_type > 4)
		return count;

	if ((check_fwstate(pmlmepriv, WIFI_STATION_STATE))
	    && (check_fwstate(pmlmepriv, _FW_LINKED))) {
		if (key_type == 3) /* key_type 3 only for AP mode */
			return count;
		/* TX unicast deauth to AP */
		issue_deauth_11w(adapt, get_my_bssid(&(pmlmeinfo->network)), 0, (u8)key_type);
	} else if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {

		if (key_type == 3)
			issue_deauth_11w(adapt, bc_addr, 0, IEEE80211W_RIGHT_KEY);

		/* TX unicast deauth to every client STA */
		_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);
		for (index = 0; index < NUM_STA; index++) {
			psta = NULL;

			phead = &(pstapriv->sta_hash[index]);
			plist = get_next(phead);

			while ((!rtw_end_of_queue_search(phead, plist))) {
				psta = container_of(plist, struct sta_info, hash_list);
				plist = get_next(plist);
				memcpy(&mac_addr[psta->cmn.mac_id][0], psta->cmn.mac_addr, ETH_ALEN);
			}
		}
		_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);

		for (index = 0; index < macid_ctl->num && index < NUM_STA; index++) {
			if (rtw_macid_is_used(macid_ctl, index) && !rtw_macid_is_bmc(macid_ctl, index)) {
				if (memcmp(get_my_bssid(&(pmlmeinfo->network)), &mac_addr[index][0], ETH_ALEN)) {
					if (key_type != 3)
						issue_deauth_11w(adapt, &mac_addr[index][0], 0, (u8)key_type);

					psta = rtw_get_stainfo(pstapriv, &mac_addr[index][0]);
					if (psta && key_type != IEEE80211W_WRONG_KEY && key_type != IEEE80211W_NO_KEY) {
						u8 updated = false;

						_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
						if (!rtw_is_list_empty(&psta->asoc_list)) {
							rtw_list_delete(&psta->asoc_list);
							pstapriv->asoc_list_cnt--;
							updated = ap_free_sta(adapt, psta, false, WLAN_REASON_PREV_AUTH_NOT_VALID, true);

						}
						_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

						associated_clients_update(adapt, updated, STA_INFO_UPDATE_ALL);
					}

					RTW_INFO("STA[%u]:"MAC_FMT"\n", index , MAC_ARG(&mac_addr[index][0]));
				}
			}
		}
	}

	return count;
}

int proc_get_tx_deauth(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m, "%s\n", __func__);
	return 0;
}

ssize_t proc_set_tx_auth(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct mlme_priv *pmlmepriv = &(adapt->mlmepriv);
	struct	sta_priv *pstapriv = &adapt->stapriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct macid_ctl_t *macid_ctl = dvobj_to_macidctl(dvobj);
	struct sta_info *psta;
	struct list_head	*plist, *phead;
	unsigned long	 irqL;
	char tmp[16];
	u8	mac_addr[NUM_STA][ETH_ALEN];
	u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u32 tx_auth;
	u8 index;


	if (count > 2) {
		RTW_INFO("argument size is more than 2\n");
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {
		int num = sscanf(tmp, "%x", &tx_auth);

		if (num !=  1) {
			RTW_INFO("invalid read_reg parameter!\n");
			return count;
		}
		RTW_INFO("1: setnd auth, 2: send assoc request. tx_auth=%d\n", tx_auth);
	}

	if ((check_fwstate(pmlmepriv, WIFI_STATION_STATE))
	    && (check_fwstate(pmlmepriv, _FW_LINKED))) {
		if (tx_auth == 1) {
			/* TX unicast auth to AP */
			issue_auth(adapt, NULL, 0);
		} else if (tx_auth == 2) {
			/* TX unicast auth to AP */
			issue_assocreq(adapt);
		}
	}

	return count;
}

int proc_get_tx_auth(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	RTW_PRINT_SEL(m, "%s\n", __func__);
	return 0;
}
#endif /* CONFIG_IEEE80211W */

int proc_get_ack_timeout(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	u8 ack_timeout_val;

	ack_timeout_val = rtw_read8(adapt, REG_ACKTO);

	RTW_PRINT_SEL(m, "Current ACK Timeout = %d us (0x%x).\n", ack_timeout_val, ack_timeout_val);

	return 0;
}

ssize_t proc_set_ack_timeout(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 ack_timeout_ms, ack_timeout_ms_cck;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%u %u", &ack_timeout_ms, &ack_timeout_ms_cck);

		if (num < 1) {
			RTW_INFO(FUNC_ADPT_FMT ": input parameters < 1\n", FUNC_ADPT_ARG(adapt));
			return -EINVAL;
		}
		/* This register sets the Ack time out value after Tx unicast packet. It is in units of us. */
		rtw_write8(adapt, REG_ACKTO, (u8)ack_timeout_ms);

		RTW_INFO("Set ACK Timeout to %d us.\n", ack_timeout_ms);
	}

	return count;
}

ssize_t proc_set_fw_offload(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct adapter *pri_adapter = GET_PRIMARY_ADAPTER(adapter);
	struct hal_com_data *hal = GET_HAL_DATA(adapter);
	char tmp[32];
	u32 iqk_offload_enable = 0, ch_switch_offload_enable = 0;

	if (!buffer) {
		RTW_INFO("input buffer is NULL!\n");
		return -EFAULT;
	}

	if (count < 1) {
		RTW_INFO("input length is 0!\n");
		return -EFAULT;
	}

	if (count > sizeof(tmp)) {
		RTW_INFO("input length is too large\n");
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%d %d", &iqk_offload_enable, &ch_switch_offload_enable);

		if (num < 2) {
			RTW_INFO("input parameters < 1\n");
			return -EINVAL;
		}

		if (hal->RegIQKFWOffload != iqk_offload_enable) {
			hal->RegIQKFWOffload = iqk_offload_enable;
			rtw_hal_update_iqk_fw_offload_cap(pri_adapter);
		}

		if (hal->ch_switch_offload != ch_switch_offload_enable)
			hal->ch_switch_offload = ch_switch_offload_enable;
	}

	return count;
}

int proc_get_fw_offload(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(dev);
	struct hal_com_data *hal = GET_HAL_DATA(adapter);


	RTW_PRINT_SEL(m, "IQK FW offload:%s\n", hal->RegIQKFWOffload?"enable":"disable");
	RTW_PRINT_SEL(m, "Channel switch FW offload:%s\n", hal->ch_switch_offload?"enable":"disable");
	return 0;
}

#ifdef CONFIG_DBG_RF_CAL
int proc_get_iqk_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	return 0;
}

ssize_t proc_set_iqk(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 recovery, clear, segment;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%d %d %d", &recovery, &clear, &segment);

		rtw_hal_iqk_test(adapt, recovery, clear, segment);
	}

	return count;

}

int proc_get_lck_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);

	return 0;
}

ssize_t proc_set_lck(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 trigger;

	if (count < 1)
		return -EFAULT;

	if (count > sizeof(tmp)) {
		rtw_warn_on(1);
		return -EFAULT;
	}

	if (buffer && !copy_from_user(tmp, buffer, count)) {
		int num = sscanf(tmp, "%d", &trigger);

		rtw_hal_lck_test(adapt);
	}

	return count;
}
#endif /* CONFIG_DBG_RF_CAL */

#endif /* CONFIG_PROC_DEBUG */
#define RTW_BUFDUMP_BSIZE		16
inline void RTW_BUF_DUMP_SEL(uint _loglevel, void *sel, u8 *_titlestring,
					bool _idx_show, const u8 *_hexdata, int _hexdatalen)
{
	int __i;
	u8 *ptr = (u8 *)_hexdata;

	if (_loglevel <= rtw_drv_log_level) {
		if (_titlestring) {
			if (sel == RTW_DBGDUMP)
				RTW_PRINT("");
			_RTW_PRINT_SEL(sel, "%s", _titlestring);
			if (_hexdatalen >= RTW_BUFDUMP_BSIZE)
				_RTW_PRINT_SEL(sel, "\n");
		}

		for (__i = 0; __i < _hexdatalen; __i++) {
			if (((__i % RTW_BUFDUMP_BSIZE) == 0) && (_hexdatalen >= RTW_BUFDUMP_BSIZE)) {
				if (sel == RTW_DBGDUMP)
					RTW_PRINT("");
				if (_idx_show)
					_RTW_PRINT_SEL(sel, "0x%03X: ", __i);
			}
			_RTW_PRINT_SEL(sel, "%02X%s", ptr[__i], (((__i + 1) % 4) == 0) ? "  " : " ");
			if ((__i + 1 < _hexdatalen) && ((__i + 1) % RTW_BUFDUMP_BSIZE) == 0)
				_RTW_PRINT_SEL(sel, "\n");
		}
		_RTW_PRINT_SEL(sel, "\n");
	}
}
