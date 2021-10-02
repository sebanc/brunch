/*************************************************************************/ /*!
@File
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides system-specific declarations and macros
@License        MIT

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__SYSINFO_H__)
#define __SYSINFO_H__

/*!< System specific poll/timeout details */
#if defined(PVR_LINUX_USING_WORKQUEUES)
#define MAX_HW_TIME_US                           (1000000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT  (10000)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT (3600000)
#define WAIT_TRY_COUNT                           (20000)
#else
#define MAX_HW_TIME_US                           (500000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT  (10000)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT (3600000)
#define WAIT_TRY_COUNT                           (10000)
#endif

#define SYS_RGX_OF_COMPATIBLE "mediatek,mt8173-gpu"

#endif	/* !defined(__SYSINFO_H__) */
