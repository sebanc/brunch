#ifdef CONFIG_X86
__SYSCALL_64(0, __x64_sys_read, )
#else /* CONFIG_UML */
__SYSCALL_64(0, sys_read, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(0, __x64_sys_read, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(1, __x64_sys_write, )
#else /* CONFIG_UML */
__SYSCALL_64(1, sys_write, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(1, __x64_sys_write, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(2, __x64_sys_open, )
#else /* CONFIG_UML */
__SYSCALL_64(2, sys_open, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(2, __x64_sys_open, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(3, __x64_sys_close, )
#else /* CONFIG_UML */
__SYSCALL_64(3, sys_close, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(3, __x64_sys_close, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(4, __x64_sys_newstat, )
#else /* CONFIG_UML */
__SYSCALL_64(4, sys_newstat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(4, __x64_sys_newstat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(5, __x64_sys_newfstat, )
#else /* CONFIG_UML */
__SYSCALL_64(5, sys_newfstat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(5, __x64_sys_newfstat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(6, __x64_sys_newlstat, )
#else /* CONFIG_UML */
__SYSCALL_64(6, sys_newlstat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(6, __x64_sys_newlstat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(7, __x64_sys_poll, )
#else /* CONFIG_UML */
__SYSCALL_64(7, sys_poll, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(7, __x64_sys_poll, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(8, __x64_sys_lseek, )
#else /* CONFIG_UML */
__SYSCALL_64(8, sys_lseek, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(8, __x64_sys_lseek, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(9, __x64_sys_mmap, )
#else /* CONFIG_UML */
__SYSCALL_64(9, sys_mmap, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(9, __x64_sys_mmap, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(10, __x64_sys_mprotect, )
#else /* CONFIG_UML */
__SYSCALL_64(10, sys_mprotect, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(10, __x64_sys_mprotect, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(11, __x64_sys_munmap, )
#else /* CONFIG_UML */
__SYSCALL_64(11, sys_munmap, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(11, __x64_sys_munmap, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(12, __x64_sys_brk, )
#else /* CONFIG_UML */
__SYSCALL_64(12, sys_brk, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(12, __x64_sys_brk, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(13, __x64_sys_rt_sigaction, )
#else /* CONFIG_UML */
__SYSCALL_64(13, sys_rt_sigaction, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(14, __x64_sys_rt_sigprocmask, )
#else /* CONFIG_UML */
__SYSCALL_64(14, sys_rt_sigprocmask, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(14, __x64_sys_rt_sigprocmask, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(15, __x64_sys_rt_sigreturn, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(15, sys_rt_sigreturn, ptregs)
#endif
#ifdef CONFIG_X86
__SYSCALL_64(16, __x64_sys_ioctl, )
#else /* CONFIG_UML */
__SYSCALL_64(16, sys_ioctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(17, __x64_sys_pread64, )
#else /* CONFIG_UML */
__SYSCALL_64(17, sys_pread64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(17, __x64_sys_pread64, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(18, __x64_sys_pwrite64, )
#else /* CONFIG_UML */
__SYSCALL_64(18, sys_pwrite64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(18, __x64_sys_pwrite64, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(19, __x64_sys_readv, )
#else /* CONFIG_UML */
__SYSCALL_64(19, sys_readv, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(20, __x64_sys_writev, )
#else /* CONFIG_UML */
__SYSCALL_64(20, sys_writev, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(21, __x64_sys_access, )
#else /* CONFIG_UML */
__SYSCALL_64(21, sys_access, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(21, __x64_sys_access, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(22, __x64_sys_pipe, )
#else /* CONFIG_UML */
__SYSCALL_64(22, sys_pipe, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(22, __x64_sys_pipe, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(23, __x64_sys_select, )
#else /* CONFIG_UML */
__SYSCALL_64(23, sys_select, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(23, __x64_sys_select, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(24, __x64_sys_sched_yield, )
#else /* CONFIG_UML */
__SYSCALL_64(24, sys_sched_yield, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(24, __x64_sys_sched_yield, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(25, __x64_sys_mremap, )
#else /* CONFIG_UML */
__SYSCALL_64(25, sys_mremap, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(25, __x64_sys_mremap, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(26, __x64_sys_msync, )
#else /* CONFIG_UML */
__SYSCALL_64(26, sys_msync, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(26, __x64_sys_msync, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(27, __x64_sys_mincore, )
#else /* CONFIG_UML */
__SYSCALL_64(27, sys_mincore, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(27, __x64_sys_mincore, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(28, __x64_sys_madvise, )
#else /* CONFIG_UML */
__SYSCALL_64(28, sys_madvise, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(28, __x64_sys_madvise, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(29, __x64_sys_shmget, )
#else /* CONFIG_UML */
__SYSCALL_64(29, sys_shmget, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(29, __x64_sys_shmget, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(30, __x64_sys_shmat, )
#else /* CONFIG_UML */
__SYSCALL_64(30, sys_shmat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(30, __x64_sys_shmat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(31, __x64_sys_shmctl, )
#else /* CONFIG_UML */
__SYSCALL_64(31, sys_shmctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(31, __x64_sys_shmctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(32, __x64_sys_dup, )
#else /* CONFIG_UML */
__SYSCALL_64(32, sys_dup, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(32, __x64_sys_dup, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(33, __x64_sys_dup2, )
#else /* CONFIG_UML */
__SYSCALL_64(33, sys_dup2, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(33, __x64_sys_dup2, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(34, __x64_sys_pause, )
#else /* CONFIG_UML */
__SYSCALL_64(34, sys_pause, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(34, __x64_sys_pause, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(35, __x64_sys_nanosleep, )
#else /* CONFIG_UML */
__SYSCALL_64(35, sys_nanosleep, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(35, __x64_sys_nanosleep, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(36, __x64_sys_getitimer, )
#else /* CONFIG_UML */
__SYSCALL_64(36, sys_getitimer, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(36, __x64_sys_getitimer, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(37, __x64_sys_alarm, )
#else /* CONFIG_UML */
__SYSCALL_64(37, sys_alarm, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(37, __x64_sys_alarm, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(38, __x64_sys_setitimer, )
#else /* CONFIG_UML */
__SYSCALL_64(38, sys_setitimer, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(38, __x64_sys_setitimer, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(39, __x64_sys_getpid, )
#else /* CONFIG_UML */
__SYSCALL_64(39, sys_getpid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(39, __x64_sys_getpid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(40, __x64_sys_sendfile64, )
#else /* CONFIG_UML */
__SYSCALL_64(40, sys_sendfile64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(40, __x64_sys_sendfile64, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(41, __x64_sys_socket, )
#else /* CONFIG_UML */
__SYSCALL_64(41, sys_socket, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(41, __x64_sys_socket, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(42, __x64_sys_connect, )
#else /* CONFIG_UML */
__SYSCALL_64(42, sys_connect, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(42, __x64_sys_connect, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(43, __x64_sys_accept, )
#else /* CONFIG_UML */
__SYSCALL_64(43, sys_accept, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(43, __x64_sys_accept, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(44, __x64_sys_sendto, )
#else /* CONFIG_UML */
__SYSCALL_64(44, sys_sendto, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(44, __x64_sys_sendto, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(45, __x64_sys_recvfrom, )
#else /* CONFIG_UML */
__SYSCALL_64(45, sys_recvfrom, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(46, __x64_sys_sendmsg, )
#else /* CONFIG_UML */
__SYSCALL_64(46, sys_sendmsg, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(47, __x64_sys_recvmsg, )
#else /* CONFIG_UML */
__SYSCALL_64(47, sys_recvmsg, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(48, __x64_sys_shutdown, )
#else /* CONFIG_UML */
__SYSCALL_64(48, sys_shutdown, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(48, __x64_sys_shutdown, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(49, __x64_sys_bind, )
#else /* CONFIG_UML */
__SYSCALL_64(49, sys_bind, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(49, __x64_sys_bind, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(50, __x64_sys_listen, )
#else /* CONFIG_UML */
__SYSCALL_64(50, sys_listen, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(50, __x64_sys_listen, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(51, __x64_sys_getsockname, )
#else /* CONFIG_UML */
__SYSCALL_64(51, sys_getsockname, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(51, __x64_sys_getsockname, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(52, __x64_sys_getpeername, )
#else /* CONFIG_UML */
__SYSCALL_64(52, sys_getpeername, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(52, __x64_sys_getpeername, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(53, __x64_sys_socketpair, )
#else /* CONFIG_UML */
__SYSCALL_64(53, sys_socketpair, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(53, __x64_sys_socketpair, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(54, __x64_sys_setsockopt, )
#else /* CONFIG_UML */
__SYSCALL_64(54, sys_setsockopt, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(55, __x64_sys_getsockopt, )
#else /* CONFIG_UML */
__SYSCALL_64(55, sys_getsockopt, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(56, __x64_sys_clone, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(56, sys_clone, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(56, __x64_sys_clone, ptregs)
#endif
#ifdef CONFIG_X86
__SYSCALL_64(57, __x64_sys_fork, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(57, sys_fork, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(57, __x64_sys_fork, ptregs)
#endif
#ifdef CONFIG_X86
__SYSCALL_64(58, __x64_sys_vfork, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(58, sys_vfork, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(58, __x64_sys_vfork, ptregs)
#endif
#ifdef CONFIG_X86
__SYSCALL_64(59, __x64_sys_execve, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(59, sys_execve, ptregs)
#endif
#ifdef CONFIG_X86
__SYSCALL_64(60, __x64_sys_exit, )
#else /* CONFIG_UML */
__SYSCALL_64(60, sys_exit, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(60, __x64_sys_exit, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(61, __x64_sys_wait4, )
#else /* CONFIG_UML */
__SYSCALL_64(61, sys_wait4, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(61, __x64_sys_wait4, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(62, __x64_sys_kill, )
#else /* CONFIG_UML */
__SYSCALL_64(62, sys_kill, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(62, __x64_sys_kill, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(63, __x64_sys_newuname, )
#else /* CONFIG_UML */
__SYSCALL_64(63, sys_newuname, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(63, __x64_sys_newuname, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(64, __x64_sys_semget, )
#else /* CONFIG_UML */
__SYSCALL_64(64, sys_semget, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(64, __x64_sys_semget, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(65, __x64_sys_semop, )
#else /* CONFIG_UML */
__SYSCALL_64(65, sys_semop, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(65, __x64_sys_semop, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(66, __x64_sys_semctl, )
#else /* CONFIG_UML */
__SYSCALL_64(66, sys_semctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(66, __x64_sys_semctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(67, __x64_sys_shmdt, )
#else /* CONFIG_UML */
__SYSCALL_64(67, sys_shmdt, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(67, __x64_sys_shmdt, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(68, __x64_sys_msgget, )
#else /* CONFIG_UML */
__SYSCALL_64(68, sys_msgget, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(68, __x64_sys_msgget, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(69, __x64_sys_msgsnd, )
#else /* CONFIG_UML */
__SYSCALL_64(69, sys_msgsnd, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(69, __x64_sys_msgsnd, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(70, __x64_sys_msgrcv, )
#else /* CONFIG_UML */
__SYSCALL_64(70, sys_msgrcv, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(70, __x64_sys_msgrcv, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(71, __x64_sys_msgctl, )
#else /* CONFIG_UML */
__SYSCALL_64(71, sys_msgctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(71, __x64_sys_msgctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(72, __x64_sys_fcntl, )
#else /* CONFIG_UML */
__SYSCALL_64(72, sys_fcntl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(72, __x64_sys_fcntl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(73, __x64_sys_flock, )
#else /* CONFIG_UML */
__SYSCALL_64(73, sys_flock, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(73, __x64_sys_flock, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(74, __x64_sys_fsync, )
#else /* CONFIG_UML */
__SYSCALL_64(74, sys_fsync, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(74, __x64_sys_fsync, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(75, __x64_sys_fdatasync, )
#else /* CONFIG_UML */
__SYSCALL_64(75, sys_fdatasync, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(75, __x64_sys_fdatasync, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(76, __x64_sys_truncate, )
#else /* CONFIG_UML */
__SYSCALL_64(76, sys_truncate, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(76, __x64_sys_truncate, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(77, __x64_sys_ftruncate, )
#else /* CONFIG_UML */
__SYSCALL_64(77, sys_ftruncate, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(77, __x64_sys_ftruncate, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(78, __x64_sys_getdents, )
#else /* CONFIG_UML */
__SYSCALL_64(78, sys_getdents, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(78, __x64_sys_getdents, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(79, __x64_sys_getcwd, )
#else /* CONFIG_UML */
__SYSCALL_64(79, sys_getcwd, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(79, __x64_sys_getcwd, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(80, __x64_sys_chdir, )
#else /* CONFIG_UML */
__SYSCALL_64(80, sys_chdir, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(80, __x64_sys_chdir, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(81, __x64_sys_fchdir, )
#else /* CONFIG_UML */
__SYSCALL_64(81, sys_fchdir, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(81, __x64_sys_fchdir, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(82, __x64_sys_rename, )
#else /* CONFIG_UML */
__SYSCALL_64(82, sys_rename, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(82, __x64_sys_rename, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(83, __x64_sys_mkdir, )
#else /* CONFIG_UML */
__SYSCALL_64(83, sys_mkdir, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(83, __x64_sys_mkdir, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(84, __x64_sys_rmdir, )
#else /* CONFIG_UML */
__SYSCALL_64(84, sys_rmdir, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(84, __x64_sys_rmdir, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(85, __x64_sys_creat, )
#else /* CONFIG_UML */
__SYSCALL_64(85, sys_creat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(85, __x64_sys_creat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(86, __x64_sys_link, )
#else /* CONFIG_UML */
__SYSCALL_64(86, sys_link, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(86, __x64_sys_link, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(87, __x64_sys_unlink, )
#else /* CONFIG_UML */
__SYSCALL_64(87, sys_unlink, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(87, __x64_sys_unlink, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(88, __x64_sys_symlink, )
#else /* CONFIG_UML */
__SYSCALL_64(88, sys_symlink, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(88, __x64_sys_symlink, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(89, __x64_sys_readlink, )
#else /* CONFIG_UML */
__SYSCALL_64(89, sys_readlink, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(89, __x64_sys_readlink, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(90, __x64_sys_chmod, )
#else /* CONFIG_UML */
__SYSCALL_64(90, sys_chmod, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(90, __x64_sys_chmod, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(91, __x64_sys_fchmod, )
#else /* CONFIG_UML */
__SYSCALL_64(91, sys_fchmod, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(91, __x64_sys_fchmod, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(92, __x64_sys_chown, )
#else /* CONFIG_UML */
__SYSCALL_64(92, sys_chown, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(92, __x64_sys_chown, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(93, __x64_sys_fchown, )
#else /* CONFIG_UML */
__SYSCALL_64(93, sys_fchown, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(93, __x64_sys_fchown, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(94, __x64_sys_lchown, )
#else /* CONFIG_UML */
__SYSCALL_64(94, sys_lchown, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(94, __x64_sys_lchown, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(95, __x64_sys_umask, )
#else /* CONFIG_UML */
__SYSCALL_64(95, sys_umask, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(95, __x64_sys_umask, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(96, __x64_sys_gettimeofday, )
#else /* CONFIG_UML */
__SYSCALL_64(96, sys_gettimeofday, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(96, __x64_sys_gettimeofday, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(97, __x64_sys_getrlimit, )
#else /* CONFIG_UML */
__SYSCALL_64(97, sys_getrlimit, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(97, __x64_sys_getrlimit, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(98, __x64_sys_getrusage, )
#else /* CONFIG_UML */
__SYSCALL_64(98, sys_getrusage, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(98, __x64_sys_getrusage, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(99, __x64_sys_sysinfo, )
#else /* CONFIG_UML */
__SYSCALL_64(99, sys_sysinfo, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(99, __x64_sys_sysinfo, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(100, __x64_sys_times, )
#else /* CONFIG_UML */
__SYSCALL_64(100, sys_times, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(100, __x64_sys_times, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(101, __x64_sys_ptrace, )
#else /* CONFIG_UML */
__SYSCALL_64(101, sys_ptrace, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(102, __x64_sys_getuid, )
#else /* CONFIG_UML */
__SYSCALL_64(102, sys_getuid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(102, __x64_sys_getuid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(103, __x64_sys_syslog, )
#else /* CONFIG_UML */
__SYSCALL_64(103, sys_syslog, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(103, __x64_sys_syslog, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(104, __x64_sys_getgid, )
#else /* CONFIG_UML */
__SYSCALL_64(104, sys_getgid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(104, __x64_sys_getgid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(105, __x64_sys_setuid, )
#else /* CONFIG_UML */
__SYSCALL_64(105, sys_setuid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(105, __x64_sys_setuid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(106, __x64_sys_setgid, )
#else /* CONFIG_UML */
__SYSCALL_64(106, sys_setgid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(106, __x64_sys_setgid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(107, __x64_sys_geteuid, )
#else /* CONFIG_UML */
__SYSCALL_64(107, sys_geteuid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(107, __x64_sys_geteuid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(108, __x64_sys_getegid, )
#else /* CONFIG_UML */
__SYSCALL_64(108, sys_getegid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(108, __x64_sys_getegid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(109, __x64_sys_setpgid, )
#else /* CONFIG_UML */
__SYSCALL_64(109, sys_setpgid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(109, __x64_sys_setpgid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(110, __x64_sys_getppid, )
#else /* CONFIG_UML */
__SYSCALL_64(110, sys_getppid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(110, __x64_sys_getppid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(111, __x64_sys_getpgrp, )
#else /* CONFIG_UML */
__SYSCALL_64(111, sys_getpgrp, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(111, __x64_sys_getpgrp, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(112, __x64_sys_setsid, )
#else /* CONFIG_UML */
__SYSCALL_64(112, sys_setsid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(112, __x64_sys_setsid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(113, __x64_sys_setreuid, )
#else /* CONFIG_UML */
__SYSCALL_64(113, sys_setreuid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(113, __x64_sys_setreuid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(114, __x64_sys_setregid, )
#else /* CONFIG_UML */
__SYSCALL_64(114, sys_setregid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(114, __x64_sys_setregid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(115, __x64_sys_getgroups, )
#else /* CONFIG_UML */
__SYSCALL_64(115, sys_getgroups, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(115, __x64_sys_getgroups, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(116, __x64_sys_setgroups, )
#else /* CONFIG_UML */
__SYSCALL_64(116, sys_setgroups, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(116, __x64_sys_setgroups, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(117, __x64_sys_setresuid, )
#else /* CONFIG_UML */
__SYSCALL_64(117, sys_setresuid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(117, __x64_sys_setresuid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(118, __x64_sys_getresuid, )
#else /* CONFIG_UML */
__SYSCALL_64(118, sys_getresuid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(118, __x64_sys_getresuid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(119, __x64_sys_setresgid, )
#else /* CONFIG_UML */
__SYSCALL_64(119, sys_setresgid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(119, __x64_sys_setresgid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(120, __x64_sys_getresgid, )
#else /* CONFIG_UML */
__SYSCALL_64(120, sys_getresgid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(120, __x64_sys_getresgid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(121, __x64_sys_getpgid, )
#else /* CONFIG_UML */
__SYSCALL_64(121, sys_getpgid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(121, __x64_sys_getpgid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(122, __x64_sys_setfsuid, )
#else /* CONFIG_UML */
__SYSCALL_64(122, sys_setfsuid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(122, __x64_sys_setfsuid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(123, __x64_sys_setfsgid, )
#else /* CONFIG_UML */
__SYSCALL_64(123, sys_setfsgid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(123, __x64_sys_setfsgid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(124, __x64_sys_getsid, )
#else /* CONFIG_UML */
__SYSCALL_64(124, sys_getsid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(124, __x64_sys_getsid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(125, __x64_sys_capget, )
#else /* CONFIG_UML */
__SYSCALL_64(125, sys_capget, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(125, __x64_sys_capget, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(126, __x64_sys_capset, )
#else /* CONFIG_UML */
__SYSCALL_64(126, sys_capset, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(126, __x64_sys_capset, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(127, __x64_sys_rt_sigpending, )
#else /* CONFIG_UML */
__SYSCALL_64(127, sys_rt_sigpending, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(128, __x64_sys_rt_sigtimedwait, )
#else /* CONFIG_UML */
__SYSCALL_64(128, sys_rt_sigtimedwait, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(129, __x64_sys_rt_sigqueueinfo, )
#else /* CONFIG_UML */
__SYSCALL_64(129, sys_rt_sigqueueinfo, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(130, __x64_sys_rt_sigsuspend, )
#else /* CONFIG_UML */
__SYSCALL_64(130, sys_rt_sigsuspend, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(130, __x64_sys_rt_sigsuspend, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(131, __x64_sys_sigaltstack, )
#else /* CONFIG_UML */
__SYSCALL_64(131, sys_sigaltstack, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(132, __x64_sys_utime, )
#else /* CONFIG_UML */
__SYSCALL_64(132, sys_utime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(132, __x64_sys_utime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(133, __x64_sys_mknod, )
#else /* CONFIG_UML */
__SYSCALL_64(133, sys_mknod, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(133, __x64_sys_mknod, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(135, __x64_sys_personality, )
#else /* CONFIG_UML */
__SYSCALL_64(135, sys_personality, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(135, __x64_sys_personality, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(136, __x64_sys_ustat, )
#else /* CONFIG_UML */
__SYSCALL_64(136, sys_ustat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(136, __x64_sys_ustat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(137, __x64_sys_statfs, )
#else /* CONFIG_UML */
__SYSCALL_64(137, sys_statfs, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(137, __x64_sys_statfs, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(138, __x64_sys_fstatfs, )
#else /* CONFIG_UML */
__SYSCALL_64(138, sys_fstatfs, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(138, __x64_sys_fstatfs, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(139, __x64_sys_sysfs, )
#else /* CONFIG_UML */
__SYSCALL_64(139, sys_sysfs, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(139, __x64_sys_sysfs, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(140, __x64_sys_getpriority, )
#else /* CONFIG_UML */
__SYSCALL_64(140, sys_getpriority, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(140, __x64_sys_getpriority, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(141, __x64_sys_setpriority, )
#else /* CONFIG_UML */
__SYSCALL_64(141, sys_setpriority, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(141, __x64_sys_setpriority, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(142, __x64_sys_sched_setparam, )
#else /* CONFIG_UML */
__SYSCALL_64(142, sys_sched_setparam, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(142, __x64_sys_sched_setparam, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(143, __x64_sys_sched_getparam, )
#else /* CONFIG_UML */
__SYSCALL_64(143, sys_sched_getparam, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(143, __x64_sys_sched_getparam, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(144, __x64_sys_sched_setscheduler, )
#else /* CONFIG_UML */
__SYSCALL_64(144, sys_sched_setscheduler, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(144, __x64_sys_sched_setscheduler, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(145, __x64_sys_sched_getscheduler, )
#else /* CONFIG_UML */
__SYSCALL_64(145, sys_sched_getscheduler, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(145, __x64_sys_sched_getscheduler, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(146, __x64_sys_sched_get_priority_max, )
#else /* CONFIG_UML */
__SYSCALL_64(146, sys_sched_get_priority_max, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(146, __x64_sys_sched_get_priority_max, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(147, __x64_sys_sched_get_priority_min, )
#else /* CONFIG_UML */
__SYSCALL_64(147, sys_sched_get_priority_min, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(147, __x64_sys_sched_get_priority_min, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(148, __x64_sys_sched_rr_get_interval, )
#else /* CONFIG_UML */
__SYSCALL_64(148, sys_sched_rr_get_interval, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(148, __x64_sys_sched_rr_get_interval, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(149, __x64_sys_mlock, )
#else /* CONFIG_UML */
__SYSCALL_64(149, sys_mlock, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(149, __x64_sys_mlock, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(150, __x64_sys_munlock, )
#else /* CONFIG_UML */
__SYSCALL_64(150, sys_munlock, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(150, __x64_sys_munlock, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(151, __x64_sys_mlockall, )
#else /* CONFIG_UML */
__SYSCALL_64(151, sys_mlockall, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(151, __x64_sys_mlockall, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(152, __x64_sys_munlockall, )
#else /* CONFIG_UML */
__SYSCALL_64(152, sys_munlockall, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(152, __x64_sys_munlockall, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(153, __x64_sys_vhangup, )
#else /* CONFIG_UML */
__SYSCALL_64(153, sys_vhangup, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(153, __x64_sys_vhangup, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(154, __x64_sys_modify_ldt, )
#else /* CONFIG_UML */
__SYSCALL_64(154, sys_modify_ldt, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(154, __x64_sys_modify_ldt, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(155, __x64_sys_pivot_root, )
#else /* CONFIG_UML */
__SYSCALL_64(155, sys_pivot_root, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(155, __x64_sys_pivot_root, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(156, __x64_sys_sysctl, )
#else /* CONFIG_UML */
__SYSCALL_64(156, sys_sysctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(157, __x64_sys_prctl, )
#else /* CONFIG_UML */
__SYSCALL_64(157, sys_prctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(157, __x64_sys_prctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(158, __x64_sys_arch_prctl, )
#else /* CONFIG_UML */
__SYSCALL_64(158, sys_arch_prctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(158, __x64_sys_arch_prctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(159, __x64_sys_adjtimex, )
#else /* CONFIG_UML */
__SYSCALL_64(159, sys_adjtimex, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(159, __x64_sys_adjtimex, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(160, __x64_sys_setrlimit, )
#else /* CONFIG_UML */
__SYSCALL_64(160, sys_setrlimit, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(160, __x64_sys_setrlimit, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(161, __x64_sys_chroot, )
#else /* CONFIG_UML */
__SYSCALL_64(161, sys_chroot, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(161, __x64_sys_chroot, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(162, __x64_sys_sync, )
#else /* CONFIG_UML */
__SYSCALL_64(162, sys_sync, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(162, __x64_sys_sync, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(163, __x64_sys_acct, )
#else /* CONFIG_UML */
__SYSCALL_64(163, sys_acct, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(163, __x64_sys_acct, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(164, __x64_sys_settimeofday, )
#else /* CONFIG_UML */
__SYSCALL_64(164, sys_settimeofday, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(164, __x64_sys_settimeofday, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(165, __x64_sys_mount, )
#else /* CONFIG_UML */
__SYSCALL_64(165, sys_mount, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(165, __x64_sys_mount, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(166, __x64_sys_umount, )
#else /* CONFIG_UML */
__SYSCALL_64(166, sys_umount, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(166, __x64_sys_umount, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(167, __x64_sys_swapon, )
#else /* CONFIG_UML */
__SYSCALL_64(167, sys_swapon, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(167, __x64_sys_swapon, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(168, __x64_sys_swapoff, )
#else /* CONFIG_UML */
__SYSCALL_64(168, sys_swapoff, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(168, __x64_sys_swapoff, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(169, __x64_sys_reboot, )
#else /* CONFIG_UML */
__SYSCALL_64(169, sys_reboot, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(169, __x64_sys_reboot, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(170, __x64_sys_sethostname, )
#else /* CONFIG_UML */
__SYSCALL_64(170, sys_sethostname, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(170, __x64_sys_sethostname, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(171, __x64_sys_setdomainname, )
#else /* CONFIG_UML */
__SYSCALL_64(171, sys_setdomainname, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(171, __x64_sys_setdomainname, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(172, __x64_sys_iopl, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(172, sys_iopl, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(172, __x64_sys_iopl, ptregs)
#endif
#ifdef CONFIG_X86
__SYSCALL_64(173, __x64_sys_ioperm, )
#else /* CONFIG_UML */
__SYSCALL_64(173, sys_ioperm, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(173, __x64_sys_ioperm, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(175, __x64_sys_init_module, )
#else /* CONFIG_UML */
__SYSCALL_64(175, sys_init_module, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(175, __x64_sys_init_module, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(176, __x64_sys_delete_module, )
#else /* CONFIG_UML */
__SYSCALL_64(176, sys_delete_module, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(176, __x64_sys_delete_module, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(179, __x64_sys_quotactl, )
#else /* CONFIG_UML */
__SYSCALL_64(179, sys_quotactl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(179, __x64_sys_quotactl, )
#endif
#ifdef CONFIG_X86_X32_ABI
#endif
#ifdef CONFIG_X86_X32_ABI
#endif
#ifdef CONFIG_X86_X32_ABI
#endif
#ifdef CONFIG_X86_X32_ABI
#endif
#ifdef CONFIG_X86_X32_ABI
#endif
#ifdef CONFIG_X86
__SYSCALL_64(186, __x64_sys_gettid, )
#else /* CONFIG_UML */
__SYSCALL_64(186, sys_gettid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(186, __x64_sys_gettid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(187, __x64_sys_readahead, )
#else /* CONFIG_UML */
__SYSCALL_64(187, sys_readahead, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(187, __x64_sys_readahead, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(188, __x64_sys_setxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(188, sys_setxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(188, __x64_sys_setxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(189, __x64_sys_lsetxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(189, sys_lsetxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(189, __x64_sys_lsetxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(190, __x64_sys_fsetxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(190, sys_fsetxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(190, __x64_sys_fsetxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(191, __x64_sys_getxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(191, sys_getxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(191, __x64_sys_getxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(192, __x64_sys_lgetxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(192, sys_lgetxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(192, __x64_sys_lgetxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(193, __x64_sys_fgetxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(193, sys_fgetxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(193, __x64_sys_fgetxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(194, __x64_sys_listxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(194, sys_listxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(194, __x64_sys_listxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(195, __x64_sys_llistxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(195, sys_llistxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(195, __x64_sys_llistxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(196, __x64_sys_flistxattr, )
#else /* CONFIG_UML */
__SYSCALL_64(196, sys_flistxattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(196, __x64_sys_flistxattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(197, __x64_sys_removexattr, )
#else /* CONFIG_UML */
__SYSCALL_64(197, sys_removexattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(197, __x64_sys_removexattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(198, __x64_sys_lremovexattr, )
#else /* CONFIG_UML */
__SYSCALL_64(198, sys_lremovexattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(198, __x64_sys_lremovexattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(199, __x64_sys_fremovexattr, )
#else /* CONFIG_UML */
__SYSCALL_64(199, sys_fremovexattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(199, __x64_sys_fremovexattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(200, __x64_sys_tkill, )
#else /* CONFIG_UML */
__SYSCALL_64(200, sys_tkill, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(200, __x64_sys_tkill, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(201, __x64_sys_time, )
#else /* CONFIG_UML */
__SYSCALL_64(201, sys_time, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(201, __x64_sys_time, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(202, __x64_sys_futex, )
#else /* CONFIG_UML */
__SYSCALL_64(202, sys_futex, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(202, __x64_sys_futex, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(203, __x64_sys_sched_setaffinity, )
#else /* CONFIG_UML */
__SYSCALL_64(203, sys_sched_setaffinity, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(203, __x64_sys_sched_setaffinity, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(204, __x64_sys_sched_getaffinity, )
#else /* CONFIG_UML */
__SYSCALL_64(204, sys_sched_getaffinity, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(204, __x64_sys_sched_getaffinity, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(206, __x64_sys_io_setup, )
#else /* CONFIG_UML */
__SYSCALL_64(206, sys_io_setup, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(207, __x64_sys_io_destroy, )
#else /* CONFIG_UML */
__SYSCALL_64(207, sys_io_destroy, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(207, __x64_sys_io_destroy, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(208, __x64_sys_io_getevents, )
#else /* CONFIG_UML */
__SYSCALL_64(208, sys_io_getevents, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(208, __x64_sys_io_getevents, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(209, __x64_sys_io_submit, )
#else /* CONFIG_UML */
__SYSCALL_64(209, sys_io_submit, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(210, __x64_sys_io_cancel, )
#else /* CONFIG_UML */
__SYSCALL_64(210, sys_io_cancel, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(210, __x64_sys_io_cancel, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(212, __x64_sys_lookup_dcookie, )
#else /* CONFIG_UML */
__SYSCALL_64(212, sys_lookup_dcookie, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(212, __x64_sys_lookup_dcookie, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(213, __x64_sys_epoll_create, )
#else /* CONFIG_UML */
__SYSCALL_64(213, sys_epoll_create, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(213, __x64_sys_epoll_create, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(216, __x64_sys_remap_file_pages, )
#else /* CONFIG_UML */
__SYSCALL_64(216, sys_remap_file_pages, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(216, __x64_sys_remap_file_pages, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(217, __x64_sys_getdents64, )
#else /* CONFIG_UML */
__SYSCALL_64(217, sys_getdents64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(217, __x64_sys_getdents64, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(218, __x64_sys_set_tid_address, )
#else /* CONFIG_UML */
__SYSCALL_64(218, sys_set_tid_address, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(218, __x64_sys_set_tid_address, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(219, __x64_sys_restart_syscall, )
#else /* CONFIG_UML */
__SYSCALL_64(219, sys_restart_syscall, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(219, __x64_sys_restart_syscall, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(220, __x64_sys_semtimedop, )
#else /* CONFIG_UML */
__SYSCALL_64(220, sys_semtimedop, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(220, __x64_sys_semtimedop, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(221, __x64_sys_fadvise64, )
#else /* CONFIG_UML */
__SYSCALL_64(221, sys_fadvise64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(221, __x64_sys_fadvise64, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(222, __x64_sys_timer_create, )
#else /* CONFIG_UML */
__SYSCALL_64(222, sys_timer_create, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(223, __x64_sys_timer_settime, )
#else /* CONFIG_UML */
__SYSCALL_64(223, sys_timer_settime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(223, __x64_sys_timer_settime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(224, __x64_sys_timer_gettime, )
#else /* CONFIG_UML */
__SYSCALL_64(224, sys_timer_gettime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(224, __x64_sys_timer_gettime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(225, __x64_sys_timer_getoverrun, )
#else /* CONFIG_UML */
__SYSCALL_64(225, sys_timer_getoverrun, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(225, __x64_sys_timer_getoverrun, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(226, __x64_sys_timer_delete, )
#else /* CONFIG_UML */
__SYSCALL_64(226, sys_timer_delete, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(226, __x64_sys_timer_delete, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(227, __x64_sys_clock_settime, )
#else /* CONFIG_UML */
__SYSCALL_64(227, sys_clock_settime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(227, __x64_sys_clock_settime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(228, __x64_sys_clock_gettime, )
#else /* CONFIG_UML */
__SYSCALL_64(228, sys_clock_gettime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(228, __x64_sys_clock_gettime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(229, __x64_sys_clock_getres, )
#else /* CONFIG_UML */
__SYSCALL_64(229, sys_clock_getres, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(229, __x64_sys_clock_getres, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(230, __x64_sys_clock_nanosleep, )
#else /* CONFIG_UML */
__SYSCALL_64(230, sys_clock_nanosleep, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(230, __x64_sys_clock_nanosleep, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(231, __x64_sys_exit_group, )
#else /* CONFIG_UML */
__SYSCALL_64(231, sys_exit_group, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(231, __x64_sys_exit_group, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(232, __x64_sys_epoll_wait, )
#else /* CONFIG_UML */
__SYSCALL_64(232, sys_epoll_wait, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(232, __x64_sys_epoll_wait, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(233, __x64_sys_epoll_ctl, )
#else /* CONFIG_UML */
__SYSCALL_64(233, sys_epoll_ctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(233, __x64_sys_epoll_ctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(234, __x64_sys_tgkill, )
#else /* CONFIG_UML */
__SYSCALL_64(234, sys_tgkill, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(234, __x64_sys_tgkill, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(235, __x64_sys_utimes, )
#else /* CONFIG_UML */
__SYSCALL_64(235, sys_utimes, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(235, __x64_sys_utimes, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(237, __x64_sys_mbind, )
#else /* CONFIG_UML */
__SYSCALL_64(237, sys_mbind, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(237, __x64_sys_mbind, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(238, __x64_sys_set_mempolicy, )
#else /* CONFIG_UML */
__SYSCALL_64(238, sys_set_mempolicy, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(238, __x64_sys_set_mempolicy, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(239, __x64_sys_get_mempolicy, )
#else /* CONFIG_UML */
__SYSCALL_64(239, sys_get_mempolicy, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(239, __x64_sys_get_mempolicy, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(240, __x64_sys_mq_open, )
#else /* CONFIG_UML */
__SYSCALL_64(240, sys_mq_open, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(240, __x64_sys_mq_open, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(241, __x64_sys_mq_unlink, )
#else /* CONFIG_UML */
__SYSCALL_64(241, sys_mq_unlink, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(241, __x64_sys_mq_unlink, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(242, __x64_sys_mq_timedsend, )
#else /* CONFIG_UML */
__SYSCALL_64(242, sys_mq_timedsend, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(242, __x64_sys_mq_timedsend, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(243, __x64_sys_mq_timedreceive, )
#else /* CONFIG_UML */
__SYSCALL_64(243, sys_mq_timedreceive, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(243, __x64_sys_mq_timedreceive, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(244, __x64_sys_mq_notify, )
#else /* CONFIG_UML */
__SYSCALL_64(244, sys_mq_notify, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(245, __x64_sys_mq_getsetattr, )
#else /* CONFIG_UML */
__SYSCALL_64(245, sys_mq_getsetattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(245, __x64_sys_mq_getsetattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(246, __x64_sys_kexec_load, )
#else /* CONFIG_UML */
__SYSCALL_64(246, sys_kexec_load, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(247, __x64_sys_waitid, )
#else /* CONFIG_UML */
__SYSCALL_64(247, sys_waitid, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(248, __x64_sys_add_key, )
#else /* CONFIG_UML */
__SYSCALL_64(248, sys_add_key, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(248, __x64_sys_add_key, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(249, __x64_sys_request_key, )
#else /* CONFIG_UML */
__SYSCALL_64(249, sys_request_key, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(249, __x64_sys_request_key, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(250, __x64_sys_keyctl, )
#else /* CONFIG_UML */
__SYSCALL_64(250, sys_keyctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(250, __x64_sys_keyctl, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(251, __x64_sys_ioprio_set, )
#else /* CONFIG_UML */
__SYSCALL_64(251, sys_ioprio_set, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(251, __x64_sys_ioprio_set, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(252, __x64_sys_ioprio_get, )
#else /* CONFIG_UML */
__SYSCALL_64(252, sys_ioprio_get, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(252, __x64_sys_ioprio_get, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(253, __x64_sys_inotify_init, )
#else /* CONFIG_UML */
__SYSCALL_64(253, sys_inotify_init, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(253, __x64_sys_inotify_init, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(254, __x64_sys_inotify_add_watch, )
#else /* CONFIG_UML */
__SYSCALL_64(254, sys_inotify_add_watch, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(254, __x64_sys_inotify_add_watch, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(255, __x64_sys_inotify_rm_watch, )
#else /* CONFIG_UML */
__SYSCALL_64(255, sys_inotify_rm_watch, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(255, __x64_sys_inotify_rm_watch, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(256, __x64_sys_migrate_pages, )
#else /* CONFIG_UML */
__SYSCALL_64(256, sys_migrate_pages, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(256, __x64_sys_migrate_pages, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(257, __x64_sys_openat, )
#else /* CONFIG_UML */
__SYSCALL_64(257, sys_openat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(257, __x64_sys_openat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(258, __x64_sys_mkdirat, )
#else /* CONFIG_UML */
__SYSCALL_64(258, sys_mkdirat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(258, __x64_sys_mkdirat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(259, __x64_sys_mknodat, )
#else /* CONFIG_UML */
__SYSCALL_64(259, sys_mknodat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(259, __x64_sys_mknodat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(260, __x64_sys_fchownat, )
#else /* CONFIG_UML */
__SYSCALL_64(260, sys_fchownat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(260, __x64_sys_fchownat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(261, __x64_sys_futimesat, )
#else /* CONFIG_UML */
__SYSCALL_64(261, sys_futimesat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(261, __x64_sys_futimesat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(262, __x64_sys_newfstatat, )
#else /* CONFIG_UML */
__SYSCALL_64(262, sys_newfstatat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(262, __x64_sys_newfstatat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(263, __x64_sys_unlinkat, )
#else /* CONFIG_UML */
__SYSCALL_64(263, sys_unlinkat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(263, __x64_sys_unlinkat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(264, __x64_sys_renameat, )
#else /* CONFIG_UML */
__SYSCALL_64(264, sys_renameat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(264, __x64_sys_renameat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(265, __x64_sys_linkat, )
#else /* CONFIG_UML */
__SYSCALL_64(265, sys_linkat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(265, __x64_sys_linkat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(266, __x64_sys_symlinkat, )
#else /* CONFIG_UML */
__SYSCALL_64(266, sys_symlinkat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(266, __x64_sys_symlinkat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(267, __x64_sys_readlinkat, )
#else /* CONFIG_UML */
__SYSCALL_64(267, sys_readlinkat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(267, __x64_sys_readlinkat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(268, __x64_sys_fchmodat, )
#else /* CONFIG_UML */
__SYSCALL_64(268, sys_fchmodat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(268, __x64_sys_fchmodat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(269, __x64_sys_faccessat, )
#else /* CONFIG_UML */
__SYSCALL_64(269, sys_faccessat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(269, __x64_sys_faccessat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(270, __x64_sys_pselect6, )
#else /* CONFIG_UML */
__SYSCALL_64(270, sys_pselect6, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(270, __x64_sys_pselect6, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(271, __x64_sys_ppoll, )
#else /* CONFIG_UML */
__SYSCALL_64(271, sys_ppoll, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(271, __x64_sys_ppoll, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(272, __x64_sys_unshare, )
#else /* CONFIG_UML */
__SYSCALL_64(272, sys_unshare, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(272, __x64_sys_unshare, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(273, __x64_sys_set_robust_list, )
#else /* CONFIG_UML */
__SYSCALL_64(273, sys_set_robust_list, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(274, __x64_sys_get_robust_list, )
#else /* CONFIG_UML */
__SYSCALL_64(274, sys_get_robust_list, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(275, __x64_sys_splice, )
#else /* CONFIG_UML */
__SYSCALL_64(275, sys_splice, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(275, __x64_sys_splice, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(276, __x64_sys_tee, )
#else /* CONFIG_UML */
__SYSCALL_64(276, sys_tee, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(276, __x64_sys_tee, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(277, __x64_sys_sync_file_range, )
#else /* CONFIG_UML */
__SYSCALL_64(277, sys_sync_file_range, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(277, __x64_sys_sync_file_range, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(278, __x64_sys_vmsplice, )
#else /* CONFIG_UML */
__SYSCALL_64(278, sys_vmsplice, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(279, __x64_sys_move_pages, )
#else /* CONFIG_UML */
__SYSCALL_64(279, sys_move_pages, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(280, __x64_sys_utimensat, )
#else /* CONFIG_UML */
__SYSCALL_64(280, sys_utimensat, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(280, __x64_sys_utimensat, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(281, __x64_sys_epoll_pwait, )
#else /* CONFIG_UML */
__SYSCALL_64(281, sys_epoll_pwait, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(281, __x64_sys_epoll_pwait, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(282, __x64_sys_signalfd, )
#else /* CONFIG_UML */
__SYSCALL_64(282, sys_signalfd, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(282, __x64_sys_signalfd, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(283, __x64_sys_timerfd_create, )
#else /* CONFIG_UML */
__SYSCALL_64(283, sys_timerfd_create, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(283, __x64_sys_timerfd_create, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(284, __x64_sys_eventfd, )
#else /* CONFIG_UML */
__SYSCALL_64(284, sys_eventfd, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(284, __x64_sys_eventfd, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(285, __x64_sys_fallocate, )
#else /* CONFIG_UML */
__SYSCALL_64(285, sys_fallocate, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(285, __x64_sys_fallocate, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(286, __x64_sys_timerfd_settime, )
#else /* CONFIG_UML */
__SYSCALL_64(286, sys_timerfd_settime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(286, __x64_sys_timerfd_settime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(287, __x64_sys_timerfd_gettime, )
#else /* CONFIG_UML */
__SYSCALL_64(287, sys_timerfd_gettime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(287, __x64_sys_timerfd_gettime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(288, __x64_sys_accept4, )
#else /* CONFIG_UML */
__SYSCALL_64(288, sys_accept4, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(288, __x64_sys_accept4, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(289, __x64_sys_signalfd4, )
#else /* CONFIG_UML */
__SYSCALL_64(289, sys_signalfd4, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(289, __x64_sys_signalfd4, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(290, __x64_sys_eventfd2, )
#else /* CONFIG_UML */
__SYSCALL_64(290, sys_eventfd2, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(290, __x64_sys_eventfd2, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(291, __x64_sys_epoll_create1, )
#else /* CONFIG_UML */
__SYSCALL_64(291, sys_epoll_create1, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(291, __x64_sys_epoll_create1, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(292, __x64_sys_dup3, )
#else /* CONFIG_UML */
__SYSCALL_64(292, sys_dup3, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(292, __x64_sys_dup3, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(293, __x64_sys_pipe2, )
#else /* CONFIG_UML */
__SYSCALL_64(293, sys_pipe2, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(293, __x64_sys_pipe2, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(294, __x64_sys_inotify_init1, )
#else /* CONFIG_UML */
__SYSCALL_64(294, sys_inotify_init1, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(294, __x64_sys_inotify_init1, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(295, __x64_sys_preadv, )
#else /* CONFIG_UML */
__SYSCALL_64(295, sys_preadv, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(296, __x64_sys_pwritev, )
#else /* CONFIG_UML */
__SYSCALL_64(296, sys_pwritev, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(297, __x64_sys_rt_tgsigqueueinfo, )
#else /* CONFIG_UML */
__SYSCALL_64(297, sys_rt_tgsigqueueinfo, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(298, __x64_sys_perf_event_open, )
#else /* CONFIG_UML */
__SYSCALL_64(298, sys_perf_event_open, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(298, __x64_sys_perf_event_open, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(299, __x64_sys_recvmmsg, )
#else /* CONFIG_UML */
__SYSCALL_64(299, sys_recvmmsg, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(300, __x64_sys_fanotify_init, )
#else /* CONFIG_UML */
__SYSCALL_64(300, sys_fanotify_init, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(300, __x64_sys_fanotify_init, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(301, __x64_sys_fanotify_mark, )
#else /* CONFIG_UML */
__SYSCALL_64(301, sys_fanotify_mark, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(301, __x64_sys_fanotify_mark, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(302, __x64_sys_prlimit64, )
#else /* CONFIG_UML */
__SYSCALL_64(302, sys_prlimit64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(302, __x64_sys_prlimit64, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(303, __x64_sys_name_to_handle_at, )
#else /* CONFIG_UML */
__SYSCALL_64(303, sys_name_to_handle_at, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(303, __x64_sys_name_to_handle_at, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(304, __x64_sys_open_by_handle_at, )
#else /* CONFIG_UML */
__SYSCALL_64(304, sys_open_by_handle_at, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(304, __x64_sys_open_by_handle_at, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(305, __x64_sys_clock_adjtime, )
#else /* CONFIG_UML */
__SYSCALL_64(305, sys_clock_adjtime, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(305, __x64_sys_clock_adjtime, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(306, __x64_sys_syncfs, )
#else /* CONFIG_UML */
__SYSCALL_64(306, sys_syncfs, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(306, __x64_sys_syncfs, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(307, __x64_sys_sendmmsg, )
#else /* CONFIG_UML */
__SYSCALL_64(307, sys_sendmmsg, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(308, __x64_sys_setns, )
#else /* CONFIG_UML */
__SYSCALL_64(308, sys_setns, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(308, __x64_sys_setns, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(309, __x64_sys_getcpu, )
#else /* CONFIG_UML */
__SYSCALL_64(309, sys_getcpu, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(309, __x64_sys_getcpu, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(310, __x64_sys_process_vm_readv, )
#else /* CONFIG_UML */
__SYSCALL_64(310, sys_process_vm_readv, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(311, __x64_sys_process_vm_writev, )
#else /* CONFIG_UML */
__SYSCALL_64(311, sys_process_vm_writev, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(312, __x64_sys_kcmp, )
#else /* CONFIG_UML */
__SYSCALL_64(312, sys_kcmp, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(312, __x64_sys_kcmp, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(313, __x64_sys_finit_module, )
#else /* CONFIG_UML */
__SYSCALL_64(313, sys_finit_module, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(313, __x64_sys_finit_module, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(314, __x64_sys_sched_setattr, )
#else /* CONFIG_UML */
__SYSCALL_64(314, sys_sched_setattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(314, __x64_sys_sched_setattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(315, __x64_sys_sched_getattr, )
#else /* CONFIG_UML */
__SYSCALL_64(315, sys_sched_getattr, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(315, __x64_sys_sched_getattr, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(316, __x64_sys_renameat2, )
#else /* CONFIG_UML */
__SYSCALL_64(316, sys_renameat2, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(316, __x64_sys_renameat2, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(317, __x64_sys_seccomp, )
#else /* CONFIG_UML */
__SYSCALL_64(317, sys_seccomp, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(317, __x64_sys_seccomp, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(318, __x64_sys_getrandom, )
#else /* CONFIG_UML */
__SYSCALL_64(318, sys_getrandom, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(318, __x64_sys_getrandom, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(319, __x64_sys_memfd_create, )
#else /* CONFIG_UML */
__SYSCALL_64(319, sys_memfd_create, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(319, __x64_sys_memfd_create, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(320, __x64_sys_kexec_file_load, )
#else /* CONFIG_UML */
__SYSCALL_64(320, sys_kexec_file_load, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(320, __x64_sys_kexec_file_load, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(321, __x64_sys_bpf, )
#else /* CONFIG_UML */
__SYSCALL_64(321, sys_bpf, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(321, __x64_sys_bpf, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(322, __x64_sys_execveat, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(322, sys_execveat, ptregs)
#endif
#ifdef CONFIG_X86
__SYSCALL_64(323, __x64_sys_userfaultfd, )
#else /* CONFIG_UML */
__SYSCALL_64(323, sys_userfaultfd, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(323, __x64_sys_userfaultfd, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(324, __x64_sys_membarrier, )
#else /* CONFIG_UML */
__SYSCALL_64(324, sys_membarrier, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(324, __x64_sys_membarrier, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(325, __x64_sys_mlock2, )
#else /* CONFIG_UML */
__SYSCALL_64(325, sys_mlock2, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(325, __x64_sys_mlock2, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(326, __x64_sys_copy_file_range, )
#else /* CONFIG_UML */
__SYSCALL_64(326, sys_copy_file_range, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(326, __x64_sys_copy_file_range, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(327, __x64_sys_preadv2, )
#else /* CONFIG_UML */
__SYSCALL_64(327, sys_preadv2, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(328, __x64_sys_pwritev2, )
#else /* CONFIG_UML */
__SYSCALL_64(328, sys_pwritev2, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(329, __x64_sys_pkey_mprotect, )
#else /* CONFIG_UML */
__SYSCALL_64(329, sys_pkey_mprotect, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(329, __x64_sys_pkey_mprotect, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(330, __x64_sys_pkey_alloc, )
#else /* CONFIG_UML */
__SYSCALL_64(330, sys_pkey_alloc, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(330, __x64_sys_pkey_alloc, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(331, __x64_sys_pkey_free, )
#else /* CONFIG_UML */
__SYSCALL_64(331, sys_pkey_free, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(331, __x64_sys_pkey_free, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(332, __x64_sys_statx, )
#else /* CONFIG_UML */
__SYSCALL_64(332, sys_statx, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(332, __x64_sys_statx, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(333, __x64_sys_io_pgetevents, )
#else /* CONFIG_UML */
__SYSCALL_64(333, sys_io_pgetevents, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(333, __x64_sys_io_pgetevents, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(334, __x64_sys_rseq, )
#else /* CONFIG_UML */
__SYSCALL_64(334, sys_rseq, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(334, __x64_sys_rseq, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(424, __x64_sys_pidfd_send_signal, )
#else /* CONFIG_UML */
__SYSCALL_64(424, sys_pidfd_send_signal, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(424, __x64_sys_pidfd_send_signal, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(425, __x64_sys_io_uring_setup, )
#else /* CONFIG_UML */
__SYSCALL_64(425, sys_io_uring_setup, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(425, __x64_sys_io_uring_setup, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(426, __x64_sys_io_uring_enter, )
#else /* CONFIG_UML */
__SYSCALL_64(426, sys_io_uring_enter, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(426, __x64_sys_io_uring_enter, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(427, __x64_sys_io_uring_register, )
#else /* CONFIG_UML */
__SYSCALL_64(427, sys_io_uring_register, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(427, __x64_sys_io_uring_register, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(428, __x64_sys_open_tree, )
#else /* CONFIG_UML */
__SYSCALL_64(428, sys_open_tree, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(428, __x64_sys_open_tree, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(429, __x64_sys_move_mount, )
#else /* CONFIG_UML */
__SYSCALL_64(429, sys_move_mount, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(429, __x64_sys_move_mount, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(430, __x64_sys_fsopen, )
#else /* CONFIG_UML */
__SYSCALL_64(430, sys_fsopen, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(430, __x64_sys_fsopen, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(431, __x64_sys_fsconfig, )
#else /* CONFIG_UML */
__SYSCALL_64(431, sys_fsconfig, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(431, __x64_sys_fsconfig, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(432, __x64_sys_fsmount, )
#else /* CONFIG_UML */
__SYSCALL_64(432, sys_fsmount, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(432, __x64_sys_fsmount, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(433, __x64_sys_fspick, )
#else /* CONFIG_UML */
__SYSCALL_64(433, sys_fspick, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(433, __x64_sys_fspick, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(434, __x64_sys_pidfd_open, )
#else /* CONFIG_UML */
__SYSCALL_64(434, sys_pidfd_open, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(434, __x64_sys_pidfd_open, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(435, __x64_sys_clone3, ptregs)
#else /* CONFIG_UML */
__SYSCALL_64(435, sys_clone3, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(435, __x64_sys_clone3, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(512, __x32_compat_sys_rt_sigaction, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(513, sys32_x32_rt_sigreturn, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(514, __x32_compat_sys_ioctl, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(515, __x32_compat_sys_readv, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(516, __x32_compat_sys_writev, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(517, __x32_compat_sys_recvfrom, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(518, __x32_compat_sys_sendmsg, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(519, __x32_compat_sys_recvmsg, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(520, __x32_compat_sys_execve, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(521, __x32_compat_sys_ptrace, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(522, __x32_compat_sys_rt_sigpending, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(523, __x32_compat_sys_rt_sigtimedwait_time64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(524, __x32_compat_sys_rt_sigqueueinfo, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(525, __x32_compat_sys_sigaltstack, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(526, __x32_compat_sys_timer_create, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(527, __x32_compat_sys_mq_notify, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(528, __x32_compat_sys_kexec_load, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(529, __x32_compat_sys_waitid, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(530, __x32_compat_sys_set_robust_list, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(531, __x32_compat_sys_get_robust_list, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(532, __x32_compat_sys_vmsplice, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(533, __x32_compat_sys_move_pages, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(534, __x32_compat_sys_preadv64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(535, __x32_compat_sys_pwritev64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(536, __x32_compat_sys_rt_tgsigqueueinfo, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(537, __x32_compat_sys_recvmmsg_time64, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(538, __x32_compat_sys_sendmmsg, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(539, __x32_compat_sys_process_vm_readv, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(540, __x32_compat_sys_process_vm_writev, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(541, __x32_compat_sys_setsockopt, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(542, __x32_compat_sys_getsockopt, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(543, __x32_compat_sys_io_setup, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(544, __x32_compat_sys_io_submit, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(545, __x32_compat_sys_execveat, ptregs)
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(546, __x32_compat_sys_preadv64v2, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_X32(547, __x32_compat_sys_pwritev64v2, )
#endif
