/* SPDX-License-Identifier: GPL-2.0 */
/*
 * These are the VP9 state controls for use with stateless VP9
 * codec drivers.
 *
 * It turns out that these structs are not stable yet and will undergo
 * more changes. So keep them private until they are stable and ready to
 * become part of the official public API.
 */

#ifndef _VP9_CTRLS_H_
#define _VP9_CTRLS_H_

#include <linux/types.h>

#define V4L2_PIX_FMT_VP9_FRAME v4l2_fourcc('V', 'P', '9', 'F')

#define V4L2_CID_MPEG_VIDEO_VP9_FRAME_CONTEXT(i)	(V4L2_CID_CODEC_BASE + 4000 + (i))
#define V4L2_CID_MPEG_VIDEO_VP9_FRAME_DECODE_PARAMS	(V4L2_CID_CODEC_BASE + 4004)
#define V4L2_CTRL_TYPE_VP9_FRAME_CONTEXT		0x400
#define V4L2_CTRL_TYPE_VP9_FRAME_DECODE_PARAMS		0x404

/**
 * enum v4l2_vp9_loop_filter_flags - VP9 loop filter flags
 *
 * @V4L2_VP9_LOOP_FILTER_FLAG_DELTA_ENABLED: the filter level depends on
 *					     the mode and reference frame used
 *					     to predict a block
 * @V4L2_VP9_LOOP_FILTER_FLAG_DELTA_UPDATE: the bitstream contains additional
 *					    syntax elements that specify which
 *					    mode and reference frame deltas
 *					    are to be updated
 *
 * Those are the flags you should pass to &v4l2_vp9_loop_filter.flags. See
 * section '7.2.8 Loop filter semantics' of the VP9 specification for more
 * details.
 */
enum v4l2_vp9_loop_filter_flags {
	V4L2_VP9_LOOP_FILTER_FLAG_DELTA_ENABLED = 1 << 0,
	V4L2_VP9_LOOP_FILTER_FLAG_DELTA_UPDATE = 1 << 1,
};

/**
 * struct v4l2_vp9_loop_filter - VP9 loop filter parameters
 *
 * @flags: combination of V4L2_VP9_LOOP_FILTER_FLAG_* flags
 * @level: indicates the loop filter strength
 * @sharpness: indicates the sharpness level
 * @ref_deltas: contains the adjustment needed for the filter level based on
 *		the chosen reference frame
 * @mode_deltas: contains the adjustment needed for the filter level based on
 *		 the chosen mode
 * @level_lookup: level lookup table
 *
 * This structure contains all loop filter related parameters. See sections
 * '7.2.8 Loop filter semantics' and '8.8.1 Loop filter frame init process'
 * of the VP9 specification for more details.
 */
struct v4l2_vp9_loop_filter {
	__u8 flags;
	__u8 level;
	__u8 sharpness;
	__s8 ref_deltas[4];
	__s8 mode_deltas[2];
	__u8 level_lookup[8][4][2];
};

/**
 * struct v4l2_vp9_quantization - VP9 quantization parameters
 *
 * @base_q_idx: indicates the base frame qindex
 * @delta_q_y_dc: indicates the Y DC quantizer relative to base_q_idx
 * @delta_q_uv_dc: indicates the UV DC quantizer relative to base_q_idx
 * @delta_q_uv_ac indicates the UV AC quantizer relative to base_q_idx
 * @padding: padding bytes to align things on 64 bits. Must be set to 0
 *
 * Encodes the quantization parameters. See section '7.2.9 Quantization params
 * syntax' of the VP9 specification for more details.
 */
struct v4l2_vp9_quantization {
	/* TODO LOSSLESS flag missing? */
	__u8 base_q_idx;
	__s8 delta_q_y_dc;
	__s8 delta_q_uv_dc;
	__s8 delta_q_uv_ac;
	__u8 padding[4];
};

/**
 * enum v4l2_vp9_segmentation_flags - VP9 segmentation flags
 *
 * @V4L2_VP9_SEGMENTATION_FLAG_ENABLED: indicates that this frame makes use of
 *					the segmentation tool
 * @V4L2_VP9_SEGMENTATION_FLAG_UPDATE_MAP: indicates that the segmentation map
 *					   should be updated during the
 *					   decoding of this frame
 * @V4L2_VP9_SEGMENTATION_FLAG_TEMPORAL_UPDATE: indicates that the updates to
 *						the segmentation map are coded
 *						relative to the existing
 *						segmentation map
 * @V4L2_VP9_SEGMENTATION_FLAG_UPDATE_DATA: indicates that new parameters are
 *					    about to be specified for each
 *					    segment
 * @V4L2_VP9_SEGMENTATION_FLAG_ABS_OR_DELTA_UPDATE: indicates that the
 *						    segmentation parameters
 *						    represent the actual values
 *						    to be used
 *
 * Those are the flags you should pass to &v4l2_vp9_segmentation.flags. See
 * section '7.2.10 Segmentation params syntax' of the VP9 specification for
 * more details.
 */
enum v4l2_vp9_segmentation_flags {
	V4L2_VP9_SEGMENTATION_FLAG_ENABLED = 1 << 0,
	V4L2_VP9_SEGMENTATION_FLAG_UPDATE_MAP = 1 << 1,
	V4L2_VP9_SEGMENTATION_FLAG_TEMPORAL_UPDATE = 1 << 2,
	V4L2_VP9_SEGMENTATION_FLAG_UPDATE_DATA = 1 << 3,
	V4L2_VP9_SEGMENTATION_FLAG_ABS_OR_DELTA_UPDATE = 1 << 4,
};

#define V4L2_VP9_SEGMENT_FEATURE_ENABLED(id)	(1 << (id))
#define V4L2_VP9_SEGMENT_FEATURE_ENABLED_MASK	0xf

/**
 * enum v4l2_vp9_segment_feature - VP9 segment feature IDs
 *
 * @V4L2_VP9_SEGMENT_FEATURE_QP_DELTA: QP delta segment feature
 * @V4L2_VP9_SEGMENT_FEATURE_LF: loop filter segment feature
 * @V4L2_VP9_SEGMENT_FEATURE_REF_FRAME: reference frame segment feature
 * @V4L2_VP9_SEGMENT_FEATURE_SKIP: skip segment feature
 * @V4L2_VP9_SEGMENT_FEATURE_CNT: number of segment features
 *
 * Segment feature IDs. See section '7.2.10 Segmentation params syntax' of the
 * VP9 specification for more details.
 */
enum v4l2_vp9_segment_feature {
	V4L2_VP9_SEGMENT_FEATURE_QP_DELTA,
	V4L2_VP9_SEGMENT_FEATURE_LF,
	V4L2_VP9_SEGMENT_FEATURE_REF_FRAME,
	V4L2_VP9_SEGMENT_FEATURE_SKIP,
	V4L2_VP9_SEGMENT_FEATURE_CNT,
};

/**
 * struct v4l2_vp9_segmentation - VP9 segmentation parameters
 *
 * @flags: combination of V4L2_VP9_SEGMENTATION_FLAG_* flags
 * @tree_probs: specifies the probability values to be used when
 *              decoding a Segment-ID. See '5.15. Segmentation map'
 *              section of the VP9 specification for more details.
 * @pred_prob: specifies the probability values to be used when decoding a
 *	       Predicted-Segment-ID. See '6.4.14. Get segment id syntax'
 *	       section of :ref:`vp9` for more details..
 * @padding: padding used to make things aligned on 64 bits. Shall be zero
 *	     filled
 * @feature_enabled: bitmask defining which features are enabled in each
 *		     segment
 * @feature_data: data attached to each feature. Data entry is only valid if
 *		  the feature is enabled
 *
 * Encodes the quantization parameters. See section '7.2.10 Segmentation
 * params syntax' of the VP9 specification for more details.
 */
struct v4l2_vp9_segmentation {
	__u8 flags;
	__u8 tree_probs[7];
	__u8 pred_probs[3];
	__u8 padding[5];
	__u8 feature_enabled[8];
	__s16 feature_data[8][4];
};

/**
 * enum v4l2_vp9_intra_prediction_mode - VP9 Intra prediction modes
 *
 * @V4L2_VP9_INTRA_PRED_DC: DC intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_V: vertical intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_H: horizontal intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_D45: D45 intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_D135: D135 intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_D117: D117 intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_D153: D153 intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_D207: D207 intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_D63: D63 intra prediction
 * @V4L2_VP9_INTRA_PRED_MODE_TM: True Motion intra prediction
 *
 * See section '7.4.5 Intra frame mode info semantics' for more details.
 */
// TODO where is this used??
enum v4l2_vp9_intra_prediction_mode {
	V4L2_VP9_INTRA_PRED_MODE_DC,
	V4L2_VP9_INTRA_PRED_MODE_V,
	V4L2_VP9_INTRA_PRED_MODE_H,
	V4L2_VP9_INTRA_PRED_MODE_D45,
	V4L2_VP9_INTRA_PRED_MODE_D135,
	V4L2_VP9_INTRA_PRED_MODE_D117,
	V4L2_VP9_INTRA_PRED_MODE_D153,
	V4L2_VP9_INTRA_PRED_MODE_D207,
	V4L2_VP9_INTRA_PRED_MODE_D63,
	V4L2_VP9_INTRA_PRED_MODE_TM,
};

/**
 * struct v4l2_vp9_mv_probabilities - VP9 Motion vector probabilities
 * @joint: motion vector joint probabilities
 * @sign: motion vector sign probabilities
 * @class_: motion vector class probabilities
 * @class0_bit: motion vector class0 bit probabilities
 * @bits: motion vector bits probabilities
 * @class0_fr: motion vector class0 fractional bit probabilities
 * @fr: motion vector fractional bit probabilities
 * @class0_hp: motion vector class0 high precision fractional bit probabilities
 * @hp: motion vector high precision fractional bit probabilities
 */
struct v4l2_vp9_mv_probabilities {
	__u8 joint[3];
	__u8 sign[2];
	__u8 class_[2][10];
	__u8 class0_bit[2];
	__u8 bits[2][10];
	__u8 class0_fr[2][2][3];
	__u8 fr[2][3];
	__u8 class0_hp[2];
	__u8 hp[2];
};

/**
 * struct v4l2_vp9_probabilities - VP9 Probabilities
 *
 * @tx8: TX 8x8 probabilities
 * @tx16: TX 16x16 probabilities
 * @tx32: TX 32x32 probabilities
 * @coef: coefficient probabilities
 * @skip: skip probabilities
 * @inter_mode: inter mode probabilities
 * @interp_filter: interpolation filter probabilities
 * @is_inter: is inter-block probabilities
 * @comp_mode: compound prediction mode probabilities
 * @single_ref: single ref probabilities
 * @comp_ref: compound ref probabilities
 * @y_mode: Y prediction mode probabilities
 * @uv_mode: UV prediction mode probabilities
 * @partition: partition probabilities
 * @mv: motion vector probabilities
 *
 * Structure containing most VP9 probabilities. See the VP9 specification
 * for more details.
 */
struct v4l2_vp9_probabilities {
	__u8 tx8[2][1];
	__u8 tx16[2][2];
	__u8 tx32[2][3];
	__u8 coef[4][2][2][6][6][3];
	__u8 skip[3];
	__u8 inter_mode[7][3];
	__u8 interp_filter[4][2];
	__u8 is_inter[4];
	__u8 comp_mode[5];
	__u8 single_ref[5][2];
	__u8 comp_ref[5];
	__u8 y_mode[4][9];
	__u8 uv_mode[10][9];
	__u8 partition[16][3];

	struct v4l2_vp9_mv_probabilities mv;
};

/**
 * enum v4l2_vp9_reset_frame_context - Valid values for
 *			&v4l2_ctrl_vp9_frame_decode_params->reset_frame_context
 *
 * @V4L2_VP9_RESET_FRAME_CTX_NONE: don't reset any frame context
 * @V4L2_VP9_RESET_FRAME_CTX_SPEC: reset the frame context pointed by
 *			&v4l2_ctrl_vp9_frame_decode_params.frame_context_idx
 * @V4L2_VP9_RESET_FRAME_CTX_ALL: reset all frame contexts
 *
 * See section '7.2 Uncompressed header semantics' of the VP9 specification
 * for more details.
 */
enum v4l2_vp9_reset_frame_context {
	V4L2_VP9_RESET_FRAME_CTX_NONE,
	V4L2_VP9_RESET_FRAME_CTX_SPEC,
	V4L2_VP9_RESET_FRAME_CTX_ALL,
};

/**
 * enum v4l2_vp9_interpolation_filter - VP9 interpolation filter types
 *
 * @V4L2_VP9_INTERP_FILTER_8TAP: height tap filter
 * @V4L2_VP9_INTERP_FILTER_8TAP_SMOOTH: height tap smooth filter
 * @V4L2_VP9_INTERP_FILTER_8TAP_SHARP: height tap sharp filter
 * @V4L2_VP9_INTERP_FILTER_BILINEAR: bilinear filter
 * @V4L2_VP9_INTERP_FILTER_SWITCHABLE: filter selection is signaled at the
 *				       block level
 *
 * See section '7.2.7 Interpolation filter semantics' of the VP9 specification
 * for more details.
 */
enum v4l2_vp9_interpolation_filter {
	V4L2_VP9_INTERP_FILTER_8TAP,
	V4L2_VP9_INTERP_FILTER_8TAP_SMOOTH,
	V4L2_VP9_INTERP_FILTER_8TAP_SHARP,
	V4L2_VP9_INTERP_FILTER_BILINEAR,
	V4L2_VP9_INTERP_FILTER_SWITCHABLE,
};

/**
 * enum v4l2_vp9_reference_mode - VP9 reference modes
 *
 * @V4L2_VP9_REF_MODE_SINGLE: indicates that all the inter blocks use only a
 *			      single reference frame to generate motion
 *			      compensated prediction
 * @V4L2_VP9_REF_MODE_COMPOUND: requires all the inter blocks to use compound
 *				mode. Single reference frame prediction is not
 *				allowed
 * @V4L2_VP9_REF_MODE_SELECT: allows each individual inter block to select
 *			      between single and compound prediction modes
 *
 * See section '7.3.6 Frame reference mode semantics' of the VP9 specification
 * for more details.
 */
enum v4l2_vp9_reference_mode {
	V4L2_VP9_REF_MODE_SINGLE,
	V4L2_VP9_REF_MODE_COMPOUND,
	V4L2_VP9_REF_MODE_SELECT,
};

/**
 * enum v4l2_vp9_tx_mode - VP9 TX modes
 *
 * @V4L2_VP9_TX_MODE_ONLY_4X4: transform size is 4x4
 * @V4L2_VP9_TX_MODE_ALLOW_8X8: transform size can be up to 8x8
 * @V4L2_VP9_TX_MODE_ALLOW_16X16: transform size can be up to 16x16
 * @V4L2_VP9_TX_MODE_ALLOW_32X32: transform size can be up to 32x32
 * @V4L2_VP9_TX_MODE_SELECT: bitstream contains transform size for each block
 *
 * See section '7.3.1 Tx mode semantics' of the VP9 specification for more
 * details.
 */
enum v4l2_vp9_tx_mode {
	V4L2_VP9_TX_MODE_ONLY_4X4,
	V4L2_VP9_TX_MODE_ALLOW_8X8,
	V4L2_VP9_TX_MODE_ALLOW_16X16,
	V4L2_VP9_TX_MODE_ALLOW_32X32,
	V4L2_VP9_TX_MODE_SELECT,
};

/**
 * enum v4l2_vp9_ref_id - VP9 Reference frame IDs
 *
 * @V4L2_REF_ID_LAST: last reference frame
 * @V4L2_REF_ID_GOLDEN: golden reference frame
 * @V4L2_REF_ID_ALTREF: alternative reference frame
 * @V4L2_REF_ID_CNT: number of reference frames
 *
 * See section '7.4.12 Ref frames semantics' of the VP9 specification for more
 * details.
 */
enum v4l2_vp9_ref_id {
	V4L2_REF_ID_LAST,
	V4L2_REF_ID_GOLDEN,
	V4L2_REF_ID_ALTREF,
	V4L2_REF_ID_CNT,
};

/**
 * enum v4l2_vp9_frame_flags - VP9 frame flags
 * @V4L2_VP9_FRAME_FLAG_KEY_FRAME: the frame is a key frame
 * @V4L2_VP9_FRAME_FLAG_SHOW_FRAME: the frame should be displayed
 * @V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT: the decoding should be error resilient
 * @V4L2_VP9_FRAME_FLAG_INTRA_ONLY: the frame does not reference other frames
 * @V4L2_VP9_FRAME_FLAG_ALLOW_HIGH_PREC_MV: the frame might can high precision
 *					    motion vectors
 * @V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX: frame context should be updated
 *					   after decoding
 * @V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE: parallel decoding is used
 * @V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING: vertical subsampling is enabled
 * @V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING: horizontal subsampling is enabled
 * @V4L2_VP9_FRAME_FLAG_COLOR_RANGE_FULL_SWING: full UV range is used
 *
 * Check the VP9 specification for more details.
 */
enum v4l2_vp9_frame_flags {
	V4L2_VP9_FRAME_FLAG_KEY_FRAME = 1 << 0,
	V4L2_VP9_FRAME_FLAG_SHOW_FRAME = 1 << 1,
	V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT = 1 << 2,
	V4L2_VP9_FRAME_FLAG_INTRA_ONLY = 1 << 3,
	V4L2_VP9_FRAME_FLAG_ALLOW_HIGH_PREC_MV = 1 << 4,
	V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX = 1 << 5,
	V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE = 1 << 6,
	V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING = 1 << 7,
	V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING = 1 << 8,
	V4L2_VP9_FRAME_FLAG_COLOR_RANGE_FULL_SWING = 1 << 9,
};

#define V4L2_VP9_PROFILE_MAX		3

/**
 * struct v4l2_ctrl_vp9_frame_decode_params - VP9 frame decoding control
 *
 * @flags: combination of V4L2_VP9_FRAME_FLAG_* flags
 * @compressed_header_size: compressed header size in bytes
 * @uncompressed_header_size: uncompressed header size in bytes
 * @profile: VP9 profile. Can be 0, 1, 2 or 3
 * @reset_frame_context: specifies whether the frame context should be reset
 *			 to default values. See &v4l2_vp9_reset_frame_context
 *			 for more details
 * @frame_context_idx: frame context that should be used/updated
 * @bit_depth: bits per components. Can be 8, 10 or 12. Note that not all
 *	       profiles support 10 and/or 12 bits depths
 * @interpolation_filter: specifies the filter selection used for performing
 *			  inter prediction. See &v4l2_vp9_interpolation_filter
 *			  for more details
 * @tile_cols_log2: specifies the base 2 logarithm of the width of each tile
 *		    (where the width is measured in units of 8x8 blocks).
 *		    Shall be less than or equal to 6
 * @tile_rows_log2: specifies the base 2 logarithm of the height of each tile
 *		    (where the height is measured in units of 8x8 blocks)
 * @tx_mode: specifies the TX mode. See &v4l2_vp9_tx_mode for more details
 * @reference_mode: specifies the type of inter prediction to be used. See
 *		    &v4l2_vp9_reference_mode for more details
 * @ref_frame_sign_biases: intended direction in time of the motion vector for
 *                         each reference frame (0: backward, 1: forward).
 *                         Only the first V4L2_REF_ID_CNT are used.
 * @padding: needed to make this struct 64 bit aligned. Shall be filled with
 *	     zeros
 * @frame_width_minus_1: add 1 to it and you'll get the frame width expressed
 *			 in pixels
 * @frame_height_minus_1: add 1 to it and you'll get the frame height expressed
 *			  in pixels
 * @frame_width_minus_1: add 1 to it and you'll get the expected render width
 *			 expressed in pixels. This is not used during the
 *			 decoding process but might be used by HW scalers to
 *			 prepare a frame that's ready for scanout
 * @frame_height_minus_1: add 1 to it and you'll get the expected render height
 *			 expressed in pixels. This is not used during the
 *			 decoding process but might be used by HW scalers to
 *			 prepare a frame that's ready for scanout
 * @refs: array of reference frames, identified by timestamp. See
          &v4l2_vp9_ref_id for more details
 * @lf: loop filter parameters. See &v4l2_vp9_loop_filter for more details
 * @quant: quantization parameters. See &v4l2_vp9_quantization for more details
 * @seg: segmentation parameters. See &v4l2_vp9_segmentation for more details
 * @probs: probabilities. See &v4l2_vp9_probabilities for more details
 */
struct v4l2_ctrl_vp9_frame_decode_params {
	__u32 flags;
	__u16 compressed_header_size;
	__u16 uncompressed_header_size;
	__u8 profile;
	__u8 reset_frame_context;
	__u8 frame_context_idx;
	__u8 bit_depth;
	__u8 interpolation_filter;
	__u8 tile_cols_log2;
	__u8 tile_rows_log2;
	__u8 tx_mode;
	__u8 reference_mode;
	__u8 ref_frame_sign_biases;
	__u8 padding[5];
	__u16 frame_width_minus_1;
	__u16 frame_height_minus_1;
	__u16 render_width_minus_1;
	__u16 render_height_minus_1;
	__u64 refs[V4L2_REF_ID_CNT];
	struct v4l2_vp9_loop_filter lf;
	struct v4l2_vp9_quantization quant;
	struct v4l2_vp9_segmentation seg;
	struct v4l2_vp9_probabilities probs;
};

#define V4L2_VP9_NUM_FRAME_CTX	4

/**
 * struct v4l2_ctrl_vp9_frame_ctx - VP9 frame context control
 *
 * @probs: VP9 probabilities
 *
 * This control is accessed in both direction. The user should initialize the
 * 4 contexts with default values just after starting the stream. Then before
 * decoding a frame it should query the current frame context (the one passed
 * through &v4l2_ctrl_vp9_frame_decode_params.frame_context_idx) to initialize
 * &v4l2_ctrl_vp9_frame_decode_params.probs. The probs are then adjusted based
 * on the bitstream info and passed to the kernel. The codec should update
 * the frame context after the frame has been decoded, so that next time
 * userspace query this context it contains the updated probabilities.
 */
struct v4l2_ctrl_vp9_frame_ctx {
	struct v4l2_vp9_probabilities probs;
};

#endif /* _VP9_CTRLS_H_ */
