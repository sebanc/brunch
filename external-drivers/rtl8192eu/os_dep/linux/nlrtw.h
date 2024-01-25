/******************************************************************************
 *
 * Copyright(c) 2007 - 2020 Realtek Corporation.
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
#ifndef __RTW_NLRTW_H_
#define __RTW_NLRTW_H_

#ifdef CONFIG_RTW_NLRTW
int rtw_nlrtw_init(void);
int rtw_nlrtw_deinit(void);
int rtw_nlrtw_ch_util_rpt(_adapter *adapter, u8 n_rpts, u8 *val, u8 **mac_addr);
#else
static inline int rtw_nlrtw_init(void) {return _FAIL;}
static inline int rtw_nlrtw_deinit(void) {return _FAIL;}
static inline int rtw_nlrtw_ch_util_rpt(_adapter *adapter, u8 n_rpts, u8 *val, u8 **mac_addr) {return _FAIL;}
#endif

#endif /* __RTW_NLRTW_H_ */
