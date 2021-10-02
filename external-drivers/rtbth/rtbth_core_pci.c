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

#include <linux/pci.h>
#include <linux/interrupt.h>
#include "hps_bluez.h"
#include "rt_linux.h"
#include "rtbt_hal.h"
#include "rtbt_osabl.h"
#include "rtbth_us.h"

#ifdef OS_ABL_SUPPORT
/* struct pci_device_id *rtbt_pci_ids = NULL; */
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static const struct pci_device_id rtbt_pci_ids[] __devinitdata = {
#else
static const struct pci_device_id rtbt_pci_ids[] = {
#endif
    {PCI_DEVICE(0x1814, 0x3298)},
    /* do not remove the last entry */
    {}
};
MODULE_DEVICE_TABLE(pci, rtbt_pci_ids);
#endif // OS_ABL_SUPPORT //

int rtbth_rx_packet(void *pdata, RXBI_STRUC rxbi, void *buf, unsigned int len);

#ifdef CONFIG_PM
static int rtbt_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
    struct hci_dev *hci_dev = (struct hci_dev *)pci_get_drvdata(pdev);
    struct rtbt_os_ctrl *os_ctrl;
    BT_DBG("-->%s(): pm_message_state=%d", __FUNCTION__, state.event);
    if (!hci_dev) {
        BT_ERR("%s(): pci_get_drvdata failed!", __FUNCTION__);
        return -ENODEV;
    }
    os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hci_dev);
    if (!os_ctrl) {
        BT_ERR("%s(): hci_dev->driver_data is NULL!", __FUNCTION__);
        return -ENODATA;
    }
//msleep(10000);
//    rtbt_hps_iface_detach(os_ctrl);
//    os_ctrl->hps_ops->suspend(os_ctrl->dev_ctrl);
    BT_DBG("<--%s()", __FUNCTION__);
    return 0;
}

static int rtbt_pci_resume(struct pci_dev *pdev)
{
    struct hci_dev *hci_dev = (struct hci_dev *)pci_get_drvdata(pdev);
    struct rtbt_os_ctrl *os_ctrl;
    BT_DBG("-->%s()", __FUNCTION__);
    if (!hci_dev) {
        BT_ERR("%s(): pci_get_drvdata failed!", __FUNCTION__);
        return -ENODEV;
    }
    os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hci_dev);
    if (!os_ctrl) {
        BT_ERR("%s(): hci_dev->driver_data is NULL!", __FUNCTION__);
        return -ENODATA;
    }
 //   os_ctrl->hps_ops->resume(os_ctrl->dev_ctrl);
 //   rtbt_hps_iface_attach(os_ctrl);
    BT_DBG("<--%s()", __FUNCTION__);
    return 0;
}
#endif /* CONFIG_PM */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __devinit rtbt_pci_probe(struct pci_dev *pdev,
                                    const struct pci_device_id *id)
#else
static int rtbt_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
#endif
{
	const char *print_name;
	struct rtbt_os_ctrl *os_ctrl;
	struct rtbt_dev_ops *dev_ops;
	void __iomem *csr_addr = NULL;
	int rv;

	BT_DBG("-->%s(): probe for device(Vendor=0x%x, Device=0x%p)",
			__FUNCTION__, pdev->vendor, &pdev->device);

	if (!id->driver_data) {
		BT_ERR("pci_device_id->driver_data is NULL!");
		return -1;
	}

	dev_ops = (struct rtbt_dev_ops *)(id->driver_data);
	if (!(dev_ops->dev_ctrl_init && dev_ops->dev_ctrl_deinit &&
		dev_ops->dev_resource_init && dev_ops->dev_resource_deinit &&
		dev_ops->dev_hw_init && dev_ops->dev_hw_deinit)) {
	    BT_ERR("dev_ops have null function pointer!");
	    return -1;
	}

	rv = pci_enable_device(pdev);
	if (rv) {
		BT_ERR("call pci_enable_dev failed(%d)", rv);
		return rv;
	}

	print_name = pci_name(pdev);
	if ((rv = pci_request_region(pdev, 0, print_name)) != 0) {
		BT_ERR("Request PCI resource failed(%d)", rv);
		goto err_out_disable_dev;
	}

	/* map physical address to virtual address for accessing register */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
	csr_addr = pci_iomap(pdev, 0, pci_resource_len(pdev, 0));
#else
	csr_addr = ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
#endif
	if (!csr_addr) {
		BT_ERR("ioremap failed, region 0x%lx @ 0x%lX",
				(unsigned long)pci_resource_start(pdev, 0),
				(unsigned long)pci_resource_len(pdev, 0));
		goto err_out_free_res;
	} else {
		BT_WARN("%s(): PCI Dev(%s) get resource at 0x%lx,VA 0x%lx,IRQ %d.",
				__FUNCTION__, print_name,
				(ULONG)pci_resource_start(pdev, 0),
				(ULONG)csr_addr, pdev->irq);
	}

	/* Config PCI bus features */
	// TODO: Shiang, does our chip support mwi??
	rv = pci_set_mwi(pdev);
	if (rv != 0) {
		BT_ERR("set MWI failed(%d)", rv);
		goto err_out_free_res;
	}

	pci_set_master(pdev);

	/* device control block initialization */
	BT_WARN("call dev_ops->dev_ctrl_init!");
	if (dev_ops->dev_ctrl_init(&os_ctrl, csr_addr))
		goto err_out_free_res;

	os_ctrl->dev_ops = dev_ops;
	os_ctrl->if_dev = (void *) pdev;
//    rtbth_us_init(os_ctrl->dev_ctrl);

	BT_WARN("call dev_ops->dev_resource_init!");
	if (dev_ops->dev_resource_init(os_ctrl))
		goto err_dev_ctrl;


    rtbth_us_init(os_ctrl->dev_ctrl);
	/* Init the host protocol stack hooking interface */
	if (rtbt_hps_iface_init(RAL_INF_PCI, pdev, os_ctrl))
		goto err_dev_resource;
#if 0
	/* Link the host protocol stack interface to the protocl stack */
	if (rtbt_hps_iface_attach(os_ctrl))
		goto err_hps_iface;
#endif
	BT_DBG("<--%s()", __FUNCTION__);
	return 0;

	// TODO: Shiang, free the resource here.
//err_hps_iface:
//	rtbt_hps_iface_deinit(RAL_INF_PCI, pdev, os_ctrl);

err_dev_resource:
	BT_ERR("call rtbt_dev_resource_deinit()");
	dev_ops->dev_resource_deinit(os_ctrl);

err_dev_ctrl:
	BT_ERR("call rtbt_dev_ctrl_deinit()");
	dev_ops->dev_ctrl_deinit(os_ctrl);

err_out_free_res:
	if (csr_addr) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
		pci_iounmap(pdev, csr_addr);
#else
		iounmap((csr_addr));
#endif
	}
	pci_set_drvdata(pdev, NULL);

	pci_release_region(pdev, 0);

err_out_disable_dev:
	pci_disable_device(pdev);

	BT_ERR("<--%s(): fail", __FUNCTION__);
	return -1;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static void __devexit rtbt_pci_remove(struct pci_dev *pdev)
#else
static void rtbt_pci_remove(struct pci_dev *pdev)
#endif
{
	struct hci_dev *hci_dev = (struct hci_dev *)pci_get_drvdata(pdev);
	struct rtbt_os_ctrl *os_ctrl;
	struct rtbt_dev_ops *dev_ops;
	void __iomem *csr_addr;

	if (hci_dev == NULL){
		BT_ERR("%s(): pci_get_drvdata failed!", __FUNCTION__);
		return;
	}

	os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hci_dev);
	if (os_ctrl == NULL) {
		BT_ERR("%s(): hci_dev->driver_data is NULL!", __FUNCTION__);
		return;
	}

	csr_addr = os_ctrl->if_ops.pci_ops.csr_addr;
	BT_DBG("-->%s(): csr_addr=0x%lx!", __FUNCTION__,
			(unsigned long)os_ctrl->if_ops.pci_ops.csr_addr);

#if 0
	rtbt_hps_iface_detach(os_ctrl);
#endif
	rtbt_hps_iface_deinit(RAL_INF_PCI, pdev, os_ctrl);

    rtbth_us_deinit(os_ctrl->dev_ctrl);

	if (os_ctrl->dev_ops) {
		dev_ops = os_ctrl->dev_ops;
		if (dev_ops->dev_resource_deinit)
			dev_ops->dev_resource_deinit(os_ctrl);

		if (dev_ops->dev_ctrl_deinit)
			dev_ops->dev_ctrl_deinit(os_ctrl);
	}

 //   rtbth_us_deinit(os_ctrl->dev_ctrl);

	pci_set_drvdata(pdev, NULL);

	if (csr_addr != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
		pci_iounmap(pdev, csr_addr);
#else
		iounmap(csr_addr);
#endif
	}

	pci_release_region(pdev, 0);
	pci_disable_device(pdev);

	BT_DBG("<--%s()", __FUNCTION__);
}


static struct pci_driver rtbt_pci_driver = {
    .name = "rtbt",
#ifndef OS_ABL_SUPPORT
    .id_table = rtbt_pci_ids,
#endif /* OS_ABL_SUPPORT */
    .probe = rtbt_pci_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
    .remove = __devexit_p(rtbt_pci_remove),
#else
    .remove = rtbt_pci_remove,
#endif
#ifdef CONFIG_PM
    .suspend = rtbt_pci_suspend,
    .resume = rtbt_pci_resume,
#endif /* CONFIG_PM */
};



/*******************************************************************************

	Device IRQ related functions.

 *******************************************************************************/

/*++
Routine Description:

	Interrupt handler for the device.

Arguments:

	Interupt - Address of the KINTERRUPT Object for our device.
	ServiceContext - Pointer to our adapter

Return Value:

	 TRUE if our device is interrupting, FALSE otherwise.

--*/
IRQ_HANDLE_TYPE
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
BthIsr(int irq, void *dev_instance)
#else
BthIsr(int irq, void *dev_instance, struct pt_regs *regs)
#endif
{
	struct hci_dev *hdev = (struct hci_dev *)dev_instance;
	struct rtbt_os_ctrl *os_ctrl;
	int retval = -1;

	ASSERT(hdev);

	if (!test_bit(HCI_RUNNING, &hdev->flags)) {
		BT_ERR("%s(): HCI_RUNNING not set!", __FUNCTION__);
		goto done;
	}

	if (hdev){
		os_ctrl = (struct rtbt_os_ctrl *)hci_get_drvdata(hdev);
		ASSERT(os_ctrl);
		if (os_ctrl && os_ctrl->if_ops.pci_ops.isr) {
			retval = (os_ctrl->if_ops.pci_ops.isr)(os_ctrl->dev_ctrl);
		}
		else
		{
			if (os_ctrl)
				BT_ERR("os_ctrl->if_ops.pci_ops.isr=0x%p", os_ctrl->if_ops.pci_ops.isr);
		}
	}

done:
	return  IRQ_HANDLED;
}


int RtmpOSIRQRequest(IN void *if_dev, void *dev_id)
{
	struct pci_dev *pdev = if_dev;
	struct hci_dev *hdev = (struct hci_dev *)dev_id;
	int retval = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	retval = request_irq(pdev->irq, BthIsr, IRQF_SHARED, hdev->name, hdev);
#else
	retval = request_irq(pdev->irq, BthIsr, SA_SHIRQ, hdev->name, hdev);
#endif
	if (retval != 0)
		BT_ERR("RT_BT: request_irq (IRQ=%d) ERROR(%d)", pdev->irq, retval);
	else
		BT_WARN("%s(): request_irq (IRQ=%d)done, isr_handler=0x%lx!", __FUNCTION__, pdev->irq, (ULONG)BthIsr);

	return retval;

}


int RtmpOSIRQRelease(IN void *if_dev, void *dev_id)
{
	struct pci_dev *pdev = if_dev;
	synchronize_irq(pdev->irq);
	free_irq(pdev->irq, dev_id);
	return 0;
}

void RT_PCI_IO_READ32(ULONG addr, UINT32 *val)
{
	*val = readl((void *)addr);
}

void RT_PCI_IO_WRITE32(ULONG addr, UINT32 val)
{
	writel(val, (void *)addr);
}


// Function for hardware Descriptor Memory allocation.
int rtbt_dma_mem_alloc(
	IN void *if_dev,
	IN unsigned long len,
	IN unsigned char cached,
	OUT void **virt_addr,
	OUT	PHYSICAL_ADDRESS *phy_addr)
{
	*virt_addr = (PVOID)pci_alloc_consistent((struct pci_dev *)if_dev,
												sizeof(char)*len,
												(dma_addr_t *)phy_addr);

	return TRUE;
}


// Function for free allocated hardware Descriptor Memory.
void rtbt_dma_mem_free(
	IN void *if_dev,
	IN unsigned long len,
	IN void *virt_addr,
	IN PHYSICAL_ADDRESS phy_addr)
{
	pci_free_consistent((struct pci_dev *)if_dev,
						len, virt_addr, phy_addr);
}



#if 1 //def OS_ABL_SUPPORT
static int rtbt_linux_pci_ids_create(struct rtbt_dev_entry *pRalDevList)
{
	struct pci_device_id *pIdTable = NULL, *pTbPtr = NULL;
	struct ral_dev_id *pDevIDList;
	int size = 0;
	int phase = 1;
	//int status;

	/* First parsing the IdTb and convert to linux specific "pci_device_id" format */
loop:
	pDevIDList = pRalDevList->pDevIDList;
	while(pDevIDList){
		BT_WARN("RTBT_Tb: vendor=0x%x, device=0x%x", pDevIDList->vid, pDevIDList->pid);
		if (pDevIDList->pid && pDevIDList->vid) {
			if (phase == 1)
				size++;
			else
			{
				pTbPtr->vendor = pDevIDList->vid;
				pTbPtr->device = pDevIDList->pid;
				pTbPtr->subvendor = PCI_ANY_ID;
				pTbPtr->subdevice = PCI_ANY_ID;
				pTbPtr->driver_data = (unsigned long)pRalDevList->dev_ops;
				BT_WARN("Convert: vendor=0x%x, device=0x%x", pTbPtr->vendor, pTbPtr->device);
				pTbPtr++;
			}
			pDevIDList++;
		}
		else
			break;
	}

	if (phase == 1) {
		if (size > 0 ) {
			size = (size+1) * sizeof(struct pci_device_id);
			if ((pIdTable = kmalloc(size, GFP_KERNEL)) == NULL)
				return -1;
			memset(pIdTable, 0, size);
			BT_WARN("DynamicAlloc pci_device_id table at 0x%p with size %d", pIdTable, size);
			pTbPtr = pIdTable;
			pRalDevList->os_private = pIdTable;
			phase = 2;
			goto loop;
		}
	}

	if (pRalDevList->os_private) {
		pTbPtr = (struct pci_device_id *)pRalDevList->os_private;
		while(pTbPtr->vendor != 0){
			BT_WARN("pci_device_id: vendor=0x%x, device=0x%x", pTbPtr->vendor, pTbPtr->device);
			pTbPtr++;
		}
	}
	return ((pRalDevList->os_private == NULL) ? -1 : 0);
}
#endif /* OS_ABL_SUPPORT */


/*******************************************************************************

	Functions used for OS hooking functions.

 *******************************************************************************/
int rtbt_iface_pci_hook(struct rtbt_dev_entry *pIdTb)
{

#if 1 // def OS_ABL_SUPPORT
	rtbt_linux_pci_ids_create(pIdTb);

	rtbt_pci_driver.id_table = (struct pci_device_id *)pIdTb->os_private;
	if (rtbt_pci_driver.id_table  == NULL){
		BT_ERR("id_table is NULL!");
		return -1;
	}
#else
	//rtbt_pci_driver.id_table = rtbt_pci_ids;
	//BT_WARN("rtbt_iface_pci_hook! IdCnt=%d!rtbt_pci_ids=0x%x\n", IdCnt, (ULONG)rtbt_pci_ids);
#endif

		return pci_register_driver(&rtbt_pci_driver);

}


int rtbt_iface_pci_unhook(struct rtbt_dev_entry *pIdTb)
{
	pci_unregister_driver(&rtbt_pci_driver);

#if 1 //def OS_ABL_SUPPORT
	if (pIdTb->os_private)
		kfree(pIdTb->os_private);
	rtbt_pci_driver.id_table = NULL;
#endif /* OS_ABL_SUPPORT */

	BT_INFO("remove rtbt_pci_driver done!");
	return 0;
}

UINT8
BthGetTxRingSize(
	__in  PRTBTH_ADAPTER	pAd,
	__in  UINT8				RingIdx
	)
{
	UNREFERENCED_PARAMETER(pAd);

	switch(RingIdx)
	{
		//case TX_RING_INQ_IDX:
		//	return TX_INQ_RING_SIZE;

		//case TX_RING_INQ_SCAN_IDX:
		//	return TX_INQ_SCAN_RING_SIZE;

		case TX_RING_ASBU_IDX:
			return TX_ASBU_RING_SIZE;

		case TX_RING_PSBU_IDX:
			return TX_PSBU_RING_SIZE;

		case TX_RING_PSBC_IDX:
			return TX_PSBC_RING_SIZE;

		case TX_RING_SYNC_IDX:
		case TX_RING_SYNC_IDX+1:
		case TX_RING_SYNC_IDX+2:
			return TX_SYNC_RING_SIZE;

		case TX_RING_ACLU_IDX:
		case TX_RING_ACLU_IDX+1:
		case TX_RING_ACLU_IDX+2:
		case TX_RING_ACLU_IDX+3:
		case TX_RING_ACLU_IDX+4:
		case TX_RING_ACLU_IDX+5:
		case TX_RING_ACLU_IDX+6:
			return TX_ACLU_RING_SIZE;

		case TX_RING_ACLC_IDX:
		case TX_RING_ACLC_IDX+1:
		case TX_RING_ACLC_IDX+2:
		case TX_RING_ACLC_IDX+3:
		case TX_RING_ACLC_IDX+4:
		case TX_RING_ACLC_IDX+5:
		case TX_RING_ACLC_IDX+6:
			return TX_ACLC_RING_SIZE;

		case TX_RING_LEACL_IDX:
		case TX_RING_LEACL_IDX+1:
			return TX_LEACL_RING_SIZE;

		default:
			DebugPrint(ERROR, DBG_INIT, "BthGetTxRingSize: Wrong idx(%d)\n", RingIdx);
			return 0;
	}
}


UINT16
BthGetTxRingPacketSize(
	IN PRTBTH_ADAPTER pAd,
	IN UINT8 RingIdx)
{
	UNREFERENCED_PARAMETER(pAd);

	switch(RingIdx)
	{
		//case TX_RING_INQ_IDX:
		//	return TX_INQ_PACKET_SIZE;

		//case TX_RING_INQ_SCAN_IDX:
		//	return TX_INQ_SCAN_PACKET_SIZE;

		case TX_RING_ASBU_IDX:
			return TX_ASBU_PACKET_SIZE;

		case TX_RING_PSBU_IDX:
			return TX_PSBU_PACKET_SIZE;

		case TX_RING_PSBC_IDX:
			return TX_PSBC_PACKET_SIZE;

		case TX_RING_SYNC_IDX:
		case TX_RING_SYNC_IDX+1:
		case TX_RING_SYNC_IDX+2:
			return TX_SYNC_PACKET_SIZE;

		case TX_RING_ACLU_IDX:
		case TX_RING_ACLU_IDX+1:
		case TX_RING_ACLU_IDX+2:
		case TX_RING_ACLU_IDX+3:
		case TX_RING_ACLU_IDX+4:
		case TX_RING_ACLU_IDX+5:
		case TX_RING_ACLU_IDX+6:
			return TX_ACLU_PACKET_SIZE;

		case TX_RING_ACLC_IDX:
		case TX_RING_ACLC_IDX+1:
		case TX_RING_ACLC_IDX+2:
		case TX_RING_ACLC_IDX+3:
		case TX_RING_ACLC_IDX+4:
		case TX_RING_ACLC_IDX+5:
		case TX_RING_ACLC_IDX+6:
			return TX_ACLC_PACKET_SIZE;

		case TX_RING_LEACL_IDX:
		case TX_RING_LEACL_IDX+1:
			return TX_LEACL_PACKET_SIZE;

		default:
			DebugPrint(ERROR, DBG_INIT, "BthGetTxRingPacketSize: Wrong RingIdx %d\n", RingIdx);
			return 0;
	}
}


UINT32 BthGetTxRingOffset(
	IN PRTBTH_ADAPTER pAd,
	IN UINT8 RingIdx)
{
	UNREFERENCED_PARAMETER(pAd);

	switch(RingIdx)
	{
		//case TX_RING_INQ_IDX:
		//	return 0;

		//case TX_RING_INQ_SCAN_IDX:
		//	return TX_INQ_SCAN_RING_BASE_PTR0_OFFSET;

		case TX_RING_ASBU_IDX:
			return TX_ASBU_RING_BASE_PTR0_OFFSET;

		case TX_RING_PSBU_IDX:
			return TX_PSBU_RING_BASE_PTR0_OFFSET;

		case TX_RING_PSBC_IDX:
			return TX_PSBC_RING_BASE_PTR0_OFFSET;

		case TX_RING_SYNC_IDX:
			return TX_SYNC_RING_BASE_PTR0_OFFSET_BASE;
		case TX_RING_SYNC_IDX+1:
			return (TX_SYNC_RING_BASE_PTR0_OFFSET_BASE + (TX_SYNC_RING_SIZE * TXD_SIZE));
		case TX_RING_SYNC_IDX+2:
			return (TX_SYNC_RING_BASE_PTR0_OFFSET_BASE + 2*(TX_SYNC_RING_SIZE * TXD_SIZE));


		case TX_RING_ACLU_IDX:
			return TX_ACLU_RING_BASE_PTR0_OFFSET_BASE;
		case TX_RING_ACLU_IDX+1:
			return (TX_ACLU_RING_BASE_PTR0_OFFSET_BASE + (TX_ACLU_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLU_IDX+2:
			return (TX_ACLU_RING_BASE_PTR0_OFFSET_BASE + 2*(TX_ACLU_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLU_IDX+3:
			return (TX_ACLU_RING_BASE_PTR0_OFFSET_BASE + 3*(TX_ACLU_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLU_IDX+4:
			return (TX_ACLU_RING_BASE_PTR0_OFFSET_BASE + 4*(TX_ACLU_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLU_IDX+5:
			return (TX_ACLU_RING_BASE_PTR0_OFFSET_BASE + 5*(TX_ACLU_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLU_IDX+6:
			return (TX_ACLU_RING_BASE_PTR0_OFFSET_BASE + 6*(TX_ACLU_RING_SIZE * TXD_SIZE));

		case TX_RING_ACLC_IDX:
			return TX_ACLC_RING_BASE_PTR0_OFFSET_BASE;
		case TX_RING_ACLC_IDX+1:
			return (TX_ACLC_RING_BASE_PTR0_OFFSET_BASE + (TX_ACLC_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLC_IDX+2:
			return (TX_ACLC_RING_BASE_PTR0_OFFSET_BASE + 2*(TX_ACLC_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLC_IDX+3:
			return (TX_ACLC_RING_BASE_PTR0_OFFSET_BASE + 3*(TX_ACLC_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLC_IDX+4:
			return (TX_ACLC_RING_BASE_PTR0_OFFSET_BASE + 4*(TX_ACLC_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLC_IDX+5:
			return (TX_ACLC_RING_BASE_PTR0_OFFSET_BASE + 5*(TX_ACLC_RING_SIZE * TXD_SIZE));
		case TX_RING_ACLC_IDX+6:
			return (TX_ACLC_RING_BASE_PTR0_OFFSET_BASE + 6*(TX_ACLC_RING_SIZE * TXD_SIZE));

		case TX_RING_LEACL_IDX:
			return TX_LEACL_RING_BASE_PTR0_OFFSET_BASE;
		case TX_RING_LEACL_IDX+1:
			return (TX_LEACL_RING_BASE_PTR0_OFFSET_BASE + (TX_LEACL_RING_SIZE * TXD_SIZE));

		default:
			DebugPrint(ERROR, DBG_INIT, "BthGetTxRingOffset: Wrong idx(%d)\n", RingIdx);
			return 0;
	}
}


BOOLEAN BthEnableInterrupt(
	IN RTBTH_ADAPTER *pAd)
{
	INT_MASK_CSR_STRUC IntMaskCsr;

	IntMaskCsr.word = DELAYINTMASK;
	IntMaskCsr.field.TxDone = 0x3fffff;
	IntMaskCsr.field.RxDone = 0x3;
	IntMaskCsr.field.McuEvtInt = 1;
	IntMaskCsr.field.McuCmdInt = 1;
	IntMaskCsr.field.McuFErrorInt = 1;
	IntMaskCsr.field.LmpTmr0Int = 1;
	RT_IO_WRITE32(pAd, INT_MASK_CSR, IntMaskCsr.word);

	/*i =0;
	do
	{
		RT_IO_READ32(FdoData, INT_MASK_CSR, &temp);
		DBGPRINT(DBG_NIC_TRACE,( "-->%d thh time : NICEnableInterrupt %x\n", i, temp));
		if (temp != DELAYINTMASK)
			RT_IO_WRITE32(FdoData, INT_MASK_CSR, DELAYINTMASK);

		if (temp != 0)
			break;
		i++;
	}while(i < 10);*/

	return TRUE;
}


VOID BthDisableInterrupt(
	IN struct _RTBTH_ADAPTER *pAd)//sean wang
{
	ULONG   	value;
	value = 0x0;
	RT_IO_FORCE_WRITE32(pAd, INT_MASK_CSR, value);
}


NTSTATUS BthInitTxRingByIdx(
	IN PRTBTH_ADAPTER	pAd,
	IN UINT8				RingIdx,
	IN PVOID   			RingBaseVa,
	IN ULONG   			RingBasePaHigh,
	IN ULONG   			RingBasePaLow,
	IN PUINT32			pPerRingAllocBuffSz)
{
	struct rtbt_os_ctrl *os_ctrl = pAd->os_ctrl;
	NTSTATUS		Status = STATUS_SUCCESS;
	UCHAR			Index;
	PRT_TX_RING 	pTxRing;
	PRTMP_DMABUF	pTxDescRing;
	PRTMP_DMABUF	pDmaBuf;
	PTXD_STRUC  	pTxD;
	PVOID   			BufBaseVa;
	ULONG   			BufBasePa;
	UINT8			TxRingSize;
	UINT16			TxRingPacketSize;
	UINT32			TxRingOffset;


	RTBT_ASSERT(pPerRingAllocBuffSz);
	if (pPerRingAllocBuffSz == NULL)
		return STATUS_UNSUCCESSFUL;

	TxRingSize = BthGetTxRingSize(pAd, RingIdx);
	if (TxRingSize == 0)
	{
		Status = STATUS_UNSUCCESSFUL;
		return Status;
	}

	TxRingPacketSize = BthGetTxRingPacketSize(pAd, RingIdx);
	if (TxRingPacketSize == 0)
	{
		Status = STATUS_UNSUCCESSFUL;
		return Status;
	}

	pTxDescRing = &pAd->TxDescRing[RingIdx];
	pTxRing = &pAd->TxRing[RingIdx];

	pTxRing->TxCpuIdx = 0;
	pTxRing->TxDmaIdx = 0;
	pTxRing->TxSwFreeIdx = 0;

	pTxDescRing->AllocSize = TxRingSize * TXD_SIZE;
	pTxDescRing->AllocVa = (PUCHAR)RingBaseVa;
	pTxDescRing->AllocPa = RingBasePaLow;

	if ((RingIdx % 4) == 0)
		RtbtusecDelay(2);

	if (RingIdx > 0)
	{
		TxRingOffset = BthGetTxRingOffset(pAd, RingIdx);
		RT_IO_WRITE32(pAd, TX_BASE_PTR0 + RingIdx * RINGREG_DIFF, TxRingOffset);
	}
	else
	{
		RT_IO_WRITE32(pAd, TX_BASE_PTR0 + RingIdx * RINGREG_DIFF, RingBasePaLow);
	}

	//DBGPRINT(DBG_NIC_TRACE, ("TX_Base_PTR[%d]= %x.\n", Num, RingBasePaLow));
	DebugPrint(TRACE, DBG_INIT, "TX_Base_PTR(%02d)-regAddr:%x, regVal=0x%x\n", RingIdx, TX_BASE_PTR0 + RingIdx * RINGREG_DIFF, RingBasePaLow);

	for (Index = 0; Index < TxRingSize; Index++)
	{
		pTxRing->TxCpuIdx = 0;

		pTxRing->Cell[Index].AllocSize = TXD_SIZE;
		pTxRing->Cell[Index].AllocVa = RingBaseVa;
		pTxRing->Cell[Index].AllocPa = RingBasePaLow;

		pDmaBuf = &pTxRing->Cell[Index].DmaBuf;
		pDmaBuf->AllocSize = TxRingPacketSize;
		rtbt_dma_mem_alloc(os_ctrl->if_dev,
						pDmaBuf->AllocSize,
						FALSE,
						&pDmaBuf->AllocVa,
						&pDmaBuf->AllocPa);

		if (pDmaBuf->AllocVa== NULL)
		{
			DebugPrint(ERROR, DBG_INIT, "Failed to allocate a tx ring big buffer\n");

			Status = STATUS_UNSUCCESSFUL;
			return Status;
		}
		else
		{
			*pPerRingAllocBuffSz += pDmaBuf->AllocSize;
			DebugPrint(TRACE, DBG_INIT, "RingIdx %d, Index %d, allocSize %d, PerRingAllocBuffSz %d\n",
				RingIdx, Index, pDmaBuf->AllocSize, *pPerRingAllocBuffSz);
		}

		BufBasePa = pDmaBuf->AllocPa;

		BufBaseVa = pDmaBuf->AllocVa;
		//DBGPRINT(DBG_NIC_TRACE, ("\tTxD[%d]->BufBasePa= %x.\n", Index, BufBasePa));

		// link the pre-allocated TxBuf to TXD
		pTxD = (PTXD_STRUC)pTxRing->Cell[Index].AllocVa;
		pTxD->SDPtr0 = BufBasePa;
		pTxD->QSEL = RingIdx;
		// initilize set DmaDone=1 first before enable DMA.
		pTxD->DmaDone = 1;

		// Next Descriptor
		RingBasePaLow += TXD_SIZE;
		RingBaseVa = (PUCHAR)RingBaseVa + TXD_SIZE;
	}

	return Status;
}


/*++
Routine Description:

	Initialize send data structures. Can be called at DISPATCH_LEVEL.

Arguments:

	FdoData 	Pointer to our adapter

Return Value:

	None

--*/
NTSTATUS BthInitSend(
	IN RTBTH_ADAPTER *pAd)
{
	struct rtbt_os_ctrl *os_ctrl = pAd->os_ctrl;
	NTSTATUS			Status = STATUS_SUCCESS;
	UCHAR   			Num; //, Index;
	//PRT_TX_RING 		pTxRing;
	//PRTMP_DMABUF		pDmaBuf;
	//PTXD_STRUC  		pTxD;
	PVOID   			RingBaseVa;
	PHYSICAL_ADDRESS	RingBasePa, RingBasePaHigh, RingBasePaLow;
	//PVOID   			BufBaseVa;
	//PHYSICAL_ADDRESS	BufBasePa;
	//PHYSICAL_ADDRESS	RingPa;
	RTMP_DMABUF 		*pTxDescBuf;
	UINT8				TxRingSize;

	DebugPrint(TRACE, DBG_INIT, "--> BthInitSend\n");

	for (Num = 0; Num < NUM_OF_TX_RING; Num++)
	{
		DebugPrint(TRACE, DBG_INIT, "TxRing(Idx:%2d, RingSize:%2d, RingPktSz:%4d, RingOffset:%4d)\n",
				Num,
				BthGetTxRingSize(pAd, Num),
				BthGetTxRingPacketSize(pAd, Num),
				BthGetTxRingOffset(pAd, Num));
	}

	/*
		For each TxDesc, the size is "TXD_SIZE"
		For each TxRing, we allocate "TX_RING_SIZE" TxDescriptors
		Totally we have to allocate memory with size
			= "NUM_OF_TX_RING" * "TX_RING_SIZE" * "TXD_SIZE"
	*/
	pTxDescBuf = &pAd->TxDescMemPtr;
	pTxDescBuf->AllocSize = (TX_RING_TOTAL_DESC_NUM * TXD_SIZE + 15);
	rtbt_dma_mem_alloc(os_ctrl->if_dev,
						pTxDescBuf->AllocSize,
						FALSE,
						&pTxDescBuf->AllocVa,
						&pTxDescBuf->AllocPa);
	if (pTxDescBuf->AllocVa == NULL)
	{
		DebugPrint(ERROR, DBG_INIT, "Failed to allocate a Tx desc buffer\n");
		return STATUS_UNSUCCESSFUL;
	}
//	RingBaseVa = (PVOID)(((UINT32)pTxDescBuf->AllocVa + 0xf) & (~0xf));
    RingBaseVa = (PVOID)(((unsigned long)pTxDescBuf->AllocVa + 0xf) & (~0xf));
	RingBasePa = (PHYSICAL_ADDRESS)(((UINT32)pTxDescBuf->AllocPa + 0xf) & (~0xf));
	RtlZeroMemory(pTxDescBuf->AllocVa, pTxDescBuf->AllocSize);
	DebugPrint(INFO, DBG_INIT, "AllocTxDescRing at 0x%x(phy_addr:0x%x, len 0x%x)!\n"
							"\tAfter Aligned, RingBaseVa at 0x%x(0x%x)\n",
				pTxDescBuf->AllocVa, pTxDescBuf->AllocPa,
				pTxDescBuf->AllocSize, RingBaseVa, RingBasePa);

	RingBasePaHigh = 0;
	RingBasePaLow = RingBasePa;

	for (Num = 0; Num < NUM_OF_TX_RING; Num++)
	{
		// TODO: shiang, these parts not finish yet!
		UINT32				TotalAllocBuffSz = 0;
		UINT32				PerRingAllocBuffSz = 0;

	DebugPrint(INFO, DBG_INIT, "Assign TxRing(Idx:%d) at virtAddr(0x%x), phyAddr(0x%x)\n",
				Num, RingBaseVa, RingBasePaLow);
		if (BthInitTxRingByIdx(pAd, Num, RingBaseVa, RingBasePaHigh, RingBasePaLow, &PerRingAllocBuffSz) != STATUS_SUCCESS)
		{
			return Status;
		}
		else
		{
			// Update Per Ring info.
			TxRingSize = BthGetTxRingSize(pAd, Num);
			RingBasePaLow += TxRingSize * TXD_SIZE;
			RingBaseVa = (PUCHAR)RingBaseVa + TxRingSize * TXD_SIZE;
			TotalAllocBuffSz += PerRingAllocBuffSz;
			PerRingAllocBuffSz = 0;

			RT_IO_WRITE32(pAd, TX_MAX_CNT0 + Num * RINGREG_DIFF, TxRingSize);
			RT_IO_WRITE32(pAd, TX_CTX_IDX0 + Num * RINGREG_DIFF, 0);

			DebugPrint(TRACE, DBG_INIT, "RingIdx %d, PerRingAllocBuffSz %d, TotalAllocBuffSz %d\n",
				Num,
				PerRingAllocBuffSz,
				TotalAllocBuffSz);
		}
	}

	DebugPrint(TRACE, DBG_INIT, "<-- BthInitSend\n");

	return Status;

#if 0
err:
#endif
	return STATUS_UNSUCCESSFUL;

}


/*++
Routine Description:

	Initialize receive data structures

Arguments:

	FdoData 	Pointer to our adapter

Return Value:

--*/
NTSTATUS BthInitRecv(IN RTBTH_ADAPTER *pAd)
{
	struct rtbt_os_ctrl *os_ctrl = pAd->os_ctrl;
	NTSTATUS			Status = STATUS_SUCCESS;
	UCHAR   			Num, Index;
	PRT_RX_RING 		pRxRing;
	PRTMP_DMABUF		pRxDescRing;
	PRTMP_DMABUF		pDmaBuf;
	PRXD_STRUC  		pRxD;
	PVOID   			RingBaseVa;
	PHYSICAL_ADDRESS   	RingBasePa;
	PVOID   			BufBaseVa;
	ULONG   			BufBasePa;
	PHYSICAL_ADDRESS	RingPa;

	DebugPrint(TRACE, DBG_INIT, "BthInitRecv==>\n");

	// RX_BASE_PTR1 16bit offset
	rtbt_dma_mem_alloc(os_ctrl->if_dev, NUM_OF_RX_RING * RX_RING_SIZE * RXD_SIZE, FALSE, &RingBaseVa, &RingBasePa);
	if (RingBaseVa == NULL)
	{
		DebugPrint(ERROR, DBG_INIT, "Failed to allocate a Rx descriptor big buffer\n");

		Status = STATUS_UNSUCCESSFUL;
		return Status;
	}
	RtlZeroMemory(RingBaseVa, NUM_OF_RX_RING * RX_RING_SIZE * RXD_SIZE);


	for (Num = 0; Num < NUM_OF_RX_RING; Num++)
	{
		pRxDescRing = &pAd->RxDescRing[Num];
		pRxRing = &pAd->RxRing[Num];

		pRxRing->RxCpuIdx = RX_RING_SIZE - 1;
		pRxRing->RxDmaIdx = 0;
		pRxRing->RxSwReadIdx = 0;

		pRxDescRing->AllocSize = RX_RING_SIZE * RXD_SIZE;
		pRxDescRing->AllocVa = (PUCHAR)RingBaseVa;
		pRxDescRing->AllocPa = RingBasePa;

		/*
			RX_BASE_PRT0: absolute 32bit address
			RX_BASE_PTR1: offset address
		*/
		RingPa = ((Num == 0) ? RingBasePa : (Num * RX_RING_SIZE * RXD_SIZE));
		RT_IO_WRITE32(pAd, RX_BASE_PTR0 + Num * RINGREG_DIFF, RingPa);
		RT_IO_WRITE32(pAd, RX_MAX_CNT + Num * RINGREG_DIFF, RX_RING_SIZE);
		RT_IO_WRITE32(pAd, RX_CRX_IDX + Num * RINGREG_DIFF, (RX_RING_SIZE-1));

		DebugPrint(ERROR, DBG_INIT, "RX_Base_PTR%d(0x%02x)= 0x%x\n",
							Num, RX_BASE_PTR0 + Num * RINGREG_DIFF, RingPa);


		RingBaseVa = pRxDescRing->AllocVa;
		for (Index = 0; Index < RX_RING_SIZE; Index++)
		{
			pRxRing->Cell[Index].AllocSize = RXD_SIZE;
			pRxRing->Cell[Index].AllocVa = RingBaseVa;
			pRxRing->Cell[Index].AllocPa = RingBasePa;

			pDmaBuf = &pRxRing->Cell[Index].DmaBuf;
			pDmaBuf->AllocSize = RX_PACKET_SIZE;
			rtbt_dma_mem_alloc(os_ctrl->if_dev, pDmaBuf->AllocSize, FALSE, &pDmaBuf->AllocVa, &pDmaBuf->AllocPa);
			if (pDmaBuf->AllocVa== NULL)
			{
				DebugPrint(ERROR, DBG_INIT, "RxRing[%d]:Failed to allocate rx buffer for cell[%d]\n", Num, Index);
				Status = STATUS_UNSUCCESSFUL;
				return Status;
			}

			BufBaseVa = pDmaBuf->AllocVa;
			BufBasePa = pDmaBuf->AllocPa;

			//DBGPRINT(DBG_NIC_TRACE, ("\tRxD[%d]->BufBasePa= %x.\n", Index, BufBasePa));

			// Write RxD buffer address & allocated buffer length
			pRxD = (PRXD_STRUC)pRxRing->Cell[Index].AllocVa;
			pRxD->SDP0 = BufBasePa;
			pRxD->DDONE = 0;

			// Next Descriptor
			RingBasePa += RXD_SIZE;
			RingBaseVa = (PUCHAR)RingBaseVa + RXD_SIZE;
		}
	}

	DebugPrint(TRACE, DBG_INIT, "<-- BthInitRecv\n");

	return Status;
}

VOID BthFreeRfd(IN RTBTH_ADAPTER *pAd)
{
	struct rtbt_os_ctrl *os_ctrl = pAd->os_ctrl;
	UCHAR   index;
	UCHAR   RingIdx = 0;
	RT_DMACB *pDmaCb;
	RTMP_DMABUF *pRxDescRing, *pDmaBuf;
	UINT8	TxRingSize;

	DebugPrint(TRACE, DBG_INIT, "BthFreeRfd ==>\n");

	/* Free RxRing */
	for (RingIdx = 0; RingIdx < NUM_OF_RX_RING; RingIdx++)
	{
		for (index = 0; index < RX_RING_SIZE; index++)
		{
			pDmaCb = &pAd->RxRing[RingIdx].Cell[index];
			pDmaCb->AllocVa = NULL;  // This is freed above. Assign null directly.
			pDmaBuf = &pAd->RxRing[RingIdx].Cell[index].DmaBuf;
			if (pDmaBuf->AllocVa != NULL)
			{
				rtbt_dma_mem_free(os_ctrl->if_dev, pDmaBuf->AllocSize, pDmaBuf->AllocVa, pDmaBuf->AllocPa);
				pDmaBuf->AllocVa = NULL;
			}
		}
	}
	DebugPrint(TRACE, DBG_INIT, "Free Rx staff done\n");

	pRxDescRing = &pAd->RxDescRing[0];
	if (pRxDescRing->AllocVa != NULL)
	{
		rtbt_dma_mem_free(os_ctrl->if_dev, pRxDescRing->AllocSize, pRxDescRing->AllocVa, pRxDescRing->AllocPa);
		pRxDescRing->AllocVa = NULL;
	}
	DebugPrint(TRACE, DBG_INIT, "Free RxDescRing[0] staff done\n");


	/* Free Tx Buffer Ring */
	for (RingIdx = 0; RingIdx < NUM_OF_TX_RING; RingIdx++)
	{
		pAd->TxDescRing[RingIdx].AllocVa = NULL;
		TxRingSize = BthGetTxRingSize(pAd, RingIdx);
		for (index = 0; index < TxRingSize; index++)
		{
			pAd->TxRing[RingIdx].Cell[index].AllocVa = NULL; // This is freed above. Assign null directly.
			pDmaBuf = &pAd->TxRing[RingIdx].Cell[index].DmaBuf;
			if (pDmaBuf->AllocVa != NULL)
			{
				rtbt_dma_mem_free(os_ctrl->if_dev, pDmaBuf->AllocSize, pDmaBuf->AllocVa, pDmaBuf->AllocPa);
				pDmaBuf->AllocVa = NULL;
			}
		}
	}
	DebugPrint(TRACE, DBG_INIT, "Free TxRing staff done\n");
	/* Free Tx Descriptor Ring */
	pDmaBuf = &pAd->TxDescMemPtr;
	if (pDmaBuf->AllocVa != NULL)
	{
		rtbt_dma_mem_free(os_ctrl->if_dev, pDmaBuf->AllocSize, pDmaBuf->AllocVa, pDmaBuf->AllocPa);
		pDmaBuf->AllocVa = NULL;
	}
	DebugPrint(TRACE, DBG_INIT, "Free TxDescRing[0] staff done\n");


	DebugPrint(TRACE, DBG_INIT, " BthFreeRfd <==\n");

}

VOID reg_dump_txdesc(struct _RTBTH_ADAPTER *pAd)//sean wang
{
	int num;
	UINT32 regAddr, regVal;


	DebugPrint(NONE, DBG_MISC, "%s():Dump TxDesc registers:\n", __FUNCTION__);
	for (num = 0; num < NUM_OF_TX_RING; num++)
	{
		DebugPrint(NONE, DBG_MISC, "TX_RING_%02d:\n", num);
		regAddr = TX_BASE_PTR0 + num * RINGREG_DIFF;
		RT_IO_READ32(pAd, regAddr, &regVal);
		DebugPrint(NONE, DBG_MISC, "\tBASE_PTR:(addr=0x%x, val=0x%x)\n", regAddr, regVal);
		regAddr = TX_MAX_CNT0 + num * RINGREG_DIFF;
		RT_IO_READ32(pAd, regAddr, &regVal);
		DebugPrint(NONE, DBG_INIT, "\tMAX_CNT:(addr=0x%x, val=0x%x)\n", regAddr, regVal);
		regAddr = TX_CTX_IDX0 + num * RINGREG_DIFF;
		RT_IO_READ32(pAd, regAddr, &regVal);
		DebugPrint(NONE, DBG_INIT, "\tMAX_CNT:(addr=0x%x, val=0x%x)\n", regAddr, regVal);
	}
}

VOID reg_dump_rxdesc(struct _RTBTH_ADAPTER *pAd)//sean wang
{
	int num;
	UINT32 regAddr, regVal;


	DebugPrint(NONE, DBG_MISC, "%s():Dump RxDesc registers:\n", __FUNCTION__);
	for (num = 0; num < NUM_OF_RX_RING; num++)
	{
		DebugPrint(NONE, DBG_MISC, "RX_RING_%02d:\n", num);
		regAddr = RX_BASE_PTR0 + num * RINGREG_DIFF;
		RT_IO_READ32(pAd, regAddr, &regVal);
		DebugPrint(NONE, DBG_MISC, "\tBASE_PTR:(addr=0x%x, val=0x%x)\n", regAddr, regVal);
		regAddr = RX_MAX_CNT + num * RINGREG_DIFF;
		RT_IO_READ32(pAd, regAddr, &regVal);
		DebugPrint(NONE, DBG_INIT, "\tMAX_CNT:(addr=0x%x, val=0x%x)\n", regAddr, regVal);
		regAddr = RX_CRX_IDX + num * RINGREG_DIFF;
		RT_IO_READ32(pAd, regAddr, &regVal);
		DebugPrint(NONE, DBG_INIT, "\tMAX_CNT:(addr=0x%x, val=0x%x)\n", regAddr, regVal);
	}
}

VOID RtbtResetPDMA(IN RTBTH_ADAPTER *pAd)
{
	UCHAR   			Num, Index;
	PRT_RX_RING 		pRxRing;
	PRXD_STRUC  		pRxD;
	PRT_TX_RING 		pTxRing;
	PRTMP_DMABUF		pDmaBuf;
	PTXD_STRUC  		pTxD;
	UINT8				TxRingSize;
	UINT32				regVal;

	// Reset PDMA TX:DTX and RX:DRX
	RT_IO_WRITE32(pAd, PDMA_RST_CSR, 0x33fffff);

	for (Num = 0; Num < NUM_OF_TX_RING; Num++)
	{
		if ((Num % 4) == 0)
			rtbt_usec_delay(2);

		TxRingSize = BthGetTxRingSize(pAd, Num);
//+++Add by shiang for debug
		regVal = (Num > 0) ? BthGetTxRingOffset(pAd, Num) : pAd->TxDescRing[Num].AllocPa;
		RT_IO_WRITE32(pAd, TX_BASE_PTR0 + Num * RINGREG_DIFF, regVal);
//---Add by shiang for debug
		RT_IO_WRITE32(pAd, TX_MAX_CNT0 + Num * RINGREG_DIFF, TxRingSize);
		RT_IO_WRITE32(pAd, TX_CTX_IDX0 + Num * RINGREG_DIFF, 0);
	}

	for (Num = 0; Num < NUM_OF_RX_RING; Num++)
	{
//+++Add by shiang for debug
		PHYSICAL_ADDRESS	RingPa;

		RingPa = ((Num == 0) ? pAd->RxDescRing[Num].AllocPa : (Num * RX_RING_SIZE * RXD_SIZE));
		RT_IO_WRITE32(pAd, RX_BASE_PTR0 + Num * RINGREG_DIFF, RingPa);
//---Add by shiang for debug
		RT_IO_WRITE32(pAd, RX_MAX_CNT + Num * RINGREG_DIFF, RX_RING_SIZE);
		RT_IO_WRITE32(pAd, RX_CRX_IDX + Num * RINGREG_DIFF, (RX_RING_SIZE-1));
	}

	for (Num = 0; Num < NUM_OF_RX_RING; Num++)
	{
		pRxRing = &pAd->RxRing[Num];

		pRxRing->RxCpuIdx = RX_RING_SIZE - 1;
		pRxRing->RxDmaIdx = 0;
		pRxRing->RxSwReadIdx = 0;

		for (Index = 0; Index < RX_RING_SIZE; Index++)
		{
			// Write RxD buffer address & allocated buffer length
			pRxD = (PRXD_STRUC)pRxRing->Cell[Index].AllocVa;
			if (pRxD)
				pRxD->DDONE = 0;
		}
	}

	for (Num = 0; Num < NUM_OF_TX_RING; Num++)
	{
		pTxRing = &pAd->TxRing[Num];

		pTxRing->TxDmaIdx = 0;
		pTxRing->TxSwFreeIdx = 0;
		pTxRing->TxCpuIdx = 0;

		TxRingSize = BthGetTxRingSize(pAd, Num);
		for (Index = 0; Index < TxRingSize; Index++)
		{
			pDmaBuf = &pTxRing->Cell[Index].DmaBuf;
			pTxD = (PTXD_STRUC)pTxRing->Cell[Index].AllocVa;
			// initilize set DmaDone=1 first before enable DMA.
			if (pTxD)
				pTxD->DmaDone = 1;
		}
	}

DebugPrint(TRACE, DBG_MISC, "%s():dump tx/rx desc register value\n", __FUNCTION__);
	//reg_dump_txdesc(pAd); // muted debug output
	//reg_dump_rxdesc(pAd); // here too
DebugPrint(TRACE, DBG_MISC, "<--%s()\n", __FUNCTION__);
}


VOID
BthRxPacket(
	PRTBTH_ADAPTER  pAd,
	UCHAR   		RingIdx,
	ULONG   		RxDmaIdx,
	PULONG  		pSwReadIndex
	)
{
//	BOOLEAN			bConsumed;
	PRXD_STRUC		pRxD;
	PRXBI_STRUC		pRxBI;
	ULONG			PacketLen;
	PULONG			pData;
	ULONG			GET, Count;

	// Get how many packet become readable.
	if (RxDmaIdx > *pSwReadIndex)
		GET = RxDmaIdx - *pSwReadIndex;
	else
		GET = RxDmaIdx + RX_RING_SIZE - *pSwReadIndex;

DebugPrint(TRACE, DBG_MISC, "BthRxPacket ..... GET = %d, RxDmaIdx = 0x%08x, pSwReadIndex = 0x%08x\n",
    GET, RxDmaIdx, *pSwReadIndex);


// A for loop to receive all packets that are readable.
	for (Count = 0; Count < GET; Count++)
	{
		pRxD = (PRXD_STRUC)pAd->RxRing[RingIdx].Cell[*pSwReadIndex].AllocVa;
		pData = (PULONG)pAd->RxRing[RingIdx].Cell[*pSwReadIndex].DmaBuf.AllocVa;
		//  Sanity check
		if (BthPacketFilter(pAd, RingIdx, RxDmaIdx, pSwReadIndex) == FALSE)
		{
			//DebugPrint(TRACE, DBG_PDMA_DATA, "BthPacketFilter break. RingIdx = %d, Read Idx = %d GET=%d\n", RingIdx,*pSwReadIndex, GET);
			break;
		}

		pRxBI   = (PRXBI_STRUC) (pData);
		PacketLen = pRxD->SDL0;

		// pRxD->SDL0: word boundary of (RXBI + Payload)
		RTBT_ASSERT(pRxBI->Len <= pRxD->SDL0);

       //put data to rx queue
       rtbth_rx_packet(pAd, *(pRxBI),  ((PUINT8)pData)+sizeof(RXBI_STRUC), pRxBI->Len);

		// Keep the packet in PDMA
//sean wang marked for USLM
//if (bConsumed == FALSE)
		//	break;
        //

		// Host finish reading. Set Ddone to 0.
		pRxD->DDONE = 0;
		// Increase ReadIdx by 1
		INC_RING_INDEX(*pSwReadIndex, RX_RING_SIZE);
//		DebugPrint(INFO, DBG_PDMA_DATA, "SDL0 = %d. RxBI = %x, %x, %x, %x. New  SwReadIndex = %d\n",
//		  					 PacketLen, *pData, *(pData+1), *(pData+2), *(pData+3), *pSwReadIndex);
	}

}


/*++
Routine Description:

	handle RXDone interrupt routine..

Arguments:

	FdoData 	Pointer to our FdoData

Return Value:

	 None

--*/
VOID
BthHandleRecvInterrupt(
	IN PRTBTH_ADAPTER pAd)
{
	ULONG   	RxSwReadIdx = 0;
	ULONG   	RxDmaIdx = 0;
	//PRXD_STRUC		pRxD;
	UCHAR   	RingIdx;
	KIRQL oldirql;

	for (RingIdx = 0; RingIdx < NUM_OF_RX_RING; RingIdx++)
	{
		ral_spin_lock(&pAd->RcvLock,&oldirql);
		RT_IO_READ32(pAd, (RX_DRX_IDX + RingIdx * RINGREG_DIFF), &RxDmaIdx);

//		DebugPrint(INFO, DBG_PDMA_DATA, "BthHandleRecvInterrupt - RingIdx=%d, RxDmaIdx=%d. RxSwReadIdx = %d. RxCpuIdx = %d)\n",
//		  					  RingIdx, RxDmaIdx, pAd->RxRing[RingIdx].RxSwReadIdx, pAd->RxRing[RingIdx].RxCpuIdx);

		// Service Irp, return CPU index
		RxSwReadIdx = pAd->RxRing[RingIdx].RxSwReadIdx;

		BthRxPacket(pAd, RingIdx, RxDmaIdx, &RxSwReadIdx);

		// If NICRxPacket successfully. update necessary parameter and write to register.
		if (pAd->RxRing[RingIdx].RxSwReadIdx != RxSwReadIdx)
		{
			pAd->RxRing[RingIdx].RxSwReadIdx = RxSwReadIdx;
			pAd->RxRing[RingIdx].RxCpuIdx = (RxSwReadIdx == 0)?(RX_RING_SIZE-1):(RxSwReadIdx-1);
			RT_IO_WRITE32(pAd, (RX_CRX_IDX + RingIdx * RINGREG_DIFF), pAd->RxRing[RingIdx].RxCpuIdx);
		}
		ral_spin_unlock(&pAd->RcvLock,oldirql);
	}

}


BOOLEAN BthPacketFilter(
	PRTBTH_ADAPTER	pAd,
	UCHAR			RingIdx,
	ULONG			RxDmaIdx,
	PULONG			pSwReadIndex)
{
	PRXD_STRUC	pRxD;

	if (*pSwReadIndex == RxDmaIdx)
	{
//		DebugPrint(TRACE, DBG_MISC, "3298 RxDone - No packets in RX Int (pAd->RxRing[%d].RxSwReadIdx=%d)\n",
//								RingIdx, pAd->RxRing[RingIdx].RxSwReadIdx);
		return FALSE;
	}

	// Point to Rx indexed rx ring descriptor
	pRxD = (PRXD_STRUC)pAd->RxRing[RingIdx].Cell[*pSwReadIndex].AllocVa;
	// Check DDONE. Still not DDONE, return.
	if (pRxD->DDONE == 0)
	{
		DebugPrint(TRACE, DBG_MISC, "3298 pRxD[%d]->DDONE=%d. return \n" ,pAd->RxRing[RingIdx].RxSwReadIdx, pRxD->DDONE);
		return FALSE;
	}

	return TRUE;
}


/*++
Routine Description:

	handle TXDone interrupt routine..

Arguments:

	FdoData 	Pointer to our FdoData

Return Value:

	 None

--*/
VOID
BthHandleTxRingDmaDoneInterrupt(
	IN  PRTBTH_ADAPTER  	 pAd,
	IN  INT_SOURCE_CSR_STRUC TxRingBitmap)
{
	PRT_TX_RING 	pTxRing;
	PTXD_STRUC  	pTxD;
	UCHAR   		RingIdx;
	UCHAR   		Count;
	UINT8		TxRingSize;

	for (RingIdx = 0; RingIdx < NUM_OF_TX_RING; RingIdx++)
	{
		Count = 0;
		pTxRing = &pAd->TxRing[RingIdx];

		KeAcquireSpinLockAtDpcLevel(&pAd->SendLock);

		RT_IO_READ32(pAd, TX_DTX_IDX0 + RingIdx * RINGREG_DIFF, &pTxRing->TxDmaIdx);
		while ((pTxRing->TxDmaIdx != 0xffffffff) && (pTxRing->TxSwFreeIdx != pTxRing->TxDmaIdx))
		{
#if  0
			if (pTxRing->TxDmaIdx >= TX_RING_SIZE)
			{
				DebugPrint(ERROR, DBG_MISC, "BthHandleTxRingDmaDoneInterrupt: Invalid pTxRing->TxDmaIdx = 0x%08x\n", pTxRing->TxDmaIdx);

				//
				// Invalid value, try to read it again.
				// It should be fine after read again.
				//
				RT_IO_READ32(pAd, TX_DTX_IDX0 + RingIdx * RINGREG_DIFF, &pTxRing->TxDmaIdx);
				Count++;
				if (Count > 10)
					break;
				continue;
			}
#endif
			pTxD = (PTXD_STRUC) (pTxRing->Cell[pTxRing->TxSwFreeIdx].AllocVa);
			if(pTxD->DmaDone == 0)
			{
				break;
			}
			//pTxD->DmaDone = 0;

//DebugPrint(TRACE, DBG_MISC, "-->%s(TxRing-%d):pTxRing->TxDmaIdx=0x%x, pTxRing->TxSwFreeIdx=0x%x!\n", __FUNCTION__, RingIdx, pTxRing->TxDmaIdx, pTxRing->TxSwFreeIdx);
			TxRingSize = BthGetTxRingSize(pAd, RingIdx);
			//INC_RING_INDEX(pTxRing->TxSwFreeIdx, TX_RING_SIZE);
			INC_RING_INDEX(pTxRing->TxSwFreeIdx, TxRingSize);
			KeSetEvent(&pAd->CoreEventTriggered, 1, FALSE);
		}

		KeReleaseSpinLockFromDpcLevel(&pAd->SendLock);
	}
}


int rtbt_pci_isr(IN void *handle)
{
	RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)handle;
	INT_SOURCE_CSR_STRUC IntSource;
	UINT32  	IntStatus, IntMask;
	//ULONG	irqflags;


	IntSource.word = 0x00000000L;
	//
	// We process the interrupt if it's not disabled and it's active
	//
	//RT_PCI_IO_READ32(pAd->CSRAddress + INT_SOURCE_CSR, &IntStatus);
	//RT_PCI_IO_READ32(pAd->CSRAddress + INT_MASK_CSR, &IntMask);
	IntStatus = *(PUINT32)(pAd->CSRAddress + INT_SOURCE_CSR);
	IntMask = *(PUINT32)(pAd->CSRAddress + INT_MASK_CSR);
	if (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE)
		|| ((IntStatus & IntMask) == 0))
	{
DebugPrint(INFO, DBG_INIT, "pAd->Flags=0x%x(%d), IntStatus=0x%x, IntMask=0x%x(0x%x)\n",
		pAd->Flags, RT_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE),
		IntStatus, IntMask, IntStatus & IntMask);
		return -1;
	}

	//BthDisableInterrupt(pAd);

	RT_IO_READ32(pAd, INT_SOURCE_CSR, &IntSource.word);

DebugPrint(TRACE, DBG_INIT,"***************INT_SOURCE_CSR = %08x\n", IntSource.word);

	if (IntSource.word == 0xffffffff)
	{
		//if (!RT_TEST_FLAG(fdoData, fRTMP_ADAPTER_SLEEP_IN_PROGRESS))
		{
			RT_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST);
			DebugPrint(ERROR, DBG_INIT, "Hardware has been removed!(Surprise Removal) \n");
		}
		return -1;
	}

	RT_IO_WRITE32(pAd, INT_SOURCE_CSR, IntSource.word); // write 1 to clear

	if (IntSource.field.TxDone) {
		//ral_spin_lock(KSPIN_LOCK * pLock, unsigned long * irq_flag);
		//if (pAd->int_disable_mask & INT_MASK_TXDONE == 0)
		{
			//rtbt_int_disable(pAd, INT_MASK_TXDONE);
			BthHandleTxRingDmaDoneInterrupt(pAd, IntSource);
		}
	}

	if (IntSource.field.RxDone ||IntSource.field.RxDelayDone) {

	DebugPrint(TRACE, DBG_INIT, "RX Done interrupt comming... \n");

		BthHandleRecvInterrupt(pAd);
	}

	if (IntSource.field.McuCmdInt) {
     _rtbth_us_event_notification(pAd, INT_MCUCMD_EVENT);
    }

	if (IntSource.field.McuEvtInt) {
     _rtbth_us_event_notification(pAd, INT_MCUEVT_EVENT);
	}

	if (IntSource.field.LmpTmr0Int) {
#ifdef LINUX

		KeSetEvent(&pAd->HCISCODataEventTriggered, 1, FALSE);
		KeAcquireSpinLockAtDpcLevel(&pAd->scoSpinLock);
		pAd->sco_tx_cnt++;
//		_BRM_Transmit_Sync_Packet(pAd);
		KeReleaseSpinLockFromDpcLevel(&pAd->scoSpinLock);

		//hps_sco_traffic_notification(pAd);

#endif /* LINUX */
	}

	if (IntSource.field.McuFErrorInt) {
        _rtbth_us_event_notification(pAd, INT_LMPTMR_EVENT);
        _rtbth_us_event_notification(pAd, INT_LMPERR_EVENT);
	}


	//BthEnableInterrupt(pAd);

	return 0;
}

int rtbt_pci_resource_deinit(struct rtbt_os_ctrl *os_ctrl)
{
	RTBTH_ADAPTER *pAd;

	ASSERT(os_ctrl);
	ASSERT(os_ctrl->dev_ctrl);

DebugPrint(TRACE, DBG_MISC, "-->%s()\n", __FUNCTION__);

	pAd = (RTBTH_ADAPTER *)os_ctrl->dev_ctrl;
	BthFreeRfd(pAd);

DebugPrint(TRACE, DBG_MISC, "<--%s()\n", __FUNCTION__);

	return 0;
}

int rtbt_pci_resource_init(struct rtbt_os_ctrl *os_ctrl)
{
	RTBTH_ADAPTER *pAd;
	NTSTATUS Status;

	ASSERT(os_ctrl);
	ASSERT(os_ctrl->dev_ctrl);

DebugPrint(TRACE, DBG_MISC, "-->%s()\n", __FUNCTION__);

	pAd = (RTBTH_ADAPTER *)os_ctrl->dev_ctrl;
	Status = BthInitSend(pAd);
	if (!NT_SUCCESS (Status))
	{
		DebugPrint(ERROR, DBG_INIT, "BthInitSend fail(%x)\n", Status);
		goto fail;
	}

	Status = BthInitRecv(pAd);
	if (!NT_SUCCESS (Status))
	{
		DebugPrint(ERROR, DBG_INIT, "BthInitRecv(%x)\n", Status);
		goto fail;
	}

	//reg_dump_txdesc(pAd); // muted debug output
	//reg_dump_rxdesc(pAd); // here too

DebugPrint(TRACE, DBG_MISC, "<--%s()\n", __FUNCTION__);

	return 0;

fail:
	rtbt_pci_resource_deinit(os_ctrl);
	return -1;
}


