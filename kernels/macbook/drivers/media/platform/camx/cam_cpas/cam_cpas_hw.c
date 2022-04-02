// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interconnect.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/module.h>

#include "cam_cpas_hw.h"
#include "cam_cpas_hw_intf.h"
#include "cam_cpas_soc.h"

static uint cam_min_camnoc_ib_bw;
module_param(cam_min_camnoc_ib_bw, uint, 0644);

#define RPMH_REGULATOR_LEVEL_OFF	(0)
#define RPMH_REGULATOR_LEVEL_RETENTION	RPMH_REGULATOR_LEVEL_OFF
#define RPMH_REGULATOR_LEVEL_MIN_SVS	(1)
#define RPMH_REGULATOR_LEVEL_LOW_SVS	RPMH_REGULATOR_LEVEL_MIN_SVS
#define RPMH_REGULATOR_LEVEL_SVS	RPMH_REGULATOR_LEVEL_MIN_SVS
#define RPMH_REGULATOR_LEVEL_SVS_L1	RPMH_REGULATOR_LEVEL_MIN_SVS
#define RPMH_REGULATOR_LEVEL_NOM	(2)
#define RPMH_REGULATOR_LEVEL_NOM_L1	RPMH_REGULATOR_LEVEL_NOM
#define RPMH_REGULATOR_LEVEL_NOM_L2	RPMH_REGULATOR_LEVEL_NOM
#define RPMH_REGULATOR_LEVEL_TURBO	(3)
#define RPMH_REGULATOR_LEVEL_TURBO_L1	RPMH_REGULATOR_LEVEL_TURBO

static const unsigned int ahb_bus_bw_kbps_tbl[] = {
	0, 89500, 125000, 250000
};

static const struct cam_cpas_bus_bw_map ahb_bus_map[] = {
	{
		.rpmh_level	= CAM_SUSPEND_VOTE,
		.level_name	= "suspend",
		.index		= RPMH_REGULATOR_LEVEL_OFF,
	},
	{
		.rpmh_level	= CAM_SUSPEND_VOTE,
		.level_name	= "suspend",
		.index		= RPMH_REGULATOR_LEVEL_RETENTION,
	},
	{
		.rpmh_level	= CAM_MINSVS_VOTE,
		.level_name	= "lowsvs",
		.index		= RPMH_REGULATOR_LEVEL_MIN_SVS,
	},
	{
		.rpmh_level	= CAM_LOWSVS_VOTE,
		.level_name	= "lowsvs",
		.index		= RPMH_REGULATOR_LEVEL_LOW_SVS,
	},
	{
		.rpmh_level	= CAM_SVS_VOTE,
		.level_name	= "svs",
		.index		= RPMH_REGULATOR_LEVEL_SVS,
	},
	{
		.rpmh_level	= CAM_SVSL1_VOTE,
		.level_name	= "svs_l1",
		.index		= RPMH_REGULATOR_LEVEL_SVS_L1,
	},
	{
		.rpmh_level	= CAM_SVSL1_VOTE,
		.level_name	= "nominal",
		.index		= RPMH_REGULATOR_LEVEL_NOM,
	},
	{
		.rpmh_level	= CAM_SVSL1_VOTE,
		.level_name	= "nominal",
		.index		= RPMH_REGULATOR_LEVEL_NOM_L1,
	},
	{
		.rpmh_level	= CAM_NOMINAL_VOTE,
		.level_name	= "nominal",
		.index		= RPMH_REGULATOR_LEVEL_NOM_L2,
	},
	{
		.rpmh_level	= CAM_TURBO_VOTE,
		.level_name	= "turbo",
		.index		= RPMH_REGULATOR_LEVEL_TURBO,
	},
	{
		.rpmh_level	= CAM_TURBO_VOTE,
		.level_name	= "turbo",
		.index		= RPMH_REGULATOR_LEVEL_TURBO_L1,
	},
};

int cam_cpas_util_reg_update(struct cam_hw_info *cpas_hw,
	enum cam_cpas_reg_base reg_base, struct cam_cpas_reg *reg_info)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	uint32_t value;
	int reg_base_index;

	if (reg_info->enable == false)
		return 0;

	reg_base_index = cpas_core->regbase_index[reg_base];
	if (reg_base_index == -1)
		return -EINVAL;

	if (reg_info->masked_value) {
		value = cam_io_r_mb(
			soc_info->reg_map[reg_base_index].mem_base +
			reg_info->offset);
		value = value & (~reg_info->mask);
		value = value | (reg_info->value << reg_info->shift);
	} else {
		value = reg_info->value;
	}

	CAM_DBG(CAM_CPAS, "Base[%d] Offset[0x%8x] Value[0x%8x]",
		reg_base, reg_info->offset, value);

	cam_io_w_mb(value, soc_info->reg_map[reg_base_index].mem_base +
		reg_info->offset);

	return 0;
}

static int _cpas_vote_bus_level(struct cam_cpas_bus_client *bus_client,
				unsigned int level)
{
	u8 tbl_idx;
	u32 peak_bw, aver_bw = 0;

	if (!bus_client->valid || (bus_client->dyn_vote == true)) {
		CAM_ERR(CAM_CPAS, "Invalid params %d %d", bus_client->valid,
			bus_client->dyn_vote);
		return -EINVAL;
	}

	if (!bus_client->bw_tbl || level >= bus_client->bw_tbl_size) {
		CAM_ERR(CAM_CPAS, "Invalid vote level = %d", level);
		return -EINVAL;
	}

	/* Get the index corresponding to defined level */
	tbl_idx = bus_client->bw_tbl[level].index;
	/* Get the relevant bandwidth */
	peak_bw = ahb_bus_bw_kbps_tbl[tbl_idx];
	if (tbl_idx > 0 && tbl_idx < bus_client->bw_tbl_size)
		aver_bw = (ahb_bus_bw_kbps_tbl[tbl_idx - 1] + peak_bw) >> 1;

	CAM_DBG(CAM_CPAS, "Client:%s RPMH level:%d, Aver BW:%u Peak BW:%u",
		bus_client->name, level, aver_bw, peak_bw);

	return icc_set_bw(bus_client->path, aver_bw, peak_bw);
}

static int _cpas_vote_bus_bw(
	struct cam_cpas_bus_client *bus_client, uint64_t ab, uint64_t ib,
	bool camnoc_bw)
{
	uint64_t min_camnoc_ib_bw = CAM_CPAS_AXI_MIN_CAMNOC_IB_BW;

	if (cam_min_camnoc_ib_bw > 0)
		min_camnoc_ib_bw = (uint64_t)cam_min_camnoc_ib_bw * 1000000L;

	CAM_DBG(CAM_CPAS, "cam_min_camnoc_ib_bw = %d, min_camnoc_ib_bw=%llu",
		cam_min_camnoc_ib_bw, min_camnoc_ib_bw);

	if (!bus_client->valid) {
		CAM_ERR(CAM_CPAS, "bus client not valid");
		return -EINVAL;
	}

	if (bus_client->dyn_vote != true) {
		CAM_ERR(CAM_CPAS, "dynamic update not allowed");
		return -EINVAL;
	}

	if (camnoc_bw == true) {
		if ((ab > 0) && (ab < CAM_CPAS_AXI_MIN_CAMNOC_AB_BW))
			ab = CAM_CPAS_AXI_MIN_CAMNOC_AB_BW;

		if ((ib > 0) && (ib < min_camnoc_ib_bw))
			ib = min_camnoc_ib_bw;
	} else {
		if ((ab > 0) && (ab < CAM_CPAS_AXI_MIN_MNOC_AB_BW))
			ab = CAM_CPAS_AXI_MIN_MNOC_AB_BW;

		if ((ib > 0) && (ib < CAM_CPAS_AXI_MIN_MNOC_IB_BW))
			ib = CAM_CPAS_AXI_MIN_MNOC_IB_BW;
	}

	CAM_DBG(CAM_CPAS, "Bus client=[%s] :ab[%llu] ib[%llu]",
		bus_client->name, ab, ib);

	return icc_set_bw(bus_client->path, ab, ib);
}

static int cam_cpas_util_register_bus_client(
	struct cam_hw_soc_info *soc_info, struct device_node *dev_node,
	struct cam_cpas_bus_client *bus_client, const char *path_name,
	const struct cam_cpas_bus_bw_map *bw_tbl, size_t bw_tbl_size)
{
	bus_client->path = of_icc_get(soc_info->dev, path_name);
	if (IS_ERR_OR_NULL(bus_client->path)) {
		if (PTR_ERR(bus_client->path) != -EPROBE_DEFER)
			CAM_ERR(CAM_CPAS, "Failed to get icc_path for %s (%ld)",
				path_name, PTR_ERR(bus_client->path));
		return PTR_ERR(bus_client->path);
	}

	bus_client->dyn_vote = of_property_read_bool(dev_node,
		"qcom,msm-bus-vector-dyn-vote");
	bus_client->bw_tbl = bw_tbl;
	bus_client->bw_tbl_size = bw_tbl_size;
	bus_client->valid = true;

	mutex_init(&bus_client->lock);
	strncpy(bus_client->name, path_name, sizeof(bus_client->name));

	return 0;
}

static int cam_cpas_util_unregister_bus_client(
	struct cam_cpas_bus_client *bus_client)
{
	if (!bus_client->valid)
		return -EINVAL;

	if (bus_client->dyn_vote)
		_cpas_vote_bus_bw(bus_client, 0, 0, false);
	else
		_cpas_vote_bus_level(bus_client, 0);

	bus_client->valid = false;

	icc_put(bus_client->path);
	mutex_destroy(&bus_client->lock);

	return 0;
}

static int cam_cpas_util_axi_cleanup(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *)soc_info->soc_private;
	struct cam_cpas_axi_port *curr_port;
	struct cam_cpas_axi_port *temp_port;

	list_for_each_entry_safe(curr_port, temp_port,
		&cpas_core->axi_ports_list_head, sibling_port) {
		cam_cpas_util_unregister_bus_client(&curr_port->mnoc_bus);
		of_node_put(curr_port->axi_port_mnoc_node);
		if (soc_private->axi_camnoc_based) {
			cam_cpas_util_unregister_bus_client(
				&curr_port->camnoc_bus);
			of_node_put(curr_port->axi_port_camnoc_node);
		}
		of_node_put(curr_port->axi_port_node);
		list_del(&curr_port->sibling_port);
		mutex_destroy(&curr_port->lock);
		kfree(curr_port);
	}

	of_node_put(soc_private->axi_port_list_node);

	return 0;
}

static int cam_cpas_util_axi_setup(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *)soc_info->soc_private;
	struct cam_cpas_axi_port *axi_port;
	int rc;
	struct device_node *axi_port_list_node;
	struct device_node *axi_port_node = NULL;
	struct device_node *axi_port_mnoc_node = NULL;
	struct device_node *axi_port_camnoc_node = NULL;
	char path_name[32] = {};

	INIT_LIST_HEAD(&cpas_core->axi_ports_list_head);

	axi_port_list_node = of_find_node_by_name(soc_info->pdev->dev.of_node,
		"qcom,axi-port-list");
	if (!axi_port_list_node) {
		CAM_ERR(CAM_CPAS, "Node qcom,axi-port-list not found.");
		return -EINVAL;
	}

	soc_private->axi_port_list_node = axi_port_list_node;

	for_each_available_child_of_node(axi_port_list_node, axi_port_node) {
		axi_port = kzalloc(sizeof(*axi_port), GFP_KERNEL);
		if (!axi_port) {
			rc = -ENOMEM;
			goto error_previous_axi_cleanup;
		}
		axi_port->axi_port_node = axi_port_node;

		rc = of_property_read_string_index(axi_port_node,
			"qcom,axi-port-name", 0,
			(const char **)&axi_port->axi_port_name);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"failed to read qcom,axi-port-name rc=%d", rc);
			goto port_name_fail;
		}

		axi_port_mnoc_node = of_find_node_by_name(axi_port_node,
			"qcom,axi-port-mnoc");
		if (!axi_port_mnoc_node) {
			CAM_ERR(CAM_CPAS, "Node qcom,axi-port-mnoc not found.");
			rc = -EINVAL;
			goto mnoc_node_get_fail;
		}
		axi_port->axi_port_mnoc_node = axi_port_mnoc_node;

		snprintf(path_name, sizeof(path_name), "%s_mnoc",
			 axi_port->axi_port_name);
		rc = cam_cpas_util_register_bus_client(soc_info,
						       axi_port_mnoc_node,
						       &axi_port->mnoc_bus,
						       path_name, NULL, 0);
		if (rc) {
			CAM_INFO(CAM_CPAS, "axi_port:'%s', path:'%s'",
				axi_port->axi_port_name, path_name);
			goto mnoc_register_fail;
		}

		if (soc_private->axi_camnoc_based) {
			axi_port_camnoc_node = of_find_node_by_name(
				axi_port_node, "qcom,axi-port-camnoc");
			if (!axi_port_camnoc_node) {
				CAM_ERR(CAM_CPAS,
					"Node qcom,axi-port-camnoc not found");
				rc = -EINVAL;
				goto camnoc_node_get_fail;
			}
			axi_port->axi_port_camnoc_node = axi_port_camnoc_node;

			snprintf(path_name, sizeof(path_name), "%s_camnoc",
				 axi_port->axi_port_name);
			rc = cam_cpas_util_register_bus_client(soc_info,
							axi_port_camnoc_node,
							&axi_port->camnoc_bus,
							path_name, NULL, 0);
			if (rc) {
				CAM_INFO(CAM_CPAS, "axi_port:'%s', path:'%s'",
					axi_port->axi_port_name, path_name);
					goto camnoc_register_fail;
			}
		}

		mutex_init(&axi_port->lock);

		INIT_LIST_HEAD(&axi_port->sibling_port);
		list_add_tail(&axi_port->sibling_port,
			&cpas_core->axi_ports_list_head);
		INIT_LIST_HEAD(&axi_port->clients_list_head);
	}

	return 0;
camnoc_register_fail:
	of_node_put(axi_port->axi_port_camnoc_node);
camnoc_node_get_fail:
	cam_cpas_util_unregister_bus_client(&axi_port->mnoc_bus);
mnoc_register_fail:
	of_node_put(axi_port->axi_port_mnoc_node);
mnoc_node_get_fail:
port_name_fail:
	of_node_put(axi_port->axi_port_node);
	kfree(axi_port);
error_previous_axi_cleanup:
	cam_cpas_util_axi_cleanup(cpas_core, soc_info);
	return rc;
}

static int cam_cpas_util_vote_default_ahb_axi(struct cam_hw_info *cpas_hw,
	int enable)
{
	int rc;
	struct cam_cpas *cpas_core = (struct cam_cpas *)cpas_hw->core_info;
	struct cam_cpas_axi_port *curr_port;
	struct cam_cpas_axi_port *temp_port;
	uint64_t camnoc_bw, mnoc_bw;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;

	rc = _cpas_vote_bus_level(&cpas_core->ahb_bus_client,
		(enable == true) ? CAM_SVS_VOTE : CAM_SUSPEND_VOTE);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Failed in AHB vote, enable=%d, rc=%d",
			enable, rc);
		return rc;
	}

	if (enable) {
		mnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
		camnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
	} else {
		mnoc_bw = 0;
		camnoc_bw = 0;
	}

	list_for_each_entry_safe(curr_port, temp_port,
		&cpas_core->axi_ports_list_head, sibling_port) {
		rc = _cpas_vote_bus_bw(&curr_port->mnoc_bus,
			mnoc_bw, mnoc_bw, false);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"Failed in mnoc vote, enable=%d, rc=%d",
				enable, rc);
			goto remove_ahb_vote;
		}

		if (soc_private->axi_camnoc_based) {
			_cpas_vote_bus_bw(
				&curr_port->camnoc_bus, 0, camnoc_bw, true);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"Failed in mnoc vote, enable=%d, %d",
					enable, rc);
				_cpas_vote_bus_bw(
					&curr_port->mnoc_bus, 0, 0, false);
				goto remove_ahb_vote;
			}
		}
	}

	return 0;
remove_ahb_vote:
	_cpas_vote_bus_level(&cpas_core->ahb_bus_client,
		CAM_SUSPEND_VOTE);
	return rc;
}

static int cam_cpas_util_insert_client_to_axi_port(struct cam_cpas *cpas_core,
	struct cam_cpas_private_soc *soc_private,
	struct cam_cpas_client *cpas_client, int32_t client_indx)
{
	struct cam_cpas_axi_port *curr_port;
	struct cam_cpas_axi_port *temp_port;

	list_for_each_entry_safe(curr_port, temp_port,
		&cpas_core->axi_ports_list_head, sibling_port) {
		if (strnstr(curr_port->axi_port_name,
			soc_private->client_axi_port_name[client_indx],
			strlen(curr_port->axi_port_name))) {

			cpas_client->axi_port = curr_port;
			INIT_LIST_HEAD(&cpas_client->axi_sibling_client);

			mutex_lock(&curr_port->lock);
			list_add_tail(&cpas_client->axi_sibling_client,
				&cpas_client->axi_port->clients_list_head);
			mutex_unlock(&curr_port->lock);
			break;
		}
	}

	return 0;
}

static void cam_cpas_util_remove_client_from_axi_port(
	struct cam_cpas_client *cpas_client)
{
	mutex_lock(&cpas_client->axi_port->lock);
	list_del(&cpas_client->axi_sibling_client);
	mutex_unlock(&cpas_client->axi_port->lock);
}

static int cam_cpas_hw_reg_write(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, enum cam_cpas_reg_base reg_base,
	uint32_t offset, bool mb, uint32_t value)
{
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	int reg_base_index = cpas_core->regbase_index[reg_base];
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (reg_base_index < 0 || reg_base_index >= soc_info->num_reg_map) {
		CAM_ERR(CAM_CPAS,
			"Invalid reg_base=%d, reg_base_index=%d, num_map=%d",
			reg_base, reg_base_index, soc_info->num_reg_map);
		return -EINVAL;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto unlock_client;
	}

	if (mb)
		cam_io_w_mb(value,
			soc_info->reg_map[reg_base_index].mem_base + offset);
	else
		cam_io_w(value,
			soc_info->reg_map[reg_base_index].mem_base + offset);

unlock_client:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	return rc;
}

static int cam_cpas_hw_reg_read(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, enum cam_cpas_reg_base reg_base,
	uint32_t offset, bool mb, uint32_t *value)
{
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	int reg_base_index = cpas_core->regbase_index[reg_base];
	uint32_t reg_value;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!value)
		return -EINVAL;

	if (reg_base_index < 0 || reg_base_index >= soc_info->num_reg_map) {
		CAM_ERR(CAM_CPAS,
			"Invalid reg_base=%d, reg_base_index=%d, num_map=%d",
			reg_base, reg_base_index, soc_info->num_reg_map);
		return -EINVAL;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto unlock_client;
	}

	if (mb)
		reg_value = cam_io_r_mb(
			soc_info->reg_map[reg_base_index].mem_base + offset);
	else
		reg_value = cam_io_r(
			soc_info->reg_map[reg_base_index].mem_base + offset);

	*value = reg_value;

unlock_client:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	return rc;
}

static int cam_cpas_util_set_camnoc_axi_clk_rate(
	struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_axi_vote consolidated_axi_vote;
	int rc = 0;

	CAM_DBG(CAM_CPAS, "control_camnoc_axi_clk=%d",
		soc_private->control_camnoc_axi_clk);

	if (soc_private->control_camnoc_axi_clk) {
		struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
		struct cam_cpas_axi_port *curr_axi_port = NULL;
		struct cam_cpas_axi_port *temp_axi_port = NULL;
		uint64_t required_camnoc_bw = 0;
		int64_t clk_rate = 0;

		list_for_each_entry_safe(curr_axi_port, temp_axi_port,
			&cpas_core->axi_ports_list_head, sibling_port) {
			consolidated_axi_vote =
				curr_axi_port->consolidated_axi_vote;

			if (consolidated_axi_vote.uncompressed_bw
				> required_camnoc_bw)
				required_camnoc_bw =
					consolidated_axi_vote.uncompressed_bw;

			CAM_DBG(CAM_CPAS, "[%s] : curr=%llu, overal=%llu",
				curr_axi_port->axi_port_name,
				consolidated_axi_vote.uncompressed_bw,
				required_camnoc_bw);
		}

		required_camnoc_bw += (required_camnoc_bw *
			soc_private->camnoc_axi_clk_bw_margin) / 100;

		if ((required_camnoc_bw > 0) &&
			(required_camnoc_bw <
			soc_private->camnoc_axi_min_ib_bw))
			required_camnoc_bw = soc_private->camnoc_axi_min_ib_bw;

		clk_rate = required_camnoc_bw / soc_private->camnoc_bus_width;

		CAM_DBG(CAM_CPAS, "Setting camnoc axi clk rate : %llu %lld",
			required_camnoc_bw, clk_rate);

		rc = cam_soc_util_set_src_clk_rate(soc_info, clk_rate);
		if (rc)
			CAM_ERR(CAM_CPAS,
				"Failed in setting camnoc axi clk %llu %lld %d",
				required_camnoc_bw, clk_rate, rc);
	}

	return rc;
}

static int cam_cpas_util_apply_client_axi_vote(
	struct cam_hw_info *cpas_hw,
	struct cam_cpas_client *cpas_client,
	struct cam_axi_vote *axi_vote)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_cpas_client *curr_client;
	struct cam_cpas_client *temp_client;
	struct cam_axi_vote req_axi_vote = *axi_vote;
	struct cam_cpas_axi_port *axi_port = cpas_client->axi_port;
	uint64_t camnoc_bw = 0, mnoc_bw = 0, mnoc_bw_ab = 0;
	int rc = 0;

	if (!axi_port) {
		CAM_ERR(CAM_CPAS, "axi port does not exists");
		return -EINVAL;
	}

	/*
	 * Make sure we use same bw for both compressed, uncompressed
	 * in case client has requested either of one only
	 */
	if (req_axi_vote.compressed_bw == 0) {
		req_axi_vote.compressed_bw = req_axi_vote.uncompressed_bw;
		req_axi_vote.compressed_bw_ab = req_axi_vote.uncompressed_bw;
	}

	if (req_axi_vote.compressed_bw_ab == 0)
		req_axi_vote.compressed_bw_ab = req_axi_vote.compressed_bw;

	if (req_axi_vote.uncompressed_bw == 0)
		req_axi_vote.uncompressed_bw = req_axi_vote.compressed_bw;

	if ((cpas_client->axi_vote.compressed_bw ==
		req_axi_vote.compressed_bw) &&
		(cpas_client->axi_vote.compressed_bw_ab ==
		req_axi_vote.compressed_bw_ab) &&
		(cpas_client->axi_vote.uncompressed_bw ==
		req_axi_vote.uncompressed_bw))
		return 0;

	mutex_lock(&axi_port->lock);
	cpas_client->axi_vote = req_axi_vote;

	list_for_each_entry_safe(curr_client, temp_client,
		&axi_port->clients_list_head, axi_sibling_client) {
		camnoc_bw += curr_client->axi_vote.uncompressed_bw;
		mnoc_bw += curr_client->axi_vote.compressed_bw;
		mnoc_bw_ab += curr_client->axi_vote.compressed_bw_ab;
	}

	if ((!soc_private->axi_camnoc_based) && (mnoc_bw < camnoc_bw))
		mnoc_bw = camnoc_bw;

	if ((!soc_private->axi_camnoc_based) && (mnoc_bw_ab < camnoc_bw))
		mnoc_bw_ab = mnoc_bw;

	axi_port->consolidated_axi_vote.compressed_bw = mnoc_bw;
	axi_port->consolidated_axi_vote.uncompressed_bw = camnoc_bw;

	rc = _cpas_vote_bus_bw(&axi_port->mnoc_bus,
		mnoc_bw, mnoc_bw, false);
	if (rc) {
		CAM_ERR(CAM_CPAS,
			"Failed in mnoc vote ab[%llu] ib[%llu] rc=%d",
			mnoc_bw_ab, mnoc_bw, rc);
		goto unlock_axi_port;
	}

	if (soc_private->axi_camnoc_based) {
		rc = _cpas_vote_bus_bw(&axi_port->camnoc_bus,
			0, camnoc_bw, true);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"Failed camnoc vote ab[%llu] ib[%llu] rc=%d",
				(uint64_t)0, camnoc_bw, rc);
			goto unlock_axi_port;
		}
	}

	mutex_unlock(&axi_port->lock);

	rc = cam_cpas_util_set_camnoc_axi_clk_rate(cpas_hw);
	if (rc)
		CAM_ERR(CAM_CPAS, "Failed in setting axi clk rate rc=%d", rc);

	return rc;

unlock_axi_port:
	mutex_unlock(&axi_port->lock);
	return rc;
}

static int cam_cpas_hw_update_axi_vote(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, struct cam_axi_vote *client_axi_vote)
{
	struct cam_axi_vote axi_vote;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!client_axi_vote) {
		CAM_ERR(CAM_CPAS, "Invalid arg client_handle=%d",
			client_handle);
		return -EINVAL;
	}

	axi_vote = *client_axi_vote;

	if ((axi_vote.compressed_bw == 0) &&
		(axi_vote.uncompressed_bw == 0) &&
		(axi_vote.compressed_bw_ab == 0)) {
		CAM_DBG(CAM_CPAS, "0 vote from client_handle=%d",
			client_handle);
		axi_vote.compressed_bw = CAM_CPAS_DEFAULT_AXI_BW;
		axi_vote.uncompressed_bw = CAM_CPAS_DEFAULT_AXI_BW;
		axi_vote.compressed_bw_ab = CAM_CPAS_DEFAULT_AXI_BW;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto unlock_client;
	}

	CAM_DBG(CAM_PERF,
		"Client=[%d][%s][%d] Req comp[%llu], comp_ab[%llu], uncomp[%llu]",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, axi_vote.compressed_bw,
		axi_vote.compressed_bw_ab, axi_vote.uncompressed_bw);

	rc = cam_cpas_util_apply_client_axi_vote(cpas_hw,
		cpas_core->cpas_client[client_indx], &axi_vote);

unlock_client:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_util_get_ahb_level(struct cam_hw_info *cpas_hw,
	struct device *dev, unsigned long freq, enum cam_vote_level *req_level)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct dev_pm_opp *opp;
	unsigned int corner;
	enum cam_vote_level level = CAM_SVS_VOTE;
	unsigned long corner_freq = freq;
	int i;

	if (!dev || !req_level) {
		CAM_ERR(CAM_CPAS, "Invalid params %pK, %pK", dev, req_level);
		return -EINVAL;
	}

	opp = dev_pm_opp_find_freq_ceil(dev, &corner_freq);
	if (IS_ERR(opp)) {
		CAM_DBG(CAM_CPAS, "OPP Ceil not available for freq :%ld, %pK",
			corner_freq, opp);
		*req_level = CAM_TURBO_VOTE;
		return 0;
	}

	corner = dev_pm_opp_get_voltage(opp);

	for (i = 0; i < soc_private->num_vdd_ahb_mapping; i++)
		if (corner == soc_private->vdd_ahb[i].vdd_corner)
			level = soc_private->vdd_ahb[i].ahb_level;

	CAM_DBG(CAM_CPAS,
		"From OPP table : freq=[%ld][%ld], corner=%d, level=%d",
		freq, corner_freq, corner, level);

	*req_level = level;

	return 0;
}

static int cam_cpas_util_apply_client_ahb_vote(struct cam_hw_info *cpas_hw,
	struct cam_cpas_client *cpas_client, struct cam_ahb_vote *ahb_vote,
	enum cam_vote_level *applied_level)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_bus_client *ahb_bus_client = &cpas_core->ahb_bus_client;
	enum cam_vote_level required_level;
	enum cam_vote_level highest_level;
	int i, rc = 0;

	if (!ahb_bus_client->valid) {
		CAM_ERR(CAM_CPAS, "AHB Bus client not valid");
		return -EINVAL;
	}

	if (ahb_vote->type == CAM_VOTE_DYNAMIC) {
		rc = cam_cpas_util_get_ahb_level(cpas_hw, cpas_client->data.dev,
			ahb_vote->vote.freq, &required_level);
		if (rc)
			return rc;
	} else {
		required_level = ahb_vote->vote.level;
	}

	if (cpas_client->ahb_level == required_level)
		return 0;

	mutex_lock(&ahb_bus_client->lock);
	cpas_client->ahb_level = required_level;

	CAM_DBG(CAM_CPAS, "Client=[%s] required level[%d]",
		ahb_bus_client->name, required_level);

	highest_level = required_level;
	for (i = 0; i < cpas_core->num_clients; i++) {
		if (cpas_core->cpas_client[i] && (highest_level <
			cpas_core->cpas_client[i]->ahb_level))
			highest_level = cpas_core->cpas_client[i]->ahb_level;
	}

	CAM_DBG(CAM_CPAS, "Required highest_level[%d]", highest_level);

	rc = _cpas_vote_bus_level(ahb_bus_client,
		highest_level);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Failed in ahb vote, level=%d, rc=%d",
			highest_level, rc);
		goto unlock_bus_client;
	}

	rc = cam_soc_util_set_clk_rate_level(&cpas_hw->soc_info, highest_level);
	if (rc) {
		CAM_ERR(CAM_CPAS,
			"Failed in scaling clock rate level %d for AHB",
			highest_level);
		goto unlock_bus_client;
	}

	if (applied_level)
		*applied_level = highest_level;

unlock_bus_client:
	mutex_unlock(&ahb_bus_client->lock);
	return rc;
}

static int cam_cpas_hw_update_ahb_vote(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, struct cam_ahb_vote *client_ahb_vote)
{
	struct cam_ahb_vote ahb_vote;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!client_ahb_vote) {
		CAM_ERR(CAM_CPAS, "Invalid input arg");
		return -EINVAL;
	}

	ahb_vote = *client_ahb_vote;

	if (ahb_vote.vote.level == 0) {
		CAM_DBG(CAM_CPAS, "0 ahb vote from client %d",
			client_handle);
		ahb_vote.type = CAM_VOTE_ABSOLUTE;
		ahb_vote.vote.level = CAM_SVS_VOTE;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto unlock_client;
	}

	CAM_DBG(CAM_PERF,
		"client=[%d][%s][%d] : type[%d], level[%d], freq[%ld], applied[%d]",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, ahb_vote.type,
		ahb_vote.vote.level, ahb_vote.vote.freq,
		cpas_core->cpas_client[client_indx]->ahb_level);

	rc = cam_cpas_util_apply_client_ahb_vote(cpas_hw,
		cpas_core->cpas_client[client_indx], &ahb_vote, NULL);

unlock_client:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_hw_start(void *hw_priv, void *start_args,
	uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	uint32_t client_indx;
	struct cam_cpas_hw_cmd_start *cmd_hw_start;
	struct cam_cpas_client *cpas_client;
	struct cam_ahb_vote *ahb_vote;
	struct cam_axi_vote *axi_vote;
	enum cam_vote_level applied_level = CAM_SVS_VOTE;
	int rc;

	if (!hw_priv || !start_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, start_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_cmd_start) != arg_size) {
		CAM_ERR(CAM_CPAS, "HW_CAPS size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_cmd_start), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	cmd_hw_start = (struct cam_cpas_hw_cmd_start *)start_args;
	client_indx = CAM_CPAS_GET_CLIENT_IDX(cmd_hw_start->client_handle);
	ahb_vote = cmd_hw_start->ahb_vote;
	axi_vote = cmd_hw_start->axi_vote;

	if (!ahb_vote || !axi_vote)
		return -EINVAL;

	if ((ahb_vote->vote.level == 0) || ((axi_vote->compressed_bw == 0) &&
		(axi_vote->uncompressed_bw == 0))) {
		CAM_ERR(CAM_CPAS, "Invalid vote ahb[%d], axi[%llu], [%llu]",
			ahb_vote->vote.level, axi_vote->compressed_bw,
			axi_vote->uncompressed_bw);
		return -EINVAL;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx)) {
		CAM_ERR(CAM_CPAS, "Client index invalid %d", client_indx);
		return -EINVAL;
	}

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_REGISTERED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d] is not registered",
			client_indx);
		rc = -EPERM;
		goto done;
	}

	if (CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] is in start state",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto done;
	}

	CAM_DBG(CAM_CPAS,
		"AHB :client=[%d][%s][%d] type[%d], level[%d], applied[%d]",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index,
		ahb_vote->type, ahb_vote->vote.level, cpas_client->ahb_level);
	rc = cam_cpas_util_apply_client_ahb_vote(cpas_hw, cpas_client,
		ahb_vote, &applied_level);
	if (rc)
		goto done;

	rc = cam_cpas_util_apply_client_axi_vote(cpas_hw,
		cpas_client, axi_vote);
	if (rc)
		goto done;

	CAM_DBG(CAM_CPAS,
		"AXI client=[%d][%s][%d] comp:%llu, comp_ab:%llu, uncomp:%llu",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, axi_vote->compressed_bw,
		axi_vote->compressed_bw_ab, axi_vote->uncompressed_bw);

	if (cpas_core->streamon_clients == 0) {
		atomic_set(&cpas_core->irq_count, 1);
		rc = cam_cpas_soc_enable_resources(&cpas_hw->soc_info,
			applied_level);
		if (rc) {
			atomic_set(&cpas_core->irq_count, 0);
			CAM_ERR(CAM_CPAS, "enable_resorce failed, rc=%d", rc);
			goto done;
		}

		if (cpas_core->internal_ops.power_on) {
			rc = cpas_core->internal_ops.power_on(cpas_hw);
			if (rc) {
				atomic_set(&cpas_core->irq_count, 0);
				cam_cpas_soc_disable_resources(
					&cpas_hw->soc_info, true, true);
				CAM_ERR(CAM_CPAS,
					"failed in power_on settings rc=%d",
					rc);
				goto done;
			}
		}
		CAM_DBG(CAM_CPAS, "irq_count=%d\n",
			atomic_read(&cpas_core->irq_count));
		cpas_hw->hw_state = CAM_HW_STATE_POWER_UP;
	}

	cpas_client->started = true;
	cpas_core->streamon_clients++;

	CAM_DBG(CAM_CPAS, "client=[%d][%s][%d] streamon_clients=%d",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, cpas_core->streamon_clients);
done:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int _check_irq_count(struct cam_cpas *cpas_core)
{
	return (atomic_read(&cpas_core->irq_count) > 0) ? 0 : 1;
}

static int cam_cpas_hw_stop(void *hw_priv, void *stop_args,
	uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	uint32_t client_indx;
	struct cam_cpas_hw_cmd_stop *cmd_hw_stop;
	struct cam_cpas_client *cpas_client;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	int rc = 0;
	long result;

	if (!hw_priv || !stop_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, stop_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_cmd_stop) != arg_size) {
		CAM_ERR(CAM_CPAS, "HW_CAPS size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_cmd_stop), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	cmd_hw_stop = (struct cam_cpas_hw_cmd_stop *)stop_args;
	client_indx = CAM_CPAS_GET_CLIENT_IDX(cmd_hw_stop->client_handle);

	if (!CAM_CPAS_CLIENT_VALID(client_indx)) {
		CAM_ERR(CAM_CPAS, "Client index invalid %d", client_indx);
		return -EINVAL;
	}

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	CAM_DBG(CAM_CPAS, "Client=[%d][%s][%d] streamon_clients=%d",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, cpas_core->streamon_clients);

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "Client=[%d][%s][%d] is not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto done;
	}

	cpas_client->started = false;
	cpas_core->streamon_clients--;

	if (cpas_core->streamon_clients == 0) {
		if (cpas_core->internal_ops.power_off) {
			rc = cpas_core->internal_ops.power_off(cpas_hw);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"failed in power_off settings rc=%d",
					rc);
				/* Do not return error, passthrough */
			}
		}

		rc = cam_cpas_soc_disable_irq(&cpas_hw->soc_info);
		if (rc) {
			CAM_ERR(CAM_CPAS, "disable_irq failed, rc=%d", rc);
			goto done;
		}

		/* Wait for any IRQs still being handled */
		atomic_dec(&cpas_core->irq_count);
		result = wait_event_timeout(cpas_core->irq_count_wq,
			_check_irq_count(cpas_core), HZ);
		if (result == 0) {
			CAM_ERR(CAM_CPAS, "Wait failed: irq_count=%d",
				atomic_read(&cpas_core->irq_count));
		}

		rc = cam_cpas_soc_disable_resources(&cpas_hw->soc_info,
			true, false);
		if (rc) {
			CAM_ERR(CAM_CPAS, "disable_resorce failed, rc=%d", rc);
			goto done;
		}
		CAM_DBG(CAM_CPAS, "Disabled all the resources: irq_count=%d\n",
			atomic_read(&cpas_core->irq_count));
		cpas_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	}

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SUSPEND_VOTE;
	rc = cam_cpas_util_apply_client_ahb_vote(cpas_hw, cpas_client,
		&ahb_vote, NULL);
	if (rc) {
		CAM_ERR(CAM_CPAS, "ahb vote failed for %s rc %d",
			cpas_client->data.identifier, rc);
		goto done;
	}

	axi_vote.uncompressed_bw = 0;
	axi_vote.compressed_bw = 0;
	axi_vote.compressed_bw_ab = 0;
	rc = cam_cpas_util_apply_client_axi_vote(cpas_hw,
		cpas_client, &axi_vote);
	if (rc)
		CAM_ERR(CAM_CPAS, "axi vote failed for %s rc %d",
			cpas_client->data.identifier, rc);

done:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_hw_init(void *hw_priv, void *init_hw_args,
	uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	int rc = 0;

	if (!hw_priv || !init_hw_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, init_hw_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_caps) != arg_size) {
		CAM_ERR(CAM_CPAS, "INIT HW size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_caps), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *)cpas_hw->core_info;

	if (cpas_core->internal_ops.init_hw_version) {
		rc = cpas_core->internal_ops.init_hw_version(cpas_hw,
			(struct cam_cpas_hw_caps *)init_hw_args);
	}

	return rc;
}

static int cam_cpas_hw_register_client(struct cam_hw_info *cpas_hw,
	struct cam_cpas_register_params *register_params)
{
	int rc;
	struct cam_cpas_client *cpas_client;
	char client_name[CAM_HW_IDENTIFIER_LENGTH + 3];
	u32 client_indx;
	s32 idx = -1;
	struct cam_cpas *cpas_core = (struct cam_cpas *)cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;

	CAM_DBG(CAM_CPAS, "Register params : identifier=%s, cell_index=%d",
		register_params->identifier, register_params->cell_index);

	if (soc_private->client_id_based)
		snprintf(client_name, sizeof(client_name), "%s%d",
			register_params->identifier,
			register_params->cell_index);
	else
		snprintf(client_name, sizeof(client_name), "%s",
			register_params->identifier);

	mutex_lock(&cpas_hw->hw_mutex);

	rc = cam_common_util_get_string_index(soc_private->client_name,
		soc_private->num_clients, client_name, &idx);

	if (rc) {
		CAM_ERR(CAM_CPAS, "No match found for client %s",
			client_name);
		mutex_unlock(&cpas_hw->hw_mutex);
		return rc;
	}

	client_indx = idx;
	if (idx < 0 || !CAM_CPAS_CLIENT_VALID(client_indx)) {
		CAM_ERR(CAM_CPAS,
			"Inval client %s %d : %d",
			register_params->identifier,
			register_params->cell_index, idx);
		mutex_unlock(&cpas_hw->hw_mutex);
		return -EPERM;
	}

	mutex_lock(&cpas_core->client_mutex[client_indx]);

	if (CAM_CPAS_CLIENT_REGISTERED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS,
			"Client already registered %s %d : %pK",
			register_params->identifier,
			register_params->cell_index,
			cpas_core->cpas_client[client_indx]);
		mutex_unlock(&cpas_core->client_mutex[client_indx]);
		mutex_unlock(&cpas_hw->hw_mutex);
		return -EPERM;
	}

	cpas_client = kzalloc(sizeof(struct cam_cpas_client), GFP_KERNEL);
	if (!cpas_client) {
		mutex_unlock(&cpas_core->client_mutex[client_indx]);
		mutex_unlock(&cpas_hw->hw_mutex);
		return -ENOMEM;
	}

	rc = cam_cpas_util_insert_client_to_axi_port(cpas_core, soc_private,
		cpas_client, client_indx);
	if (rc) {
		CAM_ERR(CAM_CPAS,
			"axi_port_insert failed Client=[%d][%s][%d], rc=%d",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index, rc);
		kfree(cpas_client);
		mutex_unlock(&cpas_core->client_mutex[client_indx]);
		mutex_unlock(&cpas_hw->hw_mutex);
		return -EINVAL;
	}

	register_params->client_handle =
		CAM_CPAS_GET_CLIENT_HANDLE(client_indx);
	memcpy(&cpas_client->data, register_params,
		sizeof(struct cam_cpas_register_params));
	cpas_core->cpas_client[client_indx] = cpas_client;
	cpas_core->registered_clients++;

	CAM_DBG(CAM_CPAS, "client=[%d][%s][%d], registered_clients=%d",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, cpas_core->registered_clients);

	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);

	return 0;
}

static int cam_cpas_hw_unregister_client(struct cam_hw_info *cpas_hw,
	uint32_t client_handle)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_REGISTERED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "Client=[%d][%s][%d] not registered",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto done;
	}

	if (CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "Client=[%d][%s][%d] is not stopped",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto done;
	}

	cam_cpas_util_remove_client_from_axi_port(
		cpas_core->cpas_client[client_indx]);

	CAM_DBG(CAM_CPAS, "client=[%d][%s][%d], registered_clients=%d",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, cpas_core->registered_clients);

	kfree(cpas_core->cpas_client[client_indx]);
	cpas_core->cpas_client[client_indx] = NULL;
	cpas_core->registered_clients--;
done:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_hw_get_hw_info(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	struct cam_cpas_hw_caps *hw_caps;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, get_hw_cap_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_caps) != arg_size) {
		CAM_ERR(CAM_CPAS, "HW_CAPS size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_caps), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	hw_caps = (struct cam_cpas_hw_caps *)get_hw_cap_args;

	*hw_caps = cpas_core->hw_caps;

	return 0;
}


static int cam_cpas_hw_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!hw_priv || !cmd_args ||
		(cmd_type >= CAM_CPAS_HW_CMD_INVALID)) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK %d",
			hw_priv, cmd_args, cmd_type);
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_CPAS_HW_CMD_REGISTER_CLIENT: {
		struct cam_cpas_register_params *register_params;

		if (sizeof(struct cam_cpas_register_params) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		register_params = (struct cam_cpas_register_params *)cmd_args;
		rc = cam_cpas_hw_register_client(hw_priv, register_params);
		break;
	}
	case CAM_CPAS_HW_CMD_UNREGISTER_CLIENT: {
		uint32_t *client_handle;

		if (sizeof(uint32_t) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		client_handle = (uint32_t *)cmd_args;
		rc = cam_cpas_hw_unregister_client(hw_priv, *client_handle);
		break;
	}
	case CAM_CPAS_HW_CMD_REG_WRITE: {
		struct cam_cpas_hw_cmd_reg_read_write *reg_write;

		if (sizeof(struct cam_cpas_hw_cmd_reg_read_write) !=
			arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		reg_write =
			(struct cam_cpas_hw_cmd_reg_read_write *)cmd_args;
		rc = cam_cpas_hw_reg_write(hw_priv, reg_write->client_handle,
			reg_write->reg_base, reg_write->offset, reg_write->mb,
			reg_write->value);
		break;
	}
	case CAM_CPAS_HW_CMD_REG_READ: {
		struct cam_cpas_hw_cmd_reg_read_write *reg_read;

		if (sizeof(struct cam_cpas_hw_cmd_reg_read_write) !=
			arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		reg_read =
			(struct cam_cpas_hw_cmd_reg_read_write *)cmd_args;
		rc = cam_cpas_hw_reg_read(hw_priv,
			reg_read->client_handle, reg_read->reg_base,
			reg_read->offset, reg_read->mb, &reg_read->value);

		break;
	}
	case CAM_CPAS_HW_CMD_AHB_VOTE: {
		struct cam_cpas_hw_cmd_ahb_vote *cmd_ahb_vote;

		if (sizeof(struct cam_cpas_hw_cmd_ahb_vote) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		cmd_ahb_vote = (struct cam_cpas_hw_cmd_ahb_vote *)cmd_args;
		rc = cam_cpas_hw_update_ahb_vote(hw_priv,
			cmd_ahb_vote->client_handle, cmd_ahb_vote->ahb_vote);
		break;
	}
	case CAM_CPAS_HW_CMD_AXI_VOTE: {
		struct cam_cpas_hw_cmd_axi_vote *cmd_axi_vote;

		if (sizeof(struct cam_cpas_hw_cmd_axi_vote) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		cmd_axi_vote = (struct cam_cpas_hw_cmd_axi_vote *)cmd_args;
		rc = cam_cpas_hw_update_axi_vote(hw_priv,
			cmd_axi_vote->client_handle, cmd_axi_vote->axi_vote);
		break;
	}
	default:
		CAM_ERR(CAM_CPAS, "CPAS HW command not valid =%d", cmd_type);
		break;
	}

	return rc;
}

static int cam_cpas_util_client_setup(struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	int i;

	for (i = 0; i < CAM_CPAS_MAX_CLIENTS; i++) {
		mutex_init(&cpas_core->client_mutex[i]);
		cpas_core->cpas_client[i] = NULL;
	}

	return 0;
}

static int cam_cpas_util_client_cleanup(struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	int i;

	for (i = 0; i < CAM_CPAS_MAX_CLIENTS; i++) {
		if (cpas_core->cpas_client[i]) {
			cam_cpas_hw_unregister_client(cpas_hw, i);
			cpas_core->cpas_client[i] = NULL;
		}
		mutex_destroy(&cpas_core->client_mutex[i]);
	}

	return 0;
}

static int cam_cpas_util_get_internal_ops(struct platform_device *pdev,
	struct cam_hw_intf *hw_intf, struct cam_cpas_internal_ops *internal_ops)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc;
	const char *compat_str = NULL;

	rc = of_property_read_string_index(of_node, "arch-compat", 0,
		(const char **)&compat_str);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed to get arch-compat rc=%d", rc);
		return -EINVAL;
	}

	if (strnstr(compat_str, "camss_top", strlen(compat_str))) {
		hw_intf->hw_type = CAM_HW_CAMSSTOP;
		rc = cam_camsstop_get_internal_ops(internal_ops);
	} else if (strnstr(compat_str, "cpas_top", strlen(compat_str))) {
		hw_intf->hw_type = CAM_HW_CPASTOP;
		rc = cam_cpastop_get_internal_ops(internal_ops);
	} else {
		CAM_ERR(CAM_CPAS, "arch-compat %s not supported", compat_str);
		rc = -EINVAL;
	}

	return rc;
}

int cam_cpas_hw_probe(struct platform_device *pdev,
	struct cam_hw_intf **hw_intf)
{
	int rc = 0;
	int i;
	struct cam_hw_info *cpas_hw = NULL;
	struct cam_hw_intf *cpas_hw_intf = NULL;
	struct cam_cpas *cpas_core = NULL;
	struct cam_cpas_private_soc *soc_private;
	struct cam_cpas_internal_ops *internal_ops;

	cpas_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!cpas_hw_intf)
		return -ENOMEM;

	cpas_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!cpas_hw) {
		kfree(cpas_hw_intf);
		return -ENOMEM;
	}

	cpas_core = kzalloc(sizeof(struct cam_cpas), GFP_KERNEL);
	if (!cpas_core) {
		kfree(cpas_hw);
		kfree(cpas_hw_intf);
		return -ENOMEM;
	}

	for (i = 0; i < CAM_CPAS_REG_MAX; i++)
		cpas_core->regbase_index[i] = -1;

	cpas_hw_intf->hw_priv = cpas_hw;
	cpas_hw->core_info = cpas_core;

	cpas_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	cpas_hw->soc_info.pdev = pdev;
	cpas_hw->soc_info.dev = &pdev->dev;
	cpas_hw->soc_info.dev_name = pdev->name;
	cpas_hw->open_count = 0;
	mutex_init(&cpas_hw->hw_mutex);
	spin_lock_init(&cpas_hw->hw_lock);
	init_completion(&cpas_hw->hw_complete);

	cpas_hw_intf->hw_ops.get_hw_caps = cam_cpas_hw_get_hw_info;
	cpas_hw_intf->hw_ops.init = cam_cpas_hw_init;
	cpas_hw_intf->hw_ops.deinit = NULL;
	cpas_hw_intf->hw_ops.reset = NULL;
	cpas_hw_intf->hw_ops.reserve = NULL;
	cpas_hw_intf->hw_ops.release = NULL;
	cpas_hw_intf->hw_ops.start = cam_cpas_hw_start;
	cpas_hw_intf->hw_ops.stop = cam_cpas_hw_stop;
	cpas_hw_intf->hw_ops.read = NULL;
	cpas_hw_intf->hw_ops.write = NULL;
	cpas_hw_intf->hw_ops.process_cmd = cam_cpas_hw_process_cmd;

	cpas_core->work_queue = alloc_workqueue("cam-cpas",
		WQ_UNBOUND | WQ_MEM_RECLAIM, CAM_CPAS_INFLIGHT_WORKS);
	if (!cpas_core->work_queue) {
		rc = -ENOMEM;
		goto release_mem;
	}

	internal_ops = &cpas_core->internal_ops;
	rc = cam_cpas_util_get_internal_ops(pdev, cpas_hw_intf, internal_ops);
	if (rc)
		goto release_workq;

	rc = cam_cpas_soc_init_resources(&cpas_hw->soc_info,
		internal_ops->handle_irq, cpas_hw);
	if (rc)
		goto release_workq;

	soc_private = (struct cam_cpas_private_soc *)
		cpas_hw->soc_info.soc_private;
	cpas_core->num_clients = soc_private->num_clients;
	atomic_set(&cpas_core->irq_count, 0);
	init_waitqueue_head(&cpas_core->irq_count_wq);

	if (internal_ops->setup_regbase) {
		rc = internal_ops->setup_regbase(&cpas_hw->soc_info,
			cpas_core->regbase_index, CAM_CPAS_REG_MAX);
		if (rc)
			goto deinit_platform_res;
	}

	rc = cam_cpas_util_client_setup(cpas_hw);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in client setup, rc=%d", rc);
		goto deinit_platform_res;
	}

	rc = cam_cpas_util_register_bus_client(&cpas_hw->soc_info,
		cpas_hw->soc_info.pdev->dev.of_node,
		&cpas_core->ahb_bus_client, "cam_ahb",
		ahb_bus_map, ARRAY_SIZE(ahb_bus_map));
	if (rc) {
		CAM_INFO(CAM_CPAS, "Cannot setup AHB");
		goto client_cleanup;
	}

	rc = cam_cpas_util_axi_setup(cpas_core, &cpas_hw->soc_info);
	if (rc) {
		CAM_WARN(CAM_CPAS, "Cannot setup AXI");
		goto ahb_cleanup;
	}

	/* Need to vote first before enabling clocks */
	rc = cam_cpas_util_vote_default_ahb_axi(cpas_hw, true);
	if (rc)
		goto axi_cleanup;

	rc = cam_cpas_soc_enable_resources(&cpas_hw->soc_info, CAM_SVS_VOTE);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in soc_enable_resources, rc=%d", rc);
		goto remove_default_vote;
	}

	if (internal_ops->get_hw_info) {
		rc = internal_ops->get_hw_info(cpas_hw, &cpas_core->hw_caps);
		if (rc) {
			CAM_ERR(CAM_CPAS, "failed in get_hw_info, rc=%d", rc);
			goto disable_soc_res;
		}
	} else {
		CAM_ERR(CAM_CPAS, "Invalid get_hw_info");
		goto disable_soc_res;
	}

	rc = cam_cpas_hw_init(cpas_hw_intf->hw_priv,
		&cpas_core->hw_caps, sizeof(struct cam_cpas_hw_caps));
	if (rc)
		goto disable_soc_res;

	rc = cam_cpas_soc_disable_resources(&cpas_hw->soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in soc_disable_resources, rc=%d", rc);
		goto remove_default_vote;
	}

	rc = cam_cpas_util_vote_default_ahb_axi(cpas_hw, false);
	if (rc)
		goto axi_cleanup;

	*hw_intf = cpas_hw_intf;

	CAM_DBG(CAM_CPAS, "Probe success");

	return 0;

disable_soc_res:
	cam_cpas_soc_disable_resources(&cpas_hw->soc_info, true, true);
remove_default_vote:
	cam_cpas_util_vote_default_ahb_axi(cpas_hw, false);
axi_cleanup:
	cam_cpas_util_axi_cleanup(cpas_core, &cpas_hw->soc_info);
ahb_cleanup:
	cam_cpas_util_unregister_bus_client(&cpas_core->ahb_bus_client);
client_cleanup:
	cam_cpas_util_client_cleanup(cpas_hw);
deinit_platform_res:
	cam_cpas_soc_deinit_resources(&cpas_hw->soc_info);
release_workq:
	flush_workqueue(cpas_core->work_queue);
	destroy_workqueue(cpas_core->work_queue);
release_mem:
	mutex_destroy(&cpas_hw->hw_mutex);
	kfree(cpas_core);
	kfree(cpas_hw);
	kfree(cpas_hw_intf);

	if (rc != -EPROBE_DEFER)
		CAM_ERR(CAM_CPAS, "Failed in hw probe rc=%d", rc);
	else
		CAM_DBG(CAM_CPAS, "HW probe deferred");

	return rc;
}

int cam_cpas_hw_remove(struct cam_hw_intf *cpas_hw_intf)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;

	if (!cpas_hw_intf) {
		CAM_ERR(CAM_CPAS, "cpas interface not initialized");
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)cpas_hw_intf->hw_priv;
	cpas_core = (struct cam_cpas *)cpas_hw->core_info;

	if (cpas_hw->hw_state == CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_CPAS, "cpas hw is in power up state");
		return -EINVAL;
	}

	cam_cpas_util_axi_cleanup(cpas_core, &cpas_hw->soc_info);
	cam_cpas_util_unregister_bus_client(&cpas_core->ahb_bus_client);
	cam_cpas_util_client_cleanup(cpas_hw);
	cam_cpas_soc_deinit_resources(&cpas_hw->soc_info);
	flush_workqueue(cpas_core->work_queue);
	destroy_workqueue(cpas_core->work_queue);
	mutex_destroy(&cpas_hw->hw_mutex);
	kfree(cpas_core);
	kfree(cpas_hw);
	kfree(cpas_hw_intf);

	return 0;
}
