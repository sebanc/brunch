/*
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * This file contains common code for devices with Cr50 firmware.
 */

#include <linux/suspend.h>
#include "cr50.h"

#ifdef CONFIG_PM_SLEEP
int cr50_resume(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);

	clear_bit(0, &chip->is_suspended);

	if (pm_suspend_via_firmware())
		return tpm_pm_resume(dev);
	else
		return 0;
}
EXPORT_SYMBOL(cr50_resume);

int cr50_suspend(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);

	if (pm_suspend_via_firmware()) {
		return tpm_pm_suspend(dev);
	} else {
		set_bit(0, &chip->is_suspended);
		return 0;
	}
}
EXPORT_SYMBOL(cr50_suspend);
#endif /* CONFIG_PM_SLEEP */
