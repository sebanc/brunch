/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 */

#ifndef UFSHCD_PLTFRM_H_
#define UFSHCD_PLTFRM_H_

#include "ufshcd.h"

int ufshcd_pltfrm_init(struct platform_device *pdev,
		       struct ufs_hba_variant_ops *vops);
void ufshcd_pltfrm_shutdown(struct platform_device *pdev);

#ifdef CONFIG_PM

int ufshcd_pltfrm_suspend(struct device *dev);
int ufshcd_pltfrm_resume(struct device *dev);
int ufshcd_pltfrm_runtime_suspend(struct device *dev);
int ufshcd_pltfrm_runtime_resume(struct device *dev);
int ufshcd_pltfrm_runtime_idle(struct device *dev);

#else /* !CONFIG_PM */

#define ufshcd_pltfrm_suspend	NULL
#define ufshcd_pltfrm_resume	NULL
#define ufshcd_pltfrm_runtime_suspend	NULL
#define ufshcd_pltfrm_runtime_resume	NULL
#define ufshcd_pltfrm_runtime_idle	NULL

#endif /* CONFIG_PM */

#endif /* UFSHCD_PLTFRM_H_ */
