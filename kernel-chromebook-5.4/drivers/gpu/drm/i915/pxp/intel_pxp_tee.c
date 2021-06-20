// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020 Intel Corporation.
 */

#include <linux/component.h>
#include "drm/i915_pxp_tee_interface.h"
#include "drm/i915_component.h"
#include  "i915_drv.h"
#include "intel_pxp.h"
#include "intel_pxp_context.h"
#include "intel_pxp_tee.h"
#include "intel_pxp_arb.h"

static int intel_pxp_tee_io_message(struct intel_pxp *pxp,
				    void *msg_in, u32 msg_in_size,
				    void *msg_out, u32 *msg_out_size_ptr,
				    u32 msg_out_buf_size)
{
	int ret;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);
	struct drm_i915_private *i915 = gt->i915;
	struct i915_pxp_comp_master *pxp_tee_master = i915->pxp_tee_master;

	if (!pxp_tee_master || !msg_in || !msg_out || !msg_out_size_ptr)
		return -EINVAL;

	lockdep_assert_held(&i915->pxp_tee_comp_mutex);

	if (drm_debug_syslog_enabled(DRM_UT_DRIVER))
		print_hex_dump(KERN_DEBUG, "TEE input message binaries:",
			       DUMP_PREFIX_OFFSET, 4, 4, msg_in, msg_in_size, true);

	ret = pxp_tee_master->ops->send(pxp_tee_master->tee_dev, msg_in, msg_in_size, 1);
	if (ret) {
		drm_err(&i915->drm, "Failed to send TEE message\n");
		return -EFAULT;
	}

	ret = pxp_tee_master->ops->receive(pxp_tee_master->tee_dev, msg_out, msg_out_buf_size, 1);
	if (ret < 0) {
		drm_err(&i915->drm, "Failed to receive TEE message\n");
		return -EFAULT;
	}

	if (ret > msg_out_buf_size) {
		drm_err(&i915->drm, "Failed to receive TEE message due to unexpected output size\n");
		return -EFAULT;
	}

	*msg_out_size_ptr = ret;
	ret = 0;

	if (drm_debug_syslog_enabled(DRM_UT_DRIVER))
		print_hex_dump(KERN_DEBUG, "TEE output message binaries:",
			       DUMP_PREFIX_OFFSET, 4, 4, msg_out, *msg_out_size_ptr, true);

	return ret;
}

/**
 * i915_pxp_tee_component_bind - bind funciton to pass the function pointers to pxp_tee
 * @i915_kdev: pointer to i915 kernel device
 * @tee_kdev: pointer to tee kernel device
 * @data: pointer to pxp_tee_master containing the function pointers
 *
 * This bind function is called during the system boot or resume from system sleep.
 *
 * Return: return 0 if successful.
 */
static int i915_pxp_tee_component_bind(struct device *i915_kdev,
				       struct device *tee_kdev, void *data)
{
	int ret;
	struct drm_i915_private *i915 = kdev_to_i915(i915_kdev);
	struct intel_pxp *pxp = &i915->gt.pxp;

	if (!i915 || !tee_kdev || !data)
		return -EPERM;

	mutex_lock(&i915->pxp_tee_comp_mutex);
	i915->pxp_tee_master = (struct i915_pxp_comp_master *)data;
	i915->pxp_tee_master->tee_dev = tee_kdev;
	mutex_unlock(&i915->pxp_tee_comp_mutex);

	mutex_lock(&pxp->ctx.mutex);
	/* Create arb session only if tee is ready, during system boot or sleep/resume */
	ret = intel_pxp_arb_create_session(pxp);
	mutex_unlock(&pxp->ctx.mutex);

	if (ret) {
		drm_err(&i915->drm, "Failed to create arb session ret=[%d]\n", ret);
		return ret;
	}

	return 0;
}

static void i915_pxp_tee_component_unbind(struct device *i915_kdev,
					  struct device *tee_kdev, void *data)
{
	struct drm_i915_private *i915 = kdev_to_i915(i915_kdev);

	if (!i915 || !tee_kdev || !data)
		return;

	mutex_lock(&i915->pxp_tee_comp_mutex);
	i915->pxp_tee_master = NULL;
	mutex_unlock(&i915->pxp_tee_comp_mutex);
}

static const struct component_ops i915_pxp_tee_component_ops = {
	.bind   = i915_pxp_tee_component_bind,
	.unbind = i915_pxp_tee_component_unbind,
};

void intel_pxp_tee_component_init(struct intel_pxp *pxp)
{
	int ret;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);
	struct drm_i915_private *i915 = gt->i915;

	ret = component_add_typed(i915->drm.dev, &i915_pxp_tee_component_ops,
				  I915_COMPONENT_PXP);
	if (ret < 0) {
		drm_err(&i915->drm, "Failed at component add(%d)\n", ret);
		return;
	}

	mutex_lock(&i915->pxp_tee_comp_mutex);
	i915->pxp_tee_comp_added = true;
	mutex_unlock(&i915->pxp_tee_comp_mutex);
}

void intel_pxp_tee_component_fini(struct intel_pxp *pxp)
{
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);
	struct drm_i915_private *i915 = gt->i915;

	mutex_lock(&i915->pxp_tee_comp_mutex);
	i915->pxp_tee_comp_added = false;
	mutex_unlock(&i915->pxp_tee_comp_mutex);

	component_del(i915->drm.dev, &i915_pxp_tee_component_ops);
}

int intel_pxp_tee_cmd_create_arb_session(struct intel_pxp *pxp)
{
	int ret;
	u32 msg_out_size_received = 0;
	u32 msg_in[PXP_TEE_ARB_CMD_DW_LEN] = PXP_TEE_ARB_CMD_BIN;
	u32 msg_out[PXP_TEE_ARB_CMD_DW_LEN] = {0};
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);
	struct drm_i915_private *i915 = gt->i915;

	mutex_lock(&i915->pxp_tee_comp_mutex);

	ret = intel_pxp_tee_io_message(pxp,
				       &msg_in,
				       sizeof(msg_in),
				       &msg_out, &msg_out_size_received,
				       sizeof(msg_out));

	mutex_unlock(&i915->pxp_tee_comp_mutex);

	if (ret)
		drm_err(&i915->drm, "Failed to send/receive tee message\n");

	return ret;
}

int intel_pxp_tee_ioctl_io_message(struct intel_pxp *pxp,
				   void __user *msg_in_user_ptr, u32 msg_in_size,
				   void __user *msg_out_user_ptr, u32 *msg_out_size_ptr,
				   u32 msg_out_buf_size)
{
	int ret;
	void *msg_in = NULL;
	void *msg_out = NULL;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);
	struct drm_i915_private *i915 = gt->i915;

	if (!msg_in_user_ptr || !msg_out_user_ptr || msg_out_buf_size == 0 ||
	    msg_in_size == 0 || !msg_out_size_ptr)
		return -EINVAL;

	msg_in = kzalloc(msg_in_size, GFP_KERNEL);
	if (!msg_in)
		return -ENOMEM;

	msg_out = kzalloc(msg_out_buf_size, GFP_KERNEL);
	if (!msg_out) {
		ret = -ENOMEM;
		goto end;
	}

	if (copy_from_user(msg_in, msg_in_user_ptr, msg_in_size) != 0) {
		ret = -EFAULT;
		drm_err(&i915->drm, "Failed to copy_from_user for TEE message\n");
		goto end;
	}

	mutex_lock(&i915->pxp_tee_comp_mutex);

	ret = intel_pxp_tee_io_message(pxp,
				       msg_in, msg_in_size,
				       msg_out, msg_out_size_ptr,
				       msg_out_buf_size);

	mutex_unlock(&i915->pxp_tee_comp_mutex);

	if (ret) {
		drm_err(&i915->drm, "Failed to send/receive tee message\n");
		goto end;
	}

	if (copy_to_user(msg_out_user_ptr, msg_out, *msg_out_size_ptr) != 0) {
		ret = -EFAULT;
		drm_err(&i915->drm, "Failed to copy_to_user for TEE message\n");
		goto end;
	}

end:
	kfree(msg_in);
	kfree(msg_out);
	return ret;
}
