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

#ifndef __RTBT_OSABL_H
#define __RTBT_OSABL_H

#include "rtbt_type.h"

#ifdef RTBT_IFACE_PCI
#include "rtbth_pci.h"
#endif // RTBT_IFACE_PCI //

#define RTBT_HOST_DEV_NAME_LEN	16

#define RTBT_FIRMWARE_PATH "/etc/rtbt/rt3298.bin"


/*
	Lock related data structures and functions
*/
// TODO:Shiang, change it to more generic definitions

#define NT_SUCCESS(_status)		(_status == STATUS_SUCCESS)
#define Executive 0
#define KernelMode 0

typedef enum {
	RES_INVALID = 0,
	RES_VALID = 1,
	RES_INUSE = 2,
}RES_STATE;

typedef unsigned long KIRQL;	// work-around to fix windows definitions
typedef LONG KPRIORITY;

typedef enum _RT_LOCK_LEVEL{
	RT_SIRQL = 0x0,
	RT_HIRQL = 0x80000000,
}RT_LOCK_LEVEL;


typedef struct _KSPIN_LOCK{
#ifdef OSABL_DBG
	char spinID[16];
#endif // OSABL_DBG //

	void *pOSLock;
	int ref_cnt;
	RES_STATE state;
}KSPIN_LOCK, *PKSPIN_LOCK;


/*
	usage of event queue
	1. Initialization
		init_waitqueue_head(&self->query_wait);
	2. Waiting for somthing
		if (wait_event_interruptible(self->query_wait, (self->iriap==NULL)))
	3. wake up the waiting queue
		wake_up_interruptible(&self->query_wait);
*/
typedef enum{
	SynchronizationEvent = 0,
	NotificationEvent = 1,
}EVENT_TYPE;

typedef enum {
	EVENT_LOCKED = 0,
	EVENT_UNLOCKED = 1,
}EVENT_STATE;

typedef struct _KEVENT{
#ifdef OSABL_DBG
	char eventID[16];
#endif // OSABL_DBG //
	void *pOSEvent;
	int refCnt;
}KEVENT, *PKEVENT;


struct _KDPC;
typedef void (*PKDEFERRED_ROUTINE)(struct _KDPC *, PVOID, PVOID, PVOID);
typedef struct _KDPC{
	PKDEFERRED_ROUTINE func;
	ULONG data;
}KDPC, *PKDPC;

typedef struct _KTIMER{
#ifdef OSABL_DBG
	char timerID[16];
#endif // OSABL_DBG //
	void *pOSTimer;
	void *dev_ctrl;
	KDPC dpc;
	KSPIN_LOCK lock;
	int valid;
	int refCnt;
	RES_STATE state;
}KTIMER, *PKTIMER;




/*
	TASK related functions and data structures
*/
typedef int (*RTBT_OS_TASK_CALLBACK)(void *);

typedef struct _PKTHREAD{
#ifdef OSABL_DBG
	char threadID[16];
#endif // OSABL_DBG //

	void *pOSThread;
	int refCnt;
}KTHREAD, *PKTHREAD;


/*
	file operation related functions
*/
typedef enum{
	RTBT_FOP_READ = 1,
}RTBT_FILE_OP_MODE;

typedef struct _RTBT_FILE{
#ifdef OSABL_DBG
	char fileID[16];
#endif // OSABL_DBG //

	void *pOSFile;
	int fileSize;
	int refCnt;
}RAL_FILE;


/*******************************************************************************
	Memory related data structures and marcos/functions definitions
 *******************************************************************************/
typedef enum{
	RAL_MEM_NORMAL = 1,
	RAL_MEM_CONTINUOS = 2,
	RAL_MEM_NONCACHE = 4,
	RAL_MEM_PHYSICAL = 8,
}RAL_MEM_FLAG;

#define RAL_MEM_DMA 	(RAL_MEM_CONTINUOS | RAL_MEM_NONCACHE)


typedef enum{
	RAL_INF_INVAL = 0,
	RAL_INF_PCI = 1,
	RAL_INF_USB = 2,
}RAL_DEV_INF_TYPE;


typedef enum{
	RAL_DEV_INVAL = 0,
	RAL_DEV_BT = 1,
	RAL_DEV_NET = 2,
}RAL_DEV_FUNC_TYPE;


struct ral_dev_id{
	unsigned short vid;
	unsigned short pid;
};


struct rtbt_dev_entry{
	struct rtbt_dev_entry *next;
	RAL_DEV_INF_TYPE	infType;
	RAL_DEV_FUNC_TYPE	devType;
	struct ral_dev_id		*pDevIDList;
	void *dev_ops;
	void *os_private;
	int mlen;
};


struct rtbt_os_ctrl;
struct rtbt_dev_ops{
	int (*dev_ctrl_init)(struct rtbt_os_ctrl **, void*);
	int (*dev_ctrl_deinit)(struct rtbt_os_ctrl *);
	int (*dev_resource_init)(struct rtbt_os_ctrl *);
	int (*dev_resource_deinit)(struct rtbt_os_ctrl *);
	int (*dev_hw_init)(void *);
	int (*dev_hw_deinit)(void *);
};

struct rtbt_hps_ops{
	int (*open)(void *handle);
	int (*close)(void *handle);

    int (*suspend)(void *handle);
    int (*resume)(void *handle);

    int (*recv)(void *bt_dev, int type, char *buf, int len);
	int (*hci_cmd)(void *dev_ctrl, void *buf, ULONG len);
	int (*hci_acl_data)(void *dev_ctrl, void *buf, ULONG len);
	int (*hci_sco_data)(void *dev_ctrl, void *buf, ULONG len);
	int (*flush)(void *handle);
	int (*ioctl)(void *handle, unsigned int cmd, unsigned long arg);
	void (*destruct)(void *handle);
	void (*notify)(void *handle, unsigned int evt);
};

union rtbt_if_ops{
	struct rtbt_pci_ops{
		int (*isr)(void *);
		void *csr_addr;
	}pci_ops;
};

struct rtbt_os_ctrl{
	void *dev_ctrl;
	void *if_dev;
	void *bt_dev;

	union rtbt_if_ops if_ops;
	struct rtbt_dev_ops *dev_ops;
	struct rtbt_hps_ops *hps_ops;

	int sco_tx_seq;
	unsigned long sco_time_hci;
};


/*******************************************************************************
	Memory related functions
 *******************************************************************************/
void RtlCopyMemory(
	IN VOID *Destination,
	IN const VOID *Source,
	IN int Length);


VOID RtlZeroMemory(
 IN VOID *Destination,
 IN int Length);


VOID RtlFillMemory(
	IN VOID *Destination,
	IN int c,
	IN int Length);

#define bt_memset(_ptr, _n, _size)	RtlFillMemory(_ptr, _n, _size)

int RtlCompareMemory(
	IN VOID *s1,
	IN VOID *s2,
	IN int size);

int RtlStringCchPrintfA(
	IN char *str,
	IN int len,
	IN const char *format,
	...);

ULONG KeQuerySystemTime(LARGE_INTEGER *timeval);

void *ral_mem_alloc(int size, RAL_MEM_FLAG memFlag);
int ral_mem_free(void *ptr);

void *ral_mem_valloc(int size);
int ral_mem_vfree(void *ptr);


/*******************************************************************************
	timer/delay related functions
 *******************************************************************************/
void KeStallExecutionProcessor(IN ULONG usec);

VOID rtbt_usec_delay(IN ULONG usec);
#define RtbtusecDelay(_usec)		rtbt_usec_delay(_usec)

BOOLEAN KeSetTimer(
	IN KTIMER *os_timer,
	IN LARGE_INTEGER msec,
	IN PKDPC dpc);

BOOLEAN KeCancelTimer(
	IN KTIMER *os_timer);

INT KeFreeTimer(
	IN KTIMER *os_timer);

INT ral_timer_init(
	IN KTIMER *os_timer,
	IN ULONG func);

VOID KeInitializeDpc(
	OUT PKDPC dpc,
	IN PKDEFERRED_ROUTINE func,
	IN  PVOID data);


/*******************************************************************************
	File related functions.
 *******************************************************************************/
int ral_file_obj_init(
	IN PSTRING FileName,
	IN RAL_FILE **pFileHd);

int ral_file_open(RAL_FILE *pFileHd, RTBT_FILE_OP_MODE opMode);
int ral_file_read(RAL_FILE *pFileHd, void *pBuf, int len);
int ral_file_write(RAL_FILE *pFileHd, void *pBuf, int len);
int ral_file_close(RAL_FILE *pFileHd);
int ral_file_obj_deinit(RAL_FILE *pFileHd);


/*******************************************************************************
	synchronization/lock/semaphore related functions
 *******************************************************************************/
int ral_spin_lock(KSPIN_LOCK *pLock, unsigned long *irqFlag);
int ral_spin_unlock(KSPIN_LOCK *pLock, unsigned long irqFlag);
int ral_spin_init(KSPIN_LOCK *pLock);
int ral_spin_deinit(KSPIN_LOCK *pLock);

int KeAcquireSpinLockAtDpcLevel(KSPIN_LOCK *pLock);
int KeReleaseSpinLockFromDpcLevel(KSPIN_LOCK *pLock);


LONG KeSetEvent(
	INOUT KEVENT *Event,
	IN KPRIORITY Increment,
	IN BOOLEAN Wait);

NTSTATUS KeWaitForSingleObject(
	IN KEVENT *Object,
	IN int WaitReason,
	IN int WaitMode,
	IN BOOLEAN Alertable,
	IN PLARGE_INTEGER Timeout);

NTSTATUS KeInitializeEvent(
	IN KEVENT *pEventObj,
	IN int Type,
	IN EVENT_STATE State);

NTSTATUS KeDestoryEvent(
	IN KEVENT *pEventObj);


/*******************************************************************************
	task related declartions
 *******************************************************************************/
 int ral_task_init(
	IN KTHREAD *pTask,
	IN PSTRING	pTaskName,
	IN VOID		*pPriv);

int ral_task_deinit(
	IN KTHREAD *pTask);

int ral_task_attach(
	IN KTHREAD *pTask,
	IN RTBT_OS_TASK_CALLBACK fn,
	IN ULONG arg);

void ral_task_customize(IN KTHREAD *pTask);
int ral_task_notify_exit(IN KTHREAD *pTask);
NDIS_STATUS ral_task_kill(IN KTHREAD *pTask);

/*
	Debug related functions
*/
VOID DebugPrint(
    __in ULONG   level,
    __in ULONG   flag,
    __in PUCHAR  msg,
    ...
    );

VOID Bth_Dbg_DumpBuffer(
	IN PVOID	Buffer,
	IN LONG		Length,
	IN ULONG	Offset,
	IN PVOID	pOpaque,
	IN VOID 	(*pOutPutFn)(UINT32, const CHAR *, const CHAR *, PVOID));

int hex_dump(char *title, char *buf, int len);


/*******************************************************************************
	System hooking functions
 *******************************************************************************/
int ral_os_register(struct rtbt_dev_entry *devlist);
int ral_os_unregister(struct rtbt_dev_entry *devlist);

#endif // __RTBT_OSABL_H //
