/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

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
#define EDID_LENGTH 128

#define SINK_ACCEPT_YCC420              1
#define SINK_LIMITED_TO_YCC420          2

#define CONFIG_HDMI_YCC420_PREFERRED
#define PRINT_EDID(...) do { if(hdmi_print_log) { pr_info(__VA_ARGS__); } } while(0);

#define EDID_LENGTH 128
#define EDID_4BLOCK_LENGTH      512

struct est_timings {
        u8 t1;
        u8 t2;
        u8 mfg_rsvd;
} __attribute__((packed));

struct std_timing {
        u8 hsize; /* need to multiply by 8 then add 248 */
        u8 vfreq_aspect;
} __attribute__((packed));

/* If detailed data is pixel timing */
struct detailed_pixel_timing {
        u8 hactive_lo;
        u8 hblank_lo;
        u8 hactive_hblank_hi;
        u8 vactive_lo;
        u8 vblank_lo;
        u8 vactive_vblank_hi;
        u8 hsync_offset_lo;
        u8 hsync_pulse_width_lo;
        u8 vsync_offset_pulse_width_lo;
        u8 hsync_vsync_offset_pulse_width_hi;
        u8 width_mm_lo;
        u8 height_mm_lo;
        u8 width_height_mm_hi;
        u8 hborder;
        u8 vborder;
        u8 misc;
} __attribute__((packed));

/* If it's not pixel timing, it'll be one of the below */
struct detailed_data_string {
        u8 str[13];
} __attribute__((packed));

struct detailed_data_monitor_range {
        u8 min_vfreq;
        u8 max_vfreq;
        u8 min_hfreq_khz;
        u8 max_hfreq_khz;
        u8 pixel_clock_mhz; /* need to multiply by 10 */
        u8 flags;
        union {
                struct {
                        u8 reserved;
                        u8 hfreq_start_khz; /* need to multiply by 2 */
                        u8 c; /* need to divide by 2 */
                        u16 m;
                        u8 k;
                        u8 j; /* need to divide by 2 */
                } __attribute__((packed)) gtf2;
                struct {
                        u8 version;
                        u8 data1; /* high 6 bits: extra clock resolution */
                        u8 data2; /* plus low 2 of above: max hactive */
                        u8 supported_aspects;
                        u8 flags; /* preferred aspect and blanking support */
                        u8 supported_scalings;
                        u8 preferred_refresh;
                } __attribute__((packed)) cvt;
        } formula;
} __attribute__((packed));

struct detailed_data_wpindex {
        u8 white_yx_lo; /* Lower 2 bits each */
        u8 white_x_hi;
        u8 white_y_hi;
        u8 gamma; /* need to divide by 100 then add 1 */
} __attribute__((packed));

struct detailed_data_color_point {
        u8 windex1;
        u8 wpindex1[3];
        u8 windex2;
        u8 wpindex2[3];
} __attribute__((packed));

struct cvt_timing {
        u8 code[3];
} __attribute__((packed));

struct detailed_non_pixel {
        u8 pad1;
        u8 type; /* ff=serial, fe=string, fd=monitor range, fc=monitor name
                    fb=color point data, fa=standard timing data,
                    f9=undefined, f8=mfg. reserved */
        u8 pad2;
        union {
                struct detailed_data_string str;
                struct detailed_data_monitor_range range;
                struct detailed_data_wpindex color;
                struct std_timing timings[6];
                struct cvt_timing cvt[4];
        } data;
} __attribute__((packed));
struct detailed_timing {
        u16 pixel_clock; /* need to multiply by 10 KHz */
        union {
                struct detailed_pixel_timing pixel_data;
                struct detailed_non_pixel other_data;
        } data;
} __attribute__((packed));

struct edid {
        u8 header[8];
        /* Vendor & product info */
        u8 mfg_id[2];
        u8 prod_code[2];
        u32 serial; /* FIXME: byte order */
        u8 mfg_week;
        u8 mfg_year;
        /* EDID version */
        u8 version;
        u8 revision;
        /* Display info: */
        u8 input;
        u8 width_cm;
        u8 height_cm;
        u8 gamma;
        u8 features;
        /* Color characteristics */
        u8 red_green_lo;
        u8 black_white_lo;
        u8 red_x;
        u8 red_y;
        u8 green_x;
        u8 green_y;
        u8 blue_x;
        u8 blue_y;
        u8 white_x;
        u8 white_y;
        /* Est. timings and mfg rsvd timings*/
        struct est_timings established_timings;
        /* Standard timings 1-8*/
        struct std_timing standard_timings[8];
        /* Detailing timings 1-4 */
        struct detailed_timing detailed_timings[4];
        /* Number of 128 byte ext. blocks */
        u8 extensions;
        /* Checksum */
        u8 checksum;
} __attribute__((packed));


struct shortVideoDesc_t{
        unsigned vic;
        unsigned ycc420;
} ;


struct sink_edid_simple_t{
        int parse_done;
        int edid_scdc_present;

        /**
         * edid raw data - it can store 4block edids */
        unsigned char rawdata[EDID_4BLOCK_LENGTH];
} ;

struct sink_edid {
        int edid_done;
        int edid_hdmi_support;
        int edid_mYcc444Support;
        int edid_mYcc422Support;
        int edid_mYcc420Support;
        int edid_m20Sink;
        int edid_scdc_present;
        int edid_lts_340mcs_scramble;

        unsigned int sink_product_id;
        unsigned int sink_serial;
        int edid_svd_index;
        char sink_manufacturer_name[4];
        struct shortVideoDesc_t edid_mSvd[128];

        unsigned char rawdata[EDID_4BLOCK_LENGTH];
};

struct sink_manufacture_list_t{
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
        int ycc420;
        int supported;
};

static struct sink_edid  sink_edid;

/*
 * In general, HDCP Keepout is set to 1 when TMDS frequencyrk is higher than
 * 340 MHz or when HDCP is enabled.
 * When HDCP Keepout is set to 1, the control period configuration is changed.
 * Exceptionally, if HDCP keepout is set to 0 for VIZIO TV, there is a problem
 * of swinging HPD.
 * hdmi driver reads the EDID of the SINK and sets HDCP keepout to always 1
 * if this SINK is a VIZIO TV. */
static struct sink_manufacture_list_t sink_manufacture_list[] = {
        {{"VIZ"}}, /* VIZIO TV */
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

static int _edid_checksum(u8 * edid)
{
        int i, checksum = 0;

        for(i = 0; i < EDID_LENGTH; i++)
                checksum += edid[i];

        return checksum % 256; //CEA-861 Spec
}

static  int hdmi_read_edid(struct hdmi_tx_dev *dev, void * edid)
{
        int ret;
        const unsigned char header[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
		ret = hdmi_ddc_read(dev, EDID_I2C_ADDR, EDID_I2C_SEGMENT_ADDR, 0, 0, 8, (unsigned char *)edid);
        if(ret < 0){
                //PRINT_EDID("EDID read failed\r\n");
                goto end_process;
        }
        ret = memcmp((unsigned char * ) edid, (unsigned char *) header, sizeof(header));
        if(ret){
                //PRINT_EDID("EDID header check failed\r\n");
                ret = -1;
                goto end_process;
        }

        ret = hdmi_ddc_read(dev, EDID_I2C_ADDR, EDID_I2C_SEGMENT_ADDR, 0, 8, 120, (unsigned char *)(edid+8));
        if(ret < 0){
                //PRINT_EDID("EDID read failed\r\n");
                goto end_process;
        }

        ret = _edid_checksum((unsigned char *) edid);
        if(ret){
                //PRINT_EDID("EDID checksum failed\r\n");
                ret = -1;
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
        /*to incorporate extensions we have to include the following - see VESA E-DDC spec. P 11 */
        unsigned char start_pointer = block / 2; // pointer to segments of 256 bytes
        unsigned char start_address = ((block % 2) * 0x80); //offset in segment; first block 0-127; second 128-255

        ret = hdmi_ddc_read(dev, EDID_I2C_ADDR, EDID_I2C_SEGMENT_ADDR, start_pointer, start_address, 128, edid_ext);

        if(ret < 0){
                PRINT_EDID("EDID extension read failed\r\n");
                goto end_process;
        }

        ret = _edid_checksum(edid_ext);
        if(ret){
                PRINT_EDID("EDID extension checksum failed\r\n");
                ret = -1;
                goto end_process;
        }
end_process:
        return ret;
}


static void edid_parse(struct edid *edid)
{
        unsigned char tmp;
        int width, height, hz, ar = 0;
        int hblank, vblank, interlaced;
        int loop_index, support_index;
        int find_index, find_vic_index[2];

        PRINT_EDID("%s \r\n", __func__);

        if(edid == NULL) {
                PRINT_EDID("%s parameter is NULL\r\n", __func__);
                return;
        }

        /* manufacture properties */
        tmp = edid->mfg_id[0] >> 2;
        if(tmp > 0) {
                sink_edid.sink_manufacturer_name[0] = '@' +tmp;
        }
        tmp = (edid->mfg_id[0] & 3) << 3;
        tmp |= (edid->mfg_id[1] >> 5);
        if(tmp > 0) {
                sink_edid.sink_manufacturer_name[1] = '@' +tmp;
        }
        tmp = (edid->mfg_id[1] & 0x1F);
        if(tmp > 0) {
                sink_edid.sink_manufacturer_name[2] = '@' +tmp;
        }
        sink_edid.sink_product_id = edid->prod_code[0] | (edid->prod_code[1] << 8);
        sink_edid.sink_serial = edid->serial;


        /* Established Timing */
        PRINT_EDID("%s established timing \r\n", __func__);
        if(edid->established_timings.t1 & (1 << 5)) {
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
                if(edid->standard_timings[loop_index].hsize == 0) {
                        continue;
                }
                if(edid->standard_timings[loop_index].vfreq_aspect == 1 && edid->standard_timings[loop_index].hsize == 1) {
                        continue;
                }
                height = 0;
                width = (edid->standard_timings[loop_index].hsize * 8) + 248;

                /* Value Stored (in binary) = Field Refresh Rate (in Hz) - 60 */
                hz = (edid->standard_timings[loop_index].vfreq_aspect & 0x3F) + 60;

                switch(edid->standard_timings[loop_index].vfreq_aspect >> 6) {
                        case 1:
                                // 4:3
                                height = (3*width)/4;
                                break;

                        case 3:
                                // 16:9
                                height = (9*width)/16;
                                ar = 1;
                                break;
                        default:
                                break;
                }

                if(height > 0) {
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
        }


        /* detailed timings */
        PRINT_EDID("%s detailed timing \r\n", __func__);
        for (loop_index = 0; loop_index < 4; loop_index++) {
                if(edid->detailed_timings[loop_index].pixel_clock > 0){
                        width = (edid->detailed_timings[loop_index].data.pixel_data.hblank_lo |
                                        ((edid->detailed_timings[loop_index].data.pixel_data.vactive_lo & 0xF0) << 4));
                        height = (edid->detailed_timings[loop_index].data.pixel_data.vblank_lo |
                                        ((edid->detailed_timings[loop_index].data.pixel_data.hsync_offset_lo & 0xF0) << 4));
                        hblank = (edid->detailed_timings[loop_index].data.pixel_data.hactive_hblank_hi |
                                        ((edid->detailed_timings[loop_index].data.pixel_data.vactive_lo & 0x0F) << 8));
                        vblank =  (edid->detailed_timings[loop_index].data.pixel_data.vactive_vblank_hi |
                                        ((edid->detailed_timings[loop_index].data.pixel_data.hsync_offset_lo & 0x0F) << 8));
                        interlaced = (edid->detailed_timings[loop_index].data.pixel_data.misc >> 7);

                        find_index = 0;
                        for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                                if(hdmi_supported_list[support_index].pixel_clock == edid->detailed_timings[support_index].pixel_clock &&
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
        sink_edid.edid_done = 1;
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
                result = data[0];
                switch((data[0] >> 0x5) & 0x7) {
                        case 2:
                                length = data[0] & 0x1F;
                                for (loop_index = 1; loop_index < length; loop_index++) {
                                        sink_edid.edid_mSvd[sink_edid.edid_svd_index++].vic = data[loop_index];
                                        //PRINT_EDID("%s vic[%d]\r\n", __func__, data[loop_index]);
                                }
                                break;
                        case 3:
                                if(data[1] == 0x03 && data[2] == 0x0C && data[3] == 0x00) {
                                        sink_edid.edid_hdmi_support = 1;
                                        PRINT_EDID("%s find VSDB\r\n", __func__);
                                }
                                if(data[1] == 0xD8 && data[2] == 0x5D && data[3] == 0xC4) {
                                        sink_edid.edid_hdmi_support = 1;
                                        sink_edid.edid_m20Sink = 1;
                        		sink_edid.edid_scdc_present = ((data[6] >> 7) & 1);
                        		sink_edid.edid_lts_340mcs_scramble = ((data[6] >> 3) & 1);
                                        PRINT_EDID("%s find HF_VSDB\r\n", __func__);
                                }
                                break;
                        case 7:
                                switch(data[1]) {
                                        case 0xe:
                                                        /** If it is a YCC420 VDB then VICs can ONLY be displayed in YCC 4:2:0 */
                                                        /** 7.5.10 YCBCR 4:2:0 Video Data Block */
                                                        /** If Sink has YCC Datablocks it is HDMI 2.0 */
                                                        sink_edid.edid_m20Sink = 1;
                                                        length = data[0] & 0x1F;

                                                        for (loop_index = 0; loop_index < (length - 1); loop_index++) {
                                                                /** Lenght includes the tag byte*/
                                                                int svd_index, svd_applyed = 0;

                                                                for (svd_index = 0;svd_index < sink_edid.edid_svd_index ;svd_index++) {
                                                                        if((sink_edid.edid_mSvd[svd_index].vic & 0x7F) == data[2 + loop_index]) {
                                                                           sink_edid.edid_mSvd[svd_index].ycc420 = SINK_LIMITED_TO_YCC420;
                                                                           svd_applyed = 1;
                                                                           PRINT_EDID("%s vic(%d) limited to ycc420\r\n", __func__, sink_edid.edid_mSvd[svd_index].vic & 0x7F);
                                                                        }
                                                                }
                                                                if(svd_applyed == 0 &&
                                                                        sink_edid.edid_svd_index < 128) {
                                                                        sink_edid.edid_mSvd[sink_edid.edid_svd_index].ycc420 = SINK_LIMITED_TO_YCC420;
                                                                        sink_edid.edid_mSvd[sink_edid.edid_svd_index++].vic = data[2 + loop_index];
                                                                        PRINT_EDID("%s add new vic (%d)\r\n", __func__, data[2 + loop_index]);
                                                                }
                                                        }
                                                        break;

                                                case 0x0f:
                                                        {
                                                                int svd_index, bit_index;
                                                                /** If it is a YCC420 CDB then VIC can ALSO be displayed in YCC 4:2:0 */
                                                                sink_edid.edid_m20Sink = 1;
                                                                length = data[0] & 0x1F;

                                                                if(length > 1) {
                                                                        /* If YCC420 CMDB is bigger than 1, then there is SVD info to parse */
                                                                        for(loop_index = 0; loop_index < (length-1); loop_index++) {
                                                                                for (bit_index = 0; bit_index < 8; bit_index++) {
                                                                                        /** Lenght includes the tag byte*/
                                                                                        if(data[2 + loop_index] & (1 << bit_index)) {
                                                                                                svd_index = bit_index + (8*loop_index);
                                                                                                sink_edid.edid_mSvd[svd_index].ycc420 = SINK_ACCEPT_YCC420;
                                                                                                PRINT_EDID("%s vic(%d) accept to ycc420\r\n", __func__, sink_edid.edid_mSvd[svd_index].vic & 0x7F);
                                                                                        }
                                                                                }
                                                                        }
                                                                } else if(length == 1) {
                                                                        /* Otherwise, all SVDs present at the Video Data Block support YCC420*/
                                                                        for (svd_index = 0;svd_index < sink_edid.edid_svd_index ;svd_index++) {
                                                                                switch(sink_edid.edid_mSvd[svd_index].vic & 0x7F) {
                                                                                        case 96:
                                                                        		case 97:
                                                                        		case 101:
                                                                        		case 102:
                                                                        		case 106:
                                                                        		case 107:
                                                                                                sink_edid.edid_mSvd[svd_index].ycc420 = SINK_ACCEPT_YCC420;
                                                                                                PRINT_EDID("%s vic(%d) accept to ycc420\r\n", __func__, sink_edid.edid_mSvd[svd_index].vic & 0x7F);
                                                                                                break;
                                                                                }
                                                                        }
                                                                }
                                                        }
                                                        break;
                                }
                                break;

                }
        } while(0);

        return  (result >= 0)?((result & 0x1F) + 1):result;
}


static int edid_parser_SimpleParseDataBlock(unsigned char* rawdata, struct sink_edid_simple_t *sink_edid_simple)
{
        unsigned char tag, length = 0;

        do {
                //pr_info("edid_parser_SimpleParseDataBlock");
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
                //pr_info("edid_parser_simple_exension");
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
                //pr_info("edid_parser_simple_exension offset = %d", offset);
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

                //ALOGI("[%s] number_of_extension_blocks = %d", __func__, number_of_extension_blocks);
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

        struct edid *edid_base;
        unsigned char *edid_extension;

        memset(&sink_edid, 0, sizeof(struct sink_edid));
        do {
                edid_base = (struct edid *)sink_edid.rawdata;

                /* Read EDID Base 0-127*/
                do{
                        ret = hdmi_read_edid(dev, edid_base);
                        if(ret == 0) {
                                number_of_extension_blocks = edid_base->extensions;
                                edid_parse(edid_base);
                                if(sink_edid.edid_done) {
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
                        edid_extension = &sink_edid.rawdata[128 + (128 * block)];
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
                                                        sink_edid.edid_mYcc422Support = 1;
                                                        PRINT_EDID("support YCC422\r\n");
                                                }
                                                if(edid_extension[3] & (1 << 5)) {
                                                        sink_edid.edid_mYcc444Support = 1;
                                                        PRINT_EDID("support YCC444\r\n");
                                                }

						for (edid_extension_loop = 4; edid_extension_loop < edid_extension[2];edid_extension_loop += read_bytes) {
                                                        read_bytes = edid_parse_extension(edid_extension + edid_extension_loop);
                                                        if(read_bytes < 0) {
                                                                PRINT_EDID("%s edid_parse_extension failed\r\n", __func__);
                                                                break;
                                                        }
        					}

                                                for(loop_index = 0; loop_index < sink_edid.edid_svd_index ; loop_index++) {
                                                        for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                                                                if(hdmi_supported_list[support_index].vic == (int)((sink_edid.edid_mSvd[loop_index].vic & 0x7F))) {
                                                                        hdmi_supported_list[support_index].supported = 1;
                                                                        hdmi_supported_list[support_index].native = (sink_edid.edid_mSvd[loop_index].vic >> 7);
                                                                        hdmi_supported_list[support_index].ycc420 = sink_edid.edid_mSvd[loop_index].ycc420;

                                                                        PRINT_EDID("vic_%03d %dx%d%s %dhz %s %s\r\n", hdmi_supported_list[support_index].vic,
                                                                                                                hdmi_supported_list[support_index].width,
                                                                                                                hdmi_supported_list[support_index].height,
                                                                                                                hdmi_supported_list[support_index].interlaced?"i":"p",
                                                                                                                hdmi_supported_list[support_index].hz,
                                                                                                                (hdmi_supported_list[support_index].ycc420==SINK_ACCEPT_YCC420)?"ycc420":
                                                                                                                (hdmi_supported_list[support_index].ycc420==SINK_LIMITED_TO_YCC420)?"limited ycc420":"none",
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
                sink_edid.edid_done = 0;
        }
}

static int edid_get_sink_manufacture(void)
{
        int list_loop, list_max;
        unsigned int sink_manufacture = 0;

        do {
                if(!sink_edid.edid_done) {
                        break;
                }

                list_max = sizeof(sink_manufacture_list)/sizeof(struct sink_manufacture_list_t);

                for(list_loop = 0; list_loop < list_max; list_loop++) {
                        if(!memcmp(sink_edid.sink_manufacturer_name, sink_manufacture_list[list_loop].manufacturer_name, 3)) {
                                sink_manufacture = list_loop;
                                break;
                        }
                }
        }while(0);

        return sink_manufacture;
}

void edid_get_manufacturer_info(char* manufacturer_name)
{
        if(manufacturer_name != NULL && sink_edid.edid_done) {
                memcpy(manufacturer_name, sink_edid.sink_manufacturer_name, 4);
        }
}

/*
 * In general, HDCP Keepout is set to 1 when TMDS frequencyrk is higher than
 * 340 MHz or when HDCP is enabled.
 * When HDCP Keepout is set to 1, the control period configuration is changed.
 * Exceptionally, if HDCP keepout is set to 0 for VIZIO TV, there is a problem
 * of swinging HPD.
 * hdmi driver reads the EDID of the SINK and sets HDCP keepout to always 1
 * if this SINK is a VIZIO TV. */
int edid_is_sink_vizio(void)
{
        int sink_manufacture = edid_get_sink_manufacture();
        return (sink_manufacture == 1)?1:0;
}

int edid_get_product_id(void)
{
        return sink_edid.sink_product_id;
}

int  edid_get_serial(void)
{
        return sink_edid.sink_serial;
}

int edid_get_scdc_present(void)
{
	return sink_edid.edid_scdc_present;
}

int edid_get_lts_340mcs_scramble(void)
{
	return sink_edid.edid_lts_340mcs_scramble;
}

int edid_get_hdmi20(void)
{
        return sink_edid.edid_m20Sink;
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
                                if(!memcmp(sink_edid_simple.rawdata, sink_edid.rawdata, EDID_4BLOCK_LENGTH)) {
                                        break;
                                }
        			pr_info(" EDID was unmatched \r\n");
                                hdmi_read_edid_and_parse(dev);
                        }while(retry--);
                }
        }

        hdmi_get_soc_features(dev, &soc_feature);
        if(sink_edid.edid_done) {
                *hdmi_mode = sink_edid.edid_hdmi_support;
                /* find biggest vic */
                for(support_index = 0; (hdmi_supported_list[support_index].vic > 0); support_index++) {
                        if(hdmi_supported_list[support_index].supported) {
                                if(hdmi_supported_list[support_index].pixel_clock > 340000000) {
                                        if(soc_feature.max_tmds_mhz > 0 && soc_feature.max_tmds_mhz <= 340) {
                                                continue;
                                        }
                                        if(sink_edid.edid_scdc_present == 0 && hdmi_supported_list[support_index].ycc420 == 0) {
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
                                        if(sink_edid.edid_scdc_present == 0 && hdmi_supported_list[support_index].ycc420 == 0) {
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
                                        if(!sink_edid.edid_scdc_present) {
                                               *encoding = YCC420;
                                        }
					#if defined(CONFIG_HDMI_YCC420_PREFERRED)
                                        /* The YCC420 is preferred to YCC444 on the output-starter */
                                        if(hdmi_supported_list[optimal_index].ycc420 > 0) {
                                                *encoding = YCC420;
                                        }
					#endif
                                        break;
        		}
                        if(*encoding == RGB) {
                                if(sink_edid.edid_mYcc444Support) {
                                        *encoding = YCC444;
                                } else if(sink_edid.edid_mYcc422Support) {
                                        *encoding = YCC422;
                                }
                        }
                }else {
                         PRINT_EDID("%s hdmi is dvi mode \r\n", __func__);
                }

        }

        return 0;
}



