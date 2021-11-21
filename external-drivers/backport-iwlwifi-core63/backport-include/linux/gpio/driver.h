#ifndef __BP_GPIO_DRIVER_H
#define __BP_GPIO_DRIVER_H
#include <linux/version.h>
#if LINUX_VERSION_IS_LESS(3,13,0)
#include <asm-generic/gpio.h>
#else
#include_next <linux/gpio/driver.h>
#endif

#if LINUX_VERSION_IN_RANGE(3,17,0, 5,3,0)
enum gpiod_flags;
enum gpio_lookup_flags;

#define gpiochip_request_own_desc LINUX_BACKPORT(gpiochip_request_own_desc)
struct gpio_desc *backport_gpiochip_request_own_desc(struct gpio_chip *gc,
					    unsigned int hwnum,
					    const char *label,
					    enum gpio_lookup_flags lflags,
					    enum gpiod_flags dflags);
#endif /* 3.17.0 <= x < 5.3.0 */

#endif /* __BP_GPIO_DRIVER_H */
