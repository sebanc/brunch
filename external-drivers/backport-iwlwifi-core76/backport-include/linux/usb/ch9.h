#ifndef __BACKPORT__LINUX_USB_CH9_H
#define __BACKPORT__LINUX_USB_CH9_H

#include <linux/version.h>
#include_next <linux/usb/ch9.h>


#if LINUX_VERSION_IS_LESS(4,6,0)
#define USB_SPEED_SUPER_PLUS	6
#endif

#endif /* __BACKPORT__LINUX_USB_CH9_H */
