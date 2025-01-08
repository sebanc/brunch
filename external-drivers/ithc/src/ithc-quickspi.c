// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause

// Some public THC/QuickSPI documentation can be found in:
// - Intel Firmware Support Package repo: https://github.com/intel/FSP
// - HID over SPI (HIDSPI) spec: https://www.microsoft.com/en-us/download/details.aspx?id=103325

#include "ithc.h"

static const guid_t guid_hidspi =
	GUID_INIT(0x6e2ac436, 0x0fcf, 0x41af, 0xa2, 0x65, 0xb3, 0x2a, 0x22, 0x0d, 0xcf, 0xab);
static const guid_t guid_thc_quickspi =
	GUID_INIT(0x300d35b7, 0xac20, 0x413e, 0x8e, 0x9c, 0x92, 0xe4, 0xda, 0xfd, 0x0a, 0xfe);
static const guid_t guid_thc_ltr =
	GUID_INIT(0x84005682, 0x5b71, 0x41a4, 0x8d, 0x66, 0x81, 0x30, 0xf7, 0x87, 0xa1, 0x38);

// TODO The HIDSPI spec says revision should be 3. Should we try both?
#define DSM_REV 2

struct hidspi_header {
	u8 type;
	u16 len;
	u8 id;
} __packed;
static_assert(sizeof(struct hidspi_header) == 4);

#define HIDSPI_INPUT_TYPE_DATA                        1
#define HIDSPI_INPUT_TYPE_RESET_RESPONSE              3
#define HIDSPI_INPUT_TYPE_COMMAND_RESPONSE            4
#define HIDSPI_INPUT_TYPE_GET_FEATURE_RESPONSE        5
#define HIDSPI_INPUT_TYPE_DEVICE_DESCRIPTOR           7
#define HIDSPI_INPUT_TYPE_REPORT_DESCRIPTOR           8
#define HIDSPI_INPUT_TYPE_SET_FEATURE_RESPONSE        9
#define HIDSPI_INPUT_TYPE_OUTPUT_REPORT_RESPONSE      10
#define HIDSPI_INPUT_TYPE_GET_INPUT_REPORT_RESPONSE   11

#define HIDSPI_OUTPUT_TYPE_DEVICE_DESCRIPTOR_REQUEST  1
#define HIDSPI_OUTPUT_TYPE_REPORT_DESCRIPTOR_REQUEST  2
#define HIDSPI_OUTPUT_TYPE_SET_FEATURE                3
#define HIDSPI_OUTPUT_TYPE_GET_FEATURE                4
#define HIDSPI_OUTPUT_TYPE_OUTPUT_REPORT              5
#define HIDSPI_OUTPUT_TYPE_INPUT_REPORT_REQUEST       6
#define HIDSPI_OUTPUT_TYPE_COMMAND                    7

struct hidspi_device_descriptor {
	u16 wDeviceDescLength;
	u16 bcdVersion;
	u16 wReportDescLength;
	u16 wMaxInputLength;
	u16 wMaxOutputLength;
	u16 wMaxFragmentLength;
	u16 wVendorID;
	u16 wProductID;
	u16 wVersionID;
	u16 wFlags;
	u32 dwReserved;
};
static_assert(sizeof(struct hidspi_device_descriptor) == 24);

static int read_acpi_u32(struct ithc *ithc, const guid_t *guid, u32 func, u32 *dest)
{
	acpi_handle handle = ACPI_HANDLE(&ithc->pci->dev);
	union acpi_object *o = acpi_evaluate_dsm(handle, guid, DSM_REV, func, NULL);
	if (!o)
		return 0;
	if (o->type != ACPI_TYPE_INTEGER) {
		pci_err(ithc->pci, "DSM %pUl %u returned type %i instead of integer\n",
			guid, func, o->type);
		ACPI_FREE(o);
		return -1;
	}
	pci_dbg(ithc->pci, "DSM %pUl %u = 0x%08x\n", guid, func, (u32)o->integer.value);
	*dest = (u32)o->integer.value;
	ACPI_FREE(o);
	return 1;
}

static int read_acpi_buf(struct ithc *ithc, const guid_t *guid, u32 func, size_t len, u8 *dest)
{
	acpi_handle handle = ACPI_HANDLE(&ithc->pci->dev);
	union acpi_object *o = acpi_evaluate_dsm(handle, guid, DSM_REV, func, NULL);
	if (!o)
		return 0;
	if (o->type != ACPI_TYPE_BUFFER) {
		pci_err(ithc->pci, "DSM %pUl %u returned type %i instead of buffer\n",
			guid, func, o->type);
		ACPI_FREE(o);
		return -1;
	}
	if (o->buffer.length != len) {
		pci_err(ithc->pci, "DSM %pUl %u returned len %u instead of %zu\n",
			guid, func, o->buffer.length, len);
		ACPI_FREE(o);
		return -1;
	}
	memcpy(dest, o->buffer.pointer, len);
	pci_dbg(ithc->pci, "DSM %pUl %u = 0x%02x\n", guid, func, dest[0]);
	ACPI_FREE(o);
	return 1;
}

int ithc_read_acpi_config(struct ithc *ithc, struct ithc_acpi_config *cfg)
{
	int r;
	acpi_handle handle = ACPI_HANDLE(&ithc->pci->dev);

	cfg->has_config = acpi_check_dsm(handle, &guid_hidspi, DSM_REV, BIT(0));
	if (!cfg->has_config)
		return 0;

	// HIDSPI settings

	r = read_acpi_u32(ithc, &guid_hidspi, 1, &cfg->input_report_header_address);
	if (r < 0)
		return r;
	cfg->has_input_report_header_address = r > 0;
	if (r > 0 && cfg->input_report_header_address > 0xffffff) {
		pci_err(ithc->pci, "Invalid input report header address 0x%x\n",
			cfg->input_report_header_address);
		return -1;
	}

	r = read_acpi_u32(ithc, &guid_hidspi, 2, &cfg->input_report_body_address);
	if (r < 0)
		return r;
	cfg->has_input_report_body_address = r > 0;
	if (r > 0 && cfg->input_report_body_address > 0xffffff) {
		pci_err(ithc->pci, "Invalid input report body address 0x%x\n",
			cfg->input_report_body_address);
		return -1;
	}

	r = read_acpi_u32(ithc, &guid_hidspi, 3, &cfg->output_report_body_address);
	if (r < 0)
		return r;
	cfg->has_output_report_body_address = r > 0;
	if (r > 0 && cfg->output_report_body_address > 0xffffff) {
		pci_err(ithc->pci, "Invalid output report body address 0x%x\n",
			cfg->output_report_body_address);
		return -1;
	}

	r = read_acpi_buf(ithc, &guid_hidspi, 4, sizeof(cfg->read_opcode), &cfg->read_opcode);
	if (r < 0)
		return r;
	cfg->has_read_opcode = r > 0;

	r = read_acpi_buf(ithc, &guid_hidspi, 5, sizeof(cfg->write_opcode), &cfg->write_opcode);
	if (r < 0)
		return r;
	cfg->has_write_opcode = r > 0;

	u32 flags;
	r = read_acpi_u32(ithc, &guid_hidspi, 6, &flags);
	if (r < 0)
		return r;
	cfg->has_read_mode = cfg->has_write_mode = r > 0;
	if (r > 0) {
		cfg->read_mode = (flags >> 14) & 3;
		cfg->write_mode = flags & BIT(13) ? cfg->read_mode : SPI_MODE_SINGLE;
	}

	// Quick SPI settings

	r = read_acpi_u32(ithc, &guid_thc_quickspi, 1, &cfg->spi_frequency);
	if (r < 0)
		return r;
	cfg->has_spi_frequency = r > 0;

	r = read_acpi_u32(ithc, &guid_thc_quickspi, 2, &cfg->limit_packet_size);
	if (r < 0)
		return r;
	cfg->has_limit_packet_size = r > 0;

	r = read_acpi_u32(ithc, &guid_thc_quickspi, 3, &cfg->tx_delay);
	if (r < 0)
		return r;
	cfg->has_tx_delay = r > 0;
	if (r > 0)
		cfg->tx_delay &= 0xffff;

	// LTR settings

	r = read_acpi_u32(ithc, &guid_thc_ltr, 1, &cfg->active_ltr);
	if (r < 0)
		return r;
	cfg->has_active_ltr = r > 0;
	if (r > 0 && (!cfg->active_ltr || cfg->active_ltr > 0x3ff)) {
		if (cfg->active_ltr != 0xffffffff)
			pci_warn(ithc->pci, "Ignoring invalid active LTR value 0x%x\n",
				cfg->active_ltr);
		cfg->active_ltr = 500;
	}

	r = read_acpi_u32(ithc, &guid_thc_ltr, 2, &cfg->idle_ltr);
	if (r < 0)
		return r;
	cfg->has_idle_ltr = r > 0;
	if (r > 0 && (!cfg->idle_ltr || cfg->idle_ltr > 0x3ff)) {
		if (cfg->idle_ltr != 0xffffffff)
			pci_warn(ithc->pci, "Ignoring invalid idle LTR value 0x%x\n",
				cfg->idle_ltr);
		cfg->idle_ltr = 500;
		if (cfg->has_active_ltr && cfg->active_ltr > cfg->idle_ltr)
			cfg->idle_ltr = cfg->active_ltr;
	}

	return 0;
}

void ithc_print_acpi_config(struct ithc *ithc, const struct ithc_acpi_config *cfg)
{
	if (!cfg->has_config) {
		pci_info(ithc->pci, "No ACPI config");
		return;
	}

	char input_report_header_address[16] = "-";
	if (cfg->has_input_report_header_address)
		sprintf(input_report_header_address, "0x%x", cfg->input_report_header_address);
	char input_report_body_address[16] = "-";
	if (cfg->has_input_report_body_address)
		sprintf(input_report_body_address, "0x%x", cfg->input_report_body_address);
	char output_report_body_address[16] = "-";
	if (cfg->has_output_report_body_address)
		sprintf(output_report_body_address, "0x%x", cfg->output_report_body_address);
	char read_opcode[16] = "-";
	if (cfg->has_read_opcode)
		sprintf(read_opcode, "0x%02x", cfg->read_opcode);
	char write_opcode[16] = "-";
	if (cfg->has_write_opcode)
		sprintf(write_opcode, "0x%02x", cfg->write_opcode);
	char read_mode[16] = "-";
	if (cfg->has_read_mode)
		sprintf(read_mode, "%i", cfg->read_mode);
	char write_mode[16] = "-";
	if (cfg->has_write_mode)
		sprintf(write_mode, "%i", cfg->write_mode);
	char spi_frequency[16] = "-";
	if (cfg->has_spi_frequency)
		sprintf(spi_frequency, "%u", cfg->spi_frequency);
	char limit_packet_size[16] = "-";
	if (cfg->has_limit_packet_size)
		sprintf(limit_packet_size, "%u", cfg->limit_packet_size);
	char tx_delay[16] = "-";
	if (cfg->has_tx_delay)
		sprintf(tx_delay, "%u", cfg->tx_delay);
	char active_ltr[16] = "-";
	if (cfg->has_active_ltr)
		sprintf(active_ltr, "%u", cfg->active_ltr);
	char idle_ltr[16] = "-";
	if (cfg->has_idle_ltr)
		sprintf(idle_ltr, "%u", cfg->idle_ltr);

	pci_info(ithc->pci, "ACPI config: InputHeaderAddr=%s InputBodyAddr=%s OutputBodyAddr=%s ReadOpcode=%s WriteOpcode=%s ReadMode=%s WriteMode=%s Frequency=%s LimitPacketSize=%s TxDelay=%s ActiveLTR=%s IdleLTR=%s\n",
		input_report_header_address, input_report_body_address, output_report_body_address,
		read_opcode, write_opcode, read_mode, write_mode,
		spi_frequency, limit_packet_size, tx_delay, active_ltr, idle_ltr);
}

static void set_opcode(struct ithc *ithc, size_t i, u8 opcode)
{
	writeb(opcode, &ithc->regs->opcode[i].header);
	writeb(opcode, &ithc->regs->opcode[i].single);
	writeb(opcode, &ithc->regs->opcode[i].dual);
	writeb(opcode, &ithc->regs->opcode[i].quad);
}

static int ithc_quickspi_init_regs(struct ithc *ithc, const struct ithc_acpi_config *cfg)
{
	pci_dbg(ithc->pci, "initializing QuickSPI registers\n");

	// SPI frequency and mode
	if (!cfg->has_spi_frequency || !cfg->spi_frequency) {
		pci_err(ithc->pci, "Missing SPI frequency in configuration\n");
		return -EINVAL;
	}
	unsigned int clkdiv = DIV_ROUND_UP(SPI_CLK_FREQ_BASE, cfg->spi_frequency);
	bool clkdiv8 = clkdiv > 7;
	if (clkdiv8)
		clkdiv = min(7u, DIV_ROUND_UP(clkdiv, 8u));
	if (!clkdiv)
		clkdiv = 1;
	CHECK_RET(ithc_set_spi_config, ithc, clkdiv, clkdiv8,
		cfg->has_read_mode ? cfg->read_mode : SPI_MODE_SINGLE,
		cfg->has_write_mode ? cfg->write_mode : SPI_MODE_SINGLE);

	// SPI addresses and opcodes
	if (cfg->has_input_report_header_address)
		writel(cfg->input_report_header_address, &ithc->regs->spi_header_addr);
	if (cfg->has_input_report_body_address) {
		writel(cfg->input_report_body_address, &ithc->regs->dma_rx[0].spi_addr);
		writel(cfg->input_report_body_address, &ithc->regs->dma_rx[1].spi_addr);
	}
	if (cfg->has_output_report_body_address)
		writel(cfg->output_report_body_address, &ithc->regs->dma_tx.spi_addr);

	switch (ithc->pci->device) {
	// LKF/TGL don't support QuickSPI.
	// For ADL, opcode layout is RX/TX/unused.
	case PCI_DEVICE_ID_INTEL_THC_ADL_S_PORT1:
	case PCI_DEVICE_ID_INTEL_THC_ADL_S_PORT2:
	case PCI_DEVICE_ID_INTEL_THC_ADL_P_PORT1:
	case PCI_DEVICE_ID_INTEL_THC_ADL_P_PORT2:
	case PCI_DEVICE_ID_INTEL_THC_ADL_M_PORT1:
	case PCI_DEVICE_ID_INTEL_THC_ADL_M_PORT2:
		if (cfg->has_read_opcode) {
			set_opcode(ithc, 0, cfg->read_opcode);
		}
		if (cfg->has_write_opcode) {
			set_opcode(ithc, 1, cfg->write_opcode);
		}
		break;
	// For MTL, opcode layout was changed to RX/RX/TX.
	// (RPL layout is unknown.)
	default:
		if (cfg->has_read_opcode) {
			set_opcode(ithc, 0, cfg->read_opcode);
			set_opcode(ithc, 1, cfg->read_opcode);
		}
		if (cfg->has_write_opcode) {
			set_opcode(ithc, 2, cfg->write_opcode);
		}
		break;
	}

	ithc_log_regs(ithc);

	// The rest...
	bitsl_set(&ithc->regs->dma_rx[0].init_unknown, INIT_UNKNOWN_31);

	bitsl(&ithc->regs->quickspi_config1,
		QUICKSPI_CONFIG1_UNKNOWN_0(0xff) | QUICKSPI_CONFIG1_UNKNOWN_5(0xff) |
		QUICKSPI_CONFIG1_UNKNOWN_10(0xff) | QUICKSPI_CONFIG1_UNKNOWN_16(0xffff),
		QUICKSPI_CONFIG1_UNKNOWN_0(4) | QUICKSPI_CONFIG1_UNKNOWN_5(4) |
		QUICKSPI_CONFIG1_UNKNOWN_10(22) | QUICKSPI_CONFIG1_UNKNOWN_16(2));

	bitsl(&ithc->regs->quickspi_config2,
		QUICKSPI_CONFIG2_UNKNOWN_0(0xff) | QUICKSPI_CONFIG2_UNKNOWN_5(0xff) |
		QUICKSPI_CONFIG2_UNKNOWN_12(0xff),
		QUICKSPI_CONFIG2_UNKNOWN_0(8) | QUICKSPI_CONFIG2_UNKNOWN_5(14) |
		QUICKSPI_CONFIG2_UNKNOWN_12(2));

	u32 pktsize = cfg->has_limit_packet_size && cfg->limit_packet_size == 1 ? 4 : 0x80;
	bitsl(&ithc->regs->spi_config,
		SPI_CONFIG_READ_PACKET_SIZE(0xfff) | SPI_CONFIG_WRITE_PACKET_SIZE(0xfff),
		SPI_CONFIG_READ_PACKET_SIZE(pktsize) | SPI_CONFIG_WRITE_PACKET_SIZE(pktsize));

	bitsl_set(&ithc->regs->quickspi_config2,
		QUICKSPI_CONFIG2_UNKNOWN_16 | QUICKSPI_CONFIG2_UNKNOWN_17);
	bitsl(&ithc->regs->quickspi_config2,
		QUICKSPI_CONFIG2_DISABLE_READ_ADDRESS_INCREMENT |
		QUICKSPI_CONFIG2_DISABLE_WRITE_ADDRESS_INCREMENT |
		QUICKSPI_CONFIG2_ENABLE_WRITE_STREAMING_MODE, 0);

	return 0;
}

static int wait_for_report(struct ithc *ithc)
{
	CHECK_RET(waitl, ithc, &ithc->regs->dma_rx[0].status,
		DMA_RX_STATUS_READY, DMA_RX_STATUS_READY);
	writel(DMA_RX_STATUS_READY, &ithc->regs->dma_rx[0].status);

	u32 h = readl(&ithc->regs->input_header);
	ithc_log_regs(ithc);
	if (INPUT_HEADER_SYNC(h) != INPUT_HEADER_SYNC_VALUE
		|| INPUT_HEADER_VERSION(h) != INPUT_HEADER_VERSION_VALUE) {
		pci_err(ithc->pci, "invalid input report frame header 0x%08x\n", h);
		return -ENODATA;
	}
	return INPUT_HEADER_REPORT_LENGTH(h) * 4;
}

static int ithc_quickspi_init_hidspi(struct ithc *ithc, const struct ithc_acpi_config *cfg)
{
	pci_dbg(ithc->pci, "initializing HIDSPI\n");

	// HIDSPI initialization sequence:
	// "1. The host shall invoke the ACPI reset method to clear the device state."
	acpi_status s = acpi_evaluate_object(ACPI_HANDLE(&ithc->pci->dev), "_RST", NULL, NULL);
	if (ACPI_FAILURE(s)) {
		pci_err(ithc->pci, "ACPI reset failed\n");
		return -EIO;
	}

	bitsl(&ithc->regs->control_bits, CONTROL_QUIESCE, 0);

	// "2. Within 1 second, the device shall signal an interrupt and make available to the host
	// an input report containing a device reset response."
	int size = wait_for_report(ithc);
	if (size < 0)
		return size;
	if (size < sizeof(struct hidspi_header)) {
		pci_err(ithc->pci, "SPI data size too small for reset response (%u)\n", size);
		return -EMSGSIZE;
	}

	// "3. The host shall read the reset response from the device at the Input Report addresses
	// specified in ACPI."
	u32 in_addr = cfg->has_input_report_body_address ? cfg->input_report_body_address : 0x1000;
	struct {
		struct hidspi_header header;
		union {
			struct hidspi_device_descriptor device_desc;
			u32 data[16];
		};
	} resp = { 0 };
	if (size > sizeof(resp)) {
		pci_err(ithc->pci, "SPI data size for reset response too big (%u)\n", size);
		return -EMSGSIZE;
	}
	CHECK_RET(ithc_spi_command, ithc, SPI_CMD_CODE_READ, in_addr, size, &resp);
	if (resp.header.type != HIDSPI_INPUT_TYPE_RESET_RESPONSE) {
		pci_err(ithc->pci, "received type %i instead of reset response\n", resp.header.type);
		return -ENOMSG;
	}

	// "4. The host shall then write an Output Report to the device at the Output Report Address
	// specified in ACPI, requesting the Device Descriptor from the device."
	u32 out_addr = cfg->has_output_report_body_address ? cfg->output_report_body_address : 0x1000;
	struct hidspi_header req = { .type = HIDSPI_OUTPUT_TYPE_DEVICE_DESCRIPTOR_REQUEST };
	CHECK_RET(ithc_spi_command, ithc, SPI_CMD_CODE_WRITE, out_addr, sizeof(req), &req);

	// "5. Within 1 second, the device shall signal an interrupt and make available to the host
	// an input report containing the Device Descriptor."
	size = wait_for_report(ithc);
	if (size < 0)
		return size;
	if (size < sizeof(resp.header) + sizeof(resp.device_desc)) {
		pci_err(ithc->pci, "SPI data size too small for device descriptor (%u)\n", size);
		return -EMSGSIZE;
	}

	// "6. The host shall read the Device Descriptor from the Input Report addresses specified
	// in ACPI."
	if (size > sizeof(resp)) {
		pci_err(ithc->pci, "SPI data size for device descriptor too big (%u)\n", size);
		return -EMSGSIZE;
	}
	memset(&resp, 0, sizeof(resp));
	CHECK_RET(ithc_spi_command, ithc, SPI_CMD_CODE_READ, in_addr, size, &resp);
	if (resp.header.type != HIDSPI_INPUT_TYPE_DEVICE_DESCRIPTOR) {
		pci_err(ithc->pci, "received type %i instead of device descriptor\n",
			resp.header.type);
		return -ENOMSG;
	}
	struct hidspi_device_descriptor *d = &resp.device_desc;
	if (resp.header.len < sizeof(*d)) {
		pci_err(ithc->pci, "response too small for device descriptor (%u)\n",
			resp.header.len);
		return -EMSGSIZE;
	}
	if (d->wDeviceDescLength != sizeof(*d)) {
		pci_err(ithc->pci, "invalid device descriptor length (%u)\n",
			d->wDeviceDescLength);
		return -EMSGSIZE;
	}

	pci_info(ithc->pci, "Device descriptor: bcdVersion=0x%04x wReportDescLength=%u wMaxInputLength=%u wMaxOutputLength=%u wMaxFragmentLength=%u wVendorID=0x%04x wProductID=0x%04x wVersionID=0x%04x wFlags=0x%04x dwReserved=0x%08x\n",
		d->bcdVersion, d->wReportDescLength,
		d->wMaxInputLength, d->wMaxOutputLength, d->wMaxFragmentLength,
		d->wVendorID, d->wProductID, d->wVersionID,
		d->wFlags, d->dwReserved);

	ithc->vendor_id = d->wVendorID;
	ithc->product_id = d->wProductID;
	ithc->product_rev = d->wVersionID;
	ithc->max_rx_size = max_t(u32, d->wMaxInputLength,
		d->wReportDescLength + sizeof(struct hidspi_header));
	ithc->max_tx_size = d->wMaxOutputLength;
	ithc->have_config = true;

	// "7. The device and host shall then enter their "Ready" states - where the device may
	// begin sending Input Reports, and the device shall be prepared for Output Reports from
	// the host."

	return 0;
}

int ithc_quickspi_init(struct ithc *ithc, const struct ithc_acpi_config *cfg)
{
	bitsl_set(&ithc->regs->control_bits, CONTROL_QUIESCE);
	CHECK_RET(waitl, ithc, &ithc->regs->control_bits, CONTROL_IS_QUIESCED, CONTROL_IS_QUIESCED);

	ithc_log_regs(ithc);
	CHECK_RET(ithc_quickspi_init_regs, ithc, cfg);
	ithc_log_regs(ithc);
	CHECK_RET(ithc_quickspi_init_hidspi, ithc, cfg);
	ithc_log_regs(ithc);

	// This value is set to 2 in ithc_quickspi_init_regs(). It needs to be set to 1 here,
	// otherwise DMA will not work. Maybe selects between DMA and PIO mode?
	bitsl(&ithc->regs->quickspi_config1,
		QUICKSPI_CONFIG1_UNKNOWN_16(0xffff), QUICKSPI_CONFIG1_UNKNOWN_16(1));

	// TODO Do we need to set any of the following bits here?
	//bitsb_set(&ithc->regs->dma_rx[1].control2, DMA_RX_CONTROL2_UNKNOWN_4);
	//bitsb_set(&ithc->regs->dma_rx[0].control2, DMA_RX_CONTROL2_UNKNOWN_5);
	//bitsb_set(&ithc->regs->dma_rx[1].control2, DMA_RX_CONTROL2_UNKNOWN_5);
	//bitsl_set(&ithc->regs->dma_rx[0].init_unknown, INIT_UNKNOWN_3);
	//bitsl_set(&ithc->regs->dma_rx[0].init_unknown, INIT_UNKNOWN_31);

	ithc_log_regs(ithc);

	return 0;
}

void ithc_quickspi_exit(struct ithc *ithc)
{
	// TODO Should we send HIDSPI 'power off' command?
	//struct hidspi_header h = { .type = HIDSPI_OUTPUT_TYPE_COMMAND, .id = 3, };
	//struct ithc_data d = { .type = ITHC_DATA_RAW, .data = &h, .size = sizeof(h) };
	//CHECK(ithc_dma_tx, ithc, &d); // or ithc_spi_command()
}

int ithc_quickspi_decode_rx(struct ithc *ithc, const void *src, size_t len, struct ithc_data *dest)
{
	const struct hidspi_header *hdr = src;

	if (len < sizeof(*hdr))
		return -ENODATA;
	// TODO Do we need to handle HIDSPI packet fragmentation?
	if (len < sizeof(*hdr) + hdr->len)
		return -EMSGSIZE;
	if (len > round_up(sizeof(*hdr) + hdr->len, 4))
		return -EMSGSIZE;

	switch (hdr->type) {
	case HIDSPI_INPUT_TYPE_RESET_RESPONSE:
		// TODO "When the device detects an error condition, it may interrupt and make
		// available to the host an Input Report containing an unsolicited Reset Response.
		// After receiving an unsolicited Reset Response, the host shall initiate the
		// request procedure from step (4) in the [HIDSPI initialization] process."
		dest->type = ITHC_DATA_ERROR;
		return 0;
	case HIDSPI_INPUT_TYPE_REPORT_DESCRIPTOR:
		dest->type = ITHC_DATA_REPORT_DESCRIPTOR;
		dest->data = hdr + 1;
		dest->size = hdr->len;
		return 0;
	case HIDSPI_INPUT_TYPE_DATA:
	case HIDSPI_INPUT_TYPE_GET_INPUT_REPORT_RESPONSE:
		dest->type = ITHC_DATA_INPUT_REPORT;
		dest->data = &hdr->id;
		dest->size = hdr->len + 1;
		return 0;
	case HIDSPI_INPUT_TYPE_GET_FEATURE_RESPONSE:
		dest->type = ITHC_DATA_GET_FEATURE;
		dest->data = &hdr->id;
		dest->size = hdr->len + 1;
		return 0;
	case HIDSPI_INPUT_TYPE_SET_FEATURE_RESPONSE:
	case HIDSPI_INPUT_TYPE_OUTPUT_REPORT_RESPONSE:
		dest->type = ITHC_DATA_IGNORE;
		return 0;
	default:
		return -EINVAL;
	}
}

ssize_t ithc_quickspi_encode_tx(struct ithc *ithc, const struct ithc_data *src, void *dest,
	size_t maxlen)
{
	struct hidspi_header *hdr = dest;

	size_t src_size = src->size;
	const u8 *src_data = src->data;
	u8 type;

	switch (src->type) {
	case ITHC_DATA_SET_FEATURE:
		type = HIDSPI_OUTPUT_TYPE_SET_FEATURE;
		break;
	case ITHC_DATA_GET_FEATURE:
		type = HIDSPI_OUTPUT_TYPE_GET_FEATURE;
		break;
	case ITHC_DATA_OUTPUT_REPORT:
		type = HIDSPI_OUTPUT_TYPE_OUTPUT_REPORT;
		break;
	case ITHC_DATA_REPORT_DESCRIPTOR:
		type = HIDSPI_OUTPUT_TYPE_REPORT_DESCRIPTOR_REQUEST;
		src_size = 0;
		break;
	default:
		return -EINVAL;
	}

	u8 id = 0;
	if (src_size) {
		id = *src_data++;
		src_size--;
	}

	// Data must be padded to next 4-byte boundary.
	size_t padded = round_up(src_size, 4);
	if (sizeof(*hdr) + padded > maxlen)
		return -EOVERFLOW;

	// Fill the TX buffer with header and data.
	hdr->type = type;
	hdr->len = (u16)src_size;
	hdr->id = id;
	memcpy_and_pad(hdr + 1, padded, src_data, src_size, 0);

	return sizeof(*hdr) + padded;
}

