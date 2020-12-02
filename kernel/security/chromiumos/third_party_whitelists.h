/*
 * Linux Security Module for Chromium OS
 *
 * Copyright 2018 Google LLC. All Rights Reserved
 *
 * Authors:
 *      Micah Morton <mortonm@chromium.org>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef THIRD_PARTY_WHITELISTS_H
#define THIRD_PARTY_WHITELISTS_H

/*
 * NOTE: the purpose of this header is only to pull out the definition of this
 * array from alt-syscall.c for the purposes of readability. It should not be
 * included in other .c files.
 */

#include "alt-syscall.h"

static struct syscall_whitelist_entry third_party_whitelist[] = {
	SYSCALL_ENTRY(accept),
	SYSCALL_ENTRY(bind),
	SYSCALL_ENTRY(brk),
	SYSCALL_ENTRY(chdir),
	SYSCALL_ENTRY(clock_gettime),
	SYSCALL_ENTRY(clone),
	SYSCALL_ENTRY(close),
	SYSCALL_ENTRY(connect),
	SYSCALL_ENTRY(dup),
	SYSCALL_ENTRY(execve),
	SYSCALL_ENTRY(exit),
	SYSCALL_ENTRY(exit_group),
	SYSCALL_ENTRY(fcntl),
	SYSCALL_ENTRY(fstat),
	SYSCALL_ENTRY(futex),
	SYSCALL_ENTRY(getcwd),
	SYSCALL_ENTRY(getdents64),
	SYSCALL_ENTRY(getpid),
	SYSCALL_ENTRY(getpgid),
	SYSCALL_ENTRY(getppid),
	SYSCALL_ENTRY(getpriority),
	SYSCALL_ENTRY(getrlimit),
	SYSCALL_ENTRY(getsid),
	SYSCALL_ENTRY(gettimeofday),
	SYSCALL_ENTRY(ioctl),
	SYSCALL_ENTRY(listen),
	SYSCALL_ENTRY(lseek),
	SYSCALL_ENTRY(madvise),
        SYSCALL_ENTRY(memfd_create),
	SYSCALL_ENTRY(mprotect),
	SYSCALL_ENTRY(munmap),
	SYSCALL_ENTRY(nanosleep),
	SYSCALL_ENTRY(openat),
	SYSCALL_ENTRY(prlimit64),
	SYSCALL_ENTRY(read),
	SYSCALL_ENTRY(recvfrom),
	SYSCALL_ENTRY(recvmsg),
	SYSCALL_ENTRY(rt_sigaction),
	SYSCALL_ENTRY(rt_sigprocmask),
	SYSCALL_ENTRY(rt_sigreturn),
	SYSCALL_ENTRY(sendfile),
	SYSCALL_ENTRY(sendmsg),
	SYSCALL_ENTRY(sendto),
	SYSCALL_ENTRY(set_robust_list),
	SYSCALL_ENTRY(set_tid_address),
	SYSCALL_ENTRY(setpgid),
	SYSCALL_ENTRY(setpriority),
	SYSCALL_ENTRY(setsid),
	SYSCALL_ENTRY(setsockopt),
	SYSCALL_ENTRY(socket),
	SYSCALL_ENTRY(socketpair),
	SYSCALL_ENTRY(syslog),
	SYSCALL_ENTRY(statfs),
	SYSCALL_ENTRY(umask),
	SYSCALL_ENTRY(uname),
	SYSCALL_ENTRY(wait4),
	SYSCALL_ENTRY(write),
	SYSCALL_ENTRY(writev),

	/*
	 * Deprecated syscalls which are not wired up on new architectures
	 * such as ARM64.
	 */
#ifndef CONFIG_ARM64
	SYSCALL_ENTRY(access),
	SYSCALL_ENTRY(creat),
	SYSCALL_ENTRY(dup2),
	SYSCALL_ENTRY(getdents),
	SYSCALL_ENTRY(getpgrp),
	SYSCALL_ENTRY(lstat),
	SYSCALL_ENTRY(mkdir),
	SYSCALL_ENTRY(open),
	SYSCALL_ENTRY(pipe),
	SYSCALL_ENTRY(poll),
	SYSCALL_ENTRY(readlink),
	SYSCALL_ENTRY(stat),
	SYSCALL_ENTRY(unlink),
#endif

	SYSCALL_ENTRY(accept),
	SYSCALL_ENTRY(bind),
	SYSCALL_ENTRY(connect),
	SYSCALL_ENTRY(listen),
	SYSCALL_ENTRY(recvfrom),
	SYSCALL_ENTRY(recvmsg),
	SYSCALL_ENTRY(sendmsg),
	SYSCALL_ENTRY(sendto),
	SYSCALL_ENTRY(setsockopt),
	SYSCALL_ENTRY(socket),
	SYSCALL_ENTRY(socketpair),

	/* 64-bit only syscalls. */
	SYSCALL_ENTRY(getegid),
	SYSCALL_ENTRY(geteuid),
	SYSCALL_ENTRY(getgid),
	SYSCALL_ENTRY(getuid),
	SYSCALL_ENTRY(mmap),
	SYSCALL_ENTRY(setgid),
	SYSCALL_ENTRY(setuid),
	/*
	 * chown(2), lchown(2), and select(2) are deprecated and not wired up
	 * on ARM64.
	 */
#ifndef CONFIG_ARM64
	SYSCALL_ENTRY(select),
#endif

	/* X86_64-specific syscalls. */
#ifdef CONFIG_X86_64
	SYSCALL_ENTRY(arch_prctl),
#endif
}; /* end third_party_whitelist */

#ifdef CONFIG_COMPAT
static struct syscall_whitelist_entry third_party_compat_whitelist[] = {
	COMPAT_SYSCALL_ENTRY(access),
	COMPAT_SYSCALL_ENTRY(brk),
	COMPAT_SYSCALL_ENTRY(chdir),
	COMPAT_SYSCALL_ENTRY(clock_gettime),
	COMPAT_SYSCALL_ENTRY(clone),
	COMPAT_SYSCALL_ENTRY(close),
	COMPAT_SYSCALL_ENTRY(creat),
	COMPAT_SYSCALL_ENTRY(dup),
	COMPAT_SYSCALL_ENTRY(dup2),
	COMPAT_SYSCALL_ENTRY(execve),
	COMPAT_SYSCALL_ENTRY(exit),
	COMPAT_SYSCALL_ENTRY(exit_group),
	COMPAT_SYSCALL_ENTRY(fcntl),
	COMPAT_SYSCALL_ENTRY(fcntl64),
	COMPAT_SYSCALL_ENTRY(fstat),
	COMPAT_SYSCALL_ENTRY(fstat64),
	COMPAT_SYSCALL_ENTRY(futex),
	COMPAT_SYSCALL_ENTRY(getcwd),
	COMPAT_SYSCALL_ENTRY(getdents),
	COMPAT_SYSCALL_ENTRY(getdents64),
	COMPAT_SYSCALL_ENTRY(getegid),
	COMPAT_SYSCALL_ENTRY(geteuid),
	COMPAT_SYSCALL_ENTRY(geteuid32),
	COMPAT_SYSCALL_ENTRY(getgid),
	COMPAT_SYSCALL_ENTRY(getpgid),
	COMPAT_SYSCALL_ENTRY(getpgrp),
	COMPAT_SYSCALL_ENTRY(getpid),
	COMPAT_SYSCALL_ENTRY(getpriority),
	COMPAT_SYSCALL_ENTRY(getppid),
	COMPAT_SYSCALL_ENTRY(getsid),
	COMPAT_SYSCALL_ENTRY(gettimeofday),
	COMPAT_SYSCALL_ENTRY(getuid),
	COMPAT_SYSCALL_ENTRY(getuid32),
	COMPAT_SYSCALL_ENTRY(ioctl),
	COMPAT_SYSCALL_ENTRY(_llseek),
	COMPAT_SYSCALL_ENTRY(lseek),
	COMPAT_SYSCALL_ENTRY(lstat),
	COMPAT_SYSCALL_ENTRY(lstat64),
	COMPAT_SYSCALL_ENTRY(madvise),
        COMPAT_SYSCALL_ENTRY(memfd_create),
	COMPAT_SYSCALL_ENTRY(mkdir),
	COMPAT_SYSCALL_ENTRY(mmap2),
	COMPAT_SYSCALL_ENTRY(mprotect),
	COMPAT_SYSCALL_ENTRY(munmap),
	COMPAT_SYSCALL_ENTRY(nanosleep),
	COMPAT_SYSCALL_ENTRY(_newselect),
	COMPAT_SYSCALL_ENTRY(open),
	COMPAT_SYSCALL_ENTRY(openat),
	COMPAT_SYSCALL_ENTRY(pipe),
	COMPAT_SYSCALL_ENTRY(poll),
	COMPAT_SYSCALL_ENTRY(prlimit64),
	COMPAT_SYSCALL_ENTRY(read),
	COMPAT_SYSCALL_ENTRY(readlink),
	COMPAT_SYSCALL_ENTRY(rt_sigaction),
	COMPAT_SYSCALL_ENTRY(rt_sigprocmask),
	COMPAT_SYSCALL_ENTRY(rt_sigreturn),
	COMPAT_SYSCALL_ENTRY(sendfile),
	COMPAT_SYSCALL_ENTRY(set_robust_list),
	COMPAT_SYSCALL_ENTRY(set_tid_address),
	COMPAT_SYSCALL_ENTRY(setgid32),
	COMPAT_SYSCALL_ENTRY(setuid32),
	COMPAT_SYSCALL_ENTRY(setpgid),
	COMPAT_SYSCALL_ENTRY(setpriority),
	COMPAT_SYSCALL_ENTRY(setsid),
	COMPAT_SYSCALL_ENTRY(stat),
	COMPAT_SYSCALL_ENTRY(stat64),
	COMPAT_SYSCALL_ENTRY(statfs),
	COMPAT_SYSCALL_ENTRY(syslog),
	COMPAT_SYSCALL_ENTRY(ugetrlimit),
	COMPAT_SYSCALL_ENTRY(umask),
	COMPAT_SYSCALL_ENTRY(uname),
	COMPAT_SYSCALL_ENTRY(unlink),
	COMPAT_SYSCALL_ENTRY(wait4),
	COMPAT_SYSCALL_ENTRY(write),
	COMPAT_SYSCALL_ENTRY(writev),

	/* IA32 uses the common socketcall(2) entrypoint for socket calls. */
#ifdef CONFIG_X86_64
	COMPAT_SYSCALL_ENTRY(socketcall),
#endif

#ifdef CONFIG_ARM64
	COMPAT_SYSCALL_ENTRY(accept),
	COMPAT_SYSCALL_ENTRY(bind),
	COMPAT_SYSCALL_ENTRY(connect),
	COMPAT_SYSCALL_ENTRY(listen),
	COMPAT_SYSCALL_ENTRY(recvfrom),
	COMPAT_SYSCALL_ENTRY(recvmsg),
	COMPAT_SYSCALL_ENTRY(sendmsg),
	COMPAT_SYSCALL_ENTRY(sendto),
	COMPAT_SYSCALL_ENTRY(setsockopt),
	COMPAT_SYSCALL_ENTRY(socket),
	COMPAT_SYSCALL_ENTRY(socketpair),
#endif

	/*
	 * getrlimit(2) is deprecated and not wired in the ARM compat table
	 * on ARM64.
	 */
#ifndef CONFIG_ARM64
	COMPAT_SYSCALL_ENTRY(getrlimit),
#endif

}; /* end third_party_compat_whitelist */
#endif /* CONFIG_COMPAT */

#endif /* THIRD_PARTY_WHITELISTS_H */
