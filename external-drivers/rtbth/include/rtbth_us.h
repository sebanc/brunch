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

#ifndef _RTBTH_US_H
#define _RTBTH_US_H

//
// event type
//
typedef enum _RTBTH_EVENT_T {
    BZ_HCI_EVENT = 0,
    BZ_ACL_EVENT,
    BZ_SCO_EVENT,
    INIT_COREINIT_EVENT,
    INIT_COREDEINIT_EVENT,
    INIT_CORESTART_EVENT,
    INIT_CORESTOP_EVENT,
    INIT_EPINIT_EVENT,
    INT_RX_EVENT,
    INT_TX_EVENT,
    INT_MCUCMD_EVENT,
    INT_MCUEVT_EVENT,
    INT_LMPTMR_EVENT,
    INT_LMPERR_EVENT,
    RTBTH_TEST_EVENT,
    RTBTH_EXIT,
    RTBTH_EVENT_NUM
} RTBTH_EVENT_T;

//
// ioctl code
//
#if 1
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
#define RTBTH_IOCDMAC       _IOWR(RTBTH_US_IOC_MAGIC,14, int)
#define RTBTH_IOCRMODE      _IOWR(RTBTH_US_IOC_MAGIC,15, int)

#define RTBTH_IOC_MAXNR 16
#endif
//
// ioctl op struct
//
typedef enum _bz_type_t {
    bz_hci = 0,
    bz_acl,
    bz_sco,
    bz_num,
} bz_type_t;

typedef enum _tr_op_t {
    tr_tx = 0,
    tr_rx ,
    tr_tx_free,
    tr_tx_empty,
    tr_tx_num
} tr_op_t;

struct rtbth_pci32write {
	unsigned long  addr; /*input*/
	unsigned int value; /*input*/
} ;

struct rtbth_pci32read {
	unsigned long  addr; /*input*/
	unsigned int value; /*output*/
} ;

struct rtbth_mcuwrite {
	unsigned long  addr; /*input*/
	unsigned short len; /*input*/
	unsigned char *buf; /*input*/
} ;

struct rtbth_mcuread {
	unsigned long  addr; /*input*/
	unsigned short  len;  /*input*/
	unsigned char  *buf;  /*output*/
} ;

struct rtbth_bbwrite {
	unsigned char  id; /*input*/
	unsigned char  val;  /*input*/
} ;

struct rtbth_bbread {
	unsigned char  id;  /*input*/
	unsigned char  val;  /*output*/
} ;

struct rtbth_rfwrite {
	unsigned char  id;  /*input*/
	unsigned char  val;  /*input*/
} ;

struct rtbth_rfread {
	unsigned char  id; /*input*/
	unsigned char  val;  /*output*/
} ;

struct rtbth_bzwrite {
	bz_type_t       type; /*input*/
	unsigned short  len;  /*input*/
	unsigned char  *buf;  /*input*/
} ;

struct rtbth_bzread {
	//bz_type_t       type; /*output*/
    bz_type_t type;
    unsigned short  len_buf;   /*intput*/
	unsigned short  len_pkt;  /*output*/
	unsigned char   *buf;  /*output*/
} ;

struct rtbth_trctrl {
	tr_op_t  op; /*input*/
	unsigned short  len_struct; /*input*/
	void *payload; /*per operation*/
};

struct rtbth_dmac {
    int dmac_op;
};

struct rtbth_tx_ctrl {
	unsigned char ll_idx; /*input*/
	unsigned char ring_idx; /*input*/
	unsigned char pkt_type; /*input*/
	unsigned char ll_id; /*input*/
	unsigned char pflow; /*input*/
	unsigned short tag; /*input*/
	unsigned char *ppkt; /*input*/
	unsigned short len; /*input*/
	unsigned char auto_flushable; /*input*/
	unsigned char pkt_seq; /*input*/
	unsigned char ch_sel; /*input*/
	unsigned char code_type; /*input*/
};

typedef struct  _RXBI_STRUC {
    // Word 0
    unsigned int      Len:12;             // Received packet length
    unsigned int      Type:4;             // Received packet type
    unsigned int      Ptt:2;              // Packet type table.
                                    // [00]: SCO/ACL [01]: eSCO [10]: EDR ACL [11]: EDR eSCO
    unsigned int      FhsId:2;            // 0: Inquiry result, 1: page scan result, 2: role switch result
    unsigned int      BoundaryFlag:2;     // Packet boundary flag
    unsigned int      BroadcastFlag:2;    // Broadcast flag
                                    // [00]: point-to-point
                                    // [01]: Packet received as a slave not in park state (either Active Slave Broadcast or Parked Slave Broadcast)
                                    // [10]: Packet received as a slave in park state (Parked Slave Broadcast)
                                    // [11]: reserved
    unsigned int      Crc:1;              // 0: No error, 1: error
    unsigned int      PFlow:1;            // L2CAP FLOW bit
    unsigned int      Dven:1;             // indicate the packet is dv type
    unsigned int      CodecType:2;
    unsigned int      ChSel:2;            // select one of three CVSD channels
    unsigned int      QSel:1;             // DMA Rx queue selection
    // Word 1
    unsigned int      ChNo:7;             // the hopping channel number for this packet
    unsigned int      Rsv2:1;
    unsigned int      LlIdx:5;            // logical link index for this packet
    unsigned int      Rsv3:3;
    unsigned int      IntrSltCnt:11;      // the intraslot time of the beginning of the current packet
    unsigned int      Rsv4:5;
    //Word2
    unsigned int      BtClk:28;           // the BT clock of the beginning of the current packet
    unsigned int      Rsv5:4;
    //Word3
    unsigned int      Rssi:8;             // the received signal strength of the current packet
    unsigned int      Lna:2;              // the received signal LNA gain of the current packet
    unsigned int      Rsv6:14;
    unsigned int      ScoSeq:8;
}   RXBI_STRUC, *PRXBI_STRUC;

struct rtbth_rx_ctrl {
	RXBI_STRUC   rxbi;   /*output*/
	unsigned short  rdidx;  /*output*/
 	unsigned short  len_pkt;  /*output*/
	unsigned char *pkt;  /*output*/
};

struct rtbth_tx_frcnt {
	unsigned char  ring_idx;  /*input*/
	unsigned short free_cnt;  /*output*/
};

struct rtbth_tx_empty {
	unsigned char ring_idx;  /*input*/
	unsigned char isempty;   /*ouput*/
};

typedef enum _job_sync_t {
    sync_core_init = 0,
    sync_core_deinit,
    sync_core_start,
    sync_core_end,
    sync_core_epinit
} job_sync_t;

struct rtbth_sync {
	job_sync_t  job; /*input*/
} ;

//INT32 _rtbth_us_event_notification(RTBTH_ADAPTER *pAd, RTBTH_EVENT_T evt);

#endif

