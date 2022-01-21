// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/of_platform.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/genalloc.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <uapi/media/cam_req_mgr.h>
#include <linux/debugfs.h>
#include "cam_smmu_api.h"
#include "cam_debug_util.h"

#define IO_MEM_POOL_GRANULARITY 12
#define SHARED_MEM_POOL_GRANULARITY 16

#define BYTE_SIZE 8
#define COOKIE_NUM_BYTE 2
#define COOKIE_SIZE (BYTE_SIZE*COOKIE_NUM_BYTE)
#define COOKIE_MASK ((1<<COOKIE_SIZE)-1)
#define HANDLE_INIT (-1)
#define CAM_SMMU_CB_MAX 5

#define GET_SMMU_HDL(x, y) (((x) << COOKIE_SIZE) | ((y) & COOKIE_MASK))
#define GET_SMMU_TABLE_IDX(x) (((x) >> COOKIE_SIZE) & COOKIE_MASK)

static int g_num_pf_handled = 4;
module_param(g_num_pf_handled, int, 0644);

struct firmware_alloc_info {
	struct device *fw_dev;
	void *fw_kva;
	size_t num_pages;
	struct page **pages;
};

static struct firmware_alloc_info icp_fw;

struct cam_smmu_work_payload {
	int idx;
	struct iommu_domain *domain;
	struct device *dev;
	unsigned long iova;
	int flags;
	void *token;
	struct list_head list;
};

enum cam_iommu_type {
	CAM_SMMU_INVALID,
	CAM_QSMMU,
	CAM_ARM_SMMU,
	CAM_SMMU_MAX,
};

enum cam_smmu_buf_state {
	CAM_SMMU_BUFF_EXIST,
	CAM_SMMU_BUFF_NOT_EXIST,
};

enum cam_smmu_init_dir {
	CAM_SMMU_TABLE_INIT,
	CAM_SMMU_TABLE_DEINIT,
};

struct secheap_buf_info {
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
};

struct cam_context_bank_info {
	struct device *dev;
	struct iommu_domain *domain;
	dma_addr_t va_start;
	size_t va_len;
	const char *name;
	uint8_t firmware_support;
	uint8_t shared_support;
	uint8_t io_support;
	uint8_t secheap_support;
	uint8_t qdss_support;
	dma_addr_t qdss_phy_addr;
	bool is_fw_allocated;
	bool is_secheap_allocated;
	bool is_qdss_allocated;

	struct gen_pool *shared_mem_pool;
	struct gen_pool *io_mem_pool;

	struct cam_smmu_region_info firmware_info;
	struct cam_smmu_region_info shared_info;
	struct cam_smmu_region_info io_info;
	struct cam_smmu_region_info secheap_info;
	struct cam_smmu_region_info qdss_info;
	struct secheap_buf_info secheap_buf;

	struct list_head smmu_buf_list;
	struct list_head smmu_buf_kernel_list;
	struct mutex lock;
	int handle;
	enum cam_smmu_ops_param state;

	cam_smmu_client_page_fault_handler handler[CAM_SMMU_CB_MAX];
	void *token[CAM_SMMU_CB_MAX];
	int cb_count;
	int pf_count;
};

struct cam_iommu_cb_set {
	struct cam_context_bank_info *cb_info;
	u32 cb_num;
	u32 cb_init_count;
	struct work_struct smmu_work;
	struct mutex payload_list_lock;
	struct list_head payload_list;
	u32 non_fatal_fault;
};

static const struct of_device_id msm_cam_smmu_dt_match[] = {
	{ .compatible = "qcom,msm-cam-smmu", },
	{ .compatible = "qcom,msm-cam-smmu-cb", },
	{ .compatible = "qcom,msm-cam-smmu-fw-dev", },
	{}
};

struct cam_dma_buff_info {
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	enum dma_data_direction dir;
	enum cam_smmu_region_id region_id;
	int ref_count;
	dma_addr_t paddr;
	struct list_head list;
	int dma_fd;
	size_t len;
	size_t phys_len;
};

static const char *qdss_region_name = "qdss";

static struct cam_iommu_cb_set iommu_cb_set;

static struct dentry *smmu_dentry;

static bool smmu_fatal_flag = true;

static int cam_smmu_check_handle_unique(int hdl);

static int cam_smmu_create_iommu_handle(int idx);

static int cam_smmu_create_add_handle_in_table(char *name,
	int *hdl);

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
	int dma_fd);

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_dma_buf(int idx,
	struct dma_buf *buf);

static int cam_smmu_map_buffer_and_add_to_list(int idx, int dma_fd,
	enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr, enum cam_smmu_region_id region_id);

static int cam_smmu_map_kernel_buffer_and_add_to_list(int idx,
	struct dma_buf *buf, enum dma_data_direction dma_dir,
	dma_addr_t *paddr_ptr, size_t *len_ptr,
	enum cam_smmu_region_id region_id);

static int cam_smmu_unmap_buf_and_remove_from_list(
	struct cam_dma_buff_info *mapping_info, int idx);

static void cam_smmu_clean_user_buffer_list(int idx);

static void cam_smmu_clean_kernel_buffer_list(int idx);

static void cam_smmu_print_user_list(int idx);

static void cam_smmu_print_kernel_list(int idx);

static void cam_smmu_print_table(void);

static int cam_smmu_probe(struct platform_device *pdev);

static uint32_t cam_smmu_find_closest_mapping(int idx, void *vaddr);

static void cam_smmu_page_fault_work(struct work_struct *work)
{
	int j;
	int idx;
	struct cam_smmu_work_payload *payload;
	uint32_t buf_info;

	mutex_lock(&iommu_cb_set.payload_list_lock);
	if (list_empty(&iommu_cb_set.payload_list)) {
		CAM_ERR(CAM_SMMU, "Payload list empty");
		mutex_unlock(&iommu_cb_set.payload_list_lock);
		return;
	}

	payload = list_first_entry(&iommu_cb_set.payload_list,
		struct cam_smmu_work_payload,
		list);
	list_del(&payload->list);
	mutex_unlock(&iommu_cb_set.payload_list_lock);

	/* Dereference the payload to call the handler */
	idx = payload->idx;
	buf_info = cam_smmu_find_closest_mapping(idx, (void *)payload->iova);
	if (buf_info != 0)
		CAM_INFO(CAM_SMMU, "closest buf 0x%x idx %d", buf_info, idx);

	for (j = 0; j < CAM_SMMU_CB_MAX; j++) {
		if ((iommu_cb_set.cb_info[idx].handler[j])) {
			iommu_cb_set.cb_info[idx].handler[j](
				payload->domain,
				payload->dev,
				payload->iova,
				payload->flags,
				iommu_cb_set.cb_info[idx].token[j],
				buf_info);
		}
	}
	kfree(payload);
}

static int cam_smmu_create_debugfs_entry(void)
{
	int rc = 0;

	smmu_dentry = debugfs_create_dir("camera_smmu", NULL);
	if (!smmu_dentry)
		return -ENOMEM;

	if (!debugfs_create_bool("cam_smmu_fatal",
		0644,
		smmu_dentry,
		&smmu_fatal_flag)) {
		CAM_ERR(CAM_SMMU, "failed to create cam_smmu_fatal entry");
		rc = -ENOMEM;
		goto err;
	}

	return rc;
err:
	debugfs_remove_recursive(smmu_dentry);
	smmu_dentry = NULL;
	return rc;
}

static void cam_smmu_print_user_list(int idx)
{
	struct cam_dma_buff_info *mapping;

	CAM_ERR(CAM_SMMU, "index = %d", idx);
	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		CAM_ERR(CAM_SMMU,
			"dma_fd = %d, paddr= 0x%pK, len = %u, region = %d",
			 mapping->dma_fd, (void *)mapping->paddr,
			 (unsigned int)mapping->len,
			 mapping->region_id);
	}
}

static void cam_smmu_print_kernel_list(int idx)
{
	struct cam_dma_buff_info *mapping;

	CAM_ERR(CAM_SMMU, "index = %d", idx);
	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list, list) {
		CAM_ERR(CAM_SMMU,
			"dma_buf = %pK, paddr= 0x%pK, len = %u, region = %d",
			 mapping->buf, (void *)mapping->paddr,
			 (unsigned int)mapping->len,
			 mapping->region_id);
	}
}

static void cam_smmu_print_table(void)
{
	int i;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		CAM_ERR(CAM_SMMU, "i= %d, handle= %d, name_addr=%pK", i,
			   (int)iommu_cb_set.cb_info[i].handle,
			   (void *)iommu_cb_set.cb_info[i].name);
		CAM_ERR(CAM_SMMU, "dev = %pK", iommu_cb_set.cb_info[i].dev);
	}
}

static uint32_t cam_smmu_find_closest_mapping(int idx, void *vaddr)
{
	struct cam_dma_buff_info *mapping, *closest_mapping =  NULL;
	unsigned long start_addr, end_addr, current_addr;
	uint32_t buf_handle = 0;

	long delta = 0, lowest_delta = 0;

	current_addr = (unsigned long)vaddr;
	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		start_addr = (unsigned long)mapping->paddr;
		end_addr = (unsigned long)mapping->paddr + mapping->len;

		if (start_addr <= current_addr && current_addr <= end_addr) {
			closest_mapping = mapping;
			CAM_INFO(CAM_SMMU,
				"Found va 0x%lx in:0x%lx-0x%lx, fd %d cb:%s",
				current_addr, start_addr,
				end_addr, mapping->dma_fd,
				iommu_cb_set.cb_info[idx].name);
			goto end;
		} else {
			if (start_addr > current_addr)
				delta =  start_addr - current_addr;
			else
				delta = current_addr - end_addr - 1;

			if (delta < lowest_delta || lowest_delta == 0) {
				lowest_delta = delta;
				closest_mapping = mapping;
			}
			CAM_DBG(CAM_SMMU,
				"approx va %lx not in range: %lx-%lx fd = %0x",
				current_addr, start_addr,
				end_addr, mapping->dma_fd);
		}
	}

end:
	if (closest_mapping) {
		buf_handle = GET_MEM_HANDLE(idx, closest_mapping->dma_fd);
		CAM_INFO(CAM_SMMU,
			"Closest map fd %d 0x%lx 0x%lx-0x%lx buf=%pK mem %0x",
			closest_mapping->dma_fd, current_addr,
			(unsigned long)closest_mapping->paddr,
			(unsigned long)closest_mapping->paddr + mapping->len,
			closest_mapping->buf,
			buf_handle);
	} else
		CAM_INFO(CAM_SMMU,
			"Cannot find vaddr:%lx in SMMU %s virt address",
			current_addr, iommu_cb_set.cb_info[idx].name);

	return buf_handle;
}

void cam_smmu_set_client_page_fault_handler(int handle,
	cam_smmu_client_page_fault_handler handler_cb, void *token)
{
	int idx, i = 0;

	if (!token || (handle == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: token is NULL or invalid handle");
		return;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return;
	}

	if (handler_cb) {
		if (iommu_cb_set.cb_info[idx].cb_count == CAM_SMMU_CB_MAX) {
			CAM_ERR(CAM_SMMU,
				"%s Should not regiester more handlers",
				iommu_cb_set.cb_info[idx].name);
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return;
		}

		iommu_cb_set.cb_info[idx].cb_count++;

		for (i = 0; i < iommu_cb_set.cb_info[idx].cb_count; i++) {
			if (iommu_cb_set.cb_info[idx].token[i] == NULL) {
				iommu_cb_set.cb_info[idx].token[i] = token;
				iommu_cb_set.cb_info[idx].handler[i] =
					handler_cb;
				break;
			}
		}
	} else {
		for (i = 0; i < CAM_SMMU_CB_MAX; i++) {
			if (iommu_cb_set.cb_info[idx].token[i] == token) {
				iommu_cb_set.cb_info[idx].token[i] = NULL;
				iommu_cb_set.cb_info[idx].handler[i] =
					NULL;
				iommu_cb_set.cb_info[idx].cb_count--;
				break;
			}
		}
		if (i == CAM_SMMU_CB_MAX)
			CAM_ERR(CAM_SMMU,
				"Error: hdl %x no matching tokens: %s",
				handle, iommu_cb_set.cb_info[idx].name);
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
}

void cam_smmu_unset_client_page_fault_handler(int handle, void *token)
{
	int idx, i = 0;

	if (!token || (handle == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: token is NULL or invalid handle");
		return;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return;
	}

	for (i = 0; i < CAM_SMMU_CB_MAX; i++) {
		if (iommu_cb_set.cb_info[idx].token[i] == token) {
			iommu_cb_set.cb_info[idx].token[i] = NULL;
			iommu_cb_set.cb_info[idx].handler[i] =
				NULL;
			iommu_cb_set.cb_info[idx].cb_count--;
			break;
		}
	}
	if (i == CAM_SMMU_CB_MAX)
		CAM_ERR(CAM_SMMU, "Error: hdl %x no matching tokens: %s",
			handle, iommu_cb_set.cb_info[idx].name);
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
}

static int cam_smmu_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova,
	int flags, void *token)
{
	char *cb_name;
	int idx;
	struct cam_smmu_work_payload *payload;

	if (!token) {
		CAM_ERR(CAM_SMMU, "Error: token is NULL");
		CAM_ERR(CAM_SMMU, "Error: domain = %pK, device = %pK",
			domain, dev);
		CAM_ERR(CAM_SMMU, "iova = %lX, flags = %d", iova, flags);
		return -EINVAL;
	}

	cb_name = (char *)token;
	/* Check whether it is in the table */
	for (idx = 0; idx < iommu_cb_set.cb_num; idx++) {
		if (!strcmp(iommu_cb_set.cb_info[idx].name, cb_name))
			break;
	}

	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: index is not valid, index = %d, token = %s",
			idx, cb_name);
		return -EINVAL;
	}

	if (++iommu_cb_set.cb_info[idx].pf_count > g_num_pf_handled) {
		CAM_INFO_RATE_LIMIT(CAM_SMMU, "PF already handled %d %d %d",
			g_num_pf_handled, idx,
			iommu_cb_set.cb_info[idx].pf_count);
		return -EINVAL;
	}

	payload = kzalloc(sizeof(struct cam_smmu_work_payload), GFP_ATOMIC);
	if (!payload)
		return -EINVAL;

	payload->domain = domain;
	payload->dev = dev;
	payload->iova = iova;
	payload->flags = flags;
	payload->token = token;
	payload->idx = idx;

	mutex_lock(&iommu_cb_set.payload_list_lock);
	list_add_tail(&payload->list, &iommu_cb_set.payload_list);
	mutex_unlock(&iommu_cb_set.payload_list_lock);

	cam_smmu_page_fault_work(&iommu_cb_set.smmu_work);

	return -EINVAL;
}

static void cam_smmu_reset_iommu_table(enum cam_smmu_init_dir ops)
{
	unsigned int i;
	int j = 0;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		iommu_cb_set.cb_info[i].handle = HANDLE_INIT;
		INIT_LIST_HEAD(&iommu_cb_set.cb_info[i].smmu_buf_list);
		INIT_LIST_HEAD(&iommu_cb_set.cb_info[i].smmu_buf_kernel_list);
		iommu_cb_set.cb_info[i].state = CAM_SMMU_DETACH;
		iommu_cb_set.cb_info[i].dev = NULL;
		iommu_cb_set.cb_info[i].cb_count = 0;
		iommu_cb_set.cb_info[i].pf_count = 0;
		for (j = 0; j < CAM_SMMU_CB_MAX; j++) {
			iommu_cb_set.cb_info[i].token[j] = NULL;
			iommu_cb_set.cb_info[i].handler[j] = NULL;
		}
		if (ops == CAM_SMMU_TABLE_INIT)
			mutex_init(&iommu_cb_set.cb_info[i].lock);
		else
			mutex_destroy(&iommu_cb_set.cb_info[i].lock);
	}
}

static int cam_smmu_check_handle_unique(int hdl)
{
	int i;

	if (hdl == HANDLE_INIT) {
		CAM_DBG(CAM_SMMU,
			"iommu handle is init number. Need to try again");
		return 1;
	}

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (iommu_cb_set.cb_info[i].handle == HANDLE_INIT)
			continue;

		if (iommu_cb_set.cb_info[i].handle == hdl) {
			CAM_DBG(CAM_SMMU, "iommu handle %d conflicts",
				(int)hdl);
			return 1;
		}
	}
	return 0;
}

/**
 *  use low 2 bytes for handle cookie
 */
static int cam_smmu_create_iommu_handle(int idx)
{
	int rand, hdl = 0;

	get_random_bytes(&rand, COOKIE_NUM_BYTE);
	hdl = GET_SMMU_HDL(idx, rand);
	CAM_DBG(CAM_SMMU, "create handle value = %x", (int)hdl);
	return hdl;
}

static int cam_smmu_attach_device(int idx)
{
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];

	if (IS_ERR_OR_NULL(cb->domain)) {
		CAM_ERR(CAM_SMMU, "Cannot get context bank domain");
		return PTR_ERR(cb->domain);
	}

	return iommu_attach_device(cb->domain, cb->dev);
}

static int cam_smmu_create_add_handle_in_table(char *name,
	int *hdl)
{
	int i;
	int handle;

	/* create handle and add in the iommu hardware table */
	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (!strcmp(iommu_cb_set.cb_info[i].name, name)) {
			mutex_lock(&iommu_cb_set.cb_info[i].lock);
			if (iommu_cb_set.cb_info[i].handle != HANDLE_INIT) {
				mutex_unlock(&iommu_cb_set.cb_info[i].lock);
				CAM_ERR(CAM_SMMU,
					"Error: %s already got handle 0x%x",
					name, iommu_cb_set.cb_info[i].handle);

				return -EINVAL;
			}

			/* make sure handle is unique */
			do {
				handle = cam_smmu_create_iommu_handle(i);
			} while (cam_smmu_check_handle_unique(handle));

			/* put handle in the table */
			iommu_cb_set.cb_info[i].handle = handle;
			iommu_cb_set.cb_info[i].cb_count = 0;
			*hdl = handle;
			CAM_DBG(CAM_SMMU, "%s creates handle 0x%x",
				name, handle);
			mutex_unlock(&iommu_cb_set.cb_info[i].lock);
			return 0;
		}
	}

	CAM_ERR(CAM_SMMU, "Error: Cannot find name %s or all handle exist",
		name);
	cam_smmu_print_table();
	return -EINVAL;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
	int dma_fd)
{
	struct cam_dma_buff_info *mapping;

	if (dma_fd < 0) {
		CAM_ERR(CAM_SMMU, "Invalid fd %d", dma_fd);
		return NULL;
	}

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->dma_fd == dma_fd) {
			CAM_DBG(CAM_SMMU, "find dma_fd %d", dma_fd);
			return mapping;
		}
	}

	CAM_ERR(CAM_SMMU, "Error: Cannot find entry by index %d", idx);

	return NULL;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_dma_buf(int idx,
	struct dma_buf *buf)
{
	struct cam_dma_buff_info *mapping;

	if (!buf) {
		CAM_ERR(CAM_SMMU, "Invalid dma_buf");
		return NULL;
	}

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list,
			list) {
		if (mapping->buf == buf) {
			CAM_DBG(CAM_SMMU, "find dma_buf %pK", buf);
			return mapping;
		}
	}

	CAM_ERR(CAM_SMMU, "Error: Cannot find entry by index %d", idx);

	return NULL;
}

static void cam_smmu_clean_user_buffer_list(int idx)
{
	int ret;
	struct cam_dma_buff_info *mapping_info, *temp;

	list_for_each_entry_safe(mapping_info, temp,
			&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		CAM_DBG(CAM_SMMU, "Free mapping address %pK, i = %d, fd = %d",
			(void *)mapping_info->paddr, idx,
			mapping_info->dma_fd);

		/* Clean up regular mapped buffers */
		ret = cam_smmu_unmap_buf_and_remove_from_list(
				mapping_info,
				idx);
		if (ret < 0) {
			CAM_ERR(CAM_SMMU, "Buffer delete failed: idx = %d",
				idx);
			CAM_ERR(CAM_SMMU,
				"Buffer delete failed: addr = %lx, fd = %d",
				(unsigned long)mapping_info->paddr,
				mapping_info->dma_fd);
			/*
			 * Ignore this error and continue to delete other
			 * buffers in the list
			 */
			continue;
		}
	}
}

static void cam_smmu_clean_kernel_buffer_list(int idx)
{
	int ret;
	struct cam_dma_buff_info *mapping_info, *temp;

	list_for_each_entry_safe(mapping_info, temp,
			&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list, list) {
		CAM_DBG(CAM_SMMU,
			"Free mapping address %pK, i = %d, dma_buf = %pK",
			(void *)mapping_info->paddr, idx,
			mapping_info->buf);

		/* Clean up regular mapped buffers */
		ret = cam_smmu_unmap_buf_and_remove_from_list(
				mapping_info,
				idx);

		if (ret < 0) {
			CAM_ERR(CAM_SMMU,
				"Buffer delete in kernel list failed: idx = %d",
				idx);
			CAM_ERR(CAM_SMMU,
				"Buffer delete failed: addr = %lx, dma_buf = %pK",
				(unsigned long)mapping_info->paddr,
				mapping_info->buf);
			/*
			 * Ignore this error and continue to delete other
			 * buffers in the list
			 */
			continue;
		}
	}
}

static int cam_smmu_attach(int idx)
{
	int ret;

	if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_ATTACH) {
		ret = -EALREADY;
	} else if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_DETACH) {
		ret = cam_smmu_attach_device(idx);
		if (ret < 0) {
			CAM_ERR(CAM_SMMU, "Error: ATTACH fail");
			return -ENODEV;
		}
		iommu_cb_set.cb_info[idx].state = CAM_SMMU_ATTACH;
		ret = 0;
	} else {
		CAM_ERR(CAM_SMMU, "Error: Not detach/attach: %d",
			iommu_cb_set.cb_info[idx].state);
		ret = -EINVAL;
	}

	return ret;
}

static int cam_smmu_detach_device(int idx)
{
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];

	if (IS_ERR_OR_NULL(cb->domain)) {
		CAM_ERR(CAM_SMMU, "Invalid context bank domain");
		return PTR_ERR(cb->domain);
	}

	iommu_detach_device(cb->domain, cb->dev);

	return 0;
}

int cam_smmu_alloc_firmware(int32_t smmu_hdl,
	dma_addr_t *iova,
	uintptr_t *cpuva,
	size_t *len)
{
	int rc = 0;
	int32_t idx;
	size_t firmware_len = 0;
	size_t firmware_start = 0;
	struct iommu_domain *domain;
	struct sg_table sgt;
	int i;

	if (!iova || !len || !cpuva || (smmu_hdl == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: Input args are invalid");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].firmware_support) {
		CAM_ERR(CAM_SMMU,
			"Firmware memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_fw_allocated) {
		CAM_ERR(CAM_SMMU, "Trying to allocate twice");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	firmware_len = iommu_cb_set.cb_info[idx].firmware_info.iova_len;
	firmware_start = iommu_cb_set.cb_info[idx].firmware_info.iova_start;
	CAM_DBG(CAM_SMMU, "Firmware area len from DT = %zu", firmware_len);

	icp_fw.num_pages = ALIGN(firmware_len, PAGE_SIZE) / PAGE_SIZE;
	icp_fw.pages = kcalloc(icp_fw.num_pages, sizeof(*icp_fw.pages), GFP_KERNEL);
	if (!icp_fw.pages)
		goto unlock_and_end;

	for (i = 0; i < icp_fw.num_pages; ++i) {
		icp_fw.pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!icp_fw.pages[i])
			goto free_pages;
	}

	icp_fw.fw_kva = vmap(icp_fw.pages, icp_fw.num_pages, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	if (!icp_fw.fw_kva) {
		CAM_ERR(CAM_SMMU, "FW memory alloc failed");
		rc = -ENOMEM;
		goto free_pages;
	}

	rc = sg_alloc_table_from_pages(&sgt, icp_fw.pages, icp_fw.num_pages, 0, firmware_len, GFP_KERNEL);
	if (rc)
		goto vunmap;

	/*
	 * FIXME: Map once to flush the caches and then unmap, because
	 * iommu_map_sg() needs an unmapped scatterlist.
	 */
	rc = dma_map_sg_attrs(icp_fw.fw_dev, sgt.sgl, sgt.orig_nents, DMA_BIDIRECTIONAL, 0);
	if (rc <= 0) {
		CAM_ERR(CAM_SMMU, "FW memory flush failed");
		sg_free_table(&sgt);
		rc = -ENOMEM;
		goto vunmap;
	}
	dma_unmap_sg_attrs(icp_fw.fw_dev, sgt.sgl, sgt.orig_nents, DMA_BIDIRECTIONAL, 0);

	domain = iommu_cb_set.cb_info[idx].domain;
	rc = iommu_map_sg(domain, firmware_start, sgt.sgl, sgt.orig_nents, IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV);
	sg_free_table(&sgt);

	if (rc < firmware_len) {
		CAM_ERR(CAM_SMMU, "Failed to map FW into IOMMU");
		rc = -ENOMEM;
		goto vunmap;
	}

	iommu_cb_set.cb_info[idx].is_fw_allocated = true;

	*iova = iommu_cb_set.cb_info[idx].firmware_info.iova_start;
	*cpuva = (uintptr_t)icp_fw.fw_kva;
	*len = firmware_len;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	return 0;
vunmap:
	vunmap(icp_fw.fw_kva);
	icp_fw.fw_kva = NULL;
free_pages:
	for (i = 0; i < icp_fw.num_pages; ++i) {
		if (icp_fw.pages[i])
			__free_pages(icp_fw.pages[i], 0);
	}
	kfree(icp_fw.pages);
	icp_fw.pages = NULL;
unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_alloc_firmware);

int cam_smmu_dealloc_firmware(int32_t smmu_hdl)
{
	int rc = 0;
	int32_t idx;
	size_t firmware_len = 0;
	size_t firmware_start = 0;
	struct iommu_domain *domain = NULL;
	size_t unmapped = 0;
	int i;

	if (smmu_hdl == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].firmware_support) {
		CAM_ERR(CAM_SMMU,
			"Firmware memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (!iommu_cb_set.cb_info[idx].is_fw_allocated) {
		CAM_ERR(CAM_SMMU,
			"Trying to deallocate firmware that is not allocated");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	firmware_len = iommu_cb_set.cb_info[idx].firmware_info.iova_len;
	firmware_start = iommu_cb_set.cb_info[idx].firmware_info.iova_start;
	domain = iommu_cb_set.cb_info[idx].domain;
	unmapped = iommu_unmap(domain,
		firmware_start,
		firmware_len);

	if (unmapped != firmware_len) {
		CAM_ERR(CAM_SMMU, "Only %zu unmapped out of total %zu",
			unmapped,
			firmware_len);
		rc = -EINVAL;
	}

	vunmap(icp_fw.fw_kva);
	icp_fw.fw_kva = NULL;

	for (i = 0; i < icp_fw.num_pages; ++i) {
		if (icp_fw.pages[i])
			__free_pages(icp_fw.pages[i], 0);
	}
	kfree(icp_fw.pages);
	icp_fw.pages = NULL;

	iommu_cb_set.cb_info[idx].is_fw_allocated = false;

unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_dealloc_firmware);

int cam_smmu_alloc_qdss(int32_t smmu_hdl,
	dma_addr_t *iova,
	size_t *len)
{
	int rc = 0;
	int32_t idx;
	size_t qdss_len = 0;
	size_t qdss_start = 0;
	dma_addr_t qdss_phy_addr;
	struct iommu_domain *domain;

	if (!iova || !len || (smmu_hdl == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: Input args are invalid");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].qdss_support) {
		CAM_ERR(CAM_SMMU,
			"QDSS memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_qdss_allocated) {
		CAM_ERR(CAM_SMMU, "Trying to allocate twice");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	qdss_len = iommu_cb_set.cb_info[idx].qdss_info.iova_len;
	qdss_start = iommu_cb_set.cb_info[idx].qdss_info.iova_start;
	qdss_phy_addr = iommu_cb_set.cb_info[idx].qdss_phy_addr;
	CAM_DBG(CAM_SMMU, "QDSS area len from DT = %zu", qdss_len);

	domain = iommu_cb_set.cb_info[idx].domain;
	rc = iommu_map(domain,
		qdss_start,
		qdss_phy_addr,
		qdss_len,
		IOMMU_READ|IOMMU_WRITE);

	if (rc) {
		CAM_ERR(CAM_SMMU, "Failed to map QDSS into IOMMU");
		goto unlock_and_end;
	}

	iommu_cb_set.cb_info[idx].is_qdss_allocated = true;

	*iova = iommu_cb_set.cb_info[idx].qdss_info.iova_start;
	*len = qdss_len;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	return rc;

unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_alloc_qdss);

int cam_smmu_dealloc_qdss(int32_t smmu_hdl)
{
	int rc = 0;
	int32_t idx;
	size_t qdss_len = 0;
	size_t qdss_start = 0;
	struct iommu_domain *domain = NULL;
	size_t unmapped = 0;

	if (smmu_hdl == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].qdss_support) {
		CAM_ERR(CAM_SMMU,
			"QDSS memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (!iommu_cb_set.cb_info[idx].is_qdss_allocated) {
		CAM_ERR(CAM_SMMU,
			"Trying to deallocate qdss that is not allocated");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	qdss_len = iommu_cb_set.cb_info[idx].qdss_info.iova_len;
	qdss_start = iommu_cb_set.cb_info[idx].qdss_info.iova_start;
	domain = iommu_cb_set.cb_info[idx].domain;
	unmapped = iommu_unmap(domain, qdss_start, qdss_len);

	if (unmapped != qdss_len) {
		CAM_ERR(CAM_SMMU, "Only %zu unmapped out of total %zu",
			unmapped,
			qdss_len);
		rc = -EINVAL;
	}

	iommu_cb_set.cb_info[idx].is_qdss_allocated = false;

unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_dealloc_qdss);

int cam_smmu_get_io_region_info(int32_t smmu_hdl,
	dma_addr_t *iova, size_t *len)
{
	int32_t idx;

	if (!iova || !len || (smmu_hdl == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: Input args are invalid");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		return -EINVAL;
	}

	if (!iommu_cb_set.cb_info[idx].io_support) {
		CAM_ERR(CAM_SMMU,
			"I/O memory not supported for this SMMU handle");
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	*iova = iommu_cb_set.cb_info[idx].io_info.iova_start;
	*len = iommu_cb_set.cb_info[idx].io_info.iova_len;

	CAM_DBG(CAM_SMMU,
		"I/O area for hdl = %x start addr = %llx len = %zu",
		smmu_hdl, *iova, *len);
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	return 0;
}

int cam_smmu_get_region_info(int32_t smmu_hdl,
	enum cam_smmu_region_id region_id,
	struct cam_smmu_region_info *region_info)
{
	int32_t idx;
	struct cam_context_bank_info *cb = NULL;

	if (!region_info) {
		CAM_ERR(CAM_SMMU, "Invalid region_info pointer");
		return -EINVAL;
	}

	if (smmu_hdl == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "Handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	cb = &iommu_cb_set.cb_info[idx];
	if (!cb) {
		CAM_ERR(CAM_SMMU, "SMMU context bank pointer invalid");
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	switch (region_id) {
	case CAM_SMMU_REGION_FIRMWARE:
		if (!cb->firmware_support) {
			CAM_ERR(CAM_SMMU, "Firmware not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->firmware_info.iova_start;
		region_info->iova_len = cb->firmware_info.iova_len;
		break;
	case CAM_SMMU_REGION_SHARED:
		if (!cb->shared_support) {
			CAM_ERR(CAM_SMMU, "Shared mem not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->shared_info.iova_start;
		region_info->iova_len = cb->shared_info.iova_len;
		break;
	case CAM_SMMU_REGION_IO:
		if (!cb->io_support) {
			CAM_ERR(CAM_SMMU, "IO memory not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->io_info.iova_start;
		region_info->iova_len = cb->io_info.iova_len;
		break;
	case CAM_SMMU_REGION_SECHEAP:
		if (!cb->secheap_support) {
			CAM_ERR(CAM_SMMU, "Secondary heap not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->secheap_info.iova_start;
		region_info->iova_len = cb->secheap_info.iova_len;
		break;
	default:
		CAM_ERR(CAM_SMMU, "Invalid region id: %d for smmu hdl: %X",
			smmu_hdl, region_id);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_get_region_info);

static int cam_smmu_map_to_pool(struct iommu_domain *domain,
				struct gen_pool *pool, struct sg_table *table,
				dma_addr_t *iova, size_t *size)
{
	struct scatterlist *sg;
	size_t buffer_size;
	size_t mapped_size;
	int i;

	if (!pool)
		return -EINVAL;

	buffer_size = 0;
	for_each_sg(table->sgl, sg, table->orig_nents, i)
		buffer_size += sg->length;

	/*
	 * The real buffer size is less than it was requested by the caller.
	 * Handle this gracefully by allocating more IOVA space. This will
	 * prevent out of bounds access, which instead will result in IOMMU
	 * page faults.
	 */
	if (WARN_ON_ONCE(buffer_size < *size))
		buffer_size = *size;

	/*
	 * iommu_map_sgtable() will map the entire sgtable, even if
	 * the assumed buffer size is smaller. Use the real buffer size
	 * for IOVA space allocation to take care of this.
	 */
	WARN_ON_ONCE(buffer_size > *size);

	*iova = gen_pool_alloc(pool, buffer_size);
	if (!*iova)
		return -ENOMEM;

	mapped_size = iommu_map_sgtable(domain, *iova, table,
					IOMMU_READ | IOMMU_WRITE);
	if (mapped_size < *size) {
		CAM_ERR(CAM_SMMU, "iommu_map_sgtable() failed");
		goto err_free_iova;
	}
	*size = mapped_size;

	return 0;

err_free_iova:
	gen_pool_free(pool, *iova, buffer_size);

	return -ENOMEM;
}

static void cam_smmu_unmap_from_pool(struct iommu_domain *domain,
				     struct gen_pool *pool,
				     dma_addr_t iova, size_t size)
{
	if (!pool)
		return;

	iommu_unmap(domain, iova, size);
	gen_pool_free(pool, iova, size);
}

static struct gen_pool *
cam_smmu_get_pool_for_region(struct cam_context_bank_info *cb,
			     enum cam_smmu_region_id region_id)
{
	switch (region_id) {
	case CAM_SMMU_REGION_SHARED:
		return cb->shared_mem_pool;
	case CAM_SMMU_REGION_IO:
		return cb->io_mem_pool;
	default:
		return NULL;
	}
}

static int cam_smmu_map_buffer_validate(struct dma_buf *buf,
	int idx, enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr, enum cam_smmu_region_id region_id,
	struct cam_dma_buff_info **mapping_info)
{
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct gen_pool *pool;
	int rc;

	if (IS_ERR_OR_NULL(buf)) {
		rc = PTR_ERR(buf);
		CAM_ERR(CAM_SMMU, "Error: mapping_info is invalid");
		return rc;
	}

	if (!mapping_info) {
		rc = PTR_ERR(buf);
		CAM_ERR(CAM_SMMU, "Error: mapping_info is invalid");
		return rc;
	}

	attach = dma_buf_attach(buf, cb->dev->parent);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach);
		CAM_ERR(CAM_SMMU, "Error: dma buf attach failed");
		return rc;
	}

	table = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR_OR_NULL(table)) {
		rc = PTR_ERR(table);
		CAM_ERR(CAM_SMMU, "Error: dma map attachment failed");
		goto err_buf_detach;
	}

	pool = cam_smmu_get_pool_for_region(cb, region_id);
	if (!pool) {
		CAM_ERR(CAM_SMMU, "Error: Wrong region requested");
		rc = -EINVAL;
		goto err_buf_unmap_attach;
	}

	rc = cam_smmu_map_to_pool(cb->domain, pool, table, paddr_ptr, len_ptr);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU,
			"Buffer map failed, region_id=%u, size=%zu, idx=%d, handle=%d",
			region_id, *len_ptr, idx,
			iommu_cb_set.cb_info[idx].handle);
		goto err_buf_unmap_attach;
	}

	CAM_DBG(CAM_SMMU, "cam_smmu_map_to_pool returned iova=%pad, size=%zu",
		paddr_ptr, *len_ptr);

	CAM_DBG(CAM_SMMU, "region_id=%d, paddr=%llx, len=%zu",
		region_id, *paddr_ptr, *len_ptr);

	CAM_DBG(CAM_SMMU,
		"DMA buf: %pK, device: %pK, attach: %pK, table: %pK",
		(void *)buf,
		(void *)iommu_cb_set.cb_info[idx].dev,
		(void *)attach, (void *)table);
	CAM_DBG(CAM_SMMU, "table sgl: %pK, rc: %d, dma_address: 0x%x",
		(void *)table->sgl, rc,
		(unsigned int)table->sgl->dma_address);

	/* fill up mapping_info */
	*mapping_info = kzalloc(sizeof(struct cam_dma_buff_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(*mapping_info)) {
		rc = PTR_ERR(*mapping_info);
		goto err_free_iova;
	}

	(*mapping_info)->buf = buf;
	(*mapping_info)->attach = attach;
	(*mapping_info)->table = table;
	(*mapping_info)->paddr = *paddr_ptr;
	(*mapping_info)->len = *len_ptr;
	(*mapping_info)->dir = dma_dir;
	(*mapping_info)->ref_count = 1;
	(*mapping_info)->region_id = region_id;

	if (!*paddr_ptr || !*len_ptr) {
		CAM_ERR(CAM_SMMU, "Error: Space Allocation failed");
		kfree(*mapping_info);
		*mapping_info = NULL;
		rc = -ENOSPC;
		goto err_free_iova;
	}

	CAM_DBG(CAM_SMMU, "idx=%d, dma_buf=%pK, dev=%pK, paddr=%pK, len=%u",
		idx, buf, (void *)iommu_cb_set.cb_info[idx].dev,
		(void *)*paddr_ptr, (unsigned int)*len_ptr);

	return 0;

err_free_iova:
	cam_smmu_unmap_from_pool(cb->domain, pool, *len_ptr, *paddr_ptr);
err_buf_unmap_attach:
	dma_buf_unmap_attachment(attach, table, dma_dir);
err_buf_detach:
	dma_buf_detach(buf, attach);

	return rc;
}

static int cam_smmu_map_buffer_and_add_to_list(int idx, int dma_fd,
	 enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	 size_t *len_ptr, enum cam_smmu_region_id region_id)
{
	int rc = -1;
	struct cam_dma_buff_info *mapping_info = NULL;
	struct dma_buf *buf = NULL;

	/* returns the dma_buf structure related to an fd */
	buf = dma_buf_get(dma_fd);

	rc = cam_smmu_map_buffer_validate(buf, idx, dma_dir, paddr_ptr, len_ptr,
		region_id, &mapping_info);

	if (rc) {
		CAM_ERR(CAM_SMMU, "buffer validation failure");
		return rc;
	}

	mapping_info->dma_fd = dma_fd;
	/* add to the list */
	list_add(&mapping_info->list,
		&iommu_cb_set.cb_info[idx].smmu_buf_list);

	return 0;
}

static int cam_smmu_map_kernel_buffer_and_add_to_list(int idx,
	struct dma_buf *buf, enum dma_data_direction dma_dir,
	dma_addr_t *paddr_ptr, size_t *len_ptr,
	enum cam_smmu_region_id region_id)
{
	int rc = -1;
	struct cam_dma_buff_info *mapping_info = NULL;

	rc = cam_smmu_map_buffer_validate(buf, idx, dma_dir, paddr_ptr, len_ptr,
		region_id, &mapping_info);

	if (rc) {
		CAM_ERR(CAM_SMMU, "buffer validation failure");
		return rc;
	}

	mapping_info->dma_fd = -1;

	/* add to the list */
	list_add(&mapping_info->list,
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list);

	return 0;
}


static int cam_smmu_unmap_buf_and_remove_from_list(
	struct cam_dma_buff_info *mapping_info,
	int idx)
{
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];
	struct gen_pool *pool = NULL;

	if ((!mapping_info->buf) || (!mapping_info->table) ||
		(!mapping_info->attach)) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params dev = %pK, table = %pK",
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)mapping_info->table);
		CAM_ERR(CAM_SMMU, "Error:dma_buf = %pK, attach = %pK",
			(void *)mapping_info->buf,
			(void *)mapping_info->attach);
		return -EINVAL;
	}

	CAM_DBG(CAM_SMMU, "Removing SHARED buffer paddr = %pK, len = %zu",
		(void *)mapping_info->paddr, mapping_info->len);

	pool = cam_smmu_get_pool_for_region(cb, mapping_info->region_id);
	cam_smmu_unmap_from_pool(cb->domain, pool,
				 mapping_info->paddr,
				 mapping_info->len);

	dma_buf_unmap_attachment(mapping_info->attach,
		mapping_info->table, mapping_info->dir);
	dma_buf_detach(mapping_info->buf, mapping_info->attach);

	if (mapping_info->dma_fd)
		dma_buf_put(mapping_info->buf);

	mapping_info->buf = NULL;

	list_del_init(&mapping_info->list);

	/* free one buffer */
	kfree(mapping_info);
	return 0;
}

static enum cam_smmu_buf_state cam_smmu_check_fd_in_list(int idx,
	int dma_fd, dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		if (mapping->dma_fd == dma_fd) {
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			return CAM_SMMU_BUFF_EXIST;
		}
	}

	return CAM_SMMU_BUFF_NOT_EXIST;
}

static enum cam_smmu_buf_state cam_smmu_check_dma_buf_in_list(int idx,
	struct dma_buf *buf, dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list, list) {
		if (mapping->buf == buf) {
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			return CAM_SMMU_BUFF_EXIST;
		}
	}

	return CAM_SMMU_BUFF_NOT_EXIST;
}

int cam_smmu_get_handle(char *identifier, int *handle_ptr)
{
	int ret = 0;

	if (!identifier) {
		CAM_ERR(CAM_SMMU, "Error: iommu hardware name is NULL");
		return -EINVAL;
	}

	if (!handle_ptr) {
		CAM_ERR(CAM_SMMU, "Error: handle pointer is NULL");
		return -EINVAL;
	}

	/* create and put handle in the table */
	ret = cam_smmu_create_add_handle_in_table(identifier, handle_ptr);
	if (ret < 0)
		CAM_ERR(CAM_SMMU, "Error: %s get handle fail", identifier);

	return ret;
}
EXPORT_SYMBOL(cam_smmu_get_handle);

int cam_smmu_ops(int handle, enum cam_smmu_ops_param ops)
{
	int ret = 0, idx;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "Error: Index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	switch (ops) {
	case CAM_SMMU_ATTACH: {
		ret = cam_smmu_attach(idx);
		break;
	}
	case CAM_SMMU_DETACH: {
		ret = cam_smmu_detach_device(idx);
		break;
	}
	case CAM_SMMU_VOTE:
	case CAM_SMMU_DEVOTE:
	default:
		CAM_ERR(CAM_SMMU, "Error: idx = %d, ops = %d", idx, ops);
		ret = -EINVAL;
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return ret;
}
EXPORT_SYMBOL(cam_smmu_ops);

static int cam_smmu_map_iova_validate_params(int handle,
	enum dma_data_direction dir,
	dma_addr_t *paddr_ptr, size_t *len_ptr,
	enum cam_smmu_region_id region_id)
{
	int idx, rc = 0;

	if (!paddr_ptr || !len_ptr) {
		CAM_ERR(CAM_SMMU, "Input pointers are invalid");
		return -EINVAL;
	}

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Invalid handle");
		return -EINVAL;
	}

	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	if (region_id != CAM_SMMU_REGION_SHARED)
		*len_ptr = (size_t)0;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	return rc;
}

int cam_smmu_map_user_iova(int handle, int dma_fd, enum dma_data_direction dir,
			   dma_addr_t *paddr_ptr, size_t *len_ptr,
			   enum cam_smmu_region_id region_id)
{
	int idx, rc = 0;
	enum cam_smmu_buf_state buf_state;
	enum dma_data_direction dma_dir;

	rc = cam_smmu_map_iova_validate_params(handle, dir, paddr_ptr,
		len_ptr, region_id);
	if (rc) {
		CAM_ERR(CAM_SMMU, "initial checks failed, unable to proceed");
		return rc;
	}

	dma_dir = (enum dma_data_direction)dir;
	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"hdl is not valid, idx=%d, table_hdl = %x, hdl = %x",
			idx, iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		CAM_ERR(CAM_SMMU,
			"Err:Dev %s should call SMMU attach before map buffer",
			iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_fd_in_list(idx, dma_fd, paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CAM_ERR(CAM_SMMU,
			"fd:%d already in list idx:%d, handle=%d, give same addr back",
			dma_fd, idx, handle);
		rc = -EALREADY;
		goto get_addr_end;
	}

	rc = cam_smmu_map_buffer_and_add_to_list(idx, dma_fd, dma_dir,
						 paddr_ptr, len_ptr, region_id);
	if (rc < 0)
		CAM_ERR(CAM_SMMU,
			"mapping or add list fail, idx=%d, fd=%d, region=%d, rc=%d",
			idx, dma_fd, region_id, rc);

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_map_user_iova);

int cam_smmu_map_kernel_iova(int handle, struct dma_buf *buf,
	enum dma_data_direction dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr, enum cam_smmu_region_id region_id)
{
	int idx, rc = 0;
	enum cam_smmu_buf_state buf_state;

	rc = cam_smmu_map_iova_validate_params(handle, dir, paddr_ptr,
		len_ptr, region_id);
	if (rc) {
		CAM_ERR(CAM_SMMU, "initial checks failed, unable to proceed");
		return rc;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU, "hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		CAM_ERR(CAM_SMMU,
			"Err:Dev %s should call SMMU attach before map buffer",
			iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_dma_buf_in_list(idx, buf,
	paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CAM_ERR(CAM_SMMU,
			"dma_buf :%pK already in the list", buf);
		rc = -EALREADY;
		goto get_addr_end;
	}

	rc = cam_smmu_map_kernel_buffer_and_add_to_list(idx, buf, dir,
							paddr_ptr, len_ptr,
							region_id);
	if (rc < 0)
		CAM_ERR(CAM_SMMU, "mapping or add list fail");

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_map_kernel_iova);

int cam_smmu_get_iova(int handle, int dma_fd, dma_addr_t *paddr_ptr,
		      size_t *len_ptr)
{
	int idx, rc = 0;
	enum cam_smmu_buf_state buf_state;

	if (!paddr_ptr || !len_ptr) {
		CAM_ERR(CAM_SMMU, "Error: Input pointers are invalid");
		return -EINVAL;
	}

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_fd_in_list(idx, dma_fd, paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_NOT_EXIST) {
		CAM_ERR(CAM_SMMU, "dma_fd:%d not in the mapped list", dma_fd);
		rc = -EINVAL;
		goto get_addr_end;
	}

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_get_iova);

static int cam_smmu_unmap_validate_params(int handle)
{
	int idx;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	return 0;
}

int cam_smmu_unmap_user_iova(int handle,
	int dma_fd, enum cam_smmu_region_id region_id)
{
	int idx, rc;
	struct cam_dma_buff_info *mapping_info;

	rc = cam_smmu_unmap_validate_params(handle);
	if (rc) {
		CAM_ERR(CAM_SMMU, "unmap util validation failure");
		return rc;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Based on dma_fd & index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_ion_index(idx, dma_fd);

	if (!mapping_info) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params idx = %d, fd = %d",
			idx, dma_fd);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Unmapping one buffer from device */
	CAM_DBG(CAM_SMMU, "SMMU: removing buffer idx = %d", idx);
	rc = cam_smmu_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (rc < 0)
		CAM_ERR(CAM_SMMU, "Error: unmap or remove list fail");

unmap_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_unmap_user_iova);

int cam_smmu_unmap_kernel_iova(int handle,
	struct dma_buf *buf, enum cam_smmu_region_id region_id)
{
	int idx, rc;
	struct cam_dma_buff_info *mapping_info;

	rc = cam_smmu_unmap_validate_params(handle);
	if (rc) {
		CAM_ERR(CAM_SMMU, "unmap util validation failure");
		return rc;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Based on dma_buf & index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_dma_buf(idx, buf);

	if (!mapping_info) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params idx = %d, dma_buf = %pK",
			idx, buf);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Unmapping one buffer from device */
	CAM_DBG(CAM_SMMU, "SMMU: removing buffer idx = %d", idx);
	rc = cam_smmu_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (rc < 0)
		CAM_ERR(CAM_SMMU, "Error: unmap or remove list fail");

unmap_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_unmap_kernel_iova);


int cam_smmu_put_iova(int handle, int dma_fd)
{
	int idx;
	int rc = 0;
	struct cam_dma_buff_info *mapping_info;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto put_addr_end;
	}

	/* based on fd and index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_ion_index(idx, dma_fd);
	if (!mapping_info) {
		CAM_ERR(CAM_SMMU, "Error: Invalid params idx = %d, fd = %d",
			idx, dma_fd);
		rc = -EINVAL;
		goto put_addr_end;
	}

put_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_put_iova);

int cam_smmu_destroy_handle(int handle)
{
	int idx;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	if (!list_empty_careful(&iommu_cb_set.cb_info[idx].smmu_buf_list)) {
		CAM_ERR(CAM_SMMU, "UMD %s buffer list is not clean",
			iommu_cb_set.cb_info[idx].name);
		cam_smmu_print_user_list(idx);
		cam_smmu_clean_user_buffer_list(idx);
	}

	if (!list_empty_careful(
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list)) {
		CAM_ERR(CAM_SMMU, "KMD %s buffer list is not clean",
			iommu_cb_set.cb_info[idx].name);
		cam_smmu_print_kernel_list(idx);
		cam_smmu_clean_kernel_buffer_list(idx);
	}

	iommu_cb_set.cb_info[idx].cb_count = 0;
	iommu_cb_set.cb_info[idx].handle = HANDLE_INIT;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_destroy_handle);

static void cam_smmu_deinit_cb(struct cam_context_bank_info *cb)
{
	if (cb->shared_support) {
		gen_pool_destroy(cb->shared_mem_pool);
		cb->shared_mem_pool = NULL;
	}

	if (cb->io_support) {
		gen_pool_destroy(cb->io_mem_pool);
		iommu_cb_set.non_fatal_fault = false;
	}

	WARN_ON(cb->state == CAM_SMMU_ATTACH);
	iommu_domain_free(cb->domain);
}

static void cam_smmu_release_cb(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < iommu_cb_set.cb_num; i++)
		cam_smmu_deinit_cb(&iommu_cb_set.cb_info[i]);

	devm_kfree(&pdev->dev, iommu_cb_set.cb_info);
	iommu_cb_set.cb_num = 0;
}

static struct gen_pool *
cam_smmu_pool_create(size_t iova_start, size_t iova_len, int granularity)
{
	struct gen_pool *pool;
	int rc;

	pool = gen_pool_create(granularity, -1);
	if (!pool)
		return ERR_PTR(-ENOMEM);

	rc = gen_pool_add(pool, iova_start, iova_len, -1);
	if (rc) {
		CAM_ERR(CAM_SMMU, "Genpool chunk creation failed");
		gen_pool_destroy(pool);
		return ERR_PTR(rc);
	}

	return pool;
}

static int cam_smmu_setup_cb(struct cam_context_bank_info *cb,
	struct device *dev)
{
	int rc = 0;

	if (!cb || !dev) {
		CAM_ERR(CAM_SMMU, "Error: invalid input params");
		return -EINVAL;
	}

	cb->dev = dev;
	cb->is_fw_allocated = false;
	cb->is_secheap_allocated = false;

	/* Create a pool with 64K granularity for supporting shared memory */
	if (cb->shared_support) {
		cb->shared_mem_pool =
			cam_smmu_pool_create(cb->shared_info.iova_start,
					     cb->shared_info.iova_len,
					     SHARED_MEM_POOL_GRANULARITY);
		if (IS_ERR(cb->shared_mem_pool)) {
			CAM_ERR(CAM_SMMU, "Shared memory pool creation failed");
			return PTR_ERR(cb->shared_mem_pool);
		}

		CAM_DBG(CAM_SMMU, "Shared mem start->%lX",
			(unsigned long)cb->shared_info.iova_start);
		CAM_DBG(CAM_SMMU, "Shared mem len->%zu",
			cb->shared_info.iova_len);
	}

	/* create a virtual mapping */
	if (cb->io_support) {
		cb->io_mem_pool =
			cam_smmu_pool_create(cb->io_info.iova_start,
					     cb->io_info.iova_len,
					     IO_MEM_POOL_GRANULARITY);
		if (IS_ERR(cb->io_mem_pool)) {
			CAM_ERR(CAM_SMMU, "IO memory pool creation failed");
			rc = PTR_ERR(cb->io_mem_pool);
			goto err_free_shared;
		}

		CAM_DBG(CAM_SMMU, "IO mem start->%lX",
			(unsigned long)cb->io_info.iova_start);
		CAM_DBG(CAM_SMMU, "IO mem len->%zu",
			cb->io_info.iova_len);
		iommu_cb_set.non_fatal_fault = smmu_fatal_flag;
	} else {
		CAM_ERR(CAM_SMMU, "Context bank does not have IO region");
		rc = -ENODEV;
		goto err_free_shared;
	}

	return 0;

err_free_shared:
	if (cb->shared_support) {
		gen_pool_destroy(cb->shared_mem_pool);
		cb->shared_mem_pool = NULL;
	}

	return rc;
}

static int cam_alloc_smmu_context_banks(struct device *dev)
{
	struct device_node *domains_child_node = NULL;

	if (!dev) {
		CAM_ERR(CAM_SMMU, "Error: Invalid device");
		return -ENODEV;
	}

	iommu_cb_set.cb_num = 0;

	/* traverse thru all the child nodes and increment the cb count */
	for_each_available_child_of_node(dev->of_node, domains_child_node) {
		if (of_device_is_compatible(domains_child_node,
			"qcom,msm-cam-smmu-cb"))
			iommu_cb_set.cb_num++;

		if (of_device_is_compatible(domains_child_node,
			"qcom,qsmmu-cam-cb"))
			iommu_cb_set.cb_num++;
	}

	if (iommu_cb_set.cb_num == 0) {
		CAM_ERR(CAM_SMMU, "Error: no context banks present");
		return -ENOENT;
	}

	/* allocate memory for the context banks */
	iommu_cb_set.cb_info = devm_kzalloc(dev,
		iommu_cb_set.cb_num * sizeof(struct cam_context_bank_info),
		GFP_KERNEL);

	if (!iommu_cb_set.cb_info) {
		CAM_ERR(CAM_SMMU, "Error: cannot allocate context banks");
		return -ENOMEM;
	}

	cam_smmu_reset_iommu_table(CAM_SMMU_TABLE_INIT);
	iommu_cb_set.cb_init_count = 0;
	dma_set_mask(dev, DMA_BIT_MASK(64));

	CAM_DBG(CAM_SMMU, "no of context banks :%d", iommu_cb_set.cb_num);
	return 0;
}

static int cam_smmu_get_memory_regions_info(struct device_node *of_node,
	struct cam_context_bank_info *cb)
{
	int rc = 0;
	struct device_node *mem_map_node = NULL;
	struct device_node *child_node = NULL;
	const char *region_name;
	int num_regions = 0;

	if (!of_node || !cb) {
		CAM_ERR(CAM_SMMU, "Invalid argument(s)");
		return -EINVAL;
	}

	mem_map_node = of_get_child_by_name(of_node, "iova-mem-map");
	if (!mem_map_node) {
		CAM_ERR(CAM_SMMU, "iova-mem-map not present");
		return -EINVAL;
	}

	for_each_available_child_of_node(mem_map_node, child_node) {
		uint32_t region_start;
		uint32_t region_len;
		uint32_t region_id;
		uint32_t qdss_region_phy_addr = 0;

		num_regions++;
		rc = of_property_read_string(child_node,
			"iova-region-name", &region_name);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "IOVA region not found");
			return -EINVAL;
		}

		rc = of_property_read_u32(child_node,
			"iova-region-start", &region_start);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "Failed to read iova-region-start");
			return -EINVAL;
		}

		rc = of_property_read_u32(child_node,
			"iova-region-len", &region_len);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "Failed to read iova-region-len");
			return -EINVAL;
		}

		rc = of_property_read_u32(child_node,
			"iova-region-id", &region_id);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "Failed to read iova-region-id");
			return -EINVAL;
		}

		if (strcmp(region_name, qdss_region_name) == 0) {
			rc = of_property_read_u32(child_node,
				"qdss-phy-addr", &qdss_region_phy_addr);
			if (rc < 0) {
				of_node_put(mem_map_node);
				CAM_ERR(CAM_SMMU,
					"Failed to read qdss phy addr");
				return -EINVAL;
			}
		}

		switch (region_id) {
		case CAM_SMMU_REGION_FIRMWARE:
			cb->firmware_support = 1;
			cb->firmware_info.iova_start = region_start;
			cb->firmware_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_SHARED:
			cb->shared_support = 1;
			cb->shared_info.iova_start = region_start;
			cb->shared_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_IO:
			cb->io_support = 1;
			cb->io_info.iova_start = region_start;
			cb->io_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_SECHEAP:
			cb->secheap_support = 1;
			cb->secheap_info.iova_start = region_start;
			cb->secheap_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_QDSS:
			cb->qdss_support = 1;
			cb->qdss_info.iova_start = region_start;
			cb->qdss_info.iova_len = region_len;
			cb->qdss_phy_addr = qdss_region_phy_addr;
			break;
		default:
			CAM_ERR(CAM_SMMU,
				"Incorrect region id present in DT file: %d",
				region_id);
		}

		CAM_DBG(CAM_SMMU, "Found label -> %s", cb->name);
		CAM_DBG(CAM_SMMU, "Found region -> %s", region_name);
		CAM_DBG(CAM_SMMU, "region_start -> %X", region_start);
		CAM_DBG(CAM_SMMU, "region_len -> %X", region_len);
		CAM_DBG(CAM_SMMU, "region_id -> %X", region_id);
	}
	of_node_put(mem_map_node);

	if (!num_regions) {
		CAM_ERR(CAM_SMMU,
			"No memory regions found, at least one needed");
		rc = -ENODEV;
	}

	return rc;
}

static int cam_populate_smmu_context_banks(struct device *dev,
	enum cam_iommu_type type)
{
	int rc = 0;
	struct cam_context_bank_info *cb;
	struct device *ctx = NULL;

	if (!dev) {
		CAM_ERR(CAM_SMMU, "Error: Invalid device");
		return -ENODEV;
	}

	/* check the bounds */
	if (iommu_cb_set.cb_init_count >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "Error: populate more than allocated cb");
		rc = -EBADHANDLE;
		goto cb_init_fail;
	}

	/* read the context bank from cb set */
	cb = &iommu_cb_set.cb_info[iommu_cb_set.cb_init_count];

	/* set the name of the context bank */
	rc = of_property_read_string(dev->of_node, "label", &cb->name);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU,
			"Error: failed to read label from sub device");
		goto cb_init_fail;
	}

	rc = cam_smmu_get_memory_regions_info(dev->of_node,
		cb);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: Getting region info");
		return rc;
	}

	/* set up the iommu mapping for the  context bank */
	if (type == CAM_QSMMU) {
		CAM_ERR(CAM_SMMU, "Error: QSMMU ctx not supported for : %s",
			cb->name);
		return -ENODEV;
	}

	ctx = dev;
	CAM_DBG(CAM_SMMU, "getting Arm SMMU ctx : %s", cb->name);

	rc = cam_smmu_setup_cb(cb, ctx);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: failed to setup cb : %s", cb->name);
		goto cb_init_fail;
	}

	cb->domain = iommu_domain_alloc(&platform_bus_type);
	if (IS_ERR_OR_NULL(cb->domain)) {
		CAM_ERR(CAM_SMMU, "Invalid domain for device");
		return PTR_ERR(cb->domain);
	}

	iommu_set_fault_handler(cb->domain, cam_smmu_iommu_fault_handler,
				(void *)cb->name);

	/* increment count to next bank */
	iommu_cb_set.cb_init_count++;

	CAM_DBG(CAM_SMMU, "X: cb init count :%d", iommu_cb_set.cb_init_count);

cb_init_fail:
	return rc;
}

static int cam_smmu_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device *dev = &pdev->dev;

	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu")) {
		rc = cam_alloc_smmu_context_banks(dev);
		if (rc < 0) {
			CAM_ERR(CAM_SMMU, "Error: allocating context banks");
			return -ENOMEM;
		}
	}
	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_ARM_SMMU);
		if (rc < 0) {
			CAM_ERR(CAM_SMMU, "Error: populating context banks");
			cam_smmu_release_cb(pdev);
			return -ENOMEM;
		}
		return rc;
	}
	if (of_device_is_compatible(dev->of_node, "qcom,qsmmu-cam-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_QSMMU);
		if (rc < 0) {
			CAM_ERR(CAM_SMMU, "Error: populating context banks");
			return -ENOMEM;
		}
		return rc;
	}

	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu-fw-dev")) {
		icp_fw.fw_dev = &pdev->dev;
		icp_fw.fw_kva = NULL;
		icp_fw.pages = NULL;
		icp_fw.num_pages = 0;
		dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
		return rc;
	}

	/* probe through all the subdevices */
	rc = of_platform_populate(pdev->dev.of_node, msm_cam_smmu_dt_match,
				NULL, &pdev->dev);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: populating devices");
	} else {
		INIT_WORK(&iommu_cb_set.smmu_work, cam_smmu_page_fault_work);
		mutex_init(&iommu_cb_set.payload_list_lock);
		INIT_LIST_HEAD(&iommu_cb_set.payload_list);
	}

	pr_info("%s driver probed successfully\n", KBUILD_MODNAME);

	return rc;
}

static int cam_smmu_remove(struct platform_device *pdev)
{
	/* release all the context banks and memory allocated */
	cam_smmu_reset_iommu_table(CAM_SMMU_TABLE_DEINIT);
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-cam-smmu"))
		cam_smmu_release_cb(pdev);
	return 0;
}

static struct platform_driver cam_smmu_driver = {
	.probe = cam_smmu_probe,
	.remove = cam_smmu_remove,
	.driver = {
		.name = "msm_cam_smmu",
		.owner = THIS_MODULE,
		.of_match_table = msm_cam_smmu_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_smmu_init_module(void)
{
	cam_smmu_create_debugfs_entry();
	return platform_driver_register(&cam_smmu_driver);
}

static void __exit cam_smmu_exit_module(void)
{
	platform_driver_unregister(&cam_smmu_driver);
}

module_init(cam_smmu_init_module);
module_exit(cam_smmu_exit_module);
MODULE_DESCRIPTION("MSM Camera SMMU driver");
MODULE_LICENSE("GPL v2");
