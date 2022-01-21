// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2016 Google Inc.
 *
 * Based on Linux Kernel TPM driver by
 * Peter Huewe <peter.huewe@infineon.com>
 * Copyright (C) 2011 Infineon Technologies
 */

/*
 * cr50 is a firmware for H1 secure modules that requires special
 * handling for the I2C interface.
 *
 * - Use an interrupt for transaction status instead of hardcoded delays
 * - Must use write+wait+read read protocol
 * - All 4 bytes of status register must be read/written at once
 * - Burst count max is 63 bytes, and burst count behaves
 *   slightly differently than other I2C TPMs
 * - When reading from FIFO the full burstcnt must be read
 *   instead of just reading header and determining the remainder
 */

#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include "tpm.h"
#include "tpm_tis_core.h"

#define CR50_MAX_BUFSIZE	63
#define CR50_TIMEOUT_SHORT_MS	2	/* Short timeout during transactions */
#define CR50_TIMEOUT_NOIRQ_MS	20	/* Timeout for TPM ready without IRQ */
#define CR50_I2C_DID_VID	0x00281ae0L
#define CR50_I2C_MAX_RETRIES	3	/* Max retries due to I2C errors */
#define CR50_I2C_RETRY_DELAY_LO	55	/* Min usecs between retries on I2C */
#define CR50_I2C_RETRY_DELAY_HI	65	/* Max usecs between retries on I2C */

struct priv_data {
	int irq;
	int locality;
	struct completion tpm_ready;
	u8 buf[CR50_MAX_BUFSIZE + sizeof(u8)];
};

/*
 * The cr50 interrupt handler just signals waiting threads that the
 * interrupt was asserted.  It does not do any processing triggered
 * by interrupts but is instead used to avoid fixed delays.
 */
static irqreturn_t cr50_i2c_int_handler(int dummy, void *dev_id)
{
	struct tpm_chip *chip = dev_id;
	struct priv_data *priv = dev_get_drvdata(&chip->dev);

	complete(&priv->tpm_ready);

	return IRQ_HANDLED;
}

/*
 * Wait for completion interrupt if available, otherwise use a fixed
 * delay for the TPM to be ready.
 *
 * Returns negative number for error, positive number for success.
 */
static int cr50_i2c_wait_tpm_ready(struct tpm_chip *chip)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	long rc;

	/* Use a safe fixed delay if interrupt is not supported */
	if (priv->irq <= 0) {
		msleep(CR50_TIMEOUT_NOIRQ_MS);
		return 1;
	}

	/* Wait for interrupt to indicate TPM is ready to respond */
	rc = wait_for_completion_timeout(&priv->tpm_ready,
		msecs_to_jiffies(chip->timeout_a));

	if (rc == 0)
		dev_warn(&chip->dev, "Timeout waiting for TPM ready\n");

	return rc;
}

static void cr50_i2c_enable_tpm_irq(struct tpm_chip *chip)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);

	if (priv->irq > 0) {
		reinit_completion(&priv->tpm_ready);
		enable_irq(priv->irq);
	}
}

static void cr50_i2c_disable_tpm_irq(struct tpm_chip *chip)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);

	if (priv->irq > 0)
		disable_irq(priv->irq);
}

/*
 * cr50_i2c_transfer - transfer messages over i2c
 *
 * @adapter: i2c adapter
 * @msgs: array of messages to transfer
 * @num: number of messages in the array
 *
 * Call unlocked i2c transfer routine with the provided parameters and retry
 * in case of bus errors. Returns the number of transferred messages.
 */
static int cr50_i2c_transfer(struct device *dev, struct i2c_adapter *adapter,
			     struct i2c_msg *msgs, int num)
{
	int rc, try;

	for (try = 0; try < CR50_I2C_MAX_RETRIES; try++) {
		rc = __i2c_transfer(adapter, msgs, num);
		if (rc > 0)
			break;
		if (try)
			dev_warn(dev, "i2c transfer failed (attempt %d/%d): %d\n",
				 try+1, CR50_I2C_MAX_RETRIES, rc);
		usleep_range(CR50_I2C_RETRY_DELAY_LO, CR50_I2C_RETRY_DELAY_HI);
	}

	return rc;
}

/*
 * cr50_i2c_read() - read from TPM register
 *
 * @chip: TPM chip information
 * @addr: register address to read from
 * @buffer: provided by caller
 * @len: number of bytes to read
 *
 * 1) send register address byte 'addr' to the TPM
 * 2) wait for TPM to indicate it is ready
 * 3) read 'len' bytes of TPM response into the provided 'buffer'
 *
 * Returns negative number for error, 0 for success.
 */
static int cr50_i2c_read(struct tpm_chip *chip, u8 addr, u8 *buffer, size_t len)
{
	struct i2c_client *client = to_i2c_client(chip->dev.parent);
	struct i2c_msg msg1 = {
		.addr = client->addr,
		.len = 1,
		.buf = &addr
	};
	struct i2c_msg msg2 = {
		.addr = client->addr,
		.flags = I2C_M_RD,
		.len = len,
		.buf = buffer
	};
	int rc;

	i2c_lock_bus(client->adapter, I2C_LOCK_SEGMENT);

	/* Prepare for completion interrupt */
	cr50_i2c_enable_tpm_irq(chip);

	/* Send the register address byte to the TPM */
	rc = cr50_i2c_transfer(&chip->dev, client->adapter, &msg1, 1);
	if (rc <= 0)
		goto out;

	/* Wait for TPM to be ready with response data */
	rc = cr50_i2c_wait_tpm_ready(chip);
	if (rc < 0)
		goto out;

	/* Read response data from the TPM */
	rc = cr50_i2c_transfer(&chip->dev, client->adapter, &msg2, 1);

out:
	cr50_i2c_disable_tpm_irq(chip);
	i2c_unlock_bus(client->adapter, I2C_LOCK_SEGMENT);

	if (rc < 0)
		return rc;
	if (rc == 0)
		return -EIO; /* No i2c segments transferred */

	return 0;
}

/*
 * cr50_i2c_write() - write to TPM register
 *
 * @chip: TPM chip information
 * @addr: register address to write to
 * @buffer: data to write
 * @len: number of bytes to write
 *
 * 1) prepend the provided address to the provided data
 * 2) send the address+data to the TPM
 * 3) wait for TPM to indicate it is done writing
 *
 * Returns negative number for error, 0 for success.
 */
static int cr50_i2c_write(struct tpm_chip *chip, u8 addr, u8 *buffer,
			  size_t len)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	struct i2c_client *client = to_i2c_client(chip->dev.parent);
	struct i2c_msg msg1 = {
		.addr = client->addr,
		.len = len + 1,
		.buf = priv->buf
	};
	int rc;

	if (len > CR50_MAX_BUFSIZE)
		return -EINVAL;

	i2c_lock_bus(client->adapter, I2C_LOCK_SEGMENT);

	/* Prepend the 'register address' to the buffer */
	priv->buf[0] = addr;
	memcpy(priv->buf + 1, buffer, len);

	/* Prepare for completion interrupt */
	cr50_i2c_enable_tpm_irq(chip);

	/* Send write request buffer with address */
	rc = cr50_i2c_transfer(&chip->dev, client->adapter, &msg1, 1);
	if (rc <= 0)
		goto out;

	/* Wait for TPM to be ready, ignore timeout */
	cr50_i2c_wait_tpm_ready(chip);

out:
	cr50_i2c_disable_tpm_irq(chip);
	i2c_unlock_bus(client->adapter, I2C_LOCK_SEGMENT);

	if (rc < 0)
		return rc;
	if (rc == 0)
		return -EIO; /* No i2c segments transferred */

	return 0;
}

#undef	TPM_ACCESS
#undef	TPM_STS
#undef	TPM_DATA_FIFO
#undef	TPM_DID_VID

#define	TPM_ACCESS(l)			(0x0000 | ((l) << 4))
#define	TPM_STS(l)			(0x0001 | ((l) << 4))
#define	TPM_DATA_FIFO(l)		(0x0005 | ((l) << 4))
#define	TPM_DID_VID(l)			(0x0006 | ((l) << 4))

static int check_locality(struct tpm_chip *chip, int loc)
{
	u8 mask = TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY;
	u8 buf;
	int rc;

	rc = cr50_i2c_read(chip, TPM_ACCESS(loc), &buf, 1);
	if (rc < 0)
		return rc;

	if ((buf & mask) == mask)
		return loc;

	return -EIO;
}

static void release_locality(struct tpm_chip *chip, int force)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	u8 mask = TPM_ACCESS_VALID | TPM_ACCESS_REQUEST_PENDING;
	u8 addr = TPM_ACCESS(priv->locality);
	u8 buf;

	if (cr50_i2c_read(chip, addr, &buf, 1) < 0)
		return;

	if (force || (buf & mask) == mask) {
		buf = TPM_ACCESS_ACTIVE_LOCALITY;
		cr50_i2c_write(chip, addr, &buf, 1);
	}

	priv->locality = 0;
}

static int request_locality(struct tpm_chip *chip, int loc)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	u8 buf = TPM_ACCESS_REQUEST_USE;
	unsigned long stop;
	int rc;

	if (check_locality(chip, loc) == loc)
		return loc;

	rc = cr50_i2c_write(chip, TPM_ACCESS(loc), &buf, 1);
	if (rc < 0)
		return rc;

	stop = jiffies + chip->timeout_a;
	do {
		if (check_locality(chip, loc) == loc) {
			priv->locality = loc;
			return loc;
		}
		msleep(CR50_TIMEOUT_SHORT_MS);
	} while (time_before(jiffies, stop));

	return -ETIMEDOUT;
}

/* cr50 requires all 4 bytes of status register to be read */
static u8 cr50_i2c_tis_status(struct tpm_chip *chip)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	u8 buf[4];

	if (cr50_i2c_read(chip, TPM_STS(priv->locality), buf, sizeof(buf)) < 0)
		return 0;
	return buf[0];
}

/* cr50 requires all 4 bytes of status register to be written */
static void cr50_i2c_tis_ready(struct tpm_chip *chip)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	u8 buf[4] = { TPM_STS_COMMAND_READY };

	cr50_i2c_write(chip, TPM_STS(priv->locality), buf, sizeof(buf));
	msleep(CR50_TIMEOUT_SHORT_MS);
}

/*
 * cr50 uses bytes 3:2 of status register for burst count and
 * all 4 bytes must be read
 */
static int cr50_i2c_wait_burststs(struct tpm_chip *chip, u8 mask,
				  size_t *burst, int *status)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	unsigned long stop;
	u8 buf[4];

	/* wait for burstcount */
	stop = jiffies + chip->timeout_b;
	do {
		if (cr50_i2c_read(chip, TPM_STS(priv->locality),
				  (u8 *)&buf, sizeof(buf)) < 0) {
			msleep(CR50_TIMEOUT_SHORT_MS);
			continue;
		}

		*status = *buf;
		*burst = le16_to_cpup((__le16 *)(buf + 1));

		if ((*status & mask) == mask &&
		    *burst > 0 && *burst <= CR50_MAX_BUFSIZE)
			return 0;

		msleep(CR50_TIMEOUT_SHORT_MS);
	} while (time_before(jiffies, stop));

	dev_err(&chip->dev, "Timeout reading burst and status\n");
	return -ETIMEDOUT;
}

static int cr50_i2c_tis_recv(struct tpm_chip *chip, u8 *buf, size_t buf_len)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	int status, rc;
	size_t burstcnt, cur, len, expected;
	u8 addr = TPM_DATA_FIFO(priv->locality);
	u8 mask = TPM_STS_VALID | TPM_STS_DATA_AVAIL;

	if (buf_len < TPM_HEADER_SIZE)
		return -EINVAL;

	rc = cr50_i2c_wait_burststs(chip, mask, &burstcnt, &status);
	if (rc < 0)
		goto out_err;

	if (burstcnt > buf_len || burstcnt < TPM_HEADER_SIZE) {
		dev_err(&chip->dev,
			"Unexpected burstcnt: %zu (max=%zu, min=%d)\n",
			burstcnt, buf_len, TPM_HEADER_SIZE);
		rc = -EIO;
		goto out_err;
	}

	/* Read first chunk of burstcnt bytes */
	rc = cr50_i2c_read(chip, addr, buf, burstcnt);
	if (rc < 0) {
		dev_err(&chip->dev, "Read of first chunk failed\n");
		goto out_err;
	}

	/* Determine expected data in the return buffer */
	expected = be32_to_cpup((__be32 *)(buf + 2));
	if (expected > buf_len) {
		dev_err(&chip->dev, "Too much data in FIFO\n");
		goto out_err;
	}

	/* Now read the rest of the data */
	cur = burstcnt;
	while (cur < expected) {
		/* Read updated burst count and check status */
		rc = cr50_i2c_wait_burststs(chip, mask, &burstcnt, &status);
		if (rc < 0)
			goto out_err;

		len = min_t(size_t, burstcnt, expected - cur);
		rc = cr50_i2c_read(chip, addr, buf + cur, len);
		if (rc < 0) {
			dev_err(&chip->dev, "Read failed\n");
			goto out_err;
		}

		cur += len;
	}

	/* Ensure TPM is done reading data */
	rc = cr50_i2c_wait_burststs(chip, TPM_STS_VALID, &burstcnt, &status);
	if (rc < 0)
		goto out_err;
	if (status & TPM_STS_DATA_AVAIL) {
		dev_err(&chip->dev, "Data still available\n");
		rc = -EIO;
		goto out_err;
	}

	release_locality(chip, 0);
	return cur;

out_err:
	/* Abort current transaction if still pending */
	if (cr50_i2c_tis_status(chip) & TPM_STS_COMMAND_READY)
		cr50_i2c_tis_ready(chip);

	release_locality(chip, 0);
	return rc;
}

static int cr50_i2c_tis_send(struct tpm_chip *chip, u8 *buf, size_t len)
{
	struct priv_data *priv = dev_get_drvdata(&chip->dev);
	int rc, status;
	size_t burstcnt, limit, sent = 0;
	u8 tpm_go[4] = { TPM_STS_GO };
	unsigned long stop;

	rc = request_locality(chip, 0);
	if (rc < 0)
		return rc;

	/* Wait until TPM is ready for a command */
	stop = jiffies + chip->timeout_b;
	while (!(cr50_i2c_tis_status(chip) & TPM_STS_COMMAND_READY)) {
		if (time_after(jiffies, stop)) {
			rc = -ETIMEDOUT;
			goto out_err;
		}

		cr50_i2c_tis_ready(chip);
	}

	while (len > 0) {
		u8 mask = TPM_STS_VALID;

		/* Wait for data if this is not the first chunk */
		if (sent > 0)
			mask |= TPM_STS_DATA_EXPECT;

		/* Read burst count and check status */
		rc = cr50_i2c_wait_burststs(chip, mask, &burstcnt, &status);
		if (rc < 0)
			goto out_err;

		/*
		 * Use burstcnt - 1 to account for the address byte
		 * that is inserted by cr50_i2c_write()
		 */
		limit = min_t(size_t, burstcnt - 1, len);
		rc = cr50_i2c_write(chip, TPM_DATA_FIFO(priv->locality),
				    &buf[sent], limit);
		if (rc < 0) {
			dev_err(&chip->dev, "Write failed\n");
			goto out_err;
		}

		sent += limit;
		len -= limit;
	}

	/* Ensure TPM is not expecting more data */
	rc = cr50_i2c_wait_burststs(chip, TPM_STS_VALID, &burstcnt, &status);
	if (rc < 0)
		goto out_err;
	if (status & TPM_STS_DATA_EXPECT) {
		dev_err(&chip->dev, "Data still expected\n");
		rc = -EIO;
		goto out_err;
	}

	/* Start the TPM command */
	rc = cr50_i2c_write(chip, TPM_STS(priv->locality), tpm_go,
			    sizeof(tpm_go));
	if (rc < 0) {
		dev_err(&chip->dev, "Start command failed\n");
		goto out_err;
	}
	return 0;

out_err:
	/* Abort current transaction if still pending */
	if (cr50_i2c_tis_status(chip) & TPM_STS_COMMAND_READY)
		cr50_i2c_tis_ready(chip);

	release_locality(chip, 0);
	return rc;
}

static bool cr50_i2c_req_canceled(struct tpm_chip *chip, u8 status)
{
	return (status == TPM_STS_COMMAND_READY);
}

static const struct tpm_class_ops cr50_i2c = {
	.flags = TPM_OPS_AUTO_STARTUP,
	.status = &cr50_i2c_tis_status,
	.recv = &cr50_i2c_tis_recv,
	.send = &cr50_i2c_tis_send,
	.cancel = &cr50_i2c_tis_ready,
	.req_complete_mask = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_complete_val = TPM_STS_DATA_AVAIL | TPM_STS_VALID,
	.req_canceled = &cr50_i2c_req_canceled,
};

static int cr50_i2c_init(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct tpm_chip *chip;
	struct priv_data *priv;
	u8 buf[4];
	u32 vendor;
	int rc;

	chip = tpmm_chip_alloc(dev, &cr50_i2c);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* cr50 is a TPM 2.0 chip */
	chip->flags |= TPM_CHIP_FLAG_TPM2;
	chip->flags |= TPM_CHIP_FLAG_FIRMWARE_POWER_MANAGED;

	/* Default timeouts */
	chip->timeout_a = msecs_to_jiffies(TIS_SHORT_TIMEOUT);
	chip->timeout_b = msecs_to_jiffies(TIS_LONG_TIMEOUT);
	chip->timeout_c = msecs_to_jiffies(TIS_SHORT_TIMEOUT);
	chip->timeout_d = msecs_to_jiffies(TIS_SHORT_TIMEOUT);

	dev_set_drvdata(&chip->dev, priv);
	init_completion(&priv->tpm_ready);

	if (client->irq > 0) {
		rc = devm_request_irq(dev, client->irq, cr50_i2c_int_handler,
				      IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				      dev->driver->name, chip);
		if (rc < 0) {
			dev_err(dev, "Failed to probe IRQ %d\n", client->irq);
			return rc;
		}

		disable_irq(client->irq);
		priv->irq = client->irq;
	} else {
		dev_warn(dev, "No IRQ, will use %ums delay for TPM ready\n",
			 CR50_TIMEOUT_NOIRQ_MS);
	}

	rc = request_locality(chip, 0);
	if (rc < 0) {
		dev_err(dev, "Could not request locality\n");
		return rc;
	}

	/* Read four bytes from DID_VID register */
	rc = cr50_i2c_read(chip, TPM_DID_VID(0), buf, sizeof(buf));
	if (rc < 0) {
		dev_err(dev, "Could not read vendor id\n");
		release_locality(chip, 1);
		return rc;
	}

	vendor = le32_to_cpup((__le32 *)buf);
	if (vendor != CR50_I2C_DID_VID) {
		dev_err(dev, "Vendor ID did not match! ID was %08x\n", vendor);
		release_locality(chip, 1);
		return -ENODEV;
	}

	dev_info(dev, "cr50 TPM 2.0 (i2c 0x%02x irq %d id 0x%x)\n",
		 client->addr, client->irq, vendor >> 16);

	return tpm_chip_register(chip);
}

static const struct i2c_device_id cr50_i2c_table[] = {
	{"cr50_i2c", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cr50_i2c_table);

#ifdef CONFIG_ACPI
static const struct acpi_device_id cr50_i2c_acpi_id[] = {
	{ "GOOG0005", 0 },
	{}
};
MODULE_DEVICE_TABLE(acpi, cr50_i2c_acpi_id);
#endif

#ifdef CONFIG_OF
static const struct of_device_id of_cr50_i2c_match[] = {
	{ .compatible = "google,cr50", },
	{}
};
MODULE_DEVICE_TABLE(of, of_cr50_i2c_match);
#endif

static int cr50_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	return cr50_i2c_init(client);
}

static int cr50_i2c_remove(struct i2c_client *client)
{
	struct tpm_chip *chip = i2c_get_clientdata(client);

	tpm_chip_unregister(chip);
	release_locality(chip, 1);

	return 0;
}

static SIMPLE_DEV_PM_OPS(cr50_i2c_pm, tpm_pm_suspend, tpm_pm_resume);

static struct i2c_driver cr50_i2c_driver = {
	.id_table = cr50_i2c_table,
	.probe = cr50_i2c_probe,
	.remove = cr50_i2c_remove,
	.driver = {
		.name = "cr50_i2c",
		.pm = &cr50_i2c_pm,
		.acpi_match_table = ACPI_PTR(cr50_i2c_acpi_id),
		.of_match_table = of_match_ptr(of_cr50_i2c_match),
	},
};

module_i2c_driver(cr50_i2c_driver);

MODULE_DESCRIPTION("cr50 TPM I2C Driver");
MODULE_LICENSE("GPL");
