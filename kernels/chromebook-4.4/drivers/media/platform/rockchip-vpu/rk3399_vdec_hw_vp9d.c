/*
 * Rockchip VPU codec driver
 *
 * Copyright (C) 2016 Rockchip Electronics Co., Ltd.
 *	Alpha Lin <Alpha.Lin@rock-chips.com>
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

#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include "rockchip_vpu_common.h"

#include "rk3399_vdec_regs.h"
#include "rockchip_vpu_hw.h"

#define RKV_VP9D_PROBE_SIZE		4864
#define RKV_VP9D_COUNT_SIZE		13232
#define RKV_VP9D_MAX_SEGMAP_SIZE	73728
#define RKV_VP9D_NUM_SEGMAP		2

struct rk3399_vdec_vp9d_segmap {
	u8 data[RKV_VP9D_MAX_SEGMAP_SIZE];
};

/* Data structure describing auxiliary buffer format. */
struct rk3399_vdec_vp9d_priv_tbl {
	u8 prob_table[RKV_VP9D_PROBE_SIZE];
	struct rk3399_vdec_vp9d_segmap segmap[RKV_VP9D_NUM_SEGMAP];
};

enum {
	TX_4X4,
	TX_8X8,
	TX_16X16,
	TX_32X32,
	N_TXFM_SIZES,
	TX_SWITCHABLE = N_TXFM_SIZES,
	N_TXFM_MODES
};

enum {
	DCT_DCT,
	DCT_ADST,
	ADST_DCT,
	ADST_ADST,
	N_TXFM_TYPES
};

enum {
	VERT_PRED,
	HOR_PRED,
	DC_PRED,
	DIAG_DOWN_LEFT_PRED,
	DIAG_DOWN_RIGHT_PRED,
	VERT_RIGHT_PRED,
	HOR_DOWN_PRED,
	VERT_LEFT_PRED,
	HOR_UP_PRED,
	TM_VP8_PRED,
	LEFT_DC_PRED,
	TOP_DC_PRED,
	DC_128_PRED,
	DC_127_PRED,
	DC_129_PRED,
	N_INTRA_PRED_MODES
};

enum {
	FILTER_8TAP_SMOOTH,
	FILTER_8TAP_REGULAR,
	FILTER_8TAP_SHARP,
	FILTER_BILINEAR,
	FILTER_SWITCHABLE,
};

enum {
	PRED_SINGLEREF,
	PRED_COMPREF,
	PRED_SWITCHABLE,
};


#ifndef FASTDIV
#define FASTDIV(a, b) ((u32)((((u64)a) * vp9_inverse[b]) >> 32))
#endif /* FASTDIV */

const u32 vp9_inverse[] = {
	0, 4294967295U, 2147483648U, 1431655766, 1073741824,  858993460,
	715827883,  613566757,	536870912,  477218589,  429496730,  390451573,
	357913942,  330382100,  306783379,  286331154,	268435456,  252645136,
	238609295,  226050911,  214748365,  204522253,  195225787,  186737709,
	178956971,  171798692,  165191050,  159072863,  153391690,  148102321,
	143165577,  138547333,	134217728,  130150525,  126322568,  122713352,
	119304648,  116080198,  113025456,  110127367,	107374183,  104755300,
	102261127,   99882961,   97612894,   95443718,   93368855,   91382283,
	89478486,   87652394,   85899346,   84215046,   82595525,   81037119,
	79536432,   78090315,	76695845,   75350304,   74051161,   72796056,
	71582789,   70409300,   69273667,   68174085,	67108864,   66076420,
	65075263,   64103990,   63161284,   62245903,   61356676,   60492498,
	59652324,   58835169,   58040099,   57266231,   56512728,   55778797,
	55063684,   54366675,	53687092,   53024288,   52377650,   51746594,
	51130564,   50529028,   49941481,   49367441,	48806447,   48258060,
	47721859,   47197443,   46684428,   46182445,   45691142,   45210183,
	44739243,   44278014,   43826197,   43383509,   42949673,   42524429,
	42107523,   41698712,	41297763,   40904451,   40518560,   40139882,
	39768216,   39403370,   39045158,   38693400,	38347923,   38008561,
	37675152,   37347542,   37025581,   36709123,   36398028,   36092163,
	35791395,   35495598,   35204650,   34918434,   34636834,   34359739,
	34087043,   33818641,	33554432,   33294321,   33038210,   32786010,
	32537632,   32292988,   32051995,   31814573,	31580642,   31350127,
	31122952,   30899046,   30678338,   30460761,   30246249,   30034737,
	29826162,   29620465,   29417585,   29217465,   29020050,   28825284,
	28633116,   28443493,	28256364,   28071682,   27889399,   27709467,
	27531842,   27356480,   27183338,   27012373,	26843546,   26676816,
	26512144,   26349493,   26188825,   26030105,   25873297,   25718368,
	25565282,   25414008,   25264514,   25116768,   24970741,   24826401,
	24683721,   24542671,	24403224,   24265352,   24129030,   23994231,
	23860930,   23729102,   23598722,   23469767,	23342214,   23216040,
	23091223,   22967740,   22845571,   22724695,   22605092,   22486740,
	22369622,   22253717,   22139007,   22025474,   21913099,   21801865,
	21691755,   21582751,	21474837,   21367997,   21262215,   21157475,
	21053762,   20951060,   20849356,   20748635,	20648882,   20550083,
	20452226,   20355296,   20259280,   20164166,   20069941,   19976593,
	19884108,   19792477,   19701685,   19611723,   19522579,   19434242,
	19346700,   19259944,	19173962,   19088744,   19004281,   18920561,
	18837576,   18755316,   18673771,   18592933,	18512791,   18433337,
	18354562,   18276457,   18199014,   18122225,   18046082,   17970575,
	17895698,   17821442,   17747799,   17674763,   17602325,   17530479,
	17459217,   17388532,	17318417,   17248865,   17179870,   17111424,
	17043522,   16976156,   16909321,   16843010,   16777216
};

static u8 vp9_kf_y_mode_prob[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1] = {
	{
		/* above = dc */
		{ 137,  30,  42, 148, 151, 207,  70,  52,  91 },/*left = dc  */
		{  92,  45, 102, 136, 116, 180,  74,  90, 100 },/*left = v   */
		{  73,  32,  19, 187, 222, 215,  46,  34, 100 },/*left = h   */
		{  91,  30,  32, 116, 121, 186,  93,  86,  94 },/*left = d45 */
		{  72,  35,  36, 149,  68, 206,  68,  63, 105 },/*left = d135*/
		{  73,  31,  28, 138,  57, 124,  55, 122, 151 },/*left = d117*/
		{  67,  23,  21, 140, 126, 197,  40,  37, 171 },/*left = d153*/
		{  86,  27,  28, 128, 154, 212,  45,  43,  53 },/*left = d207*/
		{  74,  32,  27, 107,  86, 160,  63, 134, 102 },/*left = d63 */
		{  59,  67,  44, 140, 161, 202,  78,  67, 119 } /*left = tm  */
	}, {  /* above = v */
		{  63,  36, 126, 146, 123, 158,  60,  90,  96 },/*left = dc  */
		{  43,  46, 168, 134, 107, 128,  69, 142,  92 },/*left = v   */
		{  44,  29,  68, 159, 201, 177,  50,  57,  77 },/*left = h   */
		{  58,  38,  76, 114,  97, 172,  78, 133,  92 },/*left = d45 */
		{  46,  41,  76, 140,  63, 184,  69, 112,  57 },/*left = d135*/
		{  38,  32,  85, 140,  46, 112,  54, 151, 133 },/*left = d117*/
		{  39,  27,  61, 131, 110, 175,  44,  75, 136 },/*left = d153*/
		{  52,  30,  74, 113, 130, 175,  51,  64,  58 },/*left = d207*/
		{  47,  35,  80, 100,  74, 143,  64, 163,  74 },/*left = d63 */
		{  36,  61, 116, 114, 128, 162,  80, 125,  82 } /*left = tm  */
	}, {  /* above = h */
		{  82,  26,  26, 171, 208, 204,  44,  32, 105 },/*left = dc  */
		{  55,  44,  68, 166, 179, 192,  57,  57, 108 },/*left = v   */
		{  42,  26,  11, 199, 241, 228,  23,  15,  85 },/*left = h   */
		{  68,  42,  19, 131, 160, 199,  55,  52,  83 },/*left = d45 */
		{  58,  50,  25, 139, 115, 232,  39,  52, 118 },/*left = d135*/
		{  50,  35,  33, 153, 104, 162,  64,  59, 131 },/*left = d117*/
		{  44,  24,  16, 150, 177, 202,  33,  19, 156 },/*left = d153*/
		{  55,  27,  12, 153, 203, 218,  26,  27,  49 },/*left = d207*/
		{  53,  49,  21, 110, 116, 168,  59,  80,  76 },/*left = d63 */
		{  38,  72,  19, 168, 203, 212,  50,  50, 107 } /*left = tm  */
	}, {  /* above = d45 */
		{ 103,  26,  36, 129, 132, 201,  83,  80,  93 },/*left = dc  */
		{  59,  38,  83, 112, 103, 162,  98, 136,  90 },/*left = v   */
		{  62,  30,  23, 158, 200, 207,  59,  57,  50 },/*left = h   */
		{  67,  30,  29,  84,  86, 191, 102,  91,  59 },/*left = d45 */
		{  60,  32,  33, 112,  71, 220,  64,  89, 104 },/*left = d135*/
		{  53,  26,  34, 130,  56, 149,  84, 120, 103 },/*left = d117*/
		{  53,  21,  23, 133, 109, 210,  56,  77, 172 },/*left = d153*/
		{  77,  19,  29, 112, 142, 228,  55,  66,  36 },/*left = d207*/
		{  61,  29,  29,  93,  97, 165,  83, 175, 162 },/*left = d63 */
		{  47,  47,  43, 114, 137, 181, 100,  99,  95 } /*left = tm  */
	}, {  /* above = d135 */
		{  69,  23,  29, 128,  83, 199,  46,  44, 101 },/*left = dc  */
		{  53,  40,  55, 139,  69, 183,  61,  80, 110 },/*left = v   */
		{  40,  29,  19, 161, 180, 207,  43,  24,  91 },/*left = h   */
		{  60,  34,  19, 105,  61, 198,  53,  64,  89 },/*left = d45 */
		{  52,  31,  22, 158,  40, 209,  58,  62,  89 },/*left = d135*/
		{  44,  31,  29, 147,  46, 158,  56, 102, 198 },/*left = d117*/
		{  35,  19,  12, 135,  87, 209,  41,  45, 167 },/*left = d153*/
		{  55,  25,  21, 118,  95, 215,  38,  39,  66 },/*left = d207*/
		{  51,  38,  25, 113,  58, 164,  70,  93,  97 },/*left = d63 */
		{  47,  54,  34, 146, 108, 203,  72, 103, 151 } /*left = tm  */
	}, {  /* above = d117 */
		{  64,  19,  37, 156,  66, 138,  49,  95, 133 },/*left = dc  */
		{  46,  27,  80, 150,  55, 124,  55, 121, 135 },/*left = v   */
		{  36,  23,  27, 165, 149, 166,  54,  64, 118 },/*left = h   */
		{  53,  21,  36, 131,  63, 163,  60, 109,  81 },/*left = d45 */
		{  40,  26,  35, 154,  40, 185,  51,  97, 123 },/*left = d135*/
		{  35,  19,  34, 179,  19,  97,  48, 129, 124 },/*left = d117*/
		{  36,  20,  26, 136,  62, 164,  33,  77, 154 },/*left = d153*/
		{  45,  18,  32, 130,  90, 157,  40,  79,  91 },/*left = d207*/
		{  45,  26,  28, 129,  45, 129,  49, 147, 123 },/*left = d63 */
		{  38,  44,  51, 136,  74, 162,  57,  97, 121 } /*left = tm  */
	}, {  /* above = d153 */
		{  75,  17,  22, 136, 138, 185,  32,  34, 166 },/*left = dc  */
		{  56,  39,  58, 133, 117, 173,  48,  53, 187 },/*left = v   */
		{  35,  21,  12, 161, 212, 207,  20,  23, 145 },/*left = h   */
		{  56,  29,  19, 117, 109, 181,  55,  68, 112 },/*left = d45 */
		{  47,  29,  17, 153,  64, 220,  59,  51, 114 },/*left = d135*/
		{  46,  16,  24, 136,  76, 147,  41,  64, 172 },/*left = d117*/
		{  34,  17,  11, 108, 152, 187,  13,  15, 209 },/*left = d153*/
		{  51,  24,  14, 115, 133, 209,  32,  26, 104 },/*left = d207*/
		{  55,  30,  18, 122,  79, 179,  44,  88, 116 },/*left = d63 */
		{  37,  49,  25, 129, 168, 164,  41,  54, 148 } /*left = tm  */
	}, {  /* above = d207 */
		{  82,  22,  32, 127, 143, 213,  39,  41,  70 },/*left = dc  */
		{  62,  44,  61, 123, 105, 189,  48,  57,  64 },/*left = v   */
		{  47,  25,  17, 175, 222, 220,  24,  30,  86 },/*left = h   */
		{  68,  36,  17, 106, 102, 206,  59,  74,  74 },/*left = d45 */
		{  57,  39,  23, 151,  68, 216,  55,  63,  58 },/*left = d135*/
		{  49,  30,  35, 141,  70, 168,  82,  40, 115 },/*left = d117*/
		{  51,  25,  15, 136, 129, 202,  38,  35, 139 },/*left = d153*/
		{  68,  26,  16, 111, 141, 215,  29,  28,  28 },/*left = d207*/
		{  59,  39,  19, 114,  75, 180,  77, 104,  42 },/*left = d63 */
		{  40,  61,  26, 126, 152, 206,  61,  59,  93 } /*left = tm  */
	}, {  /* above = d63 */
		{  78,  23,  39, 111, 117, 170,  74, 124,  94 },/*left = dc  */
		{  48,  34,  86, 101,  92, 146,  78, 179, 134 },/*left = v   */
		{  47,  22,  24, 138, 187, 178,  68,  69,  59 },/*left = h   */
		{  56,  25,  33, 105, 112, 187,  95, 177, 129 },/*left = d45 */
		{  48,  31,  27, 114,  63, 183,  82, 116,  56 },/*left = d135*/
		{  43,  28,  37, 121,  63, 123,  61, 192, 169 },/*left = d117*/
		{  42,  17,  24, 109,  97, 177,  56,  76, 122 },/*left = d153*/
		{  58,  18,  28, 105, 139, 182,  70,  92,  63 },/*left = d207*/
		{  46,  23,  32,  74,  86, 150,  67, 183,  88 },/*left = d63 */
		{  36,  38,  48,  92, 122, 165,  88, 137,  91 } /*left = tm  */
	}, {  /* above = tm */
		{  65,  70,  60, 155, 159, 199,  61,  60,  81 },/*left = dc  */
		{  44,  78, 115, 132, 119, 173,  71, 112,  93 },/*left = v   */
		{  39,  38,  21, 184, 227, 206,  42,  32,  64 },/*left = h   */
		{  58,  47,  36, 124, 137, 193,  80,  82,  78 },/*left = d45 */
		{  49,  50,  35, 144,  95, 205,  63,  78,  59 },/*left = d135*/
		{  41,  53,  52, 148,  71, 142,  65, 128,  51 },/*left = d117*/
		{  40,  36,  28, 143, 143, 202,  40,  55, 137 },/*left = d153*/
		{  52,  34,  29, 129, 183, 227,  42,  35,  43 },/*left = d207*/
		{  42,  44,  44, 104, 105, 164,  64, 130,  80 },/*left = d63 */
		{  43,  81,  53, 140, 169, 204,  68,  84,  72 } /*left = tm  */
	}
};

static u8 vp9_kf_partition_probs[PARTITION_CONTEXTS][PARTITION_TYPES - 1] = {
	/* 8x8 -> 4x4 */
	{ 158,  97,  94 },	/* a/l both not split   */
	{  93,  24,  99 },	/* a split, l not split */
	{  85, 119,  44 },	/* l split, a not split */
	{  62,  59,  67 },	/* a/l both split       */
	/* 16x16 -> 8x8 */
	{ 149,  53,  53 },	/* a/l both not split   */
	{  94,  20,  48 },	/* a split, l not split */
	{  83,  53,  24 },	/* l split, a not split */
	{  52,  18,  18 },	/* a/l both split       */
	/* 32x32 -> 16x16 */
	{ 150,  40,  39 },	/* a/l both not split   */
	{  78,  12,  26 },	/* a split, l not split */
	{  67,  33,  11 },	/* l split, a not split */
	{  24,   7,   5 },	/* a/l both split       */
	/* 64x64 -> 32x32 */
	{ 174,  35,  49 },	/* a/l both not split   */
	{  68,  11,  27 },	/* a split, l not split */
	{  57,  15,   9 },	/* l split, a not split */
	{  12,   3,   3 },	/* a/l both split       */
};

static const u8 vp9_kf_uv_mode_prob[INTRA_MODES][INTRA_MODES - 1] = {
	{ 144,  11,  54, 157, 195, 130,  46,  58, 108 },  /* y = dc   */
	{ 118,  15, 123, 148, 131, 101,  44,  93, 131 },  /* y = v    */
	{ 113,  12,  23, 188, 226, 142,  26,  32, 125 },  /* y = h    */
	{ 120,  11,  50, 123, 163, 135,  64,  77, 103 },  /* y = d45  */
	{ 113,   9,  36, 155, 111, 157,  32,  44, 161 },  /* y = d135 */
	{ 116,   9,  55, 176,  76,  96,  37,  61, 149 },  /* y = d117 */
	{ 115,   9,  28, 141, 161, 167,  21,  25, 193 },  /* y = d153 */
	{ 120,  12,  32, 145, 195, 142,  32,  38,  86 },  /* y = d207 */
	{ 116,  12,  64, 120, 140, 125,  49, 115, 121 },  /* y = d63  */
	{ 102,  19,  66, 162, 182, 122,  35,  59, 128 }   /* y = tm   */
};

int rk3399_vdec_vp9d_init(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;
	int ret;

	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->hw.vp9d.priv_tbl,
				sizeof(struct rk3399_vdec_vp9d_priv_tbl));
	if (ret) {
		vpu_err("allocate vp9d priv_tbl failed\n");
		return ret;
	}

	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->hw.vp9d.priv_dst,
					 RKV_VP9D_COUNT_SIZE);
	if (ret) {
		vpu_err("allocate vp9d priv_dst failed\n");
		return ret;
	}

	memset(&ctx->hw.vp9d.last_info, 0, sizeof(ctx->hw.vp9d.last_info));
	ctx->hw.vp9d.mv_base_addr = 0;
	ctx->hw.vp9d.last_info.segmap_idx = true;

	return 0;
}

void rk3399_vdec_vp9d_exit(struct rockchip_vpu_ctx *ctx)
{
	rockchip_vpu_aux_buf_free(ctx->dev, &ctx->hw.vp9d.priv_tbl);
	rockchip_vpu_aux_buf_free(ctx->dev, &ctx->hw.vp9d.priv_dst);
}

static void vp9d_write_coeff_plane(
	const u8 coef[COEF_BANDS][COEFF_CONTEXTS][3],
	u8 *coeff_plane)
{
	s32 k, m, n;
	u8 p;
	u8 byte_count = 0;
	int idx = 0;

	for (k = 0; k < COEF_BANDS; k++) {
		for (m = 0; m < COEFF_CONTEXTS; m++) {
			for (n = 0; n < UNCONSTRAINED_NODES; n++) {
				p = coef[k][m][n];
				coeff_plane[idx++] = p;
				byte_count++;
				if (byte_count == 27) {
					idx += 5;
					byte_count = 0;
				}
			}
		}
	}
}

static void rk3399_vdec_vp9d_output_prob(struct rockchip_vpu_ctx *ctx)
{
	const struct v4l2_ctrl_vp9_entropy *entropy =
		ctx->run.vp9d.entropy;
	const struct v4l2_ctrl_vp9_frame_hdr *frmhdr = ctx->run.vp9d.frame_hdr;
	struct rk3399_vdec_vp9d_priv_tbl *tbl = ctx->hw.vp9d.priv_tbl.cpu;
	u8 p;

	s32 i, j, k, m;
	bool intra_only;

	const struct v4l2_vp9_entropy_ctx *fc;
	struct vp9_decoder_probs_hw *probs =
		(struct vp9_decoder_probs_hw *)tbl->prob_table;

	vpu_debug_enter();

	fc = &entropy->current_entropy_ctx;

	memset(tbl->prob_table, 0, sizeof(tbl->prob_table));

	intra_only = (frmhdr->frame_type == 0) ||
		(frmhdr->flags & V4L2_VP9_FRAME_HDR_FLAG_FRAME_INTRA);

	/* sb info  5 x 128 bit */
	if (intra_only) {
		memcpy(probs->partition_probs, vp9_kf_partition_probs,
			sizeof(probs->partition_probs));
	} else {
		memcpy(probs->partition_probs, fc->partition_probs,
			sizeof(probs->partition_probs));
	}
	memcpy(probs->pred_probs, frmhdr->sgmnt_params.pred_probs,
		sizeof(probs->pred_probs));
	memcpy(probs->tree_probs, frmhdr->sgmnt_params.tree_probs,
		sizeof(probs->tree_probs));
	memcpy(probs->skip_prob, fc->skip_prob, sizeof(probs->skip_prob));
	memcpy(probs->tx_probs_32x32, fc->tx_probs_32x32,
		sizeof(probs->tx_probs_32x32));
	memcpy(probs->tx_probs_16x16, fc->tx_probs_16x16,
		sizeof(probs->tx_probs_16x16));
	memcpy(probs->tx_probs_8x8, fc->tx_probs_8x8,
		sizeof(probs->tx_probs_8x8));
	memcpy(probs->is_inter_prob, fc->is_inter_prob,
		sizeof(probs->is_inter_prob));

	if (intra_only) {
		struct intra_only_frm_spec *probs_intra = &probs->intra_spec;

		/* intra probs */
		/* intra only 149 x 128 bits ,aligned to 152 x 128 bits
		 * coeff related prob 64 x 128 bits
		 */
		for (i = 0; i < TX_SIZES; i++)
			for (j = 0; j < PLANE_TYPES; j++)
				vp9d_write_coeff_plane(
					fc->coef_probs[i][j][0],
					probs_intra->coef_probs_intra[i][j]);

		/* intra mode prob  80 x 128 bits */
		for (i = 0; i < INTRA_MODES; i++) {
			struct intra_mode_prob *intra_mode =
				&probs_intra->intra_mode[i];
			u32 byte_count = 0;
			int idx = 0;

			/* vp9_kf_y_mode_prob */
			for (j = 0; j < INTRA_MODES; j++) {
				for (k = 0; k < INTRA_MODES - 1; k++) {
					p = vp9_kf_y_mode_prob[i][j][k];
					intra_mode->y_mode_prob[idx++] = p;
					byte_count++;
					if (byte_count == 27) {
						byte_count = 0;
						idx += 5;
					}
				}
			}

			idx = 0;
			if (i < 4) {
				for (m = 0; m < (i < 3 ? 23 : 21); m++) {
					u8 *ptr = (u8 *)vp9_kf_uv_mode_prob;

					intra_mode->uv_mode_prob[idx++] =
						ptr[i * 23 + m];
				}
			}
		}
	} else {
		struct inter_frm_spec *probs_inter = &probs->inter_spec;
		/* inter probs
		 * 151 x 128 bits, aligned to 152 x 128 bits
		 * inter only
		 * intra_y_mode & inter_block info 6 x 128 bits
		 */

		memcpy(probs_inter->y_mode_probs, fc->y_mode_probs,
			sizeof(probs_inter->y_mode_probs));
		memcpy(probs_inter->comp_mode_prob, fc->comp_mode_prob,
			sizeof(probs_inter->comp_mode_prob));
		memcpy(probs_inter->comp_ref_prob, fc->comp_ref_prob,
			sizeof(probs_inter->comp_ref_prob));
		memcpy(probs_inter->single_ref_prob, fc->single_ref_prob,
			sizeof(probs_inter->single_ref_prob));
		memcpy(probs_inter->inter_mode_probs, fc->inter_mode_probs,
			sizeof(probs_inter->inter_mode_probs));
		memcpy(probs_inter->interp_filter_probs,
			fc->interp_filter_probs,
			sizeof(probs_inter->interp_filter_probs));

		/* 128 x 128 bits coeff related */
		for (i = 0; i < TX_SIZES; i++)
			for (j = 0; j < PLANE_TYPES; j++)
				vp9d_write_coeff_plane(
					fc->coef_probs[i][j][0],
					probs_inter->coef_probs[0][i][j]);

		for (i = 0; i < TX_SIZES; i++)
			for (j = 0; j < PLANE_TYPES; j++)
				vp9d_write_coeff_plane(
					fc->coef_probs[i][j][1],
					probs_inter->coef_probs[1][i][j]);

		/* intra uv mode 6 x 128 */
		memcpy(probs_inter->uv_mode_prob_0_2, fc->uv_mode_probs,
			sizeof(probs_inter->uv_mode_prob_0_2));
		memcpy(probs_inter->uv_mode_prob_3_5, &fc->uv_mode_probs[3],
			sizeof(probs_inter->uv_mode_prob_3_5));
		memcpy(probs_inter->uv_mode_prob_6_8, &fc->uv_mode_probs[6],
			sizeof(probs_inter->uv_mode_prob_6_8));
		memcpy(probs_inter->uv_mode_prob_9, &fc->uv_mode_probs[9],
			sizeof(probs_inter->uv_mode_prob_9));

		/* mv related 6 x 128 */
		memcpy(probs_inter->mv_joint_probs, fc->mv_joint_probs,
			sizeof(probs_inter->mv_joint_probs));
		memcpy(probs_inter->mv_sign_prob, fc->mv_sign_prob,
			sizeof(probs_inter->mv_sign_prob));
		memcpy(probs_inter->mv_class_probs, fc->mv_class_probs,
			sizeof(probs_inter->mv_class_probs));
		memcpy(probs_inter->mv_class0_bit_prob, fc->mv_class0_bit_prob,
			sizeof(probs_inter->mv_class0_bit_prob));
		memcpy(probs_inter->mv_bits_prob, fc->mv_bits_prob,
			sizeof(probs_inter->mv_bits_prob));
		memcpy(probs_inter->mv_class0_fr_probs, fc->mv_class0_fr_probs,
			sizeof(probs_inter->mv_class0_fr_probs));
		memcpy(probs_inter->mv_fr_probs, fc->mv_fr_probs,
			sizeof(probs_inter->mv_fr_probs));
		memcpy(probs_inter->mv_class0_hp_prob, fc->mv_class0_hp_prob,
			sizeof(probs_inter->mv_class0_hp_prob));
		memcpy(probs_inter->mv_hp_prob, fc->mv_hp_prob,
			sizeof(probs_inter->mv_hp_prob));
	}
}

static void rk3399_vdec_vp9d_prepare_table(struct rockchip_vpu_ctx *ctx)
{
	/*
	 * Prepare auxiliary buffer.
	 */
	rk3399_vdec_vp9d_output_prob(ctx);
}

struct vp9d_ref_config {
	u32 reg_frm_size;
	u32 reg_hor_stride;
	u32 reg_y_stride;
	u32 reg_yuv_stride;
	u32 reg_ref_base;
};

enum FRAME_REF_INDEX {
	REF_INDEX_LAST_FRAME,
	REF_INDEX_GOLDEN_FRAME,
	REF_INDEX_ALTREF_FRAME,
	REF_INDEX_NUM
};

static struct vp9d_ref_config ref_config[REF_INDEX_NUM] = {
	{
		.reg_frm_size = RKVDEC_REG_VP9_FRAME_SIZE(0),
		.reg_hor_stride = RKVDEC_VP9_HOR_VIRSTRIDE(0),
		.reg_y_stride = RKVDEC_VP9_LAST_FRAME_YSTRIDE,
		.reg_yuv_stride = RKVDEC_VP9_LAST_FRAME_YUVSTRIDE,
		.reg_ref_base = RKVDEC_REG_VP9_LAST_FRAME_BASE,
	},
	{
		.reg_frm_size = RKVDEC_REG_VP9_FRAME_SIZE(1),
		.reg_hor_stride = RKVDEC_VP9_HOR_VIRSTRIDE(1),
		.reg_y_stride = RKVDEC_VP9_GOLDEN_FRAME_YSTRIDE,
		.reg_yuv_stride = 0,
		.reg_ref_base = RKVDEC_REG_VP9_GOLDEN_FRAME_BASE,
	},
	{
		.reg_frm_size = RKVDEC_REG_VP9_FRAME_SIZE(2),
		.reg_hor_stride = RKVDEC_VP9_HOR_VIRSTRIDE(2),
		.reg_y_stride = RKVDEC_VP9_ALTREF_FRAME_YSTRIDE,
		.reg_yuv_stride = 0,
		.reg_ref_base = RKVDEC_REG_VP9_ALTREF_FRAME_BASE,
	}
};

static void rk3399_vdec_vp9d_config_registers(struct rockchip_vpu_ctx *ctx)
{
	const struct v4l2_ctrl_vp9_decode_param *dec_param =
		ctx->run.vp9d.dec_param;
	struct v4l2_ctrl_vp9_entropy *entropy =
		ctx->run.vp9d.entropy;
	const struct v4l2_ctrl_vp9_frame_hdr *frmhdr = ctx->run.vp9d.frame_hdr;
	struct rockchip_vpu_vp9d_last_info *last_info =
		&ctx->hw.vp9d.last_info;
	struct rockchip_vpu_dev *vpu = ctx->dev;
	dma_addr_t hw_base;
	u32 reg = 0;
	u32 stream_len = 0;
	u32 aligned_pitch = 0;
	u32 y_len = 0;
	u32 uv_len = 0;
	u32 yuv_len = 0;
	u32 bit_depth;
	u32 align_height;
	s32 i;
	u16 mvscale[3][2];

	bool intra_only = (frmhdr->frame_type == 0) ||
		(frmhdr->flags & V4L2_VP9_FRAME_HDR_FLAG_FRAME_INTRA);

	struct v4l2_vp9_entropy_ctx *fc;

	vpu_debug_enter();

	fc = &entropy->current_entropy_ctx;

	reg = RKVDEC_MODE(RKVDEC_MODE_VP9);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_SYSCTRL);

	bit_depth = frmhdr->bit_depth;
	align_height = round_up(ctx->dst_fmt.height, SB_DIM);

	aligned_pitch = round_up(ctx->dst_fmt.width * bit_depth, 512) / 8;
	y_len = align_height * aligned_pitch;
	uv_len = y_len / 2;
	yuv_len = y_len + uv_len;

	reg = RKVDEC_Y_HOR_VIRSTRIDE(aligned_pitch / 16)
		| RKVDEC_UV_HOR_VIRSTRIDE(aligned_pitch / 16);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_PICPAR);

	reg = RKVDEC_Y_VIRSTRIDE(y_len / 16);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_Y_VIRSTRIDE);

	reg = RKVDEC_YUV_VIRSTRIDE(yuv_len / 16);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_YUV_VIRSTRIDE);

	stream_len = vb2_get_plane_payload(&ctx->run.src->b.vb2_buf, 0);

	reg = RKVDEC_STRM_LEN(stream_len);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_STRM_LEN);

	if (intra_only) {
		/*
		 * reset count buffer, because decoder only output intra
		 * related syntax counts when decoding intra frame,
		 * but update entropy need to update all the probabilities.
		 */
		void *cnt_ptr = ctx->hw.vp9d.priv_dst.cpu;

		memset(cnt_ptr, 0, RKV_VP9D_COUNT_SIZE);
	}

	if (!intra_only && !(frmhdr->flags &
			     V4L2_VP9_FRAME_HDR_FLAG_ERR_RES))
		last_info->mv_base_addr = ctx->hw.vp9d.mv_base_addr;

	/* store mv base address, before yuv_virstride change */
	ctx->hw.vp9d.mv_base_addr =
		vb2_dma_contig_plane_dma_addr(&ctx->run.dst->b.vb2_buf, 0) +
		yuv_len;

	for (i = 0; i < REF_INDEX_NUM; i++) {
		u32 ref_idx;
		u32 width;
		u32 height;
		struct vb2_buffer *buf;

		ref_idx = dec_param->active_ref_frames[i].buf_index;

		if (ref_idx >= ctx->vq_dst.num_buffers) {
			buf = &ctx->run.dst->b.vb2_buf;
			vdpu_write_relaxed(
				vpu,
				vb2_dma_contig_plane_dma_addr(buf, 0),
				ref_config[i].reg_ref_base);

			reg = RKVDEC_VP9_FRAMEWIDTH(frmhdr->frame_width)
				| RKVDEC_VP9_FRAMEHEIGHT(frmhdr->frame_height);
			vdpu_write_relaxed(vpu, reg,
					   ref_config[i].reg_frm_size);
			continue;
		}

		width = dec_param->active_ref_frames[i].frame_width;
		height = dec_param->active_ref_frames[i].frame_height;
		buf = ctx->dst_bufs[ref_idx];

		reg = RKVDEC_VP9_FRAMEWIDTH(width)
			| RKVDEC_VP9_FRAMEHEIGHT(height);
		vdpu_write_relaxed(vpu, reg, ref_config[i].reg_frm_size);

		align_height = round_up(height, SB_DIM);
		aligned_pitch = round_up(width * bit_depth, 512) / 8;

		y_len = aligned_pitch * align_height;
		uv_len = y_len / 2;
		yuv_len = y_len + uv_len;

		reg = RKVDEC_HOR_Y_VIRSTRIDE(aligned_pitch / 16)
			| RKVDEC_HOR_UV_VIRSTRIDE(aligned_pitch / 16);
		vdpu_write_relaxed(vpu, reg, ref_config[i].reg_hor_stride);

		reg = RKVDEC_VP9_REF_YSTRIDE(y_len / 16);
		vdpu_write_relaxed(vpu, reg, ref_config[i].reg_y_stride);

		reg = RKVDEC_VP9_REF_YUVSTRIDE(yuv_len / 16);
		if (ref_config[i].reg_yuv_stride)
			vdpu_write_relaxed(vpu, reg,
					   ref_config[i].reg_yuv_stride);

		vdpu_write_relaxed(vpu, vb2_dma_contig_plane_dma_addr(buf, 0),
			ref_config[i].reg_ref_base);
	}

	for (i = 0; i < 8; i++) {
		reg = RKVDEC_SEGID_FRAME_QP_DELTA_EN(
			!!(last_info->feature_mask[i] & 1));
		reg |= RKVDEC_SEGID_FRAME_QP_DELTA(
			last_info->feature_data[i][0]);
		reg |= RKVDEC_SEGID_FRAME_LOOPFILTER_VALUE_EN(
			!!(last_info->feature_mask[i] & 2));
		reg |= RKVDEC_SEGID_FRAME_LOOPFILTER_VALUE(
			last_info->feature_data[i][1]);
		reg |= RKVDEC_SEGID_REFERINFO_EN(
			!!(last_info->feature_mask[i] & 4));
		reg |= RKVDEC_SEGID_REFERINFO(last_info->feature_data[i][2]);
		reg |= RKVDEC_SEGID_FRAME_SKIP_EN(
			!!(last_info->feature_mask[i] & 8));
		reg |= RKVDEC_SEGID_ABS_DELTA(
			!!(i == 0 && last_info->abs_delta));
		vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_SEGID_GRP(i));
	}

	reg = RKVDEC_VP9_TX_MODE(entropy->tx_mode)
		| RKVDEC_VP9_FRAME_REF_MODE(entropy->reference_mode);
	vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_CPRHEADER_CONFIG);

	reg = 0;
	if (intra_only) {
		last_info->segmentation_enable = false;
		last_info->intra_only = true;
	} else {
		reg = RKVDEC_REF_DELTAS0_LASTFRAME(last_info->ref_deltas[0]);
		reg |= RKVDEC_REF_DELTAS1_LASTFRAME(last_info->ref_deltas[1]);
		reg |= RKVDEC_REF_DELTAS2_LASTFRAME(last_info->ref_deltas[2]);
		reg |= RKVDEC_REF_DELTAS3_LASTFRAME(last_info->ref_deltas[3]);
		vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_REF_DELTAS_LASTFRAME);

		reg = RKVDEC_MODE_DELTAS0_LASTFRAME(last_info->mode_deltas[0]);
		reg |= RKVDEC_MODE_DELTAS1_LASTFRAME(last_info->mode_deltas[1]);
		vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_INFO_LASTFRAME);
	}

	if (last_info->segmentation_enable)
		reg |= RKVDEC_SEG_EN_LASTFRAME;
	if (last_info->show_frame)
		reg |= RKVDEC_LAST_SHOW_FRAME;
	if (last_info->intra_only)
		reg |= RKVDEC_LAST_INTRA_ONLY;
	if (frmhdr->frame_width == last_info->width &&
	    frmhdr->frame_height == last_info->height)
		reg |= RKVDEC_LAST_WIDHHEIGHT_EQCUR;
	vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_INFO_LASTFRAME);

	reg = stream_len - frmhdr->header_size_in_bytes;
	vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_LASTTILE_SIZE);

	for (i = 0; i < 3; i++) {
		u32 refw = dec_param->active_ref_frames[i].frame_width;
		u32 refh = dec_param->active_ref_frames[i].frame_height;

		mvscale[i][0] = (refw << 14) / frmhdr->frame_width;
		mvscale[i][1] = (refh << 14) / frmhdr->frame_height;
	}

	if (!intra_only) {
		/* last frame */
		reg = RKVDEC_VP9_REF_HOR_SCALE(mvscale[0][0])
			| RKVDEC_VP9_REF_VER_SCALE(mvscale[0][1]);
		vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_REF_SCALE(0));
		/* golden frame */
		reg = RKVDEC_VP9_REF_HOR_SCALE(mvscale[1][0])
			| RKVDEC_VP9_REF_VER_SCALE(mvscale[1][1]);
		vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_REF_SCALE(1));
		/* altref frame */
		reg = RKVDEC_VP9_REF_HOR_SCALE(mvscale[2][0])
			| RKVDEC_VP9_REF_VER_SCALE(mvscale[2][1]);
		vdpu_write_relaxed(vpu, reg, RKVDEC_VP9_REF_SCALE(2));
	}

	hw_base = vb2_dma_contig_plane_dma_addr(&ctx->run.dst->b.vb2_buf, 0);
	vdpu_write_relaxed(vpu, hw_base, RKVDEC_REG_DECOUT_BASE);

	hw_base = vb2_dma_contig_plane_dma_addr(&ctx->run.src->b.vb2_buf, 0);
	vdpu_write_relaxed(vpu, hw_base, RKVDEC_REG_STRM_RLC_BASE);

	hw_base = ctx->hw.vp9d.priv_tbl.dma +
		offsetof(struct rk3399_vdec_vp9d_priv_tbl, prob_table);
	vdpu_write_relaxed(vpu, hw_base, RKVDEC_REG_CABACTBL_PROB_BASE);

	hw_base = ctx->hw.vp9d.priv_dst.dma;
	vdpu_write_relaxed(vpu, hw_base, RKVDEC_REG_VP9COUNT_BASE);

	hw_base = ctx->hw.vp9d.priv_tbl.dma +
		offsetof(struct rk3399_vdec_vp9d_priv_tbl,
			 segmap[!last_info->segmap_idx]);
	vdpu_write_relaxed(vpu, hw_base, RKVDEC_REG_VP9_SEGIDCUR_BASE);
	hw_base = ctx->hw.vp9d.priv_tbl.dma +
		offsetof(struct rk3399_vdec_vp9d_priv_tbl,
			 segmap[last_info->segmap_idx]);
	vdpu_write_relaxed(vpu, hw_base, RKVDEC_REG_VP9_SEGIDLAST_BASE);

	if ((frmhdr->sgmnt_params.flags &
	     V4L2_VP9_SGMNT_PARAM_FLAG_ENABLED) &&
	    (frmhdr->sgmnt_params.flags &
	     V4L2_VP9_SGMNT_PARAM_FLAG_UPDATE_MAP))
		last_info->segmap_idx = !last_info->segmap_idx;

	if (!last_info->mv_base_addr)
		last_info->mv_base_addr = ctx->hw.vp9d.mv_base_addr;

	vdpu_write_relaxed(vpu, last_info->mv_base_addr,
			   RKVDEC_VP9_REF_COLMV_BASE);

	vdpu_write_relaxed(vpu, ctx->dst_fmt.width |
				(ctx->dst_fmt.height << 16),
			   RKVDEC_REG_PERFORMANCE_CYCLE);
}

void rk3399_vdec_vp9d_run(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;

	/* Prepare data in memory. */
	rk3399_vdec_vp9d_prepare_table(ctx);

	rockchip_vpu_power_on(vpu);

	/* Configure hardware registers. */
	rk3399_vdec_vp9d_config_registers(ctx);

	schedule_delayed_work(&vpu->watchdog_work, msecs_to_jiffies(2000));

	vdpu_write(vpu, 1, RKVDEC_REG_PREF_LUMA_CACHE_COMMAND);
	vdpu_write(vpu, 1, RKVDEC_REG_PREF_CHR_CACHE_COMMAND);

	/* Start decoding! */
	vdpu_write(vpu,
		   RKVDEC_INTERRUPT_DEC_E |
		   RKVDEC_CONFIG_DEC_CLK_GATE_E |
		   RKVDEC_TIMEOUT_E,
		   RKVDEC_REG_INTERRUPT);
}

static u8 adapt_prob(u8 p1, u32 ct0, u32 ct1,
		     s32 max_count, s32 update_factor)
{
	u32 ct = ct0 + ct1, p2;
	u32 lo = 1;
	u32 hi = 255;

	if (!ct)
		return p1;

	p2 = ((ct0 << 8) + (ct >> 1)) / ct;
	p2 = clamp(p2, lo, hi);
	ct = min_t(u32, ct, max_count);

	if (WARN_ON(max_count >= 257))
		return p1;

	update_factor = FASTDIV(update_factor * ct, max_count);

	return p1 + (((p2 - p1) * update_factor + 128) >> 8);
}

#define BAND_COEFF_CONTEXTS(band) ((band) == 0 ? 3 : COEFF_CONTEXTS)

static void vp9d_adapt_coeff(
	u8 coef_probs[COEF_BANDS][COEFF_CONTEXTS][3],
	u8 pre_coef_probs[COEF_BANDS][COEFF_CONTEXTS][3],
	struct refs_counts ref_cnt[COEF_BANDS][COEFF_CONTEXTS], s32 uf)
{
	s32 l, m, n;

	for (l = 0; l < COEF_BANDS; l++) {
		for (m = 0; m < BAND_COEFF_CONTEXTS(l); m++) {
			u8 *pp = pre_coef_probs[l][m];
			u8 *p = coef_probs[l][m];
			const u32 n0 = ref_cnt[l][m].coeff[0];
			const u32 n1 = ref_cnt[l][m].coeff[1];
			const u32 n2 = ref_cnt[l][m].coeff[2];
			const u32 neob = ref_cnt[l][m].eob[1];
			const u32 eob_count = ref_cnt[l][m].eob[0];

			const u32 branch_ct[UNCONSTRAINED_NODES][2] = {
				{ neob, eob_count - neob },
				{ n0, n1 + n2 },
				{ n1, n2 }
			};
			for (n = 0; n < UNCONSTRAINED_NODES; n++)
				p[n] = adapt_prob(pp[n], branch_ct[n][0],
						  branch_ct[n][1], 24, uf);
		}
	}
}

static int intra_mode_map[10] = {
	1, 2, 0, 3, 4, 5, 6, 8, 7, 9
};

static void adapt_probs(struct rockchip_vpu_ctx *ctx, u8 *count_info)
{
	s32 i, j, k;
	const struct v4l2_ctrl_vp9_frame_hdr *frmhdr = ctx->run.vp9d.frame_hdr;
	struct v4l2_vp9_entropy_ctx *fc =
		&ctx->run.vp9d.entropy->current_entropy_ctx;
	struct v4l2_vp9_entropy_ctx *pre_fc =
		&ctx->run.vp9d.entropy->initial_entropy_ctx;
	struct rockchip_vpu_vp9d_last_info *last_info = &ctx->hw.vp9d.last_info;
	bool intra_only = (frmhdr->frame_type == 0) ||
			  (frmhdr->flags & V4L2_VP9_FRAME_HDR_FLAG_FRAME_INTRA);
	s32 uf = (intra_only || last_info->frame_type != 0) ? 112 : 128;
	struct refs_counts (*ref_cnt)[REF_TYPES][TX_SIZES][PLANE_TYPES]
				     [COEF_BANDS][COEFF_CONTEXTS];

	union rkv_vp9_symbol_counts *counts =
		(union rkv_vp9_symbol_counts *)count_info;

	struct symbol_counts_for_inter_frame *interc = &counts->inter_spec;

	if (intra_only)
		ref_cnt = &counts->intra_spec.ref_cnt;
	else
		ref_cnt = &counts->inter_spec.ref_cnt;


	/* coefficients */
	for (i = 0; i < TX_SIZES; i++) {
		for (j = 0; j < PLANE_TYPES; j++) {
			for (k = 0; k < REF_TYPES; k++) {
				vp9d_adapt_coeff(fc->coef_probs[i][j][k],
						 pre_fc->coef_probs[i][j][k],
						 (*ref_cnt)[k][i][j],
						 uf);
			}
		}
	}

	if (intra_only)
		return;

	/* skip flag */
	for (i = 0; i < 3; i++)
		fc->skip_prob[i] = adapt_prob(pre_fc->skip_prob[i],
					      interc->skip[i][0],
					      interc->skip[i][1], 20, 128);

	/* intra/inter flag */
	for (i = 0; i < 4; i++)
		fc->is_inter_prob[i] = adapt_prob(pre_fc->is_inter_prob[i],
						  interc->inter[i][0],
						  interc->inter[i][1], 20, 128);

	/* comppred flag */
	for (i = 0; i < 5; i++)
		fc->comp_mode_prob[i] =
			adapt_prob(pre_fc->comp_mode_prob[i],
				   interc->comp[i][0],
				   interc->comp[i][1], 20, 128);

	/* reference frames */
	for (i = 0; i < 5; i++)
		fc->comp_ref_prob[i] =
			adapt_prob(pre_fc->comp_ref_prob[i],
				   interc->comp_ref[i][0],
				   interc->comp_ref[i][1], 20, 128);

	if (ctx->run.vp9d.entropy->reference_mode != PRED_COMPREF) {
		for (i = 0; i < 5; i++) {
			u8 *pp = pre_fc->single_ref_prob[i];
			u8 *p = fc->single_ref_prob[i];
			u32(*c)[2] = interc->single_ref[i];

			p[0] = adapt_prob(pp[0], c[0][0], c[0][1], 20, 128);
			p[1] = adapt_prob(pp[1], c[1][0], c[1][1], 20, 128);
		}
	}

	/* block partitioning */
	for (i = 0; i < PARTITION_CONTEXTS; i++) {
		u8 *pp = pre_fc->partition_probs[i];
		u8 *p = fc->partition_probs[i];
		u32 *c = interc->partition[i];

		p[0] = adapt_prob(pp[0], c[0], c[1] + c[2] + c[3], 20,
				  128);
		p[1] = adapt_prob(pp[1], c[1], c[2] + c[3], 20, 128);
		p[2] = adapt_prob(pp[2], c[2], c[3], 20, 128);
	}

	/* tx size */
	if (ctx->run.vp9d.entropy->tx_mode == TX_SWITCHABLE) {
		for (i = 0; i < 2; i++) {
			u32 *c16 = interc->tx16p[i], *c32 = interc->tx32p[i];

			fc->tx_probs_8x8[i][0] =
				adapt_prob(pre_fc->tx_probs_8x8[i][0],
					   interc->tx8p[i][0],
					   interc->tx8p[i][1], 20, 128);
			fc->tx_probs_16x16[i][0] =
				adapt_prob(pre_fc->tx_probs_16x16[i][0],
					   c16[0], c16[1] + c16[2], 20, 128);
			fc->tx_probs_16x16[i][1] =
				adapt_prob(pre_fc->tx_probs_16x16[i][1],
					   c16[1], c16[2], 20, 128);
			fc->tx_probs_32x32[i][0] =
				adapt_prob(pre_fc->tx_probs_32x32[i][0], c32[0],
					   c32[1] + c32[2] + c32[3], 20, 128);
			fc->tx_probs_32x32[i][1] =
				adapt_prob(pre_fc->tx_probs_32x32[i][1], c32[1],
					   c32[2] + c32[3], 20, 128);
			fc->tx_probs_32x32[i][2] =
				adapt_prob(pre_fc->tx_probs_32x32[i][2], c32[2],
					   c32[3], 20, 128);
		}
	}

	/* interpolation filter */
	if (frmhdr->interpolation_filter == FILTER_SWITCHABLE) {
		for (i = 0; i < 4; i++) {
			u8 *pp = pre_fc->interp_filter_probs[i];
			u8 *p = fc->interp_filter_probs[i];
			u32 *c = interc->filter[i];

			p[0] = adapt_prob(pp[0], c[0], c[1] + c[2], 20, 128);
			p[1] = adapt_prob(pp[1], c[1], c[2], 20, 128);
		}
	}

	/* inter modes */
	for (i = 0; i < 7; i++) {
		u8 *pp = pre_fc->inter_mode_probs[i];
		u8 *p = fc->inter_mode_probs[i];
		u32 *c = interc->mv_mode[i];

		p[0] = adapt_prob(pp[0], c[2], c[1] + c[0] + c[3], 20, 128);
		p[1] = adapt_prob(pp[1], c[0], c[1] + c[3], 20, 128);
		p[2] = adapt_prob(pp[2], c[1], c[3], 20, 128);
	}

	/* mv joints */
	{
		u8 *pp = pre_fc->mv_joint_probs;
		u8 *p = fc->mv_joint_probs;
		u32 *c = interc->mv_joint;

		p[0] = adapt_prob(pp[0], c[0], c[1] + c[2] + c[3], 20, 128);
		p[1] = adapt_prob(pp[1], c[1], c[2] + c[3], 20, 128);
		p[2] = adapt_prob(pp[2], c[2], c[3], 20, 128);
	}

	/* mv components */
	for (i = 0; i < 2; i++) {
		u8 *pp = pre_fc->mv_sign_prob;
		u8 *p = fc->mv_sign_prob;
		u32 *c, (*c2)[2], sum;

		p[i] = adapt_prob(pp[i], interc->sign[i][0],
				  interc->sign[i][1], 20, 128);

		pp = pre_fc->mv_class_probs[i];
		p = fc->mv_class_probs[i];
		c = interc->classes[i];
		sum = c[1] + c[2] + c[3] + c[4] + c[5] +
			c[6] + c[7] + c[8] + c[9] + c[10];
		p[0] = adapt_prob(pp[0], c[0], sum, 20, 128);
		sum -= c[1];
		p[1] = adapt_prob(pp[1], c[1], sum, 20, 128);
		sum -= c[2] + c[3];
		p[2] = adapt_prob(pp[2], c[2] + c[3], sum, 20, 128);
		p[3] = adapt_prob(pp[3], c[2], c[3], 20, 128);
		sum -= c[4] + c[5];
		p[4] = adapt_prob(pp[4], c[4] + c[5], sum, 20, 128);
		p[5] = adapt_prob(pp[5], c[4], c[5], 20, 128);
		sum -= c[6];
		p[6] = adapt_prob(pp[6], c[6], sum, 20, 128);
		p[7] = adapt_prob(pp[7], c[7] + c[8], c[9] + c[10], 20, 128);
		p[8] = adapt_prob(pp[8], c[7], c[8], 20, 128);
		p[9] = adapt_prob(pp[9], c[9], c[10], 20, 128);

		pp = pre_fc->mv_class0_bit_prob;
		p = fc->mv_class0_bit_prob;
		p[i] = adapt_prob(pp[i],
				  interc->class0[i][0],
				  interc->class0[i][1], 20, 128);

		pp = pre_fc->mv_bits_prob[i];
		p = fc->mv_bits_prob[i];
		c2 = interc->bits[i];
		for (j = 0; j < 10; j++)
			p[j] = adapt_prob(pp[j], c2[j][0], c2[j][1], 20, 128);

		for (j = 0; j < 2; j++) {
			pp = pre_fc->mv_class0_fr_probs[i][j];
			p = fc->mv_class0_fr_probs[i][j];
			c = interc->class0_fp[i][j];
			p[0] = adapt_prob(pp[0], c[0], c[1] + c[2] + c[3],
					  20, 128);
			p[1] = adapt_prob(pp[1], c[1], c[2] + c[3], 20, 128);
			p[2] = adapt_prob(pp[2], c[2], c[3], 20, 128);
		}

		pp = pre_fc->mv_fr_probs[i];
		p = fc->mv_fr_probs[i];
		c = interc->fp[i];
		p[0] = adapt_prob(pp[0], c[0], c[1] + c[2] + c[3], 20, 128);
		p[1] = adapt_prob(pp[1], c[1], c[2] + c[3], 20, 128);
		p[2] = adapt_prob(pp[2], c[2], c[3], 20, 128);

		if (frmhdr->flags & V4L2_VP9_FRAME_HDR_ALLOW_HIGH_PREC_MV) {
			pp = pre_fc->mv_class0_hp_prob;
			p = fc->mv_class0_hp_prob;
			p[i] = adapt_prob(pp[i],
					  interc->class0_hp[i][0],
					  interc->class0_hp[i][1], 20, 128);

			pp = pre_fc->mv_hp_prob;
			p = fc->mv_hp_prob;
			p[i] = adapt_prob(pp[i], interc->hp[i][0],
					  interc->hp[i][1], 20, 128);
		}
	}

	/* The hardware output count order is rather different from syntax,
	 * the following map is to translate the hardware output count to syntax
	 * need.
	 *    syntax              hardware
	 *+++++ v   ++++*     *++++ dc   ++++*
	 *+++++ h   ++++*     *++++ v   ++++*
	 *+++++ dc  ++++*     *++++ h  ++++*
	 *+++++ d45 ++++*     *++++ d45 ++++*
	 *+++++ d135++++*     *++++ d135++++*
	 *+++++ d117++++*     *++++ d117++++*
	 *+++++ d153++++*     *++++ d153++++*
	 *+++++ d63 ++++*     *++++ d207++++*
	 *+++++ d207 ++++*    *++++ d63 ++++*
	 *+++++ tm  ++++*     *++++ tm  ++++*
	 */

	/* y intra modes */
	for (i = 0; i < 4; i++) {
		u8 *pp = pre_fc->y_mode_probs[i];
		u8 *p = fc->y_mode_probs[i];
		u32 *c = interc->y_mode[i], sum, s2;

		sum = c[intra_mode_map[0]] + c[intra_mode_map[1]] +
			c[intra_mode_map[3]] + c[intra_mode_map[4]] +
			c[intra_mode_map[5]] + c[intra_mode_map[6]] +
			c[intra_mode_map[7]] + c[intra_mode_map[8]] +
			c[intra_mode_map[9]];
		p[0] = adapt_prob(pp[0], c[intra_mode_map[DC_PRED]],
				  sum, 20, 128);
		sum -= c[intra_mode_map[TM_VP8_PRED]];
		p[1] = adapt_prob(pp[1], c[intra_mode_map[TM_VP8_PRED]], sum,
				  20, 128);
		sum -= c[intra_mode_map[VERT_PRED]];
		p[2] = adapt_prob(pp[2], c[intra_mode_map[VERT_PRED]], sum,
				  20, 128);
		s2 = c[intra_mode_map[HOR_PRED]] +
			c[intra_mode_map[DIAG_DOWN_RIGHT_PRED]] +
			c[intra_mode_map[VERT_RIGHT_PRED]];
		sum -= s2;
		p[3] = adapt_prob(pp[3], s2, sum, 20, 128);
		s2 -= c[intra_mode_map[HOR_PRED]];
		p[4] = adapt_prob(pp[4], c[intra_mode_map[HOR_PRED]], s2,
				  20, 128);
		p[5] = adapt_prob(pp[5],
				  c[intra_mode_map[DIAG_DOWN_RIGHT_PRED]],
				  c[intra_mode_map[VERT_RIGHT_PRED]], 20, 128);
		sum -= c[intra_mode_map[DIAG_DOWN_LEFT_PRED]];
		p[6] = adapt_prob(pp[6], c[intra_mode_map[DIAG_DOWN_LEFT_PRED]],
				  sum, 20, 128);
		sum -= c[intra_mode_map[VERT_LEFT_PRED]];
		p[7] = adapt_prob(pp[7], c[intra_mode_map[VERT_LEFT_PRED]], sum,
				  20, 128);
		p[8] = adapt_prob(pp[8], c[intra_mode_map[HOR_DOWN_PRED]],
				  c[intra_mode_map[HOR_UP_PRED]], 20, 128);
	}

	/* uv intra modes */
	for (i = 0; i < 10; i++) {
		u8 *pp = pre_fc->uv_mode_probs[i];
		u8 *p = fc->uv_mode_probs[i];
		u32 *c = interc->uv_mode[i], sum, s2;

		sum = c[intra_mode_map[0]] + c[intra_mode_map[1]] +
			c[intra_mode_map[3]] + c[intra_mode_map[4]] +
			c[intra_mode_map[5]] + c[intra_mode_map[6]] +
			c[intra_mode_map[7]] + c[intra_mode_map[8]] +
			c[intra_mode_map[9]];
		p[0] = adapt_prob(pp[0], c[intra_mode_map[DC_PRED]],
				  sum, 20, 128);
		sum -= c[intra_mode_map[TM_VP8_PRED]];
		p[1] = adapt_prob(pp[1], c[intra_mode_map[TM_VP8_PRED]], sum,
				  20, 128);
		sum -= c[intra_mode_map[VERT_PRED]];
		p[2] = adapt_prob(pp[2], c[intra_mode_map[VERT_PRED]], sum,
				  20, 128);
		s2 = c[intra_mode_map[HOR_PRED]] +
			c[intra_mode_map[DIAG_DOWN_RIGHT_PRED]] +
			c[intra_mode_map[VERT_RIGHT_PRED]];
		sum -= s2;
		p[3] = adapt_prob(pp[3], s2, sum, 20, 128);
		s2 -= c[intra_mode_map[HOR_PRED]];
		p[4] = adapt_prob(pp[4], c[intra_mode_map[HOR_PRED]],
				  s2, 20, 128);
		p[5] = adapt_prob(pp[5],
				  c[intra_mode_map[DIAG_DOWN_RIGHT_PRED]],
				  c[intra_mode_map[VERT_RIGHT_PRED]], 20, 128);
		sum -= c[intra_mode_map[DIAG_DOWN_LEFT_PRED]];
		p[6] = adapt_prob(pp[6], c[intra_mode_map[DIAG_DOWN_LEFT_PRED]],
				  sum, 20, 128);
		sum -= c[intra_mode_map[VERT_LEFT_PRED]];
		p[7] = adapt_prob(pp[7], c[intra_mode_map[VERT_LEFT_PRED]], sum,
				  20, 128);
		p[8] = adapt_prob(pp[8], c[intra_mode_map[HOR_DOWN_PRED]],
				  c[intra_mode_map[HOR_UP_PRED]], 20, 128);
	}
}

static void vp9d_probs_update(struct rockchip_vpu_ctx *ctx,
			      u8 *count_info, u32 len)
{
	const struct v4l2_ctrl_vp9_frame_hdr *frmhdr = ctx->run.vp9d.frame_hdr;

	if (!(frmhdr->flags & V4L2_VP9_FRAME_HDR_FLAG_ERR_RES) &&
	    !(frmhdr->flags & V4L2_VP9_FRAME_HDR_PARALLEL_DEC_MODE))
		adapt_probs(ctx, count_info);

	if (frmhdr->flags & V4L2_VP9_FRAME_HDR_REFRESH_FRAME_CTX) {
		struct v4l2_ctrl_vp9_entropy *entropy =
			ctx->run.vp9d.entropy;
		struct v4l2_ext_controls cs;

		memset(&cs, 0, sizeof(cs));

		cs.config_store = ctx->run.src->b.config_store;
		cs.count = 1;
		cs.controls = vmalloc(sizeof(struct v4l2_ext_control));
		if (!cs.controls) {
			vpu_err("no mem for controls\n");
			return;
		}

		cs.controls[0].id = V4L2_CID_MPEG_VIDEO_VP9_ENTROPY;
		cs.controls[0].ptr = entropy;
		cs.controls[0].size = sizeof(*entropy);

		if (v4l2_s_ext_ctrls(&ctx->fh, &ctx->ctrl_handler, &cs) < 0)
			vpu_err("try to store the adapted entropy failed\n");

		vfree(cs.controls);
	}
}

void rk3399_vdec_vp9d_done(struct rockchip_vpu_ctx *ctx,
		      enum vb2_buffer_state result)
{
	struct rockchip_vpu_vp9d_last_info *last_info =
			&ctx->hw.vp9d.last_info;
	const struct v4l2_ctrl_vp9_frame_hdr *frmhdr = ctx->run.vp9d.frame_hdr;
	u32 i;

	vpu_debug_enter();

	vp9d_probs_update(ctx,
			  ctx->hw.vp9d.priv_dst.cpu,
			  ctx->hw.vp9d.priv_dst.size);

	last_info->abs_delta = frmhdr->sgmnt_params.flags &
		V4L2_VP9_SGMNT_PARAM_FLAG_ABS_OR_DELTA_UPDATE;

	for (i = 0 ; i < 4; i++)
		last_info->ref_deltas[i] = frmhdr->lf_params.deltas[i];

	for (i = 0 ; i < 2; i++)
		last_info->mode_deltas[i] = frmhdr->lf_params.mode_deltas[i];

	for (i = 0; i < 8; i++) {
		last_info->feature_data[i][0] =
			frmhdr->sgmnt_params.feature_data[i][0];
		last_info->feature_data[i][1] =
			frmhdr->sgmnt_params.feature_data[i][1];
		last_info->feature_data[i][2] =
			frmhdr->sgmnt_params.feature_data[i][2];
		last_info->feature_data[i][3] =
			frmhdr->sgmnt_params.feature_data[i][3];
		last_info->feature_mask[i] =
			frmhdr->sgmnt_params.feature_enabled[i][0] |
			(frmhdr->sgmnt_params.feature_enabled[i][1] << 1) |
			(frmhdr->sgmnt_params.feature_enabled[i][2] << 2) |
			(frmhdr->sgmnt_params.feature_enabled[i][3] << 3);
	}

	last_info->segmentation_enable |=
		frmhdr->sgmnt_params.flags & V4L2_VP9_SGMNT_PARAM_FLAG_ENABLED;

	last_info->show_frame = frmhdr->flags &
		V4L2_VP9_FRAME_HDR_FLAG_SHOW_FRAME;
	last_info->width = frmhdr->frame_width;
	last_info->height = frmhdr->frame_height;
	last_info->intra_only = (frmhdr->frame_type == 0) ||
		(frmhdr->flags & V4L2_VP9_FRAME_HDR_FLAG_FRAME_INTRA);
	last_info->frame_type = frmhdr->frame_type;

	rockchip_vpu_run_done(ctx, result);

	vpu_debug_leave();
}
