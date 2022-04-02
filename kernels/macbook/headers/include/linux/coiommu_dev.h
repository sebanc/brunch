/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COIOMMU_DEV_H
#define __LINUX_COIOMMU_DEV_H

struct pin_pages_info {
	unsigned short	bdf;
	unsigned short	pad[3];
	unsigned long	nr_pages;
	uint64_t	pfn[];
};

struct coiommu_dev;
struct coiommu_dev_ops {
	int (*execute_request)(struct coiommu_dev *dev, unsigned long pfn, unsigned short bdf);
	int (*execute_requests)(struct coiommu_dev *dev, struct pin_pages_info *pin_info);
	void (*park_unpin)(struct coiommu_dev *dev, bool park);
};

extern void coiommu_init(unsigned short ep_count, unsigned short *endpoints);
extern int coiommu_enable_dtt(u64 *dtt_addr, u64 *dtt_level);
extern void coiommu_disable_dtt(void);
extern int coiommu_setup_dev_ops(const struct coiommu_dev_ops *ops, void *dev);

#define PCI_VENDOR_ID_COIOMMU	0x1234
#define PCI_DEVICE_ID_COIOMMU	0xabcd

#endif /* __LINUX_COIOMMU_DEV_H */
