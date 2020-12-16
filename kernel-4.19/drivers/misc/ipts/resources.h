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

#ifndef _IPTS_RESOURCE_H_
#define _IPTS_RESOURCE_H_

typedef enum {
	IPTS_BIN_KERNEL,
	IPTS_BIN_RO_DATA,
	IPTS_BIN_RW_DATA,
	IPTS_BIN_SENSOR_FRAME,
	IPTS_BIN_OUTPUT,
	IPTS_BIN_DYNAMIC_STATE_HEAP,
	IPTS_BIN_PATCH_LOCATION_LIST,
	IPTS_BIN_ALLOCATION_LIST,
	IPTS_BIN_COMMAND_BUFFER_PACKET,
	IPTS_BIN_TAG,
} ipts_bin_res_type_t;

typedef struct ipts_bin_header {
	char str[4];
	unsigned int version;
	unsigned int gfxcore;
	unsigned int revid;
} ipts_bin_header_t;

typedef struct ipts_bin_alloc {
	unsigned int handle;
	unsigned int reserved;
} ipts_bin_alloc_t;

typedef struct ipts_bin_alloc_list {
	unsigned int num;
	ipts_bin_alloc_t alloc[];
} ipts_bin_alloc_list_t;

typedef struct ipts_bin_cmdbuf {
	unsigned int size;
	char data[];
} ipts_bin_cmdbuf_t;

typedef struct ipts_bin_res {
	unsigned int handle;
	ipts_bin_res_type_t type;
	unsigned int initialize;
	unsigned int aligned_size;
	unsigned int size;
	char data[];
} ipts_bin_res_t;

typedef struct ipts_bin_io_header {
	char str[10];
	unsigned short type;
} ipts_bin_io_header_t;

typedef struct ipts_bin_res_list {
	unsigned int num;
	ipts_bin_res_t res[];
} ipts_bin_res_list_t;

typedef struct ipts_bin_patch {
	unsigned int index;
	unsigned int reserved1[2];
	unsigned int alloc_offset;
	unsigned int patch_offset;
	unsigned int reserved2;
} ipts_bin_patch_t;

typedef struct ipts_bin_patch_list {
	unsigned int num;
	ipts_bin_patch_t patch[];
} ipts_bin_patch_list_t;

typedef struct ipts_bin_guc_wq_info {
	unsigned int batch_offset;
	unsigned int size;
	char data[];
} ipts_bin_guc_wq_info_t;

typedef struct ipts_bin_bufid_patch {
	unsigned int imm_offset;
	unsigned int mem_offset;
} ipts_bin_bufid_patch_t;

typedef struct bin_workload {
	int cmdbuf_index;
	int iobuf_input;
	int iobuf_output[MAX_NUM_OUTPUT_BUFFERS];
} bin_workload_t;

typedef struct bin_buffer {
	unsigned int handle;
	ipts_mapbuffer_t *buf;
} bin_buffer_t;

typedef struct bin_alloc_info {
	bin_buffer_t *buffs;
	int num_of_allocations;
	int num_of_outputs;
	int num_of_buffers;
} bin_alloc_info_t;

typedef struct bin_guc_wq_item {
	unsigned int batch_offset;
	unsigned int size;
	char data[];
} bin_guc_wq_item_t;

typedef struct bin_kernel_info {
	bin_workload_t *wl;
	bin_alloc_info_t *alloc_info;
	bin_guc_wq_item_t *guc_wq_item;
	ipts_bin_bufid_patch_t bufid_patch;
	ipts_mapbuffer_t *bufid_buf;
} bin_kernel_info_t;

typedef struct bin_parse_info {
	u8 *data;
	int size;
	int parsed;
} bin_parse_info_t;

void ipts_free_resources(ipts_info_t *ipts);
int ipts_allocate_resources(ipts_info_t *ipts);

#endif // _IPTS_RESOURCE_H_
