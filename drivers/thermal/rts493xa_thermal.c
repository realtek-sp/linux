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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/of_device.h>

#include "thermal_core.h"

#define SYS_TM_CTRL	   0x0
#define SYS_TM_CFG0	   0x4
#define SYS_TM_CFG1	   0x8
#define SYS_TM_CFG2	   0xc
#define SYS_TM_CFG3	   0x10
#define SYS_TM_CFG4	   0x14
#define SYS_TM_CFG5	   0x18
#define SYS_TM_CFG6	   0x1c
#define SYS_TM_OUT	   0x20
#define SYS_TM_DEBUG	   0x24
#define SYS_TM_OUT_D	   0x28
#define SYS_TM_CT_HIGH	   0x2c
#define SYS_TM_CT_LOW	   0x30
#define SYS_TM_CT_INT_EN   0x34
#define SYS_TM_CT_INT_FLAG 0x38

#define RTS_MAX_TEMP 0x7cfff
#define RTS_MIN_TEMP 0

#define RTS_HYST_TEMP 500

struct rts_thermal_zone {
	void __iomem *base;
	bool irq_enabled;
	const struct rts_thermal_data *data;
	struct thermal_zone_device *therm_dev;
};

static inline uint32_t rts_thermal_reg_read(struct rts_thermal_zone *zone,
					    uint32_t reg)
{
	return readl(zone->base + reg);
}

static inline void rts_thermal_reg_write(struct rts_thermal_zone *zone,
					 uint32_t reg, uint32_t val)
{
	writel(val, zone->base + reg);
}

u32 temp2code(struct rts_thermal_zone *zone, u32 temp)
{
	return temp * 1024 / 1000;
}

u32 code2temp(struct rts_thermal_zone *zone, u32 code)
{
	u32 temp;
	s32 code_s32;

	if (code & 0x40000) {
		code_s32 = 0 - (s32)((~((code & 0x3ffff) - 1)) & 0x3ffff);
		temp = code_s32 * 1000 / 1024;
	} else {
		temp = code * 1000 / 1024;
	}

	return temp;
}

static int rts_get_temp(struct thermal_zone_device *thermal, int *temp)
{
	struct rts_thermal_zone *pzone = thermal_zone_device_priv(thermal);
	unsigned long code = rts_thermal_reg_read(pzone, SYS_TM_OUT);

	*temp = code2temp(pzone, code);

	/*
	 * FIXME: This is a temporary solution to disable the interrupt when the
	 * temperature is below 0, because low threshold setting minimum value
	 * is 0.
	 */

	if (*temp <= 0 && pzone->irq_enabled) {
		rts_thermal_reg_write(pzone, SYS_TM_CT_INT_EN, 0);
		pzone->irq_enabled = false;
	}

	if (*temp >= 5000 && !pzone->irq_enabled) {
		rts_thermal_reg_write(pzone, SYS_TM_CT_INT_FLAG, 1);
		rts_thermal_reg_write(pzone, SYS_TM_CT_INT_EN, 1);
		pzone->irq_enabled = true;
	}

	return 0;
}

static int rts_set_trips(struct thermal_zone_device *thermal, int low, int high)
{
	struct rts_thermal_zone *pzone = thermal_zone_device_priv(thermal);
	unsigned int dft_low, dft_high;

	dft_low = max(low, RTS_MIN_TEMP + RTS_HYST_TEMP) - RTS_HYST_TEMP;
	dft_high = min(high, RTS_MAX_TEMP - RTS_HYST_TEMP) + RTS_HYST_TEMP;

	dft_low = temp2code(pzone, dft_low);
	dft_high = temp2code(pzone, dft_high);

	rts_thermal_reg_write(pzone, SYS_TM_CT_LOW, dft_low);
	rts_thermal_reg_write(pzone, SYS_TM_CT_HIGH, dft_high);

	if (!pzone->irq_enabled) {
		rts_thermal_reg_write(pzone, SYS_TM_CT_INT_FLAG, 1);
		rts_thermal_reg_write(pzone, SYS_TM_CT_INT_EN, 1);
		pzone->irq_enabled = true;
	}

	return 0;
}

static struct thermal_zone_device_ops rts_thermal_ops = {
	.get_temp = rts_get_temp,
	.set_trips = rts_set_trips,
};

static void rts_enable_thermal(struct rts_thermal_zone *pzone)
{
	rts_thermal_reg_write(pzone, SYS_TM_CTRL, 0x103);
	usleep_range(100, 200);
	rts_thermal_reg_write(pzone, SYS_TM_CTRL, 0x107);
	usleep_range(100, 200);
	rts_thermal_reg_write(pzone, SYS_TM_CTRL, 0x1bf);
	rts_thermal_reg_write(pzone, SYS_TM_CFG2, 0x181);
	usleep_range(5000, 10000);

	rts_thermal_reg_write(pzone, SYS_TM_CT_LOW, RTS_MIN_TEMP);
	rts_thermal_reg_write(pzone, SYS_TM_CT_HIGH, RTS_MAX_TEMP);
}

static void rts_disable_thermal(struct rts_thermal_zone *pzone)
{
	clear_bit(0, pzone->base + SYS_TM_CTRL);
	rts_thermal_reg_write(pzone, SYS_TM_CT_INT_EN, 0);
	rts_thermal_reg_write(pzone, SYS_TM_CT_INT_FLAG, 1);
}

static irqreturn_t rts_thermal_irq_handler(int irq, void *irq_data)
{
	struct rts_thermal_zone *pzone = irq_data;
	int flag;

	flag = rts_thermal_reg_read(pzone, SYS_TM_CT_INT_FLAG);
	if (flag == 0)
		return IRQ_NONE;

	thermal_zone_device_update(pzone->therm_dev, THERMAL_EVENT_UNSPECIFIED);

	rts_thermal_reg_write(pzone, SYS_TM_CT_INT_FLAG, 1);

	return IRQ_HANDLED;
}

static int rts_thermal_probe(struct platform_device *pdev)
{
	struct rts_thermal_zone *pzone;
	int irq, ret;

	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	if (!pzone)
		return -ENOMEM;

	pzone->base = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(pzone->base))
		return PTR_ERR(pzone->base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Get IRQ failed.\n");
		return irq;
	}

	rts_enable_thermal(pzone);

	platform_set_drvdata(pdev, pzone);

	pzone->therm_dev = devm_thermal_of_zone_register(&pdev->dev, 0, pzone,
							 &rts_thermal_ops);
	if (IS_ERR(pzone->therm_dev)) {
		dev_err(&pdev->dev, "Register thermal zone sensor failed.\n");
		ret = PTR_ERR(pzone->therm_dev);
		goto failed;
	}

	if (!strcmp(pzone->therm_dev->type, "ddrc-thermal")) {
		thermal_zone_device_set_policy(pzone->therm_dev, "bang_bang");
	}

	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					rts_thermal_irq_handler, IRQF_ONESHOT,
					"rts_thermal", pzone);
	if (ret) {
		dev_err(&pdev->dev, "Request IRQ failed.\n");
		return ret;
	}

	dev_info(&pdev->dev, "Thermal zone device registered.\n");
failed:
	return ret;
}

static void rts_thermal_remove(struct platform_device *pdev)
{
	struct rts_thermal_zone *pzone = platform_get_drvdata(pdev);

	rts_disable_thermal(pzone);
}

static const struct of_device_id rts_tm_match[] = {
	{
		.compatible = "realtek,rts493xa-thermal",
	},
	{}
};
MODULE_DEVICE_TABLE(of, rts_tm_match);

static struct platform_driver rts_thermal_driver = {
	.driver = {
		.name = "rts-thermal",
		.of_match_table = rts_tm_match,
	},
	.probe = rts_thermal_probe,
	.remove_new = rts_thermal_remove,
};

module_platform_driver(rts_thermal_driver);

MODULE_DESCRIPTION("RTS493xA Thermal Driver");
MODULE_LICENSE("GPL");
