// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/*
 * Copyright 2021 Google LLC
 */

/**
 * Power management configuration
 *
 * Attached value: pointer to @ref kbase_pm_callback_conf
 * Default value: See @ref kbase_pm_callback_conf
 */
#define POWER_MANAGEMENT_CALLBACKS (NULL)

/**
 * Platform specific configuration functions
 *
 * Attached value: pointer to @ref kbase_platform_funcs_conf
 * Default value: See @ref kbase_platform_funcs_conf
 */
#define PLATFORM_FUNCS (NULL)

extern struct kbase_pm_callback_conf mt8183_pm_callbacks;
extern struct kbase_platform_funcs_conf mt8183_platform_funcs;

extern struct kbase_pm_callback_conf mt8192_pm_callbacks;
extern struct kbase_platform_funcs_conf mt8192_platform_funcs;

extern struct kbase_pm_callback_conf mt8195_pm_callbacks;
extern struct kbase_platform_funcs_conf mt8195_platform_funcs;

extern const struct of_device_id kbase_dt_ids[];
