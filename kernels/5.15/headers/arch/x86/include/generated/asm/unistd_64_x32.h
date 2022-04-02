#ifndef _UAPI_ASM_UNISTD_64_X32_H
#define _UAPI_ASM_UNISTD_64_X32_H

#define __NR_x32_rt_sigaction 512
#define __NR_x32_rt_sigreturn 513
#define __NR_x32_ioctl 514
#define __NR_x32_readv 515
#define __NR_x32_writev 516
#define __NR_x32_recvfrom 517
#define __NR_x32_sendmsg 518
#define __NR_x32_recvmsg 519
#define __NR_x32_execve 520
#define __NR_x32_ptrace 521
#define __NR_x32_rt_sigpending 522
#define __NR_x32_rt_sigtimedwait 523
#define __NR_x32_rt_sigqueueinfo 524
#define __NR_x32_sigaltstack 525
#define __NR_x32_timer_create 526
#define __NR_x32_mq_notify 527
#define __NR_x32_kexec_load 528
#define __NR_x32_waitid 529
#define __NR_x32_set_robust_list 530
#define __NR_x32_get_robust_list 531
#define __NR_x32_vmsplice 532
#define __NR_x32_move_pages 533
#define __NR_x32_preadv 534
#define __NR_x32_pwritev 535
#define __NR_x32_rt_tgsigqueueinfo 536
#define __NR_x32_recvmmsg 537
#define __NR_x32_sendmmsg 538
#define __NR_x32_process_vm_readv 539
#define __NR_x32_process_vm_writev 540
#define __NR_x32_setsockopt 541
#define __NR_x32_getsockopt 542
#define __NR_x32_io_setup 543
#define __NR_x32_io_submit 544
#define __NR_x32_execveat 545
#define __NR_x32_preadv2 546
#define __NR_x32_pwritev2 547

#ifdef __KERNEL__
#define __NR_x32_syscalls 548
#endif

#endif /* _UAPI_ASM_UNISTD_64_X32_H */
