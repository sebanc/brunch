/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __BASIC_TYPES_H__
#define __BASIC_TYPES_H__


#define SUCCESS	0
#define FAIL	(-1)

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/utsname.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19))
enum {
	false	= 0,
	true	= 1
};
#endif

typedef void (*proc_t)(void *);

#define FIELD_OFFSET(s, field)	((__kernel_ssize_t)&((s *)(0))->field)

#define MEM_ALIGNMENT_OFFSET	(sizeof (__kernel_size_t))
#define MEM_ALIGNMENT_PADDING	(sizeof(__kernel_size_t) - 1)

#define SIZE_PTR __kernel_size_t
#define SSIZE_PTR __kernel_ssize_t

/*
* Continuous bits starting from least significant bit
* Example:
* BIT_LEN_MASK_32(0) => 0x00000000
* BIT_LEN_MASK_32(1) => 0x00000001
* BIT_LEN_MASK_32(2) => 0x00000003
* BIT_LEN_MASK_32(32) => 0xFFFFFFFF
*/
#define BIT_LEN_MASK_32(__BitLen) ((u32)(0xFFFFFFFF >> (32 - (__BitLen))))
#define BIT_LEN_MASK_16(__BitLen) ((u16)(0xFFFF >> (16 - (__BitLen))))
#define BIT_LEN_MASK_8(__BitLen) ((0x0FF >> (8 - (__BitLen))))

/*
* Continuous bits starting from least significant bit
* Example:
* BIT_OFFSET_LEN_MASK_32(0, 2) => 0x00000003
* BIT_OFFSET_LEN_MASK_32(16, 2) => 0x00030000
*/
#define BIT_OFFSET_LEN_MASK_32(__BitOffset, __BitLen) ((u32)(BIT_LEN_MASK_32(__BitLen) << (__BitOffset)))
#define BIT_OFFSET_LEN_MASK_16(__BitOffset, __BitLen) ((u16)(BIT_LEN_MASK_16(__BitLen) << (__BitOffset)))
#define BIT_OFFSET_LEN_MASK_8(__BitOffset, __BitLen) ((u8)(BIT_LEN_MASK_8(__BitLen) << (__BitOffset)))

/*
* Convert LE data to host byte order
*/
#define EF1Byte (u8)
#define EF2Byte le16_to_cpu
#define EF4Byte le32_to_cpu

/*
* Read LE data from memory to host byte order
*/
#define ReadLE4Byte(_ptr)	le32_to_cpu(*((__le32 *)(_ptr)))
#define ReadLE2Byte(_ptr)	le16_to_cpu(*((__le16 *)(_ptr)))
#define ReadLE1Byte(_ptr)	(*((u8 *)(_ptr)))

/*
* Read BE data from memory to host byte order
*/
#define ReadBEE4Byte(_ptr)	be32_to_cpu(*((__be32 *)(_ptr)))
#define ReadBE2Byte(_ptr)	be16_to_cpu(*((__be16 *)(_ptr)))
#define ReadBE1Byte(_ptr)	(*((u8 *)(_ptr)))

/*
* Write host byte order data to memory in LE order
*/
#define WriteLE4Byte(_ptr, _val)	((*((__le32 *)(_ptr))) = cpu_to_le32(_val))
#define WriteLE2Byte(_ptr, _val)	((*((__le16 *)(_ptr))) = cpu_to_le16(_val))
#define WriteLE1Byte(_ptr, _val)	((*((u8 *)(_ptr))) = ((u8)(_val)))

/*
* Write host byte order data to memory in BE order
*/
#define WriteBE4Byte(_ptr, _val)	((*((__be32 *)(_ptr))) = cpu_to_be32(_val))
#define WriteBE2Byte(_ptr, _val)	((*((__be16 *)(_ptr))) = cpu_to_be16(_val))
#define WriteBE1Byte(_ptr, _val)	((*((u8 *)(_ptr))) = ((u8)(_val)))

/*
* Return 4-byte value in host byte ordering from 4-byte pointer in little-endian system.
*/
#define LE_P4BYTE_TO_HOST_4BYTE(__pStart) (le32_to_cpu(*((__le32 *)(__pStart))))
#define LE_P2BYTE_TO_HOST_2BYTE(__pStart) (le16_to_cpu(*((__le16 *)(__pStart))))
#define LE_P1BYTE_TO_HOST_1BYTE(__pStart) ((*((u8 *)(__pStart))))

/*
* Return 4-byte value in host byte ordering from 4-byte pointer in big-endian system.
*/
#define BE_P4BYTE_TO_HOST_4BYTE(__pStart) (be32_to_cpu(*((__be32 *)(__pStart))))
#define BE_P2BYTE_TO_HOST_2BYTE(__pStart) (be16_to_cpu(*((__be16 *)(__pStart))))
#define BE_P1BYTE_TO_HOST_1BYTE(__pStart) ((*((u8 *)(__pStart))))

/*
* Translate subfield (continuous bits in little-endian) of 4-byte value in LE byte to
* 4-byte value in host byte ordering.
*/
#define LE_BITS_TO_4BYTE(__pStart, __BitOffset, __BitLen) \
	((LE_P4BYTE_TO_HOST_4BYTE(__pStart) >> (__BitOffset)) & BIT_LEN_MASK_32(__BitLen))

#define LE_BITS_TO_2BYTE(__pStart, __BitOffset, __BitLen) \
	((LE_P2BYTE_TO_HOST_2BYTE(__pStart) >> (__BitOffset)) & BIT_LEN_MASK_16(__BitLen))

#define LE_BITS_TO_1BYTE(__pStart, __BitOffset, __BitLen) \
	((LE_P1BYTE_TO_HOST_1BYTE(__pStart) >> (__BitOffset)) & BIT_LEN_MASK_8(__BitLen))

/*
* Translate subfield (continuous bits in big-endian) of 4-byte value in BE byte to
* 4-byte value in host byte ordering.
*/
#define BE_BITS_TO_4BYTE(__pStart, __BitOffset, __BitLen) \
	((BE_P4BYTE_TO_HOST_4BYTE(__pStart) >> (__BitOffset)) & BIT_LEN_MASK_32(__BitLen))

#define BE_BITS_TO_2BYTE(__pStart, __BitOffset, __BitLen) \
	((BE_P2BYTE_TO_HOST_2BYTE(__pStart) >> (__BitOffset)) & BIT_LEN_MASK_16(__BitLen))

#define BE_BITS_TO_1BYTE(__pStart, __BitOffset, __BitLen) \
	((BE_P1BYTE_TO_HOST_1BYTE(__pStart) >> (__BitOffset)) & BIT_LEN_MASK_8(__BitLen))

/*
* Mask subfield (continuous bits in little-endian) of 4-byte value in LE byte oredering
* and return the result in 4-byte value in host byte ordering.
*/
#define LE_BITS_CLEARED_TO_4BYTE(__pStart, __BitOffset, __BitLen) \
	(LE_P4BYTE_TO_HOST_4BYTE(__pStart) & (~BIT_OFFSET_LEN_MASK_32(__BitOffset, __BitLen)))

#define LE_BITS_CLEARED_TO_2BYTE(__pStart, __BitOffset, __BitLen) \
	(LE_P2BYTE_TO_HOST_2BYTE(__pStart) & (~BIT_OFFSET_LEN_MASK_16(__BitOffset, __BitLen)))

#define LE_BITS_CLEARED_TO_1BYTE(__pStart, __BitOffset, __BitLen) \
	(LE_P1BYTE_TO_HOST_1BYTE(__pStart) & ((u8)(~BIT_OFFSET_LEN_MASK_8(__BitOffset, __BitLen))))

/*
* Mask subfield (continuous bits in big-endian) of 4-byte value in BE byte oredering
* and return the result in 4-byte value in host byte ordering.
*/
#define BE_BITS_CLEARED_TO_4BYTE(__pStart, __BitOffset, __BitLen) \
	(BE_P4BYTE_TO_HOST_4BYTE(__pStart) & (~BIT_OFFSET_LEN_MASK_32(__BitOffset, __BitLen)))

#define BE_BITS_CLEARED_TO_2BYTE(__pStart, __BitOffset, __BitLen) \
	(BE_P2BYTE_TO_HOST_2BYTE(__pStart) & (~BIT_OFFSET_LEN_MASK_16(__BitOffset, __BitLen)))

#define BE_BITS_CLEARED_TO_1BYTE(__pStart, __BitOffset, __BitLen) \
	(BE_P1BYTE_TO_HOST_1BYTE(__pStart) & (~BIT_OFFSET_LEN_MASK_8(__BitOffset, __BitLen)))

/*
* Set subfield of little-endian 4-byte value to specified value.
*/
#define SET_BITS_TO_LE_4BYTE(__pStart, __BitOffset, __BitLen, __Value) \
	do { \
		if (__BitOffset == 0 && __BitLen == 32) \
			WriteLE4Byte(__pStart, __Value); \
		else { \
			WriteLE4Byte(__pStart, \
				LE_BITS_CLEARED_TO_4BYTE(__pStart, __BitOffset, __BitLen) \
				| \
				((((u32)__Value) & BIT_LEN_MASK_32(__BitLen)) << (__BitOffset)) \
			); \
		} \
	} while (0)

#define SET_BITS_TO_LE_2BYTE(__pStart, __BitOffset, __BitLen, __Value) \
	do { \
		if (__BitOffset == 0 && __BitLen == 16) \
			WriteLE2Byte(__pStart, __Value); \
		else { \
			WriteLE2Byte(__pStart, \
				LE_BITS_CLEARED_TO_2BYTE(__pStart, __BitOffset, __BitLen) \
				| \
				((((u16)__Value) & BIT_LEN_MASK_16(__BitLen)) << (__BitOffset)) \
			); \
		} \
	} while (0)

#define SET_BITS_TO_LE_1BYTE(__pStart, __BitOffset, __BitLen, __Value) \
	do { \
		if (__BitOffset == 0 && __BitLen == 8) \
			WriteLE1Byte(__pStart, __Value); \
		else { \
			WriteLE1Byte(__pStart, \
				LE_BITS_CLEARED_TO_1BYTE(__pStart, __BitOffset, __BitLen) \
				| \
				((((u8)__Value) & BIT_LEN_MASK_8(__BitLen)) << (__BitOffset)) \
			); \
		} \
	} while (0)

/*
* Set subfield of big-endian 4-byte value to specified value.
*/
#define SET_BITS_TO_BE_4BYTE(__pStart, __BitOffset, __BitLen, __Value) \
	do { \
		if (__BitOffset == 0 && __BitLen == 32) \
			WriteBE4Byte(__pStart, __Value); \
		else { \
			WriteBE4Byte(__pStart, \
				BE_BITS_CLEARED_TO_4BYTE(__pStart, __BitOffset, __BitLen) \
				| \
				((((u32)__Value) & BIT_LEN_MASK_32(__BitLen)) << (__BitOffset)) \
			); \
		} \
	} while (0)

#define SET_BITS_TO_BE_2BYTE(__pStart, __BitOffset, __BitLen, __Value) \
	do { \
		if (__BitOffset == 0 && __BitLen == 16) \
			WriteBE2Byte(__pStart, __Value); \
		else { \
			WriteBE2Byte(__pStart, \
				BE_BITS_CLEARED_TO_2BYTE(__pStart, __BitOffset, __BitLen) \
				| \
				((((u16)__Value) & BIT_LEN_MASK_16(__BitLen)) << (__BitOffset)) \
			); \
		} \
	} while (0)

#define SET_BITS_TO_BE_1BYTE(__pStart, __BitOffset, __BitLen, __Value) \
	do { \
		if (__BitOffset == 0 && __BitLen == 8) \
			WriteBE1Byte(__pStart, __Value); \
		else { \
			WriteBE1Byte(__pStart, \
				BE_BITS_CLEARED_TO_1BYTE(__pStart, __BitOffset, __BitLen) \
				| \
				((((u8)__Value) & BIT_LEN_MASK_8(__BitLen)) << (__BitOffset)) \
			); \
		} \
	} while (0)

/* Get the N-bytes aligment offset from the current length */
#define N_BYTE_ALIGMENT(__Value, __Aligment) ((__Aligment == 1) ? (__Value) : (((__Value + __Aligment - 1) / __Aligment) * __Aligment))

#define TEST_FLAG(__Flag, __testFlag)		(((__Flag) & (__testFlag)) != 0)
#define SET_FLAG(__Flag, __setFlag)			((__Flag) |= __setFlag)
#define CLEAR_FLAG(__Flag, __clearFlag)		((__Flag) &= ~(__clearFlag))
#define CLEAR_FLAGS(__Flag)					((__Flag) = 0)
#define TEST_FLAGS(__Flag, __testFlags)		(((__Flag) & (__testFlags)) == (__testFlags))

#endif /* __BASIC_TYPES_H__ */
