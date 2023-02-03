#ifndef __BACKPORT_IW_HANDLER_H
#define __BACKPORT_IW_HANDLER_H
#include_next <net/iw_handler.h>


/* this was added in v3.2.79, v3.18.30, v4.1.21, v4.4.6 and 4.5 */
#if !LINUX_VERSION_IS_GEQ(4,4,6)
#define wireless_nlevent_flush LINUX_BACKPORT(wireless_nlevent_flush)
static inline void wireless_nlevent_flush(void) {}
#endif
#endif /* __BACKPORT_IW_HANDLER_H */
