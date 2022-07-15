#include "ithc.h"

#define reg_num(r) (0x1fff & (u16)(u64)(r))

void bitsl(u32 *reg, u32 mask, u32 val) {
	if (val & ~mask) pr_err("register 0x%x: invalid value 0x%x for bitmask 0x%x\n", reg_num(reg), val, mask);
	writel((readl(reg) & ~mask) | (val & mask), reg);
}

void bitsb(u8 *reg, u8 mask, u8 val) {
	if (val & ~mask) pr_err("register 0x%x: invalid value 0x%x for bitmask 0x%x\n", reg_num(reg), val, mask);
	writeb((readb(reg) & ~mask) | (val & mask), reg);
}

int waitl(struct ithc *ithc, u32 *reg, u32 mask, u32 val) {
	pci_dbg(ithc->pci, "waiting for reg 0x%04x mask 0x%08x val 0x%08x\n", reg_num(reg), mask, val);
	u32 x;
	if (readl_poll_timeout(reg, x, (x & mask) == val, 200, 1000*1000)) {
		pci_err(ithc->pci, "timed out waiting for reg 0x%04x mask 0x%08x val 0x%08x\n", reg_num(reg), mask, val);
		return -ETIMEDOUT;
	}
	pci_dbg(ithc->pci, "done waiting\n");
	return 0;
}

int waitb(struct ithc *ithc, u8 *reg, u8 mask, u8 val) {
	pci_dbg(ithc->pci, "waiting for reg 0x%04x mask 0x%02x val 0x%02x\n", reg_num(reg), mask, val);
	u8 x;
	if (readb_poll_timeout(reg, x, (x & mask) == val, 200, 1000*1000)) {
		pci_err(ithc->pci, "timed out waiting for reg 0x%04x mask 0x%02x val 0x%02x\n", reg_num(reg), mask, val);
		return -ETIMEDOUT;
	}
	pci_dbg(ithc->pci, "done waiting\n");
	return 0;
}

int ithc_set_spi_config(struct ithc *ithc, u8 speed, u8 mode) {
	pci_dbg(ithc->pci, "setting SPI speed to %i, mode %i\n", speed, mode);
	if (mode == 3) mode = 2;
	bitsl(&ithc->regs->spi_config,
		SPI_CONFIG_MODE(0xff) | SPI_CONFIG_SPEED(0xff) | SPI_CONFIG_UNKNOWN_18(0xff) | SPI_CONFIG_SPEED2(0xff),
		SPI_CONFIG_MODE(mode) | SPI_CONFIG_SPEED(speed) | SPI_CONFIG_UNKNOWN_18(0) | SPI_CONFIG_SPEED2(speed));
	return 0;
}

int ithc_spi_command(struct ithc *ithc, u8 command, u32 offset, u32 size, void *data) {
	pci_dbg(ithc->pci, "SPI command %u, size %u, offset %u\n", command, size, offset);
	if (size > sizeof ithc->regs->spi_cmd.data) return -EINVAL;
	CHECK_RET(waitl, ithc, &ithc->regs->spi_cmd.status, SPI_CMD_STATUS_BUSY, 0);
	writel(SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR, &ithc->regs->spi_cmd.status);
	writeb(command, &ithc->regs->spi_cmd.code);
	writew(size, &ithc->regs->spi_cmd.size);
	writel(offset, &ithc->regs->spi_cmd.offset);
	u32 *p = data, n = (size + 3) / 4;
	for (u32 i = 0; i < n; i++) writel(p[i], &ithc->regs->spi_cmd.data[i]);
	bitsb_set(&ithc->regs->spi_cmd.control, SPI_CMD_CONTROL_SEND);
	CHECK_RET(waitl, ithc, &ithc->regs->spi_cmd.status, SPI_CMD_STATUS_BUSY, 0);
	if ((readl(&ithc->regs->spi_cmd.status) & (SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR)) != SPI_CMD_STATUS_DONE) return -EIO;
	if (readw(&ithc->regs->spi_cmd.size) != size) return -EMSGSIZE;
	for (u32 i = 0; i < n; i++) p[i] = readl(&ithc->regs->spi_cmd.data[i]);
	writel(SPI_CMD_STATUS_DONE | SPI_CMD_STATUS_ERROR, &ithc->regs->spi_cmd.status);
	return 0;
}

