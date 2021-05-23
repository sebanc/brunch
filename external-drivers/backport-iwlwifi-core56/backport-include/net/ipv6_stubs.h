/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _BACKPORT_IPV6_STUBS_H
#define _BACKPORT_IPV6_STUBS_H

#if LINUX_VERSION_IS_LESS(5,2,0)

#include <net/addrconf.h>

#else
#include_next <net/ipv6_stubs.h>
#endif

#endif	/* _BACKPORT_IPV6_STUBS_H */
