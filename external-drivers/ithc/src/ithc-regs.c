// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause

#include "ithc.h"

#define reg_num(r) (0x1fff & (u16)(__force u64)(r))

void bitsl(__iomem u32 *reg, u32 mask, u32 val)
{
	if (val & ~mask)
		pr_err("register 0x%x: invalid value 0x%x for bitmask 0x%x\n",
			reg_num(reg), val, mask);
	writel((readl(reg) & ~mask) | (val & mask), reg);
}

void bitsb(__iomem u8 *reg, u8 mask, u8 val)
{
	if (val & ~mask)
		pr_err("register 0x%x: invalid value 0x%x for bitmask 0x%x\n",
			reg_num(reg), val, mask);
	writeb((readb(reg) & ~mask) | (val & mask), reg);
}

int waitl(struct ithc *ithc, __iomem u32 *reg, u32 mask, u32 val)
{
	ithc_log_regs(ithc);
	pci_dbg(ithc->pci, "waiting for reg 0x%04x mask 0x%08x val 0x%08x\n",
		reg_num(reg), mask, val);
	u32 x;
	if (readl_poll_timeout(reg, x, (x & mask) == val, 200, 1000*1000)) {
		ithc_log_regs(ithc);
		pci_err(ithc->pci, "timed out waiting for reg 0x%04x mask 0x%08x val 0x%08x\n",
			reg_num(reg), mask, val);
		return -ETIMEDOUT;
	}
	ithc_log_regs(ithc);
	pci_dbg(ithc->pci, "done waiting\n");
	return 0;
}

int waitb(struct ithc *ithc, __iomem u8 *reg, u8 mask, u8 val)
{
	ithc_log_regs(ithc);
	pci_dbg(ithc->pci, "waiting for reg 0x%04x mask 0x%02x val 0x%02x\n",
		reg_num(reg), mask, val);
	u8 x;
	if (readb_poll_timeout(reg, x, (x & mask) == val, 200, 1000*1000)) {
		ithc_log_regs(ithc);
		pci_err(ithc->pci, "timed out waiting for reg 0x%04x mask 0x%02x val 0x%02x\n",
			reg_num(reg), mask, val);
		return -ETIMEDOUT;
	}
	ithc_log_regs(ithc);
	pci_dbg(ithc->pci, "done waiting\n");
	return 0;
}

static void calc_ltr(u64 *ns, unsigned int *val, unsigned int *scale)
{
	unsigned int s = 0;
	u64 v = *ns;
	while (v > 0x3ff) {
		s++;
		v >>= 5;
	}
	if (s > 5) {
		s = 5;
		v = 0x3ff;
	}
	*val = v;
	*scale = s;
	*ns = v << (5 * s);
}

void ithc_set_ltr_config(struct ithc *ithc, u64 active_ltr_ns, u64 idle_ltr_ns)
{
	unsigned int active_val, active_scale, idle_val, idle_scale;
	calc_ltr(&active_ltr_ns, &active_val, &active_scale);
	calc_ltr(&idle_ltr_ns, &idle_val, &idle_scale);
	pci_dbg(ithc->pci, "setting active LTR value to %llu ns, idle LTR value to %llu ns\n",
		active_ltr_ns, idle_ltr_ns);
	writel(LTR_CONFIG_ENABLE_ACTIVE | LTR_CONFIG_ENABLE_IDLE | LTR_CONFIG_APPLY |
		LTR_CONFIG_ACTIVE_LTR_SCALE(active_scale) | LTR_CONFIG_ACTIVE_LTR_VALUE(active_val) |
		LTR_CONFIG_IDLE_LTR_SCALE(idle_scale) | LTR_CONFIG_IDLE_LTR_VALUE(idle_val),
		&ithc->regs->ltr_config);
}

void ithc_set_ltr_idle(struct ithc *ithc)
{
	u32 ltr = readl(&ithc->regs->ltr_config);
	switch (ltr & (LTR_CONFIG_STATUS_ACTIVE | LTR_CONFIG_STATUS_IDLE)) {
	case LTR_CONFIG_STATUS_IDLE:
		break;
	case LTR_CONFIG_STATUS_ACTIVE:
		writel(ltr | LTR_CONFIG_TOGGLE | LTR_CONFIG_APPLY, &ithc->regs->ltr_config);
		break;
	default:
		pci_err(ithc->pci, "invalid LTR state 0x%08x\n", ltr);
		break;
	}
}

int ithc_set_spi_config(struct ithc *ithc, u8 clkdiv, bool clkdiv8, u8 read_mode, u8 write_mode)
{
	if (clkdiv == 0 || clkdiv > 7 || read_mode > SPI_MODE_QUAD || write_mode > SPI_MODE_QUAD)
		return -EINVAL;
	static const char * const modes[] = { "single", "dual", "quad" };
	pci_dbg(ithc->pci, "setting SPI frequency to %i Hz, %s read, %s write\n",
		SPI_CLK_FREQ_BASE / (clkdiv * (clkdiv8 ? 8 : 1)),
		modes[read_mode], modes[write_mode]);
	bitsl(&ithc->regs->spi_config,
		SPI_CONFIG_READ_MODE(0xff) | SPI_CONFIG_READ_CLKDIV(0xff) |
		SPI_CONFIG_WRITE_MODE(0xff) | SPI_CONFIG_WRITE_CLKDIV(0xff) |
		SPI_CONFIG_CLKDIV_8,
		SPI_CONFIG_READ_MODE(read_mode) | SPI_CONFIG_READ_CLKDIV(clkdiv) |
		SPI_CONFIG_WRITE_MODE(write_mode) | SPI_CONFIG_WRITE_CLKDIV(clkdiv) |
		(clkdiv8 ? SPI_CONFIG_CLKDIV_8 : 0));
	return 0;
}

int ithc_spi_command(struct ithc *ithc, u8 command, u32 offset, u32 size, void *data)
{
	pci_dbg(ithc->pci, "SPI command %u, size %u, offset 0x%x\n", command, size, offset);
	if (size > sizeof(ithc->regs->spi_cmd.data))
		return -EINVAL;

	// Wait if the device is still busy.
	CHECK_RET(waitl, ithc, &ithc->regs->spi_cmd.status, SPI_CMD_STATUS_BUSY, 0);
	// Clear result flags.
	writel(SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR, &ithc->regs->spi_cmd.status);

	// Init SPI command data.
	writeb(command, &ithc->regs->spi_cmd.code);
	writew(size, &ithc->regs->spi_cmd.size);
	writel(offset, &ithc->regs->spi_cmd.offset);
	u32 *p = data, n = (size + 3) / 4;
	for (u32 i = 0; i < n; i++)
		writel(p[i], &ithc->regs->spi_cmd.data[i]);

	// Start transmission.
	bitsb_set(&ithc->regs->spi_cmd.control, SPI_CMD_CONTROL_SEND);
	CHECK_RET(waitl, ithc, &ithc->regs->spi_cmd.status, SPI_CMD_STATUS_BUSY, 0);

	// Read response.
	if ((readl(&ithc->regs->spi_cmd.status) & (SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR)) != SPI_CMD_STATUS_DONE)
		return -EIO;
	if (readw(&ithc->regs->spi_cmd.size) != size)
		return -EMSGSIZE;
	for (u32 i = 0; i < n; i++)
		p[i] = readl(&ithc->regs->spi_cmd.data[i]);

	writel(SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR, &ithc->regs->spi_cmd.status);
	return 0;
}

