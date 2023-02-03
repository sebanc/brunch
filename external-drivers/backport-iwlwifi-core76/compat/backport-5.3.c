// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>

/**
 * gpiod_configure_flags - helper function to configure a given GPIO
 * @desc:	gpio whose value will be assigned
 * @con_id:	function within the GPIO consumer
 * @lflags:	bitmask of gpio_lookup_flags GPIO_* values - returned from
 *		of_find_gpio() or of_get_gpio_hog()
 * @dflags:	gpiod_flags - optional GPIO initialization flags
 *
 * Return 0 on success, -ENOENT if no GPIO has been assigned to the
 * requested function and/or index, or another IS_ERR() code if an error
 * occurred while trying to acquire the GPIO.
 */
static int gpiod_configure_flags(struct gpio_desc *desc, const char *con_id,
		unsigned long lflags, enum gpiod_flags dflags)
{
	int ret;

#ifdef GPIO_TRANSITORY
	ret = gpiod_set_transitory(desc, (lflags & GPIO_TRANSITORY));
	if (ret < 0)
		return ret;
#endif

	/* No particular flag request, return here... */
	if (!(dflags & GPIOD_FLAGS_BIT_DIR_SET))
		return 0;

	/* Process flags */
	if (dflags & GPIOD_FLAGS_BIT_DIR_OUT)
		ret = gpiod_direction_output(desc,
				!!(dflags & GPIOD_FLAGS_BIT_DIR_VAL));
	else
		ret = gpiod_direction_input(desc);

	return ret;
}

#undef gpiochip_request_own_desc

/* In kernels older than 5.4, these function declarations are inside a
 * CONFIG_GPIOLIB ifdef, so we need to declare them here.
 */
#ifndef CONFIG_GPIOLIB
extern struct gpio_desc *gpiochip_request_own_desc(struct gpio_chip *chip,
						   u16 hwnum,
						   const char *label);
void gpiochip_free_own_desc(struct gpio_desc *desc);
#endif /* CONFIG_GPIOLIB */

struct gpio_desc *backport_gpiochip_request_own_desc(struct gpio_chip *gc,
					    unsigned int hwnum,
					    const char *label,
					    enum gpio_lookup_flags lflags,
					    enum gpiod_flags dflags)
{
	struct gpio_desc *desc;
	int ret;

#if LINUX_VERSION_IS_GEQ(5,0,0)
	desc = gpiochip_request_own_desc(gc, hwnum, label, dflags);
#else
	desc = gpiochip_request_own_desc(gc, hwnum, label);
#endif
	if (IS_ERR(desc))
		return desc;

	ret = gpiod_configure_flags(desc, label, lflags, dflags);
	if (ret) {
		gpiochip_free_own_desc(desc);
		return ERR_PTR(ret);
	}

	return desc;
}
EXPORT_SYMBOL_GPL(backport_gpiochip_request_own_desc);
