/*
 * IR driver for remote controller : tcc_remocon.c
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/capability.h>
#include <linux/uio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#endif

#include <linux/time.h>
#include "tcc_remocon.h"

//#define ENABLE_DEBUG_LOG
//#define ENABLE_DEBUG_FIFO_DATA

//+[TCCQB] QuickBoot Booting & Making flag
#if defined(CONFIG_HIBERNATION)
extern unsigned int do_hibernate_boot;
#endif
//-[TCCQB]
//

/*	You can modify below parameters via typing command line in console.
 *
 *  cat /sys/module/tcc_remocon/parameters/debug_log
 *  cat /sys/module/tcc_remocon/parameters/sleep_time
 *  cat /sys/module/tcc_remocon/parameters/timer_time
 *
 *	echo [value] > /sys/module/tcc_remocon/parameters/debug_log
 *	echo [value] > /sys/module/tcc_remocon/parameters/sleep_time
 *	echo [value] > /sys/module/tcc_remocon/parameters/timer_time
 *
 */
static int debug_log = 0;
module_param(debug_log, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug_log, "Enable/Disable Remote Debug log.");

static int sleep_time = REMOCON_SLEEP_TIME;
module_param(sleep_time, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sleep_time, "Set sleep_time for Data filled.");

static int timer_time = REMOCON_TIMER_TIME;
module_param(timer_time, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(timer_time, "Timer of Remocon to release key event.");

extern struct clk *clk_get(struct device *dev, const char *id);
extern int clk_enable(struct clk *clk);
extern void clk_disable(struct clk *clk);

unsigned long rmt_clk_rate = 0;

void __iomem *remote_base = NULL;
void __iomem *remote_cfg_base = NULL;

#define DEVICE_NAME	"tcc-remote"

#define TCC_IRVERSION	0.001

#define KEY_RELEASED	0
#define KEY_PRESSED		1


struct remote_data {
	struct device_node	*remote_np;
	struct clk			*pclk;
	struct clk			*hclk;
	int					 irq_port;
	unsigned int		 core_clk_rate;

	struct workqueue_struct *irq_work_queue;
	struct work_struct irq_work;
};

struct remote_data st_remote_data;

static unsigned int data = 0;
static unsigned int data_id = 0;
static unsigned int data_high = 0;
static unsigned int data_low = 0;
static unsigned int get_keycode;

static int irq_cnt = 0;
#if defined(ENABLE_DEBUG_FIFO_DATA)
int g_rd_data[1000];
int g_rd_vcnt[1000];
int g_rd_index[1000];
int gi=0;
static struct task_struct *g_remocon_fifo_thread=NULL;
static int fifo_checker(void *arg);
#endif


/*****************************************************************************
*
* structures
*
******************************************************************************/
struct tcc_remote
{
	unsigned int old_key;		//Previous KeyCode
	int status;		//Key Status

	struct input_dev *dev;  
};

typedef struct
{
	unsigned int repeatCount;
	unsigned int BitCnt;	//Data Bit Count
	unsigned long long Buf;	//Data Buffer
	unsigned int Id;	//Remocon ID
	unsigned int Code;	//Parsing Return Value
	unsigned int Stat;	//Remocon Status
}REM_DATA;

static int remote_state = 0;

static struct tcc_remote *rem;
static REM_DATA Rem;

static struct timer_list remocon_timer;

char output_lcdc_onoff = 1;

static int irq_flag = 0;

unsigned int tcc_remocon_GetTickCount(void)
{
	struct timeval tTimeVal;

	do_gettimeofday(&tTimeVal);

	(void)printk(KERN_INFO "[INFO][%s] %d sec %d usec\n", TCC_REMOCON_NAME, (int)tTimeVal.tv_sec, (int)tTimeVal.tv_usec);

	return tTimeVal.tv_sec * 1000 + tTimeVal.tv_usec / 1000;
}


/*****************************************************************************
* Function Name : static void remocon_timer_handler(unsigned long data);
* Description : timer event handler for key release.
* Arguments :
******************************************************************************/
static void remocon_timer_handler(unsigned long data)
{
	struct input_dev *dev = rem->dev;

	if(rem->status==KEY_PRESSED)
	{
		input_report_key(dev, rem->old_key, 0);
		input_sync(dev);
		if(debug_log==1)
			(void)printk(KERN_DEBUG "[DEBUG][%s] ############### nRem=%d released(timer)\n", TCC_REMOCON_NAME, rem->old_key);
		rem->status = KEY_RELEASED;
		rem->old_key = NO_KEY;
		Rem.Code = NO_KEY;
		Rem.Buf = 0;
	}
}

/*****************************************************************************
* Function Name : static int tca_rem_getkeycodebyscancode(unsigned short kc);
* Description : transform ScanCode into KeyCode.
* Arguments : kc - scancode
******************************************************************************/
static int tca_rem_getkeycodebyscancode(unsigned short kc)
{
	int i;
	for (i = 0;i < sizeof(key_mapping)/sizeof(SCANCODE_MAPPING);i++)
		if (kc == key_mapping[i].rcode) 
			return key_mapping[i].vkcode;
	return -1;
}

/*****************************************************************************
* Function Name : static void tca_rem_sendremocondata(unsigned int key_type);
* Description : key event is generated.
* Arguments : Data - key type(new or repeat)
******************************************************************************/
static void tca_rem_sendremocondata(unsigned int key_type)
{
	struct input_dev *dev = rem->dev;
	unsigned int nRem, repeatCheck = 1;
	//struct timeval tevent;

	nRem = get_keycode;

	switch(key_type) {
		case NEW_KEY:
			//nRem = tca_rem_getkeycodebyscancode(Rem.Code);
			if(nRem == -1 || (output_lcdc_onoff==0 && nRem!=REM_POWER))
				return;
			del_timer(&remocon_timer);
			if(rem->status==KEY_PRESSED) {
				if(debug_log == 1)
					(void)printk(KERN_DEBUG "[DEBUG][%s] ############### nRem=%d released\n", TCC_REMOCON_NAME, rem->old_key);
				input_report_key(dev, rem->old_key, 0);
				input_sync(dev);
				rem->status=KEY_RELEASED;
			}
			if(debug_log == 1)
				(void)printk(KERN_DEBUG "[DEBUG][%s] ############### nRem=%d pressed\n", TCC_REMOCON_NAME, nRem);
			rem->status = KEY_PRESSED;
			input_report_key(dev, nRem, 1);
			input_sync(dev);
			rem->old_key = nRem;
			Rem.repeatCount = 0;
			break;
		case REPEAT_KEY:
			repeatCheck = REMOCON_REPEAT_TH;
			if(output_lcdc_onoff==0 || !isRepeatableKey(rem->old_key))
				return;
			// Check Repeat interval time
			//		do_gettimeofday(&tevent);
			//		(void)printk(KERN_INFO "[INFO][%s] %d msec\n", TCC_REMOCON_NAME, ((int)tevent.tv_sec*1000)+((int)tevent.tv_usec/1000));
			if(rem->old_key == REM_POWER)
				repeatCheck = 15;

			del_timer(&remocon_timer);
			if(rem->status==KEY_PRESSED && Rem.repeatCount>repeatCheck) {
				if(debug_log == 1)
					(void)printk(KERN_DEBUG "[DEBUG][%s] ############### nRem=%d repeat=%d\n", TCC_REMOCON_NAME, rem->old_key, Rem.repeatCount);
				if(Rem.repeatCount % REMOCON_REPEAT_FQ == 0)
				{
					input_event(dev, EV_KEY, rem->old_key, 2);    // repeat event
					input_sync(dev);
				}
			}
			Rem.repeatCount++;
			break;
		default:
			break;
	}
	remocon_timer.expires = jiffies + msecs_to_jiffies(timer_time);
	add_timer(&remocon_timer);
}

/*****************************************************************************
 * Function Name : static irqreturn_t remocon_handler(int irq, void *dev);
 * Description : IR interrupt event handler
 * Arguments :
 ******************************************************************************/
static irqreturn_t remocon_handler(int irq, void *dev)
{
	irq_flag = 1;
	irq_cnt++;
	if((irq_cnt%2) == 1)
		return IRQ_HANDLED;

	queue_work(st_remote_data.irq_work_queue, &st_remote_data.irq_work);

	return IRQ_HANDLED;
}

static void tcc_remocon_irq_work_queue_func(struct work_struct *work)
{
	disable_irq_nosync(st_remote_data.irq_port);

	if(irq_flag==1)
	{
		data_low = 0;
		data_high = 0 ;
		data = 0;
		int key_type;

		int rd_data[IR_FIFO_READ_COUNT];
		int low;
		char low_bit='x';
		int low_err=0;
		int high=0;
		char high_bit='x';
		int high_err=0;
		int i=0;
		char f_low, f_high;

#if defined(CONFIG_PBUS_DIVIDE_CLOCK_WITHOUT_XTIN)
		int delay=0;
#endif
		Rem.Stat = STATUS0;

		{
			msleep(sleep_time);
			do
			{
				rd_data[i] = readl(remote_base);
				
				low = rd_data[i] & 0xffff;
				high = (rd_data[i] >> 16) & 0xffff;

				if ((low > LOW_MIN_VALUE) && (low < LOW_MAX_VALUE))
				{ 		
					low_bit='0';
					data_low = 0;
				}
				else if ((low > HIGH_MIN_VALUE) && (low < HIGH_MAX_VALUE))
				{
					low_bit='1';
					data_low = 1;
				}
				else if ((low > REPEAT_MIN_VALUE) && (low < REPEAT_MAX_VALUE))
				{
					low_bit='R';
				}
				else if ((low > START_MIN_VALUE) && (low < START_MAX_VALUE))
				{
					low_bit='S';
				}
				else
				{
					low_bit='E';
				}

				if ((high > LOW_MIN_VALUE) && (high < LOW_MAX_VALUE))
				{
					high_bit='0';
					data_high = 0;
				}
				else if ((high > HIGH_MIN_VALUE) && (high < HIGH_MAX_VALUE))
				{
					high_bit='1';
					data_high = 1;
				}
				else if ((high > REPEAT_MIN_VALUE) && (high < REPEAT_MAX_VALUE))
				{
					high_bit='R';
				}
				else if ((high > START_MIN_VALUE) && (high < START_MAX_VALUE))
				{
					high_bit='S';
				}
				else
				{
					high_bit='E';
				}

				if(i==0)
				{
					f_low=low_bit;
					f_high=high_bit;
				}
				if(low_bit == 'S' || high_bit=='S') {
					if(debug_log == 1)
						(void)printk(KERN_DEBUG "[DEBUG][%s] \n############### start\n", TCC_REMOCON_NAME);
				}
				if(debug_log == 1)
					(void)printk(KERN_DEBUG "[DEBUG][%s] %04x|%04x => %c%c  (%d)\n", TCC_REMOCON_NAME, low, high, low_bit, high_bit, i);
				if(low_bit == 'E')
				{
					low_err++;
					low_bit = 0;
				}

				if(high_bit == 'E')
				{
					high_err++;
					high_bit = 0;
				}
				/* Make SCAN_CODE from FIFO raw data. */
				if(i==0)
				{
					data = data | (data_high<<i);
				} else if (i==(IR_FIFO_READ_COUNT-1)) {
					data = data | (data_low<<(2*i-1));
				} else {
					data = data | (data_low<<(2*i-1));
					data = data | (data_high<<(2*i));
				}
			}
			while (++i < IR_FIFO_READ_COUNT);


#if defined (CONFIG_VERIZON)
			data = (data & 0xffff);
			data_id = REMOCON_ID;
#else
			data_id = (data & 0xfff);
			data = (data>>16);
#endif

			get_keycode = tca_rem_getkeycodebyscancode(data);
			if(debug_log==1)
			{
				(void)printk(KERN_DEBUG "[DEBUG][%s] IR SCAN CODE : 0x%x ID : 0x%x (%d) \n", TCC_REMOCON_NAME, data, data_id, get_keycode);
			}
			if(get_keycode > 0 && (readl(remote_base + REMOTE_BDR0_OFFSET) != 0) && (get_keycode != -1))
			{
				if(debug_log==1)
					(void)printk(KERN_DEBUG "[DEBUG][%s] Get New Key Code ! [%d] \n", TCC_REMOCON_NAME, get_keycode);
#if defined (CONFIG_VERIZON)
				if(data_id == REMOCON_ID){
#else
				if(data_id == (REMOCON_ID & 0xfff)){
#endif
					key_type = NEW_KEY;
				}else{
					key_type = REPEAT_KEY;
				}

			}else if((f_low == '0') && (f_high=='E')){
				key_type = REPEAT_KEY;
			}else{
				key_type = REPEAT_KEY;
			}

			if (key_type!=NO_KEY)
			{
				tca_rem_sendremocondata(key_type);
			}
		}
		if(debug_log==1)
		{
			tca_remocon_reg_dump();
		}

		RemoconInit(remote_state);
		irq_flag = 0;
	}else{
		irq_flag = 0;
	}
	enable_irq(st_remote_data.irq_port);
}

/*****************************************************************************
* Name :  Device register
* Description : This functions register device
* Arguments :
******************************************************************************/
static int remocon_probe(struct platform_device *pdev)
{
	int ret = -1;
	int err = -ENOMEM, i;
	unsigned int get_gpio = 0;
	unsigned int default_cfg = 10; //GPIO_C29 , RMISEL[0:2].
	struct device_node *np = pdev->dev.of_node;
	struct input_dev *input_dev;

	remote_base = of_iomap(np,0);
	remote_cfg_base = of_iomap(np,1);

	(void)printk(KERN_INFO "[INFO][%s] Get remocon driver [base address : 0x%08x] [cfg address : 0x%08x] \n", TCC_REMOCON_NAME, remote_base, remote_cfg_base);

	if (!pdev->dev.of_node){
		struct clk *remote_clk = clk_get(NULL, "remocon");
		if(IS_ERR(remote_clk)) {
			(void)printk(KERN_ERR "[ERROR][%s] can't find remocon clk driver!", TCC_REMOCON_NAME);
			remote_clk = NULL;
		} else {
			clk_enable(remote_clk);
			tcc_remocon_rmcfg(default_cfg);
		clk_set_rate(remote_clk, 24000000/4);

		rmt_clk_rate = clk_get_rate(remote_clk);
		(void)printk(KERN_INFO "[INFO][%s] ############## %s: remote clk_rate = %lu\n", TCC_REMOCON_NAME, __func__, rmt_clk_rate);
		}
	}
	else{
		st_remote_data.remote_np = pdev->dev.of_node;

		st_remote_data.irq_port = platform_get_irq(pdev, 0);
		st_remote_data.hclk = of_clk_get(st_remote_data.remote_np, 1);
		st_remote_data.pclk = of_clk_get(st_remote_data.remote_np, 0);

		if (IS_ERR(st_remote_data.pclk) ||IS_ERR(st_remote_data.hclk) ) {
			(void)printk(KERN_ERR "[ERROR][%s] REMOTE: failed to get remote clock\n", TCC_REMOCON_NAME);
			st_remote_data.pclk = NULL;
			st_remote_data.hclk = NULL;
			return -ENODEV;
		}
		
		if (st_remote_data.pclk && st_remote_data.hclk){
			clk_prepare_enable(st_remote_data.pclk);
			clk_prepare_enable(st_remote_data.hclk);
			
			of_property_read_u32(st_remote_data.remote_np, "clock-frequency", &st_remote_data.core_clk_rate);

			(void)printk(KERN_INFO "[INFO][%s] Telechips Remote Controller [irq_port:%d], [core_clk_rate:%d]\n", TCC_REMOCON_NAME, st_remote_data.irq_port, st_remote_data.core_clk_rate);

			of_property_read_u32(st_remote_data.remote_np, "wakeupsrc",&get_gpio);
			tcc_remocon_rmcfg(get_gpio);
			clk_set_rate(st_remote_data.pclk, st_remote_data.core_clk_rate/4);
				
			rmt_clk_rate = clk_get_rate(st_remote_data.pclk);
			(void)printk(KERN_INFO "[INFO][%s] %s: remote clk_rate = %lu\n", TCC_REMOCON_NAME, __func__, rmt_clk_rate);
		}
	}

	rem = kzalloc(sizeof(struct tcc_remote), GFP_KERNEL);
	input_dev = input_allocate_device();

	if (!rem || !input_dev)
	{
		err = -ENOMEM;
		goto error_alloc;
	}

	platform_set_drvdata(pdev, rem);

	rem->dev = input_dev;

	rem->dev->name = "telechips_remote_controller";
	rem->dev->phys = DEVICE_NAME;
	rem->dev->evbit[0] = BIT(EV_KEY);
	rem->dev->id.version = TCC_IRVERSION;
	input_dev->dev.parent = &pdev->dev;
	for (i = 0; i < ARRAY_SIZE(key_mapping); i++)
	{
		set_bit(key_mapping[i].vkcode & KEY_MAX, rem->dev->keybit);
	}

	//Init_IR_Port();
	RemoconConfigure ();
	RemoconStatus();
	RemoconDivide(remote_state);		//remocon clk divide and end_cout
	RemoconCommandOpen();
	RemoconIntClear();

	//Init IR variable
	rem->old_key = -1;
	rem->status = KEY_RELEASED;

	init_timer(&remocon_timer);
	remocon_timer.data = (unsigned long)NULL;
	remocon_timer.function = remocon_timer_handler;

	tca_remocon_set_pbd();

	st_remote_data.irq_work_queue = create_singlethread_workqueue("tcc_remote_irq_queue");
	if(st_remote_data.irq_work_queue)
	{
		INIT_WORK(&st_remote_data.irq_work, tcc_remocon_irq_work_queue_func);
#if defined(TCC_PM_SLEEP_WFI_USED) && defined(CONFIG_SLEEP_MODE)
	(void)printk(KERN_INFO "[INFO][%s] IR request_irq with WFI mode\n", TCC_REMOCON_NAME);
	ret =  request_threaded_irq(st_remote_data.irq_port, NULL , remocon_handler ,IRQF_ONESHOT, DEVICE_NAME, rem);
#else
	(void)printk(KERN_INFO "[INFO][%s] IR request_irq\n", TCC_REMOCON_NAME);
	ret =  request_threaded_irq(st_remote_data.irq_port, NULL , remocon_handler ,IRQF_ONESHOT, DEVICE_NAME, rem);
#endif
	}

	if (ret)
	{
		(void)printk(KERN_ERR "[ERROR][%s] IR remote request_irq error\n", TCC_REMOCON_NAME);
		goto error_irq;
	}

	ret = input_register_device(rem->dev);
	if (ret) 
		goto error_register;

#if defined(ENABLE_DEBUG_FIFO_DATA)
    if(g_remocon_fifo_thread == NULL){ 
        g_remocon_fifo_thread = (struct task_struct *)kthread_run(fifo_checker, NULL, "remocon_fifo_checker");
    }
#endif

	return 0;

error_alloc:
	input_free_device(input_dev);

error_irq:
	free_irq(st_remote_data.irq_port, rem);

error_register:
	input_unregister_device(rem->dev);

	return ret;
}

static int remocon_remove(struct platform_device *pdev)
{
	flush_workqueue(st_remote_data.irq_work_queue);
	destroy_workqueue(st_remote_data.irq_work_queue);
	disable_irq(st_remote_data.irq_port);
	del_timer(&remocon_timer);
	free_irq(st_remote_data.irq_port, rem->dev);
	input_unregister_device(rem->dev);
	kfree(rem);

	if (st_remote_data.pclk && st_remote_data.hclk){
		clk_disable_unprepare(st_remote_data.pclk);
		clk_disable_unprepare(st_remote_data.hclk);
		st_remote_data.pclk = NULL;
		st_remote_data.hclk = NULL;
	}

	return 0;
}

#ifdef CONFIG_PM
static int remocon_suspend(struct platform_device *pdev, pm_message_t state)
{
	tca_remocon_suspend();

	return 0;
}

static int remocon_resume(struct platform_device *pdev)
{
	unsigned int get_gpio;
//+[TCCQB] Quickboot resume
  #if defined(CONFIG_HIBERNATION)
	if( do_hibernate_boot) {
		//Init_IR_Port();
		RemoconConfigure ();
		RemoconStatus();
		RemoconDivide(remote_state);		//remocon clk divide and end_cout
		RemoconCommandOpen();
		RemoconIntClear();
	}
  #endif
//-[TCCQB]
//

//*
	//Init_IR_Port();
	RemoconConfigure ();
	RemoconStatus();
	RemoconDivide(remote_state);		//remocon clk divide and end_cout
	RemoconCommandOpen();	
	RemoconIntClear();
//*/
	st_remote_data.remote_np = pdev->dev.of_node;
	of_property_read_u32(st_remote_data.remote_np, "wakeupsrc",&get_gpio);
	tcc_remocon_rmcfg(get_gpio);

	tca_remocon_resume();

	remote_state = 0;
	RemoconInit(remote_state);

	return 0;
}
#else
#define remocon_suspend NULL
#define remocon_resume NULL
#endif

#ifdef CONFIG_OF
static struct of_device_id tcc_remote_of_match[] = {
	{ .compatible = "telechips,tccxxxx-remote" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_remote_of_match);
#endif

static struct platform_driver remocon_driver =
{
	.driver	=
	{
		.name	= DEVICE_NAME,
		.owner 	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_remote_of_match)
	},
	.probe		= remocon_probe,
	.remove		= remocon_remove,
#ifdef CONFIG_PM
	.suspend		= remocon_suspend,
	.resume		= remocon_resume,
#endif
};

static int __init remocon_init(void)
{
	(void)printk(KERN_INFO "[INFO][%s] Telechips Remote Controller Driver Init\n", TCC_REMOCON_NAME);
	return platform_driver_register(&remocon_driver);
}

static void __exit remocon_exit(void)
{
	(void)printk(KERN_INFO "[INFO][%s] Telechips Remote Controller Driver Exit \n", TCC_REMOCON_NAME);
	platform_driver_unregister(&remocon_driver);
}

module_init(remocon_init);
module_exit(remocon_exit);

MODULE_AUTHOR("Linux Team<linux@telechips.com>");
MODULE_DESCRIPTION("IR remote control driver");
MODULE_LICENSE("GPL");
