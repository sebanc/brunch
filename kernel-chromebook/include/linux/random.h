/*
 * include/linux/random.h
 *
 * Include file for the random number generator.
 */
#ifndef _LINUX_RANDOM_H
#define _LINUX_RANDOM_H

#include <linux/list.h>
#include <linux/once.h>

#include <uapi/linux/random.h>

struct random_ready_callback {
	struct list_head list;
	void (*func)(struct random_ready_callback *rdy);
	struct module *owner;
};

extern void add_device_randomness(const void *, unsigned int);
extern void add_input_randomness(unsigned int type, unsigned int code,
				 unsigned int value);
extern void add_interrupt_randomness(int irq, int irq_flags);

extern void get_random_bytes(void *buf, int nbytes);
extern int add_random_ready_callback(struct random_ready_callback *rdy);
extern void del_random_ready_callback(struct random_ready_callback *rdy);
extern void get_random_bytes_arch(void *buf, int nbytes);
void generate_random_uuid(unsigned char uuid_out[16]);
extern int random_int_secret_init(void);

#ifndef MODULE
extern const struct file_operations random_fops, urandom_fops;
#endif

unsigned int get_random_int(void);
unsigned long get_random_long(void);
unsigned long randomize_range(unsigned long start, unsigned long end, unsigned long len);

/*
 * This is designed to be standalone for just prandom
 * users, but for now we include it from <linux/random.h>
 * for legacy reasons.
 */
#include <linux/prandom.h>

#ifdef CONFIG_ARCH_RANDOM
# include <asm/archrandom.h>
#else
static inline int arch_get_random_long(unsigned long *v)
{
	return 0;
}
static inline int arch_get_random_int(unsigned int *v)
{
	return 0;
}
static inline int arch_has_random(void)
{
	return 0;
}
static inline int arch_get_random_seed_long(unsigned long *v)
{
	return 0;
}
static inline int arch_get_random_seed_int(unsigned int *v)
{
	return 0;
}
static inline int arch_has_random_seed(void)
{
	return 0;
}
#endif

#endif /* _LINUX_RANDOM_H */
