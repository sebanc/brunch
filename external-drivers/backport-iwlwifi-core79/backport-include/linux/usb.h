#ifndef __BACKPORT_LINUX_USB_H
#define __BACKPORT_LINUX_USB_H
#include_next <linux/usb.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,12,0)
#define usb_find_common_endpoints LINUX_BACKPORT(usb_find_common_endpoints)
int __must_check
usb_find_common_endpoints(struct usb_host_interface *alt,
		struct usb_endpoint_descriptor **bulk_in,
		struct usb_endpoint_descriptor **bulk_out,
		struct usb_endpoint_descriptor **int_in,
		struct usb_endpoint_descriptor **int_out);
#endif /* < 4.12 */

#if LINUX_VERSION_IS_LESS(5,19,0)
static inline u16 backport_usb_maxpacket(struct usb_device *udev, int pipe)
{
	return usb_maxpacket(udev, pipe, usb_pipeout(pipe));
}
#define usb_maxpacket LINUX_BACKPORT(usb_maxpacket)
#endif /* <5.19 */

#endif /* __BACKPORT_LINUX_USB_H */
