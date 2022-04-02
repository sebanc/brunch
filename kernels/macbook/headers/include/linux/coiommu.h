/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COIOMMU_H
#define __LINUX_COIOMMU_H

#ifdef CONFIG_COIOMMU
extern int coiommu_configure(struct device *dev);
#else
static inline int coiommu_configure(struct device *dev) { return 0; }
#endif

#endif /* __LINUX_COIOMMU_H */
