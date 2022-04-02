// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2014 Broadcom Corporation
 */
#ifndef BRCMFMAC_COMMON_H
#define BRCMFMAC_COMMON_H

#include <linux/platform_device.h>
#include <linux/platform_data/brcmfmac.h>
#include "fwil_types.h"

#define BRCMF_FW_ALTPATH_LEN			256
#define BRCMF_OTP_VERSION_MAX			64

extern char brcmf_firmware_path[BRCMF_FW_ALTPATH_LEN];
extern char brcmf_mac_addr[18];
extern char brcmf_otp_chip_id[BRCMF_OTP_VERSION_MAX];
extern char brcmf_otp_nvram_id[BRCMF_OTP_VERSION_MAX];

/**
 * struct brcmf_mp_device - Device module paramaters.
 *
 * @p2p_enable: Legacy P2P0 enable (old wpa_supplicant).
 * @feature_disable: Feature_disable bitmask.
 * @fcmode: FWS flow control.
 * @roamoff: Firmware roaming off?
 * @ignore_probe_fail: Ignore probe failure.
 * @country_codes: If available, pointer to struct for translating country codes
 * @bus: Bus specific platform data. Only SDIO at the mmoment.
 */
struct brcmf_mp_device {
	bool		p2p_enable;
	unsigned int	feature_disable;
	int		fcmode;
	bool		roamoff;
	bool		iapp;
	bool		ignore_probe_fail;
	struct brcmfmac_pd_cc *country_codes;
	const char	*board_type;
	union {
		struct brcmfmac_sdio_pd sdio;
	} bus;
};

void brcmf_c_set_joinpref_default(struct brcmf_if *ifp);

struct brcmf_mp_device *brcmf_get_module_param(struct device *dev,
					       enum brcmf_bus_type bus_type,
					       u32 chip, u32 chiprev);
void brcmf_release_module_param(struct brcmf_mp_device *module_param);

/* Sets dongle media info (drv_version, mac address). */
int brcmf_c_preinit_dcmds(struct brcmf_if *ifp);

#ifdef CONFIG_DMI
void brcmf_dmi_probe(struct brcmf_mp_device *settings, u32 chip, u32 chiprev);
#else
static inline void
brcmf_dmi_probe(struct brcmf_mp_device *settings, u32 chip, u32 chiprev) {}
#endif

u8 brcmf_map_prio_to_prec(void *cfg, u8 prio);

u8 brcmf_map_prio_to_aci(void *cfg, u8 prio);

#endif /* BRCMFMAC_COMMON_H */
