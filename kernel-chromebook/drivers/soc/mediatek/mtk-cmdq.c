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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <soc/mediatek/cmdq.h>

/*
 * Please calculate this value for each platform.
 * task number = vblank time / ((task cmds * cmd ticks) / GCE freq)
 */
#define CMDQ_MAX_TASK_IN_THREAD		70

#define CMDQ_INITIAL_CMD_BLOCK_SIZE	PAGE_SIZE
#define CMDQ_CMD_BUF_POOL_BUF_SIZE	PAGE_SIZE
#define CMDQ_CMD_BUF_POOL_BUF_NUM	140 /* 2 * 70 = 140 */
#define CMDQ_INST_SIZE			8 /* instruction is 64-bit */

/*
 * cmdq_thread cookie value is from 0 to CMDQ_MAX_COOKIE_VALUE.
 * And, this value also be used as MASK.
 */
#define CMDQ_MAX_COOKIE_VALUE		0xffff
#define CMDQ_COOKIE_MASK		CMDQ_MAX_COOKIE_VALUE

#define CMDQ_DEFAULT_TIMEOUT_MS		1000
#define CMDQ_ACQUIRE_THREAD_TIMEOUT_MS	5000
#define CMDQ_PREALARM_TIMEOUT_NS	200000000

#define CMDQ_INVALID_THREAD		-1

#define CMDQ_DRIVER_DEVICE_NAME		"mtk_cmdq"

#define CMDQ_CLK_NAME			"gce"

#define CMDQ_CURR_IRQ_STATUS_OFFSET	0x010
#define CMDQ_CURR_LOADED_THR_OFFSET	0x018
#define CMDQ_THR_SLOT_CYCLES_OFFSET	0x030
#define CMDQ_THR_EXEC_CYCLES_OFFSET	0x034
#define CMDQ_THR_TIMEOUT_TIMER_OFFSET	0x038
#define CMDQ_BUS_CONTROL_TYPE_OFFSET	0x040

#define CMDQ_SYNC_TOKEN_ID_OFFSET	0x060
#define CMDQ_SYNC_TOKEN_VAL_OFFSET	0x064
#define CMDQ_SYNC_TOKEN_UPD_OFFSET	0x068

#define CMDQ_GPR_SHIFT			0x004
#define CMDQ_GPR_OFFSET			0x080

#define CMDQ_THR_SHIFT			0x080
#define CMDQ_THR_WARM_RESET_OFFSET	0x100
#define CMDQ_THR_ENABLE_TASK_OFFSET	0x104
#define CMDQ_THR_SUSPEND_TASK_OFFSET	0x108
#define CMDQ_THR_CURR_STATUS_OFFSET	0x10c
#define CMDQ_THR_IRQ_STATUS_OFFSET	0x110
#define CMDQ_THR_IRQ_ENABLE_OFFSET	0x114
#define CMDQ_THR_CURR_ADDR_OFFSET	0x120
#define CMDQ_THR_END_ADDR_OFFSET	0x124
#define CMDQ_THR_EXEC_CNT_OFFSET	0x128
#define CMDQ_THR_WAIT_TOKEN_OFFSET	0x130
#define CMDQ_THR_CFG_OFFSET		0x140
#define CMDQ_THR_INST_CYCLES_OFFSET	0x150
#define CMDQ_THR_INST_THRESX_OFFSET	0x154
#define CMDQ_THR_STATUS_OFFSET		0x18c

#define CMDQ_SYNC_TOKEN_SET		BIT(16)
#define CMDQ_IRQ_MASK			0xffff

#define CMDQ_THR_ENABLED		0x1
#define CMDQ_THR_DISABLED		0x0
#define CMDQ_THR_SUSPEND		0x1
#define CMDQ_THR_RESUME			0x0
#define CMDQ_THR_STATUS_SUSPENDED	BIT(1)
#define CMDQ_THR_WARM_RESET		BIT(0)
#define CMDQ_THR_SLOT_CYCLES		0x3200
#define CMDQ_THR_NO_TIMEOUT		0x0
#define CMDQ_THR_PRIORITY		3
#define CMDQ_THR_IRQ_DONE		0x1
#define CMDQ_THR_IRQ_ERROR		0x12
#define CMDQ_THR_IRQ_EN			0x13 /* done + error */
#define CMDQ_THR_IRQ_MASK		0x13
#define CMDQ_THR_EXECUTING		BIT(31)
#define CMDQ_THR_IS_WAITING		BIT(31)

#define CMDQ_ARG_A_MASK			0xffffff
#define CMDQ_ARG_A_WRITE_MASK		0xffff
#define CMDQ_ARG_A_SUBSYS_MASK		0x1f0000
#define CMDQ_SUBSYS_MASK		0x1f

#define CMDQ_OP_CODE_SHIFT		24
#define CMDQ_SUBSYS_SHIFT		16

#define CMDQ_JUMP_BY_OFFSET		0x10000000
#define CMDQ_JUMP_BY_PA			0x10000001
#define CMDQ_JUMP_PASS			CMDQ_INST_SIZE

#define CMDQ_WFE_UPDATE			BIT(31)
#define CMDQ_WFE_WAIT			BIT(15)
#define CMDQ_WFE_WAIT_VALUE		0x1

#define CMDQ_MARK_NON_SUSPENDABLE	BIT(21) /* 53 - 32 = 21 */
#define CMDQ_MARK_NOT_ADD_COUNTER	BIT(16) /* 48 - 32 = 16 */
#define CMDQ_MARK_PREFETCH_MARKER	BIT(20)
#define CMDQ_MARK_PREFETCH_MARKER_EN	BIT(17)
#define CMDQ_MARK_PREFETCH_EN		BIT(16)

#define CMDQ_EOC_IRQ_EN			BIT(0)

#define CMDQ_ENABLE_MASK		BIT(0)

#define CMDQ_OP_CODE_MASK		0xff000000

enum cmdq_thread_index {
	CMDQ_THR_DISP_DSI0 = 0,		/* main: dsi0 */
	CMDQ_THR_DISP_DPI0,		/* sub: dpi0 */
	CMDQ_MAX_THREAD_COUNT,		/* max */
};

struct cmdq_command {
	struct cmdq		*cqctx;
	/* bit flag of used engines */
	u64			engine_flag;
	/*
	 * pointer of instruction buffer
	 * This must point to an 64-bit aligned u32 array
	 */
	u32			*va_base;
	/* size of instruction buffer, in bytes. */
	size_t			block_size;
};

enum cmdq_code {
	/* These are actual HW op code. */
	CMDQ_CODE_MOVE = 0x02,
	CMDQ_CODE_WRITE = 0x04,
	CMDQ_CODE_JUMP = 0x10,
	CMDQ_CODE_WFE = 0x20,	/* wait for event (and clear) */
	CMDQ_CODE_CLEAR_EVENT = 0x21,	/* clear event */
	CMDQ_CODE_EOC = 0x40,	/* end of command */
};

enum cmdq_task_state {
	TASK_STATE_IDLE,	/* free task */
	TASK_STATE_BUSY,	/* task running on a thread */
	TASK_STATE_KILLED,	/* task process being killed */
	TASK_STATE_ERROR,	/* task execution error */
	TASK_STATE_DONE,	/* task finished */
	TASK_STATE_WAITING,	/* allocated but waiting for available thread */
};

struct cmdq_cmd_buf {
	atomic_t		used;
	void			*va;
	dma_addr_t		pa;
};

struct cmdq_task_cb {
	/* called by isr */
	cmdq_async_flush_cb	isr_cb;
	void			*isr_data;
	/* called by releasing task */
	cmdq_async_flush_cb	done_cb;
	void			*done_data;
};

struct cmdq_task {
	struct cmdq		*cqctx;
	struct list_head	list_entry;

	/* state for task life cycle */
	enum cmdq_task_state	task_state;
	/* virtual address of command buffer */
	u32			*va_base;
	/* physical address of command buffer */
	dma_addr_t		mva_base;
	/* size of allocated command buffer */
	size_t			buf_size;
	/* It points to a cmdq_cmd_buf if this task use command buffer pool. */
	struct cmdq_cmd_buf	*cmd_buf;

	u64			engine_flag;
	size_t			command_size;
	u32			num_cmd; /* 2 * number of commands */
	int			reorder;
	/* HW thread ID; CMDQ_INVALID_THREAD if not running */
	int			thread;
	/* flag of IRQ received */
	int			irq_flag;
	/* callback functions */
	struct cmdq_task_cb	cb;
	/* work item when auto release is used */
	struct work_struct	auto_release_work;

	ktime_t			submit; /* submit time */

	pid_t			caller_pid;
	char			caller_name[TASK_COMM_LEN];
};

struct cmdq_thread {
	u32			task_count;
	u32			wait_cookie;
	u32			next_cookie;
	struct cmdq_task	*cur_task[CMDQ_MAX_TASK_IN_THREAD];
};

struct cmdq {
	struct device		*dev;

	void __iomem		*base;
	u32			irq;

	/*
	 * task information
	 * task_cache: struct cmdq_task object cache
	 * task_free_list: unused free tasks
	 * task_active_list: active tasks
	 * task_consume_wait_queue_item: task consumption work item
	 * task_auto_release_wq: auto-release workqueue
	 * task_consume_wq: task consumption workqueue (for queued tasks)
	 */
	struct kmem_cache	*task_cache;
	struct list_head	task_free_list;
	struct list_head	task_active_list;
	struct list_head	task_wait_list;
	struct work_struct	task_consume_wait_queue_item;
	struct workqueue_struct	*task_auto_release_wq;
	struct workqueue_struct	*task_consume_wq;

	struct cmdq_thread	thread[CMDQ_MAX_THREAD_COUNT];

	/* mutex, spinlock, flag */
	struct mutex		task_mutex;	/* for task list */
	struct mutex		clock_mutex;	/* for clock operation */
	spinlock_t		thread_lock;	/* for cmdq hardware thread */
	int			thread_usage;
	spinlock_t		exec_lock;	/* for exec task */

	/* command buffer pool */
	struct cmdq_cmd_buf	cmd_buf_pool[CMDQ_CMD_BUF_POOL_BUF_NUM];

	/*
	 * notification
	 * wait_queue: for task done
	 * thread_dispatch_queue: for thread acquiring
	 */
	wait_queue_head_t	wait_queue[CMDQ_MAX_THREAD_COUNT];
	wait_queue_head_t	thread_dispatch_queue;

	/* ccf */
	struct clk		*clock;
};

struct cmdq_event_item {
	enum cmdq_event	event;
	const char	*name;
};

struct cmdq_subsys {
	u32		base_addr;
	int		id;
	const char	*name;
};

static const struct cmdq_event_item cmdq_events[] = {
	/* Display start of frame(SOF) events */
	{CMDQ_EVENT_DISP_OVL0_SOF, "CMDQ_EVENT_DISP_OVL0_SOF"},
	{CMDQ_EVENT_DISP_OVL1_SOF, "CMDQ_EVENT_DISP_OVL1_SOF"},
	{CMDQ_EVENT_DISP_RDMA0_SOF, "CMDQ_EVENT_DISP_RDMA0_SOF"},
	{CMDQ_EVENT_DISP_RDMA1_SOF, "CMDQ_EVENT_DISP_RDMA1_SOF"},
	{CMDQ_EVENT_DISP_RDMA2_SOF, "CMDQ_EVENT_DISP_RDMA2_SOF"},
	{CMDQ_EVENT_DISP_WDMA0_SOF, "CMDQ_EVENT_DISP_WDMA0_SOF"},
	{CMDQ_EVENT_DISP_WDMA1_SOF, "CMDQ_EVENT_DISP_WDMA1_SOF"},
	/* Display end of frame(EOF) events */
	{CMDQ_EVENT_DISP_OVL0_EOF, "CMDQ_EVENT_DISP_OVL0_EOF"},
	{CMDQ_EVENT_DISP_OVL1_EOF, "CMDQ_EVENT_DISP_OVL1_EOF"},
	{CMDQ_EVENT_DISP_RDMA0_EOF, "CMDQ_EVENT_DISP_RDMA0_EOF"},
	{CMDQ_EVENT_DISP_RDMA1_EOF, "CMDQ_EVENT_DISP_RDMA1_EOF"},
	{CMDQ_EVENT_DISP_RDMA2_EOF, "CMDQ_EVENT_DISP_RDMA2_EOF"},
	{CMDQ_EVENT_DISP_WDMA0_EOF, "CMDQ_EVENT_DISP_WDMA0_EOF"},
	{CMDQ_EVENT_DISP_WDMA1_EOF, "CMDQ_EVENT_DISP_WDMA1_EOF"},
	/* Mutex end of frame(EOF) events */
	{CMDQ_EVENT_MUTEX0_STREAM_EOF, "CMDQ_EVENT_MUTEX0_STREAM_EOF"},
	{CMDQ_EVENT_MUTEX1_STREAM_EOF, "CMDQ_EVENT_MUTEX1_STREAM_EOF"},
	{CMDQ_EVENT_MUTEX2_STREAM_EOF, "CMDQ_EVENT_MUTEX2_STREAM_EOF"},
	{CMDQ_EVENT_MUTEX3_STREAM_EOF, "CMDQ_EVENT_MUTEX3_STREAM_EOF"},
	{CMDQ_EVENT_MUTEX4_STREAM_EOF, "CMDQ_EVENT_MUTEX4_STREAM_EOF"},
	/* Display underrun events */
	{CMDQ_EVENT_DISP_RDMA0_UNDERRUN, "CMDQ_EVENT_DISP_RDMA0_UNDERRUN"},
	{CMDQ_EVENT_DISP_RDMA1_UNDERRUN, "CMDQ_EVENT_DISP_RDMA1_UNDERRUN"},
	{CMDQ_EVENT_DISP_RDMA2_UNDERRUN, "CMDQ_EVENT_DISP_RDMA2_UNDERRUN"},
	/* Keep this at the end of HW events */
	{CMDQ_MAX_HW_EVENT_COUNT, "CMDQ_MAX_HW_EVENT_COUNT"},
	/* This is max event and also can be used as mask. */
	{CMDQ_SYNC_TOKEN_MAX, "CMDQ_SYNC_TOKEN_MAX"},
	/* Invalid event */
	{CMDQ_SYNC_TOKEN_INVALID, "CMDQ_SYNC_TOKEN_INVALID"},
};

static const struct cmdq_subsys g_subsys[] = {
	{0x1400, 1, "MMSYS"},
	{0x1401, 2, "DISP"},
	{0x1402, 3, "DISP"},
};

static const char *cmdq_event_get_name(enum cmdq_event event)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cmdq_events); i++)
		if (cmdq_events[i].event == event)
			return cmdq_events[i].name;

	return "CMDQ_EVENT_UNKNOWN";
}

static void cmdq_event_reset(struct cmdq *cqctx)
{
	int i;

	/* set all defined HW events to 0 */
	for (i = 0; i < ARRAY_SIZE(cmdq_events); i++) {
		if (cmdq_events[i].event >= CMDQ_MAX_HW_EVENT_COUNT)
			break;
		writel(cmdq_events[i].event,
		       cqctx->base + CMDQ_SYNC_TOKEN_UPD_OFFSET);
	}
}

static int cmdq_subsys_base_addr_to_id(u32 base_addr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_subsys); i++) {
		if (g_subsys[i].base_addr == base_addr)
			return g_subsys[i].id;
	}

	return -EFAULT;
}

static u32 cmdq_subsys_id_to_base_addr(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_subsys); i++) {
		if (g_subsys[i].id == id)
			return g_subsys[i].base_addr;
	}

	return 0;
}

static const char *cmdq_subsys_base_addr_to_name(u32 base_addr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_subsys); i++)
		if (g_subsys[i].base_addr == base_addr)
			return g_subsys[i].name;

	return NULL;
}

static int cmdq_eng_get_thread(u64 flag)
{
	if (flag & BIT_ULL(CMDQ_ENG_DISP_DSI0))
		return CMDQ_THR_DISP_DSI0;
	else /* CMDQ_ENG_DISP_DPI0 */
		return CMDQ_THR_DISP_DPI0;
}

static const char *cmdq_event_get_module(enum cmdq_event event)
{
	const char *module;

	switch (event) {
	case CMDQ_EVENT_DISP_RDMA0_SOF:
	case CMDQ_EVENT_DISP_RDMA1_SOF:
	case CMDQ_EVENT_DISP_RDMA2_SOF:
	case CMDQ_EVENT_DISP_RDMA0_EOF:
	case CMDQ_EVENT_DISP_RDMA1_EOF:
	case CMDQ_EVENT_DISP_RDMA2_EOF:
	case CMDQ_EVENT_DISP_RDMA0_UNDERRUN:
	case CMDQ_EVENT_DISP_RDMA1_UNDERRUN:
	case CMDQ_EVENT_DISP_RDMA2_UNDERRUN:
		module = "DISP_RDMA";
		break;
	case CMDQ_EVENT_DISP_WDMA0_SOF:
	case CMDQ_EVENT_DISP_WDMA1_SOF:
	case CMDQ_EVENT_DISP_WDMA0_EOF:
	case CMDQ_EVENT_DISP_WDMA1_EOF:
		module = "DISP_WDMA";
		break;
	case CMDQ_EVENT_DISP_OVL0_SOF:
	case CMDQ_EVENT_DISP_OVL1_SOF:
	case CMDQ_EVENT_DISP_OVL0_EOF:
	case CMDQ_EVENT_DISP_OVL1_EOF:
		module = "DISP_OVL";
		break;
	case CMDQ_EVENT_MUTEX0_STREAM_EOF ... CMDQ_EVENT_MUTEX4_STREAM_EOF:
		module = "DISP";
		break;
	default:
		module = "CMDQ";
		break;
	}

	return module;
}

static u32 cmdq_thread_get_cookie(struct cmdq *cqctx, int tid)
{
	return readl(cqctx->base + CMDQ_THR_EXEC_CNT_OFFSET +
		     CMDQ_THR_SHIFT * tid) & CMDQ_COOKIE_MASK;
}

static int cmdq_cmd_buf_pool_init(struct cmdq *cqctx)
{
	struct device *dev = cqctx->dev;
	int i;
	int ret = 0;
	struct cmdq_cmd_buf *buf;

	for (i = 0; i < ARRAY_SIZE(cqctx->cmd_buf_pool); i++) {
		buf = &cqctx->cmd_buf_pool[i];
		buf->va = dma_alloc_coherent(dev, CMDQ_CMD_BUF_POOL_BUF_SIZE,
					     &buf->pa, GFP_KERNEL);
		if (!buf->va) {
			dev_err(dev, "failed to alloc cmdq_cmd_buf\n");
			ret = -ENOMEM;
			goto fail_alloc;
		}
	}

	return 0;

fail_alloc:
	for (i -= 1; i >= 0 ; i--) {
		buf = &cqctx->cmd_buf_pool[i];
		dma_free_coherent(dev, CMDQ_CMD_BUF_POOL_BUF_SIZE, buf->va,
				  buf->pa);
	}

	return ret;
}

static void cmdq_cmd_buf_pool_uninit(struct cmdq *cqctx)
{
	struct device *dev = cqctx->dev;
	int i;
	struct cmdq_cmd_buf *buf;

	for (i = 0; i < ARRAY_SIZE(cqctx->cmd_buf_pool); i++) {
		buf = &cqctx->cmd_buf_pool[i];
		dma_free_coherent(dev, CMDQ_CMD_BUF_POOL_BUF_SIZE, buf->va,
				  buf->pa);
		if (atomic_read(&buf->used))
			dev_err(dev,
				"cmdq_cmd_buf[%d] va:0x%p still in use\n",
				i, buf->va);
	}
}

static struct cmdq_cmd_buf *cmdq_cmd_buf_pool_get(struct cmdq *cqctx)
{
	int i;
	struct cmdq_cmd_buf *buf;

	for (i = 0; i < ARRAY_SIZE(cqctx->cmd_buf_pool); i++) {
		buf = &cqctx->cmd_buf_pool[i];
		if (!atomic_cmpxchg(&buf->used, 0, 1))
			return buf;
	}

	return NULL;
}

static void cmdq_cmd_buf_pool_put(struct cmdq_cmd_buf *buf)
{
	atomic_set(&buf->used, 0);
}

static int cmdq_subsys_from_phys_addr(struct cmdq *cqctx, u32 cmdq_phys_addr)
{
	u32 base_addr = cmdq_phys_addr >> 16;
	int subsys = cmdq_subsys_base_addr_to_id(base_addr);

	if (subsys < 0)
		dev_err(cqctx->dev,
			"unknown subsys: error=%d, phys=0x%08x\n",
			subsys, cmdq_phys_addr);

	return subsys;
}

/*
 * It's a kmemcache creator for cmdq_task to initialize variables
 * without command buffer.
 */
static void cmdq_task_ctor(void *param)
{
	struct cmdq_task *task = param;

	memset(task, 0, sizeof(*task));
	INIT_LIST_HEAD(&task->list_entry);
	task->task_state = TASK_STATE_IDLE;
	task->thread = CMDQ_INVALID_THREAD;
}

static void cmdq_task_free_command_buffer(struct cmdq_task *task)
{
	struct cmdq *cqctx = task->cqctx;
	struct device *dev = cqctx->dev;

	if (!task->va_base)
		return;

	if (task->cmd_buf)
		cmdq_cmd_buf_pool_put(task->cmd_buf);
	else
		dma_free_coherent(dev, task->buf_size, task->va_base,
				  task->mva_base);

	task->va_base = NULL;
	task->mva_base = 0;
	task->buf_size = 0;
	task->command_size = 0;
	task->num_cmd = 0;
	task->cmd_buf = NULL;
}

/*
 * Ensure size of command buffer in the given cmdq_task.
 * Existing buffer data will be copied to new buffer.
 * This buffer is guaranteed to be physically continuous.
 * returns -ENOMEM if cannot allocate new buffer
 */
static int cmdq_task_realloc_command_buffer(struct cmdq_task *task, size_t size)
{
	struct cmdq *cqctx = task->cqctx;
	struct device *dev = cqctx->dev;
	void *new_buf = NULL;
	dma_addr_t new_mva_base;
	size_t cmd_size;
	u32 num_cmd;
	struct cmdq_cmd_buf *cmd_buf = NULL;

	if (task->va_base && task->buf_size >= size)
		return 0;

	/* try command pool first */
	if (size <= CMDQ_CMD_BUF_POOL_BUF_SIZE) {
		cmd_buf = cmdq_cmd_buf_pool_get(cqctx);
		if (cmd_buf) {
			new_buf = cmd_buf->va;
			new_mva_base = cmd_buf->pa;
			memset(new_buf, 0, CMDQ_CMD_BUF_POOL_BUF_SIZE);
		}
	}

	if (!new_buf) {
		new_buf = dma_alloc_coherent(dev, size, &new_mva_base,
					     GFP_KERNEL);
		if (!new_buf) {
			dev_err(dev, "realloc cmd buffer of size %zu failed\n",
				size);
			return -ENOMEM;
		}
	}

	/* copy and release old buffer */
	if (task->va_base)
		memcpy(new_buf, task->va_base, task->buf_size);

	/*
	 * we should keep track of num_cmd and cmd_size
	 * since they are cleared in free command buffer
	 */
	num_cmd = task->num_cmd;
	cmd_size = task->command_size;
	cmdq_task_free_command_buffer(task);

	/* attach the new buffer */
	task->va_base = new_buf;
	task->mva_base = new_mva_base;
	task->buf_size = cmd_buf ? CMDQ_CMD_BUF_POOL_BUF_SIZE : size;
	task->num_cmd = num_cmd;
	task->command_size = cmd_size;
	task->cmd_buf = cmd_buf;

	return 0;
}

/* allocate and initialize struct cmdq_task and its command buffer */
static struct cmdq_task *cmdq_task_create(struct cmdq *cqctx)
{
	struct device *dev = cqctx->dev;
	struct cmdq_task *task;
	int status;

	task = kmem_cache_alloc(cqctx->task_cache, GFP_KERNEL);
	task->cqctx = cqctx;
	status = cmdq_task_realloc_command_buffer(
			task, CMDQ_INITIAL_CMD_BLOCK_SIZE);
	if (status < 0) {
		dev_err(dev, "allocate command buffer failed\n");
		kmem_cache_free(cqctx->task_cache, task);
		return NULL;
	}
	return task;
}

static int cmdq_dev_init(struct platform_device *pdev, struct cmdq *cqctx)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cqctx->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(cqctx->base)) {
		dev_err(dev, "failed to ioremap gce\n");
		return PTR_ERR(cqctx->base);
	}

	cqctx->irq = irq_of_parse_and_map(node, 0);
	if (!cqctx->irq) {
		dev_err(dev, "failed to get irq\n");
		return -EINVAL;
	}

	dev_dbg(dev, "cmdq device: addr:0x%p, va:0x%p, irq:%d\n",
		dev, cqctx->base, cqctx->irq);
	return 0;
}

static void cmdq_task_release_unlocked(struct cmdq_task *task)
{
	struct cmdq *cqctx = task->cqctx;

	/* This func should be inside cqctx->task_mutex mutex */
	lockdep_assert_held(&cqctx->task_mutex);

	task->task_state = TASK_STATE_IDLE;
	task->thread = CMDQ_INVALID_THREAD;

	cmdq_task_free_command_buffer(task);

	/*
	 * move from active/waiting list to free list
	 * todo: shrink free list
	 */
	list_move_tail(&task->list_entry, &cqctx->task_free_list);
}

static void cmdq_task_release_internal(struct cmdq_task *task)
{
	struct cmdq *cqctx = task->cqctx;

	mutex_lock(&cqctx->task_mutex);
	cmdq_task_release_unlocked(task);
	mutex_unlock(&cqctx->task_mutex);
}

static struct cmdq_task *cmdq_core_find_free_task(struct cmdq *cqctx)
{
	struct cmdq_task *task;

	mutex_lock(&cqctx->task_mutex);

	/*
	 * Pick from free list first;
	 * create one if there is no free entry.
	 */
	if (list_empty(&cqctx->task_free_list)) {
		task = cmdq_task_create(cqctx);
	} else {
		task = list_first_entry(&cqctx->task_free_list,
					struct cmdq_task, list_entry);
		/* remove from free list */
		list_del_init(&task->list_entry);
	}

	mutex_unlock(&cqctx->task_mutex);

	return task;
}

/* After dropping error task, we have to reorder remaining valid tasks. */
static void cmdq_thread_reorder_task_array(struct cmdq_thread *thread,
					   int prev_id)
{
	int i, j;
	int next_id, search_id;
	int reorder_count = 0;
	struct cmdq_task *task;

	next_id = prev_id + 1;
	for (i = 1; i < (CMDQ_MAX_TASK_IN_THREAD - 1); i++, next_id++) {
		if (next_id >= CMDQ_MAX_TASK_IN_THREAD)
			next_id = 0;

		if (thread->cur_task[next_id])
			break;

		search_id = next_id + 1;
		for (j = (i + 1); j < CMDQ_MAX_TASK_IN_THREAD;
		     j++, search_id++) {
			if (search_id >= CMDQ_MAX_TASK_IN_THREAD)
				search_id = 0;

			if (thread->cur_task[search_id]) {
				thread->cur_task[next_id] =
					thread->cur_task[search_id];
				thread->cur_task[search_id] = NULL;
				if ((j - i) > reorder_count)
					reorder_count = j - i;

				break;
			}
		}

		task = thread->cur_task[next_id];
		if ((task->va_base[task->num_cmd - 1] == CMDQ_JUMP_BY_OFFSET) &&
		    (task->va_base[task->num_cmd - 2] == CMDQ_JUMP_PASS)) {
			/* We reached the last task */
			break;
		}
	}

	thread->next_cookie -= reorder_count;
}

static int cmdq_core_sync_command(struct cmdq_task *task,
				  struct cmdq_command *cmd_desc)
{
	struct cmdq *cqctx = task->cqctx;
	struct device *dev = cqctx->dev;
	int status;
	size_t size;

	size = task->command_size + CMDQ_INST_SIZE;
	status = cmdq_task_realloc_command_buffer(task, size);
	if (status < 0) {
		dev_err(dev, "failed to realloc command buffer\n");
		dev_err(dev, "task=0x%p, request size=%zu\n", task, size);
		return status;
	}

	/* copy the commands to our DMA buffer */
	memcpy(task->va_base, cmd_desc->va_base, cmd_desc->block_size);

	/* re-adjust num_cmd according to command_size */
	task->num_cmd = task->command_size / sizeof(task->va_base[0]);

	return 0;
}

static struct cmdq_task *cmdq_core_acquire_task(struct cmdq_command *cmd_desc,
						struct cmdq_task_cb *cb)
{
	struct cmdq *cqctx = cmd_desc->cqctx;
	struct device *dev = cqctx->dev;
	struct cmdq_task *task;

	task = cmdq_core_find_free_task(cqctx);
	if (!task) {
		dev_err(dev, "can't acquire task info\n");
		return NULL;
	}

	/* initialize field values */
	task->engine_flag = cmd_desc->engine_flag;
	task->task_state = TASK_STATE_WAITING;
	task->reorder = 0;
	task->thread = CMDQ_INVALID_THREAD;
	task->irq_flag = 0x0;
	if (cb)
		task->cb = *cb;
	else
		memset(&task->cb, 0, sizeof(task->cb));
	task->command_size = cmd_desc->block_size;

	/* store caller info for debug */
	if (current) {
		task->caller_pid = current->pid;
		memcpy(task->caller_name, current->comm, sizeof(current->comm));
	}

	if (cmdq_core_sync_command(task, cmd_desc) < 0) {
		dev_err(dev, "fail to sync command\n");
		cmdq_task_release_internal(task);
		return NULL;
	}

	/* insert into waiting list to process */
	if (task) {
		task->submit = ktime_get();
		mutex_lock(&cqctx->task_mutex);
		list_add_tail(&task->list_entry, &cqctx->task_wait_list);
		mutex_unlock(&cqctx->task_mutex);
	}

	return task;
}

static int cmdq_clk_enable(struct cmdq *cqctx)
{
	struct device *dev = cqctx->dev;
	int ret = 0;

	if (cqctx->thread_usage == 0) {
		ret = clk_prepare_enable(cqctx->clock);
		if (ret) {
			dev_err(dev, "prepare and enable clk:%s fail\n",
				CMDQ_CLK_NAME);
			return ret;
		}
		cmdq_event_reset(cqctx);
	}
	cqctx->thread_usage++;

	return ret;
}

static void cmdq_clk_disable(struct cmdq *cqctx)
{
	cqctx->thread_usage--;
	if (cqctx->thread_usage <= 0)
		clk_disable_unprepare(cqctx->clock);
}

static int cmdq_core_find_free_thread(struct cmdq *cqctx, int tid)
{
	struct cmdq_thread *thread = cqctx->thread;
	u32 next_cookie;

	/*
	 * make sure the found thread has enough space for the task;
	 * cmdq_thread->cur_task has size limitation.
	 */
	if (thread[tid].task_count >= CMDQ_MAX_TASK_IN_THREAD) {
		dev_warn(cqctx->dev, "thread(%d) task count = %d\n",
			 tid, thread[tid].task_count);
		return CMDQ_INVALID_THREAD;
	}

	next_cookie = thread[tid].next_cookie % CMDQ_MAX_TASK_IN_THREAD;
	if (thread[tid].cur_task[next_cookie]) {
		dev_warn(cqctx->dev, "thread(%d) next cookie = %d\n",
			 tid, next_cookie);
		return CMDQ_INVALID_THREAD;
	}

	return tid;
}

static struct cmdq_thread *cmdq_core_acquire_thread(struct cmdq *cqctx,
						    int candidate_tid)
{
	int tid;

	tid = cmdq_core_find_free_thread(cqctx, candidate_tid);
	if (tid != CMDQ_INVALID_THREAD) {
		mutex_lock(&cqctx->clock_mutex);
		cmdq_clk_enable(cqctx);
		mutex_unlock(&cqctx->clock_mutex);
		return &cqctx->thread[tid];
	}
	return NULL;
}

static void cmdq_core_release_thread(struct cmdq *cqctx, int tid)
{
	if (WARN_ON(tid == CMDQ_INVALID_THREAD))
		return;

	mutex_lock(&cqctx->clock_mutex);
	cmdq_clk_disable(cqctx);
	mutex_unlock(&cqctx->clock_mutex);
}

static void cmdq_task_remove_thread(struct cmdq_task *task)
{
	int tid = task->thread;

	task->thread = CMDQ_INVALID_THREAD;
	cmdq_core_release_thread(task->cqctx, tid);
}

static int cmdq_thread_suspend(struct cmdq *cqctx, int tid)
{
	struct device *dev = cqctx->dev;
	void __iomem *gce_base = cqctx->base;
	u32 enabled;
	u32 status;

	/* write suspend bit */
	writel(CMDQ_THR_SUSPEND,
	       gce_base + CMDQ_THR_SUSPEND_TASK_OFFSET +
	       CMDQ_THR_SHIFT * tid);

	/* If already disabled, treat as suspended successful. */
	enabled = readl(gce_base + CMDQ_THR_ENABLE_TASK_OFFSET +
			CMDQ_THR_SHIFT * tid);
	if (!(enabled & CMDQ_THR_ENABLED))
		return 0;

	/* poll suspended status */
	if (readl_poll_timeout_atomic(gce_base +
				      CMDQ_THR_CURR_STATUS_OFFSET +
				      CMDQ_THR_SHIFT * tid,
				      status,
				      status & CMDQ_THR_STATUS_SUSPENDED,
				      0, 10)) {
		dev_err(dev, "Suspend HW thread %d failed\n", tid);
		return -EFAULT;
	}

	return 0;
}

static void cmdq_thread_resume(struct cmdq *cqctx, int tid)
{
	void __iomem *gce_base = cqctx->base;

	writel(CMDQ_THR_RESUME,
	       gce_base + CMDQ_THR_SUSPEND_TASK_OFFSET +
	       CMDQ_THR_SHIFT * tid);
}

static int cmdq_thread_reset(struct cmdq *cqctx, int tid)
{
	struct device *dev = cqctx->dev;
	void __iomem *gce_base = cqctx->base;
	u32 warm_reset;

	writel(CMDQ_THR_WARM_RESET,
	       gce_base + CMDQ_THR_WARM_RESET_OFFSET +
	       CMDQ_THR_SHIFT * tid);

	if (readl_poll_timeout_atomic(gce_base + CMDQ_THR_WARM_RESET_OFFSET +
				      CMDQ_THR_SHIFT * tid,
				      warm_reset,
				      !(warm_reset & CMDQ_THR_WARM_RESET),
				      0, 10)) {
		dev_err(dev, "Reset HW thread %d failed\n", tid);
		return -EFAULT;
	}

	writel(CMDQ_THR_SLOT_CYCLES, gce_base + CMDQ_THR_SLOT_CYCLES_OFFSET);
	return 0;
}

static int cmdq_thread_disable(struct cmdq *cqctx, int tid)
{
	void __iomem *gce_base = cqctx->base;

	cmdq_thread_reset(cqctx, tid);
	writel(CMDQ_THR_DISABLED,
	       gce_base + CMDQ_THR_ENABLE_TASK_OFFSET +
	       CMDQ_THR_SHIFT * tid);
	return 0;
}

static u32 *cmdq_task_get_pc_and_inst(const struct cmdq_task *task, int tid,
				      u32 insts[2])
{
	struct cmdq *cqctx;
	void __iomem *gce_base;
	unsigned long pc_pa;
	u8 *pc_va;
	u8 *cmd_end;

	memset(insts, 0, sizeof(u32) * 2);

	if (!task ||
	    !task->va_base ||
	    tid == CMDQ_INVALID_THREAD) {
		pr_err("cmdq get pc failed since invalid param, task 0x%p, task->va_base:0x%p, thread:%d\n",
		       task, task->va_base, tid);
		return NULL;
	}

	cqctx = task->cqctx;
	gce_base = cqctx->base;

	pc_pa = (unsigned long)readl(gce_base + CMDQ_THR_CURR_ADDR_OFFSET +
				     CMDQ_THR_SHIFT * tid);
	pc_va = (u8 *)task->va_base + (pc_pa - task->mva_base);
	cmd_end = (u8 *)(task->va_base + task->num_cmd - 1);

	if (((u8 *)task->va_base <= pc_va) && (pc_va <= cmd_end)) {
		if (pc_va < cmd_end) {
			/* get arg_a and arg_b */
			insts[0] = readl(pc_va);
			insts[1] = readl(pc_va + 4);
		} else {
			/* get arg_a and arg_b of previous cmd */
			insts[0] = readl(pc_va - 8);
			insts[1] = readl(pc_va - 4);
		}
	} else {
		return NULL;
	}

	return (u32 *)pc_va;
}

static const char *cmdq_core_parse_module_from_subsys(u32 arg_a)
{
	int id = (arg_a & CMDQ_ARG_A_SUBSYS_MASK) >> CMDQ_SUBSYS_SHIFT;
	u32 base_addr = cmdq_subsys_id_to_base_addr(id);
	const char *module = cmdq_subsys_base_addr_to_name(base_addr);

	return module ? module : "CMDQ";
}

static const char *cmdq_core_parse_op(u32 op_code)
{
	switch (op_code) {
	case CMDQ_CODE_WRITE:
		return "WRIT";
	case CMDQ_CODE_WFE:
		return "SYNC";
	case CMDQ_CODE_MOVE:
		return "MASK";
	case CMDQ_CODE_JUMP:
		return "JUMP";
	case CMDQ_CODE_EOC:
		return "MARK";
	}
	return NULL;
}

static void cmdq_core_parse_error(struct cmdq_task *task, int tid,
				  const char **module_name, int *flag,
				  u32 *inst_a, u32 *inst_b)
{
	int irq_flag = task->irq_flag;
	u32 insts[2] = { 0 };
	const char *module;

	/*
	 * other cases, use instruction to judge
	 * because engine flag are not sufficient
	 */
	if (cmdq_task_get_pc_and_inst(task, tid, insts)) {
		u32 op, arg_a, arg_b;

		op = insts[1] >> CMDQ_OP_CODE_SHIFT;
		arg_a = insts[1] & CMDQ_ARG_A_MASK;
		arg_b = insts[0];

		switch (op) {
		case CMDQ_CODE_WRITE:
			module = cmdq_core_parse_module_from_subsys(arg_a);
			break;
		case CMDQ_CODE_WFE:
			/* arg_a is the event id */
			module = cmdq_event_get_module((enum cmdq_event)arg_a);
			break;
		case CMDQ_CODE_MOVE:
		case CMDQ_CODE_JUMP:
		case CMDQ_CODE_EOC:
		default:
			module = "CMDQ";
			break;
		}
	} else {
		module = "CMDQ";
	}

	/* fill output parameter */
	*module_name = module;
	*flag = irq_flag;
	*inst_a = insts[1];
	*inst_b = insts[0];
}

static void cmdq_thread_insert_task_by_cookie(struct cmdq_thread *thread,
					      struct cmdq_task *task,
					      int cookie)
{
	thread->wait_cookie = cookie;
	thread->next_cookie = cookie + 1;
	if (thread->next_cookie > CMDQ_MAX_COOKIE_VALUE)
		thread->next_cookie = 0;

	/* first task, so set to 1 */
	thread->task_count = 1;

	thread->cur_task[cookie % CMDQ_MAX_TASK_IN_THREAD] = task;
}

static int cmdq_thread_remove_task_by_index(struct cmdq_thread *thread,
					    int index,
					    enum cmdq_task_state new_state)
{
	struct cmdq_task *task;
	struct device *dev;

	task = thread->cur_task[index];
	if (!task) {
		pr_err("%s: remove fail, task:%d on thread:0x%p is NULL\n",
		       __func__, index, thread);
		return -EINVAL;
	}
	dev = task->cqctx->dev;

	/*
	 * note timing to switch a task to done_status(_ERROR, _KILLED, _DONE)
	 * is aligned with thread's taskcount change
	 * check task status to prevent double clean-up thread's taskcount
	 */
	if (task->task_state != TASK_STATE_BUSY) {
		dev_err(dev, "remove task failed\n");
		dev_err(dev, "state:%d. thread:0x%p, task:%d, new_state:%d\n",
			task->task_state, thread, index, new_state);
		return -EINVAL;
	}

	if (thread->task_count == 0) {
		dev_err(dev, "no task to remove\n");
		dev_err(dev, "thread:%d, index:%d\n", task->thread, index);
		return -EINVAL;
	}

	task->task_state = new_state;
	thread->cur_task[index] = NULL;
	thread->task_count--;

	return 0;
}

static int cmdq_thread_force_remove_task(struct cmdq_task *task, int tid)
{
	struct cmdq *cqctx = task->cqctx;
	struct cmdq_thread *thread = &cqctx->thread[tid];
	void __iomem *gce_base = cqctx->base;
	int status;
	int cookie;
	struct cmdq_task *exec_task;

	status = cmdq_thread_suspend(cqctx, tid);

	writel(CMDQ_THR_NO_TIMEOUT,
	       gce_base + CMDQ_THR_INST_CYCLES_OFFSET +
	       CMDQ_THR_SHIFT * tid);

	/* The cookie of the task currently being processed */
	cookie = cmdq_thread_get_cookie(cqctx, tid) + 1;

	exec_task = thread->cur_task[cookie % CMDQ_MAX_TASK_IN_THREAD];
	if (exec_task && exec_task == task) {
		dma_addr_t eoc_pa = task->mva_base + task->command_size - 16;

		/* The task is executed now, set the PC to EOC for bypass */
		writel(eoc_pa,
		       gce_base + CMDQ_THR_CURR_ADDR_OFFSET +
		       CMDQ_THR_SHIFT * tid);

		thread->cur_task[cookie % CMDQ_MAX_TASK_IN_THREAD] = NULL;
		task->task_state = TASK_STATE_KILLED;
	} else {
		int i, j;

		j = thread->task_count;
		for (i = cookie; j > 0; j--, i++) {
			i %= CMDQ_MAX_TASK_IN_THREAD;

			exec_task = thread->cur_task[i];
			if (!exec_task)
				continue;

			if ((exec_task->va_base[exec_task->num_cmd - 1] ==
			     CMDQ_JUMP_BY_OFFSET) &&
			    (exec_task->va_base[exec_task->num_cmd - 2] ==
			     CMDQ_JUMP_PASS)) {
				/* reached the last task */
				break;
			}

			if (exec_task->va_base[exec_task->num_cmd - 2] ==
			    task->mva_base) {
				/* fake EOC command */
				exec_task->va_base[exec_task->num_cmd - 2] =
					CMDQ_EOC_IRQ_EN;
				exec_task->va_base[exec_task->num_cmd - 1] =
					CMDQ_CODE_EOC << CMDQ_OP_CODE_SHIFT;

				/* bypass the task */
				exec_task->va_base[exec_task->num_cmd] =
					task->va_base[task->num_cmd - 2];
				exec_task->va_base[exec_task->num_cmd + 1] =
					task->va_base[task->num_cmd - 1];

				i = (i + 1) % CMDQ_MAX_TASK_IN_THREAD;

				thread->cur_task[i] = NULL;
				task->task_state = TASK_STATE_KILLED;
				status = 0;
				break;
			}
		}
	}

	return status;
}

static struct cmdq_task *cmdq_thread_search_task_by_pc(
		const struct cmdq_thread *thread, u32 pc)
{
	struct cmdq_task *task;
	int i;

	for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; i++) {
		task = thread->cur_task[i];
		if (task &&
		    pc >= task->mva_base &&
		    pc <= task->mva_base + task->command_size)
			break;
	}

	return task;
}

/*
 * Re-fetch thread's command buffer
 * Use Case:
 *     If SW modifies command buffer content after SW configed commands to GCE,
 *     SW should notify GCE to re-fetch commands in order to
 *     prevent inconsistent command buffer content between DRAM and GCE's SRAM.
 */
static void cmdq_core_invalidate_hw_fetched_buffer(struct cmdq *cqctx,
						   int tid)
{
	void __iomem *pc_va;
	u32 pc;

	/*
	 * Setting HW thread PC will invoke that
	 * GCE (CMDQ HW) gives up fetched command buffer,
	 * and fetch command from DRAM to GCE's SRAM again.
	 */
	pc_va = cqctx->base + CMDQ_THR_CURR_ADDR_OFFSET + CMDQ_THR_SHIFT * tid;
	pc = readl(pc_va);
	writel(pc, pc_va);
}

static int cmdq_task_insert_into_thread(struct cmdq_task *task,
					int tid, int loop)
{
	struct cmdq *cqctx = task->cqctx;
	struct device *dev = cqctx->dev;
	struct cmdq_thread *thread = &cqctx->thread[tid];
	struct cmdq_task *prev_task;
	int index, prev;

	/* find previous task and then link this task behind it */

	index = thread->next_cookie % CMDQ_MAX_TASK_IN_THREAD;
	prev = (index + CMDQ_MAX_TASK_IN_THREAD - 1) % CMDQ_MAX_TASK_IN_THREAD;

	prev_task = thread->cur_task[prev];

	/* maybe the job is killed, search a new one */
	for (; !prev_task && loop > 1; loop--) {
		dev_err(dev,
			"prev_task is NULL, prev:%d, loop:%d, index:%d\n",
			prev, loop, index);

		prev--;
		if (prev < 0)
			prev = CMDQ_MAX_TASK_IN_THREAD - 1;

		prev_task = thread->cur_task[prev];
	}

	if (!prev_task) {
		dev_err(dev,
			"invalid prev_task index:%d, loop:%d\n",
			index, loop);
		return -EFAULT;
	}

	/* insert this task */
	thread->cur_task[index] = task;
	/* let previous task jump to this new task */
	prev_task->va_base[prev_task->num_cmd - 1] = CMDQ_JUMP_BY_PA;
	prev_task->va_base[prev_task->num_cmd - 2] = task->mva_base;

	/* re-fetch command buffer again. */
	cmdq_core_invalidate_hw_fetched_buffer(cqctx, tid);

	return 0;
}

static bool cmdq_command_is_wfe(u32 *cmd)
{
	u32 wfe_option = CMDQ_WFE_UPDATE | CMDQ_WFE_WAIT | CMDQ_WFE_WAIT_VALUE;
	u32 wfe_op = CMDQ_CODE_WFE << CMDQ_OP_CODE_SHIFT;

	return (cmd[0] == wfe_option && (cmd[1] & CMDQ_OP_CODE_MASK) == wfe_op);
}

/* we assume tasks in the same display thread are waiting the same event. */
static void cmdq_task_remove_wfe(struct cmdq_task *task)
{
	u32 *base = task->va_base;
	int i;

	/*
	 * Replace all WFE commands in the task command queue and
	 * replace them with JUMP_PASS.
	 */
	for (i = 0; i < task->num_cmd; i += 2) {
		if (cmdq_command_is_wfe(&base[i])) {
			base[i] = CMDQ_JUMP_PASS;
			base[i + 1] = CMDQ_JUMP_BY_OFFSET;
		}
	}
}

static bool cmdq_thread_is_in_wfe(struct cmdq *cqctx, int tid)
{
	return readl(cqctx->base + CMDQ_THR_WAIT_TOKEN_OFFSET +
		     CMDQ_THR_SHIFT * tid) & CMDQ_THR_IS_WAITING;
}

static void cmdq_thread_wait_end(struct cmdq *cqctx, int tid,
				 unsigned long end_pa)
{
	void __iomem *gce_base = cqctx->base;
	unsigned long curr_pa;

	if (readl_poll_timeout_atomic(
			gce_base + CMDQ_THR_CURR_ADDR_OFFSET +
			CMDQ_THR_SHIFT * tid,
			curr_pa, curr_pa == end_pa, 1, 20)) {
		dev_err(cqctx->dev, "GCE thread(%d) cannot run to end.\n", tid);
	}
}

static int cmdq_task_exec_async_impl(struct cmdq_task *task, int tid)
{
	struct cmdq *cqctx = task->cqctx;
	struct device *dev = cqctx->dev;
	void __iomem *gce_base = cqctx->base;
	int status;
	struct cmdq_thread *thread;
	unsigned long flags;
	int loop;
	int minimum;
	int cookie;

	status = 0;
	thread = &cqctx->thread[tid];

	spin_lock_irqsave(&cqctx->exec_lock, flags);

	/* update task's thread info */
	task->thread = tid;
	task->irq_flag = 0;
	task->task_state = TASK_STATE_BUSY;

	/* case 1. first task for this thread */
	if (thread->task_count <= 0) {
		if (cmdq_thread_reset(cqctx, tid) < 0) {
			spin_unlock_irqrestore(&cqctx->exec_lock, flags);
			return -EFAULT;
		}

		writel(CMDQ_THR_NO_TIMEOUT,
		       gce_base + CMDQ_THR_INST_CYCLES_OFFSET +
		       CMDQ_THR_SHIFT * tid);

		writel(task->mva_base,
		       gce_base + CMDQ_THR_CURR_ADDR_OFFSET +
		       CMDQ_THR_SHIFT * tid);
		writel(task->mva_base + task->command_size,
		       gce_base + CMDQ_THR_END_ADDR_OFFSET +
		       CMDQ_THR_SHIFT * tid);
		writel(CMDQ_THR_PRIORITY,
		       gce_base + CMDQ_THR_CFG_OFFSET +
		       CMDQ_THR_SHIFT * tid);

		writel(CMDQ_THR_IRQ_EN,
		       gce_base + CMDQ_THR_IRQ_ENABLE_OFFSET +
		       CMDQ_THR_SHIFT * tid);

		minimum = cmdq_thread_get_cookie(cqctx, tid);
		cmdq_thread_insert_task_by_cookie(
				thread, task, (minimum + 1));

		/* enable HW thread */
		writel(CMDQ_THR_ENABLED,
		       gce_base + CMDQ_THR_ENABLE_TASK_OFFSET +
		       CMDQ_THR_SHIFT * tid);
	} else {
		unsigned long curr_pa, end_pa;

		status = cmdq_thread_suspend(cqctx, tid);
		if (status < 0) {
			spin_unlock_irqrestore(&cqctx->exec_lock, flags);
			return status;
		}

		writel(CMDQ_THR_NO_TIMEOUT,
		       gce_base + CMDQ_THR_INST_CYCLES_OFFSET +
		       CMDQ_THR_SHIFT * tid);

		cookie = thread->next_cookie;

		curr_pa = (unsigned long)readl(gce_base +
					       CMDQ_THR_CURR_ADDR_OFFSET +
					       CMDQ_THR_SHIFT * tid);
		end_pa = (unsigned long)readl(gce_base +
					      CMDQ_THR_END_ADDR_OFFSET +
					      CMDQ_THR_SHIFT * tid);

		/*
		 * case 2. If already exited WFE, wait for current task to end
		 * and then jump directly to new task.
		 */
		if (!cmdq_thread_is_in_wfe(cqctx, tid)) {
			cmdq_thread_resume(cqctx, tid);
			cmdq_thread_wait_end(cqctx, tid, end_pa);
			status = cmdq_thread_suspend(cqctx, tid);
			if (status < 0) {
				spin_unlock_irqrestore(&cqctx->exec_lock,
						       flags);
				return status;
			}
			/* set to task directly */
			writel(task->mva_base,
			       gce_base + CMDQ_THR_CURR_ADDR_OFFSET +
			       CMDQ_THR_SHIFT * tid);
			writel(task->mva_base + task->command_size,
			       gce_base + CMDQ_THR_END_ADDR_OFFSET +
			       CMDQ_THR_SHIFT * tid);
			thread->cur_task[cookie % CMDQ_MAX_TASK_IN_THREAD] = task;
			thread->task_count++;

		/*
		 * case 3. If thread is still in WFE from previous task, clear
		 * WFE in new task and append to thread.
		 */
		} else {
			/* Current task that shuld be processed */
			minimum = cmdq_thread_get_cookie(cqctx, tid) + 1;
			if (minimum > CMDQ_MAX_COOKIE_VALUE)
				minimum = 0;

			/* Calculate loop count to adjust the tasks' order */
			if (minimum <= cookie)
				loop = cookie - minimum;
			else
				/* Counter wrapped */
				loop = (CMDQ_MAX_COOKIE_VALUE - minimum + 1) +
				       cookie;

			if (loop < 0) {
				dev_err(dev, "reorder fail:\n");
				dev_err(dev, "  task count=%d\n", loop);
				dev_err(dev, "  thread=%d\n", tid);
				dev_err(dev, "  next cookie=%d\n",
					thread->next_cookie);
				dev_err(dev, "  (HW) next cookie=%d\n",
					minimum);
				dev_err(dev, "  task=0x%p\n", task);

				spin_unlock_irqrestore(&cqctx->exec_lock,
						       flags);
				return -EFAULT;
			}

			if (loop > CMDQ_MAX_TASK_IN_THREAD)
				loop %= CMDQ_MAX_TASK_IN_THREAD;

			status = cmdq_task_insert_into_thread(task, tid, loop);
			if (status < 0) {
				spin_unlock_irqrestore(
						&cqctx->exec_lock, flags);
				dev_err(dev,
					"invalid task state for reorder.\n");
				return status;
			}

			cmdq_task_remove_wfe(task);

			smp_mb(); /* modify jump before enable thread */

			writel(task->mva_base + task->command_size,
			       gce_base + CMDQ_THR_END_ADDR_OFFSET +
			       CMDQ_THR_SHIFT * tid);
			thread->task_count++;
		}

		thread->next_cookie += 1;
		if (thread->next_cookie > CMDQ_MAX_COOKIE_VALUE)
			thread->next_cookie = 0;

		/* resume HW thread */
		cmdq_thread_resume(cqctx, tid);
	}

	spin_unlock_irqrestore(&cqctx->exec_lock, flags);

	return status;
}

static void cmdq_core_handle_error(struct cmdq *cqctx, int tid, int value)
{
	struct device *dev = cqctx->dev;
	void __iomem *gce_base = cqctx->base;
	struct cmdq_thread *thread;
	struct cmdq_task *task;
	int cookie;
	int count;
	int inner;
	int status;
	u32 curr_pa, end_pa;

	curr_pa = readl(gce_base + CMDQ_THR_CURR_ADDR_OFFSET +
			CMDQ_THR_SHIFT * tid);
	end_pa = readl(gce_base + CMDQ_THR_END_ADDR_OFFSET +
		       CMDQ_THR_SHIFT * tid);

	dev_err(dev, "IRQ: error thread=%d, irq_flag=0x%x\n", tid, value);
	dev_err(dev, "IRQ: Thread PC: 0x%08x, End PC:0x%08x\n",
		curr_pa, end_pa);

	thread = &cqctx->thread[tid];

	cookie = cmdq_thread_get_cookie(cqctx, tid);

	/*
	 * we assume error happens BEFORE EOC
	 * because it wouldn't be error if this interrupt is issue by EOC.
	 * so we should inc by 1 to locate "current" task
	 */
	cookie++;

	/* set the issued task to error state */
	if (thread->cur_task[cookie % CMDQ_MAX_TASK_IN_THREAD]) {
		task = thread->cur_task[cookie % CMDQ_MAX_TASK_IN_THREAD];
		task->irq_flag = value;
		cmdq_thread_remove_task_by_index(
				thread, cookie % CMDQ_MAX_TASK_IN_THREAD,
				TASK_STATE_ERROR);
	} else {
		dev_err(dev,
			"IRQ: can not find task in %s, pc:0x%08x, end_pc:0x%08x\n",
			__func__, curr_pa, end_pa);
		if (thread->task_count <= 0) {
			/*
			 * suspend HW thread first,
			 * so that we work in a consistent state
			 * outer function should acquire spinlock:
			 *   cqctx->exec_lock
			 */
			status = cmdq_thread_suspend(cqctx, tid);
			if (status < 0)
				dev_err(dev, "IRQ: suspend HW thread failed!");

			cmdq_thread_disable(cqctx, tid);
			dev_err(dev,
				"IRQ: there is no task for thread (%d) %s\n",
				tid, __func__);
		}
	}

	/* set the remain tasks to done state */
	if (thread->wait_cookie <= cookie) {
		count = cookie - thread->wait_cookie + 1;
	} else if ((cookie + 1) % CMDQ_MAX_COOKIE_VALUE ==
			thread->wait_cookie) {
		count = 0;
	} else {
		/* counter wrapped */
		count = (CMDQ_MAX_COOKIE_VALUE - thread->wait_cookie + 1) +
			(cookie + 1);
		dev_err(dev,
			"IRQ: counter wrapped: wait cookie:%d, hw cookie:%d, count=%d",
			thread->wait_cookie, cookie, count);
	}

	for (inner = (thread->wait_cookie % CMDQ_MAX_TASK_IN_THREAD); count > 0;
	     count--, inner++) {
		if (inner >= CMDQ_MAX_TASK_IN_THREAD)
			inner = 0;

		if (thread->cur_task[inner]) {
			task = thread->cur_task[inner];
			task->irq_flag = 0;	/* don't know irq flag */
			/* still call isr_cb to prevent lock */
			if (task->cb.isr_cb)
				task->cb.isr_cb(task->cb.isr_data);
			cmdq_thread_remove_task_by_index(
					thread, inner, TASK_STATE_DONE);
		}
	}

	thread->wait_cookie = cookie + 1;
	if (thread->wait_cookie > CMDQ_MAX_COOKIE_VALUE)
		thread->wait_cookie -= (CMDQ_MAX_COOKIE_VALUE + 1);
			/* min cookie value is 0 */

	wake_up(&cqctx->wait_queue[tid]);
}

static void cmdq_core_handle_done(struct cmdq *cqctx, int tid, int value)
{
	struct device *dev = cqctx->dev;
	struct cmdq_thread *thread = &cqctx->thread[tid];
	int cookie = cmdq_thread_get_cookie(cqctx, tid);
	int count;
	int i;
	struct cmdq_task *task;

	if (thread->wait_cookie <= cookie) {
		count = cookie - thread->wait_cookie + 1;
	} else if ((cookie + 1) % CMDQ_MAX_COOKIE_VALUE ==
			thread->wait_cookie) {
		count = 0;
	} else {
		/* counter wrapped */
		count = (CMDQ_MAX_COOKIE_VALUE - thread->wait_cookie + 1) +
			(cookie + 1);
		dev_err(dev,
			"IRQ: counter wrapped: wait cookie:%d, hw cookie:%d, count=%d",
			thread->wait_cookie, cookie, count);
	}

	for (i = (thread->wait_cookie % CMDQ_MAX_TASK_IN_THREAD); count > 0;
	     count--, i++) {
		if (i >= CMDQ_MAX_TASK_IN_THREAD)
			i = 0;

		if (thread->cur_task[i]) {
			task = thread->cur_task[i];
			task->irq_flag = value;
			if (task->cb.isr_cb)
				task->cb.isr_cb(task->cb.isr_data);
			cmdq_thread_remove_task_by_index(
					thread, i, TASK_STATE_DONE);
		}
	}

	thread->wait_cookie = cookie + 1;
	if (thread->wait_cookie > CMDQ_MAX_COOKIE_VALUE)
		thread->wait_cookie -= (CMDQ_MAX_COOKIE_VALUE + 1);
			/* min cookie value is 0 */

	wake_up(&cqctx->wait_queue[tid]);
}

static void cmdq_core_handle_irq(struct cmdq *cqctx, int tid)
{
	struct device *dev = cqctx->dev;
	void __iomem *gce_base = cqctx->base;
	unsigned long flags = 0L;
	int value;
	int enabled;
	int cookie;

	/*
	 * normal execution, marks tasks done and remove from thread
	 * also, handle "loop CB fail" case
	 */
	spin_lock_irqsave(&cqctx->exec_lock, flags);

	/*
	 * it is possible for another CPU core
	 * to run "release task" right before we acquire the spin lock
	 * and thus reset / disable this HW thread
	 * so we check both the IRQ flag and the enable bit of this thread
	 */
	value = readl(gce_base + CMDQ_THR_IRQ_STATUS_OFFSET +
		      CMDQ_THR_SHIFT * tid);
	if (!(value & CMDQ_THR_IRQ_MASK)) {
		dev_err(dev,
			"IRQ: thread %d got interrupt but IRQ flag is 0x%08x\n",
			tid, value);
		spin_unlock_irqrestore(&cqctx->exec_lock, flags);
		return;
	}

	enabled = readl(gce_base + CMDQ_THR_ENABLE_TASK_OFFSET +
			CMDQ_THR_SHIFT * tid);
	if (!(enabled & CMDQ_THR_ENABLED)) {
		dev_err(dev,
			"IRQ: thread %d got interrupt already disabled 0x%08x\n",
			tid, enabled);
		spin_unlock_irqrestore(&cqctx->exec_lock, flags);
		return;
	}

	/* read HW cookie here for printing message */
	cookie = cmdq_thread_get_cookie(cqctx, tid);

	/*
	 * Move the reset IRQ before read HW cookie
	 * to prevent race condition and save the cost of suspend
	 */
	writel(~value,
	       gce_base + CMDQ_THR_IRQ_STATUS_OFFSET +
	       CMDQ_THR_SHIFT * tid);

	if (value & CMDQ_THR_IRQ_ERROR)
		cmdq_core_handle_error(cqctx, tid, value);
	else if (value & CMDQ_THR_IRQ_DONE)
		cmdq_core_handle_done(cqctx, tid, value);

	spin_unlock_irqrestore(&cqctx->exec_lock, flags);
}

static int cmdq_task_exec_async(struct cmdq_task *task, int tid)
{
	struct device *dev = task->cqctx->dev;
	int status;

	status = cmdq_task_exec_async_impl(task, tid);
	if (status >= 0)
		return status;

	if ((task->task_state == TASK_STATE_KILLED) ||
	    (task->task_state == TASK_STATE_ERROR)) {
		dev_err(dev, "cmdq_task_exec_async_impl fail\n");
		return -EFAULT;
	}

	return 0;
}

static void cmdq_core_consume_waiting_list(struct work_struct *work)
{
	struct list_head *p, *n = NULL;
	bool thread_acquired;
	ktime_t consume_time;
	s64 waiting_time_ns;
	bool need_log;
	struct cmdq *cqctx;
	struct device *dev;
	u32 err_bits = 0;

	cqctx = container_of(work, struct cmdq,
			     task_consume_wait_queue_item);
	dev = cqctx->dev;

	consume_time = ktime_get();

	mutex_lock(&cqctx->task_mutex);

	thread_acquired = false;

	/* scan and remove (if executed) waiting tasks */
	list_for_each_safe(p, n, &cqctx->task_wait_list) {
		struct cmdq_task *task;
		struct cmdq_thread *thread;
		int tid;
		int status;

		task = list_entry(p, struct cmdq_task, list_entry);
		tid = cmdq_eng_get_thread(task->engine_flag);

		waiting_time_ns = ktime_to_ns(
				ktime_sub(consume_time, task->submit));
		need_log = waiting_time_ns >= CMDQ_PREALARM_TIMEOUT_NS;

		/*
		 * Once waiting occur,
		 * skip following tasks to keep order of display tasks.
		 */
		if (err_bits & BIT(tid))
			continue;

		/* acquire HW thread */
		thread = cmdq_core_acquire_thread(cqctx, tid);
		if (!thread) {
			/* have to wait, remain in wait list */
			dev_warn(dev, "acquire thread(%d) fail, need to wait\n",
				 tid);
			if (need_log) /* task wait too long */
				dev_warn(dev, "waiting:%lldns, task:0x%p\n",
					 waiting_time_ns, task);
			err_bits |= BIT(tid);
			continue;
		}

		/* some task is ready to run */
		thread_acquired = true;

		/*
		 * start execution
		 * remove from wait list and put into active list
		 */
		list_move_tail(&task->list_entry,
			       &cqctx->task_active_list);

		/* run task on thread */
		status = cmdq_task_exec_async(task, tid);
		if (status < 0) {
			dev_err(dev, "%s fail, release task 0x%p\n",
				__func__, task);
			cmdq_task_remove_thread(task);
			cmdq_task_release_unlocked(task);
			task = NULL;
		}
	}

	if (thread_acquired) {
		/*
		 * notify some task's sw thread to change their waiting state.
		 * (if they have already called cmdq_task_wait_and_release())
		 */
		wake_up_all(&cqctx->thread_dispatch_queue);
	}

	mutex_unlock(&cqctx->task_mutex);
}

static int cmdq_core_submit_task_async(struct cmdq_command *cmd_desc,
				       struct cmdq_task **task_out,
				       struct cmdq_task_cb *cb)
{
	struct cmdq *cqctx = cmd_desc->cqctx;

	/* creates a new task and put into tail of waiting list */
	*task_out = cmdq_core_acquire_task(cmd_desc, cb);

	if (!(*task_out))
		return -EFAULT;

	/*
	 * Consume the waiting list.
	 * This may or may not execute the task, depending on available threads.
	 */
	cmdq_core_consume_waiting_list(&cqctx->task_consume_wait_queue_item);

	return 0;
}

static int cmdq_core_release_task(struct cmdq_task *task)
{
	struct cmdq *cqctx = task->cqctx;
	int tid = task->thread;
	struct cmdq_thread *thread = &cqctx->thread[tid];
	unsigned long flags;
	int status;

	if (tid != CMDQ_INVALID_THREAD && thread) {
		/* this task is being executed (or queueed) on a hw thread */

		/* get sw lock first to ensure atomic access hw */
		spin_lock_irqsave(&cqctx->exec_lock, flags);
		smp_mb();	/* make sure atomic access hw */

		status = cmdq_thread_force_remove_task(task, tid);
		if (thread->task_count > 0)
			cmdq_thread_resume(cqctx, tid);

		spin_unlock_irqrestore(&cqctx->exec_lock, flags);
		wake_up(&cqctx->wait_queue[tid]);
	}

	cmdq_task_remove_thread(task);
	cmdq_task_release_internal(task);
	return 0;
}

struct cmdq_task_error_report {
	bool throw_err;
	const char *module;
	u32 inst_a;
	u32 inst_b;
	u32 irq_flag;
};

static int cmdq_task_handle_error_result(
		struct cmdq_task *task, int tid, int wait_q,
		struct cmdq_task_error_report *error_report)
{
	struct cmdq *cqctx = task->cqctx;
	struct device *dev = cqctx->dev;
	void __iomem *gce_base = cqctx->base;
	struct cmdq_thread *thread = &cqctx->thread[tid];
	int status = 0;
	int i;
	bool is_err = false;
	struct cmdq_task *next_task;
	struct cmdq_task *prev_task;
	int cookie;
	unsigned long thread_pc;

	dev_err(dev,
		"task(0x%p) state is not TASK_STATE_DONE, but %d.\n",
		task, task->task_state);

	/*
	 * Oops, that task is not done.
	 * We have several possible error cases:
	 * 1. task still running (hang / timeout)
	 * 2. IRQ pending (done or error/timeout IRQ)
	 * 3. task's SW thread has been signaled (e.g. SIGKILL)
	 */

	/*
	 * suspend HW thread first,
	 * so that we work in a consistent state
	 */
	status = cmdq_thread_suspend(cqctx, tid);
	if (status < 0)
		error_report->throw_err = true;

	/* The cookie of the task currently being processed */
	cookie = cmdq_thread_get_cookie(cqctx, tid) + 1;
	thread_pc = (unsigned long)readl(gce_base +
					 CMDQ_THR_CURR_ADDR_OFFSET +
					 CMDQ_THR_SHIFT * tid);

	/* process any pending IRQ */
	error_report->irq_flag = readl(
			gce_base + CMDQ_THR_IRQ_STATUS_OFFSET +
			CMDQ_THR_SHIFT * tid);
	if (error_report->irq_flag & CMDQ_THR_IRQ_ERROR)
		cmdq_core_handle_error(cqctx, tid, error_report->irq_flag);
	else if (error_report->irq_flag & CMDQ_THR_IRQ_DONE)
		cmdq_core_handle_done(cqctx, tid, error_report->irq_flag);

	writel(~error_report->irq_flag,
	       gce_base + CMDQ_THR_IRQ_STATUS_OFFSET +
	       CMDQ_THR_SHIFT * tid);

	/* check if this task has finished after handling pending IRQ */
	if (task->task_state == TASK_STATE_DONE)
		return 0;

	/* Then decide we are SW timeout or SIGNALed (not an error) */
	if (!wait_q) {
		/* SW timeout and no IRQ received */
		is_err = true;
		dev_err(dev, "SW timeout of task 0x%p on tid %d\n",
			task, tid);
		error_report->throw_err = true;
		cmdq_core_parse_error(task, tid,
				      &error_report->module,
				      &error_report->irq_flag,
				      &error_report->inst_a,
				      &error_report->inst_b);
		status = -ETIMEDOUT;
	} else if (wait_q < 0) {
		/*
		 * Task is killed.
		 * Not an error, but still need to remove.
		 */
		is_err = false;

		if (wait_q == -ERESTARTSYS)
			dev_err(dev,
				"Task 0x%p KILLED by wait_q = -ERESTARTSYS\n",
				task);
		else if (wait_q == -EINTR)
			dev_err(dev,
				"Task 0x%p KILLED by wait_q = -EINTR\n",
				task);
		else
			dev_err(dev,
				"Task 0x%p KILLED by wait_q = %d\n",
				task, wait_q);

		status = wait_q;
	}

	if (task->task_state == TASK_STATE_BUSY) {
		/*
		 * if task_state is BUSY,
		 * this means we did not reach EOC,
		 * did not have error IRQ.
		 * - remove the task from thread.cur_task[]
		 * - and decrease thread.task_count
		 * NOTE: after this,
		 * the cur_task will not contain link to task anymore.
		 * and task should become TASK_STATE_ERROR
		 */

		/* we find our place in thread->cur_task[]. */
		for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; i++) {
			if (thread->cur_task[i] == task) {
				/* update task_count and cur_task[] */
				cmdq_thread_remove_task_by_index(
						thread, i, is_err ?
						TASK_STATE_ERROR :
						TASK_STATE_KILLED);
				break;
			}
		}
	}

	next_task = NULL;

	/* find task's jump destination or no next task*/
	if (task->va_base[task->num_cmd - 1] == CMDQ_JUMP_BY_PA)
		next_task = cmdq_thread_search_task_by_pc(
				thread,
				task->va_base[task->num_cmd - 2]);

	/*
	 * Then, we try remove task from the chain of thread->cur_task.
	 * . if HW PC falls in task range
	 * . HW EXEC_CNT += 1
	 * . thread.wait_cookie += 1
	 * . set HW PC to next task head
	 * . if not, find previous task
	 *                (whose jump address is task->mva_base)
	 * . check if HW PC points is not at the EOC/JUMP end
	 * . change jump to fake EOC(no IRQ)
	 * . insert jump to next task head and increase cmd buffer size
	 * . if there is no next task, set HW End Address
	 */
	if (task->num_cmd && thread_pc >= task->mva_base &&
	    thread_pc <= (task->mva_base + task->command_size)) {
		if (next_task) {
			/* cookie already +1 */
			writel(cookie,
			       gce_base + CMDQ_THR_EXEC_CNT_OFFSET +
			       CMDQ_THR_SHIFT * tid);
			thread->wait_cookie = cookie + 1;
			writel(next_task->mva_base,
			       gce_base + CMDQ_THR_CURR_ADDR_OFFSET +
			       CMDQ_THR_SHIFT * tid);
		}
	} else {
		prev_task = NULL;
		for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; i++) {
			u32 *prev_va, *curr_va;
			u32 prev_num, curr_num;

			prev_task = thread->cur_task[i];
			if (!prev_task)
				continue;

			prev_va = prev_task->va_base;
			prev_num = prev_task->num_cmd;
			if (!prev_num)
				continue;

			curr_va = task->va_base;
			curr_num = task->num_cmd;

			/* find which task JUMP into task */
			if (prev_va[prev_num - 2] == task->mva_base &&
			    prev_va[prev_num - 1] == CMDQ_JUMP_BY_PA) {
				/* Copy Jump instruction */
				prev_va[prev_num - 2] =
					curr_va[curr_num - 2];
				prev_va[prev_num - 1] =
					curr_va[curr_num - 1];

				if (next_task)
					cmdq_thread_reorder_task_array(
							thread, i);

				/*
				 * Give up fetched command,
				 * invoke CMDQ HW to re-fetch command.
				 */
				cmdq_core_invalidate_hw_fetched_buffer(
						cqctx, tid);

				break;
			}
		}
	}

	return status;
}

static int cmdq_task_wait_result(struct cmdq_task *task, int tid, int wait_q)
{
	struct cmdq *cqctx = task->cqctx;
	struct cmdq_thread *thread = &cqctx->thread[tid];
	int status = 0;
	unsigned long flags;
	struct cmdq_task_error_report error_report = { 0 };

	/*
	 * Note that although we disable IRQ, HW continues to execute
	 * so it's possible to have pending IRQ
	 */
	spin_lock_irqsave(&cqctx->exec_lock, flags);

	if (task->task_state != TASK_STATE_DONE)
		status = cmdq_task_handle_error_result(
				task, tid, wait_q, &error_report);

	if (thread->task_count <= 0)
		cmdq_thread_disable(cqctx, tid);
	else
		cmdq_thread_resume(cqctx, tid);

	spin_unlock_irqrestore(&cqctx->exec_lock, flags);

	if (error_report.throw_err) {
		u32 op = error_report.inst_a >> CMDQ_OP_CODE_SHIFT;

		switch (op) {
		case CMDQ_CODE_WFE:
			dev_err(cqctx->dev,
				"%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:WAIT EVENT:%s\n",
				error_report.module, error_report.irq_flag,
				error_report.inst_a, error_report.inst_b,
				cmdq_event_get_name(error_report.inst_a &
						    CMDQ_ARG_A_MASK));
			break;
		default:
			dev_err(cqctx->dev,
				"%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:%s\n",
				error_report.module, error_report.irq_flag,
				error_report.inst_a, error_report.inst_b,
				cmdq_core_parse_op(op));
			break;
		}
	}

	return status;
}

static int cmdq_task_wait_done(struct cmdq_task *task)
{
	struct cmdq *cqctx = task->cqctx;
	struct device *dev = cqctx->dev;
	int wait_q;
	int tid;
	unsigned long timeout = msecs_to_jiffies(
			CMDQ_ACQUIRE_THREAD_TIMEOUT_MS);

	/*
	 * wait for acquire thread
	 * (this is done by cmdq_core_consume_waiting_list);
	 */
	wait_q = wait_event_timeout(
			cqctx->thread_dispatch_queue,
			(task->thread != CMDQ_INVALID_THREAD), timeout);

	if (!wait_q) {
		mutex_lock(&cqctx->task_mutex);

		/*
		 * it's possible that the task was just consumed now.
		 * so check again.
		 */
		if (task->thread == CMDQ_INVALID_THREAD) {
			/*
			 * Task may have released,
			 * or starved to death.
			 */
			dev_err(dev,
				"task(0x%p) timeout with invalid thread\n",
				task);

			/*
			 * remove from waiting list,
			 * so that it won't be consumed in the future
			 */
			list_del_init(&task->list_entry);

			mutex_unlock(&cqctx->task_mutex);
			return -EINVAL;
		}

		/* valid thread, so we keep going */
		mutex_unlock(&cqctx->task_mutex);
	}

	tid = task->thread;
	if (tid < 0 || tid >= CMDQ_MAX_THREAD_COUNT) {
		dev_err(dev, "invalid thread %d in %s\n", tid, __func__);
		return -EINVAL;
	}

	/* start to wait */
	wait_q = wait_event_timeout(task->cqctx->wait_queue[tid],
				    (task->task_state != TASK_STATE_BUSY &&
				     task->task_state != TASK_STATE_WAITING),
				    msecs_to_jiffies(CMDQ_DEFAULT_TIMEOUT_MS));
	if (!wait_q)
		dev_dbg(dev, "timeout!\n");

	/* wake up and continue */
	return cmdq_task_wait_result(task, tid, wait_q);
}

static int cmdq_task_wait_and_release(struct cmdq_task *task)
{
	struct cmdq *cqctx;
	int status;

	if (!task) {
		pr_err("%s err ptr=0x%p\n", __func__, task);
		return -EFAULT;
	}

	if (task->task_state == TASK_STATE_IDLE) {
		pr_err("%s task=0x%p is IDLE\n", __func__, task);
		return -EFAULT;
	}

	cqctx = task->cqctx;

	/* wait for task finish */
	status = cmdq_task_wait_done(task);

	/* release */
	cmdq_task_remove_thread(task);
	cmdq_task_release_internal(task);

	return status;
}

static void cmdq_core_auto_release_work(struct work_struct *work_item)
{
	struct cmdq_task *task;
	int status;
	struct cmdq_task_cb cb;

	task = container_of(work_item, struct cmdq_task, auto_release_work);
	cb = task->cb;
	status = cmdq_task_wait_and_release(task);

	/* isr fail, so call isr_cb here to prevent lock */
	if (status && cb.isr_cb)
		cb.isr_cb(cb.isr_data);

	if (cb.done_cb)
		cb.done_cb(cb.done_data);
}

static int cmdq_core_auto_release_task(struct cmdq_task *task)
{
	struct cmdq *cqctx = task->cqctx;

	/*
	 * the work item is embeded in task already
	 * but we need to initialized it
	 */
	INIT_WORK(&task->auto_release_work, cmdq_core_auto_release_work);
	queue_work(cqctx->task_auto_release_wq, &task->auto_release_work);
	return 0;
}

static int cmdq_core_submit_task(struct cmdq_command *cmd_desc)
{
	struct device *dev = cmd_desc->cqctx->dev;
	int status;
	struct cmdq_task *task;

	status = cmdq_core_submit_task_async(cmd_desc, &task, NULL);
	if (status < 0) {
		dev_err(dev, "cmdq_core_submit_task_async failed=%d\n", status);
		return status;
	}

	status = cmdq_task_wait_and_release(task);
	if (status < 0)
		dev_err(dev, "task(0x%p) wait fail\n", task);

	return status;
}

static void cmdq_core_deinitialize(struct platform_device *pdev)
{
	struct cmdq *cqctx = platform_get_drvdata(pdev);
	int i;
	struct list_head *lists[] = {
		&cqctx->task_free_list,
		&cqctx->task_active_list,
		&cqctx->task_wait_list
	};

	/*
	 * Directly destroy the auto release WQ
	 * since we're going to release tasks anyway.
	 */
	destroy_workqueue(cqctx->task_auto_release_wq);
	cqctx->task_auto_release_wq = NULL;

	destroy_workqueue(cqctx->task_consume_wq);
	cqctx->task_consume_wq = NULL;

	/* release all tasks in both list */
	for (i = 0; i < ARRAY_SIZE(lists); i++) {
		struct cmdq_task *task, *tmp;

		list_for_each_entry_safe(task, tmp, lists[i], list_entry) {
			cmdq_task_free_command_buffer(task);
			kmem_cache_free(cqctx->task_cache, task);
			list_del(&task->list_entry);
		}
	}

	kmem_cache_destroy(cqctx->task_cache);
	cqctx->task_cache = NULL;

	/* release command buffer pool */
	cmdq_cmd_buf_pool_uninit(cqctx);
}

static irqreturn_t cmdq_irq_handler(int irq, void *dev)
{
	struct cmdq *cqctx = dev;
	int i;
	u32 irq_status;
	bool handled = false;

	irq_status = readl(cqctx->base + CMDQ_CURR_IRQ_STATUS_OFFSET);
	irq_status &= CMDQ_IRQ_MASK;
	for (i = 0;
	     irq_status != CMDQ_IRQ_MASK && i < CMDQ_MAX_THREAD_COUNT;
	     i++) {
		/* STATUS bit set to 0 means IRQ asserted */
		if (irq_status & BIT(i))
			continue;

		/*
		 * We mark irq_status to 1 to denote finished
		 * processing, and we can early-exit if no more
		 * threads being asserted.
		 */
		irq_status |= BIT(i);

		cmdq_core_handle_irq(cqctx, i);
		handled = true;
	}

	if (!handled)
		return IRQ_NONE;

	queue_work(cqctx->task_consume_wq,
		   &cqctx->task_consume_wait_queue_item);
	return IRQ_HANDLED;
}

static int cmdq_core_initialize(struct platform_device *pdev,
				struct cmdq **cqctx)
{
	struct cmdq *lcqctx; /* local cmdq context */
	int i;
	int ret = 0;

	lcqctx = devm_kzalloc(&pdev->dev, sizeof(*lcqctx), GFP_KERNEL);

	/* save dev */
	lcqctx->dev = &pdev->dev;

	/* initial cmdq device related data */
	ret = cmdq_dev_init(pdev, lcqctx);
	if (ret) {
		dev_err(&pdev->dev, "failed to init cmdq device\n");
		goto fail_dev;
	}

	/* initial mutex, spinlock */
	mutex_init(&lcqctx->task_mutex);
	mutex_init(&lcqctx->clock_mutex);
	spin_lock_init(&lcqctx->thread_lock);
	spin_lock_init(&lcqctx->exec_lock);

	/* initial wait queue for notification */
	for (i = 0; i < ARRAY_SIZE(lcqctx->wait_queue); i++)
		init_waitqueue_head(&lcqctx->wait_queue[i]);
	init_waitqueue_head(&lcqctx->thread_dispatch_queue);

	/* create task pool */
	lcqctx->task_cache = kmem_cache_create(
			CMDQ_DRIVER_DEVICE_NAME "_task",
			sizeof(struct cmdq_task),
			__alignof__(struct cmdq_task),
			SLAB_POISON | SLAB_HWCACHE_ALIGN | SLAB_RED_ZONE,
			&cmdq_task_ctor);

	/* initialize task lists */
	INIT_LIST_HEAD(&lcqctx->task_free_list);
	INIT_LIST_HEAD(&lcqctx->task_active_list);
	INIT_LIST_HEAD(&lcqctx->task_wait_list);
	INIT_WORK(&lcqctx->task_consume_wait_queue_item,
		  cmdq_core_consume_waiting_list);

	/* initialize command buffer pool */
	ret = cmdq_cmd_buf_pool_init(lcqctx);
	if (ret) {
		dev_err(&pdev->dev, "failed to init command buffer pool\n");
		goto fail_cmd_buf_pool;
	}

	lcqctx->task_auto_release_wq = alloc_ordered_workqueue(
			"%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "cmdq_auto_release");
	lcqctx->task_consume_wq = alloc_ordered_workqueue(
			"%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "cmdq_task");

	*cqctx = lcqctx;
	return ret;

fail_cmd_buf_pool:
	destroy_workqueue(lcqctx->task_auto_release_wq);
	destroy_workqueue(lcqctx->task_consume_wq);
	kmem_cache_destroy(lcqctx->task_cache);

fail_dev:
	return ret;
}

static int cmdq_rec_realloc_cmd_buffer(struct cmdq_rec *handle, size_t size)
{
	void *new_buf;

	new_buf = krealloc(handle->buf_ptr, size, GFP_KERNEL | __GFP_ZERO);
	if (!new_buf)
		return -ENOMEM;
	handle->buf_ptr = new_buf;
	handle->buf_size = size;
	return 0;
}

static int cmdq_rec_stop_running_task(struct cmdq_rec *handle)
{
	int status;

	status = cmdq_core_release_task(handle->running_task_ptr);
	handle->running_task_ptr = NULL;
	return status;
}

int cmdq_rec_create(struct device *dev, u64 engine_flag,
		    struct cmdq_rec **handle_ptr)
{
	struct cmdq *cqctx;
	struct cmdq_rec *handle;
	int ret;

	cqctx = dev_get_drvdata(dev);
	if (!cqctx) {
		dev_err(dev, "cmdq context is NULL\n");
		return -EINVAL;
	}

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->cqctx = dev_get_drvdata(dev);
	handle->engine_flag = engine_flag;

	ret = cmdq_rec_realloc_cmd_buffer(handle, CMDQ_INITIAL_CMD_BLOCK_SIZE);
	if (ret) {
		kfree(handle);
		return ret;
	}

	*handle_ptr = handle;

	return 0;
}
EXPORT_SYMBOL(cmdq_rec_create);

static int cmdq_rec_append_command(struct cmdq_rec *handle,
				   enum cmdq_code code,
				   u32 arg_a, u32 arg_b)
{
	struct cmdq *cqctx;
	struct device *dev;
	int subsys;
	u32 *cmd_ptr;
	int ret;

	cqctx = handle->cqctx;
	dev = cqctx->dev;
	cmd_ptr = (u32 *)((u8 *)handle->buf_ptr + handle->block_size);

	if (handle->finalized) {
		dev_err(dev,
			"already finalized record(cannot add more command)");
		dev_err(dev, "handle=0x%p, tid=%d\n", handle, current->pid);
		return -EBUSY;
	}

	/* check if we have sufficient buffer size */
	if (unlikely(handle->block_size + CMDQ_INST_SIZE > handle->buf_size)) {
		ret = cmdq_rec_realloc_cmd_buffer(handle, handle->buf_size * 2);
		if (ret)
			return ret;
	}

	/*
	 * we must re-calculate current PC
	 * because we may already insert MARKER inst.
	 */
	cmd_ptr = (u32 *)((u8 *)handle->buf_ptr + handle->block_size);

	switch (code) {
	case CMDQ_CODE_MOVE:
		cmd_ptr[0] = arg_b;
		cmd_ptr[1] = (CMDQ_CODE_MOVE << CMDQ_OP_CODE_SHIFT) |
			     (arg_a & CMDQ_ARG_A_MASK);
		break;
	case CMDQ_CODE_WRITE:
		subsys = cmdq_subsys_from_phys_addr(cqctx, arg_a);
		if (subsys < 0) {
			dev_err(dev,
				"unsupported memory base address 0x%08x\n",
				arg_a);
			return -EFAULT;
		}

		cmd_ptr[0] = arg_b;
		cmd_ptr[1] = (CMDQ_CODE_WRITE << CMDQ_OP_CODE_SHIFT) |
			     (arg_a & CMDQ_ARG_A_WRITE_MASK) |
			     ((subsys & CMDQ_SUBSYS_MASK) << CMDQ_SUBSYS_SHIFT);
		break;
	case CMDQ_CODE_JUMP:
		cmd_ptr[0] = arg_b;
		cmd_ptr[1] = (CMDQ_CODE_JUMP << CMDQ_OP_CODE_SHIFT) |
			     (arg_a & CMDQ_ARG_A_MASK);
		break;
	case CMDQ_CODE_WFE:
		/*
		 * bit 0-11: wait_value, 1
		 * bit 15: to_wait, true
		 * bit 16-27: update_value, 0
		 * bit 31: to_update, true
		 */
		cmd_ptr[0] = CMDQ_WFE_UPDATE | CMDQ_WFE_WAIT |
			     CMDQ_WFE_WAIT_VALUE;
		cmd_ptr[1] = (CMDQ_CODE_WFE << CMDQ_OP_CODE_SHIFT) | arg_a;
		break;
	case CMDQ_CODE_CLEAR_EVENT:
		/*
		 * bit 0-11: wait_value, 0
		 * bit 15: to_wait, false
		 * bit 16-27: update_value, 0
		 * bit 31: to_update, true
		 */
		cmd_ptr[0] = CMDQ_WFE_UPDATE;
		cmd_ptr[1] = (CMDQ_CODE_WFE << CMDQ_OP_CODE_SHIFT) | arg_a;
		break;
	case CMDQ_CODE_EOC:
		cmd_ptr[0] = arg_b;
		cmd_ptr[1] = (CMDQ_CODE_EOC << CMDQ_OP_CODE_SHIFT) |
			     (arg_a & CMDQ_ARG_A_MASK);
		break;
	default:
		return -EFAULT;
	}

	handle->block_size += CMDQ_INST_SIZE;

	return 0;
}

int cmdq_rec_reset(struct cmdq_rec *handle)
{
	if (handle->running_task_ptr)
		cmdq_rec_stop_running_task(handle);

	handle->block_size = 0;
	handle->finalized = false;

	return 0;
}
EXPORT_SYMBOL(cmdq_rec_reset);

int cmdq_rec_write(struct cmdq_rec *handle, u32 value, u32 addr)
{
	return cmdq_rec_append_command(handle, CMDQ_CODE_WRITE, addr, value);
}
EXPORT_SYMBOL(cmdq_rec_write);

int cmdq_rec_write_mask(struct cmdq_rec *handle, u32 value,
			u32 addr, u32 mask)
{
	int ret;

	if (mask != 0xffffffff) {
		ret = cmdq_rec_append_command(handle, CMDQ_CODE_MOVE, 0, ~mask);
		if (ret)
			return ret;

		addr = addr | CMDQ_ENABLE_MASK;
	}

	return cmdq_rec_append_command(handle, CMDQ_CODE_WRITE, addr, value);
}
EXPORT_SYMBOL(cmdq_rec_write_mask);

int cmdq_rec_wait(struct cmdq_rec *handle, enum cmdq_event event)
{
	if (event == CMDQ_SYNC_TOKEN_INVALID || event >= CMDQ_SYNC_TOKEN_MAX ||
	    event < 0)
		return -EINVAL;

	return cmdq_rec_append_command(handle, CMDQ_CODE_WFE, event, 0);
}
EXPORT_SYMBOL(cmdq_rec_wait);

int cmdq_rec_clear_event(struct cmdq_rec *handle, enum cmdq_event event)
{
	if (event == CMDQ_SYNC_TOKEN_INVALID || event >= CMDQ_SYNC_TOKEN_MAX ||
	    event < 0)
		return -EINVAL;

	return cmdq_rec_append_command(handle, CMDQ_CODE_CLEAR_EVENT, event, 0);
}
EXPORT_SYMBOL(cmdq_rec_clear_event);

static int cmdq_rec_finalize_command(struct cmdq_rec *handle)
{
	int status;
	struct device *dev;
	u32 arg_b;

	dev = handle->cqctx->dev;

	if (!handle->finalized) {
		/* insert EOC and generate IRQ for each command iteration */
		arg_b = CMDQ_EOC_IRQ_EN;
		status = cmdq_rec_append_command(handle, CMDQ_CODE_EOC,
						 0, arg_b);
		if (status)
			return status;

		/* JUMP to begin */
		status = cmdq_rec_append_command(handle, CMDQ_CODE_JUMP, 0, 8);
		if (status)
			return status;

		handle->finalized = true;
	}

	return 0;
}

static int cmdq_rec_fill_cmd_desc(struct cmdq_rec *handle,
				  struct cmdq_command *desc)
{
	int ret;

	ret = cmdq_rec_finalize_command(handle);
	if (ret)
		return ret;

	desc->cqctx = handle->cqctx;
	desc->engine_flag = handle->engine_flag;
	desc->va_base = handle->buf_ptr;
	desc->block_size = handle->block_size;

	return ret;
}

int cmdq_rec_flush(struct cmdq_rec *handle)
{
	int ret;
	struct cmdq_command desc;

	ret = cmdq_rec_fill_cmd_desc(handle, &desc);
	if (ret)
		return ret;

	return cmdq_core_submit_task(&desc);
}
EXPORT_SYMBOL(cmdq_rec_flush);

static int cmdq_rec_flush_async_cb(struct cmdq_rec *handle,
				   cmdq_async_flush_cb isr_cb,
				   void *isr_data,
				   cmdq_async_flush_cb done_cb,
				   void *done_data)
{
	int ret;
	struct cmdq_command desc;
	struct cmdq_task *task;
	struct cmdq_task_cb cb;

	ret = cmdq_rec_fill_cmd_desc(handle, &desc);
	if (ret)
		return ret;

	cb.isr_cb = isr_cb;
	cb.isr_data = isr_data;
	cb.done_cb = done_cb;
	cb.done_data = done_data;

	ret = cmdq_core_submit_task_async(&desc, &task, &cb);
	if (ret)
		return ret;

	ret = cmdq_core_auto_release_task(task);

	return ret;
}

int cmdq_rec_flush_async(struct cmdq_rec *handle)
{
	return cmdq_rec_flush_async_cb(handle, NULL, NULL, NULL, NULL);
}
EXPORT_SYMBOL(cmdq_rec_flush_async);

int cmdq_rec_flush_async_callback(struct cmdq_rec *handle,
				  cmdq_async_flush_cb isr_cb,
				  void *isr_data,
				  cmdq_async_flush_cb done_cb,
				  void *done_data)
{
	return cmdq_rec_flush_async_cb(handle, isr_cb, isr_data,
				       done_cb, done_data);
}
EXPORT_SYMBOL(cmdq_rec_flush_async_callback);

void cmdq_rec_destroy(struct cmdq_rec *handle)
{
	if (handle->running_task_ptr)
		cmdq_rec_stop_running_task(handle);

	/* free command buffer */
	kfree(handle->buf_ptr);
	handle->buf_ptr = NULL;

	/* free command handle */
	kfree(handle);
}
EXPORT_SYMBOL(cmdq_rec_destroy);

static int cmdq_probe(struct platform_device *pdev)
{
	struct cmdq *cqctx;
	int ret;

	/* init cmdq context, and save it */
	ret = cmdq_core_initialize(pdev, &cqctx);
	if (ret) {
		dev_err(&pdev->dev, "failed to init cmdq context\n");
		return ret;
	}
	platform_set_drvdata(pdev, cqctx);

	ret = devm_request_irq(&pdev->dev, cqctx->irq, cmdq_irq_handler,
			       IRQF_TRIGGER_LOW | IRQF_SHARED,
			       CMDQ_DRIVER_DEVICE_NAME, cqctx);
	if (ret) {
		dev_err(&pdev->dev, "failed to register ISR (%d)\n", ret);
		goto fail;
	}

	cqctx->clock = devm_clk_get(&pdev->dev, CMDQ_CLK_NAME);
	if (IS_ERR(cqctx->clock)) {
		dev_err(&pdev->dev, "failed to get clk:%s\n", CMDQ_CLK_NAME);
		ret = PTR_ERR(cqctx->clock);
		goto fail;
	}

	return ret;

fail:
	cmdq_core_deinitialize(pdev);
	return ret;
}

static int cmdq_remove(struct platform_device *pdev)
{
	cmdq_core_deinitialize(pdev);
	return 0;
}

static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mt8173-gce",},
	{}
};

static struct platform_driver cmdq_drv = {
	.probe = cmdq_probe,
	.remove = cmdq_remove,
	.driver = {
		.name = CMDQ_DRIVER_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cmdq_of_ids,
	}
};

builtin_platform_driver(cmdq_drv);
