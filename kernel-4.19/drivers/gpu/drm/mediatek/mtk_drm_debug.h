/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_DRM_DEBUG_H
#define MTK_DRM_DEBUG_H

void mtk_drm_dbg_init(struct platform_device *pdev);
void mtk_drm_dbg_dump_path(u32 pipe);

#define mtk_drm_dbg(fmt, ...) pr_info("[mtkdrm] " fmt, ##__VA_ARGS__)

#define mtk_drm_err(fmt, ...) pr_err("[mtkdrm] error " fmt, ##__VA_ARGS__)

#endif /* MTK_DRM_DEBUGFS_H */
