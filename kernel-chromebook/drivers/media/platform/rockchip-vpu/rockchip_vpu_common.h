/*
 * Rockchip VPU codec driver
 *
 * Copyright (C) 2014 Google, Inc.
 *	Tomasz Figa <tfiga@chromium.org>
 *
 * Based on s5p-mfc driver by Samsung Electronics Co., Ltd.
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
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

#ifndef ROCKCHIP_VPU_COMMON_H_
#define ROCKCHIP_VPU_COMMON_H_

/* Enable debugging by default for now. */
#define DEBUG

#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/wait.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include "rockchip_vpu_hw.h"

#define V4L2_CID_CUSTOM_BASE		(V4L2_CID_USER_BASE | 0x1000)

#define DST_QUEUE_OFF_BASE		(1UL << 30)

#define ROCKCHIP_VPU_MAX_CTRLS		32

#define MB_DIM				16
#define MB_WIDTH(x_size)		DIV_ROUND_UP(x_size, MB_DIM)
#define MB_HEIGHT(y_size)		DIV_ROUND_UP(y_size, MB_DIM)
#define SB_DIM				64
#define SB_WIDTH(x_size)		DIV_ROUND_UP(x_size, SB_DIM)
#define SB_HEIGHT(y_size)		DIV_ROUND_UP(y_size, SB_DIM)

struct rockchip_vpu_ctx;
struct rockchip_vpu_codec_ops;

/**
 * struct rockchip_vpu_variant - information about VPU hardware variant
 *
 * @enc_offset:			Offset from VPU base to encoder registers.
 * @dec_offset:			Offset from VPU base to decoder registers.
 * @needs_dpb_map:		Needs dpb reorder mapping.
 * @enc_fmts:			Encoder formats.
 * @num_enc_fmts:		Number of encoder formats.
 * @dec_fmts:			Decoder formats.
 * @num_dec_fmts:		Number of decoder formats.
 * @mode_ops:			Codec ops.
 * @hw_probe:			Probe hardware.
 * @clk_enable:			Enable clocks.
 * @clk_disable:		Disable clocks.
 */
struct rockchip_vpu_variant {
	unsigned enc_offset;
	unsigned dec_offset;
	bool needs_dpb_map;
	const struct rockchip_vpu_fmt *enc_fmts;
	unsigned num_enc_fmts;
	const struct rockchip_vpu_fmt *dec_fmts;
	unsigned num_dec_fmts;
	const struct rockchip_vpu_codec_ops *mode_ops;
	int (*hw_probe)(struct rockchip_vpu_dev *vpu);
	void (*clk_enable)(struct rockchip_vpu_dev *vpu);
	void (*clk_disable)(struct rockchip_vpu_dev *vpu);
};

/**
 * enum rockchip_vpu_codec_mode - codec operating mode.
 * @RK_VPU_CODEC_NONE:	No operating mode. Used for RAW video formats.
 * @RK_VPU_CODEC_H264D:	H264 decoder.
 * @RK_VPU_CODEC_VP8D:	VP8 decoder.
 * @RK_VPU_CODEC_VP9D:	VP9 decoder.
 * @RK_VPU_CODEC_H264E:	H264 encoder.
 * @RK_VPU_CODEC_VP8E:	VP8 encoder.
 * @RK_VPU_CODEC_JPEGE:	JPEG encoder.
 */
enum rockchip_vpu_codec_mode {
	RK_VPU_CODEC_NONE = -1,
	RK_VPU_CODEC_H264D,
	RK_VPU_CODEC_VP8D,
	RK_VPU_CODEC_VP9D,
	RK_VPU_CODEC_H264E,
	RK_VPU_CODEC_VP8E,
	RK_VPU_CODEC_JPEGE
};

/**
 * enum rockchip_vpu_plane - indices of planes inside a VB2 buffer.
 * @PLANE_Y:		Plane containing luminance data (also denoted as Y).
 * @PLANE_CB_CR:	Plane containing interleaved chrominance data (also
 *			denoted as CbCr).
 * @PLANE_CB:		Plane containing CB part of chrominance data.
 * @PLANE_CR:		Plane containing CR part of chrominance data.
 */
enum rockchip_vpu_plane {
	PLANE_Y		= 0,
	PLANE_CB_CR	= 1,
	PLANE_CB	= 1,
	PLANE_CR	= 2,
};

/**
 * struct rockchip_vpu_vp8e_buf_data - mode-specific per-buffer data
 * @dct_offset:		Offset inside the buffer to DCT partition.
 * @hdr_size:		Size of header data in the buffer.
 * @ext_hdr_size:	Size of ext header data in the buffer.
 * @dct_size:		Size of DCT partition in the buffer.
 * @header:		Frame header to copy to destination buffer.
 */
struct rockchip_vpu_vp8e_buf_data {
	size_t dct_offset;
	size_t hdr_size;
	size_t ext_hdr_size;
	size_t dct_size;
	u8 header[ROCKCHIP_HEADER_SIZE];
};

/**
 * struct rockchip_vpu_buf - Private data related to each VB2 buffer.
 * @b:			Pointer to related VB2 buffer.
 * @list:		List head for queuing in buffer queue.
 */
struct rockchip_vpu_buf {
	struct vb2_v4l2_buffer b;
	struct list_head list;

	/* Mode-specific data. */
	union {
		struct rockchip_vpu_vp8e_buf_data vp8e;
	};
};

/**
 * enum rockchip_vpu_state - bitwise flags indicating hardware state.
 * @VPU_RUNNING:	The hardware has been programmed for operation
 *			and is running at the moment.
 * @VPU_SUSPENDED:	System is entering sleep state and no more runs
 *			should be executed on hardware.
 */
enum rockchip_vpu_state {
	VPU_RUNNING	= BIT(0),
	VPU_SUSPENDED	= BIT(1),
};

/**
 * struct rockchip_vpu_dev - driver data
 * @v4l2_dev:		V4L2 device to register video devices for.
 * @vfd_dec:		Video device for decoder.
 * @vfd_enc:		Video device for encoder.
 * @pdev:		Pointer to VPU platform device.
 * @dev:		Pointer to device for convenient logging using
 *			dev_ macros.
 * @alloc_ctx:		VB2 allocator context
 *			(for allocations without kernel mapping).
 * @alloc_ctx_vm:	VB2 allocator context
 *			(for allocations with kernel mapping).
 * @aclk:		Handle of ACLK clock.
 * @hclk:		Handle of HCLK clock.
 * @sclk_cabac:		Handle of SCLK CA clock.
 * @sclk_core:		Handle of SCLK CORE clock.
 * @base:		Mapped address of VPU registers.
 * @enc_base:		Mapped address of VPU encoder register for convenience.
 * @dec_base:		Mapped address of VPU decoder register for convenience.
 * @vpu_mutex:		Mutex to synchronize V4L2 calls.
 * @irqlock:		Spinlock to synchronize access to data structures
 *			shared with interrupt handlers.
 * @state:		Device state.
 * @ready_ctxs:		List of contexts ready to run.
 * @variant:		Hardware variant-specific parameters.
 * @current_ctx:	Context being currently processed by hardware.
 * @run_wq:		Wait queue to wait for run completion.
 * @watchdog_work:	Delayed work for hardware timeout handling.
 * @was_decoding:	Indicates whether last run context was a decoder.
 */
struct rockchip_vpu_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd_dec;
	struct video_device *vfd_enc;
	struct platform_device *pdev;
	struct device *dev;
	void *alloc_ctx;
	void *alloc_ctx_vm;
	struct clk *aclk;
	struct clk *hclk;
	struct clk *sclk_cabac;
	struct clk *sclk_core;
	void __iomem *base;
	void __iomem *enc_base;
	void __iomem *dec_base;

	struct mutex vpu_mutex;	/* video_device lock */
	spinlock_t irqlock;
	unsigned long state;
	struct list_head ready_ctxs;
	const struct rockchip_vpu_variant *variant;
	struct rockchip_vpu_ctx *current_ctx;
	wait_queue_head_t run_wq;
	struct delayed_work watchdog_work;
	bool was_decoding;
};

/**
 * struct rockchip_vpu_run_ops - per context operations on run data.
 * @prepare_run:	Called when the context was selected for running
 *			to prepare operating mode specific data.
 * @run_done:		Called when hardware completed the run to collect
 *			operating mode specific data from hardware and
 *			finalize the processing.
 */
struct rockchip_vpu_run_ops {
	void (*prepare_run)(struct rockchip_vpu_ctx *);
	void (*run_done)(struct rockchip_vpu_ctx *, enum vb2_buffer_state);
};

/**
 * struct rockchip_vpu_vp8e_run - per-run data specific to VP8 encoding.
 * @reg_params:	Pointer to a buffer containing register values prepared
 *		by user space.
 */
struct rockchip_vpu_vp8e_run {
	const struct rockchip_reg_params *reg_params;
};

/**
 * struct rockchip_vpu_vp8d_run - per-run data specific to VP8 decoding.
 * @frame_hdr: Pointer to a buffer containing per-run frame data which
 *			is needed by setting vpu register.
 */
struct rockchip_vpu_vp8d_run {
	const struct v4l2_ctrl_vp8_frame_hdr *frame_hdr;
};

/**
 * struct rockchip_vpu_h264d_run - per-run data specific to H264 decoding.
 * @sps:		Pointer to a buffer containing H264 SPS.
 * @pps:		Pointer to a buffer containing H264 PPS.
 * @scaling_matrix:	Pointer to a buffer containing scaling matrix.
 * @slice_param:	Pointer to a buffer containing slice parameters array.
 * @decode_param:	Pointer to a buffer containing decode parameters.
 * @dpb:		Array of DPB entries reordered to keep POC order, (valid
 *			only if needs_dpb_map of hardware variant is true).
 * @dpb_map:		Map of indices used in ref_pic_list_* into indices to
 *			reordered DPB array, (valid only if needs_dpb_map of
 *			hardware variant is true).
 */
struct rockchip_vpu_h264d_run {
	const struct v4l2_ctrl_h264_sps *sps;
	const struct v4l2_ctrl_h264_pps *pps;
	const struct v4l2_ctrl_h264_scaling_matrix *scaling_matrix;
	const struct v4l2_ctrl_h264_slice_param *slice_param;
	const struct v4l2_ctrl_h264_decode_param *decode_param;
	struct v4l2_h264_dpb_entry dpb[16];
	u8 dpb_map[16];
};

/**
 * struct rockchip_vpu_h264e_run - per-run data specific to H264 encoding.
 */
struct rockchip_vpu_h264e_run {
	const struct rockchip_reg_params *reg_params;
};

/**
 * struct rockchip_vpu_vp9d_run - per-run data specific to vp9
 * decoding.
 * @dec_param: Pointer to a buffer containing per-run frame data
 *	       which is needed by setting vpu register.
 */
struct rockchip_vpu_vp9d_run {
	const struct v4l2_ctrl_vp9_frame_hdr *frame_hdr;
	const struct v4l2_ctrl_vp9_decode_param *dec_param;
	struct v4l2_ctrl_vp9_entropy *entropy;
};

struct rockchip_vpu_jpege_run {
	u8 lumin_quant_tbl[ROCKCHIP_JPEG_QUANT_ELE_SIZE];
	u8 chroma_quant_tbl[ROCKCHIP_JPEG_QUANT_ELE_SIZE];
};

#define FIELD(word, bit)	(32 * (word) + (bit))

#define WRITE_HEADER(value, buffer, field)	\
	write_header(value, buffer, field ## _OFF, field ## _LEN)

void write_header(u32 value, u32 *buffer, u32 offset, u32 len);
/**
 * struct rockchip_vpu_run - per-run data for hardware code.
 * @src:		Source buffer to be processed.
 * @dst:		Destination buffer to be processed.
 * @priv_src:		Hardware private source buffer.
 * @priv_dst:		Hardware private destination buffer.
 */
struct rockchip_vpu_run {
	/* Generic for more than one operating mode. */
	struct rockchip_vpu_buf *src;
	struct rockchip_vpu_buf *dst;

	struct rockchip_vpu_aux_buf priv_src;
	struct rockchip_vpu_aux_buf priv_dst;

	/* Specific for particular operating modes. */
	union {
		struct rockchip_vpu_vp8e_run vp8e;
		struct rockchip_vpu_vp8d_run vp8d;
		struct rockchip_vpu_h264d_run h264d;
		struct rockchip_vpu_h264e_run h264e;
		struct rockchip_vpu_vp9d_run vp9d;
		struct rockchip_vpu_jpege_run jpege;
		/* Other modes will need different data. */
	};
};

/**
 * struct rockchip_vpu_ctx - Context (instance) private data.
 *
 * @dev:		VPU driver data to which the context belongs.
 * @fh:			V4L2 file handler.
 *
 * @vpu_src_fmt:	Descriptor of active source format.
 * @src_fmt:		V4L2 pixel format of active source format.
 * @vpu_dst_fmt:	Descriptor of active destination format.
 * @dst_fmt:		V4L2 pixel format of active destination format.
 *
 * @vq_src:		Videobuf2 source queue.
 * @src_queue:		Internal source buffer queue.
 * @src_crop:		Configured source crop rectangle (encoder-only).
 * @vq_dst:		Videobuf2 destination queue
 * @dst_queue:		Internal destination buffer queue.
 * @dst_bufs:		Private buffers wrapping VB2 buffers (destination).
 * @flush_buf:		Dummy buffer for CMD_STOP flushing purposes.
 *
 * @ctrls:		Array containing pointer to registered controls.
 * @ctrl_handler:	Control handler used to register controls.
 * @num_ctrls:		Number of registered controls.
 *
 * @list:		List head for queue of ready contexts.
 *
 * @run:		Structure containing data about currently scheduled
 *			processing run.
 * @run_ops:		Set of operations related to currently scheduled run.
 * @hw:			Structure containing hardware-related context.
 * @stopped:		Context received CMD_STOP {de,en}coder command.
 */
struct rockchip_vpu_ctx {
	struct rockchip_vpu_dev *dev;
	struct v4l2_fh fh;

	/* Format info */
	const struct rockchip_vpu_fmt *vpu_src_fmt;
	struct v4l2_pix_format_mplane src_fmt;
	const struct rockchip_vpu_fmt *vpu_dst_fmt;
	struct v4l2_pix_format_mplane dst_fmt;

	/* VB2 queue data */
	struct vb2_queue vq_src;
	struct list_head src_queue;
	struct v4l2_rect src_crop;
	struct vb2_queue vq_dst;
	struct list_head dst_queue;
	struct vb2_buffer *dst_bufs[VIDEO_MAX_FRAME];
	struct rockchip_vpu_buf flush_buf;

	/* Controls */
	struct v4l2_ctrl *ctrls[ROCKCHIP_VPU_MAX_CTRLS];
	struct v4l2_ctrl_handler ctrl_handler;
	unsigned num_ctrls;

	/* Various runtime data */
	struct list_head list;

	struct rockchip_vpu_run run;
	const struct rockchip_vpu_run_ops *run_ops;
	struct rockchip_vpu_hw_ctx hw;
	bool stopped;
};

/**
 * struct rockchip_vpu_fmt - information about supported video formats.
 * @name:	Human readable name of the format.
 * @fourcc:	FourCC code of the format. See V4L2_PIX_FMT_*.
 * @codec_mode:	Codec mode related to this format. See
 *		enum rockchip_vpu_codec_mode.
 * @num_mplanes:	Number of memory planes (buffers).
 * @num_cplanes:	Number of color planes used by this format
 *			(for raw formats).
 * @depth:	Depth of each plane in bits per pixel.
 * @v_subsampling:	Vertical subsampling factor
 * @h_subsampling:	Horizontal subsampling factor
 * @enc_fmt:	Format identifier for encoder registers.
 * @frmsize:	Supported range of frame sizes (only for bitstream formats).
 */
struct rockchip_vpu_fmt {
	char *name;
	u32 fourcc;
	enum rockchip_vpu_codec_mode codec_mode;
	int num_mplanes;
	int num_cplanes;
	u8 depth[VIDEO_MAX_PLANES];
	u8 h_subsampling[VIDEO_MAX_PLANES];
	u8 v_subsampling[VIDEO_MAX_PLANES];
	enum rk3288_vpu_enc_fmt enc_fmt;
	struct v4l2_frmsize_stepwise frmsize;
};

/**
 * struct rockchip_vpu_control - information about controls to be registered.
 * @id:			Control ID.
 * @type:		Type of the control.
 * @name:		Human readable name of the control.
 * @minimum:		Minimum value of the control.
 * @maximum:		Maximum value of the control.
 * @step:		Control value increase step.
 * @menu_skip_mask:	Mask of invalid menu positions.
 * @default_value:	Initial value of the control.
 * @max_stores:		Maximum number of configration stores.
 * @dims:		Size of each dimension of compound control.
 * @elem_size:		Size of individual element of compound control.
 * @is_volatile:	Control is volatile.
 * @is_read_only:	Control is read-only.
 * @can_store:		Control uses configuration stores.
 *
 * See also struct v4l2_ctrl_config.
 */
struct rockchip_vpu_control {
	u32 id;

	enum v4l2_ctrl_type type;
	const char *name;
	s32 minimum;
	s32 maximum;
	s32 step;
	u32 menu_skip_mask;
	s32 default_value;
	s32 max_stores;
	u32 dims[V4L2_CTRL_MAX_DIMS];
	u32 elem_size;

	bool is_volatile:1;
	bool is_read_only:1;
	bool can_store:1;
};

struct refs_counts {
	u32 eob[2];
	u32 coeff[3];
};

#define REF_TYPES	2	/* intra=0, inter=1 */
#define TX_SIZES	4	/**
				 * 4x4 transform
				 * 8x8 transform
				 * 16x16 transform
				 * 32x32 transform
				 **/
#define PLANE_TYPES	2	/* Y UV */
/* Middle dimension reflects the coefficient position within the transform. */
#define COEF_BANDS	6
#define COEFF_CONTEXTS	6

#define PARTITION_CONTEXTS		16
#define PARTITION_TYPES			4

#define MAX_SEGMENTS			8
#define SEG_TREE_PROBS			(MAX_SEGMENTS-1)
#define PREDICTION_PROBS		3
#define SKIP_CONTEXTS			3
#define TX_SIZE_CONTEXTS		2
#define TX_SIZES			4
#define INTRA_INTER_CONTEXTS		4
#define PLANE_TYPES			2
#define COEF_BANDS			6
#define COEFF_CONTEXTS			6
#define UNCONSTRAINED_NODES		3
#define INTRA_MODES			10
#define INTER_PROB_SIZE_ALIGN_TO_128	151
#define INTRA_PROB_SIZE_ALIGN_TO_128	149
#define BLOCK_SIZE_GROUPS		4
#define COMP_INTER_CONTEXTS		5
#define REF_CONTEXTS			5
#define INTER_MODE_CONTEXTS		7
#define SWITCHABLE_FILTERS		3 /* number of switchable filters */
#define SWITCHABLE_FILTER_CONTEXTS	(SWITCHABLE_FILTERS + 1)
#define INTER_MODES			4
#define MV_JOINTS			4
#define MV_CLASSES			11
/* bits at integer precision for class 0 */
#define CLASS0_BITS			1
#define CLASS0_SIZE			(1 << CLASS0_BITS)
#define MV_OFFSET_BITS		(MV_CLASSES + CLASS0_BITS - 2)
#define MV_FP_SIZE			4

/* count output if inter frame being decoded */
struct symbol_counts_for_inter_frame {
	u32 partition[PARTITION_CONTEXTS][PARTITION_TYPES];
	u32 skip[SKIP_CONTEXTS][2];
	u32 inter[4][2];
	u32 tx32p[TX_SIZE_CONTEXTS][TX_SIZES];
	/**
	 * tx16p counts contain 2x3 elements, only 3 TX_SIZES
	 * tx16p[0][3] and tx16p[1][3] no meaning, only use for
	 * memory align
	 **/
	u32 tx16p[TX_SIZE_CONTEXTS][TX_SIZES - 1 + 1];
	u32 tx8p[TX_SIZE_CONTEXTS][TX_SIZES - 2];
	u32 y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
	u32 uv_mode[INTRA_MODES][INTRA_MODES];
	u32 comp[COMP_INTER_CONTEXTS][2];
	u32 comp_ref[REF_CONTEXTS][2];
	u32 single_ref[REF_CONTEXTS][2][2];
	u32 mv_mode[INTER_MODE_CONTEXTS][INTER_MODES];
	u32 filter[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
	u32 mv_joint[MV_JOINTS];
	u32 sign[2][2];
	/* add 1 element for align */
	u32 classes[2][MV_CLASSES + 1];
	u32 class0[2][CLASS0_SIZE];
	u32 bits[2][MV_OFFSET_BITS][2];
	u32 class0_fp[2][CLASS0_SIZE][MV_FP_SIZE];
	u32 fp[2][4];
	u32 class0_hp[2][2];
	u32 hp[2][2];
	struct refs_counts ref_cnt[REF_TYPES][TX_SIZES][PLANE_TYPES]
				  [COEF_BANDS][COEFF_CONTEXTS];
};

/* counts output if intra frame being decoded */
struct symbol_counts_for_intra_frame {
	u32 partition[4][4][PARTITION_TYPES];
	u32 skip[SKIP_CONTEXTS][2];
	u32 intra[4][2];
	u32 tx32p[TX_SIZE_CONTEXTS][TX_SIZES];
	u32 tx16p[TX_SIZE_CONTEXTS][TX_SIZES - 1 + 1];
	u32 tx8p[TX_SIZE_CONTEXTS][TX_SIZES - 2];
	struct refs_counts ref_cnt[REF_TYPES][TX_SIZES][PLANE_TYPES]
				  [COEF_BANDS][COEFF_CONTEXTS];
};

union rkv_vp9_symbol_counts {
	struct symbol_counts_for_intra_frame intra_spec;
	struct symbol_counts_for_inter_frame inter_spec;
};

struct intra_mode_prob {
	u8 y_mode_prob[105];
	u8 uv_mode_prob[23];
};

struct intra_only_frm_spec {
	u8 coef_probs_intra[4][2][128];
	struct intra_mode_prob intra_mode[10];
};

struct inter_frm_spec {
	u8 y_mode_probs[4][9];
	u8 comp_mode_prob[5];
	u8 comp_ref_prob[5];
	u8 single_ref_prob[5][2];
	u8 inter_mode_probs[7][3];
	u8 interp_filter_probs[4][2];
	u8 res[11];
	u8 coef_probs[2][4][2][128];
	u8 uv_mode_prob_0_2[3][9];
	u8 res0[5];
	u8 uv_mode_prob_3_5[3][9];
	u8 res1[5];
	u8 uv_mode_prob_6_8[3][9];
	u8 res2[5];
	u8 uv_mode_prob_9[9];
	u8 res3[7];
	u8 res4[16];
	u8 mv_joint_probs[3];
	u8 mv_sign_prob[2];
	u8 mv_class_probs[2][10];
	u8 mv_class0_bit_prob[2];
	u8 mv_bits_prob[2][10];
	u8 mv_class0_fr_probs[2][2][3];
	u8 mv_fr_probs[2][3];
	u8 mv_class0_hp_prob[2];
	u8 mv_hp_prob[2];
};

struct vp9_decoder_probs_hw {
	u8 partition_probs[16][3];
	u8 pred_probs[3];
	u8 tree_probs[7];
	u8 skip_prob[3];
	u8 tx_probs_32x32[2][3];
	u8 tx_probs_16x16[2][2];
	u8 tx_probs_8x8[2][1];
	u8 is_inter_prob[4];
	u8 res[3];
	/* 128 bit align */
	union {
		struct intra_only_frm_spec intra_spec;
		struct inter_frm_spec inter_spec;
	};
};

/* Logging helpers */

/**
 * debug - Module parameter to control level of debugging messages.
 *
 * Level of debugging messages can be controlled by bits of module parameter
 * called "debug". Meaning of particular bits is as follows:
 *
 * bit 0 - global information: mode, size, init, release
 * bit 1 - each run start/result information
 * bit 2 - contents of small controls from userspace
 * bit 3 - contents of big controls from userspace
 * bit 4 - detail fmt, ctrl, buffer q/dq information
 * bit 5 - detail function enter/leave trace information
 * bit 6 - register write/read information
 */
extern int rockchip_vpu_debug;

#define vpu_debug(level, fmt, args...)				\
	do {							\
		if (rockchip_vpu_debug & BIT(level))		\
			pr_debug("%s:%d: " fmt,	                \
				 __func__, __LINE__, ##args);	\
	} while (0)

#define vpu_debug_enter()	vpu_debug(5, "enter\n")
#define vpu_debug_leave()	vpu_debug(5, "leave\n")

#define vpu_err(fmt, args...)					\
	pr_err("%s:%d: " fmt, __func__, __LINE__, ##args)

static inline char *fmt2str(u32 fmt, char *str)
{
	char a = fmt & 0xFF;
	char b = (fmt >> 8) & 0xFF;
	char c = (fmt >> 16) & 0xFF;
	char d = (fmt >> 24) & 0xFF;

	sprintf(str, "%c%c%c%c", a, b, c, d);

	return str;
}

/* Structure access helpers. */
static inline struct rockchip_vpu_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct rockchip_vpu_ctx, fh);
}

static inline struct rockchip_vpu_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler,
			struct rockchip_vpu_ctx, ctrl_handler);
}

static inline struct rockchip_vpu_buf *vb_to_buf(struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct rockchip_vpu_buf, b);
}

static inline bool rockchip_vpu_ctx_is_encoder(struct rockchip_vpu_ctx *ctx)
{
	return ctx->vpu_dst_fmt->codec_mode != RK_VPU_CODEC_NONE;
}

static inline unsigned int rockchip_vpu_rounded_luma_size(unsigned int w,
							  unsigned int h)
{
	return round_up(w, MB_DIM) * round_up(h, MB_DIM);
}

void rockchip_vpu_update_planes(const struct rockchip_vpu_fmt *fmt,
				struct v4l2_pix_format_mplane *pix_fmt_mp);
int rockchip_vpu_ctrls_setup(struct rockchip_vpu_ctx *ctx,
			   const struct v4l2_ctrl_ops *ctrl_ops,
			   struct rockchip_vpu_control *controls,
			   unsigned num_ctrls,
			   const char *const *(*get_menu)(u32));
void rockchip_vpu_ctrls_delete(struct rockchip_vpu_ctx *ctx);

void rockchip_vpu_try_context(struct rockchip_vpu_dev *dev,
			    struct rockchip_vpu_ctx *ctx);

void rockchip_vpu_run_done(struct rockchip_vpu_ctx *ctx,
			 enum vb2_buffer_state result);

int rockchip_vpu_aux_buf_alloc(struct rockchip_vpu_dev *vpu,
			    struct rockchip_vpu_aux_buf *buf, size_t size);
void rockchip_vpu_aux_buf_free(struct rockchip_vpu_dev *vpu,
			     struct rockchip_vpu_aux_buf *buf);

/* Register accessors. */
static inline void vepu_write_relaxed(struct rockchip_vpu_dev *vpu,
				       u32 val, u32 reg)
{
	vpu_debug(6, "MARK: set reg[%03d]: %08x\n", reg / 4, val);
	writel_relaxed(val, vpu->enc_base + reg);
}

static inline void vepu_write(struct rockchip_vpu_dev *vpu, u32 val, u32 reg)
{
	vpu_debug(6, "MARK: set reg[%03d]: %08x\n", reg / 4, val);
	writel(val, vpu->enc_base + reg);
}

static inline u32 vepu_read(struct rockchip_vpu_dev *vpu, u32 reg)
{
	u32 val = readl(vpu->enc_base + reg);

	vpu_debug(6, "MARK: get reg[%03d]: %08x\n", reg / 4, val);
	return val;
}

static inline void vdpu_write_relaxed(struct rockchip_vpu_dev *vpu,
				       u32 val, u32 reg)
{
	vpu_debug(6, "MARK: set reg[%03d]: %08x\n", reg / 4, val);
	writel_relaxed(val, vpu->dec_base + reg);
}

static inline void vdpu_write(struct rockchip_vpu_dev *vpu, u32 val, u32 reg)
{
	vpu_debug(6, "MARK: set reg[%03d]: %08x\n", reg / 4, val);
	writel(val, vpu->dec_base + reg);
}

static inline u32 vdpu_read(struct rockchip_vpu_dev *vpu, u32 reg)
{
	u32 val = readl(vpu->dec_base + reg);

	vpu_debug(6, "MARK: get reg[%03d]: %08x\n", reg / 4, val);
	return val;
}

#endif /* ROCKCHIP_VPU_COMMON_H_ */
