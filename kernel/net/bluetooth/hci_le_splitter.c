#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/hci_le_splitter.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>


/* RXed bytes we'll queue before giving up on userspace. picked arbitrarily */
#define MAX_RX_QUEUE_SZ				524288


#undef pr_fmt
#define pr_fmt(fmt) "le_splitter " fmt


struct hci_le_splitter_le_conn {
	u16 conn_id;
	size_t tx_in_flight;
};


/* This mutex protects the below (essentially splitter internal state) */
static DEFINE_MUTEX(hci_state_lock);
static struct hci_dev *cur_dev;
static u16 le_waiting_on_opcode;
static int local_lmp_ver;
static struct hci_le_splitter_le_conn tracked_conns[HCI_LE_SPLIT_MAX_LE_CONNS];
static u32 max_pkts_in_flight;
static u32 cur_pkts_in_flight;
static u8 splitter_enable_state = SPLITTER_STATE_NOT_SET;

/* protects command sequencing */
static DEFINE_SEMAPHORE(cmd_sem);

/* "is userspace connected?" */
static atomic_t usr_connected = ATOMIC_INIT(0);

/* "is chip in state to talk to second stack?" */
static atomic_t chip_ready_for_second_stack = ATOMIC_INIT(0);

/* protects messages waiting to be read */
static DEFINE_MUTEX(usr_msg_q_lock);
static DECLARE_WAIT_QUEUE_HEAD(usr_msg_wait_q);
static struct sk_buff_head usr_msg_q;
static size_t usr_msg_q_len; /* in bytes of actual data */


static DECLARE_WAIT_QUEUE_HEAD(tx_has_room_wait_q);



/* event masks for things we need */
/* disconnect, encr_change, encr_refresh */
static const uint64_t evtsNeeded = 0x0000800000000090ULL;
/* LE meta event */
static const uint64_t evtsLE = 0x2000000000000000ULL;

#define INVALID_CONN_ID			0xffff

/* missing hci defs */
#define HCI_OGF_LE			0x08
#define BT_HCI_VERSION_3		5
#define BT_LMP_VERSION_3		5
#define HCI_OP_READ_LE_HOST_SUPPORTED	0x0c6c

struct hci_rp_read_le_host_supported {
	u8	status;
	u8	le;
	u8	simul;
} __packed;



static void hci_le_splitter_usr_queue_reset_message(bool allow_commands);
static struct hci_le_splitter_le_conn *cid_find_le_conn(u16 cid);
static struct device_attribute sysfs_attr;
static struct miscdevice mdev;


/* always called with usr_msg_q_lock held */
static void hci_le_splitter_usr_purge_rx_q(void)
{
	struct sk_buff *skb;
	while (!skb_queue_empty(&usr_msg_q)) {
		skb = skb_dequeue(&usr_msg_q);
		if (skb)
			kfree_skb(skb);
	}
	usr_msg_q_len = 0;
}

/* always called with hci_state_lock held */
static void reset_tracked_le_conns(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(tracked_conns); ++i) {
		tracked_conns[i].conn_id = INVALID_CONN_ID;
		tracked_conns[i].tx_in_flight = 0;
	}
	cur_pkts_in_flight = 0;
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_set_our_dev(struct hci_dev *hdev)
{
	if (!cur_dev) {

		cur_dev = hdev;

		local_lmp_ver = -1;
		max_pkts_in_flight = 0;
		le_waiting_on_opcode = 0;
		reset_tracked_le_conns();

		return true;
	}

	return false;
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_is_our_dev(struct hci_dev *hdev)
{
	return cur_dev == hdev;
}

void hci_le_splitter_init_start(struct hci_dev *hdev)
{
	mutex_lock(&hci_state_lock);

	if (hci_le_splitter_set_our_dev(hdev))
		pr_info("HCI splitter inited\n");
	else
		pr_info("HCI splitter ignoring dev\n");

	mutex_unlock(&hci_state_lock);
}

int hci_le_splitter_init_done(struct hci_dev *hdev)
{
	/* nothing to do for now */

	return 0;
}

static ssize_t hci_le_splitter_read(struct file *file, char __user *userbuf,
				    size_t bytes, loff_t *off)
{
	struct sk_buff *skb;
	u8 packet_typ;
	ssize_t ret;


	mutex_lock(&usr_msg_q_lock);

	do {
		while (!usr_msg_q_len) {
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				goto bailout;
			}
			mutex_unlock(&usr_msg_q_lock);
			if (wait_event_interruptible(usr_msg_wait_q,
						     usr_msg_q_len)) {
				ret = -EINTR;
				goto bailout_q_unlocked;
			}
			mutex_lock(&usr_msg_q_lock);
		}

		skb = skb_dequeue(&usr_msg_q);

	} while (!skb);

	/* one byte for hci packet type */
	packet_typ = hci_skb_pkt_type(skb);

	if (skb->len + sizeof(packet_typ) > bytes) {
		ret = -ENOMEM;
	} else if (put_user(packet_typ, userbuf) ||
			copy_to_user(userbuf + sizeof(packet_typ),
							skb->data, skb->len)) {
		ret = -EFAULT;
	} else {
		usr_msg_q_len -= skb->len;
		ret = (ssize_t)skb->len + 1;
		kfree_skb(skb);
	}

	if (ret < 0)
		skb_queue_head(&usr_msg_q, skb);

bailout:
	mutex_unlock(&usr_msg_q_lock);

bailout_q_unlocked:
	return ret;
}

static ssize_t hci_le_splitter_write(struct file *file,
				     const char __user *userbuf, size_t bytes,
				     loff_t *off)
{
	struct hci_acl_hdr acl_hdr;
	struct sk_buff *skb;
	u16 cmd_val = 0;
	ssize_t ret;
	u8 pkt_typ;

	if (bytes < 1)
		return -EINVAL;

	if (copy_from_user(&pkt_typ, userbuf, sizeof(pkt_typ)))
		return -EFAULT;

	bytes -= sizeof(pkt_typ);
	userbuf += sizeof(pkt_typ);

	/* figure out the size and do some sanity checks */
	if (pkt_typ == HCI_ACLDATA_PKT) {


		if (bytes < HCI_ACL_HDR_SIZE)
			return -EFAULT;

		if (copy_from_user(&acl_hdr, userbuf, HCI_ACL_HDR_SIZE))
			return -EFAULT;

		/* verify length */
		if (bytes - __le16_to_cpu(acl_hdr.dlen) - HCI_ACL_HDR_SIZE)
			return -EINVAL;

	} else if (pkt_typ == HCI_COMMAND_PKT) {

		struct hci_command_hdr cmd_hdr;

		if (bytes < HCI_COMMAND_HDR_SIZE)
			return -EFAULT;

		if (copy_from_user(&cmd_hdr, userbuf, HCI_COMMAND_HDR_SIZE))
			return -EFAULT;

		/* verify length */
		if (bytes - cmd_hdr.plen - HCI_COMMAND_HDR_SIZE)
			return -EINVAL;

		cmd_val = __le16_to_cpu(cmd_hdr.opcode);

	} else {
		return -EINVAL;
	}

	skb = bt_skb_alloc(bytes, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	hci_skb_pkt_type(skb) = pkt_typ;
	if (copy_from_user(skb_put(skb, bytes), userbuf, bytes)) {
		kfree_skb(skb);
		return -EFAULT;
	}
	__net_timestamp(skb);

	/* perform appropriate actions-before-sending */
	if (pkt_typ == HCI_COMMAND_PKT) {

		/* commands require the semaphore */
		down(&cmd_sem);
		mutex_lock(&hci_state_lock);

	} else {
		/* ACL data is not allowed without credits
		 * (stack should be keeping track of this)
		 */
		u16 acl_hndl = hci_handle(__le16_to_cpu(acl_hdr.handle));
		struct hci_le_splitter_le_conn *conn;

		mutex_lock(&hci_state_lock);
		if (max_pkts_in_flight == cur_pkts_in_flight) {
			kfree_skb(skb);
			ret = -ENOSPC;
			goto out_unlock;
		}

		/* find conn & increment its inflight packet counter */
		conn = cid_find_le_conn(acl_hndl);
		if (!conn) {
			kfree_skb(skb);
			ret = -ENOENT;
			goto out_unlock;
		}

		conn->tx_in_flight++;
		cur_pkts_in_flight++;
	}

	/* perform the actual transmission */
	if (pkt_typ == HCI_COMMAND_PKT)
		le_waiting_on_opcode = cmd_val;

	hci_send_to_monitor(cur_dev, skb);
	skb_orphan(skb);
	if (cur_dev->send(cur_dev, skb) < 0) {
		kfree_skb(skb);

		if (pkt_typ == HCI_COMMAND_PKT)
			up(&cmd_sem);

		ret = -EIO;
		goto out_unlock;
	}

	ret = bytes + sizeof(pkt_typ);

out_unlock:
	mutex_unlock(&hci_state_lock);
	return ret;
}

static unsigned int hci_le_splitter_poll(struct file *file,
					 struct poll_table_struct *wait)
{
	int ret = 0;

	poll_wait(file, &usr_msg_wait_q, wait);

	mutex_lock(&usr_msg_q_lock);
	if (usr_msg_q_len)
		ret |= (POLLIN | POLLRDNORM);

	/* poll for POLLOUT only indicates data TX ability.
	 * commands can always be sent and will block!
	 */
	poll_wait(file, &tx_has_room_wait_q, wait);
	if (max_pkts_in_flight > cur_pkts_in_flight)
		ret |= (POLLOUT | POLLWRNORM);

	mutex_unlock(&usr_msg_q_lock);
	return ret;
}

static int hci_le_splitter_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	if (atomic_cmpxchg(&usr_connected, 0, 1))
		return -EBUSY;

	mutex_lock(&usr_msg_q_lock);
	hci_le_splitter_usr_purge_rx_q();
	mutex_unlock(&usr_msg_q_lock);
	hci_le_splitter_usr_queue_reset_message(atomic_read(&chip_ready_for_second_stack) != 0);

	return ret;
}

static int hci_le_splitter_release(struct inode *inode, struct file *file)
{
	int32_t dev_id = -1;

	mutex_lock(&usr_msg_q_lock);
	hci_le_splitter_usr_purge_rx_q();

	if (atomic_cmpxchg(&usr_connected, 1, 0)) {
		/* file close while chip was being used - we must reset it */
		if (cur_dev)
			dev_id = cur_dev->id;
		pr_info("reset queued\n");
	}

	atomic_set(&chip_ready_for_second_stack, 0);
	mutex_unlock(&usr_msg_q_lock);

	if (dev_id >= 0) {
		hci_dev_reset(dev_id);
		/* none of this matters - we must restart bluetoothd to regain ability to run */
	}

	return 0;
}

static long hci_le_splitter_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sk_buff *skb;
	int readable_sz;

	switch (cmd) {
	case FIONREAD:		/* if we had multiple readers, this would be bad */
		mutex_lock(&usr_msg_q_lock);
		skb = skb_peek(&usr_msg_q);
		readable_sz = skb ? skb->len + 1 : 0;
		mutex_unlock(&usr_msg_q_lock);
		return put_user(readable_sz, (int __user *)arg);

	default:
		return -EINVAL;
	}
}

static ssize_t hci_le_splitter_sysfs_enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	mutex_lock(&hci_state_lock);
	switch (splitter_enable_state) {
	case SPLITTER_STATE_NOT_SET:
		ret = sprintf(buf, "%s\n", "NOT SET");
		break;
	case SPLITTER_STATE_DISABLED:
		ret = sprintf(buf, "%s\n", "OFF");
		break;
	case SPLITTER_STATE_ENABLED:
		ret = sprintf(buf, "%s\n", "ON");
		break;
	}
	mutex_unlock(&hci_state_lock);

	return ret;
}

static ssize_t hci_le_splitter_sysfs_enabled_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	ssize_t ret = strlen(buf);
	bool set;

	if (strtobool(buf, &set) < 0)
		return -EINVAL;

	mutex_lock(&hci_state_lock);
	if (splitter_enable_state == SPLITTER_STATE_NOT_SET)
		splitter_enable_state =
			set ? SPLITTER_STATE_ENABLED : SPLITTER_STATE_DISABLED;
	else
		ret = -EPERM;
	mutex_unlock(&hci_state_lock);

	return ret;
}

static const struct file_operations hci_le_splitter_fops = {

	.read	        = hci_le_splitter_read,
	.write	        = hci_le_splitter_write,
	.open	        = hci_le_splitter_open,
	.poll	        = hci_le_splitter_poll,
	.release	= hci_le_splitter_release,
	.unlocked_ioctl	= hci_le_splitter_ioctl,
};

static struct miscdevice mdev = {

	.minor = MISC_DYNAMIC_MINOR,
	.name  = "hci_le",
	.fops  = &hci_le_splitter_fops
};

static struct device_attribute sysfs_attr =
	__ATTR(le_splitter_enabled,           /* file name */
	0660,                                 /* file permissions */
	hci_le_splitter_sysfs_enabled_show,   /* file read fn */
	hci_le_splitter_sysfs_enabled_store); /* file write fn */


void hci_le_splitter_deinit(struct hci_dev *hdev)
{
	mutex_lock(&hci_state_lock);
	if (hci_le_splitter_is_our_dev(hdev)) {
		mutex_lock(&usr_msg_q_lock);
		cur_dev = NULL;
		pr_info("HCI splitter unregistered\n");

		hci_le_splitter_usr_purge_rx_q();
		wake_up_interruptible(&usr_msg_wait_q);
		wake_up_interruptible(&tx_has_room_wait_q);
		atomic_set(&chip_ready_for_second_stack, 0);
		mutex_unlock(&usr_msg_q_lock);
		hci_le_splitter_usr_queue_reset_message(false);
	}
	mutex_unlock(&hci_state_lock);
}

void hci_le_splitter_init_fail(struct hci_dev *hdev)
{
	(void)hci_le_splitter_deinit(hdev);
}

bool hci_le_splitter_should_allow_bluez_tx(struct hci_dev *hdev, struct sk_buff *skb)
{
	bool ret = true, skipsem = true;
	u16 opcode = 0;

	mutex_lock(&hci_state_lock);

	if (hci_le_splitter_is_our_dev(hdev) &&
	    bt_cb(skb)->pkt_type == HCI_COMMAND_PKT) {

		void *cmdParams = ((u8 *)skb->data) + HCI_COMMAND_HDR_SIZE;
		opcode = hci_skb_opcode(skb);

		skipsem = false;

		if (splitter_enable_state == SPLITTER_STATE_NOT_SET) {
			/* if state is not set, drop all packets */
			pr_warn("LE splitter not initialized - chip TX denied!\n");
			ret = false;
		} else if (splitter_enable_state == SPLITTER_STATE_DISABLED) {
			/* if disabled - allow all packets */
			ret = true;
			skipsem = true;
		} else if (opcode == HCI_OP_RESET) {

			static bool first = true;
			if (!first)
				pr_warn("unexpected chip reset\n");
			first = false;
			/* notify other stack of reset, do not allow commands */
			hci_le_splitter_usr_queue_reset_message(false);
			reset_tracked_le_conns();
			skipsem = true;

		} else if (opcode == HCI_OP_WRITE_LE_HOST_SUPPORTED) {

			struct hci_cp_write_le_host_supported *params =
				cmdParams;

			if (params->le)
				pr_warn("EDR stack enabling LE  -> bad\n");
			if (params->simul)
				pr_warn("EDR stack enabling simul -> bad\n");

			params->le = 1;
			params->simul = 1;

		} else if (opcode == HCI_OP_SET_EVENT_MASK) {

			struct hci_cp_le_set_event_mask *params =
				cmdParams;
			__le64 *mask_loc;
			uint64_t mask, oldmask;

			mask_loc = (__le64 *)params->mask;
			oldmask = mask = __le64_to_cpup(mask_loc);

			if ((mask & evtsNeeded) != evtsNeeded) {
				pr_warn("EDR stack blocked some vital events - BAD - fixing\n");
				mask |= evtsNeeded;
			}
			if (mask & evtsLE)
				pr_info("EDR stack unmasked some events unexpectedly - OK, just weird\n");
			mask |= evtsLE;
			*mask_loc = __cpu_to_le64(mask);
			pr_info("modified event mask 0x%016llX -> 0x%016llX\n",
				(unsigned long long)oldmask,
				(unsigned long long)mask);

			/* it is now safe to prepare our stack, if this is the first time we've seen this since we were not ready */
			if (!atomic_cmpxchg(&chip_ready_for_second_stack, 0, 1))
				hci_le_splitter_usr_queue_reset_message(true);

		} else if (hci_opcode_ogf(opcode) == HCI_OGF_LE) {

			pr_warn("LE command (0x%x) from EDR stack!\n",
			     hci_opcode_ocf(opcode));
			ret = false;
		}
	}

	mutex_unlock(&hci_state_lock);

	if (ret && !skipsem && bt_cb(skb)->pkt_type == HCI_COMMAND_PKT) {
		if (-ETIMEDOUT == down_timeout(&cmd_sem, msecs_to_jiffies(1000)))
			pr_err("Breaking semaphore for out-of-order bluez command 0x%x"
				" write due to sem timeout\n", opcode);
	}
	return ret;
}

/* always called with hci_state_lock held */
static struct hci_le_splitter_le_conn *cid_find_le_conn(u16 cid)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(tracked_conns); i++)
		if (cid == tracked_conns[i].conn_id)
			return tracked_conns + i;
	return NULL;
}

/* always called with hci_state_lock held */
static bool cid_is_le_conn(u16 cid)
{
	return !!cid_find_le_conn(cid);
}

/* always takes ownership of skb */
static void hci_le_splitter_enq_packet(struct sk_buff *skb)
{
	mutex_lock(&usr_msg_q_lock);
	if (MAX_RX_QUEUE_SZ - usr_msg_q_len < skb->len) {

		pr_err("enqueue failed - not enough space to enqueue %u bytes over %zu\n",
		     skb->len, usr_msg_q_len);
		kfree_skb(skb);
	} else {
		usr_msg_q_len += skb->len;
		skb_queue_tail(&usr_msg_q, skb);
	}
	mutex_unlock(&usr_msg_q_lock);

	/* wake up userspace in either case as we have data */
	wake_up_interruptible(&usr_msg_wait_q);
}

/* used to indicate to userspace when it is and is not ok to send commands */
static void hci_le_splitter_usr_queue_reset_message(bool allow_commands)
{
	struct sk_buff *skb = bt_skb_alloc(HCI_EVENT_HDR_SIZE +
					   sizeof(struct hci_ev_cmd_complete),
					   GFP_KERNEL);
	struct hci_ev_cmd_complete *cc;
	struct hci_event_hdr *ev;

	if (!skb)
		return;

	ev = (struct hci_event_hdr *)skb_put(skb, HCI_EVENT_HDR_SIZE);
	cc = (struct hci_ev_cmd_complete *)skb_put(skb, sizeof(struct hci_ev_cmd_complete));

	hci_skb_pkt_type(skb) = HCI_EVENT_PKT;
	ev->evt = HCI_EV_CMD_COMPLETE;
	ev->plen = sizeof(struct hci_ev_cmd_complete);

	cc->ncmd = allow_commands ? 1 : 0;
	cc->opcode = cpu_to_le16(HCI_OP_RESET);

	hci_le_splitter_enq_packet(skb);
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_filter_le_meta(const struct hci_ev_le_meta *meta)
{
	const struct hci_ev_le_conn_complete *evt;

	/* We always pass LE meta events to the LE half of things. This
	 * filter's main job is to look for successful LE connection completed
	 * events and record the connection ID in the tracked_conns
	 * array used for filtering subsequent messages.
	 */
	if (HCI_EV_LE_CONN_COMPLETE != meta->subevent)
		return true;

	evt = (struct hci_ev_le_conn_complete *)(meta + 1);
	if (!evt->status) {
		u16 cid = __le16_to_cpu(evt->handle);
		size_t i;

		for (i = 0; i < ARRAY_SIZE(tracked_conns); i++) {
			struct hci_le_splitter_le_conn *c = tracked_conns + i;

			if (c->conn_id == INVALID_CONN_ID) {
				c->conn_id = cid;
				BUG_ON(c->tx_in_flight);
				break;
			}
		}

		/* At the point where we are receiving a connection completed
		 * event, we should always have room in our tracked array.  If
		 * we don't it can only be because of a bookkeeping bug.
		 */
		BUG_ON(i >= ARRAY_SIZE(tracked_conns));
	}
	return true;
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_filter_disconn(const struct hci_ev_disconn_complete *evt)
{
	u16 cid = __le16_to_cpu(evt->handle);
	struct hci_le_splitter_le_conn *c = cid_find_le_conn(cid);

	if (!c)
		return false;

	BUG_ON(cur_pkts_in_flight < c->tx_in_flight);

	c->conn_id = INVALID_CONN_ID;
	cur_pkts_in_flight -= c->tx_in_flight;
	c->tx_in_flight = 0;

	return true;
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_is_our_reply(u16 opcode)
{
	if (!le_waiting_on_opcode || le_waiting_on_opcode != opcode)
		return false;

	le_waiting_on_opcode = 0;
	return true;
}

static void hci_le_splitter_filter_ftr_page_0(u8 *features)
{
	if (features[4] & BIT(6)) {
		pr_debug("features[0]: clearing 4,6 - LE supported\n");
		features[4] &= ~BIT(6);
	}
	if (features[6] & BIT(1)) {
		pr_debug("features[0]: clearing 6,1 - EDR/LE simult\n");
		features[6] &= ~BIT(1);
	}
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_filter_cmd_complete(const struct hci_ev_cmd_complete *evt)
{
	u16 opcode = __le16_to_cpu(evt->opcode);
	bool ours = hci_le_splitter_is_our_reply(opcode);
	int i;

	if (opcode == HCI_OP_LE_READ_BUFFER_SIZE) {

		struct hci_rp_le_read_buffer_size *le_buf_sz =
			(struct hci_rp_le_read_buffer_size *)(evt + 1);

		if (!le_buf_sz->status && le_buf_sz->le_max_pkt &&
		    le_buf_sz->le_mtu)
			max_pkts_in_flight = le_buf_sz->le_max_pkt;
	} else if (opcode == HCI_OP_RESET) {
		/* Any and all command credits we have/had go out the
		 * window after a reset, and we instead have one. This
		 * code assures our semaphore acts that way too.
		 */
		while (!down_trylock(&cmd_sem))
			;
		up(&cmd_sem);
	 } else if (opcode == HCI_OP_READ_LE_HOST_SUPPORTED && !ours) {
		struct hci_rp_read_le_host_supported *repl =
			(struct hci_rp_read_le_host_supported *)(evt + 1);

		if (repl->le)
			pr_debug("host_support: clearing le\n");
		if (repl->simul)
			pr_debug("host_support: clearing simul\n");
		repl->le = 0;
		repl->simul = 0;
	} else if (opcode == HCI_OP_READ_LOCAL_VERSION && !ours) {
		struct hci_rp_read_local_version *repl =
			(struct hci_rp_read_local_version *)(evt + 1);

		if (!repl->status) {

			local_lmp_ver = repl->lmp_ver;

			if (repl->hci_ver > BT_HCI_VERSION_3) {
				pr_debug("Downgrading BT HCI version from %d\n",
				     repl->hci_ver);
				repl->hci_ver = BT_HCI_VERSION_3;
			}
			if (repl->lmp_ver > BT_LMP_VERSION_3) {
				pr_debug("Downgrading BT LMP version from %d\n",
				     repl->lmp_ver);
				repl->lmp_ver = BT_LMP_VERSION_3;
			}
		}
	} else if (opcode == HCI_OP_READ_LOCAL_COMMANDS && !ours) {
		struct hci_rp_read_local_commands *repl =
			(struct hci_rp_read_local_commands *)(evt + 1);

		if (repl->commands[24] & 0x60) {
			pr_debug("supported commands: ~LE in byte 24\n");
			repl->commands[24] &= ~0x60;
		}
		/* these are LE commands */
		for (i = 25; i <= 28; i++) {
			if (repl->commands[i])
				pr_debug("supported commands: clearing byte %d\n",
				     i);
			repl->commands[i] = 0;
		}

		/* unsetting support for 5.0 LE commands: byte 33 (partial) and
		 * byte 34-39 (full)
		 */
		if (repl->commands[33] & 0xf0) {
			pr_debug("supported commands: ~LE in byte 33\n");
			repl->commands[33] &= ~0xf0;
		}
		for (i = 34; i <= 39; i++) {
			if (repl->commands[i])
				pr_debug("supported commands: clearing byte %d\n",
					 i);
			repl->commands[i] = 0;
		}
	} else if (opcode == HCI_OP_READ_LOCAL_FEATURES && !ours) {
		struct hci_rp_read_local_features *repl =
			(struct hci_rp_read_local_features *)(evt + 1);

		hci_le_splitter_filter_ftr_page_0(repl->features);
	} else if (opcode == HCI_OP_READ_LOCAL_EXT_FEATURES && !ours) {
		struct hci_rp_read_local_ext_features *repl =
			(struct hci_rp_read_local_ext_features *)(evt + 1);

		if (repl->page == 0) {
			hci_le_splitter_filter_ftr_page_0(repl->features);
		} else if (repl->page == 1) {
			if (repl->features[0] & 0x02) {
				pr_debug("features[1]: clr 0,1 - LE supported\n");
				repl->features[4] &= ~0x02;
			}
			if (repl->features[0] & 0x04) {
				pr_debug("features[1]: clr 0,2 - EDR/LE simult\n");
				repl->features[4] &= ~0x04;
			}
		}
	} else if (opcode == HCI_OP_READ_BUFFER_SIZE) {
		struct hci_rp_read_buffer_size *repl =
			(struct hci_rp_read_buffer_size *)(evt + 1);
		u16 pkts, edr_pkts, le_pkts, reported_pkts;

		pkts = __le16_to_cpu(repl->acl_max_pkt);
		/* If we cannot hit our target number of payloads and
		 * leave at least that many payloads for the EDR
		 * stack, then give 1/2 rounded up to the normal stack,
		 * and 1/2 rounded down to LE.
		 */
		if (pkts < (2 * HCI_LE_SPLITTER_BUFFER_TARGET))
			le_pkts = pkts >> 1;
		else
			le_pkts = HCI_LE_SPLITTER_BUFFER_TARGET;
		edr_pkts = pkts - le_pkts;
		reported_pkts = ours ? le_pkts : edr_pkts;
		pr_info("Chip has %hu bufs, telling %s: '%hu bufs'.\n", pkts,
		     ours ? "LE" : "EDR", reported_pkts);
		repl->acl_max_pkt = __cpu_to_le16(reported_pkts);

		if (!max_pkts_in_flight)
			max_pkts_in_flight = le_pkts;
	} else if (hci_opcode_ogf(opcode) == HCI_OGF_LE && !ours) {

		pr_warn("unexpected LE cmd complete");
	}

	return ours;
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_filter_cmd_status(const struct hci_ev_cmd_status *evt)
{
	u16 opcode = __le16_to_cpu(evt->opcode);
	bool ours = hci_le_splitter_is_our_reply(opcode);

	if (hci_opcode_ogf(opcode) == HCI_OGF_LE && !ours)
		pr_warn("unexpected LE status");

	return ours;
}

/* always called with hci_state_lock held */
static bool hci_le_splitter_filter_num_comp_pkt(struct hci_ev_num_comp_pkts *evt,
						int *len_chng)
{
	int le_nonzero_conns = 0, edr_nonzero_conns = 0, pkts, i, j, hndl;
	int le_pkts = 0, edr_pkts = 0, le_conns = 0, edr_conns = 0;
	struct hci_le_splitter_le_conn *le_conn;

	/* first see how many of each packets there are (ours and bluez's) */
	for (i = 0; i < evt->num_hndl; i++) {

		hndl = __le16_to_cpu(evt->handles[i].handle);
		pkts = __le16_to_cpu(evt->handles[i].count);
		le_conn = cid_find_le_conn(hndl);

		if (le_conn) {
			le_conns++;
			le_pkts += pkts;
			if (pkts) {
				le_nonzero_conns++;
				BUG_ON(le_conn->tx_in_flight < pkts);
				BUG_ON(cur_pkts_in_flight < pkts);
				le_conn->tx_in_flight -= pkts;
				cur_pkts_in_flight -= pkts;
			}
		} else {
			edr_conns++;
			edr_pkts += pkts;
			if (pkts)
				edr_nonzero_conns++;
		}
	}

	/* if we have le packets, send an event to LE stack */
	if (le_nonzero_conns) {

		u32 plen = sizeof(struct hci_ev_num_comp_pkts) +
				sizeof(struct hci_comp_pkts_info) *
				le_nonzero_conns;
		struct sk_buff *le_evt = bt_skb_alloc(HCI_EVENT_HDR_SIZE +
						      plen, GFP_KERNEL);
		if (le_evt) {	/* if this fails, you have bigger problems */

			struct hci_event_hdr *new_hdr = (struct hci_event_hdr*)skb_put(le_evt, HCI_EVENT_HDR_SIZE);
			struct hci_ev_num_comp_pkts *new_evt = (struct hci_ev_num_comp_pkts *)
				skb_put(le_evt, sizeof(struct hci_ev_num_comp_pkts) + sizeof(struct hci_comp_pkts_info) * le_nonzero_conns);

			hci_skb_pkt_type(le_evt) = HCI_EVENT_PKT;
			new_hdr->evt = HCI_EV_NUM_COMP_PKTS;

			new_evt->num_hndl = 0;
			for (i = 0; i < evt->num_hndl; i++) {

				pkts = __le16_to_cpu(evt->handles[i].count);
				hndl = __le16_to_cpu(evt->handles[i].handle);

				if (pkts && cid_is_le_conn(hndl)) {

					new_evt->handles[new_evt->num_hndl].
						handle = evt->handles[i].handle;
					new_evt->handles[new_evt->num_hndl].
						count = evt->handles[i].count;
					new_evt->num_hndl++;
				}
			}

			new_hdr->plen = plen;
			hci_le_splitter_enq_packet(le_evt);
		}
	}

	/* if we had le conns at all, shrink the packet to remove them */
	if (le_conns) {

		for (i = 0, j = 0; i < evt->num_hndl; i++) {
			hndl = __le16_to_cpu(evt->handles[i].handle);

			if (cid_is_le_conn(hndl))
				continue;
			evt->handles[j].handle = evt->handles[i].handle;
			evt->handles[j].count = evt->handles[i].count;
			j++;
		}

		/* record how many bytes we shed */
		*len_chng = (j - i) * sizeof(struct hci_comp_pkts_info);

		/* adjust counter */
		evt->num_hndl = j;
	}

	/* if any LE buffers got freed, signal user */
	if (le_pkts)
		wake_up_interruptible(&tx_has_room_wait_q);

    /* if any EDR packets left in the event, it is not ours to claim */
	if (evt->num_hndl)
        return false;

    /* but if no EDR handles left, we need to free the skb */
    

	return !evt->num_hndl;
}

/* called with lock held, return true to let bluez have the event. if we return false, WE must free packet */
static bool hci_le_splitter_should_allow_bluez_rx_evt(struct sk_buff *skb)
{
	struct hci_event_hdr *hdr = (void *) skb->data;
	void *evt_data = ((u8 *) skb->data) + HCI_EVENT_HDR_SIZE;
	struct hci_ev_le_meta *le_meta = evt_data;
	struct hci_ev_cmd_status *cmd_status = evt_data;
	struct hci_ev_cmd_complete *cmd_complete = evt_data;
	struct hci_ev_encrypt_change *encr_chg = evt_data;
	struct hci_ev_disconn_complete *disconn = evt_data;
	struct hci_ev_num_comp_pkts *num_comp_pkts = evt_data;
	struct hci_ev_key_refresh_complete *key_refr = evt_data;
	bool isours = false, enq_if_ours = true;
	int len_chng = 0;

	switch (hdr->evt) {
	case HCI_EV_DISCONN_COMPLETE:
		isours = hci_le_splitter_filter_disconn(disconn);
		break;
	case HCI_EV_ENCRYPT_CHANGE:
		isours = cid_is_le_conn(__le16_to_cpu(encr_chg->handle));
		break;
	case HCI_EV_CMD_COMPLETE:
		if (cmd_complete->ncmd)
			up(&cmd_sem);
		isours = hci_le_splitter_filter_cmd_complete(cmd_complete);
		break;
	case HCI_EV_CMD_STATUS:
		if (cmd_status->ncmd)
			up(&cmd_sem);
		isours = hci_le_splitter_filter_cmd_status(cmd_status);
		break;
	case HCI_EV_NUM_COMP_PKTS:
		isours = hci_le_splitter_filter_num_comp_pkt(num_comp_pkts,
							     &len_chng);
		enq_if_ours = false;
		break;
	case HCI_EV_KEY_REFRESH_COMPLETE:
		isours = cid_is_le_conn(__le16_to_cpu(key_refr->handle));
		break;
	case HCI_EV_LE_META:
		isours = hci_le_splitter_filter_le_meta(le_meta);
		break;
	case HCI_EV_VENDOR:
		/* always ours */
		isours = true;
		break;
	}

	skb->len += len_chng;

	if (isours && enq_if_ours)
		hci_le_splitter_enq_packet(skb);
	else if (isours) /* we still own it, so free it */
		kfree_skb(skb);

	return !isours;
}

/* return true to let bluez have the packet. if we return false, WE must free packet */
bool hci_le_splitter_should_allow_bluez_rx(struct hci_dev *hdev, struct sk_buff *skb)
{
	u16 acl_handle;
	bool ret = true;

	mutex_lock(&hci_state_lock);
	if (hci_le_splitter_is_our_dev(hdev)) {

		if (splitter_enable_state == SPLITTER_STATE_NOT_SET) {
			/* if state is not set, drop all packets */
			pr_warn("LE splitter not initialized - chip RX denied!\n");
			kfree_skb(skb);
			ret = false;
		} else if (splitter_enable_state == SPLITTER_STATE_DISABLED) {
			/* if disabled - allow all packets */
			ret = true;
		} else switch (hci_skb_pkt_type(skb)) {
		case HCI_EVENT_PKT:
			/* invalid (too small) packet? let bluez handle it */
			if (HCI_EVENT_HDR_SIZE > skb->len)
				break;

			ret = hci_le_splitter_should_allow_bluez_rx_evt(skb);
			break;

		case HCI_ACLDATA_PKT:
			/* data - could be ours. look into it */

			/* invalid (too small) packet? let bluez handle it */
			if (HCI_ACL_HDR_SIZE > skb->len)
				break;

			acl_handle = __le16_to_cpu(((struct hci_acl_hdr *)skb->data)->handle);
			acl_handle = hci_handle(acl_handle);

			if (cid_is_le_conn(acl_handle)) {
				hci_le_splitter_enq_packet(skb);
				ret = false;
			}
			break;

		case HCI_SCODATA_PKT:
			/* SCO data is never for LE */
			break;

		default:
			/* let weird things go to bluez */
			break;
		}
	}
	mutex_unlock(&hci_state_lock);

	return ret;
}

int hci_le_splitter_sysfs_init(void)
{
	int err;

	BT_INFO("Initializing LE splitter sysfs");
	skb_queue_head_init(&usr_msg_q);
	misc_register(&mdev);

	err = device_create_file(mdev.this_device, &sysfs_attr);
	if (err) {
		pr_err("Cannot create sysfs file (%d) - off by default\n", err);
		splitter_enable_state = SPLITTER_STATE_DISABLED;
		return -1;
	}
	return 0;
}

int hci_le_splitter_get_enabled_state(void)
{
	return splitter_enable_state;
}
