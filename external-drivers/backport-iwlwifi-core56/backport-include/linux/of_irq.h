#ifndef __BACKPORT_OF_IRQ_H
#define __BACKPORT_OF_IRQ_H
#include_next <linux/of_irq.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,5,0) && !defined(CONFIG_OF)
#define irq_of_parse_and_map LINUX_BACKPORT(irq_of_parse_and_map)
static inline unsigned int irq_of_parse_and_map(struct device_node *dev,
						int index)
{
	return 0;
}
#endif /* LINUX_VERSION_IS_LESS(4,5,0) */

#endif /* __BACKPORT_OF_IRQ_H */
