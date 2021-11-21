// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#include <linux/wwan.h>

#include "t7xx_dev_node.h"
#include "t7xx_tty_ops.h"
#include "t7xx_hif_cldma.h"
#include "t7xx_port_config.h"

#define DATA_AT_CMD_Q	5

static struct t7xx_port md1_ccci_ports[] = {
	{CCCI_UART2_TX, CCCI_UART2_RX, DATA_AT_CMD_Q, DATA_AT_CMD_Q, 0xFF,
	 0xFF, ID_CLDMA1, PORT_F_WITH_CHAR_NODE, &wwan_sub_port_ops, 0, "ttyC0", WWAN_PORT_AT},
	{CCCI_MIPC_TX, CCCI_MIPC_RX, 2, 2, 0, 0, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &tty_port_ops, 1, "ttyCMIPC0", WWAN_PORT_AT},
	{CCCI_MD_LOG_TX, CCCI_MD_LOG_RX, 7, 7, 7, 7, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 2, "ttyCMdLog", WWAN_PORT_AT},
	{CCCI_LB_IT_TX, CCCI_LB_IT_RX, 0, 0, 0xFF, 0xFF, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 3, "ccci_lb_it",},
	{CCCI_MIPC_TX, CCCI_MIPC_RX, 2, 2, 0, 0, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &tty_port_ops, 1, "ttyCMIPC0",},
	{CCCI_MBIM_TX, CCCI_MBIM_RX, 2, 2, 0, 0, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &wwan_sub_port_ops, 10, "ttyCMBIM0", WWAN_PORT_MBIM},
	{CCCI_UART1_TX, CCCI_UART1_RX, 1, 1, 1, 1, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 11, "ttyCMdMeta",},
	{CCCI_DSS0_TX, CCCI_DSS0_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 13, "ttyCMBIMDSS0",},
	{CCCI_DSS1_TX, CCCI_DSS1_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 14, "ttyCMBIMDSS1",},
	{CCCI_DSS2_TX, CCCI_DSS2_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 15, "ttyCMBIMDSS2",},
	{CCCI_DSS3_TX, CCCI_DSS3_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 16, "ttyCMBIMDSS3",},
	{CCCI_DSS4_TX, CCCI_DSS4_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 17, "ttyCMBIMDSS4",},
	{CCCI_DSS5_TX, CCCI_DSS5_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 18, "ttyCMBIMDSS5",},
	{CCCI_DSS6_TX, CCCI_DSS6_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 19, "ttyCMBIMDSS6",},
	{CCCI_DSS7_TX, CCCI_DSS7_RX, 3, 3, 3, 3, ID_CLDMA1,
	 PORT_F_WITH_CHAR_NODE, &char_port_ops, 20, "ttyCMBIMDSS7",},
	{CCCI_CONTROL_TX, CCCI_CONTROL_RX, 0, 0, 0, 0, ID_CLDMA1,
	 0, &ctl_port_ops, 0xFF, "ccci_ctrl",},
	{CCCI_STATUS_TX, CCCI_STATUS_RX, 0, 0, 0, 0, ID_CLDMA1,
	 0, &poller_port_ops, 0xFF, "ccci_poll",},
};

int port_get_cfg(struct t7xx_port **ports)
{
	*ports = md1_ccci_ports;
	return ARRAY_SIZE(md1_ccci_ports);
}
