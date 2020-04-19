// SPDX-License-Identifier: GPL-2.0
/*
 * Container Security Monitor module
 *
 * Copyright (c) 2018 Google, Inc
 */

#include "monitor.h"

#include <linux/audit.h>
#include <linux/lsm_hooks.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sysctl.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/vm_sockets.h>
#include <linux/file.h>

/* protects csm_*_enabled and configurations. */
DECLARE_RWSEM(csm_rwsem_config);

/* protects csm_host_port and csm_vsocket. */
DECLARE_RWSEM(csm_rwsem_vsocket);

/* queue used for poll wait on config changes. */
static DECLARE_WAIT_QUEUE_HEAD(config_wait);

/* increase each time a new configuration is applied. */
static unsigned long config_version;

/*
 * Is monitoring enabled? Defaults to disabled.
 * These variables might be used without locking csm_rwsem_config to check if an
 * LSM hook can bail quickly. The semaphore is taken later to ensure CSM is
 * still enabled.
 *
 * csm_enabled is true if any collector is enabled.
 */
bool csm_enabled;
static bool csm_container_enabled;
bool csm_execute_enabled;

/* securityfs control files */
static struct dentry *csm_dir;
static struct dentry *csm_enabled_file;
static struct dentry *csm_container_file;
static struct dentry *csm_config_file;
static struct dentry *csm_config_vers_file;
static struct dentry *csm_pipe_file;

/* Pipes to forward data to user-mode. */
static struct file *csm_user_read_pipe;
struct file *csm_user_write_pipe;

/* Option to disable the CSM features at boot. */
static bool cmdline_boot_disabled;
bool cmdline_boot_vsock_disabled;

/* Options disabled by default. */
static bool cmdline_boot_pipe_enabled;
static bool cmdline_boot_config_enabled;

static int csm_boot_disabled_setup(char *str)
{
	return kstrtobool(str, &cmdline_boot_disabled);
}
early_param("csm.disabled", csm_boot_disabled_setup);

static int csm_boot_vsock_disabled_setup(char *str)
{
	return kstrtobool(str, &cmdline_boot_vsock_disabled);
}
early_param("csm.vsock.disabled", csm_boot_vsock_disabled_setup);


static int csm_boot_pipe_enabled_setup(char *str)
{
	return kstrtobool(str, &cmdline_boot_pipe_enabled);
}
early_param("csm.pipe.enabled", csm_boot_pipe_enabled_setup);

static int csm_boot_config_enabled_setup(char *str)
{
	return kstrtobool(str, &cmdline_boot_config_enabled);
}
early_param("csm.config.enabled", csm_boot_config_enabled_setup);

static void csm_update_config(schema_ConfigurationRequest *req)
{
	schema_ExecuteCollectorConfig *econf;

	/* Expect the lock to be held for write before this call. */
	lockdep_assert_held_exclusive(&csm_rwsem_config);

	csm_container_enabled = req->container_config.enabled;
	csm_execute_enabled = req->execute_config.enabled;

	/* csm_enabled is true if any collector is enabled. */
	csm_enabled = csm_container_enabled || csm_execute_enabled;

	/* Clean-up existing configurations. */
	kfree(csm_execute_config.envp_allowlist);
	memset(&csm_execute_config, 0, sizeof(csm_execute_config));

	if (csm_execute_enabled) {
		econf = &req->execute_config;
		csm_execute_config.argv_limit = econf->argv_limit;
		csm_execute_config.envp_limit = econf->envp_limit;

		/* Swap the allowlist so it is not freed on return. */
		csm_execute_config.envp_allowlist = econf->envp_allowlist.arg;
		econf->envp_allowlist.arg = NULL;
	}

	config_version++;
	wake_up(&config_wait);
}

int csm_update_config_from_buffer(void *data, size_t size)
{
	schema_ConfigurationRequest c = schema_ConfigurationRequest_init_zero;
	pb_istream_t istream;

	c.execute_config.envp_allowlist.funcs.decode = pb_decode_string_array;

	istream = pb_istream_from_buffer(data, size);
	if (!pb_decode(&istream, schema_ConfigurationRequest_fields, &c))
		return -EINVAL;

	down_write(&csm_rwsem_config);
	csm_update_config(&c);
	up_write(&csm_rwsem_config);

	return 0;
}

static ssize_t csm_config_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	ssize_t err = 0;
	void *mem;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	/* No partial writes. */
	if (*ppos != 0)
		return -EINVAL;

	/* Duplicate user memory to safely parse protobuf. */
	mem = memdup_user(buf, count);
	if (IS_ERR(mem))
		return PTR_ERR(mem);

	err = csm_update_config_from_buffer(mem, count);
	if (!err)
		err = count;

	kfree(mem);
	return err;
}

static const struct file_operations csm_config_fops = {
	.write = csm_config_write,
};

static void csm_enable(void)
{
	schema_ConfigurationRequest req = schema_ConfigurationRequest_init_zero;

	/* Expect the lock to be held for write before this call. */
	lockdep_assert_held_exclusive(&csm_rwsem_config);

	/* Default configuration */
	req.container_config.enabled = true;
	req.execute_config.enabled = true;
	req.execute_config.argv_limit = UINT_MAX;
	req.execute_config.envp_limit = UINT_MAX;
	csm_update_config(&req);
}

static void csm_disable(void)
{
	schema_ConfigurationRequest req = schema_ConfigurationRequest_init_zero;

	/* Expect the lock to be held for write before this call. */
	lockdep_assert_held_exclusive(&csm_rwsem_config);

	/* Zero configuration disable all collectors. */
	csm_update_config(&req);
	pr_info("disabled\n");
}

static ssize_t csm_enabled_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	const char *str = csm_enabled ? "1\n" : "0\n";

	return simple_read_from_buffer(buf, count, ppos, str, 2);
}

static ssize_t csm_enabled_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	bool enabled;
	int err;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (count <= 0 || count > PAGE_SIZE || *ppos)
		return -EINVAL;

	err = kstrtobool_from_user(buf, count, &enabled);
	if (err)
		return err;

	down_write(&csm_rwsem_config);

	if (enabled)
		csm_enable();
	else
		csm_disable();

	up_write(&csm_rwsem_config);

	return count;
}

static const struct file_operations csm_enabled_fops = {
	.read = csm_enabled_read,
	.write = csm_enabled_write,
};

static int csm_config_version_open(struct inode *inode, struct file *file)
{
	/* private_data is used to keep the latest config version read. */
	file->private_data = (void*)-1;
	return 0;
}

static ssize_t csm_config_version_read(struct file *file, char __user *buf,
				       size_t count, loff_t *ppos)
{
	unsigned long version = config_version;
	file->private_data = (void*)version;
	return simple_read_from_buffer(buf, count, ppos, &version,
				       sizeof(version));
}

static __poll_t csm_config_version_poll(struct file *file,
					struct poll_table_struct *poll_tab)
{
	if ((unsigned long)file->private_data != config_version)
		return EPOLLIN;
	poll_wait(file, &config_wait, poll_tab);
	if ((unsigned long)file->private_data != config_version)
		return EPOLLIN;
	return 0;
}

static const struct file_operations csm_config_version_fops = {
	.open = csm_config_version_open,
	.read = csm_config_version_read,
	.poll = csm_config_version_poll,
};

static int csm_pipe_open(struct inode *inode, struct file *file)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (!csm_enabled)
		return -EAGAIN;
	return 0;
}

static ssize_t csm_pipe_read(struct file *file, char __user *buf,
				       size_t count, loff_t *ppos)
{
	int fd;
	ssize_t err;

	/* No partial reads. */
	if (*ppos != 0)
		return -EINVAL;

	fd = get_unused_fd_flags(0);
	if (fd < 0)
		return fd;

	fd_install(fd, get_file(csm_user_read_pipe));

	err = simple_read_from_buffer(buf, count, ppos, &fd, sizeof(fd));
	if (err < 0)
		goto error;

	if (err < sizeof(fd)) {
		err = -EINVAL;
		goto error;
	}

	return err;

error:
	put_unused_fd(fd);
	return err;
}


static const struct file_operations csm_pipe_fops = {
	.open = csm_pipe_open,
	.read = csm_pipe_read,
};

static void set_container_decode_callbacks(schema_Container *container)
{
	container->pod_namespace.funcs.decode = pb_decode_string_field;
	container->pod_name.funcs.decode = pb_decode_string_field;
	container->container_name.funcs.decode = pb_decode_string_field;
	container->container_image_uri.funcs.decode = pb_decode_string_field;
	container->labels.funcs.decode = pb_decode_string_array;
}

static void set_container_encode_callbacks(schema_Container *container)
{
	container->pod_namespace.funcs.encode = pb_encode_string_field;
	container->pod_name.funcs.encode = pb_encode_string_field;
	container->container_name.funcs.encode = pb_encode_string_field;
	container->container_image_uri.funcs.encode = pb_encode_string_field;
	container->labels.funcs.encode = pb_encode_string_array;
}

static void free_container_callbacks_args(schema_Container *container)
{
	kfree(container->pod_namespace.arg);
	kfree(container->pod_name.arg);
	kfree(container->container_name.arg);
	kfree(container->container_image_uri.arg);
	kfree(container->labels.arg);
}

static ssize_t csm_container_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	ssize_t err = 0;
	void *mem;
	u64 cid;
	pb_istream_t istream;
	struct task_struct *task;
	schema_ContainerReport report = schema_ContainerReport_init_zero;
	schema_Event event = schema_Event_init_zero;
	schema_Container *container;
	char *uuid = NULL;

	/* Notify that this collector is not yet enabled. */
	if (!csm_container_enabled)
		return -EAGAIN;

	/* No partial writes. */
	if (*ppos != 0)
		return -EINVAL;

	/* Duplicate user memory to safely parse protobuf. */
	mem = memdup_user(buf, count);
	if (IS_ERR(mem))
		return PTR_ERR(mem);

	/* Callback to decode string in protobuf. */
	set_container_decode_callbacks(&report.container);

	istream = pb_istream_from_buffer(mem, count);
	if (!pb_decode(&istream, schema_ContainerReport_fields, &report)) {
		err = -EINVAL;
		goto out;
	}

	/* Check protobuf is as expected */
	if (report.pid == 0 ||
	    report.container.container_id != 0) {
		err = -EINVAL;
		goto out;
	}

	/* Find if the process id is linked to an existing container-id. */
	rcu_read_lock();
	task = find_task_by_pid_ns(report.pid, &init_pid_ns);
	if (task) {
		cid = audit_get_contid(task);
		if (cid == AUDIT_CID_UNSET)
			err = -ENOENT;
	} else {
		err = -ENOENT;
	}
	rcu_read_unlock();

	if (err)
		goto out;

	uuid = kzalloc(PROCESS_UUID_SIZE, GFP_KERNEL);
	if (!uuid)
		goto out;

	/* Provide the uuid for the top process of the container. */
	err = get_process_uuid_by_pid(report.pid, uuid, PROCESS_UUID_SIZE);
	if (err)
		goto out;

	/* Correct the container-id and feed the event to vsock */
	report.container.container_id = cid;
	report.container.init_uuid.funcs.encode = pb_encode_uuid_field;
	report.container.init_uuid.arg = uuid;
	container = &event.event.container.container;
	*container = report.container;

	/* Use encode callback to generate the final proto. */
	set_container_encode_callbacks(container);

	event.which_event = schema_Event_container_tag;

	err = csm_sendeventproto(schema_Event_fields, &event);
	if (!err)
		err = count;

out:
	/* Free any allocated nanopb callback arguments. */
	free_container_callbacks_args(&report.container);
	kfree(uuid);
	kfree(mem);
	return err;
}

static const struct file_operations csm_container_fops = {
	.write = csm_container_write,
};

/* Prevent user-mode from using vsock on our port. */
static int csm_socket_connect(struct socket *sock, struct sockaddr *address,
			      int addrlen)
{
	struct sockaddr_vm *vaddr = (struct sockaddr_vm *)address;

	/* Filter only vsock sockets */
	if (!sock->sk || sock->sk->sk_family != AF_VSOCK)
		return 0;

	/* Allow kernel sockets. */
	if (sock->sk->sk_kern_sock)
		return 0;

	if (addrlen < sizeof(*vaddr))
		return -EINVAL;

	/* Forbid access to the CSM VMM backend port. */
	if (vaddr->svm_port == CSM_HOST_PORT)
		return -EPERM;

	return 0;
}

static struct security_hook_list csm_hooks[] __lsm_ro_after_init = {
	LSM_HOOK_INIT(bprm_check_security, csm_bprm_check_security),
	LSM_HOOK_INIT(task_exit, csm_task_exit),
	LSM_HOOK_INIT(socket_connect, csm_socket_connect),
};

static int __init csm_init(void)
{
	int err;
	struct file *pipes[2] = {NULL, NULL};

	if (cmdline_boot_disabled)
		return 0;

	/*
	 * If cmdline_boot_vsock_disabled is true, only the event pool will be
	 * allocated. The destroy function will clean-up only what was reserved.
	 */
	err = vsock_initialize();
	if (err)
		return err;

	if (cmdline_boot_pipe_enabled) {
		err = create_pipe_files(pipes, O_NONBLOCK);
		if (err)
			goto error;

		csm_user_read_pipe = pipes[0];
		csm_user_write_pipe = pipes[1];
	}

	csm_dir = securityfs_create_dir("container_monitor", NULL);
	if (IS_ERR(csm_dir)) {
		err = PTR_ERR(csm_dir);
		goto error_pipe;
	}

	csm_enabled_file = securityfs_create_file("enabled", 0644, csm_dir,
						  NULL, &csm_enabled_fops);
	if (IS_ERR(csm_enabled_file)) {
		err = PTR_ERR(csm_enabled_file);
		goto error_rmdir;
	}

	csm_container_file = securityfs_create_file("container", 0200, csm_dir,
						  NULL, &csm_container_fops);
	if (IS_ERR(csm_container_file)) {
		err = PTR_ERR(csm_container_file);
		goto error_rm_enabled;
	}

	csm_config_vers_file = securityfs_create_file("config_version", 0400,
						      csm_dir, NULL,
						      &csm_config_version_fops);
	if (IS_ERR(csm_config_vers_file)) {
		err = PTR_ERR(csm_config_vers_file);
		goto error_rm_container;
	}

	if (cmdline_boot_config_enabled) {
		csm_config_file = securityfs_create_file("config", 0200,
							 csm_dir, NULL,
							 &csm_config_fops);
		if (IS_ERR(csm_config_file)) {
			err = PTR_ERR(csm_config_file);
			goto error_rm_config_vers;
		}
	}

	if (csm_user_write_pipe) {
		csm_pipe_file = securityfs_create_file("pipe", 0400, csm_dir,
						       NULL, &csm_pipe_fops);
		if (IS_ERR(csm_pipe_file)) {
			err = PTR_ERR(csm_pipe_file);
			goto error_rm_config;
		}
	}

	pr_debug("created securityfs control files\n");

	security_add_hooks(csm_hooks, ARRAY_SIZE(csm_hooks), "csm");
	pr_debug("registered hooks\n");
	return 0;

error_rm_config:
	if (cmdline_boot_config_enabled)
		securityfs_remove(csm_config_file);
error_rm_config_vers:
	securityfs_remove(csm_config_vers_file);
error_rm_container:
	securityfs_remove(csm_container_file);
error_rm_enabled:
	securityfs_remove(csm_enabled_file);
error_rmdir:
	securityfs_remove(csm_dir);
error_pipe:
	if (cmdline_boot_pipe_enabled) {
		fput(pipes[0]);
		fput(pipes[1]);
	}
error:
	vsock_destroy();
	pr_warn("fs initialization error: %d", err);
	return err;
}

late_initcall(csm_init);
