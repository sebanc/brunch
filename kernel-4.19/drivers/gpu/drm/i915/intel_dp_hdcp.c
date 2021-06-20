/* SPDX-License-Identifier: MIT */
/*
 * Copyright (C) 2020 Google, Inc.
 *
 * Authors:
 * Sean Paul <seanpaul@chromium.org>
 */

#include <drm/drm_dp_helper.h>
#include <drm/drm_dp_mst_helper.h>
#include <drm/drm_hdcp.h>
#include <drm/drm_print.h>

#include "intel_drv.h"

static
int intel_dp_hdcp_write_an_aksv(struct intel_digital_port *dig_port,
				u8 *an)
{
	u8 aksv[DRM_HDCP_KSV_LEN] = {};
	ssize_t dpcd_ret;

	/* Output An first, that's easy */
	dpcd_ret = drm_dp_dpcd_write(&dig_port->dp.aux, DP_AUX_HDCP_AN,
				     an, DRM_HDCP_AN_LEN);
	if (dpcd_ret != DRM_HDCP_AN_LEN) {
		DRM_DEBUG_KMS("Failed to write An over DP/AUX (%zd)\n",
			      dpcd_ret);
		return dpcd_ret >= 0 ? -EIO : dpcd_ret;
	}

	/*
	 * Since Aksv is Oh-So-Secret, we can't access it in software. So we
	 * send an empty buffer of the correct length through the DP helpers. On
	 * the other side, in the transfer hook, we'll generate a flag based on
	 * the destination address which will tickle the hardware to output the
	 * Aksv on our behalf after the header is sent.
	 */
	dpcd_ret = drm_dp_dpcd_write(&dig_port->dp.aux, DP_AUX_HDCP_AKSV,
				     aksv, DRM_HDCP_KSV_LEN);
	if (dpcd_ret != DRM_HDCP_KSV_LEN) {
		DRM_DEBUG_KMS("Failed to write Aksv over DP/AUX (%zd)\n",
			      dpcd_ret);
		return dpcd_ret >= 0 ? -EIO : dpcd_ret;
	}
	return 0;
}

static int intel_dp_hdcp_read_bksv(struct intel_digital_port *dig_port,
				   u8 *bksv)
{
	ssize_t ret;

	ret = drm_dp_dpcd_read(&dig_port->dp.aux, DP_AUX_HDCP_BKSV, bksv,
			       DRM_HDCP_KSV_LEN);
	if (ret != DRM_HDCP_KSV_LEN) {
		DRM_DEBUG_KMS("Read Bksv from DP/AUX failed (%zd)\n", ret);
		return ret >= 0 ? -EIO : ret;
	}
	return 0;
}

static int intel_dp_hdcp_read_bstatus(struct intel_digital_port *dig_port,
				      u8 *bstatus)
{
	ssize_t ret;

	/*
	 * For some reason the HDMI and DP HDCP specs call this register
	 * definition by different names. In the HDMI spec, it's called BSTATUS,
	 * but in DP it's called BINFO.
	 */
	ret = drm_dp_dpcd_read(&dig_port->dp.aux, DP_AUX_HDCP_BINFO,
			       bstatus, DRM_HDCP_BSTATUS_LEN);
	if (ret != DRM_HDCP_BSTATUS_LEN) {
		DRM_DEBUG_KMS("Read bstatus from DP/AUX failed (%zd)\n", ret);
		return ret >= 0 ? -EIO : ret;
	}
	return 0;
}

static
int intel_dp_hdcp_read_bcaps(struct intel_digital_port *dig_port,
			     u8 *bcaps)
{
	ssize_t ret;

	ret = drm_dp_dpcd_read(&dig_port->dp.aux, DP_AUX_HDCP_BCAPS,
			       bcaps, 1);
	if (ret != 1) {
		DRM_DEBUG_KMS("Read bcaps from DP/AUX failed (%zd)\n", ret);
		return ret >= 0 ? -EIO : ret;
	}

	return 0;
}

static
int intel_dp_hdcp_repeater_present(struct intel_digital_port *dig_port,
				   bool *repeater_present)
{
	ssize_t ret;
	u8 bcaps;

	ret = intel_dp_hdcp_read_bcaps(dig_port, &bcaps);
	if (ret)
		return ret;

	*repeater_present = bcaps & DP_BCAPS_REPEATER_PRESENT;
	return 0;
}

static
int intel_dp_hdcp_read_ri_prime(struct intel_digital_port *dig_port,
				u8 *ri_prime)
{
	ssize_t ret;

	ret = drm_dp_dpcd_read(&dig_port->dp.aux, DP_AUX_HDCP_RI_PRIME,
			       ri_prime, DRM_HDCP_RI_LEN);
	if (ret != DRM_HDCP_RI_LEN) {
		DRM_DEBUG_KMS("Read Ri' from DP/AUX failed (%zd)\n", ret);
		return ret >= 0 ? -EIO : ret;
	}
	return 0;
}

static
int intel_dp_hdcp_read_ksv_ready(struct intel_digital_port *dig_port,
				 bool *ksv_ready)
{
	ssize_t ret;
	u8 bstatus;

	ret = drm_dp_dpcd_read(&dig_port->dp.aux, DP_AUX_HDCP_BSTATUS,
			       &bstatus, 1);
	if (ret != 1) {
		DRM_DEBUG_KMS("Read bstatus from DP/AUX failed (%zd)\n", ret);
		return ret >= 0 ? -EIO : ret;
	}
	*ksv_ready = bstatus & DP_BSTATUS_READY;
	return 0;
}

static
int intel_dp_hdcp_read_ksv_fifo(struct intel_digital_port *dig_port,
				int num_downstream, u8 *ksv_fifo)
{
	ssize_t ret;
	int i;

	/* KSV list is read via 15 byte window (3 entries @ 5 bytes each) */
	for (i = 0; i < num_downstream; i += 3) {
		size_t len = min(num_downstream - i, 3) * DRM_HDCP_KSV_LEN;
		ret = drm_dp_dpcd_read(&dig_port->dp.aux,
				       DP_AUX_HDCP_KSV_FIFO,
				       ksv_fifo + i * DRM_HDCP_KSV_LEN,
				       len);
		if (ret != len) {
			DRM_DEBUG_KMS("Read ksv[%d] from DP/AUX failed (%zd)\n",
				      i, ret);
			return ret >= 0 ? -EIO : ret;
		}
	}
	return 0;
}

static
int intel_dp_hdcp_read_v_prime_part(struct intel_digital_port *dig_port,
				    int i, u32 *part)
{
	ssize_t ret;

	if (i >= DRM_HDCP_V_PRIME_NUM_PARTS)
		return -EINVAL;

	ret = drm_dp_dpcd_read(&dig_port->dp.aux,
			       DP_AUX_HDCP_V_PRIME(i), part,
			       DRM_HDCP_V_PRIME_PART_LEN);
	if (ret != DRM_HDCP_V_PRIME_PART_LEN) {
		DRM_DEBUG_KMS("Read v'[%d] from DP/AUX failed (%zd)\n", i, ret);
		return ret >= 0 ? -EIO : ret;
	}
	return 0;
}

static
int intel_dp_hdcp_toggle_signalling(struct intel_digital_port *dig_port,
				    enum transcoder cpu_transcoder,
				    bool enable)
{
	/* Not used for single stream DisplayPort setups */
	return 0;
}

static
bool intel_dp_hdcp_check_link(struct intel_digital_port *dig_port,
			      struct intel_connector *connector)
{
	ssize_t ret;
	u8 bstatus;

	ret = drm_dp_dpcd_read(&dig_port->dp.aux, DP_AUX_HDCP_BSTATUS,
			       &bstatus, 1);
	if (ret != 1) {
		DRM_DEBUG_KMS("Read bstatus from DP/AUX failed (%zd)\n", ret);
		return false;
	}

	return !(bstatus & (DP_BSTATUS_LINK_FAILURE | DP_BSTATUS_REAUTH_REQ));
}

static
int intel_dp_hdcp_capable(struct intel_digital_port *dig_port,
			  bool *hdcp_capable)
{
	ssize_t ret;
	u8 bcaps;

	ret = intel_dp_hdcp_read_bcaps(dig_port, &bcaps);
	if (ret)
		return ret;

	*hdcp_capable = bcaps & DP_BCAPS_HDCP_CAPABLE;
	return 0;
}

static const struct intel_hdcp_shim intel_dp_hdcp_shim = {
	.write_an_aksv = intel_dp_hdcp_write_an_aksv,
	.read_bksv = intel_dp_hdcp_read_bksv,
	.read_bstatus = intel_dp_hdcp_read_bstatus,
	.repeater_present = intel_dp_hdcp_repeater_present,
	.read_ri_prime = intel_dp_hdcp_read_ri_prime,
	.read_ksv_ready = intel_dp_hdcp_read_ksv_ready,
	.read_ksv_fifo = intel_dp_hdcp_read_ksv_fifo,
	.read_v_prime_part = intel_dp_hdcp_read_v_prime_part,
	.toggle_signalling = intel_dp_hdcp_toggle_signalling,
	.check_link = intel_dp_hdcp_check_link,
	.hdcp_capable = intel_dp_hdcp_capable,
};

static int
intel_dp_mst_hdcp_toggle_signalling(struct intel_digital_port *dig_port,
				    enum transcoder cpu_transcoder,
				    bool enable)
{
	int ret;

	if (!enable)
		usleep_range(6, 60); /* Bspec says >= 6us */

	ret = intel_ddi_toggle_hdcp_signalling(&dig_port->base,
					       cpu_transcoder, enable);
	if (ret)
		DRM_DEBUG_KMS("%s HDCP signalling failed (%d)\n",
			      enable ? "Enable" : "Disable", ret);
	return ret;
}

static const struct intel_hdcp_shim intel_dp_mst_hdcp_shim = {
	.write_an_aksv = intel_dp_hdcp_write_an_aksv,
	.read_bksv = intel_dp_hdcp_read_bksv,
	.read_bstatus = intel_dp_hdcp_read_bstatus,
	.repeater_present = intel_dp_hdcp_repeater_present,
	.read_ri_prime = intel_dp_hdcp_read_ri_prime,
	.read_ksv_ready = intel_dp_hdcp_read_ksv_ready,
	.read_ksv_fifo = intel_dp_hdcp_read_ksv_fifo,
	.read_v_prime_part = intel_dp_hdcp_read_v_prime_part,
	.toggle_signalling = intel_dp_mst_hdcp_toggle_signalling,
	.check_link = intel_dp_hdcp_check_link,
	.hdcp_capable = intel_dp_hdcp_capable,
};

int intel_dp_init_hdcp(struct intel_digital_port *dig_port,
		       struct intel_connector *intel_connector)
{
	struct drm_device *dev = intel_connector->base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_encoder *intel_encoder = &dig_port->base;
	enum port port = intel_encoder->port;
	struct intel_dp *intel_dp = &dig_port->dp;

	if (!is_hdcp_supported(dev_priv, port))
		return 0;

	if (intel_connector->mst_port)
		return intel_hdcp_init(intel_connector, port,
				       &intel_dp_mst_hdcp_shim);
	else if (!intel_dp_is_edp(intel_dp))
		return intel_hdcp_init(intel_connector, port,
				       &intel_dp_hdcp_shim);

	return 0;
}
