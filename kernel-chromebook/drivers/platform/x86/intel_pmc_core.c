/*
 * Intel Core SoC Power Management Controller Driver
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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/uaccess.h>

#include <asm/cpu_device_id.h>
#include <asm/intel-family.h>
#include <asm/pmc_core.h>
#include <asm/msr.h>

#include "intel_pmc_core.h"

static struct pmc_dev pmc;

/* PKGC MSRs are common across Intel Core cpus */
static const struct pmc_bit_map msr_map[] = {
	{"Package C2",			MSR_PKG_C2_RESIDENCY},
	{"Package C3",			MSR_PKG_C3_RESIDENCY},
	{"Package C6",			MSR_PKG_C6_RESIDENCY},
	{"Package C7",			MSR_PKG_C7_RESIDENCY},
	{"Package C8",			MSR_PKG_C8_RESIDENCY},
	{"Package C9",			MSR_PKG_C9_RESIDENCY},
	{"Package C10",			MSR_PKG_C10_RESIDENCY},
	{},

};

static const struct pmc_bit_map spt_pll_map[] = {
	{"MIPI PLL",			SPT_PMC_BIT_MPHY_CMN_LANE0},
	{"GEN2 USB2PCIE2 PLL",		SPT_PMC_BIT_MPHY_CMN_LANE1},
	{"DMIPCIE3 PLL",		SPT_PMC_BIT_MPHY_CMN_LANE2},
	{"SATA PLL",			SPT_PMC_BIT_MPHY_CMN_LANE3},
	{},
};

static const struct pmc_bit_map spt_mphy_map[] = {
	{"MPHY CORE LANE 0",           SPT_PMC_BIT_MPHY_LANE0},
	{"MPHY CORE LANE 1",           SPT_PMC_BIT_MPHY_LANE1},
	{"MPHY CORE LANE 2",           SPT_PMC_BIT_MPHY_LANE2},
	{"MPHY CORE LANE 3",           SPT_PMC_BIT_MPHY_LANE3},
	{"MPHY CORE LANE 4",           SPT_PMC_BIT_MPHY_LANE4},
	{"MPHY CORE LANE 5",           SPT_PMC_BIT_MPHY_LANE5},
	{"MPHY CORE LANE 6",           SPT_PMC_BIT_MPHY_LANE6},
	{"MPHY CORE LANE 7",           SPT_PMC_BIT_MPHY_LANE7},
	{"MPHY CORE LANE 8",           SPT_PMC_BIT_MPHY_LANE8},
	{"MPHY CORE LANE 9",           SPT_PMC_BIT_MPHY_LANE9},
	{"MPHY CORE LANE 10",          SPT_PMC_BIT_MPHY_LANE10},
	{"MPHY CORE LANE 11",          SPT_PMC_BIT_MPHY_LANE11},
	{"MPHY CORE LANE 12",          SPT_PMC_BIT_MPHY_LANE12},
	{"MPHY CORE LANE 13",          SPT_PMC_BIT_MPHY_LANE13},
	{"MPHY CORE LANE 14",          SPT_PMC_BIT_MPHY_LANE14},
	{"MPHY CORE LANE 15",          SPT_PMC_BIT_MPHY_LANE15},
	{},
};

static const struct pmc_bit_map spt_pfear_map[] = {
	{"PMC",				SPT_PMC_BIT_PMC},
	{"OPI-DMI",			SPT_PMC_BIT_OPI},
	{"SPI / eSPI",			SPT_PMC_BIT_SPI},
	{"XHCI",			SPT_PMC_BIT_XHCI},
	{"SPA",				SPT_PMC_BIT_SPA},
	{"SPB",				SPT_PMC_BIT_SPB},
	{"SPC",				SPT_PMC_BIT_SPC},
	{"GBE",				SPT_PMC_BIT_GBE},
	{"SATA",			SPT_PMC_BIT_SATA},
	{"HDA-PGD0",			SPT_PMC_BIT_HDA_PGD0},
	{"HDA-PGD1",			SPT_PMC_BIT_HDA_PGD1},
	{"HDA-PGD2",			SPT_PMC_BIT_HDA_PGD2},
	{"HDA-PGD3",			SPT_PMC_BIT_HDA_PGD3},
	{"RSVD",			SPT_PMC_BIT_RSVD_0B},
	{"LPSS",			SPT_PMC_BIT_LPSS},
	{"LPC",				SPT_PMC_BIT_LPC},
	{"SMB",				SPT_PMC_BIT_SMB},
	{"ISH",				SPT_PMC_BIT_ISH},
	{"P2SB",			SPT_PMC_BIT_P2SB},
	{"DFX",				SPT_PMC_BIT_DFX},
	{"SCC",				SPT_PMC_BIT_SCC},
	{"RSVD",			SPT_PMC_BIT_RSVD_0C},
	{"FUSE",			SPT_PMC_BIT_FUSE},
	{"CAMERA",			SPT_PMC_BIT_CAMREA},
	{"RSVD",			SPT_PMC_BIT_RSVD_0D},
	{"USB3-OTG",			SPT_PMC_BIT_USB3_OTG},
	{"EXI",				SPT_PMC_BIT_EXI},
	{"CSE",				SPT_PMC_BIT_CSE},
	{"CSME_KVM",			SPT_PMC_BIT_CSME_KVM},
	{"CSME_PMT",			SPT_PMC_BIT_CSME_PMT},
	{"CSME_CLINK",			SPT_PMC_BIT_CSME_CLINK},
	{"CSME_PTIO",			SPT_PMC_BIT_CSME_PTIO},
	{"CSME_USBR",			SPT_PMC_BIT_CSME_USBR},
	{"CSME_SUSRAM",			SPT_PMC_BIT_CSME_SUSRAM},
	{"CSME_SMT",			SPT_PMC_BIT_CSME_SMT},
	{"RSVD",			SPT_PMC_BIT_RSVD_1A},
	{"CSME_SMS2",			SPT_PMC_BIT_CSME_SMS2},
	{"CSME_SMS1",			SPT_PMC_BIT_CSME_SMS1},
	{"CSME_RTC",			SPT_PMC_BIT_CSME_RTC},
	{"CSME_PSF",			SPT_PMC_BIT_CSME_PSF},
	{},
};

static const struct pmc_bit_map spt_pm1_en_sts_map[] = {
	{"Power-Button",		SPT_PMC_BIT_PWRBTN_STS},
	{"RTC",				SPT_PMC_BIT_RTC_STS},
	{"PCI-Express-Wake",		SPT_PMC_BIT_PCIEXP_WAKE_STS},
	{},
};

static const struct pmc_bit_map spt_gpe0_sts_map[] = {
	{"Hot-Plug",			BIT(1)},
	{"Software-GPE",		BIT(2)},
	{"TCO-SCI",			BIT(6)},
	{"SMBus-Wake",			BIT(7)},
	{"PCI-Express",			BIT(9)},
	{"Battery-Low",			BIT(10)},
	{"PME",				BIT(11)},
	{"PME-Bus0",			BIT(13)},
	{"GPIO-Tier2",			BIT(15)},
	{"LAN_Wake#",			BIT(16)},
	{"WADT",			BIT(18)},
	{},
};

static const struct pmc_reg_map spt_reg_map = {
	.pfear_sts = spt_pfear_map,
	.mphy_sts = spt_mphy_map,
	.pll_sts = spt_pll_map,
	.msr_sts = msr_map,
	.pm1_en_sts = spt_pm1_en_sts_map,
	.gpe0_sts = spt_gpe0_sts_map,
};

static const struct pci_device_id pmc_pci_ids[] = {
	{ PCI_VDEVICE(INTEL, SPT_PMC_PCI_DEVICE_ID),
					(kernel_ulong_t)&spt_reg_map },
	{ 0, },
};

static inline u8 pmc_core_reg_read_byte(struct pmc_dev *pmcdev, int offset)
{
	return readb(pmcdev->regbase + offset);
}

static inline u32 pmc_core_reg_read(struct pmc_dev *pmcdev, int reg_offset)
{
	return readl(pmcdev->regbase + reg_offset);
}

static inline void pmc_core_reg_write(struct pmc_dev *pmcdev, int
							reg_offset, u32 val)
{
	writel(val, pmcdev->regbase + reg_offset);
}

static inline u64 pmc_core_adjust_slp_s0_step(u32 value)
{
	return (u64)value * SPT_PMC_SLP_S0_RES_COUNTER_STEP;
}

int intel_pkgc10_counter_read(u64 *data)
{

	if (!data)
		return -EINVAL;

	return rdmsrl_safe(MSR_PKG_C10_RESIDENCY, data);

}
EXPORT_SYMBOL_GPL(intel_pkgc10_counter_read);

/**
 * intel_pmc_slp_s0_counter_read() - Read SLP_S0 residency.
 * @data: Out param that contains current SLP_S0 count.
 *
 * This API currently supports Intel Skylake SoC and Sunrise
 * Point Platform Controller Hub. Future platform support
 * should be added for platforms that support low power modes
 * beyond Package C10 state.
 *
 * SLP_S0_RESIDENCY counter counts in 100 us granularity per
 * step hence function populates the multiplied value in out
 * parameter @data.
 *
 * Return: an error code or 0 on success.
 */
int intel_pmc_slp_s0_counter_read(u32 *data)
{
	struct pmc_dev *pmcdev = &pmc;
	u32 value;

	if (!pmcdev->has_slp_s0_res)
		return -EACCES;

	value = pmc_core_reg_read(pmcdev, SPT_PMC_SLP_S0_RES_COUNTER_OFFSET);
	*data = pmc_core_adjust_slp_s0_step(value);

	return 0;
}
EXPORT_SYMBOL_GPL(intel_pmc_slp_s0_counter_read);

static int pmc_core_dev_state_get(void *data, u64 *val)
{
	struct pmc_dev *pmcdev = data;
	u32 value;

	value = pmc_core_reg_read(pmcdev, SPT_PMC_SLP_S0_RES_COUNTER_OFFSET);
	*val = pmc_core_adjust_slp_s0_step(value);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(pmc_core_dev_state, pmc_core_dev_state_get, NULL, "%llu\n");

static int pmc_core_check_read_lock_bit(void)
{
	struct pmc_dev *pmcdev = &pmc;
	u32 value;

	value = pmc_core_reg_read(pmcdev, SPT_PMC_PM_CFG_OFFSET);
	return value & BIT(SPT_PMC_READ_DISABLE_BIT);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void pmc_core_display_map(struct seq_file *s, int index,
				 u8 pf_reg, const struct pmc_bit_map *pf_map)
{
	seq_printf(s, "PCH IP: %-2d - %-32s\tState: %s\n",
		   index, pf_map[index].name,
		   pf_map[index].bit_mask & pf_reg ? "Off" : "On");
}

void pmc_core_ppfear_display(void)
{
	struct pmc_dev *pmcdev = &pmc;
	const struct pmc_bit_map *map = pmcdev->map->pfear_sts;
	u8 pf_regs[NUM_ENTRIES];
	int index, iter;

	iter = SPT_PMC_XRAM_PPFEAR0A;

	for (index = 0; index < NUM_ENTRIES; index++, iter++)
		pf_regs[index] = pmc_core_reg_read_byte(pmcdev, iter);

	for (index = 0; map[index].name; index++) {
		pr_warn("PCH IP: %-2d - %-32s\tState: %s\n",
		   index, map[index].name,
		   map[index].bit_mask & pf_regs[index / 8] ? "Off" : "On");
	}
}
EXPORT_SYMBOL_GPL(pmc_core_ppfear_display);

static int pmc_core_ppfear_sts_show(struct seq_file *s, void *unused)
{
	struct pmc_dev *pmcdev = s->private;
	const struct pmc_bit_map *map = pmcdev->map->pfear_sts;
	u8 pf_regs[NUM_ENTRIES];
	int index, iter;

	iter = SPT_PMC_XRAM_PPFEAR0A;

	for (index = 0; index < NUM_ENTRIES; index++, iter++)
		pf_regs[index] = pmc_core_reg_read_byte(pmcdev, iter);

	for (index = 0; map[index].name; index++)
		pmc_core_display_map(s, index, pf_regs[index / 8], map);

	return 0;
}

static int pmc_core_ppfear_sts_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmc_core_ppfear_sts_show, inode->i_private);
}

static const struct file_operations pmc_core_ppfear_ops = {
	.open           = pmc_core_ppfear_sts_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

/* This function should return link status, 0 means ready */
static int pmc_core_mtpmc_link_status(void)
{
	struct pmc_dev *pmcdev = &pmc;
	u32 value;

	value = pmc_core_reg_read(pmcdev, SPT_PMC_PM_STS_OFFSET);
	return value & BIT(SPT_PMC_MSG_FULL_STS_BIT);
}

static int pmc_core_send_msg(u32 *addr_xram)
{
	struct pmc_dev *pmcdev = &pmc;
	u32 dest;
	int timeout;

	for (timeout = NUM_RETRIES; timeout > 0; timeout--) {
		if (pmc_core_mtpmc_link_status() == 0)
			break;
		msleep(5);
	}

	if (timeout <= 0 && pmc_core_mtpmc_link_status())
		return -EBUSY;

	dest = (*addr_xram & MTPMC_MASK) | (1U << 1);
	pmc_core_reg_write(pmcdev, SPT_PMC_MTPMC_OFFSET, dest);
	return 0;
}

static int pmc_core_mphy_pg_sts_show(struct seq_file *s, void *unused)
{
	struct pmc_dev *pmcdev = s->private;
	const struct pmc_bit_map *map = pmcdev->map->mphy_sts;
	u32 mphy_core_reg_low, mphy_core_reg_high;
	u32 val_low, val_high;
	int index, err = 0;

	if (pmcdev->pmc_xram_read_bit) {
		seq_puts(s, "Access denied: please disable PMC_READ_DISABLE setting in BIOS.");
		return 0;
	}

	mphy_core_reg_low  = (SPT_PMC_MPHY_CORE_STS_0 << 16);
	mphy_core_reg_high = (SPT_PMC_MPHY_CORE_STS_1 << 16);

	mutex_lock(&pmcdev->lock);

	if (pmc_core_send_msg(&mphy_core_reg_low) != 0) {
		err = -EBUSY;
		goto out_unlock;
	}

	msleep(10);
	val_low = pmc_core_reg_read(pmcdev, SPT_PMC_MFPMC_OFFSET);

	if (pmc_core_send_msg(&mphy_core_reg_high) != 0) {
		err = -EBUSY;
		goto out_unlock;
	}

	msleep(10);
	val_high = pmc_core_reg_read(pmcdev, SPT_PMC_MFPMC_OFFSET);

	for (index = 0; map[index].name && index < 8; index++) {
		seq_printf(s, "%-32s\tState: %s\n",
			   map[index].name,
			   map[index].bit_mask & val_low ? "Not power gated" :
			   "Power gated");
	}

	for (index = 8; map[index].name; index++) {
		seq_printf(s, "%-32s\tState: %s\n",
			   map[index].name,
			   map[index].bit_mask & val_high ? "Not power gated" :
			   "Power gated");
	}

out_unlock:
	mutex_unlock(&pmcdev->lock);
	return err;
}

static int pmc_core_mphy_pg_sts_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmc_core_mphy_pg_sts_show, inode->i_private);
}

static const struct file_operations pmc_core_mphy_pg_ops = {
	.open           = pmc_core_mphy_pg_sts_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int pmc_core_pll_show(struct seq_file *s, void *unused)
{
	struct pmc_dev *pmcdev = s->private;
	const struct pmc_bit_map *map = pmcdev->map->pll_sts;
	u32 mphy_common_reg, val;
	int index, err = 0;

	if (pmcdev->pmc_xram_read_bit) {
		seq_puts(s, "Access denied: please disable PMC_READ_DISABLE setting in BIOS.");
		return 0;
	}

	mphy_common_reg  = (SPT_PMC_MPHY_COM_STS_0 << 16);
	mutex_lock(&pmcdev->lock);

	if (pmc_core_send_msg(&mphy_common_reg) != 0) {
		err = -EBUSY;
		goto out_unlock;
	}

	/* Observed PMC HW response latency for MTPMC-MFPMC is ~10 ms */
	msleep(10);
	val = pmc_core_reg_read(pmcdev, SPT_PMC_MFPMC_OFFSET);

	for (index = 0; map[index].name ; index++) {
		seq_printf(s, "%-32s\tState: %s\n",
			   map[index].name,
			   map[index].bit_mask & val ? "Active" : "Idle");
	}

out_unlock:
	mutex_unlock(&pmcdev->lock);
	return err;
}

static int pmc_core_pll_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmc_core_pll_show, inode->i_private);
}

static const struct file_operations pmc_core_pll_ops = {
	.open           = pmc_core_pll_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static ssize_t pmc_core_ltr_ignore_write(struct file *file, const char __user
*userbuf, size_t count, loff_t *ppos)
{
	struct pmc_dev *pmcdev = &pmc;
	u32 val, buf_size, fd;
	int err = 0;

	buf_size = count < 64 ? count : 64;
	mutex_lock(&pmcdev->lock);

	if (kstrtou32_from_user(userbuf, buf_size, 10, &val)) {
		err = -EFAULT;
		goto out_unlock;
	}

	if (val > NUM_IP_IGN_ALLOWED) {
		err = -EINVAL;
		goto out_unlock;
	}

	fd = pmc_core_reg_read(pmcdev, SPT_PMC_LTR_IGNORE_OFFSET);
	fd |= (1U << val);
	pmc_core_reg_write(pmcdev, SPT_PMC_LTR_IGNORE_OFFSET, fd);

out_unlock:
	mutex_unlock(&pmcdev->lock);
	return err == 0 ? count : err;
}

static int pmc_core_ltr_ignore_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int pmc_core_ltr_ignore_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmc_core_ltr_ignore_show, inode->i_private);
}

static const struct file_operations pmc_core_ltr_ignore_ops = {
	.open           = pmc_core_ltr_ignore_open,
	.read           = seq_read,
	.write          = pmc_core_ltr_ignore_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int pmc_core_pkgc_show(struct seq_file *s, void *unused)
{
	struct pmc_dev *pmcdev = s->private;
	const struct pmc_bit_map *map = pmcdev->map->msr_sts;
	u64 pcstate_count;
	int index, err;

	for (index = 0; map[index].name ; index++) {

		err = rdmsrl_safe(map[index].bit_mask, &pcstate_count);
		if (err) {
			pr_debug("Failed to read %s residency MSR",
							map[index].name);
			return err;
		}
		seq_printf(s, "%s\t : 0x%llx\n", map[index].name,
						pcstate_count);
	}

	return 0;
}

static int pmc_core_pkgc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmc_core_pkgc_show, inode->i_private);
}

static const struct file_operations pmc_core_pkgc_ops = {
	.open           = pmc_core_pkgc_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};


static void pmc_core_dbgfs_unregister(struct pmc_dev *pmcdev)
{
	debugfs_remove_recursive(pmcdev->dbgfs_dir);
}

static int pmc_core_wake_source_show(struct seq_file *s, void *unused)
{
	struct pmc_dev *pmcdev = s->private;

	/* Pass the wake source string */
	seq_printf(s, "%s\n", pmcdev->wake_source);
	return 0;
}

static int pmc_core_wake_source_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmc_core_wake_source_show, inode->i_private);
}

static const struct file_operations pmc_core_wake_source_ops = {
	.open           = pmc_core_wake_source_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int pmc_core_dbgfs_register(struct pmc_dev *pmcdev)
{
	struct dentry *dir, *file;

	dir = debugfs_create_dir("pmc_core", NULL);
	if (!dir)
		return -ENOMEM;

	pmcdev->dbgfs_dir = dir;
	file = debugfs_create_file("slp_s0_residency_usec", S_IFREG | S_IRUGO,
				   dir, pmcdev, &pmc_core_dev_state);
	if (!file)
		goto err;

	file = debugfs_create_file("pch_ip_power_gating_status",
				   S_IFREG | S_IRUGO, dir, pmcdev,
				   &pmc_core_ppfear_ops);
	if (!file)
		goto err;

	file = debugfs_create_file("mphy_core_lanes_power_gating_status",
				   S_IFREG | S_IRUGO, dir, pmcdev,
				   &pmc_core_mphy_pg_ops);
	if (!file)
		goto err;

	file = debugfs_create_file("pll_status",
				   S_IFREG | S_IRUGO, dir, pmcdev,
				   &pmc_core_pll_ops);
	if (!file)
		goto err;

	file = debugfs_create_file("ltr_ignore",
				   S_IFREG | S_IRUGO, dir, pmcdev,
				   &pmc_core_ltr_ignore_ops);

	if (!file)
		goto err;

	file = debugfs_create_file("package_cstate_residency",
				   S_IFREG | S_IRUGO, dir, pmcdev,
				   &pmc_core_pkgc_ops);

	if (!file)
		goto err;

	file = debugfs_create_file("last_wake_source",
				   S_IFREG | S_IRUGO, dir, pmcdev,
				   &pmc_core_wake_source_ops);

	if (!file)
		goto err;

	return 0;
err:
	pmc_core_dbgfs_unregister(pmcdev);
	return -ENODEV;
}
#else
static inline int pmc_core_dbgfs_register(struct pmc_dev *pmcdev)
{
	return 0;
}

static inline void pmc_core_dbgfs_unregister(struct pmc_dev *pmcdev)
{
}
#endif /* CONFIG_DEBUG_FS */

static const struct x86_cpu_id intel_pmc_core_ids[] = {
	{ X86_VENDOR_INTEL, 6, INTEL_FAM6_SKYLAKE_MOBILE, X86_FEATURE_MWAIT,
		(kernel_ulong_t)NULL},
	{ X86_VENDOR_INTEL, 6, INTEL_FAM6_SKYLAKE_DESKTOP, X86_FEATURE_MWAIT,
		(kernel_ulong_t)NULL},
	{ X86_VENDOR_INTEL, 6, INTEL_FAM6_KABYLAKE_MOBILE, X86_FEATURE_MWAIT,
		(kernel_ulong_t)NULL},
	{ X86_VENDOR_INTEL, 6, INTEL_FAM6_KABYLAKE_DESKTOP, X86_FEATURE_MWAIT,
		(kernel_ulong_t)NULL},
	{}
};

static int pmc_core_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct device *ptr_dev = &dev->dev;
	struct pmc_dev *pmcdev = &pmc;
	const struct x86_cpu_id *cpu_id;
	const struct pmc_reg_map *map = (struct pmc_reg_map *)id->driver_data;
	int err;

	cpu_id = x86_match_cpu(intel_pmc_core_ids);
	if (!cpu_id) {
		dev_dbg(&dev->dev, "PMC Core: cpuid mismatch.\n");
		return -EINVAL;
	}

	err = pcim_enable_device(dev);
	if (err < 0) {
		dev_dbg(&dev->dev, "PMC Core: failed to enable Power Management Controller.\n");
		return err;
	}

	err = pci_read_config_dword(dev,
				    SPT_PMC_BASE_ADDR_OFFSET,
				    &pmcdev->base_addr);
	if (err < 0) {
		dev_dbg(&dev->dev, "PMC Core: failed to read PCI config space.\n");
		return err;
	}
	pmcdev->base_addr &= PMC_BASE_ADDR_MASK;
	dev_dbg(&dev->dev, "PMC Core: PWRMBASE is %#x\n", pmcdev->base_addr);

	pmcdev->regbase = devm_ioremap_nocache(ptr_dev,
					      pmcdev->base_addr,
					      SPT_PMC_MMIO_REG_LEN);
	if (!pmcdev->regbase) {
		dev_dbg(&dev->dev, "PMC Core: ioremap failed.\n");
		return -ENOMEM;
	}

	err = pci_read_config_dword(dev,
				    SPT_PMC_IO_ADDR_OFFSET,
				    &pmcdev->io_addr);
	if (err) {
		dev_dbg(&dev->dev, "PMC Core: failed to read PCI config space.\n");
		return pcibios_err_to_errno(err);
	}
	pmcdev->io_addr &= PMC_IO_ADDR_MASK;
	dev_dbg(&dev->dev, "PMC Core: PMC IO BASE is %#x\n", pmcdev->io_addr);

	pmcdev->iobase = devm_ioport_map(ptr_dev,
					      pmcdev->io_addr,
					      SPT_PMC_IO_REG_LEN);
	if (!pmcdev->iobase) {
		dev_dbg(&dev->dev, "PMC Core: ioport remap failed.\n");
		return -ENOMEM;
	}

	mutex_init(&pmcdev->lock);
	pmcdev->pmc_xram_read_bit = pmc_core_check_read_lock_bit();
	pmcdev->map = map;

	err = pmc_core_dbgfs_register(pmcdev);
	if (err < 0)
		dev_warn(&dev->dev, "PMC Core: debugfs register failed.\n");

	pmc.has_slp_s0_res = true;

	return 0;
}

static void pmc_decode_wake_source(struct device *dev)
{
	struct pmc_dev *pmcdev = &pmc;
	int index, reg_index, bit_index;
	char *s = pmcdev->wake_source;
	size_t rc, size = sizeof(pmcdev->wake_source) - 1;
	const struct pmc_bit_map *map1 = pmcdev->map->pm1_en_sts;
	const struct pmc_bit_map *map2 = pmcdev->map->gpe0_sts;

	for (index = 0; map1[index].name; index++) {
		if (map1[index].bit_mask & pmc.pm1_en_sts_val) {
			rc = snprintf(s, size, "%s ", map1[index].name);
			s += rc;
			size -= rc;
		}
	}

	/* Generic GPE0_STS[95:0] wake mapping */
	for (reg_index = 0;
	     reg_index < SPT_PMC_GPE0_127_96_REG_INDEX &&
				pmc.gpe0_sts_val[reg_index];
	     reg_index++) {
		unsigned long data = pmc.gpe0_sts_val[reg_index];

		for_each_set_bit(bit_index, &data, 32) {
			rc = snprintf(s,
				      size,
				      "GPE0[%d] ",
				      (reg_index * 32 + bit_index));
			s += rc;
			size -= rc;
		}
	}

	/* GPE0_STS_127_96 has specific mapping */
	for (index = 0; map2[index].name; index++) {
		if (map2[index].bit_mask &
		    pmc.gpe0_sts_val[SPT_PMC_GPE0_127_96_REG_INDEX]) {
			rc = snprintf(s, size, "%s ", map2[index].name);
			s += rc;
			size -= rc;
		}
	}

	/* Say "Unknown" if no wake_source is identified */
	if (size == sizeof(pmcdev->wake_source) - 1)
		snprintf(s, size, "Unknown");

	/* Log the wake source */
	dev_info(dev, "last_wake_source: %s\n", pmc.wake_source);
}

static void pmc_read_wake_source(void)
{
	int i;
	struct pmc_dev *pmcdev = &pmc;
	u32 gpe0_en[SPT_PMC_GPE0_REG_LEN];

	/*
	 * Qualify PM1_EN_STS wake status bits with enable bits
	 * PM1_EN_STS[15:0] contains wake status bits
	 * PM1_EN_STS[31:16] contains corresponding enable bits
	 *
	 * Exception: Power button cannot be disabled, check PWRBTN_STS
	 * irrespective of the enable bit.
	 */
	pmc.pm1_en_sts_val = ioread32(pmcdev->iobase +
					SPT_PMC_PM1_EN_STS_OFFSET);
	pmc.pm1_en_sts_val &= (pmc.pm1_en_sts_val >> 16) |
					SPT_PMC_BIT_PWRBTN_STS;

	/*
	 * Qualify GPE0_STS wake status bits with corresponding GPE0_EN
	 * enable bits
	 *
	 * Exception: SMB_WAK (bit7) is always enabled for wake
	 */
	for (i = 0; i < SPT_PMC_GPE0_REG_LEN; i++)
		gpe0_en[i] = ioread32(pmcdev->iobase + SPT_PMC_GPE0_EN_OFFSET +
					4 * i);
	gpe0_en[SPT_PMC_GPE0_127_96_REG_INDEX] |= SPT_PMC_BIT_SMB_WAKE_STS;

	for (i = 0; i < SPT_PMC_GPE0_REG_LEN; i++) {
		pmc.gpe0_sts_val[i] = ioread32(pmcdev->iobase +
					SPT_PMC_GPE0_STS_OFFSET + 4 * i);
		pmc.gpe0_sts_val[i] &= gpe0_en[i];
	}
}

#ifdef CONFIG_PM_SLEEP
static int pmc_suspend_noirq(struct device *dev)
{
	/* Do nothing */
	return 0;
}

static int pmc_resume_noirq(struct device *dev)
{
	/*
	 * Do nothing, but read the PM STS & GPE STS registers, decode
	 * the wake source, and report via PMC debugfs
	 */
	pmc_read_wake_source();
	pmc_decode_wake_source(dev);
	return 0;
}

static const struct dev_pm_ops pmc_dev_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(pmc_suspend_noirq, pmc_resume_noirq)
};

#define PMC_DEV_PM_OPS		(&pmc_dev_pm_ops)
#else
#define PMC_DEV_PM_OPS		NULL
#endif /* CONFIG_PM_SLEEP */

static struct pci_driver intel_pmc_core_driver = {
	.name = "intel_pmc_core",
	.id_table = pmc_pci_ids,
	.probe = pmc_core_probe,
	.driver = {
		.pm = PMC_DEV_PM_OPS,
	},
};

builtin_pci_driver(intel_pmc_core_driver);
