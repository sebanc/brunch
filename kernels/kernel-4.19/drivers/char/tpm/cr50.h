/*
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * This file contains common code for devices with Cr50 firmware.
 */

#ifndef __CR50_H__
#define __CR50_H__

#include "tpm.h"

#ifdef CONFIG_PM_SLEEP
/* Handle suspend/resume. */
int cr50_resume(struct device *dev);
int cr50_suspend(struct device *dev);
#endif /* CONFIG_PM_SLEEP */

#endif /* __CR50_H__ */
