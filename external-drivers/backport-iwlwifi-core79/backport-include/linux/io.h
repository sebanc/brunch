#ifndef __BP_LINUX_IO_H
#define __BP_LINUX_IO_H
#include_next <linux/io.h>

#ifndef IOMEM_ERR_PTR
#define IOMEM_ERR_PTR(err) (__force void __iomem *)ERR_PTR(err)
#endif

#if LINUX_VERSION_IS_LESS(4,5,0)
#define __ioread32_copy LINUX_BACKPORT(__ioread32_copy)
void __ioread32_copy(void *to, const void __iomem *from, size_t count);
#endif

#ifndef writel_relaxed
#define writel_relaxed writel_relaxed
static inline void writel_relaxed(u32 value, volatile void __iomem *addr)
{
	__raw_writel(__cpu_to_le32(value), addr);
}
#endif

#endif /* __BP_LINUX_IO_H */
