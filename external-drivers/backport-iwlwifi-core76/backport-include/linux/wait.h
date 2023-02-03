#ifndef __BACKPORT_LINUX_WAIT_H
#define __BACKPORT_LINUX_WAIT_H
#include_next <linux/wait.h>


#if LINUX_VERSION_IS_LESS(4,13,0)
#define wait_queue_entry_t wait_queue_t

#define wait_event_killable_timeout(wq_head, condition, timeout)	\
({									\
	long __ret = timeout;						\
	might_sleep();							\
	if (!___wait_cond_timeout(condition))				\
		__ret = __wait_event_killable_timeout(wq_head,		\
						condition, timeout);	\
	__ret;								\
})

#define __wait_event_killable_timeout(wq_head, condition, timeout)	\
	___wait_event(wq_head, ___wait_cond_timeout(condition),		\
		      TASK_KILLABLE, 0, timeout,			\
		      __ret = schedule_timeout(__ret))
#endif

#endif /* __BACKPORT_LINUX_WAIT_H */
