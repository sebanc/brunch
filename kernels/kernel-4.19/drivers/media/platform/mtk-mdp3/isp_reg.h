/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __ISP_REG_H__
#define __ISP_REG_H__

enum ISP_DIP_CQ {
	ISP_DRV_DIP_CQ_THRE0 = 0,
	ISP_DRV_DIP_CQ_THRE1,
	ISP_DRV_DIP_CQ_THRE2,
	ISP_DRV_DIP_CQ_THRE3,
	ISP_DRV_DIP_CQ_THRE4,
	ISP_DRV_DIP_CQ_THRE5,
	ISP_DRV_DIP_CQ_THRE6,
	ISP_DRV_DIP_CQ_THRE7,
	ISP_DRV_DIP_CQ_THRE8,
	ISP_DRV_DIP_CQ_THRE9,
	ISP_DRV_DIP_CQ_THRE10,
	ISP_DRV_DIP_CQ_THRE11,
	ISP_DRV_DIP_CQ_NUM,
	ISP_DRV_DIP_CQ_NONE,
	/* we only need 12 CQ threads in this chip,
	 *so we move the following enum behind ISP_DRV_DIP_CQ_NUM
	 */
	ISP_DRV_DIP_CQ_THRE12,
	ISP_DRV_DIP_CQ_THRE13,
	ISP_DRV_DIP_CQ_THRE14,
	ISP_DRV_DIP_CQ_THRE15,	/* CQ_THREAD15 does not connect to GCE */
	ISP_DRV_DIP_CQ_THRE16,	/* CQ_THREAD16 does not connect to GCE */
	ISP_DRV_DIP_CQ_THRE17,	/* CQ_THREAD17 does not connect to GCE */
	ISP_DRV_DIP_CQ_THRE18,	/* CQ_THREAD18 does not connect to GCE */
};

#endif  // __ISP_REG_H__
