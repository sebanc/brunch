// SPDX-License-Identifier: GPL-2.0
/*
 * Container Security Monitor module
 *
 * Copyright (c) 2018 Google, Inc
 */

#include "monitor.h"

#include <linux/mount.h>
#include <linux/pid_namespace.h>
#include <linux/timekeeping.h>
#include <linux/xattr.h>
#include <linux/random.h>
#include <overlayfs/overlayfs.h>
#include <uapi/linux/magic.h>
#include <linux/mempool.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/audit.h>
#include <linux/net.h>
#include <linux/file.h>
#include <linux/vmalloc.h>

/* Configuration options for execute collector. */
struct execute_config csm_execute_config;

/* unique atomic value for the machine boot instance */
static atomic_t machine_rand = ATOMIC_INIT(0);

/* sequential container identifier */
static atomic_t contid = ATOMIC_INIT(0);

static void *kmap_argument_stack(struct linux_binprm *bprm, void **ctx)
{
	char *argv;
	int err;
	unsigned long i, pos, count;
	void *map;
	struct page *page;

	/* vma_pages() returns the number of pages reserved for the stack */
	count = vma_pages(bprm->vma);

	if (likely(count == 1)) {
		err = get_user_pages_remote(current, bprm->mm, bprm->p, 1,
					    FOLL_FORCE, &page, NULL, NULL);
		if (err != 1)
			return NULL;

		argv = kmap(page);
		*ctx = page;
	} else {
		/*
		 * If more than one pages is needed, copy all of them to a set
		 * of pages. Parsing the argument across kmap pages in different
		 * addresses would make it impractical.
		 */
		argv = vmalloc(count * PAGE_SIZE);
		if (!argv)
			return NULL;

		for (i = 0; i < count; i++) {
			pos = ALIGN_DOWN(bprm->p, PAGE_SIZE) + i * PAGE_SIZE;
			err = get_user_pages_remote(current, bprm->mm, pos, 1,
						    FOLL_FORCE, &page, NULL,
						    NULL);
			if (err <= 0) {
				vfree(argv);
				return NULL;
			}

			map = kmap(page);
			memcpy(argv + i * PAGE_SIZE, map, PAGE_SIZE);
			kunmap(page);
			put_page(page);
		}
		*ctx = bprm;
	}

	return argv;
}

static void kunmap_argument_stack(struct linux_binprm *bprm, void *addr,
				  void *ctx)
{
	struct page *page;

	if (!addr)
		return;

	if (likely(vma_pages(bprm->vma) == 1)) {
		page = (struct page *)ctx;
		kunmap(page);
		put_page(ctx);
	} else {
		vfree(addr);
	}
}

static char *find_array_next_entry(char *array, unsigned long *offset,
				   unsigned long end)
{
	char *entry;
	unsigned long off = *offset;

	if (off >= end)
		return NULL;

	/* Check the entry is null terminated and in bound */
	entry = array + off;
	while (array[off]) {
		if (++off >= end)
			return NULL;
	}

	/* Pass the null byte for the next iteration */
	*offset = off + 1;

	return entry;
}

struct string_arr_ctx {
	struct linux_binprm *bprm;
	void *stack;
};

static size_t get_config_limit(size_t *config_ptr)
{
	lockdep_assert_held_read(&csm_rwsem_config);

	/*
	 * If execute is not enabled, do not capture arguments.
	 * The vsock packet won't be sent anyway.
	 */
	if (!csm_execute_enabled)
		return 0;

	return *config_ptr;
}

static bool encode_current_argv(pb_ostream_t *stream, const pb_field_t *field,
				void * const *arg)
{
	struct string_arr_ctx *ctx = (struct string_arr_ctx *)*arg;
	int i;
	struct linux_binprm *bprm = ctx->bprm;
	unsigned long offset = bprm->p % PAGE_SIZE;
	unsigned long end = vma_pages(bprm->vma) * PAGE_SIZE;
	char *argv = ctx->stack;
	char *entry;
	size_t limit, used = 0;
	ssize_t ret;

	limit = get_config_limit(&csm_execute_config.argv_limit);
	if (!limit)
		return true;

	for (i = 0; i < bprm->argc; i++) {
		entry = find_array_next_entry(argv, &offset, end);
		if (!entry)
			return false;

		ret = pb_encode_string_field_limit(stream, field,
						   (void * const *)&entry,
						   limit - used);
		if (ret < 0)
			return false;

		used += ret;

		if (used >= limit)
			break;
	}

	return true;
}

static bool check_envp_allowlist(char *envp)
{
	bool ret = false;
	char *strs, *equal;
	size_t str_size, equal_pos;

	/* If execute is not enabled, skip all. */
	if (!csm_execute_enabled)
		goto out;

	/* No filter, allow all. */
	strs = csm_execute_config.envp_allowlist;
	if (!strs) {
		ret = true;
		goto out;
	}

	/*
	 * Identify the key=value separation.
	 * If none exists use the whole string as a key.
	 */
	equal = strchr(envp, '=');
	equal_pos = equal ? (equal - envp) : strlen(envp);

	/* Default to skip if no match found. */
	ret = false;

	do {
		str_size = strlen(strs);

		/*
		 * If the filter length align with the key value equal sign,
		 * it might be a match, check the key value.
		 */
		if (str_size == equal_pos &&
		    !strncmp(strs, envp, str_size)) {
			ret = true;
			goto out;
		}

		strs += str_size + 1;
	} while (*strs != 0);

out:
	return ret;
}

static bool encode_current_envp(pb_ostream_t *stream, const pb_field_t *field,
				void * const *arg)
{
	struct string_arr_ctx *ctx = (struct string_arr_ctx *)*arg;
	int i;
	struct linux_binprm *bprm = ctx->bprm;
	unsigned long offset = bprm->p % PAGE_SIZE;
	unsigned long end = vma_pages(bprm->vma) * PAGE_SIZE;
	char *argv = ctx->stack;
	char *entry;
	size_t limit, used = 0;
	ssize_t ret;

	limit = get_config_limit(&csm_execute_config.envp_limit);
	if (!limit)
		return true;

	/* Skip arguments */
	for (i = 0; i < bprm->argc; i++) {
		if (!find_array_next_entry(argv, &offset, end))
			return false;
	}

	for (i = 0; i < bprm->envc; i++) {
		entry = find_array_next_entry(argv, &offset, end);
		if (!entry)
			return false;

		if (!check_envp_allowlist(entry))
			continue;

		ret = pb_encode_string_field_limit(stream, field,
						   (void * const *)&entry,
						   limit - used);
		if (ret < 0)
			return false;

		used += ret;

		if (used >= limit)
			break;
	}

	return true;
}

static bool is_overlayfs_mounted(struct file *file)
{
	struct vfsmount *mnt;
	struct super_block *mnt_sb;

	mnt = file->f_path.mnt;
	if (mnt == NULL)
		return false;

	mnt_sb = mnt->mnt_sb;
	if (mnt_sb == NULL || mnt_sb->s_magic != OVERLAYFS_SUPER_MAGIC)
		return false;

	return true;
}

/*
 * Before the process starts, identify a possible container by checking if the
 * task is on a pid namespace and the target file is using an overlayfs mounting
 * point. This check is valid for COS and GKE but not all existing containers.
 */
static bool is_possible_container(struct task_struct *task,
				  struct file *file)
{
	if (task_active_pid_ns(task) == &init_pid_ns)
		return false;

	return is_overlayfs_mounted(file);
}

/*
 * Generates a random identifier for this boot instance.
 * This identifier is generated only when needed to increase the entropy
 * available compared to doing it at early boot.
 */
static u32 get_machine_id(void)
{
	int machineid, old;

	machineid = atomic_read(&machine_rand);

	if (unlikely(machineid == 0)) {
		machineid = (int)get_random_int();
		if (machineid == 0)
			machineid = 1;
		old = atomic_cmpxchg(&machine_rand, 0, machineid);

		/* If someone beat us, use their value. */
		if (old != 0)
			machineid = old;
	}

	return (u32)machineid;
}

/*
 * Generate a 128-bit unique identifier for the process by appending:
 *  - A machine identifier unique per boot.
 *  - The start time of the process in nanoseconds.
 *  - The tgid for the set of threads in a process.
 */
static int get_process_uuid(struct task_struct *task, char *buffer, size_t size)
{
	union process_uuid *id = (union process_uuid *)buffer;

	memset(buffer, 0, size);

	if (WARN_ON(size < PROCESS_UUID_SIZE))
		return -EINVAL;

	id->machineid = get_machine_id();
	id->start_time = ktime_mono_to_real(task->start_time);
	id->tgid = task_tgid_nr(task);

	return 0;
}

int get_process_uuid_by_pid(pid_t pid_nr, char *buffer, size_t size)
{
	int err;
	struct task_struct *task = NULL;

	rcu_read_lock();
	task = find_task_by_pid_ns(pid_nr, &init_pid_ns);
	if (!task) {
		err = -ENOENT;
		goto out;
	}
	err = get_process_uuid(task, buffer, size);
out:
	rcu_read_unlock();
	return err;
}

u64 csm_set_contid(struct task_struct *task)
{
	u64 cid;

	/* TODO: Report error to the backend */
	if (WARN_ON(!task->audit))
		return AUDIT_CID_UNSET;

	cid = atomic_inc_return(&contid);
	task->audit->contid = cid;
	return cid;
}

static u32 get_fd_inode_mode(int fd)
{
	u32 mode = 0;
	struct fd sfd;

	sfd = fdget(fd);
	if (sfd.file)
		mode = file_inode(sfd.file)->i_mode;
	fdput(sfd);

	return mode;
}

int csm_bprm_check_security(struct linux_binprm *bprm)
{
	char *buf;
	struct dentry *dentry;
	char *path;
	char uuid[PROCESS_UUID_SIZE];
	char parent_uuid[PROCESS_UUID_SIZE];
	int err;
	schema_Event event = schema_Event_init_zero;
	schema_Process *proc;
	schema_Overlay *overlayfs;
	struct string_arr_ctx argv_ctx;
	void *stack = NULL, *ctx = NULL;
	u64 cid;

	/*
	 * Always create a container-id for containerized processes.
	 * If the LSM is enabled later, we can track existing containers.
	 */
	cid = audit_get_contid(current);

	if (cid == AUDIT_CID_UNSET) {
		if (!is_possible_container(current, bprm->file))
			return 0;

		cid = csm_set_contid(current);

		if (cid == AUDIT_CID_UNSET)
			return 0;
	}

	if (!csm_execute_enabled)
		return 0;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	path = d_path(&bprm->file->f_path, buf, PAGE_SIZE);
	if (IS_ERR(path)) {
		err = PTR_ERR(path);
		goto out_free_buf;
	}

	proc = &event.event.execute.proc;
	proc->creation_timestamp = ktime_get_real_ns();
	proc->container_id = cid;

	/* Add information about pid in different namespaces */
	proc->pid = task_pid_nr(current);
	proc->parent_pid = task_ppid_nr(current);
	proc->container_pid = task_pid_nr_ns(current, NULL);
	proc->container_parent_pid = task_ppid_nr_ns(current, NULL);

	/* Generate unique identifier for the process and its parent */
	err = get_process_uuid(current, uuid, sizeof(uuid));
	if (err)
		goto out_free_buf;

	proc->uuid.funcs.encode = pb_encode_uuid_field;
	proc->uuid.arg = uuid;

	if (proc->parent_pid) {
		err = get_process_uuid_by_pid(proc->parent_pid, parent_uuid,
					      sizeof(parent_uuid));
		/* Report the parent process if available. */
		if (!err) {
			proc->parent_uuid.funcs.encode = pb_encode_uuid_field;
			proc->parent_uuid.arg = parent_uuid;
		} else if (err != -ENOENT) {
			goto out_free_buf;
		}
	}

	/* Provide information about the launched binary */
	proc->binary.fullpath.funcs.encode = pb_encode_string_field;
	proc->binary.fullpath.arg = path;

	if (is_overlayfs_mounted(bprm->file)) {
		dentry = bprm->file->f_path.dentry;
		overlayfs = &proc->binary.filesystem.overlayfs;
		overlayfs->lower_layer = ovl_dentry_lower(dentry);
		overlayfs->upper_layer = ovl_dentry_upper(dentry);
	}
	proc->binary.which_filesystem = schema_File_overlayfs_tag;

	stack = kmap_argument_stack(bprm, &ctx);
	if (!stack) {
		err = -EFAULT;
		goto out_free_buf;
	}

	/* Capture process argument */
	argv_ctx.bprm = bprm;
	argv_ctx.stack = stack;
	proc->args.argv.funcs.encode = encode_current_argv;
	proc->args.argv.arg = &argv_ctx;

	/* Capture process environment variables */
	proc->args.envp.funcs.encode = encode_current_envp;
	proc->args.envp.arg = &argv_ctx;

	/* Information about streams */
	proc->streams.stdin.mode = get_fd_inode_mode(STDIN_FILENO);
	proc->streams.stdout.mode = get_fd_inode_mode(STDOUT_FILENO);
	proc->streams.stderr.mode = get_fd_inode_mode(STDERR_FILENO);

	event.which_event = schema_Event_execute_tag;

	/*
	 * Configurations options are checked when computing the serialized
	 * protobufs.
	 */
	down_read(&csm_rwsem_config);
	err = csm_sendeventproto(schema_Event_fields, &event);
	up_read(&csm_rwsem_config);

	if (err)
		pr_err("csm_sendeventproto returned %d on execve\n", err);
	err = 0;

out_free_buf:
	kunmap_argument_stack(bprm, stack, ctx);
	free_page((unsigned long)buf);

	/*
	 * On failure, enforce it only if the execute config is enabled.
	 * If the collector was disabled, prefer to succeed to not impact the
	 * system.
	 */
	if (err < 0 && !csm_execute_enabled)
		err = 0;

	return err;
}

void csm_task_exit(struct task_struct *task)
{
	int err;
	schema_Event event = schema_Event_init_zero;
	schema_ExitEvent *exit;
	char uuid[PROCESS_UUID_SIZE];

	/* Catch the last thread of a process with a container identifier. */
	if (!csm_execute_enabled ||
	    get_nr_threads(task) > 1 ||
	    audit_get_contid(task) == AUDIT_CID_UNSET)
		return;

	exit = &event.event.exit;

	/* Fetch the unique identifier for this process */
	err = get_process_uuid(task, uuid, sizeof(uuid));
	if (err) {
		pr_err("failed to get process uuid on exit\n");
		return;
	}

	exit->process_uuid.funcs.encode = pb_encode_uuid_field;
	exit->process_uuid.arg = uuid;

	event.which_event = schema_Event_exit_tag;

	err = csm_sendeventproto(schema_Event_fields, &event);
	if (err)
		pr_err("csm_sendeventproto returned %d on exit\n", err);
}
