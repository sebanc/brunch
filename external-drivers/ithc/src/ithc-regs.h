/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

#define CONTROL_QUIESCE                     BIT(1)
#define CONTROL_IS_QUIESCED                 BIT(2)
#define CONTROL_NRESET                      BIT(3)
#define CONTROL_READY                       BIT(29)

#define SPI_CONFIG_MODE(x)                  (((x) & 3) << 2)
#define SPI_CONFIG_SPEED(x)                 (((x) & 7) << 4)
#define SPI_CONFIG_UNKNOWN_18(x)            (((x) & 3) << 18)
#define SPI_CONFIG_SPEED2(x)                (((x) & 0xf) << 20) // high bit = high speed mode?

#define ERROR_CONTROL_UNKNOWN_0             BIT(0)
#define ERROR_CONTROL_DISABLE_DMA           BIT(1) // clears DMA_RX_CONTROL_ENABLE when a DMA error occurs
#define ERROR_CONTROL_UNKNOWN_2             BIT(2)
#define ERROR_CONTROL_UNKNOWN_3             BIT(3)
#define ERROR_CONTROL_IRQ_DMA_UNKNOWN_9     BIT(9)
#define ERROR_CONTROL_IRQ_DMA_UNKNOWN_10    BIT(10)
#define ERROR_CONTROL_IRQ_DMA_UNKNOWN_12    BIT(12)
#define ERROR_CONTROL_IRQ_DMA_UNKNOWN_13    BIT(13)
#define ERROR_CONTROL_UNKNOWN_16(x)         (((x) & 0xff) << 16) // spi error code irq?
#define ERROR_CONTROL_SET_DMA_STATUS        BIT(29) // sets DMA_RX_STATUS_ERROR when a DMA error occurs

#define ERROR_STATUS_DMA                    BIT(28)
#define ERROR_STATUS_SPI                    BIT(30)

#define ERROR_FLAG_DMA_UNKNOWN_9            BIT(9)
#define ERROR_FLAG_DMA_UNKNOWN_10           BIT(10)
#define ERROR_FLAG_DMA_UNKNOWN_12           BIT(12) // set when we receive a truncated DMA message
#define ERROR_FLAG_DMA_UNKNOWN_13           BIT(13)
#define ERROR_FLAG_SPI_BUS_TURNAROUND       BIT(16)
#define ERROR_FLAG_SPI_RESPONSE_TIMEOUT     BIT(17)
#define ERROR_FLAG_SPI_INTRA_PACKET_TIMEOUT BIT(18)
#define ERROR_FLAG_SPI_INVALID_RESPONSE     BIT(19)
#define ERROR_FLAG_SPI_HS_RX_TIMEOUT        BIT(20)
#define ERROR_FLAG_SPI_TOUCH_IC_INIT        BIT(21)

#define SPI_CMD_CONTROL_SEND                BIT(0) // cleared by device when sending is complete
#define SPI_CMD_CONTROL_IRQ                 BIT(1)

#define SPI_CMD_CODE_READ                   4
#define SPI_CMD_CODE_WRITE                  6

#define SPI_CMD_STATUS_DONE                 BIT(0)
#define SPI_CMD_STATUS_ERROR                BIT(1)
#define SPI_CMD_STATUS_BUSY                 BIT(3)

#define DMA_TX_CONTROL_SEND                 BIT(0) // cleared by device when sending is complete
#define DMA_TX_CONTROL_IRQ                  BIT(3)

#define DMA_TX_STATUS_DONE                  BIT(0)
#define DMA_TX_STATUS_ERROR                 BIT(1)
#define DMA_TX_STATUS_UNKNOWN_2             BIT(2)
#define DMA_TX_STATUS_UNKNOWN_3             BIT(3) // busy?

#define DMA_RX_CONTROL_ENABLE               BIT(0)
#define DMA_RX_CONTROL_IRQ_UNKNOWN_1        BIT(1) // rx1 only?
#define DMA_RX_CONTROL_IRQ_ERROR            BIT(3) // rx1 only?
#define DMA_RX_CONTROL_IRQ_UNKNOWN_4        BIT(4) // rx0 only?
#define DMA_RX_CONTROL_IRQ_DATA             BIT(5)

#define DMA_RX_CONTROL2_UNKNOWN_5           BIT(5) // rx0 only?
#define DMA_RX_CONTROL2_RESET               BIT(7) // resets ringbuffer indices

#define DMA_RX_WRAP_FLAG                    BIT(7)

#define DMA_RX_STATUS_ERROR                 BIT(3)
#define DMA_RX_STATUS_UNKNOWN_4             BIT(4) // set in rx0 after using CONTROL_NRESET when it becomes possible to read config (can take >100ms)
#define DMA_RX_STATUS_HAVE_DATA             BIT(5)
#define DMA_RX_STATUS_ENABLED               BIT(8)

// COUNTER_RESET can be written to counter registers to reset them to zero. However, in some cases this can mess up the THC.
#define COUNTER_RESET                       BIT(31)

struct ithc_registers {
	/* 0000 */ u32 _unknown_0000[1024];
	/* 1000 */ u32 _unknown_1000;
	/* 1004 */ u32 _unknown_1004;
	/* 1008 */ u32 control_bits;
	/* 100c */ u32 _unknown_100c;
	/* 1010 */ u32 spi_config;
	/* 1014 */ u32 _unknown_1014[3];
	/* 1020 */ u32 error_control;
	/* 1024 */ u32 error_status; // write to clear
	/* 1028 */ u32 error_flags; // write to clear
	/* 102c */ u32 _unknown_102c[5];
	struct {
		/* 1040 */ u8 control;
		/* 1041 */ u8 code;
		/* 1042 */ u16 size;
		/* 1044 */ u32 status; // write to clear
		/* 1048 */ u32 offset;
		/* 104c */ u32 data[16];
		/* 108c */ u32 _unknown_108c;
	} spi_cmd;
	struct {
		/* 1090 */ u64 addr; // cannot be written with writeq(), must use lo_hi_writeq()
		/* 1098 */ u8 control;
		/* 1099 */ u8 _unknown_1099;
		/* 109a */ u8 _unknown_109a;
		/* 109b */ u8 num_prds;
		/* 109c */ u32 status; // write to clear
	} dma_tx;
	/* 10a0 */ u32 _unknown_10a0[7];
	/* 10bc */ u32 state; // is 0xe0000402 (dev config val 0) after CONTROL_NRESET, 0xe0000461 after first touch, 0xe0000401 after DMA_RX_CODE_RESET
	/* 10c0 */ u32 _unknown_10c0[8];
	/* 10e0 */ u32 _unknown_10e0_counters[3];
	/* 10ec */ u32 _unknown_10ec[5];
	struct {
		/* 1100/1200 */ u64 addr; // cannot be written with writeq(), must use lo_hi_writeq()
		/* 1108/1208 */ u8 num_bufs;
		/* 1109/1209 */ u8 num_prds;
		/* 110a/120a */ u16 _unknown_110a;
		/* 110c/120c */ u8 control;
		/* 110d/120d */ u8 head;
		/* 110e/120e */ u8 tail;
		/* 110f/120f */ u8 control2;
		/* 1110/1210 */ u32 status; // write to clear
		/* 1114/1214 */ u32 _unknown_1114;
		/* 1118/1218 */ u64 _unknown_1118_guc_addr;
		/* 1120/1220 */ u32 _unknown_1120_guc;
		/* 1124/1224 */ u32 _unknown_1124_guc;
		/* 1128/1228 */ u32 unknown_init_bits; // bit 2 = guc related, bit 3 = rx1 related, bit 4 = guc related
		/* 112c/122c */ u32 _unknown_112c;
		/* 1130/1230 */ u64 _unknown_1130_guc_addr;
		/* 1138/1238 */ u32 _unknown_1138_guc;
		/* 113c/123c */ u32 _unknown_113c;
		/* 1140/1240 */ u32 _unknown_1140_guc;
		/* 1144/1244 */ u32 _unknown_1144[23];
		/* 11a0/12a0 */ u32 _unknown_11a0_counters[6];
		/* 11b8/12b8 */ u32 _unknown_11b8[18];
	} dma_rx[2];
};
static_assert(sizeof(struct ithc_registers) == 0x1300);

#define DEVCFG_DMA_RX_SIZE(x)          ((((x) & 0x3fff) + 1) << 6)
#define DEVCFG_DMA_TX_SIZE(x)          (((((x) >> 14) & 0x3ff) + 1) << 6)

#define DEVCFG_TOUCH_MASK              0x3f
#define DEVCFG_TOUCH_ENABLE            BIT(0)
#define DEVCFG_TOUCH_UNKNOWN_1         BIT(1)
#define DEVCFG_TOUCH_UNKNOWN_2         BIT(2)
#define DEVCFG_TOUCH_UNKNOWN_3         BIT(3)
#define DEVCFG_TOUCH_UNKNOWN_4         BIT(4)
#define DEVCFG_TOUCH_UNKNOWN_5         BIT(5)
#define DEVCFG_TOUCH_UNKNOWN_6         BIT(6)

#define DEVCFG_DEVICE_ID_TIC           0x43495424 // "$TIC"

#define DEVCFG_SPI_MAX_FREQ(x)         (((x) >> 1) & 0xf) // high bit = use high speed mode?
#define DEVCFG_SPI_MODE(x)             (((x) >> 6) & 3)
#define DEVCFG_SPI_UNKNOWN_8(x)        (((x) >> 8) & 0x3f)
#define DEVCFG_SPI_NEEDS_HEARTBEAT     BIT(20) // TODO implement heartbeat
#define DEVCFG_SPI_HEARTBEAT_INTERVAL(x) (((x) >> 21) & 7)
#define DEVCFG_SPI_UNKNOWN_25          BIT(25)
#define DEVCFG_SPI_UNKNOWN_26          BIT(26)
#define DEVCFG_SPI_UNKNOWN_27          BIT(27)
#define DEVCFG_SPI_DELAY(x)            (((x) >> 28) & 7) // TODO use this
#define DEVCFG_SPI_USE_EXT_READ_CFG    BIT(31) // TODO use this?

struct ithc_device_config { // (Example values are from an SP7+.)
	u32 _unknown_00;      // 00 = 0xe0000402 (0xe0000401 after DMA_RX_CODE_RESET)
	u32 _unknown_04;      // 04 = 0x00000000
	u32 dma_buf_sizes;    // 08 = 0x000a00ff
	u32 touch_cfg;        // 0c = 0x0000001c
	u32 _unknown_10;      // 10 = 0x0000001c
	u32 device_id;        // 14 = 0x43495424 = "$TIC"
	u32 spi_config;       // 18 = 0xfda00a2e
	u16 vendor_id;        // 1c = 0x045e = Microsoft Corp.
	u16 product_id;       // 1e = 0x0c1a
	u32 revision;         // 20 = 0x00000001
	u32 fw_version;       // 24 = 0x05008a8b = 5.0.138.139 (this value looks more random on newer devices)
	u32 _unknown_28;      // 28 = 0x00000000
	u32 fw_mode;          // 2c = 0x00000000 (for fw update?)
	u32 _unknown_30;      // 30 = 0x00000000
	u32 _unknown_34;      // 34 = 0x0404035e (u8,u8,u8,u8 = version?)
	u32 _unknown_38;      // 38 = 0x000001c0 (0x000001c1 after DMA_RX_CODE_RESET)
	u32 _unknown_3c;      // 3c = 0x00000002
};

void bitsl(__iomem u32 *reg, u32 mask, u32 val);
void bitsb(__iomem u8 *reg, u8 mask, u8 val);
#define bitsl_set(reg, x) bitsl(reg, x, x)
#define bitsb_set(reg, x) bitsb(reg, x, x)
int waitl(struct ithc *ithc, __iomem u32 *reg, u32 mask, u32 val);
int waitb(struct ithc *ithc, __iomem u8 *reg, u8 mask, u8 val);
int ithc_set_spi_config(struct ithc *ithc, u8 speed, u8 mode);
int ithc_spi_command(struct ithc *ithc, u8 command, u32 offset, u32 size, void *data);

