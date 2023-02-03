// SPDX-License-Identifier: GPL-2.0

#include <linux/compat.h>
#include <linux/export.h>
#include <linux/uaccess.h>
#include <linux/netdevice.h>

#include <uapi/linux/if.h>

#if LINUX_VERSION_IS_GEQ(4,6,0)

int get_user_ifreq(struct ifreq *ifr, void __user **ifrdata, void __user *arg)
{
#ifdef CONFIG_COMPAT
	if (in_compat_syscall()) {
		struct compat_ifreq *ifr32 = (struct compat_ifreq *)ifr;

		memset(ifr, 0, sizeof(*ifr));
		if (copy_from_user(ifr32, arg, sizeof(*ifr32)))
			return -EFAULT;

		if (ifrdata)
			*ifrdata = compat_ptr(ifr32->ifr_data);

		return 0;
	}
#endif

	if (copy_from_user(ifr, arg, sizeof(*ifr)))
		return -EFAULT;

	if (ifrdata)
		*ifrdata = ifr->ifr_data;

	return 0;
}
EXPORT_SYMBOL(get_user_ifreq);

int put_user_ifreq(struct ifreq *ifr, void __user *arg)
{
	size_t size = sizeof(*ifr);

#ifdef CONFIG_COMPAT
	if (in_compat_syscall())
		size = sizeof(struct compat_ifreq);
#endif

	if (copy_to_user(arg, ifr, size))
		return -EFAULT;

	return 0;
}
EXPORT_SYMBOL(put_user_ifreq);

#endif /* >= 4.6.0 */
