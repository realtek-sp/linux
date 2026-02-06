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

#include <linux/bits.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/iomatrix.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "rts591x-regmap.h"

struct rts591x_match_data {
	const struct mfd_cell *cells;
	int n_cells;
	const struct regmap_irq_chip *irq_chip;
};

static const struct rts591x_model_pdata rts591x_model_pdata = {
	.model = MODEL_ESCM
};
static const struct rts591x_model_pdata rts591x_model_hpm_pdata = {
	.model = MODEL_HPM
};

static const struct mfd_cell rts591x_mfd_cells[] = {
	{
		.name = "iomatrix-uapi",
		.of_compatible = "realtek,iomatrix-uapi",
		.id = 1,
	},
	{
		.name = "rts591x-peci",
		.of_compatible = "realtek,rts591x-peci",
	},
	{
		.name = "rts591x-gpio",
		.of_compatible = "realtek,rts591x-gpio",
		.platform_data = &rts591x_model_pdata,
		.pdata_size = sizeof(struct rts591x_model_pdata),
		.id = 1,
	},
	{
		.name = "rts591x-i2c0",
		.of_compatible = "realtek,rts591x-i2c",
	},
	{
		.name = "rts591x-i2c1",
		.of_compatible = "realtek,rts591x-i2c",
	},
	{
		.name = "rts591x-i2c3",
		.of_compatible = "realtek,rts591x-i2c",
	},
	{
		.name = "rts591x-i2c4",
		.of_compatible = "realtek,rts591x-i2c",
	},
	{
		.name = "rts591x-adc",
		.of_compatible = "realtek,rts591x-adc",
	},
	{
		.name = "rts591x-pwm-fan",
		.of_compatible = "realtek,rts591x-pwm-fan",
	},
};

static const struct mfd_cell rts591x_mfd_cells_hpm[] = {
	{
		.name = "iomatrix-uapi",
		.of_compatible = "realtek,iomatrix-uapi",
		.id = 2,
	},
	{
		.name = "rts591x-gpio",
		.of_compatible = "realtek,rts591x-gpio",
		.platform_data = &rts591x_model_hpm_pdata,
		.pdata_size = sizeof(struct rts591x_model_pdata),
		.id = 2,
	},
	{
		.name = "rts591x-adc-hpm",
		.of_compatible = "realtek,rts591x-adc",
	}
};

static const struct regmap_config rts591x_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
};

static const struct regmap_irq rts591x_irqs[] = {
	REGMAP_IRQ_REG(RTS591X_I2CSLV_PENDING_READ_INT, 0,
		       RTS591X_I2CSLV_PENDING_READ_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_KCS_IBF_INT, 0, RTS591X_KCS_IBF_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C0_INT, 0, RTS591X_I2C0_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C1_INT, 0, RTS591X_I2C1_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C2_INT, 0, RTS591X_I2C2_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C3_INT, 0, RTS591X_I2C3_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C4_INT, 0, RTS591X_I2C4_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C5_INT, 0, RTS591X_I2C5_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C6_INT, 0, RTS591X_I2C6_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_I2C7_INT, 0, RTS591X_I2C7_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_TACHO0_INT, 0, RTS591X_TACHO0_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_TACHO1_INT, 0, RTS591X_TACHO1_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_TACHO2_INT, 0, RTS591X_TACHO2_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_TACHO3_INT, 0, RTS591X_TACHO3_INT_MASK),
};

static const struct regmap_irq rts591x_hpm_irqs[] = {
	REGMAP_IRQ_REG(RTS591X_TACHO0_INT, 0, RTS591X_TACHO0_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_TACHO1_INT, 0, RTS591X_TACHO1_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_TACHO2_INT, 0, RTS591X_TACHO2_INT_MASK),
	REGMAP_IRQ_REG(RTS591X_TACHO3_INT, 0, RTS591X_TACHO3_INT_MASK),
};

static const struct regmap_irq_chip rts591x_irq_chip = {
	.name = "rts591x_irq",
	.irqs = rts591x_irqs,
	.num_irqs = ARRAY_SIZE(rts591x_irqs),
	.num_regs = 1,
	.status_base = RTS591X_IRQ_STAT_BASE,
	.ack_base = RTS591X_IRQ_STAT_BASE,
	.ack_invert = true,
};

static const struct regmap_irq_chip rts591x_hpm_irq_chip = {
	.name = "rts591x_hpm_irq",
	.irqs = rts591x_hpm_irqs,
	.num_irqs = ARRAY_SIZE(rts591x_hpm_irqs),
	.num_regs = 1,
	.status_base = RTS591X_IRQ_STAT_BASE,
	.ack_base = RTS591X_IRQ_STAT_BASE,
	.ack_invert = true,
};

static const struct rts591x_match_data rts591x_default_data = {
	.cells = rts591x_mfd_cells,
	.n_cells = ARRAY_SIZE(rts591x_mfd_cells),
	.irq_chip = &rts591x_irq_chip,
};

static const struct rts591x_match_data rts591x_hpm_data = {
	.cells = rts591x_mfd_cells_hpm,
	.n_cells = ARRAY_SIZE(rts591x_mfd_cells_hpm),
	.irq_chip = &rts591x_hpm_irq_chip,
};

static int rts591x_mfd_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct rts591x_mfd_dev *mfd_dev;
	struct irq_domain *domain = NULL;
	const struct rts591x_match_data *match_data;
	int ret;

	match_data = device_get_match_data(&client->dev);
	if (!match_data) {
		match_data = &rts591x_default_data;
	}

	mfd_dev = devm_kzalloc(dev, sizeof(*mfd_dev), GFP_KERNEL);
	if (!mfd_dev)
		return -ENOMEM;

	i2c_set_clientdata(client, mfd_dev);
	mfd_dev->dev = dev;

	mfd_dev->regmap =
		devm_regmap_init_i2c_iomatrix(client, &rts591x_regmap_config);
	if (IS_ERR(mfd_dev->regmap)) {
		ret = PTR_ERR(mfd_dev->regmap);
		dev_err(dev, "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	mfd_dev->irq_gpio = devm_gpiod_get_optional(dev, "mfd", GPIOD_IN);
	if (IS_ERR(mfd_dev->irq_gpio))
		return dev_err_probe(dev, PTR_ERR(mfd_dev->irq_gpio),
				     "Failed to request rts591x mfd gpio\n");

	if (mfd_dev->irq_gpio && match_data->irq_chip) {
		ret = devm_regmap_add_irq_chip(
			mfd_dev->dev, mfd_dev->regmap,
			gpiod_to_irq(mfd_dev->irq_gpio),
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, 0,
			match_data->irq_chip, &mfd_dev->irq_data);
		if (ret) {
			dev_err(dev, "Failed to add rts591x_irq_chip %d\n",
				ret);
			return ret;
		}

		domain = regmap_irq_get_domain(mfd_dev->irq_data);
	}

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_NONE, match_data->cells,
				   match_data->n_cells, NULL, 0, domain);
	if (ret) {
		dev_err(dev, "Failed to add MFD child devices: %d\n", ret);
		return ret;
	}

	dev_info(dev, "RTS591x MFD initialized successfully.\n");
	return 0;
}

static const struct of_device_id rts591x_mfd_i2c_of_match[] = {
	{
		.compatible = "realtek,rts591x-mfd-i2c",
		.data = &rts591x_default_data,
	},
	{
		.compatible = "realtek,rts591x-mfd-i2c-hpm",
		.data = &rts591x_hpm_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, rts591x_mfd_i2c_of_match);

static struct i2c_driver rts591x_mfd_driver = {
    .driver = {
        .name = "rts591x-mfd",
        .of_match_table = rts591x_mfd_i2c_of_match,
    },
    .probe = rts591x_mfd_probe,
};

module_i2c_driver(rts591x_mfd_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MFD Parent Driver for RTS591x");
