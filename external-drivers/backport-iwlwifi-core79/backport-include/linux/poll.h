#ifndef __BACKPORT_LINUX_POLL_H
#define __BACKPORT_LINUX_POLL_H
#include_next <linux/poll.h>
/* This import is needed for <= 4.15 */
#include <uapi/linux/eventpoll.h>

#endif /* __BACKPORT_LINUX_POLL_H */
