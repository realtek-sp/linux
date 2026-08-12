// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 Realtek Semiconductor Corp. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/peci.h>
#include <linux/mutex.h>
#include <linux/mfd/iomatrix.h>

/* PECI Completion Code: success */
#define PECI_CC_PASS 0x40

struct rts591x_peci_oob {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
};

static int rts591x_peci_oob_xfer(struct peci_controller *controller, u8 addr,
				 struct peci_request *req)
{
	struct rts591x_peci_oob *priv = dev_get_drvdata(controller->dev.parent);
	u8 cmd_buf[3 + PECI_REQUEST_MAX_BUF_SIZE];
	u8 resp_buf[1 + PECI_REQUEST_MAX_BUF_SIZE];
	u32 resp_len = sizeof(resp_buf);
	u32 cmd_len;
	int ret;

	if (!req) {
		dev_err(priv->dev, "req is NULL\n");
		return -EINVAL;
	}

	cmd_len = 3 + req->tx.len;
	if (cmd_len > sizeof(cmd_buf)) {
		dev_err(priv->dev, "command too long: %u\n", cmd_len);
		return -EINVAL;
	}

	cmd_buf[0] = addr;
	cmd_buf[1] = req->tx.len;
	cmd_buf[2] = req->rx.len;
	if (req->tx.len > 0)
		memcpy(cmd_buf + 3, req->tx.buf, req->tx.len);

	mutex_lock(&priv->lock);

	/* Combined send + wait + recv, regmap lock held throughout.
	 * This prevents other drivers from inserting I2C transactions
	 * and corrupting the EC's TX FIFO during OOB PECI processing.
	 */
	ret = iomatrix_regmap_peci_oob(priv->regmap, cmd_buf, cmd_len, resp_buf,
				       &resp_len);
	if (ret) {
		dev_dbg(priv->dev, "OOB transaction failed: %d\n", ret);
		/*
		 * EC returns RSP_ERR (-EPROTO) when the target PECI address is
		 * unpopulated.  Map to -EIO so peci_device_create() treats it
		 * as "no device at this address" and continues scanning rather
		 * than aborting the entire rescan.
		 */
		if (ret == -EPROTO)
			ret = -EIO;
		goto out;
	}

	if (resp_len < 1) {
		dev_err(priv->dev, "empty PECI response\n");
		ret = -EIO;
		goto out;
	}

	if (resp_len > sizeof(resp_buf)) {
		dev_err(priv->dev, "EC returned oversized response: %u\n",
			resp_len);
		ret = -EIO;
		goto out;
	}

	if (resp_buf[0] != PECI_CC_PASS) {
		dev_dbg(priv->dev, "PECI CC: 0x%02x\n", resp_buf[0]);
		ret = -EIO;
		goto out;
	}

	if (resp_len > 1 && req->rx.len > 0) {
		u32 copy_len = min_t(u32, resp_len - 1, req->rx.len);

		memcpy(req->rx.buf, resp_buf + 1, copy_len);
	}

	ret = 0;

out:
	mutex_unlock(&priv->lock);
	return ret;
}

static const struct peci_controller_ops rts591x_peci_oob_ops = {
	.xfer = rts591x_peci_oob_xfer,
};

static int rts591x_peci_oob_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	struct rts591x_mfd_dev *mfd_dev;
	struct rts591x_peci_oob *priv;
	struct peci_controller *controller;

	mfd_dev = dev_get_drvdata(parent);
	if (!mfd_dev) {
		dev_err(dev, "cannot get parent MFD device data\n");
		return -ENODEV;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	priv->regmap = mfd_dev->regmap;
	if (!priv->regmap) {
		dev_err(dev, "cannot get regmap from parent\n");
		return -ENODEV;
	}

	mutex_init(&priv->lock);
	platform_set_drvdata(pdev, priv);

	controller = devm_peci_controller_add(dev, &rts591x_peci_oob_ops);
	if (IS_ERR(controller))
		return dev_err_probe(dev, PTR_ERR(controller),
				     "failed to add PECI controller\n");

	dev_info(dev, "RTS591x PECI over eSPI OOB initialized\n");
	return 0;
}

static int rts591x_peci_oob_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "RTS591x PECI over eSPI OOB removed\n");
	return 0;
}

static const struct of_device_id rts591x_peci_oob_of_match[] = {
	{ .compatible = "realtek,rts591x-peci-oob" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rts591x_peci_oob_of_match);

static struct platform_driver rts591x_peci_oob_driver = {
	.driver = {
		.name = "rts591x-peci-oob",
		.of_match_table = rts591x_peci_oob_of_match,
	},
	.probe = rts591x_peci_oob_probe,
	.remove = rts591x_peci_oob_remove,
};
module_platform_driver(rts591x_peci_oob_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTS591x PECI over eSPI OOB controller driver");
MODULE_IMPORT_NS(PECI);
