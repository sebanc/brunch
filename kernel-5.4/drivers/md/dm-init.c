// SPDX-License-Identifier: GPL-2.0

/*
 * dm-init.c
 * Copyright (C) 2017 The Chromium OS Authors <chromium-os-dev@chromium.org>
 *
 * This file is released under the GPLv2.
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/moduleparam.h>

#define DM_MSG_PREFIX "init"
#define DM_MAX_DEVICES 256
#define DM_MAX_TARGETS 256
#define DM_MAX_STR_SIZE 4096

static char *create;

/*
 * Format: dm-mod.create=<name>,<uuid>,<minor>,<flags>,<table>[,<table>+][;<name>,<uuid>,<minor>,<flags>,<table>[,<table>+]+]
 * Table format: <start_sector> <num_sectors> <target_type> <target_args>
 *
 * See Documentation/admin-guide/device-mapper/dm-init.rst for dm-mod.create="..." format
 * details.
 */

struct dm_device {
	struct dm_ioctl dmi;
	struct dm_target_spec *table[DM_MAX_TARGETS];
	char *target_args_array[DM_MAX_TARGETS];
	struct list_head list;
};

const char * const dm_allowed_targets[] __initconst = {
	"crypt",
	"delay",
	"linear",
	"snapshot-origin",
	"striped",
	"verity",
};

static int __init dm_verify_target_type(const char *target)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(dm_allowed_targets); i++) {
		if (!strcmp(dm_allowed_targets[i], target))
			return 0;
	}
	return -EINVAL;
}

static void __init dm_setup_cleanup(struct list_head *devices)
{
	struct dm_device *dev, *tmp;
	unsigned int i;

	list_for_each_entry_safe(dev, tmp, devices, list) {
		list_del(&dev->list);
		for (i = 0; i < dev->dmi.target_count; i++) {
			kfree(dev->table[i]);
			kfree(dev->target_args_array[i]);
		}
		kfree(dev);
	}
}

/**
 * str_field_delimit - delimit a string based on a separator char.
 * @str: the pointer to the string to delimit.
 * @separator: char that delimits the field
 *
 * Find a @separator and replace it by '\0'.
 * Remove leading and trailing spaces.
 * Return the remainder string after the @separator.
 */
static char __init *str_field_delimit(char **str, char separator)
{
	char *s;

	/* TODO: add support for escaped characters */
	*str = skip_spaces(*str);
	s = strchr(*str, separator);
	/* Delimit the field and remove trailing spaces */
	if (s)
		*s = '\0';
	*str = strim(*str);
	return s ? ++s : NULL;
}

/**
 * dm_parse_table_entry - parse a table entry
 * @dev: device to store the parsed information.
 * @str: the pointer to a string with the format:
 *	<start_sector> <num_sectors> <target_type> <target_args>[, ...]
 *
 * Return the remainder string after the table entry, i.e, after the comma which
 * delimits the entry or NULL if reached the end of the string.
 */
static char __init *dm_parse_table_entry(struct dm_device *dev, char *str)
{
	const unsigned int n = dev->dmi.target_count - 1;
	struct dm_target_spec *sp;
	unsigned int i;
	/* fields:  */
	char *field[4];
	char *next;

	field[0] = str;
	/* Delimit first 3 fields that are separated by space */
	for (i = 0; i < ARRAY_SIZE(field) - 1; i++) {
		field[i + 1] = str_field_delimit(&field[i], ' ');
		if (!field[i + 1])
			return ERR_PTR(-EINVAL);
	}
	/* Delimit last field that can be terminated by comma */
	next = str_field_delimit(&field[i], ',');

	sp = kzalloc(sizeof(*sp), GFP_KERNEL);
	if (!sp)
		return ERR_PTR(-ENOMEM);
	dev->table[n] = sp;

	/* start_sector */
	if (kstrtoull(field[0], 0, &sp->sector_start))
		return ERR_PTR(-EINVAL);
	/* num_sector */
	if (kstrtoull(field[1], 0, &sp->length))
		return ERR_PTR(-EINVAL);
	/* target_type */
	strscpy(sp->target_type, field[2], sizeof(sp->target_type));
	if (dm_verify_target_type(sp->target_type)) {
		DMERR("invalid type \"%s\"", sp->target_type);
		return ERR_PTR(-EINVAL);
	}
	/* target_args */
	dev->target_args_array[n] = kstrndup(field[3], DM_MAX_STR_SIZE,
					     GFP_KERNEL);
	if (!dev->target_args_array[n])
		return ERR_PTR(-ENOMEM);

	return next;
}

/**
 * dm_parse_table - parse "dm-mod.create=" table field
 * @dev: device to store the parsed information.
 * @str: the pointer to a string with the format:
 *	<table>[,<table>+]
 */
static int __init dm_parse_table(struct dm_device *dev, char *str)
{
	char *table_entry = str;

	while (table_entry) {
		DMDEBUG("parsing table \"%s\"", str);
		if (++dev->dmi.target_count > DM_MAX_TARGETS) {
			DMERR("too many targets %u > %d",
			      dev->dmi.target_count, DM_MAX_TARGETS);
			return -EINVAL;
		}
		table_entry = dm_parse_table_entry(dev, table_entry);
		if (IS_ERR(table_entry)) {
			DMERR("couldn't parse table");
			return PTR_ERR(table_entry);
		}
	}

	return 0;
}

/**
 * dm_parse_device_entry - parse a device entry
 * @dev: device to store the parsed information.
 * @str: the pointer to a string with the format:
 *	name,uuid,minor,flags,table[; ...]
 *
 * Return the remainder string after the table entry, i.e, after the semi-colon
 * which delimits the entry or NULL if reached the end of the string.
 */
static char __init *dm_parse_device_entry(struct dm_device *dev, char *str)
{
	/* There are 5 fields: name,uuid,minor,flags,table; */
	char *field[5];
	unsigned int i;
	char *next;

	field[0] = str;
	/* Delimit first 4 fields that are separated by comma */
	for (i = 0; i < ARRAY_SIZE(field) - 1; i++) {
		field[i+1] = str_field_delimit(&field[i], ',');
		if (!field[i+1])
			return ERR_PTR(-EINVAL);
	}
	/* Delimit last field that can be delimited by semi-colon */
	next = str_field_delimit(&field[i], ';');

	/* name */
	strscpy(dev->dmi.name, field[0], sizeof(dev->dmi.name));
	/* uuid */
	strscpy(dev->dmi.uuid, field[1], sizeof(dev->dmi.uuid));
	/* minor */
	if (strlen(field[2])) {
		if (kstrtoull(field[2], 0, &dev->dmi.dev))
			return ERR_PTR(-EINVAL);
		dev->dmi.flags |= DM_PERSISTENT_DEV_FLAG;
	}
	/* flags */
	if (!strcmp(field[3], "ro"))
		dev->dmi.flags |= DM_READONLY_FLAG;
	else if (strcmp(field[3], "rw"))
		return ERR_PTR(-EINVAL);
	/* table */
	if (dm_parse_table(dev, field[4]))
		return ERR_PTR(-EINVAL);

	return next;
}

/**
 * dm_parse_devices - parse "dm-mod.create=" argument
 * @devices: list of struct dm_device to store the parsed information.
 * @str: the pointer to a string with the format:
 *	<device>[;<device>+]
 */
static int __init dm_parse_devices(struct list_head *devices, char *str)
{
	unsigned long ndev = 0;
	struct dm_device *dev;
	char *device = str;

	DMDEBUG("parsing \"%s\"", str);
	while (device) {
		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev)
			return -ENOMEM;
		list_add_tail(&dev->list, devices);

		if (++ndev > DM_MAX_DEVICES) {
			DMERR("too many devices %lu > %d",
			      ndev, DM_MAX_DEVICES);
			return -EINVAL;
		}

		device = dm_parse_device_entry(dev, device);
		if (IS_ERR(device)) {
			DMERR("couldn't parse device");
			return PTR_ERR(device);
		}
	}

	return 0;
}

/**
 * dm_init_init - parse "dm-mod.create=" argument and configure drivers
 */
static int __init dm_init_init(void)
{
	struct dm_device *dev;
	LIST_HEAD(devices);
	char *str;
	int r;

	if (!create)
		return 0;

	if (strlen(create) >= DM_MAX_STR_SIZE) {
		DMERR("Argument is too big. Limit is %d", DM_MAX_STR_SIZE);
		return -EINVAL;
	}
	str = kstrndup(create, DM_MAX_STR_SIZE, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	r = dm_parse_devices(&devices, str);
	if (r)
		goto out;

	DMINFO("waiting for all devices to be available before creating mapped devices");
	wait_for_device_probe();

	list_for_each_entry(dev, &devices, list) {
		if (dm_early_create(&dev->dmi, dev->table,
				    dev->target_args_array))
			break;
	}
out:
	kfree(str);
	dm_setup_cleanup(&devices);
	return r;
}

late_initcall(dm_init_init);

module_param(create, charp, 0);
MODULE_PARM_DESC(create, "Create a mapped device in early boot");

/* ---------------------------------------------------------------
 * ChromeOS shim - convert dm= format to dm-mod.create= format
 * ---------------------------------------------------------------
 */

struct dm_chrome_target {
	char *field[4];
};

struct dm_chrome_dev {
	char *name, *uuid, *mode;
	unsigned int num_targets;
	struct dm_chrome_target targets[DM_MAX_TARGETS];
};

static char __init *dm_chrome_parse_target(char *str, struct dm_chrome_target *tgt)
{
	unsigned int i;

	tgt->field[0] = str;
	/* Delimit first 3 fields that are separated by space */
	for (i = 0; i < ARRAY_SIZE(tgt->field) - 1; i++) {
		tgt->field[i + 1] = str_field_delimit(&tgt->field[i], ' ');
		if (!tgt->field[i + 1])
			return NULL;
	}
	/* Delimit last field that can be terminated by comma */
	return str_field_delimit(&tgt->field[i], ',');
}

static char __init *dm_chrome_parse_dev(char *str, struct dm_chrome_dev *dev)
{
	char *target, *num;
	unsigned int i;

	if (!str)
		return ERR_PTR(-EINVAL);

	target = str_field_delimit(&str, ',');
	if (!target)
		return ERR_PTR(-EINVAL);

	/* Delimit first 3 fields that are separated by space */
	dev->name = str;
	dev->uuid = str_field_delimit(&dev->name, ' ');
	if (!dev->uuid)
		return ERR_PTR(-EINVAL);

	dev->mode = str_field_delimit(&dev->uuid, ' ');
	if (!dev->mode)
		return ERR_PTR(-EINVAL);

	/* num is optional */
	num = str_field_delimit(&dev->mode, ' ');
	if (!num)
		dev->num_targets = 1;
	else {
		/* Delimit num and check if it the last field */
		if(str_field_delimit(&num, ' '))
			return ERR_PTR(-EINVAL);
		if (kstrtouint(num, 0, &dev->num_targets))
			return ERR_PTR(-EINVAL);
	}

	if (dev->num_targets > DM_MAX_TARGETS) {
		DMERR("too many targets %u > %d",
		      dev->num_targets, DM_MAX_TARGETS);
		return ERR_PTR(-EINVAL);
	}

	for (i = 0; i < dev->num_targets - 1; i++) {
		target = dm_chrome_parse_target(target, &dev->targets[i]);
		if (!target)
			return ERR_PTR(-EINVAL);
	}
	/* The last one can return NULL if it reaches the end of str */
	return dm_chrome_parse_target(target, &dev->targets[i]);
}

static char __init *dm_chrome_convert(struct dm_chrome_dev *devs, unsigned int num_devs)
{
	char *str = kmalloc(DM_MAX_STR_SIZE, GFP_KERNEL);
	char *p = str;
	unsigned int i, j;
	int ret;

	if (!str)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num_devs; i++) {
		if (!strcmp(devs[i].uuid, "none"))
			devs[i].uuid = "";
		ret = snprintf(p, DM_MAX_STR_SIZE - (p - str),
			       "%s,%s,,%s",
			       devs[i].name,
			       devs[i].uuid,
			       devs[i].mode);
		if (ret < 0)
			goto out;
		p += ret;

		for (j = 0; j < devs[i].num_targets; j++) {
			ret = snprintf(p, DM_MAX_STR_SIZE - (p - str),
				       ",%s %s %s %s",
				       devs[i].targets[j].field[0],
				       devs[i].targets[j].field[1],
				       devs[i].targets[j].field[2],
				       devs[i].targets[j].field[3]);
			if (ret < 0)
				goto out;
			p += ret;
		}
		if (i < num_devs - 1) {
			ret = snprintf(p, DM_MAX_STR_SIZE - (p - str), ";");
			if (ret < 0)
				goto out;
			p += ret;
		}
	}

	return str;

out:
	kfree(str);
	return ERR_PTR(ret);
}

/**
 * dm_chrome_shim - convert old dm= format used in chromeos to the new
 * upstream format.
 *
 * ChromeOS old format
 * -------------------
 * <device>        ::= [<num>] <device-mapper>+
 * <device-mapper> ::= <head> "," <target>+
 * <head>          ::= <name> <uuid> <mode> [<num>]
 * <target>        ::= <start> <length> <type> <options> ","
 * <mode>          ::= "ro" | "rw"
 * <uuid>          ::= xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx | "none"
 * <type>          ::= "verity" | "bootcache" | ...
 *
 * Example:
 * 2 vboot none ro 1,
 *     0 1768000 bootcache
 *       device=aa55b119-2a47-8c45-946a-5ac57765011f+1
 *       signature=76e9be054b15884a9fa85973e9cb274c93afadb6
 *       cache_start=1768000 max_blocks=100000 size_limit=23 max_trace=20000,
 *   vroot none ro 1,
 *     0 1740800 verity payload=254:0 hashtree=254:0 hashstart=1740800 alg=sha1
 *       root_hexdigest=76e9be054b15884a9fa85973e9cb274c93afadb6
 *       salt=5b3549d54d6c7a3837b9b81ed72e49463a64c03680c47835bef94d768e5646fe
 *
 * Notes:
 *  1. uuid is a label for the device and we set it to "none".
 *  2. The <num> field will be optional initially and assumed to be 1.
 *     Once all the scripts that set these fields have been set, it will
 *     be made mandatory.
 */

static char *chrome_create;

static int __init dm_chrome_shim(char *arg) {
	if (!arg || create)
		return -EINVAL;
	chrome_create = arg;
	return 0;
}

static int __init dm_chrome_parse_devices(void)
{
	struct dm_chrome_dev *devs;
	unsigned int num_devs, i;
	char *next, *base_str;
	int ret = 0;

	/* Verify if dm-mod.create was not used */
	if (!chrome_create || create)
		return -EINVAL;

	if (strlen(chrome_create) >= DM_MAX_STR_SIZE) {
		DMERR("Argument is too big. Limit is %d\n", DM_MAX_STR_SIZE);
		return -EINVAL;
	}

	base_str = kstrdup(chrome_create, GFP_KERNEL);
	if (!base_str)
		return -ENOMEM;

	next = str_field_delimit(&base_str, ' ');
	if (!next) {
		ret = -EINVAL;
		goto out_str;
	}

	/* if first field is not the optional <num> field */
	if (kstrtouint(base_str, 0, &num_devs)) {
		num_devs = 1;
		/* rewind next pointer */
		next = base_str;
	}

	if (num_devs > DM_MAX_DEVICES) {
		DMERR("too many devices %u > %d", num_devs, DM_MAX_DEVICES);
		ret = -EINVAL;
		goto out_str;
	}

	devs = kcalloc(num_devs, sizeof(*devs), GFP_KERNEL);
	if (!devs)
		return -ENOMEM;

	/* restore string */
	strcpy(base_str, chrome_create);

	/* parse devices */
	for (i = 0; i < num_devs; i++) {
		next = dm_chrome_parse_dev(next, &devs[i]);
		if (IS_ERR(next)) {
			DMERR("couldn't parse device");
			ret = PTR_ERR(next);
			goto out_devs;
		}
	}

	create = dm_chrome_convert(devs, num_devs);
	if (IS_ERR(create)) {
		ret = PTR_ERR(create);
		goto out_devs;
	}

	DMDEBUG("Converting:\n\tdm=\"%s\"\n\tdm-mod.create=\"%s\"\n",
		chrome_create, create);

	/* Call upstream code */
	dm_init_init();

	kfree(create);

out_devs:
	create = NULL;
	kfree(devs);
out_str:
	kfree(base_str);

	return ret;
}

late_initcall(dm_chrome_parse_devices);

__setup("dm=", dm_chrome_shim);
