/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright(c) 2017-2021 Intel Corporation */

#ifndef __GNA_IOCTL_H__
#define __GNA_IOCTL_H__

struct file;

long gna_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

#endif // __GNA_IOCTL_H__
