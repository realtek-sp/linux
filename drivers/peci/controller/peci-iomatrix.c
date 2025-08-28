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

#include <linux/bitfield.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/peci.h>
#include <linux/regmap.h>
#include <linux/mfd/iomatrix.h>

/* TX DATA REGISTER */
#define RTS591X_PECI_TX	     (0x00)
#define RTS591X_PECI_TX_DATA GENMASK(7, 0)

/* RX DATA REGISTER */
#define RTS591X_PECI_RX	     (0x04)
#define RTS591X_PECI_RX_DATA GENMASK(7, 0)

/* CONTROL REGISTER */
#define RTS591X_PECI_CTRL	  (0x08)
#define RTS591X_PECI_CTRL_EN	  BIT(0)
#define RTS591X_PECI_CTRL_RST	  BIT(3)
#define RTS591X_PECI_CTRL_FIFORST BIT(5)
#define RTS591X_PECI_CTRL_TXEN	  BIT(6)
#define RTS591X_PECI_CTRL_INTEN	  BIT(7)

/* STATUS REGISTER #0 */
#define RTS591X_PECI_STS0	 (0x0C)
#define RTS591X_PECI_STS0_BOFSTS BIT(0)
#define RTS591X_PECI_STS0_EOFSTS BIT(1)
#define RTS591X_PECI_STS0_ERRSTS BIT(2)
#define RTS591X_PECI_STS0_INTSTS BIT(7)

/* STATUS REGISTER #1 */
#define RTS591X_PECI_STS1	  (0x10)
#define RTS591X_PECI_STS1_TXFULL  BIT(0)
#define RTS591X_PECI_STS1_TXEMPTY BIT(1)
#define RTS591X_PECI_STS1_RXFULL  BIT(2)
#define RTS591X_PECI_STS1_RXEMPTY BIT(3)
#define RTS591X_PECI_STS1_BUSY	  BIT(7)

/* ERROR REGISTER */
#define RTS591X_PECI_ERR	 (0x14)
#define RTS591X_PECI_ERR_FCSERR	 BIT(0)
#define RTS591X_PECI_ERR_BUSERR	 BIT(1)
#define RTS591X_PECI_ERR_TXOV	 BIT(4)
#define RTS591X_PECI_ERR_TXUDRUN BIT(5)
#define RTS591X_PECI_ERR_RXOV	 BIT(6)
#define RTS591X_PECI_ERR_CLKERR	 BIT(7)

/* INTERRUPT ENABLE REGISTER #0 */
#define RTS591X_PECI_INTEN0	  (0x18)
#define RTS591X_PECI_INTEN0_BOFEN BIT(0)
#define RTS591X_PECI_INTEN0_EOFEN BIT(1)
#define RTS591X_PECI_INTEN0_ERREN BIT(2)

/* INTERRUPT ENABLE REGISTER #1 */
#define RTS591X_PECI_INTEN1	  (0x1C)
#define RTS591X_PECI_INTEN1_WFEEN BIT(1)
#define RTS591X_PECI_INTEN1_RFFEN BIT(2)

/* OPTIMAL BIT TIME REGISTER LOW */
#define RTS591X_PECI_OBTL     (0x20)
#define RTS591X_PECI_OBTL_VAL GENMASK(7, 0)

/* OPTIMAL BIT TIME REGISTER HIGH */
#define RTS591X_PECI_OBTH     (0x24)
#define RTS591X_PECI_OBTH_VAL GENMASK(7, 0)

#define PECI_DEFAULT_TIMEOUT_US	 (100000) /* us */
#define RTS591X_POLL_SLEEP	 (2000) /* us */
#define RTS591X_PECI_MAX_RETRIES (3)
#define RTS591X_PECI_HDR_LEN	 (3)
#define RTS591X_PECI_CMD_MAX_LEN \
	(PECI_REQUEST_MAX_BUF_SIZE + RTS591X_PECI_HDR_LEN)

struct rts591x_peci {
	struct peci_controller *controller;
	struct device *dev;
	u32 base;
	struct regmap *regmap;
	u8 cmd[RTS591X_PECI_CMD_MAX_LEN];
	struct mutex lock;
};

static int rts591x_peci_enable(struct rts591x_peci *priv)
{
	return regmap_update_bits(priv->regmap, priv->base + RTS591X_PECI_CTRL,
				  RTS591X_PECI_CTRL_EN, RTS591X_PECI_CTRL_EN);
}

static int rts591x_peci_wait_idle(struct rts591x_peci *priv, u32 max_delay_us)
{
	u32 val;

	/* STS1_BUSY 1: IDLE 0: BUSY*/
	return regmap_read_poll_timeout(priv->regmap,
					priv->base + RTS591X_PECI_STS1, val,
					(val & RTS591X_PECI_STS1_BUSY),
					RTS591X_POLL_SLEEP, max_delay_us);
}

static int rts591x_peci_reset(struct rts591x_peci *priv)
{
	int ret;
	struct device *dev = priv->dev;

	// Reset PECI core and FIFOs
	ret = regmap_update_bits(
		priv->regmap, priv->base + RTS591X_PECI_CTRL,
		RTS591X_PECI_CTRL_RST | RTS591X_PECI_CTRL_FIFORST,
		RTS591X_PECI_CTRL_RST | RTS591X_PECI_CTRL_FIFORST);
	if (ret) {
		dev_err(dev, "Failed to set reset bits\n");
		return ret;
	}

	msleep(1);

	// Return to normal operation
	ret = regmap_write(priv->regmap, priv->base + RTS591X_PECI_CTRL, 0x81);
	if (ret) {
		dev_err(dev, "Failed to set normal operation\n");
		return ret;
	}

	// Wait for controller to indicate idle state
	// This has been observed to take up to 2.25ms
	ret = rts591x_peci_wait_idle(priv, PECI_DEFAULT_TIMEOUT_US);
	if (ret) {
		dev_err(dev,
			"PECI controller failed to become idle after reset (timeout)\n");
		return ret;
	}

	ret = regmap_write(priv->regmap, priv->base + RTS591X_PECI_OBTL, 0x81);
	if (ret) {
		dev_err(dev, "Failed to set OBTL to 0x81\n");
		return ret;
	}

	ret = regmap_write(priv->regmap, priv->base + RTS591X_PECI_OBTH, 0x00);
	if (ret) {
		dev_err(dev, "Failed to set OBTH to 0x00\n");
		return ret;
	}

	msleep(1);

	dev_info(dev, "PECI controller reset successfully\n");

	return 0;
}

static int rts591x_peci_read_bytes(struct rts591x_peci *priv, u8 *buf,
				   size_t count)
{
	struct device *dev = priv->dev;
	int ret;
	size_t i;

	if (!count)
		return 0;

	for (i = 0; i < count; i++) {
		u32 status;
		u32 temp_val;

		/* Check if RXFIFO is not empty*/
		ret = regmap_read(priv->regmap, priv->base + RTS591X_PECI_STS1,
				  &status);
		if (ret) {
			dev_err(dev, "read peci STS1 err, ret %d\n", ret);
			return -EIO;
		}
		if (status & RTS591X_PECI_STS1_RXEMPTY) {
			dev_err(dev, "byte %zu: RX FIFO empty\n", i);
			return -EIO;
		}

		/* RXFIFO not empty, read data */
		ret = regmap_read(priv->regmap, priv->base + RTS591X_PECI_RX,
				  &temp_val);
		if (ret) {
			dev_err(dev,
				"Failed to read byte %zu from RX FIFO: %d\n",
				i + 1, ret);
			return ret;
		}
		buf[i] = (u8)FIELD_GET(RTS591X_PECI_RX_DATA, temp_val);
	}

	return 0;
}

static u8 rts591x_peci_calculate_fcs(uint8_t crc, const uint8_t *data_blk_ptr,
				     uint32_t length)
{
	uint8_t temp1, data_byte, bit0;
	unsigned int i, j;

	for (i = 0; i < length; i++) {
		data_byte = *data_blk_ptr++;
		for (j = 0; j < 8; j++) {
			bit0 = (data_byte & 0x80) ? 0x80 : 0;
			data_byte <<= 1;
			crc ^= bit0;
			temp1 = crc & 0x80;
			crc <<= 1;
			if (temp1)
				crc ^= 0x07;
		}
	}
	return crc;
}

static int rts591x_peci_write_cmd(struct rts591x_peci *priv, const u8 *cmd,
				  size_t len)
{
	int ret;
	size_t i;
	unsigned int status;
	u32 err_detail;

	/* Write command bytes to the TX FIFO. */
	for (i = 0; i < len; i++) {
		ret = regmap_read(priv->regmap, priv->base + RTS591X_PECI_STS1,
				  &status);
		if (ret) {
			dev_err(priv->dev,
				"Failed to read peci status1, ret %d\n", ret);
			return ret;
		}
		if (status & RTS591X_PECI_STS1_TXFULL) {
			dev_err(priv->dev, "Peci status1 txfull\n");
			return -ENOMEM;
		}

		/* Write one byte to the FIFO. */
		ret = regmap_write(priv->regmap, priv->base + RTS591X_PECI_TX,
				   (u32)FIELD_PREP(RTS591X_PECI_TX_DATA,
						   cmd[i]));
		if (ret) {
			dev_err(priv->dev, "Failed to write to TX FIFO: %d\n",
				ret);
			return ret;
		}
	}

	ret = rts591x_peci_wait_idle(priv, PECI_DEFAULT_TIMEOUT_US);
	if (ret) {
		dev_err(priv->dev,
			"Timeout waiting for controller to go busy\n");
		return ret;
	}

	/* Clear status bits (Write-1-to-Clear). */
	ret = regmap_update_bits(
		priv->regmap, priv->base + RTS591X_PECI_STS0,
		RTS591X_PECI_STS0_BOFSTS | RTS591X_PECI_STS0_EOFSTS,
		RTS591X_PECI_STS0_BOFSTS | RTS591X_PECI_STS0_EOFSTS);
	if (ret) {
		dev_err(priv->dev, "Failed to clear status bits: %d\n", ret);
		return ret;
	}

	/* Start the transmission. */
	ret = regmap_update_bits(priv->regmap, priv->base + RTS591X_PECI_CTRL,
				 RTS591X_PECI_CTRL_TXEN,
				 RTS591X_PECI_CTRL_TXEN);
	if (ret) {
		dev_err(priv->dev, "Failed to set TXEN: %d\n", ret);
		return ret;
	}

	/* Wait for End Of Frame (EOF) */
	ret = regmap_read_poll_timeout(priv->regmap,
				       priv->base + RTS591X_PECI_STS0, status,
				       (status & RTS591X_PECI_STS0_EOFSTS),
				       RTS591X_POLL_SLEEP,
				       PECI_DEFAULT_TIMEOUT_US);
	if (ret) {
		if (status & RTS591X_PECI_STS0_ERRSTS) {
			regmap_read(priv->regmap, priv->base + RTS591X_PECI_ERR,
				    &err_detail);
			dev_err(priv->dev,
				"PECI hardware error. status: 0x%x, err_detail: 0x%x\n",
				status, err_detail);
		}
		dev_err(priv->dev, "Timeout waiting for End-of-Frame\n");
		return ret;
	}

	/* Wait for the bus to become idle after the transaction. */
	ret = rts591x_peci_wait_idle(priv, PECI_DEFAULT_TIMEOUT_US);
	if (ret) {
		dev_err(priv->dev, "Timeout waiting for bus to go idle\n");
		return ret;
	}

	return 0;
}

static int rts591x_peci_reset_fifos(struct rts591x_peci *priv)
{
	unsigned int status;
	int ret;

	/* Toggle the FIFO reset bit */
	regmap_update_bits(priv->regmap, priv->base + RTS591X_PECI_CTRL,
			   RTS591X_PECI_CTRL_FIFORST,
			   RTS591X_PECI_CTRL_FIFORST);
	udelay(5);
	regmap_update_bits(priv->regmap, priv->base + RTS591X_PECI_CTRL,
			   RTS591X_PECI_CTRL_FIFORST, 0);

	ret = regmap_read(priv->regmap, priv->base + RTS591X_PECI_STS1,
			  &status);
	if (ret) {
		dev_err(priv->dev,
			"reset fifos: Failed to read peci status1, ret %d\n",
			ret);
		return ret;
	}

	if (!(status & RTS591X_PECI_STS1_RXEMPTY)) {
		dev_warn(priv->dev, "FIFO did not clear after reset!\n");
		return -EIO;
	}

	return 0;
}

static int rts591x_peci_send_cmd(struct rts591x_peci *priv, const u8 *write_buf,
				 u8 write_len, u8 *read_buf, u8 read_len)
{
	int ret = 0, retry;
	u8 expected_header_fcs, rcvd_header_fcs;
	u8 expected_data_fcs, rcvd_data_fcs;
	unsigned int err_status, err_detail;
	bool needs_full_reset = false;

	expected_header_fcs =
		rts591x_peci_calculate_fcs(0, write_buf, write_len);

	for (retry = 0; retry <= RTS591X_PECI_MAX_RETRIES; retry++) {
		if (retry > 0) {
			dev_err(priv->dev,
				"PECI transaction failed, retrying (%d/%d)\n",
				retry, RTS591X_PECI_MAX_RETRIES);

			if (needs_full_reset) {
				rts591x_peci_reset(priv);
			} else {
				/* Perform light reset and check if it succeeded */
				if (rts591x_peci_reset_fifos(priv)) {
					/* If FIFO reset fails, escalate to a full reset */
					rts591x_peci_reset(priv);
				}
			}
		}

		ret = rts591x_peci_write_cmd(priv, write_buf, write_len);
		if (ret) {
			needs_full_reset = true;
			continue;
		}

		ret = rts591x_peci_read_bytes(priv, &rcvd_header_fcs, 1);
		if (ret) {
			needs_full_reset = true;
			continue;
		}

		regmap_read(priv->regmap, priv->base + RTS591X_PECI_STS0,
			    &err_status);
		if (err_status & RTS591X_PECI_STS0_ERRSTS) {
			regmap_read(priv->regmap, priv->base + RTS591X_PECI_ERR,
				    &err_detail);
			dev_err(priv->dev,
				"PECI hardware error. STATUS0: 0x%x, ERR: 0x%x\n",
				err_status, err_detail);

			regmap_write(priv->regmap,
				     priv->base + RTS591X_PECI_ERR, 0xFF);

			needs_full_reset =
				!(err_detail & RTS591X_PECI_ERR_FCSERR);
			ret = -EIO;
			continue;
		}

		/* Default to full reset for all subsequent logical errors */
		needs_full_reset = true;

		if (rcvd_header_fcs != expected_header_fcs) {
			dev_err(priv->dev,
				"Header FCS mismatch. Rcvd: 0x%02x, Exp: 0x%02x\n",
				rcvd_header_fcs, expected_header_fcs);
			ret = -EIO;
			continue;
		}

		if (read_len == 0)
			return 0;

		ret = rts591x_peci_read_bytes(priv, read_buf, read_len);
		if (ret)
			continue;

		ret = rts591x_peci_read_bytes(priv, &rcvd_data_fcs, 1);
		if (ret)
			continue;

		if (rcvd_header_fcs == 0 && rcvd_data_fcs == 0) {
			bool all_zeros = true;
			size_t i;
			for (i = 0; i < read_len; i++) {
				if (read_buf[i] != 0) {
					all_zeros = false;
					break;
				}
			}
			if (all_zeros) {
				dev_err(priv->dev,
					"Received all-zero payload and FCS, forcing retry\n");
				ret = -EIO;
				continue;
			}
		}

		expected_data_fcs =
			rts591x_peci_calculate_fcs(0, read_buf, read_len);
		if (rcvd_data_fcs != expected_data_fcs) {
			dev_err(priv->dev,
				"Data FCS mismatch. Rcvd: 0x%02x, Exp: 0x%02x\n",
				rcvd_data_fcs, expected_data_fcs);
			ret = -EIO;
			continue;
		}

		return 0; /* Success! */
	}

	dev_err(priv->dev, "PECI transaction failed after %d retries\n",
		RTS591X_PECI_MAX_RETRIES);
	return ret;
}

static int rts591x_peci_xfer(struct peci_controller *controller, u8 addr,
			     struct peci_request *req)
{
	struct rts591x_peci *priv = dev_get_drvdata(controller->dev.parent);
	size_t cmd_len;
	int ret;

	if (!req) {
		dev_err(priv->dev, "Param req is NULL\n");
		return -EINVAL;
	}

	if (!req->tx.buf || !req->rx.buf) {
		dev_err(priv->dev, "Invalid buffer pointers in request\n");
		return -EINVAL;
	}

	cmd_len = req->tx.len + RTS591X_PECI_HDR_LEN;

	if (cmd_len > RTS591X_PECI_CMD_MAX_LEN) {
		dev_err(priv->dev,
			"Command length %zu exceeds buffer size %d\n", cmd_len,
			RTS591X_PECI_CMD_MAX_LEN);
		return -EINVAL;
	}

	mutex_lock(&priv->lock);

	priv->cmd[0] = addr;
	priv->cmd[1] = req->tx.len;
	priv->cmd[2] = req->rx.len;
	memcpy(priv->cmd + RTS591X_PECI_HDR_LEN, req->tx.buf, req->tx.len);

	ret = rts591x_peci_send_cmd(priv, priv->cmd, cmd_len, req->rx.buf,
				    req->rx.len);
	if (ret) {
		dev_err(priv->dev, "Failed to send cmd, ret = %d\n", ret);
		goto out;
	}

	ret = 0;

out:
	mutex_unlock(&priv->lock);
	return ret;
}

static const struct peci_controller_ops rts591x_ops = {
	.xfer = rts591x_peci_xfer,
};

static int rts591x_peci_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	struct rts591x_mfd_dev *mfd_dev;
	struct rts591x_peci *priv;

	mfd_dev = dev_get_drvdata(parent);
	if (!mfd_dev) {
		dev_err(dev, "Failed to get mfd_dev data\n");
		return -EINVAL;
	}

	/* set private data */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "devm_kzalloc failed\n");
		return -ENOMEM;
	}

	priv->dev = dev;
	platform_set_drvdata(pdev, priv);

	ret = of_property_read_u32(dev->of_node, "reg", &priv->base);
	if (ret) {
		dev_err(dev, "Failed to get base addr\n");
		return ret;
	}

	priv->regmap = mfd_dev->regmap;
	if (!(priv->regmap)) {
		dev_err(dev, "Failed to get regmap\n");
		return -EINVAL;
	}

	mutex_init(&priv->lock);

	/* init */
	ret = rts591x_peci_enable(priv);
	if (ret) {
		dev_err(dev, "PECI enable failed\n");
		return ret;
	}

	priv->controller = devm_peci_controller_add(priv->dev, &rts591x_ops);
	if (IS_ERR(priv->controller))
		return dev_err_probe(dev, PTR_ERR(priv->controller),
				     "failed to add aspeed peci controller\n");

	return 0;
}

static int rts591x_peci_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "RTS591x PECI sub-driver removed.\n");

	return 0;
}

static const struct of_device_id rts591x_peci_of_match[] = {
	{ .compatible = "realtek,rts591x-peci" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, rts591x_peci_of_match);

static struct platform_driver rts591x_peci_driver = {
	.driver = {
		.name = "rts591x-peci",
		.of_match_table = rts591x_peci_of_match,
	},
	.probe = rts591x_peci_probe,
	.remove = rts591x_peci_remove,
};

module_platform_driver(rts591x_peci_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PECI sub-driver for RTS591x using MFD");
MODULE_IMPORT_NS(PECI);
