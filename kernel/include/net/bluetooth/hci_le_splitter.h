/*
   HCI LE splitter - two stacks - one chip
   Copyright (C) 2017 Google, Inc

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS,
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS
   SOFTWARE IS DISCLAIMED.
*/

#ifndef __BT_HCI_LE_SPLITTER_H
#define __BT_HCI_LE_SPLITTER_H

#include <linux/types.h>

#define HCI_LE_SPLIT_MAX_LE_CONNS			64
#define HCI_LE_SPLITTER_BUFFER_TARGET			3

struct hci_dev;
struct sk_buff;

#ifdef CONFIG_BT_HCI_LE_SPLITTER

int hci_le_splitter_sysfs_init(void);
void hci_le_splitter_init_start(struct hci_dev *hdev);
int hci_le_splitter_init_done(struct hci_dev *hdev);
void hci_le_splitter_init_fail(struct hci_dev *hdev);
void hci_le_splitter_deinit(struct hci_dev *hdev);

/* return true to let bluez have it */
bool hci_le_splitter_should_allow_bluez_rx(struct hci_dev *hdev, struct sk_buff *skb);

/* return true to allow transmission */
bool hci_le_splitter_should_allow_bluez_tx(struct hci_dev *hdev, struct sk_buff *skb);


#else

static inline int hci_le_splitter_init_done(struct hci_dev *hdev)
{
	return 0;
}

static inline void hci_le_splitter_init_fail(struct hci_dev *hdev)
{

}

static inline void hci_le_splitter_deinit(struct hci_dev *hdev)
{

}

static inline bool hci_le_splitter_should_allow_bluez_rx(struct hci_dev *hdev, struct sk_buff *skb)
{
	return true;
}

static inline bool hci_le_splitter_should_allow_bluez_tx(struct hci_dev *hdev, struct sk_buff *skb)
{
	return true;
}

static inline void hci_le_splitter_init_start(struct hci_dev *hdev)
{

}

static inline int hci_le_splitter_sysfs_init(void)
{
	return 0;
}

#endif

#endif /* __HCI_LE_SPLITTER_H */
