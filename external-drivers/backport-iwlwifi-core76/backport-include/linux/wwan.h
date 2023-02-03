#ifndef __BACKPORT_WWAN_H
#define __BACKPORT_WWAN_H
#include <linux/version.h>
#if LINUX_VERSION_IS_GEQ(5,13,0)
#include_next <linux/wwan.h>

#if LINUX_VERSION_IS_LESS(5,14,0)
#define WWAN_PORT_UNKNOWN (WWAN_PORT_MAX + 1)
#endif /* <= 5.14 */

#else

/**
 * enum wwan_port_type - WWAN port types
 * @WWAN_PORT_AT: AT commands
 * @WWAN_PORT_MBIM: Mobile Broadband Interface Model control
 * @WWAN_PORT_QMI: Qcom modem/MSM interface for modem control
 * @WWAN_PORT_QCDM: Qcom Modem diagnostic interface
 * @WWAN_PORT_FIREHOSE: XML based command protocol
 *
 * @WWAN_PORT_MAX: Highest supported port types
 * @WWAN_PORT_UNKNOWN: Special value to indicate an unknown port type
 * @__WWAN_PORT_MAX: Internal use
 */
enum wwan_port_type {
	WWAN_PORT_AT,
	WWAN_PORT_MBIM,
	WWAN_PORT_QMI,
	WWAN_PORT_QCDM,
	WWAN_PORT_FIREHOSE,

	/* Add new port types above this line */

	__WWAN_PORT_MAX,
	WWAN_PORT_MAX = __WWAN_PORT_MAX - 1,
	WWAN_PORT_UNKNOWN,
};

#endif /* >= 5.13 */
#endif /* __BACKPORT_WWAN_H */
