#ifndef __BACKPORT_LINUX_EVENTPOLL_H
#define __BACKPORT_LINUX_EVENTPOLL_H
#include_next <uapi/linux/eventpoll.h>

#ifndef EPOLLIN
#define EPOLLIN		0x00000001
#endif

#ifndef EPOLLPRI
#define EPOLLPRI	0x00000002
#endif

#ifndef EPOLLOUT
#define EPOLLOUT	0x00000004
#endif

#ifndef EPOLLERR
#define EPOLLERR	0x00000008
#endif

#ifndef EPOLLHUP
#define EPOLLHUP	0x00000010
#endif

#ifndef EPOLLRDNORM
#define EPOLLRDNORM	0x00000040
#endif

#ifndef EPOLLRDBAND
#define EPOLLRDBAND	0x00000080
#endif

#ifndef EPOLLWRNORM
#define EPOLLWRNORM	0x00000100
#endif

#ifndef EPOLLWRBAND
#define EPOLLWRBAND	0x00000200
#endif

#ifndef EPOLLMSG
#define EPOLLMSG	0x00000400
#endif

#ifndef EPOLLRDHUP
#define EPOLLRDHUP	0x00002000
#endif

#endif /* __BACKPORT_LINUX_EVENTPOLL_H */
