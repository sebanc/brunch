#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(6,0,0)
#undef VSTRING_MSG_MAX
#define VSTRING_MSG_MAX	512

#undef __vstring
#define __vstring(msg, fmt, va) __dynamic_array(char, msg, VSTRING_MSG_MAX)

#undef __assign_vstr
#define __assign_vstr(msg, fmt, va)					  \
	WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg), VSTRING_MSG_MAX, \
			       fmt, *va) >= VSTRING_MSG_MAX)
#endif /* <6.0 */

#include_next <trace/trace_events.h>
