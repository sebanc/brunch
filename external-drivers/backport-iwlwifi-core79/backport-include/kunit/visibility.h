#ifndef __BP_KUNIT_VISIBILITY_H
#define __BP_KUNIT_VISIBILITY_H
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(6,2,0)
#define VISIBLE_IF_KUNIT static
#define EXPORT_SYMBOL_IF_KUNIT(symbol)
#else
#include_next <kunit/visibility.h>
#endif

#endif /* __BP_KUNIT_VISIBILITY_H */
