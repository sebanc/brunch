// SPDX-License-Identifier: GPL-2.0
/*
 * Container Security Monitor module
 *
 * Copyright (c) 2018 Google, Inc
 */

#include "monitor.h"

#include <net/net_namespace.h>
#include <net/vsock_addr.h>
#include <net/sock.h>
#include <linux/mempool.h>
#include <linux/socket.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/delay.h>

/*
 * virtio vsocket over which to send events to the host.
 * NULL if monitoring is disabled, or if the socket was disconnected and we're
 * trying to reconnect to the host.
 */
static struct socket *csm_vsocket;

/* reconnect delay */
#define CSM_RECONNECT_FREQ_MSEC 5000

/* config pull delay */
#define CSM_CONFIG_FREQ_MSEC 1000

/* vsock receive attempts and delay until giving up */
#define CSM_RECV_ATTEMPTS 2
#define CSM_RECV_DELAY_MSEC 100

/* heartbeat work */
#define CSM_HEARTBEAT_FREQ msecs_to_jiffies(5000)
static void csm_heartbeat(struct work_struct *work);
static DECLARE_DELAYED_WORK(csm_heartbeat_work, csm_heartbeat);

/* size used for the config error message. */
#define CSM_ERROR_BUF_SIZE 40

/* Size of each event entry in the mempool */
static size_t event_pool_size = PAGE_SIZE;

/* Memory pool for fast memory access on event creation */
static mempool_t *event_pool;

/* Running thread to manage vsock connections. */
static struct task_struct *socket_thread;

/* Mutex to ensure sequential dumping of protos */
static DEFINE_MUTEX(protodump);

static struct socket *csm_create_socket(void)
{
	int err;
	struct sockaddr_vm host_addr;
	struct socket *sock;

	err = sock_create_kern(&init_net, AF_VSOCK, SOCK_STREAM, 0,
			       &sock);
	if (err) {
		pr_debug("error creating AF_VSOCK socket: %d\n", err);
		return ERR_PTR(err);
	}

	vsock_addr_init(&host_addr, VMADDR_CID_HYPERVISOR, CSM_HOST_PORT);

	err = kernel_connect(sock, (struct sockaddr *)&host_addr,
			     sizeof(host_addr), 0);
	if (err) {
		if (err != -ECONNRESET) {
			pr_debug("error connecting AF_VSOCK socket to host port %u: %d\n",
				CSM_HOST_PORT, err);
		}
		goto error_release;
	}

	return sock;

error_release:
	sock_release(sock);
	return ERR_PTR(err);
}

static void csm_destroy_socket(void)
{
	down_write(&csm_rwsem_vsocket);
	if (csm_vsocket) {
		sock_release(csm_vsocket);
		csm_vsocket = NULL;
	}
	up_write(&csm_rwsem_vsocket);
}

static int csm_vsock_sendmsg(struct kvec *vecs, size_t vecs_size,
			     size_t total_length)
{
	struct msghdr msg = { };
	int res = -EPIPE;

	if (cmdline_boot_vsock_disabled)
		return 0;

	down_read(&csm_rwsem_vsocket);
	if (csm_vsocket) {
		res = kernel_sendmsg(csm_vsocket, &msg, vecs, vecs_size,
				     total_length);
		if (res > 0)
			res = 0;
	}
	up_read(&csm_rwsem_vsocket);

	return res;
}

static ssize_t csm_user_pipe_write(struct kvec *vecs, size_t vecs_size,
				   size_t total_length)
{
	ssize_t perr;
	struct iov_iter io = { };
	loff_t pos = 0;

	if (!csm_user_write_pipe)
		return 0;

	iov_iter_kvec(&io, ITER_KVEC|WRITE, vecs, vecs_size,
		      total_length);

	file_start_write(csm_user_write_pipe);
	perr = vfs_iter_write(csm_user_write_pipe, &io, &pos, 0);
	file_end_write(csm_user_write_pipe);

	return perr;
}

static int csm_sendmsg(int type, const void *buf, size_t len)
{
	struct csm_msg_hdr hdr = {
		.msg_type = cpu_to_le32(type),
		.msg_length = cpu_to_le32(sizeof(hdr) + len),
	};
	struct kvec vecs[] = {
		{
			.iov_base = &hdr,
			.iov_len = sizeof(hdr),
		}, {
			.iov_base = (void *)buf,
			.iov_len = len,
		}
	};
	int res;
	ssize_t perr;

	res = csm_vsock_sendmsg(vecs, ARRAY_SIZE(vecs),
				le32_to_cpu(hdr.msg_length));
	if (res < 0) {
		pr_warn_ratelimited("sendmsg error (msg_type=%d, msg_length=%u): %d\n",
				    type, le32_to_cpu(hdr.msg_length), res);
	}

	perr = csm_user_pipe_write(vecs, ARRAY_SIZE(vecs),
				   le32_to_cpu(hdr.msg_length));
	if (perr < 0) {
		pr_warn_ratelimited("vfs_iter_write error (msg_type=%d, msg_length=%u): %zd\n",
				    type, le32_to_cpu(hdr.msg_length), perr);
	}

	return res;
}

/*
 * Allocate an event using the best available allocator.
 * The context argument is set for tracking the pool used.
 */
static void *event_alloc(size_t size, mempool_t **context)
{
	if (!context)
		return NULL;

	*context = NULL;

	if (size > event_pool_size)
		return kmalloc(size, GFP_KERNEL);

	*context = event_pool;
	return mempool_alloc(event_pool, GFP_KERNEL);
}

static void event_free(void *ptr, mempool_t *context)
{
	if (context)
		mempool_free(ptr, context);
	else
		kfree(ptr);
}

static int csm_sendproto(int type, const pb_field_t fields[],
			 const void *src_struct)
{
	int err = 0;
	char *msg;
	mempool_t *ctx;
	pb_ostream_t pos;
	size_t size;

	if (!pb_get_encoded_size(&size, fields, src_struct))
		return -EINVAL;

	msg = event_alloc(size, &ctx);
	if (!msg)
		return -ENOMEM;

	pos = pb_ostream_from_buffer(msg, size);
	if (!pb_encode(&pos, fields, src_struct)) {
		err = -EINVAL;
		goto out_free;
	}

	/*
	 * Nanopb doesn't support dumping protobuf to string. Instead, hexdump
	 * the serialized buffer so test code can unmarshal it.
	 *
	 * A mutex is used to ensure protobufs dumps are not racing each others.
	 * The testing code will grep the prefix and parse the result so racing
	 * other logs is okay but racing other protobuf dumps will create
	 * problems for testing output.
	 */
	if (IS_ENABLED(CONFIG_SECURITY_CONTAINER_MONITOR_DEBUG)) {
		mutex_lock(&protodump);
		print_hex_dump_debug("crst-protobuf ", DUMP_PREFIX_OFFSET, 16,
				     1, msg, pos.bytes_written, true);
		mutex_unlock(&protodump);
	}

	err = csm_sendmsg(type, msg, pos.bytes_written);
out_free:
	event_free(msg, ctx);
	return err;
}

int csm_sendeventproto(const pb_field_t fields[], const void *src_struct)
{
	/* Last check before generating and sending an event. */
	if (!csm_enabled)
		return -ENOTSUPP;

	return csm_sendproto(CSM_MSG_EVENT_PROTO, fields, src_struct);
}

int csm_sendconfigrespproto(const pb_field_t fields[], const void *src_struct)
{
	return csm_sendproto(CSM_MSG_CONFIG_RESPONSE_PROTO, fields, src_struct);
}

static void csm_heartbeat(struct work_struct *work)
{
	csm_sendmsg(CSM_MSG_TYPE_HEARTBEAT, NULL, 0);
	schedule_delayed_work(&csm_heartbeat_work, CSM_HEARTBEAT_FREQ);
}

static int config_send_response(int err)
{
	char buf[CSM_ERROR_BUF_SIZE] = {};
	schema_ConfigurationResponse resp =
		schema_ConfigurationResponse_init_zero;

	resp.error = schema_ConfigurationResponse_ErrorCode_NO_ERROR;
	resp.version = CSM_VERSION;
	resp.kernel_version = LINUX_VERSION_CODE;

	if (err) {
		resp.error = schema_ConfigurationResponse_ErrorCode_UNKNOWN;
		snprintf(buf, sizeof(buf) - 1, "error code: %d", err);
		resp.msg.funcs.encode = pb_encode_string_field;
		resp.msg.arg = buf;
	}

	return csm_sendconfigrespproto(schema_ConfigurationResponse_fields,
				       &resp);
}

static int csm_recvmsg(void *buf, size_t len, bool expected)
{
	int err = 0;
	struct msghdr msg = {};
	struct kvec vecs;
	size_t pos = 0;
	size_t attempts = 0;

	while (pos < len) {
		vecs.iov_base = (char *)buf + pos;
		vecs.iov_len = len - pos;

		down_read(&csm_rwsem_vsocket);
		if (csm_vsocket) {
			err = kernel_recvmsg(csm_vsocket, &msg, &vecs, 1, len,
					     MSG_DONTWAIT);
		} else {
			pr_err("csm_vsocket was unset while the config thread was running\n");
			err = -ENOENT;
		}
		up_read(&csm_rwsem_vsocket);

		if (err == 0) {
			err = -ENOTCONN;
			pr_warn_ratelimited("vsock connection was reset\n");
			break;
		}

		if (err == -EAGAIN) {
			/*
			 * If nothing is received and nothing was expected
			 * just bail.
			 */
			if (!expected && pos == 0) {
				err = -EAGAIN;
				break;
			}

			/*
			 * If we missing data after multiple attempts
			 * reset the connection.
			 */
			if (++attempts >= CSM_RECV_ATTEMPTS) {
				err = -EPIPE;
				break;
			}

			msleep(CSM_RECV_DELAY_MSEC);
			continue;
		}

		if (err < 0) {
			pr_err_ratelimited("kernel_recvmsg failed with %d\n",
					   err);
			break;
		}

		pos += err;
	}

	return err;
}

/*
 * Listen for configuration until connection is closed or desynchronize.
 * If something wrong happens while parsing the packet buffer that may
 * desynchronize the thread with the backend, the connection is reset.
 */

static void listen_configuration(void *buf)
{
	int err;
	struct csm_msg_hdr hdr = {};
	uint32_t msg_type, msg_length;

	pr_debug("listening for configuration messages\n");

	while (true) {
		err = csm_recvmsg(&hdr, sizeof(hdr), false);

		/* Nothing available, wait and try again. */
		if (err == -EAGAIN) {
			msleep(CSM_CONFIG_FREQ_MSEC);
			continue;
		}

		if (err < 0)
			break;

		msg_type = le32_to_cpu(hdr.msg_type);

		if (msg_type != CSM_MSG_CONFIG_REQUEST_PROTO) {
			pr_warn_ratelimited("unexpected message type: %d\n",
					    msg_type);
			break;
		}

		msg_length = le32_to_cpu(hdr.msg_length);

		if (msg_length <= sizeof(hdr) || msg_length > PAGE_SIZE) {
			pr_warn_ratelimited("unexpected message length: %d\n",
					    msg_length);
			break;
		}

		/* The message length include the size of the header. */
		msg_length -= sizeof(hdr);

		err = csm_recvmsg(buf, msg_length, true);
		if (err < 0) {
			pr_warn_ratelimited("failed to gather configuration: %d\n",
					    err);
			break;
		}

		err = csm_update_config_from_buffer(buf, msg_length);
		if (err < 0) {
			/*
			 * Warn of the error but continue listening for
			 * configuration changes.
			 */
			pr_warn_ratelimited("config update failed: %d\n", err);
		} else {
			pr_debug("config received and applied\n");
		}

		err = config_send_response(err);
		if (err < 0) {
			pr_err_ratelimited("config response failed: %d\n", err);
			break;
		}

		pr_debug("config response sent\n");
	}
}

/* Thread managing connection and listening for new configurations. */
static int socket_thread_fn(void *unsued)
{
	void *buf;
	struct socket *sock;

	/* One page should be enough for current configurations. */
	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	while (true) {
		sock = csm_create_socket();
		if (IS_ERR(sock)) {
			pr_debug("unable to connect to host (port %u), will retry in %u ms\n",
				 CSM_HOST_PORT, CSM_RECONNECT_FREQ_MSEC);
			msleep(CSM_RECONNECT_FREQ_MSEC);
			continue;
		}

		down_write(&csm_rwsem_vsocket);
		csm_vsocket = sock;
		up_write(&csm_rwsem_vsocket);

		schedule_delayed_work(&csm_heartbeat_work, 0);

		listen_configuration(buf);

		pr_warn("vsock state incorrect, disconnecting. Messages will be lost.\n");

		cancel_delayed_work_sync(&csm_heartbeat_work);
		csm_destroy_socket();
	}

	return 0;
}

void __init vsock_destroy(void)
{
	if (socket_thread) {
		kthread_stop(socket_thread);
		socket_thread = NULL;
	}

	mempool_destroy(event_pool);
	event_pool = NULL;
}

int __init vsock_initialize(void)
{
	struct task_struct *task;

	/*
	 * Eventually should increase the mempool entities and reduce the
	 * expected size for all events. The current settings should work while
	 * the protobufs are still being defined.
	 */
	event_pool = mempool_create_kmalloc_pool(NR_CPUS, event_pool_size);
	if (!event_pool) {
		pr_err("failed to allocate event memory pool\n");
		return -ENOMEM;
	}

	if (!cmdline_boot_vsock_disabled) {
		task = kthread_run(socket_thread_fn, NULL, "csm-vsock-thread");
		if (IS_ERR(task)) {
			pr_err("failed to create socket thread: %ld\n", PTR_ERR(task));
			vsock_destroy();
			return PTR_ERR(task);
		}

		socket_thread = task;
	}
	return 0;
}
