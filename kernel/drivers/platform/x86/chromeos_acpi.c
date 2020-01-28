 /*
 *  chromeos_acpi.c - ChromeOS specific ACPI support
 *
 *
 * Copyright (C) 2011 The Chromium OS Authors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver attaches to the ChromeOS ACPI device and the exports the values
 * reported by the ACPI in a sysfs directory
 * (/sys/devices/platform/chromeos_acpi).
 *
 * The first version of the driver provides only static information; the
 * values reported by the driver are the snapshot reported by the ACPI at
 * driver installation time.
 *
 * All values are presented in the string form (numbers as decimal values) and
 * can be accessed as the contents of the appropriate read only files in the
 * sysfs directory tree originating in /sys/devices/platform/chromeos_acpi.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvram.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>

#include "../chrome/chromeos.h"

#define CHNV_DEBUG_RESET_FLAG	0x40	     /* flag for S3 reboot */
#define CHNV_RECOVERY_FLAG	0x80	     /* flag for recovery reboot */

#define CHSW_RECOVERY_FW	0x00000002   /* recovery button depressed */
#define CHSW_RECOVERY_EC	0x00000004   /* recovery button depressed */
#define CHSW_DEVELOPER_MODE	0x00000020   /* developer switch set */
#define CHSW_WP			0x00000200   /* write-protect (optional) */

/*
 * Structure containing one ACPI exported integer along with the validity
 * flag.
 */
struct chromeos_acpi_datum {
	unsigned cad_value;
	bool	 cad_is_set;
};

/*
 * Structure containing the set of ACPI exported integers required by chromeos
 * wrapper.
 */
struct chromeos_acpi_if {
	struct chromeos_acpi_datum	switch_state;

	/* chnv is a single byte offset in nvram. exported by older firmware */
	struct chromeos_acpi_datum	chnv;

	/* vbnv is an address range in nvram, exported by newer firmware */
	struct chromeos_acpi_datum	nv_base;
	struct chromeos_acpi_datum	nv_size;
};

#define MY_LOGPREFIX "chromeos_acpi: "
#define MY_ERR KERN_ERR MY_LOGPREFIX
#define MY_NOTICE KERN_NOTICE MY_LOGPREFIX
#define MY_INFO KERN_INFO MY_LOGPREFIX

/* ACPI method name for MLST; the response for this method is a
 * package of strings listing the methods which should be reflected in
 * sysfs. */
#define MLST_METHOD "MLST"

static const struct acpi_device_id chromeos_device_ids[] = {
	{"GGL0001", 0}, /* Google's own */
	{"", 0},
};

MODULE_DEVICE_TABLE(acpi, chromeos_device_ids);

static int chromeos_device_add(struct acpi_device *device);
static int chromeos_device_remove(struct acpi_device *device);

static struct chromeos_acpi_if chromeos_acpi_if_data;
static struct acpi_driver chromeos_acpi_driver = {
	.name = "ChromeOS Device",
	.class = "ChromeOS",
	.ids = chromeos_device_ids,
	.ops = {
		.add = chromeos_device_add,
		.remove = chromeos_device_remove,
		},
	.owner = THIS_MODULE,
};

/* The default list of methods the chromeos ACPI device is supposed to export,
 * if the MLST method is not present or is poorly formed.  The MLST method
 * itself is included, to aid in debugging. */
static char *default_methods[] = {
	"CHSW", "HWID", "BINF", "GPIO", "CHNV", "FWID", "FRID", MLST_METHOD
};

/*
 * Representation of a single sys fs attribute. In addition to the standard
 * device_attribute structure has a link field, allowing to create a list of
 * these structures (to keep track for de-allocation when removing the driver)
 * and a pointer to the actual attribute value, reported when accessing the
 * appropriate sys fs file
 */
struct acpi_attribute {
	struct device_attribute dev_attr;
	struct acpi_attribute *next_acpi_attr;
	char *value;
};

/*
 * Representation of a sys fs attribute group (a sub directory in the device's
 * sys fs directory). In addition to the standard structure has a link to
 * allow to keep track of the allocated structures.
 */
struct acpi_attribute_group {
	struct attribute_group ag;
	struct acpi_attribute_group *next_acpi_attr_group;
};

/*
 * ChromeOS ACPI device wrapper adds links pointing at lists of allocated
 * attributes and attribute groups.
 */
struct chromeos_acpi_dev {
	struct platform_device *p_dev;
	struct acpi_attribute *attributes;
	struct acpi_attribute_group *groups;
};

static struct chromeos_acpi_dev chromeos_acpi = { };

static bool chromeos_on_legacy_firmware(void)
{
	/*
	 * Presense of the CHNV ACPI element implies running on a legacy
	 * firmware
	 */
	return chromeos_acpi_if_data.chnv.cad_is_set;
}

/*
 * This function operates on legacy BIOSes which do not export VBNV element
 * through ACPI. These BIOSes use a fixed location in NVRAM to contain a
 * bitmask of known flags.
 *
 * @flag - the bitmask to set, it is the responsibility of the caller to set
 *         the proper bits.
 *
 * returns 0 on success (is running in legacy mode and chnv is initialized) or
 *         -1 otherwise.
 */
static int chromeos_set_nvram_flag(u8 flag)
{
	u8 cur;
	unsigned index = chromeos_acpi_if_data.chnv.cad_value;

	if (!chromeos_on_legacy_firmware())
		return -ENODEV;

	cur = nvram_read_byte(index);

	if ((cur & flag) != flag)
		nvram_write_byte(cur | flag, index);
	return 0;
}

int chromeos_legacy_set_need_recovery(void)
{
	return chromeos_set_nvram_flag(CHNV_RECOVERY_FLAG);
}

/*
 * Read the nvram buffer contents into the user provided space.
 *
 * retrun number of bytes copied, or -1 on any error.
 */
static ssize_t chromeos_vbc_nvram_read(void *buf, size_t count)
{

	int base, size, i;

	if (!chromeos_acpi_if_data.nv_base.cad_is_set ||
	    !chromeos_acpi_if_data.nv_size.cad_is_set) {
		printk(MY_ERR "%s: NVRAM not configured!\n", __func__);
		return -ENODEV;
	}

	base = chromeos_acpi_if_data.nv_base.cad_value;
	size = chromeos_acpi_if_data.nv_size.cad_value;

	if (count < size) {
		pr_err("%s: not enough room to read nvram (%zd < %d)\n",
		       __func__, count, size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++)
		((u8 *)buf)[i] = nvram_read_byte(base++);

	return size;
}

static ssize_t chromeos_vbc_nvram_write(const void *buf, size_t count)
{
	unsigned base, size, i;

	if (!chromeos_acpi_if_data.nv_base.cad_is_set ||
	    !chromeos_acpi_if_data.nv_size.cad_is_set) {
		printk(MY_ERR "%s: NVRAM not configured!\n", __func__);
		return -ENODEV;
	}

	size = chromeos_acpi_if_data.nv_size.cad_value;
	base = chromeos_acpi_if_data.nv_base.cad_value;

	if (count != size) {
		printk(MY_ERR "%s: wrong buffer size (%zd != %d)!\n", __func__,
		       count, size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		u8 c;

		c = nvram_read_byte(base + i);
		if (c == ((u8 *)buf)[i])
			continue;
		nvram_write_byte(((u8 *)buf)[i], base + i);
	}
	return size;
}

/*
 * To show attribute value just access the container structure's `value'
 * field.
 */
static ssize_t show_acpi_attribute(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct acpi_attribute *paa;

	paa = container_of(attr, struct acpi_attribute, dev_attr);
	return snprintf(buf, PAGE_SIZE, paa->value);
}

/*
 * create_sysfs_attribute() create and initialize an ACPI sys fs attribute
 *			    structure.
 * @value: attribute value
 * @name: base attribute name
 * @count: total number of instances of this attribute
 * @instance: instance number of this particular attribute
 *
 * This function allocates and initializes the structure containing all
 * information necessary to add a sys fs attribute. In case the attribute has
 * just a single instance, the attribute file name is equal to the @name
 * parameter . In case the attribute has several instances, the attribute
 * file name is @name.@instance.
 *
 * Returns: a pointer to the allocated and initialized structure, or null if
 * allocation failed.
 *
 * As a side effect, the allocated structure is added to the list in the
 * chromeos_acpi structure. Note that the actual attribute creation is not
 * attempted yet, in case of creation error the structure would not have an
 * actual attribute associated with it, so when de-installing the driver this
 * structure would be used to try to remove an attribute which does not exist.
 * This is considered acceptable, as there is no reason for sys fs attribute
 * creation failure.
 */
static struct acpi_attribute *create_sysfs_attribute(char *value, char *name,
						     int count, int instance)
{
	struct acpi_attribute *paa;
	int total_size, room_left;
	int value_len = strlen(value);

	if (!value_len)
		return NULL;

	value_len++; /* include the terminating zero */

	/*
	 * total allocation size includes (all strings with including
	 * terminating zeros):
	 *
	 * - value string
	 * - attribute structure size
	 * - name string
	 * - suffix string (in case there are multiple instances)
	 * - dot separating the instance suffix
	 */

	total_size = value_len + sizeof(struct acpi_attribute) +
			strlen(name) + 1;

	if (count != 1) {
		if (count >= 1000) {
			printk(MY_ERR "%s: too many (%d) instances of %s\n",
			       __func__, count, name);
			return NULL;
		}
		/* allow up to three digits and the dot */
		total_size += 4;
	}

	paa = kzalloc(total_size, GFP_KERNEL);
	if (!paa) {
		printk(MY_ERR "out of memory in %s!\n", __func__);
		return NULL;
	}

	sysfs_attr_init(&paa->dev_attr.attr);
	paa->dev_attr.attr.mode = 0444;  /* read only */
	paa->dev_attr.show = show_acpi_attribute;
	paa->value = (char *)(paa + 1);
	strcpy(paa->value, value);
	paa->dev_attr.attr.name = paa->value + value_len;

	room_left = total_size - value_len -
			offsetof(struct acpi_attribute, value);

	if (count == 1) {
		snprintf((char *)paa->dev_attr.attr.name, room_left, name);
	} else {
		snprintf((char *)paa->dev_attr.attr.name, room_left,
			 "%s.%d", name, instance);
	}

	paa->next_acpi_attr = chromeos_acpi.attributes;
	chromeos_acpi.attributes = paa;

	return paa;
}

/*
 * add_sysfs_attribute() create and initialize an ACPI sys fs attribute
 *			    structure and create the attribute.
 * @value: attribute value
 * @name: base attribute name
 * @count: total number of instances of this attribute
 * @instance: instance number of this particular attribute
 */

static void add_sysfs_attribute(char *value, char *name,
				int count, int instance)
{
	struct acpi_attribute *paa =
	    create_sysfs_attribute(value, name, count, instance);

	if (!paa)
		return;

	if (device_create_file(&chromeos_acpi.p_dev->dev, &paa->dev_attr))
		printk(MY_ERR "failed to create attribute for %s\n", name);
}

/*
 * handle_nested_acpi_package() create sysfs group including attributes
 *				representing a nested ACPI package.
 *
 * @po: package contents as returned by ACPI
 * @pm: name of the group
 * @total: number of instances of this package
 * @instance: instance number of this particular group
 *
 * The created group is called @pm in case there is a single instance, or
 * @pm.@instance otherwise.
 *
 * All group and attribute storage allocations are included in the lists for
 * tracking of allocated memory.
 */
static void handle_nested_acpi_package(union acpi_object *po, char *pm,
				       int total, int instance)
{
	int i, size, count, j;
	struct acpi_attribute_group *aag;

	count = po->package.count;

	size = strlen(pm) + 1 + sizeof(struct acpi_attribute_group) +
	    sizeof(struct attribute *) * (count + 1);

	if (total != 1) {
		if (total >= 1000) {
			printk(MY_ERR "%s: too many (%d) instances of %s\n",
			       __func__, total, pm);
			return;
		}
		/* allow up to three digits and the dot */
		size += 4;
	}

	aag = kzalloc(size, GFP_KERNEL);
	if (!aag) {
		printk(MY_ERR "out of memory in %s!\n", __func__);
		return;
	}

	aag->next_acpi_attr_group = chromeos_acpi.groups;
	chromeos_acpi.groups = aag->next_acpi_attr_group;
	aag->ag.attrs = (struct attribute **)(aag + 1);
	aag->ag.name = (const char *)&aag->ag.attrs[count + 1];

	/* room left in the buffer */
	size = size - (aag->ag.name - (char *)aag);

	if (total != 1)
		snprintf((char *)aag->ag.name, size, "%s.%d", pm, instance);
	else
		snprintf((char *)aag->ag.name, size, "%s", pm);

	j = 0;			/* attribute index */
	for (i = 0; i < count; i++) {
		union acpi_object *element = po->package.elements + i;
		int copy_size = 0;
		char attr_value[40];	/* 40 chars be enough for names */
		struct acpi_attribute *paa;

		switch (element->type) {
		case ACPI_TYPE_INTEGER:
			copy_size = snprintf(attr_value, sizeof(attr_value),
					     "%d", (int)element->integer.value);
			paa = create_sysfs_attribute(attr_value, pm, count, i);
			break;

		case ACPI_TYPE_STRING:
			copy_size = min(element->string.length,
					(u32)(sizeof(attr_value)) - 1);
			memcpy(attr_value, element->string.pointer, copy_size);
			attr_value[copy_size] = '\0';
			paa = create_sysfs_attribute(attr_value, pm, count, i);
			break;

		default:
			printk(MY_ERR "ignoring nested type %d\n",
			       element->type);
			continue;
		}
		aag->ag.attrs[j++] = &paa->dev_attr.attr;
	}

	if (sysfs_create_group(&chromeos_acpi.p_dev->dev.kobj, &aag->ag))
		printk(MY_ERR "failed to create group %s.%d\n", pm, instance);
}

/*
 * maybe_export_acpi_int() export a single int value when required
 *
 * @pm: name of the package
 * @index: index of the element of the package
 * @value: value of the element
 */
static void maybe_export_acpi_int(const char *pm, int index, unsigned value)
{
	int i;
	struct chromeos_acpi_exported_ints {
		const char *acpi_name;
		int acpi_index;
		struct chromeos_acpi_datum *cad;
	} exported_ints[] = {
		{ "VBNV", 0, &chromeos_acpi_if_data.nv_base },
		{ "VBNV", 1, &chromeos_acpi_if_data.nv_size },
		{ "CHSW", 0, &chromeos_acpi_if_data.switch_state },
		{ "CHNV", 0, &chromeos_acpi_if_data.chnv }
	};

	for (i = 0; i < ARRAY_SIZE(exported_ints); i++) {
		struct chromeos_acpi_exported_ints *exported_int;

		exported_int = exported_ints + i;

		if (!strncmp(pm, exported_int->acpi_name, 4) &&
		    (exported_int->acpi_index == index)) {
			printk(MY_NOTICE "registering %s %d\n", pm, index);
			exported_int->cad->cad_value = value;
			exported_int->cad->cad_is_set = true;
			return;
		}
	}
}

/*
 * acpi_buffer_to_string() convert contents of an ACPI buffer element into a
 *		hex string truncating it if necessary to fit into one page.
 *
 * @element: an acpi element known to contain an ACPI buffer.
 *
 * Returns: pointer to an ASCII string containing the buffer representation
 *	    (whatever fit into PAGE_SIZE). The caller is responsible for
 *	    freeing the memory.
 */
static char *acpi_buffer_to_string(union acpi_object *element)
{
	char *base, *p;
	int i;
	unsigned room_left;
	/* Include this many characters per line */
	unsigned char_per_line = 16;
	unsigned blob_size;
	unsigned string_buffer_size;

	/*
	 * As of now the VDAT structure can supply as much as 3700 bytes. When
	 * expressed as a hex dump it becomes 3700 * 3 + 3700/16 + .. which
	 * clearly exceeds the maximum allowed sys fs buffer size of one page
	 * (4k).
	 *
	 * What this means is that we can't keep the entire blob in one sysfs
	 * file. Currently verified boot (the consumer of the VDAT contents)
	 * does not care about the most of the data, so as a quick fix we will
	 * truncate it here. Once the blob data beyond the 4K boundary is
	 * required this approach will have to be reworked.
	 *
	 * TODO(vbendeb): Split the data into multiple VDAT instances, each
	 * not exceeding 4K or consider exporting as a binary using
	 * sysfs_create_bin_file().
	 */

	/*
	 * X, the maximum number of bytes which will fit into a sysfs file
	 * (one memory page) can be derived from the following equation (where
	 * N is number of bytes included in every hex string):
	 *
	 * 3X + X/N + 4 <= PAGE_SIZE.
	 *
	 * Solving this for X gives the following
	 */
	blob_size = ((PAGE_SIZE - 4) * char_per_line) / (char_per_line * 3 + 1);

	if (element->buffer.length > blob_size)
		printk(MY_INFO "truncating buffer from %d to %d\n",
		       element->buffer.length, blob_size);
	else
		blob_size = element->buffer.length;

	string_buffer_size =
		/* three characters to display one byte */
		blob_size * 3 +
		/* one newline per line, all rounded up, plus
		 * extra newline in the end, plus terminating
		 * zero, hence + 4
		 */
		blob_size/char_per_line + 4;

	p = kzalloc(string_buffer_size, GFP_KERNEL);
	if (!p) {
		printk(MY_ERR "out of memory in %s!\n", __func__);
		return NULL;
	}

	base = p;
	room_left = string_buffer_size;
	for (i = 0; i < blob_size; i++) {
		int printed;
		printed = snprintf(p, room_left, " %2.2x",
				   element->buffer.pointer[i]);
		room_left -= printed;
		p += printed;
		if (((i + 1) % char_per_line) == 0) {
			if (!room_left)
				break;
			room_left--;
			*p++ = '\n';
		}
	}
	if (room_left < 2) {
		printk(MY_ERR "%s: no room in the buffer!\n", __func__);
		*p = '\0';
	} else {
		*p++ = '\n';
		*p++ = '\0';
	}
	return base;
}

/*
 * handle_acpi_package() create sysfs group including attributes
 *			 representing an ACPI package.
 *
 * @po: package contents as returned by ACPI
 * @pm: name of the group
 *
 * Scalar objects included in the package get sys fs attributes created for
 * them. Nested packages are passed to a function creating a sys fs group per
 * package.
 */
static void handle_acpi_package(union acpi_object *po, char *pm)
{
	int j;
	int count = po->package.count;
	for (j = 0; j < count; j++) {
		union acpi_object *element = po->package.elements + j;
		int copy_size = 0;
		char attr_value[256];	/* strings could be this long */

		switch (element->type) {
		case ACPI_TYPE_INTEGER:
			copy_size = snprintf(attr_value, sizeof(attr_value),
					     "%d", (int)element->integer.value);
			add_sysfs_attribute(attr_value, pm, count, j);
			maybe_export_acpi_int(pm, j, (unsigned)
					      element->integer.value);
			break;

		case ACPI_TYPE_STRING:
			copy_size = min(element->string.length,
					(u32)(sizeof(attr_value)) - 1);
			memcpy(attr_value, element->string.pointer, copy_size);
			attr_value[copy_size] = '\0';
			add_sysfs_attribute(attr_value, pm, count, j);
			break;

		case ACPI_TYPE_BUFFER: {
			char *buf_str;
			buf_str = acpi_buffer_to_string(element);
			if (buf_str) {
				add_sysfs_attribute(buf_str, pm, count, j);
				kfree(buf_str);
			}
			break;
		}
		case ACPI_TYPE_PACKAGE:
			handle_nested_acpi_package(element, pm, count, j);
			break;

		default:
			printk(MY_ERR "ignoring type %d (%s)\n",
			       element->type, pm);
			break;
		}
	}
}


/*
 * add_acpi_method() evaluate an ACPI method and create sysfs attributes.
 *
 * @device: ACPI device
 * @pm: name of the method to evaluate
 */
static void add_acpi_method(struct acpi_device *device, char *pm)
{
	acpi_status status;
	struct acpi_buffer output;
	union acpi_object *po;

	output.length = ACPI_ALLOCATE_BUFFER;
	output.pointer = NULL;

	status = acpi_evaluate_object(device->handle, pm, NULL, &output);

	if (!ACPI_SUCCESS(status)) {
		printk(MY_ERR "failed to retrieve %s (%d)\n", pm, status);
		return;
	}

	po = output.pointer;

	if (po->type != ACPI_TYPE_PACKAGE)
		printk(MY_ERR "%s is not a package, ignored\n", pm);
	else
		handle_acpi_package(po, pm);
	kfree(output.pointer);
}

/*
 * chromeos_process_mlst() Evaluate the MLST method and add methods listed
 *                         in the response.
 *
 * @device: ACPI device
 *
 * Returns: 0 if successful, non-zero if error.
 */
static int chromeos_process_mlst(struct acpi_device *device)
{
	acpi_status status;
	struct acpi_buffer output;
	union acpi_object *po;
	int j;

	output.length = ACPI_ALLOCATE_BUFFER;
	output.pointer = NULL;

	status = acpi_evaluate_object(device->handle, MLST_METHOD, NULL,
				      &output);
	if (!ACPI_SUCCESS(status)) {
		pr_debug(MY_LOGPREFIX "failed to retrieve MLST (%d)\n",
			 status);
		return 1;
	}

	po = output.pointer;
	if (po->type != ACPI_TYPE_PACKAGE) {
		printk(MY_ERR MLST_METHOD "is not a package, ignored\n");
		kfree(output.pointer);
		return -EINVAL;
	}

	for (j = 0; j < po->package.count; j++) {
		union acpi_object *element = po->package.elements + j;
		int copy_size = 0;
		char method[ACPI_NAME_SIZE + 1];

		if (element->type == ACPI_TYPE_STRING) {
			copy_size = min(element->string.length,
					(u32)ACPI_NAME_SIZE);
			memcpy(method, element->string.pointer, copy_size);
			method[copy_size] = '\0';
			add_acpi_method(device, method);
		} else {
			pr_debug(MY_LOGPREFIX "ignoring type %d\n",
				 element->type);
		}
	}

	kfree(output.pointer);
	return 0;
}

static int chromeos_device_add(struct acpi_device *device)
{
	int i;

	/* Attempt to add methods by querying the device's MLST method
	 * for the list of methods. */
	if (!chromeos_process_mlst(device))
		return 0;

	printk(MY_INFO "falling back to default list of methods\n");
	for (i = 0; i < ARRAY_SIZE(default_methods); i++)
		add_acpi_method(device, default_methods[i]);
	return 0;
}

static int chromeos_device_remove(struct acpi_device *device)
{
	return 0;
}

static struct chromeos_vbc chromeos_vbc_nvram = {
	.name = "chromeos_vbc_nvram",
	.read = chromeos_vbc_nvram_read,
	.write = chromeos_vbc_nvram_write,
};

static int __init chromeos_acpi_init(void)
{
	int ret = 0;

	if (acpi_disabled)
		return -ENODEV;

	ret = chromeos_vbc_register(&chromeos_vbc_nvram);
	if (ret)
		return ret;

	chromeos_acpi.p_dev = platform_device_register_simple("chromeos_acpi",
							      -1, NULL, 0);
	if (IS_ERR(chromeos_acpi.p_dev)) {
		printk(MY_ERR "unable to register platform device\n");
		return PTR_ERR(chromeos_acpi.p_dev);
	}

	ret = acpi_bus_register_driver(&chromeos_acpi_driver);
	if (ret < 0) {
		printk(MY_ERR "failed to register driver (%d)\n", ret);
		platform_device_unregister(chromeos_acpi.p_dev);
		chromeos_acpi.p_dev = NULL;
		return ret;
	}
	printk(MY_INFO "installed%s\n",
	       chromeos_on_legacy_firmware() ? " (legacy mode)" : "");

	return 0;
}

subsys_initcall(chromeos_acpi_init);
