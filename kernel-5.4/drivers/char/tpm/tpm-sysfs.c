// SPDX-License-Identifier: GPL-2.0-only
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
 */
#include <linux/device.h>
#include "tpm.h"

struct tpm_readpubek_out {
	u8 algorithm[4];
	u8 encscheme[2];
	u8 sigscheme[2];
	__be32 paramsize;
	u8 parameters[12];
	__be32 keysize;
	u8 modulus[256];
	u8 checksum[20];
} __packed;

#define READ_PUBEK_RESULT_MIN_BODY_SIZE (28 + 256)
#define TPM_ORD_READPUBEK 124

static ssize_t pubek_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct tpm_buf tpm_buf;
	struct tpm_readpubek_out *out;
	int i;
	char *str = buf;
	struct tpm_chip *chip = to_tpm_chip(dev);
	char anti_replay[20];

	memset(&anti_replay, 0, sizeof(anti_replay));

	if (tpm_try_get_ops(chip))
		return 0;

	if (tpm_buf_init(&tpm_buf, TPM_TAG_RQU_COMMAND, TPM_ORD_READPUBEK))
		goto out_ops;

	tpm_buf_append(&tpm_buf, anti_replay, sizeof(anti_replay));

	if (tpm_transmit_cmd(chip, &tpm_buf, READ_PUBEK_RESULT_MIN_BODY_SIZE,
			     "attempting to read the PUBEK"))
		goto out_buf;

	out = (struct tpm_readpubek_out *)&tpm_buf.data[10];
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
		    out->algorithm[0], out->algorithm[1], out->algorithm[2],
		    out->algorithm[3],
		    out->encscheme[0], out->encscheme[1],
		    out->sigscheme[0], out->sigscheme[1],
		    out->parameters[0], out->parameters[1],
		    out->parameters[2], out->parameters[3],
		    out->parameters[4], out->parameters[5],
		    out->parameters[6], out->parameters[7],
		    out->parameters[8], out->parameters[9],
		    out->parameters[10], out->parameters[11],
		    be32_to_cpu(out->keysize));

	for (i = 0; i < 256; i++) {
		str += sprintf(str, "%02X ", out->modulus[i]);
		if ((i + 1) % 16 == 0)
			str += sprintf(str, "\n");
	}

out_buf:
	tpm_buf_destroy(&tpm_buf);
out_ops:
	tpm_put_ops(chip);
	return str - buf;
}
static DEVICE_ATTR_RO(pubek);

static ssize_t pcrs_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	cap_t cap;
	u8 digest[TPM_DIGEST_SIZE];
	u32 i, j, num_pcrs;
	char *str = buf;
	struct tpm_chip *chip = to_tpm_chip(dev);

	if (tpm_try_get_ops(chip))
		return 0;

	if (tpm1_getcap(chip, TPM_CAP_PROP_PCR, &cap,
			"attempting to determine the number of PCRS",
			sizeof(cap.num_pcrs))) {
		tpm_put_ops(chip);
		return 0;
	}

	num_pcrs = be32_to_cpu(cap.num_pcrs);
	for (i = 0; i < num_pcrs; i++) {
		if (tpm1_pcr_read(chip, i, digest)) {
			str = buf;
			break;
		}
		str += sprintf(str, "PCR-%02d: ", i);
		for (j = 0; j < TPM_DIGEST_SIZE; j++)
			str += sprintf(str, "%02X ", digest[j]);
		str += sprintf(str, "\n");
	}
	tpm_put_ops(chip);
	return str - buf;
}
static DEVICE_ATTR_RO(pcrs);

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);
	ssize_t rc = 0;
	cap_t cap;

	if (tpm_try_get_ops(chip))
		return 0;

	if (tpm1_getcap(chip, TPM_CAP_FLAG_PERM, &cap,
			"attempting to determine the permanent enabled state",
			sizeof(cap.perm_flags)))
		goto out_ops;

	rc = sprintf(buf, "%d\n", !cap.perm_flags.disable);
out_ops:
	tpm_put_ops(chip);
	return rc;
}
static DEVICE_ATTR_RO(enabled);

static ssize_t active_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);
	ssize_t rc = 0;
	cap_t cap;

	if (tpm_try_get_ops(chip))
		return 0;

	if (tpm1_getcap(chip, TPM_CAP_FLAG_PERM, &cap,
			"attempting to determine the permanent active state",
			sizeof(cap.perm_flags)))
		goto out_ops;

	rc = sprintf(buf, "%d\n", !cap.perm_flags.deactivated);
out_ops:
	tpm_put_ops(chip);
	return rc;
}
static DEVICE_ATTR_RO(active);

static ssize_t owned_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);
	ssize_t rc = 0;
	cap_t cap;

	if (tpm_try_get_ops(chip))
		return 0;

	if (tpm1_getcap(to_tpm_chip(dev), TPM_CAP_PROP_OWNER, &cap,
			"attempting to determine the owner state",
			sizeof(cap.owned)))
		goto out_ops;

	rc = sprintf(buf, "%d\n", cap.owned);
out_ops:
	tpm_put_ops(chip);
	return rc;
}
static DEVICE_ATTR_RO(owned);

static ssize_t temp_deactivated_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);
	ssize_t rc = 0;
	cap_t cap;

	if (tpm_try_get_ops(chip))
		return 0;

	if (tpm1_getcap(to_tpm_chip(dev), TPM_CAP_FLAG_VOL, &cap,
			"attempting to determine the temporary state",
			sizeof(cap.stclear_flags)))
		goto out_ops;

	rc = sprintf(buf, "%d\n", cap.stclear_flags.deactivated);
out_ops:
	tpm_put_ops(chip);
	return rc;
}
static DEVICE_ATTR_RO(temp_deactivated);

static ssize_t caps_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct tpm_chip *chip = to_tpm_chip(dev);
	ssize_t rc = 0;
	char *str = buf;
	cap_t cap;

	if (tpm_try_get_ops(chip))
		return 0;

	if (tpm1_getcap(chip, TPM_CAP_PROP_MANUFACTURER, &cap,
			"attempting to determine the manufacturer",
			sizeof(cap.manufacturer_id)))
		goto out_ops;

	str += sprintf(str, "Manufacturer: 0x%x\n",
		       be32_to_cpu(cap.manufacturer_id));

	/* Try to get a TPM version 1.2 TPM_CAP_VERSION_INFO */
	rc = tpm1_getcap(chip, TPM_CAP_VERSION_1_2, &cap,
			 "attempting to determine the 1.2 version",
			 sizeof(cap.tpm_version_1_2));
	if (!rc) {
		str += sprintf(str,
			       "TCG version: %d.%d\nFirmware version: %d.%d\n",
			       cap.tpm_version_1_2.Major,
			       cap.tpm_version_1_2.Minor,
			       cap.tpm_version_1_2.revMajor,
			       cap.tpm_version_1_2.revMinor);
	} else {
		/* Otherwise just use TPM_STRUCT_VER */
		if (tpm1_getcap(chip, TPM_CAP_VERSION_1_1, &cap,
				"attempting to determine the 1.1 version",
				sizeof(cap.tpm_version)))
			goto out_ops;
		str += sprintf(str,
			       "TCG version: %d.%d\nFirmware version: %d.%d\n",
			       cap.tpm_version.Major,
			       cap.tpm_version.Minor,
			       cap.tpm_version.revMajor,
			       cap.tpm_version.revMinor);
	}
	rc = str - buf;
out_ops:
	tpm_put_ops(chip);
	return rc;
}
static DEVICE_ATTR_RO(caps);

static ssize_t cancel_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct tpm_chip *chip = to_tpm_chip(dev);

	if (tpm_try_get_ops(chip))
		return 0;

	chip->ops->cancel(chip);
	tpm_put_ops(chip);
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
	struct tpm_chip *chip = to_tpm_chip(dev);
	u32 flags;
	ssize_t rc;

	rc = tpm_try_get_ops(chip);
	if (rc)
		return rc;

	rc = tpm2_get_tpm_pt(chip, pa->property_id, &flags, "reading property");
	if (rc) {
		rc = 0;
		goto error;
	}

	rc = sprintf(buf, "%d\n", !!(flags & pa->flag_mask));
error:
	tpm_put_ops(chip);
	return rc;
}

static ssize_t tpm2_prop_u32_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct tpm2_prop_u32_dev_attribute *pa =
		container_of(attr, struct tpm2_prop_u32_dev_attribute, attr);
	struct tpm_chip *chip = to_tpm_chip(dev);
	u32 value;
	ssize_t rc;

	rc = tpm_try_get_ops(chip);
	if (rc)
		return rc;

	rc = tpm2_get_tpm_pt(chip, pa->property_id, &value, "reading property");
	if (rc) {
		rc = 0;
		goto error;
	}

	rc = sprintf(buf, "%u\n", value);
error:
	tpm_put_ops(chip);
	return rc;
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
	/* FIXME: update tpm_sysfs to explicitly lock chip->ops for TPM 2.0
	 */
	WARN_ON(chip->groups_cnt != 0);
	chip->groups[chip->groups_cnt++] =
		(chip->flags & TPM_CHIP_FLAG_TPM2) ?
		&tpm2_dev_group : &tpm1_dev_group;
}
