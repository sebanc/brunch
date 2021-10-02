// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/thermal.h>

/* svs bank 1-line sw id */
#define SVSB_CPU_LITTLE			BIT(0)
#define SVSB_CPU_BIG			BIT(1)
#define SVSB_CCI			BIT(2)
#define SVSB_GPU			BIT(3)

/* svs bank mode support */
#define SVSB_MODE_ALL_DISABLE		0
#define SVSB_MODE_INIT01		BIT(1)
#define SVSB_MODE_INIT02		BIT(2)
#define SVSB_MODE_MON			BIT(3)

/* svs bank volt flags */
#define SVSB_INIT01_VOLT_IGNORE		BIT(1)
#define SVSB_INIT01_VOLT_INC_ONLY	BIT(2)
#define SVSB_INIT02_RM_DVTFIXED		BIT(8)
#define SVSB_MON_VOLT_IGNORE		BIT(16)

/* svs bank common setting */
#define SVSB_DET_CLK_EN			BIT(31)
#define SVSB_TZONE_HIGH_TEMP_MAX	U32_MAX
#define SVSB_RUNCONFIG_DEFAULT		0x80000000
#define SVSB_DC_SIGNED_BIT		0x8000
#define SVSB_INTEN_INIT0x		0x00005f01
#define SVSB_INTEN_MONVOPEN		0x00ff0000
#define SVSB_EN_OFF			0x0
#define SVSB_EN_MASK			0x7
#define SVSB_EN_INIT01			0x1
#define SVSB_EN_INIT02			0x5
#define SVSB_EN_MON			0x2
#define SVSB_INTSTS_MONVOP		0x00ff0000
#define SVSB_INTSTS_COMPLETE		0x1
#define SVSB_INTSTS_CLEAN		0x00ffffff

#define debug_fops_ro(name)						\
	static int svs_##name##_debug_open(struct inode *inode,		\
					   struct file *filp)		\
	{								\
		return single_open(filp, svs_##name##_debug_show,	\
				   inode->i_private);			\
	}								\
	static const struct file_operations svs_##name##_debug_fops = {	\
		.owner = THIS_MODULE,					\
		.open = svs_##name##_debug_open,			\
		.read = seq_read,					\
		.llseek = seq_lseek,					\
		.release = single_release,				\
	}

#define debug_fops_rw(name)						\
	static int svs_##name##_debug_open(struct inode *inode,		\
					   struct file *filp)		\
	{								\
		return single_open(filp, svs_##name##_debug_show,	\
				   inode->i_private);			\
	}								\
	static const struct file_operations svs_##name##_debug_fops = {	\
		.owner = THIS_MODULE,					\
		.open = svs_##name##_debug_open,			\
		.read = seq_read,					\
		.write = svs_##name##_debug_write,			\
		.llseek = seq_lseek,					\
		.release = single_release,				\
	}

#define svs_dentry(name)	{__stringify(name), &svs_##name##_debug_fops}

static DEFINE_SPINLOCK(mtk_svs_lock);

/*
 * enum svsb_phase - svs bank phase enumeration
 * @SVSB_PHASE_INIT01: basic init for svs bank
 * @SVSB_PHASE_INIT02: svs bank can provide voltages
 * @SVSB_PHASE_MON: svs bank can provide voltages with thermal effect
 * @SVSB_PHASE_ERROR: svs bank encounters unexpected condition
 *
 * Each svs bank has its own independent phase. We enable each svs bank by
 * running their phase orderly. However, When svs bank encounters unexpected
 * condition, it will fire an irq (PHASE_ERROR) to inform svs software.
 *
 * svs bank general phase-enabled order:
 * SVSB_PHASE_INIT01 -> SVSB_PHASE_INIT02 -> SVSB_PHASE_MON
 */
enum svsb_phase {
	SVSB_PHASE_ERROR = 0,
	SVSB_PHASE_INIT01,
	SVSB_PHASE_INIT02,
	SVSB_PHASE_MON,
	SVSB_PHASE_NUM,
};

enum svs_reg_index {
	DESCHAR = 0,
	TEMPCHAR,
	DETCHAR,
	AGECHAR,
	DCCONFIG,
	AGECONFIG,
	FREQPCT30,
	FREQPCT74,
	LIMITVALS,
	VBOOT,
	DETWINDOW,
	CONFIG,
	TSCALCS,
	RUNCONFIG,
	SVSEN,
	INIT2VALS,
	DCVALUES,
	AGEVALUES,
	VOP30,
	VOP74,
	TEMP,
	INTSTS,
	INTSTSRAW,
	INTEN,
	CHKINT,
	CHKSHIFT,
	STATUS,
	VDESIGN30,
	VDESIGN74,
	DVT30,
	DVT74,
	AGECOUNT,
	SMSTATE0,
	SMSTATE1,
	CTL0,
	DESDETSEC,
	TEMPAGESEC,
	CTRLSPARE0,
	CTRLSPARE1,
	CTRLSPARE2,
	CTRLSPARE3,
	CORESEL,
	THERMINTST,
	INTST,
	THSTAGE0ST,
	THSTAGE1ST,
	THSTAGE2ST,
	THAHBST0,
	THAHBST1,
	SPARE0,
	SPARE1,
	SPARE2,
	SPARE3,
	THSLPEVEB,
	SVS_REG_NUM,
};

static const u32 svs_regs_v2[] = {
	[DESCHAR]		= 0xc00,
	[TEMPCHAR]		= 0xc04,
	[DETCHAR]		= 0xc08,
	[AGECHAR]		= 0xc0c,
	[DCCONFIG]		= 0xc10,
	[AGECONFIG]		= 0xc14,
	[FREQPCT30]		= 0xc18,
	[FREQPCT74]		= 0xc1c,
	[LIMITVALS]		= 0xc20,
	[VBOOT]			= 0xc24,
	[DETWINDOW]		= 0xc28,
	[CONFIG]		= 0xc2c,
	[TSCALCS]		= 0xc30,
	[RUNCONFIG]		= 0xc34,
	[SVSEN]			= 0xc38,
	[INIT2VALS]		= 0xc3c,
	[DCVALUES]		= 0xc40,
	[AGEVALUES]		= 0xc44,
	[VOP30]			= 0xc48,
	[VOP74]			= 0xc4c,
	[TEMP]			= 0xc50,
	[INTSTS]		= 0xc54,
	[INTSTSRAW]		= 0xc58,
	[INTEN]			= 0xc5c,
	[CHKINT]		= 0xc60,
	[CHKSHIFT]		= 0xc64,
	[STATUS]		= 0xc68,
	[VDESIGN30]		= 0xc6c,
	[VDESIGN74]		= 0xc70,
	[DVT30]			= 0xc74,
	[DVT74]			= 0xc78,
	[AGECOUNT]		= 0xc7c,
	[SMSTATE0]		= 0xc80,
	[SMSTATE1]		= 0xc84,
	[CTL0]			= 0xc88,
	[DESDETSEC]		= 0xce0,
	[TEMPAGESEC]		= 0xce4,
	[CTRLSPARE0]		= 0xcf0,
	[CTRLSPARE1]		= 0xcf4,
	[CTRLSPARE2]		= 0xcf8,
	[CTRLSPARE3]		= 0xcfc,
	[CORESEL]		= 0xf00,
	[THERMINTST]		= 0xf04,
	[INTST]			= 0xf08,
	[THSTAGE0ST]		= 0xf0c,
	[THSTAGE1ST]		= 0xf10,
	[THSTAGE2ST]		= 0xf14,
	[THAHBST0]		= 0xf18,
	[THAHBST1]		= 0xf1c,
	[SPARE0]		= 0xf20,
	[SPARE1]		= 0xf24,
	[SPARE2]		= 0xf28,
	[SPARE3]		= 0xf2c,
	[THSLPEVEB]		= 0xf30,
};

/*
 * struct thermal_parameter - This is for storing thermal efuse data.
 * We calculate thermal efuse data to produce "mts" and "bts" for
 * svs bank mon mode.
 */
struct thermal_parameter {
	int adc_ge_t;
	int adc_oe_t;
	int ge;
	int oe;
	int gain;
	int o_vtsabb;
	int o_vtsmcu1;
	int o_vtsmcu2;
	int o_vtsmcu3;
	int o_vtsmcu4;
	int o_vtsmcu5;
	int degc_cali;
	int adc_cali_en_t;
	int o_slope;
	int o_slope_sign;
	int ts_id;
};

/*
 * struct svs_platform - svs platform data
 * @dev: svs platform device
 * @base: svs platform register address base
 * @main_clk: main clock for svs bank
 * @pbank: phandle of svs bank and needs to be protected by spin_lock
 * @banks: phandle of the banks that support
 * @efuse_parsing: phandle of efuse parsing function
 * @irqflags: irq settings flags
 * @rst: svs reset control
 * @regs: phandle to the registers map
 * @efuse_num: the total number of svs platform efuse
 * @tefuse_num: the total number of thermal efuse
 * @bank_num: the total number of banks
 * @efuse_check: the svs efuse check index
 * @efuse: svs platform efuse data received from NVMEM framework
 * @tefuse: thermal efuse data received from NVMEM framework
 * @name: svs platform name
 */
struct svs_platform {
	struct device *dev;
	void __iomem *base;
	struct clk *main_clk;
	struct svs_bank *pbank;
	struct svs_bank *banks;
	bool (*efuse_parsing)(struct svs_platform *svsp);
	unsigned long irqflags;
	struct reset_control *rst;
	const u32 *regs;
	char *name;
	size_t efuse_num;
	size_t tefuse_num;
	u32 bank_num;
	u32 efuse_check;
	u32 *efuse;
	u32 *tefuse;
};

/*
 * struct svs_bank - svs bank representation
 * @dev: svs bank device
 * @opp_dev: device for opp table/buck control
 * @pd_dev: power domain device for SoC mtcmos control
 * @init_completion: the timeout completion for bank init
 * @buck: phandle of the regulator
 * @lock: mutex lock to protect voltage update process
 * @phase: bank current phase
 * @name: bank name
 * @tzone_name: thermal zone name
 * @buck_name: regulator name
 * @suspended: suspend flag of this bank
 * @pd_req: bank's power-domain on request
 * @enable_pm_runtime_ever: bank enables pm-runtime flag
 * @set_freqs_pct: phandle of set frequencies percent function
 * @get_vops: phandle of get bank voltages function
 * @volt_offset: bank voltage offset controlled by svs software
 * @mode_support: bank mode support.
 * @opp_freqs: signed-off frequencies from default opp table
 * @opp_volts: signed-off voltages from default opp table
 * @freqs_pct: percent of "opp_freqs / freq_base" for bank init
 * @volts: bank voltages
 * @reg_data: bank register data of each phase
 * @freq_base: reference frequency for bank init
 * @vboot: voltage request for bank init01 stage only
 * @volt_step: bank voltage step
 * @volt_base: bank voltage base
 * @volt_flags: bank voltage flags
 * @vmax: bank voltage maximum
 * @vmin: bank voltage minimum
 * @temp: bank temperature
 * @temp_upper_bound: bank temperature upper bound
 * @temp_lower_bound: bank temperature lower bound
 * @tzone_high_temp: thermal zone high temperature threshold
 * @tzone_high_temp_offset: thermal zone high temperature offset
 * @tzone_low_temp: thermal zone low temperature threshold
 * @tzone_low_temp_offset: thermal zone low temperature offset
 * @core_sel: bank selection
 * @opp_count: bank opp count
 * @int_st: bank interrupt identification
 * @sw_id: bank software identification
 * @hw_id: bank hardware identification
 * @ctl0: bank thermal sensor selection
 * @cpu_id: cpu core id for SVS CPU only
 *
 * Other structure members which are not listed above are svs platform
 * efuse data for bank init
 */
struct svs_bank {
	struct device *dev;
	struct device *opp_dev;
	struct device *pd_dev;
	struct completion init_completion;
	struct regulator *buck;
	struct mutex lock;	/* lock to protect voltage update process */
	enum svsb_phase phase;
	char *name;
	char *tzone_name;
	char *buck_name;
	bool suspended;
	bool pd_req;
	bool enable_pm_runtime_ever;
	void (*set_freqs_pct)(struct svs_platform *svsp);
	void (*get_vops)(struct svs_platform *svsp);
	s32 volt_offset;
	u32 mode_support;
	u32 opp_freqs[16];
	u32 opp_volts[16];
	u32 freqs_pct[16];
	u32 volts[16];
	u32 reg_data[SVSB_PHASE_NUM][SVS_REG_NUM];
	u32 freq_base;
	u32 vboot;
	u32 volt_step;
	u32 volt_base;
	u32 volt_flags;
	u32 vmax;
	u32 vmin;
	u32 bts;
	u32 mts;
	u32 bdes;
	u32 mdes;
	u32 mtdes;
	u32 dcbdet;
	u32 dcmdet;
	u32 dthi;
	u32 dtlo;
	u32 det_window;
	u32 det_max;
	u32 age_config;
	u32 age_voffset_in;
	u32 agem;
	u32 dc_config;
	u32 dc_voffset_in;
	u32 dvt_fixed;
	u32 vco;
	u32 chk_shift;
	u32 temp;
	u32 temp_upper_bound;
	u32 temp_lower_bound;
	u32 tzone_high_temp;
	u32 tzone_high_temp_offset;
	u32 tzone_low_temp;
	u32 tzone_low_temp_offset;
	u32 core_sel;
	u32 opp_count;
	u32 int_st;
	u32 sw_id;
	u32 hw_id;
	u32 ctl0;
	u32 cpu_id;
};

static u32 percent(u32 numerator, u32 denominator)
{
	/* If not divide 1000, "numerator * 100" will have data overflow. */
	numerator /= 1000;
	denominator /= 1000;

	return DIV_ROUND_UP(numerator * 100, denominator);
}

static u32 svs_readl(struct svs_platform *svsp, enum svs_reg_index rg_i)
{
	return readl(svsp->base + svsp->regs[rg_i]);
}

static void svs_writel(struct svs_platform *svsp, u32 val,
		       enum svs_reg_index rg_i)
{
	writel(val, svsp->base + svsp->regs[rg_i]);
}

static void svs_switch_bank(struct svs_platform *svsp)
{
	struct svs_bank *svsb = svsp->pbank;

	svs_writel(svsp, svsb->core_sel, CORESEL);
}

static u32 svs_bank_volt_to_opp_volt(u32 svsb_volt, u32 svsb_volt_step,
				     u32 svsb_volt_base)
{
	return (svsb_volt * svsb_volt_step) + svsb_volt_base;
}

static int svs_get_bank_zone_temperature(const char *tzone_name,
					 int *tzone_temp)
{
	struct thermal_zone_device *tzd;

	tzd = thermal_zone_get_zone_by_name(tzone_name);
	if (IS_ERR(tzd))
		return PTR_ERR(tzd);

	return thermal_zone_get_temp(tzd, tzone_temp);
}

static int svs_adjust_pm_opp_volts(struct svs_bank *svsb, bool force_update)
{
	int tzone_temp = 0, ret = -EPERM;
	u32 i, svsb_volt, opp_volt, temp_offset = 0;

	mutex_lock(&svsb->lock);

	/*
	 * If svs bank is suspended, it means signed-off voltages are applied.
	 * Don't need to update opp voltage anymore.
	 */
	if (svsb->suspended && !force_update) {
		dev_notice(svsb->dev, "bank is suspended\n");
		ret = -EPERM;
		goto unlock_mutex;
	}

	/* Get thermal effect */
	if (svsb->phase == SVSB_PHASE_MON) {
		if (svsb->temp > svsb->temp_upper_bound &&
		    svsb->temp < svsb->temp_lower_bound) {
			dev_warn(svsb->dev, "svsb temp = 0x%x?\n", svsb->temp);
			ret = -EINVAL;
			goto unlock_mutex;
		}

		ret = svs_get_bank_zone_temperature(svsb->tzone_name,
						    &tzone_temp);
		if (ret) {
			dev_err(svsb->dev, "no %s? (%d), run default volts\n",
				svsb->tzone_name, ret);
			svsb->phase = SVSB_PHASE_ERROR;
		}

		if (tzone_temp >= svsb->tzone_high_temp)
			temp_offset += svsb->tzone_high_temp_offset;
		else if (tzone_temp <= svsb->tzone_low_temp)
			temp_offset += svsb->tzone_low_temp_offset;
	}

	/* vmin <= svsb_volt (opp_volt) <= signed-off (default) voltage */
	for (i = 0; i < svsb->opp_count; i++) {
		if (svsb->phase == SVSB_PHASE_MON) {
			svsb_volt = max(svsb->volts[i] + svsb->volt_offset +
					temp_offset, svsb->vmin);
			opp_volt = svs_bank_volt_to_opp_volt(svsb_volt,
							     svsb->volt_step,
							     svsb->volt_base);
		} else if (svsb->phase == SVSB_PHASE_INIT02) {
			svsb_volt = max(svsb->volts[i] + svsb->volt_offset,
					svsb->vmin);
			opp_volt = svs_bank_volt_to_opp_volt(svsb_volt,
							     svsb->volt_step,
							     svsb->volt_base);
		} else if (svsb->phase == SVSB_PHASE_ERROR) {
			opp_volt = svsb->opp_volts[i];
		} else {
			dev_err(svsb->dev, "unknown phase: %u?\n", svsb->phase);
			ret = -EINVAL;
			goto unlock_mutex;
		}

		opp_volt = min(opp_volt, svsb->opp_volts[i]);
		ret = dev_pm_opp_adjust_voltage(svsb->opp_dev,
						svsb->opp_freqs[i],
						opp_volt, opp_volt,
						svsb->opp_volts[i]);
		if (ret) {
			dev_err(svsb->dev, "set voltage fail: %d\n", ret);
			goto unlock_mutex;
		}
	}

unlock_mutex:
	mutex_unlock(&svsb->lock);

	return ret;
}

static u32 interpolate(u32 f0, u32 f1, u32 v0, u32 v1, u32 fx)
{
	u32 vx;

	if (v0 == v1 || f0 == f1)
		return v0;

	/* *100 to have decimal fraction factor */
	vx = (v0 * 100) - ((((v0 - v1) * 100) / (f0 - f1)) * (f0 - fx));

	return DIV_ROUND_UP(vx, 100);
}

static void svs_get_vops_v2(struct svs_platform *svsp)
{
	struct svs_bank *svsb = svsp->pbank;
	u32 temp, i;

	if (svsb->phase == SVSB_PHASE_MON &&
	    svsb->volt_flags & SVSB_MON_VOLT_IGNORE)
		return;

	temp = svs_readl(svsp, VOP74);
	svsb->volts[14] = (temp >> 24) & GENMASK(7, 0);
	svsb->volts[12] = (temp >> 16) & GENMASK(7, 0);
	svsb->volts[10] = (temp >> 8)  & GENMASK(7, 0);
	svsb->volts[8] = (temp & GENMASK(7, 0));

	temp = svs_readl(svsp, VOP30);
	svsb->volts[6] = (temp >> 24) & GENMASK(7, 0);
	svsb->volts[4] = (temp >> 16) & GENMASK(7, 0);
	svsb->volts[2] = (temp >> 8)  & GENMASK(7, 0);
	svsb->volts[0] = (temp & GENMASK(7, 0));

	for (i = 0; i <= 12; i += 2)
		svsb->volts[i + 1] =
			interpolate(svsb->freqs_pct[i],
				    svsb->freqs_pct[i + 2],
				    svsb->volts[i],
				    svsb->volts[i + 2],
				    svsb->freqs_pct[i + 1]);

	svsb->volts[15] =
		interpolate(svsb->freqs_pct[12],
			    svsb->freqs_pct[14],
			    svsb->volts[12],
			    svsb->volts[14],
			    svsb->freqs_pct[15]);

	if (svsb->volt_flags & SVSB_INIT02_RM_DVTFIXED)
		for (i = 0; i < svsb->opp_count; i++)
			svsb->volts[i] -= svsb->dvt_fixed;
}

static void svs_set_freqs_pct_v2(struct svs_platform *svsp)
{
	struct svs_bank *svsb = svsp->pbank;

	svs_writel(svsp,
		   (svsb->freqs_pct[14] << 24) |
		   (svsb->freqs_pct[12] << 16) |
		   (svsb->freqs_pct[10] << 8) |
		   svsb->freqs_pct[8],
		   FREQPCT74);

	svs_writel(svsp,
		   (svsb->freqs_pct[6] << 24) |
		   (svsb->freqs_pct[4] << 16) |
		   (svsb->freqs_pct[2] << 8) |
		   svsb->freqs_pct[0],
		   FREQPCT30);
}

static void svs_set_bank_phase(struct svs_platform *svsp,
			       enum svsb_phase target_phase)
{
	struct svs_bank *svsb = svsp->pbank;
	u32 des_char, temp_char, det_char, limit_vals;
	u32 init2vals, ts_calcs, val, filter, i;

	svs_switch_bank(svsp);

	des_char = (svsb->bdes << 8) | svsb->mdes;
	svs_writel(svsp, des_char, DESCHAR);

	temp_char = (svsb->vco << 16) | (svsb->mtdes << 8) | svsb->dvt_fixed;
	svs_writel(svsp, temp_char, TEMPCHAR);

	det_char = (svsb->dcbdet << 8) | svsb->dcmdet;
	svs_writel(svsp, det_char, DETCHAR);

	svs_writel(svsp, svsb->dc_config, DCCONFIG);
	svs_writel(svsp, svsb->age_config, AGECONFIG);

	if (!svsb->agem) {
		svs_writel(svsp, SVSB_RUNCONFIG_DEFAULT, RUNCONFIG);
	} else {
		val = 0x0;

		for (i = 0; i < 24; i += 2) {
			filter = 0x3 << i;

			if (!(svsb->age_config & filter))
				val |= (0x1 << i);
			else
				val |= (svsb->age_config & filter);
		}
		svs_writel(svsp, val, RUNCONFIG);
	}

	svsb->set_freqs_pct(svsp);

	limit_vals = (svsb->vmax << 24) | (svsb->vmin << 16) |
		     (svsb->dthi << 8) | svsb->dtlo;
	svs_writel(svsp, limit_vals, LIMITVALS);
	svs_writel(svsp, svsb->vboot, VBOOT);
	svs_writel(svsp, svsb->det_window, DETWINDOW);
	svs_writel(svsp, svsb->det_max, CONFIG);

	if (svsb->chk_shift)
		svs_writel(svsp, svsb->chk_shift, CHKSHIFT);

	if (svsb->ctl0)
		svs_writel(svsp, svsb->ctl0, CTL0);

	svs_writel(svsp, SVSB_INTSTS_CLEAN, INTSTS);

	switch (target_phase) {
	case SVSB_PHASE_INIT01:
		svs_writel(svsp, SVSB_INTEN_INIT0x, INTEN);
		svs_writel(svsp, SVSB_EN_INIT01, SVSEN);
		break;
	case SVSB_PHASE_INIT02:
		svs_writel(svsp, SVSB_INTEN_INIT0x, INTEN);
		init2vals = (svsb->age_voffset_in << 16) | svsb->dc_voffset_in;
		svs_writel(svsp, init2vals, INIT2VALS);
		svs_writel(svsp, SVSB_EN_INIT02, SVSEN);
		break;
	case SVSB_PHASE_MON:
		ts_calcs = (svsb->bts << 12) | svsb->mts;
		svs_writel(svsp, ts_calcs, TSCALCS);
		svs_writel(svsp, SVSB_INTEN_MONVOPEN, INTEN);
		svs_writel(svsp, SVSB_EN_MON, SVSEN);
		break;
	default:
		WARN_ON(1);
		break;
	}
}

static inline void svs_init01_isr_handler(struct svs_platform *svsp)
{
	struct svs_bank *svsb = svsp->pbank;
	enum svs_reg_index rg_i;

	dev_info(svsb->dev, "%s: VDN74~30:0x%08x~0x%08x, DC:0x%08x\n",
		 __func__, svs_readl(svsp, VDESIGN74),
		 svs_readl(svsp, VDESIGN30), svs_readl(svsp, DCVALUES));

	for (rg_i = DESCHAR; rg_i < SVS_REG_NUM; rg_i++)
		svsb->reg_data[SVSB_PHASE_INIT01][rg_i] = svs_readl(svsp, rg_i);

	svsb->phase = SVSB_PHASE_INIT01;
	svsb->dc_voffset_in = ~(svs_readl(svsp, DCVALUES) & GENMASK(15, 0)) + 1;
	if (svsb->volt_flags & SVSB_INIT01_VOLT_IGNORE ||
	    (svsb->dc_voffset_in & SVSB_DC_SIGNED_BIT &&
	     svsb->volt_flags & SVSB_INIT01_VOLT_INC_ONLY))
		svsb->dc_voffset_in = 0;

	svsb->age_voffset_in = svs_readl(svsp, AGEVALUES) & GENMASK(15, 0);

	svs_writel(svsp, SVSB_EN_OFF, SVSEN);
	svs_writel(svsp, SVSB_INTSTS_COMPLETE, INTSTS);

	/* svs init01 clock gating */
	svsb->core_sel &= ~SVSB_DET_CLK_EN;
}

static inline void svs_init02_isr_handler(struct svs_platform *svsp)
{
	struct svs_bank *svsb = svsp->pbank;
	enum svs_reg_index rg_i;

	dev_info(svsb->dev, "%s: VOP74~30:0x%08x~0x%08x, DC:0x%08x\n",
		 __func__, svs_readl(svsp, VOP74), svs_readl(svsp, VOP30),
		 svs_readl(svsp, DCVALUES));

	for (rg_i = DESCHAR; rg_i < SVS_REG_NUM; rg_i++)
		svsb->reg_data[SVSB_PHASE_INIT02][rg_i] = svs_readl(svsp, rg_i);

	svsb->phase = SVSB_PHASE_INIT02;
	svsb->get_vops(svsp);

	svs_writel(svsp, SVSB_EN_OFF, SVSEN);
	svs_writel(svsp, SVSB_INTSTS_COMPLETE, INTSTS);
}

static inline void svs_mon_mode_isr_handler(struct svs_platform *svsp)
{
	struct svs_bank *svsb = svsp->pbank;
	enum svs_reg_index rg_i;

	for (rg_i = DESCHAR; rg_i < SVS_REG_NUM; rg_i++)
		svsb->reg_data[SVSB_PHASE_MON][rg_i] = svs_readl(svsp, rg_i);

	svsb->phase = SVSB_PHASE_MON;
	svsb->temp = svs_readl(svsp, TEMP) & GENMASK(7, 0);
	svsb->get_vops(svsp);

	svs_writel(svsp, SVSB_INTSTS_MONVOP, INTSTS);
}

static inline void svs_error_isr_handler(struct svs_platform *svsp)
{
	struct svs_bank *svsb = svsp->pbank;
	enum svs_reg_index rg_i;

	dev_err(svsb->dev, "%s: CORESEL = 0x%08x\n",
		__func__, svs_readl(svsp, CORESEL));
	dev_err(svsb->dev, "SVSEN = 0x%08x, INTSTS = 0x%08x\n",
		svs_readl(svsp, SVSEN), svs_readl(svsp, INTSTS));
	dev_err(svsb->dev, "SMSTATE0 = 0x%08x, SMSTATE1 = 0x%08x\n",
		svs_readl(svsp, SMSTATE0), svs_readl(svsp, SMSTATE1));
	dev_err(svsb->dev, "TEMP = 0x%08x\n", svs_readl(svsp, TEMP));

	for (rg_i = DESCHAR; rg_i < SVS_REG_NUM; rg_i++)
		svsb->reg_data[SVSB_PHASE_ERROR][rg_i] = svs_readl(svsp, rg_i);

	svsb->mode_support = SVSB_MODE_ALL_DISABLE;
	svsb->phase = SVSB_PHASE_ERROR;

	svs_writel(svsp, SVSB_EN_OFF, SVSEN);
	svs_writel(svsp, SVSB_INTSTS_CLEAN, INTSTS);
}

static irqreturn_t svs_isr(int irq, void *data)
{
	struct svs_platform *svsp = data;
	struct svs_bank *svsb = NULL;
	unsigned long flags;
	u32 idx, int_sts, svs_en;

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		WARN_ON(!svsb);

		spin_lock_irqsave(&mtk_svs_lock, flags);
		svsp->pbank = svsb;

		/* Find out which svs bank fires interrupt */
		if (svsb->int_st & svs_readl(svsp, INTST)) {
			spin_unlock_irqrestore(&mtk_svs_lock, flags);
			continue;
		}

		if (!svsb->suspended) {
			svs_switch_bank(svsp);
			int_sts = svs_readl(svsp, INTSTS);
			svs_en = svs_readl(svsp, SVSEN) & SVSB_EN_MASK;

			if (int_sts == SVSB_INTSTS_COMPLETE &&
			    svs_en == SVSB_EN_INIT01)
				svs_init01_isr_handler(svsp);
			else if (int_sts == SVSB_INTSTS_COMPLETE &&
				 svs_en == SVSB_EN_INIT02)
				svs_init02_isr_handler(svsp);
			else if (int_sts & SVSB_INTSTS_MONVOP)
				svs_mon_mode_isr_handler(svsp);
			else
				svs_error_isr_handler(svsp);
		}

		spin_unlock_irqrestore(&mtk_svs_lock, flags);
		break;
	}

	if (svsb->phase != SVSB_PHASE_INIT01)
		svs_adjust_pm_opp_volts(svsb, false);

	if (svsb->phase == SVSB_PHASE_INIT01 ||
	    svsb->phase == SVSB_PHASE_INIT02)
		complete(&svsb->init_completion);

	return IRQ_HANDLED;
}

static void svs_mon_mode(struct svs_platform *svsp)
{
	struct svs_bank *svsb;
	unsigned long flags;
	u32 idx;

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!(svsb->mode_support & SVSB_MODE_MON))
			continue;

		spin_lock_irqsave(&mtk_svs_lock, flags);
		svsp->pbank = svsb;
		svs_set_bank_phase(svsp, SVSB_PHASE_MON);
		spin_unlock_irqrestore(&mtk_svs_lock, flags);
	}
}

static int svs_init02(struct svs_platform *svsp)
{
	struct svs_bank *svsb;
	unsigned long flags, time_left;
	u32 idx;

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!(svsb->mode_support & SVSB_MODE_INIT02))
			continue;

		reinit_completion(&svsb->init_completion);
		spin_lock_irqsave(&mtk_svs_lock, flags);
		svsp->pbank = svsb;
		svs_set_bank_phase(svsp, SVSB_PHASE_INIT02);
		spin_unlock_irqrestore(&mtk_svs_lock, flags);

		time_left =
			wait_for_completion_timeout(&svsb->init_completion,
						    msecs_to_jiffies(5000));
		if (!time_left) {
			dev_err(svsb->dev, "init02 completion timeout\n");
			return -EBUSY;
		}
	}

	return 0;
}

static int svs_init01(struct svs_platform *svsp)
{
	struct svs_bank *svsb;
	struct pm_qos_request *qos_request;
	unsigned long flags, time_left;
	bool search_done;
	int ret = 0;
	u32 opp_freqs, opp_vboot, buck_volt, idx, i;

	qos_request = kzalloc(sizeof(*qos_request), GFP_KERNEL);
	if (!qos_request)
		return -ENOMEM;

	/* Let CPUs leave idle-off state for initializing svs_init01. */
	cpu_latency_qos_add_request(qos_request, 0);

	/*
	 * Sometimes two svs banks use the same buck.
	 * Therefore, we set each svs bank to vboot voltage first.
	 */
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!(svsb->mode_support & SVSB_MODE_INIT01))
			continue;

		search_done = false;

		if (svsb->pd_req) {
			ret = regulator_enable(svsb->buck);
			if (ret) {
				dev_err(svsb->dev, "%s enable fail: %d\n",
					svsb->buck_name, ret);
				goto init01_finish;
			}

			if (!pm_runtime_enabled(svsb->pd_dev)) {
				pm_runtime_enable(svsb->pd_dev);
				svsb->enable_pm_runtime_ever = true;
			}

			ret = pm_runtime_get_sync(svsb->pd_dev);
			if (ret < 0) {
				dev_err(svsb->dev, "mtcmos on fail: %d\n", ret);
				goto init01_finish;
			}
		}

		if (regulator_set_mode(svsb->buck, REGULATOR_MODE_FAST))
			dev_notice(svsb->dev, "set fast mode fail\n");

		/*
		 * Find the fastest freq that can be run at vboot and
		 * fix to that freq until svs_init01 is done.
		 */
		opp_vboot = svs_bank_volt_to_opp_volt(svsb->vboot,
						      svsb->volt_step,
						      svsb->volt_base);

		for (i = 0; i < svsb->opp_count; i++) {
			opp_freqs = svsb->opp_freqs[i];
			if (!search_done && svsb->opp_volts[i] <= opp_vboot) {
				ret = dev_pm_opp_adjust_voltage(svsb->opp_dev,
								opp_freqs,
								opp_vboot,
								opp_vboot,
								opp_vboot);
				if (ret) {
					dev_err(svsb->dev,
						"set voltage fail: %d\n", ret);
					goto init01_finish;
				}

				search_done = true;
			} else {
				dev_pm_opp_disable(svsb->opp_dev,
						   svsb->opp_freqs[i]);
			}
		}
	}

	/* svs bank init01 begins */
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!(svsb->mode_support & SVSB_MODE_INIT01))
			continue;

		opp_vboot = svs_bank_volt_to_opp_volt(svsb->vboot,
						      svsb->volt_step,
						      svsb->volt_base);

		buck_volt = regulator_get_voltage(svsb->buck);
		if (buck_volt != opp_vboot) {
			dev_err(svsb->dev,
				"buck voltage: %u, expected vboot: %u\n",
				buck_volt, opp_vboot);
			ret = -EPERM;
			goto init01_finish;
		}

		spin_lock_irqsave(&mtk_svs_lock, flags);
		svsp->pbank = svsb;
		svs_set_bank_phase(svsp, SVSB_PHASE_INIT01);
		spin_unlock_irqrestore(&mtk_svs_lock, flags);

		time_left =
			wait_for_completion_timeout(&svsb->init_completion,
						    msecs_to_jiffies(5000));
		if (!time_left) {
			dev_err(svsb->dev, "init01 completion timeout\n");
			ret = -EBUSY;
			goto init01_finish;
		}
	}

init01_finish:
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!(svsb->mode_support & SVSB_MODE_INIT01))
			continue;

		for (i = 0; i < svsb->opp_count; i++)
			dev_pm_opp_enable(svsb->opp_dev, svsb->opp_freqs[i]);

		if (regulator_set_mode(svsb->buck, REGULATOR_MODE_NORMAL))
			dev_notice(svsb->dev, "fail to set normal mode\n");

		if (svsb->pd_req) {
			if (pm_runtime_put_sync(svsb->pd_dev))
				dev_err(svsb->dev, "mtcmos off fail\n");

			if (svsb->enable_pm_runtime_ever) {
				pm_runtime_disable(svsb->pd_dev);
				svsb->enable_pm_runtime_ever = false;
			}

			if (regulator_disable(svsb->buck))
				dev_err(svsb->dev, "%s disable fail: %d\n",
					svsb->buck_name, ret);
		}
	}

	cpu_latency_qos_remove_request(qos_request);
	kfree(qos_request);

	return ret;
}

static int svs_start(struct svs_platform *svsp)
{
	int ret;

	ret = svs_init01(svsp);
	if (ret)
		return ret;

	ret = svs_init02(svsp);
	if (ret)
		return ret;

	svs_mon_mode(svsp);

	return 0;
}

static struct device *svs_get_subsys_device(struct svs_platform *svsp,
					    const char *node_name)
{
	struct platform_device *pdev;
	struct device_node *np;

	np = of_find_node_by_name(NULL, node_name);
	if (!np) {
		dev_err(svsp->dev, "cannot find %s node\n", node_name);
		return ERR_PTR(-ENODEV);
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		of_node_put(np);
		dev_err(svsp->dev, "cannot find pdev by %s\n", node_name);
		return ERR_PTR(-ENXIO);
	}

	of_node_put(np);

	return &pdev->dev;
}

static struct device *svs_add_device_link(struct svs_platform *svsp,
					  const char *node_name)
{
	struct device *dev;
	struct device_link *sup_link;

	if (!node_name) {
		dev_err(svsp->dev, "node name cannot be null\n");
		return ERR_PTR(-EINVAL);
	}

	dev = svs_get_subsys_device(svsp, node_name);
	if (IS_ERR(dev))
		return dev;

	sup_link = device_link_add(svsp->dev, dev,
				   DL_FLAG_AUTOREMOVE_CONSUMER);
	if (!sup_link) {
		dev_err(svsp->dev, "sup_link is NULL\n");
		return ERR_PTR(-EINVAL);
	}

	if (sup_link->supplier->links.status != DL_DEV_DRIVER_BOUND)
		return ERR_PTR(-EPROBE_DEFER);

	return dev;
}

static int svs_resource_setup(struct svs_platform *svsp)
{
	struct svs_bank *svsb;
	struct dev_pm_opp *opp;
	unsigned long freq;
	int count, ret;
	u32 idx, i;

	dev_set_drvdata(svsp->dev, svsp);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		switch (svsb->sw_id) {
		case SVSB_CPU_LITTLE:
			svsb->name = "SVSB_CPU_LITTLE";
			break;
		case SVSB_CPU_BIG:
			svsb->name = "SVSB_CPU_BIG";
			break;
		case SVSB_CCI:
			svsb->name = "SVSB_CCI";
			break;
		case SVSB_GPU:
			svsb->name = "SVSB_GPU";
			break;
		default:
			WARN_ON(1);
			return -EINVAL;
		}

		svsb->dev = devm_kzalloc(svsp->dev, sizeof(*svsb->dev),
					 GFP_KERNEL);
		if (!svsb->dev)
			return -ENOMEM;

		ret = dev_set_name(svsb->dev, "%s", svsb->name);
		if (ret)
			return ret;

		dev_set_drvdata(svsb->dev, svsp);

		ret = dev_pm_opp_of_add_table(svsb->opp_dev);
		if (ret) {
			dev_err(svsb->dev, "add opp table fail: %d\n", ret);
			return ret;
		}

		mutex_init(&svsb->lock);
		init_completion(&svsb->init_completion);

		svsb->buck = devm_regulator_get_optional(svsb->opp_dev,
							 svsb->buck_name);
		if (IS_ERR(svsb->buck)) {
			dev_err(svsb->dev, "cannot get \"%s-supply\"\n",
				svsb->buck_name);
			return PTR_ERR(svsb->buck);
		}

		count = dev_pm_opp_get_opp_count(svsb->opp_dev);
		if (svsb->opp_count != count) {
			dev_err(svsb->dev,
				"opp_count not \"%u\" but get \"%d\"?\n",
				svsb->opp_count, count);
			return count;
		}

		for (i = 0, freq = U32_MAX; i < svsb->opp_count; i++, freq--) {
			opp = dev_pm_opp_find_freq_floor(svsb->opp_dev, &freq);
			if (IS_ERR(opp)) {
				dev_err(svsb->dev, "cannot find freq = %ld\n",
					PTR_ERR(opp));
				return PTR_ERR(opp);
			}

			svsb->opp_freqs[i] = freq;
			svsb->opp_volts[i] = dev_pm_opp_get_voltage(opp);
			svsb->freqs_pct[i] = percent(svsb->opp_freqs[i],
						     svsb->freq_base);
			dev_pm_opp_put(opp);
		}
	}

	return 0;
}

static bool svs_mt8183_efuse_parsing(struct svs_platform *svsp)
{
	struct thermal_parameter tp;
	struct svs_bank *svsb;
	bool mon_mode_support = true;
	int format[6], x_roomt[6], tb_roomt = 0;
	struct nvmem_cell *cell;
	u32 idx, i, ft_pgm, mts, temp0, temp1, temp2;

	for (i = 0; i < svsp->efuse_num; i++)
		if (svsp->efuse[i])
			dev_info(svsp->dev, "M_HW_RES%d: 0x%08x\n",
				 i, svsp->efuse[i]);

	/* Svs efuse parsing */
	ft_pgm = (svsp->efuse[0] >> 4) & GENMASK(3, 0);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (ft_pgm <= 1)
			svsb->volt_flags |= SVSB_INIT01_VOLT_IGNORE;

		switch (svsb->sw_id) {
		case SVSB_CPU_LITTLE:
			svsb->bdes = svsp->efuse[16] & GENMASK(7, 0);
			svsb->mdes = (svsp->efuse[16] >> 8) & GENMASK(7, 0);
			svsb->dcbdet = (svsp->efuse[16] >> 16) & GENMASK(7, 0);
			svsb->dcmdet = (svsp->efuse[16] >> 24) & GENMASK(7, 0);
			svsb->mtdes  = (svsp->efuse[17] >> 16) & GENMASK(7, 0);

			if (ft_pgm <= 3)
				svsb->volt_offset += 10;
			else
				svsb->volt_offset += 2;
			break;
		case SVSB_CPU_BIG:
			svsb->bdes = svsp->efuse[18] & GENMASK(7, 0);
			svsb->mdes = (svsp->efuse[18] >> 8) & GENMASK(7, 0);
			svsb->dcbdet = (svsp->efuse[18] >> 16) & GENMASK(7, 0);
			svsb->dcmdet = (svsp->efuse[18] >> 24) & GENMASK(7, 0);
			svsb->mtdes  = svsp->efuse[17] & GENMASK(7, 0);

			if (ft_pgm <= 3)
				svsb->volt_offset += 15;
			else
				svsb->volt_offset += 12;
			break;
		case SVSB_CCI:
			svsb->bdes = svsp->efuse[4] & GENMASK(7, 0);
			svsb->mdes = (svsp->efuse[4] >> 8) & GENMASK(7, 0);
			svsb->dcbdet = (svsp->efuse[4] >> 16) & GENMASK(7, 0);
			svsb->dcmdet = (svsp->efuse[4] >> 24) & GENMASK(7, 0);
			svsb->mtdes  = (svsp->efuse[5] >> 16) & GENMASK(7, 0);

			if (ft_pgm <= 3)
				svsb->volt_offset += 10;
			else
				svsb->volt_offset += 2;
			break;
		case SVSB_GPU:
			svsb->bdes = svsp->efuse[6] & GENMASK(7, 0);
			svsb->mdes = (svsp->efuse[6] >> 8) & GENMASK(7, 0);
			svsb->dcbdet = (svsp->efuse[6] >> 16) & GENMASK(7, 0);
			svsb->dcmdet = (svsp->efuse[6] >> 24) & GENMASK(7, 0);
			svsb->mtdes  = svsp->efuse[5] & GENMASK(7, 0);

			if (ft_pgm >= 2) {
				svsb->freq_base = 800000000; /* 800MHz */
				svsb->dvt_fixed = 2;
			}
			break;
		default:
			break;
		}
	}

	/* Get thermal efuse by nvmem */
	cell = nvmem_cell_get(svsp->dev, "t-calibration-data");
	if (IS_ERR_OR_NULL(cell)) {
		dev_err(svsp->dev, "no thermal cell, no mon mode\n");
		for (idx = 0; idx < svsp->bank_num; idx++) {
			svsb = &svsp->banks[idx];
			svsb->mode_support &= ~SVSB_MODE_MON;
		}

		return true;
	}

	svsp->tefuse = nvmem_cell_read(cell, &svsp->tefuse_num);
	svsp->tefuse_num /= sizeof(u32);
	nvmem_cell_put(cell);

	/* Thermal efuse parsing */
	tp.adc_ge_t = (svsp->tefuse[1] >> 22) & GENMASK(9, 0);
	tp.adc_oe_t = (svsp->tefuse[1] >> 12) & GENMASK(9, 0);

	tp.o_vtsmcu1 = (svsp->tefuse[0] >> 17) & GENMASK(8, 0);
	tp.o_vtsmcu2 = (svsp->tefuse[0] >> 8) & GENMASK(8, 0);
	tp.o_vtsmcu3 = svsp->tefuse[1] & GENMASK(8, 0);
	tp.o_vtsmcu4 = (svsp->tefuse[2] >> 23) & GENMASK(8, 0);
	tp.o_vtsmcu5 = (svsp->tefuse[2] >> 5) & GENMASK(8, 0);
	tp.o_vtsabb = (svsp->tefuse[2] >> 14) & GENMASK(8, 0);

	tp.degc_cali = (svsp->tefuse[0] >> 1) & GENMASK(5, 0);
	tp.adc_cali_en_t = svsp->tefuse[0] & BIT(0);
	tp.o_slope_sign = (svsp->tefuse[0] >> 7) & BIT(0);

	tp.ts_id = (svsp->tefuse[1] >> 9) & BIT(0);
	tp.o_slope = (svsp->tefuse[0] >> 26) & GENMASK(5, 0);

	if (tp.adc_cali_en_t == 1) {
		if (!tp.ts_id)
			tp.o_slope = 0;

		if (tp.adc_ge_t < 265 || tp.adc_ge_t > 758 ||
		    tp.adc_oe_t < 265 || tp.adc_oe_t > 758 ||
		    tp.o_vtsmcu1 < -8 || tp.o_vtsmcu1 > 484 ||
		    tp.o_vtsmcu2 < -8 || tp.o_vtsmcu2 > 484 ||
		    tp.o_vtsmcu3 < -8 || tp.o_vtsmcu3 > 484 ||
		    tp.o_vtsmcu4 < -8 || tp.o_vtsmcu4 > 484 ||
		    tp.o_vtsmcu5 < -8 || tp.o_vtsmcu5 > 484 ||
		    tp.o_vtsabb < -8 || tp.o_vtsabb > 484 ||
		    tp.degc_cali < 1 || tp.degc_cali > 63) {
			dev_err(svsp->dev, "bad thermal efuse, no mon mode\n");
			mon_mode_support = false;
		}
	} else {
		dev_err(svsp->dev, "no thermal efuse, no mon mode\n");
		mon_mode_support = false;
	}

	if (!mon_mode_support) {
		for (idx = 0; idx < svsp->bank_num; idx++) {
			svsb = &svsp->banks[idx];
			svsb->mode_support &= ~SVSB_MODE_MON;
		}

		return true;
	}

	tp.ge = ((tp.adc_ge_t - 512) * 10000) / 4096;
	tp.oe = (tp.adc_oe_t - 512);
	tp.gain = (10000 + tp.ge);

	format[0] = (tp.o_vtsmcu1 + 3350 - tp.oe);
	format[1] = (tp.o_vtsmcu2 + 3350 - tp.oe);
	format[2] = (tp.o_vtsmcu3 + 3350 - tp.oe);
	format[3] = (tp.o_vtsmcu4 + 3350 - tp.oe);
	format[4] = (tp.o_vtsmcu5 + 3350 - tp.oe);
	format[5] = (tp.o_vtsabb + 3350 - tp.oe);

	for (i = 0; i < 6; i++)
		x_roomt[i] = (((format[i] * 10000) / 4096) * 10000) / tp.gain;

	temp0 = (10000 * 100000 / tp.gain) * 15 / 18;

	if (!tp.o_slope_sign)
		mts = (temp0 * 10) / (1534 + tp.o_slope * 10);
	else
		mts = (temp0 * 10) / (1534 - tp.o_slope * 10);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svsb->mts = mts;

		switch (svsb->sw_id) {
		case SVSB_CPU_LITTLE:
			tb_roomt = x_roomt[3];
			break;
		case SVSB_CPU_BIG:
			tb_roomt = x_roomt[4];
			break;
		case SVSB_CCI:
			tb_roomt = x_roomt[3];
			break;
		case SVSB_GPU:
			tb_roomt = x_roomt[1];
			break;
		default:
			break;
		}

		temp0 = (tp.degc_cali * 10 / 2);
		temp1 = ((10000 * 100000 / 4096 / tp.gain) *
			 tp.oe + tb_roomt * 10) * 15 / 18;

		if (!tp.o_slope_sign)
			temp2 = temp1 * 100 / (1534 + tp.o_slope * 10);
		else
			temp2 = temp1 * 100 / (1534 - tp.o_slope * 10);

		svsb->bts = (temp0 + temp2 - 250) * 4 / 10;
	}

	return true;
}

static bool svs_is_supported(struct svs_platform *svsp)
{
	struct nvmem_cell *cell;

	/* Get svs efuse by nvmem */
	cell = nvmem_cell_get(svsp->dev, "svs-calibration-data");
	if (IS_ERR_OR_NULL(cell)) {
		dev_err(svsp->dev,
			"no \"svs-calibration-data\" from dts? disable svs\n");
		return false;
	}

	svsp->efuse = nvmem_cell_read(cell, &svsp->efuse_num);
	svsp->efuse_num /= sizeof(u32);
	nvmem_cell_put(cell);

	if (!svsp->efuse[svsp->efuse_check]) {
		dev_err(svsp->dev, "svs_efuse[%u] = 0x%x?\n",
			svsp->efuse_check, svsp->efuse[svsp->efuse_check]);
		return false;
	}

	return svsp->efuse_parsing(svsp);
}

static int svs_suspend(struct device *dev)
{
	struct svs_platform *svsp = dev_get_drvdata(dev);
	struct svs_bank *svsb;
	unsigned long flags;
	int ret;
	u32 idx;

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		/* Wait if svs_isr() is still in process. */
		spin_lock_irqsave(&mtk_svs_lock, flags);
		svsp->pbank = svsb;
		svs_switch_bank(svsp);
		svs_writel(svsp, SVSB_EN_OFF, SVSEN);
		svs_writel(svsp, SVSB_INTSTS_CLEAN, INTSTS);
		spin_unlock_irqrestore(&mtk_svs_lock, flags);

		svsb->suspended = true;
		if (svsb->phase != SVSB_PHASE_INIT01) {
			svsb->phase = SVSB_PHASE_ERROR;
			svs_adjust_pm_opp_volts(svsb, true);
		}
	}

	if (svsp->rst) {
		ret = reset_control_assert(svsp->rst);
		if (ret) {
			dev_err(svsp->dev, "cannot assert reset %d\n", ret);
			return ret;
		}
	}

	clk_disable_unprepare(svsp->main_clk);

	return 0;
}

static int svs_resume(struct device *dev)
{
	struct svs_platform *svsp = dev_get_drvdata(dev);
	struct svs_bank *svsb;
	int ret;
	u32 idx;

	ret = clk_prepare_enable(svsp->main_clk);
	if (ret) {
		dev_err(svsp->dev, "cannot enable main_clk, disable svs\n");
		return ret;
	}

	if (svsp->rst) {
		ret = reset_control_deassert(svsp->rst);
		if (ret) {
			dev_err(svsp->dev, "cannot deassert reset %d\n", ret);
			return ret;
		}
	}

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svsb->suspended = false;
	}

	ret = svs_init02(svsp);
	if (ret)
		return ret;

	svs_mon_mode(svsp);

	return 0;
}

/*
 * svs_dump_debug_show - dump svs/thermal efuse and svs banks' registers
 */
static int svs_dump_debug_show(struct seq_file *m, void *p)
{
	struct svs_platform *svsp = (struct svs_platform *)m->private;
	struct svs_bank *svsb;
	unsigned long svs_reg_addr;
	u32 idx, i, j;

	for (i = 0; i < svsp->efuse_num; i++)
		if (svsp->efuse && svsp->efuse[i])
			seq_printf(m, "M_HW_RES%d = 0x%08x\n",
				   i, svsp->efuse[i]);

	for (i = 0; i < svsp->tefuse_num; i++)
		if (svsp->tefuse)
			seq_printf(m, "THERMAL_EFUSE%d = 0x%08x\n",
				   i, svsp->tefuse[i]);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		for (i = SVSB_PHASE_INIT01; i <= SVSB_PHASE_MON; i++) {
			seq_printf(m, "Bank_number = %u\n", svsb->hw_id);

			if (i == SVSB_PHASE_INIT01 || i == SVSB_PHASE_INIT02)
				seq_printf(m, "mode = init%d\n", i);
			else if (i == SVSB_PHASE_MON)
				seq_puts(m, "mode = mon\n");
			else
				seq_puts(m, "mode = error\n");

			for (j = DESCHAR; j < SVS_REG_NUM; j++) {
				svs_reg_addr = (unsigned long)(svsp->base +
							       svsp->regs[j]);
				seq_printf(m, "0x%08lx = 0x%08x\n",
					   svs_reg_addr, svsb->reg_data[i][j]);
			}
		}
	}

	return 0;
}

debug_fops_ro(dump);

/*
 * svs_enable_debug_show - show svs bank current enable phase
 */
static int svs_enable_debug_show(struct seq_file *m, void *v)
{
	struct svs_bank *svsb = (struct svs_bank *)m->private;

	if (svsb->phase == SVSB_PHASE_INIT01)
		seq_puts(m, "init1\n");
	else if (svsb->phase == SVSB_PHASE_INIT02)
		seq_puts(m, "init2\n");
	else if (svsb->phase == SVSB_PHASE_MON)
		seq_puts(m, "mon mode\n");
	else if (svsb->phase == SVSB_PHASE_ERROR)
		seq_puts(m, "disabled\n");
	else
		seq_puts(m, "unknown\n");

	return 0;
}

/*
 * svs_enable_debug_write - we only support svs bank disable control
 */
static ssize_t svs_enable_debug_write(struct file *filp,
				      const char __user *buffer,
				      size_t count, loff_t *pos)
{
	struct svs_bank *svsb = file_inode(filp)->i_private;
	struct svs_platform *svsp = dev_get_drvdata(svsb->dev);
	unsigned long flags;
	int enabled, ret;
	char *buf = NULL;

	if (count >= PAGE_SIZE)
		return -EINVAL;

	buf = (char *)memdup_user_nul(buffer, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	ret = kstrtoint(buf, 10, &enabled);
	if (ret)
		return ret;

	if (!enabled) {
		spin_lock_irqsave(&mtk_svs_lock, flags);
		svsp->pbank = svsb;
		svsb->mode_support = SVSB_MODE_ALL_DISABLE;
		svs_switch_bank(svsp);
		svs_writel(svsp, SVSB_EN_OFF, SVSEN);
		svs_writel(svsp, SVSB_INTSTS_CLEAN, INTSTS);
		spin_unlock_irqrestore(&mtk_svs_lock, flags);

		svsb->phase = SVSB_PHASE_ERROR;
		svs_adjust_pm_opp_volts(svsb, true);
	}

	kfree(buf);

	return count;
}

debug_fops_rw(enable);

/*
 * svs_status_debug_show - show svs bank's tzone_temp/voltages/freqs_pct
 * and its corresponding opp-table's opp_freqs/opp_volts
 */
static int svs_status_debug_show(struct seq_file *m, void *v)
{
	struct svs_bank *svsb = (struct svs_bank *)m->private;
	struct dev_pm_opp *opp;
	int tzone_temp = 0, ret;
	u32 i;

	ret = svs_get_bank_zone_temperature(svsb->tzone_name, &tzone_temp);
	if (ret)
		seq_printf(m, "%s: no \"%s\" zone?\n", svsb->name,
			   svsb->tzone_name);
	else
		seq_printf(m, "%s: temperature = %d\n", svsb->name, tzone_temp);

	for (i = 0; i < svsb->opp_count; i++) {
		opp = dev_pm_opp_find_freq_exact(svsb->opp_dev,
						 svsb->opp_freqs[i], true);
		if (IS_ERR(opp)) {
			seq_printf(m, "%s: cannot find freq = %u (%ld)\n",
				   svsb->name, svsb->opp_freqs[i],
				   PTR_ERR(opp));
			return PTR_ERR(opp);
		}

		seq_printf(m, "opp_freqs[%02u]: %u, opp_volts[%02u]: %lu, ",
			   i, svsb->opp_freqs[i], i,
			   dev_pm_opp_get_voltage(opp));
		seq_printf(m, "svsb_volts[%02u]: 0x%x, freqs_pct[%02u]: %u\n",
			   i, svsb->volts[i], i, svsb->freqs_pct[i]);
		dev_pm_opp_put(opp);
	}

	return 0;
}

debug_fops_ro(status);

/*
 * svs_volt_offset_debug_show - show svs bank's voltage offset
 */
static int svs_volt_offset_debug_show(struct seq_file *m, void *v)
{
	struct svs_bank *svsb = (struct svs_bank *)m->private;

	seq_printf(m, "%d\n", svsb->volt_offset);

	return 0;
}

/*
 * svs_volt_offset_debug_write - write svs bank's voltage offset
 */
static ssize_t svs_volt_offset_debug_write(struct file *filp,
					   const char __user *buffer,
					   size_t count, loff_t *pos)
{
	struct svs_bank *svsb = file_inode(filp)->i_private;
	char *buf = NULL;
	s32 volt_offset;

	if (count >= PAGE_SIZE)
		return -EINVAL;

	buf = (char *)memdup_user_nul(buffer, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	if (!kstrtoint(buf, 10, &volt_offset)) {
		svsb->volt_offset = volt_offset;
		svs_adjust_pm_opp_volts(svsb, true);
	}

	kfree(buf);

	return count;
}

debug_fops_rw(volt_offset);

static int svs_create_svs_debug_cmds(struct svs_platform *svsp)
{
	struct svs_bank *svsb;
	struct dentry *svs_dir, *svsb_dir, *file_entry;
	const char *d = "/sys/kernel/debug/svs";
	u32 i, idx;

	struct svs_dentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct svs_dentry svs_entries[] = {
		svs_dentry(dump),
	};

	struct svs_dentry svsb_entries[] = {
		svs_dentry(enable),
		svs_dentry(status),
		svs_dentry(volt_offset),
	};

	svs_dir = debugfs_create_dir("svs", NULL);
	if (IS_ERR(svs_dir)) {
		dev_err(svsp->dev, "cannot create %s: %ld\n",
			d, PTR_ERR(svs_dir));
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(svs_entries); i++) {
		file_entry = debugfs_create_file(svs_entries[i].name, 0664,
						 svs_dir, svsp,
						 svs_entries[i].fops);
		if (IS_ERR(file_entry)) {
			dev_err(svsp->dev, "cannot create %s/%s: %ld\n",
				d, svs_entries[i].name, PTR_ERR(file_entry));
			return PTR_ERR(file_entry);
		}
	}

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		svsb_dir = debugfs_create_dir(svsb->name, svs_dir);
		if (IS_ERR(svsb_dir)) {
			dev_err(svsp->dev, "cannot create %s/%s: %ld\n",
				d, svsb->name, PTR_ERR(svsb_dir));
			return PTR_ERR(svsb_dir);
		}

		for (i = 0; i < ARRAY_SIZE(svsb_entries); i++) {
			file_entry = debugfs_create_file(svsb_entries[i].name,
							 0664, svsb_dir, svsb,
							 svsb_entries[i].fops);
			if (IS_ERR(file_entry)) {
				dev_err(svsp->dev, "no %s/%s/%s?: %ld\n",
					d, svsb->name, svsb_entries[i].name,
					PTR_ERR(file_entry));
				return PTR_ERR(file_entry);
			}
		}
	}

	return 0;
}

static struct svs_bank svs_mt8183_banks[] = {
	{
		.sw_id			= SVSB_CPU_LITTLE,
		.set_freqs_pct		= svs_set_freqs_pct_v2,
		.get_vops		= svs_get_vops_v2,
		.hw_id			= 0,
		.cpu_id			= 0,
		.tzone_name		= "tzts4",
		.buck_name		= "proc",
		.pd_req			= false,
		.volt_flags		= SVSB_INIT01_VOLT_INC_ONLY,
		.mode_support		= SVSB_MODE_INIT01 | SVSB_MODE_INIT02,
		.opp_count		= 16,
		.freq_base		= 1989000000,
		.vboot			= 0x30,
		.volt_step		= 6250,
		.volt_base		= 500000,
		.volt_offset		= 0,
		.vmax			= 0x64,
		.vmin			= 0x18,
		.dthi			= 0x1,
		.dtlo			= 0xfe,
		.det_window		= 0xa28,
		.det_max		= 0xffff,
		.age_config		= 0x555555,
		.agem			= 0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x7,
		.vco			= 0x10,
		.chk_shift		= 0x77,
		.temp_upper_bound	= 0x64,
		.temp_lower_bound	= 0xb2,
		.tzone_high_temp	= SVSB_TZONE_HIGH_TEMP_MAX,
		.tzone_low_temp		= 25000,
		.tzone_low_temp_offset	= 0,
		.core_sel		= 0x8fff0000,
		.int_st			= BIT(0),
		.ctl0			= 0x00010001,
	},
	{
		.sw_id			= SVSB_CPU_BIG,
		.set_freqs_pct		= svs_set_freqs_pct_v2,
		.get_vops		= svs_get_vops_v2,
		.hw_id			= 1,
		.cpu_id			= 4,
		.tzone_name		= "tzts5",
		.buck_name		= "proc",
		.pd_req			= false,
		.volt_flags		= SVSB_INIT01_VOLT_INC_ONLY,
		.mode_support		= SVSB_MODE_INIT01 | SVSB_MODE_INIT02,
		.opp_count		= 16,
		.freq_base		= 1989000000,
		.vboot			= 0x30,
		.volt_step		= 6250,
		.volt_base		= 500000,
		.volt_offset		= 0,
		.vmax			= 0x58,
		.vmin			= 0x10,
		.dthi			= 0x1,
		.dtlo			= 0xfe,
		.det_window		= 0xa28,
		.det_max		= 0xffff,
		.age_config		= 0x555555,
		.agem			= 0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x7,
		.vco			= 0x10,
		.chk_shift		= 0x77,
		.temp_upper_bound	= 0x64,
		.temp_lower_bound	= 0xb2,
		.tzone_high_temp	= SVSB_TZONE_HIGH_TEMP_MAX,
		.tzone_low_temp		= 25000,
		.tzone_low_temp_offset	= 0,
		.core_sel		= 0x8fff0001,
		.int_st			= BIT(1),
		.ctl0			= 0x00000001,
	},
	{
		.sw_id			= SVSB_CCI,
		.set_freqs_pct		= svs_set_freqs_pct_v2,
		.get_vops		= svs_get_vops_v2,
		.hw_id			= 2,
		.tzone_name		= "tzts4",
		.buck_name		= "proc",
		.pd_req			= false,
		.volt_flags		= SVSB_INIT01_VOLT_INC_ONLY,
		.mode_support		= SVSB_MODE_INIT01 | SVSB_MODE_INIT02,
		.opp_count		= 16,
		.freq_base		= 1196000000,
		.vboot			= 0x30,
		.volt_step		= 6250,
		.volt_base		= 500000,
		.volt_offset		= 0,
		.vmax			= 0x64,
		.vmin			= 0x18,
		.dthi			= 0x1,
		.dtlo			= 0xfe,
		.det_window		= 0xa28,
		.det_max		= 0xffff,
		.age_config		= 0x555555,
		.agem			= 0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x7,
		.vco			= 0x10,
		.chk_shift		= 0x77,
		.temp_upper_bound	= 0x64,
		.temp_lower_bound	= 0xb2,
		.tzone_high_temp	= SVSB_TZONE_HIGH_TEMP_MAX,
		.tzone_low_temp		= 25000,
		.tzone_low_temp_offset	= 0,
		.core_sel		= 0x8fff0002,
		.int_st			= BIT(2),
		.ctl0			= 0x00100003,
	},
	{
		.sw_id			= SVSB_GPU,
		.set_freqs_pct		= svs_set_freqs_pct_v2,
		.get_vops		= svs_get_vops_v2,
		.hw_id			= 3,
		.tzone_name		= "tzts2",
		.buck_name		= "mali",
		.pd_req			= true,
		.volt_flags		= SVSB_INIT01_VOLT_INC_ONLY,
		.mode_support		= SVSB_MODE_INIT01 | SVSB_MODE_INIT02 |
					  SVSB_MODE_MON,
		.opp_count		= 16,
		.freq_base		= 900000000,
		.vboot			= 0x30,
		.volt_step		= 6250,
		.volt_base		= 500000,
		.volt_offset		= 0,
		.vmax			= 0x40,
		.vmin			= 0x14,
		.dthi			= 0x1,
		.dtlo			= 0xfe,
		.det_window		= 0xa28,
		.det_max		= 0xffff,
		.age_config		= 0x555555,
		.agem			= 0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x3,
		.vco			= 0x10,
		.chk_shift		= 0x77,
		.temp_upper_bound	= 0x64,
		.temp_lower_bound	= 0xb2,
		.tzone_high_temp	= SVSB_TZONE_HIGH_TEMP_MAX,
		.tzone_low_temp		= 25000,
		.tzone_low_temp_offset	= 3,
		.core_sel		= 0x8fff0003,
		.int_st			= BIT(3),
		.ctl0			= 0x00050001,
	},
};

static int svs_get_svs_mt8183_platform_data(struct svs_platform *svsp)
{
	struct device *dev;
	struct svs_bank *svsb;
	u32 idx;

	svsp->name = "mt8183-svs";
	svsp->banks = svs_mt8183_banks;
	svsp->efuse_parsing = svs_mt8183_efuse_parsing;
	svsp->regs = svs_regs_v2;
	svsp->irqflags = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	svsp->rst = NULL;
	svsp->bank_num = ARRAY_SIZE(svs_mt8183_banks);
	svsp->efuse_check = 2;

	dev = svs_add_device_link(svsp, "thermal");
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		switch (svsb->sw_id) {
		case SVSB_CPU_LITTLE:
		case SVSB_CPU_BIG:
			svsb->opp_dev = get_cpu_device(svsb->cpu_id);
			break;
		case SVSB_CCI:
			svsb->opp_dev = svs_add_device_link(svsp, "cci");
			break;
		case SVSB_GPU:
			svsb->opp_dev = svs_add_device_link(svsp, "mali");
			svsb->pd_dev = svs_add_device_link(svsp,
							   "mali_gpu_core2");
			if (IS_ERR(svsb->pd_dev))
				return PTR_ERR(svsb->pd_dev);
			break;
		default:
			WARN_ON(1);
			return -EINVAL;
		}

		if (IS_ERR(svsb->opp_dev))
			return PTR_ERR(svsb->opp_dev);
	}

	return 0;
}

static const struct of_device_id mtk_svs_of_match[] = {
	{
		.compatible = "mediatek,mt8183-svs",
		.data = &svs_get_svs_mt8183_platform_data,
	}, {
		/* Sentinel */
	},
};

static int svs_probe(struct platform_device *pdev)
{
	int (*svs_get_svs_platform_data)(struct svs_platform *svsp);
	struct svs_platform *svsp;
	unsigned int svsp_irq;
	int ret;

	svsp = devm_kzalloc(&pdev->dev, sizeof(*svsp), GFP_KERNEL);
	if (!svsp)
		return -ENOMEM;

	svs_get_svs_platform_data = of_device_get_match_data(&pdev->dev);
	if (!svs_get_svs_platform_data) {
		dev_err(svsp->dev, "no svs platform data? why?\n");
		return -EPERM;
	}

	svsp->dev = &pdev->dev;
	ret = svs_get_svs_platform_data(svsp);
	if (ret) {
		dev_err_probe(svsp->dev, ret, "fail to get svsp data\n");
		return ret;
	}

	if (!svs_is_supported(svsp)) {
		dev_notice(svsp->dev, "svs is not supported\n");
		return -EPERM;
	}

	ret = svs_resource_setup(svsp);
	if (ret) {
		dev_err(svsp->dev, "svs resource setup fail: %d\n", ret);
		return ret;
	}

	svsp_irq = irq_of_parse_and_map(svsp->dev->of_node, 0);
	ret = devm_request_threaded_irq(svsp->dev, svsp_irq, NULL, svs_isr,
					svsp->irqflags, svsp->name, svsp);
	if (ret) {
		dev_err(svsp->dev, "register irq(%d) failed: %d\n",
			svsp_irq, ret);
		return ret;
	}

	svsp->main_clk = devm_clk_get(svsp->dev, "main");
	if (IS_ERR(svsp->main_clk)) {
		dev_err(svsp->dev, "failed to get clock: %ld\n",
			PTR_ERR(svsp->main_clk));
		return PTR_ERR(svsp->main_clk);
	}

	ret = clk_prepare_enable(svsp->main_clk);
	if (ret) {
		dev_err(svsp->dev, "cannot enable main clk: %d\n", ret);
		return ret;
	}

	svsp->base = of_iomap(svsp->dev->of_node, 0);
	if (IS_ERR_OR_NULL(svsp->base)) {
		dev_err(svsp->dev, "cannot find svs register base\n");
		ret = -EINVAL;
		goto svs_probe_clk_disable;
	}

	ret = svs_start(svsp);
	if (ret) {
		dev_err(svsp->dev, "svs start fail: %d\n", ret);
		goto svs_probe_iounmap;
	}

	ret = svs_create_svs_debug_cmds(svsp);
	if (ret) {
		dev_err(svsp->dev, "svs create debug cmds fail: %d\n", ret);
		goto svs_probe_iounmap;
	}

	return 0;

svs_probe_iounmap:
	iounmap(svsp->base);

svs_probe_clk_disable:
	clk_disable_unprepare(svsp->main_clk);

	return ret;
}

static SIMPLE_DEV_PM_OPS(svs_pm_ops, svs_suspend, svs_resume);

static struct platform_driver svs_driver = {
	.probe	= svs_probe,
	.driver	= {
		.name		= "mtk-svs",
		.pm		= &svs_pm_ops,
		.of_match_table	= of_match_ptr(mtk_svs_of_match),
	},
};

module_platform_driver(svs_driver);

MODULE_AUTHOR("Roger Lu <roger.lu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SVS driver");
MODULE_LICENSE("GPL v2");
