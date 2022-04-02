
#include "mp_precomp.h"
#include "../phydm_precomp.h"

#if (RTL8821C_SUPPORT == 1)


/*---------------------------Define Local Constant---------------------------*/

static u32	dpk_result[DPK_BACKUP_REG_NUM_8821C] ;
#define enable_8821c_dpk 1
#define dpk_forcein_sram4 0

/*---------------------------Define Local Constant---------------------------*/


#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
void do_iqk_8821c(
	void		*p_dm_void,
	u8		delta_thermal_index,
	u8		thermal_value,
	u8		threshold
)
{
	struct PHY_DM_STRUCT		*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;

	struct _ADAPTER		*adapter = p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(adapter);

	odm_reset_iqk_result(p_dm_odm);
	p_dm_odm->rf_calibrate_info.thermal_value_iqk = thermal_value;
	phy_iq_calibrate_8821c(p_dm_odm, true);
}
#else
/*Originally p_config->do_iqk is hooked phy_iq_calibrate_8821c, but do_iqk_8821c and phy_iq_calibrate_8821c have different arguments*/
void do_iqk_8821c(
	void		*p_dm_void,
	u8	delta_thermal_index,
	u8	thermal_value,
	u8	threshold
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	boolean		is_recovery = (boolean) delta_thermal_index;

	phy_iq_calibrate_8821c(p_dm_odm, true);
}
#endif
void do_dpk_8821c(
	void		*p_dm_void,
	u8	delta_thermal_index,
	u8	thermal_value,
	u8	threshold
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	boolean		is_recovery = (boolean) delta_thermal_index;
	phy_dp_calibrate_8821c(p_dm_odm, true);
}



void
_iqk_check_coex_status(
	struct PHY_DM_STRUCT	*p_dm_odm,
	boolean		beforeK
)
{
	u8		u1b_tmp;
	u16		count = 0;
	u8		h2c_parameter;

	h2c_parameter = 1;

	if (beforeK) {
		u1b_tmp = odm_read_1byte(p_dm_odm, 0x49c);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]check 0x49c[0] = 0x%x before h2c 0x6d\n", u1b_tmp));
		RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]check 0x49c[0] = 0x%x before h2c 0x6d\n", u1b_tmp));

		/*check if BT IQK */
		u1b_tmp = odm_read_1byte(p_dm_odm, 0x49c);
		while ((u1b_tmp & BIT(1)) && (count < 100)) {
			ODM_delay_ms(10);
			u1b_tmp = odm_read_1byte(p_dm_odm, 0x49c);
			count++;
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]check 0x49c[1]=0x%x, count = %d\n", u1b_tmp, count));
			RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]check 0x49c[1]=0x%x, count = %d\n", u1b_tmp, count));
		}
#if 1
		odm_fill_h2c_cmd(p_dm_odm, ODM_H2C_WIFI_CALIBRATION, 1, &h2c_parameter);

		u1b_tmp = odm_read_1byte(p_dm_odm, 0x49c);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]check 0x49c[0] = 0x%x after h2c 0x6d\n", u1b_tmp));
		RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]check 0x49c[0] = 0x%x after h2c 0x6d\n", u1b_tmp));

		u1b_tmp = odm_read_1byte(p_dm_odm, 0x49c);
		/*check if WL IQK available form WL FW */
		while ((!(u1b_tmp & BIT(0))) && (count < 100)) {
			ODM_delay_ms(10);
			u1b_tmp = odm_read_1byte(p_dm_odm, 0x49c);
			count++;
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]check 0x49c[1]=0x%x, count = %d\n", u1b_tmp, count));
			RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]check 0x49c[1]=0x%x, count = %d\n", u1b_tmp, count));
		}

		if (count >= 100)
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]Polling 0x49c to 1 for WiFi calibration H2C cmd FAIL! count(%d)\n", count));
#endif

	} else
		odm_set_bb_reg(p_dm_odm, 0x49c, BIT(0), 0x0);
}


u32
_iqk_indirect_read_reg(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u16 reg_addr
)
{
	u32 j = 0;

	/*wait for ready bit before access 0x1700*/
	odm_write_4byte(p_dm_odm, 0x1700, 0x800f0000 | reg_addr);

	do {
		j++;
	} while (((odm_read_1byte(p_dm_odm, 0x1703) & BIT(5)) == 0) && (j < 30000));

	return odm_read_4byte(p_dm_odm, 0x1708);  /*get read data*/

}


void
_iqk_indirect_write_reg(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u16 reg_addr,
	u32 bit_mask,
	u32 reg_value
)
{
	u32 val, i = 0, j = 0, bitpos = 0;

	if (bit_mask == 0x0)
		return;
	if (bit_mask == 0xffffffff) {
		odm_write_4byte(p_dm_odm, 0x1704, reg_value); /*put write data*/

		/*wait for ready bit before access 0x1700*/
		do {
			j++;
		} while (((odm_read_1byte(p_dm_odm, 0x1703) & BIT(5)) == 0) && (j < 30000));

		odm_write_4byte(p_dm_odm, 0x1700, 0xc00f0000 | reg_addr);
	} else {
		for (i = 0; i <= 31; i++) {
			if (((bit_mask >> i) & 0x1) == 0x1) {
				bitpos = i;
				break;
			}
		}

		/*read back register value before write*/
		val = _iqk_indirect_read_reg(p_dm_odm, reg_addr);
		val = (val & (~bit_mask)) | (reg_value << bitpos);

		odm_write_4byte(p_dm_odm, 0x1704, val); /*put write data*/

		/*wait for ready bit before access 0x1700*/
		do {
			j++;
		} while (((odm_read_1byte(p_dm_odm, 0x1703) & BIT(5)) == 0) && (j < 30000));

		odm_write_4byte(p_dm_odm, 0x1700, 0xc00f0000 | reg_addr);
	}
}


void
_iqk_set_gnt_wl_high(
	struct PHY_DM_STRUCT	*p_dm_odm
)
{
	u32 val = 0;
	u8 state = 0x1, sw_control = 0x1;

	/*GNT_WL = 1*/
	val = (sw_control) ? ((state << 1) | 0x1) : 0;
	_iqk_indirect_write_reg(p_dm_odm, 0x38, 0x3000, val); /*0x38[13:12]*/
	_iqk_indirect_write_reg(p_dm_odm, 0x38, 0x0300, val); /*0x38[9:8]*/
}

void _iqk_set_gnt_bt_low(
	struct PHY_DM_STRUCT	*p_dm_odm
)
{
	u32 val = 0;
	u8 state = 0x0, sw_control = 0x1;

	/*GNT_BT = 0*/
	val = (sw_control) ? ((state << 1) | 0x1) : 0;
	_iqk_indirect_write_reg(p_dm_odm, 0x38, 0xc000, val); /*0x38[15:14]*/
	_iqk_indirect_write_reg(p_dm_odm, 0x38, 0x0c00, val); /*0x38[11:10]*/
}

void _iqk_set_gnt_wl_gnt_bt(
	struct PHY_DM_STRUCT	*p_dm_odm,
	boolean beforeK
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	if (beforeK) {
		_iqk_set_gnt_wl_high(p_dm_odm);
		_iqk_set_gnt_bt_low(p_dm_odm);
	} else
		_iqk_indirect_write_reg(p_dm_odm, 0x38, MASKDWORD, p_iqk_info->tmp_GNTWL);
}


void
_iqk_fill_iqk_report_8821c(
	void		*p_dm_void,
	u8			channel
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	u32		tmp1 = 0x0, tmp2 = 0x0, tmp3 = 0x0;
	u8		i;

	for (i = 0; i < SS_8821C; i++) {
		tmp1 = tmp1 + ((p_iqk_info->IQK_fail_report[channel][i][TX_IQK] & 0x1) << i);
		tmp2 = tmp2 + ((p_iqk_info->IQK_fail_report[channel][i][RX_IQK] & 0x1) << (i + 4));
		tmp3 = tmp3 + ((p_iqk_info->RXIQK_fail_code[channel][i] & 0x3) << (i * 2 + 8));
	}
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008);
	odm_set_bb_reg(p_dm_odm, 0x1bf0, 0x00ffffff, tmp1 | tmp2 | tmp3);

	for (i = 0; i < SS_8821C; i++)
		odm_write_4byte(p_dm_odm, 0x1be8 + (i * 4), (p_iqk_info->RXIQK_AGC[channel][(i * 2) + 1] << 16) | p_iqk_info->RXIQK_AGC[channel][i * 2]);
}


void
_iqk_iqk_fail_report_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm
)
{
	u32		tmp1bf0 = 0x0;
	u8		i;

	tmp1bf0 = odm_read_4byte(p_dm_odm, 0x1bf0);

	for (i = 0; i < 4; i++) {
		if (tmp1bf0 & (0x1 << i))
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK] please check S%d TXIQK\n", i));
#else
			panic_printk("[IQK] please check S%d TXIQK\n", i);
#endif
		if (tmp1bf0 & (0x1 << (i + 12)))
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK] please check S%d RXIQK\n", i));
#else
			panic_printk("[IQK] please check S%d RXIQK\n", i);
#endif

	}
}


void
_iqk_backup_mac_bb_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u32		*MAC_backup,
	u32		*BB_backup,
	u32		*backup_mac_reg,
	u32		*backup_bb_reg,
	u8		num_backup_bb_reg
)
{
	u32 i;

	for (i = 0; i < MAC_REG_NUM_8821C; i++)
		MAC_backup[i] = odm_read_4byte(p_dm_odm, backup_mac_reg[i]);

	for (i = 0; i < num_backup_bb_reg; i++)
		BB_backup[i] = odm_read_4byte(p_dm_odm, backup_bb_reg[i]);
	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]BackupMacBB Success!!!!\n")); */
}


void
_iqk_backup_rf_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u32		RF_backup[][SS_8821C],
	u32		*backup_rf_reg
)
{
	u32 i, j;

	for (i = 0; i < RF_REG_NUM_8821C; i++)
		for (j = 0; j < SS_8821C; j++)
			RF_backup[i][j] = odm_get_rf_reg(p_dm_odm, j, backup_rf_reg[i], RFREGOFFSETMASK);
	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]BackupRF Success!!!!\n")); */

}

void
_iqk_agc_bnd_int_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm
)
{
	/*initialize RX AGC bnd, it must do after bbreset*/
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf80a7008);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8015008);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008);
	/*ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]init. rx agc bnd\n"));*/
}


void
_iqk_bb_reset_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm
)
{
	boolean		cca_ing = false;
	u32		count = 0;

	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x0, RFREGOFFSETMASK, 0x10000);

	while (1) {
		odm_write_4byte(p_dm_odm, 0x8fc, 0x0);
		odm_set_bb_reg(p_dm_odm, 0x198c, 0x7, 0x7);
		cca_ing = (boolean) odm_get_bb_reg(p_dm_odm, 0xfa0, BIT(3));

		if (count > 30)
			cca_ing = false;

		if (cca_ing) {
			ODM_delay_ms(1);
			count++;
		} else {
			odm_write_1byte(p_dm_odm, 0x808, 0x0);	/*RX ant off*/
			odm_set_bb_reg(p_dm_odm, 0xa04, BIT(27) | BIT26 | BIT25 | BIT24, 0x0);		/*CCK RX path off*/

			/*BBreset*/
			odm_set_bb_reg(p_dm_odm, 0x0, BIT(16), 0x0);
			odm_set_bb_reg(p_dm_odm, 0x0, BIT(16), 0x1);

			if (odm_get_bb_reg(p_dm_odm, 0x660, BIT(16)))
				odm_write_4byte(p_dm_odm, 0x6b4, 0x89000006);
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]BBreset!!!!\n"));
			break;
		}
	}
}

void
_iqk_afe_setting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	boolean		do_iqk
)
{
	if (do_iqk) {
		/*IQK AFE setting RX_WAIT_CCA mode */
		odm_write_4byte(p_dm_odm, 0xc60, 0x50000000);
		odm_write_4byte(p_dm_odm, 0xc60, 0x700F0040);


		/*AFE setting*/
		odm_write_4byte(p_dm_odm, 0xc58, 0xd8000402);
		odm_write_4byte(p_dm_odm, 0xc5c, 0xd1000120);
		odm_write_4byte(p_dm_odm, 0xc6c, 0x00000a15);
		_iqk_bb_reset_8821c(p_dm_odm);
		/*		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]AFE setting for IQK mode!!!!\n")); */
	} else {
		/*IQK AFE setting RX_WAIT_CCA mode */
		odm_write_4byte(p_dm_odm, 0xc60, 0x50000000);
		odm_write_4byte(p_dm_odm, 0xc60, 0x700B8040);

		/*AFE setting*/
		odm_write_4byte(p_dm_odm, 0xc58, 0xd8020402);
		odm_write_4byte(p_dm_odm, 0xc5c, 0xde000120);
		odm_write_4byte(p_dm_odm, 0xc6c, 0x0000122a);
		/*		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]AFE setting for Normal mode!!!!\n")); */
	}
}

void
_iqk_restore_mac_bb_8821c(
	struct PHY_DM_STRUCT		*p_dm_odm,
	u32		*MAC_backup,
	u32		*BB_backup,
	u32		*backup_mac_reg,
	u32		*backup_bb_reg,
	u8		num_backup_bb_reg
)
{
	u32 i;

	for (i = 0; i < MAC_REG_NUM_8821C; i++)
		odm_write_4byte(p_dm_odm, backup_mac_reg[i], MAC_backup[i]);
	for (i = 0; i < num_backup_bb_reg; i++)
		odm_write_4byte(p_dm_odm, backup_bb_reg[i], BB_backup[i]);

	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]RestoreMacBB Success!!!!\n")); */
}

void
_iqk_restore_rf_8821c(
	struct PHY_DM_STRUCT			*p_dm_odm,
	u32			*backup_rf_reg,
	u32			RF_backup[][SS_8821C]
)
{
	u32 i;

	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xef, RFREGOFFSETMASK, 0x0);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xee, RFREGOFFSETMASK, 0x0);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xdf, RFREGOFFSETMASK, RF_backup[0][ODM_RF_PATH_A] & (~BIT(4)));
	/*odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xde, RFREGOFFSETMASK, RF_backup[1][ODM_RF_PATH_A]|BIT4);*/
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xde, RFREGOFFSETMASK, RF_backup[1][ODM_RF_PATH_A] & (~BIT(4)));

	for (i = 2; i < (RF_REG_NUM_8821C-1); i++)
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, backup_rf_reg[i], RFREGOFFSETMASK, RF_backup[i][ODM_RF_PATH_A]);

	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x1, RFREGOFFSETMASK, (RF_backup[5][ODM_RF_PATH_A] & (~BIT(0))));

	/*ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]RestoreRF Success!!!!\n")); */

}


void
_iqk_backup_iqk_8821c(
	struct PHY_DM_STRUCT			*p_dm_odm,
	u8				step
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	u8		i, j, k, path, idx;
	u32		tmp;
	u16		iqk_apply[2] = {0xc94, 0xe94};

	if (step == 0x0) {
		p_iqk_info->iqk_channel[1] = p_iqk_info->iqk_channel[0];
		for (i = 0; i < SS_8821C; i++) {
			p_iqk_info->LOK_IDAC[1][i] = p_iqk_info->LOK_IDAC[0][i];
			p_iqk_info->RXIQK_AGC[1][i] = p_iqk_info->RXIQK_AGC[0][i];
			p_iqk_info->bypass_iqk[1][i] = p_iqk_info->bypass_iqk[0][i];
			p_iqk_info->RXIQK_fail_code[1][i] = p_iqk_info->RXIQK_fail_code[0][i];
			for (j = 0; j < 2; j++) {
				p_iqk_info->IQK_fail_report[1][i][j] = p_iqk_info->IQK_fail_report[0][i][j];
				for (k = 0; k < 8; k++) {
					p_iqk_info->IQK_CFIR_real[1][i][j][k] = p_iqk_info->IQK_CFIR_real[0][i][j][k];
					p_iqk_info->IQK_CFIR_imag[1][i][j][k] = p_iqk_info->IQK_CFIR_imag[0][i][j][k];
				}
			}
		}

		for (i = 0; i < 4; i++) {
			p_iqk_info->RXIQK_fail_code[0][i] = 0x0;
			p_iqk_info->RXIQK_AGC[0][i] = 0x0;
			for (j = 0; j < 2; j++) {
				p_iqk_info->IQK_fail_report[0][i][j] = true;
				p_iqk_info->gs_retry_count[0][i][j] = 0x0;
			}
			for (j = 0; j < 3; j++)
				p_iqk_info->retry_count[0][i][j] = 0x0;
		}
	} else {
		p_iqk_info->iqk_channel[0] = p_iqk_info->rf_reg18;
		for (path = 0; path < SS_8821C; path++) {
			p_iqk_info->LOK_IDAC[0][path] = odm_get_rf_reg(p_dm_odm, path, 0x58, RFREGOFFSETMASK);
			p_iqk_info->bypass_iqk[0][path] = odm_get_bb_reg(p_dm_odm, iqk_apply[path], MASKDWORD);

			for (idx = 0; idx < 2; idx++) {
				odm_set_bb_reg(p_dm_odm, 0x1b00, MASKDWORD, 0xf8000008 | path << 1);

				if (idx == 0)
					odm_set_bb_reg(p_dm_odm, 0x1b0c, BIT(13) | BIT(12), 0x3);
				else
					odm_set_bb_reg(p_dm_odm, 0x1b0c, BIT(13) | BIT(12), 0x1);

				odm_set_bb_reg(p_dm_odm, 0x1bd4, BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16), 0x10);

				for (i = 0; i < 8; i++) {
					odm_set_bb_reg(p_dm_odm, 0x1bd8, MASKDWORD, 0xe0000001 + (i * 4));
					tmp = odm_get_bb_reg(p_dm_odm, 0x1bfc, MASKDWORD);
					p_iqk_info->IQK_CFIR_real[0][path][idx][i] = (tmp & 0x0fff0000) >> 16;
					p_iqk_info->IQK_CFIR_imag[0][path][idx][i] = tmp & 0xfff;
				}
			}
			odm_set_bb_reg(p_dm_odm, 0x1bd8, MASKDWORD, 0x0);
			odm_set_bb_reg(p_dm_odm, 0x1b0c, BIT(13) | BIT(12), 0x0);
		}
	}
}


void
_iqk_reload_iqk_setting_8821c(
	struct PHY_DM_STRUCT			*p_dm_odm,
	u8				channel,
	u8				reload_idx  /*1: reload TX, 2: reload LO, TX, RX*/
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	u8 i, path, idx;
	u16		iqk_apply[2] = {0xc94, 0xe94};

	for (path = 0; path < SS_8821C; path++) {
#if 0
		if (reload_idx == 2) {
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xdf, BIT(4), 0x1);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x58, RFREGOFFSETMASK, p_iqk_info->LOK_IDAC[channel][path]);
		}
#endif
		for (idx = 0; idx < reload_idx; idx++) {
			odm_set_bb_reg(p_dm_odm, 0x1b00, MASKDWORD, 0xf8000008 | path << 1);
			odm_set_bb_reg(p_dm_odm, 0x1b2c, MASKDWORD, 0x7);
			odm_set_bb_reg(p_dm_odm, 0x1b38, MASKDWORD, 0x20000000);
			odm_set_bb_reg(p_dm_odm, 0x1b3c, MASKDWORD, 0x20000000);
			odm_set_bb_reg(p_dm_odm, 0x1bcc, MASKDWORD, 0x00000000);

			if (idx == 0)
				odm_set_bb_reg(p_dm_odm, 0x1b0c, BIT(13) | BIT(12), 0x3);
			else
				odm_set_bb_reg(p_dm_odm, 0x1b0c, BIT(13) | BIT(12), 0x1);

			odm_set_bb_reg(p_dm_odm, 0x1bd4, BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16), 0x10);

			for (i = 0; i < 8; i++) {
				odm_write_4byte(p_dm_odm, 0x1bd8,	((0xc0000000 >> idx) + 0x3) + (i * 4) + (p_iqk_info->IQK_CFIR_real[channel][path][idx][i] << 9));
				odm_write_4byte(p_dm_odm, 0x1bd8, ((0xc0000000 >> idx) + 0x1) + (i * 4) + (p_iqk_info->IQK_CFIR_imag[channel][path][idx][i] << 9));
			}
		}
		odm_set_bb_reg(p_dm_odm, iqk_apply[path], MASKDWORD, p_iqk_info->bypass_iqk[channel][path]);

		odm_set_bb_reg(p_dm_odm, 0x1bd8, MASKDWORD, 0x0);
		odm_set_bb_reg(p_dm_odm, 0x1b0c, BIT(13) | BIT(12), 0x0);
	}
}


boolean
_iqk_reload_iqk_8821c(
	struct PHY_DM_STRUCT			*p_dm_odm,
	boolean			reset
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	u8 i;
	boolean reload = false;

	if (reset) {
		for (i = 0; i < 2; i++)
			p_iqk_info->iqk_channel[i] = 0x0;
	} else {
		p_iqk_info->rf_reg18 = odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x18, RFREGOFFSETMASK);

		for (i = 0; i < 2; i++) {
			if (p_iqk_info->rf_reg18 == p_iqk_info->iqk_channel[i]) {
				_iqk_reload_iqk_setting_8821c(p_dm_odm, i, 2);
				_iqk_fill_iqk_report_8821c(p_dm_odm, i);
				ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]reload IQK result before!!!!\n"));
				reload = true;
			}
		}
	}
	return reload;
}


void
_iqk_rfe_setting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	boolean		ext_pa_on
)
{
	if (ext_pa_on) {
		/*RFE setting*/
		odm_write_4byte(p_dm_odm, 0xcb0, 0x77777777);
		odm_write_4byte(p_dm_odm, 0xcb4, 0x00007777);
		odm_write_4byte(p_dm_odm, 0xcbc, 0x0000083B);
		/*odm_write_4byte(p_dm_odm, 0x1990, 0x00000c30);*/
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]external PA on!!!!\n"));
	} else {
		/*RFE setting*/
		odm_write_4byte(p_dm_odm, 0xcb0, 0x77171117);
		odm_write_4byte(p_dm_odm, 0xcb4, 0x00001177);
		odm_write_4byte(p_dm_odm, 0xcbc, 0x00000404);
		/*odm_write_4byte(p_dm_odm, 0x1990, 0x00000c30);*/
		/*		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]external PA off!!!!\n"));*/
	}
}

void
_iqk_rfsetting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	u8 path;
	u32 tmp;

	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008);
	odm_write_4byte(p_dm_odm, 0x1bb8, 0x00000000);

	for (path = 0; path < SS_8821C; path++) {
		/*0xdf:B11 = 1,B4 = 0, B1 = 1*/
		tmp = odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xdf, RFREGOFFSETMASK);
		tmp = (tmp & (~BIT(4))) | BIT(1) | BIT(11);
		odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xdf, RFREGOFFSETMASK, tmp);

		if (p_iqk_info->is_BTG) {
			tmp = odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xde, RFREGOFFSETMASK);
			tmp = (tmp & (~BIT(4))) | BIT(15);
			/*tmp = tmp|BIT4|BIT15; //manual LOK value  for A-cut*/
			odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xde, RFREGOFFSETMASK, tmp);
		}

		if (!p_iqk_info->is_BTG) {
			/*WLAN_AG*/
			/*TX IQK	 mode init*/
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xef, RFREGOFFSETMASK, 0x80000);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x33, RFREGOFFSETMASK, 0x00024);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x3e, RFREGOFFSETMASK, 0x0003f);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x3f, RFREGOFFSETMASK, 0x60fde);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xef, RFREGOFFSETMASK, 0x00000);
			if (*p_dm_odm->p_band_type == ODM_BAND_5G) {
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xef, BIT(19), 0x1);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x33, RFREGOFFSETMASK, 0x00026);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x3e, RFREGOFFSETMASK, 0x00037);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x3f, RFREGOFFSETMASK, 0xdefce);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xef, BIT(19), 0x0);
			} else {
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xef, BIT(19), 0x1);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x33, RFREGOFFSETMASK, 0x00026);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x3e, RFREGOFFSETMASK, 0x00037);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x3f, RFREGOFFSETMASK, 0x5efce);
				odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xef, BIT(19), 0x0);
			}
		} else {
			/*WLAN_BTG*/
			/*TX IQK	 mode init*/
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xee, RFREGOFFSETMASK, 0x01000);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x33, RFREGOFFSETMASK, 0x00004);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x3f, RFREGOFFSETMASK, 0x01ec1);
			odm_set_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0xee, RFREGOFFSETMASK, 0x00000);
		}
	}
}

void
_iqk_configure_macbb_8821c(
	struct PHY_DM_STRUCT		*p_dm_odm
)
{
	/*MACBB register setting*/
	odm_write_1byte(p_dm_odm, 0x522, 0x7f);
	odm_set_bb_reg(p_dm_odm, 0x1518, BIT(16), 0x1);
	odm_set_bb_reg(p_dm_odm, 0x550, BIT(11) | BIT(3), 0x0);
	odm_set_bb_reg(p_dm_odm, 0x90c, BIT(15), 0x1);			/*0x90c[15]=1: dac_buf reset selection*/
	odm_set_bb_reg(p_dm_odm, 0x9a4, BIT(31), 0x0);         /*0x9a4[31]=0: Select da clock*/
	/*0xc94[0]=1, 0xe94[0]=1: 讓tx從iqk打出來*/
	odm_set_bb_reg(p_dm_odm, 0xc94, BIT(0), 0x1);
	/* 3-wire off*/
	odm_write_4byte(p_dm_odm, 0xc00, 0x00000004);
	/*disable PMAC*/
	odm_set_bb_reg(p_dm_odm, 0xb00, BIT(8), 0x0);
	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]Set MACBB setting for IQK!!!!\n"));*/

}


void
_iqk_lok_setting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path,
	u8          uPADindex
)
{
	u32 LOK0x56_2G = 0x50ef3;
	u32 LOK0x56_5G = 0x50ee8;
	u32 LOK0x33 = 0;
	u32 LOK0x78 = 0xbcbba;
	u32 tmp = 0;

	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	LOK0x33 = uPADindex;
/*add delay of MAC send packet*/
	if (p_dm_odm->mp_mode)
		odm_set_bb_reg(p_dm_odm, 0x810, BIT(7)|BIT(6)|BIT(5)|BIT(4), 0x8);

	if (p_iqk_info->is_BTG) {
		tmp = (LOK0x78 & 0x1c000) >> 14;
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		odm_write_4byte(p_dm_odm, 0x1bcc, 0x1b);
		odm_write_1byte(p_dm_odm, 0x1b23, 0x00);
		odm_write_1byte(p_dm_odm, 0x1b2b, 0x80);
		/*0x78[11:0] = IDAC value*/
		LOK0x78 = LOK0x78 & (0xe3fff | ((u32)uPADindex << 14));
		odm_set_rf_reg(p_dm_odm, path, 0x78, RFREGOFFSETMASK, LOK0x78);
		odm_set_rf_reg(p_dm_odm, path, 0x5c, RFREGOFFSETMASK, 0x05320);
		odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xac018);
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xee, BIT(4), 0x1);
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x33, BIT(3), 0x0);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK] In the BTG\n"));
	} else {
		/*tmp = (LOK0x56 & 0xe0) >> 5;*/
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		odm_write_4byte(p_dm_odm, 0x1bcc, 0x9);
		odm_write_1byte(p_dm_odm, 0x1b23, 0x00);

		switch (*p_dm_odm->p_band_type) {
		case ODM_BAND_2_4G:
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			LOK0x56_2G = LOK0x56_2G & (0xfff1f | ((u32)uPADindex << 5));
			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, LOK0x56_2G);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xadc18);
			odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xef, BIT(4), 0x1);
			odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x33, BIT(3), 0x0);
			break;
		case ODM_BAND_5G:
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			LOK0x56_5G = LOK0x56_5G & (0xfff1f | ((u32)uPADindex << 5));
			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, LOK0x56_5G);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xadc18);
			odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xef, BIT(4), 0x1);
			odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x33, BIT(3), 0x1);
			break;
		}
	}
	/*for IDAC LUT by PAD idx*/
	odm_set_rf_reg(p_dm_odm, path, 0x33, BIT(2) | BIT(1) | BIT(0), LOK0x33);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[IQK] LOK0x33 = 0x%x, LOK0x56_2G = 0x%x, LOK0x56_5G = 0x%x,LOK0x78 =0x%x\n",
		      LOK0x33, LOK0x56_2G, LOK0x56_5G, LOK0x78));
	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]Set LOK setting!!!!\n"));*/
}


void
_iqk_txk_setting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	if (p_iqk_info->is_BTG) {
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		odm_write_4byte(p_dm_odm, 0x1bcc, 0x1b);
		odm_write_4byte(p_dm_odm, 0x1b20, 0x00840008);

		/*0x78[11:0] = IDAC value*/
		odm_set_rf_reg(p_dm_odm, path, 0x78, RFREGOFFSETMASK, 0xbcbba);
		odm_set_rf_reg(p_dm_odm, path, 0x5c, RFREGOFFSETMASK, 0x04320);
		odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xac018);
		odm_write_1byte(p_dm_odm, 0x1b2b, 0x80);
	} else {
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		odm_write_4byte(p_dm_odm, 0x1bcc, 0x9);
		odm_write_4byte(p_dm_odm, 0x1b20, 0x01440008);

		switch (*p_dm_odm->p_band_type) {
		case ODM_BAND_2_4G:
			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, 0x50EF3);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xadc18);
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			break;
		case ODM_BAND_5G:
			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, 0x50EF0);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xa9c18);
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			break;
		}

	}
	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]Set TXK setting!!!!\n"));*/
}


void
_iqk_rxk1setting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	if (p_iqk_info->is_BTG) {
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		odm_write_1byte(p_dm_odm, 0x1b2b, 0x80);
		odm_write_4byte(p_dm_odm, 0x1bcc, 0x09);
		odm_write_4byte(p_dm_odm, 0x1b20, 0x01450008);
		odm_write_4byte(p_dm_odm, 0x1b24, 0x01460c88);

		/*0x78[11:0] = IDAC value*/
		odm_set_rf_reg(p_dm_odm, path, 0x78, RFREGOFFSETMASK, 0x8cbba);
		odm_set_rf_reg(p_dm_odm, path, 0x5c, RFREGOFFSETMASK, 0x00320);
		odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xa8018);
	} else {
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		switch (*p_dm_odm->p_band_type) {
		case ODM_BAND_2_4G:
			odm_write_1byte(p_dm_odm, 0x1bcc, 0x12);
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			odm_write_4byte(p_dm_odm, 0x1b20, 0x01450008);
			odm_write_4byte(p_dm_odm, 0x1b24, 0x01461068);

			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, 0x510f3);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xa9c00);
			break;
		case ODM_BAND_5G:
			odm_write_1byte(p_dm_odm, 0x1bcc, 0x9);
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			odm_write_4byte(p_dm_odm, 0x1b20, 0x00450008);
			odm_write_4byte(p_dm_odm, 0x1b24, 0x00461468);

			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, 0x510f3);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xa9c00);
			break;
		}
	}
	/*ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]Set RXK setting!!!!\n"));*/
}

static	u8	btg_lna[5] = {0x0, 0x4, 0x8, 0xc, 0xf};
static	u8	wlg_lna[5] = {0x0, 0x1, 0x2, 0x3, 0x5};
static	u8	wla_lna[5] = {0x0, 0x1, 0x3, 0x4, 0x5};

void
_iqk_rxk2setting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path,
	boolean is_gs
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	if (p_iqk_info->is_BTG) {
		if (is_gs) {
			p_iqk_info->tmp1bcc = 0x1b;
			p_iqk_info->lna_idx = 2;
		}
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		odm_write_1byte(p_dm_odm, 0x1b2b, 0x80);
		odm_write_4byte(p_dm_odm, 0x1bcc, p_iqk_info->tmp1bcc);
		odm_write_4byte(p_dm_odm, 0x1b20, 0x01450008);
		odm_write_4byte(p_dm_odm, 0x1b24, (0x01460048 | (btg_lna[p_iqk_info->lna_idx] << 10)));
		/*0x78[11:0] = IDAC value*/
		odm_set_rf_reg(p_dm_odm, path, 0x78, RFREGOFFSETMASK, 0x8cbba);
		odm_set_rf_reg(p_dm_odm, path, 0x5c, RFREGOFFSETMASK, 0x00320);
		odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xa8018);
	} else {
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		switch (*p_dm_odm->p_band_type) {
		case ODM_BAND_2_4G:
			if (is_gs) {
				p_iqk_info->tmp1bcc = 0x12;
				p_iqk_info->lna_idx = 2;
			}
			odm_write_1byte(p_dm_odm, 0x1bcc, p_iqk_info->tmp1bcc);
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			odm_write_4byte(p_dm_odm, 0x1b20, 0x01450008);
			odm_write_4byte(p_dm_odm, 0x1b24, (0x01460048 | (wlg_lna[p_iqk_info->lna_idx] << 10)));
			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, 0x510f3);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xa9c00);
			break;
		case ODM_BAND_5G:
			if (is_gs) {
				p_iqk_info->tmp1bcc = 0x12;
				p_iqk_info->lna_idx = 2;
			}
			odm_write_1byte(p_dm_odm, 0x1bcc, p_iqk_info->tmp1bcc);
			odm_write_1byte(p_dm_odm, 0x1b2b, 0x00);
			odm_write_4byte(p_dm_odm, 0x1b20, 0x00450008);
			odm_write_4byte(p_dm_odm, 0x1b24, (0x01460048 | (wla_lna[p_iqk_info->lna_idx] << 10)));
			odm_set_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK, 0x51000);
			odm_set_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK, 0xa9c00);
			break;
		}
	}
	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]Set RXK setting!!!!\n"));*/

}

boolean
_iqk_check_cal_8821c(
	struct PHY_DM_STRUCT			*p_dm_odm,
	u32				IQK_CMD
)
{
	boolean		notready = true, fail = true;
	u32		delay_count = 0x0;

	while (notready) {
		if (odm_read_4byte(p_dm_odm, 0x1b00) == (IQK_CMD & 0xffffff0f)) {
			fail = (boolean) odm_get_bb_reg(p_dm_odm, 0x1b08, BIT(26));
			notready = false;
		} else {
			ODM_delay_ms(1);
			delay_count++;
		}

		if (delay_count >= 50) {
			fail = true;
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				     ("[IQK]IQK timeout!!!\n"));
			break;
		}
	}
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
		     ("[IQK]delay count = 0x%x!!!\n", delay_count));
	return fail;
}


boolean
_iqk_rx_iqk_gain_search_fail_8821c(
	struct PHY_DM_STRUCT			*p_dm_odm,
	u8		path,
	u8		step
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	boolean		fail = true;
	u32	IQK_CMD = 0x0, rf_reg0, tmp, rxbb;
	u8	IQMUX[4] = {0x9, 0x12, 0x1b, 0x24}, *plna;
	u8	idx;
	u8	lna_setting[5];

	if (p_iqk_info->is_BTG)
		plna = btg_lna;
	else if (*p_dm_odm->p_band_type == ODM_BAND_2_4G)
		plna = wlg_lna;
	else
		plna = wla_lna;


	for (idx = 0; idx < 4; idx++)
		if (p_iqk_info->tmp1bcc == IQMUX[idx])
			break;

	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
	odm_write_4byte(p_dm_odm, 0x1bcc, p_iqk_info->tmp1bcc);

	if (step == RXIQK1)
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]============ S%d RXIQK GainSearch ============\n", p_iqk_info->is_BTG));

	if (step == RXIQK1)
		IQK_CMD = 0xf8000208 | (1 << (path + 4));
	else
		IQK_CMD = 0xf8000308 | (1 << (path + 4));

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]S%d GS%d_Trigger = 0x%x\n", path, step, IQK_CMD));

	_iqk_set_gnt_wl_gnt_bt(p_dm_odm, true);
	odm_write_4byte(p_dm_odm, 0x1b00, IQK_CMD);
	odm_write_4byte(p_dm_odm, 0x1b00, IQK_CMD + 0x1);
	ODM_delay_ms(GS_delay_8821C);
	fail = _iqk_check_cal_8821c(p_dm_odm, IQK_CMD);
	RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]check 0x49c = %x\n", odm_read_1byte(p_dm_odm, 0x49c)));
	_iqk_set_gnt_wl_gnt_bt(p_dm_odm, false);

	if (step == RXIQK2) {
		rf_reg0 = odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x0, RFREGOFFSETMASK);
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			("[IQK]S%d ==> RF0x0 = 0x%x, tmp1bcc = 0x%x, idx = %d, 0x1b3c = 0x%x\n", path, rf_reg0, p_iqk_info->tmp1bcc, idx, odm_read_4byte(p_dm_odm, 0x1b3c)));
		tmp = (rf_reg0 & 0x1fe0) >> 5;
		rxbb = tmp & 0x1f;
#if 1

		if (rxbb == 0x1) {
			if (idx != 3)
				idx++;
			else if (p_iqk_info->lna_idx != 0x0)
				p_iqk_info->lna_idx--;
			else
				p_iqk_info->isbnd = true;
			fail = true;
		} else if (rxbb == 0xa) {
			if (idx != 0)
				idx--;
			else if (p_iqk_info->lna_idx != 0x4)
				p_iqk_info->lna_idx++;
			else
				p_iqk_info->isbnd = true;
			fail = true;
		} else
			fail = false;

		if (p_iqk_info->isbnd == true)
			fail = false;
#endif

#if 0
		if (rxbb == 0x1) {
			if (p_iqk_info->lna_idx != 0x0)
				p_iqk_info->lna_idx--;
			else if (idx != 3)
				idx++;
			else
				p_iqk_info->isbnd = true;
			fail = true;
		} else if (rxbb == 0xa) {
			if (idx != 0)
				idx--;
			else if (p_iqk_info->lna_idx != 0x7)
				p_iqk_info->lna_idx++;
			else
				p_iqk_info->isbnd = true;
			fail = true;
		} else
			fail = false;

		if (p_iqk_info->isbnd == true)
			fail = false;
#endif
		p_iqk_info->tmp1bcc = IQMUX[idx];

		if (fail) {
			odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
			odm_write_4byte(p_dm_odm, 0x1b24, (odm_read_4byte(p_dm_odm, 0x1b24) & 0xffffc3ff) | (*(plna + p_iqk_info->lna_idx) << 10));
		}
	}
	return fail;
}

boolean
_lok_one_shot_8821c(
	struct PHY_DM_STRUCT	*p_dm_void,
	u8 path,
	u8          uPADindex

)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	u8		delay_count = 0, i;
	boolean		LOK_notready = false;
	u32		LOK_temp1 = 0, LOK_temp2 = 0, LOK_temp3 = 0;
	u32		IQK_CMD = 0x0;
	u8		LOKreg[] = {0x58, 0x78};

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[IQK]==========S%d LOK ==========\n", p_iqk_info->is_BTG));

	IQK_CMD = 0xf8000008 | (1 << (4 + path));

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]LOK_Trigger = 0x%x\n", IQK_CMD));

	_iqk_set_gnt_wl_gnt_bt(p_dm_odm, true);
	odm_write_4byte(p_dm_odm, 0x1b00, IQK_CMD);
	odm_write_4byte(p_dm_odm, 0x1b00, IQK_CMD + 1);
	/*LOK: CMD ID = 0	{0xf8000018, 0xf8000028}*/
	/*LOK: CMD ID = 0	{0xf8000019, 0xf8000029}*/
	ODM_delay_ms(LOK_delay_8821C);

	delay_count = 0;
	LOK_notready = true;

	while (LOK_notready) {
		if (odm_read_4byte(p_dm_odm, 0x1b00) == (IQK_CMD & 0xffffff0f))
			LOK_notready = false;
		else
			LOK_notready = true;

		if (LOK_notready) {
			ODM_delay_ms(1);
			delay_count++;
		}

		if (delay_count >= 50) {
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				     ("[IQK]S%d LOK timeout!!!\n", path));
			break;
		}
	}

	_iqk_set_gnt_wl_gnt_bt(p_dm_odm, false);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[IQK]S%d ==> delay_count = 0x%x\n", path, delay_count));

	if (!LOK_notready) {
		LOK_temp2 = odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x8, RFREGOFFSETMASK);
		LOK_temp3 = odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x58, RFREGOFFSETMASK);

		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			("[IQK]0x8 = 0x%x, 0x58 = 0x%x\n", LOK_temp2, LOK_temp3));
	} else {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("[IQK]==>S%d LOK Fail!!!\n", path));
	}
	p_iqk_info->LOK_fail[path] = LOK_notready;

	/*fill IDAC LUT table*/
	/*
	for (i = 0; i < 8; i++) {
		odm_set_rf_reg(p_dm_odm, path, 0x33, BIT(2)|BIT(1)|BIT(0), i);
		odm_set_rf_reg(p_dm_odm, path, 0x8, RFREGOFFSETMASK, LOK_temp2);
	}
	*/
	return LOK_notready;
}

boolean
_iqk_one_shot_8821c(
	void		*p_dm_void,
	u8		path,
	u8		idx
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	u8		delay_count = 0;
	boolean		notready = true, fail = true, search_fail = true;
	u32		IQK_CMD = 0x0, tmp;
	u16		iqk_apply[2] = {0xc94, 0xe94};

	if (idx == TX_IQK)
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]============ S%d WBTXIQK ============\n", p_iqk_info->is_BTG));
	else if (idx == RXIQK1)
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]============ S%d WBRXIQK STEP1============\n", p_iqk_info->is_BTG));
	else
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]============ S%d WBRXIQK STEP2============\n", p_iqk_info->is_BTG));

	if (idx == TXIQK) {
		IQK_CMD = 0xf8000008 | ((*p_dm_odm->p_band_width + 4) << 8) | (1 << (path + 4));
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]TXK_Trigger = 0x%x\n", IQK_CMD));
		/*{0xf8000418, 0xf800042a} ==> 20 WBTXK (CMD = 4)*/
		/*{0xf8000518, 0xf800052a} ==> 40 WBTXK (CMD = 5)*/
		/*{0xf8000618, 0xf800062a} ==> 80 WBTXK (CMD = 6)*/
	} else if (idx == RXIQK1) {
		if (*p_dm_odm->p_band_width == 2)
			IQK_CMD = 0xf8000808 | (1 << (path + 4));
		else
			IQK_CMD = 0xf8000708 | (1 << (path + 4));
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]RXK1_Trigger = 0x%x\n", IQK_CMD));
		/*{0xf8000718, 0xf800072a} ==> 20 WBTXK (CMD = 7)*/
		/*{0xf8000718, 0xf800072a} ==> 40 WBTXK (CMD = 7)*/
		/*{0xf8000818, 0xf800082a} ==> 80 WBTXK (CMD = 8)*/
	} else if (idx == RXIQK2) {
		IQK_CMD = 0xf8000008 | ((*p_dm_odm->p_band_width + 9) << 8) | (1 << (path + 4));
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]RXK2_Trigger = 0x%x\n", IQK_CMD));
		/*{0xf8000918, 0xf800092a} ==> 20 WBRXK (CMD = 9)*/
		/*{0xf8000a18, 0xf8000a2a} ==> 40 WBRXK (CMD = 10)*/
		/*{0xf8000b18, 0xf8000b2a} ==> 80 WBRXK (CMD = 11)*/
	}

	_iqk_set_gnt_wl_gnt_bt(p_dm_odm, true);


	odm_write_4byte(p_dm_odm, 0x1bc8, 0x80000000);
	odm_write_4byte(p_dm_odm, 0x8f8, 0x41400080);


	odm_write_4byte(p_dm_odm, 0x1b00, IQK_CMD);
	odm_write_4byte(p_dm_odm, 0x1b00, IQK_CMD + 0x1);
	ODM_delay_ms(WBIQK_delay_8821C);


	while (notready) {
		if (odm_read_4byte(p_dm_odm, 0xfa0) & BIT(27))/*if (odm_read_4byte(p_dm_odm, 0x1b00) == (IQK_CMD & 0xffffff0f))*/
			notready = false;
		else
			notready = true;

		if (notready) {
			ODM_delay_ms(1);
			delay_count++;
		} else {
			fail = (boolean) odm_get_bb_reg(p_dm_odm, 0x1b08, BIT(26));
			break;
		}

		if (delay_count >= 50) {
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				     ("[IQK]S%d IQK timeout!!!\n", path));
			break;
		}
	}

	RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]check 0x49c = %x\n", odm_read_1byte(p_dm_odm, 0x49c)));
	_iqk_set_gnt_wl_gnt_bt(p_dm_odm, false);

	if (p_dm_odm->debug_components && ODM_COMP_CALIBRATION) {
		odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			("[IQK]S%d ==> 0x1b00 = 0x%x, 0x1b08 = 0x%x\n", path, odm_read_4byte(p_dm_odm, 0x1b00), odm_read_4byte(p_dm_odm, 0x1b08)));
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			("[IQK]S%d ==> delay_count = 0x%x\n", path, delay_count));
		if (idx != TXIQK)
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
				("[IQK]S%d ==> RF0x0 = 0x%x, RF0x%x = 0x%x\n", path,
				odm_get_rf_reg(p_dm_odm, path, 0x0, RFREGOFFSETMASK), (p_iqk_info->is_BTG) ? 0x78 : 0x56,
				(p_iqk_info->is_BTG) ? odm_get_rf_reg(p_dm_odm, path, 0x78, RFREGOFFSETMASK) : odm_get_rf_reg(p_dm_odm, path, 0x56, RFREGOFFSETMASK)));
	}

	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | path << 1);
	if (idx == TXIQK)
		if (fail)
			odm_set_bb_reg(p_dm_odm, iqk_apply[path], BIT(0), 0x0);

	if (idx == RXIQK2) {
		p_iqk_info->RXIQK_AGC[0][path] =
			(u16)(((odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x0, RFREGOFFSETMASK) >> 5) & 0xff) |
			      (p_iqk_info->tmp1bcc << 8));

		odm_write_4byte(p_dm_odm, 0x1b38, 0x20000000);

		if (!fail)
			odm_set_bb_reg(p_dm_odm, iqk_apply[path], (BIT(11) | BIT(10)), 0x1);
		else
			odm_set_bb_reg(p_dm_odm, iqk_apply[path], (BIT(11) | BIT(10)), 0x0);
	}

	if (idx == TXIQK)
		p_iqk_info->IQK_fail_report[0][path][TXIQK] = fail;
	else
		p_iqk_info->IQK_fail_report[0][path][RXIQK] = fail;

	return fail;
}

boolean
_iqk_rxiqkbystep_8821c(
	void		*p_dm_void,
	u8		path
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	boolean		KFAIL = true, gonext;

#if 1
	switch (p_iqk_info->rxiqk_step) {
	case 1:		/*gain search_RXK1*/
		_iqk_rxk1setting_8821c(p_dm_odm, path);
		gonext = false;
		while (1) {
			KFAIL = _iqk_rx_iqk_gain_search_fail_8821c(p_dm_odm, path, RXIQK1);
			if (KFAIL && (p_iqk_info->gs_retry_count[0][path][RXIQK1] < 2))
				p_iqk_info->gs_retry_count[0][path][RXIQK1]++;
			else if (KFAIL) {
				p_iqk_info->RXIQK_fail_code[0][path] = 0;
				p_iqk_info->rxiqk_step = 5;
				gonext = true;
			} else {
				p_iqk_info->rxiqk_step++;
				gonext = true;
			}
			if (gonext)
				break;
		}
		break;
	case 2:		/*gain search_RXK2*/
		_iqk_rxk2setting_8821c(p_dm_odm, path, true);
		p_iqk_info->isbnd = false;
		while (1) {
			KFAIL = _iqk_rx_iqk_gain_search_fail_8821c(p_dm_odm, path, RXIQK2);
			if (KFAIL && (p_iqk_info->gs_retry_count[0][path][RXIQK2] < rxiqk_gs_limit))
				p_iqk_info->gs_retry_count[0][path][RXIQK2]++;
			else {
				p_iqk_info->rxiqk_step++;
				break;
			}
		}
		break;
	case 3:		/*RXK1*/
		_iqk_rxk1setting_8821c(p_dm_odm, path);
		gonext = false;
		while (1) {
			KFAIL = _iqk_one_shot_8821c(p_dm_odm, path, RXIQK1);
			if (KFAIL && (p_iqk_info->retry_count[0][path][RXIQK1] < 2))
				p_iqk_info->retry_count[0][path][RXIQK1]++;
			else if (KFAIL) {
				p_iqk_info->RXIQK_fail_code[0][path] = 1;
				p_iqk_info->rxiqk_step = 5;
				gonext = true;
			} else {
				p_iqk_info->rxiqk_step++;
				gonext = true;
			}
			if (gonext)
				break;
		}
		break;
	case 4:		/*RXK2*/
		_iqk_rxk2setting_8821c(p_dm_odm, path, false);
		gonext = false;
		while (1) {
			KFAIL = _iqk_one_shot_8821c(p_dm_odm, path,	RXIQK2);
			if (KFAIL && (p_iqk_info->retry_count[0][path][RXIQK2] < 2))
				p_iqk_info->retry_count[0][path][RXIQK2]++;
			else if (KFAIL) {
				p_iqk_info->RXIQK_fail_code[0][path] = 2;
				p_iqk_info->rxiqk_step = 5;
				gonext = true;
			} else {
				p_iqk_info->rxiqk_step++;
				gonext = true;
			}
			if (gonext)
				break;
		}
		break;
	}
	return KFAIL;
#endif
}


void
_iqk_iqk_by_path_8821c(
	void		*p_dm_void,
	boolean		segment_iqk
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	boolean		KFAIL = true;
	u8		i, kcount_limit;
	u32		cnt_iqk_fail = 0;
	/*	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]iqk_step = 0x%x\n", p_dm_odm->rf_calibrate_info.iqk_step)); */

	if (*p_dm_odm->p_band_width == 2)
		kcount_limit = kcount_limit_80m;
	else
		kcount_limit = kcount_limit_others;

	while (1) {
		switch (p_dm_odm->rf_calibrate_info.iqk_step) {
		case 1:		/*S0 LOK*/
			for (i = 0; i < 8 ; i++) {/* the LOK Cal in the each PAD stage*/
				_iqk_lok_setting_8821c(p_dm_odm, ODM_RF_PATH_A, i);
				_lok_one_shot_8821c(p_dm_odm, ODM_RF_PATH_A, i);
			}
			p_dm_odm->rf_calibrate_info.iqk_step++;
			break;
		case 2:		/*S0 TXIQK*/
			_iqk_txk_setting_8821c(p_dm_odm, ODM_RF_PATH_A);
			KFAIL = _iqk_one_shot_8821c(p_dm_odm, ODM_RF_PATH_A, TXIQK);
			p_iqk_info->kcount++;
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]KFail = 0x%x\n", KFAIL));
			if (KFAIL)
				cnt_iqk_fail++;
			if (KFAIL && (p_iqk_info->retry_count[0][ODM_RF_PATH_A][TXIQK] < 3))
				p_iqk_info->retry_count[0][ODM_RF_PATH_A][TXIQK]++;
			else
				p_dm_odm->rf_calibrate_info.iqk_step++;
			break;
		case 3:		/*S0 RXIQK*/
			while (1) {
				KFAIL = _iqk_rxiqkbystep_8821c(p_dm_odm, ODM_RF_PATH_A);
				ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]S0RXK KFail = 0x%x\n", KFAIL));
				if (p_iqk_info->rxiqk_step == 5) {
					p_dm_odm->rf_calibrate_info.iqk_step++;
					p_iqk_info->rxiqk_step = 1;
					if (KFAIL) {
						cnt_iqk_fail++;
						ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
							("[IQK]S0RXK fail code: %d!!!\n", p_iqk_info->RXIQK_fail_code[0][ODM_RF_PATH_A]));
					}
					break;
				}
			}
			p_iqk_info->kcount++;
			break;
		}


		if (p_dm_odm->rf_calibrate_info.iqk_step == 4) {
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
				("[IQK]==========LOK summary ==========\n"));
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				     ("[IQK]PathA_LOK_notready = %d\n",
				      p_iqk_info->LOK_fail[ODM_RF_PATH_A]));
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
				("[IQK]==========IQK summary ==========\n"));
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				     ("[IQK]PathA_TXIQK_fail = %d\n",
				p_iqk_info->IQK_fail_report[0][ODM_RF_PATH_A][TXIQK]));
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				     ("[IQK]PathA_RXIQK_fail = %d\n",
				p_iqk_info->IQK_fail_report[0][ODM_RF_PATH_A][RXIQK]));
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				     ("[IQK]PathA_TXIQK_retry = %d\n",
				p_iqk_info->retry_count[0][ODM_RF_PATH_A][TXIQK]));
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				("[IQK]PathA_RXK1_retry = %d, PathA_RXK2_retry = %d\n",
				p_iqk_info->retry_count[0][ODM_RF_PATH_A][RXIQK1], p_iqk_info->retry_count[0][ODM_RF_PATH_A][RXIQK2]));
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				("[IQK]PathA_GS1_retry = %d, PathA_GS2_retry = %d\n",
				p_iqk_info->gs_retry_count[0][ODM_RF_PATH_A][RXIQK1], p_iqk_info->gs_retry_count[0][ODM_RF_PATH_A][RXIQK2]));

			for (i = 0; i < SS_8821C; i++) {
				odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008 | i << 1);
				odm_write_4byte(p_dm_odm, 0x1b2c, 0x7);
				odm_write_4byte(p_dm_odm, 0x1bcc, 0x0);
				odm_write_4byte(p_dm_odm, 0x1b38, 0x20000000);
			}
			break;
		}

		p_dm_odm->n_iqk_cnt++;

		if (cnt_iqk_fail == 0)
			p_dm_odm->n_iqk_ok_cnt++;
		else
			p_dm_odm->n_iqk_fail_cnt = p_dm_odm->n_iqk_fail_cnt + cnt_iqk_fail;

		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			("All/Ok/Fail = %d %d %d\n", p_dm_odm->n_iqk_cnt, p_dm_odm->n_iqk_ok_cnt, p_dm_odm->n_iqk_fail_cnt));

		if ((segment_iqk == true) && (p_iqk_info->kcount == kcount_limit))
			break;

	}
}

void
_iqk_start_iqk_8821c(
	struct PHY_DM_STRUCT		*p_dm_odm,
	boolean			segment_iqk
)
{
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	u32 tmp;

	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008);
	odm_write_4byte(p_dm_odm, 0x1bb8, 0x00000000);

	/*GNT_WL = 1*/
	if (p_iqk_info->is_BTG) {
		tmp = odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x1, RFREGOFFSETMASK);
		tmp = (tmp & (~BIT(3))) | BIT(0) | BIT(2);
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x1, RFREGOFFSETMASK, tmp);
	}

	_iqk_iqk_by_path_8821c(p_dm_odm, segment_iqk);
}

void
_iq_calibrate_8821c_init(
	void		*p_dm_void
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;
	u8	i, j, k, m;

	if (p_iqk_info->iqk_times == 0x0) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]=====>PHY_IQCalibrate_8821C_Init\n"));

		for (i = 0; i < SS_8821C; i++) {
			for (j = 0; j < 2; j++) {
				p_iqk_info->LOK_fail[i] = true;
				p_iqk_info->IQK_fail[j][i] = true;
				p_iqk_info->iqc_matrix[j][i] = 0x20000000;
			}
		}

		for (i = 0; i < 2; i++) {
			p_iqk_info->iqk_channel[i] = 0x0;

			for (j = 0; j < SS_8821C; j++) {
				p_iqk_info->LOK_IDAC[i][j] = 0x0;
				p_iqk_info->RXIQK_AGC[i][j] = 0x0;
				p_iqk_info->bypass_iqk[i][j] = 0x0;

				for (k = 0; k < 2; k++) {
					p_iqk_info->IQK_fail_report[i][j][k] = true;
					for (m = 0; m < 8; m++) {
						p_iqk_info->IQK_CFIR_real[i][j][k][m] = 0x0;
						p_iqk_info->IQK_CFIR_imag[i][j][k][m] = 0x0;
					}
				}

				for (k = 0; k < 3; k++)
					p_iqk_info->retry_count[i][j][k] = 0x0;
			}
		}
	}
}
/*
void
_DPK_BackupReg_8821C(
	struct PHY_DM_STRUCT*	p_dm_odm,
	static u32*	DPK_backup,
	u32*		backup_dpk_reg
	)
{

	u32 i;

	for (i = 0; i < DPK_BACKUP_REG_NUM_8821C; i++)
		DPK_backup[i] = odm_read_4byte(p_dm_odm, backup_dpk_reg[i]);

}
void
_DPK_Restore_8821C(
	struct PHY_DM_STRUCT*		p_dm_odm,
	static  u32*		DPK_backup,
	u32*		backup_dpk_reg
	)
{
	u32 i;
	for (i = 0; i < DPK_BACKUP_REG_NUM_8821C; i++)
		odm_write_4byte(p_dm_odm, backup_dpk_reg[i], DPK_backup[i]);
}

*/
void
_dpk_dpk_setting_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path
)
{
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[DPK]==========Start the DPD setting Initilaize/n"));
	/*AFE setting*/
	odm_write_4byte(p_dm_odm, 0xc60, 0x50000000);
	odm_write_4byte(p_dm_odm, 0xc60, 0x700F0040);
	odm_write_4byte(p_dm_odm, 0xc5c, 0xd1000120);
	odm_write_4byte(p_dm_odm, 0xc58, 0xd8000402);
	odm_write_4byte(p_dm_odm, 0xc6c, 0x00000a15);
	odm_write_4byte(p_dm_odm, 0xc00, 0x00000000);
	/*_iqk_bb_reset_8821c(p_dm_odm);*/
	odm_write_4byte(p_dm_odm, 0xe5c, 0xD1000120);
	odm_write_4byte(p_dm_odm, 0xc6c, 0x00000A15);
	odm_write_4byte(p_dm_odm, 0xe6c, 0x00000A15);
	odm_write_4byte(p_dm_odm, 0x808, 0x2D028200);
	odm_write_4byte(p_dm_odm, 0x810, 0x20101063);
	odm_write_4byte(p_dm_odm, 0x90c, 0x0B00C000);
	odm_write_4byte(p_dm_odm, 0x9a4, 0x00000080);
	odm_write_4byte(p_dm_odm, 0xc94, 0x01000101);
	odm_write_4byte(p_dm_odm, 0xe94, 0x01000101);
	odm_write_4byte(p_dm_odm, 0xe5c, 0xD1000120);
	odm_write_4byte(p_dm_odm, 0xc6c, 0x00000A15);
	odm_write_4byte(p_dm_odm, 0xe6c, 0x00000A15);


	odm_write_4byte(p_dm_odm, 0x808, 0x2D028200);
	odm_write_4byte(p_dm_odm, 0x810, 0x20101063);
	odm_write_4byte(p_dm_odm, 0x90c, 0x0B00C000);
	odm_write_4byte(p_dm_odm, 0x9a4, 0x00000080);
	odm_write_4byte(p_dm_odm, 0xc94, 0x01000101);
	odm_write_4byte(p_dm_odm, 0xe94, 0x01000101);
	odm_write_4byte(p_dm_odm, 0x1904, 0x00020000);
	/*path A*/
	odm_set_bb_reg(p_dm_odm, 0x1d00, MASKDWORD, 0x30303030); /* cck */
	odm_set_bb_reg(p_dm_odm, 0x1d04, MASKDWORD, 0x30303030); /* ofdm 6M/9M/12M/18M */
	odm_set_bb_reg(p_dm_odm, 0x1d08, MASKDWORD, 0x30303030); /* ofdm 24M/36M/48M/54M */
	odm_set_bb_reg(p_dm_odm, 0x1d0c, MASKDWORD, 0x30303030); /* mcs0~3 */
	odm_set_bb_reg(p_dm_odm, 0x1d10, MASKDWORD, 0x30303030); /* mcs4~7 */
	odm_set_bb_reg(p_dm_odm, 0x1d2c, MASKDWORD, 0x30303030); /* vht_1ss_mcs0~3 */
	odm_set_bb_reg(p_dm_odm, 0x1d30, MASKDWORD, 0x30303030); /* vht_1ss_mcs4~7 */
	odm_set_bb_reg(p_dm_odm, 0x1d34, 0x0000FFFF, 0x3030);     /* vht_1ss_mcs8/9 */

	/*RF*/
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xEF, RFREGOFFSETMASK, 0x80000);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x33, RFREGOFFSETMASK, 0x00024);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x3E, RFREGOFFSETMASK, 0x0003F);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x3F, RFREGOFFSETMASK, 0xCBFCE);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xEF, RFREGOFFSETMASK, 0x00000);
	/*AGC boundary selection*/
	odm_write_4byte(p_dm_odm, 0x1bbc, 0x0001abf6);
	odm_write_4byte(p_dm_odm, 0x1b90, 0x0001e018);
	odm_write_4byte(p_dm_odm, 0x1bb8, 0x000fffff);
	odm_write_4byte(p_dm_odm, 0x1bc8, 0x000c55aa);
	/*odm_write_4byte(p_dm_odm, 0x1bcc, 0x11978200);*/
	odm_write_4byte(p_dm_odm, 0x1bcc, 0x11978800);
	odm_write_4byte(p_dm_odm, 0xcb0, 0x77775747);
	odm_write_4byte(p_dm_odm, 0xcb4, 0x100000f7);
	odm_write_4byte(p_dm_odm, 0xcbc, 0x0);
}

void
_dpk_dynamic_bias_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path,
	u8 dynamicbias
)
{
	u32 tmp;
	tmp = odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xdf, RFREGOFFSETMASK);
	tmp = tmp | BIT(8);
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xdf, RFREGOFFSETMASK, tmp);
	if ((*p_dm_odm->p_band_type == ODM_BAND_5G) && (*p_dm_odm->p_band_width == 1))
		odm_set_rf_reg(p_dm_odm, path, 0x61, BIT(7) | BIT(6) | BIT(5) | BIT(4), dynamicbias);
	if ((*p_dm_odm->p_band_type == ODM_BAND_5G) && (*p_dm_odm->p_band_width == 2))
		odm_set_rf_reg(p_dm_odm, path, 0x61, BIT(7) | BIT(6) | BIT(5) | BIT(4), dynamicbias);
	tmp = tmp & (~BIT(8));
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xdf, RFREGOFFSETMASK, tmp);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[DPK]Set DynamicBias 0xdf=0x%x, 0x61=0x%x\n", odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0xdf, RFREGOFFSETMASK), odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x61, RFREGOFFSETMASK)));

}


void
_dpk_dpk_boundary_selection_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path
)
{
	u8 tmp_pad, compared_pad, reg_tmp, compared_txbb;
	u8 tmp_txbb = 0;
	u32 rf_backup_reg00;
	u8 i = 0;
	u8 j = 1;
	u32 boundaryselect = 0;
	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[DPK]Start the DPD boundary selection\n"));
	rf_backup_reg00 = odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x00, RFREGOFFSETMASK);
	tmp_pad = 0;
	compared_pad = 0;
	boundaryselect = 0;
#if dpk_forcein_sram4
	for (i = 0x1f; i > 0x0; i--) { /*i=tx index*/
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x00, RFREGOFFSETMASK, 0x20000 + i);

		if (p_iqk_info->is_BTG) {
			compared_pad = (u8)((0x1c000 & odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x78, RFREGOFFSETMASK)) >> 14);
			compared_txbb = (u8)((0x07C00 & odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x5c, RFREGOFFSETMASK)) >> 10);
		} else {
			compared_pad = (u8)((0xe0 & odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x56, RFREGOFFSETMASK)) >> 5);
			compared_txbb = (u8)((0x1f & odm_get_rf_reg(p_dm_odm, (enum odm_rf_radio_path_e)path, 0x56, RFREGOFFSETMASK)));
		}
		if (i == 0x1f) {
			/*boundaryselect = compared_txbb;*/
			boundaryselect = 0x1f;
			tmp_pad = compared_pad;
		}
		if (compared_pad < tmp_pad) {
			boundaryselect = boundaryselect + (i << (j * 5));
			tmp_pad = compared_pad ;
			j++;
		}

		if (j >= 4)
			break;
	}

#else
	boundaryselect = 0x0;
#endif
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x00, RFREGOFFSETMASK, rf_backup_reg00);
	odm_write_4byte(p_dm_odm, 0x1bbc, boundaryselect);

}

u8
_dpk_get_dpk_tx_agc_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path
)
{

	u8 tx_agc_init_value = 0x1f; /* DPK TXAGC value*/
	u32 rf_reg00 = 0x0;
	u8 gainloss = 0x1;
	u8 best_tx_agc ;
	u8 tmp;
	u8 i = 0;
	boolean notready = true;
	u8 delay_count = 0x0;
	u32 temp2;
	/* rf_reg00 = 0x40000 + tx_agc_init_value;  set TXAGC value */
	if (*p_dm_odm->p_band_type == ODM_BAND_5G) {
		tx_agc_init_value = 0x1d;
		rf_reg00 = 0x40000 + tx_agc_init_value; /* set TXAGC value*/
		odm_write_4byte(p_dm_odm, 0x1bc8, 0x000c55aa);
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x8F, RFREGOFFSETMASK, 0xA9C00);
	} else {
		tx_agc_init_value = 0x17;
		rf_reg00 = 0x44000 + tx_agc_init_value; /* set TXAGC value*/
		odm_write_4byte(p_dm_odm, 0x1bc8, 0x000c44aa);
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x8F, RFREGOFFSETMASK, 0xAEC00);
	}
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x00, RFREGOFFSETMASK, rf_reg00);
	odm_set_bb_reg(p_dm_odm, 0x1b8c, BIT(15) | BIT(14) | BIT(13), gainloss);
	odm_set_bb_reg(p_dm_odm, 0x1bc8, BIT(31), 0x1);
	odm_set_bb_reg(p_dm_odm, 0x8f8, BIT(25) | BIT(24) | BIT(23) | BIT(22), 0x5);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000d18);
	/*ODM_delay_ms(1);*/
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000d19);
	ODM_delay_ms(2);

#if 0
	ODM_delay_ms(100);
#else
	while (notready) {
		if (odm_read_4byte(p_dm_odm, 0xfa0) & BIT(27))/*if (odm_read_4byte(p_dm_odm, 0x1b00) == (IQK_CMD & 0xffffff0f))*/
			notready = false;
		else
			notready = true;

		if (notready) {
			ODM_delay_ms(1);
			delay_count++;
		}

		if (delay_count >= 50) {
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				("[DPK]S%d DPK_GetDPKTXAGC_8821C timeout!!!\n", path));
			break;
		}
	}

	odm_write_4byte(p_dm_odm, 0x1b90, 0x0001e018);
#endif

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("rf_reg00 =0x%x, 0x8F =0x%x\n",	 odm_get_rf_reg(p_dm_odm, path, 0x00, RFREGOFFSETMASK), odm_get_rf_reg(p_dm_odm, path, 0x8f, RFREGOFFSETMASK)));


	odm_write_4byte(p_dm_odm, 0x1bd4, 0x60001);
	tmp = (u8)odm_read_4byte(p_dm_odm, 0x1bfc);
	best_tx_agc = tx_agc_init_value - (0xa - tmp);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[DPK](2), 0x1b8c =0x%x, delay_count=%d, rf_reg00 = 0x%x, 0x1b00 = 0x%x, 0x1bfc = 0x%x, 0x1bd4 = 0x%x,best_tx_agc =0x%x, tmp =0x%x, delay =%d ms\n",
		odm_read_4byte(p_dm_odm, 0x1b8c), delay_count, rf_reg00, odm_read_4byte(p_dm_odm, 0x1b00), odm_read_4byte(p_dm_odm, 0x1bfc), odm_read_4byte(p_dm_odm, 0x1bd4), best_tx_agc, tmp, i * 2));
	/* dbg message*/
#if 0

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[DQK] 3  0x1bcc = 0x%x, 0x1bb8 =0x%x\n", odm_read_4byte(p_dm_odm, 0x1bcc), odm_read_4byte(p_dm_odm, 0x1bb8)));

	odm_write_4byte(p_dm_odm, 0x1bcc, 0x118f8800);
	for (i = 0 ; i < 8; i++) {
		odm_write_4byte(p_dm_odm, 0x1b90, 0x0101e018 + i);
		odm_write_4byte(p_dm_odm, 0x1bd4, 0x00060000);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("0x%x\n",
			      odm_read_4byte(p_dm_odm, 0x1bfc)));
		odm_write_4byte(p_dm_odm, 0x1bd4, 0x00070000);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("0x%x\n",
			      odm_read_4byte(p_dm_odm, 0x1bfc)));
		odm_write_4byte(p_dm_odm, 0x1bd4, 0x00080000);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("0x%x\n",
			      odm_read_4byte(p_dm_odm, 0x1bfc)));
		odm_write_4byte(p_dm_odm, 0x1bd4, 0x00090000);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("0x%x\n",
			      odm_read_4byte(p_dm_odm, 0x1bfc)));

	}

	odm_write_4byte(p_dm_odm, 0x1b90, 0x0001e018);

#endif
	return best_tx_agc;

}

boolean
_dpk_enable_dpk_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path,
	u8 best_tx_agc
)
{
	u32 rf_reg00 = 0x0;
	u32 tmp;
	boolean fail = true;
	u8 i = 0;
	boolean notready = true;
	u8 delay_count = 0x0;

	if (*p_dm_odm->p_band_type == ODM_BAND_5G)	   {
		rf_reg00 = 0x40000 + best_tx_agc; /* set TXAGC value*/
	} else {
		rf_reg00 = 0x44000 + best_tx_agc; /* set TXAGC value*/
	}
	odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x00, RFREGOFFSETMASK, rf_reg00);
	ODM_delay_ms(1);
	odm_set_bb_reg(p_dm_odm, 0x1bc8, BIT(31), 0x1);
	odm_write_4byte(p_dm_odm, 0x8f8, 0x41400080);
	ODM_delay_ms(1);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000e18);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000e19);
#if 0
	ODM_delay_ms(10);
#else

	ODM_delay_ms(5);
	while (notready) {
		if (odm_read_4byte(p_dm_odm, 0xfa0) & BIT(27))/*if (odm_read_4byte(p_dm_odm, 0x1b00) == (IQK_CMD & 0xffffff0f))*/
			notready = false;
		else
			notready = true;

		if (notready) {
			ODM_delay_ms(1);
			delay_count++;
		}

		if (delay_count >= 50) {
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				("[DPK]S%d DPK_GetDPKTXAGC_8821C timeout!!!\n", path));
			break;
		}
	}


#endif
	odm_write_4byte(p_dm_odm, 0x1b90, 0x0001e018);
	odm_write_4byte(p_dm_odm, 0x1bd4, 0xA0001);
	tmp = odm_read_4byte(p_dm_odm, 0x1bfc);

	if ((odm_read_4byte(p_dm_odm, 0x1b08) & 0x0f000000) == 0x0)
		fail = false;
	else
		fail = true;
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[DPK] (3), delay_count= %d, 0x1b0b = 0x%x, 0x1bc8 = 0x%x, rf_reg00 = 0x%x, ,0x1bfc = 0x%x, 0x1b90=0x%x, 0x1b94=0x%x\n",
		delay_count, odm_read_1byte(p_dm_odm, 0x1b0b), odm_read_4byte(p_dm_odm, 0x1bc8), rf_reg00, tmp, odm_read_4byte(p_dm_odm, 0x1b90), odm_read_4byte(p_dm_odm, 0x1b94)));
	/* dbg message*/
#if 0
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008);
	odm_write_4byte(p_dm_odm, 0x1b08, 0x00000080);
	odm_write_4byte(p_dm_odm, 0x1bd4, 0x00040001);

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
		     ("[DPK] SRAM value!!!\n"));

	for (i = 0 ; i < 64; i++) {
		/*odm_write_4byte(p_dm_odm, 0x1b90, 0x0101e018+i);*/
		odm_write_4byte(p_dm_odm, 0x1bdc, 0xc0000081 + i * 2);

		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("0x%x\n", odm_read_4byte(p_dm_odm, 0x1bfc)));

	}
	odm_write_4byte(p_dm_odm, 0x1bd4, 0x00050001);
	for (i = 0 ; i < 64; i++) {
		/*odm_write_4byte(p_dm_odm, 0x1b90, 0x0101e018+i);*/
		odm_write_4byte(p_dm_odm, 0x1bdc, 0xc0000081 + i * 2);

		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("0x%x\n", odm_read_4byte(p_dm_odm, 0x1bfc)));

	}

	/*odm_write_4byte(p_dm_odm, 0x1b08, 0x00000080);*/
	odm_write_4byte(p_dm_odm, 0x1bd4, 0x00000001);
	odm_write_4byte(p_dm_odm, 0x1bdc, 0x00000000);
#endif
	return fail;

}


boolean
_dpk_enable_dpd_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8 path,
	u8 best_tx_agc
)
{

	boolean fail = true;
	u8 tmp;
	u8 offset = 0x0;
	u8 i = 0;
	boolean notready = true;
	u8 delay_count = 0x0;


	odm_set_bb_reg(p_dm_odm, 0x1bc8, BIT(31), 0x1);
	odm_write_4byte(p_dm_odm, 0x8f8, 0x41400080);
	ODM_delay_ms(1);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000f18);
	odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000f19);
	ODM_delay_ms(30);
#if 0
	ODM_delay_ms(100);
#else
	while (notready) {
		if (odm_read_4byte(p_dm_odm, 0xfa0) & BIT(27))/*if (odm_read_4byte(p_dm_odm, 0x1b00) == (IQK_CMD & 0xffffff0f))*/
			notready = false;
		else
			notready = true;

		if (notready) {
			ODM_delay_ms(1);
			delay_count++;
		}

		if (delay_count >= 50) {
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				("[DPK]S%d DPK_GetDPKTXAGC_8821C timeout!!!\n", path));
			break;
		}
	}
	odm_write_4byte(p_dm_odm, 0x1b90, 0x0001e018);
#endif
	odm_write_4byte(p_dm_odm, 0x1b90, 0x0001e018);
	odm_write_4byte(p_dm_odm, 0x1bd4, 0xA0001);
	tmp = odm_read_1byte(p_dm_odm, 0x1bfc);


	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[DPK](4) init 0x1b08 =%x, 0x1bc8 = 0x%x,0x1bfc = 0x%x, ,0x1bd0 = 0x%x, offset =%x, 1bcc =%x\n",
		odm_read_4byte(p_dm_odm, 0x1b08), odm_read_4byte(p_dm_odm, 0x1bc8), tmp, odm_read_4byte(p_dm_odm, 0x1bd0), offset, odm_read_4byte(p_dm_odm, 0x1bcc)));

	/*if( (odm_read_4byte(p_dm_odm, 0x1b08) & 0x0f000000) == 0x0)*/
	if (true) {
		odm_write_4byte(p_dm_odm, 0x1b98, 0x48004800);
		odm_write_4byte(p_dm_odm, 0x1bdc, 0x0);

		if (best_tx_agc >= 0x19)
			offset = best_tx_agc - 0x19;
		else
			offset = 0x20 - (0x19 - best_tx_agc);
		odm_set_bb_reg(p_dm_odm, 0x1bd0, BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8), offset);

		fail = false;

		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			("[DPK](4) OK 0x1b08 =%x, 0x1bc8 = 0x%x,0x1bfc = 0x%x, ,0x1bd0 = 0x%x, offset =%x, 1bcc =%x\n",
			odm_read_4byte(p_dm_odm, 0x1b08), odm_read_4byte(p_dm_odm, 0x1bc8), tmp, odm_read_4byte(p_dm_odm, 0x1bd0), offset, odm_read_4byte(p_dm_odm, 0x1bcc)));
	} else
		fail = true;




	return fail;

}
void
phy_dpd_calibrate_8821c(
	struct PHY_DM_STRUCT		*p_dm_odm,
	boolean			reset
)
{

	u32  backup_dpdbb[3];
	u32	backup_dpdbb_reg[3] = {0x1b2c, 0x1b38, 0xc1b3c};
	u8	best_tx_agc = 0x1c;
	u32	MAC_backup[MAC_REG_NUM_8821C], RF_backup[RF_REG_NUM_8821C][1];
	u32	backup_mac_reg[MAC_REG_NUM_8821C] = {0x520, 0x550, 0x1518};
	u32  BB_backup[DPK_BB_REG_NUM_8821C];
	u32	backup_bb_reg[DPK_BB_REG_NUM_8821C] = {0x808, 0x90c, 0xc00, 0xcb0, 0xcb4, 0xcbc, 0x1990, 0x9a4, 0xa04
		, 0xc58, 0xc5c, 0xe58, 0xe5c, 0xc6c, 0xe6c, 0x810, 0x90c, 0xc94, 0xe94, 0x1904, 0xcb0, 0xcb4, 0xcbc, 0xc00
						  };
	u32	backup_rf_reg[RF_REG_NUM_8821C] = {0xdf,  0xde, 0x8f, 0x65, 0x0, 0x1};
	u8  i;
	u32	backup_dpk_reg[3] = {0x1bd0, 0x1b98, 0x1bbc};


	struct _IQK_INFORMATION   *p_iqk_info = &p_dm_odm->IQK_info;
	p_iqk_info->is_BTG = (boolean) odm_get_bb_reg(p_dm_odm, 0xcb8, BIT(16));
	if (!p_dm_odm->mp_mode)
		if (_iqk_reload_iqk_8821c(p_dm_odm, reset))
			return;
	/*2G is not stable*/
	/* if (!(*p_dm_odm->p_band_type == ODM_BAND_5G)) return; */

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[DPK]==========DPK strat!!!!!==========\n"));
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
		("[DPK]p_band_type = %s, band_width = %d, ExtPA2G = %d, ext_pa_5g = %d\n", (*p_dm_odm->p_band_type == ODM_BAND_5G) ? "5G" : "2G", *p_dm_odm->p_band_width, p_dm_odm->ext_pa, p_dm_odm->ext_pa_5g));
#if 1
	_iqk_backup_mac_bb_8821c(p_dm_odm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg, DPK_BB_REG_NUM_8821C);
	_iqk_afe_setting_8821c(p_dm_odm, true);
	_iqk_backup_rf_8821c(p_dm_odm, RF_backup, backup_rf_reg);

#else
	_iqk_rfe_setting_8821c(p_dm_odm, false);
	_iqk_agc_bnd_int_8821c(p_dm_odm);
	_iqk_rf_setting_8821c(p_dm_odm);

#endif


	if (p_iqk_info->is_BTG) {
	} else {
		if (*p_dm_odm->p_band_type == ODM_BAND_2_4G)
			odm_set_bb_reg(p_dm_odm, 0xcb8, BIT(8), 0x1);
		else
			odm_set_bb_reg(p_dm_odm, 0xcb8, BIT(8), 0x0);
	}


	/*backup 0x1b2c, 1b38,0x1b3c*/
	{
		backup_dpdbb[0] = odm_read_4byte(p_dm_odm, 0x1b2c);
	}
	{
		backup_dpdbb[1] = odm_read_4byte(p_dm_odm, 0x1b38);
	}
	{
		backup_dpdbb[2] = odm_read_4byte(p_dm_odm, 0x1b3c);
	}
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[DPK]In DPD Proces(1), Backup\n"));
#if 1

	/*PDK Init Register setting*/
	_dpk_dpk_setting_8821c(p_dm_odm, ODM_RF_PATH_A);
	_dpk_dpk_boundary_selection_8821c(p_dm_odm, ODM_RF_PATH_A);
	odm_set_bb_reg(p_dm_odm, 0x1bc8, BIT(31), 0x1);
	odm_set_bb_reg(p_dm_odm, 0x8f8, BIT(25) | BIT(24) | BIT(23) | BIT(22), 0x5);
	/* Get the best TXAGC*/
#endif

#if 1
	best_tx_agc = _dpk_get_dpk_tx_agc_8821c(p_dm_odm, ODM_RF_PATH_A);
#endif
	ODM_delay_ms(2);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[DPK]In DPD Process(2), Best TXAGC = 0x%x\n", best_tx_agc));

#if 1
	if (_dpk_enable_dpk_8821c(p_dm_odm, ODM_RF_PATH_A, best_tx_agc)) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("[DPK]In DPD Process(3), DPK process is Fail\n"));
	}
#endif
#if 1
	ODM_delay_ms(2);
	if (_dpk_enable_dpd_8821c(p_dm_odm, ODM_RF_PATH_A, best_tx_agc)) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
			     ("[DPK]In DPD Process(4), DPD process is Fail\n"));
	}
#endif
	/* restore IQK */
	p_iqk_info->rf_reg18 = odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x18, RFREGOFFSETMASK);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[DPK]reload IQK result before, p_iqk_info->rf_reg18=0x%x, p_iqk_info->iqk_channel[0]=0x%x, p_iqk_info->iqk_channel[1]=0x%x!!!!\n", p_iqk_info->rf_reg18, p_iqk_info->iqk_channel[0], p_iqk_info->iqk_channel[1]));
	_iqk_reload_iqk_setting_8821c(p_dm_odm, 0, 2);
	_iqk_fill_iqk_report_8821c(p_dm_odm, 0);
	/* Restore setup */
	odm_set_bb_reg(p_dm_odm, 0x8f8, BIT(25) | BIT(24) | BIT(23) | BIT(22), 0x5);
	odm_set_bb_reg(p_dm_odm, 0x1bd4, BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16), 0x0);
	odm_set_bb_reg(p_dm_odm, 0x1b00, BIT(2) | BIT(1), 0x0);
	odm_set_bb_reg(p_dm_odm, 0x1b08, BIT(6) | BIT(5), 0x2);

	odm_write_4byte(p_dm_odm, 0x1b2c, backup_dpdbb[0]);
	odm_write_4byte(p_dm_odm, 0x1b38, backup_dpdbb[1]);
	odm_write_4byte(p_dm_odm, 0x1b3c, backup_dpdbb[2]);
	/*enable DPK*/
	odm_set_bb_reg(p_dm_odm, 0x1b2c, BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0), 0x5);
	/*enable boundary condition*/
#if dpk_forcein_sram4 /* disable : froce in sram4*/
	odm_set_bb_reg(p_dm_odm, 0x1bcc, BIT(27), 0x1);
#endif
	odm_write_4byte(p_dm_odm, 0x1bcc, 0x11868800);
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[DPK]In DPD Process(5), Restore\n"));
#if 1
	_iqk_restore_mac_bb_8821c(p_dm_odm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg, DPK_BB_REG_NUM_8821C);
	_iqk_afe_setting_8821c(p_dm_odm, false);
	_iqk_restore_rf_8821c(p_dm_odm, backup_rf_reg, RF_backup);
#else
	_iqk_restore_rf_8821c(p_dm_odm, backup_rf_reg, RF_backup);
#endif
	/* backup the DPK current result*/
	for (i = 0; i < DPK_BACKUP_REG_NUM_8821C; i++)
		dpk_result[i] = odm_read_4byte(p_dm_odm, backup_dpk_reg[i]);

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		("[DPK]the DPD calibration Process Finish (6), dpk_result = 0x%x\n", dpk_result[0]));
	return;
}

void
_phy_iq_calibrate_8821c(
	struct PHY_DM_STRUCT		*p_dm_odm,
	boolean			reset
)
{

	u32	MAC_backup[MAC_REG_NUM_8821C], BB_backup[BB_REG_NUM_8821C], RF_backup[RF_REG_NUM_8821C][1];
	u32	backup_mac_reg[MAC_REG_NUM_8821C] = {0x520, 0x550, 0x1518};
	u32	backup_bb_reg[BB_REG_NUM_8821C] = {0x808, 0x90c, 0xc00, 0xcb0, 0xcb4, 0xcbc, 0x1990, 0x9a4, 0xa04, 0xb00};
	u32	backup_rf_reg[RF_REG_NUM_8821C] = {0xdf,  0xde, 0x8f, 0x65, 0x0, 0x1};
	u8	i, j;
	boolean segment_iqk = false, is_mp = false;

	struct _IQK_INFORMATION	*p_iqk_info = &p_dm_odm->IQK_info;

	if (p_dm_odm->mp_mode)
		is_mp = true;
	else if (p_dm_odm->is_linked)
		segment_iqk = false;

	p_iqk_info->is_BTG = (boolean)odm_get_bb_reg(p_dm_odm, 0xcb8, BIT(16));

	if (!is_mp)
		if (_iqk_reload_iqk_8821c(p_dm_odm, reset))
			return;

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[IQK]==========IQK strat!!!!!==========\n"));

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
		("[IQK]p_band_type = %s, band_width = %d, ExtPA2G = %d, ext_pa_5g = %d\n", (*p_dm_odm->p_band_type == ODM_BAND_5G) ? "5G" : "2G", *p_dm_odm->p_band_width, p_dm_odm->ext_pa, p_dm_odm->ext_pa_5g));
	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
		("[IQK]Interface = %d, cut_version = %x\n", p_dm_odm->support_interface, p_dm_odm->cut_version));

	p_iqk_info->tmp_GNTWL = _iqk_indirect_read_reg(p_dm_odm, 0x38);

	p_iqk_info->iqk_times++;
	p_iqk_info->kcount = 0;
	p_dm_odm->rf_calibrate_info.iqk_total_progressing_time = 0;
	p_dm_odm->rf_calibrate_info.iqk_step = 1;
	p_iqk_info->rxiqk_step = 1;

	_iqk_backup_iqk_8821c(p_dm_odm, 0);
	_iqk_backup_mac_bb_8821c(p_dm_odm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg,BB_REG_NUM_8821C);
	_iqk_backup_rf_8821c(p_dm_odm, RF_backup, backup_rf_reg);
#if 0
	_iqk_configure_macbb_8821c(p_dm_odm);
	_iqk_afe_setting_8821c(p_dm_odm, true);
	_iqk_rfe_setting_8821c(p_dm_odm, false);
	_iqk_agc_bnd_int_8821c(p_dm_odm);
	_IQK_RFSetting_8821C(p_dm_odm);
#endif

	while (1) {
		if (!is_mp)
			p_dm_odm->rf_calibrate_info.iqk_start_time = odm_get_current_time(p_dm_odm);

		_iqk_configure_macbb_8821c(p_dm_odm);
		_iqk_afe_setting_8821c(p_dm_odm, true);
		_iqk_rfe_setting_8821c(p_dm_odm, false);
		_iqk_agc_bnd_int_8821c(p_dm_odm);
		_iqk_rfsetting_8821c(p_dm_odm);

		_iqk_start_iqk_8821c(p_dm_odm, segment_iqk);

		_iqk_afe_setting_8821c(p_dm_odm, false);
		_iqk_restore_mac_bb_8821c(p_dm_odm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg,BB_REG_NUM_8821C);
		_iqk_restore_rf_8821c(p_dm_odm, backup_rf_reg, RF_backup);

		if (!is_mp) {
			p_dm_odm->rf_calibrate_info.iqk_progressing_time = odm_get_progressing_time(p_dm_odm, p_dm_odm->rf_calibrate_info.iqk_start_time);
			p_dm_odm->rf_calibrate_info.iqk_total_progressing_time += odm_get_progressing_time(p_dm_odm, p_dm_odm->rf_calibrate_info.iqk_start_time);
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
				("[IQK]IQK progressing_time = %lld ms\n", p_dm_odm->rf_calibrate_info.iqk_progressing_time));
		}

		if (p_dm_odm->rf_calibrate_info.iqk_step == 4)
			break;

		p_iqk_info->kcount = 0;
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]delay 50ms!!!\n"));
		ODM_delay_ms(50);
	};

	_iqk_backup_iqk_8821c(p_dm_odm, 1);
#if 0
	_iqk_afe_setting_8821c(p_dm_odm, false);
	_iqk_restore_mac_bb_8821c(p_dm_odm, MAC_backup, BB_backup, backup_mac_reg, backup_bb_reg);
	_iqk_restore_rf_8821c(p_dm_odm, backup_rf_reg, RF_backup);
#endif
	_iqk_fill_iqk_report_8821c(p_dm_odm, 0);

	if (!is_mp)
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]Total IQK progressing_time = %lld ms\n",
			p_dm_odm->rf_calibrate_info.iqk_total_progressing_time));

	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
		     ("[IQK]==========IQK end!!!!!==========\n"));

	RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]check 0x49c = %x\n", odm_read_1byte(p_dm_odm, 0x49c)));
}


void
_phy_iq_calibrate_by_fw_8821c(
	struct PHY_DM_STRUCT	*p_dm_odm,
	u8		clear
)
{

	u8			iqk_cmd[3] = { *p_dm_odm->p_channel, 0x0, 0x0};
	u8			buf1 = 0x0;
	u8			buf2 = 0x0;
}


/*IQK version:0xe, NCTL:0x7*/
/*1. disable segment IQK*/
void
phy_iq_calibrate_8821c(
	void		*p_dm_void,
	boolean		clear
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	u32		counter = 0x0;

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct _ADAPTER		*p_adapter = p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(p_adapter);

#if (MP_DRIVER == 1)
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
		PMPT_CONTEXT	p_mpt_ctx = &(p_adapter->MptCtx);
#else
	PMPT_CONTEXT	p_mpt_ctx = &(p_adapter->mppriv.mpt_ctx);
#endif
#endif
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN))
	if (odm_check_power_status(p_adapter) == false)
		return;
#endif

#if MP_DRIVER == 1
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	if (p_mpt_ctx->bSingleTone || p_mpt_ctx->bCarrierSuppression)
	return;
#else
	if (p_mpt_ctx->is_single_tone || p_mpt_ctx->is_carrier_suppression)
		return;
#endif
#endif
#endif

	if (!p_dm_odm->mp_mode)
		_iqk_check_coex_status(p_dm_odm, true);

	if (*(p_dm_odm->p_is_scan_in_process)) {
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]scan is in process, bypass IQK\n"));
		return;
	}

	p_dm_odm->iqk_fw_offload = 0;

	/*FW IQK*/
	if (p_dm_odm->iqk_fw_offload) {
		if (!p_dm_odm->rf_calibrate_info.is_iqk_in_progress) {
			odm_acquire_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
			p_dm_odm->rf_calibrate_info.is_iqk_in_progress = true;
			odm_release_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);

			p_dm_odm->rf_calibrate_info.iqk_start_time = odm_get_current_time(p_dm_odm);

			odm_write_4byte(p_dm_odm, 0x1b00, 0xf8000008);
			odm_set_bb_reg(p_dm_odm, 0x1bf0, 0xff000000, 0xff);
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE,
				("[IQK]0x1bf0 = 0x%x\n", odm_read_4byte(p_dm_odm, 0x1bf0)));

			_phy_iq_calibrate_by_fw_8821c(p_dm_odm, clear);

			while (1) {
				if (((odm_read_4byte(p_dm_odm, 0x1bf0) >> 24) == 0x7f) || (counter > 300))
					break;

				counter++;
				ODM_delay_ms(1);
			};

			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_TRACE, ("[IQK]counter = %d\n", counter));

			p_dm_odm->rf_calibrate_info.iqk_progressing_time = odm_get_progressing_time(p_dm_odm, p_dm_odm->rf_calibrate_info.iqk_start_time);

			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]IQK progressing_time = %lld ms\n", p_dm_odm->rf_calibrate_info.iqk_progressing_time));

			odm_acquire_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
			p_dm_odm->rf_calibrate_info.is_iqk_in_progress = false;
			odm_release_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
		}	else
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("== Return the IQK CMD, because the IQK in Progress ==\n"));
	} else {

		_iq_calibrate_8821c_init(p_dm_void);

		if (!p_dm_odm->rf_calibrate_info.is_iqk_in_progress) {
			odm_acquire_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
			p_dm_odm->rf_calibrate_info.is_iqk_in_progress = true;
			odm_release_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
			if (p_dm_odm->mp_mode)
				p_dm_odm->rf_calibrate_info.iqk_start_time = odm_get_current_time(p_dm_odm);

#if (DM_ODM_SUPPORT_TYPE & (ODM_CE))
			_phy_iq_calibrate_8821c(p_dm_odm, clear);
			/*DBG_871X("%s,%d, do IQK %u ms\n", __func__, __LINE__, rtw_get_passing_time_ms(time_iqk));*/
#else
			_phy_iq_calibrate_8821c(p_dm_odm, clear);
#endif
			if (p_dm_odm->mp_mode) {
				p_dm_odm->rf_calibrate_info.iqk_progressing_time = odm_get_progressing_time(p_dm_odm, p_dm_odm->rf_calibrate_info.iqk_start_time);
				ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]IQK progressing_time = %lld ms\n", p_dm_odm->rf_calibrate_info.iqk_progressing_time));
			}
			odm_acquire_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
			p_dm_odm->rf_calibrate_info.is_iqk_in_progress = false;
			odm_release_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
		} else
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[IQK]== Return the IQK CMD, because the IQK in Progress ==\n"));
	}

#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	_iqk_iqk_fail_report_8821c(p_dm_odm);
#endif

	if (!p_dm_odm->mp_mode)
		_iqk_check_coex_status(p_dm_odm, false);
	RT_TRACE(COMP_COEX, DBG_LOUD, ("[IQK]final 0x49c = %x\n", odm_read_1byte(p_dm_odm, 0x49c)));
}

void
phy_dp_calibrate_8821c(
	void		*p_dm_void,
	boolean		clear
)
{
	struct PHY_DM_STRUCT	*p_dm_odm = (struct PHY_DM_STRUCT *)p_dm_void;
	u32		counter = 0x0;


#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct _ADAPTER		*p_adapter = p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(p_adapter);

#if (MP_DRIVER == 1)
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	PMPT_CONTEXT	p_mpt_ctx = &(p_adapter->MptCtx);
#else
	PMPT_CONTEXT	p_mpt_ctx = &(p_adapter->mppriv.mpt_ctx);
#endif
#endif
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN))
	if (odm_check_power_status(p_adapter) == false)
		return;
#endif

#if MP_DRIVER == 1
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	if (p_mpt_ctx->bSingleTone || p_mpt_ctx->bCarrierSuppression)
		return;
#else
	if (p_mpt_ctx->is_single_tone || p_mpt_ctx->is_carrier_suppression)
		return;
#endif
#endif

#endif


	ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[DPK] In PHY, p_dm_odm->dpk_en == %x\n", p_dm_odm->dpk_en));

	/*if dpk is not enable*/
	if (p_dm_odm->dpk_en == 0x0)
		return;

	/*start*/
	if (!p_dm_odm->rf_calibrate_info.is_iqk_in_progress) {
		odm_acquire_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
		p_dm_odm->rf_calibrate_info.is_iqk_in_progress = true;
		odm_release_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
		if (p_dm_odm->mp_mode)
			p_dm_odm->rf_calibrate_info.iqk_start_time = odm_get_current_time(p_dm_odm);

		/*do DPK*/
		phy_dpd_calibrate_8821c(p_dm_odm, clear);


		if (p_dm_odm->mp_mode) {
			p_dm_odm->rf_calibrate_info.iqk_progressing_time = odm_get_progressing_time(p_dm_odm, p_dm_odm->rf_calibrate_info.iqk_start_time);
			ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[DPK]DPK progressing_time = %lld ms\n", p_dm_odm->rf_calibrate_info.iqk_progressing_time));
		}
		odm_acquire_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
		p_dm_odm->rf_calibrate_info.is_iqk_in_progress = false;
		odm_release_spin_lock(p_dm_odm, RT_IQK_SPINLOCK);
	} else
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[DPK]== Return the DPK CMD, because the DPK in Progress ==\n"));




}
void dpk_temperature_compensate_8821c(
	void		*p_dm_void
)
{

	struct PHY_DM_STRUCT		*p_dm_odm	=	(struct PHY_DM_STRUCT *) p_dm_void;
	struct _ADAPTER		*adapter = p_dm_odm->adapter;
	HAL_DATA_TYPE	*p_hal_data = GET_HAL_DATA(adapter);
	static u8	dpk_tm_trigger = 0;
	u8			thermal_value = 0, delta_dpk, p = 0, i = 0;
	u8			thermal_value_avg_count = 0;
	u8          thermal_value_avg_times = 2;
	u32	        thermal_value_avg = 0;
	u8          tmp, abs_temperature;

	/*if dpk is not enable*/
	if (p_dm_odm->dpk_en == 0x0)
		return;


	if (!dpk_tm_trigger) {
		odm_set_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x42, BIT(17) | BIT(16), 0x03);
		/*ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[DPK] (1) Trigger Thermal Meter!!\n"));*/
		dpk_tm_trigger = 1;
		return;
	} else {

		/* Initialize */
		dpk_tm_trigger = 0;
		/*ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("[DPK] (2) calculate the thermal !!\n"));
		*/

		/* calculate average thermal meter */
		thermal_value = (u8)odm_get_rf_reg(p_dm_odm, ODM_RF_PATH_A, 0x42, 0xfc00);	/*0x42: RF Reg[15:10] 88E*/
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			("[DPK] (3) current Thermal Meter = %d\n", thermal_value));

		p_dm_odm->rf_calibrate_info.thermal_value_dpk = thermal_value;
		p_dm_odm->rf_calibrate_info.thermal_value_avg[p_dm_odm->rf_calibrate_info.thermal_value_avg_index] = thermal_value;
		p_dm_odm->rf_calibrate_info.thermal_value_avg_index++;
		if (p_dm_odm->rf_calibrate_info.thermal_value_avg_index == thermal_value_avg_times)
			p_dm_odm->rf_calibrate_info.thermal_value_avg_index = 0;
		for (i = 0; i < thermal_value_avg_times; i++) {
			if (p_dm_odm->rf_calibrate_info.thermal_value_avg[i]) {
				thermal_value_avg += p_dm_odm->rf_calibrate_info.thermal_value_avg[i];
				thermal_value_avg_count++;
			}
		}
		if (thermal_value_avg_count)  /*Calculate Average thermal_value after average enough times*/
			thermal_value = (u8)(thermal_value_avg / thermal_value_avg_count);
		/* compensate the DPK */
		delta_dpk = (thermal_value > p_hal_data->eeprom_thermal_meter) ? (thermal_value - p_hal_data->eeprom_thermal_meter) : (p_hal_data->eeprom_thermal_meter - thermal_value);
		tmp = (u8)((dpk_result[0] & 0x00001f00) >> 8);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			("[DPK] (5)delta_dpk = %d, eeprom_thermal_meter = %d, tmp=%d\n", delta_dpk, p_hal_data->eeprom_thermal_meter, tmp));


		if (thermal_value > p_hal_data->eeprom_thermal_meter) {
			abs_temperature = thermal_value - p_hal_data->eeprom_thermal_meter;
			if (abs_temperature >= 20)
				tmp = tmp + 4;
			else if (abs_temperature >= 15)
				tmp = tmp + 3;
			else if (abs_temperature >= 10)
				tmp = tmp + 2;
			else if (abs_temperature >= 5)
				tmp = tmp + 1;
		} else { /*low temperature*/
			abs_temperature = p_hal_data->eeprom_thermal_meter - thermal_value;
			if (abs_temperature >= 20)
				tmp = tmp - 4;
			else if (abs_temperature >= 15)
				tmp = tmp - 3;
			else if (abs_temperature >= 10)
				tmp = tmp - 2;
			else if (abs_temperature >= 5)
				tmp = tmp - 1;
		}

		odm_set_bb_reg(p_dm_odm, 0x1bd0, BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8), tmp);
		ODM_RT_TRACE(p_dm_odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD,
			("[DPK] (6)delta_dpk = %d, eeprom_thermal_meter = %d, new tmp=%d, 0x1bd0=0x%x\n", delta_dpk, p_hal_data->eeprom_thermal_meter, tmp, odm_read_4byte(p_dm_odm, 0x1bd0)));

	}
}

#endif
