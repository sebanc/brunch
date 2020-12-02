#ifndef _BACKPORT_TIMER_H
#define _BACKPORT_TIMER_H

#include_next <linux/timer.h>

#ifndef setup_deferrable_timer
/*
 * The TIMER_DEFERRABLE flag has not been around since 3.0 so
 * two different backports are needed here.
 */
#ifdef TIMER_DEFERRABLE
#define setup_deferrable_timer(timer, fn, data)                         \
        __setup_timer((timer), (fn), (data), TIMER_DEFERRABLE)
#else
static inline void setup_deferrable_timer_key(struct timer_list *timer,
					      const char *name,
					      struct lock_class_key *key,
					      void (*func)(unsigned long),
					      unsigned long data)
{
	timer->function = func;
	timer->data = data;
	init_timer_deferrable_key(timer, name, key);
}
#define setup_deferrable_timer(timer, fn, data)				\
	do {								\
		static struct lock_class_key __key;			\
		setup_deferrable_timer_key((timer), #timer, &__key,	\
					   (fn), (data));		\
	} while (0)
#endif
#endif

#ifndef TIMER_DEFERRABLE
#define TIMER_DEFERRABLE	1
#endif

#ifndef from_timer
#define TIMER_DATA_TYPE          unsigned long
#define TIMER_FUNC_TYPE          void (*)(TIMER_DATA_TYPE)

static inline void timer_setup(struct timer_list *timer,
			       void (*callback) (struct timer_list *),
			       unsigned int flags)
{
#ifdef __setup_timer
	__setup_timer(timer, (TIMER_FUNC_TYPE) callback,
		      (TIMER_DATA_TYPE) timer, flags);
#else
	if (flags & TIMER_DEFERRABLE)
		setup_deferrable_timer(timer, (TIMER_FUNC_TYPE) callback,
				       (TIMER_DATA_TYPE) timer);
	else
		setup_timer(timer, (TIMER_FUNC_TYPE) callback,
			    (TIMER_DATA_TYPE) timer);
#endif
}

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)
#endif

#if LINUX_VERSION_IS_LESS(4,15,0)
#undef DEFINE_TIMER
#define DEFINE_TIMER(_name, _function)				\
	struct timer_list _name =				\
		__TIMER_INITIALIZER(_function, 0, 0, 0)
#endif

#endif /* _BACKPORT_TIMER_H */
