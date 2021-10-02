#ifndef _LINUX_VIRTIO_WL_H
#define _LINUX_VIRTIO_WL_H
/*
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 */
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/virtwl.h>

#define VIRTWL_IN_BUFFER_SIZE 4096
#define VIRTWL_OUT_BUFFER_SIZE 4096
#define VIRTWL_VQ_IN 0
#define VIRTWL_VQ_OUT 1
#define VIRTWL_QUEUE_COUNT 2
#define VIRTWL_MAX_ALLOC 0x800
#define VIRTWL_PFN_SHIFT 12

/* Enables the transition to new flag semantics */
#define VIRTIO_WL_F_TRANS_FLAGS 1

struct virtio_wl_config {
};

/*
 * The structure of each of these is virtio_wl_ctrl_hdr or one of its subclasses
 * where noted.
 */
enum virtio_wl_ctrl_type {
	VIRTIO_WL_CMD_VFD_NEW = 0x100, /* virtio_wl_ctrl_vfd_new */
	VIRTIO_WL_CMD_VFD_CLOSE, /* virtio_wl_ctrl_vfd */
	VIRTIO_WL_CMD_VFD_SEND, /* virtio_wl_ctrl_vfd_send + data */
	VIRTIO_WL_CMD_VFD_RECV, /* virtio_wl_ctrl_vfd_recv + data */
	VIRTIO_WL_CMD_VFD_NEW_CTX, /* virtio_wl_ctrl_vfd_new */
	VIRTIO_WL_CMD_VFD_NEW_PIPE, /* virtio_wl_ctrl_vfd_new */
	VIRTIO_WL_CMD_VFD_HUP, /* virtio_wl_ctrl_vfd */
	VIRTIO_WL_CMD_VFD_NEW_DMABUF, /* virtio_wl_ctrl_vfd_new */
	VIRTIO_WL_CMD_VFD_DMABUF_SYNC, /* virtio_wl_ctrl_vfd_dmabuf_sync */
	VIRTIO_WL_CMD_VFD_SEND_FOREIGN_ID, /* virtio_wl_ctrl_vfd_send + data */
	VIRTIO_WL_CMD_VFD_NEW_CTX_NAMED, /* virtio_wl_ctrl_vfd_new */

	VIRTIO_WL_RESP_OK = 0x1000,
	VIRTIO_WL_RESP_VFD_NEW = 0x1001, /* virtio_wl_ctrl_vfd_new */
	VIRTIO_WL_RESP_VFD_NEW_DMABUF = 0x1002, /* virtio_wl_ctrl_vfd_new */

	VIRTIO_WL_RESP_ERR = 0x1100,
	VIRTIO_WL_RESP_OUT_OF_MEMORY,
	VIRTIO_WL_RESP_INVALID_ID,
	VIRTIO_WL_RESP_INVALID_TYPE,
	VIRTIO_WL_RESP_INVALID_FLAGS,
	VIRTIO_WL_RESP_INVALID_CMD,
};

struct virtio_wl_ctrl_hdr {
	__le32 type; /* one of virtio_wl_ctrl_type */
	__le32 flags; /* always 0 */
};

enum virtio_wl_vfd_flags {
	VIRTIO_WL_VFD_WRITE = 0x1, /* intended to be written by guest */
	VIRTIO_WL_VFD_READ = 0x2, /* intended to be read by guest */
};

struct virtio_wl_ctrl_vfd {
	struct virtio_wl_ctrl_hdr hdr;
	__le32 vfd_id;
};

/*
 * If this command is sent to the guest, it indicates that the VFD has been
 * created and the fields indicate the properties of the VFD being offered.
 *
 * If this command is sent to the host, it represents a request to create a VFD
 * of the given properties. The pfn field is ignored by the host.
 */
struct virtio_wl_ctrl_vfd_new {
	struct virtio_wl_ctrl_hdr hdr;
	__le32 vfd_id; /* MSB indicates device allocated vfd */
	__le32 flags; /* virtio_wl_vfd_flags */
	__le64 pfn; /* first guest physical page frame number if VFD_MAP */
	__le32 size; /* size in bytes if VIRTIO_WL_CMD_VFD_NEW* */
	union {
		/* buffer description if VIRTIO_WL_CMD_VFD_NEW_DMABUF */
		struct {
			__le32 width; /* width in pixels */
			__le32 height; /* height in pixels */
			__le32 format; /* fourcc format */
			__le32 stride0; /* return stride0 */
			__le32 stride1; /* return stride1 */
			__le32 stride2; /* return stride2 */
			__le32 offset0; /* return offset0 */
			__le32 offset1; /* return offset1 */
			__le32 offset2; /* return offset2 */
		} dmabuf;
		/* name of socket if VIRTIO_WL_CMD_VFD_NEW_CTX_NAMED */
		char name[32];
	};
};


enum virtio_wl_ctrl_vfd_send_kind {
	/* The id after this one indicates an ordinary vfd_id. */
	VIRTIO_WL_CTRL_VFD_SEND_KIND_LOCAL,
	/* The id after this one is a virtio-gpu resource id. */
	VIRTIO_WL_CTRL_VFD_SEND_KIND_VIRTGPU,
};

struct virtio_wl_ctrl_vfd_send_vfd {
	__le32 kind; /* virtio_wl_ctrl_vfd_send_kind */
	__le32 id;
};

struct virtio_wl_ctrl_vfd_send {
	struct virtio_wl_ctrl_hdr hdr;
	__le32 vfd_id;
	__le32 vfd_count; /* struct is followed by this many IDs */

	/*
	 * If hdr.type == VIRTIO_WL_CMD_VFD_SEND_FOREIGN_ID, there is a
	 * vfd_count array of virtio_wl_ctrl_vfd_send_vfd. Otherwise, there is a
	 * vfd_count array of vfd_ids.
	 */

	/* the remainder is raw data */
};

struct virtio_wl_ctrl_vfd_recv {
	struct virtio_wl_ctrl_hdr hdr;
	__le32 vfd_id;
	__le32 vfd_count; /* struct is followed by this many IDs */
	/* the remainder is raw data */
};

struct virtio_wl_ctrl_vfd_dmabuf_sync {
	struct virtio_wl_ctrl_hdr hdr;
	__le32 vfd_id;
	__le32 flags;
};

#endif /* _LINUX_VIRTIO_WL_H */
