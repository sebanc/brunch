/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Hantro VPU codec driver
 *
 * Copyright 2018 Google LLC.
 *	Tomasz Figa <tfiga@chromium.org>
 */

#ifndef HANTRO_HW_H_
#define HANTRO_HW_H_

#include <linux/interrupt.h>
#include <linux/v4l2-controls.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>

#define DEC_8190_ALIGN_MASK	0x07U

#define MB_DIM			16
#define MB_WIDTH(w)		DIV_ROUND_UP(w, MB_DIM)
#define MB_HEIGHT(h)		DIV_ROUND_UP(h, MB_DIM)

#define HANTRO_VP8_HEADER_SIZE		1280
#define HANTRO_VP8_HW_PARAMS_SIZE	5487
#define HANTRO_VP8_RET_PARAMS_SIZE	488

/* Support up to High Profile, so no 4:4:4 or 10-bit */
#define HANTRO_H264_QP_NUM		52
#define HANTRO_H264_CABAC_CV_NUM	460
/* XXX: not sure why it needs four extra entries beyond the standard 460 */
#define HANTRO_H264_CABAC_TABLE_SIZE   (HANTRO_H264_QP_NUM * 2 * \
					(HANTRO_H264_CABAC_CV_NUM + 4))

struct hantro_dev;
struct hantro_ctx;
struct hantro_buf;
struct hantro_variant;

/**
 * struct hantro_h1_vp8_enc_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct hantro_h1_vp8_enc_reg_params {
	u32 unused_00[5];
	u32 hdr_len;
	u32 unused_18[8];
	u32 enc_ctrl;
	u32 unused_3c;
	u32 enc_ctrl0;
	u32 enc_ctrl1;
	u32 enc_ctrl2;
	u32 enc_ctrl3;
	u32 enc_ctrl5;
	u32 enc_ctrl4;
	u32 str_hdr_rem_msb;
	u32 str_hdr_rem_lsb;
	u32 unused_60;
	u32 mad_ctrl;
	u32 unused_68;
	u32 qp_val[8];
	u32 bool_enc;
	u32 vp8_ctrl0;
	u32 rlc_ctrl;
	u32 mb_ctrl;
	u32 unused_9c[14];
	u32 rgb_yuv_coeff[2];
	u32 rgb_mask_msb;
	u32 intra_area_ctrl;
	u32 cir_intra_ctrl;
	u32 unused_e8[2];
	u32 first_roi_area;
	u32 second_roi_area;
	u32 mvc_ctrl;
	u32 unused_fc;
	u32 intra_penalty[7];
	u32 unused_11c;
	u32 seg_qp[24];
	u32 dmv_4p_1p_penalty[32];
	u32 dmv_qpel_penalty[32];
	u32 vp8_ctrl1;
	u32 bit_cost_golden;
	u32 loop_flt_delta[2];
};

/**
 * struct hantro_h264_enc_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct hantro_h264_enc_reg_params {
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
 * struct rk3399_vp8_enc_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct rk3399_vp8_enc_reg_params {
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
 * struct hantro_reg_params - low level encoding parameters
 */
struct hantro_reg_params {
	/* Mode-specific data. */
	union {
		const struct hantro_h1_vp8_enc_reg_params hantro_h1_vp8_enc;
		const struct hantro_h264_enc_reg_params hantro_h264_enc;
		const struct rk3399_vp8_enc_reg_params rk3399_vp8_enc;
	};
};

/**
 * struct hantro_aux_buf - auxiliary DMA buffer for hardware data
 * @cpu:	CPU pointer to the buffer.
 * @dma:	DMA address of the buffer.
 * @size:	Size of the buffer.
 * @attrs:	Attributes of the DMA mapping.
 */
struct hantro_aux_buf {
	void *cpu;
	dma_addr_t dma;
	size_t size;
	unsigned long attrs;
};

/**
 * struct hantro_h264_enc_hw_ctx - Context private data specific to
 * codec mode.
 * @ctrl_buf:		H264 control buffer.
 * @ext_buf:		H264 ext data buffer.
 * @ref_rec_ptr:	Bit flag for swapping ref and rec buffers every frame.
 */
struct hantro_h264_enc_hw_ctx {
	struct hantro_aux_buf cabac_tbl[3];
	struct hantro_aux_buf ext_buf;
	struct hantro_aux_buf feedback;

	u8 ref_rec_ptr:1;
};

/**
 * struct hantro_vp8_enc_buf_data - mode-specific per-buffer data
 * @dct_offset:		Offset inside the buffer to DCT partition.
 * @hdr_size:		Size of header data in the buffer.
 * @ext_hdr_size:	Size of ext header data in the buffer.
 * @dct_size:		Size of DCT partition in the buffer.
 * @header:		Frame header to copy to destination buffer.
 */
struct hantro_vp8_enc_buf_data {
	size_t dct_offset;
	size_t hdr_size;
	size_t ext_hdr_size;
	size_t dct_size;
	u8 header[HANTRO_VP8_HEADER_SIZE];
};

/**
 * struct hantro_vp8_enc_ctrl_buf - hardware control buffer layout
 * @ext_hdr_size:	Ext header size in bytes (written by hardware).
 * @dct_size:		DCT partition size (written by hardware).
 * @rsvd:		Reserved for hardware.
 */
struct hantro_vp8_enc_ctrl_buf {
	u32 ext_hdr_size;
	u32 dct_size;
	u8 reserved[1016];
};

/**
 * struct hantro_vp8_enc_hw_ctx - Context private data specific to codec mode.
 * @ctrl_buf:		VP8 control buffer.
 * @ext_buf:		VP8 ext data buffer.
 * @mv_buf:			VP8 motion vector buffer.
 * @ref_rec_ptr:	Bit flag for swapping ref and rec buffers every frame.
 */
struct hantro_vp8_enc_hw_ctx {
	struct hantro_aux_buf ctrl_buf;
	struct hantro_aux_buf ext_buf;
	struct hantro_aux_buf mv_buf;

	struct hantro_aux_buf priv_src;
	struct hantro_aux_buf priv_dst;

	struct hantro_vp8_enc_buf_data buf_data;

	const struct rk3399_vp8_enc_reg_params *params;

	u8 ref_rec_ptr:1;
};

/* Max. number of entries in the DPB (HW limitation). */
#define HANTRO_H264_DPB_SIZE		16

/**
 * struct hantro_h264_dec_ctrls
 * @decode:	Decode params
 * @scaling:	Scaling info
 * @sps:	SPS info
 * @pps:	PPS info
 */
struct hantro_h264_dec_ctrls {
	const struct v4l2_ctrl_h264_decode_params *decode;
	const struct v4l2_ctrl_h264_scaling_matrix *scaling;
	const struct v4l2_ctrl_h264_sps *sps;
	const struct v4l2_ctrl_h264_pps *pps;
};

/**
 * struct hantro_h264_dec_reflists
 * @p:		P reflist
 * @b0:		B0 reflist
 * @b1:		B1 reflist
 */
struct hantro_h264_dec_reflists {
	u8 p[HANTRO_H264_DPB_SIZE];
	u8 b0[HANTRO_H264_DPB_SIZE];
	u8 b1[HANTRO_H264_DPB_SIZE];
};

/**
 * struct hantro_h264_dec_hw_ctx
 * @priv:	Private auxiliary buffer for hardware.
 * @dpb:	DPB
 * @reflists:	P/B0/B1 reflists
 * @ctrls:	V4L2 controls attached to a run
 */
struct hantro_h264_dec_hw_ctx {
	struct hantro_aux_buf priv;
	struct v4l2_h264_dpb_entry dpb[HANTRO_H264_DPB_SIZE];
	struct hantro_h264_dec_reflists reflists;
	struct hantro_h264_dec_ctrls ctrls;
};

/**
 * struct hantro_mpeg2_dec_hw_ctx
 * @qtable:		Quantization table
 */
struct hantro_mpeg2_dec_hw_ctx {
	struct hantro_aux_buf qtable;
};

/**
 * struct hantro_vp8d_hw_ctx
 * @segment_map:	Segment map buffer.
 * @prob_tbl:		Probability table buffer.
 */
struct hantro_vp8_dec_hw_ctx {
	struct hantro_aux_buf segment_map;
	struct hantro_aux_buf prob_tbl;
};

/**
 * struct hantro_postproc_ctx
 *
 * @dec_q:		References buffers, in decoder format.
 */
struct hantro_postproc_ctx {
	struct hantro_aux_buf dec_q[VB2_MAX_FRAME];
};

/**
 * struct hantro_codec_ops - codec mode specific operations
 *
 * @init:	If needed, can be used for initialization.
 *		Optional and called from process context.
 * @exit:	If needed, can be used to undo the .init phase.
 *		Optional and called from process context.
 * @run:	Start single {en,de)coding job. Called from atomic context
 *		to indicate that a pair of buffers is ready and the hardware
 *		should be programmed and started. Returns zero if OK, a
 *		negative value in error cases.
 * @done:	Read back processing results and additional data from hardware.
 * @reset:	Reset the hardware in case of a timeout.
 */
struct hantro_codec_ops {
	int (*init)(struct hantro_ctx *ctx);
	void (*exit)(struct hantro_ctx *ctx);
	int (*run)(struct hantro_ctx *ctx);
	void (*done)(struct hantro_ctx *ctx);
	void (*reset)(struct hantro_ctx *ctx);
};

/**
 * enum hantro_enc_fmt - source format ID for hardware registers.
 */
enum hantro_enc_fmt {
	RK3288_VPU_ENC_FMT_YUV420P = 0,
	RK3288_VPU_ENC_FMT_YUV420SP = 1,
	RK3288_VPU_ENC_FMT_YUYV422 = 2,
	RK3288_VPU_ENC_FMT_UYVY422 = 3,
};

extern const struct hantro_variant rk3399_vpu_variant;
extern const struct hantro_variant rk3328_vpu_variant;
extern const struct hantro_variant rk3288_vpu_variant;
extern const struct hantro_variant imx8mq_vpu_variant;

extern const struct hantro_postproc_regs hantro_g1_postproc_regs;

extern const u32 hantro_vp8_dec_mc_filter[8][6];

void hantro_watchdog(struct work_struct *work);
void hantro_run(struct hantro_ctx *ctx);
void hantro_irq_done(struct hantro_dev *vpu,
		     enum vb2_buffer_state result);
void hantro_start_prepare_run(struct hantro_ctx *ctx);
void hantro_end_prepare_run(struct hantro_ctx *ctx);

int hantro_h1_jpeg_enc_run(struct hantro_ctx *ctx);
int rk3399_vpu_jpeg_enc_run(struct hantro_ctx *ctx);
void hantro_h1_jpeg_enc_done(struct hantro_ctx *ctx);
void rk3399_vpu_jpeg_enc_done(struct hantro_ctx *ctx);

int hantro_h1_vp8_enc_run(struct hantro_ctx *ctx);
int hantro_vp8_enc_init(struct hantro_ctx *ctx);
void hantro_vp8_enc_assemble_bitstream(struct hantro_ctx *ctx,
				       struct vb2_buffer *vb);
void hantro_vp8_enc_exit(struct hantro_ctx *ctx);
void hantro_vp8_enc_done(struct hantro_ctx *ctx);

int rk3399_vpu_vp8_enc_run(struct hantro_ctx *ctx);
int rk3399_vpu_vp8_enc_init(struct hantro_ctx *ctx);
void rk3399_vpu_vp8_enc_done(struct hantro_ctx *ctx);
void rk3399_vpu_vp8_enc_exit(struct hantro_ctx *ctx);

int rk3399_vpu_h264_enc_run(struct hantro_ctx *ctx);
int rk3399_vpu_h264_enc_init(struct hantro_ctx *ctx);
void rk3399_vpu_h264_enc_done(struct hantro_ctx *ctx);
void rk3399_vpu_h264_enc_exit(struct hantro_ctx *ctx);

dma_addr_t hantro_h264_get_ref_buf(struct hantro_ctx *ctx,
				   unsigned int dpb_idx);
int hantro_h264_dec_prepare_run(struct hantro_ctx *ctx);
int hantro_g1_h264_dec_run(struct hantro_ctx *ctx);
int hantro_h264_dec_init(struct hantro_ctx *ctx);
void hantro_h264_dec_exit(struct hantro_ctx *ctx);

static inline size_t
hantro_h264_mv_size(unsigned int width, unsigned int height)
{
	/*
	 * A decoded 8-bit 4:2:0 NV12 frame may need memory for up to
	 * 448 bytes per macroblock with additional 32 bytes on
	 * multi-core variants.
	 *
	 * The H264 decoder needs extra space on the output buffers
	 * to store motion vectors. This is needed for reference
	 * frames and only if the format is non-post-processed NV12.
	 *
	 * Memory layout is as follow:
	 *
	 * +---------------------------+
	 * | Y-plane   256 bytes x MBs |
	 * +---------------------------+
	 * | UV-plane  128 bytes x MBs |
	 * +---------------------------+
	 * | MV buffer  64 bytes x MBs |
	 * +---------------------------+
	 * | MC sync          32 bytes |
	 * +---------------------------+
	 */
	return 64 * MB_WIDTH(width) * MB_WIDTH(height) + 32;
}

int hantro_g1_mpeg2_dec_run(struct hantro_ctx *ctx);
int rk3399_vpu_mpeg2_dec_run(struct hantro_ctx *ctx);
void hantro_mpeg2_dec_copy_qtable(u8 *qtable,
	const struct v4l2_ctrl_mpeg2_quantization *ctrl);
int hantro_mpeg2_dec_init(struct hantro_ctx *ctx);
void hantro_mpeg2_dec_exit(struct hantro_ctx *ctx);

int hantro_g1_vp8_dec_run(struct hantro_ctx *ctx);
int rk3399_vpu_vp8_dec_run(struct hantro_ctx *ctx);
int hantro_vp8_dec_init(struct hantro_ctx *ctx);
void hantro_vp8_dec_exit(struct hantro_ctx *ctx);
void hantro_vp8_prob_update(struct hantro_ctx *ctx,
			    const struct v4l2_ctrl_vp8_frame *hdr);

#endif /* HANTRO_HW_H_ */
