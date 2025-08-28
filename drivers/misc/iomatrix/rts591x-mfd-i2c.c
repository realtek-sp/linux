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

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/iomatrix.h>
#include <linux/regmap.h>

#include "rts591x-regmap.h"

static const struct mfd_cell rts591x_mfd_cells[] = {
	{
		.name = "rts591x-peci",
		.of_compatible = "realtek,rts591x-peci",
	},
};

static const struct regmap_config rts591x_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
};

static int rts591x_mfd_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct rts591x_mfd_dev *mfd_dev;
	int ret;

	mfd_dev = devm_kzalloc(dev, sizeof(*mfd_dev), GFP_KERNEL);
	if (!mfd_dev) {
		return -ENOMEM;
	}

	i2c_set_clientdata(client, mfd_dev);
	mfd_dev->dev = dev;

	mfd_dev->regmap =
		devm_regmap_init_i2c_iomatrix(client, &rts591x_regmap_config);
	if (IS_ERR(mfd_dev->regmap)) {
		ret = PTR_ERR(mfd_dev->regmap);
		dev_err(dev, "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_NONE, rts591x_mfd_cells,
				   ARRAY_SIZE(rts591x_mfd_cells), NULL, 0,
				   NULL);
	if (ret) {
		dev_err(dev, "Failed to add MFD child devices: %d\n", ret);
		return ret;
	}

	dev_info(dev, "RTS591x MFD initialized successfully.\n");
	return 0;
}

static const struct of_device_id rts591x_mfd_i2c_of_match[] = {
	{ .compatible = "realtek,rts591x-mfd-i2c" },
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
