#ifndef __BACKPORT_SCATTERLIST_H
#define __BACKPORT_SCATTERLIST_H
#include_next <linux/scatterlist.h>

#if LINUX_VERSION_IS_LESS(4, 17, 0)

#define sg_init_marker LINUX_BACKPORT(sg_init_marker)
/**
 * sg_init_marker - Initialize markers in sg table
 * @sgl:	   The SG table
 * @nents:	   Number of entries in table
 *
 **/
static inline void sg_init_marker(struct scatterlist *sgl,
				  unsigned int nents)
{
#ifdef CONFIG_DEBUG_SG
	unsigned int i;

	for (i = 0; i < nents; i++)
		sgl[i].sg_magic = SG_MAGIC;
#endif
	sg_mark_end(&sgl[nents - 1]);
}

#endif /* LINUX_VERSION_IS_LESS(4, 17, 0) */

#endif /* __BACKPORT_SCATTERLIST_H */
