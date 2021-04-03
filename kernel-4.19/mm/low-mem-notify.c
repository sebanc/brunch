/*
 * mm/low-mem-notify.c
 *
 * Sends low-memory notifications to processes via /dev/low-mem.
 *
 * Copyright (C) 2012 The Chromium OS Authors
 * This program is free software, released under the GPL.
 * Based on a proposal by Minchan Kim
 *
 * A process that polls /dev/low-mem is notified of a low-memory situation.
 * The intent is to allow the process to free some memory before the OOM killer
 * is invoked.
 *
 * A low-memory condition is estimated by subtracting anonymous memory
 * (i.e. process data segments), kernel memory, and a fixed amount of
 * file-backed memory from total memory.  This is just a heuristic, as in
 * general we don't know how much memory can be reclaimed before we try to
 * reclaim it, and that's too expensive or too late.
 *
 * This is tailored to Chromium OS, where a single program (the browser)
 * controls most of the memory, and (currently) no swap space is used.
 */


#include <linux/low-mem-notify.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/kstaled.h>

#define MB (1 << 20)

static DECLARE_WAIT_QUEUE_HEAD(low_mem_wait);
static atomic_t low_mem_state = ATOMIC_INIT(0);
atomic_t in_low_mem_check = ATOMIC_INIT(0);

/* This is a list of thresholds in pages and should be in ascending order. */
unsigned long low_mem_thresholds[LOW_MEM_THRESHOLD_MAX] = {
	50 * MB / PAGE_SIZE };
unsigned int low_mem_threshold_count = 1;

/* last observed threshold */
unsigned int low_mem_threshold_last = UINT_MAX;
bool low_mem_margin_enabled = true;
unsigned int low_mem_ram_vs_swap_weight = 4;

/* Limit logging low memory to once per second. */
DEFINE_RATELIMIT_STATE(low_mem_logging_ratelimit, 1 * HZ, 1);

unsigned long low_mem_lowest_seen_anon_mem;
const unsigned long low_mem_anon_mem_delta = 10 * MB / PAGE_SIZE;
static struct kernfs_node *low_mem_available_dirent;

struct low_mem_notify_file_info {
	unsigned long unused;
};

void low_mem_notify(void)
{
	atomic_set(&low_mem_state, true);
	wake_up(&low_mem_wait);
}

static int low_mem_notify_open(struct inode *inode, struct file *file)
{
	struct low_mem_notify_file_info *info;
	int err = 0;

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto out;
	}

	file->private_data = info;
	kstaled_disable_throttle();
out:
	return err;
}

static int low_mem_notify_release(struct inode *inode, struct file *file)
{
	kstaled_enable_throttle();
	kfree(file->private_data);
	return 0;
}

static __poll_t low_mem_notify_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* Update state to reflect any recent freeing. */
	atomic_set(&low_mem_state, low_mem_check());

	poll_wait(file, &low_mem_wait, wait);

	if (low_mem_margin_enabled && atomic_read(&low_mem_state) != 0)
		ret = POLLIN;

	return ret;
}

const struct file_operations low_mem_notify_fops = {
	.open = low_mem_notify_open,
	.release = low_mem_notify_release,
	.poll = low_mem_notify_poll,
};
EXPORT_SYMBOL(low_mem_notify_fops);

#ifdef CONFIG_SYSFS

#define LOW_MEM_ATTR(_name)				      \
	static struct kobj_attribute low_mem_##_name##_attr = \
		__ATTR(_name, 0644, low_mem_##_name##_show,   \
		       low_mem_##_name##_store)

static ssize_t low_mem_margin_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	if (low_mem_margin_enabled && low_mem_threshold_count) {
		int i;
		ssize_t written = 0;

		for (i = 0; i < low_mem_threshold_count; i++)
			written += sprintf(buf + written, "%lu ",
			    low_mem_thresholds[i] * PAGE_SIZE / MB);
		written += sprintf(buf + written, "\n");
		return written;
	} else
		return sprintf(buf, "off\n");
}

static ssize_t low_mem_margin_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	int i = 0, consumed = 0;
	const char *start = buf;
	char *endp;
	unsigned long thresholds[LOW_MEM_THRESHOLD_MAX];

	memset(thresholds, 0, sizeof(thresholds));
	/*
	 * Even though the API does not say anything about this, the string in
	 * buf is zero-terminated (as long as count < PAGE_SIZE) because buf is
	 * a newly allocated zero-filled page.  Most other sysfs handlers rely
	 * on this too.
	 */
	if (strncmp("off", buf, 3) == 0) {
		pr_info("low_mem: disabling notifier\n");
		low_mem_margin_enabled = false;
		return count;
	}
	if (strncmp("on", buf, 2) == 0) {
		pr_info("low_mem: enabling notifier\n");
		low_mem_margin_enabled = true;
		return count;
	}
	/*
	 * This takes a space separated list of thresholds in ascending order,
	 * and a trailing newline is optional.
	 */
	while (consumed < count) {
		if (i >= LOW_MEM_THRESHOLD_MAX) {
			pr_warn("low-mem: too many thresholds");
			return -EINVAL;
		}
		/* special case for trailing newline */
		if (*start == '\n')
			break;

		thresholds[i] = simple_strtoul(start, &endp, 0);
		if ((endp == start) && *endp != '\n')
			return -EINVAL;

		/* make sure each is larger than the last one */
		if (i && thresholds[i] <= thresholds[i - 1]) {
			pr_warn("low-mem: thresholds not in increasing order: %lu then %lu\n",
				thresholds[i - 1], thresholds[i]);
			return -EINVAL;
		}

		if (thresholds[i] * (MB / PAGE_SIZE) > totalram_pages) {
			pr_warn("low-mem: threshold too high\n");
			return -EINVAL;
		}

		consumed += endp - start + 1;
		start = endp + 1;
		i++;
	}

	low_mem_threshold_count = i;
	low_mem_margin_enabled = !!low_mem_threshold_count;

	/* Convert to pages outside the allocator fast path. */
	for (i = 0; i < low_mem_threshold_count; i++) {
		low_mem_thresholds[i] =
			thresholds[i] * (MB / PAGE_SIZE);
		pr_info("low_mem: threshold[%d] %lu MB\n", i,
			low_mem_thresholds[i] * PAGE_SIZE / MB);
	}

	return count;
}
LOW_MEM_ATTR(margin);

static ssize_t low_mem_ram_vs_swap_weight_show(struct kobject *kobj,
					       struct kobj_attribute *attr,
					       char *buf)
{
	return sprintf(buf, "%u\n", low_mem_ram_vs_swap_weight);
}

static ssize_t low_mem_ram_vs_swap_weight_store(struct kobject *kobj,
						struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long weight;
	int err;

	err = kstrtoul(buf, 10, &weight);
	if (err)
		return -EINVAL;
	/* The special value 0 represents infinity. */
	low_mem_ram_vs_swap_weight = weight == 0 ?
		-1U : (unsigned int) weight;
	pr_info("low_mem: setting ram weight to %u\n",
		low_mem_ram_vs_swap_weight);
	return count;
}
LOW_MEM_ATTR(ram_vs_swap_weight);

static ssize_t low_mem_available_show(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      char *buf)
{
	unsigned long available_mem = get_available_mem_adj();

	return sprintf(buf, "%lu\n",
		       available_mem / (MB / PAGE_SIZE));
}

static ssize_t low_mem_available_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	return -EINVAL;
}
LOW_MEM_ATTR(available);

static struct attribute *low_mem_attrs[] = {
	&low_mem_margin_attr.attr,
	&low_mem_ram_vs_swap_weight_attr.attr,
	&low_mem_available_attr.attr,
	NULL,
};

static struct attribute_group low_mem_attr_group = {
	.attrs = low_mem_attrs,
	.name = "chromeos-low_mem",
};

void low_mem_threshold_notify(void)
{
	if (low_mem_available_dirent)
		sysfs_notify_dirent(low_mem_available_dirent);
}

static int __init low_mem_init(void)
{
	struct kernfs_node *low_mem_node;
	int err = sysfs_create_group(mm_kobj, &low_mem_attr_group);
	if (err)
		pr_err("low_mem: register sysfs failed\n");

	low_mem_node = sysfs_get_dirent(mm_kobj->sd, "chromeos-low_mem");
	if (low_mem_node) {
		low_mem_available_dirent =
		    sysfs_get_dirent(low_mem_node, "available");
		sysfs_put(low_mem_node);
	}

	if (!low_mem_available_dirent)
		pr_warn("unable to find dirent for \"available\" attribute\n");

	low_mem_lowest_seen_anon_mem = totalram_pages;
	return err;
}
module_init(low_mem_init)

#endif
