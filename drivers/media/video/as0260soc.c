/*
 * Aptina AS0260 SoC Camera Driver
 *
 * Copyright (C) 2014 AntMicro Ltd
 * Based on Generic Platform Camera Driver,
 * Copyright (C) 2008 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

/* MODULE NAME */
#define MODULE_NAME "as0260soc"

static unsigned int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level");

#define I2C_RETRY_COUNT          5
#define LINE_PIXELS                640
#define FRAME_LINES                480

/* Logical address */
#define _VAR(id, offset, base)        (base | (id & 0x1F) << 10 | (offset & 0x3FF))
#define VAR_L(id, offset)  _VAR(id, offset, 0x0000)
#define VAR(id, offset) _VAR(id, offset, 0x8000)


/* Register definitions */
static const unsigned short AS0620_SYS_CTL        = 0x001A; /* Control register, write 0x01 to reset, 0x00 to run the camera. */
static const unsigned short AS0620_BRIGHT_CTL     = 0xCC0A; /* Brightness control */
static const unsigned short AS0620_SATURATION_CTL = 0xCC12; /* Color saturation control */
static const unsigned short AS0620_HUE_CTL        = 0xCC10; /* Hue control */
static const unsigned short AS0620_CONTRAST_CTL   = 0xCC0C; /* Contrast control */


/* Camera properties definition */

struct i2c_reg_value {
        unsigned char reg;
        unsigned char value;
};

typedef struct {
        enum v4l2_mbus_pixelcode mbus_code;
        enum v4l2_colorspace colorspace;

} AS0260_Format_Struct;

AS0260_Format_Struct as0260soc_formats[] = {
        {
                .mbus_code        = V4L2_MBUS_FMT_YUYV8_2X8,
                .colorspace        = V4L2_COLORSPACE_JPEG,
        },
};

#define AP0260_FMTS ARRAY_SIZE(as0260soc_formats)

struct as0260soc_decoder {
        struct v4l2_subdev sd;
        const struct as0260soc_format_struct *fmt_list;
        struct as0260soc_format_struct *fmt;
        int num_fmts;
        int active_input;
};

static const struct v4l2_queryctrl as0260soc_controls[] = {
        {
                .id                    = V4L2_CID_BRIGHTNESS,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "brightness",
                .minimum        = 0,
                .maximum        = 255,
                .step                = 1,
                .default_value        = 128,
        },
        {
                .id                    = V4L2_CID_CONTRAST,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "contrast",
                .minimum        = 0,
                .maximum        = 207,
                .step                = 1,
                .default_value        = 128,
        },
        {
                .id                    = V4L2_CID_SATURATION,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "saturation",
                .minimum        = 0,
                .maximum        = 255,
                .step                = 1,
                .default_value        = 128,
        },
        {
                .id                    = V4L2_CID_HUE,
                .type                = V4L2_CTRL_TYPE_INTEGER,
                .name                = "hue",
                .minimum        = -127,
                .maximum        = 127,
                .step                = 1,
                .default_value        = 0,
        },
};

static inline struct as0260soc_decoder *to_decoder(struct v4l2_subdev *sd)
{
        return container_of(sd, struct as0260soc_decoder, sd);
}

/****************************************************************************/
/*                                                                          */
/*                                           Camera access                                */
/*                                                                          */
/****************************************************************************/

static int as0260soc_read_reg(struct v4l2_subdev *sd, unsigned char addr, unsigned char *val)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        int err, retry = I2C_RETRY_COUNT;

    printk(KERN_ERR "as0260soc driver: read_reg function.\n");

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
                v4l2_dbg(2, debug, sd, "as0260: read 0x%02x = 0x%02x\n", addr, *val);
        }

    printk(KERN_ERR "as0260soc driver: read_reg function, result %d.\n", err);
    
    return err;
}

static int as0260soc_write_reg(struct v4l2_subdev *sd, unsigned char addr, unsigned char val)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        int err, retry = I2C_RETRY_COUNT;

    printk(KERN_ERR "as0260soc driver: write_reg function.\n");

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
                v4l2_dbg(2, debug, sd, "as0260: write 0x%02x = 0x%02x\n", addr, val);

    printk(KERN_ERR "as0260soc driver: write_reg function, result %d.\n", err);

        return err;
}

static int as0260soc_write_inittab(struct v4l2_subdev *sd,        const struct i2c_reg_value *regs)
{
        while (regs->reg != 0xff) {
                as0260soc_write_reg(sd, regs->reg, regs->value);
                regs++;
        }
        return 0;
}

static int tvp5150soc_s_input(struct file *file, void *priv, unsigned int i)
{
    /* TODO implement this functionality */
    printk(KERN_ERR "as0260soc driver: s_input function.\n");
        return 0;
}

static int as0260soc_g_input(struct file *file, void *priv, unsigned int *i)
{
    /* TODO implement this functionality */
    printk(KERN_ERR "as0260soc driver: g_input function.\n");
        return 0;
}

static int as0260soc_set_bus_param(struct soc_camera_device *icd, unsigned long flags)
{
    /* TODO implement this functionality */
    return 0;
}

static unsigned long as0260soc_query_bus_param(struct soc_camera_device *icd)
{
        struct soc_camera_link *icl = to_soc_camera_link(icd);

        unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
                SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
                SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;

        return soc_camera_apply_sensor_flags(icl, flags);
}

static int as0260soc_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
                /* as0260soc_read_reg(sd, AS0620_BRIGHT_CTL, (unsigned char*)&ctrl->value); */
                return 0;
        case V4L2_CID_CONTRAST:
                /* as0260soc_read_reg(sd, AS0620_CONTRAST_CTL, (unsigned char*)&ctrl->value); */
                return 0;
        case V4L2_CID_SATURATION:
                /* as0260soc_read_reg(sd, AS0620_SATURATION_CTL, (unsigned char*)&ctrl->value); */
                return 0;
        case V4L2_CID_HUE:
                /* as0260soc_read_reg(sd, AS0620_HUE_CTL, (unsigned char*)&ctrl->value); */
                return 0;
        }
        return -EINVAL;
}

static int as0260soc_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
                /* as0260soc_write_reg(sd, AS0620_BRIGHT_CTL, ctrl->value); */
                return 0;
        case V4L2_CID_CONTRAST:
                /* as0260soc_write_reg(sd, AS0620_CONTRAST_CTL, ctrl->value); */
                return 0;
        case V4L2_CID_SATURATION:
                /* as0260soc_write_reg(sd, AS0620_SATURATION_CTL, ctrl->value); */
                return 0;
        case V4L2_CID_HUE:
                /* as0260soc_write_reg(sd, AS0620_HUE_CTL, ctrl->value); */
                return 0;
        }
        return -EINVAL;
}

static int as0260soc_reset(struct v4l2_subdev *sd, u32 val)
{
    printk(KERN_ERR "as0260soc driver: reset function.\n");

    return 0;
}

/****************************************************************************/
/*                                                                          */
/*                                         Format control                                 */
/*                                                                          */
/****************************************************************************/

static int as0260soc_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
        /*
         * Set the image format. Currently we support only one format with
         * fixed resolution, so we can set the format as it is on camera startup.
         */
        as0260soc_reset(sd, 0);

    printk(KERN_ERR "as0260soc driver: s_mbus_fmt function.\n");

        return 0;
}

static int as0260soc_try_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
        struct as0260soc_decoder *decoder = to_decoder(sd);
        int index;

    printk(KERN_ERR "as0260soc driver: try_mbus_fmt function.\n");

        /* Check if we support queried image format. */
        for (index = 0; index < AP0260_FMTS; index++)
                if (as0260soc_formats[index].mbus_code == mf->code)
            {
                break;
            }

    /* If not, set the only one which we support */
        if (index >= AP0260_FMTS) {
                /* default to first format */
                index = 0;
                mf->code = as0260soc_formats[0].mbus_code;
        }

        /* Store the current format */
        decoder->fmt = &as0260soc_formats[index];

        /* Fixed value, move to as0260soc_formats */
        mf->field = V4L2_FIELD_NONE;
    mf->width = LINE_PIXELS;
        mf->height = FRAME_LINES;
        mf->colorspace = as0260soc_formats[index].colorspace;

    printk(KERN_ERR "as0260soc driver: try_mbus_fmt function exit.\n");
        return 0;
}

static int as0260soc_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index, enum v4l2_mbus_pixelcode *code)
{
    printk(KERN_ERR "as0260soc driver: enum_mbus_fmt function.\n");

    if (index >= ARRAY_SIZE(as0260soc_formats))
        {
            printk(KERN_ERR "as0260soc driver: enum_mbus_fmt function, index error.\n");
            return -EINVAL;
        }

    *code = as0260soc_formats[index].mbus_code;

    printk(KERN_ERR "as0260soc driver: enum_mbus_fmt function exit.\n");

    return 0;
}

/****************************************************************************/
/*                                                                          */
/*                                         Camera options                                 */
/*                                                                          */
/****************************************************************************/

static struct soc_camera_ops as0260soc_camera_ops = {
        .set_bus_param                = as0260soc_set_bus_param,
        .query_bus_param        = as0260soc_query_bus_param,
        .controls                        = as0260soc_controls,
        .num_controls                = ARRAY_SIZE(as0260soc_controls),
};

static const struct v4l2_subdev_core_ops as0260soc_core_ops = {
        .g_ctrl = as0260soc_g_ctrl,
        .s_ctrl = as0260soc_s_ctrl,
        .reset = as0260soc_reset,
};

static const struct v4l2_subdev_video_ops as0260soc_video_ops = {
        .s_mbus_fmt         = as0260soc_s_mbus_fmt,
        .try_mbus_fmt         = as0260soc_try_mbus_fmt,
        .enum_mbus_fmt         = as0260soc_enum_mbus_fmt,
};

static const struct v4l2_subdev_ops as0260soc_ops = {
        .core = &as0260soc_core_ops,
        .video = &as0260soc_video_ops,
};

/****************************************************************************/
/*                                                                          */
/*                                       I2C Client & Driver                              */
/*                                                                          */
/****************************************************************************/

static int as0260soc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct soc_camera_device *icd = client->dev.platform_data;
        struct soc_camera_link *icl;
        struct as0260soc_decoder *decoder;
        struct v4l2_subdev *sd;
    struct v4l2_ioctl_ops *ops;
        unsigned char msb_id, lsb_id, msb_rom, lsb_rom;

    printk(KERN_ERR "as0260soc driver: probe function.\n");

        /* Check if the adapter supports the needed features */
        if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        {
            printk(KERN_ERR "as0260soc driver: probe function, i2c_check_functionality returns error.\n");
            return -EIO;
        }

    icl = to_soc_camera_link(icd);
        if (!icl)
        {
        printk(KERN_ERR "as0260soc driver: probe function, to_soc_camera_link returns data.\n");
        dev_err(&client->dev, "No platform data!!\n");
                return -ENODEV;
        }

        decoder = kzalloc(sizeof(struct as0260soc_decoder), GFP_KERNEL);
        if (!decoder)
        {
        printk(KERN_ERR "as0260soc driver: probe function, canot allocate memory for decoder struct.\n");
        dev_err(&client->dev, "Failed to allocate memory for private data!\n");
                return -ENOMEM;
        }

        /* TODO: init def settings of as0260soc_decoder */
        sd = &decoder->sd;

        /* Register with V4L2 layer as slave device */
        v4l2_i2c_subdev_init(sd, client, &as0260soc_ops);

    /* TODO: Taken from other driver */
        /* as0260soc_read_reg(sd, AS0260_MSB_DEV_ID, &msb_id); */
        /* as0260soc_read_reg(sd, AS0260_LSB_DEV_ID, &lsb_id); */
        /* as0260soc_read_reg(sd, AS0260_ROM_MAJOR_VER, &msb_rom); */
        /* as0260soc_read_reg(sd, AS0260_ROM_MINOR_VER, &lsb_rom); */

        /* if (msb_rom == 4 && lsb_rom == 0) { /\* Is TVP5150AM1 *\/ */
        /*         v4l2_info(sd, "as0260%02x%02xam1 detected.\n", msb_id, lsb_id); */
        /*         /\* ITU-T BT.656.4 timing *\/ */
        /*         as0260soc_write_reg(sd, AS0260_REV_SELECT, 0); */
        /* } else { */
        /*         if (msb_rom == 3 || lsb_rom == 0x21) { /\* Is TVP5150A *\/ */
        /*                 v4l2_info(sd, "as0260%02x%02xa detected.\n", msb_id, lsb_id); */
        /*         } else { */
        /*                 v4l2_info(sd, "*** unknown as0260%02x%02x chip detected.\n", msb_id, lsb_id); */
        /*                 v4l2_info(sd, "*** Rom ver is %d.%d\n", msb_rom, lsb_rom); */
        /*         } */
        /* } */
    /* ~TODO */

        icd->ops = &as0260soc_camera_ops;

        /*
         * This is the only way to support more than one input as soc_camera
         * assumes in its own vidioc_s(g)_input implementation that only one
         * input is present we have to override that with our own handlers.
         */

    /* TODO */
        /* ops = (struct v4l2_ioctl_ops*)icd->vdev->ioctl_ops; */
        /* ops->vidioc_s_input = &as0260soc_s_input; */
        /* ops->vidioc_g_input = &as0260soc_g_input; */

    printk(KERN_ERR "as0260soc driver: probe function exit.\n");

        return 0;
}

static int as0260soc_remove(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
        struct as0260soc_decoder *decoder = to_decoder(sd);

    printk(KERN_ERR "as0260soc driver: remove function.\n");

        v4l2_device_unregister_subdev(sd);
        kfree(decoder);
    
        return 0;
}

static const struct i2c_device_id as0260soc_id[] = {
        { MODULE_NAME, 0 },
        { }
};

static struct i2c_driver as0260soc_driver = {
        .driver         = {
                .name        = MODULE_NAME,
                .owner        = THIS_MODULE,
        },
        .probe                = as0260soc_probe,
        .remove                = as0260soc_remove,
    .id_table   = as0260soc_id
};

static int __init init_as0260soc(void)
{
        return i2c_add_driver(&as0260soc_driver);
}

static void __exit exit_as0260soc(void)
{
        i2c_del_driver(&as0260soc_driver);
}

module_init(init_as0260soc);
module_exit(exit_as0260soc);

MODULE_DESCRIPTION("Aptina AS0260 SoC Camera driver");
MODULE_AUTHOR("AntMicro");
MODULE_LICENSE("GPL v2");
