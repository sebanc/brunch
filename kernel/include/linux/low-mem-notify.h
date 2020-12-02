#ifndef _LINUX_LOW_MEM_NOTIFY_H
#define _LINUX_LOW_MEM_NOTIFY_H

#include <linux/types.h>

#ifdef CONFIG_LOW_MEM_NOTIFY
extern const struct file_operations low_mem_notify_fops;

void low_mem_notify(void);
bool low_mem_check(void);
#else
static inline void low_mem_notify(void)
{
}

static inline bool low_mem_check(void)
{
	return false;
}
#endif

#endif
