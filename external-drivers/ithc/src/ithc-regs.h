/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

#define LTR_CONFIG_ENABLE_ACTIVE            BIT(0)
#define LTR_CONFIG_TOGGLE                   BIT(1)
#define LTR_CONFIG_ENABLE_IDLE              BIT(2)
#define LTR_CONFIG_APPLY                    BIT(3)
#define LTR_CONFIG_IDLE_LTR_SCALE(x)        (((x) & 7) << 4)
#define LTR_CONFIG_IDLE_LTR_VALUE(x)        (((x) & 0x3ff) << 7)
#define LTR_CONFIG_ACTIVE_LTR_SCALE(x)      (((x) & 7) << 17)
#define LTR_CONFIG_ACTIVE_LTR_VALUE(x)      (((x) & 0x3ff) << 20)
#define LTR_CONFIG_STATUS_ACTIVE            BIT(30)
#define LTR_CONFIG_STATUS_IDLE              BIT(31)

#define CONTROL_QUIESCE                     BIT(1)
#define CONTROL_IS_QUIESCED                 BIT(2)
#define CONTROL_NRESET                      BIT(3)
#define CONTROL_UNKNOWN_24(x)               (((x) & 3) << 24)
#define CONTROL_READY                       BIT(29)

#define SPI_CONFIG_READ_MODE(x)             (((x) & 3) << 2)
#define SPI_CONFIG_READ_CLKDIV(x)           (((x) & 7) << 4)
#define SPI_CONFIG_READ_PACKET_SIZE(x)      (((x) & 0x1ff) << 7)
#define SPI_CONFIG_WRITE_MODE(x)            (((x) & 3) << 18)
#define SPI_CONFIG_WRITE_CLKDIV(x)          (((x) & 7) << 20)
#define SPI_CONFIG_CLKDIV_8                 BIT(23) // additionally divide clk by 8, for both read and write
#define SPI_CONFIG_WRITE_PACKET_SIZE(x)     (((x) & 0xff) << 24)

#define SPI_CLK_FREQ_BASE                   125000000
#define SPI_MODE_SINGLE                     0
#define SPI_MODE_DUAL                       1
#define SPI_MODE_QUAD                       2

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
#define ERROR_FLAG_DMA_RX_TIMEOUT           BIT(12) // set when we receive a truncated DMA message
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

#define INPUT_HEADER_VERSION(x)             ((x) & 0xf)
#define INPUT_HEADER_REPORT_LENGTH(x)       (((x) >> 8) & 0x3fff)
#define INPUT_HEADER_SYNC(x)                ((x) >> 24)
#define INPUT_HEADER_VERSION_VALUE          3
#define INPUT_HEADER_SYNC_VALUE             0x5a

#define QUICKSPI_CONFIG1_UNKNOWN_0(x)       (((x) & 0x1f) << 0)
#define QUICKSPI_CONFIG1_UNKNOWN_5(x)       (((x) & 0x1f) << 5)
#define QUICKSPI_CONFIG1_UNKNOWN_10(x)      (((x) & 0x1f) << 10)
#define QUICKSPI_CONFIG1_UNKNOWN_16(x)      (((x) & 0xffff) << 16)

#define QUICKSPI_CONFIG2_UNKNOWN_0(x)       (((x) & 0x1f) << 0)
#define QUICKSPI_CONFIG2_UNKNOWN_5(x)       (((x) & 0x1f) << 5)
#define QUICKSPI_CONFIG2_UNKNOWN_12(x)      (((x) & 0xf) << 12)
#define QUICKSPI_CONFIG2_UNKNOWN_16         BIT(16)
#define QUICKSPI_CONFIG2_UNKNOWN_17         BIT(17)
#define QUICKSPI_CONFIG2_DISABLE_READ_ADDRESS_INCREMENT  BIT(24)
#define QUICKSPI_CONFIG2_DISABLE_WRITE_ADDRESS_INCREMENT BIT(25)
#define QUICKSPI_CONFIG2_ENABLE_WRITE_STREAMING_MODE     BIT(27)
#define QUICKSPI_CONFIG2_IRQ_POLARITY       BIT(28)

#define DMA_RX_CONTROL_ENABLE               BIT(0)
#define DMA_RX_CONTROL_IRQ_UNKNOWN_1        BIT(1) // rx1 only?
#define DMA_RX_CONTROL_IRQ_ERROR            BIT(3) // rx1 only?
#define DMA_RX_CONTROL_IRQ_READY            BIT(4) // rx0 only
#define DMA_RX_CONTROL_IRQ_DATA             BIT(5)

#define DMA_RX_CONTROL2_UNKNOWN_4           BIT(4) // rx1 only?
#define DMA_RX_CONTROL2_UNKNOWN_5           BIT(5) // rx0 only?
#define DMA_RX_CONTROL2_RESET               BIT(7) // resets ringbuffer indices

#define DMA_RX_WRAP_FLAG                    BIT(7)

#define DMA_RX_STATUS_ERROR                 BIT(3)
#define DMA_RX_STATUS_READY                 BIT(4) // set in rx0 after using CONTROL_NRESET when it becomes possible to read config (can take >100ms)
#define DMA_RX_STATUS_HAVE_DATA             BIT(5)
#define DMA_RX_STATUS_ENABLED               BIT(8)

#define INIT_UNKNOWN_GUC_2                  BIT(2)
#define INIT_UNKNOWN_3                      BIT(3)
#define INIT_UNKNOWN_GUC_4                  BIT(4)
#define INIT_UNKNOWN_5                      BIT(5)
#define INIT_UNKNOWN_31                     BIT(31)

// COUNTER_RESET can be written to counter registers to reset them to zero. However, in some cases this can mess up the THC.
#define COUNTER_RESET                       BIT(31)

struct ithc_registers {
	/* 0000 */ u32 _unknown_0000[5];
	/* 0014 */ u32 ltr_config;
	/* 0018 */ u32 _unknown_0018[1018];
	/* 1000 */ u32 _unknown_1000;
	/* 1004 */ u32 _unknown_1004;
	/* 1008 */ u32 control_bits;
	/* 100c */ u32 _unknown_100c;
	/* 1010 */ u32 spi_config;
	struct {
		/* 1014/1018/101c */ u8 header;
		/* 1015/1019/101d */ u8 quad;
		/* 1016/101a/101e */ u8 dual;
		/* 1017/101b/101f */ u8 single;
	} opcode[3];
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
		/* 10a0 */ u32 _unknown_10a0[5];
		/* 10b4 */ u32 spi_addr;
	} dma_tx;
	/* 10b8 */ u32 spi_header_addr;
	union {
		/* 10bc */ u32 irq_cause; // in legacy THC mode
		/* 10bc */ u32 input_header; // in QuickSPI mode (see HIDSPI spec)
	};
	/* 10c0 */ u32 _unknown_10c0[8];
	/* 10e0 */ u32 _unknown_10e0_counters[3];
	/* 10ec */ u32 quickspi_config1;
	/* 10f0 */ u32 quickspi_config2;
	/* 10f4 */ u32 _unknown_10f4[3];
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
		/* 1128/1228 */ u32 init_unknown;
		/* 112c/122c */ u32 _unknown_112c;
		/* 1130/1230 */ u64 _unknown_1130_guc_addr;
		/* 1138/1238 */ u32 _unknown_1138_guc;
		/* 113c/123c */ u32 _unknown_113c;
		/* 1140/1240 */ u32 _unknown_1140_guc;
		/* 1144/1244 */ u32 _unknown_1144[11];
		/* 1170/1270 */ u32 spi_addr;
		/* 1174/1274 */ u32 _unknown_1174[11];
		/* 11a0/12a0 */ u32 _unknown_11a0_counters[6];
		/* 11b8/12b8 */ u32 _unknown_11b8[18];
	} dma_rx[2];
};
static_assert(sizeof(struct ithc_registers) == 0x1300);

void bitsl(__iomem u32 *reg, u32 mask, u32 val);
void bitsb(__iomem u8 *reg, u8 mask, u8 val);
#define bitsl_set(reg, x) bitsl(reg, x, x)
#define bitsb_set(reg, x) bitsb(reg, x, x)
int waitl(struct ithc *ithc, __iomem u32 *reg, u32 mask, u32 val);
int waitb(struct ithc *ithc, __iomem u8 *reg, u8 mask, u8 val);

void ithc_set_ltr_config(struct ithc *ithc, u64 active_ltr_ns, u64 idle_ltr_ns);
void ithc_set_ltr_idle(struct ithc *ithc);
int ithc_set_spi_config(struct ithc *ithc, u8 clkdiv, bool clkdiv8, u8 read_mode, u8 write_mode);
int ithc_spi_command(struct ithc *ithc, u8 command, u32 offset, u32 size, void *data);

