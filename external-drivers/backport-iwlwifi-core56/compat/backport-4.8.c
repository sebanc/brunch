/*
 * Copyright(c) 2017 Intel Deutschland GmbH
 *
 * Backport functionality introduced in Linux 4.8.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/pci.h>

int cdc_parse_cdc_header(struct usb_cdc_parsed_header *hdr,
			 struct usb_interface *intf,
			 u8 *buffer, int buflen)
{
	/* duplicates are ignored */
	struct usb_cdc_union_desc *union_header = NULL;

	/* duplicates are not tolerated */
	struct usb_cdc_header_desc *header = NULL;
	struct usb_cdc_ether_desc *ether = NULL;
	struct usb_cdc_mdlm_detail_desc *detail = NULL;
	struct usb_cdc_mdlm_desc *desc = NULL;

	unsigned int elength;
	int cnt = 0;

	memset(hdr, 0x00, sizeof(struct usb_cdc_parsed_header));
	hdr->phonet_magic_present = false;
	while (buflen > 0) {
		elength = buffer[0];
		if (!elength) {
			dev_err(&intf->dev, "skipping garbage byte\n");
			elength = 1;
			goto next_desc;
		}
		if (buffer[1] != USB_DT_CS_INTERFACE) {
			dev_err(&intf->dev, "skipping garbage\n");
			goto next_desc;
		}

		switch (buffer[2]) {
		case USB_CDC_UNION_TYPE: /* we've found it */
			if (elength < sizeof(struct usb_cdc_union_desc))
				goto next_desc;
			if (union_header) {
				dev_err(&intf->dev, "More than one union descriptor, skipping ...\n");
				goto next_desc;
			}
			union_header = (struct usb_cdc_union_desc *)buffer;
			break;
		case USB_CDC_COUNTRY_TYPE:
			if (elength < sizeof(struct usb_cdc_country_functional_desc))
				goto next_desc;
			hdr->usb_cdc_country_functional_desc =
				(struct usb_cdc_country_functional_desc *)buffer;
			break;
		case USB_CDC_HEADER_TYPE:
			if (elength != sizeof(struct usb_cdc_header_desc))
				goto next_desc;
			if (header)
				return -EINVAL;
			header = (struct usb_cdc_header_desc *)buffer;
			break;
		case USB_CDC_ACM_TYPE:
			if (elength < sizeof(struct usb_cdc_acm_descriptor))
				goto next_desc;
			hdr->usb_cdc_acm_descriptor =
				(struct usb_cdc_acm_descriptor *)buffer;
			break;
		case USB_CDC_ETHERNET_TYPE:
			if (elength != sizeof(struct usb_cdc_ether_desc))
				goto next_desc;
			if (ether)
				return -EINVAL;
			ether = (struct usb_cdc_ether_desc *)buffer;
			break;
		case USB_CDC_CALL_MANAGEMENT_TYPE:
			if (elength < sizeof(struct usb_cdc_call_mgmt_descriptor))
				goto next_desc;
			hdr->usb_cdc_call_mgmt_descriptor =
				(struct usb_cdc_call_mgmt_descriptor *)buffer;
			break;
		case USB_CDC_DMM_TYPE:
			if (elength < sizeof(struct usb_cdc_dmm_desc))
				goto next_desc;
			hdr->usb_cdc_dmm_desc =
				(struct usb_cdc_dmm_desc *)buffer;
			break;
		case USB_CDC_MDLM_TYPE:
			if (elength < sizeof(struct usb_cdc_mdlm_desc *))
				goto next_desc;
			if (desc)
				return -EINVAL;
			desc = (struct usb_cdc_mdlm_desc *)buffer;
			break;
		case USB_CDC_MDLM_DETAIL_TYPE:
			if (elength < sizeof(struct usb_cdc_mdlm_detail_desc *))
				goto next_desc;
			if (detail)
				return -EINVAL;
			detail = (struct usb_cdc_mdlm_detail_desc *)buffer;
			break;
		case USB_CDC_NCM_TYPE:
			if (elength < sizeof(struct usb_cdc_ncm_desc))
				goto next_desc;
			hdr->usb_cdc_ncm_desc = (struct usb_cdc_ncm_desc *)buffer;
			break;
		case USB_CDC_MBIM_TYPE:
			if (elength < sizeof(struct usb_cdc_mbim_desc))
				goto next_desc;

			hdr->usb_cdc_mbim_desc = (struct usb_cdc_mbim_desc *)buffer;
			break;
		case USB_CDC_MBIM_EXTENDED_TYPE:
			if (elength < sizeof(struct usb_cdc_mbim_extended_desc))
				break;
			hdr->usb_cdc_mbim_extended_desc =
				(struct usb_cdc_mbim_extended_desc *)buffer;
			break;
		case CDC_PHONET_MAGIC_NUMBER:
			hdr->phonet_magic_present = true;
			break;
		default:
			/*
			 * there are LOTS more CDC descriptors that
			 * could legitimately be found here.
			 */
			dev_dbg(&intf->dev, "Ignoring descriptor: type %02x, length %ud\n",
					buffer[2], elength);
			goto next_desc;
		}
		cnt++;
next_desc:
		buflen -= elength;
		buffer += elength;
	}
	hdr->usb_cdc_union_desc = union_header;
	hdr->usb_cdc_header_desc = header;
	hdr->usb_cdc_mdlm_detail_desc = detail;
	hdr->usb_cdc_mdlm_desc = desc;
	hdr->usb_cdc_ether_desc = ether;
	return cnt;
}
EXPORT_SYMBOL_GPL(cdc_parse_cdc_header);

#ifdef CONFIG_PCI
#ifdef CONFIG_PCI_MSI

/**
 * pci_alloc_irq_vectors - allocate multiple IRQs for a device
 * @dev:		PCI device to operate on
 * @min_vecs:		minimum number of vectors required (must be >= 1)
 * @max_vecs:		maximum (desired) number of vectors
 * @flags:		flags or quirks for the allocation
 *
 * Allocate up to @max_vecs interrupt vectors for @dev, using MSI-X or MSI
 * vectors if available, and fall back to a single legacy vector
 * if neither is available.  Return the number of vectors allocated,
 * (which might be smaller than @max_vecs) if successful, or a negative
 * error code on error. If less than @min_vecs interrupt vectors are
 * available for @dev the function will fail with -ENOSPC.
 *
 * To get the Linux IRQ number used for a vector that can be passed to
 * request_irq() use the pci_irq_vector() helper.
 */
int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
		unsigned int max_vecs, unsigned int flags)
{
	int vecs = -ENOSPC;

	if (flags & PCI_IRQ_MSIX) {
		vecs = pci_enable_msix_range(dev, NULL, min_vecs, max_vecs);
		if (vecs > 0)
			return vecs;
	}

	if (flags & PCI_IRQ_MSI) {
		vecs = pci_enable_msi_range(dev, min_vecs, max_vecs);
		if (vecs > 0)
			return vecs;
	}

	/* use legacy irq if allowed */
	if ((flags & PCI_IRQ_LEGACY) && min_vecs == 1) {
		pci_intx(dev, 1);
		return 1;
	}

	return vecs;
}
EXPORT_SYMBOL_GPL(pci_alloc_irq_vectors);
#endif /* CONFIG_PCI_MSI */
#endif /* CONFIG_PCI */
