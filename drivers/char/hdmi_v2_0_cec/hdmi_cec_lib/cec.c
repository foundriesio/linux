/*
 * cec.c
 *
 *  Created on: Jan 8, 2015
 *      Author: 
 */
#include "../include/hdmi_cec.h"
#include "cec_reg.h"
#include "cec.h"
#include "cec_access.h"


/******************************************************************************
 *
 *****************************************************************************/
/**
 * Wait for message (quit after a given period)
 * @param dev:     address and device information
 * @param buf:     buffer to hold data
 * @param size:    maximum data length [byte]
 * @param timeout: time out period [ms]
 * @return error code or received bytes
 */
static int cec_msgRx(struct cec_device * dev, char *buf, unsigned size,
		 int timeout)
{
	int i, retval = -1;

	if (timeout < 0) {
		/* TO BE USED only by Receptor */
		while (cec_GetLocked(dev) == 0)
			;
	} else {
		for (i = 0; i < (timeout/100); i++) {
			if (cec_GetLocked(dev) != 0)
				break;
			msleep(1);
		}
	}
	if (cec_GetLocked(dev) != 0) {
		retval = cec_CfgRxBuf(dev, buf, size);
		cec_IntClear(dev, CEC_MASK_EOM_MASK | CEC_MASK_ERROR_FLOW_MASK);
		cec_SetLocked(dev);
	}
	return retval;
}

/**
 * Send a message (retry in case of failure)
 * @param dev:   address and device information
 * @param buf:   data to send
 * @param size:  data length [byte]
 * @param retry: maximum number of retries
 * @return error code or transmitted bytes
 */
static int cec_msgTx(struct cec_device * dev, const char *buf, unsigned size,
		 unsigned retry)
{
	int i, retval = 0; 

	if (size > 0) {
		if (cec_CfgTxBuf(dev, buf, size) == (int)size) {
			for (i = 0; i < (int)retry; i++) {
				if (cec_CfgSignalFreeTime(dev, 5))
					break;

				cec_IntClear(dev, CEC_MASK_DONE_MASK|CEC_MASK_NACK_MASK|CEC_MASK_ARB_LOST_MASK|CEC_MASK_ERROR_INITIATOR_MASK);
				cec_SetSend(dev);
				while (cec_GetSend(dev) != 0);
				
#if 0 // for debuging
				printk("CEC interrupt status = 0x%08x \r\n",cec_dev_read(dev,IO_IH_CEC_STAT0));
#endif

				if (cec_IntStatus(dev, CEC_MASK_NACK_MASK) != 0) {
					retval = -1;
				}

				if (cec_IntStatus(dev, CEC_MASK_ERROR_INITIATOR_MASK) != 0) {
					msleep(100);
					continue;
				}

				if (cec_IntStatus(dev, CEC_MASK_DONE_MASK) != 0) {
					retval = size;
					break;
				}
			}
		}
	}

	msleep(1);

	return retval;
}

int cec_CfgWakeupFlag(struct cec_device * dev, int wakeup)
{
	if(wakeup) {
		cec_IntDisable(dev, CEC_MASK_DONE_MASK|CEC_MASK_EOM_MASK|CEC_MASK_NACK_MASK|
				CEC_MASK_ARB_LOST_MASK|CEC_MASK_ERROR_FLOW_MASK|CEC_MASK_ERROR_INITIATOR_MASK);
	
		cec_dev_write(dev,IH_MUTE,cec_dev_read(dev,IH_MUTE) & ~IH_MUTE_MUTE_WAKEUP_INTERRUPT_MASK);
		
		cec_dev_pmu_write_mask(dev, 0x20, 0x2, 0x1);
		printk("[%s] PMU_WKUPEN0(Reg=0x14400020) = 0x%08x \n",__func__,cec_dev_pmu_read(dev,0x20));
		
		cec_dev_pmu_write_mask(dev, 0x28, 0x2, 0x0);
		printk("[%s] PMU_WKUPPOL0(Reg=0x14400028) = 0x%08x \n",__func__,cec_dev_pmu_read(dev,0x28));

		cec_dev_pmu_write_mask(dev, 0x30, 0x2, 0x1);
		printk("[%s] PMU_WKUPCLR0(Reg=0x14400030) = 0x%08x \n",__func__,cec_dev_pmu_read(dev,0x30));


		cec_dev_write(dev,CEC_WAKEUPCTRL,0xff);

		cec_IntClear(dev, CEC_MASK_WAKEUP_MASK);
		cec_IntEnable(dev, CEC_MASK_WAKEUP_MASK);
		cec_CfgStandbyMode(dev, 1);

		printk("[%s] CEC_WAKEUPCTRL(Offset=0x7D31) = 0x%08x \n",__func__,cec_dev_read(dev,CEC_WAKEUPCTRL));
		printk("[%s] IH_MUTE(Offset=0x1FF) = 0x%08x \n",__func__,cec_dev_read(dev,IH_MUTE));
		printk("[%s] IH_MUTE_CEC_STAT0(Offset=0x186) = 0x%08x \n",__func__,cec_dev_read(dev,IO_IH_MUTE_CEC_STAT0));
	} else {
		cec_dev_write(dev,IH_MUTE,cec_dev_read(dev,IH_MUTE) | IH_MUTE_MUTE_WAKEUP_INTERRUPT_MASK);
		cec_dev_write_mask(dev,CEC_WAKEUPCTRL,WAKEUP_MASK_FLAG,0x0);
		cec_IntDisable(dev, CEC_MASK_WAKEUP_MASK);
	}
	return 0;
}

int cec_check_wake_up_interrupt(struct cec_device * dev)
{
	
	printk("[%s] IH_CEC_STAT0(Offset=0x106) = 0x%08x, %s \n",__func__,cec_dev_read(dev,IO_IH_CEC_STAT0),
		cec_dev_read_mask(dev,IO_IH_CEC_STAT0,CEC_MASK_WAKEUP_MASK) == 1 ? "Wake-up Interrupt Occurred" : "Wake-up Interrupt Not-occurred");

	printk("[%s] PMU_FSMSTS(Reg=0x14400018), 0x%08x, %s \n",__func__,cec_dev_pmu_read(dev,0x18),
		(cec_dev_pmu_read(dev,0x18) == 0x80000000) ? "Wake-up Signal Occurred" : "Wake-up Signal Not-occurred");

	return 0;
}

/**
 * Open a CEC controller
 * @warning Execute before start using a CEC controller
 * @param dev:  address and device information
 * @return error code
 */
int cec_Init(struct cec_device * dev)
{
	/*
	 * This function is also called after wake up,
	 * so it must NOT reset logical address allocation
	 */

	cec_dev_write(dev,IH_MUTE,cec_dev_read(dev,IH_MUTE) & ~IH_MUTE_MUTE_ALL_INTERRUPT_MASK);

	cec_CfgStandbyMode(dev, 0);
	cec_IntDisable(dev, CEC_MASK_WAKEUP_MASK);
	cec_IntClear(dev, CEC_MASK_WAKEUP_MASK);
	cec_IntEnable(dev, CEC_MASK_DONE_MASK|CEC_MASK_EOM_MASK|CEC_MASK_NACK_MASK|
			CEC_MASK_ARB_LOST_MASK|CEC_MASK_ERROR_FLOW_MASK|CEC_MASK_ERROR_INITIATOR_MASK);

	return 0;
}

/**
 * Close a CEC controller
 * @warning Execute before stop using a CEC controller
 * @param dev:    address and device information
 * @param wakeup: enable wake up mode (don't enable it in TX side)
 * @return error code
 */
int cec_Disable(struct cec_device * dev, int wakeup)
{

	cec_dev_write(dev,IH_MUTE,cec_dev_read(dev,IH_MUTE) | IH_MUTE_MUTE_ALL_INTERRUPT_MASK);
	cec_IntDisable(dev, CEC_MASK_DONE_MASK|CEC_MASK_EOM_MASK|CEC_MASK_NACK_MASK|
			CEC_MASK_ARB_LOST_MASK|CEC_MASK_ERROR_FLOW_MASK|CEC_MASK_ERROR_INITIATOR_MASK);
	if (wakeup) {
		cec_IntClear(dev, CEC_MASK_WAKEUP_MASK);
		cec_IntEnable(dev, CEC_MASK_WAKEUP_MASK);
		cec_CfgStandbyMode(dev, 1);
	} else
		cec_CfgLogicAddr(dev, BCST_ADDR, 1);
	return 0;
}

/**
 * Configure logical address
 * @param dev:    address and device information
 * @param addr:   logical address
 * @param enable: alloc/free address
 * @return error code
 */
int cec_CfgLogicAddr(struct cec_device * dev, unsigned addr, int enable)
{
	unsigned char regs;

	if (addr > BCST_ADDR) {
		printk("%s: invalid parameter",__func__);
		return -1;
	}
	if (addr == BCST_ADDR) {
		if (enable) {
			cec_dev_write(dev, CEC_ADDR_H, 0x80);
			cec_dev_write(dev, CEC_ADDR_L, 0x00);
			return 0;
		} else {
			printk("%s: cannot de-allocate broadcast logical address",__func__);
			return -1;
		}
	}
	regs = (addr > 7) ? CEC_ADDR_H : CEC_ADDR_L;
	addr = (addr > 7) ? (addr - 8) : addr;
	if (enable)
		cec_dev_write(dev, regs, cec_dev_read(dev, regs) |  (1 << addr));
	else
		cec_dev_write(dev, regs, cec_dev_read(dev, regs) & ~(1 << addr));
	
	return 0;
}

/**
 * Configure standby mode
 * @param dev:    address and device information
 * @param enable: if true enable standby mode
 * @return error code
 */
int cec_CfgStandbyMode(struct cec_device * dev, int enable)
{

	if (enable)
		cec_dev_write(dev, CEC_CTRL,cec_dev_read(dev, CEC_CTRL) |  CEC_CTRL_STANDBY_MASK);
	else
		cec_dev_write(dev, CEC_CTRL,cec_dev_read(dev, CEC_CTRL) & ~CEC_CTRL_STANDBY_MASK);
	return 0;
}



/**
 * Configure signal free time
 * @param dev:  address and device information
 * @param time: time between attempts [nr of frames]
 * @return error code
 */
int cec_CfgSignalFreeTime(struct cec_device * dev, int time)
{
	unsigned char data;

	data = cec_dev_read(dev, CEC_CTRL) & ~(CEC_CTRL_FRAME_TYP_MASK);
	if (3 == time)
		data |= (0<<1 | 0<<2);                  /* 0 */
	else if (5 == time)
		data |= FRAME_TYP0;                /* 1 */
	else if (7 == time)
		data |= FRAME_TYP1 /*| FRAME_TYP0*/;   /* 2 */
	else
		return -1;
	cec_dev_write(dev, CEC_CTRL, data);
	return 0;
}

/**
 * Read send status
 * @param dev:  address and device information
 * @return SEND status
 */
int cec_GetSend(struct cec_device * dev)
{
	return (cec_dev_read(dev, CEC_CTRL) & CEC_CTRL_SEND_MASK) != 0;
}

/**
 * Set send status
 * @param dev: address and device information
 * @return error code
 */
int cec_SetSend(struct cec_device * dev)
{
	cec_dev_write(dev, CEC_CTRL, cec_dev_read(dev, CEC_CTRL) | CEC_CTRL_SEND_MASK);
	return 0;
}

/**
 * Clear interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to clear
 * @return error code
 */
int cec_IntClear(struct cec_device * dev, unsigned char mask)
{
	cec_dev_write(dev, IO_IH_CEC_STAT0, mask);
	return 0;
}

/**
 * Disable interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to disable
 * @return error code
 */
int cec_IntDisable(struct cec_device * dev, unsigned char mask)
{
	cec_dev_write(dev, IO_IH_MUTE_CEC_STAT0, cec_dev_read(dev, IO_IH_MUTE_CEC_STAT0) | mask);
	return 0;
}

/**
 * Enable interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to enable
 * @return error code
 */
int cec_IntEnable(struct cec_device * dev, unsigned char mask)
{
	cec_dev_write(dev, IO_IH_MUTE_CEC_STAT0, cec_dev_read(dev, IO_IH_MUTE_CEC_STAT0) & ~mask);
	return 0;
}

/**
 * Read interrupts
 * @param dev:  address and device information
 * @param mask: interrupt mask to read
 * @return INT content
 */
int cec_IntStatus(struct cec_device * dev, unsigned char mask)
{
	return cec_dev_read(dev, IO_IH_CEC_STAT0) & mask;
}

/**
 * Write transmission buffer
 * @param dev:  address and device information
 * @param buf:  data to transmit
 * @param size: data length [byte]
 * @return error code or bytes configured
 */
int cec_CfgTxBuf(struct cec_device * dev, const char *buf, unsigned size)
{
	int index;	

	if (size > CEC_TX_DATA_SIZE) {
		printk("%s: invalid parameter\n",__func__);
		return -1;
	}

	for(index = 0; index < size; index++)
		cec_dev_write(dev, CEC_TX_DATA + sizeof(u32)*index, buf[index]);

	cec_dev_write(dev, CEC_TX_CNT, size);   /* mask 7-5? */

	return size;
}

/**
 * Read reception buffer
 * @param dev:  address and device information
 * @param buf:  buffer to hold receive data
 * @param size: maximum data length [byte]
 * @return error code or received bytes
 */
int cec_CfgRxBuf(struct cec_device * dev, char *buf, unsigned size)
{
	unsigned char cnt = 0;
	int index;

	cnt = cec_dev_read(dev, CEC_RX_CNT);   /* mask 7-5? */
	cnt = (cnt > size) ? size : cnt;
	if (cnt > CEC_RX_DATA_SIZE) {
		printk("%s: wrong byte count\n",__func__);		
		return -1;
	}

	for(index = 0; index < cnt; index++)
		buf[index] = cec_dev_read(dev,CEC_RX_DATA + sizeof(u32)*index);

	return cnt;
}

/**
 * Read locked status
 * @param dev: address and device information
 * @return LOCKED status
 */
int cec_GetLocked(struct cec_device * dev)
{
	return (cec_dev_read(dev, CEC_LOCK) & CEC_LOCK_LOCKED_BUFFER_MASK) != 0;
}

/**
 * Set locked status
 * @param dev: address and device information
 * @return error code
 */
int cec_SetLocked(struct cec_device * dev)
{
	cec_dev_write(dev, CEC_LOCK, cec_dev_read(dev, CEC_LOCK) & ~CEC_LOCK_LOCKED_BUFFER_MASK);
	return TRUE;
}

/**
 * Configure broadcast rejection
 * @param dev:   address and device information
 * @param enable: if true enable broadcast rejection
 * @return error code
 */
int cec_CfgBroadcastNAK(struct cec_device * dev, int enable)
{
//	printk("%s: %i\n",__func__,enable);	
	if (enable)
		cec_dev_write(dev, CEC_CTRL, cec_dev_read(dev, CEC_CTRL) |  CEC_CTRL_BC_NACK_MASK);
	else
		cec_dev_write(dev, CEC_CTRL, cec_dev_read(dev, CEC_CTRL) & ~CEC_CTRL_BC_NACK_MASK);
	return TRUE;
}

/**
 * Attempt to receive data (quit after 1s)
 * @warning Caller context must allow sleeping
 * @param dev:  address and device information
 * @param buf:  container for incoming data (first byte will be header block)
 * @param size: maximum data size [byte]
 * @return error code or received message size [byte]
 */
int cec_ctrlReceiveFrame(struct cec_device * dev, char *buf, unsigned size)
{
	if (buf == NULL) {
		printk("%s: invalid parameter\n",__func__);
		return -1;
	}
	return cec_msgRx(dev, buf, size, 1000);
}

/**
 * Attempt to send data (retry 5 times)
 * @warning Doesn't check if source address is allocated
 * @param dev:  address and device information
 * @param buf:  container of the outgoing data (without header block)
 * @param size: request size [byte] (must be less than 15)
 * @param src:  initiator logical address
 * @param dst:  destination logical address
 * @return error code or request size [byte]
 */
int cec_ctrlSendFrame(struct cec_device * dev, const char *buf, unsigned size)
{
	if (buf == NULL || size >= CEC_TX_DATA_SIZE) {
		printk("%s: invalid parameter\n",__func__);		
		return -1;
	}

	 return cec_msgTx(dev, buf, size, 5);

}

