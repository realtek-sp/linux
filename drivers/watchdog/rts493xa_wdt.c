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

#include <linux/module.h> /* For module specific items */
#include <linux/moduleparam.h> /* For new moduleparam's */
#include <linux/types.h> /* For standard types (like size_t) */
#include <linux/errno.h> /* For the -ENODEV/... values */
#include <linux/kernel.h> /* For printk/panic/... */
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/fs.h> /* For file operations */
#include <linux/miscdevice.h>
#include <linux/watchdog.h> /* For the watchdog specific items */
#include <linux/init.h> /* For __init/__exit/... */
#include <linux/platform_device.h> /* For platform_driver framework */
#include <linux/spinlock.h> /* For spin_lock/spin_unlock/... */
#include <linux/uaccess.h> /* For copy_to_user/put_user/... */
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>

#define DRIVER_NAME "rts493xa-wdt"

#define WATCHDOG_CFG_REG  0
#define WATCHDOG_CTL	  0x4
#define WATCHDOG_INT_EN	  0x4
#define WATCHDOG_INT_FLAG 0x4

#define WDOG_TIME_2		 16
#define WDOG_RST_PAD_PUE	 9
#define WDOG_RST_PAD_PDE	 8
#define WDOG_RST_PAD_SR_SLOW	 7
#define WDOG_RST_PAD_DRV_8MA	 6
#define WDOG_RST_PMU_VOLTAGE_3V3 5
#define WDOG_RST_PMU_ENABLE	 4
#define WDOG_TIME		 2
#define WDOG_RST_EN		 1
#define WDOG_EN			 0

#define WATCHDOG_TIMEOUT 8

#define RTS_GETFIELD(val, width, offset) ((val >> offset) & ((1 << width) - 1))
#define RTS_SETFIELD(reg, field, width, offset)      \
	((reg & (~(((1 << width) - 1) << offset))) | \
	 ((field & ((1 << width) - 1)) << offset))

static struct {
	unsigned long inuse;
	spinlock_t io_lock;
} rts_wdt_device;

static void __iomem *wdt_reg;
static int expect_close;
static int timeout = WATCHDOG_TIMEOUT;
static bool nowayout = WATCHDOG_NOWAYOUT;
static int ictype;

enum {
	TYPE_RTS493XA = 1,

	TYPE_FPGA = (1 << 16),
};

#define RTS_SOC_CAM_HW_ID(type) ((type) & 0xff)

static void rts_set_field(void __iomem *reg, unsigned int field,
			  unsigned int width, unsigned int offset)
{
	unsigned int val = readl(reg);

	val = RTS_SETFIELD(val, field, width, offset);
	writel(val, reg);
}

static int rts_wdt_set(int new_timeout)
{
	u8 time;
	unsigned long flag;

	spin_lock_irqsave(&rts_wdt_device.io_lock, flag);

	if (new_timeout >= 64) {
		timeout = 64;
		time = 6;
	} else if (new_timeout >= 32) {
		timeout = 32;
		time = 5;
	} else if (new_timeout >= 16) {
		timeout = 16;
		time = 4;
	} else if (new_timeout >= 8) {
		timeout = 8;
		time = 3;
	} else if (new_timeout >= 4) {
		timeout = 4;
		time = 2;
	} else if (new_timeout >= 2) {
		timeout = 2;
		time = 1;
	} else {
		timeout = 1;
		time = 0;
	}

	rts_set_field(wdt_reg + WATCHDOG_CFG_REG, time, 3, WDOG_TIME_2);

	spin_unlock_irqrestore(&rts_wdt_device.io_lock, flag);

	return 0;
}

static void rts_wdt_start(void)
{
	unsigned long flag;

	rts_wdt_set(timeout);

	spin_lock_irqsave(&rts_wdt_device.io_lock, flag);

#ifdef CONFIG_EXTERNAL_RESET
	rts_set_field(wdt_reg + WATCHDOG_CFG_REG, 1, 1, WDOG_RST_PMU_ENABLE);
#else
	rts_set_field(wdt_reg + WATCHDOG_CFG_REG, 1, 1, WDOG_RST_EN);
#endif
	rts_set_field(wdt_reg + WATCHDOG_CFG_REG, 1, 1, WDOG_EN);

	spin_unlock_irqrestore(&rts_wdt_device.io_lock, flag);
	pr_info("Started watchdog timer\n");
}

static void rts_wdt_stop(void)
{
	unsigned long flag;

	spin_lock_irqsave(&rts_wdt_device.io_lock, flag);

	rts_set_field(wdt_reg + WATCHDOG_CFG_REG, 0, 1, WDOG_EN);

	spin_unlock_irqrestore(&rts_wdt_device.io_lock, flag);
	pr_info("Stopped watchdog timer\n");
}

static void rts_wdt_ping(void)
{
	unsigned long flag;

	spin_lock_irqsave(&rts_wdt_device.io_lock, flag);
	rts_set_field(wdt_reg + WATCHDOG_CTL, 1, 1, 0);
	spin_unlock_irqrestore(&rts_wdt_device.io_lock, flag);
}

static int rts_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &rts_wdt_device.inuse))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	rts_wdt_start();
	rts_wdt_ping();

	return nonseekable_open(inode, file);
}

static int rts_wdt_release(struct inode *inode, struct file *file)
{
	if (expect_close == 42) {
		rts_wdt_stop();
		module_put(THIS_MODULE);
	} else {
		pr_crit("device closed\n");
		rts_wdt_ping();
	}
	clear_bit(0, &rts_wdt_device.inuse);
	return 0;
}

static ssize_t rts_wdt_write(struct file *file, const char *data, size_t len,
			     loff_t *ppos)
{
	if (len) {
		if (!nowayout) {
			size_t i;

			expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;

				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}
		rts_wdt_ping();
		return len;
	}
	return 0;
}

static int wdt_restart_handle(struct notifier_block *this, unsigned long mode,
			      void *cmd)
{
	rts_wdt_set(1);
	rts_wdt_start();

	mdelay(2000);

	pr_emerg("Unable to restart system\n");

	return NOTIFY_DONE;
}

static struct notifier_block wdt_restart_handler = {
	.notifier_call = wdt_restart_handle,
	.priority = 128,
};

static long rts_wdt_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int new_timeout;
	unsigned int value;
	static const struct watchdog_info ident = {
		.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING |
			   WDIOF_MAGICCLOSE,
		.identity = "RTS_WDT Watchdog",
	};
	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user(argp, &ident, sizeof(ident)))
			return -EFAULT;
		break;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		value = 0;
		if (copy_to_user(argp, &value, sizeof(int)))
			return -EFAULT;
		break;
	case WDIOC_SETOPTIONS:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;
		switch (value) {
		case WDIOS_ENABLECARD:
			rts_wdt_start();
			break;
		case WDIOS_DISABLECARD:
			rts_wdt_stop();
			break;
		default:
			return -EINVAL;
		}
		break;
	case WDIOC_KEEPALIVE:
		rts_wdt_ping();
		break;
	case WDIOC_SETTIMEOUT:
		if (copy_from_user(&new_timeout, argp, sizeof(int)))
			return -EFAULT;
		if (rts_wdt_set(new_timeout))
			return -EINVAL;
		break;
	case WDIOC_GETTIMEOUT:
		return copy_to_user(argp, &timeout, sizeof(int));
	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations rts_wdt_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.write = rts_wdt_write,
	.unlocked_ioctl = rts_wdt_ioctl,
	.open = rts_wdt_open,
	.release = rts_wdt_release,
};

static struct miscdevice rts_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &rts_wdt_fops,
};

static const struct of_device_id rts493xa_wd_match[] = {
	{
		.compatible = "realtek,rts493xa-wdt",
		.data = (void *)(TYPE_RTS493XA),
	},
	{}
};
MODULE_DEVICE_TABLE(of, rts493xa_wd_match);

static int rts_wdt_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *r;
	const struct of_device_id *of_id;

	of_id = of_match_device(rts493xa_wd_match, &pdev->dev);

	ictype = RTS_SOC_CAM_HW_ID((int)of_id->data);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		pr_err("failed to retrieve resources\n");
		return -ENODEV;
	}

	wdt_reg = ioremap(r->start, resource_size(r));
	if (!wdt_reg) {
		pr_err("failed to remap I/O resources\n");
		return -ENXIO;
	}

	spin_lock_init(&rts_wdt_device.io_lock);

	rts_wdt_stop();

	if (rts_wdt_set(timeout))
		rts_wdt_set(WATCHDOG_TIMEOUT);

	ret = register_restart_handler(&wdt_restart_handler);
	if (ret) {
		pr_err("cannot register restart handler (err=%d)\n", ret);
		goto err_out_reboot;
	}

	ret = misc_register(&rts_wdt_miscdev);
	if (ret < 0) {
		pr_err("failed to register watchdog device\n");
		goto unmap;
	}

	pr_info("timer margin: %d sec\n", timeout);

	return 0;
err_out_reboot:
	unregister_restart_handler(&wdt_restart_handler);
unmap:
	iounmap(wdt_reg);
	return ret;
}

static int rts_wdt_remove(struct platform_device *pdev)
{
	misc_deregister(&rts_wdt_miscdev);
	unregister_restart_handler(&wdt_restart_handler);
	iounmap(wdt_reg);
	return 0;
}

static void rts_wdt_shutdown(struct platform_device *pdev)
{
	rts_wdt_stop();
}

static struct platform_driver rts_wdt_driver = {
	.probe = rts_wdt_probe,
	.remove = rts_wdt_remove,
	.shutdown = rts_wdt_shutdown,
	.driver = {
		.name = "watchdog-platform",
		.of_match_table = rts493xa_wd_match,
	},
};
module_platform_driver(rts_wdt_driver);
