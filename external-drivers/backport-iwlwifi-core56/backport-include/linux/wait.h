#ifndef __BACKPORT_LINUX_WAIT_H
#define __BACKPORT_LINUX_WAIT_H
#include_next <linux/wait.h>

#if LINUX_VERSION_IS_LESS(3,17,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
extern int bit_wait(void *);
extern int bit_wait_io(void *);

static inline int
backport_wait_on_bit(void *word, int bit, unsigned mode)
{
	return wait_on_bit(word, bit, bit_wait, mode);
}

static inline int
backport_wait_on_bit_io(void *word, int bit, unsigned mode)
{
	return wait_on_bit(word, bit, bit_wait_io, mode);
}

#define wait_on_bit LINUX_BACKPORT(wait_on_bit)
#define wait_on_bit_io LINUX_BACKPORT(wait_on_bit_io)

#endif

#if LINUX_VERSION_IS_LESS(3,18,12)
#define WQ_FLAG_WOKEN		0x02

#define wait_woken LINUX_BACKPORT(wait_woken)
long wait_woken(wait_queue_t *wait, unsigned mode, long timeout);
#define wait_woken LINUX_BACKPORT(wait_woken)
int woken_wake_function(wait_queue_t *wait, unsigned mode, int sync, void *key);
#endif

/**
 * For wait_on_bit_timeout() an extra member in struct wait_bit_key is needed.
 * This was introuced in kernel 3.17 and we are only able to backport this
 * function on these kernel versions.
 */
#if LINUX_VERSION_IS_GEQ(3,17,0)
#if LINUX_VERSION_IS_LESS(3,18,0)
#define out_of_line_wait_on_bit_timeout LINUX_BACKPORT(out_of_line_wait_on_bit_timeout)
int out_of_line_wait_on_bit_timeout(void *, int, wait_bit_action_f *, unsigned, unsigned long);

#define bit_wait_timeout LINUX_BACKPORT(bit_wait_timeout)
extern int bit_wait_timeout(struct wait_bit_key *);
#endif

#if LINUX_VERSION_IS_LESS(3,20,0)
#define wait_on_bit_timeout LINUX_BACKPORT(wait_on_bit_timeout)
/**
 * wait_on_bit_timeout - wait for a bit to be cleared or a timeout elapses
 * @word: the word being waited on, a kernel virtual address
 * @bit: the bit of the word being waited on
 * @mode: the task state to sleep in
 * @timeout: timeout, in jiffies
 *
 * Use the standard hashed waitqueue table to wait for a bit
 * to be cleared. This is similar to wait_on_bit(), except also takes a
 * timeout parameter.
 *
 * Returned value will be zero if the bit was cleared before the
 * @timeout elapsed, or non-zero if the @timeout elapsed or process
 * received a signal and the mode permitted wakeup on that signal.
 */
static inline int
wait_on_bit_timeout(void *word, int bit, unsigned mode, unsigned long timeout)
{
	might_sleep();
	if (!test_bit(bit, word))
		return 0;
	return out_of_line_wait_on_bit_timeout(word, bit,
					       bit_wait_timeout,
					       mode, timeout);
}
#endif
#endif

#if LINUX_VERSION_IS_LESS(4,13,0)
#define wait_queue_entry_t wait_queue_t

#define wait_event_killable_timeout(wq_head, condition, timeout)	\
({									\
	long __ret = timeout;						\
	might_sleep();							\
	if (!___wait_cond_timeout(condition))				\
		__ret = __wait_event_killable_timeout(wq_head,		\
						condition, timeout);	\
	__ret;								\
})

#define __wait_event_killable_timeout(wq_head, condition, timeout)	\
	___wait_event(wq_head, ___wait_cond_timeout(condition),		\
		      TASK_KILLABLE, 0, timeout,			\
		      __ret = schedule_timeout(__ret))
#endif

#endif /* __BACKPORT_LINUX_WAIT_H */
