#ifndef __BACKPORT_SCATTERLIST_H
#define __BACKPORT_SCATTERLIST_H
#include_next <linux/scatterlist.h>

#if LINUX_VERSION_IS_LESS(3,7,0)
int sg_nents(struct scatterlist *sg);
#endif

#if LINUX_VERSION_IS_LESS(3, 9, 0)

/*
 * sg page iterator
 *
 * Iterates over sg entries page-by-page.  On each successful iteration,
 * @piter->page points to the current page, @piter->sg to the sg holding this
 * page and @piter->sg_pgoffset to the page's page offset within the sg. The
 * iteration will stop either when a maximum number of sg entries was reached
 * or a terminating sg (sg_last(sg) == true) was reached.
 */
struct sg_page_iter {
	struct page             *page;          /* current page */
	struct scatterlist      *sg;            /* sg holding the page */
	unsigned int            sg_pgoffset;    /* page offset within the sg */

	/* these are internal states, keep away */
	unsigned int            __nents;        /* remaining sg entries */
	int                     __pg_advance;   /* nr pages to advance at the
						 * next step */
};

struct backport_sg_mapping_iter {
	/* the following three fields can be accessed directly */
	struct page		*page;		/* currently mapped page */
	void			*addr;		/* pointer to the mapped area */
	size_t			length;		/* length of the mapped area */
	size_t			consumed;	/* number of consumed bytes */
	struct sg_page_iter	piter;		/* page iterator */

	/* these are internal states, keep away */
	unsigned int		__offset;	/* offset within page */
	unsigned int		__remaining;	/* remaining bytes on page */
	unsigned int		__flags;
};
#define sg_mapping_iter LINUX_BACKPORT(sg_mapping_iter)

/**
 * sg_page_iter_page - get the current page held by the page iterator
 * @piter:	page iterator holding the page
 */
static inline struct page *sg_page_iter_page(struct sg_page_iter *piter)
{
	return nth_page(sg_page(piter->sg), piter->sg_pgoffset);
}

bool __sg_page_iter_next(struct sg_page_iter *piter);
void __sg_page_iter_start(struct sg_page_iter *piter,
			  struct scatterlist *sglist, unsigned int nents,
			  unsigned long pgoffset);

void backport_sg_miter_start(struct sg_mapping_iter *miter, struct scatterlist *sgl,
		    unsigned int nents, unsigned int flags);
bool backport_sg_miter_next(struct sg_mapping_iter *miter);
void backport_sg_miter_stop(struct sg_mapping_iter *miter);
#define sg_miter_start LINUX_BACKPORT(sg_miter_start)
#define sg_miter_next LINUX_BACKPORT(sg_miter_next)
#define sg_miter_stop LINUX_BACKPORT(sg_miter_stop)

/**
 * for_each_sg_page - iterate over the pages of the given sg list
 * @sglist:    sglist to iterate over
 * @piter:     page iterator to hold current page, sg, sg_pgoffset
 * @nents:     maximum number of sg entries to iterate over
 * @pgoffset:  starting page offset
 */
#define for_each_sg_page(sglist, piter, nents, pgoffset)		\
	for (__sg_page_iter_start((piter), (sglist), (nents), (pgoffset)); \
	     __sg_page_iter_next(piter);)

#endif /* LINUX_VERSION_IS_LESS(3, 9, 0) */

#if LINUX_VERSION_IS_LESS(3, 11, 0)
size_t sg_copy_buffer(struct scatterlist *sgl, unsigned int nents, void *buf,
		      size_t buflen, off_t skip, bool to_buffer);

#define sg_pcopy_to_buffer LINUX_BACKPORT(sg_pcopy_to_buffer)

static inline
size_t sg_pcopy_to_buffer(struct scatterlist *sgl, unsigned int nents,
			  void *buf, size_t buflen, off_t skip)
{
	return sg_copy_buffer(sgl, nents, buf, buflen, skip, true);
}

#define sg_pcopy_from_buffer LINUX_BACKPORT(sg_pcopy_from_buffer)

static inline
size_t sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents,
			    void *buf, size_t buflen, off_t skip)
{
	return sg_copy_buffer(sgl, nents, buf, buflen, skip, false);
}

#endif /* LINUX_VERSION_IS_LESS(3, 11, 0) */

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
