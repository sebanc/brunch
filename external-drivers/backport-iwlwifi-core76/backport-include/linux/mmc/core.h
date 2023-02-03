#ifndef __BACKPORT_LINUX_MMC_CORE_H
#define __BACKPORT_LINUX_MMC_CORE_H

#include_next <linux/mmc/card.h>
#include_next <linux/mmc/core.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,19,0)
static inline int backport_mmc_sw_reset(struct mmc_card *card)
{
	return mmc_sw_reset(card->host);
}
#define mmc_sw_reset LINUX_BACKPORT(mmc_sw_reset)

static inline int backport_mmc_hw_reset(struct mmc_card *card)
{
	return mmc_hw_reset(card->host);
}
#define mmc_hw_reset LINUX_BACKPORT(mmc_hw_reset)
#endif /* <5.19 */

#endif
