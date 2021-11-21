// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#include <uapi/linux/dma-buf.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/dma-buf.h>

#include "cam_buf_mgr.h"
#include "cam_req_mgr_util.h"
#include "cam_mem_mgr.h"
#include "cam_smmu_api.h"
#include "cam_debug_util.h"

static struct cam_mem_table tbl;

static int cam_mem_util_get_dma_dir(uint32_t flags)
{
	int rc = -EINVAL;

	if (flags & CAM_MEM_FLAG_HW_READ_ONLY)
		rc = DMA_TO_DEVICE;
	else if (flags & CAM_MEM_FLAG_HW_WRITE_ONLY)
		rc = DMA_FROM_DEVICE;
	else if (flags & CAM_MEM_FLAG_HW_READ_WRITE)
		rc = DMA_BIDIRECTIONAL;
	else if (flags & CAM_MEM_FLAG_PROTECTED_MODE)
		rc = DMA_BIDIRECTIONAL;

	return rc;
}

static int cam_mem_util_map_cpu_va(struct dma_buf *dmabuf,
	uintptr_t *vaddr,
	size_t *len)
{
	struct dma_buf_map map;
	void *addr;
	int rc;

	/*
	 * dma_buf_begin_cpu_access() and dma_buf_end_cpu_access()
	 * need to be called in pair to avoid stability issue.
	 */
	rc = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (rc) {
		CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);
		return rc;
	}

	rc = dma_buf_vmap(dmabuf, &map);
	if (rc) {
		CAM_ERR(CAM_MEM, "Mapping failed");
		goto err_end_cpu_access;
	}
	addr = map.vaddr;

	*vaddr = (uintptr_t)addr;
	*len = dmabuf->size;

	return 0;

err_end_cpu_access:
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	return rc;
}

static int cam_mem_util_unmap_cpu_va(struct dma_buf *dmabuf,
	uint64_t vaddr)
{
	int rc;
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR((void *)vaddr);

	dma_buf_vunmap(dmabuf, &map);

	/*
	 * dma_buf_begin_cpu_access() and
	 * dma_buf_end_cpu_access() need to be called in pair
	 * to avoid stability issue.
	 */
	rc = dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (rc) {
		CAM_ERR(CAM_MEM, "Failed in end cpu access, dmabuf=%pK",
			dmabuf);
		return rc;
	}

	return rc;
}

static int32_t cam_mem_get_slot(struct cam_mem_table *table)
{
	int idx;

	mutex_lock(&table->m_lock);
	idx = find_first_zero_bit(table->bitmap, table->bits);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Cannot get zero bit, idx:%d", idx);
		idx = -ENOMEM;
		goto err_tbl_unlock;
	}

	set_bit(idx, table->bitmap);
	table->bufq[idx].active = true;
	mutex_unlock(&table->m_lock);

	return idx;

err_tbl_unlock:
	mutex_unlock(&table->m_lock);
	return idx;
}

static void cam_mem_put_slot(struct cam_mem_table *table, int idx)
{
	mutex_lock(&table->m_lock);
	clear_bit(idx, table->bitmap);
	table->bufq[idx].active = false;
	mutex_unlock(&table->m_lock);
}

int cam_mem_get_io_buf(int32_t buf_handle, int32_t mmu_handle,
	uint64_t *iova_ptr, size_t *len_ptr)
{
	int rc = 0, idx;

	*len_ptr = 0;

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.m_lock);
	if (!tbl.bufq[idx].active) {
		rc = -EINVAL;
		goto handle_mismatch;
	}

	if (buf_handle != tbl.bufq[idx].buf_handle) {
		rc = -EINVAL;
		goto handle_mismatch;
	}

	rc = cam_smmu_get_iova(mmu_handle,
		tbl.bufq[idx].fd,
		iova_ptr,
		len_ptr);
	if (rc) {
		CAM_ERR(CAM_MEM,
			"fail to map buf_hdl:0x%x, mmu_hdl: 0x%x for fd:%d",
			buf_handle, mmu_handle, tbl.bufq[idx].fd);
		goto handle_mismatch;
	}

	CAM_DBG(CAM_MEM,
		"handle:0x%x fd:%d iova_ptr:%pK len_ptr:%zu",
		mmu_handle, tbl.bufq[idx].fd, iova_ptr, *len_ptr);
handle_mismatch:
	mutex_unlock(&tbl.m_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_get_io_buf);

int cam_mem_get_cpu_buf(int32_t buf_handle, uintptr_t *vaddr_ptr, size_t *len)
{
	int rc = 0;
	int idx;

	if (!buf_handle || !vaddr_ptr || !len)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.m_lock);
	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "idx: %d not active", idx);
		rc = -EPERM;
		goto end;
	}

	if (buf_handle != tbl.bufq[idx].buf_handle) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid buf handle %d",
				idx, buf_handle);
		rc = -EINVAL;
		goto end;
	}

	if (!(tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS)) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid flag 0x%x",
					idx, tbl.bufq[idx].flags);
		rc = -EINVAL;
		goto end;
	}

	/* Sync from the device to main memory */
	rc = dma_buf_begin_cpu_access(tbl.bufq[idx].dma_buf, DMA_FROM_DEVICE);
	if (rc)
		CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);

	*vaddr_ptr = tbl.bufq[idx].kmdvaddr;
	*len = tbl.bufq[idx].len;

	rc = dma_buf_end_cpu_access(tbl.bufq[idx].dma_buf, DMA_FROM_DEVICE);
	if (rc)
		CAM_ERR(CAM_MEM, "dma end access failed rc=%d", rc);
end:
	mutex_unlock(&tbl.m_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_get_cpu_buf);

int cam_mem_put_cpu_buf(int32_t buf_handle)
{
	int rc = 0;
	int idx;

	if (!buf_handle)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.m_lock);
	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "idx: %d not active", idx);
		rc = -EPERM;
		goto end;
	}

	if (buf_handle != tbl.bufq[idx].buf_handle) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid buf handle %d",
				idx, buf_handle);
		rc = -EINVAL;
		goto end;
	}

	/* Sync from main memory to the device */
	rc = dma_buf_begin_cpu_access(tbl.bufq[idx].dma_buf, DMA_TO_DEVICE);
	if (rc)
		CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);

	rc = dma_buf_end_cpu_access(tbl.bufq[idx].dma_buf, DMA_TO_DEVICE);
	if (rc)
		CAM_ERR(CAM_MEM, "dma end access failed rc=%d", rc);
end:
	mutex_unlock(&tbl.m_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_put_cpu_buf);

static int cam_mem_util_get_dma_buf_fd(size_t len,
	size_t align,
	unsigned int flags,
	struct dma_buf **buf,
	int *fd)
{
	struct dma_buf *dmabuf = NULL;
	int rc = 0;

	if (!buf || !fd) {
		CAM_ERR(CAM_MEM, "Invalid params, buf=%pK, fd=%pK", buf, fd);
		return -EINVAL;
	}

	*buf = cmm_alloc_buffer(len, flags);
	if (IS_ERR_OR_NULL(*buf))
		return -ENOMEM;

	*fd = dma_buf_fd(*buf, O_CLOEXEC);
	if (*fd < 0) {
		CAM_ERR(CAM_MEM, "get fd fail, *fd=%d", *fd);
		rc = -EINVAL;
		goto get_fd_fail;
	}

	/*
	 * increment the ref count so that ref count becomes 2 here
	 * when we close fd, refcount becomes 1 and when we do
	 * dmap_put_buf, ref count becomes 0 and memory will be freed.
	 */
	dmabuf = dma_buf_get(*fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		CAM_ERR(CAM_MEM, "dma_buf_get failed, *fd=%d", *fd);
		rc = -EINVAL;
	}

	return rc;

get_fd_fail:
	cmm_free_buffer(*buf);
	return rc;
}

static int cam_mem_util_check_alloc_flags(struct cam_mem_mgr_alloc_cmd *cmd)
{
	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl exceeded maximum(%d)",
			CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		CAM_ERR(CAM_MEM, "Mapping in secure mode not allowed");
		return -EINVAL;
	}

	return 0;
}

static int cam_mem_util_check_map_flags(struct cam_mem_mgr_map_cmd *cmd)
{
	if (!cmd->flags) {
		CAM_ERR(CAM_MEM, "Invalid flags");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl %d exceeded maximum(%d)",
			cmd->num_hdl, CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		CAM_ERR(CAM_MEM,
			"Mapping in secure mode not allowed, flags=0x%x",
			cmd->flags);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		CAM_ERR(CAM_MEM,
			"Shared memory buffers are not allowed to be mapped");
		return -EINVAL;
	}

	return 0;
}

static int cam_mem_util_map_hw_va(uint32_t flags,
	int32_t *mmu_hdls,
	int32_t num_hdls,
	int fd,
	dma_addr_t *hw_vaddr,
	size_t *len,
	enum cam_smmu_region_id region)
{
	int i;
	int rc = -1;
	int dir = cam_mem_util_get_dma_dir(flags);

	if (dir < 0) {
		CAM_ERR(CAM_MEM, "fail to map DMA direction, dir=%d", dir);
		return dir;
	}

	CAM_DBG(CAM_MEM,
		"map_hw_va : fd = %d,  flags = 0x%x, dir=%d, num_hdls=%d",
		fd, flags, dir, num_hdls);

	for (i = 0; i < num_hdls; i++) {
		rc = cam_smmu_map_user_iova(mmu_hdls[i],
			fd,
			dir,
			(dma_addr_t *)hw_vaddr,
			len,
			region);

		if (rc < 0) {
			CAM_ERR(CAM_MEM,
				"Failed to map to smmu, i=%d, fd=%d, dir=%d, mmu_hdl=%d, region=%d, rc=%d",
				i, fd, dir, mmu_hdls[i], region, rc);
			goto multi_map_fail;
		}
	}

	return rc;
multi_map_fail:
	for (--i; i > 0; i--)
		cam_smmu_unmap_user_iova(mmu_hdls[i],
			fd,
			CAM_SMMU_REGION_IO);
	return rc;

}

int cam_mem_mgr_alloc_and_map(struct cam_mem_mgr_alloc_cmd *cmd)
{
	int rc;
	int32_t idx;
	struct dma_buf *dmabuf = NULL;
	int fd = -1;
	dma_addr_t hw_vaddr = 0;
	size_t len;
	uintptr_t kvaddr = 0;
	size_t klen;

	if (!cmd) {
		CAM_ERR(CAM_MEM, " Invalid argument");
		return -EINVAL;
	}
	len = cmd->len;

	rc = cam_mem_util_check_alloc_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_MEM, "Invalid flags: flags = 0x%X, rc=%d",
			cmd->flags, rc);
		return rc;
	}

	rc = cam_mem_util_get_dma_buf_fd(cmd->len, cmd->align, cmd->flags,
					 &dmabuf, &fd);
	if (rc) {
		CAM_ERR(CAM_MEM,
			"Alloc failed, len=%llu, align=%llu, flags=0x%x, num_hdl=%d",
			cmd->len, cmd->align, cmd->flags, cmd->num_hdl);
		return rc;
	}

	idx = cam_mem_get_slot(&tbl);
	if (idx < 0) {
		CAM_ERR(CAM_MEM, "Failed in getting mem slot, idx=%d", idx);
		rc = -ENOMEM;
		goto slot_fail;
	}

	if ((cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) ||
		(cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)) {

		enum cam_smmu_region_id region;

		if (cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;


		if (cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS)
			region = CAM_SMMU_REGION_SHARED;

		if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
			region = CAM_SMMU_REGION_SECHEAP;

		rc = cam_mem_util_map_hw_va(cmd->flags,
			cmd->mmu_hdls,
			cmd->num_hdl,
			fd,
			&hw_vaddr,
			&len,
			region);

		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed in map_hw_va, flags=0x%x, fd=%d, region=%d, num_hdl=%d, rc=%d",
				cmd->flags, fd, region, cmd->num_hdl, rc);
			goto map_hw_fail;
		}
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].fd = fd;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].flags = cmd->flags;
	tbl.bufq[idx].buf_handle = GET_MEM_HANDLE(idx, fd);

	if (cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		rc = cam_mem_util_map_cpu_va(dmabuf, &kvaddr, &klen);
		if (rc) {
			CAM_ERR(CAM_MEM, "dmabuf: %pK mapping failed: %d",
				dmabuf, rc);
			goto map_kernel_fail;
		}
	}

	tbl.bufq[idx].kmdvaddr = kvaddr;
	tbl.bufq[idx].vaddr = hw_vaddr;
	tbl.bufq[idx].dma_buf = dmabuf;
	tbl.bufq[idx].len = cmd->len;
	tbl.bufq[idx].num_hdl = cmd->num_hdl;
	memcpy(tbl.bufq[idx].hdls, cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);
	tbl.bufq[idx].is_imported = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.fd = tbl.bufq[idx].fd;
	cmd->out.vaddr = 0;

	CAM_DBG(CAM_MEM,
		"fd=%d, flags=0x%x, num_hdl=%d, idx=%d, buf handle=%x, len=%zu",
		cmd->out.fd, cmd->flags, cmd->num_hdl, idx, cmd->out.buf_handle,
		tbl.bufq[idx].len);

	return rc;

map_kernel_fail:
	mutex_unlock(&tbl.bufq[idx].q_lock);
map_hw_fail:
	cam_mem_put_slot(&tbl, idx);
slot_fail:
	dma_buf_put(dmabuf);
	return rc;
}

int cam_mem_mgr_map(struct cam_mem_mgr_map_cmd *cmd)
{
	int32_t idx;
	int rc;
	struct dma_buf *dmabuf;
	dma_addr_t hw_vaddr = 0;
	size_t len = 0;

	if (!cmd || (cmd->fd < 0)) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl %d exceeded maximum(%d)",
			cmd->num_hdl, CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	rc = cam_mem_util_check_map_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_MEM, "Invalid flags: flags = %X", cmd->flags);
		return rc;
	}

	dmabuf = dma_buf_get(cmd->fd);
	if (IS_ERR_OR_NULL((void *)(dmabuf))) {
		CAM_ERR(CAM_MEM, "Failed to import dma_buf fd %d, rc %ld",
			cmd->fd, (IS_ERR(dmabuf) ? PTR_ERR(dmabuf) : 0));
		return -EINVAL;
	}

	if ((cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)) {
		rc = cam_mem_util_map_hw_va(cmd->flags,
			cmd->mmu_hdls,
			cmd->num_hdl,
			cmd->fd,
			&hw_vaddr,
			&len,
			CAM_SMMU_REGION_IO);
		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed in map_hw_va, flags=0x%x, fd=%d, region=%d, num_hdl=%d, rc=%d",
				cmd->flags, cmd->fd, CAM_SMMU_REGION_IO,
				cmd->num_hdl, rc);
			goto map_fail;
		}
	}

	idx = cam_mem_get_slot(&tbl);
	if (idx < 0) {
		rc = -ENOMEM;
		goto map_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].fd = cmd->fd;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].flags = cmd->flags;
	tbl.bufq[idx].buf_handle = GET_MEM_HANDLE(idx, cmd->fd);
	tbl.bufq[idx].kmdvaddr = 0;

	if (cmd->num_hdl > 0)
		tbl.bufq[idx].vaddr = hw_vaddr;
	else
		tbl.bufq[idx].vaddr = 0;

	tbl.bufq[idx].dma_buf = dmabuf;
	tbl.bufq[idx].len = len;
	tbl.bufq[idx].num_hdl = cmd->num_hdl;
	memcpy(tbl.bufq[idx].hdls, cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);
	tbl.bufq[idx].is_imported = true;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.vaddr = 0;

	CAM_DBG(CAM_MEM,
		"fd=%d, flags=0x%x, num_hdl=%d, idx=%d, buf handle=%x, len=%zu",
		cmd->fd, cmd->flags, cmd->num_hdl, idx, cmd->out.buf_handle,
		tbl.bufq[idx].len);

	return rc;

map_fail:
	dma_buf_put(dmabuf);
	return rc;
}

static int cam_mem_util_unmap_hw_va(int32_t idx,
	enum cam_smmu_region_id region,
	enum cam_smmu_mapping_client client)
{
	int i;
	uint32_t flags;
	int32_t *mmu_hdls;
	int num_hdls;
	int fd;
	int rc = 0;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index");
		return -EINVAL;
	}

	flags = tbl.bufq[idx].flags;
	mmu_hdls = tbl.bufq[idx].hdls;
	num_hdls = tbl.bufq[idx].num_hdl;
	fd = tbl.bufq[idx].fd;

	CAM_DBG(CAM_MEM,
		"unmap_hw_va : idx=%d, fd=%x, flags=0x%x, num_hdls=%d, client=%d",
		idx, fd, flags, num_hdls, client);

	for (i = 0; i < num_hdls; i++) {
		if (client == CAM_SMMU_MAPPING_USER) {
			rc = cam_smmu_unmap_user_iova(mmu_hdls[i],
				fd, region);
		} else if (client == CAM_SMMU_MAPPING_KERNEL) {
			rc = cam_smmu_unmap_kernel_iova(mmu_hdls[i],
				tbl.bufq[idx].dma_buf, region);
		} else {
			CAM_ERR(CAM_MEM,
				"invalid caller for unmapping : %d",
				client);
			rc = -EINVAL;
		}
		if (rc < 0) {
			CAM_ERR(CAM_MEM,
				"Failed in unmap, i=%d, fd=%d, mmu_hdl=%d, region=%d, rc=%d",
				i, fd, mmu_hdls[i], region, rc);
			goto unmap_end;
		}
	}

	return rc;

unmap_end:
	CAM_ERR(CAM_MEM, "unmapping failed");
	return rc;
}

static int cam_mem_util_unmap(int32_t idx,
	enum cam_smmu_mapping_client client)
{
	int rc = 0;
	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index");
		return -EINVAL;
	}

	lockdep_assert_held(&tbl.m_lock);

	CAM_DBG(CAM_MEM, "Flags = %X idx %d", tbl.bufq[idx].flags, idx);

	if ((!tbl.bufq[idx].active) &&
		(tbl.bufq[idx].vaddr) == 0) {
		CAM_WARN(CAM_MEM, "Buffer at idx=%d is already unmapped,",
			idx);
		return 0;
	}

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS) {
		if (tbl.bufq[idx].dma_buf && tbl.bufq[idx].kmdvaddr) {
			rc = cam_mem_util_unmap_cpu_va(tbl.bufq[idx].dma_buf,
				tbl.bufq[idx].kmdvaddr);
			if (rc)
				CAM_ERR(CAM_MEM,
					"Failed, dmabuf=%pK, kmdvaddr=%lxK",
					tbl.bufq[idx].dma_buf,
					tbl.bufq[idx].kmdvaddr);
		}
	}

	/* SHARED flag gets precedence, all other flags after it */
	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		region = CAM_SMMU_REGION_SHARED;
	} else {
		if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;
	}

	if ((tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_PROTECTED_MODE)) {
		if (cam_mem_util_unmap_hw_va(idx, region, client))
			CAM_ERR(CAM_MEM, "Failed, dmabuf=%pK",
				tbl.bufq[idx].dma_buf);
		if (client == CAM_SMMU_MAPPING_KERNEL)
			tbl.bufq[idx].dma_buf = NULL;
	}

	tbl.bufq[idx].flags = 0;
	tbl.bufq[idx].buf_handle = -1;
	tbl.bufq[idx].vaddr = 0;
	memset(tbl.bufq[idx].hdls, 0,
		sizeof(int32_t) * CAM_MEM_MMU_MAX_HANDLE);

	CAM_DBG(CAM_MEM,
		"buf at idx = %d freeing fd = %d, imported %d, dma_buf %pK",
		idx, tbl.bufq[idx].fd,
		tbl.bufq[idx].is_imported,
		tbl.bufq[idx].dma_buf);

	/* Allways decrement dmabuf ref counter */
	if (tbl.bufq[idx].dma_buf)
		cmm_free_buffer(tbl.bufq[idx].dma_buf);

	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].is_imported = false;
	tbl.bufq[idx].len = 0;
	tbl.bufq[idx].num_hdl = 0;
	tbl.bufq[idx].active = false;
	tbl.bufq[idx].kmdvaddr = 0;
	clear_bit(idx, tbl.bitmap);

	return rc;
}

int cam_mem_mgr_release(struct cam_mem_mgr_release_cmd *cmd)
{
	int idx;
	int rc;

	if (!cmd) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	mutex_lock(&tbl.m_lock);
	idx = CAM_MEM_MGR_GET_HDL_IDX(cmd->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index %d extracted from mem handle",
			idx);
		rc = -EINVAL;
		goto err_mutex_unlock;
	}

	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "Released buffer state should be active");
		rc = -EINVAL;
		goto err_mutex_unlock;
	}

	if (tbl.bufq[idx].buf_handle != cmd->buf_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle %d not matching within table %d, idx=%d",
			cmd->buf_handle, tbl.bufq[idx].buf_handle, idx);
		rc = -EINVAL;
		goto err_mutex_unlock;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %x, idx = %d", cmd->buf_handle, idx);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_USER);

err_mutex_unlock:
	mutex_unlock(&tbl.m_lock);
	return rc;
}

int cam_mem_mgr_request_mem(struct cam_mem_mgr_request_desc *inp,
	struct cam_mem_mgr_memory_desc *out)
{
	struct dma_buf *buf = NULL;
	int fd = -1;
	int rc = 0;
	uintptr_t kvaddr;
	dma_addr_t iova = 0;
	size_t request_len = 0;
	uint32_t mem_handle;
	int32_t idx;
	int32_t smmu_hdl = 0;
	int32_t num_hdl = 0;

	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (!inp || !out) {
		CAM_ERR(CAM_MEM, "Invalid params");
		return -EINVAL;
	}

	if (!(inp->flags & CAM_MEM_FLAG_HW_READ_WRITE ||
		inp->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS ||
		inp->flags & CAM_MEM_FLAG_CACHE)) {
		CAM_ERR(CAM_MEM, "Invalid flags for request mem");
		return -EINVAL;
	}

	buf = cmm_alloc_buffer(inp->size, inp->flags);
	if (!buf) {
		CAM_ERR(CAM_MEM, "Alloc failed for shared buffer");
		goto get_dma_buf_fail;
	} else {
		CAM_DBG(CAM_MEM, "Got dma_buf = %pK", buf);
	}

	/*
	 * we are mapping kva always here,
	 * update flags so that we do unmap properly
	 */
	inp->flags |= CAM_MEM_FLAG_KMD_ACCESS;
	rc = cam_mem_util_map_cpu_va(buf, &kvaddr, &request_len);
	if (rc) {
		CAM_ERR(CAM_MEM, "Failed to get kernel vaddr");
		goto map_fail;
	}

	if (!inp->smmu_hdl) {
		CAM_ERR(CAM_MEM, "Invalid SMMU handle");
		rc = -EINVAL;
		goto smmu_fail;
	}

	/* SHARED flag gets precedence, all other flags after it */
	if (inp->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		region = CAM_SMMU_REGION_SHARED;
	} else {
		if (inp->flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;
	}

	rc = cam_smmu_map_kernel_iova(inp->smmu_hdl,
		buf,
		DMA_BIDIRECTIONAL,
		&iova,
		&request_len,
		region);

	if (rc < 0) {
		CAM_ERR(CAM_MEM, "SMMU mapping failed");
		goto smmu_fail;
	}

	smmu_hdl = inp->smmu_hdl;
	num_hdl = 1;

	idx = cam_mem_get_slot(&tbl);
	if (idx < 0) {
		CAM_ERR(CAM_MEM, "Get slot failed");
		rc = -ENOMEM;
		goto slot_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	mem_handle = GET_MEM_HANDLE(idx, fd);
	tbl.bufq[idx].dma_buf = buf;
	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].flags = inp->flags;
	tbl.bufq[idx].buf_handle = mem_handle;
	tbl.bufq[idx].kmdvaddr = kvaddr;

	tbl.bufq[idx].vaddr = iova;

	tbl.bufq[idx].len = inp->size;
	tbl.bufq[idx].num_hdl = num_hdl;
	memcpy(tbl.bufq[idx].hdls, &smmu_hdl,
		sizeof(int32_t));
	tbl.bufq[idx].is_imported = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	out->kva = kvaddr;
	out->iova = (uint32_t)iova;
	out->smmu_hdl = smmu_hdl;
	out->mem_handle = mem_handle;
	out->len = inp->size;
	out->region = region;

	return rc;
slot_fail:
	cam_smmu_unmap_kernel_iova(inp->smmu_hdl,
		buf, region);
smmu_fail:
	cam_mem_util_unmap_cpu_va(buf, kvaddr);
map_fail:
	cmm_free_buffer(buf);
get_dma_buf_fail:
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_request_mem);

int cam_mem_mgr_release_mem(struct cam_mem_mgr_memory_desc *inp)
{
	int32_t idx;
	int rc;

	if (!inp) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	mutex_lock(&tbl.m_lock);
	idx = CAM_MEM_MGR_GET_HDL_IDX(inp->mem_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index extracted from mem handle");
		rc = -EINVAL;
		goto err_mutex_unlock;
	}

	if (!tbl.bufq[idx].active) {
		if (tbl.bufq[idx].vaddr == 0) {
			CAM_ERR(CAM_MEM, "buffer is released already");
			rc = 0;
			goto err_mutex_unlock;
		}
		CAM_ERR(CAM_MEM, "Released buffer state should be active");
		rc = -EINVAL;
		goto err_mutex_unlock;
	}

	if (tbl.bufq[idx].buf_handle != inp->mem_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle not matching within table");
		rc = -EINVAL;
		goto err_mutex_unlock;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %X", inp->mem_handle);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_KERNEL);

err_mutex_unlock:
	mutex_unlock(&tbl.m_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_release_mem);

static int cmm_slot_table_create(struct cam_mem_table *table)
{
	int i, bitmap_size;
	int rc = 0;

	bitmap_size = BITS_TO_LONGS(CAM_MEM_BUFQ_MAX) * sizeof(long);
	table->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(table->bitmap)) {
		rc = PTR_ERR(table->bitmap);
		CAM_ERR(CAM_MEM, "Cannot allocate bitmap, rc:%d", rc);
		return rc;
	}

	mutex_init(&table->m_lock);
	mutex_lock(&table->m_lock);
	table->bits = bitmap_size * BITS_PER_BYTE;
	/* We need to reserve slot 0 */
	set_bit(0, table->bitmap);
	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		table->bufq[i].fd	  = -1;
		table->bufq[i].buf_handle = -1;
		mutex_init(&table->bufq[i].q_lock);
	}
	mutex_unlock(&table->m_lock);

	return rc;
}

static void cmm_slot_table_destroy(struct cam_mem_table *table)
{
	int i;

	mutex_lock(&table->m_lock);
	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		mutex_unlock(&table->bufq[i].q_lock);
		mutex_destroy(&table->bufq[i].q_lock);
	}
	memset(table->bufq, 0, sizeof(table->bufq));
	mutex_unlock(&table->m_lock);
	mutex_destroy(&table->m_lock);
	kfree(table->bitmap);
}

void cam_mem_mgr_close(void)
{
	struct cam_mem_mgr_release_cmd cmd;
        int i = 1;

        for_each_set_bit_from(i, tbl.bitmap, tbl.bits) {
		mutex_lock(&tbl.m_lock);
                if (tbl.bufq[i].active) {
                        CAM_DBG(CAM_MEM, "Active buffer idx=%d", i);
			cmd.buf_handle = tbl.bufq[i].buf_handle;
			mutex_lock(&tbl.bufq[i].q_lock);
			mutex_unlock(&tbl.m_lock);
			cam_mem_mgr_release(&cmd);
			mutex_unlock(&tbl.bufq[i].q_lock);
		} else {
			mutex_unlock(&tbl.m_lock);
		}
        }
}

int cam_mem_mgr_init(struct platform_device *pdev)
{
	int rc;

	rc = cam_buf_mgr_init(pdev);
	if (rc) {
		CAM_ERR(CAM_MEM, "Cannot initialize buffer manager, rc:%d", rc);
		return rc;
	}

	return cmm_slot_table_create(&tbl);
}

void cam_mem_mgr_exit(struct platform_device *pdev)
{
	cmm_slot_table_destroy(&tbl);

	cam_buf_mgr_exit(pdev);
}
