/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */
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

#ifndef __RTS493XA_TIMER_H
#define __RTS493XA_TIMER_H

#include <linux/compiler.h>
#include <linux/of_gpio.h>
#include <linux/rts493xa_timer.h>

struct rts_timer_dev {
	unsigned long hd_ireqfreq;
	unsigned long hd_irqdata;
	wait_queue_head_t hd_waitqueue;
	struct fasync_struct *hd_async_queue;
	unsigned int hd_flags;
	unsigned long hp_delta;
	void __iomem *regmap_base;
	struct platform_device *pdev;
	unsigned int irq;
	struct cdev cdev;
	dev_t devt;
	struct gpio_desc *io;
};

int invert = 1;

#ifdef CONFIG_RTS493XA_BUS_TIMER_TEST
struct gpio_desc *gpio_req(struct device *dev)
{
	return gpiod_get_index(dev, "io", 0, 0);
}

void gpio_toogle(struct rts_timer_dev *devp, int toogle)
{
	gpiod_set_value(devp->io, toogle);
	invert ^= 1;
}
#else /* !CONFIG_RTS493XA_BUS_TIMER_TEST */
struct gpio_desc *gpio_req(struct device *dev)
{
	return 0;
}

void gpio_toogle(struct rts_timer_dev *devp, int toogle)
{
}
#endif

#ifdef CONFIG_COMPAT
rts_timer_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err;

	return err;
}
#else
#define rts_timer_compat_ioctl NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#define rts_timer_suspend NULL
#define rts_timer_resume  NULL
#else
static int rts_timer_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev;

	dev = &pdev->dev;

	dev_info(dev, "rts_timer_suspend successfully.\n");

	return 0;
}

static int rts_timer_resume(struct platform_device *pdev)
{
	struct device *dev;

	dev = &pdev->dev;

	dev_info(dev, "rts_timer_resume successfully.\n");

	return 0;
}
#endif

#endif /* __RTS493XA_timer_H */
