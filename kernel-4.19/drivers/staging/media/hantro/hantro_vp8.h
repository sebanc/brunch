/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 */

#ifndef _HANTRO_VP8_H
#define _HANTRO_VP8_H

#include "hantro_v4l2.h"

#define V4L2_CID_CUSTOM_BASE			(V4L2_CID_USER_BASE | 0x1000)
#define V4L2_CID_PRIVATE_HANTRO_HEADER		(V4L2_CID_CUSTOM_BASE + 0)
#define V4L2_CID_PRIVATE_HANTRO_REG_PARAMS	(V4L2_CID_CUSTOM_BASE + 1)
#define V4L2_CID_PRIVATE_HANTRO_HW_PARAMS	(V4L2_CID_CUSTOM_BASE + 2)
#define V4L2_CID_PRIVATE_HANTRO_RET_PARAMS	(V4L2_CID_CUSTOM_BASE + 3)

/**
 * struct hantro_vp8_enc_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct hantro_vp8_enc_reg_params {
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

#endif //_HANTRO_VP8_H
