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

#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/module.h>

#include "internal.h"

#define HEAD_BYTES (3)
#define REG_BYTES  (4)
#define VAL_BYTES  (4)

#define CMD_WRITE_MEM (0x03)
#define CMD_READ_MEM  (0x04)

#define RSP_ACK (0x00)
#define RSP_ERR (0x01)
#define RSP_DAT (0x02)

static int iomatrix_bus_xfer(u8 cmd, void *context, u32 reg, u32 w_val,
			     u32 *r_val)
{
	int ret;
	u32 i2c_msg_len;
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	u32 payload, payload_len;
	u8 req_buf[HEAD_BYTES + REG_BYTES + VAL_BYTES];
	u8 rsp_head[HEAD_BYTES];
	u8 rsp_payload[VAL_BYTES];

	if (cmd == CMD_READ_MEM && !r_val) {
		dev_err(dev, "Parameter r_val is NULL\n");
		return -EINVAL;
	}

	/* prepare request data */
	req_buf[0] = cmd;
	req_buf[1] = 0; //rsp_type
	payload_len =
		(CMD_WRITE_MEM == cmd ? REG_BYTES + VAL_BYTES : REG_BYTES);
	req_buf[2] = (0x00 << 6) | payload_len;
	put_unaligned_le32(reg, &req_buf[HEAD_BYTES]);
	if (cmd == CMD_WRITE_MEM)
		put_unaligned_le32(w_val, &req_buf[HEAD_BYTES + REG_BYTES]);

	/* send command */
	i2c_msg_len = HEAD_BYTES + payload_len;
	ret = i2c_master_send(i2c, req_buf, i2c_msg_len);
	if (ret < 0) {
		dev_err(dev, "I2C send failed: %d\n", ret);
		return ret;
	}
	if (ret != i2c_msg_len) {
		dev_err(dev, "I2C short write: wrote %d, expected %d\n", ret,
			i2c_msg_len);
		return -EIO;
	}

	/* waiting for the data to be ready */
	udelay(50);

	/* get response header */
	i2c_msg_len = ARRAY_SIZE(rsp_head);
	ret = i2c_master_recv(i2c, rsp_head, i2c_msg_len);
	if (ret < 0) {
		dev_err(dev, "I2C rsp_head receive failed: %d\n", ret);
		return ret;
	}
	if (ret != i2c_msg_len) {
		dev_err(dev, "I2C short rsp_head read: got %d, expected %d\n",
			ret, i2c_msg_len);
		return -EIO;
	}

	if (rsp_head[0] != cmd)
		return -EINVAL;

	if (rsp_head[1] == RSP_ACK)
		return 0;

	/* get response payload */
	i2c_msg_len = ARRAY_SIZE(rsp_payload);
	ret = i2c_master_recv(i2c, rsp_payload, i2c_msg_len);
	if (ret < 0) {
		dev_err(dev, "I2C rsp_payload receive failed: %d\n", ret);
		return ret;
	}
	if (ret != i2c_msg_len) {
		dev_err(dev, "I2C short payload read: got %d, expected %d\n",
			ret, i2c_msg_len);
		return -EIO;
	}

	payload = get_unaligned_le32(rsp_payload);
	if (RSP_DAT == rsp_head[1]) {
		if (cmd == CMD_READ_MEM) {
			*r_val = payload;
			return 0;
		}

		dev_err(dev, "Unexpected data response for write command\n");
		return -EPROTO;
	}

	dev_err(dev, "Command 0x%02x failed, device error code: 0x%08x\n", cmd,
		payload);

	return -EPROTO;
}

static int regmap_i2c_write(void *context, u32 reg, u32 val)
{
	return iomatrix_bus_xfer(CMD_WRITE_MEM, context, reg, val, NULL);
}

static int regmap_i2c_read(void *context, u32 reg, u32 *val)
{
	return iomatrix_bus_xfer(CMD_READ_MEM, context, reg, 0, val);
}

static const struct regmap_bus iomatrix_regmap_i2c = {
	.reg_write = regmap_i2c_write,
	.reg_read = regmap_i2c_read,
};

struct regmap *__regmap_init_i2c_iomatrix(struct i2c_client *i2c,
					  const struct regmap_config *config,
					  struct lock_class_key *lock_key,
					  const char *lock_name)
{
	return __regmap_init(&i2c->dev, &iomatrix_regmap_i2c, &i2c->dev, config,
			     lock_key, lock_name);
}
EXPORT_SYMBOL_GPL(__regmap_init_i2c_iomatrix);

struct regmap *__devm_regmap_init_i2c_iomatrix(
	struct i2c_client *i2c, const struct regmap_config *config,
	struct lock_class_key *lock_key, const char *lock_name)
{
	return __devm_regmap_init(&i2c->dev, &iomatrix_regmap_i2c, &i2c->dev,
				  config, lock_key, lock_name);
}
EXPORT_SYMBOL_GPL(__devm_regmap_init_i2c_iomatrix);

MODULE_DESCRIPTION("Regmap IOMatrix Module");
MODULE_LICENSE("GPL v2");
