// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#include "intel_pxp_cmd.h"
#include "i915_drv.h"
#include "gt/intel_context.h"
#include "gt/intel_engine_pm.h"
#include "intel_pxp_sm.h"

/* PXP GPU command definitions */

/* MI_SET_APPID */
#define   MI_SET_APPID_TYPE1_APP        BIT(7)
#define   MI_SET_APPID_SESSION_ID(x)    ((x) << 0)

/* MI_FLUSH_DW */
#define   MI_FLUSH_DW_DW0_PROTECTED_MEMORY_ENABLE   BIT(22)

/* MI_WAIT */
#define   MFX_WAIT_DW0_PXP_SYNC_CONTROL_FLAG BIT(9)
#define   MFX_WAIT_DW0_MFX_SYNC_CONTROL_FLAG  BIT(8)

/* CRYPTO_KEY_EXCHANGE */
#define CRYPTO_KEY_EXCHANGE ((0x3 << 29) | (0x01609 << 16))

static struct i915_vma *intel_pxp_cmd_get_batch(struct intel_pxp *pxp,
						struct intel_context *ce,
						struct intel_gt_buffer_pool_node *pool,
						u32 *cmd_buf, int cmd_size_in_dw)
{
	struct i915_vma *batch = ERR_PTR(-EINVAL);
	struct intel_gt *gt = container_of(pxp, struct intel_gt, pxp);
	u32 *cmd;

	if (!ce || !ce->engine || !cmd_buf)
		return ERR_PTR(-EINVAL);

	if (cmd_size_in_dw * 4 > PAGE_SIZE) {
		drm_err(&gt->i915->drm, "Failed to %s, invalid cmd_size_id_dw=[%d]\n",
			__func__, cmd_size_in_dw);
		return ERR_PTR(-EINVAL);
	}

	cmd = i915_gem_object_pin_map(pool->obj, I915_MAP_FORCE_WC);
	if (IS_ERR(cmd)) {
		drm_err(&gt->i915->drm, "Failed to i915_gem_object_pin_map()\n");
		return ERR_PTR(-EINVAL);
	}

	memcpy(cmd, cmd_buf, cmd_size_in_dw * 4);

	if (drm_debug_syslog_enabled(DRM_UT_DRIVER)) {
		print_hex_dump(KERN_DEBUG, "cmd binaries:",
			       DUMP_PREFIX_OFFSET, 4, 4, cmd, cmd_size_in_dw * 4, true);
	}

	i915_gem_object_unpin_map(pool->obj);

	batch = i915_vma_instance(pool->obj, ce->vm, NULL);
	if (IS_ERR(batch)) {
		drm_err(&gt->i915->drm, "Failed to i915_vma_instance()\n");
		return batch;
	}

	return batch;
}

int intel_pxp_cmd_submit(struct intel_pxp *pxp, u32 *cmd,
			 int cmd_size_in_dw)
{
	int err = -EINVAL;
	struct i915_vma *batch;
	struct i915_request *rq;
	struct intel_context *ce = NULL;
	bool is_engine_pm_get = false;
	bool is_batch_vma_pin = false;
	bool is_skip_req_on_err = false;
	bool is_engine_get_pool = false;
	struct intel_gt_buffer_pool_node *pool = NULL;
	struct intel_gt *gt = container_of(pxp, struct intel_gt, pxp);

	if (!cmd || (cmd_size_in_dw * 4) > PAGE_SIZE) {
		drm_err(&gt->i915->drm, "Failed to %s bad params\n", __func__);
		return -EINVAL;
	}

	ce = pxp->vcs_engine->kernel_context;
	if (!ce) {
		drm_err(&gt->i915->drm, "VCS engine does not have context\n");
		err = -EINVAL;
		goto end;
	}

	intel_engine_pm_get(ce->engine);
	is_engine_pm_get = true;

	pool = intel_gt_get_buffer_pool(gt, PAGE_SIZE);
	if (IS_ERR(pool)) {
		drm_err(&gt->i915->drm, "Failed to intel_engine_get_pool()\n");
		err = PTR_ERR(pool);
		goto end;
	}
	is_engine_get_pool = true;

	batch = intel_pxp_cmd_get_batch(pxp, ce, pool, cmd, cmd_size_in_dw);
	if (IS_ERR(batch)) {
		drm_err(&gt->i915->drm, "Failed to intel_pxp_cmd_get_batch()\n");
		err = PTR_ERR(batch);
		goto end;
	}

	err = i915_vma_pin(batch, 0, 0, PIN_USER);
	if (err) {
		drm_err(&gt->i915->drm, "Failed to i915_vma_pin()\n");
		goto end;
	}
	is_batch_vma_pin = true;

	rq = intel_context_create_request(ce);
	if (IS_ERR(rq)) {
		drm_err(&gt->i915->drm, "Failed to intel_context_create_request()\n");
		err = PTR_ERR(rq);
		goto end;
	}
	is_skip_req_on_err = true;

	err = intel_gt_buffer_pool_mark_active(pool, rq);
	if (err) {
		drm_err(&gt->i915->drm, "Failed to intel_engine_pool_mark_active()\n");
		goto end;
	}

	i915_vma_lock(batch);
	err = i915_request_await_object(rq, batch->obj, false);
	if (!err)
		err = i915_vma_move_to_active(batch, rq, 0);
	i915_vma_unlock(batch);
	if (err) {
		drm_err(&gt->i915->drm, "Failed to i915_request_await_object()\n");
		goto end;
	}

	if (ce->engine->emit_init_breadcrumb) {
		err = ce->engine->emit_init_breadcrumb(rq);
		if (err) {
			drm_err(&gt->i915->drm, "Failed to emit_init_breadcrumb()\n");
			goto end;
		}
	}

	err = ce->engine->emit_bb_start(rq, batch->node.start,
		batch->node.size, 0);
	if (err) {
		drm_err(&gt->i915->drm, "Failed to emit_bb_start()\n");
		goto end;
	}

	i915_request_add(rq);

end:
	if (unlikely(err) && is_skip_req_on_err)
		i915_request_set_error_once(rq, err);

	if (is_batch_vma_pin)
		i915_vma_unpin(batch);

	if (is_engine_get_pool)
		intel_gt_buffer_pool_put(pool);

	if (is_engine_pm_get)
		intel_engine_pm_put(ce->engine);

	return err;
}

int intel_pxp_cmd_add_prolog(struct intel_pxp *pxp, u32 *cmd,
			     int session_type,
			     int session_index)
{
	u32 increased_size_in_dw = 0;
	u32 *cmd_prolog = cmd;
	const int cmd_prolog_size_in_dw = 10;
	struct intel_gt *gt = container_of(pxp, typeof(*gt), pxp);

	if (!cmd)
		return cmd_prolog_size_in_dw;

	/* MFX_WAIT - stall until prior PXP and MFX/HCP/HUC objects are cmopleted */
	*cmd_prolog++ = (MFX_WAIT | MFX_WAIT_DW0_PXP_SYNC_CONTROL_FLAG |
			 MFX_WAIT_DW0_MFX_SYNC_CONTROL_FLAG);

	/* MI_FLUSH_DW - pxp off */
	*cmd_prolog++ = MI_FLUSH_DW;  /* DW0 */
	*cmd_prolog++ = 0;            /* DW1 */
	*cmd_prolog++ = 0;            /* DW2 */

	/* MI_SET_APPID */
	if (session_type == SESSION_TYPE_TYPE1) {
		if (session_index >= PXP_MAX_TYPE1_SESSIONS) {
			drm_err(&gt->i915->drm, "Failed to %s invalid session_index\n", __func__);
			goto end;
		}

		*cmd_prolog++ = (MI_SET_APPID | MI_SET_APPID_TYPE1_APP |
				 MI_SET_APPID_SESSION_ID(session_index));
	} else {
		if (session_index >= PXP_MAX_TYPE0_SESSIONS) {
			drm_err(&gt->i915->drm, "Failed to %s invalid session_index\n", __func__);
			goto end;
		}

		*cmd_prolog++ = (MI_SET_APPID | MI_SET_APPID_SESSION_ID(session_index));
	}

	/* MFX_WAIT */
	*cmd_prolog++ = (MFX_WAIT | MFX_WAIT_DW0_PXP_SYNC_CONTROL_FLAG |
			 MFX_WAIT_DW0_MFX_SYNC_CONTROL_FLAG);

	/* MI_FLUSH_DW - pxp on */
	*cmd_prolog++ = (MI_FLUSH_DW | MI_FLUSH_DW_DW0_PROTECTED_MEMORY_ENABLE); /* DW0 */
	*cmd_prolog++ = 0;                                                       /* DW1 */
	*cmd_prolog++ = 0;                                                       /* DW2 */

	/* MFX_WAIT */
	*cmd_prolog++ = (MFX_WAIT | MFX_WAIT_DW0_PXP_SYNC_CONTROL_FLAG |
			 MFX_WAIT_DW0_MFX_SYNC_CONTROL_FLAG);

	increased_size_in_dw = (cmd_prolog - cmd);
end:
	return increased_size_in_dw;
}

int intel_pxp_cmd_add_epilog(u32 *cmd)
{
	u32 increased_size_in_dw = 0;
	u32 *cmd_epilog = cmd;
	const int cmd_epilog_size_in_dw = 5;

	if (!cmd)
		return cmd_epilog_size_in_dw;

	/* MI_FLUSH_DW - pxp off */
	*cmd_epilog++ = MI_FLUSH_DW;  /* DW0 */
	*cmd_epilog++ = 0;            /* DW1 */
	*cmd_epilog++ = 0;            /* DW2 */

	/* MFX_WAIT - stall until prior PXP and MFX/HCP/HUC objects are cmopleted */
	*cmd_epilog++ = (MFX_WAIT | MFX_WAIT_DW0_PXP_SYNC_CONTROL_FLAG |
			 MFX_WAIT_DW0_MFX_SYNC_CONTROL_FLAG);

	/* MI_BATCH_BUFFER_END */
	*cmd_epilog++ = MI_BATCH_BUFFER_END;

	increased_size_in_dw = (cmd_epilog - cmd);
	return increased_size_in_dw;
}

int intel_pxp_cmd_add_inline_termination(u32 *cmd)
{
	u32 increased_size_in_dw = 0;
	u32 *cmd_termin = cmd;
	const int cmd_termin_size_in_dw = 2;

	if (!cmd)
		return cmd_termin_size_in_dw;

	/* CRYPTO_KEY_EXCHANGE - session inline termination */
	*cmd_termin++ = CRYPTO_KEY_EXCHANGE; /* DW0 */
	*cmd_termin++ = 0;                   /* DW1 */

	increased_size_in_dw = (cmd_termin - cmd);
	return increased_size_in_dw;
}

int intel_pxp_cmd_terminate_all_hw_session(struct intel_pxp *pxp,
					   int session_type)
{
	u32 *cmd = NULL;
	u32 *cmd_ptr = NULL;
	int cmd_size_in_dw = 0;
	int ret;
	int idx;
	struct intel_gt *gt = container_of(pxp, struct intel_gt, pxp);

	/* Calculate how many bytes need to be alloc */
	for (idx = 0; idx < pxp_session_max(session_type); idx++) {
		if (intel_pxp_sm_is_hw_session_in_play(pxp, session_type, idx)) {
			cmd_size_in_dw += intel_pxp_cmd_add_prolog(pxp, NULL, session_type, idx);
			cmd_size_in_dw += intel_pxp_cmd_add_inline_termination(NULL);
		}
	}
	cmd_size_in_dw += intel_pxp_cmd_add_epilog(NULL);

	cmd = kzalloc(cmd_size_in_dw * 4, GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	/* Program the command */
	cmd_ptr = cmd;
	for (idx = 0; idx < pxp_session_max(session_type); idx++) {
		if (intel_pxp_sm_is_hw_session_in_play(pxp, session_type, idx)) {
			cmd_ptr += intel_pxp_cmd_add_prolog(pxp, cmd_ptr, session_type, idx);
			cmd_ptr += intel_pxp_cmd_add_inline_termination(cmd_ptr);
		}
	}
	cmd_ptr += intel_pxp_cmd_add_epilog(cmd_ptr);

	if (cmd_size_in_dw != (cmd_ptr - cmd)) {
		ret = -EINVAL;
		drm_err(&gt->i915->drm, "Failed to %s\n", __func__);
		goto end;
	}

	if (drm_debug_syslog_enabled(DRM_UT_DRIVER)) {
		print_hex_dump(KERN_DEBUG, "global termination cmd binaries:",
			       DUMP_PREFIX_OFFSET, 4, 4, cmd, cmd_size_in_dw * 4, true);
	}

	ret = intel_pxp_cmd_submit(pxp, cmd, cmd_size_in_dw);
	if (ret) {
		drm_err(&gt->i915->drm, "Failed to pxp_submit_cmd()\n");
		goto end;
	}

end:
	kfree(cmd);
	return ret;
}
