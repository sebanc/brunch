/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright(c) 2017-2021 Intel Corporation */

#ifndef __GNA_PCI_H__
#define __GNA_PCI_H__

struct pci_device_id;
struct pci_dev;

int gna_pci_probe(struct pci_dev *dev, const struct pci_device_id *id);

#endif /* __GNA_PCI_H__ */
