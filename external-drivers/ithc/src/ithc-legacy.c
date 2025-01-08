// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause

#include "ithc.h"

#define DEVCFG_DMA_RX_SIZE(x)          ((((x) & 0x3fff) + 1) << 6)
#define DEVCFG_DMA_TX_SIZE(x)          (((((x) >> 14) & 0x3ff) + 1) << 6)

#define DEVCFG_TOUCH_MASK              0x3f
#define DEVCFG_TOUCH_ENABLE            BIT(0)
#define DEVCFG_TOUCH_PROP_DATA_ENABLE  BIT(1)
#define DEVCFG_TOUCH_HID_REPORT_ENABLE BIT(2)
#define DEVCFG_TOUCH_POWER_STATE(x)    (((x) & 7) << 3)
#define DEVCFG_TOUCH_UNKNOWN_6         BIT(6)

#define DEVCFG_DEVICE_ID_TIC           0x43495424 // "$TIC"

#define DEVCFG_SPI_CLKDIV(x)           (((x) >> 1) & 7)
#define DEVCFG_SPI_CLKDIV_8            BIT(4)
#define DEVCFG_SPI_SUPPORTS_SINGLE     BIT(5)
#define DEVCFG_SPI_SUPPORTS_DUAL       BIT(6)
#define DEVCFG_SPI_SUPPORTS_QUAD       BIT(7)
#define DEVCFG_SPI_MAX_TOUCH_POINTS(x) (((x) >> 8) & 0x3f)
#define DEVCFG_SPI_MIN_RESET_TIME(x)   (((x) >> 16) & 0xf)
#define DEVCFG_SPI_NEEDS_HEARTBEAT     BIT(20) // TODO implement heartbeat
#define DEVCFG_SPI_HEARTBEAT_INTERVAL(x) (((x) >> 21) & 7)
#define DEVCFG_SPI_UNKNOWN_25          BIT(25)
#define DEVCFG_SPI_UNKNOWN_26          BIT(26)
#define DEVCFG_SPI_UNKNOWN_27          BIT(27)
#define DEVCFG_SPI_DELAY(x)            (((x) >> 28) & 7) // TODO use this
#define DEVCFG_SPI_USE_EXT_READ_CFG    BIT(31) // TODO use this?

struct ithc_device_config { // (Example values are from an SP7+.)
	u32 irq_cause;        // 00 = 0xe0000402 (0xe0000401 after DMA_RX_CODE_RESET)
	u32 error;            // 04 = 0x00000000
	u32 dma_buf_sizes;    // 08 = 0x000a00ff
	u32 touch_cfg;        // 0c = 0x0000001c
	u32 touch_state;      // 10 = 0x0000001c
	u32 device_id;        // 14 = 0x43495424 = "$TIC"
	u32 spi_config;       // 18 = 0xfda00a2e
	u16 vendor_id;        // 1c = 0x045e = Microsoft Corp.
	u16 product_id;       // 1e = 0x0c1a
	u32 revision;         // 20 = 0x00000001
	u32 fw_version;       // 24 = 0x05008a8b = 5.0.138.139 (this value looks more random on newer devices)
	u32 command;          // 28 = 0x00000000
	u32 fw_mode;          // 2c = 0x00000000 (for fw update?)
	u32 _unknown_30;      // 30 = 0x00000000
	u8 eds_minor_ver;     // 34 = 0x5e
	u8 eds_major_ver;     // 35 = 0x03
	u8 interface_rev;     // 36 = 0x04
	u8 eu_kernel_ver;     // 37 = 0x04
	u32 _unknown_38;      // 38 = 0x000001c0 (0x000001c1 after DMA_RX_CODE_RESET)
	u32 _unknown_3c;      // 3c = 0x00000002
};
static_assert(sizeof(struct ithc_device_config) == 64);

#define RX_CODE_INPUT_REPORT          3
#define RX_CODE_FEATURE_REPORT        4
#define RX_CODE_REPORT_DESCRIPTOR     5
#define RX_CODE_RESET                 7

#define TX_CODE_SET_FEATURE           3
#define TX_CODE_GET_FEATURE           4
#define TX_CODE_OUTPUT_REPORT         5
#define TX_CODE_GET_REPORT_DESCRIPTOR 7

static int ithc_set_device_enabled(struct ithc *ithc, bool enable)
{
	u32 x = ithc->legacy_touch_cfg =
		(ithc->legacy_touch_cfg & ~(u32)DEVCFG_TOUCH_MASK) |
		DEVCFG_TOUCH_HID_REPORT_ENABLE |
		(enable ? DEVCFG_TOUCH_ENABLE | DEVCFG_TOUCH_POWER_STATE(3) : 0);
	return ithc_spi_command(ithc, SPI_CMD_CODE_WRITE,
		offsetof(struct ithc_device_config, touch_cfg), sizeof(x), &x);
}

int ithc_legacy_init(struct ithc *ithc)
{
	// Since we don't yet know which SPI config the device wants, use default speed and mode
	// initially for reading config data.
	CHECK(ithc_set_spi_config, ithc, 2, true, SPI_MODE_SINGLE, SPI_MODE_SINGLE);

	// Setting the following bit seems to make reading the config more reliable.
	bitsl_set(&ithc->regs->dma_rx[0].init_unknown, INIT_UNKNOWN_31);

	// Setting this bit may be necessary on ADL devices.
	switch (ithc->pci->device) {
	case PCI_DEVICE_ID_INTEL_THC_ADL_S_PORT1:
	case PCI_DEVICE_ID_INTEL_THC_ADL_S_PORT2:
	case PCI_DEVICE_ID_INTEL_THC_ADL_P_PORT1:
	case PCI_DEVICE_ID_INTEL_THC_ADL_P_PORT2:
	case PCI_DEVICE_ID_INTEL_THC_ADL_M_PORT1:
	case PCI_DEVICE_ID_INTEL_THC_ADL_M_PORT2:
		bitsl_set(&ithc->regs->dma_rx[0].init_unknown, INIT_UNKNOWN_5);
		break;
	}

	// Take the touch device out of reset.
	bitsl(&ithc->regs->control_bits, CONTROL_QUIESCE, 0);
	CHECK_RET(waitl, ithc, &ithc->regs->control_bits, CONTROL_IS_QUIESCED, 0);
	for (int retries = 0; ; retries++) {
		ithc_log_regs(ithc);
		bitsl_set(&ithc->regs->control_bits, CONTROL_NRESET);
		if (!waitl(ithc, &ithc->regs->irq_cause, 0xf, 2))
			break;
		if (retries > 5) {
			pci_err(ithc->pci, "failed to reset device, irq_cause = 0x%08x\n",
				readl(&ithc->regs->irq_cause));
			return -ETIMEDOUT;
		}
		pci_warn(ithc->pci, "invalid irq_cause, retrying reset\n");
		bitsl(&ithc->regs->control_bits, CONTROL_NRESET, 0);
		if (msleep_interruptible(1000))
			return -EINTR;
	}
	ithc_log_regs(ithc);

	CHECK(waitl, ithc, &ithc->regs->dma_rx[0].status, DMA_RX_STATUS_READY, DMA_RX_STATUS_READY);

	// Read configuration data.
	u32 spi_cfg;
	for (int retries = 0; ; retries++) {
		ithc_log_regs(ithc);
		struct ithc_device_config config = { 0 };
		CHECK_RET(ithc_spi_command, ithc, SPI_CMD_CODE_READ, 0, sizeof(config), &config);
		u32 *p = (void *)&config;
		pci_info(ithc->pci, "config: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
		if (config.device_id == DEVCFG_DEVICE_ID_TIC) {
			spi_cfg = config.spi_config;
			ithc->vendor_id = config.vendor_id;
			ithc->product_id = config.product_id;
			ithc->product_rev = config.revision;
			ithc->max_rx_size = DEVCFG_DMA_RX_SIZE(config.dma_buf_sizes);
			ithc->max_tx_size = DEVCFG_DMA_TX_SIZE(config.dma_buf_sizes);
			ithc->legacy_touch_cfg = config.touch_cfg;
			ithc->have_config = true;
			break;
		}
		if (retries > 10) {
			pci_err(ithc->pci, "failed to read config, unknown device ID 0x%08x\n",
				config.device_id);
			return -EIO;
		}
		pci_warn(ithc->pci, "failed to read config, retrying\n");
		if (msleep_interruptible(100))
			return -EINTR;
	}
	ithc_log_regs(ithc);

	// Apply SPI config and enable touch device.
	CHECK_RET(ithc_set_spi_config, ithc,
		DEVCFG_SPI_CLKDIV(spi_cfg), (spi_cfg & DEVCFG_SPI_CLKDIV_8) != 0,
		spi_cfg & DEVCFG_SPI_SUPPORTS_QUAD ? SPI_MODE_QUAD :
		spi_cfg & DEVCFG_SPI_SUPPORTS_DUAL ? SPI_MODE_DUAL :
		SPI_MODE_SINGLE,
		SPI_MODE_SINGLE);
	CHECK_RET(ithc_set_device_enabled, ithc, true);
	ithc_log_regs(ithc);
	return 0;
}

void ithc_legacy_exit(struct ithc *ithc)
{
	CHECK(ithc_set_device_enabled, ithc, false);
}

int ithc_legacy_decode_rx(struct ithc *ithc, const void *src, size_t len, struct ithc_data *dest)
{
	const struct {
		u32 code;
		u32 data_size;
		u32 _unknown[14];
	} *hdr = src;

	if (len < sizeof(*hdr))
		return -ENODATA;
	// Note: RX data is not padded, even though TX data must be padded.
	if (len != sizeof(*hdr) + hdr->data_size)
		return -EMSGSIZE;

	dest->data = hdr + 1;
	dest->size = hdr->data_size;

	switch (hdr->code) {
	case RX_CODE_RESET:
		// The THC sends a reset request when we need to reinitialize the device.
		// This usually only happens if we send an invalid command or put the device
		// in a bad state.
		dest->type = ITHC_DATA_ERROR;
		return 0;
	case RX_CODE_REPORT_DESCRIPTOR:
		// The descriptor is preceded by 8 nul bytes.
		if (hdr->data_size < 8)
			return -ENODATA;
		dest->type = ITHC_DATA_REPORT_DESCRIPTOR;
		dest->data = (char *)(hdr + 1) + 8;
		dest->size = hdr->data_size - 8;
		return 0;
	case RX_CODE_INPUT_REPORT:
		dest->type = ITHC_DATA_INPUT_REPORT;
		return 0;
	case RX_CODE_FEATURE_REPORT:
		dest->type = ITHC_DATA_GET_FEATURE;
		return 0;
	default:
		return -EINVAL;
	}
}

ssize_t ithc_legacy_encode_tx(struct ithc *ithc, const struct ithc_data *src, void *dest,
	size_t maxlen)
{
	struct {
		u32 code;
		u32 data_size;
	} *hdr = dest;

	size_t src_size = src->size;
	const void *src_data = src->data;
	const u64 get_report_desc_data = 0;
	u32 code;

	switch (src->type) {
	case ITHC_DATA_SET_FEATURE:
		code = TX_CODE_SET_FEATURE;
		break;
	case ITHC_DATA_GET_FEATURE:
		code = TX_CODE_GET_FEATURE;
		break;
	case ITHC_DATA_OUTPUT_REPORT:
		code = TX_CODE_OUTPUT_REPORT;
		break;
	case ITHC_DATA_REPORT_DESCRIPTOR:
		code = TX_CODE_GET_REPORT_DESCRIPTOR;
		src_size = sizeof(get_report_desc_data);
		src_data = &get_report_desc_data;
		break;
	default:
		return -EINVAL;
	}

	// Data must be padded to next 4-byte boundary.
	size_t padded = round_up(src_size, 4);
	if (sizeof(*hdr) + padded > maxlen)
		return -EOVERFLOW;

	// Fill the TX buffer with header and data.
	hdr->code = code;
	hdr->data_size = src_size;
	memcpy_and_pad(hdr + 1, padded, src_data, src_size, 0);

	return sizeof(*hdr) + padded;
}

