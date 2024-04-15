/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

void ithc_debug_init_module(void);
void ithc_debug_exit_module(void);
int ithc_debug_init_device(struct ithc *ithc);
void ithc_log_regs(struct ithc *ithc);

