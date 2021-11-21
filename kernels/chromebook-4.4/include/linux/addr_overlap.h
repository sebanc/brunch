/*
 * Functions for checking if address ranges overlap.
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ADDR_OVERLAP_H
#define _ADDR_OVERLAP_H

/**
 * Tell whether the two regions of physical addresses overlap at all.
 *
 * If there's any overlap between the region a..a+a_size and
 * b..b+b_size then we'll return true.
 *
 * @a: Start of region 1.
 * @a_size: Size of region 1 in bytes.
 * @b: Start of region 2.
 * @b_size: Size of region 2 in bytes.
 * @return: True if the regions overlap; false otherwise.
*/

static inline bool phys_addrs_overlap(phys_addr_t a, size_t a_size,
				      phys_addr_t b, size_t b_size)
{
	if ((b + b_size) <= a)
		return false;

	if (b >= (a + a_size))
		return false;

	return true;
}

/**
 * Tell whether the two regions of virtual addresses overlap at all.
 *
 * If there's any overlap between the region a..a+a_size and
 * b..b+b_size then we'll return true.
 *
 * @a: Start of region 1.
 * @a_size: Size of region 1 in bytes.
 * @b: Start of region 2.
 * @b_size: Size of region 2 in bytes.
 * @return: True if the regions overlap; false otherwise.
*/

static inline bool virt_addrs_overlap(void *a, size_t a_size,
				      void *b, size_t b_size)
{
	if ((b + b_size) <= a)
		return false;

	if (b >= (a + a_size))
		return false;

	return true;
}

#endif /* _ADDR_OVERLAP_H */
