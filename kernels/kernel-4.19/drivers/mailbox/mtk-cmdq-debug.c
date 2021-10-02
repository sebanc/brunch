// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/workqueue.h>
#include "mtk-cmdq-debug.h"

#define CMDQ_OP_CODE_MASK			(0xff << CMDQ_OP_CODE_SHIFT)
#define CMDQ_NUM_CMD(t)				(t->cmd_buf_size / CMDQ_INST_SIZE)
#define CMDQ_REG_IDX_PREFIX(type)		((type) ? "" : "Reg Index ")

struct cmdq_instruction {
        union {
                u32 value;
                u32 mask;
        };
        union {
                u16 offset;
                u16 event;
        };
        u8 subsys;
        u8 op;
};

static void cmdq_buf_print_write(struct device *dev, u32 offset,
				 struct cmdq_instruction *cmdq_inst)
{
	u32 addr = ((u32)(cmdq_inst->offset |
		    (cmdq_inst->subsys << CMDQ_SUBSYS_SHIFT)));

	dev_err(dev, "0x%08x [Write] Subsys Reg 0x%08x = 0x%08x\n",
		offset, addr, cmdq_inst->value);
}

static void cmdq_buf_print_wfe(struct device *dev, u32 offset,
			       struct cmdq_instruction *cmdq_inst)
{
	dev_err(dev, "0x%08x %s event ID %d\n", offset,
		(cmdq_inst->value & CMDQ_WFE_WAIT) ? "wait for" : "clear",
		cmdq_inst->event);
}

static void cmdq_buf_print_mask(struct device *dev, u32 offset,
				struct cmdq_instruction *cmdq_inst)
{
	dev_err(dev, "0x%08x mask 0x%08x\n", offset, ~(cmdq_inst->mask));
}

static void cmdq_buf_print_misc(struct device *dev, u32 offset,
				struct cmdq_instruction *cmdq_inst)
{
	char *cmd_str;

	switch (cmdq_inst->op) {
	case CMDQ_CODE_JUMP:
		cmd_str = "jump";
		break;
	case CMDQ_CODE_EOC:
		cmd_str = "eoc";
		break;
	case CMDQ_CODE_POLL:
		cmd_str = "polling";
		break;
	default:
		cmd_str = "unknown";
		break;
	}

	dev_err(dev, "0x%08x %s\n", offset, cmd_str);
}

void cmdq_debug_buf_dump_work(struct work_struct *work_item)
{
	struct cmdq_buf_dump *buf_dump = container_of(work_item,
			struct cmdq_buf_dump, dump_work);
	struct device *dev = buf_dump->dev;
	struct cmdq_instruction *cmdq_inst =
		(struct cmdq_instruction *)buf_dump->cmd_buf;
	u32 i, offset = 0;

	dev_err(dev, "dump %s task start ----------\n",
		buf_dump->timeout ? "timeout" : "error");
	for (i = 0; i < CMDQ_NUM_CMD(buf_dump); i++) {
		if (offset == buf_dump->pa_offset)
			dev_err(dev,
				"\e[1;31;40m==========ERROR==========\e[0m\n");
		switch (cmdq_inst[i].op) {
		case CMDQ_CODE_WRITE:
			cmdq_buf_print_write(dev, offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_WFE:
			cmdq_buf_print_wfe(dev, offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_MASK:
			cmdq_buf_print_mask(dev, offset, &cmdq_inst[i]);
			break;
		default:
			cmdq_buf_print_misc(dev, offset, &cmdq_inst[i]);
			break;
		}
		if (offset == buf_dump->pa_offset)
			dev_err(dev,
				"\e[1;31;40m==========ERROR==========\e[0m\n");
		offset += CMDQ_INST_SIZE;
	}
	dev_err(dev, "dump %s task end   ----------\n",
		buf_dump->timeout ? "timeout" : "error");

	kfree(buf_dump->cmd_buf);
	kfree(buf_dump);
}
EXPORT_SYMBOL(cmdq_debug_buf_dump_work);

MODULE_LICENSE("GPL v2");
