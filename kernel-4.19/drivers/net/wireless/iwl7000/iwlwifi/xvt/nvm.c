// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2012-2014 Intel Corporation
 * Copyright (C) 2015 Intel Deutschland GmbH
 */
#include <linux/firmware.h>

#include "iwl-trans.h"
#include "xvt.h"
#include "iwl-eeprom-parse.h"
#include "iwl-eeprom-read.h"
#include "iwl-nvm-parse.h"
#include "iwl-prph.h"
#include "fw-api.h"

/* Default NVM size to read */
#define IWL_NVM_DEFAULT_CHUNK_SIZE	(2*1024)
#define IWL_MAX_NVM_SECTION_SIZE	7000

#define NVM_WRITE_OPCODE 1
#define NVM_READ_OPCODE 0

enum wkp_nvm_offsets {
	/* NVM HW-Section offset (in words) definitions */
	HW_ADDR = 0x15,
};

enum ext_nvm_offsets {
	/* NVM HW-Section offset (in words) definitions */
	MAC_ADDRESS_OVERRIDE_EXT_NVM = 1,
};

/*
 * prepare the NVM host command w/ the pointers to the nvm buffer
 * and send it to fw
 */
static int iwl_nvm_write_chunk(struct iwl_xvt *xvt, u16 section,
			       u16 offset, u16 length, const u8 *data)
{
	struct iwl_nvm_access_cmd nvm_access_cmd = {
		.offset = cpu_to_le16(offset),
		.length = cpu_to_le16(length),
		.type = cpu_to_le16(section),
		.op_code = NVM_WRITE_OPCODE,
	};
	struct iwl_host_cmd cmd = {
		.id = NVM_ACCESS_CMD,
		.len = { sizeof(struct iwl_nvm_access_cmd), length },
		.flags = CMD_SEND_IN_RFKILL,
		.data = { &nvm_access_cmd, data },
		/* data may come from vmalloc, so use _DUP */
		.dataflags = { 0, IWL_HCMD_DFL_DUP },
	};

	return iwl_xvt_send_cmd(xvt, &cmd);
}

static int iwl_nvm_write_section(struct iwl_xvt *xvt, u16 section,
				 const u8 *data, u16 length)
{
	int offset = 0;

	/* copy data in chunks of 2k (and remainder if any) */

	while (offset < length) {
		int chunk_size, ret;

		chunk_size = min(IWL_NVM_DEFAULT_CHUNK_SIZE,
				 length - offset);

		ret = iwl_nvm_write_chunk(xvt, section, offset,
					  chunk_size, data + offset);
		if (ret < 0)
			return ret;

		offset += chunk_size;
	}

	return 0;
}

#define MAX_NVM_FILE_LEN	16384

/*
 * HOW TO CREATE THE NVM FILE FORMAT:
 * ------------------------------
 * 1. create hex file, format:
 *      3800 -> header
 *      0000 -> header
 *      5a40 -> data
 *
 *   rev - 6 bit (word1)
 *   len - 10 bit (word1)
 *   id - 4 bit (word2)
 *   rsv - 12 bit (word2)
 *
 * 2. flip 8bits with 8 bits per line to get the right NVM file format
 *
 * 3. create binary file from the hex file
 *
 * 4. save as "iNVM_xxx.bin" under /lib/firmware
 */
static int iwl_xvt_load_external_nvm(struct iwl_xvt *xvt)
{
	int ret, section_size;
	u16 section_id;
	const struct firmware *fw_entry;
	const struct {
		__le16 word1;
		__le16 word2;
		u8 data[];
	} *file_sec;
	const u8 *eof;
	const __le32 *dword_buff;
	const u8 *hw_addr;

#define NVM_WORD1_LEN(x) (8 * (x & 0x03FF))
#define NVM_WORD2_ID(x) (x >> 12)
#define EXT_NVM_WORD2_LEN(x) (2 * (((x) & 0xFF) << 8 | (x) >> 8))
#define EXT_NVM_WORD1_ID(x) ((x) >> 4)
#define NVM_HEADER_0	(0x2A504C54)
#define NVM_HEADER_1	(0x4E564D2A)
#define NVM_HEADER_SIZE	(4 * sizeof(u32))

	/*
	 * Obtain NVM image via request_firmware. Since we already used
	 * request_firmware_nowait() for the firmware binary load and only
	 * get here after that we assume the NVM request can be satisfied
	 * synchronously.
	 */
	ret = request_firmware(&fw_entry, iwlwifi_mod_params.nvm_file,
			       xvt->trans->dev);
	if (ret) {
		IWL_WARN(xvt, "WARNING: %s isn't available %d\n",
			 iwlwifi_mod_params.nvm_file, ret);
		return 0;
	}

	IWL_INFO(xvt, "Loaded NVM file %s (%zu bytes)\n",
		 iwlwifi_mod_params.nvm_file, fw_entry->size);

	if (fw_entry->size > MAX_NVM_FILE_LEN) {
		IWL_ERR(xvt, "NVM file too large\n");
		ret = -EINVAL;
		goto out;
	}

	eof = fw_entry->data + fw_entry->size;
	dword_buff = (__le32 *)fw_entry->data;

	/* some NVM file will contain a header.
	 * The header is identified by 2 dwords header as follows:
	 * dword[0] = 0x2A504C54
	 * dword[1] = 0x4E564D2A
	 *
	 * This header must be skipped when providing the NVM data to the FW.
	 */
	if (fw_entry->size > NVM_HEADER_SIZE &&
	    dword_buff[0] == cpu_to_le32(NVM_HEADER_0) &&
	    dword_buff[1] == cpu_to_le32(NVM_HEADER_1)) {
		file_sec = (void *)(fw_entry->data + NVM_HEADER_SIZE);
		IWL_INFO(xvt, "NVM Version %08X\n", le32_to_cpu(dword_buff[2]));
		IWL_INFO(xvt, "NVM Manufacturing date %08X\n",
			 le32_to_cpu(dword_buff[3]));
	} else {
		file_sec = (void *)fw_entry->data;
	}

	while (true) {
		if (file_sec->data > eof) {
			IWL_ERR(xvt,
				"ERROR - NVM file too short for section header\n");
			ret = -EINVAL;
			break;
		}

		/* check for EOF marker */
		if (!file_sec->word1 && !file_sec->word2) {
			ret = 0;
			break;
		}

		if (xvt->trans->cfg->nvm_type != IWL_NVM_EXT) {
			section_size =
				2 * NVM_WORD1_LEN(le16_to_cpu(file_sec->word1));
			section_id = NVM_WORD2_ID(le16_to_cpu(file_sec->word2));
		} else {
			section_size = 2 * EXT_NVM_WORD2_LEN(
						le16_to_cpu(file_sec->word2));
			section_id = EXT_NVM_WORD1_ID(
						le16_to_cpu(file_sec->word1));
		}

		if (section_size > IWL_MAX_NVM_SECTION_SIZE) {
			IWL_ERR(xvt, "ERROR - section too large (%d)\n",
				section_size);
			ret = -EINVAL;
			break;
		}

		if (!section_size) {
			IWL_ERR(xvt, "ERROR - section empty\n");
			ret = -EINVAL;
			break;
		}

		if (file_sec->data + section_size > eof) {
			IWL_ERR(xvt,
				"ERROR - NVM file too short for section (%d bytes)\n",
				section_size);
			ret = -EINVAL;
			break;
		}

		if (section_id == xvt->cfg->nvm_hw_section_num) {
			hw_addr = (const u8 *)((const __le16 *)file_sec->data +
						HW_ADDR);

			/* The byte order is little endian 16 bit, meaning 214365 */
			xvt->nvm_hw_addr[0] = hw_addr[1];
			xvt->nvm_hw_addr[1] = hw_addr[0];
			xvt->nvm_hw_addr[2] = hw_addr[3];
			xvt->nvm_hw_addr[3] = hw_addr[2];
			xvt->nvm_hw_addr[4] = hw_addr[5];
			xvt->nvm_hw_addr[5] = hw_addr[4];
		}
		if (section_id == NVM_SECTION_TYPE_MAC_OVERRIDE) {
			xvt->is_nvm_mac_override = true;
			hw_addr = (const u8 *)((const __le16 *)file_sec->data +
				   MAC_ADDRESS_OVERRIDE_EXT_NVM);

			/*
			 * Store the MAC address from MAO section.
			 * No byte swapping is required in MAO section.
			 */
			memcpy(xvt->nvm_hw_addr, hw_addr, ETH_ALEN);
		}

		ret = iwl_nvm_write_section(xvt, section_id, file_sec->data,
					    section_size);
		if (ret < 0) {
			IWL_ERR(xvt, "iwl_mvm_send_cmd failed: %d\n", ret);
			break;
		}

		/* advance to the next section */
		file_sec = (void *)(file_sec->data + section_size);
	}
out:
	release_firmware(fw_entry);
	return ret;
}

int iwl_xvt_nvm_init(struct iwl_xvt *xvt)
{
	int ret;

	xvt->is_nvm_mac_override = false;

	/* load external NVM if configured */
	if (iwlwifi_mod_params.nvm_file) {
		/* move to External NVM flow */
		ret = iwl_xvt_load_external_nvm(xvt);
		if (ret)
			return ret;
	}

	return 0;
}
