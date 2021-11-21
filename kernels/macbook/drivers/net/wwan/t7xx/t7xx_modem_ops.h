/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, MediaTek Inc.
 * Copyright (c) 2021, Intel Corporation.
 */

#ifndef __T7XX_MODEM_OPS_H__
#define __T7XX_MODEM_OPS_H__

#include "t7xx_dev_node.h"
#include "t7xx_modem_ops.h"

#define RT_ID_MD_PORT_ENUM 0
#define FEATURE_COUNT 64
#define MD_FEATURE_QUERY_PATTERN 0x49434343
#define AP_FEATURE_QUERY_PATTERN 0x43434349
#define CCCI_AP_RUNTIME_RESERVED_SIZE 120
#define CCCI_MD_RUNTIME_RESERVED_SIZE 152
#define MD_SETTING_ENABLE	BIT(0)
#define MD_SETTING_RELOAD	BIT(1)
#define MD_SETTING_FIRST_BOOT	BIT(2)

enum CCCI_RUNTIME_FEATURE_SUPPORT_TYPE {
	CCCI_FEATURE_NOT_EXIST = 0,
	CCCI_FEATURE_NOT_SUPPORT = 1,
	CCCI_FEATURE_MUST_SUPPORT = 2,
	CCCI_FEATURE_OPTIONAL_SUPPORT = 3,
	CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT = 4,
};

struct ccci_feature_support {
	u8 support_mask:4;
	u8 version:4;
};

enum hif_ex_stage {
	HIF_EX_INIT = 0,	/* interrupt */
	HIF_EX_ACK,		/* AP->MD */
	HIF_EX_INIT_DONE,	/* polling */
	HIF_EX_CLEARQ_DONE,	/* interrupt */
	HIF_EX_CLEARQ_ACK,	/* AP->MD */
	HIF_EX_ALLQ_RESET,	/* polling */
};

struct ccci_runtime_feature {
	u8 feature_id;	/* for debug only */
	struct ccci_feature_support support_info;
	u8 reserved[2];
	u32 data_len;
	u8 data[];
};

enum md_event_id {
	FSM_PRE_START = 0,
	FSM_START = 1,
	FSM_READY = 2,
};

struct core_sys_info {
	atomic_t ready;
	atomic_t handshake_ongoing;
	struct ccci_feature_support feature_set[FEATURE_COUNT];
	struct ccci_feature_support support_feature_set[FEATURE_COUNT];
	struct t7xx_port *ctl_port;
};

struct ccci_modem {
	struct md_sys1_info *md_info;
	struct ccci_modem_ops *ops;
	struct mtk_pci_dev *mtk_dev;
	atomic_t wdt_enabled;
	atomic_t reset_on_going;
	struct core_sys_info core_md;
	int md_init_finish;
	atomic_t rgu_irq_asserted;
	atomic_t rgu_irq_asserted_in_suspend;

	struct workqueue_struct *rgu_workqueue;
	struct delayed_work rgu_irq_work;
	struct workqueue_struct *handshake_wq;
	struct work_struct handshake_work;
};

struct ccci_modem_ops {
	int (*init)(struct ccci_modem *md);
	int (*uninit)(struct ccci_modem *md);
	int (*start)(struct ccci_modem *md);
	int (*ee_handshake)(struct ccci_modem *md, int timeout);
	int (*md_event_notify)(struct ccci_modem *md,
			       enum md_event_id evt_id);
};

struct md_sys1_info {
	unsigned int	exp_id;
	spinlock_t	exp_spinlock; /* Protect exp_id containing EX events */
};

int ccci_md_start(struct ccci_modem *md);
void ccci_md_reset(struct ccci_modem *md);
struct ccci_modem *ccci_md_alloc(struct mtk_pci_dev *mtk_dev);
int ccci_md_register(struct device *dev, struct ccci_modem *md);
void ccci_md_unregister(struct ccci_modem *md);
void ccci_md_exception_handshake(struct ccci_modem *md, int timeout);
int ccci_modem_reset(struct mtk_pci_dev *mtk_dev);
int ccci_modem_init(struct mtk_pci_dev *mtk_dev);
int ccci_modem_exit(struct mtk_pci_dev *mtk_dev);
void mtk_clear_rgu_irq(struct mtk_pci_dev *mtk_dev);
int mtk_acpi_fldr_func(struct mtk_pci_dev *mtk_dev);
int mtk_pci_event_handler(struct mtk_pci_dev *mtk_dev, u32 event);
int mtk_pci_mhccif_isr(struct mtk_pci_dev *mtk_dev);

#endif	/* __T7XX_MODEM_OPS_H__ */
