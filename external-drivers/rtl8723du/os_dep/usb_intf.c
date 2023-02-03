// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef _HCI_INTF_C_
#define _HCI_INTF_C_

#include <drv_types.h>
#include <hal_data.h>

#include <platform_ops.h>

extern int rtw_ht_enable;

int ui_pid[3] = {0, 0, 0};

extern int pm_netdev_open(struct net_device *pnetdev, u8 bnormal);
static int rtw_suspend(struct usb_interface *intf, pm_message_t message);
static int rtw_resume(struct usb_interface *intf);


static int rtw_drv_init(struct usb_interface *pusb_intf, const struct usb_device_id *pdid);
static void rtw_dev_remove(struct usb_interface *pusb_intf);

static void rtw_dev_shutdown(struct device *dev)
{
	struct usb_interface *usb_intf = container_of(dev, struct usb_interface, dev);
	struct dvobj_priv *dvobj = NULL;
	struct adapter *adapter = NULL;

	RTW_INFO("%s\n", __func__);

	if (usb_intf) {
		dvobj = usb_get_intfdata(usb_intf);
		if (dvobj) {
			adapter = dvobj_get_primary_adapter(dvobj);
			if (adapter) {
				if (!rtw_is_surprise_removed(adapter)) {
					rtw_btcoex_HaltNotify(adapter);
					rtw_hal_deinit(adapter);
					rtw_set_surprise_removed(adapter);
				}
			}
			ATOMIC_SET(&dvobj->continual_io_error, MAX_CONTINUAL_IO_ERR + 1);
		}
	}
}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 23))
/* Some useful macros to use to create struct usb_device_id */
#define USB_DEVICE_ID_MATCH_VENDOR			 0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT			 0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO			 0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI			 0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS			 0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS		 0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL		 0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS			 0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS		 0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL		 0x0200
#define USB_DEVICE_ID_MATCH_INT_NUMBER		 0x0400


#define USB_DEVICE_ID_MATCH_INT_INFO \
	(USB_DEVICE_ID_MATCH_INT_CLASS | \
	 USB_DEVICE_ID_MATCH_INT_SUBCLASS | \
	 USB_DEVICE_ID_MATCH_INT_PROTOCOL)


#define USB_DEVICE_AND_INTERFACE_INFO(vend, prod, cl, sc, pr) \
	.match_flags = USB_DEVICE_ID_MATCH_INT_INFO \
		       | USB_DEVICE_ID_MATCH_DEVICE, \
		       .idVendor = (vend), \
				   .idProduct = (prod), \
						.bInterfaceClass = (cl), \
						.bInterfaceSubClass = (sc), \
						.bInterfaceProtocol = (pr)

/**
 * USB_VENDOR_AND_INTERFACE_INFO - describe a specific usb vendor with a class of usb interfaces
 * @vend: the 16 bit USB Vendor ID
 * @cl: bInterfaceClass value
 * @sc: bInterfaceSubClass value
 * @pr: bInterfaceProtocol value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific vendor with a specific class of interfaces.
 *
 * This is especially useful when explicitly matching devices that have
 * vendor specific bDeviceClass values, but standards-compliant interfaces.
 */
#define USB_VENDOR_AND_INTERFACE_INFO(vend, cl, sc, pr) \
	.match_flags = USB_DEVICE_ID_MATCH_INT_INFO \
		       | USB_DEVICE_ID_MATCH_VENDOR, \
		       .idVendor = (vend), \
				   .bInterfaceClass = (cl), \
						   .bInterfaceSubClass = (sc), \
						   .bInterfaceProtocol = (pr)

/* ----------------------------------------------------------------------- */
#endif


#define USB_VENDER_ID_REALTEK		0x0BDA


/* DID_USB_v916_20130116 */
static struct usb_device_id rtw_usb_id_tbl[] = {
	/*=== Realtek demoboard ===*/
	{USB_DEVICE_AND_INTERFACE_INFO(USB_VENDER_ID_REALTEK, 0xD723, 0xff, 0xff, 0xff), .driver_info = RTL8723D}, /* 8723DU 1*1 */
	{USB_DEVICE_AND_INTERFACE_INFO(0x7392, 0xD611, 0xff, 0xff, 0xff), .driver_info = RTL8723D}, /* 8723DU 1*1 */

	{}	/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, rtw_usb_id_tbl);

static int const rtw_usb_id_len = sizeof(rtw_usb_id_tbl) / sizeof(struct usb_device_id);

static struct specific_device_id specific_device_id_tbl[] = {
	{.idVendor = USB_VENDER_ID_REALTEK, .idProduct = 0x8177, .flags = SPEC_DEV_ID_DISABLE_HT}, /* 8188cu 1*1 dongole, (b/g mode only) */
	{.idVendor = USB_VENDER_ID_REALTEK, .idProduct = 0x817E, .flags = SPEC_DEV_ID_DISABLE_HT}, /* 8188CE-VAU USB minCard (b/g mode only) */
	{.idVendor = 0x0b05, .idProduct = 0x1791, .flags = SPEC_DEV_ID_DISABLE_HT},
	{.idVendor = 0x13D3, .idProduct = 0x3311, .flags = SPEC_DEV_ID_DISABLE_HT},
	{.idVendor = 0x13D3, .idProduct = 0x3359, .flags = SPEC_DEV_ID_DISABLE_HT}, /* Russian customer -Azwave (8188CE-VAU  g mode) */
#ifdef RTK_DMP_PLATFORM
	{.idVendor = USB_VENDER_ID_REALTEK, .idProduct = 0x8111, .flags = SPEC_DEV_ID_ASSIGN_IFNAME}, /* Realtek 5G dongle for WiFi Display */
	{.idVendor = 0x2019, .idProduct = 0xAB2D, .flags = SPEC_DEV_ID_ASSIGN_IFNAME}, /* PCI-Abocom 5G dongle for WiFi Display */
#endif /* RTK_DMP_PLATFORM */
	{}
};

struct rtw_usb_drv {
	struct usb_driver usbdrv;
	int drv_registered;
	u8 hw_type;
};

static struct rtw_usb_drv usb_drv = {
	.usbdrv.name = (char *)DRV_NAME,
	.usbdrv.probe = rtw_drv_init,
	.usbdrv.disconnect = rtw_dev_remove,
	.usbdrv.id_table = rtw_usb_id_tbl,
	.usbdrv.suspend =  rtw_suspend,
	.usbdrv.resume = rtw_resume,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 22))
	.usbdrv.reset_resume   = rtw_resume,
#endif
#ifdef CONFIG_AUTOSUSPEND
	.usbdrv.supports_autosuspend = 1,
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19))
	.usbdrv.drvwrap.driver.shutdown = rtw_dev_shutdown,
#else
	.usbdrv.driver.shutdown = rtw_dev_shutdown,
#endif
};

static inline int RT_usb_endpoint_dir_in(const struct usb_endpoint_descriptor *epd)
{
	return (epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN;
}

static inline int RT_usb_endpoint_dir_out(const struct usb_endpoint_descriptor *epd)
{
	return (epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT;
}

static inline int RT_usb_endpoint_xfer_int(const struct usb_endpoint_descriptor *epd)
{
	return (epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT;
}

static inline int RT_usb_endpoint_xfer_bulk(const struct usb_endpoint_descriptor *epd)
{
	return (epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK;
}

static inline int RT_usb_endpoint_is_bulk_in(const struct usb_endpoint_descriptor *epd)
{
	return RT_usb_endpoint_xfer_bulk(epd) && RT_usb_endpoint_dir_in(epd);
}

static inline int RT_usb_endpoint_is_bulk_out(const struct usb_endpoint_descriptor *epd)
{
	return RT_usb_endpoint_xfer_bulk(epd) && RT_usb_endpoint_dir_out(epd);
}

static inline int RT_usb_endpoint_is_int_in(const struct usb_endpoint_descriptor *epd)
{
	return RT_usb_endpoint_xfer_int(epd) && RT_usb_endpoint_dir_in(epd);
}

static inline int RT_usb_endpoint_num(const struct usb_endpoint_descriptor *epd)
{
	return epd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}

static u8 rtw_init_intf_priv(struct dvobj_priv *dvobj)
{
	u8 rst = _SUCCESS;

	_rtw_mutex_init(&dvobj->usb_vendor_req_mutex);
	dvobj->usb_alloc_vendor_req_buf = rtw_zmalloc(MAX_USB_IO_CTL_SIZE);
	if (!dvobj->usb_alloc_vendor_req_buf) {
		RTW_INFO("alloc usb_vendor_req_buf failed... /n");
		rst = _FAIL;
		goto exit;
	}
	dvobj->usb_vendor_req_buf  =
		(u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(dvobj->usb_alloc_vendor_req_buf), ALIGNMENT_UNIT);
exit:

	return rst;
}

static u8 rtw_deinit_intf_priv(struct dvobj_priv *dvobj)
{
	u8 rst = _SUCCESS;

	if (dvobj->usb_vendor_req_buf)
		rtw_mfree(dvobj->usb_alloc_vendor_req_buf, MAX_USB_IO_CTL_SIZE);

	_rtw_mutex_free(&dvobj->usb_vendor_req_mutex);

	return rst;
}
static void rtw_decide_chip_type_by_usb_info(struct dvobj_priv *pdvobjpriv, const struct usb_device_id *pdid)
{
	pdvobjpriv->chip_type = pdid->driver_info;

	if (pdvobjpriv->chip_type == RTL8723D)
		rtl8723du_set_hw_type(pdvobjpriv);
}

static struct dvobj_priv *usb_dvobj_init(struct usb_interface *usb_intf, const struct usb_device_id *pdid)
{
	int	i;
	int	status = _FAIL;
	struct dvobj_priv *pdvobjpriv;
	struct usb_device_descriptor	*pdev_desc;
	struct usb_host_config			*phost_conf;
	struct usb_config_descriptor		*pconf_desc;
	struct usb_host_interface		*phost_iface;
	struct usb_interface_descriptor	*piface_desc;
	struct usb_host_endpoint		*phost_endp;
	struct usb_endpoint_descriptor	*pendp_desc;
	struct usb_device				*pusbd;



	pdvobjpriv = devobj_init();
	if (!pdvobjpriv)
		goto exit;


	pdvobjpriv->pusbintf = usb_intf ;
	pusbd = pdvobjpriv->pusbdev = interface_to_usbdev(usb_intf);
	usb_set_intfdata(usb_intf, pdvobjpriv);

	pdvobjpriv->RtNumInPipes = 0;
	pdvobjpriv->RtNumOutPipes = 0;

	/* adapt->EepromAddressSize = 6; */
	/* pdvobjpriv->nr_endpoint = 6; */

	pdev_desc = &pusbd->descriptor;

	phost_conf = pusbd->actconfig;
	pconf_desc = &phost_conf->desc;

	phost_iface = &usb_intf->altsetting[0];
	piface_desc = &phost_iface->desc;

	pdvobjpriv->NumInterfaces = pconf_desc->bNumInterfaces;
	pdvobjpriv->InterfaceNumber = piface_desc->bInterfaceNumber;
	pdvobjpriv->nr_endpoint = piface_desc->bNumEndpoints;

	for (i = 0; i < pdvobjpriv->nr_endpoint; i++) {
		phost_endp = phost_iface->endpoint + i;
		if (phost_endp) {
			pendp_desc = &phost_endp->desc;

			RTW_DBG("\nusb_endpoint_descriptor(%d):\n", i);
			RTW_DBG("bLength=%x\n", pendp_desc->bLength);
			RTW_DBG("bDescriptorType=%x\n", pendp_desc->bDescriptorType);
			RTW_DBG("bEndpointAddress=%x\n", pendp_desc->bEndpointAddress);
			/* RTW_DBG("bmAttributes=%x\n",pendp_desc->bmAttributes); */
			RTW_DBG("wMaxPacketSize=%d\n", le16_to_cpu(pendp_desc->wMaxPacketSize));
			RTW_DBG("bInterval=%x\n", pendp_desc->bInterval);
			/* RTW_DBG("bRefresh=%x\n",pendp_desc->bRefresh); */
			/* RTW_DBG("bSynchAddress=%x\n",pendp_desc->bSynchAddress); */

			if (RT_usb_endpoint_is_bulk_in(pendp_desc)) {
				RTW_DBG("RT_usb_endpoint_is_bulk_in = %x\n", RT_usb_endpoint_num(pendp_desc));
				pdvobjpriv->RtInPipe[pdvobjpriv->RtNumInPipes] = RT_usb_endpoint_num(pendp_desc);
				pdvobjpriv->RtNumInPipes++;
			} else if (RT_usb_endpoint_is_int_in(pendp_desc)) {
				RTW_DBG("RT_usb_endpoint_is_int_in = %x, Interval = %x\n", RT_usb_endpoint_num(pendp_desc), pendp_desc->bInterval);
				pdvobjpriv->RtInPipe[pdvobjpriv->RtNumInPipes] = RT_usb_endpoint_num(pendp_desc);
				pdvobjpriv->RtNumInPipes++;
			} else if (RT_usb_endpoint_is_bulk_out(pendp_desc)) {
				RTW_DBG("RT_usb_endpoint_is_bulk_out = %x\n", RT_usb_endpoint_num(pendp_desc));
				pdvobjpriv->RtOutPipe[pdvobjpriv->RtNumOutPipes] = RT_usb_endpoint_num(pendp_desc);
				pdvobjpriv->RtNumOutPipes++;
			}
			pdvobjpriv->ep_num[i] = RT_usb_endpoint_num(pendp_desc);
		}
	}

	RTW_INFO("nr_endpoint=%d, in_num=%d, out_num=%d\n\n", pdvobjpriv->nr_endpoint, pdvobjpriv->RtNumInPipes, pdvobjpriv->RtNumOutPipes);

	switch (pusbd->speed) {
	case USB_SPEED_LOW:
		RTW_INFO("USB_SPEED_LOW\n");
		pdvobjpriv->usb_speed = RTW_USB_SPEED_1_1;
		break;
	case USB_SPEED_FULL:
		RTW_INFO("USB_SPEED_FULL\n");
		pdvobjpriv->usb_speed = RTW_USB_SPEED_1_1;
		break;
	case USB_SPEED_HIGH:
		RTW_INFO("USB_SPEED_HIGH\n");
		pdvobjpriv->usb_speed = RTW_USB_SPEED_2;
		break;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31))
	case USB_SPEED_SUPER:
		RTW_INFO("USB_SPEED_SUPER\n");
		pdvobjpriv->usb_speed = RTW_USB_SPEED_3;
		break;
#endif
	default:
		RTW_INFO("USB_SPEED_UNKNOWN(%x)\n", pusbd->speed);
		pdvobjpriv->usb_speed = RTW_USB_SPEED_UNKNOWN;
		break;
	}

	if (pdvobjpriv->usb_speed == RTW_USB_SPEED_UNKNOWN) {
		RTW_INFO("UNKNOWN USB SPEED MODE, ERROR !!!\n");
		goto free_dvobj;
	}

	if (rtw_init_intf_priv(pdvobjpriv) == _FAIL) {
		goto free_dvobj;
	}

	/*step 1-1., decide the chip_type via driver_info*/
	pdvobjpriv->interface_type = RTW_USB;
	rtw_decide_chip_type_by_usb_info(pdvobjpriv, pdid);

	/* .3 misc */
	sema_init(&(pdvobjpriv->usb_suspend_sema), 0);
	rtw_reset_continual_io_error(pdvobjpriv);

	usb_get_dev(pusbd);

	status = _SUCCESS;

free_dvobj:
	if (status != _SUCCESS && pdvobjpriv) {
		usb_set_intfdata(usb_intf, NULL);

		devobj_deinit(pdvobjpriv);

		pdvobjpriv = NULL;
	}
exit:
	return pdvobjpriv;
}

static void usb_dvobj_deinit(struct usb_interface *usb_intf)
{
	struct dvobj_priv *dvobj = usb_get_intfdata(usb_intf);


	usb_set_intfdata(usb_intf, NULL);
	if (dvobj) {
		/* Modify condition for 92DU DMDP 2010.11.18, by Thomas */
		if ((dvobj->NumInterfaces != 2 && dvobj->NumInterfaces != 3)
		    || (dvobj->InterfaceNumber == 1)) {
			if (interface_to_usbdev(usb_intf)->state != USB_STATE_NOTATTACHED) {
				/* If we didn't unplug usb dongle and remove/insert modlue, driver fails on sitesurvey for the first time when device is up . */
				/* Reset usb port for sitesurvey fail issue. 2009.8.13, by Thomas */
				RTW_INFO("usb attached..., try to reset usb device\n");
				usb_reset_device(interface_to_usbdev(usb_intf));
			}
		}

		rtw_deinit_intf_priv(dvobj);

		devobj_deinit(dvobj);
	}

	/* RTW_INFO("%s %d\n", __func__, ATOMIC_READ(&usb_intf->dev.kobj.kref.refcount)); */
	usb_put_dev(interface_to_usbdev(usb_intf));

}

static int usb_reprobe_switch_usb_mode(struct adapter * Adapter)
{
	struct registry_priv *registry_par = &Adapter->registrypriv;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	u8 ret = false;

	/* efuse not allow driver to switch usb mode */
	if (!pHalData->EEPROMUsbSwitch)
		goto exit;

	/* registry not allow driver to switch usb mode */
	if (registry_par->switch_usb_mode == 0)
		goto exit;

	rtw_hal_set_hwreg(Adapter, HW_VAR_USB_MODE, &ret);

exit:
	return ret;
}

u8 rtw_set_hal_ops(struct adapter *adapt)
{
	/* alloc memory for HAL DATA */
	if (rtw_hal_data_init(adapt) == _FAIL)
		return _FAIL;

	if (rtw_get_chip_type(adapt) == RTL8723D)
		rtl8723du_set_hal_ops(adapt);

	if (_FAIL == rtw_hal_ops_check(adapt))
		return _FAIL;

	if (hal_spec_init(adapt) == _FAIL)
		return _FAIL;

	return _SUCCESS;
}

static void usb_intf_start(struct adapter *adapt)
{
	struct hal_com_data * hal = GET_HAL_DATA(adapt);

	rtw_hal_inirp_init(adapt);
	hal->usb_intf_start = true;


}

static void usb_intf_stop(struct adapter *adapt)
{
	struct hal_com_data * hal = GET_HAL_DATA(adapt);

	/* disabel_hw_interrupt */
	if (!rtw_is_surprise_removed(adapt)) {
		/* device still exists, so driver can do i/o operation */
		/* TODO: */
	}

	/* cancel in irp */
	rtw_hal_inirp_deinit(adapt);

	/* cancel out irp */
	rtw_write_port_cancel(adapt);

	/* todo:cancel other irps */

	hal->usb_intf_start = false;

}

static void process_spec_devid(const struct usb_device_id *pdid)
{
	u16 vid, pid;
	u32 flags;
	int i;
	int num = sizeof(specific_device_id_tbl) / sizeof(struct specific_device_id);

	for (i = 0; i < num; i++) {
		vid = specific_device_id_tbl[i].idVendor;
		pid = specific_device_id_tbl[i].idProduct;
		flags = specific_device_id_tbl[i].flags;

		if ((pdid->idVendor == vid) && (pdid->idProduct == pid) && (flags & SPEC_DEV_ID_DISABLE_HT)) {
			rtw_ht_enable = 0;
			rtw_bw_mode = 0;
			rtw_ampdu_enable = 0;
		}

#ifdef RTK_DMP_PLATFORM
		/* Change the ifname to wlan10 when PC side WFD dongle plugin on DMP platform. */
		/* It is used to distinguish between normal and PC-side wifi dongle/module. */
		if ((pdid->idVendor == vid) && (pdid->idProduct == pid) && (flags & SPEC_DEV_ID_ASSIGN_IFNAME)) {
			extern char *ifname;
			strncpy(ifname, "wlan10", 6);
			/* RTW_INFO("%s()-%d: ifname=%s, vid=%04X, pid=%04X\n", __func__, __LINE__, ifname, vid, pid); */
		}
#endif /* RTK_DMP_PLATFORM */

	}
}

#ifdef SUPPORT_HW_RFOFF_DETECTED
int rtw_hw_suspend(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrpriv;
	struct usb_interface *pusb_intf;
	struct net_device *pnetdev;

	if (!adapt)
		goto error_exit;

	if ((false == adapt->bup) || RTW_CANNOT_RUN(adapt)) {
		RTW_INFO("adapt->bup=%d bDriverStopped=%s bSurpriseRemoved = %s\n"
			 , adapt->bup
			 , rtw_is_drv_stopped(adapt) ? "True" : "False"
			, rtw_is_surprise_removed(adapt) ? "True" : "False");
		goto error_exit;
	}

	pwrpriv = adapter_to_pwrctl(adapt);
	pusb_intf = adapter_to_dvobj(adapt)->pusbintf;
	pnetdev = adapt->pnetdev;

	LeaveAllPowerSaveMode(adapt);

	RTW_INFO("==> rtw_hw_suspend\n");
	_enter_pwrlock(&pwrpriv->lock);
	pwrpriv->bips_processing = true;
	/* adapt->net_closed = true; */
	/* s1. */
	if (pnetdev) {
		rtw_netif_carrier_off(pnetdev);
		rtw_netif_stop_queue(pnetdev);
	}

	/* s2. */
	rtw_disassoc_cmd(adapt, 500, RTW_CMDF_DIRECTLY);

	/* s2-2.  indicate disconnect to os */
	/* rtw_indicate_disconnect(adapt); */
	{
		struct	mlme_priv *pmlmepriv = &adapt->mlmepriv;
		if (check_fwstate(pmlmepriv, _FW_LINKED)) {
			_clr_fwstate_(pmlmepriv, _FW_LINKED);
			rtw_led_control(adapt, LED_CTL_NO_LINK);

			rtw_os_indicate_disconnect(adapt, 0, false);

			/* donnot enqueue cmd */
			rtw_lps_ctrl_wk_cmd(adapt, LPS_CTRL_DISCONNECT, 0);
		}
	}
	/* s2-3. */
	rtw_free_assoc_resources(adapt, 1);

	/* s2-4. */
	rtw_free_network_queue(adapt, true);
	rtw_ips_dev_unload(adapt);
	pwrpriv->rf_pwrstate = rf_off;
	pwrpriv->bips_processing = false;
	up(&pwrpriv->lock);

	return 0;

error_exit:
	RTW_INFO("%s, failed\n", __func__);
	return -1;

}

int rtw_hw_resume(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	struct usb_interface *pusb_intf = adapter_to_dvobj(adapt)->pusbintf;
	struct net_device *pnetdev = adapt->pnetdev;

	RTW_INFO("==> rtw_hw_resume\n");
	_enter_pwrlock(&pwrpriv->lock);
	pwrpriv->bips_processing = true;
	rtw_reset_drv_sw(adapt);

	if (pm_netdev_open(pnetdev, false) != 0) {
		up(&pwrpriv->lock);
		goto error_exit;
	}
	rtw_netif_device_attach(pnetdev);
	rtw_netif_carrier_on(pnetdev);

	rtw_netif_wake_queue(pnetdev);

	pwrpriv->bkeepfwalive = false;
	pwrpriv->brfoffbyhw = false;

	pwrpriv->rf_pwrstate = rf_on;
	pwrpriv->bips_processing = false;
	up(&pwrpriv->lock);

	return 0;
error_exit:
	RTW_INFO("%s, Open net dev failed\n", __func__);
	return -1;
}
#endif

static int rtw_suspend(struct usb_interface *pusb_intf, pm_message_t message)
{
	struct dvobj_priv *dvobj;
	struct pwrctrl_priv *pwrpriv;
	struct debug_priv *pdbgpriv;
	struct adapter * adapt;
	int ret = 0;


	dvobj = usb_get_intfdata(pusb_intf);
	pwrpriv = dvobj_to_pwrctl(dvobj);
	pdbgpriv = &dvobj->drv_dbg;
	adapt = dvobj_get_primary_adapter(dvobj);

	if (pwrpriv->bInSuspend) {
		RTW_INFO("%s bInSuspend = %d\n", __func__, pwrpriv->bInSuspend);
		pdbgpriv->dbg_suspend_error_cnt++;
		goto exit;
	}

	if ((adapt->bup) || !rtw_is_drv_stopped(adapt) || !rtw_is_surprise_removed(adapt)) {
#ifdef CONFIG_AUTOSUSPEND
		if (pwrpriv->bInternalAutoSuspend) {

#ifdef SUPPORT_HW_RFOFF_DETECTED
			/* The FW command register update must after MAC and FW init ready. */
			if ((GET_HAL_DATA(adapt)->bFWReady) && (pwrpriv->bHWPwrPindetect) && (adapt->registrypriv.usbss_enable)) {
				u8 bOpen = true;
				rtw_interface_ps_func(adapt, HAL_USB_SELECT_SUSPEND, &bOpen);
			}
#endif/* SUPPORT_HW_RFOFF_DETECTED */
		}
#endif/* CONFIG_AUTOSUSPEND */
	}

	ret =  rtw_suspend_common(adapt);

exit:
	return ret;
}

static int rtw_resume_process(struct adapter *adapt)
{
	int ret;
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(adapt);
	struct dvobj_priv *pdvobj = adapt->dvobj;
	struct debug_priv *pdbgpriv = &pdvobj->drv_dbg;


	if (!pwrpriv->bInSuspend) {
		pdbgpriv->dbg_resume_error_cnt++;
		RTW_INFO("%s bInSuspend = %d\n", __func__, pwrpriv->bInSuspend);
		return -1;
	}

#if defined(CONFIG_AUTOSUSPEND) /* add by amy for 8723as-vau */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
	RTW_INFO("%s...pm_usage_cnt(%d)  pwrpriv->bAutoResume=%x.  ....\n", __func__, atomic_read(&(adapter_to_dvobj(adapt)->pusbintf->pm_usage_cnt)), pwrpriv->bAutoResume);
	pm_cnt = atomic_read(&(adapter_to_dvobj(adapt)->pusbintf->pm_usage_cnt));
#else /* kernel < 2.6.32 */
	RTW_INFO("...pm_usage_cnt(%d).....\n", adapter_to_dvobj(adapt)->pusbintf->pm_usage_cnt);
	pm_cnt = adapter_to_dvobj(adapt)->pusbintf->pm_usage_cnt;
#endif /* kernel < 2.6.32 */

	RTW_INFO("pwrpriv->bAutoResume (%x)\n", pwrpriv->bAutoResume);
	if (pwrpriv->bAutoResume) {
		pwrpriv->bInternalAutoSuspend = false;
		pwrpriv->bAutoResume = false;
		RTW_INFO("pwrpriv->bAutoResume (%x)  pwrpriv->bInternalAutoSuspend(%x)\n", pwrpriv->bAutoResume, pwrpriv->bInternalAutoSuspend);

	}
#endif /* #ifdef CONFIG_AUTOSUSPEND& */

	/*
	 * Due to usb wow suspend flow will cancel read/write port via intf_stop and
	 * bReadPortCancel and bWritePortCancel are set true in intf_stop.
	 * But they will not be clear in intf_start during wow resume flow.
	 * It should move to os_intf in the feature.
	 */
	RTW_ENABLE_FUNC(adapt, DF_RX_BIT);
	RTW_ENABLE_FUNC(adapt, DF_TX_BIT);

	ret =  rtw_resume_common(adapt);

#ifdef CONFIG_AUTOSUSPEND
	if (pwrpriv->bInternalAutoSuspend) {
#ifdef SUPPORT_HW_RFOFF_DETECTED
		/* The FW command register update must after MAC and FW init ready. */
		if ((GET_HAL_DATA(adapt)->bFWReady) && (pwrpriv->bHWPwrPindetect) && (adapt->registrypriv.usbss_enable)) {
			u8 bOpen = false;
			rtw_interface_ps_func(adapt, HAL_USB_SELECT_SUSPEND, &bOpen);
		}
#endif
		RTW_INFO("pwrpriv->bAutoResume (%x)\n", pwrpriv->bAutoResume);
		if (pwrpriv->bAutoResume) {
			pwrpriv->bInternalAutoSuspend = false;
			pwrpriv->bAutoResume = false;
			RTW_INFO("pwrpriv->bAutoResume (%x)  pwrpriv->bInternalAutoSuspend(%x)\n", pwrpriv->bAutoResume, pwrpriv->bInternalAutoSuspend);
		}

		pwrpriv->brfoffbyhw = false;
	}
#endif/* CONFIG_AUTOSUSPEND */


	return ret;
}

static int rtw_resume(struct usb_interface *pusb_intf)
{
	struct dvobj_priv *dvobj;
	struct pwrctrl_priv *pwrpriv;
	struct debug_priv *pdbgpriv;
	struct adapter * adapt;
	struct mlme_ext_priv *pmlmeext;
	int ret = 0;


	dvobj = usb_get_intfdata(pusb_intf);
	pwrpriv = dvobj_to_pwrctl(dvobj);
	pdbgpriv = &dvobj->drv_dbg;
	adapt = dvobj_get_primary_adapter(dvobj);
	pmlmeext = &adapt->mlmeextpriv;

	RTW_INFO("==> %s (%s:%d)\n", __func__, current->comm, current->pid);
	pdbgpriv->dbg_resume_cnt++;

	#ifdef CONFIG_AUTOSUSPEND
	if (pwrpriv->bInternalAutoSuspend)
		ret = rtw_resume_process(adapt);
	else 
	#endif
	{
		if (pwrpriv->wowlan_mode || pwrpriv->wowlan_ap_mode) {
			rtw_resume_lock_suspend();
			ret = rtw_resume_process(adapt);
			rtw_resume_unlock_suspend();
		} else {
			if (rtw_is_earlysuspend_registered(pwrpriv)) {
				/* jeff: bypass resume here, do in late_resume */
				rtw_set_do_late_resume(pwrpriv, true);
			} else {
				rtw_resume_lock_suspend();
				ret = rtw_resume_process(adapt);
				rtw_resume_unlock_suspend();
			}
		}
	}

	pmlmeext->last_scan_time = rtw_get_current_time();
	RTW_INFO("<========  %s return %d\n", __func__, ret);

	return ret;
}



#ifdef CONFIG_AUTOSUSPEND
void autosuspend_enter(struct adapter *adapt)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);

	RTW_INFO("==>autosuspend_enter...........\n");

	pwrpriv->bInternalAutoSuspend = true;
	pwrpriv->bips_processing = true;

	if (rf_off == pwrpriv->change_rfpwrstate) {
		if (1 == pwrpriv->autopm_cnt) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
			usb_enable_autosuspend(dvobj->pusbdev);
#else
			dvobj->pusbdev->autosuspend_disabled = 0;/* autosuspend disabled by the user */
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33))
			usb_autopm_put_interface(dvobj->pusbintf);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20))
			usb_autopm_enable(dvobj->pusbintf);
#else
			usb_autosuspend_device(dvobj->pusbdev, 1);
#endif
			pwrpriv->autopm_cnt--;
		} else
			RTW_INFO("0!=pwrpriv->autopm_cnt[%d]   didn't usb_autopm_put_interface\n", pwrpriv->autopm_cnt);
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
	RTW_INFO("...pm_usage_cnt(%d).....\n", atomic_read(&(dvobj->pusbintf->pm_usage_cnt)));
#else
	RTW_INFO("...pm_usage_cnt(%d).....\n", dvobj->pusbintf->pm_usage_cnt);
#endif

}

int autoresume_enter(struct adapter *adapt)
{
	int result = _SUCCESS;
	struct security_priv *psecuritypriv = &(adapt->securitypriv);
	struct mlme_ext_priv	*pmlmeext = &adapt->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapt);
	struct pwrctrl_priv *pwrpriv = dvobj_to_pwrctl(dvobj);

	RTW_INFO("====> autoresume_enter\n");

	if (rf_off == pwrpriv->rf_pwrstate) {
		pwrpriv->ps_flag = false;
		pwrpriv->bAutoResume = true;
		if (0 == pwrpriv->autopm_cnt) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33))
			if (usb_autopm_get_interface(dvobj->pusbintf) < 0) {
				RTW_INFO("can't get autopm: %d\n", result);
				result = _FAIL;
				goto error_exit;
			}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20))
			usb_autopm_disable(dvobj->pusbintf);
#else
			usb_autoresume_device(dvobj->pusbdev, 1);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
			RTW_INFO("...pm_usage_cnt(%d).....\n", atomic_read(&(dvobj->pusbintf->pm_usage_cnt)));
#else
			RTW_INFO("...pm_usage_cnt(%d).....\n", dvobj->pusbintf->pm_usage_cnt);
#endif
			pwrpriv->autopm_cnt++;
		} else
			RTW_INFO("0!=pwrpriv->autopm_cnt[%d]   didn't usb_autopm_get_interface\n", pwrpriv->autopm_cnt);
	}
	RTW_INFO("<==== autoresume_enter\n");
error_exit:

	return result;
}
#endif

/*
 * drv_init() - a device potentially for us
 *
 * notes: drv_init() is called when the bus driver has located a card for us to support.
 *        We accept the new device by returning 0.
*/

static struct adapter *rtw_usb_primary_adapter_init(struct dvobj_priv *dvobj,
	struct usb_interface *pusb_intf)
{
	struct adapter *adapt = NULL;
	int status = _FAIL;

	adapt = (struct adapter *)vzalloc(sizeof(*adapt));
	if (!adapt)
		goto exit;

	if (loadparam(adapt) != _SUCCESS)
		goto free_adapter;

	adapt->dvobj = dvobj;


	rtw_set_drv_stopped(adapt);/*init*/

	dvobj->adapters[dvobj->iface_nums++] = adapt;
	adapt->iface_id = IFACE_ID0;

	/* set adapter_type/iface type for primary adapt */
	adapt->isprimary = true;
	adapt->adapter_type = PRIMARY_ADAPTER;
#ifdef CONFIG_MI_WITH_MBSSID_CAM/*Configure all IFACE to PORT0-MBSSID*/
	adapt->hw_port = HW_PORT0;
#else
	adapt->hw_port = HW_PORT0;
#endif

	/* step init_io_priv */
	if (rtw_init_io_priv(adapt, usb_set_intf_ops) == _FAIL)
		goto free_adapter;

	/* step 2. hook HalFunc, allocate HalData */
	if (rtw_set_hal_ops(adapt) == _FAIL)
		goto free_hal_data;


	adapt->intf_start = &usb_intf_start;
	adapt->intf_stop = &usb_intf_stop;

	/* step read_chip_version */
	rtw_hal_read_chip_version(adapt);

	/* step usb endpoint mapping */
	rtw_hal_chip_configure(adapt);

	rtw_btcoex_Initialize(adapt);
	rtw_btcoex_wifionly_initialize(adapt);

	/* step read efuse/eeprom data and get mac_addr */
	if (rtw_hal_read_chip_info(adapt) == _FAIL)
		goto free_hal_data;

	/* step 5. */
	if (rtw_init_drv_sw(adapt) == _FAIL) {
		goto free_hal_data;
	}

#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18))
	if (dvobj_to_pwrctl(dvobj)->bSupportRemoteWakeup) {
		dvobj->pusbdev->do_remote_wakeup = 1;
		pusb_intf->needs_remote_wakeup = 1;
		device_init_wakeup(&pusb_intf->dev, 1);
		RTW_INFO("pwrctrlpriv.bSupportRemoteWakeup~~~~~~\n");
		RTW_INFO("pwrctrlpriv.bSupportRemoteWakeup~~~[%d]~~~\n", device_may_wakeup(&pusb_intf->dev));
	}
#endif
#endif

#ifdef CONFIG_AUTOSUSPEND
	if (adapt->registrypriv.power_mgnt != PS_MODE_ACTIVE) {
		if (adapt->registrypriv.usbss_enable) {	/* autosuspend (2s delay) */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38))
			dvobj->pusbdev->dev.power.autosuspend_delay = 0 * HZ;/* 15 * HZ; idle-delay time */
#else
			dvobj->pusbdev->autosuspend_delay = 0 * HZ;/* 15 * HZ; idle-delay time */
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
			usb_enable_autosuspend(dvobj->pusbdev);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 34))
			adapt->bDisableAutosuspend = dvobj->pusbdev->autosuspend_disabled ;
			dvobj->pusbdev->autosuspend_disabled = 0;/* autosuspend disabled by the user */
#endif

			/* usb_autopm_get_interface(adapter_to_dvobj(adapt)->pusbintf ); */ /* init pm_usage_cnt ,let it start from 1 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
			RTW_INFO("%s...pm_usage_cnt(%d).....\n", __func__, atomic_read(&(dvobj->pusbintf->pm_usage_cnt)));
#else
			RTW_INFO("%s...pm_usage_cnt(%d).....\n", __func__, dvobj->pusbintf->pm_usage_cnt);
#endif
		}
	}
#endif
	/* 2012-07-11 Move here to prevent the 8723AS-VAU BT auto suspend influence */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33))
	if (usb_autopm_get_interface(pusb_intf) < 0)
		RTW_INFO("can't get autopm:\n");
#endif
	dvobj_to_pwrctl(dvobj)->autopm_cnt = 1;

	/* set mac addr */
	rtw_macaddr_cfg(adapter_mac_addr(adapt), get_hal_mac_addr(adapt));
#ifdef CONFIG_MI_WITH_MBSSID_CAM
	rtw_mbid_camid_alloc(adapt, adapter_mac_addr(adapt));
#endif

	rtw_init_wifidirect_addrs(adapt, adapter_mac_addr(adapt), adapter_mac_addr(adapt));
	RTW_INFO("bDriverStopped:%s, bSurpriseRemoved:%s, bup:%d, hw_init_completed:%d\n"
		 , rtw_is_drv_stopped(adapt) ? "True" : "False"
		 , rtw_is_surprise_removed(adapt) ? "True" : "False"
		 , adapt->bup
		 , rtw_get_hw_init_completed(adapt)
		);

	status = _SUCCESS;

free_hal_data:
	if (status != _SUCCESS && adapt->HalData)
		rtw_hal_free_data(adapt);
free_adapter:
	if (status != _SUCCESS && adapt) {
		vfree(adapt);
		adapt = NULL;
	}
exit:
	return adapt;
}

static void rtw_usb_primary_adapter_deinit(struct adapter *adapt)
{
	struct pwrctrl_priv *pwrctl = adapter_to_pwrctl(adapt);
	struct mlme_priv *pmlmepriv = &adapt->mlmepriv;

	RTW_INFO(FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapt));

	if (check_fwstate(pmlmepriv, _FW_LINKED))
		rtw_disassoc_cmd(adapt, 0, RTW_CMDF_DIRECTLY);

	if (MLME_IS_AP(adapt) || MLME_IS_MESH(adapt)) {
		free_mlme_ap_info(adapt);
	}
	rtw_dev_unload(adapt);

	RTW_INFO("+r871xu_dev_remove, hw_init_completed=%d\n", rtw_get_hw_init_completed(adapt));

	if (1 == pwrctl->autopm_cnt) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33))
		usb_autopm_put_interface(adapter_to_dvobj(adapt)->pusbintf);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20))
		usb_autopm_enable(adapter_to_dvobj(adapt)->pusbintf);
#else
		usb_autosuspend_device(adapter_to_dvobj(adapt)->pusbdev, 1);
#endif
		pwrctl->autopm_cnt--;
	}

	rtw_free_drv_sw(adapt);

	/* TODO: use rtw_os_ndevs_deinit instead at the first stage of driver's dev deinit function */
	rtw_os_ndev_free(adapt);

	vfree(adapt);
}

static int rtw_drv_init(struct usb_interface *pusb_intf, const struct usb_device_id *pdid)
{
	struct adapter *adapt = NULL;
	int status = _FAIL;
	struct dvobj_priv *dvobj;
#ifdef CONFIG_CONCURRENT_MODE
	int i;
#endif

	/* step 0. */
	process_spec_devid(pdid);

	/* Initialize dvobj_priv */
	dvobj = usb_dvobj_init(pusb_intf, pdid);
	if (!dvobj)
		goto exit;
	adapt = rtw_usb_primary_adapter_init(dvobj, pusb_intf);
	if (!adapt) {
		RTW_INFO("rtw_usb_primary_adapter_init Failed!\n");
		goto free_dvobj;
	}

	if (usb_reprobe_switch_usb_mode(adapt))
		goto free_if_prim;

#ifdef CONFIG_CONCURRENT_MODE
	if (adapt->registrypriv.virtual_iface_num > (CONFIG_IFACE_NUMBER - 1))
		adapt->registrypriv.virtual_iface_num = (CONFIG_IFACE_NUMBER - 1);

	for (i = 0; i < adapt->registrypriv.virtual_iface_num; i++) {
		if (!rtw_drv_add_vir_if(adapt, usb_set_intf_ops)) {
			RTW_INFO("rtw_drv_add_iface failed! (%d)\n", i);
			goto free_if_vir;
		}
	}
#endif

	if (ui_pid[1] != 0) {
		RTW_INFO("ui_pid[1]:%d\n", ui_pid[1]);
		rtw_signal_process(ui_pid[1], SIGUSR2);
	}

	/* dev_alloc_name && register_netdev */
	if (rtw_os_ndevs_init(dvobj) != _SUCCESS)
		goto free_if_vir;

	status = _SUCCESS;

free_if_vir:
	if (status != _SUCCESS) {
		#ifdef CONFIG_CONCURRENT_MODE
		rtw_drv_stop_vir_ifaces(dvobj);
		rtw_drv_free_vir_ifaces(dvobj);
		#endif
	}

free_if_prim:
	if (status != _SUCCESS && adapt)
		rtw_usb_primary_adapter_deinit(adapt);

free_dvobj:
	if (status != _SUCCESS)
		usb_dvobj_deinit(pusb_intf);
exit:
	return status == _SUCCESS ? 0 : -ENODEV;
}

/*
 * dev_remove() - our device is being removed
*/
/* rmmod module & unplug(SurpriseRemoved) will call r871xu_dev_remove() => how to recognize both */
static void rtw_dev_remove(struct usb_interface *pusb_intf)
{
	struct dvobj_priv *dvobj = usb_get_intfdata(pusb_intf);
	struct adapter *adapt = dvobj_get_primary_adapter(dvobj);

	RTW_INFO("+rtw_dev_remove\n");

	dvobj->processing_dev_remove = true;

	/* TODO: use rtw_os_ndevs_deinit instead at the first stage of driver's dev deinit function */
	rtw_os_ndevs_unregister(dvobj);

	if (usb_drv.drv_registered) {
		/* RTW_INFO("r871xu_dev_remove():adapt->bSurpriseRemoved\n"); */
		rtw_set_surprise_removed(adapt);
	}
	if (GET_HAL_DATA(adapt)->bFWReady) {
		rtw_pm_set_ips(adapt, IPS_NONE);
		rtw_pm_set_lps(adapt, PS_MODE_ACTIVE);

		LeaveAllPowerSaveMode(adapt);
	}
	rtw_set_drv_stopped(adapt);	/*for stop thread*/
	rtw_stop_cmd_thread(adapt);
#ifdef CONFIG_CONCURRENT_MODE
	rtw_drv_stop_vir_ifaces(dvobj);
#endif /* CONFIG_CONCURRENT_MODE */

	rtw_btcoex_HaltNotify(adapt);

	rtw_usb_primary_adapter_deinit(adapt);

#ifdef CONFIG_CONCURRENT_MODE
	rtw_drv_free_vir_ifaces(dvobj);
#endif /* CONFIG_CONCURRENT_MODE */

	usb_dvobj_deinit(pusb_intf);

	RTW_INFO("-r871xu_dev_remove, done\n");
	return;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
extern int console_suspend_enabled;
#endif

static int __init rtw_drv_entry(void)
{
	int ret = 0;

	RTW_PRINT("module init start\n");
	dump_drv_version(RTW_DBGDUMP);
#ifdef BTCOEXVERSION
	RTW_PRINT(DRV_NAME" BT-Coex version = %s\n", BTCOEXVERSION);
#endif /* BTCOEXVERSION */

	ret = platform_wifi_power_on();
	if (ret != 0) {
		RTW_INFO("%s: power on failed!!(%d)\n", __func__, ret);
		ret = -1;
		goto exit;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24))
	/* console_suspend_enabled=0; */
#endif

	usb_drv.drv_registered = true;
	rtw_suspend_lock_init();
	rtw_drv_proc_init();
	rtw_ndev_notifier_register();
	rtw_inetaddr_notifier_register();

	ret = usb_register(&usb_drv.usbdrv);

	if (ret != 0) {
		usb_drv.drv_registered = false;
		rtw_suspend_lock_uninit();
		rtw_drv_proc_deinit();
		rtw_ndev_notifier_unregister();
		rtw_inetaddr_notifier_unregister();
		goto exit;
	}

exit:
	RTW_PRINT("module init ret=%d\n", ret);
	return ret;
}

static void __exit rtw_drv_halt(void)
{
	RTW_PRINT("module exit start\n");

	usb_drv.drv_registered = false;

	usb_deregister(&usb_drv.usbdrv);

	platform_wifi_power_off();

	rtw_suspend_lock_uninit();
	rtw_drv_proc_deinit();
	rtw_ndev_notifier_unregister();
	rtw_inetaddr_notifier_unregister();

	RTW_PRINT("module exit success\n");

	rtw_mstat_dump(RTW_DBGDUMP);
}

module_init(rtw_drv_entry);
module_exit(rtw_drv_halt);

#endif
