#ifndef BCE_VHCI_H
#define BCE_VHCI_H

#include "queue.h"
#include "transfer.h"

struct usb_hcd;
struct bce_queue_cq;

struct bce_vhci_device {
    struct bce_vhci_transfer_queue tq[32];
    u32 tq_mask;
};
struct bce_vhci {
    struct bce_device *dev;
    dev_t vdevt;
    struct device *vdev;
    struct usb_hcd *hcd;
    struct spinlock hcd_spinlock;
    struct bce_vhci_message_queue msg_commands;
    struct bce_vhci_message_queue msg_system;
    struct bce_vhci_message_queue msg_isochronous;
    struct bce_vhci_message_queue msg_interrupt;
    struct bce_vhci_message_queue msg_asynchronous;
    struct spinlock msg_asynchronous_lock;
    struct bce_vhci_command_queue cq;
    struct bce_queue_cq *ev_cq;
    struct bce_vhci_event_queue ev_commands;
    struct bce_vhci_event_queue ev_system;
    struct bce_vhci_event_queue ev_isochronous;
    struct bce_vhci_event_queue ev_interrupt;
    struct bce_vhci_event_queue ev_asynchronous;
    u16 port_mask;
    u8 port_count;
    u16 port_power_mask;
    bce_vhci_device_t port_to_device[16];
    struct bce_vhci_device *devices[16];
    struct workqueue_struct *tq_state_wq;
    struct work_struct w_fw_events;
};

int __init bce_vhci_module_init(void);
void __exit bce_vhci_module_exit(void);

int bce_vhci_create(struct bce_device *dev, struct bce_vhci *vhci);
void bce_vhci_destroy(struct bce_vhci *vhci);

#endif //BCE_VHCI_H
