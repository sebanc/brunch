#ifndef __BACKPORT_MOD_DEVICETABLE_H
#define __BACKPORT_MOD_DEVICETABLE_H
#include_next <linux/mod_devicetable.h>

#ifndef HID_BUS_ANY
#define HID_BUS_ANY                            0xffff
#endif

#ifndef HID_GROUP_ANY
#define HID_GROUP_ANY                          0x0000
#endif

#ifndef HID_ANY_ID
#define HID_ANY_ID                             (~0)
#endif

#if LINUX_VERSION_IS_LESS(5,7,0)
#define MHI_DEVICE_MODALIAS_FMT "mhi:%s"
#define MHI_NAME_SIZE 32

/**
 * struct mhi_device_id - MHI device identification
 * @chan: MHI channel name
 * @driver_data: driver data;
 */
struct mhi_device_id {
	const char chan[MHI_NAME_SIZE];
	kernel_ulong_t driver_data;
};
#endif

#if LINUX_VERSION_IS_LESS(4,17,0)
#define DMI_OEM_STRING (DMI_STRING_MAX + 1)
#endif /* < 4.17.0 */

#endif /* __BACKPORT_MOD_DEVICETABLE_H */
