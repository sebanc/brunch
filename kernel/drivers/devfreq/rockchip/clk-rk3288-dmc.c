/*
* Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*/

#include <asm/cacheflush.h>
#include <asm/idmap.h>
#include <asm/tlbflush.h>

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <soc/rockchip/dmc-sync.h>
#include <soc/rockchip/rk3288-dmc-sram.h>

#include <dt-bindings/clock/rk3288-cru.h>

#include "../../clk/rockchip/clk.h"

/* mr0 for ddr3 */
#define DDR3_BL8	0
#define DDR3_BC4_8	1
#define DDR3_BC4	2
#define DDR3_CL(n)	(((((n) - 4) & 0x7) << 4) | ((((n) - 4) & 0x8) >> 1))
#define DDR3_WR(n)	(((n) & 0x7) << 9)
#define DDR3_DLL_DERESET	0

/* mr1 for ddr3 */
#define DDR3_DLL_ENABLE	0
#define DDR3_MR1_AL(n)	(((n)&0x3)<<3)

#define DDR3_DS_40		0
#define DDR3_DS_34		(1 << 1)
#define DDR3_RTT_NOM_DIS	0
#define DDR3_RTT_NOM_60		(1 << 2)
#define DDR3_RTT_NOM_120	(1 << 6)
#define DDR3_RTT_NOM_40		((1 << 2) | (1 << 6))

/* mr2 for ddr3 */
#define DDR3_MR2_CWL(n) ((((n) - 5) & 0x7) << 3)
#define DDR3_RTT_WR_DIS 0
#define DDR3_RTT_WR_60	(1 << 9)
#define DDR3_RTT_WR_120	(2 << 9)

#define DDR_PHY_DCR			0x30
#define DDR_PHY_DCR_DDRMD	0x07

#define DDR3_CL_CWL(d1, d2, d3, d4, d5, d6, d7) \
	{((d1) << 4) | 5, ((d2) << 4) | 5, ((d3) << 4) | 6, ((d4) << 4) | 7, \
	 ((d5) << 4) | 8, ((d6) << 4) | 9, ((d7) << 4) | 10}
#define DDR3_TRC_TFAW(trc, tfaw)	(((trc) << 8) | tfaw)
#define DDR_CFG2RBC(d1, d2, d3, d4) \
	((d1 << 7) | (d2 << 4) | (d3 << 2) | d4)

#define to_rk3288_dmcclk(obj)	container_of(obj, struct rk3288_dmcclk, hw)

static struct rk3288_dmcclk *g_dmc;

static const u8 ddr3_cl_cwl[][7] = {
	/*
	 * speed 0~330 331~400 401~533 534~666 667~800 801~933 934~1066
	 * tCK	>3 2.5~3 1.875~2.5 1.5~1.875 1.25~1.5 1.07~1.25 0.938~1.07
	 * cl<<4, cwl  cl<<4, cwl  cl<<4, cwl
	 */
	DDR3_CL_CWL(5, 5, 0, 0, 0, 0, 0),	/* DDR3_800D (5-5-5) */
	DDR3_CL_CWL(5, 6, 0, 0, 0, 0, 0),	/* DDR3_800E (6-6-6) */
	DDR3_CL_CWL(5, 5, 6, 0, 0, 0, 0),	/* DDR3_1066E (6-6-6) */
	DDR3_CL_CWL(5, 6, 7, 0, 0, 0, 0),	/* DDR3_1066F (7-7-7) */
	DDR3_CL_CWL(5, 6, 8, 0, 0, 0, 0),	/* DDR3_1066G (8-8-8) */
	DDR3_CL_CWL(5, 5, 6, 7, 0, 0, 0),	/* DDR3_1333F (7-7-7) */
	DDR3_CL_CWL(5, 5, 7, 8, 0, 0, 0),	/* DDR3_1333G (8-8-8) */
	DDR3_CL_CWL(5, 6, 8, 9, 0, 0, 0),	/* DDR3_1333H (9-9-9) */
	DDR3_CL_CWL(5, 6, 8, 10, 0, 0, 0),	/* DDR3_1333J (10-10-10) */
	DDR3_CL_CWL(5, 5, 6, 7, 8, 0, 0),	/* DDR3_1600G (8-8-8) */
	DDR3_CL_CWL(5, 5, 6, 8, 9, 0, 0),	/* DDR3_1600H (9-9-9) */
	DDR3_CL_CWL(5, 5, 7, 9, 10, 0, 0),	/* DDR3_1600J (10-10-10) */
	DDR3_CL_CWL(5, 6, 8, 10, 11, 0, 0),	/* DDR3_1600K (11-11-11) */
	DDR3_CL_CWL(5, 5, 6, 8, 9, 11, 0),	/* DDR3_1866J (10-10-10) */
	DDR3_CL_CWL(5, 5, 7, 8, 10, 11, 0),	/* DDR3_1866K (11-11-11) */
	DDR3_CL_CWL(6, 6, 7, 9, 11, 12, 0),	/* DDR3_1866L (12-12-12) */
	DDR3_CL_CWL(6, 6, 8, 10, 11, 13, 0),	/* DDR3_1866M (13-13-13) */
	DDR3_CL_CWL(5, 5, 6, 7, 9, 10, 11),	/* DDR3_2133K (11-11-11) */
	DDR3_CL_CWL(5, 5, 6, 8, 9, 11, 12),	/* DDR3_2133L (12-12-12) */
	DDR3_CL_CWL(5, 5, 7, 9, 10, 12, 13),	/* DDR3_2133M (13-13-13) */
	DDR3_CL_CWL(6, 6, 7, 9, 11, 13, 14),	/* DDR3_2133N (14-14-14) */
	DDR3_CL_CWL(6, 6, 8, 10, 11, 13, 14),	/* DDR3_DEFAULT */
};

static const u16 ddr3_trc_tfaw[] = {
	/* tRC      tFAW */
	DDR3_TRC_TFAW(50, 50),	/* DDR3_800D (5-5-5) */
	DDR3_TRC_TFAW(53, 50),	/* DDR3_800E (6-6-6) */
	DDR3_TRC_TFAW(49, 50),	/* DDR3_1066E (6-6-6) */
	DDR3_TRC_TFAW(51, 50),	/* DDR3_1066F (7-7-7) */
	DDR3_TRC_TFAW(53, 50),	/* DDR3_1066G (8-8-8) */
	DDR3_TRC_TFAW(47, 45),	/* DDR3_1333F (7-7-7) */
	DDR3_TRC_TFAW(48, 45),	/* DDR3_1333G (8-8-8) */
	DDR3_TRC_TFAW(50, 45),	/* DDR3_1333H (9-9-9) */
	DDR3_TRC_TFAW(51, 45),	/* DDR3_1333J (10-10-10) */
	DDR3_TRC_TFAW(45, 40),	/* DDR3_1600G (8-8-8) */
	DDR3_TRC_TFAW(47, 40),	/* DDR3_1600H (9-9-9) */
	DDR3_TRC_TFAW(48, 40),	/* DDR3_1600J (10-10-10) */
	DDR3_TRC_TFAW(49, 40),	/* DDR3_1600K (11-11-11) */
	DDR3_TRC_TFAW(45, 35),	/* DDR3_1866J (10-10-10) */
	DDR3_TRC_TFAW(46, 35),	/* DDR3_1866K (11-11-11) */
	DDR3_TRC_TFAW(47, 35),	/* DDR3_1866L (12-12-12) */
	DDR3_TRC_TFAW(48, 35),	/* DDR3_1866M (13-13-13) */
	DDR3_TRC_TFAW(44, 35),	/* DDR3_2133K (11-11-11) */
	DDR3_TRC_TFAW(45, 35),	/* DDR3_2133L (12-12-12) */
	DDR3_TRC_TFAW(46, 35),	/* DDR3_2133M (13-13-13) */
	DDR3_TRC_TFAW(47, 35),	/* DDR3_2133N (14-14-14) */
	DDR3_TRC_TFAW(53, 50),	/* DDR3_DEFAULT */
};

/* MR0 (Device Information) */
#define LPDDR2_DAI	0x1 /* 0:DAI complete, 1:DAI still in progress */
#define LPDDR2_DI	(0x1 << 1) /* 0:S2 or S4 SDRAM, 1:NVM */
#define LPDDR2_DNVI	(0x1 << 2) /* 0:DNV not supported, 1:DNV supported */
#define LPDDR2_RZQI	(0x3 << 3)
/*
 * 00:RZQ self test not supported,
 * 01:ZQ-pin may connect to VDDCA or float
 * 10:ZQ-pin may short to GND.
 * 11:ZQ-pin self test completed, no error condition detected.
 */

/* MR1 (Device Feature) */
#define LPDDR2_BL4	0x2
#define LPDDR2_BL8	0x3
#define LPDDR2_BL16	0x4
#define LPDDR2_N_WR(n)	(((n) - 2) << 5)

/* MR2 (Device Feature 2) */
#define LPDDR2_RL3_WL1	0x1
#define LPDDR2_RL4_WL2	0x2
#define LPDDR2_RL5_WL2	0x3
#define LPDDR2_RL6_WL3	0x4
#define LPDDR2_RL7_WL4	0x5
#define LPDDR2_RL8_WL4	0x6

/* MR3 (IO Configuration 1) */
#define LPDDR2_DS_34	0x1
#define LPDDR2_DS_40	0x2
#define LPDDR2_DS_48	0x3
#define LPDDR2_DS_60	0x4
#define LPDDR2_DS_80	0x6
#define LPDDR2_DS_120	0x7	/* optional */

/* MR4 (Device Temperature) */
#define LPDDR2_TREF_MASK	0x7
#define LPDDR2_4_TREF		0x1
#define LPDDR2_2_TREF		0x2
#define LPDDR2_1_TREF		0x3
#define LPDDR2_025_TREF		0x5
#define LPDDR2_025_TREF_DERATE	0x6

#define LPDDR2_TUF	(0x1 << 7)

/* MR8 (Basic configuration 4) */
#define LPDDR2_S4		0x0
#define LPDDR2_S2		0x1
#define LPDDR2_N		0x2
#define LPDDR2_DENSITY(mr8)	(8 << (((mr8) >> 2) & 0xf))	/* Unit:MB */
#define LPDDR2_IO_WIDTH(mr8)	(32 >> (((mr8) >> 6) & 0x3))

/* MR10 (Calibration) */
#define LPDDR2_ZQINIT	0xff
#define LPDDR2_ZQCL	0xab
#define LPDDR2_ZQCS	0x56
#define LPDDR2_ZQRESET	0xc3

/* MR16 (PASR Bank Mask) */
/* S2 SDRAM Only*/
#define LPDDR2_PASR_FULL	0x0
#define LPDDR2_PASR_1_2		0x1
#define LPDDR2_PASR_1_4		0x2
#define LPDDR2_PASR_1_8		0x3

/* MR0 (Device Information) */
/* 0:DAI complete,
 * 1:DAI still in progress */
#define LPDDR3_DAI	0x1
/* 00:RZQ self test not supported,
 * 01:ZQ-pin may connect to VDDCA or float
 * 10:ZQ-pin may short to GND.
 * 11:ZQ-pin self test completed, no error condition detected. */
#define LPDDR3_RZQI		(0x3 << 3)
/* 0:DRAM does not support WL(Set B),
 * 1:DRAM support WL(Set B) */
#define LPDDR3_WL_SUPOT		(1 << 6)
/* 0:DRAM does not support RL=3,nWR=3,WL=1;
 * 1:DRAM supports RL=3,nWR=3,WL=1 for frequencies <=166 */
#define LPDDR3_RL3_SUPOT	(1 << 7)

/* MR1 (Device Feature) */
#define LPDDR3_BL8	0x3
#define LPDDR3_N_WR(n)	((n) << 5)

/* MR2 (Device Feature 2) */
/* WL Set A,default */
#define LPDDR3_RL3_WL1	0x1	/* <=166MHz,optional */
#define LPDDR3_RL6_WL3	0x4	/* <=400MHz */
#define LPDDR3_RL8_WL4	0x6	/* <=533MHz */
#define LPDDR3_RL9_WL5	0x7	/* <=600MHz */
#define LPDDR3_RL10_WL6 0x8	/* <=667MHz,default */
#define LPDDR3_RL11_WL6 0x9	/* <=733MHz */
#define LPDDR3_RL12_WL6 0xa	/* <=800MHz */
#define LPDDR3_RL14_WL8 0xc	/* <=933MHz */
#define LPDDR3_RL16_WL8	0xe	/* <=1066MHz */
/* WL Set B, optional */
#define LPDDR3_RL10_WL8		0x8	/* <=667MHz,default */
#define LPDDR3_RL11_WL9		0x9	/* <=733MHz */
#define LPDDR3_RL12_WL9		0xa	/* <=800MHz */
#define LPDDR3_RL14_WL11	0xc	/* <=933MHz */
#define LPDDR3_RL16_WL13	0xe	/* <=1066MHz */

#define LPDDR3_N_WRE	(1 << 4) /* 1:enable nWR programming > 9(default) */
#define LPDDR3_WL_S	(1 << 6) /* 1:Select WL Set B */
#define LPDDR3_WR_LEVEL	(1 << 7) /* 1:enable */

/* MR3 (IO Configuration 1) */
#define LPDDR3_DS_34		0x1
#define LPDDR3_DS_40		0x2
#define LPDDR3_DS_48		0x3
#define LPDDR3_DS_60		0x4
#define LPDDR3_DS_80		0x6
#define LPDDR3_DS_34D_40U	0x9
#define LPDDR3_DS_40D_48U	0xa
#define LPDDR3_DS_34D_48U	0xb

/* MR4 (Device Temperature) */
#define LPDDR3_TREF_MASK	0x7
/* SDRAM Low temperature operating limit exceeded */
#define LPDDR3_LT_EXED		0x0
#define LPDDR3_4_TREF		0x1
#define LPDDR3_2_TREF		0x2
#define LPDDR3_1_TREF		0x3
#define LPDDR3_05_TREF		0x4
#define LPDDR3_025_TREF		0x5
#define LPDDR3_025_TREF_DERATE	0x6
/* SDRAM High temperature operating limit exceeded */
#define LPDDR3_HT_EXED		0x7

/* 1:value has changed since last read of MR4 */
#define LPDDR3_TUF	(0x1 << 7)

/* MR8 (Basic configuration 4) */
#define LPDDR3_S8		0x3
#define LPDDR3_DENSITY(mr8)	(8 << (((mr8) >> 2) & 0xf))
#define LPDDR3_IO_WIDTH(mr8)	(32 >> (((mr8) >> 6) & 0x3))

/* MR10 (Calibration) */
#define LPDDR3_ZQINIT	0xff
#define LPDDR3_ZQCL	0xab
#define LPDDR3_ZQCS	0x56
#define LPDDR3_ZQRESET	0xc3

/* MR11 (ODT Control) */
#define LPDDR3_ODT_60	1
#define LPDDR3_ODT_120	2
#define LPDDR3_ODT_240	3
#define LPDDR3_ODT_DIS	0

#define DDR3_TREFI_7_8_US	78	/* unit 100ns */
#define DDR3_TMRD		4	/* tMRD, 4 tCK */
#define DDR3_TRFC_512MBIT	90	/* ns */
#define DDR3_TRFC_1GBIT		110	/* ns */
#define DDR3_TRFC_2GBIT		160	/* ns */
#define DDR3_TRFC_4GBIT		300	/* ns */
#define DDR3_TRFC_8GBIT		350	/* ns */
#define DDR3_TRTW		2	/* register min valid value */
#define DDR3_TRAS		37	/* tRAS,37.5ns(400MHz) 37.5ns(533MHz) */
#define DDR3_TRRD		10	/* tRRD = max(4nCK, 10ns) */
#define DDR3_TRTP		7	/* tRTP, max(4 tCK,7.5ns) */
#define DDR3_TWR		15	/* tWR, 15ns */
#define DDR3_TWTR		7	/* tWTR, max(4 tCK,7.5ns) */
#define DDR3_TXP		7	/* tXP, max(3 tCK, 7.5ns)(<933MHz) */
#define DDR3_TXPDLL		24	/* tXPDLL, max(10 tCK,24ns) */
#define DDR3_TZQCS		80	/* tZQCS, max(64 tCK, 80ns) */
#define DDR3_TZQCSI		0	/* ns */
#define DDR3_TDQS		1	/* tCK */
#define DDR3_TCKSRE		10	/* tCKSRX, max(5 tCK, 10ns) */
#define DDR3_TCKE_400MHZ	7	/* tCKE, max(3 tCK,7.5ns)(400MHz) */
#define DDR3_TCKE_533MHZ	6	/* tCKE, max(3 tCK,5.625ns)(533MHz) */
#define DDR3_TMOD		15	/* tMOD, max(12 tCK,15ns) */
#define DDR3_TRSTL		100	/* tRSTL, 100ns */
#define DDR3_TZQCL		320	/* tZQCL, max(256 tCK, 320ns) */
#define DDR3_TDLLK		512	/* tXSR, =tDLLK=512 tCK */

extern char _binary_arch_arm_mach_rockchip_embedded_rk3288_sram_bin_start;
extern char _binary_arch_arm_mach_rockchip_embedded_rk3288_sram_bin_end;
extern void call_with_mmu_disabled(void (*fn) (void *), void *arg, void *sp);

static int ddr3_get_parameter(struct rk3288_dmcclk *dmc)
{
	u32 tmp, al, bl, bl_tmp, cl, cwl, ch;
	u32 ret = 0;
	u32 nmhz = dmc->target_freq;
	u32 ddr_speed_bin = 21;
	u32 ddr_capability_per_die = dmc->ddr_capability_per_die;

	for (ch = 0; ch < dmc->channel_num; ch++) {
		if (ddr_speed_bin == 21 ||
		    ddr_speed_bin < dmc->oftimings.ddr_speed_bin)
			ddr_speed_bin = dmc->oftimings.ddr_speed_bin;
	}

	if (ddr_speed_bin > 21) {
		ret = -EPERM;
		goto out;
	}

	dmc->p_ctl_timing->togcnt1u = nmhz;
	dmc->p_ctl_timing->togcnt100n = nmhz / 10;
	dmc->p_ctl_timing->tinit = 200;
	dmc->p_ctl_timing->trsth = 500;

	al = 0;
	bl = 8;

	if (nmhz <= 330)
		tmp = 0;
	else if (nmhz <= 400)
		tmp = 1;
	else if (nmhz <= 533)
		tmp = 2;
	else if (nmhz <= 666)
		tmp = 3;
	else if (nmhz <= 800)
		tmp = 4;
	else if (nmhz <= 933)
		tmp = 5;
	else
		tmp = 6;

	/* when dll bypss cl = cwl = 6 */
	if (nmhz < 300) {
		cl = 6;
		cwl = 6;
	} else {
		cl = (ddr3_cl_cwl[ddr_speed_bin][tmp] >> 4) & 0xf;
		cwl = ddr3_cl_cwl[ddr_speed_bin][tmp] & 0xf;
	}

	if (cl == 0)
		ret = -EPERM;

	if (nmhz <= dmc->oftimings.odt_disable_freq)
		dmc->p_phy_timing->mr[1] = DDR3_DS_40 | DDR3_RTT_NOM_DIS;
	else
		dmc->p_phy_timing->mr[1] = DDR3_DS_40 | DDR3_RTT_NOM_120;

	dmc->p_phy_timing->mr[2] = DDR3_MR2_CWL(cwl);

	/* tREFI, average periodic refresh interval, 7.8us */
	dmc->p_ctl_timing->trefi = DDR3_TREFI_7_8_US;
	dmc->p_ctl_timing->tmrd = DDR3_TMRD & 0x7;
	dmc->p_phy_timing->dtpr0.b.tmrd = DDR3_TMRD - 4;
	/* tRFC, 90ns(512Mb),110ns(1Gb),160ns(2Gb),300ns(4Gb),350ns(8Gb) */
	if (ddr_capability_per_die <= 0x4000000)
		tmp = DDR3_TRFC_512MBIT;
	else if (ddr_capability_per_die <= 0x8000000)
		tmp = DDR3_TRFC_1GBIT;
	else if (ddr_capability_per_die <= 0x10000000)
		tmp = DDR3_TRFC_2GBIT;
	else if (ddr_capability_per_die <= 0x20000000)
		tmp = DDR3_TRFC_4GBIT;
	else
		tmp = DDR3_TRFC_8GBIT;

	dmc->p_ctl_timing->trfc = DIV_ROUND_UP(tmp * nmhz, 1000);
	dmc->p_phy_timing->dtpr1.b.trfc = DIV_ROUND_UP(tmp * nmhz, 1000);
	dmc->p_ctl_timing->texsr = DDR3_TDLLK;
	dmc->p_phy_timing->dtpr2.b.txs = DDR3_TDLLK;
	dmc->p_ctl_timing->trp = cl;
	dmc->p_phy_timing->dtpr0.b.trp = cl;
	dmc->p_noc_timing->b.wr_to_miss =
		(cwl + DIV_ROUND_UP(DDR3_TWR * nmhz, 1000) + cl + cl);
	dmc->p_ctl_timing->trc =
		DIV_ROUND_UP((ddr3_trc_tfaw[ddr_speed_bin] >> 8) * nmhz, 1000) &
		0x3f;
	dmc->p_noc_timing->b.act_to_act =
		DIV_ROUND_UP((ddr3_trc_tfaw[ddr_speed_bin] >> 8) * nmhz, 1000);
	dmc->p_phy_timing->dtpr0.b.trc =
		DIV_ROUND_UP((ddr3_trc_tfaw[ddr_speed_bin] >> 8) * nmhz, 1000);

	dmc->p_ctl_timing->trtw = (cl + 2 - cwl);
	dmc->p_phy_timing->dtpr1.b.trtw = 0;
	dmc->p_noc_timing->b.rd_to_wr = (cl + 2 - cwl);
	dmc->p_ctl_timing->tal = al;
	dmc->p_ctl_timing->tcl = cl;
	dmc->p_ctl_timing->tcwl = cwl;
	dmc->p_ctl_timing->tras =
		DIV_ROUND_UP(DDR3_TRAS * nmhz + (nmhz >> 1), 1000) & 0x3f;
	dmc->p_phy_timing->dtpr0.b.tras =
		DIV_ROUND_UP(DDR3_TRAS * nmhz + (nmhz >> 1), 1000);

	dmc->p_ctl_timing->trcd = cl;
	dmc->p_phy_timing->dtpr0.b.trcd = cl;
	tmp = DIV_ROUND_UP(DDR3_TRRD * nmhz, 1000);
	tmp = max_t(u32, tmp, 4);
	dmc->p_ctl_timing->trrd = (tmp & 0xf);
	dmc->p_phy_timing->dtpr0.b.trrd = tmp;
	dmc->p_noc_activate->b.rrd = tmp;

	tmp = DIV_ROUND_UP(DDR3_TRTP * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, 4);
	dmc->p_ctl_timing->trtp = tmp & 0xf;
	dmc->p_phy_timing->dtpr0.b.trtp = tmp;
	dmc->p_noc_timing->b.rd_to_miss = (tmp + cl + cl - (bl >> 1));

	tmp = DIV_ROUND_UP(DDR3_TWR * nmhz, 1000);
	dmc->p_ctl_timing->twr = tmp & 0x1f;
	if (tmp < 9) {
		tmp = tmp - 4;
	} else {
		tmp += (tmp & 0x1) ? 1 : 0;
		tmp = tmp >> 1;
	}
	bl_tmp = DDR3_BL8;
	dmc->p_phy_timing->mr[0] = bl_tmp | DDR3_CL(cl) | DDR3_WR(tmp);

	tmp = DIV_ROUND_UP(DDR3_TWTR * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, (u32)4);
	dmc->p_ctl_timing->twtr = tmp & 0xf;
	dmc->p_phy_timing->dtpr0.b.twtr = tmp;
	dmc->p_noc_timing->b.wr_to_rd = (tmp + cwl);

	tmp = DIV_ROUND_UP(DDR3_TXP * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, 3);
	dmc->p_ctl_timing->txp = tmp & 0x7;

	tmp = DIV_ROUND_UP(DDR3_TXPDLL * nmhz, 1000);
	tmp = max_t(u32, tmp, 10);
	dmc->p_ctl_timing->txpdll = tmp & 0x3f;
	dmc->p_phy_timing->dtpr2.b.txp = tmp;

	tmp = DIV_ROUND_UP(DDR3_TZQCS * nmhz, 1000);
	tmp = max_t(u32, tmp, 64);
	dmc->p_ctl_timing->tzqcs = tmp & 0x7f;
	dmc->p_ctl_timing->tzqcsi = DDR3_TZQCSI;
	dmc->p_ctl_timing->tdqs = DDR3_TDQS;

	tmp = DIV_ROUND_UP(DDR3_TCKSRE * nmhz, 1000);
	tmp = max_t(u32, tmp, 5);
	dmc->p_ctl_timing->tcksre = tmp & 0x1f;
	dmc->p_ctl_timing->tcksrx = tmp & 0x1f;
	if (nmhz >= 533)
		tmp = DIV_ROUND_UP(DDR3_TCKE_533MHZ * nmhz, 1000);
	else
		tmp = DIV_ROUND_UP(DDR3_TCKE_400MHZ * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, 3);
	dmc->p_ctl_timing->tcke = tmp & 0x7;
	dmc->p_ctl_timing->tckesr = (tmp + 1) & 0xf;
	dmc->p_phy_timing->dtpr2.b.tcke = tmp + 1;

	tmp = DIV_ROUND_UP(DDR3_TMOD * nmhz, 1000);
	tmp = max_t(u32, tmp, 12);
	dmc->p_ctl_timing->tmod = tmp & 0x1f;
	dmc->p_phy_timing->dtpr1.b.tmod = (tmp - 12);
	dmc->p_ctl_timing->trstl = DIV_ROUND_UP(DDR3_TRSTL * nmhz, 1000) & 0x7f;

	tmp = DIV_ROUND_UP(DDR3_TZQCL * nmhz, 1000);
	tmp = max_t(u32, tmp, 256);
	dmc->p_ctl_timing->tzqcl = tmp & 0x3ff;
	dmc->p_ctl_timing->tmrr = 0;
	dmc->p_ctl_timing->tdpd = 0;

	dmc->p_phy_timing->dtpr0.b.tccd = 0;
	dmc->p_phy_timing->dtpr1.b.tdqsck_max = 0;
	/*
	 * tRTODT, 0:ODT may be turned on immediately
	 * after read post-amble
	 * 1:ODT may not be turned on until one clock
	 * after the read post-amble
	 */
	dmc->p_phy_timing->dtpr1.b.trtodt = 1;

	/*
	 * tFAW, 40ns(400MHz 1KB page)
	 * 37.5ns(533MHz 1KB page)
	 * 50ns(400MHz 2KB page)
	 * 50ns(533MHz 2KB page)
	 */
	tmp = DIV_ROUND_UP((ddr3_trc_tfaw[ddr_speed_bin] & 0x0ff) * nmhz, 1000);
	dmc->p_phy_timing->dtpr1.b.tfaw = tmp;
	dmc->p_noc_activate->b.faw_bank = 1;
	dmc->p_noc_activate->b.faw = tmp;
	dmc->p_phy_timing->dtpr1.b.taond = 0;
	dmc->p_phy_timing->dtpr2.b.tdllk = DDR3_TDLLK;
	dmc->p_noc_timing->b.burst_len = (bl >> 1);

out:
	return ret;
}

#define LPDDR2_TREFI_3_9_US		39	/* unit 100ns */
#define LPDDR2_TREFI_7_8_US		78	/* unit 100ns */
#define LPDDR2_TMRD			5	/* tMRD, (=tMRW), 5 tCK */
#define LPDDR2_TRFC_8GBIT		210	/* ns */
#define LPDDR2_TRFC_4GBIT		130	/* ns */
#define LPDDR2_TRP_PB_4_BANK		24	/* ns */
#define LPDDR2_TRP_AB_SUB_TRP_PB_4_BANK	0	/* ns */
#define LPDDR2_TRP_PB_8_BANK		24	/* ns */
#define LPDDR2_TRP_AB_SUB_TRP_PB_8_BANK	3	/* ns */
#define LPDDR2_TRTW			1	/* tCK reg min valid value */
#define LPDDR2_TRAS			42	/* tRAS, max(3tCK,42ns) */
#define LPDDR2_TRCD			24	/* tRCD,15ns-18ns-24ns */
#define LPDDR2_TRRD			10	/* tRRD, max(2tCK,10ns) */
#define LPDDR2_TRTP			7	/* tRTP, max(2tCK, 7.5ns) */
#define LPDDR2_TWR			15	/* tWR, max(3tCK,15ns) */
#define LPDDR2_TWTR_GREAT_200MHZ	7	/* ns */
#define LPDDR2_TWTR_LITTLE_200MHZ	10	/* ns */
#define LPDDR2_TXP			7	/* tXP, max(2tCK,7.5ns) */
#define LPDDR2_TXPDLL			0
#define LPDDR2_TZQCS			90	/* tZQCS, 90ns */
#define LPDDR2_TZQCSI			0
#define LPDDR2_TDQS			1
#define LPDDR2_TCKSRE			1	/* tCK */
#define LPDDR2_TCKSRX			2	/* tCK */
#define LPDDR2_TCKE			3	/* tCK */
#define LPDDR2_TMOD			0
#define LPDDR2_TRSTL			0
#define LPDDR2_TZQCL			360	/* tZQCL, 360ns */
#define LPDDR2_TMRR			2	/* tCK */
#define LPDDR2_TCKESR			15	/* tCKESR, max(3tCK,15ns) */
#define LPDDR2_TDPD_US			500	/* us */
#define LPDDR2_TFAW_GREAT_200MHZ	50	/* ns */
#define LPDDR2_TFAW_LITTLE_200MHZ	60	/* ns */
#define LPDDR2_TDLLK			2	/* tCK */
#define LPDDR2_TDQSCK_MAX		3	/* tDQSCKmax,5.5ns */
#define LPDDR2_TDQSCK_MIN		0	/* tDQSCKmin,2.5ns */
#define LPDDR2_TDQSS			1	/* tCK */

static int lpddr2_get_parameter(struct rk3288_dmcclk *dmc)
{
	u32 tmp, al, bl, bl_tmp, cl, cwl, trp_tmp, trcd_tmp, tras_tmp,
		trtp_tmp, twr_tmp;
	u32 nmhz = dmc->target_freq;
	u32 ddr_capability_per_die = dmc->ddr_capability_per_die;

	dmc->p_ctl_timing->togcnt1u = nmhz;
	dmc->p_ctl_timing->togcnt100n = nmhz / 10;
	dmc->p_ctl_timing->tinit = 200;
	dmc->p_ctl_timing->trsth = 500;

	al = 0;
	bl = 8;

	/*
	 *     1066 933 800 667 533 400 333
	 * RL,   8   7   6   5   4   3   3
	 * WL,   4   4   3   2   2   1   1
	 */
	if (nmhz <= 200) {
		cl = 3;
		cwl = 1;
		dmc->p_phy_timing->mr[2] = LPDDR2_RL3_WL1;
	} else if (nmhz <= 266) {
		cl = 4;
		cwl = 2;
		dmc->p_phy_timing->mr[2] = LPDDR2_RL4_WL2;
	} else if (nmhz <= 333) {
		cl = 5;
		cwl = 2;
		dmc->p_phy_timing->mr[2] = LPDDR2_RL5_WL2;
	} else if (nmhz <= 400) {
		cl = 6;
		cwl = 3;
		dmc->p_phy_timing->mr[2] = LPDDR2_RL6_WL3;
	} else if (nmhz <= 466) {
		cl = 7;
		cwl = 4;
		dmc->p_phy_timing->mr[2] = LPDDR2_RL7_WL4;
	} else {
		cl = 8;
		cwl = 4;
		dmc->p_phy_timing->mr[2] = LPDDR2_RL8_WL4;
	}
	dmc->p_phy_timing->mr[0] = 0;

	/*
	 * tREFI, average periodic refresh interval,
	 * 15.6us(<256Mb) 7.8us(256Mb-1Gb) 3.9us(2Gb-8Gb)
	 */
	if (ddr_capability_per_die >= 0x10000000)
		dmc->p_ctl_timing->trefi = LPDDR2_TREFI_3_9_US;
	else
		dmc->p_ctl_timing->trefi = LPDDR2_TREFI_7_8_US;
	dmc->p_ctl_timing->tmrd = LPDDR2_TMRD & 0x7;
	dmc->p_phy_timing->dtpr0.b.tmrd = 3;
	/* tRFC, 90ns(<=512Mb) 130ns(1Gb-4Gb) 210ns(8Gb) */
	if (ddr_capability_per_die >= 0x40000000) {
		dmc->p_ctl_timing->trfc =
			DIV_ROUND_UP(LPDDR2_TRFC_8GBIT * nmhz, 1000);
		dmc->p_phy_timing->dtpr1.b.trfc =
			DIV_ROUND_UP(LPDDR2_TRFC_8GBIT * nmhz, 1000);
		tmp = DIV_ROUND_UP((LPDDR2_TRFC_8GBIT + 10) * nmhz, 1000);
	} else {
		dmc->p_ctl_timing->trfc =
			DIV_ROUND_UP(LPDDR2_TRFC_4GBIT * nmhz, 1000);
		dmc->p_phy_timing->dtpr1.b.trfc =
			DIV_ROUND_UP(LPDDR2_TRFC_4GBIT * nmhz, 1000);
		tmp = DIV_ROUND_UP((LPDDR2_TRFC_4GBIT + 10) * nmhz, 1000);
	}
	tmp = max_t(u32, tmp, 2);
	dmc->p_ctl_timing->texsr = tmp & 0x3ff;
	dmc->p_phy_timing->dtpr2.b.txs = tmp;

	/*
	 * tRP, max(3tCK, 4-bank:15ns(Fast) 18ns(Typ) 24ns(Slow),
	 *  8-bank:18ns(Fast) 21ns(Typ) 27ns(Slow))
	 */
	trp_tmp = DIV_ROUND_UP(LPDDR2_TRP_PB_8_BANK * nmhz, 1000);
	trp_tmp = max_t(u32, trp_tmp, 3);
	dmc->p_ctl_timing->trp =
		((DIV_ROUND_UP(LPDDR2_TRP_AB_SUB_TRP_PB_8_BANK * nmhz, 1000) &
		  0x3) << 16) | (trp_tmp & 0xf);
	dmc->p_phy_timing->dtpr0.b.trp = trp_tmp;

	tras_tmp = DIV_ROUND_UP(LPDDR2_TRAS * nmhz, 1000);
	tras_tmp = max_t(u32, tras_tmp, 3);
	dmc->p_ctl_timing->tras = (tras_tmp & 0x3f);
	dmc->p_phy_timing->dtpr0.b.tras = tras_tmp;

	trcd_tmp = DIV_ROUND_UP(LPDDR2_TRCD * nmhz, 1000);
	trcd_tmp = max_t(u32, trcd_tmp, 3);
	dmc->p_ctl_timing->trcd = (trcd_tmp & 0xf);
	dmc->p_phy_timing->dtpr0.b.trcd = trcd_tmp;

	trtp_tmp = DIV_ROUND_UP(LPDDR2_TRTP * nmhz + (nmhz >> 1), 1000);
	trtp_tmp = max_t(u32, trtp_tmp, 2);
	dmc->p_ctl_timing->trtp = trtp_tmp & 0xF;
	dmc->p_phy_timing->dtpr0.b.trtp = trtp_tmp;

	twr_tmp = DIV_ROUND_UP(LPDDR2_TWR * nmhz, 1000);
	twr_tmp = max_t(u32, twr_tmp, 3);
	dmc->p_ctl_timing->twr = twr_tmp & 0x1f;
	bl_tmp = (bl == 16) ? LPDDR2_BL16 :
			      ((bl == 8) ? LPDDR2_BL8 : LPDDR2_BL4);
	dmc->p_phy_timing->mr[1] = bl_tmp | LPDDR2_N_WR(twr_tmp);

	dmc->p_noc_timing->b.wr_to_miss = (cwl + twr_tmp + trp_tmp + trcd_tmp);
	dmc->p_noc_timing->b.rd_to_miss =
		(trtp_tmp + trp_tmp + trcd_tmp - (bl >> 1));
	dmc->p_ctl_timing->trc = ((tras_tmp + trp_tmp) & 0x3f);
	dmc->p_noc_timing->b.act_to_act = (tras_tmp + trp_tmp);
	dmc->p_phy_timing->dtpr0.b.trc = (tras_tmp + trp_tmp);

	dmc->p_ctl_timing->trtw = (cl + 2 - cwl);
	dmc->p_phy_timing->dtpr1.b.trtw = 0;
	dmc->p_noc_timing->b.rd_to_wr = (cl + 2 - cwl);
	dmc->p_ctl_timing->tal = al;
	dmc->p_ctl_timing->tcl = cl;
	dmc->p_ctl_timing->tcwl = cwl;

	tmp = DIV_ROUND_UP(LPDDR2_TRRD * nmhz, 1000);
	tmp = max_t(u32, tmp, 2);
	dmc->p_ctl_timing->trrd = (tmp & 0xf);
	dmc->p_phy_timing->dtpr0.b.trrd = tmp;
	dmc->p_noc_activate->b.rrd = tmp;

	/* tWTR, max(2tCK, 7.5ns(533-266MHz)  10ns(200-166MHz)) */
	if (nmhz > 200)
		tmp = DIV_ROUND_UP(LPDDR2_TWTR_GREAT_200MHZ * nmhz +
				   (nmhz >> 1), 1000);
	else
		tmp = DIV_ROUND_UP(LPDDR2_TWTR_LITTLE_200MHZ * nmhz, 1000);
	tmp = max_t(u32, tmp, 2);
	dmc->p_ctl_timing->twtr = tmp & 0xf;
	dmc->p_phy_timing->dtpr0.b.twtr = tmp;
	dmc->p_noc_timing->b.wr_to_rd = (cwl + tmp);

	tmp = DIV_ROUND_UP(LPDDR2_TXP * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, 2);
	dmc->p_ctl_timing->txp = tmp & 0x7;
	dmc->p_phy_timing->dtpr2.b.txp = tmp;
	dmc->p_ctl_timing->txpdll = LPDDR2_TXPDLL;
	dmc->p_ctl_timing->tzqcs =
		DIV_ROUND_UP(LPDDR2_TZQCS * nmhz, 1000) & 0x7f;
	dmc->p_ctl_timing->tzqcsi = LPDDR2_TZQCSI;
	dmc->p_ctl_timing->tdqs = LPDDR2_TDQS;
	dmc->p_ctl_timing->tcksre = LPDDR2_TCKSRE;
	dmc->p_ctl_timing->tcksrx = LPDDR2_TCKSRX;
	dmc->p_ctl_timing->tcke = LPDDR2_TCKE;
	dmc->p_phy_timing->dtpr2.b.tcke = LPDDR2_TCKE;
	dmc->p_ctl_timing->tmod = LPDDR2_TMOD;
	dmc->p_phy_timing->dtpr1.b.tmod = LPDDR2_TMOD;
	dmc->p_ctl_timing->trstl = LPDDR2_TRSTL;
	dmc->p_ctl_timing->tzqcl =
		DIV_ROUND_UP(LPDDR2_TZQCL * nmhz, 1000) & 0x3ff;
	dmc->p_ctl_timing->tmrr = LPDDR2_TMRR;

	tmp = DIV_ROUND_UP(LPDDR2_TCKESR * nmhz, 1000);
	tmp = max_t(u32, tmp, 3);
	dmc->p_ctl_timing->tckesr = tmp & 0xF;
	dmc->p_ctl_timing->tdpd = LPDDR2_TDPD_US;
	dmc->p_phy_timing->dtpr0.b.tccd = 0;
	dmc->p_phy_timing->dtpr1.b.tdqsck_max = LPDDR2_TDQSCK_MAX;
	dmc->p_phy_timing->dtpr1.b.tdqsck = LPDDR2_TDQSCK_MIN;
	/*
	 * tRTODT, 0:ODT may be turned on immediately after read post-amble
	 *         1:ODT may not be turned on until one clock after the read
	 *         post-amble
	*/
	dmc->p_phy_timing->dtpr1.b.trtodt = 1;
	/* tFAW,max(8tCK, 50ns(200-533MHz)	60ns(166MHz)) */
	if (nmhz >= 200)
		tmp = DIV_ROUND_UP(LPDDR2_TFAW_GREAT_200MHZ * nmhz, 1000);
	else
		tmp = DIV_ROUND_UP(LPDDR2_TFAW_LITTLE_200MHZ * nmhz, 1000);
	tmp = max_t(u32, tmp, 8);
	dmc->p_phy_timing->dtpr1.b.tfaw = tmp;
	dmc->p_noc_activate->b.faw_bank = 1;
	dmc->p_noc_activate->b.faw = tmp;
	dmc->p_phy_timing->dtpr1.b.taond = 0;
	dmc->p_phy_timing->dtpr2.b.tdllk = LPDDR2_TDLLK;
	dmc->p_noc_timing->b.burst_len = (bl >> 1);

	return 0;
}

#define LPDDR3_TREFI_3_9_US		39	/* unit 100ns */
#define LPDDR3_TMRD			10	/* tMRD, (=tMRW), 10 tCK */
#define LPDDR3_TRFC_8GBIT		210	/* 130ns(4Gb) 210ns(>4Gb) */
#define LPDDR3_TRFC_4GBIT		130	/* ns */
#define LPDDR3_TRP_PB_8_BANK		24	/* tRP, 18ns-21ns-27ns */
#define LPDDR3_TRP_AB_SUB_TRP_PB_8_BANK	3	/* ns */
#define LPDDR3_TRTW			1	/* tCK reg min valid value */
#define LPDDR3_TRAS			42	/* tRAS, max(3tCK,42ns) */
#define LPDDR3_TRCD			24	/* tRCD, 15ns-18ns-24ns */
#define LPDDR3_TRRD			10	/* tRRD, max(2tCK,10ns) */
#define LPDDR3_TRTP			7	/* tRTP, max(4tCK, 7.5ns) */
#define LPDDR3_TWR			15	/* tWR, max(4tCK,15ns) */
#define LPDDR3_TWTR			7	/* tWTR, max(4tCK, 7.5ns) */
#define LPDDR3_TXP			7	/* tXP, max(3tCK,7.5ns) */
#define LPDDR3_TXPDLL			0
#define LPDDR3_TZQCS			90	/* tZQCS, 90ns */
#define LPDDR3_TZQCSI			0
#define LPDDR3_TDQS			1
#define LPDDR3_TCKSRE			2	/* tCKSRE=tCPDED, 2 tCK */
#define LPDDR3_TCKSRX			2	/* tCKSRX, 2 tCK */
#define LPDDR3_TCKE			3	/* tCKE, (max 7.5ns,3 tCK) */
#define LPDDR3_TMOD			0
#define LPDDR3_TRSTL			0
#define LPDDR3_TZQCL			360	/* tZQCL, 360ns */
#define LPDDR3_TMRR			4	/* tMRR, 4 tCK */
#define LPDDR3_TCKESR			15	/* tCKESR, max(3tCK,15ns) */
#define LPDDR3_TDPD_US			500	/* tDPD, 500us */
#define LPDDR3_TFAW			50	/* tFAW,max(8tCK, 50ns) */
#define LPDDR3_TDLLK			2	/* tCK */
#define LPDDR3_TDQSCK_MAX		3	/* tDQSCKmax,5.5ns */
#define LPDDR3_TDQSCK_MIN		0	/* tDQSCKmin,2.5ns */
#define LPDDR3_TDQSS			1	/* tCK */

static int lpddr3_get_parameter(struct rk3288_dmcclk *dmc)
{
	u32 tmp, al, bl, bl_tmp, cl, cwl, trp_tmp, trcd_tmp, tras_tmp,
		trtp_tmp, twr_tmp;
	u32 nmhz = dmc->target_freq;
	u32 ddr_capability_per_die = dmc->ddr_capability_per_die;

	dmc->p_ctl_timing->togcnt1u = nmhz;
	dmc->p_ctl_timing->togcnt100n = nmhz / 10;
	dmc->p_ctl_timing->tinit = 200;
	dmc->p_ctl_timing->trsth = 500;

	al = 0;
	bl = 8;

	/*
	 * Only support Write Latency Set A here
	 *     1066 933 800 733 667 600 533 400 166
	 * RL,	16  14  12  11  10  9	 8   6   3
	 * WL,	 8  8   6    6   6  5    4   3   1
	 */
	if (nmhz <= 166) {
		cl = 3;
		cwl = 1;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL3_WL1;
	} else if (nmhz <= 400) {
		cl = 6;
		cwl = 3;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL6_WL3;
	} else if (nmhz <= 533) {
		cl = 8;
		cwl = 4;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL8_WL4;
	} else if (nmhz <= 600) {
		cl = 9;
		cwl = 5;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL9_WL5;
	} else if (nmhz <= 667) {
		cl = 10;
		cwl = 6;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL10_WL6;
	} else if (nmhz <= 733) {
		cl = 11;
		cwl = 6;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL11_WL6;
	} else if (nmhz <= 800) {
		cl = 12;
		cwl = 6;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL12_WL6;
	} else if (nmhz <= 933) {
		cl = 14;
		cwl = 8;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL14_WL8;
	} else {
		cl = 16;
		cwl = 8;
		dmc->p_phy_timing->mr[2] = LPDDR3_RL16_WL8;
	}

	if (nmhz <= dmc->oftimings.odt_disable_freq)
		dmc->p_phy_timing->mr11 = LPDDR3_ODT_DIS;
	else
		dmc->p_phy_timing->mr11 = LPDDR3_ODT_240;

	dmc->p_phy_timing->mr[0] = 0;

	/* tREFI, average periodic refresh interval, 3.9us(4Gb-16Gb) */
	dmc->p_ctl_timing->trefi = LPDDR3_TREFI_3_9_US;
	dmc->p_ctl_timing->tmrd = LPDDR3_TMRD & 0x7;
	dmc->p_phy_timing->dtpr0.b.tmrd = 3;
	if (ddr_capability_per_die > 0x20000000) {
		dmc->p_ctl_timing->trfc =
			DIV_ROUND_UP(LPDDR3_TRFC_8GBIT * nmhz, 1000);
		dmc->p_phy_timing->dtpr1.b.trfc =
			DIV_ROUND_UP(LPDDR3_TRFC_8GBIT * nmhz, 1000);
		tmp = DIV_ROUND_UP((LPDDR3_TRFC_8GBIT + 10) * nmhz, 1000);
	} else {
		dmc->p_ctl_timing->trfc =
			DIV_ROUND_UP(LPDDR3_TRFC_4GBIT * nmhz, 1000);
		dmc->p_phy_timing->dtpr1.b.trfc =
			DIV_ROUND_UP(LPDDR3_TRFC_4GBIT * nmhz, 1000);
		tmp = DIV_ROUND_UP((LPDDR3_TRFC_4GBIT + 10) * nmhz, 1000);
	}
	tmp = max_t(u32, tmp, 2);
	dmc->p_ctl_timing->texsr = tmp & 0x3ff;
	dmc->p_phy_timing->dtpr2.b.txs = tmp;

	trp_tmp = DIV_ROUND_UP(LPDDR3_TRP_PB_8_BANK * nmhz, 1000);
	trp_tmp = max_t(u32, trp_tmp, 3);
	dmc->p_ctl_timing->trp =
		((DIV_ROUND_UP(LPDDR3_TRP_AB_SUB_TRP_PB_8_BANK * nmhz, 1000) &
		  0x3) << 16) | (trp_tmp & 0xf);
	dmc->p_phy_timing->dtpr0.b.trp = trp_tmp;

	tras_tmp = DIV_ROUND_UP(LPDDR3_TRAS * nmhz, 1000);
	tras_tmp = max_t(u32, tras_tmp, 3);
	dmc->p_ctl_timing->tras = (tras_tmp & 0x3f);
	dmc->p_phy_timing->dtpr0.b.tras = tras_tmp;

	trcd_tmp = DIV_ROUND_UP(LPDDR3_TRCD * nmhz, 1000);
	trcd_tmp = max_t(u32, trcd_tmp, 3);
	dmc->p_ctl_timing->trcd = (trcd_tmp & 0xf);
	dmc->p_phy_timing->dtpr0.b.trcd = trcd_tmp;

	trtp_tmp = DIV_ROUND_UP(LPDDR3_TRTP * nmhz + (nmhz >> 1), 1000);
	trtp_tmp = max_t(u32, trtp_tmp, 4);
	dmc->p_ctl_timing->trtp = trtp_tmp & 0xf;
	dmc->p_phy_timing->dtpr0.b.trtp = trtp_tmp;

	twr_tmp = DIV_ROUND_UP(LPDDR3_TWR * nmhz, 1000);
	twr_tmp = max_t(u32, twr_tmp, 4);
	dmc->p_ctl_timing->twr = twr_tmp & 0x1f;

	bl_tmp = LPDDR3_BL8;
	dmc->p_phy_timing->mr[1] = bl_tmp | LPDDR2_N_WR(twr_tmp);
	dmc->p_noc_timing->b.wr_to_miss = (cwl + twr_tmp + trp_tmp + trcd_tmp);
	dmc->p_noc_timing->b.rd_to_miss =
		(trtp_tmp + trp_tmp + trcd_tmp - (bl >> 1));
	dmc->p_ctl_timing->trc = ((tras_tmp + trp_tmp) & 0x3F);
	dmc->p_noc_timing->b.act_to_act = (tras_tmp + trp_tmp);
	dmc->p_phy_timing->dtpr0.b.trc = (tras_tmp + trp_tmp);
	dmc->p_ctl_timing->trtw = (cl + 2 - cwl);
	dmc->p_phy_timing->dtpr1.b.trtw = 0;
	dmc->p_noc_timing->b.rd_to_wr = (cl + 2 - cwl);
	dmc->p_ctl_timing->tal = al;
	dmc->p_ctl_timing->tcl = cl;
	dmc->p_ctl_timing->tcwl = cwl;

	tmp = DIV_ROUND_UP(LPDDR3_TRRD * nmhz, 1000);
	tmp = max_t(u32, tmp, 2);
	dmc->p_ctl_timing->trrd = (tmp & 0xf);
	dmc->p_phy_timing->dtpr0.b.trrd = tmp;
	dmc->p_noc_activate->b.rrd = tmp;

	tmp = DIV_ROUND_UP(LPDDR3_TWTR * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, 4);
	dmc->p_ctl_timing->twtr = tmp & 0xf;
	dmc->p_phy_timing->dtpr0.b.twtr = tmp;
	dmc->p_noc_timing->b.wr_to_rd = (cwl + tmp);

	tmp = DIV_ROUND_UP(LPDDR3_TXP * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, 3);
	dmc->p_ctl_timing->txp = tmp & 0x7;
	dmc->p_phy_timing->dtpr2.b.txp = tmp;
	dmc->p_ctl_timing->txpdll = LPDDR3_TXPDLL;
	dmc->p_ctl_timing->tzqcs =
		DIV_ROUND_UP(LPDDR3_TZQCS * nmhz, 1000) & 0x7f;
	dmc->p_ctl_timing->tzqcsi = LPDDR3_TZQCSI;
	dmc->p_ctl_timing->tdqs = LPDDR3_TDQS;
	dmc->p_ctl_timing->tcksre = LPDDR3_TCKSRE;
	dmc->p_ctl_timing->tcksrx = LPDDR3_TCKSRX;

	tmp = DIV_ROUND_UP(7 * nmhz + (nmhz >> 1), 1000);
	tmp = max_t(u32, tmp, LPDDR3_TCKE);
	dmc->p_ctl_timing->tcke = tmp;
	dmc->p_phy_timing->dtpr2.b.tcke = tmp;
	dmc->p_ctl_timing->tmod = LPDDR3_TMOD;
	dmc->p_phy_timing->dtpr1.b.tmod = LPDDR3_TMOD;
	dmc->p_ctl_timing->trstl = LPDDR3_TRSTL;
	dmc->p_ctl_timing->tzqcl =
		DIV_ROUND_UP(LPDDR3_TZQCL * nmhz, 1000) & 0x3ff;
	dmc->p_ctl_timing->tmrr = LPDDR3_TMRR;

	tmp = DIV_ROUND_UP(LPDDR3_TCKESR * nmhz, 1000);
	tmp = max_t(u32, tmp, 3);
	dmc->p_ctl_timing->tckesr = tmp & 0xf;
	dmc->p_ctl_timing->tdpd = LPDDR3_TDPD_US;

	dmc->p_phy_timing->dtpr0.b.tccd = 0;
	dmc->p_phy_timing->dtpr1.b.tdqsck_max = LPDDR3_TDQSCK_MAX;
	dmc->p_phy_timing->dtpr1.b.tdqsck = LPDDR3_TDQSCK_MIN;
	/*
	 * tRTODT, 0:ODT may be turned on immediately after
	 *			 read post-amble
	 *		   1:ODT may not be turned on until one clock
	 *			 after the read post-amble
	 */
	dmc->p_phy_timing->dtpr1.b.trtodt = 1;

	tmp = DIV_ROUND_UP(LPDDR3_TFAW * nmhz, 1000);
	tmp = max_t(u32, tmp, 8);
	dmc->p_phy_timing->dtpr1.b.tfaw = tmp;
	dmc->p_noc_activate->b.faw_bank = 1;
	dmc->p_noc_activate->b.faw = tmp;
	dmc->p_phy_timing->dtpr1.b.taond = 0;
	dmc->p_phy_timing->dtpr2.b.tdllk = LPDDR3_TDLLK;
	dmc->p_noc_timing->b.burst_len = (bl >> 1);

	return 0;
}

#define PAUSE_CPU_STACK_SIZE	64

static const u32 cpu_stack_idx[4][4] = {
	{3, 0, 1, 2},
	{2, 3, 0, 1},
	{1, 2, 3, 0},
	{0, 1, 2, 3},
};

static void pause_cpu(void *info)
{
	struct rk3288_dmcclk *dmc = (struct rk3288_dmcclk *)info;
	struct mm_struct *saved_mm = current->active_mm;
	struct rockchip_dmc_sram_params *params = dmc->sram_params;
	void (*fn)(void (*fn) (void *), void *arg, void *sp) =
		(void *)(unsigned long)virt_to_phys(call_with_mmu_disabled);
	u32 cpu = raw_smp_processor_id();
	u32 stack_off = cpu_stack_idx[params->get_major_cpu()][cpu];

	stack_off *= PAUSE_CPU_STACK_SIZE;
	setup_mm_for_reboot();
	fn(params->pause_cpu_in_sram, (void *)cpu,
	   (void *)(dmc->sram_stack_phys - stack_off));
	cpu_switch_mm(saved_mm->pgd, saved_mm);
	local_flush_bp_all();
	local_flush_tlb_all();
}

static void dmc_set_rate(struct rk3288_dmcclk *dmc)
{
	struct rockchip_dmc_sram_params *params = dmc->sram_params;
	struct mm_struct *saved_mm = current->active_mm;
	void (*fn)(void (*fn) (void *), void *arg, void *sp) =
		(void *)(unsigned long)virt_to_phys(call_with_mmu_disabled);

	/*
	 * Read needed values from DRAM before turning off mmu to avoid cache
	 * flush for latency reasons.
	 */
	params->dmc_pre_set_rate(dmc);
	setup_mm_for_reboot();
	fn(params->dmc_set_rate_in_sram, NULL,
	   dmc->sram_stack_phys - (3 * PAUSE_CPU_STACK_SIZE));
	cpu_switch_mm(saved_mm->pgd, saved_mm);
	local_flush_bp_all();
	local_flush_tlb_all();

	/* Read values from sram into ddr after turning on the mmu. */
	params->dmc_post_set_rate(dmc);
}

static int dmc_get_set_rate_time(void)
{
	unsigned int max_cpu_mhz = cpufreq_quick_get_max(0) / 1000;

	/* If over fast enough our time is OK */
	if (max_cpu_mhz >= DMC_TIMEOUT_MHZ)
		return DMC_SET_RATE_TIME_NS;

	/* We'll assume it's linear under the needed speed */
	return DMC_SET_RATE_TIME_NS * DMC_TIMEOUT_MHZ / max_cpu_mhz;
}

static int dmc_set_rate_single_cpu(struct rk3288_dmcclk *dmc)
{
	struct rockchip_dmc_sram_params *params = dmc->sram_params;
	ktime_t timeout, now;
	unsigned int this_cpu;
	unsigned int cpu;
	int ret = 0;

	dmc->training_retries = 0;
	/* Make sure other CPUs aren't processing the previous IPI. */
	kick_all_cpus_sync();
	rockchip_dmc_lock();
	ret = rockchip_dmc_wait(&timeout);
	if (ret) {
		dev_err(dmc->dev, "Failed to determine timeout\n");
		ret = -ETIMEDOUT;
		goto out_locked;
	}

	/*
	 * We need to disable softirqs when pausing the other cpus. We deadlock
	 * without this in this scenario:
	 * Other CPU:		This CPU:
	 * spin_lock_bh(lock)
	 *			smp_call_function()
	 * pause_cpu (no irqs)
	 *			Enter softirq:
	 *			spin_lock(lock)
	 *			Deadlock
	 */
	local_bh_disable();
	now = ktime_get();
	timeout = ktime_sub_ns(timeout, dmc_get_set_rate_time());
	/* This can happen if a irq/softirq delays us long enough. */
	if (ktime_compare(now, timeout) >= 0) {
		dev_dbg(dmc->dev, "timeout before pausing cpus\n");
		ret = -ETIMEDOUT;
		goto out_bh_disabled;
	}

	this_cpu = smp_processor_id();
	params->set_major_cpu(this_cpu);
	params->set_major_cpu_paused(this_cpu, true);
	smp_call_function((smp_call_func_t)pause_cpu, (void *)dmc, 0);

	for_each_online_cpu(cpu) {
		if (cpu == this_cpu)
			continue;
		while (!params->is_cpux_paused(cpu)) {
			now = ktime_get();
			if (ktime_compare(now, timeout) >= 0) {
				dev_dbg(dmc->dev,
					"pause cpu %d timeout\n", cpu);
				params->set_major_cpu_paused(this_cpu, false);
				ret = -ETIMEDOUT;
				goto out_bh_disabled;
			}
			cpu_relax();
		}
	}

	local_irq_disable();
	now = ktime_get();
	if (ktime_compare(now, timeout) >= 0) {
		params->set_major_cpu_paused(this_cpu, false);
		local_irq_enable();
		dev_dbg(dmc->dev, "timeout after pausing cpus\n");
		ret = -ETIMEDOUT;
		goto out_bh_disabled;
	}
	dmc_set_rate(dmc);
	/*
	 * We don't need this to be protected by disabled irqs, but unpausing
	 * the other cpus as soon as possible is worth delaying an irq a little
	 * longer.
	 */
	params->set_major_cpu_paused(this_cpu, false);
	local_irq_enable();
out_bh_disabled:
	local_bh_enable();
out_locked:
	rockchip_dmc_unlock();

	WARN(dmc->training_retries > 0, "data training retries %d times\n",
	     dmc->training_retries);

	return ret;
}

static int dmc_pclk_ptcl_publ_enable(struct rk3288_dmcclk *dmc)
{
	int ret;

	ret = clk_prepare_enable(dmc->pclk_ddrupctl0);
	if (ret < 0) {
		dev_err(dmc->dev, "cannot enable pclk_ddrupctl0 %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(dmc->pclk_publ0);
	if (ret < 0) {
		dev_err(dmc->dev, "cannot enable pclk_publ0 %d\n", ret);
		goto publ0;
	}
	ret = clk_prepare_enable(dmc->pclk_ddrupctl1);
	if (ret < 0) {
		dev_err(dmc->dev, "cannot enable pclk_ddrupctl1 %d\n", ret);
		goto ddrupctl1;
	}
	ret = clk_prepare_enable(dmc->pclk_publ1);
	if (ret < 0) {
		dev_err(dmc->dev, "cannot enable pclk_publ1 %d\n", ret);
		goto publ1;
	}

	return 0;
publ1:
	clk_disable_unprepare(dmc->pclk_ddrupctl1);
ddrupctl1:
	clk_disable_unprepare(dmc->pclk_publ0);
publ0:
	clk_disable_unprepare(dmc->pclk_ddrupctl0);
	return ret;
}

static void dmc_pclk_ptcl_publ_disable(struct rk3288_dmcclk *dmc)
{
	clk_disable_unprepare(dmc->pclk_ddrupctl0);
	clk_disable_unprepare(dmc->pclk_publ0);
	clk_disable_unprepare(dmc->pclk_ddrupctl1);
	clk_disable_unprepare(dmc->pclk_publ1);
}

static int ddr_change_freq(struct rk3288_dmcclk *dmc)
{
	int ret;

	ret = dmc_pclk_ptcl_publ_enable(dmc);
	if (ret < 0)
		return ret;

	/* ddr get parameter */
	switch (dmc->ddr_type) {
	case DDR3:
		ddr3_get_parameter(dmc);
		break;
	case LPDDR2:
		lpddr2_get_parameter(dmc);
		break;
	case LPDDR3:
		lpddr3_get_parameter(dmc);
		break;
	default:
		dev_err(dmc->dev, "cannot get timing parameter for ddr\n");
		dmc_pclk_ptcl_publ_disable(dmc);
		return -EPERM;
	}

	ret = dmc_set_rate_single_cpu(dmc);

	dmc_pclk_ptcl_publ_disable(dmc);
	return ret;
}

#define CLKSEL_DDR_CLK_PLL_SRC_SHIFT	2
#define CLKSEL_DDR_CLK_PLL_SRC_MASK	0x1
#define CLKSEL_DDR_CLK_PLL_DIV_SHIFT	0
#define CLKSEL_DDR_CLK_PLL_DIV_MASK	0x3
#define PLLCON0_OD_MASK			0xf
#define PLLCON0_NR_MASK			0x3f
#define PLLCON0_NR_SHIFT		8
#define PLLCON1_NF_MASK			0x1fff

#define DDR_PCTL_TOGCNT1U		0xc0

/*
 * The register of PMU_SYS_REG2 have memory infomation
 * what was written by coreboot ddr driver.
 * (row/col/bank/bw/cs/channel_number)
 */
#define PMU_SYS_REG2			0x9c

#define MSCH0_DDRCONF			0x08
#define MSCH1_DDRCONF			0x88

#define VCO_MIN_HZ	440000000ULL
#define VCO_MAX_HZ	2200000000ULL

static const struct rockchip_pll_rate_table rk3288_dpll_rates[] = {
	RK3066_PLL_RATE(666000000, 2, 111, 2),
	RK3066_PLL_RATE(533000000, 3, 133, 2), /* Actually 532 */
	RK3066_PLL_RATE(433000000, 1, 72, 4),  /* Actually 432 */
	RK3066_PLL_RATE(400000000, 1, 66, 4),  /* Actually 396 */
	RK3066_PLL_RATE(333000000, 1, 83, 6),  /* Actually 332 */
	RK3066_PLL_RATE(266000000, 1, 88, 8),  /* Actually 264 */
	RK3066_PLL_RATE(200000000, 1, 50, 6),
};

static long rk3288_dmcclk_round_rate(struct rk3288_dmcclk *dmc,
				     unsigned long rate)
{
	const struct rockchip_pll_rate_table *prate;
	u64 vco_rate, clk_rate;
	int i;

	/* Assuming rates are in decreasing order */
	for (i = 0; i < ARRAY_SIZE(rk3288_dpll_rates); i++) {
		prate = &rk3288_dpll_rates[i];
		if (rate >= prate->rate)
			break;
	}

	dmc->clkf = prate->nf;
	dmc->clkod = prate->no;
	dmc->clkr = prate->nr;

	vco_rate = 24000000;
	vco_rate *= (u64)dmc->clkf;
	do_div(vco_rate, dmc->clkr);
	WARN_ON(vco_rate < VCO_MIN_HZ || vco_rate > VCO_MAX_HZ);
	clk_rate = vco_rate;
	do_div(clk_rate, dmc->clkod);

	dev_dbg(dmc->dev,
		"%s(%lu) vco: (24 MHz * %u) / %u = %llu Hz => clk: vco / %u = %llu Hz\n",
		__func__, rate, dmc->clkf, dmc->clkr, vco_rate, dmc->clkod,
		clk_rate);

	return clk_rate;
}

int rk3288_dmcclk_set_rate(unsigned long req_rate)
{
	struct rk3288_dmcclk *dmc = g_dmc;
	unsigned long rate;
	int ret;

	if (!dmc)
		return -ENODEV;

	rate = rk3288_dmcclk_round_rate(dmc, req_rate);
	rate /= 1000000;

	if (dmc->cur_freq == rate)
		return 0;

	dmc->target_freq = rate;

	ret = ddr_change_freq(dmc);
	if (ret) {
		dev_dbg(dmc->dev, "failed to change dmc freq\n");
		return ret;
	}

	dmc->cur_freq = rate;

	return 0;
}

struct stride_info_tag {
	u32 size;
	u32 half_cap;
};

static const struct stride_info_tag stride_info[] = {
	{0x10000000, 0x10000000},	/* 256MB */
	{0x20000000, 0x20000000},	/* 512MB */
	{0x40000000, 0x40000000},	/* 1GB */
	{0x80000000, 0x80000000},	/* 2GB */

	{128, 0x20000000},
	{256, 0x20000000},
	{512, 0x20000000},
	{4096, 0x20000000},

	{128, 0x40000000},
	{256, 0x40000000},
	{512, 0x40000000},
	{4096, 0x40000000},

	{128, 0x80000000},
	{256, 0x80000000},
	{512, 0x80000000},
	{4096, 0x80000000},

	{128, 0x60000000},
	{256, 0x60000000},
	{512, 0x60000000},
	{4096, 0x60000000},

	{0, 0x20000000},
	{0, 0x40000000},
	{0, 0x80000000},
	{0, 0x80000000},	/* 4GB */

	{0, 0},			/*reserved */
	{0, 0},			/*reserved */

	{0, 0},
	{128, 0},
};

static const u16 ddr_cfg_2_rbc[] = {
	/*
	 * [8:7]  bank(n:n bit bank)
	 * [6:4]  row(12+n)
	 * [3:2]  bank(n:n bit bank)
	 * [1:0]  col(9+n)
	 *
	 * all config have (13col, 3bank, 16row, 1cs)
	 */
	DDR_CFG2RBC(3, 3, 0, 2),	/* 0     11   8   15 */
	DDR_CFG2RBC(0, 1, 3, 1),	/* 1     10   8   13 */
	DDR_CFG2RBC(0, 2, 3, 1),	/* 2     10   8   14 */
	DDR_CFG2RBC(0, 3, 3, 1),	/* 3     10   8   15 */
	DDR_CFG2RBC(0, 4, 3, 1),	/* 4     10   8   16 */
	DDR_CFG2RBC(0, 1, 3, 2),	/* 5     11   8   13 */
	DDR_CFG2RBC(0, 2, 3, 2),	/* 6     11   8   14 */
	DDR_CFG2RBC(0, 3, 3, 2),	/* 7     11   8   15 */
	DDR_CFG2RBC(0, 1, 3, 0),	/* 8     9    8   13 */
	DDR_CFG2RBC(0, 2, 3, 0),	/* 9     9    8   14 */
	DDR_CFG2RBC(0, 3, 3, 0),	/* 10    9    8   15 */
	DDR_CFG2RBC(0, 2, 2, 0),	/* 11    9    4   14 */
	DDR_CFG2RBC(0, 1, 2, 1),	/* 12    10   4   13 */
	DDR_CFG2RBC(0, 0, 2, 2),	/* 13    11   4   12 */
	DDR_CFG2RBC(3, 4, 0, 1),	/* 14    10   8   16 */
	DDR_CFG2RBC(0, 4, 3, 2),	/* 15    11   8   16 */
};

#define GET_MEMORY_ROW(reg, ch)		(13+(((reg)>>(6+16*(ch)))&0x3))
#define GET_MEMORY_CS1_ROW(reg, ch)	(13+(((reg)>>(4+16*(ch)))&0x3))
#define GET_MEMORY_COL(reg, ch)		(9+(((reg)>>(9+16*(ch)))&0x3))
#define GET_MEMORY_BANK(reg, ch)	(3-(((reg)>>(8+16*(ch)))&0x1))
#define GET_MEMORY_BW(reg, ch)		(2>>(((reg)>>(2+16*(ch)))&0x3))
#define GET_MEMORY_DIE_BW(reg, ch)	(2>>(((reg)>>(16*(ch)))&0x3))
#define GET_MEMORY_CS_CNT(reg, ch)	((((reg)>>(11+16*(ch)))&0x1)+1)

static void dmc_init_dt_info(struct rk3288_dmcclk *dmc)
{
	u32 ch, sys_reg2, stride, half_cap;

	stride = readl_relaxed(dmc->sgrf + 0x8) & 0x1f;
	dmc->stride = stride_info[stride].size;
	half_cap = stride_info[stride].half_cap;
	dev_dbg(dmc->dev, "stride=%d, size=%d, stride_half_cap=%x\n", stride,
		dmc->stride, half_cap);

	/* get memory infomation */
	sys_reg2 = readl(dmc->pmu + PMU_SYS_REG2);
	for (ch = 0; ch < dmc->channel_num; ch++) {
		dmc->ranks[ch] = GET_MEMORY_CS_CNT(sys_reg2, ch);
		dmc->rank_step[ch] = half_cap / dmc->ranks[ch];
	}
}

static void dmc_init_ddr_info(struct rk3288_dmcclk *dmc, u32 ch)
{
	u32 sys_reg2, tmp, die_cnt, channel_bw, die_bw, cs0_row, cs1_row, col,
	    bank, cs_cnt;

	if (ch)
		dev_dbg(dmc->dev, "Channel b:\n");
	else
		dev_dbg(dmc->dev, "Channel a:\n");

	/* get memory infomation */
	sys_reg2 = readl(dmc->pmu + PMU_SYS_REG2);
	tmp = readl(dmc->phy_regs[ch] + DDR_PHY_DCR) & DDR_PHY_DCR_DDRMD;
	if ((tmp == LPDDR2) &&
	    (((sys_reg2 >> 13) & 0x7) == 6))
		tmp = LPDDR3;
	switch (tmp) {
	case DDR3:
		dev_info(dmc->dev, "DDR3 Device\n");
		break;
	case LPDDR3:
		dev_info(dmc->dev, "LPDDR3 Device\n");
		break;
	case LPDDR2:
		dev_info(dmc->dev, "LPDDR2 Device\n");
		break;
	default:
		dev_info(dmc->dev, "Unkown Device\n");
		tmp = DRAM_MAX;
	}
	dmc->ddr_type = tmp;

	/* get capability per chip, not total size, used for calculate tRFC */
	channel_bw = GET_MEMORY_BW(sys_reg2, ch);
	die_bw = GET_MEMORY_DIE_BW(sys_reg2, ch);
	cs0_row = GET_MEMORY_ROW(sys_reg2, ch);
	cs1_row = GET_MEMORY_CS1_ROW(sys_reg2, ch);
	col = GET_MEMORY_COL(sys_reg2, ch);
	bank = GET_MEMORY_BANK(sys_reg2, ch);
	cs_cnt = GET_MEMORY_CS_CNT(sys_reg2, ch);

	die_cnt = (8 << channel_bw) / (8 << die_bw);
	tmp = (1 << (cs0_row + col + bank + channel_bw));
	if ((tmp / die_cnt) > dmc->ddr_capability_per_die)
		dmc->ddr_capability_per_die = tmp / die_cnt;

	if (cs_cnt > 1)
		tmp += tmp >> (cs0_row - cs1_row);
	if (((sys_reg2 >> (30 + (ch))) & 0x1))
		tmp = tmp * 3 / 4;

	dev_dbg(dmc->dev,
		"Bus Width=%d Col=%d Bank=%d Row=%d CS=%d "
		"Total Capability=%dMB\n",
		channel_bw * 16, col, (0x1 << bank), cs0_row, cs_cnt,
		(tmp >> 20));

	dmc->cur_freq = readl(dmc->ddr_regs[0] + DDR_PCTL_TOGCNT1U);
	dev_dbg(dmc->dev, "freq = %dMHz\n", dmc->cur_freq);
}

#define SRAM_STACK_OFFSET	64

/* SRAM section definitions from the linker */
static int rk3288_sram_init(struct rk3288_dmcclk *dmc)
{
	void *start =
		&_binary_arch_arm_mach_rockchip_embedded_rk3288_sram_bin_start;
	void *end =
		&_binary_arch_arm_mach_rockchip_embedded_rk3288_sram_bin_end;
	u32 size = end - start;

	dmc->sram_stack = dmc->sram + dmc->sram_len - SRAM_STACK_OFFSET;
	dmc->sram_stack_phys = dmc->sram_phys + dmc->sram_len -
		SRAM_STACK_OFFSET;
	flush_cache_all();

	memset(dmc->sram, 0x0, size);
	/* Load the ddr freq sram code and data into the Bus Int Mem. */
	memcpy(dmc->sram, start, size);
	if (0 == memcmp(dmc->sram, start, size)) {
		dev_dbg(dmc->dev,
			"CPU SRAM: checked sram data from %p to %p - %d\n",
			dmc->sram, start, size);
	} else {
		dev_err(dmc->dev,
			"CPU SRAM: failed sram data check from %p to %p - %d\n",
			dmc->sram, start, size);
		return -EPERM;
	}

	/*
	 * We have to flush the write buffer and the prefetch buffer before
	 * executing the code that we just copied into sram.
	 */
	dsb();
	isb();

	/*
	 * We need to init the struct that contains the function pointers for
	 * calling into the sram code. It's position independent code, so we
	 * can't statically init it. It also inits information in sram based on
	 * the values in dmc.
	 */
	dmc->sram_params = ((struct rockchip_dmc_sram_params *(*)
			    (struct rk3288_dmcclk *))(dmc->sram))(dmc);
	dmc->regtiming = dmc->sram_params->dmc_get_regtiming_addr();
	dmc->p_ctl_timing = &dmc->regtiming->ctl_timing;
	dmc->p_phy_timing = &dmc->regtiming->phy_timing;
	dmc->p_noc_timing = &dmc->regtiming->noc_timing;
	dmc->p_noc_activate = &dmc->regtiming->noc_activate;

	return 0;
}

/*
 * Get the physical address and length of iomem from teh device tree and map it
 * in the kernel and sram page table.
 */
static void __iomem *rk3288_getmap_iomem(struct rk3288_dmcclk *dmc,
					 const char *name, void __iomem **phys)
{
	struct device *dev = dmc->dev;
	struct device_node *node;
	struct resource res;
	void __iomem *mem;
	int err;

	node = of_parse_phandle(dmc->dev->of_node, name, 0);
	if (!node) {
		dev_err(dev, "Could not find DTS param for %s\n", name);
		return ERR_PTR(-ENODEV);
	}

	err = of_address_to_resource(node, 0, &res);
	if (err) {
		dev_err(dev, "Error parsing param %s\n", name);
		return ERR_PTR(err);
	}

	mem = devm_ioremap_resource(dev, &res);
	if (IS_ERR(mem))
		dev_err(dev, "Could not map %s\n", name);

	if (phys)
		*phys = (void __iomem *)(unsigned long)res.start;

	return mem;
}

static int rk3288_dmcclk_probe(struct platform_device *pdev)
{
	struct rk3288_dmcclk *dmc;
	struct device_node *node;
	struct resource res;
	int i, ret;
	u32 tmp;

	dmc = devm_kzalloc(&pdev->dev, sizeof(*dmc), GFP_KERNEL);
	if (!dmc) {
		dev_err(dmc->dev, "no memory for state\n");
		return -ENOMEM;
	}

	dmc->dev = &pdev->dev;

	dmc->pclk_ddrupctl0 = devm_clk_get(dmc->dev, "pclk_ddrupctl0");
	if (IS_ERR(dmc->pclk_ddrupctl0)) {
		dev_err(dmc->dev, "cannot get pclk_ddrupctl0\n");
		return PTR_ERR(dmc->pclk_ddrupctl0);
	}
	dmc->pclk_publ0 = devm_clk_get(dmc->dev, "pclk_publ0");
	if (IS_ERR(dmc->pclk_publ0)) {
		dev_err(dmc->dev, "cannot get pclk_publ0\n");
		return PTR_ERR(dmc->pclk_publ0);
	}
	dmc->pclk_ddrupctl1 = devm_clk_get(dmc->dev, "pclk_ddrupctl1");
	if (IS_ERR(dmc->pclk_ddrupctl1)) {
		dev_err(dmc->dev, "cannot get pclk_ddrupctl1\n");
		return PTR_ERR(dmc->pclk_ddrupctl1);
	}
	dmc->pclk_publ1 = devm_clk_get(dmc->dev, "pclk_publ1");
	if (IS_ERR(dmc->pclk_publ1)) {
		dev_err(dmc->dev, "cannot get pclk_publ1\n");
		return PTR_ERR(dmc->pclk_publ1);
	}

	node = of_parse_phandle(pdev->dev.of_node, "rockchip,sram", 0);
	if (!node) {
		dev_err(dmc->dev, "cannot get sram location\n");
		return -ENODEV;
	}
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(dmc->dev, "error parsing param rockchip,sram\n");
		return ret;
	}
	/* We need to map the sram as executable for running code. */
	dmc->sram = __arm_ioremap_exec(res.start, res.end - res.start + 1,
				       false);
	if (!dmc->sram) {
		dev_err(dmc->dev, "Could not map sram\n");
		return PTR_ERR(dmc->sram);
	}
	dmc->sram_len = res.end - res.start + 1;
	dmc->sram_phys = (void __iomem *)(unsigned long)res.start;

	/*
	 * Get cru, grf, sgrf, pmu base regs. We can't use the regmap API when
	 * we turn off DDR, so we're just using iomem right here. Any registers
	 * not owned by ddr clk are unchanged after changing the ddr clk
	 * frequency.
	 */
	dmc->cru = rk3288_getmap_iomem(dmc, "rockchip,cru", &dmc->cru_phys);
	if (IS_ERR(dmc->cru))
		return PTR_ERR(dmc->cru);
	dmc->grf = rk3288_getmap_iomem(dmc, "rockchip,grf", &dmc->grf_phys);
	if (IS_ERR(dmc->grf))
		return PTR_ERR(dmc->grf);
	dmc->pmu = rk3288_getmap_iomem(dmc, "rockchip,pmu", &dmc->pmu_phys);
	if (IS_ERR(dmc->pmu))
		return PTR_ERR(dmc->pmu);
	dmc->sgrf = rk3288_getmap_iomem(dmc, "rockchip,sgrf", NULL);
	if (IS_ERR(dmc->sgrf))
		return PTR_ERR(dmc->sgrf);
	dmc->noc = rk3288_getmap_iomem(dmc, "rockchip,service_bus", &dmc->noc_phys);
	if (IS_ERR(dmc->noc))
		return PTR_ERR(dmc->noc);

	/* get memory controller channel number */
	dmc->channel_num = (1 + ((readl(dmc->pmu + PMU_SYS_REG2) >> 12) & 0x1));
	dmc->ddr_capability_per_die = 0;

	/* get memory controller base regs */
	for (i = 0; i < NUM_MC_CHANNEL_MAX; i++) {
		struct resource *res;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i * 2);
		dmc->ddr_regs[i] = devm_ioremap_resource(dmc->dev, res);
		if (IS_ERR(dmc->ddr_regs[i]))
			return PTR_ERR(dmc->ddr_regs[i]);
		dmc->ddr_regs_phys[i] = (void *)(unsigned long)res->start;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i * 2 + 1);
		dmc->phy_regs[i] = devm_ioremap_resource(dmc->dev, res);
		if (IS_ERR(dmc->phy_regs[i]))
			return PTR_ERR(dmc->phy_regs[i]);
		dmc->phy_regs_phys[i] = (void *)(unsigned long)res->start;
	}

	/* initialize active ddr channels */
	for (i = 0; i < dmc->channel_num; i++)
		dmc_init_ddr_info(dmc, i);

	/* load timings from dts */
	node = pdev->dev.of_node;
	ret |= of_property_read_u32(node, "rockchip,odt-disable-freq", &tmp);
	dmc->oftimings.odt_disable_freq = tmp / 1000000;
	ret |= of_property_read_u32(node, "rockchip,dll-disable-freq", &tmp);
	dmc->oftimings.dll_disable_freq = tmp / 1000000;
	ret |= of_property_read_u32(node, "rockchip,sr-enable-freq", &tmp);
	dmc->oftimings.sr_enable_freq = tmp / 1000000;
	ret |= of_property_read_u32(node, "rockchip,pd-enable-freq", &tmp);
	dmc->oftimings.pd_enable_freq = tmp / 1000000;
	ret |= of_property_read_u32(node, "rockchip,auto-self-refresh-cnt",
				    &tmp);
	dmc->oftimings.sr_cnt = tmp;
	ret |= of_property_read_u32(node, "rockchip,auto-power-down-cnt", &tmp);
	dmc->oftimings.pd_cnt = tmp;
	ret |= of_property_read_u32(node, "rockchip,ddr-speed-bin", &tmp);
	dmc->oftimings.ddr_speed_bin = tmp;
	ret |= of_property_read_u32(node, "rockchip,trcd", &tmp);
	dmc->oftimings.trcd = tmp;
	ret |= of_property_read_u32(node, "rockchip,trp", &tmp);
	dmc->oftimings.trp = tmp;
	if (ret) {
		dev_err(dmc->dev, "load timings from dts failed %d\n", ret);
		return ret;
	}

	/* Find the information needed for Data Training. */
	dmc_init_dt_info(dmc);

	/* Init sram code and page tables using vlaues in dmc struct. */
	ret = rk3288_sram_init(dmc);
	if (ret) {
		dev_err(dmc->dev, "%s: fail to init sram\n", __func__);
		return ret;
	}

	platform_set_drvdata(pdev, dmc);
	g_dmc = dmc;
	platform_device_register_data(dmc->dev, "rk3288-dmc-freq",
				      PLATFORM_DEVID_AUTO, NULL, 0);

	return 0;
}

static __maybe_unused int rk3288_dmcclk_resume(struct device *dev)
{
	struct rk3288_dmcclk *dmc = dev_get_drvdata(dev);
	int ret;

	/*
	 * Need to load sram code for resume since the sram we store the code in
	 * does not have to keep its state across suspend/resume.
	 */
	ret = rk3288_sram_init(dmc);
	if (ret)
		dev_err(dmc->dev, "%s: fail to init sram\n", __func__);

	return ret;
}

static const struct of_device_id rk3288_dmcclk_of_match[] = {
	{ .compatible = "rockchip,rk3288-dmc", },
	{ },
};
MODULE_DEVICE_TABLE(of, rk3288_dmcclk_of_match);

static SIMPLE_DEV_PM_OPS(rk3288_dmcclk_pm_ops, NULL, rk3288_dmcclk_resume);

static struct platform_driver rk3288_dmcclk_driver = {
	.probe = rk3288_dmcclk_probe,
	.driver = {
		.name = "rk3288-dmc",
		.of_match_table = rk3288_dmcclk_of_match,
		.pm = &rk3288_dmcclk_pm_ops,
		.suppress_bind_attrs = true,
	},
};

static int __init rk3288_dmcclk_modinit(void)
{
	int ret;

	ret = platform_driver_register(&rk3288_dmcclk_driver);
	if (ret < 0)
		pr_err("Failed to register platform driver %s\n",
				rk3288_dmcclk_driver.driver.name);

	return ret;
}

module_init(rk3288_dmcclk_modinit);

MODULE_DESCRIPTION("rockchip rk3288 DMC CLK driver");
MODULE_LICENSE("GPL v2");
