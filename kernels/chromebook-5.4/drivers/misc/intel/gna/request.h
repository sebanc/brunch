/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright(c) 2017-2021 Intel Corporation */

#ifndef __GNA_REQUEST_H__
#define __GNA_REQUEST_H__

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include <uapi/misc/intel/gna.h>

struct gna_private;
struct file;

enum gna_request_state {
	NEW,
	ACTIVE,
	DONE,
};

struct gna_file_private;

struct gna_request {
	u64 request_id;

	struct kref refcount;

	struct gna_private *gna_priv;
	struct file *fd;

	u32 hw_status;

	enum gna_request_state state;

	int status;

	struct gna_hw_perf hw_perf;
	struct gna_drv_perf drv_perf;

	struct list_head node;

	struct gna_compute_cfg compute_cfg;

	struct gna_buffer *buffer_list;
	u64 buffer_count;

	struct work_struct work;
	struct wait_queue_head waitq;
};

int gna_enqueue_request(struct gna_compute_cfg *compute_cfg,
			struct gna_file_private *file_priv, u64 *request_id);

void gna_request_release(struct kref *ref);

struct gna_request *gna_find_request_by_id(u64 req_id, struct gna_private *gna_priv);

void gna_delete_request_by_id(u64 req_id, struct gna_private *gna_priv);

void gna_delete_file_requests(struct file *fd, struct gna_private *gna_priv);

void gna_delete_memory_requests(u64 memory_id, struct gna_private *gna_priv);

#endif // __GNA_REQUEST_H__
