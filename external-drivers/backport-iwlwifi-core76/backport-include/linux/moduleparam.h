#ifndef __BACKPORT_LINUX_MODULEPARAM_H
#define __BACKPORT_LINUX_MODULEPARAM_H
#include_next <linux/moduleparam.h>

#ifndef module_param_hw_array
#define module_param_hw_array(name, type, hwtype, nump, perm) \
	module_param_array(name, type, nump, perm)
#endif

#endif /* __BACKPORT_LINUX_MODULEPARAM_H */
