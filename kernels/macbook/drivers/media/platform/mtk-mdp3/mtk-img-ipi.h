/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Holmes Chiou <holmes.chiou@mediatek.com>
 *         Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_IMG_IPI_H__
#define __MTK_IMG_IPI_H__

#include <linux/types.h>

/* ISP-MDP generic input information */

#define IMG_MAX_HW_INPUTS	3

#define IMG_MAX_HW_OUTPUTS	4

#define IMG_MAX_PLANES		3

#define IMG_IPI_INIT    1
#define IMG_IPI_DEINIT  2
#define IMG_IPI_FRAME   3
#define IMG_IPI_DEBUG   4

struct img_timeval {
	int32_t tv_sec;
	int32_t tv_usec;
} __packed;

struct img_addr {
	u64 va; /* Used for Linux OS access */
	u32 pa; /* Used for CM4 access */
	u32 iova; /* Used for IOMMU HW access */
} __packed;

struct tuning_addr {
	u64	present;
	u32	pa;	/* Used for CM4 access */
	u32	iova;	/* Used for IOMMU HW access */
} __packed;

struct img_sw_addr {
	u64 va; /* Used for APMCU access */
	u32 pa; /* Used for CM4 access */
} __packed;

struct img_plane_format {
	u32 size;
	u32 stride;
} __packed;

struct img_pix_format {
	u32 width;
	u32 height;
	u32 colorformat; /* enum mdp_color */
	u32 ycbcr_prof; /* enum mdp_ycbcr_profile */
	struct img_plane_format plane_fmt[IMG_MAX_PLANES];
} __packed;

struct img_image_buffer {
	struct img_pix_format format;
	u32 iova[IMG_MAX_PLANES];
	/* enum mdp_buffer_usage, FD or advanced ISP usages */
	u32 usage;
} __packed;

#define IMG_SUBPIXEL_SHIFT	20

struct img_crop {
	s32 left;
	s32 top;
	u32 width;
	u32 height;
	u32 left_subpix;
	u32 top_subpix;
	u32 width_subpix;
	u32 height_subpix;
} __packed;

#define IMG_CTRL_FLAG_HFLIP	BIT(0)
#define IMG_CTRL_FLAG_DITHER	BIT(1)
#define IMG_CTRL_FLAG_SHARPNESS	BIT(4)
#define IMG_CTRL_FLAG_HDR	BIT(5)
#define IMG_CTRL_FLAG_DRE	BIT(6)
#define IMG_CTRL_FLAG_RSZ	BIT(7)

struct img_input {
	struct img_image_buffer buffer;
	u32 flags; /* HDR, DRE, dither */
} __packed;

struct img_output {
	struct img_image_buffer buffer;
	struct img_crop crop;
	s32 rotation;
	u32 flags; /* H-flip, sharpness, dither */
	u64 pqid;
} __packed;

struct img_ipi_frameparam {
	u32 index;
	u32 frame_no;
	struct img_timeval timestamp;
	u32 type; /* enum mdp_stream_type */
	u32 state;
	u32 num_inputs;
	u32 num_outputs;
	u64 drv_data;
	struct img_input inputs[IMG_MAX_HW_INPUTS];
	struct img_output outputs[IMG_MAX_HW_OUTPUTS];
	struct tuning_addr tuning_data;
	struct img_addr subfrm_data;
	struct img_sw_addr config_data;
	struct img_sw_addr self_data;
	u32 frame_change;
} __packed;

struct img_sw_buffer {
	u64	handle;		/* Used for APMCU access */
	u32	scp_addr;	/* Used for CM4 access */
} __packed;

struct img_ipi_param {
	u32 usage;
	struct img_sw_buffer frm_param;
} __packed;

struct img_frameparam {
	struct list_head list_entry;
	struct img_ipi_frameparam frameparam;
} __packed;

/* ISP-MDP generic output information */

struct img_comp_frame {
	u32 output_disable;
	u32 bypass;
	u32 in_width;
	u32 in_height;
	u32 out_width;
	u32 out_height;
	struct img_crop crop;
	u32 in_total_width;
	u32 out_total_width;
} __packed;

struct img_region {
	s32 left;
	s32 right;
	s32 top;
	s32 bottom;
} __packed;

struct img_offset {
	s32 left;
	s32 top;
	u32 left_subpix;
	u32 top_subpix;
} __packed;

struct img_comp_subfrm {
	u32 tile_disable;
	struct img_region in;
	struct img_region out;
	struct img_offset luma;
	struct img_offset chroma;
	s32 out_vertical; /* Output vertical index */
	s32 out_horizontal; /* Output horizontal index */
} __packed;

#define IMG_MAX_SUBFRAMES	14

struct mdp_rdma_subfrm {
	u32 offset[IMG_MAX_PLANES];
	u32 offset_0_p;
	u32 src;
	u32 clip;
	u32 clip_ofst;
	u32 in_tile_xleft;
	u32 in_tile_ytop;
} __packed;

struct mdp_rdma_data {
	u32 src_ctrl;
	u32 comp_ctrl;
	u32 control;
	u32 iova[IMG_MAX_PLANES];
	u32 iova_end[IMG_MAX_PLANES];
	u32 mf_bkgd;
	u32 mf_bkgd_in_pxl;
	u32 sf_bkgd;
	u32 ufo_dec_y;
	u32 ufo_dec_c;
	u32 transform;
	u32 dmabuf_con0;
	u32 ultra_th_high_con0;
	u32 ultra_th_low_con0;
	u32 dmabuf_con1;
	u32 ultra_th_high_con1;
	u32 ultra_th_low_con1;
	u32 dmabuf_con2;
	u32 ultra_th_high_con2;
	u32 ultra_th_low_con2;
	u32 dmabuf_con3;
	struct mdp_rdma_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_fg_subfrm {
	u32 info_0;
	u32 info_1;
} __packed;

struct mdp_fg_data {
	u32 ctrl_0;
	u32 ck_en;
	struct mdp_fg_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_hdr_subfrm {
	u32 win_size;
	u32 src;
	u32 clip_ofst0;
	u32 clip_ofst1;
	u32 hist_ctrl_0;
	u32 hist_ctrl_1;
	u32 hdr_top;
	u32 hist_addr;
} __packed;

struct mdp_hdr_data {
	u32 top;
	u32 relay;
	struct mdp_hdr_subfrm   subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_aal_subfrm {
	u32 src;
	u32 clip;
	u32 clip_ofst;
} __packed;

struct mdp_aal_data {
	u32 cfg_main;
	u32 cfg;
	struct mdp_aal_subfrm   subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_rsz_subfrm {
	u32 control2;
	u32 src;
	u32 clip;
	u32 hdmirx_en;
	u32 luma_h_int_ofst;
	u32 luma_h_sub_ofst;
	u32 luma_v_int_ofst;
	u32 luma_v_sub_ofst;
	u32 chroma_h_int_ofst;
	u32 chroma_h_sub_ofst;
	u32 rsz_switch;
	u32 merge_cfg;
} __packed;

struct mdp_rsz_data {
	u32 coeff_step_x;
	u32 coeff_step_y;
	u32 control1;
	u32 control2;
	u32 etc_control;
	u32 prz_enable;
	u32 ibse_softclip;
	u32 tap_adapt;
	u32 ibse_gaincontrol1;
	u32 ibse_gaincontrol2;
	u32 ibse_ylevel_1;
	u32 ibse_ylevel_2;
	u32 ibse_ylevel_3;
	u32 ibse_ylevel_4;
	u32 ibse_ylevel_5;
	struct mdp_rsz_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_tdshp_subfrm {
	u32 src;
	u32 clip;
	u32 clip_ofst;
	u32 hist_cfg_0;
	u32 hist_cfg_1;
} __packed;

struct mdp_tdshp_data {
	u32 cfg;
	struct mdp_tdshp_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_color_subfrm {
	u32 in_hsize;
	u32 in_vsize;
} __packed;

struct mdp_color_data {
	u32 start;
	struct mdp_color_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_ovl_subfrm {
	u32 L0_src_size;
	u32 roi_size;
} __packed;

struct mdp_ovl_data {
	u32 L0_con;
	u32 src_con;
	struct mdp_ovl_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_pad_subfrm {
	u32 pic_size;
} __packed;

struct mdp_pad_data {
	struct mdp_pad_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_tcc_subfrm {
	u32 pic_size;
} __packed;

struct mdp_tcc_data {
	struct mdp_tcc_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_wrot_subfrm {
	u32 offset[IMG_MAX_PLANES];
	u32 src;
	u32 clip;
	u32 clip_ofst;
	u32 main_buf;
} __packed;

struct mdp_wrot_data {
	u32 iova[IMG_MAX_PLANES];
	u32 control;
	u32 stride[IMG_MAX_PLANES];
	u32 mat_ctrl;
	u32 fifo_test;
	u32 filter;
	u32 pre_ultra;
	u32 framesize;
	u32 afbc_yuvtrans;
	u32 scan_10bit;
	u32 pending_zero;
	u32 bit_number;
	u32 pvric;
	u32 vpp02vpp1;
	struct mdp_wrot_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_wdma_subfrm {
	u32 offset[IMG_MAX_PLANES];
	u32 src;
	u32 clip;
	u32 clip_ofst;
} __packed;

struct mdp_wdma_data {
	u32 wdma_cfg;
	u32 iova[IMG_MAX_PLANES];
	u32 w_in_byte;
	u32 uv_stride;
	struct mdp_wdma_subfrm subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct isp_data {
	u64 dl_flags; /* 1 << (enum mdp_comp_type) */
	u32 smxi_iova[4];
	u32 cq_idx;
	u32 cq_iova;
	u32 tpipe_iova[IMG_MAX_SUBFRAMES];
} __packed;

struct img_compparam {
	u32 type; /* enum mdp_comp_id */
	u32 id; /* engine alias_id */
	u32 input;
	u32 outputs[IMG_MAX_HW_OUTPUTS];
	u32 num_outputs;
	struct img_comp_frame frame;
	struct img_comp_subfrm subfrms[IMG_MAX_SUBFRAMES];
	u32 num_subfrms;
	union {
		struct mdp_rdma_data rdma;
		struct mdp_fg_data fg;
		struct mdp_hdr_data hdr;
		struct mdp_aal_data aal;
		struct mdp_rsz_data rsz;
		struct mdp_tdshp_data tdshp;
		struct mdp_color_data color;
		struct mdp_ovl_data ovl;
		struct mdp_pad_data pad;
		struct mdp_tcc_data tcc;
		struct mdp_wrot_data wrot;
		struct mdp_wdma_data wdma;
		struct isp_data isp;
	};
} __packed;

#define IMG_MAX_COMPONENTS	20

struct img_mux {
	u32 reg;
	u32 value;
	u32 vpp_id;
} __packed;

struct img_mmsys_ctrl {
	struct img_mux sets[IMG_MAX_COMPONENTS * 2];
	u32 num_sets;
} __packed;

struct img_config {
	struct img_compparam components[IMG_MAX_COMPONENTS];
	u32 num_components;
	struct img_mmsys_ctrl ctrls[IMG_MAX_SUBFRAMES];
	u32 num_subfrms;
} __packed;

#endif  /* __MTK_IMG_IPI_H__ */
