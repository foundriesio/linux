/****************************************************************************
Copyright (C) 2018 Telechips Inc.

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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/device.h> // dev_xet_drv_data
#include <asm/io.h>
#include <linux/clk.h>  // clk (example clk_set_rate)
#include <asm/bitops.h> // bit macros


#include <include/hdmi_includes.h>
#include <include/hdmi_ioctls.h>
#include <include/hdmi_access.h>
#include <bsp/i2cm.h>
#include <soc/soc.h>
#include <scdc/scdc.h>
#include <output_starter_hdmi.h>
#include <output_starter_hdmi_edid.h>


// EDID LIB
#define EDID_I2C_ADDR		0x50
#define EDID_I2C_SEGMENT_ADDR	0x30

#define SINK_ACCEPT_YCC420              1
#define SINK_LIMITED_TO_YCC420          2

#define CONFIG_HDMI_YCC420_PREFERRED
#define PRINT_EDID(...) do { if(hdmi_print_log) { pr_info(__VA_ARGS__); } } while(0);

#define EDID_HEADER_LENGTH      8
#define EDID_LENGTH 128
#define EDID_4BLOCK_LENGTH      512

/* YCBCR420 */
#define SINK_ACCEPT_YCBCR420              1
#define SINK_LIMITED_TO_YCBCR420          2

#define HDMI_YCBCR420_PREFERRED

/* EDID YCbCr4xx */
#define EDID_SUPPORT_444        (1 << 0)
#define EDID_SUPPORT_422        (1 << 1)


#pragma pack(push, 1)
struct edid_standard_timing_t {
        unsigned char horizontal;
        unsigned char aspect_ratio_refresh_ratio;
};

struct edid_detailed_timing_t {
        union {
                unsigned char display_descriptor[18];
                struct {
                        unsigned short pixel_clock;
                        unsigned char hactive_lower;
                        unsigned char hblank_lower;
                        unsigned char hactive_hblank_upper;
                        unsigned char vactive_lower;
                        unsigned char vblank_lower;
                        unsigned char vactive_vblank_upper;
                        unsigned char hfront_porch_lower;
                        unsigned char hsync_pulse_width_lower;
                        unsigned char vfront_porch_lower_vsync_plus_width_lower;
                        unsigned char hsync_vsync_offset_pulse_width_upper;
                        /* video image size & boder defines */
                        unsigned char reserved[4];
                        unsigned char reserved_part2;
                };
        }u;
};

struct edid_t {
        union {
                unsigned char raw_data[128];
                struct {
                        unsigned char reserved_1[8]; /* 0-8 */
                        unsigned char manucfacture_name[2]; /* 8-2 */
                        unsigned char product_name[2]; /* 10-2*/
                        unsigned int serial_number; /* 12-4*/
                        unsigned char reserved[19]; /* 16-19 */
                        unsigned char established_timing[3];         /* 35-3 */
                        struct edid_standard_timing_t standard_timing[8]; /* 38-16 */
                        struct edid_detailed_timing_t detailed_timing[4]; /* 54-72 */
                        unsigned char extensions; /* 126 -1 */
                        unsigned char reserved_3;
                };
        }u;
};
#pragma pack(pop)

struct simple_video_t{
        unsigned int vic;
        unsigned int ycbcr420;
};

struct sink_edid_simple_t{
        int parse_done;
        int edid_scdc_present;

        /**
         * edid raw data - it can store 4block edids */
        unsigned char rawdata[EDID_4BLOCK_LENGTH];
} ;

struct edid_info_t {
        int edid_done;
        int edid_hdmi_support;
        int edid_support_ycbcr;
        int edid_sink_is_hdmiv2;
        int edid_scdc_present;
        int edid_lts_340mcs_scramble;
        int edid_gamut_meta_data_support;

        unsigned int sink_product_id;
        unsigned int sink_serial;
        int edid_svd_index;
        char sink_manufacturer_name[4];
        struct simple_video_t edid_simple_video[128];

        unsigned char rawdata[EDID_4BLOCK_LENGTH];
};

struct sink_manufacture_list_t{
       const int manufacture_id;
       const char manufacturer_name[4];
};

struct hdmi_supported_list {
        int vic;
        int pixel_clock;
        int width;
        int height;
        int hz;
        int hblank;
        int vblank;
        int interlaced;
        int no_dvi;

        // from edid
        int native;
        int ycbcr420;
        int supported;
};

static struct edid_info_t  sink_edid_info;

/* In general, HDCP Keepout is set to 1 when TMDS character rate is higher
 * than 340 MHz or when HDCP is enabled.
 * If HDCP Keepout is set to 1 then the control period configuration is changed
 * in order to supports scramble and HDCP encryption. But some SINK needs this
 * packet configuration always even if HDMI ouput is not scrambled or HDCP is
 * not enabled. */
static struct sink_manufacture_list_t sink_manufacture_list[] = {
        {
        	1, "VIZ" /* VIZIO TV */
	},
	{
		2, "SAM" /* SAMSUNG TV */
	},
};

struct hdmi_supported_list hdmi_supported_list[] = {
        {/* v3840x2160p_60Hz */ 97, 594000000, 3840, 2160, 60,  560,  90, 0, 1, 0 ,0 ,0 },
        {/* v3840x2160p_50Hz */ 96, 594000000, 3840, 2160, 50, 1440,  90, 0, 1, 0 ,0 ,0 },
        {/* v3840x2160p_30Hz */ 95, 297000000, 3840, 2160, 30,  560,  90, 0, 1, 0 ,0 ,0 },
        {/* v3840x2160p_25Hz */ 94, 297000000, 3840, 2160, 25, 1440,  90, 0, 1, 0 ,0 ,0 },
        {/* v3840x2160p_24Hz */ 93, 297000000, 3840, 2160, 24, 1660,  90, 0, 1, 0 ,0 ,0 },
        {/* v1920x1080p_60Hz */ 16, 148500000, 1920, 1080, 60,  280,  45, 0, 0, 0 ,0 ,0 },
        {/* v1920x1080p_50Hz */ 31, 148500000, 1920, 1080, 50,  720,  45, 0, 0, 0 ,0 ,0 },
        {/* v1920x1080p_30Hz */ 34,  74250000, 1920, 1080, 30,  280,  45, 0, 0, 0 ,0 ,0 },
        {/* v1920x1080p_24Hz */ 32,  74250000, 1920, 1080, 24,  830,  45, 0, 0, 0 ,0 ,0 },
        {/* v1920x1080i_60Hz */  5,  74250000, 1920, 1080, 60,  280,  22, 1, 0, 0 ,0 ,0 },
        {/* v1920x1080i_50Hz */ 20,  74250000, 1920, 1080, 50,  720,  22, 1, 0, 0 ,0 ,0 },
        {/*  v1280x720p_60Hz */  4,  74250000, 1280,  720, 60,  370,  30, 0, 0, 0 ,0 ,0 },
        {/*  v1280x720p_50Hz */ 19,  74250000, 1280,  720, 50,  700,  30, 0, 0, 0 ,0 ,0 },
        {/*   v720x576p_50Hz */ 17,  27000000,  720,  576, 50,  144,  49, 0, 0, 0 ,0 ,0 },
        {/*   v720x576p_50Hz */ 18,  27000000,  720,  576, 50,  144,  49, 0, 0, 0 ,0 ,0 },
        {/*   v720x480p_60Hz */  2,  27000000,  720,  480, 60,  138,  45, 0, 0, 0 ,0 ,0 },
        {/*   v720x480p_60Hz */  3,  27000000,  720,  480, 60,  138,  45, 0, 0, 0 ,0 ,0 },
        {/*   v640x480p_60Hz */  1,  25175000,  640,  480, 60,  160,  45, 0, 0, 0 ,0 ,0 },
        {                        0,         0,    0,    0,  0,    0,   0, 0, 0, 0 ,0 ,0 },
};

static int hdmi_print_log = 0;

static int hdmi_edid_checksum(unsigned char * edid)
{
        int i;
        unsigned char checksum = 0;

        for(i = 0; i < EDID_LENGTH; i++)
                checksum += edid[i];

        return (checksum & 0xFF);
}

static  int hdmi_read_edid(struct hdmi_tx_dev *dev, unsigned char *edid)
{
        int ret;
        const unsigned char header[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
	ret = hdmi_ddc_read(dev, EDID_I2C_ADDR, EDID_I2C_SEGMENT_ADDR, 0, 0, EDID_HEADER_LENGTH, edid);
        if(ret < 0){
                pr_info("%s failed to read a edid header \r\n", __func__);
                goto end_process;
        }
        ret = memcmp((unsigned char * ) edid, (unsigned char *) header, sizeof(header));
        if(ret){
                pr_info("%s mismatch a edid header \r\n", __func__);
                ret = -1;
                goto end_process;
        }

        ret = hdmi_ddc_read(dev, EDID_I2C_ADDR, EDID_I2C_SEGMENT_ADDR, 0, EDID_HEADER_LENGTH, EDID_LENGTH-EDID_HEADER_LENGTH, edid+EDID_HEADER_LENGTH);
        if(ret < 0){
                pr_info("%s failed to read a edid data \r\n", __func__);
                goto end_process;
        }

        #if 0
        {
                int i;
                for(i = 0; i < EDID_LENGTH ; i++) {
                        printk("%02x", edid[i]);
                        if(!((i+1)%32)) printf("\r\n");
                }
                printk("\r\n");
        }
        #endif

        ret = hdmi_edid_checksum((unsigned char *) edid);
        if(ret){
                ret = -1;
                pr_info("%s mismatch a edid checksum \r\n", __func__);
                goto end_process;
        }
        ret = 0;

end_process:
        if(ret < 0)
                hdmi_i2cddc_bus_clear(dev);

        return ret;
}

static int hdmi_edid_extension_read(struct hdmi_tx_dev *dev, int block, unsigned char * edid_ext)
{
        int ret = -1;
        unsigned char pointer = (block >> 1);
        unsigned char address = ((block % 2) << 7);

        do {
                ret = hdmi_ddc_read(dev, EDID_I2C_ADDR, EDID_I2C_SEGMENT_ADDR, pointer, address, 128, edid_ext);

                if(ret < 0){
                        PRINT_EDID("Failed to read EDID extensions\r\n");
                        break;
                }

                ret = hdmi_edid_checksum(edid_ext);
                if(ret){
                        PRINT_EDID("Failed to checksum of EDID extensions\r\n");
                        ret = -1;
                        break;
                }
        } while(0);

        return ret;
}


static void edid_parse(struct edid_t *edid)
{
        unsigned char tmp;
        int width, height, hz, ar = 0;
        int hblank, vblank, interlaced;
        int loop_index, support_index;
        int find_index, find_vic_index[2];

        int image_aspect_ratio;

        PRINT_EDID("%s \r\n", __func__);

        if(edid == NULL) {
                PRINT_EDID("%s parameter is NULL\r\n", __func__);
                return;
        }

        /* manufacture properties */
        tmp = edid->u.manucfacture_name[0] >> 2;
        sink_edid_info.sink_manufacturer_name[0] = (tmp>0)?('@' +tmp):0;
        tmp = (edid->u.manucfacture_name[0] & 3) << 3;
        tmp |= (edid->u.manucfacture_name[1] >> 5);
        sink_edid_info.sink_manufacturer_name[1] = (tmp>0)?('@' +tmp):0;
        tmp = (edid->u.manucfacture_name[1] & 0x1F);
        sink_edid_info.sink_manufacturer_name[2] = (tmp>0)?('@' +tmp):0;

        sink_edid_info.sink_product_id = edid->u.product_name[0] | (edid->u.product_name[1] << 8);
        sink_edid_info.sink_serial = edid->u.serial_number;


        /* Established Timing - bit 5 is 640x480p60Hz */
        PRINT_EDID("%s established timing \r\n", __func__);
        if(edid->u.established_timing[0] & (1 << 5)) {
                for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                        if(hdmi_supported_list[support_index].vic == 1) {
                                hdmi_supported_list[support_index].supported = 1;
                                PRINT_EDID("Established Timing vic_%03d %dx%d%s %dhz\r\n", hdmi_supported_list[support_index].vic,
                                        hdmi_supported_list[support_index].width,
                                        hdmi_supported_list[support_index].height,
                                        hdmi_supported_list[support_index].interlaced?"i":"p",
                                        hdmi_supported_list[support_index].hz);
                                break;
                        }
                }
        }

        /* Standard Timing */
        PRINT_EDID("%s Standard timing \r\n", __func__);
        for (loop_index = 0; loop_index < 8; loop_index++) {
                if(edid->u.standard_timing[loop_index].horizontal == 0) {
                        continue;
                }
                if(edid->u.standard_timing[loop_index].aspect_ratio_refresh_ratio == 1 && edid->u.standard_timing[loop_index].horizontal == 1) {
                        continue;
                }
                width = (edid->u.standard_timing[loop_index].horizontal << 3) + 248;

                /* Value Stored (in binary) = Field Refresh Rate (in Hz) - 60 */
                hz = (edid->u.standard_timing[loop_index].aspect_ratio_refresh_ratio & 0x3F) - 60;

                image_aspect_ratio = (edid->u.standard_timing[loop_index].aspect_ratio_refresh_ratio >> 6);

                if(image_aspect_ratio == 1) {
                        // 4:3
                        height = (3*width) >> 2;
                } else if (image_aspect_ratio == 3) {
                        // 16:9
                        height = (9*width) >> 4;
                        ar = 1;
                } else {
                        continue;
                }

                find_index = 0;
                for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                        if(hdmi_supported_list[support_index].width == width &&
                           hdmi_supported_list[support_index].height == height &&
                           hdmi_supported_list[support_index].hz == hz) {
                                find_vic_index[find_index++] = support_index;
                        }
                }

                switch(find_index) {
                        case 0:
                        default:
                                support_index = -1;
                                break;
                        case 1:
                                support_index = find_vic_index[0];
                                break;
                        case 2:
                                support_index = find_vic_index[ar];
                                break;
                }
                if(support_index >= 0) {
                        hdmi_supported_list[support_index].supported = 1;
                        PRINT_EDID("Standard Timing vic_%03d %dx%d%s %dhz\r\n", hdmi_supported_list[support_index].vic,
                                                                hdmi_supported_list[support_index].width,
                                                                hdmi_supported_list[support_index].height,
                                                                hdmi_supported_list[support_index].interlaced?"i":"p",
                                                                hdmi_supported_list[support_index].hz);
                }
        }


        /* detailed timings */
        PRINT_EDID("%s detailed timing \r\n", __func__);
        for (loop_index = 0; loop_index < 4; loop_index++) {
                if(edid->u.detailed_timing[loop_index].u.display_descriptor[0] == 0 &&
                        edid->u.detailed_timing[loop_index].u.display_descriptor[1] == 0 &&
                        edid->u.detailed_timing[loop_index].u.display_descriptor[2] == 0) {
                        /* Display Descriptor */
                } else {
                        width = (edid->u.detailed_timing[loop_index].u.hactive_lower |
                                        (edid->u.detailed_timing[loop_index].u.hactive_hblank_upper & 0xF0));
                        height = (edid->u.detailed_timing[loop_index].u.vactive_lower |
                                        ((edid->u.detailed_timing[loop_index].u.vactive_vblank_upper & 0xF0) << 8));
                        hblank = (edid->u.detailed_timing[loop_index].u.hblank_lower |
                                        ((edid->u.detailed_timing[loop_index].u.hactive_hblank_upper& 0x0F) << 8));

                        vblank =  (edid->u.detailed_timing[loop_index].u.vblank_lower |
                                        ((edid->u.detailed_timing[loop_index].u.vactive_vblank_upper & 0x0F) << 8));
                        interlaced = ((edid->u.detailed_timing[loop_index].u.reserved_part2 >> 7) & 1);

                        find_index = 0;
                        for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                                if(hdmi_supported_list[support_index].pixel_clock == edid->u.detailed_timing[support_index].u.pixel_clock &&
                                        hdmi_supported_list[support_index].width == width &&
                                        hdmi_supported_list[support_index].height == height &&
                                        hdmi_supported_list[support_index].hblank == hblank &&
                                        hdmi_supported_list[support_index].vblank == vblank &&
                                        hdmi_supported_list[support_index].interlaced == interlaced) {

                                        find_vic_index[find_index++] = support_index;
                                }
                        }

                        switch(find_index) {
                                case 0:
                                default:
                                        support_index = -1;
                                        break;
                                case 1:
                                        support_index = find_vic_index[0];
                                        break;
                                case 2:
                                        support_index = find_vic_index[ar];
                                        break;
                        }
                        if(support_index >= 0) {
                                hdmi_supported_list[support_index].supported = 1;
                                PRINT_EDID("Detailed Timing vic_%03d %dx%d%s %dhz\r\n", hdmi_supported_list[support_index].vic,
                                                                        hdmi_supported_list[support_index].width,
                                                                        hdmi_supported_list[support_index].height,
                                                                        hdmi_supported_list[support_index].interlaced?"i":"p",
                                                                        hdmi_supported_list[support_index].hz);
                        }

                }
        }
        sink_edid_info.edid_done = 1;
}


static int edid_parse_extension(unsigned char * data)
{
        int length, loop_index;
        int result = -1;

        do {
                if(data == NULL) {
                        PRINT_EDID("%s input paramter is NULL\r\n", __func__);
                        break;
                }
                length = data[0] & 0x1F;
                switch((data[0] >> 0x5) & 0x7) {
                        case 2:
                                for (loop_index = 1; loop_index < length; loop_index++) {
                                        sink_edid_info.edid_simple_video[sink_edid_info.edid_svd_index++].vic = data[loop_index];
                                }
                                break;
                        case 3:
                                if(data[1] == 0x03 && data[2] == 0x0C && data[3] == 0x00) {
                                        sink_edid_info.edid_hdmi_support = 1;
                                        PRINT_EDID("%s find VSDB\r\n", __func__);
                                }
                                if(data[1] == 0xD8 && data[2] == 0x5D && data[3] == 0xC4) {
                                        sink_edid_info.edid_hdmi_support = 1;
                                        sink_edid_info.edid_sink_is_hdmiv2 = 1;
                        		sink_edid_info.edid_scdc_present = ((data[6] >> 7) & 1);
                        		sink_edid_info.edid_lts_340mcs_scramble = ((data[6] >> 3) & 1);
                                        PRINT_EDID("%s find HF_VSDB\r\n", __func__);
                                }
                                break;
                        case 7:
                                switch(data[1]) {
                                        case 0x5:
                                                if(length == 0x3 && data[3] & 0x1) {
                                                        sink_edid_info.edid_gamut_meta_data_support= 1;
                                                }
                                                break;
                                        case 0xe:
                                                sink_edid_info.edid_sink_is_hdmiv2 = 1;
                                                for (loop_index = 0; loop_index < (length - 1); loop_index++) {
                                                        /** Lenght includes the tag byte*/
                                                        int svd_index, svd_applyed = 0;

                                                        for (svd_index = 0;svd_index < sink_edid_info.edid_svd_index ;svd_index++) {
                                                                if((sink_edid_info.edid_simple_video[svd_index].vic & 0x7F) == data[2 + loop_index]) {
                                                                   sink_edid_info.edid_simple_video[svd_index].ycbcr420 = SINK_LIMITED_TO_YCBCR420;
                                                                   svd_applyed = 1;
                                                                   PRINT_EDID("%s vic(%d) limited to ycbcr420\r\n", __func__, sink_edid_info.edid_simple_video[svd_index].vic & 0x7F);
                                                                }
                                                        }
                                                        if(svd_applyed == 0 &&
                                                                sink_edid_info.edid_svd_index < 128) {
                                                                sink_edid_info.edid_simple_video[sink_edid_info.edid_svd_index].ycbcr420 = SINK_LIMITED_TO_YCBCR420;
                                                                sink_edid_info.edid_simple_video[sink_edid_info.edid_svd_index++].vic = data[2 + loop_index];
                                                                PRINT_EDID("%s add new vic (%d)\r\n", __func__, data[2 + loop_index]);
                                                        }
                                                }
                                                break;

                                        case 0x0f:
                                                {
                                                        int svd_index, bit_index;
                                                        sink_edid_info.edid_sink_is_hdmiv2 = 1;
                                                        if(length > 1) {
                                                                for(loop_index = 0; loop_index < (length-1); loop_index++) {
                                                                        for (bit_index = 0; bit_index < 8; bit_index++) {
                                                                                if(data[2 + loop_index] & (1 << bit_index)) {
                                                                                        svd_index = bit_index + (loop_index << 3);
                                                                                        sink_edid_info.edid_simple_video[svd_index].ycbcr420 = SINK_ACCEPT_YCBCR420;
                                                                                        PRINT_EDID("%s vic(%d) accept to ycbcr420\r\n", __func__, sink_edid_info.edid_simple_video[svd_index].vic & 0x7F);
                                                                                }
                                                                        }
                                                                }
                                                        } else if(length == 1) {
                                                                for (svd_index = 0;svd_index < sink_edid_info.edid_svd_index ;svd_index++) {
                                                                        switch(sink_edid_info.edid_simple_video[svd_index].vic & 0x7F) {
                                                                                case 96:
                                                                		case 97:
                                                                		case 101:
                                                                		case 102:
                                                                		case 106:
                                                                		case 107:
                                                                                        sink_edid_info.edid_simple_video[svd_index].ycbcr420 = SINK_ACCEPT_YCBCR420;
                                                                                        PRINT_EDID("%s vic(%d) accept to ycbcr420\r\n", __func__, sink_edid_info.edid_simple_video[svd_index].vic & 0x7F);
                                                                                        break;
                                                                        }
                                                                }
                                                        }
                                                }
                                                break;
                                }
                                break;

                }
                result = length;
        } while(0);

        return  (result >= 0)?(result + 1):result;
}


static int edid_parser_SimpleParseDataBlock(unsigned char* rawdata, struct sink_edid_simple_t *sink_edid_simple)
{
        unsigned char tag, length = 0;

        do {
                if(rawdata == NULL) {
                        pr_info("%s rawdata is NULL\r\n", __func__);
                        break;
                }

                if(sink_edid_simple == NULL) {
                        pr_info("%s sink_edid_simple is NULL\r\n", __func__);
                        break;
                }
                tag = ((rawdata[0] >> 5) & 0x7);
                length = (rawdata[0] & 0x1F);
                if(tag == 0x3) {
                        PRINT_EDID("%s %02x %02x %02x", __func__, rawdata[3], rawdata[2], rawdata[1]);
                        if(rawdata[3] ==  0xC4 && rawdata[2] == 0x5D && rawdata[1] == 0xD8) {
                                sink_edid_simple->edid_scdc_present = ((rawdata[6] >> 7) & 1);
                                sink_edid_simple->parse_done = 1;
                        }
                }
        } while(0);
        return length + 1;
}

static int edid_parser_simple_exension(unsigned char* rawdata, struct sink_edid_simple_t * sink_edid_simple)
{
        int i, offset;
        int ret = -1;

        do {
                if(rawdata == NULL) {
                        pr_info("%s rawdata is NULL", __func__);
                        break;
                }

                if(sink_edid_simple == NULL) {
                        pr_info("%s sink_edid_simple is NULL", __func__);
                        break;
                }

                /* Version */
                if (rawdata[1] < 0x03){
                        pr_info("Invalid version for CEA Extension block, only rev 3 or higher is supported");
                        break;
                }

                /* Offset */
                offset = rawdata[2];
                if (offset > 4) {
                        for (i = 4; i < offset; ) {
                                i += edid_parser_SimpleParseDataBlock(rawdata + i, sink_edid_simple);
                                if(sink_edid_simple->parse_done) {
                                        break;
                                }
                        }
                }
                ret = 0;
        } while(0);

        return ret;
}

static int edid_read_simple(struct hdmi_tx_dev *dev, struct sink_edid_simple_t * sink_edid_simple)
{
        int block;
        int ret = -1;
        int edid_tries = 3;
        int number_of_extension_blocks = 0;

        unsigned char *edid_base;
        unsigned char *edid_extension;

        do {
                if(sink_edid_simple == NULL) {
                        pr_info("%s sink_edid_simple is NULL\r\n", __func__);
                        break;
                }

                /* Clear Sink Data */
                memset(sink_edid_simple, 0, sizeof(struct sink_edid_simple_t));

                edid_base = sink_edid_simple->rawdata;

                /* Read EDID Base 0-127*/
                do{
                        ret = hdmi_read_edid(dev, edid_base);
                        if(ret == 0) {
                                number_of_extension_blocks = edid_base[126];
                                break;
                        }
                }while(edid_tries--);

                if(ret < 0) {
                        pr_info( "[%s] Failed read edid base\r\n", __func__);
                        break;
                }

                /* Support 4block only */
                if(number_of_extension_blocks > 3) {
                        pr_info("EDID limited to 4Block (%d)\r\n", number_of_extension_blocks);
                        number_of_extension_blocks = 3;
                }

                for(block = 0; block < number_of_extension_blocks; block++) {
                        edid_tries = 3;
                        edid_extension = &sink_edid_simple->rawdata[128 + (128 * block)];
                        do{
                                ret = hdmi_edid_extension_read(dev, block+1, edid_extension);
                                if(ret == 0) {
                                        ret = edid_parser_simple_exension(edid_extension, sink_edid_simple);
                                        break;
                                }
                        }while(edid_tries--);

                        if(ret < 0) {
                             pr_info( "[%s] Failed read edid extension [%d] block", __func__, block+1);
                             break;
                        }
                }
        } while(0);
        return ret;
}

static void hdmi_read_edid_and_parse(struct hdmi_tx_dev *dev)
{

        int ret = -1;
        int block = 0;
        int read_bytes;
        int edid_tries = 3;
        int number_of_extension_blocks = 0;
        int loop_index, support_index, edid_extension_loop;

        struct edid_t *edid_base;
        unsigned char *edid_extension;

        /* Clear Sink Data */
        memset(&sink_edid_info, 0, sizeof(struct edid_info_t));
        do {
                edid_base = (struct edid_t *)sink_edid_info.rawdata;

                /* Read EDID Base 0-127*/
                do{
                        ret = hdmi_read_edid(dev, (unsigned char*)edid_base);
                        if(ret == 0) {
                                number_of_extension_blocks = edid_base->u.extensions;
                                edid_parse(edid_base);
                                if(sink_edid_info.edid_done) {
                                        break;
                                } else {
                                        pr_info( "Failed to parse EDID\r\n");
                                        ret = -1;
                                }
                        }
                }while(edid_tries--);

                if(ret < 0) {
                        pr_info( "Failed read edid base\r\n");
                        break;
                }

                /* Support 4block only */
                if(number_of_extension_blocks > 3) {
                        pr_info("EDID limited to 4Block (%d)\r\n", number_of_extension_blocks);
                        number_of_extension_blocks = 3;
                }

                for(block = 0; block < number_of_extension_blocks; block++) {
                        edid_tries = 3;
                        edid_extension = &sink_edid_info.rawdata[128 + (128 * block)];
                        do{
                                ret = hdmi_edid_extension_read(dev, block+1, edid_extension);
                                if(ret == 0) {
                                        /* Check edid extension tag */
                                        if(edid_extension[0] == 2) {
                                                /* Check edid extension version */
        					if (edid_extension[1] < 0x03){
        						continue;
        					}
                                                /* supported color format */
                                                if(edid_extension[3] & (1 << 4)) {
                                                        sink_edid_info.edid_support_ycbcr |= EDID_SUPPORT_422;
                                                        PRINT_EDID("support YCC422\r\n");
                                                }
                                                if(edid_extension[3] & (1 << 5)) {
                                                        sink_edid_info.edid_support_ycbcr |= EDID_SUPPORT_444;
                                                        PRINT_EDID("support YCC444\r\n");
                                                }

						for (edid_extension_loop = 4; edid_extension_loop < edid_extension[2];edid_extension_loop += read_bytes) {
                                                        read_bytes = edid_parse_extension(edid_extension + edid_extension_loop);
                                                        if(read_bytes < 0) {
                                                                PRINT_EDID("%s edid_parse_extension failed\r\n", __func__);
                                                                break;
                                                        }
        					}

                                                for(loop_index = 0; loop_index < sink_edid_info.edid_svd_index ; loop_index++) {
                                                        for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                                                                if(hdmi_supported_list[support_index].vic == (int)((sink_edid_info.edid_simple_video[loop_index].vic & 0x7F))) {
                                                                        hdmi_supported_list[support_index].supported = 1;
                                                                        hdmi_supported_list[support_index].native = (sink_edid_info.edid_simple_video[loop_index].vic >> 7);
                                                                        hdmi_supported_list[support_index].ycbcr420 = sink_edid_info.edid_simple_video[loop_index].ycbcr420;

                                                                        PRINT_EDID("vic_%03d %dx%d%s %dhz %s %s\r\n", hdmi_supported_list[support_index].vic,
                                                                                                                hdmi_supported_list[support_index].width,
                                                                                                                hdmi_supported_list[support_index].height,
                                                                                                                hdmi_supported_list[support_index].interlaced?"i":"p",
                                                                                                                hdmi_supported_list[support_index].hz,
                                                                                                                (hdmi_supported_list[support_index].ycbcr420==SINK_ACCEPT_YCBCR420)?"ycbcr420":
                                                                                                                (hdmi_supported_list[support_index].ycbcr420==SINK_LIMITED_TO_YCBCR420)?"limited ycbcr420":"none",
                                                                                                                hdmi_supported_list[support_index].native?"native":"normal");
                                                                        break;
                                                                }
                                                        }
                                                }

                                                break;
                                        }
                                }
                        }while(edid_tries--);

                        if(ret < 0) {
                             pr_info( "[%s] Failed read and parse EXTENSION Blocks [%d]", __func__, block+1);
                             break;
                        }
                }
        } while(0);

        if(ret < 0) {
                pr_info( "[%s] Failed read edid extension [%d] block", __func__, block);
                sink_edid_info.edid_done = 0;
        }
}

static int edid_get_sink_manufacture(void)
{
        int list_loop, list_max;
        unsigned int sink_manufacture = 0;

        do {
                if(!sink_edid_info.edid_done) {
                        break;
                }

                list_max = sizeof(sink_manufacture_list)/sizeof(struct sink_manufacture_list_t);

                for(list_loop = 0; list_loop < list_max; list_loop++) {
                        if(!memcmp(sink_edid_info.sink_manufacturer_name, sink_manufacture_list[list_loop].manufacturer_name, 3)) {
                                sink_manufacture = sink_manufacture_list[list_loop].manufacture_id;
                                break;
                        }
                }
        }while(0);

        return sink_manufacture;
}

void edid_get_manufacturer_info(char* manufacturer_name)
{
        if(manufacturer_name != NULL && sink_edid_info.edid_done) {
                memcpy(manufacturer_name, sink_edid_info.sink_manufacturer_name, 4);
        }
}

int edid_get_product_id(void)
{
        unsigned int sink_product_id = 0;

	if(sink_edid_info.edid_done) {
		sink_product_id = sink_edid_info.sink_product_id;
	}

        return sink_product_id;
}

int  edid_get_serial(void)
{
	unsigned int sink_serial = 0;

	if(sink_edid_info.edid_done) {
		sink_serial = sink_edid_info.sink_serial;
	}

	return sink_serial;
}


/* In general, HDCP Keepout is set to 1 when TMDS character rate is higher
 * than 340 MHz or when HDCP is enabled.
 * If HDCP Keepout is set to 1 then the control period configuration is changed
 * in order to supports scramble and HDCP encryption. But some SINK needs this
 * packet configuration always even if HDMI ouput is not scrambled or HDCP is
 * not enabled. */
int edid_is_sink_need_hdcp_keepout(void)
{
	int need_hdcp_keepout = 0;

	switch(edid_get_sink_manufacture()) {
		case 1: /* VIZIO */
			need_hdcp_keepout = 1;
			break;
		case 2:
			/* SAMSUNG */
			if(edid_get_product_id() == 0x0F13) {
				need_hdcp_keepout = 1;
			}
			break;
	}
	return need_hdcp_keepout;
}

int edid_get_scdc_present(void)
{
	return sink_edid_info.edid_scdc_present;
}

int edid_get_lts_340mcs_scramble(void)
{
	return sink_edid_info.edid_lts_340mcs_scramble;
}

int edid_get_hdmi20(void)
{
        return sink_edid_info.edid_sink_is_hdmiv2;
}

int edid_get_gamut_meta_data_support(void) {
        return sink_edid_info.edid_gamut_meta_data_support;
}

void edid_set_print_enable(int enable)
{
        hdmi_print_log = enable;
}

int edid_get_optimal_settings(struct hdmi_tx_dev *dev, int *hdmi_mode, int *vic, encoding_t *encoding, int optimal_phase)
{
        int retry = 1;
        int support_index;
        int optimal_index = -1;
        int hotplug_loop = 3;

        hdmi_soc_features soc_feature;
        struct sink_edid_simple_t sink_edid_simple;

        PRINT_EDID("%s\r\n", __func__);
        if(dev == NULL) {
                return -1;
        }

        if(hdmi_mode == NULL || vic == NULL || encoding == NULL) {
                return -1;
        }

        /* default setting - 1280x720p@60Hz RGB HDMI*/
        *hdmi_mode = HDMI;
        *vic = 4;
        *encoding = RGB;

        /* check hdmi hotplug */
        while(hotplug_loop--) {
                if(tcc_hdmi_detect_cable()) {
                        break;
                }
                msleep(100);
        }

        if(hotplug_loop > 0) {
                if(optimal_phase) {
                        hdmi_read_edid_and_parse(dev);
                } else {
			do {
	                        edid_read_simple(dev, &sink_edid_simple);
	                        if(sink_edid_simple.parse_done && sink_edid_simple.edid_scdc_present) {
	                                scdc_write_source_version(dev, 1);
	                                scdc_tmds_config_status(dev);
	                                scdc_set_tmds_bit_clock_ratio_and_scrambling(dev, 0, 0);
	                                scdc_tmds_config_status(dev);
	                                scdc_scrambling_status(dev);
	                        }
	                        if(!memcmp(sink_edid_simple.rawdata, sink_edid_info.rawdata, EDID_4BLOCK_LENGTH)) {
	                                break;
                                }
        			pr_info(" EDID was unmatched \r\n");
                                hdmi_read_edid_and_parse(dev);
                        }while(retry--);
                }
        }

        hdmi_get_soc_features(dev, &soc_feature);
        if(sink_edid_info.edid_done) {
                *hdmi_mode = sink_edid_info.edid_hdmi_support;
                /* find biggest vic */
                for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                        if(hdmi_supported_list[support_index].supported) {
                                if(hdmi_supported_list[support_index].pixel_clock > 340000000) {
                                        if(soc_feature.max_tmds_mhz > 0 && soc_feature.max_tmds_mhz <= 340) {
                                                continue;
                                        }
                                        if(sink_edid_info.edid_scdc_present == 0 && hdmi_supported_list[support_index].ycbcr420 == 0) {
                                                continue;
                                        }
                                }
                                if(*hdmi_mode == DVI && hdmi_supported_list[support_index].no_dvi) {
                                        continue;
                                }
                                optimal_index = support_index;
                                break;
                        }
                }

                /* find native vic */
                for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                        if(hdmi_supported_list[support_index].supported && hdmi_supported_list[support_index].native) {
                                if(hdmi_supported_list[support_index].pixel_clock > 340000000) {
                                        if(soc_feature.max_tmds_mhz > 0 && soc_feature.max_tmds_mhz <= 340) {
                                                continue;
                                        }
                                        if(sink_edid_info.edid_scdc_present == 0 && hdmi_supported_list[support_index].ycbcr420 == 0) {
                                                continue;
                                        }
                                }
                                if(*hdmi_mode == DVI && hdmi_supported_list[support_index].no_dvi) {
                                        continue;
                                }
                                optimal_index = support_index;
                                break;
                        }
                }

        }

        if(optimal_index >= 0) {
                *vic = hdmi_supported_list[optimal_index].vic;
                if(*hdmi_mode == HDMI) /* HDMI */ {
                        PRINT_EDID("%s hdmi is hdmi mode \r\n", __func__);
        		switch(hdmi_supported_list[optimal_index].vic) {
                                case 96:
                                case 97:
                                        if(!sink_edid_info.edid_scdc_present) {
                                               *encoding = YCC420;
                                        }
					#if defined(CONFIG_HDMI_YCC420_PREFERRED)
                                        /* The ycbcr420 is preferred to YCC444 on the bootloader */
                                        if(hdmi_supported_list[optimal_index].ycbcr420 > 0) {
                                                *encoding = YCC420;
                                        }
					#endif
                                        break;
        		}
                        if(*encoding == RGB) {
                                if(sink_edid_info.edid_support_ycbcr & EDID_SUPPORT_444 ) {
                                        *encoding = YCC444;
                                } else if(sink_edid_info.edid_support_ycbcr & EDID_SUPPORT_422 ) {
                                        *encoding = YCC422;
                                }
                        }
                }else {
                         PRINT_EDID("%s hdmi is dvi mode \r\n", __func__);
                }

        }

        return 0;
}



