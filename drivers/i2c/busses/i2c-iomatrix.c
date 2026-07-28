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
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/mfd/iomatrix.h>
#include <linux/reset.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/units.h>

#include "i2c-designware-core.h"

static int reg_offset_read(void *ctx, unsigned int reg, unsigned int *val)
{
	struct dw_i2c_dev *dev = ctx;

	return regmap_read(dev->sysmap, dev->base_addr + reg, val);
}

static int reg_offset_write(void *ctx, unsigned int reg, unsigned int val)
{
	struct dw_i2c_dev *dev = ctx;

	return regmap_write(dev->sysmap, dev->base_addr + reg, val);
}

static struct regmap_config cfg = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.reg_read = reg_offset_read,
	.reg_write = reg_offset_write,
	.max_register = DW_IC_COMP_TYPE,
};

static int rts591x_i2c_request_regs(struct dw_i2c_dev *dev)
{
	int ret;
	struct rts591x_mfd_dev *mfd_dev;
	struct regmap *alias_map;

	ret = of_property_read_u32(dev->dev->of_node, "reg", &dev->base_addr);
	if (ret) {
		dev_err(dev->dev, "Failed to get base addr\n");
		return ret;
	}

	mfd_dev = dev_get_drvdata(dev->dev->parent);
	if (!mfd_dev) {
		dev_err(dev->dev, "Failed to get mfd_dev data\n");
		return -EINVAL;
	}

	if (!mfd_dev->regmap) {
		dev_err(dev->dev, "Failed to get regmap\n");
		return -EINVAL;
	}
	dev->sysmap = mfd_dev->regmap;

	alias_map = devm_regmap_init(dev->dev, NULL, dev, &cfg);
	if (IS_ERR(alias_map)) {
		dev_err(dev->dev, "Failed to init alias regmap\n");
		return -EINVAL;
	}
	dev->map = alias_map;

	dev->irq = platform_get_irq(to_platform_device(dev->dev), 0);
	if (dev->irq < 0) {
		dev_err(dev->dev, "Failed to get IRQ %d\n", dev->irq);
		return dev->irq;
	}

	return 0;
}

static int dw_i2c_plat_request_regs(struct dw_i2c_dev *dev)
{
	return rts591x_i2c_request_regs(dev);
}

static int dw_i2c_plat_probe(struct platform_device *pdev)
{
	struct i2c_adapter *adap;
	struct dw_i2c_dev *dev;
	struct i2c_timings *t;
	int ret;

	dev = devm_kzalloc(&pdev->dev, sizeof(struct dw_i2c_dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->flags = (uintptr_t)device_get_match_data(&pdev->dev);

	dev->dev = &pdev->dev;
	platform_set_drvdata(pdev, dev);

	ret = dw_i2c_plat_request_regs(dev);
	if (ret)
		return ret;

	t = &dev->timings;
	i2c_parse_fw_timings(&pdev->dev, t, false);
	i2c_dw_adjust_bus_speed(dev);
	ret = i2c_dw_validate_speed(dev);
	if (ret)
		return ret;

	i2c_dw_configure(dev);

	adap = &dev->adapter;
	adap->owner = THIS_MODULE;
	adap->dev.of_node = pdev->dev.of_node;
	adap->nr = -1;

	ret = i2c_dw_probe(dev);
	if (ret)
		return ret;

	return 0;
}

static void dw_i2c_plat_remove(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev = platform_get_drvdata(pdev);

	i2c_del_adapter(&dev->adapter);

	dev->disable(dev);
}

static const struct of_device_id dw_i2c_of_match[] = {
	{
		.compatible = "realtek,rts591x-i2c",
		.data = (void *)MODEL_RTS591X,
	},
	{},
};
MODULE_DEVICE_TABLE(of, dw_i2c_of_match);

/* Work with hotplug and coldplug */
MODULE_ALIAS("platform:i2c_rts591x");

static struct platform_driver dw_i2c_driver = {
	.probe = dw_i2c_plat_probe,
	.remove_new = dw_i2c_plat_remove,
	.driver		= {
		.name	= "i2c_rts591x",
		.of_match_table = of_match_ptr(dw_i2c_of_match),
	},
};

static int __init dw_i2c_init_driver(void)
{
	return platform_driver_register(&dw_i2c_driver);
}
subsys_initcall(dw_i2c_init_driver);

static void __exit dw_i2c_exit_driver(void)
{
	platform_driver_unregister(&dw_i2c_driver);
}
module_exit(dw_i2c_exit_driver);

MODULE_DESCRIPTION("Synopsys DesignWare I2C bus adapter For IOMatrix");
MODULE_LICENSE("GPL");
