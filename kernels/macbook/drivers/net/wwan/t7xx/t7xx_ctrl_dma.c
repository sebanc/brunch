// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#define dev_fmt(fmt) "cldma: " fmt

#include <linux/delay.h>
#include <linux/io.h>

#include "t7xx_ctrl_dma.h"

void cldma_clear_ip_busy(struct cldma_hw_info *hw_info)
{
	/* write 1 to clear IP busy register wake up cpu case */
	iowrite32(ioread32(hw_info->ap_pdn_base + REG_CLDMA_IP_BUSY) | IP_BUSY_WAKEUP,
		  hw_info->ap_pdn_base + REG_CLDMA_IP_BUSY);
}

void cldma_hw_reg_restore(struct cldma_hw_info *hw_info)
{
	u32 ul_cfg = ioread32(hw_info->ap_pdn_base + REG_CLDMA_UL_CFG);

	ul_cfg &= ~UL_CFG_BIT_MODE_MASK;
	if (hw_info->hw_mode == MODE_BIT_64)
		ul_cfg |= UL_CFG_BIT_MODE_64;
	else if (hw_info->hw_mode == MODE_BIT_40)
		ul_cfg |= UL_CFG_BIT_MODE_40;
	else if (hw_info->hw_mode == MODE_BIT_36)
		ul_cfg |= UL_CFG_BIT_MODE_36;

	iowrite32(ul_cfg, hw_info->ap_pdn_base + REG_CLDMA_UL_CFG);
	iowrite32(UL_DUMMY_ILL_MEM_CHECK_DIS,
		  hw_info->ap_pdn_base + REG_CLDMA_UL_DUMMY_0);
	iowrite32(SO_DUMMY_ILL_MEM_CHECK_DIS,
		  hw_info->ap_pdn_base + REG_CLDMA_SO_DUMMY_0);
}

void cldma_hw_start_queue(struct cldma_hw_info *hw_info, u8 qno, bool is_rx)
{
	u32 val = (qno == CLDMA_ALL_Q) ? CLDMA_ALL_Q : BIT(qno);
	void __iomem *reg_start_cmd;

	mb(); /* prevents outstanding GPD updates */
	reg_start_cmd = is_rx ? hw_info->ap_pdn_base + REG_CLDMA_SO_START_CMD :
				hw_info->ap_pdn_base + REG_CLDMA_UL_START_CMD;

	iowrite32(val, reg_start_cmd);
}

void cldma_hw_start(struct cldma_hw_info *hw_info)
{
	/* unmask txrx interrupt */
	iowrite32(TXRX_STATUS_BITMASK,
		  hw_info->ap_pdn_base + REG_CLDMA_L2TIMCR0);
	iowrite32(TXRX_STATUS_BITMASK,
		  hw_info->ap_ao_base + REG_CLDMA_L2RIMCR0);
	/* unmask empty queue interrupt */
	iowrite32(EMPTY_STATUS_BITMASK,
		  hw_info->ap_pdn_base + REG_CLDMA_L2TIMCR0);
	iowrite32(EMPTY_STATUS_BITMASK,
		  hw_info->ap_ao_base + REG_CLDMA_L2RIMCR0);
}

void cldma_hw_reset(void __iomem *ao_base)
{
	iowrite32(ioread32(ao_base + REG_INFRA_RST4_SET) | RST4_CLDMA1_SW_RST_SET,
		  ao_base + REG_INFRA_RST4_SET);
	iowrite32(ioread32(ao_base + REG_INFRA_RST2_SET) | RST2_CLDMA1_AO_SW_RST_SET,
		  ao_base + REG_INFRA_RST2_SET);
	udelay(1);
	iowrite32(ioread32(ao_base + REG_INFRA_RST4_CLR) | RST4_CLDMA1_SW_RST_CLR,
		  ao_base + REG_INFRA_RST4_CLR);
	iowrite32(ioread32(ao_base + REG_INFRA_RST2_CLR) | RST2_CLDMA1_AO_SW_RST_CLR,
		  ao_base + REG_INFRA_RST2_CLR);
}

bool cldma_tx_addr_is_set(struct cldma_hw_info *hw_info, unsigned char qno)
{
	u32 start_addr_l = ioread32(hw_info->ap_pdn_base +
				    REG_CLDMA_UL_START_ADDRL_0 + qno * 8);
	u32 start_addr_h = ioread32(hw_info->ap_pdn_base +
				    REG_CLDMA_UL_START_ADDRL_0 + qno * 8 + 4);

	return start_addr_l || start_addr_h;
}

void cldma_hw_set_start_address(struct cldma_hw_info *hw_info,
				unsigned char qno, u64 address, bool is_rx)
{
	void __iomem *reg_start_addr;

	reg_start_addr = is_rx ? hw_info->ap_ao_base + REG_CLDMA_SO_START_ADDRL_0 :
				 hw_info->ap_pdn_base + REG_CLDMA_UL_START_ADDRL_0;

	iowrite32((u32)address, reg_start_addr + qno * 8);
	iowrite32((u32)(address >> 32), reg_start_addr + qno * 8 + 4);
}

void cldma_hw_resume_queue(struct cldma_hw_info *hw_info,
			   unsigned char qno, bool is_rx)
{
	void __iomem *reg_resume_cmd;

	mb(); /* prevents outstanding GPD updates */
	reg_resume_cmd = is_rx ? hw_info->ap_pdn_base + REG_CLDMA_SO_RESUME_CMD :
				 hw_info->ap_pdn_base + REG_CLDMA_UL_RESUME_CMD;

	iowrite32(BIT(qno), reg_resume_cmd);
}

unsigned int cldma_hw_queue_status(struct cldma_hw_info *hw_info,
				   unsigned char qno, bool is_rx)
{
	u32 mask = (qno == CLDMA_ALL_Q) ? CLDMA_ALL_Q : BIT(qno);
	void __iomem *reg_status;

	reg_status = is_rx ? hw_info->ap_ao_base + REG_CLDMA_SO_STATUS :
			     hw_info->ap_pdn_base + REG_CLDMA_UL_STATUS;

	return ioread32(reg_status) & mask;
}

void cldma_hw_tx_done(struct cldma_hw_info *hw_info, unsigned int bitmask)
{
	unsigned int ch_id;

	ch_id = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2TISAR0) & bitmask;
	/* ack interrupt */
	iowrite32(ch_id, hw_info->ap_pdn_base + REG_CLDMA_L2TISAR0);
	ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2TISAR0);
}

void cldma_hw_rx_done(struct cldma_hw_info *hw_info, unsigned int bitmask)
{
	unsigned int ch_id;

	ch_id = ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2RISAR0) & bitmask;
	/* ack interrupt */
	iowrite32(ch_id, hw_info->ap_pdn_base + REG_CLDMA_L2RISAR0);
	ioread32(hw_info->ap_pdn_base + REG_CLDMA_L2RISAR0);
}

unsigned int cldma_hw_int_status(struct cldma_hw_info *hw_info,
				 unsigned int bitmask, bool is_rx)
{
	void __iomem *reg_int_sta;

	reg_int_sta = is_rx ? hw_info->ap_pdn_base + REG_CLDMA_L2RISAR0 :
			      hw_info->ap_pdn_base + REG_CLDMA_L2TISAR0;

	return ioread32(reg_int_sta) & bitmask;
}

void cldma_hw_mask_txrxirq(struct cldma_hw_info *hw_info,
			   unsigned char qno, bool is_rx)
{
	u32 val = (qno == CLDMA_ALL_Q) ? CLDMA_ALL_Q : BIT(qno);
	void __iomem *reg_ims;

	/* select the right interrupt mask set register */
	reg_ims = is_rx ? hw_info->ap_ao_base + REG_CLDMA_L2RIMSR0 :
			  hw_info->ap_pdn_base + REG_CLDMA_L2TIMSR0;

	iowrite32(val, reg_ims);
}

void cldma_hw_mask_eqirq(struct cldma_hw_info *hw_info,
			 unsigned char qno, bool is_rx)
{
	u32 val = (qno == CLDMA_ALL_Q) ? CLDMA_ALL_Q : BIT(qno);
	void __iomem *reg_ims;

	/* select the right interrupt mask set register */
	reg_ims = is_rx ? hw_info->ap_ao_base + REG_CLDMA_L2RIMSR0 :
			  hw_info->ap_pdn_base + REG_CLDMA_L2TIMSR0;

	iowrite32(val << EQ_STA_BIT_OFFSET, reg_ims);
}

void cldma_hw_dismask_txrxirq(struct cldma_hw_info *hw_info,
			      unsigned char qno, bool is_rx)
{
	u32 val = (qno == CLDMA_ALL_Q) ? CLDMA_ALL_Q : BIT(qno);
	void __iomem *reg_imc;

	/* select the right interrupt mask clear register */
	reg_imc = is_rx ? hw_info->ap_ao_base + REG_CLDMA_L2RIMCR0 :
			  hw_info->ap_pdn_base + REG_CLDMA_L2TIMCR0;

	iowrite32(val, reg_imc);
}

void cldma_hw_dismask_eqirq(struct cldma_hw_info *hw_info,
			    unsigned char qno, bool is_rx)
{
	u32 val = (qno == CLDMA_ALL_Q) ? CLDMA_ALL_Q : BIT(qno);
	void __iomem *reg_imc;

	/* select the right interrupt mask clear register */
	reg_imc = is_rx ? hw_info->ap_ao_base + REG_CLDMA_L2RIMCR0 :
			  hw_info->ap_pdn_base + REG_CLDMA_L2TIMCR0;

	iowrite32(val << EQ_STA_BIT_OFFSET, reg_imc);
}

void cldma_hw_init(struct cldma_hw_info *hw_info)
{
	u32 ul_cfg = ioread32(hw_info->ap_pdn_base + REG_CLDMA_UL_CFG);
	u32 so_cfg = ioread32(hw_info->ap_ao_base + REG_CLDMA_SO_CFG);

	/* configure the DRAM address mode */
	ul_cfg &= ~UL_CFG_BIT_MODE_MASK;
	so_cfg &= ~SO_CFG_BIT_MODE_MASK;
	if (hw_info->hw_mode == MODE_BIT_64) {
		ul_cfg |= UL_CFG_BIT_MODE_64;
		so_cfg |= SO_CFG_BIT_MODE_64;
	} else if (hw_info->hw_mode == MODE_BIT_40) {
		ul_cfg |= UL_CFG_BIT_MODE_40;
		so_cfg |= SO_CFG_BIT_MODE_40;
	} else if (hw_info->hw_mode == MODE_BIT_36) {
		ul_cfg |= UL_CFG_BIT_MODE_36;
		so_cfg |= SO_CFG_BIT_MODE_36;
	}

	iowrite32(ul_cfg, hw_info->ap_pdn_base + REG_CLDMA_UL_CFG);
	so_cfg |= SO_CFG_UP_HW_LAST;
	iowrite32(so_cfg, hw_info->ap_ao_base + REG_CLDMA_SO_CFG);
	/* enable interrupt */
	iowrite32(0x0, hw_info->ap_ao_base + REG_CLDMA_INT_MASK);
	/* mask wakeup signal */
	iowrite32(BUSY_MASK_MD, hw_info->ap_ao_base + REG_CLDMA_BUSY_MASK);
	/* disable TX and RX invalid address check */
	iowrite32(UL_DUMMY_ILL_MEM_CHECK_DIS,
		  hw_info->ap_pdn_base + REG_CLDMA_UL_DUMMY_0);
	iowrite32(SO_DUMMY_ILL_MEM_CHECK_DIS,
		  hw_info->ap_pdn_base + REG_CLDMA_SO_DUMMY_0);
}

void cldma_hw_stop_queue(struct cldma_hw_info *hw_info, u8 qno, bool is_rx)
{
	u32 val = (qno == CLDMA_ALL_Q) ? CLDMA_ALL_Q : BIT(qno);
	void __iomem *reg_stop_cmd;

	reg_stop_cmd = is_rx ? hw_info->ap_pdn_base + REG_CLDMA_SO_STOP_CMD :
			       hw_info->ap_pdn_base + REG_CLDMA_UL_STOP_CMD;

	iowrite32(val, reg_stop_cmd);
}

void cldma_hw_stop(struct cldma_hw_info *hw_info, bool is_rx)
{
	void __iomem *reg_ims;

	/* select the right L2 interrupt mask set register */
	reg_ims = is_rx ? hw_info->ap_ao_base + REG_CLDMA_L2RIMSR0 :
			  hw_info->ap_pdn_base + REG_CLDMA_L2TIMSR0;

	iowrite32(TXRX_STATUS_BITMASK, reg_ims);
	iowrite32(EMPTY_STATUS_BITMASK, reg_ims);
}
