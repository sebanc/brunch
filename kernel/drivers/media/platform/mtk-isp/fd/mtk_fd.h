/* SPDX-License-Identifier: GPL-2.0 */
//
// Copyright (c) 2018 MediaTek Inc.

#ifndef __MTK_FD_HW_H__
#define __MTK_FD_HW_H__

#include <linux/completion.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <linux/mtk-fd-v4l2-controls.h>

#define MTK_FD_OUTPUT_MIN_WIDTH			26U
#define MTK_FD_OUTPUT_MIN_HEIGHT		26U
#define MTK_FD_OUTPUT_MAX_WIDTH			640U
#define MTK_FD_OUTPUT_MAX_HEIGHT		480U
#define MTK_FD_IPI_SEND_TIMEOUT			0U

#define MTK_FD_HW_FMT_VYUY			2
#define MTK_FD_HW_FMT_UYVY			3
#define MTK_FD_HW_FMT_YVYU			4
#define MTK_FD_HW_FMT_YUYV			5
#define MTK_FD_HW_FMT_YVU_2P			6
#define MTK_FD_HW_FMT_YUV_2P			7
#define MTK_FD_HW_FMT_UNKNOWN			8

#define MTK_FD_IPI_CMD_INIT			0
#define MTK_FD_IPI_CMD_INIT_ACK			1
#define MTK_FD_IPI_CMD_ENQUEUE			2
#define MTK_FD_IPI_CMD_ENQ_ACK			3
#define MTK_FD_IPI_CMD_RESET			4
#define MTK_FD_IPI_CMD_RESET_ACK		5

#define MTK_FD_REG_OFFSET_HW_ENABLE		0x4
#define MTK_FD_REG_OFFSET_INT_EN		0x15c
#define MTK_FD_REG_OFFSET_INT_VAL		0x168
#define MTK_FD_REG_OFFSET_RESULT		0x178

#define MTK_FD_SET_HW_ENABLE			0x111
#define MTK_FD_RS_BUF_SIZE			2289664
#define MTK_FD_HW_WORK_BUF_SIZE			0x100000
#define MTK_FD_MAX_SPEEDUP			7
#define MTK_FD_MAX_RESULT_NUM			1026

/* Max scale size counts */
#define MTK_FD_SCALE_ARR_NUM			15

#define MTK_FD_HW_TIMEOUT			1000

enum face_angle {
	MTK_FD_FACE_FRONT,
	MTK_FD_FACE_RIGHT_50,
	MTK_FD_FACE_LEFT_50,
	MTK_FD_FACE_RIGHT_90,
	MTK_FD_FACE_LEFT_90,
	MTK_FD_FACE_ANGLE_NUM,
};

struct fd_buffer {
	__u32 scp_addr;	/* used by SCP */
	__u32 dma_addr;	/* used by DMA HW */
} __packed;

struct fd_face_result {
	char data[16];
};

struct fd_user_output {
	struct fd_face_result results[MTK_FD_MAX_RESULT_NUM];
	__u16 number;
};

struct user_param {
	u8 fd_speedup;
	u8 fd_extra_model;
	u8 scale_img_num;
	u8 src_img_fmt;
	__u16 scale_img_width[MTK_FD_SCALE_ARR_NUM];
	__u16 scale_img_height[MTK_FD_SCALE_ARR_NUM];
	__u16 face_directions[MTK_FD_FACE_ANGLE_NUM];
} __packed;

struct fd_init_param {
	struct fd_buffer fd_manager;
	__u32 rs_dma_addr;
} __packed;

struct fd_enq_param {
	__u64 output_vaddr;
	struct fd_buffer src_img[2];
	struct fd_buffer user_result;
	struct user_param user_param;
} __packed;

struct fd_ack_param {
	__u32 ret_code;
	__u32 ret_msg;
} __packed;

struct ipi_message {
	u8 cmd_id;
	union {
		struct fd_init_param fd_init_param;
		struct fd_enq_param fd_enq_param;
		struct fd_ack_param fd_ack_param;
	};
} __packed;

struct mtk_fd_dev {
	struct device *dev;
	struct mtk_fd_ctx *ctx;
	struct v4l2_device v4l2_dev;
	struct v4l2_m2m_dev *m2m_dev;
	struct media_device mdev;
	struct video_device vfd;
	struct platform_device *scp_pdev;
	struct clk *fd_clk;
	struct rproc *rproc_handle;

	/* Lock for V4L2 operations */
	struct mutex vfd_lock;

	struct fd_user_output *output;
	struct fd_buffer scp_mem;
	void __iomem *fd_base;
	void *rs_dma_buf;
	dma_addr_t rs_dma_handle;
	void *scp_mem_virt_addr;

	u32 fd_stream_count;
	struct completion fd_irq_done;
};

struct mtk_fd_ctx {
	struct mtk_fd_dev *fd_dev;
	struct device *dev;
	struct v4l2_fh fh;
	struct v4l2_ctrl_handler hdl;
	struct v4l2_pix_format_mplane src_fmt;
	struct v4l2_meta_format dst_fmt;
	struct user_param user_param;
};

#endif/*__MTK_FD_HW_H__*/
