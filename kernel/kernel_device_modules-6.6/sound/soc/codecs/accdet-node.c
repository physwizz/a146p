

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author: Kevin Huang <shang-ming.huang@mediatek.com>
 */

#include <linux/cdev.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/nvmem-consumer.h>
#include <linux/init.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/pinctrl/consumer.h>
#include "mt6358-accdet.h"
#include "mt6358.h"

#define ACCDET_AUDIO_DEVNAME "audio"
#define ACCDET_AUDIO_EARJACK_DEVNAME "earjack"

struct char_dev{
	int value;
	dev_t accdet_devno;
	struct class *accdet_class;
	int char_value;
	struct device *dev;
};
static struct device *accdet_earjack = NULL;

static ssize_t state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int accdet_AB = 0;
	return scnprintf(buf, PAGE_SIZE, "%d\n", accdet_AB);
};
static DEVICE_ATTR_RO(state);

static int  cdev_open(struct inode *inode, struct file *file)
{
	printk("cdev open succussfull\n");
	return 0;
}

static int cdev_release(struct inode *inode, struct file *file)
{
	struct char_dev *c_dev = (struct char_dev *)dev_get_drvdata(accdet_earjack);
	device_remove_file(c_dev->dev, &dev_attr_state);
	device_destroy(c_dev->accdet_class, c_dev->accdet_devno);
	class_destroy(c_dev->accdet_class);
	dev_set_drvdata(c_dev->dev,NULL);
	return 0;
}

ssize_t cdev_read(struct file *file, char __user *buff, size_t count, loff_t *loff)
{
	struct char_dev *c_dev = (struct char_dev *)dev_get_drvdata(accdet_earjack);
	c_dev->value = 0;
	return count;
}

ssize_t cdev_write(struct file *file, const char __user *buff, size_t count, loff_t *loff)
{
	struct char_dev *c_dev = (struct char_dev *)dev_get_drvdata(accdet_earjack);
	c_dev->value = 1;
	return count;
}

static struct file_operations chrdev_fops={
	.open = cdev_open,
	.release = cdev_release,
	.read = cdev_read,
	.write = cdev_write,
};

static int __init create_headphone_node_init(void)
{
	int ret = 0;
	struct cdev *cdev =NULL;
	struct char_dev *accdet = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
	if(!accdet)
		return -ENOMEM;

	alloc_chrdev_region(&accdet->accdet_devno, 0, 1, ACCDET_AUDIO_DEVNAME);

	cdev = cdev_alloc();

	memset(cdev,0,sizeof(struct cdev));

	cdev_init(cdev, &chrdev_fops);

	ret = cdev_add(cdev, accdet->accdet_devno,1);

	if(ret)
	{
		return -EINVAL;
	}

	accdet->accdet_class = class_create(ACCDET_AUDIO_DEVNAME);

	//device create
	accdet->dev = device_create(accdet->accdet_class, NULL, accdet->accdet_devno,
		NULL, ACCDET_AUDIO_EARJACK_DEVNAME);

	accdet_earjack = accdet->dev;

	dev_set_drvdata(accdet->dev, accdet);

	//device create file
	ret = device_create_file(accdet->dev, &dev_attr_state);

	return 0;
}

static void __exit create_headphone_node_exit(void)
{
	printk("cdev_exit\n");
	struct char_dev *c_dev = (struct char_dev *)dev_get_drvdata(accdet_earjack);
	device_remove_file(c_dev->dev, &dev_attr_state);
	device_destroy(c_dev->accdet_class, c_dev->accdet_devno);
	class_destroy(c_dev->accdet_class);
	dev_set_drvdata(c_dev->dev, NULL);
}
module_init(create_headphone_node_init);
module_exit(create_headphone_node_exit);

/* Module information */
MODULE_DESCRIPTION("MT6789 ALSA SoC accdet driver");
MODULE_LICENSE("GPL v2");