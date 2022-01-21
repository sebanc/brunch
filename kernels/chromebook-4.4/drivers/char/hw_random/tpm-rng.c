/*
 * Copyright (C) 2012 Kent Yoder IBM Corporation
 *
 * HWRNG interfaces to pull RNG data from a TPM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/module.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>
#include <linux/tpm.h>

#define MODULE_NAME "tpm-rng"

struct tpm_rng_data {
	struct platform_device *pdev;
	struct hwrng rng;
	struct mutex access_mutex;
	bool suspended;
};

static struct platform_device *tpm_rng_pdev;

static int tpm_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	struct tpm_rng_data *trd = container_of(rng, struct tpm_rng_data, rng);
	int ret;

	mutex_lock(&trd->access_mutex);
	if (trd->suspended) {
		dev_warn(&trd->pdev->dev,
			 "blocking tpm_rng_read while suspended\n");
		ret = -EAGAIN;
	} else {
		ret = tpm_get_random(TPM_ANY_NUM, data, max);
	}
	mutex_unlock(&trd->access_mutex);

	return ret;
}

static int tpm_rng_probe(struct platform_device *pdev)
{
	struct tpm_rng_data *trd;
	int ret;
	u8 data[4];

	ret = tpm_get_random(TPM_ANY_NUM, data, sizeof(data));
	if (ret == -ENODEV)
		return -EPROBE_DEFER;

	trd = devm_kzalloc(&pdev->dev, sizeof(*trd), GFP_KERNEL);
	if (!trd)
		return -ENOMEM;
	trd->pdev = pdev;
	mutex_init(&trd->access_mutex);
	trd->rng.name = pdev->name;
	trd->rng.read = tpm_rng_read;
	trd->rng.quality = 1000;
	platform_set_drvdata(pdev, trd);
	return hwrng_register(&trd->rng);
}

static int tpm_rng_remove(struct platform_device *pdev)
{
	struct tpm_rng_data *trd = platform_get_drvdata(pdev);

	hwrng_unregister(&trd->rng);
	return 0;
}

#ifdef CONFIG_PM
static int tpm_rng_suspend(struct device *dev)
{
	struct tpm_rng_data *trd = dev_get_drvdata(dev);

	mutex_lock(&trd->access_mutex);
	trd->suspended = true;
	mutex_unlock(&trd->access_mutex);
	return 0;
}

static int tpm_rng_resume(struct device *dev)
{
	struct tpm_rng_data *trd = dev_get_drvdata(dev);

	mutex_lock(&trd->access_mutex);
	trd->suspended = false;
	mutex_unlock(&trd->access_mutex);
	return 0;
}

static SIMPLE_DEV_PM_OPS(tpm_rng_pm_ops, tpm_rng_suspend, tpm_rng_resume);
#endif /* CONFIG_PM */

static struct platform_driver tpm_rng_driver = {
	.probe		= tpm_rng_probe,
	.remove		= tpm_rng_remove,
	.driver		= {
		.name	= MODULE_NAME,
#ifdef CONFIG_PM
		.pm	= &tpm_rng_pm_ops,
#endif /* CONFIG_PM */
	},
};

struct platform_device_info tpm_rng_devinfo = {
	.name = MODULE_NAME,
	.id = PLATFORM_DEVID_NONE,
};

static int __init tpm_rng_init(void)
{
	int ret = platform_driver_register(&tpm_rng_driver);

	if (ret)
		return ret;

	tpm_rng_pdev = platform_device_register_full(&tpm_rng_devinfo);
	if (IS_ERR(tpm_rng_pdev)) {
		ret = PTR_ERR(tpm_rng_pdev);
		platform_driver_unregister(&tpm_rng_driver);
	}

	return ret;
}
late_initcall(tpm_rng_init);

static void __exit tpm_rng_exit(void)
{
	platform_driver_unregister(&tpm_rng_driver);
	if (!IS_ERR_OR_NULL(tpm_rng_pdev))
		platform_device_unregister(tpm_rng_pdev);
}
module_exit(tpm_rng_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Kent Yoder <key@linux.vnet.ibm.com>");
MODULE_DESCRIPTION("RNG driver for TPM devices");
