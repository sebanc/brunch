// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-device.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcodec_fw.h"

module_param(mtk_v4l2_dbg_level, int, 0644);
module_param(mtk_vcodec_dbg, bool, 0644);

static struct of_device_id mtk_vdec_drv_ids[] = {
	{
		.compatible = "mediatek,mtk-vcodec-lat",
		.data = (void *)MTK_VDEC_LAT0,
	},
	{
		.compatible = "mediatek,mtk-vcodec-core",
		.data = (void *)MTK_VDEC_CORE,
	},
	{},
};

static inline int mtk_vdec_compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static inline void mtk_vdec_release_of(struct device *dev, void *data)
{
	of_node_put(data);
}

static inline int mtk_vdec_bind(struct device *dev)
{
	struct mtk_vcodec_dev *data = dev_get_drvdata(dev);

	return component_bind_all(dev, data);
}

static inline void mtk_vdec_unbind(struct device *dev)
{
	struct mtk_vcodec_dev *data = dev_get_drvdata(dev);

	component_unbind_all(dev, data);
}

static const struct component_master_ops mtk_vdec_ops = {
	.bind = mtk_vdec_bind,
	.unbind = mtk_vdec_unbind,
};

static struct component_match *mtk_vdec_match_add(struct mtk_vcodec_dev *vdec_dev)
{
	struct platform_device *pdev = vdec_dev->plat_dev;
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_vdec_drv_ids); i++) {
		struct device_node *comp_node;
		enum mtk_vdec_hw_id comp_idx;
		const struct of_device_id *of_id;

		comp_node = of_find_compatible_node(NULL, NULL,
			mtk_vdec_drv_ids[i].compatible);
		if (!comp_node)
			continue;

		if (!of_device_is_available(comp_node)) {
			of_node_put(comp_node);
			dev_err(&pdev->dev, "Fail to get MMSYS node\n");
			continue;
		}

		of_id = of_match_node(mtk_vdec_drv_ids, comp_node);
		if (!of_id) {
			dev_err(&pdev->dev, "Failed to get match node\n");
			return ERR_PTR(-EINVAL);
		}

		comp_idx = (enum mtk_vdec_hw_id)of_id->data;
		mtk_v4l2_debug(4, "Get component:hw_id(%d),vdec_dev(0x%p),comp_node(0x%p)\n",
			comp_idx, vdec_dev, comp_node);
		vdec_dev->component_node[comp_idx] = comp_node;

		component_match_add_release(&pdev->dev, &match, mtk_vdec_release_of,
			mtk_vdec_compare_of, comp_node);
	}

	return match;
}

static int mtk_vcodec_get_reg_bases(struct mtk_vcodec_dev *dev)
{
	struct platform_device *pdev = dev->plat_dev;
	int reg_num, i, ret = 0;

	/*
	 * Sizeof(u32) * 4 bytes for each register base.
	 */
	reg_num = of_property_count_elems_of_size(pdev->dev.of_node, "reg",
		sizeof(u32) * 4);
	if (!reg_num || reg_num >= NUM_MAX_VDEC_REG_BASE) {
		dev_err(&pdev->dev, "Invalid register property size: %d\n", reg_num);
		return -EINVAL;
	}

	/*
	 * For component device, need to use misc_reg to clear interrupt,
	 * but it is also be used in master device, so store misc_reg to
	 * reg_base[0]. For master register base, start to store its register
	 * bases from reg_base[1].
	 */
	for (i = 0; i < reg_num; i++) {
		dev->reg_base[i + 1] = devm_platform_ioremap_resource(pdev, i);
		if (IS_ERR((__force void *)dev->reg_base[i + 1])) {
			ret = PTR_ERR((__force void *)dev->reg_base[i + 1]);
			break;
		}
	}

	return ret;
}

static int fops_vcodec_open(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = NULL;
	int ret = 0;
	struct vb2_queue *src_vq;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mutex_lock(&dev->dev_mutex);
	ctx->id = dev->id_counter++;
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	INIT_LIST_HEAD(&ctx->list);
	ctx->dev = dev;
	init_waitqueue_head(&ctx->queue);
	if (VDEC_LAT_ARCH(ctx->dev->vdec_pdata->hw_arch))
		init_waitqueue_head(&ctx->core_queue);
	mutex_init(&ctx->lock);

	ctx->type = MTK_INST_DECODER;
	ret = dev->vdec_pdata->ctrls_setup(ctx);
	if (ret) {
		mtk_v4l2_err("Failed to setup mt vcodec controls");
		goto err_ctrls_setup;
	}
	ctx->m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev_dec, ctx,
		&mtk_vcodec_dec_queue_init);
	if (IS_ERR((__force void *)ctx->m2m_ctx)) {
		ret = PTR_ERR((__force void *)ctx->m2m_ctx);
		mtk_v4l2_err("Failed to v4l2_m2m_ctx_init() (%d)",
			ret);
		goto err_m2m_ctx_init;
	}
	src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	ctx->empty_flush_buf.vb.vb2_buf.vb2_queue = src_vq;
	mtk_vcodec_dec_set_default_params(ctx);

	if (v4l2_fh_is_singular(&ctx->fh)) {
		/*
		 * Does nothing if firmware was already loaded.
		 */
		ret = mtk_vcodec_fw_load_firmware(dev->fw_handler);
		if (ret < 0) {
			/*
			 * Return 0 if downloading firmware successfully,
			 * otherwise it is failed
			 */
			mtk_v4l2_err("failed to load firmware!");
			goto err_load_fw;
		}

		dev->dec_capability =
			mtk_vcodec_fw_get_vdec_capa(dev->fw_handler);
		mtk_v4l2_debug(0, "decoder capability %x", dev->dec_capability);
	}

	list_add(&ctx->list, &dev->ctx_list);

	mutex_unlock(&dev->dev_mutex);
	mtk_v4l2_debug(0, "%s decoder [%d]", dev_name(&dev->plat_dev->dev),
			ctx->id);
	return ret;

	/* Deinit when failure occurred */
err_load_fw:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
err_m2m_ctx_init:
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
err_ctrls_setup:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vcodec_release(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	mtk_v4l2_debug(0, "[%d] decoder", ctx->id);
	mutex_lock(&dev->dev_mutex);

	/*
	 * Call v4l2_m2m_ctx_release before mtk_vcodec_dec_release. First, it
	 * makes sure the worker thread is not running after vdec_if_deinit.
	 * Second, the decoder will be flushed and all the buffers will be
	 * returned in stop_streaming.
	 */
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mtk_vcodec_dec_release(ctx);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);

	list_del_init(&ctx->list);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);
	return 0;
}

static const struct v4l2_file_operations mtk_vcodec_fops = {
	.owner		= THIS_MODULE,
	.open		= fops_vcodec_open,
	.release	= fops_vcodec_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static int mtk_vcodec_probe(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev;
	struct video_device *vfd_dec;
	phandle rproc_phandle;
	enum mtk_vcodec_fw_type fw_type;
	struct component_match *match;
	int i, ret;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	INIT_LIST_HEAD(&dev->ctx_list);
	dev->plat_dev = pdev;

	dev->vdec_pdata = of_device_get_match_data(&pdev->dev);
	if (!of_property_read_u32(pdev->dev.of_node, "mediatek,vpu",
				  &rproc_phandle)) {
		fw_type = VPU;
	} else if (!of_property_read_u32(pdev->dev.of_node, "mediatek,scp",
					 &rproc_phandle)) {
		fw_type = SCP;
	} else {
		mtk_v4l2_err("Could not get vdec IPI device");
		return -ENODEV;
	}
	dma_set_max_seg_size(&pdev->dev, UINT_MAX);

	dev->fw_handler = mtk_vcodec_fw_select(dev, fw_type, DECODER);
	if (IS_ERR(dev->fw_handler))
		return PTR_ERR(dev->fw_handler);

	ret = mtk_vcodec_get_reg_bases(dev);
	if (ret)
		goto err_res;

	if (VDEC_LAT_ARCH(dev->vdec_pdata->hw_arch)) {
		init_waitqueue_head(&dev->core_read);
		INIT_LIST_HEAD(&dev->core_queue);
		spin_lock_init(&dev->core_lock);
		dev->kthread_core = kthread_run(vdec_msg_queue_core_thead, dev,
			"mtk-%s", "core");
		dev->num_core = 0;
	}

	if (of_get_property(pdev->dev.of_node, "dma-ranges", NULL))
		dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	for (i = 0; i < MTK_VDEC_HW_MAX; i++)
		mutex_init(&dev->dec_mutex[i]);
	mutex_init(&dev->dev_mutex);
	spin_lock_init(&dev->irqlock);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
		"[/MTK_V4L2_VDEC]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		mtk_v4l2_err("v4l2_device_register err=%d", ret);
		goto err_res;
	}

	init_waitqueue_head(&dev->queue);

	vfd_dec = video_device_alloc();
	if (!vfd_dec) {
		mtk_v4l2_err("Failed to allocate video device");
		ret = -ENOMEM;
		goto err_dec_alloc;
	}
	vfd_dec->fops		= &mtk_vcodec_fops;
	vfd_dec->ioctl_ops	= &mtk_vdec_ioctl_ops;
	vfd_dec->release	= video_device_release;
	vfd_dec->lock		= &dev->dev_mutex;
	vfd_dec->v4l2_dev	= &dev->v4l2_dev;
	vfd_dec->vfl_dir	= VFL_DIR_M2M;
	vfd_dec->device_caps	= V4L2_CAP_VIDEO_M2M_MPLANE |
			V4L2_CAP_STREAMING;

	snprintf(vfd_dec->name, sizeof(vfd_dec->name), "%s",
		MTK_VCODEC_DEC_NAME);
	video_set_drvdata(vfd_dec, dev);
	dev->vfd_dec = vfd_dec;

	dev->m2m_dev_dec = v4l2_m2m_init(&mtk_vdec_m2m_ops);
	if (IS_ERR((__force void *)dev->m2m_dev_dec)) {
		mtk_v4l2_err("Failed to init mem2mem dec device");
		ret = PTR_ERR((__force void *)dev->m2m_dev_dec);
		goto err_dec_mem_init;
	}

	dev->decode_workqueue =
		alloc_ordered_workqueue(MTK_VCODEC_DEC_NAME,
			WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!dev->decode_workqueue) {
		mtk_v4l2_err("Failed to create decode workqueue");
		ret = -EINVAL;
		goto err_event_workq;
	}

	if (dev->vdec_pdata->uses_stateless_api) {
		dev->mdev_dec.dev = &pdev->dev;
		strscpy(dev->mdev_dec.model, MTK_VCODEC_DEC_NAME,
				sizeof(dev->mdev_dec.model));

		media_device_init(&dev->mdev_dec);
		dev->mdev_dec.ops = &mtk_vcodec_media_ops;
		dev->v4l2_dev.mdev = &dev->mdev_dec;

		ret = v4l2_m2m_register_media_controller(dev->m2m_dev_dec,
			dev->vfd_dec, MEDIA_ENT_F_PROC_VIDEO_DECODER);
		if (ret) {
			mtk_v4l2_err("Failed to register media controller");
			goto err_reg_cont;
		}

		ret = media_device_register(&dev->mdev_dec);
		if (ret) {
			mtk_v4l2_err("Failed to register media device");
			goto err_media_reg;
		}

		mtk_v4l2_debug(0, "media registered as /dev/media%d",
			vfd_dec->num);
	}
	ret = video_register_device(vfd_dec, VFL_TYPE_GRABBER, 0);
	if (ret) {
		mtk_v4l2_err("Failed to register video device");
		goto err_dec_reg;
	}

	mtk_v4l2_debug(0, "decoder registered as /dev/video%d",
		vfd_dec->num);

	match = mtk_vdec_match_add(dev);
	if (IS_ERR_OR_NULL(match))
		goto err_component_match;

	platform_set_drvdata(pdev, dev);
	ret = component_master_add_with_match(&pdev->dev, &mtk_vdec_ops, match);
	if (ret < 0)
		goto err_component_match;

	return 0;
err_component_match:
	video_unregister_device(vfd_dec);
err_dec_reg:
	if (dev->vdec_pdata->uses_stateless_api)
		media_device_unregister(&dev->mdev_dec);
err_media_reg:
	if (dev->vdec_pdata->uses_stateless_api)
		v4l2_m2m_unregister_media_controller(dev->m2m_dev_dec);
err_reg_cont:
	destroy_workqueue(dev->decode_workqueue);
err_event_workq:
	v4l2_m2m_release(dev->m2m_dev_dec);
err_dec_mem_init:
	video_device_release(vfd_dec);
err_dec_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_res:
	mtk_vcodec_fw_release(dev->fw_handler);
	return ret;
}

extern const struct mtk_vcodec_dec_pdata mtk_vdec_8173_pdata;
extern const struct mtk_vcodec_dec_pdata mtk_vdec_8183_pdata;
extern const struct mtk_vcodec_dec_pdata mtk_lat_sig_core_pdata;

static const struct of_device_id mtk_vcodec_match[] = {
	{
		.compatible = "mediatek,mt8173-vcodec-dec",
		.data = &mtk_vdec_8173_pdata,
	},
	{
		.compatible = "mediatek,mt8183-vcodec-dec",
		.data = &mtk_vdec_8183_pdata,
	},
	{
		.compatible = "mediatek,mt8192-vcodec-dec",
		.data = &mtk_lat_sig_core_pdata,
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_vcodec_match);

static int mtk_vcodec_dec_remove(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev = platform_get_drvdata(pdev);

	flush_workqueue(dev->decode_workqueue);
	destroy_workqueue(dev->decode_workqueue);

	if (media_devnode_is_registered(dev->mdev_dec.devnode)) {
		media_device_unregister(&dev->mdev_dec);
		v4l2_m2m_unregister_media_controller(dev->m2m_dev_dec);
		media_device_cleanup(&dev->mdev_dec);
	}

	if (dev->m2m_dev_dec)
		v4l2_m2m_release(dev->m2m_dev_dec);

	if (dev->vfd_dec)
		video_unregister_device(dev->vfd_dec);

	v4l2_device_unregister(&dev->v4l2_dev);
	mtk_vcodec_release_dec_pm(dev);
	mtk_vcodec_fw_release(dev->fw_handler);
	return 0;
}

static struct platform_driver mtk_vcodec_dec_driver = {
	.probe	= mtk_vcodec_probe,
	.remove	= mtk_vcodec_dec_remove,
	.driver	= {
		.name	= MTK_VCODEC_DEC_NAME,
		.of_match_table = mtk_vcodec_match,
	},
};

static struct platform_driver * const mtk_vdec_drivers[] = {
	&mtk_vdec_hw_driver,
	&mtk_vcodec_dec_driver,
};

static int __init mtk_vdec_init(void)
{
	return platform_register_drivers(mtk_vdec_drivers,
					 ARRAY_SIZE(mtk_vdec_drivers));
}

static void __exit mtk_vdec_exit(void)
{
	platform_unregister_drivers(mtk_vdec_drivers,
				    ARRAY_SIZE(mtk_vdec_drivers));
}

module_init(mtk_vdec_init);
module_exit(mtk_vdec_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek video codec V4L2 decoder driver");
