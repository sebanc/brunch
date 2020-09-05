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

#ifndef __RTBT_HAL_H
#define __RTBT_HAL_H


#include "rtbt_ctrl.h"


VOID BthUserCfgInit(PRTBTH_ADAPTER pAd);

int BthInitialize(RTBTH_ADAPTER *pAd);

INT rtbt_hps_close(void *pDevCtrl);
INT rtbt_hps_open(void *pDevCtrl);

INT rtbt_hps_suspend(void *pDevCtrl);
INT rtbt_hps_resume(void *pDevCtrl);

int rtbt_dev_hw_init(void *dev_ctrl);
int rtbt_dev_hw_deinit(void *dev_ctrl);
	
int rtbt_dev_resource_init(struct rtbt_os_ctrl *);
int rtbt_dev_resource_deinit(struct rtbt_os_ctrl *);

int rtbt_dev_ctrl_init(struct rtbt_os_ctrl **pDevCtrl, void *csr);
int rtbt_dev_ctrl_deinit(struct rtbt_os_ctrl *);


NTSTATUS RtbtHalCancelHCIReceiveACLData(struct _RTBTH_ADAPTER *pAd);//sean wang linux
NTSTATUS RtbtHalCancelHCIGetEvent(struct _RTBTH_ADAPTER *pAd);//sean wang linux
NTSTATUS RtbtHalCancelHCIReceiveSCOData(struct _RTBTH_ADAPTER *pAd);//sean wang linux

int rtbt_hal_HCI_cmd_handle(
	IN void *dev_ctrl,
	IN PVOID pBuffer,
	IN ULONG length);

int rtbt_hal_hci_sco_task(PVOID context);
int rtbt_hal_HCI_event_Task(PVOID context);
int rtbt_hal_HCI_Acl_Task(PVOID context);
	
int RtbtHalHCISendACLData(
	IN void *dev_ctrl,
	IN PVOID pBuffer,
	IN ULONG length);

NTSTATUS RtbtHalHCISendSCOData(
	IN void *dev_ctrl,
	IN PVOID pBuffer,
	IN ULONG length);
	
VOID hps_sco_traffic_notification(IN RTBTH_ADAPTER *pAd);

#endif // __RTBT_HAL_H //
