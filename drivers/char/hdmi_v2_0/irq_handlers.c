/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include <linux/gpio.h>
#include "include/hdmi_includes.h"
#include "include/hdmi_access.h"
#include "include/hdmi_log.h"
#include <asm/bitops.h> // bit macros
#include "include/irq_handlers.h"
#include "hdmi_api_lib/include/core/irq.h"
#include "hdmi_api_lib/include/core/interrupt/interrupt_reg.h"
#include "hdmi_api_lib/include/phy/phy.h"
#include "hdmi_api_lib/include/hdcp/hdcp.h"

static void dwc_hdmi_tx_handler_thread(struct work_struct *work)
{
        uint8_t phy_decode;
        uint32_t decode;
        struct hdmi_tx_dev *dev = (work!=NULL)?container_of(work, struct hdmi_tx_dev, tx_handler):NULL;

        if(dev != NULL) {
                if(dev->verbose >= VERBOSE_IRQ)
                        pr_info("dwc_hdmi_tx_handler_thread\r\n");

                // Read HDMI TX IRQ
                decode = hdmi_read_interrupt_decode(dev);

                // precess irq
                if((decode & IH_DECODE_IH_FC_STAT0_MASK) ? 1 : 0){
                        hdmi_irq_clear_source(dev, AUDIO_PACKETS);
                }

                if((decode & IH_DECODE_IH_FC_STAT1_MASK) ? 1 : 0){
                        hdmi_irq_clear_source(dev, OTHER_PACKETS);
                }

                if((decode & IH_DECODE_IH_FC_STAT2_VP_MASK) ? 1 : 0){
                        hdmi_irq_mute_source(dev, PACKETS_OVERFLOW);
                        hdmi_irq_mute_source(dev, VIDEO_PACKETIZER);
                }
                if((decode & IH_DECODE_IH_AS_STAT0_MASK) ? 1 : 0){
                        hdmi_irq_clear_source(dev, AUDIO_SAMPLER);
                }
                if((decode & IH_DECODE_IH_PHY_MASK) ? 1 : 0){
                        hdmi_irq_read_stat(dev, PHY, &phy_decode);

                        if((phy_decode & IH_PHY_STAT0_TX_PHY_LOCK_MASK) ? 1 : 0){
                                hdmi_irq_clear_bit(dev, PHY, IH_PHY_STAT0_TX_PHY_LOCK_MASK);
                        }

                        if((phy_decode & IH_PHY_STAT0_HPD_MASK) ? 1 : 0) {
                                dev->hotplug_real_status = hdmi_phy_hot_plug_detected(dev);
                                hdmi_irq_clear_bit(dev, PHY, IH_PHY_STAT0_HPD_MASK);
                                if(dev->verbose >= VERBOSE_IRQ)
                                        pr_info(" >>>HPD :%d\r\n",dev->hotplug_real_status );
                                if(!test_bit(HDMI_TX_HOTPLUG_STATUS_LOCK, &dev->status)) {
                                        dev->hotplug_status = dev->hotplug_real_status;
                                }
                                wake_up_interruptible(&dev->poll_wq);
                        }
                        hdmi_irq_clear_source(dev, PHY);
                }
                #if defined(CONFIG_HDMI_SUPPORT_SCDC_READREQ)
                if((decode & IH_DECODE_IH_I2CM_STAT0_MASK) ? 1 : 0){
                        uint8_t state = 0;

                        hdmi_irq_read_stat(dev, I2C_DDC, &state);

                        // I2Cmastererror - I2Cmasterdone
                        if(state & 0x3){
                                hdmi_irq_clear_bit(dev, I2C_DDC, IH_I2CM_STAT0_I2CMASTERDONE_MASK);
                                //The I2C communication interrupts must be masked - they will be handled by polling in the eDDC block
                                pr_err("%s:I2C DDC interrupt received 0x%x - mask interrupt", __func__, state);
                        }
                        // SCDC_READREQ
                        else if(state & 0x4){
                                hdmi_irq_clear_bit(dev, I2C_DDC, IH_I2CM_STAT0_SCDC_READREQ_MASK);
                        }
                }
                #endif
                if((decode & IH_DECODE_IH_CEC_STAT0_MASK) ? 1 : 0){
                        hdmi_irq_clear_source(dev, CEC);
                }
                hdmi_irq_unmute(dev);
        }
}

 static void dwc_hdmi_tx_hdcp_handler_thread(struct work_struct *work)
 {
        uint32_t hdcp_irq = 0, hdcp22_irq=0;
        int temp = 0;
        struct hdmi_tx_dev *dev;

        dev = container_of(work, struct hdmi_tx_dev, tx_hdcp_handler);

        if(dev->verbose >= VERBOSE_IRQ)
                pr_info("dwc_hdmi_tx_hdcp_handler_thread\r\n");
        hdcp_irq = hdcp_interrupt_status(dev);
        hdcp22_irq = _HDCP22RegReadStat(dev);

        if(hdcp_irq != 0){
                hdcp_event_handler(dev, &temp);
        }
        if(hdcp22_irq) {
                hdcp22_event_handler(dev, &temp);
        }
}

static void dwc_hdmi_tx_hotplug_thread(struct work_struct *work)
{
        int i;
        int match = 0;
        struct hdmi_tx_dev *dev;
        int prev_hpd, current_hpd;

        dev = container_of(work, struct hdmi_tx_dev, tx_hotplug_handler);

        if(dev != NULL) {
                prev_hpd = gpio_get_value(dev->hotplug_gpio);

                /* Check HPD */
                for(i=0;i<4;i++) {
                        current_hpd = gpio_get_value(dev->hotplug_gpio);
                        if(current_hpd == prev_hpd) {
                                match++;
                        }
                        udelay(10);
                }

                /* If match is less than 4, it is assumed to be noise. */
                if(match >= 4) {
			pr_info("\e[33mhotplug_real_status=%d \e[0m\r\n", current_hpd);
                        dev->hotplug_real_status = current_hpd;
                        if(!test_bit(HDMI_TX_HOTPLUG_STATUS_LOCK, &dev->status)) {
                                dev->hotplug_status = dev->hotplug_real_status;
                        }
			wake_up_interruptible(&dev->poll_wq);
                }
                dwc_hdmi_tx_set_hotplug_interrupt(dev, 1);
        }
}

static irqreturn_t
dwc_hdmi_tx_hpd_irq_handler(int irq, void *dev_id)
{
        struct hdmi_tx_dev *dev =  (struct hdmi_tx_dev *)dev_id;

        if(dev == NULL) {
                pr_err("%s: irq_dev is NULL\r\n", __func__);
                goto end_handler;
        }
        /* disable hpd irq */
        disable_irq_nosync(dev->hotplug_irq);
        dev->hotplug_irq_enabled = 0;

        schedule_work(&dev->tx_hotplug_handler);

        return IRQ_HANDLED;

end_handler:
        return IRQ_NONE;
}

static irqreturn_t
dwc_hdmi_tx_handler(int irq, void *dev_id)
{
        struct irq_dev_id *irq_dev = (struct irq_dev_id *)dev_id;
	struct hdmi_tx_dev *dev;
        uint32_t decode;

        if(irq_dev == NULL) {
                pr_err("%s: irq_dev is NULL\r\n", FUNC_NAME);
                goto end_handler;
        }

        dev = (struct hdmi_tx_dev *)irq_dev->dev;
        if(dev == NULL) {
                pr_err("%s: dev is NULL\r\n", FUNC_NAME);
                goto end_handler;
        }

	if(dev->verbose >= VERBOSE_BASIC)
		pr_info("%s\n", FUNC_NAME);

        decode = hdmi_read_interrupt_decode(dev);
        if(!decode) {
                goto end_handler;
        }
        hdmi_irq_mute(dev);
        schedule_work(&dev->tx_handler);
        return IRQ_HANDLED;

end_handler:
        return IRQ_NONE;

}

static irqreturn_t
hdcp_handler(int irq, void *dev_id){
        struct irq_dev_id *irq_dev = (struct irq_dev_id *)dev_id;
        struct hdmi_tx_dev *dev;
        uint32_t hdcp_irq, hdcp22_irq;

        if(irq_dev == NULL) {
                pr_err("%s: irq_dev is NULL\r\n", FUNC_NAME);
                goto end_handler;
        }

        dev = (struct hdmi_tx_dev *)irq_dev->dev;
        if(dev == NULL){
                pr_err("%s: dev is NULL\r\n", FUNC_NAME);
                goto end_handler;
        }
        if(dev->verbose >= VERBOSE_BASIC)
                pr_info("%s\n", FUNC_NAME);

        hdcp_irq = hdcp_interrupt_status(dev);
        hdcp22_irq = _HDCP22RegReadStat(dev);
        if(hdcp_irq){
                _InterruptMask(dev, hdcp_irq | _InterruptMaskStatus(dev));
        }
        if(hdcp22_irq){
                _HDCP22RegMask(dev, hdcp22_irq | _HDCP22RegMaskRead(dev));
                _HDCP22RegMute(dev, hdcp22_irq | _HDCP22RegMuteRead(dev));
        }
        if(!hdcp_irq && !hdcp22_irq) {
                goto end_handler;
        }
        schedule_work(&dev->tx_hdcp_handler);
        return IRQ_HANDLED;

end_handler:
        return IRQ_NONE;
}

#if defined(CONFIG_HDMI_USE_CEC_IRQ)
static irqreturn_t
dwc_hdmi_tx_cec_handler(int irq, void *dev_id){
	struct hdmi_tx_dev *dev = NULL;

	if(dev_id == NULL)
		return IRQ_NONE;

	dev = dev_id;

	if(dev->verbose >= VERBOSE_BASIC)
		pr_info("%s\n", FUNC_NAME);

	return IRQ_HANDLED;
}
#endif

int
dwc_init_interrupts(struct hdmi_tx_dev *dev)
{
        int flag;
        int ret = 0;
        INIT_WORK(&dev->tx_handler, dwc_hdmi_tx_handler_thread);
        INIT_WORK(&dev->tx_hdcp_handler, dwc_hdmi_tx_hdcp_handler_thread);
        INIT_WORK(&dev->tx_hotplug_handler, dwc_hdmi_tx_hotplug_thread);

        /* Check GPIO HPD */
        dev->hotplug_irq = -1;
        if (gpio_is_valid(dev->hotplug_gpio)) {
                if(devm_gpio_request(dev->parent_dev, dev->hotplug_gpio, "hdmi_hotplug") < 0 ) {
                        pr_err("%s failed get gpio request\r\n", __func__);
                } else {
                        gpio_direction_input(dev->hotplug_gpio);
                        dev->hotplug_irq = gpio_to_irq(dev->hotplug_gpio);

                        if(dev->hotplug_irq < 0) {
                                pr_err("%s can not convert gpio to irq\r\n", __func__);
                                ret = -1;
                        } else {
                                pr_info("%s using gpio hotplug interrupt (%d)\r\n", __func__, dev->hotplug_irq);
                                dev->hotplug_status = dev->hotplug_real_status = gpio_get_value(dev->hotplug_gpio)?1:0;
                                /* Disable IRQ auto enable */
                                irq_set_status_flags(dev->hotplug_irq, IRQ_NOAUTOEN);
                                flag = (dev->hotplug_real_status?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH)|IRQF_ONESHOT;
                                ret = devm_request_irq(dev->parent_dev, dev->hotplug_irq,
                                        dwc_hdmi_tx_hpd_irq_handler, flag,
                                        "hpd_irq_handler", dev);

                                dwc_hdmi_tx_set_hotplug_interrupt(dev, 1);
                        }
                        if(ret < 0) {
                                pr_err("%s failed request interrupt for hotplug\r\n", __func__);
                        }
                }
        }

        dev->irq_dev[HDMI_IRQ_TX_CORE].dev = dev;
        ret = devm_request_irq(dev->parent_dev, dev->irq[HDMI_IRQ_TX_CORE],
                               dwc_hdmi_tx_handler, IRQF_SHARED,
                               "dwc_hdmi_tx_handler", &dev->irq_dev[HDMI_IRQ_TX_CORE]);
        if (ret){
                pr_err("%s:Could not register dwc_hdmi_tx interrupt\n",
                        FUNC_NAME);
        }

        dev->irq_dev[HDMI_IRQ_TX_HDCP].dev = dev;
        ret = devm_request_irq(dev->parent_dev, dev->irq[HDMI_IRQ_TX_CORE],
                               hdcp_handler, IRQF_SHARED,
                               "hdcp_handler", &dev->irq_dev[HDMI_IRQ_TX_HDCP]);
        if (ret){
                pr_err("%s:Could not register hdcp interrupt\n",
                        FUNC_NAME);
        }

        #if defined(CONFIG_HDMI_USE_CEC_IRQ)
        dev->irq_dev[HDMI_IRQ_TX_CEC].dev = dev;
        ret = devm_request_irq(dev->parent_dev, dev->irq[HDMI_IRQ_TX_CEC],
                               dwc_hdmi_tx_cec_handler, IRQF_SHARED,
                               "dwc_hdmi_tx_cec_handler", &dev->irq_dev[HDMI_IRQ_TX_CEC]);
        if (ret){
                pr_err("%s:Could not register dwc_hdmi_tx_cec interrupt\n",
                        FUNC_NAME);
        }
        #endif

        // Disable irq handler
        disable_irq(dev->irq[HDMI_IRQ_TX_CORE]);
        #if defined(CONFIG_HDMI_USE_CEC_IRQ)
        disable_irq(dev->irq[HDMI_IRQ_TX_CEC]);
        #endif
        clear_bit(HDMI_TX_INTERRUPT_HANDLER_ON, &dev->status);

        return ret;
}


int
dwc_deinit_interrupts(struct hdmi_tx_dev *dev){
        int irq_loop;
        cancel_work_sync(&dev->tx_handler);
        cancel_work_sync(&dev->tx_hdcp_handler);
        cancel_work_sync(&dev->tx_hotplug_handler);

        // Unregister interrupts
        for(irq_loop = 0; irq_loop < HDMI_IRQ_TX_MAX; irq_loop++) {
                devm_free_irq(dev->parent_dev, dev->irq[irq_loop], &dev->irq_dev[irq_loop]);
        }
        devm_free_irq(dev->parent_dev, dev->hotplug_irq, &dev);
        return 0;
}

void dwc_hdmi_enable_interrupt(struct hdmi_tx_dev *dev){
        if(!test_bit(HDMI_TX_INTERRUPT_HANDLER_ON, &dev->status)) {
                // Mask and clear tx Interrupt
                hdmi_irq_mask_all(dev);
                hdmi_irq_clear_all(dev);

                // Mask and clear hdcp1.4 Interrupt
                _InterruptMask(dev, 0xFF);
                hdcp_interrupt_clear(dev, 0xFF);

                // Mask and clear hdcp2.2 Interrupt
                _HDCP22RegMask(dev, 0x3F);
                _HDCP22RegMute(dev, 0x3F);
                _HDCP22RegStat(dev, 0x3F);

                // Enable Irq Handler
                enable_irq(dev->irq[HDMI_IRQ_TX_CORE]);
                #if defined(CONFIG_HDMI_USE_CEC_IRQ)
                enable_irq(dev->irq[HDMI_IRQ_TX_CEC]);
                #endif

                hdmi_irq_unmute(dev);
                set_bit(HDMI_TX_INTERRUPT_HANDLER_ON, &dev->status);
        }
}

void dwc_hdmi_disable_interrupt(struct hdmi_tx_dev *dev){
        if(test_bit(HDMI_TX_INTERRUPT_HANDLER_ON, &dev->status)) {
                // Disable irq handler
                disable_irq(dev->irq[HDMI_IRQ_TX_CORE]);
                #if defined(CONFIG_HDMI_USE_CEC_IRQ)
                disable_irq(dev->irq[HDMI_IRQ_TX_CEC]);
                #endif

                cancel_work_sync(&dev->tx_handler);
                cancel_work_sync(&dev->tx_hdcp_handler);

                // Mask and clear tx Interrupt
                hdmi_irq_mask_all(dev);
                hdmi_irq_clear_all(dev);

                // Mask and clear hdcp1.4 Interrupt
                _InterruptMask(dev, 0xFF);
                hdcp_interrupt_clear(dev, 0xFF);

                // Mask and clear hdcp2.2 Interrupt
                _HDCP22RegMask(dev, 0x3F);
                _HDCP22RegMute(dev, 0x3F);
                _HDCP22RegStat(dev, 0x3F);
                clear_bit(HDMI_TX_INTERRUPT_HANDLER_ON, &dev->status);
        }
}

void dwc_hdmi_tx_set_hotplug_interrupt(struct hdmi_tx_dev *dev, int enable)
{
        int flag;

	if(dev != NULL) {
	        if(enable) {
	                flag = (dev->hotplug_real_status?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH)|IRQF_ONESHOT;
	                irq_set_irq_type(dev->hotplug_irq, flag);
			if(!dev->hotplug_irq_enabled) {
                                dev->hotplug_irq_enabled = 1;
	                	enable_irq(dev->hotplug_irq);
			} else {
			        pr_info("%s already enable irq\r\n", __func__);
			}
	        } else {
			if(dev->hotplug_irq_enabled) {
                                dev->hotplug_irq_enabled = 0;
	                	disable_irq(dev->hotplug_irq);
			} else {
			        pr_info("%s disable irq\r\n", __func__);
			}
	                cancel_work_sync(&dev->tx_hotplug_handler);
	        }
	}
}

