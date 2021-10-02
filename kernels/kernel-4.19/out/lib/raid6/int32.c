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
 * int32.c
 *
 * 32-way unrolled portable integer math RAID-6 instruction set
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
#if (32 <= 8) || defined(__ia64__)


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


static void raid6_int32_gen_syndrome(int disks, size_t bytes, void **ptrs)
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
	unative_t wd8, wq8, wp8, w18, w28;
	unative_t wd9, wq9, wp9, w19, w29;
	unative_t wd10, wq10, wp10, w110, w210;
	unative_t wd11, wq11, wp11, w111, w211;
	unative_t wd12, wq12, wp12, w112, w212;
	unative_t wd13, wq13, wp13, w113, w213;
	unative_t wd14, wq14, wp14, w114, w214;
	unative_t wd15, wq15, wp15, w115, w215;
	unative_t wd16, wq16, wp16, w116, w216;
	unative_t wd17, wq17, wp17, w117, w217;
	unative_t wd18, wq18, wp18, w118, w218;
	unative_t wd19, wq19, wp19, w119, w219;
	unative_t wd20, wq20, wp20, w120, w220;
	unative_t wd21, wq21, wp21, w121, w221;
	unative_t wd22, wq22, wp22, w122, w222;
	unative_t wd23, wq23, wp23, w123, w223;
	unative_t wd24, wq24, wp24, w124, w224;
	unative_t wd25, wq25, wp25, w125, w225;
	unative_t wd26, wq26, wp26, w126, w226;
	unative_t wd27, wq27, wp27, w127, w227;
	unative_t wd28, wq28, wp28, w128, w228;
	unative_t wd29, wq29, wp29, w129, w229;
	unative_t wd30, wq30, wp30, w130, w230;
	unative_t wd31, wq31, wp31, w131, w231;

	z0 = disks - 3;		/* Highest data disk */
	p = dptr[z0+1];		/* XOR parity */
	q = dptr[z0+2];		/* RS syndrome */

	for ( d = 0 ; d < bytes ; d += NSIZE*32 ) {
		wq0 = wp0 = *(unative_t *)&dptr[z0][d+0*NSIZE];
		wq1 = wp1 = *(unative_t *)&dptr[z0][d+1*NSIZE];
		wq2 = wp2 = *(unative_t *)&dptr[z0][d+2*NSIZE];
		wq3 = wp3 = *(unative_t *)&dptr[z0][d+3*NSIZE];
		wq4 = wp4 = *(unative_t *)&dptr[z0][d+4*NSIZE];
		wq5 = wp5 = *(unative_t *)&dptr[z0][d+5*NSIZE];
		wq6 = wp6 = *(unative_t *)&dptr[z0][d+6*NSIZE];
		wq7 = wp7 = *(unative_t *)&dptr[z0][d+7*NSIZE];
		wq8 = wp8 = *(unative_t *)&dptr[z0][d+8*NSIZE];
		wq9 = wp9 = *(unative_t *)&dptr[z0][d+9*NSIZE];
		wq10 = wp10 = *(unative_t *)&dptr[z0][d+10*NSIZE];
		wq11 = wp11 = *(unative_t *)&dptr[z0][d+11*NSIZE];
		wq12 = wp12 = *(unative_t *)&dptr[z0][d+12*NSIZE];
		wq13 = wp13 = *(unative_t *)&dptr[z0][d+13*NSIZE];
		wq14 = wp14 = *(unative_t *)&dptr[z0][d+14*NSIZE];
		wq15 = wp15 = *(unative_t *)&dptr[z0][d+15*NSIZE];
		wq16 = wp16 = *(unative_t *)&dptr[z0][d+16*NSIZE];
		wq17 = wp17 = *(unative_t *)&dptr[z0][d+17*NSIZE];
		wq18 = wp18 = *(unative_t *)&dptr[z0][d+18*NSIZE];
		wq19 = wp19 = *(unative_t *)&dptr[z0][d+19*NSIZE];
		wq20 = wp20 = *(unative_t *)&dptr[z0][d+20*NSIZE];
		wq21 = wp21 = *(unative_t *)&dptr[z0][d+21*NSIZE];
		wq22 = wp22 = *(unative_t *)&dptr[z0][d+22*NSIZE];
		wq23 = wp23 = *(unative_t *)&dptr[z0][d+23*NSIZE];
		wq24 = wp24 = *(unative_t *)&dptr[z0][d+24*NSIZE];
		wq25 = wp25 = *(unative_t *)&dptr[z0][d+25*NSIZE];
		wq26 = wp26 = *(unative_t *)&dptr[z0][d+26*NSIZE];
		wq27 = wp27 = *(unative_t *)&dptr[z0][d+27*NSIZE];
		wq28 = wp28 = *(unative_t *)&dptr[z0][d+28*NSIZE];
		wq29 = wp29 = *(unative_t *)&dptr[z0][d+29*NSIZE];
		wq30 = wp30 = *(unative_t *)&dptr[z0][d+30*NSIZE];
		wq31 = wp31 = *(unative_t *)&dptr[z0][d+31*NSIZE];
		for ( z = z0-1 ; z >= 0 ; z-- ) {
			wd0 = *(unative_t *)&dptr[z][d+0*NSIZE];
			wd1 = *(unative_t *)&dptr[z][d+1*NSIZE];
			wd2 = *(unative_t *)&dptr[z][d+2*NSIZE];
			wd3 = *(unative_t *)&dptr[z][d+3*NSIZE];
			wd4 = *(unative_t *)&dptr[z][d+4*NSIZE];
			wd5 = *(unative_t *)&dptr[z][d+5*NSIZE];
			wd6 = *(unative_t *)&dptr[z][d+6*NSIZE];
			wd7 = *(unative_t *)&dptr[z][d+7*NSIZE];
			wd8 = *(unative_t *)&dptr[z][d+8*NSIZE];
			wd9 = *(unative_t *)&dptr[z][d+9*NSIZE];
			wd10 = *(unative_t *)&dptr[z][d+10*NSIZE];
			wd11 = *(unative_t *)&dptr[z][d+11*NSIZE];
			wd12 = *(unative_t *)&dptr[z][d+12*NSIZE];
			wd13 = *(unative_t *)&dptr[z][d+13*NSIZE];
			wd14 = *(unative_t *)&dptr[z][d+14*NSIZE];
			wd15 = *(unative_t *)&dptr[z][d+15*NSIZE];
			wd16 = *(unative_t *)&dptr[z][d+16*NSIZE];
			wd17 = *(unative_t *)&dptr[z][d+17*NSIZE];
			wd18 = *(unative_t *)&dptr[z][d+18*NSIZE];
			wd19 = *(unative_t *)&dptr[z][d+19*NSIZE];
			wd20 = *(unative_t *)&dptr[z][d+20*NSIZE];
			wd21 = *(unative_t *)&dptr[z][d+21*NSIZE];
			wd22 = *(unative_t *)&dptr[z][d+22*NSIZE];
			wd23 = *(unative_t *)&dptr[z][d+23*NSIZE];
			wd24 = *(unative_t *)&dptr[z][d+24*NSIZE];
			wd25 = *(unative_t *)&dptr[z][d+25*NSIZE];
			wd26 = *(unative_t *)&dptr[z][d+26*NSIZE];
			wd27 = *(unative_t *)&dptr[z][d+27*NSIZE];
			wd28 = *(unative_t *)&dptr[z][d+28*NSIZE];
			wd29 = *(unative_t *)&dptr[z][d+29*NSIZE];
			wd30 = *(unative_t *)&dptr[z][d+30*NSIZE];
			wd31 = *(unative_t *)&dptr[z][d+31*NSIZE];
			wp0 ^= wd0;
			wp1 ^= wd1;
			wp2 ^= wd2;
			wp3 ^= wd3;
			wp4 ^= wd4;
			wp5 ^= wd5;
			wp6 ^= wd6;
			wp7 ^= wd7;
			wp8 ^= wd8;
			wp9 ^= wd9;
			wp10 ^= wd10;
			wp11 ^= wd11;
			wp12 ^= wd12;
			wp13 ^= wd13;
			wp14 ^= wd14;
			wp15 ^= wd15;
			wp16 ^= wd16;
			wp17 ^= wd17;
			wp18 ^= wd18;
			wp19 ^= wd19;
			wp20 ^= wd20;
			wp21 ^= wd21;
			wp22 ^= wd22;
			wp23 ^= wd23;
			wp24 ^= wd24;
			wp25 ^= wd25;
			wp26 ^= wd26;
			wp27 ^= wd27;
			wp28 ^= wd28;
			wp29 ^= wd29;
			wp30 ^= wd30;
			wp31 ^= wd31;
			w20 = MASK(wq0);
			w21 = MASK(wq1);
			w22 = MASK(wq2);
			w23 = MASK(wq3);
			w24 = MASK(wq4);
			w25 = MASK(wq5);
			w26 = MASK(wq6);
			w27 = MASK(wq7);
			w28 = MASK(wq8);
			w29 = MASK(wq9);
			w210 = MASK(wq10);
			w211 = MASK(wq11);
			w212 = MASK(wq12);
			w213 = MASK(wq13);
			w214 = MASK(wq14);
			w215 = MASK(wq15);
			w216 = MASK(wq16);
			w217 = MASK(wq17);
			w218 = MASK(wq18);
			w219 = MASK(wq19);
			w220 = MASK(wq20);
			w221 = MASK(wq21);
			w222 = MASK(wq22);
			w223 = MASK(wq23);
			w224 = MASK(wq24);
			w225 = MASK(wq25);
			w226 = MASK(wq26);
			w227 = MASK(wq27);
			w228 = MASK(wq28);
			w229 = MASK(wq29);
			w230 = MASK(wq30);
			w231 = MASK(wq31);
			w10 = SHLBYTE(wq0);
			w11 = SHLBYTE(wq1);
			w12 = SHLBYTE(wq2);
			w13 = SHLBYTE(wq3);
			w14 = SHLBYTE(wq4);
			w15 = SHLBYTE(wq5);
			w16 = SHLBYTE(wq6);
			w17 = SHLBYTE(wq7);
			w18 = SHLBYTE(wq8);
			w19 = SHLBYTE(wq9);
			w110 = SHLBYTE(wq10);
			w111 = SHLBYTE(wq11);
			w112 = SHLBYTE(wq12);
			w113 = SHLBYTE(wq13);
			w114 = SHLBYTE(wq14);
			w115 = SHLBYTE(wq15);
			w116 = SHLBYTE(wq16);
			w117 = SHLBYTE(wq17);
			w118 = SHLBYTE(wq18);
			w119 = SHLBYTE(wq19);
			w120 = SHLBYTE(wq20);
			w121 = SHLBYTE(wq21);
			w122 = SHLBYTE(wq22);
			w123 = SHLBYTE(wq23);
			w124 = SHLBYTE(wq24);
			w125 = SHLBYTE(wq25);
			w126 = SHLBYTE(wq26);
			w127 = SHLBYTE(wq27);
			w128 = SHLBYTE(wq28);
			w129 = SHLBYTE(wq29);
			w130 = SHLBYTE(wq30);
			w131 = SHLBYTE(wq31);
			w20 &= NBYTES(0x1d);
			w21 &= NBYTES(0x1d);
			w22 &= NBYTES(0x1d);
			w23 &= NBYTES(0x1d);
			w24 &= NBYTES(0x1d);
			w25 &= NBYTES(0x1d);
			w26 &= NBYTES(0x1d);
			w27 &= NBYTES(0x1d);
			w28 &= NBYTES(0x1d);
			w29 &= NBYTES(0x1d);
			w210 &= NBYTES(0x1d);
			w211 &= NBYTES(0x1d);
			w212 &= NBYTES(0x1d);
			w213 &= NBYTES(0x1d);
			w214 &= NBYTES(0x1d);
			w215 &= NBYTES(0x1d);
			w216 &= NBYTES(0x1d);
			w217 &= NBYTES(0x1d);
			w218 &= NBYTES(0x1d);
			w219 &= NBYTES(0x1d);
			w220 &= NBYTES(0x1d);
			w221 &= NBYTES(0x1d);
			w222 &= NBYTES(0x1d);
			w223 &= NBYTES(0x1d);
			w224 &= NBYTES(0x1d);
			w225 &= NBYTES(0x1d);
			w226 &= NBYTES(0x1d);
			w227 &= NBYTES(0x1d);
			w228 &= NBYTES(0x1d);
			w229 &= NBYTES(0x1d);
			w230 &= NBYTES(0x1d);
			w231 &= NBYTES(0x1d);
			w10 ^= w20;
			w11 ^= w21;
			w12 ^= w22;
			w13 ^= w23;
			w14 ^= w24;
			w15 ^= w25;
			w16 ^= w26;
			w17 ^= w27;
			w18 ^= w28;
			w19 ^= w29;
			w110 ^= w210;
			w111 ^= w211;
			w112 ^= w212;
			w113 ^= w213;
			w114 ^= w214;
			w115 ^= w215;
			w116 ^= w216;
			w117 ^= w217;
			w118 ^= w218;
			w119 ^= w219;
			w120 ^= w220;
			w121 ^= w221;
			w122 ^= w222;
			w123 ^= w223;
			w124 ^= w224;
			w125 ^= w225;
			w126 ^= w226;
			w127 ^= w227;
			w128 ^= w228;
			w129 ^= w229;
			w130 ^= w230;
			w131 ^= w231;
			wq0 = w10 ^ wd0;
			wq1 = w11 ^ wd1;
			wq2 = w12 ^ wd2;
			wq3 = w13 ^ wd3;
			wq4 = w14 ^ wd4;
			wq5 = w15 ^ wd5;
			wq6 = w16 ^ wd6;
			wq7 = w17 ^ wd7;
			wq8 = w18 ^ wd8;
			wq9 = w19 ^ wd9;
			wq10 = w110 ^ wd10;
			wq11 = w111 ^ wd11;
			wq12 = w112 ^ wd12;
			wq13 = w113 ^ wd13;
			wq14 = w114 ^ wd14;
			wq15 = w115 ^ wd15;
			wq16 = w116 ^ wd16;
			wq17 = w117 ^ wd17;
			wq18 = w118 ^ wd18;
			wq19 = w119 ^ wd19;
			wq20 = w120 ^ wd20;
			wq21 = w121 ^ wd21;
			wq22 = w122 ^ wd22;
			wq23 = w123 ^ wd23;
			wq24 = w124 ^ wd24;
			wq25 = w125 ^ wd25;
			wq26 = w126 ^ wd26;
			wq27 = w127 ^ wd27;
			wq28 = w128 ^ wd28;
			wq29 = w129 ^ wd29;
			wq30 = w130 ^ wd30;
			wq31 = w131 ^ wd31;
		}
		*(unative_t *)&p[d+NSIZE*0] = wp0;
		*(unative_t *)&p[d+NSIZE*1] = wp1;
		*(unative_t *)&p[d+NSIZE*2] = wp2;
		*(unative_t *)&p[d+NSIZE*3] = wp3;
		*(unative_t *)&p[d+NSIZE*4] = wp4;
		*(unative_t *)&p[d+NSIZE*5] = wp5;
		*(unative_t *)&p[d+NSIZE*6] = wp6;
		*(unative_t *)&p[d+NSIZE*7] = wp7;
		*(unative_t *)&p[d+NSIZE*8] = wp8;
		*(unative_t *)&p[d+NSIZE*9] = wp9;
		*(unative_t *)&p[d+NSIZE*10] = wp10;
		*(unative_t *)&p[d+NSIZE*11] = wp11;
		*(unative_t *)&p[d+NSIZE*12] = wp12;
		*(unative_t *)&p[d+NSIZE*13] = wp13;
		*(unative_t *)&p[d+NSIZE*14] = wp14;
		*(unative_t *)&p[d+NSIZE*15] = wp15;
		*(unative_t *)&p[d+NSIZE*16] = wp16;
		*(unative_t *)&p[d+NSIZE*17] = wp17;
		*(unative_t *)&p[d+NSIZE*18] = wp18;
		*(unative_t *)&p[d+NSIZE*19] = wp19;
		*(unative_t *)&p[d+NSIZE*20] = wp20;
		*(unative_t *)&p[d+NSIZE*21] = wp21;
		*(unative_t *)&p[d+NSIZE*22] = wp22;
		*(unative_t *)&p[d+NSIZE*23] = wp23;
		*(unative_t *)&p[d+NSIZE*24] = wp24;
		*(unative_t *)&p[d+NSIZE*25] = wp25;
		*(unative_t *)&p[d+NSIZE*26] = wp26;
		*(unative_t *)&p[d+NSIZE*27] = wp27;
		*(unative_t *)&p[d+NSIZE*28] = wp28;
		*(unative_t *)&p[d+NSIZE*29] = wp29;
		*(unative_t *)&p[d+NSIZE*30] = wp30;
		*(unative_t *)&p[d+NSIZE*31] = wp31;
		*(unative_t *)&q[d+NSIZE*0] = wq0;
		*(unative_t *)&q[d+NSIZE*1] = wq1;
		*(unative_t *)&q[d+NSIZE*2] = wq2;
		*(unative_t *)&q[d+NSIZE*3] = wq3;
		*(unative_t *)&q[d+NSIZE*4] = wq4;
		*(unative_t *)&q[d+NSIZE*5] = wq5;
		*(unative_t *)&q[d+NSIZE*6] = wq6;
		*(unative_t *)&q[d+NSIZE*7] = wq7;
		*(unative_t *)&q[d+NSIZE*8] = wq8;
		*(unative_t *)&q[d+NSIZE*9] = wq9;
		*(unative_t *)&q[d+NSIZE*10] = wq10;
		*(unative_t *)&q[d+NSIZE*11] = wq11;
		*(unative_t *)&q[d+NSIZE*12] = wq12;
		*(unative_t *)&q[d+NSIZE*13] = wq13;
		*(unative_t *)&q[d+NSIZE*14] = wq14;
		*(unative_t *)&q[d+NSIZE*15] = wq15;
		*(unative_t *)&q[d+NSIZE*16] = wq16;
		*(unative_t *)&q[d+NSIZE*17] = wq17;
		*(unative_t *)&q[d+NSIZE*18] = wq18;
		*(unative_t *)&q[d+NSIZE*19] = wq19;
		*(unative_t *)&q[d+NSIZE*20] = wq20;
		*(unative_t *)&q[d+NSIZE*21] = wq21;
		*(unative_t *)&q[d+NSIZE*22] = wq22;
		*(unative_t *)&q[d+NSIZE*23] = wq23;
		*(unative_t *)&q[d+NSIZE*24] = wq24;
		*(unative_t *)&q[d+NSIZE*25] = wq25;
		*(unative_t *)&q[d+NSIZE*26] = wq26;
		*(unative_t *)&q[d+NSIZE*27] = wq27;
		*(unative_t *)&q[d+NSIZE*28] = wq28;
		*(unative_t *)&q[d+NSIZE*29] = wq29;
		*(unative_t *)&q[d+NSIZE*30] = wq30;
		*(unative_t *)&q[d+NSIZE*31] = wq31;
	}
}

static void raid6_int32_xor_syndrome(int disks, int start, int stop,
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
	unative_t wd8, wq8, wp8, w18, w28;
	unative_t wd9, wq9, wp9, w19, w29;
	unative_t wd10, wq10, wp10, w110, w210;
	unative_t wd11, wq11, wp11, w111, w211;
	unative_t wd12, wq12, wp12, w112, w212;
	unative_t wd13, wq13, wp13, w113, w213;
	unative_t wd14, wq14, wp14, w114, w214;
	unative_t wd15, wq15, wp15, w115, w215;
	unative_t wd16, wq16, wp16, w116, w216;
	unative_t wd17, wq17, wp17, w117, w217;
	unative_t wd18, wq18, wp18, w118, w218;
	unative_t wd19, wq19, wp19, w119, w219;
	unative_t wd20, wq20, wp20, w120, w220;
	unative_t wd21, wq21, wp21, w121, w221;
	unative_t wd22, wq22, wp22, w122, w222;
	unative_t wd23, wq23, wp23, w123, w223;
	unative_t wd24, wq24, wp24, w124, w224;
	unative_t wd25, wq25, wp25, w125, w225;
	unative_t wd26, wq26, wp26, w126, w226;
	unative_t wd27, wq27, wp27, w127, w227;
	unative_t wd28, wq28, wp28, w128, w228;
	unative_t wd29, wq29, wp29, w129, w229;
	unative_t wd30, wq30, wp30, w130, w230;
	unative_t wd31, wq31, wp31, w131, w231;

	z0 = stop;		/* P/Q right side optimization */
	p = dptr[disks-2];	/* XOR parity */
	q = dptr[disks-1];	/* RS syndrome */

	for ( d = 0 ; d < bytes ; d += NSIZE*32 ) {
		/* P/Q data pages */
		wq0 = wp0 = *(unative_t *)&dptr[z0][d+0*NSIZE];
		wq1 = wp1 = *(unative_t *)&dptr[z0][d+1*NSIZE];
		wq2 = wp2 = *(unative_t *)&dptr[z0][d+2*NSIZE];
		wq3 = wp3 = *(unative_t *)&dptr[z0][d+3*NSIZE];
		wq4 = wp4 = *(unative_t *)&dptr[z0][d+4*NSIZE];
		wq5 = wp5 = *(unative_t *)&dptr[z0][d+5*NSIZE];
		wq6 = wp6 = *(unative_t *)&dptr[z0][d+6*NSIZE];
		wq7 = wp7 = *(unative_t *)&dptr[z0][d+7*NSIZE];
		wq8 = wp8 = *(unative_t *)&dptr[z0][d+8*NSIZE];
		wq9 = wp9 = *(unative_t *)&dptr[z0][d+9*NSIZE];
		wq10 = wp10 = *(unative_t *)&dptr[z0][d+10*NSIZE];
		wq11 = wp11 = *(unative_t *)&dptr[z0][d+11*NSIZE];
		wq12 = wp12 = *(unative_t *)&dptr[z0][d+12*NSIZE];
		wq13 = wp13 = *(unative_t *)&dptr[z0][d+13*NSIZE];
		wq14 = wp14 = *(unative_t *)&dptr[z0][d+14*NSIZE];
		wq15 = wp15 = *(unative_t *)&dptr[z0][d+15*NSIZE];
		wq16 = wp16 = *(unative_t *)&dptr[z0][d+16*NSIZE];
		wq17 = wp17 = *(unative_t *)&dptr[z0][d+17*NSIZE];
		wq18 = wp18 = *(unative_t *)&dptr[z0][d+18*NSIZE];
		wq19 = wp19 = *(unative_t *)&dptr[z0][d+19*NSIZE];
		wq20 = wp20 = *(unative_t *)&dptr[z0][d+20*NSIZE];
		wq21 = wp21 = *(unative_t *)&dptr[z0][d+21*NSIZE];
		wq22 = wp22 = *(unative_t *)&dptr[z0][d+22*NSIZE];
		wq23 = wp23 = *(unative_t *)&dptr[z0][d+23*NSIZE];
		wq24 = wp24 = *(unative_t *)&dptr[z0][d+24*NSIZE];
		wq25 = wp25 = *(unative_t *)&dptr[z0][d+25*NSIZE];
		wq26 = wp26 = *(unative_t *)&dptr[z0][d+26*NSIZE];
		wq27 = wp27 = *(unative_t *)&dptr[z0][d+27*NSIZE];
		wq28 = wp28 = *(unative_t *)&dptr[z0][d+28*NSIZE];
		wq29 = wp29 = *(unative_t *)&dptr[z0][d+29*NSIZE];
		wq30 = wp30 = *(unative_t *)&dptr[z0][d+30*NSIZE];
		wq31 = wp31 = *(unative_t *)&dptr[z0][d+31*NSIZE];
		for ( z = z0-1 ; z >= start ; z-- ) {
			wd0 = *(unative_t *)&dptr[z][d+0*NSIZE];
			wd1 = *(unative_t *)&dptr[z][d+1*NSIZE];
			wd2 = *(unative_t *)&dptr[z][d+2*NSIZE];
			wd3 = *(unative_t *)&dptr[z][d+3*NSIZE];
			wd4 = *(unative_t *)&dptr[z][d+4*NSIZE];
			wd5 = *(unative_t *)&dptr[z][d+5*NSIZE];
			wd6 = *(unative_t *)&dptr[z][d+6*NSIZE];
			wd7 = *(unative_t *)&dptr[z][d+7*NSIZE];
			wd8 = *(unative_t *)&dptr[z][d+8*NSIZE];
			wd9 = *(unative_t *)&dptr[z][d+9*NSIZE];
			wd10 = *(unative_t *)&dptr[z][d+10*NSIZE];
			wd11 = *(unative_t *)&dptr[z][d+11*NSIZE];
			wd12 = *(unative_t *)&dptr[z][d+12*NSIZE];
			wd13 = *(unative_t *)&dptr[z][d+13*NSIZE];
			wd14 = *(unative_t *)&dptr[z][d+14*NSIZE];
			wd15 = *(unative_t *)&dptr[z][d+15*NSIZE];
			wd16 = *(unative_t *)&dptr[z][d+16*NSIZE];
			wd17 = *(unative_t *)&dptr[z][d+17*NSIZE];
			wd18 = *(unative_t *)&dptr[z][d+18*NSIZE];
			wd19 = *(unative_t *)&dptr[z][d+19*NSIZE];
			wd20 = *(unative_t *)&dptr[z][d+20*NSIZE];
			wd21 = *(unative_t *)&dptr[z][d+21*NSIZE];
			wd22 = *(unative_t *)&dptr[z][d+22*NSIZE];
			wd23 = *(unative_t *)&dptr[z][d+23*NSIZE];
			wd24 = *(unative_t *)&dptr[z][d+24*NSIZE];
			wd25 = *(unative_t *)&dptr[z][d+25*NSIZE];
			wd26 = *(unative_t *)&dptr[z][d+26*NSIZE];
			wd27 = *(unative_t *)&dptr[z][d+27*NSIZE];
			wd28 = *(unative_t *)&dptr[z][d+28*NSIZE];
			wd29 = *(unative_t *)&dptr[z][d+29*NSIZE];
			wd30 = *(unative_t *)&dptr[z][d+30*NSIZE];
			wd31 = *(unative_t *)&dptr[z][d+31*NSIZE];
			wp0 ^= wd0;
			wp1 ^= wd1;
			wp2 ^= wd2;
			wp3 ^= wd3;
			wp4 ^= wd4;
			wp5 ^= wd5;
			wp6 ^= wd6;
			wp7 ^= wd7;
			wp8 ^= wd8;
			wp9 ^= wd9;
			wp10 ^= wd10;
			wp11 ^= wd11;
			wp12 ^= wd12;
			wp13 ^= wd13;
			wp14 ^= wd14;
			wp15 ^= wd15;
			wp16 ^= wd16;
			wp17 ^= wd17;
			wp18 ^= wd18;
			wp19 ^= wd19;
			wp20 ^= wd20;
			wp21 ^= wd21;
			wp22 ^= wd22;
			wp23 ^= wd23;
			wp24 ^= wd24;
			wp25 ^= wd25;
			wp26 ^= wd26;
			wp27 ^= wd27;
			wp28 ^= wd28;
			wp29 ^= wd29;
			wp30 ^= wd30;
			wp31 ^= wd31;
			w20 = MASK(wq0);
			w21 = MASK(wq1);
			w22 = MASK(wq2);
			w23 = MASK(wq3);
			w24 = MASK(wq4);
			w25 = MASK(wq5);
			w26 = MASK(wq6);
			w27 = MASK(wq7);
			w28 = MASK(wq8);
			w29 = MASK(wq9);
			w210 = MASK(wq10);
			w211 = MASK(wq11);
			w212 = MASK(wq12);
			w213 = MASK(wq13);
			w214 = MASK(wq14);
			w215 = MASK(wq15);
			w216 = MASK(wq16);
			w217 = MASK(wq17);
			w218 = MASK(wq18);
			w219 = MASK(wq19);
			w220 = MASK(wq20);
			w221 = MASK(wq21);
			w222 = MASK(wq22);
			w223 = MASK(wq23);
			w224 = MASK(wq24);
			w225 = MASK(wq25);
			w226 = MASK(wq26);
			w227 = MASK(wq27);
			w228 = MASK(wq28);
			w229 = MASK(wq29);
			w230 = MASK(wq30);
			w231 = MASK(wq31);
			w10 = SHLBYTE(wq0);
			w11 = SHLBYTE(wq1);
			w12 = SHLBYTE(wq2);
			w13 = SHLBYTE(wq3);
			w14 = SHLBYTE(wq4);
			w15 = SHLBYTE(wq5);
			w16 = SHLBYTE(wq6);
			w17 = SHLBYTE(wq7);
			w18 = SHLBYTE(wq8);
			w19 = SHLBYTE(wq9);
			w110 = SHLBYTE(wq10);
			w111 = SHLBYTE(wq11);
			w112 = SHLBYTE(wq12);
			w113 = SHLBYTE(wq13);
			w114 = SHLBYTE(wq14);
			w115 = SHLBYTE(wq15);
			w116 = SHLBYTE(wq16);
			w117 = SHLBYTE(wq17);
			w118 = SHLBYTE(wq18);
			w119 = SHLBYTE(wq19);
			w120 = SHLBYTE(wq20);
			w121 = SHLBYTE(wq21);
			w122 = SHLBYTE(wq22);
			w123 = SHLBYTE(wq23);
			w124 = SHLBYTE(wq24);
			w125 = SHLBYTE(wq25);
			w126 = SHLBYTE(wq26);
			w127 = SHLBYTE(wq27);
			w128 = SHLBYTE(wq28);
			w129 = SHLBYTE(wq29);
			w130 = SHLBYTE(wq30);
			w131 = SHLBYTE(wq31);
			w20 &= NBYTES(0x1d);
			w21 &= NBYTES(0x1d);
			w22 &= NBYTES(0x1d);
			w23 &= NBYTES(0x1d);
			w24 &= NBYTES(0x1d);
			w25 &= NBYTES(0x1d);
			w26 &= NBYTES(0x1d);
			w27 &= NBYTES(0x1d);
			w28 &= NBYTES(0x1d);
			w29 &= NBYTES(0x1d);
			w210 &= NBYTES(0x1d);
			w211 &= NBYTES(0x1d);
			w212 &= NBYTES(0x1d);
			w213 &= NBYTES(0x1d);
			w214 &= NBYTES(0x1d);
			w215 &= NBYTES(0x1d);
			w216 &= NBYTES(0x1d);
			w217 &= NBYTES(0x1d);
			w218 &= NBYTES(0x1d);
			w219 &= NBYTES(0x1d);
			w220 &= NBYTES(0x1d);
			w221 &= NBYTES(0x1d);
			w222 &= NBYTES(0x1d);
			w223 &= NBYTES(0x1d);
			w224 &= NBYTES(0x1d);
			w225 &= NBYTES(0x1d);
			w226 &= NBYTES(0x1d);
			w227 &= NBYTES(0x1d);
			w228 &= NBYTES(0x1d);
			w229 &= NBYTES(0x1d);
			w230 &= NBYTES(0x1d);
			w231 &= NBYTES(0x1d);
			w10 ^= w20;
			w11 ^= w21;
			w12 ^= w22;
			w13 ^= w23;
			w14 ^= w24;
			w15 ^= w25;
			w16 ^= w26;
			w17 ^= w27;
			w18 ^= w28;
			w19 ^= w29;
			w110 ^= w210;
			w111 ^= w211;
			w112 ^= w212;
			w113 ^= w213;
			w114 ^= w214;
			w115 ^= w215;
			w116 ^= w216;
			w117 ^= w217;
			w118 ^= w218;
			w119 ^= w219;
			w120 ^= w220;
			w121 ^= w221;
			w122 ^= w222;
			w123 ^= w223;
			w124 ^= w224;
			w125 ^= w225;
			w126 ^= w226;
			w127 ^= w227;
			w128 ^= w228;
			w129 ^= w229;
			w130 ^= w230;
			w131 ^= w231;
			wq0 = w10 ^ wd0;
			wq1 = w11 ^ wd1;
			wq2 = w12 ^ wd2;
			wq3 = w13 ^ wd3;
			wq4 = w14 ^ wd4;
			wq5 = w15 ^ wd5;
			wq6 = w16 ^ wd6;
			wq7 = w17 ^ wd7;
			wq8 = w18 ^ wd8;
			wq9 = w19 ^ wd9;
			wq10 = w110 ^ wd10;
			wq11 = w111 ^ wd11;
			wq12 = w112 ^ wd12;
			wq13 = w113 ^ wd13;
			wq14 = w114 ^ wd14;
			wq15 = w115 ^ wd15;
			wq16 = w116 ^ wd16;
			wq17 = w117 ^ wd17;
			wq18 = w118 ^ wd18;
			wq19 = w119 ^ wd19;
			wq20 = w120 ^ wd20;
			wq21 = w121 ^ wd21;
			wq22 = w122 ^ wd22;
			wq23 = w123 ^ wd23;
			wq24 = w124 ^ wd24;
			wq25 = w125 ^ wd25;
			wq26 = w126 ^ wd26;
			wq27 = w127 ^ wd27;
			wq28 = w128 ^ wd28;
			wq29 = w129 ^ wd29;
			wq30 = w130 ^ wd30;
			wq31 = w131 ^ wd31;
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
			w28 = MASK(wq8);
			w29 = MASK(wq9);
			w210 = MASK(wq10);
			w211 = MASK(wq11);
			w212 = MASK(wq12);
			w213 = MASK(wq13);
			w214 = MASK(wq14);
			w215 = MASK(wq15);
			w216 = MASK(wq16);
			w217 = MASK(wq17);
			w218 = MASK(wq18);
			w219 = MASK(wq19);
			w220 = MASK(wq20);
			w221 = MASK(wq21);
			w222 = MASK(wq22);
			w223 = MASK(wq23);
			w224 = MASK(wq24);
			w225 = MASK(wq25);
			w226 = MASK(wq26);
			w227 = MASK(wq27);
			w228 = MASK(wq28);
			w229 = MASK(wq29);
			w230 = MASK(wq30);
			w231 = MASK(wq31);
			w10 = SHLBYTE(wq0);
			w11 = SHLBYTE(wq1);
			w12 = SHLBYTE(wq2);
			w13 = SHLBYTE(wq3);
			w14 = SHLBYTE(wq4);
			w15 = SHLBYTE(wq5);
			w16 = SHLBYTE(wq6);
			w17 = SHLBYTE(wq7);
			w18 = SHLBYTE(wq8);
			w19 = SHLBYTE(wq9);
			w110 = SHLBYTE(wq10);
			w111 = SHLBYTE(wq11);
			w112 = SHLBYTE(wq12);
			w113 = SHLBYTE(wq13);
			w114 = SHLBYTE(wq14);
			w115 = SHLBYTE(wq15);
			w116 = SHLBYTE(wq16);
			w117 = SHLBYTE(wq17);
			w118 = SHLBYTE(wq18);
			w119 = SHLBYTE(wq19);
			w120 = SHLBYTE(wq20);
			w121 = SHLBYTE(wq21);
			w122 = SHLBYTE(wq22);
			w123 = SHLBYTE(wq23);
			w124 = SHLBYTE(wq24);
			w125 = SHLBYTE(wq25);
			w126 = SHLBYTE(wq26);
			w127 = SHLBYTE(wq27);
			w128 = SHLBYTE(wq28);
			w129 = SHLBYTE(wq29);
			w130 = SHLBYTE(wq30);
			w131 = SHLBYTE(wq31);
			w20 &= NBYTES(0x1d);
			w21 &= NBYTES(0x1d);
			w22 &= NBYTES(0x1d);
			w23 &= NBYTES(0x1d);
			w24 &= NBYTES(0x1d);
			w25 &= NBYTES(0x1d);
			w26 &= NBYTES(0x1d);
			w27 &= NBYTES(0x1d);
			w28 &= NBYTES(0x1d);
			w29 &= NBYTES(0x1d);
			w210 &= NBYTES(0x1d);
			w211 &= NBYTES(0x1d);
			w212 &= NBYTES(0x1d);
			w213 &= NBYTES(0x1d);
			w214 &= NBYTES(0x1d);
			w215 &= NBYTES(0x1d);
			w216 &= NBYTES(0x1d);
			w217 &= NBYTES(0x1d);
			w218 &= NBYTES(0x1d);
			w219 &= NBYTES(0x1d);
			w220 &= NBYTES(0x1d);
			w221 &= NBYTES(0x1d);
			w222 &= NBYTES(0x1d);
			w223 &= NBYTES(0x1d);
			w224 &= NBYTES(0x1d);
			w225 &= NBYTES(0x1d);
			w226 &= NBYTES(0x1d);
			w227 &= NBYTES(0x1d);
			w228 &= NBYTES(0x1d);
			w229 &= NBYTES(0x1d);
			w230 &= NBYTES(0x1d);
			w231 &= NBYTES(0x1d);
			wq0 = w10 ^ w20;
			wq1 = w11 ^ w21;
			wq2 = w12 ^ w22;
			wq3 = w13 ^ w23;
			wq4 = w14 ^ w24;
			wq5 = w15 ^ w25;
			wq6 = w16 ^ w26;
			wq7 = w17 ^ w27;
			wq8 = w18 ^ w28;
			wq9 = w19 ^ w29;
			wq10 = w110 ^ w210;
			wq11 = w111 ^ w211;
			wq12 = w112 ^ w212;
			wq13 = w113 ^ w213;
			wq14 = w114 ^ w214;
			wq15 = w115 ^ w215;
			wq16 = w116 ^ w216;
			wq17 = w117 ^ w217;
			wq18 = w118 ^ w218;
			wq19 = w119 ^ w219;
			wq20 = w120 ^ w220;
			wq21 = w121 ^ w221;
			wq22 = w122 ^ w222;
			wq23 = w123 ^ w223;
			wq24 = w124 ^ w224;
			wq25 = w125 ^ w225;
			wq26 = w126 ^ w226;
			wq27 = w127 ^ w227;
			wq28 = w128 ^ w228;
			wq29 = w129 ^ w229;
			wq30 = w130 ^ w230;
			wq31 = w131 ^ w231;
		}
		*(unative_t *)&p[d+NSIZE*0] ^= wp0;
		*(unative_t *)&p[d+NSIZE*1] ^= wp1;
		*(unative_t *)&p[d+NSIZE*2] ^= wp2;
		*(unative_t *)&p[d+NSIZE*3] ^= wp3;
		*(unative_t *)&p[d+NSIZE*4] ^= wp4;
		*(unative_t *)&p[d+NSIZE*5] ^= wp5;
		*(unative_t *)&p[d+NSIZE*6] ^= wp6;
		*(unative_t *)&p[d+NSIZE*7] ^= wp7;
		*(unative_t *)&p[d+NSIZE*8] ^= wp8;
		*(unative_t *)&p[d+NSIZE*9] ^= wp9;
		*(unative_t *)&p[d+NSIZE*10] ^= wp10;
		*(unative_t *)&p[d+NSIZE*11] ^= wp11;
		*(unative_t *)&p[d+NSIZE*12] ^= wp12;
		*(unative_t *)&p[d+NSIZE*13] ^= wp13;
		*(unative_t *)&p[d+NSIZE*14] ^= wp14;
		*(unative_t *)&p[d+NSIZE*15] ^= wp15;
		*(unative_t *)&p[d+NSIZE*16] ^= wp16;
		*(unative_t *)&p[d+NSIZE*17] ^= wp17;
		*(unative_t *)&p[d+NSIZE*18] ^= wp18;
		*(unative_t *)&p[d+NSIZE*19] ^= wp19;
		*(unative_t *)&p[d+NSIZE*20] ^= wp20;
		*(unative_t *)&p[d+NSIZE*21] ^= wp21;
		*(unative_t *)&p[d+NSIZE*22] ^= wp22;
		*(unative_t *)&p[d+NSIZE*23] ^= wp23;
		*(unative_t *)&p[d+NSIZE*24] ^= wp24;
		*(unative_t *)&p[d+NSIZE*25] ^= wp25;
		*(unative_t *)&p[d+NSIZE*26] ^= wp26;
		*(unative_t *)&p[d+NSIZE*27] ^= wp27;
		*(unative_t *)&p[d+NSIZE*28] ^= wp28;
		*(unative_t *)&p[d+NSIZE*29] ^= wp29;
		*(unative_t *)&p[d+NSIZE*30] ^= wp30;
		*(unative_t *)&p[d+NSIZE*31] ^= wp31;
		*(unative_t *)&q[d+NSIZE*0] ^= wq0;
		*(unative_t *)&q[d+NSIZE*1] ^= wq1;
		*(unative_t *)&q[d+NSIZE*2] ^= wq2;
		*(unative_t *)&q[d+NSIZE*3] ^= wq3;
		*(unative_t *)&q[d+NSIZE*4] ^= wq4;
		*(unative_t *)&q[d+NSIZE*5] ^= wq5;
		*(unative_t *)&q[d+NSIZE*6] ^= wq6;
		*(unative_t *)&q[d+NSIZE*7] ^= wq7;
		*(unative_t *)&q[d+NSIZE*8] ^= wq8;
		*(unative_t *)&q[d+NSIZE*9] ^= wq9;
		*(unative_t *)&q[d+NSIZE*10] ^= wq10;
		*(unative_t *)&q[d+NSIZE*11] ^= wq11;
		*(unative_t *)&q[d+NSIZE*12] ^= wq12;
		*(unative_t *)&q[d+NSIZE*13] ^= wq13;
		*(unative_t *)&q[d+NSIZE*14] ^= wq14;
		*(unative_t *)&q[d+NSIZE*15] ^= wq15;
		*(unative_t *)&q[d+NSIZE*16] ^= wq16;
		*(unative_t *)&q[d+NSIZE*17] ^= wq17;
		*(unative_t *)&q[d+NSIZE*18] ^= wq18;
		*(unative_t *)&q[d+NSIZE*19] ^= wq19;
		*(unative_t *)&q[d+NSIZE*20] ^= wq20;
		*(unative_t *)&q[d+NSIZE*21] ^= wq21;
		*(unative_t *)&q[d+NSIZE*22] ^= wq22;
		*(unative_t *)&q[d+NSIZE*23] ^= wq23;
		*(unative_t *)&q[d+NSIZE*24] ^= wq24;
		*(unative_t *)&q[d+NSIZE*25] ^= wq25;
		*(unative_t *)&q[d+NSIZE*26] ^= wq26;
		*(unative_t *)&q[d+NSIZE*27] ^= wq27;
		*(unative_t *)&q[d+NSIZE*28] ^= wq28;
		*(unative_t *)&q[d+NSIZE*29] ^= wq29;
		*(unative_t *)&q[d+NSIZE*30] ^= wq30;
		*(unative_t *)&q[d+NSIZE*31] ^= wq31;
	}

}

const struct raid6_calls raid6_intx32 = {
	raid6_int32_gen_syndrome,
	raid6_int32_xor_syndrome,
	NULL,			/* always valid */
	"int" NSTRING "x32",
	0
};

#endif
