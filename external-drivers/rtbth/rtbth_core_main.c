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

#include <linux/init.h>
#include <linux/module.h>
#include "include/rtbth_dbg.h"
#include "rtbt_osabl.h"
#include "rtbth_3298.h"
#include "rtbt_ctrl.h"

#define VERSION	"3.9.7"

MODULE_AUTHOR("Ralink Tech.");
MODULE_DESCRIPTION("Support for Ralink Bluetooth RT3290 Cards");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);


static struct rtbt_dev_entry *rtbt_pci_dev_list = NULL;

static int __init rtbth_init(void)
{
	struct rtbt_dev_entry *ent;

	DebugPrint(TRACE, DBG_INIT, "--->%s()\n", __FUNCTION__);

#ifdef RT3298
	ent = rtbt_3298_init();
#endif // RT3298 //

	if (ent)
	{
		rtbt_pci_dev_list = ent;
		ral_os_register(rtbt_pci_dev_list);
	}

    DebugPrint(TRACE, DBG_INIT, "<---%s()\n", __FUNCTION__);
	return 0;
}

static void __exit rtbth_exit(void)
{
	DebugPrint(TRACE, DBG_INIT,"--->%s()\n", __FUNCTION__);

	if (rtbt_pci_dev_list)
		ral_os_unregister(rtbt_pci_dev_list);
#ifdef RT3298
	rtbt_3298_exit();
#endif // RT3298 //
	DebugPrint(TRACE, DBG_INIT,"<---%s()\n", __FUNCTION__);
}

module_init(rtbth_init);
module_exit(rtbth_exit);
