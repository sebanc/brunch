/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MMSYS_REG_BASE_H__
#define __MMSYS_REG_BASE_H__

#ifndef CMDQ_ADDR_HIGH
#define CMDQ_ADDR_HIGH(addr) ((u32)(((addr) >> 16) & GENMASK(31, 0)))
#endif
#ifndef CMDQ_ADDR_LOW
#define CMDQ_ADDR_LOW(addr)	 ((u16)(addr & 0xFFFF) | BIT(1))
#endif
#define UNUSED(x) (void)(x)

#define CMDQ_THR_SPR_0	(0)
#define CMDQ_THR_SPR_1	(1)
#define CMDQ_THR_SPR_2	(2)
#define CMDQ_THR_SPR_3	(3)
#define GCE_GPR_R11     (11)
/* CMDQ: P7: debug */
#define GCE_GPR_R14		0x0E
#define GCE_GPR_R15		0x0F

#define MM_REG_WRITE_MASK(cmd, id, base, ofst, val, mask, ...) \
	cmdq_pkt_write_mask(cmd->pkt, id, \
		(base) + (ofst), (val), (mask), ##__VA_ARGS__)
#define MM_REG_WRITE(cmd, id, base, ofst, val, mask, ...) \
	MM_REG_WRITE_MASK(cmd, id, base, ofst, val, \
		(((mask) & (ofst##_MASK)) == (ofst##_MASK)) ? \
			(0xffffffff) : (mask), ##__VA_ARGS__)

#define MM_REG_WRITE_S(cmd, id, base, ofst, val, mask, ...) \
do{ \
    UNUSED(id); \
    cmdq_pkt_assign(cmd->pkt, CMDQ_THR_SPR_0, \
        CMDQ_ADDR_HIGH(base+ofst)); \
	cmdq_pkt_write_s_mask_value(cmd->pkt, CMDQ_THR_SPR_0, \
		CMDQ_ADDR_LOW(base+ofst), (val), (mask), ##__VA_ARGS__); \
}while(0)

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

#define MM_REG_POLL_S(cmd, id, base, ofst, val, mask, ...) \
do{ \
    UNUSED(id); \
    cmdq_pkt_poll_addr(cmd->pkt, val, ((base) + (ofst)), mask, GCE_GPR_R14); \
}while(0)

#endif  // __MM_REG_BASE_H__
