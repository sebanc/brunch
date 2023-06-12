#ifndef _BACKPORT_LINUX_CORDIC_H
#define _BACKPORT_LINUX_CORDIC_H 1

#include_next <linux/cordic.h>


#ifndef CORDIC_FLOAT
#define CORDIC_ANGLE_GEN	39797
#define CORDIC_PRECISION_SHIFT	16
#define CORDIC_NUM_ITER	(CORDIC_PRECISION_SHIFT + 2)

#define CORDIC_FIXED(X)	((s32)((X) << CORDIC_PRECISION_SHIFT))
#define CORDIC_FLOAT(X)	(((X) >= 0) \
		? ((((X) >> (CORDIC_PRECISION_SHIFT - 1)) + 1) >> 1) \
		: -((((-(X)) >> (CORDIC_PRECISION_SHIFT - 1)) + 1) >> 1))
#endif

#endif /* _BACKPORT_LINUX_CORDIC_H */
