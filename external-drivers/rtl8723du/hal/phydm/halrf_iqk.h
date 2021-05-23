/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef	__PHYDMIQK_H__
#define __PHYDMIQK_H__

/*--------------------------Define Parameters-------------------------------*/
#define	LOK_delay 1
#define	WBIQK_delay 10
#define	TX_IQK 0
#define	RX_IQK 1
#define	TXIQK 0
#define	RXIQK1 1
#define	RXIQK2 2
#define kcount_limit_80m 2
#define kcount_limit_others 4
#define rxiqk_gs_limit 4

#define	NUM 4
/*---------------------------End Define Parameters-------------------------------*/

struct _IQK_INFORMATION {
	bool		LOK_fail[NUM];
	bool		IQK_fail[2][NUM];
	u32		iqc_matrix[2][NUM];
	u8      iqk_times;
	u32		rf_reg18;
	u32		lna_idx;
	u8		rxiqk_step;
	u8		tmp1bcc;
	u8		kcount;
	u8		rfk_ing; /*bit0:IQKing, bit1:LCKing, bit2:DPKing*/
	bool rfk_forbidden;	
};

#endif
