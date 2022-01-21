#ifndef __BACKPORT_CFG80211_
#define __BACKPORT_CFG80211_

#if LINUX_VERSION_IS_LESS(5,11,0)
/* This is really really awkward but we need to include include/linux/rkfill.h
 * here only for backport purposes. There is content in
 * backport-include/linux/rfkill.h that should have gone
 * to uapi/linux/rfkill.h but we couldn't put it there because
 * include_next gets totally confused and can't include the right
 * file. So we had to put that content in linux/rfkill.h. In a real
 * kernel we wouldn't need to include this from cfg80211.h.
 */
#include <linux/rfkill.h>
#endif
#include_next <net/cfg80211.h>

#endif /* __BACKPORT_CFG80211 */
