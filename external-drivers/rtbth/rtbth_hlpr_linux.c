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

#include "include/rtbt_osabl.h"
#include "include/rt_linux.h"

static DEFINE_SPINLOCK(g_reslock);

/*******************************************************************************

	Message dump/printing related functions.

 *******************************************************************************/
int RtlStringCchPrintfA(
	IN char *str,
	IN int len,
	IN const char *format,
	...)
{
	va_list args;
	int i;

	va_start(args, format);
	i = vsnprintf(str, len, format, args);
	va_end(args);

	return i;
}


/*******************************************************************************

	Memory operation related functions.

 *******************************************************************************/

void *ral_mem_valloc(int size)
{
	return vmalloc(size);
}


int ral_mem_vfree(void *ptr)
{
	vfree(ptr);
	return TRUE;
}

void *ral_mem_alloc(int size, RAL_MEM_FLAG memFlag)
{
	void *pMemPtr = NULL;

	if (memFlag == RAL_MEM_NORMAL)
		pMemPtr = kmalloc(size, GFP_KERNEL);

	return pMemPtr;
}


int ral_mem_free(void *ptr)
{
	kfree(ptr);
	return TRUE;
}


void RtlCopyMemory(
	IN VOID *Destination,
	IN const VOID *Source,
	IN int Length)
{
	memcpy(Destination, Source, Length);
}

VOID RtlZeroMemory(
	IN VOID *Destination,
	IN int Length)
{
	memset(Destination, 0, Length);
}


VOID RtlFillMemory(
	IN VOID *Destination,
	IN int c,
	IN int Length)
{
	memset(Destination, c, Length);
}


int RtlCompareMemory(
	IN VOID *s1,
	IN VOID *s2,
	IN int size)
{
	return memcmp(s1, s2, size);
}


/*******************************************************************************

	Lock/Synchronization related functions.

 *******************************************************************************/
int ral_spin_lock(KSPIN_LOCK *pLock, unsigned long *irq_flag)
{
	unsigned long local_flag;

	if (!pLock->pOSLock) {
		printk("Error, invalid lock structure!\n");
		dump_stack();
		return FALSE;
	}

	spin_lock_irqsave(&g_reslock, local_flag);
	if (pLock->state == RES_INVALID) {
		spin_unlock_irqrestore(&g_reslock, local_flag);
		printk("Error, try to lock a invalid structure!\n");
		return FALSE;
	}
	else {
		pLock->ref_cnt++;
		pLock->state = RES_INUSE;
	}
	spin_unlock_irqrestore(&g_reslock, local_flag);

	spin_lock_irqsave((spinlock_t *)pLock->pOSLock, *irq_flag);

	return TRUE;
}


int ral_spin_unlock(KSPIN_LOCK *pLock, unsigned long irq_flag)
{
	unsigned long local_flag;

	spin_unlock_irqrestore((spinlock_t *)pLock->pOSLock, irq_flag);

	spin_lock_irqsave(&g_reslock, local_flag);
	if (pLock->ref_cnt == 0) {
		printk("Error, unlock a not acquired spin!\n");
		dump_stack();
	}
	pLock->ref_cnt --;
	if (pLock->state == RES_INUSE && pLock->ref_cnt == 0)
		pLock->state = RES_VALID;
	spin_unlock_irqrestore(&g_reslock, local_flag);

	return TRUE;
}


int ral_spin_init(KSPIN_LOCK *pLock)
{
	int flags;
	spinlock_t *pSpinLock;
	unsigned long irq_flags;

	if (in_interrupt())
		flags = GFP_ATOMIC;
	else
		flags = GFP_KERNEL;

	memset(pLock, 0, sizeof(KSPIN_LOCK));
	pSpinLock = kmalloc(sizeof(spinlock_t), flags);
	if (pSpinLock){
		pLock->pOSLock = pSpinLock;
		spin_lock_init(pSpinLock);
		spin_lock_irqsave(&g_reslock, irq_flags);
		pLock->state = RES_VALID;
		pLock->ref_cnt = 0;
		spin_unlock_irqrestore(&g_reslock, irq_flags);
		return 0;
	}

	return -1;
}


int ral_spin_deinit(KSPIN_LOCK *pLock)
{
	unsigned long irq_flags;

	if (pLock->pOSLock) {
		spin_lock_irqsave(&g_reslock, irq_flags);
		if (pLock->state == RES_INUSE) {
			spin_unlock_irqrestore(&g_reslock, irq_flags);
			printk("Error, free a in_used spin!\n");
			dump_stack();
		} else {
			pLock->state = RES_INVALID;
			spin_unlock_irqrestore(&g_reslock, irq_flags);
			kfree(pLock->pOSLock);
			pLock->pOSLock = NULL;
		}
	}

	return TRUE;
}



/*
	Work around to make Windows feel fun!
*/
// TODO: Shiang, following functions need to revised!!
int KeAcquireSpinLockAtDpcLevel(KSPIN_LOCK *pLock)
{
	// Do spin lock in Dpc level, it actually just return directly.
	//printk("%s(): TODO!!!\n", __FUNCTION__);
	return TRUE;
}


int KeReleaseSpinLockFromDpcLevel(KSPIN_LOCK *pLock)
{
	// Do spin lock in Dpc level, it actually just return directly.
	//printk("%s(): TODO!!!\n", __FUNCTION__);
	return TRUE;
}


/*******************************************************************************

	Eventing system related functions.

 *******************************************************************************/
LONG KeSetEvent(
  INOUT KEVENT *Event,
  IN KPRIORITY Increment,
  IN BOOLEAN Wait)
{
    return 0;
}

NTSTATUS KeWaitForSingleObject(
  IN KEVENT *Object,
  IN int WaitReason,
  IN int WaitMode,
  IN BOOLEAN Alertable,
  IN PLARGE_INTEGER Timeout)
{
    return 0;
}

NTSTATUS KeInitializeEvent(
	IN KEVENT *pEventObj,
	IN int Type,
	IN EVENT_STATE init_state)
{
	NTSTATUS status= STATUS_SUCCESS;
	struct semaphore *pSema;
	int initVal, flags;

	/* First allocate the memory space for Event Object */
	flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	pSema = kmalloc(sizeof(struct semaphore), flags);

	if (pSema){
		initVal = ((init_state == EVENT_LOCKED) ? 0 : 1);
		printk("Create Event with status %s\n", (initVal == 1 ? "UNLOCKED" : "LOCKED"));
		sema_init(pSema, initVal);
		pEventObj->pOSEvent = pSema;
	}
	else
		status = STATUS_FAILURE;

	return status;

}

NTSTATUS KeDestoryEvent(
	IN KEVENT *pEventObj)
{
	if (pEventObj->pOSEvent)
		kfree(pEventObj->pOSEvent);

	return TRUE;
}

ULONG KeQuerySystemTime(LARGE_INTEGER *timeval)
{
	ULONG nowTime = jiffies;

	timeval->QuadPart = nowTime;

	return nowTime;
}


// Unify all delay routine by using NdisStallExecution
VOID rtbt_usec_delay(
	IN ULONG usec)
{
	ULONG   i;

	for (i = 0; i < (usec / 50); i++)
		udelay(50);

	if (usec % 50)
		udelay(usec % 50);
}

/*


*/
// TODO: Change this windows function to a generic function name
void KeStallExecutionProcessor(
	IN ULONG usec)
{
	rtbt_usec_delay(usec);
}

BOOLEAN KeSetTimer(
  IN KTIMER *os_timer,
  IN LARGE_INTEGER msec,
  IN PKDPC Dpc)
{
	struct timer_list *timer;
	unsigned long irqflags, expires;

	ral_spin_lock(&os_timer->lock, &irqflags);
	if ((os_timer->state == RES_INVALID) || (!os_timer->pOSTimer)) {
		printk("os_timer invalid state(%d) or timer structure(0x%p) is NULL!\n",
				os_timer->state, os_timer->pOSTimer);
		ral_spin_unlock(&os_timer->lock, irqflags);
		return FALSE;
	}

	timer = os_timer->pOSTimer;
	expires = msec.QuadPart * HZ / 1000;
	timer->expires = jiffies + expires;
//	printk("set timer->expires as 0x%x, original msec=0x%x, now jiffies=0x%x!\n",  timer->expires, msec.QuadPart, jiffies);
	if (timer_pending(timer))
		mod_timer(timer, timer->expires);
	else	 {
		os_timer->dpc.func = (void *)Dpc->func;
		os_timer->dpc.data = (long)Dpc->data;
		add_timer(timer);
	}
	ral_spin_unlock(&os_timer->lock, irqflags);

	return FALSE;
}

BOOLEAN KeCancelTimer(
	IN KTIMER *os_timer)
{
	int status;
	unsigned long irqflags;

	ral_spin_lock(&os_timer->lock, &irqflags);
	if (os_timer->pOSTimer && (os_timer->state == RES_VALID)) {
		status = del_timer_sync((struct timer_list *)os_timer->pOSTimer);
		if (status < 0)
			printk("%s(): del os timer failed(%d)!\n", __FUNCTION__, status);
	}
	ral_spin_unlock(&os_timer->lock, irqflags);
	return TRUE;
}

INT KeFreeTimer(
	IN PKTIMER os_timer)
{
	struct timer_list *timer;
	unsigned long irqflags;

	if (os_timer->state == RES_INVALID)
		return 0;

	ral_spin_lock(&os_timer->lock, &irqflags);
	os_timer->state = RES_INVALID;
	ral_spin_unlock(&os_timer->lock, irqflags);

	if (os_timer->pOSTimer) {
		timer = (struct timer_list *)os_timer->pOSTimer;
		if (timer_pending(timer))
			del_timer_sync(timer);
		kfree(timer);
		os_timer->pOSTimer = NULL;
	}

	ral_spin_deinit(&os_timer->lock);

	return 0;
}


INT ral_timer_init(
	IN PKTIMER os_timer,
	IN ULONG func)
{
	unsigned long irqflags;
	struct timer_list *timer;

	memset(os_timer, 0, sizeof(KTIMER));
	if (ral_spin_init(&os_timer->lock))
		return -1;

	os_timer->pOSTimer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (os_timer->pOSTimer) {
		timer = os_timer->pOSTimer;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
		timer_setup(timer, (void *)func, 0);
#else
		setup_timer(timer, (void *)func, (unsigned long)os_timer);
#endif
		ral_spin_lock(&os_timer->lock, &irqflags);
		os_timer->state = RES_VALID;
		ral_spin_unlock(&os_timer->lock, irqflags);
		return 0;
	} else {
		ral_spin_deinit(&os_timer->lock);
		return -1;
	}
}


VOID KeInitializeDpc(
  OUT PKDPC Dpc,
  IN	PKDEFERRED_ROUTINE DeferredRoutine,
  IN  PVOID DeferredContext)
{
	Dpc->func = (PKDEFERRED_ROUTINE)DeferredRoutine;
	Dpc->data = (ULONG)DeferredContext;
}


int ral_file_obj_init(PSTRING FileName, RAL_FILE **pFileHd)
{
	/* struct file *pf; */
	int status = STATUS_FAILURE;


	*pFileHd = NULL;

	return status;
}


/* this function only support for READ only. */
int ral_file_open(RAL_FILE *pFileHd, RTBT_FILE_OP_MODE opMode)
{
	/* struct file *fp; */
	int status = STATUS_FAILURE;
	int flag;

	if (pFileHd == NULL)
		return status;

	switch(opMode){
		case RTBT_FOP_READ:
			flag = 1; //O_RDONLY;
			break;
		default:
			return STATUS_FAILURE;
	}

	return status;
}

int ral_file_read(RAL_FILE *pFileHd, void *pBuf, int len)
{
	// TODO:
	return FALSE;
}


int ral_file_write(RAL_FILE *pFileHd, void *pBuf, int len)
{
	// TODO:
	return FALSE;
}

int ral_file_close(RAL_FILE *pFileHd)
{
	// TODO:

	return FALSE;
}

int ral_file_obj_deinit(RAL_FILE *pFileHd)
{
	// TODO:
	return FALSE;
}


/*******************************************************************************

	Task create/management/kill related functions.

 *******************************************************************************/
NDIS_STATUS ral_task_kill(
	IN KTHREAD *pTask)
{
	int ret = STATUS_FAILURE;
	RTBT_OS_TASK *pOSThread = (RTBT_OS_TASK *)pTask->pOSThread;

#ifdef KTHREAD_SUPPORT
	if (pOSThread->kthread_task) {
		kthread_stop(pOSThread->kthread_task);
		ret = STATUS_SUCCESS;
	}
#else
	if(IS_VALID_PID(pOSThread->taskPID)) {
		printk(KERN_WARNING "Terminate the task(%s) with pid(%d)!\n",
					pOSThread->taskName, GET_PID_NUMBER(pOSThread->taskPID));

		mb();
		pOSThread->task_killed = 1;
		mb();
		ret = KILL_THREAD_PID(pOSThread->taskPID, SIGTERM, 1);
		if (ret){
			printk(KERN_WARNING "kill task(%s) with pid(%d) failed(retVal=%d)!\n",
				pOSThread->taskName, GET_PID_NUMBER(pOSThread->taskPID), ret);
		} else {
			wait_for_completion(&pOSThread->taskComplete);
			pOSThread->taskPID = THREAD_PID_INIT_VALUE;
			pOSThread->task_killed = 0;
			ret = STATUS_SUCCESS;
		}
	}
#endif

	return ret;

}


INT ral_task_notify_exit(
	IN KTHREAD *pTask)
{
#ifndef KTHREAD_SUPPORT
	RTBT_OS_TASK *pOSThread = (RTBT_OS_TASK *)pTask->pOSThread;
	complete_and_exit(&pOSThread->taskComplete, 0);
#endif
	return 0;
}


void ral_task_customize(
	IN KTHREAD *pTask)
{
#ifndef KTHREAD_SUPPORT
	RTBT_OS_TASK *pOSTask = (RTBT_OS_TASK *)pTask->pOSThread;
	daemonize((PSTRING)&pOSTask->taskName[0]);
	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	current->flags |= PF_NOFREEZE;
	RTMP_GET_OS_PID(pOSTask->taskPID, current->pid);
    /* signal that we've started the thread */
	complete(&pOSTask->taskComplete);
#endif // KTHREAD_SUPPORT //
}


int ral_task_attach(
	IN KTHREAD *pTask,
	IN RTBT_OS_TASK_CALLBACK fn,
	IN ULONG arg)
{
	RTBT_OS_TASK *pOSTask = (RTBT_OS_TASK *)pTask->pOSThread;
	NDIS_STATUS status = STATUS_SUCCESS;
#ifndef KTHREAD_SUPPORT
	pid_t pid_number = -1;
#endif // KTHREAD_SUPPORT //

#ifdef KTHREAD_SUPPORT
	pOSTask->task_killed = 0;
	pOSTask->kthread_task = NULL;
	pOSTask->kthread_task = kthread_run(fn, (void *)arg, pOSTask->taskName);
	if (IS_ERR(pOSTask->kthread_task))
		status = STATUS_FAILURE;
#else
	pid_number = kernel_thread(fn, (void *)arg, RTBT_OS_MGMT_TASK_FLAGS);
	if (pid_number < 0) {
		printk(KERN_WARNING "Attach task(%s) failed!\n", pOSTask->taskName);
		status = STATUS_FAILURE;
	} else {
		// Wait for the thread to start
		wait_for_completion(&pOSTask->taskComplete);
		status = STATUS_SUCCESS;
	}
#endif

printk("%s(): task attach done, status=%d\n", __FUNCTION__, status);
	return status;
}


int ral_task_init(
	IN KTHREAD *pTask,
	IN PSTRING	pTaskName,
	IN VOID		*pPriv)
{
	int len;
	RTBT_OS_TASK *pOSTask;
	RTBT_ASSERT(pTask);

	pTask->pOSThread = kmalloc(sizeof(RTBT_OS_TASK), GFP_KERNEL);

	if (pTask->pOSThread == NULL)
		return STATUS_FAILURE;

	pOSTask = (RTBT_OS_TASK *)pTask->pOSThread;
#ifndef KTHREAD_SUPPORT
	memset((PUCHAR)pOSTask, 0, sizeof(RTBT_OS_TASK));
#endif

	len = strlen(pTaskName);
	//len = len > (RTBT_OS_TASK_NAME_LEN -1) ? (RTBT_OS_TASK_NAME_LEN-1) : len;
	memcpy(&pOSTask->taskName[0], pTaskName, len > (RTBT_OS_TASK_NAME_LEN -1) ? (RTBT_OS_TASK_NAME_LEN-1) : len);
	pOSTask->priv = pPriv;

#ifndef KTHREAD_SUPPORT
	RTMP_SEM_EVENT_INIT_LOCKED(&(pOSTask->taskSema));
	pOSTask->taskPID = THREAD_PID_INIT_VALUE;

	init_completion(&pOSTask->taskComplete);
#endif

	return STATUS_SUCCESS;
}


int ral_task_deinit(
	IN KTHREAD *pTask)
{
	RTBT_OS_TASK *pOSTask;

	RTBT_ASSERT(pTask);
	if (pTask->refCnt > 1) {
		DebugPrint(ERROR, DBG_MISC, "Err, pTask->refCnt=%d!\n", pTask->refCnt);
	}

	if (pTask->pOSThread) {
		pOSTask = pTask->pOSThread;

		RTMP_SEM_EVENT_DESTORY(&pOSTask->taskSema);
		kfree(pTask->pOSThread);
		pTask->pOSThread = NULL;
		pTask->refCnt = 0;
	}

	return STATUS_SUCCESS;
}

#ifdef OS_ABL_SUPPORT
static struct rtbt_dev_entry *g_devlist = NULL;
static DEFINE_SPINLOCK(g_devlock);

void dump_dev_list(struct rtbt_dev_entry *devlist)
{
	unsigned long irq_flag;
	struct rtbt_dev_entry *dev_ent;
	int cnt = 0;
	int dump_all;

	dump_all = devlist ? 0 : 1;

	printk("Wrapper:dump the global dev list:\n");

	spin_lock_irqsave(&g_devlock, irq_flag);
	dev_ent = dump_all ? g_devlist : devlist;
	while (dev_ent != NULL) {
		struct ral_dev_id *dev_id;

		printk("\tDevGroup[%d]:\n", cnt);
		printk("\t\tinfType = %d\n", dev_ent->infType);
		printk("\t\tdevType = %d\n", dev_ent->devType);
		printk("\t\tdev_priv = 0x%p\n", dev_ent->dev_ops);
		printk("\t\tos_priv = 0x%p\n", dev_ent->os_private);
		printk("\t\tmlen = %d\n", dev_ent->mlen);
		printk("\t\tdevIDList:\n");
		if (dev_ent->pDevIDList) {
			dev_id = dev_ent->pDevIDList;
			while((dev_id->pid != 0) && (dev_id->vid != 0)) {
				printk("\t\t\tPID:0x%02x, DID:0x%02x\n",
					dev_id->pid, dev_id->vid);
				dev_id++;
			}
		}
		dev_ent = (dump_all ? dev_ent->next : NULL);
	}

	printk("\n\n");
	spin_unlock_irqrestore(&g_devlock, irq_flag);

}

static int rtbt_dev_list_add(struct rtbt_dev_entry *devlist)
{
	unsigned long irq_flag;

	printk("%s(): add devlist from global list!\n", __FUNCTION__);

	spin_lock_irqsave(&g_devlock, irq_flag);
	devlist->next = g_devlist;
	g_devlist = devlist;
	spin_unlock_irqrestore(&g_devlock, irq_flag);

	dump_dev_list(NULL);
	return 0;
}

static int rtbt_dev_list_del(struct rtbt_dev_entry *devlist)
{
	unsigned long irq_flag;
	struct rtbt_dev_entry *head, *prev;

	printk("%s(): remove devlist from global list!\n", __FUNCTION__);

	spin_lock_irqsave(&g_devlock, irq_flag);
	head = g_devlist;
	while (head != NULL){
		if (devlist == head)
			break;
		prev = head;
		head = head->next;
	}

	if (head){
		if (head == g_devlist)
			g_devlist = head->next;
		else
			prev->next = head->next;
	}
	spin_unlock_irqrestore(&g_devlock, irq_flag);

	dump_dev_list(NULL);

	return 0;
}

static int __init rtbt_linux_init(void)
{
	unsigned long irq_flag;

	printk("-->%s()\n", __FUNCTION__);
	spin_lock_init(&g_devlock);
	spin_lock_init(&g_reslock);

	spin_lock_irqsave(&g_devlock, irq_flag);
	g_devlist = NULL;
	spin_unlock_irqrestore(&g_devlock, irq_flag);
	printk("<--%s(): g_devlist(@0x%lx)=0x%p!\n", __FUNCTION__, (ULONG)&g_devlist, g_devlist);

	return 0;
}

static void __exit rtbt_linux_exit(void)
{
	printk("-->%s()\n", __FUNCTION__);
	while (g_devlist != NULL)
		ral_os_unregister(g_devlist);
	printk("<--%s(): g_devlist(@0x%lx)=0x%p!\n", __FUNCTION__, (ULONG)&g_devlist, g_devlist);

	return;
}

MODULE_AUTHOR("Ralink Tech.");
MODULE_DESCRIPTION("Support for Ralink Bluetooth cards");
MODULE_SUPPORTED_DEVICE("Ralink Bluetooth cards");
MODULE_LICENSE("GPL");

module_init(rtbt_linux_init);
module_exit(rtbt_linux_exit);
#endif // OS_ABL_SUPPORT //

int ral_os_register(struct rtbt_dev_entry *pDevList)
{
	int retVal = -1;

#ifdef OS_ABL_SUPPORT
	rtbt_dev_list_add(pDevList);
#endif // OS_ABL_SUPPORT //

#ifdef RTBT_IFACE_PCI
	if ((pDevList->infType == RAL_INF_PCI) &&
		(pDevList->devType == RAL_DEV_BT)){

		retVal = rtbt_iface_pci_hook(pDevList);
		if (retVal)
			printk("register device to OS failed\n");
	}
#endif // RTBT_IFACE_PCI //

	return retVal;
}


int ral_os_unregister(struct rtbt_dev_entry * pDevList)
{
	int rv = -1;

#ifdef RTBT_IFACE_PCI
	if (pDevList->infType == RAL_INF_PCI) {
		rv = rtbt_iface_pci_unhook(pDevList);
		if (rv)
			printk("unregister device to OS failed\n");
	}
#endif // RTBT_IFACE_PCI //

#ifdef OS_ABL_SUPPORT
	rtbt_dev_list_del(pDevList);
#endif // OS_ABL_SUPPORT //

	return rv;
}
