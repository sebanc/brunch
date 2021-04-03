// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

/* ************************************************************
 * include files
 * ************************************************************ */

#include "mp_precomp.h"
#include "phydm_precomp.h"

/* ODM IO Relative API. */

u8 odm_read_1byte(struct PHY_DM_STRUCT *p_dm, u32 reg_addr)
{
	struct adapter		*adapter = p_dm->adapter;

	return rtw_read8(adapter, reg_addr);
}

u16 odm_read_2byte(struct PHY_DM_STRUCT *p_dm, u32 reg_addr)
{
	struct adapter		*adapter = p_dm->adapter;
	return rtw_read16(adapter, reg_addr);
}


u32
odm_read_4byte(
	struct PHY_DM_STRUCT		*p_dm,
	u32			reg_addr
)
{
	struct adapter		*adapter = p_dm->adapter;
	return rtw_read32(adapter, reg_addr);
}

void
odm_write_1byte(
	struct PHY_DM_STRUCT		*p_dm,
	u32			reg_addr,
	u8			data
)
{
	struct adapter		*adapter = p_dm->adapter;
	rtw_write8(adapter, reg_addr, data);
}


void
odm_write_2byte(
	struct PHY_DM_STRUCT		*p_dm,
	u32			reg_addr,
	u16			data
)
{
	struct adapter		*adapter = p_dm->adapter;
	rtw_write16(adapter, reg_addr, data);
}

void
odm_write_4byte(
	struct PHY_DM_STRUCT		*p_dm,
	u32			reg_addr,
	u32			data
)
{
	struct adapter		*adapter = p_dm->adapter;
	rtw_write32(adapter, reg_addr, data);
}

void
odm_set_mac_reg(
	struct PHY_DM_STRUCT	*p_dm,
	u32		reg_addr,
	u32		bit_mask,
	u32		data
)
{
	phy_set_bb_reg(p_dm->adapter, reg_addr, bit_mask, data);
}

u32
odm_get_mac_reg(
	struct PHY_DM_STRUCT	*p_dm,
	u32		reg_addr,
	u32		bit_mask
)
{
	return phy_query_mac_reg(p_dm->adapter, reg_addr, bit_mask);
}


void
odm_set_bb_reg(
	struct PHY_DM_STRUCT	*p_dm,
	u32		reg_addr,
	u32		bit_mask,
	u32		data
)
{
	phy_set_bb_reg(p_dm->adapter, reg_addr, bit_mask, data);
}


u32
odm_get_bb_reg(
	struct PHY_DM_STRUCT	*p_dm,
	u32		reg_addr,
	u32		bit_mask
)
{
	return phy_query_bb_reg(p_dm->adapter, reg_addr, bit_mask);
}


void
odm_set_rf_reg(
	struct PHY_DM_STRUCT			*p_dm,
	u8			e_rf_path,
	u32				reg_addr,
	u32				bit_mask,
	u32				data
)
{
	phy_set_rf_reg(p_dm->adapter, e_rf_path, reg_addr, bit_mask, data);
}

u32
odm_get_rf_reg(
	struct PHY_DM_STRUCT			*p_dm,
	u8			e_rf_path,
	u32				reg_addr,
	u32				bit_mask
)
{
	return phy_query_rf_reg(p_dm->adapter, e_rf_path, reg_addr, bit_mask);
}

enum hal_status
phydm_set_reg_by_fw(
	struct PHY_DM_STRUCT			*p_dm,
	enum phydm_halmac_param	config_type,
	u32	offset,
	u32	data,
	u32	mask,
	enum rf_path	e_rf_path,
	u32 delay_time
)
{
	return rtw_phydm_cfg_phy_para(p_dm,
							config_type,
							offset,
							data,
							mask,
							e_rf_path,
							delay_time);
}

/*
 * ODM Memory relative API.
 *   */
void
odm_allocate_memory(
	struct PHY_DM_STRUCT	*p_dm,
	void **p_ptr,
	u32		length
)
{
	*p_ptr = vzalloc(length);
}

/* length could be ignored, used to detect memory leakage. */
void
odm_free_memory(
	struct PHY_DM_STRUCT	*p_dm,
	void		*p_ptr,
	u32		length
)
{
	vfree(p_ptr);
}

void
odm_move_memory(
	struct PHY_DM_STRUCT	*p_dm,
	void		*p_dest,
	void		*p_src,
	u32		length
)
{
	memcpy(p_dest, p_src, length);
}

void odm_memory_set(
	struct PHY_DM_STRUCT	*p_dm,
	void		*pbuf,
	s8		value,
	u32		length
)
{
	memset(pbuf, value, length);
}
s32 odm_compare_memory(
	struct PHY_DM_STRUCT		*p_dm,
	void           *p_buf1,
	void           *p_buf2,
	u32          length
)
{
	return !memcmp(p_buf1, p_buf2, length);
}



/*
 * ODM MISC relative API.
 *   */
void
odm_acquire_spin_lock(
	struct PHY_DM_STRUCT			*p_dm,
	enum rt_spinlock_type	type
)
{
	struct adapter *adapter = p_dm->adapter;

	rtw_odm_acquirespinlock(adapter, type);
}
void
odm_release_spin_lock(
	struct PHY_DM_STRUCT			*p_dm,
	enum rt_spinlock_type	type
)
{
	struct adapter *adapter = p_dm->adapter;

	rtw_odm_releasespinlock(adapter, type);
}

/*
 * ODM Timer relative API.
 *   */

void
ODM_delay_ms(u32	ms)
{
	rtw_mdelay_os(ms);
}

void
ODM_delay_us(u32	us)
{
	rtw_udelay_os(us);
}

void
ODM_sleep_ms(u32	ms)
{
	rtw_msleep_os(ms);
}

void
ODM_sleep_us(u32	us)
{
	rtw_usleep_os(us);
}

void
odm_set_timer(
	struct PHY_DM_STRUCT		*p_dm,
	struct timer_list		*p_timer,
	u32			ms_delay
)
{
	_set_timer(p_timer, ms_delay); /* ms */

}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void
odm_initialize_timer(
	struct PHY_DM_STRUCT			*p_dm,
	struct timer_list			*p_timer,
	void	*call_back_func,
	void				*p_context,
	const char			*sz_id
)
{
	struct adapter *adapter = p_dm->adapter;

	_init_timer(p_timer, adapter->pnetdev, call_back_func, p_dm);
}
#endif

void
odm_cancel_timer(
	struct PHY_DM_STRUCT		*p_dm,
	struct timer_list		*p_timer
)
{
	_cancel_timer_ex(p_timer);
}


void
odm_release_timer(
	struct PHY_DM_STRUCT		*p_dm,
	struct timer_list		*p_timer
)
{
}

static u8
phydm_trans_h2c_id(
	struct PHY_DM_STRUCT	*p_dm,
	u8		phydm_h2c_id
)
{
	u8 platform_h2c_id = phydm_h2c_id;

	switch (phydm_h2c_id) {
	/* 1 [0] */
	case ODM_H2C_RSSI_REPORT:

		platform_h2c_id = H2C_RSSI_SETTING;

		break;

	/* 1 [3] */
	case ODM_H2C_WIFI_CALIBRATION:
		break;
	/* 1 [4] */
	case ODM_H2C_IQ_CALIBRATION:
		break;
	/* 1 [5] */
	case ODM_H2C_RA_PARA_ADJUST:
		break;
	/* 1 [6] */
	case PHYDM_H2C_DYNAMIC_TX_PATH:
		break;
	/* [7]*/
	case PHYDM_H2C_FW_TRACE_EN:
		platform_h2c_id = 0x49;
		break;
	case PHYDM_H2C_TXBF:
		break;
	case PHYDM_H2C_MU:
		break;
	default:
		platform_h2c_id = phydm_h2c_id;
		break;
	}

	return platform_h2c_id;

}

/*ODM FW relative API.*/

void
odm_fill_h2c_cmd(
	struct PHY_DM_STRUCT		*p_dm,
	u8			phydm_h2c_id,
	u32			cmd_len,
	u8			*p_cmd_buffer
)
{
	struct adapter	*adapter = p_dm->adapter;
	u8		h2c_id = phydm_trans_h2c_id(p_dm, phydm_h2c_id);

	PHYDM_DBG(p_dm, DBG_RA, ("[H2C]  h2c_id=((0x%x))\n", h2c_id));

	rtw_hal_fill_h2c_cmd(adapter, h2c_id, cmd_len, p_cmd_buffer);
}

u8
phydm_c2H_content_parsing(
	void			*p_dm_void,
	u8			c2h_cmd_id,
	u8			c2h_cmd_len,
	u8			*tmp_buf
)
{
	struct PHY_DM_STRUCT		*p_dm = (struct PHY_DM_STRUCT *)p_dm_void;
	u8	extend_c2h_sub_id = 0;
	u8	find_c2h_cmd = true;
	
	if ((c2h_cmd_len > 12) || (c2h_cmd_len == 0)) {
		dbg_print("[Warning] Error C2H ID=%d, len=%d\n", c2h_cmd_id, c2h_cmd_len);
		
		find_c2h_cmd = false;
		return find_c2h_cmd;
	}
	
	switch (c2h_cmd_id) {
	case PHYDM_C2H_DBG:
		phydm_fw_trace_handler(p_dm, tmp_buf, c2h_cmd_len);
		break;
	case PHYDM_C2H_RA_RPT:
		phydm_c2h_ra_report_handler(p_dm, tmp_buf, c2h_cmd_len);
		break;
	case PHYDM_C2H_RA_PARA_RPT:
		odm_c2h_ra_para_report_handler(p_dm, tmp_buf, c2h_cmd_len);
		break;
	case PHYDM_C2H_DYNAMIC_TX_PATH_RPT:
		if (p_dm->support_ic_type & (ODM_RTL8814A))
			phydm_c2h_dtp_handler(p_dm, tmp_buf, c2h_cmd_len);
		break;
	case PHYDM_C2H_IQK_FINISH:
		break;
	case PHYDM_C2H_CLM_MONITOR:
		phydm_c2h_clm_report_handler(p_dm, tmp_buf, c2h_cmd_len);
		break;
	case PHYDM_C2H_DBG_CODE:
		phydm_fw_trace_handler_code(p_dm, tmp_buf, c2h_cmd_len);
		break;
	case PHYDM_C2H_EXTEND:
		extend_c2h_sub_id = tmp_buf[0];
		if (extend_c2h_sub_id == PHYDM_EXTEND_C2H_DBG_PRINT)
			phydm_fw_trace_handler_8051(p_dm, tmp_buf, c2h_cmd_len);
		break;
	default:
		find_c2h_cmd = false;
		break;
	}

	return find_c2h_cmd;

}

u64
odm_get_current_time(
	struct PHY_DM_STRUCT		*p_dm
)
{
	return rtw_get_current_time();
}

u64
odm_get_progressing_time(
	struct PHY_DM_STRUCT		*p_dm,
	u64			start_time
)
{
	return rtw_get_passing_time_ms((unsigned long)start_time);
}

void
phydm_set_hw_reg_handler_interface (
	struct PHY_DM_STRUCT		*p_dm,
	u8				RegName,
	u8				*val
	)
{
	struct adapter *adapter = p_dm->adapter;

	adapter->hal_func.set_hw_reg_handler(adapter, RegName, val);
}

void
phydm_get_hal_def_var_handler_interface (
	struct PHY_DM_STRUCT		*p_dm,
	enum hal_def_variable		e_variable,
	void						*p_value
	)
{
	struct adapter *adapter = p_dm->adapter;

	adapter->hal_func.get_hal_def_var_handler(adapter, e_variable, p_value);
}

void
odm_set_tx_power_index_by_rate_section (
	struct PHY_DM_STRUCT	*p_dm,
	enum rf_path		path,
	u8				Channel,
	u8				RateSection
	)
{
	phy_set_tx_power_index_by_rate_section(p_dm->adapter, path, Channel, RateSection);
}


u8
odm_get_tx_power_index (
	struct PHY_DM_STRUCT	*p_dm,
	enum rf_path		path,
	u8				tx_rate,
	u8				band_width,
	u8				Channel
	)
{
	return phy_get_tx_power_index(p_dm->adapter, path, tx_rate, band_width, Channel);
}

u8
odm_efuse_one_byte_read(
	struct PHY_DM_STRUCT	*p_dm,
	u16			addr,
	u8			*data,
	bool		b_pseu_do_test
	)
{
	return efuse_onebyte_read(p_dm->adapter, addr, data, b_pseu_do_test);
}

void
odm_efuse_logical_map_read(
	struct	PHY_DM_STRUCT	*p_dm,
	u8	type,
	u16	offset,
	u32	*data
)
{
	efuse_logical_map_read(p_dm->adapter, type, offset, data);
}

enum hal_status
odm_iq_calibrate_by_fw(
	struct PHY_DM_STRUCT	*p_dm,
	u8 clear,
	u8 segment
	)
{
	enum hal_status iqk_result = HAL_STATUS_FAILURE;

	iqk_result = rtw_phydm_fw_iqk(p_dm, clear, segment);
	return iqk_result;
}

void
odm_cmn_info_ptr_array_hook(
	struct PHY_DM_STRUCT		*p_dm,
	enum odm_cmninfo_e	cmn_info,
	u16			index,
	void			*p_value
)
{
	switch	(cmn_info) {
	/*Dynamic call by reference pointer.	*/
	case	ODM_CMNINFO_STA_STATUS:
		p_dm->p_odm_sta_info[index] = (struct sta_info *)p_value;
		break;
	/* To remove the compiler warning, must add an empty default statement to handle the other values. */
	default:
		/* do nothing */
		break;
	}

}

void
phydm_cmn_sta_info_hook(
	struct PHY_DM_STRUCT		*p_dm,
	u8			mac_id,
	struct cmn_sta_info *pcmn_sta_info
)
{
	p_dm->p_phydm_sta_info[mac_id] = pcmn_sta_info;

	if (is_sta_active(pcmn_sta_info))
		p_dm->phydm_macid_table[pcmn_sta_info->mac_id] = mac_id;
}

void
phydm_add_interrupt_mask_handler(
	struct PHY_DM_STRUCT		*p_dm,
	u8							interrupt_type
)
{
}

void
phydm_enable_rx_related_interrupt_handler(
	struct PHY_DM_STRUCT		*p_dm
)
{
}

bool
phydm_get_txbf_en(
	struct PHY_DM_STRUCT		*p_dm,
	u16							mac_id,
	u8							i
)
{
	bool txbf_en = false;

	return txbf_en;
}

void
phydm_iqk_wait(
	struct PHY_DM_STRUCT		*p_dm,
	u32		timeout
)
{
	struct adapter *adapter = p_dm->adapter;

	rtl8812_iqk_wait(adapter, timeout);
}
