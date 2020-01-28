/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MDP3_CORE_H__
#define __MTK_MDP3_CORE_H__

#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include "mtk-mdp3-comp.h"
#include "mtk-mdp3-vpu.h"

#define MDP_MODULE_NAME	"mtk-mdp3"

enum mdp_buffer_usage {
	MDP_BUFFER_USAGE_HW_READ,
	MDP_BUFFER_USAGE_MDP,
	MDP_BUFFER_USAGE_MDP2,
	MDP_BUFFER_USAGE_ISP,
	MDP_BUFFER_USAGE_WPE,
};

struct mdp_dev {
	struct platform_device	*pdev;
	struct mdp_comp		mmsys;
	struct mdp_comp		mm_mutex;
	struct mdp_comp		*comp[MDP_MAX_COMP_COUNT];
	s32			event[MDP_MAX_EVENT_COUNT];

	struct workqueue_struct	*job_wq;
	struct workqueue_struct	*clock_wq;
	struct mdp_vpu_dev	vpu;
	struct platform_device	*vpu_dev;
	struct rproc		*rproc_handle;
	/* synchronization protect for accessing vpu working buffer info */
	struct mutex		vpu_lock;
	s32			vpu_count;
	u32			id_count;
	struct ida		mdp_ida;
	struct cmdq_client	*cmdq_clt;
	wait_queue_head_t	callback_wq;

	struct v4l2_device	v4l2_dev;
	struct video_device	*m2m_vdev;
	struct v4l2_m2m_dev	*m2m_dev;
	/* synchronization protect for m2m device operation */
	struct mutex		m2m_lock;
	atomic_t		suspended;
	atomic_t		job_count;
};

int mdp_vpu_get_locked(struct mdp_dev *mdp);
void mdp_vpu_put_locked(struct mdp_dev *mdp);

extern int mtk_mdp_debug;

#define DEBUG
#if defined(DEBUG)

#define mdp_dbg(level, fmt, ...)\
	do {\
		if (mtk_mdp_debug >= (level))\
			pr_info("[MTK-MDP3] %d %s:%d: " fmt "\n",\
				level, __func__, __LINE__, ##__VA_ARGS__);\
	} while (0)

#define mdp_err(fmt, ...)\
	pr_err("[MTK-MDP3][ERR] %s:%d: " fmt "\n", __func__, __LINE__,\
		##__VA_ARGS__)

#else

#define mdp_dbg(level, fmt, ...)	do {} while (0)
#define mdp_err(fmt, ...)		do {} while (0)

#endif

#define mdp_dbg_enter() mdp_dbg(3, "+")
#define mdp_dbg_leave() mdp_dbg(3, "-")

#endif  /* __MTK_MDP3_CORE_H__ */

