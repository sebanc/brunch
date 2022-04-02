#ifdef CONFIG_X86_32
__SYSCALL_I386(0, sys_restart_syscall, )
#else
__SYSCALL_I386(0, __ia32_sys_restart_syscall, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(1, sys_exit, )
#else
__SYSCALL_I386(1, __ia32_sys_exit, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(2, sys_fork, )
#else
__SYSCALL_I386(2, __ia32_sys_fork, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(3, sys_read, )
#else
__SYSCALL_I386(3, __ia32_sys_read, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(4, sys_write, )
#else
__SYSCALL_I386(4, __ia32_sys_write, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(5, sys_open, )
#else
__SYSCALL_I386(5, __ia32_compat_sys_open, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(6, sys_close, )
#else
__SYSCALL_I386(6, __ia32_sys_close, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(7, sys_waitpid, )
#else
__SYSCALL_I386(7, __ia32_sys_waitpid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(8, sys_creat, )
#else
__SYSCALL_I386(8, __ia32_sys_creat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(9, sys_link, )
#else
__SYSCALL_I386(9, __ia32_sys_link, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(10, sys_unlink, )
#else
__SYSCALL_I386(10, __ia32_sys_unlink, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(11, sys_execve, )
#else
__SYSCALL_I386(11, __ia32_compat_sys_execve, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(12, sys_chdir, )
#else
__SYSCALL_I386(12, __ia32_sys_chdir, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(13, sys_time32, )
#else
__SYSCALL_I386(13, __ia32_sys_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(14, sys_mknod, )
#else
__SYSCALL_I386(14, __ia32_sys_mknod, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(15, sys_chmod, )
#else
__SYSCALL_I386(15, __ia32_sys_chmod, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(16, sys_lchown16, )
#else
__SYSCALL_I386(16, __ia32_sys_lchown16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(18, sys_stat, )
#else
__SYSCALL_I386(18, __ia32_sys_stat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(19, sys_lseek, )
#else
__SYSCALL_I386(19, __ia32_compat_sys_lseek, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(20, sys_getpid, )
#else
__SYSCALL_I386(20, __ia32_sys_getpid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(21, sys_mount, )
#else
__SYSCALL_I386(21, __ia32_compat_sys_mount, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(22, sys_oldumount, )
#else
__SYSCALL_I386(22, __ia32_sys_oldumount, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(23, sys_setuid16, )
#else
__SYSCALL_I386(23, __ia32_sys_setuid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(24, sys_getuid16, )
#else
__SYSCALL_I386(24, __ia32_sys_getuid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(25, sys_stime32, )
#else
__SYSCALL_I386(25, __ia32_sys_stime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(26, sys_ptrace, )
#else
__SYSCALL_I386(26, __ia32_compat_sys_ptrace, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(27, sys_alarm, )
#else
__SYSCALL_I386(27, __ia32_sys_alarm, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(28, sys_fstat, )
#else
__SYSCALL_I386(28, __ia32_sys_fstat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(29, sys_pause, )
#else
__SYSCALL_I386(29, __ia32_sys_pause, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(30, sys_utime32, )
#else
__SYSCALL_I386(30, __ia32_sys_utime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(33, sys_access, )
#else
__SYSCALL_I386(33, __ia32_sys_access, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(34, sys_nice, )
#else
__SYSCALL_I386(34, __ia32_sys_nice, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(36, sys_sync, )
#else
__SYSCALL_I386(36, __ia32_sys_sync, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(37, sys_kill, )
#else
__SYSCALL_I386(37, __ia32_sys_kill, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(38, sys_rename, )
#else
__SYSCALL_I386(38, __ia32_sys_rename, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(39, sys_mkdir, )
#else
__SYSCALL_I386(39, __ia32_sys_mkdir, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(40, sys_rmdir, )
#else
__SYSCALL_I386(40, __ia32_sys_rmdir, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(41, sys_dup, )
#else
__SYSCALL_I386(41, __ia32_sys_dup, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(42, sys_pipe, )
#else
__SYSCALL_I386(42, __ia32_sys_pipe, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(43, sys_times, )
#else
__SYSCALL_I386(43, __ia32_compat_sys_times, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(45, sys_brk, )
#else
__SYSCALL_I386(45, __ia32_sys_brk, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(46, sys_setgid16, )
#else
__SYSCALL_I386(46, __ia32_sys_setgid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(47, sys_getgid16, )
#else
__SYSCALL_I386(47, __ia32_sys_getgid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(48, sys_signal, )
#else
__SYSCALL_I386(48, __ia32_sys_signal, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(49, sys_geteuid16, )
#else
__SYSCALL_I386(49, __ia32_sys_geteuid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(50, sys_getegid16, )
#else
__SYSCALL_I386(50, __ia32_sys_getegid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(51, sys_acct, )
#else
__SYSCALL_I386(51, __ia32_sys_acct, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(52, sys_umount, )
#else
__SYSCALL_I386(52, __ia32_sys_umount, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(54, sys_ioctl, )
#else
__SYSCALL_I386(54, __ia32_compat_sys_ioctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(55, sys_fcntl, )
#else
__SYSCALL_I386(55, __ia32_compat_sys_fcntl64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(57, sys_setpgid, )
#else
__SYSCALL_I386(57, __ia32_sys_setpgid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(59, sys_olduname, )
#else
__SYSCALL_I386(59, __ia32_sys_olduname, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(60, sys_umask, )
#else
__SYSCALL_I386(60, __ia32_sys_umask, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(61, sys_chroot, )
#else
__SYSCALL_I386(61, __ia32_sys_chroot, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(62, sys_ustat, )
#else
__SYSCALL_I386(62, __ia32_compat_sys_ustat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(63, sys_dup2, )
#else
__SYSCALL_I386(63, __ia32_sys_dup2, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(64, sys_getppid, )
#else
__SYSCALL_I386(64, __ia32_sys_getppid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(65, sys_getpgrp, )
#else
__SYSCALL_I386(65, __ia32_sys_getpgrp, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(66, sys_setsid, )
#else
__SYSCALL_I386(66, __ia32_sys_setsid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(67, sys_sigaction, )
#else
__SYSCALL_I386(67, __ia32_compat_sys_sigaction, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(68, sys_sgetmask, )
#else
__SYSCALL_I386(68, __ia32_sys_sgetmask, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(69, sys_ssetmask, )
#else
__SYSCALL_I386(69, __ia32_sys_ssetmask, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(70, sys_setreuid16, )
#else
__SYSCALL_I386(70, __ia32_sys_setreuid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(71, sys_setregid16, )
#else
__SYSCALL_I386(71, __ia32_sys_setregid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(72, sys_sigsuspend, )
#else
__SYSCALL_I386(72, __ia32_sys_sigsuspend, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(73, sys_sigpending, )
#else
__SYSCALL_I386(73, __ia32_compat_sys_sigpending, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(74, sys_sethostname, )
#else
__SYSCALL_I386(74, __ia32_sys_sethostname, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(75, sys_setrlimit, )
#else
__SYSCALL_I386(75, __ia32_compat_sys_setrlimit, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(76, sys_old_getrlimit, )
#else
__SYSCALL_I386(76, __ia32_compat_sys_old_getrlimit, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(77, sys_getrusage, )
#else
__SYSCALL_I386(77, __ia32_compat_sys_getrusage, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(78, sys_gettimeofday, )
#else
__SYSCALL_I386(78, __ia32_compat_sys_gettimeofday, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(79, sys_settimeofday, )
#else
__SYSCALL_I386(79, __ia32_compat_sys_settimeofday, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(80, sys_getgroups16, )
#else
__SYSCALL_I386(80, __ia32_sys_getgroups16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(81, sys_setgroups16, )
#else
__SYSCALL_I386(81, __ia32_sys_setgroups16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(82, sys_old_select, )
#else
__SYSCALL_I386(82, __ia32_compat_sys_old_select, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(83, sys_symlink, )
#else
__SYSCALL_I386(83, __ia32_sys_symlink, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(84, sys_lstat, )
#else
__SYSCALL_I386(84, __ia32_sys_lstat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(85, sys_readlink, )
#else
__SYSCALL_I386(85, __ia32_sys_readlink, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(86, sys_uselib, )
#else
__SYSCALL_I386(86, __ia32_sys_uselib, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(87, sys_swapon, )
#else
__SYSCALL_I386(87, __ia32_sys_swapon, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(88, sys_reboot, )
#else
__SYSCALL_I386(88, __ia32_sys_reboot, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(89, sys_old_readdir, )
#else
__SYSCALL_I386(89, __ia32_compat_sys_old_readdir, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(90, sys_old_mmap, )
#else
__SYSCALL_I386(90, __ia32_compat_sys_x86_mmap, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(91, sys_munmap, )
#else
__SYSCALL_I386(91, __ia32_sys_munmap, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(92, sys_truncate, )
#else
__SYSCALL_I386(92, __ia32_compat_sys_truncate, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(93, sys_ftruncate, )
#else
__SYSCALL_I386(93, __ia32_compat_sys_ftruncate, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(94, sys_fchmod, )
#else
__SYSCALL_I386(94, __ia32_sys_fchmod, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(95, sys_fchown16, )
#else
__SYSCALL_I386(95, __ia32_sys_fchown16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(96, sys_getpriority, )
#else
__SYSCALL_I386(96, __ia32_sys_getpriority, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(97, sys_setpriority, )
#else
__SYSCALL_I386(97, __ia32_sys_setpriority, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(99, sys_statfs, )
#else
__SYSCALL_I386(99, __ia32_compat_sys_statfs, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(100, sys_fstatfs, )
#else
__SYSCALL_I386(100, __ia32_compat_sys_fstatfs, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(101, sys_ioperm, )
#else
__SYSCALL_I386(101, __ia32_sys_ioperm, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(102, sys_socketcall, )
#else
__SYSCALL_I386(102, __ia32_compat_sys_socketcall, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(103, sys_syslog, )
#else
__SYSCALL_I386(103, __ia32_sys_syslog, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(104, sys_setitimer, )
#else
__SYSCALL_I386(104, __ia32_compat_sys_setitimer, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(105, sys_getitimer, )
#else
__SYSCALL_I386(105, __ia32_compat_sys_getitimer, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(106, sys_newstat, )
#else
__SYSCALL_I386(106, __ia32_compat_sys_newstat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(107, sys_newlstat, )
#else
__SYSCALL_I386(107, __ia32_compat_sys_newlstat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(108, sys_newfstat, )
#else
__SYSCALL_I386(108, __ia32_compat_sys_newfstat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(109, sys_uname, )
#else
__SYSCALL_I386(109, __ia32_sys_uname, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(110, sys_iopl, )
#else
__SYSCALL_I386(110, __ia32_sys_iopl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(111, sys_vhangup, )
#else
__SYSCALL_I386(111, __ia32_sys_vhangup, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(113, sys_vm86old, )
#else
__SYSCALL_I386(113, __ia32_sys_ni_syscall, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(114, sys_wait4, )
#else
__SYSCALL_I386(114, __ia32_compat_sys_wait4, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(115, sys_swapoff, )
#else
__SYSCALL_I386(115, __ia32_sys_swapoff, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(116, sys_sysinfo, )
#else
__SYSCALL_I386(116, __ia32_compat_sys_sysinfo, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(117, sys_ipc, )
#else
__SYSCALL_I386(117, __ia32_compat_sys_ipc, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(118, sys_fsync, )
#else
__SYSCALL_I386(118, __ia32_sys_fsync, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(119, sys_sigreturn, )
#else
__SYSCALL_I386(119, __ia32_compat_sys_sigreturn, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(120, sys_clone, )
#else
__SYSCALL_I386(120, __ia32_compat_sys_x86_clone, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(121, sys_setdomainname, )
#else
__SYSCALL_I386(121, __ia32_sys_setdomainname, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(122, sys_newuname, )
#else
__SYSCALL_I386(122, __ia32_sys_newuname, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(123, sys_modify_ldt, )
#else
__SYSCALL_I386(123, __ia32_sys_modify_ldt, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(124, sys_adjtimex_time32, )
#else
__SYSCALL_I386(124, __ia32_sys_adjtimex_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(125, sys_mprotect, )
#else
__SYSCALL_I386(125, __ia32_sys_mprotect, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(126, sys_sigprocmask, )
#else
__SYSCALL_I386(126, __ia32_compat_sys_sigprocmask, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(128, sys_init_module, )
#else
__SYSCALL_I386(128, __ia32_sys_init_module, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(129, sys_delete_module, )
#else
__SYSCALL_I386(129, __ia32_sys_delete_module, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(131, sys_quotactl, )
#else
__SYSCALL_I386(131, __ia32_compat_sys_quotactl32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(132, sys_getpgid, )
#else
__SYSCALL_I386(132, __ia32_sys_getpgid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(133, sys_fchdir, )
#else
__SYSCALL_I386(133, __ia32_sys_fchdir, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(134, sys_bdflush, )
#else
__SYSCALL_I386(134, __ia32_sys_bdflush, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(135, sys_sysfs, )
#else
__SYSCALL_I386(135, __ia32_sys_sysfs, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(136, sys_personality, )
#else
__SYSCALL_I386(136, __ia32_sys_personality, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(138, sys_setfsuid16, )
#else
__SYSCALL_I386(138, __ia32_sys_setfsuid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(139, sys_setfsgid16, )
#else
__SYSCALL_I386(139, __ia32_sys_setfsgid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(140, sys_llseek, )
#else
__SYSCALL_I386(140, __ia32_sys_llseek, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(141, sys_getdents, )
#else
__SYSCALL_I386(141, __ia32_compat_sys_getdents, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(142, sys_select, )
#else
__SYSCALL_I386(142, __ia32_compat_sys_select, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(143, sys_flock, )
#else
__SYSCALL_I386(143, __ia32_sys_flock, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(144, sys_msync, )
#else
__SYSCALL_I386(144, __ia32_sys_msync, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(145, sys_readv, )
#else
__SYSCALL_I386(145, __ia32_compat_sys_readv, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(146, sys_writev, )
#else
__SYSCALL_I386(146, __ia32_compat_sys_writev, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(147, sys_getsid, )
#else
__SYSCALL_I386(147, __ia32_sys_getsid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(148, sys_fdatasync, )
#else
__SYSCALL_I386(148, __ia32_sys_fdatasync, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(149, sys_sysctl, )
#else
__SYSCALL_I386(149, __ia32_compat_sys_sysctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(150, sys_mlock, )
#else
__SYSCALL_I386(150, __ia32_sys_mlock, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(151, sys_munlock, )
#else
__SYSCALL_I386(151, __ia32_sys_munlock, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(152, sys_mlockall, )
#else
__SYSCALL_I386(152, __ia32_sys_mlockall, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(153, sys_munlockall, )
#else
__SYSCALL_I386(153, __ia32_sys_munlockall, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(154, sys_sched_setparam, )
#else
__SYSCALL_I386(154, __ia32_sys_sched_setparam, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(155, sys_sched_getparam, )
#else
__SYSCALL_I386(155, __ia32_sys_sched_getparam, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(156, sys_sched_setscheduler, )
#else
__SYSCALL_I386(156, __ia32_sys_sched_setscheduler, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(157, sys_sched_getscheduler, )
#else
__SYSCALL_I386(157, __ia32_sys_sched_getscheduler, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(158, sys_sched_yield, )
#else
__SYSCALL_I386(158, __ia32_sys_sched_yield, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(159, sys_sched_get_priority_max, )
#else
__SYSCALL_I386(159, __ia32_sys_sched_get_priority_max, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(160, sys_sched_get_priority_min, )
#else
__SYSCALL_I386(160, __ia32_sys_sched_get_priority_min, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(161, sys_sched_rr_get_interval_time32, )
#else
__SYSCALL_I386(161, __ia32_sys_sched_rr_get_interval_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(162, sys_nanosleep_time32, )
#else
__SYSCALL_I386(162, __ia32_sys_nanosleep_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(163, sys_mremap, )
#else
__SYSCALL_I386(163, __ia32_sys_mremap, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(164, sys_setresuid16, )
#else
__SYSCALL_I386(164, __ia32_sys_setresuid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(165, sys_getresuid16, )
#else
__SYSCALL_I386(165, __ia32_sys_getresuid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(166, sys_vm86, )
#else
__SYSCALL_I386(166, __ia32_sys_ni_syscall, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(168, sys_poll, )
#else
__SYSCALL_I386(168, __ia32_sys_poll, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(170, sys_setresgid16, )
#else
__SYSCALL_I386(170, __ia32_sys_setresgid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(171, sys_getresgid16, )
#else
__SYSCALL_I386(171, __ia32_sys_getresgid16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(172, sys_prctl, )
#else
__SYSCALL_I386(172, __ia32_sys_prctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(173, sys_rt_sigreturn, )
#else
__SYSCALL_I386(173, __ia32_compat_sys_rt_sigreturn, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(174, sys_rt_sigaction, )
#else
__SYSCALL_I386(174, __ia32_compat_sys_rt_sigaction, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(175, sys_rt_sigprocmask, )
#else
__SYSCALL_I386(175, __ia32_compat_sys_rt_sigprocmask, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(176, sys_rt_sigpending, )
#else
__SYSCALL_I386(176, __ia32_compat_sys_rt_sigpending, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(177, sys_rt_sigtimedwait_time32, )
#else
__SYSCALL_I386(177, __ia32_compat_sys_rt_sigtimedwait_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(178, sys_rt_sigqueueinfo, )
#else
__SYSCALL_I386(178, __ia32_compat_sys_rt_sigqueueinfo, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(179, sys_rt_sigsuspend, )
#else
__SYSCALL_I386(179, __ia32_compat_sys_rt_sigsuspend, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(180, sys_pread64, )
#else
__SYSCALL_I386(180, __ia32_compat_sys_x86_pread, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(181, sys_pwrite64, )
#else
__SYSCALL_I386(181, __ia32_compat_sys_x86_pwrite, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(182, sys_chown16, )
#else
__SYSCALL_I386(182, __ia32_sys_chown16, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(183, sys_getcwd, )
#else
__SYSCALL_I386(183, __ia32_sys_getcwd, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(184, sys_capget, )
#else
__SYSCALL_I386(184, __ia32_sys_capget, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(185, sys_capset, )
#else
__SYSCALL_I386(185, __ia32_sys_capset, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(186, sys_sigaltstack, )
#else
__SYSCALL_I386(186, __ia32_compat_sys_sigaltstack, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(187, sys_sendfile, )
#else
__SYSCALL_I386(187, __ia32_compat_sys_sendfile, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(190, sys_vfork, )
#else
__SYSCALL_I386(190, __ia32_sys_vfork, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(191, sys_getrlimit, )
#else
__SYSCALL_I386(191, __ia32_compat_sys_getrlimit, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(192, sys_mmap_pgoff, )
#else
__SYSCALL_I386(192, __ia32_sys_mmap_pgoff, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(193, sys_truncate64, )
#else
__SYSCALL_I386(193, __ia32_compat_sys_x86_truncate64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(194, sys_ftruncate64, )
#else
__SYSCALL_I386(194, __ia32_compat_sys_x86_ftruncate64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(195, sys_stat64, )
#else
__SYSCALL_I386(195, __ia32_compat_sys_x86_stat64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(196, sys_lstat64, )
#else
__SYSCALL_I386(196, __ia32_compat_sys_x86_lstat64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(197, sys_fstat64, )
#else
__SYSCALL_I386(197, __ia32_compat_sys_x86_fstat64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(198, sys_lchown, )
#else
__SYSCALL_I386(198, __ia32_sys_lchown, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(199, sys_getuid, )
#else
__SYSCALL_I386(199, __ia32_sys_getuid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(200, sys_getgid, )
#else
__SYSCALL_I386(200, __ia32_sys_getgid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(201, sys_geteuid, )
#else
__SYSCALL_I386(201, __ia32_sys_geteuid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(202, sys_getegid, )
#else
__SYSCALL_I386(202, __ia32_sys_getegid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(203, sys_setreuid, )
#else
__SYSCALL_I386(203, __ia32_sys_setreuid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(204, sys_setregid, )
#else
__SYSCALL_I386(204, __ia32_sys_setregid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(205, sys_getgroups, )
#else
__SYSCALL_I386(205, __ia32_sys_getgroups, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(206, sys_setgroups, )
#else
__SYSCALL_I386(206, __ia32_sys_setgroups, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(207, sys_fchown, )
#else
__SYSCALL_I386(207, __ia32_sys_fchown, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(208, sys_setresuid, )
#else
__SYSCALL_I386(208, __ia32_sys_setresuid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(209, sys_getresuid, )
#else
__SYSCALL_I386(209, __ia32_sys_getresuid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(210, sys_setresgid, )
#else
__SYSCALL_I386(210, __ia32_sys_setresgid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(211, sys_getresgid, )
#else
__SYSCALL_I386(211, __ia32_sys_getresgid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(212, sys_chown, )
#else
__SYSCALL_I386(212, __ia32_sys_chown, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(213, sys_setuid, )
#else
__SYSCALL_I386(213, __ia32_sys_setuid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(214, sys_setgid, )
#else
__SYSCALL_I386(214, __ia32_sys_setgid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(215, sys_setfsuid, )
#else
__SYSCALL_I386(215, __ia32_sys_setfsuid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(216, sys_setfsgid, )
#else
__SYSCALL_I386(216, __ia32_sys_setfsgid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(217, sys_pivot_root, )
#else
__SYSCALL_I386(217, __ia32_sys_pivot_root, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(218, sys_mincore, )
#else
__SYSCALL_I386(218, __ia32_sys_mincore, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(219, sys_madvise, )
#else
__SYSCALL_I386(219, __ia32_sys_madvise, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(220, sys_getdents64, )
#else
__SYSCALL_I386(220, __ia32_sys_getdents64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(221, sys_fcntl64, )
#else
__SYSCALL_I386(221, __ia32_compat_sys_fcntl64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(224, sys_gettid, )
#else
__SYSCALL_I386(224, __ia32_sys_gettid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(225, sys_readahead, )
#else
__SYSCALL_I386(225, __ia32_compat_sys_x86_readahead, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(226, sys_setxattr, )
#else
__SYSCALL_I386(226, __ia32_sys_setxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(227, sys_lsetxattr, )
#else
__SYSCALL_I386(227, __ia32_sys_lsetxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(228, sys_fsetxattr, )
#else
__SYSCALL_I386(228, __ia32_sys_fsetxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(229, sys_getxattr, )
#else
__SYSCALL_I386(229, __ia32_sys_getxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(230, sys_lgetxattr, )
#else
__SYSCALL_I386(230, __ia32_sys_lgetxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(231, sys_fgetxattr, )
#else
__SYSCALL_I386(231, __ia32_sys_fgetxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(232, sys_listxattr, )
#else
__SYSCALL_I386(232, __ia32_sys_listxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(233, sys_llistxattr, )
#else
__SYSCALL_I386(233, __ia32_sys_llistxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(234, sys_flistxattr, )
#else
__SYSCALL_I386(234, __ia32_sys_flistxattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(235, sys_removexattr, )
#else
__SYSCALL_I386(235, __ia32_sys_removexattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(236, sys_lremovexattr, )
#else
__SYSCALL_I386(236, __ia32_sys_lremovexattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(237, sys_fremovexattr, )
#else
__SYSCALL_I386(237, __ia32_sys_fremovexattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(238, sys_tkill, )
#else
__SYSCALL_I386(238, __ia32_sys_tkill, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(239, sys_sendfile64, )
#else
__SYSCALL_I386(239, __ia32_sys_sendfile64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(240, sys_futex_time32, )
#else
__SYSCALL_I386(240, __ia32_sys_futex_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(241, sys_sched_setaffinity, )
#else
__SYSCALL_I386(241, __ia32_compat_sys_sched_setaffinity, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(242, sys_sched_getaffinity, )
#else
__SYSCALL_I386(242, __ia32_compat_sys_sched_getaffinity, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(243, sys_set_thread_area, )
#else
__SYSCALL_I386(243, __ia32_sys_set_thread_area, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(244, sys_get_thread_area, )
#else
__SYSCALL_I386(244, __ia32_sys_get_thread_area, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(245, sys_io_setup, )
#else
__SYSCALL_I386(245, __ia32_compat_sys_io_setup, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(246, sys_io_destroy, )
#else
__SYSCALL_I386(246, __ia32_sys_io_destroy, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(247, sys_io_getevents_time32, )
#else
__SYSCALL_I386(247, __ia32_sys_io_getevents_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(248, sys_io_submit, )
#else
__SYSCALL_I386(248, __ia32_compat_sys_io_submit, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(249, sys_io_cancel, )
#else
__SYSCALL_I386(249, __ia32_sys_io_cancel, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(250, sys_fadvise64, )
#else
__SYSCALL_I386(250, __ia32_compat_sys_x86_fadvise64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(252, sys_exit_group, )
#else
__SYSCALL_I386(252, __ia32_sys_exit_group, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(253, sys_lookup_dcookie, )
#else
__SYSCALL_I386(253, __ia32_compat_sys_lookup_dcookie, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(254, sys_epoll_create, )
#else
__SYSCALL_I386(254, __ia32_sys_epoll_create, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(255, sys_epoll_ctl, )
#else
__SYSCALL_I386(255, __ia32_sys_epoll_ctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(256, sys_epoll_wait, )
#else
__SYSCALL_I386(256, __ia32_sys_epoll_wait, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(257, sys_remap_file_pages, )
#else
__SYSCALL_I386(257, __ia32_sys_remap_file_pages, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(258, sys_set_tid_address, )
#else
__SYSCALL_I386(258, __ia32_sys_set_tid_address, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(259, sys_timer_create, )
#else
__SYSCALL_I386(259, __ia32_compat_sys_timer_create, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(260, sys_timer_settime32, )
#else
__SYSCALL_I386(260, __ia32_sys_timer_settime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(261, sys_timer_gettime32, )
#else
__SYSCALL_I386(261, __ia32_sys_timer_gettime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(262, sys_timer_getoverrun, )
#else
__SYSCALL_I386(262, __ia32_sys_timer_getoverrun, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(263, sys_timer_delete, )
#else
__SYSCALL_I386(263, __ia32_sys_timer_delete, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(264, sys_clock_settime32, )
#else
__SYSCALL_I386(264, __ia32_sys_clock_settime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(265, sys_clock_gettime32, )
#else
__SYSCALL_I386(265, __ia32_sys_clock_gettime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(266, sys_clock_getres_time32, )
#else
__SYSCALL_I386(266, __ia32_sys_clock_getres_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(267, sys_clock_nanosleep_time32, )
#else
__SYSCALL_I386(267, __ia32_sys_clock_nanosleep_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(268, sys_statfs64, )
#else
__SYSCALL_I386(268, __ia32_compat_sys_statfs64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(269, sys_fstatfs64, )
#else
__SYSCALL_I386(269, __ia32_compat_sys_fstatfs64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(270, sys_tgkill, )
#else
__SYSCALL_I386(270, __ia32_sys_tgkill, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(271, sys_utimes_time32, )
#else
__SYSCALL_I386(271, __ia32_sys_utimes_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(272, sys_fadvise64_64, )
#else
__SYSCALL_I386(272, __ia32_compat_sys_x86_fadvise64_64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(274, sys_mbind, )
#else
__SYSCALL_I386(274, __ia32_sys_mbind, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(275, sys_get_mempolicy, )
#else
__SYSCALL_I386(275, __ia32_compat_sys_get_mempolicy, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(276, sys_set_mempolicy, )
#else
__SYSCALL_I386(276, __ia32_sys_set_mempolicy, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(277, sys_mq_open, )
#else
__SYSCALL_I386(277, __ia32_compat_sys_mq_open, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(278, sys_mq_unlink, )
#else
__SYSCALL_I386(278, __ia32_sys_mq_unlink, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(279, sys_mq_timedsend_time32, )
#else
__SYSCALL_I386(279, __ia32_sys_mq_timedsend_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(280, sys_mq_timedreceive_time32, )
#else
__SYSCALL_I386(280, __ia32_sys_mq_timedreceive_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(281, sys_mq_notify, )
#else
__SYSCALL_I386(281, __ia32_compat_sys_mq_notify, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(282, sys_mq_getsetattr, )
#else
__SYSCALL_I386(282, __ia32_compat_sys_mq_getsetattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(283, sys_kexec_load, )
#else
__SYSCALL_I386(283, __ia32_compat_sys_kexec_load, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(284, sys_waitid, )
#else
__SYSCALL_I386(284, __ia32_compat_sys_waitid, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(286, sys_add_key, )
#else
__SYSCALL_I386(286, __ia32_sys_add_key, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(287, sys_request_key, )
#else
__SYSCALL_I386(287, __ia32_sys_request_key, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(288, sys_keyctl, )
#else
__SYSCALL_I386(288, __ia32_compat_sys_keyctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(289, sys_ioprio_set, )
#else
__SYSCALL_I386(289, __ia32_sys_ioprio_set, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(290, sys_ioprio_get, )
#else
__SYSCALL_I386(290, __ia32_sys_ioprio_get, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(291, sys_inotify_init, )
#else
__SYSCALL_I386(291, __ia32_sys_inotify_init, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(292, sys_inotify_add_watch, )
#else
__SYSCALL_I386(292, __ia32_sys_inotify_add_watch, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(293, sys_inotify_rm_watch, )
#else
__SYSCALL_I386(293, __ia32_sys_inotify_rm_watch, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(294, sys_migrate_pages, )
#else
__SYSCALL_I386(294, __ia32_sys_migrate_pages, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(295, sys_openat, )
#else
__SYSCALL_I386(295, __ia32_compat_sys_openat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(296, sys_mkdirat, )
#else
__SYSCALL_I386(296, __ia32_sys_mkdirat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(297, sys_mknodat, )
#else
__SYSCALL_I386(297, __ia32_sys_mknodat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(298, sys_fchownat, )
#else
__SYSCALL_I386(298, __ia32_sys_fchownat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(299, sys_futimesat_time32, )
#else
__SYSCALL_I386(299, __ia32_sys_futimesat_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(300, sys_fstatat64, )
#else
__SYSCALL_I386(300, __ia32_compat_sys_x86_fstatat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(301, sys_unlinkat, )
#else
__SYSCALL_I386(301, __ia32_sys_unlinkat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(302, sys_renameat, )
#else
__SYSCALL_I386(302, __ia32_sys_renameat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(303, sys_linkat, )
#else
__SYSCALL_I386(303, __ia32_sys_linkat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(304, sys_symlinkat, )
#else
__SYSCALL_I386(304, __ia32_sys_symlinkat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(305, sys_readlinkat, )
#else
__SYSCALL_I386(305, __ia32_sys_readlinkat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(306, sys_fchmodat, )
#else
__SYSCALL_I386(306, __ia32_sys_fchmodat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(307, sys_faccessat, )
#else
__SYSCALL_I386(307, __ia32_sys_faccessat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(308, sys_pselect6_time32, )
#else
__SYSCALL_I386(308, __ia32_compat_sys_pselect6_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(309, sys_ppoll_time32, )
#else
__SYSCALL_I386(309, __ia32_compat_sys_ppoll_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(310, sys_unshare, )
#else
__SYSCALL_I386(310, __ia32_sys_unshare, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(311, sys_set_robust_list, )
#else
__SYSCALL_I386(311, __ia32_compat_sys_set_robust_list, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(312, sys_get_robust_list, )
#else
__SYSCALL_I386(312, __ia32_compat_sys_get_robust_list, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(313, sys_splice, )
#else
__SYSCALL_I386(313, __ia32_sys_splice, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(314, sys_sync_file_range, )
#else
__SYSCALL_I386(314, __ia32_compat_sys_x86_sync_file_range, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(315, sys_tee, )
#else
__SYSCALL_I386(315, __ia32_sys_tee, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(316, sys_vmsplice, )
#else
__SYSCALL_I386(316, __ia32_compat_sys_vmsplice, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(317, sys_move_pages, )
#else
__SYSCALL_I386(317, __ia32_compat_sys_move_pages, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(318, sys_getcpu, )
#else
__SYSCALL_I386(318, __ia32_sys_getcpu, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(319, sys_epoll_pwait, )
#else
__SYSCALL_I386(319, __ia32_sys_epoll_pwait, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(320, sys_utimensat_time32, )
#else
__SYSCALL_I386(320, __ia32_sys_utimensat_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(321, sys_signalfd, )
#else
__SYSCALL_I386(321, __ia32_compat_sys_signalfd, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(322, sys_timerfd_create, )
#else
__SYSCALL_I386(322, __ia32_sys_timerfd_create, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(323, sys_eventfd, )
#else
__SYSCALL_I386(323, __ia32_sys_eventfd, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(324, sys_fallocate, )
#else
__SYSCALL_I386(324, __ia32_compat_sys_x86_fallocate, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(325, sys_timerfd_settime32, )
#else
__SYSCALL_I386(325, __ia32_sys_timerfd_settime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(326, sys_timerfd_gettime32, )
#else
__SYSCALL_I386(326, __ia32_sys_timerfd_gettime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(327, sys_signalfd4, )
#else
__SYSCALL_I386(327, __ia32_compat_sys_signalfd4, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(328, sys_eventfd2, )
#else
__SYSCALL_I386(328, __ia32_sys_eventfd2, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(329, sys_epoll_create1, )
#else
__SYSCALL_I386(329, __ia32_sys_epoll_create1, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(330, sys_dup3, )
#else
__SYSCALL_I386(330, __ia32_sys_dup3, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(331, sys_pipe2, )
#else
__SYSCALL_I386(331, __ia32_sys_pipe2, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(332, sys_inotify_init1, )
#else
__SYSCALL_I386(332, __ia32_sys_inotify_init1, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(333, sys_preadv, )
#else
__SYSCALL_I386(333, __ia32_compat_sys_preadv, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(334, sys_pwritev, )
#else
__SYSCALL_I386(334, __ia32_compat_sys_pwritev, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(335, sys_rt_tgsigqueueinfo, )
#else
__SYSCALL_I386(335, __ia32_compat_sys_rt_tgsigqueueinfo, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(336, sys_perf_event_open, )
#else
__SYSCALL_I386(336, __ia32_sys_perf_event_open, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(337, sys_recvmmsg_time32, )
#else
__SYSCALL_I386(337, __ia32_compat_sys_recvmmsg_time32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(338, sys_fanotify_init, )
#else
__SYSCALL_I386(338, __ia32_sys_fanotify_init, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(339, sys_fanotify_mark, )
#else
__SYSCALL_I386(339, __ia32_compat_sys_fanotify_mark, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(340, sys_prlimit64, )
#else
__SYSCALL_I386(340, __ia32_sys_prlimit64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(341, sys_name_to_handle_at, )
#else
__SYSCALL_I386(341, __ia32_sys_name_to_handle_at, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(342, sys_open_by_handle_at, )
#else
__SYSCALL_I386(342, __ia32_compat_sys_open_by_handle_at, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(343, sys_clock_adjtime32, )
#else
__SYSCALL_I386(343, __ia32_sys_clock_adjtime32, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(344, sys_syncfs, )
#else
__SYSCALL_I386(344, __ia32_sys_syncfs, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(345, sys_sendmmsg, )
#else
__SYSCALL_I386(345, __ia32_compat_sys_sendmmsg, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(346, sys_setns, )
#else
__SYSCALL_I386(346, __ia32_sys_setns, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(347, sys_process_vm_readv, )
#else
__SYSCALL_I386(347, __ia32_compat_sys_process_vm_readv, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(348, sys_process_vm_writev, )
#else
__SYSCALL_I386(348, __ia32_compat_sys_process_vm_writev, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(349, sys_kcmp, )
#else
__SYSCALL_I386(349, __ia32_sys_kcmp, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(350, sys_finit_module, )
#else
__SYSCALL_I386(350, __ia32_sys_finit_module, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(351, sys_sched_setattr, )
#else
__SYSCALL_I386(351, __ia32_sys_sched_setattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(352, sys_sched_getattr, )
#else
__SYSCALL_I386(352, __ia32_sys_sched_getattr, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(353, sys_renameat2, )
#else
__SYSCALL_I386(353, __ia32_sys_renameat2, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(354, sys_seccomp, )
#else
__SYSCALL_I386(354, __ia32_sys_seccomp, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(355, sys_getrandom, )
#else
__SYSCALL_I386(355, __ia32_sys_getrandom, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(356, sys_memfd_create, )
#else
__SYSCALL_I386(356, __ia32_sys_memfd_create, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(357, sys_bpf, )
#else
__SYSCALL_I386(357, __ia32_sys_bpf, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(358, sys_execveat, )
#else
__SYSCALL_I386(358, __ia32_compat_sys_execveat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(359, sys_socket, )
#else
__SYSCALL_I386(359, __ia32_sys_socket, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(360, sys_socketpair, )
#else
__SYSCALL_I386(360, __ia32_sys_socketpair, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(361, sys_bind, )
#else
__SYSCALL_I386(361, __ia32_sys_bind, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(362, sys_connect, )
#else
__SYSCALL_I386(362, __ia32_sys_connect, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(363, sys_listen, )
#else
__SYSCALL_I386(363, __ia32_sys_listen, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(364, sys_accept4, )
#else
__SYSCALL_I386(364, __ia32_sys_accept4, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(365, sys_getsockopt, )
#else
__SYSCALL_I386(365, __ia32_compat_sys_getsockopt, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(366, sys_setsockopt, )
#else
__SYSCALL_I386(366, __ia32_compat_sys_setsockopt, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(367, sys_getsockname, )
#else
__SYSCALL_I386(367, __ia32_sys_getsockname, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(368, sys_getpeername, )
#else
__SYSCALL_I386(368, __ia32_sys_getpeername, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(369, sys_sendto, )
#else
__SYSCALL_I386(369, __ia32_sys_sendto, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(370, sys_sendmsg, )
#else
__SYSCALL_I386(370, __ia32_compat_sys_sendmsg, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(371, sys_recvfrom, )
#else
__SYSCALL_I386(371, __ia32_compat_sys_recvfrom, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(372, sys_recvmsg, )
#else
__SYSCALL_I386(372, __ia32_compat_sys_recvmsg, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(373, sys_shutdown, )
#else
__SYSCALL_I386(373, __ia32_sys_shutdown, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(374, sys_userfaultfd, )
#else
__SYSCALL_I386(374, __ia32_sys_userfaultfd, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(375, sys_membarrier, )
#else
__SYSCALL_I386(375, __ia32_sys_membarrier, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(376, sys_mlock2, )
#else
__SYSCALL_I386(376, __ia32_sys_mlock2, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(377, sys_copy_file_range, )
#else
__SYSCALL_I386(377, __ia32_sys_copy_file_range, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(378, sys_preadv2, )
#else
__SYSCALL_I386(378, __ia32_compat_sys_preadv2, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(379, sys_pwritev2, )
#else
__SYSCALL_I386(379, __ia32_compat_sys_pwritev2, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(380, sys_pkey_mprotect, )
#else
__SYSCALL_I386(380, __ia32_sys_pkey_mprotect, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(381, sys_pkey_alloc, )
#else
__SYSCALL_I386(381, __ia32_sys_pkey_alloc, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(382, sys_pkey_free, )
#else
__SYSCALL_I386(382, __ia32_sys_pkey_free, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(383, sys_statx, )
#else
__SYSCALL_I386(383, __ia32_sys_statx, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(384, sys_arch_prctl, )
#else
__SYSCALL_I386(384, __ia32_compat_sys_arch_prctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(385, sys_io_pgetevents_time32, )
#else
__SYSCALL_I386(385, __ia32_compat_sys_io_pgetevents, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(386, sys_rseq, )
#else
__SYSCALL_I386(386, __ia32_sys_rseq, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(393, sys_semget, )
#else
__SYSCALL_I386(393, __ia32_sys_semget, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(394, sys_semctl, )
#else
__SYSCALL_I386(394, __ia32_compat_sys_semctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(395, sys_shmget, )
#else
__SYSCALL_I386(395, __ia32_sys_shmget, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(396, sys_shmctl, )
#else
__SYSCALL_I386(396, __ia32_compat_sys_shmctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(397, sys_shmat, )
#else
__SYSCALL_I386(397, __ia32_compat_sys_shmat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(398, sys_shmdt, )
#else
__SYSCALL_I386(398, __ia32_sys_shmdt, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(399, sys_msgget, )
#else
__SYSCALL_I386(399, __ia32_sys_msgget, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(400, sys_msgsnd, )
#else
__SYSCALL_I386(400, __ia32_compat_sys_msgsnd, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(401, sys_msgrcv, )
#else
__SYSCALL_I386(401, __ia32_compat_sys_msgrcv, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(402, sys_msgctl, )
#else
__SYSCALL_I386(402, __ia32_compat_sys_msgctl, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(403, sys_clock_gettime, )
#else
__SYSCALL_I386(403, __ia32_sys_clock_gettime, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(404, sys_clock_settime, )
#else
__SYSCALL_I386(404, __ia32_sys_clock_settime, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(405, sys_clock_adjtime, )
#else
__SYSCALL_I386(405, __ia32_sys_clock_adjtime, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(406, sys_clock_getres, )
#else
__SYSCALL_I386(406, __ia32_sys_clock_getres, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(407, sys_clock_nanosleep, )
#else
__SYSCALL_I386(407, __ia32_sys_clock_nanosleep, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(408, sys_timer_gettime, )
#else
__SYSCALL_I386(408, __ia32_sys_timer_gettime, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(409, sys_timer_settime, )
#else
__SYSCALL_I386(409, __ia32_sys_timer_settime, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(410, sys_timerfd_gettime, )
#else
__SYSCALL_I386(410, __ia32_sys_timerfd_gettime, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(411, sys_timerfd_settime, )
#else
__SYSCALL_I386(411, __ia32_sys_timerfd_settime, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(412, sys_utimensat, )
#else
__SYSCALL_I386(412, __ia32_sys_utimensat, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(413, sys_pselect6, )
#else
__SYSCALL_I386(413, __ia32_compat_sys_pselect6_time64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(414, sys_ppoll, )
#else
__SYSCALL_I386(414, __ia32_compat_sys_ppoll_time64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(416, sys_io_pgetevents, )
#else
__SYSCALL_I386(416, __ia32_sys_io_pgetevents, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(417, sys_recvmmsg, )
#else
__SYSCALL_I386(417, __ia32_compat_sys_recvmmsg_time64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(418, sys_mq_timedsend, )
#else
__SYSCALL_I386(418, __ia32_sys_mq_timedsend, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(419, sys_mq_timedreceive, )
#else
__SYSCALL_I386(419, __ia32_sys_mq_timedreceive, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(420, sys_semtimedop, )
#else
__SYSCALL_I386(420, __ia32_sys_semtimedop, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(421, sys_rt_sigtimedwait, )
#else
__SYSCALL_I386(421, __ia32_compat_sys_rt_sigtimedwait_time64, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(422, sys_futex, )
#else
__SYSCALL_I386(422, __ia32_sys_futex, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(423, sys_sched_rr_get_interval, )
#else
__SYSCALL_I386(423, __ia32_sys_sched_rr_get_interval, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(424, sys_pidfd_send_signal, )
#else
__SYSCALL_I386(424, __ia32_sys_pidfd_send_signal, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(425, sys_io_uring_setup, )
#else
__SYSCALL_I386(425, __ia32_sys_io_uring_setup, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(426, sys_io_uring_enter, )
#else
__SYSCALL_I386(426, __ia32_sys_io_uring_enter, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(427, sys_io_uring_register, )
#else
__SYSCALL_I386(427, __ia32_sys_io_uring_register, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(428, sys_open_tree, )
#else
__SYSCALL_I386(428, __ia32_sys_open_tree, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(429, sys_move_mount, )
#else
__SYSCALL_I386(429, __ia32_sys_move_mount, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(430, sys_fsopen, )
#else
__SYSCALL_I386(430, __ia32_sys_fsopen, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(431, sys_fsconfig, )
#else
__SYSCALL_I386(431, __ia32_sys_fsconfig, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(432, sys_fsmount, )
#else
__SYSCALL_I386(432, __ia32_sys_fsmount, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(433, sys_fspick, )
#else
__SYSCALL_I386(433, __ia32_sys_fspick, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(434, sys_pidfd_open, )
#else
__SYSCALL_I386(434, __ia32_sys_pidfd_open, )
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(435, sys_clone3, )
#else
__SYSCALL_I386(435, __ia32_sys_clone3, )
#endif
