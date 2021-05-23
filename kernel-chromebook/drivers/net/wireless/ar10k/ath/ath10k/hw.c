/*
 * Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/types.h>
#include <linux/bitfield.h>
#include "core.h"
#include "hw.h"
#include "bmi.h"
#include "hif.h"

const struct ath10k_hw_regs qca988x_regs = {
	.rtc_state_cold_reset_mask	= 0x00000400,
	.rtc_soc_base_address		= 0x00004000,
	.rtc_wmac_base_address		= 0x00005000,
	.soc_core_base_address		= 0x00009000,
	.ce_wrapper_base_address	= 0x00057000,
	.ce0_base_address		= 0x00057400,
	.ce1_base_address		= 0x00057800,
	.ce2_base_address		= 0x00057c00,
	.ce3_base_address		= 0x00058000,
	.ce4_base_address		= 0x00058400,
	.ce5_base_address		= 0x00058800,
	.ce6_base_address		= 0x00058c00,
	.ce7_base_address		= 0x00059000,
	.soc_reset_control_si0_rst_mask	= 0x00000001,
	.soc_reset_control_ce_rst_mask	= 0x00040000,
	.soc_chip_id_address		= 0x000000ec,
	.scratch_3_address		= 0x00000030,
	.fw_indicator_address		= 0x00009030,
	.pcie_local_base_address	= 0x00080000,
	.ce_wrap_intr_sum_host_msi_lsb	= 0x00000008,
	.ce_wrap_intr_sum_host_msi_mask	= 0x0000ff00,
	.pcie_intr_fw_mask		= 0x00000400,
	.pcie_intr_ce_mask_all		= 0x0007f800,
	.pcie_intr_clr_address		= 0x00000014,
};

const struct ath10k_hw_regs qca6174_regs = {
	.rtc_state_cold_reset_mask		= 0x00002000,
	.rtc_soc_base_address			= 0x00000800,
	.rtc_wmac_base_address			= 0x00001000,
	.soc_core_base_address			= 0x0003a000,
	.ce_wrapper_base_address		= 0x00034000,
	.ce0_base_address			= 0x00034400,
	.ce1_base_address			= 0x00034800,
	.ce2_base_address			= 0x00034c00,
	.ce3_base_address			= 0x00035000,
	.ce4_base_address			= 0x00035400,
	.ce5_base_address			= 0x00035800,
	.ce6_base_address			= 0x00035c00,
	.ce7_base_address			= 0x00036000,
	.soc_reset_control_si0_rst_mask		= 0x00000000,
	.soc_reset_control_ce_rst_mask		= 0x00000001,
	.soc_chip_id_address			= 0x000000f0,
	.scratch_3_address			= 0x00000028,
	.fw_indicator_address			= 0x0003a028,
	.pcie_local_base_address		= 0x00080000,
	.ce_wrap_intr_sum_host_msi_lsb		= 0x00000008,
	.ce_wrap_intr_sum_host_msi_mask		= 0x0000ff00,
	.pcie_intr_fw_mask			= 0x00000400,
	.pcie_intr_ce_mask_all			= 0x0007f800,
	.pcie_intr_clr_address			= 0x00000014,
	.cpu_pll_init_address			= 0x00404020,
	.cpu_speed_address			= 0x00404024,
	.core_clk_div_address			= 0x00404028,
};

const struct ath10k_hw_regs qca99x0_regs = {
	.rtc_state_cold_reset_mask		= 0x00000400,
	.rtc_soc_base_address			= 0x00080000,
	.rtc_wmac_base_address			= 0x00000000,
	.soc_core_base_address			= 0x00082000,
	.ce_wrapper_base_address		= 0x0004d000,
	.ce0_base_address			= 0x0004a000,
	.ce1_base_address			= 0x0004a400,
	.ce2_base_address			= 0x0004a800,
	.ce3_base_address			= 0x0004ac00,
	.ce4_base_address			= 0x0004b000,
	.ce5_base_address			= 0x0004b400,
	.ce6_base_address			= 0x0004b800,
	.ce7_base_address			= 0x0004bc00,
	/* Note: qca99x0 supports upto 12 Copy Engines. Other than address of
	 * CE0 and CE1 no other copy engine is directly referred in the code.
	 * It is not really neccessary to assign address for newly supported
	 * CEs in this address table.
	 *	Copy Engine		Address
	 *	CE8			0x0004c000
	 *	CE9			0x0004c400
	 *	CE10			0x0004c800
	 *	CE11			0x0004cc00
	 */
	.soc_reset_control_si0_rst_mask		= 0x00000001,
	.soc_reset_control_ce_rst_mask		= 0x00000100,
	.soc_chip_id_address			= 0x000000ec,
	.scratch_3_address			= 0x00040050,
	.fw_indicator_address			= 0x00040050,
	.pcie_local_base_address		= 0x00000000,
	.ce_wrap_intr_sum_host_msi_lsb		= 0x0000000c,
	.ce_wrap_intr_sum_host_msi_mask		= 0x00fff000,
	.pcie_intr_fw_mask			= 0x00100000,
	.pcie_intr_ce_mask_all			= 0x000fff00,
	.pcie_intr_clr_address			= 0x00000010,
};

const struct ath10k_hw_regs qca4019_regs = {
	.rtc_soc_base_address                   = 0x00080000,
	.soc_core_base_address                  = 0x00082000,
	.ce_wrapper_base_address                = 0x0004d000,
	.ce0_base_address                       = 0x0004a000,
	.ce1_base_address                       = 0x0004a400,
	.ce2_base_address                       = 0x0004a800,
	.ce3_base_address                       = 0x0004ac00,
	.ce4_base_address                       = 0x0004b000,
	.ce5_base_address                       = 0x0004b400,
	.ce6_base_address                       = 0x0004b800,
	.ce7_base_address                       = 0x0004bc00,
	/* qca4019 supports upto 12 copy engines. Since base address
	 * of ce8 to ce11 are not directly referred in the code,
	 * no need have them in separate members in this table.
	 *      Copy Engine             Address
	 *      CE8                     0x0004c000
	 *      CE9                     0x0004c400
	 *      CE10                    0x0004c800
	 *      CE11                    0x0004cc00
	 */
	.soc_reset_control_si0_rst_mask         = 0x00000001,
	.soc_reset_control_ce_rst_mask          = 0x00000100,
	.soc_chip_id_address                    = 0x000000ec,
	.fw_indicator_address                   = 0x0004f00c,
	.ce_wrap_intr_sum_host_msi_lsb          = 0x0000000c,
	.ce_wrap_intr_sum_host_msi_mask         = 0x00fff000,
	.pcie_intr_fw_mask                      = 0x00100000,
	.pcie_intr_ce_mask_all                  = 0x000fff00,
	.pcie_intr_clr_address                  = 0x00000010,
};

const struct ath10k_hw_values qca988x_values = {
	.rtc_state_val_on		= 3,
	.ce_count			= 8,
	.msi_assign_ce_max		= 7,
	.num_target_ce_config_wlan	= 7,
	.ce_desc_meta_data_mask		= 0xFFFC,
	.ce_desc_meta_data_lsb		= 2,
};

const struct ath10k_hw_values qca6174_values = {
	.rtc_state_val_on		= 3,
	.ce_count			= 8,
	.msi_assign_ce_max		= 7,
	.num_target_ce_config_wlan	= 7,
	.ce_desc_meta_data_mask		= 0xFFFC,
	.ce_desc_meta_data_lsb		= 2,
};

const struct ath10k_hw_values qca99x0_values = {
	.rtc_state_val_on		= 5,
	.ce_count			= 12,
	.msi_assign_ce_max		= 12,
	.num_target_ce_config_wlan	= 10,
	.ce_desc_meta_data_mask		= 0xFFF0,
	.ce_desc_meta_data_lsb		= 4,
};

const struct ath10k_hw_values qca4019_values = {
	.ce_count                       = 12,
	.num_target_ce_config_wlan      = 10,
	.ce_desc_meta_data_mask         = 0xFFF0,
	.ce_desc_meta_data_lsb          = 4,
};

const struct ath10k_hw_clk_params qca6174_clk[ATH10K_HW_REFCLK_COUNT] = {
	{
		.refclk = 48000000,
		.div = 0xe,
		.rnfrac = 0x2aaa8,
		.settle_time = 2400,
		.refdiv = 0,
		.outdiv = 1,
	},
	{
		.refclk = 19200000,
		.div = 0x24,
		.rnfrac = 0x2aaa8,
		.settle_time = 960,
		.refdiv = 0,
		.outdiv = 1,
	},
	{
		.refclk = 24000000,
		.div = 0x1d,
		.rnfrac = 0x15551,
		.settle_time = 1200,
		.refdiv = 0,
		.outdiv = 1,
	},
	{
		.refclk = 26000000,
		.div = 0x1b,
		.rnfrac = 0x4ec4,
		.settle_time = 1300,
		.refdiv = 0,
		.outdiv = 1,
	},
	{
		.refclk = 37400000,
		.div = 0x12,
		.rnfrac = 0x34b49,
		.settle_time = 1870,
		.refdiv = 0,
		.outdiv = 1,
	},
	{
		.refclk = 38400000,
		.div = 0x12,
		.rnfrac = 0x15551,
		.settle_time = 1920,
		.refdiv = 0,
		.outdiv = 1,
	},
	{
		.refclk = 40000000,
		.div = 0x12,
		.rnfrac = 0x26665,
		.settle_time = 2000,
		.refdiv = 0,
		.outdiv = 1,
	},
	{
		.refclk = 52000000,
		.div = 0x1b,
		.rnfrac = 0x4ec4,
		.settle_time = 2600,
		.refdiv = 0,
		.outdiv = 1,
	},
};

void ath10k_hw_fill_survey_time(struct ath10k *ar, struct survey_info *survey,
				u32 cc, u32 rcc, u32 cc_prev, u32 rcc_prev)
{
	u32 cc_fix = 0;

	survey->filled |= SURVEY_INFO_TIME |
			  SURVEY_INFO_TIME_BUSY;

	if (ar->hw_params.has_shifted_cc_wraparound && cc < cc_prev) {
		cc_fix = 0x7fffffff;
		survey->filled &= ~SURVEY_INFO_TIME_BUSY;
	}

	cc -= cc_prev - cc_fix;
	rcc -= rcc_prev;

	survey->time = CCNT_TO_MSEC(ar, cc);
	survey->time_busy = CCNT_TO_MSEC(ar, rcc);
}

/* Program CPU_ADDR_MSB to allow different memory
 * region access.
 */
static void ath10k_hw_map_target_mem(struct ath10k *ar, u32 msb)
{
	u32 address = SOC_CORE_BASE_ADDRESS + FW_RAM_CONFIG_ADDRESS;

	ath10k_hif_write32(ar, address, msb);
}

/* 1. Write to memory region of target, such as IRAM adn DRAM.
 * 2. Target address( 0 ~ 00100000 & 0x00400000~0x00500000)
 *    can be written directly. See ath10k_pci_targ_cpu_to_ce_addr() too.
 * 3. In order to access the region other than the above,
 *    we need to set the value of register CPU_ADDR_MSB.
 * 4. Target memory access space is limited to 1M size. If the size is larger
 *    than 1M, need to split it and program CPU_ADDR_MSB accordingly.
 */
static int ath10k_hw_diag_segment_msb_download(struct ath10k *ar,
					       const void *buffer,
					       u32 address,
					       u32 length)
{
	u32 addr = address & REGION_ACCESS_SIZE_MASK;
	int ret, remain_size, size;
	const u8 *buf;

	ath10k_hw_map_target_mem(ar, CPU_ADDR_MSB_REGION_VAL(address));

	if (addr + length > REGION_ACCESS_SIZE_LIMIT) {
		size = REGION_ACCESS_SIZE_LIMIT - addr;
		remain_size = length - size;

		ret = ath10k_hif_diag_write(ar, address, buffer, size);
		if (ret) {
			ath10k_warn(ar,
				    "failed to download the first %d bytes segment to address:0x%x: %d\n",
				    size, address, ret);
			goto done;
		}

		/* Change msb to the next memory region*/
		ath10k_hw_map_target_mem(ar,
					 CPU_ADDR_MSB_REGION_VAL(address) + 1);
		buf = buffer +  size;
		ret = ath10k_hif_diag_write(ar,
					    address & ~REGION_ACCESS_SIZE_MASK,
					    buf, remain_size);
		if (ret) {
			ath10k_warn(ar,
				    "failed to download the second %d bytes segment to address:0x%x: %d\n",
				    remain_size,
				    address & ~REGION_ACCESS_SIZE_MASK,
				    ret);
			goto done;
		}
	} else {
		ret = ath10k_hif_diag_write(ar, address, buffer, length);
		if (ret) {
			ath10k_warn(ar,
				    "failed to download the only %d bytes segment to address:0x%x: %d\n",
				    length, address, ret);
			goto done;
		}
	}

done:
	/* Change msb to DRAM */
	ath10k_hw_map_target_mem(ar,
				 CPU_ADDR_MSB_REGION_VAL(DRAM_BASE_ADDRESS));
	return ret;
}

static int ath10k_hw_diag_segment_download(struct ath10k *ar,
					   const void *buffer,
					   u32 address,
					   u32 length)
{
	if (address >= DRAM_BASE_ADDRESS + REGION_ACCESS_SIZE_LIMIT)
		/* Needs to change MSB for memory write */
		return ath10k_hw_diag_segment_msb_download(ar, buffer,
							   address, length);
	else
		return ath10k_hif_diag_write(ar, address, buffer, length);
}

int ath10k_hw_diag_fast_download(struct ath10k *ar,
				 u32 address,
				 const void *buffer,
				 u32 length)
{
	const u8 *buf = buffer;
	bool sgmt_end = false;
	u32 base_addr = 0;
	u32 base_len = 0;
	u32 left = 0;
	struct bmi_segmented_file_header *hdr;
	struct bmi_segmented_metadata *metadata;
	int ret = 0;

	if (length < sizeof(*hdr))
		return -EINVAL;

	/* check firmware header. If it has no correct magic number
	 * or it's compressed, returns error.
	 */
	hdr = (struct bmi_segmented_file_header *)buf;
	if (__le32_to_cpu(hdr->magic_num) != BMI_SGMTFILE_MAGIC_NUM) {
		ath10k_dbg(ar, ATH10K_DBG_BOOT,
			   "Not a supported firmware, magic_num:0x%x\n",
			   hdr->magic_num);
		return -EINVAL;
	}

	if (hdr->file_flags != 0) {
		ath10k_dbg(ar, ATH10K_DBG_BOOT,
			   "Not a supported firmware, file_flags:0x%x\n",
			   hdr->file_flags);
		return -EINVAL;
	}

	metadata = (struct bmi_segmented_metadata *)hdr->data;
	left = length - sizeof(*hdr);

	while (left > 0) {
		if (left < sizeof(*metadata)) {
			ath10k_warn(ar, "firmware segment is truncated: %d\n",
				    left);
			ret = -EINVAL;
			break;
		}
		base_addr = __le32_to_cpu(metadata->addr);
		base_len = __le32_to_cpu(metadata->length);
		buf = metadata->data;
		left -= sizeof(*metadata);

		switch (base_len) {
		case BMI_SGMTFILE_BEGINADDR:
			/* base_addr is the start address to run */
			ret = ath10k_bmi_set_start(ar, base_addr);
			base_len = 0;
			break;
		case BMI_SGMTFILE_DONE:
			/* no more segment */
			base_len = 0;
			sgmt_end = true;
			ret = 0;
			break;
		case BMI_SGMTFILE_BDDATA:
		case BMI_SGMTFILE_EXEC:
			ath10k_warn(ar,
				    "firmware has unsupported segment:%d\n",
				    base_len);
			ret = -EINVAL;
			break;
		default:
			if (base_len > left) {
				/* sanity check */
				ath10k_warn(ar,
					    "firmware has invalid segment length, %d > %d\n",
					    base_len, left);
				ret = -EINVAL;
				break;
			}

			ret = ath10k_hw_diag_segment_download(ar,
							      buf,
							      base_addr,
							      base_len);

			if (ret)
				ath10k_warn(ar,
					    "failed to download firmware via diag interface:%d\n",
					    ret);
			break;
		}

		if (ret || sgmt_end)
			break;

		metadata = (struct bmi_segmented_metadata *)(buf + base_len);
		left -= base_len;
	}

	if (ret == 0)
		ath10k_dbg(ar, ATH10K_DBG_BOOT,
			   "boot firmware fast diag download successfully.\n");
	return ret;
}

const struct ath10k_hw_ops qca988x_ops = {
};

static int ath10k_qca99x0_rx_desc_get_l3_pad_bytes(struct htt_rx_desc *rxd)
{
	return MS(__le32_to_cpu(rxd->msdu_end.qca99x0.info1),
		  RX_MSDU_END_INFO1_L3_HDR_PAD);
}

const struct ath10k_hw_ops qca99x0_ops = {
	.rx_desc_get_l3_pad_bytes = ath10k_qca99x0_rx_desc_get_l3_pad_bytes,
};

/**
 * ath10k_hw_qca6174_enable_pll_clock() - enable the qca6174 hw pll clock
 * @ar: the ath10k blob
 *
 * This function is very hardware specific, the clock initialization
 * steps is very sensitive and could lead to unknown crash, so they
 * should be done in sequence.
 *
 * *** Be aware if you planned to refactor them. ***
 *
 * Return: 0 if successfully enable the pll, otherwise EINVAL
 */
static int ath10k_hw_qca6174_enable_pll_clock(struct ath10k *ar)
{
	int ret, wait_limit;
	u32 clk_div_addr, pll_init_addr, speed_addr;
	u32 addr, reg_val, mem_val;
	struct ath10k_hw_params *hw;
	const struct ath10k_hw_clk_params *hw_clk;

	hw = &ar->hw_params;

	if (ar->regs->core_clk_div_address == 0 ||
	    ar->regs->cpu_pll_init_address == 0 ||
	    ar->regs->cpu_speed_address == 0)
		return -EINVAL;

	clk_div_addr = ar->regs->core_clk_div_address;
	pll_init_addr = ar->regs->cpu_pll_init_address;
	speed_addr = ar->regs->cpu_speed_address;

	/* Read efuse register to find out the right hw clock configuration */
	addr = (RTC_SOC_BASE_ADDRESS | EFUSE_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	/* sanitize if the hw refclk index is out of the boundary */
	if (MS(reg_val, EFUSE_XTAL_SEL) > ATH10K_HW_REFCLK_COUNT)
		return -EINVAL;

	hw_clk = &hw->hw_clk[MS(reg_val, EFUSE_XTAL_SEL)];

	/* Set the rnfrac and outdiv params to bb_pll register */
	addr = (RTC_SOC_BASE_ADDRESS | BB_PLL_CONFIG_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	reg_val &= ~(BB_PLL_CONFIG_FRAC_MASK | BB_PLL_CONFIG_OUTDIV_MASK);
	reg_val |= (SM(hw_clk->rnfrac, BB_PLL_CONFIG_FRAC) |
		    SM(hw_clk->outdiv, BB_PLL_CONFIG_OUTDIV));
	ret = ath10k_bmi_write_soc_reg(ar, addr, reg_val);
	if (ret)
		return -EINVAL;

	/* Set the correct settle time value to pll_settle register */
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_SETTLE_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	reg_val &= ~WLAN_PLL_SETTLE_TIME_MASK;
	reg_val |= SM(hw_clk->settle_time, WLAN_PLL_SETTLE_TIME);
	ret = ath10k_bmi_write_soc_reg(ar, addr, reg_val);
	if (ret)
		return -EINVAL;

	/* Set the clock_ctrl div to core_clk_ctrl register */
	addr = (RTC_SOC_BASE_ADDRESS | SOC_CORE_CLK_CTRL_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	reg_val &= ~SOC_CORE_CLK_CTRL_DIV_MASK;
	reg_val |= SM(1, SOC_CORE_CLK_CTRL_DIV);
	ret = ath10k_bmi_write_soc_reg(ar, addr, reg_val);
	if (ret)
		return -EINVAL;

	/* Set the clock_div register */
	mem_val = 1;
	ret = ath10k_bmi_write_memory(ar, clk_div_addr, &mem_val,
				      sizeof(mem_val));
	if (ret)
		return -EINVAL;

	/* Configure the pll_control register */
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	reg_val |= (SM(hw_clk->refdiv, WLAN_PLL_CONTROL_REFDIV) |
		    SM(hw_clk->div, WLAN_PLL_CONTROL_DIV) |
		    SM(1, WLAN_PLL_CONTROL_NOPWD));
	ret = ath10k_bmi_write_soc_reg(ar, addr, reg_val);
	if (ret)
		return -EINVAL;

	/* busy wait (max 1s) the rtc_sync status register indicate ready */
	wait_limit = 100000;
	addr = (RTC_WMAC_BASE_ADDRESS | RTC_SYNC_STATUS_OFFSET);
	do {
		ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
		if (ret)
			return -EINVAL;

		if (!MS(reg_val, RTC_SYNC_STATUS_PLL_CHANGING))
			break;

		wait_limit--;
		udelay(10);

	} while (wait_limit > 0);

	if (MS(reg_val, RTC_SYNC_STATUS_PLL_CHANGING))
		return -EINVAL;

	/* Unset the pll_bypass in pll_control register */
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	reg_val &= ~WLAN_PLL_CONTROL_BYPASS_MASK;
	reg_val |= SM(0, WLAN_PLL_CONTROL_BYPASS);
	ret = ath10k_bmi_write_soc_reg(ar, addr, reg_val);
	if (ret)
		return -EINVAL;

	/* busy wait (max 1s) the rtc_sync status register indicate ready */
	wait_limit = 100000;
	addr = (RTC_WMAC_BASE_ADDRESS | RTC_SYNC_STATUS_OFFSET);
	do {
		ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
		if (ret)
			return -EINVAL;

		if (!MS(reg_val, RTC_SYNC_STATUS_PLL_CHANGING))
			break;

		wait_limit--;
		udelay(10);

	} while (wait_limit > 0);

	if (MS(reg_val, RTC_SYNC_STATUS_PLL_CHANGING))
		return -EINVAL;

	/* Enable the hardware cpu clock register */
	addr = (RTC_SOC_BASE_ADDRESS | SOC_CPU_CLOCK_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	reg_val &= ~SOC_CPU_CLOCK_STANDARD_MASK;
	reg_val |= SM(1, SOC_CPU_CLOCK_STANDARD);
	ret = ath10k_bmi_write_soc_reg(ar, addr, reg_val);
	if (ret)
		return -EINVAL;

	/* unset the nopwd from pll_control register */
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	ret = ath10k_bmi_read_soc_reg(ar, addr, &reg_val);
	if (ret)
		return -EINVAL;

	reg_val &= ~WLAN_PLL_CONTROL_NOPWD_MASK;
	ret = ath10k_bmi_write_soc_reg(ar, addr, reg_val);
	if (ret)
		return -EINVAL;

	/* enable the pll_init register */
	mem_val = 1;
	ret = ath10k_bmi_write_memory(ar, pll_init_addr, &mem_val,
				      sizeof(mem_val));
	if (ret)
		return -EINVAL;

	/* set the target clock frequency to speed register */
	ret = ath10k_bmi_write_memory(ar, speed_addr, &hw->target_cpu_freq,
				      sizeof(hw->target_cpu_freq));
	if (ret)
		return -EINVAL;

	return 0;
}

const struct ath10k_hw_ops qca6174_ops = {
	.enable_pll_clk = ath10k_hw_qca6174_enable_pll_clock,
};
