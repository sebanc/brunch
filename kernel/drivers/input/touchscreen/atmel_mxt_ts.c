/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/platform_data/atmel_mxt_ts.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* Version */
#define MXT_VER_20		20
#define MXT_VER_21		21
#define MXT_VER_22		22

/* Firmware */
#define MXT_FW_NAME		"maxtouch.fw"

/* Config file */
#define MXT_CONFIG_NAME		"maxtouch.cfg"

/* Configuration Data */
#define MXT_CONFIG_VERSION	"OBP_RAW V1"

/* Registers */
#define MXT_INFO		0x00
#define MXT_FAMILY_ID		0x00
#define MXT_VARIANT_ID		0x01
#define MXT_VERSION		0x02
#define MXT_BUILD		0x03
#define MXT_MATRIX_X_SIZE	0x04
#define MXT_MATRIX_Y_SIZE	0x05
#define MXT_OBJECT_NUM		0x06
#define MXT_OBJECT_START	0x07

#define MXT_OBJECT_SIZE		6

/* Object types */
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_GEN_MESSAGE_T5		5
#define MXT_GEN_COMMAND_T6		6
#define MXT_GEN_POWER_T7		7
#define MXT_GEN_ACQUIRE_T8		8
#define MXT_GEN_DATASOURCE_T53		53
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_PROCI_ADAPTIVETHRESHOLD_T55 55
#define MXT_PROCI_SHIELDLESS_T56	56
#define MXT_PROCI_EXTRATOUCHSCREENDATA_T57	57
#define MXT_PROCG_NOISESUPPRESSION_T62	62
#define MXT_PROCI_LENSBENDING_T65	65
#define MXT_PROCG_NOISESUPPRESSION_T72	72
#define MXT_PROCI_GLOVEDETECTION_T78	78
#define MXT_PROCI_RETRANSMISSIONCOMPANSATION_T80	80
#define MXT_PROCG_NOISESUPACTIVESTYLUS_T86	86
#define MXT_PROCI_SYMBOLGESTUREPROCESSOR_T92	92
#define MXT_PROCI_TOUCHSEQUENCELOGGER_T93	93
#define MXT_PROCG_PTCNOISESUPPRESSION_T98	98
#define MXT_PROCI_ACTIVESTYLUS_T107	107
#define MXT_PROCG_NOISESUPSELFCAP_T108	108
#define MXT_PROCI_SELFCAPGRIPSUPPRESSION_T112	112
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_USERDATA_T38		38
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_SPT_TIMER_T61		61
#define MXT_SPT_GOLDENREFERENCES_T66	66
#define MXT_SPT_SERIALDATACOMMAND_T68	68
#define MXT_SPT_DYNAMICCONFIGURATIONCONTROLLER_T70	70
#define MXT_SPT_DYNAMICCONFIGURATIONCONTAINER_T71	71
#define MXT_SPT_CTESCANCONFIG_T77	77
#define MXT_SPT_TOUCHEVENTTRIGGER_T79	79
#define MXT_SPT_PTCCONFIG_T95		95
#define MXT_SPT_PTCTUNINGPARAMS_T96	96
#define MXT_SPT_TOUCHSCREENHOVER_T101	101
#define MXT_SPT_AUXTOUCHCONFIG_T104	104
#define MXT_SPT_SELFCAPGLOBALCONFIG_T109	109
#define MXT_SPT_SELFCAPTUNINGPARAMS_T110	110
#define MXT_SPT_SELFCAPCONFIG_T111	111
#define MXT_SPT_SELFCAPMEASURECONFIG_T113	113
#define MXT_SPT_ACTIVESTYLUSMEASCONFIG_T114	114
#define MXT_TOUCH_MULTI_T9		9
#define MXT_TOUCH_KEYARRAY_T15		15
#define MXT_TOUCH_PROXIMITY_T23		23
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_TOUCH_PTCKEYS_T97		97
#define MXT_TOUCH_MULTITOUCHSCREEN_T100 100

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

#define MXT_T6_CMD_PAGE_UP		0x01
#define MXT_T6_CMD_PAGE_DOWN		0x02
#define MXT_T6_CMD_DELTAS		0x10
#define MXT_T6_CMD_REFS			0x11
#define MXT_T6_CMD_DEVICE_ID		0x80
#define MXT_T6_CMD_TOUCH_THRESH		0xF4
#define MXT_T6_CMD_SELF_DELTAS		0xF7
#define MXT_T6_CMD_SELF_REFS		0xF8

/* MXT_GEN_POWER_T7 field */
#define MXT_POWER_IDLEACQINT	0
#define MXT_POWER_ACTVACQINT	1
#define MXT_POWER_ACTV2IDLETO	2

/* MXT_GEN_ACQUIRE_T8 field */
#define MXT_ACQUIRE_CHRGTIME	0
#define MXT_ACQUIRE_TCHDRIFT	2
#define MXT_ACQUIRE_DRIFTST	3
#define MXT_ACQUIRE_TCHAUTOCAL	4
#define MXT_ACQUIRE_SYNC	5
#define MXT_ACQUIRE_ATCHCALST	6
#define MXT_ACQUIRE_ATCHCALSTHR	7

/* MXT_TOUCH_MULTI_T9 field */
#define MXT_TOUCH_CTRL		0
#define MXT_TOUCH_XORIGIN	1
#define MXT_TOUCH_YORIGIN	2
#define MXT_TOUCH_XSIZE		3
#define MXT_TOUCH_YSIZE		4
#define MXT_TOUCH_BLEN		6
#define MXT_TOUCH_TCHTHR	7
#define MXT_TOUCH_TCHDI		8
#define MXT_TOUCH_ORIENT	9
#define MXT_TOUCH_MOVHYSTI	11
#define MXT_TOUCH_MOVHYSTN	12
#define MXT_TOUCH_NUMTOUCH	14
#define MXT_TOUCH_MRGHYST	15
#define MXT_TOUCH_MRGTHR	16
#define MXT_TOUCH_AMPHYST	17
#define MXT_TOUCH_XRANGE_LSB	18
#define MXT_TOUCH_XRANGE_MSB	19
#define MXT_TOUCH_YRANGE_LSB	20
#define MXT_TOUCH_YRANGE_MSB	21
#define MXT_TOUCH_XLOCLIP	22
#define MXT_TOUCH_XHICLIP	23
#define MXT_TOUCH_YLOCLIP	24
#define MXT_TOUCH_YHICLIP	25
#define MXT_TOUCH_XEDGECTRL	26
#define MXT_TOUCH_XEDGEDIST	27
#define MXT_TOUCH_YEDGECTRL	28
#define MXT_TOUCH_YEDGEDIST	29
#define MXT_TOUCH_JUMPLIMIT	30

/* T100 Multiple Touch Touchscreen */
#define MXT_T100_CTRL      0
#define MXT_T100_CFG1      1
#define MXT_T100_SCRAUX    2
#define MXT_T100_TCHAUX    3
#define MXT_T100_XRANGE    13
#define MXT_T100_YRANGE    24
#define MXT_T100_XSIZE     9
#define MXT_T100_YSIZE     20

#define MXT_T100_CFG_SWITCHXY	(1 << 5)

#define MXT_T100_SRCAUX_NUMRPTTCH	(1 << 0)
#define MXT_T100_SRCAUX_TCHAREA		(1 << 1)
#define MXT_T100_SRCAUX_ATCHAREA	(1 << 2)
#define MXT_T100_SRCAUX_INTTHRAREA	(1 << 3)

#define MXT_T100_TCHAUX_VECT	(1 << 0)
#define MXT_T100_TCHAUX_AMPL	(1 << 1)
#define MXT_T100_TCHAUX_AREA	(1 << 2)
#define MXT_T100_TCHAUX_PEAK	(1 << 4)


/* MXT_TOUCH_CTRL bits */
#define MXT_TOUCH_CTRL_ENABLE	(1 << 0)
#define MXT_TOUCH_CTRL_RPTEN	(1 << 1)
#define MXT_TOUCH_CTRL_DISAMP	(1 << 2)
#define MXT_TOUCH_CTRL_DISVECT	(1 << 3)
#define MXT_TOUCH_CTRL_DISMOVE	(1 << 4)
#define MXT_TOUCH_CTRL_DISREL	(1 << 5)
#define MXT_TOUCH_CTRL_DISPRESS	(1 << 6)
#define MXT_TOUCH_CTRL_SCANEN	(1 << 7)
#define MXT_TOUCH_CTRL_OPERATIONAL	(MXT_TOUCH_CTRL_ENABLE | \
					 MXT_TOUCH_CTRL_SCANEN | \
					 MXT_TOUCH_CTRL_RPTEN)
#define MXT_TOUCH_CTRL_SCANNING		(MXT_TOUCH_CTRL_ENABLE | \
					 MXT_TOUCH_CTRL_SCANEN)
#define MXT_TOUCH_CTRL_OFF		0x0

/* MXT_PROCI_GRIPFACE_T20 field */
#define MXT_GRIPFACE_CTRL	0
#define MXT_GRIPFACE_XLOGRIP	1
#define MXT_GRIPFACE_XHIGRIP	2
#define MXT_GRIPFACE_YLOGRIP	3
#define MXT_GRIPFACE_YHIGRIP	4
#define MXT_GRIPFACE_MAXTCHS	5
#define MXT_GRIPFACE_SZTHR1	7
#define MXT_GRIPFACE_SZTHR2	8
#define MXT_GRIPFACE_SHPTHR1	9
#define MXT_GRIPFACE_SHPTHR2	10
#define MXT_GRIPFACE_SUPEXTTO	11

/* MXT_PROCI_NOISE field */
#define MXT_NOISE_CTRL		0
#define MXT_NOISE_OUTFLEN	1
#define MXT_NOISE_GCAFUL_LSB	3
#define MXT_NOISE_GCAFUL_MSB	4
#define MXT_NOISE_GCAFLL_LSB	5
#define MXT_NOISE_GCAFLL_MSB	6
#define MXT_NOISE_ACTVGCAFVALID	7
#define MXT_NOISE_NOISETHR	8
#define MXT_NOISE_FREQHOPSCALE	10
#define MXT_NOISE_FREQ0		11
#define MXT_NOISE_FREQ1		12
#define MXT_NOISE_FREQ2		13
#define MXT_NOISE_FREQ3		14
#define MXT_NOISE_FREQ4		15
#define MXT_NOISE_IDLEGCAFVALID	16

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1

/* MXT_SPT_CTECONFIG_T28 field */
#define MXT_CTE_CTRL		0
#define MXT_CTE_CMD		1
#define MXT_CTE_MODE		2
#define MXT_CTE_IDLEGCAFDEPTH	3
#define MXT_CTE_ACTVGCAFDEPTH	4
#define MXT_CTE_VOLTAGE		5

#define MXT_VOLTAGE_DEFAULT	2700000
#define MXT_VOLTAGE_STEP	10000

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_BOOT_VALUE		0xa5
#define MXT_BACKUP_VALUE	0x55
#define MXT_BACKUP_TIME		50	/* msec */
#define MXT_RESET_TIME		200	/* msec */
#define MXT_CAL_TIME		25	/* msec */

#define MXT_FWRESET_TIME	500	/* msec */

#define MXT_POWERON_DELAY	250	/* msec */

/* Default value for acquisition interval when in suspend mode*/
#define MXT_SUSPEND_ACQINT_VALUE 32      /* msec */

/* MXT_SPT_GPIOPWM_T19 field */
#define MXT_GPIO0_MASK		0x04
#define MXT_GPIO1_MASK		0x08
#define MXT_GPIO2_MASK		0x10
#define MXT_GPIO3_MASK		0x20

/* Command to unlock bootloader */
#define MXT_UNLOCK_CMD_MSB	0xaa
#define MXT_UNLOCK_CMD_LSB	0xdc

/* Bootloader mode status */
#define MXT_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT_FRAME_CRC_CHECK	0x02
#define MXT_FRAME_CRC_FAIL	0x03
#define MXT_FRAME_CRC_PASS	0x04
#define MXT_APP_CRC_FAIL	0x40	/* valid 7 8 bit only */
#define MXT_BOOT_STATUS_MASK	0x3f

/* Touch status */
#define MXT_UNGRIP		(1 << 0)
#define MXT_SUPPRESS		(1 << 1)
#define MXT_AMP			(1 << 2)
#define MXT_VECTOR		(1 << 3)
#define MXT_MOVE		(1 << 4)
#define MXT_RELEASE		(1 << 5)
#define MXT_PRESS		(1 << 6)
#define MXT_DETECT		(1 << 7)

/* Touch orient bits */
#define MXT_XY_SWITCH		(1 << 0)
#define MXT_X_INVERT		(1 << 1)
#define MXT_Y_INVERT		(1 << 2)

/* Touchscreen absolute values */
#define MXT_MAX_AREA		0xff

/* Fallback T7 values to restore functionality in the event of i2c problems */
#define FALLBACK_MXT_POWER_IDLEACQINT	0xff
#define FALLBACK_MXT_POWER_ACTVACQINT	0xff
#define FALLBACK_MXT_POWER_ACTV2IDLETO	0x20

/* For CMT (must match XRANGE/YRANGE as defined in board config */
#define MXT_PIXELS_PER_MM	20

/* Define for TOUCH_MOUTITOUCHSCREEN_T100 Touch Status */
#define TOUCH_STATUS_DETECT		0x80
#define TOUCH_STATUS_EVENT_NO_EVENT	0x00
#define TOUCH_STATUS_EVENT_MOVE		0x01
#define TOUCH_STATUS_EVENT_UNSUP	0x02
#define TOUCH_STATUS_EVENT_SUP		0x03
#define TOUCH_STATUS_EVENT_DOWN		0x04
#define TOUCH_STATUS_EVENT_UP		0x05
#define TOUCH_STATUS_EVENT_UNSUPSUP	0x06
#define TOUCH_STATUS_EVENT_UNSUPUP	0x07
#define TOUCH_STATUS_EVENT_DOWNSUP	0x08
#define TOUCH_STATUS_EVENT_DOWNUP	0x09

/* MXT touch types */
#define TOUCH_STATUS_TYPE_RESERVED	0
#define TOUCH_STATUS_TYPE_FINGER	1
#define TOUCH_STATUS_TYPE_STYLUS	2
#define TOUCH_STATUS_TYPE_HOVERING	4
#define TOUCH_STATUS_TYPE_GLOVE		5
#define TOUCH_STATUS_TYPE_LARGE_TOUCH	6

#define DISTANCE_ACTIVE_TOUCH		0
#define DISTANCE_HOVERING		1

struct mxt_cfg_file_hdr {
	bool valid;
	u32 info_crc;
	u32 cfg_crc;
};

struct mxt_cfg_file_line {
	struct list_head list;
	u16 addr;
	u8 size;
	u8 *content;
};

struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u8 size;		/* Size of each instance - 1 */
	u8 instances;		/* Number of instances - 1 */
	u8 num_report_ids;
} __packed;

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct regulator *vdd;
	struct regulator *avdd;
	struct input_dev *input_dev;
	char phys[64];		/* device physical location */
	const struct mxt_platform_data *pdata;
	struct mxt_object *object_table;
	struct mxt_info info;
	bool is_tp;

	struct gpio_desc *reset_gpio;

	unsigned int irq;

	unsigned int max_x;
	unsigned int max_y;

	/* max touchscreen area in terms of pixels and channels */
	unsigned int max_area_pixels;
	unsigned int max_area_channels;

	unsigned int num_touchids;

	u32 info_csum;
	u32 config_csum;

	bool has_T9;
	bool has_T100;

	/* Cached parameters from object table */
	u16 T5_address;
	u8 T6_reportid;
	u8 T9_reportid_min;
	u8 T9_reportid_max;
	u8 T19_reportid;
	u16 T44_address;
	u8 T100_reportid_min;
	u8 T100_reportid_max;
	u8 message_length;

	/* T100 Configuration.  Which calculations are enabled*/
	bool T100_enabled_num_reportable_touches;
	bool T100_enabled_touch_area;
	bool T100_enabled_antitouch_area;
	bool T100_enabled_internal_tracking_area;
	bool T100_enabled_vector;
	bool T100_enabled_amplitude;
	bool T100_enabled_area;
	bool T100_enabled_peak;

	/* for fw update in bootloader */
	struct completion bl_completion;

	/* per-instance debugfs root */
	struct dentry *dentry_dev;
	struct dentry *dentry_deltas;
	struct dentry *dentry_refs;
	struct dentry *dentry_object;
	/* for self-capacitance information */
	struct dentry *dentry_self_deltas;
	struct dentry *dentry_self_refs;

	/* Protect access to the T37 object buffer, used by debugfs */
	struct mutex T37_buf_mutex;
	u8 *T37_buf;
	size_t T37_buf_size;

	/* Saved T7 configuration
	 * [0] = IDLEACQINT
	 * [1] = ACTVACQINT
	 * [2] = ACTV2IDLETO
	 */
	u8 T7_config[3];
	bool T7_config_valid;

	/* T7 IDLEACQINT & ACTVACQINT setting when in suspend mode*/
	u8 suspend_acq_interval;

	u8 T101_ctrl;
	bool T101_ctrl_valid;

	bool irq_wake;  /* irq wake is enabled */
	/* Saved T42 Touch Suppression field */
	u8 T42_ctrl;
	bool T42_ctrl_valid;

	/* Saved T19 GPIO config */
	u8 T19_ctrl;
	bool T19_ctrl_valid;

	u8 T19_status;

	/* Protect access to the object register buffer */
	struct mutex object_str_mutex;
	char *object_str;
	size_t object_str_size;

	/* for auto-calibration in suspend */
	struct completion auto_cal_completion;

	/*
	 * Protects from concurrent firmware/config updates and
	 * suspending/resuming.
	 */
	struct mutex fw_mutex;

	/* firmware file name */
	char *fw_file;

	/* config file name */
	char *config_file;

	/* map for the tracking id currently being used */
	bool *current_id;

	/* Cleanup flags */
	bool sysfs_group_created;
	bool power_sysfs_group_merged;
	bool debugfs_initialized;
};

/* global root node of the atmel_mxt_ts debugfs directory. */
static struct dentry *mxt_debugfs_root;

static int mxt_calc_resolution_T9(struct mxt_data *data);
static int mxt_calc_resolution_T100(struct mxt_data *data);
static void mxt_free_object_table(struct mxt_data *data);
static void mxt_save_power_config(struct mxt_data *data);
static void mxt_save_aux_regs(struct mxt_data *data);
static void mxt_start(struct mxt_data *data);
static void mxt_stop(struct mxt_data *data);
static int mxt_initialize(struct mxt_data *data);
static int mxt_get_config_from_chip(struct mxt_data *data);
static int mxt_input_dev_create(struct mxt_data *data);
static int get_touch_major_pixels(struct mxt_data *data, int touch_channels);

static const char *mxt_event_to_string(int event)
{
	static const char *const str[] = {
		"NO_EVENT",
		"MOVE    ",
		"UNSUP   ",
		"SUP     ",
		"DOWN    ",
		"UP      ",
		"UNSUPSUP",
		"UNSUPUP ",
		"DOWNSUP ",
		"DOWNUP  ",
	};
	return event < ARRAY_SIZE(str) ? str[event] : "UNKNOWN ";
}

static inline bool is_mxt_33x_t(struct mxt_data *data)
{
	struct mxt_info *info = &data->info;
	/* vairant_id: 336t(5), 337t(17) */
	return ((info->family_id == 164) &&
		((info->variant_id == 5) || (info->variant_id == 17)));
}

static inline bool is_hovering_supported(struct mxt_data *data)
{
	struct mxt_info *info = &data->info;
	/* vairant_id: 337t(164.17), 2954t2(164.13) */
	return ((info->family_id == 164) &&
		((info->variant_id == 13) || (info->variant_id == 17)));
}

static inline size_t mxt_obj_size(const struct mxt_object *obj)
{
	return obj->size + 1;
}

static inline size_t mxt_obj_instances(const struct mxt_object *obj)
{
	return obj->instances + 1;
}

static bool mxt_object_readable(unsigned int type)
{
	switch (type) {
	case MXT_GEN_COMMAND_T6:
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_GEN_DATASOURCE_T53:
	case MXT_PROCI_GRIPFACE_T20:
	case MXT_PROCG_NOISE_T22:
	case MXT_PROCI_ONETOUCH_T24:
	case MXT_PROCI_TWOTOUCH_T27:
	case MXT_PROCI_GRIP_T40:
	case MXT_PROCI_PALM_T41:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCG_NOISESUPPRESSION_T48:
	case MXT_PROCI_ADAPTIVETHRESHOLD_T55:
	case MXT_PROCI_SHIELDLESS_T56:
	case MXT_PROCI_EXTRATOUCHSCREENDATA_T57:
	case MXT_PROCG_NOISESUPPRESSION_T62:
	case MXT_PROCI_LENSBENDING_T65:
	case MXT_PROCG_NOISESUPPRESSION_T72:
	case MXT_PROCI_GLOVEDETECTION_T78:
	case MXT_PROCI_RETRANSMISSIONCOMPANSATION_T80:
	case MXT_PROCG_NOISESUPACTIVESTYLUS_T86:
	case MXT_PROCI_SYMBOLGESTUREPROCESSOR_T92:
	case MXT_PROCI_TOUCHSEQUENCELOGGER_T93:
	case MXT_PROCG_PTCNOISESUPPRESSION_T98:
	case MXT_PROCI_ACTIVESTYLUS_T107:
	case MXT_PROCG_NOISESUPSELFCAP_T108:
	case MXT_PROCI_SELFCAPGRIPSUPPRESSION_T112:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_SPT_SELFTEST_T25:
	case MXT_SPT_CTECONFIG_T28:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_DIGITIZER_T43:
	case MXT_SPT_CTECONFIG_T46:
	case MXT_SPT_TIMER_T61:
	case MXT_SPT_GOLDENREFERENCES_T66:
	case MXT_SPT_DYNAMICCONFIGURATIONCONTROLLER_T70:
	case MXT_SPT_DYNAMICCONFIGURATIONCONTAINER_T71:
	case MXT_SPT_CTESCANCONFIG_T77:
	case MXT_SPT_TOUCHEVENTTRIGGER_T79:
	case MXT_SPT_PTCCONFIG_T95:
	case MXT_SPT_PTCTUNINGPARAMS_T96:
	case MXT_SPT_TOUCHSCREENHOVER_T101:
	case MXT_SPT_AUXTOUCHCONFIG_T104:
	case MXT_SPT_SELFCAPGLOBALCONFIG_T109:
	case MXT_SPT_SELFCAPTUNINGPARAMS_T110:
	case MXT_SPT_SELFCAPCONFIG_T111:
	case MXT_SPT_SELFCAPMEASURECONFIG_T113:
	case MXT_SPT_ACTIVESTYLUSMEASCONFIG_T114:
	case MXT_TOUCH_MULTI_T9:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_TOUCH_PROXIMITY_T23:
	case MXT_TOUCH_PROXKEY_T52:
	case MXT_TOUCH_PTCKEYS_T97:
	case MXT_TOUCH_MULTITOUCHSCREEN_T100:
		return true;
	default:
		return false;
	}
}

static bool mxt_object_writable(unsigned int type)
{
	switch (type) {
	case MXT_GEN_COMMAND_T6:
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_PROCI_GRIPFACE_T20:
	case MXT_PROCG_NOISE_T22:
	case MXT_PROCI_ONETOUCH_T24:
	case MXT_PROCI_TWOTOUCH_T27:
	case MXT_PROCI_GRIP_T40:
	case MXT_PROCI_PALM_T41:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCG_NOISESUPPRESSION_T48:
	case MXT_PROCI_ADAPTIVETHRESHOLD_T55:
	case MXT_PROCI_SHIELDLESS_T56:
	case MXT_PROCI_EXTRATOUCHSCREENDATA_T57:
	case MXT_PROCG_NOISESUPPRESSION_T62:
	case MXT_PROCI_LENSBENDING_T65:
	case MXT_PROCG_NOISESUPPRESSION_T72:
	case MXT_PROCI_GLOVEDETECTION_T78:
	case MXT_PROCI_RETRANSMISSIONCOMPANSATION_T80:
	case MXT_PROCG_NOISESUPACTIVESTYLUS_T86:
	case MXT_PROCI_SYMBOLGESTUREPROCESSOR_T92:
	case MXT_PROCI_TOUCHSEQUENCELOGGER_T93:
	case MXT_PROCG_PTCNOISESUPPRESSION_T98:
	case MXT_PROCI_ACTIVESTYLUS_T107:
	case MXT_PROCG_NOISESUPSELFCAP_T108:
	case MXT_PROCI_SELFCAPGRIPSUPPRESSION_T112:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_SPT_SELFTEST_T25:
	case MXT_SPT_CTECONFIG_T28:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_DIGITIZER_T43:
	case MXT_SPT_CTECONFIG_T46:
	case MXT_SPT_TIMER_T61:
	case MXT_SPT_GOLDENREFERENCES_T66:
	case MXT_SPT_SERIALDATACOMMAND_T68:
	case MXT_SPT_DYNAMICCONFIGURATIONCONTROLLER_T70:
	case MXT_SPT_DYNAMICCONFIGURATIONCONTAINER_T71:
	case MXT_SPT_CTESCANCONFIG_T77:
	case MXT_SPT_TOUCHEVENTTRIGGER_T79:
	case MXT_SPT_PTCCONFIG_T95:
	case MXT_SPT_PTCTUNINGPARAMS_T96:
	case MXT_SPT_TOUCHSCREENHOVER_T101:
	case MXT_SPT_AUXTOUCHCONFIG_T104:
	case MXT_SPT_SELFCAPGLOBALCONFIG_T109:
	case MXT_SPT_SELFCAPTUNINGPARAMS_T110:
	case MXT_SPT_SELFCAPCONFIG_T111:
	case MXT_SPT_SELFCAPMEASURECONFIG_T113:
	case MXT_SPT_ACTIVESTYLUSMEASCONFIG_T114:
	case MXT_TOUCH_MULTI_T9:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_TOUCH_PROXIMITY_T23:
	case MXT_TOUCH_PROXKEY_T52:
	case MXT_TOUCH_PTCKEYS_T97:
	case MXT_TOUCH_MULTITOUCHSCREEN_T100:
		return true;
	default:
		return false;
	}
}

static void mxt_dump_message(struct device *dev, u8 *message)
{
	dev_dbg(dev, "reportid: %u\tmessage: %*ph\n",
		message[0], 7, &message[1]);
}

/*
 * Release all the fingers that are being tracked. To avoid unwanted gestures,
 * move all the fingers to (0,0) with largest PRESSURE and TOUCH_MAJOR.
 * Userspace apps can use these info to filter out these events and/or cancel
 * existing gestures.
 */
static void __maybe_unused mxt_release_all_fingers(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	int id;
	int max_area_channels = min(255U, data->max_area_channels);
	int max_touch_major = get_touch_major_pixels(data, max_area_channels);
	bool need_update = false;
	for (id = 0; id < data->num_touchids; id++) {
		if (data->current_id[id]) {
			dev_warn(dev, "Move touch %d to (0,0)\n", id);
			input_mt_slot(input_dev, id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
						   true);
			input_report_abs(input_dev, ABS_MT_POSITION_X, 0);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, 0);
			input_report_abs(input_dev, ABS_MT_PRESSURE, 255);
			input_report_abs(input_dev, ABS_MT_DISTANCE,
					 DISTANCE_ACTIVE_TOUCH);
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					 max_touch_major);
			need_update = true;
		}
	}
	if (need_update)
		input_sync(data->input_dev);

	for (id = 0; id < data->num_touchids; id++) {
		if (data->current_id[id]) {
			dev_warn(dev, "Release touch contact %d\n", id);
			input_mt_slot(input_dev, id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
						   false);
			data->current_id[id] = false;
		}
	}
	if (need_update)
		input_sync(data->input_dev);
}

static bool mxt_in_bootloader(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	return (client->addr == 0x25 ||
		client->addr == 0x26 ||
		client->addr == 0x27);
}

static bool mxt_in_appmode(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	return (client->addr == 0x4a || client->addr == 0x4b);
}

static int mxt_i2c_recv(struct i2c_client *client, u8 *buf, size_t count)
{
	int ret;

	ret = i2c_master_recv(client, buf, count);
	if (ret == count) {
		ret = 0;
	} else if (ret != count) {
		ret = (ret < 0) ? ret : -EIO;
		dev_err(&client->dev, "i2c recv failed (%d)\n", ret);
	}

	return ret;
}

static int mxt_i2c_send(struct i2c_client *client, const u8 *buf, size_t count)
{
	int ret;

	ret = i2c_master_send(client, buf, count);
	if (ret == count) {
		ret = 0;
	} else if (ret != count) {
		ret = (ret < 0) ? ret : -EIO;
		dev_err(&client->dev, "i2c send failed (%d)\n", ret);
	}

	return ret;
}

static int mxt_i2c_transfer(struct i2c_client *client, struct i2c_msg *msgs,
		size_t count)
{
	int ret;

	ret = i2c_transfer(client->adapter, msgs, count);
	if (ret == count) {
		ret = 0;
	} else {
		ret = (ret < 0) ? ret : -EIO;
		dev_err(&client->dev, "i2c transfer failed (%d)\n", ret);
	}

	return ret;
}

static int mxt_wait_for_chg(struct mxt_data *data, unsigned int timeout_ms)
{
	struct device *dev = &data->client->dev;
	struct completion *comp = &data->bl_completion;
	unsigned long timeout = msecs_to_jiffies(timeout_ms);
	long ret;

	ret = wait_for_completion_interruptible_timeout(comp, timeout);
	if (ret < 0) {
		dev_err(dev, "Wait for completion interrupted.\n");
		/*
		 * TODO: handle -EINTR better by terminating fw update process
		 * before returning to userspace by writing length 0x000 to
		 * device (iff we are in WAITING_FRAME_DATA state).
		 */
		return -EINTR;
	} else if (ret == 0) {
		dev_err(dev, "Wait for completion timed out.\n");
		return -ETIMEDOUT;
	}
	return 0;
}


static int mxt_lookup_bootloader_address(struct mxt_data *data)
{
	u8 addr = data->client->addr;
	u8 family_id = data->info.family_id;
	u8 bootloader = 0;

	if (mxt_in_bootloader(data))
		return addr;

	if (addr == 0x4a) {
		bootloader = 0x26;
	} else if (addr == 0x4b) {
		bootloader = family_id >= 0xa2 ? 0x27 : 0x25;
	} else {
		dev_err(&data->client->dev,
			"Appmode i2c address 0x%02x not found\n", addr);
	}
	return bootloader;
}

static int mxt_lookup_appmode_address(struct mxt_data *data)
{
	u8 addr = data->client->addr;
	u8 appmode = 0;

	if (mxt_in_appmode(data))
		return addr;

	if (addr == 0x26)
		appmode = 0x4a;
	else if (addr == 0x25 || addr == 0x27) {
		appmode = 0x4b;
	} else {
		dev_err(&data->client->dev,
			"Bootloader mode i2c address 0x%02x not found\n", addr);
	}
	return appmode;
}


static int mxt_check_bootloader(struct mxt_data *data, unsigned int state)
{
	struct i2c_client *client = data->client;
	u8 val;
	int ret;

recheck:
	if (state != MXT_WAITING_BOOTLOAD_CMD) {
		/*
		 * In application update mode, the interrupt
		 * line signals state transitions. We must wait for the
		 * CHG assertion before reading the status byte.
		 * Once the status byte has been read, the line is deasserted.
		 */
		int ret = mxt_wait_for_chg(data, 300);
		if (ret) {
			dev_err(&client->dev,
				"Update wait error %d, state %d\n", ret, state);
			return ret;
		}
	}

	ret = mxt_i2c_recv(client, &val, 1);
	if (ret)
		return ret;

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
		dev_info(&client->dev, "bootloader version: %d\n",
			 val & MXT_BOOT_STATUS_MASK);
	case MXT_WAITING_FRAME_DATA:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK)
			goto recheck;
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(&client->dev, "Unvalid bootloader mode state\n");
		dev_err(&client->dev, "Invalid bootloader mode state %d, %d\n",
			val, state);
		return -EINVAL;
	}

	return 0;
}

static int mxt_unlock_bootloader(struct i2c_client *client)
{
	u8 buf[2];

	buf[0] = MXT_UNLOCK_CMD_LSB;
	buf[1] = MXT_UNLOCK_CMD_MSB;

	return mxt_i2c_send(client, buf, 2);
}

static int mxt_fw_write(struct i2c_client *client,
			     const u8 *data, unsigned int frame_size)
{
	return mxt_i2c_send(client, data, frame_size);
}

#ifdef DEBUG
#define DUMP_LEN	16
static void mxt_dump_xfer(struct device *dev, const char *func, u16 reg,
			  u16 len, const u8 *val)
{
	/* Rough guess for string size */
	char str[DUMP_LEN * 3 + 2];
	int i;
	size_t n;

	for (i = 0, n = 0; i < len; i++) {
		n += snprintf(&str[n], sizeof(str) - n, "%02x ", val[i]);
		if ((i + 1) % DUMP_LEN == 0 || (i + 1) == len) {
			dev_dbg(dev,
				"%s(reg: %d len: %d offset: 0x%02x): %s\n",
				func, reg, len, (i / DUMP_LEN) * DUMP_LEN,
				str);
			n = 0;
		}
	}
}
#undef DUMP_LEN
#else
static void mxt_dump_xfer(struct device *dev, const char *func, u16 reg,
			  u16 len, const u8 *val) { }
#endif

static int __mxt_read_reg(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];
	int ret;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	ret = mxt_i2c_transfer(client, xfer, 2);
	if (ret == 0)
		mxt_dump_xfer(&client->dev, __func__, reg, len, val);

	return ret;
}

static int __mxt_write_reg(struct i2c_client *client, u16 reg, u16 len,
			   const void *val)
{
	u8 *buf;
	size_t count;
	int ret;

	count = len + 2;
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[2], val, len);

	mxt_dump_xfer(&client->dev, __func__, reg, len, val);
	ret = mxt_i2c_send(client, buf, count);
	kfree(buf);
	return ret;
}

static int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	return __mxt_write_reg(client, reg, 1, &val);
}

static struct mxt_object *
mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_err(&data->client->dev, "Invalid object type %d\n", type);
	return NULL;
}

static int mxt_read_num_messages(struct mxt_data *data, u8 *count)
{
	/* TODO: Optimization: read first message along with message count */
	return __mxt_read_reg(data->client, data->T44_address, 1, count);
}

static int mxt_read_messages(struct mxt_data *data, u8 count, u8 *buf)
{
	return __mxt_read_reg(data->client, data->T5_address,
			data->message_length * count, buf);
}

static int mxt_write_obj_instance(struct mxt_data *data, u8 type, u8 instance,
		u8 offset, u8 val)
{
	struct mxt_object *object;
	u16 reg;

	object = mxt_get_object(data, type);
	if (!object || offset >= mxt_obj_size(object) ||
	    instance >= mxt_obj_instances(object))
		return -EINVAL;

	reg = object->start_address + instance * mxt_obj_size(object) + offset;
	return mxt_write_reg(data->client, reg, val);
}

static int mxt_ensure_obj_instance(struct mxt_data *data, u8 type, u8 instance,
		u8 offset, u8 val)
{
	struct mxt_object *object;
	u16 reg;
	u8 current_val;
	int ret;

	object = mxt_get_object(data, type);
	if (!object || offset >= mxt_obj_size(object) ||
	    instance >= mxt_obj_instances(object))
		return -EINVAL;

	reg = object->start_address + instance * mxt_obj_size(object) + offset;

	ret = __mxt_read_reg(data->client, reg, 1, &current_val);
	if (ret)
		return -EINVAL;

	if (val != current_val) {
		ret = __mxt_write_reg(data->client, reg, 1, &val);
		if (ret)
			return -EINVAL;
	}

	return 0;
}

static int mxt_write_object(struct mxt_data *data, u8 type, u8 offset, u8 val)
{
	return mxt_write_obj_instance(data, type, 0, offset, val);
}

static int mxt_ensure_object(struct mxt_data *data, u8 type, u8 offset, u8 val)
{
	return mxt_ensure_obj_instance(data, type, 0, offset, val);
}

static int mxt_recalibrate(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

	dev_dbg(dev, "Recalibration ...\n");
	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				 MXT_COMMAND_CALIBRATE, 1);
	if (error)
		dev_err(dev, "Recalibration failed %d\n", error);
	else
		msleep(MXT_CAL_TIME);
	return error;
}

static void mxt_input_button(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input = data->input_dev;
	u8 *payload = &message[1];
	bool button;
	int i;

	dev_dbg(dev, "GPIO Event :%X\n", payload[0]);
	if (!data->pdata) {
		/* Active-low switch */
		if (data->has_T100) {
			/* mXT336T GPIO bitmask is different */
			if (is_mxt_33x_t(data))
				button = !(payload[0] & (MXT_GPIO3_MASK >> 2));
			else
				button = !(payload[0] & MXT_GPIO2_MASK);
		} else {
			button = !(payload[0] & MXT_GPIO3_MASK);
		}
		input_report_key(input, BTN_LEFT, button);
		dev_dbg(dev, "Button state: %d\n", button);
		return;
	}

	/* Active-low switch */
	for (i = 0; i < MXT_NUM_GPIO; i++) {
		if (data->pdata->key_map[i] == KEY_RESERVED)
			continue;
		button = !(payload[0] & MXT_GPIO0_MASK << i);
		input_report_key(input, data->pdata->key_map[i], button);
		dev_dbg(dev, "Button state: %d\n", button);
	}
}

/*
 * Assume a circle touch contact and use the diameter as the touch major.
 * touch_pixels = touch_channels * (max_area_pixels / max_area_channels)
 * touch_pixels = pi * (touch_major / 2) ^ 2;
 */
static int get_touch_major_pixels(struct mxt_data *data, int touch_channels)
{
	int touch_pixels;

	if (data->max_area_channels == 0)
		return 0;

	touch_pixels = DIV_ROUND_CLOSEST(touch_channels * data->max_area_pixels,
					 data->max_area_channels);
	return int_sqrt(DIV_ROUND_CLOSEST(touch_pixels * 100, 314)) * 2;
}

static void mxt_handle_screen_status_report(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	u8 *payload = &message[1];
	u8 status = payload[0];
	u8 num_reportable_touches = 0;
	int touch_area = 0;
	int antitouch_area = 0;
	int internal_tracking_area = 0;
	int next_index = 1;

	/* Process the values according to the internal sequence */
	if (data->T100_enabled_num_reportable_touches) {
		num_reportable_touches = payload[next_index];
		next_index += 1;
	}

	if (data->T100_enabled_touch_area) {
		touch_area = payload[next_index + 1] << 8 |
				payload[next_index];
		next_index += 2;
	}

	if (data->T100_enabled_antitouch_area) {
		antitouch_area = payload[next_index + 1] << 8 |
					payload[next_index];
		next_index += 2;
	}

	if (data->T100_enabled_internal_tracking_area) {
		internal_tracking_area = payload[next_index + 1] << 8 |
						payload[next_index];
		next_index += 2;
	}

	dev_dbg(dev,
		"Screen Status Report : status = %X, N=%X, T=%d, A=%d, I=%d\n",
		status, num_reportable_touches, touch_area, antitouch_area,
		internal_tracking_area);
}


static void mxt_input_touchevent(struct mxt_data *data, u8 *message, int id)
{
	struct device *dev = &data->client->dev;
	u8 *payload = &message[1];
	u8 status = payload[0];
	struct input_dev *input_dev = data->input_dev;
	int x;
	int y;
	int area;
	int pressure;
	int touch_major;
	int vector1, vector2;

	x = (payload[1] << 4) | ((payload[3] >> 4) & 0xf);
	y = (payload[2] << 4) | ((payload[3] & 0xf));
	if (data->max_x < 1024)
		x = x >> 2;
	if (data->max_y < 1024)
		y = y >> 2;

	area = payload[4];
	touch_major = get_touch_major_pixels(data, area);
	pressure = payload[5];

	/* The two vector components are 4-bit signed ints (2s complement) */
	vector1 = (signed)((signed char)payload[6]) >> 4;
	vector2 = (signed)((signed char)(payload[6] << 4)) >> 4;

	dev_dbg(dev,
		"[%u] %c%c%c%c%c%c%c%c x: %5u y: %5u area: %3u amp: %3u vector: [%d,%d]\n",
		id,
		(status & MXT_DETECT) ? 'D' : '.',
		(status & MXT_PRESS) ? 'P' : '.',
		(status & MXT_RELEASE) ? 'R' : '.',
		(status & MXT_MOVE) ? 'M' : '.',
		(status & MXT_VECTOR) ? 'V' : '.',
		(status & MXT_AMP) ? 'A' : '.',
		(status & MXT_SUPPRESS) ? 'S' : '.',
		(status & MXT_UNGRIP) ? 'U' : '.',
		x, y, area, pressure, vector1, vector2);

	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
				   status & MXT_DETECT);
	data->current_id[id] = status & MXT_DETECT;

	if (status & MXT_DETECT) {
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, pressure);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
		/* TODO: Use vector to report ORIENTATION & TOUCH_MINOR */
	}
}

static void mxt_input_touchevent_T100(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 reportid = message[0];
	u8 *payload = &message[1];
	u8 status = payload[0];
	u8 event = status & 0x0F;
	int id;
	int x, y;
	int area = 0;
	int pressure = 0;
	int touch_major = 0;
	int touch_peak = 0;
	int next_index = 1;
	int vector1 = 0, vector2 = 0;
	int touch_type;
	int distance;

	id = reportid - data->T100_reportid_min - 2;

	x = (payload[next_index+1] << 8) | payload[next_index];
	next_index += 2;
	y = (payload[next_index+1] << 8) | payload[next_index];
	next_index += 2;

	/* Keep the process sequence */
	if (data->T100_enabled_vector) {
		/* The two vector components are 4-bit signed ints */
		u8 values = payload[next_index];
		vector1 = (signed)((signed char)values) >> 4;
		vector2 = (signed)((signed char)(values << 4)) >> 4;
		next_index += 1;
	}

	if (data->T100_enabled_amplitude) {
		pressure = payload[next_index];
		next_index += 1;
	}

	if (data->T100_enabled_area) {
		area = payload[next_index];
		touch_major = get_touch_major_pixels(data, area);
		next_index += 1;
	}

	if (data->T100_enabled_peak) {
		touch_peak = payload[next_index];
		next_index += 1;
	}

	/*
	 * Currently there is no distance information for hovering,
	 * however, this can be used as hovering indication in user space.
	 */
	touch_type = ((payload[0] & 0x70) >> 4);
	if (touch_type == TOUCH_STATUS_TYPE_HOVERING) {
		distance = DISTANCE_HOVERING;
		pressure = 0;
	} else {
		distance = DISTANCE_ACTIVE_TOUCH;
		if (pressure == 0 && is_hovering_supported(data))
			pressure = 1;
	}

	dev_dbg(dev,
		"[%u] T%d%c %s x: %5u y: %5u a: %5u p: %5u m: %d v: [%d,%d]\n",
		id,
		touch_type,
		(status & TOUCH_STATUS_DETECT) ? 'D' : ' ',
		mxt_event_to_string(event),
		x, y, area, pressure, touch_major, vector1, vector2);

	data->current_id[id] = status & TOUCH_STATUS_DETECT;
	if (touch_type != TOUCH_STATUS_TYPE_STYLUS) {
		if (status & TOUCH_STATUS_DETECT) {
			if (event == TOUCH_STATUS_EVENT_MOVE ||
					event == TOUCH_STATUS_EVENT_DOWN ||
					event == TOUCH_STATUS_EVENT_UNSUP ||
					event == TOUCH_STATUS_EVENT_NO_EVENT) {
				input_mt_slot(input_dev, id);
				input_mt_report_slot_state(
					input_dev, MT_TOOL_FINGER, true);
				input_report_abs(input_dev,
						 ABS_MT_POSITION_X, x);
				input_report_abs(input_dev,
						 ABS_MT_POSITION_Y, y);
				input_report_abs(input_dev,
						 ABS_MT_PRESSURE, pressure);
				input_report_abs(input_dev,
						 ABS_MT_DISTANCE, distance);
				input_report_abs(input_dev,
						 ABS_MT_TOUCH_MAJOR,
						 touch_major);
			}
		} else {
			if (event == TOUCH_STATUS_EVENT_UP) {
				input_mt_slot(input_dev, id);
				input_mt_report_slot_state(input_dev,
							   MT_TOOL_FINGER,
							   false);
			} else if (event == TOUCH_STATUS_EVENT_SUP) {
				/*
				 * Suppressed touches will be reported as
				 * touches with maximum ABS_MT_TOUCH_MAJOR.
				 * The controller no longer reports suppressed
				 * touches, so we have to release the touch
				 * afterwards.
				 */
				int max = input_abs_get_max(input_dev,
							    ABS_MT_TOUCH_MAJOR);
				input_mt_slot(input_dev, id);
				input_report_abs(input_dev,
						 ABS_MT_TOUCH_MAJOR,
						 max);
				input_sync(input_dev);

				input_mt_report_slot_state(input_dev,
							   MT_TOOL_FINGER,
							   false);
			}
		}
	} else {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
}

static unsigned mxt_extract_T6_csum(const u8 *csum)
{
	return csum[0] | (csum[1] << 8) | (csum[2] << 16);
}

static bool mxt_is_T9_message(struct mxt_data *data, u8 reportid)
{
	return (reportid >= data->T9_reportid_min &&
			reportid <= data->T9_reportid_max);
}

static bool mxt_is_T100_message(struct mxt_data *data, u8 reportid)
{
	return (reportid >= data->T100_reportid_min &&
			reportid <= data->T100_reportid_max);
}

static int mxt_proc_messages(struct mxt_data *data, u8 count, bool report)
{
	struct device *dev = &data->client->dev;
	bool update_input = false;
	u8 *message_buffer;
	int ret;
	u8 i;

	message_buffer = kcalloc(count, data->message_length, GFP_KERNEL);
	if (!message_buffer)
		return -ENOMEM;

	ret = mxt_read_messages(data, count, message_buffer);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d).\n", count, ret);
		goto out;
	}
	if (!report)
		goto out;

	/* There could be a race condition for entering BL mode,
	 * it is a sanity check.
	 */
	if (!data->input_dev)
		goto out;

	for (i = 0; i < count; i++) {
		u8 *msg = &message_buffer[i * data->message_length];
		u8 reportid = msg[0];
		mxt_dump_message(dev, msg);

		if (reportid == data->T6_reportid) {
			const u8 *payload = &msg[1];
			u8 status = payload[0];
			data->config_csum = mxt_extract_T6_csum(&payload[1]);
			dev_info(dev, "Status: %02x Config Checksum: %06x\n",
				 status, data->config_csum);
			if (status == 0x00)
				complete(&data->auto_cal_completion);
		} else if (mxt_is_T9_message(data, reportid)) {
			int id = reportid - data->T9_reportid_min;
			mxt_input_touchevent(data, msg, id);
			update_input = true;
		} else if (reportid == data->T19_reportid) {
			mxt_input_button(data, msg);
			update_input = true;
			data->T19_status = msg[1];
		} else if (mxt_is_T100_message(data, reportid)) {
			/* check SCRSTATUS */
			if (reportid == data->T100_reportid_min) {
				/* Screen Status Report */
				mxt_handle_screen_status_report(data, msg);
			} else if (reportid == (data->T100_reportid_min + 1)) {
				/* skip reserved report id */
				continue;
			} else {
				mxt_input_touchevent_T100(data, msg);
				update_input = true;
			}
		}
	}

	if (update_input) {
		input_mt_report_pointer_emulation(data->input_dev,
						  data->is_tp);
		input_sync(data->input_dev);
	}

out:
	kfree(message_buffer);
	return ret;
}

static int mxt_handle_messages(struct mxt_data *data, bool report)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 count;

	ret = mxt_read_num_messages(data, &count);
	if (ret) {
		dev_err(dev, "Failed to read message count (%d).\n", ret);
		return ret;
	}

	if (count > 0)
		ret = mxt_proc_messages(data, count, report);

	return ret;
}

static int mxt_enter_bl(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	int ret;

	disable_irq(data->irq);

	if (data->input_dev) {
		input_unregister_device(data->input_dev);
		data->input_dev = NULL;
	}

	enable_irq(data->irq);
	/* Clean up message queue in device */
	mxt_handle_messages(data, false);

	disable_irq(data->irq);


	/* Change to the bootloader mode */
	ret = mxt_write_object(data, MXT_GEN_COMMAND_T6,
			       MXT_COMMAND_RESET, MXT_BOOT_VALUE);
	if (ret) {
		dev_err(dev, "Failed to change to bootloader mode %d.\n", ret);
		enable_irq(data->irq);
		return ret;
	}

	/* Change to slave address of bootloader */
	client->addr = mxt_lookup_bootloader_address(data);

	init_completion(&data->bl_completion);
	enable_irq(data->irq);

	/* Wait for CHG assert to indicate successful reset into bootloader */
	ret = mxt_wait_for_chg(data, MXT_RESET_TIME);
	if (ret) {
		dev_err(dev, "Failed waiting for reset to bootloader %d.\n",
			ret);
		client->addr = mxt_lookup_appmode_address(data);
		return ret;
	}
	return 0;
}

static void mxt_exit_bl(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	int error;

	init_completion(&data->bl_completion);

	/* Wait for reset */
	mxt_wait_for_chg(data, MXT_FWRESET_TIME);

	disable_irq(data->irq);
	client->addr = mxt_lookup_appmode_address(data);

	mxt_free_object_table(data);

	error = mxt_initialize(data);
	if (error) {
		dev_err(dev, "Failed to initialize on exit bl. error = %d\n",
			error);
		return;
	}

	error = mxt_input_dev_create(data);
	if (error) {
		dev_err(dev, "Create input dev failed after init. error = %d\n",
			error);
		return;
	}

	error = mxt_handle_messages(data, false);
	if (error)
		dev_err(dev, "Failed to clear CHG after init. error = %d\n",
			error);
	enable_irq(data->irq);
}

static irqreturn_t mxt_interrupt(int irq, void *dev_id)
{
	struct mxt_data *data = dev_id;
	struct device *dev = &data->client->dev;
	char *envp[] = {"ERROR=1", NULL};
	int ret;

	if (mxt_in_bootloader(data)) {
		/* bootloader state transition completion */
		complete(&data->bl_completion);
	} else {
		ret = mxt_handle_messages(data, true);
		if (ret) {
			dev_err(dev, "Handling message fails in IRQ, %d.\n",
				ret);
			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
		}
	}
	return IRQ_HANDLED;
}

static int mxt_apply_pdata_config(struct mxt_data *data)
{
	const struct mxt_platform_data *pdata = data->pdata;
	struct mxt_object *object;
	struct device *dev = &data->client->dev;
	int index = 0;
	int i, size;
	int ret;

	if (!pdata->config) {
		dev_info(dev, "No cfg data defined, skipping reg init\n");
		return 0;
	}

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_writable(object->type))
			continue;

		size = mxt_obj_size(object) * mxt_obj_instances(object);
		if (index + size > pdata->config_length) {
			dev_err(dev, "Not enough config data!\n");
			return -EINVAL;
		}

		ret = __mxt_write_reg(data->client, object->start_address,
				size, &pdata->config[index]);
		if (ret)
			return ret;
		index += size;
	}

	return 0;
}

static int mxt_handle_pdata(struct mxt_data *data)
{
	const struct mxt_platform_data *pdata = data->pdata;
	struct device *dev = &data->client->dev;
	u8 voltage;
	int ret;

	if (!pdata) {
		dev_info(dev, "No platform data provided\n");
		return 0;
	}

	if (pdata->is_tp)
		data->is_tp = true;

	ret = mxt_apply_pdata_config(data);
	if (ret)
		return ret;

	/* Set touchscreen lines */
	mxt_write_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_XSIZE,
			pdata->x_line);
	mxt_write_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_YSIZE,
			pdata->y_line);

	/* Set touchscreen orient */
	mxt_write_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_ORIENT,
			pdata->orient);

	/* Set touchscreen burst length */
	mxt_write_object(data, MXT_TOUCH_MULTI_T9,
			MXT_TOUCH_BLEN, pdata->blen);

	/* Set touchscreen threshold */
	mxt_write_object(data, MXT_TOUCH_MULTI_T9,
			MXT_TOUCH_TCHTHR, pdata->threshold);

	/* Set touchscreen resolution */
	mxt_write_object(data, MXT_TOUCH_MULTI_T9,
			MXT_TOUCH_XRANGE_LSB, (pdata->x_size - 1) & 0xff);
	mxt_write_object(data, MXT_TOUCH_MULTI_T9,
			MXT_TOUCH_XRANGE_MSB, (pdata->x_size - 1) >> 8);
	mxt_write_object(data, MXT_TOUCH_MULTI_T9,
			MXT_TOUCH_YRANGE_LSB, (pdata->y_size - 1) & 0xff);
	mxt_write_object(data, MXT_TOUCH_MULTI_T9,
			MXT_TOUCH_YRANGE_MSB, (pdata->y_size - 1) >> 8);

	/* Set touchscreen voltage */
	if (pdata->voltage) {
		if (pdata->voltage < MXT_VOLTAGE_DEFAULT) {
			voltage = (MXT_VOLTAGE_DEFAULT - pdata->voltage) /
				MXT_VOLTAGE_STEP;
			voltage = 0xff - voltage + 1;
		} else
			voltage = (pdata->voltage - MXT_VOLTAGE_DEFAULT) /
				MXT_VOLTAGE_STEP;

		mxt_write_object(data, MXT_SPT_CTECONFIG_T28,
				MXT_CTE_VOLTAGE, voltage);
	}

	/* Backup to memory */
	ret = mxt_write_object(data, MXT_GEN_COMMAND_T6,
			       MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);
	if (ret)
		return ret;
	msleep(MXT_BACKUP_TIME);

	return 0;
}

/* Update 24-bit CRC with two new bytes of data */
static u32 crc24_step(u32 crc, u8 byte1, u8 byte2)
{
	const u32 crcpoly = 0x80001b;
	u16 data = byte1 | (byte2 << 8);
	u32 result = data ^ (crc << 1);

	/* XOR result with crcpoly if bit 25 is set (overflow occurred) */
	if (result & 0x01000000)
		result ^= crcpoly;

	return result & 0x00ffffff;
}

static u32 crc24(u32 crc, const u8 *data, size_t len)
{
	size_t i;

	for (i = 0; i < len - 1; i += 2)
		crc = crc24_step(crc, data[i], data[i + 1]);

	/* If there were an odd number of bytes pad with 0 */
	if (i < len)
		crc = crc24_step(crc, data[i], 0);

	return crc;
}

static int mxt_verify_info_block_csum(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	size_t object_table_size, info_block_size;
	u32 crc = 0;
	u8 *info_block;
	int ret = 0;

	object_table_size = data->info.object_num * MXT_OBJECT_SIZE;
	info_block_size = sizeof(data->info) + object_table_size;
	info_block = kmalloc(info_block_size, GFP_KERNEL);
	if (!info_block)
		return -ENOMEM;

	/*
	 * Information Block CRC is computed over both ID info and Object Table
	 * So concat them in a temporary buffer, before computing CRC.
	 * TODO: refactor how the info block is read from the device such
	 * that it ends up in a single buffer and this copy is not needed.
	 */
	memcpy(info_block, &data->info, sizeof(data->info));
	memcpy(&info_block[sizeof(data->info)], data->object_table,
			object_table_size);

	crc = crc24(crc, info_block, info_block_size);

	if (crc != data->info_csum) {
		dev_err(dev, "Information Block CRC mismatch: %06x != %06x\n",
			data->info_csum, crc);
		ret = -EINVAL;
	}

	kfree(info_block);
	return ret;
}

static int mxt_get_info(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_info *info = &data->info;
	int error;

	/* Read 7-byte info block starting at address 0 */
	error = __mxt_read_reg(client, MXT_INFO, sizeof(*info), info);
	if (error)
		return error;

	return 0;
}

static void mxt_free_object_table(struct mxt_data *data)
{
	if (data->object_table) {
		devm_kfree(&data->client->dev, data->object_table);
		data->object_table = NULL;
	}

	if (data->current_id) {
		devm_kfree(&data->client->dev, data->current_id);
		data->current_id = NULL;
	}

	data->object_table = NULL;
	data->T6_reportid = 0;
	data->T9_reportid_min = 0;
	data->T9_reportid_max = 0;
	data->T19_reportid = 0;
	data->T100_reportid_min = 0;
	data->T100_reportid_max = 0;
	data->num_touchids = 0;
}

static int mxt_get_object_table(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &data->client->dev;
	size_t table_size;
	int error;
	int i;
	u8 reportid;
	u8 csum[3];

	/* Start by zapping old contents, if any. */
	mxt_free_object_table(data);

	table_size = data->info.object_num * sizeof(struct mxt_object);

	data->object_table = devm_kzalloc(dev, table_size, GFP_KERNEL);
	if (!data->object_table) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	error = __mxt_read_reg(client, MXT_OBJECT_START, table_size,
			data->object_table);
	if (error)
		return error;

	/*
	 * Read Information Block checksum from 3 bytes immediately following
	 * info block
	 */
	error = __mxt_read_reg(client, MXT_OBJECT_START + table_size,
			sizeof(csum), csum);
	if (error)
		return error;

	data->info_csum = csum[0] | (csum[1] << 8) | (csum[2] << 16);
	dev_info(dev, "Information Block Checksum = %06x\n", data->info_csum);

	error = mxt_verify_info_block_csum(data);
	if (error)
		return error;

	/* Valid Report IDs start counting from 1 */
	reportid = 1;
	for (i = 0; i < data->info.object_num; i++) {
		struct mxt_object *object = data->object_table + i;
		u8 min_id, max_id;

		le16_to_cpus(&object->start_address);

		if (object->num_report_ids) {
			min_id = reportid;
			reportid += object->num_report_ids *
					mxt_obj_instances(object);
			max_id = reportid - 1;
		} else {
			min_id = 0;
			max_id = 0;
		}

		dev_dbg(&data->client->dev,
			"Type %2d Start %3d Size %3zu Instances %2zu ReportIDs %3u : %3u\n",
			object->type, object->start_address,
			mxt_obj_size(object), mxt_obj_instances(object),
			min_id, max_id);

		switch (object->type) {
		case MXT_GEN_MESSAGE_T5:
			data->T5_address = object->start_address;
			data->message_length = mxt_obj_size(object) - 1;
			break;
		case MXT_GEN_COMMAND_T6:
			data->T6_reportid = min_id;
			break;
		case MXT_TOUCH_MULTI_T9:
			/* Only handle messages from first T9 instance */
			data->T9_reportid_min = min_id;
			data->T9_reportid_max = min_id +
						object->num_report_ids - 1;
			data->num_touchids = object->num_report_ids;
			data->has_T9 = true;
			break;
		case MXT_SPT_GPIOPWM_T19:
			data->T19_reportid = min_id;
			break;
		case MXT_SPT_MESSAGECOUNT_T44:
			data->T44_address = object->start_address;
			break;
		case MXT_TOUCH_MULTITOUCHSCREEN_T100:
			data->T100_reportid_min = min_id;
			data->T100_reportid_max = max_id;
			data->num_touchids = object->num_report_ids - 2;
			data->has_T100 = true;
			break;
		}
	}

	data->current_id = devm_kcalloc(dev,
					data->num_touchids,
					sizeof(*data->current_id),
					GFP_KERNEL);
	if (!data->current_id)
		return -ENOMEM;

	return 0;
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_info *info = &data->info;
	int error;

	error = mxt_get_info(data);
	if (error)
		return error;

	/* Get object table information */
	error = mxt_get_object_table(data);
	if (error)
		return error;

	/* Apply config from platform data */
	error = mxt_handle_pdata(data);
	if (error)
		return error;

	/* Soft reset */
	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				 MXT_COMMAND_RESET, 1);
	if (error)
		return error;

	msleep(MXT_RESET_TIME);

	dev_info(&client->dev,
			"Family ID: %u Variant ID: %u Major.Minor.Build: %u.%u.%02X\n",
			info->family_id, info->variant_id, info->version >> 4,
			info->version & 0xf, info->build);

	dev_info(&client->dev,
			"Matrix X Size: %u Matrix Y Size: %u Object Num: %u\n",
			info->matrix_xsize, info->matrix_ysize,
			info->object_num);

	mxt_save_power_config(data);
	mxt_save_aux_regs(data);
	mxt_stop(data);

	error = mxt_get_config_from_chip(data);
	if (error) {
		dev_err(&client->dev,
			"Failed to fetch config from chip: %d\n", error);
		return error;
	}

	return 0;
}

static int mxt_update_setting_T100(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_object *T100;
	u8 srcaux, tchaux;
	int ret;

	T100 = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!T100)
		return -EINVAL;

	/* Get SRCAUX Setting */
	ret = __mxt_read_reg(client, T100->start_address + MXT_T100_SCRAUX,
			1, &srcaux);
	if (ret)
		return ret;
	data->T100_enabled_num_reportable_touches =
			(srcaux & MXT_T100_SRCAUX_NUMRPTTCH);
	data->T100_enabled_touch_area = (srcaux & MXT_T100_SRCAUX_TCHAREA);
	data->T100_enabled_antitouch_area = (srcaux & MXT_T100_SRCAUX_ATCHAREA);
	data->T100_enabled_internal_tracking_area =
			(srcaux & MXT_T100_SRCAUX_INTTHRAREA);

	/* Get TCHAUX Setting */
	ret = __mxt_read_reg(client, T100->start_address + MXT_T100_TCHAUX,
			 1, &tchaux);
	if (ret)
		return ret;
	data->T100_enabled_vector = (tchaux & MXT_T100_TCHAUX_VECT);
	data->T100_enabled_amplitude = (tchaux & MXT_T100_TCHAUX_AMPL);
	data->T100_enabled_area = (tchaux & MXT_T100_TCHAUX_AREA);
	data->T100_enabled_peak = (tchaux & MXT_T100_TCHAUX_PEAK);

	dev_info(&client->dev, "T100 Config: SCRAUX : %X, TCHAUX : %X",
		 srcaux, tchaux);

	return 0;
}

static int mxt_calc_resolution_T100(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	u8 orient;
	__le16 xyrange[2];
	unsigned int max_x, max_y;
	u8 xylines[2];
	int ret;

	struct mxt_object *T100 = mxt_get_object(
		data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!T100)
		return -EINVAL;

	/* Get touchscreen resolution */
	ret = __mxt_read_reg(client, T100->start_address + MXT_T100_XRANGE,
			2, &xyrange[0]);
	if (ret)
		return ret;

	ret = __mxt_read_reg(client, T100->start_address + MXT_T100_YRANGE,
			2, &xyrange[1]);
	if (ret)
		return ret;

	ret = __mxt_read_reg(client, T100->start_address + MXT_T100_CFG1,
			1, &orient);
	if (ret)
		return ret;

	ret = __mxt_read_reg(client, T100->start_address + MXT_T100_XSIZE,
			1, &xylines[0]);
	if (ret)
		return ret;

	ret = __mxt_read_reg(client, T100->start_address + MXT_T100_YSIZE,
			1, &xylines[1]);
	if (ret)
		return ret;

	/* TODO: Read the TCHAUX field and save the VECT/AMPL/AREA config. */

	max_x = le16_to_cpu(xyrange[0]);
	max_y = le16_to_cpu(xyrange[1]);

	if (max_x == 0)
		max_x = 1023;

	if (max_y == 0)
		max_y = 1023;

	if (orient & MXT_T100_CFG_SWITCHXY) {
		data->max_x = max_y;
		data->max_y = max_x;
	} else {
		data->max_x = max_x;
		data->max_y = max_y;
	}

	data->max_area_pixels = max_x * max_y;
	data->max_area_channels = xylines[0] * xylines[1];

	dev_info(&client->dev,
		 "T100 Config: XSIZE %u, YSIZE %u, XLINE %u, YLINE %u",
		 max_x, max_y, xylines[0], xylines[1]);

	return 0;
}

static int mxt_calc_resolution_T9(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	u8 orient;
	__le16 xyrange[2];
	unsigned int max_x, max_y;
	u8 xylines[2];
	int ret;

	struct mxt_object *T9 = mxt_get_object(data, MXT_TOUCH_MULTI_T9);
	if (T9 == NULL)
		return -EINVAL;

	/* Get touchscreen resolution */
	ret = __mxt_read_reg(client, T9->start_address + MXT_TOUCH_XRANGE_LSB,
			4, xyrange);
	if (ret)
		return ret;

	ret = __mxt_read_reg(client, T9->start_address + MXT_TOUCH_ORIENT,
			1, &orient);
	if (ret)
		return ret;

	ret = __mxt_read_reg(client, T9->start_address + MXT_TOUCH_XSIZE,
			2, xylines);
	if (ret)
		return ret;

	max_x = le16_to_cpu(xyrange[0]);
	max_y = le16_to_cpu(xyrange[1]);

	if (orient & MXT_XY_SWITCH) {
		data->max_x = max_y;
		data->max_y = max_x;
	} else {
		data->max_x = max_x;
		data->max_y = max_y;
	}

	data->max_area_pixels = max_x * max_y;
	data->max_area_channels = xylines[0] * xylines[1];

	dev_info(&client->dev,
		 "T9 Config: XSIZE %u, YSIZE %u, XLINE %u, YLINE %u",
		 max_x, max_y, xylines[0], xylines[1]);

	return 0;
}

/*
 * Atmel Raw Config File Format
 *
 * The first four lines of the raw config file contain:
 *  1) Version
 *  2) Chip ID Information (first 7 bytes of device memory)
 *  3) Chip Information Block 24-bit CRC Checksum
 *  4) Chip Configuration 24-bit CRC Checksum
 *
 * The rest of the file consists of one line per object instance:
 *   <TYPE> <INSTANCE> <SIZE> <CONTENTS>
 *
 *  <TYPE> - 2-byte object type as hex
 *  <INSTANCE> - 2-byte object instance number as hex
 *  <SIZE> - 2-byte object size as hex
 *  <CONTENTS> - array of <SIZE> 1-byte hex values
 */
static int mxt_cfg_verify_hdr(struct mxt_data *data, char **config)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	struct mxt_info info;
	char *token;
	int ret = 0;
	u32 crc;

	/* Process the first four lines of the file*/
	/* 1) Version */
	token = strsep(config, "\n");
	dev_info(dev, "Config File: Version = %s\n", token ?: "<null>");
	if (!token ||
	    strncmp(token, MXT_CONFIG_VERSION, strlen(MXT_CONFIG_VERSION))) {
		dev_err(dev, "Invalid config file: Bad Version\n");
		return -EINVAL;
	}

	/* 2) Chip ID */
	token = strsep(config, "\n");
	if (!token) {
		dev_err(dev, "Invalid config file: No Chip ID\n");
		return -EINVAL;
	}
	ret = sscanf(token, "%hhx %hhx %hhx %hhx %hhx %hhx %hhx",
		     &info.family_id, &info.variant_id,
		     &info.version, &info.build, &info.matrix_xsize,
		     &info.matrix_ysize, &info.object_num);
	dev_info(dev, "Config File: Chip ID = %02x %02x %02x %02x %02x %02x %02x\n",
		info.family_id, info.variant_id, info.version, info.build,
		info.matrix_xsize, info.matrix_ysize, info.object_num);
	if (ret != 7 ||
	    info.family_id != data->info.family_id ||
	    info.variant_id != data->info.variant_id ||
	    info.version != data->info.version ||
	    info.build != data->info.build ||
	    info.object_num != data->info.object_num) {
		dev_err(dev, "Invalid config file: Chip ID info mismatch\n");
		dev_err(dev, "Chip Info: %02x %02x %02x %02x %02x %02x %02x\n",
			data->info.family_id, data->info.variant_id,
			data->info.version, data->info.build,
			data->info.matrix_xsize, data->info.matrix_ysize,
			data->info.object_num);
		return -EINVAL;
	}

	/* 3) Info Block CRC */
	token = strsep(config, "\n");
	if (!token) {
		dev_err(dev, "Invalid config file: No Info Block CRC\n");
		return -EINVAL;
	}

	if (info.matrix_xsize != data->info.matrix_xsize ||
	    info.matrix_ysize != data->info.matrix_ysize) {
		/*
		 * Matrix xsize and ysize depend on the state of T46 byte 1
		 * for the XY Mode. A mismatch is possible due to
		 * a corrupted register set. The config update should proceed
		 * to correct the problem. In this condition, the info block
		 * CRC check should be skipped.
		 */
		dev_info(dev, "Matrix Xsize and Ysize mismatch. Updating.\n");
		dev_info(dev, "Chip Info: %02x %02x %02x %02x %02x %02x %02x\n",
			 data->info.family_id, data->info.variant_id,
			 data->info.version, data->info.build,
			 data->info.matrix_xsize, data->info.matrix_ysize,
			 data->info.object_num);
		goto config_crc;
	}

	ret = sscanf(token, "%x", &crc);
	if (ret != 1 || crc != data->info_csum) {
		dev_err(dev, "Config File: Info Block CRC = %06x, info_csum = %06x\n",
			 crc, data->info_csum);
		dev_err(dev, "Invalid config file: Bad Info Block CRC\n");
		return -EINVAL;
	}

config_crc:
	/* 4) Config CRC */
	/*
	 * Parse but don't verify against current config;
	 * TODO: Verify against CRC of rest of file?
	 */
	token = strsep(config, "\n");
	if (!token) {
		dev_err(dev, "Invalid config file: No Config CRC\n");
		return -EINVAL;
	}
	ret = sscanf(token, "%x", &crc);
	dev_info(dev, "Config File: Config CRC = %06x\n", crc);
	if (ret != 1) {
		dev_err(dev, "Invalid config file: Bad Config CRC\n");
		return -EINVAL;
	}

	return 0;
}

static int mxt_cfg_proc_line(struct mxt_data *data, const char *line,
			     struct list_head *cfg_list)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	int ret;
	u16 type, instance, size;
	int len;
	struct mxt_cfg_file_line *cfg_line;
	struct mxt_object *object;
	u8 *content;
	size_t i;

	ret = sscanf(line, "%hx %hx %hx%n", &type, &instance, &size, &len);
	/* Skip unparseable lines */
	if (ret < 3)
		return 0;
	/* Only support 1-byte types */
	if (type > 0xff) {
		dev_err(dev, "Invalid type = %X\n", type);
		return -EINVAL;
	}

	/* Supplied object MUST be a valid instance and match object size */
	object = mxt_get_object(data, type);
	if (!object) {
		dev_err(dev, "Can't get object\n");
		return -EINVAL;
	}

	if (instance > mxt_obj_instances(object)) {
		dev_err(dev, "Too many instances.  Type=%x (%u > %zu)\n",
			type, instance, mxt_obj_instances(object));
		return -EINVAL;
	}

	if (size != mxt_obj_size(object)) {
		dev_err(dev, "Incorrect obect size. Type=%x (%u != %zu)\n",
			type, size, mxt_obj_size(object));
		return -EINVAL;
	}

	content = kmalloc(size, GFP_KERNEL);
	if (!content)
		return -ENOMEM;

	for (i = 0; i < size; i++) {
		line += len;
		ret = sscanf(line, "%hhx%n", &content[i], &len);
		if (ret < 1) {
			ret = -EINVAL;
			goto free_content;
		}
	}

	cfg_line = kzalloc(sizeof(*cfg_line), GFP_KERNEL);
	if (!cfg_line) {
		ret = -ENOMEM;
		goto free_content;
	}
	INIT_LIST_HEAD(&cfg_line->list);
	cfg_line->addr = object->start_address +
		instance * mxt_obj_size(object);
	cfg_line->size = mxt_obj_size(object);
	cfg_line->content = content;
	list_add_tail(&cfg_line->list, cfg_list);

	return 0;

free_content:
	kfree(content);
	return ret;
}

static int mxt_cfg_proc_data(struct mxt_data *data, char **config)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	char *line;
	int ret = 0;
	struct list_head cfg_lines;
	struct mxt_cfg_file_line *cfg_line, *cfg_line_tmp;

	INIT_LIST_HEAD(&cfg_lines);

	while ((line = strsep(config, "\n"))) {
		ret = mxt_cfg_proc_line(data, line, &cfg_lines);
		if (ret < 0)
			goto free_objects;
	}

	list_for_each_entry(cfg_line, &cfg_lines, list) {
		dev_dbg(dev, "Addr = %u Size = %u\n",
			cfg_line->addr, cfg_line->size);
		print_hex_dump(KERN_DEBUG, "atmel_mxt_ts: ", DUMP_PREFIX_OFFSET,
			       16, 1, cfg_line->content, cfg_line->size, false);

		ret = __mxt_write_reg(client, cfg_line->addr, cfg_line->size,
				cfg_line->content);
		if (ret)
			break;
	}

free_objects:
	list_for_each_entry_safe(cfg_line, cfg_line_tmp, &cfg_lines, list) {
		list_del(&cfg_line->list);
		kfree(cfg_line->content);
		kfree(cfg_line);
	}
	return ret;
}

static int mxt_load_config(struct mxt_data *data, const struct firmware *fw)
{
	struct device *dev = &data->client->dev;
	int ret, ret2;
	char *cfg_copy = NULL;
	char *running;

	ret = mutex_lock_interruptible(&data->fw_mutex);
	if (ret)
		return ret;

	/* Make a mutable, '\0'-terminated copy of the config file */
	cfg_copy = kmalloc(fw->size + 1, GFP_KERNEL);
	if (!cfg_copy) {
		ret = -ENOMEM;
		goto err_alloc_copy;
	}
	memcpy(cfg_copy, fw->data, fw->size);
	cfg_copy[fw->size] = '\0';

	/* Verify config file header (after which running points to data) */
	running = cfg_copy;
	ret = mxt_cfg_verify_hdr(data, &running);
	if (ret) {
		dev_err(dev, "Error verifying config header (%d)\n", ret);
		goto free_cfg_copy;
	}

	disable_irq(data->irq);

	if (data->input_dev) {
		input_unregister_device(data->input_dev);
		data->input_dev = NULL;
	}

	/* Write configuration */
	ret = mxt_cfg_proc_data(data, &running);
	if (ret) {
		dev_err(dev, "Error writing config file (%d)\n", ret);
		goto register_input_dev;
	}

	/* Backup nvram */
	ret = mxt_write_object(data, MXT_GEN_COMMAND_T6,
			       MXT_COMMAND_BACKUPNV,
			       MXT_BACKUP_VALUE);
	if (ret) {
		dev_err(dev, "Error backup to nvram (%d)\n", ret);
		goto register_input_dev;
	}
	msleep(MXT_BACKUP_TIME);

	/* Reset device */
	ret = mxt_write_object(data, MXT_GEN_COMMAND_T6,
			       MXT_COMMAND_RESET, 1);
	if (ret) {
		dev_err(dev, "Error resetting device (%d)\n", ret);
		goto register_input_dev;
	}
	msleep(MXT_RESET_TIME);

	mxt_save_power_config(data);
	mxt_save_aux_regs(data);
	mxt_stop(data);

register_input_dev:
	ret2 = mxt_get_config_from_chip(data);
	if (ret2) {
		dev_err(dev, "Failed to fetch config from chip (%d)\n", ret2);
		ret = ret2;
	}

	ret2 = mxt_input_dev_create(data);
	if (ret2) {
		dev_err(dev, "Error creating input_dev (%d)\n", ret2);
		ret = ret2;
	}

	/* Clear message buffer */
	ret2 = mxt_handle_messages(data, true);
	if (ret2) {
		dev_err(dev, "Error clearing msg buffer (%d)\n", ret2);
		ret = ret2;
	}

	enable_irq(data->irq);
free_cfg_copy:
	kfree(cfg_copy);
err_alloc_copy:
	mutex_unlock(&data->fw_mutex);
	return ret;
}

/*
 * Helper function for performing a T6 diagnostic command
 */
static int mxt_T6_diag_cmd(struct mxt_data *data, struct mxt_object *T6,
			   u8 cmd)
{
	int ret;
	u16 addr = T6->start_address + MXT_COMMAND_DIAGNOSTIC;

	ret = mxt_write_reg(data->client, addr, cmd);
	if (ret)
		return ret;

	/*
	 * Poll T6.diag until it returns 0x00, which indicates command has
	 * completed.
	 */
	while (cmd != 0) {
		ret = __mxt_read_reg(data->client, addr, 1, &cmd);
		if (ret)
			return ret;
	}
	return 0;
}

/*
 * SysFS Helper function for reading DELTAS and REFERENCE values for T37 object
 *
 * For both modes, a T37_buf is allocated to stores matrix_xsize * matrix_ysize
 * 2-byte (little-endian) values, which are returned to userspace unmodified.
 *
 * It is left to userspace to parse the 2-byte values.
 * - deltas are signed 2's complement 2-byte little-endian values.
 *     s32 delta = (b[0] + (b[1] << 8));
 * - refs are signed 'offset binary' 2-byte little-endian values, with offset
 *   value 0x4000:
 *     s32 ref = (b[0] + (b[1] << 8)) - 0x4000;
 */
static ssize_t mxt_T37_fetch(struct mxt_data *data, u8 mode)
{
	struct mxt_object *T6, *T37;
	u8 *obuf;
	ssize_t ret = 0;
	size_t i;
	size_t T37_buf_size, num_pages;
	size_t pos;

	if (!data || !data->object_table)
		return -ENODEV;

	T6 = mxt_get_object(data, MXT_GEN_COMMAND_T6);
	T37 = mxt_get_object(data, MXT_DEBUG_DIAGNOSTIC_T37);
	if (!T6 || mxt_obj_size(T6) < 6 || !T37 || mxt_obj_size(T37) < 3) {
		dev_err(&data->client->dev, "Invalid T6 or T37 object\n");
		return -ENODEV;
	}

	/* Something has gone wrong if T37_buf is already allocated */
	if (data->T37_buf)
		return -EINVAL;

	T37_buf_size = data->info.matrix_xsize * data->info.matrix_ysize *
		       sizeof(__le16);
	data->T37_buf_size = T37_buf_size;
	data->T37_buf = kmalloc(data->T37_buf_size, GFP_KERNEL);
	if (!data->T37_buf)
		return -ENOMEM;

	/* Temporary buffer used to fetch one T37 page */
	obuf = kmalloc(mxt_obj_size(T37), GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	disable_irq(data->irq);
	num_pages = DIV_ROUND_UP(T37_buf_size, mxt_obj_size(T37) - 2);
	pos = 0;
	for (i = 0; i < num_pages; i++) {
		u8 cmd;
		size_t chunk_len;

		/* For first page, send mode as cmd, otherwise PageUp */
		cmd = (i == 0) ? mode : MXT_T6_CMD_PAGE_UP;
		ret = mxt_T6_diag_cmd(data, T6, cmd);
		if (ret)
			goto err_free_T37_buf;

		ret = __mxt_read_reg(data->client, T37->start_address,
				mxt_obj_size(T37), obuf);
		if (ret)
			goto err_free_T37_buf;

		/* Verify first two bytes are current mode and page # */
		if (obuf[0] != mode) {
			dev_err(&data->client->dev,
				"Unexpected mode (%u != %u)\n", obuf[0], mode);
			ret = -EIO;
			goto err_free_T37_buf;
		}

		if (obuf[1] != i) {
			dev_err(&data->client->dev,
				"Unexpected page (%u != %zu)\n", obuf[1], i);
			ret = -EIO;
			goto err_free_T37_buf;
		}

		/*
		 * Copy the data portion of the page, or however many bytes are
		 * left, whichever is less.
		 */
		chunk_len = min(mxt_obj_size(T37) - 2, T37_buf_size - pos);
		memcpy(&data->T37_buf[pos], &obuf[2], chunk_len);
		pos += chunk_len;
	}

	goto out;

err_free_T37_buf:
	kfree(data->T37_buf);
	data->T37_buf = NULL;
	data->T37_buf_size = 0;
out:
	kfree(obuf);
	enable_irq(data->irq);
	return ret ?: 0;
}

static int mxt_update_file_name(struct device *dev, char** file_name,
				const char *buf, size_t count)
{
	char *new_file_name;

	/* Simple sanity check */
	if (count > 64) {
		dev_warn(dev, "File name too long\n");
		return -EINVAL;
	}

	/* FIXME: devm_kmemdup() when available */
	new_file_name = devm_kmalloc(dev, count + 1, GFP_KERNEL);
	if (!new_file_name) {
		dev_warn(dev, "no memory\n");
		return -ENOMEM;
	}

	memcpy(new_file_name, buf, count + 1);

	/* Echo into the sysfs entry may append newline at the end of buf */
	if (new_file_name[count - 1] == '\n')
		count--;

	new_file_name[count] = '\0';

	if (*file_name)
		devm_kfree(dev, *file_name);

	*file_name = new_file_name;

	return 0;
}

static ssize_t mxt_backupnv_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	/* Backup non-volatile memory */
	ret = mxt_write_object(data, MXT_GEN_COMMAND_T6,
			       MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);
	if (ret)
		return ret;
	msleep(MXT_BACKUP_TIME);

	return count;
}

static ssize_t mxt_calibrate_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	disable_irq(data->irq);
	ret = mxt_recalibrate(data);
	enable_irq(data->irq);
	return ret ?: count;
}

static ssize_t mxt_config_csum_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%06x\n", data->config_csum);
}

static ssize_t mxt_config_file_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n", data->config_file);
}

static ssize_t mxt_config_file_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	ret = mxt_update_file_name(dev, &data->config_file, buf, count);
	return ret ? ret : count;
}

static ssize_t mxt_fw_file_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n", data->fw_file);
}

static ssize_t mxt_fw_file_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	ret = mxt_update_file_name(dev, &data->fw_file, buf, count);
	if (ret)
		return ret;

	return count;
}

/* Firmware Version is returned as Major.Minor.Build */
static ssize_t mxt_fw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_info *info = &data->info;
	return scnprintf(buf, PAGE_SIZE, "%u.%u.%02X\n",
			 info->version >> 4, info->version & 0xf, info->build);
}

/* Hardware Version is returned as FamilyID.VariantID */
static ssize_t mxt_hw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_info *info = &data->info;
	return scnprintf(buf, PAGE_SIZE, "%u.%u\n",
			 info->family_id, info->variant_id);
}

static ssize_t mxt_info_csum_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%06x\n", data->info_csum);
}

/* Matrix Size is <MatrixSizeX> <MatrixSizeY> */
static ssize_t mxt_matrix_size_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_info *info = &data->info;
	return scnprintf(buf, PAGE_SIZE, "%u %u\n",
			 info->matrix_xsize, info->matrix_ysize);
}

static ssize_t mxt_object_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;
	u32 param;
	u8 type, instance, offset, val;

	ret = kstrtou32(buf, 16, &param);
	if (ret < 0)
		return -EINVAL;

	/*
	 * Byte Write Command is encoded in 32-bit word: TTIIOOVV:
	 * <Type> <Instance> <Offset> <Value>
	 */
	type = (param & 0xff000000) >> 24;
	instance = (param & 0x00ff0000) >> 16;
	offset = (param & 0x0000ff00) >> 8;
	val = param & 0x000000ff;

	ret = mxt_write_obj_instance(data, type, instance, offset, val);
	if (ret)
		return ret;

	return count;
}

static ssize_t mxt_update_config_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	const struct firmware *fw;
	int error;

	error = request_firmware(&fw, data->config_file, dev);
	if (error) {
		dev_err(dev, "Unable to open config file %s, %d\n",
			data->config_file, error);
		return error;
	}

	dev_info(dev, "Using config file %s (size = %zu)\n",
		 data->config_file, fw->size);

	error = mxt_load_config(data, fw);
	if (error)
		dev_err(dev, "The config update failed (%d)\n", error);
	else
		dev_dbg(dev, "The config update succeeded\n");

	release_firmware(fw);
	return error ?: count;
}

static int mxt_load_fw(struct mxt_data *data, const struct firmware *fw)
{
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	unsigned int frame_size;
	unsigned int pos = 0;
	int ret;

	ret = mutex_lock_interruptible(&data->fw_mutex);
	if (ret)
		return ret;

	if (!mxt_in_bootloader(data)) {
		ret = mxt_enter_bl(data);
		if (ret) {
			dev_err(dev, "Failed to enter bootloader, %d.\n", ret);
			goto out;
		}
	}

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD);
	if (ret) {
		dev_err(dev, "Checking WAITING_BOOTLOAD_CMD failed, %d\n", ret);
		goto out;
	}

	init_completion(&data->bl_completion);

	/* Unlock bootloader */
	ret = mxt_unlock_bootloader(client);
	if (ret) {
		dev_err(dev, "Unlock bootloader failed, %d\n", ret);
		goto out;
	}

	while (pos < fw->size) {
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA);
		if (ret) {
			dev_err(dev, "Checking WAITING_FRAME_DATE failed, %d\n",
				ret);
			goto out;
		}

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* We should add 2 at frame size as the the firmware data is not
		 * included the CRC bytes.
		 */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_fw_write(client, fw->data + pos, frame_size);
		if (ret) {
			dev_err(dev, "Writing frame to device failed, %d\n",
				ret);
			goto out;
		}

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS);
		if (ret) {
			dev_err(dev, "Checking FRAME_CRC_PASS failed, %d\n",
				ret);
			goto out;
		}

		pos += frame_size;

		dev_dbg(dev, "Updated %d bytes / %zd bytes\n", pos, fw->size);
	}

	/* Device exits bl mode to app mode only if successful */
	mxt_exit_bl(data);

out:
	mutex_unlock(&data->fw_mutex);
	return ret;
}

static ssize_t mxt_update_fw_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	const struct firmware *fw;
	char *envp[] = {"ERROR=1", NULL};
	int error;

	error = request_firmware(&fw, data->fw_file, dev);
	if (error) {
		dev_err(dev, "Unable to open firmware %s: %d\n",
			data->fw_file, error);
		return error;
	}

	error = mxt_load_fw(data, fw);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
	} else {
		dev_dbg(dev, "The firmware update succeeded\n");
	}

	release_firmware(fw);
	return error ?: count;
}

static ssize_t mxt_suspend_acq_interval_ms_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	u8 interval_reg = data->suspend_acq_interval;
	u8 interval_ms = (interval_reg == 255) ? 0 : interval_reg;
	return scnprintf(buf, PAGE_SIZE, "%u\n", interval_ms);
}

static ssize_t mxt_suspend_acq_interval_ms_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;
	u32 param;

	ret = kstrtou32(buf, 10, &param);
	if (ret < 0)
		return -EINVAL;

	/* 0 ms inteval means "free run" */
	if (param == 0)
		param = 255;
	/* 254 ms is the largest interval */
	else if (param > 254)
		param = 254;

	data->suspend_acq_interval = param;
	return count;
}

static ssize_t mxt_force_T19_report(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 T19_ctrl = 0;

	ret = __mxt_read_reg(client, MXT_SPT_GPIOPWM_T19, 1, &T19_ctrl);
	if (ret)
		return ret;
	/* Force T19 to report status */
	T19_ctrl = T19_ctrl | 0x04;
	ret = mxt_write_object(data, MXT_SPT_GPIOPWM_T19, 0, T19_ctrl);
	return ret ?: count;
}

static ssize_t mxt_T19_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%02x\n", data->T19_status);
}

static DEVICE_ATTR(backupnv, S_IWUSR, NULL, mxt_backupnv_store);
static DEVICE_ATTR(calibrate, S_IWUSR, NULL, mxt_calibrate_store);
static DEVICE_ATTR(config_csum, S_IRUGO, mxt_config_csum_show, NULL);
static DEVICE_ATTR(config_file, S_IRUGO | S_IWUSR, mxt_config_file_show,
		   mxt_config_file_store);
static DEVICE_ATTR(fw_file, S_IRUGO | S_IWUSR, mxt_fw_file_show,
		   mxt_fw_file_store);
static DEVICE_ATTR(fw_version, S_IRUGO, mxt_fw_version_show, NULL);
static DEVICE_ATTR(hw_version, S_IRUGO, mxt_hw_version_show, NULL);
static DEVICE_ATTR(info_csum, S_IRUGO, mxt_info_csum_show, NULL);
static DEVICE_ATTR(matrix_size, S_IRUGO, mxt_matrix_size_show, NULL);
static DEVICE_ATTR(object, S_IWUSR, NULL, mxt_object_store);
static DEVICE_ATTR(update_config, S_IWUSR, NULL, mxt_update_config_store);
static DEVICE_ATTR(update_fw, S_IWUSR, NULL, mxt_update_fw_store);
static DEVICE_ATTR(suspend_acq_interval_ms, S_IRUGO | S_IWUSR,
		   mxt_suspend_acq_interval_ms_show,
		   mxt_suspend_acq_interval_ms_store);
static DEVICE_ATTR(T19_status, S_IRUGO | S_IWUSR, mxt_T19_status_show,
		   mxt_force_T19_report);

static struct attribute *mxt_attrs[] = {
	&dev_attr_backupnv.attr,
	&dev_attr_calibrate.attr,
	&dev_attr_config_csum.attr,
	&dev_attr_config_file.attr,
	&dev_attr_fw_file.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_info_csum.attr,
	&dev_attr_matrix_size.attr,
	&dev_attr_object.attr,
	&dev_attr_update_config.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_T19_status.attr,
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

static struct attribute *mxt_power_attrs[] = {
	&dev_attr_suspend_acq_interval_ms.attr,
	NULL
};

static const struct attribute_group mxt_power_attr_group = {
	.name = power_group_name,
	.attrs = mxt_power_attrs,
};

/*
 **************************************************************
 * debugfs helper functions
 **************************************************************
*/

/*
 * Print the formatted string into the end of string |*str| which has size
 * |*str_size|. Extra space will be allocated to hold the formatted string
 * and |*str_size| will be updated accordingly.
 */
static int mxt_asprintf(char **str, size_t *str_size, const char *fmt, ...)
{
	unsigned int len;
	va_list ap, aq;
	int ret;
	char *str_tmp;

	va_start(ap, fmt);
	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	str_tmp = krealloc(*str, *str_size + len + 1, GFP_KERNEL);
	if (str_tmp == NULL)
		return -ENOMEM;

	*str = str_tmp;

	ret = vsnprintf(*str + *str_size, len + 1, fmt, ap);
	va_end(ap);

	if (ret != len)
		return -EINVAL;

	*str_size += len;

	return 0;
}

static int mxt_instance_fetch(char **str, size_t *count,
		struct mxt_object *object, int instance, const u8 *val)
{
	int i;
	int ret;

	if (mxt_obj_instances(object) > 1) {
		ret = mxt_asprintf(str, count, "Instance: %zu\n", instance);
		if (ret)
			return ret;
	}

	for (i = 0; i < mxt_obj_size(object); i++) {
		ret = mxt_asprintf(str, count,
				"\t[%2zu]: %02x (%d)\n", i, val[i], val[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int mxt_object_fetch(struct mxt_data *data)
{
	struct mxt_object *object;
	size_t count = 0;
	size_t i, j;
	int ret = 0;
	char *str = NULL;
	u8 *obuf;

	if (data->object_str)
		return -EINVAL;

	/* Pre-allocate buffer large enough to hold max sized object. */
	obuf = kmalloc(256, GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_readable(object->type))
			continue;

		ret = mxt_asprintf(&str, &count, "\nT%u\n", object->type);
		if (ret)
			goto err;

		for (j = 0; j < mxt_obj_instances(object); j++) {
			u16 size = mxt_obj_size(object);
			u16 addr = object->start_address + j * size;

			ret = __mxt_read_reg(data->client, addr, size, obuf);
			if (ret)
				goto done;

			ret = mxt_instance_fetch(&str, &count, object, j, obuf);
			if (ret)
				goto err;
		}
	}

	goto done;

err:
	kfree(str);
	str = NULL;
	count = 0;
done:
	data->object_str = str;
	data->object_str_size = count;
	kfree(obuf);
	return ret;
}

/*
 **************************************************************
 * debugfs interface
 **************************************************************
*/
static int mxt_debugfs_T37_open(struct inode *inode, struct file *file)
{
	struct mxt_data *mxt = inode->i_private;
	int ret;
	u8 cmd;

	if (file->f_path.dentry == mxt->dentry_deltas)
		cmd = MXT_T6_CMD_DELTAS;
	else if (file->f_path.dentry == mxt->dentry_refs)
		cmd = MXT_T6_CMD_REFS;
	else if (file->f_path.dentry == mxt->dentry_self_deltas)
		cmd = MXT_T6_CMD_SELF_DELTAS;
	else if (file->f_path.dentry == mxt->dentry_self_refs)
		cmd = MXT_T6_CMD_SELF_REFS;
	else
		return -EINVAL;

	/* Only allow one T37 debugfs file to be opened at a time */
	ret = mutex_lock_interruptible(&mxt->T37_buf_mutex);
	if (ret)
		return ret;

	if (!i2c_use_client(mxt->client)) {
		ret = -ENODEV;
		goto err_unlock;
	}

	/* Fetch all T37 pages into mxt->T37_buf */
	ret = mxt_T37_fetch(mxt, cmd);
	if (ret)
		goto err_release;

	file->private_data = mxt;

	return 0;

err_release:
	i2c_release_client(mxt->client);
err_unlock:
	mutex_unlock(&mxt->T37_buf_mutex);
	return ret;
}

static int mxt_debugfs_T37_release(struct inode *inode, struct file *file)
{
	struct mxt_data *mxt = file->private_data;

	file->private_data = NULL;

	kfree(mxt->T37_buf);
	mxt->T37_buf = NULL;
	mxt->T37_buf_size = 0;

	i2c_release_client(mxt->client);
	mutex_unlock(&mxt->T37_buf_mutex);

	return 0;
}


/* Return some bytes from the buffered T37 object, starting from *ppos */
static ssize_t mxt_debugfs_T37_read(struct file *file, char __user *buffer,
				    size_t count, loff_t *ppos)
{
	struct mxt_data *mxt = file->private_data;

	if (!mxt->T37_buf)
		return -ENODEV;

	if (*ppos >= mxt->T37_buf_size)
		return 0;

	if (count + *ppos > mxt->T37_buf_size)
		count = mxt->T37_buf_size - *ppos;

	if (copy_to_user(buffer, &mxt->T37_buf[*ppos], count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static const struct file_operations mxt_debugfs_T37_fops = {
	.owner = THIS_MODULE,
	.open = mxt_debugfs_T37_open,
	.release = mxt_debugfs_T37_release,
	.read = mxt_debugfs_T37_read
};

static int mxt_debugfs_object_open(struct inode *inode, struct file *file)
{
	struct mxt_data *mxt = inode->i_private;
	int ret;

	/* Only allow one object debugfs file to be opened at a time */
	ret = mutex_lock_interruptible(&mxt->object_str_mutex);
	if (ret)
		return ret;

	if (!i2c_use_client(mxt->client)) {
		ret = -ENODEV;
		goto err_object_unlock;
	}

	ret = mxt_object_fetch(mxt);
	if (ret)
		goto err_object_i2c_release;
	file->private_data = mxt;

	return 0;

err_object_i2c_release:
	i2c_release_client(mxt->client);
err_object_unlock:
	mutex_unlock(&mxt->object_str_mutex);
	return ret;
}

static int mxt_debugfs_object_release(struct inode *inode, struct file *file)
{
	struct mxt_data *mxt = file->private_data;
	file->private_data = NULL;

	kfree(mxt->object_str);
	mxt->object_str = NULL;
	mxt->object_str_size = 0;

	i2c_release_client(mxt->client);
	mutex_unlock(&mxt->object_str_mutex);

	return 0;
}

static ssize_t mxt_debugfs_object_read(struct file *file, char __user* buffer,
				   size_t count, loff_t *ppos)
{
	struct mxt_data *mxt = file->private_data;
	if (!mxt->object_str)
		return -ENODEV;

	if (*ppos >= mxt->object_str_size)
		return 0;

	if (count + *ppos > mxt->object_str_size)
		count = mxt->object_str_size - *ppos;

	if (copy_to_user(buffer, &mxt->object_str[*ppos], count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static const struct file_operations mxt_debugfs_object_fops = {
	.owner = THIS_MODULE,
	.open = mxt_debugfs_object_open,
	.release = mxt_debugfs_object_release,
	.read = mxt_debugfs_object_read,
};

static int mxt_debugfs_init(struct mxt_data *mxt)
{
	struct device *dev = &mxt->client->dev;

	if (!mxt_debugfs_root)
		return -ENODEV;

	mxt->dentry_dev = debugfs_create_dir(kobject_name(&dev->kobj),
					     mxt_debugfs_root);

	if (!mxt->dentry_dev)
		return -ENODEV;

	mutex_init(&mxt->T37_buf_mutex);

	mxt->dentry_deltas = debugfs_create_file("deltas", S_IRUSR,
						 mxt->dentry_dev, mxt,
						 &mxt_debugfs_T37_fops);
	mxt->dentry_refs = debugfs_create_file("refs", S_IRUSR,
					       mxt->dentry_dev, mxt,
					       &mxt_debugfs_T37_fops);
	if (is_hovering_supported(mxt)) {
		mxt->dentry_self_deltas =
			debugfs_create_file("self_deltas", S_IRUSR,
					    mxt->dentry_dev, mxt,
					    &mxt_debugfs_T37_fops);
		mxt->dentry_self_refs =
			debugfs_create_file("self_refs", S_IRUSR,
					    mxt->dentry_dev, mxt,
					    &mxt_debugfs_T37_fops);
	}
	mutex_init(&mxt->object_str_mutex);

	mxt->dentry_object = debugfs_create_file("object", S_IRUGO,
						 mxt->dentry_dev, mxt,
						 &mxt_debugfs_object_fops);
	return 0;
}

static void mxt_debugfs_remove(struct mxt_data *mxt)
{
	if (mxt->dentry_dev) {
		debugfs_remove_recursive(mxt->dentry_dev);
		mutex_destroy(&mxt->object_str_mutex);
		kfree(mxt->object_str);
		mutex_destroy(&mxt->T37_buf_mutex);
		kfree(mxt->T37_buf);
	}
}

static int mxt_save_regs(struct mxt_data *data, u8 type, u8 instance,
			 u8 offset, u8 *val, u16 size)
{
	struct mxt_object *object;
	u16 addr;
	int ret;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	addr = object->start_address + instance * mxt_obj_size(object) + offset;
	ret = __mxt_read_reg(data->client, addr, size, val);
	if (ret)
		return -EINVAL;

	return 0;
}

static int mxt_set_regs(struct mxt_data *data, u8 type, u8 instance,
			u8 offset, const u8 *val, u16 size)
{
	struct mxt_object *object;
	u16 addr;
	int ret;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	addr = object->start_address + instance * mxt_obj_size(object) + offset;
	ret = __mxt_write_reg(data->client, addr, size, val);
	if (ret)
		return -EINVAL;

	return 0;
}

static void mxt_save_power_config(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;

	/* Save 3 bytes T7 Power config */
	ret = mxt_save_regs(data, MXT_GEN_POWER_T7, 0, 0,
			    data->T7_config, sizeof(data->T7_config));
	if (ret)
		dev_err(dev, "Save T7 Power config failed, %d\n", ret);

	data->T7_config_valid = (ret == 0);
}

static void mxt_restore_power_config(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;

	if (data->T7_config_valid) {
		ret = mxt_set_regs(data, MXT_GEN_POWER_T7, 0, 0,
				   data->T7_config, sizeof(data->T7_config));
		if (ret)
			dev_err(dev, "Set T7 power config failed, %d\n", ret);
	} else {
		static const u8 fallback_T7_config[] = {
			FALLBACK_MXT_POWER_IDLEACQINT,
			FALLBACK_MXT_POWER_ACTVACQINT,
			FALLBACK_MXT_POWER_ACTV2IDLETO
		};

		dev_err(dev, "No T7 values found, setting to fallback value\n");
		ret = mxt_set_regs(data, MXT_GEN_POWER_T7, 0, 0,
				   fallback_T7_config,
				   sizeof(fallback_T7_config));
		if (ret)
			dev_err(dev, "Set T7 to fallbacks failed, %d\n", ret);
	}
}

static void mxt_save_aux_regs(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;

	if (is_hovering_supported(data)) {
		/* Save 1 byte T101 Ctrl config */
		ret = mxt_save_regs(data, MXT_SPT_TOUCHSCREENHOVER_T101, 0, 0,
				    &data->T101_ctrl, 1);
		if (ret)
			dev_err(dev, "Save T101 ctrl config failed, %d\n", ret);
		data->T101_ctrl_valid = (ret == 0);
	}

	ret = mxt_save_regs(data, MXT_PROCI_TOUCHSUPPRESSION_T42, 0, 0,
			    &data->T42_ctrl, 1);
	if (ret)
		dev_err(dev, "Save T42 ctrl config failed, %d\n", ret);
	data->T42_ctrl_valid = (ret == 0);

	ret = mxt_save_regs(data, MXT_SPT_GPIOPWM_T19, 0, 0,
			    &data->T19_ctrl, 1);
	if (ret)
		dev_err(dev, "Save T19 ctrl config failed, %d\n", ret);
	data->T19_ctrl_valid = (ret == 0);
}

static void mxt_restore_aux_regs(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;

	/* Restore the T42 ctrl to before-suspend value */
	if (data->T42_ctrl_valid) {
		ret = mxt_set_regs(data, MXT_PROCI_TOUCHSUPPRESSION_T42, 0, 0,
				   &data->T42_ctrl, 1);
		if (ret)
			dev_err(dev, "Set T42 ctrl failed, %d\n", ret);
	}

	/* Restore the T19 ctrl to before-suspend value */
	if (data->T19_ctrl_valid) {
		ret = mxt_set_regs(data, MXT_SPT_GPIOPWM_T19, 0, 0,
				   &data->T19_ctrl, 1);
		if (ret)
			dev_err(dev, "Set T19 ctrl failed, %d\n", ret);
	}

	if (is_hovering_supported(data) && data->T101_ctrl_valid) {
		ret = mxt_set_regs(data, MXT_SPT_TOUCHSCREENHOVER_T101, 0, 0,
				   &data->T101_ctrl, 1);
		if (ret)
			dev_err(dev, "Set T101 ctrl config failed, %d\n", ret);
	}
}

static void mxt_deep_sleep(struct mxt_data *data)
{
	static const u8 T7_config_deepsleep[3] = { 0x00, 0x00, 0x00 };
	int error;

	error = mxt_set_regs(data, MXT_GEN_POWER_T7, 0, 0,
			     T7_config_deepsleep, sizeof(T7_config_deepsleep));
	if (error)
		dev_err(&data->client->dev,
			"Set T7 Power config (deep sleep) failed: %d\n",
			error);
}

static void mxt_start(struct mxt_data *data)
{
	if (data->has_T100)
		mxt_ensure_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100,
				  MXT_T100_CTRL, MXT_TOUCH_CTRL_OPERATIONAL);

	mxt_restore_power_config(data);

	/* Enable touch reporting */
	if (data->has_T9)
		mxt_write_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_CTRL,
				 MXT_TOUCH_CTRL_OPERATIONAL);
}

static void mxt_stop(struct mxt_data *data)
{
	mxt_deep_sleep(data);

	/* Disable touch reporting */
	if (data->has_T9)
		mxt_write_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_CTRL,
				 MXT_TOUCH_CTRL_OFF);
}

static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	if (!dev->inhibited) {
		disable_irq(data->client->irq);
		mxt_start(data);
		enable_irq(data->client->irq);
	}

	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	if (!dev->inhibited) {
		disable_irq(data->client->irq);
		mxt_stop(data);
		enable_irq(data->client->irq);
	}
}

static int mxt_input_inhibit(struct input_dev *input)
{
	struct mxt_data *data = input_get_drvdata(input);
	struct device *dev = &data->client->dev;

	dev_dbg(dev, "inhibit\n");

	if (input->users) {
		disable_irq(data->client->irq);
		mxt_save_aux_regs(data);
		mxt_stop(data);
		enable_irq(data->client->irq);
	}

	return 0;
}

static int mxt_input_uninhibit(struct input_dev *input)
{
	struct mxt_data *data = input_get_drvdata(input);
	struct device *dev = &data->client->dev;
	int error;

	dev_dbg(dev, "uninhibit\n");

	if (!input->users)
		return 0;

	disable_irq(data->client->irq);

	/* Read all pending messages so that CHG line can be de-asserted */
	error = mxt_handle_messages(data, false);
	if (error)
		dev_warn(dev,
			 "error while clearing pending messages when un-inhibiting: %d\n",
			 error);

	mxt_release_all_fingers(data);

	mxt_restore_aux_regs(data);
	mxt_start(data);

	/*
	 * As hovering is supported in mXT33xT series, an extra calibration is
	 * required to reflect environment change especially for sensitive SC
	 * deltas when system resumes from suspend/idle.
	 */
	if (is_mxt_33x_t(data))
		mxt_recalibrate(data);

	enable_irq(data->client->irq);

	return 0;
}

static int mxt_get_config_from_chip(struct mxt_data *data)
{
	int error;

	error = data->has_T9 ? mxt_calc_resolution_T9(data) :
			       mxt_calc_resolution_T100(data);
	if (error)
		return error;

	/* Update T100 settings */
	if (data->has_T100) {
		error = mxt_update_setting_T100(data);
		if (error)
			return error;
	}

	return 0;
}

static int mxt_input_dev_create(struct mxt_data *data)
{
	const struct mxt_platform_data *pdata = data->pdata;
	struct input_dev *input_dev;
	int error;
	int max_area_channels;
	int max_touch_major;

	/* Don't need to register input_dev in bl mode */
	if (mxt_in_bootloader(data))
		return 0;

	/* Clear the existing one if it exists */
	if (data->input_dev) {
		input_unregister_device(data->input_dev);
		data->input_dev = NULL;
	}

	input_dev = devm_input_allocate_device(&data->client->dev);
	if (!input_dev)
		return -ENOMEM;

	input_dev->name = (data->is_tp) ? "Atmel maXTouch Touchpad" :
					  "Atmel maXTouch Touchscreen";
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &data->client->dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;
	input_dev->inhibit = mxt_input_inhibit;
	input_dev->uninhibit = mxt_input_uninhibit;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	if (data->is_tp) {
		int i;
		__set_bit(INPUT_PROP_POINTER, input_dev->propbit);
		__set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);

		if (!pdata)
			__set_bit(BTN_LEFT, input_dev->keybit);
		for (i = 0; i < MXT_NUM_GPIO; i++)
			if (pdata && pdata->key_map[i] != KEY_RESERVED)
				__set_bit(pdata->key_map[i], input_dev->keybit);

		__set_bit(BTN_TOOL_FINGER, input_dev->keybit);
		__set_bit(BTN_TOOL_DOUBLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_TRIPLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_QUADTAP, input_dev->keybit);
		__set_bit(BTN_TOOL_QUINTTAP, input_dev->keybit);

		input_abs_set_res(input_dev, ABS_X, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_Y, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_X,
				  MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_Y,
				  MXT_PIXELS_PER_MM);
	}

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE,
			     0, 255, 0, 0);
	input_abs_set_res(input_dev, ABS_X, MXT_PIXELS_PER_MM);
	input_abs_set_res(input_dev, ABS_Y, MXT_PIXELS_PER_MM);

	/* For multi touch */
	error = input_mt_init_slots(input_dev, data->num_touchids, 0);
	if (error)
		goto err_free_input_dev;

	max_area_channels = min(255U, data->max_area_channels);
	max_touch_major = get_touch_major_pixels(data, max_area_channels);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, max_touch_major, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			     0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_DISTANCE,
			     DISTANCE_ACTIVE_TOUCH, DISTANCE_HOVERING, 0, 0);
	input_abs_set_res(input_dev, ABS_MT_POSITION_X, MXT_PIXELS_PER_MM);
	input_abs_set_res(input_dev, ABS_MT_POSITION_Y, MXT_PIXELS_PER_MM);

	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error)
		goto err_free_input_dev;

	data->input_dev = input_dev;
	return 0;

err_free_input_dev:

	/*
	 * Even though input device is managed we free it on error
	 * because next time mxt_input_dev_create() is called it
	 * would not know if device is fully registered or not and
	 * if it should be unregistered or freed.
	 */
	input_free_device(input_dev);
	return error;
}

static int mxt_power_on(struct mxt_data *data)
{
	int error;

	/*
	 * If we do not have reset gpio assume platform firmware
	 * controls regulators and does power them on for us.
	 */
	if (IS_ERR_OR_NULL(data->reset_gpio))
		return 0;

	gpiod_set_value_cansleep(data->reset_gpio, 1);

	error = regulator_enable(data->vdd);
	if (error) {
		dev_err(&data->client->dev,
			"failed to enable vdd regulator: %d\n",
			error);
		goto release_reset_gpio;
	}

	error = regulator_enable(data->avdd);
	if (error) {
		dev_err(&data->client->dev,
			"failed to enable avdd regulator: %d\n",
			error);
		regulator_disable(data->vdd);
		goto release_reset_gpio;
	}

release_reset_gpio:
	gpiod_set_value_cansleep(data->reset_gpio, 0);
	if (error)
		return error;

	msleep(MXT_POWERON_DELAY);

	return 0;
}

static void mxt_power_off(void *_data)
{
	struct mxt_data *data = _data;

	if (!IS_ERR_OR_NULL(data->reset_gpio)) {
		/*
		 * Activate reset gpio to prevent leakage through the
		 * pin once we shut off power to the controller.
		 */
		gpiod_set_value_cansleep(data->reset_gpio, 1);
		regulator_disable(data->avdd);
		regulator_disable(data->vdd);
	}
}

static int mxt_check_device_present(struct i2c_client *client)
{
	union i2c_smbus_data dummy;
	unsigned short orig_addr = client->addr;

	do {
		if (i2c_smbus_xfer(client->adapter, client->addr,
				   0, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE,
				   &dummy) == 0)
			return 0;

		/*
		 * FIXME: This is basically a hack to cope with device tree
		 * or ACPI specifying given address but device coming up in
		 * bootloader mode and using another address. We can't do what
		 * we did earlier and define 2-nd device at alternate address
		 * as they will both try to grab reset gpio and clash, so we
		 * hack around it here.
		 * Note that we do not know family ID yet, so we need to try
		 * both bootloader addresses.
		 */
		switch (client->addr) {
		case 0x4a:
		case 0x4b:
			client->addr -= 0x24;
			break;

		case 0x27:
		case 0x26:
			if (orig_addr == 0x4a || orig_addr == 0x4b) {
				/* Try 0x24 or 0x25 */
				client->addr -= 2;
				break;
			}
			/* Fall through */

		default:
			return -ENXIO;
		}
	} while (1);
}

static void mxt_cleanup_fs(void *_data)
{
	struct mxt_data *data = _data;
	struct i2c_client *client = data->client;

	if (data->debugfs_initialized)
		mxt_debugfs_remove(data);

	if (data->power_sysfs_group_merged)
		sysfs_unmerge_group(&client->dev.kobj, &mxt_power_attr_group);

	if (data->sysfs_group_created)
		sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
}

static int mxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	const struct mxt_platform_data *pdata = dev_get_platdata(&client->dev);
	struct mxt_data *data;
	unsigned long irqflags;
	int error;

	data = devm_kzalloc(&client->dev, sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	if (id)
		data->is_tp = !strcmp(id->name, "atmel_mxt_tp");
#ifdef CONFIG_ACPI
	else {
		/*
		 * Check the ACPI device ID to determine if this device
		 * is a touchpad because i2c_device_id is NULL when probed
		 * from the ACPI device id table.
		 */
		struct acpi_device *adev;
		acpi_status status;
		status = acpi_bus_get_device(ACPI_HANDLE(&client->dev), &adev);
		if (ACPI_SUCCESS(status))
			data->is_tp = !strncmp(dev_name(&adev->dev),
					       "ATML0000", 8);
	}
#endif
	data->client = client;
	i2c_set_clientdata(client, data);

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/input0",
		 client->adapter->nr, client->addr);

	data->pdata = pdata;
	data->irq = client->irq;

	mutex_init(&data->fw_mutex);

	init_completion(&data->bl_completion);
	init_completion(&data->auto_cal_completion);

	data->suspend_acq_interval = MXT_SUSPEND_ACQINT_VALUE;

	data->vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		error = PTR_ERR(data->vdd);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
				"Failed to get 'vdd' regulator: %d\n",
				error);
		return error;
	}

	data->avdd = devm_regulator_get(&client->dev, "avdd");
	if (IS_ERR(data->avdd)) {
		error = PTR_ERR(data->avdd);
		if (error != -EPROBE_DEFER)
			dev_err(&client->dev,
				"Failed to get 'avdd' regulator: %d\n",
				error);
		return error;
	}

	data->reset_gpio = devm_gpiod_get(&client->dev, "atmel,reset",
					  GPIOD_OUT_LOW);
	if (IS_ERR(data->reset_gpio)) {
		error = PTR_ERR(data->reset_gpio);

		/*
		 * On Chromebooks vendors like to source touch panels from
		 * different vendors, but they are connected to the same
		 * regulators/GPIO pin. The drivers also use asynchronous
		 * probing, which means that more than one driver will
		 * attempt to claim the reset line. If we find it busy,
		 * let's try again later.
		 */
		if (error == -EBUSY) {
			dev_info(&client->dev,
				 "reset gpio is busy, deferring probe\n");
			return -EPROBE_DEFER;
		}

		if (error == -EPROBE_DEFER)
			return error;

		if (error != -ENOENT && error != -ENOSYS) {
			dev_err(&client->dev,
				"failed to get reset gpio: %d\n",
				error);
			return error;
		}
	}

	error = mxt_power_on(data);
	if (error)
		return error;

	error = devm_add_action(&client->dev, mxt_power_off, data);
	if (error) {
		dev_err(&client->dev,
			"failed to install power off action: %d\n", error);
		mxt_power_off(data);
		return error;
	}

	error = mxt_check_device_present(client);
	if (error)
		return error;

	error = mxt_update_file_name(&client->dev, &data->fw_file, MXT_FW_NAME,
				     strlen(MXT_FW_NAME));
	if (error)
		return error;

	error = mxt_update_file_name(&client->dev, &data->config_file,
				     MXT_CONFIG_NAME, strlen(MXT_CONFIG_NAME));
	if (error)
		return error;

	if (mxt_in_bootloader(data)) {
		dev_warn(&client->dev, "device in bootloader at probe\n");
	} else {
		error = mxt_initialize(data);
		if (error)
			return error;

	}

	/* Default to falling edge if no platform data provided */
	irqflags = data->pdata ? data->pdata->irqflags : IRQF_TRIGGER_FALLING;
	error = devm_request_threaded_irq(&client->dev, client->irq,
					  NULL, mxt_interrupt,
					  irqflags | IRQF_ONESHOT,
					  client->name, data);
	if (error) {
		dev_err(&client->dev, "failed to register interrupt: %d\n",
			error);
		return error;
	}

	/*
	 * Force the device to report back status so we can cache the device
	 * config checksum.
	 */
	error = mxt_write_object(data, MXT_GEN_COMMAND_T6,
				 MXT_COMMAND_REPORTALL, 1);
	if (error)
		dev_warn(&client->dev, "error making device report status.\n");

	if (!mxt_in_bootloader(data)) {
		error = mxt_input_dev_create(data);
		if (error)
			return error;

		error = mxt_handle_messages(data, true);
		if (error)
			return error;
	}

	error = devm_add_action(&client->dev, mxt_cleanup_fs, data);
	if (error) {
		dev_err(&client->dev,
			"failed to add cleanup fs action: %d\n",
			error);
		return error;
	}

	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error) {
		dev_err(&client->dev, "error creating sysfs entries.\n");
		return error;
	} else {
		data->sysfs_group_created = true;
	}

	error = sysfs_merge_group(&client->dev.kobj, &mxt_power_attr_group);
	if (error)
		dev_warn(&client->dev, "error merging power sysfs entries.\n");
	else
		data->power_sysfs_group_merged = true;

	error = mxt_debugfs_init(data);
	if (error)
		dev_warn(&client->dev, "error creating debugfs entries.\n");
	else
		data->debugfs_initialized = true;

	device_set_wakeup_enable(&client->dev, false);

	return 0;
}

static void __maybe_unused mxt_suspend_enable_T9(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	u8 T9_ctrl;
	int error;
	bool will_recalibrate;

	error = mxt_save_regs(data, MXT_TOUCH_MULTI_T9, 0, 0,
			      &T9_ctrl, 1);
	if (error) {
		dev_err(dev, "Get T9 ctrl config failed, %d\n", error);
		return;
	}

	dev_dbg(dev, "Current T9_Ctrl is %x\n", T9_ctrl);

	/*
	 * If the ENABLE bit is toggled, there will be auto-calibration msg.
	 * We will have to clear this msg before going into suspend otherwise
	 * it will wake up the device immediately
	 */
	will_recalibrate = !(T9_ctrl & MXT_TOUCH_CTRL_ENABLE);
	if (will_recalibrate)
		init_completion(&data->auto_cal_completion);

	/* Enable T9 object (ENABLE and REPORT) */
	T9_ctrl = MXT_TOUCH_CTRL_ENABLE | MXT_TOUCH_CTRL_RPTEN;
	error = mxt_set_regs(data, MXT_TOUCH_MULTI_T9, 0, 0,
			     &T9_ctrl, 1);
	if (error) {
		dev_err(dev, "Set T9 ctrl config failed, %d\n", error);
		return;
	}

	if (will_recalibrate) {
		error = wait_for_completion_timeout(&data->auto_cal_completion,
						    msecs_to_jiffies(350));
		if (error <= 0)
			dev_err(dev, "Wait for auto cal completion failed.\n");
	}
}

static void __maybe_unused mxt_idle_sleep(struct mxt_data *data)
{
	u8 T7_config_idle[] = {
		data->suspend_acq_interval, data->suspend_acq_interval, 0x00
	};
	int error;

	error = mxt_set_regs(data, MXT_GEN_POWER_T7, 0, 0,
			     T7_config_idle, sizeof(T7_config_idle));
	if (error)
		dev_err(&data->client->dev,
			"Set T7 Power config (idle) failed: %d\n", error);
}

static int __maybe_unused mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev;
	int ret;

	/*
	 * fw_mutex protects, as part of firmware/config update procedures,
	 * device transitions to and form bootloader mode and also
	 * teardown and [re-]creation of input device.
	 */
	ret = mutex_lock_interruptible(&data->fw_mutex);
	if (ret)
		return ret;

	if (mxt_in_bootloader(data))
		goto out;

	/*
	 * Even if device is not in bootloader mode we may not have
	 * input device created (no memory or something else). While
	 * we might do something smart with the power in such case, is
	 * should be not common occurrence so let's treat it the same
	 * as when device is in bootloader mode and just exit.
	 */
	input_dev = data->input_dev;
	if (!input_dev)
		goto out;

	/*
	 * Note that holding mutex here is not strictly necessary
	 * if inhibit/uninhibit/open/close can only be invoked by
	 * userspace activity (as they currently are) and not from
	 * within the kernel, since userspace is stunned during
	 * system suspend transition. But to be protected against
	 * possible future changes we are taking the mutex anyway.
	 */
	mutex_lock(&input_dev->mutex);

	disable_irq(data->irq);

	if (input_dev->inhibited)
		goto out_unlock_input;

	mxt_save_aux_regs(data);

	/*
	 *  For tpads, save T42 and T19 ctrl registers if may wakeup,
	 *  enable large object suppression, and disable button wake.
	 *  This will prevent a lid close from acting as a wake source.
	 */
	if (data->is_tp && device_may_wakeup(dev)) {
		u8 T42_sleep = 0x01;
		u8 T19_sleep = 0x00;
		u8 T101_sleep = 0x00;

		/* Enable Large Object Suppression */
		ret = mxt_set_regs(data, MXT_PROCI_TOUCHSUPPRESSION_T42, 0, 0,
				   &T42_sleep, 1);
		if (ret)
			dev_err(dev, "Set T42 ctrl failed, %d\n", ret);

		/* Disable Touchpad Button via GPIO */
		ret = mxt_set_regs(data, MXT_SPT_GPIOPWM_T19, 0, 0,
				   &T19_sleep, 1);
		if (ret)
			dev_err(dev, "Set T19 ctrl failed, %d\n", ret);

		/* Disable Hover, if supported */
		if (is_hovering_supported(data) &&
		    data->T101_ctrl_valid &&
		    data->T101_ctrl != T101_sleep) {
			unsigned long timeout = msecs_to_jiffies(350);

			init_completion(&data->auto_cal_completion);
			enable_irq(data->irq);

			ret = mxt_set_regs(data, MXT_SPT_TOUCHSCREENHOVER_T101,
					   0, 0, &T101_sleep, 1);
			if (ret)
				dev_err(dev, "Set T101 ctrl failed, %d\n", ret);

			ret = wait_for_completion_interruptible_timeout(
					&data->auto_cal_completion, timeout);
			if (ret <= 0)
				dev_err(dev,
					"Wait for cal completion failed.\n");
			disable_irq(data->irq);
		}
	} else {
		data->T42_ctrl_valid = data->T19_ctrl_valid = false;
	}

	if (device_may_wakeup(dev)) {
		mxt_idle_sleep(data);

		/*
		 * If we allow wakeup from touch, we have to enable T9 so
		 * that IRQ will be generated.
		 */
		if (data->has_T9)
			mxt_suspend_enable_T9(data);

		/* Enable wake from IRQ */
		data->irq_wake = (enable_irq_wake(data->irq) == 0);
	} else if (input_dev->users) {
		mxt_stop(data);
	}

out_unlock_input:
	mutex_unlock(&input_dev->mutex);
out:
	mutex_unlock(&data->fw_mutex);
	/*
	 * Shut off the controller unless it is supposed to be
	 * a wakeup source. Note that we do not use data->irq_wake
	 * as a condition here because enable_irq_wake() does fail
	 * on some ACPI systems which still get woken up properly
	 * because of platform magic.
	 */
	if (!device_may_wakeup(dev))
		mxt_power_off(data);

	disable_irq(data->irq);

	return 0;
}

static int __maybe_unused mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;
	int ret = 0;

	if (data->irq_wake) {
		disable_irq_wake(data->irq);
		data->irq_wake = false;
	}

	if (!device_may_wakeup(dev)) {
		/*
		 * If the controller was not a wake up source
		 * it was powered off and we need to power it on
		 * again.
		 */
		ret = mxt_power_on(data);
		if (ret)
			return ret;
	}

	ret = mutex_lock_interruptible(&data->fw_mutex);
	if (ret)
		return ret;

	if (mxt_in_bootloader(data))
		goto out;

	input_dev = data->input_dev;
	if (!input_dev)
		goto out;

	enable_irq(data->irq);

	mutex_lock(&input_dev->mutex);

	/*
	 * Process any pending message so that CHG line can be
	 * de-asserted
	 */
	ret = mxt_handle_messages(data, false);
	if (ret)
		dev_err(dev, "Handling message fails upon resume, %d\n", ret);

	if (input_dev->inhibited || !input_dev->users) {
		/*
		 * If device was indeed powered off in suspend then after
		 * powering it on it is normal operating mode and we need
		 * to put it to sleep.
		 */
		mxt_stop(data);
	} else {
		mxt_release_all_fingers(data);
		mxt_restore_aux_regs(data);
		mxt_start(data);

		/* Recalibration in case of environment change */
		if (!device_may_wakeup(dev) || is_mxt_33x_t(data))
			mxt_recalibrate(data);
	}

	enable_irq(data->irq);

	mutex_unlock(&input_dev->mutex);
out:
	mutex_unlock(&data->fw_mutex);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mxt_pm_ops, mxt_suspend, mxt_resume);

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "atmel_mxt_tp", 0 },
	{ "mXT224", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id mxt_acpi_id[] = {
	{ "ATML0000", 0 }, /* Touchpad */
	{ "ATML0001", 0 }, /* Touchscreen */
	{ }
};
MODULE_DEVICE_TABLE(acpi, mxt_acpi_id);
#endif

static struct i2c_driver mxt_driver = {
	.driver = {
		.name	= "atmel_mxt_ts",
		.pm	= &mxt_pm_ops,
		.acpi_match_table = ACPI_PTR(mxt_acpi_id),
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe		= mxt_probe,
	.id_table	= mxt_id,
};

static int __init mxt_init(void)
{
	/* Create a global debugfs root for all atmel_mxt_ts devices */
	mxt_debugfs_root = debugfs_create_dir(mxt_driver.driver.name, NULL);
	if (mxt_debugfs_root == ERR_PTR(-ENODEV))
		mxt_debugfs_root = NULL;

	return i2c_add_driver(&mxt_driver);
}

static void __exit mxt_exit(void)
{
	debugfs_remove_recursive(mxt_debugfs_root);

	i2c_del_driver(&mxt_driver);
}

module_init(mxt_init);
module_exit(mxt_exit);

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");
