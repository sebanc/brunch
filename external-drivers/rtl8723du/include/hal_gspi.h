/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __HAL_GSPI_H_
#define __HAL_GSPI_H_

#define ffaddr2deviceId(pdvobj, addr)	(pdvobj->Queue2Pipe[addr])

u8 rtw_hal_gspi_max_txoqt_free_space(struct adapter *adapt);
u8 rtw_hal_gspi_query_tx_freepage(struct adapter *adapt, u8 PageIdx, u8 RequiredPageNum);
void rtw_hal_gspi_update_tx_freepage(struct adapter *adapt, u8 PageIdx, u8 RequiredPageNum);
void rtw_hal_set_gspi_tx_max_length(struct adapter * adapt, u8 numHQ, u8 numNQ, u8 numLQ, u8 numPubQ);
u32 rtw_hal_get_gspi_tx_max_length(struct adapter * adapt, u8 queue_idx);

#endif
