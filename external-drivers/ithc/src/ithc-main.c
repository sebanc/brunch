// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause

#include "ithc.h"

MODULE_DESCRIPTION("Intel Touch Host Controller driver");
MODULE_LICENSE("Dual BSD/GPL");

static const struct pci_device_id ithc_pci_tbl[] = {
	{
		.vendor = PCI_VENDOR_ID_INTEL,
		.device = PCI_ANY_ID,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
		.class = PCI_CLASS_INPUT_PEN << 8,
		.class_mask = ~0,
	},
	{}
};
MODULE_DEVICE_TABLE(pci, ithc_pci_tbl);

// Module parameters

static bool ithc_use_polling = false;
module_param_named(poll, ithc_use_polling, bool, 0);
MODULE_PARM_DESC(poll, "Use polling instead of interrupts");

// Since all known devices seem to use only channel 1, by default we disable channel 0.
static bool ithc_use_rx0 = false;
module_param_named(rx0, ithc_use_rx0, bool, 0);
MODULE_PARM_DESC(rx0, "Use DMA RX channel 0");

static bool ithc_use_rx1 = true;
module_param_named(rx1, ithc_use_rx1, bool, 0);
MODULE_PARM_DESC(rx1, "Use DMA RX channel 1");

static int ithc_active_ltr_us = -1;
module_param_named(activeltr, ithc_active_ltr_us, int, 0);
MODULE_PARM_DESC(activeltr, "Active LTR value override (in microseconds)");

static int ithc_idle_ltr_us = -1;
module_param_named(idleltr, ithc_idle_ltr_us, int, 0);
MODULE_PARM_DESC(idleltr, "Idle LTR value override (in microseconds)");

static unsigned int ithc_idle_delay_ms = 1000;
module_param_named(idledelay, ithc_idle_delay_ms, uint, 0);
MODULE_PARM_DESC(idleltr, "Minimum idle time before applying idle LTR value (in milliseconds)");

static bool ithc_log_regs_enabled = false;
module_param_named(logregs, ithc_log_regs_enabled, bool, 0);
MODULE_PARM_DESC(logregs, "Log changes in register values (for debugging)");

// Interrupts/polling

static void ithc_disable_interrupts(struct ithc *ithc)
{
	writel(0, &ithc->regs->error_control);
	bitsb(&ithc->regs->spi_cmd.control, SPI_CMD_CONTROL_IRQ, 0);
	bitsb(&ithc->regs->dma_rx[0].control, DMA_RX_CONTROL_IRQ_UNKNOWN_1 | DMA_RX_CONTROL_IRQ_ERROR | DMA_RX_CONTROL_IRQ_READY | DMA_RX_CONTROL_IRQ_DATA, 0);
	bitsb(&ithc->regs->dma_rx[1].control, DMA_RX_CONTROL_IRQ_UNKNOWN_1 | DMA_RX_CONTROL_IRQ_ERROR | DMA_RX_CONTROL_IRQ_READY | DMA_RX_CONTROL_IRQ_DATA, 0);
	bitsb(&ithc->regs->dma_tx.control, DMA_TX_CONTROL_IRQ, 0);
}

static void ithc_clear_dma_rx_interrupts(struct ithc *ithc, unsigned int channel)
{
	writel(DMA_RX_STATUS_ERROR | DMA_RX_STATUS_READY | DMA_RX_STATUS_HAVE_DATA,
		&ithc->regs->dma_rx[channel].status);
}

static void ithc_clear_interrupts(struct ithc *ithc)
{
	writel(0xffffffff, &ithc->regs->error_flags);
	writel(ERROR_STATUS_DMA | ERROR_STATUS_SPI, &ithc->regs->error_status);
	writel(SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR, &ithc->regs->spi_cmd.status);
	ithc_clear_dma_rx_interrupts(ithc, 0);
	ithc_clear_dma_rx_interrupts(ithc, 1);
	writel(DMA_TX_STATUS_DONE | DMA_TX_STATUS_ERROR | DMA_TX_STATUS_UNKNOWN_2,
		&ithc->regs->dma_tx.status);
}

static void ithc_idle_timer_callback(struct timer_list *t)
{
	struct ithc *ithc = container_of(t, struct ithc, idle_timer);
	ithc_set_ltr_idle(ithc);
}

static void ithc_process(struct ithc *ithc)
{
	ithc_log_regs(ithc);

	// The THC automatically transitions from LTR idle to active at the start of a DMA transfer.
	// It does not appear to automatically go back to idle, so we switch it back after a delay.
	mod_timer(&ithc->idle_timer, jiffies + msecs_to_jiffies(ithc_idle_delay_ms));

	bool rx0 = ithc_use_rx0 && (readl(&ithc->regs->dma_rx[0].status) & (DMA_RX_STATUS_ERROR | DMA_RX_STATUS_HAVE_DATA)) != 0;
	bool rx1 = ithc_use_rx1 && (readl(&ithc->regs->dma_rx[1].status) & (DMA_RX_STATUS_ERROR | DMA_RX_STATUS_HAVE_DATA)) != 0;

	// Read and clear error bits
	u32 err = readl(&ithc->regs->error_flags);
	if (err) {
		writel(err, &ithc->regs->error_flags);
		if (err & ~ERROR_FLAG_DMA_RX_TIMEOUT)
			pci_err(ithc->pci, "error flags: 0x%08x\n", err);
		if (err & ERROR_FLAG_DMA_RX_TIMEOUT)
			pci_err(ithc->pci, "DMA RX timeout/error (try decreasing activeltr/idleltr if this happens frequently)\n");
	}

	// Process DMA rx
	if (ithc_use_rx0) {
		ithc_clear_dma_rx_interrupts(ithc, 0);
		if (rx0)
			ithc_dma_rx(ithc, 0);
	}
	if (ithc_use_rx1) {
		ithc_clear_dma_rx_interrupts(ithc, 1);
		if (rx1)
			ithc_dma_rx(ithc, 1);
	}

	ithc_log_regs(ithc);
}

static irqreturn_t ithc_interrupt_thread(int irq, void *arg)
{
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

static int ithc_poll_thread(void *arg)
{
	struct ithc *ithc = arg;
	unsigned int sleep = 100;
	while (!kthread_should_stop()) {
		u32 n = ithc->dma_rx[1].num_received;
		ithc_process(ithc);
		// Decrease polling interval to 20ms if we received data, otherwise slowly
		// increase it up to 200ms.
		sleep = n != ithc->dma_rx[1].num_received ? 20
			: min(200u, sleep + (sleep >> 4) + 1);
		msleep_interruptible(sleep);
	}
	return 0;
}

// Device initialization and shutdown

static void ithc_disable(struct ithc *ithc)
{
	bitsl_set(&ithc->regs->control_bits, CONTROL_QUIESCE);
	CHECK(waitl, ithc, &ithc->regs->control_bits, CONTROL_IS_QUIESCED, CONTROL_IS_QUIESCED);
	bitsl(&ithc->regs->control_bits, CONTROL_NRESET, 0);
	bitsb(&ithc->regs->spi_cmd.control, SPI_CMD_CONTROL_SEND, 0);
	bitsb(&ithc->regs->dma_tx.control, DMA_TX_CONTROL_SEND, 0);
	bitsb(&ithc->regs->dma_rx[0].control, DMA_RX_CONTROL_ENABLE, 0);
	bitsb(&ithc->regs->dma_rx[1].control, DMA_RX_CONTROL_ENABLE, 0);
	ithc_disable_interrupts(ithc);
	ithc_clear_interrupts(ithc);
}

static int ithc_init_device(struct ithc *ithc)
{
	// Read ACPI config for QuickSPI mode
	struct ithc_acpi_config cfg = { 0 };
	CHECK_RET(ithc_read_acpi_config, ithc, &cfg);
	if (!cfg.has_config)
		pci_info(ithc->pci, "no ACPI config, using legacy mode\n");
	else
		ithc_print_acpi_config(ithc, &cfg);
	ithc->use_quickspi = cfg.has_config;

	// Shut down device
	ithc_log_regs(ithc);
	bool was_enabled = (readl(&ithc->regs->control_bits) & CONTROL_NRESET) != 0;
	ithc_disable(ithc);
	CHECK_RET(waitl, ithc, &ithc->regs->control_bits, CONTROL_READY, CONTROL_READY);
	ithc_log_regs(ithc);

	// If the device was previously enabled, wait a bit to make sure it's fully shut down.
	if (was_enabled)
		if (msleep_interruptible(100))
			return -EINTR;

	// Set Latency Tolerance Reporting config. The device will automatically
	// apply these values depending on whether it is active or idle.
	// If active value is too high, DMA buffer data can become truncated.
	// By default, we set the active LTR value to 50us, and idle to 100ms.
	u64 active_ltr_ns = ithc_active_ltr_us >= 0 ? (u64)ithc_active_ltr_us * 1000
		: cfg.has_config && cfg.has_active_ltr ? (u64)cfg.active_ltr << 10
		: 50 * 1000;
	u64 idle_ltr_ns = ithc_idle_ltr_us >= 0 ? (u64)ithc_idle_ltr_us * 1000
		: cfg.has_config && cfg.has_idle_ltr ? (u64)cfg.idle_ltr << 10
		: 100 * 1000 * 1000;
	ithc_set_ltr_config(ithc, active_ltr_ns, idle_ltr_ns);

	if (ithc->use_quickspi)
		CHECK_RET(ithc_quickspi_init, ithc, &cfg);
	else
		CHECK_RET(ithc_legacy_init, ithc);

	return 0;
}

int ithc_reset(struct ithc *ithc)
{
	// FIXME This should probably do devres_release_group()+ithc_start().
	// But because this is called during DMA processing, that would have to be done
	// asynchronously (schedule_work()?). And with extra locking?
	pci_err(ithc->pci, "reset\n");
	CHECK(ithc_init_device, ithc);
	if (ithc_use_rx0)
		ithc_dma_rx_enable(ithc, 0);
	if (ithc_use_rx1)
		ithc_dma_rx_enable(ithc, 1);
	ithc_log_regs(ithc);
	pci_dbg(ithc->pci, "reset completed\n");
	return 0;
}

static void ithc_stop(void *res)
{
	struct ithc *ithc = res;
	pci_dbg(ithc->pci, "stopping\n");
	ithc_log_regs(ithc);

	if (ithc->poll_thread)
		CHECK(kthread_stop, ithc->poll_thread);
	if (ithc->irq >= 0)
		disable_irq(ithc->irq);
	if (ithc->use_quickspi)
		ithc_quickspi_exit(ithc);
	else
		ithc_legacy_exit(ithc);
	ithc_disable(ithc);
	del_timer_sync(&ithc->idle_timer);

	// Clear DMA config.
	for (unsigned int i = 0; i < 2; i++) {
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

static void ithc_clear_drvdata(void *res)
{
	struct pci_dev *pci = res;
	pci_set_drvdata(pci, NULL);
}

static int ithc_start(struct pci_dev *pci)
{
	pci_dbg(pci, "starting\n");
	if (pci_get_drvdata(pci)) {
		pci_err(pci, "device already initialized\n");
		return -EINVAL;
	}
	if (!devres_open_group(&pci->dev, ithc_start, GFP_KERNEL))
		return -ENOMEM;

	// Allocate/init main driver struct.
	struct ithc *ithc = devm_kzalloc(&pci->dev, sizeof(*ithc), GFP_KERNEL);
	if (!ithc)
		return -ENOMEM;
	ithc->irq = -1;
	ithc->pci = pci;
	snprintf(ithc->phys, sizeof(ithc->phys), "pci-%s/" DEVNAME, pci_name(pci));
	pci_set_drvdata(pci, ithc);
	CHECK_RET(devm_add_action_or_reset, &pci->dev, ithc_clear_drvdata, pci);
	if (ithc_log_regs_enabled)
		ithc->prev_regs = devm_kzalloc(&pci->dev, sizeof(*ithc->prev_regs), GFP_KERNEL);

	// PCI initialization.
	CHECK_RET(pcim_enable_device, pci);
	pci_set_master(pci);
	CHECK_RET(pcim_iomap_regions, pci, BIT(0), DEVNAME " regs");
	CHECK_RET(dma_set_mask_and_coherent, &pci->dev, DMA_BIT_MASK(64));
	CHECK_RET(pci_set_power_state, pci, PCI_D0);
	ithc->regs = pcim_iomap_table(pci)[0];

	// Allocate IRQ.
	if (!ithc_use_polling) {
		CHECK_RET(pci_alloc_irq_vectors, pci, 1, 1, PCI_IRQ_MSI | PCI_IRQ_MSIX);
		ithc->irq = CHECK(pci_irq_vector, pci, 0);
		if (ithc->irq < 0)
			return ithc->irq;
	}

	// Initialize THC and touch device.
	CHECK_RET(ithc_init_device, ithc);

	// Initialize HID and DMA.
	CHECK_RET(ithc_hid_init, ithc);
	if (ithc_use_rx0)
		CHECK_RET(ithc_dma_rx_init, ithc, 0);
	if (ithc_use_rx1)
		CHECK_RET(ithc_dma_rx_init, ithc, 1);
	CHECK_RET(ithc_dma_tx_init, ithc);

	timer_setup(&ithc->idle_timer, ithc_idle_timer_callback, 0);

	// Add ithc_stop() callback AFTER setting up DMA buffers, so that polling/irqs/DMA are
	// disabled BEFORE the buffers are freed.
	CHECK_RET(devm_add_action_or_reset, &pci->dev, ithc_stop, ithc);

	// Start polling/IRQ.
	if (ithc_use_polling) {
		pci_info(pci, "using polling instead of irq\n");
		// Use a thread instead of simple timer because we want to be able to sleep.
		ithc->poll_thread = kthread_run(ithc_poll_thread, ithc, DEVNAME "poll");
		if (IS_ERR(ithc->poll_thread)) {
			int err = PTR_ERR(ithc->poll_thread);
			ithc->poll_thread = NULL;
			return err;
		}
	} else {
		CHECK_RET(devm_request_threaded_irq, &pci->dev, ithc->irq, NULL,
			ithc_interrupt_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT, DEVNAME, ithc);
	}

	if (ithc_use_rx0)
		ithc_dma_rx_enable(ithc, 0);
	if (ithc_use_rx1)
		ithc_dma_rx_enable(ithc, 1);

	// hid_add_device() can only be called after irq/polling is started and DMA is enabled,
	// because it calls ithc_hid_parse() which reads the report descriptor via DMA.
	CHECK_RET(hid_add_device, ithc->hid.dev);

	CHECK(ithc_debug_init_device, ithc);

	ithc_set_ltr_idle(ithc);

	pci_dbg(pci, "started\n");
	return 0;
}

static int ithc_probe(struct pci_dev *pci, const struct pci_device_id *id)
{
	pci_dbg(pci, "device probe\n");
	return ithc_start(pci);
}

static void ithc_remove(struct pci_dev *pci)
{
	pci_dbg(pci, "device remove\n");
	// all cleanup is handled by devres
}

// For suspend/resume, we just deinitialize and reinitialize everything.
// TODO It might be cleaner to keep the HID device around, however we would then have to signal
// to userspace that the touch device has lost state and userspace needs to e.g. resend 'set
// feature' requests. Hidraw does not seem to have a facility to do that.
static int ithc_suspend(struct device *dev)
{
	struct pci_dev *pci = to_pci_dev(dev);
	pci_dbg(pci, "pm suspend\n");
	devres_release_group(dev, ithc_start);
	return 0;
}

static int ithc_resume(struct device *dev)
{
	struct pci_dev *pci = to_pci_dev(dev);
	pci_dbg(pci, "pm resume\n");
	return ithc_start(pci);
}

static int ithc_freeze(struct device *dev)
{
	struct pci_dev *pci = to_pci_dev(dev);
	pci_dbg(pci, "pm freeze\n");
	devres_release_group(dev, ithc_start);
	return 0;
}

static int ithc_thaw(struct device *dev)
{
	struct pci_dev *pci = to_pci_dev(dev);
	pci_dbg(pci, "pm thaw\n");
	return ithc_start(pci);
}

static int ithc_restore(struct device *dev)
{
	struct pci_dev *pci = to_pci_dev(dev);
	pci_dbg(pci, "pm restore\n");
	return ithc_start(pci);
}

static struct pci_driver ithc_driver = {
	.name = DEVNAME,
	.id_table = ithc_pci_tbl,
	.probe = ithc_probe,
	.remove = ithc_remove,
	.driver.pm = &(const struct dev_pm_ops) {
		.suspend = ithc_suspend,
		.resume = ithc_resume,
		.freeze = ithc_freeze,
		.thaw = ithc_thaw,
		.restore = ithc_restore,
	},
	.driver.probe_type = PROBE_PREFER_ASYNCHRONOUS,
};

static int __init ithc_init(void)
{
	ithc_debug_init_module();
	return pci_register_driver(&ithc_driver);
}

static void __exit ithc_exit(void)
{
	pci_unregister_driver(&ithc_driver);
	ithc_debug_exit_module();
}

module_init(ithc_init);
module_exit(ithc_exit);

