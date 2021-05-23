// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 Bitland Inc.
// Copyright 2020 Google LLC.

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/regulator/consumer.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

/* External clock frequency supported by the driver */
#define GC5035_MCLK_RATE				24000000UL
/* Number of lanes supported by this driver */
#define GC5035_DATA_LANES				2
/* Bits per sample of sensor output */
#define GC5035_BITS_PER_SAMPLE				10

/* System registers (accessible regardless of the page. */

/* Chip ID */
#define GC5035_REG_CHIP_ID_H				0xf0
#define GC5035_REG_CHIP_ID_L				0xf1
#define GC5035_CHIP_ID						0x5035
#define GC5035_ID(_msb, _lsb)				((_msb) << 8 | (_lsb))

/* Register page selection register */
#define GC5035_PAGE_REG					0xfe

/* Page 0 registers */

/* Exposure control */
#define GC5035_REG_EXPOSURE_H				0x03
#define GC5035_REG_EXPOSURE_L				0x04
#define GC5035_EXPOSURE_H_MASK				0x3f
#define GC5035_EXPOSURE_MIN				4
#define GC5035_EXPOSURE_STEP				4

/* Analog gain control */
#define GC5035_REG_ANALOG_GAIN				0xb6
#define GC5035_ANALOG_GAIN_MIN				0
#define GC5035_ANALOG_GAIN_MAX				31
#define GC5035_ANALOG_GAIN_STEP				1
#define GC5035_ANALOG_GAIN_DEFAULT			GC5035_ANALOG_GAIN_MIN

/* Digital gain control */
#define GC5035_REG_DIGI_GAIN_H				0xb1
#define GC5035_REG_DIGI_GAIN_L				0xb2
#define GC5035_DGAIN_H_MASK				0x0f
#define GC5035_DGAIN_L_MASK				0xfc
#define GC5035_DGAIN_L_SHIFT				2
#define GC5035_DIGI_GAIN_MIN				0
#define GC5035_DIGI_GAIN_MAX				1023
#define GC5035_DIGI_GAIN_STEP				1
#define GC5035_DIGI_GAIN_DEFAULT			GC5035_DIGI_GAIN_MAX

/* Vblank control */
#define GC5035_REG_VTS_H				0x41
#define GC5035_REG_VTS_L				0x42
#define GC5035_VTS_H_MASK				0x3f
#define GC5035_VTS_MAX					16383
#define GC5035_EXPOSURE_MARGIN				16

#define GC5035_REG_CTRL_MODE				0x3e
#define GC5035_MODE_SW_STANDBY				0x01
#define GC5035_MODE_STREAMING				0x91

/* Page 1 registers */

/* Test pattern control */
#define GC5035_REG_TEST_PATTERN				0x8c
#define GC5035_TEST_PATTERN_ENABLE			0x11
#define GC5035_TEST_PATTERN_DISABLE			0x10

/* Page 2 registers */

/* OTP access registers */
#define GC5035_REG_OTP_MODE				0xf3
#define GC5035_OTP_PRE_READ				0x20
#define GC5035_OTP_READ_MODE				0x12
#define GC5035_OTP_READ_DONE				0x00
#define GC5035_REG_OTP_DATA				0x6c
#define GC5035_REG_OTP_ACCESS_ADDR_H			0x69
#define GC5035_REG_OTP_ACCESS_ADDR_L			0x6a
#define GC5035_OTP_ACCESS_ADDR_H_MASK			0x1f
#define GC5035_OTP_ADDR_MASK				0x1fff
#define GC5035_OTP_ADDR_SHIFT				3
#define GC5035_REG_DD_TOTALNUM_H			0x01
#define GC5035_REG_DD_TOTALNUM_L			0x02
#define GC5035_DD_TOTALNUM_H_MASK			0x07
#define GC5035_REG_DD_LOAD_STATUS			0x06
#define GC5035_OTP_BIT_LOAD				BIT(0)

/* OTP-related definitions */

#define GC5035_OTP_ID_SIZE				9
#define GC5035_OTP_ID_DATA_OFFSET			0x0020
#define GC5035_OTP_DATA_LENGTH				1024

/* OTP DPC parameters */
#define GC5035_OTP_DPC_FLAG_OFFSET			0x0068
#define GC5035_OTP_DPC_FLAG_MASK			0x03
#define GC5035_OTP_FLAG_EMPTY				0x00
#define GC5035_OTP_FLAG_VALID				0x01
#define GC5035_OTP_DPC_TOTAL_NUMBER_OFFSET		0x0070
#define GC5035_OTP_DPC_ERROR_NUMBER_OFFSET		0x0078

/* OTP register parameters */
#define GC5035_OTP_REG_FLAG_OFFSET			0x0880
#define GC5035_OTP_REG_DATA_OFFSET			0x0888
#define GC5035_OTP_REG_ADDR_OFFSET			1
#define GC5035_OTP_REG_VAL_OFFSET			2
#define GC5035_OTP_PAGE_FLAG_OFFSET			3
#define GC5035_OTP_PER_PAGE_SIZE			4
#define GC5035_OTP_REG_PAGE_MASK			0x07
#define GC5035_OTP_REG_MAX_GROUP			5
#define GC5035_OTP_REG_BYTE_PER_GROUP			5
#define GC5035_OTP_REG_PER_GROUP			2
#define GC5035_OTP_REG_BYTE_PER_REG			2
#define GC5035_OTP_REG_DATA_SIZE			25
#define GC5035_OTP_REG_SIZE				10

#define GC5035_DD_DELAY_US				(10 * 1000)
#define GC5035_DD_TIMEOUT_US				(100 * 1000)

static const char * const gc5035_supplies[] = {
	/*
	 * Requested separately due to power sequencing needs:
	 * "iovdd",	 * Power supply for I/O circuits *
	 */
	"dvdd12",	/* Digital core power */
	"avdd28",	/* Analog power */
};

struct gc5035_regval {
	u8 addr;
	u8 val;
};

struct gc5035_reg {
	u8 page;
	struct gc5035_regval regval;
};

struct gc5035_otp_regs {
	unsigned int num_regs;
	struct gc5035_reg regs[GC5035_OTP_REG_SIZE];
};

struct gc5035_dpc {
	bool valid;
	unsigned int total_num;
};

struct gc5035_mode {
	u32 width;
	u32 height;
	u32 max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct gc5035_regval *reg_list;
	size_t num_regs;
};

struct gc5035 {
	struct i2c_client *client;
	struct clk *mclk;
	unsigned long mclk_rate;
	struct gpio_desc *resetb_gpio;
	struct gpio_desc *pwdn_gpio;
	struct regulator *iovdd_supply;
	struct regulator_bulk_data supplies[ARRAY_SIZE(gc5035_supplies)];

	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;

	bool otp_read;
	u8 otp_id[GC5035_OTP_ID_SIZE];
	struct gc5035_dpc dpc;
	struct gc5035_otp_regs otp_regs;

	/*
	 * Serialize control access, get/set format, get selection
	 * and start streaming.
	 */
	struct mutex mutex;
	bool streaming;
	const struct gc5035_mode *cur_mode;
};

static inline struct gc5035 *to_gc5035(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc5035, subdev);
}

static const struct gc5035_regval gc5035_otp_init_regs[] = {
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe9},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x82},
	{0xfa, 0x00},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x00},
	{0xf7, 0x01},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0xfa, 0x10},
	{0xf5, 0xe9},
	{0xfe, 0x02},
	{0x67, 0xc0},
	{0x59, 0x3f},
	{0x55, 0x84},
	{0x65, 0x80},
	{0x66, 0x03},
	{0xfe, 0x00},
};

static const struct gc5035_regval gc5035_otp_exit_regs[] = {
	{0xfe, 0x02},
	{0x67, 0x00},
	{0xfe, 0x00},
	{0xfa, 0x00},
};

static const struct gc5035_regval gc5035_dd_auto_load_regs[] = {
	{0xfe, 0x02},
	{0xbe, 0x00},
	{0xa9, 0x01},
	{0x09, 0x33},
};

static const struct gc5035_regval gc5035_otp_dd_regs[] = {
	{0x03, 0x00},
	{0x04, 0x80},
	{0x95, 0x0a},
	{0x96, 0x30},
	{0x97, 0x0a},
	{0x98, 0x32},
	{0x99, 0x07},
	{0x9a, 0xa9},
	{0xf3, 0x80},
};

static const struct gc5035_regval gc5035_otp_dd_enable_regs[] = {
	{0xbe, 0x01},
	{0x09, 0x00},
	{0xfe, 0x01},
	{0x80, 0x02},
	{0xfe, 0x00},
};

/*
 * Xclk 24Mhz
 * Pclk 87.6Mhz
 * grabwindow_width 2592
 * grabwindow_height 1944
 * max_framerate 30fps
 * mipi_datarate per lane 876Mbps
 */
static const struct gc5035_regval gc5035_global_regs[] = {
	/*init*/
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe9},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x82},
	{0xfa, 0x00},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x00},
	{0xfe, 0x03},
	{0x01, 0xe7},
	{0xf7, 0x01},
	{0xfc, 0x8f},
	{0xfc, 0x8f},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0x87, 0x18},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},

	/* Analog & CISCTL */
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0xda},
	{0x9d, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x04},
	{0x0b, 0x00},
	{0x0c, 0x03},
	{0x0d, 0x07},
	{0x0e, 0xa8},
	{0x0f, 0x0a},
	{0x10, 0x30},
	{0x11, 0x02},
	{0x17, 0x80},
	{0x19, 0x05},
	{0xfe, 0x02},
	{0x30, 0x03},
	{0x31, 0x03},
	{0xfe, 0x00},
	{0xd9, 0xc0},
	{0x1b, 0x20},
	{0x21, 0x48},
	{0x28, 0x22},
	{0x29, 0x58},
	{0x44, 0x20},
	{0x4b, 0x10},
	{0x4e, 0x1a},
	{0x50, 0x11},
	{0x52, 0x33},
	{0x53, 0x44},
	{0x55, 0x10},
	{0x5b, 0x11},
	{0xc5, 0x02},
	{0x8c, 0x1a},
	{0xfe, 0x02},
	{0x33, 0x05},
	{0x32, 0x38},
	{0xfe, 0x00},
	{0x91, 0x80},
	{0x92, 0x28},
	{0x93, 0x20},
	{0x95, 0xa0},
	{0x96, 0xe0},
	{0xd5, 0xfc},
	{0x97, 0x28},
	{0x16, 0x0c},
	{0x1a, 0x1a},
	{0x1f, 0x11},
	{0x20, 0x10},
	{0x46, 0x83},
	{0x4a, 0x04},
	{0x54, 0x02},
	{0x62, 0x00},
	{0x72, 0x8f},
	{0x73, 0x89},
	{0x7a, 0x05},
	{0x7d, 0xcc},
	{0x90, 0x00},
	{0xce, 0x18},
	{0xd0, 0xb2},
	{0xd2, 0x40},
	{0xe6, 0xe0},
	{0xfe, 0x02},
	{0x12, 0x01},
	{0x13, 0x01},
	{0x14, 0x01},
	{0x15, 0x02},
	{0x22, 0x7c},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},

	/* Gain */
	{0xfe, 0x00},
	{0xb0, 0x6e},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb6, 0x00},

	/* ISP */
	{0xfe, 0x01},
	{0x53, 0x00},
	{0x89, 0x03},
	{0x60, 0x40},

	/* BLK */
	{0xfe, 0x01},
	{0x42, 0x21},
	{0x49, 0x03},
	{0x4a, 0xff},
	{0x4b, 0xc0},
	{0x55, 0x00},

	/* Anti_blooming */
	{0xfe, 0x01},
	{0x41, 0x28},
	{0x4c, 0x00},
	{0x4d, 0x00},
	{0x4e, 0x3c},
	{0x44, 0x08},
	{0x48, 0x02},

	/* Crop */
	{0xfe, 0x01},
	{0x91, 0x00},
	{0x92, 0x08},
	{0x93, 0x00},
	{0x94, 0x07},
	{0x95, 0x07},
	{0x96, 0x98},
	{0x97, 0x0a},
	{0x98, 0x20},
	{0x99, 0x00},

	/* MIPI */
	{0xfe, 0x03},
	{0x02, 0x57},
	{0x03, 0xb7},
	{0x15, 0x14},
	{0x18, 0x0f},
	{0x21, 0x22},
	{0x22, 0x06},
	{0x23, 0x48},
	{0x24, 0x12},
	{0x25, 0x28},
	{0x26, 0x08},
	{0x29, 0x06},
	{0x2a, 0x58},
	{0x2b, 0x08},
	{0xfe, 0x01},
	{0x8c, 0x10},

	{0xfe, 0x00},
	{0x3e, 0x01},
};

/*
 * Xclk 24Mhz
 * Pclk 87.6Mhz
 * grabwindow_width 2592
 * grabwindow_height 1944
 * max_framerate 30fps
 * mipi_datarate per lane 876Mbps
 */
static const struct gc5035_regval gc5035_2592x1944_regs[] = {
	/* System */
	{0xfe, 0x00},
	{0x3e, 0x01},
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe9},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x82},
	{0xfa, 0x00},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x00},
	{0xfe, 0x03},
	{0x01, 0xe7},
	{0xf7, 0x01},
	{0xfc, 0x8f},
	{0xfc, 0x8f},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0x87, 0x18},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},

	/* Analog & CISCTL */
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0xda},
	{0x9d, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x04},
	{0x0b, 0x00},
	{0x0c, 0x03},
	{0x0d, 0x07},
	{0x0e, 0xa8},
	{0x0f, 0x0a},
	{0x10, 0x30},
	{0x21, 0x48},
	{0x29, 0x58},
	{0x44, 0x20},
	{0x4e, 0x1a},
	{0x8c, 0x1a},
	{0x91, 0x80},
	{0x92, 0x28},
	{0x93, 0x20},
	{0x95, 0xa0},
	{0x96, 0xe0},
	{0xd5, 0xfc},
	{0x97, 0x28},
	{0x1f, 0x11},
	{0xce, 0x18},
	{0xd0, 0xb2},
	{0xfe, 0x02},
	{0x14, 0x01},
	{0x15, 0x02},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},

	/* BLK */
	{0xfe, 0x01},
	{0x49, 0x03},
	{0x4a, 0xff},
	{0x4b, 0xc0},

	/* Anti_blooming */
	{0xfe, 0x01},
	{0x4e, 0x3c},
	{0x44, 0x08},

	/* Crop */
	{0xfe, 0x01},
	{0x91, 0x00},
	{0x92, 0x08},
	{0x93, 0x00},
	{0x94, 0x07},
	{0x95, 0x07},
	{0x96, 0x98},
	{0x97, 0x0a},
	{0x98, 0x20},
	{0x99, 0x00},

	/* MIPI */
	{0xfe, 0x03},
	{0x02, 0x57},
	{0x22, 0x06},
	{0x26, 0x08},
	{0x29, 0x06},
	{0x2b, 0x08},
	{0xfe, 0x01},
	{0x8c, 0x10},

	{0xfe, 0x00},
	{0x3e, 0x91},
};

/*
 * Xclk 24Mhz
 * Pclk 87.6Mhz
 * grabwindow_width 1296
 * grabwindow_height 972
 * mipi_datarate per lane 876Mbps
 */
static const struct gc5035_regval gc5035_1296x972_regs[] = {
	/*NULL*/
	{0xfe, 0x00},
	{0x3e, 0x01},
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe4},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x12},
	{0xfa, 0x01},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x20},
	{0xfe, 0x03},
	{0x01, 0x87},
	{0xf7, 0x11},
	{0xfc, 0x8f},
	{0xfc, 0x8f},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0x87, 0x18},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},

	/* Analog & CISCTL */
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0xda},
	{0x9d, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x04},
	{0x0b, 0x00},
	{0x0c, 0x03},
	{0x0d, 0x07},
	{0x0e, 0xa8},
	{0x0f, 0x0a},
	{0x10, 0x30},
	{0x21, 0x60},
	{0x29, 0x30},
	{0x44, 0x18},
	{0x4e, 0x20},
	{0x8c, 0x20},
	{0x91, 0x15},
	{0x92, 0x3a},
	{0x93, 0x20},
	{0x95, 0x45},
	{0x96, 0x35},
	{0xd5, 0xf0},
	{0x97, 0x20},
	{0x1f, 0x19},
	{0xce, 0x18},
	{0xd0, 0xb3},
	{0xfe, 0x02},
	{0x14, 0x02},
	{0x15, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},

	/* BLK */
	{0xfe, 0x01},
	{0x49, 0x00},
	{0x4a, 0x01},
	{0x4b, 0xf8},

	/* Anti_blooming */
	{0xfe, 0x01},
	{0x4e, 0x06},
	{0x44, 0x02},

	/* Crop */
	{0xfe, 0x01},
	{0x91, 0x00},
	{0x92, 0x04},
	{0x93, 0x00},
	{0x94, 0x03},
	{0x95, 0x03},
	{0x96, 0xcc},
	{0x97, 0x05},
	{0x98, 0x10},
	{0x99, 0x00},

	/* MIPI */
	{0xfe, 0x03},
	{0x02, 0x58},
	{0x22, 0x03},
	{0x26, 0x06},
	{0x29, 0x03},
	{0x2b, 0x06},
	{0xfe, 0x01},
	{0x8c, 0x10},
};

/*
 * Xclk 24Mhz
 * Pclk 87.6Mhz
 * linelength 672{0x2a0)
 * framelength 2232{0x8b8)
 * grabwindow_width 1280
 * grabwindow_height 720
 * max_framerate 30fps
 * mipi_datarate per lane 876Mbps
 */
static const struct gc5035_regval gc5035_1280x720_regs[] = {
	/* System */
	{0xfe, 0x00},
	{0x3e, 0x01},
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe4},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x12},
	{0xfa, 0x01},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x20},
	{0xfe, 0x03},
	{0x01, 0x87},
	{0xf7, 0x11},
	{0xfc, 0x8f},
	{0xfc, 0x8f},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0x87, 0x18},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},

	/* Analog & CISCTL */
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0xda},
	{0x9d, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x04},
	{0x0b, 0x00},
	{0x0c, 0x03},
	{0x0d, 0x07},
	{0x0e, 0xa8},
	{0x0f, 0x0a},
	{0x10, 0x30},
	{0x21, 0x60},
	{0x29, 0x30},
	{0x44, 0x18},
	{0x4e, 0x20},
	{0x8c, 0x20},
	{0x91, 0x15},
	{0x92, 0x3a},
	{0x93, 0x20},
	{0x95, 0x45},
	{0x96, 0x35},
	{0xd5, 0xf0},
	{0x97, 0x20},
	{0x1f, 0x19},
	{0xce, 0x18},
	{0xd0, 0xb3},
	{0xfe, 0x02},
	{0x14, 0x02},
	{0x15, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},

	/* BLK */
	{0xfe, 0x01},
	{0x49, 0x00},
	{0x4a, 0x01},
	{0x4b, 0xf8},

	/* Anti_blooming */
	{0xfe, 0x01},
	{0x4e, 0x06},
	{0x44, 0x02},

	/* Crop */
	{0xfe, 0x01},
	{0x91, 0x00},
	{0x92, 0x0a},
	{0x93, 0x00},
	{0x94, 0x0b},
	{0x95, 0x02},
	{0x96, 0xd0},
	{0x97, 0x05},
	{0x98, 0x00},
	{0x99, 0x00},

	/* MIPI */
	{0xfe, 0x03},
	{0x02, 0x58},
	{0x22, 0x03},
	{0x26, 0x06},
	{0x29, 0x03},
	{0x2b, 0x06},
	{0xfe, 0x01},
	{0x8c, 0x10},
	{0xfe, 0x00},
	{0x3e, 0x91},
};

static const struct gc5035_mode gc5035_modes[] = {
	{
		.width = 2592,
		.height = 1944,
		.max_fps = 30,
		.exp_def = 0x258,
		.hts_def = 2920,
		.vts_def = 2008,
		.reg_list = gc5035_2592x1944_regs,
		.num_regs = ARRAY_SIZE(gc5035_2592x1944_regs),
	},
	{
		.width = 1296,
		.height = 972,
		.max_fps = 30,
		.exp_def = 0x258,
		.hts_def = 1460,
		.vts_def = 2008,
		.reg_list = gc5035_1296x972_regs,
		.num_regs = ARRAY_SIZE(gc5035_1296x972_regs),
	},
	{
		.width = 1280,
		.height = 720,
		.max_fps = 60,
		.exp_def = 0x258,
		.hts_def = 1896,
		.vts_def = 1536,
		.reg_list = gc5035_1280x720_regs,
		.num_regs = ARRAY_SIZE(gc5035_1280x720_regs),
	},
};

static const char * const gc5035_test_pattern_menu[] = {
	"Disabled",
	"Color Bar",
};

static const s64 gc5035_link_freqs[] = {
	438000000,
};

static u64 gc5035_link_to_pixel_rate(u32 f_index)
{
	u64 pixel_rate = gc5035_link_freqs[f_index] * 2 * GC5035_DATA_LANES;

	do_div(pixel_rate, GC5035_BITS_PER_SAMPLE);

	return pixel_rate;
}

static int gc5035_write_reg(struct gc5035 *gc5035, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(gc5035->client, reg, val);
}

static int gc5035_write_array(struct gc5035 *gc5035,
			      const struct gc5035_regval *regs,
			      size_t num_regs)
{
	unsigned int i;
	int ret;

	for (i = 0; i < num_regs; i++) {
		ret = gc5035_write_reg(gc5035, regs[i].addr, regs[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

static int gc5035_read_reg(struct gc5035 *gc5035, u8 reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(gc5035->client, reg);
	if (ret < 0)
		return ret;

	*val = (unsigned char)ret;

	return 0;
}

static int gc5035_otp_read_data(struct gc5035 *gc5035, u16 bit_addr, u8 *data,
				size_t length)
{
	size_t i;
	int ret;

	if (WARN_ON(bit_addr % 8))
		return -EINVAL;

	if (WARN_ON(bit_addr / 8 + length > GC5035_OTP_DATA_LENGTH))
		return -EINVAL;

	ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG, 2);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_OTP_ACCESS_ADDR_H,
			       (bit_addr >> 8) &
			       GC5035_OTP_ACCESS_ADDR_H_MASK);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_OTP_ACCESS_ADDR_L,
			       bit_addr & 0xff);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_OTP_MODE,
			       GC5035_OTP_PRE_READ);
	if (ret)
		goto out_read_done;

	ret = gc5035_write_reg(gc5035, GC5035_REG_OTP_MODE,
			       GC5035_OTP_READ_MODE);
	if (ret)
		goto out_read_done;

	for (i = 0; i < length; i++) {
		ret = gc5035_read_reg(gc5035, GC5035_REG_OTP_DATA, &data[i]);
		if (ret)
			goto out_read_done;
	}

out_read_done:
	gc5035_write_reg(gc5035, GC5035_REG_OTP_MODE, GC5035_OTP_READ_DONE);

	return ret;
}

static int gc5035_read_otp_regs(struct gc5035 *gc5035)
{
	struct device *dev = &gc5035->client->dev;
	struct gc5035_otp_regs *otp_regs = &gc5035->otp_regs;
	u8 regs[GC5035_OTP_REG_DATA_SIZE] = {0};
	unsigned int i, j;
	u8 flag;
	int ret;

	ret = gc5035_otp_read_data(gc5035, GC5035_OTP_REG_FLAG_OFFSET,
				   &flag, 1);
	if (ret) {
		dev_err(dev, "failed to read otp reg flag\n");
		return ret;
	}

	dev_dbg(dev, "register update flag = 0x%x\n", flag);

	gc5035->otp_regs.num_regs = 0;
	if (flag != GC5035_OTP_FLAG_VALID)
		return 0;

	ret = gc5035_otp_read_data(gc5035, GC5035_OTP_REG_DATA_OFFSET,
				   regs, sizeof(regs));
	if (ret) {
		dev_err(dev, "failed to read otp reg data\n");
		return ret;
	}

	for (i = 0; i < GC5035_OTP_REG_MAX_GROUP; i++) {
		unsigned int base_group = i * GC5035_OTP_REG_BYTE_PER_GROUP;

		for (j = 0; j < GC5035_OTP_REG_PER_GROUP; j++) {
			struct gc5035_reg *reg;

			if (!(regs[base_group] &
			      BIT((GC5035_OTP_PER_PAGE_SIZE * j +
				  GC5035_OTP_PAGE_FLAG_OFFSET))))
				continue;

			reg = &otp_regs->regs[otp_regs->num_regs++];
			reg->page = (regs[base_group] >>
					(GC5035_OTP_PER_PAGE_SIZE * j)) &
					GC5035_OTP_REG_PAGE_MASK;
			reg->regval.addr = regs[base_group + j *
					GC5035_OTP_REG_BYTE_PER_REG +
					GC5035_OTP_REG_ADDR_OFFSET];
			reg->regval.val = regs[base_group + j *
					GC5035_OTP_REG_BYTE_PER_REG +
					GC5035_OTP_REG_VAL_OFFSET];
		}
	}

	return 0;
}

static int gc5035_read_dpc(struct gc5035 *gc5035)
{
	struct device *dev = &gc5035->client->dev;
	struct gc5035_dpc *dpc = &gc5035->dpc;
	u8 dpc_flag = 0;
	u8 error_number = 0;
	u8 total_number = 0;
	int ret;

	ret = gc5035_otp_read_data(gc5035, GC5035_OTP_DPC_FLAG_OFFSET,
				   &dpc_flag, 1);
	if (ret) {
		dev_err(dev, "failed to read dpc flag\n");
		return ret;
	}

	dev_dbg(dev, "dpc flag = 0x%x\n", dpc_flag);

	dpc->valid = false;

	switch (dpc_flag & GC5035_OTP_DPC_FLAG_MASK) {
	case GC5035_OTP_FLAG_EMPTY:
		dev_dbg(dev, "dpc info is empty!!\n");
		break;

	case GC5035_OTP_FLAG_VALID:
		dev_dbg(dev, "dpc info is valid!\n");
		ret = gc5035_otp_read_data(gc5035,
					   GC5035_OTP_DPC_TOTAL_NUMBER_OFFSET,
					   &total_number, 1);
		if (ret) {
			dev_err(dev, "failed to read dpc total number\n");
			return ret;
		}

		ret = gc5035_otp_read_data(gc5035,
					   GC5035_OTP_DPC_ERROR_NUMBER_OFFSET,
					   &error_number, 1);
		if (ret) {
			dev_err(dev, "failed to read dpc error number\n");
			return ret;
		}

		dpc->total_num = total_number + error_number;
		dpc->valid = true;
		dev_dbg(dev, "total_num = %d\n", dpc->total_num);
		break;

	default:
		break;
	}

	return ret;
}

static int gc5035_otp_read_sensor_info(struct gc5035 *gc5035)
{
	int ret;

	ret = gc5035_read_dpc(gc5035);
	if (ret)
		return ret;

	return gc5035_read_otp_regs(gc5035);
}

static int gc5035_check_dd_load_status(struct gc5035 *gc5035)
{
	u8 status;
	int ret;

	ret = gc5035_read_reg(gc5035, GC5035_REG_DD_LOAD_STATUS, &status);
	if (ret)
		return ret;

	if (status & GC5035_OTP_BIT_LOAD)
		return status;
	else
		return 0;
}

static int gc5035_otp_update_dd(struct gc5035 *gc5035)
{
	struct device *dev = &gc5035->client->dev;
	struct gc5035_dpc *dpc = &gc5035->dpc;
	int val, ret;

	if (!dpc->valid) {
		dev_dbg(dev, "DPC table invalid, not updating DD.\n");
		return 0;
	}

	dev_dbg(dev, "DD auto load start\n");

	ret = gc5035_write_array(gc5035, gc5035_dd_auto_load_regs,
				 ARRAY_SIZE(gc5035_dd_auto_load_regs));
	if (ret) {
		dev_err(dev, "failed to write dd auto load reg\n");
		return ret;
	}

	ret = gc5035_write_reg(gc5035, GC5035_REG_DD_TOTALNUM_H,
			       (dpc->total_num >> 8) &
			       GC5035_DD_TOTALNUM_H_MASK);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_DD_TOTALNUM_L,
			       dpc->total_num & 0xff);
	if (ret)
		return ret;

	ret = gc5035_write_array(gc5035, gc5035_otp_dd_regs,
				 ARRAY_SIZE(gc5035_otp_dd_regs));
	if (ret)
		return ret;

	/* Wait for DD to finish loading automatically */
	ret = readx_poll_timeout(gc5035_check_dd_load_status, gc5035,
				val, val <= 0, GC5035_DD_DELAY_US,
				GC5035_DD_TIMEOUT_US);
	if (ret < 0) {
		dev_err(dev, "DD load timeout\n");
		return -EFAULT;
	}
	if (val < 0) {
		dev_err(dev, "DD load failure\n");
		return val;
	}

	ret = gc5035_write_array(gc5035, gc5035_otp_dd_enable_regs,
				 ARRAY_SIZE(gc5035_otp_dd_enable_regs));
	if (ret)
		return ret;

	return 0;
}

static int gc5035_otp_update_regs(struct gc5035 *gc5035)
{
	struct device *dev = &gc5035->client->dev;
	struct gc5035_otp_regs *otp_regs = &gc5035->otp_regs;
	unsigned int i;
	int ret;

	dev_dbg(dev, "reg count = %d\n", otp_regs->num_regs);

	for (i = 0; i < otp_regs->num_regs; i++) {
		ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG,
				       otp_regs->regs[i].page);
		if (ret)
			return ret;

		ret = gc5035_write_reg(gc5035,
				       otp_regs->regs[i].regval.addr,
				       otp_regs->regs[i].regval.val);
		if (ret)
			return ret;
	}

	return 0;
}

static int gc5035_otp_update(struct gc5035 *gc5035)
{
	struct device *dev = &gc5035->client->dev;
	int ret;

	ret = gc5035_otp_update_dd(gc5035);
	if (ret) {
		dev_err(dev, "failed to update otp dd\n");
		return ret;
	}

	ret = gc5035_otp_update_regs(gc5035);
	if (ret) {
		dev_err(dev, "failed to update otp regs\n");
		return ret;
	}

	return ret;
}

static int gc5035_set_otp_config(struct gc5035 *gc5035)
{
	struct device *dev = &gc5035->client->dev;
	u8 otp_id[GC5035_OTP_ID_SIZE];
	int ret;

	ret = gc5035_write_array(gc5035, gc5035_otp_init_regs,
				 ARRAY_SIZE(gc5035_otp_init_regs));
	if (ret) {
		dev_err(dev, "failed to write otp init reg\n");
		return ret;
	}

	ret = gc5035_otp_read_data(gc5035, GC5035_OTP_ID_DATA_OFFSET,
				   &otp_id[0], GC5035_OTP_ID_SIZE);
	if (ret) {
		dev_err(dev, "failed to read otp id\n");
		goto out_otp_exit;
	}

	if (!gc5035->otp_read || memcmp(gc5035->otp_id, otp_id, sizeof(otp_id))) {
		dev_dbg(dev, "reading OTP configuration\n");

		memset(&gc5035->otp_regs, 0, sizeof(gc5035->otp_regs));
		memset(&gc5035->dpc, 0, sizeof(gc5035->dpc));

		memcpy(gc5035->otp_id, otp_id, sizeof(gc5035->otp_id));

		ret = gc5035_otp_read_sensor_info(gc5035);
		if (ret < 0) {
			dev_err(dev, "failed to read otp info\n");
			goto out_otp_exit;
		}

		gc5035->otp_read = true;
	}

	ret = gc5035_otp_update(gc5035);
	if (ret < 0)
		return ret;

out_otp_exit:
	ret = gc5035_write_array(gc5035, gc5035_otp_exit_regs,
				 ARRAY_SIZE(gc5035_otp_exit_regs));
	if (ret) {
		dev_err(dev, "failed to write otp exit reg\n");
		return ret;
	}

	return ret;
}

static int gc5035_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct gc5035 *gc5035 = to_gc5035(sd);
	const struct gc5035_mode *mode;
	s64 h_blank, vblank_def;

	mode = v4l2_find_nearest_size(gc5035_modes,
				      ARRAY_SIZE(gc5035_modes), width,
				      height, fmt->format.width,
				      fmt->format.height);

	fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;

	mutex_lock(&gc5035->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
	} else {
		gc5035->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc5035->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = round_up(mode->vts_def, 4) - mode->height;
		__v4l2_ctrl_modify_range(gc5035->vblank, vblank_def,
					 GC5035_VTS_MAX - mode->height,
					 4, vblank_def);
	}
	mutex_unlock(&gc5035->mutex);

	return 0;
}

static int gc5035_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct gc5035 *gc5035 = to_gc5035(sd);
	const struct gc5035_mode *mode = gc5035->cur_mode;

	mutex_lock(&gc5035->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gc5035->mutex);

	return 0;
}

static int gc5035_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return 0;
}

static int gc5035_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(gc5035_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SRGGB10_1X10)
		return -EINVAL;

	fse->min_width  = gc5035_modes[fse->index].width;
	fse->max_width  = gc5035_modes[fse->index].width;
	fse->max_height = gc5035_modes[fse->index].height;
	fse->min_height = gc5035_modes[fse->index].height;

	return 0;
}

static int __gc5035_start_stream(struct gc5035 *gc5035)
{
	int ret;

	ret = gc5035_write_array(gc5035, gc5035_global_regs,
				 ARRAY_SIZE(gc5035_global_regs));
	if (ret)
		return ret;

	ret = gc5035_set_otp_config(gc5035);
	if (ret)
		return ret;

	ret = gc5035_write_array(gc5035, gc5035->cur_mode->reg_list,
				 gc5035->cur_mode->num_regs);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&gc5035->ctrl_handler);
	if (ret)
		return ret;

	gc5035_write_reg(gc5035, GC5035_PAGE_REG, 0);
	if (ret)
		return ret;

	return gc5035_write_reg(gc5035, GC5035_REG_CTRL_MODE,
				GC5035_MODE_STREAMING);
}

static int __gc5035_stop_stream(struct gc5035 *gc5035)
{
	int ret;

	ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG, 0);
	if (ret)
		return ret;

	return gc5035_write_reg(gc5035, GC5035_REG_CTRL_MODE,
				GC5035_MODE_SW_STANDBY);
}

static int gc5035_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc5035 *gc5035 = to_gc5035(sd);
	struct i2c_client *client = gc5035->client;
	int ret = 0;

	mutex_lock(&gc5035->mutex);
	on = !!on;
	if (on == gc5035->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc5035_start_stream(gc5035);
		if (ret) {
			dev_err(&client->dev, "start stream failed\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc5035_stop_stream(gc5035);
		pm_runtime_put(&client->dev);
	}

	gc5035->streaming = on;

unlock_and_return:
	mutex_unlock(&gc5035->mutex);

	return ret;
}

static int gc5035_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc5035 *gc5035 = to_gc5035(sd);
	unsigned long delay;
	int ret;

	/* IOVDD must be enabled first. */
	ret = regulator_enable(gc5035->iovdd_supply);
	if (ret) {
		dev_err(dev, "Failed to enable iovdd regulator: %d\n", ret);
		return ret;
	}

	/* Wait at least 50 us between IOVDD and AVDD/DVDD. */
	usleep_range(50, 150);

	ret = regulator_bulk_enable(ARRAY_SIZE(gc5035_supplies),
				    gc5035->supplies);
	if (ret) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		goto disable_iovdd;
	}

	gpiod_set_value_cansleep(gc5035->resetb_gpio, 0);
	gpiod_set_value_cansleep(gc5035->pwdn_gpio, 0);

	ret = clk_prepare_enable(gc5035->mclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable mclk\n");
		goto enable_pwdn;
	}

	/* min. 1200 MCLK cycles required before first I2C transaction. */
	delay = DIV_ROUND_UP(1200UL * USEC_PER_SEC, gc5035->mclk_rate);
	usleep_range(delay, delay + 200);

	return 0;

enable_pwdn:
	gpiod_set_value_cansleep(gc5035->pwdn_gpio, 1);
	gpiod_set_value_cansleep(gc5035->resetb_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(gc5035_supplies), gc5035->supplies);
disable_iovdd:
	regulator_disable(gc5035->iovdd_supply);

	return ret;
}

static int gc5035_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc5035 *gc5035 = to_gc5035(sd);
	unsigned long delay;

	/* min. 2000 MCLK cycles required after streaming stops. */
	delay = DIV_ROUND_UP(2000UL * USEC_PER_SEC, gc5035->mclk_rate);
	usleep_range(delay, delay + 200);

	clk_disable_unprepare(gc5035->mclk);

	gpiod_set_value_cansleep(gc5035->pwdn_gpio, 1);
	gpiod_set_value_cansleep(gc5035->resetb_gpio, 1);

	regulator_bulk_disable(ARRAY_SIZE(gc5035_supplies), gc5035->supplies);
	regulator_disable(gc5035->iovdd_supply);

	return 0;
}

static int gc5035_entity_init_cfg(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.format = {
			.width = 2592,
			.height = 1944,
		}
	};

	gc5035_set_fmt(subdev, cfg, &fmt);

	return 0;
}

static const struct dev_pm_ops gc5035_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(gc5035_runtime_suspend,
			   gc5035_runtime_resume, NULL)
};

static const struct v4l2_subdev_video_ops gc5035_video_ops = {
	.s_stream = gc5035_s_stream,
};

static const struct v4l2_subdev_pad_ops gc5035_pad_ops = {
	.init_cfg = gc5035_entity_init_cfg,
	.enum_mbus_code = gc5035_enum_mbus_code,
	.enum_frame_size = gc5035_enum_frame_sizes,
	.get_fmt = gc5035_get_fmt,
	.set_fmt = gc5035_set_fmt,
};

static const struct v4l2_subdev_ops gc5035_subdev_ops = {
	.video	= &gc5035_video_ops,
	.pad	= &gc5035_pad_ops,
};

static const struct media_entity_operations gc5035_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int gc5035_set_exposure(struct gc5035 *gc5035, int val)
{
	int ret;

	ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG, 0);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_EXPOSURE_H,
			       (val >> 8) & GC5035_EXPOSURE_H_MASK);
	if (ret)
		return ret;

	return gc5035_write_reg(gc5035, GC5035_REG_EXPOSURE_L, val & 0xff);
}

static int gc5035_set_analogue_gain(struct gc5035 *gc5035, int val)
{
	int ret;

	ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG, 0);
	if (ret)
		return ret;

	return gc5035_write_reg(gc5035, GC5035_REG_ANALOG_GAIN, val);
}

static int gc5035_set_digital_gain(struct gc5035 *gc5035, int val)
{
	int ret;

	ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG, 0);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_DIGI_GAIN_H,
			       (val >> (8 - GC5035_DGAIN_L_SHIFT))
			       & GC5035_DGAIN_H_MASK);
	if (ret)
		return ret;

	return gc5035_write_reg(gc5035, GC5035_REG_DIGI_GAIN_L,
				(val << GC5035_DGAIN_L_SHIFT)
				& GC5035_DGAIN_L_MASK);
}

static int gc5035_set_vblank(struct gc5035 *gc5035, int val)
{
	int frame_length = 0;
	int ret;

	frame_length = val + gc5035->cur_mode->height;

	ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG, 0);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_VTS_H,
			       (frame_length >> 8) & GC5035_VTS_H_MASK);
	if (ret)
		return ret;

	return gc5035_write_reg(gc5035, GC5035_REG_VTS_L, frame_length & 0xff);
}

static int gc5035_enable_test_pattern(struct gc5035 *gc5035, int pattern)
{
	int ret;

	if (pattern)
		pattern = GC5035_TEST_PATTERN_ENABLE;
	else
		pattern = GC5035_TEST_PATTERN_DISABLE;

	ret = gc5035_write_reg(gc5035, GC5035_PAGE_REG, 1);
	if (ret)
		return ret;

	ret = gc5035_write_reg(gc5035, GC5035_REG_TEST_PATTERN, pattern);
	if (ret)
		return ret;

	return gc5035_write_reg(gc5035, GC5035_PAGE_REG, 0);
}

static int gc5035_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc5035 *gc5035 = container_of(ctrl->handler,
					     struct gc5035, ctrl_handler);
	struct i2c_client *client = gc5035->client;
	s64 max;
	int ret;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = gc5035->cur_mode->height + ctrl->val
			- GC5035_EXPOSURE_MARGIN;
		__v4l2_ctrl_modify_range(gc5035->exposure,
					 gc5035->exposure->minimum, max,
					 gc5035->exposure->step,
					 gc5035->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ret = gc5035_set_exposure(gc5035, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = gc5035_set_analogue_gain(gc5035, ctrl->val);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = gc5035_set_digital_gain(gc5035, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = gc5035_set_vblank(gc5035, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = gc5035_enable_test_pattern(gc5035, ctrl->val);
		break;
	default:
		ret = -EINVAL;
		break;
	};

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc5035_ctrl_ops = {
	.s_ctrl = gc5035_set_ctrl,
};

static int gc5035_initialize_controls(struct gc5035 *gc5035)
{
	const struct gc5035_mode *mode;
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	u64 exposure_max, pixel_rate;
	u32 h_blank, vblank_def;
	int ret;

	handler = &gc5035->ctrl_handler;
	mode = gc5035->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;

	handler->lock = &gc5035->mutex;

	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, gc5035_link_freqs);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	pixel_rate = gc5035_link_to_pixel_rate(0);
	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, pixel_rate, 1, pixel_rate);

	h_blank = mode->hts_def - mode->width;
	gc5035->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (gc5035->hblank)
		gc5035->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = round_up(mode->vts_def, 4) - mode->height;
	gc5035->vblank = v4l2_ctrl_new_std(handler, &gc5035_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   GC5035_VTS_MAX - mode->height,
					   4, vblank_def);

	exposure_max = mode->vts_def - GC5035_EXPOSURE_MARGIN;
	gc5035->exposure = v4l2_ctrl_new_std(handler, &gc5035_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     GC5035_EXPOSURE_MIN, exposure_max,
					     GC5035_EXPOSURE_STEP,
					     mode->exp_def);

	v4l2_ctrl_new_std(handler, &gc5035_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  GC5035_ANALOG_GAIN_MIN, GC5035_ANALOG_GAIN_MAX,
			  GC5035_ANALOG_GAIN_STEP, GC5035_ANALOG_GAIN_DEFAULT);

	v4l2_ctrl_new_std(handler, &gc5035_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  GC5035_DIGI_GAIN_MIN, GC5035_DIGI_GAIN_MAX,
			  GC5035_DIGI_GAIN_STEP, GC5035_DIGI_GAIN_DEFAULT);

	v4l2_ctrl_new_std_menu_items(handler, &gc5035_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(gc5035_test_pattern_menu) - 1,
				     0, 0, gc5035_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gc5035->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc5035->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc5035_check_sensor_id(struct gc5035 *gc5035,
				  struct i2c_client *client)
{
	struct device *dev = &gc5035->client->dev;
	u16 id;
	u8 pid = 0;
	u8 ver = 0;
	int ret;

	ret = gc5035_read_reg(gc5035, GC5035_REG_CHIP_ID_H, &pid);
	if (ret)
		return ret;

	ret = gc5035_read_reg(gc5035, GC5035_REG_CHIP_ID_L, &ver);
	if (ret)
		return ret;

	id = GC5035_ID(pid, ver);
	if (id != GC5035_CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%04x)\n", id);
		return -EINVAL;
	}

	dev_dbg(dev, "Detected GC%04x sensor\n", id);

	return 0;
}

static int gc5035_get_hwcfg(struct gc5035 *gc5035)
{
	struct device *dev = &gc5035->client->dev;
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	struct fwnode_handle *ep;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	unsigned long link_freq_mask = 0;
	unsigned int i, j;
	int ret;

	if (!fwnode)
		return -ENODEV;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENODEV;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	if (ret)
		goto out;

	dev_dbg(dev, "num of link freqs: %d", bus_cfg.nr_of_link_frequencies);
	if (!bus_cfg.nr_of_link_frequencies) {
		dev_warn(dev, "no link frequencies defined");
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(gc5035_link_freqs); ++i) {
		for (j = 0; j < bus_cfg.nr_of_link_frequencies; j++) {
			if (bus_cfg.link_frequencies[j]
			    == gc5035_link_freqs[i]) {
				link_freq_mask |= BIT(i);
				dev_dbg(dev, "Link frequency %lld supported\n",
					gc5035_link_freqs[i]);
				break;
			}
		}
	}
	if (!link_freq_mask) {
		dev_err(dev, "No supported link frequencies found\n");
		ret = -EINVAL;
		goto out;
	}

out:
	v4l2_fwnode_endpoint_free(&bus_cfg);
	fwnode_handle_put(ep);
	return ret;
}

static int gc5035_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct gc5035 *gc5035;
	struct v4l2_subdev *sd;
	int ret, i;
	u32 freq;

	gc5035 = devm_kzalloc(dev, sizeof(*gc5035), GFP_KERNEL);
	if (!gc5035)
		return -ENOMEM;

	gc5035->client = client;
	gc5035->cur_mode = &gc5035_modes[0];

	gc5035->mclk = devm_clk_get(dev, "mclk");
	if (IS_ERR(gc5035->mclk))
		return dev_err_probe(dev, PTR_ERR(gc5035->mclk), "Failed to get mclk\n");

	ret = device_property_read_u32(dev, "clock-frequency", &freq);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get clock-frequency\n");
	if (freq != GC5035_MCLK_RATE)
		dev_warn(dev, "Given clock-frequency %u is different from expected %lu\n",
			 freq, GC5035_MCLK_RATE);

	ret = gc5035_get_hwcfg(gc5035);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get hardware config\n");

	ret = clk_set_rate(gc5035->mclk, freq);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to set mclk rate\n");
	gc5035->mclk_rate = clk_get_rate(gc5035->mclk);
	if (gc5035->mclk_rate != freq)
		dev_warn(dev, "mclk rate set to %lu instead of requested %u\n",
			 gc5035->mclk_rate, freq);

	gc5035->pwdn_gpio = devm_gpiod_get_optional(dev, "pwdn", GPIOD_OUT_HIGH);
	if (IS_ERR(gc5035->pwdn_gpio))
		return dev_err_probe(dev, PTR_ERR(gc5035->pwdn_gpio),
				     "Failed to get pwdn-gpios\n");

	gc5035->resetb_gpio = devm_gpiod_get(dev, "resetb", GPIOD_OUT_HIGH);
	if (IS_ERR(gc5035->resetb_gpio))
		return dev_err_probe(dev, PTR_ERR(gc5035->resetb_gpio),
				     "Failed to get resetb-gpios\n");

	gc5035->iovdd_supply = devm_regulator_get(dev, "iovdd");
	if (IS_ERR(gc5035->iovdd_supply))
		return dev_err_probe(dev, PTR_ERR(gc5035->iovdd_supply),
				     "Failed to get iovdd regulator\n");

	for (i = 0; i < ARRAY_SIZE(gc5035_supplies); i++)
		gc5035->supplies[i].supply = gc5035_supplies[i];
	ret = devm_regulator_bulk_get(&gc5035->client->dev,
				       ARRAY_SIZE(gc5035_supplies),
				       gc5035->supplies);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	mutex_init(&gc5035->mutex);

	sd = &gc5035->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc5035_subdev_ops);

	ret = gc5035_initialize_controls(gc5035);
	if (ret) {
		dev_err_probe(dev, ret, "Failed to initialize controls\n");
		goto err_destroy_mutex;
	}

	ret = gc5035_runtime_resume(dev);
	if (ret) {
		dev_err_probe(dev, ret, "Failed to power on\n");
		goto err_free_handler;
	}

	ret = gc5035_check_sensor_id(gc5035, client);
	if (ret) {
		dev_err_probe(dev, ret, "Sensor ID check failed\n");
		goto err_power_off;
	}

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.ops = &gc5035_subdev_entity_ops;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	gc5035->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sd->entity, 1, &gc5035->pad);
	if (ret < 0) {
		dev_err_probe(dev, ret, "Failed to initialize pads\n");
		goto err_power_off;
	}

	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret) {
		dev_err_probe(dev, ret, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
	media_entity_cleanup(&sd->entity);
err_power_off:
	gc5035_runtime_suspend(dev);
err_free_handler:
	v4l2_ctrl_handler_free(&gc5035->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc5035->mutex);

	return ret;
}

static int gc5035_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc5035 *gc5035 = to_gc5035(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&gc5035->ctrl_handler);
	mutex_destroy(&gc5035->mutex);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		gc5035_runtime_suspend(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static const struct of_device_id gc5035_of_match[] = {
	{ .compatible = "galaxycore,gc5035" },
	{},
};
MODULE_DEVICE_TABLE(of, gc5035_of_match);

static struct i2c_driver gc5035_i2c_driver = {
	.driver = {
		.name = "gc5035",
		.pm = &gc5035_pm_ops,
		.of_match_table = gc5035_of_match,
	},
	.probe_new	= &gc5035_probe,
	.remove		= &gc5035_remove,
};
module_i2c_driver(gc5035_i2c_driver);

MODULE_AUTHOR("Hao He <hao.he@bitland.com.cn>");
MODULE_AUTHOR("Xingyu Wu <wuxy@bitland.com.cn>");
MODULE_AUTHOR("Tomasz Figa <tfiga@chromium.org>");
MODULE_DESCRIPTION("GalaxyCore gc5035 sensor driver");
MODULE_LICENSE("GPL v2");
