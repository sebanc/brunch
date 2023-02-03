#ifndef __BACKPORT_LINUX_KCONFIG_H
#define __BACKPORT_LINUX_KCONFIG_H
#include <linux/version.h>
#include_next <linux/kconfig.h>

#ifndef IS_REACHABLE
/*
 * IS_REACHABLE(CONFIG_FOO) evaluates to 1 if the currently compiled
 * code can call a function defined in code compiled based on CONFIG_FOO.
 * This is similar to IS_ENABLED(), but returns false when invoked from
 * built-in code when CONFIG_FOO is set to 'm'.
 */
#define IS_REACHABLE(option) (config_enabled(option) || \
		 (config_enabled(option##_MODULE) && config_enabled(MODULE)))
#endif

#endif
