#ifndef __BACKPORT_IDR_H
#define __BACKPORT_IDR_H
#include_next <linux/idr.h>
#include <linux/version.h>


#if LINUX_VERSION_IS_LESS(4, 11, 0)
static inline void *backport_idr_remove(struct idr *idr, int id)
{
	void *item = idr_find(idr, id);
	idr_remove(idr, id);
	return item;
}
#define idr_remove LINUX_BACKPORT(idr_remove)
#endif

#endif /* __BACKPORT_IDR_H */
