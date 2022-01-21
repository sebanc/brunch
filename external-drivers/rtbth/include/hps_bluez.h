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

#ifndef __HPS_BLUEZ_H
#define __HPS_BLUEZ_H


#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include "rt_linux.h"

// BT_WARN macro is missing on older kernels
#ifndef BT_WARN
  #define BT_WARN BT_INFO
#endif // BT_WARN //

// enable hci_skb_pkt_type macro for older kernels
#ifndef hci_skb_pkt_type
  #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
    #define hci_skb_pkt_type(skb) bt_cb((skb))->pkt_type
  #else
    #define hci_skb_pkt_type(skb) (skb)->pkt_type
  #endif
#endif // hci_skb_pkt_type //

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
  // define hci_set/get_drvdata macro for older kernels
  #define hci_set_drvdata(hdev, os_ctrl) (hdev)->driver_data = (os_ctrl)
  #define hci_get_drvdata(hdev) (hdev)->driver_data
  // define rtbt_dev_hold and rtbt_dev_put
  #define rtbt_dev_hold(hdev) __hci_dev_hold((hdev))
  #define rtbt_dev_put(hdev) __hci_dev_put((hdev))
#else
  #define rtbt_dev_hold(hdev) hci_dev_hold((hdev))
  #define rtbt_dev_put(hdev) hci_dev_put((hdev))
#endif

struct rtbt_os_ctrl;

int rtbt_hps_iface_init(int if_type, void *if_dev, struct rtbt_os_ctrl *ctrl);
int rtbt_hps_iface_deinit(int if_type, void *if_dev, struct rtbt_os_ctrl *os_ctrl);

int rtbt_hps_iface_suspend(IN struct rtbt_os_ctrl *os_ctrl);
int rtbt_hps_iface_resume(IN struct rtbt_os_ctrl *os_ctrl);

int rtbt_hps_iface_attach(struct rtbt_os_ctrl *os_ctrl);
int rtbt_hps_iface_detach(struct rtbt_os_ctrl *os_ctrl);


#endif // __HPS_BLUEZ_H //

