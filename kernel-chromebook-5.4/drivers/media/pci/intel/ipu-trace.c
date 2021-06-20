// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2014 - 2020 Intel Corporation

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/sizes.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "ipu.h"
#include "ipu-platform-regs.h"
#include "ipu-trace.h"

/* Input data processing states */
enum config_file_parse_states {
	STATE_FILL = 0,
	STATE_COMMENT,
	STATE_COMPLETE,
};

struct trace_register_range {
	u32 start;
	u32 end;
};

static u16 trace_unit_template[] = TRACE_REG_CREATE_TUN_REGISTER_LIST;
static u16 trace_monitor_template[] = TRACE_REG_CREATE_TM_REGISTER_LIST;
static u16 trace_gpc_template[] = TRACE_REG_CREATE_GPC_REGISTER_LIST;

static struct trace_register_range trace_csi2_range_template[] = {
	{
	 .start = TRACE_REG_CSI2_TM_RESET_REG_IDX,
	 .end = TRACE_REG_CSI2_TM_IRQ_ENABLE_REG_ID_N(7)
	},
	{
	 .start = TRACE_REG_END_MARK,
	 .end = TRACE_REG_END_MARK
	}
};

static struct trace_register_range trace_csi2_3ph_range_template[] = {
	{
	 .start = TRACE_REG_CSI2_3PH_TM_RESET_REG_IDX,
	 .end = TRACE_REG_CSI2_3PH_TM_IRQ_ENABLE_REG_ID_N(7)
	},
	{
	 .start = TRACE_REG_END_MARK,
	 .end = TRACE_REG_END_MARK
	}
};

static struct trace_register_range trace_sig2cio_range_template[] = {
	{
	 .start = TRACE_REG_SIG2CIO_ADDRESS,
	 .end = (TRACE_REG_SIG2CIO_STATUS + 8 * TRACE_REG_SIG2CIO_SIZE_OF)
	},
	{
	 .start = TRACE_REG_END_MARK,
	 .end = TRACE_REG_END_MARK
	}
};

#define LINE_MAX_LEN			128
#define MEMORY_RING_BUFFER_SIZE		(SZ_1M * 32)
#define TRACE_MESSAGE_SIZE		16
/*
 * It looks that the trace unit sometimes writes outside the given buffer.
 * To avoid memory corruption one extra page is reserved at the end
 * of the buffer. Read also the extra area since it may contain valid data.
 */
#define MEMORY_RING_BUFFER_GUARD	PAGE_SIZE
#define MEMORY_RING_BUFFER_OVERREAD	MEMORY_RING_BUFFER_GUARD
#define MAX_TRACE_REGISTERS		200
#define TRACE_CONF_DUMP_BUFFER_SIZE	(MAX_TRACE_REGISTERS * 2 * 32)

#define IPU_TRACE_TIME_RETRY	5

struct config_value {
	u32 reg;
	u32 value;
};

struct ipu_trace_buffer {
	dma_addr_t dma_handle;
	void *memory_buffer;
};

struct ipu_subsystem_trace_config {
	u32 offset;
	void __iomem *base;
	struct ipu_trace_buffer memory;	/* ring buffer */
	struct device *dev;
	struct ipu_trace_block *blocks;
	unsigned int fill_level;	/* Nbr of regs in config table below */
	bool running;
	/* Cached register values  */
	struct config_value config[MAX_TRACE_REGISTERS];
};

/*
 * State of the input data processing is kept in this structure.
 * Only one user is supported at time.
 */
struct buf_state {
	char line_buffer[LINE_MAX_LEN];
	enum config_file_parse_states state;
	int offset;	/* Offset to line_buffer */
};

struct ipu_trace {
	struct mutex lock; /* Protect ipu trace operations */
	bool open;
	char *conf_dump_buffer;
	int size_conf_dump;
	struct buf_state buffer_state;

	struct ipu_subsystem_trace_config isys;
	struct ipu_subsystem_trace_config psys;
};

static void __ipu_trace_restore(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_device *isp = adev->isp;
	struct ipu_trace *trace = isp->trace;
	struct config_value *config;
	struct ipu_subsystem_trace_config *sys = adev->trace_cfg;
	struct ipu_trace_block *blocks;
	u32 mapped_trace_buffer;
	void __iomem *addr = NULL;
	int i;

	if (trace->open) {
		dev_info(dev, "Trace control file open. Skipping update\n");
		return;
	}

	if (!sys)
		return;

	/* leave if no trace configuration for this subsystem */
	if (sys->fill_level == 0)
		return;

	/* Find trace unit base address */
	blocks = sys->blocks;
	while (blocks->type != IPU_TRACE_BLOCK_END) {
		if (blocks->type == IPU_TRACE_BLOCK_TUN) {
			addr = sys->base + blocks->offset;
			break;
		}
		blocks++;
	}
	if (!addr)
		return;

	if (!sys->memory.memory_buffer) {
		sys->memory.memory_buffer =
		    dma_alloc_coherent(dev, MEMORY_RING_BUFFER_SIZE +
				       MEMORY_RING_BUFFER_GUARD,
				       &sys->memory.dma_handle,
				       GFP_KERNEL);
	}

	if (!sys->memory.memory_buffer) {
		dev_err(dev, "No memory for tracing. Trace unit disabled\n");
		return;
	}

	config = sys->config;
	mapped_trace_buffer = sys->memory.dma_handle;

	/* ring buffer base */
	writel(mapped_trace_buffer, addr + TRACE_REG_TUN_DRAM_BASE_ADDR);

	/* ring buffer end */
	writel(mapped_trace_buffer + MEMORY_RING_BUFFER_SIZE -
		   TRACE_MESSAGE_SIZE, addr + TRACE_REG_TUN_DRAM_END_ADDR);

	/* Infobits for ddr trace */
	writel(IPU_INFO_REQUEST_DESTINATION_PRIMARY,
	       addr + TRACE_REG_TUN_DDR_INFO_VAL);

	/* Find trace timer reset address */
	addr = NULL;
	blocks = sys->blocks;
	while (blocks->type != IPU_TRACE_BLOCK_END) {
		if (blocks->type == IPU_TRACE_TIMER_RST) {
			addr = sys->base + blocks->offset;
			break;
		}
		blocks++;
	}
	if (!addr) {
		dev_err(dev, "No trace reset addr\n");
		return;
	}

	/* Remove reset from trace timers */
	writel(TRACE_REG_GPREG_TRACE_TIMER_RST_OFF, addr);

	/* Register config received from userspace */
	for (i = 0; i < sys->fill_level; i++) {
		dev_dbg(dev,
			"Trace restore: reg 0x%08x, value 0x%08x\n",
			config[i].reg, config[i].value);
		writel(config[i].value, isp->base + config[i].reg);
	}

	sys->running = true;
}

void ipu_trace_restore(struct device *dev)
{
	struct ipu_trace *trace = to_ipu_bus_device(dev)->isp->trace;

	if (!trace)
		return;

	mutex_lock(&trace->lock);
	__ipu_trace_restore(dev);
	mutex_unlock(&trace->lock);
}
EXPORT_SYMBOL_GPL(ipu_trace_restore);

static void __ipu_trace_stop(struct device *dev)
{
	struct ipu_subsystem_trace_config *sys =
	    to_ipu_bus_device(dev)->trace_cfg;
	struct ipu_trace_block *blocks;

	if (!sys)
		return;

	if (!sys->running)
		return;
	sys->running = false;

	/* Turn off all the gpc blocks */
	blocks = sys->blocks;
	while (blocks->type != IPU_TRACE_BLOCK_END) {
		if (blocks->type == IPU_TRACE_BLOCK_GPC) {
			writel(0, sys->base + blocks->offset +
				   TRACE_REG_GPC_OVERALL_ENABLE);
		}
		blocks++;
	}

	/* Turn off all the trace monitors */
	blocks = sys->blocks;
	while (blocks->type != IPU_TRACE_BLOCK_END) {
		if (blocks->type == IPU_TRACE_BLOCK_TM) {
			writel(0, sys->base + blocks->offset +
				   TRACE_REG_TM_TRACE_ENABLE_NPK);

			writel(0, sys->base + blocks->offset +
				   TRACE_REG_TM_TRACE_ENABLE_DDR);
		}
		blocks++;
	}

	/* Turn off trace units */
	blocks = sys->blocks;
	while (blocks->type != IPU_TRACE_BLOCK_END) {
		if (blocks->type == IPU_TRACE_BLOCK_TUN) {
			writel(0, sys->base + blocks->offset +
				   TRACE_REG_TUN_DDR_ENABLE);
			writel(0, sys->base + blocks->offset +
				   TRACE_REG_TUN_NPK_ENABLE);
		}
		blocks++;
	}
}

void ipu_trace_stop(struct device *dev)
{
	struct ipu_trace *trace = to_ipu_bus_device(dev)->isp->trace;

	if (!trace)
		return;

	mutex_lock(&trace->lock);
	__ipu_trace_stop(dev);
	mutex_unlock(&trace->lock);
}
EXPORT_SYMBOL_GPL(ipu_trace_stop);

static int validate_register(u32 base, u32 reg, u16 *template)
{
	int i = 0;

	while (template[i] != TRACE_REG_END_MARK) {
		if (template[i] + base != reg) {
			i++;
			continue;
		}
		/* This is a valid register */
		return 0;
	}
	return -EINVAL;
}

static int validate_register_range(u32 base, u32 reg,
				   struct trace_register_range *template)
{
	unsigned int i = 0;

	if (!IS_ALIGNED(reg, sizeof(u32)))
		return -EINVAL;

	while (template[i].start != TRACE_REG_END_MARK) {
		if ((reg < template[i].start + base) ||
		    (reg > template[i].end + base)) {
			i++;
			continue;
		}
		/* This is a valid register */
		return 0;
	}
	return -EINVAL;
}

static int update_register_cache(struct ipu_device *isp, u32 reg, u32 value)
{
	struct ipu_trace *dctrl = isp->trace;
	const struct ipu_trace_block *blocks;
	struct ipu_subsystem_trace_config *sys;
	struct device *dev;
	u32 base = 0;
	u16 *template = NULL;
	struct trace_register_range *template_range = NULL;
	int i, range;
	int rval = -EINVAL;

	if (dctrl->isys.offset == dctrl->psys.offset) {
		/* For the IPU with uniform address space */
		if (reg >= IPU_ISYS_OFFSET &&
		    reg < IPU_ISYS_OFFSET + TRACE_REG_MAX_ISYS_OFFSET)
			sys = &dctrl->isys;
		else if (reg >= IPU_PSYS_OFFSET &&
			 reg < IPU_PSYS_OFFSET + TRACE_REG_MAX_PSYS_OFFSET)
			sys = &dctrl->psys;
		else
			goto error;
	} else {
		if (dctrl->isys.offset &&
		    reg >= dctrl->isys.offset &&
		    reg < dctrl->isys.offset + TRACE_REG_MAX_ISYS_OFFSET)
			sys = &dctrl->isys;
		else if (dctrl->psys.offset &&
			 reg >= dctrl->psys.offset &&
			 reg < dctrl->psys.offset + TRACE_REG_MAX_PSYS_OFFSET)
			sys = &dctrl->psys;
		else
			goto error;
	}

	blocks = sys->blocks;
	dev = sys->dev;

	/* Check registers block by block */
	i = 0;
	while (blocks[i].type != IPU_TRACE_BLOCK_END) {
		base = blocks[i].offset + sys->offset;
		if ((reg >= base && reg < base + TRACE_REG_MAX_BLOCK_SIZE))
			break;
		i++;
	}

	range = 0;
	switch (blocks[i].type) {
	case IPU_TRACE_BLOCK_TUN:
		template = trace_unit_template;
		break;
	case IPU_TRACE_BLOCK_TM:
		template = trace_monitor_template;
		break;
	case IPU_TRACE_BLOCK_GPC:
		template = trace_gpc_template;
		break;
	case IPU_TRACE_CSI2:
		range = 1;
		template_range = trace_csi2_range_template;
		break;
	case IPU_TRACE_CSI2_3PH:
		range = 1;
		template_range = trace_csi2_3ph_range_template;
		break;
	case IPU_TRACE_SIG2CIOS:
		range = 1;
		template_range = trace_sig2cio_range_template;
		break;
	default:
		goto error;
	}

	if (range)
		rval = validate_register_range(base, reg, template_range);
	else
		rval = validate_register(base, reg, template);

	if (rval)
		goto error;

	if (sys->fill_level < MAX_TRACE_REGISTERS) {
		dev_dbg(dev,
			"Trace reg addr 0x%08x value 0x%08x\n", reg, value);
		sys->config[sys->fill_level].reg = reg;
		sys->config[sys->fill_level].value = value;
		sys->fill_level++;
	} else {
		rval = -ENOMEM;
		goto error;
	}
	return 0;
error:
	dev_info(&isp->pdev->dev,
		 "Trace register address 0x%08x ignored as invalid register\n",
		 reg);
	return rval;
}

/*
 * We don't know how much data is received this time. Process given data
 * character by character.
 * Fill the line buffer until either
 * 1) new line is got -> go to decode
 * or
 * 2) line_buffer is full -> ignore rest of line and then try to decode
 * or
 * 3) Comment mark is found -> ignore rest of the line and then try to decode
 *    the data which was received before the comment mark
 *
 * Decode phase tries to find "reg = value" pairs and validates those
 */
static int process_buffer(struct ipu_device *isp,
			  char *buffer, int size, struct buf_state *state)
{
	int i, ret;
	int curr_state = state->state;
	u32 reg, value;

	for (i = 0; i < size; i++) {
		/*
		 * Comment mark in any position turns on comment mode
		 * until end of line
		 */
		if (curr_state != STATE_COMMENT && buffer[i] == '#') {
			state->line_buffer[state->offset] = '\0';
			curr_state = STATE_COMMENT;
			continue;
		}

		switch (curr_state) {
		case STATE_COMMENT:
			/* Only new line can break this mode */
			if (buffer[i] == '\n')
				curr_state = STATE_COMPLETE;
			break;
		case STATE_FILL:
			state->line_buffer[state->offset] = buffer[i];
			state->offset++;

			if (state->offset >= sizeof(state->line_buffer) - 1) {
				/* Line buffer full - ignore rest */
				state->line_buffer[state->offset] = '\0';
				curr_state = STATE_COMMENT;
				break;
			}

			if (buffer[i] == '\n') {
				state->line_buffer[state->offset] = '\0';
				curr_state = STATE_COMPLETE;
			}
			break;
		default:
			state->offset = 0;
			state->line_buffer[state->offset] = '\0';
			curr_state = STATE_COMMENT;
		}

		if (curr_state == STATE_COMPLETE) {
			ret = sscanf(state->line_buffer, "%x = %x",
				     &reg, &value);
			if (ret == 2)
				update_register_cache(isp, reg, value);

			state->offset = 0;
			curr_state = STATE_FILL;
		}
	}
	state->state = curr_state;
	return 0;
}

static void traceconf_dump(struct ipu_device *isp)
{
	struct ipu_subsystem_trace_config *sys[2] = {
		&isp->trace->isys,
		&isp->trace->psys
	};
	int i, j, rem_size;
	char *out;

	isp->trace->size_conf_dump = 0;
	out = isp->trace->conf_dump_buffer;
	rem_size = TRACE_CONF_DUMP_BUFFER_SIZE;

	for (j = 0; j < ARRAY_SIZE(sys); j++) {
		for (i = 0; i < sys[j]->fill_level && rem_size > 0; i++) {
			int bytes_print;
			int n = snprintf(out, rem_size, "0x%08x = 0x%08x\n",
					 sys[j]->config[i].reg,
					 sys[j]->config[i].value);

			bytes_print = min(n, rem_size - 1);
			rem_size -= bytes_print;
			out += bytes_print;
		}
	}
	isp->trace->size_conf_dump = out - isp->trace->conf_dump_buffer;
}

static void clear_trace_buffer(struct ipu_subsystem_trace_config *sys)
{
	if (!sys->memory.memory_buffer)
		return;

	memset(sys->memory.memory_buffer, 0, MEMORY_RING_BUFFER_SIZE +
	       MEMORY_RING_BUFFER_OVERREAD);

	dma_sync_single_for_device(sys->dev,
				   sys->memory.dma_handle,
				   MEMORY_RING_BUFFER_SIZE +
				   MEMORY_RING_BUFFER_GUARD, DMA_FROM_DEVICE);
}

static int traceconf_open(struct inode *inode, struct file *file)
{
	int ret;
	struct ipu_device *isp;

	if (!inode->i_private)
		return -EACCES;

	isp = inode->i_private;

	ret = mutex_trylock(&isp->trace->lock);
	if (!ret)
		return -EBUSY;

	if (isp->trace->open) {
		mutex_unlock(&isp->trace->lock);
		return -EBUSY;
	}

	file->private_data = isp;
	isp->trace->open = 1;
	if (file->f_mode & FMODE_WRITE) {
		/* TBD: Allocate temp buffer for processing.
		 * Push validated buffer to active config
		 */

		/* Forget old config if opened for write */
		isp->trace->isys.fill_level = 0;
		isp->trace->psys.fill_level = 0;
	}

	if (file->f_mode & FMODE_READ) {
		isp->trace->conf_dump_buffer =
		    vzalloc(TRACE_CONF_DUMP_BUFFER_SIZE);
		if (!isp->trace->conf_dump_buffer) {
			isp->trace->open = 0;
			mutex_unlock(&isp->trace->lock);
			return -ENOMEM;
		}
		traceconf_dump(isp);
	}
	mutex_unlock(&isp->trace->lock);
	return 0;
}

static ssize_t traceconf_read(struct file *file, char __user *buf,
			      size_t len, loff_t *ppos)
{
	struct ipu_device *isp = file->private_data;

	return simple_read_from_buffer(buf, len, ppos,
				       isp->trace->conf_dump_buffer,
				       isp->trace->size_conf_dump);
}

static ssize_t traceconf_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	struct ipu_device *isp = file->private_data;
	char buffer[64];
	ssize_t bytes, count;
	loff_t pos = *ppos;

	if (*ppos < 0)
		return -EINVAL;

	count = min(len, sizeof(buffer));
	bytes = copy_from_user(buffer, buf, count);
	if (bytes == count)
		return -EFAULT;

	count -= bytes;
	mutex_lock(&isp->trace->lock);
	process_buffer(isp, buffer, count, &isp->trace->buffer_state);
	mutex_unlock(&isp->trace->lock);
	*ppos = pos + count;

	return count;
}

static int traceconf_release(struct inode *inode, struct file *file)
{
	struct ipu_device *isp = file->private_data;
	struct device *psys_dev = isp->psys ? &isp->psys->dev : NULL;
	struct device *isys_dev = isp->isys ? &isp->isys->dev : NULL;
	int pm_rval = -EINVAL;

	/*
	 * Turn devices on outside trace->lock mutex. PM transition may
	 * cause call to function which tries to take the same lock.
	 * Also do this before trace->open is set back to 0 to avoid
	 * double restore (one here and one in pm transition). We can't
	 * rely purely on the restore done by pm call backs since trace
	 * configuration can occur in any phase compared to other activity.
	 */

	if (file->f_mode & FMODE_WRITE) {
		if (isys_dev)
			pm_rval = pm_runtime_get_sync(isys_dev);

		if (pm_rval >= 0) {
			/* ISYS ok or missing */
			if (psys_dev)
				pm_rval = pm_runtime_get_sync(psys_dev);

			if (pm_rval < 0) {
				pm_runtime_put_noidle(psys_dev);
				if (isys_dev)
					pm_runtime_put(isys_dev);
			}
		} else {
			pm_runtime_put_noidle(&isp->isys->dev);
		}
	}

	mutex_lock(&isp->trace->lock);
	isp->trace->open = 0;
	vfree(isp->trace->conf_dump_buffer);
	isp->trace->conf_dump_buffer = NULL;

	if (pm_rval >= 0) {
		/* Update new cfg to HW */
		if (isys_dev) {
			__ipu_trace_stop(isys_dev);
			clear_trace_buffer(isp->isys->trace_cfg);
			__ipu_trace_restore(isys_dev);
		}

		if (psys_dev) {
			__ipu_trace_stop(psys_dev);
			clear_trace_buffer(isp->psys->trace_cfg);
			__ipu_trace_restore(psys_dev);
		}
	}

	mutex_unlock(&isp->trace->lock);

	if (pm_rval >= 0) {
		/* Again - this must be done with trace->lock not taken */
		if (psys_dev)
			pm_runtime_put(psys_dev);
		if (isys_dev)
			pm_runtime_put(isys_dev);
	}
	return 0;
}

static const struct file_operations ipu_traceconf_fops = {
	.owner = THIS_MODULE,
	.open = traceconf_open,
	.release = traceconf_release,
	.read = traceconf_read,
	.write = traceconf_write,
	.llseek = no_llseek,
};

static int gettrace_open(struct inode *inode, struct file *file)
{
	struct ipu_subsystem_trace_config *sys = inode->i_private;

	if (!sys)
		return -EACCES;

	if (!sys->memory.memory_buffer)
		return -EACCES;

	dma_sync_single_for_cpu(sys->dev,
				sys->memory.dma_handle,
				MEMORY_RING_BUFFER_SIZE +
				MEMORY_RING_BUFFER_GUARD, DMA_FROM_DEVICE);

	file->private_data = sys;
	return 0;
};

static ssize_t gettrace_read(struct file *file, char __user *buf,
			     size_t len, loff_t *ppos)
{
	struct ipu_subsystem_trace_config *sys = file->private_data;

	return simple_read_from_buffer(buf, len, ppos,
				       sys->memory.memory_buffer,
				       MEMORY_RING_BUFFER_SIZE +
				       MEMORY_RING_BUFFER_OVERREAD);
}

static ssize_t gettrace_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	struct ipu_subsystem_trace_config *sys = file->private_data;
	static const char str[] = "clear";
	char buffer[sizeof(str)] = { 0 };
	ssize_t ret;

	ret = simple_write_to_buffer(buffer, sizeof(buffer), ppos, buf, len);
	if (ret < 0)
		return ret;

	if (ret < sizeof(str) - 1)
		return -EINVAL;

	if (!strncmp(str, buffer, sizeof(str) - 1)) {
		clear_trace_buffer(sys);
		return len;
	}

	return -EINVAL;
}

static int gettrace_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations ipu_gettrace_fops = {
	.owner = THIS_MODULE,
	.open = gettrace_open,
	.release = gettrace_release,
	.read = gettrace_read,
	.write = gettrace_write,
	.llseek = no_llseek,
};

int ipu_trace_init(struct ipu_device *isp, void __iomem *base,
		   struct device *dev, struct ipu_trace_block *blocks)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_trace *trace = isp->trace;
	struct ipu_subsystem_trace_config *sys;
	int ret = 0;

	if (!isp->trace)
		return 0;

	mutex_lock(&isp->trace->lock);

	if (dev == &isp->isys->dev) {
		sys = &trace->isys;
	} else if (dev == &isp->psys->dev) {
		sys = &trace->psys;
	} else {
		ret = -EINVAL;
		goto leave;
	}

	adev->trace_cfg = sys;
	sys->dev = dev;
	sys->offset = base - isp->base;	/* sub system offset */
	sys->base = base;
	sys->blocks = blocks;

leave:
	mutex_unlock(&isp->trace->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(ipu_trace_init);

void ipu_trace_uninit(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_device *isp = adev->isp;
	struct ipu_trace *trace = isp->trace;
	struct ipu_subsystem_trace_config *sys = adev->trace_cfg;

	if (!trace || !sys)
		return;

	mutex_lock(&trace->lock);

	if (sys->memory.memory_buffer)
		dma_free_coherent(sys->dev,
				  MEMORY_RING_BUFFER_SIZE +
				  MEMORY_RING_BUFFER_GUARD,
				  sys->memory.memory_buffer,
				  sys->memory.dma_handle);

	sys->dev = NULL;
	sys->memory.memory_buffer = NULL;

	mutex_unlock(&trace->lock);
}
EXPORT_SYMBOL_GPL(ipu_trace_uninit);

int ipu_trace_debugfs_add(struct ipu_device *isp, struct dentry *dir)
{
	struct dentry *files[3];
	int i = 0;

	files[i] = debugfs_create_file("traceconf", 0644,
				       dir, isp, &ipu_traceconf_fops);
	if (!files[i])
		return -ENOMEM;
	i++;

	files[i] = debugfs_create_file("getisystrace", 0444,
				       dir,
				       &isp->trace->isys, &ipu_gettrace_fops);

	if (!files[i])
		goto error;
	i++;

	files[i] = debugfs_create_file("getpsystrace", 0444,
				       dir,
				       &isp->trace->psys, &ipu_gettrace_fops);
	if (!files[i])
		goto error;

	return 0;

error:
	for (; i > 0; i--)
		debugfs_remove(files[i - 1]);
	return -ENOMEM;
}

int ipu_trace_add(struct ipu_device *isp)
{
	isp->trace = devm_kzalloc(&isp->pdev->dev,
				  sizeof(struct ipu_trace), GFP_KERNEL);
	if (!isp->trace)
		return -ENOMEM;

	mutex_init(&isp->trace->lock);

	return 0;
}

void ipu_trace_release(struct ipu_device *isp)
{
	if (!isp->trace)
		return;
	mutex_destroy(&isp->trace->lock);
}

MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu trace support");
