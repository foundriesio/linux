/*
 * drivers/media/video/ov7670soc.c
 *
 * OmniVision OV7670 cameras driver
 *
 * Copyright (c) 2011 Ming-Yao Chen <mychen0518@gmail.com>
 * (based on tvp514x.c)
 *
 * Copyright (c) 2013 Ant Micro <www.antmicro.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/soc_camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>

/* MODULE NAME*/
#define OV7670SOC_MODULE_NAME "ov7670soc"

/* Private macros for OV7670 */
#define I2C_RETRY_COUNT                (5)
#define VGA_WIDTH                        (640)
#define VGA_HEIGHT                        (480)

/* Registers */
#define REG_GAIN        0x00        /* Gain lower 8 bits (rest in vref) */
#define REG_BLUE        0x01        /* blue gain */
#define REG_RED                0x02        /* red gain */
#define REG_VREF        0x03        /* Pieces of GAIN, VSTART, VSTOP */
#define REG_COM1        0x04        /* Control 1 */
#define  COM1_CCIR656          0x40  /* CCIR656 enable */
#define REG_BAVE        0x05        /* U/B Average level */
#define REG_GbAVE        0x06        /* Y/Gb Average level */
#define REG_AECHH        0x07        /* AEC MS 5 bits */
#define REG_RAVE        0x08        /* V/R Average level */
#define REG_COM2        0x09        /* Control 2 */
#define  COM2_SSLEEP          0x10        /* Soft sleep mode */
#define REG_PID                0x0a        /* Product ID MSB */
#define REG_VER                0x0b        /* Product ID LSB */
#define REG_COM3        0x0c        /* Control 3 */
#define  COM3_SWAP          0x40          /* Byte swap */
#define  COM3_SCALEEN          0x08          /* Enable scaling */
#define  COM3_DCWEN          0x04          /* Enable downsamp/crop/window */
#define REG_COM4        0x0d        /* Control 4 */
#define REG_COM5        0x0e        /* All "reserved" */
#define REG_COM6        0x0f        /* Control 6 */
#define REG_AECH        0x10        /* More bits of AEC value */
#define REG_CLKRC        0x11        /* Clocl control */
#define   CLK_EXT          0x40          /* Use external clock directly */
#define   CLK_SCALE          0x3f          /* Mask for internal clock scale */
#define REG_COM7        0x12        /* Control 7 */
#define   COM7_RESET          0x80          /* Register reset */
#define   COM7_FMT_MASK          0x38
#define   COM7_FMT_VGA          0x00
#define          COM7_FMT_CIF          0x20          /* CIF format */
#define   COM7_FMT_QVGA          0x10          /* QVGA format */
#define   COM7_FMT_QCIF          0x08          /* QCIF format */
#define          COM7_RGB          0x04          /* bits 0 and 2 - RGB format */
#define          COM7_YUV          0x00          /* YUV */
#define          COM7_BAYER          0x01          /* Bayer format */
#define          COM7_PBAYER          0x05          /* "Processed bayer" */
#define REG_COM8        0x13        /* Control 8 */
#define   COM8_FASTAEC          0x80          /* Enable fast AGC/AEC */
#define   COM8_AECSTEP          0x40          /* Unlimited AEC step size */
#define   COM8_BFILT          0x20          /* Band filter enable */
#define   COM8_AGC          0x04          /* Auto gain enable */
#define   COM8_AWB          0x02          /* White balance enable */
#define   COM8_AEC          0x01          /* Auto exposure enable */
#define REG_COM9        0x14        /* Control 9  - gain ceiling */
#define REG_COM10        0x15        /* Control 10 */
#define   COM10_HSYNC          0x40          /* HSYNC instead of HREF */
#define   COM10_PCLK_HB          0x20          /* Suppress PCLK on horiz blank */
#define   COM10_HREF_REV  0x08          /* Reverse HREF */
#define   COM10_VS_LEAD          0x04          /* VSYNC on clock leading edge */
#define   COM10_VS_NEG          0x02          /* VSYNC negative */
#define   COM10_HS_NEG          0x01          /* HSYNC negative */
#define REG_HSTART        0x17        /* Horiz start high bits */
#define REG_HSTOP        0x18        /* Horiz stop high bits */
#define REG_VSTART        0x19        /* Vert start high bits */
#define REG_VSTOP        0x1a        /* Vert stop high bits */
#define REG_PSHFT        0x1b        /* Pixel delay after HREF */
#define REG_MIDH        0x1c        /* Manuf. ID high */
#define REG_MIDL        0x1d        /* Manuf. ID low */
#define REG_MVFP        0x1e        /* Mirror / vflip */
#define   MVFP_MIRROR          0x20          /* Mirror image */
#define   MVFP_FLIP          0x10          /* Vertical flip */
#define REG_AEW                0x24        /* AGC upper limit */
#define REG_AEB                0x25        /* AGC lower limit */
#define REG_VPT                0x26        /* AGC/AEC fast mode op region */
#define REG_HSYST        0x30        /* HSYNC rising edge delay */
#define REG_HSYEN        0x31        /* HSYNC falling edge delay */
#define REG_HREF        0x32        /* HREF pieces */
#define REG_TSLB        0x3a        /* lots of stuff */
#define   TSLB_YLAST          0x08          /* UYVY or VYUY - see com13 */
#define REG_COM11        0x3b        /* Control 11 */
#define   COM11_NIGHT          0x80          /* NIght mode enable */
#define   COM11_NMFR          0x60          /* Two bit NM frame rate */
#define   COM11_HZAUTO          0x10          /* Auto detect 50/60 Hz */
#define          COM11_50HZ          0x08          /* Manual 50Hz select */
#define   COM11_EXP          0x02
#define REG_COM12        0x3c        /* Control 12 */
#define   COM12_HREF          0x80          /* HREF always */
#define REG_COM13        0x3d        /* Control 13 */
#define   COM13_GAMMA          0x80          /* Gamma enable */
#define          COM13_UVSAT          0x40          /* UV saturation auto adjustment */
#define   COM13_UVSWAP          0x01          /* V before U - w/TSLB */
#define REG_COM14        0x3e        /* Control 14 */
#define   COM14_DCWEN          0x10          /* DCW/PCLK-scale enable */
#define REG_EDGE        0x3f        /* Edge enhancement factor */
#define REG_COM15        0x40        /* Control 15 */
#define   COM15_R10F0          0x00          /* Data range 10 to F0 */
#define          COM15_R01FE          0x80          /*            01 to FE */
#define   COM15_R00FF          0xc0          /*            00 to FF */
#define   COM15_RGB565          0x10          /* RGB565 output */
#define   COM15_RGB555          0x30          /* RGB555 output */
#define REG_COM16        0x41        /* Control 16 */
#define   COM16_AWBGAIN   0x08          /* AWB gain enable */
#define REG_COM17        0x42        /* Control 17 */
#define   COM17_AECWIN          0xc0          /* AEC window - must match COM4 */
#define   COM17_CBAR          0x08          /* DSP Color bar */
/*
 * This matrix defines how the colors are generated, must be
 * tweaked to adjust hue and saturation.
 *
 * Order: v-red, v-green, v-blue, u-red, u-green, u-blue
 *
 * They are nine-bit signed quantities, with the sign bit
 * stored in 0x58.  Sign for v-red is bit 0, and up from there.
 */
#define        REG_CMATRIX_BASE        0x4f
#define   CMATRIX_LEN 6
#define REG_MTX1                        0x4f
#define REG_MTX2                        0x50
#define REG_MTX3                        0x51
#define REG_MTX4                        0x52
#define REG_MTX5                        0x53
#define REG_MTX6                        0x54
#define REG_BRIGHTNESS                0x55 /* Brightness */
#define REG_CONTRAST                0x56 /* Contrast control */
#define REG_CMATRIX_SIGN         0x58
#define REG_GFIX                        0x69 /* Fix gain control */
#define REG_DBLV                        0x6b /* PLL control an debugging */
#define   DBLV_BYPASS        0x00         /* Bypass PLL */
#define   DBLV_X4                  0x01         /* clock x4 */
#define   DBLV_X6                  0x10         /* clock x6 */
#define   DBLV_X8                  0x11         /* clock x8 */
#define REG_SCALING_X           0x70
#define REG_SCALING_Y           0x71
#define REG_REG76                        0x76 /* OV's name */
#define   R76_BLKPCOR        0x80         /* Black pixel correction enable */
#define   R76_WHTPCOR        0x40         /* White pixel correction enable */
#define REG_RGB444                        0x8c /* RGB 444 control */
#define   R444_ENABLE        0x02         /* Turn on RGB444, overrides 5x5 */
#define   R444_RGBX                  0x01         /* Empty nibble at end */
#define REG_HAECC1                        0x9f /* Hist AEC/AGC control 1 */
#define REG_HAECC2                        0xa0 /* Hist AEC/AGC control 2 */
#define REG_BD50MAX                        0xa5 /* 50hz banding step limit */
#define REG_HAECC3                        0xa6 /* Hist AEC/AGC control 3 */
#define REG_HAECC4                        0xa7 /* Hist AEC/AGC control 4 */
#define REG_HAECC5                        0xa8 /* Hist AEC/AGC control 5 */
#define REG_HAECC6                        0xa9 /* Hist AEC/AGC control 6 */
#define REG_HAECC7                        0xaa /* Hist AEC/AGC control 7 */
#define REG_BD60MAX                        0xab /* 60hz banding step limit */

/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix numbers come from OmniVision.
 */
static struct ov7670soc_format_struct {
        enum v4l2_mbus_pixelcode mbus_code;
        enum v4l2_colorspace colorspace;
        struct regval_list *regs;
        int cmatrix[CMATRIX_LEN];
} ov7670soc_formats[] = {
        {
                /* TODO: registers are set for UYUV, but  we are present as YUYV, otherwise image is 
                 * invalid 
                 */
                .mbus_code        = V4L2_MBUS_FMT_YVYU8_2X8,
                .colorspace        = V4L2_COLORSPACE_JPEG,
                /*TODO: list of regs with preset values for specified format
                .regs                 = ov7670soc_fmt_yuv422,*/
                .cmatrix        = { -64, -52, -12, -23, -41, 132 },
        },
};
#define N_OV7670_FMTS ARRAY_SIZE(ov7670soc_formats)

/**
 * struct ov7670soc_decoder - OV7670 decoder object
 * sd: Subdevice Slave handle
 * TODO: ov7670soc_regs: copy of hw's regs with preset values.
 * pdata: Board specific
 * fmt_list: Format list
 * num_fmts: Number of formats
 * fmt: Current format
 * brightness: only for queries
 * contrast: only for queries
 * saturation: only for queries
 * hue: only for queries
 */
struct ov7670soc_decoder {
        struct v4l2_subdev sd;
        /*
        TODO: list of def reg values for read queries and RMW operations.
        Local cache due to impossibility of registers read
        struct ov7670soc_reg ov7670soc_regs[ARRAY_SIZE(ov7670soc_reg_list_default)];*/
        const struct ov7670soc_platform_data *pdata;
        const struct ov7670soc_format_struct *fmt_list;
        int num_fmts;
        struct ov7670soc_format_struct *fmt;  /* Current format */
        int brightness;
        int contrast;
        int saturation;
        int hue;
};

static const struct v4l2_queryctrl ov7670soc_controls[] = {
        {
                .id                = V4L2_CID_BRIGHTNESS,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "brightness",
                .minimum        = 0,
                .maximum        = 255,
                .step                = 1,
                .default_value        = 128,
        },
        {
                .id                = V4L2_CID_CONTRAST,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "contrast",
                .minimum        = 0,
                .maximum        = 127,
                .step                = 1,
                .default_value        = 64,
        },
        {
                .id                = V4L2_CID_SATURATION,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "saturation",
                .minimum        = 0,
                .maximum        = 256,
                .step                = 1,
                .default_value        = 128,
        },
        {
                .id                = V4L2_CID_HUE,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "hue",
                .minimum        = -180,
                .maximum        = 180,
                .step                = 5,
                .default_value        = 0,
        },
};

static inline struct ov7670soc_decoder *to_decoder(struct v4l2_subdev *sd)
{
        return container_of(sd, struct ov7670soc_decoder, sd);
}

/* 
 * Read register prohibited on Tegra T-20 due to
 * arbitration lost after sending device read addres
 */
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static int ov7670soc_read_reg(struct i2c_client *client, unsigned char reg, unsigned char *val)
{
        int err, retry = 0;

read_again:
        err = i2c_smbus_read_byte_data(client, reg);
        if (err < 0) {
                if (retry <= I2C_RETRY_COUNT) {
                        retry++;
                        msleep_interruptible(10);
                        goto read_again;
                }
                dev_err(&client->dev, "Failed to read register 0x%02X!\n", reg);
        }
        *val = (unsigned char)err;
        return err;
}
#endif

static int ov7670soc_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
        int err, retry = 0;

write_again:
        err = i2c_smbus_write_byte_data(client, reg, val);
        if (err) {
                if (retry <= I2C_RETRY_COUNT) {
                        retry++;
                        msleep_interruptible(10);
                        goto write_again;
                }
                dev_err(&client->dev, "Failed to write 0x%02X to register 0x%02X!\n", val, reg);
        }

        return err;
}

/* 
 * Reset all camera registers to default values
 */
static int ov7670soc_reset(struct i2c_client *client)
{
        int ret;
        
        ret = ov7670soc_write_reg(client, REG_COM7, COM7_RESET);
        if (ret)
            dev_err(&client->dev, "An error occurred while entering soft reset!\n");

        return ret;
}

/*
 * ov7670soc_setup - initializes a list of OV7670 registers
 */
static int ov7670soc_setup(struct v4l2_subdev *sd, u32 val)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);

    ov7670soc_reset(client);

    /* Configure HSYNC & VSYNC */
        ov7670soc_write_reg(client, REG_COM10, COM10_HSYNC | COM10_VS_NEG);
        /* not sure what this does, but improves colors quality */
        ov7670soc_write_reg(client, 0xb0, 0x84);

        /* Config HSYNC offset */
        ov7670soc_write_reg(client, REG_HSTART, 0x00);
        ov7670soc_write_reg(client, REG_HSTOP, 0x00); 
        ov7670soc_write_reg(client, REG_HSYST, 0x00);
        ov7670soc_write_reg(client, REG_HSYEN, 0x05);
        /* default MTX1..MTX5 and MTX6 = 0x84 makes better image */
        ov7670soc_write_reg(client, REG_MTX6, 0x84);
        
    //TODO: output format, move to mbus_s_fmt
    ov7670soc_write_reg(client, REG_COM7, COM7_YUV);  /* Selects YUV mode */
    /* U before V */
    ov7670soc_write_reg(client, REG_COM13, COM13_GAMMA | COM13_UVSAT);
      
        return 0;
}

/*
 * Hue also requires messing with the color matrix.  It also requires
 * trig functions, which tend not to be well supported in the kernel.
 * So here is a simple table of sine values, 0-90 degrees, in steps
 * of five degrees.  Values are multiplied by 1000.
 *
 * The following naive approximate trig functions require an argument
 * carefully limited to -180 <= theta <= 180.
 */
#define SIN_STEP 5
static const int ov7670soc_sin_table[] = {
           0,         87,   173,   258,   342,   422,
         499,        573,   642,   707,   766,   819,
         866,        906,   939,   965,   984,   996,
        1000
};

static int ov7670soc_sine(int theta)
{
        int chs = 1;
        int sine;

        if (theta < 0) {
                theta = -theta;
                chs = -1;
        }
        if (theta <= 90)
                sine = ov7670soc_sin_table[theta/SIN_STEP];
        else {
                theta -= 90;
                sine = 1000 - ov7670soc_sin_table[theta/SIN_STEP];
        }
        return sine*chs;
}

static int ov7670soc_cosine(int theta)
{
        theta = 90 - theta;
        if (theta > 180)
                theta -= 360;
        else if (theta < -180)
                theta += 360;
        return ov7670soc_sine(theta);
}

static void ov7670soc_calc_cmatrix(struct ov7670soc_decoder *decoder, int matrix[CMATRIX_LEN], int sat, int hue)
{
        int i;
        /*
         * Apply the current saturation setting first.
         */
        for (i = 0; i < CMATRIX_LEN; i++)
                matrix[i] = (decoder->fmt->cmatrix[i] * sat) >> 7;
        /*
         * Then, if need be, rotate the hue value.
         */
        if (hue != 0) {
                int sinth, costh;
                sinth = ov7670soc_sine(hue);
                costh = ov7670soc_cosine(hue);

                matrix[0] = (matrix[3]*sinth + matrix[0]*costh)/1000;
                matrix[1] = (matrix[4]*sinth + matrix[1]*costh)/1000;
                matrix[2] = (matrix[5]*sinth + matrix[2]*costh)/1000;
                matrix[3] = (matrix[3]*costh - matrix[0]*sinth)/1000;
                matrix[4] = (matrix[4]*costh - matrix[1]*sinth)/1000;
                matrix[5] = (matrix[5]*costh - matrix[2]*sinth)/1000;
        }
}

static int ov7670soc_store_cmatrix(struct v4l2_subdev *sd, int matrix[CMATRIX_LEN])
{
        int i, ret;
        unsigned char signbits = 0;
        struct i2c_client *client = v4l2_get_subdevdata(sd);

        /*
         * Weird crap seems to exist in the upper part of
         * the sign bits register, so let's preserve it.
         */
/*#ifndef CONFIG_ARCH_TEGRA_2x_SOC
        /* TODO: read forbidden on T-20 
        ret = ov7670soc_read_reg(client, REG_CMATRIX_SIGN, &signbits);
        signbits &= 0xc0;
#endif*/
        for (i = 0; i < CMATRIX_LEN; i++) {
                unsigned char raw;

                if (matrix[i] < 0) {
                        signbits |= (1 << i);
                        if (matrix[i] < -255)
                                raw = 0xff;
                        else
                                raw = (-1 * matrix[i]) & 0xff;
                }
                else {
                        if (matrix[i] > 255)
                                raw = 0xff;
                        else
                                raw = matrix[i] & 0xff;
                }
                ret += ov7670soc_write_reg(client, REG_CMATRIX_BASE + i, raw);
        }
        ret += ov7670soc_write_reg(client, REG_CMATRIX_SIGN, signbits);
        return ret;
}

static unsigned char ov7670soc_abs_to_sm(unsigned char v)
{
        if (v > 127)
                return v & 0x7f;
        return (128 - v) | 0x80;
}

static int ov7670soc_s_brightness(struct v4l2_subdev *sd, int value)
{
        unsigned char v;//, com8 = 0;
        int ret;
        struct i2c_client *client = v4l2_get_subdevdata(sd);

        /* TODO: read forbidden on T-20 */
/*#ifndef CONFIG_ARCH_TEGRA_2x_SOC
        unsigned char com8 = 0;
        ret = ov7670soc_read_reg(client, REG_COM8, &com8);
        com8 &= ~COM8_AEC;
#endif*/

        ov7670soc_write_reg(client, REG_COM8, 0x8e); //defaul val is 0x8f, 0x8e->disable AEC
        v = ov7670soc_abs_to_sm(value);
        ret = ov7670soc_write_reg(client, REG_BRIGHTNESS, v);
        return ret;
}

static int ov7670soc_s_contrast(struct v4l2_subdev *sd, int value)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);

        return ov7670soc_write_reg(client, REG_CONTRAST, (unsigned char) value);
}

static int ov7670soc_s_sat_hue(struct v4l2_subdev *sd, int sat, int hue)
{
        struct ov7670soc_decoder *decoder = to_decoder(sd);
        int matrix[CMATRIX_LEN];
        int ret;

        ov7670soc_calc_cmatrix(decoder, matrix, sat, hue);
        ret = ov7670soc_store_cmatrix(sd, matrix);
        return ret;
}

static int ov7670soc_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        struct ov7670soc_decoder *decoder = to_decoder(sd);
        
        switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
                return ov7670soc_s_brightness(sd, ctrl->value);
        case V4L2_CID_CONTRAST:
                return ov7670soc_s_contrast(sd, ctrl->value);
        case V4L2_CID_SATURATION:
                return ov7670soc_s_sat_hue(sd, ctrl->value, decoder->hue);
        case V4L2_CID_HUE:
                return ov7670soc_s_sat_hue(sd, decoder->saturation, ctrl->value);
        }
        return -EINVAL;
}

static int ov7670soc_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        struct ov7670soc_decoder *decoder = to_decoder(sd);

        switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
                ctrl->value = decoder->brightness;
                break;
        case V4L2_CID_CONTRAST:
                ctrl->value = decoder->contrast;
                break;
        case V4L2_CID_SATURATION:
                ctrl->value = decoder->saturation;
                break;
        case V4L2_CID_HUE:
                ctrl->value = decoder->hue;
                break;
        }
        return 0;
}

/* Functions required by soc_camera_ops ***************************************/
/* Alter bus settings on camera side */
static int ov7670soc_set_bus_param(struct soc_camera_device *icd, unsigned long flags)
{
        return 0;
}

static unsigned long ov7670soc_query_bus_param(struct soc_camera_device *icd)
{
        struct soc_camera_link *icl = to_soc_camera_link(icd);
                
        unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
                SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
                SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;

        return soc_camera_apply_sensor_flags(icl, flags);
}
/******************************************************************************/

/* Functions required by v4l2_subdev_video_ops ********************************/
static int ov7670soc_s_stream(struct v4l2_subdev *sd, int enable)
{
        /* 
        The OV7670 camera dose not have ability to start/stop streaming
        */
        return 0;
}

static int ov7670soc_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
        /*
         * Set the image format. Currently we support only one format with
         * fixed resolution, so we can set the format as it is on camera startup.
         */
        ov7670soc_setup(sd, 0);

        return 0;
}

static int ov7670soc_try_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
        struct ov7670soc_decoder *decoder = to_decoder(sd);
        int index;

        /* Check if we support queried image format. */        
        for (index = 0; index < N_OV7670_FMTS; index++)
                if (ov7670soc_formats[index].mbus_code == mf->code)
                        break;
        /* If not, set the only one which we support */
        if (index >= N_OV7670_FMTS) {
                /* default to first format */
                index = 0;
                mf->code = ov7670soc_formats[0].mbus_code;
        }

        /* Store the current format */        
        decoder->fmt = &ov7670soc_formats[index];
        
        /* Fixed value, move to ov7670_formats */
        mf->field = V4L2_FIELD_NONE;
        /* TODO: support for other resolutions (CIF/QCIF etc).*/
    mf->width = VGA_WIDTH;
        mf->height = VGA_HEIGHT;
        mf->colorspace = decoder->fmt->colorspace;

        return 0;
}

static int ov7670soc_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index, enum v4l2_mbus_pixelcode *code)
{
        if (index >= ARRAY_SIZE(ov7670soc_formats))
                return -EINVAL;

    *code = ov7670soc_formats[index].mbus_code;

        return 0;
}
/******************************************************************************/

static struct soc_camera_ops ov7670soc_soc_camera_ops = {
        .set_bus_param                = ov7670soc_set_bus_param,
        .query_bus_param        = ov7670soc_query_bus_param,
        .controls                        = ov7670soc_controls,
        .num_controls                = ARRAY_SIZE(ov7670soc_controls),
};

static const struct v4l2_subdev_core_ops ov7670soc_core_ops = {
        .g_ctrl = ov7670soc_g_ctrl,
        .s_ctrl = ov7670soc_s_ctrl,
};

static const struct v4l2_subdev_video_ops ov7670soc_video_ops = {
        .s_stream = ov7670soc_s_stream,
        .s_mbus_fmt = ov7670soc_s_mbus_fmt,
        .try_mbus_fmt = ov7670soc_try_mbus_fmt,
        .enum_mbus_fmt = ov7670soc_enum_mbus_fmt,
};

static const struct v4l2_subdev_ops ov7670soc_ops = {
        .core = &ov7670soc_core_ops,
        .video = &ov7670soc_video_ops,
};

static int ov7670soc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ov7670soc_decoder *decoder;
    struct soc_camera_device *icd = client->dev.platform_data;
    struct soc_camera_link *icl;
        struct v4l2_subdev *sd;

        /* Check if the adapter supports the needed features */
        if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
                return -EIO;

    icl = to_soc_camera_link(icd);
        if (!icl) {
                dev_err(&client->dev, "No platform data!!\n");
                return -ENODEV;
        }

        decoder = kzalloc(sizeof(struct ov7670soc_decoder), GFP_KERNEL);
        if (!decoder)
        {
            dev_err(&client->dev, "Failed to allocate memory for private data!\n");
                return -ENOMEM;
        }
        sd = &decoder->sd;

        /* Initialize the ov7670soc_decoder with default configuration */
        decoder->fmt_list = ov7670soc_formats;
        decoder->num_fmts = ARRAY_SIZE(ov7670soc_formats);
        decoder->fmt = &ov7670soc_formats[0];
        decoder->pdata = client->dev.platform_data;
        decoder->brightness = 128;
        decoder->contrast = 64;
        decoder->saturation = 128;
        decoder->hue = 0;
        /* Register with V4L2 layer as slave device */
        v4l2_i2c_subdev_init(sd, client, &ov7670soc_ops);

        icd->ops = &ov7670soc_soc_camera_ops;

        return 0;
}

static int ov7670soc_remove(struct i2c_client *client)
{
        struct v4l2_subdev *sd = i2c_get_clientdata(client);
        struct ov7670soc_decoder *decoder = to_decoder(sd);

        v4l2_device_unregister_subdev(sd);
        kfree(decoder);

        return 0;
}

static const struct i2c_device_id ov7670soc_id[] = 
{
        {OV7670SOC_MODULE_NAME, 0},
        { }
};
MODULE_DEVICE_TABLE(i2c, ov7670soc_id);

static struct i2c_driver ov7670soc_driver = {
        .driver = {
                .owner = THIS_MODULE,
                .name = OV7670SOC_MODULE_NAME,
        },
        .probe = ov7670soc_probe,
        .remove = ov7670soc_remove,
        .id_table = ov7670soc_id,
};

static int __init ov7670soc_init(void)
{
        return i2c_add_driver(&ov7670soc_driver);
}

static void __exit ov7670soc_exit(void)
{
        i2c_del_driver(&ov7670soc_driver);
}

module_init(ov7670soc_init);
module_exit(ov7670soc_exit);

MODULE_AUTHOR("Antmicro Ltd.");
MODULE_DESCRIPTION("OV7670 linux decoder driver");
MODULE_LICENSE("GPL");
