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
 * $Id: linux_osl.c 383331 2013-02-06 10:27:24Z $
 */

#define LINUX_PORT

#include <typedefs.h>
#include <bcmendian.h>
#include <linuxver.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <linux/delay.h>
#include <pcicfg.h>

#include <linux/fs.h>

#define PCI_CFG_RETRY 		10

#define OS_HANDLE_MAGIC		0x1234abcd	
#define BCM_MEM_FILENAME_LEN 	24		

typedef struct bcm_mem_link {
	struct bcm_mem_link *prev;
	struct bcm_mem_link *next;
	uint	size;
	int	line;
	void 	*osh;
	char	file[BCM_MEM_FILENAME_LEN];
} bcm_mem_link_t;

struct osl_info {
	osl_pubinfo_t pub;
	uint magic;
	void *pdev;
	atomic_t malloced;
	atomic_t pktalloced; 	
	uint failed;
	uint bustype;
	bcm_mem_link_t *dbgmem_list;
	spinlock_t dbgmem_lock;
	spinlock_t pktalloc_lock;
};

#define OSL_PKTTAG_CLEAR(p) \
do { \
	struct sk_buff *s = (struct sk_buff *)(p); \
	ASSERT(OSL_PKTTAG_SZ == 32); \
	*(uint32 *)(&s->cb[0]) = 0; *(uint32 *)(&s->cb[4]) = 0; \
	*(uint32 *)(&s->cb[8]) = 0; *(uint32 *)(&s->cb[12]) = 0; \
	*(uint32 *)(&s->cb[16]) = 0; *(uint32 *)(&s->cb[20]) = 0; \
	*(uint32 *)(&s->cb[24]) = 0; *(uint32 *)(&s->cb[28]) = 0; \
} while (0)

uint32 g_assert_type = FALSE;

static int16 linuxbcmerrormap[] =
{	0, 			
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-E2BIG,			
	-E2BIG,			
	-EBUSY, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EFAULT, 		
	-ENOMEM, 		
	-EOPNOTSUPP,		
	-EMSGSIZE,		
	-EINVAL,		
	-EPERM,			
	-ENOMEM, 		
	-EINVAL, 		
	-ERANGE, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL,		
	-EIO,			
	-ENODEV,		
	-EINVAL,		
	-EIO,			
	-EIO,			
	-ENODEV,		
	-EINVAL,		
	-ENODATA,		
	-EINVAL,		

#if BCME_LAST != -43
#error "You need to add a OS error translation in the linuxbcmerrormap \
	for new error code defined in bcmutils.h"
#endif
};

int
osl_error(int bcmerror)
{
	if (bcmerror > 0)
		bcmerror = 0;
	else if (bcmerror < BCME_LAST)
		bcmerror = BCME_ERROR;

	return linuxbcmerrormap[-bcmerror];
}

extern uint8* dhd_os_prealloc(void *osh, int section, int size);

osl_t *
osl_attach(void *pdev, uint bustype, bool pkttag)
{
	osl_t *osh;

	osh = kmalloc(sizeof(osl_t), GFP_ATOMIC);
	ASSERT(osh);

	bzero(osh, sizeof(osl_t));

	ASSERT(ABS(BCME_LAST) == (ARRAYSIZE(linuxbcmerrormap) - 1));

	osh->magic = OS_HANDLE_MAGIC;
	atomic_set(&osh->malloced, 0);
	osh->failed = 0;
	osh->dbgmem_list = NULL;
	spin_lock_init(&(osh->dbgmem_lock));
	osh->pdev = pdev;
	osh->pub.pkttag = pkttag;
	osh->bustype = bustype;

	switch (bustype) {
		case PCI_BUS:
		case SI_BUS:
		case PCMCIA_BUS:
			osh->pub.mmbus = TRUE;
			break;
		case JTAG_BUS:
		case SDIO_BUS:
		case USB_BUS:
		case SPI_BUS:
		case RPC_BUS:
			osh->pub.mmbus = FALSE;
			break;
		default:
			ASSERT(FALSE);
			break;
	}

	spin_lock_init(&(osh->pktalloc_lock));

#ifdef BCMDBG
	if (pkttag) {
		struct sk_buff *skb;
		BCM_REFERENCE(skb);
		ASSERT(OSL_PKTTAG_SZ <= sizeof(skb->cb));
	}
#endif
	return osh;
}

void
osl_detach(osl_t *osh)
{
	if (osh == NULL)
		return;

	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	kfree(osh);
}

static struct sk_buff *osl_alloc_skb(unsigned int len)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
	gfp_t flags = GFP_ATOMIC;
	struct sk_buff *skb;

	skb = __dev_alloc_skb(len, flags);
	return skb;
#else
	return dev_alloc_skb(len);
#endif 
}

struct sk_buff * BCMFASTPATH
osl_pkt_tonative(osl_t *osh, void *pkt)
{
	struct sk_buff *nskb;

	if (osh->pub.pkttag)
		OSL_PKTTAG_CLEAR(pkt);

	for (nskb = (struct sk_buff *)pkt; nskb; nskb = nskb->next) {
		atomic_sub(PKTISCHAINED(nskb) ? PKTCCNT(nskb) : 1, &osh->pktalloced);

	}
	return (struct sk_buff *)pkt;
}

void * BCMFASTPATH
osl_pkt_frmnative(osl_t *osh, void *pkt)
{
	struct sk_buff *nskb;

	if (osh->pub.pkttag)
		OSL_PKTTAG_CLEAR(pkt);

	for (nskb = (struct sk_buff *)pkt; nskb; nskb = nskb->next) {
		atomic_add(PKTISCHAINED(nskb) ? PKTCCNT(nskb) : 1, &osh->pktalloced);

	}
	return (void *)pkt;
}

void * BCMFASTPATH
osl_pktget(osl_t *osh, uint len)
{
	struct sk_buff *skb;

	if ((skb = osl_alloc_skb(len))) {
#ifdef BCMDBG
		skb_put(skb, len);
#else
		skb->tail += len;
		skb->len  += len;
#endif
		skb->priority = 0;

		atomic_inc(&osh->pktalloced);
	}

	return ((void*) skb);
}

void BCMFASTPATH
osl_pktfree(osl_t *osh, void *p, bool send)
{
	struct sk_buff *skb, *nskb;

	skb = (struct sk_buff*) p;

	if (send && osh->pub.tx_fn)
		osh->pub.tx_fn(osh->pub.tx_ctx, p, 0);

	PKTDBG_TRACE(osh, (void *) skb, PKTLIST_PKTFREE);

	while (skb) {
		nskb = skb->next;
		skb->next = NULL;

		{
			if (skb->destructor)

				dev_kfree_skb_any(skb);
			else

				dev_kfree_skb(skb);
		}
		atomic_dec(&osh->pktalloced);
		skb = nskb;
	}
}

uint32
osl_pci_read_config(osl_t *osh, uint offset, uint size)
{
	uint val = 0;
	uint retry = PCI_CFG_RETRY;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	ASSERT(size == 4);

	do {
		pci_read_config_dword(osh->pdev, offset, &val);
		if (val != 0xffffffff)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		printk("PCI CONFIG READ access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry));
#endif 

	return (val);
}

void
osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val)
{
	uint retry = PCI_CFG_RETRY;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	ASSERT(size == 4);

	do {
		pci_write_config_dword(osh->pdev, offset, val);
		if (offset != PCI_BAR0_WIN)
			break;
		if (osl_pci_read_config(osh, offset, size) == val)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		printk("PCI CONFIG WRITE access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry));
#endif 
}

uint
osl_pci_bus(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return ((struct pci_dev *)osh->pdev)->bus->number;
}

uint
osl_pci_slot(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

#if defined(__ARM_ARCH_7A__) && LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn) + 1;
#else
	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn);
#endif
}

struct pci_dev *
osl_pci_device(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return osh->pdev;
}

static void
osl_pcmcia_attr(osl_t *osh, uint offset, char *buf, int size, bool write)
{
}

void
osl_pcmcia_read_attr(osl_t *osh, uint offset, void *buf, int size)
{
	osl_pcmcia_attr(osh, offset, (char *) buf, size, FALSE);
}

void
osl_pcmcia_write_attr(osl_t *osh, uint offset, void *buf, int size)
{
	osl_pcmcia_attr(osh, offset, (char *) buf, size, TRUE);
}

#ifdef BCMDBG_MEM

static
#endif
void *
osl_malloc(osl_t *osh, uint size)
{
	void *addr;

	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

	if ((addr = kmalloc(size, GFP_ATOMIC)) == NULL) {
		if (osh)
			osh->failed++;
		return (NULL);
	}
	if (osh)
		atomic_add(size, &osh->malloced);

	return (addr);
}

#ifdef BCMDBG_MEM

static
#endif
void
osl_mfree(osl_t *osh, void *addr, uint size)
{
	if (osh) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
		atomic_sub(size, &osh->malloced);
	}
	kfree(addr);
}

uint
osl_malloced(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (atomic_read(&osh->malloced));
}

uint
osl_malloc_failed(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (osh->failed);
}

#ifdef BCMDBG_MEM
#define MEMLIST_LOCK(osh, flags)	spin_lock_irqsave(&(osh)->dbgmem_lock, flags)
#define MEMLIST_UNLOCK(osh, flags)	spin_unlock_irqrestore(&(osh)->dbgmem_lock, flags)

void *
osl_debug_malloc(osl_t *osh, uint size, int line, const char* file)
{
	bcm_mem_link_t *p;
	const char* basename;
	unsigned long flags = 0;

	if (!size) {
		printk("%s: allocating zero sized mem at %s line %d\n", __FUNCTION__, file, line);
		ASSERT(0);
	}

	if (osh) {
		MEMLIST_LOCK(osh, flags);
	}
	if ((p = (bcm_mem_link_t*)osl_malloc(osh, sizeof(bcm_mem_link_t) + size)) == NULL) {
		if (osh) {
			MEMLIST_UNLOCK(osh, flags);
		}
		return (NULL);
	}

	p->size = size;
	p->line = line;
	p->osh = (void *)osh;

	basename = strrchr(file, '/');

	if (basename)
		basename++;

	if (!basename)
		basename = file;

	strncpy(p->file, basename, BCM_MEM_FILENAME_LEN);
	p->file[BCM_MEM_FILENAME_LEN - 1] = '\0';

	if (osh) {
		p->prev = NULL;
		p->next = osh->dbgmem_list;
		if (p->next)
			p->next->prev = p;
		osh->dbgmem_list = p;
		MEMLIST_UNLOCK(osh, flags);
	}

	return p + 1;
}

void
osl_debug_mfree(osl_t *osh, void *addr, uint size, int line, const char* file)
{
	bcm_mem_link_t *p = (bcm_mem_link_t *)((int8*)addr - sizeof(bcm_mem_link_t));
	unsigned long flags = 0;

	ASSERT(osh == NULL || osh->magic == OS_HANDLE_MAGIC);

	if (p->size == 0) {
		printk("osl_debug_mfree: double free on addr %p size %d at line %d file %s\n",
			addr, size, line, file);
		ASSERT(p->size);
		return;
	}

	if (p->size != size) {
		printk("%s: dealloca size does not match alloc size\n", __FUNCTION__);
		printk("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file);
		printk("Alloc size %d line %d file %s\n", p->size, p->line, p->file);
		ASSERT(p->size == size);
		return;
	}

	if (p->osh != (void *)osh) {
		printk("osl_debug_mfree: alloc osh %p does not match dealloc osh %p\n",
			p->osh, osh);
		printk("Dealloc addr %p size %d at line %d file %s\n", addr, size, line, file);
		printk("Alloc size %d line %d file %s\n", p->size, p->line, p->file);
		ASSERT(p->osh == (void *)osh);
		return;
	}

	if (osh) {
		MEMLIST_LOCK(osh, flags);
		if (p->prev)
			p->prev->next = p->next;
		if (p->next)
			p->next->prev = p->prev;
		if (osh->dbgmem_list == p)
			osh->dbgmem_list = p->next;
		p->next = p->prev = NULL;
	}
	p->size = 0;

	osl_mfree(osh, p, size + sizeof(bcm_mem_link_t));
	if (osh) {
		MEMLIST_UNLOCK(osh, flags);
	}
}

int
osl_debug_memdump(osl_t *osh, struct bcmstrbuf *b)
{
	bcm_mem_link_t *p;
	unsigned long flags = 0;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	MEMLIST_LOCK(osh, flags);
	if (osh->dbgmem_list) {
		if (b != NULL)
			bcm_bprintf(b, "   Address   Size File:line\n");
		else
			printf("   Address   Size File:line\n");

		for (p = osh->dbgmem_list; p; p = p->next) {
			if (b != NULL)
				bcm_bprintf(b, "%p %6d %s:%d\n", (char*)p + sizeof(bcm_mem_link_t),
					p->size, p->file, p->line);
			else
				printf("%p %6d %s:%d\n", (char*)p + sizeof(bcm_mem_link_t),
					p->size, p->file, p->line);

			if (p == p->next) {
				if (b != NULL)
					bcm_bprintf(b, "WARNING: loop-to-self "
						"p %p p->next %p\n", p, p->next);
				else
					printf("WARNING: loop-to-self "
						"p %p p->next %p\n", p, p->next);

				break;
			}
		}
	}
	MEMLIST_UNLOCK(osh, flags);

	return 0;
}

#endif	

uint
osl_dma_consistent_align(void)
{
	return (PAGE_SIZE);
}

void*
osl_dma_alloc_consistent(osl_t *osh, uint size, uint16 align_bits, uint *alloced, ulong *pap)
{
	void *va;
	uint16 align = (1 << align_bits);
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	if (!ISALIGNED(DMA_CONSISTENT_ALIGN, align))
		size += align;
	*alloced = size;

#ifdef __ARM_ARCH_7A__
	va = kmalloc(size, GFP_ATOMIC | __GFP_ZERO);
	if (va)
		*pap = (ulong)__virt_to_phys(va);
#else
	va = pci_alloc_consistent(osh->pdev, size, (dma_addr_t*)pap);
#endif
	return va;
}

void
osl_dma_free_consistent(osl_t *osh, void *va, uint size, ulong pa)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

#ifdef __ARM_ARCH_7A__
	kfree(va);
#else
	pci_free_consistent(osh->pdev, size, va, (dma_addr_t)pa);
#endif
}

uint BCMFASTPATH
osl_dma_map(osl_t *osh, void *va, uint size, int direction, void *p, hnddma_seg_map_t *dmah)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX)? PCI_DMA_TODEVICE: PCI_DMA_FROMDEVICE;

#if defined(__ARM_ARCH_7A__) && defined(BCMDMASGLISTOSL)
	if (dmah != NULL) {
		int32 nsegs, i, totsegs = 0, totlen = 0;
		struct scatterlist *sg, _sg[MAX_DMA_SEGS * 2];
		struct sk_buff *skb;
		for (skb = (struct sk_buff *)p; skb != NULL; skb = PKTNEXT(osh, skb)) {
			sg = &_sg[totsegs];
			if (skb_is_nonlinear(skb)) {
				nsegs = skb_to_sgvec(skb, sg, 0, PKTLEN(osh, skb));
				ASSERT((nsegs > 0) && (totsegs + nsegs <= MAX_DMA_SEGS));
				pci_map_sg(osh->pdev, sg, nsegs, dir);
			} else {
				nsegs = 1;
				ASSERT(totsegs + nsegs <= MAX_DMA_SEGS);
				sg->page_link = 0;
				sg_set_buf(sg, PKTDATA(osh, skb), PKTLEN(osh, skb));
				pci_map_single(osh->pdev, PKTDATA(osh, skb), PKTLEN(osh, skb), dir);
			}
			totsegs += nsegs;
			totlen += PKTLEN(osh, skb);
		}
		dmah->nsegs = totsegs;
		dmah->origsize = totlen;
		for (i = 0, sg = _sg; i < totsegs; i++, sg++) {
			dmah->segs[i].addr = sg_phys(sg);
			dmah->segs[i].length = sg->length;
		}
		return dmah->segs[0].addr;
	}
#endif 

	return (pci_map_single(osh->pdev, va, size, dir));
}

void BCMFASTPATH
osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX)? PCI_DMA_TODEVICE: PCI_DMA_FROMDEVICE;
	pci_unmap_single(osh->pdev, (uint32)pa, size, dir);
}

#if defined(BCMDBG_ASSERT)
void
osl_assert(const char *exp, const char *file, int line)
{
	char tempbuf[256];
	const char *basename;

	basename = strrchr(file, '/');

	if (basename)
		basename++;

	if (!basename)
		basename = file;

#ifdef BCMDBG_ASSERT
	snprintf(tempbuf, 256, "assertion \"%s\" failed: file \"%s\", line %d\n",
		exp, basename, line);

	if (!in_interrupt() && g_assert_type != 1) {
		const int delay = 3;
		printk("%s", tempbuf);
		printk("panic in %d seconds\n", delay);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(delay * HZ);
	}

	switch (g_assert_type) {
	case 0:
		panic("%s", tempbuf);
		break;
	case 1:
		printk("%s", tempbuf);
		break;
	case 2:
		printk("%s", tempbuf);
		BUG();
		break;
	default:
		break;
	}
#endif 

}
#endif 

void
osl_delay(uint usec)
{
	uint d;

	while (usec > 0) {
		d = MIN(usec, 1000);
		udelay(d);
		usec -= d;
	}
}

void *
osl_pktdup(osl_t *osh, void *skb)
{
	void * p;

	ASSERT(!PKTISCHAINED(skb));

	PKTCTFMAP(osh, skb);

	if ((p = skb_clone((struct sk_buff *)skb, GFP_ATOMIC)) == NULL)
		return NULL;

	PKTSETCLINK(p, NULL);
	PKTCCLRFLAGS(p);
	PKTCSETCNT(p, 1);
	PKTCSETLEN(p, PKTLEN(osh, skb));

	if (osh->pub.pkttag)
		OSL_PKTTAG_CLEAR(p);

	atomic_inc(&osh->pktalloced);
	return (p);
}

uint32
osl_sysuptime(void)
{
	return ((uint32)jiffies * (1000 / HZ));
}

int
osl_printf(const char *format, ...)
{
	va_list args;
	static char printbuf[1024];
	int len;

	va_start(args, format);
	len = vsnprintf(printbuf, 1024, format, args);
	va_end(args);

	if (len > sizeof(printbuf)) {
		printk("osl_printf: buffer overrun\n");
		return (0);
	}

	return (printk("%s", printbuf));
}

int
osl_sprintf(char *buf, const char *format, ...)
{
	va_list args;
	int rc;

	va_start(args, format);
	rc = vsprintf(buf, format, args);
	va_end(args);
	return (rc);
}

int
osl_snprintf(char *buf, size_t n, const char *format, ...)
{
	va_list args;
	int rc;

	va_start(args, format);
	rc = vsnprintf(buf, n, format, args);
	va_end(args);
	return (rc);
}

int
osl_vsprintf(char *buf, const char *format, va_list ap)
{
	return (vsprintf(buf, format, ap));
}

int
osl_vsnprintf(char *buf, size_t n, const char *format, va_list ap)
{
	return (vsnprintf(buf, n, format, ap));
}

int
osl_strcmp(const char *s1, const char *s2)
{
	return (strcmp(s1, s2));
}

int
osl_strncmp(const char *s1, const char *s2, uint n)
{
	return (strncmp(s1, s2, n));
}

int
osl_strlen(const char *s)
{
	return (strlen(s));
}

char*
osl_strcpy(char *d, const char *s)
{
	return (strcpy(d, s));
}

char*
osl_strncpy(char *d, const char *s, uint n)
{
	return (strncpy(d, s, n));
}

char*
osl_strchr(const char *s, int c)
{
	return (strchr(s, c));
}

char*
osl_strrchr(const char *s, int c)
{
	return (strrchr(s, c));
}

void*
osl_memset(void *d, int c, size_t n)
{
	return memset(d, c, n);
}

void*
osl_memcpy(void *d, const void *s, size_t n)
{
	return memcpy(d, s, n);
}

void*
osl_memmove(void *d, const void *s, size_t n)
{
	return memmove(d, s, n);
}

int
osl_memcmp(const void *s1, const void *s2, size_t n)
{
	return memcmp(s1, s2, n);
}

uint32
osl_readl(volatile uint32 *r)
{
	return (readl(r));
}

uint16
osl_readw(volatile uint16 *r)
{
	return (readw(r));
}

uint8
osl_readb(volatile uint8 *r)
{
	return (readb(r));
}

void
osl_writel(uint32 v, volatile uint32 *r)
{
	writel(v, r);
}

void
osl_writew(uint16 v, volatile uint16 *r)
{
	writew(v, r);
}

void
osl_writeb(uint8 v, volatile uint8 *r)
{
	writeb(v, r);
}

void *
osl_uncached(void *va)
{
	return ((void*)va);
}

void *
osl_cached(void *va)
{
	return ((void*)va);
}

uint
osl_getcycles(void)
{
	uint cycles;

#if defined(__i386__)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
	cycles = (u32)rdtsc();
#else
	rdtscl(cycles);
#endif
#else
	cycles = 0;
#endif 
	return cycles;
}

void *
osl_reg_map(uint32 pa, uint size)
{
	return (ioremap_nocache((unsigned long)pa, (unsigned long)size));
}

void
osl_reg_unmap(void *va)
{
	iounmap(va);
}

int
osl_busprobe(uint32 *val, uint32 addr)
{
	*val = readl((uint32 *)(uintptr)addr);
	return 0;
}

bool
osl_pktshared(void *skb)
{
	return (((struct sk_buff*)skb)->cloned);
}

uchar*
osl_pktdata(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->data);
}

uint
osl_pktlen(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->len);
}

uint
osl_pktheadroom(osl_t *osh, void *skb)
{
	return (uint) skb_headroom((struct sk_buff *) skb);
}

uint
osl_pkttailroom(osl_t *osh, void *skb)
{
	return (uint) skb_tailroom((struct sk_buff *) skb);
}

void*
osl_pktnext(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->next);
}

void
osl_pktsetnext(void *skb, void *x)
{
	((struct sk_buff*)skb)->next = (struct sk_buff*)x;
}

void
osl_pktsetlen(osl_t *osh, void *skb, uint len)
{
	__skb_trim((struct sk_buff*)skb, len);
}

uchar*
osl_pktpush(osl_t *osh, void *skb, int bytes)
{
	return (skb_push((struct sk_buff*)skb, bytes));
}

uchar*
osl_pktpull(osl_t *osh, void *skb, int bytes)
{
	return (skb_pull((struct sk_buff*)skb, bytes));
}

void*
osl_pkttag(void *skb)
{
	return ((void*)(((struct sk_buff*)skb)->cb));
}

void*
osl_pktlink(void *skb)
{
	return (((struct sk_buff*)skb)->prev);
}

void
osl_pktsetlink(void *skb, void *x)
{
	((struct sk_buff*)skb)->prev = (struct sk_buff*)x;
}

uint
osl_pktprio(void *skb)
{
	return (((struct sk_buff*)skb)->priority);
}

void
osl_pktsetprio(void *skb, uint x)
{
	((struct sk_buff*)skb)->priority = x;
}

uint
osl_pktalloced(osl_t *osh)
{
	return (atomic_read(&osh->pktalloced));
}

void *
osl_os_open_image(char *filename)
{
	struct file *fp;

	fp = filp_open(filename, O_RDONLY, 0);

	 if (IS_ERR(fp))
		 fp = NULL;

	 return fp;
}

int
osl_os_get_image_block(char *buf, int len, void *image)
{
	struct file *fp = (struct file *)image;
	int rdlen;
	loff_t pos;

	if (!image)
		return 0;

	pos = fp->f_pos;
	rdlen = kernel_read(fp,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
			pos,
#endif
			buf, len
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
			,&pos
#endif
	);
	if (rdlen > 0)
		fp->f_pos += rdlen;

	return rdlen;
}

void
osl_os_close_image(void *image)
{
	if (image)
		filp_close((struct file *)image, NULL);
}

int
osl_os_image_size(void *image)
{
	int len = 0, curroffset;

	if (image) {

		curroffset = generic_file_llseek(image, 0, 1);

		len = generic_file_llseek(image, 0, 2);

		generic_file_llseek(image, curroffset, 0);
	}
	return len;
}
