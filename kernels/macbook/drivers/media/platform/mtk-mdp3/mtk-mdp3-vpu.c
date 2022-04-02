// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include "mtk-mdp3-vpu.h"
#include "mtk-mdp3-core.h"

#define MDP_VPU_MESSAGE_TIMEOUT 500U
#define MDP_VPU_PATH_SIZE	0x78000

static inline struct mdp_dev *vpu_to_mdp(struct mdp_vpu_dev *vpu)
{
	return container_of(vpu, struct mdp_dev, vpu);
}

/* The shared VPU memory is used to communicate between scp and ap.
 * It will cost lot of time to allocate such large memory (> 480KB)
 * when every time playback starts which may decrease performance.
 * So this memory only allocated once and will be freed when mdp device released.
 * To avoid occupied memory without using, memory will be allocated in streamon().
 *
 * These memory is tied to the MDP device.
 * Although multi-instances scenario do exist, but there is only one v4l2 device
 * which implies there only exists one work_queue
 * and frame flip from each instance will be processed one by one.
 */
static int mdp_vpu_shared_mem_alloc(struct mdp_vpu_dev *vpu)
{
	if (!vpu->work) {
		vpu->work = dma_alloc_wc(scp_get_device(vpu->scp), vpu->work_size,
					 &vpu->work_addr, GFP_KERNEL);
		if (!vpu->work)
			goto err_return;
	}

	if (!vpu->config) {
		vpu->config = dma_alloc_wc(scp_get_device(vpu->scp), vpu->config_size,
					   &vpu->config_addr, GFP_KERNEL);
		if (!vpu->config)
			goto err_free_work;
	}

	if (!vpu->path) {
		vpu->path = dma_alloc_wc(scp_get_device(vpu->scp), vpu->path_size,
					 &vpu->path_addr, GFP_KERNEL);
		if (!vpu->path)
			goto err_free_config;
	}

	return 0;

err_free_config:
	dma_free_wc(scp_get_device(vpu->scp), vpu->config_size,
		    vpu->config, vpu->config_addr);
	vpu->config = NULL;
err_free_work:
	dma_free_wc(scp_get_device(vpu->scp), vpu->work_size,
		    vpu->work, vpu->work_addr);
	vpu->work = NULL;
err_return:
	return -ENOMEM;
}

void mdp_vpu_shared_mem_free(struct mdp_vpu_dev *vpu)
{
	if (vpu->work && vpu->work_addr)
		dma_free_wc(scp_get_device(vpu->scp), vpu->work_size,
			    vpu->work, vpu->work_addr);

	if (vpu->config && vpu->config_addr)
		dma_free_wc(scp_get_device(vpu->scp), vpu->config_size,
			    vpu->config, vpu->config_addr);

	if (vpu->path && vpu->path_addr)
		dma_free_wc(scp_get_device(vpu->scp), vpu->path_size,
			    vpu->path, vpu->path_addr);
}

static void mdp_vpu_ipi_handle_init_ack(void *data, unsigned int len,
					void *priv)
{
	struct mdp_ipi_init_msg *msg = (struct mdp_ipi_init_msg *)data;
	struct mdp_vpu_dev *vpu =
		(struct mdp_vpu_dev *)(unsigned long)msg->drv_data;

	if (!vpu->work_size)
		vpu->work_size = msg->work_size;

	vpu->status = msg->status;
	complete(&vpu->ipi_acked);
}

static void mdp_vpu_ipi_handle_deinit_ack(void *data, unsigned int len,
					  void *priv)
{
	struct mdp_ipi_deinit_msg *msg = (struct mdp_ipi_deinit_msg *)data;
	struct mdp_vpu_dev *vpu =
		(struct mdp_vpu_dev *)(unsigned long)msg->drv_data;

	vpu->status = msg->status;
	complete(&vpu->ipi_acked);
}

static void mdp_vpu_ipi_handle_frame_ack(void *data, unsigned int len,
					 void *priv)
{
	struct img_sw_addr *addr = (struct img_sw_addr *)data;
	struct img_ipi_frameparam *param =
		(struct img_ipi_frameparam *)(unsigned long)addr->va;
	struct mdp_vpu_ctx *ctx =
		(struct mdp_vpu_ctx *)(unsigned long)param->drv_data;

	if (param->state) {
		struct mdp_dev *mdp = vpu_to_mdp(ctx->vpu_dev);

		dev_err(&mdp->pdev->dev, "VPU MDP failure:%d\n", param->state);
	}
	complete(&ctx->vpu_dev->ipi_acked);
}

int mdp_vpu_register(struct mdp_dev *mdp)
{
	int err;
	struct mtk_scp *scp = mdp->scp;
	struct device *dev = &mdp->pdev->dev;

	err = scp_ipi_register(scp, SCP_IPI_MDP_INIT,
			       mdp_vpu_ipi_handle_init_ack, NULL);
	if (err) {
		dev_err(dev, "scp_ipi_register failed %d\n", err);
		goto err_ipi_init;
	}
	err = scp_ipi_register(scp, SCP_IPI_MDP_DEINIT,
			       mdp_vpu_ipi_handle_deinit_ack, NULL);
	if (err) {
		dev_err(dev, "scp_ipi_register failed %d\n", err);
		goto err_ipi_deinit;
	}
	err = scp_ipi_register(scp, SCP_IPI_MDP_FRAME,
			       mdp_vpu_ipi_handle_frame_ack, NULL);
	if (err) {
		dev_err(dev, "scp_ipi_register failed %d\n", err);
		goto err_ipi_frame;
	}
	return 0;

err_ipi_frame:
	scp_ipi_unregister(scp, SCP_IPI_MDP_DEINIT);
err_ipi_deinit:
	scp_ipi_unregister(scp, SCP_IPI_MDP_INIT);
err_ipi_init:

	return err;
}

void mdp_vpu_unregister(struct mdp_dev *mdp)
{
	scp_ipi_unregister(mdp->scp, SCP_IPI_MDP_INIT);
	scp_ipi_unregister(mdp->scp, SCP_IPI_MDP_DEINIT);
	scp_ipi_unregister(mdp->scp, SCP_IPI_MDP_FRAME);
}

static int mdp_vpu_sendmsg(struct mdp_vpu_dev *vpu, enum scp_ipi_id id,
			   void *buf, unsigned int len)
{
	struct mdp_dev *mdp = vpu_to_mdp(vpu);
	unsigned int t = MDP_VPU_MESSAGE_TIMEOUT;
	int ret;

	if (!vpu->scp) {
		dev_dbg(&mdp->pdev->dev, "vpu scp is NULL");
		return -EINVAL;
	}
	ret = scp_ipi_send(vpu->scp, id, buf, len, 2000);

	if (ret) {
		dev_err(&mdp->pdev->dev, "scp_ipi_send failed %d\n", ret);
		return -EPERM;
	}
	ret = wait_for_completion_timeout(&vpu->ipi_acked,
					  msecs_to_jiffies(t));
	if (!ret)
		ret = -ETIME;
	else if (vpu->status)
		ret = -EINVAL;
	else
		ret = 0;
	return ret;
}

int mdp_vpu_dev_init(struct mdp_vpu_dev *vpu, struct mtk_scp *scp,
		     struct mutex *lock)
{
	struct mdp_ipi_init_msg msg = {
		.drv_data = (unsigned long)vpu,
	};
	struct mdp_dev *mdp = vpu_to_mdp(vpu);
	int err;

	init_completion(&vpu->ipi_acked);
	vpu->scp = scp;
	vpu->lock = lock;
	vpu->work_size = 0;
	err = mdp_vpu_sendmsg(vpu, SCP_IPI_MDP_INIT, &msg, sizeof(msg));
	if (err)
		goto err_work_size;
	/* vpu work_size was set in mdp_vpu_ipi_handle_init_ack */

	vpu->config_size = MDP_DUAL_PIPE * sizeof(struct img_config);
	vpu->path_size = MDP_VPU_PATH_SIZE;
	err = mdp_vpu_shared_mem_alloc(vpu);
	if (err) {
		dev_err(&mdp->pdev->dev, "VPU memory alloc fail!");
		goto err_mem_alloc;
	}

	msg.work_addr = vpu->work_addr;
	msg.work_size = vpu->work_size;
	err = mdp_vpu_sendmsg(vpu, SCP_IPI_MDP_INIT, &msg, sizeof(msg));
	if (err)
		goto err_work_size;

	return 0;

err_work_size:
	switch (vpu->status) {
	case -MDP_IPI_EBUSY:
		err = -EBUSY;
		break;
	case -MDP_IPI_ENOMEM:
		err = -ENOSPC;	/* -ENOMEM */
		break;
	}
	return err;
err_mem_alloc:
	return err;
}

int mdp_vpu_dev_deinit(struct mdp_vpu_dev *vpu)
{
	struct mdp_ipi_deinit_msg msg = {
		.drv_data = (unsigned long)vpu,
		.work_addr = vpu->work_addr,
	};

	return mdp_vpu_sendmsg(vpu, SCP_IPI_MDP_DEINIT, &msg, sizeof(msg));
}

static struct img_config *mdp_config_get(struct mdp_vpu_dev *vpu,
					 enum mdp_config_id id, uint32_t *addr)
{
	struct img_config *config;

	if (id < 0 || id >= MDP_CONFIG_POOL_SIZE)
		return ERR_PTR(-EINVAL);

	config = vpu->config;
	*addr = vpu->config_addr;

	return config;
}

static int mdp_config_put(struct mdp_vpu_dev *vpu,
			  enum mdp_config_id id,
			  const struct img_config *config)
{
	int err = 0;

	if (id < 0 || id >= MDP_CONFIG_POOL_SIZE)
		return -EINVAL;

	return err;
}

int mdp_vpu_ctx_init(struct mdp_vpu_ctx *ctx, struct mdp_vpu_dev *vpu,
		     enum mdp_config_id id)
{
	ctx->config = mdp_config_get(vpu, id, &ctx->inst_addr);
	if (IS_ERR(ctx->config)) {
		int err = PTR_ERR(ctx->config);

		ctx->config = NULL;
		return err;
	}
	ctx->config_id = id;
	ctx->vpu_dev = vpu;
	return 0;
}

int mdp_vpu_ctx_deinit(struct mdp_vpu_ctx *ctx)
{
	int err = mdp_config_put(ctx->vpu_dev, ctx->config_id, ctx->config);

	ctx->config_id = 0;
	ctx->config = NULL;
	ctx->inst_addr = 0;
	return err;
}

int mdp_vpu_process(struct mdp_vpu_ctx *ctx, struct img_ipi_frameparam *param)
{
	struct img_sw_addr addr;

	memset((void *)ctx->vpu_dev->work, 0, ctx->vpu_dev->work_size);

	if (param->frame_change)
		memset((void *)ctx->vpu_dev->path, 0, ctx->vpu_dev->path_size);
	param->self_data.pa = ctx->vpu_dev->path_addr;
	param->self_data.va = (u64)ctx->vpu_dev->path;

	memset(ctx->config, 0, ctx->vpu_dev->config_size);
	param->config_data.va = (u64)ctx->config;
	param->config_data.pa = ctx->inst_addr;
	param->drv_data = (unsigned long)ctx;

	memcpy((void *)ctx->vpu_dev->work, param, sizeof(*param));
	addr.pa = ctx->vpu_dev->work_addr;
	addr.va = (u64)ctx->vpu_dev->work;
	return mdp_vpu_sendmsg(ctx->vpu_dev, SCP_IPI_MDP_FRAME,
		&addr, sizeof(addr));
}
