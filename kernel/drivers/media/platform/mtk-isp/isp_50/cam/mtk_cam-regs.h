/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_REGS_H__
#define __MTK_CAM_REGS_H__

/* ISP interrupt enable */
#define REG_CTL_RAW_INT_EN		0x0020
#define DMA_ERR_INT_EN			BIT(29)

/* ISP interrupt status */
#define REG_CTL_RAW_INT_STAT		0x0024
#define VS_INT_ST			BIT(0)
#define TG_ERR_ST			BIT(4)
#define TG_GBERR_ST			BIT(5)
#define CQ_CODE_ERR_ST			BIT(6)
#define CQ_APB_ERR_ST			BIT(7)
#define CQ_VS_ERR_ST			BIT(8)
#define HW_PASS1_DON_ST			BIT(11)
#define SOF_INT_ST			BIT(12)
#define AMX_ERR_ST			BIT(15)
#define RMX_ERR_ST			BIT(16)
#define BMX_ERR_ST			BIT(17)
#define RRZO_ERR_ST			BIT(18)
#define AFO_ERR_ST			BIT(19)
#define IMGO_ERR_ST			BIT(20)
#define AAO_ERR_ST			BIT(21)
#define PSO_ERR_ST			BIT(22)
#define LCSO_ERR_ST			BIT(23)
#define BNR_ERR_ST			BIT(24)
#define LSCI_ERR_ST			BIT(25)
#define DMA_ERR_ST			BIT(29)
#define SW_PASS1_DON_ST			BIT(30)

/* ISP interrupt 2 status */
#define REG_CTL_RAW_INT2_STAT		0x0034
#define AFO_DONE_ST			BIT(5)
#define AAO_DONE_ST			BIT(7)

/* Configures sensor mode */
#define REG_TG_SEN_MODE			0x0230
#define TG_SEN_MODE_CMOS_EN		BIT(0)

/* View finder mode control */
#define REG_TG_VF_CON			0x0234
#define TG_VF_CON_VFDATA_EN		BIT(0)

/* View finder mode control */
#define REG_TG_INTER_ST			0x026c
#define TG_CS_MASK			0x3f00
#define TG_IDLE_ST			BIT(8)

/* IMGO error status register */
#define REG_IMGO_ERR_STAT		0x1360
/* RRZO error status register */
#define REG_RRZO_ERR_STAT		0x1364
/* AAO error status register */
#define REG_AAO_ERR_STAT		0x1368
/* AFO error status register */
#define REG_AFO_ERR_STAT		0x136c
/* LCSO error status register */
#define REG_LCSO_ERR_STAT		0x1370
/* BPCI error status register */
#define REG_BPCI_ERR_STAT		0x137c
/* LSCI error status register */
#define REG_LSCI_ERR_STAT		0x1384
/* LMVO error status register */
#define REG_LMVO_ERR_STAT		0x1390
/* FLKO error status register */
#define REG_FLKO_ERR_STAT		0x1394
/* PSO error status register */
#define REG_PSO_ERR_STAT		0x13a0

/* CQ0 base address */
#define REG_CQ_THR0_BASEADDR		0x0198
/* Frame sequence number */
#define REG_FRAME_SEQ_NUM		0x13b8

/* Spare register for meta0 vb2 index */
#define REG_META0_VB2_INDEX		0x14dc
/* Spare register for meta1 vb2 index */
#define REG_META1_VB2_INDEX		0x151c

/* AAO FBC's status */
#define REG_AAO_FBC_STATUS		0x013c
/* AFO FBC's status */
#define REG_AFO_FBC_STATUS		0x0134

/* IRQ Error Mask */
#define INT_ST_MASK_CAM_ERR		( \
					TG_ERR_ST |\
					TG_GBERR_ST |\
					CQ_CODE_ERR_ST |\
					CQ_APB_ERR_ST |\
					CQ_VS_ERR_ST |\
					BNR_ERR_ST |\
					RMX_ERR_ST |\
					BMX_ERR_ST |\
					BNR_ERR_ST |\
					LSCI_ERR_ST |\
					DMA_ERR_ST)

#endif	/* __MTK_CAM_REGS_H__ */
