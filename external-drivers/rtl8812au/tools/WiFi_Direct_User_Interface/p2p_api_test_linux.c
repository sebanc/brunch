
#include "p2p_test.h"

unsigned int wps_pin_checksum(unsigned int pin)
{
	unsigned int accum = 0;
	while( pin )
	{
		accum += pin % 10;
		pin /= 10;
		accum += 3 * (pin % 10);
		pin /= 10;
	}
	return( accum % 10 );
}

char lower(char s)
{    
	if(('A' <= s) && (s <= 'Z'))
		return ( s = 'a' + (s - 'A') );
	return s;
}

int p2p_check_success(struct p2p *p)
{
	int ret = 0;
	
	if( p->status == P2P_STATE_GONEGO_OK )
		ret = 1;
		
	return ret;
}

int read_all_sta(struct p2p *p)
{
	int sta_count = 0;
	FILE *pf;
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "%s all_sta > supp_status.txt", p->apcli_path);
	system( p->cmd );
	pf = fopen( "./supp_status.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( p->parse, 0x00, CMD_SZ );
			fgets( p->parse, CMD_SZ, pf );
			if( strncmp( p->parse, "dot11RSNAStatsSTAAddress=", 25) == 0 )
			{
				sta_count++;
				if( p->no_sta_connected == sta_count )
					return _TRUE;
			}
		}

		fclose( pf );
	}
	
	return _FALSE;
}

void do_wps(struct p2p *p)
{
		FILE *pf = NULL;
		int ret = _FALSE, parsing_ok = _FALSE;
		
		do
		{
			memset( p->cmd, 0x00, CMD_SZ );
			if( p->ap_open == _TRUE )
			{
				if(p->wps_info==1 || p->wps_info==2)
					sprintf( p->cmd, "%s wps_pin any %d > supp_status.txt", p->apcli_path, p->pin);
				else if(p->wps_info==3)
					sprintf( p->cmd, "%s wps_pbc any > supp_status.txt", p->apcli_path);
			}
			else if(p->wpa_open == _TRUE)
			{
				if(p->connect_go==1)
				{
					if(p->wps_info==1 || p->wps_info==2)
						sprintf( p->cmd, "%s wps_pin %s %d > supp_status.txt ", p->wpacli_path, p->peer_devaddr, p->pin);
					else if(p->wps_info==3)
						sprintf( p->cmd, "%s wps_pbc %s > supp_status.txt ", p->wpacli_path, p->peer_devaddr);
				}
				else if( strncmp(p->peer_ifaddr, "00:00:00:00:00:00", 17)==0 )
				{
					if(p->wps_info==1 || p->wps_info==2)
						sprintf( p->cmd, "%s wps_pin any %d > supp_status.txt ", p->wpacli_path, p->pin);
					else if(p->wps_info==3)
						sprintf( p->cmd, "%s wps_pbc any > supp_status.txt ", p->wpacli_path);
				}
				else
				{
					if(p->wps_info==1 || p->wps_info==2)
						sprintf( p->cmd, "%s wps_pin %s %d > supp_status.txt ", p->wpacli_path, p->peer_ifaddr, p->pin);
					else if(p->wps_info==3)
						sprintf( p->cmd, "%s wps_pbc %s > supp_status.txt ", p->wpacli_path, p->peer_ifaddr);
				}
			}
			system( p->cmd );
	
			pf = fopen( "./supp_status.txt", "r" );	
			if ( pf )
			{
				while( !feof( pf ) ){
					memset(p->parse, 0x00, CMD_SZ);
					fgets(p->parse, CMD_SZ, pf);

					if(p->ap_open == _TRUE)
					{
						if( (p->wps_info==1 || p->wps_info==2) && (strncmp(p->parse, "", 2) == 0) )
								parsing_ok = _TRUE;
						else if( (p->wps_info==3) && (strncmp(p->parse, "OK", 2) == 0) )
								parsing_ok = _TRUE;
					}
					else if(p->wpa_open == _TRUE)
					{
						if( (p->wps_info==1 || p->wps_info==2) &&	(strncmp(p->parse, "Selected", 8) == 0) )
								parsing_ok = _TRUE;
						else if( (p->wps_info==3) && (strncmp(p->parse, "OK", 2) == 0) )
								parsing_ok = _TRUE;
					}
					
					if( parsing_ok == _TRUE )
					{
						ret = _TRUE;
						p->wpsing = _TRUE;
					}
				}
		
				fclose( pf );
			}
			
			if( ret == 0 )
				usleep( HOSTAPD_INIT_TIME );
		}
		while( ret == 0 );
}

void p2p_enable(struct p2p *p)
{
	p->have_p2p_dev = 0;
	p->connect_go = 0;
			
	if(p->enable == -1 )
	{
		if(p->wpa_open == _TRUE){
			p->wpa_open = _FALSE;
			system("killall wpa_supplicant");
#ifdef DHCP
			system("killall dhclient");
#endif			
		}
		if(p->ap_open == _TRUE){
			p->ap_open = _FALSE;
			system("killall hostapd");
#ifdef DHCP
			system("killall dhcpd");
#endif
		}

		memset( p->cmd, 0x00, CMD_SZ );
		sprintf( p->cmd, "iwpriv %s p2p_set enable=0", p->ifname);
		system( p->cmd );
		
		return;
	}
	
	p->p2p_get=0;
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set enable=%d", p->ifname, p->enable);
	system( p->cmd );
	
	if( p->enable == P2P_ROLE_DISABLE )
	{
		p->wps_info = 0;
		p->pin = 12345670;

		p2p_status(p, 0);
		p2p_role(p, 0);
		
		if(p->res == 0)
		{
			p->res = 1;
		}

		if(p->res_go == 0)
		{
			p->res_go = 1;
		}
				
		p->wpa_open = _FALSE;
		system("killall wpa_supplicant");
#ifdef DHCP
		system("killall dhclient");
#endif
		system("clear");
		p->ap_open = _FALSE;
		system("killall hostapd");
#ifdef DHCP
		system("killall dhcpd");
#endif
		system("clear");
		
	}
	else if( p->enable == P2P_ROLE_DEVICE )
	{
		char msg[5] = "NULL";

#ifdef P2P_AUTO
		p->res = pthread_create(&p->pthread, NULL, &polling_status, (void *)p);
#endif
		
		if( p->res !=0 )
		{
			p->p2p_get=1;
			memset( p->print_line, 0x00, CMD_SZ );
			sprintf( p->print_line, "Thread creation failed" );
		}
		
		if(p->wpa_open == _TRUE){
			p->wpa_open = _FALSE;
			system("killall wpa_supplicant");
#ifdef DHCP
			system("killall dhclient");
#endif
		}
		if(p->ap_open == _TRUE){
			p->ap_open = _FALSE;
			system("killall hostapd");
#ifdef DHCP
			system("killall dhcpd");
#endif
		}
		
		p->intent = 1;
		p2p_intent(p);
		
		p2p_set_opch(p, NULL, 0);
		usleep(50000);
		p2p_softap_ssid(p, NULL, 0);

		p2p_setDN(p);
		p2p_role(p, 0);

		p2p_scan(p);
		
	}
	else if( p->enable == P2P_ROLE_CLIENT )
	{
		if(p->ap_open == _TRUE){
			p->ap_open = _FALSE;
			system("killall hostapd");
#ifdef DHCP
			system("killall dhcpd");
#endif
		}
		
		p2p_status(p, 0);
		p2p_role(p, 0);
		p2p_intent(p);
	}
	else if( p->enable == P2P_ROLE_GO )
	{
		if(p->wpa_open == _TRUE){
			p->wpa_open = _FALSE;
			system("killall wpa_supplicant");
#ifdef DHCP
			system("killall dhclient");
#endif
		}

		p2p_status(p, 0);
		p2p_role(p, 0);
		p2p_intent(p);
		
		p2p_set_opch(p, NULL, 0);
		usleep(50000);
		p2p_softap_ssid(p, NULL, 0);

		p2p_setDN(p);

		if(p->ap_open != _TRUE)
		{
			memset( p->cmd, 0x00, CMD_SZ );
			sprintf( p->cmd, "%s -B %s > temp.txt",p->ap_path, p->ap_conf);
			system( p->cmd );
	
			p->ap_open = _TRUE;
		}

#ifdef P2P_AUTO
		p->res_go = pthread_create(&p->pthread_go, NULL, &polling_client, (void *)p);
#endif

		if( p->res_go != 0 )
		{
			p->p2p_get=1;
			memset( p->print_line, 0x00, CMD_SZ );
			sprintf( p->print_line, "Thread creation failed" );
		}
		
	}

}

void p2p_scan(struct p2p *p)
{
	p->p2p_get=0;
	if( p->enable >= P2P_ROLE_DEVICE )
	{
		p->have_p2p_dev=1;
		memset( p->cmd, 0x00, CMD_SZ );
		sprintf( p->cmd, "iwlist %s scan > scan.txt", p->ifname );
		system( p->cmd );

		p2p_status(p, 0);
		
	}
	else
	{
		p->p2p_get=1;
		memset( p->print_line, 0x00, CMD_SZ );
		sprintf( p->print_line, "%s", p->scan_msg );
	}
}

void scan_result(struct p2p *p)
{
	FILE *pf=NULL;
	int no_dev=0;
	char cms[30] = { 0x00 };
	char dns[SSID_SZ] = { 0x00 };
	char parse[100] = { 0x00 };
	struct scan *pscan_pool;
	
	pf = fopen( "./scan.txt", "r" );
	if ( pf )
	{
		p->count_line=0;
		while( (!feof( pf )) && (no_dev < SCAN_POOL_NO))
		{
			memset( parse, 0x00, CMD_SZ );
			fgets( parse, CMD_SZ, pf );

			if(parse[0] == '\n' || parse[0] == '\0')
				break;
			if( strncmp(parse+10, "Scan completed :", 16) == 0 )
			{
				printf("* NO   DEVICE NAME                BSSID                 GO    CONFIG METHOD                        *\n");
				p->count_line++;
			}
			else if( strncmp(parse+20, "Address:", 8) == 0 )
			{
				pscan_pool = &p->scan_pool[no_dev];
				memset( pscan_pool->addr, 0x00, sizeof(struct scan) );
				strncpy( pscan_pool->addr, parse+29, 17);
			}
			else if( strncmp(parse+20, "ESSID:", 6) == 0 )
			{
				pscan_pool = &p->scan_pool[no_dev];

				p2p_wps_cm(p, pscan_pool->addr, cms);
				p2p_device_name(p, pscan_pool->addr, dns);
				
				if( strncmp(parse+26, "\"DIRECT-\"", 9) == 0 )
				{
					printf("*[%02d]  %-25s  %s          %-38s*\n",no_dev+1, dns, pscan_pool->addr, cms);	
				}
				else
				{
					printf("*[%02d]  %-25s  %s      *   %-38s*\n",no_dev+1, dns, pscan_pool->addr, cms);	
					pscan_pool->go = 1;
				}
				p->count_line++;
				no_dev++;
				
			}
		}
		
		if( p->count_line < (SCAN_POOL_NO + 1) )
		{
			for(p->count_line; p->count_line < SCAN_POOL_NO+1; p->count_line++ )
				printf("*                                                                                                  *\n");
		}
		fclose( pf );
	}

}

void p2p_setDN(struct p2p *p)
{

	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set setDN=%s", p->ifname, p->dev_name);
	system( p->cmd );
}

void p2p_intent(struct p2p *p)
{
	p->p2p_get=0;
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set intent=%d", p->ifname, p->intent);
	system( p->cmd );
}

void p2p_wpsinfo(struct p2p *p)
{
	p->p2p_get=0;
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set got_wpsinfo=%d", p->ifname, p->wps_info);
	system( p->cmd );
}

void p2p_pincode(struct p2p *p, char *ins_no, char *ins_no_again)
{
	int pin_check=0;
	p->p2p_get=0;
	p->show_scan_result = 1;
	ui_screen(p);
	printf("%s", ins_no);
	scanf("%d",&pin_check);
	while( wps_pin_checksum(pin_check) != 0 )
	{
		p->show_scan_result = 1;
		ui_screen(p);
		printf("%s", ins_no_again);	
		scanf("%d",&pin_check);
	}
	p->pin = pin_check;
}

void p2p_devaddr(struct p2p *p)
{
	int c;
	struct scan *pscan_pool;

	p->p2p_get=0;
	
	scanf("%d", &c);
	pscan_pool = &p->scan_pool[c-1];
	strncpy(p->peer_devaddr, pscan_pool->addr, 17);

	if( pscan_pool->go == 1)
		p->connect_go = 1;
}

void p2p_role(struct p2p *p, int flag)
{
	FILE *pf=NULL;
			
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get role > status.txt", p->ifname);
	system( p->cmd );

	pf = fopen( "./status.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( p->parse, 0x00, CMD_SZ );
			fgets( p->parse, CMD_SZ, pf );
			if( strncmp( p->parse, "Role", 4) == 0 )
			{
				p->role = atoi( &p->parse[ 5 ] );
				if(flag==1){
					p->p2p_get=1;
					sprintf( p->print_line, "Role=%s", naming_role(p->role));
				}
				break;
			}	
		}
		fclose( pf );
	}
}

void p2p_status(struct p2p *p, int flag)
{
	FILE *pf=NULL;
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get status > status.txt", p->ifname);
	system( p->cmd );

	pf = fopen( "./status.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( p->parse, 0x00, CMD_SZ );
			fgets( p->parse, CMD_SZ, pf );
			if( strncmp( p->parse, "Status", 6) == 0 )
			{
				p->status = atoi( &p->parse[ 7 ] );		
				if(flag==1){
					p->p2p_get=1;
					sprintf( p->print_line, "Status=%s", naming_status(p->status));
				}
				break;
			}	
		}
		fclose( pf );
	}
}

void change_hostapd_op_ch(struct p2p *p, int op_ch)
{
	FILE *pfin = NULL;
	FILE *pfout = NULL;
	char parse[CMD_SZ] = { 0x00 };
	char cmd[CMD_SZ] = { 0x00 };
		
	pfin = fopen( p->ap_conf, "r" );
	pfout = fopen( "./p2p_hostapd_temp.conf", "w" );
	
	if( pfin && pfout )
	{
		while( !feof( pfin ) ){
			memset( parse, 0x00, CMD_SZ );
			fgets( parse, CMD_SZ, pfin );

			if(strncmp(parse, "channel=", 8) == 0)
			{
				memset(parse, 0x00, CMD_SZ);
				sprintf( parse, "channel=%d\n", op_ch );
				fputs( parse, pfout );
			}
			else
				fputs(parse, pfout);
		}
	}
	else 
	{
		return;
	}

	if( pfin != NULL )
		fclose( pfin );
	if( pfout != NULL )
		fclose( pfout );

	memset( cmd, 0x00, CMD_SZ);
	sprintf( cmd, "rm -rf %s", p->ap_conf );
	system( cmd );
	memset( cmd, 0x00, CMD_SZ);
	sprintf( cmd, "mv ./p2p_hostapd_temp.conf %s", p->ap_conf );
	system( cmd );

	return;
}

void p2p_get_opch(struct p2p *p)
{
	FILE *pf=NULL;
	int peer_op_ch = 0;
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get op_ch > cm.txt", p->ifname);
	system( p->cmd );

	pf = fopen( "./cm.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( p->parse, 0x00, CMD_SZ );
			fgets( p->parse, CMD_SZ, pf );
			if( strncmp( p->parse, "Op_ch", 5) == 0 )
			{
				peer_op_ch = atoi( &p->parse[ 6 ] );
				if( peer_op_ch != p->op_ch )
				{
					change_hostapd_op_ch( p, peer_op_ch );
				}
				break;
			}	
		}
		fclose( pf );
	}
}

void p2p_prov_disc_no_addr(struct p2p *p, char *msg)
{
	p->p2p_get=1;
	memset( p->print_line, 0x00, CMD_SZ );
	sprintf( p->print_line, "%s", msg);
}

#ifdef P2P_AUTO
void p2p_prov_disc(struct p2p *p, char *msg, char *dis_msg, char *label_msg)
{
	int wps_cm, retry_count=0;
	char prov[100] = { 0x00 };

	p->p2p_get=0;
	p->show_scan_result = 1;
	ui_screen(p);
	printf("%s", msg);
	scanf("%d",&wps_cm);

	if(p->res == 0)
	{
		p->res = 1;
	}

	memset( p->cmd, 0x00, CMD_SZ );	
	if( wps_cm == 0 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_display", p->ifname, p->peer_devaddr);
	else if( wps_cm == 1 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_keypad", p->ifname, p->peer_devaddr);
	else if( wps_cm == 2 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_pbc", p->ifname, p->peer_devaddr);
	else if( wps_cm == 3 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_label", p->ifname, p->peer_devaddr);	
	system( p->cmd );
	strcpy( prov, p->cmd );

	usleep(500000);
	p2p_status( p, 0 );
	
	while( p->status != P2P_STATE_RX_PROVISION_DIS_RSP && retry_count < MAX_PROV_RETRY)
	{
		usleep( PROV_WAIT_TIME );
		retry_count++;
		p2p_status( p, 0 );
		if( (retry_count % PROV_RETRY_INTERVAL) == 0)
			system( prov );
	}
	
	if( p->status == P2P_STATE_RX_PROVISION_DIS_RSP )
	{
		switch(wps_cm)
		{
			case 0:	p->wps_info=1;	break;
			case 1: p->wps_info=2;	break;
			case 2: p->wps_info=3;	break;
			case 3: p->wps_info=1;	break;
		}
		
		if( wps_cm==1 || wps_cm==2 )
		{
			p2p_wpsinfo(p);
			
			if(p->connect_go == 1)
				p2p_client_mode(p);
			else
				p2p_set_nego(p);
		}
		else if( wps_cm==0 || wps_cm==3 )
		{
			ui_screen(p);
			if( wps_cm ==0 )
				printf("%s", dis_msg);	
			else if( wps_cm == 3 )
				printf("%s", label_msg);
			scanf("%d",&p->pin);

			p2p_wpsinfo(p);	

			if(p->connect_go == 1)
				p2p_client_mode(p);
			else
				p2p_set_nego(p);
				
		}
	}
	else
	{
		p->p2p_get = 1;
		memset( p->print_line, 0x00, CMD_SZ );
		sprintf( p->print_line, "Issue provision discovery fail" );
		ui_screen(p);	

#ifdef P2P_AUTO		
		pthread_create(&p->pthread, NULL, &polling_status, (void *)p);
#endif
		
	}

}
#else

// This mode is without the following procedures:
// 1.set config method
// 2.start group negotiation
// 3.start wpa_supplicant or hostapd
void p2p_prov_disc(struct p2p *p, char *msg, char *dis_msg, char *label_msg)
{
	int wps_cm;
	p->p2p_get=0;
	p->show_scan_result = 1;
	ui_screen(p);
	printf("%s", msg);
	scanf("%d",&wps_cm);
	memset( p->cmd, 0x00, CMD_SZ );

	
	if( wps_cm == 0 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_display", p->ifname, p->peer_devaddr);
	else if( wps_cm == 1 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_keypad", p->ifname, p->peer_devaddr);
	else if( wps_cm == 2 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_pbc", p->ifname, p->peer_devaddr);
	else if( wps_cm == 3 )
		sprintf( p->cmd, "iwpriv %s p2p_set prov_disc=%s_label", p->ifname, p->peer_devaddr);	
	system( p->cmd );

}
#endif

void p2p_set_nego(struct p2p *p)
{
	FILE *pf=NULL;
	int retry_count = 0, success = 0;
	int retry = NEGO_RETRY_INTERVAL, query = NEGO_QUERY_INTERVAL;

	p->p2p_get=1;
	memset( p->print_line, 0x00, CMD_SZ );
	strcpy( p->print_line, p->nego_msg);
	ui_screen(p);
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set nego=%s ", p->ifname, p->peer_devaddr);
	system( p->cmd );

	usleep( PRE_NEGO_INTERVAL );

	p2p_status(p, 0);

	while( !p2p_check_success(p) && (retry_count < 120 / NEGO_QUERY_INTERVAL ))
	{
		retry_count++;

		if( (retry_count % ( retry / query ) )==0 )
		{
			memset( p->cmd, 0x00, CMD_SZ );
			sprintf( p->cmd, "iwpriv %s p2p_set nego=%s ", p->ifname, p->peer_devaddr);
			system( p->cmd );
			
			usleep( NEGO_QUERY_INTERVAL );
			p2p_status(p, 1);
		}
		else
		{
			ui_screen(p);
			usleep( NEGO_QUERY_INTERVAL );
			p2p_status(p, 1);
		}
	}

	if( p2p_check_success(p) )
	{
		p2p_role(p ,0);
		p->p2p_get = 1;
		memset( p->print_line, 0x00, CMD_SZ );
		sprintf( p->print_line, "%s", p->ok_msg );
		p2p_ifaddr(p);
		ui_screen(p);		
		
		if( p->role == P2P_ROLE_CLIENT )
		{
			p2p_client_mode(p);
		}
		else if( p->role == P2P_ROLE_GO )
		{
			p2p_get_opch(p);
			p2p_go_mode(p);
		}
	}
	else
	{
		p->p2p_get = 1;
		p2p_status(p, 0);
		memset( p->print_line, 0x00, CMD_SZ );
		sprintf( p->print_line, "Status= %d, %s", p->status, p->fail_msg );
		ui_screen(p);

#ifdef P2P_ATUO		
		pthread_create(&p->pthread, NULL, &polling_status, (void *)p);
#endif
		
	}
}

//After negotiation success, get peer device's interface address.
void p2p_ifaddr(struct p2p *p)
{
	FILE *pf=NULL;
	char addr_12[12] = { 0x00 };
	int i;
	
	/* peer_ifaddr */
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get peer_ifa > status.txt", p->ifname);
	system( p->cmd );
	
	pf = fopen( "./status.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( p->parse, 0x00, CMD_SZ );
			fgets( p->parse, CMD_SZ, pf );
			if( strncmp( p->parse, "MAC", 3) == 0 )
			{
				strncpy( p->peer_ifaddr, p->parse+4, 17 );
				break;
			}	
		}
		fclose( pf );
	}
	
}

void p2p_client_mode(struct p2p *p)
{
	FILE *pf = NULL;
	int count = 0, ret = 0;
	int inactive_count = 0, inactive_restart = 0;

	if(p->wpa_open==_TRUE)
		return;
	else
		p->wpa_open = _TRUE;

	p2p_ifaddr(p);
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "%s -i %s -c %s -B ",p->wpa_path, p->ifname, p->wpa_conf);
	system( p->cmd );

	p->p2p_get=1;
	memset( p->print_line, 0x00, CMD_SZ );
	strcpy( p->print_line, "Start wpa_supplicant");
	ui_screen(p);

	usleep( SUPPLICANT_INIT_TIME );
	
	do_wps(p);
	
	usleep( SUPPLICANT_INTERVAL );
		
	while( count < WPS_RETRY )		//query status
	{
	
		memset( p->cmd, 0x00, CMD_SZ );
		sprintf( p->cmd, "%s status > supp_status.txt", p->wpacli_path);
		system( p->cmd );
		
		pf = fopen( "./supp_status.txt", "r" );
		if ( pf )
		{
			while( !feof( pf ) ){
				memset( p->parse, 0x00, CMD_SZ );
				fgets( p->parse, CMD_SZ, pf );
				if( strncmp( p->parse, "wpa_state=", 10) == 0 )
				{
					int i;
					if( strncmp( p->parse, "wpa_state=COMPLETED", 19) == 0 ){
						count = WPS_RETRY;
						p->wpsing = _FALSE;
#ifdef DHCP
						memset( p->cmd, 0x00, CMD_SZ );
						sprintf( p->cmd, "dhclient %s", p->ifname);
						system( p->cmd );
#endif //DHCP
					}
					else if( strncmp( p->parse, "wpa_state=INACTIVE", 18) == 0 ){
						inactive_count++;
						if( (inactive_count % 5)== 0)
						{
							if( p->wps_info == 2 )
							{
								memset( p->cmd, 0x00, CMD_SZ );
								sprintf( p->cmd, "%s wps_pin %s %d > supp_status.txt ", p->wpacli_path, p->peer_ifaddr, p->pin);
								system( p->cmd );
								
								inactive_restart = 1;
							}			
						}
					}

					if( inactive_restart == 1 )
					{
						inactive_restart = 0;
						p->p2p_get=1;	
						memset(p->print_line, 0x00, CMD_SZ);
						sprintf(p->print_line, "Restart WPS");
						ui_screen(p);
					}
					else
					{
						p->p2p_get=1;
						memset(p->print_line, 0x00, CMD_SZ);
						for(i=0; i<CMD_SZ; i++){
							if(p->parse[i] == '\n'){
								p->parse[i] = ' ';
							}
						}	
						sprintf(p->print_line, "%s", p->parse);
						ui_screen(p);
					}
					
					break;
				}	
			}

			fclose( pf );
		}
		
		count++;
		usleep( SUPPLICANT_INTERVAL );
	
	}
	
	p->wpsing = _FALSE;
}

void p2p_go_mode(struct p2p *p)
{
	int count = 0, i = -1;
	char addr_lower[18];

	p2p_ifaddr(p);
	p->no_sta_connected++;

	p->p2p_get=1;
	memset( p->print_line, 0x00, CMD_SZ );
	strcpy( p->print_line, "Start hostapd");
	ui_screen(p);

	if(p->ap_open != _TRUE)
	{
		memset( p->cmd, 0x00, CMD_SZ );
		sprintf( p->cmd, "%s -B %s > temp.txt",p->ap_path, p->ap_conf);
		system( p->cmd );

		usleep( HOSTAPD_INIT_TIME );
		p->ap_open = _TRUE;
	}

	do_wps(p);

	usleep( HOSTAPD_INTERVAL );


	while( count < WPS_RETRY )		//query status
	{
		if( read_all_sta(p) == _TRUE )
		{
			count = WPS_RETRY;
			p->wpsing = _FALSE;

			p->p2p_get=1;
			memset(p->print_line, 0x00, CMD_SZ);
			for(i=0; i<CMD_SZ; i++){
				if(p->parse[i] == '\n'){
					p->parse[i] = ' ';
				}
			}		
			sprintf(p->print_line, "%s", p->parse);
			ui_screen(p);
#ifdef DHCP
			memset( p->cmd, 0x00, CMD_SZ );
			sprintf( p->cmd, "ifconfig %s 192.168.1.254", p->ifname);
			system( p->cmd );

			usleep(50000);
			
			system( "/etc/rc.d/init.d/dhcpd start" );
			system( "clear" );
#endif //DHCP

			//After starting hostapd and doing WPS connection successful,
			//We create a thread to query driver if some other p2p devices connected.
			p2p_status(p, 0);
			usleep(50000);

#ifdef P2P_AUTO
			p->res_go = pthread_create(&p->pthread_go, NULL, &polling_client, (void *)p);

			if( p->res_go != 0 )
			{
				p->p2p_get=1;
				memset( p->print_line, 0x00, CMD_SZ );
				sprintf( p->print_line, "Thread creation failed" );
			}
#endif
			break;		
		}
		else
		{
			if( count == WPS_RETRY)
				break;

			count++;
			usleep( HOSTAPD_INTERVAL );
		
			p->p2p_get=1;
			memset( p->print_line, 0x00, CMD_SZ );
			sprintf( p->print_line, "hostapd open, count:%d", count);
			ui_screen(p);
		}
	}

	p->wpsing = _FALSE;
}

void p2p_get_hostapd_conf(struct p2p *p)
{
	FILE *pf = NULL;
	
	pf = fopen( p->ap_conf, "r" );	
	if ( pf )
	{
		while( !feof( pf ) ){
			memset(p->parse, 0x00, CMD_SZ);
			fgets(p->parse, CMD_SZ, pf);
			if(strncmp(p->parse, "ssid=", 5) == 0)
			{
				strcpy( p->apd_ssid, p->parse+5 );
			}
			else if(strncmp(p->parse, "channel=", 8) == 0)
			{
				p->op_ch = atoi( p->parse+8 );
			}
			else if(strncmp(p->parse, "device_name=", 12) == 0)
			{
				int i;
				p->dev_name[0] = '"';
				strncpy( p->dev_name+1, p->parse+12, 32 );
				for(i=31; i>0; i--)
				{
					if(p->dev_name[i] == '\n')
					{
						p->dev_name[i]='"';
						p->dev_name[i+1]=' ';
						break;
					}
				}
			}
		}

		fclose( pf );
	}
	
}

void p2p_set_opch(struct p2p *p, char *msg, int print)
{
	if(print == 1)
	{
		p->show_scan_result = 1;
		ui_screen(p);
		printf("%s", msg);	
		scanf("%d",&p->op_ch);
	}
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set op_ch=%d", p->ifname, p->op_ch);
	system( p->cmd );
}

void p2p_softap_ssid(struct p2p *p, char *msg, int print)
{
	if(print == 1)
	{
		p->show_scan_result = 1;
		ui_screen(p);
		printf("%s", msg);
		scanf("%s",p->apd_ssid);
	}
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set ssid=%s ", p->ifname, p->apd_ssid);
	system( p->cmd );
}

void p2p_listen_ch(struct p2p *p, char *msg)
{
	p->show_scan_result = 1;
	ui_screen(p);
	printf("%s", msg);
	scanf("%d",&p->listen_ch);
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_set listen_ch=%d ", p->ifname, p->listen_ch);
	system( p->cmd );
}

//When receive provision discovery request,
//it can show which device address that are connected.
void p2p_peer_devaddr(struct p2p *p, char *peer_devaddr)
{
	FILE *pf = NULL;
	char addr_12[12] = { 0x00 };
	int i;
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get peer_deva > peer.txt", p->ifname);
	system( p->cmd );
	
	pf = fopen( "./peer.txt", "r" );
	if ( pf )
	{
		memset( p->parse, 0x00, CMD_SZ );
		fgets( p->parse, CMD_SZ, pf );
		fgets( p->parse, CMD_SZ, pf );
		strncpy(addr_12, p->parse, 12 );

		for(i=0; i<6; i++)
		{
			p->peer_devaddr[3*i] = addr_12[2*i];
			p->peer_devaddr[3*i+1] = addr_12[2*i+1];

			if(i==5)
				p->peer_devaddr[3*i+2] = '\0';
			else
				p->peer_devaddr[3*i+2] = ':';
		}

		fclose( pf );

	}
	
}

//When receive provision discovery request,
//it can show which config method that want to be as WPS connection.
void p2p_peer_req_cm(struct p2p *p, char *peer_req_cm)
{
	FILE *pf = NULL;
	
	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get req_cm > peer.txt", p->ifname);
	system( p->cmd );
	
	pf = fopen( "./peer.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( p->parse, 0x00, CMD_SZ );
			fgets( p->parse, CMD_SZ, pf );
			if( strncmp( p->parse, "CM", 2) == 0 )
			{
				strncpy( peer_req_cm, p->parse+3, 3 );
				break;
			}	
		}
		fclose( pf );
	}
	
}

//When be regotiated passsively and successfully,
//it will show peer device's peer device address and request config method
void p2p_peer_info(struct p2p *p, char *peer_devaddr, char *peer_req_cm)
{
	p->p2p_get=1;

	memset( p->print_line, 0x00, CMD_SZ );
	sprintf( p->print_line, "Peer address:%s, req_cm: %s ", peer_devaddr, peer_req_cm);

}

//After scan, it will get other devices' supported config methods.
void p2p_wps_cm(struct p2p *p, char *scan_addr, char *cms)
{
	FILE *pf = NULL;
	int cm=0, i=0;
	char parse[100] = {0x00};
	memset( cms, 0x00, 30 );

	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get2 wpsCM=%s > cm.txt", p->ifname, scan_addr);	
	system( p->cmd );	
		
	pf = fopen( "./cm.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( parse, 0x00, CMD_SZ );
			fgets( parse, CMD_SZ, pf );
			if( strncmp( parse, "M=", 2 ) == 0 )
			{
				cm = atoi( &parse[ 2 ] );
				
				if((cm & WPS_CONFIG_METHOD_LABEL) == WPS_CONFIG_METHOD_LABEL){
					strncpy( cms+i, " LAB", 4 );
					i=i+4;
				}
				if((cm & WPS_CONFIG_METHOD_DISPLAY) == WPS_CONFIG_METHOD_DISPLAY){
					if((cm & WPS_CONFIG_METHOD_VDISPLAY) == WPS_CONFIG_METHOD_VDISPLAY){
						strncpy( cms+i, " VDIS", 5 );
						i=i+5;
					}else if((cm & WPS_CONFIG_METHOD_PDISPLAY) == WPS_CONFIG_METHOD_PDISPLAY){
						strncpy( cms+i, " PDIS", 5 );
						i=i+5;
					}else{
						strncpy( cms+i, " DIS", 4 );
						i=i+4;
					}
				}
				if((cm & WPS_CONFIG_METHOD_E_NFC) == WPS_CONFIG_METHOD_E_NFC){
					strncpy( cms+i, " ENFC", 5 );
					i=i+5;
				}
				if((cm & WPS_CONFIG_METHOD_I_NFC) == WPS_CONFIG_METHOD_I_NFC){
					strncpy( cms+i, " INFC", 5 );
					i=i+5;
				}
				if((cm & WPS_CONFIG_METHOD_NFC) == WPS_CONFIG_METHOD_NFC){
					strncpy( cms+i, " NFC", 4 );
					i=i+4;
				}
				if((cm & WPS_CONFIG_METHOD_PBC) == WPS_CONFIG_METHOD_PBC){
					if((cm & WPS_CONFIG_METHOD_VPBC) == WPS_CONFIG_METHOD_VPBC){
						strncpy( cms+i, " VPBC", 5 );
						i=i+5;
					}else if((cm & WPS_CONFIG_METHOD_PPBC) == WPS_CONFIG_METHOD_PPBC){
						strncpy( cms+i, " PPBC", 5 );
						i=i+5;
					}else{
						strncpy( cms+i, " PBC", 4 );
						i=i+4;
					}
				}
				if((cm & WPS_CONFIG_METHOD_KEYPAD) == WPS_CONFIG_METHOD_KEYPAD){
					strncpy( cms+i, " PAD", 4 );
					i=i+4;
				}

				break;
			}	
		}
		fclose( pf );
	}
	
}

//After scan, it will get other devices' device name.
void p2p_device_name(struct p2p *p, char *scan_addr, char *dns)
{
	FILE *pf = NULL;
	int i;
	char parse[100] = {0x00};
	memset( dns, 0x00, 32 );

	memset( p->cmd, 0x00, CMD_SZ );
	sprintf( p->cmd, "iwpriv %s p2p_get2 devN=%s > cm.txt", p->ifname, scan_addr);	
	system( p->cmd );	
		
	pf = fopen( "./cm.txt", "r" );
	if ( pf )
	{
		while( !feof( pf ) ){
			memset( parse, 0x00, CMD_SZ );
			fgets( parse, CMD_SZ, pf );
			if( strncmp( parse, "N=", 2) == 0 )
			{
				strncpy( dns, parse+2, 32);
				for( i=0; i<32; i++)
				{
					if(*(dns+i) == '\n'){
						*(dns+i) = ' ' ;
						break;
					}
				}

				break;
			}	
		}
		fclose( pf );
	}
}

//Successively query local device status with interval "POLLING_INTERVAL"
//when status == P2P_STATE_RX_PROVISION_DIS_REQ, 
//it require user to insert corresponding WPS config method
void *polling_status(void *arg)
{
	struct p2p *p=(struct p2p*)arg;
	
	while( p->res == 0 ){

		p2p_status(p, 0);
		if( (p->status == P2P_STATE_RX_PROVISION_DIS_REQ) || (p->status == P2P_STATE_GONEGO_FAIL) )
		{
			p->thread_trigger = THREAD_DEVICE ;
			
			char peer_devaddr[18];
			char peer_req_cm[4];
			
			memset( peer_devaddr, 0x00, 18);
			memset( peer_req_cm, 0x00, 4);
			
			p2p_peer_devaddr(p, peer_devaddr);
			p2p_peer_req_cm(p, peer_req_cm);
			p2p_peer_info(p, p->peer_devaddr, peer_req_cm);

			ui_screen(p);

			//strncpy(p->peer_devaddr, peer_devaddr, 17);
			if( (strncmp( peer_req_cm, "dis", 3) == 0) || (strncmp( peer_req_cm, "lab", 3) == 0) )
			{
				printf("Here is your PIN, insert c to continue: %d\n", p->pin);
			}
			else if( (strncmp( peer_req_cm, "pbc", 3) == 0) )
			{
				printf("Please push b to accept:\n");
			}
			else if( (strncmp( peer_req_cm, "pad", 3) == 0) )
			{
				printf("Please insert peer PIN code:\n");
			}

			break;
  	}

		usleep( POLLING_INTERVAL );
	}

	return NULL;
}

//If p2p device becomes GO, we still polling driver status
//to check whether some other p2p devices connected
void *polling_client(void *arg)
{
	struct p2p *p=(struct p2p*)arg;
	
	while( p->res_go == 0 ){

		if( p->no_sta_connected > 0 && ( p->wpsing == _FALSE ) )
		{
			if( read_all_sta(p) == _FALSE )
			{
				p->no_sta_connected--;
			}
		}

		p2p_status(p, 0);
		if( p->status == P2P_STATE_RX_PROVISION_DIS_REQ || p->status == P2P_STATE_GONEGO_FAIL || p->status == P2P_STATE_GONEGO_ING )
		{
			p->thread_trigger = THREAD_GO ;
			
			char peer_devaddr[18];
			char peer_req_cm[4];
			
			memset( peer_devaddr, 0x00, 18);
			memset( peer_req_cm, 0x00, 4);
			
			p2p_peer_devaddr(p, peer_devaddr);
			p2p_peer_req_cm(p, peer_req_cm);
			p2p_peer_info(p, p->peer_devaddr, peer_req_cm);

			ui_screen(p);

			//strncpy(p->peer_devaddr, peer_devaddr, 17);
			if( (strncmp( peer_req_cm, "dis", 3) == 0) || (strncmp( peer_req_cm, "lab", 3) == 0) )
			{
				printf("Here is your PIN, insert c to continue: %d\n", p->pin);
			}
			else if( (strncmp( peer_req_cm, "pbc", 3) == 0) )
			{
				printf("Please push b to accept:\n", p->status);
			}
			else if( (strncmp( peer_req_cm, "pad", 3) == 0) )
			{
				printf("Please insert peer PIN code:\n");
			}

			break;
  	}

		usleep( POLLING_INTERVAL );
	}

	return NULL;
}
