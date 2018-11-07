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

//#include <asm/system.h>
#include <video/tcc/tcc_hdmi_in_parameters.h>
#include "../tcc_hdin_ctrl.h"
#include "../tcc_hdin_i2c.h"
#include "ep9553t.h"


static int debug_video	   = 0;
static int debug_sound	   = 0;
#define vprintk(msg...)	if(debug_video) { printk( "ep9553t(Video): " msg); }
#define sprintk(msg...)	if(debug_sound) { printk( "ep9553t(Sound): " msg); }

unsigned char tcc_get_ep9553e_vin_size(struct tcc_hdin_device *vdev)
{
	
	struct tcc_hdin_device *dev = vdev;
	unsigned char  V_TIME   =	0x00;
	unsigned short reg      =	0x0301; 	// V-TIME : Decoded Video Timing Register

	dev->hdin_interlaced_mode = false; //default : Progressive, true = Interlaced, false = Progressive

	dev->input_width = 0;
	dev->input_height = 0;

	DDI_I2C_Read_HDMI_IN(reg, 2, &V_TIME, 1);
	vprintk("read register 0x%04x, Value is 0x%02x \n",reg,V_TIME);
	
	if(V_TIME == 0x01){

		reg      =	0x0600;
		DDI_I2C_Read_HDMI_IN(reg, 2, &V_TIME, 1);
		vprintk("read register 0x%04x, Value is 0x%02x \n",reg,V_TIME);
		
		if(V_TIME == 0x02){

			reg      =	0x0601;
			DDI_I2C_Read_HDMI_IN(reg, 2, &V_TIME, 1);
			vprintk("read register 0x%04x, Value is 0x%02x \n",reg,V_TIME);
			
			if(V_TIME == 0x80)	V_TIME = 0x01;
			else				V_TIME = 0x00;
			
		}
		else{
			V_TIME = 0x00;
		}
	}

	//CEA-861D
	switch(V_TIME){

		// 640 x 480
		case 0x01:
			dev->input_width = 640;
			dev->input_height = 480;
			V_TIME = SIZE_640_480P;
			break;

		// 720 x 480
		case 0x30:	case 0x31:	case 0x02:
		case 0x03:	case 0x38:	case 0x39:
			dev->input_width = 720;
			dev->input_height = 480;
			V_TIME = SIZE_720_480P;
			break;
		case 0x06:	case 0x07:
			dev->hdin_interlaced_mode = true;
			dev->input_width = 720;
			dev->input_height = 240;
			V_TIME = SIZE_720_480I;
			break;

		// 720 x 576
		case 0x11:	case 0x12:	case 0x2A:
		case 0x2B:	case 0x34:	case 0x35:
			dev->input_width = 720;
			dev->input_height = 576;
			V_TIME = SIZE_720_576P;
			break;

		//1440 x 480
		case 0x32:	case 0x33:
		case 0x3A:	case 0x3B: 
			dev->hdin_interlaced_mode = true;
			dev->input_width = 1440;
			dev->input_height = 240;
			V_TIME = SIZE_1440_480I;
			break;
		case 0x0E:	case 0x0F:	
			dev->input_width = 1440;
			dev->input_height = 480;
			V_TIME = SIZE_1440_480P;
			break;

		// 1440 x 240
		case 0x08:			
		case 0x09:		
			dev->input_width = 1440;
			dev->input_height = 240;
			V_TIME = SIZE_1440_240P;
			break;

		// 1440 x 576
		case 0x15:	case 0x16:	case 0x2C:
		case 0x2D:	case 0x36:	case 0x37:
			dev->hdin_interlaced_mode = true;
			dev->input_width = 1440;
			dev->input_height = 288;
			V_TIME = SIZE_1440_576I;
			break;
		case 0x1D:
		case 0x1E:
			dev->input_width = 1440;
			dev->input_height = 576;
			V_TIME = SIZE_1440_576P;			
			break;

		// 1440 x 288
		case 0x17:	case 0x18:		
		case 0x1B:	case 0x1C:	
			dev->input_width = 1440;
			dev->input_height = 288;
			V_TIME = SIZE_1440_288P;			
			break;

		// 1280 x 720
		case 0x04: 	case 0x13: 		
		case 0x29:	case 0x2F:
			dev->input_width = 1280;
			dev->input_height = 720;
			V_TIME = SIZE_1280_720P;					
			break;

		// 1920 x 1080
		case 0x05: 	case 0x14: 	case 0x27: 
		case 0x28: 	case 0x2E:
			dev->hdin_interlaced_mode = true;
			dev->input_width = 1920;
			dev->input_height = 540;
			V_TIME = SIZE_1920_1080I;					
			break;
			
		case 0x10: 	case 0x1F: 	case 0x20: 
		case 0x21: 	case 0x22: 
			dev->input_width = 1920;
			dev->input_height = 1080;
			V_TIME = SIZE_1920_1080P;					
			break;

		// 2880 x 480
		case 0x0A: 
		case 0x0B: 
			dev->hdin_interlaced_mode = true;
			dev->input_width = 2880;
			dev->input_height = 240;
			V_TIME = SIZE_2880_480I;								
			break;			
		case 0x23:
		case 0x24:
			dev->input_width = 2880;
			dev->input_height = 480;
			V_TIME = SIZE_2880_480P;				
			break;

		// 2880 x 240
		case 0x0C: 
		case 0x0D: 			
			dev->input_width = 2880;
			dev->input_height = 240;
			V_TIME = SIZE_2880_240P;				
			break;

		// 2880 x 576
		case 0x19:			
		case 0x1A:	
			dev->hdin_interlaced_mode = true;
			dev->input_width = 2880;
			dev->input_height = 288;
			V_TIME = SIZE_2880_576I;				
			break;
		case 0x25:
		case 0x26:
			dev->input_width = 2880;
			dev->input_height = 576;
			V_TIME = SIZE_2880_576P;					
			break;


	}
	
	vprintk("Preview Size is %d x %d \n",dev->input_width,dev->input_height);
	return V_TIME;
}


#if 0 //use static color format "RGB 444"
/*** EP9553E Input Video Format Indicator

4¡¯b_0000	RGB444_LR (Limited Range, ITU601 detected)		
4¡¯b_0100	RGB444_LR (Limited Range, ITU709 detected)		
4¡¯b_0x10	RGB444_FR (Full Range)		
4¡¯b_1000	YUV444 with Color Space ITU601		
4¡¯b_1001	YUV422 with Color Space ITU601		
4¡¯b_1010	YUV444 with Color Space xvYCC601		
4¡¯b_1011	YUV422 with Color Space xvYCC601		
4¡¯b_1100	YUV444 with Color Space ITU709		
4¡¯b_1101	YUV422 with Color Space ITU709		
4¡¯b_1110	YUV444 with Color Space xvYCC709		
4¡¯b_1111	YUV422 with Color Space xvYCC709		

***/
void tcc_get_ep9553e_vin_fmt()
{
	
	unsigned char  VIN_FMT   =	0x00;
	unsigned short reg      =	0x0300; 	// V-TIME : Decoded Video Timing Register
	unsigned char  prv_VIN_FMT = 0x00;
	unsigned int   cnt		=	0;
	
	hdmi_in_vin_yuvmode = false; //default : RGB mode, true : yuv mode, false : RGB mode
	

	
	for(cnt = 0;cnt<10;cnt++){
		DDI_I2C_Read_HDMI_IN(reg, 2, &VIN_FMT, 1);
		vprintk("HDMI IN Video IN register 0x%04x, Value is 0x%02x \n",reg,VIN_FMT);
		
		if(VIN_FMT != prv_VIN_FMT){
			if(VIN_FMT != 0x00)	prv_VIN_FMT = VIN_FMT;
			hdin_ctrl_delay(500);
		} else {
			break;
		}
	}

	
	switch(VIN_FMT){

		case 0x08: // YUV444 with Color Space ITU601	
		case 0x0A: // YUV444 with Color Space xvYCC601
		case 0x0C: // YUV444 with Color Space ITU709	
		case 0x0E: // YUV444 with Color Space xvYCC709
			hdmi_in_vin_yuvmode = true;
		case 0x00:  // RGB444_LR (Limited Range, ITU601 detected)	
		case 0x04:	// RGB444_LR (Limited Range, ITU709 detected)		
		case 0x02: // RGB444_FR (Full Range)	
		case 0x06:
		
			hdmi_in_vin_fmt = FMT_RGB444_24BIT;
			break;
			
			
		case 0x0D: // YUV422 with Color Space ITU709
		case 0x0B: // YUV422 with Color Space xvYCC601
		case 0x09: // YUV422 with Color Space ITU601
		case 0x0F: // YUV422 with Color Space xvYCC709	
			hdmi_in_vin_yuvmode = true;		
			hdmi_in_vin_fmt = FMT_YUV422_16BIT;
			break;
	
		default:
			hdmi_in_vin_fmt = FMT_RGB444_24BIT;
			
	}

}
#endif

int tcc_check_ep9553e_audio_PCM(void)
{
	unsigned char  value	=	0x00;
	unsigned short reg      =	0x0401; 	
	unsigned char  PCM_value	=	0;

	do{
	DDI_I2C_Read_HDMI_IN(reg, 2, &value, 1);
	}while(value == 0xb7);
	
	PCM_value = (value/2)%2;
	
	sprintk("PCM_value = %d\n",PCM_value);

	reg	  = 0x2400;
	
	DDI_I2C_Read_HDMI_IN(reg, 2, &value, 1);

	if(PCM_value == 1) 	value = value & 0xFE;//set XXXX XXX0 : Compressed Data
	else				value =value | 0x01;//set XXXX XXX1 : PCM

	sprintk("%d %x \n",PCM_value,value);
	
	DDI_I2C_Write_HDMI_IN(reg, &value, 2, 1);

	return PCM_value;

}

unsigned char tcc_get_ep9553e_audio_samplerate(void)
{
	unsigned char  value	=	0x00;
	unsigned short reg      =	0x0400; 	

	do{
		DDI_I2C_Read_HDMI_IN(reg, 2, &value, 1);
	}while(value == 0xb7);

	sprintk("Sample rate register 0x%04x, Value is 0x%02x \n",reg,value);

	return value;
}

static int sensor_close(struct file *file)
{
	unsigned char close_value = 0x12;
	unsigned short reg       = 0x2100;
	DDI_I2C_Write_HDMI_IN(reg, &close_value, 2, 1);

	reg = 0x2300;
	close_value = 0x02;
	DDI_I2C_Write_HDMI_IN(reg, &close_value, 2, 1);

	return 0;
}

static int sensor_open(struct file *file)
{
	unsigned char init_value = 0x00;
	unsigned int init_value2 = 0x00;
	unsigned short reg       = 0x00;

	// KeyScan(Mode Change) Register Control(to HDMI mode)
	reg = 0x2100;
	init_value = 0x10; // the register 0x2100 bit1, PD_HDMI, to "0" through IIC.
	DDI_I2C_Write_HDMI_IN(reg, &init_value, 2, 1);
	reg = 0x2300;
	init_value = 0x09; // VOUT FMT : YUV422
	DDI_I2C_Write_HDMI_IN(reg, &init_value, 2, 1);
	reg = 0x2004;
	init_value = 0x00; //replaced with SDI0 for noise. Multi-channel causes noise
	DDI_I2C_Write_HDMI_IN(reg, &init_value, 2, 1);

	hdin_ctrl_delay(5000); // waiting for C BUS connect

	// the register 0x41 bit5 swing for support "MHL Mode"  2013 09 12 swhwang 
	reg = 0x41;
	init_value = 0x2C; 
	DDI_I2C_Write_HDMI_IN(reg, &init_value, 1, 1);
	
	init_value = 0x0C; 
	DDI_I2C_Write_HDMI_IN(reg, &init_value, 1, 1);


    reg = 0x2001;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x2001],Value:[0x%x],Vsync Polarity:[%s],Hsync Polarity:[%s]\n",__func__,
                                                                                     init_value,
                                                                                     ((init_value&0x2)>>1 == 1) ? "Negative":"Positive",
                                                                                     ((init_value&0x1) == 1) ? "Negative":"Positive");
    reg = 0x2002;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x2002],Value:[0x%x],DCLK Polarity:[%s]\n",__func__,
                                                                init_value,
                                                                ((init_value&0x2)>>1 == 1) ? "Rising edge":"Falling edge");
                                                                
    printk("[Hsync Timing------------------------------]\n");
    reg = 0x0600;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0600],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value << 8;
    reg = 0x0601;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0601],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value2 | init_value;
    printk("[%s]APPL = %d\n",__func__,init_value2);

    init_value2 = 0x00;
    reg = 0x0602;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0602],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value << 8;
    reg = 0x0603;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0603],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value2 | init_value;
    printk("[%s]HFP = %d\n",__func__,init_value2);
    
    init_value2 = 0x00;
    reg = 0x0604;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0604],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value << 8;
    reg = 0x0605;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0605],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value2 | init_value;
    printk("[%s]HBP = %d\n",__func__,init_value2);
    
    init_value2 = 0x00;
    reg = 0x0606;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0606],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value << 8;
    reg = 0x0607;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0607],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value2 | init_value;
    printk("[%s]HPW = %d\n\n",__func__,init_value2);
    
    printk("[Vsync Timing------------------------------]\n");
    init_value2 = 0x00;
    reg = 0x0700;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0700],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value << 8;
    reg = 0x0701;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0701],Value:[0x%02x]\n",__func__,init_value);
    init_value2 = init_value2 | init_value;
    printk("[%s]ALPF = %d\n",__func__,init_value2);
    
    reg = 0x0702;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0702],Value:[0x%02x]\n",__func__,init_value);
    printk("[%s]VFP = %d\n",__func__,init_value);	
	
    reg = 0x0703;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0703],Value:[0x%02x]\n",__func__,init_value);
    printk("[%s]VBP = %d\n",__func__,init_value);		

    reg = 0x0704;
    DDI_I2C_Read_HDMI_IN(reg, 2, &init_value, 1);
    printk("[%s]Reg:[0x0704],Value:[0x%02x]\n",__func__,init_value);
    printk("[%s]INTL = %d\n",__func__,(init_value&0x10) >> 7);
    printk("[%s]VPW = %d\n\n",__func__,init_value&0x7F);
	
	return 0;
}

static int sensor_get_VideoSize(struct file *file)
{
	struct tcc_hdin_device *dev = video_drvdata(file);
	
	return tcc_get_ep9553e_vin_size(dev);
}

static int sensor_get_AudioSR(void)
{
	return tcc_get_ep9553e_audio_samplerate();
}

static int sensor_get_AudioType(void)
{
	return tcc_check_ep9553e_audio_PCM();;
}

/**********************************************************
*    Function  
**********************************************************/
void module_init_fnc(MODULE_FUNC_TYPE *sensor_func)
{
	sensor_func->Open 			= sensor_open;
	sensor_func->Close 		= sensor_close;
	sensor_func->GetVideoSize	= sensor_get_VideoSize;
	sensor_func->GetAudioSR	= sensor_get_AudioSR;
	sensor_func->GetAudioType	= sensor_get_AudioType;

}


