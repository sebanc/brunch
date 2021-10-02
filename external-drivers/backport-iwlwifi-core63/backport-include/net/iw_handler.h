#ifndef __BACKPORT_IW_HANDLER_H
#define __BACKPORT_IW_HANDLER_H
#include_next <net/iw_handler.h>

#if LINUX_VERSION_IS_LESS(4,1,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline char *
iwe_stream_add_event_check(struct iw_request_info *info, char *stream,
			   char *ends, struct iw_event *iwe, int event_len)
{
	char *res = iwe_stream_add_event(info, stream, ends, iwe, event_len);

	if (res == stream)
		return ERR_PTR(-E2BIG);
	return res;
}

static inline char *
iwe_stream_add_point_check(struct iw_request_info *info, char *stream,
			   char *ends, struct iw_event *iwe, char *extra)
{
	char *res = iwe_stream_add_point(info, stream, ends, iwe, extra);

	if (res == stream)
		return ERR_PTR(-E2BIG);
	return res;
}
#endif /* LINUX_VERSION_IS_LESS(4,1,0) */

/* this was added in v3.2.79, v3.18.30, v4.1.21, v4.4.6 and 4.5 */
#if !(LINUX_VERSION_IS_GEQ(4,4,6) || \
      (LINUX_VERSION_IS_GEQ(4,1,21) && \
       LINUX_VERSION_IS_LESS(4,2,0)) || \
      (LINUX_VERSION_IS_GEQ(3,18,30) && \
       LINUX_VERSION_IS_LESS(3,19,0)) || \
      (LINUX_VERSION_IS_GEQ(3,2,79) && \
       LINUX_VERSION_IS_LESS(3,3,0)))
#define wireless_nlevent_flush LINUX_BACKPORT(wireless_nlevent_flush)
static inline void wireless_nlevent_flush(void) {}
#endif
#endif /* __BACKPORT_IW_HANDLER_H */
