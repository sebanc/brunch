#ifndef __BACKPORT_LINUX_COMPILER_H
#define __BACKPORT_LINUX_COMPILER_H
#include_next <linux/compiler.h>

#include <linux/compiler_attributes.h>

#ifndef __rcu
#define __rcu
#endif

#ifndef __always_unused
#ifdef __GNUC__
#define __always_unused			__attribute__((unused))
#else
#define __always_unused			/* unimplemented */
#endif
#endif

#ifndef __PASTE
/* Indirect macros required for expanded argument pasting, eg. __LINE__. */
#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)
#endif

/* Not-quite-unique ID. */
#ifndef __UNIQUE_ID
# define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __LINE__)
#endif

#ifndef barrier_data
#ifdef __GNUC__
#define barrier_data(ptr) __asm__ __volatile__("": :"r"(ptr) :"memory")
#else /* __GNUC__ */
# define barrier_data(ptr) barrier()
#endif /* __GNUC__ */
#endif

#ifndef READ_ONCE
#include <linux/types.h>

#define __READ_ONCE_SIZE						\
({									\
	switch (size) {							\
	case 1: *(__u8 *)res = *(volatile __u8 *)p; break;		\
	case 2: *(__u16 *)res = *(volatile __u16 *)p; break;		\
	case 4: *(__u32 *)res = *(volatile __u32 *)p; break;		\
	case 8: *(__u64 *)res = *(volatile __u64 *)p; break;		\
	default:							\
		barrier();						\
		__builtin_memcpy((void *)res, (const void *)p, size);	\
		barrier();						\
	}								\
})

static __always_inline
void __read_once_size(const volatile void *p, void *res, int size)
{
	__READ_ONCE_SIZE;
}

#define __READ_ONCE(x, check)						\
({									\
	union { typeof(x) __val; char __c[1]; } __u;			\
	__read_once_size(&(x), __u.__c, sizeof(x));			\
	__u.__val;							\
})

#define READ_ONCE(x) __READ_ONCE(x, 1)

static __always_inline void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile __u8 *)p = *(__u8 *)res; break;
	case 2: *(volatile __u16 *)p = *(__u16 *)res; break;
	case 4: *(volatile __u32 *)p = *(__u32 *)res; break;
	case 8: *(volatile __u64 *)p = *(__u64 *)res; break;
	default:
		barrier();
		__builtin_memcpy((void *)p, (const void *)res, size);
		barrier();
	}
}

#define WRITE_ONCE(x, val) \
({							\
	union { typeof(x) __val; char __c[1]; } __u =	\
		{ .__val = (__force typeof(x)) (val) }; \
	__write_once_size(&(x), __u.__c, sizeof(x));	\
	__u.__val;					\
})
#endif

#ifndef OPTIMIZER_HIDE_VAR
#define OPTIMIZER_HIDE_VAR(var) barrier()
#endif

#ifndef data_race
#define data_race(expr)	(expr)
#endif

#endif /* __BACKPORT_LINUX_COMPILER_H */
