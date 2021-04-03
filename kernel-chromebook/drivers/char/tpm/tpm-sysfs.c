/*
 * Copyright (C) 2004 IBM Corporation
 * Authors:
 * Leendert van Doorn <leendert@watson.ibm.com>
 * Dave Safford <safford@watson.ibm.com>
 * Reiner Sailer <sailer@watson.ibm.com>
 * Kylene Hall <kjhall@us.ibm.com>
 *
 * Copyright (C) 2013 Obsidian Research Corp
 * Jason Gunthorpe <jgunthorpe@obsidianresearch.com>
 *
 * sysfs filesystem inspection interface to the TPM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 */
#include <linux/device.h>
#include "tpm.h"

#define READ_PUBEK_RESULT_SIZE 314
#define TPM_ORD_READPUBEK cpu_to_be32(124)
static struct tpm_input_header tpm_readpubek_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(30),
	.ordinal = TPM_ORD_READPUBEK
};
static ssize_t pubek_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	u8 *data;
	struct tpm_cmd_t tpm_cmd;
	ssize_t err;
	int i, rc;
	char *str = buf;

	struct tpm_chip *chip = to_tpm_chip(dev);

	memset(&tpm_cmd, 0, sizeof(tpm_cmd));

	tpm_cmd.header.in = tpm_readpubek_header;
	err = tpm_transmit_cmd(chip, &tpm_cmd, READ_PUBEK_RESULT_SIZE, 0,
			       "attempting to read the PUBEK");
	if (err)
		goto out;

	/*
	   ignore header 10 bytes
	   algorithm 32 bits (1 == RSA )
	   encscheme 16 bits
	   sigscheme 16 bits
	   parameters (RSA 12->bytes: keybit, #primes, expbit)
	   keylenbytes 32 bits
	   256 byte modulus
	   ignore checksum 20 bytes
	 */
	data = tpm_cmd.params.readpubek_out_buffer;
	str +=
	    sprintf(str,
		    "Algorithm: %02X %02X %02X %02X\n"
		    "Encscheme: %02X %02X\n"
		    "Sigscheme: %02X %02X\n"
		    "Parameters: %02X %02X %02X %02X "
		    "%02X %02X %02X %02X "
		    "%02X %02X %02X %02X\n"
		    "Modulus length: %d\n"
		    "Modulus:\n",
		    data[0], data[1], data[2], data[3],
		    data[4], data[5],
		    data[6], data[7],
		    data[12], data[13], data[14], data[15],
		    data[16], data[17], data[18], data[19],
		    data[20], data[21], data[22], data[23],
		    be32_to_cpu(*((__be32 *) (data + 24))));

	for (i = 0; i < 256; i++) {
		str += sprintf(str, "%02X ", data[i + 28]);
		if ((i + 1) % 16 == 0)
			str += sprintf(str, "\n");
	}
out:
	rc = str - buf;
	return rc;
}
static DEVICE_ATTR_RO(pubek);

static ssize_t pcrs_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	cap_t cap;
	u8 digest[TPM_DIGEST_SIZE];
	ssize_t rc;
	int i, j, num_pcrs;
	char *str = buf;
	struct tpm_chip *chip = to_tpm_chip(dev);

	rc = tpm_getcap(chip, TPM_CAP_PROP_PCR, &cap,
			"attempting to determine the number of PCRS");
	if (rc)
		return 0;

	num_pcrs = be32_to_cpu(cap.num_pcrs);
	for (i = 0; i < num_pcrs; i++) {
		rc = tpm_pcr_read_dev(chip, i, digest);
		if (rc)
			break;
		str += sprintf(str, "PCR-%02d: ", i);
		for (j = 0; j < TPM_DIGEST_SIZE; j++)
			str += sprintf(str, "%02X ", digest[j]);
		str += sprintf(str, "\n");
	}
	return str - buf;
}
static DEVICE_ATTR_RO(pcrs);

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	cap_t cap;
	ssize_t rc;

	rc = tpm_getcap(to_tpm_chip(dev), TPM_CAP_FLAG_PERM, &cap,
			"attempting to determine the permanent enabled state");
	if (rc)
		return 0;

	rc = sprintf(buf, "%d\n", !cap.perm_flags.disable);
	return rc;
}
static DEVICE_ATTR_RO(enabled);

static ssize_t active_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	cap_t cap;
	ssize_t rc;

	rc = tpm_getcap(to_tpm_chip(dev), TPM_CAP_FLAG_PERM, &cap,
			"attempting to determine the permanent active state");
	if (rc)
		return 0;

	rc = sprintf(buf, "%d\n", !cap.perm_flags.deactivated);
	return rc;
}
static DEVICE_ATTR_RO(active);

static ssize_t owned_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	cap_t cap;
	ssize_t rc;

	rc = tpm_getcap(to_tpm_chip(dev), TPM_CAP_PROP_OWNER, &cap,
			"attempting to determine the owner state");
	if (rc)
		return 0;

	rc = sprintf(buf, "%d\n", cap.owned);
	return rc;
}
static DEVICE_ATTR_RO(owned);

static ssize_t temp_deactivated_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cap_t cap;
	ssize_t rc;

	rc = tpm_getcap(to_tpm_chip(dev), TPM_CAP_FLAG_VOL, &cap,
			"attempting to determine the temporary state");
	if (rc)
		return 0;

	rc = sprintf(buf, "%d\n", cap.stclear_flags.deactivated);
	return rc;
}
static DEVICE_ATTR_RO(temp_deactivated);

static ssize_t caps_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);
	cap_t cap;
	ssize_t rc;
	char *str = buf;

	rc = tpm_getcap(chip, TPM_CAP_PROP_MANUFACTURER, &cap,
			"attempting to determine the manufacturer");
	if (rc)
		return 0;
	str += sprintf(str, "Manufacturer: 0x%x\n",
		       be32_to_cpu(cap.manufacturer_id));

	/* Try to get a TPM version 1.2 TPM_CAP_VERSION_INFO */
	rc = tpm_getcap(chip, CAP_VERSION_1_2, &cap,
			"attempting to determine the 1.2 version");
	if (!rc) {
		str += sprintf(str,
			       "TCG version: %d.%d\nFirmware version: %d.%d\n",
			       cap.tpm_version_1_2.Major,
			       cap.tpm_version_1_2.Minor,
			       cap.tpm_version_1_2.revMajor,
			       cap.tpm_version_1_2.revMinor);
	} else {
		/* Otherwise just use TPM_STRUCT_VER */
		rc = tpm_getcap(chip, CAP_VERSION_1_1, &cap,
				"attempting to determine the 1.1 version");
		if (rc)
			return 0;
		str += sprintf(str,
			       "TCG version: %d.%d\nFirmware version: %d.%d\n",
			       cap.tpm_version.Major,
			       cap.tpm_version.Minor,
			       cap.tpm_version.revMajor,
			       cap.tpm_version.revMinor);
	}

	return str - buf;
}
static DEVICE_ATTR_RO(caps);

static ssize_t cancel_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct tpm_chip *chip = to_tpm_chip(dev);
	if (chip == NULL)
		return 0;

	chip->ops->cancel(chip);
	return count;
}
static DEVICE_ATTR_WO(cancel);

static ssize_t durations_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);

	if (chip->duration[TPM_LONG] == 0)
		return 0;

	return sprintf(buf, "%d %d %d [%s]\n",
		       jiffies_to_usecs(chip->duration[TPM_SHORT]),
		       jiffies_to_usecs(chip->duration[TPM_MEDIUM]),
		       jiffies_to_usecs(chip->duration[TPM_LONG]),
		       chip->duration_adjusted
		       ? "adjusted" : "original");
}
static DEVICE_ATTR_RO(durations);

static ssize_t timeouts_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);

	return sprintf(buf, "%d %d %d %d [%s]\n",
		       jiffies_to_usecs(chip->timeout_a),
		       jiffies_to_usecs(chip->timeout_b),
		       jiffies_to_usecs(chip->timeout_c),
		       jiffies_to_usecs(chip->timeout_d),
		       chip->timeout_adjusted
		       ? "adjusted" : "original");
}
static DEVICE_ATTR_RO(timeouts);

static struct attribute *tpm1_dev_attrs[] = {
	&dev_attr_pubek.attr,
	&dev_attr_pcrs.attr,
	&dev_attr_enabled.attr,
	&dev_attr_active.attr,
	&dev_attr_owned.attr,
	&dev_attr_temp_deactivated.attr,
	&dev_attr_caps.attr,
	&dev_attr_cancel.attr,
	&dev_attr_durations.attr,
	&dev_attr_timeouts.attr,
	NULL,
};

static const struct attribute_group tpm1_dev_group = {
	.attrs = tpm1_dev_attrs,
};

struct tpm2_prop_flag_dev_attribute {
	struct device_attribute attr;
	u32 property_id;
	u32 flag_mask;
};

struct tpm2_prop_u32_dev_attribute {
	struct device_attribute attr;
	u32 property_id;
};

static ssize_t tpm2_prop_flag_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tpm2_prop_flag_dev_attribute *pa =
		container_of(attr, struct tpm2_prop_flag_dev_attribute, attr);
	u32 flags;
	ssize_t rc;

	rc = tpm2_get_tpm_pt(to_tpm_chip(dev), pa->property_id, &flags,
			     "reading property");
	if (rc)
		return 0;

	return sprintf(buf, "%d\n", !!(flags & pa->flag_mask));
}

static ssize_t tpm2_prop_u32_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct tpm2_prop_u32_dev_attribute *pa =
		container_of(attr, struct tpm2_prop_u32_dev_attribute, attr);
	u32 value;
	ssize_t rc;

	rc = tpm2_get_tpm_pt(to_tpm_chip(dev), pa->property_id, &value,
			     "reading property");
	if (rc)
		return 0;

	return sprintf(buf, "%u\n", value);
}

#define TPM2_PROP_FLAG_ATTR(_name, _property_id, _flag_mask)           \
	struct tpm2_prop_flag_dev_attribute attr_tpm2_prop_##_name = { \
		__ATTR(_name, S_IRUGO, tpm2_prop_flag_show, NULL),     \
		_property_id, _flag_mask                               \
	}

#define TPM2_PROP_U32_ATTR(_name, _property_id)                        \
	struct tpm2_prop_u32_dev_attribute attr_tpm2_prop_##_name = {  \
		__ATTR(_name, S_IRUGO, tpm2_prop_u32_show, NULL),      \
		_property_id                                           \
	}

TPM2_PROP_FLAG_ATTR(owner_auth_set,
		    TPM2_PT_PERMANENT, TPM2_ATTR_OWNER_AUTH_SET);
TPM2_PROP_FLAG_ATTR(endorsement_auth_set,
		    TPM2_PT_PERMANENT, TPM2_ATTR_ENDORSEMENT_AUTH_SET);
TPM2_PROP_FLAG_ATTR(lockout_auth_set,
		    TPM2_PT_PERMANENT, TPM2_ATTR_LOCKOUT_AUTH_SET);
TPM2_PROP_FLAG_ATTR(disable_clear,
		    TPM2_PT_PERMANENT, TPM2_ATTR_DISABLE_CLEAR);
TPM2_PROP_FLAG_ATTR(in_lockout,
		    TPM2_PT_PERMANENT, TPM2_ATTR_IN_LOCKOUT);
TPM2_PROP_FLAG_ATTR(tpm_generated_eps,
		    TPM2_PT_PERMANENT, TPM2_ATTR_TPM_GENERATED_EPS);

TPM2_PROP_FLAG_ATTR(ph_enable,
		    TPM2_PT_STARTUP_CLEAR, TPM2_ATTR_PH_ENABLE);
TPM2_PROP_FLAG_ATTR(sh_enable,
		    TPM2_PT_STARTUP_CLEAR, TPM2_ATTR_SH_ENABLE);
TPM2_PROP_FLAG_ATTR(eh_enable,
		    TPM2_PT_STARTUP_CLEAR, TPM2_ATTR_EH_ENABLE);
TPM2_PROP_FLAG_ATTR(ph_enable_nv,
		    TPM2_PT_STARTUP_CLEAR, TPM2_ATTR_PH_ENABLE_NV);
TPM2_PROP_FLAG_ATTR(orderly,
		    TPM2_PT_STARTUP_CLEAR, TPM2_ATTR_ORDERLY);

/* Aliases for userland scripts in TPM2 case */
TPM2_PROP_FLAG_ATTR(enabled,
		    TPM2_PT_STARTUP_CLEAR, TPM2_ATTR_SH_ENABLE);
TPM2_PROP_FLAG_ATTR(owned,
		    TPM2_PT_PERMANENT, TPM2_ATTR_OWNER_AUTH_SET);

TPM2_PROP_U32_ATTR(lockout_counter, TPM2_PT_LOCKOUT_COUNTER);
TPM2_PROP_U32_ATTR(max_auth_fail, TPM2_PT_MAX_AUTH_FAIL);
TPM2_PROP_U32_ATTR(lockout_interval, TPM2_PT_LOCKOUT_INTERVAL);
TPM2_PROP_U32_ATTR(lockout_recovery, TPM2_PT_LOCKOUT_RECOVERY);

#define ATTR_FOR_TPM2_PROP(_name) (&attr_tpm2_prop_##_name.attr.attr)
static struct attribute *tpm2_dev_attrs[] = {
	ATTR_FOR_TPM2_PROP(owner_auth_set),
	ATTR_FOR_TPM2_PROP(endorsement_auth_set),
	ATTR_FOR_TPM2_PROP(lockout_auth_set),
	ATTR_FOR_TPM2_PROP(disable_clear),
	ATTR_FOR_TPM2_PROP(in_lockout),
	ATTR_FOR_TPM2_PROP(tpm_generated_eps),
	ATTR_FOR_TPM2_PROP(ph_enable),
	ATTR_FOR_TPM2_PROP(sh_enable),
	ATTR_FOR_TPM2_PROP(eh_enable),
	ATTR_FOR_TPM2_PROP(ph_enable_nv),
	ATTR_FOR_TPM2_PROP(orderly),
	ATTR_FOR_TPM2_PROP(enabled),
	ATTR_FOR_TPM2_PROP(owned),
	ATTR_FOR_TPM2_PROP(lockout_counter),
	ATTR_FOR_TPM2_PROP(max_auth_fail),
	ATTR_FOR_TPM2_PROP(lockout_interval),
	ATTR_FOR_TPM2_PROP(lockout_recovery),
	&dev_attr_durations.attr,
	&dev_attr_timeouts.attr,
	NULL,
};

static const struct attribute_group tpm2_dev_group = {
	.attrs = tpm2_dev_attrs,
};

void tpm_sysfs_add_device(struct tpm_chip *chip)
{
	/* The sysfs routines rely on an implicit tpm_try_get_ops, device_del
	 * is called before ops is null'd and the sysfs core synchronizes this
	 * removal so that no callbacks are running or can run again
	 */
	WARN_ON(chip->groups_cnt != 0);
	chip->groups[chip->groups_cnt++] =
		(chip->flags & TPM_CHIP_FLAG_TPM2) ?
		&tpm2_dev_group : &tpm1_dev_group;
}
