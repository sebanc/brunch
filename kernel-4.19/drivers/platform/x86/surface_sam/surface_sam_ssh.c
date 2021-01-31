/*
 * Surface Serial Hub (SSH) driver for communication with the Surface/System
 * Aggregator Module.
 */

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/crc-ccitt.h>
#include <linux/dmaengine.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/refcount.h>
#include <linux/serdev.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "surface_sam_ssh.h"


#define SSH_RQST_TAG_FULL			"surface_sam_ssh_rqst: "
#define SSH_RQST_TAG				"rqst: "
#define SSH_EVENT_TAG				"event: "
#define SSH_RECV_TAG				"recv: "

#define SSH_SUPPORTED_FLOW_CONTROL_MASK		(~((u8) ACPI_UART_FLOW_CONTROL_HW))

#define SSH_BYTELEN_SYNC			2
#define SSH_BYTELEN_TERM			2
#define SSH_BYTELEN_CRC				2
#define SSH_BYTELEN_CTRL			4	// command-header, ACK, or RETRY
#define SSH_BYTELEN_CMDFRAME			8	// without payload

#define SSH_MAX_WRITE (				\
	  SSH_BYTELEN_SYNC			\
	+ SSH_BYTELEN_CTRL			\
	+ SSH_BYTELEN_CRC			\
	+ SSH_BYTELEN_CMDFRAME			\
	+ SURFACE_SAM_SSH_MAX_RQST_PAYLOAD	\
	+ SSH_BYTELEN_CRC			\
)

#define SSH_MSG_LEN_CTRL (			\
	  SSH_BYTELEN_SYNC			\
	+ SSH_BYTELEN_CTRL			\
	+ SSH_BYTELEN_CRC			\
	+ SSH_BYTELEN_TERM			\
)

#define SSH_MSG_LEN_CMD_BASE (			\
	  SSH_BYTELEN_SYNC			\
	+ SSH_BYTELEN_CTRL			\
	+ SSH_BYTELEN_CRC			\
	+ SSH_BYTELEN_CRC			\
)	// without payload and command-frame

#define SSH_WRITE_TIMEOUT		msecs_to_jiffies(1000)
#define SSH_READ_TIMEOUT		msecs_to_jiffies(1000)
#define SSH_NUM_RETRY			3

#define SSH_WRITE_BUF_LEN		SSH_MAX_WRITE
#define SSH_READ_BUF_LEN		512		// must be power of 2
#define SSH_EVAL_BUF_LEN		SSH_MAX_WRITE	// also works for reading

#define SSH_FRAME_TYPE_CMD_NOACK	0x00	// request/event that does not to be ACKed
#define SSH_FRAME_TYPE_CMD		0x80	// request/event
#define SSH_FRAME_TYPE_ACK		0x40	// ACK for request/event
#define SSH_FRAME_TYPE_RETRY		0x04	// error or retry indicator

#define SSH_FRAME_OFFS_CTRL		SSH_BYTELEN_SYNC
#define SSH_FRAME_OFFS_CTRL_CRC		(SSH_FRAME_OFFS_CTRL + SSH_BYTELEN_CTRL)
#define SSH_FRAME_OFFS_TERM		(SSH_FRAME_OFFS_CTRL_CRC + SSH_BYTELEN_CRC)
#define SSH_FRAME_OFFS_CMD		SSH_FRAME_OFFS_TERM	// either TERM or CMD
#define SSH_FRAME_OFFS_CMD_PLD		(SSH_FRAME_OFFS_CMD + SSH_BYTELEN_CMDFRAME)

/*
 * A note on Request IDs (RQIDs):
 *	0x0000 is not a valid RQID
 *	0x0001 is valid, but reserved for Surface Laptop keyboard events
 */
#define SAM_NUM_EVENT_TYPES		((1 << SURFACE_SAM_SSH_RQID_EVENT_BITS) - 1)

/*
 * Sync:			aa 55
 * Terminate:			ff ff
 *
 * Request Message:		sync cmd-hdr crc(cmd-hdr) cmd-rqst-frame crc(cmd-rqst-frame)
 * Ack Message:			sync ack crc(ack) terminate
 * Retry Message:		sync retry crc(retry) terminate
 * Response Message:		sync cmd-hdr crc(cmd-hdr) cmd-resp-frame crc(cmd-resp-frame)
 *
 * Command Header:		80 LEN 00 SEQ
 * Ack:				40 00 00 SEQ
 * Retry:			04 00 00 00
 * Command Request Frame:	80 RTC 01 00 RIID RQID RCID PLD
 * Command Response Frame:	80 RTC 00 01 RIID RQID RCID PLD
 */

struct ssh_frame_ctrl {
	u8 type;
	u8 len;			// without crc
	u8 pad;
	u8 seq;
} __packed;

struct ssh_frame_cmd {
	u8 type;
	u8 tc;
	u8 pri_out;
	u8 pri_in;
	u8 iid;
	u8 rqid_lo;		// id for request/response matching (low byte)
	u8 rqid_hi;		// id for request/response matching (high byte)
	u8 cid;
} __packed;


enum ssh_ec_state {
	SSH_EC_UNINITIALIZED,
	SSH_EC_INITIALIZED,
	SSH_EC_SUSPENDED,
};

struct ssh_counters {
	u8  seq;		// control sequence id
	u16 rqid;		// id for request/response matching
};

struct ssh_writer {
	u8 *data;
	u8 *ptr;
} __packed;

enum ssh_receiver_state {
	SSH_RCV_DISCARD,
	SSH_RCV_CONTROL,
	SSH_RCV_COMMAND,
};

struct ssh_receiver {
	spinlock_t lock;
	enum ssh_receiver_state state;
	struct completion signal;
	struct kfifo fifo;
	struct {
		bool pld;
		u8 seq;
		u16 rqid;
	} expect;
	struct {
		u16 cap;
		u16 len;
		u8 *ptr;
	} eval_buf;
};

struct ssh_event_handler {
	surface_sam_ssh_event_handler_fn handler;
	surface_sam_ssh_event_handler_delay delay;
	void *data;
};

struct ssh_events {
	spinlock_t lock;
	struct workqueue_struct *queue_ack;
	struct workqueue_struct *queue_evt;
	struct ssh_event_handler handler[SAM_NUM_EVENT_TYPES];
};

struct sam_ssh_ec {
	struct mutex lock;
	enum ssh_ec_state state;
	struct serdev_device *serdev;
	struct ssh_counters counter;
	struct ssh_writer writer;
	struct ssh_receiver receiver;
	struct ssh_events events;
	int irq;
	bool irq_wakeup_enabled;
};

struct ssh_fifo_packet {
	u8 type;	// packet type (ACK/RETRY/CMD)
	u8 seq;
	u8 len;
};

struct ssh_event_work {
	refcount_t refcount;
	struct sam_ssh_ec *ec;
	struct work_struct work_ack;
	struct delayed_work work_evt;
	struct surface_sam_ssh_event event;
	u8 seq;
};


static struct sam_ssh_ec ssh_ec = {
	.lock   = __MUTEX_INITIALIZER(ssh_ec.lock),
	.state  = SSH_EC_UNINITIALIZED,
	.serdev = NULL,
	.counter = {
		.seq  = 0,
		.rqid = 0,
	},
	.writer = {
		.data = NULL,
		.ptr  = NULL,
	},
	.receiver = {
		.lock = __SPIN_LOCK_UNLOCKED(),
		.state = SSH_RCV_DISCARD,
		.expect = {},
	},
	.events = {
		.lock = __SPIN_LOCK_UNLOCKED(),
		.handler = {},
	},
	.irq = -1,
};


inline static struct sam_ssh_ec *surface_sam_ssh_acquire(void)
{
	struct sam_ssh_ec *ec = &ssh_ec;

	mutex_lock(&ec->lock);
	return ec;
}

inline static void surface_sam_ssh_release(struct sam_ssh_ec *ec)
{
	mutex_unlock(&ec->lock);
}

inline static struct sam_ssh_ec *surface_sam_ssh_acquire_init(void)
{
	struct sam_ssh_ec *ec = surface_sam_ssh_acquire();

	if (ec->state == SSH_EC_UNINITIALIZED) {
		surface_sam_ssh_release(ec);
		return NULL;
	}

	return ec;
}

int surface_sam_ssh_consumer_register(struct device *consumer)
{
	u32 flags = DL_FLAG_PM_RUNTIME | DL_FLAG_AUTOREMOVE_CONSUMER;
	struct sam_ssh_ec *ec;
	struct device_link *link;

	ec = surface_sam_ssh_acquire_init();
	if (!ec) {
		return -ENXIO;
	}

	link = device_link_add(consumer, &ec->serdev->dev, flags);
	if (!link) {
		return -EFAULT;
	}

	surface_sam_ssh_release(ec);
	return 0;
}
EXPORT_SYMBOL_GPL(surface_sam_ssh_consumer_register);


inline static u16 sam_rqid_to_rqst(u16 rqid) {
	return rqid << SURFACE_SAM_SSH_RQID_EVENT_BITS;
}

inline static bool sam_rqid_is_event(u16 rqid) {
	const u16 mask = (1 << SURFACE_SAM_SSH_RQID_EVENT_BITS) - 1;
	return rqid != 0 && (rqid | mask) == mask;
}

int surface_sam_ssh_enable_event_source(u8 tc, u8 unknown, u16 rqid)
{
	u8 pld[4] = { tc, unknown, rqid & 0xff, rqid >> 8 };
	u8 buf[1] = { 0x00 };

	struct surface_sam_ssh_rqst rqst = {
		.tc  = 0x01,
		.cid = 0x0b,
		.iid = 0x00,
		.pri = SURFACE_SAM_PRIORITY_NORMAL,
		.snc = 0x01,
		.cdl = 0x04,
		.pld = pld,
	};

	struct surface_sam_ssh_buf result = {
		result.cap = ARRAY_SIZE(buf),
		result.len = 0,
		result.data = buf,
	};

	int status;

	// only allow RQIDs that lie within event spectrum
	if (!sam_rqid_is_event(rqid)) {
		return -EINVAL;
	}

	status = surface_sam_ssh_rqst(&rqst, &result);

	if (buf[0] != 0x00) {
		printk(KERN_WARNING SSH_RQST_TAG_FULL
		       "unexpected result while enabling event source: 0x%02x\n",
		       buf[0]);
	}

	return status;

}
EXPORT_SYMBOL_GPL(surface_sam_ssh_enable_event_source);

int surface_sam_ssh_disable_event_source(u8 tc, u8 unknown, u16 rqid)
{
	u8 pld[4] = { tc, unknown, rqid & 0xff, rqid >> 8 };
	u8 buf[1] = { 0x00 };

	struct surface_sam_ssh_rqst rqst = {
		.tc  = 0x01,
		.cid = 0x0c,
		.iid = 0x00,
		.pri = SURFACE_SAM_PRIORITY_NORMAL,
		.snc = 0x01,
		.cdl = 0x04,
		.pld = pld,
	};

	struct surface_sam_ssh_buf result = {
		result.cap = ARRAY_SIZE(buf),
		result.len = 0,
		result.data = buf,
	};

	int status;

	// only allow RQIDs that lie within event spectrum
	if (!sam_rqid_is_event(rqid)) {
		return -EINVAL;
	}

	status = surface_sam_ssh_rqst(&rqst, &result);

	if (buf[0] != 0x00) {
		printk(KERN_WARNING SSH_RQST_TAG_FULL
		       "unexpected result while disabling event source: 0x%02x\n",
		       buf[0]);
	}

	return status;
}
EXPORT_SYMBOL_GPL(surface_sam_ssh_disable_event_source);

static unsigned long sam_event_default_delay(struct surface_sam_ssh_event *event, void *data)
{
	return event->pri == SURFACE_SAM_PRIORITY_HIGH ? SURFACE_SAM_SSH_EVENT_IMMEDIATE : 0;
}

int surface_sam_ssh_set_delayed_event_handler(
		u16 rqid, surface_sam_ssh_event_handler_fn fn,
		surface_sam_ssh_event_handler_delay delay,
		void *data)
{
	struct sam_ssh_ec *ec;
	unsigned long flags;

	if (!sam_rqid_is_event(rqid)) {
		return -EINVAL;
	}

	ec = surface_sam_ssh_acquire_init();
	if (!ec) {
		return -ENXIO;
	}

	if (!delay) {
		delay = sam_event_default_delay;
	}

	spin_lock_irqsave(&ec->events.lock, flags);
	// check if we already have a handler
	if (ec->events.handler[rqid - 1].handler) {
		spin_unlock_irqrestore(&ec->events.lock, flags);
		return -EINVAL;
	}

	// 0 is not a valid event RQID
	ec->events.handler[rqid - 1].handler = fn;
	ec->events.handler[rqid - 1].delay = delay;
	ec->events.handler[rqid - 1].data = data;

	spin_unlock_irqrestore(&ec->events.lock, flags);
	surface_sam_ssh_release(ec);

	return 0;
}
EXPORT_SYMBOL_GPL(surface_sam_ssh_set_delayed_event_handler);

int surface_sam_ssh_remove_event_handler(u16 rqid)
{
	struct sam_ssh_ec *ec;
	unsigned long flags;

	if (!sam_rqid_is_event(rqid)) {
		return -EINVAL;
	}

	ec = surface_sam_ssh_acquire_init();
	if (!ec) {
		return -ENXIO;
	}

	spin_lock_irqsave(&ec->events.lock, flags);

	// 0 is not a valid event RQID
	ec->events.handler[rqid - 1].handler = NULL;
	ec->events.handler[rqid - 1].delay = NULL;
	ec->events.handler[rqid - 1].data = NULL;

	spin_unlock_irqrestore(&ec->events.lock, flags);
	surface_sam_ssh_release(ec);

	/*
	 * Make sure that the handler is not in use any more after we've
	 * removed it.
	 */
	flush_workqueue(ec->events.queue_evt);

	return 0;
}
EXPORT_SYMBOL_GPL(surface_sam_ssh_remove_event_handler);


inline static u16 ssh_crc(const u8 *buf, size_t size)
{
	return crc_ccitt_false(0xffff, buf, size);
}

inline static void ssh_write_u16(struct ssh_writer *writer, u16 in)
{
	put_unaligned_le16(in, writer->ptr);
	writer->ptr += 2;
}

inline static void ssh_write_crc(struct ssh_writer *writer,
				 const u8 *buf, size_t size)
{
	ssh_write_u16(writer, ssh_crc(buf, size));
}

inline static void ssh_write_syn(struct ssh_writer *writer)
{
	u8 *w = writer->ptr;

	*w++ = 0xaa;
	*w++ = 0x55;

	writer->ptr = w;
}

inline static void ssh_write_ter(struct ssh_writer *writer)
{
	u8 *w = writer->ptr;

	*w++ = 0xff;
	*w++ = 0xff;

	writer->ptr = w;
}

inline static void ssh_write_buf(struct ssh_writer *writer,
				 u8 *in, size_t len)
{
	writer->ptr = memcpy(writer->ptr, in, len) + len;
}

inline static void ssh_write_hdr(struct ssh_writer *writer,
				 const struct surface_sam_ssh_rqst *rqst,
				 struct sam_ssh_ec *ec)
{
	struct ssh_frame_ctrl *hdr = (struct ssh_frame_ctrl *)writer->ptr;
	u8 *begin = writer->ptr;

	hdr->type = SSH_FRAME_TYPE_CMD;
	hdr->len  = SSH_BYTELEN_CMDFRAME + rqst->cdl;	// without CRC
	hdr->pad  = 0x00;
	hdr->seq  = ec->counter.seq;

	writer->ptr += sizeof(*hdr);

	ssh_write_crc(writer, begin, writer->ptr - begin);
}

inline static void ssh_write_cmd(struct ssh_writer *writer,
				 const struct surface_sam_ssh_rqst *rqst,
				 struct sam_ssh_ec *ec)
{
	struct ssh_frame_cmd *cmd = (struct ssh_frame_cmd *)writer->ptr;
	u8 *begin = writer->ptr;

	u16 rqid = sam_rqid_to_rqst(ec->counter.rqid);
	u8 rqid_lo = rqid & 0xFF;
	u8 rqid_hi = rqid >> 8;

	cmd->type     = SSH_FRAME_TYPE_CMD;
	cmd->tc       = rqst->tc;
	cmd->pri_out  = rqst->pri;
	cmd->pri_in   = 0x00;
	cmd->iid      = rqst->iid;
	cmd->rqid_lo  = rqid_lo;
	cmd->rqid_hi  = rqid_hi;
	cmd->cid      = rqst->cid;

	writer->ptr += sizeof(*cmd);

	ssh_write_buf(writer, rqst->pld, rqst->cdl);
	ssh_write_crc(writer, begin, writer->ptr - begin);
}

inline static void ssh_write_ack(struct ssh_writer *writer, u8 seq)
{
	struct ssh_frame_ctrl *ack = (struct ssh_frame_ctrl *)writer->ptr;
	u8 *begin = writer->ptr;

	ack->type = SSH_FRAME_TYPE_ACK;
	ack->len  = 0x00;
	ack->pad  = 0x00;
	ack->seq  = seq;

	writer->ptr += sizeof(*ack);

	ssh_write_crc(writer, begin, writer->ptr - begin);
}

inline static void ssh_writer_reset(struct ssh_writer *writer)
{
	writer->ptr = writer->data;
}

inline static int ssh_writer_flush(struct sam_ssh_ec *ec)
{
	struct ssh_writer *writer = &ec->writer;
	struct serdev_device *serdev = ec->serdev;
	int status;

	size_t len = writer->ptr - writer->data;

	dev_dbg(&ec->serdev->dev, "sending message\n");
	print_hex_dump_debug("send: ", DUMP_PREFIX_OFFSET, 16, 1,
	                     writer->data, writer->ptr - writer->data, false);

	status = serdev_device_write(serdev, writer->data, len, SSH_WRITE_TIMEOUT);
	return status >= 0 ? 0 : status;
}

inline static void ssh_write_msg_cmd(struct sam_ssh_ec *ec,
				     const struct surface_sam_ssh_rqst *rqst)
{
	ssh_writer_reset(&ec->writer);
	ssh_write_syn(&ec->writer);
	ssh_write_hdr(&ec->writer, rqst, ec);
	ssh_write_cmd(&ec->writer, rqst, ec);
}

inline static void ssh_write_msg_ack(struct sam_ssh_ec *ec, u8 seq)
{
	ssh_writer_reset(&ec->writer);
	ssh_write_syn(&ec->writer);
	ssh_write_ack(&ec->writer, seq);
	ssh_write_ter(&ec->writer);
}

inline static void ssh_receiver_restart(struct sam_ssh_ec *ec,
					const struct surface_sam_ssh_rqst *rqst)
{
	unsigned long flags;

	spin_lock_irqsave(&ec->receiver.lock, flags);
	reinit_completion(&ec->receiver.signal);
	ec->receiver.state = SSH_RCV_CONTROL;
	ec->receiver.expect.pld = rqst->snc;
	ec->receiver.expect.seq = ec->counter.seq;
	ec->receiver.expect.rqid = sam_rqid_to_rqst(ec->counter.rqid);
	ec->receiver.eval_buf.len = 0;
	spin_unlock_irqrestore(&ec->receiver.lock, flags);
}

inline static void ssh_receiver_discard(struct sam_ssh_ec *ec)
{
	unsigned long flags;

	spin_lock_irqsave(&ec->receiver.lock, flags);
	ec->receiver.state = SSH_RCV_DISCARD;
	ec->receiver.eval_buf.len = 0;
	kfifo_reset(&ec->receiver.fifo);
	spin_unlock_irqrestore(&ec->receiver.lock, flags);
}

static int surface_sam_ssh_rqst_unlocked(struct sam_ssh_ec *ec,
					 const struct surface_sam_ssh_rqst *rqst,
					 struct surface_sam_ssh_buf *result)
{
	struct device *dev = &ec->serdev->dev;
	struct ssh_fifo_packet packet = {};
	int status;
	int try;
	unsigned int rem;

	if (rqst->cdl > SURFACE_SAM_SSH_MAX_RQST_PAYLOAD) {
		dev_err(dev, SSH_RQST_TAG "request payload too large\n");
		return -EINVAL;
	}

	// write command in buffer, we may need it multiple times
	ssh_write_msg_cmd(ec, rqst);
	ssh_receiver_restart(ec, rqst);

	// send command, try to get an ack response
	for (try = 0; try < SSH_NUM_RETRY; try++) {
		status = ssh_writer_flush(ec);
		if (status) {
			goto out;
		}

		rem = wait_for_completion_timeout(&ec->receiver.signal, SSH_READ_TIMEOUT);
		if (rem) {
			// completion assures valid packet, thus ignore returned length
			(void) !kfifo_out(&ec->receiver.fifo, &packet, sizeof(packet));

			if (packet.type == SSH_FRAME_TYPE_ACK) {
				break;
			}
		}
	}

	// check if we ran out of tries?
	if (try >= SSH_NUM_RETRY) {
		dev_err(dev, SSH_RQST_TAG "communication failed %d times, giving up\n", try);
		status = -EIO;
		goto out;
	}

	ec->counter.seq  += 1;
	ec->counter.rqid += 1;

	// get command response/payload
	if (rqst->snc && result) {
		rem = wait_for_completion_timeout(&ec->receiver.signal, SSH_READ_TIMEOUT);
		if (rem) {
			// completion assures valid packet, thus ignore returned length
			(void) !kfifo_out(&ec->receiver.fifo, &packet, sizeof(packet));

			if (result->cap < packet.len) {
				status = -EINVAL;
				goto out;
			}

			// completion assures valid packet, thus ignore returned length
			(void) !kfifo_out(&ec->receiver.fifo, result->data, packet.len);
			result->len = packet.len;
		} else {
			dev_err(dev, SSH_RQST_TAG "communication timed out\n");
			status = -EIO;
			goto out;
		}

		// send ACK
		if (packet.type == SSH_FRAME_TYPE_CMD) {
			ssh_write_msg_ack(ec, packet.seq);
			status = ssh_writer_flush(ec);
			if (status) {
				goto out;
			}
		}
	}

out:
	ssh_receiver_discard(ec);
	return status;
}

int surface_sam_ssh_rqst(const struct surface_sam_ssh_rqst *rqst, struct surface_sam_ssh_buf *result)
{
	struct sam_ssh_ec *ec;
	int status;

	ec = surface_sam_ssh_acquire_init();
	if (!ec) {
		printk(KERN_WARNING SSH_RQST_TAG_FULL "embedded controller is uninitialized\n");
		return -ENXIO;
	}

	if (ec->state == SSH_EC_SUSPENDED) {
		dev_warn(&ec->serdev->dev, SSH_RQST_TAG "embedded controller is suspended\n");

		surface_sam_ssh_release(ec);
		return -EPERM;
	}

	status = surface_sam_ssh_rqst_unlocked(ec, rqst, result);

	surface_sam_ssh_release(ec);
	return status;
}
EXPORT_SYMBOL_GPL(surface_sam_ssh_rqst);


static int surface_sam_ssh_ec_resume(struct sam_ssh_ec *ec)
{
	u8 buf[1] = { 0x00 };

	struct surface_sam_ssh_rqst rqst = {
		.tc  = 0x01,
		.cid = 0x16,
		.iid = 0x00,
		.pri = SURFACE_SAM_PRIORITY_NORMAL,
		.snc = 0x01,
		.cdl = 0x00,
		.pld = NULL,
	};

	struct surface_sam_ssh_buf result = {
		result.cap = ARRAY_SIZE(buf),
		result.len = 0,
		result.data = buf,
	};

	int status = surface_sam_ssh_rqst_unlocked(ec, &rqst, &result);
	if (status) {
		return status;
	}

	if (buf[0] != 0x00) {
		dev_warn(&ec->serdev->dev,
		         "unexpected result while trying to resume EC: 0x%02x\n",
			 buf[0]);
	}

	return 0;
}

static int surface_sam_ssh_ec_suspend(struct sam_ssh_ec *ec)
{
	u8 buf[1] = { 0x00 };

	struct surface_sam_ssh_rqst rqst = {
		.tc  = 0x01,
		.cid = 0x15,
		.iid = 0x00,
		.pri = SURFACE_SAM_PRIORITY_NORMAL,
		.snc = 0x01,
		.cdl = 0x00,
		.pld = NULL,
	};

	struct surface_sam_ssh_buf result = {
		result.cap = ARRAY_SIZE(buf),
		result.len = 0,
		result.data = buf,
	};

	int status = surface_sam_ssh_rqst_unlocked(ec, &rqst, &result);
	if (status) {
		return status;
	}

	if (buf[0] != 0x00) {
		dev_warn(&ec->serdev->dev,
		         "unexpected result while trying to suspend EC: 0x%02x\n",
			 buf[0]);
	}

	return 0;
}


inline static bool ssh_is_valid_syn(const u8 *ptr)
{
	return ptr[0] == 0xaa && ptr[1] == 0x55;
}

inline static bool ssh_is_valid_ter(const u8 *ptr)
{
	return ptr[0] == 0xff && ptr[1] == 0xff;
}

inline static bool ssh_is_valid_crc(const u8 *begin, const u8 *end)
{
	u16 crc = ssh_crc(begin, end - begin);
	return (end[0] == (crc & 0xff)) && (end[1] == (crc >> 8));
}


static int surface_sam_ssh_send_ack(struct sam_ssh_ec *ec, u8 seq)
{
	int status;
	u8 buf[SSH_MSG_LEN_CTRL];
	u16 crc;

	buf[0] = 0xaa;
	buf[1] = 0x55;
	buf[2] = 0x40;
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = seq;

	crc = ssh_crc(buf + SSH_FRAME_OFFS_CTRL, SSH_BYTELEN_CTRL);
	buf[6] = crc & 0xff;
	buf[7] = crc >> 8;

	buf[8] = 0xff;
	buf[9] = 0xff;

	dev_dbg(&ec->serdev->dev, "sending message\n");
	print_hex_dump_debug("send: ", DUMP_PREFIX_OFFSET, 16, 1,
	                     buf, SSH_MSG_LEN_CTRL, false);

	status = serdev_device_write(ec->serdev, buf, SSH_MSG_LEN_CTRL, SSH_WRITE_TIMEOUT);
	return status >= 0 ? 0 : status;
}

static void surface_sam_ssh_event_work_ack_handler(struct work_struct *_work)
{
	struct surface_sam_ssh_event *event;
	struct ssh_event_work *work;
	struct sam_ssh_ec *ec;
	struct device *dev;
	int status;

	work = container_of(_work, struct ssh_event_work, work_ack);
	event = &work->event;
	ec = work->ec;
	dev = &ec->serdev->dev;

	// make sure we load a fresh ec state
	smp_mb();

	if (ec->state == SSH_EC_INITIALIZED) {
		status = surface_sam_ssh_send_ack(ec, work->seq);
		if (status) {
			dev_err(dev, SSH_EVENT_TAG "failed to send ACK: %d\n", status);
		}
	}

	if (refcount_dec_and_test(&work->refcount)) {
		kfree(work);
	}
}

static void surface_sam_ssh_event_work_evt_handler(struct work_struct *_work)
{
	struct delayed_work *dwork = (struct delayed_work *)_work;
	struct ssh_event_work *work;
	struct surface_sam_ssh_event *event;
	struct sam_ssh_ec *ec;
	struct device *dev;
	unsigned long flags;

	surface_sam_ssh_event_handler_fn handler;
	void *handler_data;

	int status = 0;

	work = container_of(dwork, struct ssh_event_work, work_evt);
	event = &work->event;
	ec = work->ec;
	dev = &ec->serdev->dev;

	spin_lock_irqsave(&ec->events.lock, flags);
	handler       = ec->events.handler[event->rqid - 1].handler;
	handler_data  = ec->events.handler[event->rqid - 1].data;
	spin_unlock_irqrestore(&ec->events.lock, flags);

	/*
	 * During handler removal or driver release, we ensure every event gets
	 * handled before return of that function. Thus a handler obtained here is
	 * guaranteed to be valid at least until this function returns.
	 */

	if (handler) {
		status = handler(event, handler_data);
	} else {
		dev_warn(dev, SSH_EVENT_TAG "unhandled event (rqid: %04x)\n", event->rqid);
	}

	if (status) {
		dev_err(dev, SSH_EVENT_TAG "error handling event: %d\n", status);
	}

	if (refcount_dec_and_test(&work->refcount)) {
		kfree(work);
	}
}

static void ssh_handle_event(struct sam_ssh_ec *ec, const u8 *buf)
{
	struct device *dev = &ec->serdev->dev;
	const struct ssh_frame_ctrl *ctrl;
	const struct ssh_frame_cmd *cmd;
	struct ssh_event_work *work;
	unsigned long flags;
	u16 pld_len;

	surface_sam_ssh_event_handler_delay delay_fn;
	void *handler_data;
	unsigned long delay;

	ctrl = (const struct ssh_frame_ctrl *)(buf + SSH_FRAME_OFFS_CTRL);
	cmd  = (const struct ssh_frame_cmd  *)(buf + SSH_FRAME_OFFS_CMD);

	pld_len = ctrl->len - SSH_BYTELEN_CMDFRAME;

	work = kzalloc(sizeof(struct ssh_event_work) + pld_len, GFP_ATOMIC);
	if (!work) {
		dev_warn(dev, SSH_EVENT_TAG "failed to allocate memory, dropping event\n");
		return;
	}

	refcount_set(&work->refcount, 1);
	work->ec         = ec;
	work->seq        = ctrl->seq;
	work->event.rqid = (cmd->rqid_hi << 8) | cmd->rqid_lo;
	work->event.tc   = cmd->tc;
	work->event.cid  = cmd->cid;
	work->event.iid  = cmd->iid;
	work->event.pri  = cmd->pri_in;
	work->event.len  = pld_len;
	work->event.pld  = ((u8*) work) + sizeof(struct ssh_event_work);

	memcpy(work->event.pld, buf + SSH_FRAME_OFFS_CMD_PLD, pld_len);

	// queue ACK for if required
	if (ctrl->type == SSH_FRAME_TYPE_CMD) {
		refcount_set(&work->refcount, 2);
		INIT_WORK(&work->work_ack, surface_sam_ssh_event_work_ack_handler);
		queue_work(ec->events.queue_ack, &work->work_ack);
	}

	spin_lock_irqsave(&ec->events.lock, flags);
	handler_data = ec->events.handler[work->event.rqid - 1].data;
	delay_fn = ec->events.handler[work->event.rqid - 1].delay;

	/* Note:
	 * We need to check delay_fn here: This may have never been set as we
	 * can't guarantee that events only occur when they have been enabled.
	 */
	delay = delay_fn ? delay_fn(&work->event, handler_data) : 0;
	spin_unlock_irqrestore(&ec->events.lock, flags);

	// immediate execution for high priority events (e.g. keyboard)
	if (delay == SURFACE_SAM_SSH_EVENT_IMMEDIATE) {
		surface_sam_ssh_event_work_evt_handler(&work->work_evt.work);
	} else {
		INIT_DELAYED_WORK(&work->work_evt, surface_sam_ssh_event_work_evt_handler);
		queue_delayed_work(ec->events.queue_evt, &work->work_evt, delay);
	}
}

static int ssh_receive_msg_ctrl(struct sam_ssh_ec *ec, const u8 *buf, size_t size)
{
	struct device *dev = &ec->serdev->dev;
	struct ssh_receiver *rcv = &ec->receiver;
	const struct ssh_frame_ctrl *ctrl;
	struct ssh_fifo_packet packet;

	const u8 *ctrl_begin = buf + SSH_FRAME_OFFS_CTRL;
	const u8 *ctrl_end   = buf + SSH_FRAME_OFFS_CTRL_CRC;

	ctrl = (const struct ssh_frame_ctrl *)(ctrl_begin);

	// actual length check
	if (size < SSH_MSG_LEN_CTRL) {
		return 0;			// need more bytes
	}

	// validate TERM
	if (!ssh_is_valid_ter(buf + SSH_FRAME_OFFS_TERM)) {
		dev_err(dev, SSH_RECV_TAG "invalid end of message\n");
		return size;			// discard everything
	}

	// validate CRC
	if (!ssh_is_valid_crc(ctrl_begin, ctrl_end)) {
		dev_err(dev, SSH_RECV_TAG "invalid checksum (ctrl)\n");
		return SSH_MSG_LEN_CTRL;	// only discard message
	}

	// check if we expect the message
	if (rcv->state != SSH_RCV_CONTROL) {
		dev_err(dev, SSH_RECV_TAG "discarding message: ctrl not expected\n");
		return SSH_MSG_LEN_CTRL;	// discard message
	}

	// check if it is for our request
	if (ctrl->type == SSH_FRAME_TYPE_ACK && ctrl->seq != rcv->expect.seq) {
		dev_err(dev, SSH_RECV_TAG "discarding message: ack does not match\n");
		return SSH_MSG_LEN_CTRL;	// discard message
	}

	// we now have a valid & expected ACK/RETRY message
	dev_dbg(dev, SSH_RECV_TAG "valid control message received (type: 0x%02x)\n", ctrl->type);

	packet.type = ctrl->type;
	packet.seq  = ctrl->seq;
	packet.len  = 0;

	if (kfifo_avail(&rcv->fifo) >= sizeof(packet)) {
		kfifo_in(&rcv->fifo, (u8 *) &packet, sizeof(packet));

	} else {
		dev_warn(dev, SSH_RECV_TAG
			 "dropping frame: not enough space in fifo (type = %d)\n",
			 ctrl->type);

		return SSH_MSG_LEN_CTRL;	// discard message
	}

	// update decoder state
	if (ctrl->type == SSH_FRAME_TYPE_ACK) {
		rcv->state = rcv->expect.pld
			? SSH_RCV_COMMAND
			: SSH_RCV_DISCARD;
	}

	complete(&rcv->signal);
	return SSH_MSG_LEN_CTRL;		// handled message
}

static int ssh_receive_msg_cmd(struct sam_ssh_ec *ec, const u8 *buf, size_t size)
{
	struct device *dev = &ec->serdev->dev;
	struct ssh_receiver *rcv = &ec->receiver;
	const struct ssh_frame_ctrl *ctrl;
	const struct ssh_frame_cmd *cmd;
	struct ssh_fifo_packet packet;

	const u8 *ctrl_begin     = buf + SSH_FRAME_OFFS_CTRL;
	const u8 *ctrl_end       = buf + SSH_FRAME_OFFS_CTRL_CRC;
	const u8 *cmd_begin      = buf + SSH_FRAME_OFFS_CMD;
	const u8 *cmd_begin_pld  = buf + SSH_FRAME_OFFS_CMD_PLD;
	const u8 *cmd_end;

	size_t msg_len;

	ctrl = (const struct ssh_frame_ctrl *)(ctrl_begin);
	cmd  = (const struct ssh_frame_cmd  *)(cmd_begin);

	// we need at least a full control frame
	if (size < (SSH_BYTELEN_SYNC + SSH_BYTELEN_CTRL + SSH_BYTELEN_CRC)) {
		return 0;		// need more bytes
	}

	// validate control-frame CRC
	if (!ssh_is_valid_crc(ctrl_begin, ctrl_end)) {
		dev_err(dev, SSH_RECV_TAG "invalid checksum (cmd-ctrl)\n");
		/*
		 * We can't be sure here if length is valid, thus
		 * discard everything.
		 */
		return size;
	}

	// actual length check (ctrl->len contains command-frame but not crc)
	msg_len = SSH_MSG_LEN_CMD_BASE + ctrl->len;
	if (size < msg_len) {
		return 0;			// need more bytes
	}

	cmd_end = cmd_begin + ctrl->len;

	// validate command-frame type
	if (cmd->type != SSH_FRAME_TYPE_CMD) {
		dev_err(dev, SSH_RECV_TAG "expected command frame type but got 0x%02x\n", cmd->type);
		return size;			// discard everything
	}

	// validate command-frame CRC
	if (!ssh_is_valid_crc(cmd_begin, cmd_end)) {
		dev_err(dev, SSH_RECV_TAG "invalid checksum (cmd-pld)\n");

		/*
		 * The message length is provided in the control frame. As we
		 * already validated that, we can be sure here that it's
		 * correct, so we only need to discard the message.
		 */
		return msg_len;
	}

	// check if we received an event notification
	if (sam_rqid_is_event((cmd->rqid_hi << 8) | cmd->rqid_lo)) {
		ssh_handle_event(ec, buf);
		return msg_len;			// handled message
	}

	// check if we expect the message
	if (rcv->state != SSH_RCV_COMMAND) {
		dev_dbg(dev, SSH_RECV_TAG "discarding message: command not expected\n");
		return msg_len;			// discard message
	}

	// check if response is for our request
	if (rcv->expect.rqid != (cmd->rqid_lo | (cmd->rqid_hi << 8))) {
		dev_dbg(dev, SSH_RECV_TAG "discarding message: command not a match\n");
		return msg_len;			// discard message
	}

	// we now have a valid & expected command message
	dev_dbg(dev, SSH_RECV_TAG "valid command message received\n");

	packet.type = ctrl->type;
	packet.seq = ctrl->seq;
	packet.len = cmd_end - cmd_begin_pld;

	if (kfifo_avail(&rcv->fifo) >= sizeof(packet) + packet.len) {
		kfifo_in(&rcv->fifo, &packet, sizeof(packet));
		kfifo_in(&rcv->fifo, cmd_begin_pld, packet.len);

	} else {
		dev_warn(dev, SSH_RECV_TAG
			 "dropping frame: not enough space in fifo (type = %d)\n",
			 ctrl->type);

		return SSH_MSG_LEN_CTRL;	// discard message
	}

	rcv->state = SSH_RCV_DISCARD;

	complete(&rcv->signal);
	return msg_len;				// handled message
}

static int ssh_eval_buf(struct sam_ssh_ec *ec, const u8 *buf, size_t size)
{
	struct device *dev = &ec->serdev->dev;
	struct ssh_frame_ctrl *ctrl;

	// we need at least a control frame to check what to do
	if (size < (SSH_BYTELEN_SYNC + SSH_BYTELEN_CTRL)) {
		return 0;		// need more bytes
	}

	// make sure we're actually at the start of a new message
	if (!ssh_is_valid_syn(buf)) {
		dev_err(dev, SSH_RECV_TAG "invalid start of message\n");
		return size;		// discard everything
	}

	// handle individual message types seperately
	ctrl = (struct ssh_frame_ctrl *)(buf + SSH_FRAME_OFFS_CTRL);

	switch (ctrl->type) {
	case SSH_FRAME_TYPE_ACK:
	case SSH_FRAME_TYPE_RETRY:
		return ssh_receive_msg_ctrl(ec, buf, size);

	case SSH_FRAME_TYPE_CMD:
	case SSH_FRAME_TYPE_CMD_NOACK:
		return ssh_receive_msg_cmd(ec, buf, size);

	default:
		dev_err(dev, SSH_RECV_TAG "unknown frame type 0x%02x\n", ctrl->type);
		return size;		// discard everything
	}
}

static int ssh_receive_buf(struct serdev_device *serdev,
			   const unsigned char *buf, size_t size)
{
	struct sam_ssh_ec *ec = serdev_device_get_drvdata(serdev);
	struct ssh_receiver *rcv = &ec->receiver;
	unsigned long flags;
	int offs = 0;
	int used, n;

	dev_dbg(&serdev->dev, SSH_RECV_TAG "received buffer (size: %zu)\n", size);
	print_hex_dump_debug(SSH_RECV_TAG, DUMP_PREFIX_OFFSET, 16, 1, buf, size, false);

	/*
	 * The battery _BIX message gets a bit long, thus we have to add some
	 * additional buffering here.
	 */

	spin_lock_irqsave(&rcv->lock, flags);

	// copy to eval-buffer
	used = min(size, (size_t)(rcv->eval_buf.cap - rcv->eval_buf.len));
	memcpy(rcv->eval_buf.ptr + rcv->eval_buf.len, buf, used);
	rcv->eval_buf.len += used;

	// evaluate buffer until we need more bytes or eval-buf is empty
	while (offs < rcv->eval_buf.len) {
		n = rcv->eval_buf.len - offs;
		n = ssh_eval_buf(ec, rcv->eval_buf.ptr + offs, n);
		if (n <= 0) break;	// need more bytes

		offs += n;
	}

	// throw away the evaluated parts
	rcv->eval_buf.len -= offs;
	memmove(rcv->eval_buf.ptr, rcv->eval_buf.ptr + offs, rcv->eval_buf.len);

	spin_unlock_irqrestore(&rcv->lock, flags);

	return used;
}


#ifdef CONFIG_SURFACE_SAM_SSH_DEBUG_DEVICE

#include <linux/sysfs.h>

static char sam_ssh_debug_rqst_buf_sysfs[SURFACE_SAM_SSH_MAX_RQST_RESPONSE + 1] = { 0 };
static char sam_ssh_debug_rqst_buf_pld[SURFACE_SAM_SSH_MAX_RQST_PAYLOAD] = { 0 };
static char sam_ssh_debug_rqst_buf_res[SURFACE_SAM_SSH_MAX_RQST_RESPONSE] = { 0 };

struct sysfs_rqst {
	u8 tc;
	u8 cid;
	u8 iid;
	u8 pri;
	u8 snc;
	u8 cdl;
	u8 pld[0];
} __packed;

static ssize_t rqst_read(struct file *f, struct kobject *kobj, struct bin_attribute *attr,
                         char *buf, loff_t offs, size_t count)
{
	if (offs < 0 || count + offs > SURFACE_SAM_SSH_MAX_RQST_RESPONSE) {
		return -EINVAL;
	}

	memcpy(buf, sam_ssh_debug_rqst_buf_sysfs + offs, count);
	return count;
}

static ssize_t rqst_write(struct file *f, struct kobject *kobj, struct bin_attribute *attr,
			  char *buf, loff_t offs, size_t count)
{
	struct sysfs_rqst *input;
	struct surface_sam_ssh_rqst rqst = {};
	struct surface_sam_ssh_buf result = {};
	int status;

	// check basic write constriants
	if (offs != 0 || count > SURFACE_SAM_SSH_MAX_RQST_PAYLOAD + sizeof(struct sysfs_rqst)) {
		return -EINVAL;
	}

	if (count < sizeof(struct sysfs_rqst)) {
		return -EINVAL;
	}

	input = (struct sysfs_rqst *)buf;

	// payload length should be consistent with data provided
	if (input->cdl + sizeof(struct sysfs_rqst) != count) {
		return -EINVAL;
	}

	rqst.tc  = input->tc;
	rqst.cid = input->cid;
	rqst.iid = input->iid;
	rqst.pri = input->pri;
	rqst.snc = input->snc;
	rqst.cdl = input->cdl;
	rqst.pld = sam_ssh_debug_rqst_buf_pld;
	memcpy(sam_ssh_debug_rqst_buf_pld, &input->pld[0], input->cdl);

	result.cap = SURFACE_SAM_SSH_MAX_RQST_RESPONSE;
	result.len = 0;
	result.data = sam_ssh_debug_rqst_buf_res;

	status = surface_sam_ssh_rqst(&rqst, &result);
	if (status) {
		return status;
	}

	sam_ssh_debug_rqst_buf_sysfs[0] = result.len;
	memcpy(sam_ssh_debug_rqst_buf_sysfs + 1, result.data, result.len);
	memset(sam_ssh_debug_rqst_buf_sysfs + result.len + 1, 0,
	       SURFACE_SAM_SSH_MAX_RQST_RESPONSE + 1 - result.len);

	return count;
}

static const BIN_ATTR_RW(rqst, SURFACE_SAM_SSH_MAX_RQST_RESPONSE + 1);


int surface_sam_ssh_sysfs_register(struct device *dev)
{
	return sysfs_create_bin_file(&dev->kobj, &bin_attr_rqst);
}

void surface_sam_ssh_sysfs_unregister(struct device *dev)
{
	sysfs_remove_bin_file(&dev->kobj, &bin_attr_rqst);
}

#else	/* CONFIG_SURFACE_ACPI_SSH_DEBUG_DEVICE */

int surface_sam_ssh_sysfs_register(struct device *dev)
{
	return 0;
}

void surface_sam_ssh_sysfs_unregister(struct device *dev)
{
}

#endif	/* CONFIG_SURFACE_SAM_SSH_DEBUG_DEVICE */


static const struct acpi_gpio_params gpio_sam_wakeup_int = { 0, 0, false };
static const struct acpi_gpio_params gpio_sam_wakeup     = { 1, 0, false };

static const struct acpi_gpio_mapping surface_sam_acpi_gpios[] = {
	{ "sam_wakeup-int-gpio", &gpio_sam_wakeup_int, 1 },
	{ "sam_wakeup-gpio",     &gpio_sam_wakeup,     1 },
	{ },
};

static irqreturn_t surface_sam_irq_handler(int irq, void *dev_id)
{
	struct serdev_device *serdev = dev_id;

	dev_info(&serdev->dev, "wake irq triggered\n");
	return IRQ_HANDLED;
}

static int surface_sam_setup_irq(struct serdev_device *serdev)
{
	const int irqf = IRQF_SHARED | IRQF_ONESHOT | IRQF_TRIGGER_RISING;
	struct gpio_desc *gpiod;
	int irq;
	int status;

	gpiod = gpiod_get(&serdev->dev, "sam_wakeup-int", GPIOD_ASIS);
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	irq = gpiod_to_irq(gpiod);
	gpiod_put(gpiod);

	if (irq < 0)
		return irq;

	status = request_threaded_irq(irq, NULL, surface_sam_irq_handler,
				      irqf, "surface_sam_wakeup", serdev);
	if (status)
		return status;

	return irq;
}


static acpi_status
ssh_setup_from_resource(struct acpi_resource *resource, void *context)
{
	struct serdev_device *serdev = context;
	struct acpi_resource_common_serialbus *serial;
	struct acpi_resource_uart_serialbus *uart;
	int status = 0;

	if (resource->type != ACPI_RESOURCE_TYPE_SERIAL_BUS) {
		return AE_OK;
	}

	serial = &resource->data.common_serial_bus;
	if (serial->type != ACPI_RESOURCE_SERIAL_TYPE_UART) {
		return AE_OK;
	}

	uart = &resource->data.uart_serial_bus;

	// set up serdev device
	serdev_device_set_baudrate(serdev, uart->default_baud_rate);

	// serdev currently only supports RTSCTS flow control
	if (uart->flow_control & SSH_SUPPORTED_FLOW_CONTROL_MASK) {
		dev_warn(&serdev->dev, "unsupported flow control (value: 0x%02x)\n", uart->flow_control);
	}

	// set RTSCTS flow control
	serdev_device_set_flow_control(serdev, uart->flow_control & ACPI_UART_FLOW_CONTROL_HW);

	// serdev currently only supports EVEN/ODD parity
	switch (uart->parity) {
	case ACPI_UART_PARITY_NONE:
		status = serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
		break;
	case ACPI_UART_PARITY_EVEN:
		status = serdev_device_set_parity(serdev, SERDEV_PARITY_EVEN);
		break;
	case ACPI_UART_PARITY_ODD:
		status = serdev_device_set_parity(serdev, SERDEV_PARITY_ODD);
		break;
	default:
		dev_warn(&serdev->dev, "unsupported parity (value: 0x%02x)\n", uart->parity);
		break;
	}

	if (status) {
		dev_err(&serdev->dev, "failed to set parity (value: 0x%02x)\n", uart->parity);
		return status;
	}

	return AE_CTRL_TERMINATE;       // we've found the resource and are done
}


static int surface_sam_ssh_suspend(struct device *dev)
{
	struct sam_ssh_ec *ec;
	int status;

	dev_dbg(dev, "suspending\n");

	ec = surface_sam_ssh_acquire_init();
	if (ec) {
		status = surface_sam_ssh_ec_suspend(ec);
		if (status) {
			surface_sam_ssh_release(ec);
			return status;
		}

		if (device_may_wakeup(dev)) {
			status = enable_irq_wake(ec->irq);
			if (status) {
				surface_sam_ssh_release(ec);
				return status;
			}

			ec->irq_wakeup_enabled = true;
		} else {
			ec->irq_wakeup_enabled = false;
		}

		ec->state = SSH_EC_SUSPENDED;
		surface_sam_ssh_release(ec);
	}

	return 0;
}

static int surface_sam_ssh_resume(struct device *dev)
{
	struct sam_ssh_ec *ec;
	int status;

	dev_dbg(dev, "resuming\n");

	ec = surface_sam_ssh_acquire_init();
	if (ec) {
		ec->state = SSH_EC_INITIALIZED;

		if (ec->irq_wakeup_enabled) {
			status = disable_irq_wake(ec->irq);
			if (status) {
				surface_sam_ssh_release(ec);
				return status;
			}

			ec->irq_wakeup_enabled = false;
		}

		status = surface_sam_ssh_ec_resume(ec);
		if (status) {
			surface_sam_ssh_release(ec);
			return status;
		}

		surface_sam_ssh_release(ec);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(surface_sam_ssh_pm_ops, surface_sam_ssh_suspend, surface_sam_ssh_resume);


static const struct serdev_device_ops ssh_device_ops = {
	.receive_buf  = ssh_receive_buf,
	.write_wakeup = serdev_device_write_wakeup,
};


int surface_sam_ssh_sysfs_register(struct device *dev);
void surface_sam_ssh_sysfs_unregister(struct device *dev);

static int surface_sam_ssh_probe(struct serdev_device *serdev)
{
	struct sam_ssh_ec *ec;
	struct workqueue_struct *event_queue_ack;
	struct workqueue_struct *event_queue_evt;
	u8 *write_buf;
	u8 *read_buf;
	u8 *eval_buf;
	acpi_handle *ssh = ACPI_HANDLE(&serdev->dev);
	acpi_status status;
	int irq;

	dev_dbg(&serdev->dev, "probing\n");

	if (gpiod_count(&serdev->dev, NULL) < 0)
		return -ENODEV;

	status = devm_acpi_dev_add_driver_gpios(&serdev->dev, surface_sam_acpi_gpios);
	if (status)
		return status;

	// allocate buffers
	write_buf = kzalloc(SSH_WRITE_BUF_LEN, GFP_KERNEL);
	if (!write_buf) {
		status = -ENOMEM;
		goto err_write_buf;
	}

	read_buf = kzalloc(SSH_READ_BUF_LEN, GFP_KERNEL);
	if (!read_buf) {
		status = -ENOMEM;
		goto err_read_buf;
	}

	eval_buf = kzalloc(SSH_EVAL_BUF_LEN, GFP_KERNEL);
	if (!eval_buf) {
		status = -ENOMEM;
		goto err_eval_buf;
	}

	event_queue_ack = create_singlethread_workqueue("surface_sh_ackq");
	if (!event_queue_ack) {
		status = -ENOMEM;
		goto err_ackq;
	}

	event_queue_evt = create_workqueue("surface_sh_evtq");
	if (!event_queue_evt) {
		status = -ENOMEM;
		goto err_evtq;
	}

	irq = surface_sam_setup_irq(serdev);
	if (irq < 0) {
		status = irq;
		goto err_irq;
	}

	// set up EC
	ec = surface_sam_ssh_acquire();
	if (ec->state != SSH_EC_UNINITIALIZED) {
		dev_err(&serdev->dev, "embedded controller already initialized\n");
		surface_sam_ssh_release(ec);

		status = -EBUSY;
		goto err_busy;
	}

	ec->serdev      = serdev;
	ec->irq         = irq;
	ec->writer.data = write_buf;
	ec->writer.ptr  = write_buf;

	// initialize receiver
	init_completion(&ec->receiver.signal);
	kfifo_init(&ec->receiver.fifo, read_buf, SSH_READ_BUF_LEN);
	ec->receiver.eval_buf.ptr = eval_buf;
	ec->receiver.eval_buf.cap = SSH_EVAL_BUF_LEN;
	ec->receiver.eval_buf.len = 0;

	// initialize event handling
	ec->events.queue_ack = event_queue_ack;
	ec->events.queue_evt = event_queue_evt;

	ec->state = SSH_EC_INITIALIZED;

	serdev_device_set_drvdata(serdev, ec);

	// ensure everything is properly set-up before we open the device
	smp_mb();

	serdev_device_set_client_ops(serdev, &ssh_device_ops);
	status = serdev_device_open(serdev);
	if (status) {
		goto err_open;
	}

	status = acpi_walk_resources(ssh, METHOD_NAME__CRS,
	                             ssh_setup_from_resource, serdev);
	if (ACPI_FAILURE(status)) {
		goto err_devinit;
	}

	status = surface_sam_ssh_ec_resume(ec);
	if (status) {
		goto err_devinit;
	}

	status = surface_sam_ssh_sysfs_register(&serdev->dev);
	if (status) {
		goto err_devinit;
	}

	surface_sam_ssh_release(ec);

	// TODO: The EC can wake up the system via the associated GPIO interrupt in
	// multiple situations. One of which is the remaining battery capacity
	// falling below a certain threshold. Normally, we should use the
	// device_init_wakeup function, however, the EC also seems to have other
	// reasons for waking up the system and it seems that Windows has
	// additional checks whether the system should be resumed. In short, this
	// causes some spourious unwanted wake-ups. For now let's thus default
	// power/wakeup to false.
	device_set_wakeup_capable(&serdev->dev, true);
	acpi_walk_dep_device_list(ssh);

	return 0;

err_devinit:
	serdev_device_close(serdev);
err_open:
	ec->state = SSH_EC_UNINITIALIZED;
	serdev_device_set_drvdata(serdev, NULL);
	surface_sam_ssh_release(ec);
err_busy:
	free_irq(irq, serdev);
err_irq:
	destroy_workqueue(event_queue_evt);
err_evtq:
	destroy_workqueue(event_queue_ack);
err_ackq:
	kfree(eval_buf);
err_eval_buf:
	kfree(read_buf);
err_read_buf:
	kfree(write_buf);
err_write_buf:
	return status;
}

static void surface_sam_ssh_remove(struct serdev_device *serdev)
{
	struct sam_ssh_ec *ec;
	unsigned long flags;
	int status;

	ec = surface_sam_ssh_acquire_init();
	if (!ec) {
		return;
	}

	free_irq(ec->irq, serdev);
	surface_sam_ssh_sysfs_unregister(&serdev->dev);

	// suspend EC and disable events
	status = surface_sam_ssh_ec_suspend(ec);
	if (status) {
		dev_err(&serdev->dev, "failed to suspend EC: %d\n", status);
	}

	// make sure all events (received up to now) have been properly handled
	flush_workqueue(ec->events.queue_ack);
	flush_workqueue(ec->events.queue_evt);

	// remove event handlers
	spin_lock_irqsave(&ec->events.lock, flags);
	memset(ec->events.handler, 0,
	       sizeof(struct ssh_event_handler)
	        * SAM_NUM_EVENT_TYPES);
	spin_unlock_irqrestore(&ec->events.lock, flags);

	// set device to deinitialized state
	ec->state  = SSH_EC_UNINITIALIZED;
	ec->serdev = NULL;

	// ensure state and serdev get set before continuing
	smp_mb();

	/*
	 * Flush any event that has not been processed yet to ensure we're not going to
	 * use the serial device any more (e.g. for ACKing).
	 */
	flush_workqueue(ec->events.queue_ack);
	flush_workqueue(ec->events.queue_evt);

	serdev_device_close(serdev);

	/*
         * Only at this point, no new events can be received. Destroying the
         * workqueue here flushes all remaining events. Those events will be
         * silently ignored and neither ACKed nor any handler gets called.
	 */
	destroy_workqueue(ec->events.queue_ack);
	destroy_workqueue(ec->events.queue_evt);

	// free writer
	kfree(ec->writer.data);
	ec->writer.data = NULL;
	ec->writer.ptr  = NULL;

	// free receiver
	spin_lock_irqsave(&ec->receiver.lock, flags);
	ec->receiver.state = SSH_RCV_DISCARD;
	kfifo_free(&ec->receiver.fifo);

	kfree(ec->receiver.eval_buf.ptr);
	ec->receiver.eval_buf.ptr = NULL;
	ec->receiver.eval_buf.cap = 0;
	ec->receiver.eval_buf.len = 0;
	spin_unlock_irqrestore(&ec->receiver.lock, flags);

	device_set_wakeup_capable(&serdev->dev, false);
	serdev_device_set_drvdata(serdev, NULL);
	surface_sam_ssh_release(ec);
}


static const struct acpi_device_id surface_sam_ssh_match[] = {
	{ "MSHW0084", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, surface_sam_ssh_match);

static struct serdev_device_driver surface_sam_ssh = {
	.probe = surface_sam_ssh_probe,
	.remove = surface_sam_ssh_remove,
	.driver = {
		.name = "surface_sam_ssh",
		.acpi_match_table = ACPI_PTR(surface_sam_ssh_match),
		.pm = &surface_sam_ssh_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};


static int __init surface_sam_ssh_init(void)
{
	return serdev_device_driver_register(&surface_sam_ssh);
}

static void __exit surface_sam_ssh_exit(void)
{
	serdev_device_driver_unregister(&surface_sam_ssh);
}

/*
 * Ensure that the driver is loaded late due to some issues with the UART
 * communication. Specifically, we want to ensure that DMA is ready and being
 * used. Not using DMA can result in spurious communication failures,
 * especially during boot, which among other things will result in wrong
 * battery information (via ACPI _BIX) being displayed. Using a late init_call
 * instead of the normal module_init gives the DMA subsystem time to
 * initialize and via that results in a more stable communication, avoiding
 * such failures.
 */
late_initcall(surface_sam_ssh_init);
module_exit(surface_sam_ssh_exit);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Surface Serial Hub Driver for 5th Generation Surface Devices");
MODULE_LICENSE("GPL v2");
