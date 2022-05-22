/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/

/*============================================================
 * include files
 *============================================================
 */
#include "mp_precomp.h"
#include "phydm_precomp.h"

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)

#if 0
u32 _sqrt(u64 n)
{
	u64	ans = 0, q = 0;
	s64	i;

	/*for (i = sizeof(n) * 8 - 2; i > -1; i = i - 2) {*/
	for (i = 8 * 8 - 2; i > -1; i = i - 2) {
		q = (q << 2) | ((n & (3 << i)) >> i);
		if (q >= ((ans << 2) | 1))
		{
			q = q - ((ans << 2) | 1);
			ans = (ans << 1) | 1;
		}
		else
			ans = ans << 1;
	}
	DbgPrint("ans=0x%x\n", ans);

	return (u32)ans;
}
#endif

u64 _sqrt(u64 x)
{
	u64 i = 0;
	u64 j = x / 2 + 1;

	while (i <= j) {
		u64 mid = (i + j) / 2;

		u64 sq = mid * mid;

		if (sq == x)
			return mid;
		else if (sq < x)
			i = mid + 1;
		else
			j = mid - 1;
	}

	return j;
}

u32 halrf_get_psd_data(
	struct dm_struct *dm,
	u32 point)
{
	struct _hal_rf_ *rf = &(dm->rf_table);
	struct _halrf_psd_data *psd = &(rf->halrf_psd_data);
	u32 psd_val = 0, psd_reg, psd_report, psd_point, psd_start, i, delay_time;

#if (DEV_BUS_TYPE == RT_USB_INTERFACE) || (DEV_BUS_TYPE == RT_SDIO_INTERFACE)
	if (psd->average == 0)
		delay_time = 100;
	else
		delay_time = 0;
#else
	if (psd->average == 0)
		delay_time = 1000;
	else
		delay_time = 100;
#endif

	if (dm->support_ic_type & (ODM_RTL8812 | ODM_RTL8821 | ODM_RTL8814A | ODM_RTL8822B | ODM_RTL8821C)) {
		psd_reg = R_0x910;
		psd_report = R_0xf44;
	} else {
		psd_reg = R_0x808;
		psd_report = R_0x8b4;
	}

	if (dm->support_ic_type & ODM_RTL8710B) {
		psd_point = 0xeffffc00;
		psd_start = 0x10000000;
	} else {
		psd_point = 0xffbffc00;
		psd_start = 0x00400000;
	}

	psd_val = odm_get_bb_reg(dm, psd_reg, MASKDWORD);

	psd_val &= psd_point;
	psd_val |= point;

	odm_set_bb_reg(dm, psd_reg, MASKDWORD, psd_val);

	psd_val |= psd_start;

	odm_set_bb_reg(dm, psd_reg, MASKDWORD, psd_val);

	for (i = 0; i < delay_time; i++)
		ODM_delay_us(1);

	psd_val = odm_get_bb_reg(dm, psd_report, MASKDWORD);

	if (dm->support_ic_type & (ODM_RTL8821C | ODM_RTL8710B)) {
		psd_val &= MASKL3BYTES;
		psd_val = psd_val / 32;
	} else
		psd_val &= MASKLWORD;

	return psd_val;
}

void halrf_psd(
	struct dm_struct *dm,
	u32 point,
	u32 start_point,
	u32 stop_point,
	u32 average)
{
	struct _hal_rf_ *rf = &(dm->rf_table);
	struct _halrf_psd_data *psd = &(rf->halrf_psd_data);

	u32 i = 0, j = 0, k = 0;
	u32 psd_reg, avg_org, point_temp, average_tmp;
	u64 data_tatal = 0, data_temp[64] = {0};

	psd->buf_size = 256;

	if (average == 0)
		average_tmp = 1;
	else
		average_tmp = average;

	if (dm->support_ic_type & (ODM_RTL8812 | ODM_RTL8821 | ODM_RTL8814A | ODM_RTL8822B | ODM_RTL8821C))
		psd_reg = R_0x910;
	else
		psd_reg = R_0x808;

#if 0
	dbg_print("[PSD]point=%d, start_point=%d, stop_point=%d, average=%d, average_tmp=%d, buf_size=%d\n",
		point, start_point, stop_point, average, average_tmp, psd->buf_size);
#endif

	for (i = 0; i < psd->buf_size; i++)
		psd->psd_data[i] = 0;

	if (dm->support_ic_type & ODM_RTL8710B)
		avg_org = odm_get_bb_reg(dm, psd_reg, 0x30000);
	else
		avg_org = odm_get_bb_reg(dm, psd_reg, 0x3000);

	if (average != 0) {
		if (dm->support_ic_type & ODM_RTL8710B)
			odm_set_bb_reg(dm, psd_reg, 0x30000, 0x1);
		else
			odm_set_bb_reg(dm, psd_reg, 0x3000, 0x1);
	}

#if 0
	if (avg_temp == 0)
		avg = 1;
	else if (avg_temp == 1)
		avg = 8;
	else if (avg_temp == 2)
		avg = 16;
	else if (avg_temp == 3)
		avg = 32;
#endif

	i = start_point;
	while (i < stop_point) {
		data_tatal = 0;

		if (i >= point)
			point_temp = i - point;
		else
			point_temp = i;

		for (k = 0; k < average_tmp; k++) {
			data_temp[k] = halrf_get_psd_data(dm, point_temp);
			data_tatal = data_tatal + (data_temp[k] * data_temp[k]);

#if 0
			if ((k % 20) == 0)
				dbg_print("\n ");

			dbg_print("0x%x ", data_temp[k]);
#endif
		}
		/*dbg_print("\n");*/

		data_tatal = ((data_tatal * 100) / average_tmp);
		psd->psd_data[j] = (u32)_sqrt(data_tatal);

		i++;
		j++;
	}

#if 0
	for (i = 0; i < psd->buf_size; i++) {
		if ((i % 20) == 0)
			dbg_print("\n ");

		dbg_print("0x%x ", psd->psd_data[i]);
	}
	dbg_print("\n\n");
#endif

	if (dm->support_ic_type & ODM_RTL8710B)
		odm_set_bb_reg(dm, psd_reg, 0x30000, avg_org);
	else
		odm_set_bb_reg(dm, psd_reg, 0x3000, avg_org);
}

enum rt_status
halrf_psd_init(
	struct dm_struct *dm)
{
	enum rt_status ret_status = RT_STATUS_SUCCESS;
	struct _hal_rf_ *rf = &(dm->rf_table);
	struct _halrf_psd_data *psd = &(rf->halrf_psd_data);

	if (psd->psd_progress)
		ret_status = RT_STATUS_PENDING;
	else {
		psd->psd_progress = 1;
		halrf_psd(dm, psd->point, psd->start_point, psd->stop_point, psd->average);
		psd->psd_progress = 0;
	}

	return ret_status;
}

enum rt_status
halrf_psd_query(
	struct dm_struct *dm,
	u32 *outbuf,
	u32 buf_size)
{
	enum rt_status ret_status = RT_STATUS_SUCCESS;
	struct _hal_rf_ *rf = &(dm->rf_table);
	struct _halrf_psd_data *psd = &(rf->halrf_psd_data);

	if (psd->psd_progress)
		ret_status = RT_STATUS_PENDING;
	else
		PlatformMoveMemory(outbuf, psd->psd_data, 0x400);

	return ret_status;
}

enum rt_status
halrf_psd_init_query(
	struct dm_struct *dm,
	u32 *outbuf,
	u32 point,
	u32 start_point,
	u32 stop_point,
	u32 average,
	u32 buf_size)
{
	enum rt_status ret_status = RT_STATUS_SUCCESS;
	struct _hal_rf_ *rf = &(dm->rf_table);
	struct _halrf_psd_data *psd = &(rf->halrf_psd_data);

	psd->point = point;
	psd->start_point = start_point;
	psd->stop_point = stop_point;
	psd->average = average;

	if (psd->psd_progress)
		ret_status = RT_STATUS_PENDING;
	else {
		psd->psd_progress = 1;
		halrf_psd(dm, psd->point, psd->start_point, psd->stop_point, psd->average);
		PlatformMoveMemory(outbuf, psd->psd_data, 0x400);
		psd->psd_progress = 0;
	}

	return ret_status;
}

#endif /*#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)*/
