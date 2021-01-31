// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2018-2019  Realtek Corporation
 */

#include <linux/acpi.h>
#include "main.h"
#include "debug.h"
#include "phy.h"
#include "sar.h"

#define SAR_WRDS_CHAIN_NR	2

enum sar_limit_index {
	LMT_CH1_14,
	LMT_CH36_64,
	LMT_UND1,
	LMT_CH100_144,
	LMT_CH149_165,

	LMT_TOTAL_NR,
};

struct sar_limits {
	s8 limit[LMT_TOTAL_NR];
};

struct sar_wrds {
	struct sar_limits chain[SAR_WRDS_CHAIN_NR];
};

#define ACPI_WRDS_METHOD	"WRDS"
#define ACPI_WRDS_SIZE		sizeof(struct sar_wrds)
#define ACPI_WRDS_TOTAL_SIZE	(sizeof(struct sar_wrds) + 2)
#define ACPI_WIFI_DOMAIN	0x07

#ifdef CONFIG_ACPI
static union acpi_object *rtw_sar_get_acpiobj(struct rtw_dev *rtwdev,
					      const char *method)
{
	struct device *dev = rtwdev->dev;
	acpi_handle root_handle;
	acpi_handle handle;
	acpi_status status;
	struct acpi_buffer buf = {ACPI_ALLOCATE_BUFFER, NULL};

	/* Check device handler */
	root_handle = ACPI_HANDLE(dev);
	if (!root_handle) {
		rtw_dbg(rtwdev, RTW_DBG_REGD,
			"SAR: Could not retireve root port ACPI handle\n");
		return NULL;
	}

	/* Get method's handler */
	status = acpi_get_handle(root_handle, (acpi_string)method, &handle);
	if (ACPI_FAILURE(status)) {
		rtw_dbg(rtwdev, RTW_DBG_REGD, "SAR: %s method not found\n",
			method);
		return NULL;
	}

	/* Call specific method with no argument */
	status = acpi_evaluate_object(handle, NULL, NULL, &buf);
	if (ACPI_FAILURE(status)) {
		rtw_dbg(rtwdev, RTW_DBG_REGD,
			"SAR: %s invocation failed (0x%x)\n", method, status);
		return NULL;
	}

	return buf.pointer;
}

static
union acpi_object *rtw_sar_get_wifi_pkt(struct rtw_dev *rtwdev,
					union acpi_object *obj,
					u32 element_count)
{
	union acpi_object *wifi_pkg;
	u32 i;

	if (obj->type != ACPI_TYPE_PACKAGE ||
	    obj->package.count < 2 ||
	    obj->package.elements[0].type != ACPI_TYPE_INTEGER ||
	    obj->package.elements[0].integer.value != 0) {
		rtw_dbg(rtwdev, RTW_DBG_REGD,
			"SAR: Unsupported wifi package structure\n");
		return NULL;
	}

	/* loop through all the packages to find the one for WiFi */
	for (i = 1; i < obj->package.count; i++) {
		union acpi_object *domain;

		wifi_pkg = &obj->package.elements[i];

		/* Skip anything that is not a package with the right
		 * amount of elements (i.e. domain_type,
		 * enabled/disabled plus the sar table size.
		 */
		if (wifi_pkg->type != ACPI_TYPE_PACKAGE ||
		    wifi_pkg->package.count != element_count)
			continue;

		domain = &wifi_pkg->package.elements[0];
		if (domain->type == ACPI_TYPE_INTEGER &&
		    domain->integer.value == ACPI_WIFI_DOMAIN)
			return wifi_pkg;
	}

	return NULL;
}

static void *rtw_sar_get_wrds_table(struct rtw_dev *rtwdev)
{
	union acpi_object *wrds, *wrds_pkg;
	int i, idx = 2;
	u8 *wrds_raw = NULL;

	wrds = rtw_sar_get_acpiobj(rtwdev, ACPI_WRDS_METHOD);
	if (!wrds)
		return false;

	wrds_pkg = rtw_sar_get_wifi_pkt(rtwdev, wrds, ACPI_WRDS_TOTAL_SIZE);
	if (!wrds_pkg)
		goto out;

	/* WiFiSarEnable 0: ignore BIOS config; 1: use BIOS config */
	if (wrds_pkg->package.elements[1].type != ACPI_TYPE_INTEGER ||
	    wrds_pkg->package.elements[1].integer.value == 0)
		goto out;

	wrds_raw = kmalloc(ACPI_WRDS_SIZE, GFP_KERNEL);
	if (!wrds_raw)
		goto out;

	/* read elements[2~11] */
	for (i = 0; i < ACPI_WRDS_SIZE; i++) {
		union acpi_object *entry;

		entry = &wrds_pkg->package.elements[idx++];
		if (entry->type != ACPI_TYPE_INTEGER ||
		    entry->integer.value > U8_MAX) {
			kfree(wrds_raw);
			wrds_raw = NULL;
			goto out;
		}

		wrds_raw[i] = entry->integer.value;
	}
out:
	kfree(wrds);

	return wrds_raw;
}

static void rtw_sar_apply_wrds(struct rtw_dev *rtwdev,
			       const struct sar_wrds *wrds)
{
	int path;

	for (path = 0; path < SAR_WRDS_CHAIN_NR; path++) {
		rtw_phy_set_tx_power_sar(rtwdev, RTW_REGD_WW, path, 1, 14,
					 wrds->chain[path].limit[LMT_CH1_14]);
		rtw_phy_set_tx_power_sar(rtwdev, RTW_REGD_WW, path, 36, 64,
					 wrds->chain[path].limit[LMT_CH36_64]);
		rtw_phy_set_tx_power_sar(rtwdev, RTW_REGD_WW, path, 100, 144,
					 wrds->chain[path].limit[LMT_CH100_144]);
		rtw_phy_set_tx_power_sar(rtwdev, RTW_REGD_WW, path, 149, 165,
					 wrds->chain[path].limit[LMT_CH149_165]);
	}
}

static bool rtw_sar_load_static_tables(struct rtw_dev *rtwdev)
{
	struct sar_wrds *wrds;

	wrds = rtw_sar_get_wrds_table(rtwdev);
	if (!wrds)
		return false;

	rtw_dbg(rtwdev, RTW_DBG_REGD,
		"SAR: Apply WRDS to TX power\n");

	rtw_sar_apply_wrds(rtwdev, wrds);
	kfree(wrds);

	return true;
}

#define ACPI_RWRD_METHOD	"RWRD"
#define ACPI_RWSI_METHOD	"RWSI"
#define ACPI_RWGS_METHOD	"RWGS"

#define SAR_RWRD_ID_HP		0x5048
#define SAR_RWRD_ID_RT		0x5452

#define SAR_RWRD_CHAIN_NR	4

struct sar_rwrd {
	u16 id;
	u8 en;
	u8 count;
	struct {
		struct sar_limits chain[SAR_RWRD_CHAIN_NR];
	} mode[0];
} __packed;

struct sar_rwsi_hp {
	u8 index[SAR_RWRD_CHAIN_NR];
};

struct sar_rwsi_rt {
	u8 index;
};

union sar_rwsi {
	struct sar_rwsi_hp hp;
	struct sar_rwsi_rt rt;
};

enum sar_rwgs_band {
	SAR_RWGS_2G,
	SAR_RWGS_5G,
	SAR_RWGS_BAND_NR,
};

enum sar_rwgs_geo_hp {
	SAR_RWGS_HP_FCC_IC,
	SAR_RWGS_HP_ETSI_MKK,
	SAR_RWGS_HP_WW_KCC,

	SAR_RWGS_HP_NR,
};

struct sar_rwgs_hp {
	struct {
		struct {
			s8 max;		/* Q1 + 10 */
			s8 delta[4];	/* Q1 */
		} band[SAR_RWGS_BAND_NR];
	} geo[SAR_RWGS_HP_NR];
} __packed;

enum sar_rwgs_geo_rt {
	SAR_RWGS_RT_FCC,
	SAR_RWGS_RT_CE,
	SAR_RWGS_RT_MKK,
	SAR_RWGS_RT_IC,
	SAR_RWGS_RT_KCC,
	SAR_RWGS_RT_WW,

	SAR_RWGS_RT_NR,
};

struct sar_rwgs_rt {
	struct {
		struct {
			u8 max;		/* Q3 */
			s8 delta;	/* Q1 */
		} band[SAR_RWGS_BAND_NR];
	} geo[SAR_RWGS_RT_NR];
} __packed;

union sar_rwgs {
	struct sar_rwgs_hp hp;
	struct sar_rwgs_rt rt;
};

struct geo_map {
	int idx;	/* index of rwgs.geo[] */
	int rd;		/* RTW_REGD_xxx */
};

static const struct geo_map geo_map_hp[] = {
	{SAR_RWGS_HP_FCC_IC,   RTW_REGD_FCC},
	{SAR_RWGS_HP_FCC_IC,   RTW_REGD_IC},
	{SAR_RWGS_HP_ETSI_MKK, RTW_REGD_ETSI},
	{SAR_RWGS_HP_ETSI_MKK, RTW_REGD_MKK},
	{SAR_RWGS_HP_WW_KCC,   RTW_REGD_KCC},
	{SAR_RWGS_HP_WW_KCC,   RTW_REGD_WW},
};

static const struct geo_map geo_map_rt[] = {
	{SAR_RWGS_RT_FCC, RTW_REGD_FCC},
	{SAR_RWGS_RT_CE,  RTW_REGD_ETSI},
	{SAR_RWGS_RT_MKK, RTW_REGD_MKK},
	{SAR_RWGS_RT_IC,  RTW_REGD_IC},
	{SAR_RWGS_RT_KCC, RTW_REGD_KCC},
	{SAR_RWGS_RT_WW,  RTW_REGD_WW},
};

struct sar_read {
	int (*rwsi_mode)(struct rtw_dev *rtwdev, int path);
	int (*rwrd_base_q3)(struct rtw_dev *rtwdev, int mode, int path, int chidx);
	int (*rwgs_delta_q3)(struct rtw_dev *rtwdev, int gi, int path, int band);
	int (*rwgs_max_q3)(struct rtw_dev *rtwdev, int gi, int band);
	const struct geo_map *gm, *gm_end;
	int rwsi_sz;
	int rwgs_sz;
	int rwgs_geos;
};

static int rwsi_mode_hp(struct rtw_dev *rtwdev, int path)
{
	return rtwdev->sar_rwsi->hp.index[path] - 1;
}

static int rwrd_base_q3_hp(struct rtw_dev *rtwdev, int mode, int path, int chidx)
{
	int sar;

	sar = rtwdev->sar_rwrd->mode[mode].chain[path].limit[chidx];

	return (10 << 3) + (sar << 2);
}

static int rwgs_delta_q3_hp(struct rtw_dev *rtwdev, int gi, int path, int band)
{
	return rtwdev->sar_rwgs->hp.geo[gi].band[band].delta[path] << 2;
}

static int rwgs_max_q3_hp(struct rtw_dev *rtwdev, int gi, int band)
{
	return (10 << 3) + (rtwdev->sar_rwgs->hp.geo[gi].band[band].max << 2);
}

static const struct sar_read sar_read_hp = {
	.rwsi_mode = rwsi_mode_hp,
	.rwrd_base_q3 = rwrd_base_q3_hp,
	.rwgs_delta_q3 = rwgs_delta_q3_hp,
	.rwgs_max_q3 = rwgs_max_q3_hp,
	.gm = geo_map_hp,
	.gm_end = geo_map_hp + ARRAY_SIZE(geo_map_hp),
	.rwsi_sz = sizeof(struct sar_rwsi_hp),
	.rwgs_sz = sizeof(struct sar_rwgs_hp),
	.rwgs_geos = SAR_RWGS_HP_NR,
};

static int rwsi_mode_rt(struct rtw_dev *rtwdev, int path)
{
	return rtwdev->sar_rwsi->rt.index - 1;
}

static int rwrd_base_q3_rt(struct rtw_dev *rtwdev, int mode, int path, int chidx)
{
	return rtwdev->sar_rwrd->mode[mode].chain[path].limit[chidx] << 3;
}

static int rwgs_delta_q3_rt(struct rtw_dev *rtwdev, int gi, int path, int band)
{
	return rtwdev->sar_rwgs->rt.geo[gi].band[band].delta << 2;
}

static int rwgs_max_q3_rt(struct rtw_dev *rtwdev, int gi, int band)
{
	return rtwdev->sar_rwgs->rt.geo[gi].band[band].max;
}

static const struct sar_read sar_read_rt = {
	.rwsi_mode = rwsi_mode_rt,
	.rwrd_base_q3 = rwrd_base_q3_rt,
	.rwgs_delta_q3 = rwgs_delta_q3_rt,
	.rwgs_max_q3 = rwgs_max_q3_rt,
	.gm = geo_map_rt,
	.gm_end = geo_map_rt + ARRAY_SIZE(geo_map_rt),
	.rwsi_sz = sizeof(struct sar_rwsi_rt),
	.rwgs_sz = sizeof(struct sar_rwgs_rt),
	.rwgs_geos = SAR_RWGS_RT_NR,
};

static u8 *rtw_sar_get_raw_package(struct rtw_dev *rtwdev,
				   union acpi_object *obj, int *len)
{
	u8 *raw;
	u32 i;

	if (obj->type != ACPI_TYPE_PACKAGE || obj->package.count <= 0) {
		rtw_dbg(rtwdev, RTW_DBG_REGD,
			"SAR: Unsupported obj to dump\n");
		return NULL;
	}

	raw = kmalloc(obj->package.count, GFP_KERNEL);
	if (!raw)
		return NULL;

	for (i = 0; i < obj->package.count; i++) {
		union acpi_object *element;

		element = &obj->package.elements[i];

		if (element->type != ACPI_TYPE_INTEGER) {
			rtw_dbg(rtwdev, RTW_DBG_REGD,
				"SAR: Unexpected element type\n");
			kfree(raw);
			return NULL;
		}

		raw[i] = (u8)element->integer.value;
	}

	*len = obj->package.count;

	return raw;
}

static void *rtw_sar_get_raw_table(struct rtw_dev *rtwdev, const char *method,
				   int *len)
{
	union acpi_object *obj;
	u8 *raw;

	obj = rtw_sar_get_acpiobj(rtwdev, method);
	if (!obj)
		return NULL;

	raw = rtw_sar_get_raw_package(rtwdev, obj, len);
	kfree(obj);

	return raw;
}

static bool valid_rwrd(struct rtw_dev *rtwdev, const struct sar_rwrd *rwrd,
		       int len)
{
	if (len < sizeof(*rwrd)) {
		rtw_dbg(rtwdev, RTW_DBG_REGD,
			"SAR: RWRD: len %d is too short\n", len);
		return false;
	}

	switch (rwrd->id) {
	case SAR_RWRD_ID_HP:
		rtwdev->sar_read = &sar_read_hp;
		break;
	case SAR_RWRD_ID_RT:
		rtwdev->sar_read = &sar_read_rt;
		break;
	default:
		rtw_dbg(rtwdev, RTW_DBG_REGD,
			"SAR: RWRD: ID %04x isn't supported\n", rwrd->id);
		return false;
	}

	if (sizeof(*rwrd) + rwrd->count * sizeof(rwrd->mode[0]) != len) {
		rtw_dbg(rtwdev, RTW_DBG_REGD,
			"SAR: RWRD: len(%d) doesn't match count(%d)\n",
			len, rwrd->count);
		return false;
	}

	return true;
}

static bool valid_rwsi_idx(struct rtw_dev *rtwdev, const struct sar_rwrd *rwrd,
			   const u8 index[], int len)
{
	/* index range is one based. i.e. 1 <= index[] <= rwrd->count */
	int i;

	for (i = 0; i < len; i++)
		if (index[i] < 1 || index[i] > rwrd->count) {
			rtw_dbg(rtwdev, RTW_DBG_REGD,
				"SAR: RWSI: index is out of range\n");
			return false;
		}

	return true;
}

static bool valid_rwsi(struct rtw_dev *rtwdev, const struct sar_rwrd *rwrd,
		       const union sar_rwsi *rwsi, int len)
{
	const struct sar_read *r = rtwdev->sar_read;

	if (r->rwsi_sz != len)
		goto err;

	if (rwrd->id == SAR_RWRD_ID_HP &&
	    valid_rwsi_idx(rtwdev, rwrd, rwsi->hp.index, SAR_RWRD_CHAIN_NR))
		return true;

	if (rwrd->id == SAR_RWRD_ID_RT &&
	    valid_rwsi_idx(rtwdev, rwrd, &rwsi->rt.index, 1)) {
		return true;
	}

err:
	rtw_dbg(rtwdev, RTW_DBG_REGD,
		"SAR: RWSI: len doesn't match struct size\n");

	return false;
}

static bool valid_rwgs(struct rtw_dev *rtwdev, const struct sar_rwrd *rwrd,
		       const union sar_rwgs *rwgs, int len)
{
	const struct sar_read *r = rtwdev->sar_read;

	if (r->rwgs_sz == len)
		return true;

	rtw_dbg(rtwdev, RTW_DBG_REGD,
		"SAR: RWGS: len doesn't match struct size\n");

	return false;
}

#ifdef CONFIG_RTW88_DEBUGFS
void rtw_sar_dump_via_debugfs(struct rtw_dev *rtwdev, struct seq_file *m)
{
#define q3_int(q3)	((q3) >> 3)
#define q3_fra(q3)	(((q3) & 0x7) * 125)

	const struct sar_rwrd *rwrd = rtwdev->sar_rwrd;
	const union sar_rwsi *rwsi = rtwdev->sar_rwsi;
	const union sar_rwgs *rwgs = rtwdev->sar_rwgs;
	const struct sar_read *r = rtwdev->sar_read;
	int q3;
	int mode;
	int path;
	int chidx;
	int gi;
	int band;

	if (!rwrd || !rwsi || !rwgs || !r) {
		seq_puts(m, "(No SAR data)\n");
		return;
	}

	seq_printf(m, "Customer ID: 0x%04x\n", rwrd->id);
	seq_printf(m, "WiFiEnable: 0x%x\n", rwrd->en);
	seq_printf(m, "Total SAR Table Count: %d\n", rwrd->count);
	seq_printf(m, "Current SAR Table Index: (%*ph)\n", r->rwsi_sz, rwsi);
	seq_puts(m, "\n");

	seq_printf(m, "Dump RWRD SAR RAW DATA. (Total Count: %ld)\n",
		   rwrd->count * sizeof(rwrd->mode[0]));
	for (mode = 0; mode < rwrd->count; mode++)
		seq_printf(m, "%02x: %20ph\n", mode + 1, &rwrd->mode[mode]);
	seq_puts(m, "\n");

	seq_puts(m, "Show SAR PowerLimit:\n");
	for (path = 0; path < 2; path++) {
		mode = r->rwsi_mode(rtwdev, path);
		q3 = r->rwrd_base_q3(rtwdev, mode, path, LMT_CH1_14);
		seq_printf(m, "2.4G Antenna %d: [%d.%d] dBm\n", path,
			   q3_int(q3), q3_fra(q3));
	}
	seq_puts(m, "\n");

	for (path = 0; path < 2; path++) {
		mode = r->rwsi_mode(rtwdev, path);
		seq_printf(m, "5G Antenna %d: [", path);
		for (chidx = LMT_CH36_64; chidx <= LMT_CH149_165; chidx++) {
			q3 = r->rwrd_base_q3(rtwdev, mode, path, chidx);
			seq_printf(m, "%d.%d, ", q3_int(q3), q3_fra(q3));
		}
		seq_puts(m, "] dBm\n");
	}
	seq_puts(m, "\n");

	seq_printf(m, "Dump Geo-SAR Table RAW DATA. (Total Count: %d)\n",
		   r->rwgs_sz);
	for (gi = 0; gi < r->rwgs_geos; gi++) {
		seq_printf(m, "geo-%d: %*ph\n", gi, r->rwgs_sz / r->rwgs_geos,
			   (u8 *)rwgs + gi * (r->rwgs_sz / r->rwgs_geos));
	}
	seq_puts(m, "\n");

	gi = 1;
	seq_puts(m, "Show Geo-SAR PowerLimit:\n");
	seq_printf(m, "2G Geo Table Index: %d\n", gi);
	seq_printf(m, "5G Geo Table Index: %d\n", gi);
	for (band = SAR_RWGS_2G; band < SAR_RWGS_BAND_NR; band++) {
		seq_puts(m, "\n");
		seq_printf(m, "%dGHz:\n", band == 0 ? 2 : 5);
		q3 = r->rwgs_max_q3(rtwdev, gi, band);
		seq_printf(m, "Max Power: [%d.%d] dBm\n", q3_int(q3),
			   q3_fra(q3));
		for (path = 0; path < 2; path++) {
			q3 = r->rwgs_delta_q3(rtwdev, gi, path, band);
			seq_printf(m, "Ant-%d delta value: [%d.%d] dB\n", path,
				   q3_int(q3), q3_fra(q3));
		}
	}
}
#endif

static void rtw_sar_apply_dynamic_tables(struct rtw_dev *rtwdev)
{
	struct rtw_hal *hal = &rtwdev->hal;
	const struct sar_read *r = rtwdev->sar_read;
	const struct geo_map *gm = r->gm;
	const struct geo_map *gm_end = r->gm_end;
	int path_num = min_t(int, SAR_RWRD_CHAIN_NR, hal->rf_path_num);
	int path, mode;
	int sar, delta, max;

	for (; gm < gm_end; gm++) {
		for (path = 0; path < path_num; path++) {
			mode = r->rwsi_mode(rtwdev, path);

			/* 2.4G part */
			delta = r->rwgs_delta_q3(rtwdev, gm->idx, path, SAR_RWGS_2G);
			max = r->rwgs_max_q3(rtwdev, gm->idx, SAR_RWGS_2G);

			sar = r->rwrd_base_q3(rtwdev, mode, path, LMT_CH1_14);
			sar = min(sar + delta, max);
			rtw_phy_set_tx_power_sar(rtwdev, gm->rd, path, 1, 14, sar);

			/* 5G part */
			delta = r->rwgs_delta_q3(rtwdev, gm->idx, path, SAR_RWGS_5G);
			max = r->rwgs_max_q3(rtwdev, gm->idx, SAR_RWGS_5G);

			sar = r->rwrd_base_q3(rtwdev, mode, path, LMT_CH36_64);
			sar = min(sar + delta, max);
			rtw_phy_set_tx_power_sar(rtwdev, gm->rd, path, 36, 64, sar);

			sar = r->rwrd_base_q3(rtwdev, mode, path, LMT_CH100_144);
			sar = min(sar + delta, max);
			rtw_phy_set_tx_power_sar(rtwdev, gm->rd, path, 100, 144, sar);

			sar = r->rwrd_base_q3(rtwdev, mode, path, LMT_CH149_165);
			sar = min(sar + delta, max);
			rtw_phy_set_tx_power_sar(rtwdev, gm->rd, path, 149, 165, sar);
		}
	}
}

static bool rtw_sar_check_if_rwsi_changed(struct rtw_dev *rtwdev)
{
	union sar_rwsi *rwsi, *old;
	bool valid;
	int len;

	if (!rtwdev->sar_rwrd || !rtwdev->sar_rwsi || !rtwdev->sar_rwgs)
		return false;

	rwsi = rtw_sar_get_raw_table(rtwdev, ACPI_RWSI_METHOD, &len);
	if (!rwsi)
		return false;
	valid = valid_rwsi(rtwdev, rtwdev->sar_rwrd, rwsi, len);
	if (!valid) {
		kfree(rwsi);
		return false;
	}

	if (memcmp(rwsi, rtwdev->sar_rwsi, len) == 0) {
		kfree(rwsi);
		return true;
	}

	old = rtwdev->sar_rwsi;
	rtwdev->sar_rwsi = rwsi;
	kfree(old);

	rtw_dbg(rtwdev, RTW_DBG_REGD, "SAR: RWSI is changed\n");

	rtw_sar_apply_dynamic_tables(rtwdev);

	rtw_phy_set_tx_power_level(rtwdev, rtwdev->hal.current_channel);

	return true;
}

static bool rtw_sar_load_dynamic_tables(struct rtw_dev *rtwdev)
{
	struct sar_rwrd *rwrd;
	union sar_rwsi *rwsi;
	union sar_rwgs *rwgs;
	int len;
	bool valid;

	rwrd = rtw_sar_get_raw_table(rtwdev, ACPI_RWRD_METHOD, &len);
	if (!rwrd)
		goto out;
	valid = valid_rwrd(rtwdev, rwrd, len);
	if (!valid)
		goto out_rwrd;
	if (!rwrd->en) {
		rtw_dbg(rtwdev, RTW_DBG_REGD, "SAR: RWRD isn't enabled\n");
		goto out_rwrd;
	}

	rwsi = rtw_sar_get_raw_table(rtwdev, ACPI_RWSI_METHOD, &len);
	if (!rwsi)
		goto out_rwrd;
	valid = valid_rwsi(rtwdev, rwrd, rwsi, len);
	if (!valid)
		goto out_rwsi;

	rwgs = rtw_sar_get_raw_table(rtwdev, ACPI_RWGS_METHOD, &len);
	if (!rwgs)
		goto out_rwsi;
	valid = valid_rwgs(rtwdev, rwrd, rwgs, len);
	if (!valid)
		goto out_rwgs;

	rtwdev->sar_rwrd = rwrd;
	rtwdev->sar_rwsi = rwsi;
	rtwdev->sar_rwgs = rwgs;

	rtw_sar_apply_dynamic_tables(rtwdev);

	rtw_dbg(rtwdev, RTW_DBG_REGD, "SAR: RWRD/RWSI/RWGS is adopted\n");

	return true;

out_rwgs:
	kfree(rwgs);
out_rwsi:
	kfree(rwsi);
out_rwrd:
	kfree(rwrd);
out:
	return false;
}
#else
static bool rtw_sar_load_static_tables(struct rtw_dev *rtwdev)
{
	return false;
}

static bool rtw_sar_check_if_rwsi_changed(struct rtw_dev *rtwdev)
{
	return false;
}

static bool rtw_sar_load_dynamic_tables(struct rtw_dev *rtwdev)
{
	return false;
}

void rtw_sar_dump_via_debugfs(struct rtw_dev *rtwdev, struct seq_file *m)
{
	seq_puts(m, "(No SAR data)\n");
}
#endif /* CONFIG_ACPI */

bool rtw_sar_load_table(struct rtw_dev *rtwdev)
{
	bool ret;

	ret = rtw_sar_load_dynamic_tables(rtwdev);
	if (ret)
		return ret;

	return rtw_sar_load_static_tables(rtwdev);
}

void rtw_sar_work(struct work_struct *work)
{
	struct rtw_dev *rtwdev = container_of(work, struct rtw_dev,
					      sar_work.work);

	if (!rtw_sar_check_if_rwsi_changed(rtwdev))
		return;

	ieee80211_queue_delayed_work(rtwdev->hw, &rtwdev->sar_work,
				     RTW_SAR_DELAY_TIME);
}
