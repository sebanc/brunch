/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef __MTK_SVS_H__
#define __MTK_SVS_H__

#if defined(CONFIG_MTK_SVS)
unsigned long claim_mtk_svs_lock(void);
void release_mtk_svs_lock(unsigned long flags);
#else
static inline unsigned long claim_mtk_svs_lock(void)
{
	return 0;
}

static inline void release_mtk_svs_lock(unsigned long flags)
{
}
#endif /* CONFIG_MTK_SVS */

#endif /* __MTK_SVS_H__ */
