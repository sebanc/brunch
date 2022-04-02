/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MDP3_CORE_H__
#define __MTK_MDP3_CORE_H__

#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <linux/soc/mediatek/mtk-mmsys.h>
#include <linux/soc/mediatek/mtk-mutex.h>
#include "mtk-mdp3-regs.h"
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-vpu.h"

#define MDP_MODULE_NAME	"mtk-mdp3"
#define MDP_RDMA0_NODE      0
#define MDP_DUAL_PIPE       2

#define MDP_STAGE_JOB_INC		BIT(0)
#define MDP_STAGE_SUSPEND_CHECK		BIT(1)
#define MDP_STAGE_MUTEX_ON		BIT(2)
#define MDP_STAGE_CLK_ON		BIT(3)
#define MDP_STAGE_CMDQ_FLUSH		BIT(4)
#define MDP_STAGE_CB_START		BIT(5)
#define MDP_STAGE_M2M_JOB_DONE		BIT(6)
#define MDP_STAGE_SEND_CB		BIT(7)
#define MDP_STAGE_CB_DONE		BIT(8)
#define MDP_STAGE_RELEASE_START		BIT(9)
#define MDP_STAGE_MUTEX_OFF		BIT(10)
#define MDP_STAGE_CLK_OFF		BIT(11)
#define MDP_STAGE_FREE_COMP		BIT(12)
#define MDP_STAGE_RELEASE_DONE		BIT(13)

#define MDP_STAGE_ERROR(x) (((x) != (0)) || ((x) != (0x3FFF)))

enum mdp_buffer_usage {
	MDP_BUFFER_USAGE_HW_READ,
	MDP_BUFFER_USAGE_MDP,
	MDP_BUFFER_USAGE_MDP2,
	MDP_BUFFER_USAGE_ISP,
	MDP_BUFFER_USAGE_WPE,
};

enum mdp_cmdq_usage {
	MDP_CMDQ_V4L2,
	MDP_CMDQ_DL,
	MDP_CMDQ_NUM
};

struct mdp_platform_config {
	bool rdma_support_10bit;
	bool rdma_rsz1_sram_sharing;
	bool rdma_upsample_repeat_only;
	bool rdma_support_extend_ufo;
	bool rdma_support_hyfbc;
	bool rdma_support_afbc;
	bool rdma_esl_setting;
	bool rsz_disable_dcm_small_sample;
	bool rsz_etc_control;
	bool tdshp_1_1;
	bool wrot_filter_constraint;
	bool wrot_support_afbc;
	bool wrot_support_10bit;
	bool mdp_version_6885;
	bool mdp_version_8195;
	bool support_dual_pipe;
	u8 tdshp_dyn_contrast_version;
	u32 gce_event_offset;
};

struct mtk_mdp_driver_data {
	const struct mdp_platform_config *mdp_cfg;
	const enum mdp_comp_event *event;
	unsigned int event_len;
	const struct mdp_comp_list *comp_list;
	const struct mdp_comp_data *comp_data;
	unsigned int comp_data_len;
	const struct mdp_comp_info *comp_info;
	unsigned int comp_info_len;
	const struct mdp_pipe_info *pipe_info;
	unsigned int pipe_info_len;
	const struct mdp_format *format;
	unsigned int format_len;
	const enum mdp_mmsys_config_id *config_table;
};

struct mdp_dev {
	struct platform_device			*pdev;
	struct device				*mdp_mmsys;
	struct device				*mdp_mmsys2;
	struct mtk_mutex			*mdp_mutex[MDP_PIPE_MAX];
	struct mtk_mutex			*mdp_mutex2[MDP_PIPE_MAX];
	struct mdp_comp				*comp[MDP_MAX_COMP_COUNT];
	const struct mtk_mdp_driver_data	*mdp_data;
	s32					event[MDP_MAX_EVENT_COUNT];

	struct workqueue_struct			*job_wq;
	struct workqueue_struct			*clock_wq;
	struct mdp_vpu_dev			vpu;
	struct mtk_scp				*scp;
	struct rproc				*rproc_handle;
	/* synchronization protect for accessing vpu working buffer info */
	struct mutex				vpu_lock;
	s32					vpu_count;
	u32					id_count;
	struct ida				mdp_ida;
	struct cmdq_client			*cmdq_clt[MDP_DUAL_PIPE];
	wait_queue_head_t			callback_wq;

	struct v4l2_device			v4l2_dev;
	struct video_device			*m2m_vdev;
	struct v4l2_m2m_dev			*m2m_dev;
	/* synchronization protect for m2m device operation */
	struct mutex				m2m_lock;
	atomic_t				suspended;
	atomic_t				job_count;
	atomic_t				cmdq_count[MDP_CMDQ_NUM];
	struct mdp_framechange_param		prev_image;
	u32	stage_flag[MDP_CMDQ_NUM];
};

struct mdp_pipe_info {
	enum mtk_mdp_pipe_id pipe_id;
	u32 mmsys_id;
	u32 mutex_id;
	u32 sof;
};

int mdp_vpu_get_locked(struct mdp_dev *mdp);
void mdp_vpu_put_locked(struct mdp_dev *mdp);
int mdp_vpu_register(struct mdp_dev *mdp);
void mdp_vpu_unregister(struct mdp_dev *mdp);
void mdp_video_device_release(struct video_device *vdev);

#endif  /* __MTK_MDP3_CORE_H__ */
