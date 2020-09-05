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

#ifndef __RT3298_IOMAP_H
#define __RT3298_IOMAP_H

//
// PCI registers - base address 0x0000
//
#define PCI_CFG         0x0000
#define PCI_EECTRL      0x0004
#define PCI_MCUCTRL     0x0008


#define RINGREG_DIFF    0x10
#define TX_BASE_PTR0    0x0000 //TX0 base address
#define TX_MAX_CNT0     0x0004
#define TX_CTX_IDX0     0x0008
#define TX_DTX_IDX0     0x000c

#define QID_ASBU        0
#define QID_PSBU        1
#define QID_PSBC        2
#define QID_SYNC        3   // 3 SCO
#define QID_ACLU        6   // 7 ACLU
#define QID_ACLC        13  // 7 ACLC
#define QID_LEACL       20


#define RX_BASE_PTR0    0x0180  //RX base address
#define RX_MAX_CNT      0x0184
#define RX_CRX_IDX      0x0188
#define RX_DRX_IDX      0x018c

#define QID_RX_HI       0   // ACLC
#define QID_RX_LOW      1   // ACLU/SCO/eSCO

#define WPDMA_GLO_CFG   0x0204
#ifdef RT_BIG_ENDIAN
typedef union   _WPDMA_GLO_CFG_STRUC    {
    struct  {
    }   field;
    UINT32          word;
}WPDMA_GLO_CFG_STRUC, *PWPDMA_GLO_CFG_STRUC;
#else
typedef union   _WPDMA_GLO_CFG_STRUC    {
    struct  {
        UINT32      EnableTxDMA:1;
        UINT32      TxDMABusy:1;
        UINT32      EnableRxDMA:1;
        UINT32      RxDMABusy:1;
        UINT32      WPDMABurstSIZE:1;
        UINT32      rsv:1;
        UINT32      EnTXWriteBackDDONE:1;
        UINT32      BigEndian:1;
        UINT32      :8;
        UINT32      RXHdrScater:13;
        UINT32      :3;
    }   field;
    UINT32          word;
} WPDMA_GLO_CFG_STRUC, *PWPDMA_GLO_CFG_STRUC;
#endif
#define PDMA_RST_CSR    0x208
#define DELAY_INT_CSR   0x20c
#define INT_SOURCE_CSR  0x220
#ifdef RT_BIG_ENDIAN
typedef union   _INT_SOURCE_CSR_STRUC   {
    struct  {
    }   field;
    UINT32          word;
}   INT_SOURCE_CSR_STRUC, *PINT_SOURCE_CSR_STRUC;
#else
typedef union   _INT_SOURCE_CSR_STRUC   {
    struct  {
        UINT32      TxDone:22;
        UINT32      RxDone:2;
        UINT32      McuEvtInt:1;
        UINT32      McuCmdInt:1;
        UINT32      LmpTmr0Int:1;
        UINT32      McuFErrorInt:1; /* MCU Fatal error or LMP_TIMER1 */
        UINT32      TxDelayDone:1;
        UINT32      TxCoherent:1;
        UINT32      RxDelayDone:1;
        UINT32      RxCoherent:1;
    }   field;
    UINT32          word;
} INT_SOURCE_CSR_STRUC, *PINT_SOURCE_CSR_STRUC;
#endif
#define INT_MASK_CSR        0x0228
//
// INT_MASK_CSR:   Interrupt MASK register.   1: the interrupt is mask OFF
//
#ifdef RT_BIG_ENDIAN
typedef union   _INT_MASK_CSR_STRUC {
    struct  {
    }   field;
    UINT32          word;
}INT_MASK_CSR_STRUC, *PINT_MASK_CSR_STRUC;
#else
typedef union   _INT_MASK_CSR_STRUC {
    struct  {
        UINT32      TxDone:22;
        UINT32      RxDone:2;
        UINT32      McuEvtInt:1;
        UINT32      McuCmdInt:1;
        UINT32      LmpTmr0Int:1;
        UINT32      McuFErrorInt:1; /* MCU fatal error or LMP_TIMER1 */
        UINT32      TxDelayDone:1;
        UINT32      TxCoherent:1;
        UINT32      RxDelayDone:1;
        UINT32      RxCoherent:1;
    }   field;
    UINT32          word;
} INT_MASK_CSR_STRUC, *PINT_MASK_CSR_STRUC;
#endif

#define ASIC_VER_CSR    0x300
#ifdef BIG_ENDIAN
typedef union   _ASIC_VER_ID_STRUC  {
    struct  {
        UINT16      ASICVer;        // version : 3290
        UINT16      ASICRev;        // reversion  : 2
    }   field;
    ULONG           word;
}   ASIC_VER_ID_STRUC, *PASIC_VER_ID_STRUC;
#else
typedef union   _ASIC_VER_ID_STRUC  {
    struct  {
        UINT16      ASICRev;        // reversion  : 2
        UINT16      ASICVer;        // version : 3290
    }   field;
    ULONG           word;
}   ASIC_VER_ID_STRUC, *PASIC_VER_ID_STRUC;
#endif

#define ASIC_VER_RT3290     0x3290

#define ASIC_REV_B          0x0002
#define ASIC_REV_C          0x0003
#define ASIC_REV_LE         0x0011  // Support LE feature

#define E2PROM_CSR      0x304

#define CMB_CTRL        0x320
#ifdef BIG_ENDIAN
typedef union   _CMB_CTRL_STRUC {
    struct {
        UINT32      LDO0En:1;
        UINT32      LDO3En:1;
        UINT32      LDOBGSel:2;
        UINT32      LDOCoreLevel:4;
        UINT32      PllLD:1;
        UINT32      XtalRdy:1;
        UINT32      Reserved:2;
        UINT32      LDO25FrcOn:1;
        UINT32      LDO25Largea:1;
        UINT32      LDO25Level:2;
        UINT32      AuxOpt:16;
    } field;
    UINT32          word;
}   CMB_CTRL_STRUC, *PCMB_CTRL_STRUC;
#else
typedef union   _CMB_CTRL_STRUC {
    struct {
        UINT32      AuxOpt:16;
        UINT32      LDO25Level:2;
        UINT32      LDO25Largea:1;
        UINT32      LDO25FrcOn:1;
        UINT32      Reserved:2;
        UINT32      XtalRdy:1;
        UINT32      PllLD:1;
        UINT32      LDOCoreLevel:4;
        UINT32      LDOBGSel:2;
        UINT32      LDO3En:1;
        UINT32      LDO0En:1;
    } field;
    UINT32          word;
}   CMB_CTRL_STRUC, *PCMB_CTRL_STRUC;
#endif

#define AUX_OPT_INTF_CLK_MASK   0x80


#define EFUSE_CTRL      0x324

#ifdef RT_BIG_ENDIAN
typedef union _EFUSE_CTRL_STRUC {
    struct {
        UINT32      SEL_EFUSE:1;
        UINT32      EFSROM_KICK:1;
        UINT32      RESERVED:4;
        UINT32      EFSROM_AIN:10;
        UINT32      EFSROM_LDO_ON_TIME:2;
        UINT32      EFSROM_LDO_OFF_TIME:6;
        UINT32      EFSROM_MODE:2;
        UINT32      EFSROM_AOUT:6;
    } field;
    UINT32          word;
}   EFUSE_CTRL_STRUC, *PEFUSE_CTRL_STRUC;
#else
typedef union _EFUSE_CTRL_STRUC {
    struct {
        UINT32      EFSROM_AOUT:6;
        UINT32      EFSROM_MODE:2;
        UINT32      EFSROM_LDO_OFF_TIME:6;
        UINT32      EFSROM_LDO_ON_TIME:2;
        UINT32      EFSROM_AIN:10;
        UINT32      RESERVED:4;
        UINT32      EFSROM_KICK:1;
        UINT32      SEL_EFUSE:1;
    } field;
    UINT32          word;
}   EFUSE_CTRL_STRUC, *PEFUSE_CTRL_STRUC;
#endif

#define EFUSE_USAGE_MAP_START   0x1e0
#define EFUSE_USAGE_MAP_END     0x1fd
//#define EFUSE_TAG               0x2fe
#define EFUSE_USAGE_MAP_SIZE    30
#define EXTERNAL_EEPROM         0
#define INTERNAL_EFFUSE_PROM    1

#define EFUSE_DATA0     0x328
#define EFUSE_DATA1     0x32C
#define EFUSE_DATA2     0x330
#define EFUSE_DATA3     0x334

#define COEXCFG0        0x340

#define RF_TEST         0x35c

#ifdef COEX_TEST
#ifdef RT_BIG_ENDIAN
typedef union  _RF_TEST_STRUC {
}   RF_TEST_STRUC, *PRF_TEST_STRUC;
#else
typedef union  _RF_TEST_STRUC {
    struct {
        UINT32      Rsv:14;
        UINT32      TxH:1;
        UINT32      RxH:1;
        UINT32      Rsv2:17;
    }   field;
    UINT32 word;
}   RF_TEST_STRUC, *PRF_TEST_STRUC;
#endif
#endif

#define WLAN_FUN_INFO   0x384
#ifdef RT_BIG_ENDIAN
    struct {
        UINT32      BT_EEP_BUSY:1;  // Read only for WLAN driver
        UINT32      Rsv1:25;
        UINT32      BtCoexMode:6;
    } field;
    UINT32 word;
#else
typedef union  _WLAN_FUN_INFO_STRUC {
    struct {
        UINT32      BtCoexMode:6;
        UINT32      Rsv1:25;
        UINT32      BT_EEP_BUSY:1;  // Read only for WLAN driver
    } field;
    UINT32 word;
}   WLAN_FUN_INFO_STRUC, *PWLAN_FUN_INFO_STRUC;
#endif

#define BT_FUN_CTRL 0x3c0
#ifdef BIG_ENDIAN
typedef union   _BT_FUNC_STRUC  {
}   BT_FUNC_STRUC, *PBT_FUNC_STRUC;
#else
typedef union   _BT_FUNC_STRUC  {
    struct  {
        ULONG BT_EN:1;
        ULONG BT_CLK_EN:1;
        ULONG BT_RF_EN:1;
        ULONG BT_RESET:1;
        ULONG PCIE_APP1_CLK_REQ:1;
        ULONG URXD_GPIO_MODE:1;
        ULONG INV_TR_SW1:1;
        ULONG BT_ACC_WLAN:1;
        ULONG GPIO1_IN:8;
        ULONG GPIO1_OUT:8;
        ULONG GPIO1_OUT_OE_N:8;
    }   field;
    ULONG           word;
} BT_FUNC_STRUC, *PBT_FUNC_STRUC;
#endif

#define OSCCTL          0x338

#ifdef BIG_ENDIAN
typedef union   _OSCCTL_STRUC   {
    struct  {
        ULONG       ROSC_EN:1;          //Ring oscilator enable
        ULONG       CAL_REQ:1;          //Ring oscilator calibration request
        ULONG       CLK_32K_VLD:1;      //Ring oscilator 32KHz output clock valid
        ULONG       CAL_ACK:1;          //Ring oscillator calibration ack
        ULONG       CAL_CNT:12;         //Ring oscillator calibration counter result
        ULONG       Rsv:3;
        ULONG       REF_CYCLE:13;       //Reference clock cycles to measure the clock divider's dividend
    }   field;
    ULONG           word;
}   OSCCTL_STRUC, *POSCCTL_STRUC;
#else
typedef union   _OSCCTL_STRUC   {
    struct  {
        ULONG       REF_CYCLE:13;       //Reference clock cycles to measure the clock divider's dividend
        ULONG       Rsv:3;
        ULONG       CAL_CNT:12;         //Ring oscillator calibration counter result
        ULONG       CAL_ACK:1;          //Ring oscillator calibration ack
        ULONG       CLK_32K_VLD:1;      //Ring oscilator 32KHz output clock valid
        ULONG       CAL_REQ:1;          //Ring oscilator calibration request
        ULONG       ROSC_EN:1;          //Ring oscilator enable
    }   field;
    ULONG           word;
} OSCCTL_STRUC, *POSCCTL_STRUC;
#endif

#ifdef COEX_TEST
#ifdef RT_BIG_ENDIAN
typedef union  _BT_FUNC_INFO_STRUC {
}   BT_FUNC_INFO_STRUC, *PBT_FUNC_INFO_STRUC;
#else
typedef union  _BT_FUNC_INFO_STRUC {
    struct {
        UINT32      BlackList:16;
        UINT32      CoexTxPwr:8;
        UINT32      CoexRssi:8;
    }   field;
    UINT32 word;
}   BT_FUNC_INFO_STRUC, *PBT_FUNC_INFO_STRUC;
#endif
#endif

#define BT_FUNC_INFO    0x3c4
#define LMP_TIMER0      0x3c8
#define LMP_TIMER1      0x3cc
#ifdef RT_BIG_ENDIAN
typedef union  _LMP_TIMER_STRUC {
}   LMP_TIMER_STRUC, *PLMP_TIMER_STRUC;
#else
typedef union  _LMP_TIMER_STRUC {
    struct {
        UINT32      Value:26;
        UINT32      Rsv:2;
        UINT32      Mode:1;
        UINT32      Load:1;
        UINT32      Irq:1;
        UINT32      En:1;
    }   field;
    UINT32 word;
}   LMP_TIMER_STRUC, *PLMP_TIMER_STRUC;
#endif

#define LMP_TIMER_AUTORELOAD_MODE   1
#define LMP_TIMER_SINGLESHOT_MODE   0


#define SYS_CTRL        0x400
#define PBF_CFG         0x404
#define BUF_CTRL        0x408
#ifdef RT_BIG_ENDIAN
typedef union  _BUF_CTRL_STRUC {
}   BUF_CTRL_STRUC, *PBUF_CTRL_STRUC;
#else
typedef union  _BUF_CTRL_STRUC {
    struct {
        UINT32      TxQRdSel:5;     // TX queue read selection
                                    // 0-22: Read Tx packet from TxQ#0 - TxQ#21
                                    // Others: Read Tx packet from shared memory (addressed by PKT_TX_OFFSET)
        UINT32      Rsv:3;
        UINT32      RxQWrSel:1;     // RX queue write selection
                                    // 0: Write Rx packet to packet memory
                                    // 1: Write Rx packet to shared memory (addressed by PKT_RX_OFFSET)
        UINT32      Rsv2:7;
        UINT32      RxMode:2;       // Packet buffer Rx control mode
                                    // 00: Manual mode
                                    // 01: Auto Rx commit mode
                                    // 10: Auto Rx drop mode
                                    // 11: Reserved
        UINT32      TxMode:1;       // Packet buffer Tx control mode
                                    // 0: Manual mode
                                    // 1: Auto clear mode
        UINT32      Rsv3:13;
    }   field;
    UINT32 word;
}   BUF_CTRL_STRUC, *PBUF_CTRL_STRUC;
#endif

#define TXQ_CTRL        0x40c
#ifdef RT_BIG_ENDIAN
typedef union  _TXQ_CTRL_STRUC {
}   TXQ_CTRL_STRUC, *PTXQ_CTRL_STRUC;
#else
typedef union  _TXQ_CTRL_STRUC {
    struct {
        UINT32      TxQRdClr:1;     // Manual TXQ packet clear (by TXQ_FW_RD_SEL)
        UINT32      RxQWrCmt:1;     // Manual RxQ packet commit
        UINT32      RxQWrDrp:1;     // Manual RxQ packet drop
        UINT32      BufRst:1;       // Buffer reset
        UINT32      Rsv:4;
        UINT32      TxFwRdSel:5;    // FW controlled TXQ selection
        UINT32      Rsv2:3;
        UINT32      TxInfQOut:8;    // Starting page address of FW selected TXQ (by TXQ_FW_RD_SEL)
                                    // Physical address = 0x8000 + TX_INFQ_OUT*64
        UINT32      TxInfQEmpty:1;  // TXQ empty status of FW selected TXQ (by TXQ_FW_RD_SEL)
        UINT32      TxInfQFull:1;   // TXQ full status of FW selected TXQ (by TXQ_FW_RD_SEL)
        UINT32      Rsv3:6;
    }   field;
    UINT32 word;
}   TXQ_CTRL_STRUC, *PTXQ_CTRL_STRUC;
#endif

#define TXQ_PRIORITY    0x41c
#define RXQ_INF         0x440
#define Q_EMPTY_STATUS  0x444
#define Q_FULL_STATUS   0x448

//
// MCU interrupt status
//
#define PBF_INT_STA     0x410
#ifdef RT_BIG_ENDIAN
typedef union  _PBF_INT_STA_STRUC {
}   PBF_INT_STA_STRUC, *PPBF_INT_STA_STRUC;
#else
typedef union  _PBF_INT_STA_STRUC {
    struct {
        UINT32      HCmdInt:1;
        UINT32      DTxInt:1;
        UINT32      DRxInt:1;
        UINT32      LcCmdInt:1;
        UINT32      Rsv:28;
    }   field;
    UINT32 word;
}   PBF_INT_STA_STRUC, *PPBF_INT_STA_STRUC;
#endif

#define TXQ_PRIORITY    0x41c

//
// MCU_CMD_CSR: For embedded processorto interrupt HOST
//
#define MCU_CMD_CSR     0x420

//
// HOST_CMD_CSR: For HOST to interrupt embedded processor
//
#define HOST_CMD_CSR    0x424
#ifdef RT_BIG_ENDIAN
#else
typedef union  _HOST_CMD_STRUC {
    struct {
        UINT32      HostCommand:8;
        UINT32      Rsv:24;
    }   field;
    UINT32          word;
}   HOST_CMD_STRUC, *PHOST_CMD_STRUC;
#endif

#define AFH_CFG0_CSR    0x428
#ifdef RT_BIG_ENDIAN
#else
typedef union  _AFH_CFG_STRUC {
    struct {
        UINT32      Offset0:7;
        UINT32      Rsv:1;
        UINT32      Offset1:7;
        UINT32      Rsv1:1;
        UINT32      Offset2:7;
        UINT32      Rsv2:1;
        UINT32      Offset3:7;
        UINT32      Rsv3:1;
    }   field;
    UINT32          word;
}   AFH_CFG_STRUC, *PAFH_CFG_STRUC;
#endif

#define AFH_CFG1_CSR    0x42c

#define AFH_CFG_OFFSET  0x40

#define RX_INF          0x440
#define Q_EMPTY_STATUS  0x444
#define Q_FULL_STATUS   0x448


//
// LC Register (0x600 - 0x1000)
//
#define LC_BASE         0x600
#define REV_CODE        0x600

#define BD_ADDR_L_CSR   0x604
#define BD_ADDR_H_CSR   0x608
#define SYNC_L_CSR      0x60c
#define SYNC_H_CSR      0x610
#define FHS_PAYLOAD_0_CSR   0x614
#ifdef RT_BIG_ENDIAN
#else
typedef union  _FHS_PAYLOAD_0_STRUC {
    struct {
        UINT32      LtAddr:3;
        UINT32      Rsv:5;
        UINT32      SRMode:2;
        UINT32      Rsv1:6;
        UINT32      PageScanMode:3;
        UINT32      Rsv2:5;
        UINT32      Eir:1;
        UINT32      Rsv3:7;
    }   field;
    UINT32          word;
}   FHS_PAYLOAD_0_STRUC, *PFHS_PAYLOAD_0_STRUC;
#endif

#define FHS_PAYLOAD_1_CSR   0x618


#define PICOBTCLKREG_DIFF   0x10
#define NATIVE_BT_CLK       0x620
#ifdef RT_BIG_ENDIAN
#else
typedef union  _NATIVE_BT_CLK_STRUC {
    struct {
        UINT32      NativeBtClk:28;
        UINT32      Rsv:4;
    }   field;
    UINT32          word;
}   NATIVE_BT_CLK_STRUC, *PNATIVE_BT_CLK_STRUC;
#endif

#define PICO_INTRA_OFFSET   0x630
#ifdef RT_BIG_ENDIAN
#else
typedef union  _PICO_INTRA_OFFSET_STRUC {
    struct {
        UINT32      Offset:11;
        UINT32      Rsv:21;
    }   field;
    UINT32          word;
}   PICO_INTRA_OFFSET_STRUC, *PPICO_INTRA_OFFSET_STRUC;
#endif

#define PICO_BT_CLK_OFFSET  0x634
#ifdef RT_BIG_ENDIAN
#else
typedef union  _PICO_BT_CLK_OFFSET_STRUC {
    struct {
        UINT32      Rsv:2;
        UINT32      Offset:26;
        UINT32      Rsv1:4;
    }   field;
    UINT32          word;
}   PICO_BT_CLK_OFFSET_STRUC, *PPICO_BT_CLK_OFFSET_STRUC;
#endif

#define PICO_CUR_BT_CLK_OFFSET  0x638
#ifdef RT_BIG_ENDIAN
#else
typedef union  _PICO_CUR_BT_CLK_OFFSET_STRUC {
    struct {
        UINT32      Offset:28;
        UINT32      Rsv:4;
    }   field;
    UINT32          word;
}   PICO_CUR_BT_CLK_OFFSET_STRUC, *PPICO_CUR_BT_CLK_OFFSET_STRUC;
#endif

#define PICO_BT_CLK     0x63c
#ifdef RT_BIG_ENDIAN
#else
typedef union  _PICO_BT_CLK_STRUC {
    struct {
        UINT32      Clk:28;
        UINT32      Rsv:4;
    }   field;
    UINT32          word;
}   PICO_BT_CLK_STRUC, *PPICO_BT_CLK_STRUC;
#endif

#define TIMING_CONTROL_0    0x660
#define TIMING_CONTROL_1    0x664
#define TIMING_CONTROL_2    0x668
#define TIMING_CONTROL_3    0x66C
#define TIMING_CONTROL_4    0x670
#define TIMING_CONTROL_5    0x674

#define SPI_BUSY    0x690
#ifdef BIG_ENDIAN
typedef union   _SPI_BUSY_STRUC {
    struct {
    } field;
    UINT32           word;
} SPI_BUSY_STRUC, *PSPI_BUSY_STRUC;
#else
typedef union   _SPI_BUSY_STRUC {
    struct {
        UINT32       rsv:8;
        UINT32       SPIBusy:1;
        UINT32       :7;
        UINT32       RFChCmd:8;
        UINT32       :8;
    } field;
    UINT32           word;
} SPI_BUSY_STRUC, *PSPI_BUSY_STRUC;
#endif

#define SPI_TIME    0x694
#define SPI_RDATA   0x698
#ifdef BIG_ENDIAN
typedef union   _SPI_RDATA_STRUC {
    struct {
    } field;
    UINT32           word;
} SPI_RDATA_STRUC, *PSPI_RDATA_STRUC;
#else
typedef union   _SPI_RDATA_STRUC {
    struct {
        UINT32   SPIRData:8;
        UINT32   rsv:24;
    } field;
    UINT32           word;
} SPI_RDATA_STRUC, *PSPI_RDATA_STRUC;
#endif

#define SPI_CMD_DATA    0x69c
#ifdef BIG_ENDIAN
typedef union   _SPI_CMD_DATA_STRUC {
    struct {
    } field;
    UINT32           word;
} SPI_CMD_DATA_STRUC, *PSPI_CMD_DATA_STRUC;
#else
typedef union   _SPI_CMD_DATA_STRUC {
    struct {
        UINT32       WriteData:8;
        UINT32       Address:8;
        UINT32       :7;
        UINT32       RWSpiCmd:1;  // Read/Write bit in SPI Command, 1:Write, 0:Read
        UINT32       :7;
        UINT32       RWMode:1; // 1: Write: 0: Read
    } field;
    UINT32           word;
} SPI_CMD_DATA_STRUC, *PSPI_CMD_DATA_STRUC;
#endif

#define MDM_CTRL    0x6a0
//
// MDM_CTRL: Modem Control Register
//
#ifdef RT_BIG_ENDIAN
typedef union   _MDM_CTRL_STRUC {
    struct {
    } field;
    UINT32           word;
}   MDM_CTRL_STRUC, *PMDM_CTRL_STRUC;
#else
typedef union   _MDM_CTRL_STRUC {
    struct {
        UINT32      MdmData:8;
        UINT32      MdmAddr:8;
        UINT32      rsv:15;
        UINT32      MdmWrite:1;
    }   field;
    UINT32 word;
}   MDM_CTRL_STRUC, *PMDM_CTRL_STRUC;
#endif

#define MDM_RDATA   0x6a4
//
// MDM_RDATA: Modem Register Read Data
//
#ifdef RT_BIG_ENDIAN
typedef union   _MDM_RDATA_STRUC {
    struct {
    } field;
    UINT32           word;
} MDM_RDATA_STRUC, *PMDM_RDATA_STRUC;
#else
typedef union   _MDM_RDATA_STRUC {
    struct {
        UINT32       MdmRData:8;
        UINT32       rsv:24;
    } field;
    UINT32           word;
} MDM_RDATA_STRUC, *PMDM_RDATA_STRUC;
#endif

//
// LC Register (0x600 - 0x1000)
//
#define PBF_LPK_RX_BI       0x700
#define PBF_LPK_CONTROL     0x704
#ifdef RT_BIG_ENDIAN
typedef union  _PBF_LPK_CTRL_STRUC {
}   PBF_LPK_CSR_STRUC, *PPBF_LPK_CTRL_STRUC;
#else
typedef union  _PBF_LPK_CTRL_STRUC {
    struct {
        UINT32      Len:12;     // The packet length when doing loopback in PBF
        UINT32      Rsv:4;
        UINT32      Enable:1;   // Enable the loopback test of PBF
        UINT32      Rsv2:7;
        UINT32      Mode:1;     // The PBF loopback mode selection. Set to one and use PBF_LPK_EN to start the test
        UINT32      Rsv3:7;
    }   field;
    UINT32 word;
}   PBF_LPK_CTRL_STRUC, *PPBF_LPK_CTRL_STRUC;
#endif

#define PRBS_SEED       0x724

#define RF_CTRL_CSR     0x728
#ifdef RT_BIG_ENDIAN
typedef union  _RF_CTRL_STRUC {
} RF_CTRL_STRUC, *PRF_CTRL_STRUC;
#else
typedef union  _RF_CTRL_STRUC {
    struct {
        UINT32      TRSWMode:1;
        UINT32      TRSW:1;
        UINT32      TxEnMode:1;
        UINT32      TxEn:1;
        UINT32      RxEnMode:1;
        UINT32      RxEn:1;
        UINT32      PAEnMode:1;
        UINT32      PAEn:1;
        UINT32      PLLEnMode:1;
        UINT32      PLLEn:1;
        UINT32      Rsv:6;
        UINT32      TxExtraCnt:5;
        UINT32      Rsv2:11;
    }   field;
    UINT32 word;
} RF_CTRL_STRUC, *PRF_CTRL_STRUC;
#endif

#define LT_INFO_BASE    0x730
#ifdef RT_BIG_ENDIAN
typedef union  _LT_INFO_STRUC {
}   LT_INFO_STRUC, *PLT_INFO_STRUC;
#else
typedef union  _LT_INFO_STRUC {
    struct {
        UINT32      LtAddr:3;
        UINT32      Rsv:5;
        UINT32      Arqn:1;
        UINT32      Seqn:1;
        UINT32      IFlow:1;
        UINT32      Rsv1:1;
        UINT32      RArqn:1;
        UINT32      RSeqn:1;
        UINT32      OFlow:1;
        UINT32      Rsv2:1;
        UINT32      PlIndex:4;
        UINT32      Rsv3:12;
    }   field;
    UINT32 word;
}   LT_INFO_STRUC, *PLT_INFO_STRUC;
#endif

#define AUX_INQ_CTL_CSR      0x790
#ifdef RT_BIG_ENDIAN
typedef union  _INQ_CTL_STRUC {
}   AUX_INQ_CTL_STRUC, *PAUX_INQ_CTL_STRUC;
#else
typedef union  _AUX_INQ_CTL_STRUC {
    struct {
        UINT8       ControlWord; // 0: Koffset, 1: RcvFHSFor1stID 4: RxEIRRequest,
                                 // 7: Owner
        UINT8       Nirq[2];
        UINT8       Rsv;
    }   field;
    UINT32 word;
}   AUX_INQ_CTL_STRUC, *PAUX_INQ_CTL_STRUC;
#endif

#define AUX_INQSCAN_CTL_CSR    0x798
#ifdef RT_BIG_ENDIAN
typedef union  _AUX_INQ_SCAN_CTL_STRUC {
}   AUX_INQSCAN_CTL_STRUC, *PAUX_INQSCAN_CTL_STRUC;
#else
typedef union  _AUX_INQ_SCAN_CTL_STRUC {
    struct {
        UINT8       ControlWord; // 0: Parrallel Scan 4: TxEIRRequest,
                                 // 5: ScanType, 6: ScanMode, 7: Owner
        UINT8       ResponseTimes;
        UINT16      ScanWindow;
    }   field;
    UINT32 word;
}   AUX_INQSCAN_CTL_STRUC, *PAUX_INQSCAN_CTL_STRUC;
#endif

#define AUX_PAGE_CTL_CSR        0x7a0
typedef struct _AUX_PAGE_CTL_STRUC {
    UINT8       ControlWord; // 1: Koffset, [4:6]:LtAddr
                             // 7: Owner
    UINT8       NpageLo;
    UINT8       NpageHi;
    UINT8       FrameIndex;
    UINT8       ClockOffsetLo;
    UINT8       ClockOffsetHi;
    UINT8       DeviceAddress[6];
    UINT8       DeviceSyncWord[8];
} AUX_PAGE_CTL_STRUC, *PAUX_PAGE_CTL_STRUC;

#define AUX_PAGESCAN_CTL_CSR   0x7b8
typedef union  _AUX_PAGESCAN_CTL_STRUC {
    struct {
        UINT8       ControlWord; // 0: Koffset, 5: ScanType, 6: ScanMode
                                 // 7: Owner
        UINT8       Rsv;
        UINT16      ScanWindow;
    }   field;
    UINT32 word;
}   AUX_PAGESCAN_CTL_STRUC, *PAUX_PAGESCAN_CTL_STRUC;

//
// Test Mode Registers (0x1C0 - 0x1D7)
//
#define TEST_MODE_CTL_CSR   0x7c0
#ifdef RT_BIG_ENDIAN
typedef union   _TEST_MODE_CTL0_CSR_STRUC {
    struct  {
    }   field;
    UINT32          word;
}TEST_MODE_CTL0_CSR_STRUC, *PTEST_MODE_CTL0_CSR_STRUC;
#else
typedef union   _TEST_MODE_CTL0_CSR_STRUC {
    struct  {
        UINT32      TblSel:1;
        UINT32      Rsv:31;
    }   field;
    UINT32          word;
} TEST_MODE_CTL0_CSR_STRUC, *PTEST_MODE_CTL0_CSR_STRUC;
#endif

#define TEST_MODE_TBL0_0_CSR  0x7c4
#ifdef RT_BIG_ENDIAN
typedef union   _TEST_MODE_TBL_0_CSR_STRUC {
    struct  {
    }   field;
    UINT32          word;
}TEST_MODE_TBL_0_CSR_STRUC, *PTEST_MODE_TBL_0_CSR_STRUC;
#else
typedef union   _TEST_MODE_TBL_0_CSR_STRUC {
    struct  {
        UINT32      Mode:2;
        UINT32      PatternOption:3;
        UINT32      FixedChannelMode:1;
        UINT32      EnableWhitening:1;
        UINT32      Owner:1;
        UINT32      PktType:6;
        UINT32      Rsv:2;
        UINT32      PktLength:10;
        UINT32      Rsv1:6;
    }   field;
    UINT32          word;
} TEST_MODE_TBL_0_CSR_STRUC, *PTEST_MODE_TBL_0_CSR_STRUC;
#endif

#define TEST_MODE_TBL0_1_CSR    0x7c8
#ifdef RT_BIG_ENDIAN
typedef union   _TEST_MODE_TBL_1_CSR_STRUC {
    struct  {
    }   field;
    UINT32          word;
}TEST_MODE_TBL_1_CSR_STRUC, *PTEST_MODE_TBL_1_CSR_STRUC;
#else
typedef union   _TEST_MODE_TBL_1_CSR_STRUC {
    struct  {
        UINT32      FixedTxChannel:7;
        UINT32      Rsv:1;
        UINT32      FixedRxChannel:7;
        UINT32      Rsv1:17;
    }   field;
    UINT32          word;
} TEST_MODE_TBL_1_CSR_STRUC, *PTEST_MODE_TBL_1_CSR_STRUC;
#endif

#define TEST_MODE_TBL1_0_CSR  0x7cc
#define TEST_MODE_TBL1_1_CSR  0x7d0
#define TEST_MODE_TBL_OFFSET  8
#define MAX_NUM_TEST_MODE_TBL 2

#define AFH_MAP_TABLE_ADDR  0x800

#define ECO_CTL_CSR         0x820
#ifdef RT_BIG_ENDIAN
typedef union   _ECO_CTL_CSR_STRUC {
} ECO_CTL_CSR_STRUC, *PECO_CTL_CSR_STRUC;
#else
typedef union   _ECO_CTL_CSR_STRUC {
    struct  {
        UINT32      EDRLen0CRCEn:1;
        UINT32      RSMResetEn:1;
        UINT32      HECErrBIEn:1;
        UINT32      Rsv1:5;
        UINT32      RSMState:4;
        UINT32      RFRxEn:1;
        UINT32      Rsv2:19;
    }   field;
    UINT32          word;
} ECO_CTL_CSR_STRUC, *PECO_CTL_CSR_STRUC;
#endif


#define AFH_MAP_TABLE_SIZE  64
#define NUM_AFH_MAP_TABLE   8

//
// Random Address (0x90c-0x913)
//
#define LE_RANDOM_ADDR_L    0x90c
#define LE_RANDOM_ADDR_H    0x910

//
// White List (0x924-0x92f)
//
#define LE_WHITE_LIST_CMD       0x924

#ifdef RT_BIG_ENDIAN
typedef union  _LE_WHITE_LIST_CMD_STRUC {
}   LE_WHITE_LIST_STRUC, *PLE_WHITE_LIST_CMD_STRUC;
#else
typedef union  _LE_WHITE_LIST_CMD_STRUC {
    struct {
        UINT32      Cmd:4;
        UINT32      Rsv:4;
        UINT32      CmdDone:1;
        UINT32      Rsv2:7;
        UINT32      CmdResult:1;
        UINT32      Rsv3:15;
    }   field;
    UINT32 word;
} LE_WHITE_LIST_CMD_STRUC, *PLE_WHITE_LIST_CMD_STRUC;
#endif

#define LE_WHILTE_LIST_CMD_CLR  0x01
#define LE_WHILTE_LIST_CMD_ADD  0x02
#define LE_WHILTE_LIST_CMD_DEL  0x04
#define LE_WHILTE_LIST_CMD_RD   0x08

#define LE_WHITE_LIST_ADDR_L    0x928
#define LE_WHITE_LIST_ADDR_H    0x92c

#ifdef RT_BIG_ENDIAN
typedef union  _LE_WHITE_LIST_ADDR_STRUC {
}   LE_WHITE_LIST_ADDR_STRUC, *PLE_WHITE_LIST_ADDR_STRUC;
#else
typedef union  _LE_WHITE_LIST_ADDR_STRUC {
    struct {
        UINT32      Addr:16;
        UINT32      AddrType:1;
        UINT32      Valid:1;
        UINT32      Rsv:6;
        UINT32      ReadAddr:8;
    }   field;
    UINT32 word;
} LE_WHITE_LIST_ADDR_STRUC, *PLE_WHITE_LIST_ADDR_STRUC;
#endif

#define MAX_LE_WHITE_LIST_ENTRY 8


//
// LE logical transport (0x930-0x94f)
//
#ifdef RT_BIG_ENDIAN
typedef union  _LE_LT_INFO_STRUC {
}   LE_LT_INFO_STRUC, *PLE_LT_INFO_STRUC;
#else
typedef union  _LE_LT_INFO_STRUC {
    struct {
        UINT32      TxHeader:8;
        UINT32      RxHeader:8;
        UINT32      PlIndex:4;
        UINT32      Rsv:4;
        UINT32      TxMIC:1;
        UINT32      TxNull:1;
        UINT32      RxMIC:1;
        UINT32      RXNull:1;
        UINT32      PreCRC:1;
        UINT32      NowCRC:1;
        UINT32      Rsv2:2;
    }   field;
    UINT32 word;
}   LE_LT_INFO_STRUC, *PLE_LT_INFO_STRUC;
#endif

#define LE_LOGICAL_TRANSPORT_0_INFO     0x930
#define LE_LOGICAL_TRANSPORT_1_INFO     0x934
#define LE_LOGICAL_TRANSPORT_2_INFO     0x938
#define LE_LOGICAL_TRANSPORT_3_INFO     0x93c
#define LE_LOGICAL_TRANSPORT_4_INFO     0x940
#define LE_LOGICAL_TRANSPORT_5_INFO     0x944
#define LE_LOGICAL_TRANSPORT_6_INFO     0x948
#define LE_LOGICAL_TRANSPORT_7_INFO     0x94c

#define LE_LATENCY_CONTROL              0x954
#ifdef RT_BIG_ENDIAN
typedef union  _LE_LATENCY_CONTROL_STRUC {
}   LE_LATENCY_CONTROL_STRUC, *PLE_LATENCY_CONTROL_STRUC;
#else
typedef union  _LE_LATENCY_CONTROL_STRUC {
    struct {
        UINT32      TxLatency:11;
        UINT32      Rsv:5;
        UINT32      RxLatency:11;
        UINT32      Rsv2:5;
    }   field;
    UINT32 word;
}   LE_LATENCY_CONTROL_STRUC, *PLE_LATENCY_CONTROL_STRUC;
#endif

#define MAC_CSR0            0x1000

#define PACKET_MEMORY_BASE  0x8000  // HST_PM_SEL=0
#define PACKET_MEMORY_SIZE  0x6000

#define FIRMWARE_IMAGE_BASE 0x8000  // HST_PM_SEL=1
#define LOW_PROGRAM_MEMORY_SIZE     0x8000  // 32k
#define HI_PROGRAM_MEMORY_SIZE      0x2000  // 8k
#define MAX_FIRMWAREIMAGE_LENGTH    (LOW_PROGRAM_MEMORY_SIZE+HI_PROGRAM_MEMORY_SIZE)

//
// TX descriptor format, Tx ring, Mgmt Ring
//
#ifdef RT_BIG_ENDIAN
typedef struct  _TXD_STRUC {
}   TXD_STRUC, *PTXD_STRUC;
#else
typedef struct  _TXD_STRUC {
    // Word 0
    UINT32      SDPtr0;
    // Word 1
    UINT32      SDLen1:14;
    UINT32      LastSec1:1;
    UINT32      SwTestForAssignRxRing:1;
    UINT32      SDLen0:14;
    UINT32      LastSec0:1;
    UINT32      DmaDone:1;
    //Word2
    UINT32      SDPtr1;
    //Word3
    UINT32      rsv2:24;
    UINT32      QSEL:5; // select on-chip FIFO ID
    UINT32      rsv3:3;
}   TXD_STRUC, *PTXD_STRUC;
#endif

//
// TXD Wireless Information format for Tx ring and Mgmt Ring
//
//txop : for txop mode
// 0:txop for the MPDU frame will be handles by ASIC by register
// 1/2/3:the MPDU frame is send after PIFS/backoff/SIFS
#ifdef RT_BIG_ENDIAN
typedef struct  _TXBI_STRUC {
}   TXBI_STRUC, *PTXBI_STRUC;
#else
typedef struct  _TXBI_STRUC {
    // Word 0
    UINT32      Len:12;         // transmit packet length
    UINT32      Type:4;         // transmit packet type
    UINT32      Ptt:2;          // packet type table.
                                //[00]: SCO/ACL [01]: eSCO [10]: EDR ACL [11]: EDR eSCO
    UINT32      Rsv0:2;
    UINT32      Llid:2;         // logical link identifier
    UINT32      Rsv1:3;
    UINT32      PFlow:1;        // L2CAP FLOW bit
    UINT32      Dven:1;         // indicate the packet is DV type. Set 0 for RT3290
    UINT32      CodecType:2;    // select one of a-law, u-law, CVSD or none. Set 0 for RT3290
    UINT32      ChSel:2;        // select one of three CVSD channels. Set 0 for RT3290
    UINT32      Rsv2:1;
    // Word 1
    UINT32      AF:1;           // automatic flushable
    UINT32      Rsv3:31;
    //Word2
    UINT32      BtClk:28;       // the time stamp for the beginning of the packet
    UINT32      Rsv4:4;
    //Word3
    UINT32      PktSeq:8;
    UINT32      LmpTag:8;       // The tags used by LMP for LC
    UINT32      Rsv5:16;
}   TXBI_STRUC, *PTXBI_STRUC;
#endif

#ifdef RT_BIG_ENDIAN
typedef struct  _LE_TXBI_STRUC {
}   LE_TXBI_STRUC, *PLE_TXBI_STRUC;
#else
typedef struct  _LE_TXBI_STRUC {
    // Word 0
    UINT32      Len:12;         // transmit packet length
    UINT32      Rsv:20;

    // Word 1
    UINT32      Rsv1;

    //Word2
    UINT32      BtClk:28;       // the time stamp for the beginning of the packet
    UINT32      Rsv2:4;

    //Word3
    UINT32      Rsv3:12;
    UINT32      LlcpTag:4;      // The tags used by LLCP for LC
    UINT32      PDUHeader:16;
}   LE_TXBI_STRUC, *PLE_TXBI_STRUC;
#endif


//
// Rx descriptor format, Rx Ring
//
#ifdef RT_BIG_ENDIAN
typedef struct  _RXD_STRUC  {
}   RXD_STRUC, *PRXD_STRUC;
#else
typedef struct  _RXD_STRUC  {
    // Word 0
    UINT32      SDP0;
    // Word 1
    UINT32      SDL1:14;
    UINT32      Rsv:2;
    UINT32      SDL0:14;
    UINT32      LS0:1;
    UINT32      DDONE:1;
    // Word 2
    UINT32      SDP1;
    // Word 3
    UINT32      Rsv1;
}   RXD_STRUC, *PRXD_STRUC;
#endif

#ifdef RT_BIG_ENDIAN
typedef struct  _LE_RXBI_STRUC {
}   LE_RXBI_STRUC, *PLE_RXBI_STRUC;
#else
typedef struct  _LE_RXBI_STRUC {
    // Word 0
    UINT32      Len:12;             // PDU size + CRC size + MIC size (if MIC exists)
    UINT32      Type:4;             // Received packet type
    UINT32      AdvTag:1;           // 0: Advertising report, 1: Adv before conn req
    UINT32      Rsv:7;
    UINT32      CRC:1;              // 1: CRC error
    UINT32      ADV:1;              // 1: Advertising packet, 0: Data packet
    UINT32      Rsv1:3;
    UINT32      ChSel:2;            // select one of three CVSD channels
    UINT32      QSel:1;             // DMA Rx queue selection
    // Word 1
    UINT32      ChNo:7;             // the hopping channel number for this packet
    UINT32      Rsv2:1;
    UINT32      LlIdx:5;            // logical link index for this packet
    UINT32      Rsv3:3;
    UINT32      IntrSltCnt:11;      // the intraslot time of the beginning of the current packet
    UINT32      Rsv4:5;
    //Word2
    UINT32      BtClk:28;           // the piconet clock
    UINT32      Rsv5:4;
    //Word3
    UINT32      Rssi:8;             // the received signal strength of the current packet
    UINT32      Lna:2;              // the received signal LNA gain of the current packet
    UINT32      Rsv6:2;
    UINT32      BBAck:4;
    UINT32      PDUHeader:16;       // the Length field in LE PDU Header is the PDU size
}   LE_RXBI_STRUC, *PLE_RXBI_STRUC;
#endif


#ifdef RT_BIG_ENDIAN
typedef struct  _LE_DATA_RXBI_STRUC {
}   LE_DATA_RXBI_STRUC, *PLE_DATA_RXBI_STRUC;
#else
typedef struct  _LE_DATA_RXBI_STRUC {
    // Word 0
    UINT32      Len:12;             // PDU size + CRC size + MIC size (if MIC exists)
    UINT32      Type:4;             // Received packet type
    UINT32      AdvTag:1;           // 0: Advertising report, 1: Adv before conn req
    UINT32      Rsv:7;
    UINT32      CRC:1;              // 1: CRC error
    UINT32      ADV:1;              // 1: Advertising packet, 0: Data packet
    UINT32      Rsv1:3;
    UINT32      ChSel:2;            // select one of three CVSD channels
    UINT32      QSel:1;             // DMA Rx queue selection

    // Word 1
    UINT32      ChNo:7;             // the hopping channel number for this packet
    UINT32      Rsv2:1;
    UINT32      LlIdx:5;            // logical link index for this packet
    UINT32      Rsv3:3;
    UINT32      EventCount:16;      // connEventCount

    //Word2
    UINT32      EventAnchor:28;     // Anchor point of event
    UINT32      Rsv5:4;

    //Word3
    UINT32      Rssi:8;             // the received signal strength of the current packet
    UINT32      Lna:2;              // the received signal LNA gain of the current packet
    UINT32      Rsv6:2;
    UINT32      BBAck:4;            // Baseband Ack
    UINT32      PDUHeader:16;       // the Length field in LE PDU Header is the PDU size
}   LE_DATA_RXBI_STRUC, *PLE_DATA_RXBI_STRUC;
#endif

#define FHSID_INQUIRY               0
#define FHSID_PAGE                  1
#define FHSID_ROLESWITCH            2

#define LNA_STATUS_HIGH             0
#define LNA_STATUS_MEDIUM           1
#define LNA_STATUS_LOW              2

#define LNA_BIAS                    50
#define LNA_GAIN_HIGH               38
#define LNA_GAIN_MEDIUM             18
#define LNA_GAIN_LOW                2

#define LE_RXBI_ADV_PACKET          1
#define LE_RXBIT_DATA_PACKET        0


//-------------------------------------------------------------------------
// EEPROM definition
//-------------------------------------------------------------------------
#define EESK                        0x01
#define EECS                        0x02
#define EEDI                        0x04
#define EEDO                        0x08

#define EEPROM_WRITE_OPCODE         0x05
#define EEPROM_READ_OPCODE          0x06
#define EEPROM_EWDS_OPCODE          0x10
#define EEPROM_EWEN_OPCODE          0x13

// RT3298 use 93C66
#define EEPROM_ADDRESS_NUM          8    //6: 93C46, 8:93C66/93C68

#define BUSY                        1
#define IDLE                        0
#define MAX_BUSY_COUNT              100  // Number of retry before failing access BBP & RF indirect register

#define EEPROM_VERSION              0x02
#define EEPROM_RF_VERSION           0xA4
#define EEPROM_HW_RADIO_SUPPORT     0x9a


//-------------------------------------------------------------------------
// RF sections
//-------------------------------------------------------------------------
#define RF_R00          0
#define RF_R01          1
#define RF_R02          2
#define RF_R03          3
#define RF_R04          4
#define RF_R05          5
#define RF_R06          6
#define RF_R07          7
#define RF_R08          8
#define RF_R09          9
#define RF_R10          10
#define RF_R11          11
#define RF_R12          12
#define RF_R13          13
#define RF_R14          14
#define RF_R15          15
#define RF_R16          16
#define RF_R17          17
#define RF_R18          18
#define RF_R19          19
#define RF_R20          20
#define RF_R21          21
#define RF_R22          22
#define RF_R23          23
#define RF_R24          24
#define RF_R25          25
#define RF_R26          26
#define RF_R27          27
#define RF_R28          28
#define RF_R29          29
#define RF_R30          30
#define RF_R39          39
#define RF_R40          40
#define RF_R65          65
#define RF_R66          66
#define RF_R78          78
#define RF_R79          79

//-------------------------------------------------------------------------
// BBP sections
//-------------------------------------------------------------------------
#define BBP_R0          0
#define BBP_R1          1
#define BBP_R2          2
#define BBP_R3          3
#define BBP_R4          4
#define BBP_R5          5
#define BBP_R6          6
#define BBP_R14         14
#define BBP_R16         16
#define BBP_R17         17
#define BBP_R18         18
#define BBP_R21         21
#define BBP_R22         22
#define BBP_R24         24
#define BBP_R25         25
#define BBP_R26         26
#define BBP_R27         27
#define BBP_R31         31
#define BBP_R49         49
#define BBP_R50         50
#define BBP_R51         51
#define BBP_R52         52
#define BBP_R55         55
#define BBP_R62         62
#define BBP_R63         63
#define BBP_R64         64
#define BBP_R65         65
#define BBP_R66         66
#define BBP_R67         67
#define BBP_R68         68
#define BBP_R69         69
#define BBP_R70         70
#define BBP_R73         73
#define BBP_R75         75
#define BBP_R77         77
#define BBP_R78         78
#define BBP_R79         79
#define BBP_R80         80
#define BBP_R81         81
#define BBP_R82         82
#define BBP_R83         83
#define BBP_R84         84
#define BBP_R86         86
#define BBP_R91         91
#define BBP_R92         92
#define BBP_R94         94
#define BBP_R103        103
#define BBP_R105        105
#define BBP_R106        106
#define BBP_R113        113
#define BBP_R114        114
#define BBP_R115        115
#define BBP_R116        116
#define BBP_R117        117
#define BBP_R118        118
#define BBP_R119        119
#define BBP_R120        120
#define BBP_R121        121
#define BBP_R122        122
#define BBP_R123        123


//-------------------------------------------------------------------------
// RFIC sections
//-------------------------------------------------------------------------
#define RFIC_3290       0xA0
#define RFIC_TC2001     0xFF
#define RFIC_TC6004     0xB0

//
// LC memory layout
//
#define SHARED_MEMORY_BASE       0xe000
#define SHARED_MEMORY_SIZE       0x2000

#define MCU_LC_BASE_ADDR         0xe000

//
// PDU (0xDE80-0xDFFF)
//
#define MCU_TEST_PDU_TXBI_ADDR          0xde80
#define MCU_TEST_PDU_PAYLOAD_ADDR       0xde90
#define MCU_EMPTY_PDU_TXBI_ADDR         0xdec0
#define MCU_EMPTY_PDU_PAYLOAD_ADDR      0xded0
#define MCU_ADV_PDU_TXBI_ADDR           0xdf00
#define MCU_ADV_PDU_PAYLOAD_ADDR        0xdf10
#define MCU_CONNREQ_PDU_TXBI_ADDR       0xdf40
#define MCU_CONNREQ_PDU_PAYLOAD_ADDR    0xdf50
#define MCU_SCANRSP_PDU_TXBI_ADDR       0xdf80
#define MCU_SCANRSP_PDU_PAYLOAD_ADDR    0xdf90
#define MCU_SCANREQ_PDU_TXBI_ADDR       0xdfc0
#define MCU_SCANREQ_PDU_PAYLOAD_ADDR    0xdfd0

//
// Logic Link Table(16*32)
//
#define MCU_LOGICAL_LINK_ADDR    0xe000

#define MCU_LOGICAL_LINK_TABLE_SIZE 16

#ifdef RT_BIG_ENDIAN
typedef struct _LC_LLINK_STRUC {
} LC_LLINK_STRUC, *PLC_LLINK_STRUC;
#else
typedef struct _LC_LLINK_STRUC {
    // Byte 0
    UINT8       Owner:1;
    UINT8       Suspend:1;
    UINT8       EscoInd:1;
    UINT8       Rsv:1;
    UINT8       PlFlowR:1;
    UINT8       PlFlowL:1;
    UINT8       Ptt:2;

    // Byte 1
    UINT8       TxH:1;
    UINT8       RxH:1;
    UINT8       Rsv2:6;

    // Byte 2
    UINT8       LtIdx;      // bit 0-3

    // Byte 3
    UINT8       AclcIdx;    // bit 0-4

    // Byte 4
    UINT8       AcluIdx;    // bit 0-4

    // Byte 5
    UINT8       HVPacketType:6;
    UINT8       ESCOReservedSlots:2;

    // Byte 6 - 7
    UINT16      eSCOPktLen; // bit 0-9

    // Byte 8
    UINT8       InRingIdx;  // bit 0-4

    // Byte 9
    UINT8       OutRingIdx:5;   // bit 0-4

    // Byte 10 - 11
    UINT16      Rsv10;

    UINT32      FlushTimeout;
} LC_LLINK_STRUC, *PLC_LLINK_STRUC;
#endif


#define LINK_PRIORITY_LOW           0
#define LINK_PRIORITY_HIGH          1

#define MIN_SNIFF_LINK_PRIO_HIGH    32  /* Sniff threshold to escalate the link priority */


//
// Duty Cycle Generator Table(E200 - E39F)
//
#define MCU_DCG_CSR             0xe200
#define MCU_DCG_OFFSET          16
#define MAX_MCU_DCG_TABLE_SIZE  26

#ifdef RT_BIG_ENDIAN
#else
typedef struct _MCU_DCG_STRUC {
    // Word 0
    UINT32      Mode:8;
    UINT32      Piconet:2;
    UINT32      Rsv:6;
    UINT32      ExtInterval:8;
    UINT32      Update:1;
    UINT32      Rsv1:7;

    // Word 1
    UINT32      BeginInstant:28;
    UINT32      Rsv2:4;

    // Word 2
    UINT32      Window:16;
    UINT32      Interval:16;

    // Word 3
    UINT32      Rsv3;
} MCU_DCG_STRUC, *PMCU_DCG_STRUC;
#endif


//
// Physical Link Table(32*32)
//
#define MCU_PHY_LINK_ADDR           0xe400
#define MCU_PHY_LINK_TABLE_SIZE     60
#define MCU_PHY_LINK_TABLE_OFFSET   64

#ifdef RT_BIG_ENDIAN
typedef struct _LC_PHY_LINK_STRUC {
} LC_PHY_LINK_STRUC, *PLC_PHY_LINK_STRUC;
#else
typedef struct _LC_PHY_LINK_STRUC {
    // Word 0 - 1
    UINT8       SyncWord[8];

    // Word 2 - 3
    UINT8       LAP[3];
    UINT8       UAP;
    UINT8       NAP[2];
    INT8        RSSI;
    UINT8       LNA:2;

    // Word 4 - 7
    UINT8       EncKey[16];

    // Word 8
    UINT32      EncControl:8;
    UINT32      Role:1;
    UINT32      Rsv1:7;
    UINT32      Piconet:2;
    UINT32      Rsv2:6;
    UINT32      ChInfoIdx:3;
    UINT32      AfhEn:1;
    UINT32      Rsv3:4;

    // Word 9
    UINT8       TxPwr;
    UINT8       Rsv4[3];

    // Word 10
    UINT16      UpperTValue;    /* Scheduling parameter 1 */
    UINT16      LowerTValue;

    // Word 11
    UINT16      HCounter;       /* Scheduling parameter 2 */
    UINT16      ECounter;       /* Scheduling parameter 3 */

    // Word 12
    UINT32      SupervisionTo;  // clock

    // Word 13-15
    UINT32      Rsv5[3];        // Reserved for LC LastPollInstant, LastRespInstant, LastServedInstant

} LC_PHY_LINK_STRUC, *PLC_PHY_LINK_STRUC;
#endif

#define MCU_PHY_LINK_RSSI_DW_OFFSET 12
#define MCU_PHY_LINK_LNA_DW_OFFSET  12

#define MCU_LE_PHY_LINK_RSSI_DW_OFFSET  60

#define MCU_PHY_LINK_RSSI_OFFSET        2
#define MCU_PHY_LINK_LNA_OFFSET         3

/*
 * Link scheduling
 */
#ifdef NEW_LC_SCHED_2011_03
//#define DFLT_LINK_SCH_MASTER_T_SERVE      20
#define DFLT_LINK_SCH_MASTER_T_HOLD         1
#define DFLT_LINK_SCH_MASTER_WEIGHT         1

//#define HIGH_LINK_SCH_MASTER_T_SERVE      28
#define HIGH_LINK_SCH_MASTER_T_HOLD         1
#define HIGH_LINK_SCH_MASTER_WEIGHT         10

#define CSR_HIGH_LINK_SCH_MASTER_T_SERVE    4   /* Workaround for iPhone 3G(CSR): iPhone plays A2DP and inquiry */
#define CSR_HIGH_LINK_SCH_MASTER_T_HOLD     1   /* Workaround for iPhone 3G(CSR): iPhone plays A2DP and inquiry */
#define CSR_HIGH_LINK_SCH_MASTER_WEIGHT     1   /* Workaround for iPhone 3G(CSR): iPhone plays A2DP and inquiry */

#define MIN_LINK_SCH_MASTER_T_SERVE         9
#define MIN_LINK_SCH_MASTER_T_HOLD          3
#define MIN_LINK_SCH_MASTER_WEIGHT          1


#define DFLT_LINK_SCH_SLAVE_T_SERVE         160
#define DFLT_LINK_SCH_SLAVE_T_HOLD          21  /* Workaround for SCO(Slave) and FTP(Master) */
#define DFLT_LINK_SCH_SLAVE_WEIGHT          0

#define HIGH_LINK_SCH_SLAVE_T_SERVE         30
#define HIGH_LINK_SCH_SLAVE_T_HOLD          21  /* Workaround for SCO(Slave) and FTP(Master) */
#define HIGH_LINK_SCH_SLAVE_WEIGHT          1

#define MIN_LINK_SCH_SLAVE_T_SERVE          80
#define MIN_LINK_SCH_SLAVE_T_HOLD           41  /* Workaround for SCO(Slave) and FTP(Master) */
#define MIN_LINK_SCH_SLAVE_WEIGHT           0
#else
#define DFLT_LINK_SCH_MASTER_UPPER_T_VAL        60
#define DFLT_LINK_SCH_MASTER_LOWER_T_VAL        20
#define DFLT_LINK_SCH_MASTER_H_VAL              3
#define DFLT_LINK_SCH_MASTER_E_VAL              1

#define HIGH_LINK_SCH_MASTER_UPPER_T_VAL        28
#define HIGH_LINK_SCH_MASTER_LOWER_T_VAL        14
#define HIGH_LINK_SCH_MASTER_H_VAL              14
#define HIGH_LINK_SCH_MASTER_E_VAL              1

#define CSR_HIGH_LINK_SCH_MASTER_UPPER_T_VAL    4   /* Workaround for iPhone 3G(CSR): iPhone plays A2DP and inquiry */
#define CSR_HIGH_LINK_SCH_MASTER_LOWER_T_VAL    4   /* Workaround for iPhone 3G(CSR): iPhone plays A2DP and inquiry */
#define CSR_HIGH_LINK_SCH_MASTER_H_VAL          2   /* Workaround for iPhone 3G(CSR): iPhone plays A2DP and inquiry */
#define CSR_HIGH_LINK_SCH_MASTER_E_VAL          1   /* Workaround for iPhone 3G(CSR): iPhone plays A2DP and inquiry */

#define LOW_LINK_SCH_MASTER_LOWER_T_VAL         30

#define MIN_LINK_SCH_MASTER_UPPER_T_VAL         9
#define MIN_LINK_SCH_MASTER_LOWER_T_VAL         3
#define MIN_LINK_SCH_MASTER_H_VAL               3
#define MIN_LINK_SCH_MASTER_E_VAL               1

#define DFLT_LINK_SCH_SLAVE_UPPER_T_VAL         160
#define DFLT_LINK_SCH_SLAVE_LOWER_T_VAL         80
#define DFLT_LINK_SCH_SLAVE_H_VAL               21  /* Workaround for SCO(Slave) and FTP(Master) */
#define DFLT_LINK_SCH_SLAVE_E_VAL               0

#define HIGH_LINK_SCH_SLAVE_UPPER_T_VAL         30
#define HIGH_LINK_SCH_SLAVE_LOWER_T_VAL         15
#define HIGH_LINK_SCH_SLAVE_H_VAL               21  /* Workaround for SCO(Slave) and FTP(Master) */
#define HIGH_LINK_SCH_SLAVE_E_VAL               1

#define MIN_LINK_SCH_SLAVE_UPPER_T_VAL          80
#define MIN_LINK_SCH_SLAVE_LOWER_T_VAL          60
#define MIN_LINK_SCH_SLAVE_H_VAL                41  /* Workaround for SCO(Slave) and FTP(Master) */
#define MIN_LINK_SCH_SLAVE_E_VAL                0
#endif


#ifdef RT_BIG_ENDIAN
typedef struct _MCU_LE_PHY_LINK_STRUC {
} MCU_LE_PHY_LINK_STRUC, *PMCU_LE_PHY_LINK_STRUC;
#else
typedef struct _MCU_LE_PHY_LINK_STRUC {
    // Word 0 - 1
    UINT8       IV[8];

    // Word 2
    UINT32      AccessAddress;

    // Word 3
    UINT8       CrcInit[3];
    UINT8       HopIncrement;   // Bit 0-4

    // Word 4-7
    UINT8       EncKey[16];

    // Word 8
    UINT32      Rsv:2;
    UINT32      RxEnc:1;
    UINT32      TxEnc:1;
    UINT32      Rsv1:4;
    UINT32      Role:1;
    UINT32      Rsv2:7;
    UINT32      Piconet:3;
    UINT32      Rsv3:5;
    UINT32      ChInfoIdx:3;
    UINT32      Rsv4:5;

    // Word 9
    UINT8       TxPwr;  // Bit 0:5
    UINT8       SCA;
    UINT16      SupervisionTo;

    // Word 10
    UINT8       LastSuccessRxInstant[3];
    UINT8       UnmappedCh;

    // Word 11
    UINT16      ActiveWindowStartInstant;
    UINT16      ActiveWindowSize;

    // Word 12-14
    UINT8       MicTxPacketCount[5];
    UINT8       MicRxPacketCount[5];
    UINT8       ConnEventCount[2];

    // Word 15
    UINT16      SlaveLatency;
    UINT16      Rsv5;
} MCU_LE_PHY_LINK_STRUC, *PMCU_LE_PHY_LINK_STRUC;
#endif

#define MCU_CH_INFO_ADDR        0xe800
#define MCU_CH_INFO_BASE_ADDR   0xe000
#define MAX_NUM_OF_MCU_CH_INFO  8
#define MCU_CH_INFO_OFFSET      128

typedef struct _MCU_CH_INFO_STRUC {
    // Byte 0 - 78
    UINT8       MappingTable[79];

    // Byte 79
    UINT8       RetransmitCount;

    // Byte 80 - 89
    UINT8       ChannelMap[10];

    // Byte 90
    UINT8       Control;

    // Byte 91
    UINT8       NumOfUsedCh;
} MCU_CH_INFO_STRUC, *PMCU_CH_INFO_STRUC;

#define MCU_CH_INFO_CTL_OFFSET  88
#ifdef RT_BIG_ENDIAN
typedef union  _MCU_CH_INFO_CTL_STRUC {
}   MCU_CH_INFO_CTL_STRUC, *PMCU_CH_INFO_CTL_STRUC;
#else
typedef union  _MCU_CH_INFO_CTL_STRUC {
    struct {
        UINT32      Rsv:22;
        UINT32      Active:1;
        UINT32      Owner:1;
        UINT32      Rsv1:8;
    }   field;
    UINT32 word;
}   MCU_CH_INFO_CTL_STRUC, *PMCU_CH_INFO_CTL_STRUC;
#endif

typedef union _MCU_CH_UPDT_CTL_STRUC {
    struct {
        UINT8   ChInfoIdx:3;
        UINT8   AfhEn:1;
    } field;
    UINT8 byte;
} MCU_CH_UPDT_CTL_STRUC, *PMCU_CH_UPDT_CTL_STRUC;

#define MCU_IAC_CTL_ADDR        0xf010
typedef struct _MCU_IAC_CTL_STRUC {
    // Word 0-1
    UINT32      PrimarySyncWord[2];

    // Word 2-3
    UINT32      SecondarySyncWord[2];
} MCU_IAC_CTL_STRUC, *PMCU_IAC_CTL_STRUC;

#define BBP_COMPENP_ADDR        0xf020
#define RF_FREQ_OFFSET_R39_ADDR 0xf021
#define RF_FREQ_OFFSET_R40_ADDR 0xf022

#define LE_TEST_NUM_OF_PKT      0xf024
#define LC_ANTENNA_MODE         0xf026

//
// EIR Response (0xF100 - 0xF1FF)
//
#define MCU_EIR_TXBI_ADDR           0xf100
#define MCU_EIR_PAYLOAD_ADDR        0xf110
#define MCU_EIR_TXBI_OFFSET_ADDR    256
#define MAX_NUM_OF_EIR              1

//
// Channel Assessment (0xF200 - )
//
#define MCU_CH_STAT_ADDR            0xf200
#define MCU_CH_STAT_OFFSET          240

//
// Host to MCU Mailbox
//
#define H2M_MAILBOX_CSR         0xff00

#define OWNER_HOST              0
#define OWNER_MCU               1

#ifdef RT_BIG_ENDIAN
typedef struct _H2M_MAILBOX_STRUC {
}   H2M_MAILBOX_STRUC, *PH2M_MAILBOX_STRUC;
#else
typedef struct _H2M_MAILBOX_STRUC {
    UINT8       Owner;
    UINT8       TransactionID;
    UINT8       CommandID;
    UINT8       Length;

    UINT8       LLIdx;
    UINT8       PiconetIdx;
    UINT8       Reserved[2];

    UINT32      TimeStamp;
    UINT32      StartTime;
} H2M_MAILBOX_STRUC, *PH2M_MAILBOX_STRUC;
#endif  //#ifdef RT_BIG_ENDIAN

#define H2M_MAILBOX_DATA_BUF_CSR 0xff10

//
// MCU to Host Event
//
#define M2H_EVENT_CSR           0xff80

#ifdef RT_BIG_ENDIAN
typedef struct _M2H_EVENT_STRUC {
}   M2H_EVENT_STRUC, *PM2H_EVENT_STRUC;
#else
typedef struct _M2H_EVENT_STRUC {
    UINT8       Owner;
    UINT8       TransactionID;
    UINT8       EventID;
    UINT8       Length;

    UINT8       LLIdx;
    UINT8       PiconetIdx;
    UINT8       Rsv[2];

    UINT32      TimeStamp;
} M2H_EVENT_STRUC, *PM2H_EVENT_STRUC;
#endif //RT_BIG_ENDIAN

#define M2H_EVENT_DATA_BUF_CSR 0xff8C

#define M2H_LC_FERROR_CSR   0xffc0

#ifdef RT_BIG_ENDIAN
typedef struct _M2H_LC_FERROR_STRUC {
} M2H_LC_FERROR_STRUC, *PM2H_LC_FERROR_STRUC;
#else
typedef struct _M2H_LC_FERROR_STRUC {
    UINT8       ErrorCode;
    UINT8       LLTIdx;
    UINT16      RefPointer;

    UINT32      NativeTimeStamp;
    UINT32      PiconetTimeStamp;

    /* 0xffcc - 0xffff */
    UINT8       Reserved[52];
} M2H_LC_FERROR_STRUC, *PM2H_LC_FERROR_STRUC;
#endif

#endif

