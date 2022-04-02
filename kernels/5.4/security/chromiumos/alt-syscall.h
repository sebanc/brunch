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

#ifndef ALT_SYSCALL_H
#define ALT_SYSCALL_H

/*
 * NOTE: this file uses the 'static' keyword for variable and function
 * definitions because alt-syscall.c is the only .c file that is expected to
 * include this header. Definitions were pulled out from alt-syscall.c into
 * this header and the *_whitelists.h headers for the sake of readability.
 */

static int allow_devmode_syscalls;

#ifdef CONFIG_SYSCTL
static int zero;
static int one = 1;

static struct ctl_path chromiumos_sysctl_path[] = {
        { .procname = "kernel", },
        { .procname = "chromiumos", },
        { .procname = "alt_syscall", },
        { }
};

static struct ctl_table chromiumos_sysctl_table[] = {
        {
                .procname       = "allow_devmode_syscalls",
                .data           = &allow_devmode_syscalls,
                .maxlen         = sizeof(int),
                .mode           = 0644,
                .proc_handler   = proc_dointvec_minmax,
                .extra1         = &zero,
                .extra2         = &one,
        },
        { }
};
#endif

struct syscall_whitelist_entry {
        unsigned int nr;
        sys_call_ptr_t alt;
};

struct syscall_whitelist {
        const char *name;
        const struct syscall_whitelist_entry *whitelist;
        unsigned int nr_whitelist;
#ifdef CONFIG_COMPAT
        const struct syscall_whitelist_entry *compat_whitelist;
        unsigned int nr_compat_whitelist;
#endif
        bool permissive;
};

static struct alt_sys_call_table default_table;

#define SYSCALL_ENTRY_ALT(name, func)                                   \
        {                                                               \
                .nr = __NR_ ## name,                                    \
                .alt = (sys_call_ptr_t)func,                            \
        }
#define SYSCALL_ENTRY(name) SYSCALL_ENTRY_ALT(name, NULL)
#define COMPAT_SYSCALL_ENTRY_ALT(name, func)                            \
        {                                                               \
                .nr = __NR_compat_ ## name,                             \
                .alt = (sys_call_ptr_t)func,                            \
        }
#define COMPAT_SYSCALL_ENTRY(name) COMPAT_SYSCALL_ENTRY_ALT(name, NULL)

/*
 * If an alt_syscall table allows prctl(), override it to prevent a process
 * from changing its syscall table.
 */
static asmlinkage long alt_sys_prctl(struct pt_regs *regs);

#ifdef CONFIG_COMPAT
#define SYSCALL_WHITELIST_COMPAT(x)                                     \
        .compat_whitelist = x ## _compat_whitelist,                     \
        .nr_compat_whitelist = ARRAY_SIZE(x ## _compat_whitelist),
#else
#define SYSCALL_WHITELIST_COMPAT(x)
#endif

#define SYSCALL_WHITELIST(x)                                            \
        {                                                               \
                .name = #x,                                             \
                .whitelist = x ## _whitelist,                           \
                .nr_whitelist = ARRAY_SIZE(x ## _whitelist),            \
                SYSCALL_WHITELIST_COMPAT(x)                             \
        }

#define PERMISSIVE_SYSCALL_WHITELIST(x)                                 \
        {                                                               \
                .name = #x "_permissive",                               \
                .permissive = true,                                     \
                .whitelist = x ## _whitelist,                           \
                .nr_whitelist = ARRAY_SIZE(x ## _whitelist),            \
                SYSCALL_WHITELIST_COMPAT(x)                             \
        }

#ifdef CONFIG_COMPAT
#ifdef CONFIG_X86_64
#define __NR_compat_access      __NR_ia32_access
#define __NR_compat_adjtimex    __NR_ia32_adjtimex
#define __NR_compat_brk __NR_ia32_brk
#define __NR_compat_capget      __NR_ia32_capget
#define __NR_compat_capset      __NR_ia32_capset
#define __NR_compat_chdir       __NR_ia32_chdir
#define __NR_compat_chmod       __NR_ia32_chmod
#define __NR_compat_clock_adjtime       __NR_ia32_clock_adjtime
#define __NR_compat_clock_getres        __NR_ia32_clock_getres
#define __NR_compat_clock_gettime       __NR_ia32_clock_gettime
#define __NR_compat_clock_nanosleep     __NR_ia32_clock_nanosleep
#define __NR_compat_clock_settime       __NR_ia32_clock_settime
#define __NR_compat_clone       __NR_ia32_clone
#define __NR_compat_close       __NR_ia32_close
#define __NR_compat_creat       __NR_ia32_creat
#define __NR_compat_dup __NR_ia32_dup
#define __NR_compat_dup2        __NR_ia32_dup2
#define __NR_compat_dup3        __NR_ia32_dup3
#define __NR_compat_epoll_create        __NR_ia32_epoll_create
#define __NR_compat_epoll_create1       __NR_ia32_epoll_create1
#define __NR_compat_epoll_ctl   __NR_ia32_epoll_ctl
#define __NR_compat_epoll_wait  __NR_ia32_epoll_wait
#define __NR_compat_epoll_pwait __NR_ia32_epoll_pwait
#define __NR_compat_eventfd     __NR_ia32_eventfd
#define __NR_compat_eventfd2    __NR_ia32_eventfd2
#define __NR_compat_execve      __NR_ia32_execve
#define __NR_compat_exit        __NR_ia32_exit
#define __NR_compat_exit_group  __NR_ia32_exit_group
#define __NR_compat_faccessat   __NR_ia32_faccessat
#define __NR_compat_fallocate   __NR_ia32_fallocate
#define __NR_compat_fchdir      __NR_ia32_fchdir
#define __NR_compat_fchmod      __NR_ia32_fchmod
#define __NR_compat_fchmodat    __NR_ia32_fchmodat
#define __NR_compat_fchown      __NR_ia32_fchown
#define __NR_compat_fchownat    __NR_ia32_fchownat
#define __NR_compat_fcntl       __NR_ia32_fcntl
#define __NR_compat_fdatasync   __NR_ia32_fdatasync
#define __NR_compat_fgetxattr   __NR_ia32_fgetxattr
#define __NR_compat_flistxattr  __NR_ia32_flistxattr
#define __NR_compat_flock       __NR_ia32_flock
#define __NR_compat_fork        __NR_ia32_fork
#define __NR_compat_fremovexattr        __NR_ia32_fremovexattr
#define __NR_compat_fsetxattr   __NR_ia32_fsetxattr
#define __NR_compat_fstat       __NR_ia32_fstat
#define __NR_compat_fstatfs     __NR_ia32_fstatfs
#define __NR_compat_fsync       __NR_ia32_fsync
#define __NR_compat_ftruncate   __NR_ia32_ftruncate
#define __NR_compat_futex       __NR_ia32_futex
#define __NR_compat_futimesat   __NR_ia32_futimesat
#define __NR_compat_getcpu      __NR_ia32_getcpu
#define __NR_compat_getcwd      __NR_ia32_getcwd
#define __NR_compat_getdents    __NR_ia32_getdents
#define __NR_compat_getdents64  __NR_ia32_getdents64
#define __NR_compat_getegid     __NR_ia32_getegid
#define __NR_compat_geteuid     __NR_ia32_geteuid
#define __NR_compat_getgid      __NR_ia32_getgid
#define __NR_compat_getgroups32 __NR_ia32_getgroups32
#define __NR_compat_getpgid     __NR_ia32_getpgid
#define __NR_compat_getpgrp     __NR_ia32_getpgrp
#define __NR_compat_getpid      __NR_ia32_getpid
#define __NR_compat_getppid     __NR_ia32_getppid
#define __NR_compat_getpriority __NR_ia32_getpriority
#define __NR_compat_getrandom   __NR_ia32_getrandom
#define __NR_compat_getresgid   __NR_ia32_getresgid
#define __NR_compat_getresuid   __NR_ia32_getresuid
#define __NR_compat_getrlimit   __NR_ia32_getrlimit
#define __NR_compat_getrusage   __NR_ia32_getrusage
#define __NR_compat_getsid      __NR_ia32_getsid
#define __NR_compat_gettid      __NR_ia32_gettid
#define __NR_compat_gettimeofday        __NR_ia32_gettimeofday
#define __NR_compat_getuid      __NR_ia32_getuid
#define __NR_compat_getxattr    __NR_ia32_getxattr
#define __NR_compat_inotify_add_watch   __NR_ia32_inotify_add_watch
#define __NR_compat_inotify_init        __NR_ia32_inotify_init
#define __NR_compat_inotify_init1       __NR_ia32_inotify_init1
#define __NR_compat_inotify_rm_watch    __NR_ia32_inotify_rm_watch
#define __NR_compat_ioctl       __NR_ia32_ioctl
#define __NR_compat_io_destroy  __NR_ia32_io_destroy
#define __NR_compat_io_getevents      __NR_ia32_io_getevents
#define __NR_compat_io_setup  __NR_ia32_io_setup
#define __NR_compat_io_submit __NR_ia32_io_submit
#define __NR_compat_ioprio_set  __NR_ia32_ioprio_set
#define __NR_compat_keyctl      __NR_ia32_keyctl
#define __NR_compat_kill        __NR_ia32_kill
#define __NR_compat_lgetxattr   __NR_ia32_lgetxattr
#define __NR_compat_link        __NR_ia32_link
#define __NR_compat_linkat      __NR_ia32_linkat
#define __NR_compat_listxattr   __NR_ia32_listxattr
#define __NR_compat_llistxattr  __NR_ia32_llistxattr
#define __NR_compat_lremovexattr        __NR_ia32_lremovexattr
#define __NR_compat_lseek       __NR_ia32_lseek
#define __NR_compat_lsetxattr   __NR_ia32_lsetxattr
#define __NR_compat_lstat       __NR_ia32_lstat
#define __NR_compat_madvise     __NR_ia32_madvise
#define __NR_compat_memfd_create        __NR_ia32_memfd_create
#define __NR_compat_mincore     __NR_ia32_mincore
#define __NR_compat_mkdir       __NR_ia32_mkdir
#define __NR_compat_mkdirat     __NR_ia32_mkdirat
#define __NR_compat_mknod       __NR_ia32_mknod
#define __NR_compat_mknodat     __NR_ia32_mknodat
#define __NR_compat_mlock       __NR_ia32_mlock
#define __NR_compat_munlock     __NR_ia32_munlock
#define __NR_compat_mlockall    __NR_ia32_mlockall
#define __NR_compat_munlockall  __NR_ia32_munlockall
#define __NR_compat_modify_ldt  __NR_ia32_modify_ldt
#define __NR_compat_mount       __NR_ia32_mount
#define __NR_compat_mprotect    __NR_ia32_mprotect
#define __NR_compat_mremap      __NR_ia32_mremap
#define __NR_compat_msync       __NR_ia32_msync
#define __NR_compat_munmap      __NR_ia32_munmap
#define __NR_compat_name_to_handle_at   __NR_ia32_name_to_handle_at
#define __NR_compat_nanosleep   __NR_ia32_nanosleep
#define __NR_compat_open        __NR_ia32_open
#define __NR_compat_open_by_handle_at   __NR_ia32_open_by_handle_at
#define __NR_compat_openat      __NR_ia32_openat
#define __NR_compat_perf_event_open     __NR_ia32_perf_event_open
#define __NR_compat_personality __NR_ia32_personality
#define __NR_compat_pipe        __NR_ia32_pipe
#define __NR_compat_pipe2       __NR_ia32_pipe2
#define __NR_compat_poll        __NR_ia32_poll
#define __NR_compat_ppoll       __NR_ia32_ppoll
#define __NR_compat_prctl       __NR_ia32_prctl
#define __NR_compat_pread64     __NR_ia32_pread64
#define __NR_compat_preadv      __NR_ia32_preadv
#define __NR_compat_prlimit64   __NR_ia32_prlimit64
#define __NR_compat_process_vm_readv    __NR_ia32_process_vm_readv
#define __NR_compat_process_vm_writev   __NR_ia32_process_vm_writev
#define __NR_compat_pselect6    __NR_ia32_pselect6
#define __NR_compat_ptrace      __NR_ia32_ptrace
#define __NR_compat_pwrite64    __NR_ia32_pwrite64
#define __NR_compat_pwritev     __NR_ia32_pwritev
#define __NR_compat_read        __NR_ia32_read
#define __NR_compat_readahead   __NR_ia32_readahead
#define __NR_compat_readv       __NR_ia32_readv
#define __NR_compat_readlink    __NR_ia32_readlink
#define __NR_compat_readlinkat  __NR_ia32_readlinkat
#define __NR_compat_recvmmsg    __NR_ia32_recvmmsg
#define __NR_compat_remap_file_pages    __NR_ia32_remap_file_pages
#define __NR_compat_removexattr __NR_ia32_removexattr
#define __NR_compat_rename      __NR_ia32_rename
#define __NR_compat_renameat    __NR_ia32_renameat
#define __NR_compat_restart_syscall     __NR_ia32_restart_syscall
#define __NR_compat_rmdir       __NR_ia32_rmdir
#define __NR_compat_rt_sigaction        __NR_ia32_rt_sigaction
#define __NR_compat_rt_sigpending       __NR_ia32_rt_sigpending
#define __NR_compat_rt_sigprocmask      __NR_ia32_rt_sigprocmask
#define __NR_compat_rt_sigqueueinfo     __NR_ia32_rt_sigqueueinfo
#define __NR_compat_rt_sigreturn        __NR_ia32_rt_sigreturn
#define __NR_compat_rt_sigsuspend       __NR_ia32_rt_sigsuspend
#define __NR_compat_rt_sigtimedwait     __NR_ia32_rt_sigtimedwait
#define __NR_compat_rt_tgsigqueueinfo   __NR_ia32_rt_tgsigqueueinfo
#define __NR_compat_sched_get_priority_max      __NR_ia32_sched_get_priority_max
#define __NR_compat_sched_get_priority_min      __NR_ia32_sched_get_priority_min
#define __NR_compat_sched_getaffinity   __NR_ia32_sched_getaffinity
#define __NR_compat_sched_getparam      __NR_ia32_sched_getparam
#define __NR_compat_sched_getscheduler  __NR_ia32_sched_getscheduler
#define __NR_compat_sched_setaffinity   __NR_ia32_sched_setaffinity
#define __NR_compat_sched_setparam      __NR_ia32_sched_setparam
#define __NR_compat_sched_setscheduler  __NR_ia32_sched_setscheduler
#define __NR_compat_sched_yield __NR_ia32_sched_yield
#define __NR_compat_seccomp     __NR_ia32_seccomp
#define __NR_compat_sendfile    __NR_ia32_sendfile
#define __NR_compat_sendfile64  __NR_ia32_sendfile64
#define __NR_compat_sendmmsg    __NR_ia32_sendmmsg
#define __NR_compat_setdomainname       __NR_ia32_setdomainname
#define __NR_compat_set_robust_list     __NR_ia32_set_robust_list
#define __NR_compat_set_tid_address     __NR_ia32_set_tid_address
#define __NR_compat_set_thread_area     __NR_ia32_set_thread_area
#define __NR_compat_setgid      __NR_ia32_setgid
#define __NR_compat_setgroups   __NR_ia32_setgroups
#define __NR_compat_setitimer   __NR_ia32_setitimer
#define __NR_compat_setns       __NR_ia32_setns
#define __NR_compat_setpgid     __NR_ia32_setpgid
#define __NR_compat_setpriority __NR_ia32_setpriority
#define __NR_compat_setregid    __NR_ia32_setregid
#define __NR_compat_setresgid   __NR_ia32_setresgid
#define __NR_compat_setresuid   __NR_ia32_setresuid
#define __NR_compat_setrlimit   __NR_ia32_setrlimit
#define __NR_compat_setsid      __NR_ia32_setsid
#define __NR_compat_settimeofday        __NR_ia32_settimeofday
#define __NR_compat_setuid      __NR_ia32_setuid
#define __NR_compat_setxattr    __NR_ia32_setxattr
#define __NR_compat_signalfd4   __NR_ia32_signalfd4
#define __NR_compat_sigaltstack __NR_ia32_sigaltstack
#define __NR_compat_socketcall  __NR_ia32_socketcall
#define __NR_compat_splice      __NR_ia32_splice
#define __NR_compat_stat        __NR_ia32_stat
#define __NR_compat_statfs      __NR_ia32_statfs
#define __NR_compat_symlink     __NR_ia32_symlink
#define __NR_compat_symlinkat   __NR_ia32_symlinkat
#define __NR_compat_sync        __NR_ia32_sync
#define __NR_compat_syncfs      __NR_ia32_syncfs
#define __NR_compat_sync_file_range     __NR_ia32_sync_file_range
#define __NR_compat_sysinfo     __NR_ia32_sysinfo
#define __NR_compat_syslog      __NR_ia32_syslog
#define __NR_compat_tee         __NR_ia32_tee
#define __NR_compat_tgkill      __NR_ia32_tgkill
#define __NR_compat_tkill       __NR_ia32_tkill
#define __NR_compat_time        __NR_ia32_time
#define __NR_compat_timer_create        __NR_ia32_timer_create
#define __NR_compat_timer_delete        __NR_ia32_timer_delete
#define __NR_compat_timer_getoverrun    __NR_ia32_timer_getoverrun
#define __NR_compat_timer_gettime       __NR_ia32_timer_gettime
#define __NR_compat_timer_settime       __NR_ia32_timer_settime
#define __NR_compat_timerfd_create      __NR_ia32_timerfd_create
#define __NR_compat_timerfd_gettime     __NR_ia32_timerfd_gettime
#define __NR_compat_timerfd_settime     __NR_ia32_timerfd_settime
#define __NR_compat_times               __NR_ia32_times
#define __NR_compat_truncate    __NR_ia32_truncate
#define __NR_compat_umask       __NR_ia32_umask
#define __NR_compat_umount2     __NR_ia32_umount2
#define __NR_compat_uname       __NR_ia32_uname
#define __NR_compat_unlink      __NR_ia32_unlink
#define __NR_compat_unlinkat    __NR_ia32_unlinkat
#define __NR_compat_unshare     __NR_ia32_unshare
#define __NR_compat_ustat       __NR_ia32_ustat
#define __NR_compat_utimensat   __NR_ia32_utimensat
#define __NR_compat_utimes      __NR_ia32_utimes
#define __NR_compat_vfork       __NR_ia32_vfork
#define __NR_compat_vmsplice    __NR_ia32_vmsplice
#define __NR_compat_wait4       __NR_ia32_wait4
#define __NR_compat_waitid      __NR_ia32_waitid
#define __NR_compat_waitpid     __NR_ia32_waitpid
#define __NR_compat_write       __NR_ia32_write
#define __NR_compat_writev      __NR_ia32_writev
#define __NR_compat_chown32     __NR_ia32_chown32
#define __NR_compat_fadvise64   __NR_ia32_fadvise64
#define __NR_compat_fadvise64_64        __NR_ia32_fadvise64_64
#define __NR_compat_fchown32    __NR_ia32_fchown32
#define __NR_compat_fcntl64     __NR_ia32_fcntl64
#define __NR_compat_fstat64     __NR_ia32_fstat64
#define __NR_compat_fstatat64   __NR_ia32_fstatat64
#define __NR_compat_fstatfs64   __NR_ia32_fstatfs64
#define __NR_compat_ftruncate64 __NR_ia32_ftruncate64
#define __NR_compat_getegid32   __NR_ia32_getegid32
#define __NR_compat_geteuid32   __NR_ia32_geteuid32
#define __NR_compat_getgid32    __NR_ia32_getgid32
#define __NR_compat_getresgid32 __NR_ia32_getresgid32
#define __NR_compat_getresuid32 __NR_ia32_getresuid32
#define __NR_compat_getuid32    __NR_ia32_getuid32
#define __NR_compat_lchown32    __NR_ia32_lchown32
#define __NR_compat_lstat64     __NR_ia32_lstat64
#define __NR_compat_mmap2       __NR_ia32_mmap2
#define __NR_compat__newselect  __NR_ia32__newselect
#define __NR_compat__llseek     __NR_ia32__llseek
#define __NR_compat_sigaction   __NR_ia32_sigaction
#define __NR_compat_sigpending  __NR_ia32_sigpending
#define __NR_compat_sigprocmask __NR_ia32_sigprocmask
#define __NR_compat_sigreturn   __NR_ia32_sigreturn
#define __NR_compat_sigsuspend  __NR_ia32_sigsuspend
#define __NR_compat_setgid32    __NR_ia32_setgid32
#define __NR_compat_setgroups32 __NR_ia32_setgroups32
#define __NR_compat_setregid32  __NR_ia32_setregid32
#define __NR_compat_setresgid32 __NR_ia32_setresgid32
#define __NR_compat_setresuid32 __NR_ia32_setresuid32
#define __NR_compat_setreuid32  __NR_ia32_setreuid32
#define __NR_compat_setuid32    __NR_ia32_setuid32
#define __NR_compat_stat64      __NR_ia32_stat64
#define __NR_compat_statfs64    __NR_ia32_statfs64
#define __NR_compat_truncate64  __NR_ia32_truncate64
#define __NR_compat_ugetrlimit  __NR_ia32_ugetrlimit
#endif
#endif

#endif /* ALT_SYSCALL_H */
