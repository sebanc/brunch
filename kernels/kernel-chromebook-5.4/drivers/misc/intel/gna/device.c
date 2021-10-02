// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2021 Intel Corporation

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <uapi/misc/intel/gna.h>

#include "device.h"
#include "hw.h"
#include "ioctl.h"
#include "request.h"

static int recovery_timeout = 60;

#ifdef CONFIG_DEBUG_INTEL_GNA
module_param(recovery_timeout, int, 0644);
MODULE_PARM_DESC(recovery_timeout, "Recovery timeout in seconds");
#endif

static int __maybe_unused gna_runtime_suspend(struct device *dev)
{
	struct gna_private *gna_priv = dev_get_drvdata(dev);
	u32 val = gna_reg_read(gna_priv, GNA_MMIO_D0I3C);

	dev_dbg(dev, "%s D0I3, reg %.8x\n", __func__, val);

	return 0;
}

static int __maybe_unused gna_runtime_resume(struct device *dev)
{
	struct gna_private *gna_priv = dev_get_drvdata(dev);
	u32 val = gna_reg_read(gna_priv, GNA_MMIO_D0I3C);

	dev_dbg(dev, "%s D0I3, reg %.8x\n", __func__, val);

	return 0;
}

const struct dev_pm_ops __maybe_unused gna_pm = {
	SET_RUNTIME_PM_OPS(gna_runtime_suspend, gna_runtime_resume, NULL)
};

static int gna_open(struct inode *inode, struct file *f)
{
	struct gna_file_private *file_priv;
	struct gna_private *gna_priv;

	gna_priv = container_of(f->private_data, struct gna_private, misc);

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file_priv->fd = f;
	file_priv->gna_priv = gna_priv;

	mutex_init(&file_priv->memlist_lock);
	INIT_LIST_HEAD(&file_priv->memory_list);

	INIT_LIST_HEAD(&file_priv->flist);

	mutex_lock(&gna_priv->flist_lock);
	list_add_tail(&file_priv->flist, &gna_priv->file_list);
	mutex_unlock(&gna_priv->flist_lock);

	f->private_data = file_priv;

	return 0;
}

static int gna_release(struct inode *inode, struct file *f)
{
	struct gna_memory_object *iter_mo, *temp_mo;
	struct gna_file_private *file_priv;
	struct gna_private *gna_priv;

	/* free all memory objects created by that file */
	file_priv = (struct gna_file_private *)f->private_data;
	gna_priv = file_priv->gna_priv;

	mutex_lock(&file_priv->memlist_lock);
	list_for_each_entry_safe(iter_mo, temp_mo, &file_priv->memory_list, file_mem_list) {
		queue_work(gna_priv->request_wq, &iter_mo->work);
		wait_event(iter_mo->waitq, true);
		gna_memory_free(gna_priv, iter_mo);
	}
	mutex_unlock(&file_priv->memlist_lock);

	gna_delete_file_requests(f, gna_priv);

	mutex_lock(&gna_priv->flist_lock);
	list_del_init(&file_priv->flist);
	mutex_unlock(&gna_priv->flist_lock);
	kfree(file_priv);
	f->private_data = NULL;

	return 0;
}

static const struct file_operations gna_file_ops = {
	.owner		=	THIS_MODULE,
	.open		=	gna_open,
	.release	=	gna_release,
	.unlocked_ioctl =	gna_ioctl,
};

static void gna_devm_idr_destroy(void *data)
{
	struct idr *idr = data;

	idr_destroy(idr);
}

static void gna_devm_deregister_misc_dev(void *data)
{
	struct miscdevice *misc = data;

	misc_deregister(misc);
}

static int gna_devm_register_misc_dev(struct device *parent, struct miscdevice *misc)
{
	int ret;

	ret = misc_register(misc);
	if (ret) {
		dev_err(parent, "misc device %s registering failed. errcode: %d\n",
			misc->name, ret);
		gna_devm_deregister_misc_dev(misc);
	} else {
		dev_dbg(parent, "device: %s registered\n",
			misc->name);
	}

	ret = devm_add_action(parent, gna_devm_deregister_misc_dev, misc);
	if (ret) {
		dev_err(parent, "could not add devm action for %s misc deregister\n", misc->name);
		gna_devm_deregister_misc_dev(misc);
	}

	return ret;
}

static void gna_pm_init(struct device *dev)
{
	pm_runtime_set_autosuspend_delay(dev, 200);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_allow(dev);
	pm_runtime_put_noidle(dev);
}

static void gna_pm_remove(void *data)
{
	struct device *dev = data;

	pm_runtime_get_noresume(dev);
}

static irqreturn_t gna_interrupt(int irq, void *priv)
{
	struct gna_private *gna_priv;

	gna_priv = (struct gna_private *)priv;
	gna_priv->dev_busy = false;
	wake_up(&gna_priv->dev_busy_waitq);
	return IRQ_HANDLED;
}

static void gna_devm_destroy_workqueue(void *data)
{
	struct workqueue_struct *request_wq = data;

	destroy_workqueue(request_wq);
}

static int gna_devm_create_singlethread_workqueue(struct gna_private *gna_priv)
{
	struct device *dev = gna_parent(gna_priv);
	const char *name = gna_name(gna_priv);
	int ret;

	gna_priv->request_wq = create_singlethread_workqueue(name);
	if (!gna_priv->request_wq) {
		dev_err(dev, "could not create %s workqueue\n", name);
		return -EFAULT;
	}

	ret = devm_add_action(dev, gna_devm_destroy_workqueue, gna_priv->request_wq);
	if (ret) {
		dev_err(dev, "could not add devm action for %s workqueue\n", name);
		gna_devm_destroy_workqueue(gna_priv->request_wq);
	}

	return ret;
}

int gna_probe(struct device *parent, struct gna_dev_info *dev_info, void __iomem *iobase, int irq)
{
	static atomic_t dev_last_idx = ATOMIC_INIT(-1);
	struct gna_private *gna_priv;
	const char *dev_misc_name;
	u32 bld_reg;
	int ret;

	gna_priv = devm_kzalloc(parent, sizeof(*gna_priv), GFP_KERNEL);
	if (!gna_priv)
		return -ENOMEM;

	gna_priv->index = atomic_inc_return(&dev_last_idx);
	gna_priv->recovery_timeout_jiffies = msecs_to_jiffies(recovery_timeout * 1000);
	gna_priv->iobase = iobase;
	gna_priv->info = *dev_info;
	gna_priv->misc.parent = parent;

	dev_misc_name = devm_kasprintf(parent, GFP_KERNEL, "%s%d", GNA_DV_NAME, gna_priv->index);
	if (!dev_misc_name)
		return -ENOMEM;

	gna_priv->misc.name = dev_misc_name;

	if (!(sizeof(dma_addr_t) > 4) ||
		dma_set_mask(parent, DMA_BIT_MASK(64))) {
		ret = dma_set_mask(parent, DMA_BIT_MASK(32));
		if (ret) {
			dev_err(parent, "dma_set_mask error: %d\n", ret);
			return ret;
		}
	}

	bld_reg = gna_reg_read(gna_priv, GNA_MMIO_IBUFFS);
	gna_priv->hw_info.in_buf_s = bld_reg & GENMASK(7, 0);

	ret = gna_mmu_alloc(gna_priv);
	if (ret) {
		dev_err(parent, "mmu allocation failed\n");
		return ret;

	}
	dev_dbg(parent, "maximum memory size %llu num pd %d\n",
		gna_priv->info.max_hw_mem, gna_priv->info.num_pagetables);
	dev_dbg(parent, "desc rsvd size %d mmu vamax size %d\n",
		gna_priv->info.desc_info.rsvd_size,
		gna_priv->info.desc_info.mmu_info.vamax_size);

	mutex_init(&gna_priv->mmu_lock);

	idr_init(&gna_priv->memory_idr);
	ret = devm_add_action(parent, gna_devm_idr_destroy, &gna_priv->memory_idr);
	if (ret) {
		dev_err(parent, "could not add devm action for idr\n");
		return ret;
	}

	mutex_init(&gna_priv->memidr_lock);

	mutex_init(&gna_priv->flist_lock);
	INIT_LIST_HEAD(&gna_priv->file_list);

	atomic_set(&gna_priv->request_count, 0);

	mutex_init(&gna_priv->reqlist_lock);
	INIT_LIST_HEAD(&gna_priv->request_list);

	init_waitqueue_head(&gna_priv->dev_busy_waitq);

	ret = gna_devm_create_singlethread_workqueue(gna_priv);
	if (ret)
		return ret;

	ret = devm_request_irq(parent, irq, gna_interrupt,
			IRQF_SHARED, dev_misc_name, gna_priv);
	if (ret) {
		dev_err(parent, "could not register for interrupt\n");
		return ret;
	}

	gna_priv->misc.minor = MISC_DYNAMIC_MINOR;
	gna_priv->misc.fops = &gna_file_ops;
	gna_priv->misc.mode = 0666;

	ret = gna_devm_register_misc_dev(parent, &gna_priv->misc);
	if (ret)
		return ret;

	dev_set_drvdata(parent, gna_priv);

	gna_pm_init(parent);
	ret = devm_add_action(parent, gna_pm_remove, parent);
	if (ret) {
		dev_err(parent, "could not add devm action for pm\n");
		return ret;
	}

	return 0;
}

static u32 gna_device_type_by_hwid(u32 hwid)
{
	switch (hwid) {
	case GNA_DEV_HWID_CNL:
		return GNA_DEV_TYPE_0_9;
	case GNA_DEV_HWID_GLK:
	case GNA_DEV_HWID_EHL:
	case GNA_DEV_HWID_ICL:
		return GNA_DEV_TYPE_1_0;
	case GNA_DEV_HWID_JSL:
	case GNA_DEV_HWID_TGL:
	case GNA_DEV_HWID_RKL:
		return GNA_DEV_TYPE_2_0;
	case GNA_DEV_HWID_ADL:
	case GNA_DEV_HWID_RPL:
		return GNA_DEV_TYPE_3_0;
	default:
		return 0;
	}
}

int gna_getparam(struct gna_private *gna_priv, union gna_parameter *param)
{
	switch (param->in.id) {
	case GNA_PARAM_DEVICE_ID:
		param->out.value = gna_priv->info.hwid;
		break;
	case GNA_PARAM_RECOVERY_TIMEOUT:
		param->out.value = jiffies_to_msecs(gna_priv->recovery_timeout_jiffies) / 1000;
		break;
	case GNA_PARAM_INPUT_BUFFER_S:
		param->out.value = gna_priv->hw_info.in_buf_s;
		break;
	case GNA_PARAM_DEVICE_TYPE:
		param->out.value = gna_device_type_by_hwid(gna_priv->info.hwid);
		break;
	default:
		dev_err(gna_dev(gna_priv), "unknown parameter id %llu\n", param->in.id);
		return -EINVAL;
	}

	return 0;
}

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Gaussian & Neural Accelerator (Intel(R) GNA) Driver");
MODULE_LICENSE("GPL");
