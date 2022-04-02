// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2017-2021 Intel Corporation

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/jiffies.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include <uapi/misc/intel/gna.h>

#include "device.h"
#include "ioctl.h"
#include "mem.h"
#include "request.h"
#include "score.h"

static int gna_ioctl_score(struct gna_file_private *file_priv, void __user *argptr)
{
	union gna_compute score_args;
	struct gna_private *gna_priv;
	u64 request_id;
	int ret;

	gna_priv = file_priv->gna_priv;

	if (copy_from_user(&score_args, argptr, sizeof(score_args))) {
		dev_err(gna_dev(gna_priv), "could not copy score ioctl config from user\n");
		return -EFAULT;
	}

	ret = gna_validate_score_config(&score_args.in.config, file_priv);
	if (ret) {
		dev_err(gna_dev(gna_priv), "request not valid\n");
		return ret;
	}

	ret = gna_enqueue_request(&score_args.in.config, file_priv, &request_id);
	if (ret) {
		dev_err(gna_dev(gna_priv), "could not enqueue score request %d\n", ret);
		return ret;
	}

	score_args.out.request_id = request_id;
	if (copy_to_user(argptr, &score_args, sizeof(score_args))) {
		dev_err(gna_dev(gna_priv), "could not copy score ioctl status to user\n");
		return -EFAULT;
	}

	return 0;
}

static int gna_ioctl_wait(struct file *f, void __user *argptr)
{
	struct gna_file_private *file_priv;
	struct gna_request *score_request;
	struct gna_private *gna_priv;
	union gna_wait wait_data;
	u64 request_id;
	u32 timeout;
	int ret;

	file_priv = (struct gna_file_private *)f->private_data;
	gna_priv = file_priv->gna_priv;

	ret = 0;

	if (copy_from_user(&wait_data, argptr, sizeof(wait_data))) {
		dev_err(gna_dev(gna_priv), "could not copy wait ioctl data from user\n");
		return -EFAULT;
	}

	request_id = wait_data.in.request_id;
	timeout = wait_data.in.timeout;

	score_request = gna_find_request_by_id(request_id, gna_priv);

	if (!score_request) {
		dev_err(gna_dev(gna_priv), "could not find request with id: %llu\n", request_id);
		return -EINVAL;
	}

	if (score_request->fd != f) {
		kref_put(&score_request->refcount, gna_request_release);
		return -EINVAL;
	}

	dev_dbg(gna_dev(gna_priv), "waiting for request %llu for timeout %u\n", request_id, timeout);

	ret = wait_event_interruptible_timeout(score_request->waitq, score_request->state == DONE,
					       msecs_to_jiffies(timeout));
	if (ret == 0 || ret == -ERESTARTSYS) {
		dev_err(gna_dev(gna_priv), "request timed out, id: %llu\n", request_id);
		kref_put(&score_request->refcount, gna_request_release);
		return -EBUSY;
	}

	dev_dbg(gna_dev(gna_priv), "request wait completed with %d req id %llu\n", ret, request_id);

	wait_data.out.hw_perf = score_request->hw_perf;
	wait_data.out.drv_perf = score_request->drv_perf;
	wait_data.out.hw_status = score_request->hw_status;

	ret = score_request->status;

	dev_dbg(gna_dev(gna_priv), "request status %d, hw status: %#x\n",
		score_request->status, score_request->hw_status);
	kref_put(&score_request->refcount, gna_request_release);

	gna_delete_request_by_id(request_id, gna_priv);

	if (copy_to_user(argptr, &wait_data, sizeof(wait_data))) {
		dev_err(gna_dev(gna_priv), "could not copy wait ioctl status to user\n");
		ret = -EFAULT;
	}

	return ret;
}

static int gna_ioctl_map(struct gna_file_private *file_priv, void __user *argptr)
{
	struct gna_private *gna_priv;
	union gna_memory_map gna_mem;
	int ret;

	gna_priv = file_priv->gna_priv;

	if (copy_from_user(&gna_mem, argptr, sizeof(gna_mem))) {
		dev_err(gna_dev(gna_priv), "could not copy userptr ioctl data from user\n");
		return -EFAULT;
	}

	ret = gna_map_memory(file_priv, &gna_mem);
	if (ret)
		return ret;

	if (copy_to_user(argptr, &gna_mem, sizeof(gna_mem))) {
		dev_err(gna_dev(gna_priv), "could not copy userptr ioctl status to user\n");
		return -EFAULT;
	}

	return 0;
}

static int gna_ioctl_free(struct gna_file_private *file_priv, unsigned long arg)
{
	struct gna_memory_object *iter_mo, *temp_mo;
	struct gna_memory_object *mo;
	struct gna_private *gna_priv;

	u64 memory_id = arg;

	gna_priv = file_priv->gna_priv;

	mutex_lock(&gna_priv->memidr_lock);
	mo = idr_find(&gna_priv->memory_idr, memory_id);
	mutex_unlock(&gna_priv->memidr_lock);

	if (!mo) {
		dev_warn(gna_dev(gna_priv), "memory object not found\n");
		return -EINVAL;
	}

	queue_work(gna_priv->request_wq, &mo->work);
	if (wait_event_interruptible(mo->waitq, true)) {
		dev_dbg(gna_dev(gna_priv), "wait interrupted\n");
		return -ETIME;
	}

	mutex_lock(&file_priv->memlist_lock);
	list_for_each_entry_safe(iter_mo, temp_mo, &file_priv->memory_list, file_mem_list) {
		if (iter_mo->memory_id == memory_id) {
			list_del(&iter_mo->file_mem_list);
			break;
		}
	}
	mutex_unlock(&file_priv->memlist_lock);

	gna_memory_free(gna_priv, mo);

	return 0;
}

static int gna_ioctl_getparam(struct gna_private *gna_priv, void __user *argptr)
{
	union gna_parameter param;
	int ret;

	if (copy_from_user(&param, argptr, sizeof(param))) {
		dev_err(gna_dev(gna_priv), "could not copy getparam ioctl data from user\n");
		return -EFAULT;
	}

	ret = gna_getparam(gna_priv, &param);
	if (ret)
		return ret;

	if (copy_to_user(argptr, &param, sizeof(param))) {
		dev_err(gna_dev(gna_priv), "could not copy getparam ioctl status to user\n");
		return -EFAULT;
	}

	return 0;
}

long gna_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct gna_file_private *file_priv;
	struct gna_private *gna_priv;
	void __user *argptr;
	int ret = 0;

	argptr = (void __user *)arg;

	file_priv = (struct gna_file_private *)f->private_data;
	// TODO following is always false?
	if (!file_priv)
		return -ENODEV;

	gna_priv = file_priv->gna_priv;
	if (!gna_priv)
		return -ENODEV;

	switch (cmd) {
	case GNA_GET_PARAMETER:
		ret = gna_ioctl_getparam(gna_priv, argptr);
		break;

	case GNA_MAP_MEMORY:
		ret = gna_ioctl_map(file_priv, argptr);
		break;

	case GNA_UNMAP_MEMORY:
		ret = gna_ioctl_free(file_priv, arg);
		break;

	case GNA_COMPUTE:
		ret = gna_ioctl_score(file_priv, argptr);
		break;

	case GNA_WAIT:
		ret = gna_ioctl_wait(f, argptr);
		break;

	default:
		dev_warn(gna_dev(gna_priv), "wrong ioctl %#x\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}
