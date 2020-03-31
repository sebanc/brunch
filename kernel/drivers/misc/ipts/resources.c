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

#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/intel-ipts.h>

#include "resources.h"

static ipts_mapbuffer_t *ipts_map_buffer(ipts_info_t *ipts, u32 size, u32 flags)
{
	ipts_mapbuffer_t *buf;
	int ret;

	buf = devm_kzalloc(&ipts->cldev->dev, sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->size = size;
	buf->flags = flags;

	ret = intel_ipts_map_buffer(buf);
	if (ret) {
		devm_kfree(&ipts->cldev->dev, buf);
		return NULL;
	}

	return buf;
}

static void ipts_unmap_buffer(ipts_info_t *ipts, ipts_mapbuffer_t *buf)
{
	if (!buf)
		return;

	intel_ipts_unmap_buffer(buf->buf_handle);

	devm_kfree(&ipts->cldev->dev, buf);
}

static int bin_read_fw(ipts_info_t *ipts, u8* data, int size)
{
	const struct firmware *fw = NULL;
	char fw_path[256];
	int ret = 0;

	snprintf(fw_path, 256, "intel/ipts/SurfaceTouchServicingSFTConfig%s.bin", ipts->hardware_id);
	ret = request_firmware(&fw, fw_path, &ipts->cldev->dev);
	if (ret) {
		dev_err(&ipts->cldev->dev, "cannot read fw %s\n", fw_path);
		return ret;
	}

	if (fw->size > size) {
		dev_err(&ipts->cldev->dev, "too small buffer to contain fw data\n");
		ret = -EINVAL;
		goto rel_return;
	}

	memcpy(data, fw->data, fw->size);

rel_return:
	release_firmware(fw);

	return ret;
}

static int bin_read_allocation_list(ipts_info_t *ipts,
					bin_parse_info_t *parse_info,
					bin_alloc_info_t *alloc_info)
{
	ipts_bin_alloc_list_t *alloc_list;
	int alloc_idx, buf_idx, num_of_buffers, parallel_idx, parsed, size;

	parsed = parse_info->parsed;
	size = parse_info->size;

	alloc_list = (ipts_bin_alloc_list_t *)&parse_info->data[parsed];

	if (sizeof(alloc_list->num) > size - parsed)
		return -EINVAL;

	parsed += sizeof(alloc_list->num);

	if (sizeof(alloc_list->alloc[0]) * alloc_list->num > size - parsed)
		return -EINVAL;

	num_of_buffers = TOUCH_SENSOR_MAX_DATA_BUFFERS * alloc_list->num + TOUCH_SENSOR_MAX_DATA_BUFFERS;

	alloc_info->buffs = vmalloc(sizeof(bin_buffer_t) * num_of_buffers);
	if (alloc_info->buffs == NULL)
		return -ENOMEM;

	memset(alloc_info->buffs, 0, sizeof(bin_buffer_t) * num_of_buffers);
	for (alloc_idx = 0; alloc_idx < alloc_list->num; alloc_idx++) {
		for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
			buf_idx = alloc_idx + (parallel_idx * alloc_list->num);
			alloc_info->buffs[buf_idx].handle =
					alloc_list->alloc[alloc_idx].handle;

		}

		parsed += sizeof(alloc_list->alloc[0]);
	}

	parse_info->parsed = parsed;
	alloc_info->num_of_allocations = alloc_list->num;
	alloc_info->num_of_buffers = num_of_buffers;

	dev_dbg(&ipts->cldev->dev, "number of allocations = %d, buffers = %d\n",
						alloc_info->num_of_allocations,
						alloc_info->num_of_buffers);

	return 0;
}

static int bin_read_cmd_buffer(ipts_info_t *ipts,
					bin_parse_info_t *parse_info,
					bin_alloc_info_t *alloc_info,
					bin_workload_t *wl)
{
	ipts_bin_cmdbuf_t *cmd;
	ipts_mapbuffer_t *buf;
	int cmdbuf_idx, parallel_idx, parsed, size;

	size = parse_info->size;
	parsed = parse_info->parsed;

	cmd = (ipts_bin_cmdbuf_t *)&parse_info->data[parsed];

	if (sizeof(cmd->size) > size - parsed)
		return -EINVAL;

	parsed += sizeof(cmd->size);
	if (cmd->size > size - parsed)
		return -EINVAL;

	dev_dbg(&ipts->cldev->dev, "cmd buf size = %d\n", cmd->size);

	cmdbuf_idx = TOUCH_SENSOR_MAX_DATA_BUFFERS * alloc_info->num_of_allocations;
	for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
		buf = ipts_map_buffer(ipts, cmd->size, 0);
		if (buf == NULL)
			return -ENOMEM;

		dev_dbg(&ipts->cldev->dev, "cmd_idx[%d] = %d, g:0x%p, c:0x%p\n", parallel_idx,
					cmdbuf_idx, buf->gfx_addr, buf->cpu_addr);

		memcpy((void *)buf->cpu_addr, &(cmd->data[0]), cmd->size);
		alloc_info->buffs[cmdbuf_idx].buf = buf;
		wl[parallel_idx].cmdbuf_index = cmdbuf_idx;

		cmdbuf_idx++;
	}

	parsed += cmd->size;
	parse_info->parsed = parsed;

	return 0;
}

static int bin_read_res_list(ipts_info_t *ipts,
					bin_parse_info_t *parse_info,
					bin_alloc_info_t *alloc_info,
					bin_workload_t *wl)
{
	ipts_bin_res_list_t *res_list;
	ipts_bin_res_t *res;
	ipts_mapbuffer_t *buf;
	int buf_idx, i, parallel_idx, parsed, size, output_idx = -1;
	u8 *bin_data;
	u32 buf_size, io_buf_type;
	
	parsed = parse_info->parsed;
	size = parse_info->size;
	bin_data = parse_info->data;

	res_list = (ipts_bin_res_list_t *)&parse_info->data[parsed];
	if (sizeof(res_list->num) > (size - parsed))
		return -EINVAL;
	parsed += sizeof(res_list->num);

	dev_dbg(&ipts->cldev->dev, "number of resources %u\n", res_list->num);
	for (i = 0; i < res_list->num; i++) {
		io_buf_type = 0;

		res = (ipts_bin_res_t *)(&(bin_data[parsed]));
		if (sizeof(res[0]) > (size - parsed)) {
			return -EINVAL;
		}

		dev_dbg(&ipts->cldev->dev, "Resource(%d):handle 0x%08x type %u init %u"
				" size %u aligned_size %u data 0x%08x\n",
				i, res->handle, res->type, res->initialize,
				res->size, res->aligned_size, res->data[0]);
                parsed += sizeof(res[0]);

		if (res->initialize) {
			if (res->size > (size - parsed)) {
				return -EINVAL;
			}
			parsed += res->size;
		}

		if (res->aligned_size) {
			ipts_bin_io_header_t *io_hdr;
			int alloc;
			for (alloc = 0; alloc < alloc_info->num_of_allocations; alloc++) {
				if (alloc_info->buffs[alloc].handle == res->handle)
					break;
			}

			if (alloc == alloc_info->num_of_allocations)
				dev_dbg(&ipts->cldev->dev,
				       "Couldnt match the handle 0x%x in res list\n", alloc);

			if (alloc_info->buffs[alloc].buf != NULL) {
				dev_dbg(&ipts->cldev->dev,
				    "Duplicate entry for resource index %d\n",
				     alloc);
				continue;
			}

			io_hdr = vmalloc(sizeof(ipts_bin_io_header_t));
			if (res->size > sizeof(ipts_bin_io_header_t)) {
				io_hdr = (ipts_bin_io_header_t *)(&res->data[0]);
				if (strncmp(io_hdr->str, "INTELTOUCH", 10) == 0)
					io_buf_type = (1 << io_hdr->type);
				if (io_buf_type == 2)
					output_idx++;
			}

			for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
				buf_idx = alloc + (parallel_idx * alloc_info->num_of_allocations);
				buf_size = res->aligned_size;
				if (io_buf_type == 1)
					wl[parallel_idx].iobuf_input = buf_idx;
				else if (io_buf_type == 2)
					wl[parallel_idx].iobuf_output[output_idx] = buf_idx;
				else if (parallel_idx != 0)
						continue;

				buf = ipts_map_buffer(ipts, buf_size, 1);
				if (!buf)
					return -ENOMEM;

				alloc_info->buffs[buf_idx].buf = buf;

				if (io_buf_type == 4) {
					bin_read_fw(ipts, buf->cpu_addr, buf_size);
					continue;
				}

				if (res->initialize)
					memcpy((void *)buf->cpu_addr, &(res->data[0]), res->size);
				else
					memset((void *)buf->cpu_addr, 0x0, buf_size);
			}
		}
	}

        alloc_info->num_of_outputs = output_idx + 1;
	parse_info->parsed = parsed;

	return 0;
}

static int bin_read_patch_list(ipts_info_t *ipts,
					bin_parse_info_t *parse_info,
					bin_alloc_info_t *alloc_info,
					bin_workload_t *wl)
{
	ipts_bin_patch_list_t *patch_list;
	ipts_mapbuffer_t *cmd = NULL;
	int buf_idx, cmd_idx, i, parallel_idx, parsed, size;
	u8 *batch;
	unsigned int gtt_offset;

	parsed = parse_info->parsed;
	size = parse_info->size;
	patch_list = (ipts_bin_patch_list_t *)&parse_info->data[parsed];

	if (sizeof(patch_list->num) > (size - parsed)) {
		return -EFAULT;
	}
	parsed += sizeof(patch_list->num);

	for (i = 0; i < patch_list->num; i++) {
		if (sizeof(patch_list->patch[0]) > (size - parsed)) {
			return -EFAULT;
		}

		for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
			cmd_idx = wl[parallel_idx].cmdbuf_index;
			buf_idx = patch_list->patch[i].index + parallel_idx *
						alloc_info->num_of_allocations;

			if (alloc_info->buffs[buf_idx].buf == NULL)
				buf_idx = patch_list->patch[i].index;

			cmd = alloc_info->buffs[cmd_idx].buf;
			batch = (char *)(u64)cmd->cpu_addr;

			gtt_offset = 0;
			if(alloc_info->buffs[buf_idx].buf != NULL)
				gtt_offset = (u32)(u64)
					alloc_info->buffs[buf_idx].buf->gfx_addr;
			gtt_offset += patch_list->patch[i].alloc_offset;

			batch += patch_list->patch[i].patch_offset;
			*(u32*)batch = gtt_offset;
		}

		parsed += sizeof(patch_list->patch[0]);
	}

	parse_info->parsed = parsed;

	return 0;
}

static int bin_read_guc_wq_item(ipts_info_t *ipts,
					bin_parse_info_t *parse_info,
					bin_guc_wq_item_t **guc_wq_item)
{
	ipts_bin_guc_wq_info_t *bin_guc_wq;
	bin_guc_wq_item_t *item;
	int hdr_size, i, parsed, size;
	
	parsed = parse_info->parsed;
	size = parse_info->size;
	bin_guc_wq = (ipts_bin_guc_wq_info_t *)&parse_info->data[parsed];

	dev_dbg(&ipts->cldev->dev, "wi size = %d, bt offset = %d\n", bin_guc_wq->size, bin_guc_wq->batch_offset);
	for (i = 0; i < bin_guc_wq->size / sizeof(u32); i++) {
		dev_dbg(&ipts->cldev->dev, "wi[%d] = 0x%08x\n", i, *((u32*)bin_guc_wq->data + i));
	}
	hdr_size = sizeof(bin_guc_wq->size) + sizeof(bin_guc_wq->batch_offset);

	if (hdr_size > (size - parsed)) {
		return -EINVAL;
	}
	parsed += hdr_size;

	item = vmalloc(sizeof(bin_guc_wq_item_t) + bin_guc_wq->size);
	if (item == NULL)
		return -ENOMEM;

	item->size = bin_guc_wq->size;
	item->batch_offset = bin_guc_wq->batch_offset;
	memcpy(item->data, bin_guc_wq->data, bin_guc_wq->size);

	*guc_wq_item = item;

	parsed += bin_guc_wq->size;
	parse_info->parsed = parsed;

	return 0;
}

static int bin_setup_guc_workqueue(ipts_info_t *ipts,
					bin_kernel_info_t *kernel)
{
	bin_alloc_info_t *alloc_info;
	bin_buffer_t *bin_buf;
	bin_workload_t *wl;
	int batch_offset, cmd_idx, i, iter_size, parallel_idx, wi_size, wq_size;
	u8 *wq_start, *wq_addr, *wi_data;
	
	wq_addr = (u8*)ipts->resources.wq_info.wq_addr;
	wq_size = ipts->resources.wq_info.wq_size;

	iter_size = ipts->resources.wq_item_size * TOUCH_SENSOR_MAX_DATA_BUFFERS;
	if (wq_size % iter_size) {
		dev_err(&ipts->cldev->dev, "wq item cannot fit into wq\n");
		return -EINVAL;
	}

	wq_start = wq_addr;
	for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS;
							parallel_idx++) {
			wl = kernel->wl;
			alloc_info = kernel->alloc_info;

			batch_offset = kernel->guc_wq_item->batch_offset;
			wi_size = kernel->guc_wq_item->size;
			wi_data = &kernel->guc_wq_item->data[0];
			
			cmd_idx = wl[parallel_idx].cmdbuf_index;
			bin_buf = &alloc_info->buffs[cmd_idx];

			*(u32*)(wi_data + batch_offset) =
				(u32)(unsigned long)(bin_buf->buf->gfx_addr);

			memcpy(wq_addr, wi_data, wi_size);

			wq_addr += wi_size;
	}

	for (i = 0; i < (wq_size / iter_size) - 1; i++) {
		memcpy(wq_addr, wq_start, iter_size);
		wq_addr += iter_size;
	}

	return 0;
}

static int bin_read_bufid_patch(ipts_info_t *ipts,
					bin_parse_info_t *parse_info,
					ipts_bin_bufid_patch_t *bufid_patch)
{
	ipts_bin_bufid_patch_t *patch;
	int size, parsed;

	parsed = parse_info->parsed;
	size = parse_info->size;
	patch = (ipts_bin_bufid_patch_t *)&parse_info->data[parsed];

	if (sizeof(ipts_bin_bufid_patch_t) > (size - parsed)) {
		dev_err(&ipts->cldev->dev, "invalid bufid info\n");
		return -EINVAL;
	}
	parsed += sizeof(ipts_bin_bufid_patch_t);

	memcpy(bufid_patch, patch, sizeof(ipts_bin_bufid_patch_t));

	parse_info->parsed = parsed;

	return 0;
}

static int bin_setup_bufid_buffer(ipts_info_t *ipts, bin_kernel_info_t *kernel)
{
	bin_alloc_info_t *alloc_info;
	bin_workload_t *wl;
	ipts_mapbuffer_t *buf, *cmd_buf;
	int cmd_idx, parallel_idx;
	u8 *batch;
	u32 mem_offset, imm_offset;

	buf = ipts_map_buffer(ipts, PAGE_SIZE, 0);
	if (!buf) {
		return -ENOMEM;
	}

	mem_offset = kernel->bufid_patch.mem_offset;
	imm_offset = kernel->bufid_patch.imm_offset;
	wl = kernel->wl;
	alloc_info = kernel->alloc_info;

        *((u32*)buf->cpu_addr) = -1;
	ipts->resources.last_buffer_completed = -1;
	ipts->resources.last_buffer_submitted = (int*)buf->cpu_addr;

	for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
		cmd_idx = wl[parallel_idx].cmdbuf_index;
		cmd_buf = alloc_info->buffs[cmd_idx].buf;
		batch = (u8*)(u64)cmd_buf->cpu_addr;

		*((u32*)(batch + mem_offset)) = (u32)(u64)(buf->gfx_addr);
                *((u32*)(batch + imm_offset)) = parallel_idx;
	}

	kernel->bufid_buf = buf;

	return 0;
}

static void unmap_buffers(ipts_info_t *ipts, bin_alloc_info_t *alloc_info)
{
	bin_buffer_t *buffs;
	int i, num_of_buffers;

	num_of_buffers = alloc_info->num_of_buffers;
	buffs = &alloc_info->buffs[0];

	for (i = 0; i < num_of_buffers; i++) {
		if (buffs[i].buf != NULL)
			ipts_unmap_buffer(ipts, buffs[i].buf);
	}
}

static int load_kernel(ipts_info_t *ipts, bin_parse_info_t *parse_info,
						bin_kernel_info_t *kernel)
{
	ipts_bin_header_t *hdr;
	bin_workload_t *wl;
	bin_alloc_info_t *alloc_info;
	bin_guc_wq_item_t *guc_wq_item = NULL;
	ipts_bin_bufid_patch_t bufid_patch;
	int ret;

	hdr = (ipts_bin_header_t *)parse_info->data;
	if (strncmp(hdr->str, "IOCL", 4) != 0) {
		dev_err(&ipts->cldev->dev, "binary header is not correct version = %d, "
				"string = %c%c%c%c\n", hdr->version,
				hdr->str[0], hdr->str[1],
				hdr->str[2], hdr->str[3] );
		return -EINVAL;
	}

	parse_info->parsed = sizeof(ipts_bin_header_t);
	wl = vmalloc(sizeof(bin_workload_t) * TOUCH_SENSOR_MAX_DATA_BUFFERS);
	if (wl == NULL)
		return -ENOMEM;
	memset(wl, 0, sizeof(bin_workload_t) * TOUCH_SENSOR_MAX_DATA_BUFFERS);

	alloc_info = vmalloc(sizeof(bin_alloc_info_t));
	if (alloc_info == NULL) {
		vfree(wl);
		return -ENOMEM;
	}
	memset(alloc_info, 0, sizeof(bin_alloc_info_t));

        dev_dbg(&ipts->cldev->dev, "kernel setup(size : %d)\n", parse_info->size);

	ret = bin_read_allocation_list(ipts, parse_info, alloc_info);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error read_allocation_list\n");
		goto setup_error;
	}

	ret = bin_read_cmd_buffer(ipts, parse_info, alloc_info, wl);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error read_cmd_buffer\n");
		goto setup_error;
	}

	ret = bin_read_res_list(ipts, parse_info, alloc_info, wl);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error read_res_list\n");
		goto setup_error;
	}

	ret = bin_read_patch_list(ipts, parse_info, alloc_info, wl);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error read_patch_list\n");
		goto setup_error;
	}

	ret = bin_read_guc_wq_item(ipts, parse_info, &guc_wq_item);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error read_guc_workqueue\n");
		goto setup_error;
	}

	memset(&bufid_patch, 0, sizeof(bufid_patch));
	ret = bin_read_bufid_patch(ipts, parse_info, &bufid_patch);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error read_bufid_patch\n");
		goto setup_error;
	}

	kernel->wl = wl;
	kernel->alloc_info = alloc_info;
	kernel->guc_wq_item = guc_wq_item;
	memcpy(&kernel->bufid_patch, &bufid_patch, sizeof(bufid_patch));

        return 0;

setup_error:
	vfree(guc_wq_item);

	unmap_buffers(ipts, alloc_info);

	vfree(alloc_info->buffs);
	vfree(alloc_info);
	vfree(wl);

	return ret;
}

void ipts_set_input_buffer(ipts_info_t *ipts, int parallel_idx,
						u8* cpu_addr, u64 dma_addr)
{
	ipts_buffer_info_t *touch_buf;

	touch_buf = ipts->resources.touch_data_buffer_raw;
	touch_buf[parallel_idx].dma_addr = dma_addr;
	touch_buf[parallel_idx].addr = cpu_addr;
}

void ipts_set_output_buffer(ipts_info_t *ipts, int parallel_idx, int output_idx,
						u8* cpu_addr, u64 dma_addr)
{
	ipts_buffer_info_t *output_buf;

	output_buf = &ipts->resources.raw_data_mode_output_buffer[parallel_idx][output_idx];

	output_buf->dma_addr = dma_addr;
	output_buf->addr = cpu_addr;
}

static void bin_setup_input_output(ipts_info_t *ipts, bin_kernel_info_t *kernel)
{
	bin_workload_t *wl;
	ipts_mapbuffer_t *buf;
	bin_alloc_info_t *alloc_info;
	int buf_idx, i, parallel_idx;

	wl = kernel->wl;
	alloc_info = kernel->alloc_info;
	ipts->resources.num_of_outputs = alloc_info->num_of_outputs;

	for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
		buf_idx = wl[parallel_idx].iobuf_input;
		buf = alloc_info->buffs[buf_idx].buf;

		dev_dbg(&ipts->cldev->dev, "in_buf[%d](%d) c:%p, p:%p, g:%p\n",
					parallel_idx, buf_idx, (void*)buf->cpu_addr,
					(void*)buf->phy_addr, (void*)buf->gfx_addr);

		ipts_set_input_buffer(ipts, parallel_idx, buf->cpu_addr,
								buf->phy_addr);

		for (i = 0; i < alloc_info->num_of_outputs; i++) {
			buf_idx = wl[parallel_idx].iobuf_output[i];
			buf = alloc_info->buffs[buf_idx].buf;

			dev_dbg(&ipts->cldev->dev, "out_buf[%d][%d] c:%p, p:%p, g:%p\n",
					parallel_idx, i, (void*)buf->cpu_addr,
					(void*)buf->phy_addr, (void*)buf->gfx_addr);

			ipts_set_output_buffer(ipts, parallel_idx, i,
					buf->cpu_addr, buf->phy_addr);
		}
	}
}

static void unload_kernel(ipts_info_t *ipts, bin_kernel_info_t *kernel)
{
	bin_alloc_info_t *alloc_info = kernel->alloc_info;
	bin_guc_wq_item_t *guc_wq_item = kernel->guc_wq_item;

	if (guc_wq_item) {
		vfree(guc_wq_item);
	}

	if (alloc_info) {
		unmap_buffers(ipts, alloc_info);

		vfree(alloc_info->buffs);
		vfree(alloc_info);
	}
}

static int setup_kernel(ipts_info_t *ipts)
{
	bin_kernel_info_t *kernel = NULL;
	const struct firmware *fw = NULL;
	bin_workload_t *wl;
	bin_parse_info_t parse_info;
	int ret = 0;
	char fw_path[256];

	kernel = vmalloc(sizeof(*kernel));
	if (kernel == NULL)
		return -ENOMEM;

	memset(kernel, 0, sizeof(*kernel));

	if (!strcmp(ipts->hardware_id, "MSHW0076") || !strcmp(ipts->hardware_id, "MSHW0078") || !strcmp(ipts->hardware_id, "MSHW0103"))
		snprintf(fw_path, 256, "intel/ipts/SurfaceTouchServicingKernelSKL%s.bin", ipts->hardware_id);
	else
		snprintf(fw_path, 256, "intel/ipts/SurfaceTouchServicingKernel%s.bin", ipts->hardware_id);
	ret = request_firmware(&fw, (const char *)fw_path, &ipts->cldev->dev);
	if (ret) {
		dev_err(&ipts->cldev->dev, "cannot read fw %s\n", fw_path);
		goto error_exit;
	}

	parse_info.data = (u8*)fw->data;
	parse_info.size = fw->size;
	parse_info.parsed = 0;

	ret = load_kernel(ipts, &parse_info, kernel);
	if (ret) {
		dev_err(&ipts->cldev->dev, "do_setup_kernel error : %d\n", ret);
		release_firmware(fw);
		goto error_exit;
	}

	release_firmware(fw);

	ipts->resources.wq_item_size = kernel->guc_wq_item->size;
	
	ret = bin_setup_guc_workqueue(ipts, kernel);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error setup_guc_workqueue\n");
		goto error_exit;
	}

	ret = bin_setup_bufid_buffer(ipts, kernel);
	if (ret) {
        	dev_err(&ipts->cldev->dev, "error setup_lastbubmit_buffer\n");
		goto error_exit;
	}

	bin_setup_input_output(ipts, kernel);

	wl = kernel->wl;
	vfree(wl);
	
	ipts->resources.kernel_handle = (u64)kernel;

	return 0;

error_exit:

	wl = kernel->wl;
	vfree(wl);
	unload_kernel(ipts, kernel);

	vfree(kernel);

	return ret;
}

static void release_kernel(ipts_info_t *ipts)
{
	bin_kernel_info_t *kernel;

	kernel = (bin_kernel_info_t *)ipts->resources.kernel_handle;

	unload_kernel(ipts, kernel);

	ipts_unmap_buffer(ipts, kernel->bufid_buf);

	vfree(kernel);

	ipts->resources.kernel_handle = 0;
}

static int ipts_allocate_common_resource(ipts_info_t *ipts)
{
	char *addr, *me2hid_addr;
	dma_addr_t dma_addr;
	int parallel_idx;

	addr = dmam_alloc_coherent(&ipts->cldev->dev,
			ipts->device_info.feedback_size,
			&dma_addr,
			GFP_ATOMIC|__GFP_ZERO);
	if (addr == NULL)
		return -ENOMEM;

	me2hid_addr = devm_kzalloc(&ipts->cldev->dev, ipts->device_info.feedback_size, GFP_KERNEL);
	if (me2hid_addr == NULL)
		return -ENOMEM;

	ipts->resources.hid2me_buffer.addr = addr;
	ipts->resources.hid2me_buffer.dma_addr = dma_addr;
	ipts->resources.hid2me_buffer_size = ipts->device_info.feedback_size;
	ipts->resources.me2hid_buffer = me2hid_addr;

	for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
		ipts->resources.feedback_buffer[parallel_idx].addr = dmam_alloc_coherent(&ipts->cldev->dev,
				ipts->device_info.feedback_size,
				&ipts->resources.feedback_buffer[parallel_idx].dma_addr,
				GFP_ATOMIC|__GFP_ZERO);

		if (ipts->resources.feedback_buffer[parallel_idx].addr == NULL)
			return -ENOMEM;
	}

	return 0;
}

static void ipts_free_common_resource(ipts_info_t *ipts)
{
	int parallel_idx;

	if (ipts->resources.me2hid_buffer) {
		devm_kfree(&ipts->cldev->dev, ipts->resources.me2hid_buffer);
		ipts->resources.me2hid_buffer = 0;
	}

	if (ipts->resources.hid2me_buffer.addr) {
		dmam_free_coherent(&ipts->cldev->dev, ipts->resources.hid2me_buffer_size, ipts->resources.hid2me_buffer.addr, ipts->resources.hid2me_buffer.dma_addr);
		ipts->resources.hid2me_buffer.addr = 0;
		ipts->resources.hid2me_buffer.dma_addr = 0;
		ipts->resources.hid2me_buffer_size = 0;
	}

	for (parallel_idx = 0; parallel_idx < TOUCH_SENSOR_MAX_DATA_BUFFERS; parallel_idx++) {
		if (ipts->resources.feedback_buffer[parallel_idx].addr) {
			dmam_free_coherent(&ipts->cldev->dev,
				ipts->device_info.feedback_size,
				ipts->resources.feedback_buffer[parallel_idx].addr,
				ipts->resources.feedback_buffer[parallel_idx].dma_addr);
			ipts->resources.feedback_buffer[parallel_idx].addr = 0;
			ipts->resources.feedback_buffer[parallel_idx].dma_addr = 0;
		}
	}
}

int ipts_allocate_resources(ipts_info_t *ipts)
{
	int ret;

	ret = ipts_allocate_common_resource(ipts);
	if (ret) {
		dev_err(&ipts->cldev->dev, "cannot allocate common resource\n");
		return ret;
	}

	ret = setup_kernel(ipts);
	if (ret) {
		dev_err(&ipts->cldev->dev, "cannot allocate raw data resource\n");
		ipts_free_common_resource(ipts);
		return ret;
	}

	ipts->resources.allocated = true;

	return 0;
}

void ipts_free_resources(ipts_info_t *ipts)
{
	if (!ipts->resources.allocated)
		return;

	ipts->resources.allocated = false;

	release_kernel(ipts);

	ipts_free_common_resource(ipts);
}
