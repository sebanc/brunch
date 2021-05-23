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

#include "rtbth_dbg.h"
#include "include/rt_linux.h"

//
// Global debug error level
//
unsigned long DebugLevel = ERROR;
unsigned long DebugFlag = 0xFFFFFFFF;

void DebugPrint(
    unsigned long DebugPrintLevel,
    unsigned long DebugPrintFlag,
    unsigned char *DebugMessage,
    ...
    )
{
#ifdef DBG
	va_list    args;

	va_start(args, DebugMessage);
	if (DebugMessage) {
		//if ((DebugPrintLevel <= INFO) || 
		//	((DebugPrintLevel <= DebugLevel) && 
		//	  ((DebugPrintFlag & DebugFlag) == DebugPrintFlag))
		//)
        if(DebugPrintLevel <= DebugLevel)
			vprintk(DebugMessage, args);
	}
	va_end(args);
	if (DebugPrintLevel == FATAL)
		dump_stack();
#endif
}

int hex_dump(char *title, char *dumpBuf, int len)
{
	int i;
	
	printk("%s(addr=0x%p, len=%d)!\n", title, dumpBuf, len);

	if (dumpBuf && len) {
		for(i = 0 ; i < len; i++){
			if ((i != 0) && (i % 16 == 0))
				printk("\n");
			printk("%02x ", dumpBuf[i]& 0xff);
			if (i == (len -1))
				printk("\n");
		}
	}
	
	return 0;
}

