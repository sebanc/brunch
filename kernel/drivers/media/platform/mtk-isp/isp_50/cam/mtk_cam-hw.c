// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/remoteproc/mtk_scp.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>

#include <media/v4l2-event.h>

#include "mtk_cam.h"
#include "mtk_cam-hw.h"
#include "mtk_cam-regs.h"

#define MTK_ISP_COMPOSER_MEM_SIZE		0x200000
#define MTK_ISP_CQ_BUFFER_COUNT			3
#define MTK_ISP_CQ_ADDRESS_OFFSET		0x640

/*
 *
 * MTK Camera ISP P1 HW supports 3 ISP HW (CAM A/B/C).
 * The T-put capability of CAM B is the maximum (max line buffer: 5376 pixels)
 * For CAM A/C, it only supports max line buffer with 3328 pixels.
 * In current driver, only supports CAM B.
 *
 */
#define MTK_ISP_CAM_ID_B			3
#define MTK_ISP_AUTOSUSPEND_DELAY_MS		66
#define MTK_ISP_IPI_SEND_TIMEOUT		1000
#define MTK_ISP_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)

/* Meta index with non-request */
#define META0	0
#define META1	1

static void isp_tx_frame_worker(struct work_struct *work)
{
	struct mtk_cam_dev_request *req =
		container_of(work, struct mtk_cam_dev_request, frame_work);
	struct mtk_cam_dev *cam =
		container_of(req->req.mdev, struct mtk_cam_dev, media_dev);
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(cam->dev);

	scp_ipi_send(p1_dev->scp, SCP_IPI_ISP_FRAME, &req->frame_params,
		     sizeof(req->frame_params), MTK_ISP_IPI_SEND_TIMEOUT);
}

static void isp_composer_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_isp_p1_device *p1_dev = (struct mtk_isp_p1_device *)priv;
	struct device *dev = p1_dev->dev;
	struct mtk_isp_scp_p1_cmd *ipi_msg;

	ipi_msg = (struct mtk_isp_scp_p1_cmd *)data;

	if (len < offsetofend(struct mtk_isp_scp_p1_cmd, ack_info)) {
		dev_err(dev, "wrong IPI len:%d\n", len);
		return;
	}

	if (ipi_msg->cmd_id != ISP_CMD_ACK ||
	    ipi_msg->ack_info.cmd_id != ISP_CMD_FRAME_ACK)
		return;

	p1_dev->composed_frame_seq_no = ipi_msg->ack_info.frame_seq_no;
	dev_dbg(dev, "ack frame_num:%d\n", p1_dev->composed_frame_seq_no);
}

static int isp_composer_init(struct mtk_isp_p1_device *p1_dev)
{
	struct device *dev = p1_dev->dev;
	int ret;

	ret = scp_ipi_register(p1_dev->scp, SCP_IPI_ISP_CMD,
			       isp_composer_handler, p1_dev);
	if (ret) {
		dev_err(dev, "failed to register IPI cmd\n");
		return ret;
	}
	ret = scp_ipi_register(p1_dev->scp, SCP_IPI_ISP_FRAME,
			       isp_composer_handler, p1_dev);
	if (ret) {
		dev_err(dev, "failed to register IPI frame\n");
		goto unreg_ipi_cmd;
	}

	p1_dev->composer_wq =
		alloc_ordered_workqueue(dev_name(p1_dev->dev),
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE);
	if (!p1_dev->composer_wq) {
		dev_err(dev, "failed to alloc composer workqueue\n");
		goto unreg_ipi_frame;
	}

	return 0;

unreg_ipi_frame:
	scp_ipi_unregister(p1_dev->scp, SCP_IPI_ISP_FRAME);
unreg_ipi_cmd:
	scp_ipi_unregister(p1_dev->scp, SCP_IPI_ISP_CMD);

	return ret;
}

static void isp_composer_uninit(struct mtk_isp_p1_device *p1_dev)
{
	destroy_workqueue(p1_dev->composer_wq);
	scp_ipi_unregister(p1_dev->scp, SCP_IPI_ISP_CMD);
	scp_ipi_unregister(p1_dev->scp, SCP_IPI_ISP_FRAME);
}

static void isp_composer_meta_config(struct mtk_isp_p1_device *p1_dev,
				     unsigned int dma)
{
	struct mtk_isp_scp_p1_cmd composer_tx_cmd;

	memset(&composer_tx_cmd, 0, sizeof(composer_tx_cmd));
	composer_tx_cmd.cmd_id = ISP_CMD_CONFIG_META;
	composer_tx_cmd.enabled_dmas = dma;

	scp_ipi_send(p1_dev->scp, SCP_IPI_ISP_CMD, &composer_tx_cmd,
		     sizeof(composer_tx_cmd), MTK_ISP_IPI_SEND_TIMEOUT);
}

static void isp_composer_hw_init(struct mtk_isp_p1_device *p1_dev)
{
	struct mtk_isp_scp_p1_cmd composer_tx_cmd;

	memset(&composer_tx_cmd, 0, sizeof(composer_tx_cmd));
	composer_tx_cmd.cmd_id = ISP_CMD_INIT;
	composer_tx_cmd.init_param.hw_module = MTK_ISP_CAM_ID_B;

	/*
	 * Passed coherent reserved memory info. for SCP firmware usage.
	 * This buffer is used for SCP's ISP composer to compose.
	 * The size of is fixed to 0x200000 for the requirement of composer.
	 */
	composer_tx_cmd.init_param.cq_addr.iova = p1_dev->composer_iova;
	composer_tx_cmd.init_param.cq_addr.scp_addr = p1_dev->composer_scp_addr;

	scp_ipi_send(p1_dev->scp, SCP_IPI_ISP_CMD, &composer_tx_cmd,
		     sizeof(composer_tx_cmd), MTK_ISP_IPI_SEND_TIMEOUT);
}

static void isp_composer_hw_deinit(struct mtk_isp_p1_device *p1_dev)
{
	struct mtk_isp_scp_p1_cmd composer_tx_cmd;

	memset(&composer_tx_cmd, 0, sizeof(composer_tx_cmd));
	composer_tx_cmd.cmd_id = ISP_CMD_DEINIT;

	scp_ipi_send(p1_dev->scp, SCP_IPI_ISP_CMD, &composer_tx_cmd,
		     sizeof(composer_tx_cmd), MTK_ISP_IPI_SEND_TIMEOUT);

	isp_composer_uninit(p1_dev);
}

void mtk_isp_hw_config(struct mtk_cam_dev *cam,
		       struct p1_config_param *config_param)
{
	struct mtk_isp_scp_p1_cmd composer_tx_cmd;
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(cam->dev);

	memset(&composer_tx_cmd, 0, sizeof(composer_tx_cmd));
	composer_tx_cmd.cmd_id = ISP_CMD_CONFIG;
	memcpy(&composer_tx_cmd.config_param, config_param,
	       sizeof(*config_param));

	scp_ipi_send(p1_dev->scp, SCP_IPI_ISP_CMD, &composer_tx_cmd,
		     sizeof(composer_tx_cmd), MTK_ISP_IPI_SEND_TIMEOUT);
}

void mtk_isp_stream(struct mtk_cam_dev *cam, int on)
{
	struct mtk_isp_scp_p1_cmd composer_tx_cmd;
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(cam->dev);

	memset(&composer_tx_cmd, 0, sizeof(composer_tx_cmd));
	composer_tx_cmd.cmd_id = ISP_CMD_STREAM;
	composer_tx_cmd.is_stream_on = on;

	scp_ipi_send(p1_dev->scp, SCP_IPI_ISP_CMD, &composer_tx_cmd,
		     sizeof(composer_tx_cmd), MTK_ISP_IPI_SEND_TIMEOUT);
}

int mtk_isp_hw_init(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(dev);
	int ret, i;

	ret = rproc_boot(p1_dev->rproc_handle);
	if (ret) {
		dev_err(dev, "failed to rproc_boot\n");
		return ret;
	}

	ret = isp_composer_init(p1_dev);
	if (ret)
		return ret;

	pm_runtime_get_sync(dev);

	isp_composer_hw_init(p1_dev);
	isp_composer_meta_config(p1_dev, cam->enabled_dmas);

	p1_dev->enqueued_frame_seq_no = 0;
	p1_dev->dequeued_frame_seq_no = 0;
	p1_dev->composed_frame_seq_no = 0;
	p1_dev->sof_count = 0;

	for (i = 0; i < ARRAY_SIZE(p1_dev->enqueued_meta_seq_no); i++) {
		p1_dev->enqueued_meta_seq_no[i] = 0;
		p1_dev->dequeued_meta_seq_no[i] = 0;
	}

	dev_dbg(dev, "%s done\n", __func__);

	return 0;
}

int mtk_isp_hw_release(struct mtk_cam_dev *cam)
{
	struct device *dev = cam->dev;
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(dev);

	isp_composer_hw_deinit(p1_dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
	rproc_shutdown(p1_dev->rproc_handle);

	dev_dbg(dev, "%s done\n", __func__);

	return 0;
}

void mtk_isp_enqueue(struct mtk_cam_dev *cam, unsigned int dma_port,
		     struct mtk_cam_dev_buffer *buffer)
{
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(cam->dev);
	struct mtk_isp_scp_p1_cmd cmd_params;
	int idx = buffer->node_id - MTK_CAM_P1_META_OUT_0;

	if (!cam->enabled_dmas) {
		dev_dbg(cam->dev, "Discard meta buf. when dma is disabled\n");
		return;
	}

	buffer->seq_no = ++p1_dev->enqueued_meta_seq_no[idx];

	memset(&cmd_params, 0, sizeof(cmd_params));
	cmd_params.cmd_id = ISP_CMD_ENQUEUE_META;
	cmd_params.meta_frame.enabled_dma = dma_port;
	cmd_params.meta_frame.seq_no = buffer->seq_no;
	cmd_params.meta_frame.meta_addr.iova = buffer->daddr;
	cmd_params.meta_frame.meta_addr.scp_addr = buffer->scp_addr;

	scp_ipi_send(p1_dev->scp, SCP_IPI_ISP_CMD,
		     &cmd_params, sizeof(cmd_params), MTK_ISP_IPI_SEND_TIMEOUT);
}

void mtk_isp_req_enqueue(struct mtk_cam_dev *cam,
			 struct mtk_cam_dev_request *req)
{
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(cam->dev);

	/* Accumulated frame sequence number */
	req->frame_params.frame_seq_no = ++p1_dev->enqueued_frame_seq_no;

	INIT_WORK(&req->frame_work, isp_tx_frame_worker);
	queue_work(p1_dev->composer_wq, &req->frame_work);
	dev_dbg(cam->dev, "enqueue fd:%s frame_seq_no:%d job cnt:%d\n",
		req->req.debug_str, req->frame_params.frame_seq_no,
		cam->running_job_count);
}

static void isp_irq_handle_event(struct mtk_isp_p1_device *p1_dev,
				 unsigned int irq_status,
				 unsigned int dma_status)
{
	if (irq_status & HW_PASS1_DON_ST && dma_status & AAO_DONE_ST)
		mtk_cam_dev_dequeue_frame(&p1_dev->cam_dev,
					  MTK_CAM_P1_META_OUT_0,
					  p1_dev->dequeued_frame_seq_no,
					  p1_dev->dequeued_meta_seq_no[META0]);

	if (dma_status & AFO_DONE_ST)
		mtk_cam_dev_dequeue_frame(&p1_dev->cam_dev,
					  MTK_CAM_P1_META_OUT_1,
					  p1_dev->dequeued_frame_seq_no,
					  p1_dev->dequeued_meta_seq_no[META1]);

	if (irq_status & SW_PASS1_DON_ST) {
		mtk_cam_dev_dequeue_frame(&p1_dev->cam_dev,
					  MTK_CAM_P1_META_OUT_0,
					  p1_dev->dequeued_frame_seq_no,
					  p1_dev->dequeued_meta_seq_no[META0]);
		mtk_cam_dev_dequeue_req_frame(&p1_dev->cam_dev,
					      p1_dev->dequeued_frame_seq_no);
		mtk_cam_dev_req_try_queue(&p1_dev->cam_dev);
	}

	dev_dbg(p1_dev->dev,
		"%s IRQ:0x%x DMA:0x%x seq:%d idx0:%d idx1:%d\n",
		__func__, irq_status, dma_status,
		p1_dev->dequeued_frame_seq_no,
		p1_dev->dequeued_meta_seq_no[META0],
		p1_dev->dequeued_meta_seq_no[META1]);
}

static void isp_irq_handle_sof(struct mtk_isp_p1_device *p1_dev,
			       unsigned int dequeued_frame_seq_no,
			       unsigned int meta0_seq_no,
			       unsigned int meta1_seq_no)
{
	dma_addr_t base_addr = p1_dev->composer_iova;
	struct device *dev = p1_dev->dev;
	struct mtk_cam_dev_request *req;
	int composed_frame_seq_no = p1_dev->composed_frame_seq_no;
	unsigned int addr_offset;

	/* Send V4L2_EVENT_FRAME_SYNC event */
	mtk_cam_dev_event_frame_sync(&p1_dev->cam_dev, dequeued_frame_seq_no);

	p1_dev->sof_count += 1;
	/* Save frame information */
	p1_dev->dequeued_frame_seq_no = dequeued_frame_seq_no;
	p1_dev->dequeued_meta_seq_no[META0] = meta0_seq_no;
	p1_dev->dequeued_meta_seq_no[META1] = meta1_seq_no;

	req = mtk_cam_dev_get_req(&p1_dev->cam_dev, dequeued_frame_seq_no);
	if (req)
		req->timestamp = ktime_get_ns();

	/* Update CQ base address if needed */
	if (composed_frame_seq_no <= dequeued_frame_seq_no) {
		dev_dbg(dev,
			"SOF_INT_ST, no update, cq_num:%d, frame_seq:%d\n",
			composed_frame_seq_no, dequeued_frame_seq_no);
		return;
	}

	addr_offset = MTK_ISP_CQ_ADDRESS_OFFSET *
		(dequeued_frame_seq_no % MTK_ISP_CQ_BUFFER_COUNT);
	writel(base_addr + addr_offset, p1_dev->regs + REG_CQ_THR0_BASEADDR);
	dev_dbg(dev,
		"SOF_INT_ST, update next, cq_num:%d, frame_seq:%d cq_addr:0x%x\n",
		composed_frame_seq_no, dequeued_frame_seq_no, addr_offset);
}

static void isp_irq_handle_dma_err(struct mtk_isp_p1_device *p1_dev)
{
	u32 val;

	dev_err(p1_dev->dev,
		"IMGO:0x%x, RRZO:0x%x, AAO=0x%x, AFO=0x%x, LMVO=0x%x\n",
		readl(p1_dev->regs + REG_IMGO_ERR_STAT),
		readl(p1_dev->regs + REG_RRZO_ERR_STAT),
		readl(p1_dev->regs + REG_AAO_ERR_STAT),
		readl(p1_dev->regs + REG_AFO_ERR_STAT),
		readl(p1_dev->regs + REG_LMVO_ERR_STAT));
	dev_err(p1_dev->dev,
		"LCSO=0x%x, PSO=0x%x, FLKO=0x%x, BPCI:0x%x, LSCI=0x%x\n",
		readl(p1_dev->regs + REG_LCSO_ERR_STAT),
		readl(p1_dev->regs + REG_PSO_ERR_STAT),
		readl(p1_dev->regs + REG_FLKO_ERR_STAT),
		readl(p1_dev->regs + REG_BPCI_ERR_STAT),
		readl(p1_dev->regs + REG_LSCI_ERR_STAT));

	/* Disable DMA error mask to avoid too much error log */
	val = readl(p1_dev->regs + REG_CTL_RAW_INT_EN);
	writel((val & (~DMA_ERR_INT_EN)), p1_dev->regs + REG_CTL_RAW_INT_EN);
	dev_dbg(p1_dev->dev, "disable DMA error mask:0x%x\n", val);
}

static irqreturn_t isp_irq_cam(int irq, void *data)
{
	struct mtk_isp_p1_device *p1_dev = (struct mtk_isp_p1_device *)data;
	struct device *dev = p1_dev->dev;
	unsigned int dequeued_frame_seq_no;
	unsigned int meta0_seq_no, meta1_seq_no;
	unsigned int irq_status, err_status, dma_status;
	unsigned int aao_fbc, afo_fbc;
	unsigned long flags;

	spin_lock_irqsave(&p1_dev->spinlock_irq, flags);
	irq_status = readl(p1_dev->regs + REG_CTL_RAW_INT_STAT);
	err_status = irq_status & INT_ST_MASK_CAM_ERR;
	dma_status = readl(p1_dev->regs + REG_CTL_RAW_INT2_STAT);
	dequeued_frame_seq_no = readl(p1_dev->regs + REG_FRAME_SEQ_NUM);
	meta0_seq_no = readl(p1_dev->regs + REG_META0_SEQ_NUM);
	meta1_seq_no = readl(p1_dev->regs + REG_META1_SEQ_NUM);
	aao_fbc = readl(p1_dev->regs + REG_AAO_FBC_STATUS);
	afo_fbc = readl(p1_dev->regs + REG_AFO_FBC_STATUS);
	spin_unlock_irqrestore(&p1_dev->spinlock_irq, flags);

	/*
	 * In normal case, the next SOF ISR should come after HW PASS1 DONE ISR.
	 * If these two ISRs come together, print warning msg to hint.
	 */
	if ((irq_status & SOF_INT_ST) && (irq_status & HW_PASS1_DON_ST))
		dev_warn(dev, "sof_done block cnt:%d\n", p1_dev->sof_count);

	/* De-queue frame */
	isp_irq_handle_event(p1_dev, irq_status, dma_status);

	/* Save frame info. & update CQ address for frame HW en-queue */
	if (irq_status & SOF_INT_ST)
		isp_irq_handle_sof(p1_dev, dequeued_frame_seq_no,
				   meta0_seq_no, meta1_seq_no);

	/* Check ISP error status */
	if (err_status) {
		dev_err(dev, "int_err:0x%x 0x%x\n", irq_status, err_status);
		/* Show DMA errors in detail */
		if (err_status & DMA_ERR_ST)
			isp_irq_handle_dma_err(p1_dev);
	}

	dev_dbg(dev, "SOF:%d irq:0x%x, dma:0x%x, frame_num:%d, fbc1:0x%x , fbc2:0x%x",
		p1_dev->sof_count, irq_status, dma_status,
		dequeued_frame_seq_no, aao_fbc, afo_fbc);

	return IRQ_HANDLED;
}

static int isp_setup_scp_rproc(struct mtk_isp_p1_device *p1_dev,
			       struct platform_device *pdev)
{
	struct device *dev = p1_dev->dev;
	dma_addr_t addr;
	void *ptr;
	int ret;

	p1_dev->scp = scp_get(pdev);
	if (!p1_dev->scp) {
		dev_err(dev, "failed to get scp device\n");
		return -ENODEV;
	}

	p1_dev->rproc_handle = scp_get_rproc(p1_dev->scp);
	dev_dbg(dev, "p1 rproc_phandle: 0x%pK\n", p1_dev->rproc_handle);
	p1_dev->cam_dev.smem_dev = scp_get_device(p1_dev->scp);

	/*
	 * Allocate coherent reserved memory for SCP firmware usage.
	 * The size of SCP composer's memory is fixed to 0x200000
	 * for the requirement of firmware.
	 */
	ptr = dma_alloc_coherent(p1_dev->cam_dev.smem_dev,
				 MTK_ISP_COMPOSER_MEM_SIZE, &addr, GFP_KERNEL);
	if (!ptr) {
		ret = -ENOMEM;
		goto fail_put_scp;
	}

	p1_dev->composer_scp_addr = addr;
	p1_dev->composer_virt_addr = ptr;
	dev_dbg(dev, "scp addr:%pad va:%pK\n", &addr, ptr);

	/*
	 * This reserved memory is also be used by ISP P1 HW.
	 * Need to get iova address for ISP P1 DMA.
	 */
	addr = dma_map_resource(dev, addr, MTK_ISP_COMPOSER_MEM_SIZE,
				DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(dev, addr)) {
		dev_err(dev, "failed to map scp iova\n");
		ret = -ENOMEM;
		goto fail_free_mem;
	}
	p1_dev->composer_iova = addr;
	dev_dbg(dev, "scp iova addr:%pad\n", &addr);

	return 0;

fail_free_mem:
	dma_free_coherent(p1_dev->cam_dev.smem_dev, MTK_ISP_COMPOSER_MEM_SIZE,
			  p1_dev->composer_virt_addr,
			  p1_dev->composer_scp_addr);
	p1_dev->composer_scp_addr = 0;
fail_put_scp:
	scp_put(p1_dev->scp);

	return ret;
}

static void isp_teardown_scp_rproc(struct mtk_isp_p1_device *p1_dev)
{
	dma_free_coherent(p1_dev->cam_dev.smem_dev, MTK_ISP_COMPOSER_MEM_SIZE,
			  p1_dev->composer_virt_addr,
			  p1_dev->composer_scp_addr);
	p1_dev->composer_scp_addr = 0;
	scp_put(p1_dev->scp);
}

static int mtk_isp_pm_suspend(struct device *dev)
{
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle if possible */
	dev_dbg(dev, "cam suspend, disable VF\n");
	val = readl(p1_dev->regs + REG_TG_VF_CON);
	writel(val & (~TG_VF_CON_VFDATA_EN), p1_dev->regs + REG_TG_VF_CON);
	readl_poll_timeout_atomic(p1_dev->regs + REG_TG_INTER_ST, val,
				  (val & TG_CS_MASK) == TG_IDLE_ST,
				  USEC_PER_MSEC, MTK_ISP_STOP_HW_TIMEOUT);

	/* Disable CMOS */
	val = readl(p1_dev->regs + REG_TG_SEN_MODE);
	writel(val & (~TG_SEN_MODE_CMOS_EN), p1_dev->regs + REG_TG_SEN_MODE);

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev);
	if (ret) {
		dev_err(dev, "failed to force suspend:%d\n", ret);
		goto reenable_hw;
	}

	return 0;

reenable_hw:
	val = readl(p1_dev->regs + REG_TG_SEN_MODE);
	writel(val | TG_SEN_MODE_CMOS_EN, p1_dev->regs + REG_TG_SEN_MODE);
	val = readl(p1_dev->regs + REG_TG_VF_CON);
	writel(val | TG_VF_CON_VFDATA_EN, p1_dev->regs + REG_TG_VF_CON);

	return ret;
}

static int mtk_isp_pm_resume(struct device *dev)
{
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	/* Enable CMOS */
	dev_dbg(dev, "cam resume, enable CMOS/VF\n");
	val = readl(p1_dev->regs + REG_TG_SEN_MODE);
	writel(val | TG_SEN_MODE_CMOS_EN, p1_dev->regs + REG_TG_SEN_MODE);

	/* Enable VF */
	val = readl(p1_dev->regs + REG_TG_VF_CON);
	writel(val | TG_VF_CON_VFDATA_EN, p1_dev->regs + REG_TG_VF_CON);

	return 0;
}

static int mtk_isp_runtime_suspend(struct device *dev)
{
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(dev);

	dev_dbg(dev, "%s:disable clock\n", __func__);
	clk_bulk_disable_unprepare(p1_dev->num_clks, p1_dev->clks);

	return 0;
}

static int mtk_isp_runtime_resume(struct device *dev)
{
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "%s:enable clock\n", __func__);
	ret = clk_bulk_prepare_enable(p1_dev->num_clks, p1_dev->clks);
	if (ret) {
		dev_err(dev, "failed to enable clock:%d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_isp_probe(struct platform_device *pdev)
{
	/* List of clocks required by isp cam */
	static const char * const clk_names[] = {
		"camsys_cam_cgpdn", "camsys_camtg_cgpdn"
	};
	struct mtk_isp_p1_device *p1_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, ret, i;

	p1_dev = devm_kzalloc(dev, sizeof(*p1_dev), GFP_KERNEL);
	if (!p1_dev)
		return -ENOMEM;

	p1_dev->dev = dev;
	dev_set_drvdata(dev, p1_dev);

	/*
	 * Now only support single CAM with CAM B.
	 * Get CAM B register base with CAM B index.
	 * Support multiple CAMs in future.
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, MTK_ISP_CAM_ID_B);
	p1_dev->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(p1_dev->regs)) {
		dev_err(dev, "failed to map reister base\n");
		return PTR_ERR(p1_dev->regs);
	}
	dev_dbg(dev, "cam, map_addr=0x%pK\n", p1_dev->regs);

	/*
	 * The cam_sys unit only supports reg., but has no IRQ support.
	 * The reg. & IRQ index is shifted with 1 for CAM B in DTS.
	 */
	irq = platform_get_irq(pdev, MTK_ISP_CAM_ID_B - 1);
	if (!irq) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(dev, irq, isp_irq_cam, 0, dev_name(dev),
			       p1_dev);
	if (ret) {
		dev_err(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);
	spin_lock_init(&p1_dev->spinlock_irq);

	p1_dev->num_clks = ARRAY_SIZE(clk_names);
	p1_dev->clks = devm_kcalloc(dev, p1_dev->num_clks,
				    sizeof(*p1_dev->clks), GFP_KERNEL);
	if (!p1_dev->clks)
		return -ENOMEM;

	for (i = 0; i < p1_dev->num_clks; ++i)
		p1_dev->clks[i].id = clk_names[i];

	ret = devm_clk_bulk_get(dev, p1_dev->num_clks, p1_dev->clks);
	if (ret) {
		dev_err(dev, "failed to get isp cam clock:%d\n", ret);
		return ret;
	}

	ret = isp_setup_scp_rproc(p1_dev, pdev);
	if (ret)
		return ret;

	pm_runtime_set_autosuspend_delay(dev, MTK_ISP_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);

	/* Initialize the v4l2 common part */
	ret = mtk_cam_dev_init(pdev, &p1_dev->cam_dev);
	if (ret) {
		isp_teardown_scp_rproc(p1_dev);
		return ret;
	}

	return 0;
}

static int mtk_isp_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_isp_p1_device *p1_dev = dev_get_drvdata(dev);

	mtk_cam_dev_cleanup(&p1_dev->cam_dev);
	pm_runtime_dont_use_autosuspend(dev);
	pm_runtime_disable(dev);
	dma_unmap_page_attrs(dev, p1_dev->composer_iova,
			     MTK_ISP_COMPOSER_MEM_SIZE, DMA_BIDIRECTIONAL,
			     DMA_ATTR_SKIP_CPU_SYNC);
	isp_teardown_scp_rproc(p1_dev);

	return 0;
}

static const struct dev_pm_ops mtk_isp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_isp_pm_suspend, mtk_isp_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_isp_runtime_suspend, mtk_isp_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_isp_of_ids[] = {
	{.compatible = "mediatek,mt8183-camisp",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_isp_of_ids);

static struct platform_driver mtk_isp_driver = {
	.probe   = mtk_isp_probe,
	.remove  = mtk_isp_remove,
	.driver  = {
		.name  = "mtk-cam-p1",
		.of_match_table = of_match_ptr(mtk_isp_of_ids),
		.pm     = &mtk_isp_pm_ops,
	}
};

module_platform_driver(mtk_isp_driver);

MODULE_DESCRIPTION("Mediatek ISP P1 driver");
MODULE_LICENSE("GPL v2");
