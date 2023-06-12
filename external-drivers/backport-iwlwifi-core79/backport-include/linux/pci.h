#ifndef _BACKPORT_LINUX_PCI_H
#define _BACKPORT_LINUX_PCI_H
#include_next <linux/pci.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,4,0)
#include <linux/pci-aspm.h>
#endif



#if LINUX_VERSION_IS_LESS(4,8,0)
#define pci_alloc_irq_vectors LINUX_BACKPORT(pci_alloc_irq_vectors)
#ifdef CONFIG_PCI_MSI
int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
		unsigned int max_vecs, unsigned int flags);
#else
static inline int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
		unsigned int max_vecs, unsigned int flags)
{ return -ENOSYS; }
#endif
#endif

#if LINUX_VERSION_IS_LESS(4,8,0)
#define pci_free_irq_vectors LINUX_BACKPORT(pci_free_irq_vectors)
static inline void pci_free_irq_vectors(struct pci_dev *dev)
{
}
#endif


#if LINUX_VERSION_IS_LESS(4,9,0) &&			\
	!LINUX_VERSION_IN_RANGE(4,4,37, 4,5,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,8,13, 4,9,0)

static inline struct pci_dev *pcie_find_root_port(struct pci_dev *dev)
{
	while (1) {
		if (!pci_is_pcie(dev))
			break;
		if (pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT)
			return dev;
		if (!dev->bus->self)
			break;
		dev = dev->bus->self;
	}
	return NULL;
}

#endif/* <4.9.0 but not >= 4.4.37, 4.8.13 */

#ifndef PCI_IRQ_LEGACY
#define PCI_IRQ_LEGACY		(1 << 0) /* Allow legacy interrupts */
#define PCI_IRQ_MSI		(1 << 1) /* Allow MSI interrupts */
#define PCI_IRQ_MSIX		(1 << 2) /* Allow MSI-X interrupts */
#define PCI_IRQ_ALL_TYPES \
	(PCI_IRQ_LEGACY | PCI_IRQ_MSI | PCI_IRQ_MSIX)
#endif

#if defined(CONFIG_PCI)
#if LINUX_VERSION_IS_LESS(5,3,0)
static inline int
backport_pci_disable_link_state(struct pci_dev *pdev, int state)
{
	u16 aspmc;

	pci_disable_link_state(pdev, state);

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &aspmc);
	if ((state & PCIE_LINK_STATE_L0S) &&
	    (aspmc & PCI_EXP_LNKCTL_ASPM_L0S))
		return -EPERM;

	if ((state & PCIE_LINK_STATE_L1) &&
	    (aspmc & PCI_EXP_LNKCTL_ASPM_L1))
		return -EPERM;

	return 0;
}
#define pci_disable_link_state LINUX_BACKPORT(pci_disable_link_state)

#endif /* < 5.3 */
#endif /* defined(CONFIG_PCI) */

#endif /* _BACKPORT_LINUX_PCI_H */
