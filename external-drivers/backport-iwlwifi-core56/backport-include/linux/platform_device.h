#ifndef __BACKPORT_PLATFORM_DEVICE_H
#define __BACKPORT_PLATFORM_DEVICE_H

#include_next <linux/platform_device.h>
#include <linux/version.h>

#ifndef module_platform_driver_probe
#define module_platform_driver_probe(__platform_driver, __platform_probe) \
static int __init __platform_driver##_init(void) \
{ \
	return platform_driver_probe(&(__platform_driver), \
				     __platform_probe);    \
} \
module_init(__platform_driver##_init); \
static void __exit __platform_driver##_exit(void) \
{ \
	platform_driver_unregister(&(__platform_driver)); \
} \
module_exit(__platform_driver##_exit);
#endif

#ifndef PLATFORM_DEVID_NONE
#define PLATFORM_DEVID_NONE	(-1)
#endif

#ifndef PLATFORM_DEVID_AUTO
#define PLATFORM_DEVID_AUTO	(-1)
#endif

#ifndef module_platform_driver
#define module_platform_driver(__platform_driver) \
        module_driver(__platform_driver, platform_driver_register, \
                        platform_driver_unregister)
#endif

#if LINUX_VERSION_IS_LESS(5,1,0)
/**
 * devm_platform_ioremap_resource - call devm_ioremap_resource() for a platform
 *				    device
 *
 * @pdev: platform device to use both for memory resource lookup as well as
 *        resource management
 * @index: resource index
 */
#ifdef CONFIG_HAS_IOMEM
#define devm_platform_ioremap_resource LINUX_BACKPORT(devm_platform_ioremap_resource)
static inline void __iomem *devm_platform_ioremap_resource(struct platform_device *pdev,
					     unsigned int index)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	return devm_ioremap_resource(&pdev->dev, res);
}
#endif /* CONFIG_HAS_IOMEM */
#endif

#endif /* __BACKPORT_PLATFORM_DEVICE_H */
