/*
 * *  ffu.c
 *
 *  Copyright 2007-2008 Pierre Ossman
 *
 *  Modified by SanDisk Corp.
 *  Modified by Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/mmc/card.h>
#include <linux/mmc/ffu.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/core.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>

/**
 * struct mmc_ffu_pages - pages allocated by 'alloc_pages()'.
 * @page: first page in the allocation
 * @order: order of the number of pages allocated
 */
struct mmc_ffu_pages {
	struct page *page;
	unsigned int order;
};

/**
 * struct mmc_ffu_mem - allocated memory.
 * @arr: array of allocations
 * @cnt: number of allocations
 */
struct mmc_ffu_mem {
	struct mmc_ffu_pages *arr;
	unsigned int cnt;
};

struct mmc_ffu_area {
	unsigned long max_sz;
	unsigned int max_tfr;
	unsigned int max_segs;
	unsigned int max_seg_sz;
	unsigned int blocks;
	unsigned int sg_len;
	struct mmc_ffu_mem mem;
	struct sg_table sgtable;
};

/*
 * Get hack value
 */
static const struct mmc_ffu_hack *mmc_get_hack(
		const struct mmc_ffu_args *args,
		enum mmc_ffu_hack_type type)
{
	int i;

	for (i = 0; i < args->ack_nb; i++) {
		if (args->hack[i].type == type)
			return &args->hack[i];
	}
	return NULL;
}

/*
 * Map memory into a scatterlist.
 */
static unsigned int mmc_ffu_map_sg(struct mmc_ffu_mem *mem, int size,
				   struct scatterlist *sglist)
{
	struct scatterlist *sg = sglist;
	unsigned int i;
	unsigned long sz = size;
	unsigned int sctr_len = 0;
	unsigned long len;

	for (i = 0; i < mem->cnt && sz; i++, sz -= len) {
		len = PAGE_SIZE << mem->arr[i].order;

		if (len > sz) {
			len = sz;
			sz = 0;
		}

		sg_set_page(sg, mem->arr[i].page, len, 0);
		sg = sg_next(sg);
		sctr_len++;
	}

	return sctr_len;
}

static void mmc_ffu_free_mem(struct mmc_ffu_mem *mem)
{
	if (!mem)
		return;

	while (mem->cnt--)
		__free_pages(mem->arr[mem->cnt].page, mem->arr[mem->cnt].order);

	kfree(mem->arr);
}

/*
 * Cleanup struct mmc_ffu_area.
 */
static void mmc_ffu_area_cleanup(struct mmc_ffu_area *area)
{
	sg_free_table(&area->sgtable);
	mmc_ffu_free_mem(&area->mem);
}

/*
 * Allocate a lot of memory, preferably max_sz but at least min_sz. In case
 * there isn't much memory do not exceed 1/16th total low mem* pages.
 * Also do not exceed a maximum number of segments and try not to make
 * segments much bigger than maximum segment size.
 */
static int mmc_ffu_alloc_mem(struct mmc_ffu_area *area, unsigned long min_sz)
{
	unsigned long max_page_cnt = DIV_ROUND_UP(area->max_tfr, PAGE_SIZE);
	unsigned long min_page_cnt = DIV_ROUND_UP(min_sz, PAGE_SIZE);
	unsigned long max_seg_page_cnt = DIV_ROUND_UP(area->max_seg_sz,
						      PAGE_SIZE);
	unsigned long page_cnt = 0;
	/*
	 * We divide by 16 to ensure we will not allocate a big amount
	 * of unnecessary pages.
	 */
	unsigned long limit = nr_free_buffer_pages() >> 4;

	gfp_t flags = GFP_KERNEL | GFP_DMA | __GFP_NOWARN | __GFP_NORETRY;

	if (max_page_cnt > limit)
		max_page_cnt = limit;

	if (min_page_cnt > max_page_cnt)
		min_page_cnt = max_page_cnt;

	if (area->max_segs * max_seg_page_cnt > max_page_cnt)
		area->max_segs = DIV_ROUND_UP(max_page_cnt, max_seg_page_cnt);

	area->mem.arr = kcalloc(area->max_segs,
				sizeof(*area->mem.arr),
				GFP_KERNEL);
	if (!area->mem.arr)
		return -ENOMEM;
	area->mem.cnt = 0;

	while (max_page_cnt) {
		struct page *page;
		unsigned int order;

		order = get_order(max_seg_page_cnt << PAGE_SHIFT);

		do {
			page = alloc_pages(flags, order);
		} while (!page && order--);

		if (!page)
			goto out_free;

		area->mem.arr[area->mem.cnt].page = page;
		area->mem.arr[area->mem.cnt].order = order;
		area->mem.cnt++;
		page_cnt += 1UL << order;
		if (max_page_cnt <= (1UL << order))
			break;
		max_page_cnt -= 1UL << order;
	}

	if (page_cnt < min_page_cnt)
		goto out_free;

	return 0;

out_free:
	mmc_ffu_free_mem(&area->mem);
	return -ENOMEM;
}

/*
 * Initialize an area for data transfers.
 * Copy the data to the allocated pages.
 */
static int mmc_ffu_area_init(struct mmc_ffu_area *area, struct mmc_card *card,
			     const u8 *data, int size)
{
	int ret;
	int i;
	int length = 0, page_length;
	int min_size = 0;

	area->max_tfr = size;

	ret = mmc_ffu_alloc_mem(area, 1);
	for (i = 0; i < area->mem.cnt; i++) {
		if (length > size) {
			ret = -EINVAL;
			goto out_free;
		}
		page_length = PAGE_SIZE << area->mem.arr[i].order;
		min_size = min(size - length, page_length);
		memcpy(page_address(area->mem.arr[i].page), data + length,
		       min(size - length, page_length));
		length += page_length;
	}

	ret = sg_alloc_table(&area->sgtable, area->mem.cnt, GFP_KERNEL);
	if (ret)
		goto out_free;

	area->sg_len = mmc_ffu_map_sg(&area->mem, size, area->sgtable.sgl);

	return 0;

out_free:
	mmc_ffu_free_mem(&area->mem);
	return ret;
}

static int mmc_ffu_write(struct mmc_card *card, const u8 *src, u32 arg,
			 int size)
{
	int rc;
	struct mmc_ffu_area area = {0};
	int max_tfr;

	area.max_segs = card->host->max_segs;
	area.max_seg_sz = card->host->max_seg_size;

	do {
		max_tfr = size;
		if ((max_tfr >> 9) > card->host->max_blk_count)
			max_tfr = card->host->max_blk_count << 9;
		if (max_tfr > card->host->max_req_size)
			max_tfr = card->host->max_req_size;
		if (DIV_ROUND_UP(max_tfr, area.max_seg_sz) > area.max_segs)
			max_tfr = area.max_segs * area.max_seg_sz;

		rc = mmc_ffu_area_init(&area, card, src, max_tfr);
		if (rc != 0)
			return rc;

		rc = mmc_simple_transfer(card, area.sgtable.sgl, area.sg_len,
					 arg, max_tfr >> 9, 512, 1);
		mmc_ffu_area_cleanup(&area);
		if (rc != 0) {
			dev_err(mmc_dev(card->host),
				"%s mmc_ffu_simple_transfer %d\n",
				__func__, rc);
			return rc;
		}
		src += max_tfr;
		size -= max_tfr;

	} while (size > 0);

	return rc;
}

static int mmc_ffu_install(struct mmc_card *card)
{
	int err;

	/* check mode operation */
	if (!card->ext_csd.ffu_mode_op) {
		/*
		 * Host switch back to work in normal MMC Read/Write commands.
		 * For Hynix, be sure the command set is 0.
		 */
		err = mmc_switch(card, 0, EXT_CSD_MODE_CONFIG, 0,
				card->ext_csd.generic_cmd6_time);
		if (err) {
			dev_err(mmc_dev(card->host),
				"FFU: %s: switch to normal mode error %d:\n",
				mmc_hostname(card->host), err);
			return err;
		}

		/* restart the eMMC */
		err = mmc_hw_reset(card->host);
		if (err) {
			dev_err(mmc_dev(card->host),
				"FFU: %s: install error %d:\n",
				mmc_hostname(card->host), err);
			return err;
		}
	} else {
		/* set ext_csd to install mode */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_MODE_OPERATION_CODES, 0x1,
				 card->ext_csd.mode_op_codes_time);
		if (err) {
			dev_err(mmc_dev(card->host),
				"FFU: %s: error %d setting install mode\n",
				mmc_hostname(card->host), err);
			return err;
		}
	}

	return 0;
}

int mmc_ffu_invoke(struct mmc_card *card, const struct mmc_ffu_args *args)
{
	u8 *ext_csd = NULL;
	int err;
	u32 arg;
	u32 fw_prog_bytes;
	const struct firmware *fw;
	const struct mmc_ffu_hack *hack;


	/* Check if FFU is supported */
	if (!card->ext_csd.ffu_capable) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: error FFU is not supported %d rev %d\n",
			mmc_hostname(card->host), card->ext_csd.ffu_capable,
			card->ext_csd.rev);
		return -EOPNOTSUPP;
	}

	if (strlen(args->name) > 512) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: name %.20s is too long.\n",
			mmc_hostname(card->host), args->name);
		return -EINVAL;
	}

	/* setup FW data buffer */
	err = request_firmware(&fw, args->name, &card->dev);
	if (err) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: Firmware request failed %d\n",
			mmc_hostname(card->host), err);
		return err;
	}
	if (fw->size % 512) {
		dev_warn(mmc_dev(card->host),
			 "FFU: %s: Warning %zd firmware data size unaligned!\n",
			 mmc_hostname(card->host),
			 fw->size);
	}

	mmc_get_card(card);

	/* trigger flushing*/
	err = mmc_flush_cache(card);
	if (err) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: error %d flushing data\n",
			mmc_hostname(card->host), err);
		goto exit_set_normal;
	}

	/* set CMD ARG */
	hack = mmc_get_hack(args, MMC_OVERRIDE_FFU_ARG);
	if (hack == NULL) {
		arg = card->ext_csd.ffu_arg;
	} else {
		arg = cpu_to_le32((u32)hack->value);
	}

	/* set device to FFU mode */
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_MODE_CONFIG, 0x1,
			 card->ext_csd.generic_cmd6_time);
	if (err) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: error %d FFU is not supported\n",
			mmc_hostname(card->host), err);
		goto exit_set_normal;
	}

	err = mmc_ffu_write(card, fw->data, arg, fw->size);
	if (err) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: write error %d\n",
			mmc_hostname(card->host), err);
		goto exit_set_normal;
	}
	/* payload  will be checked only in op_mode supported */
	if (card->ext_csd.ffu_mode_op) {
		err = mmc_get_ext_csd(card, &ext_csd);
		if (err) {
			dev_err(mmc_dev(card->host),
				"FFU: %s: error %d sending ext_csd\n",
				mmc_hostname(card->host), err);
			goto exit_set_normal;
		}

		/* check that the eMMC has received the payload */
		fw_prog_bytes = ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG] |
			ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 1] << 8 |
			ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 2] << 16 |
			ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 3] << 24;
		kfree(ext_csd);
		ext_csd = NULL;

		fw_prog_bytes *= card->ext_csd.data_sector_size;
		if (fw_prog_bytes != fw->size) {
			err = -EINVAL;
			dev_err(mmc_dev(card->host),
				"FFU: %s: error %d: incorrect programmation\n",
				__func__, err);
			dev_err(mmc_dev(card->host),
				"FFU: sectors written: %d, expected %zd\n",
				fw_prog_bytes, fw->size);
			goto exit_set_normal;
		}
	}

	err = mmc_ffu_install(card);
	if (err) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: error firmware install %d\n",
			mmc_hostname(card->host), err);
		goto exit_set_normal;
	}

	/* read ext_csd */
	err = mmc_get_ext_csd(card, &ext_csd);
	if (err) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* return status */
	err = ext_csd[EXT_CSD_FFU_STATUS];
	if (err) {
		dev_err(mmc_dev(card->host),
			"FFU: %s: FFU status 0x%02x, expected 0\n",
			mmc_hostname(card->host), err);
		err = -EINVAL;
		goto exit;
	}

exit_set_normal:
	if (err != 0) {
		/*
		 * Host switch back to work in normal MMC
		 * Read/Write commands.
		 */
		mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_MODE_CONFIG, 0,
			   card->ext_csd.generic_cmd6_time);
	}

exit:
	kfree(ext_csd);
	release_firmware(fw);
	mmc_put_card(card);

	return err;
}
EXPORT_SYMBOL(mmc_ffu_invoke);
