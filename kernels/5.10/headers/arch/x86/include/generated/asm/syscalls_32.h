__SYSCALL_I386(0, sys_restart_syscall)
__SYSCALL_I386(1, sys_exit)
__SYSCALL_I386(2, sys_fork)
__SYSCALL_I386(3, sys_read)
__SYSCALL_I386(4, sys_write)
#ifdef CONFIG_X86_32
__SYSCALL_I386(5, sys_open)
#else
__SYSCALL_I386(5, compat_sys_open)
#endif
__SYSCALL_I386(6, sys_close)
__SYSCALL_I386(7, sys_waitpid)
__SYSCALL_I386(8, sys_creat)
__SYSCALL_I386(9, sys_link)
__SYSCALL_I386(10, sys_unlink)
#ifdef CONFIG_X86_32
__SYSCALL_I386(11, sys_execve)
#else
__SYSCALL_I386(11, compat_sys_execve)
#endif
__SYSCALL_I386(12, sys_chdir)
__SYSCALL_I386(13, sys_time32)
__SYSCALL_I386(14, sys_mknod)
__SYSCALL_I386(15, sys_chmod)
__SYSCALL_I386(16, sys_lchown16)
__SYSCALL_I386(18, sys_stat)
#ifdef CONFIG_X86_32
__SYSCALL_I386(19, sys_lseek)
#else
__SYSCALL_I386(19, compat_sys_lseek)
#endif
__SYSCALL_I386(20, sys_getpid)
__SYSCALL_I386(21, sys_mount)
__SYSCALL_I386(22, sys_oldumount)
__SYSCALL_I386(23, sys_setuid16)
__SYSCALL_I386(24, sys_getuid16)
__SYSCALL_I386(25, sys_stime32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(26, sys_ptrace)
#else
__SYSCALL_I386(26, compat_sys_ptrace)
#endif
__SYSCALL_I386(27, sys_alarm)
__SYSCALL_I386(28, sys_fstat)
__SYSCALL_I386(29, sys_pause)
__SYSCALL_I386(30, sys_utime32)
__SYSCALL_I386(33, sys_access)
__SYSCALL_I386(34, sys_nice)
__SYSCALL_I386(36, sys_sync)
__SYSCALL_I386(37, sys_kill)
__SYSCALL_I386(38, sys_rename)
__SYSCALL_I386(39, sys_mkdir)
__SYSCALL_I386(40, sys_rmdir)
__SYSCALL_I386(41, sys_dup)
__SYSCALL_I386(42, sys_pipe)
#ifdef CONFIG_X86_32
__SYSCALL_I386(43, sys_times)
#else
__SYSCALL_I386(43, compat_sys_times)
#endif
__SYSCALL_I386(45, sys_brk)
__SYSCALL_I386(46, sys_setgid16)
__SYSCALL_I386(47, sys_getgid16)
__SYSCALL_I386(48, sys_signal)
__SYSCALL_I386(49, sys_geteuid16)
__SYSCALL_I386(50, sys_getegid16)
__SYSCALL_I386(51, sys_acct)
__SYSCALL_I386(52, sys_umount)
#ifdef CONFIG_X86_32
__SYSCALL_I386(54, sys_ioctl)
#else
__SYSCALL_I386(54, compat_sys_ioctl)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(55, sys_fcntl)
#else
__SYSCALL_I386(55, compat_sys_fcntl64)
#endif
__SYSCALL_I386(57, sys_setpgid)
__SYSCALL_I386(59, sys_olduname)
__SYSCALL_I386(60, sys_umask)
__SYSCALL_I386(61, sys_chroot)
#ifdef CONFIG_X86_32
__SYSCALL_I386(62, sys_ustat)
#else
__SYSCALL_I386(62, compat_sys_ustat)
#endif
__SYSCALL_I386(63, sys_dup2)
__SYSCALL_I386(64, sys_getppid)
__SYSCALL_I386(65, sys_getpgrp)
__SYSCALL_I386(66, sys_setsid)
#ifdef CONFIG_X86_32
__SYSCALL_I386(67, sys_sigaction)
#else
__SYSCALL_I386(67, compat_sys_sigaction)
#endif
__SYSCALL_I386(68, sys_sgetmask)
__SYSCALL_I386(69, sys_ssetmask)
__SYSCALL_I386(70, sys_setreuid16)
__SYSCALL_I386(71, sys_setregid16)
__SYSCALL_I386(72, sys_sigsuspend)
#ifdef CONFIG_X86_32
__SYSCALL_I386(73, sys_sigpending)
#else
__SYSCALL_I386(73, compat_sys_sigpending)
#endif
__SYSCALL_I386(74, sys_sethostname)
#ifdef CONFIG_X86_32
__SYSCALL_I386(75, sys_setrlimit)
#else
__SYSCALL_I386(75, compat_sys_setrlimit)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(76, sys_old_getrlimit)
#else
__SYSCALL_I386(76, compat_sys_old_getrlimit)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(77, sys_getrusage)
#else
__SYSCALL_I386(77, compat_sys_getrusage)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(78, sys_gettimeofday)
#else
__SYSCALL_I386(78, compat_sys_gettimeofday)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(79, sys_settimeofday)
#else
__SYSCALL_I386(79, compat_sys_settimeofday)
#endif
__SYSCALL_I386(80, sys_getgroups16)
__SYSCALL_I386(81, sys_setgroups16)
#ifdef CONFIG_X86_32
__SYSCALL_I386(82, sys_old_select)
#else
__SYSCALL_I386(82, compat_sys_old_select)
#endif
__SYSCALL_I386(83, sys_symlink)
__SYSCALL_I386(84, sys_lstat)
__SYSCALL_I386(85, sys_readlink)
__SYSCALL_I386(86, sys_uselib)
__SYSCALL_I386(87, sys_swapon)
__SYSCALL_I386(88, sys_reboot)
#ifdef CONFIG_X86_32
__SYSCALL_I386(89, sys_old_readdir)
#else
__SYSCALL_I386(89, compat_sys_old_readdir)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(90, sys_old_mmap)
#else
__SYSCALL_I386(90, compat_sys_ia32_mmap)
#endif
__SYSCALL_I386(91, sys_munmap)
#ifdef CONFIG_X86_32
__SYSCALL_I386(92, sys_truncate)
#else
__SYSCALL_I386(92, compat_sys_truncate)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(93, sys_ftruncate)
#else
__SYSCALL_I386(93, compat_sys_ftruncate)
#endif
__SYSCALL_I386(94, sys_fchmod)
__SYSCALL_I386(95, sys_fchown16)
__SYSCALL_I386(96, sys_getpriority)
__SYSCALL_I386(97, sys_setpriority)
#ifdef CONFIG_X86_32
__SYSCALL_I386(99, sys_statfs)
#else
__SYSCALL_I386(99, compat_sys_statfs)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(100, sys_fstatfs)
#else
__SYSCALL_I386(100, compat_sys_fstatfs)
#endif
__SYSCALL_I386(101, sys_ioperm)
#ifdef CONFIG_X86_32
__SYSCALL_I386(102, sys_socketcall)
#else
__SYSCALL_I386(102, compat_sys_socketcall)
#endif
__SYSCALL_I386(103, sys_syslog)
#ifdef CONFIG_X86_32
__SYSCALL_I386(104, sys_setitimer)
#else
__SYSCALL_I386(104, compat_sys_setitimer)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(105, sys_getitimer)
#else
__SYSCALL_I386(105, compat_sys_getitimer)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(106, sys_newstat)
#else
__SYSCALL_I386(106, compat_sys_newstat)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(107, sys_newlstat)
#else
__SYSCALL_I386(107, compat_sys_newlstat)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(108, sys_newfstat)
#else
__SYSCALL_I386(108, compat_sys_newfstat)
#endif
__SYSCALL_I386(109, sys_uname)
__SYSCALL_I386(110, sys_iopl)
__SYSCALL_I386(111, sys_vhangup)
#ifdef CONFIG_X86_32
__SYSCALL_I386(113, sys_vm86old)
#else
__SYSCALL_I386(113, sys_ni_syscall)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(114, sys_wait4)
#else
__SYSCALL_I386(114, compat_sys_wait4)
#endif
__SYSCALL_I386(115, sys_swapoff)
#ifdef CONFIG_X86_32
__SYSCALL_I386(116, sys_sysinfo)
#else
__SYSCALL_I386(116, compat_sys_sysinfo)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(117, sys_ipc)
#else
__SYSCALL_I386(117, compat_sys_ipc)
#endif
__SYSCALL_I386(118, sys_fsync)
#ifdef CONFIG_X86_32
__SYSCALL_I386(119, sys_sigreturn)
#else
__SYSCALL_I386(119, compat_sys_sigreturn)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(120, sys_clone)
#else
__SYSCALL_I386(120, compat_sys_ia32_clone)
#endif
__SYSCALL_I386(121, sys_setdomainname)
__SYSCALL_I386(122, sys_newuname)
__SYSCALL_I386(123, sys_modify_ldt)
__SYSCALL_I386(124, sys_adjtimex_time32)
__SYSCALL_I386(125, sys_mprotect)
#ifdef CONFIG_X86_32
__SYSCALL_I386(126, sys_sigprocmask)
#else
__SYSCALL_I386(126, compat_sys_sigprocmask)
#endif
__SYSCALL_I386(128, sys_init_module)
__SYSCALL_I386(129, sys_delete_module)
__SYSCALL_I386(131, sys_quotactl)
__SYSCALL_I386(132, sys_getpgid)
__SYSCALL_I386(133, sys_fchdir)
__SYSCALL_I386(134, sys_bdflush)
__SYSCALL_I386(135, sys_sysfs)
__SYSCALL_I386(136, sys_personality)
__SYSCALL_I386(138, sys_setfsuid16)
__SYSCALL_I386(139, sys_setfsgid16)
__SYSCALL_I386(140, sys_llseek)
#ifdef CONFIG_X86_32
__SYSCALL_I386(141, sys_getdents)
#else
__SYSCALL_I386(141, compat_sys_getdents)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(142, sys_select)
#else
__SYSCALL_I386(142, compat_sys_select)
#endif
__SYSCALL_I386(143, sys_flock)
__SYSCALL_I386(144, sys_msync)
__SYSCALL_I386(145, sys_readv)
__SYSCALL_I386(146, sys_writev)
__SYSCALL_I386(147, sys_getsid)
__SYSCALL_I386(148, sys_fdatasync)
__SYSCALL_I386(149, sys_ni_syscall)
__SYSCALL_I386(150, sys_mlock)
__SYSCALL_I386(151, sys_munlock)
__SYSCALL_I386(152, sys_mlockall)
__SYSCALL_I386(153, sys_munlockall)
__SYSCALL_I386(154, sys_sched_setparam)
__SYSCALL_I386(155, sys_sched_getparam)
__SYSCALL_I386(156, sys_sched_setscheduler)
__SYSCALL_I386(157, sys_sched_getscheduler)
__SYSCALL_I386(158, sys_sched_yield)
__SYSCALL_I386(159, sys_sched_get_priority_max)
__SYSCALL_I386(160, sys_sched_get_priority_min)
__SYSCALL_I386(161, sys_sched_rr_get_interval_time32)
__SYSCALL_I386(162, sys_nanosleep_time32)
__SYSCALL_I386(163, sys_mremap)
__SYSCALL_I386(164, sys_setresuid16)
__SYSCALL_I386(165, sys_getresuid16)
#ifdef CONFIG_X86_32
__SYSCALL_I386(166, sys_vm86)
#else
__SYSCALL_I386(166, sys_ni_syscall)
#endif
__SYSCALL_I386(168, sys_poll)
__SYSCALL_I386(170, sys_setresgid16)
__SYSCALL_I386(171, sys_getresgid16)
__SYSCALL_I386(172, sys_prctl)
#ifdef CONFIG_X86_32
__SYSCALL_I386(173, sys_rt_sigreturn)
#else
__SYSCALL_I386(173, compat_sys_rt_sigreturn)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(174, sys_rt_sigaction)
#else
__SYSCALL_I386(174, compat_sys_rt_sigaction)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(175, sys_rt_sigprocmask)
#else
__SYSCALL_I386(175, compat_sys_rt_sigprocmask)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(176, sys_rt_sigpending)
#else
__SYSCALL_I386(176, compat_sys_rt_sigpending)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(177, sys_rt_sigtimedwait_time32)
#else
__SYSCALL_I386(177, compat_sys_rt_sigtimedwait_time32)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(178, sys_rt_sigqueueinfo)
#else
__SYSCALL_I386(178, compat_sys_rt_sigqueueinfo)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(179, sys_rt_sigsuspend)
#else
__SYSCALL_I386(179, compat_sys_rt_sigsuspend)
#endif
__SYSCALL_I386(180, sys_ia32_pread64)
__SYSCALL_I386(181, sys_ia32_pwrite64)
__SYSCALL_I386(182, sys_chown16)
__SYSCALL_I386(183, sys_getcwd)
__SYSCALL_I386(184, sys_capget)
__SYSCALL_I386(185, sys_capset)
#ifdef CONFIG_X86_32
__SYSCALL_I386(186, sys_sigaltstack)
#else
__SYSCALL_I386(186, compat_sys_sigaltstack)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(187, sys_sendfile)
#else
__SYSCALL_I386(187, compat_sys_sendfile)
#endif
__SYSCALL_I386(190, sys_vfork)
#ifdef CONFIG_X86_32
__SYSCALL_I386(191, sys_getrlimit)
#else
__SYSCALL_I386(191, compat_sys_getrlimit)
#endif
__SYSCALL_I386(192, sys_mmap_pgoff)
__SYSCALL_I386(193, sys_ia32_truncate64)
__SYSCALL_I386(194, sys_ia32_ftruncate64)
#ifdef CONFIG_X86_32
__SYSCALL_I386(195, sys_stat64)
#else
__SYSCALL_I386(195, compat_sys_ia32_stat64)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(196, sys_lstat64)
#else
__SYSCALL_I386(196, compat_sys_ia32_lstat64)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(197, sys_fstat64)
#else
__SYSCALL_I386(197, compat_sys_ia32_fstat64)
#endif
__SYSCALL_I386(198, sys_lchown)
__SYSCALL_I386(199, sys_getuid)
__SYSCALL_I386(200, sys_getgid)
__SYSCALL_I386(201, sys_geteuid)
__SYSCALL_I386(202, sys_getegid)
__SYSCALL_I386(203, sys_setreuid)
__SYSCALL_I386(204, sys_setregid)
__SYSCALL_I386(205, sys_getgroups)
__SYSCALL_I386(206, sys_setgroups)
__SYSCALL_I386(207, sys_fchown)
__SYSCALL_I386(208, sys_setresuid)
__SYSCALL_I386(209, sys_getresuid)
__SYSCALL_I386(210, sys_setresgid)
__SYSCALL_I386(211, sys_getresgid)
__SYSCALL_I386(212, sys_chown)
__SYSCALL_I386(213, sys_setuid)
__SYSCALL_I386(214, sys_setgid)
__SYSCALL_I386(215, sys_setfsuid)
__SYSCALL_I386(216, sys_setfsgid)
__SYSCALL_I386(217, sys_pivot_root)
__SYSCALL_I386(218, sys_mincore)
__SYSCALL_I386(219, sys_madvise)
__SYSCALL_I386(220, sys_getdents64)
#ifdef CONFIG_X86_32
__SYSCALL_I386(221, sys_fcntl64)
#else
__SYSCALL_I386(221, compat_sys_fcntl64)
#endif
__SYSCALL_I386(224, sys_gettid)
__SYSCALL_I386(225, sys_ia32_readahead)
__SYSCALL_I386(226, sys_setxattr)
__SYSCALL_I386(227, sys_lsetxattr)
__SYSCALL_I386(228, sys_fsetxattr)
__SYSCALL_I386(229, sys_getxattr)
__SYSCALL_I386(230, sys_lgetxattr)
__SYSCALL_I386(231, sys_fgetxattr)
__SYSCALL_I386(232, sys_listxattr)
__SYSCALL_I386(233, sys_llistxattr)
__SYSCALL_I386(234, sys_flistxattr)
__SYSCALL_I386(235, sys_removexattr)
__SYSCALL_I386(236, sys_lremovexattr)
__SYSCALL_I386(237, sys_fremovexattr)
__SYSCALL_I386(238, sys_tkill)
__SYSCALL_I386(239, sys_sendfile64)
__SYSCALL_I386(240, sys_futex_time32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(241, sys_sched_setaffinity)
#else
__SYSCALL_I386(241, compat_sys_sched_setaffinity)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(242, sys_sched_getaffinity)
#else
__SYSCALL_I386(242, compat_sys_sched_getaffinity)
#endif
__SYSCALL_I386(243, sys_set_thread_area)
__SYSCALL_I386(244, sys_get_thread_area)
#ifdef CONFIG_X86_32
__SYSCALL_I386(245, sys_io_setup)
#else
__SYSCALL_I386(245, compat_sys_io_setup)
#endif
__SYSCALL_I386(246, sys_io_destroy)
__SYSCALL_I386(247, sys_io_getevents_time32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(248, sys_io_submit)
#else
__SYSCALL_I386(248, compat_sys_io_submit)
#endif
__SYSCALL_I386(249, sys_io_cancel)
__SYSCALL_I386(250, sys_ia32_fadvise64)
__SYSCALL_I386(252, sys_exit_group)
#ifdef CONFIG_X86_32
__SYSCALL_I386(253, sys_lookup_dcookie)
#else
__SYSCALL_I386(253, compat_sys_lookup_dcookie)
#endif
__SYSCALL_I386(254, sys_epoll_create)
__SYSCALL_I386(255, sys_epoll_ctl)
__SYSCALL_I386(256, sys_epoll_wait)
__SYSCALL_I386(257, sys_remap_file_pages)
__SYSCALL_I386(258, sys_set_tid_address)
#ifdef CONFIG_X86_32
__SYSCALL_I386(259, sys_timer_create)
#else
__SYSCALL_I386(259, compat_sys_timer_create)
#endif
__SYSCALL_I386(260, sys_timer_settime32)
__SYSCALL_I386(261, sys_timer_gettime32)
__SYSCALL_I386(262, sys_timer_getoverrun)
__SYSCALL_I386(263, sys_timer_delete)
__SYSCALL_I386(264, sys_clock_settime32)
__SYSCALL_I386(265, sys_clock_gettime32)
__SYSCALL_I386(266, sys_clock_getres_time32)
__SYSCALL_I386(267, sys_clock_nanosleep_time32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(268, sys_statfs64)
#else
__SYSCALL_I386(268, compat_sys_statfs64)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(269, sys_fstatfs64)
#else
__SYSCALL_I386(269, compat_sys_fstatfs64)
#endif
__SYSCALL_I386(270, sys_tgkill)
__SYSCALL_I386(271, sys_utimes_time32)
__SYSCALL_I386(272, sys_ia32_fadvise64_64)
__SYSCALL_I386(274, sys_mbind)
#ifdef CONFIG_X86_32
__SYSCALL_I386(275, sys_get_mempolicy)
#else
__SYSCALL_I386(275, compat_sys_get_mempolicy)
#endif
__SYSCALL_I386(276, sys_set_mempolicy)
#ifdef CONFIG_X86_32
__SYSCALL_I386(277, sys_mq_open)
#else
__SYSCALL_I386(277, compat_sys_mq_open)
#endif
__SYSCALL_I386(278, sys_mq_unlink)
__SYSCALL_I386(279, sys_mq_timedsend_time32)
__SYSCALL_I386(280, sys_mq_timedreceive_time32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(281, sys_mq_notify)
#else
__SYSCALL_I386(281, compat_sys_mq_notify)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(282, sys_mq_getsetattr)
#else
__SYSCALL_I386(282, compat_sys_mq_getsetattr)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(283, sys_kexec_load)
#else
__SYSCALL_I386(283, compat_sys_kexec_load)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(284, sys_waitid)
#else
__SYSCALL_I386(284, compat_sys_waitid)
#endif
__SYSCALL_I386(286, sys_add_key)
__SYSCALL_I386(287, sys_request_key)
#ifdef CONFIG_X86_32
__SYSCALL_I386(288, sys_keyctl)
#else
__SYSCALL_I386(288, compat_sys_keyctl)
#endif
__SYSCALL_I386(289, sys_ioprio_set)
__SYSCALL_I386(290, sys_ioprio_get)
__SYSCALL_I386(291, sys_inotify_init)
__SYSCALL_I386(292, sys_inotify_add_watch)
__SYSCALL_I386(293, sys_inotify_rm_watch)
__SYSCALL_I386(294, sys_migrate_pages)
#ifdef CONFIG_X86_32
__SYSCALL_I386(295, sys_openat)
#else
__SYSCALL_I386(295, compat_sys_openat)
#endif
__SYSCALL_I386(296, sys_mkdirat)
__SYSCALL_I386(297, sys_mknodat)
__SYSCALL_I386(298, sys_fchownat)
__SYSCALL_I386(299, sys_futimesat_time32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(300, sys_fstatat64)
#else
__SYSCALL_I386(300, compat_sys_ia32_fstatat64)
#endif
__SYSCALL_I386(301, sys_unlinkat)
__SYSCALL_I386(302, sys_renameat)
__SYSCALL_I386(303, sys_linkat)
__SYSCALL_I386(304, sys_symlinkat)
__SYSCALL_I386(305, sys_readlinkat)
__SYSCALL_I386(306, sys_fchmodat)
__SYSCALL_I386(307, sys_faccessat)
#ifdef CONFIG_X86_32
__SYSCALL_I386(308, sys_pselect6_time32)
#else
__SYSCALL_I386(308, compat_sys_pselect6_time32)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(309, sys_ppoll_time32)
#else
__SYSCALL_I386(309, compat_sys_ppoll_time32)
#endif
__SYSCALL_I386(310, sys_unshare)
#ifdef CONFIG_X86_32
__SYSCALL_I386(311, sys_set_robust_list)
#else
__SYSCALL_I386(311, compat_sys_set_robust_list)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(312, sys_get_robust_list)
#else
__SYSCALL_I386(312, compat_sys_get_robust_list)
#endif
__SYSCALL_I386(313, sys_splice)
__SYSCALL_I386(314, sys_ia32_sync_file_range)
__SYSCALL_I386(315, sys_tee)
__SYSCALL_I386(316, sys_vmsplice)
#ifdef CONFIG_X86_32
__SYSCALL_I386(317, sys_move_pages)
#else
__SYSCALL_I386(317, compat_sys_move_pages)
#endif
__SYSCALL_I386(318, sys_getcpu)
__SYSCALL_I386(319, sys_epoll_pwait)
__SYSCALL_I386(320, sys_utimensat_time32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(321, sys_signalfd)
#else
__SYSCALL_I386(321, compat_sys_signalfd)
#endif
__SYSCALL_I386(322, sys_timerfd_create)
__SYSCALL_I386(323, sys_eventfd)
__SYSCALL_I386(324, sys_ia32_fallocate)
__SYSCALL_I386(325, sys_timerfd_settime32)
__SYSCALL_I386(326, sys_timerfd_gettime32)
#ifdef CONFIG_X86_32
__SYSCALL_I386(327, sys_signalfd4)
#else
__SYSCALL_I386(327, compat_sys_signalfd4)
#endif
__SYSCALL_I386(328, sys_eventfd2)
__SYSCALL_I386(329, sys_epoll_create1)
__SYSCALL_I386(330, sys_dup3)
__SYSCALL_I386(331, sys_pipe2)
__SYSCALL_I386(332, sys_inotify_init1)
#ifdef CONFIG_X86_32
__SYSCALL_I386(333, sys_preadv)
#else
__SYSCALL_I386(333, compat_sys_preadv)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(334, sys_pwritev)
#else
__SYSCALL_I386(334, compat_sys_pwritev)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(335, sys_rt_tgsigqueueinfo)
#else
__SYSCALL_I386(335, compat_sys_rt_tgsigqueueinfo)
#endif
__SYSCALL_I386(336, sys_perf_event_open)
#ifdef CONFIG_X86_32
__SYSCALL_I386(337, sys_recvmmsg_time32)
#else
__SYSCALL_I386(337, compat_sys_recvmmsg_time32)
#endif
__SYSCALL_I386(338, sys_fanotify_init)
#ifdef CONFIG_X86_32
__SYSCALL_I386(339, sys_fanotify_mark)
#else
__SYSCALL_I386(339, compat_sys_fanotify_mark)
#endif
__SYSCALL_I386(340, sys_prlimit64)
__SYSCALL_I386(341, sys_name_to_handle_at)
#ifdef CONFIG_X86_32
__SYSCALL_I386(342, sys_open_by_handle_at)
#else
__SYSCALL_I386(342, compat_sys_open_by_handle_at)
#endif
__SYSCALL_I386(343, sys_clock_adjtime32)
__SYSCALL_I386(344, sys_syncfs)
#ifdef CONFIG_X86_32
__SYSCALL_I386(345, sys_sendmmsg)
#else
__SYSCALL_I386(345, compat_sys_sendmmsg)
#endif
__SYSCALL_I386(346, sys_setns)
__SYSCALL_I386(347, sys_process_vm_readv)
__SYSCALL_I386(348, sys_process_vm_writev)
__SYSCALL_I386(349, sys_kcmp)
__SYSCALL_I386(350, sys_finit_module)
__SYSCALL_I386(351, sys_sched_setattr)
__SYSCALL_I386(352, sys_sched_getattr)
__SYSCALL_I386(353, sys_renameat2)
__SYSCALL_I386(354, sys_seccomp)
__SYSCALL_I386(355, sys_getrandom)
__SYSCALL_I386(356, sys_memfd_create)
__SYSCALL_I386(357, sys_bpf)
#ifdef CONFIG_X86_32
__SYSCALL_I386(358, sys_execveat)
#else
__SYSCALL_I386(358, compat_sys_execveat)
#endif
__SYSCALL_I386(359, sys_socket)
__SYSCALL_I386(360, sys_socketpair)
__SYSCALL_I386(361, sys_bind)
__SYSCALL_I386(362, sys_connect)
__SYSCALL_I386(363, sys_listen)
__SYSCALL_I386(364, sys_accept4)
#ifdef CONFIG_X86_32
__SYSCALL_I386(365, sys_getsockopt)
#else
__SYSCALL_I386(365, sys_getsockopt)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(366, sys_setsockopt)
#else
__SYSCALL_I386(366, sys_setsockopt)
#endif
__SYSCALL_I386(367, sys_getsockname)
__SYSCALL_I386(368, sys_getpeername)
__SYSCALL_I386(369, sys_sendto)
#ifdef CONFIG_X86_32
__SYSCALL_I386(370, sys_sendmsg)
#else
__SYSCALL_I386(370, compat_sys_sendmsg)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(371, sys_recvfrom)
#else
__SYSCALL_I386(371, compat_sys_recvfrom)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(372, sys_recvmsg)
#else
__SYSCALL_I386(372, compat_sys_recvmsg)
#endif
__SYSCALL_I386(373, sys_shutdown)
__SYSCALL_I386(374, sys_userfaultfd)
__SYSCALL_I386(375, sys_membarrier)
__SYSCALL_I386(376, sys_mlock2)
__SYSCALL_I386(377, sys_copy_file_range)
#ifdef CONFIG_X86_32
__SYSCALL_I386(378, sys_preadv2)
#else
__SYSCALL_I386(378, compat_sys_preadv2)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(379, sys_pwritev2)
#else
__SYSCALL_I386(379, compat_sys_pwritev2)
#endif
__SYSCALL_I386(380, sys_pkey_mprotect)
__SYSCALL_I386(381, sys_pkey_alloc)
__SYSCALL_I386(382, sys_pkey_free)
__SYSCALL_I386(383, sys_statx)
#ifdef CONFIG_X86_32
__SYSCALL_I386(384, sys_arch_prctl)
#else
__SYSCALL_I386(384, compat_sys_arch_prctl)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(385, sys_io_pgetevents_time32)
#else
__SYSCALL_I386(385, compat_sys_io_pgetevents)
#endif
__SYSCALL_I386(386, sys_rseq)
__SYSCALL_I386(393, sys_semget)
#ifdef CONFIG_X86_32
__SYSCALL_I386(394, sys_semctl)
#else
__SYSCALL_I386(394, compat_sys_semctl)
#endif
__SYSCALL_I386(395, sys_shmget)
#ifdef CONFIG_X86_32
__SYSCALL_I386(396, sys_shmctl)
#else
__SYSCALL_I386(396, compat_sys_shmctl)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(397, sys_shmat)
#else
__SYSCALL_I386(397, compat_sys_shmat)
#endif
__SYSCALL_I386(398, sys_shmdt)
__SYSCALL_I386(399, sys_msgget)
#ifdef CONFIG_X86_32
__SYSCALL_I386(400, sys_msgsnd)
#else
__SYSCALL_I386(400, compat_sys_msgsnd)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(401, sys_msgrcv)
#else
__SYSCALL_I386(401, compat_sys_msgrcv)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(402, sys_msgctl)
#else
__SYSCALL_I386(402, compat_sys_msgctl)
#endif
__SYSCALL_I386(403, sys_clock_gettime)
__SYSCALL_I386(404, sys_clock_settime)
__SYSCALL_I386(405, sys_clock_adjtime)
__SYSCALL_I386(406, sys_clock_getres)
__SYSCALL_I386(407, sys_clock_nanosleep)
__SYSCALL_I386(408, sys_timer_gettime)
__SYSCALL_I386(409, sys_timer_settime)
__SYSCALL_I386(410, sys_timerfd_gettime)
__SYSCALL_I386(411, sys_timerfd_settime)
__SYSCALL_I386(412, sys_utimensat)
#ifdef CONFIG_X86_32
__SYSCALL_I386(413, sys_pselect6)
#else
__SYSCALL_I386(413, compat_sys_pselect6_time64)
#endif
#ifdef CONFIG_X86_32
__SYSCALL_I386(414, sys_ppoll)
#else
__SYSCALL_I386(414, compat_sys_ppoll_time64)
#endif
__SYSCALL_I386(416, sys_io_pgetevents)
#ifdef CONFIG_X86_32
__SYSCALL_I386(417, sys_recvmmsg)
#else
__SYSCALL_I386(417, compat_sys_recvmmsg_time64)
#endif
__SYSCALL_I386(418, sys_mq_timedsend)
__SYSCALL_I386(419, sys_mq_timedreceive)
__SYSCALL_I386(420, sys_semtimedop)
#ifdef CONFIG_X86_32
__SYSCALL_I386(421, sys_rt_sigtimedwait)
#else
__SYSCALL_I386(421, compat_sys_rt_sigtimedwait_time64)
#endif
__SYSCALL_I386(422, sys_futex)
__SYSCALL_I386(423, sys_sched_rr_get_interval)
__SYSCALL_I386(424, sys_pidfd_send_signal)
__SYSCALL_I386(425, sys_io_uring_setup)
__SYSCALL_I386(426, sys_io_uring_enter)
__SYSCALL_I386(427, sys_io_uring_register)
__SYSCALL_I386(428, sys_open_tree)
__SYSCALL_I386(429, sys_move_mount)
__SYSCALL_I386(430, sys_fsopen)
__SYSCALL_I386(431, sys_fsconfig)
__SYSCALL_I386(432, sys_fsmount)
__SYSCALL_I386(433, sys_fspick)
__SYSCALL_I386(434, sys_pidfd_open)
__SYSCALL_I386(435, sys_clone3)
__SYSCALL_I386(436, sys_close_range)
__SYSCALL_I386(437, sys_openat2)
__SYSCALL_I386(438, sys_pidfd_getfd)
__SYSCALL_I386(439, sys_faccessat2)
__SYSCALL_I386(440, sys_process_madvise)
