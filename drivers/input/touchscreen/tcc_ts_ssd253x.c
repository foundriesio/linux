/****** You need to define some conditions ******/
/* 1. SSL_DEBUG: print debug information of this driver */
/* 2. CAL_THRESHOLD: auto-calibration threshold, it should be negative */
/* 3. CAL_INTERVAL: idle time between auto-calibration checking	*/
/* 4. CAL_COUNT: auto-calibration will check condition for CAL_COUNT seconds */
/* 5. TOUCH_INT_PIN and TOUCH_RES_PIN: GPIO for IRQ and RESET signal */
/* 6. HAS_BUTTON: if there is any button */
/* 7. SIMULATED_BUTTON: if buttons are simulated by touch area */
/* 8. FINGER_USED: define finger count in system */
/*  Modify VARIKOREA for TLM-P706T2120 & VK-PM8801 jackson, sam  */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#if LINUX_VERSION_CODE>= KERNEL_VERSION(3,0,0)
#include <linux/input/mt.h>
#endif
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
//#include <asm/mach-types.h>
//#include <asm/system.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
//#include "tcc_ts.h"
/****** output debug information ******/
//#define SSL_DEBUG

#ifdef SSL_DEBUG
#define SSL_PRINT(x...)		printk("SSD253X:"x)
#else
#define SSL_PRINT(x...)
#endif

#define GPIO_HIGH 1
#define GPIO_LOW  0

#define PVK0010 //PVK0010-01 TP(tcc8021 evb & tcc8971-battleship)
/****** auto-calibration conditions (only applied to SSD2531) ******/
/****** define auto-calibration threshold ******/
#define CAL_THRESHOLD -100
/****** define auto-calibration checking interval ******/
#define CAL_INTERVAL	50	//in ms
/****** define auto-calibration checking times ******/
#define CAL_COUNT		35	//check 35 seconds after boot-up/sleep out
/****** define the number of finger that used to report coodinates ******/
#define FINGER_USED		10

/****** define your RESET and IRQ input here ******/
//Touch //for VK-PM8935-0002
static int ssd253x_rst_port = (int)NULL;
static int ssd253x_int_port = (int)NULL;
static int ssd253x_int_irq = (int)NULL;
static int ssd253x_off = 0;
/****** define your button codes ******/
//#define HAS_BUTTON
//#define SIMULATED_BUTTON

#define BUTTON0	59	//menu
#define BUTTON1	158	//back
#define BUTTON2	217	//search
#define BUTTON3	102	//home

typedef struct{
	unsigned long x;	//center x of button
	unsigned long y;	//center y of button
	unsigned long dx;	//x range (x-dx)~(x+dx)
	unsigned long dy;	//y range (y-dy)~(y+dy)
	unsigned long code;	//key code for simulated key
}SKey_Info,*pSKey_Info;

/****** define simulated button information ******/
#ifdef SIMULATED_BUTTON
SKey_Info SKeys[]={
{45,530,20,16,BUTTON0},
{160,530,20,16,BUTTON3},
{275,530,20,16,BUTTON1},
};
#endif

#define REG_DEVICE_ID		    0x02
#define FINGER_STATUS           0x79
#define FINGER_DATA             0x7C
#define BUTTON_RAW				0xB5
#define BUTTON_STATUS		   	0xB9

enum CMD_TYPE{CMD_DELAY=-1,CMD_ONLY=0,CMD_1B=1,CMD_2B=2};
typedef struct{
	enum CMD_TYPE type;
	unsigned int reg;
	unsigned char data1;
	unsigned char data2;
}Reg_Item,*pReg_Item;

/****** The following block is TP configuration, please get it from your TP vendor ******/
/****** TP configuration starts here ******/
#define LCD_RANGE_X 800
#define LCD_RANGE_Y 480

#if defined(PVK0010)
Reg_Item ssd2533_Init[]={   //PVK0010-01 TP initial
    {CMD_ONLY,0x04},            //Exit Sleep Mode
    {CMD_DELAY, 10/*50*/},

    {CMD_1B,0x06,0x10},         //Set Drive Line
    {CMD_1B,0x07,0x1B},         //Set Sense Line

    {CMD_2B,0x08,0x00,0x84},    //Set 5 drive line reg
    {CMD_2B,0x09,0x00,0x85},    //Set 6 drive line reg
    {CMD_2B,0x0A,0x00,0x86},    //Set 7 drive line reg
    {CMD_2B,0x0B,0x00,0x87},    //Set 8 drive line reg

    {CMD_2B,0x0C,0x00,0x88},    //Set 9 drive line reg
    {CMD_2B,0x0D,0x00,0x89},    //Set 10 drive line reg
    {CMD_2B,0x0E,0x00,0x8A},    //Set 11 drive line reg
    {CMD_2B,0x0F,0x00,0x8B},    //Set 12 drive line reg
    {CMD_2B,0x10,0x00,0x8C},    //Set 13 drive line reg
    {CMD_2B,0x11,0x00,0x8D},    //Set 14 drive line reg
    {CMD_2B,0x12,0x00,0x8E},    //Set 15 drive line reg
    {CMD_2B,0x13,0x00,0x8F},    //Set 16 drive line reg
    {CMD_2B,0x14,0x00,0x90},    //Set 17 drive line reg
    {CMD_2B,0x15,0x00,0x91},    //Set 18 drive line reg
    {CMD_2B,0x16,0x00,0x92},    //Set 19 drive line reg
    {CMD_2B,0x17,0x00,0x93},    //Set 20 drive line reg
    {CMD_2B,0x18,0x00,0x94},    //Set 21 drive line reg

    {CMD_1B,0xD5,0x03},         //Set Driving voltage 0(5.5V)to 7(9V)
    {CMD_1B,0xD8,0x07},         //Set Sense Resistance

    {CMD_1B,0x2A,0x07},         //Set sub-frame (N-1)default=1, range 0 to F
    {CMD_1B,0x2C,0x01},
    {CMD_1B,0x2E,0x0B},         //Sub-frame Drive pulse number (N-1) default=0x17
    {CMD_1B,0x2F,0x01},         //Integration Gain (N-1) default=2, 0 to 4

    {CMD_1B,0x30,0x05},         //start integrate 125ns/div
    {CMD_1B,0x31,0x08},         //stop integrate 125ns/div
    {CMD_1B,0xD7,0x02},         //ADC range default=4, 0 to 7
    {CMD_1B,0xDB,0x01},         //Set integration cap default=0, 0 to 7

    {CMD_2B,0x33,0x00,0x01},    //Set Min. Finger area
    {CMD_2B,0x34,0x00,0x58},    //Set Min. Finger level
    {CMD_2B,0x35,0x00,0x1F},    //Set Min, Finger weight
    {CMD_2B,0x36,0xFF,0x18},    //Set Max, Finger weight

    {CMD_1B,0x37,0x07},         //Segmentation Depth
    {CMD_1B,0x3D,0x01},
    {CMD_1B,0x53,0x16},         //Event move tolerance

    {CMD_2B,0x54,0x00,0x80},    //X tracking
    {CMD_2B,0x55,0x00,0x80},    //Y tracking

    {CMD_1B,0x65,0x00},         //Touch Coordinate Mapping Default = 4, VARIKOREA PM8801 = 1, VARIKOREA PM8935 = 0

    {CMD_2B,0x66,0x72,0x10},    //set Sense scale
    {CMD_2B,0x67,0x70,0xA0},    //set Driver scale

    {CMD_1B,0x56,0x02},
    {CMD_1B,0x59,0x01},         //Enable Random walk
    {CMD_1B,0x5B,0x03},         //Set Random walk window

	{CMD_2B,0x7A,0xFF,0xFF},
    {CMD_2B,0x7B,0x00,0x03},

    {CMD_1B,0x8A,0x0A},
    {CMD_1B,0x8B,0x10},
    {CMD_1B,0x8C,0xB0},

    {CMD_1B,0x25,0x02},
    {CMD_DELAY, 25/*300*/},
    {CMD_ONLY,0x00},
};
#else
Reg_Item ssd2533_Init[]={   //first Release 7.0" Android Initial
    {CMD_ONLY,0x04},            //Exit Sleep Mode
    {CMD_DELAY, 25},

    {CMD_1B,0x06,0x14},         //Set Drive Line
    {CMD_1B,0x07,0x1B},         //Set Sense Line

    {CMD_2B,0x08,0x00,0x80},    //Set 1 drive line reg
    {CMD_2B,0x09,0x00,0x81},    //Set 2 drive line reg
    {CMD_2B,0x0A,0x00,0x82},    //Set 3 drive line reg
    {CMD_2B,0x0B,0x00,0x83},    //Set 4 drive line reg
    {CMD_2B,0x0C,0x00,0x84},    //Set 5 drive line reg
    {CMD_2B,0x0D,0x00,0x85},    //Set 6 drive line reg
    {CMD_2B,0x0E,0x00,0x86},    //Set 7 drive line reg
    {CMD_2B,0x0F,0x00,0x87},    //Set 8 drive line reg

    {CMD_2B,0x10,0x00,0x88},    //Set 9 drive line reg
    {CMD_2B,0x11,0x00,0x89},    //Set 10 drive line reg
    {CMD_2B,0x12,0x00,0x8A},    //Set 11 drive line reg
    {CMD_2B,0x13,0x00,0x8B},    //Set 12 drive line reg
    {CMD_2B,0x14,0x00,0x8C},    //Set 13 drive line reg
    {CMD_2B,0x15,0x00,0x8D},    //Set 14 drive line reg
    {CMD_2B,0x16,0x00,0x8E},    //Set 15 drive line reg
    {CMD_2B,0x17,0x00,0x8F},    //Set 16 drive line reg
    {CMD_2B,0x18,0x00,0x90},    //Set 17 drive line reg
    {CMD_2B,0x19,0x00,0x91},    //Set 18 drive line reg
    {CMD_2B,0x1A,0x00,0x92},    //Set 19 drive line reg
    {CMD_2B,0x1B,0x00,0x93},    //Set 20 drive line reg
    {CMD_2B,0x1C,0x00,0x94},    //Set 21 drive line reg

    {CMD_1B,0xD5,0x03},         //Set Driving voltage 0(5.5V)to 7(9V)
    {CMD_DELAY, 5},
    {CMD_1B,0xD8,0x07},         //Set Sense Resistance
    {CMD_DELAY, 5},

    {CMD_1B,0x2A,0x07},         //Set sub-frame (N-1)default=1, range 0 to F
    {CMD_1B,0x2C,0x01},
    {CMD_1B,0x2E,0x0B},         //Sub-frame Drive pulse number (N-1) default=0x17
    {CMD_1B,0x2F,0x01},         //Integration Gain (N-1) default=2, 0 to 4

    {CMD_1B,0x30,0x07},         //start integrate 125ns/div
    {CMD_1B,0x31,0x0B},         //stop integrate 125ns/div
    {CMD_1B,0xD7,0x00},         //ADC range default=4, 0 to 7
    {CMD_1B,0xDB,0x00},         //Set integration cap default=0, 0 to 7

    {CMD_2B,0x33,0x00,0x01},    //Set Min. Finger area
    {CMD_2B,0x34,0x00,0x48},    //Set Min. Finger level
    {CMD_2B,0x35,0x00,0x1F},    //Set Min, Finger weight
    {CMD_2B,0x36,0xFF,0x18},    //Set Max, Finger weight

    {CMD_1B,0x37,0x00},         //Segmentation Depth
    {CMD_1B,0x3D,0x01},
    {CMD_1B,0x53,0x16},         //Event move tolerance

    {CMD_2B,0x54,0x00,0x80},    //X tracking
    {CMD_2B,0x55,0x00,0x80},    //Y tracking

    {CMD_1B,0x65,0x00},         //Touch Coordinate Mapping Default = 4, VARIKOREA PM8801 = 1, VARIKOREA PM8935 = 0

    {CMD_2B,0x66,0x72,0x40/*30*/},    //set Sense scale
    {CMD_2B,0x67,0x5B,0x30},    //set Driver scale

    {CMD_1B,0x56,0x02},
    {CMD_1B,0x58,0x00},         //Finger weight scaling
    {CMD_1B,0x59,0x01},         //Enable Random walk
    {CMD_1B,0x5A,0x01},         //Disable Missing Frame
    {CMD_1B,0x5B,0x03},         //Set Random walk window

//  {CMD_2B,0x7A,0xFF,0xFF},
    {CMD_2B,0x7A,0xFF,0xC7},
    {CMD_2B,0x7B,0xC0,0x0F},

    {CMD_1B,0x8A,0x0A},
    {CMD_1B,0x8B,0x10},
    {CMD_1B,0x8C,0xB0},

    {CMD_1B,0x25,0x02},
    {CMD_DELAY, 25},
    {CMD_ONLY,0x00},
};
#endif

//static int ssd253x_ts_open(struct input_dev *dev);
//static void ssd253x_ts_close(struct input_dev *dev);
static irqreturn_t ssd253x_ts_isr(int irq, void *dev_id);
static enum hrtimer_restart ssd253x_ts_timer(struct hrtimer *timer);
//static int print_read_time();

static struct workqueue_struct *ssd253x_wq;

struct ssl_ts_priv {
    spinlock_t irq_lock;
	struct i2c_client *client;
	struct input_dev *input;
	struct hrtimer timer;
	struct work_struct  ssl_work;
	u32 fingers;
	u32 buttons;
    s32 irq_is_disable;
	int cal_counter;
	int cal_tick;
	int irq;
	int prev_delta;
	int prev_x[10];
	int prev_y[10];
	int est_en[10];
	int est_x[10];
	int est_y[10];
	u8	keys[4];
	u8	keyindex[4];
	u32 fingerbits;
	unsigned long touchtime[10];
	unsigned long timemark;
	u32	tp_id;
	unsigned long cal_end_time;
	int finger_count;
	int working_mode;
	int drives;
	int senses;
	u8	r25h;
	u8	bsleep;
	u8	binitialrun;
	u8	bcalibrating;
	u8	rawout;
};

#define PROCFS_NAME "ssd253x-mode"

struct ssl_ts_priv *ssl_priv_global;
static void ssd253x_i2c_reset(void);

void ssd253x_reset(s32 ms)
{
    gpio_direction_output(ssd253x_rst_port, 0);
    msleep(ms);
    gpio_direction_input(ssd253x_rst_port);
    //msleep(20);
	usleep_range(25, 30);
}

void ssd253x_irq_enable(struct ssl_ts_priv *ssl_priv)
{   
    unsigned long irqflags;

    spin_lock_irqsave(&ssl_priv->irq_lock, irqflags);
    if (ssl_priv->irq_is_disable) 
    {
        enable_irq(ssl_priv->client->irq);
        ssl_priv->irq_is_disable = 0; 
    }
    spin_unlock_irqrestore(&ssl_priv->irq_lock, irqflags);
}

void ssd253x_irq_disable(struct ssl_ts_priv *ssl_priv)
{   
    unsigned long irqflags;

    spin_lock_irqsave(&ssl_priv->irq_lock, irqflags);
    if (!ssl_priv->irq_is_disable)
    {
        ssl_priv->irq_is_disable = 1; 
        disable_irq_nosync(ssl_priv->client->irq);
    }
    spin_unlock_irqrestore(&ssl_priv->irq_lock, irqflags);
}

static void ssd253x_i2c_reset(void)
{
    //  reset
    gpio_direction_output(ssd253x_rst_port, 0);
    //msleep(20);
	usleep_range(25,30);
    gpio_set_value_cansleep(ssd253x_rst_port,GPIO_LOW);
    //msleep(10);
	usleep_range(25,30);
    gpio_set_value_cansleep(ssd253x_rst_port,GPIO_HIGH);
    //mdelay(10);
	udelay(30);
}

/* Read data from register
* return 0, if success
* return negative errno, if error occur */
static int ssd253x_read_reg(
struct i2c_client *client,
u8 reg_index, /* start reg_index */
u8* rbuff, /* buffer to restore data be read */
u8 rbuff_len, /* the size of rbuff */
u8 length) /* how many bytes will be read */
{
	int ret = 0;
	u8 idx[2];
	struct i2c_msg msgs[2];
	/* send reg index */
	idx[0] = reg_index;
	idx[1] = 0x0;
	/* msg[0]: write reg command */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0; /* i2c write */
	msgs[0].len = 1;
	msgs[0].buf = idx;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD; /* i2c read */
	msgs[1].len = length;
	msgs[1].buf = rbuff;

	if (length > rbuff_len){
		return -EINVAL;
	}
	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0){
		SSL_PRINT("Error in %s, reg is 0x%02X.\n",__FUNCTION__,reg_index);
		return -EIO;
	}
	return 0;
}

/* write command & parameters
* return 0, if success
* return negative errno, if error occur */
static int ssd253x_write_cmd(
struct i2c_client *client,
u8 command,	/* start reg_index */
u8* wbuff,		/* buffer to write */
u8 length)		/* how many byte will be write */
{
	int ret = 0;
	struct i2c_msg msg;
	u8 buf[128];
	buf[0] = command;

	if (length > 126){
		return -1;
	}

	memcpy(&buf[1], wbuff, length);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = length+1;
	msg.buf = buf;

	/* step1. send reg index */
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0){
		SSL_PRINT("Error in %s, reg is 0x%02X. len=%d\n",__FUNCTION__,command,(int)length);
		return -EIO;
	}

	return 0;
}
/*static void print_read_time(void)
{
    struct timeval now;
    int temp, second, minute, hour;
    do_gettimeofday(&now);
    temp = now.tv_sec;
    second = temp%60;
    temp /= 60;
    minute = temp%60;
    temp /= 60;
    hour = temp%24;
    SSL_PRINT("TIME = [ %02d : %02d ]", minute, second);
}*/

static int ssd253x_read_id(struct i2c_client *client)
{
	u8 buf[2];
	u16 ic_id;
	buf[0]=0;
	ssd253x_write_cmd(client,0x04,buf,0);
	mdelay(10);
	ssd253x_read_reg(client,0x02,&buf[0],2,2);

	ic_id=buf[0]<<8|buf[1];
	if(ic_id==0x2533)
		return ic_id;
	buf[0]=0x0;
	ssd253x_write_cmd(client,0x23,buf,1);
	mdelay(10);
	buf[0]=0x02;
	ssd253x_write_cmd(client,0x2b,buf,1);
	mdelay(10);
	ssd253x_read_reg(client,0x02,&buf[0],2,2);
	ic_id=buf[0]<<8|buf[1];
	if(ic_id==0x2531||ic_id==0x2528)
		return ic_id;
	else
		return 0;
}

int deviceInit(struct ssl_ts_priv *ssl_priv)
{
	int err = 0;
	int index;
	pReg_Item pReg;
	int InitSize;
	u8 data_buf[2];
	struct i2c_client *client = ssl_priv->client;
	gpio_set_value_cansleep(ssd253x_rst_port,GPIO_LOW);
	//mdelay(10);
	udelay(30);
	gpio_set_value_cansleep(ssd253x_rst_port,GPIO_HIGH);
	//mdelay(10);
	udelay(30);
    //ssd253x_reset(20);
    ssd253x_reset(1);
	if(ssl_priv->tp_id==0x2533){
		pReg=ssd2533_Init;
		InitSize=sizeof(ssd2533_Init)/sizeof(Reg_Item);
	}
/*	else if(ssl_priv->tp_id==0x2531||ssl_priv->tp_id==0x2528){
		pReg=ssd2531_Init;
		InitSize=sizeof(ssd2531_Init)/sizeof(Reg_Item);
	}
*/
	else
		return -1;
	for (index=0; index<InitSize;index++){
		switch(pReg->type){
		case CMD_DELAY:
			mdelay(pReg->reg);
			break;
		default:
			{
				if(pReg->reg==0x25)
					ssl_priv->r25h=pReg->data1;
				else if(pReg->reg==0x06){
					if(ssl_priv->tp_id==0x2533)
						ssl_priv->drives=pReg->data1+1;
					else
						ssl_priv->drives=pReg->data1+6;
				}
				else if(pReg->reg==0x07){
					if(ssl_priv->tp_id==0x2533)
						ssl_priv->senses=pReg->data1+1;
					else
						ssl_priv->senses=pReg->data1+6;
				}
				data_buf[0] = pReg->data1;
				data_buf[1] = pReg->data2;

				if(pReg->reg!=0x25){
					err = ssd253x_write_cmd(client, pReg->reg, &data_buf[0], (int)(pReg->type));
					//mdelay(1);
				}
				break;
			}
		}
		pReg++;
	}
	return 0;
}

void StartWork(struct ssl_ts_priv *ssl_priv)
{
	u8 buf[2];
	buf[0]=ssl_priv->r25h;
	ssd253x_write_cmd(ssl_priv->client,0x25,buf,1);
	ssl_priv->cal_tick=1-(500/CAL_INTERVAL);
	ssl_priv->fingers=0;
	ssl_priv->buttons=0;
	ssl_priv->fingerbits=0;
	if(ssl_priv->tp_id==0x2531)
		ssl_priv->binitialrun=1;
	ssl_priv->cal_counter=0;
	ssl_priv->cal_end_time=jiffies+HZ*CAL_COUNT;
	hrtimer_start(&ssl_priv->timer, ktime_set(0, 300*1000000), HRTIMER_MODE_REL); //check status after 300ms
	ssl_priv->bsleep=0;
}

void deviceWakeUp(struct ssl_ts_priv *ssl_priv)
{
	struct pinctrl *pinctrl;
	deviceInit(ssl_priv);
	StartWork(ssl_priv);
    gpio_direction_output(ssd253x_int_port, 0);
    //msleep(2);
	usleep_range(25, 30);
    gpio_direction_input(ssd253x_int_port);
    //msleep(2);
	usleep_range(25, 30);
    gpio_direction_output(ssd253x_int_port, 0);
	#if 0//defined(CONFIG_ARCH_TCC897X)
    tcc_gpio_config(ssd253x_int_port , GPIO_FN(0));
	#else
	pinctrl = pinctrl_get_select(&ssl_priv->client->dev, "default");
	if(IS_ERR(pinctrl))
	   printk("%s: pinctrl select failed\n", "tcc-ts-ssd253x");
	else
		pinctrl_put(pinctrl);
	#endif
    gpio_direction_input(ssd253x_int_port);
    //msleep(10);
	usleep_range(25, 30);
}

void deviceSleep(struct ssl_ts_priv *ssl_priv)
{
	ssl_priv->bsleep=1;
	hrtimer_cancel(&ssl_priv->timer);
	mdelay(10);
	gpio_set_value_cansleep(ssd253x_rst_port,0);
	mdelay(10);
	gpio_set_value_cansleep(ssd253x_rst_port,1);
    ssd253x_reset(20);
}

static void ssd253x_raw(struct ssl_ts_priv *ssl_priv)
{
	long max_raw=0,min_raw=4095,avg_raw=0,temp;
	int i,j;
	u8 reg_buff[4]={0};
#ifdef SSL_DEBUG
	volatile unsigned long time0=jiffies;
#endif
#if (defined(HAS_BUTTON) && !defined(SIMULATED_BUTTON))
	SSL_PRINT("Buttons: ");
	for(i=0;i<4;i++){
		ssd253x_read_reg(ssl_priv->client, BUTTON_RAW+i, &reg_buff[0], 2,2);
		temp=reg_buff[1]+(reg_buff[0]<<8);
		SSL_PRINT("[%d]=%ld ",i,temp);
	}
	SSL_PRINT("\n");
#endif
	if(ssl_priv->tp_id==0x2533){
		reg_buff[0]=0;
		reg_buff[1]=1;
		ssd253x_write_cmd(ssl_priv->client,0x93,reg_buff,2);
	}
	else if(ssl_priv->tp_id==0x2531){
		reg_buff[0]=0;
		ssd253x_write_cmd(ssl_priv->client,0x93,reg_buff,1);
	}
	else
		return;
	mdelay(30);
	reg_buff[0]=0;
	ssd253x_write_cmd(ssl_priv->client,0x8E,reg_buff,1);
	reg_buff[0]=0;
	ssd253x_write_cmd(ssl_priv->client,0x8F,reg_buff,1);
	reg_buff[0]=0;
	ssd253x_write_cmd(ssl_priv->client,0x90,reg_buff,1);
	mdelay(10);
	for(i=0;i<ssl_priv->drives;i++){
		for(j=0;j<ssl_priv->senses;j++){
			ssd253x_read_reg(ssl_priv->client, 0x92, &reg_buff[0], 2,2);
			temp=reg_buff[1]+((reg_buff[0]&0x0F)<<8);
			if(ssl_priv->rawout)
				SSL_PRINT("%4ld ",temp);
			avg_raw+=temp;
			if(max_raw<temp)
				max_raw=temp;
			if(min_raw>temp)
				min_raw=temp;
		}
		if(ssl_priv->rawout)
			SSL_PRINT("\n");
	}
	avg_raw/=ssl_priv->drives*ssl_priv->senses;
	SSL_PRINT("Raw (read in %u ms): max=%ld, min=%ld, avg=%ld\n",jiffies_to_msecs(jiffies-time0),max_raw,min_raw,avg_raw);
}

static int ssd253x_delta(struct ssl_ts_priv *ssl_priv, int bout)
{
	int max_delta=-256,min_delta=4095,temp;
	int i,j;
	u8 reg_buff[4]={0};
#ifdef SSL_DEBUG
	volatile unsigned long time0=jiffies;
#endif
	if(ssl_priv->tp_id==0x2533){
		reg_buff[0]=0;
		reg_buff[1]=5;
		ssd253x_write_cmd(ssl_priv->client,0x93,reg_buff,2);
		mdelay(30);
	}
	else if(ssl_priv->tp_id==0x2531){
		reg_buff[0]=1;
		ssd253x_write_cmd(ssl_priv->client,0x8D,reg_buff,1);
	}
	else
		return 0;
	reg_buff[0]=0;
	ssd253x_write_cmd(ssl_priv->client,0x8E,reg_buff,1);
	reg_buff[0]=0;
	ssd253x_write_cmd(ssl_priv->client,0x8F,reg_buff,1);
	reg_buff[0]=0;
	ssd253x_write_cmd(ssl_priv->client,0x90,reg_buff,1);
	for(i=0;i<ssl_priv->drives;i++){
		for(j=0;j<ssl_priv->senses;j++){
			ssd253x_read_reg(ssl_priv->client, 0x92, &reg_buff[0], 2,2);
			if(ssl_priv->tp_id==0x2533){
				temp=reg_buff[1]+((reg_buff[0]&0x03)<<8);
				if(temp>511)
					temp-=1024;
			}
			else{
				temp=reg_buff[1]+((reg_buff[0]&0x01)<<8);
				if(temp>255)
					temp-=512;
			}
			if(ssl_priv->rawout)
				SSL_PRINT("%4d ",temp);
			if(max_delta<temp)
				max_delta=temp;
			if(min_delta>temp)
				min_delta=temp;
		}
		if(ssl_priv->rawout)
			SSL_PRINT("\n");
	}
	if(bout)
		SSL_PRINT("Delta (read in %u ms): max=%d, min=%d\n",jiffies_to_msecs(jiffies-time0),max_delta,min_delta);
	if(ssl_priv->tp_id==0x2531){
		reg_buff[0]=0;
		ssd253x_write_cmd(ssl_priv->client,0x8D,reg_buff,1);
	}
	if(min_delta<CAL_THRESHOLD)
		return 1;
	else
		return 0;
}

static void ssd253x_finger_down(struct input_dev *input, int id, int px, int py,int pressure)
{
#if LINUX_VERSION_CODE>= KERNEL_VERSION(3,0,0)

	input_mt_slot(input, id);
	input_mt_report_slot_state(input, MT_TOOL_FINGER,true);
	input_report_abs(input, ABS_MT_POSITION_X, px);
	input_report_abs(input, ABS_MT_POSITION_Y, py);
	input_report_abs(input, ABS_MT_PRESSURE, pressure);
#else
	input_report_abs(input, ABS_MT_TRACKING_ID, id);
	input_report_abs(input, ABS_MT_POSITION_X, px);
	input_report_abs(input, ABS_MT_POSITION_Y, py);

	input_report_abs(input, ABS_MT_PRESSURE, pressure);
	input_mt_sync(input);
#endif
}

static void ssd253x_finger_up(struct input_dev *input, int id)
{
#if LINUX_VERSION_CODE>= KERNEL_VERSION(3,0,0)
	input_mt_slot(input, id);
	input_mt_report_slot_state(input, MT_TOOL_FINGER,false);
#else
	input_report_abs(input, ABS_MT_PRESSURE, 0);
	input_report_abs(input, ABS_MT_TRACKING_ID, id);
	input_mt_sync(input);
#endif
}

#define X_COOR (reg_buff[0] + (((u16)reg_buff[2] & 0xF0) << 4))
#define Y_COOR (reg_buff[1] + (((u16)reg_buff[2] & 0x0F) << 8))
#define PRESS_PRESSURE ((reg_buff[3] >> 4) & 0x0F)
#define PRESS_SPEED  (reg_buff[3]&0x0F)
#define SPEED_EST_TH	3
#define SPEED_EST_RATE	3
#define DEFORMATION_DETECT	2	//deformation detection interval
#define LCD_EDGE_DEVIATION	8
static void ssd253x_ts_work(struct work_struct *work)
{
	static int wmode=0;
    static int count_up;
    static int count_down;
	static int oldx[10],oldy[10];
	int ret = 0;
	int i,smode;
    #if defined(HAS_BUTTON)
    int j;
    #endif
	u8 reg_buff[4] = { 0 };
	u32 finger_flag = 0;
	u32 old_flag=0;
	u32 button_flag=0;
	u32 bitmask;
	int px,py;
	struct ssl_ts_priv *ssl_priv = container_of(work,struct ssl_ts_priv,ssl_work);
	SSL_PRINT("Run into %s.\n",__FUNCTION__);
	if(ssl_priv->bsleep)
		return;
	smode=ssl_priv->working_mode;
	if(wmode!=smode){
		if(ssl_priv->tp_id==0x2533){
			if(smode==0){
				reg_buff[0]=0;
				ssd253x_write_cmd(ssl_priv->client,0x8D,reg_buff,1);
				mdelay(50);
				reg_buff[0]=ssl_priv->r25h;
				ssd253x_write_cmd(ssl_priv->client,0x25,reg_buff,1);
				mdelay(300);
			}
			else{
				reg_buff[0]=0;
				ssd253x_write_cmd(ssl_priv->client,0x25,reg_buff,1);
				mdelay(100);
				reg_buff[0]=1;
				ssd253x_write_cmd(ssl_priv->client,0x8D,reg_buff,1);
				mdelay(50);
				reg_buff[0]=1;
				ssd253x_write_cmd(ssl_priv->client,0xC2,reg_buff,1);
				reg_buff[0]=3;
				ssd253x_write_cmd(ssl_priv->client,0xC3,reg_buff,1);
			}
		}
		else if(ssl_priv->tp_id==0x2531){
			if(smode==1)//raw
				reg_buff[0]=1;
			else
				reg_buff[0]=0;
			ssd253x_write_cmd(ssl_priv->client,0x8D,reg_buff,1);
			mdelay(50);
		}
		wmode=smode;
	}
	if(smode!=0){
		if(smode==1)	//raw
		{
			SSL_PRINT("wds==ssd253x_raw==ssl_priv\n");
			ssd253x_raw(ssl_priv);
		}
		else	//delta
		{
			SSL_PRINT("wds==ssd253x_delta==ssl_priv\n");
			ssd253x_delta(ssl_priv,1);
		}
		hrtimer_start(&ssl_priv->timer, ktime_set(0, 100000000), HRTIMER_MODE_REL);
		return;
	}

	if(ssl_priv->tp_id==0x2533||ssl_priv->tp_id==0x2528){
		ret = ssd253x_read_reg(ssl_priv->client, FINGER_STATUS, &reg_buff[0], 2,2);
		finger_flag=reg_buff[0]<<8;
		finger_flag+=reg_buff[1];
		if(ssl_priv->rawout)
		SSL_PRINT("R79h:0x%04X.\n",finger_flag);
	}
	else if(ssl_priv->tp_id==0x2531){
		ret = ssd253x_read_reg(ssl_priv->client, FINGER_STATUS, &reg_buff[0], 1,1);
		finger_flag=((reg_buff[0]&0xF0)>>4)|((reg_buff[0]&0x0F)<<4);
		if(ssl_priv->rawout)
		SSL_PRINT("R79h:0x%02X.\n",finger_flag);
	}
	else{
		SSL_PRINT("Not IRQ from SSL CTP device!\n");
		input_sync(ssl_priv->input);
	}
	if(ret){
		finger_flag=0;
		//I2C Device fail
	}
	else{
		for(i=0;i<ssl_priv->finger_count;i++){
			bitmask=0x10<<i;
			if(finger_flag & bitmask) { /*finger exists*/
				ret = ssd253x_read_reg(ssl_priv->client, FINGER_DATA+i, reg_buff, 4,4);//wds
				if(ret) {
					//I2C device fail
				}
				else{
					px=X_COOR;
					py=Y_COOR;
					if((px!=0xFFF)&&(py!=0xFFF)){
						ssl_priv->prev_x[i]=px;
						ssl_priv->prev_y[i]=py;
					}
					if(finger_flag&0x04)	//don't report finger if there is large object
						continue;
					SSL_PRINT("SSD253X: Finger%d at x=%d, y=%d.\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i]);
                    //print_read_time(); //Add test sam
					SSL_PRINT("COORDINATE - SSD253X: Finger%d at x=%d, y=%d.\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i]);

			#if (defined(HAS_BUTTON) && defined(SIMULATED_BUTTON))
					for(j=0;j<sizeof(SKeys)/sizeof(SKey_Info);j++){
						if(ssl_priv->prev_x[i]>(SKeys[j].x-SKeys[j].dx) &&
						   ssl_priv->prev_x[i]<(SKeys[j].x+SKeys[j].dx) &&
						   ssl_priv->prev_y[i]>(SKeys[j].y-SKeys[j].dy) &&
						   ssl_priv->prev_y[i]<(SKeys[j].y+SKeys[j].dy)){	//simulated button down
							SSL_PRINT("Finger%d at:(%d,%d), report as button%d down\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i],j);
							if(i<FINGER_USED)
								input_report_key(ssl_priv->input,ssl_priv->keys[j],1);
							ssl_priv->buttons|=(1<<j);
							ssl_priv->keyindex[j]=i;
							break;
						}
						else{	//finger is not in button area
							if((ssl_priv->buttons)&(1<<j)){	//but the button is down previously
								if(ssl_priv->keyindex[j]==i){	//finger is the one caused button down and now is out of button area, report as button up
									SSL_PRINT("Finger%d at:(%d,%d), moves out of button area, report as button%d up\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i],j);
									if(i<FINGER_USED)
										input_report_key(ssl_priv->input,ssl_priv->keys[j],0);
									ssl_priv->buttons&=~(1<<j);
								}
							}
						}
					}
					if(ssl_priv->prev_x[i]>=(LCD_EDGE_DEVIATION+LCD_RANGE_X)||ssl_priv->prev_y[i]>=(LCD_EDGE_DEVIATION+LCD_RANGE_Y)){
						//finger out of LCD range, regard as leave
						if(ssl_priv->fingerbits&(1<<i)){
							SSL_PRINT("Finger%d at:(%d,%d), out of LCD range, report as finger leave\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i]);
							if(i<FINGER_USED){
								ssd253x_finger_up(ssl_priv->input,i);
							}
							ssl_priv->fingerbits&=~(1<<i);
						}
						continue;
					}
			#endif
					if(ssl_priv->prev_x[i]>=LCD_RANGE_X)
						ssl_priv->prev_x[i]=LCD_RANGE_X-1;
					if(ssl_priv->prev_y[i]>=LCD_RANGE_Y)
						ssl_priv->prev_y[i]=LCD_RANGE_Y-1;
					if(ssl_priv->prev_x[i]==0)
						ssl_priv->prev_x[i]=1;
					if(ssl_priv->prev_y[i]==0)
						ssl_priv->prev_y[i]=1;
					if(ssl_priv->fingerbits & (1<<i)){
						SSL_PRINT("Finger%d move:(%d,%d)\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i]);
						px=abs(ssl_priv->prev_x[i]-oldx[i])+abs(ssl_priv->prev_y[i]-oldy[i]);
						py=jiffies_to_msecs(jiffies-ssl_priv->touchtime[i]);
						if(py==0)
							py=10;
						px/=py;
						if(px>SPEED_EST_TH){//trigger moving estimation
							ssl_priv->est_x[i]=ssl_priv->prev_x[i]+SPEED_EST_RATE*(ssl_priv->prev_x[i]-oldx[i]);
							if(ssl_priv->est_x[i]>=LCD_RANGE_X){
								ssl_priv->est_x[i]=LCD_RANGE_X-1;
							}
							else if(ssl_priv->est_x[i]<0){
								ssl_priv->est_x[i]=0;
							}
							if(ssl_priv->prev_x[i]==oldx[i])
								ssl_priv->est_y[i]=ssl_priv->prev_y[i]+SPEED_EST_RATE*(ssl_priv->prev_y[i]-oldy[i]);
							else
								ssl_priv->est_y[i]=ssl_priv->prev_y[i]+(ssl_priv->est_x[i]-ssl_priv->prev_x[i])*(ssl_priv->prev_y[i]-oldy[i])/(ssl_priv->prev_x[i]-oldx[i]);
							if(ssl_priv->est_y[i]>=LCD_RANGE_Y){
								ssl_priv->est_y[i]=LCD_RANGE_Y-1;
							}
							else if(ssl_priv->est_y[i]<0){
								ssl_priv->est_y[i]=0;
							}
							if(ssl_priv->prev_y[i]==oldy[i])
								ssl_priv->est_x[i]=ssl_priv->prev_x[i]+SPEED_EST_RATE*(ssl_priv->prev_x[i]-oldx[i]);
							else
								ssl_priv->est_x[i]=ssl_priv->prev_x[i]+(ssl_priv->est_y[i]-ssl_priv->prev_y[i])*(ssl_priv->prev_x[i]-oldx[i])/(ssl_priv->prev_y[i]-oldy[i]);
							ssl_priv->est_en[i]=1;
						}
						else{
								ssl_priv->est_en[i]=0;
						}
						//finger[i] move
					}
					else{
						SSL_PRINT("Finger%d enter:(%d,%d)\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i]);
						ssl_priv->fingerbits|=(1<<i);
						//finger[i] enter
					}
					//report finger[i] coordinate here
					ssl_priv->touchtime[i]=jiffies;
					oldx[i]=ssl_priv->prev_x[i];
					oldy[i]=ssl_priv->prev_y[i];
					if(i<FINGER_USED){
						ssd253x_finger_down(ssl_priv->input,i,ssl_priv->prev_x[i],ssl_priv->prev_y[i],PRESS_PRESSURE+1);
					}
				}
			}
			else{
				if(ssl_priv->fingers & bitmask){
					//finger[i] leave
	#if (defined(HAS_BUTTON) && defined(SIMULATED_BUTTON))
					if(ssl_priv->prev_x[i]>=LCD_RANGE_X||ssl_priv->prev_y[i]>=LCD_RANGE_Y){
						for(j=0;j<sizeof(SKeys)/sizeof(SKey_Info);j++){
							if(ssl_priv->prev_x[i]>(SKeys[j].x-SKeys[j].dx) &&
							   ssl_priv->prev_x[i]<(SKeys[j].x+SKeys[j].dx) &&
							   ssl_priv->prev_y[i]>(SKeys[j].y-SKeys[j].dy) &&
							   ssl_priv->prev_y[i]<(SKeys[j].y+SKeys[j].dy)){	//simulated button up
								SSL_PRINT("Finger%d leave at:(%d,%d), report as button%d up\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i],j);
								if(i<FINGER_USED)
									input_report_key(ssl_priv->input,ssl_priv->keys[j],0);
								ssl_priv->buttons&=~(1<<j);
								break;
							}
						}
						continue;
					}
	#endif
					if(ssl_priv->est_en[i]){
						finger_flag|=bitmask;
						ssl_priv->est_en[i]=0;
						if(i<FINGER_USED){
//								ssl_priv->prev_x[i],ssl_priv->prev_y[i],ssl_priv->est_x[i],ssl_priv->est_y[i]);
						SSL_PRINT("SSD253X: real leave coordinate (%d,%d), estimated leave coordinate (%d,%d).\n",\
								ssl_priv->prev_x[i],ssl_priv->prev_y[i],ssl_priv->est_x[i],ssl_priv->est_y[i]);
							ssl_priv->prev_x[i]=ssl_priv->est_x[i];
							ssl_priv->prev_y[i]=ssl_priv->est_y[i];
							ssd253x_finger_down(ssl_priv->input,i,ssl_priv->prev_x[i],ssl_priv->prev_y[i],1);
						}
					}
					else{
						SSL_PRINT("Finger%d leave:(%d,%d)\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i]);
                        //print_read_time();
						SSL_PRINT("COORDINATE - > Finger%d leave:(%d,%d)\n",i,ssl_priv->prev_x[i],ssl_priv->prev_y[i]);
						if(i<FINGER_USED){
							ssd253x_finger_up(ssl_priv->input,i);
						}
						ssl_priv->fingerbits&=~(1<<i);
					}
				}
			}
		}
		old_flag=ssl_priv->fingers;
		ssl_priv->fingers=finger_flag;
	}
#if (defined(HAS_BUTTON) && !defined(SIMULATED_BUTTON))
	ret = ssd253x_read_reg(ssl_priv->client, BUTTON_STATUS, &reg_buff[0], 1,1);
	if(ret){
		//I2C Device fail
		button_flag=0;
	}
	else{
		button_flag = reg_buff[0];
		SSL_PRINT("RB9h:0x%02X.\n",button_flag);
		for(i=0;i<4;i++){
			bitmask=1<<i;
			if(button_flag & bitmask) { /*button[i] on*/
				if(ssl_priv->buttons & bitmask){	//button[i] still on
					//repeat button[i]
					SSL_PRINT("Button%d repeat.\n",i);
				}
				else{
					//button[i] down
					SSL_PRINT("Button%d down.\n",i);
					input_report_key(ssl_priv->input,ssl_priv->keys[i],1);
				}
			}
			else{
				if(ssl_priv->buttons & bitmask){
					//button[i] up
					SSL_PRINT("Button%d up.\n",i);
					input_report_key(ssl_priv->input,ssl_priv->keys[i],0);
				}
			}
		}
		ssl_priv->buttons=button_flag;
	}
#endif
	input_sync(ssl_priv->input);
	if(finger_flag){
		if(old_flag==0){
			if(ssl_priv->binitialrun==1){	//the first time finger on after sleep out
				ssl_priv->binitialrun=2;
				ssl_priv->timemark=jiffies-(HZ*DEFORMATION_DETECT)-1;
			}
			else{
				ssl_priv->timemark=jiffies;
			}
		}
		if((ssl_priv->tp_id==0x2531)&&(time_after(jiffies,ssl_priv->timemark+(HZ*DEFORMATION_DETECT)))&&(ssl_priv->prev_delta||time_before(jiffies,ssl_priv->cal_end_time))){
			ssl_priv->timemark=jiffies;
			ssl_priv->bcalibrating=1;
			ssl_priv->cal_counter++;
			ssl_priv->prev_delta=ssd253x_delta(ssl_priv,0);
			if(ssl_priv->prev_delta){	//oops, there is abnormal finger hole
				SSL_PRINT("SSD253X: Lens deformation detected, now calibrate!\n");
				reg_buff[0]=0x01;
				ssd253x_write_cmd(ssl_priv->client,0xA2,reg_buff,1);
				ssl_priv->cal_tick=-1;
				hrtimer_start(&ssl_priv->timer, ktime_set(0, 300*1000000), HRTIMER_MODE_REL); //check again after 300ms
				return;
			}
			else{
				ssl_priv->bcalibrating=0;
			}
		}
	}
	if(button_flag==0 && finger_flag ==0){	//no finger or button on, so no need to monitor status
		SSL_PRINT("No finger or button on.\n");
		if(ssl_priv->cal_tick>=1){
			ssl_priv->cal_tick=0;
			ret=ssd253x_read_reg(ssl_priv->client,0x26,reg_buff,1,1);
			if((ret!=0)||(reg_buff[0]==0)||(ssl_priv->tp_id==0)){	//oops, the TP was reset, need re-init
				SSL_PRINT("SSD253X: TP was reset, now restart it!\n");
				deviceWakeUp(ssl_priv);
				return;
			}
			else{
				if(ssl_priv->binitialrun){
					if(ssl_priv->prev_delta||time_before(jiffies,ssl_priv->cal_end_time)){	//only check when bootup/sleepout
						if(ssl_priv->tp_id==0x2531){	//only SSD2531 needs auto-calibration
							ssl_priv->cal_counter++;
							ssl_priv->bcalibrating=1;
							ssl_priv->prev_delta=ssd253x_delta(ssl_priv,0);
							if(ssl_priv->prev_delta){	//oops, there is abnormal finger hole
								SSL_PRINT("SSD253X: Finger hole detected, now calibrate!\n");
								reg_buff[0]=0x01;
								ssd253x_write_cmd(ssl_priv->client,0xA2,reg_buff,1);
								ssl_priv->cal_tick=-1;
								SSL_PRINT("Calibrate! ssl_priv->cal_counter=%d.\n",ssl_priv->cal_counter);
							}
							else{
								ssl_priv->bcalibrating=0;
							}
						}
					}
					else{
						//SSL_PRINT("SSD253X: Totally checked finger hole for %d times.\n",ssl_priv->cal_counter);
						ssl_priv->binitialrun=0;
					}
				}
			}
		}
		ssl_priv->cal_tick++;
        if ( count_down < 5) 
        {
    		if(ssl_priv->bcalibrating==1){
    			hrtimer_start(&ssl_priv->timer, ktime_set(0, 300*1000000), HRTIMER_MODE_REL); //check again after 300ms
    			return;
    		}
    		if(ssl_priv->binitialrun==1)
    			hrtimer_start(&ssl_priv->timer, ktime_set(0, CAL_INTERVAL*1000000), HRTIMER_MODE_REL); //check again after 30ms
    		else if(ssl_priv->binitialrun==2)
    			hrtimer_start(&ssl_priv->timer, ktime_set(0, 500000000), HRTIMER_MODE_REL); //check again after 500ms
    		else
    			hrtimer_start(&ssl_priv->timer, ktime_set(0, 1000000000), HRTIMER_MODE_REL); //check again after 1s
        }
        else
            count_down = 0;
        count_down++;
	}
	else{
		ssl_priv->cal_tick=0;
        if( count_up < 30 )
		  hrtimer_start(&ssl_priv->timer, ktime_set(0, 16600000), HRTIMER_MODE_REL); //polling for finger up after 16.6ms.
        else
            count_up = 0;
        count_up++;
	}
    ssd253x_irq_enable(ssl_priv);

}

static int ssd253x_ts_probe(struct i2c_client *client,const struct i2c_device_id *idp)
{
	struct ssl_ts_priv *ssl_priv;
	struct input_dev *ssl_input = NULL;
#ifdef CONFIG_OF
    struct device_node *np = client->dev.of_node;
#endif
	int error;
#if defined(HAS_BUTTON)
    int i;
#endif
	struct device *dev;
	struct pinctrl *pinctrl;
	printk("Run into %s.\n",__FUNCTION__);

	dev = &client->dev;
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
        error=-ENODEV;
        goto    err0;
    }

    ssl_priv = kzalloc(sizeof(*ssl_priv), GFP_KERNEL);
    if (!ssl_priv){
        error=-ENODEV;
        goto    err1;
    }
    ssl_priv_global = ssl_priv;
    dev_set_drvdata(&client->dev, ssl_priv);

	pinctrl = pinctrl_get_select(dev, "default");
	if(IS_ERR(pinctrl)) {
		printk("%s: pinctrl select failed\n", "tcc-ts-ssd253x");
		error=-ENODEV;
		goto err1;
	}
	else
		pinctrl_put(pinctrl);	
    if (np) {
        ssd253x_int_port = of_get_named_gpio(np, "int-gpios", 0);
        if (!gpio_is_valid(ssd253x_int_port)) {
            printk("%s: err to get gpios: ret:%x\n", __func__, ssd253x_int_port);
            ssd253x_int_port = -1;
        }
        else {
            printk("%s: int gpios: ret:%x\n", __func__, ssd253x_int_port);
            ssd253x_int_irq = gpio_to_irq(ssd253x_int_port);
        }

        ssd253x_rst_port = of_get_named_gpio(np, "rst-gpios", 0);
        if (!gpio_is_valid(ssd253x_rst_port)) {
            printk("%s: err to get gpios: ret:%x\n", __func__, ssd253x_rst_port);
            ssd253x_rst_port = -1;
        }
        else
            printk("%s: rst gpios: ret:%x\n", __func__, ssd253x_rst_port);
    }

    //mdelay(10);
    gpio_direction_output(ssd253x_rst_port, 1);
   	//msleep(1);
	usleep_range(25,30);
    gpio_direction_output(ssd253x_rst_port, 0);
    //msleep(1);
	usleep_range(25,30);
    gpio_direction_output(ssd253x_rst_port, 1);
    //msleep(1);
	udelay(30);

	ssl_priv->tp_id=ssd253x_read_id(client);
	if (ssl_priv->tp_id==0x2533){
		printk("Found SSL CTP device(SSD2533) at address 0x%02X.\n",client->addr);
		ssl_priv->finger_count=10;
	}
	else if(ssl_priv->tp_id==0x2531){
		printk("Found SSL CTP device(SSD2531) at address 0x%02X.\n",client->addr);
		ssl_priv->finger_count=4;
	}
	else if(ssl_priv->tp_id==0x2528){
		printk("Found SSL CTP device(SSD2528) at address 0x%02X.\n",client->addr);
		ssl_priv->finger_count=2;
	}
	else{
		printk("No SSL CTP device found(0x%x) at address 0x%02X.\n",ssl_priv->tp_id, client->addr);
		ssl_priv->tp_id=0;
		//printk("No SSL CTP device found at address 0x%02X.\n",client->addr);
		printk("SSL driver version failed!!\n");
		goto    err3;
	}

    error = gpio_request(ssd253x_rst_port,"TCC_TS_RST_PORT");
    if(error<0){
        printk("GPIO request for RST fail.\n");
        error=-ENODEV;
        goto err2;
    }

    error = gpio_request(ssd253x_int_port,"TCC_TS_INT_IRQ");
    if(error<0){
        printk("GPIO request for INT fail.\n");
        error=-ENODEV;
        goto err3;
    }

    ssl_priv->irq = ssd253x_int_irq;
    if(ssl_priv->irq < 0){
        printk("Can't get irq number from GPIO(%d)\n", ssl_priv->irq);
        error=-ENODEV;
        goto err3;
    }
	else
	{
		printk("[%s:irq no : %d]\n", __func__, ssl_priv->irq);
	}

    gpio_direction_input(ssd253x_int_port);
	ssl_priv->client = client;
	deviceInit(ssl_priv);
	ssl_input = input_allocate_device();
	if (!ssl_input){
		error=-ENODEV;
		goto	err3;
	}
	set_bit(EV_KEY, ssl_input->evbit);
	set_bit(EV_ABS, ssl_input->evbit);
	set_bit(EV_SYN, ssl_input->evbit);
#ifdef HAS_BUTTON
#ifdef SIMULATED_BUTTON
	for(i=0;i<sizeof(SKeys)/sizeof(SKey_Info);i++){
		ssl_priv->keys[i]=SKeys[i].code;
		input_set_capability(ssl_input, EV_KEY, ssl_priv->keys[i]);
	}
#else
	ssl_priv->keys[0]=BUTTON0;
	ssl_priv->keys[1]=BUTTON1;
	ssl_priv->keys[2]=BUTTON2;
	ssl_priv->keys[3]=BUTTON3;
	for(i=0;i<4;i++){
		input_set_capability(ssl_input, EV_KEY, ssl_priv->keys[i]);
	}
#endif
#endif
	#ifndef CONFIG_ANDROID
	set_bit(BTN_TOUCH, ssl_input->keybit);
	set_bit(ABS_X, ssl_input->absbit);
	set_bit(ABS_Y, ssl_input->absbit);
	#endif
	set_bit(ABS_MT_POSITION_X, ssl_input->absbit);
	set_bit(ABS_MT_POSITION_Y, ssl_input->absbit);
	set_bit(ABS_MT_PRESSURE, ssl_input->absbit);
	input_set_abs_params(ssl_input, ABS_MT_PRESSURE, 0, 16, 0, 0);
#if LINUX_VERSION_CODE>= KERNEL_VERSION(3,0,0)
	#ifdef CONFIG_ANDROID
	set_bit(MT_TOOL_FINGER, ssl_input->keybit);
	#endif
	set_bit(INPUT_PROP_DIRECT, ssl_input->propbit);
	input_mt_init_slots(ssl_input, ssl_priv->finger_count, 0);
	input_set_abs_params(ssl_input, ABS_X, 0, LCD_RANGE_X-1, 0, 0);
	input_set_abs_params(ssl_input, ABS_Y, 0, LCD_RANGE_Y-1, 0, 0);
	input_set_abs_params(ssl_input, ABS_MT_POSITION_X, 0, LCD_RANGE_X-1, 0, 0);
	input_set_abs_params(ssl_input, ABS_MT_POSITION_Y, 0, LCD_RANGE_Y-1, 0, 0);
#else
	set_bit(ABS_MT_TRACKING_ID, ssl_input->absbit);
	input_set_abs_params(ssl_input, ABS_MT_TRACKING_ID, 0,ssl_priv->finger_count, 0, 0);
	input_set_abs_params(ssl_input, ABS_MT_POSITION_X, 0, LCD_RANGE_X, 0, 0);
	input_set_abs_params(ssl_input, ABS_MT_POSITION_Y, 0, LCD_RANGE_Y, 0, 0);

#endif
	ssl_input->name = client->name;
	ssl_input->id.bustype = BUS_I2C;
	ssl_input->dev.parent = &client->dev;

	input_set_drvdata(ssl_input, ssl_priv);

	ssl_priv->input = ssl_input;

	INIT_WORK(&ssl_priv->ssl_work, ssd253x_ts_work);
    spin_lock_init(&ssl_priv->irq_lock);

	error = request_irq(ssl_priv->irq, ssd253x_ts_isr, IRQF_TRIGGER_FALLING, client->name, ssl_priv);
	if (error){
		printk("IRQ request fail.\n");
		error=-ENODEV;
		goto err3;
	}
	disable_irq_nosync(ssl_priv->irq);
	hrtimer_init(&ssl_priv->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ssl_priv->timer.function = ssd253x_ts_timer;
	error = input_register_device(ssl_input);
	if(error){
		error=-ENODEV;
		goto	err4;
	}

	StartWork(ssl_priv);
	enable_irq(ssl_priv_global->irq);
    printk("%s : Solomon Touch driver is registered.\n",__func__);

	return 0;
err4:
	free_irq(ssl_priv->irq, ssl_priv);
err3:
	gpio_free(ssd253x_int_port);
err2:
	input_free_device(ssl_input);
    gpio_free(ssd253x_rst_port);
err1:
	kfree(ssl_priv);
	dev_set_drvdata(&client->dev, NULL);
err0:
    ssd253x_off = true;
	printk("%s : failed to run driver(%d).\n", __func__, error);
	return error;
}

//static int ssd253x_ts_suspend(struct i2c_client *client, pm_message_t mesg)
static int ssd253x_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ssl_ts_priv *ssl_priv = dev_get_drvdata(&client->dev);
	SSL_PRINT("Run into %s.\n",__FUNCTION__);
	deviceSleep(ssl_priv);
	return 0;
}
//static int ssd253x_ts_resume(struct i2c_client *client)
static int ssd253x_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
    struct ssl_ts_priv *ssl_priv = dev_get_drvdata(&client->dev);
	SSL_PRINT("Run into %s.\n",__FUNCTION__);
    ssd253x_i2c_reset();
	deviceWakeUp(ssl_priv);
	return 0;
}

static int ssd253x_ts_remove(struct i2c_client *client)
{
	struct ssl_ts_priv *ssl_priv = dev_get_drvdata(&client->dev);
	SSL_PRINT("Run into %s.\n",__FUNCTION__);
	free_irq(ssl_priv->irq, ssl_priv);
	gpio_free(ssd253x_int_port);
	gpio_free(ssd253x_rst_port);
	input_unregister_device(ssl_priv->input);
	input_free_device(ssl_priv->input);
	//remove_proc_entry(PROCFS_NAME, NULL); //kernel warning
	kfree(ssl_priv);
	dev_set_drvdata(&client->dev, NULL);
	return 0;
}
static irqreturn_t ssd253x_ts_isr(int irq, void *dev_id)
{
	struct ssl_ts_priv *ssl_priv = dev_id;
	if(ssl_priv->bcalibrating==1)
		return IRQ_HANDLED;
    ssd253x_irq_disable(ssl_priv);
	hrtimer_cancel(&ssl_priv->timer);
	SSL_PRINT("Run into %s.\n",__FUNCTION__);
	queue_work(ssd253x_wq, &ssl_priv->ssl_work);
	return IRQ_HANDLED;
}
static enum hrtimer_restart ssd253x_ts_timer(struct hrtimer *timer)
{
	struct ssl_ts_priv *ssl_priv = container_of(timer, struct ssl_ts_priv, timer);
	SSL_PRINT("Run into %s.\n",__FUNCTION__);
	if(ssl_priv->bcalibrating==1){
		ssl_priv->bcalibrating=0;
	}
	queue_work(ssd253x_wq, &ssl_priv->ssl_work);
	return HRTIMER_NORESTART;
}
static const struct i2c_device_id ssd253x_ts_id[] = {
	{ "tcc-ts-ssd253x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssd253x_ts_id);

#ifdef CONFIG_OF
static const struct of_device_id ssd253x_ts_of_match[] = {
    {.compatible = "solomon,ssd253x", },
    { },
};
MODULE_DEVICE_TABLE(of, ssd253x_ts_of_match);
#endif

static SIMPLE_DEV_PM_OPS(ssd253x_ts_pm_ops, ssd253x_ts_suspend, ssd253x_ts_resume);

static struct i2c_driver ssd253x_ts_driver = {
	.driver = {
		.name = "tcc-ts-ssd253x",
		.pm = &ssd253x_ts_pm_ops,
#ifdef CONFIG_OF
        .of_match_table = ssd253x_ts_of_match,
#endif
	},
	.probe = ssd253x_ts_probe,
	.remove = ssd253x_ts_remove,
	//.suspend = ssd253x_ts_suspend,
	//.resume = ssd253x_ts_resume,
	.id_table = ssd253x_ts_id,
};

static char banner[] __initdata = KERN_INFO "SSL Touchscreen driver, (c) 2011 Solomon Systech Ltd.\n";
static int __init ssd253x_ts_init(void)
{
	int ret;

	printk(banner);
	ssd253x_wq = create_singlethread_workqueue("ssd253x_wq");
	if (!ssd253x_wq){
		return -ENOMEM;
	}
	else{
	}
	ret=i2c_add_driver(&ssd253x_ts_driver);
	return ret;
}

static void __exit ssd253x_ts_exit(void)
{
	i2c_del_driver(&ssd253x_ts_driver);
	if (ssd253x_wq) destroy_workqueue(ssd253x_wq);
}

module_init(ssd253x_ts_init);
module_exit(ssd253x_ts_exit);

MODULE_AUTHOR("Solomon Systech Ltd - Rover Shen");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SSD2531/SSD2532/SSD2533 Touchscreen Driver V2.2.4, updated at 14:20, July 24, 2013");
