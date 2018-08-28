/****************************************************************************
 * Copyright (C) 2015 Telechips Inc.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <mach/tca_mailbox.h>

#include "tcc_dsp.h"
#include "tcc_dsp_ipc.h"
#include "tcc_dsp_ctrl.h"
#include <mach/tcc-dsp-api.h>

struct tcc_dsp_t {
	struct tcc_dsp_ctrl_t dsp_ctrl;
	struct mutex m;
};

ssize_t tcc_dsp_read(struct file *flip, char __user *ibuf, size_t count, loff_t *f_pos)
{
	//struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	//struct tcc_dsp_t *dsp = (struct tcc_dsp_t*)dev_get_drvdata(misc->parent);

	return 0;
}

ssize_t tcc_dsp_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_pos)
{
	//struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	//struct tcc_dsp_t *dsp = (struct tcc_dsp_t*)dev_get_drvdata(misc->parent);

	return count;
}

long tcc_dsp_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	struct tcc_dsp_t *dsp = (struct tcc_dsp_t*)dev_get_drvdata(misc->parent);
	int ret = -1;

	mutex_lock(&dsp->m);
	switch (cmd) {
		case IOCTL_TCC_CONTROL_SET_PARAM:
			{
				struct tcc_control_param_t param;
				if (copy_from_user(&param, (void*)arg, sizeof(struct tcc_control_param_t)))
					break;

				ret = tcc_control_set_param(&dsp->dsp_ctrl, &param);

				if (copy_to_user((void*)arg, &param, sizeof(struct tcc_control_param_t)))
					break;
			}
			break;
		case IOCTL_TCC_CONTROL_GET_PARAM:
			{
				struct tcc_control_param_t param;
				if (copy_from_user(&param, (void*)arg, sizeof(struct tcc_control_param_t)))
					break;
			
				ret = tcc_control_get_param(&dsp->dsp_ctrl, &param);
				
				if (copy_to_user((void*)arg, &param, sizeof(struct tcc_control_param_t)))
					break;
			}
			break;
		case IOCTL_TCC_AM3D_SET_PARAM:
			{
				struct tcc_am3d_param_t param;
				if (copy_from_user(&param, (void*)arg, sizeof(struct tcc_am3d_param_t)))
					break;

				ret = tcc_am3d_set_param(&dsp->dsp_ctrl, &param);

				if (copy_to_user((void*)arg, &param, sizeof(struct tcc_am3d_param_t)))
					break;
			}
			break;
		case IOCTL_TCC_AM3D_GET_PARAM:
			{
				struct tcc_am3d_param_t param;
				if (copy_from_user(&param, (void*)arg, sizeof(struct tcc_am3d_param_t)))
					break;

				ret = tcc_am3d_get_param(&dsp->dsp_ctrl, &param);

				if (copy_to_user((void*)arg, &param, sizeof(struct tcc_am3d_param_t)))
					break;
			}
			break;
		case IOCTL_TCC_AM3D_SET_PARAM_TABLE:
			{
				struct tcc_am3d_param_tbl_t param_tbl;
				if (copy_from_user(&param_tbl, (void*)arg, sizeof(struct tcc_am3d_param_tbl_t)))
					break;

				ret = tcc_am3d_set_param_tbl(&dsp->dsp_ctrl, &param_tbl);

				if (copy_to_user((void*)arg, &param_tbl, sizeof(struct tcc_am3d_param_tbl_t)))
					break;
			}
			break;
		case IOCTL_TCC_AM3D_GET_PARAM_TABLE:
			{
				struct tcc_am3d_param_tbl_t param_tbl;
				if (copy_from_user(&param_tbl, (void*)arg, sizeof(struct tcc_am3d_param_tbl_t)))
					break;

				ret = tcc_am3d_get_param_tbl(&dsp->dsp_ctrl, &param_tbl);

				if (copy_to_user((void*)arg, &param_tbl, sizeof(struct tcc_am3d_param_tbl_t)))
					break;
			}
			break;
		default:
			break;
	}
	mutex_unlock(&dsp->m);
	return ret;
}

int tcc_dsp_open(struct inode *inode, struct file *flip)
{
	//struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	//struct tcc_dsp_t *dsp = (struct tcc_dsp_t*)dev_get_drvdata(misc->parent);
	
	return 0;
}

int tcc_dsp_release(struct inode *inode, struct file *flip)
{
	//struct miscdevice *misc = (struct miscdevice*)flip->private_data;
	//struct tcc_dsp_t *dsp = (struct tcc_dsp_t*)dev_get_drvdata(misc->parent);

	return 0;
}

static const struct file_operations tcc_dsp_fops =
{
	.owner          = THIS_MODULE,
	.read           = tcc_dsp_read,
	.write          = tcc_dsp_write,
	.unlocked_ioctl = tcc_dsp_ioctl,
	.open           = tcc_dsp_open,
	.release        = tcc_dsp_release,
};

static struct miscdevice tcc_dsp_misc_device =
{
	MISC_DYNAMIC_MINOR,
	"tcc_dsp",
	&tcc_dsp_fops,
};


static int tcc_dsp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_dsp_t *dsp = (struct tcc_dsp_t*)kzalloc(sizeof(struct tcc_dsp_t), GFP_KERNEL);

	tcc_dsp_ipc_initialize();
	tcc_dsp_ctrl_init(&dsp->dsp_ctrl, &pdev->dev);

	mutex_init(&dsp->m);

	platform_set_drvdata(pdev, dsp);

    {
        int dsp_sound_init(void);
        dsp_sound_init();
    }
    
	tcc_dsp_misc_device.parent = &pdev->dev;

	if (misc_register(&tcc_dsp_misc_device)) {
		printk(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}

	return ret;
}

static int tcc_dsp_remove(struct platform_device *pdev)
{
	printk("%s\n", __func__);
	return 0;
}

static int tcc_dsp_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("%s\n", __func__);
	return 0;
}

static int tcc_dsp_resume(struct platform_device *pdev)
{
	printk("%s\n", __func__);
	return 0;
}

static struct of_device_id tcc_dsp_of_match[] = {
	{ .compatible = "telechips,tcc_dsp" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_dsp_of_match);

static struct platform_driver tcc_dsp_driver = {
	.probe		= tcc_dsp_probe,
	.remove		= tcc_dsp_remove,
	.suspend	= tcc_dsp_suspend,
	.resume		= tcc_dsp_resume,
	.driver 	= {
		.name	= "tcc_dsp_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_dsp_of_match),
#endif
	},
};

static int __init tcc_dsp_init(void)
{
	return platform_driver_register(&tcc_dsp_driver);
}

static void __exit tcc_dsp_exit(void)
{
	platform_driver_unregister(&tcc_dsp_driver);
}
//------------------------------------------------------

extern void tc_dsp_setCurrAudioBuffer(unsigned long i2s_num, unsigned long CurrAudioBufferAddr,int playback );

void CA7s_IPC_AlsaMain_handler(struct tcc_dsp_ipc_data_t *ipc_data, void *pdata)
{
#if 0    
    printk("%s - %d\n", __func__, __LINE__);
    printk("id:0x%08x, type:0x%08x, param[0-5]:0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",
            ipc_data->id, ipc_data->type,
            ipc_data->param[0], ipc_data->param[1], ipc_data->param[2],
            ipc_data->param[3], ipc_data->param[4], ipc_data->param[5]);
#endif
    if(ipc_data->type == INFO_TYPE){
        if(ipc_data->param[0] == ALSA_OUT_BUF_PARAM){
#if 0
            printk("id:0x%08x, type:0x%08x,ALSA_OUT_BUF_PARAM = 0x%08x,CurrAddr 0x%08x\n",
                    ipc_data->id, ipc_data->type,
                    ipc_data->param[0], ipc_data->param[1]);
#endif
            tc_dsp_setCurrAudioBuffer(VIR_IDX_I2S0, ipc_data->param[1],1);
            {
                void tcc_dma_done_handler(int num, int dir);
                tcc_dma_done_handler(VIR_IDX_I2S0, 1);
            }
        }
    }
    
}

void CA7s_IPC_AlsaInMain_handler(struct tcc_dsp_ipc_data_t *ipc_data, void *pdata)
{
#if 0    
    printk("%s - %d\n", __func__, __LINE__);
    printk("id:0x%08x, type:0x%08x, param[0-5]:0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",
            ipc_data->id, ipc_data->type,
            ipc_data->param[0], ipc_data->param[1], ipc_data->param[2],
            ipc_data->param[3], ipc_data->param[4], ipc_data->param[5]);
#endif
    if(ipc_data->type == INFO_TYPE){
        if(ipc_data->param[0] == ALSA_IN_BUF_PARAM){
#if 0
            printk("id:0x%08x, type:0x%08x,ALSA_IN_BUF_PARAM = 0x%08x,CurrAddr 0x%08x\n",
                    ipc_data->id, ipc_data->type,
                    ipc_data->param[0], ipc_data->param[1]);
#endif
            tc_dsp_setCurrAudioBuffer(VIR_IDX_I2S3, ipc_data->param[1],0);
            {
                void tcc_dma_done_handler(int num, int dir);
                tcc_dma_done_handler(VIR_IDX_I2S3, 0);
            }
        }
    }
    
}

void CA7s_IPC_AlsaSub_handler(struct tcc_dsp_ipc_data_t *ipc_data, void *pdata)
{
#if 0   
    printk("%s - %d\n", __func__, __LINE__);
    printk("id:0x%08x, type:0x%08x, param[0-5]:0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",
            ipc_data->id, ipc_data->type,
            ipc_data->param[0], ipc_data->param[1], ipc_data->param[2],
            ipc_data->param[3], ipc_data->param[4], ipc_data->param[5]);
#endif
    if(ipc_data->type == INFO_TYPE){
        if(ipc_data->param[0] == ALSA_OUT_BUF_PARAM){
#if 0
            printk("id:0x%08x, type:0x%08x,ALSA_OUT_BUF_PARAM = 0x%08x,CurrAddr 0x%08x\n",
                    ipc_data->id, ipc_data->type,
                    ipc_data->param[0], ipc_data->param[1]);
#endif
            tc_dsp_setCurrAudioBuffer(VIR_IDX_I2S1, ipc_data->param[1],1);

            {
                void tcc_dma_done_handler(int num, int dir);
                tcc_dma_done_handler(VIR_IDX_I2S1, 1);
            }
        }
    }
    
}

void CA7s_IPC_AlsaSub2_handler(struct tcc_dsp_ipc_data_t *ipc_data, void *pdata)
{
#if 0 
    printk("%s - %d\n", __func__, __LINE__);
    printk("id:0x%08x, type:0x%08x, param[0-5]:0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",
            ipc_data->id, ipc_data->type,
            ipc_data->param[0], ipc_data->param[1], ipc_data->param[2],
            ipc_data->param[3], ipc_data->param[4], ipc_data->param[5]);
#endif
    if(ipc_data->type == INFO_TYPE){
        if(ipc_data->param[0] == ALSA_OUT_BUF_PARAM){
#if 0
            printk("id:0x%08x, type:0x%08x,ALSA_OUT_BUF_PARAM = 0x%08x,CurrAddr 0x%08x\n",
                    ipc_data->id, ipc_data->type,
                    ipc_data->param[0], ipc_data->param[1]);
#endif
            tc_dsp_setCurrAudioBuffer(VIR_IDX_I2S2, ipc_data->param[1],1);

            {
                void tcc_dma_done_handler(int num, int dir);
                tcc_dma_done_handler(VIR_IDX_I2S2, 1);
            }
        }
    }

}


int dsp_sound_init(void)
{
  	printk("%s\n", __func__);
	register_tcc_dsp_ipc(TCC_DSP_IPC_ID_ALSA_MAIN, CA7s_IPC_AlsaMain_handler, NULL, "CA7s_IPC_AlsaMain_handler");
	register_tcc_dsp_ipc(TCC_DSP_IPC_ID_ALSA_SUB01, CA7s_IPC_AlsaSub_handler, NULL, "CA7s_IPC_AlsaSub_handler");
	register_tcc_dsp_ipc(TCC_DSP_IPC_ID_ALSA_SUB02, CA7s_IPC_AlsaSub2_handler, NULL, "CA7s_IPC_AlsaSub2_handler");
	register_tcc_dsp_ipc(TCC_DSP_IPC_ID_ALSA_IN_MAIN, CA7s_IPC_AlsaInMain_handler, NULL, "CA7s_IPC_AlsaInMain_handler");
	return 0;
}


module_init(tcc_dsp_init);
module_exit(tcc_dsp_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips DSP Driver");
MODULE_LICENSE("GPL");

