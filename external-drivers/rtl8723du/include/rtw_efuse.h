/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2007 - 2017 Realtek Corporation */

#ifndef __RTW_EFUSE_H__
#define __RTW_EFUSE_H__


#define	EFUSE_ERROE_HANDLE		1

#define	PG_STATE_HEADER		0x01
#define	PG_STATE_WORD_0		0x02
#define	PG_STATE_WORD_1		0x04
#define	PG_STATE_WORD_2		0x08
#define	PG_STATE_WORD_3		0x10
#define	PG_STATE_DATA			0x20

#define	PG_SWBYTE_H			0x01
#define	PG_SWBYTE_L			0x02

#define	PGPKT_DATA_SIZE		8

#define	EFUSE_WIFI				0
#define	EFUSE_BT				1

enum _EFUSE_DEF_TYPE {
	TYPE_EFUSE_MAX_SECTION				= 0,
	TYPE_EFUSE_REAL_CONTENT_LEN			= 1,
	TYPE_AVAILABLE_EFUSE_BYTES_BANK		= 2,
	TYPE_AVAILABLE_EFUSE_BYTES_TOTAL	= 3,
	TYPE_EFUSE_MAP_LEN					= 4,
	TYPE_EFUSE_PROTECT_BYTES_BANK		= 5,
	TYPE_EFUSE_CONTENT_LEN_BANK			= 6,
};

#define		EFUSE_MAX_MAP_LEN		1024

#define		EFUSE_MAX_HW_SIZE		1024
#define		EFUSE_MAX_SECTION_BASE	16
#define		EFUSE_MAX_SECTION_NUM	128
#define		EFUSE_MAX_BANK_SIZE		512

/*RTL8822B 8821C BT EFUSE Define 1 BANK 128 size logical map 1024*/ 

#define EXT_HEADER(header) ((header & 0x1F) == 0x0F)
#define ALL_WORDS_DISABLED(wde)	((wde & 0x0F) == 0x0F)
#define GET_HDR_OFFSET_2_0(header) ((header & 0xE0) >> 5)

#define		EFUSE_REPEAT_THRESHOLD_			3

#define IS_MASKED_MP(ic, txt, offset) (EFUSE_IsAddressMasked_MP_##ic##txt(offset))
#define IS_MASKED_TC(ic, txt, offset) (EFUSE_IsAddressMasked_TC_##ic##txt(offset))
#define GET_MASK_ARRAY_LEN_MP(ic, txt) (EFUSE_GetArrayLen_MP_##ic##txt())
#define GET_MASK_ARRAY_LEN_TC(ic, txt) (EFUSE_GetArrayLen_TC_##ic##txt())
#define GET_MASK_ARRAY_MP(ic, txt, offset) (EFUSE_GetMaskArray_MP_##ic##txt(offset))
#define GET_MASK_ARRAY_TC(ic, txt, offset) (EFUSE_GetMaskArray_TC_##ic##txt(offset))


#define IS_MASKED(ic, txt, offset) (IS_MASKED_MP(ic, txt, offset))
#define GET_MASK_ARRAY_LEN(ic, txt) (GET_MASK_ARRAY_LEN_MP(ic, txt))
#define GET_MASK_ARRAY(ic, txt, out) do { GET_MASK_ARRAY_MP(ic, txt, out); } while (0)

/* *********************************************
 *	The following is for BT Efuse definition
 * ********************************************* */
#define		EFUSE_BT_MAX_MAP_LEN		1024
#define		EFUSE_MAX_BANK			4
#define		EFUSE_MAX_BT_BANK		(EFUSE_MAX_BANK-1)
/* *********************************************
 *--------------------------Define Parameters-------------------------------*/
#define		EFUSE_MAX_WORD_UNIT			4

/*------------------------------Define structure----------------------------*/
struct pg_pkt_struct {
	u8 offset;
	u8 word_en;
	u8 data[8];
	u8 word_cnts;
};

enum error_code {
	ERR_SUCCESS = 0,
	ERR_DRIVER_FAILURE,
	ERR_IO_FAILURE,
	ERR_WI_TIMEOUT,
	ERR_WI_BUSY,
	ERR_BAD_FORMAT,
	ERR_INVALID_DATA,
	ERR_NOT_ENOUGH_SPACE,
	ERR_WRITE_PROTECT,
	ERR_READ_BACK_FAIL,
	ERR_OUT_OF_RANGE
};

/*------------------------------Define structure----------------------------*/
struct efuse_hal {
	u8	fakeEfuseBank;
	u32	fakeEfuseUsedBytes;
	u8	fakeEfuseContent[EFUSE_MAX_HW_SIZE];
	u8	fakeEfuseInitMap[EFUSE_MAX_MAP_LEN];
	u8	fakeEfuseModifiedMap[EFUSE_MAX_MAP_LEN];
	u32	EfuseUsedBytes;
	u8	EfuseUsedPercentage;

	u16	BTEfuseUsedBytes;
	u8	BTEfuseUsedPercentage;
	u8	BTEfuseContent[EFUSE_MAX_BT_BANK][EFUSE_MAX_HW_SIZE];
	u8	BTEfuseInitMap[EFUSE_BT_MAX_MAP_LEN];
	u8	BTEfuseModifiedMap[EFUSE_BT_MAX_MAP_LEN];

	u16	fakeBTEfuseUsedBytes;
	u8	fakeBTEfuseContent[EFUSE_MAX_BT_BANK][EFUSE_MAX_HW_SIZE];
	u8	fakeBTEfuseInitMap[EFUSE_BT_MAX_MAP_LEN];
	u8	fakeBTEfuseModifiedMap[EFUSE_BT_MAX_MAP_LEN];

	/* EFUSE Configuration, initialized in HAL_CmnInitPGData(). */
	const u16  MaxSecNum_WiFi;
	const u16  MaxSecNum_BT;
	const u16  WordUnit;
	const u16  PhysicalLen_WiFi;
	const u16  PhysicalLen_BT;
	const u16  LogicalLen_WiFi;
	const u16  LogicalLen_BT;
	const u16  BankSize;
	const u16  TotalBankNum;
	const u16  BankNum_WiFi;
	const u16  BankNum_BT;
	const u16  OOBProtectBytes;
	const u16  ProtectBytes;
	const u16  BankAvailBytes;
	const u16  TotalAvailBytes_WiFi;
	const u16  TotalAvailBytes_BT;
	const u16  HeaderRetry;
	const u16  DataRetry;

	enum error_code	  Status;

};

extern u8 maskfileBuffer[64];

/*------------------------Export global variable----------------------------*/
extern u8 fakeEfuseBank;
extern u32 fakeEfuseUsedBytes;
extern u8 fakeEfuseContent[];
extern u8 fakeEfuseInitMap[];
extern u8 fakeEfuseModifiedMap[];

extern u32 BTEfuseUsedBytes;
extern u8 BTEfuseContent[EFUSE_MAX_BT_BANK][EFUSE_MAX_HW_SIZE];
extern u8 BTEfuseInitMap[];
extern u8 BTEfuseModifiedMap[];

extern u32 fakeBTEfuseUsedBytes;
extern u8 fakeBTEfuseContent[EFUSE_MAX_BT_BANK][EFUSE_MAX_HW_SIZE];
extern u8 fakeBTEfuseInitMap[];
extern u8 fakeBTEfuseModifiedMap[];
/*------------------------Export global variable----------------------------*/
#define		MAX_SEGMENT_SIZE	200
#define		MAX_SEGMENT_NUM		200
#define		MAX_BUF_SIZE		(MAX_SEGMENT_SIZE*MAX_SEGMENT_NUM)
#define		TMP_BUF_SIZE		100

#define		rtprintf		dcmd_Store_Return_Buf

u8	efuse_bt_GetCurrentSize(struct adapter * adapt, u16 *size);
u16	efuse_bt_GetMaxSize(struct adapter * adapt);
u16 efuse_GetavailableSize(struct adapter * adapter);

u8	efuse_GetCurrentSize(struct adapter * adapt, u16 *size);
u16	efuse_GetMaxSize(struct adapter * adapt);
u8	rtw_efuse_access(struct adapter * adapt, u8 bRead, u16 start_addr, u16 cnts, u8 *data);
u8	rtw_efuse_bt_access(struct adapter * adapter, u8 write, u16 addr, u16 cnts, u8 *data);

u8	rtw_efuse_mask_map_read(struct adapter * adapt, u16 addr, u16 cnts, u8 *data);
u8	rtw_efuse_map_read(struct adapter * adapt, u16 addr, u16 cnts, u8 *data);
u8	rtw_efuse_map_write(struct adapter * adapt, u16 addr, u16 cnts, u8 *data);
u8	rtw_BT_efuse_map_read(struct adapter * adapt, u16 addr, u16 cnts, u8 *data);
u8	rtw_BT_efuse_map_write(struct adapter * adapt, u16 addr, u16 cnts, u8 *data);

u16	Efuse_GetCurrentSize(struct adapter * pAdapter, u8 efuseType, bool bPseudoTest);
u8	Efuse_CalculateWordCnts(u8 word_en);
void	ReadEFuseByte(struct adapter * Adapter, u16 _offset, u8 *pbuf, bool bPseudoTest) ;
void	EFUSE_GetEfuseDefinition(struct adapter * pAdapter, u8 efuseType, u8 type, void *pOut, bool bPseudoTest);
u8	efuse_OneByteRead(struct adapter * pAdapter, u16 addr, u8 *data, bool	 bPseudoTest);
#define efuse_onebyte_read(adapter, addr, data, pseudo_test) efuse_OneByteRead((adapter), (addr), (data), (pseudo_test))

u8	efuse_OneByteWrite(struct adapter * pAdapter, u16 addr, u8 data, bool	 bPseudoTest);

void	BTEfuse_PowerSwitch(struct adapter * pAdapter, u8	bWrite, u8	 PwrState);
void	Efuse_PowerSwitch(struct adapter * pAdapter, u8	bWrite, u8	 PwrState);
int	Efuse_PgPacketRead(struct adapter * pAdapter, u8 offset, u8 *data, bool bPseudoTest);
int	Efuse_PgPacketWrite(struct adapter * pAdapter, u8 offset, u8 word_en, u8 *data, bool bPseudoTest);
void	efuse_WordEnableDataRead(u8 word_en, u8 *sourdata, u8 *targetdata);
u8	Efuse_WordEnableDataWrite(struct adapter * pAdapter, u16 efuse_addr, u8 word_en, u8 *data, bool bPseudoTest);
void	EFUSE_ShadowMapUpdate(struct adapter * pAdapter, u8 efuseType, bool bPseudoTest);
void	EFUSE_ShadowRead(struct adapter * pAdapter, u8 Type, u16 Offset, u32 *Value);
#define efuse_logical_map_read(adapter, type, offset, value) EFUSE_ShadowRead((adapter), (type), (offset), (value))

void	hal_ReadEFuse_BT_logic_map(
	struct adapter *	adapt,
	u16			_offset,
	u16			_size_byte,
	u8			*pbuf
);
u8	EfusePgPacketWrite_BT(
	struct adapter *	pAdapter,
	u8			offset,
	u8			word_en,
	u8			*pData,
	u8			bPseudoTest);
u16 rtw_get_efuse_mask_arraylen(struct adapter * pAdapter);
void rtw_efuse_mask_array(struct adapter * pAdapter, u8 *pArray);
void rtw_efuse_analyze(struct adapter *	adapt, u8 Type, u8 Fake);

#define MAC_HIDDEN_MAX_BW_NUM 8
extern const u8 _mac_hidden_max_bw_to_hal_bw_cap[];
#define mac_hidden_max_bw_to_hal_bw_cap(max_bw) (((max_bw) >= MAC_HIDDEN_MAX_BW_NUM) ? 0 : _mac_hidden_max_bw_to_hal_bw_cap[(max_bw)])

#define MAC_HIDDEN_PROTOCOL_NUM 4
extern const u8 _mac_hidden_proto_to_hal_proto_cap[];
#define mac_hidden_proto_to_hal_proto_cap(proto) (((proto) >= MAC_HIDDEN_PROTOCOL_NUM) ? 0 : _mac_hidden_proto_to_hal_proto_cap[(proto)])

u8 mac_hidden_wl_func_to_hal_wl_func(u8 func);

u8 rtw_efuse_file_read(struct adapter * adapt, u8 *filepatch, u8 *buf, u32 len);
#ifdef CONFIG_EFUSE_CONFIG_FILE
u32 rtw_read_efuse_from_file(const char *path, u8 *buf, int map_size);
u32 rtw_read_macaddr_from_file(const char *path, u8 *buf);
#endif /* CONFIG_EFUSE_CONFIG_FILE */

#endif
