// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause

#include "ithc.h"

void ithc_log_regs(struct ithc *ithc)
{
	if (!ithc->prev_regs)
		return;
	u32 __iomem *cur = (__iomem void *)ithc->regs;
	u32 *prev = (void *)ithc->prev_regs;
	for (int i = 1024; i < sizeof(*ithc->regs) / 4; i++) {
		u32 x = readl(cur + i);
		if (x != prev[i]) {
			pci_info(ithc->pci, "reg %04x: %08x -> %08x\n", i * 4, prev[i], x);
			prev[i] = x;
		}
	}
}

static ssize_t ithc_debugfs_cmd_write(struct file *f, const char __user *buf, size_t len,
	loff_t *offset)
{
	// Debug commands consist of a single letter followed by a list of numbers (decimal or
	// hexadecimal, space-separated).
	struct ithc *ithc = file_inode(f)->i_private;
	char cmd[256];
	if (!ithc || !ithc->pci)
		return -ENODEV;
	if (!len)
		return -EINVAL;
	if (len >= sizeof(cmd))
		return -EINVAL;
	if (copy_from_user(cmd, buf, len))
		return -EFAULT;
	cmd[len] = 0;
	if (cmd[len-1] == '\n')
		cmd[len-1] = 0;
	pci_info(ithc->pci, "debug command: %s\n", cmd);

	// Parse the list of arguments into a u32 array.
	u32 n = 0;
	const char *s = cmd + 1;
	u32 a[32];
	while (*s && *s != '\n') {
		if (n >= ARRAY_SIZE(a))
			return -EINVAL;
		if (*s++ != ' ')
			return -EINVAL;
		char *e;
		a[n++] = simple_strtoul(s, &e, 0);
		if (e == s)
			return -EINVAL;
		s = e;
	}
	ithc_log_regs(ithc);

	// Execute the command.
	switch (cmd[0]) {
	case 'x': // reset
		ithc_reset(ithc);
		break;
	case 'w': // write register: offset mask value
		if (n != 3 || (a[0] & 3))
			return -EINVAL;
		pci_info(ithc->pci, "debug write 0x%04x = 0x%08x (mask 0x%08x)\n",
			a[0], a[2], a[1]);
		bitsl(((__iomem u32 *)ithc->regs) + a[0] / 4, a[1], a[2]);
		break;
	case 'r': // read register: offset
		if (n != 1 || (a[0] & 3))
			return -EINVAL;
		pci_info(ithc->pci, "debug read 0x%04x = 0x%08x\n", a[0],
			readl(((__iomem u32 *)ithc->regs) + a[0] / 4));
		break;
	case 's': // spi command: cmd offset len data...
		// read config: s 4 0 64 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
		// set touch cfg: s 6 12 4 XX
		if (n < 3 || a[2] > (n - 3) * 4)
			return -EINVAL;
		pci_info(ithc->pci, "debug spi command %u with %u bytes of data\n", a[0], a[2]);
		if (!CHECK(ithc_spi_command, ithc, a[0], a[1], a[2], a + 3))
			for (u32 i = 0; i < (a[2] + 3) / 4; i++)
				pci_info(ithc->pci, "resp %u = 0x%08x\n", i, a[3+i]);
		break;
	case 'd': // dma command: cmd len data...
		// get report descriptor: d 7 8 0 0
		// enable multitouch: d 3 2 0x0105
		if (n < 1)
			return -EINVAL;
		pci_info(ithc->pci, "debug dma command with %u bytes of data\n", n * 4);
		struct ithc_data data = { .type = ITHC_DATA_RAW, .size = n * 4, .data = a };
		if (ithc_dma_tx(ithc, &data))
			pci_err(ithc->pci, "dma tx failed\n");
		break;
	default:
		return -EINVAL;
	}
	ithc_log_regs(ithc);
	return len;
}

static struct dentry *dbg_dir;

void __init ithc_debug_init_module(void)
{
	struct dentry *d = debugfs_create_dir(DEVNAME, NULL);
	if (IS_ERR(d))
		pr_warn("failed to create debugfs dir (%li)\n", PTR_ERR(d));
	else
		dbg_dir = d;
}

void __exit ithc_debug_exit_module(void)
{
	debugfs_remove_recursive(dbg_dir);
	dbg_dir = NULL;
}

static const struct file_operations ithc_debugfops_cmd = {
	.owner = THIS_MODULE,
	.write = ithc_debugfs_cmd_write,
};

static void ithc_debugfs_devres_release(struct device *dev, void *res)
{
	struct dentry **dbgm = res;
	debugfs_remove_recursive(*dbgm);
}

int ithc_debug_init_device(struct ithc *ithc)
{
	if (!dbg_dir)
		return -ENOENT;
	struct dentry **dbgm = devres_alloc(ithc_debugfs_devres_release, sizeof(*dbgm), GFP_KERNEL);
	if (!dbgm)
		return -ENOMEM;
	devres_add(&ithc->pci->dev, dbgm);
	struct dentry *dbg = debugfs_create_dir(pci_name(ithc->pci), dbg_dir);
	if (IS_ERR(dbg))
		return PTR_ERR(dbg);
	*dbgm = dbg;

	struct dentry *cmd = debugfs_create_file("cmd", 0220, dbg, ithc, &ithc_debugfops_cmd);
	if (IS_ERR(cmd))
		return PTR_ERR(cmd);

	return 0;
}

