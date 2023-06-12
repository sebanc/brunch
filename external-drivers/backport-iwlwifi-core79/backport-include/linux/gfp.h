#ifndef __BACKPORT_LINUX_GFP_H
#define __BACKPORT_LINUX_GFP_H
#include_next <linux/gfp.h>

#ifndef ___GFP_KSWAPD_RECLAIM
#define ___GFP_KSWAPD_RECLAIM	0x0u
#endif

#ifndef __GFP_KSWAPD_RECLAIM
#define __GFP_KSWAPD_RECLAIM	((__force gfp_t)___GFP_KSWAPD_RECLAIM) /* kswapd can wake */
#endif

#if LINUX_VERSION_IS_LESS(4,10,0)
#define page_frag_alloc LINUX_BACKPORT(page_frag_alloc)
static inline void *page_frag_alloc(struct page_frag_cache *nc,
				    unsigned int fragsz, gfp_t gfp_mask)
{
	return __alloc_page_frag(nc, fragsz, gfp_mask);
}

#define __page_frag_cache_drain LINUX_BACKPORT(__page_frag_cache_drain)
void __page_frag_cache_drain(struct page *page, unsigned int count);
#endif /* < 4.10*/

#endif /* __BACKPORT_LINUX_GFP_H */
