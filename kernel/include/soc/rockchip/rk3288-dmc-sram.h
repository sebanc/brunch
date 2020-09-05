/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#ifndef __RK3288_DMC_SRAM_H
#define __RK3288_DMC_SRAM_H

#include <linux/clk-provider.h>

enum dram_type_tag {
	LPDDR = 0,
	DDR,
	DDR2,
	DDR3,
	LPDDR2,
	LPDDR3,

	DRAM_MAX
};

#define NUM_MC_CHANNEL_MAX 2

struct ctl_timing_tag {
	u32 ddr_freq;
	/* Memory Timing Registers */
	u32 togcnt1u;		/* Toggle Counter 1U Register */
	u32 tinit;		/* t_init Timing Register */
	u32 trsth;		/* Reset High Time Register */
	u32 togcnt100n;		/* Toggle Counter 100N Register */
	u32 trefi;		/* t_refi Timing Register */
	u32 tmrd;		/* t_mrd Timing Register */
	u32 trfc;		/* t_rfc Timing Register */
	u32 trp;		/* t_rp Timing Register */
	u32 trtw;		/* t_rtw Timing Register */
	u32 tal;		/* AL Latency Register */
	u32 tcl;		/* CL Timing Register */
	u32 tcwl;		/* CWL Register */
	u32 tras;		/* t_ras Timing Register */
	u32 trc;		/* t_rc Timing Register */
	u32 trcd;		/* t_rcd Timing Register */
	u32 trrd;		/* t_rrd Timing Register */
	u32 trtp;		/* t_rtp Timing Register */
	u32 twr;		/* t_wr Timing Register */
	u32 twtr;		/* t_wtr Timing Register */
	u32 texsr;		/* t_exsr Timing Register */
	u32 txp;		/* t_xp Timing Register */
	u32 txpdll;		/* t_xpdll Timing Register */
	u32 tzqcs;		/* t_zqcs Timing Register */
	u32 tzqcsi;		/* t_zqcsi Timing Register */
	u32 tdqs;		/* t_dqs Timing Register */
	u32 tcksre;		/* t_cksre Timing Register */
	u32 tcksrx;		/* t_cksrx Timing Register */
	u32 tcke;		/* t_cke Timing Register */
	u32 tmod;		/* t_mod Timing Register */
	u32 trstl;		/* Reset Low Timing Register */
	u32 tzqcl;		/* t_zqcl Timing Register */
	u32 tmrr;		/* t_mrr Timing Register */
	u32 tckesr;		/* t_ckesr Timing Register */
	u32 tdpd;		/* t_dpd Timing Register */
};

union dtpr_0_tag {
	u32 d32;
	struct {
		unsigned tmrd:2;
		unsigned trtp:3;
		unsigned twtr:3;
		unsigned trp:4;
		unsigned trcd:4;
		unsigned tras:5;
		unsigned trrd:4;
		unsigned trc:6;
		unsigned tccd:1;
	} b;
};

union dtpr_1_tag {
	u32 d32;
	struct {
		unsigned taond:2;
		unsigned trtw:1;
		unsigned tfaw:6;
		unsigned tmod:2;
		unsigned trtodt:1;
		unsigned reserved12_15:4;
		unsigned trfc:8;
		unsigned tdqsck:3;
		unsigned tdqsck_max:3;
		unsigned reserved30_31:2;
	} b;
};

union dtpr_2_tag {
	u32 d32;
	struct {
		unsigned txs:10;
		unsigned txp:5;
		unsigned tcke:4;
		unsigned tdllk:10;
		unsigned reserved29_31:3;
	} b;
};

struct phy_timing_tag {
	union dtpr_0_tag dtpr0;
	union dtpr_1_tag dtpr1;
	union dtpr_2_tag dtpr2;
	u32 mr[4];		/* LPDDR2 no MR0, mr[2] is mDDR MR1 */
	u32 mr11;		/* for LPDDR3 only */
};

union noc_timing_tag {
	u32 d32;
	struct {
		unsigned act_to_act:6;
		unsigned rd_to_miss:6;
		unsigned wr_to_miss:6;
		unsigned burst_len:3;
		unsigned rd_to_wr:5;
		unsigned wr_to_rd:5;
		unsigned bw_ratio:1;
	} b;
};

union noc_activate_tag {
	u32 d32;
	struct {
		unsigned rrd:4;		/* bit[0:3] */
		unsigned faw:6;		/* bit[4:9] */
		unsigned faw_bank:1;	/* bit 10 */
		unsigned reserved:21;
	} b;
};

struct dmc_regtiming {
	struct ctl_timing_tag ctl_timing;
	struct phy_timing_tag phy_timing;
	union noc_timing_tag noc_timing;
	union noc_activate_tag noc_activate;
};

struct dmc_oftiming {
	u32 normal_freq;
	u32 suspend_freq;
	u32 odt_disable_freq;
	u32 dll_disable_freq;
	u32 sr_enable_freq;
	u32 pd_enable_freq;
	u32 sr_cnt;
	u32 pd_cnt;
	u32 ddr_speed_bin;
	u32 trcd;
	u32 trp;
};

struct rk3288_dmcclk {
	struct device *dev;
	struct clk_hw hw;
	struct clk *pclk_ddrupctl0;
	struct clk *pclk_publ0;
	struct clk *pclk_ddrupctl1;
	struct clk *pclk_publ1;

	u32 cur_freq;
	u32 target_freq;
	u32 freq_slew;
	u32 dqstr_value;
	u32 clkod;
	u32 clkr;
	u32 clkf;
	int training_retries;

	u32 channel_num;
	u32 stride;
	u32 ranks[NUM_MC_CHANNEL_MAX];
	u32 rank_step[NUM_MC_CHANNEL_MAX];

	void __iomem *ddr_regs[NUM_MC_CHANNEL_MAX];
	void __iomem *phy_regs[NUM_MC_CHANNEL_MAX];
	void __iomem *ddr_regs_phys[NUM_MC_CHANNEL_MAX];
	void __iomem *phy_regs_phys[NUM_MC_CHANNEL_MAX];

	void __iomem *cru;
	/*
	 * We access at these without locking. The reason this works is that we
	 * either own the registers or access them with all interrupts disabled
	 * (across all cpus). The registers we don't own are put back in their
	 * original state before interrupts are enabled.
	 */
	void __iomem *grf;
	void __iomem *pmu;
	void __iomem *sgrf;
	void __iomem *noc;
	void __iomem *cru_phys;
	void __iomem *grf_phys;
	void __iomem *pmu_phys;
	void __iomem *noc_phys;

	struct rockchip_dmc_sram_params *sram_params;
	void *sram;
	void *sram_stack;
	void *sram_phys;
	void *sram_stack_phys;
	size_t sram_len;

	u32 ddr_type;
	/* Only record the max capability per die. */
	u32 ddr_capability_per_die;

	struct dmc_oftiming oftimings;
	struct dmc_regtiming *regtiming;

	struct ctl_timing_tag *p_ctl_timing;
	struct phy_timing_tag *p_phy_timing;
	union noc_timing_tag *p_noc_timing;
	union noc_activate_tag *p_noc_activate;
};

struct rockchip_dmc_sram_params {
	/* Filled in by the sram code */
	void (*pause_cpu_in_sram)(void *arg);
	void (*set_major_cpu)(unsigned int cpu);
	u32 (*get_major_cpu)(void);
	void (*set_major_cpu_paused)(unsigned int cpu, bool pause);
	bool (*is_cpux_paused)(unsigned int cpu);
	void (*dmc_pre_set_rate)(struct rk3288_dmcclk *dmc);
	void (*dmc_post_set_rate)(struct rk3288_dmcclk *dmc);
	void (*dmc_set_rate_in_sram)(void *arg);
	struct dmc_regtiming *(*dmc_get_regtiming_addr)(void);
};
#endif
