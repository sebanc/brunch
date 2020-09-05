#ifndef __BP_PAGE_REF_H
#define __BP_PAGE_REF_H
#include <linux/version.h>
#if LINUX_VERSION_IS_GEQ(4,6,0) || \
	RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
#include_next <linux/page_ref.h>
#else
static inline void page_ref_inc(struct page *page)
{
	atomic_inc(&page->_count);
}

static inline int page_ref_count(struct page *page)
{
	return atomic_read(&page->_count);
}

static inline int page_ref_sub_and_test(struct page *page, int nr)
{
	return atomic_sub_and_test(nr, &page->_count);
}
#endif

#endif /* __BP_PAGE_REF_H */
