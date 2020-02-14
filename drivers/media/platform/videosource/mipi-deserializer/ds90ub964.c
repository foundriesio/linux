/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include "../videosource_common.h"
#include "../videosource_i2c.h"
#include "../mipi-csi2/mipi-csi2.h"

//#define CONFIG_MIPI_1_CH_CAMERA
#define CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT
#define CONFIG_INTERNAL_FSYNC

#if defined (CONFIG_MIPI_1_CH_CAMERA) || !defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
#define WIDTH	1280
#else
#define WIDTH	5120
#endif
#define HEIGHT	720

extern volatile void __iomem *	ddicfg_base;
static unsigned int des_irq_num;

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};

#define SER_ADDR	(0x80 >> 1)

static struct videosource_reg sensor_camera_yuv422_8bit_4ch[] = {
// init
// enable 4ch
	{ 0x1F, 0x02},	//# CSI_PLL_CTL  - with 913A, should be 0x02
	{ 0xff, 100},  //time.sleep(0.1)

//# TIDA00262 1
	{ 0x4C, 0x01},	//# FPD3_PORT_SEL -  write PORT0
	{ 0x58, 0x58},	//# I2C pass through, BC enable, BC CRC
	{ 0x5C, 0xE8},	//# SER Alias
	{ 0x5D, 0x2E},	//# SlaveID[0]
	{ 0x65, 0x80},	//# SlaveAlias[0]
	{ 0x7C, 0x00},	//# PORT_CONFIG2 - LineValid Polarity and FrameValid Polarity
	{ 0x6e, 0x99},	//# BC_GPIO_CTL0 - BC_GPIO1_SEL and BC_GPIO0_SEL
					//YUV422 8BIT 0X1E
	{ 0x70, 0x1E},	//# RAW10_ID
	{ 0x71, 0x1E},	//# RAW12_ID
//	  { 0x7C, 0x81},  //# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits
	{ 0x7C, 0x99},	//# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits

//# TIDA00262 2
	{ 0x4C, 0x12},
	{ 0x58, 0x58},
	{ 0x5C, 0xEC},
	{ 0x5D, 0x2E},
	{ 0x65, 0x84},
	{ 0x7C, 0x00},
	{ 0x6e, 0x99},

#if 1
	{ 0x70, 0x5E},	//# RAW10_ID
	{ 0x71, 0x5E},
#else
	{ 0x70, 0x1E},	//# RAW10_ID
	{ 0x71, 0x1E},	//# RAW12_ID
#endif
//	  { 0x7C, 0x81},  //# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits
	{ 0x7C, 0x99},	//# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits

//# TIDA00262 3
	{ 0x4C, 0x24},
	{ 0x58, 0x58},
	{ 0x5C, 0xF0},
	{ 0x5D, 0x2E},
	{ 0x65, 0x88},
	{ 0x7C, 0x00},
	{ 0x6e, 0x99},

#if 1
	{ 0x70, 0x9E},
	{ 0x71, 0x9E},
#else
	{ 0x70, 0x1E},	//# RAW10_ID
	{ 0x71, 0x1E},	//# RAW12_ID
#endif
//	  { 0x7C, 0x81},  //# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits
	{ 0x7C, 0x99},	//# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits

//# TIDA00262 4
	{ 0x4C, 0x38},
	{ 0x58, 0x58},
	{ 0x5C, 0xF4},
	{ 0x5D, 0x2E},
	{ 0x65, 0x8C},
	{ 0x7C, 0x00},
	{ 0x6e, 0x99},

#if 1
	{ 0x70, 0xDE},
	{ 0x71, 0xDE},
#else
	{ 0x70, 0x1E},	//# RAW10_ID
	{ 0x71, 0x1E},	//# RAW12_ID
#endif
//	  { 0x7C, 0xc0},  //# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits
//	  { 0x7C, 0x81},  //# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits
	{ 0x7C, 0x99},	//# RAW10_8BIT_CTL = 8-bit processing using upper 8 bits

					//# BspUtils_I2cParams ub964Cfg[]?
	{ 0x32, 0x01},	//# CSI_PORT_SEL ? PORT 0 select
	{ 0x33, 0x03},	//# CSI_CTL ? 4L, CSI enable

#ifdef CONFIG_MIPI_1_CH_CAMERA
	// 1ch
	{ 0x20, 0x70},	//# FWD_CTL1 ? forwarding enable
#else
	// 4ch
	{ 0x20, 0x00},	//# FWD_CTL1 ? forwarding enable
#endif

#ifdef CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT
//	  { 0x21, 0x3c},  //# synchronous forwarding with line concatenation
	{ 0x21, 0x7c},	//# synchronous forwarding with line concatenation
#endif

	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_frame_sync[] = {
#ifdef CONFIG_INTERNAL_FSYNC
				 // enable internally generated frame sync
	{0x4C,0x01}, // RX0
	{0x6E,0xAA}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1
	{0x4C,0x12}, // RX1
	{0x6E,0xAA}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1
	{0x4C,0x24}, // RX2
	{0x6E,0xAA}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1
	{0x4C,0x38}, // RX3
	{0x6E,0xAA}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1
	{0x10,0x91}, // FrameSync signal; Device Status; Enabled
	{0x58,0x58}, // BC FREQ SELECT: 2.5 Mbps

#if 0 //60hz
	{0x19,0x00}, // FS_HIGH_TIME_1
	{0x1A,0x8A}, // FS_HIGH_TIME_0
	{0x1B,0x04}, // FS_LOW_TIME_1
	{0x1C,0xE1}, // FS_LOW_TIME_0
#else //30hz
	{0x19,0x02}, // FS_HIGH_TIME_1
	{0x1A,0x16}, // FS_HIGH_TIME_0
	{0x1B,0x08}, // FS_LOW_TIME_1
	{0x1C,0xc4}, // FS_LOW_TIME_0
#endif

	{0x18,0x01}, // Enable FrameSync
#else
				 // EXTERNAL FSYNC
	{0x4C,0x01}, // RX0
	{0x6E,0x00}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1
	{0x4C,0x12}, // RX1
	{0x6E,0x00}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1
	{0x4C,0x24}, // RX2
	{0x6E,0x00}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1
	{0x4C,0x38}, // RX3
	{0x6E,0x00}, // BC_GPIO_CTL0: FrameSync signal to GPIO0/1

	{0x18,0x81}, // Enable FrameSync
#endif
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_interrupt[] = {
	{ 0x4C, 0x01},	//# FPD3_PORT_SEL -  write PORT0
	{ 0xd8, 0x07},	// ICR_IH
	{ 0xd9, 0x74},	// ICR_IL

	{ 0x4C, 0x12},	//# FPD3_PORT_SEL -  write PORT1
	{ 0xd8, 0x07},	// ICR_IH
	{ 0xd9, 0x74},	// ICR_IL

	{ 0x4C, 0x24},	//# FPD3_PORT_SEL -  write PORT2
	{ 0xd8, 0x07},	// ICR_IH
	{ 0xd9, 0x74},	// ICR_IL

	{ 0x4C, 0x38},	//# FPD3_PORT_SEL -  write PORT3
	{ 0xd8, 0x07},	// ICR_IH
	{ 0xd9, 0x74},	// ICR_IL

#if 0
	{ 0x32, 0x01},	// write CSI TX PORT0
	{ 0x36, 0x1e},	// ICR
#endif
					// enable intb
	{0x36,0x08},
	{0x23,0x9F},	//rx all & intb pin en

	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_serializer[] = {

	{REG_TERM, VAL_TERM}
};

/* refer below camera_mode type and make up videosource_reg_table_list

 enum camera_mode {
	 MODE_INIT		   = 0,
	 MODE_SERDES_FSYNC,
	 MODE_SERDES_INTERRUPT,
	 MODE_SERDES_REMOTE_SER,
 };

*/

static struct videosource_reg * videosource_reg_table_list[] = {
	sensor_camera_yuv422_8bit_4ch,
	sensor_camera_enable_frame_sync,
	sensor_camera_enable_interrupt,
	sensor_camera_enable_serializer,
};

static int write_regs(struct i2c_client * client, const struct videosource_reg * list, unsigned int mode) {
	unsigned char data[4];
	unsigned char bytes;
	int ret, err_cnt = 0;

	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
#if 1
		if(list->reg == 0xFF && list->val != 0xFF) {
			mdelay(list->val);
			list++;
		}
		else {
#endif
			bytes = 0;
			data[bytes++]= (unsigned char)list->reg&0xff;
			data[bytes++]= (unsigned char)list->val&0xff;

			if(mode == MODE_SERDES_REMOTE_SER) {
				ret = DDI_I2C_Write_Remote(client, SER_ADDR, data, 1, 1);
			}
			else {
				ret = DDI_I2C_Write(client, data, 1, 1);
			}

			if(ret) {
				if(4 <= ++err_cnt) {
					loge("Sensor I2C !!!! \n");
					return ret;
				}
			} else {
#if 0			// for debug
				unsigned char value = 0;

				if(mode == MODE_SERDES_REMOTE_SER) {
					DDI_I2C_Read(client, SER_ADDR, list->reg, 1, &value, 1);
				}
				else {
					DDI_I2C_Read(client, list->reg, 1, &value, 1);
				}

				if(list->val != value)
					logd("ERROR :addr(0x%x) write(0x%x) read(0x%x) \n", list->reg, list->val, value);
				else
					logd("OK : addr(0x%x) write(0x%x) read(0x%x) \n", list->reg, list->val, value);
#endif

#if 0
				if((unsigned int)list->reg == (unsigned int)0x90) {
	//				logd("delay(1)\n");
					mdelay(1);
				}
#endif
				err_cnt = 0;
				list++;
			}
		}
	}

	return 0;
}

static void check_interrupt_status(struct i2c_client * client)
{
	unsigned char RX_PORT_STS1 = 0, RX_PORT_STS2 = 0, LINE_COUNT_1 = 0, LINE_COUNT_0 = 0, LINE_LEN_1 = 0, LINE_LEN_0 = 0;
	unsigned int val = 0;

	/*
	 *	LINE_LEN_1 = ReadI2C(0x75)
	 */
	DDI_I2C_Read(client, 0x75, 1, &LINE_LEN_1, 1);
	val = LINE_LEN_1;
	val <<= 8;

	/*
	 *	LINE_LEN_0 = ReadI2C(0x76)
	 */
	DDI_I2C_Read(client, 0x76, 1, &LINE_LEN_0, 1);
	val |= LINE_LEN_0;

//	logd("line length high : 0x%x , line length low : 0x%x (%d) \n", LINE_LEN_1, LINE_LEN_0, val);
	logd("line length  %d \n", val);

	/*
	 *	LINE_COUNT_1 = ReadI2C(0x73)
	 */
	DDI_I2C_Read(client, 0x73, 1, &LINE_COUNT_1, 1);
	val = LINE_COUNT_1;
	val <<= 8;

	/*
	 *	LINE_COUNT_1 = ReadI2C(0x74)
	 */
	DDI_I2C_Read(client, 0x74, 1, &LINE_COUNT_0, 1);
	val |= LINE_COUNT_0;

//	logd("line count high : 0x%x , line count low : 0x%x (%d) \n", LINE_COUNT_1, LINE_COUNT_0, val);
	logd("line count %d \n", val);

#if 0
	/*
	 *	PORT_ISR_LO = ReadI2C(0xDB)
	 */
	DDI_I2C_Read(client, 0xDB, 1, &PORT_ISR_LO, 1);

#if 1
//	  logd("0xDB PORT_ISR_LO : 0x%x \n", PORT_ISR_LO); //# readout; cleared by RX_PORT_STS2
	if ((PORT_ISR_LO & 0x40) >> 6) {
		logd("# IS_LINE_LEN_CHG INTERRUPT DETECTED \n");
	}
	if ((PORT_ISR_LO & 0x20) >> 5) {
		logd( "# IS_LINE_CNT_CHG DETECTED \n");
	}
	if ((PORT_ISR_LO & 0x10) >> 4)
		logd( "# IS_BUFFER_ERR DETECTED \n");
	if ((PORT_ISR_LO & 0x08) >> 3)
		logd( "# IS_CSI_RX_ERR DETECTED \n");
	if ((PORT_ISR_LO & 0x04) >> 2)
		logd( "# IS_FPD3_PAR_ERR DETECTED \n");
	if ((PORT_ISR_LO & 0x02) >> 1)
		logd( "# IS_PORT_PASS DETECTED \n");
	if ((PORT_ISR_LO & 0x01) )
		logd( "# IS_LOCK_STS DETECTED \n");
 #endif
	/*
	 *	PORT_ISR_HI = ReadI2C(0xDA)
	 */
	DDI_I2C_Read(client, 0xDA, 1, &PORT_ISR_HI, 1);

#if 0
//	  logd("0xDA PORT_ISR_HI : 0x%x \n", PORT_ISR_HI); //# readout; cleared by RX_PORT_STS2

	if ((PORT_ISR_HI & 0x04) >> 2)
		logd( "# IS_FPD3_ENC_ERR DETECTED \n");
	if ((PORT_ISR_HI & 0x02) >> 1)
		logd( "# IS_BCC_SEQ_ERR DETECTED \n");
	if ((PORT_ISR_HI & 0x01) )
		logd( "# IS_BCC_CRC_ERR DETECTED \n");
#endif
#endif

	/*
	 *	RX_PORT_STS1 = ReadI2C(0x4D)
	 */
	DDI_I2C_Read(client, 0x4D, 1, &RX_PORT_STS1, 1);

//	logd("0x4D RX_PORT_STS1 : 0x%x \n", RX_PORT_STS1);

#if 0
	if (((RX_PORT_STS1 & 0xc0) >> 6) == 3)
		logd( "# RX_PORT_NUM = RX3 \n");
	else if (((RX_PORT_STS1 & 0xc0) >> 6) == 2)
		logd( "# RX_PORT_NUM = RX2 \n");
	else if (((RX_PORT_STS1 & 0xc0) >> 6) == 1)
		logd( "# RX_PORT_NUM = RX1 \n");
	else if (((RX_PORT_STS1 & 0xc0) >> 6) == 0)
		logd( "# RX_PORT_NUM = RX0 \n");
#endif

	if ((RX_PORT_STS1 & 0x20) >> 5)
		logd( "# BCC_CRC_ERR DETECTED \n");
	if ((RX_PORT_STS1 & 0x10) >> 4)
		logd( "# LOCK_STS_CHG DETECTED \n");
	if ((RX_PORT_STS1 & 0x08) >> 3)
		logd( "# BCC_SEQ_ERROR DETECTED \n");
	if ((RX_PORT_STS1 & 0x04) >> 2)
		logd( "# PARITY_ERROR DETECTED \n");
	if ((RX_PORT_STS1 & 0x02) >> 1)
		logd( "# PORT_PASS=1 \n");
	if ((RX_PORT_STS1 & 0x01) )
		logd( "# LOCK_STS=1 \n");

	/*
	 *	RX_PORT_STS2 = ReadI2C(0x4E)
	 */
	DDI_I2C_Read(client, 0x4E, 1, &RX_PORT_STS2, 1);

//	logd("0x4E RX_PORT_STS2 : 0x%x \n", RX_PORT_STS2);

	if ((RX_PORT_STS2 & 0x80) >> 7)
		logd( "# LINE_LEN_UNSTABLE DETECTED \n");
	if ((RX_PORT_STS2 & 0x40) >> 6) {
		logd( "# LINE_LEN_CHG \n");

		/*
		 *	LINE_LEN_1 = ReadI2C(0x75)
		 */
		DDI_I2C_Read(client, 0x75, 1, &LINE_LEN_1, 1);
		val = LINE_LEN_1;
		val <<= 8;

		/*
		 *	LINE_LEN_0 = ReadI2C(0x76)
		 */
		DDI_I2C_Read(client, 0x76, 1, &LINE_LEN_0, 1);
		val |= LINE_LEN_0;

	//	logd("line length high : 0x%x , line length low : 0x%x (%d) \n", LINE_LEN_1, LINE_LEN_0, val);
		logd("line length  %d \n", val);
	}
	if ((RX_PORT_STS2 & 0x20) >> 5)
		logd( "# FPD3_ENCODE_ERROR DETECTED \n");
	if ((RX_PORT_STS2 & 0x10) >> 4)
		logd( "# BUFFER_ERROR DETECTED \n");
	if ((RX_PORT_STS2 & 0x08) >> 3)
		logd( "# CSI_ERR DETECTED \n");
	if ((RX_PORT_STS2 & 0x04) >> 2)
		logd( "# FREQ_STABLE DETECTED \n");
	if ((RX_PORT_STS2 & 0x02) >> 1)
		logd( "# NO_FPD3_CLK DETECTED \n");
	if ((RX_PORT_STS2 & 0x01) ) {
		logd( "# LINE_CNT_CHG DETECTED \n");

		/*
		 *	LINE_COUNT_1 = ReadI2C(0x73)
		 */
		DDI_I2C_Read(client, 0x73, 1, &LINE_COUNT_1, 1);
		val = LINE_COUNT_1;
		val <<= 8;

		/*
		 *	LINE_COUNT_1 = ReadI2C(0x74)
		 */
		DDI_I2C_Read(client, 0x74, 1, &LINE_COUNT_0, 1);
		val |= LINE_COUNT_0;

	//	logd("line count high : 0x%x , line count low : 0x%x (%d) \n", LINE_COUNT_1, LINE_COUNT_0, val);
		logd("line count %d \n", val);

	}
	logd("\n\n");
}

static void process_interrupt(struct i2c_client * client) {
	unsigned char interrupt_sts = 0;
	unsigned char data[2] = {0, };
	unsigned char retVal = 0;
	FUNCTION_IN

	if(DDI_I2C_Read(client, 0x24 , 1, &interrupt_sts, 1)) {
	   logd("ERROR: Sensor I2C !!!! \n");
	   return /*IRQ_HANDLED*/;
	}

	// find where does interrupt occurred
	if ((interrupt_sts & 0x80) >> 7)
		logd( "# GLOBAL INTERRUPT DETECTED \n");

	if ((interrupt_sts & 0x40) >> 6)
		logd( "# RESERVED \n");

	if ((interrupt_sts & 0x20) >> 5) {
		logd( "# IS_CSI_TX1 DETECTED \n");

	}
	if ((interrupt_sts & 0x10) >> 4) {
		logd("# ====== IS_CSI_TX0 DETECTED \n");

		data[0] = 0x32;
		data[1] = 0x01;

		DDI_I2C_Write(client, data, 1, 1);

		DDI_I2C_Read(client, 0x37, 1, &retVal, 1);

		logd("CSI_TX_ISR(0x37) : 0x%x \n", retVal);

		if (((retVal & 0x08) >> 3) == 1)
			logd( "# IS_CSI_SYNC_ERROR DETTECTED\n");

		logd("\n\n");

	}
	if ((interrupt_sts & 0x08) >> 3) {
		logd("# ====== IS_RX3 DETECTED \n");

		data[0] = 0x4c;
		data[1] = 0x38;

		DDI_I2C_Write(client, data, 1, 1);

		check_interrupt_status(client);
	}
	if ((interrupt_sts & 0x04) >> 2) {
		logd("# ====== IS_RX2 DETECTED \n");

		data[0] = 0x4c;
		data[1] = 0x24;
		DDI_I2C_Write(client, data, 1, 1);

		check_interrupt_status(client);
	}
	if ((interrupt_sts & 0x02) >> 1) {
		logd("# ====== IS_RX1 DETECTED \n");

		data[0] = 0x4c;
		data[1] = 0x12;
		DDI_I2C_Write(client, data, 1, 1);

		check_interrupt_status(client);

	}
	if ((interrupt_sts & 0x01) ) {
		logd("# ====== IS_RX0 DETECTED \n");

		data[0] = 0x4c;
		data[1] = 0x01;
		DDI_I2C_Write(client, data, 1, 1);

		check_interrupt_status(client);
	}
	FUNCTION_OUT
}

static irqreturn_t irq_handler(int irq, void * client_data) {
	return IRQ_WAKE_THREAD;
}

static irqreturn_t irq_thread_handler(int irq, void * client_data) {
	process_interrupt(client_data);

	return IRQ_HANDLED;
}

static int set_irq_handler(videosource_gpio_t * gpio, unsigned int enable) {
	if(enable) {
		if(0 < gpio->intb_port) {
			des_irq_num = gpio_to_irq(gpio->intb_port);

			log("des_irq_num : %d \n", des_irq_num);

//			if(request_irq(des_irq_num, irq_handler, IRQF_TRIGGER_FALLING, "ds90ub964", NULL)) {
//			if(request_threaded_irq(des_irq_num, irq_handler, irq_thread_handler, IRQF_SHARED | IRQF_TRIGGER_FALLING, "ds90ub964", "ds90ub964")) {
			if(request_threaded_irq(des_irq_num, irq_handler, irq_thread_handler, IRQF_TRIGGER_FALLING, "ds90ub964", NULL)) {
				loge("fail request irq(%d) \n", des_irq_num);

				return -1;
			}
		}
	}
	else {
		free_irq(des_irq_num, /*"ds90ub964"*/NULL);
	}

	return 0;
}

static int open(videosource_gpio_t * gpio) {
	int ret = 0;

	FUNCTION_IN

	sensor_port_disable(gpio->pwd_port);
	mdelay(20);

	sensor_port_enable(gpio->pwd_port);
	mdelay(20);

	FUNCTION_OUT
	return ret;
}

static int close(videosource_gpio_t * gpio) {
	FUNCTION_IN

	sensor_port_disable(gpio->rst_port);
	sensor_port_disable(gpio->pwr_port);
	sensor_port_disable(gpio->pwd_port);

	mdelay(5);

	FUNCTION_OUT
	return 0;
}

static int change_mode(struct i2c_client * client, int mode) {
	int	entry	= sizeof(videosource_reg_table_list) / sizeof(videosource_reg_table_list[0]);
	int	ret		= 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		loge("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	ret = write_regs(client, videosource_reg_table_list[mode], mode);

//	mdelay(10);
#if 1
	if(mode == MODE_SERDES_INTERRUPT)
		process_interrupt(client);
#endif

	return ret;
}

videosource_t videosource_ds90ub964 = {
	.type							= VIDEOSOURCE_TYPE_MIPI,

	.format = {
		.width						= WIDTH,
		.height						= HEIGHT,// - 1,
		.crop_x 					= 0,
		.crop_y 					= 0,
		.crop_w 					= 0,
		.crop_h 					= 0,
		.interlaced					= V4L2_DV_PROGRESSIVE,	//V4L2_DV_INTERLACED,
		.polarities					= 0,					// V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL,
		.data_order					= ORDER_RGB,
		.data_format				= FMT_YUV422_8BIT,		// data format
		.bit_per_pixel				= 8,					// bit per pixel
		.gen_field_en				= OFF,
		.de_active_low				= ACT_LOW,
		.field_bfield_low			= OFF,
		.vs_mask					= OFF,
		.hsde_connect_en			= OFF,
		.intpl_en					= OFF,
		.conv_en					= OFF,					// OFF: BT.601 / ON: BT.656
		.se							= ON,
		.fvs						= ON,

		.capture_w					= WIDTH,
		.capture_h					= HEIGHT,// - 1,

		.capture_zoom_offset_x		= 0,
		.capture_zoom_offset_y		= 0,
		.cam_capchg_width			= WIDTH,
		.framerate					= 30,
		.capture_skip_frame 		= 3,
		.sensor_sizes				= sensor_sizes,

#ifdef CONFIG_MIPI_1_CH_CAMERA
		.des_info.input_ch_num		= 1,
#else
		.des_info.input_ch_num		= 4,
#endif

		.des_info.pixel_mode		= PIXEL_MODE_SINGLE,
		.des_info.interleave_mode	= INTERLEAVE_MODE_VC_DT,
		.des_info.data_lane_num 	= 4,
		.des_info.data_format		= DATA_FORMAT_YUV422_8BIT,
		.des_info.hssettle			= 0x11,
		.des_info.clksettlectl		= 0x00,
	},

	.driver = {
		.open						= open,
		.close						= close,
		.change_mode				= change_mode,
		.set_irq_handler			= set_irq_handler,
	},
};

