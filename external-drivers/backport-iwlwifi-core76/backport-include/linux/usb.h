#ifndef __BACKPORT_LINUX_USB_H
#define __BACKPORT_LINUX_USB_H
#include_next <linux/usb.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,19,0)
static inline u16 backport_usb_maxpacket(struct usb_device *udev, int pipe)
{
	return usb_maxpacket(udev, pipe, usb_pipeout(pipe));
}
#define usb_maxpacket LINUX_BACKPORT(usb_maxpacket)
#endif /* <5.19 */

#endif
