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

#ifndef __RTBTH_PCI_H
#define __RTBTH_PCI_H


#include "rtbt_type.h"

struct rtbt_dev_entry;

void RT_PCI_IO_READ32(ULONG addr, UINT32 *val);
void RT_PCI_IO_WRITE32(ULONG addr, UINT32 val);

int RtmpOSIRQRequest(IN void *if_dev, void *dev_id);
int RtmpOSIRQRelease(IN void *if_dev, void *dev_id);

int rtbt_dma_mem_alloc(
	IN void *if_dev,
	IN unsigned long Length,
	IN unsigned char Cached,
	OUT void **VirtualAddress,
	OUT	PHYSICAL_ADDRESS *PhysicalAddress);

void rtbt_dma_mem_free(
	IN void *if_dev,
	IN unsigned long Length,
	IN void *VirtualAddress,
	IN PHYSICAL_ADDRESS PhysicalAddress);

int rtbt_iface_pci_hook(
	struct rtbt_dev_entry *pIdTb);

int rtbt_iface_pci_unhook(
	struct rtbt_dev_entry *pIdTb);

#endif // __RTBTH_PCI_H //
