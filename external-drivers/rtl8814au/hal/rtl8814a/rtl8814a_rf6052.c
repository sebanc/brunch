/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _RTL8814A_RF6052_C_

/* #include <drv_types.h> */
#include <rtl8814a_hal.h>


/*-----------------------------------------------------------------------------
 * Function:    PHY_RF6052SetBandwidth()
 *
 * Overview:    This function is called by SetBWModeCallback8190Pci() only
 *
 * Input:       PADAPTER				Adapter
 *			WIRELESS_BANDWIDTH_E	Bandwidth	//20M or 40M
 *
 * Output:      NONE
 *
 * Return:      NONE
 *
 * Note:		For RF type 0222D
 *---------------------------------------------------------------------------*/
void
PHY_RF6052SetBandwidth8814A(
		PADAPTER				Adapter,
		enum channel_width		Bandwidth)	/* 20M or 40M */
{
	switch (Bandwidth) {
	case CHANNEL_WIDTH_20:
		/*RTW_INFO("PHY_RF6052SetBandwidth8814A(), set 20MHz\n");*/
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_CHNLBW_Jaguar, BIT11 | BIT10, 3);
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_CHNLBW_Jaguar, BIT11 | BIT10, 3);
		phy_set_rf_reg(Adapter, RF_PATH_C, RF_CHNLBW_Jaguar, BIT11 | BIT10, 3);
		phy_set_rf_reg(Adapter, RF_PATH_D, RF_CHNLBW_Jaguar, BIT11 | BIT10, 3);
		break;

	case CHANNEL_WIDTH_40:
		/*RTW_INFO("PHY_RF6052SetBandwidth8814A(), set 40MHz\n");*/
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_CHNLBW_Jaguar, BIT11 | BIT10, 1);
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_CHNLBW_Jaguar, BIT11 | BIT10, 1);
		phy_set_rf_reg(Adapter, RF_PATH_C, RF_CHNLBW_Jaguar, BIT11 | BIT10, 1);
		phy_set_rf_reg(Adapter, RF_PATH_D, RF_CHNLBW_Jaguar, BIT11 | BIT10, 1);
		break;

	case CHANNEL_WIDTH_80:
		/*RTW_INFO("PHY_RF6052SetBandwidth8814A(), set 80MHz\n");*/
		phy_set_rf_reg(Adapter, RF_PATH_A, RF_CHNLBW_Jaguar, BIT11 | BIT10, 0);
		phy_set_rf_reg(Adapter, RF_PATH_B, RF_CHNLBW_Jaguar, BIT11 | BIT10, 0);
		phy_set_rf_reg(Adapter, RF_PATH_C, RF_CHNLBW_Jaguar, BIT11 | BIT10, 0);
		phy_set_rf_reg(Adapter, RF_PATH_D, RF_CHNLBW_Jaguar, BIT11 | BIT10, 0);
		break;

	default:
		RTW_INFO("PHY_RF6052SetBandwidth8814A(): unknown Bandwidth: %#X\n", Bandwidth);
		break;
	}
}

static int
phy_RF6052_Config_ParaFile_8814A(
		PADAPTER		Adapter
)
{
	u32					u4RegValue = 0;
	enum rf_path			eRFPath;
	int					rtStatus = _SUCCESS;
	HAL_DATA_TYPE		*pHalData = GET_HAL_DATA(Adapter);

	/* 3 */ /* ----------------------------------------------------------------- */
	/* 3 */ /* <2> Initialize RF */
	/* 3 */ /* ----------------------------------------------------------------- */
	/* for(eRFPath = RF_PATH_A; eRFPath <pHalData->NumTotalRFPath; eRFPath++) */
	for (eRFPath = 0; eRFPath < pHalData->NumTotalRFPath; eRFPath++) {
		/*----Initialize RF fom connfiguration file----*/
		switch (eRFPath) {
		case RF_PATH_A:
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
			if (PHY_ConfigRFWithParaFile(Adapter, PHY_FILE_RADIO_A, eRFPath) == _FAIL)
#endif /* CONFIG_LOAD_PHY_PARA_FROM_FILE */
			{
#ifdef CONFIG_EMBEDDED_FWIMG
				if (odm_config_rf_with_header_file(&pHalData->odmpriv, CONFIG_RF_RADIO, eRFPath) == HAL_STATUS_FAILURE)
					rtStatus = _FAIL;
#endif /* CONFIG_EMBEDDED_FWIMG */
			}
			break;
		case RF_PATH_B:
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
			if (PHY_ConfigRFWithParaFile(Adapter, PHY_FILE_RADIO_B, eRFPath) == _FAIL)
#endif /* CONFIG_LOAD_PHY_PARA_FROM_FILE */
			{
#ifdef CONFIG_EMBEDDED_FWIMG
				if (odm_config_rf_with_header_file(&pHalData->odmpriv, CONFIG_RF_RADIO, eRFPath) == HAL_STATUS_FAILURE)
					rtStatus = _FAIL;
#endif /* CONFIG_EMBEDDED_FWIMG */
			}
			break;
		case RF_PATH_C:
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
			if (PHY_ConfigRFWithParaFile(Adapter, PHY_FILE_RADIO_C, eRFPath) == _FAIL)
#endif /* CONFIG_LOAD_PHY_PARA_FROM_FILE */
			{
#ifdef CONFIG_EMBEDDED_FWIMG
				if (odm_config_rf_with_header_file(&pHalData->odmpriv, CONFIG_RF_RADIO, eRFPath) == HAL_STATUS_FAILURE)
					rtStatus = _FAIL;
#endif /* CONFIG_EMBEDDED_FWIMG */
			}
			break;
		case RF_PATH_D:
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
			if (PHY_ConfigRFWithParaFile(Adapter, PHY_FILE_RADIO_D, eRFPath) == _FAIL)
#endif /* CONFIG_LOAD_PHY_PARA_FROM_FILE */
			{
#ifdef CONFIG_EMBEDDED_FWIMG
				if (odm_config_rf_with_header_file(&pHalData->odmpriv, CONFIG_RF_RADIO, eRFPath) == HAL_STATUS_FAILURE)
					rtStatus = _FAIL;
#endif /* CONFIG_EMBEDDED_FWIMG */
			}
			break;
		default:
			break;
		}

		if (rtStatus != _SUCCESS) {
			RTW_INFO("%s():Radio[%d] Fail!!", __FUNCTION__, eRFPath);
			goto phy_RF6052_Config_ParaFile_Fail;
		}

	}

	u4RegValue = phy_query_rf_reg(Adapter, RF_PATH_A, RF_RCK1_Jaguar, bRFRegOffsetMask);
	phy_set_rf_reg(Adapter, RF_PATH_B,  RF_RCK1_Jaguar, bRFRegOffsetMask, u4RegValue);
	phy_set_rf_reg(Adapter, RF_PATH_C,  RF_RCK1_Jaguar, bRFRegOffsetMask, u4RegValue);
	phy_set_rf_reg(Adapter, RF_PATH_D,  RF_RCK1_Jaguar, bRFRegOffsetMask, u4RegValue);

	/* 3 ----------------------------------------------------------------- */
	/* 3 Configuration of Tx Power Tracking */
	/* 3 ----------------------------------------------------------------- */

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	if (PHY_ConfigRFWithTxPwrTrackParaFile(Adapter, PHY_FILE_TXPWR_TRACK) == _FAIL)
#endif /* CONFIG_LOAD_PHY_PARA_FROM_FILE */
	{
#ifdef CONFIG_EMBEDDED_FWIMG
		odm_config_rf_with_tx_pwr_track_header_file(&pHalData->odmpriv);
#endif
	}


phy_RF6052_Config_ParaFile_Fail:
	return rtStatus;
}


int
PHY_RF6052_Config_8814A(
		PADAPTER		Adapter)
{
	int					rtStatus = _SUCCESS;

	/*  */
	/* Config BB and RF */
	/*  */
	rtStatus = phy_RF6052_Config_ParaFile_8814A(Adapter);

	return rtStatus;
}


/* End of HalRf6052.c */
