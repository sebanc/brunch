/*
 * Copyright (c) 2015  Hauke Mehrtens <hauke@hauke-m.de>
 *
 * Backport functionality introduced in Linux 4.0.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/printk.h>
#include <linux/export.h>
#include <linux/trace_seq.h>
#include <linux/ftrace_event.h>
#include <asm/unaligned.h>

#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
 static __always_inline long __get_user_pages_locked(struct task_struct *tsk,
						struct mm_struct *mm,
						unsigned long start,
						unsigned long nr_pages,
						int write, int force,
						struct page **pages,
						struct vm_area_struct **vmas,
						int *locked, bool notify_drop,
						unsigned int flags)
{
	long ret, pages_done;
	bool lock_dropped;

	if (locked) {
		/* if VM_FAULT_RETRY can be returned, vmas become invalid */
		BUG_ON(vmas);
		/* check caller initialized locked */
		BUG_ON(*locked != 1);
	}

	if (pages)
		flags |= FOLL_GET;
	if (write)
		flags |= FOLL_WRITE;
	if (force)
		flags |= FOLL_FORCE;

	pages_done = 0;
	lock_dropped = false;
	for (;;) {
		ret = __get_user_pages(tsk, mm, start, nr_pages, flags, pages,
				       vmas, locked);
		if (!locked)
			/* VM_FAULT_RETRY couldn't trigger, bypass */
			return ret;

		/* VM_FAULT_RETRY cannot return errors */
		if (!*locked) {
			BUG_ON(ret < 0);
			BUG_ON(ret >= nr_pages);
		}

		if (!pages)
			/* If it's a prefault don't insist harder */
			return ret;

		if (ret > 0) {
			nr_pages -= ret;
			pages_done += ret;
			if (!nr_pages)
				break;
		}
		if (*locked) {
			/* VM_FAULT_RETRY didn't trigger */
			if (!pages_done)
				pages_done = ret;
			break;
		}
		/* VM_FAULT_RETRY triggered, so seek to the faulting offset */
		pages += ret;
		start += ret << PAGE_SHIFT;

		/*
		 * Repeat on the address that fired VM_FAULT_RETRY
		 * without FAULT_FLAG_ALLOW_RETRY but with
		 * FAULT_FLAG_TRIED.
		 */
		*locked = 1;
		lock_dropped = true;
		down_read(&mm->mmap_sem);
		ret = __get_user_pages(tsk, mm, start, 1, flags | FOLL_TRIED,
				       pages, NULL, NULL);
		if (ret != 1) {
			BUG_ON(ret > 1);
			if (!pages_done)
				pages_done = ret;
			break;
		}
		nr_pages--;
		pages_done++;
		if (!nr_pages)
			break;
		pages++;
		start += PAGE_SIZE;
	}
	if (notify_drop && lock_dropped && *locked) {
		/*
		 * We must let the caller know we temporarily dropped the lock
		 * and so the critical section protected by it was lost.
		 */
		up_read(&mm->mmap_sem);
		*locked = 0;
	}
	return pages_done;
}

/*
 * We can leverage the VM_FAULT_RETRY functionality in the page fault
 * paths better by using either get_user_pages_locked() or
 * get_user_pages_unlocked().
 *
 * get_user_pages_locked() is suitable to replace the form:
 *
 *      down_read(&mm->mmap_sem);
 *      do_something()
 *      get_user_pages(tsk, mm, ..., pages, NULL);
 *      up_read(&mm->mmap_sem);
 *
 *  to:
 *
 *      int locked = 1;
 *      down_read(&mm->mmap_sem);
 *      do_something()
 *      get_user_pages_locked(tsk, mm, ..., pages, &locked);
 *      if (locked)
 *          up_read(&mm->mmap_sem);
 */
long get_user_pages_locked(unsigned long start, unsigned long nr_pages,
			   int write, int force, struct page **pages,
			   int *locked)
{
	return __get_user_pages_locked(current, current->mm, start, nr_pages,
				       write, force, pages, NULL, locked, true,
				       FOLL_TOUCH);
}
EXPORT_SYMBOL_GPL(get_user_pages_locked);

/*
 * Same as get_user_pages_unlocked(...., FOLL_TOUCH) but it allows to
 * pass additional gup_flags as last parameter (like FOLL_HWPOISON).
 *
 * NOTE: here FOLL_TOUCH is not set implicitly and must be set by the
 * caller if required (just like with __get_user_pages). "FOLL_GET",
 * "FOLL_WRITE" and "FOLL_FORCE" are set implicitly as needed
 * according to the parameters "pages", "write", "force"
 * respectively.
 */
static __always_inline long __get_user_pages_unlocked(struct task_struct *tsk, struct mm_struct *mm,
					       unsigned long start, unsigned long nr_pages,
					       int write, int force, struct page **pages,
					       unsigned int gup_flags)
{
	long ret;
	int locked = 1;
	down_read(&mm->mmap_sem);
	ret = __get_user_pages_locked(tsk, mm, start, nr_pages, write, force,
				      pages, NULL, &locked, false, gup_flags);
	if (locked)
		up_read(&mm->mmap_sem);
	return ret;
}

/*
 * get_user_pages_unlocked() is suitable to replace the form:
 *
 *      down_read(&mm->mmap_sem);
 *      get_user_pages(tsk, mm, ..., pages, NULL);
 *      up_read(&mm->mmap_sem);
 *
 *  with:
 *
 *      get_user_pages_unlocked(tsk, mm, ..., pages);
 *
 * It is functionally equivalent to get_user_pages_fast so
 * get_user_pages_fast should be used instead, if the two parameters
 * "tsk" and "mm" are respectively equal to current and current->mm,
 * or if "force" shall be set to 1 (get_user_pages_fast misses the
 * "force" parameter).
 */
long get_user_pages_unlocked(unsigned long start, unsigned long nr_pages,
			     int write, int force, struct page **pages)
{
	return __get_user_pages_unlocked(current, current->mm, start, nr_pages,
					 write, force, pages, FOLL_TOUCH);
}
EXPORT_SYMBOL_GPL(get_user_pages_unlocked);
#endif

/**
 * hex_dump_to_buffer - convert a blob of data to "hex ASCII" in memory
 * @buf: data blob to dump
 * @len: number of bytes in the @buf
 * @rowsize: number of bytes to print per line; must be 16 or 32
 * @groupsize: number of bytes to print at a time (1, 2, 4, 8; default = 1)
 * @linebuf: where to put the converted data
 * @linebuflen: total size of @linebuf, including space for terminating NUL
 * @ascii: include ASCII after the hex output
 *
 * hex_dump_to_buffer() works on one "line" of output at a time, i.e.,
 * 16 or 32 bytes of input data converted to hex + ASCII output.
 *
 * Given a buffer of u8 data, hex_dump_to_buffer() converts the input data
 * to a hex + ASCII dump at the supplied memory location.
 * The converted output is always NUL-terminated.
 *
 * E.g.:
 *   hex_dump_to_buffer(frame->data, frame->len, 16, 1,
 *			linebuf, sizeof(linebuf), true);
 *
 * example output buffer:
 * 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f  @ABCDEFGHIJKLMNO
 *
 * Return:
 * The amount of bytes placed in the buffer without terminating NUL. If the
 * output was truncated, then the return value is the number of bytes
 * (excluding the terminating NUL) which would have been written to the final
 * string if enough space had been available.
 */
int hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize,
		       char *linebuf, size_t linebuflen, bool ascii)
{
	const u8 *ptr = buf;
	int ngroups;
	u8 ch;
	int j, lx = 0;
	int ascii_column;
	int ret;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	if (len > rowsize)		/* limit to one line at a time */
		len = rowsize;
	if (!is_power_of_2(groupsize) || groupsize > 8)
		groupsize = 1;
	if ((len % groupsize) != 0)	/* no mixed size output */
		groupsize = 1;

	ngroups = len / groupsize;
	ascii_column = rowsize * 2 + rowsize / groupsize + 1;

	if (!linebuflen)
		goto overflow1;

	if (!len)
		goto nil;

	if (groupsize == 8) {
		const u64 *ptr8 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%16.16llx", j ? " " : "",
				       get_unaligned(ptr8 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else if (groupsize == 4) {
		const u32 *ptr4 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%8.8x", j ? " " : "",
				       get_unaligned(ptr4 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else if (groupsize == 2) {
		const u16 *ptr2 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%4.4x", j ? " " : "",
				       get_unaligned(ptr2 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else {
		for (j = 0; j < len; j++) {
			if (linebuflen < lx + 3)
				goto overflow2;
			ch = ptr[j];
			linebuf[lx++] = hex_asc_hi(ch);
			linebuf[lx++] = hex_asc_lo(ch);
			linebuf[lx++] = ' ';
		}
		if (j)
			lx--;
	}
	if (!ascii)
		goto nil;

	while (lx < ascii_column) {
		if (linebuflen < lx + 2)
			goto overflow2;
		linebuf[lx++] = ' ';
	}
	for (j = 0; j < len; j++) {
		if (linebuflen < lx + 2)
			goto overflow2;
		ch = ptr[j];
		linebuf[lx++] = (isascii(ch) && isprint(ch)) ? ch : '.';
	}
nil:
	linebuf[lx] = '\0';
	return lx;
overflow2:
	linebuf[lx++] = '\0';
overflow1:
	return ascii ? ascii_column + len : (groupsize * 2 + 1) * ngroups - 1;
}
EXPORT_SYMBOL_GPL(hex_dump_to_buffer);

#if LINUX_VERSION_IS_LESS(3,17,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline unsigned char *
trace_seq_buffer_ptr(struct trace_seq *s)
{
	return s->buffer + s->len;
}
#endif

const char *
ftrace_print_array_seq(struct trace_seq *p, const void *buf, int buf_len,
		       size_t el_size)
{
	const char *ret = trace_seq_buffer_ptr(p);
	const char *prefix = "";
	void *ptr = (void *)buf;

	trace_seq_putc(p, '{');

	while (ptr < buf + buf_len) {
		switch (el_size) {
		case 1:
			trace_seq_printf(p, "%s0x%x", prefix,
					 *(u8 *)ptr);
			break;
		case 2:
			trace_seq_printf(p, "%s0x%x", prefix,
					 *(u16 *)ptr);
			break;
		case 4:
			trace_seq_printf(p, "%s0x%x", prefix,
					 *(u32 *)ptr);
			break;
		case 8:
			trace_seq_printf(p, "%s0x%llx", prefix,
					 *(u64 *)ptr);
			break;
		default:
			trace_seq_printf(p, "BAD SIZE:%zu 0x%x", el_size,
					 *(u8 *)ptr);
			el_size = 1;
		}
		prefix = ",";
		ptr += el_size;
	}

	trace_seq_putc(p, '}');
	trace_seq_putc(p, 0);

	return ret;
}
EXPORT_SYMBOL(ftrace_print_array_seq);
