/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/hid.h>
#include <linux/highmem.h>
#include <linux/input.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/iopoll.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/poll.h>

#define DEVNAME "ithc"
#define DEVFULLNAME "Intel Touch Host Controller"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define CHECK(fn, ...) ({ int r = fn(__VA_ARGS__); if (r < 0) pci_err(ithc->pci, "%s: %s failed with %i\n", __func__, #fn, r); r; })
#define CHECK_RET(...) do { int r = CHECK(__VA_ARGS__); if (r < 0) return r; } while (0)

#define NUM_RX_BUF 16

struct ithc;

#include "ithc-regs.h"
#include "ithc-hid.h"
#include "ithc-dma.h"
#include "ithc-legacy.h"
#include "ithc-quickspi.h"
#include "ithc-debug.h"

struct ithc {
	char phys[32];
	struct pci_dev *pci;
	int irq;
	struct task_struct *poll_thread;

	struct ithc_registers __iomem *regs;
	struct ithc_registers *prev_regs; // for debugging
	struct ithc_dma_rx dma_rx[2];
	struct ithc_dma_tx dma_tx;
	struct ithc_hid hid;

	bool use_quickspi;
	bool have_config;
	u16 vendor_id;
	u16 product_id;
	u32 product_rev;
	u32 max_rx_size;
	u32 max_tx_size;
	u32 legacy_touch_cfg;
};

int ithc_reset(struct ithc *ithc);

