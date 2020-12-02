#ifndef __BACKPORTS_BCM47XX_NVRAM_H
#define __BACKPORTS_BCM47XX_NVRAM_H
#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(4,1,0)
#include_next <linux/bcm47xx_nvram.h>
#else
#include <linux/types.h>
#include <linux/kernel.h>
#endif

#if LINUX_VERSION_IS_LESS(4,2,0)
#define bcm47xx_nvram_get_contents LINUX_BACKPORT(bcm47xx_nvram_get_contents)
static inline char *bcm47xx_nvram_get_contents(size_t *val_len)
{
	return NULL;
}

#define bcm47xx_nvram_release_contents LINUX_BACKPORT(bcm47xx_nvram_release_contents)
static inline void bcm47xx_nvram_release_contents(char *nvram)
{
}
#endif /* LINUX_VERSION_IS_GEQ(4,1,0) */

#endif /* __BACKPORTS_BCM47XX_NVRAM_H */
