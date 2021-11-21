/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */
#ifndef __MODEM_CLDMAHWDRV_H__
#define __MODEM_CLDMAHWDRV_H__

#define CLDMA_TXQ_NUM			8
#define CLDMA_RXQ_NUM			8
#define CLDMA_ALL_Q			0xFF

/* interrupt status bit meaning, bitmask */
#define STATUS_BITMASK			0xFFFF
#define EQ_STA_BIT_OFFSET		8
#define EMPTY_STATUS_BITMASK		0xFF00
#define TXRX_STATUS_BITMASK		0x00FF

/* L2RISAR0 */
#define TQ_ERR_INT_BITMASK		0x00FF0000
#define TQ_ACTIVE_START_ERR_INT_BITMASK	0xFF000000

#define RQ_ERR_INT_BITMASK		0x00FF0000
#define RQ_ACTIVE_START_ERR_INT_BITMASK	0xFF000000

/* HW address @sAP view for reference, eAP is the left user of CLDMA0/CLDMA1 */
#define RIGHT_CLDMA_OFFSET		0x1000

#define CLDMA0_AO_BASE			0x10049000
#define CLDMA0_PD_BASE			0x1021D000
#define CLDMA1_AO_BASE			0x1004B000
#define CLDMA1_PD_BASE			0x1021F000

#define CLDMA_R_AO_BASE			0x10023000
#define CLDMA_R_PD_BASE			0x1023D000

/* CLDMA IN(Tx) AO */
#define REG_CLDMA_UL_CURRENT_ADDRL_0_AO	0x0044
#define REG_CLDMA_UL_CURRENT_ADDRH_0_AO	0x0048

/* CLDMA IN(Tx) PD */
#define REG_CLDMA_UL_START_ADDRL_0	0x0004
#define REG_CLDMA_UL_START_ADDRH_0	0x0008
#define REG_CLDMA_UL_CURRENT_ADDRL_0	0x0044
#define REG_CLDMA_UL_CURRENT_ADDRH_0	0x0048
#define REG_CLDMA_UL_STATUS		0x0084
#define REG_CLDMA_UL_START_CMD		0x0088
#define REG_CLDMA_UL_RESUME_CMD		0x008C
#define REG_CLDMA_UL_STOP_CMD		0x0090
#define REG_CLDMA_UL_ERROR		0x0094
#define REG_CLDMA_UL_CFG		0x0098
#define UL_CFG_BIT_MODE_MASK		GENMASK(7, 5)
#define UL_CFG_BIT_MODE_64		BIT(7)
#define UL_CFG_BIT_MODE_40		BIT(6)
#define UL_CFG_BIT_MODE_36		BIT(5)
#define REG_CLDMA_UL_DUMMY_0		0x009C
#define UL_DUMMY_ILL_MEM_CHECK_DIS	BIT(0)

/* CLDMA OUT(Rx) PD */
#define REG_CLDMA_SO_ERROR		0x0500
#define REG_CLDMA_SO_START_CMD		0x05BC
#define REG_CLDMA_SO_RESUME_CMD		0x05C0
#define REG_CLDMA_SO_STOP_CMD		0x05C4
#define REG_CLDMA_SO_DUMMY_0		0x0508
#define SO_DUMMY_ILL_MEM_CHECK_DIS	BIT(0)

/* CLDMA OUT(Rx) AO */
#define REG_CLDMA_SO_CFG		0x0404
#define SO_CFG_BIT_MODE_MASK		GENMASK(12, 10)
#define SO_CFG_BIT_MODE_64		BIT(12)
#define SO_CFG_BIT_MODE_40		BIT(11)
#define SO_CFG_BIT_MODE_36		BIT(10)
#define SO_CFG_UP_HW_LAST		BIT(2)
#define REG_CLDMA_SO_START_ADDRL_0	0x0478
#define REG_CLDMA_SO_START_ADDRH_0	0x047C
#define REG_CLDMA_SO_CURRENT_ADDRL_0	0x04B8
#define REG_CLDMA_SO_CURRENT_ADDRH_0	0x04BC
#define REG_CLDMA_SO_STATUS		0x04F8
#define REG_CLDMA_DEBUG_ID_EN		0x04FC

/* CLDMA MISC PD */
#define REG_CLDMA_L2TISAR0		0x0810
#define REG_CLDMA_L2TISAR1		0x0814
#define REG_CLDMA_L2TIMR0		0x0818
#define REG_CLDMA_L2TIMR1		0x081C
#define REG_CLDMA_L2TIMCR0		0x0820
#define REG_CLDMA_L2TIMCR1		0x0824
#define REG_CLDMA_L2TIMSR0		0x0828
#define REG_CLDMA_L2TIMSR1		0x082C
#define REG_CLDMA_L3TISAR0		0x0830
#define REG_CLDMA_L3TISAR1		0x0834
#define REG_CLDMA_L3TIMR0		0x0838
#define REG_CLDMA_L3TIMR1		0x083C
#define REG_CLDMA_L3TIMCR0		0x0840
#define REG_CLDMA_L3TIMCR1		0x0844
#define REG_CLDMA_L3TIMSR0		0x0848
#define REG_CLDMA_L3TIMSR1		0x084C
#define REG_CLDMA_L2RISAR0		0x0850
#define REG_CLDMA_L2RISAR1		0x0854
#define REG_CLDMA_L3RISAR0		0x0870
#define REG_CLDMA_L3RISAR1		0x0874
#define REG_CLDMA_L3RIMR0		0x0878
#define REG_CLDMA_L3RIMR1		0x087C
#define REG_CLDMA_L3RIMCR0		0x0880
#define REG_CLDMA_L3RIMCR1		0x0884
#define REG_CLDMA_L3RIMSR0		0x0888
#define REG_CLDMA_L3RIMSR1		0x088C
#define REG_CLDMA_IP_BUSY		0x08B4
#define IP_BUSY_WAKEUP			BIT(0)
#define REG_CLDMA_L3TISAR2		0x08C0
#define REG_CLDMA_L3TIMR2		0x08C4
#define REG_CLDMA_L3TIMCR2		0x08C8
#define REG_CLDMA_L3TIMSR2		0x08CC

/* CLDMA MISC AO */
#define REG_CLDMA_L2RIMR0		0x0858
#define REG_CLDMA_L2RIMR1		0x085C
#define REG_CLDMA_L2RIMCR0		0x0860
#define REG_CLDMA_L2RIMCR1		0x0864
#define REG_CLDMA_L2RIMSR0		0x0868
#define REG_CLDMA_L2RIMSR1		0x086C
#define REG_CLDMA_BUSY_MASK		0x0954
#define BUSY_MASK_MD			BIT(2)
#define BUSY_MASK_AP			BIT(1)
#define BUSY_MASK_PCIE			BIT(0)
#define REG_CLDMA_INT_MASK		0x0960

/* CLDMA RESET */
#define REG_INFRA_RST4_SET		0x730
#define RST4_CLDMA1_SW_RST_SET		BIT(20)
#define REG_INFRA_RST4_CLR		0x734
#define RST4_CLDMA1_SW_RST_CLR		BIT(20)
#define REG_INFRA_RST2_SET		0x140
#define RST2_CLDMA1_AO_SW_RST_SET	BIT(18)
#define REG_INFRA_RST2_CLR		0x144
#define RST2_CLDMA1_AO_SW_RST_CLR	BIT(18)

enum hw_mode {
	MODE_BIT_32 = 0,
	MODE_BIT_36 = 1,
	MODE_BIT_40 = 2,
	MODE_BIT_64 = 3,
};

struct cldma_hw_info {
	enum hw_mode hw_mode;
	void __iomem *ap_ao_base;
	void __iomem *ap_pdn_base;
	u32 phy_interrupt_id;
};

void cldma_hw_mask_txrxirq(struct cldma_hw_info *hw_info,
			   unsigned char qno, bool is_rx);
void cldma_hw_mask_eqirq(struct cldma_hw_info *hw_info,
			 unsigned char qno, bool is_rx);
void cldma_hw_dismask_txrxirq(struct cldma_hw_info *hw_info,
			      unsigned char qno, bool is_rx);
void cldma_hw_dismask_eqirq(struct cldma_hw_info *hw_info,
			    unsigned char qno, bool is_rx);
unsigned int cldma_hw_queue_status(struct cldma_hw_info *hw_info,
				   unsigned char qno, bool is_rx);
void cldma_hw_init(struct cldma_hw_info *hw_info);
void cldma_hw_resume_queue(struct cldma_hw_info *hw_info,
			   unsigned char qno, bool is_rx);
void cldma_hw_start(struct cldma_hw_info *hw_info);
void cldma_hw_start_queue(struct cldma_hw_info *hw_info, u8 qno, bool is_rx);
void cldma_hw_tx_done(struct cldma_hw_info *hw_info, unsigned int bitmask);
void cldma_hw_rx_done(struct cldma_hw_info *hw_info, unsigned int bitmask);
void cldma_hw_stop_queue(struct cldma_hw_info *hw_info, u8 qno, bool is_rx);
void cldma_hw_set_start_address(struct cldma_hw_info *hw_info,
				unsigned char qno, u64 address, bool is_rx);
void cldma_hw_reset(void __iomem *ao_base);
void cldma_hw_stop(struct cldma_hw_info *hw_info, bool is_rx);
unsigned int cldma_hw_int_status(struct cldma_hw_info *hw_info,
				 unsigned int bitmask, bool is_rx);
void cldma_hw_reg_restore(struct cldma_hw_info *hw_info);
void cldma_clear_ip_busy(struct cldma_hw_info *hw_info);
bool cldma_tx_addr_is_set(struct cldma_hw_info *hw_info, unsigned char qno);
#endif
