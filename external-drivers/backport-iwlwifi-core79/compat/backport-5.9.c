// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/math64.h>

u64 mul_u64_u64_div_u64(u64 a, u64 b, u64 c)
{
	u64 res = 0, div, rem;
	int shift;

	/* can a * b overflow ? */
	if (ilog2(a) + ilog2(b) > 62) {
		/*
		 * (b * a) / c is equal to
		 *
		 *      (b / c) * a +
		 *      (b % c) * a / c
		 *
		 * if nothing overflows. Can the 1st multiplication
		 * overflow? Yes, but we do not care: this can only
		 * happen if the end result can't fit in u64 anyway.
		 *
		 * So the code below does
		 *
		 *      res = (b / c) * a;
		 *      b = b % c;
		 */
		div = div64_u64_rem(b, c, &rem);
		res = div * a;
		b = rem;

		shift = ilog2(a) + ilog2(b) - 62;
		if (shift > 0) {
			/* drop precision */
			b >>= shift;
			c >>= shift;
			if (!c)
				return res;
		}
	}

	return res + div64_u64(a * b, c);
}
EXPORT_SYMBOL_GPL(mul_u64_u64_div_u64);
