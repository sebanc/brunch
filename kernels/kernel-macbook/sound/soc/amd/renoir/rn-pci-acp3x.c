// SPDX-License-Identifier: GPL-2.0+
//
// AMD Renoir ACP PCI Driver
//
//Copyright 2020 Advanced Micro Devices, Inc.

#include <linux/pci.h>
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include "rn_acp3x.h"

static int acp_power_gating;
module_param(acp_power_gating, int, 0644);
MODULE_PARM_DESC(acp_power_gating, "Enable acp power gating");

/**
 * dmic_acpi_check = -1 - Checks ACPI method to know DMIC hardware status runtime
 *                 = 0 - Skips the DMIC device creation and returns probe failure
 *                 = 1 - Assumes that platform has DMIC support and skips ACPI
 *                       method check
 */
//static int dmic_acpi_check = ACP_DMIC_AUTO;
static int dmic_acpi_check = 1;
module_param(dmic_acpi_check, bint, 0644);
MODULE_PARM_DESC(dmic_acpi_check, "checks Dmic hardware runtime");

struct acp_dev_data {
	void __iomem *acp_base;
	struct resource *res;
	bool acp3x_audio_mode;
	struct platform_device *pdev[ACP_DEVS];
	u32 pme_en;
};

static int rn_acp_power_on(void __iomem *acp_base)
{
	u32 val;
	int timeout;

	val = rn_readl(acp_base + ACP_PGFSM_STATUS);

	if (val == 0)
		return val;

	if ((val & ACP_PGFSM_STATUS_MASK) !=
				ACP_POWER_ON_IN_PROGRESS)
		rn_writel(ACP_PGFSM_CNTL_POWER_ON_MASK,
			  acp_base + ACP_PGFSM_CONTROL);
	timeout = 0;
	while (++timeout < 500) {
		val = rn_readl(acp_base + ACP_PGFSM_STATUS);
		if (!val)
			return 0;
		udelay(1);
	}
	return -ETIMEDOUT;
}

static int rn_acp_power_off(void __iomem *acp_base)
{
	u32 val;
	int timeout;

	rn_writel(ACP_PGFSM_CNTL_POWER_OFF_MASK,
		  acp_base + ACP_PGFSM_CONTROL);
	timeout = 0;
	while (++timeout < 500) {
		val = rn_readl(acp_base + ACP_PGFSM_STATUS);
		if ((val & ACP_PGFSM_STATUS_MASK) == ACP_POWERED_OFF)
			return 0;
		udelay(1);
	}
	return -ETIMEDOUT;
}

static int rn_acp_reset(void __iomem *acp_base)
{
	u32 val;
	int timeout;

	rn_writel(1, acp_base + ACP_SOFT_RESET);
	timeout = 0;
	while (++timeout < 500) {
		val = rn_readl(acp_base + ACP_SOFT_RESET);
		if (val & ACP_SOFT_RESET_SOFTRESET_AUDDONE_MASK)
			break;
		cpu_relax();
	}
	rn_writel(0, acp_base + ACP_SOFT_RESET);
	timeout = 0;
	while (++timeout < 500) {
		val = rn_readl(acp_base + ACP_SOFT_RESET);
		if (!val)
			return 0;
		cpu_relax();
	}
	return -ETIMEDOUT;
}

static void rn_acp_enable_interrupts(void __iomem *acp_base)
{
	rn_writel(0x01, acp_base + ACP_EXTERNAL_INTR_ENB);
}

static void rn_acp_disable_interrupts(void __iomem *acp_base)
{
	rn_writel(ACP_EXT_INTR_STAT_CLEAR_MASK, acp_base +
		  ACP_EXTERNAL_INTR_STAT);
	rn_writel(0x00, acp_base + ACP_EXTERNAL_INTR_CNTL);
	rn_writel(0x00, acp_base + ACP_EXTERNAL_INTR_ENB);
}

static int rn_acp_init(void __iomem *acp_base)
{
	int ret;

	/* power on */
	ret = rn_acp_power_on(acp_base);
	if (ret) {
		pr_err("ACP power on failed\n");
		return ret;
	}
	rn_writel(0x01, acp_base + ACP_CONTROL);
	/* Reset */
	ret = rn_acp_reset(acp_base);
	if (ret) {
		pr_err("ACP reset failed\n");
		return ret;
	}
	rn_writel(0x03, acp_base + ACP_CLKMUX_SEL);
	rn_acp_enable_interrupts(acp_base);
	return 0;
}

static int rn_acp_deinit(void __iomem *acp_base)
{
	int ret;

	rn_acp_disable_interrupts(acp_base);
	/* Reset */
	ret = rn_acp_reset(acp_base);
	if (ret) {
		pr_err("ACP reset failed\n");
		return ret;
	}
	rn_writel(0x00, acp_base + ACP_CLKMUX_SEL);
	rn_writel(0x00, acp_base + ACP_CONTROL);
	/* power off */
	if (acp_power_gating) {
		ret = rn_acp_power_off(acp_base);
		if (ret) {
			pr_err("ACP power off failed\n");
			return ret;
		}
	}
	return 0;
}

static irqreturn_t acp_irq_handler(int irq, void *dev_id)
{
	struct acp_dev_data *adata;
	struct i2s_dev_data *rn_i2s_data;
	struct pdm_dev_data *rn_pdm_data;
	u16 play_flag, cap_flag, err_flag;
	u32 val;
	u32 err;

	adata = dev_id;
	if (!adata)
		return IRQ_NONE;

	play_flag = 0;
	cap_flag = 0;
	err_flag = 0;
	err = 0;
	val = rn_readl(adata->acp_base + ACP_EXTERNAL_INTR_STAT);
	if (val & BIT(BT_TX_THRESHOLD)) {
		rn_i2s_data = dev_get_drvdata(&adata->pdev[0]->dev);
		rn_writel(BIT(BT_TX_THRESHOLD), adata->acp_base +
			  ACP_EXTERNAL_INTR_STAT);
		if (rn_i2s_data->play_stream)
			snd_pcm_period_elapsed(rn_i2s_data->play_stream);
		play_flag = 1;
	}
	if (val & BIT(I2S_TX_THRESHOLD)) {
		rn_i2s_data = dev_get_drvdata(&adata->pdev[0]->dev);
		rn_writel(BIT(I2S_TX_THRESHOLD),
			  adata->acp_base + ACP_EXTERNAL_INTR_STAT);
		if (rn_i2s_data->i2ssp_play_stream)
			snd_pcm_period_elapsed(rn_i2s_data->i2ssp_play_stream);
		play_flag = 1;
	}

	if (val & BIT(BT_RX_THRESHOLD)) {
		rn_i2s_data = dev_get_drvdata(&adata->pdev[0]->dev);
		rn_writel(BIT(BT_RX_THRESHOLD), adata->acp_base +
			  ACP_EXTERNAL_INTR_STAT);
		if (rn_i2s_data->capture_stream)
			snd_pcm_period_elapsed(rn_i2s_data->capture_stream);
		cap_flag = 1;
	}
	if (val & BIT(I2S_RX_THRESHOLD)) {
		rn_i2s_data = dev_get_drvdata(&adata->pdev[0]->dev);
		rn_writel(BIT(I2S_RX_THRESHOLD),
			  adata->acp_base + ACP_EXTERNAL_INTR_STAT);
		if (rn_i2s_data->i2ssp_capture_stream)
			snd_pcm_period_elapsed(rn_i2s_data->i2ssp_capture_stream);
		cap_flag = 1;
	}
	if (val & BIT(PDM_DMA_STAT)) {
		rn_pdm_data = dev_get_drvdata(&adata->pdev[1]->dev);
		rn_writel(BIT(PDM_DMA_STAT), adata->acp_base +
			  ACP_EXTERNAL_INTR_STAT);
		if (rn_pdm_data->capture_stream)
			snd_pcm_period_elapsed(rn_pdm_data->capture_stream);
		cap_flag = 1;
	}
	if (val & BIT(29)) {
		err = rn_readl(adata->acp_base + ACP_SW_I2S_ERROR_REASON);
		rn_writel(BIT(29), adata->acp_base + ACP_EXTERNAL_INTR_STAT);
		err_flag = 1;
	}
	if (play_flag | cap_flag | err_flag)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static int snd_rn_acp_probe(struct pci_dev *pci,
			    const struct pci_device_id *pci_id)
{
	struct acp_dev_data *adata;
	struct platform_device_info pdevinfo[ACP_DEVS];
#if defined(CONFIG_ACPI)
	acpi_handle handle;
	acpi_integer dmic_status;
#endif
	unsigned int irqflags;
	int ret, index;
	int acp_devs = 0x00;
	int val =  0x00;
	u32 addr;

	if (pci->revision != 0x01)
		return -ENODEV;
	if (pci_enable_device(pci)) {
		dev_err(&pci->dev, "pci_enable_device failed\n");
		return -ENODEV;
	}

	ret = pci_request_regions(pci, "AMD ACP3x audio");
	if (ret < 0) {
		dev_err(&pci->dev, "pci_request_regions failed\n");
		goto disable_pci;
	}

	adata = devm_kzalloc(&pci->dev, sizeof(struct acp_dev_data),
			     GFP_KERNEL);
	if (!adata) {
		ret = -ENOMEM;
		goto release_regions;
	}

	irqflags = IRQF_SHARED;

	addr = pci_resource_start(pci, 0);
	adata->acp_base = devm_ioremap(&pci->dev, addr,
				       pci_resource_len(pci, 0));
	if (!adata->acp_base) {
		ret = -ENOMEM;
		goto release_regions;
	}
	pci_set_master(pci);
	pci_set_drvdata(pci, adata);
	adata->pme_en = rn_readl(adata->acp_base + ACP_PME_EN);
	ret = rn_acp_init(adata->acp_base);
	if (ret)
		goto release_regions;

	if (!dmic_acpi_check) {
		ret = -ENODEV;
		goto de_init;
	} else if (dmic_acpi_check == ACP_DMIC_AUTO) {
#if defined(CONFIG_ACPI)
		handle = ACPI_HANDLE(&pci->dev);
		ret = acpi_evaluate_integer(handle, "_WOV", NULL, &dmic_status);
		if (ACPI_FAILURE(ret)) {
			ret = -EINVAL;
			goto de_init;
		}
		if (!dmic_status) {
			ret = -ENODEV;
			goto de_init;
		}
#endif
	}
	rn_writel(0x04, adata->acp_base + ACP_I2S_PIN_CONFIG);
	val = rn_readl(adata->acp_base + ACP_I2S_PIN_CONFIG);
	pr_info("ACP_I2S_PIN_CONFIG: %d\n", val);
	switch (val) {
	case I2S_MODE:
		adata->res = devm_kzalloc(&pci->dev,
					  sizeof(struct resource) * 3,
					  GFP_KERNEL);
		if (!adata->res) {
			ret = -ENOMEM;
			goto de_init;
		}

		adata->res[0].name = "acp_iomem";
		adata->res[0].flags = IORESOURCE_MEM;
		adata->res[0].start = addr;
		adata->res[0].end = addr + (ACP_REG_END - ACP_REG_START);

		adata->res[1].name = "acp_i2s_sp";
		adata->res[1].flags = IORESOURCE_MEM;
		adata->res[1].start = addr + ACP_I2STDM_REG_START;
		adata->res[1].end = addr + ACP_I2STDM_REG_END;

		adata->res[2].name = "acp_i2s_bt";
		adata->res[2].flags = IORESOURCE_MEM;
		adata->res[2].start = addr + ACP_BT_TDM_REG_START;
		adata->res[2].end = addr + ACP_BT_TDM_REG_END;

		adata->acp3x_audio_mode = ACP_I2S_MODE;

		memset(&pdevinfo, 0, sizeof(pdevinfo));
		pdevinfo[0].name = "acp_rn_i2s_dma";
		pdevinfo[0].id = 0;
		pdevinfo[0].parent = &pci->dev;
		pdevinfo[0].num_res = 3;
		pdevinfo[0].res = &adata->res[0];

		pdevinfo[1].name = "acp_rn_pdm_dma";
		pdevinfo[1].id = 1;
		pdevinfo[1].parent = &pci->dev;
		pdevinfo[1].num_res = 1;
		pdevinfo[1].res = &adata->res[0];

		pdevinfo[2].name = "acp_i2s_playcap";
		pdevinfo[2].id = 0;
		pdevinfo[2].parent = &pci->dev;
		pdevinfo[2].num_res = 1;
		pdevinfo[2].res = &adata->res[1];

		pdevinfo[3].name = "acp_i2s_playcap";
		pdevinfo[3].id = 1;
		pdevinfo[3].parent = &pci->dev;
		pdevinfo[3].num_res = 1;
		pdevinfo[3].res = &adata->res[2];

		pdevinfo[4].name = "dmic-codec";
		pdevinfo[4].id = 0;
		pdevinfo[4].parent = &pci->dev;
		acp_devs = 5;
		for (index = 0; index < acp_devs; index++) {
			adata->pdev[index] =
				platform_device_register_full(&pdevinfo[index]);
			if (IS_ERR(adata->pdev[index])) {
				dev_err(&pci->dev, "cannot register %s device\n",
					pdevinfo[index].name);
				ret = PTR_ERR(adata->pdev[index]);
				goto unregister_devs;
			}
		}
		break;
	default:
		adata->res = devm_kzalloc(&pci->dev,
					  sizeof(struct resource) * 2,
					  GFP_KERNEL);
		if (!adata->res) {
			ret = -ENOMEM;
			goto de_init;
		}
		adata->res[0].name = "acp_pdm_iomem";
		adata->res[0].flags = IORESOURCE_MEM;
		adata->res[0].start = addr;
		adata->res[0].end = addr + (ACP_REG_END - ACP_REG_START);
		adata->res[1].name = "acp_pdm_irq";
		adata->res[1].flags = IORESOURCE_IRQ;
		adata->res[1].start = pci->irq;
		adata->res[1].end = pci->irq;

		memset(&pdevinfo, 0, sizeof(pdevinfo));
		pdevinfo[0].name = "acp_rn_pdm_dma";
		pdevinfo[0].id = 0;
		pdevinfo[0].parent = &pci->dev;
		pdevinfo[0].num_res = 2;
		pdevinfo[0].res = adata->res;
		pdevinfo[0].data = &irqflags;
		pdevinfo[0].size_data = sizeof(irqflags);

		pdevinfo[1].name = "dmic-codec";
		pdevinfo[1].id = 0;
		pdevinfo[1].parent = &pci->dev;
		pdevinfo[2].name = "acp_pdm_mach";
		pdevinfo[2].id = 0;
		pdevinfo[2].parent = &pci->dev;
		acp_devs = 3;
		for (index = 0; index < acp_devs; index++) {
			adata->pdev[index] =
				platform_device_register_full(&pdevinfo[index]);
			if (IS_ERR(adata->pdev[index])) {
				dev_err(&pci->dev, "cannot register %s device\n",
					pdevinfo[index].name);
				ret = PTR_ERR(adata->pdev[index]);
				goto unregister_devs;
			}
		}
	}
	ret = devm_request_irq(&pci->dev, pci->irq, acp_irq_handler,
			       irqflags, "ACP_PCI_IRQ", adata);
	if (ret) {
		dev_err(&pci->dev, "ACP PCI IRQ request failed\n");
		return -ENODEV;
	}

	pm_runtime_set_autosuspend_delay(&pci->dev, ACP_SUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(&pci->dev);
	pm_runtime_put_noidle(&pci->dev);
	pm_runtime_allow(&pci->dev);
	return 0;

unregister_devs:
	for (index = 0; index < acp_devs; index++)
		platform_device_unregister(adata->pdev[index]);
de_init:
	if (rn_acp_deinit(adata->acp_base))
		dev_err(&pci->dev, "ACP de-init failed\n");
release_regions:
	pci_release_regions(pci);
disable_pci:
	pci_disable_device(pci);

	return ret;
}

static int snd_rn_acp_suspend(struct device *dev)
{
	int ret;
	struct acp_dev_data *adata;

	adata = dev_get_drvdata(dev);
	pr_info("%s\n", __func__);
	ret = rn_acp_deinit(adata->acp_base);
	if (ret)
		dev_err(dev, "ACP de-init failed\n");
	else
		dev_dbg(dev, "ACP de-initialized\n");

	return ret;
}

static int snd_rn_acp_resume(struct device *dev)
{
	int ret;
	struct acp_dev_data *adata;

	adata = dev_get_drvdata(dev);
	pr_info("%s\n", __func__);
	ret = rn_acp_init(adata->acp_base);
	if (ret) {
		dev_err(dev, "ACP init failed\n");
		return ret;
	}
	return 0;
}

static const struct dev_pm_ops rn_acp_pm = {
	.runtime_suspend = snd_rn_acp_suspend,
	.runtime_resume =  snd_rn_acp_resume,
	.suspend = snd_rn_acp_suspend,
	.resume =	snd_rn_acp_resume,
};

static void snd_rn_acp_remove(struct pci_dev *pci)
{
	struct acp_dev_data *adata;
	int ret, index;
	int acp_devs = 0x00;
	u32 val = 0x00;

	adata = pci_get_drvdata(pci);

	val = rn_readl(adata->acp_base + ACP_I2S_PIN_CONFIG);
	switch (val) {
	case I2S_MODE:
		acp_devs = 5;
		break;
	default:
		acp_devs = 3;
	}
	for (index = 0; index < acp_devs; index++)
		platform_device_unregister(adata->pdev[index]);
	ret = rn_acp_deinit(adata->acp_base);
	if (ret)
		dev_err(&pci->dev, "ACP de-init failed\n");
	pm_runtime_forbid(&pci->dev);
	pm_runtime_get_noresume(&pci->dev);
	pci_release_regions(pci);
	pci_disable_device(pci);
}

static const struct pci_device_id snd_rn_acp_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, ACP_DEVICE_ID),
	.class = PCI_CLASS_MULTIMEDIA_OTHER << 8,
	.class_mask = 0xffffff },
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, snd_rn_acp_ids);

static struct pci_driver rn_acp_driver  = {
	.name = KBUILD_MODNAME,
	.id_table = snd_rn_acp_ids,
	.probe = snd_rn_acp_probe,
	.remove = snd_rn_acp_remove,
	.driver = {
		.pm = &rn_acp_pm,
	}
};

module_pci_driver(rn_acp_driver);

MODULE_AUTHOR("Vijendar.Mukunda@amd.com");
MODULE_DESCRIPTION("AMD ACP Renoir PCI driver");
MODULE_LICENSE("GPL v2");
