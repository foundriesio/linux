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
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>

#include <include/hdmi_ioctls.h>
#include <scdc/scdc.h>

#include <core/irq.h>
#include <bsp/i2cm.h>
#include <bsp/eddc_reg.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>


#include <linux/timer.h>

static int scdc_read(struct hdmi_tx_dev *dev, u8 address, u8 size, u8 * data)
{
	return hdmi_ddc_read(dev, SCDC_SLAVE_ADDRESS, 0,0 , address, size, data);
}

static int scdc_write(struct hdmi_tx_dev *dev, u8 address, u8 size, u8 * data)
{
	return hdmi_ddc_write(dev, SCDC_SLAVE_ADDRESS, address, size, data);
}

void scdc_enable_rr(struct hdmi_tx_dev *dev, u8 enable)
{

	if (enable == 1) {
		/* Enable Readrequest from the Tx controller */
		hdmi_dev_write_mask(dev, I2CM_SCDC_READ_UPDATE, I2CM_SCDC_READ_UPDATE_READ_REQUEST_EN_MASK, 1);
		scdc_set_rr_flag(dev, 0x01);
		hdmi_irq_scdc_read_request(dev, TRUE);
	}
	if (enable == 0) {
		/* Disable ReadRequest on Tx controller */
		hdmi_irq_scdc_read_request(dev, FALSE);
		scdc_set_rr_flag(dev, 0x00);
		hdmi_dev_write_mask(dev, I2CM_SCDC_READ_UPDATE, I2CM_SCDC_READ_UPDATE_READ_REQUEST_EN_MASK, 0);
	}
}

int scdc_scrambling_status(struct hdmi_tx_dev *dev)
{
	u8 read_value = 0;
	if(scdc_read(dev, SCDC_SCRAMBLER_STAT, 1, &read_value)){
		//LOGGER(SNPS_ERROR, "%s: SCDC addr 0x%x read failed ",__func__, SCDC_SINK_VER);
		return 0;
	}
	//printk("scdc_scrambling_status = 0x%x\r\n", read_value & 0x1);
	return (read_value & 0x01);
}

int scdc_scrambling_enable_flag(struct hdmi_tx_dev *dev, u8 enable)
{
        u8 read_value = 0;
        if(scdc_read(dev, SCDC_TMDS_CONFIG, 1 , &read_value)){
                return -1;
        }
	/* 7-bit of tmds_config is must set 0.  */
        read_value &= ~(0xFC | (1 << 0));
        read_value |= ((enable?1:0) << 0);
        if(scdc_write(dev, SCDC_TMDS_CONFIG, 1, &read_value)){
                return -1;
        }
        return 0;
}

int scdc_tmds_config_status(struct hdmi_tx_dev *dev)
{
        u8 read_value = 0;
        if(scdc_read(dev, SCDC_TMDS_CONFIG, 1, &read_value)){
                //LOGGER(SNPS_ERROR, "%s: SCDC addr 0x%x read failed ",__func__, SCDC_SINK_VER);
                return 0;
        }
        //printk("scdc_tmds_config_status = 0x%x\r\n", read_value &0x2);
        return (read_value & 0x2);
}

int scdc_tmds_bit_clock_ratio_enable_flag(struct hdmi_tx_dev *dev, u8 enable)
{
        u8 read_value = 0;
        if(scdc_read(dev, SCDC_TMDS_CONFIG, 1 , &read_value)){
                //LOGGER(SNPS_ERROR, "%s: SCDC addr 0x%x read failed ",__func__, SCDC_TMDS_CONFIG);
                return -1;
        }
	/* 7-bit of tmds_config is must set 0. */
        read_value &= ~(0xFC | ( 1 << 1));
        read_value |= ((enable?1:0) << 1);
        if(scdc_write(dev, SCDC_TMDS_CONFIG, 1, &read_value)){
                //LOGGER(SNPS_ERROR, "%s: SCDC addr 0x%x write failed ",__func__, SCDC_TMDS_CONFIG);
                return -1;
        }
        return 0;
}

int scdc_set_tmds_bit_clock_ratio_and_scrambling(struct hdmi_tx_dev *dev, int tmds_enable, int scramble_enable)
{
        int ret = -1;
        unsigned char scdc_val = 0;
        do {
		scdc_val = (((tmds_enable?1:0) << 1) | ((scramble_enable?1:0) << 0));
                if(scdc_write(dev, SCDC_TMDS_CONFIG, 1, &scdc_val)){
                        break;
                }
                ret = 0;
        }while(0);
        return ret;
}

void scdc_set_rr_flag(struct hdmi_tx_dev *dev, u8 enable)
{

	if(hdmi_ddc_write(dev, SCDC_SLAVE_ADDRESS, SCDC_CONFIG_0, 1, &enable)){
		LOGGER(SNPS_ERROR, "%s: SCDC addr 0x%x - 0x%x write failed ",__func__, SCDC_CONFIG_0, enable);
	}
}

int scdc_get_rr_flag(struct hdmi_tx_dev *dev, u8 * flag)
{
	if(hdmi_ddc_read(dev, SCDC_SLAVE_ADDRESS, 0, 0 , SCDC_CONFIG_0, 1, flag)){
		LOGGER(SNPS_ERROR, "%s: SCDC addr 0x%x read failed ",__func__, SCDC_CONFIG_0);
		return -1;
	}
	return 0;
}

void scdc_test_rr(struct hdmi_tx_dev *dev, u8 test_rr_delay)
{
	scdc_enable_rr(dev, 0x01);
	//test_rr_delay = set(test_rr_delay, 0x80, 1);
	test_rr_delay |= 0x80;
	scdc_write(dev, SCDC_TEST_CFG_0, 1, &test_rr_delay);
}

int scdc_test_rr_update_flag(struct hdmi_tx_dev *dev)
{
	u8 read_value = 0;
	if(scdc_read(dev, SCDC_UPDATE_0, 1, &read_value)){
		//LOGGER(SNPS_ERROR, "%s: SCDC addr 0x%x read failed ",__func__, SCDC_UPDATE_0);
		return 0;
	}
	return read_value;
}

static unsigned int scdc_time_diff_ms(struct timeval *pre_timeval, struct timeval *current_timeval)
{
        unsigned int  diff_time_ms = 0;

        long diff_sec, diff_usec;
        if (current_timeval->tv_usec >= pre_timeval->tv_usec) {
                diff_sec = current_timeval->tv_sec - pre_timeval->tv_sec;
                diff_usec = current_timeval->tv_usec - pre_timeval->tv_usec;
                diff_usec = (diff_usec > 1000)?(diff_usec/1000):0;
                diff_time_ms = (1000 * diff_sec) + diff_usec;
        } else {
                diff_sec = current_timeval->tv_sec - pre_timeval->tv_sec - 1;
                diff_usec = current_timeval->tv_usec - pre_timeval->tv_usec;
                diff_usec += 1000000;
                diff_usec = (diff_usec > 1000)?(diff_usec/1000):0;
                diff_time_ms = (1000 * diff_sec) + diff_usec;
        }
        return diff_time_ms;
}

int scdc_error_detection_core(struct hdmi_tx_dev *dev, struct hdmi_scdc_error_data *hdmi_scdc_error_data, int stage)
{
        int ret = 0;
        int scdc_support = 1;
        unsigned int diff_ms;
        unsigned char scdc_errors[8];
        struct timeval current_timeval;
        unsigned char reg_value, regl_value, regh_value;

        /* Save Time Val */
        static struct timeval prev_status_timeval = {0};
        static struct timeval prev_ced_timeval = {0};

        memset(hdmi_scdc_error_data, 0, sizeof(struct hdmi_scdc_error_data));

        if(scdc_read(dev, SCDC_SINK_VER, 1, &reg_value) < 0){
                msleep_interruptible(10);
                if(scdc_read(dev, SCDC_SINK_VER, 1, &reg_value) < 0){
                        scdc_support = 0;
                }
        }

        if(scdc_support) {
                /* Scrambler Status */
                if(scdc_read(dev, SCDC_SCRAMBLER_STAT, 1, &reg_value)) {
                        reg_value = 0;
                }
                if(reg_value & 1)
                        set_bit(SCDC_STATUS_SCRAMBLED, &hdmi_scdc_error_data->status);

                /* TMDS Config Status */
                if(scdc_read(dev, SCDC_TMDS_CONFIG, 1, &reg_value)){
                        reg_value = 0;
                }
                if(reg_value & 1)
                        set_bit(SCDC_STATUS_SET_SCRAMBLE, &hdmi_scdc_error_data->status);

                if(reg_value & 2)
                        set_bit(SCDC_STATUS_SET_TMDS, &hdmi_scdc_error_data->status);


                /* Update Flags 0 */
                if(scdc_read(dev, SCDC_UPDATE_0, 1, &reg_value)){
                      reg_value = 0;
                }
                if(reg_value & (1 << 1)) {
                      set_bit(SCDC_STATUS_CED, &hdmi_scdc_error_data->status);
                }
                if(reg_value & (1 << 0)) {
                      set_bit(SCDC_STATUS_CHANGE, &hdmi_scdc_error_data->status);
                }
                /* Clear Update Flags 0 */
                if(reg_value != 0) {
                        scdc_write(dev, SCDC_UPDATE_0, 1, &reg_value);
                }

                if(test_bit(SCDC_STATUS_CHANGE, &hdmi_scdc_error_data->status)) {
                        /* Read Status */
                        if(scdc_read(dev, SCDC_STATUS_FLAG_0, 1, &reg_value)){
                                reg_value = 0;
                        }
                        /* Clock Lock */
                        if((reg_value >> 0) & 1) {
                                set_bit(SCDC_STATUS_CLK_LOCK, &hdmi_scdc_error_data->status);
                        }
                        /* Channel 0 Lock */
                        if((reg_value >> 1) & 1) {
                                set_bit(SCDC_CHANNEL_LOCK, &hdmi_scdc_error_data->ch0);
                        }
                        /* Channel 2 Lock */
                        if((reg_value >> 2) & 1) {
                                set_bit(SCDC_CHANNEL_LOCK, &hdmi_scdc_error_data->ch1);
                        }
                        /* Channel 2 Lock */
                        if((reg_value >> 3) & 1) {
                                set_bit(SCDC_CHANNEL_LOCK, &hdmi_scdc_error_data->ch2);
                        }
                        if(test_bit(HDMI_TX_STATUS_SCDC_CHECK, &dev->status)) {
                                do_gettimeofday(&current_timeval);
                                diff_ms = scdc_time_diff_ms(&prev_status_timeval, &current_timeval);
                                memcpy(&prev_status_timeval, &current_timeval, sizeof(current_timeval));
                                pr_info("\x1b[33m"
                                   "C%d "
                                   "V%d%d "
                                   "V%d%d "
                                   "V%d%d "
                                   "%dms\x1b[0m\r\n",
                                        (unsigned int)test_bit(SCDC_STATUS_CLK_LOCK, &hdmi_scdc_error_data->status),
                                        (unsigned int)test_bit(SCDC_CHANNEL_VALID, &hdmi_scdc_error_data->ch0),
                                        (unsigned int)test_bit(SCDC_CHANNEL_LOCK, &hdmi_scdc_error_data->ch0),
                                        (unsigned int)test_bit(SCDC_CHANNEL_VALID, &hdmi_scdc_error_data->ch1),
                                        (unsigned int)test_bit(SCDC_CHANNEL_LOCK, &hdmi_scdc_error_data->ch1),
                                        (unsigned int)test_bit(SCDC_CHANNEL_VALID, &hdmi_scdc_error_data->ch2),
                                        (unsigned int)test_bit(SCDC_CHANNEL_LOCK, &hdmi_scdc_error_data->ch2),
                                        diff_ms);
                        }
                }

                /* TDMS Configs and Scrambler Status */
                reg_value = (test_bit(SCDC_STATUS_SET_SCRAMBLE, &hdmi_scdc_error_data->status) << 2) |
                            (test_bit(SCDC_STATUS_SET_TMDS, &hdmi_scdc_error_data->status) << 1) |
                            test_bit(SCDC_STATUS_SET_SCRAMBLE, &hdmi_scdc_error_data->status);

                if(reg_value != dev->prev_scdc_status) {
                        dev->prev_scdc_status = reg_value;
                        if(test_bit(HDMI_TX_STATUS_SCDC_CHECK, &dev->status)) {
                               pr_info("\x1b[33m"
                                  "TMDS_Config %02d, Scrambler Status %d"
                                  "\x1b[0m\r\n",
                                       (unsigned int)(test_bit(SCDC_STATUS_SET_TMDS, &hdmi_scdc_error_data->status) << 1) |
                                                      test_bit(SCDC_STATUS_SET_SCRAMBLE, &hdmi_scdc_error_data->status),
                                       (unsigned int)test_bit(SCDC_STATUS_SET_SCRAMBLE, &hdmi_scdc_error_data->status));
                        }
                }

                /* Check CED_Update Flag */
                if(test_bit(SCDC_STATUS_CED, &hdmi_scdc_error_data->status)) {
                        /* Read CED */
                        if(scdc_read(dev, SCDC_ERR_DET_0_L, 8, scdc_errors) == 0){

                                /* Channel 0 */
                                regl_value = scdc_errors[0]; regh_value = scdc_errors[1];
                                hdmi_scdc_error_data->ch0 = ((regl_value | ((regh_value & 0x7F) << 8)) << SCDC_CHANNEL_ERROR_SHIFT);
                                if(regh_value >> 7) {
                                        set_bit(SCDC_CHANNEL_VALID, &hdmi_scdc_error_data->ch0);
                                }

                                /* Channel 1 */
                                regl_value = scdc_errors[2]; regh_value = scdc_errors[3];
                                hdmi_scdc_error_data->ch1 = ((regl_value | ((regh_value & 0x7F) << 8)) << SCDC_CHANNEL_ERROR_SHIFT);
                                if(regh_value >> 7) {
                                        set_bit(SCDC_CHANNEL_VALID, &hdmi_scdc_error_data->ch1);
                                }

                                /* Channel 2 */
                                regl_value = scdc_errors[4]; regh_value = scdc_errors[5];
                                hdmi_scdc_error_data->ch2 = ((regl_value | ((regh_value & 0x7F) << 8)) << SCDC_CHANNEL_ERROR_SHIFT);
                                if(regh_value >> 7) {
                                        set_bit(SCDC_CHANNEL_VALID, &hdmi_scdc_error_data->ch2);
                                }

                                if(test_bit(HDMI_TX_STATUS_SCDC_CHECK, &dev->status)) {
                                        do_gettimeofday(&current_timeval);
                                        diff_ms = scdc_time_diff_ms(&prev_ced_timeval, &current_timeval);
                                        memcpy(&prev_ced_timeval, &current_timeval, sizeof(current_timeval));
                                        pr_info("\x1b[33m"
                                           "CH0_%05x "
                                           "CH1_%05x "
                                           "CH2_%05x "
                                           "%dms\x1b[0m\r\n",
                                                (unsigned int)((hdmi_scdc_error_data->ch0 >> SCDC_CHANNEL_ERROR_SHIFT) & SCDC_CHANNEL_ERROR_MASK),
                                                (unsigned int)((hdmi_scdc_error_data->ch1 >> SCDC_CHANNEL_ERROR_SHIFT) & SCDC_CHANNEL_ERROR_MASK),
                                                (unsigned int)((hdmi_scdc_error_data->ch2 >> SCDC_CHANNEL_ERROR_SHIFT) & SCDC_CHANNEL_ERROR_MASK),
                                                diff_ms);
                                }
                        }

                }
                /* Check CED_Update Flag -- */
        }
        return ret;
}

int scdc_error_detection(struct hdmi_tx_dev *dev, struct hdmi_scdc_error_data *hdmi_scdc_error_data)
{
        int ret = 0;

        if(hdmi_dev_read_mask(dev, FC_GCP, FC_GCP_SET_AVMUTE_MASK)) {
                scdc_error_detection_core(dev, hdmi_scdc_error_data, SCDC_STAGE_MUTE);
        }else {
                scdc_error_detection_core(dev, hdmi_scdc_error_data, SCDC_STAGE_UNMUTE);
        }
        return ret;
}


int scdc_read_sink_version(struct hdmi_tx_dev *dev, unsigned int *version)
{
	u8 read_value = 0;
	if(scdc_read(dev, SCDC_SINK_VER, 1, &read_value) < 0){
                msleep_interruptible(10);
                if(scdc_read(dev, SCDC_SINK_VER, 1, &read_value) < 0){
		        return -1;
                }
	}
        if(version)
                *version = read_value;
	return 0;
}

int scdc_read_source_version(struct hdmi_tx_dev *dev, unsigned int *version)
{
	u8 read_value = 0;
	if(scdc_read(dev, SCDC_SOURCE_VER, 1, &read_value) < 0){
                msleep_interruptible(10);
                if(scdc_read(dev, SCDC_SOURCE_VER, 1, &read_value) < 0){
		        return -1;
                }
	}
	if(version)
		*version = read_value;
	return 0;
}

int scdc_write_source_version(struct hdmi_tx_dev *dev, unsigned int version)
{
        u8 source_version = (u8) (version & 0xFF);
	if(scdc_write(dev, SCDC_SOURCE_VER, 1, &source_version)){
		return -1;
	}

	return 0;
}

