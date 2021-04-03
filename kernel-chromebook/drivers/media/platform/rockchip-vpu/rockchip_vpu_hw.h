/*
 * Rockchip VPU codec driver
 *
 * Copyright (C) 2014 Google, Inc.
 *	Tomasz Figa <tfiga@chromium.org>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef ROCKCHIP_VPU_HW_H_
#define ROCKCHIP_VPU_HW_H_

#include <linux/interrupt.h>
#include <linux/v4l2-controls.h>
#include <media/videobuf2-core.h>

#define ROCKCHIP_HEADER_SIZE		1280
#define ROCKCHIP_HW_PARAMS_SIZE		5487
#define ROCKCHIP_RET_PARAMS_SIZE	488
#define ROCKCHIP_JPEG_QUANT_ELE_SIZE	64

#define ROCKCHIP_VPU_CABAC_TABLE_SIZE	(52 * 2 * 464)

struct rockchip_vpu_dev;
struct rockchip_vpu_ctx;
struct rockchip_vpu_buf;
struct rockchip_vpu_variant;

struct rk3288_vpu_h264d_priv_tbl;

/**
 * struct rockchip_vpu_codec_ops - codec mode specific operations
 *
 * @init:	Prepare for streaming. Called from VB2 .start_streaming()
 *		when streaming from both queues is being enabled.
 * @exit:	Clean-up after streaming. Called from VB2 .stop_streaming()
 *		when streaming from first of both enabled queues is being
 *		disabled.
 * @run:	Start single {en,de)coding run. Called from non-atomic context
 *		to indicate that a pair of buffers is ready and the hardware
 *		should be programmed and started.
 * @done:	Read back processing results and additional data from hardware.
 * @reset:	Reset the hardware in case of a timeout.
 */
struct rockchip_vpu_codec_ops {
	int (*init)(struct rockchip_vpu_ctx *);
	void (*exit)(struct rockchip_vpu_ctx *);

	void (*run)(struct rockchip_vpu_ctx *);
	void (*done)(struct rockchip_vpu_ctx *, enum vb2_buffer_state);
	void (*reset)(struct rockchip_vpu_ctx *);
};

/**
 * enum rk3288_vpu_enc_fmt - source format ID for hardware registers.
 */
enum rk3288_vpu_enc_fmt {
	RK3288_VPU_ENC_FMT_YUV420P = 0,
	RK3288_VPU_ENC_FMT_YUV420SP = 1,
	RK3288_VPU_ENC_FMT_YUYV422 = 2,
	RK3288_VPU_ENC_FMT_UYVY422 = 3,
};

/**
 * struct rk3288_h264e_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct rk3288_h264e_reg_params {
	u32 frame_coding_type;
	s32 pic_init_qp;
	s32 slice_alpha_offset;
	s32 slice_beta_offset;
	s32 chroma_qp_index_offset;
	s32 filter_disable;
	u16 idr_pic_id;
	s32 pps_id;
	s32 frame_num;
	s32 slice_size_mb_rows;
	s32 h264_inter4x4_disabled;
	s32 enable_cabac;
	s32 transform8x8_mode;
	s32 cabac_init_idc;

	/* rate control relevant */
	s32 qp;
	s32 mad_qp_delta;
	s32 mad_threshold;
	s32 qp_min;
	s32 qp_max;
	s32 cp_distance_mbs;
	s32 cp_target[10];
	s32 target_error[7];
	s32 delta_qp[7];
};

/**
 * struct rk3399_vp8e_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct rk3399_vp8e_reg_params {
	u32 is_intra;
	u32 frm_hdr_size;

	u32 qp;

	s32 mv_prob[2][19];
	s32 intra_prob;

	u32 bool_enc_value;
	u32 bool_enc_value_bits;
	u32 bool_enc_range;

	u32 filterDisable;
	u32 filter_sharpness;
	u32 filter_level;

	s32 intra_frm_delta;
	s32 last_frm_delta;
	s32 golden_frm_delta;
	s32 altref_frm_delta;

	s32 bpred_mode_delta;
	s32 zero_mode_delta;
	s32 newmv_mode_delta;
	s32 splitmv_mode_delta;
};

/**
 * struct rockchip_reg_params - low level encoding parameters
 */
struct rockchip_reg_params {
	/* Mode-specific data. */
	union {
		const struct rk3288_h264e_reg_params rk3288_h264e;
		const struct rk3399_vp8e_reg_params rk3399_vp8e;
	};
};

struct rockchip_vpu_h264e_feedback {
	s32 qp_sum;
	s32 cp[10];
	s32 mad_count;
	s32 rlc_count;
};

/**
 * struct rockchip_vpu_aux_buf - auxiliary DMA buffer for hardware data
 * @cpu:	CPU pointer to the buffer.
 * @dma:	DMA address of the buffer.
 * @size:	Size of the buffer.
 */
struct rockchip_vpu_aux_buf {
	void *cpu;
	dma_addr_t dma;
	size_t size;
};

/**
 * struct rockchip_vpu_vp8e_hw_ctx - Context private data specific to codec
 * mode.
 * @ctrl_buf:		VP8 control buffer.
 * @ext_buf:		VP8 ext data buffer.
 * @mv_buf:		VP8 motion vector buffer.
 * @ref_rec_ptr:	Bit flag for swapping ref and rec buffers every frame.
 */
struct rockchip_vpu_vp8e_hw_ctx {
	struct rockchip_vpu_aux_buf ctrl_buf;
	struct rockchip_vpu_aux_buf ext_buf;
	struct rockchip_vpu_aux_buf mv_buf;
	u8 ref_rec_ptr:1;
};

/**
 * struct rockchip_vpu_vp8d_hw_ctx - Context private data of VP8 decoder.
 * @segment_map:	Segment map buffer.
 * @prob_tbl:		Probability table buffer.
 */
struct rockchip_vpu_vp8d_hw_ctx {
	struct rockchip_vpu_aux_buf segment_map;
	struct rockchip_vpu_aux_buf prob_tbl;
};

/**
 * struct rockchip_vpu_h264d_hw_ctx - Per context data specific to H264
 * decoding.
 * @priv_tbl:		Private auxiliary buffer for hardware.
 */
struct rockchip_vpu_h264d_hw_ctx {
	struct rockchip_vpu_aux_buf priv_tbl;
};

struct rockchip_vpu_vp9d_last_info {
	bool abs_delta;
	s8 ref_deltas[4];
	s8 mode_deltas[2];
	bool segmentation_enable;
	bool show_frame;
	bool intra_only;
	u32 width;
	u32 height;
	u8 frame_type;
	s16 feature_data[8][4];
	u8 feature_mask[8];
	dma_addr_t mv_base_addr;
	bool segmap_idx;
};

struct rockchip_vpu_vp9d_counts {
	u32 partition[4][4][4];
	u32 skip[3][2];
	u32 intra[4][2];
	u32 tx32p[2][4];
	u32 tx16p[2][4]; /* orign tx16p */
	u32 tx8p[2][2];
	u32 y_mode[4][10];
	u32 uv_mode[10][10];
	u32 comp[5][2];
	u32 comp_ref[5][2];
	u32 single_ref[5][2][2];
	u32 mv_mode[7][4];
	u32 filter[4][3];
	u32 mv_joint[4];
	u32 sign[2][2];
	u32 classes[2][12]; /* orign classes[12] */
	u32 class0[2][2];
	u32 bits[2][10][2];
	u32 class0_fp[2][2][4];
	u32 fp[2][4];
	u32 class0_hp[2][2];
	u32 hp[2][2];
	u32 coef[4][2][2][6][6][3];
	u32 eob[4][2][2][6][6][2];
};

struct rockchip_vpu_vp9d_hw_ctx {
	struct rockchip_vpu_aux_buf priv_tbl;
	struct rockchip_vpu_aux_buf priv_dst;
	struct rockchip_vpu_vp9d_last_info last_info;
	dma_addr_t mv_base_addr;
};

/**
 * struct rockchip_vpu_h264e_hw_ctx - Context private data specific to
 * codec mode.
 * @ctrl_buf:		H264 control buffer.
 * @ext_buf:		H264 ext data buffer.
 * @ref_rec_ptr:	Bit flag for swapping ref and rec buffers every frame.
 */
struct rockchip_vpu_h264e_hw_ctx {
	struct rockchip_vpu_aux_buf cabac_tbl[3];
	struct rockchip_vpu_aux_buf ext_buf;
	u8 ref_rec_ptr:1;
};

/**
 * struct rockchip_vpu_hw_ctx - Context private data of hardware code.
 * @codec_ops:		Set of operations associated with current codec mode.
 */
struct rockchip_vpu_hw_ctx {
	const struct rockchip_vpu_codec_ops *codec_ops;

	/* Specific for particular codec modes. */
	union {
		struct rockchip_vpu_vp8e_hw_ctx vp8e;
		struct rockchip_vpu_vp8d_hw_ctx vp8d;
		struct rockchip_vpu_h264e_hw_ctx h264e;
		struct rockchip_vpu_h264d_hw_ctx h264d;
		struct rockchip_vpu_vp9d_hw_ctx vp9d;
		/* Other modes will need different data. */
	};
};

extern const struct rockchip_vpu_variant rk3399_vpu_variant;
extern const struct rockchip_vpu_variant rk3399_vdec_variant;

void rockchip_vpu_watchdog(struct work_struct *work);
void rockchip_vpu_power_on(struct rockchip_vpu_dev *vpu);

int rockchip_vpu_init(struct rockchip_vpu_ctx *ctx);
void rockchip_vpu_deinit(struct rockchip_vpu_ctx *ctx);

void rockchip_vpu_run(struct rockchip_vpu_ctx *ctx);

void rockchip_vpu_irq_done(struct rockchip_vpu_dev *vpu);

/* Run ops for rk3399 vpu H264 encoder */
int rk3399_vpu_h264e_init(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_h264e_exit(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_h264e_run(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_h264e_done(struct rockchip_vpu_ctx *ctx,
			   enum vb2_buffer_state result);

/* Run ops for rk3399 vpu VP8 decoder */
int rk3399_vpu_vp8d_init(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_vp8d_exit(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_vp8d_run(struct rockchip_vpu_ctx *ctx);

/* Run ops for rk3399 vpu VP8 encoder */
int rk3399_vpu_vp8e_init(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_vp8e_exit(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_vp8e_run(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_vp8e_done(struct rockchip_vpu_ctx *ctx,
			  enum vb2_buffer_state result);

/* Run ops for rk3399 vdec H264 decoder */
int rk3399_vdec_h264d_init(struct rockchip_vpu_ctx *ctx);
void rk3399_vdec_h264d_exit(struct rockchip_vpu_ctx *ctx);
void rk3399_vdec_h264d_run(struct rockchip_vpu_ctx *ctx);

/* Run ops for rk3399 vdec VP9 decoder */
int rk3399_vdec_vp9d_init(struct rockchip_vpu_ctx *ctx);
void rk3399_vdec_vp9d_exit(struct rockchip_vpu_ctx *ctx);
void rk3399_vdec_vp9d_run(struct rockchip_vpu_ctx *ctx);
void rk3399_vdec_vp9d_done(struct rockchip_vpu_ctx *ctx,
			   enum vb2_buffer_state result);

/* Run ops for rk3399 vdec JPEG encoder */
void rk3399_vpu_jpege_run(struct rockchip_vpu_ctx *ctx);
void rk3399_vpu_jpege_done(struct rockchip_vpu_ctx *ctx,
			  enum vb2_buffer_state result);

#endif /* ROCKCHIP_VPU_HW_H_ */
