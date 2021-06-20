// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - 2020 DisplayLink (UK) Ltd.
 */
#include "hcd.h"

#include "hpal_events.h"
#include "utils.h"

static void mausb_hcd_remove(struct device *dev);
static int mausb_bus_match(struct device *dev, struct device_driver *drv);

struct mausb_hcd	*mhcd;

static struct bus_type	mausb_bus_type = {
	.name	= DRIVER_NAME,
	.match	= mausb_bus_match,
};

static struct device_driver mausb_driver = {
	.name	= DRIVER_NAME,
	.owner	= THIS_MODULE,
	.bus	= &mausb_bus_type,
};

static struct device *mausb_device;

static void mausb_hcd_remove(struct device *dev)
{
	struct usb_hcd *hcd;
	struct usb_hcd *shared_hcd;

	shared_hcd = mhcd->hcd_ss_ctx->hcd;
	if (shared_hcd) {
		usb_remove_hcd(shared_hcd);
		usb_put_hcd(shared_hcd);
		mhcd->hcd_ss_ctx = NULL;
	}

	hcd = mhcd->hcd_hs_ctx->hcd;
	if (hcd) {
		usb_remove_hcd(hcd);
		usb_put_hcd(hcd);
		mhcd->hcd_hs_ctx = NULL;
	}

	kfree(mhcd);
	mhcd = NULL;
}

static int mausb_bus_match(struct device *dev, struct device_driver *drv)
{
	return !strncmp(dev->bus->name, drv->name, strlen(drv->name));
}

int mausb_host_driver_init(void)
{
	int retval = bus_register(&mausb_bus_type);

	if (retval)
		return retval;

	retval = driver_register(&mausb_driver);
	if (retval)
		goto cleanup_bus;

	mausb_device = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!mausb_device) {
		retval = -ENOMEM;
		goto cleanup_driver;
	}

	dev_set_name(mausb_device, DEVICE_NAME);
	mausb_device->bus = &mausb_bus_type;
	mausb_device->release = (void (*)(struct device *))kfree;

	retval = device_register(mausb_device);
	if (retval) {
		put_device(mausb_device);
		goto cleanup_driver;
	}

	retval = mausb_hcd_create_and_add(mausb_device);
	if (retval)
		goto cleanup;

	return retval;

cleanup:
	device_unregister(mausb_device);
cleanup_driver:
	driver_unregister(&mausb_driver);
cleanup_bus:
	bus_unregister(&mausb_bus_type);
	return retval;
}

void mausb_host_driver_deinit(void)
{
	mausb_hcd_remove(mausb_device);
	device_unregister(mausb_device);
	driver_unregister(&mausb_driver);
	bus_unregister(&mausb_bus_type);
}

void mausb_port_has_changed(const enum mausb_device_type device_type,
			    const enum mausb_device_speed device_speed,
			    void *ma_dev)
{
	struct usb_hcd *hcd;
	unsigned long flags;
	struct mausb_device *dev = ma_dev;
	u8 port_number = dev->port_number;

	spin_lock_irqsave(&mhcd->lock, flags);

	if (device_type == USB20HUB || device_speed < SUPER_SPEED) {
		mhcd->hcd_hs_ctx->ma_devs[port_number].port_status |=
		    USB_PORT_STAT_CONNECTION | (1 <<
						USB_PORT_FEAT_C_CONNECTION);

		if (device_speed == LOW_SPEED) {
			mhcd->hcd_hs_ctx->ma_devs[port_number].port_status |=
			    MAUSB_PORT_20_STATUS_LOW_SPEED;
			mhcd->hcd_hs_ctx->ma_devs[port_number].dev_speed =
			    LOW_SPEED;
		} else if (device_speed == HIGH_SPEED) {
			mhcd->hcd_hs_ctx->ma_devs[port_number].port_status |=
			    MAUSB_PORT_20_STATUS_HIGH_SPEED;
			mhcd->hcd_hs_ctx->ma_devs[port_number].dev_speed =
			    HIGH_SPEED;
		}

		hcd = mhcd->hcd_hs_ctx->hcd;
		mhcd->hcd_hs_ctx->ma_devs[port_number].ma_dev = ma_dev;
	} else {
		mhcd->hcd_ss_ctx->ma_devs[port_number].port_status |=
		    USB_PORT_STAT_CONNECTION | (1 <<
						USB_PORT_FEAT_C_CONNECTION);
		mhcd->hcd_ss_ctx->ma_devs[port_number].dev_speed = SUPER_SPEED;

		hcd = mhcd->hcd_ss_ctx->hcd;
		mhcd->hcd_ss_ctx->ma_devs[port_number].ma_dev = ma_dev;
	}
	spin_unlock_irqrestore(&mhcd->lock, flags);

	usb_hcd_poll_rh_status(hcd);
}

void mausb_hcd_disconnect(const u8 port_number,
			  const enum mausb_device_type device_type,
			  const enum mausb_device_speed device_speed)
{
	struct usb_hcd *hcd;
	unsigned long flags;

	if (port_number >= NUMBER_OF_PORTS) {
		dev_err(mausb_host_dev.this_device, "port number out of range, port_number=%x",
			port_number);
		return;
	}

	spin_lock_irqsave(&mhcd->lock, flags);

	if (device_type == USB20HUB || device_speed < SUPER_SPEED) {
		mhcd->hcd_hs_ctx->ma_devs[port_number].port_status &=
			~(USB_PORT_STAT_CONNECTION);
		mhcd->hcd_hs_ctx->ma_devs[port_number].port_status &=
			~(USB_PORT_STAT_ENABLE);
		mhcd->hcd_hs_ctx->ma_devs[port_number].port_status |=
			(1 << USB_PORT_FEAT_C_CONNECTION);
		hcd = mhcd->hcd_hs_ctx->hcd;
	} else {
		mhcd->hcd_ss_ctx->ma_devs[port_number].port_status &=
			~(USB_PORT_STAT_CONNECTION);
		mhcd->hcd_ss_ctx->ma_devs[port_number].port_status &=
			~(USB_PORT_STAT_ENABLE);
		mhcd->hcd_ss_ctx->ma_devs[port_number].port_status |=
			(1 << USB_PORT_FEAT_C_CONNECTION);
		hcd = mhcd->hcd_ss_ctx->hcd;
	}

	spin_unlock_irqrestore(&mhcd->lock, flags);
	if (!hcd)
		return;

	usb_hcd_poll_rh_status(hcd);
}

static const char driver_name[] = "MA-USB host controller";

static void mausb_get_hub_descriptor(struct usb_hcd *hcd, u16 type_req,
				     u16 value, u16 index,
				     char *buff, u16 length);
static void mausb_set_port_feature(struct usb_hcd *hcd, u16 type_req,
				   u16 value, u16 index, char *buff,
				   u16 length);
static void mausb_get_port_status(struct usb_hcd *hcd, u16 type_req,
				  u16 value, u16 index, char *buff,
				  u16 length);
static void mausb_clear_port_feature(struct usb_hcd *hcd, u16 type_req,
				     u16 value, u16 index,
				     char *buff, u16 length);
static void mausb_get_hub_status(struct usb_hcd *hcd, u16 type_req,
				 u16 value, u16 index, char *buff,
				 u16 length);
static int mausb_add_endpoint(struct usb_hcd *hcd, struct usb_device *dev,
			      struct usb_host_endpoint *endpoint);
static int mausb_address_device(struct usb_hcd *hcd, struct usb_device *dev);
static int mausb_alloc_dev(struct usb_hcd *hcd, struct usb_device *dev);
static int mausb_check_bandwidth(struct usb_hcd *hcd, struct usb_device *dev);
static void mausb_reset_bandwidth(struct usb_hcd *hcd, struct usb_device *dev);
static int mausb_drop_endpoint(struct usb_hcd *hcd, struct usb_device *dev,
			       struct usb_host_endpoint *endpoint);
static int mausb_enable_device(struct usb_hcd *hcd, struct usb_device *dev);
static void mausb_endpoint_disable(struct usb_hcd *hcd,
                                   struct usb_host_endpoint *endpoint);
static void mausb_endpoint_reset(struct usb_hcd *hcd,
				 struct usb_host_endpoint *endpoint);
static void mausb_free_dev(struct usb_hcd *hcd, struct usb_device *dev);
static int mausb_hcd_bus_resume(struct usb_hcd *hcd);
static int mausb_hcd_bus_suspend(struct usb_hcd *hcd);
static int mausb_hcd_get_frame_number(struct usb_hcd *hcd);
static int mausb_hcd_hub_control(struct usb_hcd *hcd, u16 type_req,
				 u16 value, u16 index, char *buff,
				 u16 length);
static int mausb_hcd_hub_status(struct usb_hcd *hcd, char *buff);
static int mausb_hcd_reset(struct usb_hcd *hcd);
static int mausb_hcd_start(struct usb_hcd *hcd);
static void mausb_hcd_stop(struct usb_hcd *hcd);
static int mausb_hcd_urb_dequeue(struct usb_hcd *hcd, struct urb *urb,
				 int status);
static int mausb_hcd_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
				 gfp_t mem_flags);
static int mausb_hub_update_device(struct usb_hcd *hcd, struct usb_device *dev,
				   struct usb_tt *tt, gfp_t mem_flags);
static int mausb_reset_device(struct usb_hcd *hcd, struct usb_device *dev);
static int mausb_update_device(struct usb_hcd *hcd, struct usb_device *dev);

static void mausb_print_urb(struct urb *request)
{
	dev_vdbg(&request->dev->dev, "URB: urb=%p, ep_handle=%#x, packet_num=%d, setup_dma=%pad, is_setup_packet=%d, is_ep=%d, is_sg=%d, num_sgs=%d, num_mapped_sgs=%d, status=%d, is_transfer_buffer=%d, transfer_buffer_length=%d, is_transfer_dma=%pad, transfer_flags=%d, is_hcpriv=%d",
		 request, ((struct mausb_endpoint_ctx *)
			   request->ep->hcpriv)->ep_handle,
		 request->number_of_packets, &request->setup_dma,
		 request->setup_packet ? 1 : 0, request->ep ? 1 : 0,
		 request->sg ? 1 : 0, request->num_sgs,
		 request->num_mapped_sgs, request->status,
		 request->transfer_buffer ? 1 : 0,
		 request->transfer_buffer_length,
		 &request->transfer_dma, request->transfer_flags,
		 (request->ep && request->ep->hcpriv) ? 1 : 0);
}

static const struct hc_driver mausb_hc_driver = {
	.description  = driver_name,
	.product_desc = driver_name,
	.flags	      = HCD_USB3 | HCD_SHARED,

	.hcd_priv_size = sizeof(struct hub_ctx),

	.reset = mausb_hcd_reset,
	.start = mausb_hcd_start,
	.stop = mausb_hcd_stop,

	.urb_enqueue = mausb_hcd_urb_enqueue,
	.urb_dequeue = mausb_hcd_urb_dequeue,

	.get_frame_number = mausb_hcd_get_frame_number,

	.hub_status_data   = mausb_hcd_hub_status,
	.hub_control	   = mausb_hcd_hub_control,
	.update_hub_device = mausb_hub_update_device,
	.bus_suspend	   = mausb_hcd_bus_suspend,
	.bus_resume	   = mausb_hcd_bus_resume,

	.alloc_dev	= mausb_alloc_dev,
	.free_dev	= mausb_free_dev,
	.enable_device	= mausb_enable_device,
	.update_device	= mausb_update_device,
	.reset_device	= mausb_reset_device,

	.add_endpoint	  = mausb_add_endpoint,
	.drop_endpoint	  = mausb_drop_endpoint,
	.check_bandwidth  = mausb_check_bandwidth,
	.reset_bandwidth  = mausb_reset_bandwidth,
	.address_device   = mausb_address_device,
	.endpoint_disable = mausb_endpoint_disable,
	.endpoint_reset	  = mausb_endpoint_reset,
};

static struct {
	struct usb_bos_descriptor    bos;
	struct usb_ss_cap_descriptor ss_cap;
} usb3_bos_desc = {
	.bos = {
		.bLength	 = USB_DT_BOS_SIZE,
		.bDescriptorType = USB_DT_BOS,
		.wTotalLength	 = cpu_to_le16(sizeof(usb3_bos_desc)),
		.bNumDeviceCaps	 = 1
	},
	.ss_cap = {
		.bLength		= USB_DT_USB_SS_CAP_SIZE,
		.bDescriptorType	= USB_DT_DEVICE_CAPABILITY,
		.bDevCapabilityType	= USB_SS_CAP_TYPE,
		.wSpeedSupported	= cpu_to_le16(USB_5GBPS_OPERATION),
		.bFunctionalitySupport	= ilog2(USB_5GBPS_OPERATION)
	}
};

static int get_root_hub_port_number(struct usb_device *dev, u8 *port_number)
{
	struct usb_device *first_hub_device = dev;

	if (!dev->parent) {
		(*port_number) = 0;
		return -EINVAL;
	}

	while (first_hub_device->parent->parent)
		first_hub_device = first_hub_device->parent;

	(*port_number) = first_hub_device->portnum - 1;

	return 0;
}

static int usb_to_mausb_device_speed(u8 speed)
{
	switch (speed) {
	case USB_SPEED_LOW:
		return MA_USB_SPEED_LOW_SPEED;
	case USB_SPEED_FULL:
		return MA_USB_SPEED_FULL_SPEED;
	case USB_SPEED_WIRELESS:
	case USB_SPEED_HIGH:
		return MA_USB_SPEED_HIGH_SPEED;
	case USB_SPEED_SUPER:
		return MA_USB_SPEED_SUPER_SPEED;
	default:
		return -EINVAL;
	}
}

static struct mausb_usb_device_ctx *mausb_find_usb_device(struct mausb_dev
							*mdevs, void *dev_addr)
{
	struct rb_node *node = mdevs->usb_devices.rb_node;

	while (node) {
		struct mausb_usb_device_ctx *usb_device =
		    rb_entry(node, struct mausb_usb_device_ctx, rb_node);

		if (dev_addr < usb_device->dev_addr)
			node = usb_device->rb_node.rb_left;
		else if (dev_addr > usb_device->dev_addr)
			node = usb_device->rb_node.rb_right;
		else
			return usb_device;
	}
	return NULL;
}

static int mausb_insert_usb_device(struct mausb_dev *mdevs,
				   struct mausb_usb_device_ctx *usb_device)
{
	struct rb_node **new_node = &mdevs->usb_devices.rb_node;
	struct rb_node *parent = NULL;
	struct mausb_usb_device_ctx *current_usb_device = NULL;

	while (*new_node) {
		parent = *new_node;
		current_usb_device = rb_entry(*new_node,
					      struct mausb_usb_device_ctx,
					      rb_node);

		if (usb_device->dev_addr < current_usb_device->dev_addr)
			new_node = &((*new_node)->rb_left);
		else if (usb_device->dev_addr > current_usb_device->dev_addr)
			new_node = &((*new_node)->rb_right);
		else
			return -EEXIST;
	}
	rb_link_node(&usb_device->rb_node, parent, new_node);
	rb_insert_color(&usb_device->rb_node, &mdevs->usb_devices);
	return 0;
}

static int mausb_hcd_get_frame_number(struct usb_hcd *hcd)
{
	return 0;
}

static int mausb_hcd_reset(struct usb_hcd *hcd)
{
	if (usb_hcd_is_primary_hcd(hcd)) {
		hcd->speed = HCD_USB2;
		hcd->self.root_hub->speed = USB_SPEED_HIGH;
	} else {
		hcd->speed = HCD_USB3;
		hcd->self.root_hub->speed = USB_SPEED_SUPER;
	}
	hcd->self.no_sg_constraint = 1;
	hcd->self.sg_tablesize = UINT_MAX;

	return 0;
}

static int mausb_hcd_start(struct usb_hcd *hcd)
{
	hcd->power_budget = 0;
	hcd->uses_new_polling = 1;
	return 0;
}

static void mausb_hcd_stop(struct usb_hcd *hcd)
{
	dev_vdbg(mausb_host_dev.this_device, "HCD stopped");
}

static int mausb_hcd_hub_status(struct usb_hcd *hcd, char *buff)
{
	int retval;
	int changed;
	int i;
	struct hub_ctx *hub;
	unsigned long flags;

	hub = (struct hub_ctx *)hcd->hcd_priv;

	retval  = DIV_ROUND_UP(NUMBER_OF_PORTS + 1, 8);
	changed = 0;

	memset(buff, 0, (unsigned int)retval);

	spin_lock_irqsave(&mhcd->lock, flags);

	if (!HCD_HW_ACCESSIBLE(hcd)) {
		dev_info(mausb_host_dev.this_device, "hcd not accessible, hcd speed=%d",
			 hcd->speed);
		spin_unlock_irqrestore(&mhcd->lock, flags);
		return 0;
	}

	for (i = 0; i < NUMBER_OF_PORTS; ++i) {
		if ((hub->ma_devs[i].port_status & PORT_C_MASK)) {
			buff[(i + 1) / 8] |= 1 << (i + 1) % 8;
			changed = 1;
		}
	}

	dev_info(mausb_host_dev.this_device, "Usb %d.0 : changed=%d, retval=%d",
		 (hcd->speed == HCD_USB2) ? 2 : 3, changed, retval);

	if (hcd->state == HC_STATE_SUSPENDED && changed == 1) {
		dev_info(mausb_host_dev.this_device, "hcd state is suspended");
		usb_hcd_resume_root_hub(hcd);
	}

	spin_unlock_irqrestore(&mhcd->lock, flags);

	return changed ? retval : 0;
}

static int mausb_hcd_bus_resume(struct usb_hcd *hcd)
{
	unsigned long flags;

	spin_lock_irqsave(&mhcd->lock, flags);
	if (!HCD_HW_ACCESSIBLE(hcd)) {
		dev_info(mausb_host_dev.this_device, "hcd not accessible, hcd speed=%d",
			 hcd->speed);
		spin_unlock_irqrestore(&mhcd->lock, flags);
		return -ESHUTDOWN;
	}
	hcd->state = HC_STATE_RUNNING;
	spin_unlock_irqrestore(&mhcd->lock, flags);

	return 0;
}

static int mausb_hcd_bus_suspend(struct usb_hcd *hcd)
{
	unsigned long flags;

	spin_lock_irqsave(&mhcd->lock, flags);
	hcd->state = HC_STATE_SUSPENDED;
	spin_unlock_irqrestore(&mhcd->lock, flags);

	return 0;
}

static int mausb_hcd_hub_control(struct usb_hcd *hcd, u16 type_req,
				 u16 value, u16 index, char *buff,
				 u16 length)
{
	int retval = 0;
	unsigned long	 flags;
	bool invalid_rhport = false;

	index = ((__u8)(index & 0x00ff));
	if (index < 1 || index > NUMBER_OF_PORTS)
		invalid_rhport = true;

	dev_vdbg(mausb_host_dev.this_device, "TypeReq=%d", type_req);

	spin_lock_irqsave(&mhcd->lock, flags);

	if (!HCD_HW_ACCESSIBLE(hcd)) {
		dev_err(mausb_host_dev.this_device, "hcd not accessible, hcd speed=%d",
			hcd->speed);
		spin_unlock_irqrestore(&mhcd->lock, flags);
		return -ETIMEDOUT;
	}

	switch (type_req) {
	case ClearHubFeature:
		break;
	case ClearPortFeature:
		if (invalid_rhport)
			goto invalid_port;

		mausb_clear_port_feature(hcd, type_req, value, index, buff,
					 length);
		break;
	case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
		memcpy(buff, &usb3_bos_desc, sizeof(usb3_bos_desc));
		retval = sizeof(usb3_bos_desc);
		break;
	case GetHubDescriptor:
		mausb_get_hub_descriptor(hcd, type_req, value, index, buff,
					 length);
		break;
	case GetHubStatus:
		mausb_get_hub_status(hcd, type_req, value, index, buff,
				     length);
		break;
	case GetPortStatus:
		if (invalid_rhport)
			goto invalid_port;

		mausb_get_port_status(hcd, type_req, value, index, buff,
				      length);
		break;
	case SetHubFeature:
		retval = -EPIPE;
		break;
	case SetPortFeature:
		if (invalid_rhport)
			goto invalid_port;

		mausb_set_port_feature(hcd, type_req, value, index, buff,
				       length);
		break;
	default:
		retval = -EPIPE;
	}

invalid_port:
	spin_unlock_irqrestore(&mhcd->lock, flags);
	return retval;
}

static int mausb_validate_urb(struct urb *urb)
{
	if (!urb) {
		dev_err(mausb_host_dev.this_device, "urb is NULL");
		return -EINVAL;
	}

	if (!urb->ep->hcpriv) {
		dev_err(mausb_host_dev.this_device, "urb->ep->hcpriv is NULL");
		return -EINVAL;
	}

	if (!urb->ep->enabled) {
		dev_err(mausb_host_dev.this_device, "Endpoint not enabled");
		return -EINVAL;
	}
	return 0;
}

static int mausb_hcd_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
				 gfp_t mem_flags)
{
	struct mausb_endpoint_ctx *endpoint_ctx;
	struct mausb_device	  *ma_dev;
	struct mausb_urb_ctx	  *urb_ctx;
	int status = 0;

	if (mausb_validate_urb(urb) < 0) {
		dev_err(&urb->dev->dev, "Hpal urb enqueue failed");
		return -EPROTO;
	}

	endpoint_ctx = urb->ep->hcpriv;
	ma_dev = endpoint_ctx->ma_dev;

	if (atomic_read(&ma_dev->unresponsive_client)) {
		dev_err(&urb->dev->dev, "Client is not responsive anymore - finish urb immediately");
		return -EHOSTDOWN;
	}

	urb->hcpriv = hcd;

	dev_vdbg(&urb->dev->dev, "ep_handle=%#x, dev_handle=%#x, urb_reject=%d",
		 endpoint_ctx->ep_handle, endpoint_ctx->dev_handle,
		 atomic_read(&urb->reject));

	status = mausb_insert_urb_in_tree(urb, true);
	if (status) {
		dev_err(&urb->dev->dev, "Hpal urb enqueue failed");
		return status;
	}

	atomic_inc(&urb->use_count);

	mausb_print_urb(urb);

	status = mausb_data_req_enqueue_event(ma_dev, endpoint_ctx->ep_handle,
					      urb);
	if (status < 0) {
		urb_ctx = mausb_unlink_and_delete_urb_from_tree(urb, status);
		atomic_dec(&urb->use_count);
		if (urb_ctx) {
			mausb_uninit_data_iterator(&urb_ctx->iterator);
			kfree(urb_ctx);
		}
	}

	return status;
}

static int mausb_hcd_urb_dequeue(struct usb_hcd *hcd, struct urb *urb,
				 int status)
{
	struct mausb_endpoint_ctx *endpoint_ctx;
	struct mausb_device	  *ma_dev;
	struct mausb_urb_ctx	  *urb_ctx;

	dev_info(&urb->dev->dev, "Urb=%p", urb);

	urb_ctx = mausb_unlink_and_delete_urb_from_tree(urb, status);
	if (!urb_ctx) {
		dev_warn(mausb_host_dev.this_device, "Urb=%p is not in tree",
			 urb);
		return 0;
	}

	endpoint_ctx = urb->ep->hcpriv;
	ma_dev	     = endpoint_ctx->ma_dev;

	queue_work(ma_dev->workq, &urb_ctx->work);

	return 0;
}

void mausb_hcd_urb_complete(struct urb *urb, u32 actual_length, int status)
{
	struct mausb_urb_ctx *urb_ctx =
		mausb_unlink_and_delete_urb_from_tree(urb, status);

	if (urb_ctx) {
		mausb_uninit_data_iterator(&urb_ctx->iterator);
		kfree(urb_ctx);

		urb->status	   = status;
		urb->actual_length = actual_length;

		atomic_dec(&urb->use_count);
		usb_hcd_giveback_urb(urb->hcpriv, urb, urb->status);
	}
}

int mausb_hcd_create_and_add(struct device *dev)
{
	int retval;
	struct usb_hcd	 *hcd_ss;
	struct usb_hcd	 *hcd_hs;

	hcd_hs = usb_create_hcd(&mausb_hc_driver, dev, dev_name(dev));
	if (!hcd_hs)
		return -ENOMEM;

	mhcd = kzalloc(sizeof(*mhcd), GFP_ATOMIC);
	if (!mhcd) {
		retval = -ENOMEM;
		goto cleanup_hcd_hs;
	}

	spin_lock_init(&mhcd->lock);

	hcd_hs->has_tt = 1;
	mhcd->hcd_hs_ctx = (struct hub_ctx *)hcd_hs->hcd_priv;
	mhcd->hcd_hs_ctx->hcd = hcd_hs;

	memset(mhcd->hcd_hs_ctx->ma_devs, 0,
	       sizeof(struct mausb_dev) * NUMBER_OF_PORTS);

	retval = usb_add_hcd(hcd_hs, 0, 0);
	if (retval) {
		dev_err(dev, "usb_add_hcd failed");
		goto cleanup_mhcd;
	}

	hcd_ss = usb_create_shared_hcd(&mausb_hc_driver, dev, dev_name(dev),
				       hcd_hs);
	if (!hcd_ss) {
		retval = -ENOMEM;
		goto remove_hcd_hs;
	}
	mhcd->hcd_ss_ctx = (struct hub_ctx *)hcd_ss->hcd_priv;
	mhcd->hcd_ss_ctx->hcd = hcd_ss;

	memset(mhcd->hcd_ss_ctx->ma_devs, 0,
	       sizeof(struct mausb_dev) * NUMBER_OF_PORTS);

	retval = usb_add_hcd(hcd_ss, 0, 0);
	if (retval) {
		dev_err(dev, "usb_add_hcd failed");
		goto cleanup;
	}

	return retval;

cleanup:
	usb_put_hcd(hcd_ss);
remove_hcd_hs:
	usb_remove_hcd(hcd_hs);
cleanup_mhcd:
	kfree(mhcd);
	mhcd = NULL;
cleanup_hcd_hs:
	usb_put_hcd(hcd_hs);
	return retval;
}

static void mausb_get_hub_descriptor(struct usb_hcd *hcd, u16 type_req,
				     u16 value, u16 index,
				     char *buff, u16 length)
{
	u8 width;
	struct usb_hub_descriptor *desc = (struct usb_hub_descriptor *)buff;

	memset(desc, 0, sizeof(struct usb_hub_descriptor));

	if (hcd->speed == HCD_USB3) {
		desc->bDescriptorType	   = USB_DT_SS_HUB;
		desc->bDescLength	   = 12;
		desc->wHubCharacteristics  =
		    cpu_to_le16(HUB_CHAR_INDV_PORT_LPSM | HUB_CHAR_COMMON_OCPM);
		desc->bNbrPorts		   = NUMBER_OF_PORTS;
		desc->u.ss.bHubHdrDecLat   = 0x04;
		desc->u.ss.DeviceRemovable = cpu_to_le16(0xffff);
	} else {
		desc->bDescriptorType	  = USB_DT_HUB;
		desc->wHubCharacteristics =
		    cpu_to_le16(HUB_CHAR_INDV_PORT_LPSM | HUB_CHAR_COMMON_OCPM);
		desc->bNbrPorts		  = NUMBER_OF_PORTS;
		width			  = (u8)(desc->bNbrPorts / 8 + 1);
		desc->bDescLength	  = USB_DT_HUB_NONVAR_SIZE + 2 * width;

		memset(&desc->u.hs.DeviceRemovable[0], 0, width);
		memset(&desc->u.hs.DeviceRemovable[width], 0xff, width);
	}
}

static void mausb_set_port_feature(struct usb_hcd *hcd, u16 type_req,
				   u16 value, u16 index, char *buff,
				   u16 length)
{
	struct hub_ctx *hub = (struct hub_ctx *)hcd->hcd_priv;

	index = ((__u8)(index & 0x00ff));

	switch (value) {
	case USB_PORT_FEAT_LINK_STATE:
		dev_vdbg(mausb_host_dev.this_device, "USB_PORT_FEAT_LINK_STATE");
		if (hcd->speed == HCD_USB3) {
			if ((hub->ma_devs[index - 1].port_status &
			     USB_SS_PORT_STAT_POWER) != 0) {
				hub->ma_devs[index - 1].port_status |=
				    (1U << value);
			}
		} else {
			if ((hub->ma_devs[index - 1].port_status &
			     USB_PORT_STAT_POWER) != 0) {
				hub->ma_devs[index - 1].port_status |=
				    (1U << value);
			}
		}
		break;
	case USB_PORT_FEAT_U1_TIMEOUT:
	case USB_PORT_FEAT_U2_TIMEOUT:
		break;
	case USB_PORT_FEAT_SUSPEND:
		break;
	case USB_PORT_FEAT_POWER:
		dev_vdbg(mausb_host_dev.this_device, "USB_PORT_FEAT_POWER");

		if (hcd->speed == HCD_USB3) {
			hub->ma_devs[index - 1].port_status |=
			    USB_SS_PORT_STAT_POWER;
		} else {
			hub->ma_devs[index - 1].port_status |=
			    USB_PORT_STAT_POWER;
		}
		break;
	case USB_PORT_FEAT_BH_PORT_RESET:
		dev_dbg(mausb_host_dev.this_device, "USB_PORT_FEAT_BH_PORT_RESET");
		/* fall through */
	case USB_PORT_FEAT_RESET:
		dev_vdbg(mausb_host_dev.this_device, "USB_PORT_FEAT_RESET hcd->speed=%d",
			 hcd->speed);

		if (hcd->speed == HCD_USB3) {
			hub->ma_devs[index - 1].port_status = 0;
			hub->ma_devs[index - 1].port_status =
			    (USB_SS_PORT_STAT_POWER |
			     USB_PORT_STAT_CONNECTION | USB_PORT_STAT_RESET);
		} else if (hub->ma_devs[index - 1].port_status
			   & USB_PORT_STAT_ENABLE) {
			hub->ma_devs[index - 1].port_status &=
			    ~(USB_PORT_STAT_ENABLE |
			      USB_PORT_STAT_LOW_SPEED |
			      USB_PORT_STAT_HIGH_SPEED);
		}
		/* fall through */
	default:
		dev_vdbg(mausb_host_dev.this_device, "Default value=%d", value);

		if (hcd->speed == HCD_USB3) {
			if ((hub->ma_devs[index - 1].port_status &
			     USB_SS_PORT_STAT_POWER) != 0) {
				hub->ma_devs[index - 1].port_status |=
				    (1U << value);
			}
		} else {
			if ((hub->ma_devs[index - 1].port_status &
			     USB_PORT_STAT_POWER) != 0) {
				hub->ma_devs[index - 1].port_status |=
				    (1U << value);
			}
		}
	}
}

static void mausb_get_port_status(struct usb_hcd *hcd, u16 type_req,
				  u16 value, u16 index, char *buff,
				  u16 length)
{
	u8 dev_speed;
	struct hub_ctx *hub = (struct hub_ctx *)hcd->hcd_priv;

	index = ((__u8)(index & 0x00ff));

	if ((hub->ma_devs[index - 1].port_status &
				(1 << USB_PORT_FEAT_RESET)) != 0) {
		dev_info(mausb_host_dev.this_device, "Finished reset hcd->speed=%d",
			 hcd->speed);

		dev_speed = hub->ma_devs[index - 1].dev_speed;
		switch (dev_speed) {
		case LOW_SPEED:
			hub->ma_devs[index - 1].port_status |=
			    USB_PORT_STAT_LOW_SPEED;
			break;
		case HIGH_SPEED:
			hub->ma_devs[index - 1].port_status |=
			    USB_PORT_STAT_HIGH_SPEED;
			break;
		default:
			dev_info(mausb_host_dev.this_device, "Not updating port_status for device speed %d",
				 dev_speed);
		}

		hub->ma_devs[index - 1].port_status |=
		    (1 << USB_PORT_FEAT_C_RESET) | USB_PORT_STAT_ENABLE;
		hub->ma_devs[index - 1].port_status &=
		    ~(1 << USB_PORT_FEAT_RESET);
	}

	((__le16 *)buff)[0] = cpu_to_le16(hub->ma_devs[index - 1].port_status);
	((__le16 *)buff)[1] =
	    cpu_to_le16(hub->ma_devs[index - 1].port_status >> 16);

	dev_vdbg(mausb_host_dev.this_device, "port_status=%d",
		 hub->ma_devs[index - 1].port_status);
}

static void mausb_clear_port_feature(struct usb_hcd *hcd, u16 type_req,
				     u16 value, u16 index,
				     char *buff, u16 length)
{
	struct hub_ctx *hub = (struct hub_ctx *)hcd->hcd_priv;

	index = ((__u8)(index & 0x00ff));

	switch (value) {
	case USB_PORT_FEAT_SUSPEND:
		break;
	case USB_PORT_FEAT_POWER:
		dev_vdbg(mausb_host_dev.this_device, "USB_PORT_FEAT_POWER");

		if (hcd->speed == HCD_USB3) {
			hub->ma_devs[index - 1].port_status &=
			    ~USB_SS_PORT_STAT_POWER;
		} else {
			hub->ma_devs[index - 1].port_status &=
			    ~USB_PORT_STAT_POWER;
		}
		break;
	case USB_PORT_FEAT_RESET:
	case USB_PORT_FEAT_C_RESET:
	default:
		dev_vdbg(mausb_host_dev.this_device, "Default value: %d",
			 value);

		hub->ma_devs[index - 1].port_status &= ~(1 << value);
	}
}

static void mausb_get_hub_status(struct usb_hcd *hcd, u16 type_req,
				 u16 value, u16 index, char *buff,
				 u16 length)
{
	*(__le32 *)buff = cpu_to_le32(0);
}

static int mausb_alloc_dev(struct usb_hcd *hcd, struct usb_device *dev)
{
	return 1;
}

static void mausb_free_dev(struct usb_hcd *hcd, struct usb_device *dev)
{
	u8	port_number;
	s16	dev_handle;
	int	status;
	struct hub_ctx   *hub  = (struct hub_ctx *)hcd->hcd_priv;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_device_ctx;
	struct mausb_endpoint_ctx   *ep_ctx = dev->ep0.hcpriv;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(mausb_host_dev.this_device, "port_number out of range, port_number=%x",
			port_number);
		return;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(mausb_host_dev.this_device, "MAUSB device not found on port_number=%d",
			port_number);
		return;
	}

	usb_device_ctx = mausb_find_usb_device(&hub->ma_devs[port_number], dev);
	if (!usb_device_ctx) {
		dev_warn(mausb_host_dev.this_device, "device_ctx is not found");
		return;
	}

	dev_handle = usb_device_ctx->dev_handle;

	if (atomic_read(&ma_dev->unresponsive_client)) {
		dev_err(mausb_host_dev.this_device, "Client is not responsive anymore - free usbdevice immediately");
		dev->ep0.hcpriv = NULL;
		kfree(ep_ctx);
		goto free_dev;
	}

	if (ep_ctx) {
		mausb_epinactivate_event_to_user(ma_dev, dev_handle,
						 ep_ctx->ep_handle);

		mausb_cleartransfers_event_to_user(ma_dev, dev_handle,
						   ep_ctx->ep_handle);

		mausb_epdelete_event_to_user(ma_dev, dev_handle,
					     ep_ctx->ep_handle);

		dev->ep0.hcpriv = NULL;
		kfree(ep_ctx);
	} else {
		dev_warn(mausb_host_dev.this_device, "ep_ctx is NULL: dev_handle=%#x",
			 dev_handle);
	}

	if (dev_handle != DEV_HANDLE_NOT_ASSIGNED)
		mausb_usbdevdisconnect_event_to_user(ma_dev, dev_handle);

free_dev:
	if (atomic_sub_and_test(1, &ma_dev->num_of_usb_devices)) {
		dev_info(mausb_host_dev.this_device, "All usb devices destroyed - proceed with disconnecting");
		mausb_devdisconnect_event_to_user(ma_dev);
		queue_work(ma_dev->workq, &ma_dev->socket_disconnect_work);
	}

	rb_erase(&usb_device_ctx->rb_node,
		 &hub->ma_devs[port_number].usb_devices);
	dev_info(mausb_host_dev.this_device, "USB device deleted device=%p",
		 usb_device_ctx->dev_addr);
	kfree(usb_device_ctx);

	if (kref_put(&ma_dev->refcount, mausb_release_ma_dev_async))
		mausb_clear_hcd_madev(port_number);
}

static int mausb_device_assign_address(struct mausb_device *dev,
				       struct mausb_usb_device_ctx *usb_dev_ctx)
{
	int status =
		mausb_setusbdevaddress_event_to_user(dev,
						     usb_dev_ctx->dev_handle,
						     RESPONSE_TIMEOUT_MS);

	usb_dev_ctx->addressed = (status == 0);

	return status;
}

static struct mausb_usb_device_ctx *
mausb_alloc_device_ctx(struct hub_ctx *hub, struct usb_device *dev,
		       struct mausb_device *ma_dev, u8 port_number,
		       int *status)
{
	struct mausb_usb_device_ctx *usb_device_ctx = NULL;

	usb_device_ctx = kzalloc(sizeof(*usb_device_ctx), GFP_ATOMIC);
	if (!usb_device_ctx) {
		*status = -ENOMEM;
		return NULL;
	}

	usb_device_ctx->dev_addr   = dev;
	usb_device_ctx->dev_handle = DEV_HANDLE_NOT_ASSIGNED;
	usb_device_ctx->addressed  = false;

	if (mausb_insert_usb_device(&hub->ma_devs[port_number],
				    usb_device_ctx)) {
		dev_warn(&dev->dev, "device_ctx already exists");
		kfree(usb_device_ctx);
		*status = -EEXIST;
		return NULL;
	}

	kref_get(&ma_dev->refcount);
	dev_info(&dev->dev, "New USB device added device=%p",
		 usb_device_ctx->dev_addr);
	atomic_inc(&ma_dev->num_of_usb_devices);

	return usb_device_ctx;
}

static int mausb_address_device(struct usb_hcd *hcd, struct usb_device *dev)
{
	u8	port_number;
	int	status;
	struct hub_ctx	*hub = (struct hub_ctx *)hcd->hcd_priv;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_device_ctx;
	struct mausb_endpoint_ctx   *endpoint_ctx;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_warn(&dev->dev, "port_number out of range, port_number=%x",
			 port_number);
		return -EINVAL;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_warn(&dev->dev, "MAUSB device not found on port_number=%d",
			 port_number);
		return -ENODEV;
	}

	usb_device_ctx = mausb_find_usb_device(&hub->ma_devs[port_number], dev);
	if (!usb_device_ctx) {
		usb_device_ctx = mausb_alloc_device_ctx(hub, dev, ma_dev,
							port_number, &status);
		if (!usb_device_ctx)
			return status;
	}

	dev_info(&dev->dev, "dev_handle=%#x, dev_speed=%#x",
		 usb_device_ctx->dev_handle, dev->speed);

	if (dev->speed >= USB_SPEED_SUPER)
		dev_info(&dev->dev, "USB 3.0");
	else
		dev_info(&dev->dev, "USB 2.0");

	if (usb_device_ctx->dev_handle == DEV_HANDLE_NOT_ASSIGNED) {
		status = mausb_enable_device(hcd, dev);
		if (status < 0)
			return status;
	}

	if (!usb_device_ctx->addressed) {
		status = mausb_device_assign_address(ma_dev, usb_device_ctx);
		if (status < 0)
			return status;
	}

	endpoint_ctx = dev->ep0.hcpriv;
	if (!endpoint_ctx) {
		dev_err(&dev->dev, "endpoint_ctx is NULL: dev_handle=%#x",
			usb_device_ctx->dev_handle);
		return -EINVAL;
	}

	if (dev->speed < USB_SPEED_SUPER && endpoint_ctx->ep_handle != 0)
		return 0;

	status = mausb_modifyep0_event_to_user(ma_dev,
					       usb_device_ctx->dev_handle,
					       &endpoint_ctx->ep_handle,
					       dev->ep0.desc.wMaxPacketSize);

	return status;
}

static int mausb_add_endpoint(struct usb_hcd *hcd, struct usb_device *dev,
			      struct usb_host_endpoint *endpoint)
{
	int	status;
	u8	port_number;
	struct ma_usb_ephandlereq_desc_ss  descriptor_ss;
	struct ma_usb_ephandlereq_desc_std descriptor;
	struct hub_ctx		    *hub = (struct hub_ctx *)hcd->hcd_priv;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_dev_ctx;
	struct mausb_endpoint_ctx   *endpoint_ctx;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(&dev->dev, "port_number out of range, port_number=%x",
			port_number);
		return 0;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(&dev->dev, "MAUSB device not found on port_number=%d",
			port_number);
		return -ENODEV;
	}

	usb_dev_ctx = mausb_find_usb_device(&hub->ma_devs[port_number], dev);
	if (!usb_dev_ctx) {
		dev_warn(&dev->dev, "Device not found");
		return -ENODEV;
	}

	endpoint_ctx = kzalloc(sizeof(*endpoint_ctx), GFP_ATOMIC);
	if (!endpoint_ctx)
		return -ENOMEM;

	endpoint_ctx->dev_handle	= usb_dev_ctx->dev_handle;
	endpoint_ctx->usb_device_ctx	= usb_dev_ctx;
	endpoint_ctx->ma_dev		= ma_dev;
	endpoint->hcpriv		= endpoint_ctx;

	if (dev->speed >= USB_SPEED_SUPER) {
		mausb_init_superspeed_ep_descriptor(&descriptor_ss,
						    &endpoint->desc,
						    &endpoint->ss_ep_comp);
		status = mausb_ephandle_event_to_user(ma_dev,
						      usb_dev_ctx->dev_handle,
						      sizeof(descriptor_ss),
						      &descriptor_ss,
						      &endpoint_ctx->ep_handle);

	} else {
		mausb_init_standard_ep_descriptor(&descriptor, &endpoint->desc);
		status = mausb_ephandle_event_to_user(ma_dev,
						      usb_dev_ctx->dev_handle,
						      sizeof(descriptor),
						      &descriptor,
						      &endpoint_ctx->ep_handle);
	}

	if (status < 0) {
		endpoint->hcpriv = NULL;
		kfree(endpoint_ctx);
		return status;
	}

	dev_info(&dev->dev, "Endpoint added ep_handle=%#x, dev_handle=%#x",
		 endpoint_ctx->ep_handle, endpoint_ctx->dev_handle);

	return 0;
}

static int mausb_drop_endpoint(struct usb_hcd *hcd, struct usb_device *dev,
			       struct usb_host_endpoint *endpoint)
{
	u8	port_number;
	int	status;
	int	retries	    = 0;
	struct hub_ctx		    *hub = (struct hub_ctx *)hcd->hcd_priv;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_dev_ctx;
	struct mausb_endpoint_ctx   *endpoint_ctx = endpoint->hcpriv;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(&dev->dev, "port_number out of range, port_number=%x",
			port_number);
		return -EINVAL;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(&dev->dev, "MAUSB device not found on port_number=%d",
			port_number);
		return -ENODEV;
	}

	usb_dev_ctx = mausb_find_usb_device(&hub->ma_devs[port_number], dev);

	if (!endpoint_ctx) {
		dev_err(&dev->dev, "Endpoint context doesn't exist");
		return 0;
	}
	if (!usb_dev_ctx) {
		dev_err(&dev->dev, "Usb device context doesn't exist");
		return -ENODEV;
	}

	dev_info(&dev->dev, "Start dropping ep_handle=%#x, dev_handle=%#x",
		 endpoint_ctx->ep_handle, endpoint_ctx->dev_handle);

	if (atomic_read(&ma_dev->unresponsive_client)) {
		dev_err(&dev->dev, "Client is not responsive anymore - drop endpoint immediately");
		endpoint->hcpriv = NULL;
		kfree(endpoint_ctx);
		return -ESHUTDOWN;
	}

	mausb_epinactivate_event_to_user(ma_dev, usb_dev_ctx->dev_handle,
					 endpoint_ctx->ep_handle);

	mausb_cleartransfers_event_to_user(ma_dev, usb_dev_ctx->dev_handle,
					   endpoint_ctx->ep_handle);

	while (true) {
		status = mausb_epdelete_event_to_user(ma_dev,
						      usb_dev_ctx->dev_handle,
						      endpoint_ctx->ep_handle);
		if (status == -EBUSY) {
			if (++retries < MAUSB_BUSY_RETRIES_COUNT)
				usleep_range(9000, 10000);
			else
				return -EBUSY;
		} else {
			break;
		}
	}

	dev_info(&dev->dev, "Endpoint dropped ep_handle=%#x, dev_handle=%#x",
		 endpoint_ctx->ep_handle, endpoint_ctx->dev_handle);

	endpoint->hcpriv = NULL;
	kfree(endpoint_ctx);
	return status;
}

static int mausb_device_assign_dev_handle(struct usb_hcd *hcd,
					  struct usb_device *dev,
					  struct hub_ctx *hub,
					  struct mausb_device *ma_dev,
					  struct mausb_usb_device_ctx
					  *usb_device_ctx)
{
	u8 port_number;
	int status;
	int dev_speed;
	u16 hub_dev_handle		= 0;
	u16 parent_hs_hub_dev_handle	= 0;
	u16 parent_hs_hub_port		= 0;
	struct usb_device		   *first_hub_device = dev;
	struct mausb_usb_device_ctx	   *hub_device_ctx;
	struct mausb_endpoint_ctx	   *endpoint_ctx;
	struct ma_usb_ephandlereq_desc_std descriptor;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(mausb_host_dev.this_device, "port_number out of range, port_number=%x",
			port_number);
		return -EINVAL;
	}

	while (first_hub_device->parent->parent)
		first_hub_device = first_hub_device->parent;

	hub_device_ctx = mausb_find_usb_device(&hub->ma_devs[port_number],
					       first_hub_device);
	if (hub_device_ctx)
		hub_dev_handle = hub_device_ctx->dev_handle;

	if ((dev->speed == USB_SPEED_LOW || dev->speed == USB_SPEED_FULL) &&
	    first_hub_device->speed == USB_SPEED_HIGH) {
		parent_hs_hub_dev_handle =
			mausb_find_usb_device(&hub->ma_devs[port_number],
					      dev->parent)->dev_handle;
		parent_hs_hub_port = dev->parent->portnum;
	}

	dev_speed = usb_to_mausb_device_speed(dev->speed);
	dev_info(mausb_host_dev.this_device, "start... mausb_devspeed=%d, route=%#x, port_number=%d",
		 dev_speed, dev->route, port_number);

	if (dev_speed == -EINVAL) {
		dev_err(mausb_host_dev.this_device, "bad dev_speed");
		return -EINVAL;
	}

	status = mausb_usbdevhandle_event_to_user(ma_dev, (u8)dev_speed,
						  dev->route, hub_dev_handle,
						  parent_hs_hub_dev_handle,
						  parent_hs_hub_port, 0,
						  ma_dev->lse,
						  &usb_device_ctx->dev_handle);
	if (status < 0)
		return status;

	dev_vdbg(mausb_host_dev.this_device, "dev_handle=%#x",
		 usb_device_ctx->dev_handle);

	endpoint_ctx = kzalloc(sizeof(*endpoint_ctx), GFP_ATOMIC);
	if (!endpoint_ctx)
		return -ENOMEM;

	endpoint_ctx->dev_handle     = usb_device_ctx->dev_handle;
	endpoint_ctx->ma_dev	     = ma_dev;
	endpoint_ctx->usb_device_ctx = usb_device_ctx;
	dev->ep0.hcpriv		     = endpoint_ctx;

	mausb_init_standard_ep_descriptor(&descriptor, &dev->ep0.desc);

	status = mausb_ephandle_event_to_user(ma_dev,
					      usb_device_ctx->dev_handle,
					      sizeof(descriptor),
					      &descriptor,
					      &endpoint_ctx->ep_handle);
	if (status < 0) {
		dev->ep0.hcpriv = NULL;
		kfree(endpoint_ctx);
		return status;
	}

	return 0;
}

static int mausb_enable_device(struct usb_hcd *hcd, struct usb_device *dev)
{
	int status;
	u8 port_number;
	struct hub_ctx		    *hub = (struct hub_ctx *)hcd->hcd_priv;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_device_ctx;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(mausb_host_dev.this_device, "port_number out of range, port_number=%x",
			port_number);
		return -EINVAL;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(mausb_host_dev.this_device, "MAUSB device not found on port_number=%d",
			port_number);
		return -ENODEV;
	}

	usb_device_ctx = mausb_find_usb_device(&hub->ma_devs[port_number], dev);
	if (!usb_device_ctx) {
		usb_device_ctx = mausb_alloc_device_ctx(hub, dev, ma_dev,
							port_number, &status);
		if (!usb_device_ctx)
			return status;
	}

	if (usb_device_ctx->dev_handle == DEV_HANDLE_NOT_ASSIGNED)
		return mausb_device_assign_dev_handle(hcd, dev, hub, ma_dev,
						      usb_device_ctx);

	if (!usb_device_ctx->addressed)
		return mausb_device_assign_address(ma_dev, usb_device_ctx);

	dev_vdbg(mausb_host_dev.this_device, "Device assigned and addressed, usb_device_ctx=%p, dev_handle=%#x",
		 usb_device_ctx, usb_device_ctx->dev_handle);

	return 0;
}

static int mausb_is_hub_device(struct usb_device *dev)
{
	return dev->descriptor.bDeviceClass == 0x09;
}

static int mausb_update_device(struct usb_hcd *hcd, struct usb_device *dev)
{
	u8	port_number = 0;
	int	status	    = 0;
	struct hub_ctx		    *hub = (struct hub_ctx *)hcd->hcd_priv;
	struct mausb_device	    *ma_dev = NULL;
	struct mausb_usb_device_ctx *usb_device_ctx = NULL;

	if (mausb_is_hub_device(dev)) {
		dev_warn(mausb_host_dev.this_device, "Device is hub");
		return 0;
	}

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(mausb_host_dev.this_device, "port_number out of range, port_number=%x",
			port_number);
		return -EINVAL;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(mausb_host_dev.this_device, "MAUSB device not found on port_number=%d",
			port_number);
		return -ENODEV;
	}

	usb_device_ctx = mausb_find_usb_device(&hub->ma_devs[port_number], dev);
	if (!usb_device_ctx) {
		dev_err(mausb_host_dev.this_device, "Device not found");
		return -ENODEV;
	}

	status = mausb_updatedev_event_to_user(ma_dev,
					       usb_device_ctx->dev_handle,
					       0, 0, 0, 0, 0, 0,
					       &dev->descriptor);

	dev_vdbg(mausb_host_dev.this_device, "Finished dev_handle=%#x, status=%d",
		 usb_device_ctx->dev_handle, status);

	return status;
}

static int mausb_hub_update_device(struct usb_hcd *hcd, struct usb_device *dev,
				   struct usb_tt *tt, gfp_t mem_flags)
{
	int	status;
	u8	port_number;
	u16 max_exit_latency = 0;
	u8  number_of_ports = (u8)dev->maxchild;
	u8  mtt = 0;
	u8  ttt = 0;
	u8  integrated_hub_latency = 0;
	struct hub_ctx		    *hub = (struct hub_ctx *)hcd->hcd_priv;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_device_ctx;

	if (dev->speed == USB_SPEED_HIGH) {
		mtt = tt->multi == 0 ? 1 : 0;
		ttt = (u8)tt->think_time;
	}

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(mausb_host_dev.this_device, "port_number out of range, port_number=%x",
			port_number);
		return 0;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(mausb_host_dev.this_device, "MAUSB device not found on port_number=%d",
			port_number);
		return -ENODEV;
	}

	usb_device_ctx = mausb_find_usb_device(&hub->ma_devs[port_number],
					       dev);
	if (!usb_device_ctx) {
		dev_err(mausb_host_dev.this_device, "USB device not found");
		return -ENODEV;
	}

	if (dev->usb3_lpm_u1_enabled)
		max_exit_latency = (u16)dev->u1_params.mel;
	else if (dev->usb3_lpm_u2_enabled)
		max_exit_latency = (u16)dev->u2_params.mel;

	status = mausb_updatedev_event_to_user(ma_dev,
					       usb_device_ctx->dev_handle,
					       max_exit_latency, 1,
					       number_of_ports, mtt, ttt,
					       integrated_hub_latency,
					       &dev->descriptor);

	dev_vdbg(mausb_host_dev.this_device, "Finished dev_handle=%#x, status=%d",
		 usb_device_ctx->dev_handle, status);

	return status;
}

static int mausb_check_bandwidth(struct usb_hcd *hcd, struct usb_device *dev)
{
	return 0;
}

static void mausb_reset_bandwidth(struct usb_hcd *hcd, struct usb_device *dev)
{
}

static void mausb_endpoint_disable(struct usb_hcd *hcd,
				   struct usb_host_endpoint *endpoint)
{
}

static void mausb_endpoint_reset(struct usb_hcd *hcd,
				 struct usb_host_endpoint *endpoint)
{
	int status;
	int is_control;
	int epnum;
	int is_out;
	u16	dev_handle;
	u8	tsp;
	u8	port_number;
	struct hub_ctx		    *hub;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_device_ctx;
	struct usb_device	    *dev;
	struct mausb_endpoint_ctx   *ep_ctx;
	struct ma_usb_ephandlereq_desc_ss  descriptor_ss;
	struct ma_usb_ephandlereq_desc_std descriptor;

	ep_ctx = endpoint->hcpriv;
	if (!ep_ctx)
		return;

	usb_device_ctx	= ep_ctx->usb_device_ctx;
	dev_handle	= usb_device_ctx->dev_handle;
	dev		= usb_device_ctx->dev_addr;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(&dev->dev, "port_number out of range, port_number=%x",
			port_number);
		return;
	}

	hub = (struct hub_ctx *)hcd->hcd_priv;

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(&dev->dev, "MAUSB device not found on port_number=%d",
			port_number);
		return;
	}

	is_control = usb_endpoint_xfer_control(&endpoint->desc);
	epnum = usb_endpoint_num(&endpoint->desc);
	is_out = usb_endpoint_dir_out(&endpoint->desc);
	tsp = (u8)(is_out ? dev->toggle[1] : dev->toggle[0]);

	status = mausb_epreset_event_to_user(ma_dev, dev_handle,
					     ep_ctx->ep_handle, tsp);
	if (status < 0)
		return;

	if (status != EUCLEAN) {
		if (!tsp) {
			usb_settoggle(dev, epnum, is_out, 0U);
			if (is_control)
				usb_settoggle(dev, epnum, !is_out, 0U);
		}

		mausb_epactivate_event_to_user(ma_dev, dev_handle,
					       ep_ctx->ep_handle);

		return;
	}

	if (tsp)
		return;

	status = mausb_epinactivate_event_to_user(ma_dev, dev_handle,
						  ep_ctx->ep_handle);
	if (status < 0)
		return;

	status = mausb_epdelete_event_to_user(ma_dev, dev_handle,
					      ep_ctx->ep_handle);
	if (status < 0)
		return;

	if (dev->speed >= USB_SPEED_SUPER) {
		mausb_init_superspeed_ep_descriptor(&descriptor_ss,
						    &endpoint->desc,
						    &endpoint->ss_ep_comp);
		mausb_ephandle_event_to_user(ma_dev, dev_handle,
					     sizeof(descriptor_ss),
					     &descriptor_ss,
					     &ep_ctx->ep_handle);
	} else {
		mausb_init_standard_ep_descriptor(&descriptor, &endpoint->desc);
		mausb_ephandle_event_to_user(ma_dev, dev_handle,
					     sizeof(descriptor), &descriptor,
					     &ep_ctx->ep_handle);
	}
}

static int mausb_reset_device(struct usb_hcd *hcd, struct usb_device *dev)
{
	int status;
	u8  port_number;
	u16 dev_handle;
	struct hub_ctx		    *hub;
	struct mausb_device	    *ma_dev;
	struct mausb_usb_device_ctx *usb_device_ctx;

	hub = (struct hub_ctx *)hcd->hcd_priv;

	status = get_root_hub_port_number(dev, &port_number);
	if (status < 0 || port_number >= NUMBER_OF_PORTS) {
		dev_dbg(mausb_host_dev.this_device, "port_number out of range, port_number=%x",
			port_number);
		return -EINVAL;
	}

	ma_dev = hub->ma_devs[port_number].ma_dev;
	if (!ma_dev) {
		dev_err(mausb_host_dev.this_device, "MAUSB device not found on port_number=%d",
			port_number);
		return -ENODEV;
	}

	usb_device_ctx = mausb_find_usb_device(&hub->ma_devs[port_number], dev);

	if (!usb_device_ctx ||
	    usb_device_ctx->dev_handle == DEV_HANDLE_NOT_ASSIGNED)
		return 0;

	dev_handle = usb_device_ctx->dev_handle;

	status = mausb_usbdevreset_event_to_user(ma_dev, dev_handle);
	if (status == 0)
		usb_device_ctx->addressed = false;

	return status;
}

void mausb_clear_hcd_madev(u8 port_number)
{
	unsigned long flags;

	spin_lock_irqsave(&mhcd->lock, flags);

	memset(&mhcd->hcd_hs_ctx->ma_devs[port_number], 0,
	       sizeof(struct mausb_dev));
	memset(&mhcd->hcd_ss_ctx->ma_devs[port_number], 0,
	       sizeof(struct mausb_dev));

	mhcd->connected_ports &= ~(1 << port_number);

	mhcd->hcd_hs_ctx->ma_devs[port_number].port_status =
							USB_PORT_STAT_POWER;
	mhcd->hcd_ss_ctx->ma_devs[port_number].port_status =
							USB_SS_PORT_STAT_POWER;

	spin_unlock_irqrestore(&mhcd->lock, flags);
}
