/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Hanyi Wu <hanyi.wu@mediatek.com>
 *         Sascha Hauer <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/types.h>
#include <linux/workqueue.h>

/* AUXADC Registers */
#define AUXADC_CON0_V		0x000
#define AUXADC_CON1_V		0x004
#define AUXADC_CON1_SET_V	0x008
#define AUXADC_CON1_CLR_V	0x00c
#define AUXADC_CON2_V		0x010
#define AUXADC_DATA(channel)	(0x14 + (channel) * 4)
#define AUXADC_MISC_V		0x094

#define AUXADC_CON1_CHANNEL(x)	BIT(x)

#define APMIXED_SYS_TS_CON1	0x604

/* Thermal Controller Registers */
#define TEMP_MONCTL0		0x000
#define TEMP_MONCTL1		0x004
#define TEMP_MONCTL2		0x008
#define TEMP_MONIDET0		0x014
#define TEMP_MONIDET1		0x018
#define TEMP_MSRCTL0		0x038
#define TEMP_AHBPOLL		0x040
#define TEMP_AHBTO		0x044
#define TEMP_ADCPNP0		0x048
#define TEMP_ADCPNP1		0x04c
#define TEMP_ADCPNP2		0x050
#define TEMP_ADCPNP3		0x0b4

#define TEMP_ADCMUX		0x054
#define TEMP_ADCEN		0x060
#define TEMP_PNPMUXADDR		0x064
#define TEMP_ADCMUXADDR		0x068
#define TEMP_ADCENADDR		0x074
#define TEMP_ADCVALIDADDR	0x078
#define TEMP_ADCVOLTADDR	0x07c
#define TEMP_RDCTRL		0x080
#define TEMP_ADCVALIDMASK	0x084
#define TEMP_ADCVOLTAGESHIFT	0x088
#define TEMP_ADCWRITECTRL	0x08c
#define TEMP_MSR0		0x090
#define TEMP_MSR1		0x094
#define TEMP_MSR2		0x098
#define TEMP_MSR3		0x0B8

#define TEMP_SPARE0		0x0f0

#define SVS_BANK_CONFIG0	0x200
#define SVS_BANK_CONFIG1	0x204
#define SVS_BANK_CONFIG2	0x208
#define SVS_BANK_CONFIG3	0x20c
#define SVS_BANK_CONFIG4	0x210
#define SVS_BANK_CONFIG5	0x214
#define SVS_BANK_FREQPCT30	0x218
#define SVS_BANK_FREQPCT74	0x21c
#define SVS_BANK_LIMITVALS	0x220
#define SVS_BANK_CONFIG6	0x224
#define SVS_BANK_CONFIG7	0x228
#define SVS_BANK_CONFIG8	0x22c
#define SVS_BANK_CONFIG9	0x230
#define SVS_BANK_CONFIG10	0x234
#define SVS_BANK_EN		0x238
#define SVS_BANK_CONTROL0	0x23c
#define SVS_BANK_CONTROL1	0x240
#define SVS_BANK_CONTROL2	0x244
#define SVS_BANK_VOP30		0x248
#define SVS_BANK_VOP74		0x24c
#define SVS_BANK_INTST		0x254
#define SVS_BANK_CONTROL3	0x25c
#define SVS_BANK_CONTROL4	0x264

#define PTPCORESEL		0x400
#define SVS_SVSINTST		0x408

#define TEMP_MONCTL1_PERIOD_UNIT(x)	((x) & 0x3ff)

#define TEMP_MONCTL2_FILTER_INTERVAL(x)	(((x) & 0x3ff) << 16)
#define TEMP_MONCTL2_SENSOR_INTERVAL(x)	((x) & 0x3ff)

#define TEMP_AHBPOLL_ADC_POLL_INTERVAL(x)	(x)

#define TEMP_ADCWRITECTRL_ADC_PNP_WRITE		BIT(0)
#define TEMP_ADCWRITECTRL_ADC_MUX_WRITE		BIT(1)

#define TEMP_ADCVALIDMASK_VALID_HIGH		BIT(5)
#define TEMP_ADCVALIDMASK_VALID_POS(bit)	(bit)

#define MT8173_TS1	0
#define MT8173_TS2	1
#define MT8173_TS3	2
#define MT8173_TS4	3
#define MT8173_TSABB	4

/* AUXADC channel 11 is used for the temperature sensors */
#define MT8173_TEMP_AUXADC_CHANNEL	11

/* The total number of temperature sensors in the MT8173 */
#define MT8173_NUM_SENSORS		5

/* The number of banks in the MT8173 */
#define MT8173_NUM_ZONES		4

/* The number of sensing points per bank */
#define MT8173_NUM_SENSORS_PER_ZONE	4

/* The number of OPPs supported by SVS */
#define MT8173_NUM_SVS_OPP		8

/* Bit masks of SVS enable and IRQ configrations */
#define PHASE_0_EN		BIT(0)
#define PHASE_CON_EN		BIT(1)
#define PHASE_1_EN		BIT(2)
#define PHASE_EN_MASK		(PHASE_0_EN | PHASE_CON_EN | PHASE_1_EN)
#define PHASE_01_EN		(PHASE_0_EN | PHASE_1_EN)
#define PHASE_01_IRQ		BIT(0)
#define PHASE_CON_IRQ		(0xff << 16)

/* Bit mask of SVS bank flags*/
#define SVS_NEED_OVERFLOW_FIX	BIT(0)

/* SVS bank status */
#define SVS_STATUS_ERROR	BIT(0)

/* The number of SVS banks implemented */
#define MT8173_NUM_SVS_BANKS	2

#define MT8173_SVS_BANK_CA53	0
#define MT8173_SVS_BANK_CA72	1

/* Layout of the fuses providing the calibration data */
#define MT8173_CALIB_BUF0_VALID		BIT(0)
#define MT8173_CALIB_BUF1_ADC_GE(x)	(((x) >> 22) & 0x3ff)
#define MT8173_CALIB_BUF0_VTS_TS1(x)	(((x) >> 17) & 0x1ff)
#define MT8173_CALIB_BUF0_VTS_TS2(x)	(((x) >> 8) & 0x1ff)
#define MT8173_CALIB_BUF1_VTS_TS3(x)	(((x) >> 0) & 0x1ff)
#define MT8173_CALIB_BUF2_VTS_TS4(x)	(((x) >> 23) & 0x1ff)
#define MT8173_CALIB_BUF2_VTS_TSABB(x)	(((x) >> 14) & 0x1ff)
#define MT8173_CALIB_BUF0_DEGC_CALI(x)	(((x) >> 1) & 0x3f)
#define MT8173_CALIB_BUF0_O_SLOPE(x)	(((x) >> 26) & 0x3f)
#define MT8173_CALIB_BUF0_O_SLOPE_S(x)	(((x) >> 7) & 0x1)

/* SVS configuration register constants */
#define SVS_LIMITVALS_CONST	0x1fe
#define SVS_CONFIG1_CONST	0x100006
#define SVS_CONFIG4_CONST	0x555555
#define SVS_CONFIG5_CONST	0x555555
#define SVS_CONFIG7_CONST	0xa28
#define SVS_CONFIG8_CONST	0xffff
#define SVS_CONFIG10_CONST	0x80000000
#define SVS_CONTROL3_P01	0x5f01
#define SVS_CONTROL3_CON	0xff0000

#define SVS_CONFIG9_VAL(b, m)	(((b) & 0xfff) << 12) | ((m) & 0xfff)
#define SVS_CONTROL4_OVFIX(v)	(((v) & ~0xf) | 0x7)

#define SVS_LOW_TEMP		33000
#define SVS_LOW_TEMP_OFFSET	10

/* Constants for calibration data calculation */
#define GE_ZERO_BASE	512		/* 0 of 10-bit sign integer */
#define SLOPE_OFFSET	165		/* 0.00165 * 100000  */
#define TS_GAIN		18		/* 1.8 * 10 */
#define ADC_FS		15		/* 1.5 * 10 */
#define TEMP_OFFSET	25 * 10
#define VTS_OFFSET	3350
#define ADC_RESOLUTION	(1 << 12)	/* 12-bit ADC full code */
#define BTS_PRESCALE	4

/* Helpers to calculate configuration values from SVS calibration data */
#define SVS_CALIB_VALID	BIT(0)
#define BANK_SHIFT(bank) (((bank) == 0) ? 8 : 0)
#define SVS_CALIB_BANK_CONFIG0(buf, b)				\
	(((((buf[33] >> BANK_SHIFT(b)) & 0xff)) << 8) |		\
	((buf[32] >> BANK_SHIFT(b)) & 0xff))
#define SVS_CALIB_BANK_CONFIG1(buf, b)				\
	((((buf[34] >> BANK_SHIFT(b)) & 0xff) << 8) | SVS_CONFIG1_CONST)
#define SVS_CALIB_BANK_CONFIG2L(base, b)			\
	((buf[0] >> BANK_SHIFT(b)) & 0xff)
#define SVS_CALIB_BANK_CONFIG2H(base, b)			\
	((buf[1] >> BANK_SHIFT(b)) & 0xff)
#define SVS_CALIB_BANK_CONFIG3(base, b)				\
	(((buf[2] >> BANK_SHIFT(b)) & 0xff) << 8)

#define THERMAL_NAME    "mtk-thermal"

struct mtk_thermal;

struct mtk_thermal_bank {
	struct mtk_thermal *mt;
	int id;
};

enum mtk_svs_state {
	SVS_INIT,
	SVS_PHASE_0,
	SVS_PHASE_1,
	SVS_PHASE_CONTINUOUS,
};

struct mtk_svs_bank {
	int bank_id;
	int cpu_dev_id;

	u32 flags;

	u32 status;

	enum mtk_svs_state state;
	/* Use this to limit bank frequency */
	unsigned long max_freq_khz;
	unsigned long min_freq_khz;

	struct mtk_thermal *mt;
	struct completion init_done;
	struct work_struct work;

	struct device *dev;
	struct regulator *reg;

	/* SVS per-bank calibration values */
	u32 ctrl0;
	u32 config0;
	u32 config1;
	u32 config2;
	u32 config3;

	unsigned long freq_table[MT8173_NUM_SVS_OPP];	/* in KHz*/
	int volt_table[MT8173_NUM_SVS_OPP];		/* in uVolt */
	int updated_volt_table[MT8173_NUM_SVS_OPP];	/* in uVolt */
};

struct mtk_svs_bank_cfg {
	int ts;
	int vmin_uV;
	int vmax_uV;
	int vboot_uV;
	unsigned long base_freq_hz;
};

struct mtk_thermal {
	struct device *dev;
	void __iomem *thermal_base;

	struct clk *clk_peri_therm;
	struct clk *clk_auxadc;
	struct clk *svs_mux;
	struct clk *svs_pll;

	struct mtk_thermal_bank banks[MT8173_NUM_ZONES];

	/* lock: for getting and putting banks */
	struct mutex lock;

	int svs_irq;

	/* Calibration values */
	s32 adc_ge;
	s32 degc_cali;
	s32 o_slope;
	s32 vts[MT8173_NUM_SENSORS];

	/*
	 * MTS and BTS are factors used by SVS to get per-bank temperature:
	 * Bank Temperature = [ADC Value] * MTS + BTS[Bank]
	 */
	s32 bts[MT8173_NUM_ZONES];
	s32 mts;
};

struct mtk_thermal_bank_cfg {
	unsigned int num_sensors;
	unsigned int sensors[MT8173_NUM_SENSORS_PER_ZONE];
};

static const int sensor_mux_values[MT8173_NUM_SENSORS] = { 0, 1, 2, 3, 16 };

/*
 * The MT8173 thermal controller has four banks. Each bank can read up to
 * four temperature sensors simultaneously. The MT8173 has a total of 5
 * temperature sensors. We use each bank to measure a certain area of the
 * SoC. Since TS2 is located centrally in the SoC it is influenced by multiple
 * areas, hence is used in different banks.
 *
 * The thermal core only gets the maximum temperature of all banks, so
 * the bank concept wouldn't be necessary here. However, the SVS (Smart
 * Voltage Scaling) unit makes its decisions based on the same bank
 * data, and this indeed needs the temperatures of the individual banks
 * for making better decisions.
 */
static const struct mtk_thermal_bank_cfg bank_data[] = {
	{
		.num_sensors = 2,
		.sensors = { MT8173_TS2, MT8173_TS3 },
	}, {
		.num_sensors = 2,
		.sensors = { MT8173_TS2, MT8173_TS4 },
	}, {
		.num_sensors = 3,
		.sensors = { MT8173_TS1, MT8173_TS2, MT8173_TSABB },
	}, {
		.num_sensors = 1,
		.sensors = { MT8173_TS2 },
	},
};

struct mtk_thermal_sense_point {
	int msr;
	int adcpnp;
};

static const struct mtk_thermal_sense_point
		sensing_points[MT8173_NUM_SENSORS_PER_ZONE] = {
	{
		.msr = TEMP_MSR0,
		.adcpnp = TEMP_ADCPNP0,
	}, {
		.msr = TEMP_MSR1,
		.adcpnp = TEMP_ADCPNP1,
	}, {
		.msr = TEMP_MSR2,
		.adcpnp = TEMP_ADCPNP2,
	}, {
		.msr = TEMP_MSR3,
		.adcpnp = TEMP_ADCPNP3,
	},
};

static const struct mtk_svs_bank_cfg svs_bank_cfgs[MT8173_NUM_SVS_BANKS] = {
	[MT8173_SVS_BANK_CA53] = {
		.vmax_uV = 1125000,
		.vmin_uV = 800000,
		.vboot_uV = 1000000,
		.base_freq_hz = 1600000000,
		.ts = MT8173_TS3
	},
	[MT8173_SVS_BANK_CA72] = {
		.vmax_uV = 1125000,
		.vmin_uV = 800000,
		.vboot_uV = 1000000,
		.base_freq_hz = 2000000000,
		.ts = MT8173_TS4
	}
};

static struct mtk_svs_bank svs_banks[MT8173_NUM_SVS_BANKS] = {{0}};

/**
 * raw_to_mcelsius - convert a raw ADC value to mcelsius
 * @mt:		The thermal controller
 * @raw:	raw ADC value
 *
 * This converts the raw ADC value to mcelsius using the SoC specific
 * calibration constants
 */
static int raw_to_mcelsius(struct mtk_thermal *mt, int sensno, s32 raw)
{
	s32 tmp;

	raw &= 0xfff;

	tmp = 203450520 << 3;
	tmp /= mt->o_slope;
	tmp /= 10000 + mt->adc_ge;
	tmp *= raw - mt->vts[sensno] - VTS_OFFSET;
	tmp >>= 3;

	return mt->degc_cali * 500 - tmp;
}

/**
 * uvolt_to_config - convert a voltage value to SVS voltage config value
 * @uvolt:	voltage value
 */
static inline u8 uvolt_to_config(int uvolt)
{
	return ((uvolt / 1000 - 700) * 100 + 625 - 1) / 625;
}

/**
 * config_to_uvolt - convert a SVS voltage config value to voltage value
 * @val:	SVS voltage config value
 */
static inline int config_to_uvolt(u32 val)
{
	return ((val * 625 / 100) + 700) * 1000;
}

/**
 * hz_to_config - convert a frequency value to SVS frequency config value
 * @rate:	frequency value
 * @base_rate:	rate to be used to calculate frequency percentage
 */
static inline u8 hz_to_config(unsigned long rate, unsigned long base_rate)
{
	return (rate * 100 + base_rate - 1) / base_rate;
}

/**
 * mtk_thermal_get_bank - get bank
 * @bank:	The bank
 *
 * The bank registers are banked, we have to select a bank in the
 * PTPCORESEL register to access it.
 */
static void mtk_thermal_get_bank(struct mtk_thermal_bank *bank)
{
	struct mtk_thermal *mt = bank->mt;
	u32 val;

	mutex_lock(&mt->lock);

	val = readl(mt->thermal_base + PTPCORESEL);
	val &= ~0xf;
	val |= bank->id;
	writel(val, mt->thermal_base + PTPCORESEL);
}

/**
 * mtk_thermal_put_bank - release bank
 * @bank:	The bank
 *
 * release a bank previously taken with mtk_thermal_get_bank,
 */
static void mtk_thermal_put_bank(struct mtk_thermal_bank *bank)
{
	struct mtk_thermal *mt = bank->mt;

	mutex_unlock(&mt->lock);
}

/**
 * mtk_thermal_bank_temperature - get the temperature of a bank
 * @bank:	The bank
 *
 * The temperature of a bank is considered the maximum temperature of
 * the sensors associated to the bank.
 */
static int mtk_thermal_bank_temperature(struct mtk_thermal_bank *bank)
{
	struct mtk_thermal *mt = bank->mt;
	int i, temp = INT_MIN, max = INT_MIN;
	u32 raw;

	for (i = 0; i < bank_data[bank->id].num_sensors; i++) {
		raw = readl(mt->thermal_base + sensing_points[i].msr);

		temp = raw_to_mcelsius(mt, bank_data[bank->id].sensors[i], raw);

		/*
		 * The first read of a sensor often contains very high bogus
		 * temperature value. Filter these out so that the system does
		 * not immediately shut down.
		 */
		if (temp > 200000)
			temp = 0;

		if (temp > max)
			max = temp;
	}

	return max;
}

static int mtk_read_temp(void *data, int *temperature)
{
	struct mtk_thermal *mt = data;
	int i;
	int tempmax = INT_MIN;

	for (i = 0; i < MT8173_NUM_ZONES; i++) {
		struct mtk_thermal_bank *bank = &mt->banks[i];

		mtk_thermal_get_bank(bank);

		tempmax = max(tempmax, mtk_thermal_bank_temperature(bank));

		mtk_thermal_put_bank(bank);
	}

	*temperature = tempmax;

	return 0;
}

static const struct thermal_zone_of_device_ops mtk_thermal_ops = {
	.get_temp = mtk_read_temp,
};

static void mtk_thermal_init_bank(struct mtk_thermal *mt, int num,
				  u32 apmixed_phys_base, u32 auxadc_phys_base)
{
	struct mtk_thermal_bank *bank = &mt->banks[num];
	const struct mtk_thermal_bank_cfg *cfg = &bank_data[num];
	int i;

	bank->id = num;
	bank->mt = mt;

	mtk_thermal_get_bank(bank);

	/* bus clock 66M counting unit is 12 * 15.15ns * 256 = 46.540us */
	writel(TEMP_MONCTL1_PERIOD_UNIT(12), mt->thermal_base + TEMP_MONCTL1);

	/*
	 * filt interval is 1 * 46.540us = 46.54us,
	 * sen interval is 429 * 46.540us = 19.96ms
	 */
	writel(TEMP_MONCTL2_FILTER_INTERVAL(1) |
			TEMP_MONCTL2_SENSOR_INTERVAL(429),
			mt->thermal_base + TEMP_MONCTL2);

	/* poll is set to 10u */
	writel(TEMP_AHBPOLL_ADC_POLL_INTERVAL(768),
	       mt->thermal_base + TEMP_AHBPOLL);

	/* temperature sampling control, 1 sample */
	writel(0x0, mt->thermal_base + TEMP_MSRCTL0);

	/* exceed this polling time, IRQ would be inserted */
	writel(0xffffffff, mt->thermal_base + TEMP_AHBTO);

	/* number of interrupts per event, 1 is enough */
	writel(0x0, mt->thermal_base + TEMP_MONIDET0);
	writel(0x0, mt->thermal_base + TEMP_MONIDET1);

	/*
	 * The MT8173 thermal controller does not have its own ADC. Instead it
	 * uses AHB bus accesses to control the AUXADC. To do this the thermal
	 * controller has to be programmed with the physical addresses of the
	 * AUXADC registers and with the various bit positions in the AUXADC.
	 * Also the thermal controller controls a mux in the APMIXEDSYS register
	 * space.
	 */

	/*
	 * this value will be stored to TEMP_PNPMUXADDR (TEMP_SPARE0)
	 * automatically by hw
	 */
	writel(BIT(MT8173_TEMP_AUXADC_CHANNEL), mt->thermal_base + TEMP_ADCMUX);

	/* AHB address for auxadc mux selection */
	writel(auxadc_phys_base + AUXADC_CON1_CLR_V,
	       mt->thermal_base + TEMP_ADCMUXADDR);

	/* AHB address for pnp sensor mux selection */
	writel(apmixed_phys_base + APMIXED_SYS_TS_CON1,
	       mt->thermal_base + TEMP_PNPMUXADDR);

	/* AHB value for auxadc enable */
	writel(BIT(MT8173_TEMP_AUXADC_CHANNEL), mt->thermal_base + TEMP_ADCEN);

	/* AHB address for auxadc enable (channel 0 immediate mode selected) */
	writel(auxadc_phys_base + AUXADC_CON1_SET_V,
	       mt->thermal_base + TEMP_ADCENADDR);

	/* AHB address for auxadc valid bit */
	writel(auxadc_phys_base + AUXADC_DATA(MT8173_TEMP_AUXADC_CHANNEL),
	       mt->thermal_base + TEMP_ADCVALIDADDR);

	/* AHB address for auxadc voltage output */
	writel(auxadc_phys_base + AUXADC_DATA(MT8173_TEMP_AUXADC_CHANNEL),
	       mt->thermal_base + TEMP_ADCVOLTADDR);

	/* read valid & voltage are at the same register */
	writel(0x0, mt->thermal_base + TEMP_RDCTRL);

	/* indicate where the valid bit is */
	writel(TEMP_ADCVALIDMASK_VALID_HIGH | TEMP_ADCVALIDMASK_VALID_POS(12),
	       mt->thermal_base + TEMP_ADCVALIDMASK);

	/* no shift */
	writel(0x0, mt->thermal_base + TEMP_ADCVOLTAGESHIFT);

	/* enable auxadc mux write transaction */
	writel(TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       mt->thermal_base + TEMP_ADCWRITECTRL);

	for (i = 0; i < cfg->num_sensors; i++)
		writel(sensor_mux_values[cfg->sensors[i]],
		       mt->thermal_base + sensing_points[i].adcpnp);

	writel((1 << cfg->num_sensors) - 1, mt->thermal_base + TEMP_MONCTL0);

	writel(TEMP_ADCWRITECTRL_ADC_PNP_WRITE |
	       TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       mt->thermal_base + TEMP_ADCWRITECTRL);

	mtk_thermal_put_bank(bank);
}

static u64 of_get_phys_base(struct device_node *np)
{
	u64 size64;
	const __be32 *regaddr_p;

	regaddr_p = of_get_address(np, 0, &size64, NULL);
	if (!regaddr_p)
		return OF_BAD_ADDR;

	return of_translate_address(np, regaddr_p);
}

static int mtk_thermal_get_calibration_data(struct device *dev,
					    struct mtk_thermal *mt)
{
	struct nvmem_cell *cell;
	u32 *buf;
	size_t len;
	int i, ret = 0;
	s32 o_slope_s, ge, oe, gain, x_roomt, ts_intercept, ts_degc, ts_factor;

	/* Start with default values */
	mt->adc_ge = 512;
	for (i = 0; i < MT8173_NUM_SENSORS; i++)
		mt->vts[i] = 260;
	mt->degc_cali = 40;
	mt->o_slope = 0;
	o_slope_s = 0;

	cell = nvmem_cell_get(dev, "calibration-data");
	if (IS_ERR(cell)) {
		if (PTR_ERR(cell) == -EPROBE_DEFER)
			return PTR_ERR(cell);
		return 0;
	}

	buf = (u32 *)nvmem_cell_read(cell, &len);

	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	if (len < 3 * sizeof(u32)) {
		dev_warn(dev, "invalid calibration data\n");
		ret = -EINVAL;
		goto out;
	}

	if (buf[0] & MT8173_CALIB_BUF0_VALID) {
		mt->adc_ge = MT8173_CALIB_BUF1_ADC_GE(buf[1]);
		mt->vts[MT8173_TS1] = MT8173_CALIB_BUF0_VTS_TS1(buf[0]);
		mt->vts[MT8173_TS2] = MT8173_CALIB_BUF0_VTS_TS2(buf[0]);
		mt->vts[MT8173_TS3] = MT8173_CALIB_BUF1_VTS_TS3(buf[1]);
		mt->vts[MT8173_TS4] = MT8173_CALIB_BUF2_VTS_TS4(buf[2]);
		mt->vts[MT8173_TSABB] = MT8173_CALIB_BUF2_VTS_TSABB(buf[2]);
		mt->degc_cali = MT8173_CALIB_BUF0_DEGC_CALI(buf[0]);
		mt->o_slope = MT8173_CALIB_BUF0_O_SLOPE(buf[0]);
		o_slope_s = MT8173_CALIB_BUF0_O_SLOPE_S(buf[0]);
		if (o_slope_s != 0)
			mt->o_slope = -mt->o_slope;
		mt->o_slope += SLOPE_OFFSET;
	} else {
		dev_info(dev, "Device not calibrated, using default calibration values\n");
	}

	/*
	 * The constants 10, 10000, 100000 below are pre-scalers to avoid
	 * calculation underflow, and will be divided in the final results.
	 */
	oe = mt->adc_ge - GE_ZERO_BASE;
	ge = oe * 10000 / ADC_RESOLUTION;
	gain = 10000 + ge;

	/* calculating MTS */
	mt->mts = 100000 * 10000 / gain * ADC_FS / TS_GAIN / mt->o_slope;

	ts_degc = mt->degc_cali * 10 / 2;
	ts_factor = 100000 * 10000 / ADC_RESOLUTION / gain * ge;

	/* calculating per-bank BTS */
	for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
		int ts = svs_bank_cfgs[i].ts;

		x_roomt = mt->vts[ts] + VTS_OFFSET - oe * 10000 / ADC_RESOLUTION * 10000 / gain;
		ts_intercept = (ts_factor + x_roomt * 10 * ADC_FS / TS_GAIN ) * 10 / mt->o_slope;
		ts_intercept += ts_degc - TEMP_OFFSET;

		mt->bts[i] = ts_intercept * BTS_PRESCALE / 10;
	}

out:
	kfree(buf);

	return ret;
}

static int mtk_svs_get_calibration_data(struct device *dev,
					struct mtk_thermal *mt)
{
	struct nvmem_cell *cell;
	u32 *buf;
	size_t len;
	int i, ret = 0;

	cell = nvmem_cell_get(dev, "svs-calibration-data");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf)) {
		dev_err(dev, "failed to get svs calibration data: %ld\n",
			PTR_ERR(buf));
		return PTR_ERR(buf);
	}

	if (len < 0x8c || !(buf[29] & SVS_CALIB_VALID)) {
		dev_err(dev, "Invalid SVS calibration data\n");
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
		u32 temp;

		svs_banks[i].config0 =
				SVS_CALIB_BANK_CONFIG0(buf, i);
		svs_banks[i].config1 =
				SVS_CALIB_BANK_CONFIG1(buf, i);
		svs_banks[i].config3 =
				SVS_CALIB_BANK_CONFIG3(buf, i);

		temp = SVS_CALIB_BANK_CONFIG2H(buf, i);
		if (temp < 128 && i == MT8173_SVS_BANK_CA72) {
			temp = (unsigned char)((temp - 256) / 2);
			svs_banks[i].flags |= SVS_NEED_OVERFLOW_FIX;
		}
		temp = ((temp & 0xff) << 8) |
		       SVS_CALIB_BANK_CONFIG2L(buf, i);
		svs_banks[i].config2 = temp;
	}

out:
	kfree(buf);

	return ret;
}

/* Caller must call this function with mt->lock held */
static void mtk_svs_set_phase(struct mtk_svs_bank *svs, int phase)
{
	struct mtk_thermal *mt = svs->mt;
	unsigned long *freq_tbl, base_freq_hz;
	int id = svs->bank_id;

	freq_tbl = svs->freq_table;
	base_freq_hz = svs_bank_cfgs[id].base_freq_hz;

	writel(svs->config0, mt->thermal_base + SVS_BANK_CONFIG0);
	writel(svs->config1, mt->thermal_base + SVS_BANK_CONFIG1);
	writel(svs->config2, mt->thermal_base + SVS_BANK_CONFIG2);
	writel(svs->config3, mt->thermal_base + SVS_BANK_CONFIG3);
	writel(SVS_CONFIG4_CONST, mt->thermal_base + SVS_BANK_CONFIG4);
	writel(SVS_CONFIG5_CONST, mt->thermal_base + SVS_BANK_CONFIG5);
	writel(SVS_CONFIG10_CONST, mt->thermal_base + SVS_BANK_CONFIG10);

	/*
	 * SVS_BANK_FREQPCT30 and SVS_BANK_FREQPCT74 inform SVS the frequencies
	 * of OPP table. The frequency values are set in the form:
	 * frequency = (config / 100) * [base frequency of this bank]
	 */
	writel(hz_to_config(freq_tbl[0], base_freq_hz) |
	       (hz_to_config(freq_tbl[1], base_freq_hz) << 8) |
	       (hz_to_config(freq_tbl[2], base_freq_hz) << 16) |
	       (hz_to_config(freq_tbl[3], base_freq_hz) << 24),
	       mt->thermal_base + SVS_BANK_FREQPCT30);

	writel(hz_to_config(freq_tbl[4], base_freq_hz) |
	       (hz_to_config(freq_tbl[5], base_freq_hz) << 8) |
	       (hz_to_config(freq_tbl[6], base_freq_hz) << 16) |
	       (hz_to_config(freq_tbl[7], base_freq_hz) << 24),
	       mt->thermal_base + SVS_BANK_FREQPCT74);

	writel((uvolt_to_config(svs_bank_cfgs[id].vmax_uV) << 24) |
	       (uvolt_to_config(svs_bank_cfgs[id].vmin_uV) << 16) |
	       SVS_LIMITVALS_CONST, mt->thermal_base + SVS_BANK_LIMITVALS);

	writel(uvolt_to_config(svs_bank_cfgs[id].vboot_uV),
	       mt->thermal_base + SVS_BANK_CONFIG6);
	writel(SVS_CONFIG7_CONST, mt->thermal_base + SVS_BANK_CONFIG7);
	writel(SVS_CONFIG8_CONST, mt->thermal_base + SVS_BANK_CONFIG8);

	/* clear all pending interrupt */
	writel(0xffffffff, mt->thermal_base + SVS_BANK_INTST);

	/* Workaround for calibration data overflow on CA72 bank */
	if (svs->flags & SVS_NEED_OVERFLOW_FIX) {
		u32 reg;

		reg = readl(mt->thermal_base + SVS_BANK_CONTROL4);
		writel(SVS_CONTROL4_OVFIX(reg),
		       mt->thermal_base + MT8173_SVS_BANK_CA72);
	}

	switch (phase) {
	case SVS_PHASE_0:
		writel(SVS_CONTROL3_P01, mt->thermal_base + SVS_BANK_CONTROL3);
		writel(PHASE_0_EN, mt->thermal_base + SVS_BANK_EN);
		svs->state = SVS_PHASE_0;
		break;
	case SVS_PHASE_1:
		writel(SVS_CONTROL3_P01, mt->thermal_base + SVS_BANK_CONTROL3);
		writel(svs->ctrl0, mt->thermal_base + SVS_BANK_CONTROL0);
		writel(PHASE_0_EN | PHASE_1_EN,
		       mt->thermal_base + SVS_BANK_EN);
		svs->state = SVS_PHASE_1;
		break;
	case SVS_PHASE_CONTINUOUS:
		writel(SVS_CONFIG9_VAL(mt->bts[id], mt->mts),
		       mt->thermal_base + SVS_BANK_CONFIG9);
		writel(SVS_CONTROL3_CON, mt->thermal_base + SVS_BANK_CONTROL3);
		writel(PHASE_CON_EN, mt->thermal_base + SVS_BANK_EN);
		svs->state = SVS_PHASE_CONTINUOUS;
	}
}

static void mtk_svs_adjust_voltage(struct mtk_svs_bank *svs)
{
	int i;

	for (i = 0; i < MT8173_NUM_SVS_OPP; i++) {
		if (!svs->freq_table[i])
			continue;

		dev_pm_opp_adjust_voltage(svs->dev, svs->freq_table[i],
					  svs->updated_volt_table[i]);
	}
}

/**
 * mtk_svs_update_voltage_table - update the calculated voltage table
 * @svs: The SVS bank
 *
 * Read the calculated voltage values from registers and update the SVS bank
 * voltage table which will be write to OPP table entries later. Caller should
 * select the bank and hold mt->lock before calling it.
 */
static void mtk_svs_update_voltage_table(struct mtk_svs_bank *svs)
{
	struct mtk_thermal *mt = svs->mt;
	int vmin_uV, vmax_uV, *volt_table;
	u32 reg;
	int temp, offset = 0;

	temp = mtk_thermal_bank_temperature(&mt->banks[svs->bank_id]);
	if (temp <= SVS_LOW_TEMP)
		offset = SVS_LOW_TEMP_OFFSET;

	vmin_uV = svs_bank_cfgs[svs->bank_id].vmin_uV;
	vmax_uV = svs_bank_cfgs[svs->bank_id].vmax_uV;
	volt_table = svs->updated_volt_table;

	/*
	 * The optimized voltage values calculated by SVS are put in the two
	 * registers, SVS_BANK_VOP30 and SVS_BANK_VOP74 which stores values
	 * corresponding to OPP[4-7] and OPP[4-7].
	 */
	reg = readl(mt->thermal_base + SVS_BANK_VOP30);
	volt_table[0] = clamp(config_to_uvolt((reg & 0xff) + offset),
			      vmin_uV, vmax_uV);
	volt_table[1] = clamp(config_to_uvolt(((reg >> 8) & 0xff) + offset),
			      vmin_uV, vmax_uV);
	volt_table[2] = clamp(config_to_uvolt(((reg >> 16) & 0xff) + offset),
			      vmin_uV, vmax_uV);
	volt_table[3] = clamp(config_to_uvolt(((reg >> 24) & 0xff) + offset),
			      vmin_uV, vmax_uV);

	reg = readl(mt->thermal_base + SVS_BANK_VOP74);
	volt_table[4] = clamp(config_to_uvolt((reg & 0xff) + offset),
			      vmin_uV, vmax_uV);
	volt_table[5] = clamp(config_to_uvolt(((reg >> 8) & 0xff) + offset),
			      vmin_uV, vmax_uV);
	volt_table[6] = clamp(config_to_uvolt(((reg >> 16) & 0xff) + offset),
			      vmin_uV, vmax_uV);
	volt_table[7] = clamp(config_to_uvolt(((reg >> 24) & 0xff) + offset),
			      vmin_uV, vmax_uV);
}

static void adjust_voltage_work(struct work_struct *work)
{
	struct mtk_svs_bank *svs = container_of(work, struct mtk_svs_bank,
						work);
	struct mtk_thermal *mt = svs->mt;

	if (svs->status & SVS_STATUS_ERROR || svs->state == SVS_INIT)
		goto out_only_adjust_voltage;

	mtk_thermal_get_bank(&mt->banks[svs->bank_id]);

	mtk_svs_update_voltage_table(svs);

	if (!completion_done(&svs->init_done)) {
		complete(&svs->init_done);
		mtk_svs_set_phase(svs, SVS_PHASE_CONTINUOUS);
	}

	mtk_thermal_put_bank(&mt->banks[svs->bank_id]);

out_only_adjust_voltage:
	mtk_svs_adjust_voltage(svs);
	if (svs->state == SVS_INIT)
		complete(&svs->init_done);
}

static void mtk_svs_bank_disable(struct mtk_svs_bank *svs)
{
	struct mtk_thermal *mt = svs->mt;
	int i;

	writel(0, mt->thermal_base + SVS_BANK_EN);
	writel(0xffffff, mt->thermal_base + SVS_BANK_INTST);

	for (i = 0; i < MT8173_NUM_SVS_OPP; i++) {
		if (!svs->freq_table[i])
			continue;

		svs->updated_volt_table[i] = svs->volt_table[i];
	}
}

static irqreturn_t mtk_svs_interrupt(int irqno, void *dev_id)
{
	struct mtk_thermal *mt = dev_id;
	u32 svs_intst, bank_en, bank_intst;
	int i;


	svs_intst = readl(mt->thermal_base + SVS_SVSINTST);
	for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
		struct mtk_svs_bank *svs = &svs_banks[i];

		if (svs_intst & BIT(i))
			continue;

		mtk_thermal_get_bank(&mt->banks[i]);

		bank_intst = readl(mt->thermal_base + SVS_BANK_INTST);
		bank_en = readl(mt->thermal_base + SVS_BANK_EN);

		if (bank_intst == PHASE_01_IRQ && /* phase 0 */
		    (bank_en & PHASE_EN_MASK) == PHASE_0_EN) {
			u32 reg;

			reg = readl(mt->thermal_base + SVS_BANK_CONTROL1);
			svs->ctrl0 |= (~(reg & 0xffff) + 1) & 0xffff;
			reg =  readl(mt->thermal_base + SVS_BANK_CONTROL2);
			svs->ctrl0 |= (reg & 0xffff) << 16;

			writel(0, mt->thermal_base + SVS_BANK_EN);
			writel(PHASE_01_IRQ, mt->thermal_base + SVS_BANK_INTST);

			mtk_svs_set_phase(svs, SVS_PHASE_1);
		} else if (bank_intst == PHASE_01_IRQ && /* phase 1 */
			   (bank_en & PHASE_EN_MASK) == PHASE_01_EN) {
			/*
			 * Schedule a work to update voltages of OPP table
			 * entries.
			 */
			schedule_work(&svs->work);

			writel(0, mt->thermal_base + SVS_BANK_EN);
			writel(PHASE_01_IRQ, mt->thermal_base + SVS_BANK_INTST);
		} else if (bank_intst & PHASE_CON_IRQ) { /* phase continuous*/
			/*
			 * Schedule a work to update voltages of OPP table
			 * entries.
			 */
			schedule_work(&svs->work);

			writel(PHASE_CON_IRQ,
			       mt->thermal_base + SVS_BANK_INTST);
		} else {
			svs->status |= SVS_STATUS_ERROR;

			mtk_svs_bank_disable(svs);
			dev_err(svs->dev,
				"SVS engine internal error. disabled.\n");

			/*
			 * Schedule a work to reset voltages of OPP table
			 * entries.
			 */
			schedule_work(&svs->work);
		}

		mtk_thermal_put_bank(&mt->banks[i]);
	}

	return IRQ_HANDLED;
}

static int mtk_svs_bank_init(struct mtk_svs_bank *svs)
{
	struct dev_pm_opp *opp;
	int ret = 0, count, i;
	unsigned long rate;

	init_completion(&svs->init_done);

	INIT_WORK(&svs->work, adjust_voltage_work);

	svs->dev = get_cpu_device(svs->cpu_dev_id);
	if (!svs->dev) {
		pr_err("failed to get cpu%d device\n", svs->cpu_dev_id);
		return -ENODEV;
	}

	/* Assume CPU DVFS OPP table is already initialized by cpufreq driver*/
	rcu_read_lock();
	count = dev_pm_opp_get_opp_count(svs->dev);
	if (count > MT8173_NUM_SVS_OPP)
		dev_warn(svs->dev, "%d OPP entries found.\n"
			 "But only %d OPP entry supported.\n", count,
			 MT8173_NUM_SVS_OPP);

	for (i = 0, rate = (unsigned long)-1; i < MT8173_NUM_SVS_OPP &&
	     i < count; i++, rate--) {
		opp = dev_pm_opp_find_freq_floor(svs->dev, &rate);
		if (IS_ERR(opp)) {
			dev_err(svs->dev, "error opp entry!!\n");
			rcu_read_unlock();
			ret = PTR_ERR(opp);
			goto out;
		}

		svs->freq_table[i] = rate;
		svs->volt_table[i] = dev_pm_opp_get_voltage(opp);
	}

out:
	rcu_read_unlock();

	return ret;
}

static int mtk_svs_hw_init(struct mtk_thermal *mt)
{
	struct clk *parent;
	unsigned long timeout;
	struct mtk_svs_bank *svs;
	struct cpufreq_policy policy;
	struct pm_qos_request qos_request = {{0}};
	int i, j, ret, vboot_uV;

	parent = clk_get_parent(mt->svs_mux);
	ret = clk_set_parent(mt->svs_mux, mt->svs_pll);
	if (ret) {
		dev_err(mt->dev,
			"failed to set svs_mux to svs_pll\n");
		return ret;
	}

	/*
	 * When doing SVS init, we have to make sure all CPUs are on and
	 * working at 1.0 volt. Add a pm_qos request to prevent CPUs from
	 * entering CPU off idle state.
	 */
	pm_qos_add_request(&qos_request, PM_QOS_CPU_DMA_LATENCY, 1);

	for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
		svs = &svs_banks[i];

		/* Backup current cpufreq policy */
		ret = cpufreq_get_policy(&policy, svs->cpu_dev_id);
		if (ret) {
			dev_err(svs->dev, "cpufreq is not ready.\n");
			pm_qos_remove_request(&qos_request);
			clk_set_parent(mt->svs_mux, parent);
			return ret;
		}

		/* Force CPUFreq switch to OPP with 1.0 volt */
		for (j = 0; j < MT8173_NUM_SVS_OPP; j++) {
			svs->updated_volt_table[j] = svs->volt_table[j];
			if (svs->volt_table[j] <= svs_bank_cfgs[i].vboot_uV &&
			    (!svs->max_freq_khz || !svs->min_freq_khz)) {
				svs->updated_volt_table[j] =
						svs_bank_cfgs[i].vboot_uV;
				svs->max_freq_khz = svs->freq_table[j] / 1000;
				svs->min_freq_khz = svs->freq_table[j] / 1000;
			}
		}

		schedule_work(&svs->work);
		timeout = wait_for_completion_timeout(&svs->init_done, HZ);
		if (timeout == 0) {
			dev_err(svs->dev, "SVS vboot init timeout.\n");
			ret = -EINVAL;
			goto err_bank_init;
		}

		reinit_completion(&svs->init_done);

		cpufreq_update_policy(svs->cpu_dev_id);

		/* Check if the voltage is successfully set as 1.0 volt */
		vboot_uV = regulator_get_voltage(svs->reg);
		if (uvolt_to_config(vboot_uV) !=
		    uvolt_to_config(svs_bank_cfgs[i].vboot_uV)) {
			dev_err(svs->dev, "Vboot value mismatch!\n");
			ret = -EINVAL;
			break;
		}

		/* Configure regulator to PWM mode */
		ret = regulator_set_mode(svs->reg, REGULATOR_MODE_FAST);
		if (ret) {
			dev_err(svs->dev,
				"Failed to set regulator in PWM mode\n");
			ret = -EINVAL;
			break;
		}

		mtk_thermal_get_bank(&mt->banks[i]);

		mtk_svs_set_phase(svs, SVS_PHASE_0);

		mtk_thermal_put_bank(&mt->banks[i]);

		timeout = wait_for_completion_timeout(&svs->init_done, HZ);
		if (timeout == 0) {
			dev_err(svs->dev, "SVS initialization timeout.\n");
			ret = -EINVAL;
			goto err_bank_init;
		}

		/* Unlimit CPUFreq OPP range */
		svs->max_freq_khz = policy.max;
		svs->min_freq_khz = policy.min;
		cpufreq_update_policy(svs->cpu_dev_id);

		/* Configure regulator to normal mode */
		ret = regulator_set_mode(svs->reg, REGULATOR_MODE_NORMAL);
		if (ret)
			dev_err(svs->dev,
				"Failed to set regulator in normal mode\n");
	}

err_bank_init:

	if (ret)
		for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
			svs = &svs_banks[i];

			mtk_thermal_get_bank(&mt->banks[i]);

			mtk_svs_bank_disable(svs);
			svs->status |= SVS_STATUS_ERROR;

			mtk_thermal_put_bank(&mt->banks[i]);

			schedule_work(&svs->work);
		}

	pm_qos_remove_request(&qos_request);

	ret = clk_set_parent(mt->svs_mux, parent);
	if (ret) {
		dev_err(mt->dev,
			"failed to set svs_mux to original parent\n");
		return ret;
	}

	return ret;
}

static int cpufreq_svs_notifier(struct notifier_block *nb, unsigned long event,
				void *data)
{
	struct cpufreq_policy *policy = data;
	int i;

	for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
		if (cpumask_test_cpu(svs_banks[i].cpu_dev_id, policy->cpus))
			break;
	}

	if (i >= MT8173_NUM_SVS_BANKS)
		return NOTIFY_DONE;

	switch (event) {

	case CPUFREQ_ADJUST:
		if (!svs_banks[i].min_freq_khz || !svs_banks[i].max_freq_khz)
			return NOTIFY_DONE;

		cpufreq_verify_within_limits(policy, svs_banks[i].min_freq_khz,
					     svs_banks[i].max_freq_khz);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static bool allow_svs_late_init;

static struct notifier_block svs_cpufreq_notifier_block = {
	.notifier_call = cpufreq_svs_notifier,
};

/*
 * When doing SVS init, we have to make sure all CPUs are on and working at
 * 1.0 volt. Currently we relies on cpufreq driver doing this by changing
 * OPP voltage and limit OPP during SVS init. To make sure cpufreq is already
 * working, put SVS hardware part init in late_initcall().
 */
static int mtk_svs_late_init(void)
{
	int ret, i;

	if (!allow_svs_late_init)
		return -EINVAL;

	ret = cpufreq_register_notifier(&svs_cpufreq_notifier_block,
					CPUFREQ_POLICY_NOTIFIER);

	for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
		svs_banks[i].bank_id = i;

		ret = mtk_svs_bank_init(&svs_banks[i]);
		if (ret) {
			pr_err("failed to initialize mtk svs bank%d\n", i);
			goto out;
		}
	}

	ret = mtk_svs_hw_init(svs_banks[0].mt);
	if (ret)
		pr_err("Failed to initialze MTK SVS engine\n");

out:
	cpufreq_unregister_notifier(&svs_cpufreq_notifier_block,
				    CPUFREQ_POLICY_NOTIFIER);

	return ret;
}
late_initcall(mtk_svs_late_init);

static int mtk_svs_get_cpu_id(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;

	ret = of_property_read_u32(np, "mediatek,svs-little-core-id",
				  &svs_banks[MT8173_SVS_BANK_CA53].cpu_dev_id);
	if (ret) {
		dev_err(&pdev->dev,
			"Cannot find property mediatek,svs-little-core-id\n");
		return ret;
	}

	ret = of_property_read_u32(np, "mediatek,svs-big-core-id",
				  &svs_banks[MT8173_SVS_BANK_CA72].cpu_dev_id);
	if (ret) {
		dev_err(&pdev->dev,
			"Cannot find property mediatek,svs-big-core-id\n");
		return ret;
	}

	return ret;
}

static int mtk_svs_probe(struct platform_device *pdev)
{
	struct mtk_thermal *mt = platform_get_drvdata(pdev);
	char supply[8];
	int i, ret;

	ret = mtk_svs_get_cpu_id(pdev);
	if (ret)
		return ret;

	mt->svs_pll = devm_clk_get(&pdev->dev, "svs_pll");
	if (IS_ERR(mt->svs_pll)) {
		if (PTR_ERR(mt->svs_pll) == -EPROBE_DEFER)
			return PTR_ERR(mt->svs_pll);

		pr_err("Failed to get SVS PLL clock\n");
		return ret;
	}

	mt->svs_mux = devm_clk_get(&pdev->dev, "svs_mux");
	if (IS_ERR(mt->svs_mux)) {
		if (PTR_ERR(mt->svs_mux) == -EPROBE_DEFER)
			return PTR_ERR(mt->svs_mux);

		pr_err("Failed to get SVS MUX clock\n");
		return ret;
	}

	for (i = 0; i < MT8173_NUM_SVS_BANKS; i++) {
		struct regulator *reg;

		snprintf(supply, sizeof(supply), "bank%d", i);
		reg = devm_regulator_get_optional(&pdev->dev, supply);
		if (IS_ERR(reg)) {
			if (PTR_ERR(reg) == -EPROBE_DEFER)
				return PTR_ERR(reg);

			pr_err("Failed to get %s regulator\n", supply);
			return ret;
		}

		svs_banks[i].reg = reg;
		svs_banks[i].mt = mt;
	}

	ret = mtk_svs_get_calibration_data(mt->dev, mt);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			pr_err("Failed to get SVS calibration data\n");
		return ret;
	}

	mt->svs_irq = platform_get_irq(pdev, 1);
	ret = devm_request_threaded_irq(&pdev->dev, mt->svs_irq, NULL,
					mtk_svs_interrupt,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"mtk-svs", mt);
	if (ret) {
		pr_err("Failed to get SVS IRQ\n");
		return ret;
	}

	/* SVS has successfully probed, allow SVS late init */
	allow_svs_late_init = true;

	return 0;
}

static int mtk_thermal_probe(struct platform_device *pdev)
{
	int ret, i;
	struct device_node *auxadc, *apmixedsys, *np = pdev->dev.of_node;
	struct mtk_thermal *mt;
	struct resource *res;
	u64 auxadc_phys_base, apmixed_phys_base;
	struct thermal_zone_device *tzdev;

	mt = devm_kzalloc(&pdev->dev, sizeof(*mt), GFP_KERNEL);
	if (!mt)
		return -ENOMEM;

	mt->clk_peri_therm = devm_clk_get(&pdev->dev, "therm");
	if (IS_ERR(mt->clk_peri_therm))
		return PTR_ERR(mt->clk_peri_therm);

	mt->clk_auxadc = devm_clk_get(&pdev->dev, "auxadc");
	if (IS_ERR(mt->clk_auxadc))
		return PTR_ERR(mt->clk_auxadc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mt->thermal_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mt->thermal_base))
		return PTR_ERR(mt->thermal_base);

	ret = mtk_thermal_get_calibration_data(&pdev->dev, mt);
	if (ret)
		return ret;

	mutex_init(&mt->lock);

	mt->dev = &pdev->dev;

	auxadc = of_parse_phandle(np, "mediatek,auxadc", 0);
	if (!auxadc) {
		dev_err(&pdev->dev, "missing auxadc node\n");
		return -ENODEV;
	}

	auxadc_phys_base = of_get_phys_base(auxadc);

	of_node_put(auxadc);

	if (auxadc_phys_base == OF_BAD_ADDR) {
		dev_err(&pdev->dev, "Can't get auxadc phys address\n");
		return -EINVAL;
	}

	apmixedsys = of_parse_phandle(np, "mediatek,apmixedsys", 0);
	if (!apmixedsys) {
		dev_err(&pdev->dev, "missing apmixedsys node\n");
		return -ENODEV;
	}

	apmixed_phys_base = of_get_phys_base(apmixedsys);

	of_node_put(apmixedsys);

	if (apmixed_phys_base == OF_BAD_ADDR) {
		dev_err(&pdev->dev, "Can't get auxadc phys address\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(mt->clk_auxadc);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable auxadc clk: %d\n", ret);
		return ret;
	}

	ret = device_reset(&pdev->dev);
	if (ret)
		goto err_disable_clk_auxadc;

	ret = clk_prepare_enable(mt->clk_peri_therm);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable peri clk: %d\n", ret);
		goto err_disable_clk_auxadc;
	}

	for (i = 0; i < MT8173_NUM_ZONES; i++)
		mtk_thermal_init_bank(mt, i, apmixed_phys_base,
				      auxadc_phys_base);

	platform_set_drvdata(pdev, mt);

	tzdev = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, mt,
						     &mtk_thermal_ops);
	if (IS_ERR(tzdev)) {
		ret = PTR_ERR(tzdev);
		goto err_disable_clk_peri_therm;
	}

	ret = mtk_svs_probe(pdev);
	if (ret == -EPROBE_DEFER) {
		goto err_disable_clk_peri_therm;
	}

	return 0;

err_disable_clk_peri_therm:
	clk_disable_unprepare(mt->clk_peri_therm);
err_disable_clk_auxadc:
	clk_disable_unprepare(mt->clk_auxadc);

	return ret;
}

static int mtk_thermal_remove(struct platform_device *pdev)
{
	struct mtk_thermal *mt = platform_get_drvdata(pdev);

	clk_disable_unprepare(mt->clk_peri_therm);
	clk_disable_unprepare(mt->clk_auxadc);

	return 0;
}

static const struct of_device_id mtk_thermal_of_match[] = {
	{
		.compatible = "mediatek,mt8173-thermal",
	}, {
	},
};

static struct platform_driver mtk_thermal_driver = {
	.probe = mtk_thermal_probe,
	.remove = mtk_thermal_remove,
	.driver = {
		.name = THERMAL_NAME,
		.of_match_table = mtk_thermal_of_match,
	},
};

module_platform_driver(mtk_thermal_driver);

MODULE_AUTHOR("Sascha Hauer <s.hauer@pengutronix.de>");
MODULE_AUTHOR("Hanyi Wu <hanyi.wu@mediatek.com>");
MODULE_DESCRIPTION("Mediatek thermal driver");
MODULE_LICENSE("GPL v2");
