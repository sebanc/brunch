/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MDP_PLATFORM_H__
#define __MDP_PLATFORM_H__

#include "mtk-mdp3-comp.h"

/* CAM */
#define MDP_WPEI           MDP_COMP_WPEI
#define MDP_WPEO           MDP_COMP_WPEO
#define MDP_WPEI2          MDP_COMP_WPEI2
#define MDP_WPEO2          MDP_COMP_WPEO2
#define MDP_IMGI           MDP_COMP_ISP_IMGI
#define MDP_IMGO           MDP_COMP_ISP_IMGO
#define MDP_IMG2O          MDP_COMP_ISP_IMG2O

/* IPU */
#define MDP_IPUI           MDP_COMP_NONE
#define MDP_IPUO           MDP_COMP_NONE

/* MDP */
#define MDP_CAMIN          MDP_COMP_CAMIN
#define MDP_CAMIN2         MDP_COMP_CAMIN2
#define MDP_RDMA0          MDP_COMP_RDMA0
#define MDP_RDMA1          MDP_COMP_NONE
#define MDP_AAL0           MDP_COMP_AAL0
#define MDP_CCORR0         MDP_COMP_CCORR0
#define MDP_SCL0           MDP_COMP_RSZ0
#define MDP_SCL1           MDP_COMP_RSZ1
#define MDP_SCL2           MDP_COMP_NONE
#define MDP_TDSHP0         MDP_COMP_TDSHP0
#define MDP_COLOR0         MDP_COMP_COLOR0
#define MDP_WROT0          MDP_COMP_WROT0
#define MDP_WROT1          MDP_COMP_NONE
#define MDP_WDMA           MDP_COMP_WDMA
#define MDP_PATH0_SOUT     MDP_COMP_PATH0_SOUT
#define MDP_PATH1_SOUT     MDP_COMP_PATH1_SOUT

#define MDP_TOTAL          (MDP_COMP_WDMA + 1)

/* Platform options */
#define ESL_SETTING			1
#define RDMA_SUPPORT_10BIT		1
#define RDMA0_RSZ1_SRAM_SHARING		1
#define RDMA_UPSAMPLE_REPEAT_ONLY	1
#define RSZ_DISABLE_DCM_SMALL_TILE	0
#define WROT_FILTER_CONSTRAINT		0
#define WROT0_DISP_SRAM_SHARING		0

#define MM_MUTEX_MOD_OFFSET	0x30
#define MM_MUTEX_SOF_OFFSET	0x2c

#endif  /* __MDP_PLATFORM_H__ */

