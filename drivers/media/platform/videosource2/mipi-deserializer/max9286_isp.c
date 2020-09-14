/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include "max9286_isp.h"
#include "../videosource_common.h"
#include "../videosource_i2c.h"
#include "../videosource_if.h"
#include "../../tcc-mipi-csi2/tcc-mipi-csi2-reg.h"

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/interrupt.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
// #include <media/v4l2-of.h>

#define CONFIG_INTERNAL_FSYNC

#define NTSC_NUM_ACTIVE_PIXELS (1280 - 16)
#define NTSC_NUM_ACTIVE_LINES (960 - 16)

#define WIDTH NTSC_NUM_ACTIVE_PIXELS
#define HEIGHT NTSC_NUM_ACTIVE_LINES
#define ONSEMI_ADDR	(0x20 >> 1)
#define SER_ADDR	(0x80 >> 1)

static unsigned int des_irq_num;

static struct capture_size sensor_sizes[] = {{WIDTH, HEIGHT}, {WIDTH, HEIGHT}};
static const struct v4l2_fmtdesc max9286_isp_fmt_list[] = {
    [FMT_YUV420] =
	{
	    .index = 0,
	    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	    .flags = V4L2_TC_USERBITS_USERDEFINED,
	    .description = "8-bit YUV420 Format",
	    .pixelformat = V4L2_PIX_FMT_YUV420,
	},
    [FMT_YUV422] =
	{
	    .index = 1,
	    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	    .flags = V4L2_TC_USERBITS_USERDEFINED,
	    .description = "8-bit YUV422P Format",
	    .pixelformat = V4L2_PIX_FMT_YUV422P,
	},
};

static const struct vsrc_std_info max9286_isp_std_list[] = {
    [STD_NTSC_M] = {.width = NTSC_NUM_ACTIVE_PIXELS,
		    .height = NTSC_NUM_ACTIVE_LINES,
		    .video_std = VIDEO_STD_NTSC_M_BIT,
		    .standard =
			{
			    .index = 0,
			    .id = V4L2_STD_NTSC_M,
			    .name = "NTSC_M",
			}},
    [STD_NTSC_443] = {.width = NTSC_NUM_ACTIVE_PIXELS,
		      .height = NTSC_NUM_ACTIVE_LINES,
		      .video_std = VIDEO_STD_NTSC_443_BIT,
		      .standard =
			  {
			      .index = 3,
			      .id = V4L2_STD_NTSC_443,
			      .name = "NTSC_443",
			  }}
    // other supported standard have not set yet
};

static int change_mode(struct i2c_client *client, int mode);

static inline int max9286_isp_read(struct v4l2_subdev *sd, unsigned short reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	char reg_buf[1], val_buf[1];
	int ret = 0;

	reg_buf[0]= (char)(reg & 0xff);

	/* return i2c_smbus_read_byte_data(client, reg); */
	ret = DDI_I2C_Read(client, reg_buf, 1, val_buf, 1);
	if (ret < 0) {
		loge("Failed to read i2c value from 0x%08x\n", reg);
	}

	return ret;
}

static inline int max9286_isp_write(struct v4l2_subdev *sd, unsigned char reg,
				 unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return DDI_I2C_Write(client, &value, 1, 1);
}

static inline int max9286_isp_write_regs(struct i2c_client *client,
				      struct videosource_reg *list, unsigned int mode)
{
	unsigned short addr = 0;
	unsigned char data[4];
	unsigned char bytes;
	int ret, err_cnt = 0;

	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		if(list->reg == 0xFF && list->val != 0xFF) {
			mdelay(list->val);
			list++;
		} else {
			bytes = 0;
			if(mode == MODE_SERDES_SENSOR) {
				data[bytes++]= (list->reg >> 8) & 0xff;
			}
			data[bytes++]= list->reg&0xff;
			if(mode == MODE_SERDES_SENSOR) {
				data[bytes++]= (list->val >> 8) & 0xff;
			}
			data[bytes++]= list->val&0xff;

			// save the slave address
			if(mode == MODE_SERDES_SENSOR) {
				addr = client->addr;
				client->addr = ONSEMI_ADDR;
			}
			else if(mode == MODE_SERDES_REMOTE_SER || \
				mode == MODE_SERDES_REMOTE_SER_SERIALIZATION) {
				addr = client->addr;
				client->addr = SER_ADDR;
			}

			if(mode == MODE_SERDES_SENSOR) {
				// i2c write
				ret = DDI_I2C_Write(client, data, 2, 2);
			} else {
				// i2c write
				ret = DDI_I2C_Write(client, data, 1, 1);
			}
			// restore the slave address
			if(addr != 0) {
				client->addr = addr;
			}

			if(ret) {
				if(4 <= ++err_cnt) {
					printk("Sensor I2C !!!! \n");
					return ret;
				}
			} else {
				err_cnt = 0;
				list++;
			}
		}
	}

	return 0;
}

static struct videosource_reg sensor_camera_yuv422_8bit_4ch[] = {
	{0X0A, 0X0F}, // Disable all Forward control channel
	{0X34, 0Xb5}, // Enable auto acknowledge
	{0X15, 0X83}, // Select the combined camera line format for CSI2 output
	{0X12, 0Xc7}, // SINGLE INPUT MODE, MIPI Output setting(RAW12)
	{0X1C, 0xf6}, // BWS: 27bit
	{0xff,    5}, // 5mS
	{0X63, 0X00}, // Widows off
	{0X64, 0X00}, // Widows off
	{0X62, 0X1F}, // FRSYNC Diff off

	/* FSYNC
	 * PCLK is 50MHz
	 * FPS is 27
	 */
	{0x01, 0xe0}, // manual mode, enable gpi(des) - gpo(ser) transmission
	//{0x01, 0xc0}, // manual mode, enable gpi(des) - gpo(ser) transmission
	{0X08, 0X1C}, // FSYNC-period-High
	{0X07, 0X41}, // FSYNC-period-Mid
	{0X06, 0XCB}, // FSYNC-period-Low
	{0X00, 0XE1}, // Port 0 used
	//{0X00, 0XE3}, // Port 0, 1 used
	//{0X00, 0XEF}, // Port 0~3 used
	{0x0c, 0x11}, // disable HS/VS encoding
	{0xff,    5}, // 5mS

	{0X15, 0X9B}, // Enable MIPI output (line interleave)
	{0X69, 0XF0}, // Auto mask & comabck enable
	{0x01, 0xe0}, // enable gpi(des) - gpo(ser) transmission
	
	{0X0A, 0XFF}, // All forward channel enable

	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_frame_sync[] = {
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_interrupt[] = {
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_serializer[] = {
	{0x04, 0x47}, // configuration mode(in the case of pclk detection fail)
	{0x07, 0x40}, // dbl off, hs/vs encoding off, 27bit
	{0x0f, 0xbe}, // GPO output low to reset sensor
	{0xff,  300},
	{0x0f, 0xbf}, // GPO output hight to reset release of sensor
	
	/* test */
	//{0x07, 0xa4},
	//{0x4d, 0xc0},
//	{0x07, 0xc0}, // high bandwidth mode when BWS, HS/VS encoding disable
	{0xff,  100},
	
	// cross bar(Sensor Data Dout7 -> DIN0)
	{0x20, 0x0b},
	{0x21, 0x0a},
	{0x22, 0x09},
	{0x23, 0x08},
	{0x24, 0x07},
	{0x25, 0x06},
	{0x26, 0x05},
	{0x27, 0x04},
	{0x28, 0x03},
	{0x29, 0x02},
	{0x2a, 0x01},
	{0x2b, 0x00},
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_init_sensor[] = {
	//[AR0147Rev3_1280x960_Para_3exp_30fps]
	{0x301A, 0x10D8}, // RESET_REGISTER
	{0xff, 500},
	//STATE=Sensor Reset,1
	{0xff, 200},
	//STATE=Sensor Reset,0
	//XMCLK=24000000
	//Recommended Settings
	{0x30B0, 0x980E}, // DIGITAL_TEST
	{0x3C08, 0x0000}, // CONFIGURE_BUFFERS2
	{0x3C0C, 0x0518}, // DELAY_BUFFER_LLPCK_RD_WR_OVERLAP
	{0x3092, 0x1A24}, // ROW_NOISE_CONTROL
	{0x30B4, 0x01C7}, // TEMPSENS0_CTRL_REG
	{0x3372, 0x700F}, // DBLC_FS0_CONTROL
	{0x33EE, 0x0344}, // TEST_CTRL
	{0x350C, 0x035A}, // DAC_LD_12_13
	{0x350E, 0x0514}, // DAC_LD_14_15
	{0x3518, 0x14FE}, // DAC_LD_24_25
	{0x351A, 0x6000}, // DAC_LD_26_27
	{0x3520, 0x08CC}, // DAC_LD_32_33
	{0x3522, 0xCC08}, // DAC_LD_34_35
	{0x3524, 0x0C00}, // DAC_LD_36_37
	{0x3526, 0x0F00}, // DAC_LD_38_39
	{0x3528, 0xEEEE}, // DAC_LD_40_41
	{0x352A, 0x089F}, // DAC_LD_42_43
	{0x352C, 0x0012}, // DAC_LD_44_45
	{0x352E, 0x00EE}, // DAC_LD_46_47
	{0x3530, 0xEE00}, // DAC_LD_48_49
	{0x3536, 0xFF20}, // DAC_LD_54_55
	{0x3538, 0x3CFF}, // DAC_LD_56_57
	{0x353A, 0x9000}, // DAC_LD_58_59
	{0x353C, 0x7F00}, // DAC_LD_60_61
	{0x3540, 0xC62C}, // DAC_LD_64_65
	{0x3542, 0x4B4B}, // DAC_LD_66_67
	{0x3544, 0x3C46}, // DAC_LD_68_69
	{0x3546, 0x5A5A}, // DAC_LD_70_71
	{0x3548, 0x6400}, // DAC_LD_72_73
	{0x354A, 0x007F}, // DAC_LD_74_75
	{0x3556, 0x1010}, // DAC_LD_86_87
	{0x3566, 0x7328}, // DAC_LD_102_103
	{0x3F90, 0x0800}, // TEMPVSENS0_TMG_CTRL
	{0x3510, 0x011F}, // DAC_LD_16_17
	{0x353E, 0x801F}, // DAC_LD_62_63
	{0x3F9A, 0x0001}, // TEMPVSENS0_BOOST_SAMP_CTRL
	{0x3116, 0x0001}, // HDR_CONTROL3
	{0x3102, 0x60A0}, // DLO_CONTROL1
	{0x3104, 0x60A0}, // DLO_CONTROL2
	{0x3106, 0x60A0}, // DLO_CONTROL3
	{0x3362, 0x0000}, // DC_GAIN
	{0x3366, 0xCCCC}, // ANALOG_GAIN
	{0x3552, 0x0FB0}, // DAC_LD_82_83
	////sequencer
	{0x2512, 0x8000}, // SEQ_CTRL_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x3350}, // SEQ_DATA_PORT
	{0x2510, 0x2004}, // SEQ_DATA_PORT
	{0x2510, 0x1420}, // SEQ_DATA_PORT
	{0x2510, 0x1578}, // SEQ_DATA_PORT
	{0x2510, 0x087B}, // SEQ_DATA_PORT
	{0x2510, 0x24FF}, // SEQ_DATA_PORT
	{0x2510, 0x24FF}, // SEQ_DATA_PORT
	{0x2510, 0x24EA}, // SEQ_DATA_PORT
	{0x2510, 0x2410}, // SEQ_DATA_PORT
	{0x2510, 0x2224}, // SEQ_DATA_PORT
	{0x2510, 0x1015}, // SEQ_DATA_PORT
	{0x2510, 0xD813}, // SEQ_DATA_PORT
	{0x2510, 0x0214}, // SEQ_DATA_PORT
	{0x2510, 0x0024}, // SEQ_DATA_PORT
	{0x2510, 0xFF24}, // SEQ_DATA_PORT
	{0x2510, 0xFF24}, // SEQ_DATA_PORT
	{0x2510, 0xEA23}, // SEQ_DATA_PORT
	{0x2510, 0x2464}, // SEQ_DATA_PORT
	{0x2510, 0x7A24}, // SEQ_DATA_PORT
	{0x2510, 0x0405}, // SEQ_DATA_PORT
	{0x2510, 0x2C40}, // SEQ_DATA_PORT
	{0x2510, 0x0AFF}, // SEQ_DATA_PORT
	{0x2510, 0x0A78}, // SEQ_DATA_PORT
	{0x2510, 0x3851}, // SEQ_DATA_PORT
	{0x2510, 0x2A54}, // SEQ_DATA_PORT
	{0x2510, 0x1440}, // SEQ_DATA_PORT
	{0x2510, 0x0015}, // SEQ_DATA_PORT
	{0x2510, 0x0804}, // SEQ_DATA_PORT
	{0x2510, 0x0801}, // SEQ_DATA_PORT
	{0x2510, 0x0408}, // SEQ_DATA_PORT
	{0x2510, 0x2652}, // SEQ_DATA_PORT
	{0x2510, 0x0813}, // SEQ_DATA_PORT
	{0x2510, 0xC810}, // SEQ_DATA_PORT
	{0x2510, 0x0210}, // SEQ_DATA_PORT
	{0x2510, 0x1611}, // SEQ_DATA_PORT
	{0x2510, 0x8111}, // SEQ_DATA_PORT
	{0x2510, 0x8910}, // SEQ_DATA_PORT
	{0x2510, 0x5612}, // SEQ_DATA_PORT
	{0x2510, 0x1009}, // SEQ_DATA_PORT
	{0x2510, 0x020D}, // SEQ_DATA_PORT
	{0x2510, 0x0903}, // SEQ_DATA_PORT
	{0x2510, 0x1402}, // SEQ_DATA_PORT
	{0x2510, 0x15A8}, // SEQ_DATA_PORT
	{0x2510, 0x1388}, // SEQ_DATA_PORT
	{0x2510, 0x0938}, // SEQ_DATA_PORT
	{0x2510, 0x1199}, // SEQ_DATA_PORT
	{0x2510, 0x11D9}, // SEQ_DATA_PORT
	{0x2510, 0x091E}, // SEQ_DATA_PORT
	{0x2510, 0x1214}, // SEQ_DATA_PORT
	{0x2510, 0x10D6}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x1210}, // SEQ_DATA_PORT
	{0x2510, 0x1212}, // SEQ_DATA_PORT
	{0x2510, 0x1210}, // SEQ_DATA_PORT
	{0x2510, 0x11DD}, // SEQ_DATA_PORT
	{0x2510, 0x11D9}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x1441}, // SEQ_DATA_PORT
	{0x2510, 0x0904}, // SEQ_DATA_PORT
	{0x2510, 0x1056}, // SEQ_DATA_PORT
	{0x2510, 0x0811}, // SEQ_DATA_PORT
	{0x2510, 0xDB09}, // SEQ_DATA_PORT
	{0x2510, 0x0311}, // SEQ_DATA_PORT
	{0x2510, 0xFB11}, // SEQ_DATA_PORT
	{0x2510, 0xBB12}, // SEQ_DATA_PORT
	{0x2510, 0x1A12}, // SEQ_DATA_PORT
	{0x2510, 0x1008}, // SEQ_DATA_PORT
	{0x2510, 0x1250}, // SEQ_DATA_PORT
	{0x2510, 0x0B10}, // SEQ_DATA_PORT
	{0x2510, 0x7610}, // SEQ_DATA_PORT
	{0x2510, 0xE614}, // SEQ_DATA_PORT
	{0x2510, 0x6109}, // SEQ_DATA_PORT
	{0x2510, 0x0612}, // SEQ_DATA_PORT
	{0x2510, 0x4012}, // SEQ_DATA_PORT
	{0x2510, 0x6009}, // SEQ_DATA_PORT
	{0x2510, 0x1C14}, // SEQ_DATA_PORT
	{0x2510, 0x6009}, // SEQ_DATA_PORT
	{0x2510, 0x1215}, // SEQ_DATA_PORT
	{0x2510, 0xC813}, // SEQ_DATA_PORT
	{0x2510, 0xC808}, // SEQ_DATA_PORT
	{0x2510, 0x1066}, // SEQ_DATA_PORT
	{0x2510, 0x090B}, // SEQ_DATA_PORT
	{0x2510, 0x1588}, // SEQ_DATA_PORT
	{0x2510, 0x1388}, // SEQ_DATA_PORT
	{0x2510, 0x0913}, // SEQ_DATA_PORT
	{0x2510, 0x0C14}, // SEQ_DATA_PORT
	{0x2510, 0x4009}, // SEQ_DATA_PORT
	{0x2510, 0x0310}, // SEQ_DATA_PORT
	{0x2510, 0xE611}, // SEQ_DATA_PORT
	{0x2510, 0xFB12}, // SEQ_DATA_PORT
	{0x2510, 0x6212}, // SEQ_DATA_PORT
	{0x2510, 0x6011}, // SEQ_DATA_PORT
	{0x2510, 0xFF11}, // SEQ_DATA_PORT
	{0x2510, 0xFB14}, // SEQ_DATA_PORT
	{0x2510, 0x4109}, // SEQ_DATA_PORT
	{0x2510, 0x0210}, // SEQ_DATA_PORT
	{0x2510, 0x6609}, // SEQ_DATA_PORT
	{0x2510, 0x1211}, // SEQ_DATA_PORT
	{0x2510, 0xBB12}, // SEQ_DATA_PORT
	{0x2510, 0x6312}, // SEQ_DATA_PORT
	{0x2510, 0x6014}, // SEQ_DATA_PORT
	{0x2510, 0x0015}, // SEQ_DATA_PORT
	{0x2510, 0x1811}, // SEQ_DATA_PORT
	{0x2510, 0xB812}, // SEQ_DATA_PORT
	{0x2510, 0xA012}, // SEQ_DATA_PORT
	{0x2510, 0x0010}, // SEQ_DATA_PORT
	{0x2510, 0x2610}, // SEQ_DATA_PORT
	{0x2510, 0x0013}, // SEQ_DATA_PORT
	{0x2510, 0x0011}, // SEQ_DATA_PORT
	{0x2510, 0x0030}, // SEQ_DATA_PORT
	{0x2510, 0x5342}, // SEQ_DATA_PORT
	{0x2510, 0x1100}, // SEQ_DATA_PORT
	{0x2510, 0x1002}, // SEQ_DATA_PORT
	{0x2510, 0x1016}, // SEQ_DATA_PORT
	{0x2510, 0x1101}, // SEQ_DATA_PORT
	{0x2510, 0x1109}, // SEQ_DATA_PORT
	{0x2510, 0x1056}, // SEQ_DATA_PORT
	{0x2510, 0x1210}, // SEQ_DATA_PORT
	{0x2510, 0x0D09}, // SEQ_DATA_PORT
	{0x2510, 0x0314}, // SEQ_DATA_PORT
	{0x2510, 0x0214}, // SEQ_DATA_PORT
	{0x2510, 0x4309}, // SEQ_DATA_PORT
	{0x2510, 0x0514}, // SEQ_DATA_PORT
	{0x2510, 0x4009}, // SEQ_DATA_PORT
	{0x2510, 0x0115}, // SEQ_DATA_PORT
	{0x2510, 0xC813}, // SEQ_DATA_PORT
	{0x2510, 0xC809}, // SEQ_DATA_PORT
	{0x2510, 0x1A11}, // SEQ_DATA_PORT
	{0x2510, 0x4909}, // SEQ_DATA_PORT
	{0x2510, 0x0815}, // SEQ_DATA_PORT
	{0x2510, 0x8813}, // SEQ_DATA_PORT
	{0x2510, 0x8809}, // SEQ_DATA_PORT
	{0x2510, 0x1B11}, // SEQ_DATA_PORT
	{0x2510, 0x5909}, // SEQ_DATA_PORT
	{0x2510, 0x0B12}, // SEQ_DATA_PORT
	{0x2510, 0x1409}, // SEQ_DATA_PORT
	{0x2510, 0x0112}, // SEQ_DATA_PORT
	{0x2510, 0x1010}, // SEQ_DATA_PORT
	{0x2510, 0xD612}, // SEQ_DATA_PORT
	{0x2510, 0x1212}, // SEQ_DATA_PORT
	{0x2510, 0x1011}, // SEQ_DATA_PORT
	{0x2510, 0x5D11}, // SEQ_DATA_PORT
	{0x2510, 0x5910}, // SEQ_DATA_PORT
	{0x2510, 0x5609}, // SEQ_DATA_PORT
	{0x2510, 0x0311}, // SEQ_DATA_PORT
	{0x2510, 0x5B08}, // SEQ_DATA_PORT
	{0x2510, 0x1441}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x1440}, // SEQ_DATA_PORT
	{0x2510, 0x090C}, // SEQ_DATA_PORT
	{0x2510, 0x117B}, // SEQ_DATA_PORT
	{0x2510, 0x113B}, // SEQ_DATA_PORT
	{0x2510, 0x121A}, // SEQ_DATA_PORT
	{0x2510, 0x1210}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x1250}, // SEQ_DATA_PORT
	{0x2510, 0x10F6}, // SEQ_DATA_PORT
	{0x2510, 0x10E6}, // SEQ_DATA_PORT
	{0x2510, 0x1460}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x15A8}, // SEQ_DATA_PORT
	{0x2510, 0x13A8}, // SEQ_DATA_PORT
	{0x2510, 0x1240}, // SEQ_DATA_PORT
	{0x2510, 0x1260}, // SEQ_DATA_PORT
	{0x2510, 0x0924}, // SEQ_DATA_PORT
	{0x2510, 0x1588}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x1066}, // SEQ_DATA_PORT
	{0x2510, 0x0B08}, // SEQ_DATA_PORT
	{0x2510, 0x1388}, // SEQ_DATA_PORT
	{0x2510, 0x0925}, // SEQ_DATA_PORT
	{0x2510, 0x0C09}, // SEQ_DATA_PORT
	{0x2510, 0x0214}, // SEQ_DATA_PORT
	{0x2510, 0x4009}, // SEQ_DATA_PORT
	{0x2510, 0x0710}, // SEQ_DATA_PORT
	{0x2510, 0xE612}, // SEQ_DATA_PORT
	{0x2510, 0x6212}, // SEQ_DATA_PORT
	{0x2510, 0x6011}, // SEQ_DATA_PORT
	{0x2510, 0x7F11}, // SEQ_DATA_PORT
	{0x2510, 0x7B10}, // SEQ_DATA_PORT
	{0x2510, 0x6609}, // SEQ_DATA_PORT
	{0x2510, 0x0614}, // SEQ_DATA_PORT
	{0x2510, 0x4109}, // SEQ_DATA_PORT
	{0x2510, 0x0114}, // SEQ_DATA_PORT
	{0x2510, 0x4009}, // SEQ_DATA_PORT
	{0x2510, 0x0D11}, // SEQ_DATA_PORT
	{0x2510, 0x3B12}, // SEQ_DATA_PORT
	{0x2510, 0x6312}, // SEQ_DATA_PORT
	{0x2510, 0x6014}, // SEQ_DATA_PORT
	{0x2510, 0x0015}, // SEQ_DATA_PORT
	{0x2510, 0x1811}, // SEQ_DATA_PORT
	{0x2510, 0x3812}, // SEQ_DATA_PORT
	{0x2510, 0xA012}, // SEQ_DATA_PORT
	{0x2510, 0x0010}, // SEQ_DATA_PORT
	{0x2510, 0x2610}, // SEQ_DATA_PORT
	{0x2510, 0x0013}, // SEQ_DATA_PORT
	{0x2510, 0x0011}, // SEQ_DATA_PORT
	{0x2510, 0x0043}, // SEQ_DATA_PORT
	{0x2510, 0x7A06}, // SEQ_DATA_PORT
	{0x2510, 0x0507}, // SEQ_DATA_PORT
	{0x2510, 0x410E}, // SEQ_DATA_PORT
	{0x2510, 0x0237}, // SEQ_DATA_PORT
	{0x2510, 0x502C}, // SEQ_DATA_PORT
	{0x2510, 0x4414}, // SEQ_DATA_PORT
	{0x2510, 0x4000}, // SEQ_DATA_PORT
	{0x2510, 0x1508}, // SEQ_DATA_PORT
	{0x2510, 0x0408}, // SEQ_DATA_PORT
	{0x2510, 0x0104}, // SEQ_DATA_PORT
	{0x2510, 0x0826}, // SEQ_DATA_PORT
	{0x2510, 0x5508}, // SEQ_DATA_PORT
	{0x2510, 0x13C8}, // SEQ_DATA_PORT
	{0x2510, 0x1002}, // SEQ_DATA_PORT
	{0x2510, 0x1016}, // SEQ_DATA_PORT
	{0x2510, 0x1181}, // SEQ_DATA_PORT
	{0x2510, 0x1189}, // SEQ_DATA_PORT
	{0x2510, 0x1056}, // SEQ_DATA_PORT
	{0x2510, 0x1210}, // SEQ_DATA_PORT
	{0x2510, 0x0902}, // SEQ_DATA_PORT
	{0x2510, 0x0D09}, // SEQ_DATA_PORT
	{0x2510, 0x0314}, // SEQ_DATA_PORT
	{0x2510, 0x0215}, // SEQ_DATA_PORT
	{0x2510, 0xA813}, // SEQ_DATA_PORT
	{0x2510, 0xA814}, // SEQ_DATA_PORT
	{0x2510, 0x0309}, // SEQ_DATA_PORT
	{0x2510, 0x0614}, // SEQ_DATA_PORT
	{0x2510, 0x0209}, // SEQ_DATA_PORT
	{0x2510, 0x1F15}, // SEQ_DATA_PORT
	{0x2510, 0x8813}, // SEQ_DATA_PORT
	{0x2510, 0x8809}, // SEQ_DATA_PORT
	{0x2510, 0x0B11}, // SEQ_DATA_PORT
	{0x2510, 0x9911}, // SEQ_DATA_PORT
	{0x2510, 0xD909}, // SEQ_DATA_PORT
	{0x2510, 0x1E12}, // SEQ_DATA_PORT
	{0x2510, 0x1409}, // SEQ_DATA_PORT
	{0x2510, 0x0312}, // SEQ_DATA_PORT
	{0x2510, 0x1012}, // SEQ_DATA_PORT
	{0x2510, 0x1212}, // SEQ_DATA_PORT
	{0x2510, 0x1011}, // SEQ_DATA_PORT
	{0x2510, 0xDD11}, // SEQ_DATA_PORT
	{0x2510, 0xD909}, // SEQ_DATA_PORT
	{0x2510, 0x0114}, // SEQ_DATA_PORT
	{0x2510, 0x4009}, // SEQ_DATA_PORT
	{0x2510, 0x0711}, // SEQ_DATA_PORT
	{0x2510, 0xDB09}, // SEQ_DATA_PORT
	{0x2510, 0x0311}, // SEQ_DATA_PORT
	{0x2510, 0xFB11}, // SEQ_DATA_PORT
	{0x2510, 0xBB12}, // SEQ_DATA_PORT
	{0x2510, 0x1A12}, // SEQ_DATA_PORT
	{0x2510, 0x1009}, // SEQ_DATA_PORT
	{0x2510, 0x0112}, // SEQ_DATA_PORT
	{0x2510, 0x500B}, // SEQ_DATA_PORT
	{0x2510, 0x1076}, // SEQ_DATA_PORT
	{0x2510, 0x1066}, // SEQ_DATA_PORT
	{0x2510, 0x1460}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x15C8}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x1240}, // SEQ_DATA_PORT
	{0x2510, 0x1260}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x13C8}, // SEQ_DATA_PORT
	{0x2510, 0x0956}, // SEQ_DATA_PORT
	{0x2510, 0x1588}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x0C14}, // SEQ_DATA_PORT
	{0x2510, 0x4009}, // SEQ_DATA_PORT
	{0x2510, 0x0511}, // SEQ_DATA_PORT
	{0x2510, 0xFB12}, // SEQ_DATA_PORT
	{0x2510, 0x6212}, // SEQ_DATA_PORT
	{0x2510, 0x6011}, // SEQ_DATA_PORT
	{0x2510, 0xFF11}, // SEQ_DATA_PORT
	{0x2510, 0xFB09}, // SEQ_DATA_PORT
	{0x2510, 0x1911}, // SEQ_DATA_PORT
	{0x2510, 0xBB12}, // SEQ_DATA_PORT
	{0x2510, 0x6312}, // SEQ_DATA_PORT
	{0x2510, 0x6014}, // SEQ_DATA_PORT
	{0x2510, 0x0015}, // SEQ_DATA_PORT
	{0x2510, 0x1811}, // SEQ_DATA_PORT
	{0x2510, 0xB812}, // SEQ_DATA_PORT
	{0x2510, 0xA012}, // SEQ_DATA_PORT
	{0x2510, 0x0010}, // SEQ_DATA_PORT
	{0x2510, 0x2610}, // SEQ_DATA_PORT
	{0x2510, 0x0013}, // SEQ_DATA_PORT
	{0x2510, 0x0011}, // SEQ_DATA_PORT
	{0x2510, 0x0030}, // SEQ_DATA_PORT
	{0x2510, 0x5345}, // SEQ_DATA_PORT
	{0x2510, 0x1444}, // SEQ_DATA_PORT
	{0x2510, 0x1002}, // SEQ_DATA_PORT
	{0x2510, 0x1016}, // SEQ_DATA_PORT
	{0x2510, 0x1101}, // SEQ_DATA_PORT
	{0x2510, 0x1109}, // SEQ_DATA_PORT
	{0x2510, 0x1056}, // SEQ_DATA_PORT
	{0x2510, 0x1210}, // SEQ_DATA_PORT
	{0x2510, 0x0D09}, // SEQ_DATA_PORT
	{0x2510, 0x0314}, // SEQ_DATA_PORT
	{0x2510, 0x0614}, // SEQ_DATA_PORT
	{0x2510, 0x4709}, // SEQ_DATA_PORT
	{0x2510, 0x0514}, // SEQ_DATA_PORT
	{0x2510, 0x4409}, // SEQ_DATA_PORT
	{0x2510, 0x0115}, // SEQ_DATA_PORT
	{0x2510, 0x9813}, // SEQ_DATA_PORT
	{0x2510, 0x9809}, // SEQ_DATA_PORT
	{0x2510, 0x1A11}, // SEQ_DATA_PORT
	{0x2510, 0x4909}, // SEQ_DATA_PORT
	{0x2510, 0x0815}, // SEQ_DATA_PORT
	{0x2510, 0x8813}, // SEQ_DATA_PORT
	{0x2510, 0x8809}, // SEQ_DATA_PORT
	{0x2510, 0x1B11}, // SEQ_DATA_PORT
	{0x2510, 0x5909}, // SEQ_DATA_PORT
	{0x2510, 0x0B12}, // SEQ_DATA_PORT
	{0x2510, 0x1409}, // SEQ_DATA_PORT
	{0x2510, 0x0112}, // SEQ_DATA_PORT
	{0x2510, 0x1009}, // SEQ_DATA_PORT
	{0x2510, 0x0112}, // SEQ_DATA_PORT
	{0x2510, 0x1212}, // SEQ_DATA_PORT
	{0x2510, 0x1011}, // SEQ_DATA_PORT
	{0x2510, 0x5D11}, // SEQ_DATA_PORT
	{0x2510, 0x5909}, // SEQ_DATA_PORT
	{0x2510, 0x0511}, // SEQ_DATA_PORT
	{0x2510, 0x5B09}, // SEQ_DATA_PORT
	{0x2510, 0x1311}, // SEQ_DATA_PORT
	{0x2510, 0x7B11}, // SEQ_DATA_PORT
	{0x2510, 0x3B12}, // SEQ_DATA_PORT
	{0x2510, 0x1A12}, // SEQ_DATA_PORT
	{0x2510, 0x1009}, // SEQ_DATA_PORT
	{0x2510, 0x0112}, // SEQ_DATA_PORT
	{0x2510, 0x5010}, // SEQ_DATA_PORT
	{0x2510, 0x7610}, // SEQ_DATA_PORT
	{0x2510, 0x6614}, // SEQ_DATA_PORT
	{0x2510, 0x6409}, // SEQ_DATA_PORT
	{0x2510, 0x0115}, // SEQ_DATA_PORT
	{0x2510, 0xA813}, // SEQ_DATA_PORT
	{0x2510, 0xA812}, // SEQ_DATA_PORT
	{0x2510, 0x4012}, // SEQ_DATA_PORT
	{0x2510, 0x6009}, // SEQ_DATA_PORT
	{0x2510, 0x2015}, // SEQ_DATA_PORT
	{0x2510, 0x8809}, // SEQ_DATA_PORT
	{0x2510, 0x020B}, // SEQ_DATA_PORT
	{0x2510, 0x0901}, // SEQ_DATA_PORT
	{0x2510, 0x1388}, // SEQ_DATA_PORT
	{0x2510, 0x0925}, // SEQ_DATA_PORT
	{0x2510, 0x0C09}, // SEQ_DATA_PORT
	{0x2510, 0x0214}, // SEQ_DATA_PORT
	{0x2510, 0x4409}, // SEQ_DATA_PORT
	{0x2510, 0x0912}, // SEQ_DATA_PORT
	{0x2510, 0x6212}, // SEQ_DATA_PORT
	{0x2510, 0x6011}, // SEQ_DATA_PORT
	{0x2510, 0x7F11}, // SEQ_DATA_PORT
	{0x2510, 0x7B09}, // SEQ_DATA_PORT
	{0x2510, 0x1C11}, // SEQ_DATA_PORT
	{0x2510, 0x3B12}, // SEQ_DATA_PORT
	{0x2510, 0x6312}, // SEQ_DATA_PORT
	{0x2510, 0x6014}, // SEQ_DATA_PORT
	{0x2510, 0x0015}, // SEQ_DATA_PORT
	{0x2510, 0x1811}, // SEQ_DATA_PORT
	{0x2510, 0x3812}, // SEQ_DATA_PORT
	{0x2510, 0xA012}, // SEQ_DATA_PORT
	{0x2510, 0x0010}, // SEQ_DATA_PORT
	{0x2510, 0x2610}, // SEQ_DATA_PORT
	{0x2510, 0x0013}, // SEQ_DATA_PORT
	{0x2510, 0x0011}, // SEQ_DATA_PORT
	{0x2510, 0x0008}, // SEQ_DATA_PORT
	{0x2510, 0x7A06}, // SEQ_DATA_PORT
	{0x2510, 0x0508}, // SEQ_DATA_PORT
	{0x2510, 0x070E}, // SEQ_DATA_PORT
	{0x2510, 0x0237}, // SEQ_DATA_PORT
	{0x2510, 0x502C}, // SEQ_DATA_PORT
	{0x2510, 0xFE2A}, // SEQ_DATA_PORT
	{0x2510, 0xFE1A}, // SEQ_DATA_PORT
	{0x2510, 0x2C2C}, // SEQ_DATA_PORT
	{0xff, 200},	//DELAY= 200
	{0x32E6, 0x009A}, // MIN_SUBROW
	{0x322E, 0x258C}, // CLKS_PER_SAMPLE
	////sensor_setup
	{0x32D0, 0x3A02}, // SHUT_RST
	{0x32D2, 0x3508}, // SHUT_TX
	{0x32D4, 0x3702}, // SHUT_DCG
	{0x32D6, 0x3C04}, // SHUT_RST_BOOST
	{0x32DC, 0x370A}, // SHUT_TX_BOOST
	{0x32EA, 0x3CA8}, // SHUT_CTRL
	{0x351E, 0x0000}, // DAC_LD_30_31
	{0x3510, 0x811F}, // DAC_LD_16_17
	{0x1010, 0x0155}, // FINE_INTEGRATION_TIME4_MIN
	{0x3236, 0x00B2}, // FINE_CORRECTION4
	{0x32EA, 0x3C0E}, // SHUT_CTRL
	{0x32EC, 0x7151}, // SHUT_CTRL2
	{0x3116, 0x0001}, // HDR_CONTROL3
	{0x33E2, 0x0000}, // SAMPLE_CTRL
	{0x3088, 0x0400}, // LFM_CTRL
	{0x322A, 0x0039}, // FINE_INTEGRATION_CTRL
	////3-exp HDR Full resolution parallel 50MHz 30fps PLL enable
	{0x302A, 0x0008}, // VT_PIX_CLK_DIV
	{0x302C, 0x0001}, // VT_SYS_CLK_DIV
	{0x302E, 0x0003}, // PRE_PLL_CLK_DIV
	{0x3030, 0x0032}, // PLL_MULTIPLIER
	{0x3036, 0x0008}, // OP_WORD_CLK_DIV
	{0x3038, 0x0001}, // OP_SYS_CLK_DIV
	{0x302C, 0x0001}, // VT_SYS_CLK_DIV
	{0x30B0, 0x980E}, // DIGITAL_TEST
	{0x31DC, 0x1FB8}, // PLL_CONTROL
	////readout mode configuration
	{0x30A2, 0x0001}, // X_ODD_INC_
	{0x30A6, 0x0001}, // Y_ODD_INC_
	{0x3040, 0x0000}, // READ_MODE
	{0x3082, 0x0008}, // OPERATION_MODE_CTRL
	{0x3044, 0x0400}, // DARK_CONTROL
	{0x3180, 0x0080}, // DELTA_DK_CONTROL
	{0x33E4, 0x0080}, // VERT_SHADING_CONTROL
	{0x33E0, 0x0C80}, // TEST_ASIL_ROWS
	////HDR readout mode configuration
	{0x3064, 0x0000}, // SMIA_TEST
	////full res FOV
	{ 0x3004, 0x0020},		//X_ADDR_START = 32
	{ 0x3008, 0x051F},		//X_ADDR_END = 1311
	{ 0x3002, 0x0004},		//Y_ADDR_START = 4
	{ 0x3006, 0x03C7},		//Y_ADDR_END = 967
	{0x3400, 0x0010}, // SCALE_M
	{ 0x3402, 0x0500},		//X_OUTPUT_CONTROL = 1280
	{ 0x3404, 0x03C4},		//Y_OUTPUT_CONTROL = 964
	////3exp 30fps timing and exposure
	{0x3082, 0x0008}, // OPERATION_MODE_CTRL
	{0x30BA, 0x1002}, // DIGITAL_CTRL
	{ 0x300A, 0x04E2},		//FRAME_LENGTH_LINES = 1250
	{ 0x300C, 0x05ca},		//LINE_LENGTH_PCK = 1482
	{0x3042, 0x0000}, // EXTRA_DELAY
	{0x3238, 0x0222}, // EXPOSURE_RATIO
	{ 0x1008, 0x02A1}, 		//FINE_INTEGRATION_TIME_MIN = 673
	{ 0x100C, 0x042D},		//FINE_INTEGRATION_TIME2_MIN = 1069
	{ 0x100E, 0x05B9},		//FINE_INTEGRATION_TIME3_MIN = 1465
	{ 0x1010, 0x0115},		//FINE_INTEGRATION_TIME4_MIN = 277
	{ 0x3012, 0x012C},		//COARSE_INTEGRATION_TIME = 300
	{ 0x3014, 0x05CE},		//FINE_INTEGRATION_TIME = 1486
	{ 0x321E, 0x05CE},		//FINE_INTEGRATION_TIME2 = 1486
	{ 0x3222, 0x05CE},		//FINE_INTEGRATION_TIME3 = 1486
	{ 0x3226, 0x05CE},		//FINE_INTEGRATION_TIME4 = 1486
	{0x32EA, 0x3C0E}, // SHUT_CTRL
	{0x32EC, 0x72A1}, // SHUT_CTRL2
	{0x3362, 0x0000}, // DC_GAIN
	{0x3366, 0xCCCC}, // ANALOG_GAIN
	{0x3364, 0x01CF}, // DCG_TRIM
	{0x3C06, 0x083C}, // CONFIGURE_BUFFERS1
	{0x3C08, 0x0100}, // CONFIGURE_BUFFERS2
	////parallel 20to12bit output
	{0x31D0, 0x0001}, // COMPANDING
	{0x31AE, 0x0201}, // SERIAL_FORMAT
	{0x31AE, 0x0001}, // SERIAL_FORMAT
	{0x31AC, 0x140C}, // DATA_FORMAT_BITS
	{0x301A, 0x10DC}, // RESET_REGISTER
	//
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_set_ser_serialization[] = {
	{0x04, 0x87},
	{REG_TERM, VAL_TERM}
};

/* refer below camera_mode type and make up videosource_reg_table_list

 enum camera_mode {
	 MODE_INIT		   = 0,
	 MODE_SERDES_FSYNC,
	 MODE_SERDES_INTERRUPT,
	 MODE_SERDES_REMOTE_SER,
	 MODE_SERDES_SENSOR,
	 MODE_SERDES_REMOTE_SER_SERIALIZATION,
 };

*/

static struct videosource_reg * videosource_reg_table_list[] = {
	sensor_camera_yuv422_8bit_4ch,
	sensor_camera_enable_frame_sync,
	sensor_camera_enable_interrupt,
	sensor_camera_enable_serializer,
	sensor_camera_init_sensor,
	sensor_camera_set_ser_serialization,
};

/*
 * Helper fuctions for reflection
 */
static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct videosource, hdl)->sd;
}

static inline struct videosource *
vsrcdrv_to_vsrc(struct videosource_driver *drv)
{
	return container_of(drv, struct videosource, driver);
}

static inline struct videosource *sd_to_vsrc(struct v4l2_subdev *sd)
{
	return container_of(sd, struct videosource, sd);
}

/*
 * v4l2_ctrl_ops implementation
 */
static int max9286_isp_s_ctrl(struct v4l2_ctrl *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	default:
		return -EINVAL;
	}

	log("the s_control operation has been called. The feature is not implemented yet.\n");

	return 0;
}

static const struct v4l2_ctrl_ops max9286_isp_ctrl_ops = {
    .s_ctrl = max9286_isp_s_ctrl,
};

/*
 * v4l2_subdev_core_ops implementation
 */

static int max9286_isp_log_status(struct v4l2_subdev *sd)
{
	struct videosource *decoder = sd_to_vsrc(sd);

	/* TODO: look into registers used to see status of the
	 * decoder */
	v4l2_info(sd, "sub-device module is ready.\n");
	v4l2_ctrl_handler_log_status(&(decoder->hdl), sd->name);

	return 0;
}

static int max9286_isp_set_power(struct v4l2_subdev *sd, int on)
{
	struct videosource *decoder = sd_to_vsrc(sd);
	struct videosource_gpio *gpio = &(decoder->gpio);

	if (on) {
		sensor_port_disable(gpio->pwd_port);
		msleep(20);

		sensor_port_enable(gpio->pwd_port);
		sensor_port_enable(gpio->rst_port);
		msleep(20);
	} else {
		sensor_port_disable(gpio->rst_port);
		sensor_port_disable(gpio->pwr_port);
		sensor_port_disable(gpio->pwd_port);
		msleep(5);
	}

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int max9286_isp_g_register(struct v4l2_subdev *sd,
			       struct v4l2_dbg_register *reg)
{
	reg->val = max9286_isp_read(sd, reg->reg & 0xff);
	reg->size = 1;
	return 0;
}

static int max9286_isp_s_register(struct v4l2_subdev *sd,
			       const struct v4l2_dbg_register *reg)
{
	max9286_isp_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif//CONFIG_VIDEO_ADV_DEBUG

static const struct v4l2_subdev_core_ops max9286_isp_core_ops = {
    .log_status = max9286_isp_log_status,
    .s_power = max9286_isp_set_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
    .g_register = max9286_isp_g_register,
    .s_register = max9286_isp_s_register,
#endif//CONFIG_VIDEO_ADV_DEBUG
};

/*
 * v4l2_subdev_video_ops implementation
 */

/**
 * return current video standard
 */
static int max9286_isp_g_std(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	const struct videosource *vsrc = sd_to_vsrc(sd);

	if (!vsrc->current_std) {
		printk(KERN_ERR "%s - Current standard is not set\n", __func__);
		return -EINVAL;
	}

	// std = vsrc->current_std;
	return 0;
}

static int max9286_isp_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	struct i2c_client *client = vsrc->driver.get_i2c_client(vsrc);
	int idx = 0;

	return 0;
}

/**
 * @brief query standard whether the standard is supported in the sub-device
 *
 * @param sd v4l2 sub-device object
 * @param std v4l2 standard id
 * @return 0 on supported, -EINVAL on not supported
 */
static int max9286_isp_querystd(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	int reg, ret;

	v4l2_dbg(1, 1, sd, "Current STD: %s\n", vsrc->current_std->name);

	return ret;
}

/**
 * @brief get all standards supported by the video CAPTURE device. For
 * subdevice, only NTSC format would be supported by the device code.
 *
 * @param sd v4l2 subdevice object
 * @param std v4l2 standard object
 * @return always 0
 */
static int max9286_isp_g_tvnorms(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	int idx;

	// TODO An array of v4l2_std_id passed by arguments would be just a
	// pointer now, assume that fixed-size of arrays used for the argument.
	// This would be checked later.

	for (idx = 0; idx < vsrc->num_stds; idx++) {
		std[idx] = vsrc->std_list[idx].standard.id;
	}

	return 0;
}

/**
 * @brief get decoder status (using status-1 register)
 *
 * @param sd v4l2 subdevice object
 * @param status 0: "video not present", 1: "video detected"
 * @return -1 on error, 0 on otherwise
 */
static int max9286_isp_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	int reg;

	return 0;
}

extern int tcc_mipi_csi2_enable(unsigned int idx, videosource_format_t * format, unsigned int enable);

static int max9286_isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct videosource *vsrc = NULL;
	struct i2c_client *client = NULL;
	int ret = 0;

	printk("%s invoked\n", __func__);

	vsrc = sd_to_vsrc(sd);
	if (!vsrc) {
		printk(KERN_ERR
		       "%s - Failed to get videosource object by subdev\n",
		       __func__);
		return -EINVAL;
	}

	client = v4l2_get_subdevdata(sd);

	tcc_mipi_csi2_enable(vsrc->mipi_csi2_port,\
		&vsrc->format, \
		1);

	printk(KERN_DEBUG "name: %s, addr: 0x%x, client: 0x%p\n", client->name, (client->addr)<<1, client);
	ret = change_mode(client, MODE_INIT);

	// init remote serializer
	change_mode(client, MODE_SERDES_REMOTE_SER);

	change_mode(client, MODE_SERDES_SENSOR);

	// enable deserializer frame sync
	change_mode(client, MODE_SERDES_FSYNC);
	
	change_mode(client, MODE_SERDES_REMOTE_SER_SERIALIZATION);

	vsrc->enabled = enable;

	return 0;
}

static int
max9286_isp_g_frame_interval(struct v4l2_subdev *sd,
			  struct v4l2_subdev_frame_interval *interval)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	return vsrc->format.framerate;
}

static const struct v4l2_subdev_video_ops max9286_isp_video_ops = {
	.g_std = max9286_isp_g_std,
	.s_std = max9286_isp_s_std,
	.querystd = max9286_isp_querystd,
	.g_tvnorms = max9286_isp_g_tvnorms,
	.g_input_status = max9286_isp_g_input_status,
	.s_stream = max9286_isp_s_stream,
	.g_frame_interval = max9286_isp_g_frame_interval,
};

static const struct v4l2_subdev_ops max9286_isp_ops = {
    .core = &max9286_isp_core_ops,
    .video = &max9286_isp_video_ops,
};

static int change_mode(struct i2c_client *client, int mode)
{
	int entry = sizeof(videosource_reg_table_list) /
		    sizeof(videosource_reg_table_list[0]);
	int ret = 0;

	if ((entry <= 0) || (mode < 0) || (entry <= mode)) {
		printk("The list(%d) or the mode index(%d) is wrong\n", entry,
		     mode);
		return -1;
	}

	ret = max9286_isp_write_regs(client, videosource_reg_table_list[mode], mode);

	return ret;
}

/*
 * check whether i2c device is connected
 * 0x1e: ID register
 * max9286_isp device identifier: 0x40
 * return: 0: device is not connected, 1: connected
 */
static int check_status(struct i2c_client * client) {
	unsigned char reg[1] = { 0x1e };
	unsigned char val = 0;
	int ret = 0;

	ret = DDI_I2C_Read(client, reg, 1, &val, 1);
	if(ret) {
		printk("ERROR: Sensor I2C !!!! \n");
		return -1;
	}

	return ((val == 0x40) ? 1 : 0);	// 0: device is not connected, 1: connected
}

static irqreturn_t irq_handler(int irq, void * client_data) {
	return IRQ_WAKE_THREAD;
}

static irqreturn_t irq_thread_handler(int irq, void * client_data) {
	return IRQ_HANDLED;
}

static int set_irq_handler(videosource_gpio_t * gpio, unsigned int enable) {
	return 0;
}

static int set_i2c_client(struct videosource *vsrc, struct i2c_client *client)
{
	int ret = 0;

	struct v4l2_subdev *sd;
	if (!vsrc) {
		loge("Failed to get max9286_isp videosource. Something wrong with a timing of initialization.\n");
		ret = -1;
	}

	sd = &(vsrc->sd);
	if (!sd) {
		loge("Failed to get sub-device object of max9286_isp.\n");
		ret = -1;
	}

	// Store i2c_client object as a private data of subdevice
	if (ret != -1) {
		v4l2_set_subdevdata(sd, client);
	}

	return ret;
}

static struct i2c_client *get_i2c_client(struct videosource *vsrc)
{
	return (struct i2c_client *)v4l2_get_subdevdata(&(vsrc->sd));
}

static int subdev_init(struct videosource *vsrc)
{
	int ret = 0, err;

	struct v4l2_subdev *sd = &(vsrc->sd);

	// check i2c_client is initialized properly
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!client) {
		printk(KERN_ERR
		       "%s - i2c_client is not initialized yet. Failed to "
		       "register the device as a sub-device\n", __func__);
		ret = -1;
	} else {
		// Register with V4L2 layer as a slave device
		v4l2_i2c_subdev_init(sd, client, &max9286_isp_ops);
		/* v4l2_device_register(sd->dev, sd->v4l2_dev); */
		err = v4l2_async_register_subdev(sd);
		if (err) {
			loge("Failed to register subdevice\n");
		} else {
			loge("sub-device is initialized within v4l standard: %s.\n", sd->name);
		}
	}

	return ret;
}

struct videosource videosource_max9286_isp = {
    .type = VIDEOSOURCE_TYPE_MIPI,
    .format =
	{
	    // deprecated
	    .width = WIDTH,
	    .height = HEIGHT,		      // - 1,
	    .interlaced = V4L2_DV_PROGRESSIVE,	// V4L2_DV_INTERLACED
	    .data_format = FMT_YUV422_16BIT,   // data format
	    .gen_field_en = OFF,
	    .de_active_low = ACT_LOW,
	    .field_bfield_low = OFF,
	    .vs_mask = OFF,
	    .hsde_connect_en = OFF,
	    .intpl_en = OFF,
	    .conv_en = OFF, // OFF: BT.601 / ON: BT.656
	    .se = ON,
	    .fvs = ON,
	    .polarities = V4L2_DV_VSYNC_POS_POL,

	    // remain
	    .crop_x = 0,
	    .crop_y = 0,
	    .crop_w = 0,
	    .crop_h = 0,
	    .data_order = ORDER_RGB,
	    .bit_per_pixel = 8, // bit per pixel

	    .capture_w = WIDTH,
	    .capture_h = HEIGHT, // - 1,

	    .capture_zoom_offset_x = 0,
	    .capture_zoom_offset_y = 0,
	    .cam_capchg_width = WIDTH,
	    .framerate = 30,
	    .capture_skip_frame = 0,
	    .sensor_sizes = sensor_sizes,

	    .des_info.input_ch_num	= 1,
	    .des_info.pixel_mode		= PIXEL_MODE_SINGLE,
	    .des_info.interleave_mode	= INTERLEAVE_MODE_VC_DT,
	    .des_info.data_lane_num 	= 4,
	    .des_info.data_format		= DATA_FORMAT_RAW12,
	    .des_info.hssettle		= 0x02,
	    .des_info.clksettlectl		= 0x00,
	},
    .driver =
	{
	    .name = "max9286_isp",
	    .change_mode = change_mode,
		.set_irq_handler = set_irq_handler,
	    .check_status = check_status,
	    .set_i2c_client = set_i2c_client,
	    .get_i2c_client = get_i2c_client,
	    .subdev_init = subdev_init,
	},
    .fmt_list = max9286_isp_fmt_list,
    .num_fmts = ARRAY_SIZE(max9286_isp_fmt_list),
    .pix =
	{
	    .width = NTSC_NUM_ACTIVE_PIXELS,
	    .height = NTSC_NUM_ACTIVE_LINES,
	    .pixelformat = V4L2_PIX_FMT_YUV422P,
	    .field = V4L2_FIELD_INTERLACED,
	    .bytesperline = NTSC_NUM_ACTIVE_LINES * 2,
	    .sizeimage = NTSC_NUM_ACTIVE_PIXELS * 2 * NTSC_NUM_ACTIVE_LINES,
	    .colorspace = V4L2_COLORSPACE_SMPTE170M,
	},
    .current_std = STD_NTSC_M,
    .std_list = max9286_isp_std_list,
    .num_stds = ARRAY_SIZE(max9286_isp_std_list),
};
