#ifndef __BP_BUG_H
#define __BP_BUG_H
#include_next <linux/bug.h>

#ifndef __BUILD_BUG_ON_NOT_POWER_OF_2
#ifdef __CHECKER__
#define __BUILD_BUG_ON_NOT_POWER_OF_2(n) (0)
#else
#define __BUILD_BUG_ON_NOT_POWER_OF_2(n)	\
	BUILD_BUG_ON(((n) & ((n) - 1)) != 0)
#endif /* __CHECKER__ */
#endif /* __BUILD_BUG_ON_NOT_POWER_OF_2 */

#ifndef BUILD_BUG_ON_MSG
#define BUILD_BUG_ON_MSG(x, msg) BUILD_BUG_ON(x)
#endif

#endif /* __BP_BUG_H */
