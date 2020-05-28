/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright © 2019 Google Inc.
 */

#ifndef __DRM_PRIVACY_SCREEN_H__
#define __DRM_PRIVACY_SCREEN_H__

#include "intel_drv.h"

#ifdef CONFIG_ACPI
bool intel_privacy_screen_present(struct intel_connector *connector);
void intel_privacy_screen_set_val(struct intel_connector *connector,
				  enum intel_privacy_screen_status val);
#else
static bool intel_privacy_screen_present(struct intel_connector *connector)
{
	return false;
}

static void
intel_privacy_screen_set_val(struct intel_connector *connector,
			     enum intel_privacy_screen_status val)
{ }
#endif /* CONFIG_ACPI */

#endif /* __DRM_PRIVACY_SCREEN_H__ */
