/*
 * Surface Lid driver to enable wakeup from suspend via the lid.
 */

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>


struct sid_lid_device {
	const char *acpi_path;
	const u32 gpe_number;
};


static const struct sid_lid_device lid_device_l17 = {
	.acpi_path = "\\_SB.LID0",
	.gpe_number = 0x17,
};

static const struct sid_lid_device lid_device_l4D = {
	.acpi_path = "\\_SB.LID0",
	.gpe_number = 0x4D,
};

static const struct sid_lid_device lid_device_l4F = {
	.acpi_path = "\\_SB.LID0",
	.gpe_number = 0x4F,
};

static const struct sid_lid_device lid_device_l57 = {
	.acpi_path = "\\_SB.LID0",
	.gpe_number = 0x57,
};


static const struct dmi_system_id dmi_lid_device_table[] = {
	{
		.ident = "Surface Pro 4",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Pro 4"),
		},
		.driver_data = (void *)&lid_device_l17,
	},
	{
		.ident = "Surface Pro 5",
		.matches = {
			/* match for SKU here due to generic product name "Surface Pro" */
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "Surface_Pro_1796"),
		},
		.driver_data = (void *)&lid_device_l4F,
	},
	{
		.ident = "Surface Pro 5 (LTE)",
		.matches = {
			/* match for SKU here due to generic product name "Surface Pro" */
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "Surface_Pro_1807"),
		},
		.driver_data = (void *)&lid_device_l4F,
	},
	{
		.ident = "Surface Pro 6",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Pro 6"),
		},
		.driver_data = (void *)&lid_device_l4F,
	},
	{
		.ident = "Surface Pro 7",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Pro 7"),
		},
		.driver_data = (void *)&lid_device_l4D,
	},
	{
		.ident = "Surface Book 1",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Book"),
		},
		.driver_data = (void *)&lid_device_l17,
	},
	{
		.ident = "Surface Book 2",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Book 2"),
		},
		.driver_data = (void *)&lid_device_l17,
	},
	{
		.ident = "Surface Laptop 1",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Laptop"),
		},
		.driver_data = (void *)&lid_device_l57,
	},
	{
		.ident = "Surface Laptop 2",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Surface Laptop 2"),
		},
		.driver_data = (void *)&lid_device_l57,
	},
	{
		.ident = "Surface Laptop 3 (13\")",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "Surface_Laptop_3_1867:1868"),
		},
		.driver_data = (void *)&lid_device_l4D,
	},
	{ }
};


static int sid_lid_enable_wakeup(const struct sid_lid_device *dev, bool enable)
{
	int action = enable ? ACPI_GPE_ENABLE : ACPI_GPE_DISABLE;
	int status;

	status = acpi_set_gpe_wake_mask(NULL, dev->gpe_number, action);
	if (status)
		return -EFAULT;

	return 0;
}


static int surface_sam_sid_gpelid_suspend(struct device *dev)
{
	const struct sid_lid_device *ldev = dev_get_drvdata(dev);
	return sid_lid_enable_wakeup(ldev, true);
}

static int surface_sam_sid_gpelid_resume(struct device *dev)
{
	const struct sid_lid_device *ldev = dev_get_drvdata(dev);
	return sid_lid_enable_wakeup(ldev, false);
}

static SIMPLE_DEV_PM_OPS(surface_sam_sid_gpelid_pm,
                         surface_sam_sid_gpelid_suspend,
                         surface_sam_sid_gpelid_resume);


static int surface_sam_sid_gpelid_probe(struct platform_device *pdev)
{
	const struct dmi_system_id *match;
        struct sid_lid_device *dev;
	acpi_handle lid_handle;
	int status;

	match = dmi_first_match(dmi_lid_device_table);
	if (!match)
		return -ENODEV;

        dev = match->driver_data;
        if (!dev)
                return -ENODEV;

	status = acpi_get_handle(NULL, (acpi_string)dev->acpi_path, &lid_handle);
	if (status)
		return -EFAULT;

	status = acpi_setup_gpe_for_wake(lid_handle, NULL, dev->gpe_number);
	if (status)
		return -EFAULT;

	status = acpi_enable_gpe(NULL, dev->gpe_number);
	if (status)
		return -EFAULT;

	status = sid_lid_enable_wakeup(dev, false);
	if (status) {
		acpi_disable_gpe(NULL, dev->gpe_number);
                return status;
        }

        platform_set_drvdata(pdev, dev);
        return 0;
}

static int surface_sam_sid_gpelid_remove(struct platform_device *pdev)
{
	struct sid_lid_device *dev = platform_get_drvdata(pdev);

	/* restore default behavior without this module */
	sid_lid_enable_wakeup(dev, false);
        acpi_disable_gpe(NULL, dev->gpe_number);

        platform_set_drvdata(pdev, NULL);
        return 0;
}

static struct platform_driver surface_sam_sid_gpelid = {
	.probe = surface_sam_sid_gpelid_probe,
	.remove = surface_sam_sid_gpelid_remove,
	.driver = {
		.name = "surface_sam_sid_gpelid",
		.pm = &surface_sam_sid_gpelid_pm,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_platform_driver(surface_sam_sid_gpelid);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Surface Lid Driver for 5th Generation Surface Devices");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:surface_sam_sid_gpelid");
