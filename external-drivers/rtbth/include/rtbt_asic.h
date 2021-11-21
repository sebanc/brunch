/*
*************************************************************************
* Ralink Technology Corporation
* 5F., No. 5, Taiyuan 1st St., Jhubei City,
* Hsinchu County 302,
* Taiwan, R.O.C.
*
* (c) Copyright 2012, Ralink Technology Corporation
*
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the                         *
* Free Software Foundation, Inc.,                                       *
* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
*                                                                       *
*************************************************************************/

#ifndef _RTBT_ASIC_H_
#define _RTBT_ASIC_H_

#include "rtbt_type.h"

//struct rtbt_dev_ctrl;
struct _RTBTH_ADAPTER; //sean wang linux

VOID RtbtReadModemRegister(
	struct _RTBTH_ADAPTER *pAd,//sean wang linux 
	UCHAR		  BbpID,
	PUCHAR   	  pValue);

VOID RtbtWriteModemRegister(
	struct _RTBTH_ADAPTER *pAd,//sean wang linux
	UCHAR		  BbpID,
	UCHAR		  Value);

VOID BthDmaCfg(
	struct _RTBTH_ADAPTER *pAd, //sean wang linux
	ULONG DmaCfgMode);

VOID BthDisableBtFunc(
	struct _RTBTH_ADAPTER *pAd);//sean wang linux

VOID BthEnableBtFunc(
	struct _RTBTH_ADAPTER *pAd);//sean wang linux

VOID BthInitializeAsic(IN 
	struct _RTBTH_ADAPTER *pAd);//sean wang linux

#endif // _RTBT_ASIC_H_ //

