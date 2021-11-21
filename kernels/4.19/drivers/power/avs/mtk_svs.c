// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#define pr_fmt(fmt)	"[mtk_svs] " fmt

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/power/mtk_svs.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/thermal.h>
#include <linux/uaccess.h>

#define SVS_INIT01_VOLT_IGNORE		1
#define SVS_INIT01_VOLT_INC_ONLY	2

#define SVS_PHASE_INIT01		0
#define SVS_PHASE_INIT02		1
#define SVS_PHASE_MON			2
#define SVS_PHASE_ERROR			3

#define SVS_CPU_LITTLE			1
#define SVS_CPU_BIG			2
#define SVS_CCI				3
#define SVS_GPU				4

#define proc_fops_rw(name) \
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
		.write          = name ## _proc_write,			\
	}

#define proc_fops_ro(name) \
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
	}

#define proc_entry(name)	{__stringify(name), &name ## _proc_fops}

static DEFINE_SPINLOCK(mtk_svs_lock);
struct mtk_svs;

enum reg_index {
	TEMPMONCTL0 = 0,
	TEMPMONCTL1,
	TEMPMONCTL2,
	TEMPMONINT,
	TEMPMONINTSTS,
	TEMPMONIDET0,
	TEMPMONIDET1,
	TEMPMONIDET2,
	TEMPH2NTHRE,
	TEMPHTHRE,
	TEMPCTHRE,
	TEMPOFFSETH,
	TEMPOFFSETL,
	TEMPMSRCTL0,
	TEMPMSRCTL1,
	TEMPAHBPOLL,
	TEMPAHBTO,
	TEMPADCPNP0,
	TEMPADCPNP1,
	TEMPADCPNP2,
	TEMPADCMUX,
	TEMPADCEXT,
	TEMPADCEXT1,
	TEMPADCEN,
	TEMPPNPMUXADDR,
	TEMPADCMUXADDR,
	TEMPADCEXTADDR,
	TEMPADCEXT1ADDR,
	TEMPADCENADDR,
	TEMPADCVALIDADDR,
	TEMPADCVOLTADDR,
	TEMPRDCTRL,
	TEMPADCVALIDMASK,
	TEMPADCVOLTAGESHIFT,
	TEMPADCWRITECTRL,
	TEMPMSR0,
	TEMPMSR1,
	TEMPMSR2,
	TEMPADCHADDR,
	TEMPIMMD0,
	TEMPIMMD1,
	TEMPIMMD2,
	TEMPMONIDET3,
	TEMPADCPNP3,
	TEMPMSR3,
	TEMPIMMD3,
	TEMPPROTCTL,
	TEMPPROTTA,
	TEMPPROTTB,
	TEMPPROTTC,
	TEMPSPARE0,
	TEMPSPARE1,
	TEMPSPARE2,
	TEMPSPARE3,
	TEMPMSR0_1,
	TEMPMSR1_1,
	TEMPMSR2_1,
	TEMPMSR3_1,
	DESCHAR,
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
	reg_num,
};

static const u32 svs_regs_v2[] = {
	[TEMPMONCTL0]		= 0x000,
	[TEMPMONCTL1]		= 0x004,
	[TEMPMONCTL2]		= 0x008,
	[TEMPMONINT]		= 0x00c,
	[TEMPMONINTSTS]		= 0x010,
	[TEMPMONIDET0]		= 0x014,
	[TEMPMONIDET1]		= 0x018,
	[TEMPMONIDET2]		= 0x01c,
	[TEMPH2NTHRE]		= 0x024,
	[TEMPHTHRE]		= 0x028,
	[TEMPCTHRE]		= 0x02c,
	[TEMPOFFSETH]		= 0x030,
	[TEMPOFFSETL]		= 0x034,
	[TEMPMSRCTL0]		= 0x038,
	[TEMPMSRCTL1]		= 0x03c,
	[TEMPAHBPOLL]		= 0x040,
	[TEMPAHBTO]		= 0x044,
	[TEMPADCPNP0]		= 0x048,
	[TEMPADCPNP1]		= 0x04c,
	[TEMPADCPNP2]		= 0x050,
	[TEMPADCMUX]		= 0x054,
	[TEMPADCEXT]		= 0x058,
	[TEMPADCEXT1]		= 0x05c,
	[TEMPADCEN]		= 0x060,
	[TEMPPNPMUXADDR]	= 0x064,
	[TEMPADCMUXADDR]	= 0x068,
	[TEMPADCEXTADDR]	= 0x06c,
	[TEMPADCEXT1ADDR]	= 0x070,
	[TEMPADCENADDR]		= 0x074,
	[TEMPADCVALIDADDR]	= 0x078,
	[TEMPADCVOLTADDR]	= 0x07c,
	[TEMPRDCTRL]		= 0x080,
	[TEMPADCVALIDMASK]	= 0x084,
	[TEMPADCVOLTAGESHIFT]	= 0x088,
	[TEMPADCWRITECTRL]	= 0x08c,
	[TEMPMSR0]		= 0x090,
	[TEMPMSR1]		= 0x094,
	[TEMPMSR2]		= 0x098,
	[TEMPADCHADDR]		= 0x09c,
	[TEMPIMMD0]		= 0x0a0,
	[TEMPIMMD1]		= 0x0a4,
	[TEMPIMMD2]		= 0x0a8,
	[TEMPMONIDET3]		= 0x0b0,
	[TEMPADCPNP3]		= 0x0b4,
	[TEMPMSR3]		= 0x0b8,
	[TEMPIMMD3]		= 0x0bc,
	[TEMPPROTCTL]		= 0x0c0,
	[TEMPPROTTA]		= 0x0c4,
	[TEMPPROTTB]		= 0x0c8,
	[TEMPPROTTC]		= 0x0cc,
	[TEMPSPARE0]		= 0x0f0,
	[TEMPSPARE1]		= 0x0f4,
	[TEMPSPARE2]		= 0x0f8,
	[TEMPSPARE3]		= 0x0fc,
	[TEMPMSR0_1]		= 0x190,
	[TEMPMSR1_1]		= 0x194,
	[TEMPMSR2_1]		= 0x198,
	[TEMPMSR3_1]		= 0x1b8,
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

struct svs_bank_ops {
	void (*set_freqs_pct)(struct mtk_svs *svs);
	void (*get_vops)(struct mtk_svs *svs);
};

struct svs_bank {
	struct svs_bank_ops *ops;
	struct completion init_completion;
	struct device *dev;
	struct regulator *buck;
	struct mutex lock;	/* lock to protect update voltage process */
	bool suspended;
	bool mtcmos_request;
	bool init01_support;
	bool init02_support;
	bool mon_mode_support;
	s32 volt_offset;
	u32 *opp_freqs;
	u32 *freqs_pct;
	u32 *opp_volts;
	u32 *init02_volts;
	u32 *volts;
	u32 reg_data[3][reg_num];
	u32 freq_base;
	u32 vboot;
	u32 volt_step;
	u32 volt_base;
	u32 init01_volt_flag;
	u32 phase;
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
	u32 chkshift;
	u32 svs_temp;
	u32 upper_temp_bound;
	u32 lower_temp_bound;
	u32 low_temp_threashold;
	u32 low_temp_offset;
	u32 coresel;
	u32 opp_count;
	u32 intst;
	u32 systemclk_en;
	u32 sw_id;
	u32 hw_id;
	u32 ctl0;
	u8 *of_compatible;
	u8 *name;
	u8 *zone_name;
	u8 *buck_name;
};

struct svs_platform {
	struct svs_bank *banks;
	int (*efuse_parsing)(struct mtk_svs *svs);
	bool fake_efuse;
	const u32 *regs;
	u32 bank_num;
	u32 efuse_num;
	u32 efuse_check;
	u32 thermal_efuse_num;
	u8 *name;
};

struct mtk_svs {
	const struct svs_platform *platform;
	struct svs_bank *bank;
	struct device *dev;
	void __iomem *base;
	struct clk *main_clk;
	u32 *efuse;
	u32 *thermal_efuse;
};

unsigned long claim_mtk_svs_lock(void)
	__acquires(&mtk_svs_lock)
{
	unsigned long flags;

	spin_lock_irqsave(&mtk_svs_lock, flags);

	return flags;
}
EXPORT_SYMBOL_GPL(claim_mtk_svs_lock);

void release_mtk_svs_lock(unsigned long flags)
	__releases(&mtk_svs_lock)
{
	spin_unlock_irqrestore(&mtk_svs_lock, flags);
}
EXPORT_SYMBOL_GPL(release_mtk_svs_lock);

static u32 percent(u32 numerator, u32 denominator)
{
	u32 percent;

	/* If not divide 1000, "numerator * 100" would be data overflow. */
	numerator /= 1000;
	denominator /= 1000;
	percent = ((numerator * 100) + denominator - 1) / denominator;

	return percent;
}

static u32 svs_readl(struct mtk_svs *svs, enum reg_index i)
{
	return readl(svs->base + svs->platform->regs[i]);
}

static void svs_writel(struct mtk_svs *svs, u32 val, enum reg_index i)
{
	writel(val, svs->base + svs->platform->regs[i]);
}

static void svs_switch_bank(struct mtk_svs *svs)
{
	struct svs_bank *svsb = svs->bank;

	svs_writel(svs, svsb->coresel, CORESEL);
}

static u32 svs_volt_to_opp_volt(u32 svsb_volt,
				u32 svsb_volt_step, u32 svsb_volt_base)
{
	u32 u_volt;

	u_volt = (svsb_volt * svsb_volt_step) + svsb_volt_base;

	return u_volt;
}

static int svs_get_zone_temperature(struct svs_bank *svsb, int *zone_temp)
{
	struct thermal_zone_device *tzd;
	int ret;

	tzd = thermal_zone_get_zone_by_name(svsb->zone_name);
	ret = thermal_zone_get_temp(tzd, zone_temp);

	return ret;
}

static int svs_set_volts(struct svs_bank *svsb, bool force_update)
{
	u32 i, svsb_volt, opp_volt, low_temp_offset = 0;
	int zone_temp, ret;

	mutex_lock(&svsb->lock);

	/* If bank is suspended, it means init02 voltage is applied.
	 * Don't need to update opp voltage anymore.
	 */
	if (svsb->suspended && !force_update) {
		pr_notice("%s: bank is suspended\n", svsb->name);
		mutex_unlock(&svsb->lock);
		return -EPERM;
	}

	/* get thermal effect */
	if (svsb->phase == SVS_PHASE_MON) {
		if (svsb->svs_temp > svsb->upper_temp_bound &&
		    svsb->svs_temp < svsb->lower_temp_bound) {
			pr_err("%s: svs_temp is abnormal (0x%x)?\n",
			       svsb->name, svsb->svs_temp);
			mutex_unlock(&svsb->lock);
			return -EINVAL;
		}

		ret = svs_get_zone_temperature(svsb, &zone_temp);
		if (ret) {
			pr_err("%s: cannot get zone \"%s\" temperature\n",
			       svsb->name, svsb->zone_name);
			pr_err("%s: add low_temp_offset = %u\n",
			       svsb->name, svsb->low_temp_offset);
			zone_temp = svsb->low_temp_threashold;
		}

		if (zone_temp <= svsb->low_temp_threashold)
			low_temp_offset = svsb->low_temp_offset;
	}

	/* vmin <= svsb_volt (opp_volt) <= signed-off voltage */
	for (i = 0; i < svsb->opp_count; i++) {
		if (svsb->phase == SVS_PHASE_MON) {
			svsb_volt = max((svsb->volts[i] + svsb->volt_offset +
					 low_temp_offset), svsb->vmin);
			opp_volt = svs_volt_to_opp_volt(svsb_volt,
							svsb->volt_step,
							svsb->volt_base);
		} else if (svsb->phase == SVS_PHASE_INIT02) {
			svsb_volt = max((svsb->init02_volts[i] +
					 svsb->volt_offset), svsb->vmin);
			opp_volt = svs_volt_to_opp_volt(svsb_volt,
							svsb->volt_step,
							svsb->volt_base);
		} else if (svsb->phase == SVS_PHASE_ERROR) {
			opp_volt = svsb->opp_volts[i];
		} else {
			pr_err("%s: unknown phase: %u?\n",
			       svsb->name, svsb->phase);
			mutex_unlock(&svsb->lock);
			return -EINVAL;
		}

		opp_volt = min(opp_volt, svsb->opp_volts[i]);
		ret = dev_pm_opp_adjust_voltage(svsb->dev, svsb->opp_freqs[i],
						opp_volt);
		if (ret) {
			pr_err("%s: set voltage failed: %d\n", svsb->name, ret);
			mutex_unlock(&svsb->lock);
			return ret;
		}
	}

	mutex_unlock(&svsb->lock);

	return 0;
}

static u32 interpolate(u32 f0, u32 f1, u32 v0, u32 v1, u32 fx)
{
	u32 vy;

	if (v0 == v1 || f0 == f1)
		return v0;

	/* *100 to have decimal fraction factor, +99 for rounding up. */
	vy = (v0 * 100) - ((((v0 - v1) * 100) / (f0 - f1)) * (f0 - fx));
	vy = (vy + 99) / 100;

	return vy;
}

static void svs_get_vops_v2(struct mtk_svs *svs)
{
	struct svs_bank *svsb = svs->bank;
	u32 temp, i;

	temp = svs_readl(svs, VOP30);
	svsb->volts[6] = (temp >> 24) & 0xff;
	svsb->volts[4] = (temp >> 16) & 0xff;
	svsb->volts[2] = (temp >> 8)  & 0xff;
	svsb->volts[0] = (temp & 0xff);

	temp = svs_readl(svs, VOP74);
	svsb->volts[14] = (temp >> 24) & 0xff;
	svsb->volts[12] = (temp >> 16) & 0xff;
	svsb->volts[10] = (temp >> 8)  & 0xff;
	svsb->volts[8] = (temp & 0xff);

	for (i = 0; i <= 7; i++) {
		if (i < 7) {
			svsb->volts[(i * 2) + 1] =
				interpolate(svsb->freqs_pct[i * 2],
					    svsb->freqs_pct[(i + 1) * 2],
					    svsb->volts[i * 2],
					    svsb->volts[(i + 1) * 2],
					    svsb->freqs_pct[(i * 2) + 1]);
		} else if (i == 7) {
			svsb->volts[(i * 2) + 1] =
				interpolate(svsb->freqs_pct[(i - 1) * 2],
					    svsb->freqs_pct[i * 2],
					    svsb->volts[(i - 1) * 2],
					    svsb->volts[i * 2],
					    svsb->freqs_pct[(i * 2) + 1]);
		}
	}
}

static void svs_set_freqs_pct_v2(struct mtk_svs *svs)
{
	struct svs_bank *svsb = svs->bank;

	svs_writel(svs,
		   ((svsb->freqs_pct[6] << 24) & 0xff000000) |
		   ((svsb->freqs_pct[4] << 16) & 0xff0000) |
		   ((svsb->freqs_pct[2] << 8) & 0xff00) |
		   (svsb->freqs_pct[0] & 0xff),
		   FREQPCT30);
	svs_writel(svs,
		   ((svsb->freqs_pct[14] << 24) & 0xff000000) |
		   ((svsb->freqs_pct[12] << 16) & 0xff0000) |
		   ((svsb->freqs_pct[10] << 8) & 0xff00) |
		   ((svsb->freqs_pct[8]) & 0xff),
		   FREQPCT74);
}

static void svs_set_phase(struct mtk_svs *svs, u32 target_phase)
{
	struct svs_bank *svsb = svs->bank;
	u32 des_char, temp_char, det_char, limit_vals;
	u32 init2vals, ts_calcs, val, filter, i;

	svs_switch_bank(svs);

	des_char = ((svsb->bdes << 8) & 0xff00) | (svsb->mdes & 0xff);
	svs_writel(svs, des_char, DESCHAR);

	temp_char = ((svsb->vco << 16) & 0xff0000) |
		    ((svsb->mtdes << 8) & 0xff00) |
		    (svsb->dvt_fixed & 0xff);
	svs_writel(svs, temp_char, TEMPCHAR);

	det_char = ((svsb->dcbdet << 8) & 0xff00) | (svsb->dcmdet & 0xff);
	svs_writel(svs, det_char, DETCHAR);

	svs_writel(svs, svsb->dc_config, DCCONFIG);
	svs_writel(svs, svsb->age_config, AGECONFIG);

	if (svsb->agem == 0x0) {
		svs_writel(svs, 0x80000000, RUNCONFIG);
	} else {
		val = 0x0;

		for (i = 0; i < 24; i += 2) {
			filter = 0x3 << i;

			if ((svsb->age_config & filter) == 0x0)
				val |= (0x1 << i);
			else
				val |= (svsb->age_config & filter);
		}
		svs_writel(svs, val, RUNCONFIG);
	}

	svsb->ops->set_freqs_pct(svs);

	limit_vals = ((svsb->vmax << 24) & 0xff000000) |
		     ((svsb->vmin << 16) & 0xff0000) |
		     ((svsb->dthi << 8) & 0xff00) |
		     (svsb->dtlo & 0xff);
	svs_writel(svs, limit_vals, LIMITVALS);
	svs_writel(svs, (svsb->vboot & 0xff), VBOOT);
	svs_writel(svs, (svsb->det_window & 0xffff), DETWINDOW);
	svs_writel(svs, (svsb->det_max & 0xffff), CONFIG);

	if (svsb->chkshift != 0)
		svs_writel(svs, (svsb->chkshift & 0xff), CHKSHIFT);

	if (svsb->ctl0 != 0)
		svs_writel(svs, svsb->ctl0, CTL0);

	svs_writel(svs, 0x00ffffff, INTSTS);

	switch (target_phase) {
	case SVS_PHASE_INIT01:
		svs_writel(svs, 0x00005f01, INTEN);
		svs_writel(svs, 0x00000001, SVSEN);
		break;
	case SVS_PHASE_INIT02:
		svs_writel(svs, 0x00005f01, INTEN);
		init2vals = ((svsb->age_voffset_in << 16) & 0xffff0000) |
			    (svsb->dc_voffset_in & 0xffff);
		svs_writel(svs, init2vals, INIT2VALS);
		svs_writel(svs, 0x00000005, SVSEN);
		break;
	case SVS_PHASE_MON:
		ts_calcs = ((svsb->bts << 12) & 0xfff000) | (svsb->mts & 0xfff);
		svs_writel(svs, ts_calcs, TSCALCS);
		svs_writel(svs, 0x00FF0000, INTEN);
		svs_writel(svs, 0x00000002, SVSEN);
		break;
	default:
		WARN_ON(1);
		break;
	}
}

static inline void svs_init01_isr_handler(struct mtk_svs *svs)
{
	struct svs_bank *svsb = svs->bank;
	enum reg_index rg_i;

	pr_notice("%s: %s: VDN74:0x%08x, VDN30:0x%08x, DCVALUES:0x%08x\n",
		  svsb->name, __func__, svs_readl(svs, VDESIGN74),
		  svs_readl(svs, VDESIGN30), svs_readl(svs, DCVALUES));

	for (rg_i = TEMPMONCTL0; rg_i < reg_num; rg_i++)
		svsb->reg_data[SVS_PHASE_INIT01][rg_i] = svs_readl(svs, rg_i);

	svsb->dc_voffset_in = ~(svs_readl(svs, DCVALUES) & 0xffff) + 1;
	if (svsb->init01_volt_flag == SVS_INIT01_VOLT_IGNORE)
		svsb->dc_voffset_in = 0;
	else if ((svsb->dc_voffset_in & 0x8000) &&
		 (svsb->init01_volt_flag == SVS_INIT01_VOLT_INC_ONLY))
		svsb->dc_voffset_in = 0;

	svsb->age_voffset_in = svs_readl(svs, AGEVALUES) & 0xffff;

	svs_writel(svs, 0x0, SVSEN);
	svs_writel(svs, 0x1, INTSTS);

	/* svs init01 clock gating */
	svsb->coresel &= ~svsb->systemclk_en;

	svsb->phase = SVS_PHASE_INIT01;
	complete(&svsb->init_completion);
}

static inline void svs_init02_isr_handler(struct mtk_svs *svs)
{
	struct svs_bank *svsb = svs->bank;
	enum reg_index rg_i;

	pr_notice("%s: %s: VOP74:0x%08x, VOP30:0x%08x, DCVALUES:0x%08x\n",
		  svsb->name, __func__, svs_readl(svs, VOP74),
		  svs_readl(svs, VOP30), svs_readl(svs, DCVALUES));

	for (rg_i = TEMPMONCTL0; rg_i < reg_num; rg_i++)
		svsb->reg_data[SVS_PHASE_INIT02][rg_i] = svs_readl(svs, rg_i);

	svsb->ops->get_vops(svs);
	memcpy(svsb->init02_volts, svsb->volts, 4 * svsb->opp_count);
	svsb->phase = SVS_PHASE_INIT02;

	svs_writel(svs, 0x0, SVSEN);
	svs_writel(svs, 0x1, INTSTS);

	complete(&svsb->init_completion);
}

static inline void svs_mon_mode_isr_handler(struct mtk_svs *svs)
{
	struct svs_bank *svsb = svs->bank;
	enum reg_index rg_i;

	for (rg_i = TEMPMONCTL0; rg_i < reg_num; rg_i++)
		svsb->reg_data[SVS_PHASE_MON][rg_i] = svs_readl(svs, rg_i);

	svsb->svs_temp = svs_readl(svs, TEMP) & 0xff;

	svsb->ops->get_vops(svs);
	svsb->phase = SVS_PHASE_MON;

	svs_writel(svs, 0x00ff0000, INTSTS);
}

static inline void svs_error_isr_handler(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb = svs->bank;
	enum reg_index rg_i;

	pr_err("%s(): %s(%s)", __func__, svsp->name, svsb->name);
	pr_err("CORESEL(0x%x) = 0x%08x\n",
	       svsp->regs[CORESEL], svs_readl(svs, CORESEL)),
	pr_err("SVSEN(0x%x) = 0x%08x, INTSTS(0x%x) = 0x%08x\n",
	       svsp->regs[SVSEN], svs_readl(svs, SVSEN),
	       svsp->regs[INTSTS], svs_readl(svs, INTSTS));
	pr_err("SMSTATE0(0x%x) = 0x%08x, SMSTATE1(0x%x) = 0x%08x\n",
	       svsp->regs[SMSTATE0], svs_readl(svs, SMSTATE0),
	       svsp->regs[SMSTATE1], svs_readl(svs, SMSTATE1));

	for (rg_i = TEMPMONCTL0; rg_i < reg_num; rg_i++)
		svsb->reg_data[SVS_PHASE_MON][rg_i] = svs_readl(svs, rg_i);

	svsb->init01_support = false;
	svsb->init02_support = false;
	svsb->mon_mode_support = false;

	if (svsb->phase == SVS_PHASE_MON)
		svsb->phase = SVS_PHASE_INIT02;

	svs_writel(svs, 0x0, SVSEN);
	svs_writel(svs, 0x00ffffff, INTSTS);
}

static inline void svs_isr_handler(struct mtk_svs *svs)
{
	u32 intsts, svsen;

	svs_switch_bank(svs);

	intsts = svs_readl(svs, INTSTS);
	svsen = svs_readl(svs, SVSEN);

	if (intsts == 0x1 && ((svsen & 0x7) == 0x1))
		svs_init01_isr_handler(svs);
	else if ((intsts == 0x1) && ((svsen & 0x7) == 0x5))
		svs_init02_isr_handler(svs);
	else if ((intsts & 0x00ff0000) != 0x0)
		svs_mon_mode_isr_handler(svs);
	else
		svs_error_isr_handler(svs);
}

static irqreturn_t svs_isr(int irq, void *data)
{
	struct mtk_svs *svs = (struct mtk_svs *)data;
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb = NULL;
	unsigned long flags;
	u32 idx;

	flags = claim_mtk_svs_lock();
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svs->bank = svsb;

		if (svsb->suspended)
			continue;
		else if (svsb->intst & svs_readl(svs, INTST))
			continue;

		svs_isr_handler(svs);
		break;
	}
	release_mtk_svs_lock(flags);

	if (svsb->phase != SVS_PHASE_INIT01)
		svs_set_volts(svsb, false);

	return IRQ_HANDLED;
}

static void svs_mon_mode(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	unsigned long flags;
	u32 idx;

	flags = claim_mtk_svs_lock();
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svs->bank = svsb;

		if (!svsb->mon_mode_support)
			continue;

		svs_set_phase(svs, SVS_PHASE_MON);
	}
	release_mtk_svs_lock(flags);
}

static int svs_init02(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	unsigned long flags, time_left;
	u32 idx;

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svs->bank = svsb;

		if (!svsb->init02_support)
			continue;

		reinit_completion(&svsb->init_completion);
		flags = claim_mtk_svs_lock();
		svs_set_phase(svs, SVS_PHASE_INIT02);
		release_mtk_svs_lock(flags);
		time_left =
			wait_for_completion_timeout(&svsb->init_completion,
						    msecs_to_jiffies(2000));
		if (time_left == 0) {
			pr_err("%s: init02 completion timeout\n", svsb->name);
			return -EBUSY;
		}
	}

	return 0;
}

static int svs_init01(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	struct pm_qos_request qos_request = { {0} };
	unsigned long flags, time_left;
	bool search_done;
	int ret = -EINVAL;
	u32 opp_freqs, opp_vboot, buck_volt, idx, i;

	/* Let CPUs leave idle-off state for initializing svs_init01. */
	pm_qos_add_request(&qos_request, PM_QOS_CPU_DMA_LATENCY, 0);

	/* Sometimes two svs_bank use the same buck.
	 * Therefore, we set each svs_bank to vboot voltage first.
	 */
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		search_done = false;

		if (!svsb->init01_support)
			continue;

		ret = regulator_set_mode(svsb->buck, REGULATOR_MODE_FAST);
		if (ret)
			pr_notice("%s: fail to set fast mode: %d\n",
				  svsb->name, ret);

		if (svsb->mtcmos_request) {
			ret = regulator_enable(svsb->buck);
			if (ret) {
				pr_err("%s: fail to enable %s power: %d\n",
				       svsb->name, svsb->buck_name, ret);
				goto init01_finish;
			}

			ret = dev_pm_domain_attach(svsb->dev, false);
			if (ret) {
				pr_err("%s: attach pm domain fail: %d\n",
				       svsb->name, ret);
				goto init01_finish;
			}

			pm_runtime_enable(svsb->dev);
			ret = pm_runtime_get_sync(svsb->dev);
			if (ret < 0) {
				pr_err("%s: turn mtcmos on fail: %d\n",
				       svsb->name, ret);
				goto init01_finish;
			}
		}

		/* Find the fastest freq that can be run at vboot and
		 * fix to that freq until svs_init01 is done.
		 */
		opp_vboot = svs_volt_to_opp_volt(svsb->vboot,
						 svsb->volt_step,
						 svsb->volt_base);

		for (i = 0; i < svsb->opp_count; i++) {
			opp_freqs = svsb->opp_freqs[i];
			if (!search_done && svsb->opp_volts[i] <= opp_vboot) {
				ret = dev_pm_opp_adjust_voltage(svsb->dev,
								opp_freqs,
								opp_vboot);
				if (ret) {
					pr_err("%s: set voltage failed: %d\n",
					       svsb->name, ret);
					goto init01_finish;
				}

				search_done = true;
			} else {
				dev_pm_opp_disable(svsb->dev,
						   svsb->opp_freqs[i]);
			}
		}
	}

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svs->bank = svsb;

		if (!svsb->init01_support)
			continue;

		opp_vboot = svs_volt_to_opp_volt(svsb->vboot,
						 svsb->volt_step,
						 svsb->volt_base);

		buck_volt = regulator_get_voltage(svsb->buck);
		if (buck_volt != opp_vboot) {
			pr_err("%s: buck voltage: %u, expected vboot: %u\n",
			       svsb->name, buck_volt, opp_vboot);
			ret = -EPERM;
			goto init01_finish;
		}

		init_completion(&svsb->init_completion);
		flags = claim_mtk_svs_lock();
		svs_set_phase(svs, SVS_PHASE_INIT01);
		release_mtk_svs_lock(flags);
		time_left =
			wait_for_completion_timeout(&svsb->init_completion,
						    msecs_to_jiffies(2000));
		if (time_left == 0) {
			pr_err("%s: init01 completion timeout\n", svsb->name);
			ret = -EBUSY;
			goto init01_finish;
		}
	}

init01_finish:
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!svsb->init01_support)
			continue;

		for (i = 0; i < svsb->opp_count; i++)
			dev_pm_opp_enable(svsb->dev, svsb->opp_freqs[i]);

		if (regulator_set_mode(svsb->buck, REGULATOR_MODE_NORMAL))
			pr_notice("%s: fail to set normal mode: %d\n",
				  svsb->name, ret);

		if (svsb->mtcmos_request) {
			if (pm_runtime_put_sync(svsb->dev))
				pr_err("%s: turn mtcmos off fail: %d\n",
				       svsb->name, ret);
			pm_runtime_disable(svsb->dev);
			dev_pm_domain_detach(svsb->dev, 0);
			if (regulator_disable(svsb->buck))
				pr_err("%s: fail to disable %s power: %d\n",
				       svsb->name, svsb->buck_name, ret);
		}
	}

	pm_qos_remove_request(&qos_request);

	return ret;
}

static int svs_start(struct mtk_svs *svs)
{
	int ret;

	ret = svs_init01(svs);
	if (ret)
		return ret;

	ret = svs_init02(svs);
	if (ret)
		return ret;

	svs_mon_mode(svs);

	return ret;
}

static int svs_mt8183_efuse_parsing(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct thermal_parameter tp;
	struct svs_bank *svsb;
	bool mon_mode_support = true;
	int format[6], x_roomt[6], tb_roomt;
	u32 idx, i, ft_pgm, mts, temp0, temp1, temp2;

	if (svsp->fake_efuse) {
		pr_notice("fake efuse\n");
		svs->efuse[0] = 0x00310080;
		svs->efuse[1] = 0xabfbf757;
		svs->efuse[2] = 0x47c747c7;
		svs->efuse[3] = 0xabfbf757;
		svs->efuse[4] = 0xe7fca0ec;
		svs->efuse[5] = 0x47bf4b88;
		svs->efuse[6] = 0xabfb8fa5;
		svs->efuse[7] = 0xabfb217b;
		svs->efuse[8] = 0x4bf34be1;
		svs->efuse[9] = 0xabfb670d;
		svs->efuse[16] = 0xabfbc653;
		svs->efuse[17] = 0x47f347e1;
		svs->efuse[18] = 0xabfbd848;

		svs->thermal_efuse[0] = 0x02873f69;
		svs->thermal_efuse[1] = 0xa11d9142;
		svs->thermal_efuse[2] = 0xa2526900;
	}

	/* svs efuse parsing */
	ft_pgm = (svs->efuse[0] >> 4) & 0xf;

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		if (ft_pgm <= 1)
			svsb->init01_volt_flag = SVS_INIT01_VOLT_IGNORE;

		switch (svsb->sw_id) {
		case SVS_CPU_LITTLE:
			svsb->bdes = svs->efuse[16] & 0xff;
			svsb->mdes = (svs->efuse[16] >> 8) & 0xff;
			svsb->dcbdet = (svs->efuse[16] >> 16) & 0xff;
			svsb->dcmdet = (svs->efuse[16] >> 24) & 0xff;
			svsb->mtdes  = (svs->efuse[17] >> 16) & 0xff;

			if (ft_pgm <= 3)
				svsb->volt_offset += 10;
			else
				svsb->volt_offset += 2;
			break;
		case SVS_CPU_BIG:
			svsb->bdes = svs->efuse[18] & 0xff;
			svsb->mdes = (svs->efuse[18] >> 8) & 0xff;
			svsb->dcbdet = (svs->efuse[18] >> 16) & 0xff;
			svsb->dcmdet = (svs->efuse[18] >> 24) & 0xff;
			svsb->mtdes  = svs->efuse[17] & 0xff;

			if (ft_pgm <= 3)
				svsb->volt_offset += 15;
			else
				svsb->volt_offset += 12;
			break;
		case SVS_CCI:
			svsb->bdes = svs->efuse[4] & 0xff;
			svsb->mdes = (svs->efuse[4] >> 8) & 0xff;
			svsb->dcbdet = (svs->efuse[4] >> 16) & 0xff;
			svsb->dcmdet = (svs->efuse[4] >> 24) & 0xff;
			svsb->mtdes  = (svs->efuse[5] >> 16) & 0xff;

			if (ft_pgm <= 3)
				svsb->volt_offset += 10;
			else
				svsb->volt_offset += 2;
			break;
		case SVS_GPU:
			svsb->bdes = svs->efuse[6] & 0xff;
			svsb->mdes = (svs->efuse[6] >> 8) & 0xff;
			svsb->dcbdet = (svs->efuse[6] >> 16) & 0xff;
			svsb->dcmdet = (svs->efuse[6] >> 24) & 0xff;
			svsb->mtdes  = svs->efuse[5] & 0xff;

			if (ft_pgm >= 2) {
				svsb->freq_base = 800000000; /* 800MHz */
				svsb->dvt_fixed = 2;
			}
			break;
		default:
			break;
		}
	}

	for (i = 0; i < svsp->efuse_num; i++) {
		if (svs->efuse[i])
			pr_notice("M_HW_RES%d: 0x%08x\n", i, svs->efuse[i]);
	}

	/* thermal efuse parsing */
	if (!svs->thermal_efuse)
		return 0;

	tp.adc_ge_t = (svs->thermal_efuse[1] >> 22) & 0x3ff;
	tp.adc_oe_t = (svs->thermal_efuse[1] >> 12) & 0x3ff;

	tp.o_vtsmcu1 = (svs->thermal_efuse[0] >> 17) & 0x1ff;
	tp.o_vtsmcu2 = (svs->thermal_efuse[0] >> 8) & 0x1ff;
	tp.o_vtsmcu3 = svs->thermal_efuse[1] & 0x1ff;
	tp.o_vtsmcu4 = (svs->thermal_efuse[2] >> 23) & 0x1ff;
	tp.o_vtsmcu5 = (svs->thermal_efuse[2] >> 5) & 0x1ff;
	tp.o_vtsabb = (svs->thermal_efuse[2] >> 14) & 0x1ff;

	tp.degc_cali = (svs->thermal_efuse[0] >> 1) & 0x3f;
	tp.adc_cali_en_t = svs->thermal_efuse[0] & BIT(0);
	tp.o_slope_sign = (svs->thermal_efuse[0] >> 7) & BIT(0);

	tp.ts_id = (svs->thermal_efuse[1] >> 9) & BIT(0);
	tp.o_slope = (svs->thermal_efuse[0] >> 26) & 0x3f;

	if (tp.adc_cali_en_t == 1) {
		if (tp.ts_id == 0)
			tp.o_slope = 0;

		if ((tp.adc_ge_t < 265 || tp.adc_ge_t > 758) ||
		    (tp.adc_oe_t < 265 || tp.adc_oe_t > 758) ||
		    (tp.o_vtsmcu1 < -8 || tp.o_vtsmcu1 > 484) ||
		    (tp.o_vtsmcu2 < -8 || tp.o_vtsmcu2 > 484) ||
		    (tp.o_vtsmcu3 < -8 || tp.o_vtsmcu3 > 484) ||
		    (tp.o_vtsmcu4 < -8 || tp.o_vtsmcu4 > 484) ||
		    (tp.o_vtsmcu5 < -8 || tp.o_vtsmcu5 > 484) ||
		    (tp.o_vtsabb < -8 || tp.o_vtsabb > 484) ||
		    (tp.degc_cali < 1 || tp.degc_cali > 63)) {
			pr_err("bad thermal efuse data. disable mon mode\n");
			mon_mode_support = false;
		}
	} else {
		pr_err("no thermal efuse data. disable mon mode\n");
		mon_mode_support = false;
	}

	if (!mon_mode_support) {
		for (i = 0; i < svsp->thermal_efuse_num; i++)
			pr_err("thermal_efuse[%u] = 0x%08x\n",
			       i, svs->thermal_efuse[i]);

		for (idx = 0; idx < svsp->bank_num; idx++) {
			svsb = &svsp->banks[idx];
			svsb->mon_mode_support = false;
		}

		return 0;
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

	if (tp.o_slope_sign == 0)
		mts = (temp0 * 10) / (1534 + tp.o_slope * 10);
	else
		mts = (temp0 * 10) / (1534 - tp.o_slope * 10);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svsb->mts = mts;

		switch (svsb->sw_id) {
		case SVS_CPU_LITTLE:
			tb_roomt = x_roomt[3];
			break;
		case SVS_CPU_BIG:
			tb_roomt = x_roomt[4];
			break;
		case SVS_CCI:
			tb_roomt = x_roomt[3];
			break;
		case SVS_GPU:
			tb_roomt = x_roomt[1];
			break;
		default:
			pr_err("unknown svsb_id = %u? disable svs\n",
			       svsb->sw_id);
			return -EINVAL;
		}

		temp0 = (tp.degc_cali * 10 / 2);
		temp1 = ((10000 * 100000 / 4096 / tp.gain) *
			 tp.oe + tb_roomt * 10) * 15 / 18;

		if (tp.o_slope_sign == 0)
			temp2 = temp1 * 100 / (1534 + tp.o_slope * 10);
		else
			temp2 = temp1 * 100 / (1534 - tp.o_slope * 10);

		svsb->bts = (temp0 + temp2 - 250) * 4 / 10;
	}

	return 0;
}

static int svs_is_support(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	struct nvmem_cell *cell;
	size_t len;
	int ret;
	u32 idx, i;

	if (svsp->fake_efuse) {
		len = svsp->efuse_num * 4;
		svs->efuse = kzalloc(len, GFP_KERNEL);
		if (!svs->efuse)
			return -ENOMEM;

		len = svsp->thermal_efuse_num * 4;
		svs->thermal_efuse = kzalloc(len, GFP_KERNEL);
		if (!svs->thermal_efuse)
			return -ENOMEM;

		goto svsp_efuse_parsing;
	}

	/* get svs efuse by nvmem */
	cell = nvmem_cell_get(svs->dev, "svs-calibration-data");
	if (IS_ERR(cell)) {
		pr_err("no \"svs-calibration-data\" from dts? disable svs\n");
		return PTR_ERR(cell);
	}

	svs->efuse = (u32 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	ret = (svs->efuse[svsp->efuse_check] == 0) ? -EPERM : 0;
	if (ret) {
		pr_err("no svs efuse. disable svs.\n");
		for (i = 0; i < svsp->efuse_num; i++)
			pr_err("M_HW_RES%d: 0x%08x\n", i, svs->efuse[i]);
		return ret;
	}

	/* get thermal efuse by nvmem */
	cell = nvmem_cell_get(svs->dev, "calibration-data");
	if (IS_ERR(cell)) {
		pr_err("no \"calibration-data\" from dts? disable mon mode\n");
		svs->thermal_efuse = NULL;
		for (idx = 0; idx < svsp->bank_num; idx++) {
			svsb = &svsp->banks[idx];
			svsb->mon_mode_support = false;
		}
		goto svsp_efuse_parsing;
	}

	svs->thermal_efuse = (u32 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

svsp_efuse_parsing:
	ret = svsp->efuse_parsing(svs);

	return ret;
}

static int svs_resource_setup(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	struct platform_device *pdev;
	struct device_node *np = NULL;
	struct dev_pm_opp *opp;
	unsigned long freq;
	size_t opp_size;
	int count, ret;
	u32 idx, i;

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!svsb->init01_support)
			continue;

		switch (svsb->sw_id) {
		case SVS_CPU_LITTLE:
			svsb->name = "SVS_CPU_LITTLE";
			break;
		case SVS_CPU_BIG:
			svsb->name = "SVS_CPU_BIG";
			break;
		case SVS_CCI:
			svsb->name = "SVS_CCI";
			break;
		case SVS_GPU:
			svsb->name = "SVS_GPU";
			break;
		default:
			WARN_ON(1);
			return -EINVAL;
		}

		/* Add svs_bank device for opp-table/mtcmos/buck control */
		pdev = platform_device_alloc(svsb->name, 0);
		if (!pdev) {
			pr_err("%s: fail to alloc pdev for svs_bank\n",
			       svsb->name);
			return -ENOMEM;
		}

		for_each_child_of_node(svs->dev->of_node, np) {
			if (of_device_is_compatible(np, svsb->of_compatible)) {
				pdev->dev.of_node = np;
				break;
			}
		}

		ret = platform_device_add(pdev);
		if (ret) {
			pr_err("%s: fail to add svs_bank device: %d\n",
			       svsb->name, ret);
			return ret;
		}

		svsb->dev = &pdev->dev;
		dev_set_drvdata(svsb->dev, svs);
		ret = dev_pm_opp_of_add_table(svsb->dev);
		if (ret) {
			pr_err("%s: fail to add opp table: %d\n",
			       svsb->name, ret);
			return ret;
		}

		mutex_init(&svsb->lock);

		svsb->buck = devm_regulator_get_optional(svsb->dev,
							 svsb->buck_name);
		if (IS_ERR(svsb->buck)) {
			pr_err("%s: cannot get regulator \"%s-supply\"\n",
			       svsb->name, svsb->buck_name);
			return PTR_ERR(svsb->buck);
		}

		count = dev_pm_opp_get_opp_count(svsb->dev);
		if (svsb->opp_count != count) {
			pr_err("%s: opp_count not \"%u\" but get \"%d\"?\n",
			       svsb->name, svsb->opp_count, count);
			return count;
		}

		opp_size = 4 * svsb->opp_count;
		svsb->opp_volts = kmalloc(opp_size, GFP_KERNEL);
		if (!svsb->opp_volts)
			return -ENOMEM;

		svsb->init02_volts = kmalloc(opp_size, GFP_KERNEL);
		if (!svsb->init02_volts)
			return -ENOMEM;

		svsb->volts = kmalloc(opp_size, GFP_KERNEL);
		if (!svsb->volts)
			return -ENOMEM;

		svsb->opp_freqs = kmalloc(opp_size, GFP_KERNEL);
		if (!svsb->opp_freqs)
			return -ENOMEM;

		svsb->freqs_pct = kmalloc(opp_size, GFP_KERNEL);
		if (!svsb->freqs_pct)
			return -ENOMEM;

		for (i = 0, freq = (u32)-1; i < svsb->opp_count; i++, freq--) {
			opp = dev_pm_opp_find_freq_floor(svsb->dev, &freq);
			if (IS_ERR(opp)) {
				pr_err("%s: error opp entry!!, err = %ld\n",
				       svsb->name, PTR_ERR(opp));
				return PTR_ERR(opp);
			}

			svsb->opp_freqs[i] = freq;
			svsb->opp_volts[i] = dev_pm_opp_get_voltage(opp);
			svsb->freqs_pct[i] = percent(svsb->opp_freqs[i],
						     svsb->freq_base) & 0xff;
			dev_pm_opp_put(opp);
		}
	}

	return 0;
}

static int svs_suspend(struct device *dev)
{
	struct mtk_svs *svs = dev_get_drvdata(dev);
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	unsigned long flags;
	u32 idx;

	/* Wait if there is processing svs_isr(). Suspend all banks. */
	flags = claim_mtk_svs_lock();
	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svs->bank = svsb;
		svs_switch_bank(svs);
		svs_writel(svs, 0x0, SVSEN);
		svs_writel(svs, 0x00ffffff, INTSTS);
		svsb->suspended = true;
	}
	release_mtk_svs_lock(flags);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		if (svsb->phase == SVS_PHASE_MON) {
			svsb->phase = SVS_PHASE_INIT02;
			svs_set_volts(svsb, true);
		}
	}

	clk_disable_unprepare(svs->main_clk);

	return 0;
}

static int svs_resume(struct device *dev)
{
	struct mtk_svs *svs = dev_get_drvdata(dev);
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	int ret;
	u32 idx;

	ret = clk_prepare_enable(svs->main_clk);
	if (ret)
		pr_err("%s(): cannot enable main_clk\n", __func__);

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];
		svsb->suspended = false;
	}

	ret = svs_init02(svs);
	if (ret)
		return ret;

	svs_mon_mode(svs);

	return 0;
}

static int svs_debug_proc_show(struct seq_file *m, void *v)
{
	struct svs_bank *svsb = (struct svs_bank *)m->private;

	if (svsb->phase == SVS_PHASE_INIT01)
		seq_puts(m, "init1\n");
	else if (svsb->phase == SVS_PHASE_INIT02)
		seq_puts(m, "init2\n");
	else if (svsb->phase == SVS_PHASE_MON)
		seq_puts(m, "mon mode\n");
	else if (svsb->phase == SVS_PHASE_ERROR)
		seq_puts(m, "disabled\n");
	else
		seq_puts(m, "unknown\n");

	return 0;
}

static ssize_t svs_debug_proc_write(struct file *file,
				    const char __user *buffer,
				    size_t count, loff_t *pos)
{
	struct svs_bank *svsb = (struct svs_bank *)PDE_DATA(file_inode(file));
	struct mtk_svs *svs = dev_get_drvdata(svsb->dev);
	char *buf = (char *)__get_free_page(GFP_USER);
	unsigned long flags;
	int enabled, ret;

	if (svsb->phase == SVS_PHASE_ERROR)
		return count;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EFAULT;
	}

	buf[count] = '\0';

	ret = kstrtoint(buf, 10, &enabled);
	if (ret)
		return ret;

	if (!enabled) {
		flags = claim_mtk_svs_lock();
		svs->bank = svsb;

		svsb->init01_support = false;
		svsb->init02_support = false;
		svsb->mon_mode_support = false;

		svs_switch_bank(svs);
		svs_writel(svs, 0x0, SVSEN);
		svs_writel(svs, 0x00ffffff, INTSTS);
		release_mtk_svs_lock(flags);
	}

	svsb->phase = SVS_PHASE_ERROR;
	svs_set_volts(svsb, true);

	return count;
}

proc_fops_rw(svs_debug);

static int svs_dump_proc_show(struct seq_file *m, void *v)
{
	struct mtk_svs *svs = (struct mtk_svs *)m->private;
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	unsigned long svs_reg_addr;
	u32 idx, i, j;

	for (i = 0; i < svsp->efuse_num; i++) {
		if (svs->efuse[i])
			seq_printf(m, "M_HW_RES%d = 0x%08x\n",
				   i, svs->efuse[i]);
	}

	for (i = 0; i < svsp->thermal_efuse_num; i++) {
		if (svs->thermal_efuse && svs->thermal_efuse[i])
			seq_printf(m, "THERMAL_EFUSE%d = 0x%08x\n",
				   i, svs->thermal_efuse[i]);
	}

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!svsb->init01_support)
			continue;

		for (i = SVS_PHASE_INIT01; i <= SVS_PHASE_MON; i++) {
			seq_printf(m, "Bank_number = %u\n", svsb->hw_id);

			if (i < SVS_PHASE_MON)
				seq_printf(m, "mode = init%d\n", i + 1);
			else
				seq_puts(m, "mode = mon\n");

			for (j = TEMPMONCTL0; j < reg_num; j++) {
				svs_reg_addr = (unsigned long)(svs->base +
							       svsp->regs[j]);
				seq_printf(m, "0x%08lx = 0x%08x\n",
					   svs_reg_addr, svsb->reg_data[i][j]);
			}
		}
	}

	return 0;
}

proc_fops_ro(svs_dump);

static int svs_status_proc_show(struct seq_file *m, void *v)
{
	struct svs_bank *svsb = (struct svs_bank *)m->private;
	struct dev_pm_opp *opp;
	unsigned long freq;
	int zone_temp, ret;
	u32 i;

	ret = svs_get_zone_temperature(svsb, &zone_temp);
	if (ret)
		seq_printf(m, "%s: cannot get zone \"%s\" temperature\n",
			   svsb->name, svsb->zone_name);
	else
		seq_printf(m, "%s: temperature = %d\n", svsb->name, zone_temp);

	for (i = 0, freq = (u32)-1; i < svsb->opp_count; i++, freq--) {
		opp = dev_pm_opp_find_freq_floor(svsb->dev, &freq);
		if (IS_ERR(opp)) {
			seq_printf(m, "%s: error opp entry!!, err = %ld\n",
				   svsb->name, PTR_ERR(opp));
			return PTR_ERR(opp);
		}

		seq_printf(m, "opp_freqs[%02u]: %lu, volts[%02u]: %lu, ",
			   i, freq, i, dev_pm_opp_get_voltage(opp));
		seq_printf(m, "svsb_volts[%02u]: 0x%x, freqs_pct[%02u]: %u\n",
			   i, svsb->volts[i], i, svsb->freqs_pct[i]);
		dev_pm_opp_put(opp);
	}

	return 0;
}

proc_fops_ro(svs_status);

static int svs_volt_offset_proc_show(struct seq_file *m, void *v)
{
	struct svs_bank *svsb = (struct svs_bank *)m->private;

	seq_printf(m, "%d\n", svsb->volt_offset);

	return 0;
}

static ssize_t svs_volt_offset_proc_write(struct file *file,
					  const char __user *buffer,
					  size_t count, loff_t *pos)
{
	struct svs_bank *svsb = (struct svs_bank *)PDE_DATA(file_inode(file));
	char *buf = (char *)__get_free_page(GFP_USER);
	int ret, volt_offset;

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EFAULT;
	}

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &volt_offset)) {
		svsb->volt_offset = volt_offset;
		ret = svs_set_volts(svsb, true);
		if (ret)
			return ret;
	}

	return count;
}

proc_fops_rw(svs_volt_offset);

static int svs_create_svs_procfs(struct mtk_svs *svs)
{
	const struct svs_platform *svsp = svs->platform;
	struct svs_bank *svsb;
	struct proc_dir_entry *svs_dir, *bank_dir;
	u32 idx, i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry svs_entries[] = {
		proc_entry(svs_dump),
	};

	struct pentry bank_entries[] = {
		proc_entry(svs_debug),
		proc_entry(svs_status),
		proc_entry(svs_volt_offset),
	};

	svs_dir = proc_mkdir("svs", NULL);
	if (!svs_dir) {
		pr_err("mkdir /proc/svs failed\n");
		return -EPERM;
	}

	for (i = 0; i < ARRAY_SIZE(svs_entries); i++) {
		if (!proc_create_data(svs_entries[i].name, 0664,
				      svs_dir, svs_entries[i].fops, svs)) {
			pr_err("create /proc/svs/%s failed\n",
			       svs_entries[i].name);
			return -EPERM;
		}
	}

	for (idx = 0; idx < svsp->bank_num; idx++) {
		svsb = &svsp->banks[idx];

		if (!svsb->init01_support)
			continue;

		bank_dir = proc_mkdir(svsb->name, svs_dir);
		if (!bank_dir) {
			pr_err("mkdir /proc/svs/%s failed\n", svsb->name);
			return -EPERM;
		}

		for (i = 0; i < ARRAY_SIZE(bank_entries); i++) {
			if (!proc_create_data(bank_entries[i].name, 0664,
					      bank_dir, bank_entries[i].fops,
					      svsb)) {
				pr_err("create /proc/svs/%s/%s failed\n",
				       svsb->name, bank_entries[i].name);
				return -EPERM;
			}
		}
	}

	return 0;
}

static struct svs_bank_ops svs_mt8183_banks_ops = {
	.set_freqs_pct	= svs_set_freqs_pct_v2,
	.get_vops	= svs_get_vops_v2,
};

static struct svs_bank svs_mt8183_banks[4] = {
	{
		.of_compatible		= "mediatek,mt8183-svs-cpu-little",
		.sw_id			= SVS_CPU_LITTLE,
		.hw_id			= 0,
		.ops			= &svs_mt8183_banks_ops,
		.zone_name		= "tzts4",
		.buck_name		= "vcpu-little",
		.mtcmos_request		= false,
		.init01_volt_flag	= SVS_INIT01_VOLT_INC_ONLY,
		.init01_support		= true,
		.init02_support		= true,
		.mon_mode_support	= false,
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
		.agem			= 0x0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x7,
		.vco			= 0x10,
		.chkshift		= 0x77,
		.upper_temp_bound	= 0x64,
		.lower_temp_bound	= 0xb2,
		.low_temp_threashold	= 25000,
		.low_temp_offset	= 0,
		.coresel		= 0x8fff0000,
		.systemclk_en		= BIT(31),
		.intst			= BIT(0),
		.ctl0			= 0x00010001,
	},
	{
		.of_compatible		= "mediatek,mt8183-svs-cpu-big",
		.sw_id			= SVS_CPU_BIG,
		.hw_id			= 1,
		.ops			= &svs_mt8183_banks_ops,
		.zone_name		= "tzts5",
		.buck_name		= "vcpu-big",
		.mtcmos_request		= false,
		.init01_volt_flag	= SVS_INIT01_VOLT_INC_ONLY,
		.init01_support		= true,
		.init02_support		= true,
		.mon_mode_support	= false,
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
		.agem			= 0x0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x7,
		.vco			= 0x10,
		.chkshift		= 0x77,
		.upper_temp_bound	= 0x64,
		.lower_temp_bound	= 0xb2,
		.low_temp_threashold	= 25000,
		.low_temp_offset	= 0,
		.coresel		= 0x8fff0001,
		.systemclk_en		= BIT(31),
		.intst			= BIT(1),
		.ctl0			= 0x00000001,
	},
	{
		.of_compatible		= "mediatek,mt8183-svs-cci",
		.sw_id			= SVS_CCI,
		.hw_id			= 2,
		.ops			= &svs_mt8183_banks_ops,
		.zone_name		= "tzts4",
		.buck_name		= "vcci",
		.mtcmos_request		= false,
		.init01_volt_flag	= SVS_INIT01_VOLT_INC_ONLY,
		.init01_support		= true,
		.init02_support		= true,
		.mon_mode_support	= false,
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
		.agem			= 0x0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x7,
		.vco			= 0x10,
		.chkshift		= 0x77,
		.upper_temp_bound	= 0x64,
		.lower_temp_bound	= 0xb2,
		.low_temp_threashold	= 25000,
		.low_temp_offset	= 0,
		.coresel		= 0x8fff0002,
		.systemclk_en		= BIT(31),
		.intst			= BIT(2),
		.ctl0			= 0x00100003,
	},
	{
		.of_compatible		= "mediatek,mt8183-svs-gpu",
		.sw_id			= SVS_GPU,
		.hw_id			= 3,
		.ops			= &svs_mt8183_banks_ops,
		.zone_name		= "tzts2",
		.buck_name		= "vgpu",
		.mtcmos_request		= true,
		.init01_volt_flag	= SVS_INIT01_VOLT_INC_ONLY,
		.init01_support		= true,
		.init02_support		= true,
		.mon_mode_support	= true,
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
		.agem			= 0x0,
		.dc_config		= 0x555555,
		.dvt_fixed		= 0x3,
		.vco			= 0x10,
		.chkshift		= 0x77,
		.upper_temp_bound	= 0x64,
		.lower_temp_bound	= 0xb2,
		.low_temp_threashold	= 25000,
		.low_temp_offset	= 3,
		.coresel		= 0x8fff0003,
		.systemclk_en		= BIT(31),
		.intst			= BIT(3),
		.ctl0			= 0x00050001,
	},
};

static const struct svs_platform svs_mt8183_platform = {
	.name		= "mt8183-svs",
	.banks		= svs_mt8183_banks,
	.efuse_parsing	= svs_mt8183_efuse_parsing,
	.regs		= svs_regs_v2,
	.fake_efuse	= false,
	.bank_num	= 4,
	.efuse_num	= 25,
	.efuse_check	= 2,
	.thermal_efuse_num = 3,
};

static const struct of_device_id mtk_svs_of_match[] = {
	{
		.compatible = "mediatek,mt8183-svs",
		.data = &svs_mt8183_platform,
	}, {
		/* sentinel */
	},
};

static int svs_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_dev_id;
	struct mtk_svs *svs;
	int ret;
	u32 svs_irq;

	svs = devm_kzalloc(&pdev->dev, sizeof(*svs), GFP_KERNEL);
	if (!svs)
		return -ENOMEM;

	svs->dev = &pdev->dev;
	if (!svs->dev->of_node) {
		pr_err("cannot find device node\n");
		return -ENODEV;
	}

	svs->base = of_iomap(svs->dev->of_node, 0);
	if (IS_ERR(svs->base)) {
		pr_err("cannot find svs register base\n");
		return PTR_ERR(svs->base);
	}

	svs_irq = irq_of_parse_and_map(svs->dev->of_node, 0);
	ret = devm_request_threaded_irq(svs->dev, svs_irq, NULL, svs_isr,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"mtk-svs", svs);
	if (ret) {
		pr_err("register irq(%d) failed: %d\n", svs_irq, ret);
		return ret;
	}

	of_dev_id = of_match_node(mtk_svs_of_match, svs->dev->of_node);
	if (!of_dev_id || !of_dev_id->data)
		return -EINVAL;

	svs->platform = of_dev_id->data;
	dev_set_drvdata(svs->dev, svs);

	svs->main_clk = devm_clk_get(svs->dev, "main_clk");
	if (IS_ERR(svs->main_clk)) {
		pr_err("failed to get clock: %ld\n", PTR_ERR(svs->main_clk));
		return PTR_ERR(svs->main_clk);
	}

	ret = clk_prepare_enable(svs->main_clk);
	if (ret) {
		pr_err("cannot enable main_clk: %d\n", ret);
		return ret;
	}

	ret = svs_is_support(svs);
	if (ret)
		goto svs_probe_fail;

	ret = svs_resource_setup(svs);
	if (ret)
		goto svs_probe_fail;

	ret = svs_start(svs);
	if (ret)
		goto svs_probe_fail;

	ret = svs_create_svs_procfs(svs);
	if (ret)
		goto svs_probe_fail;

	return 0;

svs_probe_fail:
	clk_disable_unprepare(svs->main_clk);

	return ret;
}

static const struct dev_pm_ops svs_pm_ops = {
	.suspend	= svs_suspend,
	.resume		= svs_resume,
};

static struct platform_driver svs_driver = {
	.probe	= svs_probe,
	.driver	= {
		.name		= "mtk-svs",
		.pm		= &svs_pm_ops,
		.of_match_table	= of_match_ptr(mtk_svs_of_match),
	},
};

static int __init svs_init(void)
{
	int ret;

	ret = platform_driver_register(&svs_driver);
	if (ret) {
		pr_err("svs platform driver register failed: %d\n", ret);
		return ret;
	}

	return 0;
}

late_initcall_sync(svs_init);

MODULE_DESCRIPTION("MediaTek SVS Driver v1.0");
MODULE_LICENSE("GPL");
