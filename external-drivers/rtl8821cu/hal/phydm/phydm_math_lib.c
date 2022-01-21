/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

/*************************************************************
 * include files
 ************************************************************/

#include "mp_precomp.h"
#include "phydm_precomp.h"

const u16 db_invert_table[12][8] = {
	{1, 1, 1, 2, 2, 2, 2, 3},
	{3, 3, 4, 4, 4, 5, 6, 6},
	{7, 8, 9, 10, 11, 13, 14, 16},
	{18, 20, 22, 25, 28, 32, 35, 40},
	{45, 50, 56, 63, 71, 79, 89, 100},
	{112, 126, 141, 158, 178, 200, 224, 251},
	{282, 316, 355, 398, 447, 501, 562, 631},
	{708, 794, 891, 1000, 1122, 1259, 1413, 1585},
	{1778, 1995, 2239, 2512, 2818, 3162, 3548, 3981},
	{4467, 5012, 5623, 6310, 7079, 7943, 8913, 10000},
	{11220, 12589, 14125, 15849, 17783, 19953, 22387, 25119},
	{28184, 31623, 35481, 39811, 44668, 50119, 56234, 65535} };

/*Y = 10*log(X)*/
s32 odm_pwdb_conversion(s32 X, u32 total_bit, u32 decimal_bit)
{
	s32 Y, integer = 0, decimal = 0;
	u32 i;

	if (X == 0)
		X = 1; /* log2(x), x can't be 0 */

	for (i = (total_bit - 1); i > 0; i--) {
		if (X & BIT(i)) {
			integer = i;
			if (i > 0) {
				/* decimal is 0.5dB*3=1.5dB~=2dB */
				decimal = (X & BIT(i - 1)) ? 2 : 0;
			}
			break;
		}
	}

	Y = 3 * (integer - decimal_bit) + decimal; /* 10*log(x)=3*log2(x), */

	return Y;
}

s32 odm_sign_conversion(s32 value, u32 total_bit)
{
	if (value & BIT(total_bit - 1))
		value -= BIT(total_bit);

	return value;
}

void phydm_seq_sorting(void *dm_void, u32 *value, u32 *rank_idx, u32 *idx_out,
		       u8 seq_length)
{
	u8 i = 0, j = 0;
	u32 tmp_a, tmp_b;
	u32 tmp_idx_a, tmp_idx_b;

	for (i = 0; i < seq_length; i++) {
		rank_idx[i] = i;
		/**/
	}

	for (i = 0; i < (seq_length - 1); i++) {
		for (j = 0; j < (seq_length - 1 - i); j++) {
			tmp_a = value[j];
			tmp_b = value[j + 1];

			tmp_idx_a = rank_idx[j];
			tmp_idx_b = rank_idx[j + 1];

			if (tmp_a < tmp_b) {
				value[j] = tmp_b;
				value[j + 1] = tmp_a;

				rank_idx[j] = tmp_idx_b;
				rank_idx[j + 1] = tmp_idx_a;
			}
		}
	}

	for (i = 0; i < seq_length; i++) {
		idx_out[rank_idx[i]] = i + 1;
		/**/
	}
}

u32 odm_convert_to_db(u32 value)
{
	u8 i;
	u8 j;
	u32 dB;

	value = value & 0xFFFF;

	for (i = 0; i < 12; i++) {
		if (value <= db_invert_table[i][7])
			break;
	}

	if (i >= 12)
		return 96; /* maximum 96 dB */

	for (j = 0; j < 8; j++) {
		if (value <= db_invert_table[i][j])
			break;
	}

	dB = (i << 3) + j + 1;

	return dB;
}

u32 odm_convert_to_linear(u32 value)
{
	u8 i;
	u8 j;
	u32 linear;

	/* 1dB~96dB */

	value = value & 0xFF;

	i = (u8)((value - 1) >> 3);
	j = (u8)(value - 1) - (i << 3);

	linear = db_invert_table[i][j];

	return linear;
}

u16 phydm_show_fraction_num(u32 frac_val, u8 bit_num)
{
	u8 i = 0;
	u16 val = 0;
	u16 base = 5000;

	for (i = bit_num; i > 0; i--) {
		if (frac_val & BIT(i - 1))
			val += (base >> (bit_num - i));
	}
	return val;
}
