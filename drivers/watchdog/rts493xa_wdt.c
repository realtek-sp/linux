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

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/units.h>
#include <linux/watchdog.h>

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

#define WDT_DEFAULT_TIMEOUT 8

#define RTS_GETFIELD(val, width, offset) ((val >> offset) & ((1 << width) - 1))
#define RTS_SETFIELD(reg, field, width, offset)      \
	((reg & (~(((1 << width) - 1) << offset))) | \
	 ((field & ((1 << width) - 1)) << offset))

enum rts_wdt_type {
	TYPE_RTS493XA = 1,

	TYPE_FPGA = (1 << 16),
};

struct rts_wdt_priv {
	struct watchdog_device wdd;
	void __iomem *wdt_reg;
	struct clk *clk;
	enum rts_wdt_type devtype;
};

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Disable watchdog shutdown on close");

static void rts_set_field(void __iomem *reg, unsigned int field,
			  unsigned int width, unsigned int offset)
{
	unsigned int val = readl(reg);

	val = RTS_SETFIELD(val, field, width, offset);
	writel(val, reg);
}

static int rts_wdt_start(struct watchdog_device *wdd)
{
	struct rts_wdt_priv *priv = watchdog_get_drvdata(wdd);

#ifdef CONFIG_EXTERNAL_RESET
	rts_set_field(wdt_reg + WATCHDOG_CFG_REG, 1, 1, WDOG_RST_PMU_ENABLE);
#else
	rts_set_field(priv->wdt_reg + WATCHDOG_CFG_REG, 1, 1, WDOG_RST_EN);
#endif
	rts_set_field(priv->wdt_reg + WATCHDOG_CFG_REG, 1, 1, WDOG_EN);

	pr_info("Started watchdog timer\n");

	return 0;
}

static int rts_wdt_stop(struct watchdog_device *wdd)
{
	struct rts_wdt_priv *priv = watchdog_get_drvdata(wdd);

	rts_set_field(priv->wdt_reg + WATCHDOG_CFG_REG, 0, 1, WDOG_EN);

	pr_info("Stopped watchdog timer\n");

	return 0;
}

static int rts_wdt_ping(struct watchdog_device *wdd)
{
	struct rts_wdt_priv *priv = watchdog_get_drvdata(wdd);

	rts_set_field(priv->wdt_reg + WATCHDOG_CTL, 1, 1, 0);

	return 0;
}

static int rts_wdt_set_timeout(struct watchdog_device *wdd,
			       unsigned int timeout)
{
	struct rts_wdt_priv *priv = watchdog_get_drvdata(wdd);
	u8 time;

	if (timeout >= 64)
		time = 6;
	else if (timeout >= 32)
		time = 5;
	else if (timeout >= 16)
		time = 4;
	else if (timeout >= 8)
		time = 3;
	else if (timeout >= 4)
		time = 2;
	else if (timeout >= 2)
		time = 1;
	else
		time = 0;

	wdd->timeout = timeout;
	wdd->max_hw_heartbeat_ms = (1 << time) * 1000 / 2;

	rts_set_field(priv->wdt_reg + WATCHDOG_CFG_REG, time, 3, WDOG_TIME_2);

	return 0;
}

static int rts_wdt_restart(struct watchdog_device *wdd, unsigned long action,
			   void *data)
{
	rts_wdt_set_timeout(wdd, 1);
	rts_wdt_start(wdd);

	mdelay(2000);

	pr_emerg("Unable to restart system\n");

	return 0;
}

static const struct watchdog_info rts_wdt_ident = {
	.options = WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT,
	.identity = "RTS_WDT Watchdog",
};

static const struct watchdog_ops rts_wdt_ops = {
	.owner = THIS_MODULE,
	.start = rts_wdt_start,
	.stop = rts_wdt_stop,
	.ping = rts_wdt_ping,
	.set_timeout = rts_wdt_set_timeout,
	.restart = rts_wdt_restart,
};

static int rts_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rts_wdt_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->wdt_reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->wdt_reg))
		return PTR_ERR(priv->wdt_reg);

	priv->devtype = (uintptr_t)of_device_get_match_data(dev);

	priv->wdd.info = &rts_wdt_ident;
	priv->wdd.ops = &rts_wdt_ops;
	priv->wdd.parent = dev;
	priv->wdd.min_timeout = 1;
	priv->wdd.max_timeout = 64;
	priv->wdd.max_hw_heartbeat_ms = 64 * 1000;
	priv->wdd.timeout = WDT_DEFAULT_TIMEOUT;

	watchdog_set_drvdata(&priv->wdd, priv);
	watchdog_set_nowayout(&priv->wdd, nowayout);
	watchdog_stop_on_unregister(&priv->wdd);

	rts_wdt_stop(&priv->wdd);
	rts_wdt_set_timeout(&priv->wdd, WDT_DEFAULT_TIMEOUT);

	ret = devm_watchdog_register_device(dev, &priv->wdd);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);

	return 0;
}

static const struct of_device_id rts493xa_wdt_match[] = {
	{
		.compatible = "realtek,rts493xa-wdt",
		.data = (void *)(TYPE_RTS493XA),
	},
	{}
};
MODULE_DEVICE_TABLE(of, rts493xa_wdt_match);

static struct platform_driver rts_wdt_driver = {
	.probe = rts_wdt_probe,
	.driver = {
		.name = "watchdog-platform",
		.of_match_table = rts493xa_wdt_match,
	},
};
module_platform_driver(rts_wdt_driver);

MODULE_DESCRIPTION("RTS493xA Watchdog Driver");
MODULE_LICENSE("GPL");
