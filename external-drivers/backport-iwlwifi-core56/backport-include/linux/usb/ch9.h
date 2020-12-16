#ifndef __BACKPORT__LINUX_USB_CH9_H
#define __BACKPORT__LINUX_USB_CH9_H

#include <linux/version.h>
#include_next <linux/usb/ch9.h>

#if LINUX_VERSION_IS_LESS(3,2,0)
#include <linux/types.h>    /* __u8 etc */
#include <asm/byteorder.h>  /* le16_to_cpu */

/**
 * usb_endpoint_maxp - get endpoint's max packet size
 * @epd: endpoint to be checked
 *
 * Returns @epd's max packet
 */
#define usb_endpoint_maxp LINUX_BACKPORT(usb_endpoint_maxp)
static inline int usb_endpoint_maxp(const struct usb_endpoint_descriptor *epd)
{
	return __le16_to_cpu(epd->wMaxPacketSize);
}
#endif /* < 3.2 */

#if LINUX_VERSION_IS_LESS(4,6,0)
#define USB_SPEED_SUPER_PLUS	6
#endif

#endif /* __BACKPORT__LINUX_USB_CH9_H */
