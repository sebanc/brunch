// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/platform_device.h>
#include "mtk-mdp3-cmdq.h"
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-core.h"
#include "mtk-mdp3-m2m.h"

#include "mdp-platform.h"
#include "mmsys_mutex.h"

#define DISP_MUTEX_MDP_FIRST	(5)
#define DISP_MUTEX_MDP_COUNT	(5)

#define MDP_PATH_MAX_COMPS	IMG_MAX_COMPONENTS

struct mdp_path {
	struct mdp_dev		*mdp_dev;
	struct mdp_comp_ctx	comps[MDP_PATH_MAX_COMPS];
	u32			num_comps;
	const struct img_config	*config;
	const struct img_ipi_frameparam *param;
	const struct v4l2_rect	*composes[IMG_MAX_HW_OUTPUTS];
	struct v4l2_rect	bounds[IMG_MAX_HW_OUTPUTS];
};

#define has_op(ctx, op) \
	(ctx->comp->ops && ctx->comp->ops->op)
#define call_op(ctx, op, ...) \
	(has_op(ctx, op) ? ctx->comp->ops->op(ctx, ##__VA_ARGS__) : 0)

struct mdp_path_subfrm {
	s32	mutex_id;
	u32	mutex_mod;
	s32	sofs[MDP_PATH_MAX_COMPS];
	u32	num_sofs;
};

static bool is_output_disable(const struct img_compparam *param, u32 count)
{
	return (count < param->num_subfrms) ?
		(param->frame.output_disable ||
		param->subfrms[count].tile_disable) :
		true;
}

static int mdp_path_subfrm_require(struct mdp_path_subfrm *subfrm,
				   const struct mdp_path *path,
				   struct mdp_cmd *cmd, u32 count)
{
	const struct img_config *config = path->config;
	const struct mdp_comp_ctx *ctx;
	phys_addr_t mm_mutex = path->mdp_dev->mm_mutex.reg_base;
	s32 mutex_id = -1;
	u32 mutex_sof = 0;
	int mdp_color = 0;
	int index;
	u8 subsys_id = path->mdp_dev->mm_mutex.subsys_id;

	/* Default value */
	memset(subfrm, 0, sizeof(*subfrm));

	for (index = 0; index < config->num_components; index++) {
		ctx = &path->comps[index];
		if (is_output_disable(ctx->param, count))
			continue;
		switch (ctx->comp->id) {
		/**********************************************
		 * Name            MSB LSB
		 * DISP_MUTEX_MOD   23   0
		 *
		 * Specifies which modules are in this mutex.
		 * Every bit denotes a module. Bit definition:
		 *  2 mdp_rdma0
		 *  4 mdp_rsz0
		 *  5 mdp_rsz1
		 *  6 mdp_tdshp
		 *  7 mdp_wrot0
		 *  8 mdp_wdma
		 *  13 mdp_color
		 *  23 mdp_aal
		 *  24 mdp_ccorr
		 **********************************************/
		case MDP_AAL0:
			subfrm->mutex_mod |= 1 << 23;
			break;
		case MDP_CCORR0:
			subfrm->mutex_mod |= 1 << 24;
			break;
		case MDP_COLOR0:
			if (mdp_color)
				subfrm->mutex_mod |= 1 << 13;
			break;
		case MDP_WDMA:
			subfrm->mutex_mod |= 1 << 8;
			subfrm->sofs[subfrm->num_sofs++] = MDP_WDMA;
			break;
		case MDP_WROT0:
			subfrm->mutex_mod |= 1 << 7;
			subfrm->sofs[subfrm->num_sofs++] = MDP_WROT0;
			break;
		case MDP_TDSHP0:
			subfrm->mutex_mod |= 1 << 6;
			subfrm->sofs[subfrm->num_sofs++] = MDP_TDSHP0;
			break;
		case MDP_SCL1:
			subfrm->mutex_mod |= 1 << 5;
			subfrm->sofs[subfrm->num_sofs++] = MDP_SCL1;
			break;
		case MDP_SCL0:
			subfrm->mutex_mod |= 1 << 4;
			subfrm->sofs[subfrm->num_sofs++] = MDP_SCL0;
			break;
		case MDP_RDMA0:
			mutex_id = DISP_MUTEX_MDP_FIRST + 1;
			subfrm->mutex_mod |= 1 << 2;
			subfrm->sofs[subfrm->num_sofs++] = MDP_RDMA0;
			break;
		case MDP_IMGI:
			mutex_id = DISP_MUTEX_MDP_FIRST;
			break;
		case MDP_WPEI:
			mutex_id = DISP_MUTEX_MDP_FIRST + 3;
			break;
		case MDP_WPEI2:
			mutex_id = DISP_MUTEX_MDP_FIRST + 4;
			break;
		default:
			break;
		}
	}

	subfrm->mutex_id = mutex_id;
	if (-1 == mutex_id) {
		mdp_err("No mutex assigned");
		return -EINVAL;
	}

	if (subfrm->mutex_mod) {
		/* Set mutex modules */
		MM_REG_WRITE(cmd, subsys_id, mm_mutex, MM_MUTEX_MOD,
			     subfrm->mutex_mod, 0x07FFFFFF);
		MM_REG_WRITE(cmd, subsys_id, mm_mutex, MM_MUTEX_SOF,
			     mutex_sof, 0x00000007);
	}
	return 0;
}

static int mdp_path_subfrm_run(const struct mdp_path_subfrm *subfrm,
			       const struct mdp_path *path,
			       struct mdp_cmd *cmd)
{
	phys_addr_t mm_mutex = path->mdp_dev->mm_mutex.reg_base;
	s32 mutex_id = subfrm->mutex_id;
	u8 subsys_id = path->mdp_dev->mm_mutex.subsys_id;

	if (-1 == mutex_id) {
		mdp_err("Incorrect mutex id");
		return -EINVAL;
	}

	if (subfrm->mutex_mod) {
		int index;

		/* Wait WROT SRAM shared to DISP RDMA */
		/* Clear SOF event for each engine */
		for (index = 0; index < subfrm->num_sofs; index++) {
			switch (subfrm->sofs[index]) {
			case MDP_RDMA0:
				MM_REG_CLEAR(cmd, RDMA0_SOF);
				break;
			case MDP_TDSHP0:
				MM_REG_CLEAR(cmd, TDSHP0_SOF);
				break;
			case MDP_SCL0:
				MM_REG_CLEAR(cmd, RSZ0_SOF);
				break;
			case MDP_SCL1:
				MM_REG_CLEAR(cmd, RSZ1_SOF);
				break;
			case MDP_WDMA:
				MM_REG_CLEAR(cmd, WDMA0_SOF);
				break;
			case MDP_WROT0:
#if WROT0_DISP_SRAM_SHARING
				MM_REG_WAIT_NO_CLEAR(cmd, WROT0_SRAM_READY);
#endif
				MM_REG_CLEAR(cmd, WROT0_SOF);
				break;
			default:
				break;
			}
		}

		/* Enable the mutex */
		MM_REG_WRITE(cmd, subsys_id, mm_mutex, MM_MUTEX_EN, 0x1,
			     0x00000001);

		/* Wait SOF events and clear mutex modules (optional) */
		for (index = 0; index < subfrm->num_sofs; index++) {
			switch (subfrm->sofs[index]) {
			case MDP_RDMA0:
				MM_REG_WAIT(cmd, RDMA0_SOF);
				break;
			case MDP_TDSHP0:
				MM_REG_WAIT(cmd, TDSHP0_SOF);
				break;
			case MDP_SCL0:
				MM_REG_WAIT(cmd, RSZ0_SOF);
				break;
			case MDP_SCL1:
				MM_REG_WAIT(cmd, RSZ1_SOF);
				break;
			case MDP_WDMA:
				MM_REG_WAIT(cmd, WDMA0_SOF);
				break;
			case MDP_WROT0:
				MM_REG_WAIT(cmd, WROT0_SOF);
				break;
			default:
				break;
			}
		}
	}
	return 0;
}

static int mdp_path_config_subfrm(struct mdp_cmd *cmd, struct mdp_path *path,
				  u32 count)
{
	struct mdp_path_subfrm subfrm;
	const struct img_config *config = path->config;
	const struct img_mmsys_ctrl *ctrl = &config->ctrls[count];
	const struct img_mux *set;
	struct mdp_comp_ctx *ctx;
	phys_addr_t mmsys = path->mdp_dev->mmsys.reg_base;
	int index, ret;
	u8 subsys_id = path->mdp_dev->mmsys.subsys_id;

	/* Acquire components */
	ret = mdp_path_subfrm_require(&subfrm, path, cmd, count);
	if (ret)
		return ret;
	/* Enable mux settings */
	for (index = 0; index < ctrl->num_sets; index++) {
		set = &ctrl->sets[index];
		MM_REG_WRITE_MASK(cmd, subsys_id, mmsys, set->reg, set->value,
				  0xFFFFFFFF);
	}
	/* Config sub-frame information */
	for (index = (config->num_components - 1); index >= 0; index--) {
		ctx = &path->comps[index];
		if (is_output_disable(ctx->param, count))
			continue;
		ret = call_op(ctx, config_subfrm, cmd, count);
		if (ret)
			return ret;
	}
	/* Run components */
	ret = mdp_path_subfrm_run(&subfrm, path, cmd);
	if (ret)
		return ret;
	/* Wait components done */
	for (index = 0; index < config->num_components; index++) {
		ctx = &path->comps[index];
		if (is_output_disable(ctx->param, count))
			continue;
		ret = call_op(ctx, wait_comp_event, cmd);
		if (ret)
			return ret;
	}
	/* Advance to the next sub-frame */
	for (index = 0; index < config->num_components; index++) {
		ctx = &path->comps[index];
		ret = call_op(ctx, advance_subfrm, cmd, count);
		if (ret)
			return ret;
	}
	/* Disable mux settings */
	for (index = 0; index < ctrl->num_sets; index++) {
		set = &ctrl->sets[index];
		MM_REG_WRITE_MASK(cmd, subsys_id, mmsys, set->reg, 0,
				  0xFFFFFFFF);
	}
	return 0;
}

static int mdp_path_config(struct mdp_dev *mdp, struct mdp_cmd *cmd,
			   struct mdp_path *path)
{
	const struct img_config *config = path->config;
	struct mdp_comp_ctx *ctx;
	int index, count, ret;

	for (index = 0; index < config->num_components; index++) {
		ret = mdp_comp_ctx_init(mdp, &path->comps[index],
					&config->components[index],
					path->param);
		if (ret)
			return ret;
	}

	/* Config path frame */
	/* Reset components */
	for (index = 0; index < config->num_components; index++) {
		ctx = &path->comps[index];
		ret = call_op(ctx, init_comp, cmd);
		if (ret)
			return ret;
	}
	/* Config frame mode */
	for (index = 0; index < config->num_components; index++) {
		const struct v4l2_rect *compose =
			path->composes[ctx->param->outputs[0]];

		ctx = &path->comps[index];
		ret = call_op(ctx, config_frame, cmd, compose);
		if (ret)
			return ret;
	}

	/* Config path sub-frames */
	for (count = 0; count < config->num_subfrms; count++) {
		ret = mdp_path_config_subfrm(cmd, path, count);
		if (ret)
			return ret;
	}
	/* Post processing information */
	for (index = 0; index < config->num_components; index++) {
		ctx = &path->comps[index];
		ret = call_op(ctx, post_process, cmd);
		if (ret)
			return ret;
	}
	return 0;
}

static void mdp_auto_release_work(struct work_struct *work)
{
	struct mdp_cmdq_cb_param *cb_param;
	struct mdp_dev *mdp;

	cb_param = container_of(work, struct mdp_cmdq_cb_param,
				auto_release_work);
	mdp = cb_param->mdp;

	if (cb_param->comps && cb_param->num_comps) {
		int i;

		for (i = 0; i < cb_param->num_comps; i++)
			mdp_comp_clock_off(&mdp->pdev->dev,
					   &cb_param->comps[i]);
	}

	kfree(cb_param->comps);
	kfree(cb_param);

	atomic_dec(&mdp->job_count);
	wake_up(&mdp->callback_wq);
}

static void mdp_handle_cmdq_callback(struct cmdq_cb_data data)
{
	struct mdp_cmdq_cb_param *cb_param;
	struct mdp_dev *mdp;

	if (!data.data) {
		mdp_err("%s:no callback data\n", __func__);
		return;
	}

	cb_param = (struct mdp_cmdq_cb_param *)data.data;
	mdp = cb_param->mdp;

	if (cb_param->mdp_ctx)
		mdp_m2m_job_finish(cb_param->mdp_ctx);

	if (cb_param->user_cmdq_cb) {
		struct cmdq_cb_data user_cb_data;

		user_cb_data.sta = data.sta;
		user_cb_data.data = cb_param->user_cb_data;
		cb_param->user_cmdq_cb(user_cb_data);
	}

	cmdq_pkt_destroy(cb_param->pkt);
	INIT_WORK(&cb_param->auto_release_work, mdp_auto_release_work);
	queue_work(mdp->clock_wq, &cb_param->auto_release_work);
}

int mdp_cmdq_send(struct mdp_dev *mdp, struct mdp_cmdq_param *param)
{
	struct mdp_cmd cmd;
	struct mdp_path path;
	int i, ret;

	if (atomic_read(&mdp->suspended))
		return -ECANCELED;

	atomic_inc(&mdp->job_count);

	cmd.pkt = cmdq_pkt_create(mdp->cmdq_clt, SZ_16K);
	if (IS_ERR(cmd.pkt)) {
		atomic_dec(&mdp->job_count);
		wake_up(&mdp->callback_wq);
		return PTR_ERR(cmd.pkt);
	}
	cmd.event = &mdp->event[0];

	path.mdp_dev = mdp;
	path.config = param->config;
	path.param = param->param;
	for (i = 0; i < param->param->num_outputs; i++) {
		path.bounds[i].left = 0;
		path.bounds[i].top = 0;
		path.bounds[i].width =
			param->param->outputs[i].buffer.format.width;
		path.bounds[i].height =
			param->param->outputs[i].buffer.format.height;
		path.composes[i] = param->composes[i] ?
			param->composes[i] : &path.bounds[i];
	}
	ret = mdp_path_config(mdp, &cmd, &path);
	if (ret) {
		atomic_dec(&mdp->job_count);
		wake_up(&mdp->callback_wq);
		return ret;
	}

	// TODO: engine conflict dispatch
	for (i = 0; i < param->config->num_components; i++)
		mdp_comp_clock_on(&mdp->pdev->dev, path.comps[i].comp);

	if (param->wait) {
		ret = cmdq_pkt_flush(cmd.pkt);
		if (param->mdp_ctx)
			mdp_m2m_job_finish(param->mdp_ctx);
		cmdq_pkt_destroy(cmd.pkt);
		for (i = 0; i < param->config->num_components; i++)
			mdp_comp_clock_off(&mdp->pdev->dev, path.comps[i].comp);

		atomic_dec(&mdp->job_count);
		wake_up(&mdp->callback_wq);
	} else {
		struct mdp_cmdq_cb_param *cb_param;
		struct mdp_comp *comps;

		cb_param = kzalloc(sizeof(*cb_param), GFP_KERNEL);
		if (!cb_param)
			return -ENOMEM;
		comps = kcalloc(param->config->num_components, sizeof(*comps),
				GFP_KERNEL);
		if (!comps) {
			kfree(cb_param);
			mdp_err("%s:comps alloc fail!\n", __func__);
			return -ENOMEM;
		}

		for (i = 0; i < param->config->num_components; i++)
			memcpy(&comps[i], path.comps[i].comp,
			       sizeof(struct mdp_comp));
		cb_param->mdp = mdp;
		cb_param->user_cmdq_cb = param->cmdq_cb;
		cb_param->user_cb_data = param->cb_data;
		cb_param->pkt = cmd.pkt;
		cb_param->comps = comps;
		cb_param->num_comps = param->config->num_components;
		cb_param->mdp_ctx = param->mdp_ctx;

		ret = cmdq_pkt_flush_async(cmd.pkt,
					   mdp_handle_cmdq_callback,
					   (void *)cb_param);
		if (ret) {
			mdp_err("%s:cmdq_pkt_flush_async fail!\n", __func__);
			kfree(cb_param);
			kfree(comps);
		}
	}
	return ret;
}

int mdp_cmdq_sendtask(struct platform_device *pdev, struct img_config *config,
		      struct img_ipi_frameparam *param,
		      struct v4l2_rect *compose, unsigned int wait,
		      void (*cmdq_cb)(struct cmdq_cb_data data), void *cb_data)
{
	struct mdp_dev *mdp = platform_get_drvdata(pdev);
	struct mdp_cmdq_param task = {
		.config = config,
		.param = param,
		.composes[0] = compose,
		.wait = wait,
		.cmdq_cb = cmdq_cb,
		.cb_data = cb_data,
	};

	return mdp_cmdq_send(mdp, &task);
}
EXPORT_SYMBOL_GPL(mdp_cmdq_sendtask);

