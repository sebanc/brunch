#ifndef __BACKPORT_OF_ADDRESS_H
#define __BACKPORT_OF_ADDRESS_H
#include_next <linux/of_address.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,4,0) && !defined(CONFIG_OF_ADDRESS)
#ifndef OF_BAD_ADDR
#define OF_BAD_ADDR     ((u64)-1)
#endif
#define of_translate_address LINUX_BACKPORT(of_translate_addres)
static inline u64 of_translate_address(struct device_node *np,
				       const __be32 *addr)
{
	return OF_BAD_ADDR;
}
#endif /* LINUX_VERSION_IS_LESS(4,4,0) */

#endif /* __BACKPORT_OF_IRQ_H */
