#include "ithc.h"

MODULE_DESCRIPTION("Intel Touch Host Controller driver");
MODULE_LICENSE("Dual BSD/GPL");

#define PCI_DEVICE_ID_INTEL_THC_LKF_PORT1    0x98d0
#define PCI_DEVICE_ID_INTEL_THC_LKF_PORT2    0x98d1
#define PCI_DEVICE_ID_INTEL_THC_TGL_LP_PORT1 0xa0d0
#define PCI_DEVICE_ID_INTEL_THC_TGL_LP_PORT2 0xa0d1
#define PCI_DEVICE_ID_INTEL_THC_TGL_H_PORT1  0x43d0
#define PCI_DEVICE_ID_INTEL_THC_TGL_H_PORT2  0x43d1

static const struct pci_device_id ithc_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_THC_LKF_PORT1) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_THC_LKF_PORT2) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_THC_TGL_LP_PORT1) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_THC_TGL_LP_PORT2) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_THC_TGL_H_PORT1) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_THC_TGL_H_PORT2) },
	{}
};
MODULE_DEVICE_TABLE(pci, ithc_pci_tbl);

// Module parameters

static bool ithc_use_polling = false;
module_param_named(poll, ithc_use_polling, bool, 0);
MODULE_PARM_DESC(poll, "Use polling instead of interrupts");

static int ithc_use_hid = -1;
module_param_named(hid, ithc_use_hid, bint, 0);
MODULE_PARM_DESC(hid, "Operate as an HID transport driver instead of an input driver");

static bool ithc_use_rx0 = false;
module_param_named(rx0, ithc_use_rx0, bool, 0);
MODULE_PARM_DESC(rx0, "Use DMA RX channel 0");

static bool ithc_use_rx1 = true;
module_param_named(rx1, ithc_use_rx1, bool, 0);
MODULE_PARM_DESC(rx1, "Use DMA RX channel 1");

static bool ithc_log_regs_enabled = false;
module_param_named(logregs, ithc_log_regs_enabled, bool, 0);
MODULE_PARM_DESC(logregs, "Log changes in register values (for debugging)");

// Sysfs attributes

static bool ithc_is_config_valid(struct ithc *ithc) {
	return ithc->config.device_id == DEVCFG_DEVICE_ID_TIC;
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct ithc *ithc = dev_get_drvdata(dev);
	if (!ithc || !ithc_is_config_valid(ithc)) return -ENODEV;
	return sprintf(buf, "0x%04x", ithc->config.vendor_id);
}
DEVICE_ATTR_RO(vendor);
static ssize_t product_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct ithc *ithc = dev_get_drvdata(dev);
	if (!ithc || !ithc_is_config_valid(ithc)) return -ENODEV;
	return sprintf(buf, "0x%04x", ithc->config.product_id);
}
DEVICE_ATTR_RO(product);
static ssize_t revision_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct ithc *ithc = dev_get_drvdata(dev);
	if (!ithc || !ithc_is_config_valid(ithc)) return -ENODEV;
	return sprintf(buf, "%u", ithc->config.revision);
}
DEVICE_ATTR_RO(revision);
static ssize_t fw_version_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct ithc *ithc = dev_get_drvdata(dev);
	if (!ithc || !ithc_is_config_valid(ithc)) return -ENODEV;
	u32 v = ithc->config.fw_version;
	return sprintf(buf, "%i.%i.%i.%i", v >> 24, v >> 16 & 0xff, v >> 8 & 0xff, v & 0xff);
}
DEVICE_ATTR_RO(fw_version);

static const struct attribute_group *ithc_attribute_groups[] = {
	&(const struct attribute_group){
		.name = DEVNAME,
		.attrs = (struct attribute *[]){
			&dev_attr_vendor.attr,
			&dev_attr_product.attr,
			&dev_attr_revision.attr,
			&dev_attr_fw_version.attr,
			NULL
		},
	},
	NULL
};

// HID/input setup

static int ithc_hid_start(struct hid_device *hdev) { return 0; }
static void ithc_hid_stop(struct hid_device *hdev) { }
static int ithc_hid_open(struct hid_device *hdev) { return 0; }
static void ithc_hid_close(struct hid_device *hdev) { }

static int ithc_hid_parse(struct hid_device *hdev) {
	struct ithc *ithc = hdev->driver_data;
	u64 val = 0;
	WRITE_ONCE(ithc->hid_parse_done, false);
	CHECK_RET(ithc_dma_tx, ithc, DMA_TX_CODE_GET_REPORT_DESCRIPTOR, sizeof val, &val);
	if (!wait_event_timeout(ithc->wait_hid_parse, READ_ONCE(ithc->hid_parse_done), msecs_to_jiffies(1000))) return -ETIMEDOUT;
	return 0;
}

static int ithc_hid_raw_request(struct hid_device *hdev, unsigned char reportnum, __u8 *buf, size_t len, unsigned char rtype, int reqtype) {
	struct ithc *ithc = hdev->driver_data;
	if (!buf || !len) return -EINVAL;
	u32 code;
	if (rtype == HID_OUTPUT_REPORT && reqtype == HID_REQ_SET_REPORT) code = DMA_TX_CODE_OUTPUT_REPORT;
	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_SET_REPORT) code = DMA_TX_CODE_SET_FEATURE;
	else if (rtype == HID_FEATURE_REPORT && reqtype == HID_REQ_GET_REPORT) code = DMA_TX_CODE_GET_FEATURE;
	else {
		pci_err(ithc->pci, "unhandled hid request %i %i for report id %i\n", rtype, reqtype, reportnum);
		return -EINVAL;
	}
	buf[0] = reportnum;
	if (reqtype == HID_REQ_GET_REPORT) {
		mutex_lock(&ithc->hid_get_feature_mutex);
		ithc->hid_get_feature_buf = buf;
		ithc->hid_get_feature_size = len;
		mutex_unlock(&ithc->hid_get_feature_mutex);
		int r = CHECK(ithc_dma_tx, ithc, code, 1, buf);
		if (!r) {
			r = wait_event_interruptible_timeout(ithc->wait_hid_get_feature, !ithc->hid_get_feature_buf, msecs_to_jiffies(1000));
			if (!r) r = -ETIMEDOUT;
			else if (r < 0) r = -EINTR;
			else r = 0;
		}
		mutex_lock(&ithc->hid_get_feature_mutex);
		ithc->hid_get_feature_buf = NULL;
		if (!r) r = ithc->hid_get_feature_size;
		mutex_unlock(&ithc->hid_get_feature_mutex);
		return r;
	}
	CHECK_RET(ithc_dma_tx, ithc, code, len, buf);
	return 0;
}

static struct hid_ll_driver ithc_ll_driver = {
	.start = ithc_hid_start,
	.stop = ithc_hid_stop,
	.open = ithc_hid_open,
	.close = ithc_hid_close,
	.parse = ithc_hid_parse,
	.raw_request = ithc_hid_raw_request,
};

static void ithc_hid_devres_release(struct device *dev, void *res) {
	struct hid_device **hidm = res;
	if (*hidm) hid_destroy_device(*hidm);
}

static int ithc_hid_init(struct ithc *ithc) {
	struct hid_device **hidm = devres_alloc(ithc_hid_devres_release, sizeof *hidm, GFP_KERNEL);
	if (!hidm) return -ENOMEM;
	devres_add(&ithc->pci->dev, hidm);
	struct hid_device *hid = hid_allocate_device();
	if (IS_ERR(hid)) return PTR_ERR(hid);
	*hidm = hid;

	strscpy(hid->name, DEVFULLNAME, sizeof(hid->name));
	strscpy(hid->phys, ithc->phys, sizeof(hid->phys));
	hid->ll_driver = &ithc_ll_driver;
	hid->bus = BUS_PCI;
	hid->vendor = ithc->config.vendor_id;
	hid->product = ithc->config.product_id;
	hid->version = 0x100;
	hid->dev.parent = &ithc->pci->dev;
	hid->driver_data = ithc;

	ithc->hid = hid;
	return 0;
}

static int ithc_input_init(struct ithc *ithc) {
	struct input_dev *d = devm_input_allocate_device(&ithc->pci->dev);
	if (!d) return -ENOMEM;

	d->name = DEVFULLNAME;
	d->phys = ithc->phys;
	d->id.bustype = BUS_PCI;
	d->id.vendor = ithc->config.vendor_id;
	d->id.product = ithc->config.product_id;
	d->id.version = ithc->config.revision;
	d->dev.parent = &ithc->pci->dev;

	__set_bit(INPUT_PROP_DIRECT, d->propbit);
	input_set_capability(d, EV_KEY, BTN_TOUCH);
	input_set_abs_params(d, ABS_X, 0, 32767, 0, 0);
	input_set_abs_params(d, ABS_Y, 0, 32767, 0, 0);

	CHECK_RET(input_register_device, d);

	ithc->input = d;
	return 0;
}

// Interrupts/polling

static void ithc_activity_timer_callback(struct timer_list *t) {
	struct ithc *ithc = container_of(t, struct ithc, activity_timer);
	cpu_latency_qos_update_request(&ithc->activity_qos, PM_QOS_DEFAULT_VALUE);
}

void ithc_set_active(struct ithc *ithc) {
	// When CPU usage is very low, the CPU can enter various low power states (C2-C10).
	// This disrupts DMA, causing truncated DMA messages. ERROR_FLAG_DMA_UNKNOWN_12 will be set when this happens.
	// The amount of truncated messages can become very high, resulting in user-visible effects (laggy/stuttering cursor).
	// To avoid this, we use a CPU latency QoS request to prevent the CPU from entering low power states during touch interactions.
	cpu_latency_qos_update_request(&ithc->activity_qos, 0);
	mod_timer(&ithc->activity_timer, jiffies + msecs_to_jiffies(1000));
}

static int ithc_set_device_enabled(struct ithc *ithc, bool enable) {
	u32 x = ithc->config.touch_cfg = (ithc->config.touch_cfg & ~(u32)DEVCFG_TOUCH_MASK) | DEVCFG_TOUCH_UNKNOWN_2
		| (enable ? DEVCFG_TOUCH_ENABLE | DEVCFG_TOUCH_UNKNOWN_3 | DEVCFG_TOUCH_UNKNOWN_4 : 0);
	return ithc_spi_command(ithc, SPI_CMD_CODE_WRITE, offsetof(struct ithc_device_config, touch_cfg), sizeof x, &x);
}

static void ithc_disable_interrupts(struct ithc *ithc) {
	writel(0, &ithc->regs->error_control);
	bitsb(&ithc->regs->spi_cmd.control, SPI_CMD_CONTROL_IRQ, 0);
	bitsb(&ithc->regs->dma_rx[0].control, DMA_RX_CONTROL_IRQ_UNKNOWN_1 | DMA_RX_CONTROL_IRQ_ERROR | DMA_RX_CONTROL_IRQ_UNKNOWN_4 | DMA_RX_CONTROL_IRQ_DATA, 0);
	bitsb(&ithc->regs->dma_rx[1].control, DMA_RX_CONTROL_IRQ_UNKNOWN_1 | DMA_RX_CONTROL_IRQ_ERROR | DMA_RX_CONTROL_IRQ_UNKNOWN_4 | DMA_RX_CONTROL_IRQ_DATA, 0);
	bitsb(&ithc->regs->dma_tx.control, DMA_TX_CONTROL_IRQ, 0);
}

static void ithc_clear_dma_rx_interrupts(struct ithc *ithc, unsigned channel) {
	writel(DMA_RX_STATUS_ERROR | DMA_RX_STATUS_UNKNOWN_4 | DMA_RX_STATUS_HAVE_DATA, &ithc->regs->dma_rx[channel].status);
}

static void ithc_clear_interrupts(struct ithc *ithc) {
	writel(0xffffffff, &ithc->regs->error_flags);
	writel(ERROR_STATUS_DMA | ERROR_STATUS_SPI, &ithc->regs->error_status);
	writel(SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR, &ithc->regs->spi_cmd.status);
	ithc_clear_dma_rx_interrupts(ithc, 0);
	ithc_clear_dma_rx_interrupts(ithc, 1);
	writel(DMA_TX_STATUS_DONE | DMA_TX_STATUS_ERROR | DMA_TX_STATUS_UNKNOWN_2, &ithc->regs->dma_tx.status);
}

static void ithc_process(struct ithc *ithc) {
	ithc_log_regs(ithc);

	// read and clear error bits
	u32 err = readl(&ithc->regs->error_flags);
	if (err) {
		if (err & ~ERROR_FLAG_DMA_UNKNOWN_12) pci_err(ithc->pci, "error flags: 0x%08x\n", err);
		writel(err, &ithc->regs->error_flags);
	}

	// process DMA rx
	if (ithc_use_rx0) {
		ithc_clear_dma_rx_interrupts(ithc, 0);
		ithc_dma_rx(ithc, 0);
	}
	if (ithc_use_rx1) {
		ithc_clear_dma_rx_interrupts(ithc, 1);
		ithc_dma_rx(ithc, 1);
	}

	ithc_log_regs(ithc);
}

static irqreturn_t ithc_interrupt_thread(int irq, void *arg) {
	struct ithc *ithc = arg;
	pci_dbg(ithc->pci, "IRQ! err=%08x/%08x/%08x, cmd=%02x/%08x, rx0=%02x/%08x, rx1=%02x/%08x, tx=%02x/%08x\n",
		readl(&ithc->regs->error_control), readl(&ithc->regs->error_status), readl(&ithc->regs->error_flags),
		readb(&ithc->regs->spi_cmd.control), readl(&ithc->regs->spi_cmd.status),
		readb(&ithc->regs->dma_rx[0].control), readl(&ithc->regs->dma_rx[0].status),
		readb(&ithc->regs->dma_rx[1].control), readl(&ithc->regs->dma_rx[1].status),
		readb(&ithc->regs->dma_tx.control), readl(&ithc->regs->dma_tx.status));
	ithc_process(ithc);
	return IRQ_HANDLED;
}

static int ithc_poll_thread(void *arg) {
	struct ithc *ithc = arg;
	unsigned sleep = 100;
	while (!kthread_should_stop()) {
		u32 n = ithc->dma_rx[1].num_received;
		ithc_process(ithc);
		if (n != ithc->dma_rx[1].num_received) sleep = 20;
		else sleep = min(200u, sleep + (sleep >> 4) + 1);
		msleep_interruptible(sleep);
	}
	return 0;
}

// Device initialization and shutdown

static void ithc_disable(struct ithc *ithc) {
	bitsl_set(&ithc->regs->control_bits, CONTROL_QUIESCE);
	waitl(ithc, &ithc->regs->control_bits, CONTROL_IS_QUIESCED, CONTROL_IS_QUIESCED);
	bitsl(&ithc->regs->control_bits, CONTROL_NRESET, 0);
	bitsb(&ithc->regs->spi_cmd.control, SPI_CMD_CONTROL_SEND, 0);
	bitsb(&ithc->regs->dma_tx.control, DMA_TX_CONTROL_SEND, 0);
	bitsb(&ithc->regs->dma_rx[0].control, DMA_RX_CONTROL_ENABLE, 0);
	bitsb(&ithc->regs->dma_rx[1].control, DMA_RX_CONTROL_ENABLE, 0);
	ithc_disable_interrupts(ithc);
	ithc_clear_interrupts(ithc);
}

static int ithc_init_device(struct ithc *ithc) {
	ithc_log_regs(ithc);
	bool was_enabled = (readl(&ithc->regs->control_bits) & CONTROL_NRESET) != 0;
	ithc_disable(ithc);
	CHECK_RET(waitl, ithc, &ithc->regs->control_bits, CONTROL_READY, CONTROL_READY);
	ithc_set_spi_config(ithc, 10, 0);
	bitsl_set(&ithc->regs->dma_rx[0].unknown_init_bits, 0x80000000); // seems to help with reading config

	if (was_enabled) if (msleep_interruptible(100)) return -EINTR;
	bitsl(&ithc->regs->control_bits, CONTROL_QUIESCE, 0);
	CHECK_RET(waitl, ithc, &ithc->regs->control_bits, CONTROL_IS_QUIESCED, 0);
	for (int retries = 0; ; retries++) {
		ithc_log_regs(ithc);
		bitsl_set(&ithc->regs->control_bits, CONTROL_NRESET);
		if (!waitl(ithc, &ithc->regs->state, 0xf, 2)) break;
		if (retries > 5) {
			pci_err(ithc->pci, "too many retries, failed to reset device\n");
			return -ETIMEDOUT;
		}
		pci_err(ithc->pci, "invalid state, retrying reset\n");
		bitsl(&ithc->regs->control_bits, CONTROL_NRESET, 0);
		if (msleep_interruptible(1000)) return -EINTR;
	}
	ithc_log_regs(ithc);

	CHECK(waitl, ithc, &ithc->regs->dma_rx[0].status, DMA_RX_STATUS_UNKNOWN_4, DMA_RX_STATUS_UNKNOWN_4);

	// read config
	for (int retries = 0; ; retries++) {
		ithc_log_regs(ithc);
		memset(&ithc->config, 0, sizeof ithc->config);
		CHECK_RET(ithc_spi_command, ithc, SPI_CMD_CODE_READ, 0, sizeof ithc->config, &ithc->config);
		u32 *p = (void *)&ithc->config;
		pci_info(ithc->pci, "config: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
		if (ithc_is_config_valid(ithc)) break;
		if (retries > 10) {
			pci_err(ithc->pci, "failed to read config, unknown device ID 0x%08x\n", ithc->config.device_id);
			return -EIO;
		}
		pci_err(ithc->pci, "failed to read config, retrying\n");
		if (msleep_interruptible(100)) return -EINTR;
	}
	ithc_log_regs(ithc);

	CHECK_RET(ithc_set_spi_config, ithc, DEVCFG_SPI_MAX_FREQ(ithc->config.spi_config), DEVCFG_SPI_MODE(ithc->config.spi_config));
	CHECK_RET(ithc_set_device_enabled, ithc, true);
	ithc_log_regs(ithc);
	return 0;
}

int ithc_reset(struct ithc *ithc) {
	pci_err(ithc->pci, "reset\n");
	CHECK(ithc_init_device, ithc);
	if (ithc_use_rx0) {
		ithc_dma_rx_enable(ithc, 0);
		if (ithc->dma_rx[0].api && atomic_read(&ithc->dma_rx[0].api->open_count)) CHECK(ithc_set_multitouch, ithc, true);
	}
	if (ithc_use_rx1) {
		ithc_dma_rx_enable(ithc, 1);
		if (ithc->dma_rx[1].api && atomic_read(&ithc->dma_rx[1].api->open_count)) CHECK(ithc_set_multitouch, ithc, true);
	}
	ithc_log_regs(ithc);
	pci_dbg(ithc->pci, "reset completed\n");
	return 0;
}

static void ithc_stop(void *res) {
	struct ithc *ithc = res;
	pci_dbg(ithc->pci, "stopping\n");
	ithc_log_regs(ithc);
	if (ithc->poll_thread) CHECK(kthread_stop, ithc->poll_thread);
	if (ithc->irq >= 0) disable_irq(ithc->irq);
	CHECK(ithc_set_device_enabled, ithc, false);
	ithc_disable(ithc);
	del_timer_sync(&ithc->activity_timer);
	cpu_latency_qos_remove_request(&ithc->activity_qos);
	// clear dma config
	for(unsigned i = 0; i < 2; i++) {
		CHECK(waitl, ithc, &ithc->regs->dma_rx[i].status, DMA_RX_STATUS_ENABLED, 0);
		lo_hi_writeq(0, &ithc->regs->dma_rx[i].addr);
		writeb(0, &ithc->regs->dma_rx[i].num_bufs);
		writeb(0, &ithc->regs->dma_rx[i].num_prds);
	}
	lo_hi_writeq(0, &ithc->regs->dma_tx.addr);
	writeb(0, &ithc->regs->dma_tx.num_prds);
	ithc_log_regs(ithc);
	pci_dbg(ithc->pci, "stopped\n");
}

static int ithc_probe(struct pci_dev *pci, const struct pci_device_id *id) {
	pci_dbg(pci, "starting\n");

	struct ithc *ithc = devm_kzalloc(&pci->dev, sizeof *ithc, GFP_KERNEL);
	if (!ithc) return -ENOMEM;
	ithc->irq = -1;
	ithc->pci = pci;
	snprintf(ithc->phys, sizeof ithc->phys, "pci-%s/" DEVNAME, pci_name(pci));
	init_waitqueue_head(&ithc->wait_hid_parse);
	init_waitqueue_head(&ithc->wait_hid_get_feature);
	mutex_init(&ithc->hid_get_feature_mutex);
	pci_set_drvdata(pci, ithc);
	if (ithc_log_regs_enabled) ithc->prev_regs = devm_kzalloc(&pci->dev, sizeof *ithc->prev_regs, GFP_KERNEL);

	CHECK_RET(pcim_enable_device, pci);
	pci_set_master(pci);
	CHECK_RET(pcim_iomap_regions, pci, BIT(0), DEVNAME " regs");
	CHECK_RET(dma_set_mask_and_coherent, &pci->dev, DMA_BIT_MASK(64));
	CHECK_RET(pci_set_power_state, pci, PCI_D0);
	ithc->regs = pcim_iomap_table(pci)[0];

	if (!ithc_use_polling) {
		CHECK_RET(pci_alloc_irq_vectors, pci, 1, 1, PCI_IRQ_MSI | PCI_IRQ_MSIX);
		ithc->irq = CHECK(pci_irq_vector, pci, 0);
		if (ithc->irq < 0) return ithc->irq;
	}

	CHECK_RET(ithc_init_device, ithc);
	CHECK(devm_device_add_groups, &pci->dev, ithc_attribute_groups);
	if (ithc_use_rx0) CHECK_RET(ithc_dma_rx_init, ithc, 0, ithc_use_rx1 ? DEVNAME "0" : DEVNAME);
	if (ithc_use_rx1) CHECK_RET(ithc_dma_rx_init, ithc, 1, ithc_use_rx0 ? DEVNAME "1" : DEVNAME);
	CHECK_RET(ithc_dma_tx_init, ithc);

	// enable HID mode by default for non-Microsoft hardware
	if (ithc_use_hid < 0) ithc_use_hid = ithc->config.vendor_id != 0x045e;

	if (ithc_use_hid) CHECK_RET(ithc_hid_init, ithc);
	else CHECK_RET(ithc_input_init, ithc);

	cpu_latency_qos_add_request(&ithc->activity_qos, PM_QOS_DEFAULT_VALUE);
	timer_setup(&ithc->activity_timer, ithc_activity_timer_callback, 0);

	// add ithc_stop callback AFTER setting up DMA buffers, so that polling/irqs/DMA are disabled BEFORE the buffers are freed
	CHECK_RET(devm_add_action_or_reset, &pci->dev, ithc_stop, ithc);

	if (ithc_use_polling) {
		pci_info(pci, "using polling instead of irq\n");
		// use a thread instead of simple timer because we want to be able to sleep
		ithc->poll_thread = kthread_run(ithc_poll_thread, ithc, DEVNAME "poll");
		if (IS_ERR(ithc->poll_thread)) {
			int err = PTR_ERR(ithc->poll_thread);
			ithc->poll_thread = NULL;
			return err;
		}
	} else {
		CHECK_RET(devm_request_threaded_irq, &pci->dev, ithc->irq, NULL, ithc_interrupt_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT, DEVNAME, ithc);
	}

	if (ithc_use_rx0) ithc_dma_rx_enable(ithc, 0);
	if (ithc_use_rx1) ithc_dma_rx_enable(ithc, 1);

	// hid_add_device can only be called after irq/polling is started and DMA is enabled, because it calls ithc_hid_parse which reads the report descriptor via DMA
	if (ithc->hid) CHECK_RET(hid_add_device, ithc->hid);

	CHECK(ithc_debug_init, ithc);

	pci_dbg(pci, "started\n");
	return 0;
}

void ithc_remove(struct pci_dev *dev) {
	// all cleanup is handled by devres
}

static struct pci_driver ithc_driver = {
	.name = DEVNAME,
	.id_table = ithc_pci_tbl,
	.probe = ithc_probe,
	.remove = ithc_remove,
	//.dev_groups = ithc_attribute_groups, // could use this (since 5.14), however the attributes won't have valid values until config has been read anyway
};

static int __init ithc_init(void) {
	return pci_register_driver(&ithc_driver);
}

static void __exit ithc_exit(void) {
	pci_unregister_driver(&ithc_driver);
}

module_init(ithc_init);
module_exit(ithc_exit);

