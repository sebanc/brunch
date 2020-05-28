// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include <media/videobuf2-dma-contig.h>
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-m2m.h"

/* MDP debug log level (0-3). 3 shows all the logs. */
int mtk_mdp_debug;
EXPORT_SYMBOL(mtk_mdp_debug);
module_param_named(debug, mtk_mdp_debug, int, 0644);

static const struct of_device_id mdp_of_ids[] = {
	{ .compatible = "mediatek,mt8183-mdp3", },
	{},
};
MODULE_DEVICE_TABLE(of, mdp_of_ids);

struct platform_device *mdp_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *mdp_node;
	struct platform_device *mdp_pdev;

	mdp_node = of_parse_phandle(dev->of_node, "mediatek,mdp3", 0);
	if (!mdp_node) {
		dev_err(dev, "can't get mdp node\n");
		return NULL;
	}

	mdp_pdev = of_find_device_by_node(mdp_node);
	if (WARN_ON(!mdp_pdev)) {
		dev_err(dev, "mdp pdev failed\n");
		of_node_put(mdp_node);
		return NULL;
	}

	return mdp_pdev;
}
EXPORT_SYMBOL_GPL(mdp_get_plat_device);

int mdp_vpu_get_locked(struct mdp_dev *mdp)
{
	int ret = 0;

	if (mdp->vpu_count++ == 0) {
		ret = rproc_boot(mdp->rproc_handle);
		if (ret) {
			dev_err(&mdp->pdev->dev,
				"vpu_load_firmware failed %d\n", ret);
			goto err_load_vpu;
		}
		ret = mdp_vpu_register(mdp);
		if (ret) {
			dev_err(&mdp->pdev->dev,
				"mdp_vpu register failed %d\n", ret);
			goto err_reg_vpu;
		}
		ret = mdp_vpu_dev_init(&mdp->vpu, mdp->scp, &mdp->vpu_lock);
		if (ret) {
			dev_err(&mdp->pdev->dev,
				"mdp_vpu device init failed %d\n", ret);
			goto err_init_vpu;
		}
	}
	return 0;

err_init_vpu:
	mdp_vpu_unregister(mdp);
err_reg_vpu:
err_load_vpu:
	mdp->vpu_count--;
	return ret;
}

void mdp_vpu_put_locked(struct mdp_dev *mdp)
{
	if (--mdp->vpu_count == 0) {
		mdp_vpu_dev_deinit(&mdp->vpu);
		mdp_vpu_unregister(mdp);
	}
}

static int mdp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mdp_dev *mdp;
	int ret;

	mdp = devm_kzalloc(dev, sizeof(*mdp), GFP_KERNEL);
	if (!mdp)
		return -ENOMEM;

	mdp->pdev = pdev;
	ret = mdp_component_init(mdp);
	if (ret) {
		dev_err(dev, "Failed to initialize mdp components\n");
		goto err_return;
	}

	mdp->job_wq = alloc_workqueue(MDP_MODULE_NAME, WQ_FREEZABLE, 0);
	if (!mdp->job_wq) {
		dev_err(dev, "Unable to create job workqueue\n");
		ret = -ENOMEM;
		goto err_destroy_job_wq;
	}

	mdp->clock_wq = alloc_workqueue(MDP_MODULE_NAME "-clock", WQ_FREEZABLE,
					0);
	if (!mdp->clock_wq) {
		dev_err(dev, "Unable to create clock workqueue\n");
		ret = -ENOMEM;
		goto err_destroy_clock_wq;
	}

	mdp->scp = scp_get(pdev);
	if (!mdp->scp) {
		dev_err(&pdev->dev, "Could not get scp device\n");
		ret = -ENODEV;
		goto err_destroy_clock_wq;
	}

	mdp->rproc_handle = scp_get_rproc(mdp->scp);
	dev_info(&pdev->dev, "MDP rproc_handle: %pK", mdp->rproc_handle);

	mutex_init(&mdp->vpu_lock);
	mutex_init(&mdp->m2m_lock);

	mdp->cmdq_clt = cmdq_mbox_create(dev, 0, 1200);
	if (IS_ERR(mdp->cmdq_clt)) {
		ret = PTR_ERR(mdp->cmdq_clt);
		goto err_put_scp;
	}

	init_waitqueue_head(&mdp->callback_wq);
	ida_init(&mdp->mdp_ida);
	platform_set_drvdata(pdev, mdp);

	vb2_dma_contig_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));
	pm_runtime_enable(dev);

	ret = v4l2_device_register(dev, &mdp->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_mbox_destroy;
	}

	ret = mdp_m2m_device_register(mdp);
	if (ret) {
		v4l2_err(&mdp->v4l2_dev, "Failed to register m2m device\n");
		goto err_unregister_device;
	}

	dev_dbg(dev, "mdp-%d registered successfully\n", pdev->id);
	return 0;

err_unregister_device:
	v4l2_device_unregister(&mdp->v4l2_dev);
err_mbox_destroy:
	cmdq_mbox_destroy(mdp->cmdq_clt);
err_put_scp:
	scp_put(mdp->scp);
err_destroy_clock_wq:
	destroy_workqueue(mdp->clock_wq);
err_destroy_job_wq:
	destroy_workqueue(mdp->job_wq);
err_return:
	dev_dbg(dev, "Errno %d\n", ret);
	return ret;
}

static int mdp_remove(struct platform_device *pdev)
{
	struct mdp_dev *mdp = platform_get_drvdata(pdev);

	mdp_m2m_device_unregister(mdp);
	v4l2_device_unregister(&mdp->v4l2_dev);

	scp_put(mdp->scp);

	destroy_workqueue(mdp->job_wq);
	destroy_workqueue(mdp->clock_wq);

	pm_runtime_disable(&pdev->dev);

	vb2_dma_contig_clear_max_seg_size(&pdev->dev);
	mdp_component_deinit(mdp);

	mdp_vpu_shared_mem_free(&mdp->vpu);

	dev_dbg(&pdev->dev, "%s driver unloaded\n", pdev->name);
	return 0;
}

static int __maybe_unused mdp_pm_suspend(struct device *dev)
{
	struct mdp_dev *mdp = dev_get_drvdata(dev);
	int ret;

	atomic_set(&mdp->suspended, 1);

	if (atomic_read(&mdp->job_count)) {
		ret = wait_event_timeout(mdp->callback_wq,
					 !atomic_read(&mdp->job_count),
					 HZ);
		if (ret == 0)
			dev_err(dev,
				"%s:flushed cmdq task incomplete\n",
				__func__);
		else//need to remove
			pr_err("%s:ret=%d\n", __func__, ret);
	}

	return 0;
}

static int __maybe_unused mdp_pm_resume(struct device *dev)
{
	struct mdp_dev *mdp = dev_get_drvdata(dev);

	atomic_set(&mdp->suspended, 0);

	return 0;
}

static int __maybe_unused mdp_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mdp_pm_suspend(dev);
}

static int __maybe_unused mdp_resume(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return mdp_pm_resume(dev);
}

static const struct dev_pm_ops mdp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mdp_suspend, mdp_resume)
	SET_RUNTIME_PM_OPS(mdp_pm_suspend, mdp_pm_resume, NULL)
};

static struct platform_driver mdp_driver = {
	.probe		= mdp_probe,
	.remove		= mdp_remove,
	.driver = {
		.name	= MDP_MODULE_NAME,
		.pm	= &mdp_pm_ops,
		.of_match_table = of_match_ptr(mdp_of_ids),
	},
};

module_platform_driver(mdp_driver);

MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_DESCRIPTION("Mediatek image processor 3 driver");
MODULE_LICENSE("GPL v2");

