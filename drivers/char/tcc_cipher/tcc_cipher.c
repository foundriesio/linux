/*
 * linux/drivers/char/tcc_cipher/tcc_cipher.c
 *
 * Author:  <linux@telechips.com>
 * Created: March 18, 2010
 * Description: TCC Cipher driver
 *
 * Copyright (C) 20010-2011 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/arm-smccc.h>
#include <linux/uaccess.h>
#include <soc/tcc/tcc-sip.h>

#include <asm/io.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/tcc_cipher.h>

static int cipher_data_debug = 0;
module_param( cipher_data_debug, int, 0644 );
MODULE_PARM_DESC( cipher_data_debug, "Turn on/off device data debugging (default:off)." );
static int cipher_debug = 0;
module_param( cipher_debug, int, 0644 );
MODULE_PARM_DESC( cipher_debug, "Turn on/off device debugging (default:off)." );
#define dprintk( msg... )	if( cipher_debug ) { printk( "tcc_cipher: " msg); }

//#define MAJOR_ID		250
//#define MINOR_ID		0

//#define USE_REV_MEMORY
#ifdef USE_REV_MEMORY
#include <soc/tcc/pmap.h>
#endif

static struct cipher_device
{
	struct device		*dev;

	int					used;

	unsigned int		blockSize;

	dma_addr_t			srcPhy;
	u_char				*srcVir;
	dma_addr_t			dstPhy;
	u_char				*dstVir;
} *cipher_dev;


static int cipher_buffer_length = 4096;

/**
 * @ingroup cipher_drv
 * @brief Set the algorithm
 *
 * @parma[in] opMode Operation Mode, refer to eCipherOPMODE
 * @parma[in] algorithm Algorithm, refer to eCipherALGORITHM
 * @parma[in] arg1 Argument for algorithm\n
 *                 in AES, refer to #eCipherKEYLEN\n
 *                 in DES, refer to #eCipherDESMODE\n
 *                 in MULTI2, round\n
 *                 in CSA2, round\n
 *                 in CSA3, round\n
 *                 in HASH, refer to #eCipherHASH\n
 * @parma[in] arg2 Argument for algorithm\n
 *                 in HASH, round
 */
void tcc_cipher_algorithm(unsigned int opmode, unsigned int algorithm, unsigned int arg1, unsigned int arg2)
{
    struct arm_smccc_res res;
    unsigned int a0 = (unsigned int)((opmode << 16) | algorithm); 

    arm_smccc_smc(SIP_CRYPTO_CIPHER_ALGORITHM, a0, arg1, arg2, 0, 0, 0, 0, &res); 

    return;
}

/**
 * @ingroup cipher_drv
 * @brief Set the initail vector
 *
 * @parma[in] iv Initial vector pointer
 * @parma[in] len Initial vector length
 */
void tcc_cipher_iv(unsigned char *iv, unsigned int len)
{
    struct arm_smccc_res res;
    
    memcpy(cipher_dev->srcVir, iv, len);                                                         
    
    arm_smccc_smc(SIP_CRYPTO_CIPHER_IV, (unsigned long)cipher_dev->srcPhy, len, 0, 0, 0, 0, 0, &res); 

    return;
}

/**
 * @ingroup cipher_drv
 * @brief Set the Key
 *
 * @parma[in] key Key pointer
 * @parma[in] len Key length
 * @parma[in] option Key option\n
 *               It is used in MULTI2 algorithm for verify DATA key or not.
 */
void tcc_cipher_key(unsigned char *key, unsigned int len, unsigned int option)
{
    struct arm_smccc_res res;

    memcpy(cipher_dev->srcVir, key, len);
                                                              
    arm_smccc_smc(SIP_CRYPTO_CIPHER_KEY, cipher_dev->srcPhy, len, option, 0, 0, 0, 0, &res); 

    return;
}

/**                                                                        
 * @ingroup cipher_drv
 * @ntirg Set the command FIFO parameters                                         
 * @param[in] keyLoad load the key or not\n                                
 *                    0 : do not load the key\n                            
 *                    1 : load the key                                     
 * @param[in] ivLoad load the initial vector or not\n                      
 *                    0 : do not load the initial vector\n                 
 *                    1 : load the initial vector                          
 * @param[in] enc Decide encrypt or decrypt, refer to #eCipherMODE         
 * @return none                                                            
 */                                                                        
void tcc_cipher_set(unsigned int keyLoad, unsigned int ivLoad, unsigned int enc)
{
    struct arm_smccc_res res;

    arm_smccc_smc(SIP_CRYPTO_CIPHER_SET, keyLoad, ivLoad, enc, 0, 0, 0, 0, &res); 

    return;
}

/**
 * @ingroup cipher_drv
 * @brief Execute the cipher operation
 *
 * @parma[in] srcAddr Source address
 * @parma[in] dstAddr Destination address
 * @parma[in] len Source length
 *
 * @return 0 Success
 * @return else Fail
 */
static int tcc_cipher_run_inner(unsigned char *srcAddr, unsigned char *dstAddr, unsigned int len, int fromuser)
{
    struct arm_smccc_res res;

	dprintk("%s, len[%u]fromuser[%X]\n", __func__, len, fromuser);

	if (len > cipher_buffer_length) {
		dprintk("len = %d, cipher_buffer_length = %d\n", len, cipher_buffer_length);
		return -1;
	}

	if( ( cipher_dev->srcVir == NULL ) || ( cipher_dev->dstVir == NULL ) ) {
		dprintk("cipher_dev->srcVir = %p, cipher_dev->dstVir = %p\n", cipher_dev->srcVir, cipher_dev->dstVir);
		return -1;
	}

	memset(cipher_dev->srcVir, 0x0, len);
	memset(cipher_dev->dstVir, 0x0, len);

	/* Copy Plain Text from Source Buffer */
	if( fromuser ) {
		if( copy_from_user( cipher_dev->srcVir, srcAddr, len ) ) {
			return -EFAULT;
		}
	}
	else
		memcpy( cipher_dev->srcVir, srcAddr, len );

	udelay( 1 );

    arm_smccc_smc(SIP_CRYPTO_CIPHER_RUN, (unsigned long)cipher_dev->srcPhy, (unsigned long)cipher_dev->dstPhy, len, 0, 0, 0, 0, &res);
     
#ifndef USE_REV_MEMORY
	dma_sync_single_for_cpu( cipher_dev->dev, cipher_dev->dstPhy, len, DMA_FROM_DEVICE );
#endif //USE_REV_MEMORY

	msleep(1);

	/* Copy Cipher Text to Destination Buffer */
	if( fromuser ) {
		if( copy_to_user( dstAddr, cipher_dev->dstVir, len ) ) {
			return -EFAULT;
		}
	}
	else
		memcpy( dstAddr, cipher_dev->dstVir, len );

	if( cipher_data_debug ) {
		unsigned int i;
		unsigned int *pDataAddr;

		pDataAddr = (unsigned int *)cipher_dev->srcVir;
		printk("\n[ Source Text ]\n");
		for( i = 0; i < ( len / 4 ); i+=4 ) {
			printk("0x%08x 0x%08x 0x%08x 0x%08x\n", pDataAddr[i+0], pDataAddr[i+1], pDataAddr[i+2], pDataAddr[i+3]);
		}

		pDataAddr = (unsigned int *)cipher_dev->dstVir;
		printk("\n[ Dest Text ]\n");
		for( i = 0; i < ( ( len * 2 ) / 4 ); i+=4 ) {
			printk("0x%08x 0x%08x 0x%08x 0x%08x\n", pDataAddr[i+0], pDataAddr[i+1], pDataAddr[i+2], pDataAddr[i+3]);
		}
		printk("\n");
	}

	return 0;
}

/**
 * @ingroup cipher_drv
 * @brief Execute the cipher operation with physical address
 *
 * @parma[in] srcAddr Source address
 * @parma[in] dstAddr Destination address
 * @parma[in] len Source length
 *
 * @return 0 Success
 * @return else Fail
 */
static int tcc_cipher_run_phy(unsigned char *srcAddr, unsigned char *dstAddr, unsigned int len)
{
    struct arm_smccc_res res;

    arm_smccc_smc(SIP_CRYPTO_CIPHER_RUN, (unsigned long)srcAddr, (unsigned long)dstAddr, len, 0, 0, 0, 0, &res); 

    msleep(1);

	return 0;
}

/**
 * @ingroup cipher_drv
 * @brief Execute the cipher operation to be used in kernel space
 *
 * @parma[in] srcAddr Source address
 * @parma[in] dstAddr Destination address
 * @parma[in] len Source length
 *
 * @return 0 Success
 * @return else Fail
 */
int tcc_cipher_run(unsigned char *srcAddr, unsigned char *dstAddr, unsigned int len, unsigned int isPhy)
{
	if( isPhy )
		return tcc_cipher_run_phy( srcAddr, dstAddr, len );
	else
		return tcc_cipher_run_inner( srcAddr, dstAddr, len, 0 );
}

/**
 * @ingroup cipher_drv
 * @brief Execute the cipher operation to be used in user space
 *
 * @parma[in] srcAddr Source address
 * @parma[in] dstAddr Destination address
 * @parma[in] len Source length
 *
 * @return 0 Success
 * @return else Fail
 */
static int tcc_cipher_run_usr(unsigned char *srcAddr, unsigned char *dstAddr, unsigned int len, unsigned int isPhy)
{
	if( isPhy )
		return tcc_cipher_run_phy( srcAddr, dstAddr, len );
	else
		return tcc_cipher_run_inner( srcAddr, dstAddr, len, 1 );
}

static long tcc_cipher_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;
	dprintk("%s, cmd=%d\n", __func__, cmd);

	switch( cmd )
	{
		case TCC_CIPHER_IOCTL_ALGORITHM:
			{
				tCIPHER_ALGORITHM tAlgo;
				if( copy_from_user(&tAlgo, (const tCIPHER_ALGORITHM *)arg, sizeof(tCIPHER_ALGORITHM)) ) {
					err = -EFAULT;
					break;
				}
				tcc_cipher_algorithm(tAlgo.opMode, tAlgo.algorithm, tAlgo.arg1, tAlgo.arg2);
			}
			break;

		case TCC_CIPHER_IOCTL_KEY:
			{
				tCIPHER_KEY tKey;
				if( copy_from_user(&tKey, (const tCIPHER_KEY *)arg, sizeof(tCIPHER_KEY)) ) {
					err = -EFAULT;
					break;
				}
				tcc_cipher_key(tKey.key, tKey.len, tKey.option);
			}
			break;

		case TCC_CIPHER_IOCTL_IV:
			{
				tCIPHER_IV tIV;
				if( copy_from_user(&tIV, (const tCIPHER_IV *)arg, sizeof(tCIPHER_IV)) ) {
					err = -EFAULT;
					break;
				}
				tcc_cipher_iv(tIV.iv, tIV.len);
			}
			break;

		case TCC_CIPHER_IOCTL_SET:
			{
				tCIPHER_SET tSet;
				if( copy_from_user(&tSet, (const tCIPHER_SET *)arg, sizeof(tCIPHER_SET)) ) {
					err = -EFAULT;
					break;
				}
				 tcc_cipher_set(tSet.keyLoad, tSet.ivLoad, tSet.enc);
			}
			break;
            
		case TCC_CIPHER_IOCTL_RUN:
			{
				tCIPHER_RUN tRun;
				if( copy_from_user(&tRun, (const tCIPHER_RUN *)arg, sizeof(tCIPHER_RUN)) ) {
					err = -EFAULT;
					break;
				}
				err = tcc_cipher_run_usr(tRun.srcAddr, tRun.dstAddr, tRun.len, tRun.isPhy);
			}
			break;

		default:
			printk("err: unkown command(%d)\n", cmd);
			err = -ENOTTY;
			break;
	}

	return err;
}

int tcc_cipher_open(struct inode *inode, struct file *filp)
{
    struct arm_smccc_res res;

	if( cipher_dev->used ) {
		printk("%s device already used", __func__);
		return -EBUSY;
	}
                                                          
	dprintk("%s\n", __func__);

    arm_smccc_smc(SIP_CRYPTO_CIPHER_OPEN, 0, 0, 0, 0, 0, 0, 0, &res); 

	cipher_dev->used = 1;

	return 0;
}

int tcc_cipher_release(struct inode *inode, struct file *file)
{
    struct arm_smccc_res res;

	dprintk("%s\n", __func__);

    arm_smccc_smc(SIP_CRYPTO_CIPHER_CLOSE, 0, 0, 0, 0, 0, 0, 0, &res); 

	cipher_dev->used = 0;

	return 0;
}

static int tcc_cipher_mmap(struct file *filp, struct vm_area_struct *vma) {
	if( remap_pfn_range( vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, pgprot_noncached( vma->vm_page_prot ) ) != 0 ) {
		printk("%s remap_pfn_range error\n", __func__);
		return -EAGAIN;
	}
	return 0;
}

static const struct file_operations tcc_cipher_fops =
{
	.owner		= THIS_MODULE,
	.open		= tcc_cipher_open,
	.unlocked_ioctl	= tcc_cipher_ioctl,
	.mmap		= tcc_cipher_mmap,
	.release	= tcc_cipher_release,
};

static struct miscdevice cipher_misc_device =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = TCCCIPHER_DEVICE_NAME,
	.fops = &tcc_cipher_fops,
};

static int tcc_cipher_probe(struct platform_device *pdev)
{
#ifdef USE_REV_MEMORY
	pmap_t pmap_secure_hash;
#endif

	if( !pdev->dev.of_node ) {
		dev_err(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}

	cipher_dev = devm_kzalloc( &pdev->dev, sizeof(struct cipher_device), GFP_KERNEL );
	if( cipher_dev == NULL ) {
		printk("%s failed to allocate cipher_device\n", __func__);
		return -ENOMEM;
	}

	cipher_dev->dev = &pdev->dev;

#ifdef USE_REV_MEMORY
//	if (pmap_get_info("secure_hash", &pmap_secure_hash)) {
	if (pmap_get_info("tsif", &pmap_secure_hash)) {
		printk("pmap_secure_hash.base = %x, pmap_secure_hash.size = %d\n", pmap_secure_hash.base, pmap_secure_hash.size);

		cipher_buffer_length = pmap_secure_hash.size;
		cipher_dev->srcPhy = pmap_secure_hash.base;
		cipher_dev->srcVir = ioremap_nocache(cipher_dev->srcPhy, cipher_buffer_length);
		if (!cipher_dev->srcVir) {
			printk("cipher_dev->srcPhy = %llx, cipher_dev->srcVir = %p\n", cipher_dev->srcPhy, cipher_dev->srcVir);
			return -1;
		}

		cipher_dev->dstPhy = cipher_dev->srcPhy;
		cipher_dev->dstVir = cipher_dev->srcVir;
	}
	else {
		printk("pmap_get_info failed\n");
		return -1;
	}
#else //USE_REV_MEMORY
	cipher_dev->srcVir = dma_alloc_coherent( cipher_dev->dev, cipher_buffer_length, &cipher_dev->srcPhy, GFP_KERNEL );
	if( cipher_dev->srcVir == NULL ) {
		dprintk("%s cipher_dev->srcVir = %p\n", __func__, cipher_dev->srcVir);
		return -1;
	}

	cipher_dev->dstVir = dma_alloc_coherent( cipher_dev->dev, cipher_buffer_length, &cipher_dev->dstPhy, GFP_KERNEL );
	if(  cipher_dev->dstVir == NULL ) {
		dprintk("%s cipher_dev->dstVir = %p\n", __func__, cipher_dev->dstVir);
	    dma_free_coherent( cipher_dev->dev, cipher_buffer_length, cipher_dev->srcVir, cipher_dev->srcPhy );
		return -1;
	}

    //printk("cipher_dev srcAddr[%p %p] dstAddr[%p %p]\n", cipher_dev->srcVir, cipher_dev->srcPhy, cipher_dev->dstVir, cipher_dev->dstPhy);
#endif //USE_REV_MEMORY

	if( misc_register( &cipher_misc_device ) )
	{
		printk("%s Couldn't register device\n", __func__);
		dma_free_coherent( cipher_dev->dev, cipher_buffer_length, cipher_dev->srcVir, cipher_dev->srcPhy );
		dma_free_coherent( cipher_dev->dev, cipher_buffer_length, cipher_dev->dstVir, cipher_dev->dstPhy );
		return -EBUSY;
	}

	cipher_dev->used = 0;

	return 0;
}

static int tcc_cipher_remove(struct platform_device *pdev)
{
	misc_deregister( &cipher_misc_device );

#ifdef USE_REV_MEMORY
	iounmap((void*)cipher_dev->srcPhy);
#else //USE_REV_MEMORY
	dma_free_coherent( cipher_dev->dev, cipher_buffer_length, cipher_dev->srcVir, cipher_dev->srcPhy );
	dma_free_coherent( cipher_dev->dev, cipher_buffer_length, cipher_dev->dstVir, cipher_dev->dstPhy );
#endif //USE_REV_MEMORY

	devm_kfree( cipher_dev->dev, cipher_dev );

	return 0;
}

#ifdef CONFIG_PM
static int tcc_cipher_suspend( struct platform_device *pdev, pm_message_t state )
{
	dprintk("%s\n", __func__);
	return 0;
}

static int tcc_cipher_resume( struct platform_device *pdev )
{
	dprintk("%s\n", __func__);
	return 0;
}
#else
#define tcc_cipher_suspend NULL
#define tcc_cipher_resume NULL
#endif

#ifdef CONFIG_OF
static struct of_device_id cipher_of_match[] = {
	{ .compatible = "telechips,tcc-cipher" },
	{ "", "", "", NULL },
};
MODULE_DEVICE_TABLE(of, cipher_of_match);
#endif

static struct platform_driver cipher_driver =
{
	.driver	=
	{
		.name	= "cipher",
		.owner 	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr( cipher_of_match )
#endif
	},
	.probe		= tcc_cipher_probe,
	.remove		= tcc_cipher_remove,
#ifdef CONFIG_PM
	.suspend		= tcc_cipher_suspend,
	.resume		= tcc_cipher_resume,
#endif
};

static int __init tcc_cipher_init( void )
{
	int	result =0;
	dprintk("Telechips Cipher Driver Init\n");
	result = platform_driver_register( &cipher_driver );
	if( result ) {
		dprintk("%s platform_driver_register err \n", __func__);
		return 0;
	}

	return result;
}

static void __exit tcc_cipher_exit( void )
{
	dprintk("Telechips Cipher Driver Exit \n");
	platform_driver_unregister( &cipher_driver );
}

module_init( tcc_cipher_init );
module_exit( tcc_cipher_exit );

MODULE_AUTHOR( "linux <linux@telechips.com>" );
MODULE_DESCRIPTION( "Telechips TCC CIPHER driver" );
MODULE_LICENSE( "GPL" );

