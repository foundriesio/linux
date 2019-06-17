/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/dwc_otg_driver.c $
 * $Revision: #96 $
 * $Date: 2013/05/20 $
 * $Change: 2234037 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */

/** @file
 * The dwc_otg_driver module provides the initialization and cleanup entry
 * points for the DWC_otg driver. This module will be dynamically installed
 * after Linux is booted using the insmod command. When the module is
 * installed, the dwc_otg_driver_init function is called. When the module is
 * removed (using rmmod), the dwc_otg_driver_cleanup function is called.
 *
 * This module also defines a data structure for the dwc_otg_driver, which is
 * used in conjunction with the standard ARM lm_device structure. These
 * structures allow the OTG driver to comply with the standard Linux driver
 * model in which devices and drivers are registered with a bus driver. This
 * has the benefit that Linux can expose attributes of the driver and device
 * in its special sysfs file system. Users can then read or write files in
 * this file system to perform diagnostics on the driver components or the
 * device.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/stat.h>		/* permission constants */
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#if defined(CONFIG_REGULATOR)
#include <linux/regulator/consumer.h>
#endif
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>

#include <asm/io.h>
#include <asm/sizes.h>

#include "dwc_otg_os_dep.h"
#include "dwc_os.h"
#include "dwc_otg_dbg.h"
#include "dwc_otg_driver.h"
#include "dwc_otg_attr.h"
#include "dwc_otg_core_if.h"
#include "dwc_otg_pcd_if.h"
#include "dwc_otg_hcd_if.h"
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include "tcc_usb_def.h"
#include "dwc_otg_cil.h"
#include "dwc_otg_core_if.h"

//#include <mach/tcc_board_power.h>
//#include <linux/tcc_pwm.h>
#include <linux/cpufreq.h>

#include <linux/of.h>
#include <linux/of_gpio.h>

#define DWC_DRIVER_VERSION	"3.10b 20-MAY-2013"
#define DWC_DRIVER_DESC		"HS OTG USB Controller driver"

#define TCC_DWC_SOFFN_USE // SOF monitor thread
#define TCC_DWC_SUSPSTS_USE	//for check disconnect

bool microframe_schedule=true;

void dwc_otg_change_drd_mode(dwc_otg_device_t *otg_dev, unsigned int mode);
static const char dwc_driver_name[] = "dwc_otg";
dwc_otg_device_t *g_dwc_otg;
static int is_resuming = 0;

extern void trace_usb_flow(int on);
extern void tcc_ohci_clock_control(int id, int on);
extern int pcd_init(struct platform_device *_dev);
extern int hcd_init(struct platform_device *_dev);
extern int pcd_remove(struct platform_device *_dev);
extern void hcd_remove(struct platform_device *_dev);
extern struct tcc_freq_table_t gtUSBClockLimitTable;
int dwc_otg_driver_force_init(dwc_otg_core_if_t *_core_if);
#if defined(CONFIG_ARCH_TCC)	/* 015.06.04 */
int dwc_otg_tcc_power_ctrl(struct dwc_otg_device *tcc, int on_off)
{
	int err = 0;

	if ((tcc->vbus_source_ctrl == 1) && (tcc->vbus_source)) {
		if(on_off == ON) {
			err = regulator_enable(tcc->vbus_source);
			if(err) {
				printk("dwc_otg: can't enable vbus source\n");
				return err;
			}
			mdelay(1);
		} else if(on_off == OFF) {
			regulator_disable(tcc->vbus_source);
		}
	}
	
	return err;
}

void tcc_otg_vbus_init(void)
{
	dwc_otg_tcc_power_ctrl(g_dwc_otg, ON);
}
void tcc_otg_vbus_exit(void)
{
	dwc_otg_tcc_power_ctrl(g_dwc_otg, OFF);
}
#else
extern void tcc_otg_vbus_init(void);
extern void tcc_otg_vbus_exit(void);
#endif /* CONFIG_ARCH_TCC897X */

extern void tcc_otg_vbus_init(void);
extern void tcc_otg_vbus_exit(void);

#define OTG_DBG(msg...) do{ printk( KERN_ERR "DWC_OTG : " msg ); }while(0)
extern void* dummy_send;

extern int pcd_init(
	struct platform_device *dev
    );
extern int hcd_init(
	struct platform_device *dev
    );

extern int pcd_remove(
	struct platform_device *_dev
    );

extern void hcd_remove(
	struct platform_device *_dev
    );

extern void dwc_otg_adp_start(dwc_otg_core_if_t * core_if, uint8_t is_host);

/*
 * For using gadget_wrapper of dwc_otg_pcd_linux.c
 */
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
struct gadget_wrapper {
#ifndef DWC_HOST_ONLY
	dwc_otg_pcd_t *pcd;
#endif
	struct usb_gadget gadget;
	struct usb_gadget_driver *driver;

	struct usb_ep ep0;
	struct usb_ep in_ep[16];
	struct usb_ep out_ep[16];
};
extern struct gadget_wrapper *gadget_wrapper;
/*-------------------------------------------------------------------------*/
/* Encapsulate the module parameter settings */

struct dwc_otg_driver_module_params {
	int32_t opt;
	int32_t otg_cap;
	int32_t dma_enable;
	int32_t dma_desc_enable;
	int32_t dma_burst_size;
	int32_t speed;
	int32_t host_support_fs_ls_low_power;
	int32_t host_ls_low_power_phy_clk;
	int32_t enable_dynamic_fifo;
	int32_t data_fifo_size;
	int32_t dev_rx_fifo_size;
	int32_t dev_nperio_tx_fifo_size;
	uint32_t dev_perio_tx_fifo_size[MAX_PERIO_FIFOS];
	int32_t host_rx_fifo_size;
	int32_t host_nperio_tx_fifo_size;
	int32_t host_perio_tx_fifo_size;
	int32_t max_transfer_size;
	int32_t max_packet_count;
	int32_t host_channels;
	int32_t dev_endpoints;
	int32_t phy_type;
	int32_t phy_utmi_width;
	int32_t phy_ulpi_ddr;
	int32_t phy_ulpi_ext_vbus;
	int32_t i2c_enable;
	int32_t ulpi_fs_ls;
	int32_t ts_dline;
	int32_t en_multiple_tx_fifo;
	uint32_t dev_tx_fifo_size[MAX_TX_FIFOS];
	uint32_t thr_ctl;
	uint32_t tx_thr_length;
	uint32_t rx_thr_length;
	int32_t pti_enable;
	int32_t mpi_enable;
	int32_t lpm_enable;
	int32_t besl_enable;
	int32_t baseline_besl;
	int32_t deep_besl;
	int32_t ic_usb_cap;
	int32_t ahb_thr_ratio;
	int32_t power_down;
	int32_t reload_ctl;
	int32_t dev_out_nak;
	int32_t cont_on_bna;
	int32_t ahb_single;
	int32_t otg_ver;
	int32_t adp_enable;
};

static struct dwc_otg_driver_module_params dwc_otg_module_params = {
	.opt = -1,
	.otg_cap = -1,
	.dma_enable = -1,
	.dma_desc_enable = -1,
	.dma_burst_size = -1,
	.speed = -1,
	.host_support_fs_ls_low_power = -1,
	.host_ls_low_power_phy_clk = -1,
	.enable_dynamic_fifo = -1,
	.data_fifo_size = -1,
	.dev_rx_fifo_size = -1,
	.dev_nperio_tx_fifo_size = -1,
	.dev_perio_tx_fifo_size = {
				   /* dev_perio_tx_fifo_size_1 */
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1
				   /* 15 */
				   },
	.host_rx_fifo_size = -1,
	.host_nperio_tx_fifo_size = -1,
	.host_perio_tx_fifo_size = -1,
	.max_transfer_size = -1,
	.max_packet_count = -1,
	.host_channels = -1,
	.dev_endpoints = -1,
	.phy_type = -1,
	.phy_utmi_width = -1,
	.phy_ulpi_ddr = -1,
	.phy_ulpi_ext_vbus = -1,
	.i2c_enable = -1,
	.ulpi_fs_ls = -1,
	.ts_dline = -1,
	.en_multiple_tx_fifo = -1,
	.dev_tx_fifo_size = {
			     /* dev_tx_fifo_size */
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1,
			     -1
			     /* 15 */
			     },
	.thr_ctl = -1,
	.tx_thr_length = -1,
	.rx_thr_length = -1,
	.pti_enable = -1,
	.mpi_enable = -1,
	.lpm_enable = -1,
	.besl_enable = -1,
	.baseline_besl = -1,
	.deep_besl = -1,
	.ic_usb_cap = -1,
	.ahb_thr_ratio = -1,
	.power_down = -1,
	.reload_ctl = -1,
	.dev_out_nak = -1,
	.cont_on_bna = -1,
	.ahb_single = -1,
	.otg_ver = -1,
	.adp_enable = -1,
};

enum {
	resume_unlock = 0,
	resume_lock,
};

typedef enum {
	USBPHY_MODE_RESET = 0,
	USBPHY_MODE_OTG,
	USBPHY_MODE_ON,
	USBPHY_MODE_OFF,
	USBPHY_MODE_START,
	USBPHY_MODE_STOP,
	USBPHY_MODE_DEVICE_DETACH
} USBPHY_MODE_T;

#ifndef DWC_HOST_ONLY
extern const unsigned char* dwc_otg_cil_get_version(void);
extern const unsigned char* dwc_otg_cil_intr_get_version(void);
extern const unsigned char* dwc_otg_pcd_get_version(void);
extern const unsigned char* dwc_otg_pcd_intr_get_version(void);
#endif

#ifdef CONFIG_OF		/* 015.06.02 */
/*
 * OTG Controller PHY Init. Using phy-tcc-dwc_otg-phy.c driver.
 */
static int tcc_otg_phy_init(dwc_otg_device_t *otg_dev)
{
	struct usb_phy *phy = otg_dev->dwc_otg_phy;

	if (!phy || !phy->init) {
		printk("[%s:%d]PHY driver is needed\n", __func__, __LINE__);
		return -1;
	}

	return phy->init(phy);
}

#ifdef CONFIG_VBUS_CTRL_DEF_ENABLE		/* 017.08.30 */
static unsigned int vbus_control_enable = 1;  /* 2017/03/23, */
#else
static unsigned int vbus_control_enable = 0;  /* 2017/03/23, */
#endif /* CONFIG_VBUS_CTRL_DEF_ENABLE */
module_param(vbus_control_enable, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vbus_control_enable, "dwc_otg vbus control enable");
/*
 * OTG VBUS Power Control. It is controlled GPIO_EXPAND DVBUS_ON.
 * - Host mode: DVBUS_ON is HIGH
 * - Device mode: DVBUS_ON is LOW
 */
int tcc_otg_vbus_ctrl(dwc_otg_device_t *otg_dev, int on_off)
{
	struct usb_phy *phy = otg_dev->dwc_otg_phy;

	if (!vbus_control_enable) {
		printk("dwc_otg vbus ctrl disable.\n");
		return -1;
	}

	if ( !phy || !phy->set_vbus ) {
		printk("[%s:%d]PHY driver is needed\n", __func__, __LINE__);
		return -1;
	}

	otg_dev->vbus_status = on_off;

	return phy->set_vbus(phy,on_off);
}
#endif /* CONFIG_ARCH_TCC897X */

/**
 * This function shows the Driver Version.
 */
static ssize_t version_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, sizeof(DWC_DRIVER_VERSION)+2,"%s\n",
	                DWC_DRIVER_VERSION);
}

//static DRIVER_ATTR(version, S_IRUGO, version_show, NULL);
static DRIVER_ATTR_RO(version);

/**
 * Global Debug Level Mask.
 */
uint32_t g_dbg_lvl = 0; //0x8 | DBG_CIL | DBG_CILV;		/* OFF */

static inline unsigned int GetConnIdStatus(dwc_otg_core_if_t *_core_if)
{
	volatile unsigned int status = 0;
	gintsts_data_t gintsts;
	gintsts.d32 = DWC_READ_REG32(&_core_if->core_global_regs->gintsts);
	status = (gintsts.b.curmode==1)?0:1;		/* 0:host, 1:device */
	return status;
}

/**
 * This function is called during module intialization
 * to pass module parameters to the DWC_OTG CORE.
 */
static int set_parameters(dwc_otg_core_if_t * core_if)
{
	int retval = 0;
	int i;

	if (dwc_otg_module_params.otg_cap != -1) {
		retval +=
		    dwc_otg_set_param_otg_cap(core_if,
					      dwc_otg_module_params.otg_cap);
	}
	if (dwc_otg_module_params.dma_enable != -1) {
		retval +=
		    dwc_otg_set_param_dma_enable(core_if,
						 dwc_otg_module_params.
						 dma_enable);
	}
	if (dwc_otg_module_params.dma_desc_enable != -1) {
		retval +=
		    dwc_otg_set_param_dma_desc_enable(core_if,
						      dwc_otg_module_params.
						      dma_desc_enable);
	}
	if (dwc_otg_module_params.opt != -1) {
		retval +=
		    dwc_otg_set_param_opt(core_if, dwc_otg_module_params.opt);
	}
	if (dwc_otg_module_params.dma_burst_size != -1) {
		retval +=
		    dwc_otg_set_param_dma_burst_size(core_if,
						     dwc_otg_module_params.
						     dma_burst_size);
	}
	if (dwc_otg_module_params.host_support_fs_ls_low_power != -1) {
		retval +=
		    dwc_otg_set_param_host_support_fs_ls_low_power(core_if,
								   dwc_otg_module_params.
								   host_support_fs_ls_low_power);
	}
	if (dwc_otg_module_params.enable_dynamic_fifo != -1) {
		retval +=
		    dwc_otg_set_param_enable_dynamic_fifo(core_if,
							  dwc_otg_module_params.
							  enable_dynamic_fifo);
	}
	if (dwc_otg_module_params.data_fifo_size != -1) {
		retval +=
		    dwc_otg_set_param_data_fifo_size(core_if,
						     dwc_otg_module_params.
						     data_fifo_size);
	}
	if (dwc_otg_module_params.dev_rx_fifo_size != -1) {
		retval +=
		    dwc_otg_set_param_dev_rx_fifo_size(core_if,
						       dwc_otg_module_params.
						       dev_rx_fifo_size);
	}
	if (dwc_otg_module_params.dev_nperio_tx_fifo_size != -1) {
		retval +=
		    dwc_otg_set_param_dev_nperio_tx_fifo_size(core_if,
							      dwc_otg_module_params.
							      dev_nperio_tx_fifo_size);
	}
	if (dwc_otg_module_params.host_rx_fifo_size != -1) {
		retval +=
		    dwc_otg_set_param_host_rx_fifo_size(core_if,
							dwc_otg_module_params.host_rx_fifo_size);
	}
	if (dwc_otg_module_params.host_nperio_tx_fifo_size != -1) {
		retval +=
		    dwc_otg_set_param_host_nperio_tx_fifo_size(core_if,
							       dwc_otg_module_params.
							       host_nperio_tx_fifo_size);
	}
	if (dwc_otg_module_params.host_perio_tx_fifo_size != -1) {
		retval +=
		    dwc_otg_set_param_host_perio_tx_fifo_size(core_if,
							      dwc_otg_module_params.
							      host_perio_tx_fifo_size);
	}
	if (dwc_otg_module_params.max_transfer_size != -1) {
		retval +=
		    dwc_otg_set_param_max_transfer_size(core_if,
							dwc_otg_module_params.
							max_transfer_size);
	}
	if (dwc_otg_module_params.max_packet_count != -1) {
		retval +=
		    dwc_otg_set_param_max_packet_count(core_if,
						       dwc_otg_module_params.
						       max_packet_count);
	}
	if (dwc_otg_module_params.host_channels != -1) {
		retval +=
		    dwc_otg_set_param_host_channels(core_if,
						    dwc_otg_module_params.
						    host_channels);
	}
	if (dwc_otg_module_params.dev_endpoints != -1) {
		retval +=
		    dwc_otg_set_param_dev_endpoints(core_if,
						    dwc_otg_module_params.
						    dev_endpoints);
	}
	if (dwc_otg_module_params.phy_type != -1) {
		retval +=
		    dwc_otg_set_param_phy_type(core_if,
					       dwc_otg_module_params.phy_type);
	}
	if (dwc_otg_module_params.speed != -1) {
		retval +=
		    dwc_otg_set_param_speed(core_if,
					    dwc_otg_module_params.speed);
	}
	if (dwc_otg_module_params.host_ls_low_power_phy_clk != -1) {
		retval +=
		    dwc_otg_set_param_host_ls_low_power_phy_clk(core_if,
								dwc_otg_module_params.
								host_ls_low_power_phy_clk);
	}
	if (dwc_otg_module_params.phy_ulpi_ddr != -1) {
		retval +=
		    dwc_otg_set_param_phy_ulpi_ddr(core_if,
						   dwc_otg_module_params.
						   phy_ulpi_ddr);
	}
	if (dwc_otg_module_params.phy_ulpi_ext_vbus != -1) {
		retval +=
		    dwc_otg_set_param_phy_ulpi_ext_vbus(core_if,
							dwc_otg_module_params.
							phy_ulpi_ext_vbus);
	}
	if (dwc_otg_module_params.phy_utmi_width != -1) {
		retval +=
		    dwc_otg_set_param_phy_utmi_width(core_if,
						     dwc_otg_module_params.
						     phy_utmi_width);
	}
	if (dwc_otg_module_params.ulpi_fs_ls != -1) {
		retval +=
		    dwc_otg_set_param_ulpi_fs_ls(core_if,
						 dwc_otg_module_params.ulpi_fs_ls);
	}
	if (dwc_otg_module_params.ts_dline != -1) {
		retval +=
		    dwc_otg_set_param_ts_dline(core_if,
					       dwc_otg_module_params.ts_dline);
	}
	if (dwc_otg_module_params.i2c_enable != -1) {
		retval +=
		    dwc_otg_set_param_i2c_enable(core_if,
						 dwc_otg_module_params.
						 i2c_enable);
	}
	if (dwc_otg_module_params.en_multiple_tx_fifo != -1) {
		retval +=
		    dwc_otg_set_param_en_multiple_tx_fifo(core_if,
							  dwc_otg_module_params.
							  en_multiple_tx_fifo);
	}
	for (i = 0; i < 15; i++) {
		if (dwc_otg_module_params.dev_perio_tx_fifo_size[i] != -1) {
			retval +=
			    dwc_otg_set_param_dev_perio_tx_fifo_size(core_if,
								     dwc_otg_module_params.
								     dev_perio_tx_fifo_size
								     [i], i);
		}
	}

	for (i = 0; i < 15; i++) {
		if (dwc_otg_module_params.dev_tx_fifo_size[i] != -1) {
			retval += dwc_otg_set_param_dev_tx_fifo_size(core_if,
								     dwc_otg_module_params.
								     dev_tx_fifo_size
								     [i], i);
		}
	}
	if (dwc_otg_module_params.thr_ctl != -1) {
		retval +=
		    dwc_otg_set_param_thr_ctl(core_if,
					      dwc_otg_module_params.thr_ctl);
	}
	if (dwc_otg_module_params.mpi_enable != -1) {
		retval +=
		    dwc_otg_set_param_mpi_enable(core_if,
						 dwc_otg_module_params.
						 mpi_enable);
	}
	if (dwc_otg_module_params.pti_enable != -1) {
		retval +=
		    dwc_otg_set_param_pti_enable(core_if,
						 dwc_otg_module_params.
						 pti_enable);
	}
	if (dwc_otg_module_params.lpm_enable != -1) {
		retval +=
		    dwc_otg_set_param_lpm_enable(core_if,
						 dwc_otg_module_params.
						 lpm_enable);
	}
	if (dwc_otg_module_params.besl_enable != -1) {
		retval +=
		    dwc_otg_set_param_besl_enable(core_if,
						 dwc_otg_module_params.
						 besl_enable);
	}
	if (dwc_otg_module_params.baseline_besl != -1) {
		retval +=
		    dwc_otg_set_param_baseline_besl(core_if,
						 dwc_otg_module_params.
						 baseline_besl);
	}
	if (dwc_otg_module_params.deep_besl != -1) {
		retval +=
		    dwc_otg_set_param_deep_besl(core_if,
						 dwc_otg_module_params.
						 deep_besl);
	}		
	if (dwc_otg_module_params.ic_usb_cap != -1) {
		retval +=
		    dwc_otg_set_param_ic_usb_cap(core_if,
						 dwc_otg_module_params.
						 ic_usb_cap);
	}
	if (dwc_otg_module_params.tx_thr_length != -1) {
		retval +=
		    dwc_otg_set_param_tx_thr_length(core_if,
						    dwc_otg_module_params.tx_thr_length);
	}
	if (dwc_otg_module_params.rx_thr_length != -1) {
		retval +=
		    dwc_otg_set_param_rx_thr_length(core_if,
						    dwc_otg_module_params.
						    rx_thr_length);
	}
	if(dwc_otg_module_params.ahb_thr_ratio != -1) {
		retval +=
		    dwc_otg_set_param_ahb_thr_ratio(core_if,
						    dwc_otg_module_params.ahb_thr_ratio);
	}
	if (dwc_otg_module_params.power_down != -1) {
		retval +=
		    dwc_otg_set_param_power_down(core_if,
						 dwc_otg_module_params.power_down);
	}
	if (dwc_otg_module_params.reload_ctl != -1) {
		retval +=
		    dwc_otg_set_param_reload_ctl(core_if,
						 dwc_otg_module_params.reload_ctl);
	}

	if (dwc_otg_module_params.dev_out_nak != -1) {
		retval +=
			dwc_otg_set_param_dev_out_nak(core_if,
			dwc_otg_module_params.dev_out_nak);
	}

	if (dwc_otg_module_params.cont_on_bna != -1) {
		retval +=
			dwc_otg_set_param_cont_on_bna(core_if,
			dwc_otg_module_params.cont_on_bna);
	}

	if (dwc_otg_module_params.ahb_single != -1) {
		retval +=
			dwc_otg_set_param_ahb_single(core_if,
			dwc_otg_module_params.ahb_single);
	}

	if (dwc_otg_module_params.otg_ver != -1) {
		retval +=
		    dwc_otg_set_param_otg_ver(core_if,
					      dwc_otg_module_params.otg_ver);
	}
	if (dwc_otg_module_params.adp_enable != -1) {
		retval +=
		    dwc_otg_set_param_adp_enable(core_if,
						 dwc_otg_module_params.
						 adp_enable);
	}
	return retval;
}

/**
 * This function is the top level interrupt handler for the Common
 * (Device and host modes) interrupts.
 */
static irqreturn_t dwc_otg_common_irq(int irq, void *dev)
{
	int32_t retval = IRQ_NONE;

	retval = dwc_otg_handle_common_intr(dev);
	return IRQ_RETVAL(retval);
}

///**
// * This function shows the driver Debug Level.
// */
//static ssize_t dbg_level_show(struct device_driver *drv, char *buf)
//{
//	return sprintf(buf, "0x%0x\n", g_dbg_lvl);
//}
//
///**
// * This function stores the driver Debug Level.
// */
//static ssize_t dbg_level_store(struct device_driver *drv, const char *buf,
//			       size_t count)
//{
//	g_dbg_lvl = simple_strtoul(buf, NULL, 16);
//	return count;
//}
//static DRIVER_ATTR(debuglevel, S_IRUGO | S_IWUSR, dbg_level_show,
//		   dbg_level_store);

static ssize_t dwc_otg_tcc_vbus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	dwc_otg_device_t *otg_dev =	platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "dwc_otg vbus - %s\n",(otg_dev->vbus_status) ? "on":"off");
}

static ssize_t dwc_otg_tcc_vbus_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	dwc_otg_device_t *otg_dev =	platform_get_drvdata(to_platform_device(dev));

	if (!strncmp(buf, "on", 2)) {
		tcc_otg_vbus_ctrl(otg_dev, ON);
	}

	if (!strncmp(buf, "off", 3)) {
		tcc_otg_vbus_ctrl(otg_dev, OFF);
	}

	return count;
}
static DEVICE_ATTR(vbus, S_IRUGO | S_IWUSR, dwc_otg_tcc_vbus_show, dwc_otg_tcc_vbus_store);

static ssize_t dwc_otg_tcc_debug_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%0x\n", g_dbg_lvl);
}

static ssize_t dwc_otg_tcc_debug_level_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	g_dbg_lvl = simple_strtoul(buf, NULL, 16);
	return count;
}
static DEVICE_ATTR(debug_level, S_IRUGO | S_IWUSR, dwc_otg_tcc_debug_level_show, dwc_otg_tcc_debug_level_store);

/**
 * Show the DRD mode status
 */
static ssize_t drdmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	dwc_otg_device_t *otg_dev =	platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "Current mode is %s \n", (otg_dev->current_mode == DWC_OTG_MODE_HOST) ? "Host":"Device");
}

/**
 * Set the DRD mode change Request
 */
static ssize_t drdmode_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	dwc_otg_device_t *otg_dev =	platform_get_drvdata(to_platform_device(dev));
	unsigned int		mode=0;
	struct work_struct *work;

	if (!strncmp(buf, "host", 4)) {
		mode = DWC_OTG_MODE_HOST;
		printk("[drd mode req: %s]\n", "\x1b[1;41mHost\x1b[0m");
	} else if (!strncmp(buf, "device", 6)) {
		mode = DWC_OTG_MODE_DEVICE;
		printk("[drd mode req: %s]\n", "\x1b[1;46mDevice\x1b[0m");
	} else {
		return count;
	}

	if (otg_dev->current_mode == 0xFF) {
		printk("[drd mode changing to %s]\n",
			(otg_dev->new_mode == DWC_OTG_MODE_HOST) ? "Host":"Device");
		if(otg_dev->new_mode == mode){
			printk("[drd mode req Filtering]\n");
			return count;
		}
	}else{
		if(otg_dev->current_mode == mode){
			printk("[drd mode req Filtering]\n");
			return count;
		}
	}

	work = &otg_dev->drd_work;

	if (work_pending(work))
	{
		printk("[drd_store pending]\n");
		return count;
	}

	otg_dev->new_mode = mode;

	queue_work(otg_dev->drd_wq, work);
	/* wait for operation to complete */
	flush_work(work);

	return count;
}
DEVICE_ATTR(drdmode, S_IRUGO | S_IWUSR, drdmode_show, drdmode_store);

#define USBOTG_PCFG0_PHY_POR Hw31					// PHY Power-on Reset; (1:Reset)(0:Normal)
static int USBPHY_IsActive(dwc_otg_core_if_t *_core_if)
{
	return ((_core_if->tcc_phy_config->pcfg0 & USBOTG_PCFG0_PHY_POR) == 0) ? TRUE : FALSE;
}

static ssize_t dwc_otg_tcc_phystatus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	dwc_otg_device_t *otg_dev = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "PHY activation = %s\n", (USBPHY_IsActive(otg_dev->core_if)) ? "on" : "off");
}

static ssize_t dwc_otg_tcc_phystatus_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	dwc_otg_device_t *otg_dev =	platform_get_drvdata(to_platform_device(dev));
	struct usb_phy *phy = otg_dev->dwc_otg_phy;

	if (!strncmp(buf, "on", 2)) {
		phy->set_phy_state(phy, USBPHY_MODE_START);
	}

	if (!strncmp(buf, "off", 3)) {
		phy->set_phy_state(phy, USBPHY_MODE_STOP);
	}

	if (!strncmp(buf, "reset", 3)) {
		tcc_otg_phy_init(otg_dev);
	}

	return count;
}
static DEVICE_ATTR(phystatus, S_IRUGO | S_IWUSR, dwc_otg_tcc_phystatus_show, dwc_otg_tcc_phystatus_store);

#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/ohci_pdriver.h>

struct tcc_mux_hcd_device {
	struct platform_device *ehci_dev;
	struct platform_device *ohci_dev;
	u32 enable_flags;
};

static const struct usb_ehci_pdata ehci_pdata = {
};

static const struct usb_ohci_pdata ohci_pdata = {
};

static struct platform_device *tcc_mux_hcd_create_pdev(dwc_otg_device_t *otg_dev, bool ohci, u32 addr, u32 len)
{
	struct platform_device *hci_dev;
	struct resource hci_res[2];
	struct usb_hcd *hcd;
	int ret = -ENOMEM;

	memset(hci_res, 0, sizeof(hci_res));

	hci_res[0].start = addr;
	hci_res[0].end 	= hci_res[0].start + len - 1;
	hci_res[0].flags = IORESOURCE_MEM;

	hci_res[1].start = otg_dev->ehci_irq;
	hci_res[1].flags = IORESOURCE_IRQ;

	hci_dev = platform_device_alloc(ohci ? "ohci-mux" :
					"ehci-mux" , 0);

	if (!hci_dev)
		return NULL;

	hci_dev->dev.parent = otg_dev->dev;
	hci_dev->dev.dma_mask = &hci_dev->dev.coherent_dma_mask;

	ret = platform_device_add_resources(hci_dev, hci_res,
					    ARRAY_SIZE(hci_res));
	if (ret)
		goto err_alloc;
	if (ohci)
		ret = platform_device_add_data(hci_dev, &ohci_pdata,
					       sizeof(ohci_pdata));
	else
		ret = platform_device_add_data(hci_dev, &ehci_pdata,
					       sizeof(ehci_pdata));
	if (ret)
		goto err_alloc;
	ret = platform_device_add(hci_dev);
	if (ret)
		goto err_alloc;

	hcd = dev_get_drvdata(&hci_dev->dev);
	if (hcd == NULL){
		printk("\x1b[1;31m[%s:%d](hcd == NULL)\x1b[0m\n", __func__, __LINE__);
		while(1);
	}
	hcd->tpl_support = otg_dev->hcd_tpl_support;

	if (otg_dev->mhst_phy)
		hcd->usb_phy = otg_dev->mhst_phy;

	return hci_dev;

err_alloc:
	platform_device_put(hci_dev);
	return ERR_PTR(ret);
}

int dwc_otg_mux_hcd_init(dwc_otg_device_t *otg_dev)
{
	struct tcc_mux_hcd_device *usb_dev;
	int err;
	int start, len;

	usb_dev = kzalloc(sizeof(struct tcc_mux_hcd_device), GFP_KERNEL);
	if (!usb_dev)
		return -ENOMEM;

	start = (int)otg_dev->ohci_regs;
	len = otg_dev->ohci_regs_size;
	usb_dev->ohci_dev = tcc_mux_hcd_create_pdev(otg_dev, true, start, len);
	if (IS_ERR(usb_dev->ohci_dev)) {
		err = PTR_ERR(usb_dev->ohci_dev);
		goto err_free_usb_dev;
	}

	start = (int)otg_dev->ehci_regs;
	len = otg_dev->ehci_regs_size;
	usb_dev->ehci_dev = tcc_mux_hcd_create_pdev(otg_dev, false, start, len);
	if (IS_ERR(usb_dev->ehci_dev)) {
		err = PTR_ERR(usb_dev->ehci_dev);
		goto err_unregister_ohci_dev;
	}

	otg_dev->usb_dev = usb_dev;
	return 0;

err_unregister_ohci_dev:
	platform_device_unregister(usb_dev->ohci_dev);
err_free_usb_dev:
	kfree(usb_dev);
	return err;
}

static void usbotgh_hcd_remove(dwc_otg_device_t *otg_dev)
{
	struct tcc_mux_hcd_device *usb_dev;
	struct platform_device *ohci_dev;
	struct platform_device *ehci_dev;

	usb_dev = otg_dev->usb_dev;
	ohci_dev = usb_dev->ohci_dev;
	ehci_dev = usb_dev->ehci_dev;

	if (ohci_dev)
		platform_device_unregister(ohci_dev);
	if (ehci_dev)
		platform_device_unregister(ehci_dev);

	kfree(usb_dev);
}
#endif

/**
 * This function is called when a lm_device is unregistered with the
 * dwc_otg_driver. This happens, for example, when the rmmod command is
 * executed. The device may or may not be electrically present. If it is
 * present, the driver stops device processing. Any resources used on behalf
 * of this device are freed.
 *
 * @param _dev
 */
static int dwc_otg_driver_remove(struct platform_device *_dev)
{
	dwc_otg_device_t *otg_dev = platform_get_drvdata(_dev);

	DWC_DEBUGPL(DBG_ANY, "%s(%p)\n", __func__, _dev);

	if (!otg_dev) {
		/* Memory allocation for the dwc_otg_device failed. */
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev NULL!\n", __func__);
		return -1;
	}

	cancel_work_sync(&otg_dev->drd_work);
	destroy_workqueue(otg_dev->drd_wq);

	device_remove_file(&_dev->dev, &dev_attr_drdmode);
	device_remove_file(&_dev->dev, &dev_attr_vbus);
	device_remove_file(&_dev->dev, &dev_attr_debug_level);
	device_remove_file(&_dev->dev, &dev_attr_phystatus);

#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX		/* 016.11.30 */
	if (otg_dev->core_if->mux_own == DWC_OTG_MODE_HOST) {
		// mux ehci host disable
		otg_dev->core_if->mux_own = MUX_MODE_DEVICE;
		usbotgh_hcd_remove(otg_dev);
		dwc_otg_change_drd_mode(otg_dev, DWC_OTG_MODE_DEVICE);
	}
#else
	#ifndef DWC_DEVICE_ONLY
	if (otg_dev->hcd) {
		hcd_remove(_dev);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->hcd NULL!\n", __func__);
		return -1;
	}
	#endif
#endif /* CONFIG_TCC_DWC_OTG_HOST_MUX */

#ifndef DWC_HOST_ONLY
	if (otg_dev->pcd) {
		pcd_remove(_dev);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->pcd NULL!\n", __func__);
		return -1;
	}
#endif

	/*
	 * Free the IRQ
	 */
	if (otg_dev->common_irq_installed) {
		int irq = platform_get_irq(_dev, 0);
		if(irq >= 0)
			free_irq(irq, otg_dev);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: There is no installed irq!\n", __func__);
		return -1;
	}

	if (otg_dev->core_if) {
		dwc_otg_cil_remove(otg_dev->core_if);
	} else {
		DWC_DEBUGPL(DBG_ANY, "%s: otg_dev->core_if NULL!\n", __func__);
		return -1;
	}

	/*
	 * Remove the device attributes
	 */
	dwc_otg_attr_remove(_dev);

	/*
	 * Return the memory.
	 */
	release_mem_region(_dev->resource[0].start,		//otg base
	                    _dev->resource[0].end - _dev->resource[0].start + 1);
	release_mem_region(_dev->resource[1].start,		//otg phy base
	                    _dev->resource[1].end - _dev->resource[1].start + 1);
#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX		/* 016.12.07 */
	release_mem_region(_dev->resource[3].start,		//ehci phy base
	                    _dev->resource[3].end - _dev->resource[3].start + 1);
#endif /* CONFIG_TCC_DWC_OTG_HOST_MUX */

	/*
	 * Clear the drvdata pointer.
	 */
	platform_set_drvdata(_dev, 0);

	if(otg_dev)
		DWC_FREE(otg_dev);

	clk_disable(otg_dev->hclk);

	return 0;
}

#ifdef CONFIG_OF		/* 015.06.02 */
extern bool of_usb_host_tpl_support(struct device_node *np);

static int dwc_otg_device_parse_dt(struct platform_device *pdev, struct dwc_otg_device *dwc_otg_device)
{
	int err = 0;

	if (of_find_property(pdev->dev.of_node, "phy-type", 0)) {
		of_property_read_u32(pdev->dev.of_node, "phy-type", &dwc_otg_device->phy_type);
		
		/*
		 * phy type 1 = pico phy
		 * phy type 0 = nano phy
		 */
		if (dwc_otg_device->phy_type == 1)
			printk("pico phy\n");
		else if (dwc_otg_device->phy_type == 0)
			printk("nano phy\n");
		else
			printk("bad phy set!\n");
	} else {
		dwc_otg_device->phy_type = 0;	// nano phy use
		printk("nano phy\n");
	}

	//===============================================
	// Check Host enable pin	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "otgen-ctrl-able", 0)) {
		dwc_otg_device->otgen_ctrl_able = 1;

		dwc_otg_device->otgen_gpio = of_get_named_gpio(pdev->dev.of_node,
						"otgen-gpio", 0);
		if(!gpio_is_valid(dwc_otg_device->otgen_gpio)){
			dev_err(&pdev->dev, "can't find dev of node: otg en gpio\n");
			return -ENODEV;
		}

		err = gpio_request(dwc_otg_device->otgen_gpio, "otgen_gpio");
		if(err) {
			dev_err(&pdev->dev, "can't requeest otg_en gpio\n");
			return err;
		}
	} else {
		dwc_otg_device->otgen_ctrl_able = 0;	// can not control vbus
	}

	//===============================================
	// Check vbus enable pin
	//===============================================
#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX		/* 017.02.28 */
	if (of_find_property(pdev->dev.of_node, "telechips,mhst_phy", 0)) {
		dwc_otg_device->mhst_phy = devm_usb_get_phy_by_phandle(&pdev->dev, "telechips,mhst_phy", 0);
#ifdef CONFIG_ARCH_TCC803X
		err = dwc_otg_device->mhst_phy->set_vbus_resource(dwc_otg_device->mhst_phy);
		if (err) {
			dev_err(&pdev->dev, "failed to set a vbus resource\n");
		}
#endif 
		if (IS_ERR(dwc_otg_device->mhst_phy)) {
			dwc_otg_device->mhst_phy = NULL;
			printk("[%s:%d]PHY driver is needed\n", __func__, __LINE__);
			return -1;
		}
	}
#endif /* CONFIG_TCC_DWC_OTG_HOST_MUX */
	if (of_find_property(pdev->dev.of_node, "telechips,dwc_otg_phy", 0)) {
		dwc_otg_device->dwc_otg_phy = devm_usb_get_phy_by_phandle(&pdev->dev, "telechips,dwc_otg_phy", 0);
#ifdef CONFIG_ARCH_TCC803X
		err = dwc_otg_device->dwc_otg_phy->set_vbus_resource(dwc_otg_device->dwc_otg_phy);
		if (err) {
			dev_err(&pdev->dev, "failed to set a vbus resource\n");
		}
#endif 
		if (IS_ERR(dwc_otg_device->dwc_otg_phy)){
			dwc_otg_device->dwc_otg_phy = NULL;
			printk("[%s:%d]PHY driver is needed\n", __func__, __LINE__);
			return -1;
		}
	}

	//===============================================
	// Check VBUS Source enable	
	//===============================================
	if (of_find_property(pdev->dev.of_node, "vbus-source-ctrl", 0)) {
		dwc_otg_device->vbus_source_ctrl = 1;

		dwc_otg_device->vbus_source = regulator_get(&pdev->dev, "vdd_v5p0");
		if (IS_ERR(dwc_otg_device->vbus_source)) {
			dev_err(&pdev->dev, "failed to get otg vdd_source\n");
			dwc_otg_device->vbus_source = NULL;
		}
	} else {
		dwc_otg_device->vbus_source_ctrl = 0;
	}

	// PCLK
	dwc_otg_device->pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(dwc_otg_device->pclk))
		dwc_otg_device->pclk = NULL;

	// HCLK
	dwc_otg_device->hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(dwc_otg_device->hclk))
		dwc_otg_device->hclk = NULL;

	// PHY CLK
	dwc_otg_device->phy_clk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR(dwc_otg_device->phy_clk))
		dwc_otg_device->phy_clk = NULL;

	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &dwc_otg_device->core_clk_rate);

	//===============================================
	// Check TPL Support
	//===============================================
	if(of_usb_host_tpl_support(pdev->dev.of_node))
	{
		//printk("\x1b[1;33m[%s:%d] DWC_OTG Support TPL\x1b[0m\n", __func__, __LINE__);
		dwc_otg_device->hcd_tpl_support = 1;
	}
	else
	{
		//printk("\x1b[1;31m[%s:%d] DWC_OTG Not Support TPL\x1b[0m\n", __func__, __LINE__);
		dwc_otg_device->hcd_tpl_support = 0;
	}

	return err;		
}
#else
static int dwc_otg_device_parse_dt(struct platform_device *pdev, struct dwc_otg_device *dwc_otg_device)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef TCC_DWC_SOFFN_USE
#define SOFFN_MASK          0x3FFF
#define SOFFN_SHIFT         8
#define GET_SOFFN_NUM(x)    ((x >> SOFFN_SHIFT) & SOFFN_MASK)
#define SOFFN_DISCONNECT    0
#define SOFFN_CONNECTED     1

int dwc_otg_get_soffn(dwc_otg_core_if_t *core_if){
	unsigned int uTmp;

	uTmp = DWC_READ_REG32(&core_if->dev_if->dev_global_regs->dsts);
	return GET_SOFFN_NUM(uTmp);
}

#ifdef TCC_DWC_SUSPSTS_USE		/* 016.11.07 */
static int dwc_otg_soffn_monitor_thread(void * work){
	dwc_otg_device_t *otg_dev = (dwc_otg_device_t *)work;
    dwc_otg_core_if_t *core_if =otg_dev->core_if;
	bool disconnect_check = false;
	int retry = 100;		//wait for a sec until iphone role-switch

	while(!kthread_should_stop() && retry > 0) {
		usleep_range(10000, 10500);
		/* Monitoring starts when soffn is not NULL and not in suspend state
		   If host is not connected for 1 second after role-switch, disconnect is judged.
		 */
		if(!dwc_otg_get_soffn(core_if) || dwc_otg_get_suspsts(core_if)) {
			retry--;
			if (disconnect_check == true && retry <= 0)
				break;
			continue;
		}
		disconnect_check = true;
		retry = 3;		//check disconnect status 3 times
	}

	if(otg_dev->current_mode == DWC_OTG_MODE_DEVICE)
		cil_pcd_disconnect(core_if);
	else
		printk("%s - mode changed(%d)", __func__, otg_dev->current_mode);

	otg_dev->dwc_otg_soffn_thread = NULL;

	tcc_otg_vbus_ctrl(otg_dev, OFF); // VBUS off
	msleep(200);
	printk("dwc_otg device - Host Disconnected\n");

	return 0;
}
#else
// Before the Apple device starts the enumeration
// Because we should wait at least 1s for the Apple device(host) to start enumeration
#define DISCONNECT_CHECK_TOT_TIME 1000 // in msec (DO NOT MODIFY THIS VALUE)
#define DISCONNECT_CHECK_TRIES  20 // in times
#define DISCONNECT_CHECK_PERIOD DISCONNECT_CHECK_TOT_TIME / DISCONNECT_CHECK_TRIES // in msec

// After the Apple device ends the enumeration
#define CONNECT_TEST_TOT_TIME 150 // in msec (DO NOT MODIFY THIS VALUE)
#define CONNECT_TEST_TRIES  3 // in times
#define CONNECT_TEST_PERIOD CONNECT_TEST_TOT_TIME / CONNECT_TEST_TRIES // in msec/

static int dwc_otg_soffn_monitor_thread(void * work){

	dwc_otg_device_t *otg_dev = (dwc_otg_device_t *)work;
    dwc_otg_core_if_t *core_if =otg_dev->core_if;

    unsigned int new_number ,old_number;
    int retry = 0;
    int disconnect_check_cnt = 0;
    bool check_start = false;
	uint32_t disconnect_check_period, connect_test_period;

	disconnect_check_period = DISCONNECT_CHECK_PERIOD;
	connect_test_period = CONNECT_TEST_PERIOD;


    do{
        /* get old var */
        old_number = dwc_otg_get_soffn(core_if);

		// sleep time is dependent on finish of the enumeration
        DWC_MSLEEP(check_start == false ? disconnect_check_period : connect_test_period);

        /* get new var */
        new_number = dwc_otg_get_soffn(core_if);

        if((new_number != old_number) && (check_start == false) && (old_number == 0) && (new_number != 0)) {
            check_start = true;
            disconnect_check_cnt = 0;
        }

        if(check_start == true)
        {
            /* detection disconnect */
            if(new_number == old_number) {
                retry++;
            }
            else
            {
                retry = 0;
            }

            if(retry == CONNECT_TEST_TRIES - 1) {
                retry = 0;
                //dwc3_set_drd_mode(dwc3, DWC3_MODE_HOST);
                
				if(otg_dev->current_mode == DWC_OTG_MODE_DEVICE){
					cil_pcd_disconnect(core_if);
				}
				else
					printk("%s - mode changed(%d)", __func__, otg_dev->current_mode);
				//dwc3_tcc_vbus_ctrl(0);				// VBUS OFF
				//otg_dev->vbus_status = 0;
				otg_dev->dwc_otg_soffn_thread = NULL;
				break;
			}
		}
		else
		{
			if(disconnect_check_cnt == DISCONNECT_CHECK_TRIES - 1)
			{
				printk("%s - sof isn't changed(%d : %d)! force disconnect! (Timeout : %d msec)\n",
					__func__, old_number, new_number,(disconnect_check_period * DISCONNECT_CHECK_TRIES));
				if(otg_dev->current_mode == DWC_OTG_MODE_DEVICE){
					cil_pcd_disconnect(core_if);
				}
				else
				{
					printk("%s - not in device mode (%d)", __func__, otg_dev->current_mode);
				}
				//otg_dev->vbus_status = 0;
				otg_dev->dwc_otg_soffn_thread = NULL;
				break;
			}
			else
			{
				disconnect_check_cnt++;
			}
		}
	
	}while(!kthread_should_stop());
	tcc_otg_vbus_ctrl(otg_dev, OFF); // VBUS off
	printk("dwc_otg device - Host Disconnected\n");
	msleep(200);

	return 0;
}
#endif /* TCC_DWC_SUSPSTS_USE */
#endif /* TCC_DWC_SOFFN_USE */

void dwc_otg_drd_host_init(dwc_otg_core_if_t *core_if)
{
	uint32_t count = 0;

	DWC_DEBUGPL(DBG_CILV,"\x1b[1;32m[%s:%d] Host mode\x1b[0m\n", __func__, __LINE__);

	/* Wait for switch to host mode. */
	while (!dwc_otg_is_host_mode(core_if)) {
		DWC_DEBUGPL(DBG_CILV,"Waiting for Host Mode, Mode=%s count = %d\n",
			   (dwc_otg_is_host_mode(core_if) ? "Host" :
			    "Peripheral"), count);
		dwc_mdelay(1);	//vahrama previously was 100
		if (++count > 10000)
			break;
	}
	DWC_ASSERT(++count < 10000,
		   "Connection id status change timed out");
	core_if->op_state = A_HOST;

	if(core_if->otg_ver == 0)
		dwc_otg_core_init(core_if);
	dwc_otg_enable_global_interrupts(core_if);
	DWC_SPINLOCK(core_if->self_lock);
	cil_hcd_start(core_if);
	DWC_SPINUNLOCK(core_if->self_lock);
}

void dwc_otg_drd_dev_init(dwc_otg_core_if_t *core_if)
{
	uint32_t count = 0;

	DWC_DEBUGPL(DBG_CILV,"\x1b[1;32m[%s:%d] Device mode\x1b[0m\n", __func__, __LINE__);

	/* Wait for switch to device mode. */
	while (!dwc_otg_is_device_mode(core_if)) {
		DWC_DEBUGPL(DBG_ANY, "Waiting for Peripheral Mode, Mode=%s count = %d\n",
			   (dwc_otg_is_host_mode(core_if) ? "Host" :
			    "Peripheral"), count);
		dwc_mdelay(1); //vahrama previous value was 100
		if (++count > 10000)
			break;
	}
	DWC_ASSERT(++count < 10000,
		   "Connection id status change timed out");
	core_if->op_state = B_PERIPHERAL;

	if(core_if->otg_ver == 0)
		dwc_otg_core_init(core_if);
	dwc_otg_enable_global_interrupts(core_if);
	DWC_SPINLOCK(core_if->self_lock);
	cil_pcd_start(core_if);
	DWC_SPINUNLOCK(core_if->self_lock);
}

void dwc_otg_change_drd_mode(dwc_otg_device_t *otg_dev, unsigned int mode)
{
	dwc_otg_core_if_t *core_if = otg_dev->core_if;
	gusbcfg_data_t gusbcfg;
	gusbcfg.d32 = DWC_READ_REG32(&core_if->core_global_regs->gusbcfg);

	if(core_if->phy_mode == USBPHY_MODE_OFF)
	{
		printk("\x1b[1;33m[%s:%d] USBPHY is in OFF state. Reset the USBPHY. (%s)\x1b[0m\n",
			__func__, __LINE__, mode == DWC_OTG_MODE_HOST ? "Host" : "Device");
		tcc_otg_phy_init(otg_dev);
	}
	if (mode == DWC_OTG_MODE_HOST) {
		struct usb_phy *phy = otg_dev->dwc_otg_phy;
		phy->set_phy_state(phy, USBPHY_MODE_START);

		dwc_otg_disable_global_interrupts(core_if);
		DWC_SPINLOCK(core_if->self_lock);
		cil_pcd_stop(core_if);
		DWC_SPINUNLOCK(core_if->self_lock);

		gusbcfg.b.force_host_mode = 1;
		gusbcfg.b.force_dev_mode = 0;
		DWC_WRITE_REG32(&core_if->core_global_regs->gusbcfg, gusbcfg.d32);
#ifndef CONFIG_TCC_DWC_OTG_HOST_MUX
		dwc_otg_drd_host_init(core_if);
#endif
	} else if (mode == DWC_OTG_MODE_DEVICE) {
		dwc_otg_disable_global_interrupts(core_if);
#ifndef CONFIG_TCC_DWC_OTG_HOST_MUX
		DWC_SPINLOCK(core_if->self_lock);
		cil_hcd_stop(core_if);
		DWC_SPINUNLOCK(core_if->self_lock);
#endif
		gusbcfg.b.force_host_mode = 0;
		gusbcfg.b.force_dev_mode = 1;
		DWC_WRITE_REG32(&core_if->core_global_regs->gusbcfg, gusbcfg.d32);
		dwc_otg_drd_dev_init(core_if);
	}
}
#define VBUS_CTRL_MAX 10
unsigned int dwc_otg_set_drd_mode(dwc_otg_device_t *otg_dev, unsigned int mode)
{
	unsigned int 	res = 0;

	if((mode != 0xFF) && (otg_dev->current_mode != mode)) {
		otg_dev->current_mode = 0xFF;  // drd mode change start
		if (mode == DWC_OTG_MODE_HOST) {
			int retry_cnt=0;
			printk("dwc_otg switching to host role\n" );

			#ifdef TCC_DWC_SOFFN_USE
			if(otg_dev->dwc_otg_soffn_thread != NULL)
			{
				kthread_stop(otg_dev->dwc_otg_soffn_thread);
				otg_dev->dwc_otg_soffn_thread = NULL;
				cil_pcd_disconnect(otg_dev->core_if);
			}
#ifndef CONFIG_TCC_EH_ELECT_TST
			do
			{
				if(tcc_otg_vbus_ctrl(otg_dev, ON) == 0) //VBUS on
				{
					break;
				}
				else if(!vbus_control_enable)
				{
					break;
				}
				else
				{
					
					retry_cnt++;
					msleep(50);
					printk("[%s]Retry to control vbus(%d)!\n", __func__, retry_cnt);
				}
			} while(retry_cnt < VBUS_CTRL_MAX);
#endif
			#endif /* TCC_DWC_SOFFN_USE */
		}

		if (mode == DWC_OTG_MODE_DEVICE) {
			printk("dwc_otg switching to device role\n");
		}

		#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
		if (mode == DWC_OTG_MODE_DEVICE) {
			// mux ehci host disable
			otg_dev->core_if->mux_own = MUX_MODE_DEVICE;
			if(is_resuming == 0)
				usbotgh_hcd_remove(otg_dev);
			tcc_otg_phy_init(otg_dev);
		}
		#endif

		dwc_otg_change_drd_mode(otg_dev, mode);

		#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
		if (mode == DWC_OTG_MODE_HOST) {
			// mux ehci host enable
			otg_dev->core_if->mux_own = MUX_MODE_HOST;
			if (!otg_dev->mhst_phy || !otg_dev->mhst_phy->init) {
				printk("[%s:%d]PHY driver is needed\n", __func__, __LINE__);
				return -1;
			}
			otg_dev->mhst_phy->init(otg_dev->mhst_phy);
			if(is_resuming == 0)
				dwc_otg_mux_hcd_init(otg_dev);
			#ifdef CONFIG_TCC_EH_ELECT_TST
			tcc_otg_vbus_ctrl(otg_dev, ON);
			#endif
		}
		#endif

		#ifdef TCC_DWC_SOFFN_USE
		if (mode == DWC_OTG_MODE_HOST) {
		}

		if (mode == DWC_OTG_MODE_DEVICE) {
			// vbus on	& device mode = iap2
			if(otg_dev->vbus_status == 1) {
				if (otg_dev->dwc_otg_soffn_thread != NULL) {
					kthread_stop(otg_dev->dwc_otg_soffn_thread);
					otg_dev->dwc_otg_soffn_thread = NULL;
				}

				/* Start up our control thread */
				otg_dev->dwc_otg_soffn_thread = kthread_run(dwc_otg_soffn_monitor_thread, (void*)otg_dev, "dwc_otg-soffn");
				if (IS_ERR(otg_dev->dwc_otg_soffn_thread)) {
					printk("\x1b[1;33m[%s:%d]\x1b[0m thread error\n", __func__, __LINE__);
					res = PTR_ERR(otg_dev->dwc_otg_soffn_thread);
				}
			}
		}
		#endif /* TCC_DWC_SOFFN_USE*/

		otg_dev->current_mode = mode; // drd mode change complete. current mode update
		printk("[dwc_otg mode chage complete: %s]\n", (otg_dev->current_mode == DWC_OTG_MODE_HOST)? "Host":"Device");
	} else {
		printk("Current mode is %s\n", (otg_dev->current_mode == DWC_OTG_MODE_HOST) ? "Host":"Device");
		res = 0;
	}

	return res;
}

static void dwc_otg_drd_work(struct work_struct *data)
{
	dwc_otg_device_t *otg_dev = container_of(data, dwc_otg_device_t,
						drd_work);

	dwc_otg_set_drd_mode(otg_dev, otg_dev->new_mode);
}

/**
 * This function is called when an lm_device is bound to a
 * dwc_otg_driver. It creates the driver components required to
 * control the device (CIL, HCD, and PCD) and it initializes the
 * device. The driver components are stored in a dwc_otg_device
 * structure. A reference to the dwc_otg_device is saved in the
 * lm_device. This allows the driver to access the dwc_otg_device
 * structure on subsequent calls to driver methods for this device.
 *
 * @param _dev Bus device
 */
static int dwc_otg_driver_probe(struct platform_device *_dev)
{
	int retval = 0;
	dwc_otg_device_t *dwc_otg_device;
	//struct resource *resource;
	int irq;

	dev_dbg(&_dev->dev, "dwc_otg_driver_probe(%p)\n", _dev);
#ifdef CONFIG_ARCH_TCC897X
	get_tcc_chip_info();
#endif

	dwc_otg_device = DWC_ALLOC(sizeof(dwc_otg_device_t));
	if (!dwc_otg_device) {
		dev_err(&_dev->dev, "kmalloc of dwc_otg_device failed\n");
		return -ENOMEM;
	}
	g_dwc_otg = dwc_otg_device;

	memset(dwc_otg_device, 0, sizeof(*dwc_otg_device));
	dwc_otg_device->os_dep.reg_offset = 0xFFFFFFFF;

#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
	dwc_otg_device->probe_complete = 0; // Probe complete flag
#endif

	/*
	 * Map the DWC_otg Core memory into virtual address space.
	 */
	if (!request_mem_region(_dev->resource[0].start,
	                    _dev->resource[0].end - _dev->resource[0].start + 1,
	                    "dwc_otg")) {
		dev_dbg(&_dev->dev, "error reserving mapped memory\n");
		printk("\x1b[1;31m[%s:%d]\x1b[0m\n", __func__, __LINE__);
		retval = -EFAULT;
		goto fail;
	}

	dwc_otg_device->os_dep.base = ioremap_nocache(_dev->resource[0].start,
                                  _dev->resource[0].end - _dev->resource[0].start + 1);

	dev_dbg(&_dev->dev, "base=0x%08x\n", (unsigned)dwc_otg_device->os_dep.base);

	if (!dwc_otg_device->os_dep.base) {
		dev_err(&_dev->dev, "ioremap() failed\n");
		retval = -ENOMEM;
		goto fail;
	}
	dev_dbg(&_dev->dev, "base=0x%08x\n",
                (unsigned)dwc_otg_device->os_dep.base);

#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
	/*
	 * Get EHCI address space
	 */
	dwc_otg_device->ehci_regs = (void __iomem*)_dev->resource[2].start;
	dwc_otg_device->ehci_regs_size = _dev->resource[2].end - _dev->resource[2].start+1;

	dev_dbg(&_dev->dev, "base=0x%08x\n", (unsigned)dwc_otg_device->ehci_regs);

	if (!dwc_otg_device->ehci_regs) {
		dev_err(&_dev->dev, "ioremap() failed\n");
		retval = -ENOMEM;
		goto fail;
	}

	/*
	 * Get OHCI address space
	 */
	dwc_otg_device->ohci_regs = (void __iomem*)_dev->resource[4].start;
	dwc_otg_device->ohci_regs_size = _dev->resource[4].end - _dev->resource[4].start + 1;

	dev_dbg(&_dev->dev, "base=0x%08x\n", (unsigned)dwc_otg_device->ohci_regs);

	if (!dwc_otg_device->ohci_regs) {
		dev_err(&_dev->dev, "ioremap() failed\n");
		retval = -ENOMEM;
		goto fail;
	}
#endif

	dwc_otg_device_parse_dt(_dev, dwc_otg_device);

	/* clock control register */
#if defined(CONFIG_ARCH_TCC)
//	clk_prepare_enable(dwc_otg_device->core_if->pclk);
#else
	dwc_otg_device->clk[0] = clk_get(NULL, "usb_otg");
	if (IS_ERR(dwc_otg_device->clk[0])){
		printk("ERR - usb_otg clk_get fail.\n");
		goto fail;
	}
	clk_prepare_enable(dwc_otg_device->clk[0]);
#endif
	/*
	 * Initialize driver data to point to the global DWC_otg
	 * Device structure.
	 */

	platform_set_drvdata(_dev, dwc_otg_device);

	dev_dbg(&_dev->dev, "dwc_otg_device=0x%p\n", dwc_otg_device);

	/* Set tcc phyconfiguration address */
	dwc_otg_device->core_if = dwc_otg_cil_alloc();
	dwc_otg_device->core_if->tcc_phy_config = (tcc_dwc_otg_phy_t *)dwc_otg_device->dwc_otg_phy->base;

#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
	if (dwc_otg_device->mhst_phy && dwc_otg_device->mhst_phy->otg) {
		dwc_otg_device->core_if->ehci_phy_regs = dwc_otg_device->mhst_phy->base;
		dwc_otg_device->mhst_phy->otg->mux_cfg_addr = &dwc_otg_device->core_if->tcc_phy_config->otgmux;
	}
#endif
	/*
	 * Initialize Dwc_otg VBUS and PHY.
	 */
	tcc_otg_vbus_init();
	tcc_otg_phy_init(dwc_otg_device);

	/*
	 * Turn on DWC_otg core.
	 */
	clk_prepare_enable(dwc_otg_device->hclk); // HSIO BUS CLK
	clk_set_rate(dwc_otg_device->pclk, dwc_otg_device->core_clk_rate);
	printk("dwc_otg tcc: clk rate %lu\n", clk_get_rate(dwc_otg_device->pclk));

	/*
	 * Turn on DWC_otg phy clk
	 */
	if(dwc_otg_device->phy_clk)
		clk_prepare_enable(dwc_otg_device->phy_clk);

	dwc_otg_device->core_if = dwc_otg_cil_init(dwc_otg_device->os_dep.base, dwc_otg_device->core_if);
	if (!dwc_otg_device->core_if) {
		dev_err(&_dev->dev, "CIL initialization failed!\n");
		retval = -ENOMEM;
		goto fail;
	}

	/*
	 * Attempt to ensure this device is really a DWC_otg Controller.
	 * Read and verify the SNPSID register contents. The value should be
	 * 0x45F42XXX or 0x45F42XXX, which corresponds to either "OT2" or "OTG3",
	 * as in "OTG version 2.XX" or "OTG version 3.XX".
	 */

	if (((dwc_otg_get_gsnpsid(dwc_otg_device->core_if) & 0xFFFFF000) !=	0x4F542000) &&
		((dwc_otg_get_gsnpsid(dwc_otg_device->core_if) & 0xFFFFF000) != 0x4F543000)) {
		dev_err(&_dev->dev, "Bad value for SNPSID: 0x%08x\n",
			dwc_otg_get_gsnpsid(dwc_otg_device->core_if));
		retval = -EINVAL;
		goto fail;
	}

	/*
	 * Validate parameter values.
	 */
	dev_dbg(&_dev->dev, "Calling set_parameters\n");
	if (set_parameters(dwc_otg_device->core_if)) {
		retval = -EINVAL;
		goto fail;
	}

	/*
	 * Create Device Attributes in sysfs
	 */
	dev_dbg(&_dev->dev, "Calling attr_create\n");
	dwc_otg_attr_create(_dev);

	/*
	 * Disable the global interrupt until all the interrupt
	 * handlers are installed.
	 */
	dev_dbg(&_dev->dev, "Calling disable_global_interrupts\n");
	dwc_otg_disable_global_interrupts(dwc_otg_device->core_if);

	/*
	 * Install the interrupt handler for the common interrupts before
	 * enabling common interrupts in core_init below.
	 */
	irq = platform_get_irq(_dev, 0);
	if (irq < 0) {
		dev_err(&_dev->dev, "no irq? (irq=%d)\n", irq);
		retval = -ENODEV;
		goto fail;
	}
	DWC_DEBUGPL(DBG_CIL, "registering (common) handler for irq%d\n", irq);

#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
	dwc_otg_device->ehci_irq = platform_get_irq(_dev, 1);
#endif
	retval = request_irq(irq, dwc_otg_common_irq,
			     IRQF_SHARED, "dwc_otg",
                dwc_otg_device);
	if (retval) {
		DWC_ERROR("request of irq%d failed\n", irq);
		retval = -EBUSY;
		goto fail;
	} else {
		dwc_otg_device->common_irq_installed = 1;
	}

#ifdef CONFIG_TCC_DWC_IRQ_AFFINITY
	/* Set the irq affinity in order to handle the irq more stably */
	{
		unsigned int cpu = CONFIG_TCC_DWC_IRQ_AFFINITY;
		retval = irq_set_affinity(irq, cpumask_of(cpu));
		if(retval) {
			dev_err(&_dev->dev, "failed to set the irq affinity irq %d cpu %d err %d\n",
				irq, cpu, retval);
			return retval;
		}
		dev_info(&_dev->dev, "set the irq(%d) affinity to cpu(%d)\n", irq, cpu);
	}
#endif

	/*
	 * Initialize the DWC_otg core.
	 */
	dwc_otg_core_init(dwc_otg_device->core_if);

#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
	dwc_otg_device->core_if->vbus_ctrl = tcc_otg_vbus_ctrl;
	dwc_otg_device->core_if->dwc_otg_dev = dwc_otg_device;
	dwc_otg_device->core_if->enable_hcd_conn_timer= ON;
#endif /* CONFIG_TCC_DWC_HS_ELECT_TST */

#ifndef DWC_HOST_ONLY
	/*
	 * Initialize the PCD
	 */
	retval = pcd_init(_dev);
	if (retval != 0) {
		DWC_ERROR("pcd_init failed\n");
		dwc_otg_device->pcd = NULL;
		goto fail;
	}
#endif

#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
	printk("dwc_otg tcc: mux host enable\n");
#else
#ifndef DWC_DEVICE_ONLY
	/*
	 * Initialize the HCD
	 */
	retval = hcd_init(_dev);
	if (retval != 0) {
		DWC_ERROR("hcd_init failed\n");
		dwc_otg_device->hcd = NULL;
		goto fail;
	}
#ifndef CONFIG_TCC_DWC_HS_ELECT_TST
	tcc_otg_vbus_ctrl(dwc_otg_device, ON);
#endif

#endif
#endif	/* CONFIG_TCC_DWC_OTG_HOST_MUX */

#if defined(CONFIG_TCC_USB_IAP2)
	dwc_otg_device->core_if->dev = &_dev->dev;
#endif
	platform_set_drvdata(_dev, dwc_otg_device);

	dwc_otg_device->drd_wq = create_singlethread_workqueue("dwc_otg");
	if (!dwc_otg_device->drd_wq) {
		return -ENOMEM;
	}
	INIT_WORK(&dwc_otg_device->drd_work, dwc_otg_drd_work);

	retval = device_create_file(&_dev->dev, &dev_attr_drdmode);
	if (retval) {
		DWC_ERROR("failed to create drdmode\n");
	}

	retval = device_create_file(&_dev->dev, &dev_attr_vbus);
	if (retval) {
		DWC_ERROR("failed to create vbus\n");
	}

	retval = device_create_file(&_dev->dev, &dev_attr_debug_level);
	if (retval) {
		DWC_ERROR("failed to create debuglevel\n");
	}

	retval = device_create_file(&_dev->dev, &dev_attr_phystatus);
	if (retval) {
		DWC_ERROR("failed to create vbus\n");
	}

#if defined(CONFIG_TCC_DWC_OTG_DUAL_FIRST_DEV)
	dwc_otg_device->current_mode = DWC_OTG_MODE_DEVICE; // drd mode change complete. current mode update
#elif defined(CONFIG_TCC_DWC_OTG_DUAL_FIRST_HOST)
	dwc_otg_device->current_mode = DWC_OTG_MODE_HOST; // drd mode change complete. current mode update
#else
	dwc_otg_device->current_mode = DWC_OTG_MODE_DEVICE; // drd mode change complete. current mode update
#endif /* CONFIG_TCC_DWC_OTG_DUAL_FIRST_DEV */

#ifndef CONFIG_TCC_DWC_HS_ELECT_TST
	tcc_otg_vbus_ctrl(dwc_otg_device, dwc_otg_device->current_mode == DWC_OTG_MODE_DEVICE ? OFF : ON);
	dwc_otg_change_drd_mode(dwc_otg_device, dwc_otg_device->current_mode);
#else
	printk("\x1b[1;43m ## DWC_OTG COMPLIANCE TEST MODE ##\x1b[0m\n");
#endif /* CONFIG_TCC_DWC_HS_ELECT_TST */
#if defined(CONFIG_TCC_DWC_OTG_HOST_MUX) && defined(CONFIG_TCC_DWC_OTG_DUAL_FIRST_HOST)		/* 016.09.05 */
	dwc_otg_device->current_mode = DWC_OTG_MODE_DEVICE;
	dwc_otg_set_drd_mode(dwc_otg_device, DWC_OTG_MODE_HOST);
#endif /* CONFIG_TCC_DWC_OTG_HOST_MUX */

	/*
	 * Enable the global interrupt after all the interrupt
	 * handlers are installed if there is no ADP support else 
	 * perform initial actions required for Internal ADP logic.
	 */
	if (!dwc_otg_get_param_adp_enable(dwc_otg_device->core_if)) {	
        dev_dbg(&_dev->dev, "Calling enable_global_interrupts\n");
		dwc_otg_enable_global_interrupts(dwc_otg_device->core_if);
	    dev_dbg(&_dev->dev, "Done\n");
	} else{
			dwc_otg_adp_start(dwc_otg_device->core_if, dwc_otg_is_host_mode(dwc_otg_device->core_if));
	}

#ifdef CONFIG_TCC_DWC_HS_ELECT_TST
	dwc_otg_device->probe_complete = 1; // Probe complete flag
#endif

#if 0//def CONFIG_ANDROID
	if(dwc_otg_is_device_mode(dwc_otg_device->core_if))
	{
		struct usb_phy *phy = dwc_otg_device->dwc_otg_phy;
		phy->set_phy_state(phy, USBPHY_MODE_STOP);
	}
#endif
	return 0;

fail:
	dwc_otg_driver_remove(_dev);
	return retval;
}
#ifdef CONFIG_PM
static int dwc_otg_driver_suspend(struct platform_device *pdev, pm_message_t state)
{
	dwc_otg_device_t *dwc_otg_device = platform_get_drvdata(pdev);
	struct usb_phy *phy = dwc_otg_device->dwc_otg_phy;

	int irq = platform_get_irq(pdev, 0);
	printk("disable irq(%d)\n", irq);
	disable_irq(irq);

	dwc_otg_save_global_regs(dwc_otg_device->core_if);
	dwc_otg_disable_global_interrupts(dwc_otg_device->core_if);
	/* USB Phy off */
	phy->set_phy_state(phy, USBPHY_MODE_STOP);
	tcc_otg_vbus_ctrl(dwc_otg_device, OFF);

	msleep_interruptible(500);

//#if defined(CONFIG_ARCH_TCC892X) || defined(CONFIG_CHIP_TCC8935S) || defined(CONFIG_CHIP_TCC8933S) 
//|| defined(CONFIG_CHIP_TCC8937S)
//	clk_disable(dwc_otg_device->clk[0]);
//#endif

	if(__clk_is_enabled(dwc_otg_device->hclk))
		clk_disable(dwc_otg_device->hclk);
	clk_set_rate(dwc_otg_device->pclk, dwc_otg_device->core_clk_rate);

	tcc_otg_vbus_exit();
	
	return 0;
}

static int dwc_otg_driver_resume(struct platform_device *pdev)
{
//	int id = -1;
	dwc_otg_device_t *dwc_otg_device;
	is_resuming = 1;
	dwc_otg_device = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	tcc_otg_vbus_init();

	clk_prepare_enable(dwc_otg_device->hclk);
	clk_set_rate(dwc_otg_device->pclk, dwc_otg_device->core_clk_rate);

	tcc_otg_phy_init(dwc_otg_device);
#ifdef CONFIG_TCC_DWC_OTG_HOST_MUX
	//dwc_otg_device->mhst_phy->init(dwc_otg_device->mhst_phy);
	dwc_otg_device->mhst_phy->set_phy_mux_sel(dwc_otg_device->mhst_phy, 1);
#endif

	//tcc_usb_link_reset(dwc_otg_device->core_if); // 20150924 Need to verify.
	dwc_otg_cil_reinit(dwc_otg_device->core_if, dwc_otg_device->os_dep.base, (dwc_otg_core_params_t *)&dwc_otg_module_params);
	//dwc_otg_core_init(dwc_otg_device->core_if);
	dwc_otg_restore_global_regs(dwc_otg_device->core_if);
	//dwc_otg_enable_global_interrupts(dwc_otg_device->core_if);

	/* GetConnIdStatus will be updated after OTG powerup. */
	//id = GetConnIdStatus(dwc_otg_device->core_if);
	//
	//dwc_otg_device->flagDeviceVBUS = -1;
	//dwc_otg_device->flagID = -1;
	//dwc_otg_device->flagDeviceAttach = 0;

#ifndef CONFIG_TCC_DWC_HS_ELECT_TST
	tcc_otg_vbus_ctrl(dwc_otg_device, dwc_otg_device->current_mode == DWC_OTG_MODE_DEVICE ? OFF : ON);
	dwc_otg_change_drd_mode(dwc_otg_device, dwc_otg_device->current_mode);
#endif

	dwc_otg_enable_global_interrupts(dwc_otg_device->core_if);
	
	printk("enable irq(%d)\n", irq);
	enable_irq(irq);
	if (dwc_otg_device->current_mode == DWC_OTG_MODE_DEVICE) {
		dwc_otg_set_drd_mode(dwc_otg_device, DWC_OTG_MODE_HOST);
		dwc_otg_set_drd_mode(dwc_otg_device, DWC_OTG_MODE_DEVICE);
	}	
	
	is_resuming = 0;
	
	return 0;
}
#else
#define dwc_otg_driver_suspend	NULL
#define dwc_otg_driver_resume	NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_dwc_otg_match[] = {
	{ .compatible = "telechips,dwc_otg" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_dwc_otg_match);
#endif
/**
 * This structure defines the methods to be called by a bus driver
 * during the lifecycle of a device on that bus. Both drivers and
 * devices are registered with a bus driver. The bus driver matches
 * devices to drivers based on information in the device and driver
 * structures.
 *
 * The probe function is called when the bus driver matches a device
 * to this driver. The remove function is called when a device is
 * unregistered with the bus driver.
 */
static struct platform_driver dwc_otg_driver = {
	.probe		= dwc_otg_driver_probe,
	.remove		= dwc_otg_driver_remove,
	.suspend	= dwc_otg_driver_suspend,
	.resume		= dwc_otg_driver_resume,
	.driver		= {
		.name	= (char *)dwc_driver_name,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_dwc_otg_match),
#endif
	},
};


/**
 * This function is called when the dwc_otg_driver is installed with the
 * insmod command. It registers the dwc_otg_driver structure with the
 * appropriate bus driver. This will cause the dwc_otg_driver_probe function
 * to be called. In addition, the bus driver will automatically expose
 * attributes defined for the device and driver in the special sysfs file
 * system.
 *
 * @return
 */
static int __init dwc_otg_driver_init(void)
{
	int retval = 0, ret;

#if defined(DWC_HOST_ONLY)
	char *p = "HostOnly";
#elif defined(DWC_DEVICE_ONLY)
	char *p = "DeviceOnly";
#elif defined(DWC_DUAL_ROLE)
	char *p = "DualRole";
#elif defined(CONFIG_TCC_DWC_OTG_2PORT_0OTG_1HOST)
	char *p = "0OTG_1HOST";
#elif defined(CONFIG_TCC_DWC_OTG_2PORT_0HOST_1OTG)
	char *p = "0HOST_1OTG";
#else
	char *p = "Unknown";
#endif

#if defined(TCC_OTG_WAKE_LOCK)
	otg_wakelock_init();
#endif /* TCC_OTG_WAKE_LOCK */

	printk(KERN_INFO "%s: version %s (%s)\n", dwc_driver_name,
	       DWC_DRIVER_VERSION, p);
	printk(KERN_INFO "Working version %s\n", "No 007 - 10/24/2007");

	retval = platform_driver_register(&dwc_otg_driver);
	if (retval < 0) {
		printk(KERN_ERR "%s retval=%d\n", __func__, retval);
	
	#if defined(TCC_OTG_WAKE_LOCK)
		otg_wakelock_exit();
	#endif /* TCC_OTG_WAKE_LOCK */	
		return retval;
	}

	ret = driver_create_file(&dwc_otg_driver.driver, &driver_attr_version);
	//ret = driver_create_file(&dwc_otg_driver.driver, &driver_attr_debuglevel);

	return retval;
}

module_init(dwc_otg_driver_init);

/**
 * This function is called when the driver is removed from the kernel
 * with the rmmod command. The driver unregisters itself with its bus
 * driver.
 *
 */
static void __exit dwc_otg_driver_cleanup(void)
{
	printk(KERN_DEBUG "dwc_otg_driver_cleanup()\n");

	//driver_remove_file(&dwc_otg_driver.driver, &driver_attr_debuglevel);
	driver_remove_file(&dwc_otg_driver.driver, &driver_attr_version);
	
	platform_driver_unregister(&dwc_otg_driver);

	printk(KERN_INFO "%s module removed\n", dwc_driver_name);
}

module_exit(dwc_otg_driver_cleanup);

MODULE_DESCRIPTION(DWC_DRIVER_DESC);
MODULE_AUTHOR("Synopsys Inc.");
MODULE_LICENSE("GPL");

module_param_named(otg_cap, dwc_otg_module_params.otg_cap, int, 0444);
MODULE_PARM_DESC(otg_cap, "OTG Capabilities 0=HNP&SRP 1=SRP Only 2=None");
module_param_named(opt, dwc_otg_module_params.opt, int, 0444);
MODULE_PARM_DESC(opt, "OPT Mode");
module_param_named(dma_enable, dwc_otg_module_params.dma_enable, int, 0444);
MODULE_PARM_DESC(dma_enable, "DMA Mode 0=Slave 1=DMA enabled");

module_param_named(dma_desc_enable, dwc_otg_module_params.dma_desc_enable, int,
		   0444);
MODULE_PARM_DESC(dma_desc_enable,
		 "DMA Desc Mode 0=Address DMA 1=DMA Descriptor enabled");

module_param_named(dma_burst_size, dwc_otg_module_params.dma_burst_size, int,
		   0444);
MODULE_PARM_DESC(dma_burst_size,
		 "DMA Burst Size 1, 4, 8, 16, 32, 64, 128, 256");
module_param_named(speed, dwc_otg_module_params.speed, int, 0444);
MODULE_PARM_DESC(speed, "Speed 0=High Speed 1=Full Speed");
module_param_named(host_support_fs_ls_low_power,
		   dwc_otg_module_params.host_support_fs_ls_low_power, int,
		   0444);
MODULE_PARM_DESC(host_support_fs_ls_low_power,
		 "Support Low Power w/FS or LS 0=Support 1=Don't Support");
module_param_named(host_ls_low_power_phy_clk,
		   dwc_otg_module_params.host_ls_low_power_phy_clk, int, 0444);
MODULE_PARM_DESC(host_ls_low_power_phy_clk,
		 "Low Speed Low Power Clock 0=48Mhz 1=6Mhz");
module_param_named(enable_dynamic_fifo,
		   dwc_otg_module_params.enable_dynamic_fifo, int, 0444);
MODULE_PARM_DESC(enable_dynamic_fifo, "0=cC Setting 1=Allow Dynamic Sizing");
module_param_named(data_fifo_size, dwc_otg_module_params.data_fifo_size, int,
		   0444);
MODULE_PARM_DESC(data_fifo_size,
		 "Total number of words in the data FIFO memory 32-32768");
module_param_named(dev_rx_fifo_size, dwc_otg_module_params.dev_rx_fifo_size,
		   int, 0444);
MODULE_PARM_DESC(dev_rx_fifo_size, "Number of words in the Rx FIFO 16-32768");
module_param_named(dev_nperio_tx_fifo_size,
		   dwc_otg_module_params.dev_nperio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(dev_nperio_tx_fifo_size,
		 "Number of words in the non-periodic Tx FIFO 16-32768");
module_param_named(dev_perio_tx_fifo_size_1,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[0], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_1,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_2,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[1], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_2,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_3,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[2], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_3,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_4,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[3], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_4,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_5,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[4], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_5,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_6,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[5], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_6,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_7,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[6], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_7,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_8,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[7], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_8,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_9,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[8], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_9,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_10,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[9], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_10,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_11,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[10], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_11,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_12,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[11], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_12,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_13,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[12], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_13,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_14,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[13], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_14,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_15,
		   dwc_otg_module_params.dev_perio_tx_fifo_size[14], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_15,
		 "Number of words in the periodic Tx FIFO 4-768");
module_param_named(host_rx_fifo_size, dwc_otg_module_params.host_rx_fifo_size,
		   int, 0444);
MODULE_PARM_DESC(host_rx_fifo_size, "Number of words in the Rx FIFO 16-32768");
module_param_named(host_nperio_tx_fifo_size,
		   dwc_otg_module_params.host_nperio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(host_nperio_tx_fifo_size,
		 "Number of words in the non-periodic Tx FIFO 16-32768");
module_param_named(host_perio_tx_fifo_size,
		   dwc_otg_module_params.host_perio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(host_perio_tx_fifo_size,
		 "Number of words in the host periodic Tx FIFO 16-32768");
module_param_named(max_transfer_size, dwc_otg_module_params.max_transfer_size,
		   int, 0444);
/** @todo Set the max to 512K, modify checks */
MODULE_PARM_DESC(max_transfer_size,
		 "The maximum transfer size supported in bytes 2047-65535");
module_param_named(max_packet_count, dwc_otg_module_params.max_packet_count,
		   int, 0444);
MODULE_PARM_DESC(max_packet_count,
		 "The maximum number of packets in a transfer 15-511");
module_param_named(host_channels, dwc_otg_module_params.host_channels, int,
		   0444);
MODULE_PARM_DESC(host_channels,
		 "The number of host channel registers to use 1-16");
module_param_named(dev_endpoints, dwc_otg_module_params.dev_endpoints, int,
		   0444);
MODULE_PARM_DESC(dev_endpoints,
		 "The number of endpoints in addition to EP0 available for device mode 1-15");
module_param_named(phy_type, dwc_otg_module_params.phy_type, int, 0444);
MODULE_PARM_DESC(phy_type, "0=Reserved 1=UTMI+ 2=ULPI");
module_param_named(phy_utmi_width, dwc_otg_module_params.phy_utmi_width, int,
		   0444);
MODULE_PARM_DESC(phy_utmi_width, "Specifies the UTMI+ Data Width 8 or 16 bits");
module_param_named(phy_ulpi_ddr, dwc_otg_module_params.phy_ulpi_ddr, int, 0444);
MODULE_PARM_DESC(phy_ulpi_ddr,
		 "ULPI at double or single data rate 0=Single 1=Double");
module_param_named(phy_ulpi_ext_vbus, dwc_otg_module_params.phy_ulpi_ext_vbus,
		   int, 0444);
MODULE_PARM_DESC(phy_ulpi_ext_vbus,
		 "ULPI PHY using internal or external vbus 0=Internal");
module_param_named(i2c_enable, dwc_otg_module_params.i2c_enable, int, 0444);
MODULE_PARM_DESC(i2c_enable, "FS PHY Interface");
module_param_named(ulpi_fs_ls, dwc_otg_module_params.ulpi_fs_ls, int, 0444);
MODULE_PARM_DESC(ulpi_fs_ls, "ULPI PHY FS/LS mode only");
module_param_named(ts_dline, dwc_otg_module_params.ts_dline, int, 0444);
MODULE_PARM_DESC(ts_dline, "Term select Dline pulsing for all PHYs");
module_param_named(debug, g_dbg_lvl, int, 0444);
MODULE_PARM_DESC(debug, "");

module_param_named(en_multiple_tx_fifo,
		   dwc_otg_module_params.en_multiple_tx_fifo, int, 0444);
MODULE_PARM_DESC(en_multiple_tx_fifo,
		 "Dedicated Non Periodic Tx FIFOs 0=disabled 1=enabled");
module_param_named(dev_tx_fifo_size_1,
		   dwc_otg_module_params.dev_tx_fifo_size[0], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_1, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_2,
		   dwc_otg_module_params.dev_tx_fifo_size[1], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_2, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_3,
		   dwc_otg_module_params.dev_tx_fifo_size[2], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_3, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_4,
		   dwc_otg_module_params.dev_tx_fifo_size[3], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_4, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_5,
		   dwc_otg_module_params.dev_tx_fifo_size[4], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_5, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_6,
		   dwc_otg_module_params.dev_tx_fifo_size[5], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_6, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_7,
		   dwc_otg_module_params.dev_tx_fifo_size[6], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_7, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_8,
		   dwc_otg_module_params.dev_tx_fifo_size[7], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_8, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_9,
		   dwc_otg_module_params.dev_tx_fifo_size[8], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_9, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_10,
		   dwc_otg_module_params.dev_tx_fifo_size[9], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_10, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_11,
		   dwc_otg_module_params.dev_tx_fifo_size[10], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_11, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_12,
		   dwc_otg_module_params.dev_tx_fifo_size[11], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_12, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_13,
		   dwc_otg_module_params.dev_tx_fifo_size[12], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_13, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_14,
		   dwc_otg_module_params.dev_tx_fifo_size[13], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_14, "Number of words in the Tx FIFO 4-768");
module_param_named(dev_tx_fifo_size_15,
		   dwc_otg_module_params.dev_tx_fifo_size[14], int, 0444);
MODULE_PARM_DESC(dev_tx_fifo_size_15, "Number of words in the Tx FIFO 4-768");

module_param_named(thr_ctl, dwc_otg_module_params.thr_ctl, int, 0444);
MODULE_PARM_DESC(thr_ctl,
		 "Thresholding enable flag bit 0 - non ISO Tx thr., 1 - ISO Tx thr., 2 - Rx thr.- bit 0=disabled 1=enabled");
module_param_named(tx_thr_length, dwc_otg_module_params.tx_thr_length, int,
		   0444);
MODULE_PARM_DESC(tx_thr_length, "Tx Threshold length in 32 bit DWORDs");
module_param_named(rx_thr_length, dwc_otg_module_params.rx_thr_length, int,
		   0444);
MODULE_PARM_DESC(rx_thr_length, "Rx Threshold length in 32 bit DWORDs");

module_param_named(pti_enable, dwc_otg_module_params.pti_enable, int, 0444);
module_param_named(mpi_enable, dwc_otg_module_params.mpi_enable, int, 0444);
module_param_named(lpm_enable, dwc_otg_module_params.lpm_enable, int, 0444);
MODULE_PARM_DESC(lpm_enable, "LPM Enable 0=LPM Disabled 1=LPM Enabled");

module_param_named(besl_enable, dwc_otg_module_params.besl_enable, int, 0444);
MODULE_PARM_DESC(besl_enable, "BESL Enable 0=BESL Disabled 1=BESL Enabled");
module_param_named(baseline_besl, dwc_otg_module_params.baseline_besl, int, 0444);
MODULE_PARM_DESC(baseline_besl, "Set the baseline besl value");
module_param_named(deep_besl, dwc_otg_module_params.deep_besl, int, 0444);
MODULE_PARM_DESC(deep_besl, "Set the deep besl value");

module_param_named(ic_usb_cap, dwc_otg_module_params.ic_usb_cap, int, 0444);
MODULE_PARM_DESC(ic_usb_cap,
		 "IC_USB Capability 0=IC_USB Disabled 1=IC_USB Enabled");
module_param_named(ahb_thr_ratio, dwc_otg_module_params.ahb_thr_ratio, int,
		   0444);
MODULE_PARM_DESC(ahb_thr_ratio, "AHB Threshold Ratio");
module_param_named(power_down, dwc_otg_module_params.power_down, int, 0444);
MODULE_PARM_DESC(power_down, "Power Down Mode");
module_param_named(reload_ctl, dwc_otg_module_params.reload_ctl, int, 0444);
MODULE_PARM_DESC(reload_ctl, "HFIR Reload Control");
module_param_named(dev_out_nak, dwc_otg_module_params.dev_out_nak, int, 0444);
MODULE_PARM_DESC(dev_out_nak, "Enable Device OUT NAK");
module_param_named(cont_on_bna, dwc_otg_module_params.cont_on_bna, int, 0444);
MODULE_PARM_DESC(cont_on_bna, "Enable Enable Continue on BNA");
module_param_named(ahb_single, dwc_otg_module_params.ahb_single, int, 0444);
MODULE_PARM_DESC(ahb_single, "Enable AHB Single Support");
module_param_named(adp_enable, dwc_otg_module_params.adp_enable, int, 0444);
MODULE_PARM_DESC(adp_enable, "ADP Enable 0=ADP Disabled 1=ADP Enabled");
module_param_named(otg_ver, dwc_otg_module_params.otg_ver, int, 0444);
MODULE_PARM_DESC(otg_ver, "OTG revision supported 0=OTG 1.3 1=OTG 2.0");

/** @page "Module Parameters"
 *
 * The following parameters may be specified when starting the module.
 * These parameters define how the DWC_otg controller should be
 * configured. Parameter values are passed to the CIL initialization
 * function dwc_otg_cil_init
 *
 * Example: <code>modprobe dwc_otg speed=1 otg_cap=1</code>
 *

 <table>
 <tr><td>Parameter Name</td><td>Meaning</td></tr>

 <tr>
 <td>otg_cap</td>
 <td>Specifies the OTG capabilities. The driver will automatically detect the
 value for this parameter if none is specified.
 - 0: HNP and SRP capable (default, if available)
 - 1: SRP Only capable
 - 2: No HNP/SRP capable
 </td></tr>

 <tr>
 <td>dma_enable</td>
 <td>Specifies whether to use slave or DMA mode for accessing the data FIFOs.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Slave
 - 1: DMA (default, if available)
 </td></tr>

 <tr>
 <td>dma_burst_size</td>
 <td>The DMA Burst size (applicable only for External DMA Mode).
 - Values: 1, 4, 8 16, 32, 64, 128, 256 (default 32)
 </td></tr>

 <tr>
 <td>speed</td>
 <td>Specifies the maximum speed of operation in host and device mode. The
 actual speed depends on the speed of the attached device and the value of
 phy_type.
 - 0: High Speed (default)
 - 1: Full Speed
 </td></tr>

 <tr>
 <td>host_support_fs_ls_low_power</td>
 <td>Specifies whether low power mode is supported when attached to a Full
 Speed or Low Speed device in host mode.
 - 0: Don't support low power mode (default)
 - 1: Support low power mode
 </td></tr>

 <tr>
 <td>host_ls_low_power_phy_clk</td>
 <td>Specifies the PHY clock rate in low power mode when connected to a Low
 Speed device in host mode. This parameter is applicable only if
 HOST_SUPPORT_FS_LS_LOW_POWER is enabled.
 - 0: 48 MHz (default)
 - 1: 6 MHz
 </td></tr>

 <tr>
 <td>enable_dynamic_fifo</td>
 <td> Specifies whether FIFOs may be resized by the driver software.
 - 0: Use cC FIFO size parameters
 - 1: Allow dynamic FIFO sizing (default)
 </td></tr>

 <tr>
 <td>data_fifo_size</td>
 <td>Total number of 4-byte words in the data FIFO memory. This memory
 includes the Rx FIFO, non-periodic Tx FIFO, and periodic Tx FIFOs.
 - Values: 32 to 32768 (default 8192)

 Note: The total FIFO memory depth in the FPGA configuration is 8192.
 </td></tr>

 <tr>
 <td>dev_rx_fifo_size</td>
 <td>Number of 4-byte words in the Rx FIFO in device mode when dynamic
 FIFO sizing is enabled.
 - Values: 16 to 32768 (default 1064)
 </td></tr>

 <tr>
 <td>dev_nperio_tx_fifo_size</td>
 <td>Number of 4-byte words in the non-periodic Tx FIFO in device mode when
 dynamic FIFO sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>dev_perio_tx_fifo_size_n (n = 1 to 15)</td>
 <td>Number of 4-byte words in each of the periodic Tx FIFOs in device mode
 when dynamic FIFO sizing is enabled.
 - Values: 4 to 768 (default 256)
 </td></tr>

 <tr>
 <td>host_rx_fifo_size</td>
 <td>Number of 4-byte words in the Rx FIFO in host mode when dynamic FIFO
 sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>host_nperio_tx_fifo_size</td>
 <td>Number of 4-byte words in the non-periodic Tx FIFO in host mode when
 dynamic FIFO sizing is enabled in the core.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>host_perio_tx_fifo_size</td>
 <td>Number of 4-byte words in the host periodic Tx FIFO when dynamic FIFO
 sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>max_transfer_size</td>
 <td>The maximum transfer size supported in bytes.
 - Values: 2047 to 65,535 (default 65,535)
 </td></tr>

 <tr>
 <td>max_packet_count</td>
 <td>The maximum number of packets in a transfer.
 - Values: 15 to 511 (default 511)
 </td></tr>

 <tr>
 <td>host_channels</td>
 <td>The number of host channel registers to use.
 - Values: 1 to 16 (default 12)

 Note: The FPGA configuration supports a maximum of 12 host channels.
 </td></tr>

 <tr>
 <td>dev_endpoints</td>
 <td>The number of endpoints in addition to EP0 available for device mode
 operations.
 - Values: 1 to 15 (default 6 IN and OUT)

 Note: The FPGA configuration supports a maximum of 6 IN and OUT endpoints in
 addition to EP0.
 </td></tr>

 <tr>
 <td>phy_type</td>
 <td>Specifies the type of PHY interface to use. By default, the driver will
 automatically detect the phy_type.
 - 0: Full Speed
 - 1: UTMI+ (default, if available)
 - 2: ULPI
 </td></tr>

 <tr>
 <td>phy_utmi_width</td>
 <td>Specifies the UTMI+ Data Width. This parameter is applicable for a
 phy_type of UTMI+. Also, this parameter is applicable only if the
 OTG_HSPHY_WIDTH cC parameter was set to "8 and 16 bits", meaning that the
 core has been configured to work at either data path width.
 - Values: 8 or 16 bits (default 16)
 </td></tr>

 <tr>
 <td>phy_ulpi_ddr</td>
 <td>Specifies whether the ULPI operates at double or single data rate. This
 parameter is only applicable if phy_type is ULPI.
 - 0: single data rate ULPI interface with 8 bit wide data bus (default)
 - 1: double data rate ULPI interface with 4 bit wide data bus
 </td></tr>

 <tr>
 <td>i2c_enable</td>
 <td>Specifies whether to use the I2C interface for full speed PHY. This
 parameter is only applicable if PHY_TYPE is FS.
 - 0: Disabled (default)
 - 1: Enabled
 </td></tr>

 <tr>
 <td>ulpi_fs_ls</td>
 <td>Specifies whether to use ULPI FS/LS mode only.
 - 0: Disabled (default)
 - 1: Enabled
 </td></tr>

 <tr>
 <td>ts_dline</td>
 <td>Specifies whether term select D-Line pulsing for all PHYs is enabled.
 - 0: Disabled (default)
 - 1: Enabled
 </td></tr>
 
 <tr>
 <td>en_multiple_tx_fifo</td>
 <td>Specifies whether dedicatedto tx fifos are enabled for non periodic IN EPs.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Disabled
 - 1: Enabled (default, if available)
 </td></tr>

 <tr>
 <td>dev_tx_fifo_size_n (n = 1 to 15)</td>
 <td>Number of 4-byte words in each of the Tx FIFOs in device mode
 when dynamic FIFO sizing is enabled.
 - Values: 4 to 768 (default 256)
 </td></tr>

 <tr>
 <td>tx_thr_length</td>
 <td>Transmit Threshold length in 32 bit double words
 - Values: 8 to 128 (default 64)
 </td></tr>

 <tr>
 <td>rx_thr_length</td>
 <td>Receive Threshold length in 32 bit double words
 - Values: 8 to 128 (default 64)
 </td></tr>

<tr>
 <td>thr_ctl</td>
 <td>Specifies whether to enable Thresholding for Device mode. Bits 0, 1, 2 of 
 this parmater specifies if thresholding is enabled for non-Iso Tx, Iso Tx and
 Rx transfers accordingly.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - Values: 0 to 7 (default 0)
 Bit values indicate:
 - 0: Thresholding disabled
 - 1: Thresholding enabled
 </td></tr>

<tr>
 <td>dma_desc_enable</td>
 <td>Specifies whether to enable Descriptor DMA mode.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Descriptor DMA disabled
 - 1: Descriptor DMA (default, if available)
 </td></tr>

<tr>
 <td>mpi_enable</td>
 <td>Specifies whether to enable MPI enhancement mode.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: MPI disabled (default)
 - 1: MPI enable
 </td></tr>

<tr>
 <td>pti_enable</td>
 <td>Specifies whether to enable PTI enhancement support.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: PTI disabled (default)
 - 1: PTI enable
 </td></tr>

<tr>
 <td>lpm_enable</td>
 <td>Specifies whether to enable LPM support.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: LPM disabled
 - 1: LPM enable (default, if available)
 </td></tr>

 <tr>
 <td>besl_enable</td>
 <td>Specifies whether to enable LPM Errata support.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: LPM Errata disabled (default)
 - 1: LPM Errata enable 
 </td></tr>
 
  <tr>
 <td>baseline_besl</td>
 <td>Specifies the baseline besl value.
 - Values: 0 to 15 (default 0)
 </td></tr>
 
  <tr>
 <td>deep_besl</td>
 <td>Specifies the deep besl value.
 - Values: 0 to 15 (default 15)
 </td></tr>

<tr>
 <td>ic_usb_cap</td>
 <td>Specifies whether to enable IC_USB capability.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: IC_USB disabled (default, if available)
 - 1: IC_USB enable 
 </td></tr>

<tr>
 <td>ahb_thr_ratio</td>
 <td>Specifies AHB Threshold ratio.
 - Values: 0 to 3 (default 0)
 </td></tr>

<tr>
 <td>power_down</td>
 <td>Specifies Power Down(Hibernation) Mode.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Power Down disabled (default)
 - 2: Power Down enabled
 </td></tr>
 
 <tr>
 <td>reload_ctl</td>
 <td>Specifies whether dynamic reloading of the HFIR register is allowed during
 run time. The driver will automatically detect the value for this parameter if
 none is specified. In case the HFIR value is reloaded when HFIR.RldCtrl == 1'b0
 the core might misbehave.
 - 0: Reload Control disabled (default)
 - 1: Reload Control enabled
 </td></tr>

 <tr>
 <td>dev_out_nak</td>
 <td>Specifies whether  Device OUT NAK enhancement enabled or no.
 The driver will automatically detect the value for this parameter if
 none is specified. This parameter is valid only when OTG_EN_DESC_DMA == 1'b1.
 - 0: The core does not set NAK after Bulk OUT transfer complete (default)
 - 1: The core sets NAK after Bulk OUT transfer complete
 </td></tr>

 <tr>
 <td>cont_on_bna</td>
 <td>Specifies whether Enable Continue on BNA enabled or no. 
 After receiving BNA interrupt the core disables the endpoint,when the
 endpoint is re-enabled by the application the  
 - 0: Core starts processing from the DOEPDMA descriptor (default)
 - 1: Core starts processing from the descriptor which received the BNA.
 This parameter is valid only when OTG_EN_DESC_DMA == 1'b1.
 </td></tr>

 <tr>
 <td>ahb_single</td>
 <td>This bit when programmed supports SINGLE transfers for remainder data
 in a transfer for DMA mode of operation. 
 - 0: The remainder data will be sent using INCR burst size (default)
 - 1: The remainder data will be sent using SINGLE burst size.
 </td></tr>

<tr>
 <td>adp_enable</td>
 <td>Specifies whether ADP feature is enabled.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: ADP feature disabled (default)
 - 1: ADP feature enabled
 </td></tr>

  <tr>
 <td>otg_ver</td>
 <td>Specifies whether OTG is performing as USB OTG Revision 2.0 or Revision 1.3
 USB OTG device.
 - 0: OTG 2.0 support disabled (default)
 - 1: OTG 2.0 support enabled 
 </td></tr>

*/
