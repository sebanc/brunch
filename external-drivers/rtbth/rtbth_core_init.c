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

#include "rtbt_hal.h"
#include "hps_bluez.h"
#include "rtbth_us.h"

#define PKT_HV3_MAX_DATA_LEN 30UL

extern RTBTH_ADAPTER *gpAd;
static struct rtbt_hps_ops rtbt_3298_hps_ops;

int BthInitializeAdapter(IN RTBTH_ADAPTER *pAd)
{
	DebugPrint(TRACE, DBG_INIT, "--> BthInitializeAdapter\n");

	// Initialze ASIC
	BthInitializeAsic(pAd);

    _rtbth_us_event_notification(pAd, INIT_COREINIT_EVENT);

	DebugPrint(TRACE, DBG_INIT, "<-- BthInitializeAdapter\n");

	return STATUS_SUCCESS;
}

UINT32 RT3298_REG_ADDR[]= {
	0x0,
	0x4,
	0x8,
	0xc,
	0x10,
	0x14,
	0x18,
	0x1c,
	0x20,
	0x180,
	0x184,
	0x188,
	0x18c,
	0x200,
	0x204,
	0x208,
	0x20c,
	0x210,
	0x220,
	0x228,
	0x300,
	0x304,
	0x308,
	0x30c,
	0x320,
	0x324,
	0x328,
	0x32c,
	0x330,
	0x334,
	0x338,
	0x340,
	0x344,
	0x348,
	0x350,
	0x354,
	0x358,
	0x35c,
	0x360,
	0x380,
	0x384,
	0x3c0,
	0x3c4,
	0x400,
	0x404,
	0x408,
	0x40c,
	0x410,
	0x414,
	0x418,
	0x41c,
	0x420,
	0x424,
	0x428,
	0x42c,
	0x434,
	0x440,
	0x444,
	0x448,
	0x600,
	0x604,
	0x608,
	0x60c,
	0x610
	};

VOID eFusePhysicalReadRegisters(
	IN	PRTBTH_ADAPTER	pAd,
	IN	UINT16 Offset,
	IN	UINT16 Length,
	OUT	UINT16 *pData)
{
	EFUSE_CTRL_STRUC eFuseCtrlStruc;
	int i;
	UINT16 efuseDataOffset;
	ULONG data;

	RT_IO_READ32(pAd, EFUSE_CTRL, (UINT32 *) &eFuseCtrlStruc.word);

	//Step0. Write 10-bit of address to EFSROM_AIN (0x324, bit25:bit16). The address must be 16-byte alignment.
	eFuseCtrlStruc.field.EFSROM_AIN = Offset & 0xfff0;

	//Step1. Write EFSROM_MODE (0x324, bit7:bit6) to 1.
	eFuseCtrlStruc.field.EFSROM_MODE = 1;

	//Step2. Write EFSROM_KICK (0x324, bit30) to 1 to kick-off physical read procedure.
	eFuseCtrlStruc.field.EFSROM_KICK = 1;

	RtlCopyMemory(&data, &eFuseCtrlStruc.word, 4);
	RT_IO_WRITE32(pAd, EFUSE_CTRL, data);

	//Step3. Polling EFSROM_KICK(0x324, bit30) until it become 0 again.
	i = 0;

	//TODO
	while ((i < 100) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		RT_IO_READ32(pAd, EFUSE_CTRL, (UINT32 *) &eFuseCtrlStruc.word);
		if(eFuseCtrlStruc.field.EFSROM_KICK == 0)
			break;
		rtbt_usec_delay(100);
		i++;
	}

	//Step4. Read 16-byte of data from EFUSE_DATA0-3 (0x59C-0x590)
	//efuseDataOffset =  EFUSE_DATA3 - (Offset & 0xC)  ;
	efuseDataOffset =  EFUSE_DATA0 + (Offset & 0xC);

	RT_IO_READ32(pAd, efuseDataOffset, &data);

	data = data >> (8*(Offset & 0x3));

	RtlCopyMemory(pData, &data, Length);

}

UCHAR eFuseReadRegisters(
	IN RTBTH_ADAPTER *pAd,
	IN UINT16 Offset,
	IN UINT16 Length,
	OUT UINT16 *pData)
{
	EFUSE_CTRL_STRUC eFuseCtrlStruc;
	int i;
	UINT16 efuseDataOffset;
	ULONG data;

	RT_IO_READ32(pAd, EFUSE_CTRL, (UINT32 *) &eFuseCtrlStruc.word);

	//Step0. Write 10-bit of address to EFSROM_AIN (0x324, bit25:bit16). The address must be 16-byte alignment.
	//       The address must be 16-byte alignment. (This is to say, the last 4 bits must be 0)
	eFuseCtrlStruc.field.EFSROM_AIN = Offset & 0xfff0;

	//Step1. Write EFSROM_MODE (0x324, bit7:bit6) to 0.
	eFuseCtrlStruc.field.EFSROM_MODE = 0;

	//Step2. Write EFSROM_KICK (0x324, bit30) to 1 to kick-off physical read procedure.
	eFuseCtrlStruc.field.EFSROM_KICK = 1;

	RtlCopyMemory(&data, &eFuseCtrlStruc.word, 4);
	RT_IO_WRITE32(pAd, EFUSE_CTRL, data);

	//Step3. Polling EFSROM_KICK(0x324, bit30) until it become 0 again.
	i = 0;

	//TODO: NIC card existence checking
	while ((i < 100) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		RT_IO_READ32(pAd, EFUSE_CTRL, (UINT32 *) &eFuseCtrlStruc.word);
		if(eFuseCtrlStruc.field.EFSROM_KICK == 0)
		{
			break;
		}
		rtbt_usec_delay(100);
		i++;
	}

	//if EFSROM_AOUT is not found in physical address, write 0xffff
	if (eFuseCtrlStruc.field.EFSROM_AOUT == 0x3f)
	{
		for(i=0; i<Length/2; i++)
			*(pData+2*i) = 0xffff;
	}
	else
	{
		//Step4. Read 16-byte of data from EFUSE_DATA0-3 (0x590-0x59C)
		//efuseDataOffset =  EFUSE_DATA3 - (Offset & 0xC)  ;
		efuseDataOffset =  EFUSE_DATA0 + (Offset & 0xC)  ;
		RT_IO_READ32(pAd, efuseDataOffset, &data);

		data = data >> (8*(Offset & 0x3));
		RtlCopyMemory(pData, &data, Length);
	}

	return (UCHAR) eFuseCtrlStruc.field.EFSROM_AOUT;
}

VOID eFuseReadPhysical(
	IN RTBTH_ADAPTER *pAd,
  	IN UINT16 *lpInBuffer,
  	IN ULONG nInBufferSize,
  	OUT UINT16 *lpOutBuffer,
  	IN ULONG nOutBufferSize)
{
	UINT16 *pInBuf = (UINT16 *)lpInBuffer;
	UINT16 *pOutBuf = (UINT16 *)lpOutBuffer;

	UINT16 Offset = pInBuf[0];					//addr
	UINT16 Length = pInBuf[1];					//length
	UINT16 i;

	for(i=0; i<Length; i+=2)
	{
		eFusePhysicalReadRegisters(pAd,Offset+i, 2, &pOutBuf[i/2]);
	}
}

NTSTATUS eFuseRead(
	IN RTBTH_ADAPTER *pAd,
	IN UINT16 Offset,
	OUT	UINT8 *pData,
	IN UINT16 Length)
{
	USHORT* 	pOutBuf = (USHORT*)pData;
	NTSTATUS	Status = STATUS_SUCCESS;
	UCHAR		EFSROM_AOUT;
	//int	i;
	USHORT	i;

	for(i=0; i<Length; i+=2)
	{
		EFSROM_AOUT = eFuseReadRegisters(pAd, Offset+i, 2, &pOutBuf[i/2]);
	}
	return Status;
}

VOID eFusePhysicalWriteRegisters(
	IN RTBTH_ADAPTER *pAd,
	IN USHORT Offset,
	IN USHORT Length,
	OUT USHORT *pData)
{
	EFUSE_CTRL_STRUC		eFuseCtrlStruc;
	int i;
	USHORT efuseDataOffset;
	ULONG data, eFuseDataBuffer[4];


	DebugPrint(TRACE, DBG_HW_ACCESS,
				"eFusePhysicalWriteRegisters() Offset=0x%X, pData=0x%X\n", Offset, *pData);

	bt_memset(eFuseDataBuffer, 0, sizeof(ULONG)*4);


	if (RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
		return;

	//Step0. Write 16-byte of data to EFUSE_DATA0-3 (0x590-0x59C), where EFUSE_DATA0 is the LSB DW, EFUSE_DATA3 is the MSB DW.

	/////////////////////////////////////////////////////////////////
	//read current values of 16-byte block
	RT_IO_READ32(pAd, EFUSE_CTRL, (PULONG) &eFuseCtrlStruc.word);

	//Step0. Write 10-bit of address to EFSROM_AIN (0x324, bit25:bit16). The address must be 16-byte alignment.
	eFuseCtrlStruc.field.EFSROM_AIN = Offset & 0xfff0;

	//Step1. Write EFSROM_MODE (0x324, bit7:bit6) to 1.
	eFuseCtrlStruc.field.EFSROM_MODE = 1;

	//Step2. Write EFSROM_KICK (0x324, bit30) to 1 to kick-off physical read procedure.
	eFuseCtrlStruc.field.EFSROM_KICK = 1;

	RtlCopyMemory(&data, &eFuseCtrlStruc.word, 4);
	RT_IO_WRITE32(pAd, EFUSE_CTRL, data);

	//Step3. Polling EFSROM_KICK(0x324, bit30) until it become 0 again.
	i = 0;

	while ((i < 100) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		RT_IO_READ32(pAd, EFUSE_CTRL, (PULONG) &eFuseCtrlStruc.word);

		if(eFuseCtrlStruc.field.EFSROM_KICK == 0)
			break;
		rtbt_usec_delay(100);
		i++;
	}

	//Step4. Read 16-byte of data from EFUSE_DATA0-3 (0x59C-0x590)
	//efuseDataOffset =  EFUSE_DATA3;
	efuseDataOffset =  EFUSE_DATA0;
	for(i=0; i< 4; i++)
	{
		RT_IO_READ32(pAd, efuseDataOffset, (PULONG) &eFuseDataBuffer[i]);
		//efuseDataOffset -=  4;
		efuseDataOffset +=  4;
	}

	//Update the value, the offset is multiple of 2, length is 2
	efuseDataOffset = (Offset & 0xc) >> 2;
	data = pData[0] & 0xffff;
	if((Offset % 4) != 0)
	{
		eFuseDataBuffer[efuseDataOffset] = (eFuseDataBuffer[efuseDataOffset] & 0xffff) | (data << 16);
	}
	else
	{
		eFuseDataBuffer[efuseDataOffset] = (eFuseDataBuffer[efuseDataOffset] & 0xffff0000) | data;
	}

	//efuseDataOffset =  EFUSE_DATA3;
	efuseDataOffset =  EFUSE_DATA0;
	for(i=0; i< 4; i++)
	{
		RT_IO_WRITE32(pAd, efuseDataOffset, eFuseDataBuffer[i]);
		//efuseDataOffset -= 4;
		efuseDataOffset += 4;
	}
	/////////////////////////////////////////////////////////////////

	//Step1. Write 10-bit of address to EFSROM_AIN (0x324, bit25:bit16). The address must be 16-byte alignment.
	eFuseCtrlStruc.field.EFSROM_AIN = Offset & 0xfff0;

	//Step2. Write EFSROM_MODE (0x324, bit7:bit6) to 3.
	eFuseCtrlStruc.field.EFSROM_MODE = 3;

	//Step3. Write EFSROM_KICK (0x324, bit30) to 1 to kick-off physical write procedure.
	eFuseCtrlStruc.field.EFSROM_KICK = 1;

	RtlCopyMemory(&data, &eFuseCtrlStruc.word, 4);
	RT_IO_WRITE32(pAd, EFUSE_CTRL, data);

	//Step4. Polling EFSROM_KICK(0x324, bit30) until it become 0 again. It’s done.
	i = 0;

	while ((i < 100) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		RT_IO_READ32(pAd, EFUSE_CTRL, (UINT32 *) &eFuseCtrlStruc.word);

		if(eFuseCtrlStruc.field.EFSROM_KICK == 0)
			break;

		rtbt_usec_delay(100);
		i++;
	}
}

VOID eFuseWritePhysical(
	IN RTBTH_ADAPTER *pAd,
  	IN USHORT *lpInBuffer,
	IN ULONG nInBufferSize,
  	OUT UCHAR *lpOutBuffer,
  	OUT ULONG nOutBufferSize)
{
	USHORT* pInBuf = (USHORT*)lpInBuffer;
	//int 		i;
	USHORT 		i;
	//USHORT* pOutBuf = (USHORT*)ioBuffer;

	USHORT Offset = pInBuf[0];					//addr
	USHORT Length = pInBuf[1];					//length
	USHORT* pValueX = &pInBuf[2];				//value ...

	for(i=0; i<Length; i+=2)
	{
		eFusePhysicalWriteRegisters(pAd, Offset+i, 2, &pValueX[i/2]);
	}
}

NTSTATUS eFuseWriteRegisters(
	IN	PRTBTH_ADAPTER	pAd,
	IN	USHORT 			Offset,
	IN	USHORT 			Length,
	IN	USHORT* 		pData)
{
	UINT16	i;
	UINT16	eFuseData;
	UINT16	LogicalAddress, BlkNum = 0xffff;
	UINT8	EFSROM_AOUT;
	//BOOLEAN	bUpdateBlock = FALSE;
	UINT16 	addr,tmpaddr, InBuf[3], tmpOffset;
	UINT16 	buffer[8];
	BOOLEAN	bWriteSuccess = TRUE;
	ULONG	LoopCount = 0;


	DebugPrint(TRACE, DBG_HW_ACCESS, "eFuseWriteRegisters Offset=0x%X, Length=%d, pData=0x%X\n", Offset, Length,*pData);

	//Step 0. find the entry in the mapping table
	tmpOffset = Offset & 0xfffe;
	EFSROM_AOUT = eFuseReadRegisters(pAd, tmpOffset, 2, &eFuseData);
	if( EFSROM_AOUT == 0x3f)
	{
		//the logical address does not exist, find an empty one
		for (i=EFUSE_USAGE_MAP_START; i<=EFUSE_USAGE_MAP_END; i+=2)
		{
			eFusePhysicalReadRegisters(pAd, i, 2, &LogicalAddress);
			DebugPrint(TRACE, DBG_HW_ACCESS, "eFusePhysicalReadRegisters() read pool addr=0x%04X, value=0x%X\n", i, LogicalAddress);

			/* Choose Not Used Row Index */
			if( (LogicalAddress & 0xff) == 0)
			{
				BlkNum = i-EFUSE_USAGE_MAP_START;
				break;
			}
			else if(( (LogicalAddress >> 8) & 0xff) == 0)
			{
				if (i != EFUSE_USAGE_MAP_END)
				{
					BlkNum = i+1-EFUSE_USAGE_MAP_START;
				}
				break;
			}
		}
	}
	else
	{
		BlkNum = EFSROM_AOUT;
	}

	DebugPrint(TRACE, DBG_HW_ACCESS, "eFuseWriteRegisters BlkNum = 0x%02X, EFSROM_AOUT= 0x%02X \n", BlkNum, EFSROM_AOUT);

	if(BlkNum == 0xffff)
	{
		DebugPrint(ERROR, DBG_HW_ACCESS, "eFuseWriteRegisters: out of free E-fuse space!!!\n");
		return STATUS_UNSUCCESSFUL;
	}

	//Step 1. Save data of this block
	// read and save the original block data (16 bytes)
	for(i =0; i<8; i++)
	{
		addr = BlkNum * 0x10 ;

		InBuf[0] = addr+2*i;
		InBuf[1] = 2;
		InBuf[2] = 0x0;

		eFuseReadPhysical(pAd, &InBuf[0], 4, &InBuf[2], 2);

		buffer[i] = InBuf[2];
	}

	//Step 2. Update the data in buffer, and write the data to Efuse
	//Need new block but no free eFuse space
	if ((buffer[ (Offset >> 1) % 8] | pData[0]) != pData[0])
	{
		//
		// This means we need to find a new block for saving this value
		// Check if we can find a new one, EFUSE_USAGE_MAP_END is the lastest block point to Block #29 (EFUSE_USAGE_MAP_START ~ EFUSE_USAGE_MAP_END)
		//
		eFusePhysicalReadRegisters(pAd, EFUSE_USAGE_MAP_END, 2, &LogicalAddress);
		if( (LogicalAddress & 0xff) != 0)
		{
			// There is no empty one for update EFuse
			return STATUS_UNSUCCESSFUL;
		}
	}

	//Update to Target word (two bytes)
	buffer[ (Offset >> 1) % 8] = pData[0];

	do
	{
		//Step 3. Write the data to Efuse

		DebugPrint(TRACE, DBG_HW_ACCESS, "Write the data to Efuse:\n");
		if(!bWriteSuccess)
		{
			for(i =0; i<8; i++)
			{
				addr = BlkNum * 0x10 ;

				InBuf[0] = addr+2*i;
				InBuf[1] = 2;
				InBuf[2] = buffer[i];

				eFuseWritePhysical(pAd, &InBuf[0], 6, NULL, 2);
			}
		}
		else
		{
				DebugPrint(TRACE, DBG_HW_ACCESS, "!!!Check it, Write Remainder bytes at address 0x%X, pData=0x%X \n", InBuf[0], InBuf[2]);
				addr = BlkNum * 0x10 ;

				InBuf[0] = addr+(Offset % 16);
				InBuf[1] = 2;
				InBuf[2] = pData[0];

				eFuseWritePhysical(pAd, &InBuf[0], 6, NULL, 2);
		}

		//Step 4. Write mapping table
		addr = EFUSE_USAGE_MAP_START+BlkNum;

		tmpaddr = addr;

		if(addr % 2 != 0)
			addr = addr -1;
		InBuf[0] = addr;
		InBuf[1] = 2;

		//convert the address from 10 to 8 bit ( bit7, 6 = parity and bit5 ~ 0 = bit9~4), and write to logical map entry
		tmpOffset = Offset;
		/* Convert to Row address */
		tmpOffset >>= 4;
		tmpOffset |= ((~((tmpOffset & 0x01) ^ ( tmpOffset >> 1 & 0x01) ^  (tmpOffset >> 2 & 0x01) ^  (tmpOffset >> 3 & 0x01))) << 6) & 0x40;
		tmpOffset |= ((~( (tmpOffset >> 2 & 0x01) ^ (tmpOffset >> 3 & 0x01) ^ (tmpOffset >> 4 & 0x01) ^ ( tmpOffset >> 5 & 0x01))) << 7) & 0x80;

		// write the logical address
		if(tmpaddr%2 != 0)
			InBuf[2] = tmpOffset<<8;
		else
			InBuf[2] = tmpOffset;

		DebugPrint(TRACE, DBG_HW_ACCESS, "Write mapping table:\n");
		eFuseWritePhysical(pAd, &InBuf[0], 6, NULL, 0);

		//Step 5. Compare data if not the same, invalidate the mapping entry, then re-write the data until E-fuse is exhausted
		bWriteSuccess = TRUE;
		for(i =0; i<8; i++)
		{
			addr = BlkNum * 0x10 ;

			InBuf[0] = addr+2*i;
			InBuf[1] = 2;
			InBuf[2] = 0x0;

			eFuseReadPhysical(pAd, &InBuf[0], 4, &InBuf[2], 2);

			if(buffer[i] != InBuf[2])
			{
				bWriteSuccess = FALSE;
				break;
			}
		}

		//Step 6. invlidate mapping entry and find a free mapping entry if not succeed
		if (!bWriteSuccess)
		{
			DebugPrint(TRACE, DBG_HW_ACCESS, "Not bWriteSuccess BlkNum = %d\n", BlkNum);

			// the offset of current mapping entry
			addr = EFUSE_USAGE_MAP_START+BlkNum;

			//find a new mapping entry
			BlkNum = 0xffff;
			for (i=EFUSE_USAGE_MAP_START; i<=EFUSE_USAGE_MAP_END; i+=2)
			{
				eFusePhysicalReadRegisters(pAd, i, 2, &LogicalAddress);
				if( (LogicalAddress & 0xff) == 0)
				{
					BlkNum = i-EFUSE_USAGE_MAP_START;
					break;
				}
				else if(( (LogicalAddress >> 8) & 0xff) == 0)
				{
					if (i != EFUSE_USAGE_MAP_END)
					{
						BlkNum = i+1-EFUSE_USAGE_MAP_START;
					}
					break;
				}
			}

			DebugPrint(TRACE, DBG_HW_ACCESS, "Not bWriteSuccess new BlkNum = %d\n", BlkNum);


			if(BlkNum == 0xffff)
			{
				DebugPrint(TRACE, DBG_HW_ACCESS, "eFuseWriteRegisters: out of free E-fuse space!!!\n");

				return STATUS_UNSUCCESSFUL;
			}

			//invalidate the original mapping entry if new entry is not found
			tmpaddr = addr;

			if(addr % 2 != 0)
				addr = addr -1;
			InBuf[0] = addr;
			InBuf[1] = 2;

			eFuseReadPhysical(pAd, &InBuf[0], 4, &InBuf[2], 2);

			// write the logical address
			if(tmpaddr%2 != 0)
			{
				// Invalidate the high byte
				for (i=8; i<15; i++)
				{
					if( ( (InBuf[2] >> i) & 0x01) == 0)
					{
						InBuf[2] |= (0x1 <<i);
						break;
					}
				}
			}
			else
			{
				// invalidate the low byte
				for (i=0; i<8; i++)
				{
					if( ( (InBuf[2] >> i) & 0x01) == 0)
					{
						InBuf[2] |= (0x1 <<i);
						break;
					}
				}
			}
			eFuseWritePhysical(pAd, &InBuf[0], 6, NULL, 0);
		}
		LoopCount++;
	} while ((!bWriteSuccess) && (LoopCount < 200) && (!RT_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));


	return STATUS_SUCCESS;
}

NTSTATUS eFuseWrite(
   	IN RTBTH_ADAPTER *pAd,
	IN USHORT Offset,
	IN UCHAR *pData,
	IN USHORT length)
{
	//int i;
	USHORT	i;
	NTSTATUS	Status = STATUS_SUCCESS;

	USHORT *pValueX = (USHORT *) pData;				//value ...

	for(i=0; i<length; i+=2)
	{
		eFuseWriteRegisters(pAd, Offset+i, 2, &pValueX[i/2]);
	}

	return Status;
}

VOID RaiseClock(
    IN  PRTBTH_ADAPTER  pAd,
    IN  ULONG *x)
{
    *x = *x | EESK;
    RT_IO_WRITE32(pAd, E2PROM_CSR, *x);
    rtbt_usec_delay(1);               // Max frequency = 1MHz in Spec. definition
}

VOID LowerClock(
    IN  PRTBTH_ADAPTER  pAd,
    IN  ULONG *x)
{
    *x = *x & ~EESK;
    RT_IO_WRITE32(pAd, E2PROM_CSR, *x);
    rtbt_usec_delay(1);
}

USHORT ShiftInBits(
    IN  PRTBTH_ADAPTER  pAd)
{
    ULONG       x,i;
    USHORT      data=0;

    RT_IO_READ32(pAd, E2PROM_CSR, &x);

    x &= ~( EEDO | EEDI);

    for(i=0; i<16; i++)
    {
        data = data << 1;
        RaiseClock(pAd, &x);

        RT_IO_READ32(pAd, E2PROM_CSR, &x);
        LowerClock(pAd, &x);

        x &= ~(EEDI);
        if(x & EEDO)
            data |= 1;
    }

    return data;
}

VOID ShiftOutBits(
    IN  PRTBTH_ADAPTER  pAd,
    IN  USHORT data,
    IN  USHORT count)
{
    ULONG       x,mask;

    mask = 0x01 << (count - 1);
    RT_IO_READ32(pAd, E2PROM_CSR, &x);

    x &= ~(EEDO | EEDI);

    do
    {
        x &= ~EEDI;
        if(data & mask)
			x |= EEDI;

        RT_IO_WRITE32(pAd, E2PROM_CSR, x);

        RaiseClock(pAd, &x);
        LowerClock(pAd, &x);

        mask = mask >> 1;
    } while(mask);

    x &= ~EEDI;
    RT_IO_WRITE32(pAd, E2PROM_CSR, x);
}

VOID EEpromCleanup(
    IN  PRTBTH_ADAPTER  pAd)
{
    ULONG x;

    RT_IO_READ32(pAd, E2PROM_CSR, &x);

    x &= ~(EECS | EEDI);
    RT_IO_WRITE32(pAd, E2PROM_CSR, x);

    RaiseClock(pAd, &x);
    LowerClock(pAd, &x);
}

VOID EWEN(
    IN  PRTBTH_ADAPTER  pAd)
{
    ULONG   x;

    // reset bits and set EECS
    RT_IO_READ32(pAd, E2PROM_CSR, &x);
    x &= ~(EEDI | EEDO | EESK);
    x |= EECS;
    RT_IO_WRITE32(pAd, E2PROM_CSR, x);

    // kick a pulse
    RaiseClock(pAd, &x);
    LowerClock(pAd, &x);

    // output the read_opcode and six pulse in that order
    ShiftOutBits(pAd, EEPROM_EWEN_OPCODE, 5);
    ShiftOutBits(pAd, 0, 6);

    EEpromCleanup(pAd);
}


VOID EWDS(
    IN  PRTBTH_ADAPTER  pAd)
{
    ULONG   x;

    // reset bits and set EECS
    RT_IO_READ32(pAd, E2PROM_CSR, &x);
    x &= ~(EEDI | EEDO | EESK);
    x |= EECS;
    RT_IO_WRITE32(pAd, E2PROM_CSR, x);

    // kick a pulse
    RaiseClock(pAd, &x);
    LowerClock(pAd, &x);

    // output the read_opcode and six pulse in that order
    ShiftOutBits(pAd, EEPROM_EWDS_OPCODE, 5);
    ShiftOutBits(pAd, 0, 6);

    EEpromCleanup(pAd);
}


int rtbt_prom_read16(
    IN  PRTBTH_ADAPTER  pAd,
    IN  USHORT Offset,
    OUT USHORT *pData)
{
    ULONG       x;


    Offset /= 2;
    // reset bits and set EECS
    RT_IO_READ32(pAd, E2PROM_CSR, &x);
    x &= ~(EEDI | EEDO | EESK);
    x |= EECS;
    RT_IO_WRITE32(pAd, E2PROM_CSR, x);

    // kick a pulse
    RaiseClock(pAd, &x);
    LowerClock(pAd, &x);

    // output the read_opcode and register number in that order
    ShiftOutBits(pAd, EEPROM_READ_OPCODE, 3);
    ShiftOutBits(pAd, Offset, EEPROM_ADDRESS_NUM);

    // Now read the data (16 bits) in from the selected EEPROM word
    *pData = ShiftInBits(pAd);

    EEpromCleanup(pAd);

    return 0;
}


int rtbt_prom_write16(
    IN  PRTBTH_ADAPTER  pAd,
    IN  USHORT Offset,
    IN  USHORT Data)
{
    ULONG   x;

    Offset /= 2;

    EWEN(pAd);

    // reset bits and set EECS
    RT_IO_READ32(pAd, E2PROM_CSR, &x);
    x &= ~(EEDI | EEDO | EESK);
    x |= EECS;
    RT_IO_WRITE32(pAd, E2PROM_CSR, x);

    // kick a pulse
    RaiseClock(pAd, &x);
    LowerClock(pAd, &x);

    // output the read_opcode ,register number and data in that order
    ShiftOutBits(pAd, EEPROM_WRITE_OPCODE, 3);
    ShiftOutBits(pAd, Offset, EEPROM_ADDRESS_NUM);
    ShiftOutBits(pAd, Data, 16);        // 16-bit access

    // read DO status
    RT_IO_READ32(pAd, E2PROM_CSR, &x);

    EEpromCleanup(pAd);

    rtbt_usec_delay(10000);   //delay for twp(MAX)=10ms

    EWDS(pAd);

    EEpromCleanup(pAd);

	return 0;
}

USHORT Bth_EEPROM_READ16(
    IN RTBTH_ADAPTER *pAd,
    IN USHORT Offset)
{
	ULONG value = 0;
	USHORT data;
	LONG count=0;

	RT_IO_READ32(pAd, BT_FUNC_INFO, &value);
	while((value & 0x80000000) && (count <=100))
	{
		count++;
		rtbt_usec_delay(100000);
		RT_IO_READ32(pAd, BT_FUNC_INFO, &value);
		if(value == 0xffffffff)
		{
			DebugPrint(TRACE, DBG_HW_ACCESS,
						"Bth_EEPROM_READ16: Card maybe not exist or bt_func not enable  bt_fun= %x\n",pAd->btFunCtrl.word);
			break;
		}
	}

	RT_IO_READ32(pAd, WLAN_FUN_INFO, &value);
	RT_IO_WRITE32(pAd, WLAN_FUN_INFO, value | 0x80000000);

	if (pAd->bUseEfuse)
	{
		eFuseRead(pAd, Offset, (UINT8 *)&data, 2);
	}
	else
	{
		rtbt_prom_read16(pAd, Offset, &data);
	}

	value &= (~0x80000000);
	RT_IO_WRITE32(pAd, WLAN_FUN_INFO, value);

	return data;
}

VOID Bth_EEPROM_WRITE16(
    IN  PRTBTH_ADAPTER  pAd,
    IN  USHORT Offset,
    IN  USHORT Data)
{
    ULONG value = 0;
	LONG count=0;

	RT_IO_READ32(pAd, BT_FUNC_INFO, &value);
	while((value & 0x80000000) && (count<=100))
	{
		count++;
		rtbt_usec_delay(100000);
		RT_IO_READ32(pAd, BT_FUNC_INFO, &value);
		if(value == 0xffffffff)
		{
			DebugPrint(TRACE, DBG_HW_ACCESS,
				"%s():Card maybe not exist or bt_func not enable  bt_fun= %x\n",
				__FUNCTION__, pAd->btFunCtrl.word);
			break;
		}
	}

	RT_IO_READ32(pAd, WLAN_FUN_INFO, &value);
	RT_IO_WRITE32(pAd, WLAN_FUN_INFO, value | 0x80000000);

	if(pAd->bUseEfuse)
	{
		eFuseWrite(pAd, Offset, (PUCHAR) &Data, 2);
	}
	else
	{
		rtbt_prom_write16(pAd, Offset, Data);
	}

	value &= (~0x80000000);
	RT_IO_WRITE32(pAd, WLAN_FUN_INFO, value);
}

VOID dump_mac_reg(IN RTBTH_ADAPTER *pAd)
{
	UINT32 mac_val;
//	UINT32 mac_reg; //sean wang linux: warning: unused variable ‘mac_reg’
	int cnt;

	DebugPrint(TRACE, DBG_MISC, "Dump Mac Registers:\n");
	for (cnt = 0; cnt < sizeof(RT3298_REG_ADDR)/sizeof(UINT32);)
	{
		RT_IO_FORCE_READ32(pAd, RT3298_REG_ADDR[cnt], &mac_val);
		DebugPrint(TRACE, DBG_MISC, "0x%x=0x%x\t", RT3298_REG_ADDR[cnt], mac_val);
		if (((++cnt) % 4) == 0)
			DebugPrint(TRACE, DBG_MISC, "\n");
	}
	DebugPrint(TRACE, DBG_MISC, "\n-------\n");
}

void BthInitializePrerequire(IN RTBTH_ADAPTER *pAd)
{
	ULONG	Index, Value;
	USHORT	usValue;
	UCHAR	ucValue;
	EFUSE_CTRL_STRUC	eFuseCtrl;
	OSCCTL_STRUC osCtrl;

	DebugPrint(TRACE, DBG_INIT, " -->%s()\n", __FUNCTION__);

DebugPrint(TRACE, DBG_INIT, " Dump MAC registers before call BthEnableBtFunc\n");
dump_mac_reg(pAd);

	// read BT_FUN_CTRL as default value
	RT_IO_FORCE_READ32(pAd, BT_FUN_CTRL, &pAd->btFunCtrl.word);
DebugPrint(TRACE, DBG_INIT, "btFunCtrl.work = 0x08%x\n", pAd->btFunCtrl.word);
	BthEnableBtFunc(pAd);

DebugPrint(TRACE, DBG_INIT, " Dump MAC registers After call BthEnableBtFunc\n");
dump_mac_reg(pAd);

	RT_IO_READ32(pAd, BT_FUN_CTRL, &Value);
	Value |= 0x8;
	RT_IO_WRITE32(pAd, BT_FUN_CTRL, Value);
	rtbt_usec_delay(10);
	Value &= ~0x8;
	RT_IO_WRITE32(pAd, BT_FUN_CTRL, Value);

	//Enable ROSC_EN first then CAL_REQ
	RT_IO_READ32(pAd, OSCCTL, &osCtrl.word);
	osCtrl.field.ROSC_EN = TRUE; //HW force
	RT_IO_WRITE32(pAd, OSCCTL, osCtrl.word);

	osCtrl.field.ROSC_EN = TRUE; //HW force
	osCtrl.field.CAL_REQ = TRUE;
	osCtrl.field.REF_CYCLE = 0x27;
	RT_IO_WRITE32(pAd, OSCCTL, osCtrl.word);

	RT_IO_READ32(pAd, BT_FUN_CTRL, &pAd->btFunCtrl.word);

	/* Initialize bUseEfuse before any eeprom r/w */
	eFuseCtrl.word = 0;
	RT_IO_READ32(pAd, EFUSE_CTRL, &eFuseCtrl.word);
	pAd->bUseEfuse = (BOOLEAN)eFuseCtrl.field.SEL_EFUSE;
	//TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "Use %s\n", (pAd->bUseEfuse == INTERNAL_EFFUSE_PROM)?"EFUSE":"EEPROM");
	DebugPrint(TRACE, DBG_INIT, "Use %s\n", (pAd->bUseEfuse == INTERNAL_EFFUSE_PROM) ? "EFUSE" : "EEPROM");

	/* Make sure MAC gets ready. */
	Index = 0;
	do
	{
		RT_IO_READ32(pAd, ASIC_VER_CSR, &pAd->MACVersion);
		if ((pAd->MACVersion != 0x00) && (pAd->MACVersion != 0xFFFFFFFF))
			break;
		KeStallExecutionProcessor(5);
	} while (Index++ < 10);
	DebugPrint(TRACE, DBG_INIT, "PCI ASIC Ver [0x%08x]\n", pAd->MACVersion);

	RT_IO_READ32(pAd, REV_CODE, &Value);
 	DebugPrint(TRACE, DBG_INIT, "BT REV_CODE [0x%08x]\n", Value);

#if 1
// EEPROM
	usValue = Bth_EEPROM_READ16(pAd, EEPROM_VERSION);
    if (usValue != 0xffff)
    {
        ucValue = usValue & 0x00ff;
		pAd->EEPROMVersion = ucValue;
    }
	else
	{
		pAd->EEPROMVersion = 0;
		DebugPrint(ERROR, DBG_INIT, "EEPROM Version: read failed\n");
	}
	DebugPrint(TRACE, DBG_INIT, "EEPROM Version: %d\n", pAd->EEPROMVersion);

	usValue = Bth_EEPROM_READ16(pAd, EEPROM_RF_VERSION);
    if (usValue != 0xffff)
    {
        ucValue = usValue & 0x00ff;
		if(ucValue == RFIC_3290)
		{
			DebugPrint(TRACE, DBG_INIT, "EEPROM RFVersion: 3290\n");
	        	pAd->RFVersion = RFIC_3290;
		}
		else if(ucValue == RFIC_TC6004)
		{
			DebugPrint(TRACE, DBG_INIT, "EEPROM RFVersion: TC6004\n");
	        	pAd->RFVersion = RFIC_TC6004;
		}
		else
		{
			DebugPrint(ERROR, DBG_INIT, "RFVersion: unknown(0x%x)\n", ucValue);
		}
    }
	else
	{
		DebugPrint(TRACE, DBG_INIT, "RFVersion: TC2001\n");
		pAd->RFVersion = RFIC_TC2001;
	}

	usValue = (Bth_EEPROM_READ16(pAd, EEPROM_HW_RADIO_SUPPORT) & 0xff00) >> 8;
	if((usValue & 0x1) && (usValue != 0xff))
	{
		pAd->bHwRadioOnSupport = TRUE;
		DebugPrint(TRACE, DBG_INIT, " hw radio on/ff support !\n");
	}
	else
	{
		//init default value
		pAd->bHwRadio = TRUE;
	}
#endif
	pAd->bRadio = TRUE;
	Rtbth_Set_Radio_Led(pAd, TRUE);

	// Disable DMA.
	BthDmaCfg(pAd, 0);

	// asic simulation sequence put this ahead before loading firmware.
	// pbf hardware reset
	RT_IO_WRITE32(pAd, PDMA_RST_CSR, 0x33fffff);

	RT_IO_WRITE32(pAd, SYS_CTRL, 0x10e1f);	// Max: Keep MCU reset state
	RT_IO_WRITE32(pAd, SYS_CTRL, 0x10e00);	// Max: Keep MCU reset state

	RtbtResetPDMA(pAd);

  	DebugPrint(TRACE, DBG_INIT, " BthDisableInterrupt\n");
	BthDisableInterrupt(pAd);

	DebugPrint(TRACE, DBG_INIT, " <--%s()\n", __FUNCTION__);
 }

int BthInitialize(
	IN RTBTH_ADAPTER *pAd)
{
	int status;

	/* Register the ISR first */
	//
	// Disable interrupts here which is as soon as possible
	//
	RT_IO_FORCE_WRITE32(pAd, INT_MASK_CSR, 0);
	RtmpOSIRQRequest(pAd->os_ctrl->if_dev, pAd->os_ctrl->bt_dev);

	BthInitializePrerequire(pAd);

	RT_SET_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);

	status = BthInitializeAdapter(pAd);
	if (!NT_SUCCESS(status))
	{
		DebugPrint(ERROR, DBG_INIT, "BthInitializeAdapter failed: 0x%x\n", status);
		return status;
	}

    _rtbth_us_event_notification(pAd, INIT_CORESTART_EVENT);

DebugPrint(TRACE, DBG_INIT, "%s():status=%d\n", __FUNCTION__, status);
	return status;
}

void BthEnableRxTx(IN RTBTH_ADAPTER *pAd)
{
	PTXD_STRUC  	pTxD;
	PRXD_STRUC		pRxD;
	//LARGE_INTEGER delay;
	UCHAR   		i, j;
	UINT8			TxRingSize;

	DebugPrint(TRACE, DBG_INIT, "==> BthEnableRxTx\n");

	BthDmaCfg(pAd, 1);
	for (i = 0; i < NUM_OF_TX_RING; i++)
	{
		TxRingSize = BthGetTxRingSize(pAd, i);
		//for (j = 0; j < TX_RING_SIZE; j++)
		for (j = 0; j < TxRingSize; j++)
		{
			pTxD = (PTXD_STRUC) pAd->TxRing[i].Cell[j].AllocVa;
			// set DmaDone=0
			pTxD->DmaDone = 0;
		}
	}

	for (i = 0; i < NUM_OF_RX_RING; i++)
	{
		for (j = 0; j < RX_RING_SIZE; j++)
		{
			pRxD = (PRXD_STRUC) pAd->RxRing[i].Cell[j].AllocVa;
			// set DmaDone=0
			pRxD->DDONE = 0;
		}
	}


	DebugPrint(TRACE, DBG_INIT, "<== BthEnableRxTx\n");

}

VOID
Rtbth_Set_Radio_Led(
	IN PRTBTH_ADAPTER	pAd,
	IN BOOLEAN			Enable)
{
	ULONG value	= 0;

	DebugPrint(TRACE, DBG_INIT,"Rtbth_Set_Radio_Led %s(%d)\n", Enable ? "ON":"OFF", Enable);

	if (Enable == TRUE)
	{
		//
		// Set BT Led On, GPIO1 bit0
		//
		pAd->btFunCtrl.word &= ~0x01000000; /* GPIO1 output enable: Output 0, input 1 	*/
		pAd->btFunCtrl.word |=  0x00010000;	/* GPIO1 output data */
		RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, pAd->btFunCtrl.word);

		RT_IO_FORCE_READ32(pAd, BT_FUN_CTRL, &value);
		DebugPrint(TRACE, DBG_INIT,"Rtbth_Set_Radio_Led ON readback 0x%08X\n", value);
	}
	else /* Set BT Led OFF */
	{
		pAd->btFunCtrl.word &= ~0x00010000;
		RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, pAd->btFunCtrl.word);
		RT_IO_FORCE_READ32(pAd, BT_FUN_CTRL, &value);
		DebugPrint(TRACE, DBG_INIT,"Rtbth_Set_Radio_Led Off readback 0x%08X\n", value);
	}
}

VOID BthRadioOff(
	IN	PRTBTH_ADAPTER pAd)
{
	ULONG   	MacValue;

	//ask by max ,don't touch app_clk_req at B version IC
	// per Max's request, leave bit[2](BT_RF_EN) on, to fix Ch 13 issue
	pAd->btFunCtrl.word &= ~(0x1);
	//
	// Reset the whole bluetooth(PMDA, LC, MCU, ...) except RF
	//
	RT_IO_FORCE_READ32(pAd, BT_FUN_CTRL, &MacValue);
        MacValue &= ~(0x1);
	RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, MacValue);

	Rtbth_Set_Radio_Led(pAd, FALSE);
	DebugPrint(TRACE, DBG_INIT, "<== BthRadioOff\n");
}

VOID BthRadioOn(
	IN	PRTBTH_ADAPTER pAd
)
{
	ULONG   	MacValue = pAd->btFunCtrl.word;
	ULONG   	i = 0;

	DebugPrint(TRACE, DBG_INIT, "==> BthRadioOn\n");

	if (MacValue & 0x01)
		return;
	else
	{
		MacValue |= 0x5;
		MacValue &= (~0x2);
		RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, MacValue);
		pAd->btFunCtrl.word |=0x5;
		pAd->btFunCtrl.word &= (~0x2);
	}

	do
	{
		// Check PLL stable.
		i++;
		MacValue = 0;
		RT_IO_READ32(pAd, CMB_CTRL, &MacValue);

		if (i > 1000)
			break;

		if ((MacValue&0xc00000) != 0)
			break;

	} while(TRUE);

	pAd->btFunCtrl.word |=0x7;
	RT_IO_FORCE_WRITE32(pAd, BT_FUN_CTRL, pAd->btFunCtrl.word );
	//
	// Led On
	//
	Rtbth_Set_Radio_Led(pAd, TRUE);

	DebugPrint(TRACE, DBG_INIT, "<== BthRadioOn %d\n", i);
}

int rtbt_dev_hw_init(void *dev_ctrl)
{
	RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)dev_ctrl;
	struct rtbt_os_ctrl *os_ctrl;

	os_ctrl = pAd->os_ctrl;

	return 0;
}

int rtbt_dev_hw_deinit(void *dev_ctrl)
{
//	RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)dev_ctrl;

	return 0;
}

int rtbt_dev_resource_init(struct rtbt_os_ctrl *os_ctrl)
{
	return rtbt_pci_resource_init(os_ctrl);
}

int rtbt_dev_resource_deinit(struct rtbt_os_ctrl *os_ctrl)
{
	return rtbt_pci_resource_deinit(os_ctrl);
}

int rtbt_dev_ctrl_deinit(struct rtbt_os_ctrl *os_ctrl)
{
	RTBTH_ADAPTER *pAd;;

	ASSERT(os_ctrl);

	pAd = (RTBTH_ADAPTER *)(os_ctrl->dev_ctrl);
	if (!pAd) {
		DebugPrint(FATAL, DBG_MISC, "%s():Fatal Error, os_ctrl=0x%x\n", __FUNCTION__, os_ctrl);
		return -1;
	}

	/* de-initialize list heads, spinlocks, timers etc. */
	ral_spin_deinit(&pAd->SendLock);
	ral_spin_deinit(&pAd->RcvLock);

	ral_spin_deinit(&pAd->LcCmdQueueSpinLock);
	ral_spin_deinit(&pAd->LcEventQueueSpinLock);
	ral_spin_deinit(&pAd->QueueSpinLock);
    ral_spin_deinit(&pAd->ChannelMapSpinLock);

	ral_spin_deinit(&pAd->MiscSpinLock);
	ral_spin_deinit(&pAd->scoSpinLock);

	KeDestoryEvent(&pAd->CoreEventTriggered);
	KeDestoryEvent(&pAd->HCIEventTriggered);
	KeDestoryEvent(&pAd->HCIACLDataEventTriggered);
	KeDestoryEvent(&pAd->HCISCODataEventTriggered);

	KeFreeTimer(&pAd->RadioStateTimer);
	KeFreeTimer(&pAd->core_idle_timer);

	if (pAd->os_ctrl)
		ral_mem_vfree(pAd->os_ctrl);

	return 0;
}

int rtbt_dev_ctrl_init(
	IN struct rtbt_os_ctrl **pDevCtrl,
	IN void *csr)
{
	struct rtbt_os_ctrl *os_ctrl;
	//struct rtbt_dev_ctrl *pAd;//sean wang linux
	struct _RTBTH_ADAPTER *pAd;	//sean wang linux
	char *mem;
	int size;
	int spin_status = 0, timer_status = 0;
	NDIS_STATUS event_status = STATUS_SUCCESS;

//	size = sizeof(struct rtbt_dev_ctrl) + sizeof(struct rtbt_os_ctrl); //sean wang linux
//_RTBTH_ADAPTER
	size = sizeof(struct _RTBTH_ADAPTER) + sizeof(struct rtbt_os_ctrl);	//sean wang linux

	mem = ral_mem_valloc(size);
	if (mem == NULL)
	{
		DebugPrint(FATAL, DBG_INIT, "Allocate devCtrl failed!\n");
		return -1;
	}
	RtlZeroMemory(mem, size);

	os_ctrl = (struct rtbt_os_ctrl *)mem;
	//pAd = (struct rtbt_dev_ctrl *)(mem + sizeof(struct rtbt_os_ctrl));
	pAd = (struct _RTBTH_ADAPTER *)(mem + sizeof(struct rtbt_os_ctrl));

	DebugPrint(TRACE, DBG_INIT,
				"Alloc os_ctrl@0x%lx(%d), dev_ctrl@0x%lx(%d), len=%d\n",
				(ULONG)os_ctrl, sizeof(struct rtbt_os_ctrl),
				(ULONG)pAd, sizeof(struct _RTBTH_ADAPTER), size);//sean wang linux

	os_ctrl->dev_ctrl = (void *)pAd;
	os_ctrl->if_ops.pci_ops.isr = rtbt_pci_isr;
	os_ctrl->if_ops.pci_ops.csr_addr = csr;
	os_ctrl->hps_ops = &rtbt_3298_hps_ops;

	pAd->CSRAddress = csr;
	pAd->os_ctrl = os_ctrl;

#ifdef DBG
	DebugPrint(TRACE, DBG_INIT, "os_ctrl.if_ops.pci_ops.isr=0x%lx(0x%lx), "
				"csr_addr=0x%lx(0x%lx)\n",
				(ULONG)os_ctrl->if_ops.pci_ops.isr, (ULONG)rtbt_pci_isr,
				(ULONG)os_ctrl->if_ops.pci_ops.csr_addr, (ULONG)csr);
#endif

	/* Initialize list heads, spinlocks, timers etc. */
	spin_status |= ral_spin_init(&pAd->SendLock);
	spin_status |= ral_spin_init(&pAd->RcvLock);
    spin_status |= ral_spin_init(&pAd->scoSpinLock);

	RtlCopyMemory(&pAd->infName[0], "RalinkBT", 8);

	if (spin_status | event_status | timer_status) {
		DebugPrint(ERROR, DBG_INIT, "%s():Init failed(spin=%d, event=%d, timer=%d)\n",
					__FUNCTION__, spin_status, event_status, timer_status);
		rtbt_dev_ctrl_deinit(os_ctrl);
		return -1;
	}

	*pDevCtrl = os_ctrl;

	return 0;
}

static struct ral_dev_id rtbt_3298_dev_ids[]={
	{0x1814, 0x3298},
	{0,0}
};

static struct rtbt_dev_ops rtbt_3298_dev_ops = {
	.dev_ctrl_init = rtbt_dev_ctrl_init,
	.dev_ctrl_deinit = rtbt_dev_ctrl_deinit,
	.dev_resource_init = rtbt_dev_resource_init,  /* rtbt_pci_resource_init */
	.dev_resource_deinit = rtbt_dev_resource_deinit, /* rtbt_pci_resource_deinit */
	.dev_hw_init = rtbt_dev_hw_init,
	.dev_hw_deinit = rtbt_dev_hw_deinit,
};

static struct rtbt_dev_entry rtbt_3298_dev_list={
	.infType = RAL_INF_PCI,
	.devType = RAL_DEV_BT,
	.pDevIDList = rtbt_3298_dev_ids,
	.dev_ops = &rtbt_3298_dev_ops,
/*
	.os_private = NULL,
	.mlen = sizeof(struct rtbt_dev_ctrl),
*/
};

struct rtbt_dev_entry *rtbt_3298_init(void)
{
	DebugPrint(TRACE, DBG_INIT, "--->%s()\n", __FUNCTION__);

	return &rtbt_3298_dev_list;
}

int rtbt_3298_exit(void)
{
	DebugPrint(TRACE, DBG_INIT, "--->%s()\n", __FUNCTION__);

	return 0;
}

VOID BthShutdown(
	IN PRTBTH_ADAPTER     pAd)
{
	DebugPrint(TRACE, DBG_MISC, "-->BthShutdown\n");
	BthDisableInterrupt(pAd);

    _rtbth_us_event_notification(pAd, INIT_COREDEINIT_EVENT);

    Rtbth_Set_Radio_Led(pAd, FALSE);
	BthDisableBtFunc(pAd);
	DebugPrint(TRACE, DBG_MISC, "<--BthShutdown \n");
}

INT rtbt_hps_suspend(void *pDevCtrl){

//    RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)pDevCtrl;
//    struct rtbt_os_ctrl *os_ctrl;

return 1;
#if 0
    os_ctrl = pAd->os_ctrl;

    //
    // detached the blue-z core.
    //

    printk("--->%s, before rtbt_hps_iface_detach\n", __FUNCTION__);
    //msleep(3000);
  //  rtbt_hps_iface_detach(os_ctrl);
    printk("--->%s, after rtbt_hps_iface_detach\n", __FUNCTION__);
    //
    // shutdown the RT3290
    //

    DebugPrint(TRACE, DBG_MISC, "PciDrvReturnResources\n");

    /* Reset and put the device into a known initial state. */
    RT_CLEAR_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);

    BthShutdown(pAd);

    _rtbth_us_event_notification(pAd, INIT_CORESTOP_EVENT);

    //ExDeleteNPagedLookasideList(&FdoData->HCIDataLookasideList);

    RtmpOSIRQRelease(os_ctrl->if_dev, os_ctrl->bt_dev);

    if (pAd->sco_event_buf)
        ral_mem_free(pAd->sco_event_buf);

    if (pAd->sco_data_buf)
        ral_mem_free(pAd->sco_data_buf);

    /* Disconnect from the interrupt and unmap any I/O ports */
    // TODO: Shiang, fix it!!
    #if 0
    BthUnmapHWResources(pAd);
    #endif
    DebugPrint(TRACE, DBG_INIT, "\n BthFreeDeviceResources ==>\n");

    return STATUS_SUCCESS;
#endif

}

INT rtbt_hps_resume(void *pDevCtrl){
 return 1;

 #if 0
    int status;
    RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)pDevCtrl;
    struct rtbt_os_ctrl *os_ctrl;

    os_ctrl = pAd->os_ctrl;

    //
    // power on the RT3290
    //
    DebugPrint(TRACE, DBG_INIT, "-->%s()\n", __FUNCTION__);
    pAd->sco_event_buf = ral_mem_alloc(128 * 4, RAL_MEM_NORMAL);

    pAd->sco_data_buf = ral_mem_alloc( PKT_HV3_MAX_DATA_LEN * 2 + 3, RAL_MEM_NORMAL);

    status = BthInitialize(pAd);
    if (!NT_SUCCESS (status)){
        DebugPrint(TRACE, DBG_INIT, "BthInitialize failed: 0x%x\n", status);
        return status;
    }

    // Clear garbage interrupt status.
    RT_IO_WRITE32(pAd, INT_SOURCE_CSR, 0xffffffff);

    DebugPrint(TRACE, DBG_INIT, "BthEnableInterrupt \n");

    //	DebugPrint(TRACE, DBG_THREAD, "Shiang: Here we stop the rtbt_hps_open!Remove it latter(status=%d)!\n", status);
    //	return status;
    BthEnableInterrupt(pAd);
    BthEnableRxTx(pAd);

    //
    // attached the blue-z core.
    //
    //rtbt_hps_iface_attach(os_ctrl);

    return status;
#endif

}

INT rtbt_hps_open(void *pDevCtrl)
{
	int status;
	RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)pDevCtrl;

	DebugPrint(TRACE, DBG_INIT, "-->%s()\n", __FUNCTION__);

    DebugPrint(TRACE, DBG_MISC, "kfifo reset ==>\n");
    kfifo_reset(pAd->acl_fifo);
    kfifo_reset(pAd->hci_fifo);
    kfifo_reset(pAd->evt_fifo);
    kfifo_reset(pAd->sco_fifo);
    kfifo_reset(pAd->rx_fifo);
    DebugPrint(TRACE, DBG_MISC, "kfifo reset <== \n");

	pAd->sco_event_buf = ral_mem_alloc(128 * 4, RAL_MEM_NORMAL);
	pAd->sco_data_buf = ral_mem_alloc( PKT_HV3_MAX_DATA_LEN * 2 + 3, RAL_MEM_NORMAL);

	status = BthInitialize(pAd);
	if (!NT_SUCCESS (status))
	{
		DebugPrint(TRACE, DBG_INIT, "BthInitialize failed: 0x%x\n", status);

		return status;
	}

	// Clear garbage interrupt status.
	RT_IO_WRITE32(pAd, INT_SOURCE_CSR, 0xffffffff);

	DebugPrint(TRACE, DBG_INIT, "BthEnableInterrupt \n");
//	DebugPrint(TRACE, DBG_THREAD, "Shiang: Here we stop the rtbt_hps_open!Remove it latter(status=%d)!\n", status);
//	return status;
	BthEnableInterrupt(pAd);
	BthEnableRxTx(pAd);

	return status;
}


INT rtbt_hps_close(void *pDevCtrl)
{
	RTBTH_ADAPTER *pAd = (RTBTH_ADAPTER *)pDevCtrl;
	struct rtbt_os_ctrl *os_ctrl;

	DebugPrint(TRACE, DBG_MISC, "PciDrvReturnResources\n");

	/* Reset and put the device into a known initial state. */
	RT_CLEAR_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE);

	BthShutdown(pAd);

    _rtbth_us_event_notification(pAd, INIT_CORESTOP_EVENT);

	os_ctrl = pAd->os_ctrl;
	RtmpOSIRQRelease(os_ctrl->if_dev, os_ctrl->bt_dev);

	if (pAd->sco_event_buf)
		ral_mem_free(pAd->sco_event_buf);

	if (pAd->sco_data_buf)
		ral_mem_free(pAd->sco_data_buf);
	/* Disconnect from the interrupt and unmap any I/O ports */

    DebugPrint(TRACE, DBG_MISC, "kfifo reset ==>\n");
    kfifo_reset(pAd->acl_fifo);
    kfifo_reset(pAd->hci_fifo);
    kfifo_reset(pAd->evt_fifo);
    kfifo_reset(pAd->sco_fifo);
    kfifo_reset(pAd->rx_fifo);
    DebugPrint(TRACE, DBG_MISC, "kfifo reset <== \n");


	DebugPrint(TRACE, DBG_INIT, "\n BthFreeDeviceResources ==>\n");

	return STATUS_SUCCESS;
}

int rtbth_rx_packet(void *pdata, RXBI_STRUC rxbi, void *buf, unsigned int len){

    unsigned char delimiter[] = {0xcc, 0xcc};
    unsigned int  sz_need = 2 +
                            sizeof(rxbi) +
                            sizeof(len)  +
                            len;
    int ret = 0;
    unsigned long cpuflags;


    DebugPrint(TRACE, DBG_INIT, "\n **sz_need for rx=%d, kfifo_avail(rx)=%d\n", sz_need, kfifo_avail(gpAd->rx_fifo));

    DebugPrint(TRACE, DBG_INIT,"#########Rx Data len = %d, rxbi.len=%d, rxbi.crc = %d, rxbi.ptt=%d \n", len, rxbi.Len, rxbi.Crc, rxbi.Ptt);

    spin_lock_irqsave(&gpAd->rx_fifo_lock, cpuflags);
    if(kfifo_avail(gpAd->rx_fifo) > sz_need) {
        kfifo_in(gpAd->rx_fifo, &delimiter[0] , sizeof(delimiter));
        kfifo_in(gpAd->rx_fifo, &rxbi, sizeof(rxbi));
        kfifo_in(gpAd->rx_fifo, &len, sizeof(len));
        kfifo_in(gpAd->rx_fifo, buf, len);
    } else {
        DebugPrint(ERROR, DBG_INIT, "\n room of rx fifo is not available\n");
        ret = -1;
    }
    DebugPrint(TRACE, DBG_INIT, "\n **after rx data in , kfifo_len (%d), sz_need(%d), sizeof(rxbi)=%d, sizeof(len)=%d \n",
        kfifo_len(gpAd->rx_fifo), sz_need,  sizeof(rxbi),  sizeof(len)  );

    spin_unlock_irqrestore(&gpAd->rx_fifo_lock, cpuflags);

    //notify
    _rtbth_us_event_notification(gpAd, INT_RX_EVENT);

    return ret;
}


int rtbth_bz_hci_send(void *pdata, void *buf, unsigned long len){

    //hci enqueue
    unsigned char delimiter[] = {0xcc, 0xcc};
    unsigned int  sz_need = 2 + 4 + len;
    int ret = 0;
    unsigned long cpuflags;

    spin_lock_irqsave(&gpAd->hci_fifo_lock,cpuflags);
    if(kfifo_avail(gpAd->hci_fifo) > sz_need) {
        kfifo_in(gpAd->hci_fifo, &delimiter[0] , sizeof(delimiter));
        kfifo_in(gpAd->hci_fifo, &len, sizeof(len));
        kfifo_in(gpAd->hci_fifo, buf, len);
    } else {
        DebugPrint(ERROR, DBG_INIT, "\n room of bz_hci fifo is not available\n");
        ret = -1;
    }

    DebugPrint(TRACE, DBG_INIT, "\n after hci data in , kfifo_len (%d) \n", kfifo_len(gpAd->hci_fifo));
    spin_unlock_irqrestore(&gpAd->hci_fifo_lock,cpuflags);

    //notify
    _rtbth_us_event_notification(gpAd, BZ_HCI_EVENT);


    return ret;
}

int rtbth_bz_acl_send(void *pdata, void *buf, unsigned long len){

    unsigned char delimiter[] = {0xcc, 0xcc};
    unsigned int  sz_need = 2 + 4 + len;
    int ret = 0;
    unsigned long cpuflags;

    spin_lock_irqsave(&gpAd->acl_fifo_lock, cpuflags);
    //acl enqueue
    if(kfifo_avail(gpAd->acl_fifo) > sz_need) {
        kfifo_in(gpAd->acl_fifo, &delimiter[0] , sizeof(delimiter));
        kfifo_in(gpAd->acl_fifo, &len, sizeof(len));
        kfifo_in(gpAd->acl_fifo, buf, len);
    } else {
        DebugPrint(ERROR, DBG_INIT, "\n room of bz_acl fifo is not available\n");
        ret = -1;
    }
    DebugPrint(TRACE, DBG_INIT, "\n after acl data in , kfifo_len (%d) \n", kfifo_len(gpAd->acl_fifo));
    spin_unlock_irqrestore(&gpAd->acl_fifo_lock,cpuflags);

    //notify
    _rtbth_us_event_notification(gpAd, BZ_ACL_EVENT);

    return ret;
}

int rtbth_bz_sco_send(void *pdata, void *buf, unsigned long len){

    unsigned char delimiter[] = {0xcc, 0xcc};
    unsigned int  sz_need = 2 + 4 + len;
    int ret = 0;
    unsigned long cpuflags;

    spin_lock_irqsave(&gpAd->sco_fifo_lock,cpuflags);
    //sco enqueue
    if(kfifo_avail(gpAd->sco_fifo) > sz_need) {
        kfifo_in(gpAd->sco_fifo, &delimiter[0] , sizeof(delimiter));
        kfifo_in(gpAd->sco_fifo, &len, sizeof(len));
        kfifo_in(gpAd->sco_fifo, buf, len);
    } else {
        DebugPrint(ERROR, DBG_INIT, "\n room of bz_sco fifo is not available\n");
        ret = -1;
    }
    DebugPrint(TRACE, DBG_INIT, "\n after sco data in , kfifo_len (%d) \n", kfifo_len(gpAd->sco_fifo));
    spin_unlock_irqrestore(&gpAd->sco_fifo_lock,cpuflags);

    //notify
    _rtbth_us_event_notification(gpAd, BZ_SCO_EVENT);

    return ret;
}

static struct rtbt_hps_ops rtbt_3298_hps_ops = {
    .open = rtbt_hps_open,
	.close = rtbt_hps_close,
	.suspend = rtbt_hps_suspend,
	.resume =  rtbt_hps_resume,
    .hci_cmd = rtbth_bz_hci_send,
	.hci_acl_data = rtbth_bz_acl_send,
	.hci_sco_data = rtbth_bz_sco_send,
};
