/*
 * Chromium OS alt-syscall tables
 *
 * Copyright (C) 2015 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/alt-syscall.h>
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/prctl.h>
#include <linux/sched/types.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/syscalls.h>
#include <linux/timex.h>
#include <uapi/linux/sched/types.h>

#include <asm/unistd.h>

#include "alt-syscall.h"
#include "android_whitelists.h"
#include "complete_whitelists.h"
#include "read_write_test_whitelists.h"
#include "third_party_whitelists.h"

/* Intercept and log blocked syscalls. */
static asmlinkage long block_syscall(struct pt_regs *regs)
{
	struct task_struct *task = current;

	pr_warn_ratelimited("[%d] %s: blocked syscall %d\n", task_pid_nr(task),
		task->comm, syscall_get_nr(task, regs));

	return -ENOSYS;
}

/*
 * In permissive mode, warn that the syscall was blocked, but still allow
 * it to go through.  Note that since we don't have an easy way to map from
 * syscall to number of arguments, we pass the maximum (6).
 */
static asmlinkage long warn_syscall(struct pt_regs *regs)
{
	struct task_struct *task = current;
	int nr = syscall_get_nr(task, regs);
	sys_call_ptr_t fn = (sys_call_ptr_t)default_table.table[nr];

	pr_warn_ratelimited("[%d] %s: syscall %d not whitelisted\n",
			    task_pid_nr(task), task->comm, nr);

	return fn(regs);
}

#ifdef CONFIG_COMPAT
static asmlinkage long warn_compat_syscall(struct pt_regs *regs)
{
	struct task_struct *task = current;
	int nr = syscall_get_nr(task, regs);
	sys_call_ptr_t fn = (sys_call_ptr_t)default_table.compat_table[nr];

	pr_warn_ratelimited("[%d] %s: compat syscall %d not whitelisted\n",
			    task_pid_nr(task), task->comm, nr);

	return fn(regs);
}
#endif /* CONFIG_COMPAT */

static asmlinkage long alt_sys_prctl(struct pt_regs *regs)
{
	struct task_struct *task = current;
	unsigned long args[6];

	syscall_get_arguments(task, regs, args);

	if (args[0] == PR_ALT_SYSCALL &&
	    args[1] == PR_ALT_SYSCALL_SET_SYSCALL_TABLE)
		return -EPERM;

	return ksys_prctl(args[0], args[1], args[2], args[3], args[4]);
}

/* Thread priority used by Android. */
#define ANDROID_PRIORITY_FOREGROUND     -2
#define ANDROID_PRIORITY_DISPLAY        -4
#define ANDROID_PRIORITY_URGENT_DISPLAY -8
#define ANDROID_PRIORITY_AUDIO         -16
#define ANDROID_PRIORITY_URGENT_AUDIO  -19
#define ANDROID_PRIORITY_HIGHEST       -20

/* Reduced priority when running inside container. */
#define CONTAINER_PRIORITY_FOREGROUND     -1
#define CONTAINER_PRIORITY_DISPLAY        -2
#define CONTAINER_PRIORITY_URGENT_DISPLAY -4
#define CONTAINER_PRIORITY_AUDIO          -8
#define CONTAINER_PRIORITY_URGENT_AUDIO   -9
#define CONTAINER_PRIORITY_HIGHEST       -10

/*
 * TODO(mortonm): Move the implementation of these Android-specific
 * alt-syscalls (starting with android_*) to their own .c file.
 */
static asmlinkage long android_getpriority(struct pt_regs *regs)
{
	struct task_struct *task = current;
	long prio, nice;
	unsigned long args[6];
	int which, who;

	syscall_get_arguments(task, regs, args);
	which = args[0];
	who = args[1];

	prio = ksys_getpriority(which, who);
	if (prio <= 20)
		return prio;

	nice = -(prio - 20);
	switch (nice) {
	case CONTAINER_PRIORITY_FOREGROUND:
		nice = ANDROID_PRIORITY_FOREGROUND;
		break;
	case CONTAINER_PRIORITY_DISPLAY:
		nice = ANDROID_PRIORITY_DISPLAY;
		break;
	case CONTAINER_PRIORITY_URGENT_DISPLAY:
		nice = ANDROID_PRIORITY_URGENT_DISPLAY;
		break;
	case CONTAINER_PRIORITY_AUDIO:
		nice = ANDROID_PRIORITY_AUDIO;
		break;
	case CONTAINER_PRIORITY_URGENT_AUDIO:
		nice = ANDROID_PRIORITY_URGENT_AUDIO;
		break;
	case CONTAINER_PRIORITY_HIGHEST:
		nice = ANDROID_PRIORITY_HIGHEST;
		break;
	}

	return -nice + 20;
}

static asmlinkage long android_keyctl(struct pt_regs *regs)
{
	return -EACCES;
}


static asmlinkage long android_setpriority(struct pt_regs *regs)
{
	struct task_struct *task = current;
	unsigned long args[6];
	int which, who, niceval;

	syscall_get_arguments(task, regs, args);
	which = args[0];
	who = args[1];
	niceval = args[2];

	if (niceval < 0) {
		if (niceval < -20)
			niceval = -20;
		niceval = niceval / 2;
	}
	return ksys_setpriority(which, who, niceval);
}

static asmlinkage long
do_android_sched_setscheduler(pid_t pid, int policy,
			      struct sched_param __user *param)
{
	struct sched_param lparam;
	struct task_struct *p;
	long retval;

	if (!param || pid < 0)
		return -EINVAL;
	if (copy_from_user(&lparam, param, sizeof(struct sched_param)))
		return -EFAULT;

	rcu_read_lock();
	retval = -ESRCH;
	p = pid ? find_task_by_vpid(pid) : current;
	if (likely(p))
		get_task_struct(p);
	rcu_read_unlock();

	if (likely(p)) {
		const struct cred *cred = current_cred();
		kuid_t android_root_uid, android_system_uid;

		/*
		 * Allow root(0) and system(1000) processes to set RT scheduler.
		 *
		 * The system_server process run under system provides
		 * SchedulingPolicyService which is used by audioflinger and
		 * other services to boost their threads, so allow it to set RT
		 * scheduler for other threads.
		 */
		android_root_uid = make_kuid(cred->user_ns, 0);
		android_system_uid = make_kuid(cred->user_ns, 1000);
		if ((uid_eq(cred->euid, android_root_uid) ||
		     uid_eq(cred->euid, android_system_uid)) &&
		    ns_capable(cred->user_ns, CAP_SYS_NICE))
			retval = sched_setscheduler_nocheck(p, policy, &lparam);
		else
			retval = sched_setscheduler(p, policy, &lparam);
		put_task_struct(p);
	}

	return retval;
}

static asmlinkage long
android_sched_setscheduler(struct pt_regs *regs)
{
	struct task_struct *task = current;
	unsigned long args[6];
	pid_t pid;
	int policy;
	struct sched_param __user *param;

	syscall_get_arguments(task, regs, args);
	pid = args[0];
	policy = args[1];
	param = (struct sched_param __user *)args[2];

	/* negative values for policy are not valid */
	if (policy < 0)
		return -EINVAL;
	return do_android_sched_setscheduler(pid, policy, param);
}

/*
 * sched_setparam() passes in -1 for its policy, to let the functions
 * it calls know not to change it.
 */
#define SETPARAM_POLICY -1

static asmlinkage long android_sched_setparam(struct pt_regs *regs)
{
	struct task_struct *task = current;
	unsigned long args[6];
	pid_t pid;
	struct sched_param __user *param;

	syscall_get_arguments(task, regs, args);
	pid = args[0];
	param = (struct sched_param __user *)args[1];

        return do_android_sched_setscheduler(pid, SETPARAM_POLICY, param);
}

static asmlinkage int __maybe_unused android_socket(struct pt_regs *regs)
{
	struct task_struct *task = current;
	unsigned long args[6];
	int domain, type, socket;

	syscall_get_arguments(task, regs, args);
	domain = args[0];
	type = args[1];
	socket = args[2];

	if (domain == AF_VSOCK)
	       return -EACCES;
	return __sys_socket(domain, type, socket);
}

static asmlinkage long android_perf_event_open(struct pt_regs *regs)
{
	struct task_struct *task = current;
	unsigned long args[6];
	struct perf_event_attr __user *attr_uptr;
	pid_t pid;
	int cpu, group_fd;
	unsigned long flags;

	if (!allow_devmode_syscalls)
		return -EACCES;

	syscall_get_arguments(task, regs, args);
	attr_uptr = (struct perf_event_attr __user *)args[0];
	pid = args[1];
	cpu = args[2];
	group_fd = args[3];
	flags = args[4];

	return ksys_perf_event_open(attr_uptr, pid, cpu, group_fd, flags);
}

static asmlinkage long android_adjtimex(struct pt_regs *regs)
{
	struct task_struct *task = current;
	struct __kernel_timex kbuf;
	struct __kernel_timex __user *buf;
	unsigned long args[6];

	syscall_get_arguments(task, regs, args);
	buf = (struct __kernel_timex __user *)args[0];

	/* adjtimex() is allowed only for read. */
	if (copy_from_user(&kbuf, buf, sizeof(struct __kernel_timex)))
		return -EFAULT;
	if (kbuf.modes != 0)
		return -EPERM;
	return ksys_adjtimex(buf);
}

static asmlinkage long android_clock_adjtime(struct pt_regs *regs)
{
	struct task_struct *task = current;
	struct __kernel_timex kbuf;
	unsigned long args[6];
	clockid_t which_clock;
	struct __kernel_timex __user *buf;

	syscall_get_arguments(task, regs, args);
	which_clock = args[0];
	buf = (struct __kernel_timex __user *)args[1];

	/* clock_adjtime() is allowed only for read. */
	if (copy_from_user(&kbuf, buf, sizeof(struct __kernel_timex)))
		return -EFAULT;
	if (kbuf.modes != 0)
		return -EPERM;
	return ksys_clock_adjtime(which_clock, buf);
}

static asmlinkage long android_getcpu(struct pt_regs *regs)
{
	struct task_struct *task = current;
	unsigned long args[6];
	unsigned __user *cpu;
	unsigned __user *node;
	struct getcpu_cache __user *tcache;

	syscall_get_arguments(task, regs, args);
	cpu = (unsigned __user *)args[0];
	node = (unsigned __user *)args[1];
	tcache = (struct getcpu_cache __user *)args[2];

	if (node || tcache)
		return -EPERM;
	return ksys_getcpu(cpu, node, tcache);
}

#ifdef CONFIG_COMPAT
static asmlinkage long android_compat_adjtimex(struct pt_regs *regs)
{
	struct task_struct *task = current;
	struct old_timex32 kbuf;
	struct old_timex32 __user *buf;
	unsigned long args[6];

	syscall_get_arguments(task, regs, args);
	buf = (struct old_timex32 __user *)args[0];

	/* adjtimex() is allowed only for read. */
	if (copy_from_user(&kbuf, buf, sizeof(struct old_timex32)))
		return -EFAULT;
	if (kbuf.modes != 0)
		return -EPERM;
	return ksys_adjtimex_time32(buf);
}

static asmlinkage long
android_compat_clock_adjtime(struct pt_regs *regs)
{
	struct task_struct *task = current;
	struct old_timex32 kbuf;
	unsigned long args[6];
	clockid_t which_clock;
	struct old_timex32 __user *buf;

	syscall_get_arguments(task, regs, args);
	which_clock = args[0];
	buf = (struct old_timex32 __user *)args[1];

	/* clock_adjtime() is allowed only for read. */
	if (copy_from_user(&kbuf, buf, sizeof(struct old_timex32)))
		return -EFAULT;
	if (kbuf.modes != 0)
		return -EPERM;
	return ksys_clock_adjtime32(which_clock, buf);
}
#endif /* CONFIG_COMPAT */

static struct syscall_whitelist whitelists[] = {
	SYSCALL_WHITELIST(read_write_test),
	SYSCALL_WHITELIST(android),
	PERMISSIVE_SYSCALL_WHITELIST(android),
	SYSCALL_WHITELIST(third_party),
	PERMISSIVE_SYSCALL_WHITELIST(third_party),
	SYSCALL_WHITELIST(complete),
	PERMISSIVE_SYSCALL_WHITELIST(complete)
};

static int alt_syscall_apply_whitelist(const struct syscall_whitelist *wl,
				       struct alt_sys_call_table *t)
{
	unsigned int i;
	unsigned long *whitelist = kcalloc(BITS_TO_LONGS(t->size),
					   sizeof(unsigned long), GFP_KERNEL);

	if (!whitelist)
		return -ENOMEM;

	for (i = 0; i < wl->nr_whitelist; i++) {
		unsigned int nr = wl->whitelist[i].nr;

		if (nr >= t->size) {
			kfree(whitelist);
			return -EINVAL;
		}
		bitmap_set(whitelist, nr, 1);
		if (wl->whitelist[i].alt)
			t->table[nr] = wl->whitelist[i].alt;
	}

	for (i = 0; i < t->size; i++) {
		if (!test_bit(i, whitelist)) {
			t->table[i] = wl->permissive ?
				(sys_call_ptr_t)warn_syscall :
				(sys_call_ptr_t)block_syscall;
		}
	}

	kfree(whitelist);
	return 0;
}

#ifdef CONFIG_COMPAT
static int
alt_syscall_apply_compat_whitelist(const struct syscall_whitelist *wl,
				   struct alt_sys_call_table *t)
{
	unsigned int i;
	unsigned long *whitelist = kcalloc(BITS_TO_LONGS(t->compat_size),
					   sizeof(unsigned long), GFP_KERNEL);

	if (!whitelist)
		return -ENOMEM;

	for (i = 0; i < wl->nr_compat_whitelist; i++) {
		unsigned int nr = wl->compat_whitelist[i].nr;

		if (nr >= t->compat_size) {
			kfree(whitelist);
			return -EINVAL;
		}
		bitmap_set(whitelist, nr, 1);
		if (wl->compat_whitelist[i].alt)
			t->compat_table[nr] = wl->compat_whitelist[i].alt;
	}

	for (i = 0; i < t->compat_size; i++) {
		if (!test_bit(i, whitelist)) {
			t->compat_table[i] = wl->permissive ?
				(sys_call_ptr_t)warn_compat_syscall :
				(sys_call_ptr_t)block_syscall;
		}
	}

	kfree(whitelist);
	return 0;
}
#else
static inline int
alt_syscall_apply_compat_whitelist(const struct syscall_whitelist *wl,
				   struct alt_sys_call_table *t)
{
	return 0;
}
#endif /* CONFIG_COMPAT */

static int alt_syscall_init_one(const struct syscall_whitelist *wl)
{
	struct alt_sys_call_table *t;
	int err;

	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return -ENOMEM;
	strncpy(t->name, wl->name, sizeof(t->name));

	err = arch_dup_sys_call_table(t);
	if (err)
		return err;

	err = alt_syscall_apply_whitelist(wl, t);
	if (err)
		return err;
	err = alt_syscall_apply_compat_whitelist(wl, t);
	if (err)
		return err;

	return register_alt_sys_call_table(t);
}

/*
 * Register an alternate syscall table for each whitelist.  Note that the
 * lack of a module_exit() is intentional - once a syscall table is registered
 * it cannot be unregistered.
 *
 * TODO(abrestic) Support unregistering syscall tables?
 */
static int chromiumos_alt_syscall_init(void)
{
	unsigned int i;
	int err;

#ifdef CONFIG_SYSCTL
	if (!register_sysctl_paths(chromiumos_sysctl_path,
				   chromiumos_sysctl_table))
		pr_warn("Failed to register sysctl\n");
#endif

	err = arch_dup_sys_call_table(&default_table);
	if (err)
		return err;

	for (i = 0; i < ARRAY_SIZE(whitelists); i++) {
		err = alt_syscall_init_one(&whitelists[i]);
		if (err)
			pr_warn("Failed to register syscall table %s: %d\n",
				whitelists[i].name, err);
	}

	return 0;
}
module_init(chromiumos_alt_syscall_init);
