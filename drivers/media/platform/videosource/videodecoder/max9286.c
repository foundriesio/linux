/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include "../videosource_if.h"
#include "../videosource_i2c.h"
#include "videodecoder.h"
#include "../mipi-csi2/mipi-csi2.h"

//#define CONFIG_MIPI_1_CH_CAMERA
#define CONFIG_INTERNAL_FSYNC

#if defined (CONFIG_MIPI_1_CH_CAMERA) || !defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
#define MAX9286_WIDTH			1280
#else
#define MAX9286_WIDTH			5120
#endif
#define MAX9286_HEIGHT		720

extern volatile void __iomem *  ddicfg_base;
static unsigned int des_irq_num;

struct capture_size sensor_sizes[] = {
	{MAX9286_WIDTH, MAX9286_HEIGHT},
	{MAX9286_WIDTH, MAX9286_HEIGHT}
};

#define SER_ADDR    (0x80 >> 1) 

static struct videosource_reg sensor_camera_yuv422_8bit_4ch[] = {
// max9286 init
// enable 4ch
    {0X0A, 0X0F},    //    Write   Disable all Forward control channel
    {0X34, 0X35},    //    Write   Disable auto acknowledge
    {0X15, 0X83},    //    Write   Select the combined camera line format for CSI2 output
    {0X12, 0XF3},    //    Write   MIPI Output setting
    {0xff, 5},       //    Write   5mS
    {0X63, 0X00},    //    Write   Widows off
    {0X64, 0X00},    //    Write   Widows off
    {0X62, 0X1F},    //    Write   FRSYNC Diff off

#if 1
    {0x01, 0xc0},    //    Write   manual mode
    {0X08, 0X25},    //    Write   FSYNC-period-High
    {0X07, 0XC3},    //    Write   FSYNC-period-Mid
    {0X06, 0XF8},    //    Write   FSYNC-period-Low
#endif
    {0X00, 0XEF},    //    Write   Port 0~3 used
//    {0x0c, 0x91},
    {0xff, 5},       //    Write   5mS
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
    {0X15, 0X0B},    //    Write   Enable MIPI output (line concatenation)
#else
    {0X15, 0X9B},    //    Write   Enable MIPI output (line interleave)
#endif
    {0X69, 0XF0},    //    Write   Auto mask & comabck enable
    {0x01, 0x00},
    {0X0A, 0XFF},    //    Write   All forward channel enable

	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_frame_sync[] = {
    {REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_interrupt[] = {
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_serializer[] = {
                    // Sensor Data Dout7 -> DIN0
    {0x20, 0x07},
    {0x21, 0x06},
    {0x22, 0x05},
    {0x23, 0x04},
    {0x24, 0x03},
    {0x25, 0x02},
    {0x26, 0x01},
    {0x27, 0x00},
    {0x30, 0x17},
    {0x31, 0x16},
    {0x32, 0x15},
    {0x33, 0x14},
    {0x34, 0x13},
    {0x35, 0x12},
    {0x36, 0x11},
    {0x37, 0x10},

    
    {0x43, 0x01},
    {0x44, 0x00},
    {0x45, 0x33},
    {0x46, 0x90},
    {0x47, 0x00},
    {0x48, 0x0C},
    {0x49, 0xE4},
    {0x4A, 0x25},
    {0x4B, 0x83},
    {0x4C, 0x84},
    {0x43, 0x21},
    {0xff, 0xff},
	{REG_TERM, VAL_TERM}
};

/* refer below camera_mode type and make up videosource_reg_table_list

 <videosource_if.h>
 enum camera_mode {
     MODE_INIT         = 0,
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

static int write_regs_max9286(const struct videosource_reg * list, unsigned int mode) {
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
                ret = DDI_I2C_Write_Remote(SER_ADDR, data, 1, 1);
            }
            else {
        		ret = DDI_I2C_Write(data, 1, 1);
            }

    		if(ret) {
    			if(4 <= ++err_cnt) {
    				printk("ERROR: Sensor I2C !!!! \n");
    				return ret;
    			}
    		} else {
#if 0           // for debug
    		    unsigned char value = 0;

				if(mode == MODE_SERDES_REMOTE_SER) {
					DDI_I2C_Read(SER_ADDR, list->reg, 1, &value, 1);
				}
				else {
	    		    DDI_I2C_Read(list->reg, 1, &value, 1);
				}

    		    if(list->val != value)
    		        printk("ERROR : addr(0x%x) write(0x%x) read(0x%x) \n", list->reg, list->val, value);
                else
                    printk("OK : addr(0x%x) write(0x%x) read(0x%x) \n", list->reg, list->val, value);
#endif

#if 0
    			if((unsigned int)list->reg == (unsigned int)0x90) {
    //				printk("%s - delay(1)\n", __func__);
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

irqreturn_t max9286_irq_handler(int irq, void *client_data)
{
	printk("%s\n", __func__);

	return IRQ_WAKE_THREAD;
}

irqreturn_t max9286_irq_thread_handler(int irq, void * client_data) {

    return IRQ_HANDLED;
}

int max9286_set_irq_handler(TCC_SENSOR_INFO_TYPE * sensor_info, unsigned int enable) {
    if(enable) {
        if(0 < sensor_info->gpio.intb_port) {
            des_irq_num = gpio_to_irq(sensor_info->gpio.intb_port);

            printk("des_irq_num : %d \n", des_irq_num);
            
//            if(request_irq(des_irq_num, max9286_irq_handler, IRQF_SHARED | IRQF_TRIGGER_LOW, "max9286", "max9286"))
//            if(request_threaded_irq(des_irq_num, max9286_irq_handler, max9286_irq_thread_handler, IRQF_SHARED | IRQF_TRIGGER_FALLING, "max9286", "max9286")) {
            if(request_threaded_irq(des_irq_num, max9286_irq_handler, max9286_irq_thread_handler, IRQF_TRIGGER_FALLING, "max9286", NULL)) {
                printk("fail request irq(%d) \n", des_irq_num);

                return -1;
            }
        }
    }
    else {
//        free_irq(des_irq_num, "max9286");
        free_irq(des_irq_num, NULL);
    }

    return 0;
}

int max9286_open(void) {
	int ret = 0;

//	printk("%s - %s\n", __func__, MODULE_NODE);

	sensor_port_disable(videosource_info.gpio.pwd_port);
	mdelay(20);

	sensor_port_enable(videosource_info.gpio.pwd_port);
	mdelay(20);

	return ret;
}

int max9286_close(void) {
	sensor_port_disable(videosource_info.gpio.rst_port);
	sensor_port_disable(videosource_info.gpio.pwr_port);
	sensor_port_disable(videosource_info.gpio.pwd_port);

	sensor_delay(5);

	return 0;
}

int max9286_change_mode(int mode) {
	int	entry	= sizeof(videosource_reg_table_list) / sizeof(videosource_reg_table_list[0]);
	int	ret		= 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		printk("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

    ret = write_regs_max9286(videosource_reg_table_list[mode], mode);
    
//	mdelay(10);

	return ret;
}

void sensor_info_init(TCC_SENSOR_INFO_TYPE * sensor_info) {
	sensor_info->width			    = MAX9286_WIDTH;
	sensor_info->height			    = MAX9286_HEIGHT;// - 1;
    sensor_info->crop_x             = 0;
    sensor_info->crop_y             = 0;
	sensor_info->crop_w             = 0;
	sensor_info->crop_h             = 0;
	sensor_info->interlaced			= V4L2_DV_PROGRESSIVE;  //V4L2_DV_INTERLACED;
	sensor_info->polarities			= 0;		            // V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL;
	sensor_info->data_order			= ORDER_RGB;
	sensor_info->data_format		= FMT_YUV422_8BIT;	    // data format
	sensor_info->bit_per_pixel		= 8;			        // bit per pixel
	sensor_info->gen_field_en		= OFF;
	sensor_info->de_active_low		= ACT_LOW;
	sensor_info->field_bfield_low	= OFF;
	sensor_info->vs_mask			= OFF;
	sensor_info->hsde_connect_en    = OFF;
	sensor_info->intpl_en			= OFF;
	sensor_info->conv_en			= OFF;			        // OFF: BT.601 / ON: BT.656

	sensor_info->capture_w			= MAX9286_WIDTH;
	sensor_info->capture_h			= MAX9286_HEIGHT;// - 1;

	sensor_info->capture_zoom_offset_x	= 0;
	sensor_info->capture_zoom_offset_y	= 0;
	sensor_info->cam_capchg_width		= MAX9286_WIDTH;
	sensor_info->framerate			    = 30;
	sensor_info->capture_skip_frame 	= 0;
	sensor_info->sensor_sizes		    = sensor_sizes;

#ifdef CONFIG_MIPI_1_CH_CAMERA
	sensor_info->des_info.input_ch_num      = 1;
#else
	sensor_info->des_info.input_ch_num      = 4;
#endif

	sensor_info->des_info.pixel_mode        = PIXEL_MODE_SINGLE;
	sensor_info->des_info.interleave_mode   = INTERLEAVE_MODE_VC_DT;
	sensor_info->des_info.data_lane_num     = 4;
	sensor_info->des_info.data_format       = DATA_FORMAT_YUV422_8BIT;
	sensor_info->des_info.hssettle          = 0x11;
	sensor_info->des_info.clksettlectl      = 0x00;
}

void sensor_init_fnc(SENSOR_FUNC_TYPE * sensor_func) {
	sensor_func->open			    = max9286_open;
	sensor_func->close			    = max9286_close;
	sensor_func->change_mode	    = max9286_change_mode;
    sensor_func->set_irq_handler    = max9286_set_irq_handler;
}

