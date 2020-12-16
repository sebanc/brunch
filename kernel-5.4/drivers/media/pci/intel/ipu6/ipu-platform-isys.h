/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2020 Intel Corporation */

#ifndef IPU_PLATFORM_ISYS_H
#define IPU_PLATFORM_ISYS_H

#define IPU_ISYS_ENTITY_PREFIX		"Intel IPU6"

/*
 * FW support max 16 streams
 */
#define IPU_ISYS_MAX_STREAMS		16

#define ISYS_UNISPART_IRQS	(IPU_ISYS_UNISPART_IRQ_SW |	\
				 IPU_ISYS_UNISPART_IRQ_CSI0 |	\
				 IPU_ISYS_UNISPART_IRQ_CSI1)

#endif
