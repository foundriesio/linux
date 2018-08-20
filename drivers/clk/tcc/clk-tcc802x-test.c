#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clocksource.h>

#include <linux/fs.h>
#include <linux/device.h>
#include <linux/major.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>

#include <linux/clk/tcc.h>
#include "clk-tcc802x.h"

static char *common_var = NULL;

static struct tcc_ckc_ops *ckc_ops = NULL;

static ssize_t gpu_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t gpu_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t g2d_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t g2d_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t ddi_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t ddi_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t cpu1_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t cpu1_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t vbus_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t vbus_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t coda_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t coda_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t jpeg_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t jpeg_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t cm_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t cm_power_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t hsio_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t hsio_power_show(struct class *cls, struct class_attribute *attr, char *buf);

static struct class_attribute clktest_class_attrs[] = {
	__ATTR(gpu_power, S_IRUGO | S_IWUSR , gpu_power_show, gpu_power_store), 
	__ATTR(g2d_power, S_IRUGO | S_IWUSR , g2d_power_show, g2d_power_store), 
	__ATTR(ddi_power, S_IRUGO | S_IWUSR , ddi_power_show, ddi_power_store), 
	__ATTR(cpu1_power, S_IRUGO | S_IWUSR , cpu1_power_show, cpu1_power_store), 
	__ATTR(vbus_power, S_IRUGO | S_IWUSR , vbus_power_show, vbus_power_store), 
	__ATTR(coda_power, S_IRUGO | S_IWUSR , coda_power_show, coda_power_store), 
	__ATTR(jpeg_power, S_IRUGO | S_IWUSR , jpeg_power_show, jpeg_power_store), 
	__ATTR(cm_power, S_IRUGO | S_IWUSR , cm_power_show, cm_power_store), 
	__ATTR(hsio_power, S_IRUGO | S_IWUSR , hsio_power_show, hsio_power_store), 
	__ATTR_NULL
};

static struct class clktest_class =
{
	.name = "tcc_clk",
	.owner = THIS_MODULE,
	.class_attrs = clktest_class_attrs
};

static int clktest_init(void)
{
	char string[] = "nothing";
	common_var = kmalloc(sizeof(char)*strlen(string), GFP_KERNEL);
	class_register(&clktest_class);
	snprintf(common_var, sizeof(char)*strlen(string)+1, "%s", string);
	return 0;
}

static int tcc_clkctrl_enable(unsigned int id)
{                                                     
	if (ckc_ops->ckc_pmu_pwdn)                        
		ckc_ops->ckc_pmu_pwdn(id, false);        
	if (ckc_ops->ckc_swreset)                         
		ckc_ops->ckc_swreset(id, false);         
	if (ckc_ops->ckc_clkctrl_enable)                  
		ckc_ops->ckc_clkctrl_enable(id); 

	return 0;                                         
}                                                     

static void tcc_clkctrl_disable(unsigned int id)
{                                                     
	if (ckc_ops->ckc_clkctrl_disable)                 
		ckc_ops->ckc_clkctrl_disable(id);        
	if (ckc_ops->ckc_swreset)                         
		ckc_ops->ckc_swreset(id, true); 
	if (ckc_ops->ckc_pmu_pwdn)                        
		ckc_ops->ckc_pmu_pwdn(id, true);         
}                                                     

static int tcc_vpubus_enable(unsigned int id)     
{                                                   
	if (ckc_ops->ckc_vpubus_pwdn)                   
		ckc_ops->ckc_vpubus_pwdn(id, false);   
	if (ckc_ops->ckc_vpubus_swreset)                
		ckc_ops->ckc_vpubus_swreset(id, false);
	return 0;                                       
}

static void tcc_vpubus_disable(unsigned int id)   
{                                                   
	if (ckc_ops->ckc_vpubus_swreset)                
		ckc_ops->ckc_vpubus_swreset(id, true); 
	if (ckc_ops->ckc_vpubus_pwdn)                   
		ckc_ops->ckc_vpubus_pwdn(id, true);    
}

static int tcc_hsiobus_enable(unsigned int id)
{
	if (ckc_ops->ckc_hsiobus_pwdn)
		ckc_ops->ckc_hsiobus_pwdn(id, false);
	if (ckc_ops->ckc_hsiobus_swreset)
		ckc_ops->ckc_hsiobus_swreset(id, false);

	return 0;
}

static void tcc_hsiobus_disable(unsigned int id)
{
	if (ckc_ops->ckc_hsiobus_swreset)
		ckc_ops->ckc_hsiobus_swreset(id, true);
	if (ckc_ops->ckc_hsiobus_pwdn)
		ckc_ops->ckc_hsiobus_pwdn(id, true);
}

static ssize_t gpu_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("GPU Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_GPU));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t gpu_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("GPU Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_GPU);
	}
	else if(data == 0) {
		tcc_clkctrl_disable(FBUS_GPU);
	}

	printk("GPU Power Control done\n");
	
	return count;
}

static ssize_t g2d_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("G2D Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_G2D));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t g2d_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("G2D Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_G2D);
	}
	else if(data == 0) {
		tcc_clkctrl_disable(FBUS_G2D);
	}

	printk("G2D Power Control done\n");
	
	return count;
}

static ssize_t ddi_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("G2D Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_DDI));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t ddi_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("DDI Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_DDI);
	}
	else if(data == 0) {
		tcc_clkctrl_disable(FBUS_DDI);
	}

	printk("DDI Power Control done\n");
	
	return count;
}

static ssize_t cpu1_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("CPU1 Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_CPU1));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t cpu1_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("CPU1 Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_CPU1);
	}
	else if(data == 0) {
		tcc_clkctrl_disable(FBUS_CPU1);
	}

	printk("CPU1 Power Control done\n");
	
	return count;
}

static ssize_t vbus_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("VBUS ALL Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_VBUS));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t vbus_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("VBUS ALL Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_VBUS);
	}
	else if(data == 0) {
		tcc_clkctrl_disable(FBUS_VBUS);
	}

	printk("VBUS Power Control done\n");
	
	return count;
}

static ssize_t coda_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("Video CODA Power = %d\n", ckc_ops->ckc_is_vpubus_pwdn(VIDEOBUS_CODA_CORE));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t coda_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("Video CODA Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_CODA);
		tcc_vpubus_enable(VIDEOBUS_CODA_CORE);
		tcc_vpubus_enable(VIDEOBUS_CODA_BUS);
	}
	else if(data == 0) {
		tcc_vpubus_disable(VIDEOBUS_CODA_CORE);
		tcc_vpubus_disable(VIDEOBUS_CODA_BUS);
		tcc_clkctrl_disable(FBUS_CODA);
	}

	printk("Video CODA Power Control done\n");
	
	return count;
}

static ssize_t jpeg_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("Video JPEG Power = %d\n", ckc_ops->ckc_is_vpubus_pwdn(VIDEOBUS_JENC));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t jpeg_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("Video JPEG Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_vpubus_enable(VIDEOBUS_JENC);
	}
	else if(data == 0) {
		tcc_vpubus_disable(VIDEOBUS_JENC);
	}

	printk("Video JPEG Power Control done\n");
	
	return count;
}

static ssize_t cm_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("CM BUS Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_CMBUS));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t cm_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("CM BUS Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_CMBUS);
	}
	else if(data == 0) {
		tcc_clkctrl_disable(FBUS_CMBUS);
	}

	printk("CM BUS Power Control done\n");
	
	return count;
}

static ssize_t hsio_power_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	printk("HSIO BUS Power = %d\n", ckc_ops->ckc_is_pmu_pwdn(FBUS_HSIO));
	printk("%s\n", common_var);
	return sprintf(buf, "%s", common_var);
}

static ssize_t hsio_power_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;

	printk("HSIO BUS Power Control\n");
	error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	if(data == 1) {
		tcc_clkctrl_enable(FBUS_HSIO);
		tcc_hsiobus_enable(HSIOBUS_TRNG);
		tcc_hsiobus_enable(HSIOBUS_GMAC);
		tcc_hsiobus_enable(HSIOBUS_CIPHER);
		tcc_hsiobus_enable(HSIOBUS_USB20H);
		tcc_hsiobus_enable(HSIOBUS_DWC_OTG);
	}
	else if(data == 0) {
		tcc_hsiobus_disable(HSIOBUS_TRNG);
		tcc_hsiobus_disable(HSIOBUS_GMAC);
		tcc_hsiobus_disable(HSIOBUS_CIPHER);
		tcc_hsiobus_disable(HSIOBUS_USB20H);
		tcc_hsiobus_disable(HSIOBUS_DWC_OTG);
		tcc_clkctrl_disable(FBUS_HSIO);
	}

	printk("HSIO BUS Power Control done\n");
	
	return count;
}

static void  clktest_exit(void)
{
	class_unregister(&clktest_class);
	printk("In hello_exit function \n");
}

void tcc_ckctest_set_ops(struct tcc_ckc_ops *ops)
{
	ckc_ops = ops;
}

module_init(clktest_init);
module_exit(clktest_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Inc.");
