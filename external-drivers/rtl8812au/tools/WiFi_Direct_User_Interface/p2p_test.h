#ifndef _P2P_UI_TEST_H_
#define _P2P_UI_TEST_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define	P2P_AUTO	1
//#define DHCP	1
#define CMD_SZ 100
#define SSID_SZ	32

#define SEC 1000000
#define SCAN_POOL_NO	8
#define NEGO_RETRY_INTERVAL 10 * SEC
#define NEGO_QUERY_INTERVAL 0.5 * SEC
#define PRE_NEGO_INTERVAL 0.5 * SEC
#define MAX_PROV_RETRY 15
#define PROV_RETRY_INTERVAL 5
#define PROV_WAIT_TIME	1 * SEC
#define MAX_NEGO_RETRY 60
#define NEGO_WAIT_TIME	0.5 * SEC
#define	WPS_RETRY 120
#define SUPPLICANT_INIT_TIME 1 * SEC
#define HOSTAPD_INIT_TIME	1 * SEC
#define SUPPLICANT_INTERVAL 1 * SEC
#define HOSTAPD_INTERVAL 1 * SEC
#define POLLING_INTERVAL	1 * SEC
#define _TRUE 1
#define _FALSE 0

#define WPS_CONFIG_METHOD_LABEL		0x0004
#define WPS_CONFIG_METHOD_DISPLAY	0x0008
#define WPS_CONFIG_METHOD_E_NFC		0x0010
#define WPS_CONFIG_METHOD_I_NFC		0x0020
#define WPS_CONFIG_METHOD_NFC		0x0040
#define WPS_CONFIG_METHOD_PBC		0x0080
#define WPS_CONFIG_METHOD_KEYPAD	0x0100
#define WPS_CONFIG_METHOD_VPBC		0x0280
#define WPS_CONFIG_METHOD_PPBC		0x0480
#define WPS_CONFIG_METHOD_VDISPLAY	0x2008
#define WPS_CONFIG_METHOD_PDISPLAY	0x4008

enum thread_trigger{
	THREAD_NONE = 0,
	THREAD_DEVICE = 1,
	THREAD_GO = 2,
};

enum P2P_ROLE {
	P2P_ROLE_DISABLE = 0,
	P2P_ROLE_DEVICE = 1,
	P2P_ROLE_CLIENT = 2,
	P2P_ROLE_GO = 3	
};

enum P2P_STATE {
	P2P_STATE_NONE = 0,					//	P2P disable
	P2P_STATE_IDLE = 1,						//	P2P had enabled and do nothing
	P2P_STATE_LISTEN = 2,					//	In pure listen state
	P2P_STATE_SCAN = 3,					//	In scan phase
	P2P_STATE_FIND_PHASE_LISTEN = 4,		//	In the listen state of find phase
	P2P_STATE_FIND_PHASE_SEARCH = 5,		//	In the search state of find phase
	P2P_STATE_TX_PROVISION_DIS_REQ = 6,	//	In P2P provisioning discovery
	P2P_STATE_RX_PROVISION_DIS_RSP = 7,
	P2P_STATE_RX_PROVISION_DIS_REQ = 8,	
	P2P_STATE_GONEGO_ING = 9,				//	Doing the group owner negoitation handshake
	P2P_STATE_GONEGO_OK = 10,				//	finish the group negoitation handshake with success
	P2P_STATE_GONEGO_FAIL = 11,			//	finish the group negoitation handshake with failure
	P2P_STATE_RECV_INVITE_REQ = 12,		//	receiving the P2P Inviation request
	P2P_STATE_PROVISIONING_ING = 13,		//	Doing the P2P WPS
	P2P_STATE_PROVISIONING_DONE = 14,	//	Finish the P2P WPS
};

enum P2P_WPSINFO {
	P2P_NO_WPSINFO						= 0,
	P2P_GOT_WPSINFO_PEER_DISPLAY_PIN	= 1,
	P2P_GOT_WPSINFO_SELF_DISPLAY_PIN	= 2,
	P2P_GOT_WPSINFO_PBC					= 3,
};

struct scan{
	char addr[18];
	int go;
};

struct p2p{
	char ifname[10];
	int enable;
	int status;
	char dev_name[33];
	int intent;
	int listen_ch;
	int wps_info;
	int wpsing;
	unsigned int pin;
	int role;
	char peer_devaddr[18];
	int p2p_get;	//p2p_get==1 : print messages from ioctl p2p_get
	char print_line[CMD_SZ];
	int have_p2p_dev;	//have_p2p_dev==1 : after scanning p2p device
	int show_scan_result;
	int	count_line;
	char peer_ifaddr[18];
	char cmd[CMD_SZ];
	char parse[CMD_SZ];
	char apd_ssid[SSID_SZ];
	int op_ch;	//operation channel
	int wpa_open;
	int ap_open;
	char ap_conf[CMD_SZ];
	char ap_path[CMD_SZ];
	char apcli_path[CMD_SZ];
	char wpa_conf[CMD_SZ];
	char wpa_path[CMD_SZ];
	char wpacli_path[CMD_SZ];
	char ok_msg[CMD_SZ];
	char redo_msg[CMD_SZ];
	char fail_msg[CMD_SZ];
	char nego_msg[CMD_SZ];
	char scan_msg[CMD_SZ];
	int thread_trigger;
	pthread_t pthread;
	pthread_t pthread_go;
	int res;	//check if thread is created; 1: disabled, 0: enabled
	int res_go;	//created if p2p device becomes GO
	struct scan scan_pool[SCAN_POOL_NO];
	int connect_go;
	int no_sta_connected;
};

void ui_screen(struct p2p *p);
char *naming_wpsinfo(int wps_info);
char *naming_role(int role);
char *naming_status(int status);
unsigned int wps_pin_checksum(unsigned int pin);
void p2p_enable(struct p2p *p);
void p2p_scan(struct p2p *p);
void scan_result(struct p2p *p);
void p2p_intent(struct p2p *p);
void p2p_pincode(struct p2p *p, char *ins_no, char *ins_no_again);
void p2p_devaddr(struct p2p *p);
void p2p_role(struct p2p *p, int flag);
void p2p_status(struct p2p *p, int flag);
void p2p_prov_disc_no_addr(struct p2p *p, char *msg);
void p2p_prov_disc(struct p2p *p, char *msg, char *dis_msg, char *label_msg);
void p2p_set_nego(struct p2p *p);
void p2p_ifaddr(struct p2p *p);
void p2p_client_mode(struct p2p *p);
void p2p_go_mode(struct p2p *p);
void p2p_get_hostapd_conf(struct p2p *p);
void p2p_set_opch(struct p2p *p, char *msg, int print);
void p2p_softap_ssid(struct p2p *p, char *msg, int print);
void p2p_listen_ch(struct p2p *p, char *msg);
void p2p_peer_devaddr(struct p2p *p, char *peer_devaddr);
void p2p_peer_req_cm(struct p2p *p, char *peer_req_cm);
void p2p_peer_info(struct p2p *p, char *peer_devaddr, char *peer_req_cm);
void p2p_wps_cm(struct p2p *p, char *scan_addr, char *cms);
void p2p_device_name(struct p2p *p, char *scan_addr, char *dns);
void p2p_setDN(struct p2p *p);
void *polling_status(void *arg);
void *polling_client(void *arg);
void *print_status(void *arg);

#endif	//_P2P_UI_TEST_H_
