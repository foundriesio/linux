/*
 * sdhci-tcc.c support for Telechips SoCs
 *
 * Author: Telechips Inc.
 *
 * Based on sdhci-cns3xxx.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#include <linux/scatterlist.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/interrupt.h>

#include <linux/soc/telechips/tcc_sc_protocol.h>

enum tcc_sc_mmc_cookie {
	COOKIE_UNMAPPED,
	COOKIE_PRE_MAPPED,
	COOKIE_MAPPED,
};

struct tcc_sc_mmc_host {
	struct device *dev;
	const struct tcc_sc_fw_handle *handle;
	struct tcc_sc_fw_prot_mmc mmc_prot_info;

	/* Bounce buffer */
	char *bounce_buffer;	/* For packing SDMA reads/writes */
	dma_addr_t bounce_addr;
	unsigned int bounce_buffer_size;

	/* Internal data */
	struct mmc_request *mrq;
	struct mmc_host *mmc;	/* MMC structure */
	struct mmc_host_ops mmc_host_ops;	/* MMC host ops */
	u64 dma_mask;		/* custom DMA mask */

	struct tasklet_struct finish_tasklet;	/* Tasklet structures */
	struct timer_list timer;	/* Timer for timeouts */

	spinlock_t lock;	/* Mutex */
};

static bool tcc_sc_mmc_request_done(struct tcc_sc_mmc_host *host)
{
	unsigned long flags;
	struct mmc_request *mrq;
	struct mmc_data *data;

	spin_lock_irqsave(&host->lock, flags);

	mrq = host->mrq;

	if (!mrq) {
		spin_unlock_irqrestore(&host->lock, flags);
		return true;
	}

	data = mrq->data;
	del_timer(&host->timer);

	if (data && data->host_cookie == COOKIE_MAPPED) {
		if (host->bounce_buffer) {
			/*
			 * On reads, copy the bounced data into the
			 * sglist
			 */
			if (mmc_get_dma_dir(data) == DMA_FROM_DEVICE) {
				unsigned int length = data->bytes_xfered;

				if (length > host->bounce_buffer_size) {
					pr_err("%s: bounce buffer is %u bytes but DMA claims to have transferred %u bytes\n",
					       mmc_hostname(host->mmc),
					       host->bounce_buffer_size,
					       data->bytes_xfered);
					/* Cap it down and continue */
					length = host->bounce_buffer_size;
				}
				dma_sync_single_for_cpu(
					host->mmc->parent,
					host->bounce_addr,
					host->bounce_buffer_size,
					DMA_FROM_DEVICE);
				sg_copy_from_buffer(data->sg,
					data->sg_len,
					host->bounce_buffer,
					length);
			} else {
				/* No copying, just switch ownership */
				dma_sync_single_for_cpu(
					host->mmc->parent,
					host->bounce_addr,
					host->bounce_buffer_size,
					mmc_get_dma_dir(data));
			}
		} else {
			/* Unmap the raw data */
			dma_unmap_sg(mmc_dev(host->mmc), data->sg,
				     data->sg_len,
				     mmc_get_dma_dir(data));
		}
		data->host_cookie = COOKIE_UNMAPPED;
	}

	host->mrq = NULL;

	spin_unlock_irqrestore(&host->lock, flags);
	mmc_request_done(host->mmc, mrq);

	return false;
}

static void tcc_sc_mmc_tasklet_finish(unsigned long param)
{
	struct tcc_sc_mmc_host *host = (struct tcc_sc_mmc_host *)param;

	while (!tcc_sc_mmc_request_done(host))
		;
}

static void tcc_sc_mmc_timeout_timer(unsigned long data)
{
	struct tcc_sc_mmc_host *host;
	unsigned long flags;

	host = (struct tcc_sc_mmc_host*)data;

	spin_lock_irqsave(&host->lock, flags);

	if (host->mrq) {
		pr_err("%s: [ERROR][TCC_SC_MMC]Timeout waiting for hardware cmd interrupt.\n",
		       mmc_hostname(host->mmc));

		host->mrq->cmd->error = -ETIMEDOUT;

		/* Finish request */
		tasklet_schedule(&host->finish_tasklet);
	}

	spin_unlock_irqrestore(&host->lock, flags);
}


static int tcc_sc_mmc_pre_dma_transfer(struct tcc_sc_mmc_host *host,
				  struct mmc_data *data, int cookie)
{
	int sg_count;

	/*
	 * If the data buffers are already mapped, return the previous
	 * dma_map_sg() result.
	 */
	if (data->host_cookie == COOKIE_PRE_MAPPED)
		return data->sg_count;

	/* Bounce write requests to the bounce buffer */
	if (host->bounce_buffer) {
		unsigned int length = data->blksz * data->blocks;

		if (length > host->bounce_buffer_size) {
			pr_err("%s: asked for transfer of %u bytes exceeds bounce buffer %u bytes\n",
			       mmc_hostname(host->mmc), length,
			       host->bounce_buffer_size);
			return -EIO;
		}
		if (mmc_get_dma_dir(data) == DMA_TO_DEVICE) {
			/* Copy the data to the bounce buffer */
			sg_copy_to_buffer(data->sg, data->sg_len,
					  host->bounce_buffer,
					  length);
		}
		/* Switch ownership to the DMA */
		dma_sync_single_for_device(host->mmc->parent,
					   host->bounce_addr,
					   host->bounce_buffer_size,
					   mmc_get_dma_dir(data));
		/* Just a dummy value */
		sg_count = 1;
	} else {
		/* Just access the data directly from memory */
		sg_count = dma_map_sg(mmc_dev(host->mmc),
				      data->sg, data->sg_len,
				      mmc_get_dma_dir(data));
	}

	if (sg_count == 0)
		return -ENOSPC;

	data->sg_count = sg_count;
	data->host_cookie = cookie;

	return sg_count;
}

static void tcc_sc_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct tcc_sc_mmc_host *host;
	const struct tcc_sc_fw_handle *handle;
	struct tcc_sc_fw_mmc_cmd cmd;
	struct tcc_sc_fw_mmc_data data;
	int ret;
	unsigned long timeout;
	struct scatterlist sg;

	if(!mmc) {
		printk("[ERROR][TCC_SC_MMC] mmc is null\n");
		return;
	}

	host = mmc_priv(mmc);
	if(!host) {
		printk("%s: [ERROR][TCC_SC_MMC] host is null\n",
		       mmc_hostname(mmc));
		return;
	}

	handle = host->handle;
	if(!handle) {
		printk("%s: [ERROR][TCC_SC_MMC] handle is null\n",
		       mmc_hostname(mmc));
		return;
	}

	if(!mrq) {
		printk("%s: [ERROR][TCC_SC_MMC] mrq is null\n",
		       mmc_hostname(mmc));
		return;
	}

	/* Ignore stop command */
	if (mrq->stop) {
		mrq->data->stop = NULL;
		mrq->stop = NULL;
	}

	host->mrq = mrq;

	/* Request to send command */
	cmd.opcode = mrq->cmd->opcode;
	cmd.arg = mrq->cmd->arg;
	cmd.flags = mmc_resp_type(mrq->cmd);
	if(mmc->card) {
		cmd.part_num = (mmc->card->ext_csd.part_config & EXT_CSD_PART_CONFIG_ACC_MASK);
	} else {
		cmd.part_num = 0;
	}

	if(mrq->cmd->data) {
		
		int sg_cnt = tcc_sc_mmc_pre_dma_transfer(host, mrq->cmd->data, COOKIE_MAPPED);

		if (sg_cnt <= 0) {
			/*
			 * This only happens when someone fed
			 * us an invalid request.
			 */
			WARN_ON(1);
			mrq->cmd->error = sg_cnt;
			mrq->cmd->data->error = sg_cnt;

			/* Finish request */
			tasklet_schedule(&host->finish_tasklet);
			return;
		}

		data.blksz = mrq->cmd->data->blksz;
		data.blocks = mrq->cmd->data->blocks;
		data.blk_addr = mrq->cmd->data->blk_addr;
		if(mrq->cmd->data->flags & MMC_DATA_WRITE)
			data.flags = TCC_SC_MMC_DATA_WRITE;
		else
			data.flags = TCC_SC_MMC_DATA_READ;

		if((sg_cnt == 1) && (host->bounce_buffer != NULL)) {
			data.sg = &sg;
			sg_init_one(data.sg, host->bounce_buffer, data.blksz * data.blocks);
			sg_dma_address(data.sg) = host->bounce_addr;
			sg_dma_len(data.sg) = data.blksz * data.blocks;
		} else {
			data.sg = mrq->cmd->data->sg;
		}
		data.sg_count = mrq->cmd->data->sg_count;
		data.sg_len = mrq->cmd->data->sg_len;

		ret = handle->ops.mmc_ops->request_command(handle, &cmd, &data);
	} else {
		ret = handle->ops.mmc_ops->request_command(handle, &cmd, NULL);
	}

	timeout = jiffies;
	if (!mrq->cmd->data && mrq->cmd->busy_timeout > 9000)
		timeout += DIV_ROUND_UP(mrq->cmd->busy_timeout, 1000) * HZ + HZ;
	else
		timeout += 10 * HZ;
	mod_timer(&host->timer, timeout);

	if(ret) {
		mrq->cmd->error = ret;
	} else {
		mrq->cmd->resp[0] = cmd.resp[0];
		mrq->cmd->resp[1] = cmd.resp[1];
		mrq->cmd->resp[2] = cmd.resp[2];
		mrq->cmd->resp[3] = cmd.resp[3];
		mrq->cmd->error = cmd.error;

		if(mrq->cmd->data) {
			mrq->cmd->data->error = data.error;
			if(mrq->cmd->data->error == 0) {
				mrq->cmd->data->bytes_xfered = data.blksz * data.blocks;
			}
		}
	}

	/* Finish request */
	tasklet_schedule(&host->finish_tasklet);

	mmiowb();
}

static void tcc_sc_mmc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
				int err)
{
	struct tcc_sc_mmc_host *host = mmc_priv(mmc);
	struct mmc_data *data = mrq->data;

	if (data->host_cookie != COOKIE_UNMAPPED)
		dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
			     mmc_get_dma_dir(data));

	data->host_cookie = COOKIE_UNMAPPED;
}

static void tcc_sc_mmc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct tcc_sc_mmc_host *host = mmc_priv(mmc);
	struct mmc_data *data;

	mrq->data->host_cookie = COOKIE_UNMAPPED;
	data = mrq->data;

	if (!host->bounce_buffer) {
		tcc_sc_mmc_pre_dma_transfer(host, mrq->data, COOKIE_PRE_MAPPED);
	}
}

void tcc_sc_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	/* do nothing */
}

static const struct mmc_host_ops tcc_sc_mmc_ops = {
	.request	= tcc_sc_mmc_request,
	.pre_req	= tcc_sc_mmc_pre_req,
	.post_req	= tcc_sc_mmc_post_req,
	.set_ios	= tcc_sc_mmc_set_ios,
};

static const struct of_device_id tcc_sc_mmc_of_match_table[] = {
	{ .compatible = "telechips,tcc805x-sc-mmc", .data = NULL},
	{}
};
MODULE_DEVICE_TABLE(of, tcc_sc_mmc_of_match_table);

static int tcc_sc_mmc_allocate_bounce_buffer(struct tcc_sc_mmc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	unsigned int max_blocks;
	unsigned int bounce_size;
	int ret;

	bounce_size = SZ_64K;

	if (mmc->max_req_size < bounce_size)
		bounce_size = mmc->max_req_size;
	max_blocks = bounce_size / 512;

	host->bounce_buffer = devm_kmalloc(mmc->parent,
					   bounce_size,
					   GFP_KERNEL);
	if (!host->bounce_buffer) {
		pr_err("%s: [ERROR][TCC_SC_MMC] Failed to allocate %u bytes for bounce buffer, falling back to single segments\n",
		       mmc_hostname(mmc),
		       bounce_size);
		/*
		 * Exiting with zero here makes sure we proceed with
		 * mmc->max_segs == 1.
		 */
		return 0;
	}

	host->bounce_addr = dma_map_single(mmc->parent,
					   host->bounce_buffer,
					   bounce_size,
					   DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(mmc->parent, host->bounce_addr);
	if (ret)
		/* Again fall back to max_segs == 1 */
		return 0;
	host->bounce_buffer_size = bounce_size;

	/* Lie about this since we're bouncing */
	mmc->max_segs = max_blocks;
	mmc->max_seg_size = bounce_size;
	mmc->max_req_size = bounce_size;

	return 0;
}

static int tcc_sc_mmc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *fw_np;
	const struct tcc_sc_fw_handle *handle;
	struct mmc_host *mmc;
	struct tcc_sc_mmc_host *host;

	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (!fw_np) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] No sc-firmware node\n");
		return -ENODEV;
	}

	handle = tcc_sc_fw_get_handle(fw_np);
	if(!handle) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] Failed to get handle\n");
		return -ENODEV;
	}

	if(handle->ops.mmc_ops->prot_info == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] prot_info callback function is not registered\n");
		return -ENODEV;
	}

	if(handle->ops.mmc_ops->request_command == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] request_command callback function is not registered\n");
		return -ENODEV;
	}
	dev_info(&pdev->dev, "[INFO][TCC_SC_MMC] regitser tcc-sc-mmc\n");

	mmc = mmc_alloc_host(sizeof(struct tcc_sc_mmc_host), &pdev->dev);
	if (!mmc) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] Failed to allocate memory for mmc\n");
		return -ENOMEM;
	}

	host = mmc_priv(mmc);
	host->dev = &pdev->dev;
	host->handle = handle;
	host->mmc = mmc;

	ret = handle->ops.mmc_ops->prot_info(handle, &host->mmc_prot_info);
	if(ret) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] failed to get protocol info\n");
		return -ENODEV;
	}

	if(!handle->ops.mmc_ops->request_command) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] request_command is not registered\n");
		return -ENODEV;
	}

	ret = mmc_of_parse(host->mmc);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] mmc: parsing dt failed (%d)\n", ret);
		return ret;
	}

	host->mmc->caps |= MMC_CAP_CMD23;

	/*
	 * Init tasklets.
	 */
	tasklet_init(&host->finish_tasklet,
		tcc_sc_mmc_tasklet_finish, (unsigned long)host);

	setup_timer(&host->timer, tcc_sc_mmc_timeout_timer, (unsigned long)host);

	mmc->ops = &tcc_sc_mmc_ops;
	mmc->f_min = 100000;
	mmc->ocr_avail |= MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->max_segs = host->mmc_prot_info.max_segs;
	mmc->max_seg_size = host->mmc_prot_info.max_seg_len;
	mmc->max_blk_size = host->mmc_prot_info.blk_size;
	mmc->max_req_size = mmc->max_segs * mmc->max_seg_size;
	mmc->max_blk_count = 65535;
	
	/* Allocate bounce buffer */
	if(mmc->max_segs == 1) {
		ret = tcc_sc_mmc_allocate_bounce_buffer(host);
		if (ret)
			return ret;
	}

	platform_set_drvdata(pdev, host);

	spin_lock_init(&host->lock);

	ret = mmc_add_host(mmc);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_MMC] Failed to add mmc host\n");
	}

	return ret;
}

static int tcc_sc_mmc_remove(struct platform_device *pdev)
{
	return 0;
}

#define TCC_SC_MMC_PMOPS NULL

static struct platform_driver tcc_sc_mmc_driver = {
	.driver		= {
		.name	= "tcc-sc-mmc",
		.pm	= TCC_SC_MMC_PMOPS,
		.of_match_table = tcc_sc_mmc_of_match_table,
	},
	.probe		= tcc_sc_mmc_probe,
	.remove		= tcc_sc_mmc_remove,
};

module_platform_driver(tcc_sc_mmc_driver);

MODULE_DESCRIPTION("Storage Core MMC driver for Telechips");
MODULE_AUTHOR("Telechips Inc.");
MODULE_LICENSE("GPL v2");
