/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author:Mark Yao <mark.yao@rock-chips.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/dma-buf.h>
#include <linux/kernel.h>
#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <soc/rockchip/rk3399_dmc.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_gem.h"
#include "rockchip_drm_psr.h"

#define to_rockchip_fb(x) container_of(x, struct rockchip_drm_fb, fb)

struct rockchip_drm_fb {
	struct drm_framebuffer fb;
	struct drm_gem_object *obj[ROCKCHIP_MAX_FB_BUFFER];
};

struct drm_gem_object *rockchip_fb_get_gem_obj(struct drm_framebuffer *fb,
					       unsigned int plane)
{
	struct rockchip_drm_fb *rk_fb = to_rockchip_fb(fb);

	if (plane >= ROCKCHIP_MAX_FB_BUFFER)
		return NULL;

	return rk_fb->obj[plane];
}

static void rockchip_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct rockchip_drm_fb *rockchip_fb = to_rockchip_fb(fb);
	struct drm_gem_object *obj;
	int i;

	for (i = 0; i < ROCKCHIP_MAX_FB_BUFFER; i++) {
		obj = rockchip_fb->obj[i];
		if (obj)
			drm_gem_object_unreference_unlocked(obj);
	}

	drm_framebuffer_cleanup(fb);
	kfree(rockchip_fb);
}

static int rockchip_drm_fb_create_handle(struct drm_framebuffer *fb,
					 struct drm_file *file_priv,
					 unsigned int *handle)
{
	struct rockchip_drm_fb *rockchip_fb = to_rockchip_fb(fb);

	return drm_gem_handle_create(file_priv,
				     rockchip_fb->obj[0], handle);
}

static int rockchip_drm_fb_dirty(struct drm_framebuffer *fb,
				 struct drm_file *file,
				 unsigned int flags, unsigned int color,
				 struct drm_clip_rect *clips,
				 unsigned int num_clips)
{
	rockchip_drm_psr_flush_all(fb->dev);
	return 0;
}

static int rockchip_drm_get_reservations(struct drm_framebuffer *fb,
					  struct reservation_object **resvs,
					  unsigned int *num_resvs)
{
	int num_planes = drm_format_num_planes(fb->format->format);
	int i;

	if (num_planes <= 0) {
		DRM_ERROR("Invalid pixel format with no planes.\n");
		return -EINVAL;
	}

	for (i = 0; i < num_planes; i++) {
		struct drm_gem_object *obj = rockchip_fb_get_gem_obj(fb, i);
		int r;

		if (!obj) {
			DRM_ERROR("Failed to get rockchip gem object from framebuffer.\n");
			return -EINVAL;
		}

		if (!obj->dma_buf)
			continue;
		if (!obj->dma_buf->resv)
			continue;
		for (r = 0; r < *num_resvs; r++)
			if (resvs[r] == obj->dma_buf->resv)
				break;
		if (r == *num_resvs)
			resvs[(*num_resvs)++] = obj->dma_buf->resv;
	}
	return 0;
}

static const struct drm_framebuffer_funcs rockchip_drm_fb_funcs = {
	.destroy	= rockchip_drm_fb_destroy,
	.create_handle	= rockchip_drm_fb_create_handle,
	.dirty		= rockchip_drm_fb_dirty,
	.get_reservations = rockchip_drm_get_reservations,
};

static struct rockchip_drm_fb *
rockchip_fb_alloc(struct drm_device *dev, const struct drm_mode_fb_cmd2 *mode_cmd,
		  struct drm_gem_object **obj, unsigned int num_planes)
{
	struct rockchip_drm_fb *rockchip_fb;
	int ret;
	int i;

	/* The DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC modifier
	 * indicates support for AFBC buffers only up to 2560 pixels
	 * wide.
	 */
	if ((mode_cmd->flags & DRM_MODE_FB_MODIFIERS) &&
	    mode_cmd->modifier[0] == DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC &&
	    mode_cmd->width > 2560)
		return ERR_PTR(-EINVAL);

	rockchip_fb = kzalloc(sizeof(*rockchip_fb), GFP_KERNEL);
	if (!rockchip_fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(dev, &rockchip_fb->fb, mode_cmd);

	for (i = 0; i < num_planes; i++)
		rockchip_fb->obj[i] = obj[i];

	ret = drm_framebuffer_init(dev, &rockchip_fb->fb,
				   &rockchip_drm_fb_funcs);
	if (ret) {
		dev_err(dev->dev, "Failed to initialize framebuffer: %d\n",
			ret);
		kfree(rockchip_fb);
		return ERR_PTR(ret);
	}

	return rockchip_fb;
}

static struct drm_framebuffer *
rockchip_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
			const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct rockchip_drm_fb *rockchip_fb;
	struct drm_gem_object *objs[ROCKCHIP_MAX_FB_BUFFER];
	struct drm_gem_object *obj;
	unsigned int hsub;
	unsigned int vsub;
	int num_planes;
	int ret;
	int i;

	hsub = drm_format_horz_chroma_subsampling(mode_cmd->pixel_format);
	vsub = drm_format_vert_chroma_subsampling(mode_cmd->pixel_format);
	num_planes = min(drm_format_num_planes(mode_cmd->pixel_format),
			 ROCKCHIP_MAX_FB_BUFFER);

	for (i = 0; i < num_planes; i++) {
		unsigned int width = mode_cmd->width / (i ? hsub : 1);
		unsigned int height = mode_cmd->height / (i ? vsub : 1);
		unsigned int min_size;

		obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj) {
			dev_err(dev->dev, "Failed to lookup GEM object\n");
			ret = -ENXIO;
			goto err_gem_object_unreference;
		}

		min_size = (height - 1) * mode_cmd->pitches[i] +
			mode_cmd->offsets[i] +
			width * drm_format_plane_cpp(mode_cmd->pixel_format, i);

		if (obj->size < min_size) {
			drm_gem_object_unreference_unlocked(obj);
			ret = -EINVAL;
			goto err_gem_object_unreference;
		}
		objs[i] = obj;
	}

	rockchip_fb = rockchip_fb_alloc(dev, mode_cmd, objs, i);
	if (IS_ERR(rockchip_fb)) {
		ret = PTR_ERR(rockchip_fb);
		goto err_gem_object_unreference;
	}

	return &rockchip_fb->fb;

err_gem_object_unreference:
	for (i--; i >= 0; i--)
		drm_gem_object_unreference_unlocked(objs[i]);
	return ERR_PTR(ret);
}

static void rockchip_drm_output_poll_changed(struct drm_device *dev)
{
	struct rockchip_drm_private *private = dev->dev_private;
	struct drm_fb_helper *fb_helper = &private->fbdev_helper;

	if (fb_helper)
		drm_fb_helper_hotplug_event(fb_helper);
}

static void wait_for_fences(struct drm_device *dev,
			    struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	int i;

	for_each_plane_in_state(state, plane, plane_state, i) {
		if (!plane->state->fence)
			continue;

		WARN_ON(!plane->state->fb);

		dma_fence_wait(plane->state->fence, false);
		dma_fence_put(plane->state->fence);
		plane->state->fence = NULL;
	}
}

uint32_t rockchip_drm_get_vblank_ns(struct drm_display_mode *mode)
{
	uint64_t vblank_time = mode->vtotal - mode->vdisplay;

	vblank_time *= (uint64_t)NSEC_PER_SEC * mode->htotal;
	do_div(vblank_time, mode->clock * 1000);

	return vblank_time;
}

static void
rockchip_drm_check_and_block_dmcfreq(struct rockchip_atomic_commit *commit)
{
	struct drm_atomic_state *state = commit->state;
	struct drm_device *dev = commit->dev;
	struct rockchip_drm_private *priv = dev->dev_private;
	struct drm_crtc_state *old_crtc_state;
	struct drm_crtc *crtc;
	int i;

	for_each_crtc_in_state(state, crtc, old_crtc_state, i) {
		struct rockchip_crtc_state *s =
			to_rockchip_crtc_state(crtc->state);
		struct rockchip_crtc_state *old_s =
			to_rockchip_crtc_state(old_crtc_state);

		if (!old_s->needs_dmcfreq_block && s->needs_dmcfreq_block) {
			rockchip_dmcfreq_block(priv->devfreq);
			DRM_DEV_DEBUG_KMS(dev->dev,
				"%s: vblank period too short, blocking dmcfreq (crtc = %d)\n",
				__func__, drm_crtc_index(crtc));
		}
	}
}

static void
rockchip_drm_check_and_unblock_dmcfreq(struct rockchip_atomic_commit *commit)
{
	struct drm_atomic_state *state = commit->state;
	struct drm_device *dev = commit->dev;
	struct rockchip_drm_private *priv = dev->dev_private;
	struct drm_crtc_state *old_crtc_state;
	struct drm_crtc *crtc;
	int i;

	for_each_crtc_in_state(state, crtc, old_crtc_state, i) {
		struct rockchip_crtc_state *s =
			to_rockchip_crtc_state(crtc->state);
		struct rockchip_crtc_state *old_s =
			to_rockchip_crtc_state(old_crtc_state);

		if (old_s->needs_dmcfreq_block && !s->needs_dmcfreq_block) {
			rockchip_dmcfreq_unblock(priv->devfreq);
			DRM_DEV_DEBUG_KMS(dev->dev,
				"%s: vblank period long enough, unblocking dmcfreq (crtc = %d)\n",
				__func__, drm_crtc_index(crtc));
		}
	}
}

static void
rockchip_drm_psr_inhibit_get_state(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_encoder *encoder;
	u32 encoder_mask = 0;
	int i;

	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		encoder_mask |= crtc_state->encoder_mask;
		encoder_mask |= crtc->state->encoder_mask;
	}

	drm_for_each_encoder_mask(encoder, state->dev, encoder_mask)
		rockchip_drm_psr_inhibit_get(encoder);
}

static void
rockchip_drm_psr_inhibit_put_state(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_encoder *encoder;
	u32 encoder_mask = 0;
	int i;

	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		encoder_mask |= crtc_state->encoder_mask;
		encoder_mask |= crtc->state->encoder_mask;
	}

	drm_for_each_encoder_mask(encoder, state->dev, encoder_mask)
		rockchip_drm_psr_inhibit_put(encoder);
}

static void
rockchip_atomic_disable_hdcp(struct drm_atomic_state *old_state)
{
	struct drm_connector *conn;
	struct drm_connector_state *old_conn_state;
	int i;

	for_each_connector_in_state(old_state, conn, old_conn_state, i) {
		uint64_t new, old;
		if (!is_connector_cdn_dp(conn))
			continue;

		old = old_conn_state->content_protection;
		new = conn->state->content_protection;
		if ((new != old &&
		     new == DRM_MODE_CONTENT_PROTECTION_UNDESIRED) ||
		    (!conn->state->crtc && old_conn_state->crtc))
			cdn_dp_hdcp_atomic_disable(conn);
	}
}

static void
rockchip_atomic_enable_hdcp(struct drm_atomic_state *old_state)
{
	struct drm_connector *conn;
	struct drm_connector_state *old_conn_state;
	int i;

	for_each_connector_in_state(old_state, conn, old_conn_state, i) {
		uint64_t new, old;
		if (!is_connector_cdn_dp(conn))
			continue;

		old = old_conn_state->content_protection;
		new = conn->state->content_protection;
		if (!conn->state->crtc ||
		    new != DRM_MODE_CONTENT_PROTECTION_DESIRED ||
		    (new == old && old_conn_state->crtc == conn->state->crtc))
			continue;

		cdn_dp_hdcp_atomic_enable(conn);
	}
}

static void
rockchip_atomic_commit_complete(struct rockchip_atomic_commit *commit)
{
	struct drm_atomic_state *state = commit->state;
	struct drm_device *dev = commit->dev;

	wait_for_fences(dev, state);

	/* If disabling dmc, disable it before committing mode set changes. */
	rockchip_drm_check_and_block_dmcfreq(commit);

	/*
	 * Rockchip crtc support runtime PM, can't update display planes
	 * when crtc is disabled.
	 *
	 * drm_atomic_helper_commit comments detail that:
	 *     For drivers supporting runtime PM the recommended sequence is
	 *
	 *     drm_atomic_helper_commit_modeset_disables(dev, state);
	 *
	 *     drm_atomic_helper_commit_modeset_enables(dev, state);
	 *
	 *     drm_atomic_helper_commit_planes(dev, state, true);
	 *
	 * See the kerneldoc entries for these three functions for more details.
	 */

	rockchip_drm_psr_inhibit_get_state(state);

	mutex_lock(&commit->hw_lock);

	rockchip_atomic_disable_hdcp(state);

	drm_atomic_helper_commit_modeset_disables(dev, state);

	drm_atomic_helper_commit_modeset_enables(dev, state);

	drm_atomic_helper_commit_planes(dev, state, true);

	rockchip_atomic_enable_hdcp(state);

	mutex_unlock(&commit->hw_lock);

	rockchip_drm_psr_inhibit_put_state(state);

	drm_atomic_helper_wait_for_vblanks(dev, state);

	drm_atomic_helper_cleanup_planes(dev, state);

	/* If enabling dmc, enable it after mode set changes take effect. */
	rockchip_drm_check_and_unblock_dmcfreq(commit);

	drm_atomic_state_put(state);
}

void rockchip_drm_atomic_work(struct kthread_work *work)
{
	struct rockchip_atomic_commit *commit = container_of(work,
					struct rockchip_atomic_commit, work);

	rockchip_atomic_commit_complete(commit);
}

static int rockchip_drm_atomic_commit(struct drm_device *dev,
				      struct drm_atomic_state *state,
				      bool nonblock)
{
	struct rockchip_drm_private *private = dev->dev_private;
	struct rockchip_atomic_commit *commit = &private->commit;
	struct drm_plane_state *plane_state;
	struct drm_plane *plane;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	int ret;
	int i;

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret)
		return ret;

	/* serialize outstanding nonblocking commits */
	mutex_lock(&commit->lock);
	flush_kthread_worker(&commit->worker);

	commit->needs_modeset = false;
	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		if (drm_atomic_crtc_needs_modeset(crtc_state)) {
			commit->needs_modeset = true;
			break;
		}
	}

	commit->has_cursor_plane = false;
	for_each_plane_in_state(state, plane, plane_state, i) {
		if (plane->crtc && plane == plane->crtc->cursor) {
			commit->has_cursor_plane = true;
			break;
		}
	}

	drm_atomic_helper_swap_state(state, true);

	commit->dev = dev;
	commit->state = state;

	drm_atomic_state_get(state);
	if (nonblock)
		queue_kthread_work(&commit->worker, &commit->work);
	else
		rockchip_atomic_commit_complete(commit);

	mutex_unlock(&commit->lock);

	return 0;
}

static const struct drm_mode_config_funcs rockchip_drm_mode_config_funcs = {
	.fb_create = rockchip_user_fb_create,
	.output_poll_changed = rockchip_drm_output_poll_changed,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = rockchip_drm_atomic_commit,
};

struct drm_framebuffer *
rockchip_drm_framebuffer_init(struct drm_device *dev,
			      const struct drm_mode_fb_cmd2 *mode_cmd,
			      struct drm_gem_object *obj)
{
	struct rockchip_drm_fb *rockchip_fb;

	rockchip_fb = rockchip_fb_alloc(dev, mode_cmd, &obj, 1);
	if (IS_ERR(rockchip_fb))
		return NULL;

	return &rockchip_fb->fb;
}

void rockchip_drm_mode_config_init(struct drm_device *dev)
{
	unsigned long rotation_flags = DRM_ROTATE_0 | DRM_REFLECT_Y;

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 *
	 * We don't deal well with modes wider than 3840, so let's
	 * lower max_width a bit.
	 * https://bugs.chromium.org/p/chromium/issues/detail?id=761104
	 */
	dev->mode_config.max_width = 3840;
	dev->mode_config.max_height = 4096;

	dev->mode_config.allow_fb_modifiers = true;
	dev->mode_config.rotation_property =
		drm_mode_create_rotation_property(dev, rotation_flags);

	dev->mode_config.funcs = &rockchip_drm_mode_config_funcs;
}
