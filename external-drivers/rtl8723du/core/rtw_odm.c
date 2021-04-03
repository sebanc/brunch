// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2013 - 2017 Realtek Corporation */

#include <rtw_odm.h>
#include <hal_data.h>

u32 rtw_phydm_ability_ops(struct adapter *adapter, enum hal_phydm_ops ops, u32 ability)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(adapter);
	struct PHY_DM_STRUCT *podmpriv = &pHalData->odmpriv;
	u32 result = 0;

	switch (ops) {
	case HAL_PHYDM_DIS_ALL_FUNC:
		podmpriv->support_ability = DYNAMIC_FUNC_DISABLE;
		halrf_cmn_info_set(podmpriv, HALRF_CMNINFO_ABILITY, DYNAMIC_FUNC_DISABLE);
		break;
	case HAL_PHYDM_FUNC_SET:
		podmpriv->support_ability |= ability;
		break;
	case HAL_PHYDM_FUNC_CLR:
		podmpriv->support_ability &= ~(ability);
		break;
	case HAL_PHYDM_ABILITY_BK:
		/* dm flag backup*/
		podmpriv->bk_support_ability = podmpriv->support_ability;
		pHalData->bk_rf_ability = halrf_cmn_info_get(podmpriv, HALRF_CMNINFO_ABILITY);
		break;
	case HAL_PHYDM_ABILITY_RESTORE:
		/* restore dm flag */
		podmpriv->support_ability = podmpriv->bk_support_ability;
		halrf_cmn_info_set(podmpriv, HALRF_CMNINFO_ABILITY, pHalData->bk_rf_ability);
		break;
	case HAL_PHYDM_ABILITY_SET:
		podmpriv->support_ability = ability;
		break;
	case HAL_PHYDM_ABILITY_GET:
		result = podmpriv->support_ability;
		break;
	}
	return result;
}

/* set ODM_CMNINFO_IC_TYPE based on chip_type */
void rtw_odm_init_ic_type(struct adapter *adapter)
{
	struct PHY_DM_STRUCT *odm = adapter_to_phydm(adapter);
	u32 ic_type = chip_type_to_odm_ic_type(rtw_get_chip_type(adapter));

	rtw_warn_on(!ic_type);

	odm_cmn_info_init(odm, ODM_CMNINFO_IC_TYPE, ic_type);
}

static void rtw_odm_adaptivity_ver_msg(void *sel, struct adapter *adapter)
{
	RTW_PRINT_SEL(sel, "ADAPTIVITY_VERSION "ADAPTIVITY_VERSION"\n");
}

#define RTW_ADAPTIVITY_EN_DISABLE 0
#define RTW_ADAPTIVITY_EN_ENABLE 1

static void rtw_odm_adaptivity_en_msg(void *sel, struct adapter *adapter)
{
	struct registry_priv *regsty = &adapter->registrypriv;

	RTW_PRINT_SEL(sel, "RTW_ADAPTIVITY_EN_");

	if (regsty->adaptivity_en == RTW_ADAPTIVITY_EN_DISABLE)
		_RTW_PRINT_SEL(sel, "DISABLE\n");
	else if (regsty->adaptivity_en == RTW_ADAPTIVITY_EN_ENABLE)
		_RTW_PRINT_SEL(sel, "ENABLE\n");
	else
		_RTW_PRINT_SEL(sel, "INVALID\n");
}

#define RTW_ADAPTIVITY_MODE_NORMAL 0
#define RTW_ADAPTIVITY_MODE_CARRIER_SENSE 1

static void rtw_odm_adaptivity_mode_msg(void *sel, struct adapter *adapter)
{
	struct registry_priv *regsty = &adapter->registrypriv;

	RTW_PRINT_SEL(sel, "RTW_ADAPTIVITY_MODE_");

	if (regsty->adaptivity_mode == RTW_ADAPTIVITY_MODE_NORMAL)
		_RTW_PRINT_SEL(sel, "NORMAL\n");
	else if (regsty->adaptivity_mode == RTW_ADAPTIVITY_MODE_CARRIER_SENSE)
		_RTW_PRINT_SEL(sel, "CARRIER_SENSE\n");
	else
		_RTW_PRINT_SEL(sel, "INVALID\n");
}

#define RTW_ADAPTIVITY_DML_DISABLE 0
#define RTW_ADAPTIVITY_DML_ENABLE 1

static void rtw_odm_adaptivity_dml_msg(void *sel, struct adapter *adapter)
{
	struct registry_priv *regsty = &adapter->registrypriv;

	RTW_PRINT_SEL(sel, "RTW_ADAPTIVITY_DML_");

	if (regsty->adaptivity_dml == RTW_ADAPTIVITY_DML_DISABLE)
		_RTW_PRINT_SEL(sel, "DISABLE\n");
	else if (regsty->adaptivity_dml == RTW_ADAPTIVITY_DML_ENABLE)
		_RTW_PRINT_SEL(sel, "ENABLE\n");
	else
		_RTW_PRINT_SEL(sel, "INVALID\n");
}

static void rtw_odm_adaptivity_dc_backoff_msg(void *sel, struct adapter *adapter)
{
	struct registry_priv *regsty = &adapter->registrypriv;

	RTW_PRINT_SEL(sel, "RTW_ADAPTIVITY_DC_BACKOFF:%u\n", regsty->adaptivity_dc_backoff);
}

void rtw_odm_adaptivity_config_msg(void *sel, struct adapter *adapter)
{
	rtw_odm_adaptivity_ver_msg(sel, adapter);
	rtw_odm_adaptivity_en_msg(sel, adapter);
	rtw_odm_adaptivity_mode_msg(sel, adapter);
	rtw_odm_adaptivity_dml_msg(sel, adapter);
	rtw_odm_adaptivity_dc_backoff_msg(sel, adapter);
}

bool rtw_odm_adaptivity_needed(struct adapter *adapter)
{
	struct registry_priv *regsty = &adapter->registrypriv;
	bool ret = false;

	if (regsty->adaptivity_en == RTW_ADAPTIVITY_EN_ENABLE)
		ret = true;

	return ret;
}

void rtw_odm_adaptivity_parm_msg(void *sel, struct adapter *adapter)
{
	struct PHY_DM_STRUCT *odm = adapter_to_phydm(adapter);

	rtw_odm_adaptivity_config_msg(sel, adapter);

	RTW_PRINT_SEL(sel, "%10s %16s %16s %22s %12s\n"
		, "th_l2h_ini", "th_edcca_hl_diff", "th_l2h_ini_mode2", "th_edcca_hl_diff_mode2", "edcca_enable");
	RTW_PRINT_SEL(sel, "0x%-8x %-16d 0x%-14x %-22d %-12d\n"
		, (u8)odm->th_l2h_ini
		, odm->th_edcca_hl_diff
		, (u8)odm->th_l2h_ini_mode2
		, odm->th_edcca_hl_diff_mode2
		, odm->edcca_enable
	);

	RTW_PRINT_SEL(sel, "%15s %9s\n", "AdapEnableState", "Adap_Flag");
	RTW_PRINT_SEL(sel, "%-15x %-9x\n"
		, odm->adaptivity_enable
		, odm->adaptivity_flag
	);
}

void rtw_odm_adaptivity_parm_set(struct adapter *adapter, s8 th_l2h_ini, s8 th_edcca_hl_diff, s8 th_l2h_ini_mode2, s8 th_edcca_hl_diff_mode2, u8 edcca_enable)
{
	struct PHY_DM_STRUCT *odm = adapter_to_phydm(adapter);

	odm->th_l2h_ini = th_l2h_ini;
	odm->th_edcca_hl_diff = th_edcca_hl_diff;
	odm->th_l2h_ini_mode2 = th_l2h_ini_mode2;
	odm->th_edcca_hl_diff_mode2 = th_edcca_hl_diff_mode2;
	odm->edcca_enable = edcca_enable;
}

void rtw_odm_get_perpkt_rssi(void *sel, struct adapter *adapter)
{
	struct PHY_DM_STRUCT *odm = adapter_to_phydm(adapter);

	RTW_PRINT_SEL(sel, "rx_rate = %s, RSSI_A = %d(%%), RSSI_B = %d(%%)\n",
		      HDATA_RATE(odm->rx_rate), odm->RSSI_A, odm->RSSI_B);
}


void rtw_odm_acquirespinlock(struct adapter *adapter,	enum rt_spinlock_type type)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapter);
	unsigned long irqL;

	switch (type) {
	case RT_IQK_SPINLOCK:
		_enter_critical_bh(&pHalData->IQKSpinLock, &irqL);
	default:
		break;
	}
}

void rtw_odm_releasespinlock(struct adapter *adapter,	enum rt_spinlock_type type)
{
	struct hal_com_data *	pHalData = GET_HAL_DATA(adapter);
	unsigned long irqL;

	switch (type) {
	case RT_IQK_SPINLOCK:
		_exit_critical_bh(&pHalData->IQKSpinLock, &irqL);
	default:
		break;
	}
}

inline u8 rtw_odm_get_dfs_domain(struct adapter *adapter)
{
	return PHYDM_DFS_DOMAIN_UNKNOWN;
}

inline u8 rtw_odm_dfs_domain_unknown(struct adapter *adapter)
{
	return 1;
}

void rtw_odm_parse_rx_phy_status_chinfo(union recv_frame *rframe, u8 *phys)
{
	struct adapter *adapter = rframe->u.hdr.adapter;
	struct PHY_DM_STRUCT *phydm = adapter_to_phydm(adapter);
	struct rx_pkt_attrib *attrib = &rframe->u.hdr.attrib;

	if (phydm->support_ic_type & ODM_IC_PHY_STATUE_NEW_TYPE) {
		if ((*phys & 0xf) == 1) {
			struct _phy_status_rpt_jaguar2_type1 *phys_t1 = (struct _phy_status_rpt_jaguar2_type1 *)phys;
			u8 rxsc = (attrib->data_rate > DESC_RATE11M && attrib->data_rate < DESC_RATEMCS0) ? phys_t1->l_rxsc : phys_t1->ht_rxsc;
			u8 pkt_cch = 0;
			u8 pkt_bw = CHANNEL_WIDTH_20;

			#if	ODM_IC_11N_SERIES_SUPPORT
			if (phydm->support_ic_type & ODM_IC_11N_SERIES) {
				/* RXSC N-series */
				#define RXSC_DUP	0
				#define RXSC_LSC	1
				#define RXSC_USC	2
				#define RXSC_40M	3

				static const s8 cch_offset_by_rxsc[4] = {0, -2, 2, 0};

				if (phys_t1->rf_mode == 0) {
					pkt_cch = phys_t1->channel;
					pkt_bw = CHANNEL_WIDTH_20;
				} else if (phys_t1->rf_mode == 1) {
					if (rxsc == RXSC_LSC || rxsc == RXSC_USC) {
						pkt_cch = phys_t1->channel + cch_offset_by_rxsc[rxsc];
						pkt_bw = CHANNEL_WIDTH_20;
					} else if (rxsc == RXSC_40M) {
						pkt_cch = phys_t1->channel;
						pkt_bw = CHANNEL_WIDTH_40;
					}
				} else
					rtw_warn_on(1);

				goto type1_end;
			}
			#endif /* ODM_IC_11N_SERIES_SUPPORT */

			#if	ODM_IC_11AC_SERIES_SUPPORT
			if (phydm->support_ic_type & ODM_IC_11AC_SERIES) {
				/* RXSC AC-series */
				#define RXSC_DUP			0 /* 0: RX from all SC of current rf_mode */

				#define RXSC_LL20M_OF_160M	8 /* 1~8: RX from 20MHz SC */
				#define RXSC_L20M_OF_160M	6
				#define RXSC_L20M_OF_80M	4
				#define RXSC_L20M_OF_40M	2
				#define RXSC_U20M_OF_40M	1
				#define RXSC_U20M_OF_80M	3
				#define RXSC_U20M_OF_160M	5
				#define RXSC_UU20M_OF_160M	7

				#define RXSC_L40M_OF_160M	12 /* 9~12: RX from 40MHz SC */
				#define RXSC_L40M_OF_80M	10
				#define RXSC_U40M_OF_80M	9
				#define RXSC_U40M_OF_160M	11

				#define RXSC_L80M_OF_160M	14 /* 13~14: RX from 80MHz SC */
				#define RXSC_U80M_OF_160M	13

				static const s8 cch_offset_by_rxsc[15] = {0, 2, -2, 6, -6, 10, -10, 14, -14, 4, -4, 12, -12, 8, -8};

				if (phys_t1->rf_mode > 3) {
					/* invalid rf_mode */
					rtw_warn_on(1);
					goto type1_end;
				}

				if (phys_t1->rf_mode == 0) {
					/* RF 20MHz */
					pkt_cch = phys_t1->channel;
					pkt_bw = CHANNEL_WIDTH_20;
					goto type1_end;
				}

				if (rxsc == 0) {
					/* RF and RX with same BW */
					if (attrib->data_rate >= DESC_RATEMCS0) {
						pkt_cch = phys_t1->channel;
						pkt_bw = phys_t1->rf_mode;
					}
					goto type1_end;
				}

				if ((phys_t1->rf_mode == 1 && rxsc >= 1 && rxsc <= 2) /* RF 40MHz, RX 20MHz */
					|| (phys_t1->rf_mode == 2 && rxsc >= 1 && rxsc <= 4) /* RF 80MHz, RX 20MHz */
					|| (phys_t1->rf_mode == 3 && rxsc >= 1 && rxsc <= 8) /* RF 160MHz, RX 20MHz */
				) {
					pkt_cch = phys_t1->channel + cch_offset_by_rxsc[rxsc];
					pkt_bw = CHANNEL_WIDTH_20;
				} else if ((phys_t1->rf_mode == 2 && rxsc >= 9 && rxsc <= 10) /* RF 80MHz, RX 40MHz */
					|| (phys_t1->rf_mode == 3 && rxsc >= 9 && rxsc <= 12) /* RF 160MHz, RX 40MHz */
				) {
					if (attrib->data_rate >= DESC_RATEMCS0) {
						pkt_cch = phys_t1->channel + cch_offset_by_rxsc[rxsc];
						pkt_bw = CHANNEL_WIDTH_40;
					}
				} else if ((phys_t1->rf_mode == 3 && rxsc >= 13 && rxsc <= 14) /* RF 160MHz, RX 80MHz */
				) {
					if (attrib->data_rate >= DESC_RATEMCS0) {
						pkt_cch = phys_t1->channel + cch_offset_by_rxsc[rxsc];
						pkt_bw = CHANNEL_WIDTH_80;
					}
				} else
					rtw_warn_on(1);

			}
			#endif /* ODM_IC_11AC_SERIES_SUPPORT */

type1_end:
			/* for now, only return cneter channel of 20MHz packet */
			if (pkt_cch && pkt_bw == CHANNEL_WIDTH_20)
				attrib->ch = pkt_cch;
		}
	}
}

