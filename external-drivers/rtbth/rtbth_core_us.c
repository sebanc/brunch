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

#include "rt_linux.h"
#include "rtbt_ctrl.h"
#include "rtbth_us.h"
#include "hps_bluez.h"

RTBTH_ADAPTER *gpAd = 0;
static int pollm = 1;
static int evt_wq_flag = 0;
static int sync_wq_flag = 0;
static int bind_wq_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(evt_wq);
static DECLARE_WAIT_QUEUE_HEAD(sync_wq);
//static unsigned int evt;    // input buffer of read()


#define RB_SIZE(prb) ((prb)->size)
#define RB_MASK(prb) (RB_SIZE(prb) - 1)
#define RB_COUNT(prb) ((prb)->write - (prb)->read)
#define RB_FULL(prb) (RB_COUNT(prb) >= RB_SIZE(prb))
#define RB_EMPTY(prb) ((prb)->write == (prb)->read)

#define RB_INIT(prb, qsize) \
   { \
   (prb)->read = (prb)->write = 0; \
   (prb)->size = (qsize); \
   }

#define RB_PUT(prb, value) \
{ \
    if (!RB_FULL( prb )) { \
        (prb)->queue[ (prb)->write & RB_MASK(prb) ] = value; \
        ++((prb)->write); \
    } \
    else { \
        \
    } \
}

#define RB_GET(prb, value) \
{ \
    if (!RB_EMPTY(prb)) { \
        value = (prb)->queue[ (prb)->read & RB_MASK(prb) ]; \
        ++((prb)->read); \
        if (RB_EMPTY(prb)) { \
            (prb)->read = (prb)->write = 0; \
        } \
    } \
    else { \
        value = NULL; \
    } \
}

#define _osal_inline_ inline

typedef enum e_logical_link_index {
    INQUIRY_LOGICAL_LINK                = 0,
    INQUIRY_SCAN_LOGICAL_LINK           = 1,
    ASBU_LOGICAL_LINK                   = 2,
    PSBU_LOGICAL_LINK                   = 3,
    PSBC_LOGICAL_LINK                   = 4,
    SYNC_LOGICAL_LINK                   = 5,
    ACLU_LOGICAL_LINK                   = SYNC_LOGICAL_LINK + 3,
    ACLC_LOGICAL_LINK                   = ACLU_LOGICAL_LINK + 7,
    STANDBY_LOGICAL_LINK                = ACLC_LOGICAL_LINK + 7,
    LE_SCAN_LOGICAL_LINK                = STANDBY_LOGICAL_LINK + 1,
    LE_ACL_LOGICAL_LINK                 = LE_SCAN_LOGICAL_LINK + 1,
    TOTAL_NUM_OF_LLINKS                 = LE_ACL_LOGICAL_LINK + 3,
    INVALID_LOGICAL_LINK                = 0xFF
} LLIDX;

//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

UINT32 osal_op_get_id(P_OSAL_OP pOp)
{
    return (pOp) ? pOp->op.opId : 0xFFFFFFFF;
}
int osal_unsleepable_lock_init (P_OSAL_UNSLEEPABLE_LOCK pUSL)
{
    spin_lock_init(&(pUSL->lock));
    return 0;
}
int osal_lock_unsleepable_lock (P_OSAL_UNSLEEPABLE_LOCK pUSL)
{
    spin_lock_irqsave(&(pUSL->lock), pUSL->flag);
    return 0;
}
int osal_unlock_unsleepable_lock (P_OSAL_UNSLEEPABLE_LOCK pUSL)
{
    spin_unlock_irqrestore(&(pUSL->lock), pUSL->flag);
    return 0;
}

extern int osal_unsleepable_lock_deinit (P_OSAL_UNSLEEPABLE_LOCK pUSL)
{
    return 0;
}
/*
  *OSAL layer Thread Opeartion releated APIs
  *
  *
*/
_osal_inline_ int
osal_thread_create (
    P_OSAL_THREAD pThread
    )
{
    pThread->pThread = kthread_create(pThread->pThreadFunc,
        pThread->pThreadData,
        pThread->threadName);
    if (NULL == pThread->pThread) {
        return -1;
    }
    return 0;
}
_osal_inline_ int
osal_thread_run (
    P_OSAL_THREAD pThread
    )
{
    if (pThread->pThread) {
        wake_up_process(pThread->pThread);
        return 0;
    }
    else {
        return -1;
    }
}

_osal_inline_ int
osal_thread_stop (
    P_OSAL_THREAD pThread
    )
{
    int iRet;
    if ( (pThread) && (pThread->pThread) ) {
        iRet = kthread_stop(pThread->pThread);
        //pThread->pThread = NULL;
        return iRet;
    }
    return -1;
}

_osal_inline_ int
osal_thread_should_stop (
    P_OSAL_THREAD pThread
    )
{
    if ( (pThread) && (pThread->pThread) ) {
        return kthread_should_stop();
    }
    else {
        return 1;
    }
}

typedef unsigned int (*P_OSAL_EVENT_CHECKER)(P_OSAL_THREAD pThread);
_osal_inline_ int
osal_thread_wait_for_event (
    P_OSAL_THREAD pThread,
    P_OSAL_EVENT pEvent,
    P_OSAL_EVENT_CHECKER pChecker
    )
{
    if ( (pThread) && (pThread->pThread) && (pEvent) && (pChecker)) {
/*        pDevWmt = (P_DEV_WMT)(pThread->pThreadData);*/
        return wait_event_interruptible(pEvent->waitQueue,
            (/*!RB_EMPTY(&pDevWmt->rActiveOpQ) ||*/ osal_thread_should_stop(pThread) || (*pChecker)(pThread)));
    }
    return -1;
}

_osal_inline_ int
osal_thread_destroy (
    P_OSAL_THREAD pThread
    )
{
    if (pThread && (pThread->pThread)) {
        kthread_stop(pThread->pThread);
        pThread->pThread = NULL;
    }
    return 0;
}

/*
  *OSAL layer Signal Opeartion releated APIs
  *initialization
  *wait for signal
  *wait for signal timerout
  *raise signal
  *destroy a signal
  *
*/

_osal_inline_ int
osal_signal_init (
    P_OSAL_SIGNAL pSignal
    )
{
    if (pSignal) {
        init_completion(&pSignal->comp);
        return 0;
    }
    else {
        return -1;
    }
}

_osal_inline_ int
osal_wait_for_signal (
    P_OSAL_SIGNAL pSignal
    )
{
    if (pSignal) {
        wait_for_completion_interruptible(&pSignal->comp);
        return 0;
    }
    else {
        return -1;
    }
}

_osal_inline_ int
osal_wait_for_signal_timeout (
    P_OSAL_SIGNAL pSignal
    )
{
    /* return wait_for_completion_interruptible_timeout(&pSignal->comp, msecs_to_jiffies(pSignal->timeoutValue));*/
    /* [ChangeFeature][George] gps driver may be closed by -ERESTARTSYS.
     * Avoid using *interruptible" version in order to complete our jobs, such
     * as function off gracefully.
     */
    return wait_for_completion_timeout(&pSignal->comp, msecs_to_jiffies(pSignal->timeoutValue));
}

_osal_inline_ int
osal_raise_signal (
    P_OSAL_SIGNAL pSignal
    )
{
    // TODO:[FixMe][GeorgeKuo]: DO sanity check here!!!
    complete(&pSignal->comp);
    return 0;
}

_osal_inline_ int
osal_signal_deinit (
    P_OSAL_SIGNAL pSignal
    )
{
    // TODO:[FixMe][GeorgeKuo]: DO sanity check here!!!
    pSignal->timeoutValue = 0;
    return 0;
}


/*
  *OSAL layer Event Opeartion releated APIs
  *initialization
  *wait for signal
  *wait for signal timerout
  *raise signal
  *destroy a signal
  *
*/

int osal_event_init (
    P_OSAL_EVENT pEvent
    )
{
    init_waitqueue_head(&pEvent->waitQueue);

    return 0;
}

int osal_wait_for_event(
    P_OSAL_EVENT pEvent,
    int (*condition)(PVOID),
    void *cond_pa
    )
{
    return  wait_event_interruptible(pEvent->waitQueue, condition(cond_pa));
}

int osal_wait_for_event_timeout(
       P_OSAL_EVENT pEvent,
       int (*condition)(PVOID),
       void *cond_pa
       )
{
    return wait_event_interruptible_timeout(pEvent->waitQueue, condition(cond_pa), msecs_to_jiffies(pEvent->timeoutValue));
}

int osal_trigger_event(
    P_OSAL_EVENT pEvent
    )
{
    int ret = 0;
    wake_up_interruptible(&pEvent->waitQueue);
    return ret;
}

int
osal_event_deinit (
    P_OSAL_EVENT pEvent
    )
{
    return 0;
}

int osal_op_is_wait_for_signal(P_OSAL_OP pOp)
{
    return (pOp && pOp->signal.timeoutValue) ?  1 : 0;
}

void osal_op_raise_signal(P_OSAL_OP pOp, int result)
{
    if (pOp)
    {
        pOp->result = result;
        osal_raise_signal(&pOp->signal);
    }
}

static int _rtbth_us_handler(RTBTH_ADAPTER *pAd, P_STP_OP pStpOp)
{
    int ret = -1;
    RTBTH_EVENT_T evt = 0;
static int in=0;
    if (NULL == pStpOp){
        return -1;
    }

    switch(pStpOp->opId) {
        case BZ_HCI_EVENT:
        case BZ_ACL_EVENT:
        case BZ_SCO_EVENT:
        case INT_RX_EVENT:
        case INT_TX_EVENT:
        case INT_MCUCMD_EVENT:
        case INT_MCUEVT_EVENT:
        case INT_LMPTMR_EVENT:
        case INT_LMPERR_EVENT:
        case RTBTH_TEST_EVENT:
            DebugPrint(TRACE, DBG_INIT,"Event =%d, jiffies = %d, evt_fifo_len = %d, in=%d\n",
                pStpOp->opId, jiffies, kfifo_len(pAd->evt_fifo), ++in);
            if(kfifo_avail(pAd->evt_fifo) >= sizeof(RTBTH_EVENT_T)){
                evt = pStpOp->opId;
                kfifo_in(pAd->evt_fifo, &evt , sizeof(RTBTH_EVENT_T));
               // if(kfifo_len(pAd->evt_fifo) == sizeof(RTBTH_EVENT_T)){
               // if(kfifo_len(pAd->evt_fifo) > 0){
                    evt_wq_flag = 1;
                    wake_up_interruptible(&evt_wq);
              //  }
                ret = 0;
            } else {
                DebugPrint(TRACE, DBG_INIT,"evt_fifo size is not available!\n");
                ret = -1;
            }
            break;
        case INIT_COREINIT_EVENT:
        case INIT_COREDEINIT_EVENT:
        case INIT_CORESTART_EVENT:
        case INIT_CORESTOP_EVENT:
        case INIT_EPINIT_EVENT:
            DebugPrint(LOUD, DBG_INIT,"Event =%d, jiffies = %d, evt_fifo_len = %d, in=%d\n",
                pStpOp->opId,  jiffies, kfifo_len(pAd->evt_fifo), ++in);
            if(kfifo_avail(pAd->evt_fifo) >= sizeof(RTBTH_EVENT_T)){
                evt = pStpOp->opId;
                kfifo_in(pAd->evt_fifo, &evt , sizeof(RTBTH_EVENT_T));
        //        if(kfifo_len(pAd->evt_fifo) == sizeof(RTBTH_EVENT_T)){
       // if(kfifo_len(pAd->evt_fifo) > 0){
                    evt_wq_flag = 1;
                    wake_up_interruptible(&evt_wq);
         //       }
                ret = 0;
            } else {
                DebugPrint(TRACE, DBG_INIT,"evt_fifo size is not available!\n");
                ret = -1;
            }
            DebugPrint(TRACE, DBG_INIT,"wait for the event complete\n");
          //  msleep(1000);
            sync_wq_flag = 0;
            ret = wait_event_interruptible_timeout(sync_wq, sync_wq_flag != 0, HZ);
            if(!ret)
                DebugPrint(TRACE, DBG_INIT,"wait for sync timeout\n");

            break;

        case RTBTH_EXIT:
            ret = 0;
            break;

        default:
           DebugPrint(TRACE, DBG_INIT,"invalid operation id (%d)\n", pStpOp->opId);
           ret = -1;
           break;
    }

    return ret;
}

static P_OSAL_OP _rtbth_us_get_op (
    RTBTH_ADAPTER *pAd,
    P_OSAL_OP_Q pOpQ
    )
{
    P_OSAL_OP pOp;

    if (!pOpQ)
    {
       DebugPrint(TRACE, DBG_INIT,"pOpQ == NULL\n");
        return NULL;
    }

    osal_lock_unsleepable_lock(&(pAd->wq_spinlock));
    /* acquire lock success */
    RB_GET(pOpQ, pOp);

    osal_unlock_unsleepable_lock(&(pAd->wq_spinlock));

    if (!pOp)
    {
        DebugPrint(TRACE, DBG_INIT,"RB_GET fail\n");
    }

    return pOp;
}

static int _rtbth_us_put_op (RTBTH_ADAPTER *pAd, P_OSAL_OP_Q pOpQ, P_OSAL_OP pOp){
    int ret;

    if (!pOpQ || !pOp){
        DebugPrint(TRACE, DBG_INIT,"pOpQ = 0x%p, pLxOp = 0x%p \n", pOpQ, pOp);
        return 0;
    }

    ret = 0;

    osal_lock_unsleepable_lock(&(pAd->wq_spinlock));
    if (!RB_FULL(pOpQ)){
        RB_PUT(pOpQ, pOp);
    } else {
        ret = -1;
    }
    osal_unlock_unsleepable_lock(&(pAd->wq_spinlock));

    if (ret){
        DebugPrint(TRACE, DBG_INIT,"RB_FULL, RB_COUNT=%d , RB_SIZE=%d\n",RB_COUNT(pOpQ), RB_SIZE(pOpQ));
        return 0;
    } else {
        return 1;
    }
}

P_OSAL_OP _rtbth_us_get_free_op (RTBTH_ADAPTER *pAd){

    P_OSAL_OP pOp;

    if (pAd){
        pOp = _rtbth_us_get_op(pAd, &pAd->rFreeOpQ);
        if (pOp){
            memset(&pOp->op, 0, sizeof(pOp->op));
        }
        return pOp;
    } else {
        return NULL;
    }
}

int _rtbth_us_put_act_op (RTBTH_ADAPTER *pAd, P_OSAL_OP pOp)
{
    int bRet = 0;//MTK_WCN_BOOL_FALSE;
    int bCleanup = 0;//MTK_WCN_BOOL_FALSE;
    int wait_ret = -1;
    P_OSAL_SIGNAL pSignal = NULL;

    do {
        if (!pAd || !pOp)   {
            DebugPrint(ERROR, DBG_INIT,"pAd = %p, pOp = %p\n", pAd, pOp);
            break;
        }

        pSignal = &pOp->signal;

        if (pSignal->timeoutValue){
            pOp->result = -9;
            osal_signal_init(&pOp->signal);
        }

        /* put to active Q */
        bRet = _rtbth_us_put_op(pAd, &pAd->rActiveOpQ, pOp);

        if(0 == bRet){
            DebugPrint(TRACE, DBG_INIT,"+++++++++++ Put op Active queue Fail\n");
            bCleanup = 1;//MTK_WCN_BOOL_TRUE;
            break;
        }

        /* wake up wmtd */
        osal_trigger_event(&pAd->STPd_event);

        if (pSignal->timeoutValue == 0) {
            bRet = 1;//MTK_WCN_BOOL_TRUE;
            /* clean it in wmtd */
            break;
        }

        /* wait result, clean it here */
        bCleanup = 1;//MTK_WCN_BOOL_TRUE;

        /* check result */
        wait_ret = osal_wait_for_signal_timeout(&pOp->signal);

        DebugPrint(TRACE, DBG_INIT,"wait completion:%d\n", wait_ret);
        if (!wait_ret){
            DebugPrint(TRACE, DBG_INIT,"wait completion timeout \n");
            // TODO: how to handle it? retry?
        } else  {
            if (pOp->result)
            {
                DebugPrint(TRACE, DBG_INIT,"op(%d) result:%d\n", pOp->op.opId, pOp->result);
            }
            /* op completes, check result */
            bRet = (pOp->result) ? 0 : 1;
        }
    } while(0);

    if (bCleanup) {
        /* put Op back to freeQ */
        bRet = _rtbth_us_put_op(pAd, &pAd->rFreeOpQ, pOp);
        if(bRet == 0){
            DebugPrint(TRACE, DBG_INIT,"+++++++++++ Put op active free fail, maybe disable/enable psm\n");
        }
    }

    return bRet;
}

static int _rtbth_us_wait_for_evt(void *pvData)
{
    RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)pvData;

    DebugPrint(LOUD, DBG_INIT,"%s: pAd->rActiveOpQ = %d\n", __func__, RB_COUNT(&pAd->rActiveOpQ));

    return ((!RB_EMPTY(&pAd->rActiveOpQ)) || osal_thread_should_stop(&pAd->PSMd));
}

INT32 _rtbth_us_event_notification(RTBTH_ADAPTER *pAd, RTBTH_EVENT_T evt)
{
    P_OSAL_OP  pOp;
    int        bRet;
    int        retval;

    if(!pAd){
        return -1;
    }

    pOp = _rtbth_us_get_free_op(pAd);
    if (!pOp)
    {
        DebugPrint(ERROR, DBG_INIT,"get_free_lxop fail \n");
        return -1;
    }

    pOp->op.opId = (unsigned int) evt;
    pOp->signal.timeoutValue = 0;
    bRet = _rtbth_us_put_act_op(pAd, pOp);

    DebugPrint(LOUD, DBG_INIT,"OPID(%d) type(%d) bRet(%d) \n\n",
        pOp->op.opId,
        pOp->op.au4OpData[0],
        bRet);

    retval = (0 == bRet) ? (-1) : 0;

    return retval;
}

static int _rtbth_us_proc (void *pvData)
{
    RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)pvData;

    P_OSAL_OP pOp;

    unsigned int id;
    int ret;

    if (!pAd) {
        DebugPrint(TRACE, DBG_INIT,"!pAd\n");
        return -1;
    }

    DebugPrint(TRACE, DBG_INIT,"rtbth_us starts running: pWmtDev(0x%p) [pol, rt_pri, n_pri, pri]=[%d, %d, %d, %d] \n",
        pAd, current->policy, current->rt_priority, current->normal_prio, current->prio);

    for (;;) {

        pOp = NULL;

        osal_wait_for_event(&pAd->STPd_event,
            _rtbth_us_wait_for_evt,
            (void *)pAd);

        if (osal_thread_should_stop(&pAd->PSMd))
        {
            DebugPrint(TRACE, DBG_INIT,"should stop now... \n");
            // TODO: clean up active opQ
            break;
        }

        /* get Op from activeQ */
        pOp = _rtbth_us_get_op(pAd, &pAd->rActiveOpQ);
        if (!pOp)
        {
            DebugPrint(TRACE, DBG_INIT,"+++++++++++ Get op from activeQ fail, maybe disable/enable psm\n");
            continue;
        }

        id = osal_op_get_id(pOp);

        if (id >= RTBTH_EVENT_NUM)
        {
           DebugPrint(TRACE, DBG_INIT,"abnormal opid id: 0x%x \n", id);
            ret = -1;
            goto handler_done;
        }

        ret = _rtbth_us_handler(pAd, &pOp->op);

handler_done:

        if (ret)
        {
         //   DebugPrint(TRACE, DBG_INIT,"opid id(0x%x)(%s) error(%d)\n", id, (id >= 4)?("???"):(g_psm_op_name[id]), ret);
        }

        if (osal_op_is_wait_for_signal(pOp))
        {
            osal_op_raise_signal(pOp, ret);
        }
        else
        {
           /* put Op back to freeQ */
           if(_rtbth_us_put_op(pAd, &pAd->rFreeOpQ, pOp) == 0)
           {
                DebugPrint(TRACE, DBG_INIT,"+++++++++++ Put op to FreeOpQ fail, maybe disable/enable psm\n");
           }
        }

        if (RTBTH_EXIT == id)
        {
            break;
        }
    }
    DebugPrint(TRACE, DBG_INIT,"exits \n");

    return 0;
};

static atomic_t rtbth_us_avail = ATOMIC_INIT(1);
static int rtbth_us_open(struct inode *inode, struct file *file){

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    if(!atomic_dec_and_test(&rtbth_us_avail)){
        atomic_inc(&rtbth_us_avail);
        DebugPrint(WARNING, DBG_INIT,"rtbth occupied: rtbth_us_avail=%d\n",
        atomic_read(&rtbth_us_avail)
        );
        return -EBUSY;
    }

    DebugPrint(TRACE, DBG_INIT,"%s: major %d minor %d (pid %d)\n", __func__,
        imajor(inode),
        iminor(inode),
        current->pid
        );
    bind_wq_flag = 1;
#if 0
    /* Init the host protocol stack hooking interface */
    if (rtbt_hps_iface_init(RAL_INF_PCI,  gpAd->os_ctrl->if_dev , gpAd->os_ctrl)){
         DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_init fail\n");
    } else {
         DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_init ok\n");
    }
#endif
    /* Link the host protocol stack interface to the protocl stack */
    if (rtbt_hps_iface_attach(gpAd->os_ctrl)){
         DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_attach fail\n");
    } else {
         DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_attach ok\n");
    }

    return 0;
}

static int rtbth_us_close(struct inode *inode, struct file *file){

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    atomic_inc(&rtbth_us_avail);

    DebugPrint(TRACE, DBG_INIT,"%s: major %d minor %d (pid %d)\n", __func__,
        imajor(inode),
        iminor(inode),
        current->pid
        );
    bind_wq_flag = 0;

    if(rtbt_hps_iface_detach(gpAd->os_ctrl)){
         DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_detach fail\n");
    } else {
         DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_detach ok\n");
    }
#if 0
    if(rtbt_hps_iface_deinit(RAL_INF_PCI, gpAd->os_ctrl->if_dev, gpAd->os_ctrl)){
          DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_deinit fail\n");
    } else {
         DebugPrint(ERROR, DBG_INIT,"rtbt_hps_iface_deinit ok\n");
    }
#endif
    return 0;
}

#if 1

ssize_t rtbth_us_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){

    ssize_t retval = 0;
    RTBTH_EVENT_T evt = 0;
    int wait_ret = 0;

//    down(&rd_mtx);

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    while(1){
            if(kfifo_len(gpAd->evt_fifo) > 0){
                kfifo_out(gpAd->evt_fifo, &evt, sizeof(RTBTH_EVENT_T));
                DebugPrint(TRACE, DBG_INIT,"%s: event fifo len = %d\n", __func__, kfifo_len(gpAd->evt_fifo));
                break;
            } else {
                if(pollm){
                    retval = -EAGAIN;
                    break;
                } else {
                    DebugPrint(TRACE, DBG_INIT,"%s: wait for the event, pending on kernel space\n", __func__);
                    wait_ret = wait_event_interruptible_timeout(evt_wq, evt_wq_flag != 0, HZ);
                    if (wait_ret) {
                        if (-ERESTARTSYS == wait_ret) {
                            DebugPrint(TRACE, DBG_INIT,"%s: signaled by -ERESTARTSYS(%ld) \n ", __func__, wait_ret);
			    return -EAGAIN;
                        }
                        else {
                            DebugPrint(TRACE, DBG_INIT,"%s: signaled by %ld \n ", __func__, wait_ret);
                        }
                    }  else {
	                DebugPrint(INFO, DBG_INIT,"%s: wait timeout %d\n ", __func__, wait_ret);
        	        retval = -EFAULT;
                	goto out;
                    }
                    evt_wq_flag = 0;
                    DebugPrint(TRACE, DBG_INIT,"%s: receive the event, return to user space\n", __func__);
            }
        }
    }
    if(retval == -EAGAIN)
        goto out;

    if (copy_to_user(buf, &evt, sizeof(evt)))
    {
        retval = -EFAULT;
        goto out;
    }

    retval = sizeof(RTBTH_EVENT_T);

out:
//    up(&rd_mtx);

    return retval;
}
#else

ssize_t rtbth_us_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){

    ssize_t retval = 0;
    RTBTH_EVENT_T evt = 0;
    int wait_ret = 0;
static int entries = 0;
static int out=0;

    entries = 0;

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    while(1){
        if(kfifo_len(gpAd->evt_fifo) > 0){
            kfifo_out(gpAd->evt_fifo, &evt, sizeof(RTBTH_EVENT_T));
            DebugPrint(TRACE, DBG_INIT,"%s: remaining event fifo len = %d, out=%d\n", __func__, kfifo_len(gpAd->evt_fifo), ++out);
            break;
        } else {
            DebugPrint(TRACE, DBG_INIT,"%s: wait for the event, pending on kernel space, %d, kfifo_len(evt)=%d\n",
                __func__, entries++, kfifo_len(gpAd->evt_fifo));

           // wait_ret = wait_event_interruptible(evt_wq, evt_wq_flag != 0);
           wait_ret = wait_event_interruptible_timeout(evt_wq, evt_wq_flag != 0, HZ);
           DebugPrint(INFO, DBG_INIT,"%s: wait event returned value %d\n ", __func__, wait_ret);
           evt_wq_flag = 0;
            if (wait_ret) {
                if (-ERESTARTSYS == wait_ret) {
                    DebugPrint(TRACE, DBG_INIT,"%s: signaled by -ERESTARTSYS(%ld) \n ", __func__, wait_ret);
                    return -EFAULT;
                }
                else {
                    DebugPrint(TRACE, DBG_INIT,"%s: signaled by %ld \n ", __func__, wait_ret);
                }
            } else {
                DebugPrint(INFO, DBG_INIT,"%s: wait timeout %d\n ", __func__, wait_ret);
                retval = -EFAULT;
                goto out;
            }
        }
    }

    if (copy_to_user(buf, &evt, sizeof(RTBTH_EVENT_T)))
    {
        retval = -EFAULT;
        goto out;
    }
    DebugPrint(TRACE, DBG_INIT,"return type=%d, return to user space\n", evt);

    retval = sizeof(RTBTH_EVENT_T);

out:

    return retval;
}


#endif

ssize_t rtbth_us_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos){

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    return 0;
}

#if 0
#define RTBTH_US_IOC_MAGIC  'k'
#define RTBTH_IOCPCI32WRITE _IOWR(RTBTH_US_IOC_MAGIC, 1, int)
#define RTBTH_IOCPCI32READ  _IOWR(RTBTH_US_IOC_MAGIC, 2, int)
#define RTBTH_IOCMCUWRITE   _IOWR(RTBTH_US_IOC_MAGIC, 3, int)
#define RTBTH_IOCMCUREAD    _IOWR(RTBTH_US_IOC_MAGIC, 4, int)
#define RTBTH_IOCBBWRITE    _IOWR(RTBTH_US_IOC_MAGIC, 5, int)
#define RTBTH_IOCBBREAD     _IOWR(RTBTH_US_IOC_MAGIC, 6, int)
#define RTBTH_IOCRFWRITE    _IOWR(RTBTH_US_IOC_MAGIC, 7, int)
#define RTBTH_IOCRFREAD     _IOWR(RTBTH_US_IOC_MAGIC, 8, int)
#define RTBTH_IOCBZWRITE    _IOWR(RTBTH_US_IOC_MAGIC, 9, int)
#define RTBTH_IOCBZREAD     _IOWR(RTBTH_US_IOC_MAGIC,10, int)
#define RTBTH_IOCTRCTRL     _IOWR(RTBTH_US_IOC_MAGIC,11, int)
#define RTBTH_IOCSYNC       _IOWR(RTBTH_US_IOC_MAGIC,12, int)
#define RTBTH_IOCTST        _IOWR(RTBTH_US_IOC_MAGIC,13, int)
#define RTBTH_IOCRMODE      _IOWR(RTBTH_US_IOC_MAGIC,14, int)

#define RTBTH_IOC_MAXNR 15
#endif

VOID
MCU_Read_Memory(
    IN  PRTBTH_ADAPTER  pAd,
    IN  UINT32          Address,
    OUT PUINT8          pBuffer,
    IN  UINT16          Length)
{
    INT32   i, j, k;
    UINT32  Val = 0;

    // Only read shared memory at the word boundary
    RTBT_ASSERT((Address % 4) == 0);

    i = Length / 4;
    k = Length % 4;
    for (j = 0; j < i; j++)
    {
        RT_IO_READ32(pAd, Address+(j*4), &Val);
        pBuffer[j*4]   = (UINT8)Val;
        pBuffer[j*4+1] = (UINT8)(Val >> 8);
        pBuffer[j*4+2] = (UINT8)(Val >> 16);
        pBuffer[j*4+3] = (UINT8)(Val >> 24);
    }

    if (k > 0)
    {
        RT_IO_READ32(pAd, Address+(i*4), &Val);
        for (j = 0; j < k; j++)
        {
            pBuffer[(i*4)+j] = (UINT8)(Val >> (8 * j));
        }
    }

}


VOID
MCU_Write_Memory(
    IN  PRTBTH_ADAPTER  pAd,
    IN  UINT32          Address,
    IN  PUINT8          pBuffer,
    IN  UINT16          Length)
{
    INT32   i, j, k;
    UINT32  Val;

    //DBGPRINT(DBG_NIC_TRACE,( "RtbtWriteMcuMemory: Addr=0x%0x4, Len=%d\n", Address, Length));

    // Only write shared memory at the word boundary
    RTBT_ASSERT((Address % 4) == 0);

    i = Length / 4;
    k = Length % 4;
    for (j = 0; j < i; j++)
    {
        Val = ( (UINT32)pBuffer[j*4])
            | (((UINT32)pBuffer[(j*4)+1]) << 8)
            | (((UINT32)pBuffer[(j*4)+2]) << 16)
            | (((UINT32)pBuffer[(j*4)+3]) << 24);
        RT_IO_WRITE32(pAd, Address+(j*4), Val);
    }

    if (k > 0)
    {
        RT_IO_READ32(pAd, Address+(i*4), &Val);
        for (j = 0; j < k; j++)
        {
            Val &= ~(0xff << (8 * j));
            Val += ((UINT32)pBuffer[(i*4)+j]) << (8 * j);
        }
        RT_IO_WRITE32(pAd, Address+(i*4), Val);
    }

}

VOID
PDMA_Transmit_TxRing(
    IN PRTBTH_ADAPTER   pAd,
    IN UINT8            LLIdx,
    IN UINT8            RingIdx,
    IN UINT8            PktType,
    IN UINT8            Llid,
    IN UINT8            PFlow,
    IN UINT16           Tag,
    IN PUINT8           pPacket,
    IN UINT16           Length,
    IN BOOLEAN          AutoFlushable,
    IN UINT8            PktSeq,
    IN UINT8            ChSel,
    IN UINT8            CodeType)
{
    PUCHAR                  pData;
    ULONG                   TxCpuIdx;
    PTXD_STRUC              pTxD;
    PTXBI_STRUC             pTxBI;
    PRT_TX_RING             pTxRing;
    NATIVE_BT_CLK_STRUC     NativeBtClk;
    UINT8                   TxRingSize = BthGetTxRingSize(pAd, RingIdx);

    NativeBtClk.word = 0;
    RT_IO_READ32(pAd, NATIVE_BT_CLK, &NativeBtClk.word);

    pTxRing = &pAd->TxRing[RingIdx];
    TxCpuIdx = pTxRing->TxCpuIdx;

    pTxD  = (PTXD_STRUC) pTxRing->Cell[TxCpuIdx].AllocVa;
    pData = (PUCHAR)pTxRing->Cell[TxCpuIdx].DmaBuf.AllocVa;
    pTxBI = (PTXBI_STRUC)pData;

    pTxD->DmaDone = 1;  // Ben: disable DMA
    pTxD->LastSec0 = 1;
    pTxD->LastSec1 = 1;
    pTxD->QSEL = RingIdx;
    pTxD->SDLen0 = ((sizeof(TXBI_STRUC) + Length + 3) / 4) * 4; // Word boundary
    pTxD->SDLen1 = 0;
    pTxD->SwTestForAssignRxRing = 0;

    RtlZeroMemory(pTxBI, sizeof(TXBI_STRUC));
    pTxBI->Len = Length;
    pTxBI->Type = PktType & 0xf;
    pTxBI->Ptt = (PktType >> 4) & 0x3;
    pTxBI->Llid = Llid;
    pTxBI->PFlow = PFlow;
    pTxBI->BtClk = NativeBtClk.field.NativeBtClk;
    pTxBI->LmpTag = Tag;
    pTxBI->AF = AutoFlushable;
    pTxBI->PktSeq = PktSeq;
    /* Use LLIdx to set ChSel */
    //pTxBI->ChSel = ChSel;
    pTxBI->ChSel = (LLIdx >= SYNC_LOGICAL_LINK && LLIdx < ACLU_LOGICAL_LINK) ? (LLIdx - SYNC_LOGICAL_LINK) : ChSel;
    pTxBI->CodecType = CodeType;

    pData += sizeof(TXBI_STRUC);

    if (Length > 0)
    {
        RtlCopyMemory(pData, pPacket, Length);
    }

    INC_RING_INDEX(pTxRing->TxCpuIdx, TxRingSize);
    //INC_RING_INDEX(pTxRing->TxCpuIdx, TX_RING_SIZE);
    RT_IO_WRITE32(pAd, TX_CTX_IDX0 + RingIdx * RINGREG_DIFF,
                  pTxRing->TxCpuIdx);

    pTxD->DmaDone = 0;  // Ben: Kick twice
}



VOID
PDMA_Reset_TxRing(
    IN PRTBTH_ADAPTER   pAd,
    IN UINT8            TxRingIdx)
{
    UINT8                   Index, Count = 0;
 //   KIRQL                   oldIrql = 0;
    WPDMA_GLO_CFG_STRUC     GloCfg;
    PRT_TX_RING             pTxRing;
    PRTMP_DMABUF            pDmaBuf;
    PTXD_STRUC              pTxD;
    UINT8                   TxRingSize = BthGetTxRingSize(pAd, TxRingIdx);


    RTBT_ASSERT(TxRingIdx < NUM_OF_TX_RING);
    if (TxRingIdx >= NUM_OF_TX_RING)
    {
        DebugPrint(ERROR, DBG_HW_ACCESS, "PDMA_Reset_TxRing: Erorr ! TxRingIdx %d\n",
                   TxRingIdx);
        return;
    }

    do
    {
        RT_IO_READ32(pAd, WPDMA_GLO_CFG, &GloCfg.word);
        if (GloCfg.field.TxDMABusy == 0)
        {
            // Reset PDMA TX:DTX
            RT_IO_WRITE32(pAd, PDMA_RST_CSR, (1 << TxRingIdx));

            //RT_IO_WRITE32(pAd, TX_MAX_CNT0 + TxRingIdx * RINGREG_DIFF, TX_RING_SIZE);
            RT_IO_WRITE32(pAd, TX_MAX_CNT0 + TxRingIdx * RINGREG_DIFF, TxRingSize);
            RT_IO_WRITE32(pAd, TX_CTX_IDX0 + TxRingIdx * RINGREG_DIFF, 0);

            pTxRing = &pAd->TxRing[TxRingIdx];
            pTxRing->TxDmaIdx = 0;
            pTxRing->TxSwFreeIdx = 0;
            pTxRing->TxCpuIdx = 0;

            //for (Index = 0; Index < TX_RING_SIZE; Index++)
            for (Index = 0; Index < TxRingSize; Index++)
            {
                pDmaBuf = &pTxRing->Cell[Index].DmaBuf;
                pTxD = (PTXD_STRUC)pTxRing->Cell[Index].AllocVa;
                // initilize set DmaDone=1 first before enable DMA.
                pTxD->DmaDone = 1;
            }
        }

        if (GloCfg.field.TxDMABusy == 0)
        {
            break;
        }

        Count++;
        KeStallExecutionProcessor(50);
    }while(Count < 5);

    if (GloCfg.field.TxDMABusy)
    {
        DebugPrint(WARNING, DBG_HW_ACCESS, "PDMA_Reset_TxRing: Warning ! TxDMA busy(0x%08x)\n",
                   GloCfg.word);
    }

}


BOOLEAN
PDMA_Is_TxRing_Empty(
    IN PRTBTH_ADAPTER   pAd,
    IN UINT8            TxRingIdx)
{
    BOOLEAN         bQueuEmpty = TRUE, bPDMAEmpty = TRUE;
    PRT_TX_RING     pTxRing;
    ULONG           TxCpuIdx, TxSwFreeIdx, TxDmaIdx, QueueEmptyStatus;

    RTBT_ASSERT(TxRingIdx < NUM_OF_TX_RING);
    if (TxRingIdx >= NUM_OF_TX_RING)
    {
        DebugPrint(ERROR, DBG_HW_ACCESS, "PDMA_Check_TxRing_Status: Erorr ! TxRingIdx %d\n",
                   TxRingIdx);
        return TRUE;
    }

    pTxRing = &pAd->TxRing[TxRingIdx];

    TxCpuIdx = pTxRing->TxCpuIdx;
    TxSwFreeIdx = pTxRing->TxSwFreeIdx;
    TxDmaIdx = pTxRing->TxDmaIdx;

    if (TxCpuIdx != TxDmaIdx)
    {
        DebugPrint(WARNING, DBG_HW_ACCESS, "PDMA_Check_TxRing_Status: Warning ! TxRing %d, CpuIdx %d, DmaIdx %d, SwFreeIdx %d\n",
                   TxRingIdx, TxCpuIdx, TxDmaIdx, TxSwFreeIdx);

        bPDMAEmpty = FALSE;
    }

    RT_IO_READ32(pAd, Q_EMPTY_STATUS, &QueueEmptyStatus);

    if ((QueueEmptyStatus & (1 << TxRingIdx)) == 0)
    {
        // PBF not empty
        DebugPrint(WARNING, DBG_HW_ACCESS, "PDMA_Check_TxRing_Status: Warning ! TxRing %d, Queue not empty(0x%04x)\n",
                   TxRingIdx, QueueEmptyStatus);

        bQueuEmpty = FALSE;
    }

    if ((bPDMAEmpty == TRUE) && (bQueuEmpty == TRUE))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


ULONG
PDMA_Get_Txring_Freeno(
    IN PRTBTH_ADAPTER   pAd,
    IN UINT8            RingIdx)
{
    UINT8   TxRingSize = BthGetTxRingSize(pAd, RingIdx);

    return (pAd->TxRing[RingIdx].TxSwFreeIdx > pAd->TxRing[RingIdx].TxCpuIdx)    ?
            (pAd->TxRing[RingIdx].TxSwFreeIdx - pAd->TxRing[RingIdx].TxCpuIdx - 1)
             :
            (pAd->TxRing[RingIdx].TxSwFreeIdx + TxRingSize - pAd->TxRing[RingIdx].TxCpuIdx - 1);
}



void rtbth_us_pci32_write(unsigned long addr, unsigned int val){
    RT_IO_WRITE32(gpAd, addr, val);
}

void rtbth_us_pci32_read(unsigned long addr, unsigned int *pval){
    RT_IO_READ32(gpAd, addr, pval);
}

void rtbth_us_mcu_write(unsigned long addr, unsigned char *buf, unsigned short len){
    MCU_Write_Memory(gpAd, addr, buf, len);
}

void rtbth_us_mcu_read(unsigned long addr, unsigned char *buf, unsigned short len){
    MCU_Read_Memory(gpAd, addr, buf, len);
}

void rtbth_us_bb_write(unsigned char id, unsigned char val){
    RtbtWriteModemRegister(gpAd,id, val);
}

void rtbth_us_bb_read(unsigned char id, unsigned char *pval){
    RtbtReadModemRegister(gpAd,id, pval);
}

void rtbth_us_rf_write(unsigned char id, unsigned char val){
    BthWriteRFRegister(gpAd,id, val);
}

void rtbth_us_rf_read(unsigned char id, unsigned char *pval){
    BthReadRFRegister(gpAd,id, pval);
}

void rtbth_us_txring_tick(struct rtbth_tx_ctrl *tx_ctrl, unsigned char *buf){

    PDMA_Transmit_TxRing(gpAd, tx_ctrl->ll_idx, tx_ctrl->ring_idx, tx_ctrl->pkt_type, tx_ctrl->ll_id, tx_ctrl->pflow,
        tx_ctrl->tag, buf /*tx_ctrl->ppkt*/, tx_ctrl->len, tx_ctrl->auto_flushable, tx_ctrl->pkt_seq, tx_ctrl->ch_sel, tx_ctrl->code_type);
}

unsigned char rtbth_us_txring_isempty(unsigned char idx){
    return PDMA_Is_TxRing_Empty(gpAd, idx);
};

unsigned int rtbth_us_txring_free_cnt(unsigned char idx){
    return PDMA_Get_Txring_Freeno(gpAd, idx);
}
int rtbt_hci_dev_receive(void *bt_dev, int pkt_type, char *buf, int len);
int _rtbt_hci_dev_receive(void *bt_dev, int pkt_type, unsigned char *buf, int len){

    return rtbt_hci_dev_receive(bt_dev, pkt_type, buf, len);
}

long    rtbth_us_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

    int retval = 0;
    static unsigned char  buf[2048];
    int type =0;
  unsigned short len = 0;
    struct kfifo  *fifo;
    spinlock_t  *fifo_sp;
    unsigned char chk[2];
    unsigned short len_struct;
    unsigned long cpuflags;

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    if (_IOC_TYPE(cmd) != RTBTH_US_IOC_MAGIC)
        return -ENOTTY;

    if (_IOC_NR(cmd) > RTBTH_IOC_MAXNR)
        return -ENOTTY;
 //DebugPrint(TRACE, DBG_INIT,"ioctl request = %d\n", cmd);

    switch(cmd) {

        case RTBTH_IOCPCI32WRITE:

        do {
            struct rtbth_pci32write pci32wr;

	        if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&pci32wr, (void *)arg, sizeof(pci32wr))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }
          //  DebugPrint(ERROR, DBG_INIT,"RTBTH_IOCPCI32WRITE: addr =0x%08x, value = 0x%08x\n", pci32wr.addr, pci32wr.value);
            rtbth_us_pci32_write(pci32wr.addr , pci32wr.value);

        }while(0);

        break;

        case RTBTH_IOCPCI32READ:

        do {
            struct rtbth_pci32read pci32rd;
            unsigned int val;

            if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&pci32rd, (void *)arg, sizeof(pci32rd))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            rtbth_us_pci32_read(pci32rd.addr, &val);

            if (copy_to_user((void *)(arg + offsetof(struct rtbth_pci32read, value)), &val, sizeof(val)))
            {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                break;
            }
        }while(0);

        break;

        case RTBTH_IOCMCUWRITE:

        do {

            struct rtbth_mcuwrite mcuwr;

            if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&mcuwr, (void *)arg, sizeof(mcuwr))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            DebugPrint(TRACE, DBG_INIT,"RTBTH_IOCMCUWRITE: addr =0x%08x, len = %d\n", mcuwr.addr, mcuwr.len);

            if(mcuwr.len > 2048){
                DebugPrint(ERROR, DBG_INIT,"RTBTH_IOCMCUWRITE: mcuwr.len > 2048\n");
                retval = -EFAULT;
                break;
            }

            if (copy_from_user(&buf[0], (void *)(mcuwr.buf), mcuwr.len)) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            DebugPrint(TRACE, DBG_INIT,"RTBTH_IOCMCUWRITE: [0]=0x%02x [1]=0x%02x [2]=0x%02x [3]=0x%02x [4]=0x%02x [5]=0x%02x \n",
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

            rtbth_us_mcu_write(mcuwr.addr , &buf[0], mcuwr.len);

        }while(0);

        break;

        case RTBTH_IOCMCUREAD:

        do {
            struct rtbth_mcuread mcurd;

            if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&mcurd, (void *)arg, sizeof(mcurd))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            DebugPrint(TRACE, DBG_INIT,"RTBTH_IOCMCUREAD: addr =0x%08x, len = %d\n", mcurd.addr, mcurd.len);

            if(mcurd.len > 2048){
                DebugPrint(ERROR, DBG_INIT,"RTBTH_IOCMCUREAD: mcuwr.len > 2048\n");
                retval = -EFAULT;
                break;
            }

            rtbth_us_mcu_read(mcurd.addr, &buf[0], mcurd.len);

            if (copy_to_user((void *)(mcurd.buf), &buf[0], mcurd.len))
            {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                break;
            }
        }while(0);

        break;

        case RTBTH_IOCBBWRITE:

        do {
            struct rtbth_bbwrite bbwr;

            if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&bbwr, (void *)arg, sizeof(bbwr))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            rtbth_us_bb_write(bbwr.id, bbwr.val);
        }while(0);

        break;

        case RTBTH_IOCBBREAD:

        do {
            struct rtbth_bbread bbrd;
            unsigned char val;

            if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&bbrd, (void *)arg, sizeof(bbrd))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            rtbth_us_bb_read(bbrd.id, &val);

            if (copy_to_user((void *)(arg + offsetof(struct rtbth_bbread, val)), &val, sizeof(val)))
            {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                break;
            }
        }while(0);

        break;

        case RTBTH_IOCRFWRITE:

        do {
            struct rtbth_rfwrite rfwr;

            if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&rfwr, (void *)arg, sizeof(rfwr))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            rtbth_us_rf_write(rfwr.id, rfwr.val);
        }while(0);

        break;

        case RTBTH_IOCRFREAD:

        do {
            struct rtbth_rfread rfrd;
            unsigned char val;

            if (!capable(CAP_SYS_RAWIO)) {
                retval = -EPERM;
                break;
            }

            if (copy_from_user(&rfrd, (void *)arg, sizeof(rfrd))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            val = 0xbb;
            rtbth_us_rf_read(rfrd.id, &val);

            if (copy_to_user((void *)(arg + offsetof(struct rtbth_rfread, val)), &val, sizeof(val)))
            {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                break;
            }
        }while(0);

        break;

        case RTBTH_IOCBZWRITE:

        do {
            struct rtbth_bzwrite bzwr;

            if (copy_from_user(&bzwr, (void *)arg, sizeof(struct rtbth_bzwrite))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            //if(bzwr.type >= bz_num){
            //    DebugPrint(ERROR, DBG_INIT,"bzwr.type invalid (%d)\n", __LINE__, bzwr.type);
            //    retval = -EFAULT;
            //    break;
            //}

            if(bzwr.len>= 2048){
                DebugPrint(ERROR, DBG_INIT,"bzwr.len invalid (%d)\n", __LINE__, bzwr.len);
                retval = -EFAULT;
                break;
            }

            DebugPrint(TRACE, DBG_INIT,"RTBTH_IOCBZWRITE: type = %d, len = %d\n", bzwr.type, bzwr.len);

            if (copy_from_user(&buf, (void *)(bzwr.buf), bzwr.len)) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            //uslm todo: sean wang
            _rtbt_hci_dev_receive(gpAd, bzwr.type, &buf[0], bzwr.len);

        }while(0);

        break;

        case RTBTH_IOCBZREAD:

            do {

                int sz_need = 2 + 2 + 1;

                struct rtbth_bzread bzread;
                unsigned long buf_addr;

                memset(&bzread, 0, sizeof(bzread));

                if (copy_from_user(&bzread, (void *)arg, sizeof(bzread))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                    break;
                }

                if(bzread.type >= bz_num){
                     DebugPrint(ERROR, DBG_INIT,"bz read type invalid %d\n", type);
                    retval = -EFAULT;
                    break;
                }

                if(bzread.type == bz_hci){
                    fifo = gpAd->hci_fifo;
                    fifo_sp = &gpAd->hci_fifo_lock;
                }else if(bzread.type == bz_acl){
                    fifo = gpAd->acl_fifo;
                    fifo_sp = &gpAd->acl_fifo_lock;
                }else if(bzread.type == bz_sco){
                    fifo = gpAd->sco_fifo;
                    fifo_sp = &gpAd->sco_fifo_lock;
                }else {
                    DebugPrint(ERROR, DBG_INIT,"type invalid (%d)\n", type);
                    retval = -EFAULT;
                    break;
                }

                spin_lock_irqsave(fifo_sp,cpuflags);

                DebugPrint(TRACE, DBG_INIT,"bz type = %d, len of fifo = %d, len of buf = %d\n",bzread.type, kfifo_len(fifo), bzread.len_buf);
                if(kfifo_len(fifo) >= sz_need) {
                    kfifo_out(fifo, &chk[0] , sizeof(chk));
                    if(chk[0]!=0xcc && chk[1]!=0xcc){
                        DebugPrint(ERROR, DBG_INIT,"@@@@read bz data err: delimeter parsing fail, chk[0]=0x%02x, chk[1]=0x%02x\n", chk[0], chk[1]);
                        retval = -EFAULT;
                        spin_unlock_irqrestore(fifo_sp,cpuflags);
                        break;
                    }
                    kfifo_out(fifo, &len, sizeof(unsigned long));
                    if(len > 2048 || len < 1){
                        DebugPrint(ERROR, DBG_INIT,"@@@@read bz data err: len invalid (%d)\n", len);
                        retval = -EFAULT;
                        spin_unlock_irqrestore(fifo_sp,cpuflags);
                        break;
                    }
                    kfifo_out(fifo, buf, len);
                } else {
                    DebugPrint(ERROR, DBG_INIT,"@@@@read bz data err: data not available, kfifo_len (%d), sz_need = %d\n", kfifo_len(fifo), sz_need);
                    retval = -EFAULT;
                    spin_unlock_irqrestore(fifo_sp,cpuflags);
                    break;
                }
                spin_unlock_irqrestore(fifo_sp,cpuflags);

                if (copy_to_user((void *)(arg + sizeof(int) + sizeof(unsigned short)), &len, sizeof(unsigned short)))
                {
                    retval = -EFAULT;
                    break;
                }

                if (copy_from_user(&buf_addr, (void *)(arg + offsetof(struct rtbth_bzread, buf)), sizeof(unsigned char *))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                    break;
                }

                if (copy_to_user((void *)(buf_addr), &buf, len))
                {
                    retval = -EFAULT;
                    break;
                }
            }while(0);

        break;

        case RTBTH_IOCTRCTRL:

        do {
            struct rtbth_trctrl trctl;

            if (copy_from_user(&trctl, (void *)arg, sizeof(trctl))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            if(trctl.op > tr_tx_num){
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"trctl.op > tr_tx_num, trctl.op = %d\n", __LINE__, trctl.op);
                break;
            }

            if(trctl.len_struct > 2048){
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }

            DebugPrint(TRACE, DBG_INIT,"RTBTH_IOCTRCTRL: op = %d, len_struct = %d\n", trctl.op, trctl.len_struct);

            if(trctl.op == tr_tx){
            #if 1
                struct rtbth_tx_ctrl tx_ctrl;

                if (copy_from_user(&tx_ctrl, (void *)(trctl.payload), sizeof(tx_ctrl))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d, sizeof(tx_ctrl)=%d, len_struct=%d\n",
                        __LINE__, sizeof(tx_ctrl), sizeof(len_struct));
                    break;
                }

                if (copy_from_user(&buf[0], (void *)(tx_ctrl.ppkt), tx_ctrl.len)) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                    break;
                }

                rtbth_us_txring_tick( &tx_ctrl, &buf[0]);
            #else
             DebugPrint(TRACE, DBG_INIT,"op == tr_tx\n");
           #endif
            } else if (trctl.op== tr_tx_empty) {

                struct rtbth_tx_empty pempty;
                unsigned char isempty = 0;

                DebugPrint(TRACE, DBG_INIT,"op == tr_tx_empty\n");

                if (copy_from_user(&pempty, (void *)(trctl.payload), sizeof(pempty))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"111copy_from_user failed at %d\n", __LINE__);
                    break;
                }

                DebugPrint(TRACE, DBG_INIT,"pempty.ring_idx = %d, offsetof(struct rtbth_tx_empty,isempty)=%d\n",
                    pempty.ring_idx, offsetof(struct rtbth_tx_empty,isempty));

                #if 1
                    isempty = rtbth_us_txring_isempty(pempty.ring_idx);
                #else
                    isempty = 1;
                #endif

                if (copy_to_user((void *)(trctl.payload + offsetof(struct rtbth_tx_empty,isempty)), &isempty, sizeof(isempty))){
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"222copy_to_user failed at %d\n", __LINE__);
                    break;
                }
            } else if (trctl.op== tr_tx_free){

                struct rtbth_tx_frcnt pfrcnt ;
                unsigned short frcnt = 0;

                DebugPrint(TRACE, DBG_INIT,"op == tr_tx_free\n");

                if (copy_from_user(&pfrcnt, (void *)(trctl.payload), sizeof(pfrcnt))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                    break;
                }

                DebugPrint(TRACE, DBG_INIT,"pempty.ring_idx = %d, offsetof(struct rtbth_tx_empty,isempty)=%d\n",
                    pfrcnt.ring_idx, offsetof(struct rtbth_tx_frcnt, free_cnt ));

                #if 1
                    frcnt = rtbth_us_txring_free_cnt(pfrcnt.ring_idx);
                #else
                    frcnt = 0xdd;
                #endif

                if (copy_to_user((void *)(trctl.payload + offsetof(struct rtbth_tx_frcnt, free_cnt )), &frcnt, sizeof(frcnt))){
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                    break;
                }
            } else if (trctl.op== tr_rx) {


                RXBI_STRUC rxbi;
                unsigned int  pkt_len;
                unsigned long buf_addr;
                unsigned short idx;

                fifo = gpAd->rx_fifo;
                fifo_sp = &gpAd->rx_fifo_lock;

                spin_lock_irqsave(fifo_sp,cpuflags);
                //if(kfifo_len(fifo) >= sz_need) {
                    kfifo_out(fifo, &chk[0] , sizeof(chk));
                    if(chk[0]!=0xcc && chk[1]!=0xcc){
                        DebugPrint(ERROR, DBG_INIT,"@@@@rx packet: delimeter parsing fail, chk[0]=0x%02x, chk[1]=0x%02x\n", chk[0], chk[1]);
                        retval = -EFAULT;
                        spin_unlock_irqrestore(fifo_sp,cpuflags);
                        break;
                    }
                    kfifo_out(fifo, &rxbi, sizeof(rxbi));
                    kfifo_out(fifo, &pkt_len, sizeof(pkt_len));
                   // if(pkt_len > 2048 || pkt_len < sz_need){
                    if(pkt_len > 2048){
                        DebugPrint(ERROR, DBG_INIT,"@@@@rx packet err: len invalid (%d)\n", pkt_len);
                        retval = -EFAULT;
                        spin_unlock_irqrestore(fifo_sp,cpuflags);
                        break;
                    }
                    kfifo_out(fifo, buf, pkt_len);
                //} else {
                //    DebugPrint(ERROR, DBG_INIT,"@@@@read rx err: data not available, kfifo_len (%d),  sizeof(RXBI_STRUC)=%d, sizeof(unsigned int)=%d\n",
                //        kfifo_len(fifo),sizeof(RXBI_STRUC),sizeof(unsigned int)  );

               //     retval = -EFAULT;
               //     spin_unlock_irqrestore(fifo_sp,cpuflags);
               //     break;
               // }
                spin_unlock_irqrestore(fifo_sp,cpuflags);

                DebugPrint(TRACE, DBG_INIT,"op == tr_rx_packet, rxbi.Len=0x%x, rxbi.ScoSeq = 0x%x, pkt_len=%d, buf[0]=0x%x, buf[1]=0x%x, buf[2]=0x%x, buf[3]=0x%x, buf[4]=0x%x\n",
                    rxbi.Len, rxbi.ScoSeq, pkt_len, buf[0], buf[1], buf[2], buf[3], buf[4]);

                if (copy_to_user((void *)(trctl.payload + offsetof(struct rtbth_rx_ctrl, rxbi)), &rxbi, sizeof(rxbi))){
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                    break;
                }

                idx = 0;
                if (copy_to_user((void *)(trctl.payload + offsetof(struct rtbth_rx_ctrl, rdidx)), &idx, sizeof(idx))){
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                    break;
                }

                if (copy_to_user((void *)(trctl.payload + offsetof(struct rtbth_rx_ctrl, len_pkt)), &pkt_len, sizeof(pkt_len))){
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                    break;
                }

                if (copy_from_user(&buf_addr, (void *)(trctl.payload + offsetof(struct rtbth_rx_ctrl, pkt)), sizeof(unsigned char *))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                    break;
                }

                if (copy_to_user((void *)(buf_addr), &buf, pkt_len))
                {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_to_user failed at %d\n", __LINE__);
                    break;
                }
                break;
            } else {
                 DebugPrint(ERROR, DBG_INIT,"Invalid tx rx op at %d\n", __LINE__);
            }
        }while(0);


        break;

        case RTBTH_IOCSYNC:
            do {
                sync_wq_flag = 1;
                wake_up_interruptible(&sync_wq);
            }while(0);
        break;

        case RTBTH_IOCTST:
            #if 0
        do {
            unsigned int req;
            unsigned int res;

            if (copy_from_user(&req, (void *)arg, sizeof(req))) {
                retval = -EFAULT;
                DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                break;
            }
            res = req + 100;

            if(gpAd){
               mod_timer(&gpAd->tst_evt_timer_hci, jiffies + (10)/(1000/HZ));
               mod_timer(&gpAd->tst_evt_timer_acl, jiffies + (10)/(1000/HZ));
//               mod_timer(&gpAd->tst_evt_timer_sco, jiffies + (10)/(1000/HZ));
            }

            if (copy_to_user((void *)arg , &res, sizeof(res))) {
                retval = -EFAULT;
                break;
            }
        } while(0);
        #endif
        break;

        case RTBTH_IOCDMAC:
            do {
                struct rtbth_dmac dmac;

                if (copy_from_user(&dmac, (void *)arg, sizeof(dmac))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,
                               "copy_from_user failed at %d\n", __LINE__);
                    break;
                }
                DebugPrint(ERROR, DBG_INIT,
                           "RTBTH_IOCDMAC: dmac.dmac_op=%d\n", dmac.dmac_op);

                if(dmac.dmac_op == 0){
                    RtbtResetPDMA(gpAd);
                }

                if(dmac.dmac_op == 1){
                    BthEnableRxTx(gpAd);
                }

                if(dmac.dmac_op == 2){
                    DebugPrint(
                        TRACE, DBG_MISC, "%s:kfifo reset ==>\n", __func__);
                    kfifo_reset(gpAd->acl_fifo);
                    kfifo_reset(gpAd->hci_fifo);
                    kfifo_reset(gpAd->evt_fifo);
                    kfifo_reset(gpAd->sco_fifo);
                    kfifo_reset(gpAd->rx_fifo);
                    DebugPrint(
                        TRACE, DBG_MISC, "%s:kfifo reset <== \n", __func__);
                }

                if(dmac.dmac_op > 2){
                    DebugPrint(
                        ERROR, DBG_INIT, "No such dmac op=%d\n", dmac.dmac_op);
                }
            } while(0);
            break;
#if 1
        case RTBTH_IOCRMODE:
            do {
                int mode = 0;

                if (copy_from_user(&mode, (void *)arg, sizeof(int))) {
                    retval = -EFAULT;
                    DebugPrint(ERROR, DBG_INIT,"copy_from_user failed at %d\n", __LINE__);
                    break;
                }
                if(mode)
                    pollm = 1;
                else
                    pollm = 0;
                DebugPrint(INFO, DBG_INIT,"Set Poll Mode = %d\n", pollm);
            }while(0);
            break;
#endif
        default:
            retval = -EFAULT;
        break;
    }

    return retval;
}

unsigned int rtbth_us_poll(struct file *filp, poll_table *wait){

    unsigned int mask = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
    if (kfifo_size(gpAd->evt_fifo) == 0)
    {
        poll_wait(filp, &evt_wq,  wait);

        /* empty let select sleep */
        if((kfifo_size(gpAd->evt_fifo) != 0))
        {
            mask |= EPOLLIN | EPOLLRDNORM;  /* readable */
        }
    }
    else
    {
        mask |= EPOLLIN | EPOLLRDNORM;  /* readable */
    }

    mask |= EPOLLOUT | EPOLLWRNORM; /* writable */
#else
    if (kfifo_size(gpAd->evt_fifo) == 0)
    {
        poll_wait(filp, &evt_wq,  wait);

        /* empty let select sleep */
        if((kfifo_size(gpAd->evt_fifo) != 0))
        {
            mask |= POLLIN | POLLRDNORM;  /* readable */
        }
    }
    else
    {
        mask |= POLLIN | POLLRDNORM;  /* readable */
    }

    mask |= POLLOUT | POLLWRNORM; /* writable */
#endif

    return mask;

}

static struct file_operations rtbth_us_fops = {
    .open =    rtbth_us_open,
    .release = rtbth_us_close,
    .read =    rtbth_us_read,
    .write =   rtbth_us_write,
    .unlocked_ioctl = rtbth_us_unlocked_ioctl,
    .poll =    rtbth_us_poll
};


static int rtbth_us_devs = 1;        /* device count */
static int rtbth_us_maj = 192;       /* dynamic allocation */
static struct cdev rtbth_us_cdev;
#define RTBTH_US_DRIVER_NAME "rtbth_us_char"

int rtbth_bz_hci_send(void *pdata, void *buf, unsigned long len);
int rtbth_bz_acl_send(void *pdata, void *buf, unsigned long len);
int rtbth_bz_sco_send(void *pdata, void *buf, unsigned long len);
int rtbth_rx_packet(void *pdata, RXBI_STRUC rxbi, void *buf, unsigned int len);

int rtbth_us_init(void *pdata)
{
    RTBTH_ADAPTER *pAd = 0;
    dev_t dev = MKDEV(rtbth_us_maj, 0);
    int alloc_ret = 0;
    int cdev_err = 0;
    int i = 0;
    int ret = -1;

    alloc_ret = register_chrdev_region(dev, 1, RTBTH_US_DRIVER_NAME);
    if (alloc_ret) {
        DebugPrint(TRACE, DBG_INIT,"fail to register chrdev\n");
        return alloc_ret;
    }

    cdev_init(&rtbth_us_cdev, &rtbth_us_fops);
    rtbth_us_cdev.owner = THIS_MODULE;

    cdev_err = cdev_add(&rtbth_us_cdev, dev, rtbth_us_devs);
    if (cdev_err)
        goto error;

    DebugPrint(TRACE, DBG_INIT,"%s driver(major %d) installed.\n", RTBTH_US_DRIVER_NAME, rtbth_us_maj);

    //return 0;
#if 1

    pAd = (RTBTH_ADAPTER *)pdata;
    gpAd = pAd;

    osal_unsleepable_lock_init(&pAd->wq_spinlock);

    osal_event_init(&pAd->STPd_event);
    RB_INIT(&pAd->rFreeOpQ,   STP_OP_BUF_SIZE);
    RB_INIT(&pAd->rActiveOpQ, STP_OP_BUF_SIZE);
    /* Put all to free Q */
    for (i = 0; i < STP_OP_BUF_SIZE; i++)
    {
         osal_signal_init(&(pAd->arQue[i].signal));
         _rtbth_us_put_op(pAd, &pAd->rFreeOpQ, &(pAd->arQue[i]));
    }
    pAd->PSMd.pThreadData = (VOID *)pAd;
    pAd->PSMd.pThreadFunc = (VOID *)_rtbth_us_proc;

#define RTBTH_US_THREAD_NAME "rtbth_us"
    memcpy(pAd->PSMd.threadName, RTBTH_US_THREAD_NAME, strlen(RTBTH_US_THREAD_NAME));

    ret = osal_thread_create(&pAd->PSMd);
    if (ret < 0)
    {
        DebugPrint(TRACE, DBG_INIT,"osal_thread_create fail...\n");
        goto ERR_EXIT5;
    }

    //init_waitqueue_head(&pAd->wait_wmt_q);
#define STP_PSM_WAIT_EVENT_TIMEOUT        6000
    pAd->wait_wmt_q.timeoutValue = STP_PSM_WAIT_EVENT_TIMEOUT;
    osal_event_init(&pAd->wait_wmt_q);

    //Start STPd thread
    ret = osal_thread_run(&pAd->PSMd);
    if(ret < 0)
    {
         DebugPrint(TRACE, DBG_INIT,"osal_thread_run FAILS\n");
        goto ERR_EXIT6;
    }

#define SZ_EVT_FIFO 64*4        //entry * entry size
#define SZ_HCI_FIFO 64*512
#define SZ_ACL_FIFO 64*512
#define SZ_SCO_FIFO 64*512
#define SZ_RX_FIFO  64*512

    spin_lock_init(&pAd->evt_fifo_lock);
    spin_lock_init(&pAd->hci_fifo_lock);
    spin_lock_init(&pAd->acl_fifo_lock);
    spin_lock_init(&pAd->sco_fifo_lock);

    pAd->evt_fifo = kzalloc(sizeof(struct kfifo), GFP_ATOMIC);
    ret = kfifo_alloc(pAd->evt_fifo, SZ_EVT_FIFO, GFP_ATOMIC);
    if(ret < 0){
       //add error handling
    }

    pAd->hci_fifo = kzalloc(sizeof(struct kfifo), GFP_ATOMIC);
    ret = kfifo_alloc(pAd->hci_fifo, SZ_HCI_FIFO, GFP_ATOMIC);
    if(ret < 0){
       //add error handling
    }

    pAd->acl_fifo = kzalloc(sizeof(struct kfifo), GFP_ATOMIC);
    ret = kfifo_alloc(pAd->acl_fifo, SZ_ACL_FIFO, GFP_ATOMIC);
    if(ret < 0){
       //add error handling
    }

    pAd->sco_fifo = kzalloc(sizeof(struct kfifo), GFP_ATOMIC);
    ret = kfifo_alloc(pAd->sco_fifo, SZ_SCO_FIFO, GFP_ATOMIC);
    if(ret < 0){
       //add error handling
    }

    pAd->rx_fifo = kzalloc(sizeof(struct kfifo), GFP_ATOMIC);
    ret = kfifo_alloc(pAd->rx_fifo, SZ_RX_FIFO, GFP_ATOMIC);
    if(ret < 0){
       //add error handling
    }

    return 0;

ERR_EXIT6:

    ret = osal_thread_destroy(&pAd->PSMd);
    if(ret < 0)
    {
         DebugPrint(TRACE, DBG_INIT,"osal_thread_destroy FAILS\n");
        goto ERR_EXIT5;
    }

ERR_EXIT5:
    return 0;
#endif

error:
    if (cdev_err == 0)
        cdev_del(&rtbth_us_cdev);

    if (alloc_ret == 0)
        unregister_chrdev_region(dev, rtbth_us_devs);

    return 0;
}

int rtbth_us_deinit(void *pdata)
{
    RTBTH_ADAPTER *pAd = 0;
    dev_t dev = 0;

#if 1
    int ret = -1;

    DebugPrint(TRACE, DBG_INIT,"psm deinit\n");

    pAd = (RTBTH_ADAPTER *)pdata;

//    del_timer_sync(&pAd->tst_evt_timer_hci);
//    del_timer_sync(&pAd->tst_evt_timer_acl);
//    del_timer_sync(&pAd->tst_evt_timer_sco);

    if(!pAd){
        return -1;
    }

    kfifo_free(pAd->evt_fifo);
    kfree(pAd->evt_fifo);
    kfifo_free(pAd->hci_fifo);
    kfree(pAd->hci_fifo);
    kfifo_free(pAd->acl_fifo);
    kfree(pAd->acl_fifo);
    kfifo_free(pAd->sco_fifo);
    kfree(pAd->sco_fifo);
    kfifo_free(pAd->rx_fifo);
    kfree(pAd->rx_fifo);

    ret = osal_thread_destroy(&pAd->PSMd);
    if(ret < 0)
    {
         DebugPrint(TRACE, DBG_INIT,"osal_thread_destroy FAILS\n");
    }
    osal_unsleepable_lock_deinit(&pAd->wq_spinlock);
#endif

    dev = MKDEV(rtbth_us_maj, 0);

    cdev_del(&rtbth_us_cdev);
    unregister_chrdev_region(dev, rtbth_us_devs);

    DebugPrint(TRACE, DBG_INIT,"%s driver removed.\n", RTBTH_US_DRIVER_NAME);

    return 0;
}
