/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __INC_HAL8723DPHYCFG_H__
#define __INC_HAL8723DPHYCFG_H__

/*--------------------------Define Parameters-------------------------------*/
#define LOOP_LIMIT				5
#define MAX_STALL_TIME			50		/* us */
#define AntennaDiversityValue	0x80	/* (Adapter->bSoftwareAntennaDiversity ? 0x00 : 0x80) */
#define MAX_TXPWR_IDX_NMODE_92S	63
#define Reset_Cnt_Limit			3

#define MAX_AGGR_NUM	0x07


/*--------------------------Define Parameters End-------------------------------*/


/*------------------------------Define structure----------------------------*/

/*------------------------------Define structure End----------------------------*/

/*--------------------------Exported Function prototype---------------------*/
u32 PHY_QueryBBReg_8723D(struct adapter *Adapter, u32 RegAddr, u32 BitMask);

void PHY_SetBBReg_8723D(struct adapter *Adapter, u32 RegAddr, u32 BitMask,
			u32 Data);

u32 PHY_QueryRFReg_8723D(struct adapter *Adapter, enum rf_path eRFPath,
			 u32 RegAddr, u32 BitMask);

void PHY_SetRFReg_8723D(struct adapter *Adapter, enum rf_path eRFPath,
			u32 RegAddr, u32 BitMask, u32 Data);

/* MAC/BB/RF HAL config */
int PHY_BBConfig8723D(struct adapter *adapt);

int PHY_RFConfig8723D(struct adapter *adapt);

int PHY_MACConfig8723D(struct adapter *adapt);

int PHY_ConfigRFWithParaFile_8723D(struct adapter *adapt, u8 *pFileName,
				   enum rf_path eRFPath);
void PHY_SetTxPowerIndex_8723D(struct adapter *Adapter, u32 PowerIndex,
			       enum rf_path RFPath, u8	Rate);
u8 PHY_GetTxPowerIndex_8723D(struct adapter *pAdapter, enum rf_path RFPath,
			     u8 Rate, u8 BandWidth, u8 Channel,
			     struct txpwr_idx_comp *tic);

void PHY_GetTxPowerLevel8723D(struct adapter *Adapter, int *powerlevel);

void PHY_SetTxPowerLevel8723D(struct adapter *Adapter, u8 channel);

void PHY_SetSwChnlBWMode8723D(struct adapter *Adapter, u8 channel,
			      enum channel_width Bandwidth, u8 Offset40,
			      u8 Offset80);

void phy_set_rf_path_switch_8723d(struct adapter *pAdapter, bool bMain);
/*--------------------------Exported Function prototype End---------------------*/

#endif
