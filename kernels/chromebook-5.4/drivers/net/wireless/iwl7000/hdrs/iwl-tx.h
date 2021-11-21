#ifndef __IWL_CHROME_TX_H
#define __IWL_CHROME_TX_H
/* This file is pre-included from the Makefile (cc command line)
 *
 * ChromeOS backport file-specific definitions
 * Copyright (C) 2018      Intel Corporation
 */

/* This file includes our version of the headers that otherwise would
 * be taken from the kernel's headers, but that can only be included
 * in a C-file that actually uses them.
 */

#include "hdrs/net/codel.h"
#include "hdrs/net/codel_impl.h"
#include "hdrs/net/fq.h"
#include "hdrs/net/fq_impl.h"

#endif /* __IWL_CHROME_TX_H */
