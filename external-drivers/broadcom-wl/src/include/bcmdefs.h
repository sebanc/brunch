/*
 * Misc system wide definitions
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmdefs.h 432150 2013-10-25 20:57:18Z $
 */

#ifndef	_bcmdefs_h_
#define	_bcmdefs_h_

#define BCM_REFERENCE(data)	((void)(data))

#ifdef __GNUC__
#define UNUSED_VAR     __attribute__ ((unused))
#else
#define UNUSED_VAR
#endif

#define STATIC_ASSERT(expr) { \
	 \
	typedef enum { _STATIC_ASSERT_NOT_CONSTANT = (expr) } _static_assert_e UNUSED_VAR; \
	 \
	typedef char STATIC_ASSERT_FAIL[(expr) ? 1 : -1] UNUSED_VAR; \
}

#define bcmreclaimed 		0
#define BCMATTACHDATA(_data)	_data
#define BCMATTACHFN(_fn)	_fn
#define BCMPREATTACHDATA(_data)	_data
#define BCMPREATTACHFN(_fn)	_fn
#define BCMINITDATA(_data)	_data
#define BCMINITFN(_fn)		_fn
#define BCMUNINITFN(_fn)	_fn
#define	BCMNMIATTACHFN(_fn)	_fn
#define	BCMNMIATTACHDATA(_data)	_data
#define CONST	const
#if defined(BCM47XX) && defined(__ARM_ARCH_7A__)
#define BCM47XX_CA9
#else
#undef BCM47XX_CA9
#endif
#if defined(BCM47XX_CA9)
#define BCMFASTPATH		__attribute__ ((__section__ (".text.fastpath")))
#define BCMFASTPATH_HOST	__attribute__ ((__section__ (".text.fastpath_host")))
#else
#define BCMFASTPATH
#define BCMFASTPATH_HOST
#endif

#define BCMROMDATA(_data)	_data
#define BCMROMDAT_NAME(_data)	_data
#define BCMROMFN(_fn)		_fn
#define BCMROMFN_NAME(_fn)	_fn
#define STATIC	static
#define BCMROMDAT_ARYSIZ(data)	ARRAYSIZE(data)
#define BCMROMDAT_SIZEOF(data)	sizeof(data)
#define BCMROMDAT_APATCH(data)
#define BCMROMDAT_SPATCH(data)

#define	SI_BUS			0	
#define	PCI_BUS			1	
#define	PCMCIA_BUS		2	
#define SDIO_BUS		3	
#define JTAG_BUS		4	
#define USB_BUS			5	
#define SPI_BUS			6	
#define RPC_BUS			7	

#define BUSTYPE(bus) 	(bus)

#define CHIPTYPE(bus) 	(bus)

#define SPROMBUS	(PCI_BUS)

#define CHIPID(chip)	(chip)

#define CHIPREV(rev)	(rev)

#define DMADDR_MASK_32 0x0		
#define DMADDR_MASK_30 0xc0000000	
#define DMADDR_MASK_0  0xffffffff	

#define	DMADDRWIDTH_30  30 
#define	DMADDRWIDTH_32  32 
#define	DMADDRWIDTH_63  63 
#define	DMADDRWIDTH_64  64 

typedef unsigned long dmaaddr_t;
#define PHYSADDRHI(_pa) (0)
#define PHYSADDRHISET(_pa, _val)
#define PHYSADDRLO(_pa) ((_pa))
#define PHYSADDRLOSET(_pa, _val) \
	do { \
		(_pa) = (_val);			\
	} while (0)

typedef struct  {
	dmaaddr_t addr;
	uint32	  length;
} hnddma_seg_t;

#define MAX_DMA_SEGS 8

typedef struct {
	void *oshdmah; 
	uint origsize; 
	uint nsegs;
	hnddma_seg_t segs[MAX_DMA_SEGS];
} hnddma_seg_map_t;

#define BCMEXTRAHDROOM 204

#define SDALIGN	32

#define BCMDONGLEHDRSZ 12
#define BCMDONGLEPADSZ 16

#define BCMDONGLEOVERHEAD	(BCMDONGLEHDRSZ + BCMDONGLEPADSZ)

#ifdef BCMDBG

#ifndef BCMDBG_ERR
#define BCMDBG_ERR
#endif 

#ifndef BCMDBG_ASSERT
#define BCMDBG_ASSERT
#endif 

#endif 

#if defined(BCMDBG_ASSERT)
#define BCMASSERT_SUPPORT
#endif 

#define BITFIELD_MASK(width) \
		(((unsigned)1 << (width)) - 1)
#define GFIELD(val, field) \
		(((val) >> field ## _S) & field ## _M)
#define SFIELD(val, field, bits) \
		(((val) & (~(field ## _M << field ## _S))) | \
		 ((unsigned)(bits) << field ## _S))

#undef	BCMSPACE
#define bcmspace	FALSE	

#ifndef MAXSZ_NVRAM_VARS
#define	MAXSZ_NVRAM_VARS	4096
#endif

#endif 
