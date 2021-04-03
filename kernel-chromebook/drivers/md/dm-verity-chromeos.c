/*
 * Copyright (C) 2010 The Chromium OS Authors <chromium-os-dev@chromium.org>
 *                    All Rights Reserved.
 *
 * This file is released under the GPL.
 */
/*
 * Implements a Chrome OS platform specific error handler.
 * When verity detects an invalid block, this error handling will
 * attempt to corrupt the kernel boot image. On reboot, the bios will
 * detect the kernel corruption and switch to the alternate kernel
 * and root file system partitions.
 *
 * Assumptions:
 * 1. Partitions are specified on the command line using uuid.
 * 2. The kernel partition is the partition number is one less
 *    than the root partition number.
 */
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/chromeos_platform.h>
#include <linux/device.h>
#include <linux/device-mapper.h>
#include <linux/err.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/ubi.h>
#include <linux/notifier.h>
#include <linux/string.h>
#include <asm/page.h>

#include "dm-verity.h"

#define DM_MSG_PREFIX "verity-chromeos"
#define DMVERROR "DMVERROR"

static void chromeos_invalidate_kernel_endio(struct bio *bio)
{
	if (bio->bi_error) {
		DMERR("%s: bio operation failed (error=0x%x)", __func__,
		      bio->bi_error);
		chromeos_set_need_recovery();
	}

	complete(bio->bi_private);
}

static int chromeos_invalidate_kernel_submit(struct bio *bio,
					     struct block_device *bdev,
					     int rw, struct page *page)
{
	DECLARE_COMPLETION_ONSTACK(wait);

	bio->bi_private = &wait;
	bio->bi_end_io = chromeos_invalidate_kernel_endio;
	bio->bi_bdev = bdev;

	bio->bi_iter.bi_sector = 0;
	bio->bi_vcnt = 1;
	bio->bi_iter.bi_idx = 0;
	bio->bi_iter.bi_size = 512;
	bio->bi_iter.bi_bvec_done = 0;
	bio->bi_rw = rw;
	bio->bi_io_vec[0].bv_page = page;
	bio->bi_io_vec[0].bv_len = 512;
	bio->bi_io_vec[0].bv_offset = 0;

	submit_bio(rw, bio);
	/* Wait up to 2 seconds for completion or fail. */
	if (!wait_for_completion_timeout(&wait, msecs_to_jiffies(2000)))
		return -1;
	return 0;
}

static dev_t get_boot_dev_from_root_dev(struct block_device *root_bdev)
{
	/* Very basic sanity checking. This should be better. */
	if (!root_bdev || !root_bdev->bd_part ||
	    MAJOR(root_bdev->bd_dev) == 254 ||
	    root_bdev->bd_part->partno <= 1) {
		return 0;
	}
	return MKDEV(MAJOR(root_bdev->bd_dev), MINOR(root_bdev->bd_dev) - 1);
}

static char kern_guid[48];

/* get_boot_dev is bassed on dm_get_device_by_uuid in dm_bootcache. */
static dev_t get_boot_dev(void)
{
	const char partuuid[] = "PARTUUID=";
	char uuid[sizeof(partuuid) + 36];
	char *uuid_str;
	dev_t devt = 0;

	if (!strlen(kern_guid)) {
		DMERR("Couldn't get uuid, try root dev");
		return 0;
	}

	if (strncmp(kern_guid, partuuid, strlen(partuuid))) {
		/* Not prefixed with "PARTUUID=", so add it */
		strcpy(uuid, partuuid);
		strlcat(uuid, kern_guid, sizeof(uuid));
		uuid_str = uuid;
	} else {
		uuid_str = kern_guid;
	}
	devt = name_to_dev_t(uuid_str);
	if (!devt)
		goto found_nothing;
	return devt;

found_nothing:
	DMDEBUG("No matching partition for GUID: %s", uuid_str);
	return 0;
}

/* Replaces the first 8 bytes of a partition with DMVERROR */
static int chromeos_invalidate_kernel_bio(struct block_device *root_bdev)
{
	int ret = 0;
	struct block_device *bdev;
	struct bio *bio;
	struct page *page;
	dev_t devt;
	fmode_t dev_mode;
	/* Ensure we do synchronous unblocked I/O. We may also need
	 * sync_bdev() on completion, but it really shouldn't.
	 */
	int rw;

	devt = get_boot_dev();
	if (!devt) {
		devt = get_boot_dev_from_root_dev(root_bdev);
		if (!devt)
			return -EINVAL;
	}

	/* First we open the device for reading. */
	dev_mode = FMODE_READ | FMODE_EXCL;
	bdev = blkdev_get_by_dev(devt, dev_mode,
				 chromeos_invalidate_kernel_bio);
	if (IS_ERR(bdev)) {
		DMERR("invalidate_kernel: could not open device for reading");
		dev_mode = 0;
		ret = -1;
		goto failed_to_read;
	}

	bio = bio_alloc(GFP_NOIO, 1);
	if (!bio) {
		ret = -1;
		goto failed_bio_alloc;
	}

	page = alloc_page(GFP_NOIO);
	if (!page) {
		ret = -ENOMEM;
		goto failed_to_alloc_page;
	}

	/*
	 * Request read operation with REQ_FLUSH flag to ensure that the
	 * cache of non-volatile storage device has been flushed before read is
	 * started.
	 */
	rw = REQ_SYNC | REQ_NOIDLE | REQ_FLUSH;
	if (chromeos_invalidate_kernel_submit(bio, bdev, rw, page)) {
		ret = -1;
		goto failed_to_submit_read;
	}

	/* We have a page. Let's make sure it looks right. */
	if (memcmp("CHROMEOS", page_address(page), 8)) {
		DMERR("invalidate_kernel called on non-kernel partition");
		ret = -EINVAL;
		goto invalid_header;
	} else {
		DMERR("invalidate_kernel: found CHROMEOS kernel partition");
	}

	/* Stamp it and rewrite */
	memcpy(page_address(page), DMVERROR, strlen(DMVERROR));

	/* The block dev was being changed on read. Let's reopen here. */
	blkdev_put(bdev, dev_mode);
	dev_mode = FMODE_WRITE | FMODE_EXCL;
	bdev = blkdev_get_by_dev(devt, dev_mode,
				 chromeos_invalidate_kernel_bio);
	if (IS_ERR(bdev)) {
		DMERR("invalidate_kernel: could not open device for writing");
		dev_mode = 0;
		ret = -1;
		goto failed_to_write;
	}

	/* We re-use the same bio to do the write after the read. Need to reset
	 * it to initialize bio->bi_remaining.
	 */
	bio_reset(bio);

	/*
	 * Request write operation with REQ_FUA flag to ensure that I/O
	 * completion for the write is signaled only after the data has been
	 * committed to non-volatile storage.
	 */
	rw = REQ_WRITE | REQ_SYNC | REQ_NOIDLE | REQ_FUA;
	if (chromeos_invalidate_kernel_submit(bio, bdev, rw, page)) {
		ret = -1;
		goto failed_to_submit_write;
	}

	DMERR("invalidate_kernel: completed.");
	ret = 0;
failed_to_submit_write:
failed_to_write:
invalid_header:
	__free_page(page);
failed_to_submit_read:
	/* Technically, we'll leak a page with the pending bio, but
	 *  we're about to panic so it's safer to do the panic() we expect.
	 */
failed_to_alloc_page:
	bio_put(bio);
failed_bio_alloc:
	if (dev_mode)
		blkdev_put(bdev, dev_mode);
failed_to_read:
	return ret;
}

#ifdef CONFIG_MTD

struct erase_info_completion {
	struct erase_info instr;
	struct completion completion;
};

static void complete_erase(struct erase_info *instr)
{
	struct erase_info_completion *erase = container_of(
		instr, struct erase_info_completion, instr);
	complete(&erase->completion);
}

/* The maximum number of volumes per one UBI device, from ubi-media.h */
#define UBI_MAX_VOLUMES 128

static int chromeos_invalidate_kernel_nand(struct block_device *root_bdev)
{
	struct erase_info_completion erase;
	int ret;
	int partnum;
	struct mtd_info *dev;
	loff_t offset;
	size_t retlen;
	char *page = NULL;

	/* TODO(dehrenberg): replace translation of the ubiblock device
	 * number with using kern_guid= once mtd UUIDs are worked out. */
	partnum = MINOR(root_bdev->bd_dev) / UBI_MAX_VOLUMES - 1;
	DMDEBUG("Invalidating kernel on MTD partition %d", partnum);
	dev = get_mtd_device(NULL, partnum);
	if (IS_ERR(dev))
		return PTR_ERR(dev);
	/* Find the first good block to erase. Erasing a bad block might
	 * not cause a verification failure on the next boot. */
	for (offset = 0; offset < dev->size; offset += dev->erasesize) {
		if (!mtd_block_isbad(dev, offset))
			goto good;
	}
	/* No good blocks in the kernel; shouldn't happen, so to recovery */
	ret = -ERANGE;
	goto out;
good:
	/* Erase the first good block of the kernel. This will prevent
	 * that kernel from booting. */
	memset(&erase, 0, sizeof(erase));
	erase.instr.mtd = dev;
	erase.instr.addr = offset;
	erase.instr.len = dev->erasesize;
	erase.instr.callback = complete_erase;
	init_completion(&erase.completion);
	ret = mtd_erase(dev, &erase.instr);
	if (ret)
		goto out;
	wait_for_completion(&erase.completion);
	if (erase.instr.state == MTD_ERASE_FAILED) {
		ret = -EIO;
		goto out;
	}
	/* Write DMVERROR on the first page. If this fails, still return
	 * success since we will still be causing the kernel to not be
	 * selected, so no need to put the device in recovery mode. */
	page = kzalloc(dev->writesize, GFP_KERNEL);
	if (!page) {
		ret = 0;
		goto out;
	}
	memcpy(page, DMVERROR, strlen(DMVERROR));
	mtd_write(dev, offset, dev->writesize, &retlen, page);
	ret = 0;
	/* Ignore return value; no action is taken if dead */
out:
	kfree(page);
	put_mtd_device(dev);
	return ret;
}

#endif	/* CONFIG_MTD */

/*
 * Invalidate the kernel which corresponds to the root block device.
 *
 * This function stamps DMVERROR on the beginning of the kernel partition.
 * If the device is operating on raw NAND:
 *   Raw NAND mode is identified by checking if the underlying block device
 *    is an ubiblock device.
 *   The kernel partition is found by subtracting 1 from the mtd partition
 *    underlying the ubiblock device.
 *   The first erase block of the NAND is erased and "DMVERROR" is written
 *    in its place.
 * Otherwise, in the normal eMMC/SATA case:
 *   The kernel partition is attempted to be found by subtracting 1 from
 *    the root partition.
 *   If that fails, then the kernel_guid commandline parameter is used to
 *    find the kernel partition number.
 *   The DMVERROR string is stamped over only the CHROMEOS string at the
 *    beginning of the kernel blob, leaving the rest of it intact.
 */
static int chromeos_invalidate_kernel(struct block_device *root_bdev)
{
#ifdef CONFIG_MTD
	if (root_bdev && root_bdev->bd_disk) {
		char name[BDEVNAME_SIZE];
		disk_name(root_bdev->bd_disk, 0, name);
		if (strncmp(name, "ubiblock", strlen("ubiblock")) == 0)
			return chromeos_invalidate_kernel_nand(root_bdev);
	}
#endif

	return chromeos_invalidate_kernel_bio(root_bdev);
}

static int error_handler(struct notifier_block *nb, unsigned long transient,
			 void *opaque_err)
{
	struct dm_verity_error_state *err =
		(struct dm_verity_error_state *) opaque_err;
	err->behavior = DM_VERITY_ERROR_BEHAVIOR_PANIC;
	if (transient)
		return 0;

	/* TODO(wad) Implement phase 2:
	 * - Attempt to read the dev_status_offset from the hash dev.
	 * - If the status offset is 0, replace the first byte of the sector
	 *   with 01 and panic().
	 * - If the status offset is not 0, invalidate the associated kernel
	 *   partition, then reboot.
	 * - make user space tools clear the last sector
	 */
	if (chromeos_invalidate_kernel(err->dev))
		chromeos_set_need_recovery();
	return 0;
}

static struct notifier_block chromeos_nb = {
	.notifier_call = &error_handler,
	.next = NULL,
	.priority = 1,
};

static int __init dm_verity_chromeos_init(void)
{
	int r;

	r = dm_verity_register_error_notifier(&chromeos_nb);
	if (r < 0)
		DMERR("failed to register handler: %d", r);
	else
		DMINFO("dm-verity-chromeos registered");
	return r;
}

static void __exit dm_verity_chromeos_exit(void)
{
	dm_verity_unregister_error_notifier(&chromeos_nb);
}

module_init(dm_verity_chromeos_init);
module_exit(dm_verity_chromeos_exit);

MODULE_AUTHOR("Will Drewry <wad@chromium.org>");
MODULE_DESCRIPTION("chromeos-specific error handler for dm-verity");
MODULE_LICENSE("GPL");

/* Declare parameter with no module prefix */
#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX	""
module_param_string(kern_guid, kern_guid, sizeof(kern_guid), 0);
