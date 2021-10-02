/******************************************************************************
 *
 * Copyright(c) 2016 - 2018 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/

#ifndef __HALMAC_PCIE_REG_H__
#define __HALMAC_PCIE_REG_H__

/* PCIE PHY register */
#define RAC_CTRL_PPR		0x00
#define RAC_SET_PPR		0x20
#define RAC_TRG_PPR		0x21

/* PCIE CFG register */
#define PCIE_L1_BACKDOOR		0x719
#define PCIE_ASPM_CTRL			0x70F

/* PCIE MAC register */
#define LINK_CTRL2_REG_OFFSET		0xA0
#define GEN2_CTRL_OFFSET		0x80C
#define LINK_STATUS_REG_OFFSET		0x82

#define PCIE_GEN1_SPEED			0x01
#define PCIE_GEN2_SPEED			0x02

#endif/* __HALMAC_PCIE_REG_H__ */
