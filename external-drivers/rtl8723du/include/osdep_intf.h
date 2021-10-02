/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */


#ifndef __OSDEP_INTF_H_
#define __OSDEP_INTF_H_


struct intf_priv {

	u8 *intf_dev;
	u32	max_iosz;	/* USB2.0: 128, USB1.1: 64, SDIO:64 */
	u32	max_xmitsz; /* USB2.0: unlimited, SDIO:512 */
	u32	max_recvsz; /* USB2.0: unlimited, SDIO:512 */

	volatile u8 *io_rwmem;
	volatile u8 *allocated_io_rwmem;
	u32	io_wsz; /* unit: 4bytes */
	u32	io_rsz;/* unit: 4bytes */
	u8 intf_status;

	void (*_bus_io)(u8 *priv);

	/*
	Under Sync. IRP (SDIO/USB)
	A protection mechanism is necessary for the io_rwmem(read/write protocol)

	Under Async. IRP (SDIO/USB)
	The protection mechanism is through the pending queue.
	*/

	_mutex ioctl_mutex;

	/* when in USB, IO is through interrupt in/out endpoints */
	struct usb_device	*udev;
	struct urb *	piorw_urb;
	u8 io_irp_cnt;
	u8 bio_irp_pending;
	struct semaphore io_retevt;
	struct timer_list	io_timer;
	u8 bio_irp_timeout;
	u8 bio_timer_cancel;
};

#ifdef CONFIG_R871X_TEST
	int rtw_start_pseudo_adhoc(struct adapter *adapt);
	int rtw_stop_pseudo_adhoc(struct adapter *adapt);
#endif

struct dvobj_priv *devobj_init(void);
void devobj_deinit(struct dvobj_priv *pdvobj);

u8 rtw_init_drv_sw(struct adapter *adapt);
u8 rtw_free_drv_sw(struct adapter *adapt);
u8 rtw_reset_drv_sw(struct adapter *adapt);
void rtw_dev_unload(struct adapter * adapt);

u32 rtw_start_drv_threads(struct adapter *adapt);
void rtw_stop_drv_threads(struct adapter *adapt);
void rtw_cancel_all_timer(struct adapter *adapt);

uint loadparam(struct adapter *adapter);

int rtw_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

int rtw_init_netdev_name(struct net_device *pnetdev, const char *ifname);
struct net_device *rtw_init_netdev(struct adapter *adapt);

void rtw_os_ndev_free(struct adapter *adapter);
int rtw_os_ndev_init(struct adapter *adapter, const char *name);
void rtw_os_ndev_deinit(struct adapter *adapter);
void rtw_os_ndev_unregister(struct adapter *adapter);
void rtw_os_ndevs_unregister(struct dvobj_priv *dvobj);
int rtw_os_ndevs_init(struct dvobj_priv *dvobj);
void rtw_os_ndevs_deinit(struct dvobj_priv *dvobj);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
u16 rtw_recv_select_queue(struct sk_buff *skb);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35) */

int rtw_ndev_notifier_register(void);
void rtw_ndev_notifier_unregister(void);
void rtw_inetaddr_notifier_register(void);
void rtw_inetaddr_notifier_unregister(void);

#include "../os_dep/rtw_proc.h"

#include "../os_dep/ioctl_cfg80211.h"

u8 rtw_rtnl_lock_needed(struct dvobj_priv *dvobj);
void rtw_set_rtnl_lock_holder(struct dvobj_priv *dvobj, void * thd_hdl);


void rtw_ips_dev_unload(struct adapter *adapt);

int rtw_ips_pwr_up(struct adapter *adapt);
void rtw_ips_pwr_down(struct adapter *adapt);

#ifdef CONFIG_CONCURRENT_MODE
struct _io_ops;
struct dvobj_priv;
struct adapter *rtw_drv_add_vir_if(struct adapter *primary_adapt, void (*set_intf_ops)(struct adapter *primary_adapt, struct _io_ops *pops));
void rtw_drv_stop_vir_ifaces(struct dvobj_priv *dvobj);
void rtw_drv_free_vir_ifaces(struct dvobj_priv *dvobj);
#endif

void rtw_ndev_destructor(struct  net_device * ndev);
#ifdef CONFIG_ARP_KEEP_ALIVE
int rtw_gw_addr_query(struct adapter *adapt);
#endif

int rtw_suspend_common(struct adapter *adapt);
int rtw_resume_common(struct adapter *adapt);

#endif /* _OSDEP_INTF_H_ */
