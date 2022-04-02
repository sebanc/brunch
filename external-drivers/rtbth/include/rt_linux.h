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

#ifndef __RTBT_LINUX_H
#define __RTBT_LINUX_H

#define KTHREAD_SUPPORT

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <asm/delay.h>		// for udelay()
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/capability.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#endif
#ifdef KTHREAD_SUPPORT
#include <linux/kthread.h>
#endif

#include "rtbt_type.h"

#include "trace.h"
#include "rtbth_pci.h"

#include "rtbt_osabl.h"



/***********************************************************************************
 *	OS semaphore/event related data structure and definitions
 *  For the eventing subsystem, we can seperate the operations as following parts
 *		1. Initialization
 *		2. Set event as signaled state
 *		3. Get event and handle it then clear it as un-signaled state
 *		4. Destroy of the event instance
 ***********************************************************************************/
typedef struct semaphore	RTMP_OS_SEM;


#define RTMP_SEM_EVENT_INIT_LOCKED(_pSema) 	sema_init((_pSema), 0)
#define RTMP_SEM_EVENT_INIT(_pSema)			sema_init((_pSema), 1)
#define RTMP_SEM_EVENT_DESTORY(_pSema)		do{}while(0)
#define RTMP_SEM_EVENT_WAIT(_pSema, _status)	((_status) = down_interruptible((_pSema)))
#define RTMP_SEM_EVENT_UP(_pSema)			up(_pSema)

#ifdef KTHREAD_SUPPORT
#define RTMP_WAIT_EVENT_INTERRUPTIBLE(_pAd, _pTask) \
{ \
		wait_event_interruptible(_pTask->kthread_q, \
								 _pTask->kthread_running || kthread_should_stop()); \
		_pTask->kthread_running = FALSE; \
		if (kthread_should_stop()) \
		{ \
			RTMP_SET_FLAG(_pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS); \
			break; \
		} \
}
#endif

#ifdef KTHREAD_SUPPORT
#define WAKE_UP(_pTask) \
	do{ \
		if ((_pTask)->kthread_task) \
        { \
			(_pTask)->kthread_running = TRUE; \
	        wake_up(&(_pTask)->kthread_q); \
		} \
	}while(0)
#endif



/***********************************************************************************
 *	OS task related data structure and definitions
 ***********************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
/* definitions for newest kernel version */
typedef	struct pid *	RTMP_OS_PID;
typedef	struct pid *	THREAD_PID;

#define	THREAD_PID_INIT_VALUE	NULL

// TODO: Use this IOCTL carefully when linux kernel version larger than 2.6.27, because the PID only correct when the user space task do this ioctl itself.
//#define RTMP_GET_OS_PID(_x, _y)    _x = get_task_pid(current, PIDTYPE_PID);
#define RTMP_GET_OS_PID(_x, _y)		do{rcu_read_lock(); _x=current->pids[PIDTYPE_PID].pid; rcu_read_unlock();}while(0)
#define	GET_PID_NUMBER(_v)	pid_nr((_v))
#define IS_VALID_PID(_pid)	(pid_nr((_pid)) > 0)
#define KILL_THREAD_PID(_A, _B, _C)	kill_pid((_A), (_B), (_C))

#else
/* definitions for older kernel version */
typedef pid_t RTMP_OS_PID;
typedef	pid_t THREAD_PID;

#define	THREAD_PID_INIT_VALUE	-1
#define RTMP_GET_OS_PID(_x, _pid)		_x = _pid
#define	GET_PID_NUMBER(_v)	(_v)
#define IS_VALID_PID(_pid)	((_pid) >= 0)
#define KILL_THREAD_PID(_A, _B, _C)	kill_proc((_A), (_B), (_C))

#endif // LINUX_VERSION_CODE //


#define RTBT_OS_MGMT_TASK_FLAGS	CLONE_VM

#define RTBT_TASK_CAN_DO_INSERT		(RTBT_TASK_STAT_INITED |RTBT_TASK_STAT_RUNNING)

typedef enum{
	RTBT_TASK_STAT_UNKNOWN = 0,
	RTBT_TASK_STAT_INITED = 1,
	RTBT_TASK_STAT_RUNNING = 2,
	RTBT_TASK_STAT_STOPED = 4,
}RTBT_TASK_STATUS;


#define RTBT_OS_TASK_NAME_LEN	16


typedef struct _RTBT_OS_TASK_
{
#ifdef KTHREAD_SUPPORT
	struct task_struct 	*kthread_task;
	wait_queue_head_t	 kthread_q;
	BOOLEAN	 kthread_running;
#else
	struct semaphore taskSema;
	struct completion	taskComplete;
	RTMP_OS_PID	 taskPID;
#endif

	char taskName[RTBT_OS_TASK_NAME_LEN];
	void *priv;
	RTBT_TASK_STATUS taskStatus;
	RTBT_OS_TASK_CALLBACK taskEntry;
	unsigned char			task_killed;
	int refCnt;
}RTBT_OS_TASK;



/***********************************************************************************
 *	OS file operation related data structure definitions
 ***********************************************************************************/
typedef struct file* RTMP_OS_FD;

typedef struct _RTMP_OS_FS_INFO_
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	kuid_t			fsuid;
	kgid_t			fsgid;
#else
	int				fsuid;
	int				fsgid;
#endif
	mm_segment_t	fs;
}RTMP_OS_FS_INFO;

#define IS_FILE_OPEN_ERR(_fd) 	IS_ERR((_fd))



typedef struct _RT_OS_LOCK{


}RT_OS_LOCK;


typedef struct _RT_OS_THREAD{


}RT_OS_THREAD;


typedef struct _RT_OS_EVENT{

}RT_OS_EVENT;


typedef struct _RT_OS_TASK{

}RT_OS_TASK;


typedef struct _RT_OS_FILE{

}RT_OS_FILE;

#define IRQ_HANDLE_TYPE  irqreturn_t

#endif // __RTBT_LINUX_H //

