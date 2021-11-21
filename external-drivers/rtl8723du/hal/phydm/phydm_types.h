/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __ODM_TYPES_H__
#define __ODM_TYPES_H__


/*Define Different SW team support*/
#define	ODM_AP			0x01	/*BIT(0)*/
#define	ODM_CE			0x04	/*BIT(2)*/
#define	ODM_WIN		0x08	/*BIT(3)*/
#define	ODM_ADSL		0x10	/*BIT(4)*/		/*already combine with ODM_AP, and is nouse now*/
#define	ODM_IOT		0x20	/*BIT(5)*/

/*For FW API*/
#define	__iram_odm_func__

#define GET_PDM_ODM(__padapter)	((struct PHY_DM_STRUCT*)(&((GET_HAL_DATA(__padapter))->odmpriv)))

enum hal_status {
	HAL_STATUS_SUCCESS,
	HAL_STATUS_FAILURE,
	/*RT_STATUS_PENDING,
	RT_STATUS_RESOURCE,
	RT_STATUS_INVALID_CONTEXT,
	RT_STATUS_INVALID_PARAMETER,
	RT_STATUS_NOT_SUPPORT,
	RT_STATUS_OS_API_FAILED,*/
};

#define		VISTA_USB_RX_REVISE			0

/*
 * Declare for ODM spin lock defintion temporarily fro compile pass.
 *   */
enum rt_spinlock_type {
	RT_TX_SPINLOCK = 1,
	RT_RX_SPINLOCK = 2,
	RT_RM_SPINLOCK = 3,
	RT_CAM_SPINLOCK = 4,
	RT_SCAN_SPINLOCK = 5,
	RT_LOG_SPINLOCK = 7,
	RT_BW_SPINLOCK = 8,
	RT_CHNLOP_SPINLOCK = 9,
	RT_RF_OPERATE_SPINLOCK = 10,
	RT_INITIAL_SPINLOCK = 11,
	RT_RF_STATE_SPINLOCK = 12, /* For RF state. Added by Bruce, 2007-10-30. */
#if VISTA_USB_RX_REVISE
	RT_USBRX_CONTEXT_SPINLOCK = 13,
	RT_USBRX_POSTPROC_SPINLOCK = 14, /* protect data of adapter->IndicateW/ IndicateR */
#endif
	/* Shall we define Ndis 6.2 SpinLock Here ? */
	RT_PORT_SPINLOCK = 16,
	RT_VNIC_SPINLOCK = 17,
	RT_HVL_SPINLOCK = 18,
	RT_H2C_SPINLOCK = 20, /* For H2C cmd. Added by tynli. 2009.11.09. */

	rt_bt_data_spinlock = 25,

	/* add for 92D CCK control issue */
	RT_CCK_PAGEA_SPINLOCK = 28,
	RT_BUFFER_SPINLOCK = 29,
	RT_CHANNEL_AND_BANDWIDTH_SPINLOCK = 30,
	RT_GEN_TEMP_BUF_SPINLOCK = 31,
	RT_AWB_SPINLOCK = 32,
	RT_FW_PS_SPINLOCK = 33,
	RT_HW_TIMER_SPIN_LOCK = 34,
	RT_MPT_WI_SPINLOCK = 35,
	RT_P2P_SPIN_LOCK = 36,	/* Protect P2P context */
	RT_DBG_SPIN_LOCK = 37,
	RT_IQK_SPINLOCK = 38,
	RT_PENDED_OID_SPINLOCK = 39,
	RT_CHNLLIST_SPINLOCK = 40,
	RT_INDIC_SPINLOCK = 41,	/* protect indication */
	RT_RFD_SPINLOCK = 42,
	RT_SYNC_IO_CNT_SPINLOCK = 43,
	RT_LAST_SPINLOCK,
};

	#include <drv_types.h>

	#define SET_TX_DESC_ANTSEL_A_88E(__ptx_desc, __value) SET_BITS_TO_LE_4BYTE(__ptx_desc+8, 24, 1, __value)
	#define SET_TX_DESC_ANTSEL_B_88E(__ptx_desc, __value) SET_BITS_TO_LE_4BYTE(__ptx_desc+8, 25, 1, __value)
	#define SET_TX_DESC_ANTSEL_C_88E(__ptx_desc, __value) SET_BITS_TO_LE_4BYTE(__ptx_desc+28, 29, 1, __value)

	/* define useless flag to avoid compile warning */
	#define	USE_WORKITEM 0
	#define	FOR_BRAZIL_PRETEST 0
	#define	FPGA_TWO_MAC_VERIFICATION	0
	#define	RTL8881A_SUPPORT	0

	#if (defined(TESTCHIP_SUPPORT))
		#define	PHYDM_TESTCHIP_SUPPORT 1
	#else
		#define	PHYDM_TESTCHIP_SUPPORT 0
	#endif

#define READ_NEXT_PAIR(v1, v2, i) do { if (i+2 >= array_len) break; i += 2; v1 = array[i]; v2 = array[i+1]; } while (0)
#define COND_ELSE  2
#define COND_ENDIF 3

#define	MASKBYTE0		0xff
#define	MASKBYTE1		0xff00
#define	MASKBYTE2		0xff0000
#define	MASKBYTE3		0xff000000
#define	MASKHWORD		0xffff0000
#define	MASKLWORD		0x0000ffff
#define	MASKDWORD		0xffffffff
#define	MASK7BITS		0x7f
#define	MASK12BITS		0xfff
#define	MASKH4BITS		0xf0000000
#define	MASK20BITS		0xfffff
#define	MASKOFDM_D		0xffc00000
#define	MASKCCK			0x3f3f3f3f
#define RFREGOFFSETMASK	0xfffff
#define MASKH3BYTES		0xffffff00
#define MASKL3BYTES		0x00ffffff
#define MASKBYTE2HIGHNIBBLE		0x00f00000
#define MASKBYTE3LOWNIBBLE		0x0f000000
#define	MASKL3BYTES			0x00ffffff
#define RFREGOFFSETMASK	0xfffff

#endif /* __ODM_TYPES_H__ */
