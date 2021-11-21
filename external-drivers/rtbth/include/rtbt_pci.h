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

#ifndef __RTBT_PCI_H
#define __RTBT_PCI_H


//struct rtbt_dev_ctrl;
struct _RTBTH_ADAPTER; //sean wang linux
/*
	Interrupt related definitions
*/
#define DELAYINTMASK 0x400001


/* 
	Tx/Rx Ring 
*/
#define NUM_OF_TX_RING      		22

#define NUM_OF_ASBU_TX_RING			1
#define NUM_OF_PSBU_TX_RING			1
#define NUM_OF_PSBC_TX_RING			1
#define NUM_OF_SYNC_TX_RING			3
#define NUM_OF_ACLU_TX_RING			7
#define NUM_OF_ACLC_TX_RING			7
#define NUM_OF_LEACL_TX_RING			2

#define TX_RING_ASBU_IDX			0
#define TX_RING_PSBU_IDX			1
#define TX_RING_PSBC_IDX			2
#define TX_RING_SYNC_IDX			3	// 3 links
#define TX_RING_ACLU_IDX			6	// 7 links
#define TX_RING_ACLC_IDX			13	// 7 links
#define TX_RING_LEACL_IDX			20	// 2 links

/* Rx Ring */
#define NUM_OF_RX_RING      		2
//+++Add by shiang for debug in Linux 2.6.15
//#define RX_RING_SIZE        		80
#define RX_RING_SIZE        		40
//---Add by shiang for debug in Linux 2.6.15

//#define TX_RING_SIZE        		80
#define TX_RING_MAX_SIZE        	24
#define TX_ASBU_RING_SIZE        	2 	//4
#define TX_PSBU_RING_SIZE        	2 	//4
#define TX_PSBC_RING_SIZE        	4 	//4
#define TX_SYNC_RING_SIZE			24	//40
#define TX_ACLU_RING_SIZE			24	//40
#define TX_ACLC_RING_SIZE			8	//40
#define TX_LEACL_RING_SIZE			24

#define TX_ASBU_RING_BASE_PTR0_OFFSET		0
#define TX_PSBU_RING_BASE_PTR0_OFFSET		(TX_ASBU_RING_BASE_PTR0_OFFSET 		+ (TX_ASBU_RING_SIZE * TXD_SIZE))
#define TX_PSBC_RING_BASE_PTR0_OFFSET		(TX_PSBU_RING_BASE_PTR0_OFFSET 		+ (TX_PSBU_RING_SIZE * TXD_SIZE))
#define TX_SYNC_RING_BASE_PTR0_OFFSET_BASE	(TX_PSBC_RING_BASE_PTR0_OFFSET		+ (TX_PSBC_RING_SIZE * TXD_SIZE))
#define TX_ACLU_RING_BASE_PTR0_OFFSET_BASE	(TX_SYNC_RING_BASE_PTR0_OFFSET_BASE	+ (TX_SYNC_RING_SIZE * TXD_SIZE * NUM_OF_SYNC_TX_RING))
#define TX_ACLC_RING_BASE_PTR0_OFFSET_BASE	(TX_ACLU_RING_BASE_PTR0_OFFSET_BASE	+ (TX_ACLU_RING_SIZE * TXD_SIZE * NUM_OF_ACLU_TX_RING))
#define TX_LEACL_RING_BASE_PTR0_OFFSET_BASE	(TX_ACLC_RING_BASE_PTR0_OFFSET_BASE + (TX_ACLC_RING_SIZE * TXD_SIZE * NUM_OF_ACLC_TX_RING))

#define TX_RING_TOTAL_DESC_NUM	(	TX_ASBU_RING_SIZE * NUM_OF_ASBU_TX_RING			+ \
									TX_PSBU_RING_SIZE * NUM_OF_PSBU_TX_RING			+ \
									TX_PSBC_RING_SIZE * NUM_OF_PSBC_TX_RING			+ \
									TX_SYNC_RING_SIZE * NUM_OF_SYNC_TX_RING 		+ \
									TX_ACLU_RING_SIZE * NUM_OF_ACLU_TX_RING			+ \
									TX_ACLC_RING_SIZE * NUM_OF_ACLC_TX_RING			+ \
									TX_LEACL_RING_SIZE * NUM_OF_LEACL_TX_RING		)

#define RXD_SIZE            		16
#define TXD_SIZE            		16

#define TXBI_SIZE			16
#define RXBI_SIZE			16


/*
  Data buffer for DMA operation, the buffer must be contiguous physical memory
  Both DMA to / from CPU use the same structure.
*/
typedef struct  _RTMP_DMABUF
{
	UINT32				AllocSize;
	PVOID				AllocVa;		// TxBuf virtual address
	PHYSICAL_ADDRESS	AllocPa;		// TxBuf physical address
} RTMP_DMABUF, *PRTMP_DMABUF;

typedef struct _RT_DMACB
{
	UINT32				AllocSize;  	// Control block size
	PVOID				AllocVa;		// Control block virtual address
	PHYSICAL_ADDRESS	AllocPa;		// Control block physical address
	RTMP_DMABUF		DmaBuf; 	// Associated DMA buffer structure
} RT_DMACB, *PRT_DMACB;

typedef struct _RT_TX_RING
{
	//RT_DMACB	Cell[TX_RING_SIZE];
	RT_DMACB	Cell[TX_RING_MAX_SIZE];
	ULONG   	TxCpuIdx;
	ULONG   	TxDmaIdx;
	ULONG   	TxSwFreeIdx;	// software next free tx index
} RT_TX_RING, *PRT_TX_RING;

typedef struct _RT_RX_RING
{
	RT_DMACB	Cell[RX_RING_SIZE];
	ULONG   	RxCpuIdx;
	ULONG   	RxDmaIdx;
	ULONG   	RxSwReadIdx;	// software next read index
} RT_RX_RING, *PRT_RX_RING;


typedef struct rtbth_iface_pci{
	/* Following info are read from PCI config */
	UINT8	RevsionID;
	UINT16	VendorID;
	UINT16	DeviceID;
	UINT16	SubVendorID;
	UINT16	SubSystemID;

	/* */
	PVOID   				IoBaseAddress;  	// Read from PCI config
	ULONG   				IoRange;			// Read from PCI config
	ULONG   				InterruptVector;
	ULONG   				InterruptMode;
	UINT8   				InterruptLevel; 	// Read from PCI config
	PHYSICAL_ADDRESS	MemPhysAddress; 	// Read from PCI config
	ULONG   				MemRange;   		// Read from PCI config
	
}RTBTH_IFACE_PCI;


// TODO: shiang, for following two functions, I don't think that's a good way!!
#define RT_IO_READ32(_A, _R, _pV)   	\
do{	\
	UINT32	_vValue = 0;	\
	if (((_A)->btFunCtrl.word & 0x01) == 0x01)	\
	{	\
		RT_PCI_IO_READ32((ULONG)(((_A)->CSRAddress) + (_R)), (UINT32 *)(&_vValue));	\
		*(_pV) = (UINT32) _vValue;	\
	}	\
	else	\
	{	\
		_vValue = 0xffffffff;	\
		*(_pV) = (UINT32)_vValue;	\
	}	\
}while(0)


#define RT_IO_WRITE32(_A, _R, _V)		\
	do{	\
		if (((_A)->btFunCtrl.word & 0x01) == 0x01)	\
		{	\
			RT_PCI_IO_WRITE32((ULONG)(((_A)->CSRAddress) + (_R)), (UINT32)(_V));\
		}	\
	}while(0)


#define RT_IO_FORCE_READ32(_A, _R, _pV)		RT_PCI_IO_READ32((ULONG)(((_A)->CSRAddress) + (_R)), (UINT32 *)_pV)
#define RT_IO_FORCE_WRITE32(_A, _R, _V)		RT_PCI_IO_WRITE32((ULONG)(((_A)->CSRAddress) + (_R)), (UINT32)(_V))
	

UINT8 BthGetTxRingSize(
	IN struct _RTBTH_ADAPTER *pAd,//sean wang linux
	IN UINT8 RingIdx);

UINT16 BthGetTxRingPacketSize(
	IN struct _RTBTH_ADAPTER *pAd,//sean wang linux
	IN UINT8 RingIdx);

UINT32 BthGetTxRingOffset(
	IN struct _RTBTH_ADAPTER *pAd,//sean wang linux
	IN UINT8 RingIdx);


NTSTATUS BthInitSend(struct _RTBTH_ADAPTER *pAd);//sean wang linux
NTSTATUS BthInitRecv(struct _RTBTH_ADAPTER *pAd);//sean wang linux
int rtbt_pci_resource_init(struct rtbt_os_ctrl *os_ctrl);
int rtbt_pci_resource_deinit(struct rtbt_os_ctrl *os_ctrl);

VOID reg_dump_txdesc(struct _RTBTH_ADAPTER *pAd);//sean wang linux
VOID reg_dump_rxdesc(struct _RTBTH_ADAPTER *pAd);//sean wang linux
VOID RtbtResetPDMA(struct _RTBTH_ADAPTER *pAd);//sean wang linux

int rtbt_pci_isr(void *handle);
BOOLEAN BthEnableInterrupt(struct _RTBTH_ADAPTER *pAd);//sean wang linux
VOID BthDisableInterrupt(struct _RTBTH_ADAPTER *pAd);//sean wang linux
VOID BthHandleRecvInterrupt(struct _RTBTH_ADAPTER *pAd);//sean wang linux

BOOLEAN BthPacketFilter(
	struct _RTBTH_ADAPTER *pAd,//sean wang linux
	UCHAR   			RingIdx,
	ULONG   			RxDmaIdx,
	PULONG  			pSwReadIndex);



/**********************************************************************************
	PCI-E power saving related definitions
***********************************************************************************/
// TODO: Shiang, following data structure definitions are used for PCI-E power saving, need to revise!!

#define PCI_CFG_ADDR_PORT           0xcf8
#define PCI_CFG_DATA_PORT           0xcfc
// vendor ID
#define RICOH                       0x1180
#define O2MICRO                     0x1217
#define TI                          0x104c
#define RALINK                      0x1814
#define TOSHIBA                     0x1179
#define ENE                         0x1524
#define UNKNOWN                     0xffff

#define PCIBUS_INTEL_VENDOR           0x8086          // CardBus bridge class & subclass
#define PCIBUS_AMD_VENDOR1           0x1022          // One of AMD's Vendor ID
#define CARD_PCIBRIDGEPCI_CLASS           0x0604          // CardBus bridge class & subclass
#define CARD_BRIDGE_CLASS           0x0607          // CardBus bridge class & subclass


#define MAX_PCI_DEVICE              32      // support up to 32 devices per bus
#define MAX_PCI_BUS                 32      // support 10 buses
#define MAX_FUNC_NUM                8

typedef union _LINKCTRL_SETTING {
 struct {
         ULONG  HostFunc:16;
         ULONG  HostLinkCOffset:16;                // Link control offset in host
    } field;
 ULONG   word;
} LINKCTRL_SETTING, *PLINKCTRL_SETTING;

typedef union _PCIHOST_SETTING {
 struct {
         ULONG  VendorId:16;
         ULONG  DeviceId:16;
    } field;
 ULONG   word;
} PCIHOST_SETTING, *PPCIHOST_SETTING;

typedef union _PCI_SETTING {
 struct {
         ULONG  HostBus:16;
         ULONG  HostSlot:16;
    } field;
 ULONG   word;
} PCI_SETTING, *PPCI_SETTING;


// Power save method control
typedef	union	_PS_CONTROL	{
	struct	{
		ULONG		EnablePSinIdle:1;			// Enable radio off when not connect to AP. radio on only when sitesurvey,
		ULONG		EnableNewPS:1;				// Enable new  Chip power save fucntion . New method can only be applied in chip version after 2872. and PCIe.
		ULONG		rt30xxPowerMode:2;			// Power Level Mode for rt30xx chip
		ULONG		rt30xxFollowHostASPM:1;		// Card Follows Host's setting for rt30xx chip.
		ULONG		rt30xxForceASPMTest:1;		// Force enable L1 for rt30xx chip. This has higher priority than rt30xxFollowHostASPM Mode.
		ULONG		AMDNewPSOn:1;				// Enable for AMD L1 (toggle)
		ULONG		LedMode:2;			// 0: Blink normal.  1: Slow blink not normal.
		ULONG		rt30xxForceL0:1;				// Force only use L0 for rt30xx

		ULONG		rsv:22;			// Radio Measurement Enable
	}	field;
	ULONG			word;
}	PS_CONTROL, *PPS_CONTROL;


NTSTATUS
BthReInitSendDesc(
	struct _RTBTH_ADAPTER *pAd);//sean wang

NTSTATUS
BthReInitRecvDesc(
	struct _RTBTH_ADAPTER *pAd);//sean wang


VOID RTMPFindPCIePowerLinkCtrl(
	struct _RTBTH_ADAPTER *pAd);//sean wang

VOID RTMPrt3xSetPCIePowerLinkCtrl(
	struct _RTBTH_ADAPTER *pAd);//sean wang

ULONG RTMPReadCBConfig(
	IN	ULONG	Bus,
	IN	ULONG	Slot,
	IN	ULONG	Func,
	IN	ULONG	Offset);

VOID RTMPWriteCBConfig(
	IN	ULONG	Bus,
	IN	ULONG	Slot,
	IN	ULONG	Func,
	IN	ULONG	Offset,
	IN	ULONG	Value);

#endif // __RTBT_PCI_H //
