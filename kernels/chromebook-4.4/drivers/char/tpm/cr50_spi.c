/*
 * Copyright (C) 2016 Google, Inc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * This device driver implements a TCG PTP FIFO interface over SPI for chips
 * with Cr50 firmware.
 * It is based on tpm_tis_spi driver by Peter Huewe and Christophe Ricard.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>
#include "cr50.h"
#include "tpm_tis_core.h"

/*
 * Cr50 timing constants:
 * - can go to sleep not earlier than after CR50_SLEEP_DELAY_MSEC.
 * - needs up to CR50_WAKE_START_DELAY_MSEC to wake after sleep.
 * - requires waiting for "ready" IRQ, if supported; or waiting for at least
 *   CR50_NOIRQ_ACCESS_DELAY_MSEC between transactions, if IRQ is not supported.
 * - waits for up to CR50_FLOW_CONTROL_MSEC for flow control 'ready' indication.
 */
#define CR50_SLEEP_DELAY_MSEC			1000
#define CR50_WAKE_START_DELAY_MSEC		1
#define CR50_NOIRQ_ACCESS_DELAY_MSEC		2
#define CR50_READY_IRQ_TIMEOUT_MSEC		TPM2_TIMEOUT_A
#define CR50_FLOW_CONTROL_MSEC			TPM2_TIMEOUT_A
#define MAX_IRQ_CONFIRMATION_ATTEMPTS		3

#define MAX_SPI_FRAMESIZE			64

#define TPM_CR50_FW_VER(l)			(0x0F90 | ((l) << 12))
#define TPM_CR50_MAX_FW_VER_LEN			64

struct cr50_spi_phy {
	struct tpm_tis_data priv;
	struct spi_device *spi_device;

	struct mutex time_track_mutex;
	unsigned long last_access_jiffies;
	unsigned long wake_after_jiffies;

	unsigned long access_delay_jiffies;
	unsigned long sleep_delay_jiffies;
	unsigned int wake_start_delay_msec;

	struct completion tpm_ready;

	unsigned int irq_confirmation_attempt;
	bool irq_needs_confirmation;
	bool irq_confirmed;

	u8 tx_buf[MAX_SPI_FRAMESIZE] ____cacheline_aligned;
	u8 rx_buf[MAX_SPI_FRAMESIZE] ____cacheline_aligned;
};

static struct cr50_spi_phy *to_cr50_spi_phy(struct tpm_tis_data *data)
{
	return container_of(data, struct cr50_spi_phy, priv);
}

/*
 * The cr50 interrupt handler just signals waiting threads that the
 * interrupt was asserted.  It does not do any processing triggered
 * by interrupts but is instead used to avoid fixed delays.
 */
static irqreturn_t cr50_spi_irq_handler(int dummy, void *dev_id)
{
	struct cr50_spi_phy *phy = dev_id;

	phy->irq_confirmed = true;
	complete(&phy->tpm_ready);

	return IRQ_HANDLED;
}

/*
 * Cr50 needs to have at least some delay between consecutive
 * transactions. Make sure we wait.
 */
static void cr50_ensure_access_delay(struct cr50_spi_phy *phy)
{
	/*
	 * Note: There is a small chance, if Cr50 is not accessed in a few days,
	 * that time_in_range will not provide the correct result after the wrap
	 * around for jiffies. In this case, we'll have an unneeded short delay,
	 * which is fine.
	 */
	unsigned long allowed_access =
		phy->last_access_jiffies + phy->access_delay_jiffies;
	unsigned long time_now = jiffies;

	if (time_in_range_open(time_now,
			       phy->last_access_jiffies, allowed_access)) {
		unsigned long remaining =
			wait_for_completion_timeout(&phy->tpm_ready,
						    allowed_access - time_now);
		if (remaining == 0 && phy->irq_confirmed) {
			dev_warn(&phy->spi_device->dev,
				 "Timeout waiting for TPM ready IRQ");
		}
	}
	if (phy->irq_needs_confirmation) {
		if (phy->irq_confirmed) {
			phy->irq_needs_confirmation = false;
			phy->access_delay_jiffies =
				msecs_to_jiffies(CR50_READY_IRQ_TIMEOUT_MSEC);
			dev_info(&phy->spi_device->dev,
				 "TPM ready IRQ confirmed on attempt %u.\n",
				 phy->irq_confirmation_attempt);
		} else if (++phy->irq_confirmation_attempt >
			   MAX_IRQ_CONFIRMATION_ATTEMPTS) {
			phy->irq_needs_confirmation = false;
			dev_warn(&phy->spi_device->dev,
				 "IRQ not confirmed - will use delays.\n");
		}
	}
}

/*
 * Cr50 might go to sleep if there is no SPI activity for some time and
 * miss the first few bits/bytes on the bus. In such case, wake it up
 * by asserting CS and give it time to start up.
 */
static bool cr50_needs_waking(struct cr50_spi_phy *phy)
{
	/*
	 * Note: There is a small chance, if Cr50 is not accessed in a few days,
	 * that time_in_range will not provide the correct result after the wrap
	 * around for jiffies. In this case, we'll probably timeout or read
	 * incorrect value from TPM_STS and just retry the operation.
	 */
	return !time_in_range_open(jiffies,
				   phy->last_access_jiffies,
				   phy->wake_after_jiffies);
}

static void cr50_wake_if_needed(struct cr50_spi_phy *phy)
{
	if (cr50_needs_waking(phy)) {
		/* Assert CS, wait 1 msec, deassert CS */
		struct spi_transfer spi_cs_wake = { .delay_usecs = 1000 };

		spi_sync_transfer(phy->spi_device, &spi_cs_wake, 1);
		/* Wait for it to fully wake */
		msleep(phy->wake_start_delay_msec);
	}
	/* Reset the time when we need to wake Cr50 again */
	phy->wake_after_jiffies = jiffies + phy->sleep_delay_jiffies;
}

/*
 * Flow control: clock the bus and wait for cr50 to set LSB before
 * sending/receiving data. TCG PTP spec allows it to happen during
 * the last byte of header, but cr50 never does that in practice,
 * and earlier versions had a bug when it was set too early, so don't
 * check for it during header transfer.
 */
static int cr50_spi_flow_control(struct cr50_spi_phy *phy)
{
	unsigned long timeout_jiffies =
		jiffies + msecs_to_jiffies(CR50_FLOW_CONTROL_MSEC);
	struct spi_message m;
	int ret;
	struct spi_transfer spi_xfer = {
		.rx_buf = phy->rx_buf,
		.len = 1,
		.cs_change = 1,
	};

	do {
		spi_message_init(&m);
		spi_message_add_tail(&spi_xfer, &m);
		ret = spi_sync_locked(phy->spi_device, &m);
		if (ret < 0)
			return ret;
		if (time_after(jiffies, timeout_jiffies)) {
			dev_warn(&phy->spi_device->dev,
				 "Timeout during flow control\n");
			return -EBUSY;
		}
	} while (!(phy->rx_buf[0] & 0x01));
	return 0;
}

static int cr50_spi_xfer_bytes(struct tpm_tis_data *data, u32 addr,
			       u16 len, u8 *buf, bool do_write)
{
	struct cr50_spi_phy *phy = to_cr50_spi_phy(data);
	struct spi_message m;
	struct spi_transfer spi_xfer = {
		.tx_buf = phy->tx_buf,
		.rx_buf = phy->rx_buf,
		.len = 4,
		.cs_change = 1,
	};
	struct spi_transfer spi_cs_deassert = {};
	int ret;

	if (len > MAX_SPI_FRAMESIZE)
		return -EINVAL;

	/*
	 * Do this outside of spi_bus_lock in case cr50 is not the
	 * only device on that spi bus.
	 */
	mutex_lock(&phy->time_track_mutex);
	cr50_ensure_access_delay(phy);
	cr50_wake_if_needed(phy);

	phy->tx_buf[0] = (do_write ? 0x00 : 0x80) | (len - 1);
	phy->tx_buf[1] = 0xD4;
	phy->tx_buf[2] = (addr >> 8) & 0xFF;
	phy->tx_buf[3] = addr & 0xFF;

	spi_message_init(&m);
	spi_message_add_tail(&spi_xfer, &m);

	spi_bus_lock(phy->spi_device->master);
	ret = spi_sync_locked(phy->spi_device, &m);
	if (ret < 0)
		goto err;

	ret = cr50_spi_flow_control(phy);
	if (ret < 0)
		goto err;

	spi_xfer.cs_change = 0;
	spi_xfer.len = len;
	if (do_write) {
		memcpy(phy->tx_buf, buf, len);
		spi_xfer.rx_buf = NULL;
	} else {
		spi_xfer.tx_buf = NULL;
	}

	spi_message_init(&m);
	spi_message_add_tail(&spi_xfer, &m);
	reinit_completion(&phy->tpm_ready);
	ret = spi_sync_locked(phy->spi_device, &m);
	if (ret < 0)
		goto err;
	if (!do_write)
		memcpy(buf, phy->rx_buf, len);
	goto done;

err:
	/* Send an empty message to deassert CS - it could have been
	 * left asserted if we exited before the payload transmission.
	 */
	spi_message_init(&m);
	spi_message_add_tail(&spi_cs_deassert, &m);
	spi_sync_locked(phy->spi_device, &m);

done:
	spi_bus_unlock(phy->spi_device->master);
	phy->last_access_jiffies = jiffies;
	mutex_unlock(&phy->time_track_mutex);

	return ret;
}

static int cr50_spi_read_bytes(struct tpm_tis_data *data, u32 addr,
			       u16 len, u8 *result)
{
	return cr50_spi_xfer_bytes(data, addr, len, result, false);
}

static int cr50_spi_write_bytes(struct tpm_tis_data *data, u32 addr,
				u16 len, u8 *value)
{
	return cr50_spi_xfer_bytes(data, addr, len, value, true);
}

static int cr50_spi_read16(struct tpm_tis_data *data, u32 addr, u16 *result)
{
	int rc;
	__le16 le_val;

	rc = data->phy_ops->read_bytes(data, addr, sizeof(u16), (u8 *)&le_val);
	if (!rc)
		*result = le16_to_cpu(le_val);
	return rc;
}

static int cr50_spi_read32(struct tpm_tis_data *data, u32 addr, u32 *result)
{
	int rc;
	__le32 le_val;

	rc = data->phy_ops->read_bytes(data, addr, sizeof(u32), (u8 *)&le_val);
	if (!rc)
		*result = le32_to_cpu(le_val);
	return rc;
}

static int cr50_spi_write32(struct tpm_tis_data *data, u32 addr, u32 value)
{
	__le32 le_val = cpu_to_le32(value);

	return data->phy_ops->write_bytes(data, addr, sizeof(u32),
					   (u8 *)&le_val);
}

static void cr50_get_fw_version(struct tpm_tis_data *data, char *fw_ver)
{
	int i, len = 0;
	char fw_ver_block[4];

	/*
	 * Write anything to TPM_CR50_FW_VER to start from the beginning
	 * of the version string
	 */
	tpm_tis_write8(data, TPM_CR50_FW_VER(data->locality), 0);

	/* Read the string, 4 bytes at a time, until we get '\0' */
	do {
		tpm_tis_read_bytes(data, TPM_CR50_FW_VER(data->locality), 4,
				   fw_ver_block);
		for (i = 0; i < 4 && fw_ver_block[i]; ++len, ++i)
			fw_ver[len] = fw_ver_block[i];
	} while (i == 4 && len < TPM_CR50_MAX_FW_VER_LEN);
	fw_ver[len] = 0;
}

static const struct tpm_tis_phy_ops cr50_spi_phy_ops = {
	.read_bytes = cr50_spi_read_bytes,
	.write_bytes = cr50_spi_write_bytes,
	.read16 = cr50_spi_read16,
	.read32 = cr50_spi_read32,
	.write32 = cr50_spi_write32,
	.max_xfer_size = MAX_SPI_FRAMESIZE,
};

static int cr50_spi_probe(struct spi_device *dev)
{
	char fw_ver[TPM_CR50_MAX_FW_VER_LEN + 1];
	struct cr50_spi_phy *phy;
	int rc;

	phy = devm_kzalloc(&dev->dev, sizeof(struct cr50_spi_phy),
			   GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->spi_device = dev;

	phy->access_delay_jiffies =
		msecs_to_jiffies(CR50_NOIRQ_ACCESS_DELAY_MSEC);
	phy->sleep_delay_jiffies = msecs_to_jiffies(CR50_SLEEP_DELAY_MSEC);
	phy->wake_start_delay_msec = CR50_WAKE_START_DELAY_MSEC;

	mutex_init(&phy->time_track_mutex);
	phy->wake_after_jiffies = jiffies;
	phy->last_access_jiffies = jiffies;

	init_completion(&phy->tpm_ready);
	if (dev->irq > 0) {
		rc = devm_request_irq(&dev->dev, dev->irq, cr50_spi_irq_handler,
				      IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				      "cr50_spi", phy);
		if (rc < 0) {
			if (rc == -EPROBE_DEFER)
				return rc;
			dev_warn(&dev->dev, "Requesting IRQ %d failed: %d\n",
				 dev->irq, rc);
			/*
			 * This is not fatal, the driver will fall back to
			 * delays automatically, since tpm_ready will never
			 * be completed without a registered irq handler.
			 * So, just fall through.
			 */
		} else {
			/*
			 * IRQ requested, let's verify that it is actually
			 * triggered, before relying on it.
			 */
			phy->irq_needs_confirmation = true;
		}
	} else {
		dev_warn(&dev->dev,
			 "No IRQ - will use delays between transactions.\n");
	}

	rc = tpm_tis_core_init(&dev->dev, &phy->priv, -1, &cr50_spi_phy_ops,
			       NULL);
	if (rc < 0)
		return rc;
	dev_info(&dev->dev, "registered shutdown handler [gentle shutdown]\n");

	cr50_get_fw_version(&phy->priv, fw_ver);
	dev_info(&dev->dev, "Cr50 firmware version: %s\n", fw_ver);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int cr50_spi_resume(struct device *dev)
{
	struct tpm_chip *chip = dev_get_drvdata(dev);
	struct tpm_tis_data *data = dev_get_drvdata(&chip->dev);
	struct cr50_spi_phy *phy = to_cr50_spi_phy(data);

	/*
	 * Jiffies not increased during suspend, so we need to reset
	 * the time to wake Cr50 after resume.
	 */
	phy->wake_after_jiffies = jiffies;

	return cr50_resume(dev);
}
#endif

static SIMPLE_DEV_PM_OPS(cr50_spi_pm, cr50_suspend, cr50_spi_resume);

static void cr50_spi_shutdown(struct spi_device *dev)
{
	struct tpm_chip *chip = spi_get_drvdata(dev);

	tpm_chip_unregister(chip);
	tpm_tis_remove(chip);
	dev_info(&dev->dev, "gentle shutdown done\n");
}

static int cr50_spi_remove(struct spi_device *dev)
{
	cr50_spi_shutdown(dev);
	return 0;
}

static const struct spi_device_id cr50_spi_id[] = {
	{ "cr50", 0 },
	{}
};
MODULE_DEVICE_TABLE(spi, cr50_spi_id);

#ifdef CONFIG_OF
static const struct of_device_id of_cr50_spi_match[] = {
	{ .compatible = "google,cr50", },
	{}
};
MODULE_DEVICE_TABLE(of, of_cr50_spi_match);
#endif

static struct spi_driver cr50_spi_driver = {
	.driver = {
		.name = "cr50_spi",
		.pm = &cr50_spi_pm,
		.of_match_table = of_match_ptr(of_cr50_spi_match),
	},
	.probe = cr50_spi_probe,
	.remove = cr50_spi_remove,
	.shutdown = cr50_spi_shutdown,
	.id_table = cr50_spi_id,
};
module_spi_driver(cr50_spi_driver);

MODULE_DESCRIPTION("Cr50 TCG PTP FIFO SPI driver");
MODULE_LICENSE("GPL v2");
