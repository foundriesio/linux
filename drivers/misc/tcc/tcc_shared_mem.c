// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/cdev.h>		//struct cdev
#include <linux/delay.h>	// for test
#include <linux/tcc_shmem.h>

// this should be moved into linked list because critical section
// should be seperaged by channel
DEFINE_SPINLOCK(tcc_shm_spinlock1);
DEFINE_SPINLOCK(tcc_shm_spinlock2);
DEFINE_SPINLOCK(tcc_shm_spinlock3);

static uint32_t tcc_shm_valid;
struct timer_list shm_timer;

static LIST_HEAD(tcc_shm_list);
static LIST_HEAD(tcc_shm_req_name);

dev_t tcc_shmem_devt;
static struct cdev tcc_shmem_cdev;

static struct tasklet_struct tcc_shm_tasklet;
static struct tasklet_struct tcc_shm_port_req_tasklet;

static int tcc_shmem_open(struct inode *inode, struct file *file);
static long tcc_shmem_ioctl(struct file *flip, unsigned int cmd,
			    unsigned long arg);
static int32_t tcc_shmem_release(struct inode *inode, struct file *file);
static void tcc_shmem_initial_reset(struct device *dev);
static void tcc_shmem_reset(struct device *dev);
static int32_t tcc_shmem_receive_port(struct device *dev, int32_t port,
				      uint32_t size, char *data);
static int32_t tcc_shmem_create_desc_handler(struct device *dev);
static int32_t tcc_shmem_del_desc_handler(struct device *dev);
static int32_t tcc_shmem_create_desc_req(struct device *dev, char *name,
					 uint32_t size);

const static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = tcc_shmem_open,
	.unlocked_ioctl = tcc_shmem_ioctl,
	.release = tcc_shmem_release,
};

static void tcc_shmem_timer(unsigned long args)
{
	struct tcc_shm_data *shdev = (struct tcc_shm_data *)args;

	if (tcc_shm_valid != 1) {
		pr_err("%s: sync mode failed. initial reset!\n", __func__);
		tcc_shmem_initial_reset(shdev->dev);
	}

}

static void insert_pending_read_que(uint32_t read_req,
	struct tcc_shm_data *shdev)
{
	uint32_t i, j, *que_pos, *que_next_pos, port_exist;
	int32_t *read_que;

	read_que = shdev->read_que;
	que_pos = &shdev->que_pos;
	que_next_pos = &shdev->que_next_pos;

	for (i = 0; i < 32; i++) {

		if (read_req & (1 << i)) {

			if (*que_next_pos > *que_pos) {

				for (j = *que_pos; j < *que_next_pos; j++) {
					if (read_que[j] == i) {
						port_exist = 1;
						break;
					}
				}

			} else if (*que_next_pos < *que_pos) {

				for (j = *que_pos; j < 32; j++) {
					if (read_que[j] == i) {
						port_exist = 1;
						break;
					}
				}

				for (j = 0; j < *que_next_pos; j++) {
					if (read_que[j] == i) {
						port_exist = 1;
						break;
					}
				}

			}

			if (port_exist != 1) {

				if (*que_next_pos == 31) {
					read_que[*que_next_pos] = i;
					*que_next_pos = 0;
				} else {
					read_que[*que_next_pos] = i;
					*que_next_pos += 1;
				}

			}

		}
	}

	shdev->read_req = 1;

}

static irqreturn_t tcc_shmem_handler(int irq, void *dev)
{
	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	void __iomem *a72_base = shdev->base + TCC_SHM_A72_REQ_OFFSET;
	void __iomem *a72_read_req = shdev->base + TCC_SHM_A72_READ_REQ_OFFSET;
	void __iomem *a53_base = shdev->base + TCC_SHM_A53_REQ_OFFSET;
	void __iomem *a53_read_req = shdev->base + TCC_SHM_A53_READ_REQ_OFFSET;
	uint32_t val, read_req;

	if (tcc_shm_valid == 0) {
		if (shdev->core_num == 0) {
			val = tcc_shmem_readl(a53_base);
			tcc_shmem_writel(val & ~TCC_SHM_RESET_SYNC_REQ,
					a53_base);
		} else {
			val = tcc_shmem_readl(a72_base);
			tcc_shmem_writel(val & ~TCC_SHM_RESET_SYNC_REQ,
					a72_base);
		}

		if (val & TCC_SHM_ENABLED) {
			if (val & TCC_SHM_RESET_DONE) {
				tcc_shm_valid = 1;

				if (shdev->core_num == 0) {
					tcc_shmem_writel(
					tcc_shmem_readl(a53_base) &
					~TCC_SHM_RESET_DONE, a53_base);
					tcc_shmem_writel(TCC_SHM_ENABLED,
					a72_base);
				} else {
					tcc_shmem_writel(
					tcc_shmem_readl(a72_base) &
					~TCC_SHM_RESET_DONE, a72_base);
					tcc_shmem_writel(TCC_SHM_ENABLED,
					a53_base);
				}

			} else {

				tcc_shm_valid = 0;
				if (val & TCC_SHM_PORT_REQ)
					shdev->port_req = 1;

			}

			mod_timer(&shm_timer,
			jiffies+msecs_to_jiffies(SHM_SYNC_TIMEOUT));

			pr_info("%s: interrupt sync\n", __func__);

		} else {

			shdev->initial_reset_req = 1;
			pr_info("%s: interrupt initial\n", __func__);

		}

	} else {

		if (shdev->core_num == 0) {

			val = tcc_shmem_readl(a53_base);

			read_req = tcc_shmem_readl(a53_read_req);

		} else {

			val = tcc_shmem_readl(a72_base);

			read_req = tcc_shmem_readl(a72_read_req);

		}
		if (val & TCC_SHM_RESET_REQ) {
			if (shdev->core_num == 0) {
				//clear reset flag
				tcc_shmem_writel(val&~TCC_SHM_RESET_REQ,
				a53_base);
			} else {
				//clear reset flag
				tcc_shmem_writel(val&~TCC_SHM_RESET_REQ,
				a72_base);
			}
			shdev->reset_req = 1;
			tcc_shm_valid = 0;
			tasklet_schedule(&tcc_shm_tasklet);
			return IRQ_HANDLED;
		}

		if ((shdev->irq_mode) && (read_req))
			insert_pending_read_que(read_req, shdev);


		if (val & TCC_SHM_PORT_REQ)
			shdev->port_req = 1;

		if (val & TCC_SHM_PORT_DEL_REQ)
			shdev->port_del_req = 1;

	}

	tasklet_schedule(&tcc_shm_tasklet);

	return IRQ_HANDLED;
}

static void tcc_shmem_initial_reset(struct device *dev)
{
	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	void __iomem *a72_base = shdev->base + TCC_SHM_A72_REQ_OFFSET;
	void __iomem *a53_base = shdev->base + TCC_SHM_A53_REQ_OFFSET;

	if (shdev->core_num == 0) {
		tcc_shmem_writel(tcc_shmem_readl(a53_base) & ~TCC_SHM_RESET_REQ,
				    a53_base);

		while (tcc_shmem_readl(a72_base) & TCC_SHM_RESET_SYNC_REQ)
			cpu_relax();

		tcc_shmem_writel(TCC_SHM_ENABLED, a72_base);

	} else {
		tcc_shmem_writel(tcc_shmem_readl(a72_base)&~TCC_SHM_RESET_REQ,
				    a72_base);

		while (tcc_shmem_readl(a53_base) & TCC_SHM_RESET_SYNC_REQ)
			cpu_relax();

		tcc_shmem_writel(TCC_SHM_ENABLED, a53_base);

	}
	shdev->initial_reset_req = 0;
	tcc_shm_valid = 1;

}

static void tcc_shmem_reset(struct device *dev)
{
	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	uint32_t pending_offset = shdev->pending_offset;
	uint32_t bit_place = shdev->bit_place;
	void __iomem *shm_base = shdev->base + TCC_SHM_BASE_OFFSET;
	void __iomem *shm_size_base = shdev->base + TCC_SHM_SIZE_OFFSET;
	void __iomem *shm_port_base = shdev->base + TCC_SHM_PORT_OFFSET;
	void __iomem *shm_name_base = shdev->base + TCC_SHM_NAME_OFFSET;
	void __iomem *shm_name_size = shdev->base + TCC_SHM_NAME_SIZE;
	void __iomem *a53_base = shdev->base + TCC_SHM_A53_REQ_OFFSET;
	void __iomem *a72_base = shdev->base + TCC_SHM_A72_REQ_OFFSET;
	void __iomem *a53_dist_reg = shdev->a53_dist_reg;
	void __iomem *a72_dist_reg = shdev->a72_dist_reg;

	if (shdev->core_num == 0) {
		tcc_shmem_writel(tcc_shmem_readl(a72_base) |
			TCC_SHM_RESET_SYNC_REQ, a72_base);

		tcc_shmem_writel((1 << bit_place),
				 a53_dist_reg + (0x204 +
						(4 * pending_offset)));

		while (tcc_shmem_readl(a72_base) & TCC_SHM_RESET_SYNC_REQ)
			cpu_relax();

	} else {
		tcc_shmem_writel(tcc_shmem_readl(a53_base) |
			TCC_SHM_RESET_SYNC_REQ, a53_base);

		tcc_shmem_writel((1 << bit_place),
				 a72_dist_reg + (0x204 +
						(4 * pending_offset)));

		while (tcc_shmem_readl(a53_base) & TCC_SHM_RESET_SYNC_REQ)
			cpu_relax();

	}

	list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list, list) {

		if (shdev->core_num == 0) {
			tcc_shmem_writel(tcc_shmem_readl(a72_base) |
				TCC_SHM_PORT_REQ, a72_base);
		} else {
			tcc_shmem_writel(tcc_shmem_readl(a53_base) |
				TCC_SHM_PORT_REQ, a53_base);
		}

		tcc_shmem_writel(desc_pos->offset_72,
				 shm_base);
		tcc_shmem_writel(desc_pos->size,
				 shm_size_base);
		tcc_shmem_writel(desc_pos->port_num,
				 shm_port_base);
		memset_io(shm_name_base, 0,
			  TCC_SHM_NAME_SPACE);
		memcpy_toio(shm_name_base, desc_pos->name,
			    strlen(desc_pos->name));
		tcc_shmem_writel(strlen(desc_pos->name),
				 shm_name_size);

		if (shdev->core_num == 0) {
			tcc_shmem_writel((1 << bit_place),
					 a53_dist_reg +
						(0x204 + (4 * pending_offset)));
			while (tcc_shmem_readl(a72_base) & TCC_SHM_PORT_REQ)
				cpu_relax();
		} else {
			tcc_shmem_writel((1 << bit_place),
					 a72_dist_reg +
						(0x204 + (4 * pending_offset)));
			while (tcc_shmem_readl(a53_base) & TCC_SHM_PORT_REQ)
				cpu_relax();
		}

	}


	if (shdev->core_num == 0) {
		tcc_shmem_writel(tcc_shmem_readl(a72_base)|TCC_SHM_RESET_DONE,
				a72_base);
		tcc_shmem_writel((1 << bit_place),
				a53_dist_reg + (0x204 +
					(4 * pending_offset)));
		while (tcc_shmem_readl(a72_base) & TCC_SHM_RESET_DONE)
			cpu_relax();


	} else {

		tcc_shmem_writel(tcc_shmem_readl(a53_base)|TCC_SHM_RESET_DONE,
				a53_base);
		tcc_shmem_writel((1 << bit_place),
				a72_dist_reg + (0x204 +
					(4 * pending_offset)));
		while (tcc_shmem_readl(a53_base) & TCC_SHM_RESET_DONE)
			cpu_relax();
	}

	shdev->reset_req = 0;
	tcc_shm_valid = 1;

}

static void tcc_shmem_tasklet_handler(unsigned long data)
{

	struct tcc_shm_data *shdev = (struct tcc_shm_data *)data;
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	struct device *dev = shdev->dev;
	struct tcc_shm_callback *shm_callback;
	uint32_t port_found = 0, received_num;
	uint32_t *read_req = &shdev->read_req;
	uint32_t *que_pos = &shdev->que_pos;
	uint32_t *que_next_pos = &shdev->que_next_pos;
	int32_t *read_que = shdev->read_que;
	int32_t *initial_reset_req = &shdev->initial_reset_req;
	int32_t *reset_req = &shdev->reset_req;
	int32_t *port_req = &shdev->port_req;
	int32_t *port_del_req = &shdev->port_del_req;
	void __iomem *a53_read_req = shdev->base + TCC_SHM_A53_READ_REQ_OFFSET;
	void __iomem *a72_read_req = shdev->base + TCC_SHM_A72_READ_REQ_OFFSET;
	char *received_data;

	//switch clause needed.
	if (*initial_reset_req != 0) {
		tcc_shmem_initial_reset(shdev->dev);
		pr_info("%s : tcc shared mem. initial reset!!!\n", __func__);
	}

	if (*reset_req != 0) {
		tcc_shmem_reset(shdev->dev);
		pr_info("%s: tcc shared mem. reset!!!\n", __func__);
	}

	if (*port_req != 0) {
		tcc_shmem_create_desc_handler(shdev->dev);
		//pr_info("%s: tcc shared mem. desc. creation\n",
				//__func__);
	}

	if (*port_del_req != 0) {
		tcc_shmem_del_desc_handler(shdev->dev);
		//pr_info("%s: tcc_shared mem. desc. deletion\n", __func__);
	}

	if (*read_req != 0) {
		//pr_info("%s: tcc_shared mem. read request\n", __func__);
		while (!(*que_pos == *que_next_pos)) {
			list_for_each_entry_safe(desc_pos, desc_pos_s,
				&tcc_shm_list, list) {
				//pr_info("%s: desc port num : %d,
				//read port num : %s\n", __func__,
				//desc_pos->port_num,
				//shdev->read_que[shdev->que_pos]);
				if (desc_pos->port_num == read_que[*que_pos]) {
					port_found = 1;
					break;
				}
			}

			shm_callback = &desc_pos->shm_callback;

			if (port_found != 1) {
				*read_req = 0;
				pr_err("%s: no port exist!_0\n", __func__);
				return;
			}

			received_data = devm_kzalloc(dev, sizeof(char) *
							SHM_RECV_BUF_NUM,
							GFP_KERNEL);
			do {
				received_num = tcc_shmem_receive_port(dev,
						    desc_pos->port_num,
						    SHM_RECV_BUF_NUM,
						    received_data);
				if (received_num < 0)
					pr_err("%s: no port_exist!_1\n",
						    __func__);

				if (shm_callback->callback_func) {
#if 0
					pr_info("%s: callback executed\n",
					__func__);
					for (test = 0; test < received_num;
					test++) {
						pr_info("%x",
						    received_data[test]);
					}
					pr_info("len: %d\n\n", received_num);
#endif
					if (received_num > 0) {
						shm_callback->callback_func(
						shm_callback->data,
						 received_data,
						 received_num);
					}

				} else {
					pr_err("%s: callback no exist\n",
						__func__);
				}

			} while (received_num);

			if (shdev->core_num == 0) {
				tcc_shmem_writel((tcc_shmem_readl(a53_read_req)
							& ~(1 <<
							desc_pos->port_num)),
							a53_read_req);
			} else {
				tcc_shmem_writel((tcc_shmem_readl(a72_read_req)
							& ~(1 <<
							desc_pos->port_num)),
							a72_read_req);
			}

			if (shdev->que_pos == 31)
				shdev->que_pos = 0;
			else
				shdev->que_pos += 1;

			devm_kfree(dev, received_data);
		}

		shdev->read_req = 0;
	}

}

static void tcc_shmem_port_req_tasklet_handler(unsigned long data)
{

	struct tcc_shm_data *shdev = (struct tcc_shm_data *)data;
	struct tcc_shm_req_port *req_port_pos, *req_port_pos_s;
	struct device *dev = shdev->dev;

	if (shdev->core_num == 0) {	//only maincore create and request port
		list_for_each_entry_safe(req_port_pos, req_port_pos_s,
					 &tcc_shm_req_name, list) {
			tcc_shmem_create_desc_req(dev, req_port_pos->name,
						  req_port_pos->size);
			kfree(req_port_pos->name);
			list_del(&req_port_pos->list);
			kfree(req_port_pos);
		}
	}

}

int32_t tcc_shmem_register_callback(int32_t port,
				    struct tcc_shm_callback shm_callback)
{

	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	uint32_t port_found = 0;

	if (tcc_shm_valid != 0) {

		list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list,
					 list) {
			if (desc_pos->port_num == port) {
				port_found = 1;
				break;
			}
		}

		if (port_found != 1) {
			pr_err("%s: no port exist!\n", __func__);
			return -1;
		}

		if (shm_callback.callback_func != NULL) {
			desc_pos->shm_callback.callback_func =
			    shm_callback.callback_func;
		} else {
			return -1;
			pr_err("%s: no callback function\n", __func__);
		}

		if (shm_callback.data)
			desc_pos->shm_callback.data = shm_callback.data;
		else {
			return -1;
			pr_err("%s: no callback data\n", __func__);
		}

		return 0;
	}

	pr_err("shared memory is not ready\n");
	return -1;
}
EXPORT_SYMBOL(tcc_shmem_register_callback);

uint32_t tcc_shmem_is_valid(void)
{

	uint32_t i = 0, valid = 0;

	while (i < 1000) {
		if (tcc_shm_valid != 0) {
			valid = 1;
			break;
		}
		mdelay(10);
		i++;
	}

	if (valid != 0)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(tcc_shmem_is_valid);

static int tcc_shmem_open(struct inode *inode, struct file *file)
{

	pr_info("TCC shared memory device open...!!!\n");
	return 0;

}

static int tcc_shmem_release(struct inode *inode, struct file *file)
{
	pr_info("TCC shared memory device close... !!!\n");
	return 0;

}

static long tcc_shmem_ioctl(struct file *flip, unsigned int cmd,
			    unsigned long arg)
{

	int ret = 0;

	switch (cmd) {

	case TCC_SHMEM_CREATE_PORT:
		pr_info("%s: tcc_shmem create port ioctl\n", __func__);
		break;

	default:
		pr_info("%s: tcc shmem default ioctl\n", __func__);

	}

	return ret;

}

static int32_t tcc_shmem_receive_port(struct device *dev, int32_t port,
				      uint32_t size, char *data)
{
	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	uint32_t head, tail, post_tail, cnt, diff, left_cnt, port_found = 0;

	if (tcc_shm_valid != 0) {

		//spin_lock_irq(&tcc_shm_spinlock1);

		list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list,
					 list) {
			if (desc_pos->port_num == port) {
				port_found = 1;
				break;
			}
		}

		if (port_found != 1) {
			pr_err("%s: no port exists!\n", __func__);
			//spin_unlock_irq(&tcc_shm_spinlock1);
			return -1;
		}

		if (shdev->core_num == 0) {
			memcpy_fromio(&head,
				      shdev->base + desc_pos->offset_72 +
				      desc_pos->size + HEAD_OFFSET_53,
				      sizeof(uint32_t));
			memcpy_fromio(&tail,
				      shdev->base + desc_pos->offset_72 +
				      desc_pos->size + TAIL_OFFSET_53,
				      sizeof(uint32_t));
			//pr_info("%s:1 head : 0x%x tail : 0x%x addr : 0x%px\n",
			//__func__, head, tail,
			//shdev->base+desc_pos->offset_72
			//+desc_pos->size+TAIL_OFFSET_53);
		} else {
			memcpy_fromio(&head,
				      shdev->base + desc_pos->offset_72 +
				      desc_pos->size + HEAD_OFFSET_72,
				      sizeof(uint32_t));
			memcpy_fromio(&tail,
				      shdev->base + desc_pos->offset_72 +
				      desc_pos->size + TAIL_OFFSET_72,
				      sizeof(uint32_t));
			//pr_info("%s:2 head : 0x%x tail : 0x%x addr : 0x%px\n",
			//__func__, head, tail,
			//shdev->base+desc_pos->offset_72
			//+desc_pos->size+TAIL_OFFSET_72);
		}

		if (head >= tail) {
			diff = head - tail;
			if (diff < size)
				size = diff;
		} else {
			diff = ((desc_pos->size + 1) / 2) - tail;
			diff += head;
			if (diff < size)
				size = diff;
		}

		post_tail = tail + size;
		//pr_info("%s: post tail : 0x%x size : %d\n",
				//__func__, post_tail, size);

		if (post_tail < (((desc_pos->size + 1) / 2))) {
			cnt = size;
			if (shdev->core_num == 0) {
				memcpy_fromio(data,
					      shdev->base +
					      desc_pos->offset_53 + tail, cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + TAIL_OFFSET_53,
					    &post_tail, sizeof(uint32_t));
			} else {
				memcpy_fromio(data,
					      shdev->base +
					      desc_pos->offset_72 + tail, cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + TAIL_OFFSET_72,
					    &post_tail, sizeof(uint32_t));
			}
		} else {
			cnt = (((desc_pos->size + 1) / 2)) - tail;
			left_cnt = post_tail - (((desc_pos->size + 1) / 2));
			post_tail = left_cnt;
			if (shdev->core_num == 0) {
				memcpy_fromio(data,
					      shdev->base +
					      desc_pos->offset_53 + tail, cnt);
				memcpy_fromio(data + cnt,
					      shdev->base + desc_pos->offset_53,
					      left_cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + TAIL_OFFSET_53,
					    &post_tail, sizeof(uint32_t));
			} else {
				memcpy_fromio(data,
					      shdev->base +
					      desc_pos->offset_72 + tail, cnt);
				memcpy_fromio(data + cnt,
					      shdev->base + desc_pos->offset_72,
					      left_cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + TAIL_OFFSET_72,
					    &post_tail, sizeof(uint32_t));
			}
		}
		//spin_unlock_irq(&tcc_shm_spinlock1);
		return size;
	}

	pr_err("shared memory is not ready\n");
	return -1;

}

int32_t tcc_shmem_transfer_port_nodev(int32_t port, uint32_t size, char *data)
{
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	uint32_t head, post_head, cnt, left_cnt, port_found = 0;

	if (tcc_shm_valid != 0) {

		spin_lock_irq(&tcc_shm_spinlock2);

		list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list,
					 list) {
			if (desc_pos->port_num == port) {
				port_found = 1;
				break;
			}
		}

		if (port_found != 1) {
			pr_err("%s: no port exists!\n", __func__);
			spin_unlock_irq(&tcc_shm_spinlock2);
			return -1;
		}

		if (desc_pos->core_num == 0) {
			memcpy_fromio(&head,
				      desc_pos->base + desc_pos->offset_72 +
				      desc_pos->size + HEAD_OFFSET_72,
				      sizeof(uint32_t));
		} else {
			memcpy_fromio(&head,
				      desc_pos->base + desc_pos->offset_72 +
				      desc_pos->size + HEAD_OFFSET_53,
				      sizeof(uint32_t));
		}

		post_head = head + size;

		if (post_head < (((desc_pos->size + 1) / 2))) {
			cnt = size;

			//pr_info("####%s : leng0 : %d post_head : 0x%x\n"
			//, __func__, size, post_head);
			//pr_info("####head : 0x%x\n", head);
			if (desc_pos->core_num == 0) {
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_72 + head, data,
					    cnt);
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_72,
					    &post_head, sizeof(uint32_t));
				memcpy_fromio(&head,
					      desc_pos->base +
					      desc_pos->offset_72 +
					      desc_pos->size + HEAD_OFFSET_72,
					      sizeof(uint32_t));
			} else {
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_53 + head, data,
					    cnt);
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_53,
					    &post_head, sizeof(uint32_t));
				memcpy_fromio(&head,
					      desc_pos->base +
					      desc_pos->offset_72 +
					      desc_pos->size + HEAD_OFFSET_53,
					      sizeof(uint32_t));
			}
			//pr_info("####%s : head0 : 0x%x\n", __func__, head);
		} else {
			cnt = (((desc_pos->size + 1) / 2)) - head;
			left_cnt = post_head - (((desc_pos->size + 1) / 2));

			if (left_cnt > (((desc_pos->size + 1) / 2))) {
				//pr_info(("%s: size of packet is too big to",
				//__func__);
				//pr_info(transfer through shared memory.);
				//pr_info(increase size of shared memory\n);",
				spin_unlock_irq(&tcc_shm_spinlock2);
				return -1;
			}

			post_head = left_cnt;

			//pr_info("####%s : leng1 : %d post_head : 0x%x\n",
			//__func__, cnt+left_cnt, post_head);
			//pr_info("###%s : head : 0x%x\n", __func__, head);

			if (desc_pos->core_num == 0) {
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_72 + head, data,
					    cnt);
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_72, data + cnt,
					    left_cnt);
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_72,
					    &post_head, sizeof(uint32_t));
				memcpy_fromio(&head,
					      desc_pos->base +
					      desc_pos->offset_72 +
					      desc_pos->size + HEAD_OFFSET_72,
					      sizeof(uint32_t));
			} else {
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_53 + head, data,
					    cnt);
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_53, data + cnt,
					    left_cnt);
				memcpy_toio(desc_pos->base +
					    desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_53,
					    &post_head, sizeof(uint32_t));
				memcpy_fromio(&head,
					      desc_pos->base +
					      desc_pos->offset_72 +
					      desc_pos->size + HEAD_OFFSET_53,
					      sizeof(uint32_t));
			}
			//pr_info("###################%s : head1 : 0x%x\n",
			//__func__, head);
		}

		if (desc_pos->core_num == 0) {
			tcc_shmem_writel((1 << port),
					 desc_pos->base +
						TCC_SHM_A72_READ_REQ_OFFSET);
			tcc_shmem_writel((1 << desc_pos->bit_place),
					 desc_pos->a53_dist_reg +
				    (0x204 + (4 * desc_pos->pending_offset)));
		} else {
			tcc_shmem_writel((1 << port),
					 desc_pos->base +
						TCC_SHM_A53_READ_REQ_OFFSET);
			tcc_shmem_writel((1 << desc_pos->bit_place),
					 desc_pos->a72_dist_reg +
				    (0x204 + (4 * desc_pos->pending_offset)));
		}

		spin_unlock_irq(&tcc_shm_spinlock2);
		return 0;

	} else {
		pr_err("shared memory is not ready\n");
		return -1;
	}

}
EXPORT_SYMBOL(tcc_shmem_transfer_port_nodev);

#if 0
static int32_t tcc_shmem_transfer_port(struct device *dev, int32_t port,
				       uint32_t size, char *data)
{
	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	uint32_t head, post_head, cnt, left_cnt, port_found = 0;

	if (tcc_shm_valid != 0) {

		list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list,
					 list) {
			if (desc_pos->port_num == port) {
				//pr_info("%s: name : %s\n", __func__,
					//desc_pos->name);
				port_found = 1;
				break;
			}
		}

		if (port_found != 1) {
			pr_err("%s: no port exists! %d\n", __func__, port);
			return -1;
		}

		if (shdev->core_num == 0) {
			memcpy_fromio(&head,
				      shdev->base + desc_pos->offset_72 +
				      desc_pos->size + HEAD_OFFSET_72,
				      sizeof(uint32_t));
		} else {
			memcpy_fromio(&head,
				      shdev->base + desc_pos->offset_72 +
				      desc_pos->size + HEAD_OFFSET_53,
				      sizeof(uint32_t));
		}

		post_head = head + size;

		if (post_head < (((desc_pos->size + 1) / 2))) {
			cnt = size;
			if (shdev->core_num == 0) {
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    head, data, cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_72,
					    &post_head, sizeof(uint32_t));
			} else {
				memcpy_toio(shdev->base + desc_pos->offset_53 +
					    head, data, cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_53,
					    &post_head, sizeof(uint32_t));
			}
		} else {
			cnt = (((desc_pos->size + 1) / 2)) - head;
			left_cnt = post_head - (((desc_pos->size + 1) / 2));
			post_head = left_cnt;
			if (shdev->core_num == 0) {
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    head, data, cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72,
					    data + cnt, left_cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_72,
					    &post_head, sizeof(uint32_t));
			} else {
				memcpy_toio(shdev->base + desc_pos->offset_53 +
					    head, data, cnt);
				memcpy_toio(shdev->base + desc_pos->offset_53,
					    data + cnt, left_cnt);
				memcpy_toio(shdev->base + desc_pos->offset_72 +
					    desc_pos->size + HEAD_OFFSET_53,
					    &post_head, sizeof(uint32_t));
			}
		}
		return 0;

	} else {
		pr_err("shared memory is not ready\n");
		return -1;
	}

}
#endif

static int32_t tcc_shmem_del_desc_handler(struct device *dev)
{

	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	int32_t del_port;

	shdev->port_del_req = 0;

	del_port = tcc_shmem_readl(shdev->base + TCC_SHM_PORT_OFFSET);

	list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list, list) {
		if (desc_pos->port_num == del_port) {
			devm_kfree(dev, desc_pos->name);
			list_del(&desc_pos->list);
			devm_kfree(dev, desc_pos);
		}
	}

	if (shdev->core_num == 0) {
		tcc_shmem_writel(tcc_shmem_readl(shdev->base) &
				 ~TCC_SHM_PORT_DEL_REQ,
				 shdev->base + TCC_SHM_A53_REQ_OFFSET);
	} else {
		tcc_shmem_writel(tcc_shmem_readl(shdev->base) &
				 ~TCC_SHM_PORT_DEL_REQ,
				 shdev->base + TCC_SHM_A72_REQ_OFFSET);
	}

	return 0;
}

static int32_t tcc_shmem_create_desc_handler(struct device *dev)
{

	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	struct tcc_shm_desc *desc_new;
	int32_t new_port;
	uint32_t size, offset, name_size;

	shdev->port_req = 0;

	desc_new = devm_kzalloc(dev, sizeof(struct tcc_shm_desc), GFP_KERNEL);

	if(desc_new == NULL) {
	    return -1;
	}

	new_port = tcc_shmem_readl(shdev->base + TCC_SHM_PORT_OFFSET);
	size = tcc_shmem_readl(shdev->base + TCC_SHM_SIZE_OFFSET);
	offset = tcc_shmem_readl(shdev->base + TCC_SHM_BASE_OFFSET);
	name_size = tcc_shmem_readl(shdev->base + TCC_SHM_NAME_SIZE);
	desc_new->name = devm_kzalloc(dev, name_size, GFP_KERNEL);
	memcpy_fromio(desc_new->name, shdev->base + TCC_SHM_NAME_OFFSET,
		      name_size);

	//pr_info("%s: name : %s size : %d\n", __func__, desc_new->name,
	       //name_size);

	list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list, list) {
		if (desc_pos->port_num == new_port) {
			devm_kfree(dev, desc_new);
			pr_info("%s: port already exist!! port: %d\n", __func__,
			       new_port);
			return 0;
		}
	}

	//pr_info("%s: offset : 0x%x size : %d\n ", __func__, offset, size);

	desc_new->port_num = new_port;
	desc_new->size = size;
	desc_new->offset_72 = offset;
	desc_new->offset_53 = desc_new->offset_72 + ((desc_new->size + 1) / 2);
	//for exported function transfer
	desc_new->core_num = shdev->core_num;
	//for exported function transfer
	desc_new->base = shdev->base;
	//for exported function transfer
	desc_new->bit_place = shdev->bit_place;
	//for exported function transfer
	desc_new->pending_offset = shdev->pending_offset;
	if (shdev->core_num == 0) {
		//for exported function transfer
		desc_new->a53_dist_reg = shdev->a53_dist_reg;
	} else {
		//for exported function transfer
		desc_new->a72_dist_reg = shdev->a72_dist_reg;
	}
	list_add_tail(&desc_new->list, &tcc_shm_list);

	if (shdev->core_num == 0) {
		tcc_shmem_writel(tcc_shmem_readl
				 (shdev->base +
				  TCC_SHM_A53_REQ_OFFSET) & ~TCC_SHM_PORT_REQ,
				 shdev->base + TCC_SHM_A53_REQ_OFFSET);
	} else {
		tcc_shmem_writel(tcc_shmem_readl
				 (shdev->base +
				  TCC_SHM_A72_REQ_OFFSET) & ~TCC_SHM_PORT_REQ,
				 shdev->base + TCC_SHM_A72_REQ_OFFSET);
	}

	//pr_info("%s: new port : %d, size : %d, offset : 0x%x\n", __func__,
	       //desc_new->port_num, desc_new->size, desc_new->offset_72);

	return 0;

}

int32_t tcc_shmem_request_port_by_name(char *name, uint32_t size)
{

	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	struct tcc_shm_req_port *req_port_pos, *req_port_pos_s, *req_port_new;
	uint32_t port_found = 0;

	if (tcc_shm_valid != 0) {

		//pr_info("%s\n", __func__);

		list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list,
					 list) {
			//pr_info("%s: port name %s req name %s\n",
			    //__func__, desc_pos->name, name);
			if (desc_pos->name != NULL) {
				if (strcmp(desc_pos->name, name) == 0) {
					port_found = 1;
					break;
				}
			}
		}

		if (port_found == 0) {
			list_for_each_entry_safe(req_port_pos, req_port_pos_s,
						 &tcc_shm_req_name, list) {
				//pr_info("%s: port name %s req name %s\n",
				//__func__, desc_pos->name, name);
				if (req_port_pos->name != NULL) {
					if (strcmp(req_port_pos->name, name) ==
					    0) {
						port_found = 1;
						break;
					}
				}
			}

			if (port_found == 0) {
				req_port_new =
				    kzalloc(sizeof(struct tcc_shm_req_port),
					    GFP_KERNEL);
				req_port_new->name =
				    kzalloc(strlen(name), GFP_KERNEL);
				memcpy(req_port_new->name, name, strlen(name));
				req_port_new->size = size;
				//pr_info("%s: name : %s size : %d\n",
				//__func__, req_port_new->name,
				//req_port_new->size);
				list_add_tail(&req_port_new->list,
					      &tcc_shm_req_name);
				tasklet_schedule(&tcc_shm_port_req_tasklet);
			} else {
				pr_info("%s: port already requested\n",
				       __func__);
				//port is already requested
				return SHM_PORT_REQUESTED;
			}
		} else {
			pr_info("%s: port already exist\n", __func__);
			return SHM_PORT_EXIST;	// port already exist
		}

		return 0;
	}
	pr_err("shared memory is not ready\n");
	return -1;

}
EXPORT_SYMBOL(tcc_shmem_request_port_by_name);

int32_t tcc_shmem_find_port_by_name(char *name)
{

	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	uint32_t port_found = 0, i = 0;

	if (tcc_shm_valid != 0) {

		while (i < 1000) {
			list_for_each_entry_safe(desc_pos, desc_pos_s,
						 &tcc_shm_list, list) {
				if (desc_pos->name != NULL) {
					if (strcmp(desc_pos->name, name) == 0) {
				    //pr_info("%s: port name %s req name %s\n",
					//__func__, desc_pos->name, name);
						port_found = 1;
						i = 1000;
						break;
					}
				}
			}
			mdelay(10);
			i++;
		}

		if (port_found != 0)
			return desc_pos->port_num;

		pr_err("%s: no port found\n", __func__);
		return -1;
	}

	pr_err("shared memory is not ready\n");
	return -1;

}
EXPORT_SYMBOL(tcc_shmem_find_port_by_name);


static void shmem_port_alloc(int32_t total_port_num,
		    struct tcc_shm_desc *desc_new, struct device *dev)
{

	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	int32_t total_port = total_port_num, temp_port, i, j;
	uint32_t temp_offset;
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	struct tcc_shm_port_list *port_list;
	struct tcc_shm_mem_map *mem_map;
	uint32_t temp_size;
	//pr_info("%s: total port : %d\n",
	//__func__, total_port);

	port_list = devm_kzalloc(dev, sizeof(struct tcc_shm_port_list) *
			 total_port, GFP_KERNEL);
	mem_map = devm_kzalloc(dev, sizeof(struct tcc_shm_mem_map) *
			total_port, GFP_KERNEL);

	total_port = 0;

	list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list, list) {
		port_list[total_port].port_num = desc_pos->port_num;
		mem_map[total_port].port_num = desc_pos->port_num;
		mem_map[total_port].offset = desc_pos->offset_72;
		mem_map[total_port].size = desc_pos->size;
		total_port++;
	}

	for (i = 0; i < total_port - 1; i++) {
		for (j = 0; j < total_port - 1 - i; j++) {
			if (port_list[j].port_num >
			    port_list[j + 1].port_num) {
				temp_port = port_list[j].port_num;
				port_list[j].port_num =
					port_list[j + 1].port_num;
				port_list[j + 1].port_num = temp_port;
			}
		}
	}

	for (i = 0; i < total_port; i++) {
		if (i == 0) {
			if (port_list[i].port_num != 0) {
				desc_new->port_num = 0;
				break;
			}
		} else {
			if (port_list[i].port_num !=
			    (port_list[i - 1].port_num + 1)) {
				desc_new->port_num =
				    port_list[i - 1].port_num + 1;
				break;
			}
		}

		if (i == total_port - 1) {
			desc_new->port_num =
				port_list[i].port_num + 1;
		}
	}

	for (i = 0; i < total_port - 1; i++) {
		for (j = 0; j < total_port - 1 - i; j++) {
			if (mem_map[j].offset > mem_map[j + 1].offset) {
				temp_offset = mem_map[j].offset;
				mem_map[j].offset = mem_map[j + 1].offset;
				mem_map[j + 1].offset = temp_offset;

				temp_port = mem_map[j].port_num;
				mem_map[j].port_num = mem_map[j + 1].port_num;
				mem_map[j + 1].port_num = temp_port;

				temp_size = mem_map[j].size;
				mem_map[j].size = mem_map[j + 1].size;
				mem_map[j + 1].size = temp_size;
			}
		}
	}

#if 0
				pr_info("%s: reg_phy : 0x%x, size : %d\n",
				__func__, shdev->reg_phy, shdev->size);
				for (i = 0; i < total_port; i++) {
					pr_info("%s: memmap offset : 0x%x\n",
					__func__, mem_map[i].offset);
					pr_info("%s: memmap size : 0x%x\n",
					__func__, mem_map[i].size);
					pr_info("%s: port_num : %d\n",
					__func__, mem_map[i].port_num);
				}
#endif
	for (i = 0; i < total_port; i++) {
		if (i == 0) {
			if ((uintptr_t) mem_map[i].offset >
			    TCCSHM_BASE_OFFSET_VAL) {
				// ground memory address :
				// shdev->reg_phy + TCCSHM_BASE_OFFSET_VAL
				if (((uintptr_t)mem_map[i].offset -
				(TCCSHM_BASE_OFFSET_VAL)) >
				(desc_new->size + 0xf)) {
					desc_new->offset_72 =
					    TCCSHM_BASE_OFFSET_VAL;
					desc_new->offset_53 =
					    desc_new->offset_72 +
					    ((desc_new->size + 1) / 2);
					//pr_info("%s: offset set1 : 0x%x\n",
					//__func__, desc_new->offset_72);
					break;
				}
			}
		} else if (((mem_map[i].offset) - (mem_map[i - 1].offset +
			    mem_map[i - 1].size + 0xf)) >
			    (desc_new->size + 0xf)) {
				desc_new->offset_72 =
				    (mem_map[i - 1].offset +
				    mem_map[i - 1].size) + 2 + 0xf;
				// allocate memory just next to previous
				// memory(+1) and HEAD/TAIL buffer(+1).
				// plus '2'.
				// reserve 0xf for tail and head of Tx and Rx.
				// plus '0xf'.
				desc_new->offset_53 =
				    desc_new->offset_72 +
				    ((desc_new->size + 1) / 2);
				//pr_info("%s: offset set 2 : 0x%x\n",
				//__func__, desc_new->offset_72);
				break;
		}

		if (i == total_port - 1) {
			if ((mem_map[i].offset + mem_map[i].size) <
			    shdev->reg_phy + shdev->size) {
				if (((shdev->reg_phy + shdev->size) -
				    ((uintptr_t)mem_map[i].offset +
				    mem_map[i].size + 0xf)) >
				    (desc_new->size + 0xf)) {
					    desc_new->offset_72 =
						(mem_map[i].offset +
						mem_map[i].size) + 2 + 0xf;
				    // allocate memory just next to previous
				    // memory(+1) and HEAD/TAIL buffer(+1).
				    // plus '2'.
				    // reserve 0xf for tail and head of
				    // Tx and Rx. plus '0xf'.
					    desc_new->offset_53 =
						desc_new->offset_72 +
						((desc_new->size + 1) / 2);
				    //pr_info("%s: offset set 3 : 0x%x\n"
				    //, __func__, desc_new->offset_72);
				    //pr_info("%s: offset : 0x%x\n",
				    //__func__, mem_map[i].offset);
				    //pr_info("%s: size : 0x%x\n",
				    //__func__, mem_map[i].size);
					    break;
				    }
			}
		}
	}
	devm_kfree(dev, port_list);
	devm_kfree(dev, mem_map);

}

static int32_t tcc_shmem_create_desc_req(struct device *dev, char *name,
					 uint32_t size)
{

	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	struct tcc_shm_desc *desc_new;
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	int32_t total_port = 0;
	uint32_t val;

	if (tcc_shm_valid != 0) {

		spin_lock_irq(&tcc_shm_spinlock3);

		if (shdev->core_num == 0) {
			val =
			    tcc_shmem_readl(shdev->base +
					    TCC_SHM_A53_REQ_OFFSET);
		} else {
			val =
			    tcc_shmem_readl(shdev->base +
					    TCC_SHM_A72_REQ_OFFSET);
		}

		if ((val & TCC_SHM_PORT_REQ) || (val & TCC_SHM_PORT_DEL_REQ)) {
			pr_info
			("%s: port creation and deletion req. is in progress\n",
			     __func__);
			spin_unlock_irq(&tcc_shm_spinlock3);
			return -1;
		}

		if (shdev->core_num == 0) {
			tcc_shmem_writel(tcc_shmem_readl
					    (shdev->base +
					    TCC_SHM_A72_REQ_OFFSET) |
					    TCC_SHM_PORT_REQ,
					    shdev->base +
					    TCC_SHM_A72_REQ_OFFSET);
		} else {
			tcc_shmem_writel(tcc_shmem_readl
					    (shdev->base +
					    TCC_SHM_A53_REQ_OFFSET) |
					    TCC_SHM_PORT_REQ,
					    shdev->base +
					    TCC_SHM_A53_REQ_OFFSET);
		}

		//only maincore create and request port
		if (shdev->core_num == 0) {

			desc_new =
			    devm_kzalloc(dev, sizeof(struct tcc_shm_desc),
					 GFP_KERNEL);
			desc_new->name =
			    devm_kzalloc(dev, strlen(name), GFP_KERNEL);
			memcpy(desc_new->name, name, strlen(name));
			desc_new->port_num = -1;
			// address starts from '0'. minus '1'.
			desc_new->size = size - 1;

			list_for_each_entry_safe(desc_pos, desc_pos_s,
						 &tcc_shm_list, list) {
				total_port++;
			}

			if (total_port == 0) {
				//initial port
				desc_new->port_num = 0;
				//initial offset
				desc_new->offset_72 = TCCSHM_BASE_OFFSET_VAL;
				desc_new->offset_53 =
				    desc_new->offset_72 +
				    ((desc_new->size + 1) / 2);
			} else
				shmem_port_alloc(total_port, desc_new, dev);

			//for exported function transfer
			desc_new->core_num = shdev->core_num;
			//for exported function transfer
			desc_new->base = shdev->base;
			//for exported function transfer
			desc_new->bit_place = shdev->bit_place;
			//for exported function transfer
			desc_new->pending_offset = shdev->pending_offset;
			if (shdev->core_num == 0) {
				 //for exported function transfer
				desc_new->a53_dist_reg = shdev->a53_dist_reg;
			} else {
				//for exported function transfer
				desc_new->a72_dist_reg = shdev->a72_dist_reg;
			}

			list_add_tail(&desc_new->list, &tcc_shm_list);

			tcc_shmem_writel(desc_new->offset_72,
					 shdev->base + TCC_SHM_BASE_OFFSET);
			tcc_shmem_writel(desc_new->size,
					 shdev->base + TCC_SHM_SIZE_OFFSET);
			tcc_shmem_writel(desc_new->port_num,
					 shdev->base + TCC_SHM_PORT_OFFSET);
			memset_io(shdev->base + TCC_SHM_NAME_OFFSET, 0,
				  TCC_SHM_NAME_SPACE);
			memcpy_toio(shdev->base + TCC_SHM_NAME_OFFSET,
				    desc_new->name, strlen(desc_new->name));
			tcc_shmem_writel(strlen(desc_new->name),
					 shdev->base + TCC_SHM_NAME_SIZE);
			val = 0;
			memcpy_toio(shdev->base + desc_new->offset_72 +
				    desc_new->size + HEAD_OFFSET_72, &val,
				    sizeof(uint32_t));
			memcpy_toio(shdev->base + desc_new->offset_72 +
				    desc_new->size + TAIL_OFFSET_72, &val,
				    sizeof(uint32_t));
			memcpy_toio(shdev->base + desc_new->offset_72 +
				    desc_new->size + HEAD_OFFSET_53, &val,
				    sizeof(uint32_t));
			memcpy_toio(shdev->base + desc_new->offset_72 +
				    desc_new->size + TAIL_OFFSET_53, &val,
				    sizeof(uint32_t));

			//pr_info("%s: new port : %d\n"
			//, __func__, desc_new->port_num);
			//pr_info("%s: size : %d\n",
			//__func__, desc_new->size);
			//pr_info("%s: offset : 0x%x\n",
			//__func__, desc_new->offset_72);
			//pr_info("%s: head: 0x%px\n",
			//__func__, shdev->base+desc_new->
			//offset_72+desc_new->size);

			if (shdev->core_num == 0) {
				tcc_shmem_writel((1 << shdev->bit_place),
						 shdev->a53_dist_reg + (0x204 +
						(4 * shdev->pending_offset)));
			} else {
				tcc_shmem_writel((1 << shdev->bit_place),
						 shdev->a72_dist_reg + (0x204 +
						(4 * shdev->pending_offset)));
			}

			spin_unlock_irq(&tcc_shm_spinlock3);
			return desc_new->port_num;
		}

		spin_unlock_irq(&tcc_shm_spinlock3);
		return -1;
	}

	pr_err("shared memory is not ready\n");
	return -1;
}

static int32_t tcc_shmem_deletion_desc_req(struct device *dev, int32_t port)
{

	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	int32_t val;
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	int32_t ret = 0;

	if (tcc_shm_valid != 0) {
		if (shdev->core_num == 0) {
			val =
			    tcc_shmem_readl(shdev->base +
					    TCC_SHM_A53_REQ_OFFSET);
		} else {
			val =
			    tcc_shmem_readl(shdev->base +
					    TCC_SHM_A72_REQ_OFFSET);
		}

		if ((val & TCC_SHM_PORT_REQ) || (val & TCC_SHM_PORT_DEL_REQ)) {
			pr_info("%s: port deletion req. is processing!\n",
			__func__);
			return -1;
		}

		if (shdev->core_num == 0) {
			tcc_shmem_writel(tcc_shmem_readl
					    (shdev->base +
					    TCC_SHM_A72_REQ_OFFSET) |
					    TCC_SHM_PORT_DEL_REQ,
					    shdev->base +
					    TCC_SHM_A72_REQ_OFFSET);
		} else {
			tcc_shmem_writel(tcc_shmem_readl
					    (shdev->base +
					    TCC_SHM_A53_REQ_OFFSET) |
					    TCC_SHM_PORT_DEL_REQ,
					    shdev->base +
					    TCC_SHM_A53_REQ_OFFSET);
		}

		list_for_each_entry_safe(desc_pos, desc_pos_s, &tcc_shm_list,
					 list) {
			if (desc_pos->port_num == port) {
				devm_kfree(dev, desc_pos->name);
				list_del(&desc_pos->list);
				devm_kfree(dev, desc_pos);
			}
		}

		tcc_shmem_writel(port, shdev->base + TCC_SHM_PORT_OFFSET);

		if (shdev->core_num == 0) {
			tcc_shmem_writel((1 << shdev->bit_place),
					 shdev->a53_dist_reg + (0x204 +
						(4 * shdev->pending_offset)));
		} else {
			tcc_shmem_writel((1 << shdev->bit_place),
					 shdev->a72_dist_reg + (0x204 +
						(4 * shdev->pending_offset)));
		}

		return ret;
	}

	pr_err("shared memory is not ready\n");
	return -1;
}

static ssize_t shm_cre_req(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int val = 0;

	tcc_shmem_request_port_by_name("hello", 4096);
	tcc_shmem_request_port_by_name("you", 4096);
	tcc_shmem_request_port_by_name("bad", 4096);
	val = tcc_shmem_find_port_by_name("bad");

	return sprintf(buf, "value : %d\n", val);
}

static ssize_t shm_del_req(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int32_t ret, input;

	ret = kstrtoint(buf, 10, &input);

	if (ret)
		return ret;

	tcc_shmem_deletion_desc_req(dev, input);

	return count;
}

static ssize_t shm_trans_read(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	char *received_data;
	int32_t ret, received_num;

	received_data =
	    devm_kzalloc(dev, sizeof(char) * SHM_RECV_BUF_NUM, GFP_KERNEL);

	received_num =
	    tcc_shmem_receive_port(dev, 0, SHM_RECV_BUF_NUM, received_data);
	//pr_info("%s\n", received_data);

	ret = sprintf(buf, "received num : %d\n", received_num);

	devm_kfree(dev, received_data);

	return ret;
}

static ssize_t shm_trans_write(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	char *trans_data;

	trans_data = devm_kzalloc(dev, sizeof(char) * count, GFP_KERNEL);

	memcpy(trans_data, buf, count);

	tcc_shmem_transfer_port_nodev(0, count, trans_data);

	devm_kfree(dev, trans_data);

	return count;
}

static DEVICE_ATTR(tcc_shmem_port, 0644, shm_cre_req, shm_del_req);
static DEVICE_ATTR(tcc_shmem_trans, 0644, shm_trans_read,
		   shm_trans_write);

static struct attribute *tcc_shmem_test_attrs[] = {
	&dev_attr_tcc_shmem_trans.attr,
	&dev_attr_tcc_shmem_port.attr,
	NULL,
};

static const struct attribute_group tcc_shmem_test_attr_group = {
	.attrs = tcc_shmem_test_attrs,
};

static int tcc_shmem_probe(struct platform_device *pdev)
{

	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	uint32_t irq = (uint32_t) platform_get_irq(pdev, 0), irq_raw;
	struct tcc_shm_data *shdev;
	struct resource *res;
	uint32_t size;
	void __iomem *reg;
	void __iomem *a72_dist_reg;
	void __iomem *a53_dist_reg;
	uint32_t ret, pending_offset, bit_place;
	uint32_t a72_dist_phy, a72_dist_size;
	uint32_t a53_dist_phy, a53_dist_size;
	uint32_t reg_phy;
	uint32_t core_num, reg_num;

	tcc_shm_valid = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tcc_shmem");

	of_get_property(np, "reg", &reg_num);

	reg_num /= 4u;

	if (reg_num == 2) {
		of_property_read_u32_index(np, "reg", 0, &reg_phy);
		of_property_read_u32_index(np, "reg", 1, &size);
		//pr_info("%s: reg_phy : 0x%x\n", __func__, reg_phy);
	} else if (reg_num == 4) {
		of_property_read_u32_index(np, "reg", 1, &reg_phy);
		of_property_read_u32_index(np, "reg", 3, &size);
		//pr_info("%s: reg_phy : 0x%x size : 0x%x\n",
			    //__func__, reg_phy, size);
	}

	reg = devm_ioremap_nocache(&pdev->dev, reg_phy, size);

	shdev =
	    devm_kzalloc(&pdev->dev, sizeof(struct tcc_shm_data), GFP_KERNEL);
	shdev->base = reg;
	shdev->dev = dev;
	tasklet_init(&tcc_shm_tasklet, tcc_shmem_tasklet_handler,
		     (unsigned long)shdev);

	of_property_read_u32_index(np, "core-num", 0, &core_num);

	of_property_read_u32_index(np, "interrupts", 1, &irq_raw);
	//pr_info("SPI irq num : %d\n", irq_raw);

	if (irq_raw > 479) {
		devm_kfree(&pdev->dev, reg);
		pr_err("%s : over limit of the IRQ number\n", __func__);
		return -1;
	}

	pending_offset = irq_raw / 32;
	bit_place = irq_raw % 32;

	of_property_read_u32_index(np, "dist_regs", 0, &a72_dist_phy);
	of_property_read_u32_index(np, "dist_regs", 1, &a72_dist_size);
	of_property_read_u32_index(np, "dist_regs", 2, &a53_dist_phy);
	of_property_read_u32_index(np, "dist_regs", 3, &a53_dist_size);

	a72_dist_reg = devm_ioremap(&pdev->dev, a72_dist_phy, a72_dist_size);
	a53_dist_reg = devm_ioremap(&pdev->dev, a53_dist_phy, a53_dist_size);

	if ((alloc_chrdev_region(&tcc_shmem_devt, 0, 1, "TCC_SHMEM_DEV")) < 0) {
		pr_err("Cannot allocate major number\n");
		devm_kfree(&pdev->dev, reg);
		return -1;
	}
	//pr_info("Major = %d, Minor = %d\n", MAJOR(tcc_shmem_devt),
	//MINOR(tcc_shmem_devt));

	cdev_init(&tcc_shmem_cdev, &fops);

	if ((cdev_add(&tcc_shmem_cdev, tcc_shmem_devt, 1)) < 0) {
		pr_err("Cannot add the device to the system\n");
		devm_kfree(&pdev->dev, reg);
		return -1;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &tcc_shmem_test_attr_group);

	if (ret != 0)
		sysfs_remove_group(&pdev->dev.kobj, &tcc_shmem_test_attr_group);

	shdev->core_num = core_num;
	shdev->pending_offset = pending_offset;
	shdev->a53_dist_reg = a53_dist_reg;
	shdev->a72_dist_reg = a72_dist_reg;
	shdev->reg_phy = reg_phy;
	shdev->bit_place = bit_place;
	shdev->size = size;
	shdev->irq = irq;

	platform_set_drvdata(pdev, shdev);

	tasklet_init(&tcc_shm_port_req_tasklet,
		     tcc_shmem_port_req_tasklet_handler, (unsigned long)shdev);

	//temp port request and delay for virtual ethernet
	shdev->irq_mode = 1;
	//////////////////////////////////////////////////

	tcc_shmem_writel(0, shdev->base + TCC_SHM_BASE_OFFSET);
	tcc_shmem_writel(0, shdev->base + TCC_SHM_SIZE_OFFSET);
	tcc_shmem_writel(0, shdev->base + TCC_SHM_PORT_OFFSET);
	tcc_shmem_writel(0, shdev->base + TCC_SHM_A72_READ_REQ_OFFSET);
	tcc_shmem_writel(0, shdev->base + TCC_SHM_A53_READ_REQ_OFFSET);
	tcc_shmem_writel(0, shdev->base + TCC_SHM_NAME_OFFSET);
	tcc_shmem_writel(0, shdev->base + TCC_SHM_NAME_SPACE);
	tcc_shmem_writel(0, shdev->base + TCC_SHM_NAME_SIZE);

	if (shdev->core_num == 0) {
		tcc_shmem_writel(TCC_SHM_RESET_REQ,
				 shdev->base + TCC_SHM_A72_REQ_OFFSET);
		tcc_shmem_writel((1 << shdev->bit_place),
				 shdev->a53_dist_reg + (0x204 +
						(4 * shdev->pending_offset)));
	} else {
		tcc_shmem_writel(TCC_SHM_RESET_REQ,
				 shdev->base + TCC_SHM_A53_REQ_OFFSET);
		tcc_shmem_writel((1 << shdev->bit_place),
				 shdev->a72_dist_reg + (0x204 +
						(4 * shdev->pending_offset)));
	}

	init_timer(&shm_timer);
	shm_timer.function = tcc_shmem_timer;
	shm_timer.data = (unsigned long)shdev;

	ret =
	    request_irq(irq, (irq_handler_t) tcc_shmem_handler, IRQF_SHARED,
			"TCC_SHMEM", (void *)&pdev->dev);

	if (ret != SUCCESS) {
		pr_err("irq req. fail: %d irq: %x\n", ret, irq);
		devm_kfree(&pdev->dev, reg);
		return -1;
	} else
		pr_info("irq :%x\n", irq);

	return 0;

}

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_ARCH_TCC805X)

static int32_t tcc_shmem_suspend(struct device *dev)
{
	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	int32_t ret = 0;

	tcc_shm_valid = 0;

	if (shdev == NULL) {
		ret = -EINVAL;
	} else {
		del_timer(&shm_timer);
		tasklet_kill(&tcc_shm_tasklet);
		tasklet_kill(&tcc_shm_port_req_tasklet);
		free_irq(shdev->irq, (void *)dev);
	}

	return ret;
}

static int32_t tcc_shmem_resume(struct device *dev)
{
	struct tcc_shm_data *shdev = dev_get_drvdata(dev);
	struct tcc_shm_desc *desc_pos, *desc_pos_s;
	int32_t ret;

	if (shdev == NULL)
		ret = -EINVAL;
	else
		ret = SUCCESS;

	if (ret == SUCCESS) {
		tasklet_init(&tcc_shm_tasklet, tcc_shmem_tasklet_handler,
				(unsigned long)shdev);

		tasklet_init(&tcc_shm_port_req_tasklet,
				tcc_shmem_port_req_tasklet_handler,
				(unsigned long)shdev);


		tcc_shmem_writel(0, shdev->base + TCC_SHM_BASE_OFFSET);
		tcc_shmem_writel(0, shdev->base + TCC_SHM_SIZE_OFFSET);
		tcc_shmem_writel(0, shdev->base + TCC_SHM_PORT_OFFSET);
		tcc_shmem_writel(0, shdev->base + TCC_SHM_A72_READ_REQ_OFFSET);
		tcc_shmem_writel(0, shdev->base + TCC_SHM_A53_READ_REQ_OFFSET);
		tcc_shmem_writel(0, shdev->base + TCC_SHM_NAME_OFFSET);
		tcc_shmem_writel(0, shdev->base + TCC_SHM_NAME_SPACE);
		tcc_shmem_writel(0, shdev->base + TCC_SHM_NAME_SIZE);

		if (shdev->core_num == 0) {
			tcc_shmem_writel(TCC_SHM_RESET_REQ,
					shdev->base + TCC_SHM_A72_REQ_OFFSET);
			tcc_shmem_writel((1 << shdev->bit_place),
					shdev->a53_dist_reg + (0x204 +
						(4 *
						 shdev->pending_offset)));
		} else {
			tcc_shmem_writel(TCC_SHM_RESET_REQ,
					shdev->base + TCC_SHM_A53_REQ_OFFSET);
			tcc_shmem_writel((1 << shdev->bit_place),
					shdev->a72_dist_reg + (0x204 +
						(4 *
						 shdev->pending_offset)));
		}

		init_timer(&shm_timer);
		shm_timer.function = tcc_shmem_timer;
		shm_timer.data = (unsigned long)shdev;

		ret = request_irq(shdev->irq, (irq_handler_t)tcc_shmem_handler,
				IRQF_SHARED, "TCC_SHMEM", (void *)&shdev->dev);

	}

	if (ret != SUCCESS) {
		tasklet_kill(&tcc_shm_tasklet);
		tasklet_kill(&tcc_shm_port_req_tasklet);
		del_timer(&shm_timer);
		devm_kfree(dev, shdev);

		list_for_each_entry_safe(desc_pos, desc_pos_s,
		    &tcc_shm_list, list) {
			devm_kfree(dev, desc_pos->name);
			list_del(&desc_pos->list);
			devm_kfree(dev, desc_pos);
		}
	}

	return ret;

}

static SIMPLE_DEV_PM_OPS(tcc_shmem_pm_ops, tcc_shmem_suspend, tcc_shmem_resume);
#endif

static const struct of_device_id tcc_shmem_match_table[] = {
	{.compatible = "telechips,tcc_shmem"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_shmem_match_table);

static struct platform_driver tcc_shmem_driver = {
	.probe = tcc_shmem_probe,
	.driver = {
		   .name = "tcc-tcc_shmem",
#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_ARCH_TCC805X)
		   .pm = &tcc_shmem_pm_ops,
#endif
		   .of_match_table = tcc_shmem_match_table,
		   },
};

static int __init tcc_shmem_init(void)
{

	return platform_driver_register(&tcc_shmem_driver);

}

static void __exit tcc_shmem_exit(void)
{
	platform_driver_unregister(&tcc_shmem_driver);
}

arch_initcall(tcc_shmem_init);
module_exit(tcc_shmem_exit);

MODULE_DESCRIPTION("Telechips shared memory driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tcc_shmem");
