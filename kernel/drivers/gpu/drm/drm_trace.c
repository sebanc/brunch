/* SPDX-License-Identifier: MIT */
/*
 * Copyright (C) 2020 Google, Inc.
 *
 * Authors:
 * Sean Paul <seanpaul@chromium.org>
 */

#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/ring_buffer.h>
#include <linux/slab.h>
#include <linux/stat.h>

#include <drm/drm_device.h>
#include <drm/drm_print.h>

#define DRM_TRACE_MAX_LEN	256

/**
 * DOC: DRM Tracing
 *
 * *tl;dr* DRM tracing is a lightweight alternative to traditional DRM debug
 * logging.
 *
 * While DRM logging is quite convenient when reproducing a specific issue, it
 * doesn't help when something goes wrong unexpectedly. There are a couple
 * reasons why one does not want to enable DRM logging at all times:
 *
 * 1. We don't want to overwhelm syslog with drm spam, others have to use it too
 * 2. Console logging is slow
 *
 * DRM tracing aims to solve both these problems.
 *
 * To use DRM tracing, write a DRM debug category mask (this is a bitmask of
 * &drm_debug_category values) to the trace_mask file:
 * ::
 *
 *    eg: echo 0x106 > /sys/kernel/debug/dri/trace_mask
 *
 * Once active, all log messages in the specified categories will be written to
 * the DRM trace. Once at capacity, the trace will overwrite old messages with
 * new ones. At any point, one can read the trace file to extract the previous N
 * DRM messages:
 * ::
 *
 *    eg: cat /sys/kernel/debug/dri/trace
 *
 * Considerations
 * **************
 * The contents of the DRM Trace are **not** considered UABI. **DO NOT depend on
 * the values of these traces in your userspace.** These traces are intended for
 * entertainment purposes only. The contents of these logs carry no warranty,
 * expressed or implied.
 *
 * New traces can not be added to the trace buffer while it is being read. If
 * this proves to be a problem, it can be mitigated by making a copy of the
 * buffer on start of read. Since DRM trace is not meant to be continuously
 * read, this loss is acceptable.
 *
 * The timestamps on logs are CPU-local. As such, log messages from different
 * CPUs may have slightly different ideas about time.
 *
 * Since each CPU has its own buffer, they won't all overflow at the same rate.
 * This means that messages from a particularly active CPU could be dropped
 * while an inactive CPU might have much older log messages. So don't be fooled
 * if you seem to be missing log messages when you see a switch between CPUs in
 * the logs.
 *
 * Internals
 * *********
 * The DRM Tracing functions are intentionally unexported, they are not meant to
 * be used by drivers directly. The reasons are twofold:
 *
 * 1. All messages going to traces should also go to the console logs. This
 *    ensures users can choose their logging medium without fear they're losing
 *    messages.
 * 2. Writing directly to the trace skips category filtering, resulting in trace
 *    spam.
 */

struct drm_trace_info {
	struct ring_buffer *buffer;
	struct dentry *debugfs;
	struct dentry *debugfs_mask;
	struct dentry *debugfs_size;
	u32 category_mask;
};
static struct drm_trace_info drm_trace;

struct drm_trace_seq_iter {
	loff_t pos;
	cpumask_var_t cpu_mask;
	int cpu;
	u64 ts;
};

static void *drm_trace_seq_start(struct seq_file *seq_file, loff_t *pos)
{
	struct drm_trace_info *info = seq_file->private;
	struct drm_trace_seq_iter *iter;
	int cpu;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return NULL;

	if (!zalloc_cpumask_var(&iter->cpu_mask, GFP_KERNEL))
		return NULL;

	/*
	 * TODO: We could do better than stopping record for the entirety of the
	 *	 read session.
	 */
	ring_buffer_record_off(info->buffer);

	/*
	 * pos is only used as a means of determining whether we're at the start
	 * of the virtual file, or continuing a read. We don't want to skip over
	 * log lines since that's not a meaningful thing to do.
	 */
	iter->pos = *pos;

	iter->cpu = -1;

	/*
	 * There's no way to extract the ring buffer's cpumask, so we'll try
	 * every possible cpu and skip the invalid entries.
	 */
	for_each_possible_cpu(cpu) {
		if (ring_buffer_entries_cpu(info->buffer, cpu))
			cpumask_set_cpu(cpu, iter->cpu_mask);
	}

	return iter;
}

static void *drm_trace_seq_next(struct seq_file *seq_file, void *data,
				loff_t *pos)
{
	struct drm_trace_info *info = seq_file->private;
	struct drm_trace_seq_iter *iter = data;
	int cpu;

	*pos = ++iter->pos;
	iter->cpu = -1;
	iter->ts = 0;

	/* Find the oldest event across our cpu_mask */
	for_each_cpu(cpu, iter->cpu_mask) {
		u64 ts;

		if (!ring_buffer_peek(info->buffer, cpu, &ts, NULL)) {
			cpumask_clear_cpu(cpu, iter->cpu_mask);
			continue;
		}
		if (iter->cpu == -1 || ts < iter->ts) {
			iter->ts = ts;
			iter->cpu = cpu;
		}
	}
	if (iter->cpu == -1)
		return NULL;

	return iter;
}

static void drm_trace_seq_stop(struct seq_file *seq_file, void *data)
{
	struct drm_trace_info *info = seq_file->private;
	struct drm_trace_seq_iter *iter = data;

	free_cpumask_var(iter->cpu_mask);
	kfree(iter);

	ring_buffer_record_on(info->buffer);
}

static int drm_trace_seq_show(struct seq_file *seq_file, void *data)
{
	struct drm_trace_info *info = seq_file->private;
	struct drm_trace_seq_iter *iter = data;
	struct ring_buffer_event *event;
	u64 ts, usec;

	if (iter->pos == 0) {
		seq_printf(seq_file, "[%3s %12s] %s\n", "cpu", "timestamp",
			   "message");
		return 0;
	} else if (iter->cpu == -1) {
		/* This happens when we start a session with position > 0 */
		return SEQ_SKIP;
	}

	event = ring_buffer_consume(info->buffer, iter->cpu, &ts, NULL);

	ts += 500;
	/* ts converts from ns->us */
	do_div(ts, 1000);
	/* ts converts from us->s */
	usec = do_div(ts, USEC_PER_SEC);

	seq_printf(seq_file, "[%3u %5llu.%06llu] %s", iter->cpu, ts, usec,
		   (const char *)ring_buffer_event_data(event));

	return 0;
}

static const struct seq_operations drm_trace_sops = {
	.start = drm_trace_seq_start,
	.next = drm_trace_seq_next,
	.stop = drm_trace_seq_stop,
	.show  = drm_trace_seq_show
};

static int drm_trace_fop_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq_file;
	int ret;

	ret = seq_open(file, &drm_trace_sops);
	if (ret)
		return ret;

	seq_file = (struct seq_file *)file->private_data;
	seq_file->private = inode->i_private; /* this is drm_trace_info */

	return 0;
}

static const struct file_operations drm_trace_fops = {
	.open = drm_trace_fop_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int drm_trace_size_fop_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t drm_trace_size_fop_read(struct file *filp, char __user *ubuf,
				       size_t cnt, loff_t *ppos)
{
	struct drm_trace_info *info = filp->private_data;
	unsigned long size;
	char buf[32];
	int cpu, len;

	/* All CPUs have the same sized buffer, so just grab the first size */
	for_each_possible_cpu(cpu) {
		size = ring_buffer_size(info->buffer, cpu);
		if (size)
			break;
	}

	len = scnprintf(buf, sizeof(buf), "%lu\n", size / 1024);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
}

static ssize_t drm_trace_size_fop_write(struct file *filp,
					const char __user *ubuf, size_t cnt,
					loff_t *ppos)
{
	struct drm_trace_info *info = filp->private_data;
	unsigned int val;
	int ret;

	ret = kstrtouint_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	ret = ring_buffer_resize(info->buffer, val * 1024,
				 RING_BUFFER_ALL_CPUS);
	if (ret < 0)
		return ret;

	*ppos += cnt;

	return cnt;
}

static const struct file_operations drm_trace_size_fops = {
	.open = drm_trace_size_fop_open,
	.read = drm_trace_size_fop_read,
	.write = drm_trace_size_fop_write,
};

/**
 * drm_trace_init - initializes tracing for drm core
 * @debugfs_root: the dentry for drm core's debugfs root
 *
 * This function is called on drm core init. It is responsible for initializing
 * drm tracing. This function must be matched by a call to drm_trace_cleanup().
 *
 * Returns: 0 on success, -errno on failure
 */
int drm_trace_init(struct dentry *debugfs_root)
{
	struct drm_trace_info *info = &drm_trace;
	int ret;

	info->buffer = ring_buffer_alloc(PAGE_SIZE * 2, RB_FL_OVERWRITE);
	if (!info->buffer)
		return -ENOMEM;

	info->debugfs_mask = debugfs_create_u32("trace_mask",
						S_IFREG | S_IRUGO | S_IWUSR,
						debugfs_root,
						&info->category_mask);
	if (IS_ERR(info->debugfs)) {
		ret = PTR_ERR(info->debugfs);
		goto err_debugfs_mask;
	}

	info->debugfs_size = debugfs_create_file("trace_size_kb",
						S_IFREG | S_IRUGO | S_IWUSR,
						debugfs_root, info,
						&drm_trace_size_fops);
	if (IS_ERR(info->debugfs_size)) {
		ret = PTR_ERR(info->debugfs_size);
		goto err_debugfs_size;
	}

	info->debugfs = debugfs_create_file("trace", S_IFREG | S_IRUGO,
					    debugfs_root, info,
					    &drm_trace_fops);
	if (IS_ERR(info->debugfs)) {
		ret = PTR_ERR(info->debugfs);
		goto err_debugfs;
	}

	return 0;

err_debugfs:
	debugfs_remove(info->debugfs_size);
err_debugfs_size:
	debugfs_remove(info->debugfs_mask);
err_debugfs_mask:
	ring_buffer_free(info->buffer);
	return ret;
}
EXPORT_SYMBOL(drm_trace_init);

/**
 * drm_trace_cleanup - cleans up tracing for drm core
 *
 * This function is responsible for cleaning up anything that was previously
 * initialized in drm_trace_init()
 */
void drm_trace_cleanup()
{
	struct drm_trace_info *info = &drm_trace;

	debugfs_remove(info->debugfs);
	debugfs_remove(info->debugfs_mask);
	debugfs_remove(info->debugfs_size);
	ring_buffer_free(info->buffer);
	memset(info, 0, sizeof(*info));
}
EXPORT_SYMBOL(drm_trace_cleanup);

/**
 * drm_trace_enabled - check if a debug category has traces enabled
 * @category: the debug category to check
 *
 * Returns true if the given category has drm traces enabled, false otherwise.
 */
bool drm_trace_enabled(u32 category)
{
	return READ_ONCE(drm_trace.category_mask) & category;
}
EXPORT_SYMBOL(drm_trace_enabled);

static int drm_trace_write(const void *data, unsigned int len)
{
	struct drm_trace_info *info = &drm_trace;
	struct ring_buffer_event *event;
	void *event_body;

	if (!info->buffer)
		return -ENOMEM;
	event = ring_buffer_lock_reserve(info->buffer, len);
	if (!event)
		return -ENOMEM;

	event_body = ring_buffer_event_data(event);
	memcpy(event_body, data, len);

	return ring_buffer_unlock_commit(info->buffer, event);
}

/**
 * drm_trace_printf - adds an entry to the drm trace
 * @format: printf format of the message to add to the trace
 *
 * This function adds a new entry in the drm trace.
 */
void drm_trace_printf(const char *format, ...)
{
	char buf[DRM_TRACE_MAX_LEN];
	va_list args;
	int ret;

	va_start(args, format);
	ret = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if (ret < 0)
		return;
	else if (ret >= sizeof(buf))
		ret = sizeof(buf) - 1;

	drm_trace_write(buf, ret + 1);
}
EXPORT_SYMBOL(drm_trace_printf);

/**
 * drm_dev_trace_printf - adds an entry to the drm trace
 * @dev: pointer to device so we can print the name
 * @format: printf format of the message to add to the trace
 *
 * This function adds a new entry in the drm trace.
 */
void drm_dev_trace_printf(const struct device *dev, const char *format, ...)
{
	char buf[DRM_TRACE_MAX_LEN];
	struct va_format vaf;
	va_list args;
	int ret;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;
	ret = snprintf(buf, sizeof(buf), "%s %s: %pV", dev_driver_string(dev),
		       dev_name(dev), &vaf);
	va_end(args);

	if (ret < 0)
		return;
	else if (ret >= sizeof(buf))
		ret = sizeof(buf) - 1;

	drm_trace_write(buf, ret + 1);
}
EXPORT_SYMBOL(drm_dev_trace_printf);