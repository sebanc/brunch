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

#if LINUX_VERSION_IS_LESS(5,7,0)
#ifdef CONFIG_HAS_IOMEM
#define devm_platform_get_and_ioremap_resource LINUX_BACKPORT(devm_platform_get_and_ioremap_resource)
/**
 * devm_platform_get_and_ioremap_resource - call devm_ioremap_resource() for a
 *					    platform device and get resource
 *
 * @pdev: platform device to use both for memory resource lookup as well as
 *        resource management
 * @index: resource index
 * @res: optional output parameter to store a pointer to the obtained resource.
 *
 * Return: a pointer to the remapped memory or an ERR_PTR() encoded error code
 * on failure.
 */
static inline void __iomem *
devm_platform_get_and_ioremap_resource(struct platform_device *pdev,
				unsigned int index, struct resource **res)
{
	struct resource *r;

	r = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (res)
		*res = r;
	return devm_ioremap_resource(&pdev->dev, r);
}
#endif /* CONFIG_HAS_IOMEM */
#endif /* < 5.7 */

#if LINUX_VERSION_IS_LESS(5,1,0)

#ifdef CONFIG_HAS_IOMEM
#define devm_platform_ioremap_resource LINUX_BACKPORT(devm_platform_ioremap_resource)
/**
 * devm_platform_ioremap_resource - call devm_ioremap_resource() for a platform
 *				    device
 *
 * @pdev: platform device to use both for memory resource lookup as well as
 *        resource management
 * @index: resource index
 *
 * Return: a pointer to the remapped memory or an ERR_PTR() encoded error code
 * on failure.
 */
static inline void __iomem *
devm_platform_ioremap_resource(struct platform_device *pdev,
					     unsigned int index)
{
	return devm_platform_get_and_ioremap_resource(pdev, index, NULL);
}
#endif /* CONFIG_HAS_IOMEM */
#endif

#endif /* __BACKPORT_PLATFORM_DEVICE_H */
