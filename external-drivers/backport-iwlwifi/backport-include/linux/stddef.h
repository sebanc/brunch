#ifndef __BACKPORT_LINUX_STDDEF_H
#define __BACKPORT_LINUX_STDDEF_H
#include_next <linux/stddef.h>

#ifndef offsetofend
/**
 * offsetofend(TYPE, MEMBER)
 *
 * @TYPE: The type of the structure
 * @MEMBER: The member within the structure to get the end offset of
 */
#define offsetofend(TYPE, MEMBER) \
	(offsetof(TYPE, MEMBER)	+ sizeof(((TYPE *)0)->MEMBER))
#endif

#endif /* __BACKPORT_LINUX_STDDEF_H */
