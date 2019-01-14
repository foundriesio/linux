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

#include <core/interrupt/interrupt_reg.h>
#include <core/irq.h>

#include <bsp/eddc_reg.h>
#include <bsp/i2cm.h>
#include <phy/phy_reg.h>

#define I2CM_OPERATION_READ		0x01
#define I2CM_OPERATION_READ_EXT		0x02
#define I2CM_OPERATION_READ_SEQ		0x04
#define I2CM_OPERATION_READ_SEQ_EXT     0x08
#define I2CM_OPERATION_WRITE		0x10
#define I2CM_OPERATION_BUSCLEAR 	0x20


#define I2C_DIV_FACTOR	 100000

#define I2C_DELAY(us)		usleep_range(us, us *2)
//#define DBG_I2C

#if defined(DBG_I2C)
#define dpr_info(args...) pr_info(args)
#else
#define dpr_info(args...)
#endif

/*********************  PRIVATE FUNCTIONS ***********************/

/**
 * calculate the fast sped high time counter - round up
 */
static u16 _scl_calc(u16 sfrClock, u16 sclMinTime)
{
	unsigned long tmp_scl_period = 0;
	if (((sfrClock * sclMinTime) % I2C_DIV_FACTOR) != 0) {
		tmp_scl_period = (unsigned long)((sfrClock * sclMinTime) + (I2C_DIV_FACTOR - ((sfrClock * sclMinTime) % I2C_DIV_FACTOR))) / I2C_DIV_FACTOR;
	}
	else {
		tmp_scl_period = (unsigned long)(sfrClock * sclMinTime) / I2C_DIV_FACTOR;
	}
	return (u16)(tmp_scl_period);
}

static void _fast_speed_high_clk_ctrl(struct hdmi_tx_dev *dev, u16 value)
{
        if(dev != NULL && !dev->ddc_disable) {
        	LOG_TRACE2(I2CM_FS_SCL_HCNT_1_ADDR, value);
        	hdmi_dev_write(dev, I2CM_FS_SCL_HCNT_1_ADDR, (u8) (value >> 8));
        	hdmi_dev_write(dev, I2CM_FS_SCL_HCNT_0_ADDR, (u8) (value >> 0));
        }
}

static void _fast_speed_low_clk_ctrl(struct hdmi_tx_dev *dev, u16 value)
{
        if(dev != NULL && !dev->ddc_disable) {
        	LOG_TRACE2((I2CM_FS_SCL_LCNT_1_ADDR), value);
        	hdmi_dev_write(dev, I2CM_FS_SCL_LCNT_1_ADDR, (u8) (value >> 8));
        	hdmi_dev_write(dev, I2CM_FS_SCL_LCNT_0_ADDR, (u8) (value >> 0));
        }
}

static void _standard_speed_high_clk_ctrl(struct hdmi_tx_dev *dev, u16 value)
{
        if(dev != NULL && !dev->ddc_disable) {
        	LOG_TRACE2((I2CM_SS_SCL_HCNT_1_ADDR), value);
        	hdmi_dev_write(dev, I2CM_SS_SCL_HCNT_1_ADDR, (u8) (value >> 8));
        	hdmi_dev_write(dev, I2CM_SS_SCL_HCNT_0_ADDR, (u8) (value >> 0));
        }
}

static void _standard_speed_low_clk_ctrl(struct hdmi_tx_dev *dev, u16 value)
{
        if(dev != NULL && !dev->ddc_disable) {
        	LOG_TRACE2((I2CM_SS_SCL_LCNT_1_ADDR), value);
        	hdmi_dev_write(dev, I2CM_SS_SCL_LCNT_1_ADDR, (u8) (value >> 8));
        	hdmi_dev_write(dev, I2CM_SS_SCL_LCNT_0_ADDR, (u8) (value >> 0));
        }
}

static int _write(struct hdmi_tx_dev *dev, u8 i2cAddr, u8 addr, u8 data)
{
        u32 status = 0;
        int result = -1;
	int timeout = I2CDDC_TIMEOUT;

	LOG_TRACE1(addr);
        do {
                if(dev->ddc_disable) {
                        break;
                }

        	hdmi_dev_write_mask(dev, I2CM_SLAVE, I2CM_SLAVE_SLAVEADDR_MASK, i2cAddr);
        	hdmi_dev_write(dev, I2CM_ADDRESS, addr);
        	hdmi_dev_write(dev, I2CM_DATAO, data);
        	hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);
        	hdmi_dev_write(dev, I2CM_OPERATION, I2CM_OPERATION_WRITE);
        	do {
        		I2C_DELAY(100);
        		status = hdmi_dev_read_mask(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK |
        							   IH_I2CM_STAT0_I2CMASTERDONE_MASK);
        	} while (status == 0 && (timeout--));

        	hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);

        	if(status & IH_I2CM_STAT0_I2CMASTERERROR_MASK){
        		LOGGER(SNPS_NOTICE, "%s: I2C DDC write failed",__func__);
        		break;
        	}

        	if(status & IH_I2CM_STAT0_I2CMASTERDONE_MASK){
        		result = 0;
        	}
        }while(0);

	return result;
}

static int _read(struct hdmi_tx_dev *dev, u8 i2cAddr, u8 segment, u8 pointer, u8 addr,   u8 * value)
{

        u32 status = 0;
        int result = -1;
	int timeout = I2CDDC_TIMEOUT;

	LOG_TRACE1(addr);
        do {
                if(dev->ddc_disable) {
                        break;
                }
        	hdmi_dev_write_mask(dev, I2CM_SLAVE, I2CM_SLAVE_SLAVEADDR_MASK, i2cAddr);
        	hdmi_dev_write(dev, I2CM_ADDRESS, addr);
        	hdmi_dev_write(dev, I2CM_SEGADDR, segment);
        	hdmi_dev_write(dev, I2CM_SEGPTR, pointer);
        	hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);
        	if(pointer)
        		hdmi_dev_write(dev, I2CM_OPERATION, I2CM_OPERATION_READ_EXT);
        	else
        		hdmi_dev_write(dev, I2CM_OPERATION, I2CM_OPERATION_READ);

        	do {
        		I2C_DELAY(100);
        		status = hdmi_dev_read_mask(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK |
        							   IH_I2CM_STAT0_I2CMASTERDONE_MASK);
        	} while (status == 0 && (timeout--));

        	hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);

        	if(status & IH_I2CM_STAT0_I2CMASTERERROR_MASK){
        		LOGGER(SNPS_NOTICE, "%s: I2C DDC Read failed for i2cAddr 0x%x seg 0x%x pointer 0x%x addr 0x%x",__func__,
        					i2cAddr, segment, pointer, addr);
        		break;
        	}

        	if(status & IH_I2CM_STAT0_I2CMASTERDONE_MASK){
        		*value = (u8) hdmi_dev_read(dev, I2CM_DATAI);
        		result = 0;
        	}
        }while(0);

	//LOGGER(SNPS_ERROR, "%s: ASSERT I2C DDC Read timeout - check system - exiting\r\n",__func__);
	return result;
}

static int _read8(struct hdmi_tx_dev *dev, u8 i2cAddr, u8 segment, u8 pointer, u8 addr, u8 * value)
{
        u32 status = 0;
        int result = -1;
	int timeout = I2CDDC_TIMEOUT;

	LOG_TRACE1(addr);
        do {
                if(dev->ddc_disable) {
                        break;
                }
                hdmi_dev_write_mask(dev, I2CM_SLAVE, I2CM_SLAVE_SLAVEADDR_MASK, i2cAddr);
                hdmi_dev_write(dev, I2CM_SEGADDR, segment);
                hdmi_dev_write(dev, I2CM_SEGPTR, pointer);
                hdmi_dev_write(dev, I2CM_ADDRESS, addr);
                hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);

                #if defined(DBG_I2C)
                pr_info("I2CM_SLAVE = 0x%x\r\n", hdmi_dev_read(dev, I2CM_SLAVE));
                pr_info("I2CM_SEGADDR = 0x%x\r\n", hdmi_dev_read(dev, I2CM_SEGADDR));
                pr_info("I2CM_SEGPTR = 0x%x\r\n", hdmi_dev_read(dev, I2CM_SEGPTR));
                pr_info("I2CM_ADDRESS = 0x%x\r\n", hdmi_dev_read(dev, I2CM_ADDRESS));
                pr_info("IH_I2CM_STAT0 = 0x%x\r\n", hdmi_dev_read(dev, IH_I2CM_STAT0));
                #endif

                if(pointer)
                        hdmi_dev_write(dev, I2CM_OPERATION, I2CM_OPERATION_READ_SEQ_EXT);
                else
                        hdmi_dev_write(dev, I2CM_OPERATION, I2CM_OPERATION_READ_SEQ);

                // wait 64bit * 11us
                I2C_DELAY(800);
                status = hdmi_dev_read_mask(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);

                while (status == 0 && (timeout--)) {
                        I2C_DELAY(100);
                        status = hdmi_dev_read_mask(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);
                }

                hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);

                if(status & IH_I2CM_STAT0_I2CMASTERERROR_MASK){
                //LOGGER(SNPS_ERROR, "%s: I2C DDC Read8 extended failed for i2cAddr 0x%x seg 0x%x pointer 0x%x addr 0x%x",__func__,
                        //i2cAddr, segment, pointer, addr);
                        break;
                }

                if(status & IH_I2CM_STAT0_I2CMASTERDONE_MASK){
                        int i = 0;
                        while(i < 8){ //read 8 bytes
                                value[i] = (u8) hdmi_dev_read(dev, I2CM_READ_BUFF0 + (4 * i) );
                                i +=1;
                        }
                        result = 0;
                }
        } while(0);

	return result;
}

/*********************  PUBLIC FUNCTIONS ***********************/

void hdmi_i2cddc_reset(struct hdmi_tx_dev *dev)
{
        if(dev != NULL && !dev->ddc_disable) {
                hdmi_dev_write(dev, I2CM_SOFTRSTZ, 0);
        }
}

void hdmi_i2cddc_bus_clear(struct hdmi_tx_dev *dev)
{
        u32 status = 0;
        int timeout = 20;

        if(dev != NULL && !dev->ddc_disable) {
                /*clear interrupt status */
                hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);

                /* bus clear */
                hdmi_dev_write_mask(dev,  I2CM_OPERATION, I2CM_OPERATION_BUSCLEAR, 1);

                /* Wait maximum 200 us for finish bus clear */
                do {
                        I2C_DELAY(10);
                        status = hdmi_dev_read_mask(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);
                } while (status == 0 && (timeout--));

                /*clear interrupt status */
                hdmi_dev_write(dev, IH_I2CM_STAT0, IH_I2CM_STAT0_I2CMASTERERROR_MASK | IH_I2CM_STAT0_I2CMASTERDONE_MASK);
        }
}

void hdmi_i2cddc_clk_config(struct hdmi_tx_dev *dev, u16 sfrClock, u16 ss_low_ckl, u16 ss_high_ckl, u16 fs_low_ckl, u16 fs_high_ckl)
{
        if(dev != NULL && !dev->ddc_disable) {
        	_standard_speed_low_clk_ctrl(dev, _scl_calc(sfrClock, ss_low_ckl));
        	_standard_speed_high_clk_ctrl(dev, _scl_calc(sfrClock, ss_high_ckl));
        	_fast_speed_low_clk_ctrl(dev, _scl_calc(sfrClock, fs_low_ckl));
        	_fast_speed_high_clk_ctrl(dev, _scl_calc(sfrClock, fs_high_ckl));
        }
}

void hdmi_i2cddc_fast_mode(struct hdmi_tx_dev *dev, u8 value)
{
        if(dev != NULL && !dev->ddc_disable) {
        	/* bit 4 selects between high and standard speed operation */
        	hdmi_dev_write_mask(dev, I2CM_DIV, I2CM_DIV_FAST_STD_MODE_MASK, value);
        }
}

void hdmi_i2cddc_sda_hold(struct hdmi_tx_dev *dev, int hold)
{
        if(dev != NULL && !dev->ddc_disable) {
                hdmi_dev_write_mask(dev, I2CM_SDA_HOLD, I2CM_SDA_HOLD_OSDA_HOLD_MASK, hold);
        }
}

int hdmi_ddc_write(struct hdmi_tx_dev *dev, u8 i2cAddr, u8 addr, u8 len, u8 * data)
{
	int i, status = -1;

        if(dev != NULL && !dev->ddc_disable) {
        	for(i = 0; i < len; i++){
        		int tries = 3;
        		do {
        			status = _write(dev, i2cAddr, addr, data[i]);
        		} while (status && tries--);
                        if(status < 0) {
                                break;
                        }
        	}
        }
        return status;
}


int hdmi_ddc_read(struct hdmi_tx_dev *dev, u8 i2cAddr, u8 segment, u8 pointer, u8 addr, u8 len, u8 * data)
{
        int tries;
        int i, status = -1;
        if(dev != NULL && !dev->ddc_disable) {
        	for(i = 0; i < len;){
        		tries = 3;
        		if ((len - i) >= 8){
        			do {
        				status = _read8(dev, i2cAddr, segment, pointer, addr + i,  &(data[i]));
        			} while (status && tries--);

        			if(status < 0) {
                                        //Error after 3 failed writes
                                        break;
        			}
        			i +=8;
        		} else {
        			do {
        				status = _read(dev, i2cAddr, segment, pointer, addr + i,  &(data[i]));
        			} while (status && tries--);

        			if(status < 0) {
                                        //Error after 3 failed writes
        				break;
        			}
        			i++;
        		}
        	}
        }
	return status;
}

int hdmi_ddc_check(struct hdmi_tx_dev *dev, int addr, int len)
{
        int i, ret = -1;

        if(dev != NULL && !dev->ddc_disable) {
        	u8 data[8] = {0};

                if(len > 8) {
                        len = 8;
                }

                if(len > 0) {
                        ret = hdmi_ddc_read(dev, (0xA0) >> 1, 0x0, 0x0, addr, len, data);

                        printk("[0x%03x] ", addr);
                        for(i = 0;i<len;i++) {
                                printk("0x%02x ", data[i]);
                        }
                        printk("\r\n");
                }
        }
        return ret;
}
