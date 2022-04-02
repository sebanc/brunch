#include "halmac_8821c_cfg.h"


#if HALMAC_PLATFORM_WINDOWS
/*SDIO RQPN Mapping for Windows, extra queue is not implemented in Driver code*/
HALMAC_RQPN HALMAC_RQPN_SDIO_8821C[] = {
	/* { mode, vo_map, vi_map, be_map, bk_map, mg_map, hi_map } */
	{HALMAC_TRX_MODE_NORMAL, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_TRXSHARE, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_WMM, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_P2P, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
};
#else
/*SDIO RQPN Mapping*/
HALMAC_RQPN HALMAC_RQPN_SDIO_8821C[] = {
	/* { mode, vo_map, vi_map, be_map, bk_map, mg_map, hi_map } */
	{HALMAC_TRX_MODE_NORMAL, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_TRXSHARE, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_WMM, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_P2P, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
};
#endif

/*PCIE RQPN Mapping*/
HALMAC_RQPN HALMAC_RQPN_PCIE_8821C[] = {
	/* { mode, vo_map, vi_map, be_map, bk_map, mg_map, hi_map } */
	{HALMAC_TRX_MODE_NORMAL, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_TRXSHARE, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_WMM, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_P2P, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
};

/*USB 2 Bulkout RQPN Mapping*/
HALMAC_RQPN HALMAC_RQPN_2BULKOUT_8821C[] = {
	/* { mode, vo_map, vi_map, be_map, bk_map, mg_map, hi_map } */
	{HALMAC_TRX_MODE_NORMAL, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_TRXSHARE, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_WMM, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_P2P, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_LOOPBACK, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
};

/*USB 3 Bulkout RQPN Mapping*/
HALMAC_RQPN HALMAC_RQPN_3BULKOUT_8821C[] = {
	/* { mode, vo_map, vi_map, be_map, bk_map, mg_map, hi_map } */
	{HALMAC_TRX_MODE_NORMAL, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_TRXSHARE, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_WMM, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_P2P, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_LOOPBACK, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_HQ, HALMAC_MAP2_HQ},
};

/*USB 4 Bulkout RQPN Mapping*/
HALMAC_RQPN HALMAC_RQPN_4BULKOUT_8821C[] = {
	/* { mode, vo_map, vi_map, be_map, bk_map, mg_map, hi_map } */
	{HALMAC_TRX_MODE_NORMAL, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_TRXSHARE, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_WMM, HALMAC_MAP2_HQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_NQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_P2P, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, HALMAC_MAP2_NQ, HALMAC_MAP2_NQ, HALMAC_MAP2_LQ, HALMAC_MAP2_LQ, HALMAC_MAP2_EXQ, HALMAC_MAP2_HQ},
};
#if HALMAC_PLATFORM_WINDOWS
/*SDIO Page Number*/
HALMAC_PG_NUM HALMAC_PG_NUM_SDIO_8821C[] = {
	/* { mode, hq_num, nq_num, lq_num, exq_num, gap_num} */
	{HALMAC_TRX_MODE_NORMAL, 16, 16, 16, 0, 1},
	{HALMAC_TRX_MODE_TRXSHARE, 8, 8, 8, 0, 1},
	{HALMAC_TRX_MODE_WMM, 16, 16, 16, 0, 1},
	{HALMAC_TRX_MODE_P2P, 16, 16, 16, 0, 1},
	{HALMAC_TRX_MODE_LOOPBACK, 16, 16, 16, 0, 256},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, 16, 16, 16, 0, 256},
};
#else
/*SDIO Page Number*/
HALMAC_PG_NUM HALMAC_PG_NUM_SDIO_8821C[] = {
	/* { mode, hq_num, nq_num, lq_num, exq_num, gap_num} */
	{HALMAC_TRX_MODE_NORMAL, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_TRXSHARE, 8, 8, 8, 8, 1},
	{HALMAC_TRX_MODE_WMM, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_P2P, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_LOOPBACK, 16, 16, 16, 14, 256},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, 16, 16, 16, 14, 256},
};
#endif

/*PCIE Page Number*/
HALMAC_PG_NUM HALMAC_PG_NUM_PCIE_8821C[] = {
	/* { mode, hq_num, nq_num, lq_num, exq_num, gap_num} */
	{HALMAC_TRX_MODE_NORMAL, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_TRXSHARE, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_WMM, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_P2P, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_LOOPBACK, 16, 16, 16, 14, 256},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, 16, 16, 16, 14, 256},
};

/*USB 2 Bulkout Page Number*/
HALMAC_PG_NUM HALMAC_PG_NUM_2BULKOUT_8821C[] = {
	/* { mode, hq_num, nq_num, lq_num, exq_num, gap_num} */
	{HALMAC_TRX_MODE_NORMAL, 16, 16, 0, 0, 1},
	{HALMAC_TRX_MODE_TRXSHARE, 16, 16, 0, 0, 1},
	{HALMAC_TRX_MODE_WMM, 16, 16, 0, 0, 1},
	{HALMAC_TRX_MODE_P2P, 16, 16, 0, 0, 1},
	{HALMAC_TRX_MODE_LOOPBACK, 16, 16, 0, 0, 256},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, 16, 16, 0, 0, 256},
};

/*USB 3 Bulkout Page Number*/
HALMAC_PG_NUM HALMAC_PG_NUM_3BULKOUT_8821C[] = {
	/* { mode, hq_num, nq_num, lq_num, exq_num, gap_num} */
	{HALMAC_TRX_MODE_NORMAL, 16, 16, 16, 0, 1},
	{HALMAC_TRX_MODE_TRXSHARE, 16, 16, 16, 0, 1},
	{HALMAC_TRX_MODE_WMM, 16, 16, 16, 0, 1},
	{HALMAC_TRX_MODE_P2P, 16, 16, 16, 0, 1},
	{HALMAC_TRX_MODE_LOOPBACK, 16, 16, 16, 0, 256},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, 16, 16, 16, 0, 256},
};

/*USB 4 Bulkout Page Number*/
HALMAC_PG_NUM HALMAC_PG_NUM_4BULKOUT_8821C[] = {
	/* { mode, hq_num, nq_num, lq_num, exq_num, gap_num} */
	{HALMAC_TRX_MODE_NORMAL, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_TRXSHARE, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_WMM, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_P2P, 16, 16, 16, 14, 1},
	{HALMAC_TRX_MODE_LOOPBACK, 16, 16, 16, 14, 256},
	{HALMAC_TRX_MODE_DELAY_LOOPBACK, 16, 16, 16, 14, 256},
};

HALMAC_RET_STATUS
halmac_txdma_queue_mapping_8821c(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_TRX_MODE halmac_trx_mode
)
{
	u16 value16;
	VOID *pDriver_adapter = NULL;
	PHALMAC_RQPN pCurr_rqpn_Sel = NULL;
	HALMAC_RET_STATUS status;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (HALMAC_INTERFACE_SDIO == pHalmac_adapter->halmac_interface) {
		pCurr_rqpn_Sel = HALMAC_RQPN_SDIO_8821C;
	} else if (HALMAC_INTERFACE_PCIE == pHalmac_adapter->halmac_interface) {
		pCurr_rqpn_Sel = HALMAC_RQPN_PCIE_8821C;
	} else if (HALMAC_INTERFACE_USB == pHalmac_adapter->halmac_interface) {
		if (pHalmac_adapter->halmac_bulkout_num == 2) {
			pCurr_rqpn_Sel = HALMAC_RQPN_2BULKOUT_8821C;
		} else if (pHalmac_adapter->halmac_bulkout_num == 3) {
			pCurr_rqpn_Sel = HALMAC_RQPN_3BULKOUT_8821C;
		} else if (pHalmac_adapter->halmac_bulkout_num == 4) {
			pCurr_rqpn_Sel = HALMAC_RQPN_4BULKOUT_8821C;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "interface not support\n");
			return HALMAC_RET_NOT_SUPPORT;
		}
	}

	status = halmac_rqpn_parser_88xx(pHalmac_adapter, halmac_trx_mode, pCurr_rqpn_Sel);
	if (HALMAC_RET_SUCCESS != status)
		return status;

	value16 = 0;
	value16 |= BIT_TXDMA_HIQ_MAP(pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_HI]);
	value16 |= BIT_TXDMA_MGQ_MAP(pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_MG]);
	value16 |= BIT_TXDMA_BKQ_MAP(pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BK]);
	value16 |= BIT_TXDMA_BEQ_MAP(pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_BE]);
	value16 |= BIT_TXDMA_VIQ_MAP(pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VI]);
	value16 |= BIT_TXDMA_VOQ_MAP(pHalmac_adapter->halmac_ptcl_queue[HALMAC_PTCL_QUEUE_VO]);
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_TXDMA_PQ_MAP, value16);

	return HALMAC_RET_SUCCESS;
}

HALMAC_RET_STATUS
halmac_priority_queue_config_8821c(
	IN PHALMAC_ADAPTER pHalmac_adapter,
	IN HALMAC_TRX_MODE halmac_trx_mode
)
{
	u8 transfer_mode = 0;
	u8 value8;
	u32 counter;
	HALMAC_RET_STATUS status;
	PHALMAC_PG_NUM pCurr_pg_num = NULL;
	VOID *pDriver_adapter = NULL;
	PHALMAC_API pHalmac_api;

	pDriver_adapter = pHalmac_adapter->pDriver_adapter;
	pHalmac_api = (PHALMAC_API)pHalmac_adapter->pHalmac_api;

	if (HALMAC_LA_MODE_DISABLE == pHalmac_adapter->txff_allocation.la_mode) {
		if (HALMAC_RX_FIFO_EXPANDING_MODE_DISABLE == pHalmac_adapter->txff_allocation.rx_fifo_expanding_mode) {
			pHalmac_adapter->txff_allocation.tx_fifo_pg_num = HALMAC_TX_FIFO_SIZE_8821C >> HALMAC_TX_PAGE_SIZE_2_POWER_8821C;
		} else if (HALMAC_RX_FIFO_EXPANDING_MODE_1_BLOCK == pHalmac_adapter->txff_allocation.rx_fifo_expanding_mode) {
			pHalmac_adapter->txff_allocation.tx_fifo_pg_num = HALMAC_TX_FIFO_SIZE_RX_FIFO_EXPANDING_1_BLOCK_8821C >> HALMAC_TX_PAGE_SIZE_2_POWER_8821C;
			pHalmac_adapter->hw_config_info.tx_fifo_size = HALMAC_TX_FIFO_SIZE_RX_FIFO_EXPANDING_1_BLOCK_8821C;
			if (HALMAC_RX_FIFO_SIZE_RX_FIFO_EXPANDING_1_BLOCK_8821C <= HALMAC_RX_FIFO_SIZE_RX_FIFO_EXPANDING_1_BLOCK_MAX_8821C)
				pHalmac_adapter->hw_config_info.rx_fifo_size = HALMAC_RX_FIFO_SIZE_RX_FIFO_EXPANDING_1_BLOCK_8821C;
			else
				pHalmac_adapter->hw_config_info.rx_fifo_size = HALMAC_RX_FIFO_SIZE_RX_FIFO_EXPANDING_1_BLOCK_MAX_8821C;
		} else {
			pHalmac_adapter->txff_allocation.tx_fifo_pg_num = HALMAC_TX_FIFO_SIZE_8821C >> HALMAC_TX_PAGE_SIZE_2_POWER_8821C;
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "rx_fifo_expanding_mode = %d not support\n", pHalmac_adapter->txff_allocation.rx_fifo_expanding_mode);
		}
	} else {
		pHalmac_adapter->txff_allocation.tx_fifo_pg_num = HALMAC_TX_FIFO_SIZE_LA_8821C >> HALMAC_TX_PAGE_SIZE_2_POWER_8821C;
	}
	pHalmac_adapter->txff_allocation.rsvd_pg_num = (pHalmac_adapter->txff_allocation.rsvd_drv_pg_num +
							HALMAC_RSVD_H2C_EXTRAINFO_PGNUM_8821C +
							HALMAC_RSVD_H2C_QUEUE_PGNUM_8821C +
							HALMAC_RSVD_CPU_INSTRUCTION_PGNUM_8821C +
							HALMAC_RSVD_FW_TXBUFF_PGNUM_8821C);
	if (pHalmac_adapter->txff_allocation.rsvd_pg_num > pHalmac_adapter->txff_allocation.tx_fifo_pg_num)
		return HALMAC_RET_CFG_TXFIFO_PAGE_FAIL;

	pHalmac_adapter->txff_allocation.ac_q_pg_num = pHalmac_adapter->txff_allocation.tx_fifo_pg_num - pHalmac_adapter->txff_allocation.rsvd_pg_num;
	pHalmac_adapter->txff_allocation.rsvd_pg_bndy = pHalmac_adapter->txff_allocation.tx_fifo_pg_num - pHalmac_adapter->txff_allocation.rsvd_pg_num;
	pHalmac_adapter->txff_allocation.rsvd_fw_txbuff_pg_bndy = pHalmac_adapter->txff_allocation.tx_fifo_pg_num - HALMAC_RSVD_FW_TXBUFF_PGNUM_8821C;
	pHalmac_adapter->txff_allocation.rsvd_cpu_instr_pg_bndy = pHalmac_adapter->txff_allocation.rsvd_fw_txbuff_pg_bndy - HALMAC_RSVD_CPU_INSTRUCTION_PGNUM_8821C;
	pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy = pHalmac_adapter->txff_allocation.rsvd_cpu_instr_pg_bndy - HALMAC_RSVD_H2C_QUEUE_PGNUM_8821C;
	pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy = pHalmac_adapter->txff_allocation.rsvd_h2c_queue_pg_bndy - HALMAC_RSVD_H2C_EXTRAINFO_PGNUM_8821C;
	pHalmac_adapter->txff_allocation.rsvd_drv_pg_bndy = pHalmac_adapter->txff_allocation.rsvd_h2c_extra_info_pg_bndy - pHalmac_adapter->txff_allocation.rsvd_drv_pg_num;

	if (HALMAC_INTERFACE_SDIO == pHalmac_adapter->halmac_interface) {
		pCurr_pg_num = HALMAC_PG_NUM_SDIO_8821C;
	} else if (HALMAC_INTERFACE_PCIE == pHalmac_adapter->halmac_interface) {
		pCurr_pg_num = HALMAC_PG_NUM_PCIE_8821C;
	} else if (HALMAC_INTERFACE_USB == pHalmac_adapter->halmac_interface) {
		if (pHalmac_adapter->halmac_bulkout_num == 2) {
			pCurr_pg_num = HALMAC_PG_NUM_2BULKOUT_8821C;
		} else if (pHalmac_adapter->halmac_bulkout_num == 3) {
			pCurr_pg_num = HALMAC_PG_NUM_3BULKOUT_8821C;
		} else if (pHalmac_adapter->halmac_bulkout_num == 4) {
			pCurr_pg_num = HALMAC_PG_NUM_4BULKOUT_8821C;
		} else {
			PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_ERR, "interface not support\n");
			return HALMAC_RET_NOT_SUPPORT;
		}
	}

	status = halmac_pg_num_parser_88xx(pHalmac_adapter, halmac_trx_mode, pCurr_pg_num);
	if (HALMAC_RET_SUCCESS != status)
		return status;

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "Set FIFO page\n");

	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_INFO_1, pHalmac_adapter->txff_allocation.high_queue_pg_num);
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_INFO_2, pHalmac_adapter->txff_allocation.low_queue_pg_num);
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_INFO_3, pHalmac_adapter->txff_allocation.normal_queue_pg_num);
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_INFO_4, pHalmac_adapter->txff_allocation.extra_queue_pg_num);
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_INFO_5, pHalmac_adapter->txff_allocation.pub_queue_pg_num);

	pHalmac_adapter->sdio_free_space.high_queue_number = pHalmac_adapter->txff_allocation.high_queue_pg_num;
	pHalmac_adapter->sdio_free_space.normal_queue_number = pHalmac_adapter->txff_allocation.normal_queue_pg_num;
	pHalmac_adapter->sdio_free_space.low_queue_number = pHalmac_adapter->txff_allocation.low_queue_pg_num;
	pHalmac_adapter->sdio_free_space.public_queue_number = pHalmac_adapter->txff_allocation.pub_queue_pg_num;
	pHalmac_adapter->sdio_free_space.extra_queue_number = pHalmac_adapter->txff_allocation.extra_queue_pg_num;

	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_RQPN_CTRL_2, HALMAC_REG_READ_32(pHalmac_adapter, REG_RQPN_CTRL_2) | BIT(31));

	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_BCNQ_BDNY_V1, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCNQ_PGBNDY_V1));
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_FIFOPAGE_CTRL_2 + 2, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCN_HEAD_1_V1));
	HALMAC_REG_WRITE_16(pHalmac_adapter, REG_BCNQ1_BDNY_V1, (u16)(pHalmac_adapter->txff_allocation.rsvd_pg_bndy & BIT_MASK_BCNQ_PGBNDY_V1));

	HALMAC_REG_WRITE_32(pHalmac_adapter, REG_RXFF_BNDY, pHalmac_adapter->hw_config_info.rx_fifo_size - HALMAC_C2H_PKT_BUF_8821C - 1);

	PLATFORM_MSG_PRINT(pDriver_adapter, HALMAC_MSG_INIT, HALMAC_DBG_TRACE, "Init LLT table\n");

	if (HALMAC_INTERFACE_USB == pHalmac_adapter->halmac_interface) {
		value8 = (u8)(HALMAC_REG_READ_8(pHalmac_adapter, REG_AUTO_LLT_V1) & ~(BIT_MASK_BLK_DESC_NUM << BIT_SHIFT_BLK_DESC_NUM));
		value8 = (u8)(value8 | (HALMAC_BLK_DESC_NUM_8821C << BIT_SHIFT_BLK_DESC_NUM));
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_AUTO_LLT_V1, value8);

		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_AUTO_LLT_V1 + 3, HALMAC_BLK_DESC_NUM_8821C);
		HALMAC_REG_WRITE_8(pHalmac_adapter, REG_TXDMA_OFFSET_CHK + 1, HALMAC_REG_READ_8(pHalmac_adapter, REG_TXDMA_OFFSET_CHK + 1) | BIT(1));
	}

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_AUTO_LLT_V1, (u8)(HALMAC_REG_READ_8(pHalmac_adapter, REG_AUTO_LLT_V1) | BIT_AUTO_INIT_LLT_V1));
	counter = 1000;
	while (HALMAC_REG_READ_8(pHalmac_adapter, REG_AUTO_LLT_V1) & BIT_AUTO_INIT_LLT_V1) {
		counter--;
		if (counter == 0)
			return HALMAC_RET_INIT_LLT_FAIL;
	}

	if (HALMAC_TRX_MODE_DELAY_LOOPBACK == halmac_trx_mode) {
		transfer_mode = HALMAC_TRNSFER_LOOPBACK_DELAY;
		HALMAC_REG_WRITE_16(pHalmac_adapter, REG_WMAC_LBK_BUF_HD_V1, (u16)pHalmac_adapter->txff_allocation.rsvd_pg_bndy);
	} else if (HALMAC_TRX_MODE_LOOPBACK == halmac_trx_mode)
		transfer_mode = HALMAC_TRNSFER_LOOPBACK_DIRECT;
	else
		transfer_mode = HALMAC_TRNSFER_NORMAL;

	HALMAC_REG_WRITE_8(pHalmac_adapter, REG_CR + 3, (u8)transfer_mode);

	return HALMAC_RET_SUCCESS;
}
