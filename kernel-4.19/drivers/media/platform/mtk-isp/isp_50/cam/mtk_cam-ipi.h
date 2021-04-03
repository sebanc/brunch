/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_IPI_H__
#define __MTK_CAM_IPI_H__

#include <linux/types.h>

/*
 * struct img_size - Image size information.
 *
 * @w: Image width, the unit is pixel
 * @h: Image height, the unit is pixel
 * @xsize: Bytes per line based on width.
 * @stride: Bytes per line when changing line.
 *          Stride is based on xsize + HW constrain(byte align).
 *
 */
struct img_size {
	u32 w;
	u32 h;
	u32 xsize;
	u32 stride;
} __packed;

/*
 * struct p1_img_crop - image corp information
 *
 * @left: The left of crop area.
 * @top: The top of crop area.
 * @width: The width of crop area.
 * @height: The height of crop area.
 *
 */
struct p1_img_crop {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
} __packed;

/*
 * struct dma_buffer - DMA buffer address information
 *
 * @iova: DMA address for ISP DMA device
 * @scp_addr: SCP address for external co-process unit
 *
 */
struct dma_buffer {
	u32 iova;
	u32 scp_addr;
} __packed;

/*
 * struct p1_img_output - ISP P1 image output information
 *
 * @buffer: DMA buffer address of image.
 * @size: The image size configuration.
 * @crop: The crop configuration.
 * @pixel_bits: The bits per image pixel.
 * @img_fmt: The image format.
 *
 */
struct p1_img_output {
	struct dma_buffer buffer;
	struct img_size size;
	struct p1_img_crop crop;
	u8 pixel_bits;
	u32 img_fmt;
} __packed;

/*
 * struct cfg_in_param - Image input parameters structure.
 *                       Normally, it comes from sensor information.
 *
 * @continuous: Indicate the sensor mode. Continuous or single shot.
 * @subsample: Indicate to enables SOF subsample or not.
 * @pixel_mode: Describe 1/2/4 pixels per clock cycle.
 * @data_pattern: Describe input data pattern.
 * @raw_pixel_id: Bayer sequence.
 * @tg_fps: The fps rate of TG (time generator).
 * @img_fmt: The image format of input source.
 * @p1_img_crop: The crop configuration of input source.
 *
 */
struct cfg_in_param {
	u8 continuous;
	u8 subsample;
	u8 pixel_mode;
	u8 data_pattern;
	u8 raw_pixel_id;
	u16 tg_fps;
	u32 img_fmt;
	struct p1_img_crop crop;
} __packed;

/*
 * struct cfg_main_out_param - The image output parameters of main stream.
 *
 * @bypass: Indicate this device is enabled or disabled or not.
 * @pure_raw: Indicate the image path control.
 *            True: pure raw
 *            False: processing raw
 * @pure_raw_pack: Indicate the image is packed or not.
 *                 True: packed mode
 *                 False: unpacked mode
 * @p1_img_output: The output image information.
 *
 */
struct cfg_main_out_param {
	u8 bypass;
	u8 pure_raw;
	u8 pure_raw_pack;
	struct p1_img_output output;
} __packed;

/*
 * struct cfg_resize_out_param - The image output parameters of
 *                               packed out stream.
 *
 * @bypass: Indicate this device is enabled or disabled or not.
 * @p1_img_output: The output image information.
 *
 */
struct cfg_resize_out_param {
	u8 bypass;
	struct p1_img_output output;
} __packed;

/*
 * struct p1_config_param - ISP P1 configuration parameters.
 *
 * @cfg_in_param: The Image input parameters.
 * @cfg_main_param: The main output image parameters.
 * @cfg_resize_out_param: The packed output image parameters.
 * @enabled_dmas: The enabled DMA port information.
 *
 */
struct p1_config_param {
	struct cfg_in_param cfg_in_param;
	struct cfg_main_out_param cfg_main_param;
	struct cfg_resize_out_param cfg_resize_param;
	u32 enabled_dmas;
} __packed;

/*
 * struct P1_meta_frame - ISP P1 meta frame information.
 *
 * @enabled_dma: The enabled DMA port information.
 * @sequence: The sequence number of meta buffer.
 * @meta_addr: DMA buffer address of meta buffer.
 *
 */
struct P1_meta_frame {
	u32 enabled_dma;
	u32 seq_no;
	struct dma_buffer meta_addr;
} __packed;

/*
 * struct isp_init_info - ISP P1 composer init information.
 *
 * @hw_module: The ISP Camera HW module ID.
 * @cq_addr: The DMA address of composer memory.
 *
 */
struct isp_init_info {
	u8 hw_module;
	struct dma_buffer cq_addr;
} __packed;

/*
 * struct isp_ack_info - ISP P1 IPI command ack information.
 *
 * @cmd_id: The IPI command ID is acked.
 * @frame_seq_no: The IPI frame sequence number is acked.
 *
 */
struct isp_ack_info {
	u8 cmd_id;
	u32 frame_seq_no;
} __packed;

/*
 * The IPI command enumeration.
 */
enum mtk_isp_scp_cmds {
	ISP_CMD_INIT,
	ISP_CMD_CONFIG,
	ISP_CMD_STREAM,
	ISP_CMD_DEINIT,
	ISP_CMD_ACK,
	ISP_CMD_FRAME_ACK,
	ISP_CMD_CONFIG_META,
	ISP_CMD_ENQUEUE_META,
	ISP_CMD_RESERVED,
};

/*
 * struct mtk_isp_scp_p1_cmd - ISP P1 IPI command strcture.
 *
 * @cmd_id: The IPI command ID.
 * @init_param: The init formation for ISP_CMD_INIT.
 * @config_param: The cmd configuration for ISP_CMD_CONFIG.
 * @enabled_dmas: The meta configuration information for ISP_CMD_CONFIG_META.
 * @is_stream_on: The stream information for ISP_CMD_STREAM.
 * @ack_info: The cmd ack. information for ISP_CMD_ACK.
 *
 */
struct mtk_isp_scp_p1_cmd {
	u8 cmd_id;
	union {
		struct isp_init_info init_param;
		struct p1_config_param config_param;
		u32 enabled_dmas;
		struct P1_meta_frame meta_frame;
		u8 is_stream_on;
		struct isp_ack_info ack_info;
	};
} __packed;

#endif /* __MTK_CAM_IPI_H__ */
