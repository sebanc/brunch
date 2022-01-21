/*
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd
 * Author: Lin Huang <hl@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/of.h>
#include <soc/rockchip/rk3399_dmc.h>

#include "rockchip-dfi.h"
#include "../governor.h"

#define RK3399_DMC_NUM_CH	2

/* DDRMON_CTRL */
#define DDRMON_CTRL	0x04
#define CLR_DDRMON_CTRL	(0x1f0000 << 0)
#define LPDDR4_EN	(0x10001 << 4)
#define HARDWARE_EN	(0x10001 << 3)
#define HARDWARE_DIS	(0x10000 << 3)
#define LPDDR3_EN	(0x10001 << 2)
#define TIME_CNT_EN	(0x10001 << 0)
#define TIME_CNT_DIS	(0x10000 << 0)

/* DDRMON_INT_STATUS */
#define CH0_BELOW_INT		BIT(0)
#define CH0_OVER_INT		BIT(1)
#define CH1_BELOW_INT		BIT(2)
#define CH1_OVER_INT		BIT(3)

/*
 * Don't use the CH1_BELOW_INT. We need both channels to be below the threshold
 * to trigger a transition, so we can reduce spurious interrupts by just
 * tracking one channel for the below interrupt.
 */
#define CH_THRESHOLD_MASK	(CH0_BELOW_INT | CH0_OVER_INT | CH1_OVER_INT)

#define DDRMON_INT_STATUS		0x08
#define DDRMON_INT_MASK			0x0c
#define DDRMON_TIMER_COUNT		0x10
#define DDRMON_FLOOR_NUM		0x14
#define DDRMON_TOP_NUM			0x18
#define DDRMON_CH0_COUNT_NUM		0x28
#define DDRMON_CH0_DFI_ACCESS_NUM	0x2c
#define DDRMON_CH1_COUNT_NUM		0x3c
#define DDRMON_CH1_DFI_ACCESS_NUM	0x40

/* pmu grf */
#define PMUGRF_OS_REG2	0x308
#define DDRTYPE_SHIFT	13
#define DDRTYPE_MASK	7

/* DDRMON_TIMER_COUNT is based on 24M, 119047 * (1 / 24000000) = 50ms */
#define TIMER_MS		50
#define CLK1M_CYCLE_TO_1MS	1000

enum {
	DDR3 = 3,
	LPDDR3 = 6,
	LPDDR4 = 7,
	UNUSED = 0xFF
};

struct dmc_usage {
	u32 access;
	u32 total;
};

/*
 * The dfi controller can monitor DDR load. It has an upper and lower threshold
 * for the operating points. Whenever the usage leaves these bounds an event is
 * generated to indicate the DDR frequency should be changed.
 */
struct rockchip_dfi {
	struct devfreq_event_dev *edev;
	struct devfreq_event_desc *desc;
	struct dmc_usage ch_usage[RK3399_DMC_NUM_CH];
	struct device *dev;
	void __iomem *regs;
	struct regmap *regmap_pmu;
	struct clk *clk;
	struct devfreq *devfreq;
	struct mutex devfreq_lock;
	unsigned int top;
	unsigned int floor;
};

static unsigned int rockchip_dfi_calc_threshold_num(unsigned long rate,
						    unsigned int percent)
{
	u64 total_num = TIMER_MS * (rate / USEC_PER_SEC) * CLK1M_CYCLE_TO_1MS;
	unsigned int val = div_u64((u64)(percent * total_num), 100);

	/* Cycle is burst4, so divide by 4 */
	val /= 4;
	return val;
}

int rockchip_dfi_calc_top_threshold(struct devfreq_event_dev *edev,
				    unsigned long rate,
				    unsigned int percent)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);

	info->top = rockchip_dfi_calc_threshold_num(rate, percent);
	return 0;
}
EXPORT_SYMBOL_GPL(rockchip_dfi_calc_top_threshold);

int rockchip_dfi_calc_floor_threshold(struct devfreq_event_dev *edev,
				      unsigned long rate,
				      unsigned int percent)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);

	info->floor = rockchip_dfi_calc_threshold_num(rate, percent);
	return 0;
}
EXPORT_SYMBOL_GPL(rockchip_dfi_calc_floor_threshold);

static void rockchip_dfi_write_threshold(struct devfreq_event_dev *edev)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);
	void __iomem *dfi_regs = info->regs;
	u32 val = readl_relaxed(dfi_regs + DDRMON_INT_MASK);

	writel_relaxed(info->top, dfi_regs + DDRMON_TOP_NUM);
	writel_relaxed(info->floor, dfi_regs + DDRMON_FLOOR_NUM);

	if (info->top == 0)
		val &= ~(CH0_OVER_INT | CH1_OVER_INT);
	else
		val |= CH0_OVER_INT | CH1_OVER_INT;

	if (info->floor == 0)
		val &= ~CH0_BELOW_INT;
	else
		val |= CH0_BELOW_INT;

	writel_relaxed(val, dfi_regs + DDRMON_INT_MASK);
}

static void rockchip_dfi_start_hardware_counter(struct devfreq_event_dev *edev)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);
	void __iomem *dfi_regs = info->regs;
	u32 val;
	u32 ddr_type;

	/* get ddr type */
	regmap_read(info->regmap_pmu, PMUGRF_OS_REG2, &val);
	ddr_type = (val >> DDRTYPE_SHIFT) & DDRTYPE_MASK;

	/* clear DDRMON_CTRL setting */
	writel_relaxed(CLR_DDRMON_CTRL, dfi_regs + DDRMON_CTRL);

	/* set ddr type to dfi */
	if (ddr_type == LPDDR3)
		writel_relaxed(LPDDR3_EN, dfi_regs + DDRMON_CTRL);
	else if (ddr_type == LPDDR4)
		writel_relaxed(LPDDR4_EN, dfi_regs + DDRMON_CTRL);

	writel_relaxed(HARDWARE_EN, dfi_regs + DDRMON_CTRL);

	/* DDRMON_TIMER_COUNT count based on 24M */
	val = TIMER_MS * 24 * CLK1M_CYCLE_TO_1MS;
	writel_relaxed(val, dfi_regs + DDRMON_TIMER_COUNT);

	/* enable chx_over_init and ch0_below_init */
	writel_relaxed(~CH_THRESHOLD_MASK, dfi_regs + DDRMON_INT_MASK);

	/* clear irq status */
	writel_relaxed(0x0000ffff, dfi_regs + DDRMON_INT_STATUS);

	rockchip_dfi_write_threshold(edev);
	writel_relaxed(TIME_CNT_EN, dfi_regs + DDRMON_CTRL);
}

static void rockchip_dfi_stop_hardware_counter(struct devfreq_event_dev *edev)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);
	void __iomem *dfi_regs = info->regs;

	writel_relaxed(CLR_DDRMON_CTRL, dfi_regs + DDRMON_CTRL);
	writel_relaxed(0, dfi_regs + DDRMON_TOP_NUM);
	writel_relaxed(0, dfi_regs + DDRMON_FLOOR_NUM);
	writel_relaxed(0, dfi_regs + DDRMON_TIMER_COUNT);
	writel_relaxed(0x0000ffff, dfi_regs + DDRMON_INT_STATUS);
}

static int rockchip_dfi_get_busier_ch(struct devfreq_event_dev *edev)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);
	u32 tmp, max = 0;
	u32 i, busier_ch = 0;
	void __iomem *dfi_regs = info->regs;

	/* Find out which channel is busier */
	for (i = 0; i < RK3399_DMC_NUM_CH; i++) {
		info->ch_usage[i].access = readl_relaxed(dfi_regs +
				DDRMON_CH0_DFI_ACCESS_NUM + i * 20) * 4;
		info->ch_usage[i].total = readl_relaxed(dfi_regs +
				DDRMON_CH0_COUNT_NUM + i * 20);
		tmp = info->ch_usage[i].access;
		if (tmp > max) {
			busier_ch = i;
			max = tmp;
		}
	}

	return busier_ch;
}

static int rockchip_dfi_disable(struct devfreq_event_dev *edev)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);

	rockchip_dfi_stop_hardware_counter(edev);
	clk_disable_unprepare(info->clk);

	return 0;
}

static int rockchip_dfi_enable(struct devfreq_event_dev *edev)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);
	int ret;

	ret = clk_prepare_enable(info->clk);
	if (ret) {
		dev_err(&edev->dev, "failed to enable dfi clk: %d\n", ret);
		return ret;
	}

	rockchip_dfi_start_hardware_counter(edev);

	return 0;
}

static int rockchip_dfi_set_event(struct devfreq_event_dev *edev)
{
	rockchip_dfi_write_threshold(edev);
	return 0;
}

static int rockchip_dfi_get_event(struct devfreq_event_dev *edev,
				  struct devfreq_event_data *edata)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);
	int busier_ch;

	busier_ch = rockchip_dfi_get_busier_ch(edev);

	edata->load_count = info->ch_usage[busier_ch].access;
	edata->total_count = info->ch_usage[busier_ch].total;

	return 0;
}

static irqreturn_t ddrmon_thread_isr(int irq, void *data)
{
	struct rockchip_dfi *info = data;
	struct devfreq *devfreq;

	mutex_lock(&info->devfreq_lock);

	if (!info->devfreq)
		goto out;

	devfreq = info->devfreq;
	mutex_lock(&devfreq->lock);
	update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);

out:
	mutex_unlock(&info->devfreq_lock);

	return IRQ_HANDLED;
}

static irqreturn_t ddrmon_isr(int irq, void *dev_id)
{
	struct rockchip_dfi *info = dev_id;
	void __iomem *dfi_regs = info->regs;
	irqreturn_t ret = IRQ_NONE;
	u32 val;

	val = readl_relaxed(dfi_regs + DDRMON_INT_STATUS);
	if (val & CH_THRESHOLD_MASK) {
		ret = IRQ_WAKE_THREAD;
		rockchip_dfi_get_busier_ch(info->edev);
	}

	/* clear irq status */
	writel_relaxed(0x0000ffff, dfi_regs + DDRMON_INT_STATUS);
	return ret;
}

void rockchip_dfi_set_devfreq(struct devfreq_event_dev *edev,
			      struct devfreq *devfreq)
{
	struct rockchip_dfi *info = devfreq_event_get_drvdata(edev);

	mutex_lock(&info->devfreq_lock);
	info->devfreq = devfreq;
	mutex_unlock(&info->devfreq_lock);
}
EXPORT_SYMBOL_GPL(rockchip_dfi_set_devfreq);

static const struct devfreq_event_ops rockchip_dfi_ops = {
	.disable = rockchip_dfi_disable,
	.enable = rockchip_dfi_enable,
	.get_event = rockchip_dfi_get_event,
	.set_event = rockchip_dfi_set_event,
};

static const struct of_device_id rockchip_dfi_id_match[] = {
	{ .compatible = "rockchip,rk3399-dfi" },
	{ },
};

static int rockchip_dfi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rockchip_dfi *data;
	struct resource *res;
	struct devfreq_event_desc *desc;
	struct device_node *np = pdev->dev.of_node, *node;
	int irq, err;

	data = devm_kzalloc(dev, sizeof(struct rockchip_dfi), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->regs))
		return PTR_ERR(data->regs);

	data->clk = devm_clk_get(dev, "pclk_ddr_mon");
	if (IS_ERR(data->clk)) {
		dev_err(dev, "Cannot get the clk dmc_clk\n");
		return PTR_ERR(data->clk);
	};

	/* try to find the optional reference to the pmu syscon */
	node = of_parse_phandle(np, "rockchip,pmu", 0);
	if (node) {
		data->regmap_pmu = syscon_node_to_regmap(node);
		if (IS_ERR(data->regmap_pmu))
			return PTR_ERR(data->regmap_pmu);
	}
	data->dev = dev;
	mutex_init(&data->devfreq_lock);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->ops = &rockchip_dfi_ops;
	desc->driver_data = data;
	desc->name = np->name;
	data->desc = desc;

	data->edev = devm_devfreq_event_add_edev(&pdev->dev, desc);
	if (IS_ERR(data->edev)) {
		dev_err(&pdev->dev,
			"failed to add devfreq-event device\n");
		return PTR_ERR(data->edev);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return irq;
	}

	platform_set_drvdata(pdev, data);
	err = devm_request_threaded_irq(&pdev->dev, irq, ddrmon_isr,
					ddrmon_thread_isr, 0, dev_name(dev),
					data);
	if (err) {
		dev_err(&pdev->dev, "Interrupt request failed\n");
		return err;
	}

	return 0;
}

static struct platform_driver rockchip_dfi_driver = {
	.probe	= rockchip_dfi_probe,
	.driver = {
		.name	= "rockchip-dfi",
		.of_match_table = rockchip_dfi_id_match,
	},
};
module_platform_driver(rockchip_dfi_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Lin Huang <hl@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip DFI driver");
