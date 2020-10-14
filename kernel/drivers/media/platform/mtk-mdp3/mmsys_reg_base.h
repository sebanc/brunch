/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MMSYS_REG_BASE_H__
#define __MMSYS_REG_BASE_H__

#define MM_REG_WRITE_MASK(cmd, id, base, ofst, val, mask, ...) \
	cmdq_pkt_write_mask(cmd->pkt, id, \
		(base) + (ofst), (val), (mask), ##__VA_ARGS__)
#define MM_REG_WRITE(cmd, id, base, ofst, val, mask, ...) \
	MM_REG_WRITE_MASK(cmd, id, base, ofst, val, \
		(((mask) & (ofst##_MASK)) == (ofst##_MASK)) ? \
			(0xffffffff) : (mask), ##__VA_ARGS__)

#define MM_REG_WAIT(cmd, evt) \
	cmdq_pkt_wfe(cmd->pkt, cmd->event[(evt)])

#define MM_REG_WAIT_NO_CLEAR(cmd, evt) \
	cmdq_pkt_wait_no_clear(cmd->pkt, cmd->event[(evt)])

#define MM_REG_CLEAR(cmd, evt) \
	cmdq_pkt_clear_event(cmd->pkt, cmd->event[(evt)])

#define MM_REG_SET_EVENT(cmd, evt) \
	cmdq_pkt_set_event(cmd->pkt, cmd->event[(evt)])

#define MM_REG_POLL_MASK(cmd, id, base, ofst, val, mask, ...) \
	cmdq_pkt_poll_mask(cmd->pkt, id, \
		(base) + (ofst), (val), (mask), ##__VA_ARGS__)
#define MM_REG_POLL(cmd, id, base, ofst, val, mask, ...) \
	MM_REG_POLL_MASK(cmd, id, base, ofst, val, \
		(((mask) & (ofst##_MASK)) == (ofst##_MASK)) ? \
			(0xffffffff) : (mask), ##__VA_ARGS__)

#endif  // __MM_REG_BASE_H__
