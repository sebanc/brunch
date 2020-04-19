#ifndef __BACKPORT_LINUX_PNP_H
#define __BACKPORT_LINUX_PNP_H
#include_next <linux/pnp.h>

#ifndef module_pnp_driver
/**
 * module_pnp_driver() - Helper macro for registering a PnP driver
 * @__pnp_driver: pnp_driver struct
 *
 * Helper macro for PnP drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_pnp_driver(__pnp_driver) \
	module_driver(__pnp_driver, pnp_register_driver, \
				    pnp_unregister_driver)
#endif

#endif /* __BACKPORT_LINUX_PNP_H */
