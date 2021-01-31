/*
*************************************************************************
* Ralink Technology Corporation
* 5F., No. 5, Taiyuan 1st St., Jhubei City,
* Hsinchu County 302,
* Taiwan, R.O.C.
*
* (c) Copyright 2012, Ralink Technology Corporation
*
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the                         *
* Free Software Foundation, Inc.,                                       *
* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
*                                                                       *
*************************************************************************/

#ifndef __RTBT_TYPE_H
#define __RTBT_TYPE_H

#ifdef __GNUC__
#include <stdarg.h>
#endif // __GNUC__ //

typedef signed char		INT8;
typedef unsigned char		UINT8;
typedef signed short		INT16;
typedef unsigned short		UINT16;
typedef signed int			INT32;
typedef unsigned int		UINT32;
typedef signed long long	INT64;
typedef unsigned long long	UINT64;

typedef int				INT;
typedef unsigned int		UINT;
typedef unsigned char		BOOLEAN;
typedef long				LONG;
typedef unsigned long		ULONG;
typedef unsigned long		*PULONG;

typedef signed long long	DOUBLE;
typedef long long			LONGLONG;

typedef void			VOID;
typedef void 			*PVOID;

typedef char CHAR;
typedef char *PCHAR;
typedef unsigned char UCHAR;
typedef unsigned char *PUCHAR;
typedef char *PSTRING;

typedef unsigned short USHORT;
typedef unsigned char *PUINT8;
typedef unsigned short *PUINT16;
typedef unsigned int *PUINT32;
typedef void *TYPE;

typedef unsigned long PHYSICAL_ADDRESS;



#ifdef TRUE
#undef TRUE
#endif
#define TRUE		1

#ifdef FALSE
#undef FALSE
#endif
#define FALSE		0

#ifdef IN
#undef IN
#endif
#define IN

#ifdef __in
#undef
#endif
#define __in

#ifdef OUT
#undef OUT
#endif
#define OUT

#ifdef INTOUT
#undef INOUT
#endif
#define INOUT

#ifdef __GNUC__
#ifdef FORCEINLINE
#undef FORCEINLINE
#endif
#define FORCEINLINE 	static inline __attribute__((always_inline))
#endif // __GNUC__ //

#ifndef NULL
#define NULL (void *) 0
#endif


/* A structure including bit-fields will be covered with a union syntax. An
 * unsigned integer of equal-length will be allocated to a mutual space of this
 * bit-fields structure. The method is used widely, particularly defining the
 * structure of the hardware registers. But the restriction of this method is
 * the need of regular data size. Therefore, only 8,16 and 32-bits are preferred
 * and accepted. The unsigned integer allocated to the mutual space of structure
 * will be named Overall8(16 or 32). */
#define Overall32(_a_union_)        ((_a_union_).word)
#define Overall16(_a_union_)        ((_a_union_).Overall16)
#define Overall8(_a_union_)         ((_a_union_).Overall8)



/*
	Work-around for windows definitions
*/
typedef int NTSTATUS;

#define STATUS_SUCCESS 						0
#define STATUS_FAILURE 						-1
#define STATUS_UNSUCCESSFUL 				-1
#define STATUS_INSUFFICIENT_RESOURCES 	-2
#define STATUS_TIMEOUT						-3
#define STATUS_CANCELLED					-4
#define STATUS_INVALID_PARAMETER 			-5
#define STATUS_INVALID_DEVICE_STATE 		-6


/*
	Following code used to work-around to make compiler fun
*/
// TODO: Following code used for windows compatible
typedef int NDIS_STATUS;
typedef int HANDLE;


/*
	Following definitions are used to wrap for compile fun
*/
#ifdef __GNUC__
#define GNUC_PACKED    __attribute__ ((packed))
#else
#define GNUC_PACKED
#endif

#ifdef __arm
#define ARM_PACKED    __packed
#else
#define ARM_PACKED
#endif

typedef union _LARGE_INTEGER {
	struct {
#ifdef RT_BIG_ENDIAN
		INT32 HighPart;
		UINT LowPart;
#else
		UINT LowPart;
		INT32 HighPart;
#endif
	} u;
	/*INT64 QuadPart; */
	ULONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct  _REG_PAIR
{
	UCHAR   Register;
	UCHAR   Value;
	UCHAR   EEPROM;		//The EEPROM addr for updaing the registers
} REG_PAIR, *PREG_PAIR;


#define UNREFERENCED_PARAMETER(_p) (void)(_p)
#define FIELD_OFFSET(_type, _field)	((ULONG)&((_type *)0)->_field)

typedef struct _LIST_ENTRY {
  struct _LIST_ENTRY *Flink;
  struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;


#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(_ptr, _type, _field)	\
	((_type *)( (PCHAR)(_ptr) - (ULONG)(&((_type*)0)->_field)))
#endif

#define RTL_SOFT_ASSERT(expr)	
#if DBG
#ifdef ASSERT_BREAK
#define RTBT_ASSERT(expr)                                     \
    if(!(expr))                                          \
    {                                                    \
        DebugPrint(FATAL, DBG_MISC, "ASSERT failed at line %d in %s\n"      \
                  "\"" #expr "\"\n", __LINE__, __FILE__);\
    }
#else
#define RTBT_ASSERT(expr)       RTL_SOFT_ASSERT(expr)
#endif /* ASSERT_BREAK */

#else

#define RTBT_ASSERT(_X)	do{}while(0)
#endif /* DBG */

#define ASSERT					RTBT_ASSERT

#endif // __RTBT_TYPE_H //

