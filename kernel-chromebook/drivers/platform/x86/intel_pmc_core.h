/*
 * Intel Core SoC Power Management Controller Header File
 *
 * Copyright (c) 2016, Intel Corporation.
 * All Rights Reserved.
 *
 * Authors: Rajneesh Bhardwaj <rajneesh.bhardwaj@intel.com>
 *          Vishwanath Somayaji <vishwanath.somayaji@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef PMC_CORE_H
#define PMC_CORE_H

/* Sunrise Point Power Management Controller PCI Device ID */
#define SPT_PMC_PCI_DEVICE_ID			0x9d21

/* PCI Device non-standard MMIO & IO resouce BAR register offset */
#define SPT_PMC_IO_ADDR_OFFSET			0x40
#define SPT_PMC_BASE_ADDR_OFFSET		0x48

/* IO space registers */
#define SPT_PMC_PM1_EN_STS_OFFSET		0x00
#define SPT_PMC_GPE0_STS_OFFSET			0x80
#define SPT_PMC_GPE0_EN_OFFSET			0x90
#define SPT_PMC_GPE0_STS_127_96_OFFSET		0x8c
#define SPT_PMC_GPE0_EN_127_96_OFFSET		0x9c

#define SPT_PMC_GPE0_127_96_REG_INDEX		0x03
#define SPT_PMC_GPE0_REG_LEN			0x04

#define SPT_PMC_SLP_S0_RES_COUNTER_OFFSET	0x13c
#define SPT_PMC_PM_CFG_OFFSET			0x18
#define SPT_PMC_PM_STS_OFFSET			0x1c
#define SPT_PMC_MTPMC_OFFSET			0x20
#define SPT_PMC_MFPMC_OFFSET			0x38
#define SPT_PMC_LTR_IGNORE_OFFSET		0x30C
#define SPT_PMC_MPHY_CORE_STS_0			0x1143
#define SPT_PMC_MPHY_CORE_STS_1			0x1142
#define SPT_PMC_MPHY_COM_STS_0			0x1155
#define SPT_PMC_MMIO_REG_LEN			0x1000
#define SPT_PMC_IO_REG_LEN			0x100
#define SPT_PMC_SLP_S0_RES_COUNTER_STEP		0x64
#define PMC_BASE_ADDR_MASK			~(SPT_PMC_MMIO_REG_LEN - 1)
#define PMC_IO_ADDR_MASK			GENMASK_ULL(31, 8)
#define MTPMC_MASK				0xffff0000
#define NUM_ENTRIES				5
#define SPT_PMC_READ_DISABLE_BIT		0x16
#define SPT_PMC_MSG_FULL_STS_BIT		0x18
#define NUM_RETRIES				100
#define NUM_IP_IGN_ALLOWED			17

/* Sunrise Point: PGD PFET Enable Ack Status Registers */
enum ppfear_regs {
	SPT_PMC_XRAM_PPFEAR0A = 0x590,
	SPT_PMC_XRAM_PPFEAR0B,
	SPT_PMC_XRAM_PPFEAR0C,
	SPT_PMC_XRAM_PPFEAR0D,
	SPT_PMC_XRAM_PPFEAR1A,
};

#define SPT_PMC_BIT_PMC				BIT(0)
#define SPT_PMC_BIT_OPI				BIT(1)
#define SPT_PMC_BIT_SPI				BIT(2)
#define SPT_PMC_BIT_XHCI			BIT(3)
#define SPT_PMC_BIT_SPA				BIT(4)
#define SPT_PMC_BIT_SPB				BIT(5)
#define SPT_PMC_BIT_SPC				BIT(6)
#define SPT_PMC_BIT_GBE				BIT(7)

#define SPT_PMC_BIT_SATA			BIT(0)
#define SPT_PMC_BIT_HDA_PGD0			BIT(1)
#define SPT_PMC_BIT_HDA_PGD1			BIT(2)
#define SPT_PMC_BIT_HDA_PGD2			BIT(3)
#define SPT_PMC_BIT_HDA_PGD3			BIT(4)
#define SPT_PMC_BIT_RSVD_0B			BIT(5)
#define SPT_PMC_BIT_LPSS			BIT(6)
#define SPT_PMC_BIT_LPC				BIT(7)

#define SPT_PMC_BIT_SMB				BIT(0)
#define SPT_PMC_BIT_ISH				BIT(1)
#define SPT_PMC_BIT_P2SB			BIT(2)
#define SPT_PMC_BIT_DFX				BIT(3)
#define SPT_PMC_BIT_SCC				BIT(4)
#define SPT_PMC_BIT_RSVD_0C			BIT(5)
#define SPT_PMC_BIT_FUSE			BIT(6)
#define SPT_PMC_BIT_CAMREA			BIT(7)

#define SPT_PMC_BIT_RSVD_0D			BIT(0)
#define SPT_PMC_BIT_USB3_OTG			BIT(1)
#define SPT_PMC_BIT_EXI				BIT(2)
#define SPT_PMC_BIT_CSE				BIT(3)
#define SPT_PMC_BIT_CSME_KVM			BIT(4)
#define SPT_PMC_BIT_CSME_PMT			BIT(5)
#define SPT_PMC_BIT_CSME_CLINK			BIT(6)
#define SPT_PMC_BIT_CSME_PTIO			BIT(7)

#define SPT_PMC_BIT_CSME_USBR			BIT(0)
#define SPT_PMC_BIT_CSME_SUSRAM			BIT(1)
#define SPT_PMC_BIT_CSME_SMT			BIT(2)
#define SPT_PMC_BIT_RSVD_1A			BIT(3)
#define SPT_PMC_BIT_CSME_SMS2			BIT(4)
#define SPT_PMC_BIT_CSME_SMS1			BIT(5)
#define SPT_PMC_BIT_CSME_RTC			BIT(6)
#define SPT_PMC_BIT_CSME_PSF			BIT(7)

#define SPT_PMC_BIT_MPHY_LANE0			BIT(0)
#define SPT_PMC_BIT_MPHY_LANE1			BIT(1)
#define SPT_PMC_BIT_MPHY_LANE2			BIT(2)
#define SPT_PMC_BIT_MPHY_LANE3			BIT(3)
#define SPT_PMC_BIT_MPHY_LANE4			BIT(4)
#define SPT_PMC_BIT_MPHY_LANE5			BIT(5)
#define SPT_PMC_BIT_MPHY_LANE6			BIT(6)
#define SPT_PMC_BIT_MPHY_LANE7			BIT(7)

#define SPT_PMC_BIT_MPHY_LANE8			BIT(0)
#define SPT_PMC_BIT_MPHY_LANE9			BIT(1)
#define SPT_PMC_BIT_MPHY_LANE10			BIT(2)
#define SPT_PMC_BIT_MPHY_LANE11			BIT(3)
#define SPT_PMC_BIT_MPHY_LANE12			BIT(4)
#define SPT_PMC_BIT_MPHY_LANE13			BIT(5)
#define SPT_PMC_BIT_MPHY_LANE14			BIT(6)
#define SPT_PMC_BIT_MPHY_LANE15			BIT(7)

#define SPT_PMC_BIT_MPHY_CMN_LANE0		BIT(0)
#define SPT_PMC_BIT_MPHY_CMN_LANE1		BIT(1)
#define SPT_PMC_BIT_MPHY_CMN_LANE2		BIT(2)
#define SPT_PMC_BIT_MPHY_CMN_LANE3		BIT(3)

/* PM1_EN_STS register bit definitions */
#define SPT_PMC_BIT_TMROF_STS			BIT(0)
#define SPT_PMC_BIT_BM_STS			BIT(4)
#define SPT_PMC_BIT_GBL_STS			BIT(5)
#define SPT_PMC_BIT_PWRBTN_STS			BIT(8)
#define SPT_PMC_BIT_RTC_STS			BIT(10)
#define SPT_PMC_BIT_PWRBTNOR_STS		BIT(11)
#define SPT_PMC_BIT_PCIEXP_WAKE_STS		BIT(14)
#define SPT_PMC_BIT_WAKE_STS			BIT(15)

/* GPE0_STS register bit definitions */
#define SPT_PMC_BIT_SMB_WAKE_STS		BIT(7)

struct pmc_bit_map {
	const char *name;
	u32 bit_mask;
};

struct pmc_reg_map {
	const struct pmc_bit_map *pfear_sts;
	const struct pmc_bit_map *mphy_sts;
	const struct pmc_bit_map *pll_sts;
	const struct pmc_bit_map *msr_sts;
	const struct pmc_bit_map *pm1_en_sts;
	const struct pmc_bit_map *gpe0_sts;
};

/**
 * struct pmc_dev - pmc device structure
 * @base_addr:		comtains pmc base address
 * @regbase:		pointer to io-remapped memory location
 * @dbgfs_dir:		path to debug fs interface
 * @feature_available:	flag to indicate whether
 *			the feature is available
 *			on a particular platform or not.
 * pm1_en_sts_val	pm1_en_sts register value, read during
 *			resume_early(), used to report wake source
 *			in debugfs
 * gpe0_sts_val		gpe0_sts_val register value, read during
 *			resume_early(), used to report wake source
 *			in debugfs
 * wake_source		Wake source string
 *
 * pmc_dev contains info about power management controller device.
 */
struct pmc_dev {
	u32 base_addr;
	void __iomem *regbase;
	u32 io_addr;
	void __iomem *iobase;
	const struct pmc_reg_map *map;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs_dir;
#endif /* CONFIG_DEBUG_FS */
	bool has_slp_s0_res;
	int pmc_xram_read_bit;
	struct mutex lock; /* generic mutex lock for PMC Core */
	u32 pm1_en_sts_val;
	u32 gpe0_sts_val[4];
	char wake_source[1024];
};

#endif /* PMC_CORE_H */
