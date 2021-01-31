/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _RTL8814A_RXDESC_C_

//#include <drv_types.h>
#include <rtl8814a_hal.h>

void rtl8814_query_rx_desc_status(union recv_frame *precvframe, u8 *pdesc)
{
	struct rx_pkt_attrib	*pattrib = &precvframe->u.hdr.attrib;

	_rtw_memset(pattrib, 0, sizeof(struct rx_pkt_attrib));

#ifdef CONFIG_RADIOTAP_WITH_RXDESC
	_rtw_memcpy(pattrib->rxdesc, pdesc, RXDESC_SIZE);
#endif

	//Offset 0
	pattrib->pkt_len = (u16)GET_RX_STATUS_DESC_PKT_LEN_8814A(pdesc);//(le32_to_cpu(pdesc->rxdw0)&0x00003fff)
	pattrib->crc_err = (u8)GET_RX_STATUS_DESC_CRC32_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw0) >> 14) & 0x1);
	pattrib->icv_err = (u8)GET_RX_STATUS_DESC_ICV_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw0) >> 15) & 0x1);
	pattrib->drvinfo_sz = (u8)GET_RX_STATUS_DESC_DRVINFO_SIZE_8814A(pdesc) * 8;//((le32_to_cpu(pdesc->rxdw0) >> 16) & 0xf) * 8;//uint 2^3 = 8 bytes
	pattrib->encrypt = (u8)GET_RX_STATUS_DESC_SECURITY_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw0) >> 20) & 0x7);
	pattrib->qos = (u8)GET_RX_STATUS_DESC_QOS_8814A(pdesc);//(( le32_to_cpu( pdesc->rxdw0 ) >> 23) & 0x1);// Qos data, wireless lan header length is 26
	pattrib->shift_sz = (u8)GET_RX_STATUS_DESC_SHIFT_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw0) >> 24) & 0x3);
	pattrib->physt = (u8)GET_RX_STATUS_DESC_PHY_STATUS_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw0) >> 26) & 0x1);
	pattrib->bdecrypted = !GET_RX_STATUS_DESC_SWDEC_8814A(pdesc);//(le32_to_cpu(pdesc->rxdw0) & BIT(27))? 0:1;

	//Offset 4
	pattrib->priority = (u8)GET_RX_STATUS_DESC_TID_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw1) >> 8) & 0xf);
	pattrib->mdata = (u8)GET_RX_STATUS_DESC_MORE_DATA_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw1) >> 26) & 0x1);
	pattrib->mfrag = (u8)GET_RX_STATUS_DESC_MORE_FRAG_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw1) >> 27) & 0x1);//more fragment bit

	//Offset 8
	pattrib->seq_num = (u16)GET_RX_STATUS_DESC_SEQ_8814A(pdesc);//(le32_to_cpu(pdesc->rxdw2) & 0x00000fff);
	pattrib->frag_num = (u8)GET_RX_STATUS_DESC_FRAG_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw2) >> 12) & 0xf);//fragmentation number


	if (GET_RX_STATUS_DESC_RPT_SEL_8814A(pdesc))
		pattrib->pkt_rpt_type = C2H_PACKET;
	else
		pattrib->pkt_rpt_type = NORMAL_RX;

	//Offset 12
	pattrib->data_rate=(u8)GET_RX_STATUS_DESC_RX_RATE_8814A(pdesc);//((le32_to_cpu(pdesc->rxdw3))&0x7f);

	//Offset 16
	//Offset 20

}

