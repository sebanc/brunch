/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright(c) 2017-2021 Intel Corporation */

#ifndef __GNA_DEVICE_H__
#define __GNA_DEVICE_H__

#include <linux/atomic.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/types.h>

#include "hw.h"
#include "mem.h"

#define GNA_DV_NAME	"intel_gna"

struct workqueue_struct;
union gna_parameter;
struct device;
struct file;

struct gna_file_private {
	struct file *fd;
	struct gna_private *gna_priv;

	struct list_head memory_list;
	struct mutex memlist_lock;

	struct list_head flist;
};

struct gna_private {
	/* list of opened files */
	struct list_head file_list;
	/* protects file_list */
	struct mutex flist_lock;

	int index;

	int recovery_timeout_jiffies;

	/* gna misc-device */
	struct miscdevice misc;

	/* hardware status set by interrupt handler */
	u32 hw_status;

	/* device related resources */
	void __iomem *iobase;
	struct gna_dev_info info;
	struct gna_hw_info hw_info;

	struct gna_mmu_object mmu;
	struct mutex mmu_lock;

	/* if true, then gna device is processing */
	bool dev_busy;
	struct wait_queue_head dev_busy_waitq;

	struct list_head request_list;
	/* protects request_list */
	struct mutex reqlist_lock;
	struct workqueue_struct *request_wq;
	atomic_t request_count;

	/* memory objects' store */
	struct idr memory_idr;
	/* lock protecting memory_idr */
	struct mutex memidr_lock;
};

int gna_probe(struct device *parent, struct gna_dev_info *dev_info, void __iomem *iobase, int irq);
int gna_getparam(struct gna_private *gna_priv, union gna_parameter *param);

extern const struct dev_pm_ops __maybe_unused gna_pm;

static inline u32 gna_reg_read(struct gna_private *gna_priv, u32 reg)
{
	return readl(gna_priv->iobase + reg);
}

static inline void gna_reg_write(struct gna_private *gna_priv, u32 reg, u32 val)
{
	writel(val, gna_priv->iobase + reg);
}

static inline struct device *gna_parent(struct gna_private *gna_priv)
{
	return gna_priv->misc.parent;
}

static inline const char *gna_name(struct gna_private *gna_priv)
{
	return gna_priv->misc.name;
}

static inline struct device *gna_dev(struct gna_private *gna_priv)
{
	return gna_priv->misc.this_device;
}

#endif /* __GNA_DEVICE_H__ */
