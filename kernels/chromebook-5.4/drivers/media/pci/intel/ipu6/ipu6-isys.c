// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Intel Corporation

#include <linux/module.h>
#include <media/v4l2-event.h>

#include "ipu.h"
#include "ipu-platform-regs.h"
#include "ipu-trace.h"
#include "ipu-isys.h"
#ifdef CONFIG_VIDEO_INTEL_IPU_TPG
#include "ipu-isys-tpg.h"
#endif
#include "ipu-platform-isys-csi2-reg.h"

const struct ipu_isys_pixelformat ipu_isys_pfmts[] = {
	{V4L2_PIX_FMT_SBGGR12, 16, 12, 0, MEDIA_BUS_FMT_SBGGR12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG12, 16, 12, 0, MEDIA_BUS_FMT_SGBRG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG12, 16, 12, 0, MEDIA_BUS_FMT_SGRBG12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB12, 16, 12, 0, MEDIA_BUS_FMT_SRGGB12_1X12,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR10, 16, 10, 0, MEDIA_BUS_FMT_SBGGR10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGBRG10, 16, 10, 0, MEDIA_BUS_FMT_SGBRG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SGRBG10, 16, 10, 0, MEDIA_BUS_FMT_SGRBG10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SRGGB10, 16, 10, 0, MEDIA_BUS_FMT_SRGGB10_1X10,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW16},
	{V4L2_PIX_FMT_SBGGR8, 8, 8, 0, MEDIA_BUS_FMT_SBGGR8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SGBRG8, 8, 8, 0, MEDIA_BUS_FMT_SGBRG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SGRBG8, 8, 8, 0, MEDIA_BUS_FMT_SGRBG8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{V4L2_PIX_FMT_SRGGB8, 8, 8, 0, MEDIA_BUS_FMT_SRGGB8_1X8,
	 IPU_FW_ISYS_FRAME_FORMAT_RAW8},
	{}
};

struct ipu_trace_block isys_trace_blocks[] = {
	{
		.offset = IPU_TRACE_REG_IS_TRACE_UNIT_BASE,
		.type = IPU_TRACE_BLOCK_TUN,
	},
	{
		.offset = IPU_TRACE_REG_IS_SP_EVQ_BASE,
		.type = IPU_TRACE_BLOCK_TM,
	},
	{
		.offset = IPU_TRACE_REG_IS_SP_GPC_BASE,
		.type = IPU_TRACE_BLOCK_GPC,
	},
	{
		.offset = IPU_TRACE_REG_IS_ISL_GPC_BASE,
		.type = IPU_TRACE_BLOCK_GPC,
	},
	{
		.offset = IPU_TRACE_REG_IS_MMU_GPC_BASE,
		.type = IPU_TRACE_BLOCK_GPC,
	},
	{
		/* Note! this covers all 8 blocks */
		.offset = IPU_TRACE_REG_CSI2_TM_BASE(0),
		.type = IPU_TRACE_CSI2,
	},
	{
		/* Note! this covers all 11 blocks */
		.offset = IPU_TRACE_REG_CSI2_PORT_SIG2SIO_GR_BASE(0),
		.type = IPU_TRACE_SIG2CIOS,
	},
	{
		.offset = IPU_TRACE_REG_IS_GPREG_TRACE_TIMER_RST_N,
		.type = IPU_TRACE_TIMER_RST,
	},
	{
		.type = IPU_TRACE_BLOCK_END,
	}
};

void isys_setup_hw(struct ipu_isys *isys)
{
	void __iomem *base = isys->pdata->base;
	const u8 *thd = isys->pdata->ipdata->hw_variant.cdc_fifo_threshold;
	u32 irqs = 0;
	unsigned int i, nr;

	nr = (ipu_ver == IPU_VER_6 || ipu_ver == IPU_VER_6EP) ?
		IPU6_ISYS_CSI_PORT_NUM : IPU6SE_ISYS_CSI_PORT_NUM;

	/* Enable irqs for all MIPI ports */
	for (i = 0; i < nr; i++)
		irqs |= IPU_ISYS_UNISPART_IRQ_CSI2(i);

	writel(irqs, base + IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_EDGE);
	writel(irqs, base + IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_LEVEL_NOT_PULSE);
	writel(0xffffffff, base + IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_CLEAR);
	writel(irqs, base + IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_MASK);
	writel(irqs, base + IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_ENABLE);

	irqs = ISYS_UNISPART_IRQS;
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_EDGE);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_LEVEL_NOT_PULSE);
	writel(0xffffffff, base + IPU_REG_ISYS_UNISPART_IRQ_CLEAR);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_MASK);
	writel(irqs, base + IPU_REG_ISYS_UNISPART_IRQ_ENABLE);

	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);
	writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_MUX_REG);

	/* Write CDC FIFO threshold values for isys */
	for (i = 0; i < isys->pdata->ipdata->hw_variant.cdc_fifos; i++)
		writel(thd[i], base + IPU_REG_ISYS_CDC_THRESHOLD(i));
}

irqreturn_t isys_isr(struct ipu_bus_device *adev)
{
	struct ipu_isys *isys = ipu_bus_get_drvdata(adev);
	void __iomem *base = isys->pdata->base;
	u32 status_sw, status_csi;

	spin_lock(&isys->power_lock);
	if (!isys->power) {
		spin_unlock(&isys->power_lock);
		return IRQ_NONE;
	}

	status_csi = readl(isys->pdata->base +
			   IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_STATUS);
	status_sw = readl(isys->pdata->base + IPU_REG_ISYS_UNISPART_IRQ_STATUS);

	writel(ISYS_UNISPART_IRQS & ~IPU_ISYS_UNISPART_IRQ_SW,
	       base + IPU_REG_ISYS_UNISPART_IRQ_MASK);

	do {
		writel(status_csi, isys->pdata->base +
			   IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_CLEAR);
		writel(status_sw, isys->pdata->base +
			   IPU_REG_ISYS_UNISPART_IRQ_CLEAR);

		if (isys->isr_csi2_bits & status_csi) {
			unsigned int i;

			for (i = 0; i < isys->pdata->ipdata->csi2.nports; i++) {
				/* irq from not enabled port */
				if (!isys->csi2[i].base)
					continue;
				if (status_csi & IPU_ISYS_UNISPART_IRQ_CSI2(i))
					ipu_isys_csi2_isr(&isys->csi2[i]);
			}
		}

		writel(0, base + IPU_REG_ISYS_UNISPART_SW_IRQ_REG);

		if (!isys_isr_one(adev))
			status_sw = IPU_ISYS_UNISPART_IRQ_SW;
		else
			status_sw = 0;

		status_csi = readl(isys->pdata->base +
				       IPU_REG_ISYS_CSI_TOP_CTRL0_IRQ_STATUS);
		status_sw |= readl(isys->pdata->base +
				       IPU_REG_ISYS_UNISPART_IRQ_STATUS);
	} while (((status_csi & isys->isr_csi2_bits) ||
		  (status_sw & IPU_ISYS_UNISPART_IRQ_SW)) &&
		 !isys->adev->isp->flr_done);

	writel(ISYS_UNISPART_IRQS, base + IPU_REG_ISYS_UNISPART_IRQ_MASK);

	spin_unlock(&isys->power_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_VIDEO_INTEL_IPU_TPG
void ipu_isys_tpg_sof_event(struct ipu_isys_tpg *tpg)
{
	struct ipu_isys_pipeline *ip = NULL;
	struct v4l2_event ev = {
		.type = V4L2_EVENT_FRAME_SYNC,
	};
	struct video_device *vdev = tpg->asd.sd.devnode;
	unsigned long flags;
	unsigned int i, nr;

	nr = (ipu_ver == IPU_VER_6 || ipu_ver == IPU_VER_6EP) ?
		IPU6_ISYS_CSI_PORT_NUM : IPU6SE_ISYS_CSI_PORT_NUM;

	spin_lock_irqsave(&tpg->isys->lock, flags);
	for (i = 0; i < nr; i++) {
		if (tpg->isys->pipes[i] && tpg->isys->pipes[i]->tpg == tpg) {
			ip = tpg->isys->pipes[i];
			break;
		}
	}

	/* Pipe already vanished */
	if (!ip) {
		spin_unlock_irqrestore(&tpg->isys->lock, flags);
		return;
	}

	ev.u.frame_sync.frame_sequence =
		atomic_inc_return(&ip->sequence) - 1;
	spin_unlock_irqrestore(&tpg->isys->lock, flags);

	v4l2_event_queue(vdev, &ev);

	dev_dbg(&tpg->isys->adev->dev,
		"sof_event::tpg-%i sequence: %i\n",
		tpg->index, ev.u.frame_sync.frame_sequence);
}

void ipu_isys_tpg_eof_event(struct ipu_isys_tpg *tpg)
{
	struct ipu_isys_pipeline *ip = NULL;
	unsigned long flags;
	unsigned int i, nr;
	u32 frame_sequence;

	nr = (ipu_ver == IPU_VER_6 || ipu_ver == IPU_VER_6EP) ?
		IPU6_ISYS_CSI_PORT_NUM : IPU6SE_ISYS_CSI_PORT_NUM;

	spin_lock_irqsave(&tpg->isys->lock, flags);
	for (i = 0; i < nr; i++) {
		if (tpg->isys->pipes[i] && tpg->isys->pipes[i]->tpg == tpg) {
			ip = tpg->isys->pipes[i];
			break;
		}
	}

	/* Pipe already vanished */
	if (!ip) {
		spin_unlock_irqrestore(&tpg->isys->lock, flags);
		return;
	}

	frame_sequence = atomic_read(&ip->sequence);

	spin_unlock_irqrestore(&tpg->isys->lock, flags);

	dev_dbg(&tpg->isys->adev->dev,
		"eof_event::tpg-%i sequence: %i\n",
		tpg->index, frame_sequence);
}

int tpg_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ipu_isys_tpg *tpg = to_ipu_isys_tpg(sd);
	__u32 code = tpg->asd.ffmt[TPG_PAD_SOURCE].code;
	unsigned int bpp = ipu_isys_mbus_code_to_bpp(code);
	struct ipu_isys_pipeline *ip =
			to_ipu_isys_pipeline(sd->entity.pipe);

	/*
	 * MIPI_GEN block is CSI2 FB. Need to enable/disable TPG selection
	 * register to control the TPG streaming.
	 */
	if (tpg->sel)
		writel(enable ? 1 : 0, tpg->sel);

	if (!enable) {
		ip->tpg = NULL;
		writel(0, tpg->base +
		       CSI_REG_CSI_FE_ENABLE -
		       CSI_REG_PIXGEN_COM_BASE_OFFSET);
		writel(CSI_SENSOR_INPUT, tpg->base +
		       CSI_REG_CSI_FE_MUX_CTRL -
		       CSI_REG_PIXGEN_COM_BASE_OFFSET);
		writel(CSI_CNTR_SENSOR_LINE_ID |
		       CSI_CNTR_SENSOR_FRAME_ID,
		       tpg->base + CSI_REG_CSI_FE_SYNC_CNTR_SEL -
		       CSI_REG_PIXGEN_COM_BASE_OFFSET);
		writel(0, tpg->base + MIPI_GEN_REG_COM_ENABLE);
		return 0;
	}

	ip->has_sof = true;
	ip->tpg = tpg;
	/* Select MIPI GEN as input */
	writel(0, tpg->base + CSI_REG_CSI_FE_MODE -
	       CSI_REG_PIXGEN_COM_BASE_OFFSET);
	writel(1, tpg->base + CSI_REG_CSI_FE_ENABLE -
	       CSI_REG_PIXGEN_COM_BASE_OFFSET);
	writel(CSI_MIPIGEN_INPUT, tpg->base +
	       CSI_REG_CSI_FE_MUX_CTRL - CSI_REG_PIXGEN_COM_BASE_OFFSET);
	writel(0, tpg->base + CSI_REG_CSI_FE_SYNC_CNTR_SEL -
	       CSI_REG_PIXGEN_COM_BASE_OFFSET);

	writel(MIPI_GEN_COM_DTYPE_RAW(bpp),
	       tpg->base + MIPI_GEN_REG_COM_DTYPE);
	writel(ipu_isys_mbus_code_to_mipi(code),
	       tpg->base + MIPI_GEN_REG_COM_VTYPE);
	writel(0, tpg->base + MIPI_GEN_REG_COM_VCHAN);

	writel(0, tpg->base + MIPI_GEN_REG_SYNG_NOF_FRAMES);

	writel(DIV_ROUND_UP(tpg->asd.ffmt[TPG_PAD_SOURCE].width *
			    bpp, BITS_PER_BYTE),
	       tpg->base + MIPI_GEN_REG_COM_WCOUNT);
	writel(DIV_ROUND_UP(tpg->asd.ffmt[TPG_PAD_SOURCE].width,
			    MIPI_GEN_PPC),
	       tpg->base + MIPI_GEN_REG_SYNG_NOF_PIXELS);
	writel(tpg->asd.ffmt[TPG_PAD_SOURCE].height,
	       tpg->base + MIPI_GEN_REG_SYNG_NOF_LINES);

	writel(0, tpg->base + MIPI_GEN_REG_TPG_MODE);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_HCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_VCNT_MASK);
	writel(-1, tpg->base + MIPI_GEN_REG_TPG_XYCNT_MASK);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_HCNT_DELTA);
	writel(0, tpg->base + MIPI_GEN_REG_TPG_VCNT_DELTA);

	v4l2_ctrl_handler_setup(&tpg->asd.ctrl_handler);

	writel(2, tpg->base + MIPI_GEN_REG_COM_ENABLE);
	return 0;
}
#endif
