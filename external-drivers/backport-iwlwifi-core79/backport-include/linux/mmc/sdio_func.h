#ifndef __BACKPORT_MMC_SDIO_FUNC_H
#define __BACKPORT_MMC_SDIO_FUNC_H
#include <linux/version.h>
#include_next <linux/mmc/sdio_func.h>

#ifndef dev_to_sdio_func
#define dev_to_sdio_func(d)	container_of(d, struct sdio_func, dev)
#endif

#if LINUX_VERSION_IS_LESS(5,2,0) && \
    !LINUX_VERSION_IN_RANGE(5,1,15, 5,2,0) && \
    !LINUX_VERSION_IN_RANGE(4,19,56, 4,20,0)
    
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

/**
 *	sdio_retune_hold_now - start deferring retuning requests till release
 *	@func: SDIO function attached to host
 *
 *	This function can be called if it's currently a bad time to do
 *	a retune of the SDIO card.  Retune requests made during this time
 *	will be held and we'll actually do the retune sometime after the
 *	release.
 *
 *	This function could be useful if an SDIO card is in a power state
 *	where it can respond to a small subset of commands that doesn't
 *	include the retuning command.  Care should be taken when using
 *	this function since (presumably) the retuning request we might be
 *	deferring was made for a good reason.
 *
 *	This function should be called while the host is claimed.
 */
#define sdio_retune_hold_now LINUX_BACKPORT(sdio_retune_hold_now)
static inline void sdio_retune_hold_now(struct sdio_func *func)
{
	struct mmc_host *host = func->card->host;

	host->retune_now = 0;
	host->hold_retune += 1;
}

/**
 *	sdio_retune_release - signal that it's OK to retune now
 *	@func: SDIO function attached to host
 *
 *	This is the complement to sdio_retune_hold_now().  Calling this
 *	function won't make a retune happen right away but will allow
 *	them to be scheduled normally.
 *
 *	This function should be called while the host is claimed.
 */
#define sdio_retune_release LINUX_BACKPORT(sdio_retune_release)
static inline void sdio_retune_release(struct sdio_func *func)
{
	struct mmc_host *host = func->card->host;

	if (host->hold_retune)
		host->hold_retune -= 1;
	else
		WARN_ON(1);
}

#define sdio_retune_crc_disable LINUX_BACKPORT(sdio_retune_crc_disable)
static inline void sdio_retune_crc_disable(struct sdio_func *func)
{
}
#define sdio_retune_crc_enable LINUX_BACKPORT(sdio_retune_crc_enable)
static inline void sdio_retune_crc_enable(struct sdio_func *func)
{
}
#endif /* < 5.2 */

#ifndef module_sdio_driver
/**
 * module_sdio_driver() - Helper macro for registering a SDIO driver
 * @__sdio_driver: sdio_driver struct
 *
 * Helper macro for SDIO drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_sdio_driver(__sdio_driver) \
	module_driver(__sdio_driver, sdio_register_driver, \
		      sdio_unregister_driver)
#endif

#endif /* __BACKPORT_MMC_SDIO_FUNC_H */
