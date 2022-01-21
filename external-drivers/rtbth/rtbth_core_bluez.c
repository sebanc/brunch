/*
*************************************************************************
* Ralink Technology Corporation
* 5F., No. 5, Taiyuan 1st St., Jhubei City,
* Hsinchu County 302,
* Taiwan, R.O.C.
*
* (c) Copyright 2012, Ralink Technology Corporation
*
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the                         *
* Free Software Foundation, Inc.,                                       *
* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
*                                                                       *
*************************************************************************/

#include <linux/pci.h>
#include <linux/skbuff.h>
#include "rt_linux.h"
#include "hps_bluez.h"
#include "rtbt_osabl.h"

void *g_hdev = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
int rtbt_hci_dev_ioctl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg)
{
    BT_WARN("%s(dev=0x%lx): ioctl cmd=0x%x", __FUNCTION__, (ULONG)hdev, cmd);
    return -ENOIOCTLCMD;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
void rtbt_hci_dev_destruct(struct hci_dev *hdev)
{
    BT_WARN("%s(dev=0x%lx)", __FUNCTION__, (ULONG)hdev);
    return;
}
#endif

int rtbt_hci_dev_flush(struct hci_dev *hdev)
{
    BT_WARN("%s(dev=0x%lx)", __FUNCTION__, (ULONG)hdev);
    return 0;
}

//static const char *pkt_type_str[]= {"UNKNOWN", "HCI_CMD", "ACL_DATA",
//    "SCO_DATA", "HCI_EVENT", "HCI_VENDOR", "ERROR_TYPE"};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
int rtbt_hci_dev_send(struct hci_dev *hdev, struct sk_buff *skb)
{
#else
int rtbt_hci_dev_send(struct sk_buff *skb)
{
    struct hci_dev *hdev = (struct hci_dev *)skb->dev;
#endif
    struct rtbt_os_ctrl *os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hdev);
    struct rtbt_hps_ops *hps_ops;
    unsigned char pkt_type;
    int status = -EPERM;

    pkt_type = hci_skb_pkt_type(skb);
//     printk("hciName:%s type:%s(%d) len:%d\n",
//            hdev->name, pkt_type_str[pkt_type],
//            pkt_type, skb->len);
//    if (pkt_type == HCI_COMMAND_PKT)
//        hex_dump("rtbt_hci_dev_send: HCI_CMD", skb->data, skb->len);
//    else if (pkt_type == HCI_SCODATA_PKT)
//        hex_dump("rtbt_hci_dev_send: HCI_SCO", skb->data, skb->len);

    if (!os_ctrl || !os_ctrl->hps_ops) {
        kfree_skb(skb);
        return -1;
    }

    hps_ops = os_ctrl->hps_ops;
    if ((!hps_ops->hci_cmd) ||
        (!hps_ops->hci_acl_data) ||
        (!hps_ops->hci_sco_data)) {
        BT_ERR("Null Handler! hci_cmd=0x%p, acl_data=0x%p, sco_data=0x%p!",
                hps_ops->hci_cmd, hps_ops->hci_acl_data, hps_ops->hci_sco_data);
        kfree_skb(skb);
        return -1;
    }

    switch (pkt_type) {
        case HCI_COMMAND_PKT:
            status = hps_ops->hci_cmd(os_ctrl->dev_ctrl, skb->data, skb->len);
            if (hdev && (status == 0))
                hdev->stat.cmd_tx++;
            break;
        case HCI_ACLDATA_PKT:
            status = hps_ops->hci_acl_data(os_ctrl->dev_ctrl, skb->data, skb->len);
            if (hdev && (status == 0))
                hdev->stat.acl_tx++;
            break;
        case HCI_SCODATA_PKT:
            BT_DBG("%s(): sco len=%d,time=0x%lx", __FUNCTION__, skb->len, jiffies);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
            os_ctrl->sco_tx_seq = bt_cb(skb)->l2cap.txseq;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
            os_ctrl->sco_tx_seq = bt_cb(skb)->control.txseq;
#else
            os_ctrl->sco_tx_seq = bt_cb(skb)->tx_seq;
#endif
            os_ctrl->sco_time_hci = jiffies;
            status = hps_ops->hci_sco_data(os_ctrl->dev_ctrl, skb->data, skb->len);
            if (hdev && (status == 0))
                hdev->stat.sco_tx++;
            BT_DBG("%s(): sco done, time=0x%lx", __FUNCTION__, jiffies);
            break;
        case HCI_VENDOR_PKT:
            break;
    }

    if (hdev && (status == 0))
        hdev->stat.byte_tx += skb->len;
    else
        hdev->stat.err_tx++;
    kfree_skb(skb);
    return 0;
}

extern void *g_hdev;
int rtbt_hci_dev_receive(void *bt_dev, int pkt_type, char *buf, int len)
{
    struct hci_dev *hdev = 0;
    struct sk_buff *skb;

    //printk("-->%s(): receive info: pkt_type=%d(%s), len=%d!\n", __FUNCTION__,
           //pkt_type, pkt_type <= 5 ? pkt_type_str[pkt_type] : "ErrPktType", len);
    switch (pkt_type) {
        case HCI_EVENT_PKT:
            if (len < HCI_EVENT_HDR_SIZE) {
                BT_ERR("event block is too short");
                return -EILSEQ;
            }
            break;
        case HCI_ACLDATA_PKT:
            if (len < HCI_ACL_HDR_SIZE) {
                BT_ERR("data block is too short");
                return -EILSEQ;
            }
            break;
        case HCI_SCODATA_PKT:
            if (len < HCI_SCO_HDR_SIZE) {
                BT_ERR("audio block is too short");
                return -EILSEQ;
            }
            break;
    }
    skb = bt_skb_alloc(len, GFP_ATOMIC);
    if (!skb) {
        BT_ERR("No memory for the packet");
        return -ENOMEM;
    }
    skb->dev = g_hdev;
    hci_skb_pkt_type(skb) = pkt_type; // set pkt type
    memcpy(skb_put(skb, len), buf, len);
    if (pkt_type == HCI_SCODATA_PKT)
        BT_DBG("%s(): send sco data to OS, time=0x%lx", __FUNCTION__, jiffies);
    hdev = (struct hci_dev *)skb->dev;
    if (hdev)
        hdev->stat.byte_rx += len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
    return hci_recv_frame(hdev, skb);
#else
    return hci_recv_frame(skb);
#endif
}

int rtbt_hci_dev_open(struct hci_dev *hdev)
{
    int status = -EPERM;
    struct rtbt_os_ctrl *os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hdev);
    BT_DBG("-->%s()", __FUNCTION__);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
    if (test_bit(HCI_RUNNING, &hdev->flags))
        return 0;
#endif
    if (os_ctrl && os_ctrl->hps_ops && os_ctrl->hps_ops->open)
        status = os_ctrl->hps_ops->open(os_ctrl->dev_ctrl);
    else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
        clear_bit(HCI_RUNNING, &hdev->flags);
#endif
        BT_ERR("os_ctrl->hps_ops->dev_open is null");
    }
    if (status == 0) {
        os_ctrl->sco_tx_seq = -1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
        set_bit(HCI_RUNNING, &hdev->flags);
#endif
    }
    BT_DBG("<--%s(), status=%d", __FUNCTION__, status);
    return status;
}

int rtbt_hci_dev_close(struct hci_dev *hdev)
{
    int status = -EPERM;
    struct rtbt_os_ctrl *os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hdev);
    BT_DBG("-->%s()", __FUNCTION__);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0)
    if (!test_and_clear_bit(HCI_RUNNING, &(hdev->flags))){
        BT_WARN("%s(): Directly return due to hdev->flags=0x%lx", __FUNCTION__, hdev->flags);
        return 0;
    }
#endif
    if (os_ctrl && os_ctrl->hps_ops && os_ctrl->hps_ops->close)
        status = os_ctrl->hps_ops->close(os_ctrl->dev_ctrl);
    else
        BT_ERR("%s(): os_ctrl->hps_ops->close is null!", __FUNCTION__);
    BT_DBG("<--%s()", __FUNCTION__);
    return status;
}

int rtbt_hps_iface_suspend(IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev = os_ctrl->bt_dev;
    hci_suspend_dev(hdev);
    return 0;
}

int rtbt_hps_iface_resume(IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev = os_ctrl->bt_dev;
    hci_resume_dev(hdev);
    return 0;
}

int rtbt_hps_iface_detach(IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev = os_ctrl->bt_dev;
    BT_DBG("-->%s()", __FUNCTION__);
    rtbt_dev_hold(hdev);
    /* unregister HCI device */
    if (!hdev) {
        BT_ERR("%s(): os_ctrl(%p)->bt_dev is NULL", __FUNCTION__, os_ctrl);
        return -1;
    }
    hci_unregister_dev(hdev);
    rtbt_dev_put(hdev);
    BT_DBG("<--%s()", __FUNCTION__);
    return 0;
}

int rtbt_hps_iface_attach(IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev = os_ctrl->bt_dev;
    BT_DBG("-->%s()", __FUNCTION__);
    rtbt_dev_hold(hdev);
    /* Register HCI device */
    if (hci_register_dev(hdev) < 0) {
        BT_ERR("Can't register HCI device");
        return -ENODEV;
    }
    rtbt_dev_put(hdev);
    BT_DBG("<--%s()", __FUNCTION__);
    return 0;
}

int rtbt_hps_iface_deinit(
    IN int if_type,
    IN void *if_dev,
    IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev;
    if (if_type == RAL_INF_PCI) {
        hdev = (struct hci_dev *)pci_get_drvdata(if_dev);
        BT_WARN("%s(): hciDev=0x%p", __FUNCTION__, hdev);
    }
    else
        return FALSE;
    if (hdev)
        hci_free_dev(hdev);
    os_ctrl->bt_dev = NULL;
    return TRUE;
}

int rtbt_hps_iface_init(
    IN int if_type,
    IN void *if_dev,
    IN struct rtbt_os_ctrl *os_ctrl)
{
    struct hci_dev *hdev;
    BT_DBG("-->%s(): if_type=%d", __FUNCTION__, if_type);
    /* Initialize HCI device */
    hdev = hci_alloc_dev();
    if (!hdev) {
        BT_ERR("Can't allocate HCI device");
        return -1;
    }

    switch (if_type) {
        case RAL_INF_PCI:
            {
                struct pci_dev *pcidev = (struct pci_dev *)if_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
                hdev->bus = HCI_PCI;
#else
                hdev->type = HCI_PCI;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
                hdev->dev_type = HCI_PRIMARY;
#else
                hdev->dev_type = HCI_BREDR;
#endif
                pci_set_drvdata(pcidev, hdev);
                SET_HCIDEV_DEV(hdev, &pcidev->dev);
            }
            break;
        default:
            BT_ERR("invalid if_type(%d)!", if_type);
            hci_free_dev(hdev);
            return -1;
    }

    g_hdev = hdev;
    os_ctrl->bt_dev = hdev;
    os_ctrl->if_dev = if_dev;
    os_ctrl->hps_ops->recv = rtbt_hci_dev_receive;
    hci_set_drvdata(hdev, os_ctrl);
    hdev->open = rtbt_hci_dev_open;
    hdev->close = rtbt_hci_dev_close;
    hdev->flush = rtbt_hci_dev_flush;
    hdev->send = rtbt_hci_dev_send;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
    hdev->ioctl = rtbt_hci_dev_ioctl;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
    hdev->destruct = rtbt_hci_dev_destruct;
    hdev->owner = THIS_MODULE;
#endif

    BT_DBG("<--%s(): alloc hdev(0x%lx) done", __FUNCTION__, (ULONG)hdev);
    return 0;
}
