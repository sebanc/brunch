/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 */

#ifndef MTK_CMDQ_DEBUG_H
#define MTK_CMDQ_DEBUG_H

struct cmdq_buf_dump {
	struct device		*dev; /* device of cmdq controller */
	struct work_struct	dump_work;
	bool			timeout; /* 0: error, 1: timeout */
	void			*cmd_buf;
	size_t			cmd_buf_size;
	u32			pa_offset; /* pa_curr - pa_base */
};

void cmdq_debug_buf_dump_work(struct work_struct *work_item);

#endif /* MTK_CMDQ_DEBUG_H */
