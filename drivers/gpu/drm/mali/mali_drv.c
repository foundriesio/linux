/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * @file mali_drv.c
 * Implementation of the Linux device driver entrypoints for Mali DRM
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <drm/drmP.h>
#include <drm/drm_gem_cma_helper.h>
#include "mali_drv.h"

static struct platform_device *mali_drm_pdev;

void mali_drm_preclose(struct drm_device *dev, struct drm_file *file_priv)
{
}

void mali_drm_lastclose(struct drm_device *dev)
{
}

static int mali_drm_suspend(struct drm_device *dev, pm_message_t state)
{
	return 0;
}

static int mali_drm_resume(struct drm_device *dev)
{
	return 0;
}

static int mali_drm_load(struct drm_device *dev, unsigned long chipset)
{
	return 0;
}

static int mali_drm_unload(struct drm_device *dev)
{
	return 0;
}

static const struct file_operations mali_drm_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.poll = drm_poll,
	.read = drm_read,
	.llseek = no_llseek,
	.mmap = drm_gem_cma_mmap,
};

static struct drm_driver driver =
{
	.driver_features = DRIVER_GEM | DRIVER_PRIME,
	.load = mali_drm_load,
	.unload = mali_drm_unload,
	.preclose = mali_drm_preclose,
	.lastclose = mali_drm_lastclose,
	.set_busid = drm_platform_set_busid,
	.gem_free_object = drm_gem_cma_free_object,
	.gem_vm_ops = &drm_gem_cma_vm_ops,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_get_sg_table	= drm_gem_cma_prime_get_sg_table,
	.gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,
	.dumb_create		= drm_gem_cma_dumb_create,
	.dumb_map_offset = drm_gem_cma_dumb_map_offset,
	.dumb_destroy = drm_gem_dumb_destroy,
	.suspend = mali_drm_suspend,
	.resume = mali_drm_resume,
	.ioctls = NULL,
	.fops = &mali_drm_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static struct platform_driver platform_drm_driver;

int mali_drm_init(struct platform_device *dev)
{
#if 0
	printk(KERN_INFO "Mali DRM initialize, driver name: %s, version %d.%d\n", DRIVER_NAME, DRIVER_MAJOR, DRIVER_MINOR);
	if (dev == dev0) {
		printk(KERN_INFO "Mali dev0 driver detect !!\n");
		driver.num_ioctls = 0;
		return drm_platform_init(&driver, dev0);
	}
	return 0;
#else
	printk(KERN_INFO "Mali DRM initialize, driver name: %s, version %d.%d\n", DRIVER_NAME, DRIVER_MAJOR, DRIVER_MINOR);
	driver.num_ioctls = 0;
	drm_debug = 0;

	return drm_platform_init(&driver, dev);

#endif


}

void mali_drm_exit(struct platform_device *dev)
{
#if 0
	if (dev0 == dev)
		drm_platform_exit(&driver, dev);
#else
//	drm_platform_exit(&driver, dev);
#endif
}

static int mali_platform_drm_probe(struct platform_device *dev)
{
	dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	return mali_drm_init(dev);
}

static int mali_platform_drm_remove(struct platform_device *dev)
{
	mali_drm_exit(dev);

	return 0;
}

static int mali_platform_drm_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int mali_platform_drm_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver platform_drm_driver = {
	.probe = mali_platform_drm_probe,
	.remove = mali_platform_drm_remove,
	.suspend = mali_platform_drm_suspend,
	.resume = mali_platform_drm_resume,
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
	},
};

static int __init mali_platform_drm_init(void)
{
	int ret = 0;
	mali_drm_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
	if (IS_ERR(mali_drm_pdev)) {
		ret = PTR_ERR(mali_drm_pdev);
		return ret;
	}
	return platform_driver_register( &platform_drm_driver );
}

static void __exit mali_platform_drm_exit(void)
{
#if 0
	platform_driver_unregister(&platform_drm_driver);
	platform_device_unregister(dev0);
#else
	platform_driver_unregister( &platform_drm_driver );
#endif
}

#ifdef MODULE
module_init(mali_platform_drm_init);
#else
late_initcall(mali_platform_drm_init);
#endif
module_exit(mali_platform_drm_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_ALIAS(DRIVER_ALIAS);
MODULE_INFO(vermagic, VERMAGIC_STRING);
