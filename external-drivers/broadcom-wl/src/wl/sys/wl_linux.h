/*
 * wl_linux.c exported functions and definitions
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wl_linux.h 369548 2012-11-19 09:01:01Z $
 */

#ifndef _wl_linux_h_
#define _wl_linux_h_

#include <wlc_types.h>

typedef struct wl_timer {
	struct timer_list 	timer;
	struct wl_info 		*wl;
	void 				(*fn)(void *);
	void				*arg; 
	uint 				ms;
	bool 				periodic;
	bool 				set;
	struct wl_timer 	*next;
#ifdef BCMDBG
	char* 				name; 
	uint32				ticks;	
#endif
} wl_timer_t;

typedef struct wl_task {
	struct work_struct work;
	void *context;
} wl_task_t;

#define WL_IFTYPE_BSS	1 
#define WL_IFTYPE_WDS	2 
#define WL_IFTYPE_MON	3 

struct wl_if {
#ifdef USE_IW
	wl_iw_t		iw;		
#endif 
	struct wl_if *next;
	struct wl_info *wl;		
	struct net_device *dev;		
	struct wlc_if *wlcif;		
	uint subunit;			
	bool dev_registed;		
	int  if_type;			
	char name[IFNAMSIZ];		
	struct net_device_stats stats;  
	uint    stats_id;               
	struct net_device_stats stats_watchdog[2]; 

#ifdef USE_IW
	struct iw_statistics wstats_watchdog[2];
	struct iw_statistics wstats;
	int             phy_noise;
#endif 
};

struct rfkill_stuff {
	struct rfkill *rfkill;
	char rfkill_name[32];
	char registered;
};

struct wl_info {
	uint		unit;		
	wlc_pub_t	*pub;		
	void		*wlc;		
	osl_t		*osh;		
	struct net_device *dev;		

	struct semaphore sem;		
	spinlock_t	lock;		
	spinlock_t	isr_lock;	

	uint		bcm_bustype;	
	bool		piomode;	
	void *regsva;			
	wl_if_t *if_list;		
	atomic_t callbacks;		
	struct wl_timer *timers;	
	struct tasklet_struct tasklet;	
	struct tasklet_struct tx_tasklet; 

#if 0 && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30))
	struct napi_struct napi;
#endif 

	struct net_device *monitor_dev;	
	uint		monitor_type;	
	bool		resched;	
	uint32		pci_psstate[16];	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#define NUM_GROUP_KEYS 4
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	struct lib80211_crypto_ops *tkipmodops;
#else
	struct ieee80211_crypto_ops *tkipmodops;	
#endif
	struct ieee80211_tkip_data  *tkip_ucast_data;
	struct ieee80211_tkip_data  *tkip_bcast_data[NUM_GROUP_KEYS];
#endif 

	bool		txq_dispatched;	
	spinlock_t	txq_lock;	
	struct sk_buff	*txq_head;	
	struct sk_buff	*txq_tail;	
	int		txq_cnt;	

	wl_task_t	txq_task;	
	wl_task_t	multicast_task;	

	wl_task_t	wl_dpc_task;	
	bool		all_dispatch_mode;

#if defined(WL_CONFIG_RFKILL)
	struct rfkill_stuff wl_rfkill;
	mbool last_phyind;
#endif 

	uint processed;		
	struct proc_dir_entry *proc_entry;	
	uchar* bar1_addr;
	uint32 bar1_size;
};

#define HYBRID_PROC   "brcm_monitor"

#if defined(WL_ALL_PASSIVE_ON)
#define WL_ALL_PASSIVE_ENAB(wl)	1
#else
#define WL_ALL_PASSIVE_ENAB(wl)	(!(wl)->all_dispatch_mode)
#endif 

#define WL_LOCK(wl) \
do { \
	if (WL_ALL_PASSIVE_ENAB(wl)) \
		down(&(wl)->sem); \
	else \
		spin_lock_bh(&(wl)->lock); \
} while (0)

#define WL_UNLOCK(wl) \
do { \
	if (WL_ALL_PASSIVE_ENAB(wl)) \
		up(&(wl)->sem); \
	else \
		spin_unlock_bh(&(wl)->lock); \
} while (0)

#define WL_ISRLOCK(wl, flags) do {spin_lock(&(wl)->isr_lock); (void)(flags);} while (0)
#define WL_ISRUNLOCK(wl, flags) do {spin_unlock(&(wl)->isr_lock); (void)(flags);} while (0)

#define INT_LOCK(wl, flags)	spin_lock_irqsave(&(wl)->isr_lock, flags)
#define INT_UNLOCK(wl, flags)	spin_unlock_irqrestore(&(wl)->isr_lock, flags)

typedef struct wl_info wl_info_t;

#ifndef PCI_D0
#define PCI_D0		0
#endif

#ifndef PCI_D3hot
#define PCI_D3hot	3
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
extern irqreturn_t wl_isr(int irq, void *dev_id);
#else
extern irqreturn_t wl_isr(int irq, void *dev_id, struct pt_regs *ptregs);
#endif

extern int __devinit wl_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
extern void wl_free(wl_info_t *wl);
extern int  wl_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
extern struct net_device * wl_netdev_get(wl_info_t *wl);

#endif 
