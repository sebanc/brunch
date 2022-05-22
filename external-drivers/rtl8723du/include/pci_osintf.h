/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __PCI_OSINTF_H
#define __PCI_OSINTF_H

#ifdef RTK_129X_PLATFORM
#define PCIE_SLOT1_MEM_START	0x9804F000
#define PCIE_SLOT1_MEM_LEN	0x1000
#define PCIE_SLOT1_CTRL_START	0x9804EC00

#define PCIE_SLOT2_MEM_START	0x9803C000
#define PCIE_SLOT2_MEM_LEN	0x1000
#define PCIE_SLOT2_CTRL_START	0x9803BC00

#define PCIE_MASK_OFFSET	0x100 /* mask offset from CTRL_START */
#define PCIE_TRANSLATE_OFFSET	0x104 /* translate offset from CTRL_START */
#endif

#define PCI_BC_CLK_REQ		BIT0
#define PCI_BC_ASPM_L0s		BIT1
#define PCI_BC_ASPM_L1		BIT2
#define PCI_BC_ASPM_L1Off	BIT3
//#define PCI_BC_ASPM_LTR	BIT4
//#define PCI_BC_ASPM_OBFF	BIT5

void	rtw_pci_disable_aspm(struct adapter *adapt);
void	rtw_pci_enable_aspm(struct adapter *adapt);
void	PlatformClearPciPMEStatus(struct adapter * Adapter);
void	rtw_pci_aspm_config(struct adapter *adapt);
void	rtw_pci_aspm_config_l1off_general(struct adapter *adapt, u8 eanble);
#ifdef CONFIG_PCI_DYNAMIC_ASPM
void	rtw_pci_aspm_config_dynamic_l1_ilde_time(struct adapter *adapt);
#endif
#ifdef CONFIG_64BIT_DMA
	u8	PlatformEnableDMA64(struct adapter * Adapter);
#endif

#endif
