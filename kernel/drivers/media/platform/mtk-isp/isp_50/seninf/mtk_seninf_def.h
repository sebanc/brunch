/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __SENINF_DRV_DEF_H__
#define __SENINF_DRV_DEF_H__

#define SENINF_TIMESTAMP_STEP		0x67
#define SENINF_SETTLE_DELAY		0x15
#define SENINF_HS_TRAIL_PARAMETER	0x8

#define NUM_PADS			12
#define NUM_SENSORS			4
#define CAM_MUX_IDX_MIN		NUM_SENSORS
#define DEFAULT_WIDTH			1600
#define DEFAULT_HEIGHT			1200

#define PAD_10BIT			0

enum {
	TEST_GEN_PATTERN = 0x0,
	TEST_DUMP_DEBUG_INFO,
};

enum {
	CFG_CSI_PORT_0 = 0x0,	/* 4D1C */
	CFG_CSI_PORT_1,		/* 4D1C */
	CFG_CSI_PORT_2,		/* 4D1C */
	CFG_CSI_PORT_0A,	/* 2D1C */
	CFG_CSI_PORT_0B,	/* 2D1C */
	CFG_CSI_PORT_MAX_NUM,
	CFG_CSI_PORT_NONE	/*for non-MIPI sensor */
};

enum {
	ONE_PIXEL_MODE  = 0x0,
	TWO_PIXEL_MODE  = 0x1,
	FOUR_PIXEL_MODE = 0x2,
};

enum {
	SENINF_1 = 0x0,
	SENINF_2 = 0x1,
	SENINF_3 = 0x2,
	SENINF_4 = 0x3,
	SENINF_5 = 0x4,
	SENINF_NUM,
};

enum {
	RAW_8BIT_FMT        = 0x0,
	RAW_10BIT_FMT       = 0x1,
	RAW_12BIT_FMT       = 0x2,
	YUV422_FMT          = 0x3,
	RAW_14BIT_FMT       = 0x4,
	RGB565_MIPI_FMT     = 0x5,
	RGB888_MIPI_FMT     = 0x6,
	JPEG_FMT            = 0x7
};

#endif /*__SENINF_DRV_DEF_H__ */
