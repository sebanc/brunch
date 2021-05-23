/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright(c) 2018-2019  Realtek Corporation
 */

#ifndef __RTW_SAR_H_
#define __RTW_SAR_H_

bool rtw_sar_load_table(struct rtw_dev *rtwdev);
void rtw_sar_work(struct work_struct *work);
void rtw_sar_dump_via_debugfs(struct rtw_dev *rtwdev, struct seq_file *m);

#define RTW_SAR_DELAY_TIME	(10 * HZ)

#endif
