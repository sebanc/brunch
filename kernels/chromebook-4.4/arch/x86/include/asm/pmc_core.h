/*
 * Intel Core SoC Power Management Controller Header File
 *
 * Copyright (c) 2016, Intel Corporation.
 * All Rights Reserved.
 *
 * Authors: Rajneesh Bhardwaj <rajneesh.bhardwaj@intel.com>
 *          Vishwanath Somayaji <vishwanath.somayaji@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _ASM_PMC_CORE_H
#define _ASM_PMC_CORE_H

/* API to read SLP_S0_RESIDENCY counter */
#ifdef CONFIG_INTEL_PMC_CORE
extern int intel_pmc_slp_s0_counter_read(u32 *data);
extern int intel_pkgc10_counter_read(u64 *data);
void pmc_core_ppfear_display(void);
#else
static inline int intel_pmc_slp_s0_counter_read(u32 *data) { return -EPERM; }
static inline int intel_pkgc10_counter_read(u64* data) { return -EPERM; }
static inline void pmc_core_ppfear_display(void) {};
#endif
#endif /* _ASM_PMC_CORE_H */
