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

#ifndef __RTBT_CTRL_H__
#define __RTBT_CTRL_H__

#include "rtbt_type.h"
#include "rtbt_osabl.h"
#include "rtbt_pci.h"
#include "rtbth_us.h"

//#include "rtbt_pci.h"
//#include "bt_controller.h"
//#include "rtbt_io.h"
#include "rtbt_rf.h"
#include "rtbt_asic.h"
#include "rtbt_thread.h"
//#include <rtbth_chip.h>
#include <rtbth_chip.h>
#include "trace.h"

#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/completion.h>

/* Flags used for data structure rtbt_dev_ctrl::Flags */
#define fRTMP_ADAPTER_MAP_REGISTER		0x00000001
#define fRTMP_ADAPTER_INTERRUPT_IN_USE	0x00000002
#define fRTMP_ADAPTER_NIC_NOT_EXIST		0x00000004
#define fRT_ADAPTER_HALT_IN_PROGRESS		0x00000008


/* TX/RX RING */
#define TX_PACKET_SIZE      (16+1024)    // TXBI + 3DH5(1021 bytes) // TODO: ACLC only needs 17 bytes
#define RX_PACKET_SIZE      (16+1024+4)  // RXBI + 3DH5(1021 bytes) + 4(HW) // TODO: ACLC only needs 17 bytes
#define TX_ASBU_PACKET_SIZE			(16+1024)
#define TX_PSBU_PACKET_SIZE			(16+1024)
#define TX_PSBC_PACKET_SIZE			(16+24)
#define TX_SYNC_PACKET_SIZE			(16+540)	// PKT_3EV5_MAX_PAYLOAD_LEN +16
#define TX_ACLU_PACKET_SIZE			(16+1024)
#define TX_ACLC_PACKET_SIZE			(16+24)		//ACLC only needs 17 bytes
#define TX_LEACL_PACKET_SIZE		(16+28)		// LE ACL only needs max 27 bytes


#define NIC_MAX_PACKET_SIZE     	2304
#define NIC_MAX_PHYS_BUF_COUNT      8
#define NUM_OF_TCB              	512


/* General */
#define MILLISECONDS_TO_100NS   (10000)
#define MICROSECONDS_TO_100NS   (10)


typedef struct _RTBTH_ADAPTER RTBTH_ADAPTER;//sean wang linux
typedef struct _RTBTH_ADAPTER *PRTBTH_ADAPTER;//sean wang linux


typedef union rtbth_iface_info{
	RTBTH_IFACE_PCI pciInfo;
}RTBTH_IFACE_INFO;



typedef struct rtbth_chip_info{
	/*  Calibration access related callback functions */
	int (*eeinit)(RTBTH_ADAPTER *pAd);
	int (*eeread)(RTBTH_ADAPTER *pAd, USHORT offset, USHORT *pValue);
	int (*eewrite)(RTBTH_ADAPTER *pAd, USHORT offset, USHORT value);
}RTBTH_CHIP_INFO;

struct rtbt_hw_ops{
	int asic_init;
};

/////////////rtbth_us/////////////////////

#define OSAL_OP_DATA_SIZE   32
#define OSAL_OP_BUF_SIZE    64
#define MAX_THREAD_NAME_LEN 16
#define STP_OP_BUF_SIZE (16)

typedef struct _OSAL_THREAD_
{
    struct task_struct *pThread;
    void *pThreadFunc;
    void *pThreadData;
    char threadName[MAX_THREAD_NAME_LEN];
}OSAL_THREAD, *P_OSAL_THREAD;


typedef struct _OSAL_UNSLEEPABLE_LOCK_
{
    spinlock_t lock;
    unsigned long flag;
}OSAL_UNSLEEPABLE_LOCK, *P_OSAL_UNSLEEPABLE_LOCK;


typedef struct _OSAL_SLEEPABLE_LOCK_
{
    struct mutex lock;
}OSAL_SLEEPABLE_LOCK, *P_OSAL_SLEEPABLE_LOCK;

typedef struct _OSAL_SIGNAL_
{
    struct completion comp;
    unsigned int timeoutValue;
}OSAL_SIGNAL, *P_OSAL_SIGNAL;

typedef struct _OSAL_OP_DAT {
    unsigned int opId; // Event ID
    unsigned int u4InfoBit; // Reserved
    unsigned int au4OpData[OSAL_OP_DATA_SIZE]; // OP Data
} OSAL_OP_DAT, *P_OSAL_OP_DAT;


typedef P_OSAL_OP_DAT P_STP_OP;

typedef struct _OSAL_LXOP_ {
    OSAL_OP_DAT op;
    OSAL_SIGNAL signal;
    int result;
} OSAL_OP, *P_OSAL_OP;

typedef struct _OSAL_LXOP_Q {
    OSAL_SLEEPABLE_LOCK sLock;
    unsigned int write;
    unsigned int read;
    unsigned int size;
    P_OSAL_OP queue[OSAL_OP_BUF_SIZE];
} OSAL_OP_Q, *P_OSAL_OP_Q;

typedef struct _OSAL_EVENT_
{
    wait_queue_head_t waitQueue;
//    void *pWaitQueueData;
    unsigned int timeoutValue;
    int waitFlag;

}OSAL_EVENT, *P_OSAL_EVENT;

int rtbth_us_init(void *);
int rtbth_us_deinit(void *);


INT32 _rtbth_us_event_notification(RTBTH_ADAPTER *pAd, RTBTH_EVENT_T evt);

/*
	The adapter structure
*/
struct _RTBTH_ADAPTER//sean wang linux
{
	struct rtbt_os_ctrl *os_ctrl;
	
	/* 
		Hardware bus Interface related information
	*/
	int 					if_type;
	RTBTH_IFACE_INFO	if_info;
	
	/* PCI information resource variable */
	void  				*CSRAddress; // PCI MM/IO Base Address, all access will use
	UINT32				int_disable_mask;

	char					infName[RTBT_HOST_DEV_NAME_LEN];

	/*
		Chip related Information
	*/
	RTBTH_CHIP_INFO		chipInfo;
	ULONG				MACVersion;	/* MAC version */
	UINT8				RFVersion;	/* RF version */
	UCHAR				EEPROMVersion; // EEPROM version
	BOOLEAN				bUseEfuse;	/* use EFUSE or not */

	
	/* General control */
	ULONG   				Flags;
	
	struct rtbt_hw_ops *hw_ops;

	/* 
		resource for DMA operation 
	*/
	
	/* Shared memory for Tx descriptors */
	RTMP_DMABUF 		TxDescRing[NUM_OF_TX_RING];
	RTMP_DMABUF		TxDescMemPtr;
	RT_TX_RING  			TxRing[NUM_OF_TX_RING];

	/* Shared memory for RX descriptors */
	RTMP_DMABUF 		RxDescRing[NUM_OF_RX_RING];
	RT_RX_RING  			RxRing[NUM_OF_RX_RING];

	/* lock for tx/rx/ctrl */
	KSPIN_LOCK  			SendLock;
	KSPIN_LOCK  			RcvLock;
	
	KSPIN_LOCK  			LcCmdQueueSpinLock;
	KSPIN_LOCK  			LcEventQueueSpinLock;
	KSPIN_LOCK  			QueueSpinLock;
    KSPIN_LOCK              ChannelMapSpinLock;
        
	/* Protect miscellaneous data member */
	KSPIN_LOCK			MiscSpinLock;

	/* SCO relate parameters */
	KSPIN_LOCK			scoSpinLock;
	
	/*
		Related Control Blocks
	*/
#ifdef LINUX
	UINT16			sco_tx_cnt;
	PUCHAR			sco_event_buf;
	PUCHAR			sco_data_buf;
#endif /* LINUX */

	KDPC			core_idle_dpc;
	KTIMER			core_idle_timer;
	
	KEVENT  		CoreEventTriggered;	
	KEVENT  		HCIEventTriggered;
	KEVENT  		HCIACLDataEventTriggered;
	KEVENT  		HCISCODataEventTriggered;
		
	BOOLEAN			bMCUCmdSend;
	
#ifdef DBG
	ULONG   		HCIACLTxCount;
	ULONG			HCIACLRxCount;
#endif

	// Temp Use
	ULONG   		TxPacketNumForTest;
	
	//
	// SW will always copy bt_fun_ctrl value to variable
	//
//#ifdef DRV_WITH_LMP		
	BT_FUNC_STRUC	btFunCtrl;
//#endif
	
	// radio on/off setting
	BOOLEAN			bHwRadioOnSupport;
	BOOLEAN			bRadio; 				//current state
	BOOLEAN			bSwRadio; 				//current state
	BOOLEAN			bHwRadio;			// gpio pin state
	KDPC			RadioStateTimerDpc;
	KTIMER			RadioStateTimer;

    UINT16				RadioState;
	
#ifdef QUEUE_SANITY
	LARGE_INTEGER	ACLUIrpInSystemTime;
	LARGE_INTEGER	ACLUIrpOutSystemTime;
#endif


	UINT8			pNextQueriedACLULink;
	UINT8			pNextQueriedSyncLink;

	BOOLEAN			hwCodecSupport;
	BOOLEAN			bLESupported;
	BOOLEAN			bHwTimerSupported;
	
	//link control power use
	ULONG		Rt3xxHostLinkCtrl;	// USed for 3090F chip
	ULONG		Rt3xxRalinkLinkCtrl;	// USed for 3090F chip

	PS_CONTROL	PSControl;
	//ULONG 		BTFuncReg;
	USHORT		PCIePowerSaveLevel;
	USHORT		HBus;
	ULONG		HSubBus;
	USHORT		HSlot;
	USHORT		HFunc;
	ULONG		SubBusToRalink;

    OSAL_THREAD             PSMd;   /* main thread (wmtd) handle */
    OSAL_EVENT              STPd_event;
    
    OSAL_OP_Q               rFreeOpQ; /* free op queue */
    OSAL_OP_Q               rActiveOpQ; /* active op queue */
#define STP_OP_BUF_SIZE (16)    
    OSAL_OP                 arQue[STP_OP_BUF_SIZE]; /* real op instances */

    OSAL_EVENT                wait_wmt_q;

    OSAL_UNSLEEPABLE_LOCK   wq_spinlock;

//    struct timer_list  tst_evt_timer_hci;
//    struct timer_list  tst_evt_timer_acl;
//    struct timer_list  tst_evt_timer_sco;
    
    spinlock_t         evt_fifo_lock;
    spinlock_t         hci_fifo_lock;
    spinlock_t         acl_fifo_lock;
    spinlock_t         sco_fifo_lock;
    spinlock_t         rx_fifo_lock;
    
    struct kfifo       *evt_fifo;
    struct kfifo       *hci_fifo;
    struct kfifo       *acl_fifo;
    struct kfifo       *sco_fifo;
    struct kfifo       *rx_fifo;
};


#define RT_SET_FLAG(_M, _F) 	  ((_M)->Flags |= (_F))
#define RT_CLEAR_FLAG(_M, _F)     ((_M)->Flags &= ~(_F))
#define RT_CLEAR_FLAGS(_M)  	  ((_M)->Flags = 0)
#define RT_TEST_FLAG(_M, _F)	  (((_M)->Flags & (_F)) != 0)
#define RT_TEST_FLAGS(_M, _F)     (((_M)->Flags & (_F)) == (_F))


#define INC_RING_INDEX(_idx, _RingSize)    \
do{   									   \
	(_idx)++;   						   \
	if ((_idx) >= (_RingSize)) _idx=0;     \
}while(0)

static inline void dump_list(LIST_ENTRY *listHd)
{
	LIST_ENTRY *tmp;
	if (listHd && listHd->Flink)
		DebugPrint(TRACE, DBG_MISC, "Dump_list, Head=0x%x, Flink=0x%x, Blink=0x%x\n", listHd, listHd->Flink, listHd->Blink);
	else
		DebugPrint(TRACE, DBG_MISC, "Dump_list error: List not inited\n");

	tmp = listHd->Flink;
	while (tmp != listHd)
	{
		DebugPrint(TRACE, DBG_MISC, "Entry(0x%x)--Flink:0x%x, Blink:0x%x\n", tmp, tmp->Flink, tmp->Blink);
		tmp = tmp->Flink;
	};
}

enum LIST_OP{
	LIST_DO_NOTHING = 0,
	LIST_ADD_HEAD = 1,
	LIST_ADD_TAIL = 2,
	LIST_DEL_HEAD = 3,
	LIST_DEL_ENTRY = 4,
};

static char *list_op_str[] ={
	"NoOp",
	"InsertHeadList",
	"InsertTailList",
	"RemoveHeadList",
	"RemoveEntryList",
};

static inline void check_list_constant(LIST_ENTRY *listHd, enum LIST_OP op, LIST_ENTRY *entry, const char *func_str)
{
	LIST_ENTRY *ptr;
	int fcnt, bcnt, err = 0, stop_cnt = 0;
	
	if ((listHd->Flink == listHd || listHd->Blink == listHd) && 
		(op == LIST_ADD_HEAD || op == LIST_ADD_TAIL)) {
		DebugPrint(ERROR, DBG_MISC, "List Check Error in function(%s) for OpCode(%s), List still empty!\n", func_str, list_op_str[op]);
		err = 1;
		goto done;
	}

	ptr = listHd->Flink;
	fcnt = bcnt = 0;
	if (op == LIST_ADD_HEAD)
	{
		if ((ptr != entry) ||(entry->Blink != listHd))
		{
			DebugPrint(ERROR, DBG_MISC, "List Check Error in function(%s) for OpCode(%s), Not insert to the list head!\n", 
											func_str, list_op_str[op]);
			DebugPrint(ERROR, DBG_MISC, "\tHead=0x%x(F:0x%x, B:0x%x), add_entry=0x%x(F:0x%x, B:0x%x)!\n", 
											listHd, listHd->Flink, entry, entry->Flink, entry->Blink);
			err = 1;
			goto done;
		}
	}
	while ((ptr!= listHd) && (stop_cnt < 200))
	{
		if ((op == LIST_DEL_ENTRY) && (ptr == entry))
		{
			DebugPrint(ERROR, DBG_MISC, "List Check Error in function(%s) for OpCode(%s), Not deleted from the list!\n", 
											func_str, list_op_str[op]);
			DebugPrint(ERROR, DBG_MISC, "\tHead=0x%x(F:0x%x, B:0x%x), del_entry=0x%x(F:0x%x, B:0x%x)!\n", 
											listHd, listHd->Flink, entry, entry->Flink, entry->Blink, ptr);
			err = 1;
			goto done;
		}
		stop_cnt++;
		fcnt++;
		ptr = ptr->Flink;
	}

	if (stop_cnt >= 200) {
		DebugPrint(ERROR, DBG_MISC, "List Check Error in function(%s) for OpCode(%s), this list more than 200 entities when do forwarding check!\n", 
						func_str, list_op_str[op]);
		goto done;
	}
	
	stop_cnt = 0;
	ptr = listHd->Blink;
	if (op == LIST_ADD_TAIL)
	{
		if ((ptr != entry) ||(entry->Flink != listHd))
		{
			DebugPrint(ERROR, DBG_MISC, "List Check Error in function(%s) for OpCode(%s), Not insert to the list tail!\n", 
											func_str, list_op_str[op]);
			DebugPrint(ERROR, DBG_MISC, "\tHead=0x%x(F:0x%x, B:0x%x), add_entry=0x%x(F:0x%x, B:0x%x)!\n", 
											listHd, listHd->Flink, entry, entry->Flink, entry->Blink);
			err = 1;
			goto done;
		}
	}
	while((ptr != listHd) && (stop_cnt < 200))
	{
		stop_cnt++;
		bcnt++;
		ptr = ptr->Blink;
	}

	if (bcnt != fcnt) {
		DebugPrint(ERROR, DBG_MISC, "List Check Error in function(%s) for OpCode(%s), foward(%d) and backword(%d) not match!\n", 
					func_str, list_op_str[op], fcnt, bcnt);
		err = 1;
	}
	
	if (stop_cnt >= 200) {
		DebugPrint(ERROR, DBG_MISC, "List Check Error in function(%s) for OpCode(%s), this list more than 200 entities when do backward check!\n", 
					func_str, list_op_str[op]);
		goto done;
	}

done:
	if (err)
	{
		DebugPrint(ERROR, DBG_MISC, "List Check Error, Dump whole link list out!\n");
		dump_list(listHd);
	}
	
}

#define InitializeListHead(_list)	\
	do{\
		(_list)->Flink = (_list)->Blink = (_list);\
		check_list_constant(_list, LIST_DO_NOTHING, NULL, __FUNCTION__);\
	}while(0)

#define IsListEmpty(_listHd)\
	(((_listHd)->Flink == (_listHd)) && ((_listHd)->Blink == (_listHd)))

#define InsertHeadList(_listHd, _entry)	\
	do{\
		LIST_ENTRY *_tmp = (_listHd)->Flink;\
		(_entry)->Flink = _tmp;\
		(_entry)->Blink = (_listHd);\
		(_listHd)->Flink = (_entry);\
		_tmp->Blink = (_entry);\
		check_list_constant(_listHd, LIST_ADD_HEAD, _entry, __FUNCTION__);\
	}while(0)

#define InsertTailList(_listHd, _entry)	\
	do{	\
		LIST_ENTRY *_tmp = (_listHd)->Blink;\
		_tmp->Flink = (_entry);\
		(_entry)->Flink = (_listHd);\
		(_entry)->Blink = _tmp;\
		(_listHd)->Blink = (_entry);\
		check_list_constant(_listHd, LIST_ADD_TAIL, _entry, __FUNCTION__);\
	}while(0)

#define RemoveHeadList(_listHd)	\
	(((_listHd)->Flink == (_listHd)) ? NULL : (_listHd)->Flink);\
	do {\
			LIST_ENTRY *_tmp = (_listHd)->Flink;\
		if ( _tmp != (_listHd)) {\
			(_listHd)->Flink = _tmp->Flink;\
			if (_tmp->Flink == (_listHd))\
				(_listHd)->Blink = (_listHd);\
			else \
				(_tmp->Flink)->Blink = (_listHd);\
		}\
		check_list_constant(_listHd, LIST_DEL_HEAD, NULL, __FUNCTION__);\
	}while(0)

#define RemoveEntryList(_entry)\
	do{\
		if((_entry) && ((_entry)->Flink != (_entry)) &&\
		    ((_entry)->Blink != (_entry))) {\
			LIST_ENTRY *_b_entry = (_entry)->Blink;\
			LIST_ENTRY *_f_entry = (_entry)->Flink;\
			_b_entry->Flink = _f_entry;\
			_f_entry->Blink = _b_entry;\
			check_list_constant(_f_entry, LIST_DEL_ENTRY, _entry, __FUNCTION__);\
		}\
	}while(0)

void BthEnableRxTx(IN RTBTH_ADAPTER *pAd);

VOID BthRadioOff(
	IN	PRTBTH_ADAPTER pAd);

VOID BthRadioOn(
	IN	PRTBTH_ADAPTER pAd);

VOID Rtbth_Set_Radio_Led(
	IN PRTBTH_ADAPTER pAd,
	IN BOOLEAN Enable);

BOOLEAN RtbtReceivePacket(
	IN PRTBTH_ADAPTER pAd,
	IN UINT32 SwReadIndex,
	IN PRXBI_STRUC pRxBI,
	IN PUINT8 pRxPayload,
	IN ULONG RxPayloadLen);

VOID FillTxPacketForTest(
	IN PRTBTH_ADAPTER   pAd,
	IN UCHAR   	RingIdx);
	
PVOID RTBT_Alloc(IN INT	size);

#endif // __RTBT_CTRL_H__ //

