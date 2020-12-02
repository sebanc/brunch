#ifndef __BP_GPIO_DRIVER_H
#define __BP_GPIO_DRIVER_H
#include <linux/version.h>
#if LINUX_VERSION_IS_LESS(3,13,0)
#include <asm-generic/gpio.h>
#else
#include_next <linux/gpio/driver.h>
#endif

#endif /* __BP_GPIO_DRIVER_H */
