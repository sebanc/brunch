#ifndef __BACKPORT_LINUX_DELAY_H
#define __BACKPORT_LINUX_DELAY_H
#include_next <linux/delay.h>

#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,8,0)
#define fsleep LINUX_BACKPORT(fsleep)
/* see Documentation/timers/timers-howto.rst for the thresholds */
static inline void fsleep(unsigned long usecs)
{
	if (usecs <= 10)
		udelay(usecs);
	else if (usecs <= 20000)
		usleep_range(usecs, 2 * usecs);
	else
		msleep(DIV_ROUND_UP(usecs, 1000));
}
#endif /* < 5.8.0 */

#endif /* __BACKPORT_LINUX_DELAY_H */
