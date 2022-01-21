#include "vhci.h"
#include "../apple_bce.h"
#include "command.h"
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/module.h>
#include <linux/version.h>

static dev_t bce_vhci_chrdev;
static struct class *bce_vhci_class;
static const struct hc_driver bce_vhci_driver;
static u16 bce_vhci_port_mask = U16_MAX;

static int bce_vhci_create_event_queues(struct bce_vhci *vhci);
static void bce_vhci_destroy_event_queues(struct bce_vhci *vhci);
static int bce_vhci_create_message_queues(struct bce_vhci *vhci);
static void bce_vhci_destroy_message_queues(struct bce_vhci *vhci);
static void bce_vhci_handle_firmware_events_w(struct work_struct *ws);
static void bce_vhci_firmware_event_completion(struct bce_queue_sq *sq);

int bce_vhci_create(struct apple_bce_device *dev, struct bce_vhci *vhci)
{
    int status;

    spin_lock_init(&vhci->hcd_spinlock);

    vhci->dev = dev;

    vhci->vdevt = bce_vhci_chrdev;
    vhci->vdev = device_create(bce_vhci_class, dev->dev, vhci->vdevt, NULL, "bce-vhci");
    if (IS_ERR_OR_NULL(vhci->vdev)) {
        status = PTR_ERR(vhci->vdev);
        goto fail_dev;
    }

    if ((status = bce_vhci_create_message_queues(vhci)))
        goto fail_mq;
    if ((status = bce_vhci_create_event_queues(vhci)))
        goto fail_eq;

    vhci->tq_state_wq = alloc_ordered_workqueue("bce-vhci-tq-state", 0);
    INIT_WORK(&vhci->w_fw_events, bce_vhci_handle_firmware_events_w);

    vhci->hcd = usb_create_hcd(&bce_vhci_driver, vhci->vdev, "bce-vhci");
    if (!vhci->hcd) {
        status = -ENOMEM;
        goto fail_hcd;
    }
    vhci->hcd->self.sysdev = &dev->pci->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,4,0)
    vhci->hcd->self.uses_dma = 1;
#endif
    *((struct bce_vhci **) vhci->hcd->hcd_priv) = vhci;
    vhci->hcd->speed = HCD_USB2;

    if ((status = usb_add_hcd(vhci->hcd, 0, 0)))
        goto fail_hcd;

    return 0;

fail_hcd:
    bce_vhci_destroy_event_queues(vhci);
fail_eq:
    bce_vhci_destroy_message_queues(vhci);
fail_mq:
    device_destroy(bce_vhci_class, vhci->vdevt);
fail_dev:
    if (!status)
        status = -EINVAL;
    return status;
}

void bce_vhci_destroy(struct bce_vhci *vhci)
{
    usb_remove_hcd(vhci->hcd);
    bce_vhci_destroy_event_queues(vhci);
    bce_vhci_destroy_message_queues(vhci);
    device_destroy(bce_vhci_class, vhci->vdevt);
}

struct bce_vhci *bce_vhci_from_hcd(struct usb_hcd *hcd)
{
    return *((struct bce_vhci **) hcd->hcd_priv);
}

int bce_vhci_start(struct usb_hcd *hcd)
{
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    int status;
    u16 port_mask = 0;
    bce_vhci_port_t port_no = 0;
    if ((status = bce_vhci_cmd_controller_enable(&vhci->cq, 1, &port_mask)))
        return status;
    vhci->port_mask = port_mask;
    vhci->port_power_mask = 0;
    if ((status = bce_vhci_cmd_controller_start(&vhci->cq)))
        return status;
    port_mask = vhci->port_mask;
    while (port_mask) {
        port_no += 1;
        port_mask >>= 1;
    }
    vhci->port_count = port_no;
    return 0;
}

void bce_vhci_stop(struct usb_hcd *hcd)
{
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    bce_vhci_cmd_controller_disable(&vhci->cq);
}

static int bce_vhci_hub_status_data(struct usb_hcd *hcd, char *buf)
{
    return 0;
}

static int bce_vhci_reset_device(struct bce_vhci *vhci, int index, u16 timeout);

static int bce_vhci_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue, u16 wIndex, char *buf, u16 wLength)
{
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    int status;
    struct usb_hub_descriptor *hd;
    struct usb_hub_status *hs;
    struct usb_port_status *ps;
    u32 port_status;
    // pr_info("bce-vhci: bce_vhci_hub_control %x %i %i [bufl=%i]\n", typeReq, wValue, wIndex, wLength);
    if (typeReq == GetHubDescriptor && wLength >= sizeof(struct usb_hub_descriptor)) {
        hd = (struct usb_hub_descriptor *) buf;
        memset(hd, 0, sizeof(*hd));
        hd->bDescLength = sizeof(struct usb_hub_descriptor);
        hd->bDescriptorType = USB_DT_HUB;
        hd->bNbrPorts = (u8) vhci->port_count;
        hd->wHubCharacteristics = HUB_CHAR_INDV_PORT_LPSM | HUB_CHAR_INDV_PORT_OCPM;
        hd->bPwrOn2PwrGood = 0;
        hd->bHubContrCurrent = 0;
        return 0;
    } else if (typeReq == GetHubStatus && wLength >= sizeof(struct usb_hub_status)) {
        hs = (struct usb_hub_status *) buf;
        memset(hs, 0, sizeof(*hs));
        hs->wHubStatus = 0;
        hs->wHubChange = 0;
        return 0;
    } else if (typeReq == GetPortStatus && wLength >= 4 /* usb 2.0 */) {
        ps = (struct usb_port_status *) buf;
        ps->wPortStatus = 0;
        ps->wPortChange = 0;

        if (vhci->port_power_mask & BIT(wIndex))
            ps->wPortStatus |= USB_PORT_STAT_POWER;

        if (!(bce_vhci_port_mask & BIT(wIndex)))
            return 0;

        if ((status = bce_vhci_cmd_port_status(&vhci->cq, (u8) wIndex, 0, &port_status)))
            return status;

        if (port_status & 16)
            ps->wPortStatus |= USB_PORT_STAT_ENABLE | USB_PORT_STAT_HIGH_SPEED;
        if (port_status & 4)
            ps->wPortStatus |= USB_PORT_STAT_CONNECTION;
        if (port_status & 2)
            ps->wPortStatus |= USB_PORT_STAT_OVERCURRENT;
        if (port_status & 8)
            ps->wPortStatus |= USB_PORT_STAT_RESET;
        if (port_status & 0x60)
            ps->wPortStatus |= USB_PORT_STAT_SUSPEND;

        if (port_status & 0x40000)
            ps->wPortChange |= USB_PORT_STAT_C_CONNECTION;

        pr_info("bce-vhci: Translated status %x to %x:%x\n", port_status, ps->wPortStatus, ps->wPortChange);
        return 0;
    } else if (typeReq == SetPortFeature) {
        if (wValue == USB_PORT_FEAT_POWER) {
            status = bce_vhci_cmd_port_power_on(&vhci->cq, (u8) wIndex);
            /* As far as I am aware, power status is not part of the port status so store it separately */
            if (!status)
                vhci->port_power_mask |= BIT(wIndex);
            return status;
        }
        if (wValue == USB_PORT_FEAT_RESET) {
            return bce_vhci_reset_device(vhci, wIndex, wValue);
        }
        if (wValue == USB_PORT_FEAT_SUSPEND) {
            /* TODO: Am I supposed to also suspend the endpoints? */
            pr_info("bce-vhci: Suspending port %i\n", wIndex);
            return bce_vhci_cmd_port_suspend(&vhci->cq, (u8) wIndex);
        }
    } else if (typeReq == ClearPortFeature) {
        if (wValue == USB_PORT_FEAT_ENABLE)
            return bce_vhci_cmd_port_disable(&vhci->cq, (u8) wIndex);
        if (wValue == USB_PORT_FEAT_POWER) {
            status = bce_vhci_cmd_port_power_off(&vhci->cq, (u8) wIndex);
            if (!status)
                vhci->port_power_mask &= ~BIT(wIndex);
            return status;
        }
        if (wValue == USB_PORT_FEAT_C_CONNECTION)
            return bce_vhci_cmd_port_status(&vhci->cq, (u8) wIndex, 0x40000, &port_status);
        if (wValue == USB_PORT_FEAT_C_RESET) { /* I don't think I can transfer it in any way */
            return 0;
        }
        if (wValue == USB_PORT_FEAT_SUSPEND) {
            pr_info("bce-vhci: Resuming port %i\n", wIndex);
            return bce_vhci_cmd_port_resume(&vhci->cq, (u8) wIndex);
        }
    }
    pr_err("bce-vhci: bce_vhci_hub_control unhandled request: %x %i %i [bufl=%i]\n", typeReq, wValue, wIndex, wLength);
    dump_stack();
    return -EIO;
}

static int bce_vhci_enable_device(struct usb_hcd *hcd, struct usb_device *udev)
{
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    struct bce_vhci_device *vdev;
    bce_vhci_device_t devid;
    pr_info("bce_vhci_enable_device\n");

    if (vhci->port_to_device[udev->portnum])
        return 0;

    /* We need to early address the device */
    if (bce_vhci_cmd_device_create(&vhci->cq, udev->portnum, &devid))
        return -EIO;

    pr_info("bce_vhci_cmd_device_create %i -> %i\n", udev->portnum, devid);

    vdev = kzalloc(sizeof(struct bce_vhci_device), GFP_KERNEL);
    vhci->port_to_device[udev->portnum] = devid;
    vhci->devices[devid] = vdev;

    bce_vhci_create_transfer_queue(vhci, &vdev->tq[0], &udev->ep0, devid, DMA_BIDIRECTIONAL);
    udev->ep0.hcpriv = &vdev->tq[0];
    vdev->tq_mask |= BIT(0);

    bce_vhci_cmd_endpoint_create(&vhci->cq, devid, &udev->ep0.desc);
    return 0;
}

static int bce_vhci_address_device(struct usb_hcd *hcd, struct usb_device *udev)
{
    /* This is the same as enable_device, but instead in the old scheme */
    return bce_vhci_enable_device(hcd, udev);
}

static void bce_vhci_free_device(struct usb_hcd *hcd, struct usb_device *udev)
{
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    int i;
    bce_vhci_device_t devid;
    struct bce_vhci_device *dev;
    pr_info("bce_vhci_free_device %i\n", udev->portnum);
    if (!vhci->port_to_device[udev->portnum])
        return;
    devid = vhci->port_to_device[udev->portnum];
    dev = vhci->devices[devid];
    for (i = 0; i < 32; i++) {
        if (dev->tq_mask & BIT(i)) {
            bce_vhci_transfer_queue_pause(&dev->tq[i], BCE_VHCI_PAUSE_SHUTDOWN);
            bce_vhci_cmd_endpoint_destroy(&vhci->cq, devid, (u8) i);
            bce_vhci_destroy_transfer_queue(vhci, &dev->tq[i]);
        }
    }
    vhci->devices[devid] = NULL;
    vhci->port_to_device[udev->portnum] = 0;
    bce_vhci_cmd_device_destroy(&vhci->cq, devid);
    kfree(dev);
}

static int bce_vhci_reset_device(struct bce_vhci *vhci, int index, u16 timeout)
{
    struct bce_vhci_device *dev = NULL;
    bce_vhci_device_t devid;
    int i;
    int status;
    enum dma_data_direction dir;
    pr_info("bce_vhci_reset_device %i\n", index);

    devid = vhci->port_to_device[index];
    if (devid) {
        dev = vhci->devices[devid];

        for (i = 0; i < 32; i++) {
            if (dev->tq_mask & BIT(i)) {
                bce_vhci_transfer_queue_pause(&dev->tq[i], BCE_VHCI_PAUSE_SHUTDOWN);
                bce_vhci_cmd_endpoint_destroy(&vhci->cq, devid, (u8) i);
                bce_vhci_destroy_transfer_queue(vhci, &dev->tq[i]);
            }
        }
        vhci->devices[devid] = NULL;
        vhci->port_to_device[devid] = 0;
        bce_vhci_cmd_device_destroy(&vhci->cq, devid);
    }
    status = bce_vhci_cmd_port_reset(&vhci->cq, (u8) index, timeout);

    if (dev) {
        if ((status = bce_vhci_cmd_device_create(&vhci->cq, index, &devid)))
            return status;
        vhci->devices[devid] = dev;
        vhci->port_to_device[index] = devid;

        for (i = 0; i < 32; i++) {
            if (dev->tq_mask & BIT(i)) {
                dir = usb_endpoint_dir_in(&dev->tq[i].endp->desc) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
                if (i == 0)
                    dir = DMA_BIDIRECTIONAL;
                bce_vhci_create_transfer_queue(vhci, &dev->tq[i], dev->tq[i].endp, devid, dir);
                bce_vhci_cmd_endpoint_create(&vhci->cq, devid, &dev->tq[i].endp->desc);
            }
        }
    }

    return status;
}

static int bce_vhci_check_bandwidth(struct usb_hcd *hcd, struct usb_device *udev)
{
    return 0;
}

static int bce_vhci_get_frame_number(struct usb_hcd *hcd)
{
    return 0;
}

static int bce_vhci_bus_suspend(struct usb_hcd *hcd)
{
    int i, j;
    int status;
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    pr_info("bce_vhci: suspend started\n");

    pr_info("bce_vhci: suspend endpoints\n");
    for (i = 0; i < 16; i++) {
        if (!vhci->port_to_device[i])
            continue;
        for (j = 0; j < 32; j++) {
            if (!(vhci->devices[vhci->port_to_device[i]]->tq_mask & BIT(j)))
                continue;
            bce_vhci_transfer_queue_pause(&vhci->devices[vhci->port_to_device[i]]->tq[j],
                    BCE_VHCI_PAUSE_SUSPEND);
        }
    }

    pr_info("bce_vhci: suspend ports\n");
    for (i = 0; i < 16; i++) {
        if (!vhci->port_to_device[i])
            continue;
        bce_vhci_cmd_port_suspend(&vhci->cq, i);
    }
    pr_info("bce_vhci: suspend controller\n");
    if ((status = bce_vhci_cmd_controller_pause(&vhci->cq)))
        return status;

    bce_vhci_event_queue_pause(&vhci->ev_commands);
    bce_vhci_event_queue_pause(&vhci->ev_system);
    bce_vhci_event_queue_pause(&vhci->ev_isochronous);
    bce_vhci_event_queue_pause(&vhci->ev_interrupt);
    bce_vhci_event_queue_pause(&vhci->ev_asynchronous);
    pr_info("bce_vhci: suspend done\n");
    return 0;
}

static int bce_vhci_bus_resume(struct usb_hcd *hcd)
{
    int i, j;
    int status;
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    pr_info("bce_vhci: resume started\n");

    bce_vhci_event_queue_resume(&vhci->ev_system);
    bce_vhci_event_queue_resume(&vhci->ev_isochronous);
    bce_vhci_event_queue_resume(&vhci->ev_interrupt);
    bce_vhci_event_queue_resume(&vhci->ev_asynchronous);
    bce_vhci_event_queue_resume(&vhci->ev_commands);

    pr_info("bce_vhci: resume controller\n");
    if ((status = bce_vhci_cmd_controller_start(&vhci->cq)))
        return status;

    pr_info("bce_vhci: resume ports\n");
    for (i = 0; i < 16; i++) {
        if (!vhci->port_to_device[i])
            continue;
        bce_vhci_cmd_port_resume(&vhci->cq, i);
    }
    pr_info("bce_vhci: resume endpoints\n");
    for (i = 0; i < 16; i++) {
        if (!vhci->port_to_device[i])
            continue;
        for (j = 0; j < 32; j++) {
            if (!(vhci->devices[vhci->port_to_device[i]]->tq_mask & BIT(j)))
                continue;
            bce_vhci_transfer_queue_resume(&vhci->devices[vhci->port_to_device[i]]->tq[j],
                    BCE_VHCI_PAUSE_SUSPEND);
        }
    }

    pr_info("bce_vhci: resume done\n");
    return 0;
}

static int bce_vhci_urb_enqueue(struct usb_hcd *hcd, struct urb *urb, gfp_t mem_flags)
{
    struct bce_vhci_transfer_queue *q = urb->ep->hcpriv;
    pr_debug("bce_vhci_urb_enqueue %i:%x\n", q->dev_addr, urb->ep->desc.bEndpointAddress);
    if (!q)
        return -ENOENT;
    return bce_vhci_urb_create(q, urb);
}

static int bce_vhci_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
    struct bce_vhci_transfer_queue *q = urb->ep->hcpriv;
    pr_info("bce_vhci_urb_dequeue %x\n", urb->ep->desc.bEndpointAddress);
    return bce_vhci_urb_request_cancel(q, urb, status);
}

static void bce_vhci_endpoint_reset(struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
    struct bce_vhci_transfer_queue *q = ep->hcpriv;
    pr_info("bce_vhci_endpoint_reset\n");
    if (q)
        bce_vhci_transfer_queue_request_reset(q);
}

static u8 bce_vhci_endpoint_index(u8 addr)
{
    if (addr & 0x80)
        return (u8) (0x10 + (addr & 0xf));
    return (u8) (addr & 0xf);
}

static int bce_vhci_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev, struct usb_host_endpoint *endp)
{
    u8 endp_index = bce_vhci_endpoint_index(endp->desc.bEndpointAddress);
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    bce_vhci_device_t devid = vhci->port_to_device[udev->portnum];
    struct bce_vhci_device *vdev = vhci->devices[devid];
    pr_info("bce_vhci_add_endpoint %x/%x:%x\n", udev->portnum, devid, endp_index);

    if (udev->bus->root_hub == udev) /* The USB hub */
        return 0;
    if (vdev == NULL)
        return -ENODEV;
    if (vdev->tq_mask & BIT(endp_index)) {
        endp->hcpriv = &vdev->tq[endp_index];
        return 0;
    }

    bce_vhci_create_transfer_queue(vhci, &vdev->tq[endp_index], endp, devid,
            usb_endpoint_dir_in(&endp->desc) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
    endp->hcpriv = &vdev->tq[endp_index];
    vdev->tq_mask |= BIT(endp_index);

    bce_vhci_cmd_endpoint_create(&vhci->cq, devid, &endp->desc);
    return 0;
}

static int bce_vhci_drop_endpoint(struct usb_hcd *hcd, struct usb_device *udev, struct usb_host_endpoint *endp)
{
    u8 endp_index = bce_vhci_endpoint_index(endp->desc.bEndpointAddress);
    struct bce_vhci *vhci = bce_vhci_from_hcd(hcd);
    bce_vhci_device_t devid = vhci->port_to_device[udev->portnum];
    struct bce_vhci_transfer_queue *q = endp->hcpriv;
    struct bce_vhci_device *vdev = vhci->devices[devid];
    pr_info("bce_vhci_drop_endpoint %x:%x\n", udev->portnum, endp_index);
    if (!q) {
        if (vdev && vdev->tq_mask & BIT(endp_index)) {
            pr_err("something deleted the hcpriv?\n");
            q = &vdev->tq[endp_index];
        } else {
            return 0;
        }
    }

    bce_vhci_cmd_endpoint_destroy(&vhci->cq, devid, (u8) (endp->desc.bEndpointAddress & 0x8Fu));
    vhci->devices[devid]->tq_mask &= ~BIT(endp_index);
    bce_vhci_destroy_transfer_queue(vhci, q);
    return 0;
}

static int bce_vhci_create_message_queues(struct bce_vhci *vhci)
{
    if (bce_vhci_message_queue_create(vhci, &vhci->msg_commands, "VHC1HostCommands") ||
        bce_vhci_message_queue_create(vhci, &vhci->msg_system, "VHC1HostSystemEvents") ||
        bce_vhci_message_queue_create(vhci, &vhci->msg_isochronous, "VHC1HostIsochronousEvents") ||
        bce_vhci_message_queue_create(vhci, &vhci->msg_interrupt, "VHC1HostInterruptEvents") ||
        bce_vhci_message_queue_create(vhci, &vhci->msg_asynchronous, "VHC1HostAsynchronousEvents")) {
        bce_vhci_destroy_message_queues(vhci);
        return -EINVAL;
    }
    spin_lock_init(&vhci->msg_asynchronous_lock);
    bce_vhci_command_queue_create(&vhci->cq, &vhci->msg_commands);
    return 0;
}

static void bce_vhci_destroy_message_queues(struct bce_vhci *vhci)
{
    bce_vhci_command_queue_destroy(&vhci->cq);
    bce_vhci_message_queue_destroy(vhci, &vhci->msg_commands);
    bce_vhci_message_queue_destroy(vhci, &vhci->msg_system);
    bce_vhci_message_queue_destroy(vhci, &vhci->msg_isochronous);
    bce_vhci_message_queue_destroy(vhci, &vhci->msg_interrupt);
    bce_vhci_message_queue_destroy(vhci, &vhci->msg_asynchronous);
}

static void bce_vhci_handle_system_event(struct bce_vhci_event_queue *q, struct bce_vhci_message *msg);
static void bce_vhci_handle_usb_event(struct bce_vhci_event_queue *q, struct bce_vhci_message *msg);

static int bce_vhci_create_event_queues(struct bce_vhci *vhci)
{
    vhci->ev_cq = bce_create_cq(vhci->dev, 0x100);
    if (!vhci->ev_cq)
        return -EINVAL;
#define CREATE_EVENT_QUEUE(field, name, cb) bce_vhci_event_queue_create(vhci, &vhci->field, name, cb)
    if (__bce_vhci_event_queue_create(vhci, &vhci->ev_commands, "VHC1FirmwareCommands",
            bce_vhci_firmware_event_completion) ||
        CREATE_EVENT_QUEUE(ev_system,       "VHC1FirmwareSystemEvents",       bce_vhci_handle_system_event) ||
        CREATE_EVENT_QUEUE(ev_isochronous,  "VHC1FirmwareIsochronousEvents",  bce_vhci_handle_usb_event) ||
        CREATE_EVENT_QUEUE(ev_interrupt,    "VHC1FirmwareInterruptEvents",    bce_vhci_handle_usb_event) ||
        CREATE_EVENT_QUEUE(ev_asynchronous, "VHC1FirmwareAsynchronousEvents", bce_vhci_handle_usb_event)) {
        bce_vhci_destroy_event_queues(vhci);
        return -EINVAL;
    }
#undef CREATE_EVENT_QUEUE
    return 0;
}

static void bce_vhci_destroy_event_queues(struct bce_vhci *vhci)
{
    bce_vhci_event_queue_destroy(vhci, &vhci->ev_commands);
    bce_vhci_event_queue_destroy(vhci, &vhci->ev_system);
    bce_vhci_event_queue_destroy(vhci, &vhci->ev_isochronous);
    bce_vhci_event_queue_destroy(vhci, &vhci->ev_interrupt);
    bce_vhci_event_queue_destroy(vhci, &vhci->ev_asynchronous);
    if (vhci->ev_cq)
        bce_destroy_cq(vhci->dev, vhci->ev_cq);
}

static void bce_vhci_send_fw_event_response(struct bce_vhci *vhci, struct bce_vhci_message *req, u16 status)
{
    unsigned long timeout = 1000;
    struct bce_vhci_message r = *req;
    r.cmd = (u16) (req->cmd | 0x8000u);
    r.status = status;
    r.param1 = req->param1;
    r.param2 = 0;

    if (bce_reserve_submission(vhci->msg_system.sq, &timeout)) {
        pr_err("bce-vhci: Cannot reserve submision for FW event reply\n");
        return;
    }
    bce_vhci_message_queue_write(&vhci->msg_system, &r);
}

static int bce_vhci_handle_firmware_event(struct bce_vhci *vhci, struct bce_vhci_message *msg)
{
    unsigned long flags;
    bce_vhci_device_t devid;
    u8 endp;
    struct bce_vhci_device *dev;
    struct bce_vhci_transfer_queue *tq;
    if (msg->cmd == BCE_VHCI_CMD_ENDPOINT_REQUEST_STATE || msg->cmd == BCE_VHCI_CMD_ENDPOINT_SET_STATE) {
        devid = (bce_vhci_device_t) (msg->param1 & 0xff);
        endp = bce_vhci_endpoint_index((u8) ((msg->param1 >> 8) & 0xff));
        dev = vhci->devices[devid];
        if (!dev || !(dev->tq_mask & BIT(endp)))
            return BCE_VHCI_BAD_ARGUMENT;
        tq = &dev->tq[endp];
    }

    if (msg->cmd == BCE_VHCI_CMD_ENDPOINT_REQUEST_STATE) {
        if (msg->param2 == BCE_VHCI_ENDPOINT_ACTIVE) {
            bce_vhci_transfer_queue_resume(tq, BCE_VHCI_PAUSE_FIRMWARE);
            return BCE_VHCI_SUCCESS;
        } else if (msg->param2 == BCE_VHCI_ENDPOINT_PAUSED) {
            bce_vhci_transfer_queue_pause(tq, BCE_VHCI_PAUSE_FIRMWARE);
            return BCE_VHCI_SUCCESS;
        }
        return BCE_VHCI_BAD_ARGUMENT;
    } else if (msg->cmd == BCE_VHCI_CMD_ENDPOINT_SET_STATE) {
        if (msg->param2 == BCE_VHCI_ENDPOINT_STALLED) {
            tq->state = msg->param2;
            spin_lock_irqsave(&tq->urb_lock, flags);
            tq->stalled = true;
            spin_unlock_irqrestore(&tq->urb_lock, flags);
            return BCE_VHCI_SUCCESS;
        }
        return BCE_VHCI_BAD_ARGUMENT;
    }
    pr_warn("bce-vhci: Unhandled firmware event: %x s=%x p1=%x p2=%llx\n",
            msg->cmd, msg->status, msg->param1, msg->param2);
    return BCE_VHCI_BAD_ARGUMENT;
}

static void bce_vhci_handle_firmware_events_w(struct work_struct *ws)
{
    size_t cnt = 0;
    int result;
    struct bce_vhci *vhci = container_of(ws, struct bce_vhci, w_fw_events);
    struct bce_queue_sq *sq = vhci->ev_commands.sq;
    struct bce_sq_completion_data *cq;
    struct bce_vhci_message *msg, *msg2 = NULL;

    while (true) {
        if (msg2) {
            msg = msg2;
            msg2 = NULL;
        } else if ((cq = bce_next_completion(sq))) {
            if (cq->status == BCE_COMPLETION_ABORTED) {
                bce_notify_submission_complete(sq);
                continue;
            }
            msg = &vhci->ev_commands.data[sq->head];
        } else {
            break;
        }

        pr_debug("bce-vhci: Got fw event: %x s=%x p1=%x p2=%llx\n", msg->cmd, msg->status, msg->param1, msg->param2);
        if ((cq = bce_next_completion(sq))) {
            msg2 = &vhci->ev_commands.data[(sq->head + 1) % sq->el_count];
            pr_debug("bce-vhci: Got second fw event: %x s=%x p1=%x p2=%llx\n",
                    msg->cmd, msg->status, msg->param1, msg->param2);
            if (cq->status != BCE_COMPLETION_ABORTED &&
                msg2->cmd == (msg->cmd | 0x4000) && msg2->param1 == msg->param1) {
                /* Take two elements */
                pr_debug("bce-vhci: Cancelled\n");
                bce_vhci_send_fw_event_response(vhci, msg, BCE_VHCI_ABORT);

                bce_notify_submission_complete(sq);
                bce_notify_submission_complete(sq);
                msg2 = NULL;
                cnt += 2;
                continue;
            }

            pr_warn("bce-vhci: Handle fw event - unexpected cancellation\n");
        }

        result = bce_vhci_handle_firmware_event(vhci, msg);
        bce_vhci_send_fw_event_response(vhci, msg, (u16) result);


        bce_notify_submission_complete(sq);
        ++cnt;
    }
    bce_vhci_event_queue_submit_pending(&vhci->ev_commands, cnt);
    if (atomic_read(&sq->available_commands) == sq->el_count - 1) {
        pr_debug("bce-vhci: complete\n");
        complete(&vhci->ev_commands.queue_empty_completion);
    }
}

static void bce_vhci_firmware_event_completion(struct bce_queue_sq *sq)
{
    struct bce_vhci_event_queue *q = sq->userdata;
    queue_work(q->vhci->tq_state_wq, &q->vhci->w_fw_events);
}

static void bce_vhci_handle_system_event(struct bce_vhci_event_queue *q, struct bce_vhci_message *msg)
{
    if (msg->cmd & 0x8000) {
        bce_vhci_command_queue_deliver_completion(&q->vhci->cq, msg);
    } else {
        pr_warn("bce-vhci: Unhandled system event: %x s=%x p1=%x p2=%llx\n",
                msg->cmd, msg->status, msg->param1, msg->param2);
    }
}

static void bce_vhci_handle_usb_event(struct bce_vhci_event_queue *q, struct bce_vhci_message *msg)
{
    bce_vhci_device_t devid;
    u8 endp;
    struct bce_vhci_device *dev;
    if (msg->cmd & 0x8000) {
        bce_vhci_command_queue_deliver_completion(&q->vhci->cq, msg);
    } else if (msg->cmd == BCE_VHCI_CMD_TRANSFER_REQUEST || msg->cmd == BCE_VHCI_CMD_CONTROL_TRANSFER_STATUS) {
        devid = (bce_vhci_device_t) (msg->param1 & 0xff);
        endp = bce_vhci_endpoint_index((u8) ((msg->param1 >> 8) & 0xff));
        dev = q->vhci->devices[devid];
        if (!dev || (dev->tq_mask & BIT(endp)) == 0) {
            pr_err("bce-vhci: Didn't find destination for transfer queue event\n");
            return;
        }
        bce_vhci_transfer_queue_event(&dev->tq[endp], msg);
    } else {
        pr_warn("bce-vhci: Unhandled USB event: %x s=%x p1=%x p2=%llx\n",
                msg->cmd, msg->status, msg->param1, msg->param2);
    }
}



static const struct hc_driver bce_vhci_driver = {
        .description = "bce-vhci",
        .product_desc = "BCE VHCI Host Controller",
        .hcd_priv_size = sizeof(struct bce_vhci *),

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,4,0)
        .flags = HCD_USB2,
#else
        .flags = HCD_USB2 | HCD_DMA,
#endif

        .start = bce_vhci_start,
        .stop = bce_vhci_stop,
        .hub_status_data = bce_vhci_hub_status_data,
        .hub_control = bce_vhci_hub_control,
        .urb_enqueue = bce_vhci_urb_enqueue,
        .urb_dequeue = bce_vhci_urb_dequeue,
        .enable_device = bce_vhci_enable_device,
        .free_dev = bce_vhci_free_device,
        .address_device = bce_vhci_address_device,
        .add_endpoint = bce_vhci_add_endpoint,
        .drop_endpoint = bce_vhci_drop_endpoint,
        .endpoint_reset = bce_vhci_endpoint_reset,
        .check_bandwidth = bce_vhci_check_bandwidth,
        .get_frame_number = bce_vhci_get_frame_number,
        .bus_suspend = bce_vhci_bus_suspend,
        .bus_resume = bce_vhci_bus_resume
};


int __init bce_vhci_module_init(void)
{
    int result;
    if ((result = alloc_chrdev_region(&bce_vhci_chrdev, 0, 1, "bce-vhci")))
        goto fail_chrdev;
    bce_vhci_class = class_create(THIS_MODULE, "bce-vhci");
    if (IS_ERR(bce_vhci_class)) {
        result = PTR_ERR(bce_vhci_class);
        goto fail_class;
    }
    return 0;

fail_class:
    class_destroy(bce_vhci_class);
fail_chrdev:
    unregister_chrdev_region(bce_vhci_chrdev, 1);
    if (!result)
        result = -EINVAL;
    return result;
}
void __exit bce_vhci_module_exit(void)
{
    class_destroy(bce_vhci_class);
    unregister_chrdev_region(bce_vhci_chrdev, 1);
}

module_param_named(vhci_port_mask, bce_vhci_port_mask, ushort, 0444);
MODULE_PARM_DESC(vhci_port_mask, "Specifies which VHCI ports are enabled");
