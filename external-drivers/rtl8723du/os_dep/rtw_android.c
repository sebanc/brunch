// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#include <drv_types.h>

#if defined(RTW_ENABLE_WIFI_CONTROL_FUNC)
#include <linux/platform_device.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	#include <linux/wlan_plat.h>
#else
	#include <linux/wifi_tiwlan.h>
#endif
#endif /* defined(RTW_ENABLE_WIFI_CONTROL_FUNC) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
#define strnicmp	strncasecmp
#endif /* Linux kernel >= 4.0.0 */

extern void macstr2num(u8 *dst, u8 *src);

static const char *android_wifi_cmd_str[ANDROID_WIFI_CMD_MAX] = {
	"START",
	"STOP",
	"SCAN-ACTIVE",
	"SCAN-PASSIVE",
	"RSSI",
	"LINKSPEED",
	"RXFILTER-START",
	"RXFILTER-STOP",
	"RXFILTER-ADD",
	"RXFILTER-REMOVE",
	"BTCOEXSCAN-START",
	"BTCOEXSCAN-STOP",
	"BTCOEXMODE",
	"SETSUSPENDOPT",
	"P2P_DEV_ADDR",
	"SETFWPATH",
	"SETBAND",
	"GETBAND",
	"COUNTRY",
	"P2P_SET_NOA",
	"P2P_GET_NOA",
	"P2P_SET_PS",
	"SET_AP_WPS_P2P_IE",
	"MIRACAST",
	"MACADDR",
	"BLOCK_SCAN",
	"BLOCK",
	"WFD-ENABLE",
	"WFD-DISABLE",
	"WFD-SET-TCPPORT",
	"WFD-SET-MAXTPUT",
	"WFD-SET-DEVTYPE",
	"SET_DTIM",
	"HOSTAPD_SET_MACADDR_ACL",
	"HOSTAPD_ACL_ADD_STA",
	"HOSTAPD_ACL_REMOVE_STA",
#if defined(CONFIG_GTK_OL) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0))
	"GTK_REKEY_OFFLOAD",
#endif /* CONFIG_GTK_OL */
/*	Private command for	P2P disable*/
	"P2P_DISABLE",
	"DRIVER_VERSION"
};

struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
};

#ifdef CONFIG_COMPAT
struct compat_android_wifi_priv_cmd {
	compat_uptr_t buf;
	int used_len;
	int total_len;
};
#endif /* CONFIG_COMPAT */

/**
 * Local (static) functions and variables
 */

/* Initialize g_wifi_on to 1 so dhd_bus_start will be called for the first
 * time (only) in dhd_open, subsequential wifi on will be handled by
 * wl_android_wifi_on
 */
static int g_wifi_on = true;

int rtw_android_cmdstr_to_num(char *cmdstr)
{
	int cmd_num;
	for (cmd_num = 0 ; cmd_num < ANDROID_WIFI_CMD_MAX; cmd_num++)
		if (0 == strnicmp(cmdstr , android_wifi_cmd_str[cmd_num], strlen(android_wifi_cmd_str[cmd_num])))
			break;

	return cmd_num;
}

static int rtw_android_get_rssi(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(net);
	struct	mlme_priv	*pmlmepriv = &(adapt->mlmepriv);
	struct	wlan_network	*pcur_network = &pmlmepriv->cur_network;
	int bytes_written = 0;

	if (check_fwstate(pmlmepriv, _FW_LINKED)) {
		bytes_written += snprintf(&command[bytes_written], total_len, "%s rssi %d",
			pcur_network->network.Ssid.Ssid, adapt->recvpriv.rssi);
	}

	return bytes_written;
}

static int rtw_android_get_link_speed(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(net);
	int bytes_written = 0;
	u16 link_speed = 0;

	link_speed = rtw_get_cur_max_rate(adapt) / 10;
	bytes_written = snprintf(command, total_len, "LinkSpeed %d", link_speed);

	return bytes_written;
}

static int rtw_android_get_macaddr(struct net_device *net, char *command, int total_len)
{
	return snprintf(command, total_len, "Macaddr = "MAC_FMT, MAC_ARG(net->dev_addr));
}

static int rtw_android_set_country(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(net);
	char *country_code = command + strlen(android_wifi_cmd_str[ANDROID_WIFI_CMD_COUNTRY]) + 1;
	int ret = _FAIL;

	ret = rtw_set_country(adapter, country_code);

	return (ret == _SUCCESS) ? 0 : -1;
}

static int rtw_android_get_p2p_dev_addr(struct net_device *net, char *command, int total_len)
{
	int bytes_written = 0;

	/* We use the same address as our HW MAC address */
	memcpy(command, net->dev_addr, ETH_ALEN);

	bytes_written = ETH_ALEN;
	return bytes_written;
}

static int rtw_android_set_block_scan(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(net);
	char *block_value = command + strlen(android_wifi_cmd_str[ANDROID_WIFI_CMD_BLOCK_SCAN]) + 1;

	adapter_wdev_data(adapter)->block_scan = (*block_value == '0') ? false : true;
	return 0;
}

static int rtw_android_set_block(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(net);
	char *block_value = command + strlen(android_wifi_cmd_str[ANDROID_WIFI_CMD_BLOCK]) + 1;

	adapter_wdev_data(adapter)->block = (*block_value == '0') ? false : true;
	return 0;
}

static int rtw_android_setband(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(net);
	char *arg = command + strlen(android_wifi_cmd_str[ANDROID_WIFI_CMD_SETBAND]) + 1;
	u32 band = WIFI_FREQUENCY_BAND_AUTO;
	int ret = _FAIL;

	if (sscanf(arg, "%u", &band) >= 1)
		ret = rtw_set_band(adapter, band);

	return (ret == _SUCCESS) ? 0 : -1;
}

static int rtw_android_getband(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(net);
	int bytes_written = 0;

	bytes_written = snprintf(command, total_len, "%u", adapter->setband);

	return bytes_written;
}

static int rtw_android_set_miracast_mode(struct net_device *net, char *command, int total_len)
{
	struct adapter *adapter = (struct adapter *)rtw_netdev_priv(net);
	struct wifi_display_info *wfd_info = &adapter->wfd_info;
	char *arg = command + strlen(android_wifi_cmd_str[ANDROID_WIFI_CMD_MIRACAST]) + 1;
	u8 mode;
	int num;
	int ret = _FAIL;

	num = sscanf(arg, "%hhu", &mode);

	if (num < 1)
		goto exit;

	switch (mode) {
	case 1: /* source */
		mode = MIRACAST_SOURCE;
		break;
	case 2: /* sink */
		mode = MIRACAST_SINK;
		break;
	case 0: /* disabled */
	default:
		mode = MIRACAST_DISABLED;
		break;
	}
	wfd_info->stack_wfd_mode = mode;
	RTW_INFO("stack miracast mode: %s\n", get_miracast_mode_str(wfd_info->stack_wfd_mode));

	ret = _SUCCESS;

exit:
	return (ret == _SUCCESS) ? 0 : -1;
}

static int get_int_from_command(char *pcmd)
{
	int i = 0;

	for (i = 0; i < strlen(pcmd); i++) {
		if (pcmd[i] == '=') {
			/*	Skip the '=' and space characters. */
			i += 2;
			break;
		}
	}
	return rtw_atoi(pcmd + i) ;
}

#if defined(CONFIG_GTK_OL) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0))
int rtw_gtk_offload(struct net_device *net, u8 *cmd_ptr)
{
	int i;
	/* u8 *cmd_ptr = priv_cmd.buf; */
	struct sta_info *psta;
	struct adapter *adapt = (struct adapter *)rtw_netdev_priv(net);
	struct mlme_priv	*pmlmepriv = &adapt->mlmepriv;
	struct sta_priv *pstapriv = &adapt->stapriv;
	struct security_priv *psecuritypriv = &(adapt->securitypriv);
	psta = rtw_get_stainfo(pstapriv, get_bssid(pmlmepriv));


	if (!psta) {
		RTW_INFO("%s, : Obtain Sta_info fail\n", __func__);
	} else {
		/* string command length of "GTK_REKEY_OFFLOAD" */
		cmd_ptr += 18;

		memcpy(psta->kek, cmd_ptr, RTW_KEK_LEN);
		cmd_ptr += RTW_KEK_LEN;
		/*
		printk("supplicant KEK: ");
		for(i=0;i<RTW_KEK_LEN; i++)
			printk(" %02x ", psta->kek[i]);
		printk("\n supplicant KCK: ");
		*/
		memcpy(psta->kck, cmd_ptr, RTW_KCK_LEN);
		cmd_ptr += RTW_KCK_LEN;
		/*
		for(i=0;i<RTW_KEK_LEN; i++)
			printk(" %02x ", psta->kck[i]);
		*/
		memcpy(psta->replay_ctr, cmd_ptr, RTW_REPLAY_CTR_LEN);
		psecuritypriv->binstallKCK_KEK = true;

		/* printk("\nREPLAY_CTR: "); */
		/* for(i=0;i<RTW_REPLAY_CTR_LEN; i++) */
		/* printk(" %02x ", psta->replay_ctr[i]); */
	}

	return _SUCCESS;
}
#endif /* CONFIG_GTK_OL */

int rtw_android_priv_cmd(struct net_device *net, struct ifreq *ifr, int cmd)
{
	int ret = 0;
	char *command = NULL;
	int cmd_num;
	int bytes_written = 0;
	struct android_wifi_priv_cmd priv_cmd;
	struct adapter	*adapt = (struct adapter *) rtw_netdev_priv(net);
	struct wifi_display_info		*pwfd_info;

	rtw_lock_suspend();

	if (!ifr->ifr_data) {
		ret = -EINVAL;
		goto exit;
	}
	if (adapt->registrypriv.mp_mode == 1) {
		ret = -EINVAL;
		goto exit;
	}
#ifdef CONFIG_COMPAT
#if (KERNEL_VERSION(4, 6, 0) > LINUX_VERSION_CODE)
	if (is_compat_task()) {
#else
	if (in_compat_syscall()) {
#endif
		/* User space is 32-bit, use compat ioctl */
		struct compat_android_wifi_priv_cmd compat_priv_cmd;

		if (copy_from_user(&compat_priv_cmd, ifr->ifr_data, sizeof(struct compat_android_wifi_priv_cmd))) {
			ret = -EFAULT;
			goto exit;
		}
		priv_cmd.buf = compat_ptr(compat_priv_cmd.buf);
		priv_cmd.used_len = compat_priv_cmd.used_len;
		priv_cmd.total_len = compat_priv_cmd.total_len;
	} else
#endif /* CONFIG_COMPAT */
		if (copy_from_user(&priv_cmd, ifr->ifr_data, sizeof(struct android_wifi_priv_cmd))) {
			ret = -EFAULT;
			goto exit;
		}
	if (adapt->registrypriv.mp_mode == 1) {
		ret = -EFAULT;
		goto exit;
	}
	/*RTW_INFO("%s priv_cmd.buf=%p priv_cmd.total_len=%d  priv_cmd.used_len=%d\n",__func__,priv_cmd.buf,priv_cmd.total_len,priv_cmd.used_len);*/
	command = rtw_zmalloc(priv_cmd.total_len);
	if (!command) {
		RTW_INFO("%s: failed to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
	if (!access_ok(priv_cmd.buf, priv_cmd.total_len)) {
#else
	if (!access_ok(VERIFY_READ, priv_cmd.buf, priv_cmd.total_len)) {
#endif
		RTW_INFO("%s: failed to access memory\n", __func__);
		ret = -EFAULT;
		goto exit;
	}
	if (copy_from_user(command, (void *)priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto exit;
	}

	RTW_INFO("%s: Android private cmd \"%s\" on %s\n"
		 , __func__, command, ifr->ifr_name);

	cmd_num = rtw_android_cmdstr_to_num(command);

	switch (cmd_num) {
	case ANDROID_WIFI_CMD_START:
		/* bytes_written = wl_android_wifi_on(net); */
		goto response;
	case ANDROID_WIFI_CMD_SETFWPATH:
		goto response;
	}

	if (!g_wifi_on) {
		RTW_INFO("%s: Ignore private cmd \"%s\" - iface %s is down\n"
			 , __func__, command, ifr->ifr_name);
		ret = 0;
		goto exit;
	}

	if (!hal_chk_wl_func(adapt, WL_FUNC_MIRACAST)) {
		switch (cmd_num) {
		case ANDROID_WIFI_CMD_WFD_ENABLE:
		case ANDROID_WIFI_CMD_WFD_DISABLE:
		case ANDROID_WIFI_CMD_WFD_SET_TCPPORT:
		case ANDROID_WIFI_CMD_WFD_SET_MAX_TPUT:
		case ANDROID_WIFI_CMD_WFD_SET_DEVTYPE:
			goto response;
		}
	}

	switch (cmd_num) {

	case ANDROID_WIFI_CMD_STOP:
		/* bytes_written = wl_android_wifi_off(net); */
		break;

	case ANDROID_WIFI_CMD_SCAN_ACTIVE:
		break;
	case ANDROID_WIFI_CMD_SCAN_PASSIVE:
		break;
	case ANDROID_WIFI_CMD_RSSI:
		bytes_written = rtw_android_get_rssi(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_LINKSPEED:
		bytes_written = rtw_android_get_link_speed(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_MACADDR:
		bytes_written = rtw_android_get_macaddr(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_BLOCK_SCAN:
		bytes_written = rtw_android_set_block_scan(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_BLOCK:
		bytes_written = rtw_android_set_block(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_RXFILTER_START:
		break;
	case ANDROID_WIFI_CMD_RXFILTER_STOP:
		/* bytes_written = net_os_set_packet_filter(net, 0); */
		break;
	case ANDROID_WIFI_CMD_RXFILTER_ADD:
		/* int filter_num = *(command + strlen(CMD_RXFILTER_ADD) + 1) - '0'; */
		/* bytes_written = net_os_rxfilter_add_remove(net, true, filter_num); */
		break;
	case ANDROID_WIFI_CMD_RXFILTER_REMOVE:
		/* int filter_num = *(command + strlen(CMD_RXFILTER_REMOVE) + 1) - '0'; */
		/* bytes_written = net_os_rxfilter_add_remove(net, false, filter_num); */
		break;
	case ANDROID_WIFI_CMD_BTCOEXSCAN_START:
		/* TBD: BTCOEXSCAN-START */
		break;
	case ANDROID_WIFI_CMD_BTCOEXSCAN_STOP:
		/* TBD: BTCOEXSCAN-STOP */
		break;
	case ANDROID_WIFI_CMD_BTCOEXMODE:
		break;
	case ANDROID_WIFI_CMD_SETSUSPENDOPT:
		/* bytes_written = wl_android_set_suspendopt(net, command, priv_cmd.total_len); */
		break;
	case ANDROID_WIFI_CMD_SETBAND:
		bytes_written = rtw_android_setband(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_GETBAND:
		bytes_written = rtw_android_getband(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_COUNTRY:
		bytes_written = rtw_android_set_country(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_P2P_DEV_ADDR:
		bytes_written = rtw_android_get_p2p_dev_addr(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_P2P_SET_NOA:
		break;
	case ANDROID_WIFI_CMD_P2P_GET_NOA:
		break;
	case ANDROID_WIFI_CMD_P2P_SET_PS:
		break;
	case ANDROID_WIFI_CMD_SET_AP_WPS_P2P_IE: {
		int skip = strlen(android_wifi_cmd_str[ANDROID_WIFI_CMD_SET_AP_WPS_P2P_IE]) + 3;

		bytes_written = rtw_cfg80211_set_mgnt_wpsp2pie(net, command + skip, priv_cmd.total_len - skip, *(command + skip - 2) - '0');
		break;
	}
	case ANDROID_WIFI_CMD_MIRACAST:
		bytes_written = rtw_android_set_miracast_mode(net, command, priv_cmd.total_len);
		break;
	case ANDROID_WIFI_CMD_WFD_ENABLE: {
		/*	Commented by Albert 2012/07/24 */
		/*	We can enable the WFD function by using the following command: */
		/*	wpa_cli driver wfd-enable */

		if (adapt->wdinfo.driver_interface == DRIVER_CFG80211)
			rtw_wfd_enable(adapt, 1);
		break;
	}
	case ANDROID_WIFI_CMD_WFD_DISABLE: {
		/*	Commented by Albert 2012/07/24 */
		/*	We can disable the WFD function by using the following command: */
		/*	wpa_cli driver wfd-disable */

		if (adapt->wdinfo.driver_interface == DRIVER_CFG80211)
			rtw_wfd_enable(adapt, 0);
		break;
	}
	case ANDROID_WIFI_CMD_WFD_SET_TCPPORT: {
		/*	Commented by Albert 2012/07/24 */
		/*	We can set the tcp port number by using the following command: */
		/*	wpa_cli driver wfd-set-tcpport = 554 */

		if (adapt->wdinfo.driver_interface == DRIVER_CFG80211)
			rtw_wfd_set_ctrl_port(adapt, (u16)get_int_from_command(priv_cmd.buf));
		break;
	}
	case ANDROID_WIFI_CMD_WFD_SET_MAX_TPUT: {
		break;
	}
	case ANDROID_WIFI_CMD_WFD_SET_DEVTYPE: {
		/*	Commented by Albert 2012/08/28 */
		/*	Specify the WFD device type ( WFD source/primary sink ) */

		pwfd_info = &adapt->wfd_info;
		if (adapt->wdinfo.driver_interface == DRIVER_CFG80211) {
			pwfd_info->wfd_device_type = (u8) get_int_from_command(priv_cmd.buf);
			pwfd_info->wfd_device_type &= WFD_DEVINFO_DUAL;
		}
		break;
	}
	case ANDROID_WIFI_CMD_CHANGE_DTIM: {
		u8 dtim;
		u8 *ptr = (u8 *) &priv_cmd.buf;

		ptr += 9;/* string command length of  "SET_DTIM"; */

		dtim = rtw_atoi(ptr);

		RTW_INFO("DTIM=%d\n", dtim);

		rtw_lps_change_dtim_cmd(adapt, dtim);
	}
	break;

#if CONFIG_RTW_MACADDR_ACL
	case ANDROID_WIFI_CMD_HOSTAPD_SET_MACADDR_ACL: {
		rtw_set_macaddr_acl(adapt, get_int_from_command(command));
		break;
	}
	case ANDROID_WIFI_CMD_HOSTAPD_ACL_ADD_STA: {
		u8 addr[ETH_ALEN] = {0x00};
		macstr2num(addr, command + strlen("HOSTAPD_ACL_ADD_STA") + 3);	/* 3 is space bar + "=" + space bar these 3 chars */
		rtw_acl_add_sta(adapt, addr);
		break;
	}
	case ANDROID_WIFI_CMD_HOSTAPD_ACL_REMOVE_STA: {
		u8 addr[ETH_ALEN] = {0x00};
		macstr2num(addr, command + strlen("HOSTAPD_ACL_REMOVE_STA") + 3);	/* 3 is space bar + "=" + space bar these 3 chars */
		rtw_acl_remove_sta(adapt, addr);
		break;
	}
#endif /* CONFIG_RTW_MACADDR_ACL */
#if defined(CONFIG_GTK_OL) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0))
	case ANDROID_WIFI_CMD_GTK_REKEY_OFFLOAD:
		rtw_gtk_offload(net, (u8 *)command);
		break;
#endif /* CONFIG_GTK_OL		 */
	case ANDROID_WIFI_CMD_P2P_DISABLE:
		rtw_p2p_enable(adapt, P2P_ROLE_DISABLE);
		break;
	case ANDROID_WIFI_CMD_DRIVERVERSION:
		bytes_written = strlen(DRIVERVERSION);
		snprintf(command, bytes_written + 1, DRIVERVERSION);
		break;
	default:
		RTW_INFO("Unknown PRIVATE command %s - ignored\n", command);
		snprintf(command, 3, "OK");
		bytes_written = strlen("OK");
	}

response:
	if (bytes_written >= 0) {
		if ((bytes_written == 0) && (priv_cmd.total_len > 0))
			command[0] = '\0';
		if (bytes_written >= priv_cmd.total_len) {
			RTW_INFO("%s: bytes_written = %d\n", __func__, bytes_written);
			bytes_written = priv_cmd.total_len;
		} else
			bytes_written++;
		priv_cmd.used_len = bytes_written;
		if (copy_to_user((void *)priv_cmd.buf, command, bytes_written)) {
			RTW_INFO("%s: failed to copy data to user buffer\n", __func__);
			ret = -EFAULT;
		}
	} else
		ret = bytes_written;

exit:
	rtw_unlock_suspend();
	if (command)
		rtw_mfree(command, priv_cmd.total_len);

	return ret;
}


/**
 * Functions for Android WiFi card detection
 */
#if defined(RTW_ENABLE_WIFI_CONTROL_FUNC)

static int g_wifidev_registered = 0;
static struct semaphore wifi_control_sem;
static struct wifi_platform_data *wifi_control_data = NULL;
static struct resource *wifi_irqres = NULL;

static int wifi_add_dev(void);
static void wifi_del_dev(void);

void *wl_android_prealloc(int section, unsigned long size)
{
	void *alloc_ptr = NULL;
	if (wifi_control_data && wifi_control_data->mem_prealloc) {
		alloc_ptr = wifi_control_data->mem_prealloc(section, size);
		if (alloc_ptr) {
			RTW_INFO("success alloc section %d\n", section);
			if (size != 0L)
				memset(alloc_ptr, 0, size);
			return alloc_ptr;
		}
	}

	RTW_INFO("can't alloc section %d\n", section);
	return NULL;
}

int wifi_get_irq_number(unsigned long *irq_flags_ptr)
{
	if (wifi_irqres) {
		*irq_flags_ptr = wifi_irqres->flags & IRQF_TRIGGER_MASK;
		return (int)wifi_irqres->start;
	}
	return -1;
}

int wifi_set_power(int on, unsigned long msec)
{
	RTW_INFO("%s = %d\n", __func__, on);
	if (wifi_control_data && wifi_control_data->set_power)
		wifi_control_data->set_power(on);
	if (msec)
		msleep(msec);
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
int wifi_get_mac_addr(unsigned char *buf)
{
	RTW_INFO("%s\n", __func__);
	if (!buf)
		return -EINVAL;
	if (wifi_control_data && wifi_control_data->get_mac_addr)
		return wifi_control_data->get_mac_addr(buf);
	return -EOPNOTSUPP;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)) || defined(COMPAT_KERNEL_RELEASE)
void *wifi_get_country_code(char *ccode)
{
	RTW_INFO("%s\n", __func__);
	if (!ccode)
		return NULL;
	if (wifi_control_data && wifi_control_data->get_country_code)
		return wifi_control_data->get_country_code(ccode);
	return NULL;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)) */

static int wifi_set_carddetect(int on)
{
	RTW_INFO("%s = %d\n", __func__, on);
	if (wifi_control_data && wifi_control_data->set_carddetect)
		wifi_control_data->set_carddetect(on);
	return 0;
}

static int wifi_probe(struct platform_device *pdev)
{
	struct wifi_platform_data *wifi_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);
	int wifi_wake_gpio = 0;

	RTW_INFO("## %s\n", __func__);
	wifi_irqres = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "bcmdhd_wlan_irq");

	if (!wifi_irqres)
		wifi_irqres = platform_get_resource_byname(pdev,
				IORESOURCE_IRQ, "bcm4329_wlan_irq");
	else
		wifi_wake_gpio = wifi_irqres->start;

	wifi_control_data = wifi_ctrl;

	wifi_set_power(1, 0);	/* Power On */
	wifi_set_carddetect(1);	/* CardDetect (0->1) */

	up(&wifi_control_sem);
	return 0;
}

static int wifi_remove(struct platform_device *pdev)
{
	struct wifi_platform_data *wifi_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);

	RTW_INFO("## %s\n", __func__);
	wifi_control_data = wifi_ctrl;

	wifi_set_power(0, 0);	/* Power Off */
	wifi_set_carddetect(0);	/* CardDetect (1->0) */

	up(&wifi_control_sem);
	return 0;
}

static int wifi_suspend(struct platform_device *pdev, pm_message_t state)
{
	RTW_INFO("##> %s\n", __func__);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY)
	bcmsdh_oob_intr_set(0);
#endif
	return 0;
}

static int wifi_resume(struct platform_device *pdev)
{
	RTW_INFO("##> %s\n", __func__);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY)
	if (dhd_os_check_if_up(bcmsdh_get_drvdata()))
		bcmsdh_oob_intr_set(1);
#endif
	return 0;
}

/* temporarily use these two */
static struct platform_driver wifi_device = {
	.probe          = wifi_probe,
	.remove         = wifi_remove,
	.suspend        = wifi_suspend,
	.resume         = wifi_resume,
	.driver         = {
		.name   = "bcmdhd_wlan",
	}
};

static struct platform_driver wifi_device_legacy = {
	.probe          = wifi_probe,
	.remove         = wifi_remove,
	.suspend        = wifi_suspend,
	.resume         = wifi_resume,
	.driver         = {
		.name   = "bcm4329_wlan",
	}
};

static int wifi_add_dev(void)
{
	RTW_INFO("## Calling platform_driver_register\n");
	platform_driver_register(&wifi_device);
	platform_driver_register(&wifi_device_legacy);
	return 0;
}

static void wifi_del_dev(void)
{
	RTW_INFO("## Unregister platform_driver_register\n");
	platform_driver_unregister(&wifi_device);
	platform_driver_unregister(&wifi_device_legacy);
}
#endif /* defined(RTW_ENABLE_WIFI_CONTROL_FUNC) */
