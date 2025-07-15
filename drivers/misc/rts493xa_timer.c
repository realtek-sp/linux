// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * This software is a confidential and proprietary property of Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in
 * whole or in part, of this work and its derivatives without express
 * permission is prohibited.
 *
 * Realtek Semiconductor Corp. reserves the right to update, modify, or
 * discontinue this software at any time without notice. This software is
 * provided "as is" and any express or implied warranties, including, but
 * not limited to, the implied warranties of merchantability and fitness for
 * a particular purpose are disclaimed. In no event shall Realtek
 * Semiconductor Corp. be liable for any direct, indirect, incidental,
 * special, exemplary, or consequential damages (including, but not limited
 * to, procurement of substitute goods or services; loss of use, data, or
 * profits; or business interruption) however caused and on any theory of
 * liability, whether in contract, strict liability, or tort (including
 * negligence or otherwise) arising in any way out of the use of this software,
 * even if advised of the possibility of such damage.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/major.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/sysctl.h>
#include <linux/wait.h>
#include <linux/bcd.h>
#include <linux/seq_file.h>
#include <linux/bitops.h>
#include <linux/compat.h>
#include <linux/cdev.h>
#include <linux/clocksource.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/sched/signal.h>
#include <asm/current.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/div64.h>

#include "rts493xa_timer.h"

#define DRV_NAME "rts493xa_timer"

#define RTS_TIMER_EN	  0x00
#define RTS_TIMER_COMPARE 0x04
#define RTS_TIMER_CURRENT 0x08
#define RTS_TIMER_MODE	  0x0C
#define RTS_TIMER_INT_EN  0x10
#define RTS_TIMER_INT_STS 0x14

#define RTS_TIMER_USER_FREQ (25000)

#define RTS_TIMER_OPEN	   0x0001
#define RTS_TIMER_PERIODIC 0x0002

static u32 rts_timer_frequency;

static unsigned int rts_timer_readreg(struct rts_timer_dev *tdata,
				      unsigned int reg)
{
	return readl(tdata->regmap_base + reg);
}

static int rts_timer_writereg(struct rts_timer_dev *tdata, unsigned int reg,
			      unsigned int value)
{
	writel(value, tdata->regmap_base + reg);

	return 0;
}

static irqreturn_t rts_timer_interrupt(int irq, void *data)
{
	struct rts_timer_dev *devp;
	int v = 0;

	devp = (struct rts_timer_dev *)data;

	if ((devp->hd_flags & RTS_TIMER_OPEN) == 0)
		return IRQ_HANDLED;

	v = rts_timer_readreg(devp, RTS_TIMER_INT_STS);
	if (v & 0x1) {
		rts_timer_writereg(devp, RTS_TIMER_INT_STS, v & 0x1);
	} else { /* not my int */
		return IRQ_HANDLED;
	}

	devp->hd_irqdata++;

	if ((devp->hd_flags & (RTS_TIMER_PERIODIC)) == 0) {
		unsigned long m, t, mc, base, k;

		t = devp->hd_ireqfreq;
		m = rts_timer_readreg(devp, RTS_TIMER_COMPARE);
		mc = rts_timer_readreg(devp, RTS_TIMER_CURRENT);
		/* The time for the next interrupt would logically be t + m,
		 * however, if we are very unlucky and the interrupt is delayed
		 * for longer than t then we will completely miss the next
		 * interrupt if we set t + m and an application will hang.
		 * Therefore we need to make a more complex computation assuming
		 * that there exists a k for which the following is true:
		 * k * t + base < mc + delta
		 * (k + 1) * t + base > mc + delta
		 * where t is the interval in hpet ticks for the given freq,
		 * base is the theoretical start value 0 < base < t,
		 * mc is the main counter value at the time of the interrupt,
		 * delta is the time it takes to write the a value to the
		 * comparator.
		 * k may then be computed as (mc - base + delta) / t .
		 */
		base = mc % t;
		k = (mc - base + devp->hp_delta) / t;
		rts_timer_writereg(devp, RTS_TIMER_COMPARE, t * (k + 1) + base);
	}

	v = rts_timer_readreg(devp, RTS_TIMER_INT_STS);
	if (v & 0x1)
		rts_timer_writereg(devp, RTS_TIMER_INT_STS, v | 0x1);

	wake_up_interruptible(&devp->hd_waitqueue);

	gpio_toogle(devp, invert);

	kill_fasync(&devp->hd_async_queue, SIGIO, POLL_IN);

	return IRQ_HANDLED;
}

static int rts_timer_open(struct inode *inode, struct file *file)
{
	int ret;
	struct rts_timer_dev *devp;

	devp = container_of(inode->i_cdev, struct rts_timer_dev, cdev);
	if (file->f_mode & FMODE_WRITE)
		return -EINVAL;

	if (devp->hd_flags & RTS_TIMER_OPEN) {
		dev_err(&devp->pdev->dev, "timer already taken\n");
		return -EAGAIN;
	}

	file->private_data = devp;
	devp->hd_irqdata = 0;
	devp->hd_flags |= RTS_TIMER_OPEN;

	ret = request_irq(devp->irq, rts_timer_interrupt, IRQF_SHARED, DRV_NAME,
			  (void *)devp);
	if (ret < 0) {
		dev_err(&devp->pdev->dev, "request_irq failed\n");
		return ret;
	}

	return 0;
}

static ssize_t rts_timer_read(struct file *file, char __user *buf, size_t count,
			      loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long data;
	ssize_t retval;
	struct rts_timer_dev *devp;

	devp = file->private_data;
	if (!devp->hd_ireqfreq)
		return -EIO;

	if (count < sizeof(unsigned long))
		return -EINVAL;

	add_wait_queue(&devp->hd_waitqueue, &wait);

	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);

		data = devp->hd_irqdata;
		devp->hd_irqdata = 0;

		if (data)
			break;
		else if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto out;
		} else if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			goto out;
		}
		schedule();
	}

	retval = put_user(data, (unsigned long __user *)buf);
	if (!retval)
		retval = sizeof(unsigned long);
out:
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&devp->hd_waitqueue, &wait);

	return retval;
}

static unsigned int rts_timer_poll(struct file *file, poll_table *wait)
{
	unsigned long v;
	struct rts_timer_dev *devp;

	devp = file->private_data;

	if (!devp->hd_ireqfreq)
		return 0;

	poll_wait(file, &devp->hd_waitqueue, wait);

	v = devp->hd_irqdata;

	if (v != 0)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int rts_timer_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -EIO;
}

static int rts_timer_fasync(int fd, struct file *file, int on)
{
	struct rts_timer_dev *devp;

	devp = file->private_data;

	if (fasync_helper(fd, file, on, &devp->hd_async_queue) >= 0)
		return 0;
	else
		return -EIO;
}

static int rts_timer_release(struct inode *inode, struct file *file)
{
	struct rts_timer_dev *devp;
	int v;

	devp = file->private_data;
	devp->hd_flags = 0;

	/* disable the timer */
	rts_timer_writereg(devp, RTS_TIMER_EN, 0x0);
	v = rts_timer_readreg(devp, RTS_TIMER_INT_EN);
	rts_timer_writereg(devp, RTS_TIMER_INT_EN, v & (~0x1));

	free_irq(devp->irq, devp);

	file->private_data = NULL;
	return 0;
}

/* converts Hz to number of timer ticks */
static inline unsigned long rts_timer_time_div(unsigned long dis)
{
	unsigned long long m = rts_timer_frequency;

	m += (dis >> 1);
	do_div(m, dis);
	return (unsigned long)m;
}

static long rts_timer_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	int err = 0;
	struct rts_timer_dev *devp;
	int v;

	devp = file->private_data;

	switch (cmd) {
	case RTS_TIMER_EPI:
		devp->hd_flags |= RTS_TIMER_PERIODIC;
		rts_timer_writereg(devp, RTS_TIMER_MODE, 0x1);
		break;
	case RTS_TIMER_DPI:
		devp->hd_flags &= ~RTS_TIMER_PERIODIC;
		rts_timer_writereg(devp, RTS_TIMER_MODE, 0x0);
		break;
	case RTS_TIMER_IRQFREQ:
		if (arg > RTS_TIMER_USER_FREQ) {
			err = -EACCES;
			break;
		}

		if (!arg) {
			err = -EINVAL;
			break;
		}

		devp->hd_ireqfreq = rts_timer_time_div(arg);

		break;
	case RTS_TIMER_GO:
		v = rts_timer_readreg(devp, RTS_TIMER_INT_EN);
		rts_timer_writereg(devp, RTS_TIMER_INT_EN, v | 0x1);
		rts_timer_writereg(devp, RTS_TIMER_EN, 0x0);
		rts_timer_writereg(devp, RTS_TIMER_COMPARE, devp->hd_ireqfreq);
		rts_timer_writereg(devp, RTS_TIMER_EN, 0x1);

		break;
	case RTS_TIMER_STOP:
		rts_timer_writereg(devp, RTS_TIMER_EN, 0x0);
		v = rts_timer_readreg(devp, RTS_TIMER_INT_EN);
		rts_timer_writereg(devp, RTS_TIMER_INT_EN, v & (~0x1));
		break;
	default:
		break;
	}

	return err;
}

/*
 * Adjustment for when arming the timer with
 * initial conditions.  That is, main counter
 * ticks expired before interrupts are enabled.
 */
#define TICK_CALIBRATE (1000UL)

static unsigned long __rts_timer_calibrate(struct rts_timer_dev *devp)
{
	unsigned long t, m, count, i, start;

	t = rts_timer_readreg(devp, RTS_TIMER_COMPARE);

	i = 0;
	count = rts_timer_time_div(TICK_CALIBRATE);
	start = rts_timer_readreg(devp, RTS_TIMER_CURRENT);
	do {
		m = rts_timer_readreg(devp, RTS_TIMER_CURRENT);
		rts_timer_writereg(devp, RTS_TIMER_COMPARE,
				   t + m + devp->hp_delta);
	} while (i++, (m - start) < count);

	return (m - start) / i;
}

static unsigned long rts_timer_calibrate(struct rts_timer_dev *devp)
{
	unsigned long ret = ~0UL;
	unsigned long tmp;

	rts_timer_writereg(devp, RTS_TIMER_EN, 0x1);
	for (;;) {
		tmp = __rts_timer_calibrate(devp);
		if (ret <= tmp)
			break;
		ret = tmp;
	}
	rts_timer_writereg(devp, RTS_TIMER_EN, 0x0);

	return ret;
}

static const struct file_operations rts_timer_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = rts_timer_read,
	.poll = rts_timer_poll,
	.unlocked_ioctl = rts_timer_ioctl,
	.compat_ioctl = rts_timer_compat_ioctl,
	.open = rts_timer_open,
	.release = rts_timer_release,
	.fasync = rts_timer_fasync,
	.mmap = rts_timer_mmap,
};

static struct class *rts_timer_class;
static int major, minors;

static int rts_timer_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	static struct rts_timer_dev *rts_timer_device;
	struct device *dev = &pdev->dev;

	rts_timer_device =
		devm_kzalloc(dev, sizeof(struct rts_timer_dev), GFP_KERNEL);
	if (!rts_timer_device)
		return -ENOMEM;

	rts_timer_device->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "unable to get regs address\n");
		return -ENXIO;
	}

	rts_timer_device->regmap_base = devm_ioremap_resource(dev, res);
	if (rts_timer_device->regmap_base)
		dev_info(dev, "Regs maps to %p(%08x)\n",
			 rts_timer_device->regmap_base, res->start);

	rts_timer_device->irq = platform_get_irq(pdev, 0);
	if (rts_timer_device->irq < 0) {
		dev_err(dev, "no IRQ defined\n");
		return -ENODEV;
	}

	if (device_property_read_u32(dev, "clock-frequency",
				     &rts_timer_frequency)) {
		dev_err(dev, "unable to get clock-frequency form dts\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, rts_timer_device);

	rts_timer_device->io = gpio_req(dev);
	if (rts_timer_device->io)
		gpiod_direction_output(rts_timer_device->io, 1);

	init_waitqueue_head(&rts_timer_device->hd_waitqueue);

	rts_timer_device->hp_delta = rts_timer_calibrate(rts_timer_device);

	dev_info(dev, "Calibrate delta:%ld\n", rts_timer_device->hp_delta);
	dev_info(dev, "Using IRQ channel %d\n", rts_timer_device->irq);

	if (!rts_timer_class) {
		rts_timer_class = class_create("rts493xa_timer");
		if (IS_ERR(rts_timer_class)) {
			ret = PTR_ERR(rts_timer_class);
			rts_timer_class = NULL;
			dev_err(dev, "unable to create class %d\n", ret);
			goto unmap_resource;
		}
		ret = alloc_chrdev_region(&rts_timer_device->devt, 0, 2,
					  "RTS TIMER");
		if (ret) {
			dev_err(dev, "alloc_chrdev_region %d\n", ret);
			goto unmap_resource;
		}
		major = MAJOR(rts_timer_device->devt);
	}

	rts_timer_device->devt = MKDEV(major, minors);
	device_create(rts_timer_class, NULL, rts_timer_device->devt, NULL,
		      "rts493xa_timer%d", minors++);

	cdev_init(&rts_timer_device->cdev, &rts_timer_fops);
	rts_timer_device->cdev.owner = THIS_MODULE;
	ret = cdev_add(&rts_timer_device->cdev, rts_timer_device->devt, 1);
	if (ret) {
		pr_err("cdev_add fail\n");
		goto unmap_resource;
	}
	return 0;

unmap_resource:
	device_destroy(rts_timer_class, rts_timer_device->devt);
	iounmap(rts_timer_device->regmap_base);
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int rts_timer_remove(struct platform_device *pdev)
{
	struct device *dev;

	dev = &pdev->dev;

	struct rts_timer_dev *rts_timer_device = platform_get_drvdata(pdev);

	if (rts_timer_device->io)
		gpiod_put(rts_timer_device->io);

	device_destroy(rts_timer_class, rts_timer_device->devt);
	iounmap(rts_timer_device->regmap_base);
	platform_set_drvdata(pdev, NULL);

	if (major) {
		unregister_chrdev_region(MKDEV(major, 0), minors + 1);
		major = minors = 0;
	}
	class_destroy(rts_timer_class);
	rts_timer_class = NULL;
	pr_info("rts493xa_timer driver removed\n");

	return 0;
}

static const struct of_device_id rts_timer_dt_ids[] = {
	{ .compatible = "realtek,rts493xa-timer" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, rts_timer_dt_ids);

static struct platform_driver rts_timer_driver = {
	.probe = rts_timer_probe,
	.remove = rts_timer_remove,
	.suspend        = rts_timer_suspend,
	.resume         = rts_timer_resume,
	.driver = {
		.owner		= THIS_MODULE,
		.name		= DRV_NAME,
		.of_match_table = of_match_ptr(rts_timer_dt_ids),
	},
};

module_platform_driver(rts_timer_driver);

MODULE_ALIAS("platform:rts493xa_timer");
MODULE_DESCRIPTION("RTS493xA Misc Timer Driver");
MODULE_LICENSE("GPL");
