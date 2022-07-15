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
#define CHECK_RET(...) do { int r = CHECK(__VA_ARGS__); if (r < 0) return r; } while(0)

// We allocate NUM_RX_ALLOC buffers, and take a sliding window of NUM_RX_DEV buffers from that to populate the device ringbuffer.
// The remaining buffers outside the window contain old data that we keep around for sending to userspace.
#define NUM_RX_ALLOC 32
#define NUM_RX_DEV   16
_Static_assert(NUM_RX_ALLOC % NUM_RX_DEV == 0, "NUM_RX_ALLOC must be a multiple of NUM_RX_DEV");

struct ithc;
struct ithc_api;

#include "ithc-regs.h"
#include "ithc-dma.h"
#include "ithc-api.h"

struct ithc {
	char phys[32];
	struct pci_dev *pci;
	int irq;
	struct task_struct *poll_thread;
	struct pm_qos_request activity_qos;
	struct timer_list activity_timer;

	struct input_dev *input;

	struct hid_device *hid;
	bool hid_parse_done;
	wait_queue_head_t wait_hid_parse;
	wait_queue_head_t wait_hid_get_feature;
	struct mutex hid_get_feature_mutex;
	void *hid_get_feature_buf;
	size_t hid_get_feature_size;

	struct ithc_registers *regs;
	struct ithc_registers *prev_regs; // for debugging
	struct ithc_device_config config;
	struct ithc_dma_rx dma_rx[2];
	struct ithc_dma_tx dma_tx;
};

int ithc_reset(struct ithc *ithc);
void ithc_set_active(struct ithc *ithc);
int ithc_debug_init(struct ithc *ithc);
void ithc_log_regs(struct ithc *ithc);

