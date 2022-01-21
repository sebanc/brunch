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

#include "rockchip_vpu_common.h"

#include <linux/types.h>
#include <linux/sort.h>

#include "rk3399_vpu_regs.h"
#include "rockchip_vpu_hw.h"

/* Various parameters specific to VP8 encoder. */
#define VP8_CABAC_CTX_OFFSET	192
#define VP8_CABAC_CTX_SIZE	((55 + 96) << 3)
/* ((1920 * 1088 * 4 / 256 + 63) / 64 * 8) */
#define VP8_SEGMENT_MAP_SIZE	4087

/* threshold of MBs count to disable quarter pixel mv for encode speed */
#define MAX_MB_COUNT_TO_DISABLE_QUARTER_PIXEL_MV	3600

/* threshold of MBs count to disable multi mv in one macro block */
#define MAX_MB_COUNT_TO_DISABLE_SPLIT_MV		1584

#define QINDEX_RANGE 128

/* experimentally fitted, 24.893*exp(0.02545*qp) */
static const int32_t split_penalty[QINDEX_RANGE] = {
	24, 25, 26, 26, 27, 28, 29, 29,
	30, 31, 32, 32, 33, 34, 35, 36,
	37, 38, 39, 40, 41, 42, 43, 44,
	45, 47, 48, 49, 50, 52, 53, 54,
	56, 57, 59, 60, 62, 63, 65, 67,
	68, 70, 72, 74, 76, 78, 80, 82,
	84, 86, 88, 91, 93, 95, 98, 100,
	103, 106, 108, 111, 114, 117, 120, 123,
	126, 130, 133, 136, 140, 144, 147, 151,
	155, 159, 163, 167, 172, 176, 181, 185,
	190, 195, 200, 205, 211, 216, 222, 227,
	233, 239, 245, 252, 258, 265, 272, 279,
	286, 293, 301, 309, 317, 325, 333, 342,
	351, 360, 369, 379, 388, 398, 409, 419,
	430, 441, 453, 464, 476, 488, 501, 514,
	527, 541, 555, 569, 584, 599, 614, 630
};

static const int dc_q_lookup[QINDEX_RANGE] = {
	4,   5,   6,   7,   8,   9,   10,  10,  11,  12,
	13,  14,  15,  16,  17,  17,  18,  19,  20,  20,
	21,  21,  22,  22,  23,  23,  24,  25,  25,  26,
	27,  28,  29,  30,  31,  32,  33,  34,  35,  36,
	37,  37,  38,  39,  40,  41,  42,  43,  44,  45,
	46,  46,  47,  48,  49,  50,  51,  52,  53,  54,
	55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
	65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
	75,  76,  76,  77,  78,  79,  80,  81,  82,  83,
	84,  85,  86,  87,  88,  89,  91,  93,  95,  96,
	98,  100, 101, 102, 104, 106, 108, 110, 112, 114,
	116, 118, 122, 124, 126, 128, 130, 132, 134, 136,
	138, 140, 143, 145, 148, 151, 154, 157
};

static const int ac_q_lookup[QINDEX_RANGE] = {
	4,   5,   6,   7,   8,   9,   10,  11,  12,  13,
	14,  15,  16,  17,  18,  19,  20,  21,  22,  23,
	24,  25,  26,  27,  28,  29,  30,  31,  32,  33,
	34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
	44,  45,  46,  47,  48,  49,  50,  51,  52,  53,
	54,  55,  56,  57,  58,  60,  62,  64,  66,  68,
	70,  72,  74,  76,  78,  80,  82,  84,  86,  88,
	90,  92,  94,  96,  98,  100, 102, 104, 106, 108,
	110, 112, 114, 116, 119, 122, 125, 128, 131, 134,
	137, 140, 143, 146, 149, 152, 155, 158, 161, 164,
	167, 170, 173, 177, 181, 185, 189, 193, 197, 201,
	205, 209, 213, 217, 221, 225, 229, 234, 239, 245,
	249, 254, 259, 264, 269, 274, 279, 284
};

static const int32_t const qrounding_factors[QINDEX_RANGE] = {
	56, 56, 56, 56, 56, 56, 56, 56, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48
};

static const int32_t const qzbin_factors[QINDEX_RANGE] = {
	64, 64, 64, 64, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80
};

static const int32_t zbin_boost[16] = {
	0,   0,  8, 10, 12, 14, 16, 20, 24, 28,
	32, 36, 40, 44, 44, 44
};

/* Intra 16x16 mode tree penalty values */
static s32 intra_16_tree_penalty[] = {
	305, 841, 914, 1082
};


/* Intra 4x4 mode tree penalty values */
static s32 intra_4_tree_penalty[] = {
	280, 622, 832, 1177, 1240, 1341, 1085, 1259, 1357, 1495
};

/* ~round((2*(2+exp((x+22)/39)) + (2+exp((x+15)/32)))/3) */
static s32 weight[QINDEX_RANGE] = {
	4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
	4,  4,  4,  4,  4,  5,  5,  5,  5,  5,
	5,  5,  5,  5,  5,  5,  5,  6,  6,  6,
	6,  6,  6,  6,  6,  6,  6,  7,  7,  7,
	7,  7,  7,  7,  7,  8,  8,  8,  8,  8,
	8,  8,  9,  9,  9,  9,  9, 10, 10, 10,
	10, 11, 11, 11, 12, 12, 13, 13, 13, 13,
	14, 14, 14, 14, 15, 15, 15, 16, 16, 17,
	17, 18, 18, 19, 19, 20, 20, 20, 21, 22,
	23, 23, 24, 24, 25, 25, 26, 27, 28, 28,
	29, 30, 31, 32, 32, 33, 34, 35, 36, 37,
	38, 39, 40, 41, 42, 44, 44, 46, 47, 48,
	50, 51, 52, 54, 55, 57, 58, 61
};

struct tree {
	int32_t value;       /* Bits describe the bool tree  */
	int32_t number;      /* Number, valid bit count in above tree */
	int32_t index[9];    /* Probability table index */
};

/* Motion vector tree */
static struct tree mv_tree[] = {
	{ 0, 3, { 0, 1, 2 } },        /* mv_0 000 */
	{ 1, 3, { 0, 1, 2 } },        /* mv_1 001 */
	{ 2, 3, { 0, 1, 3 } },        /* mv_2 010 */
	{ 3, 3, { 0, 1, 3 } },        /* mv_3 011 */
	{ 4, 3, { 0, 4, 5 } },        /* mv_4 100 */
	{ 5, 3, { 0, 4, 5 } },        /* mv_5 101 */
	{ 6, 3, { 0, 4, 6 } },        /* mv_6 110 */
	{ 7, 3, { 0, 4, 6 } },        /* mv_7 111 */
};

/* If probability being zero is p, then average number of bits used to encode 0
 * is log2(1/p), to encode 1 is log2(1/(1-p)).
 *
 * For example, if the probability of being zero is 0.5
 * bin = 0 -> average bits used is log2(1/0.5)      = 1 bits/bin
 * bin = 1 -> average bits used is log2(1/(1 - 0.5) = 1 bits/bin
 *
 * For example, if the probability of being zero is 0.95
 * bin = 0 -> average bits used is log2(1/0.95)      = 0.074 bits/bin
 * bin = 1 -> average bits used is log2(1/(1 - 0.95) = 4.321 bits/bin
 *
 * The cost[p] is average number of bits used to encode 0 if the probability is
 * p / 256, scaled by a magic number 256,
 * i.e., cost[p] = round(log2(256 / p) * 256).
 */
static const s32 const vp8_prob_cost[] = {
	2048, 2048, 1792, 1642, 1536, 1454, 1386, 1329, 1280, 1236,
	1198, 1162, 1130, 1101, 1073, 1048, 1024, 1002,  980,  961,
	942,  924,  906,  890,  874,  859,  845,  831,  817,  804,
	792,  780,  768,  757,  746,  735,  724,  714,  705,  695,
	686,  676,  668,  659,  650,  642,  634,  626,  618,  611,
	603,  596,  589,  582,  575,  568,  561,  555,  548,  542,
	536,  530,  524,  518,  512,  506,  501,  495,  490,  484,
	479,  474,  468,  463,  458,  453,  449,  444,  439,  434,
	430,  425,  420,  416,  412,  407,  403,  399,  394,  390,
	386,  382,  378,  374,  370,  366,  362,  358,  355,  351,
	347,  343,  340,  336,  333,  329,  326,  322,  319,  315,
	312,  309,  305,  302,  299,  296,  292,  289,  286,  283,
	280,  277,  274,  271,  268,  265,  262,  259,  256,  253,
	250,  247,  245,  242,  239,  236,  234,  231,  228,  226,
	223,  220,  218,  215,  212,  210,  207,  205,  202,  200,
	197,  195,  193,  190,  188,  185,  183,  181,  178,  176,
	174,  171,  169,  167,  164,  162,  160,  158,  156,  153,
	151,  149,  147,  145,  143,  140,  138,  136,  134,  132,
	130,  128,  126,  124,  122,  120,  118,  116,  114,  112,
	110,  108,  106,  104,  102,  101,   99,   97,   95,   93,
	91,   89,   87,   86,   84,   82,   80,   78,   77,   75,
	73,   71,   70,   68,   66,   64,   63,   61,   59,   58,
	56,   54,   53,   51,   49,   48,   46,   44,   43,   41,
	40,   38,   36,   35,   33,   32,   30,   28,   27,   25,
	24,   22,   21,   19,   18,   16,   15,   13,   12,   10,
	9,    7,    6,    4,    3,    1
};

/* Approximate bit cost of bin at given probability prob */
#define COST_BOOL(prob, bin)	vp8_prob_cost[(bin) ? 255 - (prob) : prob]

/**
 * struct rk3399_vpu_vp8e_ctrl_buf - hardware control buffer layout
 * @ext_hdr_size:	Ext header size in bytes (written by hardware).
 * @dct_size:		DCT partition size (written by hardware).
 * @rsvd:		Reserved for hardware.
 */
struct rk3399_vpu_vp8e_ctrl_buf {
	u32 ext_hdr_size;
	u32 dct_size;
	u8 rsvd[1016];
};

int rk3399_vpu_vp8e_init(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;
	size_t height = ctx->src_fmt.height;
	size_t width = ctx->src_fmt.width;
	size_t ref_buf_size;
	size_t mv_size;
	int ret;

	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->hw.vp8e.ctrl_buf,
				sizeof(struct rk3399_vpu_vp8e_ctrl_buf));
	if (ret) {
		vpu_err("failed to allocate ctrl buffer\n");
		return ret;
	}

	mv_size = DIV_ROUND_UP(width, 16) * DIV_ROUND_UP(height, 16) * 4;
	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->hw.vp8e.mv_buf, mv_size);
	if (ret) {
		vpu_err("failed to allocate MV buffer\n");
		goto err_ctrl_buf;
	}

	ref_buf_size = rockchip_vpu_rounded_luma_size(width, height) * 3 / 2;
	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->hw.vp8e.ext_buf,
					2 * ref_buf_size);
	if (ret) {
		vpu_err("failed to allocate ext buffer\n");
		goto err_mv_buf;
	}

	return 0;

err_mv_buf:
	rockchip_vpu_aux_buf_free(vpu, &ctx->hw.vp8e.mv_buf);
err_ctrl_buf:
	rockchip_vpu_aux_buf_free(vpu, &ctx->hw.vp8e.ctrl_buf);

	return ret;
}

void rk3399_vpu_vp8e_exit(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;

	rockchip_vpu_aux_buf_free(vpu, &ctx->hw.vp8e.ext_buf);
	rockchip_vpu_aux_buf_free(vpu, &ctx->hw.vp8e.mv_buf);
	rockchip_vpu_aux_buf_free(vpu, &ctx->hw.vp8e.ctrl_buf);
}

static inline u32 enc_in_img_ctrl(struct rockchip_vpu_ctx *ctx)
{
	struct v4l2_pix_format_mplane *pix_fmt = &ctx->src_fmt;
	const struct rk3399_vp8e_reg_params *params =
		(struct rk3399_vp8e_reg_params *)ctx->run.vp8e.reg_params;
	struct v4l2_rect *crop = &ctx->src_crop;
	unsigned overfill_r, overfill_b;
	u32 first_free_bits = (params->frm_hdr_size & 7) * 8;

	/*
	 * The hardware needs only the value for luma plane, because
	 * values of other planes are calculated internally based on
	 * format setting.
	 */
	overfill_r = (pix_fmt->width - crop->width) / 4;
	overfill_b = pix_fmt->height - crop->height;

	/** TODO, finish first free bit when assemble frame header
	 *  done
	 */
	return VEPU_REG_STREAM_START_OFFSET(first_free_bits) |
		VEPU_REG_IN_IMG_CTRL_OVRFLR_D4(overfill_r) |
		VEPU_REG_IN_IMG_CTRL_OVRFLB(overfill_b) |
		VEPU_REG_SKIP_MACROBLOCK_PENALTY(params->qp >= 100 ?
			(3 * params->qp / 4) : 0);
}

static void rk3399_vpu_vp8e_set_buffers(struct rockchip_vpu_dev *vpu,
					struct rockchip_vpu_ctx *ctx)
{
	const u32 src_addr_regs[] = { VEPU_REG_ADDR_IN_LUMA,
				      VEPU_REG_ADDR_IN_CB,
				      VEPU_REG_ADDR_IN_CR };
	const struct rk3399_vp8e_reg_params *params =
		(struct rk3399_vp8e_reg_params *)ctx->run.vp8e.reg_params;
	struct v4l2_pix_format_mplane *src_fmt = &ctx->src_fmt;
	dma_addr_t ref_buf_dma, rec_buf_dma;
	dma_addr_t stream_dma;
	size_t rounded_size;
	dma_addr_t dst_dma;
	size_t dst_size;
	int i;

	vpu_debug_enter();

	rounded_size = rockchip_vpu_rounded_luma_size(ctx->src_fmt.width,
						      ctx->src_fmt.height);

	ref_buf_dma = rec_buf_dma = ctx->hw.vp8e.ext_buf.dma;
	if (ctx->hw.vp8e.ref_rec_ptr)
		ref_buf_dma += rounded_size * 3 / 2;
	else
		rec_buf_dma += rounded_size * 3 / 2;
	ctx->hw.vp8e.ref_rec_ptr ^= 1;

	dst_dma = vb2_dma_contig_plane_dma_addr(&ctx->run.dst->b.vb2_buf, 0);
	dst_size = vb2_plane_size(&ctx->run.dst->b.vb2_buf, 0);

	/*
	 * stream addr-->|
	 * align 64bits->|<-start offset->|
	 * |<---------header size-------->|<---dst buf---
	 */
	stream_dma = round_down(dst_dma + params->frm_hdr_size, 8);

	/**
	 * Userspace will pass 8 bytes aligned size(round_down) to us,
	 * so we need to plus start offset to get real header size.
	 *
	 * |<-aligned size->|<-start offset->|
	 * |<----------header size---------->|
	 */
	vpu_debug(0, "frame header size %u\n", params->frm_hdr_size);
	ctx->run.dst->vp8e.hdr_size = params->frm_hdr_size;

	if (params->is_intra)
		ctx->run.dst->b.flags |= V4L2_BUF_FLAG_KEYFRAME;
	else
		ctx->run.dst->b.flags &= ~V4L2_BUF_FLAG_KEYFRAME;

	/*
	 * We assume here that 1/10 of the buffer is enough for headers.
	 * DCT partition will be placed in remaining 9/10 of the buffer.
	 */
	ctx->run.dst->vp8e.dct_offset = round_up(dst_size / 10, 8);

	/* Destination buffer. */
	vepu_write_relaxed(vpu, stream_dma, VEPU_REG_ADDR_OUTPUT_STREAM);
	vepu_write_relaxed(vpu, dst_dma + ctx->run.dst->vp8e.dct_offset,
				VEPU_REG_ADDR_VP8_DCT_PART(0));
	vepu_write_relaxed(vpu, dst_size - ctx->run.dst->vp8e.dct_offset,
				VEPU_REG_STR_BUF_LIMIT);

	/* Auxiliary buffers. */
	vepu_write_relaxed(vpu, ctx->hw.vp8e.ctrl_buf.dma,
				VEPU_REG_ADDR_OUTPUT_CTRL);
	vepu_write_relaxed(vpu, ctx->hw.vp8e.mv_buf.dma,
				VEPU_REG_ADDR_MV_OUT);
	vepu_write_relaxed(vpu, ctx->run.priv_dst.dma,
				VEPU_REG_ADDR_VP8_PROB_CNT);
	vepu_write_relaxed(vpu, ctx->run.priv_src.dma + VP8_CABAC_CTX_OFFSET,
				VEPU_REG_ADDR_CABAC_TBL);

	memset(ctx->run.priv_src.cpu + VP8_CABAC_CTX_OFFSET +
		VP8_CABAC_CTX_SIZE, 0, VP8_SEGMENT_MAP_SIZE);
	vepu_write_relaxed(vpu, ctx->run.priv_src.dma
				+ VP8_CABAC_CTX_OFFSET + VP8_CABAC_CTX_SIZE,
				VEPU_REG_ADDR_VP8_SEG_MAP);

	/* Reference buffers. */
	vepu_write_relaxed(vpu, ref_buf_dma,
				VEPU_REG_ADDR_REF_LUMA);
	vepu_write_relaxed(vpu, ref_buf_dma + rounded_size,
				VEPU_REG_ADDR_REF_CHROMA);

	/* Reconstruction buffers. */
	vepu_write_relaxed(vpu, rec_buf_dma,
				VEPU_REG_ADDR_REC_LUMA);
	vepu_write_relaxed(vpu, rec_buf_dma + rounded_size,
				VEPU_REG_ADDR_REC_CHROMA);

	/* Source buffer. */
	/*
	 * TODO(crbug.com/901264): The way to pass an offset within a DMA-buf
	 * is not defined in V4L2 specification, so we abuse data_offset
	 * for now. Fix it when we have the right interface, including
	 * any necessary validation and potential alignment issues.
	 */
	for (i = 0; i < src_fmt->num_planes; ++i)
		vepu_write_relaxed(vpu, vb2_dma_contig_plane_dma_addr(
				   &ctx->run.src->b.vb2_buf, i) +
				ctx->run.src->b.vb2_buf.planes[i].data_offset,
				   src_addr_regs[i]);

	/* Source parameters. */
	vepu_write_relaxed(vpu, enc_in_img_ctrl(ctx),
			   VEPU_REG_ENC_OVER_FILL_STRM_OFFSET);

	vpu_debug_leave();
}

static s32 cost_tree(struct tree *tree, const s32 *prob)
{
	s32 value = tree->value;
	s32 number = tree->number;
	s32 *index = tree->index;
	s32 bit_cost = 0;

	while (number--)
		bit_cost += COST_BOOL(prob[*index++], (value >> number) & 1);

	return bit_cost;
}

static s32 cost_mv(s32 mvd, const s32 *mv_prob)
{
	s32 i, tmp, bit_cost = 0;

	/* Luma motion vectors are doubled, see 18.1 in "VP8 Data Format and
	 * Decoding Guide".
	 */
	BUG_ON(mvd & 1);
	tmp = abs(mvd >> 1);

	/* Short Tree */
	if (tmp < 8) {
		bit_cost += COST_BOOL(mv_prob[0], 0);
		bit_cost += cost_tree(&mv_tree[tmp], mv_prob + 2);
		if (!tmp)
			return bit_cost;

		/* Sign */
		bit_cost += COST_BOOL(mv_prob[1], mvd < 0);
		return bit_cost;
	}

	/* Long Tree */
	bit_cost += COST_BOOL(mv_prob[0], 1);

	/* Bits 0, 1, 2 */
	for (i = 0; i < 3; i++)
		bit_cost += COST_BOOL(mv_prob[9 + i], (tmp >> i) & 1);

	/* Bits 9, 8, 7, 6, 5, 4 */
	for (i = 9; i > 3; i--)
		bit_cost += COST_BOOL(mv_prob[9 + i], (tmp >> i) & 1);

	/*
	 * Bit 3: if ABS(mvd) < 8, it is coded with short tree, so if here
	 * ABS(mvd) <= 15, bit 3 must be one (because here we code values
	 * 8,...,15) and is not explicitly coded.
	 */
	if (tmp > 15)
		bit_cost += COST_BOOL(mv_prob[9 + 3], (tmp >> 3) & 1);

	/* Sign */
	bit_cost += COST_BOOL(mv_prob[1], mvd < 0);

	return bit_cost;
}

static void rk3399_vpu_vp8e_set_params(struct rockchip_vpu_dev *vpu,
				       struct rockchip_vpu_ctx *ctx)
{
	const struct rk3399_vp8e_reg_params *params =
		(struct rk3399_vp8e_reg_params *)ctx->run.vp8e.reg_params;
	int i;
	u32 reg;
	u32 mbs_in_row = MB_WIDTH(ctx->src_fmt.width);
	u32 mbs_in_col = MB_HEIGHT(ctx->src_fmt.height);
	u32 deq;
	u32 tmp;
	u32 qp = params->qp;
	s32 inter_favor = 0;

	vpu_debug_enter();

	reg = VEPU_REG_OUTPUT_SWAP32
		| VEPU_REG_OUTPUT_SWAP16
		| VEPU_REG_OUTPUT_SWAP8
		| VEPU_REG_INPUT_SWAP8
		| VEPU_REG_INPUT_SWAP16
		| VEPU_REG_INPUT_SWAP32;
	vepu_write_relaxed(vpu, reg, VEPU_REG_DATA_ENDIAN);

	reg = VEPU_REG_SIZE_TABLE_PRESENT
		| VEPU_REG_IN_IMG_CTRL_FMT(ctx->vpu_src_fmt->enc_fmt)
		| VEPU_REG_IN_IMG_ROTATE_MODE(0);
	vepu_write_relaxed(vpu, reg, VEPU_REG_ENC_CTRL1);

	reg = VEPU_REG_INTERRUPT_TIMEOUT_EN
		| VEPU_REG_MV_WRITE_EN;
	vepu_write_relaxed(vpu, reg, VEPU_REG_INTERRUPT);

	reg = VEPU_REG_IN_IMG_CHROMA_OFFSET(0)
		| VEPU_REG_IN_IMG_LUMA_OFFSET(0)
		| VEPU_REG_IN_IMG_CTRL_ROW_LEN(mbs_in_row * 16);
	vepu_write_relaxed(vpu, reg, VEPU_REG_INPUT_LUMA_INFO);

	vepu_write_relaxed(vpu, 0, VEPU_REG_STR_HDR_REM_MSB);
	vepu_write_relaxed(vpu, 0, VEPU_REG_STR_HDR_REM_LSB);

	reg = 0;
	if (mbs_in_row * mbs_in_col > MAX_MB_COUNT_TO_DISABLE_QUARTER_PIXEL_MV)
		reg = VEPU_REG_DISABLE_QUARTER_PIXEL_MV;
	reg |= VEPU_REG_ENTROPY_CODING_MODE;
	vepu_write_relaxed(vpu, reg, VEPU_REG_ENC_CTRL0);

	inter_favor = 128 - params->intra_prob;
	if (inter_favor >= 0)
		inter_favor = max(0u, qp * 2 - 40);

	reg = VEPU_REG_INTRA16X16_MODE(qp * 1024 / 128)
		| VEPU_REG_INTER_MODE(inter_favor);
	vepu_write_relaxed(vpu, reg, VEPU_REG_INTRA_INTER_MODE);

	reg = VEPU_REG_1MV_PENALTY(60 / 2 * 32)
		| VEPU_REG_QMV_PENALTY(8)
		| VEPU_REG_4MV_PENALTY(64 / 2);
	if (mbs_in_row * mbs_in_col < MAX_MB_COUNT_TO_DISABLE_SPLIT_MV)
		reg |= VEPU_REG_SPLIT_MV_MODE_EN;
	vepu_write_relaxed(vpu, reg, VEPU_REG_MV_PENALTY);

	reg = VEPU_REG_MV_PENALTY_16X8_8X16(
			min(1023, split_penalty[qp] / 2))
		| VEPU_REG_MV_PENALTY_8X8(
			min(1023, (2 * split_penalty[qp] + 40) / 4))
		| VEPU_REG_MV_PENALTY_8X4_4X8(0x3ff);
	/* no 8x4 or 4x8 block define in vp8 */
	vepu_write_relaxed(vpu, reg, VEPU_REG_ENC_CTRL4);

	reg = VEPU_REG_PENALTY_4X4MV(min(511,
					 (8 * split_penalty[qp] + 500) / 16))
		| VEPU_REG_ZERO_MV_FAVOR_D2(0);
	vepu_write_relaxed(vpu, reg, VEPU_REG_MVC_RELATE);

	/* initialize quant table for segment0 */
	deq = dc_q_lookup[qp];
	reg = VEPU_REG_VP8_SEG0_QUT_DC_Y1(min((1 << 16) / deq, 0x3FFFu));
	reg |= VEPU_REG_VP8_SEG0_ZBIN_DC_Y1(((qzbin_factors[qp] * deq) + 64) >>
					    7);
	reg |= VEPU_REG_VP8_SEG0_RND_DC_Y1((qrounding_factors[qp] * deq) >> 7);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_DC_Y1);

	deq = ac_q_lookup[qp];
	reg = VEPU_REG_VP8_SEG0_QUT_AC_Y1(min((1 << 16) / deq, 0x3FFFu));
	reg |= VEPU_REG_VP8_SEG0_ZBIN_AC_Y1(((qzbin_factors[qp] * deq) + 64) >>
					    7);
	reg |= VEPU_REG_VP8_SEG0_RND_AC_Y1((qrounding_factors[qp] * deq) >> 7);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_AC_Y1);

	deq = dc_q_lookup[qp] * 2;
	reg = VEPU_REG_VP8_SEG0_QUT_DC_Y2(min((1 << 16) / deq, 0x3FFFu));
	reg |= VEPU_REG_VP8_SEG0_ZBIN_DC_Y2((qzbin_factors[qp] * deq + 64) >>
					    7);
	reg |= VEPU_REG_VP8_SEG0_RND_DC_Y2((qrounding_factors[qp] * deq) >> 7);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_DC_Y2);

	deq = ac_q_lookup[qp] * 155 / 100;
	reg = VEPU_REG_VP8_SEG0_QUT_AC_Y2(min((1 << 16) / deq, 0x3FFFu));
	reg |= VEPU_REG_VP8_SEG0_ZBIN_AC_Y2((qzbin_factors[qp] * deq + 64) >>
					    7);
	reg |= VEPU_REG_VP8_SEG0_RND_AC_Y2((qrounding_factors[qp] * deq) >> 7);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_AC_Y2);

	deq = min(dc_q_lookup[qp], 132);
	reg = VEPU_REG_VP8_SEG0_QUT_DC_CHR(min((1 << 16) / deq, 0x3FFFu));
	reg |= VEPU_REG_VP8_SEG0_ZBIN_DC_CHR((qzbin_factors[qp] * deq + 64) >>
					     7);
	reg |= VEPU_REG_VP8_SEG0_RND_DC_CHR((qrounding_factors[qp] * deq) >> 7);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_DC_CHR);

	deq = ac_q_lookup[qp];
	reg = VEPU_REG_VP8_SEG0_QUT_AC_CHR(min((1 << 16) / deq, 0x3FFFu));
	reg |= VEPU_REG_VP8_SEG0_ZBIN_AC_CHR((qzbin_factors[qp] * deq + 64) >>
					     7);
	reg |= VEPU_REG_VP8_SEG0_RND_AC_CHR((qrounding_factors[qp] * deq) >> 7);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_AC_CHR);

	reg = VEPU_REG_VP8_MV_REF_IDX1(0);
	reg |= VEPU_REG_VP8_SEG0_DQUT_DC_Y1(dc_q_lookup[qp]);
	reg |= VEPU_REG_VP8_SEG0_DQUT_AC_Y1(ac_q_lookup[qp]);
	reg |= VEPU_REG_VP8_SEG0_DQUT_DC_Y2(dc_q_lookup[qp] * 2);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_DQUT);

	reg = VEPU_REG_VP8_MV_REF_IDX2(0);
	reg |= VEPU_REG_VP8_SEG0_DQUT_DC_CHR(min(dc_q_lookup[qp], 132));
	reg |= VEPU_REG_VP8_SEG0_DQUT_AC_CHR(ac_q_lookup[qp]);
	reg |= VEPU_REG_VP8_SEG0_DQUT_AC_Y2(ac_q_lookup[qp] * 155 / 100);
	if (params->is_intra)
		reg |= VEPU_REG_VP8_SEGMENT_MAP_UPDATE;
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_SEG0_QUANT_DQUT_1);
	vepu_write_relaxed(vpu, params->bool_enc_value,
		VEPU_REG_VP8_BOOL_ENC_VALUE);

	reg = VEPU_REG_VP8_DCT_PARTITION_CNT(0);
	reg |= VEPU_REG_VP8_FILTER_LEVEL(params->filter_level);
	reg |= VEPU_REG_VP8_FILTER_SHARPNESS(params->filter_sharpness);
	reg |= VEPU_REG_VP8_ZERO_MV_PENALTY_FOR_REF2(0);
	reg |= VEPU_REG_VP8_BOOL_ENC_VALUE_BITS(params->bool_enc_value_bits);
	reg |= VEPU_REG_VP8_BOOL_ENC_RANGE(params->bool_enc_range);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_ENC_CTRL2);

	vepu_write_relaxed(vpu, 0, VEPU_REG_ROI1);
	vepu_write_relaxed(vpu, 0, VEPU_REG_ROI2);
	vepu_write_relaxed(vpu, 0, VEPU_REG_STABILIZATION_OUTPUT);
	vepu_write_relaxed(vpu, 0, VEPU_REG_RGB2YUV_CONVERSION_COEF1);
	vepu_write_relaxed(vpu, 0, VEPU_REG_RGB2YUV_CONVERSION_COEF2);
	vepu_write_relaxed(vpu, 0, VEPU_REG_RGB2YUV_CONVERSION_COEF3);
	vepu_write_relaxed(vpu, 0, VEPU_REG_RGB_MASK_MSB);
	vepu_write_relaxed(vpu, 0, VEPU_REG_CIR_INTRA_CTRL);
	vepu_write_relaxed(vpu, 0, VEPU_REG_INTRA_AREA_CTRL);

	/* Intra 4x4 mode */
	tmp = qp * 2 + 8;
	for (i = 0; i < 5; i++) {
		reg = VEPU_REG_VP8_INTRA_4X4_PENALTY_0
			((intra_4_tree_penalty[i * 2] * tmp) >> 8);
		reg |= VEPU_REG_VP8_INTRA_4x4_PENALTY_1
			((intra_4_tree_penalty[i * 2 + 1] * tmp) >> 8);
		vepu_write_relaxed(vpu, reg,
				   VEPU_REG_VP8_INTRA_4X4_PENALTY(i));
	}

	/* Intra 16x16 mode */
	tmp = qp * 2 + 64;
	for (i = 0; i < 2; i++) {
		reg = VEPU_REG_VP8_INTRA_16X16_PENALTY_0
			((intra_16_tree_penalty[2 * i] * tmp) >> 8);
		reg |= VEPU_REG_VP8_INTRA_16X16_PENALTY_1
			((intra_16_tree_penalty[2 * i + 1] * tmp) >> 8);
		vepu_write_relaxed(vpu, reg,
				   VEPU_REG_VP8_INTRA_16X16_PENALTY(i));
	}

	reg = VEPU_REG_VP8_LF_REF_DELTA_INTRA_MB(params->intra_frm_delta);
	reg |= VEPU_REG_VP8_LF_MODE_DELTA_BPRED(params->bpred_mode_delta);
	reg |= VEPU_REG_VP8_INTER_TYPE_BIT_COST(0);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_CONTROL);

	reg = VEPU_REG_VP8_LF_REF_DELTA_ALT_REF(params->altref_frm_delta)
		| VEPU_REG_VP8_LF_REF_DELTA_LAST_REF(params->last_frm_delta)
		| VEPU_REG_VP8_LF_REF_DELTA_GOLDEN(params->golden_frm_delta);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_LOOP_FILTER_REF_DELTA);

	reg = VEPU_REG_VP8_LF_MODE_DELTA_SPLITMV(params->splitmv_mode_delta)
		| VEPU_REG_VP8_LF_MODE_DELTA_ZEROMV(params->zero_mode_delta)
		| VEPU_REG_VP8_LF_MODE_DELTA_NEWMV(params->newmv_mode_delta);
	vepu_write_relaxed(vpu, reg, VEPU_REG_VP8_LOOP_FILTER_MODE_DELTA);

	for (i = 0; i < 128; i += 4) {
		u32 x;
		u32 y;

		reg = VEPU_REG_DMV_PENALTY_TABLE_BIT(i * 2, 3);
		reg |= VEPU_REG_DMV_PENALTY_TABLE_BIT((i + 1) * 2, 2);
		reg |= VEPU_REG_DMV_PENALTY_TABLE_BIT((i + 2) * 2, 1);
		reg |= VEPU_REG_DMV_PENALTY_TABLE_BIT((i + 3) * 2, 0);
		vepu_write_relaxed(vpu, reg, VEPU_REG_DMV_PENALTY_TBL(i / 4));

		y = cost_mv(i * 2, params->mv_prob[0]);	/* mv y */
		x = cost_mv(i * 2, params->mv_prob[1]);	/* mv x */

		reg = VEPU_REG_DMV_Q_PIXEL_PENALTY_TABLE_BIT(
			min(255u, (y + x + 1) / 2 * weight[qp] >> 8), 3);

		y = cost_mv((i + 1) * 2, params->mv_prob[0]); /* mv y */
		x = cost_mv((i + 1) * 2, params->mv_prob[1]); /* mv x */
		reg |= VEPU_REG_DMV_Q_PIXEL_PENALTY_TABLE_BIT(
			min(255u, (y + x + 1) / 2 * weight[qp] >> 8), 2);

		y = cost_mv((i + 2) * 2, params->mv_prob[0]); /* mv y */
		x = cost_mv((i + 2) * 2, params->mv_prob[1]); /* mv x */
		reg |= VEPU_REG_DMV_Q_PIXEL_PENALTY_TABLE_BIT(
			min(255u, (y + x + 1) / 2 * weight[qp] >> 8), 1);

		y = cost_mv((i + 3) * 2, params->mv_prob[0]); /* mv y */
		x = cost_mv((i + 3) * 2, params->mv_prob[1]); /* mv x */
		reg |= VEPU_REG_DMV_Q_PIXEL_PENALTY_TABLE_BIT(
			min(255u, (y + x + 1) / 2 * weight[qp] >> 8), 0);

		vepu_write_relaxed(vpu, reg,
				   VEPU_REG_DMV_Q_PIXEL_PENALTY_TBL(i / 4));
	}

	vpu_debug_leave();
}

void rk3399_vpu_vp8e_run(struct rockchip_vpu_ctx *ctx)
{
	const struct rk3399_vp8e_reg_params *params =
		(struct rk3399_vp8e_reg_params *)ctx->run.vp8e.reg_params;
	struct rockchip_vpu_dev *vpu = ctx->dev;
	u32 reg;

	vpu_debug_enter();

	/* The hardware expects the control buffer to be zeroed. */
	memset(ctx->hw.vp8e.ctrl_buf.cpu, 0,
		sizeof(struct rk3399_vpu_vp8e_ctrl_buf));

	/*
	 * Program the hardware.
	 */
	rockchip_vpu_power_on(vpu);

	reg = VEPU_REG_ENCODE_FORMAT(1);
	vepu_write_relaxed(vpu, reg, VEPU_REG_ENCODE_START);

	rk3399_vpu_vp8e_set_params(vpu, ctx);
	rk3399_vpu_vp8e_set_buffers(vpu, ctx);

	reg = VEPU_REG_AXI_CTRL_READ_ID(0)
		| VEPU_REG_AXI_CTRL_WRITE_ID(0)
		| VEPU_REG_AXI_CTRL_BURST_LEN(16)
		| VEPU_REG_AXI_CTRL_INCREMENT_MODE(0)
		| VEPU_REG_AXI_CTRL_BIRST_DISCARD(0);
	vepu_write_relaxed(vpu, reg, VEPU_REG_AXI_CTRL);

	/* Make sure that all registers are written at this point. */
	wmb();

	/* Set the watchdog. */
	schedule_delayed_work(&vpu->watchdog_work, msecs_to_jiffies(2000));

	/* Start the hardware. */
	reg = VEPU_REG_MB_HEIGHT(MB_HEIGHT(ctx->src_fmt.height))
		| VEPU_REG_MB_WIDTH(MB_WIDTH(ctx->src_fmt.width))
		| VEPU_REG_PIC_TYPE(params->is_intra)
		| VEPU_REG_ENCODE_FORMAT(1)
		| VEPU_REG_ENCODE_ENABLE;
	vepu_write_relaxed(vpu, reg, VEPU_REG_ENCODE_START);

	vpu_debug_leave();
}

void rk3399_vpu_vp8e_done(struct rockchip_vpu_ctx *ctx,
			  enum vb2_buffer_state result)
{
	struct rk3399_vpu_vp8e_ctrl_buf *ctrl_buf = ctx->hw.vp8e.ctrl_buf.cpu;
	const struct rk3399_vp8e_reg_params *params =
		(struct rk3399_vp8e_reg_params *)ctx->run.vp8e.reg_params;

	vpu_debug_enter();

	/* Read length information of this run from utility buffer. */
	ctx->run.dst->vp8e.ext_hdr_size = ctrl_buf->ext_hdr_size -
		(params->frm_hdr_size & 7);
	ctx->run.dst->vp8e.dct_size = ctrl_buf->dct_size;

	rockchip_vpu_run_done(ctx, result);

	vpu_debug_leave();
}
