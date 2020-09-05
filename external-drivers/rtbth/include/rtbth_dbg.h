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

#ifndef __RTBTH_DBG_H
#define __RTBTH_DBG_H

#define     NONE       0 //  Tracing is not on
#define     FATAL      1 // Abnormal exit or termination
#define     ERROR      2 // Severe errors that need logging
#define     WARNING    3 // Warnings such as allocation failure
#define     INFO       4 // Includes non-error cases such as Entry-Exit
#define     TRACE      5 // Detailed traces from intermediate steps
#define     LOUD       6 // Detailed trace from every step
#define     NOISY      7 // Raw data

#define DBG_INIT        0x00000001

void DebugPrint(
    unsigned long level,
    unsigned long flag,
    unsigned char *msg,
    ...
    );

int hex_dump(char *title, char *dumpBuf, int len);

#endif
