#include "ithc.h"

// Userspace API:
// All data received on the active DMA RX channel is made available to userspace through character device /dev/ithc (/dev/ithc[01] if both channels are active).
// The chardev is read-only and has no ioctls. Multitouch mode is automatically activated when the device is opened and deactivated when it is closed.
// Reading from the chardev will obtain one or more messages containing data, or block if there is no new data.
// Only complete messages can be read; if the provided user buffer is too small, read() will return -EMSGSIZE instead of performing a partial read.
// Each message has a header (struct ithc_api_header) containing a sequential message number and the size of the data.

static loff_t ithc_api_llseek(struct file *f, loff_t offset, int whence) {
	struct ithc_api *a = container_of(f->private_data, struct ithc_api, m);
	return generic_file_llseek_size(f, offset, whence, MAX_LFS_FILESIZE, READ_ONCE(a->rx->pos));
}

static ssize_t ithc_api_read_unlocked(struct file *f, char __user *buf, size_t size, loff_t *offset) {
	struct ithc_api *a = container_of(f->private_data, struct ithc_api, m);
	loff_t pos = READ_ONCE(a->rx->pos);
	unsigned n = a->rx->num_received;
	unsigned newest = (n + NUM_RX_ALLOC - 1) % NUM_RX_ALLOC;
	unsigned oldest = (n + NUM_RX_DEV) % NUM_RX_ALLOC;
	unsigned i = newest;

	// scan backwards to find which buf we need to copy first
	while (1) {
		pos -= sizeof(struct ithc_api_header) + a->rx->bufs[i].data_size;
		if (pos <= *offset) break;
		if (i == oldest) break;
		i = i ? i-1 : NUM_RX_ALLOC-1;
		n--;
	}

	// copy as many bufs as possible
	size_t nread = 0;
	while (nread + sizeof(struct ithc_api_header) + a->rx->bufs[i].data_size <= size) {
		struct ithc_dma_data_buffer *b = &a->rx->bufs[i];
		if (b->active_idx >= 0) { pci_err(a->ithc->pci, "tried to access active buffer\n"); return -EFAULT; } // should be impossible
		struct ithc_api_header hdr = { .hdr_size = sizeof hdr, .msg_num = n, .size = b->data_size };
		if (copy_to_user(buf + nread, &hdr, sizeof hdr)) return -EFAULT;
		nread += sizeof hdr;

		if (copy_to_user(buf + nread, b->addr, b->data_size)) return -EFAULT;
		nread += b->data_size;

		if (i == newest) break;
		if (++i == NUM_RX_ALLOC) i = 0;
		n++;
	}
	if (!nread) return -EMSGSIZE; // user buffer is too small

	*offset = pos + nread;
	return nread;
}
static ssize_t ithc_api_read(struct file *f, char __user *buf, size_t size, loff_t *offset) {
	struct ithc_api *a = container_of(f->private_data, struct ithc_api, m);
	if (f->f_pos >= READ_ONCE(a->rx->pos)) {
		if (f->f_flags & O_NONBLOCK) return -EWOULDBLOCK;
		if (wait_event_interruptible(a->rx->wait, f->f_pos < READ_ONCE(a->rx->pos))) return -ERESTARTSYS;
	}
	// lock to avoid buffers getting transferred to device while we're reading them
	if (mutex_lock_interruptible(&a->rx->mutex)) return -ERESTARTSYS;
	int ret = ithc_api_read_unlocked(f, buf, size, offset);
	mutex_unlock(&a->rx->mutex);
	return ret;
}

static __poll_t ithc_api_poll(struct file *f, struct poll_table_struct *pt) {
	struct ithc_api *a = container_of(f->private_data, struct ithc_api, m);
	poll_wait(f, &a->rx->wait, pt);
	if (f->f_pos < READ_ONCE(a->rx->pos)) return POLLIN;
	return 0;
}

static int ithc_api_open(struct inode *n, struct file *f) {
	struct ithc_api *a = container_of(f->private_data, struct ithc_api, m);
	struct ithc *ithc = a->ithc;
	if (atomic_fetch_inc(&a->open_count) == 0) CHECK(ithc_set_multitouch, ithc, true);
	return 0;
}

static int ithc_api_release(struct inode *n, struct file *f) {
	struct ithc_api *a = container_of(f->private_data, struct ithc_api, m);
	struct ithc *ithc = a->ithc;
	int c = atomic_dec_return(&a->open_count);
	if (c == 0) CHECK(ithc_set_multitouch, ithc, false);
	if (c < 0) pci_err(ithc->pci, "open/release mismatch\n");
	return 0;
}

static const struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.llseek = ithc_api_llseek,
	.read = ithc_api_read,
	.poll = ithc_api_poll,
	.open = ithc_api_open,
	.release = ithc_api_release,
};

static void ithc_api_devres_release(struct device *dev, void *res) {
	struct ithc_api *a = res;
	misc_deregister(&a->m);
}

int ithc_api_init(struct ithc *ithc, struct ithc_dma_rx *rx, const char *name) {
	struct ithc_api *a = devres_alloc(ithc_api_devres_release, sizeof *a, GFP_KERNEL);
	if (!a) return -ENOMEM;
	a->ithc = ithc;
	a->rx = rx;
	a->m.minor = MISC_DYNAMIC_MINOR;
	a->m.name = name;
	a->m.fops = &dev_fops;
	a->m.mode = 0440;
	a->m.parent = &ithc->pci->dev;
	int r = CHECK(misc_register, &a->m);
	if (r) {
		devres_free(a);
		return r;
	}
	pci_info(ithc->pci, "registered device %s\n", name);
	devres_add(&ithc->pci->dev, a);
	rx->api = a;
	return 0;
}

