/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Intel Corporation
 */
#ifndef __BACKPORT_LINUX_EFI_H
#define __BACKPORT_LINUX_EFI_H
#include_next <linux/efi.h>

#ifndef EFI_RT_SUPPORTED_GET_VARIABLE
#define efi_rt_services_supported(...) efi_enabled(EFI_RUNTIME_SERVICES)
#endif

#endif /* __BACKPORT_LINUX_EFI_H */
