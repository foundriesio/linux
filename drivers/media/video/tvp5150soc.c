/*
 * tvp5150 - Texas Instruments TVP5150A/AM1 video decoder driver
 *
 * Copyright (c) 2005,2006 Mauro Carvalho Chehab (mchehab@infradead.org)
 * This code is placed under the terms of the GNU General Public License v2
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

#include <media/tvp5150.h>
#include "tvp5150_reg.h"

#define I2C_RETRY_COUNT                3
#define LINE_PIXELS                        576
#define FRAME_LINES                        520

#define MODULE_NAME "tvp5150soc"

static unsigned int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level");

struct i2c_reg_value {
        unsigned char reg;
        unsigned char value;
};

static struct tvp5150soc_format_struct {
        enum v4l2_mbus_pixelcode mbus_code;
        enum v4l2_colorspace colorspace;
} tvp5150soc_formats[] = {
        {
                .mbus_code        = V4L2_MBUS_FMT_YVYU8_2X8,
                .colorspace        = V4L2_COLORSPACE_JPEG,
        },
};
#define N_TVP5150_FMTS ARRAY_SIZE(tvp5150soc_formats)

struct tvp5150soc_decoder {
        struct v4l2_subdev sd;
        const struct tvp5150soc_format_struct *fmt_list;
        struct tvp5150soc_format_struct *fmt;
        int num_fmts;
        int active_input;
};

static const struct v4l2_queryctrl tvp5150soc_controls[] = {
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
                .maximum        = 207,
                .step                = 1,
                .default_value        = 128,
        },
        {
                .id                = V4L2_CID_SATURATION,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "saturation",
                .minimum        = 0,
                .maximum        = 255,
                .step                = 1,
                .default_value        = 128,
        },
        {
                .id                = V4L2_CID_HUE,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "hue",
                .minimum        = -127, /*TODO: TVP5150 supports hue in range -180..180, which is equal to -127..127 reg value*/
                .maximum        = 127,
                .step                = 1,
                .default_value        = 0,
        },
};

static inline struct tvp5150soc_decoder *to_decoder(struct v4l2_subdev *sd)
{
        return container_of(sd, struct tvp5150soc_decoder, sd);
}

static int tvp5150soc_read_reg(struct v4l2_subdev *sd, unsigned char addr, unsigned char *val)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        int err, retry = I2C_RETRY_COUNT;

    int ccc = client->addr;
    int aaa = addr;
    printk(KERN_ERR "tvp5150soc_read_reg: %04X %04X\n", ccc, aaa);

        while(retry)
        {
                err = i2c_smbus_read_byte_data(client, addr);
                if(err > 0)
                        break;
                retry--;
                msleep_interruptible(10);
        }

        if(err < 0)
                v4l2_dbg(0, debug, sd, "i2c i/o error: %d\n", err);
        else
        {
                *val = (unsigned char)err;
                v4l2_dbg(2, debug, sd, "tvp5150: read 0x%02x = 0x%02x\n", addr, *val);
        }

    printk(KERN_ERR "tvp5150soc_read_reg result: %d\n", err);
    
    return err;
}

static int tvp5150soc_write_reg(struct v4l2_subdev *sd, unsigned char addr, unsigned char val)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        int err, retry = I2C_RETRY_COUNT;

        while(retry)
        {
                err = i2c_smbus_write_byte_data(client, addr, val);
                if(err > 0)
                        break;
                retry--;
                msleep_interruptible(10);
        }

        if(err < 0)
                v4l2_dbg(0, debug, sd, "i2c i/o error: %d\n", err);
        else
                v4l2_dbg(2, debug, sd, "tvp5150: write 0x%02x = 0x%02x\n", addr, val);

        return err;
}

/* Default values as sugested at TVP5150AM1 datasheet */
static const struct i2c_reg_value tvp5150soc_init_default[] = {
        {TVP5150_VD_IN_SRC_SEL_1,0x00},                /* 0x00 */
        {TVP5150_ANAL_CHL_CTL,0x15},                /* 0x01 */
        {TVP5150_OP_MODE_CTL,0x00},                        /* 0x02 */
        {TVP5150_MISC_CTL,0x01},                        /* 0x03 */
        {TVP5150_COLOR_KIL_THSH_CTL,0x10},        /* 0x06 */
        {TVP5150_LUMA_PROC_CTL_1,0x60},                /* 0x07 */
        {TVP5150_LUMA_PROC_CTL_2,0x00},                /* 0x08 */
        {TVP5150_BRIGHT_CTL,0x80},                        /* 0x09 */
        {TVP5150_SATURATION_CTL,0x80},                /* 0x0a */
        {TVP5150_HUE_CTL,0x00},                                /* 0x0b */
        {TVP5150_CONTRAST_CTL,0x80},                /* 0x0c */
        {TVP5150_DATA_RATE_SEL,0x47},                /* 0x0d */
        {TVP5150_LUMA_PROC_CTL_3,0x00},                /* 0x0e */
        {TVP5150_CONF_SHARED_PIN,0x08},                /* 0x0f */
        {TVP5150_ACT_VD_CROP_ST_MSB,0x00},        /* 0x11 */
        {TVP5150_ACT_VD_CROP_ST_LSB,0x00},        /* 0x12 */
        {TVP5150_ACT_VD_CROP_STP_MSB,0x00},        /* 0x13 */
        {TVP5150_ACT_VD_CROP_STP_LSB,0x00},        /* 0x14 */
        {TVP5150_GENLOCK,0x01},                                /* 0x15 */
        {TVP5150_HORIZ_SYNC_START,0x80},        /* 0x16 */
        {TVP5150_VERT_BLANKING_START,0x00},        /* 0x18 */
        {TVP5150_VERT_BLANKING_STOP,0x00},        /* 0x19 */
        {TVP5150_CHROMA_PROC_CTL_1,0x0c},        /* 0x1a */
        {TVP5150_CHROMA_PROC_CTL_2,0x14},        /* 0x1b */
        {TVP5150_INT_RESET_REG_B,0x00},                /* 0x1c */
        {TVP5150_INT_ENABLE_REG_B,0x00},        /* 0x1d */
        {TVP5150_INTT_CONFIG_REG_B,0x00},        /* 0x1e */
        {TVP5150_VIDEO_STD,0x00},                        /* 0x28 */
        {TVP5150_MACROVISION_ON_CTR,0x0f},        /* 0x2e */
        {TVP5150_MACROVISION_OFF_CTR,0x01},        /* 0x2f */
        {TVP5150_TELETEXT_FIL_ENA,0x00},        /* 0xbb */
        {TVP5150_INT_STATUS_REG_A,0x00},        /* 0xc0 */
        {TVP5150_INT_ENABLE_REG_A,0x00},        /* 0xc1 */
        {TVP5150_INT_CONF,0x04},                        /* 0xc2 */
        {TVP5150_FIFO_INT_THRESHOLD,0x80},        /* 0xc8 */
        {TVP5150_FIFO_RESET,0x00},                        /* 0xc9 */
        {TVP5150_LINE_NUMBER_INT,0x00},                /* 0xca */
        {TVP5150_PIX_ALIGN_REG_LOW,0x4e},        /* 0xcb */
        {TVP5150_PIX_ALIGN_REG_HIGH,0x00},        /* 0xcc */
        {TVP5150_FIFO_OUT_CTRL,0x01},                /* 0xcd */
        {TVP5150_FULL_FIELD_ENA,0x00},                /* 0xcf */
        {TVP5150_LINE_MODE_INI,0x00},                /* 0xd0 */
        {TVP5150_FULL_FIELD_MODE_REG,0x7f},        /* 0xfc */
        { /* end of data */0xff,0xff}
};

/* Default values as sugested at TVP5150AM1 datasheet */
static const struct i2c_reg_value tvp5150soc_init_enable[] = {
        {TVP5150_VD_IN_SRC_SEL_1, 0x02},
        {TVP5150_CONF_SHARED_PIN, 0x02},
        {TVP5150_ANAL_CHL_CTL, 0x15}, /* Automatic offset and AGC enabled */
        {TVP5150_MISC_CTL, 0x6f}, /* Activate YCrCb output 0x9 or 0xd ? */
        {TVP5150_AUTOSW_MSK, 0x0}, /* Activates video std autodetection for all standards */
        {TVP5150_DATA_RATE_SEL, 0x47},/* Default format: 0x47. For 4:2:2: 0x40 */
        {TVP5150_CHROMA_PROC_CTL_1, 0x0c},
        {TVP5150_CHROMA_PROC_CTL_2, 0x54},
        {0x27, 0x20},/* Non documented, but initialized on WinTV USB2 */
        {0xff, 0xff}
};

static int tvp5150soc_write_inittab(struct v4l2_subdev *sd,        const struct i2c_reg_value *regs)
{
        while (regs->reg != 0xff) {
                tvp5150soc_write_reg(sd, regs->reg, regs->value);
                regs++;
        }
        return 0;
}

static int tvp5150soc_s_input(struct file *file, void *priv, unsigned int i)
{
        struct soc_camera_device *icd = file->private_data;
        struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
        struct tvp5150soc_decoder *decoder = to_decoder(sd);
        unsigned char val;

        if (i < 3)
        {
                decoder->active_input = i;
                switch (decoder->active_input) {
                case 0:
                        val = TVP5150_VIDEO_INPUT_SELECT_AIP1A;
                        break;
                case 1:
                        val = TVP5150_VIDEO_INPUT_SELECT_AIP1B;
                        break;
                case 2:
                        val = TVP5150_VIDEO_INPUT_SELECT_SVIDEO;
                        break;
                default:
                        val = TVP5150_VIDEO_INPUT_SELECT_AIP1A;
                }
                return tvp5150soc_write_reg(sd, TVP5150_VD_IN_SRC_SEL_1, val);
        }

        return -EINVAL;
}

static int tvp5150soc_g_input(struct file *file, void *priv, unsigned int *i)
{
        struct soc_camera_device *icd = file->private_data;
        struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
        struct tvp5150soc_decoder *decoder = to_decoder(sd);

        *i = decoder->active_input;

        return 0;
}

/********************************************************************************************************************************/

static int tvp5150soc_set_bus_param(struct soc_camera_device *icd, unsigned long flags)
{
        return 0;
}

static unsigned long tvp5150soc_query_bus_param(struct soc_camera_device *icd)
{
        struct soc_camera_link *icl = to_soc_camera_link(icd);

        unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
                SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
                SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;

        return soc_camera_apply_sensor_flags(icl, flags);
}

/********************************************************************************************************************************/

static int tvp5150soc_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
                tvp5150soc_read_reg(sd, TVP5150_BRIGHT_CTL, (unsigned char*)&ctrl->value);
                return 0;
        case V4L2_CID_CONTRAST:
                tvp5150soc_read_reg(sd, TVP5150_CONTRAST_CTL, (unsigned char*)&ctrl->value);
                return 0;
        case V4L2_CID_SATURATION:
                tvp5150soc_read_reg(sd, TVP5150_SATURATION_CTL, (unsigned char*)&ctrl->value);
                return 0;
        case V4L2_CID_HUE:
                tvp5150soc_read_reg(sd, TVP5150_HUE_CTL, (unsigned char*)&ctrl->value);
                return 0;
        }
        return -EINVAL;
}

static int tvp5150soc_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
                tvp5150soc_write_reg(sd, TVP5150_BRIGHT_CTL, ctrl->value);
                return 0;
        case V4L2_CID_CONTRAST:
                tvp5150soc_write_reg(sd, TVP5150_CONTRAST_CTL, ctrl->value);
                return 0;
        case V4L2_CID_SATURATION:
                tvp5150soc_write_reg(sd, TVP5150_SATURATION_CTL, ctrl->value);
                return 0;
        case V4L2_CID_HUE:
                tvp5150soc_write_reg(sd, TVP5150_HUE_CTL, ctrl->value);
                return 0;
        }
        return -EINVAL;
}

static int tvp5150soc_reset(struct v4l2_subdev *sd, u32 val)
{
        /* Initializes TVP5150 to its default values */
        /* TVP5150 has no ability to software reset */
        tvp5150soc_write_inittab(sd, tvp5150soc_init_default);

        /* Initializes TVP5150 to stream enabled values */
        tvp5150soc_write_inittab(sd, tvp5150soc_init_enable);

        return 0;
}

/********************************************************************************************************************************/

static int tvp5150soc_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
        /*
         * Set the image format. Currently we support only one format with
         * fixed resolution, so we can set the format as it is on camera startup.
         */
        tvp5150soc_reset(sd, 0);

        return 0;
}

static int tvp5150soc_try_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
        struct tvp5150soc_decoder *decoder = to_decoder(sd);
        int index;

    printk(KERN_ERR "tvp5150soc_probe try mbus format\n");

        /* Check if we support queried image format. */
        for (index = 0; index < N_TVP5150_FMTS; index++)
                if (tvp5150soc_formats[index].mbus_code == mf->code)
            {
                printk(KERN_ERR "tvp5150soc_probe try mbus format found format\n");
                break;
            }
        /* If not, set the only one which we support */
        if (index >= N_TVP5150_FMTS) {
                /* default to first format */
                index = 0;
        printk(KERN_ERR "tvp5150soc_probe try mbus format default format\n");
                mf->code = tvp5150soc_formats[0].mbus_code;
        }

        /* Store the current format */
        decoder->fmt = &tvp5150soc_formats[index];

        /* Fixed value, move to tvp5150soc_formats */
        mf->field = V4L2_FIELD_INTERLACED_TB;
    mf->width = LINE_PIXELS;
        mf->height = FRAME_LINES;
        mf->colorspace = tvp5150soc_formats[index].colorspace;

        return 0;
}

static int tvp5150soc_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index, enum v4l2_mbus_pixelcode *code)
{
        if (index >= ARRAY_SIZE(tvp5150soc_formats))
                return -EINVAL;

    *code = tvp5150soc_formats[index].mbus_code;

        return 0;
}
/********************************************************************************************************************************/

static struct soc_camera_ops tvp5150soc_soc_camera_ops = {
        .set_bus_param                = tvp5150soc_set_bus_param,
        .query_bus_param        = tvp5150soc_query_bus_param,
        .controls                        = tvp5150soc_controls,
        .num_controls                = ARRAY_SIZE(tvp5150soc_controls),
};

static const struct v4l2_subdev_core_ops tvp5150soc_core_ops = {
        .g_ctrl = tvp5150soc_g_ctrl,
        .s_ctrl = tvp5150soc_s_ctrl,
        .reset = tvp5150soc_reset,
};

static const struct v4l2_subdev_video_ops tvp5150soc_video_ops = {
        .s_mbus_fmt         = tvp5150soc_s_mbus_fmt,
        .try_mbus_fmt         = tvp5150soc_try_mbus_fmt,
        .enum_mbus_fmt         = tvp5150soc_enum_mbus_fmt,
};

static const struct v4l2_subdev_ops tvp5150soc_ops = {
        .core = &tvp5150soc_core_ops,
        .video = &tvp5150soc_video_ops,
};


/****************************************************************************
                        I2C Client & Driver
 ****************************************************************************/
static int tvp5150soc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct soc_camera_device *icd = client->dev.platform_data;
        struct soc_camera_link *icl;
        struct tvp5150soc_decoder *decoder;
        struct v4l2_subdev *sd;
    struct v4l2_ioctl_ops *ops;
        unsigned char msb_id, lsb_id, msb_rom, lsb_rom;

    printk(KERN_ERR "tvp5150soc_probe start\n");

        /* Check if the adapter supports the needed features */
        if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        {
            printk(KERN_ERR "tvp5150soc_probe error 1\n");
            return -EIO;
        }

    icl = to_soc_camera_link(icd);
        if (!icl)
        {
                dev_err(&client->dev, "No platform data!!\n");
        printk(KERN_ERR "tvp5150soc_probe error 2\n");
                return -ENODEV;
        }

        decoder = kzalloc(sizeof(struct tvp5150soc_decoder), GFP_KERNEL);
        if (!decoder)
        {
                dev_err(&client->dev, "Failed to allocate memory for private data!\n");
        printk(KERN_ERR "tvp5150soc_probe error 3\n");
                return -ENOMEM;
        }

        /* TODO: init def settings of tvp5150soc_decoder */

        sd = &decoder->sd;

        /* Register with V4L2 layer as slave device */
        v4l2_i2c_subdev_init(sd, client, &tvp5150soc_ops);

        tvp5150soc_read_reg(sd, TVP5150_MSB_DEV_ID, &msb_id);
        tvp5150soc_read_reg(sd, TVP5150_LSB_DEV_ID, &lsb_id);
        tvp5150soc_read_reg(sd, TVP5150_ROM_MAJOR_VER, &msb_rom);
        tvp5150soc_read_reg(sd, TVP5150_ROM_MINOR_VER, &lsb_rom);

        if (msb_rom == 4 && lsb_rom == 0) { /* Is TVP5150AM1 */
                v4l2_info(sd, "tvp%02x%02xam1 detected.\n", msb_id, lsb_id);
                /* ITU-T BT.656.4 timing */
                tvp5150soc_write_reg(sd, TVP5150_REV_SELECT, 0);
        } else {
                if (msb_rom == 3 || lsb_rom == 0x21) { /* Is TVP5150A */
                        v4l2_info(sd, "tvp%02x%02xa detected.\n", msb_id, lsb_id);
                } else {
                        v4l2_info(sd, "*** unknown tvp%02x%02x chip detected.\n", msb_id, lsb_id);
                        v4l2_info(sd, "*** Rom ver is %d.%d\n", msb_rom, lsb_rom);
                }
        }

        icd->ops = &tvp5150soc_soc_camera_ops;

        /*
         * This is the only way to support more than one input as soc_camera
         * assumes in its own vidioc_s(g)_input implementation that only one
         * input is present we have to override that with our own handlers.
         */
        ops = (struct v4l2_ioctl_ops*)icd->vdev->ioctl_ops;
        ops->vidioc_s_input = &tvp5150soc_s_input;
        ops->vidioc_g_input = &tvp5150soc_g_input;

    printk(KERN_ERR "tvp5150soc_probe return 0\n");
        return 0;
}

static int tvp5150soc_remove(struct i2c_client *client)
{
        struct v4l2_subdev *sd = i2c_get_clientdata(client);
        struct tvp5150soc_decoder *decoder = to_decoder(sd);

        v4l2_device_unregister_subdev(sd);
        kfree(decoder);

        return 0;
}

/* ----------------------------------------------------------------------- */

static const struct i2c_device_id tvp5150soc_id[] = {
        { MODULE_NAME, 0 },
        { }
};

static struct i2c_driver tvp5150soc_driver = {
        .driver = {
                .owner        = THIS_MODULE,
                .name        = MODULE_NAME,
        },
        .probe                = tvp5150soc_probe,
        .remove                = tvp5150soc_remove,
        .id_table        = tvp5150soc_id,
};

static __init int init_tvp5150soc(void)
{
        return i2c_add_driver(&tvp5150soc_driver);
}

static __exit void exit_tvp5150soc(void)
{
        i2c_del_driver(&tvp5150soc_driver);
}

module_init(init_tvp5150soc);
module_exit(exit_tvp5150soc);

MODULE_DESCRIPTION("Texas Instruments TVP5150A video decoder driver for soc_camera interface");
MODULE_AUTHOR("Antmicro Ltd.");
MODULE_LICENSE("GPL");
