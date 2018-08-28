#ifndef _TCC_SDR_H_
#define _TCC_SDR_H_

#define I2S_DADIR0							(0x000) // Digital Audio Input Register 0
#define I2S_DADIR1							(0x004) // Digital Audio Input Register 1
#define I2S_DADIR2							(0x008) // Digital Audio Input Register 2
#define I2S_DADIR3							(0x00C) // Digital Audio Input Register 3
#define I2S_DADIR4							(0x010) // Digital Audio Input Register 4
#define I2S_DADIR5							(0x014) // Digital Audio Input Register 5
#define I2S_DADIR6							(0x018) // Digital Audio Input Register 6
#define I2S_DADIR7							(0x01C) // Digital Audio Input Register 7
#define I2S_DADOR0							(0x020) // Digital Audio Output Register 0
#define I2S_DADOR1							(0x024) // Digital Audio Output Register 1
#define I2S_DADOR2							(0x028) // Digital Audio Output Register 2
#define I2S_DADOR3							(0x02C) // Digital Audio Output Register 3
#define I2S_DADOR4							(0x030) // Digital Audio Output Register 4
#define I2S_DADOR5							(0x034) // Digital Audio Output Register 5
#define I2S_DADOR6							(0x038) // Digital Audio Output Register 6
#define I2S_DADOR7							(0x03C) // Digital Audio Output Register 7
#define I2S_DAMR							(0x040) // Digital Audio Mode Register
#define I2S_DAVC							(0x044) // Digital Audio Volume Control Register
#define I2S_MCCR0							(0x048) // Multi Channel Control Register 0
#define I2S_MCCR1							(0x04C) // Multi Channel Control Register 1
#define I2S_DRMR							(0x050) // Digital Radio Mode Register

#define DAMR_BCLK_PAD_SEL					(1 << 31)
#define DAMR_LRCK_PAD_SEL					(1 << 30)
#define DAMR_DBTEN_EN						(1 << 29)
#define DAMR_MULTI_PORT_EN					(1 << 28)
#define DAMR_AFILTER_EN						(1 << 27)

#define DAMR_RXS_MASK						(3 << 20)
#define DAMR_RXS_MSB_24BIT					(0 << 20)
#define DAMR_RXS_MSB_16BIT					(1 << 20)
#define DAMR_RXS_LSB_24BIT					(2 << 20)
#define DAMR_RXS_LSB_16BIT					(3 << 20)

#define DAMR_DAI_EN							(1 << 15)
#define DAMR_DAI_RX_EN						(1 << 13)
#define DAMR_SYSCLK_MST_SEL					(1 << 11)
#define DAMR_BITCLK_MST_SEL					(1 << 10)
#define DAMR_FRMCLK_MST_SEL					(1 << 9)

#define DAMR_BITCLK_DIV_MASK				(3 << 6)
#define DAMR_BITCLK_DIV_16					(3 << 6)
#define DAMR_BITCLK_DIV_8					(2 << 6)
#define DAMR_BITCLK_DIV_6					(1 << 6)
#define DAMR_BITCLK_DIV_4					(0 << 6)

#define DAMR_FRMCLK_DIV_MASK				(3 << 4)
#define DAMR_FRMCLK_DIV_64					(2 << 4)
#define DAMR_FRMCLK_DIV_48					(1 << 4)
#define DAMR_FRMCLK_DIV_32					(0 << 4)

#define DAMR_BIT_POLARITY					(1 << 3)

#define DRMR_BITMODE_MASK					(0x0f << 0)
#define DRMR_BITMODE_16BIT					(0 << 0)
#define DRMR_BITMODE_20BIT					(1 << 0)
#define DRMR_BITMODE_24BIT					(2 << 0)
#define DRMR_BITMODE_30BIT					(3 << 0)
#define DRMR_BITMODE_32BIT					(4 << 0)
#define DRMR_BITMODE_40BIT					(5 << 0)
#define DRMR_BITMODE_48BIT					(6 << 0)
#define DRMR_BITMODE_60BIT					(7 << 0)
#define DRMR_BITMODE_64BIT					(8 << 0)
#define DRMR_BITMODE_80BIT					(9 << 0)

#define DRMR_RADIO_EN						(1 << 4)

#define DRMR_PORTSEL_MASK					(0x03 << 8)
#define DRMR_PORTSEL_1						(0 << 8)
#define DRMR_PORTSEL_2						(1 << 8)
#define DRMR_PORTSEL_4						(2 << 8)

#define DRMR_FIFO_THRESHOLD_MASK			(0x03 << 16)
#define DRMR_FIFO_THRESHOLD_64				(0 << 16)
#define DRMR_FIFO_THRESHOLD_128				(1 << 16)
#define DRMR_FIFO_THRESHOLD_256				(2 << 16)

//#define DRMR_FIFO_TX_CLR					(1 << 20)
#define DRMR_FIFO_RX_CLR					(1 << 21)

#define MCCR0_DAO_DISABLE_MASK						(0x0f << 28)
#define MCCR0_DAO0_DISABLE					(1 << 31)
#define MCCR0_DAO1_DISABLE					(1 << 30)
#define MCCR0_DAO2_DISABLE					(1 << 29)
#define MCCR0_DAO3_DISABLE					(1 << 28)

#define ADMA_ADRCNT_MODE_Pos				(31)
#define ADMA_ADRCNT_MODE_Msk				(1U << ADMA_ADRCNT_MODE_Pos)
#define ADMA_ADRCNT_MODE_SMASK				(0U << ADMA_ADRCNT_MODE_Pos)
#define ADMA_ADRCNT_MODE_ADRCNT				(1U << ADMA_ADRCNT_MODE_Pos)

#define ADMA_RxDaDar						(0x000) // DAI Rx (Right) Data Destination Address
#define ADMA_RxDaParam						(0x004) // DAI Rx Parameters
#define ADMA_RxDaTCnt						(0x008) // DAI Rx Transmission Counter Register
#define ADMA_RxDaCdar						(0x00C) // DAI Rx (Right) Data Current Destination Address
#define ADMA_RxCdDar						(0x010) // CDIF(SPDIF) Rx (Right) Data Destination Address
#define ADMA_RxCdParam						(0x014) // CDIF(SPDIF) Rx Parameters
#define ADMA_RxCdTCnt						(0x018) // CDIF(SPDIF) Rx Transmission Counter Register
#define ADMA_RxCdCdar						(0x01C) // CDIF(SPDIF) Rx (Right) Data Current Destination Address

#define ADMA_RxDaDarL						(0x028) // DAI Rx Left Data Destination Address
#define ADMA_RxDaCdarL						(0x02C) // DAI Rx Left Data Current Destination Address
#define ADMA_RxCdDarL						(0x030) // CDIF(SPDIF) Rx Left Data Destination Address
#define ADMA_RxCdCdarL						(0x034) // CDIF(SPDIF) Rx Left Data Current Destination Address
#define ADMA_TransCtrl						(0x038) // DMA Transfer Control Register
#define ADMA_RptCtrl						(0x03C) // DMA Repeat Control Register
#define ADMA_TxDaSar						(0x040) // DAI Tx (Right) Data Source Address
#define ADMA_TxDaParam						(0x044) // DAI Tx Parameters
#define ADMA_TxDaTCnt						(0x048) // DAI Tx Transmission Counter Register
#define ADMA_TxDaCsar						(0x04C) // DAI Tx (Right) Data Current Source Address
#define ADMA_TxSpSar						(0x050) // SPDIF Tx (Right) Data Source Address
#define ADMA_TxSpParam						(0x054) // SPDIF Tx Parameters
#define ADMA_TxSpTCnt						(0x058) // SPDIF Tx Transmission Counter Register
#define ADMA_TxSpCsar						(0x05C) // SPDIF Tx (Right) Data Current Source Address
#define ADMA_TxDaSarL						(0x068) // DAI Tx Left Data Source Address
#define ADMA_TxDaCsarL						(0x06C) // DAI Tx Left Data Current Source Address
#define ADMA_TxSpSarL						(0x070) // SPDIF Tx Left Data Source Address
#define ADMA_TxSpCsarL						(0x074) // SPDIF Tx Left Data Current Source address
#define ADMA_ChCtrl							(0x078) // DMA Channel Control Register
#define ADMA_IntStatus						(0x07C) // DMA Interrupt Status Register
#define ADMA_GIntReq						(0x080) // General Interrupt Request
#define ADMA_GIntStatus						(0x084) // General Interrupt Status
#define ADMA_RxDaAdrcnt						(0x08C) // DAI Rx Transmission Address Counter Register

#define ADMA_RxDaDar1						(0x100) // DAI1 Rx (Right) Data Destination Address
#define ADMA_RxDaDar2						(0x104) // DAI2 Rx (Right) Data Destination Address
#define ADMA_RxDaDar3						(0x108) // DAI3 Rx (Right) Data Destination Address
#define ADMA_RxDaCar1						(0x10C) // DAI1 Rx (Right) Data Current Destination Address
#define ADMA_RxDaCar2						(0x110) // DAI2 Rx (Right) Data Current Destination Address
#define ADMA_RxDaCar3						(0x114) // DAI3 Rx (Right) Data Current Destination Address
#define ADMA_RxDaDarL1						(0x118) // DAI1 Rx Left Data Destination Address
#define ADMA_RxDaDarL2						(0x11C) // DAI2 Rx Left Data Destination Address
#define ADMA_RxDaDarL3						(0x120) // DAI3 Rx Left Data Destination Address
#define ADMA_RxDaCarL1						(0x124) // DAI1 Rx Left Data Current Destination Address
#define ADMA_RxDaCarL2						(0x128) // DAI2 Rx Left Data Current Destination Address
#define ADMA_RxDaCarL3						(0x12C) // DAI3 Rx Left Data Current Destination Address
#define ADMA_TxDaSar1						(0x130) // DAI1 Tx (Right) Data Source Address
#define ADMA_TxDaSar2						(0x134) // DAI2 Tx (Right) Data Source Address
#define ADMA_TxDaSar3						(0x138) // DAI3 Tx (Right) Data Source Address
#define ADMA_TxDaCsar1						(0x13C) // DAI1 Tx (Right) Data Current Source Address
#define ADMA_TxDaCsar2						(0x140) // DAI2 Tx (Right) Data Current Source Address
#define ADMA_TxDaCsar3						(0x144) // DAI3 Tx (Right) Data Current Source Address
#define ADMA_TxDaDarL1						(0x148) // DAI1 Tx Left Data Source Address
#define ADMA_TxDaDarL2						(0x14C) // DAI2 Tx Left Data Source Address
#define ADMA_TxDaDarL3						(0x150) // DAI3 Tx Left Data Source Address
#define ADMA_TxDaCarL1						(0x154) // DAI1 Tx Left Data Current Source Address
#define ADMA_TxDaCarL2						(0x158) // DAI2 Tx Left Data Current Source Address
#define ADMA_TxDaCarL3						(0x15C) // DAI3 Tx Left Data Current Source Address

#define ADMA_CHCTRL_CDIF_RX_EN				(1 << 31)
#define ADMA_CHCTRL_DAI_RX_EN				(1 << 30)
#define ADMA_CHCTRL_SPDIF_TX_EN				(1 << 29)
#define ADMA_CHCTRL_DAI_TX_EN				(1 << 28)
#define ADMA_CHCTRL_DAI_SPDIF_SAME_CLOCK_EN	(1 << 27)
#define ADMA_CHCTRL_CDIF_SPDIF_SEL_MASK		(1 << 26)
#define ADMA_CHCTRL_DAI_RX_MULTI_CH_EN		(1 << 25)
#define ADMA_CHCTRL_DAI_TX_MULTI_CH_EN		(1 << 24)

#define ADMA_CHCTRL_DAI_RX_MULTI_MASK		(0x03 << 22)
#define ADMA_CHCTRL_DAI_RX_MULTI_3_1		(0 << 22)
#define ADMA_CHCTRL_DAI_RX_MULTI_5_1_TYPE1	(1 << 22)
#define ADMA_CHCTRL_DAI_RX_MULTI_5_1_TYPE2	(2 << 22)
#define ADMA_CHCTRL_DAI_RX_MULTI_7_1		(3 << 22)

#define ADMA_CHCTRL_DAI_TX_MULTI_MASK		(0x03 << 20)
#define ADMA_CHCTRL_DAI_TX_MULTI_3_1		(0 << 20)
#define ADMA_CHCTRL_DAI_TX_MULTI_5_1_TYPE1	(1 << 20)
#define ADMA_CHCTRL_DAI_TX_MULTI_5_1_TYPE2	(2 << 20)
#define ADMA_CHCTRL_DAI_TX_MULTI_7_1		(3 << 20)

#define ADMA_CHCTRL_CDIF_LRMODE_EN			(1 << 19)
#define ADMA_CHCTRL_DAI_RX_LRMODE_EN		(1 << 18)
#define ADMA_CHCTRL_SPDIF_TX_LRMODE_EN		(1 << 17)
#define ADMA_CHCTRL_DAI_TX_LRMODE_EN		(1 << 16)

#define ADMA_CHCTRL_CDIF_WIDTH_MASK			(1 << 15)
#define ADMA_CHCTRL_DAI_RX_WIDTH_MASK		(1 << 14)
#define ADMA_CHCTRL_SPDIF_TX_WIDTH_MASK		(1 << 13)
#define ADMA_CHCTRL_DAI_TX_WIDTH_MASK		(1 << 12)

#define ADMA_CHCTRL_CDIF_INT_EN				(1 << 3)
#define ADMA_CHCTRL_DAI_RX_INT_EN			(1 << 2)
#define ADMA_CHCTRL_SPDIF_TX_INT_EN			(1 << 1)
#define ADMA_CHCTRL_DAI_TX_INT_EN			(1 << 0)

#define ADMA_TRANS_HCNT_CLR					(1 << 30)
#define ADMA_TRANS_RX_CONTINUOUS_MODE_EN	(1 << 29)
#define ADMA_TRANS_DAI_RX_REPEAT_MODE_EN	(1 << 18)
#define ADMA_TRANS_DAI_RX_BSIZE_MASK		(0x03 << 12)
#define ADMA_TRANS_DAI_RX_BSIZE_1CYCLE		(0 << 12)
#define ADMA_TRANS_DAI_RX_BSIZE_2CYCLE		(1 << 12)
#define ADMA_TRANS_DAI_RX_BSIZE_4CYCLE		(2 << 12)
#define ADMA_TRANS_DAI_RX_BSIZE_8CYCLE		(3 << 12)
#define ADMA_TRANS_DAI_RX_WSIZE_MASK		(0x03 << 4)
#define ADMA_TRANS_DAI_RX_WSIZE_8			(0 << 4)
#define ADMA_TRANS_DAI_RX_WSIZE_16			(1 << 4)
#define ADMA_TRANS_DAI_RX_WSIZE_32			(2 << 4)

#define ADMA_RPTCTRL_DBTH_MASK				(0x0f << 24)
#define ADMA_RPTCTRL_DBTH(x)				(((x)&0x0f) << 24)

#define ADMA_INTSTATUS_DRI					(1<<6)
#define ADMA_INTSTATUS_DRMI					(1<<2)

#define RADIO_RX_FIFO_THRESHOLD				(DRMR_FIFO_THRESHOLD_128)
#define RADIO_RX_DBTH						(7)
#define I2S_RX_DBTH						(7)

#if defined(CONFIG_ARCH_TCC802X)
#define MAX_TCC_SYSCLK 100*1000*1000 //Max. of Audio DAI SYSCLK
#else
#define MAX_TCC_SYSCLK  625*100*1000 //Max. of Audio DAI SYSCLK
#endif

#define MIN_TCC_MCLK_FS 256     //Min. of I2S Mode MCLK FS
#define MAX_TCC_MCLK_FS 1024    //Max. of I2S Mode MCLK FS

#define TCC_SDR_BUF_SZ_MAX					(2*1024*1024) //Bytes
#define TCC_SDR_BUF_SZ_MIN					(1024) //Bytes
#define PERIOD_DIV_IN 						(8)

#define	IOCTL_HSI2S_MAGIC					('S')
#define	HSI2S_SET_PARAMS					_IO( IOCTL_HSI2S_MAGIC, 0)
#define	HSI2S_TX_START						_IO( IOCTL_HSI2S_MAGIC, 1)
#define	HSI2S_TX_STOP						_IO( IOCTL_HSI2S_MAGIC, 2)
#define	HSI2S_RX_START						_IO( IOCTL_HSI2S_MAGIC, 3)
#define	HSI2S_RX_STOP						_IO( IOCTL_HSI2S_MAGIC, 4)
#define	HSI2S_RADIO_MODE_RX_DAI				_IO( IOCTL_HSI2S_MAGIC, 5)
#define	HSI2S_I2S_MODE_RX_DAI				_IO( IOCTL_HSI2S_MAGIC, 6)

enum {
	SDR_BIT_POLARITY_POSITIVE_EDGE = 0,
	SDR_BIT_POLARITY_NEGATIVE_EDGE = 1,
};

typedef struct HS_I2S_PARAM_ {
	unsigned int eSampleRate;
	unsigned int eRadioMode;
	unsigned int eBitMode;
	unsigned int eBitPolarity;
	unsigned int eBufferSize;
	unsigned int eChannel;
	unsigned int Reserved3[4];
} HS_I2S_PARAM;

typedef struct HS_RADIO_RX_PARAM_ {
	char *eBuf;
	unsigned int eReadCount;
	int eIndex;
} HS_RADIO_RX_PARAM;

#if defined(CONFIG_ARCH_TCC802X)
typedef struct HS_I2S_PORT_ {
	char clk[3];
	char dain[4];
} HS_I2S_PORT;

extern void tca_sdr_port_mux(void *pDAIBaseAddr, HS_I2S_PORT *port);
#endif

#endif /*_TCC_SDR_H_*/
