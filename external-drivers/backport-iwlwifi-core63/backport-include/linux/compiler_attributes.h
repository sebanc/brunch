#ifndef _BACKPORTS_LINUX_COMPILER_ATTRIBUTES_H
#define _BACKPORTS_LINUX_COMPILER_ATTRIBUTES_H 1

#if LINUX_VERSION_IS_GEQ(4,20,0)
#include_next <linux/compiler_attributes.h>
#endif

#ifndef __has_attribute
# define __has_attribute(x) __GCC4_has_attribute_##x
#endif

#ifndef __GCC4_has_attribute___fallthrough__
# define __GCC4_has_attribute___fallthrough__         0
#endif /* __GCC4_has_attribute___fallthrough__ */

#ifndef fallthrough
/*
 * Add the pseudo keyword 'fallthrough' so case statement blocks
 * must end with any of these keywords:
 *   break;
 *   fallthrough;
 *   goto <label>;
 *   return [expression];
 *
 *  gcc: https://gcc.gnu.org/onlinedocs/gcc/Statement-Attributes.html#Statement-Attributes
 */
#if __has_attribute(__fallthrough__)
# define fallthrough                    __attribute__((__fallthrough__))
#else
# define fallthrough                    do {} while (0)  /* fallthrough */
#endif
#endif /* fallthrough */

#endif /* _BACKPORTS_LINUX_COMPILER_ATTRIBUTES_H */
