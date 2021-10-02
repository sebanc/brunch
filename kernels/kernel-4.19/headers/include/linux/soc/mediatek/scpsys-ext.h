/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SOC_MEDIATEK_SCPSYS_EXT_H
#define __SOC_MEDIATEK_SCPSYS_EXT_H

#include <linux/pm_domain.h>

#define MAX_STEPS	4

#define BUS_PROT(_type, _set_ofs, _clr_ofs,			\
		_en_ofs, _sta_ofs, _mask, _clr_ack_mask) {	\
		.type = _type,					\
		.set_ofs = _set_ofs,				\
		.clr_ofs = _clr_ofs,				\
		.en_ofs = _en_ofs,				\
		.sta_ofs = _sta_ofs,				\
		.mask = _mask,					\
		.clr_ack_mask = _clr_ack_mask,			\
	}

enum regmap_type {
	IFR_TYPE,
	SMI_TYPE,
	MAX_REGMAP_TYPE,
};

struct bus_prot {
	enum regmap_type type;
	u32 set_ofs;
	u32 clr_ofs;
	u32 en_ofs;
	u32 sta_ofs;
	u32 mask;
	u32 clr_ack_mask;
};

int mtk_scpsys_ext_set_bus_protection(const struct bus_prot *bp_table,
	struct regmap *infracfg, struct regmap *smi_common);
int mtk_scpsys_ext_clear_bus_protection(const struct bus_prot *bp_table,
	struct regmap *infracfg, struct regmap *smi_common);


enum pd_onoff_event {
	PD_ON_BEGIN,	/* begin of power on flow */
	PD_ON_MTCMOS,	/* power on in progress */
	PD_ON_FAIL,	/* power on failed */
	PD_OFF_BEGIN,	/* begin of power off flow */
	PD_OFF_MTCMOS,	/* power off in progress */
	PD_OFF_FAIL	/* power off failed */
};

typedef void (*pd_onoff_cb)(struct generic_pm_domain *genpd,
				enum pd_onoff_event id);

int mtk_scpsys_register_pd_onoff_cb(pd_onoff_cb cb);
int mtk_scpsys_unregister_pd_onoff_cb(pd_onoff_cb cb);

int mtk_scpsys_pd_bus_protect_enable(struct generic_pm_domain *genpd);
int mtk_scpsys_pd_bus_protect_disable(struct generic_pm_domain *genpd);

#endif /* __SOC_MEDIATEK_SCPSYS_EXT_H */
