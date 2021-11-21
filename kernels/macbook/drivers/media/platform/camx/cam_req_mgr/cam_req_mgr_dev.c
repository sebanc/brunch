// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/cam_req_mgr.h>
#include <media/cam_defs.h>
#include "cam_req_mgr_dev.h"
#include "cam_req_mgr_util.h"
#include "cam_req_mgr_core.h"
#include "cam_subdev.h"
#include "cam_mem_mgr.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_buf_mgr.h"
#if defined(CONFIG_ARCH_SM6150)
#include <linux/slub_def.h>
#endif

#define CAM_REQ_MGR_EVENT_MAX 30

static struct cam_req_mgr_device g_dev;

static int cam_media_device_setup(struct device *dev)
{
	int rc;

	g_dev.v4l2_dev->mdev = kzalloc(sizeof(*g_dev.v4l2_dev->mdev),
		GFP_KERNEL);
	if (!g_dev.v4l2_dev->mdev) {
		rc = -ENOMEM;
		goto mdev_fail;
	}

	media_device_init(g_dev.v4l2_dev->mdev);
	g_dev.v4l2_dev->mdev->dev = dev;
	strlcpy(g_dev.v4l2_dev->mdev->model, CAM_REQ_MGR_VNODE_NAME,
		sizeof(g_dev.v4l2_dev->mdev->model));

	rc = media_device_register(g_dev.v4l2_dev->mdev);
	if (rc)
		goto media_fail;

	return rc;

media_fail:
	kfree(g_dev.v4l2_dev->mdev);
	g_dev.v4l2_dev->mdev = NULL;
mdev_fail:
	return rc;
}

static void cam_media_device_cleanup(void)
{
	media_entity_cleanup(&g_dev.video->entity);
	media_device_unregister(g_dev.v4l2_dev->mdev);
	kfree(g_dev.v4l2_dev->mdev);
	g_dev.v4l2_dev->mdev = NULL;
}

static int cam_v4l2_device_setup(struct device *dev)
{
	int rc;

	g_dev.v4l2_dev = kzalloc(sizeof(*g_dev.v4l2_dev),
		GFP_KERNEL);
	if (!g_dev.v4l2_dev)
		return -ENOMEM;

	rc = v4l2_device_register(dev, g_dev.v4l2_dev);
	if (rc)
		goto reg_fail;

	return rc;

reg_fail:
	kfree(g_dev.v4l2_dev);
	g_dev.v4l2_dev = NULL;
	return rc;
}

static void cam_v4l2_device_cleanup(void)
{
	v4l2_device_unregister(g_dev.v4l2_dev);
	kfree(g_dev.v4l2_dev);
	g_dev.v4l2_dev = NULL;
}

static int cam_req_mgr_open(struct file *filep)
{
	int rc;

	mutex_lock(&g_dev.cam_lock);
	if (g_dev.open_cnt >= 1) {
		rc = -EALREADY;
		goto end;
	}

	rc = v4l2_fh_open(filep);
	if (rc) {
		CAM_ERR(CAM_CRM, "v4l2_fh_open failed: %d", rc);
		goto end;
	}

	spin_lock_bh(&g_dev.cam_eventq_lock);
	g_dev.cam_eventq = filep->private_data;
	spin_unlock_bh(&g_dev.cam_eventq_lock);

	g_dev.open_cnt++;

	mutex_unlock(&g_dev.cam_lock);
	return rc;

end:
	mutex_unlock(&g_dev.cam_lock);
	return rc;
}

static __poll_t cam_req_mgr_poll(struct file *f,
	struct poll_table_struct *pll_table)
{
	struct v4l2_fh *eventq = f->private_data;

	if (!eventq)
		return EPOLLERR | EPOLLNVAL;

	poll_wait(f, &eventq->wait, pll_table);
	if (v4l2_event_pending(eventq))
		return EPOLLPRI;

	return 0;
}

static int cam_req_mgr_close(struct file *filep)
{
	struct v4l2_subdev *sd;
	struct v4l2_fh *vfh = filep->private_data;
	struct v4l2_subdev_fh *subdev_fh = to_v4l2_subdev_fh(vfh);

	mutex_lock(&g_dev.cam_lock);

	if (g_dev.open_cnt <= 0) {
		mutex_unlock(&g_dev.cam_lock);
		return -EINVAL;
	}

	cam_req_mgr_handle_core_shutdown();

	list_for_each_entry(sd, &g_dev.v4l2_dev->subdevs, list) {
		if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE) ||
		    !sd->internal_ops)
			continue;
		if (sd->internal_ops->close) {
			CAM_DBG(CAM_CRM, "Invoke subdev close for device %s",
				sd->name);
			sd->internal_ops->close(sd, subdev_fh);
		}
		if (sd->internal_ops->release) {
			CAM_DBG(CAM_CRM, "Invoke subdev release for device %s",
				sd->name);
			sd->internal_ops->release(sd);
		}
	}

	g_dev.open_cnt--;
	v4l2_fh_release(filep);

	spin_lock_bh(&g_dev.cam_eventq_lock);
	g_dev.cam_eventq = NULL;
	spin_unlock_bh(&g_dev.cam_eventq_lock);

	cam_req_mgr_util_free_hdls();
	cam_mem_mgr_close();
	mutex_unlock(&g_dev.cam_lock);

	return 0;
}

static struct v4l2_file_operations g_cam_fops = {
	.owner  = THIS_MODULE,
	.open   = cam_req_mgr_open,
	.poll   = cam_req_mgr_poll,
	.release = cam_req_mgr_close,
	.unlocked_ioctl   = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static int cam_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, CAM_REQ_MGR_EVENT_MAX, NULL);
}

static int cam_unsubscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static long cam_private_ioctl(struct file *file, void *fh,
	bool valid_prio, unsigned int cmd, void *arg)
{
	int rc;
	struct cam_control *k_ioctl;

	if ((!arg) || (cmd != VIDIOC_CAM_CONTROL))
		return -EINVAL;

	k_ioctl = (struct cam_control *)arg;

	if (!k_ioctl->handle)
		return -EINVAL;

	switch (k_ioctl->op_code) {
	case CAM_REQ_MGR_CREATE_SESSION: {
		struct cam_req_mgr_session_info ses_info;

		if (k_ioctl->size != sizeof(ses_info))
			return -EINVAL;

		if (copy_from_user(&ses_info,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_session_info))) {
			return -EFAULT;
		}

		rc = cam_req_mgr_create_session(&ses_info);
		if (!rc)
			if (copy_to_user(
				u64_to_user_ptr(k_ioctl->handle),
				&ses_info,
				sizeof(struct cam_req_mgr_session_info)))
				rc = -EFAULT;
		}
		break;

	case CAM_REQ_MGR_DESTROY_SESSION: {
		struct cam_req_mgr_session_info ses_info;

		if (k_ioctl->size != sizeof(ses_info))
			return -EINVAL;

		if (copy_from_user(&ses_info,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_session_info))) {
			return -EFAULT;
		}

		rc = cam_req_mgr_destroy_session(&ses_info);
		}
		break;

	case CAM_REQ_MGR_LINK: {
		struct cam_req_mgr_ver_info ver_info;

		if (k_ioctl->size != sizeof(ver_info.u.link_info_v1))
			return -EINVAL;

		if (copy_from_user(&ver_info.u.link_info_v1,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_link_info))) {
			return -EFAULT;
		}
		ver_info.version = VERSION_1;
		rc = cam_req_mgr_link(&ver_info);
		if (!rc)
			if (copy_to_user(
				u64_to_user_ptr(k_ioctl->handle),
				&ver_info.u.link_info_v1,
				sizeof(struct cam_req_mgr_link_info)))
				rc = -EFAULT;
		}
		break;

	case CAM_REQ_MGR_LINK_V2: {
			struct cam_req_mgr_ver_info ver_info;

			if (k_ioctl->size != sizeof(ver_info.u.link_info_v2))
				return -EINVAL;

			if (copy_from_user(&ver_info.u.link_info_v2,
				u64_to_user_ptr(k_ioctl->handle),
				sizeof(struct cam_req_mgr_link_info_v2))) {
				return -EFAULT;
			}
			ver_info.version = VERSION_2;
			rc = cam_req_mgr_link_v2(&ver_info);
			if (!rc)
				if (copy_to_user(
					u64_to_user_ptr(k_ioctl->handle),
					&ver_info.u.link_info_v2,
					sizeof(struct
						cam_req_mgr_link_info_v2)))
					rc = -EFAULT;
			}
			break;

	case CAM_REQ_MGR_UNLINK: {
		struct cam_req_mgr_unlink_info unlink_info;

		if (k_ioctl->size != sizeof(unlink_info))
			return -EINVAL;

		if (copy_from_user(&unlink_info,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_unlink_info))) {
			return -EFAULT;
		}

		rc = cam_req_mgr_unlink(&unlink_info);
		}
		break;

	case CAM_REQ_MGR_SCHED_REQ: {
		struct cam_req_mgr_sched_request sched_req;

		if (k_ioctl->size != sizeof(sched_req))
			return -EINVAL;

		if (copy_from_user(&sched_req,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_sched_request))) {
			return -EFAULT;
		}

		rc = cam_req_mgr_schedule_request(&sched_req);
		}
		break;

	case CAM_REQ_MGR_FLUSH_REQ: {
		struct cam_req_mgr_flush_info flush_info;

		if (k_ioctl->size != sizeof(flush_info))
			return -EINVAL;

		if (copy_from_user(&flush_info,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_flush_info))) {
			return -EFAULT;
		}

		rc = cam_req_mgr_flush_requests(&flush_info);
		}
		break;

	case CAM_REQ_MGR_SYNC_MODE: {
		struct cam_req_mgr_sync_mode sync_info;

		if (k_ioctl->size != sizeof(sync_info))
			return -EINVAL;

		if (copy_from_user(&sync_info,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_sync_mode))) {
			return -EFAULT;
		}

		rc = cam_req_mgr_sync_config(&sync_info);
		}
		break;
	case CAM_REQ_MGR_ALLOC_BUF: {
		struct cam_mem_mgr_alloc_cmd cmd;

		if (k_ioctl->size != sizeof(cmd))
			return -EINVAL;

		if (copy_from_user(&cmd,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_mem_mgr_alloc_cmd))) {
			rc = -EFAULT;
			break;
		}

		rc = cam_mem_mgr_alloc_and_map(&cmd);
		if (!rc)
			if (copy_to_user(
				u64_to_user_ptr(k_ioctl->handle),
				&cmd, sizeof(struct cam_mem_mgr_alloc_cmd))) {
				rc = -EFAULT;
				break;
			}
		}
		break;
	case CAM_REQ_MGR_MAP_BUF: {
		struct cam_mem_mgr_map_cmd cmd;

		if (k_ioctl->size != sizeof(cmd))
			return -EINVAL;

		if (copy_from_user(&cmd,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_mem_mgr_map_cmd))) {
			rc = -EFAULT;
			break;
		}

		rc = cam_mem_mgr_map(&cmd);
		if (!rc)
			if (copy_to_user(
				u64_to_user_ptr(k_ioctl->handle),
				&cmd, sizeof(struct cam_mem_mgr_map_cmd))) {
				rc = -EFAULT;
				break;
			}
		}
		break;
	case CAM_REQ_MGR_RELEASE_BUF: {
		struct cam_mem_mgr_release_cmd cmd;

		if (k_ioctl->size != sizeof(cmd))
			return -EINVAL;

		if (copy_from_user(&cmd,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_mem_mgr_release_cmd))) {
			rc = -EFAULT;
			break;
		}

		rc = cam_mem_mgr_release(&cmd);
		}
		break;
	case CAM_REQ_MGR_LINK_CONTROL: {
		struct cam_req_mgr_link_control cmd;

		if (k_ioctl->size != sizeof(cmd))
			return -EINVAL;

		if (copy_from_user(&cmd,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_req_mgr_link_control))) {
			rc = -EFAULT;
			break;
		}

		rc = cam_req_mgr_link_control(&cmd);
		if (rc)
			rc = -EINVAL;
		}
		break;

	case CAM_REQ_MGR_REQUEST_DUMP: {
		struct cam_dump_req_cmd cmd;

		if (k_ioctl->size != sizeof(cmd))
			return -EINVAL;

		if (copy_from_user(&cmd,
			u64_to_user_ptr(k_ioctl->handle),
			sizeof(struct cam_dump_req_cmd))) {
			rc = -EFAULT;
			break;
		}

		rc = cam_req_mgr_dump_request(&cmd);
		if (!rc)
			if (copy_to_user(
				u64_to_user_ptr(k_ioctl->handle),
				&cmd, sizeof(struct cam_dump_req_cmd))) {
				rc = -EFAULT;
				break;
			}
		}
		break;

	default:
		return -ENOIOCTLCMD;
	}

	return rc;
}

static const struct v4l2_ioctl_ops g_cam_ioctl_ops = {
	.vidioc_subscribe_event = cam_subscribe_event,
	.vidioc_unsubscribe_event = cam_unsubscribe_event,
	.vidioc_default = cam_private_ioctl,
};

static int cam_video_device_setup(void)
{
	int rc;

	g_dev.video = video_device_alloc();
	if (!g_dev.video) {
		rc = -ENOMEM;
		goto video_fail;
	}

	g_dev.video->v4l2_dev = g_dev.v4l2_dev;

	strlcpy(g_dev.video->name, "cam-req-mgr",
		sizeof(g_dev.video->name));
	g_dev.video->release = video_device_release;
	g_dev.video->fops = &g_cam_fops;
	g_dev.video->ioctl_ops = &g_cam_ioctl_ops;
	g_dev.video->minor = -1;
	g_dev.video->vfl_type = VFL_TYPE_VIDEO;
	g_dev.video->device_caps = V4L2_CAP_DEVICE_CAPS;
	rc = video_register_device(g_dev.video, g_dev.video->vfl_type, -1);
	if (rc)
		goto v4l2_fail;

	rc = media_entity_pads_init(&g_dev.video->entity, 0, NULL);
	if (rc)
		goto entity_fail;

	g_dev.video->entity.function = CAM_VNODE_DEVICE_TYPE;
	g_dev.video->entity.name = video_device_node_name(g_dev.video);

	return rc;

entity_fail:
	video_unregister_device(g_dev.video);
v4l2_fail:
	video_device_release(g_dev.video);
	g_dev.video = NULL;
video_fail:
	return rc;
}

int cam_req_mgr_notify_message(struct cam_req_mgr_message *msg,
	uint32_t id,
	uint32_t type)
{
	struct v4l2_event event;
	struct cam_req_mgr_message *ev_header;

	if (!msg)
		return -EINVAL;

	event.id = id;
	event.type = type;
	ev_header = CAM_REQ_MGR_GET_PAYLOAD_PTR(event,
		struct cam_req_mgr_message);
	memcpy(ev_header, msg, sizeof(struct cam_req_mgr_message));
	v4l2_event_queue(g_dev.video, &event);

	return 0;
}
EXPORT_SYMBOL(cam_req_mgr_notify_message);

static void cam_video_device_cleanup(void)
{
	video_unregister_device(g_dev.video);
	video_device_release(g_dev.video);
	g_dev.video = NULL;
}

void cam_register_subdev_fops(struct v4l2_file_operations *fops)
{
	*fops = v4l2_subdev_fops;
}
EXPORT_SYMBOL(cam_register_subdev_fops);

int cam_register_subdev(struct cam_subdev *csd)
{
	struct cam_req_mgr_device *crm_ctx = &g_dev;
	struct v4l2_subdev *sd;
	int rc;

	if (!csd || !csd->name) {
		CAM_ERR(CAM_CRM, "invalid arguments");
		return -EINVAL;
	}

	CAM_DBG(CAM_CRM, "register subdev %s", csd->name);

	mutex_lock(&crm_ctx->dev_lock);

	sd = &csd->sd;
	v4l2_subdev_init(sd, csd->ops);
	sd->internal_ops = csd->internal_ops;
	snprintf(sd->name, ARRAY_SIZE(sd->name), csd->name);
	v4l2_set_subdevdata(sd, csd->token);

	sd->flags = csd->sd_flags;
	sd->entity.num_pads = 0;
	sd->entity.pads = NULL;
	sd->entity.function = csd->ent_function;

	rc = v4l2_device_register_subdev(crm_ctx->v4l2_dev, sd);
	if (rc) {
		CAM_ERR(CAM_CRM, "register subdev failed for %s", sd->name);
		goto reg_fail;
	}

	rc = v4l2_device_register_subdev_nodes(crm_ctx->v4l2_dev);
	if (rc) {
		v4l2_device_unregister_subdev(sd);
		CAM_ERR(CAM_CRM, "register subdev node failed: '%s'", sd->name);
		goto reg_fail;
	}

	list_for_each_entry(sd, &crm_ctx->v4l2_dev->subdevs, list) {
		if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE))
			continue;
		sd->entity.name = video_device_node_name(sd->devnode);
	}

reg_fail:
	mutex_unlock(&crm_ctx->dev_lock);
	return rc;
}
EXPORT_SYMBOL(cam_register_subdev);

int cam_unregister_subdev(struct cam_subdev *csd)
{
	struct cam_req_mgr_device *crm_ctx = &g_dev;

	mutex_lock(&crm_ctx->dev_lock);
	v4l2_device_unregister_subdev(&csd->sd);
	mutex_unlock(&crm_ctx->dev_lock);

	return 0;
}
EXPORT_SYMBOL(cam_unregister_subdev);

static int cam_req_mgr_remove(struct platform_device *pdev)
{
	cam_req_mgr_core_device_deinit();
	cam_req_mgr_util_deinit();
	cam_media_device_cleanup();
	cam_video_device_cleanup();
	cam_v4l2_device_cleanup();
	cam_mem_mgr_exit(pdev);
	mutex_destroy(&g_dev.dev_lock);
	g_dev.state = false;
	g_dev.subdev_nodes_created = false;

	return 0;
}

static int cam_req_mgr_probe(struct platform_device *pdev)
{
	int rc;

	struct device_node *smmu_intf, *sync_intf;
	struct platform_device *smmu_pdev, *sync_pdev;

	/* Check, that the SYNC interface is available */
	sync_intf = of_parse_phandle(pdev->dev.of_node, "sync_intf", 0);
	sync_pdev = of_find_device_by_node(sync_intf);
	if (!sync_pdev || !sync_pdev->dev.driver) {
		CAM_DBG(CAM_CRM, "Probe deferred, until SYNC become ready");
		return -EPROBE_DEFER;
	}
	put_device(&sync_pdev->dev);

	/* Check, that the SMMU interface is available */
	smmu_intf = of_parse_phandle(pdev->dev.of_node, "smmu_intf", 0);
	smmu_pdev = of_find_device_by_node(smmu_intf);
	if (!smmu_pdev || !smmu_pdev->dev.driver) {
		CAM_DBG(CAM_CRM, "Probe deferred, until SMMU become ready");
		return -EPROBE_DEFER;
	}
	put_device(&smmu_pdev->dev);

	platform_set_drvdata(pdev, &g_dev);

	rc = cam_mem_mgr_init(pdev);
	if (rc) {
		CAM_ERR(CAM_CRM, "Cannot initialize camera memory manager");
		return rc;
	}

	rc = cam_v4l2_device_setup(&pdev->dev);
	if (rc)
		goto err_mem_mgr_exit;

	rc = cam_media_device_setup(&pdev->dev);
	if (rc)
		goto media_setup_fail;

	rc = cam_video_device_setup();
	if (rc)
		goto video_setup_fail;

	g_dev.open_cnt = 0;
	mutex_init(&g_dev.cam_lock);
	spin_lock_init(&g_dev.cam_eventq_lock);
	g_dev.subdev_nodes_created = false;
	mutex_init(&g_dev.dev_lock);

	rc = cam_req_mgr_util_init();
	if (rc) {
		CAM_ERR(CAM_CRM, "cam req mgr util init is failed");
		goto req_mgr_util_fail;
	}

	rc = cam_req_mgr_core_device_init();
	if (rc) {
		CAM_ERR(CAM_CRM, "core device setup failed");
		goto req_mgr_core_fail;
	}

	g_dev.state = true;

	pr_info("%s driver probed successfully\n", KBUILD_MODNAME);

	return rc;

err_mem_mgr_exit:
	cam_mem_mgr_exit(pdev);
req_mgr_core_fail:
	cam_req_mgr_util_deinit();
req_mgr_util_fail:
	mutex_destroy(&g_dev.dev_lock);
	mutex_destroy(&g_dev.cam_lock);
	cam_video_device_cleanup();
video_setup_fail:
	cam_media_device_cleanup();
media_setup_fail:
	cam_v4l2_device_cleanup();
	return rc;
}

static const struct of_device_id cam_req_mgr_dt_match[] = {
	{.compatible = "qcom,cam-req-mgr"},
	{}
};
MODULE_DEVICE_TABLE(of, cam_req_mgr_dt_match);

static struct platform_driver cam_req_mgr_driver = {
	.probe = cam_req_mgr_probe,
	.remove = cam_req_mgr_remove,
	.driver = {
		.name = "cam_req_mgr",
		.owner = THIS_MODULE,
		.of_match_table = cam_req_mgr_dt_match,
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(cam_req_mgr_driver);

MODULE_DESCRIPTION("Camera Request Manager");
MODULE_LICENSE("GPL v2");
