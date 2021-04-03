/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __PCI_OPS_H_
#define __PCI_OPS_H_


	u32	rtl8723de_init_desc_ring(struct adapter *adapt);
	u32	rtl8723de_free_desc_ring(struct adapter *adapt);
	void	rtl8723de_reset_desc_ring(struct adapter *adapt);
	int	rtl8723de_interrupt(struct adapter * Adapter);
	void	rtl8723de_recv_tasklet(void *priv);
	void	rtl8723de_prepare_bcn_tasklet(void *priv);
	void	rtl8723de_set_intf_ops(struct _io_ops	*pops);
	u8 check_tx_desc_resource(struct adapter *adapt, int prio);

#endif
