// SPDX-License-Identifier: GPL-2.0-only
/*
 * drivers/hwmon/applesmc.c - driver for Apple's SMC (accelerometer, temperature
 * sensors, fan control, keyboard backlight control) used in Intel-based Apple
 * computers.
 *
 * Copyright (C) 2007 Nicolas Boichat <nicolas@boichat.ch>
 * Copyright (C) 2010 Henrik Rydberg <rydberg@euromail.se>
 * Copyright (C) 2019 Paul Pawlowski <paul@mrarm.io>
 *
 * Based on hdaps.c driver:
 * Copyright (C) 2005 Robert Love <rml@novell.com>
 * Copyright (C) 2005 Jesper Juhl <jj@chaosbits.net>
 *
 * Fan control based on smcFanControl:
 * Copyright (C) 2006 Hendrik Holtmann <holtmann@mac.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/dmi.h>
#include <linux/mutex.h>
#include <linux/hwmon-sysfs.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/hwmon.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/bits.h>

/* data port used by Apple SMC */
#define APPLESMC_DATA_PORT	0
/* command/status port used by Apple SMC */
#define APPLESMC_CMD_PORT	4

#define APPLESMC_NR_PORTS	32 /* 0x300-0x31f */

#define APPLESMC_IOMEM_KEY_DATA	0
#define APPLESMC_IOMEM_KEY_STATUS	0x4005
#define APPLESMC_IOMEM_KEY_NAME	0x78
#define APPLESMC_IOMEM_KEY_DATA_LEN	0x7D
#define APPLESMC_IOMEM_KEY_SMC_ID	0x7E
#define APPLESMC_IOMEM_KEY_CMD		0x7F
#define APPLESMC_IOMEM_MIN_SIZE	0x4006

#define APPLESMC_IOMEM_KEY_TYPE_CODE		0
#define APPLESMC_IOMEM_KEY_TYPE_DATA_LEN	5
#define APPLESMC_IOMEM_KEY_TYPE_FLAGS		6

#define APPLESMC_MAX_DATA_LENGTH 32

/* Apple SMC status bits */
#define SMC_STATUS_AWAITING_DATA  BIT(0) /* SMC has data waiting to be read */
#define SMC_STATUS_IB_CLOSED      BIT(1) /* Will ignore any input */
#define SMC_STATUS_BUSY           BIT(2) /* Command in progress */

/* Initial wait is 8us */
#define APPLESMC_MIN_WAIT      0x0008

#define APPLESMC_READ_CMD	0x10
#define APPLESMC_WRITE_CMD	0x11
#define APPLESMC_GET_KEY_BY_INDEX_CMD	0x12
#define APPLESMC_GET_KEY_TYPE_CMD	0x13

#define KEY_COUNT_KEY		"#KEY" /* r-o ui32 */

#define LIGHT_SENSOR_LEFT_KEY	"ALV0" /* r-o {alv (6-10 bytes) */
#define LIGHT_SENSOR_RIGHT_KEY	"ALV1" /* r-o {alv (6-10 bytes) */
#define BACKLIGHT_KEY		"LKSB" /* w-o {lkb (2 bytes) */

#define CLAMSHELL_KEY		"MSLD" /* r-o ui8 (unused) */

#define MOTION_SENSOR_X_KEY	"MO_X" /* r-o sp78 (2 bytes) */
#define MOTION_SENSOR_Y_KEY	"MO_Y" /* r-o sp78 (2 bytes) */
#define MOTION_SENSOR_Z_KEY	"MO_Z" /* r-o sp78 (2 bytes) */
#define MOTION_SENSOR_KEY	"MOCN" /* r/w ui16 */

#define FANS_COUNT		"FNum" /* r-o ui8 */
#define FANS_MANUAL		"FS! " /* r-w ui16 */
#define FAN_ID_FMT		"F%dID" /* r-o char[16] */

#define TEMP_SENSOR_TYPE	"sp78"
#define FLOAT_TYPE		"flt "

/* List of keys used to read/write fan speeds */
static const char *const fan_speed_fmt[] = {
	"F%dAc",		/* actual speed */
	"F%dMn",		/* minimum speed (rw) */
	"F%dMx",		/* maximum speed */
	"F%dSf",		/* safe speed - not all models */
	"F%dTg",		/* target speed (manual: rw) */
};
#define FAN_MANUAL_FMT "F%dMd"

#define INIT_TIMEOUT_MSECS	5000	/* wait up to 5s for device init ... */
#define INIT_WAIT_MSECS		50	/* ... in 50ms increments */

#define APPLESMC_POLL_INTERVAL	50	/* msecs */
#define APPLESMC_INPUT_FUZZ	4	/* input event threshold */
#define APPLESMC_INPUT_FLAT	4

#define to_index(attr) (to_sensor_dev_attr(attr)->index & 0xffff)
#define to_option(attr) (to_sensor_dev_attr(attr)->index >> 16)

/* Dynamic device node attributes */
struct applesmc_dev_attr {
	struct sensor_device_attribute sda;	/* hwmon attributes */
	char name[32];				/* room for node file name */
};

/* Dynamic device node group */
struct applesmc_node_group {
	char *format;				/* format string */
	void *show;				/* show function */
	void *store;				/* store function */
	int option;				/* function argument */
	struct applesmc_dev_attr *nodes;	/* dynamic node array */
};

/* AppleSMC entry - cached register information */
struct applesmc_entry {
	char key[5];		/* four-letter key code */
	u8 valid;		/* set when entry is successfully read once */
	u8 len;			/* bounded by APPLESMC_MAX_DATA_LENGTH */
	char type[5];		/* four-letter type code */
	u8 flags;		/* 0x10: func; 0x40: write; 0x80: read */
};

/* Register lookup and registers common to all SMCs */
struct applesmc_registers {
	struct mutex mutex;		/* register read/write mutex */
	unsigned int key_count;		/* number of SMC registers */
	unsigned int fan_count;		/* number of fans */
	unsigned int temp_count;	/* number of temperature registers */
	unsigned int temp_begin;	/* temperature lower index bound */
	unsigned int temp_end;		/* temperature upper index bound */
	unsigned int index_count;	/* size of temperature index array */
	int num_light_sensors;		/* number of light sensors */
	bool has_accelerometer;		/* has motion sensor */
	bool has_key_backlight;		/* has keyboard backlight */
	bool init_complete;		/* true when fully initialized */
	struct applesmc_entry *cache;	/* cached key entries */
	const char **index;		/* temperature key index */
};

struct applesmc_device {
	struct acpi_device *dev;
	struct device *ldev;
	struct applesmc_registers reg;

	bool port_base_set, iomem_base_set;
	u16 port_base;
	u8 *__iomem iomem_base;
	u32 iomem_base_addr, iomem_base_size;

	s16 rest_x;
	s16 rest_y;

	u8 backlight_state[2];

	struct device *hwmon_dev;
	struct input_dev *idev;

	/*
	 * Last index written to key_at_index sysfs file, and value to use for all other
	 * key_at_index_* sysfs files.
	 */
	unsigned int key_at_index;

	struct workqueue_struct *backlight_wq;
	struct work_struct backlight_work;
	struct led_classdev backlight_dev;
};

static const int debug;

/*
 * Wait for specific status bits with a mask on the SMC.
 * Used before all transactions.
 * This does 10 fast loops of 8us then exponentially backs off for a
 * minimum total wait of 262ms. Depending on usleep_range this could
 * run out past 500ms.
 */

static int port_wait_status(struct applesmc_device *smc, u8 val, u8 mask)
{
	u8 status;
	int us;
	int i;

	us = APPLESMC_MIN_WAIT;
	for (i = 0; i < 24 ; i++) {
		status = inb(smc->port_base + APPLESMC_CMD_PORT);
		if ((status & mask) == val)
			return 0;
		usleep_range(us, us * 2);
		if (i > 9)
			us <<= 1;
	}
	return -EIO;
}

/* port_send_byte - Write to SMC data port. Callers must hold applesmc_lock. */

static int port_send_byte(struct applesmc_device *smc, u8 cmd, u16 port)
{
	int status;

	status = port_wait_status(smc, 0, SMC_STATUS_IB_CLOSED);
	if (status)
		return status;
	/*
	 * This needs to be a separate read looking for bit 0x04
	 * after bit 0x02 falls. If consolidated with the wait above
	 * this extra read may not happen if status returns both
	 * simultaneously and this would appear to be required.
	 */
	status = port_wait_status(smc, SMC_STATUS_BUSY, SMC_STATUS_BUSY);
	if (status)
		return status;

	outb(cmd, smc->port_base + port);
	return 0;
}

/* port_send_command - Write a command to the SMC. Callers must hold applesmc_lock. */

static int port_send_command(struct applesmc_device *smc, u8 cmd)
{
	int ret;

	ret = port_wait_status(smc, 0, SMC_STATUS_IB_CLOSED);
	if (ret)
		return ret;

	outb(cmd, smc->port_base + APPLESMC_CMD_PORT);
	return 0;
}

/*
 * Based on logic from the Apple driver. This is issued before any interaction
 * If busy is stuck high, issue a read command to reset the SMC state machine.
 * If busy is stuck high after the command then the SMC is jammed.
 */

static int port_smc_sane(struct applesmc_device *smc)
{
	int ret;

	ret = port_wait_status(smc, 0, SMC_STATUS_BUSY);
	if (!ret)
		return ret;
	ret = port_send_command(smc, APPLESMC_READ_CMD);
	if (ret)
		return ret;
	return port_wait_status(smc, 0, SMC_STATUS_BUSY);
}

static int port_send_argument(struct applesmc_device *smc, const char *key)
{
	int i;

	for (i = 0; i < 4; i++)
		if (port_send_byte(smc, key[i], APPLESMC_DATA_PORT))
			return -EIO;
	return 0;
}

static int port_read_smc(struct applesmc_device *smc, u8 cmd, const char *key,
	u8 *buffer, u8 len)
{
	u8 status, data = 0;
	int i;
	int ret;

	ret = port_smc_sane(smc);
	if (ret)
		return ret;

	if (port_send_command(smc, cmd) || port_send_argument(smc, key)) {
		pr_warn("%.4s: read arg fail\n", key);
		return -EIO;
	}

	/* This has no effect on newer (2012) SMCs */
	if (port_send_byte(smc, len, APPLESMC_DATA_PORT)) {
		pr_warn("%.4s: read len fail\n", key);
		return -EIO;
	}

	for (i = 0; i < len; i++) {
		if (port_wait_status(smc,
				SMC_STATUS_AWAITING_DATA | SMC_STATUS_BUSY,
				SMC_STATUS_AWAITING_DATA | SMC_STATUS_BUSY)) {
			pr_warn("%.4s: read data[%d] fail\n", key, i);
			return -EIO;
		}
		buffer[i] = inb(smc->port_base + APPLESMC_DATA_PORT);
	}

	/* Read the data port until bit0 is cleared */
	for (i = 0; i < 16; i++) {
		udelay(APPLESMC_MIN_WAIT);
		status = inb(smc->port_base + APPLESMC_CMD_PORT);
		if (!(status & SMC_STATUS_AWAITING_DATA))
			break;
		data = inb(smc->port_base + APPLESMC_DATA_PORT);
	}
	if (i)
		pr_warn("flushed %d bytes, last value is: %d\n", i, data);

	return port_wait_status(smc, 0, SMC_STATUS_BUSY);
}

static int port_write_smc(struct applesmc_device *smc, u8 cmd, const char *key,
	const u8 *buffer, u8 len)
{
	int i;
	int ret;

	ret = port_smc_sane(smc);
	if (ret)
		return ret;

	if (port_send_command(smc, cmd) || port_send_argument(smc, key)) {
		pr_warn("%s: write arg fail\n", key);
		return -EIO;
	}

	if (port_send_byte(smc, len, APPLESMC_DATA_PORT)) {
		pr_warn("%.4s: write len fail\n", key);
		return -EIO;
	}

	for (i = 0; i < len; i++) {
		if (port_send_byte(smc, buffer[i], APPLESMC_DATA_PORT)) {
			pr_warn("%s: write data fail\n", key);
			return -EIO;
		}
	}

	return port_wait_status(smc, 0, SMC_STATUS_BUSY);
}

static int port_get_smc_key_info(struct applesmc_device *smc,
	const char *key, struct applesmc_entry *info)
{
	int ret;
	u8 raw[6];

	ret = port_read_smc(smc, APPLESMC_GET_KEY_TYPE_CMD, key, raw, 6);
	if (ret)
		return ret;
	info->len = raw[0];
	memcpy(info->type, &raw[1], 4);
	info->flags = raw[5];
	return 0;
}


/*
 * MMIO based communication.
 * TODO: Use updated mechanism for cmd timeout/retry
 */

static void iomem_clear_status(struct applesmc_device *smc)
{
	if (ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_STATUS))
		iowrite8(0, smc->iomem_base + APPLESMC_IOMEM_KEY_STATUS);
}

static int iomem_wait_read(struct applesmc_device *smc)
{
	u8 status;
	int us;
	int i;

	us = APPLESMC_MIN_WAIT;
	for (i = 0; i < 24 ; i++) {
		status = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_STATUS);
		if (status & 0x20)
			return 0;
		usleep_range(us, us * 2);
		if (i > 9)
			us <<= 1;
	}

	dev_warn(smc->ldev, "%s... timeout\n", __func__);
	return -EIO;
}

static int iomem_read_smc(struct applesmc_device *smc, u8 cmd, const char *key,
	u8 *buffer, u8 len)
{
	u8 err, remote_len;
	u32 key_int = *((u32 *) key);

	iomem_clear_status(smc);
	iowrite32(key_int, smc->iomem_base + APPLESMC_IOMEM_KEY_NAME);
	iowrite32(0, smc->iomem_base + APPLESMC_IOMEM_KEY_SMC_ID);
	iowrite32(cmd, smc->iomem_base + APPLESMC_IOMEM_KEY_CMD);

	if (iomem_wait_read(smc))
		return -EIO;

	err = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_CMD);
	if (err != 0) {
		dev_warn(smc->ldev, "read_smc_mmio(%x %8x/%.4s) failed: %u\n",
				cmd, key_int, key, err);
		return -EIO;
	}

	if (cmd == APPLESMC_READ_CMD) {
		remote_len = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_DATA_LEN);
		if (remote_len != len) {
			dev_warn(smc->ldev,
				 "read_smc_mmio(%x %8x/%.4s) failed: buffer length mismatch (remote = %u, requested = %u)\n",
				 cmd, key_int, key, remote_len, len);
			return -EINVAL;
		}
	} else {
		remote_len = len;
	}

	memcpy_fromio(buffer, smc->iomem_base + APPLESMC_IOMEM_KEY_DATA,
			remote_len);

	dev_dbg(smc->ldev, "read_smc_mmio(%x %8x/%.4s): buflen=%u reslen=%u\n",
			cmd, key_int, key, len, remote_len);
	print_hex_dump_bytes("read_smc_mmio(): ", DUMP_PREFIX_NONE, buffer, remote_len);
	return 0;
}

static int iomem_get_smc_key_type(struct applesmc_device *smc, const char *key,
	struct applesmc_entry *e)
{
	u8 err;
	u8 cmd = APPLESMC_GET_KEY_TYPE_CMD;
	u32 key_int = *((u32 *) key);

	iomem_clear_status(smc);
	iowrite32(key_int, smc->iomem_base + APPLESMC_IOMEM_KEY_NAME);
	iowrite32(0, smc->iomem_base + APPLESMC_IOMEM_KEY_SMC_ID);
	iowrite32(cmd, smc->iomem_base + APPLESMC_IOMEM_KEY_CMD);

	if (iomem_wait_read(smc))
		return -EIO;

	err = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_CMD);
	if (err != 0) {
		dev_warn(smc->ldev, "get_smc_key_type_mmio(%.4s) failed: %u\n", key, err);
		return -EIO;
	}

	e->len = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_TYPE_DATA_LEN);
	*((uint32_t *) e->type) = ioread32(
			smc->iomem_base + APPLESMC_IOMEM_KEY_TYPE_CODE);
	e->flags = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_TYPE_FLAGS);

	dev_dbg(smc->ldev, "get_smc_key_type_mmio(%.4s): len=%u type=%.4s flags=%x\n",
		key, e->len, e->type, e->flags);
	return 0;
}

static int iomem_write_smc(struct applesmc_device *smc, u8 cmd, const char *key,
	const u8 *buffer, u8 len)
{
	u8 err;
	u32 key_int = *((u32 *) key);

	iomem_clear_status(smc);
	iowrite32(key_int, smc->iomem_base + APPLESMC_IOMEM_KEY_NAME);
	memcpy_toio(smc->iomem_base + APPLESMC_IOMEM_KEY_DATA, buffer, len);
	iowrite32(len, smc->iomem_base + APPLESMC_IOMEM_KEY_DATA_LEN);
	iowrite32(0, smc->iomem_base + APPLESMC_IOMEM_KEY_SMC_ID);
	iowrite32(cmd, smc->iomem_base + APPLESMC_IOMEM_KEY_CMD);

	if (iomem_wait_read(smc))
		return -EIO;

	err = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_CMD);
	if (err != 0) {
		dev_warn(smc->ldev, "write_smc_mmio(%x %.4s) failed: %u\n", cmd, key, err);
		print_hex_dump_bytes("write_smc_mmio(): ", DUMP_PREFIX_NONE, buffer, len);
		return -EIO;
	}

	dev_dbg(smc->ldev, "write_smc_mmio(%x %.4s): buflen=%u\n", cmd, key, len);
	print_hex_dump_bytes("write_smc_mmio(): ", DUMP_PREFIX_NONE, buffer, len);
	return 0;
}


static int read_smc(struct applesmc_device *smc, const char *key,
	u8 *buffer, u8 len)
{
	if (smc->iomem_base_set)
		return iomem_read_smc(smc, APPLESMC_READ_CMD, key, buffer, len);
	else
		return port_read_smc(smc, APPLESMC_READ_CMD, key, buffer, len);
}

static int write_smc(struct applesmc_device *smc, const char *key,
	const u8 *buffer, u8 len)
{
	if (smc->iomem_base_set)
		return iomem_write_smc(smc, APPLESMC_WRITE_CMD, key, buffer, len);
	else
		return port_write_smc(smc, APPLESMC_WRITE_CMD, key, buffer, len);
}

static int get_smc_key_by_index(struct applesmc_device *smc,
	unsigned int index, char *key)
{
	__be32 be;

	be = cpu_to_be32(index);
	if (smc->iomem_base_set)
		return iomem_read_smc(smc, APPLESMC_GET_KEY_BY_INDEX_CMD,
							  (const char *) &be, (u8 *) key, 4);
	else
		return port_read_smc(smc, APPLESMC_GET_KEY_BY_INDEX_CMD,
							 (const char *) &be, (u8 *) key, 4);
}

static int get_smc_key_info(struct applesmc_device *smc, const char *key,
	struct applesmc_entry *info)
{
	if (smc->iomem_base_set)
		return iomem_get_smc_key_type(smc, key, info);
	else
		return port_get_smc_key_info(smc, key, info);
}

static int read_register_count(struct applesmc_device *smc,
	unsigned int *count)
{
	__be32 be;
	int ret;

	ret = read_smc(smc, KEY_COUNT_KEY, (u8 *)&be, 4);
	if (ret < 0)
		return ret;

	*count = be32_to_cpu(be);
	return 0;
}

/*
 * Serialized I/O
 *
 * Returns zero on success or a negative error on failure.
 * All functions below are concurrency safe - callers should NOT hold lock.
 */

static int applesmc_read_entry(struct applesmc_device *smc,
	const struct applesmc_entry *entry, u8 *buf, u8 len)
{
	int ret;

	if (entry->len != len)
		return -EINVAL;
	mutex_lock(&smc->reg.mutex);
	ret = read_smc(smc, entry->key, buf, len);
	mutex_unlock(&smc->reg.mutex);

	return ret;
}

static int applesmc_write_entry(struct applesmc_device *smc,
	const struct applesmc_entry *entry, const u8 *buf, u8 len)
{
	int ret;

	if (entry->len != len)
		return -EINVAL;
	mutex_lock(&smc->reg.mutex);
	ret = write_smc(smc, entry->key, buf, len);
	mutex_unlock(&smc->reg.mutex);
	return ret;
}

static const struct applesmc_entry *applesmc_get_entry_by_index(
	struct applesmc_device *smc, int index)
{
	struct applesmc_entry *cache = &smc->reg.cache[index];
	char key[4];
	int ret = 0;

	if (cache->valid)
		return cache;

	mutex_lock(&smc->reg.mutex);

	if (cache->valid)
		goto out;
	ret = get_smc_key_by_index(smc, index, key);
	if (ret)
		goto out;
	memcpy(cache->key, key, 4);

	ret = get_smc_key_info(smc, key, cache);
	if (ret)
		goto out;
	cache->valid = 1;

out:
	mutex_unlock(&smc->reg.mutex);
	if (ret)
		return ERR_PTR(ret);
	return cache;
}

static int applesmc_get_lower_bound(struct applesmc_device *smc,
	unsigned int *lo, const char *key)
{
	int begin = 0, end = smc->reg.key_count;
	const struct applesmc_entry *entry;

	while (begin != end) {
		int middle = begin + (end - begin) / 2;
		entry = applesmc_get_entry_by_index(smc, middle);
		if (IS_ERR(entry)) {
			*lo = 0;
			return PTR_ERR(entry);
		}
		if (strcmp(entry->key, key) < 0)
			begin = middle + 1;
		else
			end = middle;
	}

	*lo = begin;
	return 0;
}

static int applesmc_get_upper_bound(struct applesmc_device *smc,
	unsigned int *hi, const char *key)
{
	int begin = 0, end = smc->reg.key_count;
	const struct applesmc_entry *entry;

	while (begin != end) {
		int middle = begin + (end - begin) / 2;
		entry = applesmc_get_entry_by_index(smc, middle);
		if (IS_ERR(entry)) {
			*hi = smc->reg.key_count;
			return PTR_ERR(entry);
		}
		if (strcmp(key, entry->key) < 0)
			end = middle;
		else
			begin = middle + 1;
	}

	*hi = begin;
	return 0;
}

static const struct applesmc_entry *applesmc_get_entry_by_key(
	struct applesmc_device *smc, const char *key)
{
	int begin, end;
	int ret;

	ret = applesmc_get_lower_bound(smc, &begin, key);
	if (ret)
		return ERR_PTR(ret);
	ret = applesmc_get_upper_bound(smc, &end, key);
	if (ret)
		return ERR_PTR(ret);
	if (end - begin != 1)
		return ERR_PTR(-EINVAL);

	return applesmc_get_entry_by_index(smc, begin);
}

static int applesmc_read_key(struct applesmc_device *smc,
	const char *key, u8 *buffer, u8 len)
{
	const struct applesmc_entry *entry;

	entry = applesmc_get_entry_by_key(smc, key);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	return applesmc_read_entry(smc, entry, buffer, len);
}

static int applesmc_write_key(struct applesmc_device *smc,
	const char *key, const u8 *buffer, u8 len)
{
	const struct applesmc_entry *entry;

	entry = applesmc_get_entry_by_key(smc, key);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	return applesmc_write_entry(smc, entry, buffer, len);
}

static int applesmc_has_key(struct applesmc_device *smc,
	const char *key, bool *value)
{
	const struct applesmc_entry *entry;

	entry = applesmc_get_entry_by_key(smc, key);
	if (IS_ERR(entry) && PTR_ERR(entry) != -EINVAL)
		return PTR_ERR(entry);

	*value = !IS_ERR(entry);
	return 0;
}

/*
 * applesmc_read_s16 - Read 16-bit signed big endian register
 */
static int applesmc_read_s16(struct applesmc_device *smc,
	const char *key, s16 *value)
{
	u8 buffer[2];
	int ret;

	ret = applesmc_read_key(smc, key, buffer, 2);
	if (ret)
		return ret;

	*value = ((s16)buffer[0] << 8) | buffer[1];
	return 0;
}

/**
 * applesmc_float_to_u32 - Retrieve the integral part of a float.
 * This is needed because Apple made fans use float values in the T2.
 * The fractional point is not significantly useful though, and the integral
 * part can be easily extracted.
 */
static inline u32 applesmc_float_to_u32(u32 d)
{
	u8 sign = (u8) ((d >> 31) & 1);
	s32 exp = (s32) ((d >> 23) & 0xff) - 0x7f;
	u32 fr = d & ((1u << 23) - 1);

	if (sign || exp < 0)
		return 0;

	return (u32) ((1u << exp) + (fr >> (23 - exp)));
}

/**
 * applesmc_u32_to_float - Convert an u32 into a float.
 * See applesmc_float_to_u32 for a rationale.
 */
static inline u32 applesmc_u32_to_float(u32 d)
{
	u32 dc = d, bc = 0, exp;

	if (!d)
		return 0;

	while (dc >>= 1)
		++bc;
	exp = 0x7f + bc;

	return (u32) ((exp << 23) |
		((d << (23 - (exp - 0x7f))) & ((1u << 23) - 1)));
}
/*
 * applesmc_device_init - initialize the accelerometer.  Can sleep.
 */
static void applesmc_device_init(struct applesmc_device *smc)
{
	int total;
	u8 buffer[2];

	if (!smc->reg.has_accelerometer)
		return;

	for (total = INIT_TIMEOUT_MSECS; total > 0; total -= INIT_WAIT_MSECS) {
		if (!applesmc_read_key(smc, MOTION_SENSOR_KEY, buffer, 2) &&
				(buffer[0] != 0x00 || buffer[1] != 0x00))
			return;
		buffer[0] = 0xe0;
		buffer[1] = 0x00;
		applesmc_write_key(smc, MOTION_SENSOR_KEY, buffer, 2);
		msleep(INIT_WAIT_MSECS);
	}

	pr_warn("failed to init the device\n");
}

static int applesmc_init_index(struct applesmc_device *smc,
	struct applesmc_registers *s)
{
	const struct applesmc_entry *entry;
	unsigned int i;

	if (s->index)
		return 0;

	s->index = kcalloc(s->temp_count, sizeof(s->index[0]), GFP_KERNEL);
	if (!s->index)
		return -ENOMEM;

	for (i = s->temp_begin; i < s->temp_end; i++) {
		entry = applesmc_get_entry_by_index(smc, i);
		if (IS_ERR(entry))
			continue;
		if (strcmp(entry->type, TEMP_SENSOR_TYPE))
			continue;
		s->index[s->index_count++] = entry->key;
	}

	return 0;
}

/*
 * applesmc_init_smcreg_try - Try to initialize register cache. Idempotent.
 */
static int applesmc_init_smcreg_try(struct applesmc_device *smc)
{
	struct applesmc_registers *s = &smc->reg;
	bool left_light_sensor = 0, right_light_sensor = 0;
	unsigned int count;
	u8 tmp[1];
	int ret;

	if (s->init_complete)
		return 0;

	ret = read_register_count(smc, &count);
	if (ret)
		return ret;

	if (s->cache && s->key_count != count) {
		pr_warn("key count changed from %d to %d\n",
			s->key_count, count);
		kfree(s->cache);
		s->cache = NULL;
	}
	s->key_count = count;

	if (!s->cache)
		s->cache = kcalloc(s->key_count, sizeof(*s->cache), GFP_KERNEL);
	if (!s->cache)
		return -ENOMEM;

	ret = applesmc_read_key(smc, FANS_COUNT, tmp, 1);
	if (ret)
		return ret;
	s->fan_count = tmp[0];
	if (s->fan_count > 10)
		s->fan_count = 10;

	ret = applesmc_get_lower_bound(smc, &s->temp_begin, "T");
	if (ret)
		return ret;
	ret = applesmc_get_lower_bound(smc, &s->temp_end, "U");
	if (ret)
		return ret;
	s->temp_count = s->temp_end - s->temp_begin;

	ret = applesmc_init_index(smc, s);
	if (ret)
		return ret;

	ret = applesmc_has_key(smc, LIGHT_SENSOR_LEFT_KEY, &left_light_sensor);
	if (ret)
		return ret;
	ret = applesmc_has_key(smc, LIGHT_SENSOR_RIGHT_KEY, &right_light_sensor);
	if (ret)
		return ret;
	ret = applesmc_has_key(smc, MOTION_SENSOR_KEY, &s->has_accelerometer);
	if (ret)
		return ret;
	ret = applesmc_has_key(smc, BACKLIGHT_KEY, &s->has_key_backlight);
	if (ret)
		return ret;

	s->num_light_sensors = left_light_sensor + right_light_sensor;
	s->init_complete = true;

	pr_info("key=%d fan=%d temp=%d index=%d acc=%d lux=%d kbd=%d\n",
	       s->key_count, s->fan_count, s->temp_count, s->index_count,
	       s->has_accelerometer,
	       s->num_light_sensors,
	       s->has_key_backlight);

	return 0;
}

static void applesmc_destroy_smcreg(struct applesmc_device *smc)
{
	kfree(smc->reg.index);
	smc->reg.index = NULL;
	kfree(smc->reg.cache);
	smc->reg.cache = NULL;
	smc->reg.init_complete = false;
}

/*
 * applesmc_init_smcreg - Initialize register cache.
 *
 * Retries until initialization is successful, or the operation times out.
 *
 */
static int applesmc_init_smcreg(struct applesmc_device *smc)
{
	int ms, ret;

	for (ms = 0; ms < INIT_TIMEOUT_MSECS; ms += INIT_WAIT_MSECS) {
		ret = applesmc_init_smcreg_try(smc);
		if (!ret) {
			if (ms)
				pr_info("init_smcreg() took %d ms\n", ms);
			return 0;
		}
		msleep(INIT_WAIT_MSECS);
	}

	applesmc_destroy_smcreg(smc);

	return ret;
}

/* Device model stuff */

static int applesmc_init_resources(struct applesmc_device *smc);
static void applesmc_free_resources(struct applesmc_device *smc);
static int applesmc_create_modules(struct applesmc_device *smc);
static void applesmc_destroy_modules(struct applesmc_device *smc);

static int applesmc_add(struct acpi_device *dev)
{
	struct applesmc_device *smc;
	int ret;

	smc = kzalloc(sizeof(struct applesmc_device), GFP_KERNEL);
	if (!smc)
		return -ENOMEM;
	smc->dev = dev;
	smc->ldev = &dev->dev;
	mutex_init(&smc->reg.mutex);

	dev_set_drvdata(&dev->dev, smc);

	ret = applesmc_init_resources(smc);
	if (ret)
		goto out_mem;

	ret = applesmc_init_smcreg(smc);
	if (ret)
		goto out_res;

	applesmc_device_init(smc);

	ret = applesmc_create_modules(smc);
	if (ret)
		goto out_reg;

	return 0;

out_reg:
	applesmc_destroy_smcreg(smc);
out_res:
	applesmc_free_resources(smc);
out_mem:
	dev_set_drvdata(&dev->dev, NULL);
	mutex_destroy(&smc->reg.mutex);
	kfree(smc);

	return ret;
}

static int applesmc_remove(struct acpi_device *dev)
{
	struct applesmc_device *smc = dev_get_drvdata(&dev->dev);

	applesmc_destroy_modules(smc);
	applesmc_destroy_smcreg(smc);
	applesmc_free_resources(smc);

	mutex_destroy(&smc->reg.mutex);
	kfree(smc);

	return 0;
}

static acpi_status applesmc_walk_resources(struct acpi_resource *res,
	void *data)
{
	struct applesmc_device *smc = data;

	switch (res->type) {
	case ACPI_RESOURCE_TYPE_IO:
		if (!smc->port_base_set) {
			if (res->data.io.address_length < APPLESMC_NR_PORTS)
				return AE_ERROR;
			smc->port_base = res->data.io.minimum;
			smc->port_base_set = true;
		}
		return AE_OK;

	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		if (!smc->iomem_base_set) {
			if (res->data.fixed_memory32.address_length <
					APPLESMC_IOMEM_MIN_SIZE) {
				dev_warn(smc->ldev, "found iomem but it's too small: %u\n",
						 res->data.fixed_memory32.address_length);
				return AE_OK;
			}
			smc->iomem_base_addr = res->data.fixed_memory32.address;
			smc->iomem_base_size = res->data.fixed_memory32.address_length;
			smc->iomem_base_set = true;
		}
		return AE_OK;

	case ACPI_RESOURCE_TYPE_END_TAG:
		if (smc->port_base_set)
			return AE_OK;
		else
			return AE_NOT_FOUND;

	default:
		return AE_OK;
	}
}

static int applesmc_try_enable_iomem(struct applesmc_device *smc);

static int applesmc_init_resources(struct applesmc_device *smc)
{
	int ret;

	ret = acpi_walk_resources(smc->dev->handle, METHOD_NAME__CRS,
			applesmc_walk_resources, smc);
	if (ACPI_FAILURE(ret))
		return -ENXIO;

	if (!request_region(smc->port_base, APPLESMC_NR_PORTS, "applesmc"))
		return -ENXIO;

	if (smc->iomem_base_set) {
		if (applesmc_try_enable_iomem(smc))
			smc->iomem_base_set = false;
	}

	return 0;
}

static int applesmc_try_enable_iomem(struct applesmc_device *smc)
{
	u8 test_val, ldkn_version;

	dev_dbg(smc->ldev, "Trying to enable iomem based communication\n");
	smc->iomem_base = ioremap(smc->iomem_base_addr, smc->iomem_base_size);
	if (!smc->iomem_base)
		goto out;

	/* Apple's driver does this check for some reason */
	test_val = ioread8(smc->iomem_base + APPLESMC_IOMEM_KEY_STATUS);
	if (test_val == 0xff) {
		dev_warn(smc->ldev,
			 "iomem enable failed: initial status is 0xff (is %x)\n",
			 test_val);
		goto out_iomem;
	}

	if (read_smc(smc, "LDKN", &ldkn_version, 1)) {
		dev_warn(smc->ldev, "iomem enable failed: ldkn read failed\n");
		goto out_iomem;
	}

	if (ldkn_version < 2) {
		dev_warn(smc->ldev,
			 "iomem enable failed: ldkn version %u is less than minimum (2)\n",
			 ldkn_version);
		goto out_iomem;
	}

	return 0;

out_iomem:
	iounmap(smc->iomem_base);

out:
	return -ENXIO;
}

static void applesmc_free_resources(struct applesmc_device *smc)
{
	if (smc->iomem_base_set)
		iounmap(smc->iomem_base);
	release_region(smc->port_base, APPLESMC_NR_PORTS);
}

/* Synchronize device with memorized backlight state */
static int applesmc_pm_resume(struct device *dev)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);

	if (smc->reg.has_key_backlight)
		applesmc_write_key(smc, BACKLIGHT_KEY, smc->backlight_state, 2);

	return 0;
}

/* Reinitialize device on resume from hibernation */
static int applesmc_pm_restore(struct device *dev)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);

	applesmc_device_init(smc);

	return applesmc_pm_resume(dev);
}

static const struct acpi_device_id applesmc_ids[] = {
	{"APP0001", 0},
	{"", 0},
};

static const struct dev_pm_ops applesmc_pm_ops = {
	.resume = applesmc_pm_resume,
	.restore = applesmc_pm_restore,
};

static struct acpi_driver applesmc_driver = {
	.name = "applesmc",
	.class = "applesmc",
	.ids = applesmc_ids,
	.ops = {
		.add = applesmc_add,
		.remove = applesmc_remove
	},
	.drv = {
		.pm = &applesmc_pm_ops
	},
	.owner = THIS_MODULE
};

/*
 * applesmc_calibrate - Set our "resting" values.  Callers must
 * hold applesmc_lock.
 */
static void applesmc_calibrate(struct applesmc_device *smc)
{
	applesmc_read_s16(smc, MOTION_SENSOR_X_KEY, &smc->rest_x);
	applesmc_read_s16(smc, MOTION_SENSOR_Y_KEY, &smc->rest_y);
	smc->rest_x = -smc->rest_x;
}

static void applesmc_idev_poll(struct input_dev *idev)
{
	struct applesmc_device *smc = dev_get_drvdata(&idev->dev);
	s16 x, y;

	if (applesmc_read_s16(smc, MOTION_SENSOR_X_KEY, &x))
		return;
	if (applesmc_read_s16(smc, MOTION_SENSOR_Y_KEY, &y))
		return;

	x = -x;
	input_report_abs(idev, ABS_X, x - smc->rest_x);
	input_report_abs(idev, ABS_Y, y - smc->rest_y);
	input_sync(idev);
}

/* Sysfs Files */

static ssize_t applesmc_name_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "applesmc\n");
}

static ssize_t applesmc_position_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	int ret;
	s16 x, y, z;

	ret = applesmc_read_s16(smc, MOTION_SENSOR_X_KEY, &x);
	if (ret)
		goto out;
	ret = applesmc_read_s16(smc, MOTION_SENSOR_Y_KEY, &y);
	if (ret)
		goto out;
	ret = applesmc_read_s16(smc, MOTION_SENSOR_Z_KEY, &z);
	if (ret)
		goto out;

out:
	if (ret)
		return ret;
	else
		return snprintf(buf, PAGE_SIZE, "(%d,%d,%d)\n", x, y, z);
}

static ssize_t applesmc_light_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const struct applesmc_entry *entry;
	static int data_length;
	int ret;
	u8 left = 0, right = 0;
	u8 buffer[10];

	if (!data_length) {
		entry = applesmc_get_entry_by_key(smc, LIGHT_SENSOR_LEFT_KEY);
		if (IS_ERR(entry))
			return PTR_ERR(entry);
		if (entry->len > 10)
			return -ENXIO;
		data_length = entry->len;
		pr_info("light sensor data length set to %d\n", data_length);
	}

	ret = applesmc_read_key(smc, LIGHT_SENSOR_LEFT_KEY, buffer, data_length);
	if (ret)
		goto out;
	/* newer macbooks report a single 10-bit bigendian value */
	if (data_length == 10) {
		left = be16_to_cpu(*(__be16 *)(buffer + 6)) >> 2;
		goto out;
	}
	left = buffer[2];

	ret = applesmc_read_key(smc, LIGHT_SENSOR_RIGHT_KEY, buffer, data_length);
	if (ret)
		goto out;
	right = buffer[2];

out:
	if (ret)
		return ret;
	else
		return snprintf(sysfsbuf, PAGE_SIZE, "(%d,%d)\n", left, right);
}

/* Displays sensor key as label */
static ssize_t applesmc_show_sensor_label(struct device *dev,
			struct device_attribute *devattr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const char *key = smc->reg.index[to_index(devattr)];

	return snprintf(sysfsbuf, PAGE_SIZE, "%s\n", key);
}

/* Displays degree Celsius * 1000 */
static ssize_t applesmc_show_temperature(struct device *dev,
			struct device_attribute *devattr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const char *key = smc->reg.index[to_index(devattr)];
	int ret;
	s16 value;
	int temp;

	ret = applesmc_read_s16(smc, key, &value);
	if (ret)
		return ret;

	temp = 250 * (value >> 6);

	return snprintf(sysfsbuf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t applesmc_show_fan_speed(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const struct applesmc_entry *entry;
	int ret;
	unsigned int speed = 0;
	char newkey[5];
	u8 buffer[2];

	scnprintf(newkey, sizeof(newkey), fan_speed_fmt[to_option(attr)],
		  to_index(attr));

	entry = applesmc_get_entry_by_key(smc, newkey);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	if (!strcmp(entry->type, FLOAT_TYPE)) {
		ret = applesmc_read_entry(smc, entry, (u8 *) &speed, 4);
		speed = applesmc_float_to_u32(speed);
	} else {
		ret = applesmc_read_entry(smc, entry, buffer, 2);
		speed = ((buffer[0] << 8 | buffer[1]) >> 2);
	}

	if (ret)
		return ret;

	return snprintf(sysfsbuf, PAGE_SIZE, "%u\n", speed);
}

static ssize_t applesmc_store_fan_speed(struct device *dev,
					struct device_attribute *attr,
					const char *sysfsbuf, size_t count)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const struct applesmc_entry *entry;
	int ret;
	unsigned long speed;
	char newkey[5];
	u8 buffer[2];

	if (kstrtoul(sysfsbuf, 10, &speed) < 0 || speed >= 0x4000)
		return -EINVAL;		/* Bigger than a 14-bit value */

	scnprintf(newkey, sizeof(newkey), fan_speed_fmt[to_option(attr)],
		  to_index(attr));

	entry = applesmc_get_entry_by_key(smc, newkey);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	if (!strcmp(entry->type, FLOAT_TYPE)) {
		speed = applesmc_u32_to_float(speed);
		ret = applesmc_write_entry(smc, entry, (u8 *) &speed, 4);
	} else {
		buffer[0] = (speed >> 6) & 0xff;
		buffer[1] = (speed << 2) & 0xff;
		ret = applesmc_write_key(smc, newkey, buffer, 2);
	}

	if (ret)
		return ret;
	else
		return count;
}

static ssize_t applesmc_show_fan_manual(struct device *dev,
			struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	int ret;
	u16 manual = 0;
	u8 buffer[2];
	char newkey[5];
	bool has_newkey = false;

	scnprintf(newkey, sizeof(newkey), FAN_MANUAL_FMT, to_index(attr));

	ret = applesmc_has_key(smc, newkey, &has_newkey);
	if (ret)
		return ret;

	if (has_newkey) {
		ret = applesmc_read_key(smc, newkey, buffer, 1);
		manual = buffer[0];
	} else {
		ret = applesmc_read_key(smc, FANS_MANUAL, buffer, 2);
		manual = ((buffer[0] << 8 | buffer[1]) >> to_index(attr)) & 0x01;
	}

	if (ret)
		return ret;

	return snprintf(sysfsbuf, PAGE_SIZE, "%d\n", manual);
}

static ssize_t applesmc_store_fan_manual(struct device *dev,
					 struct device_attribute *attr,
					 const char *sysfsbuf, size_t count)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	int ret;
	u8 buffer[2];
	char newkey[5];
	bool has_newkey = false;
	unsigned long input;
	u16 val;

	if (kstrtoul(sysfsbuf, 10, &input) < 0)
		return -EINVAL;

	scnprintf(newkey, sizeof(newkey), FAN_MANUAL_FMT, to_index(attr));

	ret = applesmc_has_key(smc, newkey, &has_newkey);
	if (ret)
		return ret;

	if (has_newkey) {
		buffer[0] = input & 1;
		ret = applesmc_write_key(smc, newkey, buffer, 1);
	} else {
		ret = applesmc_read_key(smc, FANS_MANUAL, buffer, 2);
		val = (buffer[0] << 8 | buffer[1]);
		if (ret)
			goto out;

		if (input)
			val = val | (0x01 << to_index(attr));
		else
			val = val & ~(0x01 << to_index(attr));

		buffer[0] = (val >> 8) & 0xFF;
		buffer[1] = val & 0xFF;

		ret = applesmc_write_key(smc, FANS_MANUAL, buffer, 2);
	}

out:
	if (ret)
		return ret;
	else
		return count;
}

static ssize_t applesmc_show_fan_position(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	int ret;
	char newkey[5];
	u8 buffer[17];

	scnprintf(newkey, sizeof(newkey), FAN_ID_FMT, to_index(attr));

	ret = applesmc_read_key(smc, newkey, buffer, 16);
	buffer[16] = 0;

	if (ret)
		return ret;
	else
		return snprintf(sysfsbuf, PAGE_SIZE, "%s\n", buffer+4);
}

static ssize_t applesmc_calibrate_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);

	return snprintf(sysfsbuf, PAGE_SIZE, "(%d,%d)\n", smc->rest_x, smc->rest_y);
}

static ssize_t applesmc_calibrate_store(struct device *dev,
	struct device_attribute *attr, const char *sysfsbuf, size_t count)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);

	applesmc_calibrate(smc);

	return count;
}

static void applesmc_backlight_set(struct work_struct *work)
{
	struct applesmc_device *smc = container_of(work, struct applesmc_device, backlight_work);

	applesmc_write_key(smc, BACKLIGHT_KEY, smc->backlight_state, 2);
}

static void applesmc_brightness_set(struct led_classdev *led_cdev,
						enum led_brightness value)
{
	struct applesmc_device *smc = dev_get_drvdata(led_cdev->dev);
	int ret;

	smc->backlight_state[0] = value;
	ret = queue_work(smc->backlight_wq, &smc->backlight_work);

	if (debug && (!ret))
		dev_dbg(led_cdev->dev, "work was already on the queue.\n");
}

static ssize_t applesmc_key_count_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	int ret;
	u8 buffer[4];
	u32 count;

	ret = applesmc_read_key(smc, KEY_COUNT_KEY, buffer, 4);
	if (ret)
		return ret;

	count = ((u32)buffer[0]<<24) + ((u32)buffer[1]<<16) +
						((u32)buffer[2]<<8) + buffer[3];
	return snprintf(sysfsbuf, PAGE_SIZE, "%d\n", count);
}

static ssize_t applesmc_key_at_index_read_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const struct applesmc_entry *entry;
	int ret;

	entry = applesmc_get_entry_by_index(smc, smc->key_at_index);
	if (IS_ERR(entry))
		return PTR_ERR(entry);
	ret = applesmc_read_entry(smc, entry, sysfsbuf, entry->len);
	if (ret)
		return ret;

	return entry->len;
}

static ssize_t applesmc_key_at_index_data_length_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const struct applesmc_entry *entry;

	entry = applesmc_get_entry_by_index(smc, smc->key_at_index);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	return snprintf(sysfsbuf, PAGE_SIZE, "%d\n", entry->len);
}

static ssize_t applesmc_key_at_index_type_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const struct applesmc_entry *entry;

	entry = applesmc_get_entry_by_index(smc, smc->key_at_index);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	return snprintf(sysfsbuf, PAGE_SIZE, "%s\n", entry->type);
}

static ssize_t applesmc_key_at_index_name_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	const struct applesmc_entry *entry;

	entry = applesmc_get_entry_by_index(smc, smc->key_at_index);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	return snprintf(sysfsbuf, PAGE_SIZE, "%s\n", entry->key);
}

static ssize_t applesmc_key_at_index_show(struct device *dev,
				struct device_attribute *attr, char *sysfsbuf)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);

	return snprintf(sysfsbuf, PAGE_SIZE, "%d\n", smc->key_at_index);
}

static ssize_t applesmc_key_at_index_store(struct device *dev,
	struct device_attribute *attr, const char *sysfsbuf, size_t count)
{
	struct applesmc_device *smc = dev_get_drvdata(dev);
	unsigned long newkey;

	if (kstrtoul(sysfsbuf, 10, &newkey) < 0
	    || newkey >= smc->reg.key_count)
		return -EINVAL;

	smc->key_at_index = newkey;
	return count;
}

static struct applesmc_node_group info_group[] = {
	{ "name", applesmc_name_show },
	{ "key_count", applesmc_key_count_show },
	{ "key_at_index", applesmc_key_at_index_show, applesmc_key_at_index_store },
	{ "key_at_index_name", applesmc_key_at_index_name_show },
	{ "key_at_index_type", applesmc_key_at_index_type_show },
	{ "key_at_index_data_length", applesmc_key_at_index_data_length_show },
	{ "key_at_index_data", applesmc_key_at_index_read_show },
	{ }
};

static struct applesmc_node_group accelerometer_group[] = {
	{ "position", applesmc_position_show },
	{ "calibrate", applesmc_calibrate_show, applesmc_calibrate_store },
	{ }
};

static struct applesmc_node_group light_sensor_group[] = {
	{ "light", applesmc_light_show },
	{ }
};

static struct applesmc_node_group fan_group[] = {
	{ "fan%d_label", applesmc_show_fan_position },
	{ "fan%d_input", applesmc_show_fan_speed, NULL, 0 },
	{ "fan%d_min", applesmc_show_fan_speed, applesmc_store_fan_speed, 1 },
	{ "fan%d_max", applesmc_show_fan_speed, NULL, 2 },
	{ "fan%d_safe", applesmc_show_fan_speed, NULL, 3 },
	{ "fan%d_output", applesmc_show_fan_speed, applesmc_store_fan_speed, 4 },
	{ "fan%d_manual", applesmc_show_fan_manual, applesmc_store_fan_manual },
	{ }
};

static struct applesmc_node_group temp_group[] = {
	{ "temp%d_label", applesmc_show_sensor_label },
	{ "temp%d_input", applesmc_show_temperature },
	{ }
};

/* Module stuff */

/*
 * applesmc_destroy_nodes - remove files and free associated memory
 */
static void applesmc_destroy_nodes(struct applesmc_device *smc,
	struct applesmc_node_group *groups)
{
	struct applesmc_node_group *grp;
	struct applesmc_dev_attr *node;

	for (grp = groups; grp->nodes; grp++) {
		for (node = grp->nodes; node->sda.dev_attr.attr.name; node++)
			sysfs_remove_file(&smc->dev->dev.kobj,
					  &node->sda.dev_attr.attr);
		kfree(grp->nodes);
		grp->nodes = NULL;
	}
}

/*
 * applesmc_create_nodes - create a two-dimensional group of sysfs files
 */
static int applesmc_create_nodes(struct applesmc_device *smc,
	struct applesmc_node_group *groups, int num)
{
	struct applesmc_node_group *grp;
	struct applesmc_dev_attr *node;
	struct attribute *attr;
	int ret, i;

	for (grp = groups; grp->format; grp++) {
		grp->nodes = kcalloc(num + 1, sizeof(*node), GFP_KERNEL);
		if (!grp->nodes) {
			ret = -ENOMEM;
			goto out;
		}
		for (i = 0; i < num; i++) {
			node = &grp->nodes[i];
			scnprintf(node->name, sizeof(node->name), grp->format,
				  i + 1);
			node->sda.index = (grp->option << 16) | (i & 0xffff);
			node->sda.dev_attr.show = grp->show;
			node->sda.dev_attr.store = grp->store;
			attr = &node->sda.dev_attr.attr;
			sysfs_attr_init(attr);
			attr->name = node->name;
			attr->mode = 0444 | (grp->store ? 0200 : 0);
			ret = sysfs_create_file(&smc->dev->dev.kobj, attr);
			if (ret) {
				attr->name = NULL;
				goto out;
			}
		}
	}

	return 0;
out:
	applesmc_destroy_nodes(smc, groups);
	return ret;
}

/* Create accelerometer resources */
static int applesmc_create_accelerometer(struct applesmc_device *smc)
{
	int ret;
	if (!smc->reg.has_accelerometer)
		return 0;

	ret = applesmc_create_nodes(smc, accelerometer_group, 1);
	if (ret)
		goto out;

	smc->idev = input_allocate_device();
	if (!smc->idev) {
		ret = -ENOMEM;
		goto out_sysfs;
	}

	/* initial calibrate for the input device */
	applesmc_calibrate(smc);

	/* initialize the input device */
	smc->idev->name = "applesmc";
	smc->idev->id.bustype = BUS_HOST;
	smc->idev->dev.parent = &smc->dev->dev;
	input_set_abs_params(smc->idev, ABS_X,
			-256, 256, APPLESMC_INPUT_FUZZ, APPLESMC_INPUT_FLAT);
	input_set_abs_params(smc->idev, ABS_Y,
			-256, 256, APPLESMC_INPUT_FUZZ, APPLESMC_INPUT_FLAT);

	ret = input_setup_polling(smc->idev, applesmc_idev_poll);
	if (ret)
		goto out_idev;

	input_set_poll_interval(smc->idev, APPLESMC_POLL_INTERVAL);

	ret = input_register_device(smc->idev);
	if (ret)
		goto out_idev;

	return 0;

out_idev:
	input_free_device(smc->idev);

out_sysfs:
	applesmc_destroy_nodes(smc, accelerometer_group);

out:
	pr_warn("driver init failed (ret=%d)!\n", ret);
	return ret;
}

/* Release all resources used by the accelerometer */
static void applesmc_release_accelerometer(struct applesmc_device *smc)
{
	if (!smc->reg.has_accelerometer)
		return;
	input_unregister_device(smc->idev);
	applesmc_destroy_nodes(smc, accelerometer_group);
}

static int applesmc_create_light_sensor(struct applesmc_device *smc)
{
	if (!smc->reg.num_light_sensors)
		return 0;
	return applesmc_create_nodes(smc, light_sensor_group, 1);
}

static void applesmc_release_light_sensor(struct applesmc_device *smc)
{
	if (!smc->reg.num_light_sensors)
		return;
	applesmc_destroy_nodes(smc, light_sensor_group);
}

static int applesmc_create_key_backlight(struct applesmc_device *smc)
{
	int ret;

	if (!smc->reg.has_key_backlight)
		return 0;
	smc->backlight_wq = create_singlethread_workqueue("applesmc-led");
	if (!smc->backlight_wq)
		return -ENOMEM;

	INIT_WORK(&smc->backlight_work, applesmc_backlight_set);
	smc->backlight_dev.name = "smc::kbd_backlight";
	smc->backlight_dev.default_trigger = "nand-disk";
	smc->backlight_dev.brightness_set = applesmc_brightness_set;
	ret = led_classdev_register(&smc->dev->dev, &smc->backlight_dev);
	if (ret)
		destroy_workqueue(smc->backlight_wq);

	return ret;
}

static void applesmc_release_key_backlight(struct applesmc_device *smc)
{
	if (!smc->reg.has_key_backlight)
		return;
	led_classdev_unregister(&smc->backlight_dev);
	destroy_workqueue(smc->backlight_wq);
}

static int applesmc_dmi_match(const struct dmi_system_id *id)
{
	return 1;
}

/*
 * Note that DMI_MATCH(...,"MacBook") will match "MacBookPro1,1".
 * So we need to put "Apple MacBook Pro" before "Apple MacBook".
 */
static const struct dmi_system_id applesmc_whitelist[] __initconst = {
	{ applesmc_dmi_match, "Apple MacBook Air", {
	  DMI_MATCH(DMI_BOARD_VENDOR, "Apple"),
	  DMI_MATCH(DMI_PRODUCT_NAME, "MacBookAir") },
	},
	{ applesmc_dmi_match, "Apple MacBook Pro", {
	  DMI_MATCH(DMI_BOARD_VENDOR, "Apple"),
	  DMI_MATCH(DMI_PRODUCT_NAME, "MacBookPro") },
	},
	{ applesmc_dmi_match, "Apple MacBook", {
	  DMI_MATCH(DMI_BOARD_VENDOR, "Apple"),
	  DMI_MATCH(DMI_PRODUCT_NAME, "MacBook") },
	},
	{ applesmc_dmi_match, "Apple Macmini", {
	  DMI_MATCH(DMI_BOARD_VENDOR, "Apple"),
	  DMI_MATCH(DMI_PRODUCT_NAME, "Macmini") },
	},
	{ applesmc_dmi_match, "Apple MacPro", {
	  DMI_MATCH(DMI_BOARD_VENDOR, "Apple"),
	  DMI_MATCH(DMI_PRODUCT_NAME, "MacPro") },
	},
	{ applesmc_dmi_match, "Apple iMac", {
	  DMI_MATCH(DMI_BOARD_VENDOR, "Apple"),
	  DMI_MATCH(DMI_PRODUCT_NAME, "iMac") },
	},
	{ .ident = NULL }
};

static int applesmc_create_modules(struct applesmc_device *smc)
{
	int ret;

	ret = applesmc_create_nodes(smc, info_group, 1);
	if (ret)
		goto out;

	ret = applesmc_create_nodes(smc, fan_group, smc->reg.fan_count);
	if (ret)
		goto out_info;

	ret = applesmc_create_nodes(smc, temp_group, smc->reg.index_count);
	if (ret)
		goto out_fans;

	ret = applesmc_create_accelerometer(smc);
	if (ret)
		goto out_temperature;

	ret = applesmc_create_light_sensor(smc);
	if (ret)
		goto out_accelerometer;

	ret = applesmc_create_key_backlight(smc);
	if (ret)
		goto out_light_sysfs;

	smc->hwmon_dev = hwmon_device_register(&smc->dev->dev);
	if (IS_ERR(smc->hwmon_dev)) {
		ret = PTR_ERR(smc->hwmon_dev);
		goto out_light_ledclass;
	}

	return 0;

out_light_ledclass:
	applesmc_release_key_backlight(smc);
out_light_sysfs:
	applesmc_release_light_sensor(smc);
out_accelerometer:
	applesmc_release_accelerometer(smc);
out_temperature:
	applesmc_destroy_nodes(smc, temp_group);
out_fans:
	applesmc_destroy_nodes(smc, fan_group);
out_info:
	applesmc_destroy_nodes(smc, info_group);
out:
	return ret;
}

static void applesmc_destroy_modules(struct applesmc_device *smc)
{
	hwmon_device_unregister(smc->hwmon_dev);
	applesmc_release_key_backlight(smc);
	applesmc_release_light_sensor(smc);
	applesmc_release_accelerometer(smc);
	applesmc_destroy_nodes(smc, temp_group);
	applesmc_destroy_nodes(smc, fan_group);
	applesmc_destroy_nodes(smc, info_group);
}

static int __init applesmc_init(void)
{
	int ret;

	if (!dmi_check_system(applesmc_whitelist)) {
		pr_warn("supported laptop not found!\n");
		ret = -ENODEV;
		goto out;
	}

	ret = acpi_bus_register_driver(&applesmc_driver);
	if (ret)
		goto out;

	return 0;

out:
	pr_warn("driver init failed (ret=%d)!\n", ret);
	return ret;
}

static void __exit applesmc_exit(void)
{
	acpi_bus_unregister_driver(&applesmc_driver);
}

module_init(applesmc_init);
module_exit(applesmc_exit);

MODULE_AUTHOR("Nicolas Boichat");
MODULE_AUTHOR("Paul Pawlowski");
MODULE_DESCRIPTION("Apple SMC");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(dmi, applesmc_whitelist);
