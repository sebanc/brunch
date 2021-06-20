/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __OSDEP_LINUX_SERVICE_H_
#define __OSDEP_LINUX_SERVICE_H_

#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 5))
	#include <linux/kref.h>
#endif
/* #include <linux/smp_lock.h> */
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/skbuff.h>
#include <linux/circ_buf.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>
#include <asm/atomic.h>
#include <asm/io.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26))
	#include <asm/semaphore.h>
#else
	#include <linux/semaphore.h>
#endif
#include <linux/sem.h>
#include <linux/sched.h>
#include <linux/etherdevice.h>
#include <net/iw_handler.h>
#include <net/addrconf.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/delay.h>
#include <linux/interrupt.h>	/* for struct tasklet_struct */
#include <linux/ip.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/vmalloc.h>

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 5, 41))
	#include <linux/tqueue.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
	#include <uapi/linux/limits.h>
#else
	#include <linux/limits.h>
#endif

#ifdef RTK_DMP_PLATFORM
	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 12))
		#include <linux/pageremap.h>
	#endif
	#include <asm/io.h>
#endif

#ifdef CONFIG_NET_RADIO
	#define CONFIG_WIRELESS_EXT
#endif

/* Monitor mode */
#include <net/ieee80211_radiotap.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
	#include <linux/ieee80211.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25) && \
	 LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29))
	#define CONFIG_IEEE80211_HT_ADDT_INFO
#endif

#include <net/cfg80211.h>

#ifdef CONFIG_TCP_CSUM_OFFLOAD_TX
	#include <linux/in.h>
	#include <linux/udp.h>
#endif

#ifdef CONFIG_EFUSE_CONFIG_FILE
	#include <linux/fs.h>
#endif

#include <linux/usb.h>
#include <linux/usb/ch9.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
	#ifdef CONFIG_USB_SUSPEND
		#define CONFIG_AUTOSUSPEND	1
	#endif
#endif

#if (KERNEL_VERSION(2, 6, 29) > LINUX_VERSION_CODE && defined(CONFIG_RTW_NAPI))

	#undef CONFIG_RTW_NAPI
	/*#warning "Linux Kernel version too old to support NAPI (should newer than 2.6.29)\n"*/

#endif

#if (KERNEL_VERSION(2, 6, 33) > LINUX_VERSION_CODE)

	#warning "Linux Kernel version too old to support GRO(should newer than 2.6.33)\n"

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
	typedef struct mutex		_mutex;
#else
	typedef struct semaphore	_mutex;
#endif

struct __queue	{
	struct	list_head	queue;
	spinlock_t	lock;
};

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 41))
	typedef struct work_struct _workitem;
#else
	typedef struct tq_struct _workitem;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24))
	#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22))
/* Porting from linux kernel, for compatible with old kernel. */
static inline unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}

static inline void skb_reset_tail_pointer(struct sk_buff *skb)
{
	skb->tail = skb->data;
}

static inline void skb_set_tail_pointer(struct sk_buff *skb, const int offset)
{
	skb->tail = skb->data + offset;
}

static inline unsigned char *skb_end_pointer(const struct sk_buff *skb)
{
	return skb->end;
}
#endif

static inline struct list_head *get_next(struct list_head *list)
{
	return list->next;
}

static inline struct list_head	*get_list_head(struct __queue	*queue)
{
	return &(queue->queue);
}

static inline void _enter_critical(spinlock_t *plock, unsigned long *pirqL)
{
	spin_lock_irqsave(plock, *pirqL);
}

static inline void _exit_critical(spinlock_t *plock, unsigned long *pirqL)
{
	spin_unlock_irqrestore(plock, *pirqL);
}

static inline void _enter_critical_ex(spinlock_t *plock, unsigned long *pirqL)
{
	spin_lock_irqsave(plock, *pirqL);
}

static inline void _exit_critical_ex(spinlock_t *plock, unsigned long *pirqL)
{
	spin_unlock_irqrestore(plock, *pirqL);
}

static inline void _enter_critical_bh(spinlock_t *plock, unsigned long *pirqL)
{
	spin_lock_bh(plock);
}

static inline void _exit_critical_bh(spinlock_t *plock, unsigned long *pirqL)
{
	spin_unlock_bh(plock);
}

static inline int _enter_critical_mutex(_mutex *pmutex, unsigned long *pirqL)
{
	int ret = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
	/* mutex_lock(pmutex); */
	ret = mutex_lock_interruptible(pmutex);
#else
	ret = down_interruptible(pmutex);
#endif
	return ret;
}


static inline void _exit_critical_mutex(_mutex *pmutex, unsigned long *pirqL)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
	mutex_unlock(pmutex);
#else
	up(pmutex);
#endif
}

static inline void rtw_list_delete(struct list_head *plist)
{
	list_del_init(plist);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static inline void _init_timer(struct timer_list *ptimer, struct  net_device * nic_hdl, void *pfunc, void *cntx)
{
	/* setup_timer(ptimer, pfunc,(u32)cntx);	 */
	ptimer->function = pfunc;
	ptimer->data = (unsigned long)cntx;
	init_timer(ptimer);
}
#endif

static inline void _set_timer(struct timer_list *ptimer, u32 delay_time)
{
	mod_timer(ptimer , (jiffies + (delay_time * HZ / 1000)));
}

static inline void _cancel_timer(struct timer_list *ptimer, u8 *bcancelled)
{
	*bcancelled = del_timer_sync(ptimer) == 1 ? 1 : 0;
}

static inline void _init_workitem(_workitem *pwork, void *pfunc, void *cntx)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20))
	INIT_WORK(pwork, pfunc);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 41))
	INIT_WORK(pwork, pfunc, pwork);
#else
	INIT_TQUEUE(pwork, pfunc, pwork);
#endif
}

static inline void _set_workitem(_workitem *pwork)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 41))
	schedule_work(pwork);
#else
	schedule_task(pwork);
#endif
}

static inline void _cancel_workitem_sync(_workitem *pwork)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
	cancel_work_sync(pwork);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 41))
	flush_scheduled_work();
#else
	flush_scheduled_tasks();
#endif
}
/*
 * Global Mutex: can only be used at PASSIVE level.
 *   */

#define ACQUIRE_GLOBAL_MUTEX(_MutexCounter)                              \
	{                                                               \
		while (atomic_inc_return((atomic_t *)&(_MutexCounter)) != 1) { \
			atomic_dec((atomic_t *)&(_MutexCounter));        \
			msleep(10);                          \
		}                                                           \
	}

#define RELEASE_GLOBAL_MUTEX(_MutexCounter)                              \
	{                                                               \
		atomic_dec((atomic_t *)&(_MutexCounter));        \
	}

static inline int rtw_netif_queue_stopped(struct net_device *pnetdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	return (netif_tx_queue_stopped(netdev_get_tx_queue(pnetdev, 0)) &&
		netif_tx_queue_stopped(netdev_get_tx_queue(pnetdev, 1)) &&
		netif_tx_queue_stopped(netdev_get_tx_queue(pnetdev, 2)) &&
		netif_tx_queue_stopped(netdev_get_tx_queue(pnetdev, 3)));
#else
	return netif_queue_stopped(pnetdev);
#endif
}

static inline void rtw_netif_wake_queue(struct net_device *pnetdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	netif_tx_wake_all_queues(pnetdev);
#else
	netif_wake_queue(pnetdev);
#endif
}

static inline void rtw_netif_start_queue(struct net_device *pnetdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	netif_tx_start_all_queues(pnetdev);
#else
	netif_start_queue(pnetdev);
#endif
}

static inline void rtw_netif_stop_queue(struct net_device *pnetdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	netif_tx_stop_all_queues(pnetdev);
#else
	netif_stop_queue(pnetdev);
#endif
}
static inline void rtw_netif_device_attach(struct net_device *pnetdev)
{
	netif_device_attach(pnetdev);
}
static inline void rtw_netif_device_detach(struct net_device *pnetdev)
{
	netif_device_detach(pnetdev);
}
static inline void rtw_netif_carrier_on(struct net_device *pnetdev)
{
	netif_carrier_on(pnetdev);
}
static inline void rtw_netif_carrier_off(struct net_device *pnetdev)
{
	netif_carrier_off(pnetdev);
}

static inline int rtw_merge_string(char *dst, int dst_len, const char *src1, const char *src2)
{
	int	len = 0;
	len += snprintf(dst + len, dst_len - len, "%s", src1);
	len += snprintf(dst + len, dst_len - len, "%s", src2);

	return len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
	#define rtw_signal_process(pid, sig) kill_pid(find_vpid((pid)), (sig), 1)
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)) */
	#define rtw_signal_process(pid, sig) kill_proc((pid), (sig), 1)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)) */


/* Suspend lock prevent system from going suspend */
#ifdef CONFIG_WAKELOCK
	#include <linux/wakelock.h>
#endif

/* limitation of path length */
#define PATH_LENGTH_MAX PATH_MAX

/* Atomic integer operations */
#define ATOMIC_T atomic_t

#define rtw_netdev_priv(netdev) (((struct rtw_netdev_priv_indicator *)netdev_priv(netdev))->priv)

#define NDEV_FMT "%s"
#define NDEV_ARG(ndev) ndev->name
#define ADPT_FMT "%s"
#define ADPT_ARG(adapter) (adapter->pnetdev ? adapter->pnetdev->name : NULL)
#define FUNC_NDEV_FMT "%s(%s)"
#define FUNC_NDEV_ARG(ndev) __func__, ndev->name
#define FUNC_ADPT_FMT "%s(%s)"
#define FUNC_ADPT_ARG(adapter) __func__, (adapter->pnetdev ? adapter->pnetdev->name : NULL)

struct rtw_netdev_priv_indicator {
	void *priv;
	u32 sizeof_priv;
};
struct net_device *rtw_alloc_etherdev_with_old_priv(int sizeof_priv, void *old_priv);
extern struct net_device *rtw_alloc_etherdev(int sizeof_priv);

#define STRUCT_PACKED __attribute__ ((packed))


#endif
