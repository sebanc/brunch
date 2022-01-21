// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Support for Android Bluetooth Quality Report (BQR) specifications
 *      https://source.android.com/devices/bluetooth/hci_requirements
 *
 *  Copyright (C) 2021 Google Corporation
 */

#include <linux/module.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>

#include "btandroid.h"

#define VERSION "0.1"

/*
 * Reference for the command op codes and parameters below:
 *   https://source.android.com/devices/bluetooth/hci_requirements#bluetooth-quality-report-command
 */
#define BQR_COMMAND_OCF			0x015e
#define BQR_OPCODE			hci_opcode_pack(0x3f, BQR_COMMAND_OCF)

/* report action */
#define REPORT_ACTION_ADD		0x00
#define REPORT_ACTION_DELETE		0x01
#define REPORT_ACTION_CLEAR		0x02

/* BQR event masks */
#define QUALITY_MONITORING		(1 << 0)
#define APPRAOCHING_LSTO		(1 << 1)
#define A2DP_AUDIO_CHOPPY		(1 << 2)
#define SCO_VOICE_CHOPPY		(1 << 3)

#define DEFAULT_BQR_EVENT_MASK	(QUALITY_MONITORING | APPRAOCHING_LSTO | \
				 A2DP_AUDIO_CHOPPY | SCO_VOICE_CHOPPY)

/*
 * Reporting at seconds so as not to stress the controller too much.
 * Range: 0 ~ 65535 ms
 */
#define DEFALUT_REPORT_INTERVAL_MS	5000

struct android_bqr_cp {
	__u8	report_action;
	__u32	event_mask;
	__u16	min_report_interval;
} __packed;

static int enable_quality_report(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	struct android_bqr_cp cp;

	cp.report_action = REPORT_ACTION_ADD;
	cp.event_mask = DEFAULT_BQR_EVENT_MASK;
	cp.min_report_interval = DEFALUT_REPORT_INTERVAL_MS;

	skb = __hci_cmd_sync(hdev, BQR_OPCODE, sizeof(cp), &cp,
							HCI_CMD_TIMEOUT);
	if (IS_ERR(skb)) {
		bt_dev_err(hdev, "Enabling Android BQR failed (%ld)",
			   PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	kfree_skb(skb);
	return 0;
}

static int disable_quality_report(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	struct android_bqr_cp cp = { 0 };

	cp.report_action = REPORT_ACTION_CLEAR;

	skb = __hci_cmd_sync(hdev, BQR_OPCODE, sizeof(cp), &cp,
							HCI_CMD_TIMEOUT);
	if (IS_ERR(skb)) {
		bt_dev_err(hdev, "Disabling Android BQR failed (%ld)",
			   PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	kfree_skb(skb);
	return 0;
}

int btandroid_set_quality_report(struct hci_dev *hdev, bool enable)
{
	bt_dev_info(hdev, "quality report enable %d", enable);

	/* Enable or disable the quality report feature. */
	if (enable)
		return enable_quality_report(hdev);
	else
		return disable_quality_report(hdev);
}
EXPORT_SYMBOL_GPL(btandroid_set_quality_report);

MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("Support for Android Bluetooth Specification " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
