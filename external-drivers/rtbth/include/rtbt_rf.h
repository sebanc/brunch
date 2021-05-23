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

#ifndef _RTBT_RF_H_
#define _RTBT_RF_H_

#include "rtbt_type.h"

struct _RTBTH_ADAPTER;

NTSTATUS BthWriteRFRegister(
	IN struct _RTBTH_ADAPTER	*pAd,
	IN UCHAR			RfID,
	IN UCHAR			Value);

NTSTATUS BthReadRFRegister(
	IN struct _RTBTH_ADAPTER	*pAd,
	IN UCHAR			RfID,
	IN PUCHAR			pValue);

#endif // _RTBT_RF_H_ //

