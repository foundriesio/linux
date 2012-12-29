
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include <linux/clk.h>
#include <linux/kernel.h>

#include <mach/common.h>
#include <mach/gpio.h>
#include <mach/iomux-mx6dl.h>
#include <mach/iomux-v3.h>
#include <mach/mx6.h>

#include "devices-imx6q.h"

#define WAND_SD3_CD		IMX_GPIO_NR(3, 9)
#define WAND_SD3_WP		IMX_GPIO_NR(1, 10)

/* Syntactic sugar for pad configuration */
#define WAND_SETUP_PADS(p) \
        mxc_iomux_v3_setup_multiple_pads((p), ARRAY_SIZE(p))


/****************************************************************************
 *                                                                          
 * DMA controller init
 *                                                                          
 ****************************************************************************/

static __init void wand_init_dma(void) {
        imx6q_add_dma();        
}


/****************************************************************************
 *                                                                          
 * SD init
 *
 * SD3 is boot SD on the module
 *                                                                          
 ****************************************************************************/

#define WAND_SD3_PADS 6

static const iomux_v3_cfg_t wand_sd3_pads[3][WAND_SD3_PADS] = {
	{
	MX6DL_PAD_SD3_CLK__USDHC3_CLK_50MHZ,
	MX6DL_PAD_SD3_CMD__USDHC3_CMD_50MHZ,
	MX6DL_PAD_SD3_DAT0__USDHC3_DAT0_50MHZ,
	MX6DL_PAD_SD3_DAT1__USDHC3_DAT1_50MHZ,
	MX6DL_PAD_SD3_DAT2__USDHC3_DAT2_50MHZ,
	MX6DL_PAD_SD3_DAT3__USDHC3_DAT3_50MHZ,
        }, {
	MX6DL_PAD_SD3_CLK__USDHC3_CLK_100MHZ,
	MX6DL_PAD_SD3_CMD__USDHC3_CMD_100MHZ,
	MX6DL_PAD_SD3_DAT0__USDHC3_DAT0_100MHZ,
	MX6DL_PAD_SD3_DAT1__USDHC3_DAT1_100MHZ,
	MX6DL_PAD_SD3_DAT2__USDHC3_DAT2_100MHZ,
	MX6DL_PAD_SD3_DAT3__USDHC3_DAT3_100MHZ,
        }, {
	MX6DL_PAD_SD3_CLK__USDHC3_CLK_200MHZ,
	MX6DL_PAD_SD3_CMD__USDHC3_CMD_200MHZ,
	MX6DL_PAD_SD3_DAT0__USDHC3_DAT0_200MHZ,
	MX6DL_PAD_SD3_DAT1__USDHC3_DAT1_200MHZ,
	MX6DL_PAD_SD3_DAT2__USDHC3_DAT2_200MHZ,
	MX6DL_PAD_SD3_DAT3__USDHC3_DAT3_200MHZ,
        }
};

/* ------------------------------------------------------------------------ */

static int wand_sd3_speed_change(unsigned int sd, int clock) {
	static int pad_speed = 200;

	if (clock > 100000000) {                
		if (pad_speed == 200) return 0;
		pad_speed = 200;
		return WAND_SETUP_PADS(wand_sd3_pads[2]);
	} else if (clock > 52000000) {
		if (pad_speed == 100) return 0;
		pad_speed = 100;
		return WAND_SETUP_PADS(wand_sd3_pads[1]); 
	} else {
		if (pad_speed == 25) return 0;
		pad_speed = 25;
		return WAND_SETUP_PADS(wand_sd3_pads[0]);
	}
}

/* ------------------------------------------------------------------------ */

static const struct esdhc_platform_data wand_sd3_data = {
	.cd_gpio		= WAND_SD3_CD,
	.wp_gpio		= WAND_SD3_WP,
	.keep_power_at_suspend	= 1,
	.support_18v		= 1,
	.support_8bit		= 0,
	.delay_line		= 0,
	.platform_pad_change = wand_sd3_speed_change,
};

/* ------------------------------------------------------------------------ */

static __init void wand_init_sd(void) {
        /* Card Detect for SD3 */
        mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_DA9__GPIO_3_9);
        WAND_SETUP_PADS(wand_sd3_pads[2]);

        imx6q_add_sdhci_usdhc_imx(2, &wand_sd3_data);
}


/****************************************************************************
 *                                                                          
 * Initialize debug console (UART1)
 *                                                                          
 ****************************************************************************/

static const __initdata iomux_v3_cfg_t wand_uart_pads[] = {
	/* UART1 (debug console) */
        MX6DL_PAD_CSI0_DAT10__UART1_TXD,
        MX6DL_PAD_CSI0_DAT11__UART1_RXD,
        MX6DL_PAD_EIM_D19__UART1_CTS,
        MX6DL_PAD_EIM_D20__UART1_RTS,
};

/* ------------------------------------------------------------------------ */
 
static __init void wand_init_uart(void) {
        WAND_SETUP_PADS(wand_uart_pads);

	imx6q_add_imx_uart(0, NULL);
}


/*****************************************************************************
 *                                                                           
 * Init clocks and early boot console                                      
 *                                                                            
 *****************************************************************************/

extern void __iomem *twd_base;

static void __init wand_init_timer(void) {
	struct clk *uart_clk;
#ifdef CONFIG_LOCAL_TIMERS
	twd_base = ioremap(LOCAL_TWD_ADDR, SZ_256);
#endif
	mx6_clocks_init(32768, 24000000, 0, 0);

	uart_clk = clk_get_sys("imx-uart.0", NULL);
	early_console_setup(UART1_BASE_ADDR, uart_clk);
}

/* ------------------------------------------------------------------------ */

static struct sys_timer wand_timer = {
	.init = wand_init_timer,
};


/*****************************************************************************
 *                                                                           
 * BOARD INIT                                                                
 *                                                                            
 *****************************************************************************/

static void __init wand_board_init(void) {
        wand_init_dma();
        wand_init_uart();
        wand_init_sd();
}

/* ------------------------------------------------------------------------ */

        
MACHINE_START(WANDBOARD, "Wandboard")
	.boot_params	= MX6_PHYS_OFFSET + 0x100,
	.map_io		= mx6_map_io,
	.init_irq	= mx6_init_irq,
	.init_machine	= wand_board_init,
	.timer		= &wand_timer,
MACHINE_END

