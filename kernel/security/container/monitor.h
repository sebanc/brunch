/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Container Security Monitor module
 *
 * Copyright (c) 2018 Google, Inc
 */

#define pr_fmt(fmt)	"container-security-monitor: " fmt

#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/fs.h>
#include <linux/rwsem.h>
#include <linux/binfmts.h>
#include <config.pb.h>
#include <event.pb.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "monitoring_protocol.h"

/* Part of the CSM configuration response. */
#define CSM_VERSION 1

/* protects csm_*_enabled and configurations. */
extern struct rw_semaphore csm_rwsem_config;

/* protects csm_host_port and csm_vsocket. */
extern struct rw_semaphore csm_rwsem_vsocket;

/* Port to connect to the host on, over virtio-vsock */
#define CSM_HOST_PORT 4444

/*
 * Is monitoring enabled? Defaults to disabled.
 * These variables might be used as gates without locking (as processor ensures
 * valid proper access for native scalar values) so it can bail quickly.
 */
extern bool csm_enabled;
extern bool csm_execute_enabled;

/* Configuration options for execute collector. */
struct execute_config {
	size_t argv_limit;
	size_t envp_limit;
	char *envp_allowlist;
};

extern struct execute_config csm_execute_config;

/* pipe to forward vsock packets to user-mode. */
extern struct file *csm_user_write_pipe;

/* Was vsock disabled at boot time? */
extern bool cmdline_boot_vsock_disabled;

/* Streams file numbers are unknown from the kernel */
#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

/* monitor functions */
int csm_update_config_from_buffer(void *data, size_t size);

/* vsock functions */
int vsock_initialize(void);
void vsock_destroy(void);
int vsock_late_initialize(void);
int csm_sendeventproto(const pb_field_t fields[], const void *src_struct);
int csm_sendconfigrespproto(const pb_field_t fields[], const void *src_struct);

/* process events functions */
int csm_bprm_check_security(struct linux_binprm *bprm);
void csm_task_exit(struct task_struct *task);
int get_process_uuid_by_pid(pid_t pid_nr, char *buffer, size_t size);

/* nano functions */
bool pb_encode_string_field(pb_ostream_t *stream, const pb_field_t *field,
			    void * const *arg);
bool pb_decode_string_field(pb_istream_t *stream, const pb_field_t *field,
		      void **arg);
ssize_t pb_encode_string_field_limit(pb_ostream_t *stream,
				     const pb_field_t *field,
				     void * const *arg, size_t limit);
bool pb_encode_string_array(pb_ostream_t *stream, const pb_field_t *field,
			    void * const *arg);
bool pb_decode_string_array(pb_istream_t *stream, const pb_field_t *field,
			    void **arg);
bool pb_encode_uuid_field(pb_ostream_t *stream, const pb_field_t *field,
			  void * const *arg);
