/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

/* Container security monitoring protocol definitions */

#include <linux/types.h>

enum csm_msgtype {
	CSM_MSG_TYPE_HEARTBEAT = 1,
	CSM_MSG_EVENT_PROTO = 2,
	CSM_MSG_CONFIG_REQUEST_PROTO = 3,
	CSM_MSG_CONFIG_RESPONSE_PROTO = 4,
};

struct csm_msg_hdr {
	__le32 msg_type;
	__le32 msg_length;
};

/* The process uuid is a 128-bits identifier */
#define PROCESS_UUID_SIZE 16

/* The entire structure forms the collision domain. */
union process_uuid {
	struct {
		__u32 machineid;
		__u64 start_time;
		__u32 tgid;
	} __attribute__((packed));
	__u8 data[PROCESS_UUID_SIZE];
};
