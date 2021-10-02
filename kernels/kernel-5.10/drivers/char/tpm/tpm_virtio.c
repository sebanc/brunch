// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google Inc.
 *
 * Author: David Tolnay <dtolnay@gmail.com>
 *
 * ---
 *
 * Device driver for TPM over virtio.
 *
 * This driver employs a single virtio queue to handle both send and recv. TPM
 * commands are sent over virtio to the hypervisor during a TPM send operation
 * and responses are received over the same queue during a recv operation.
 *
 * The driver contains a single buffer that is the only buffer we ever place on
 * the virtio queue. Commands are copied from the caller's command buffer into
 * the driver's buffer before handing off to virtio, and responses are received
 * into the driver's buffer then copied into the caller's response buffer. This
 * allows us to be resilient to timeouts. When a send or recv operation times
 * out, the caller is free to destroy their buffer; we don't want the hypervisor
 * continuing to perform reads or writes against that destroyed buffer.
 *
 * This driver does not support concurrent send and recv operations. Mutual
 * exclusion is upheld by the tpm_mutex lock held in tpm-interface.c around the
 * calls to chip->ops->send and chip->ops->recv.
 *
 * The intended hypervisor-side implementation is as follows.
 *
 *     while true:
 *         await next virtio buffer.
 *         expect first descriptor in chain to be guest-to-host.
 *         read tpm command from that buffer.
 *         synchronously perform TPM work determined by the command.
 *         expect second descriptor in chain to be host-to-guest.
 *         write TPM response into that buffer.
 *         place buffer on virtio used queue indicating how many bytes written.
 */

#include <linux/virtio_config.h>

#include "tpm.h"

/*
 * Timeout duration when waiting on the hypervisor to complete its end of the
 * TPM operation. This timeout is relatively high because certain TPM operations
 * can take dozens of seconds.
 */
#define TPM_VIRTIO_TIMEOUT (120 * HZ)

struct vtpm_device {
	/*
	 * Data structure for integration with the common code of the TPM driver
	 * in tpm-chip.c.
	 */
	struct tpm_chip *chip;

	/*
	 * Virtio queue for sending TPM commands out of the virtual machine and
	 * receiving TPM responses back from the hypervisor.
	 */
	struct virtqueue *vq;

	/*
	 * Completion that is notified when a virtio operation has been
	 * fulfilled by the hypervisor.
	 */
	struct completion complete;

	/*
	 * Whether driver currently holds ownership of the virtqueue buffer.
	 * When false, the hypervisor is in the process of reading or writing
	 * the buffer and the driver must not touch it.
	 */
	bool driver_has_buffer;

	/*
	 * Whether during the most recent TPM operation, a virtqueue_kick failed
	 * or a wait timed out.
	 *
	 * The next send or recv operation will attempt a kick upon seeing this
	 * status. That should clear up the queue in the case that the
	 * hypervisor became temporarily nonresponsive, such as by resource
	 * exhaustion on the host. The extra kick enables recovery from kicks
	 * going unnoticed by the hypervisor as well as recovery from virtio
	 * callbacks going unnoticed by the guest kernel.
	 */
	bool needs_kick;

	/* Number of bytes available to read from the virtqueue buffer. */
	unsigned int readable;

	/*
	 * Buffer in which all virtio transfers take place. Buffer size is the
	 * maximum legal TPM command or response message size.
	 */
	u8 virtqueue_buffer[TPM_BUFSIZE];
};

/*
 * Wait for ownership of the virtqueue buffer.
 *
 * The why-string should begin with "waiting to..." or "waiting for..." with no
 * trailing newline. It will appear in log output.
 *
 * Returns zero for success, otherwise negative error.
 */
static int vtpm_wait_for_buffer(struct vtpm_device *dev, const char *why)
{
	int ret;
	struct tpm_chip *chip = dev->chip;
	unsigned long deadline = jiffies + TPM_VIRTIO_TIMEOUT;

	/* Kick queue if needed. */
	if (dev->needs_kick) {
		bool did_kick = virtqueue_kick(dev->vq);
		if (!did_kick) {
			dev_notice(&chip->dev, "kick failed; will retry\n");
			return -EBUSY;
		}
		dev->needs_kick = false;
	}

	while (!dev->driver_has_buffer) {
		unsigned long now = jiffies;

		/* Check timeout, otherwise `deadline - now` may underflow. */
		if time_after_eq(now, deadline) {
			dev_warn(&chip->dev, "timed out %s\n", why);
			dev->needs_kick = true;
			return -ETIMEDOUT;
		}

		/*
		 * Wait to be signaled by virtio callback.
		 *
		 * Positive ret is jiffies remaining until timeout when the
		 * completion occurred, which means successful completion. Zero
		 * ret is timeout. Negative ret is error.
		 */
		ret = wait_for_completion_killable_timeout(
				&dev->complete, deadline - now);

		/* Log if completion did not occur. */
		if (ret == -ERESTARTSYS) {
			/* Not a warning if it was simply interrupted. */
			dev_dbg(&chip->dev, "interrupted %s\n", why);
		} else if (ret == 0) {
			dev_warn(&chip->dev, "timed out %s\n", why);
			ret = -ETIMEDOUT;
		} else if (ret < 0) {
			dev_warn(&chip->dev, "failed while %s: error %d\n",
					why, -ret);
		}

		/*
		 * Return error if completion did not occur. Schedule kick to be
		 * retried at the start of the next send/recv to help unblock
		 * the queue.
		 */
		if (ret < 0) {
			dev->needs_kick = true;
			return ret;
		}

		/* Completion occurred. Expect response buffer back. */
		if (virtqueue_get_buf(dev->vq, &dev->readable)) {
			dev->driver_has_buffer = true;

			if (dev->readable > TPM_BUFSIZE) {
				dev_crit(&chip->dev,
						"hypervisor bug: response exceeds max size,"
						" %u > %u\n",
						dev->readable,
						(unsigned int) TPM_BUFSIZE);
				dev->readable = TPM_BUFSIZE;
				return -EPROTO;
			}
		}
	}

	return 0;
}

static int vtpm_op_send(struct tpm_chip *chip, u8 *caller_buf, size_t len)
{
	int ret;
	bool did_kick;
	struct scatterlist sg_outbuf, sg_inbuf;
	struct scatterlist *sgs[2] = { &sg_outbuf, &sg_inbuf };
	struct vtpm_device *dev = dev_get_drvdata(&chip->dev);
	u8 *virtqueue_buf = dev->virtqueue_buffer;

	dev_dbg(&chip->dev, "vtpm_op_send %zu bytes\n", len);

	if (len > TPM_BUFSIZE) {
		dev_err(&chip->dev,
				"command is too long, %zu > %zu\n",
				len, (size_t) TPM_BUFSIZE);
		return -EINVAL;
	}

	/*
	 * Wait until hypervisor relinquishes ownership of the virtqueue buffer.
	 *
	 * This may block if the previous recv operation timed out in the guest
	 * kernel but is still being processed by the hypervisor. Also may block
	 * if send operations are performed back-to-back, such as if something
	 * in the caller failed in between a send and recv.
	 *
	 * During normal operation absent of any errors or timeouts, this does
	 * not block.
	 */
	ret = vtpm_wait_for_buffer(dev, "waiting to begin send");
	if (ret) {
		return ret;
	}

	/* Driver owns virtqueue buffer and may now write into it. */
	memcpy(virtqueue_buf, caller_buf, len);

	/*
	 * Enqueue the virtqueue buffer once as outgoing virtio data (written by
	 * the virtual machine and read by the hypervisor) and again as incoming
	 * data (written by the hypervisor and read by the virtual machine).
	 * This step moves ownership of the virtqueue buffer from driver to
	 * hypervisor.
	 *
	 * Note that we don't know here how big of a buffer the caller will use
	 * with their later call to recv. We allow the hypervisor to write up to
	 * the TPM max message size. If the caller ends up using a smaller
	 * buffer with recv that is too small to hold the entire response, the
	 * recv will return an error. This conceivably breaks TPM
	 * implementations that want to produce a different verbosity of
	 * response depending on the receiver's buffer size.
	 */
	sg_init_one(&sg_outbuf, virtqueue_buf, len);
	sg_init_one(&sg_inbuf, virtqueue_buf, TPM_BUFSIZE);
	ret = virtqueue_add_sgs(dev->vq, sgs, 1, 1, virtqueue_buf, GFP_KERNEL);
	if (ret) {
		dev_err(&chip->dev, "failed virtqueue_add_sgs\n");
		return ret;
	}

	/* Kick the other end of the virtqueue after having added a buffer. */
	did_kick = virtqueue_kick(dev->vq);
	if (!did_kick) {
		dev->needs_kick = true;
		dev_notice(&chip->dev, "kick failed; will retry\n");

		/*
		 * We return 0 anyway because what the caller doesn't know can't
		 * hurt them. They can call recv and it will retry the kick. If
		 * that works, everything is fine.
		 *
		 * If the retry in recv fails too, they will get -EBUSY from
		 * recv.
		 */
	}

	/*
	 * Hypervisor is now processing the TPM command asynchronously. It will
	 * read the command from the output buffer and write the response into
	 * the input buffer (which are the same buffer). When done, it will send
	 * back the buffers over virtio and the driver's virtio callback will
	 * complete dev->complete so that we know the response is ready to be
	 * read.
	 *
	 * It is important to have copied data out of the caller's buffer into
	 * the driver's virtqueue buffer because the caller is free to destroy
	 * their buffer when this call returns. We can't avoid copying by
	 * waiting here for the hypervisor to finish reading, because if that
	 * wait times out, we return and the caller may destroy their buffer
	 * while the hypervisor is continuing to read from it.
	 */
	dev->driver_has_buffer = false;
	return 0;
}

static int vtpm_op_recv(struct tpm_chip *chip, u8 *caller_buf, size_t len)
{
	int ret;
	struct vtpm_device *dev = dev_get_drvdata(&chip->dev);
	u8 *virtqueue_buf = dev->virtqueue_buffer;

	dev_dbg(&chip->dev, "vtpm_op_recv\n");

	/*
	 * Wait until the virtqueue buffer is owned by the driver.
	 *
	 * This will usually block while the hypervisor finishes processing the
	 * most recent TPM command.
	 */
	ret = vtpm_wait_for_buffer(dev, "waiting for TPM response");
	if (ret) {
		return ret;
	}

	dev_dbg(&chip->dev, "received %u bytes\n", dev->readable);

	if (dev->readable > len) {
		dev_notice(&chip->dev,
				"TPM response is bigger than receiver's buffer:"
				" %u > %zu\n",
				dev->readable, len);
		return -EINVAL;
	}

	/* Copy response over to the caller. */
	memcpy(caller_buf, virtqueue_buf, dev->readable);

	return dev->readable;
}

static void vtpm_op_cancel(struct tpm_chip *chip)
{
	/*
	 * Cancel is not called in this driver's use of tpm-interface.c. It may
	 * be triggered through tpm-sysfs but that can be implemented as needed.
	 * Be aware that tpm-sysfs performs cancellation without holding the
	 * tpm_mutex that protects our send and recv operations, so a future
	 * implementation will need to consider thread safety of concurrent
	 * send/recv and cancel.
	 */
	dev_notice(&chip->dev, "cancellation is not implemented\n");
}

static u8 vtpm_op_status(struct tpm_chip *chip)
{
	/*
	 * Status is for TPM drivers that want tpm-interface.c to poll for
	 * completion before calling recv. Usually this is when the hardware
	 * needs to be polled i.e. there is no other way for recv to block on
	 * the TPM command completion.
	 *
	 * Polling goes until `(status & complete_mask) == complete_val`. This
	 * driver defines both complete_mask and complete_val as 0 and blocks on
	 * our own completion object in recv instead.
	 */
	return 0;
}

static const struct tpm_class_ops vtpm_ops = {
	.flags = TPM_OPS_AUTO_STARTUP,
	.send = vtpm_op_send,
	.recv = vtpm_op_recv,
	.cancel = vtpm_op_cancel,
	.status = vtpm_op_status,
	.req_complete_mask = 0,
	.req_complete_val = 0,
};

static void vtpm_virtio_complete(struct virtqueue *vq)
{
	struct virtio_device *vdev = vq->vdev;
	struct vtpm_device *dev = vdev->priv;

	complete(&dev->complete);
}

static int vtpm_probe(struct virtio_device *vdev)
{
	int err;
	struct vtpm_device *dev;
	struct virtqueue *vq;
	struct tpm_chip *chip;

	dev_dbg(&vdev->dev, "vtpm_probe\n");

	dev = kzalloc(sizeof(struct vtpm_device), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		dev_err(&vdev->dev, "failed kzalloc\n");
		goto err_dev_alloc;
	}
	vdev->priv = dev;

	vq = virtio_find_single_vq(vdev, vtpm_virtio_complete, "vtpm");
	if (IS_ERR(vq)) {
		err = PTR_ERR(vq);
		dev_err(&vdev->dev, "failed virtio_find_single_vq\n");
		goto err_virtio_find;
	}
	dev->vq = vq;

	chip = tpm_chip_alloc(&vdev->dev, &vtpm_ops);
	if (IS_ERR(chip)) {
		err = PTR_ERR(chip);
		dev_err(&vdev->dev, "failed tpm_chip_alloc\n");
		goto err_chip_alloc;
	}
	dev_set_drvdata(&chip->dev, dev);
	chip->flags |= TPM_CHIP_FLAG_TPM2;
	dev->chip = chip;

	init_completion(&dev->complete);
	dev->driver_has_buffer = true;
	dev->needs_kick = false;
	dev->readable = 0;

	/*
	 * Required in order to enable vq use in probe function for auto
	 * startup.
	 */
	virtio_device_ready(vdev);

	err = tpm_chip_register(dev->chip);
	if (err) {
		dev_err(&vdev->dev, "failed tpm_chip_register\n");
		goto err_chip_register;
	}

	return 0;

err_chip_register:
	put_device(&dev->chip->dev);
err_chip_alloc:
	vdev->config->del_vqs(vdev);
err_virtio_find:
	kfree(dev);
err_dev_alloc:
	return err;
}

static void vtpm_remove(struct virtio_device *vdev)
{
	struct vtpm_device *dev = vdev->priv;

	/* Undo tpm_chip_register. */
	tpm_chip_unregister(dev->chip);

	/* Undo tpm_chip_alloc. */
	put_device(&dev->chip->dev);

	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);

	kfree(dev);
}

#define VIRTIO_ID_TPM 31

static struct virtio_device_id id_table[] = {
	{
		.device = VIRTIO_ID_TPM,
		.vendor = VIRTIO_DEV_ANY_ID,
	},
	{},
};

static struct virtio_driver vtpm_driver = {
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = vtpm_probe,
	.remove = vtpm_remove,
};

module_virtio_driver(vtpm_driver);

MODULE_AUTHOR("David Tolnay (dtolnay@gmail.com)");
MODULE_DESCRIPTION("Virtio vTPM Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
