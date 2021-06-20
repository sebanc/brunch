// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#include "ip_link.h"

#include <net/tcp.h>

#include "utils.h"

static void __mausb_ip_set_options(struct socket *sock, bool udp)
{
	u32 optval = 0;
	unsigned int optlen = sizeof(optval);
	int status = 0;
	struct timeval timeo = {.tv_sec = 0, .tv_usec = 500000U };
	struct timeval send_timeo = {.tv_sec = 1, .tv_usec = 0 };

	if (!udp) {
		optval = 1;
		status = kernel_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
					   (char *)&optval, optlen);
		if (status < 0)
			dev_warn(mausb_host_dev.this_device, "Failed to set tcp no delay option: status=%d",
				 status);
	}

	status = kernel_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
				   (char *)&timeo, sizeof(timeo));
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "Failed to set recv timeout option: status=%d",
			 status);

	status = kernel_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
				   (char *)&send_timeo, sizeof(send_timeo));
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "Failed to set snd timeout option: status=%d",
			 status);

	optval = MAUSB_LINK_BUFF_SIZE;
	status  = kernel_setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
				    (char *)&optval, optlen);
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "Failed to set recv buffer size: status=%d",
			 status);

	optval = MAUSB_LINK_BUFF_SIZE;
	status  = kernel_setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
				    (char *)&optval, optlen);
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "Failed to set send buffer size: status=%d",
			 status);

	optval = MAUSB_LINK_TOS_LEVEL_EF;
	status  = kernel_setsockopt(sock, IPPROTO_IP, IP_TOS,
				    (char *)&optval, optlen);
	if (status < 0)
		dev_warn(mausb_host_dev.this_device, "Failed to set QOS: status=%d",
			 status);
}

static void __mausb_ip_connect(struct work_struct *work)
{
	int status = 0;
	struct sockaddr *sa;
	int sa_size;
	struct mausb_ip_ctx *ip_ctx = container_of(work, struct mausb_ip_ctx,
						   connect_work);
	unsigned short int family = ip_ctx->dev_addr_in.sa_in.sin_family;

	if (!ip_ctx->udp) {
		status = sock_create_kern(ip_ctx->net_ns, family, SOCK_STREAM,
					  IPPROTO_TCP, &ip_ctx->client_socket);
		if (status < 0) {
			dev_err(mausb_host_dev.this_device, "Failed to create socket: status=%d",
				status);
			goto callback;
		}
	} else {
		status = sock_create_kern(ip_ctx->net_ns, family, SOCK_DGRAM,
					  IPPROTO_UDP, &ip_ctx->client_socket);
		if (status < 0) {
			dev_err(mausb_host_dev.this_device, "Failed to create socket: status=%d",
				status);
			goto callback;
		}
	}

	__mausb_ip_set_options((struct socket *)ip_ctx->client_socket,
			       ip_ctx->udp);

	if (family == AF_INET) {
		sa = (struct sockaddr *)&ip_ctx->dev_addr_in.sa_in;
		sa_size = sizeof(ip_ctx->dev_addr_in.sa_in);
		dev_info(mausb_host_dev.this_device, "Connecting to %pI4:%d, status=%d",
			 &ip_ctx->dev_addr_in.sa_in.sin_addr,
			 htons(ip_ctx->dev_addr_in.sa_in.sin_port), status);
#if IS_ENABLED(CONFIG_IPV6)
	} else if (family == AF_INET6) {
		sa = (struct sockaddr *)&ip_ctx->dev_addr_in.sa_in6;
		sa_size = sizeof(ip_ctx->dev_addr_in.sa_in6);
		dev_info(mausb_host_dev.this_device, "Connecting to %pI6c:%d, status=%d",
			 &ip_ctx->dev_addr_in.sa_in6.sin6_addr,
			 htons(ip_ctx->dev_addr_in.sa_in6.sin6_port), status);
#endif
	} else {
		dev_err(mausb_host_dev.this_device, "Wrong network family provided");
		status = -EINVAL;
		goto callback;
	}

	status = kernel_connect(ip_ctx->client_socket, sa, sa_size, O_RDWR);
	if (status < 0) {
		dev_err(mausb_host_dev.this_device, "Failed to connect to host, status=%d",
			status);
		goto clear_socket;
	}

	queue_work(ip_ctx->recv_workq, &ip_ctx->recv_work);

	goto callback;

clear_socket:
	sock_release(ip_ctx->client_socket);
	ip_ctx->client_socket = NULL;
callback:
	ip_ctx->fn_callback(ip_ctx->ctx, ip_ctx->channel, MAUSB_LINK_CONNECT,
			    status, NULL);
}

static inline void __mausb_ip_recv_ctx_clear(struct mausb_ip_recv_ctx *recv_ctx)
{
	recv_ctx->buffer   = NULL;
	recv_ctx->left	   = 0;
	recv_ctx->received = 0;
}

static inline void __mausb_ip_recv_ctx_free(struct mausb_ip_recv_ctx *recv_ctx)
{
	kfree(recv_ctx->buffer);
	__mausb_ip_recv_ctx_clear(recv_ctx);
}

static int __mausb_ip_recv(struct mausb_ip_ctx *ip_ctx)
{
	struct msghdr msghd;
	struct kvec vec;
	int  status;
	bool peek = true;
	unsigned int optval = 1;
	struct socket *client_socket = (struct socket *)ip_ctx->client_socket;

	/* receive with timeout of 0.5s */
	while (true) {
		memset(&msghd, 0, sizeof(msghd));
		if (peek) {
			vec.iov_base = ip_ctx->recv_ctx.common_hdr;
			vec.iov_len  = sizeof(ip_ctx->recv_ctx.common_hdr);
			msghd.msg_flags = MSG_PEEK;
		} else {
			vec.iov_base =
			    ip_ctx->recv_ctx.buffer +
			    ip_ctx->recv_ctx.received;
			vec.iov_len = ip_ctx->recv_ctx.left;
			msghd.msg_flags = MSG_WAITALL;
		}

		if (!ip_ctx->udp) {
			status = kernel_setsockopt(client_socket, IPPROTO_TCP,
						   TCP_QUICKACK,
						   (char *)&optval,
						   sizeof(optval));
			if (status != 0) {
				dev_warn(mausb_host_dev.this_device, "Setting TCP_QUICKACK failed, status=%d",
					 status);
			}
		}

		status = kernel_recvmsg(client_socket, &msghd, &vec, 1,
					vec.iov_len, (int)msghd.msg_flags);
		if (status == -EAGAIN) {
			return -EAGAIN;
		} else if (status <= 0) {
			dev_warn(mausb_host_dev.this_device, "kernel_recvmsg, status=%d",
				 status);

			__mausb_ip_recv_ctx_free(&ip_ctx->recv_ctx);
			ip_ctx->fn_callback(ip_ctx->ctx, ip_ctx->channel,
					    MAUSB_LINK_RECV, status, NULL);
			return status;
		}

		dev_vdbg(mausb_host_dev.this_device, "kernel_recvmsg, status=%d",
			 status);

		if (peek) {
			if ((unsigned int)status <
					sizeof(ip_ctx->recv_ctx.common_hdr))
				return -EAGAIN;
			/* length field of mausb_common_hdr */
			ip_ctx->recv_ctx.left =
			    *(u16 *)(&ip_ctx->recv_ctx.common_hdr[2]);
			ip_ctx->recv_ctx.received = 0;
			ip_ctx->recv_ctx.buffer	  =
			    kzalloc(ip_ctx->recv_ctx.left, GFP_KERNEL);
			peek = false;
			if (!ip_ctx->recv_ctx.buffer) {
				ip_ctx->fn_callback(ip_ctx->ctx,
						    ip_ctx->channel,
						    MAUSB_LINK_RECV,
						    -ENOMEM, NULL);
				return -ENOMEM;
			}
		} else {
			if (status < ip_ctx->recv_ctx.left) {
				ip_ctx->recv_ctx.left -= (u16)status;
				ip_ctx->recv_ctx.received += (u16)status;
			} else {
				ip_ctx->fn_callback(ip_ctx->ctx,
						    ip_ctx->channel,
						    MAUSB_LINK_RECV, status,
						    ip_ctx->recv_ctx.buffer);
				__mausb_ip_recv_ctx_clear(&ip_ctx->recv_ctx);
				peek = true;
			}
		}
	}

	return status;
}

static void __mausb_ip_recv_work(struct work_struct *work)
{
	struct mausb_ip_ctx *ip_ctx = container_of(work, struct mausb_ip_ctx,
						   recv_work);
	int status = __mausb_ip_recv(ip_ctx);

	if (status <= 0 && status != -EAGAIN)
		return;

	queue_work(ip_ctx->recv_workq, &ip_ctx->recv_work);
}

int mausb_init_ip_ctx(struct mausb_ip_ctx **ip_ctx,
		      struct net *net_ns,
		      char ip_addr[INET6_ADDRSTRLEN],
		      u16 port, void *context,
		      void (*fn_callback)(void *ctx, enum mausb_channel channel,
					  enum mausb_link_action act,
					  int status, void *data),
		      enum mausb_channel channel)
{
	struct mausb_ip_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_ATOMIC);
	if (!ctx)
		return -ENOMEM;

	__mausb_ip_recv_ctx_clear(&ctx->recv_ctx);

	if (in4_pton(ip_addr, -1,
		     (u8 *)&ctx->dev_addr_in.sa_in.sin_addr.s_addr, -1,
		     NULL) == 1) {
		ctx->dev_addr_in.sa_in.sin_family = AF_INET;
		ctx->dev_addr_in.sa_in.sin_port = htons(port);
#if IS_ENABLED(CONFIG_IPV6)
	} else if (in6_pton(ip_addr, -1,
			    (u8 *)&ctx->dev_addr_in.sa_in6.sin6_addr.in6_u, -1,
			    NULL) == 1) {
		ctx->dev_addr_in.sa_in6.sin6_family = AF_INET6;
		ctx->dev_addr_in.sa_in6.sin6_port = htons(port);
#endif
	} else {
		dev_err(mausb_host_dev.this_device, "Invalid IP address received: address=%s",
			ip_addr);
		kfree(ctx);
		return -EINVAL;
	}

	ctx->net_ns = net_ns;

	ctx->udp = channel == MAUSB_ISOCH_CHANNEL;

	ctx->connect_workq = alloc_ordered_workqueue("connect_workq",
						     WQ_MEM_RECLAIM);
	if (!ctx->connect_workq) {
		kfree(ctx);
		return -ENOMEM;
	}

	ctx->recv_workq = alloc_ordered_workqueue("recv_workq", WQ_MEM_RECLAIM);
	if (!ctx->recv_workq) {
		destroy_workqueue(ctx->connect_workq);
		kfree(ctx);
		return -ENOMEM;
	}

	INIT_WORK(&ctx->connect_work, __mausb_ip_connect);
	INIT_WORK(&ctx->recv_work, __mausb_ip_recv_work);

	ctx->channel	 = channel;
	ctx->ctx	 = context;
	ctx->fn_callback = fn_callback;
	*ip_ctx = ctx;

	return 0;
}

void mausb_destroy_ip_ctx(struct mausb_ip_ctx *ip_ctx)
{
	if (!ip_ctx)
		return;

	if (ip_ctx->connect_workq) {
		flush_workqueue(ip_ctx->connect_workq);
		destroy_workqueue(ip_ctx->connect_workq);
	}

	if (ip_ctx->recv_workq) {
		flush_workqueue(ip_ctx->recv_workq);
		destroy_workqueue(ip_ctx->recv_workq);
	}
	if (ip_ctx->client_socket)
		sock_release(ip_ctx->client_socket);
	__mausb_ip_recv_ctx_free(&ip_ctx->recv_ctx);

	kfree(ip_ctx);
}

void mausb_ip_connect_async(struct mausb_ip_ctx *ip_ctx)
{
	queue_work(ip_ctx->connect_workq, &ip_ctx->connect_work);
}

int mausb_ip_disconnect(struct mausb_ip_ctx *ip_ctx)
{
	if (ip_ctx && ip_ctx->client_socket)
		return kernel_sock_shutdown(ip_ctx->client_socket, SHUT_RDWR);
	return 0;
}

int mausb_ip_send(struct mausb_ip_ctx *ip_ctx,
		  struct mausb_kvec_data_wrapper *wrapper)
{
	struct msghdr msghd;

	if (!ip_ctx) {
		dev_alert(mausb_host_dev.this_device, "Socket ctx is NULL!");
		return -EINVAL;
	}

	memset(&msghd, 0, sizeof(msghd));
	msghd.msg_flags = MSG_WAITALL;

	return kernel_sendmsg(ip_ctx->client_socket, &msghd, wrapper->kvec,
			      wrapper->kvec_num, wrapper->length);
}
