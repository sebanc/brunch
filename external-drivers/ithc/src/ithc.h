/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/pci.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/pm_qos.h>

#define DEVNAME "ithc"
#define DEVFULLNAME "Intel Touch Host Controller"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define CHECK(fn, ...) ({ int r = fn(__VA_ARGS__); if (r < 0) pci_err(ithc->pci, "%s: %s failed with %i\n", __func__, #fn, r); r; })
#define CHECK_RET(...) do { int r = CHECK(__VA_ARGS__); if (r < 0) return r; } while (0)

#define NUM_RX_BUF 16

struct ithc;

#include "ithc-regs.h"
#include "ithc-dma.h"

struct ithc {
	char phys[32];
	struct pci_dev *pci;
	int irq;
	struct task_struct *poll_thread;
	struct pm_qos_request activity_qos;
	struct timer_list activity_timer;

	struct hid_device *hid;
	bool hid_parse_done;
	wait_queue_head_t wait_hid_parse;
	wait_queue_head_t wait_hid_get_feature;
	struct mutex hid_get_feature_mutex;
	void *hid_get_feature_buf;
	size_t hid_get_feature_size;

	struct ithc_registers __iomem *regs;
	struct ithc_registers *prev_regs; // for debugging
	struct ithc_device_config config;
	struct ithc_dma_rx dma_rx[2];
	struct ithc_dma_tx dma_tx;
};

int ithc_reset(struct ithc *ithc);
void ithc_set_active(struct ithc *ithc);
int ithc_debug_init(struct ithc *ithc);
void ithc_log_regs(struct ithc *ithc);

