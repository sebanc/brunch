// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/freezer.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include "mtk-mdp3-cmdq.h"
#include "mtk_dip-dev.h"
#include "mtk_dip-hw.h"

int mtk_dip_hw_working_buf_pool_init(struct mtk_dip_dev *dip_dev)
{
	int i;
	const int working_buf_size = round_up(DIP_FRM_SZ, PAGE_SIZE);
	phys_addr_t working_buf_paddr;

	INIT_LIST_HEAD(&dip_dev->dip_freebufferlist.list);
	spin_lock_init(&dip_dev->dip_freebufferlist.lock);
	dip_dev->dip_freebufferlist.cnt = 0;

	dip_dev->working_buf_mem_size = DIP_SUB_FRM_DATA_NUM *
		working_buf_size;
	dip_dev->working_buf_mem_vaddr =
		dma_alloc_coherent(scp_get_device(dip_dev->scp),
				   dip_dev->working_buf_mem_size,
				   &dip_dev->working_buf_mem_scp_daddr,
				   GFP_KERNEL);
	if (!dip_dev->working_buf_mem_vaddr) {
		dev_err(dip_dev->dev,
			"memory alloc size %d failed\n",
			dip_dev->working_buf_mem_size);
		return -ENOMEM;
	}

	/*
	 * We got the incorrect physical address mapped when
	 * using dma_map_single() so I used dma_map_page_attrs()
	 * directly to workaround here.
	 *
	 * When I use dma_map_single() to map the address, the
	 * physical address retrieved back with iommu_get_domain_for_dev()
	 * and iommu_iova_to_phys() was not equal to the
	 * SCP dma address (it should be the same as the physical address
	 * since we don't have iommu), and was shifted by 0x4000000.
	 */
	working_buf_paddr = dip_dev->working_buf_mem_scp_daddr;

	dip_dev->working_buf_mem_isp_daddr =
		dma_map_resource(dip_dev->dev, working_buf_paddr,
				 dip_dev->working_buf_mem_size,
				 DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(dip_dev->dev,
			      dip_dev->working_buf_mem_isp_daddr)) {
		dev_err(dip_dev->dev,
			"failed to map buffer: s_daddr(%pad)\n",
			&dip_dev->working_buf_mem_scp_daddr);
		dma_free_coherent(scp_get_device(dip_dev->scp),
				  dip_dev->working_buf_mem_size,
				  dip_dev->working_buf_mem_vaddr,
				  dip_dev->working_buf_mem_scp_daddr);

		return -ENOMEM;
	}

	for (i = 0; i < DIP_SUB_FRM_DATA_NUM; i++) {
		struct mtk_dip_hw_subframe *buf = &dip_dev->working_buf[i];
		int offset;

		/*
		 * Total: 0 ~ 72 KB
		 * SubFrame: 0 ~ 16 KB
		 */
		offset = i * working_buf_size;
		buf->buffer.scp_daddr =
			dip_dev->working_buf_mem_scp_daddr + offset;
		buf->buffer.vaddr =
			dip_dev->working_buf_mem_vaddr + offset;
		buf->buffer.isp_daddr =
			dip_dev->working_buf_mem_isp_daddr + offset;
		buf->size = working_buf_size;

		/* Tuning: 16 ~ 48 KB */
		buf->tuning_buf.scp_daddr =
			buf->buffer.scp_daddr + DIP_TUNING_OFFSET;
		buf->tuning_buf.vaddr =
			buf->buffer.vaddr + DIP_TUNING_OFFSET;
		buf->tuning_buf.isp_daddr =
			buf->buffer.isp_daddr + DIP_TUNING_OFFSET;

		/* Config_data: 48 ~ 72 KB */
		buf->config_data.scp_daddr =
			buf->buffer.scp_daddr + DIP_COMP_OFFSET;
		buf->config_data.vaddr = buf->buffer.vaddr + DIP_COMP_OFFSET;

		/* Frame parameters: 72 ~ 76 KB */
		buf->frameparam.scp_daddr =
			buf->buffer.scp_daddr + DIP_FRAMEPARAM_OFFSET;
		buf->frameparam.vaddr =
			buf->buffer.vaddr + DIP_FRAMEPARAM_OFFSET;

		list_add_tail(&buf->list_entry,
			      &dip_dev->dip_freebufferlist.list);
		dip_dev->dip_freebufferlist.cnt++;
	}

	return 0;
}

void mtk_dip_hw_working_buf_pool_release(struct mtk_dip_dev *dip_dev)
{
	/* All the buffer should be in the freebufferlist when release */
	dma_unmap_resource(dip_dev->dev,
			   dip_dev->working_buf_mem_isp_daddr,
			   dip_dev->working_buf_mem_size, DMA_BIDIRECTIONAL,
			   DMA_ATTR_SKIP_CPU_SYNC);

	dma_free_coherent(scp_get_device(dip_dev->scp),
			  dip_dev->working_buf_mem_size,
			  dip_dev->working_buf_mem_vaddr,
			  dip_dev->working_buf_mem_scp_daddr);
}

static void mtk_dip_hw_working_buf_free(struct mtk_dip_dev *dip_dev,
					struct mtk_dip_hw_subframe *working_buf)
{
	unsigned long flags;

	if (!working_buf)
		return;

	spin_lock_irqsave(&dip_dev->dip_freebufferlist.lock, flags);
	list_add_tail(&working_buf->list_entry,
		      &dip_dev->dip_freebufferlist.list);
	dip_dev->dip_freebufferlist.cnt++;
	spin_unlock_irqrestore(&dip_dev->dip_freebufferlist.lock, flags);
}

static struct mtk_dip_hw_subframe*
mtk_dip_hw_working_buf_alloc(struct mtk_dip_dev *dip_dev)
{
	struct mtk_dip_hw_subframe *working_buf;
	unsigned long flags;

	spin_lock_irqsave(&dip_dev->dip_freebufferlist.lock, flags);
	if (list_empty(&dip_dev->dip_freebufferlist.list)) {
		spin_unlock_irqrestore(&dip_dev->dip_freebufferlist.lock,
				       flags);
		return NULL;
	}

	working_buf = list_first_entry(&dip_dev->dip_freebufferlist.list,
				       struct mtk_dip_hw_subframe, list_entry);
	list_del(&working_buf->list_entry);
	dip_dev->dip_freebufferlist.cnt--;
	spin_unlock_irqrestore(&dip_dev->dip_freebufferlist.lock, flags);

	return working_buf;
}

static void mtk_dip_notify(struct mtk_dip_request *req)
{
	struct mtk_dip_dev *dip_dev = req->dip_pipe->dip_dev;
	struct mtk_dip_pipe *pipe = req->dip_pipe;
	struct img_ipi_frameparam *iparam = &req->img_fparam.frameparam;
	enum vb2_buffer_state vbf_state;

	if (iparam->state != FRAME_STATE_HW_TIMEOUT)
		vbf_state = VB2_BUF_STATE_DONE;
	else
		vbf_state = VB2_BUF_STATE_ERROR;

	pm_runtime_mark_last_busy(dip_dev->dev);
	pm_runtime_put_autosuspend(dip_dev->dev);

	/*
	 * The job may be aleady removed by streamoff, so I need to check
	 * it by id here.
	 */
	if (mtk_dip_pipe_get_running_job(pipe, req->id)) {
		mtk_dip_pipe_remove_job(req);
		mtk_dip_pipe_job_finish(req, vbf_state);
		mtk_dip_hw_working_buf_free(dip_dev, req->working_buf);
		req->working_buf = NULL;
		wake_up(&dip_dev->flushing_waitq);
	}
}

static void mdp_cb_timeout_worker(struct work_struct *work)
{
	struct mtk_dip_request *req = mtk_dip_hw_mdpcb_work_to_req(work);
	struct img_ipi_param ipi_param;

	ipi_param.usage = IMG_IPI_DEBUG;
	scp_ipi_send(req->dip_pipe->dip_dev->scp, SCP_IPI_DIP,
		     &ipi_param, sizeof(ipi_param), 0);
	mtk_dip_notify(req);
}

/* Maybe in IRQ context of cmdq */
static void dip_mdp_cb_func(struct cmdq_cb_data data)
{
	struct mtk_dip_request *req;
	struct mtk_dip_dev *dip_dev;

	if (!data.data) {
		pr_err("%s: data->data is NULL\n",
		       __func__);
		return;
	}

	req = data.data;
	dip_dev = req->dip_pipe->dip_dev;

	dev_dbg(dip_dev->dev, "%s: req(%p), idx(%d), no(%d), s(%d), n_in(%d), n_out(%d)\n",
		__func__, req, req->img_fparam.frameparam.index,
		req->img_fparam.frameparam.frame_no,
		req->img_fparam.frameparam.state,
		req->img_fparam.frameparam.num_inputs,
		req->img_fparam.frameparam.num_outputs);

	if (data.sta != CMDQ_CB_NORMAL) {
		dev_err(dip_dev->dev, "%s: frame no(%d) HW timeout\n",
			__func__, req->img_fparam.frameparam.frame_no);
		req->img_fparam.frameparam.state = FRAME_STATE_HW_TIMEOUT;
		INIT_WORK(&req->mdpcb_work, mdp_cb_timeout_worker);
		queue_work(req->dip_pipe->dip_dev->mdpcb_wq,
			   &req->mdpcb_work);
	} else {
		mtk_dip_notify(req);
	}
}

static void dip_runner_func(struct work_struct *work)
{
	struct mtk_dip_request *req = mtk_dip_hw_mdp_work_to_req(work);
	struct mtk_dip_dev *dip_dev = req->dip_pipe->dip_dev;
	struct img_config *config_data =
		(struct img_config *)req->working_buf->config_data.vaddr;

	/*
	 * Call MDP/GCE API to do HW excecution
	 * Pass the framejob to MDP driver
	 */
	pm_runtime_get_sync(dip_dev->dev);
	mdp_cmdq_sendtask(dip_dev->mdp_pdev, config_data,
			  &req->img_fparam.frameparam, NULL, false,
			  dip_mdp_cb_func, req);
}

static void dip_scp_handler(void *data, unsigned int len, void *priv)
{
	int job_id;
	struct mtk_dip_pipe *pipe;
	int pipe_id;
	struct mtk_dip_request *req;
	struct img_ipi_frameparam *frameparam;
	struct mtk_dip_dev *dip_dev = (struct mtk_dip_dev *)priv;
	struct img_ipi_param *ipi_param;
	u32 num;

	if (WARN_ONCE(!data, "%s: failed due to NULL data\n", __func__))
		return;

	if (WARN_ONCE(len == sizeof(ipi_param),
		      "%s: len(%d) not match ipi_param\n", __func__, len))
		return;

	ipi_param = (struct img_ipi_param *)data;
	if (ipi_param->usage == IMG_IPI_INIT ||
	    ipi_param->usage == IMG_IPI_DEINIT )
		return;

	if (ipi_param->usage != IMG_IPI_FRAME) {
		dev_warn(dip_dev->dev,
			 "%s: received unknown ipi_param, usage(%d)\n",
			 __func__, ipi_param->usage);
		return;
	}

	job_id = ipi_param->frm_param.handle;
	pipe_id = mtk_dip_pipe_get_pipe_from_job_id(job_id);
	pipe = mtk_dip_dev_get_pipe(dip_dev, pipe_id);
	if (!pipe) {
		dev_warn(dip_dev->dev,
			 "%s: get invalid img_ipi_frameparam index(%d) from firmware\n",
			 __func__, job_id);
		return;
	}

	req = mtk_dip_pipe_get_running_job(pipe, job_id);
	if (WARN_ONCE(!req, "%s: frame_no(%d) is lost\n", __func__, job_id))
		return;

	frameparam = req->working_buf->frameparam.vaddr;
	req->img_fparam.frameparam = *frameparam;
	num = atomic_dec_return(&dip_dev->num_composing);
	up(&dip_dev->sem);

	dev_dbg(dip_dev->dev,
		"%s: frame_no(%d) is back, index(%d), composing num(%d)\n",
		__func__, frameparam->frame_no, frameparam->index, num);

	INIT_WORK(&req->mdp_work, dip_runner_func);
	queue_work(dip_dev->mdp_wq, &req->mdp_work);
}

static void dip_composer_workfunc(struct work_struct *work)
{
	struct mtk_dip_request *req = mtk_dip_hw_fw_work_to_req(work);
	struct mtk_dip_dev *dip_dev = req->dip_pipe->dip_dev;
	struct img_ipi_param ipi_param;
	struct mtk_dip_hw_subframe *buf;
	int ret;

	down(&dip_dev->sem);

	buf = mtk_dip_hw_working_buf_alloc(req->dip_pipe->dip_dev);
	if (!buf) {
		dev_err(req->dip_pipe->dip_dev->dev,
			"%s:%s:req(%p): no free working buffer available\n",
			__func__, req->dip_pipe->desc->name, req);
	}

	req->working_buf = buf;
	mtk_dip_wbuf_to_ipi_img_addr(&req->img_fparam.frameparam.subfrm_data,
				     &buf->buffer);
	memset(buf->buffer.vaddr, 0, DIP_SUB_FRM_SZ);
	mtk_dip_wbuf_to_ipi_img_sw_addr(&req->img_fparam.frameparam.config_data,
					&buf->config_data);
	memset(buf->config_data.vaddr, 0, DIP_COMP_SZ);

	if (!req->img_fparam.frameparam.tuning_data.present) {
		/*
		 * When user enqueued without tuning buffer,
		 * it would use driver internal buffer.
		 */
		dev_dbg(dip_dev->dev,
			"%s: frame_no(%d) has no tuning_data\n",
			__func__, req->img_fparam.frameparam.frame_no);

		mtk_dip_wbuf_to_ipi_tuning_addr
				(&req->img_fparam.frameparam.tuning_data,
				 &buf->tuning_buf);
		memset(buf->tuning_buf.vaddr, 0, DIP_TUNING_SZ);
	}

	mtk_dip_wbuf_to_ipi_img_sw_addr(&req->img_fparam.frameparam.self_data,
					&buf->frameparam);
	memcpy(buf->frameparam.vaddr, &req->img_fparam.frameparam,
	       sizeof(req->img_fparam.frameparam));
	ipi_param.usage = IMG_IPI_FRAME;
	ipi_param.frm_param.handle = req->id;
	ipi_param.frm_param.scp_addr = (u32)buf->frameparam.scp_daddr;

	mutex_lock(&dip_dev->hw_op_lock);
	atomic_inc(&dip_dev->num_composing);
	ret = scp_ipi_send(dip_dev->scp, SCP_IPI_DIP, &ipi_param,
			   sizeof(ipi_param), 0);
	if (ret) {
		dev_err(dip_dev->dev,
			"%s: frame_no(%d) send SCP_IPI_DIP_FRAME failed %d\n",
			__func__, req->img_fparam.frameparam.frame_no, ret);
		mtk_dip_pipe_remove_job(req);
		mtk_dip_pipe_job_finish(req, VB2_BUF_STATE_ERROR);
		mtk_dip_hw_working_buf_free(dip_dev, req->working_buf);
		req->working_buf = NULL;
		wake_up(&dip_dev->flushing_waitq);
	}
	mutex_unlock(&dip_dev->hw_op_lock);

	dev_dbg(dip_dev->dev,
		"%s: frame_no(%d),idx(0x%x), composing num(%d)\n",
		__func__, req->img_fparam.frameparam.frame_no,
		req->img_fparam.frameparam.index,
		atomic_read(&dip_dev->num_composing));
}

static int mtk_dip_hw_flush_pipe_jobs(struct mtk_dip_pipe *pipe)
{
	struct mtk_dip_request *req;
	struct list_head job_list = LIST_HEAD_INIT(job_list);
	int num;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&pipe->job_lock, flags);
	list_splice_init(&pipe->pipe_job_running_list, &job_list);
	pipe->num_jobs = 0;
	spin_unlock_irqrestore(&pipe->job_lock, flags);

	ret = wait_event_freezable_timeout
		(pipe->dip_dev->flushing_waitq,
		 !(num = atomic_read(&pipe->dip_dev->num_composing)),
		 msecs_to_jiffies(1000 / 30 * DIP_COMPOSING_MAX_NUM * 3));
	if (!ret && num) {
		dev_err(pipe->dip_dev->dev,
			"%s: flushing is aborted, num(%d)\n",
			__func__, num);
		return -EINVAL;
	}

	list_for_each_entry(req, &job_list, list)
		mtk_dip_pipe_job_finish(req, VB2_BUF_STATE_ERROR);

	return 0;
}

static int mtk_dip_hw_connect(struct mtk_dip_dev *dip_dev)
{
	int ret;
	struct img_ipi_param ipi_param;

	pm_runtime_get_sync(dip_dev->dev);
	scp_ipi_register(dip_dev->scp, SCP_IPI_DIP, dip_scp_handler, dip_dev);
	memset(&ipi_param, 0, sizeof(ipi_param));
	ipi_param.usage = IMG_IPI_INIT;

	ret = scp_ipi_send(dip_dev->scp, SCP_IPI_DIP, &ipi_param,
			   sizeof(ipi_param), 200);
	if (ret) {
		dev_err(dip_dev->dev, "%s: send SCP_IPI_DIP_FRAME failed %d\n",
			__func__, ret);
		return -EBUSY;
	}
	pm_runtime_mark_last_busy(dip_dev->dev);
	pm_runtime_put_autosuspend(dip_dev->dev);

	return 0;
}

static void mtk_dip_hw_disconnect(struct mtk_dip_dev *dip_dev)
{
	int ret;
	struct img_ipi_param ipi_param;

	ipi_param.usage = IMG_IPI_DEINIT;
	ret = scp_ipi_send(dip_dev->scp, SCP_IPI_DIP, &ipi_param,
			   sizeof(ipi_param), 200);
	if (ret)
		dev_err(dip_dev->dev,
			"%s: SCP IMG_IPI_DEINIT failed(%d)\n", __func__, ret);

	scp_ipi_unregister(dip_dev->scp, SCP_IPI_DIP);
}

int mtk_dip_hw_streamon(struct mtk_dip_pipe *pipe)
{
	struct mtk_dip_dev *dip_dev = pipe->dip_dev;
	int ret;

	mutex_lock(&dip_dev->hw_op_lock);
	if (!dip_dev->dip_stream_cnt) {
		ret = mtk_dip_hw_connect(pipe->dip_dev);
		if (ret) {
			dev_err(pipe->dip_dev->dev,
				"%s:%s: pipe(%d) connect to dip_hw failed\n",
				__func__, pipe->desc->name, pipe->desc->id);

			mutex_unlock(&dip_dev->hw_op_lock);

			return ret;
		}
	}
	dip_dev->dip_stream_cnt++;
	mutex_unlock(&dip_dev->hw_op_lock);

	pipe->streaming = 1;
	mtk_dip_pipe_try_enqueue(pipe);

	return 0;
}

int mtk_dip_hw_streamoff(struct mtk_dip_pipe *pipe)
{
	struct mtk_dip_dev *dip_dev = pipe->dip_dev;
	int ret;

	pipe->streaming = 0;

	ret = mtk_dip_hw_flush_pipe_jobs(pipe);
	if (WARN_ON(ret != 0)) {
		dev_err(dip_dev->dev,
			"%s:%s: mtk_dip_hw_flush_pipe_jobs, ret(%d)\n",
			__func__, pipe->desc->name, ret);
	}

	/* Stop the hardware if there is no streaming pipe */
	mutex_lock(&dip_dev->hw_op_lock);
	dip_dev->dip_stream_cnt--;
	if (!dip_dev->dip_stream_cnt)
		mtk_dip_hw_disconnect(dip_dev);

	mutex_unlock(&dip_dev->hw_op_lock);

	return 0;
}

void mtk_dip_hw_enqueue(struct mtk_dip_dev *dip_dev,
			struct mtk_dip_request *req)
{
	struct img_ipi_frameparam *frameparams = &req->img_fparam.frameparam;

	mtk_dip_pipe_ipi_params_config(req);
	frameparams->state = FRAME_STATE_INIT;
	frameparams->frame_no = atomic_inc_return(&dip_dev->dip_enqueue_cnt);

	dev_dbg(dip_dev->dev,
		"%s: hw job id(%d), frame_no(%d) into worklist\n",
		__func__, frameparams->index, frameparams->frame_no);

	INIT_WORK(&req->fw_work, dip_composer_workfunc);
	queue_work(dip_dev->composer_wq, &req->fw_work);
}
