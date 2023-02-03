#ifndef __BACKPORT_LINUX_DEVCOREDUMP_H
#define __BACKPORT_LINUX_DEVCOREDUMP_H
#include_next <linux/devcoredump.h>
#include <linux/version.h>
#include <linux/scatterlist.h>

/* We only need to add our wrapper inside the range from 3.18 until
 * 4.6, outside that we can let our BPAUTO mechanism handle it.
 */
#if LINUX_VERSION_IS_LESS(4,7,0)
static inline
void backport_dev_coredumpm(struct device *dev, struct module *owner,
			    void *data, size_t datalen, gfp_t gfp,
			    ssize_t (*read_fn)(char *buffer, loff_t offset,
					    size_t count, void *data,
					    size_t datalen),
			    void (*free_fn)(void *data))
{
	dev_coredumpm(dev, owner, (const void *)data, datalen, gfp,
		      (void *)read_fn, (void *)free_fn);
}
#define dev_coredumpm LINUX_BACKPORT(dev_coredumpm)

#define dev_coredumpsg LINUX_BACKPORT(dev_coredumpsg)
void dev_coredumpsg(struct device *dev, struct scatterlist *table,
		    size_t datalen, gfp_t gfp);

#endif /* LINUX_VERSION_IS_LESS(4,7,0) */

#endif /* __BACKPORT_LINUX_DEVCOREDUMP_H */
