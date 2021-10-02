// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Intel ACPI privacy screen code
 *
 * Copyright © 2019 Google Inc.
 */

#include <linux/acpi.h>

#include "intel_privacy_screen.h"

#define CONNECTOR_DSM_REVID 1

#define CONNECTOR_DSM_FN_PRIVACY_ENABLE		2
#define CONNECTOR_DSM_FN_PRIVACY_DISABLE		3

static const guid_t drm_conn_dsm_guid =
	GUID_INIT(0xC7033113, 0x8720, 0x4CEB,
		  0x90, 0x90, 0x9D, 0x52, 0xB3, 0xE5, 0x2D, 0x73);

/* Makes _DSM call to set privacy screen status */
static void acpi_privacy_screen_call_dsm(acpi_handle conn_handle, u64 func)
{
	union acpi_object *obj;

	obj = acpi_evaluate_dsm(conn_handle, &drm_conn_dsm_guid,
				CONNECTOR_DSM_REVID, func, NULL);
	if (!obj) {
		DRM_DEBUG_DRIVER("failed to evaluate _DSM for fn %llx\n", func);
		return;
	}

	ACPI_FREE(obj);
}

void intel_privacy_screen_set_val(struct intel_connector *connector,
				  enum intel_privacy_screen_status val)
{
	acpi_handle acpi_handle = connector->acpi_handle;

	if (!acpi_handle)
		return;

	if (val == PRIVACY_SCREEN_DISABLED)
		acpi_privacy_screen_call_dsm(acpi_handle,
					     CONNECTOR_DSM_FN_PRIVACY_DISABLE);
	else if (val == PRIVACY_SCREEN_ENABLED)
		acpi_privacy_screen_call_dsm(acpi_handle,
					     CONNECTOR_DSM_FN_PRIVACY_ENABLE);
	else
		DRM_WARN("%s: Cannot set privacy screen to invalid val %u\n",
			 dev_name(connector->base.dev->dev), val);
}

bool intel_privacy_screen_present(struct intel_connector *connector)
{
	acpi_handle handle = connector->acpi_handle;

	if (!handle)
		return false;

	if (!acpi_check_dsm(handle, &drm_conn_dsm_guid,
			    CONNECTOR_DSM_REVID,
			    1 << CONNECTOR_DSM_FN_PRIVACY_ENABLE |
			    1 << CONNECTOR_DSM_FN_PRIVACY_DISABLE))
		return false;
	DRM_DEV_INFO(connector->base.dev->dev, "supports privacy screen\n");
	return true;
}
