#ifndef __BACKPORT_LINUX_FTRACE_EVENT_H
#define __BACKPORT_LINUX_FTRACE_EVENT_H
#include_next <linux/ftrace_event.h>

#if LINUX_VERSION_IS_LESS(4,0,0)
const char *ftrace_print_array_seq(struct trace_seq *p,
				   const void *buf, int buf_len,
				   size_t el_size);
#endif
#endif /* __BACKPORT_LINUX_FTRACE_EVENT_H */
