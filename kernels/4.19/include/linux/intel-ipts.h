/*
 *
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _LINUX_INTEL_IPTS_H_
#define _LINUX_INTEL_IPTS_H_

#include <linux/hid.h>
#include <linux/mei_cl_bus.h>

#define MAX_NUM_OUTPUT_BUFFERS		16
#define TOUCH_SENSOR_MAX_DATA_BUFFERS   16

typedef struct ipts_buffer_info {
	char *addr;
	dma_addr_t dma_addr;
} ipts_buffer_info_t;

typedef struct ipts_wq_info {
	u64 db_addr;
	u64 db_phy_addr;
	u32 db_cookie_offset;
	u32 wq_size;
	u64 wq_addr;
	u64 wq_phy_addr;
	u64 wq_head_addr;
	u64 wq_head_phy_addr;
	u64 wq_tail_addr;
	u64 wq_tail_phy_addr;
} ipts_wq_info_t;

typedef struct ipts_resources {
	ipts_buffer_info_t feedback_buffer[TOUCH_SENSOR_MAX_DATA_BUFFERS];
	ipts_buffer_info_t hid2me_buffer;
	ipts_buffer_info_t raw_data_mode_output_buffer[TOUCH_SENSOR_MAX_DATA_BUFFERS][MAX_NUM_OUTPUT_BUFFERS];
	ipts_buffer_info_t touch_data_buffer_raw[TOUCH_SENSOR_MAX_DATA_BUFFERS];
	ipts_wq_info_t wq_info;
	u8 wq_item_size;
	u32 hid2me_buffer_size;
	u64 kernel_handle;
	int last_buffer_completed;
	int *last_buffer_submitted;
	int num_of_outputs;
	char *me2hid_buffer;
	bool allocated;
} ipts_resources_t;

typedef enum ipts_state {
	IPTS_STA_NONE,
	IPTS_STA_INIT,
	IPTS_STA_RESOURCE_READY,
	IPTS_STA_HID_STARTED,
	IPTS_STA_RAW_DATA_STARTED,
	IPTS_STA_STOPPING
} ipts_state_t;

typedef enum touch_sensor_mode
{
	TOUCH_SENSOR_MODE_HID = 0,
	TOUCH_SENSOR_MODE_RAW_DATA,
	TOUCH_SENSOR_MODE_SENSOR_DEBUG = 4,
	TOUCH_SENSOR_MODE_MAX
} touch_sensor_mode_t;

typedef enum touch_freq
{
	TOUCH_FREQ_RSVD = 0,
	TOUCH_FREQ_17MHZ,
	TOUCH_FREQ_30MHZ,
	TOUCH_FREQ_MAX
} touch_freq_t;

typedef enum touch_spi_io_mode
{
	TOUCH_SPI_IO_MODE_SINGLE = 0,
	TOUCH_SPI_IO_MODE_DUAL,
	TOUCH_SPI_IO_MODE_QUAD,
	TOUCH_SPI_IO_MODE_MAX
} touch_spi_io_mode_t;

typedef struct touch_sensor_get_device_info_rsp_data
{
	u16 vendor_id;
	u16 device_id;
	u32 hw_rev;
	u32 fw_rev;
	u32 frame_size;
	u32 feedback_size;
	touch_sensor_mode_t sensor_mode;
	u32 max_touch_points				:8;
	touch_freq_t spi_frequency			:8;
	touch_spi_io_mode_t spi_io_mode			:8;
	u32 reserved0					:8;
	u8 sensor_minor_eds_rev;
	u8 sensor_major_eds_rev;
	u8 me_minor_eds_rev;
	u8 me_major_eds_rev;
	u8 sensor_eds_intf_rev;
	u8 me_eds_intf_rev;
	u8 kernel_compat_ver;
	u8 reserved1;
	u32 reserved2[2];
} touch_sensor_get_device_info_rsp_data_t;

typedef struct ipts_info {
	struct hid_device *hid;
	struct mei_cl_device *cldev;
	ipts_resources_t resources;
	ipts_state_t state;
	touch_sensor_get_device_info_rsp_data_t device_info;
	char *hardware_id;
} ipts_info_t;

typedef struct ipts_mapbuffer {
	u32 size;
	u32 flags;
	void *gfx_addr;
	void *cpu_addr;
	u64 buf_handle;
	u64 phy_addr;
} ipts_mapbuffer_t;

int intel_ipts_map_buffer(ipts_mapbuffer_t *mapbuf);
int intel_ipts_unmap_buffer(uint64_t buf_handle);
int intel_ipts_connect(ipts_info_t *ipts, void *intel_ipts_handle_processed_data);
void intel_ipts_disconnect(void);

#endif // _LINUX_INTEL_IPTS_H_
