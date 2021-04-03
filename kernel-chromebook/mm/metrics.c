// SPDX-License-Identifier: GPL-2.0+

#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/swapfile.h>
#include <linux/debugfs.h>
#include <linux/mm_metrics.h>

/* make sure swapout timestamp won't wrap around within a year */
#define SECONDS_PER_YEAR	(60 * 60 * 24 * 365)
/* max number of buckets for histogram */
#define MAX_HISTOGRAM_SIZE	100
/* max number of digits in decimal for threshold plus one space */
#define MAX_CHARS_PER_THRESHOLD	(20 + 1)

bool swap_refault_enabled __read_mostly;
struct histogram __rcu *mm_metrics_files[NR_MM_METRICS];

static const char *const mm_metrics_names[] = {
	"swap_refault",
	"swap_latency",
	"reclaim_latency",
};

static DEFINE_SPINLOCK(histogram_lock);

static struct histogram *histogram_alloc(const u64 *thresholds,
					 unsigned int size)
{
	int i;
	int len;
	struct histogram *hist;

	VM_BUG_ON(!size || size > MAX_HISTOGRAM_SIZE);

	len = sizeof(struct histogram) + size * sizeof(*hist->thresholds);
	hist = kmalloc(len, GFP_ATOMIC);
	if (!hist)
		return ERR_PTR(-ENOMEM);

	len = size * sizeof(*hist->buckets);
	hist->buckets = __alloc_percpu_gfp(len, __alignof__(*hist->buckets),
					   GFP_ATOMIC);
	if (!hist->buckets) {
		kfree(hist);
		return ERR_PTR(-ENOMEM);
	}

	hist->size = size;
	for (i = 0; i < size; i++) {
		VM_BUG_ON(i && thresholds[i - 1] >= thresholds[i]);

		hist->thresholds[i] = thresholds[i];
	}
	VM_BUG_ON(thresholds[i - 1] != U64_MAX);

	return hist;
}

static struct histogram *histogram_create(char *buf)
{
	int i;
	unsigned int size;
	u64 *thresholds;
	struct histogram *hist;

	if (!*buf)
		return ERR_PTR(-EINVAL);

	thresholds = kmalloc_array(MAX_HISTOGRAM_SIZE, sizeof(*thresholds),
				   GFP_KERNEL);
	if (!thresholds)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < MAX_HISTOGRAM_SIZE; i++) {
		thresholds[i] = simple_strtoull(buf, &buf, 0);
		if (!*buf)
			break;

		if (!isspace(*buf)) {
			hist = ERR_PTR(-EINVAL);
			goto failed;
		}

		while (isspace(*buf))
			buf++;
	}

	if (i == MAX_HISTOGRAM_SIZE) {
		hist = ERR_PTR(-E2BIG);
		goto failed;
	}

	/* the last theshold must be U64_MAX, add it if missing */
	if (thresholds[i++] != U64_MAX) {
		if (i == MAX_HISTOGRAM_SIZE) {
			hist = ERR_PTR(-E2BIG);
			goto failed;
		}
		thresholds[i++] = U64_MAX;
	}

	size = i;

	for (i = 1; i < size; i++) {
		if (thresholds[i - 1] >= thresholds[i]) {
			hist = ERR_PTR(-EINVAL);
			goto failed;
		}
	}

	hist = histogram_alloc(thresholds, size);
failed:
	kfree(thresholds);

	return hist;
}

static void histogram_free(struct rcu_head *rcu)
{
	struct histogram *hist = container_of(rcu, struct histogram, rcu);

	VM_BUG_ON(!hist->size || hist->size > MAX_HISTOGRAM_SIZE);

	free_percpu(hist->buckets);
	kfree(hist);
}

static int mm_metrics_read(struct seq_file *sf, void *v)
{
	int i;
	int cpu;
	u64 *buckets;
	struct histogram *hist;
	int rc = 0;
	unsigned int type = (unsigned long)sf->private;

	VM_BUG_ON(type >= NR_MM_METRICS);

	rcu_read_lock();

	hist = rcu_dereference(mm_metrics_files[type]);
	if (!hist) {
		seq_puts(sf, "disabled\n");
		goto unlock;
	}

	VM_BUG_ON(!hist->size || hist->size > MAX_HISTOGRAM_SIZE);

	buckets = kmalloc_array(hist->size, sizeof(*buckets), GFP_NOWAIT);
	if (!buckets) {
		rc = -ENOMEM;
		goto unlock;
	}

	memset(buckets, 0, hist->size * sizeof(*buckets));

	for_each_possible_cpu(cpu) {
		for (i = 0; i < hist->size; i++)
			buckets[i] += per_cpu(hist->buckets[i], cpu);
	}

	for (i = 0; i < hist->size; i++) {
		u64 lower = i ? hist->thresholds[i - 1] + 1 : 0;
		u64 upper = hist->thresholds[i];

		VM_BUG_ON(lower > upper);

		seq_printf(sf, "%llu-%llu %llu\n", lower, upper, buckets[i]);
	}
	VM_BUG_ON(hist->thresholds[i - 1] != U64_MAX);

	kfree(buckets);
unlock:
	rcu_read_unlock();

	return rc;
}

static ssize_t mm_metrics_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	char *raw, *trimmed;
	struct histogram *old, *new = NULL;
	unsigned int type = (unsigned long)file_inode(file)->i_private;

	VM_BUG_ON(type >= NR_MM_METRICS);

	if (len > MAX_HISTOGRAM_SIZE * MAX_CHARS_PER_THRESHOLD)
		return -E2BIG;

	raw = memdup_user_nul(buf, len);
	if (IS_ERR(raw))
		return PTR_ERR(raw);

	trimmed = strim(raw);
	if (!strcmp(trimmed, "clear")) {
		rcu_read_lock();
		old = rcu_dereference(mm_metrics_files[type]);
		if (old)
			new = histogram_alloc(old->thresholds, old->size);
		rcu_read_unlock();
	} else if (strcmp(trimmed, "disable"))
		new = histogram_create(trimmed);

	kfree(raw);

	if (IS_ERR(new))
		return PTR_ERR(new);

	spin_lock(&histogram_lock);
	old = rcu_dereference_protected(mm_metrics_files[type],
					lockdep_is_held(&histogram_lock));
	rcu_assign_pointer(mm_metrics_files[type], new);
	spin_unlock(&histogram_lock);
	if (old)
		call_rcu(&old->rcu, histogram_free);

	return len;
}

static int mm_metrics_open(struct inode *inode, struct file *file)
{
	return single_open(file, mm_metrics_read, inode->i_private);
}

static const struct file_operations mm_metrics_ops = {
	.open		= mm_metrics_open,
	.write		= mm_metrics_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init mm_metrics_init(void)
{
	int i;
	struct dentry *dent;
#ifdef CONFIG_SWAP
	unsigned long now = ktime_get_seconds();
	unsigned long size = max_swapfile_size();

	if (SWP_TM_OFF_BITS > FIELD_SIZEOF(swp_entry_t, val) * BITS_PER_BYTE)
		pr_err("swap refault metrics disabled: 32-bit CPU\n");
	else if (size < GENMASK_ULL(SWP_TM_OFF_BITS - 1, 0) + 1)
		pr_err("swap refault metrics disabled: size %ld\n", size);
	else if (now + SECONDS_PER_YEAR > BIT_ULL(SWP_TIME_BITS))
		pr_err("swap refault metrics disabled: time %ld\n", now);
	else
		swap_refault_enabled = true;
#endif

	BUILD_BUG_ON(ARRAY_SIZE(mm_metrics_names) != NR_MM_METRICS);

	if (!debugfs_initialized())
		return -ENODEV;

	dent = debugfs_create_dir("mm_metrics", NULL);
	if (!dent)
		return -ENODEV;

	for (i = 0; i < NR_MM_METRICS; i++) {
		struct dentry *fent;

		if (i == MM_SWAP_REFAULT && !swap_refault_enabled)
			continue;

		fent = debugfs_create_file(mm_metrics_names[i], 0644, dent,
					   (void *)(long)i, &mm_metrics_ops);
		if (IS_ERR_OR_NULL(fent)) {
			debugfs_remove_recursive(dent);

			return -ENODEV;
		}
	}

	pr_info("memory metrics initialized\n");

	return 0;
}
subsys_initcall(mm_metrics_init);

void mm_metrics_record(unsigned int type, u64 val, u64 count)
{
	int lower, upper;
	struct histogram *hist;

	VM_BUG_ON(type >= NR_MM_METRICS);

	rcu_read_lock();

	hist = rcu_dereference(mm_metrics_files[type]);
	if (!hist)
		goto unlock;

	VM_BUG_ON(!hist->size || hist->size > MAX_HISTOGRAM_SIZE);

	lower = 0;
	upper = hist->size - 1;
	while (lower < upper) {
		int i = (lower + upper) >> 1;

		if (val <= hist->thresholds[i])
			upper = i;
		else
			lower = i + 1;
	}

	this_cpu_add(hist->buckets[upper], count);
unlock:
	rcu_read_unlock();
}
