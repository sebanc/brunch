#ifndef __BP_LINUX_IO_H
#define __BP_LINUX_IO_H
#include_next <linux/io.h>

#ifndef IOMEM_ERR_PTR
#define IOMEM_ERR_PTR(err) (__force void __iomem *)ERR_PTR(err)
#endif

#endif /* __BP_LINUX_IO_H */
