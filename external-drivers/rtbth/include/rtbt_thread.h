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

#ifndef _RTBT_THREAD_H_
#define _RTBT_THREAD_H_

#include "rtbt_type.h"

//struct rtbt_dev_ctrl; //sean wang linux
struct _RTBTH_ADAPTER; //sean wang linux

// TODO: shiang, it's little weired to put this marco here!
#define DECLARE_TIMER_FUNCTION(_func)			\
	void rtmp_timer_##_func(unsigned long data)

#define GET_TIMER_FUNCTION(_func)				\
	(ULONG)rtmp_timer_##_func
	
#define BUILD_TIMER_FUNCTION(_func)	 \
void rtmp_timer_##_func(unsigned long data)			\
{												\
	KTIMER *_timer = (KTIMER *)data;				\
	KDPC *_dpc;\
	if (_timer) { \
		_dpc = &_timer->dpc;	\
		if(_dpc->func)	{\
			_dpc->func(NULL, (PVOID)(_dpc->data), NULL, _timer); 	\
			return;\
		}\
	}\
	else \
		DebugPrint(ERROR, DBG_MISC, "%s():the timer function is NULL!\n", __FUNCTION__);\
}


DECLARE_TIMER_FUNCTION(RtbtCoreIdleTimerFunc);

VOID RtbtCoreIdleTimerFunc(
    PKDPC    SystemSpecific1,
    PVOID    FunctionContext,
    PVOID    SystemSpecific2,
    PVOID    SystemSpecific3);

VOID RtbtPostCoreEvent(struct _RTBTH_ADAPTER *pAd);//sean wang linux

NTSTATUS RtbtStartCore(struct _RTBTH_ADAPTER *pAd);//sean wang linux
VOID RtbtStopCore(struct _RTBTH_ADAPTER *pAd);//sean wang linux
VOID RtbtTerminateCoreThread(struct _RTBTH_ADAPTER *pAd); //sean wang linux

#endif // _RTBT_THREAD_H_ //

