/*
 * Linux OS Independent Layer
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
 * $Id: linux_osl.h 383331 2013-02-06 10:27:24Z $
 */

#ifndef _linux_osl_h_
#define _linux_osl_h_

#include <typedefs.h>

extern void * osl_os_open_image(char * filename);
extern int osl_os_get_image_block(char * buf, int len, void * image);
extern void osl_os_close_image(void * image);
extern int osl_os_image_size(void *image);

extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag);
extern void osl_detach(osl_t *osh);

extern uint32 g_assert_type;

#if defined(BCMDBG_ASSERT)
	#define ASSERT(exp) \
	  do { if (!(exp)) osl_assert(#exp, __FILE__, __LINE__); } while (0)
extern void osl_assert(const char *exp, const char *file, int line);
#else
	#ifdef __GNUC__
		#define GCC_VERSION \
			(__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
		#if GCC_VERSION > 30100
			#define ASSERT(exp)	do {} while (0)
		#else

			#define ASSERT(exp)
		#endif 
	#endif 
#endif 

#define	OSL_DELAY(usec)		osl_delay(usec)
extern void osl_delay(uint usec);

#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size) \
	osl_pcmcia_read_attr((osh), (offset), (buf), (size))
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) \
	osl_pcmcia_write_attr((osh), (offset), (buf), (size))
extern void osl_pcmcia_read_attr(osl_t *osh, uint offset, void *buf, int size);
extern void osl_pcmcia_write_attr(osl_t *osh, uint offset, void *buf, int size);

#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
	osl_pci_read_config((osh), (offset), (size))
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
	osl_pci_write_config((osh), (offset), (size), (val))
extern uint32 osl_pci_read_config(osl_t *osh, uint offset, uint size);
extern void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val);

#define OSL_PCI_BUS(osh)	osl_pci_bus(osh)
#define OSL_PCI_SLOT(osh)	osl_pci_slot(osh)
extern uint osl_pci_bus(osl_t *osh);
extern uint osl_pci_slot(osl_t *osh);
extern struct pci_dev *osl_pci_device(osl_t *osh);

typedef struct {
	bool pkttag;
	bool mmbus;		
	pktfree_cb_fn_t tx_fn;  
	void *tx_ctx;		
	void	*unused[3];
} osl_pubinfo_t;

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx)		\
	do {						\
	   ((osl_pubinfo_t*)osh)->tx_fn = _tx_fn;	\
	   ((osl_pubinfo_t*)osh)->tx_ctx = _tx_ctx;	\
	} while (0)

#define BUS_SWAP32(v)		(v)

#ifdef BCMDBG_MEM
	#define MALLOC(osh, size)	osl_debug_malloc((osh), (size), __LINE__, __FILE__)
	#define MFREE(osh, addr, size)	osl_debug_mfree((osh), (addr), (size), __LINE__, __FILE__)
	#define MALLOCED(osh)		osl_malloced((osh))
	#define MALLOC_DUMP(osh, b) 	osl_debug_memdump((osh), (b))
	extern void *osl_debug_malloc(osl_t *osh, uint size, int line, const char* file);
	extern void osl_debug_mfree(osl_t *osh, void *addr, uint size, int line, const char* file);
	extern uint osl_malloced(osl_t *osh);
	struct bcmstrbuf;
	extern int osl_debug_memdump(osl_t *osh, struct bcmstrbuf *b);
#else
	#define MALLOC(osh, size)	osl_malloc((osh), (size))
	#define MFREE(osh, addr, size)	osl_mfree((osh), (addr), (size))
	#define MALLOCED(osh)		osl_malloced((osh))
	extern void *osl_malloc(osl_t *osh, uint size);
	extern void osl_mfree(osl_t *osh, void *addr, uint size);
	extern uint osl_malloced(osl_t *osh);
#endif 

#define NATIVE_MALLOC(osh, size)		kmalloc(size, GFP_ATOMIC)
#define NATIVE_MFREE(osh, addr, size)	kfree(addr)

#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))
extern uint osl_malloc_failed(osl_t *osh);

#define	DMA_CONSISTENT_ALIGN	osl_dma_consistent_align()
#define	DMA_ALLOC_CONSISTENT(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

#define	DMA_ALLOC_CONSISTENT_FORCE32(osh, size, align, tot, pap, dmah) \
	osl_dma_alloc_consistent((osh), (size), (align), (tot), (pap))
#define	DMA_FREE_CONSISTENT_FORCE32(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))

extern uint osl_dma_consistent_align(void);
extern void *osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align, uint *tot, ulong *pap);
extern void osl_dma_free_consistent(osl_t *osh, void *va, uint size, ulong pa);

#define	DMA_TX	1	
#define	DMA_RX	2	

#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	osl_dma_unmap((osh), (pa), (size), (direction))
extern uint osl_dma_map(osl_t *osh, void *va, uint size, int direction, void *p,
	hnddma_seg_map_t *txp_dmah);
extern void osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction);

#define OSL_DMADDRWIDTH(osh, addrwidth) do {} while (0)

	#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) mmap_op
	#define SELECT_BUS_READ(osh, mmap_op, bus_op) mmap_op

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int bcmerror);

#define	PKTBUFSZ	2048   

#if defined(LINUX_PORT)
	#define	printf(fmt, args...)	printk(fmt , ## args)
	#include <linux/kernel.h>	
	#include <linux/string.h>	

	#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
	#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
	#define	bzero(b, len)		memset((b), '\0', (len))

	extern int osl_printf(const char *format, ...);
	extern int osl_sprintf(char *buf, const char *format, ...);
	extern int osl_snprintf(char *buf, size_t n, const char *format, ...);
	extern int osl_vsprintf(char *buf, const char *format, va_list ap);
	extern int osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap);
	extern int osl_strcmp(const char *s1, const char *s2);
	extern int osl_strncmp(const char *s1, const char *s2, uint n);
	extern int osl_strlen(const char *s);
	extern char* osl_strcpy(char *d, const char *s);
	extern char* osl_strncpy(char *d, const char *s, uint n);
	extern char* osl_strchr(const char *s, int c);
	extern char* osl_strrchr(const char *s, int c);
	extern void *osl_memset(void *d, int c, size_t n);
	extern void *osl_memcpy(void *d, const void *s, size_t n);
	extern void *osl_memmove(void *d, const void *s, size_t n);
	extern int osl_memcmp(const void *s1, const void *s2, size_t n);
#else

	#include <stdarg.h>			
	#include <stddef.h>			
	#undef printf
	#undef sprintf
	#undef snprintf
	#undef vsprintf
	#undef vsnprintf
	#define	printf(fmt, args...)		osl_printf((fmt) , ## args)
	#define sprintf(buf, fmt, args...)	osl_sprintf((buf), (fmt) , ## args)
	#define snprintf(buf, n, fmt, args...)	osl_snprintf((buf), (n), (fmt) , ## args)
	#define vsprintf(buf, fmt, ap)		osl_vsprintf((buf), (fmt), (ap))
	#define vsnprintf(buf, n, fmt, ap)	osl_vsnprintf((buf), (n), (fmt), (ap))
	extern int osl_printf(const char *format, ...);
	extern int osl_sprintf(char *buf, const char *format, ...);
	extern int osl_snprintf(char *buf, size_t n, const char *format, ...);
	extern int osl_vsprintf(char *buf, const char *format, va_list ap);
	extern int osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap);

	#undef strcmp
	#undef strncmp
	#undef strlen
	#undef strcpy
	#undef strncpy
	#undef strchr
	#undef strrchr
	#define	strcmp(s1, s2)			osl_strcmp((s1), (s2))
	#define	strncmp(s1, s2, n)		osl_strncmp((s1), (s2), (n))
	#define strlen(s)			osl_strlen((s))
	#define	strcpy(d, s)			osl_strcpy((d), (s))
	#define	strncpy(d, s, n)		osl_strncpy((d), (s), (n))
	#define	strchr(s, c)			osl_strchr((s), (c))
	#define	strrchr(s, c)			osl_strrchr((s), (c))
	extern int osl_strcmp(const char *s1, const char *s2);
	extern int osl_strncmp(const char *s1, const char *s2, uint n);
	extern int osl_strlen(const char *s);
	extern char* osl_strcpy(char *d, const char *s);
	extern char* osl_strncpy(char *d, const char *s, uint n);
	extern char* osl_strchr(const char *s, int c);
	extern char* osl_strrchr(const char *s, int c);

	#undef memset
	#undef memcpy
	#undef memcmp
	#define	memset(d, c, n)		osl_memset((d), (c), (n))
	#define	memcpy(d, s, n)		osl_memcpy((d), (s), (n))
	#define	memmove(d, s, n)	osl_memmove((d), (s), (n))
	#define	memcmp(s1, s2, n)	osl_memcmp((s1), (s2), (n))
	extern void *osl_memset(void *d, int c, size_t n);
	extern void *osl_memcpy(void *d, const void *s, size_t n);
	extern void *osl_memmove(void *d, const void *s, size_t n);
	extern int osl_memcmp(const void *s1, const void *s2, size_t n);

	#undef bcopy
	#undef bcmp
	#undef bzero
	#define	bcopy(src, dst, len)	osl_memcpy((dst), (src), (len))
	#define	bcmp(b1, b2, len)	osl_memcmp((b1), (b2), (len))
	#define	bzero(b, len)		osl_memset((b), '\0', (len))
#endif 

#define R_REG(osh, r) (\
	sizeof(*(r)) == sizeof(uint8) ? osl_readb((volatile uint8*)(r)) : \
	sizeof(*(r)) == sizeof(uint16) ? osl_readw((volatile uint16*)(r)) : \
	osl_readl((volatile uint32*)(r)) \
)
#define W_REG(osh, r, v) do { \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	osl_writeb((uint8)(v), (volatile uint8*)(r)); break; \
	case sizeof(uint16):	osl_writew((uint16)(v), (volatile uint16*)(r)); break; \
	case sizeof(uint32):	osl_writel((uint32)(v), (volatile uint32*)(r)); break; \
	} \
} while (0)

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))
extern uint8 osl_readb(volatile uint8 *r);
extern uint16 osl_readw(volatile uint16 *r);
extern uint32 osl_readl(volatile uint32 *r);
extern void osl_writeb(uint8 v, volatile uint8 *r);
extern void osl_writew(uint16 v, volatile uint16 *r);
extern void osl_writel(uint32 v, volatile uint32 *r);

#define OSL_SYSUPTIME()		osl_sysuptime()
extern uint32 osl_sysuptime(void);

#define OSL_UNCACHED(va)	osl_uncached((va))
extern void *osl_uncached(void *va);
#define OSL_CACHED(va)		osl_cached((va))
extern void *osl_cached(void *va);

#define OSL_PREF_RANGE_LD(va, sz)
#define OSL_PREF_RANGE_ST(va, sz)

#define OSL_GETCYCLES(x)	((x) = osl_getcycles())
extern uint osl_getcycles(void);

#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

#define	REG_MAP(pa, size)	osl_reg_map((pa), (size))
#define	REG_UNMAP(va)		osl_reg_unmap((va))
extern void *osl_reg_map(uint32 pa, uint size);
extern void osl_reg_unmap(void *va);

#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	BZERO_SM(r, len)	bzero((r), (len))

#define	PKTGET(osh, len, send)		osl_pktget((osh), (len))
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb))
#define PKTFRMNATIVE(osh, skb)		osl_pkt_frmnative((osh), (skb))
#define PKTLIST_DUMP(osh, buf)
#define PKTDBG_TRACE(osh, pkt, bit)
#define	PKTFREE(osh, skb, send)		osl_pktfree((osh), (skb), (send))
#define	PKTDATA(osh, skb)		osl_pktdata((osh), (skb))
#define	PKTLEN(osh, skb)		osl_pktlen((osh), (skb))
#define PKTHEADROOM(osh, skb)		osl_pktheadroom((osh), (skb))
#define PKTTAILROOM(osh, skb)		osl_pkttailroom((osh), (skb))
#define	PKTNEXT(osh, skb)		osl_pktnext((osh), (skb))
#define	PKTSETNEXT(osh, skb, x)		osl_pktsetnext((skb), (x))
#define	PKTSETLEN(osh, skb, len)	osl_pktsetlen((osh), (skb), (len))
#define	PKTPUSH(osh, skb, bytes)	osl_pktpush((osh), (skb), (bytes))
#define	PKTPULL(osh, skb, bytes)	osl_pktpull((osh), (skb), (bytes))
#define PKTTAG(skb)			osl_pkttag((skb))
#define PKTTONATIVE(osh, pkt)		osl_pkt_tonative((osh), (pkt))
#define	PKTLINK(skb)			osl_pktlink((skb))
#define	PKTSETLINK(skb, x)		osl_pktsetlink((skb), (x))
#define	PKTPRIO(skb)			osl_pktprio((skb))
#define	PKTSETPRIO(skb, x)		osl_pktsetprio((skb), (x))
#define PKTSHARED(skb)                  osl_pktshared((skb))
#define PKTSETPOOL(osh, skb, x, y)	do {} while (0)
#define PKTPOOL(osh, skb)		FALSE

extern void *osl_pktget(osl_t *osh, uint len);
extern void *osl_pktdup(osl_t *osh, void *skb);
extern void *osl_pkt_frmnative(osl_t *osh, void *skb);
extern void osl_pktfree(osl_t *osh, void *skb, bool send);
extern uchar *osl_pktdata(osl_t *osh, void *skb);
extern uint osl_pktlen(osl_t *osh, void *skb);
extern uint osl_pktheadroom(osl_t *osh, void *skb);
extern uint osl_pkttailroom(osl_t *osh, void *skb);
extern void *osl_pktnext(osl_t *osh, void *skb);
extern void osl_pktsetnext(void *skb, void *x);
extern void osl_pktsetlen(osl_t *osh, void *skb, uint len);
extern uchar *osl_pktpush(osl_t *osh, void *skb, int bytes);
extern uchar *osl_pktpull(osl_t *osh, void *skb, int bytes);
extern void *osl_pkttag(void *skb);
extern void *osl_pktlink(void *skb);
extern void osl_pktsetlink(void *skb, void *x);
extern uint osl_pktprio(void *skb);
extern void osl_pktsetprio(void *skb, uint x);
extern struct sk_buff *osl_pkt_tonative(osl_t *osh, void *pkt);
extern bool osl_pktshared(void *skb);

#define PKTALLOCED(osh)		osl_pktalloced(osh)
extern uint osl_pktalloced(osl_t *osh);

#define	DMA_MAP(osh, va, size, direction, p, dmah) \
	osl_dma_map((osh), (va), (size), (direction), (p), (dmah))

#endif	
