#ifndef __BACKPORT_LINUX_KTHREAD_H
#define __BACKPORT_LINUX_KTHREAD_H
#include_next <linux/kthread.h>

#if LINUX_VERSION_IS_LESS(5,17,0)
static inline void __noreturn
kthread_complete_and_exit(struct completion *c, long ret)
{
	complete_and_exit(c, ret);
}
#endif /* < 5.17 */

#endif /* __BACKPORT_LINUX_KTHREAD_H */
