/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright 2002-2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * int8.c
 *
 * 8-way unrolled portable integer math RAID-6 instruction set
 *
 * This file is postprocessed using unroll.awk
 */

#include <linux/raid/pq.h>

/*
 * This is the C data type to use
 */

/* Change this from BITS_PER_LONG if there is something better... */
#if BITS_PER_LONG == 64
# define NBYTES(x) ((x) * 0x0101010101010101UL)
# define NSIZE  8
# define NSHIFT 3
# define NSTRING "64"
typedef u64 unative_t;
#else
# define NBYTES(x) ((x) * 0x01010101U)
# define NSIZE  4
# define NSHIFT 2
# define NSTRING "32"
typedef u32 unative_t;
#endif



/*
 * IA-64 wants insane amounts of unrolling.  On other architectures that
 * is just a waste of space.
 */
#if (8 <= 8) || defined(__ia64__)


/*
 * These sub-operations are separate inlines since they can sometimes be
 * specially optimized using architecture-specific hacks.
 */

/*
 * The SHLBYTE() operation shifts each byte left by 1, *not*
 * rolling over into the next byte
 */
static inline __attribute_const__ unative_t SHLBYTE(unative_t v)
{
	unative_t vv;

	vv = (v << 1) & NBYTES(0xfe);
	return vv;
}

/*
 * The MASK() operation returns 0xFF in any byte for which the high
 * bit is 1, 0x00 for any byte for which the high bit is 0.
 */
static inline __attribute_const__ unative_t MASK(unative_t v)
{
	unative_t vv;

	vv = v & NBYTES(0x80);
	vv = (vv << 1) - (vv >> 7); /* Overflow on the top bit is OK */
	return vv;
}


static void raid6_int8_gen_syndrome(int disks, size_t bytes, void **ptrs)
{
	u8 **dptr = (u8 **)ptrs;
	u8 *p, *q;
	int d, z, z0;

	unative_t wd0, wq0, wp0, w10, w20;
	unative_t wd1, wq1, wp1, w11, w21;
	unative_t wd2, wq2, wp2, w12, w22;
	unative_t wd3, wq3, wp3, w13, w23;
	unative_t wd4, wq4, wp4, w14, w24;
	unative_t wd5, wq5, wp5, w15, w25;
	unative_t wd6, wq6, wp6, w16, w26;
	unative_t wd7, wq7, wp7, w17, w27;

	z0 = disks - 3;		/* Highest data disk */
	p = dptr[z0+1];		/* XOR parity */
	q = dptr[z0+2];		/* RS syndrome */

	for ( d = 0 ; d < bytes ; d += NSIZE*8 ) {
		wq0 = wp0 = *(unative_t *)&dptr[z0][d+0*NSIZE];
		wq1 = wp1 = *(unative_t *)&dptr[z0][d+1*NSIZE];
		wq2 = wp2 = *(unative_t *)&dptr[z0][d+2*NSIZE];
		wq3 = wp3 = *(unative_t *)&dptr[z0][d+3*NSIZE];
		wq4 = wp4 = *(unative_t *)&dptr[z0][d+4*NSIZE];
		wq5 = wp5 = *(unative_t *)&dptr[z0][d+5*NSIZE];
		wq6 = wp6 = *(unative_t *)&dptr[z0][d+6*NSIZE];
		wq7 = wp7 = *(unative_t *)&dptr[z0][d+7*NSIZE];
		for ( z = z0-1 ; z >= 0 ; z-- ) {
			wd0 = *(unative_t *)&dptr[z][d+0*NSIZE];
			wd1 = *(unative_t *)&dptr[z][d+1*NSIZE];
			wd2 = *(unative_t *)&dptr[z][d+2*NSIZE];
			wd3 = *(unative_t *)&dptr[z][d+3*NSIZE];
			wd4 = *(unative_t *)&dptr[z][d+4*NSIZE];
			wd5 = *(unative_t *)&dptr[z][d+5*NSIZE];
			wd6 = *(unative_t *)&dptr[z][d+6*NSIZE];
			wd7 = *(unative_t *)&dptr[z][d+7*NSIZE];
			wp0 ^= wd0;
			wp1 ^= wd1;
			wp2 ^= wd2;
			wp3 ^= wd3;
			wp4 ^= wd4;
			wp5 ^= wd5;
			wp6 ^= wd6;
			wp7 ^= wd7;
			w20 = MASK(wq0);
			w21 = MASK(wq1);
			w22 = MASK(wq2);
			w23 = MASK(wq3);
			w24 = MASK(wq4);
			w25 = MASK(wq5);
			w26 = MASK(wq6);
			w27 = MASK(wq7);
			w10 = SHLBYTE(wq0);
			w11 = SHLBYTE(wq1);
			w12 = SHLBYTE(wq2);
			w13 = SHLBYTE(wq3);
			w14 = SHLBYTE(wq4);
			w15 = SHLBYTE(wq5);
			w16 = SHLBYTE(wq6);
			w17 = SHLBYTE(wq7);
			w20 &= NBYTES(0x1d);
			w21 &= NBYTES(0x1d);
			w22 &= NBYTES(0x1d);
			w23 &= NBYTES(0x1d);
			w24 &= NBYTES(0x1d);
			w25 &= NBYTES(0x1d);
			w26 &= NBYTES(0x1d);
			w27 &= NBYTES(0x1d);
			w10 ^= w20;
			w11 ^= w21;
			w12 ^= w22;
			w13 ^= w23;
			w14 ^= w24;
			w15 ^= w25;
			w16 ^= w26;
			w17 ^= w27;
			wq0 = w10 ^ wd0;
			wq1 = w11 ^ wd1;
			wq2 = w12 ^ wd2;
			wq3 = w13 ^ wd3;
			wq4 = w14 ^ wd4;
			wq5 = w15 ^ wd5;
			wq6 = w16 ^ wd6;
			wq7 = w17 ^ wd7;
		}
		*(unative_t *)&p[d+NSIZE*0] = wp0;
		*(unative_t *)&p[d+NSIZE*1] = wp1;
		*(unative_t *)&p[d+NSIZE*2] = wp2;
		*(unative_t *)&p[d+NSIZE*3] = wp3;
		*(unative_t *)&p[d+NSIZE*4] = wp4;
		*(unative_t *)&p[d+NSIZE*5] = wp5;
		*(unative_t *)&p[d+NSIZE*6] = wp6;
		*(unative_t *)&p[d+NSIZE*7] = wp7;
		*(unative_t *)&q[d+NSIZE*0] = wq0;
		*(unative_t *)&q[d+NSIZE*1] = wq1;
		*(unative_t *)&q[d+NSIZE*2] = wq2;
		*(unative_t *)&q[d+NSIZE*3] = wq3;
		*(unative_t *)&q[d+NSIZE*4] = wq4;
		*(unative_t *)&q[d+NSIZE*5] = wq5;
		*(unative_t *)&q[d+NSIZE*6] = wq6;
		*(unative_t *)&q[d+NSIZE*7] = wq7;
	}
}

static void raid6_int8_xor_syndrome(int disks, int start, int stop,
				     size_t bytes, void **ptrs)
{
	u8 **dptr = (u8 **)ptrs;
	u8 *p, *q;
	int d, z, z0;

	unative_t wd0, wq0, wp0, w10, w20;
	unative_t wd1, wq1, wp1, w11, w21;
	unative_t wd2, wq2, wp2, w12, w22;
	unative_t wd3, wq3, wp3, w13, w23;
	unative_t wd4, wq4, wp4, w14, w24;
	unative_t wd5, wq5, wp5, w15, w25;
	unative_t wd6, wq6, wp6, w16, w26;
	unative_t wd7, wq7, wp7, w17, w27;

	z0 = stop;		/* P/Q right side optimization */
	p = dptr[disks-2];	/* XOR parity */
	q = dptr[disks-1];	/* RS syndrome */

	for ( d = 0 ; d < bytes ; d += NSIZE*8 ) {
		/* P/Q data pages */
		wq0 = wp0 = *(unative_t *)&dptr[z0][d+0*NSIZE];
		wq1 = wp1 = *(unative_t *)&dptr[z0][d+1*NSIZE];
		wq2 = wp2 = *(unative_t *)&dptr[z0][d+2*NSIZE];
		wq3 = wp3 = *(unative_t *)&dptr[z0][d+3*NSIZE];
		wq4 = wp4 = *(unative_t *)&dptr[z0][d+4*NSIZE];
		wq5 = wp5 = *(unative_t *)&dptr[z0][d+5*NSIZE];
		wq6 = wp6 = *(unative_t *)&dptr[z0][d+6*NSIZE];
		wq7 = wp7 = *(unative_t *)&dptr[z0][d+7*NSIZE];
		for ( z = z0-1 ; z >= start ; z-- ) {
			wd0 = *(unative_t *)&dptr[z][d+0*NSIZE];
			wd1 = *(unative_t *)&dptr[z][d+1*NSIZE];
			wd2 = *(unative_t *)&dptr[z][d+2*NSIZE];
			wd3 = *(unative_t *)&dptr[z][d+3*NSIZE];
			wd4 = *(unative_t *)&dptr[z][d+4*NSIZE];
			wd5 = *(unative_t *)&dptr[z][d+5*NSIZE];
			wd6 = *(unative_t *)&dptr[z][d+6*NSIZE];
			wd7 = *(unative_t *)&dptr[z][d+7*NSIZE];
			wp0 ^= wd0;
			wp1 ^= wd1;
			wp2 ^= wd2;
			wp3 ^= wd3;
			wp4 ^= wd4;
			wp5 ^= wd5;
			wp6 ^= wd6;
			wp7 ^= wd7;
			w20 = MASK(wq0);
			w21 = MASK(wq1);
			w22 = MASK(wq2);
			w23 = MASK(wq3);
			w24 = MASK(wq4);
			w25 = MASK(wq5);
			w26 = MASK(wq6);
			w27 = MASK(wq7);
			w10 = SHLBYTE(wq0);
			w11 = SHLBYTE(wq1);
			w12 = SHLBYTE(wq2);
			w13 = SHLBYTE(wq3);
			w14 = SHLBYTE(wq4);
			w15 = SHLBYTE(wq5);
			w16 = SHLBYTE(wq6);
			w17 = SHLBYTE(wq7);
			w20 &= NBYTES(0x1d);
			w21 &= NBYTES(0x1d);
			w22 &= NBYTES(0x1d);
			w23 &= NBYTES(0x1d);
			w24 &= NBYTES(0x1d);
			w25 &= NBYTES(0x1d);
			w26 &= NBYTES(0x1d);
			w27 &= NBYTES(0x1d);
			w10 ^= w20;
			w11 ^= w21;
			w12 ^= w22;
			w13 ^= w23;
			w14 ^= w24;
			w15 ^= w25;
			w16 ^= w26;
			w17 ^= w27;
			wq0 = w10 ^ wd0;
			wq1 = w11 ^ wd1;
			wq2 = w12 ^ wd2;
			wq3 = w13 ^ wd3;
			wq4 = w14 ^ wd4;
			wq5 = w15 ^ wd5;
			wq6 = w16 ^ wd6;
			wq7 = w17 ^ wd7;
		}
		/* P/Q left side optimization */
		for ( z = start-1 ; z >= 0 ; z-- ) {
			w20 = MASK(wq0);
			w21 = MASK(wq1);
			w22 = MASK(wq2);
			w23 = MASK(wq3);
			w24 = MASK(wq4);
			w25 = MASK(wq5);
			w26 = MASK(wq6);
			w27 = MASK(wq7);
			w10 = SHLBYTE(wq0);
			w11 = SHLBYTE(wq1);
			w12 = SHLBYTE(wq2);
			w13 = SHLBYTE(wq3);
			w14 = SHLBYTE(wq4);
			w15 = SHLBYTE(wq5);
			w16 = SHLBYTE(wq6);
			w17 = SHLBYTE(wq7);
			w20 &= NBYTES(0x1d);
			w21 &= NBYTES(0x1d);
			w22 &= NBYTES(0x1d);
			w23 &= NBYTES(0x1d);
			w24 &= NBYTES(0x1d);
			w25 &= NBYTES(0x1d);
			w26 &= NBYTES(0x1d);
			w27 &= NBYTES(0x1d);
			wq0 = w10 ^ w20;
			wq1 = w11 ^ w21;
			wq2 = w12 ^ w22;
			wq3 = w13 ^ w23;
			wq4 = w14 ^ w24;
			wq5 = w15 ^ w25;
			wq6 = w16 ^ w26;
			wq7 = w17 ^ w27;
		}
		*(unative_t *)&p[d+NSIZE*0] ^= wp0;
		*(unative_t *)&p[d+NSIZE*1] ^= wp1;
		*(unative_t *)&p[d+NSIZE*2] ^= wp2;
		*(unative_t *)&p[d+NSIZE*3] ^= wp3;
		*(unative_t *)&p[d+NSIZE*4] ^= wp4;
		*(unative_t *)&p[d+NSIZE*5] ^= wp5;
		*(unative_t *)&p[d+NSIZE*6] ^= wp6;
		*(unative_t *)&p[d+NSIZE*7] ^= wp7;
		*(unative_t *)&q[d+NSIZE*0] ^= wq0;
		*(unative_t *)&q[d+NSIZE*1] ^= wq1;
		*(unative_t *)&q[d+NSIZE*2] ^= wq2;
		*(unative_t *)&q[d+NSIZE*3] ^= wq3;
		*(unative_t *)&q[d+NSIZE*4] ^= wq4;
		*(unative_t *)&q[d+NSIZE*5] ^= wq5;
		*(unative_t *)&q[d+NSIZE*6] ^= wq6;
		*(unative_t *)&q[d+NSIZE*7] ^= wq7;
	}

}

const struct raid6_calls raid6_intx8 = {
	raid6_int8_gen_syndrome,
	raid6_int8_xor_syndrome,
	NULL,			/* always valid */
	"int" NSTRING "x8",
	0
};

#endif
