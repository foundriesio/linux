#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/i-dxb/silabs/si2166.h>

#include <tcc_power_silabs.h>
#include <tcc_i2c.h>

#include "SiLabs_API_L3_Wrapper.h"

static int idxb_si2166_debug;
static int idxb_si2166_debug_cs;

module_param_named(isi2166_debug, idxb_si2166_debug, int, 0644);
module_param_named(isi2166_debug_cs, idxb_si2166_debug_cs, int, 0644);

MODULE_PARM_DESC(isi2166_debug, "Turn on/off si2166 debugging (default:off).");
MODULE_PARM_DESC(isi2166_debug_cs, "Turn on/off si2166 callstack debugging (default:off).");

#ifdef dprintk
#undef dprintk
#endif

#define dprintk(args...) \
	do { if(idxb_si2166_debug) { printk("[%s] ", __func__); printk(args); } } while (0)

#define DEBUG_CALLSTACK		\
	do { if(idxb_si2166_debug_cs) { printk("[Call] ----> %s()\n", __func__); } } while (0);

typedef struct tag_si2166_i2c_addr_t
{
	unsigned char uc_lnb;
	unsigned char uc_tuner;
	unsigned char uc_demod;
} si2166_i2c_addr_t;

typedef struct tag_idxb_si2166_t
{
	int i_n_fe;

	char str_msg[10000];
} idxb_si2166_t;

si2166_i2c_addr_t gt_i2c_addr[SILABS_FE_TYPE_MAX] = {
	{ 0x10, 0xc4, 0xc8 }, 
	{ 0x16, 0xc4, 0xce }, 	
	{ 0x00, 0x00, 0x00 }, 
	{ 0x00, 0x00, 0x00 }
};

idxb_si2166_t *gp_instance = NULL;

static long idxb_si2166_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int i;
	long l_retval = 0;
	
	idxb_si2166_t *p_instance = gp_instance;

	DEBUG_CALLSTACK

	switch(cmd)
	{
	case SI2166_IOCTL_INIT:
		{
			char str_tag[20];			

			silabs_fe_type_e e_type = (silabs_fe_type_e)arg;
			
			dprintk("[%s:%d] SI2166_IOCTL_INIT - %d\n", __func__, __LINE__, e_type);

			tcc_power_silabs_init(e_type);
			tcc_power_silabs_on();

			p_instance->i_n_fe = (int)(e_type + 1);

			for(i = 0; i < p_instance->i_n_fe; i++)
			{
				si2166_i2c_addr_t *p_i2c_addr = &gt_i2c_addr[i];
				
				SILABS_FE_Context *p_ctx = &FrontEnd_Table[i];

				sprintf(str_tag, "fe[%d]", i);

				SiLabs_API_Frontend_Chip(p_ctx, 0x2183);

				SiLabs_API_SW_Init(p_ctx, p_i2c_addr->uc_demod, 0xc0, p_i2c_addr->uc_tuner);

				SiLabs_API_Select_SAT_Tuner(p_ctx, 0xa2018, 0);

				SiLabs_API_SAT_Select_LNB_Chip(p_ctx, 0xA8304, p_i2c_addr->uc_lnb);
				SiLabs_API_SAT_tuner_I2C_connection(p_ctx, 0);
				SiLabs_API_SAT_Clock(p_ctx, 2, 44, 27, 2);
				SiLabs_API_SAT_Spectrum(p_ctx, 0);
				SiLabs_API_SAT_AGC(p_ctx, 0xa, 1, 0x0, 1);

				SiLabs_API_Set_Index_and_Tag(p_ctx, i, str_tag);

				SiLabs_API_HW_Connect(p_ctx, 2);
			}

			for(i = 0; i < p_instance->i_n_fe; i++)
			{
				char str_msg[100];

				unsigned char uc_rbuf[1024];
				
				L0_Context t_i2c;
				
				si2166_i2c_addr_t *p_i2c_addr = &gt_i2c_addr[i];

				SILABS_FE_Context *p_ctx = &FrontEnd_Table[i];

				L0_Init(&t_i2c);
				
				/* Check demod communication */
				sprintf(str_msg, "0x%02x 0x00 0x00 1", p_i2c_addr->uc_demod);

				if(L0_ReadString(&t_i2c, str_msg, uc_rbuf) != 1)
				{
					printk("[%s:%d] Demod communication error!\n", __func__, __LINE__);
					l_retval = -1;
					break;
				}

				/* Check tuner communication */
				SiLabs_API_Tuner_I2C_Enable(p_ctx);

				sprintf(str_msg, "0x%02x 0x00 1", p_i2c_addr->uc_tuner);

				if(L0_ReadString(&t_i2c, str_msg, uc_rbuf) != 1)
				{
					printk("[%s:%d] Tuner communication error!\n", __func__, __LINE__);
					l_retval = -1;
					break;
				}
			}
		}
		break;
	case SI2166_IOCTL_TUNE:
		{
			int i_lock = 2;
			
			SILABS_FE_Context *p_ctx;
			
			si2166_tune_params_t *p_params = (si2166_tune_params_t *)arg;
			si2166_tune_params_t t_params;

			if(copy_from_user(&t_params, p_params, sizeof(si2166_tune_params_t)))
			{
				printk("[%s:%d] copy_from_user() failed!\n", __func__, __LINE__);				
				l_retval = -1;
				break;
			}

			if(t_params.i_fe_id < 0 || t_params.i_fe_id >= p_instance->i_n_fe)
			{
				printk("[%s:%d] FE id error! - %d!\n", __func__, __LINE__, t_params.i_fe_id);				
				l_retval = -1;
				break;
			}

			p_ctx = &FrontEnd_Table[t_params.i_fe_id];

			p_ctx->polarization = (int)t_params.e_polar;
			p_ctx->band 		= (int)t_params.e_band_type;

			switch(t_params.e_polar)
			{
			case SILABS_POLAR_HORIZONTAL:
				{
					SiLabs_API_SAT_voltage_and_tone(p_ctx, 18, 0);
				}
				break;
			case SILABS_POLAR_VERTICAL:
				{
					SiLabs_API_SAT_voltage_and_tone(p_ctx, 13, 0);
				}
				break;
			default:
				{
					l_retval = -1;
					goto out;
				}
				break;
			}

			if(!SiLabs_API_switch_to_standard(p_ctx, (int)t_params.e_standard, 0))
			{
				printk("[%s:%d] SiLabs_API_switch_to_standard() failed!\n", __func__, __LINE__);
				l_retval = -1;
				break;
			}

			while(i_lock > 1)
			{
				i_lock = SiLabs_API_lock_to_carrier(
					p_ctx, 
					(int)t_params.e_standard, 
					t_params.i_frequency, 
					t_params.i_bandwidth_hz, 
					(unsigned int)t_params.e_stream, 
					t_params.ui_symbol_rate_bps, 
					(unsigned int)t_params.e_constel, 
					(unsigned int)t_params.e_polar, 
					(unsigned int)t_params.e_band_type, 
					t_params.i_data_slice_id, 
					t_params.i_plp_id, 
					t_params.i_t2_lock_mode);

				if(i_lock > 1)
				{
					dprintk("Console Lock: Handshaking after %6d ms\n", i_lock);
				}
			}

			if(i_lock == 1)
			{
				SiLabs_API_FE_status(p_ctx, &FE_Status);
				SiLabs_API_Text_status(p_ctx, &FE_Status, p_instance->str_msg);

				dprintk("%s", p_instance->str_msg);
			}

			SiLabs_API_TS_Mode(p_ctx, SILABS_TS_SERIAL);
		}
		break;
	default:
		{
		}
		break;
	}

out:

	return l_retval;
}

static int idxb_si2166_open(struct inode *inode, struct file *file)
{
	idxb_si2166_t *p_instance;
	
	DEBUG_CALLSTACK

	if(gp_instance != NULL)
	{
		printk("[%s:%d] instance is not NULL!\n", __func__, __LINE__);
		goto error;
	}

	p_instance = (idxb_si2166_t *)kcalloc(1, sizeof(idxb_si2166_t), GFP_KERNEL);

	if(p_instance == NULL)
	{
		printk("[%s:%d] kcalloc() failed!\n", __func__, __LINE__);
		goto error;
	}

	tcc_i2c_init();

	gp_instance = p_instance;

	return 0;

error:

	return -1;
}

static int idxb_si2166_release(struct inode *inode, struct file *file)
{
	idxb_si2166_t *p_instance = gp_instance;
	
	DEBUG_CALLSTACK

	tcc_i2c_deinit();

	tcc_power_silabs_off();
	tcc_power_silabs_deinit();

	kfree(p_instance);

	gp_instance = NULL;
	
	return 0;
}

struct file_operations isi2166_fops = {
	.owner 			= THIS_MODULE, 
	.unlocked_ioctl = idxb_si2166_ioctl, 
	.open 			= idxb_si2166_open, 
	.release 		= idxb_si2166_release
};

static int __init idxb_si2166_init(void)
{
	int retval;

	DEBUG_CALLSTACK

	if((retval = register_chrdev(SI2166_DEV_MAJOR, SI2166_DEV_NAME, &isi2166_fops)) < 0) {
		printk("register_chrdev() failed!\n");
		goto out;
	}

	dprintk("iDxB SI2166 init success\n");

out:

	return retval;
}

static void __exit idxb_si2166_exit(void)
{
	DEBUG_CALLSTACK

	unregister_chrdev(SI2166_DEV_MAJOR, SI2166_DEV_NAME);

	dprintk("iDxB SI2166 exit success\n");
}

module_init(idxb_si2166_init);
module_exit(idxb_si2166_exit);

MODULE_AUTHOR("JP");
MODULE_LICENSE("GPL");
