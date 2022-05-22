/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __OSDEP_SERVICE_H_
#define __OSDEP_SERVICE_H_


#define _FAIL					0
#define _SUCCESS				1
#define RTW_RX_HANDLED			2
#define RTW_RFRAME_UNAVAIL		3
#define RTW_RFRAME_PKT_UNAVAIL	4
#define RTW_RBUF_UNAVAIL		5
#define RTW_RBUF_PKT_UNAVAIL	6
#define RTW_SDIO_READ_PORT_FAIL	7

/* #define RTW_STATUS_TIMEDOUT -110 */

#undef true
#define true		1

#undef false
#define false		0


	#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
	#include <linux/sched/signal.h>
	#include <linux/sched/types.h>
#endif
	#include <osdep_service_linux.h>

/* #include <rtw_byteorder.h> */

#ifndef BIT
	#define BIT(x)	(1 << (x))
#endif

#define BIT0	0x00000001
#define BIT1	0x00000002
#define BIT2	0x00000004
#define BIT3	0x00000008
#define BIT4	0x00000010
#define BIT5	0x00000020
#define BIT6	0x00000040
#define BIT7	0x00000080
#define BIT8	0x00000100
#define BIT9	0x00000200
#define BIT10	0x00000400
#define BIT11	0x00000800
#define BIT12	0x00001000
#define BIT13	0x00002000
#define BIT14	0x00004000
#define BIT15	0x00008000
#define BIT16	0x00010000
#define BIT17	0x00020000
#define BIT18	0x00040000
#define BIT19	0x00080000
#define BIT20	0x00100000
#define BIT21	0x00200000
#define BIT22	0x00400000
#define BIT23	0x00800000
#define BIT24	0x01000000
#define BIT25	0x02000000
#define BIT26	0x04000000
#define BIT27	0x08000000
#define BIT28	0x10000000
#define BIT29	0x20000000
#define BIT30	0x40000000
#define BIT31	0x80000000
#define BIT32	0x0100000000
#define BIT33	0x0200000000
#define BIT34	0x0400000000
#define BIT35	0x0800000000
#define BIT36	0x1000000000

extern int RTW_STATUS_CODE(int error_code);

#ifndef RTK_DMP_PLATFORM
	#define CONFIG_USE_VMALLOC
#endif

/* flags used for rtw_mstat_update() */
enum mstat_f {
	/* type: 0x00ff */
	MSTAT_TYPE_VIR = 0x00,
	MSTAT_TYPE_PHY = 0x01,
	MSTAT_TYPE_SKB = 0x02,
	MSTAT_TYPE_USB = 0x03,
	MSTAT_TYPE_MAX = 0x04,

	/* func: 0xff00 */
	MSTAT_FUNC_UNSPECIFIED = 0x00 << 8,
	MSTAT_FUNC_IO = 0x01 << 8,
	MSTAT_FUNC_TX_IO = 0x02 << 8,
	MSTAT_FUNC_RX_IO = 0x03 << 8,
	MSTAT_FUNC_TX = 0x04 << 8,
	MSTAT_FUNC_RX = 0x05 << 8,
	MSTAT_FUNC_CFG_VENDOR = 0x06 << 8,
	MSTAT_FUNC_MAX = 0x07 << 8,
};

#define mstat_tf_idx(flags) ((flags) & 0xff)
#define mstat_ff_idx(flags) (((flags) & 0xff00) >> 8)

enum mstat_status {
	MSTAT_ALLOC_SUCCESS = 0,
	MSTAT_ALLOC_FAIL,
	MSTAT_FREE
};

#define rtw_mstat_update(flag, status, sz) do {} while (0)
#define rtw_mstat_dump(sel) do {} while (0)
void *_rtw_zmalloc(u32 sz);
void *_rtw_malloc(u32 sz);
void _rtw_mfree(void *pbuf, u32 sz);

struct sk_buff *_rtw_skb_alloc(u32 sz);
struct sk_buff *_rtw_skb_copy(const struct sk_buff *skb);
struct sk_buff *_rtw_skb_clone(struct sk_buff *skb);
int _rtw_netif_rx(struct  net_device * ndev, struct sk_buff *skb);
#ifdef CONFIG_RTW_NAPI
int _rtw_netif_receive_skb(struct  net_device * ndev, struct sk_buff *skb);
gro_result_t _rtw_napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb);
#endif /* CONFIG_RTW_NAPI */
void _rtw_skb_queue_purge(struct sk_buff_head *list);

void *_rtw_usb_buffer_alloc(struct usb_device *dev, size_t size, dma_addr_t *dma);
void _rtw_usb_buffer_free(struct usb_device *dev, size_t size, void *addr, dma_addr_t dma);

#ifdef CONFIG_USE_VMALLOC
#else /* CONFIG_USE_VMALLOC */
#define vmalloc(sz)			_rtw_malloc((sz))
#define vmalloc_f(sz, mstat_f)			_rtw_malloc((sz))
#endif /* CONFIG_USE_VMALLOC */
#define rtw_malloc(sz)			_rtw_malloc((sz))
#define rtw_zmalloc(sz)			_rtw_zmalloc((sz))
#define rtw_mfree(pbuf, sz)		_rtw_mfree((pbuf), (sz))
#define rtw_malloc_f(sz, mstat_f)			_rtw_malloc((sz))
#define rtw_zmalloc_f(sz, mstat_f)			_rtw_zmalloc((sz))
#define rtw_mfree_f(pbuf, sz, mstat_f)		_rtw_mfree((pbuf), (sz))

#define rtw_skb_alloc(size) _rtw_skb_alloc((size))
#define rtw_skb_alloc_f(size, mstat_f)	_rtw_skb_alloc((size))
#define rtw_skb_copy(skb)	_rtw_skb_copy((skb))
#define rtw_skb_clone(skb)	_rtw_skb_clone((skb))
#define rtw_skb_copy_f(skb, mstat_f)	_rtw_skb_copy((skb))
#define rtw_skb_clone_f(skb, mstat_f)	_rtw_skb_clone((skb))
#define rtw_netif_rx(ndev, skb) _rtw_netif_rx(ndev, skb)
#ifdef CONFIG_RTW_NAPI
#define rtw_netif_receive_skb(ndev, skb) _rtw_netif_receive_skb(ndev, skb)
#define rtw_napi_gro_receive(napi, skb) _rtw_napi_gro_receive(napi, skb)
#endif /* CONFIG_RTW_NAPI */
#define rtw_skb_queue_purge(sk_buff_head) _rtw_skb_queue_purge(sk_buff_head)
#define rtw_usb_buffer_alloc(dev, size, dma) _rtw_usb_buffer_alloc((dev), (size), (dma))
#define rtw_usb_buffer_free(dev, size, addr, dma) _rtw_usb_buffer_free((dev), (size), (addr), (dma))
#define rtw_usb_buffer_alloc_f(dev, size, dma, mstat_f) _rtw_usb_buffer_alloc((dev), (size), (dma))
#define rtw_usb_buffer_free_f(dev, size, addr, dma, mstat_f) _rtw_usb_buffer_free((dev), (size), (addr), (dma))

extern void	*rtw_malloc2d(int h, int w, size_t size);
extern void	rtw_mfree2d(void *pbuf, int h, int w, int size);

extern u32	rtw_is_list_empty(struct list_head *phead);
extern void	rtw_list_delete(struct list_head *plist);

extern u32	_rtw_down_sema(struct semaphore *sema);
extern void	_rtw_mutex_init(_mutex *pmutex);
extern void	_rtw_mutex_free(_mutex *pmutex);

extern void	_rtw_init_queue(struct __queue *pqueue);
extern u32	_rtw_queue_empty(struct __queue	*pqueue);
extern u32	rtw_end_of_queue_search(struct list_head *queue, struct list_head *pelement);

extern unsigned long _rtw_get_current_time(void);
extern u32	_rtw_systime_to_ms(unsigned long stime);
extern unsigned long _rtw_ms_to_systime(u32 ms);
extern int	_rtw_get_passing_time_ms(unsigned long start);
extern int	_rtw_get_time_interval_ms(unsigned long start, unsigned long end);

#define rtw_get_current_time() _rtw_get_current_time()
#define rtw_systime_to_ms(stime) _rtw_systime_to_ms(stime)
#define rtw_ms_to_systime(ms) _rtw_ms_to_systime(ms)
#define rtw_get_passing_time_ms(start) _rtw_get_passing_time_ms(start)
#define rtw_get_time_interval_ms(start, end) _rtw_get_time_interval_ms(start, end)

extern void	rtw_sleep_schedulable(int ms);

extern void	rtw_msleep_os(int ms);
extern void	rtw_usleep_os(int us);

extern u32	rtw_atoi(u8 *s);

extern void	rtw_mdelay_os(int ms);
extern void	rtw_udelay_os(int us);

extern void rtw_yield_os(void);


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
extern void rtw_init_timer(struct timer_list *ptimer, void *adapt, void *pfunc, void *ctx);
#endif

static inline unsigned char _cancel_timer_ex(struct timer_list *ptimer)
{
	u8 bcancelled;

	_cancel_timer(ptimer, &bcancelled);

	return bcancelled;
}

static inline void thread_enter(char *name)
{
	allow_signal(SIGTERM);
}
void thread_exit(struct completion *comp);

static inline bool rtw_thread_stop(void * th)
{
	return kthread_stop(th);
}
static inline void rtw_thread_wait_stop(void)
{
	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	__set_current_state(TASK_RUNNING);
}

static inline void flush_signals_thread(void)
{
	if (signal_pending(current))
		flush_signals(current);
}

static inline int res_to_status(int res)
{
	return res;
}

static inline void rtw_dump_stack(void)
{
	dump_stack();
}

#define rtw_warn_on(condition) WARN_ON(condition)

static inline int rtw_bug_check(void *parg1, void *parg2, void *parg3, void *parg4)
{
	int ret = true;

	return ret;
}
#define RTW_DIV_ROUND_UP(n, d)	DIV_ROUND_UP(n, d)

#define _RND(sz, r) ((((sz)+((r)-1))/(r))*(r))
#define RND4(x)	(((x >> 2) + (((x & 3) == 0) ? 0 : 1)) << 2)

static inline u32 _RND4(u32 sz)
{

	u32	val;

	val = ((sz >> 2) + ((sz & 3) ? 1 : 0)) << 2;

	return val;

}

static inline u32 _RND8(u32 sz)
{

	u32	val;

	val = ((sz >> 3) + ((sz & 7) ? 1 : 0)) << 3;

	return val;

}

static inline u32 _RND128(u32 sz)
{

	u32	val;

	val = ((sz >> 7) + ((sz & 127) ? 1 : 0)) << 7;

	return val;

}

static inline u32 _RND256(u32 sz)
{

	u32	val;

	val = ((sz >> 8) + ((sz & 255) ? 1 : 0)) << 8;

	return val;

}

static inline u32 _RND512(u32 sz)
{

	u32	val;

	val = ((sz >> 9) + ((sz & 511) ? 1 : 0)) << 9;

	return val;

}

static inline u32 bitshift(u32 bitmask)
{
	u32 i;

	for (i = 0; i <= 31; i++)
		if (((bitmask >> i) &  0x1) == 1)
			break;

	return i;
}

static inline int largest_bit(u32 bitmask)
{
	int i;

	for (i = 31; i >= 0; i--)
		if (bitmask & BIT(i))
			break;

	return i;
}

#define rtw_min(a, b) ((a > b) ? b : a)
#define rtw_is_range_a_in_b(hi_a, lo_a, hi_b, lo_b) (((hi_a) <= (hi_b)) && ((lo_a) >= (lo_b)))
#define rtw_is_range_overlap(hi_a, lo_a, hi_b, lo_b) (((hi_a) > (lo_b)) && ((lo_a) < (hi_b)))

#ifndef MAC_FMT
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#ifndef MAC_ARG
#define MAC_ARG(x) ((u8 *)(x))[0], ((u8 *)(x))[1], ((u8 *)(x))[2], ((u8 *)(x))[3], ((u8 *)(x))[4], ((u8 *)(x))[5]
#endif


extern void rtw_suspend_lock_init(void);
extern void rtw_suspend_lock_uninit(void);
extern void rtw_lock_suspend(void);
extern void rtw_unlock_suspend(void);
extern void rtw_lock_suspend_timeout(u32 timeout_ms);
extern void rtw_lock_ext_suspend_timeout(u32 timeout_ms);
extern void rtw_lock_rx_suspend_timeout(u32 timeout_ms);
extern void rtw_lock_traffic_suspend_timeout(u32 timeout_ms);
extern void rtw_lock_resume_scan_timeout(u32 timeout_ms);
extern void rtw_resume_lock_suspend(void);
extern void rtw_resume_unlock_suspend(void);
extern void ATOMIC_SET(ATOMIC_T *v, int i);
extern int ATOMIC_READ(ATOMIC_T *v);
extern void ATOMIC_ADD(ATOMIC_T *v, int i);
extern void ATOMIC_SUB(ATOMIC_T *v, int i);
extern void ATOMIC_INC(ATOMIC_T *v);
extern void ATOMIC_DEC(ATOMIC_T *v);
extern int ATOMIC_ADD_RETURN(ATOMIC_T *v, int i);
extern int ATOMIC_SUB_RETURN(ATOMIC_T *v, int i);
extern int ATOMIC_INC_RETURN(ATOMIC_T *v);
extern int ATOMIC_DEC_RETURN(ATOMIC_T *v);

/* File operation APIs, just for linux now */
extern int rtw_is_file_readable(const char *path);
extern int rtw_is_file_readable_with_size(const char *path, u32 *sz);
extern int rtw_retrieve_from_file(const char *path, u8 *buf, u32 sz);
extern int rtw_store_to_file(const char *path, u8 *buf, u32 sz);

extern void rtw_free_netdev(struct net_device *netdev);

extern u64 rtw_modular64(u64 x, u64 y);
extern u64 rtw_division64(u64 x, u64 y);
extern u32 rtw_random32(void);

/* Macros for handling unaligned memory accesses */

#define RTW_GET_BE16(a) ((u16) (((a)[0] << 8) | (a)[1]))
#define RTW_PUT_BE16(a, val)			\
	do {					\
		(a)[0] = ((u16) (val)) >> 8;	\
		(a)[1] = ((u16) (val)) & 0xff;	\
	} while (0)

#define RTW_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))
#define RTW_PUT_LE16(a, val)			\
	do {					\
		(a)[1] = ((u16) (val)) >> 8;	\
		(a)[0] = ((u16) (val)) & 0xff;	\
	} while (0)

#define RTW_GET_BE24(a) ((((u32) (a)[0]) << 16) | (((u32) (a)[1]) << 8) | \
			 ((u32) (a)[2]))
#define RTW_PUT_BE24(a, val)					\
	do {							\
		(a)[0] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[2] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define RTW_GET_BE32(a) ((((u32) (a)[0]) << 24) | (((u32) (a)[1]) << 16) | \
			 (((u32) (a)[2]) << 8) | ((u32) (a)[3]))
#define RTW_PUT_BE32(a, val)					\
	do {							\
		(a)[0] = (u8) ((((u32) (val)) >> 24) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[2] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[3] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define RTW_GET_LE32(a) ((((u32) (a)[3]) << 24) | (((u32) (a)[2]) << 16) | \
			 (((u32) (a)[1]) << 8) | ((u32) (a)[0]))
#define RTW_PUT_LE32(a, val)					\
	do {							\
		(a)[3] = (u8) ((((u32) (val)) >> 24) & 0xff);	\
		(a)[2] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
		(a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
		(a)[0] = (u8) (((u32) (val)) & 0xff);		\
	} while (0)

#define RTW_GET_BE64(a) ((((u64) (a)[0]) << 56) | (((u64) (a)[1]) << 48) | \
			 (((u64) (a)[2]) << 40) | (((u64) (a)[3]) << 32) | \
			 (((u64) (a)[4]) << 24) | (((u64) (a)[5]) << 16) | \
			 (((u64) (a)[6]) << 8) | ((u64) (a)[7]))
#define RTW_PUT_BE64(a, val)				\
	do {						\
		(a)[0] = (u8) (((u64) (val)) >> 56);	\
		(a)[1] = (u8) (((u64) (val)) >> 48);	\
		(a)[2] = (u8) (((u64) (val)) >> 40);	\
		(a)[3] = (u8) (((u64) (val)) >> 32);	\
		(a)[4] = (u8) (((u64) (val)) >> 24);	\
		(a)[5] = (u8) (((u64) (val)) >> 16);	\
		(a)[6] = (u8) (((u64) (val)) >> 8);	\
		(a)[7] = (u8) (((u64) (val)) & 0xff);	\
	} while (0)

#define RTW_GET_LE64(a) ((((u64) (a)[7]) << 56) | (((u64) (a)[6]) << 48) | \
			 (((u64) (a)[5]) << 40) | (((u64) (a)[4]) << 32) | \
			 (((u64) (a)[3]) << 24) | (((u64) (a)[2]) << 16) | \
			 (((u64) (a)[1]) << 8) | ((u64) (a)[0]))
#define RTW_PUT_LE64(a, val)					\
	do {							\
		(a)[7] = (u8) ((((u64) (val)) >> 56) & 0xff);	\
		(a)[6] = (u8) ((((u64) (val)) >> 48) & 0xff);	\
		(a)[5] = (u8) ((((u64) (val)) >> 40) & 0xff);	\
		(a)[4] = (u8) ((((u64) (val)) >> 32) & 0xff);	\
		(a)[3] = (u8) ((((u64) (val)) >> 24) & 0xff);	\
		(a)[2] = (u8) ((((u64) (val)) >> 16) & 0xff);	\
		(a)[1] = (u8) ((((u64) (val)) >> 8) & 0xff);	\
		(a)[0] = (u8) (((u64) (val)) & 0xff);		\
	} while (0)

void rtw_buf_free(u8 **buf, u32 *buf_len);
void rtw_buf_update(u8 **buf, u32 *buf_len, u8 *src, u32 src_len);

struct rtw_cbuf {
	u32 write;
	u32 read;
	u32 size;
	void *bufs[0];
};

bool rtw_cbuf_full(struct rtw_cbuf *cbuf);
bool rtw_cbuf_empty(struct rtw_cbuf *cbuf);
bool rtw_cbuf_push(struct rtw_cbuf *cbuf, void *buf);
void *rtw_cbuf_pop(struct rtw_cbuf *cbuf);
struct rtw_cbuf *rtw_cbuf_alloc(u32 size);
void rtw_cbuf_free(struct rtw_cbuf *cbuf);

struct map_seg_t {
	u16 sa;
	u16 len;
	u8 *c;
};

struct map_t {
	u16 len;
	u16 seg_num;
	u8 init_value;
	struct map_seg_t *segs;
};

#define MAPSEG_ARRAY_ENT(_sa, _len, _c, arg...) \
	{ .sa = _sa, .len = _len, .c = (u8[_len]){ _c, ##arg}, }

#define MAPSEG_PTR_ENT(_sa, _len, _p) \
	{ .sa = _sa, .len = _len, .c = _p, }

#define MAP_ENT(_len, _seg_num, _init_v, _seg, arg...) \
	{ .len = _len, .seg_num = _seg_num, .init_value = _init_v, .segs = (struct map_seg_t[_seg_num]){ _seg, ##arg}, }

int map_readN(const struct map_t *map, u16 offset, u16 len, u8 *buf);
u8 map_read8(const struct map_t *map, u16 offset);

/* String handler */

bool is_null(char c);
bool is_all_null(char *c, int len);
bool is_eol(char c);
bool is_space(char c);
bool IsHexDigit(char chTmp);
bool is_alpha(char chTmp);
char alpha_to_upper(char c);

int hex2num_i(char c);
int hex2byte_i(const char *hex);
int hexstr2bin(const char *hex, u8 *buf, size_t len);

/*
 * Write formatted output to sized buffer
 */
#define rtw_sprintf(buf, size, format, arg...)	snprintf(buf, size, format, ##arg)

#endif
