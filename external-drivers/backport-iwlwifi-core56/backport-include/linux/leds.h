#ifndef __BACKPORT_LINUX_LEDS_H
#define __BACKPORT_LINUX_LEDS_H
#include_next <linux/leds.h>
#include <linux/version.h>

#include <backport/leds-disabled.h>

#ifndef CPTCFG_BPAUTO_BUILD_LEDS
#if LINUX_VERSION_IS_LESS(3,6,0)
/*
 * Backports
 *
 * commit 959d62fa865d2e616b61a509e1cc5b88741f065e
 * Author: Shuah Khan <shuahkhan@gmail.com>
 * Date:   Thu Jun 14 04:34:30 2012 +0800
 *
 *   leds: Rename led_brightness_set() to led_set_brightness()
 *
 *   Rename leds external interface led_brightness_set() to led_set_brightness().
 *   This is the second phase of the change to reduce confusion between the
 *   leds internal and external interfaces that set brightness. With this change,
 *   now the external interface is led_set_brightness(). The first phase renamed
 *   the internal interface led_set_brightness() to __led_set_brightness().
 *   There are no changes to the interface implementations.
 *
 *   Signed-off-by: Shuah Khan <shuahkhan@gmail.com>
 *   Signed-off-by: Bryan Wu <bryan.wu@canonical.com>
 */
#define led_set_brightness(_dev, _switch) led_brightness_set(_dev, _switch)
#endif /* LINUX_VERSION_IS_LESS(3,6,0) */
#endif /* CPTCFG_BPAUTO_BUILD_LEDS */

#if LINUX_VERSION_IS_LESS(4,2,0)
/*
 * There is no LINUX_BACKPORT() guard here because we want it to point to
 * the original function which is exported normally.
 */
#ifdef CONFIG_LEDS_TRIGGERS
extern void led_trigger_remove(struct led_classdev *led_cdev);
#else
static inline void led_trigger_remove(struct led_classdev *led_cdev) {}
#endif
#endif

#if LINUX_VERSION_IS_LESS(4,5,0) && \
    LINUX_VERSION_IS_GEQ(3,19,0)
#define led_set_brightness_sync LINUX_BACKPORT(led_set_brightness_sync)
/**
 * led_set_brightness_sync - set LED brightness synchronously
 * @led_cdev: the LED to set
 * @brightness: the brightness to set it to
 *
 * Set an LED's brightness immediately. This function will block
 * the caller for the time required for accessing device registers,
 * and it can sleep.
 *
 * Returns: 0 on success or negative error value on failure
 */
extern int led_set_brightness_sync(struct led_classdev *led_cdev,
				   enum led_brightness value);
#endif /* < 4.5 && >= 3.19 */

#if LINUX_VERSION_IS_LESS(4,5,0)
#define devm_led_trigger_register LINUX_BACKPORT(devm_led_trigger_register)
extern int devm_led_trigger_register(struct device *dev,
				     struct led_trigger *trigger);
#endif /* < 4.5 */

#endif /* __BACKPORT_LINUX_LEDS_H */
