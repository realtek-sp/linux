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
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/minmax.h>

#include "internal.h"

#define HEAD_BYTES (3)
#define REG_BYTES  (4)
#define VAL_BYTES  (4)
#define LEN_BYTES  (4)

#define ERASE_TYPE_BYTES (1)
#define ERASE_PAYLOAD_B	 (REG_BYTES + ERASE_TYPE_BYTES)

/* Protocol: 1-byte cmd + 1-byte resp_type + 2-bit seq + 6-bit payload_len */
#define PAYLOAD_MAX_BY_PROTO (0x3F)
#define CHUNK_BYTES	     (PAYLOAD_MAX_BY_PROTO - REG_BYTES - LEN_BYTES)

/* Command definitions */
#define CMD_WRITE_MEM  (0x03) /* single 32-bit write */
#define CMD_READ_MEM   (0x04) /* single 32-bit read */
#define CMD_WRITE_MEMS (0x05) /* block write (multi-frame) */
#define CMD_ERASE_FSPI (0x09) /* FSPI erase */
#define CMD_PECI_OOB   (0x0A) /* PECI over eSPI OOB transaction */

#define DELAY_US_RDWR_MEM   (50)
#define DELAY_US_WR_MEMS    (200)
#define DELAY_US_ERASE_FSPI (4000)

#define PECI_OOB_INIT_DELAY_MS (6)
#define PECI_OOB_TIMEOUT_US    (60000)
#define PECI_OOB_POLL_US       (500)

/* Response codes (from device) */
#define RSP_ACK		    (0x00)
#define RSP_ERR		    (0x01)
#define RSP_DAT		    (0x02)
#define RSP_CMD_RETRY_TOKEN (0xFF)
#define RSP_CMD_RETRY_MAX   (100)

enum iomatrix_fspi_erase_type {
	IOMATRIX_ERASE_4K = 0,
	IOMATRIX_ERASE_32K = 1,
	IOMATRIX_ERASE_64K = 2,
};

/*
 * Generic I2C send + response receive helper.
 *
 * - Sends a request buffer of length req_len.
 * - Waits a short delay for device processing.
 * - Receives 3-byte response header and validates the cmd.
 * - Returns rsp_type and, if present, a 32-bit LE payload (for DAT/ERR).
 *
 * Parameters:
 *   context      - device context (struct device *)
 *   delay_us     - tiny delay before reading the response
 *   req          - request buffer to send
 *   req_len      - request length in bytes
 *   expected_cmd - expected response command echo
 *   rsp_type     - out parameter for response type (ACK/DAT/ERR)
 *   payload_out  - out parameter for 32-bit payload (may be NULL if not needed)
 *
 * Returns:
 *   0 on success (including ACK or DAT/ERR properly read), negative on failure.
 */
static int iomatrix_send_and_recv(void *context, u32 delay_us, const u8 *req,
				  u32 req_len, u8 expected_cmd, u8 *rsp_type,
				  u32 *payload_out)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	int ret, tries;
	u8 rsp_head[HEAD_BYTES];

	/* Send request */
	ret = i2c_master_send(i2c, req, req_len);
	if (ret < 0) {
		dev_err(dev, "I2C send failed: %d\n", ret);
		return ret;
	}
	if (ret != req_len) {
		dev_err(dev, "I2C short write: wrote %d, expected %u\n", ret,
			req_len);
		return -EIO;
	}

	/* Tiny delay before reading the response */
	udelay(delay_us);

	/* Read response header with retry on 0xFF echo */
	for (tries = 0; tries < RSP_CMD_RETRY_MAX; tries++) {
		/* retry until response header get expected cmd */
		ret = i2c_master_recv(i2c, rsp_head, 1);
		if (ret < 0) {
			dev_err(dev, "I2C rsp_head receive cmd failed: %d\n",
				ret);
			return ret;
		}
		/* check data length */
		if (ret != 1) {
			dev_err(dev,
				"I2C short rsp_head read: got %d, expected 1\n",
				ret);
			return -EIO;
		}

		/* Validate response command echo */
		if (rsp_head[0] == expected_cmd)
			break;

		if (rsp_head[0] == RSP_CMD_RETRY_TOKEN) {
			if (tries == RSP_CMD_RETRY_MAX - 1) {
				dev_err(dev,
					"Rsp cmd 0xFF timeout after %d tries (expected 0x%02x)\n",
					RSP_CMD_RETRY_MAX, expected_cmd);
				return -ETIMEDOUT;
			}

			udelay(delay_us);
			continue;
		}

		dev_err(dev, "Unexpected rsp cmd: 0x%02x, expected 0x%02x\n",
			rsp_head[0], expected_cmd);
		return -EPROTO;
	}

	/* Receive response type and response len */
	ret = i2c_master_recv(i2c, rsp_head + 1, sizeof(rsp_head) - 1);
	if (ret < 0) {
		dev_err(dev, "I2C rsp_head receive failed: %d\n", ret);
		return ret;
	}
	if (ret != (sizeof(rsp_head) - 1)) {
		dev_err(dev, "I2C short rsp_head read: got %d, expected %zu\n",
			ret, sizeof(rsp_head));
		return -EIO;
	}

	/* Handle response data */
	*rsp_type = rsp_head[1];

	/* ACK has no payload */
	if (*rsp_type == RSP_ACK) {
		if (payload_out)
			*payload_out = 0;
		return 0;
	}

	/* DAT or ERR should carry a 32-bit payload */
	u8 rsp_payload[VAL_BYTES];

	ret = i2c_master_recv(i2c, rsp_payload, sizeof(rsp_payload));
	if (ret < 0) {
		dev_err(dev, "I2C rsp_payload receive failed: %d\n", ret);
		return ret;
	}
	if (ret != sizeof(rsp_payload)) {
		dev_err(dev, "I2C short payload read: got %d, expected %zu\n",
			ret, sizeof(rsp_payload));
		return -EIO;
	}

	if (payload_out)
		*payload_out = get_unaligned_le32(rsp_payload);

	return 0;
}

/*
 * Single 32-bit read/write transaction over I2C.
 *
 * For write (CMD_WRITE_MEM): payload = reg(4) + val(4)
 * For read  (CMD_READ_MEM): payload = reg(4)
 *
 * Response handling:
 *   - ACK for successful write, no payload.
 *   - DAT for successful read, 4-byte value payload.
 *   - ERR for device error, 4-byte error code payload.
 */
static int iomatrix_bus_xfer(u8 cmd, void *context, u32 reg, u32 w_val,
			     u32 *r_val)
{
	int ret;
	struct device *dev = context;
	u32 payload_len;
	u8 req_buf[HEAD_BYTES + REG_BYTES + VAL_BYTES];
	u8 rsp_type;
	u32 payload = 0;

	if (cmd == CMD_READ_MEM && !r_val) {
		dev_err(dev, "Parameter r_val is NULL\n");
		return -EINVAL;
	}

	/* Prepare request header and payload */
	req_buf[0] = cmd;
	req_buf[1] = 0; /* request rsp_type field (unused for request) */
	payload_len = (CMD_WRITE_MEM == cmd) ? (REG_BYTES + VAL_BYTES) :
					       REG_BYTES;
	req_buf[2] = (0x00 << 6) | (u8)payload_len;

	put_unaligned_le32(reg, &req_buf[HEAD_BYTES]);
	if (cmd == CMD_WRITE_MEM)
		put_unaligned_le32(w_val, &req_buf[HEAD_BYTES + REG_BYTES]);

	/* Send and receive common path */
	ret = iomatrix_send_and_recv(context, DELAY_US_RDWR_MEM, req_buf,
				     HEAD_BYTES + payload_len, cmd, &rsp_type,
				     &payload);
	if (ret < 0)
		return ret;

	/* Interpret response */
	if (rsp_type == RSP_ACK)
		return 0;

	if (rsp_type == RSP_DAT && cmd == CMD_READ_MEM) {
		*r_val = payload;
		return 0;
	}

	if (rsp_type == RSP_ERR) {
		dev_err(dev,
			"Command 0x%02x failed, device error code: 0x%08x\n",
			cmd, payload);
		return -EPROTO;
	}

	dev_err(dev, "Unexpected response type: 0x%02x for cmd 0x%02x\n",
		rsp_type, cmd);
	return -EPROTO;
}

/*
 * Block write helper: split len into frames.
 * Each frame (write): head(3) + reg_addr(4) + data_len(4) + data(chunk)
 * After writing a frame, read 3-byte response header and check ACK/ERR.
 *
 * Notes:
 * - Recompute 'chunk', 'payload_len' and 'msg_len' per frame to handle the
 *   tail frame correctly.
 * - 'seq' is encoded into the header high 2 bits (modulo 4) if the protocol
 *   requires frame sequencing.
 */
static int iomatrix_block_write(void *context, u32 reg_addr, const u8 *buf,
				u32 len, bool addr_autoinc)
{
	struct device *dev = context;
	u8 seq = 0;
	int ret;
	u32 proto_max_data, chunk_max;

	if (!buf || len == 0)
		return -EINVAL;

	/* Max data area per frame based on protocol and adapter limits */
	proto_max_data = PAYLOAD_MAX_BY_PROTO - REG_BYTES - LEN_BYTES;
	chunk_max = min_t(u32, CHUNK_BYTES, proto_max_data);

	if (chunk_max == 0) {
		dev_err(dev, "No writable data per frame due to limits\n");
		return -EMSGSIZE;
	}

	while (len) {
		/* Compute per-frame sizes */
		u32 chunk = min_t(u32, len, chunk_max);
		u32 payload_len = REG_BYTES + LEN_BYTES + chunk;
		u32 msg_len = HEAD_BYTES + payload_len;
		u8 req_buf[HEAD_BYTES + REG_BYTES + LEN_BYTES + CHUNK_BYTES];
		u8 rsp_type;
		u32 payload = 0;

		/* Header: cmd + resp_type + [seq|payload_len] */
		req_buf[0] = CMD_WRITE_MEMS;
		req_buf[1] = 0x00;
		req_buf[2] = ((seq & 0x3) << 6) | (u8)payload_len;

		/* Payload: reg_addr(LE) + data_len(LE) + data(chunk) */
		put_unaligned_le32(reg_addr, &req_buf[HEAD_BYTES]);
		put_unaligned_le32(chunk, &req_buf[HEAD_BYTES + REG_BYTES]);
		memcpy(&req_buf[HEAD_BYTES + REG_BYTES + LEN_BYTES], buf,
		       chunk);

		/* Send frame and receive response */
		ret = iomatrix_send_and_recv(context, DELAY_US_WR_MEMS, req_buf,
					     msg_len, CMD_WRITE_MEMS, &rsp_type,
					     &payload);
		if (ret < 0)
			return ret;

		/* Validate response */
		if (rsp_type == RSP_ACK) {
			/* OK */
		} else if (rsp_type == RSP_ERR) {
			dev_err(dev,
				"Block write failed: err 0x%08x, reg 0x%08x\n",
				payload, reg_addr);
			return -EPROTO;
		} else {
			dev_err(dev, "Unexpected rsp_type for write: 0x%02x\n",
				rsp_type);
			return -EPROTO;
		}

		/* Advance to next frame */
		if (addr_autoinc)
			reg_addr += chunk;

		buf += chunk;
		len -= chunk;
		seq = (seq + 1) & 0x3;
	}

	return 0;
}

int iomatrix_regmap_block_write_protected(struct regmap *map,
					  unsigned int base_reg,
					  const void *buf, size_t len)
{
	int ret;

	if (!map || !buf || len == 0)
		return -EINVAL;

	map->lock(map->lock_arg);
	ret = iomatrix_block_write(map->bus_context, base_reg, (const u8 *)buf,
				   (u32)len, true);
	map->unlock(map->lock_arg);

	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_block_write_protected);

/*
 * FSPI Erase single-frame transaction over I2C.
 *
 * CMD_ERASE_FSPI payload:
 *   - erase_addr(4B, LE)
 *   - erase_type(1B): 0=4K, 1=32K, 2=64K
 *
 * Response:
 *   - RSP_ACK: success, no payload
 *   - RSP_ERR: failure, 4-byte error code
 *   - others: protocol error
 */
static int iomatrix_fspi_erase(void *context, u32 erase_addr, u8 erase_type)
{
	struct device *dev = context;
	u8 req_buf[HEAD_BYTES + ERASE_PAYLOAD_B];
	u8 rsp_type;
	u32 payload = 0;
	int ret;
	const u32 erase_sz[3] = { 4U * 1024U, 32U * 1024U, 64U * 1024U };
	u32 sz;

	if (erase_type > IOMATRIX_ERASE_64K) {
		dev_err(dev, "Invalid erase_type: %u\n", erase_type);
		return -EINVAL;
	}

	sz = erase_sz[erase_type];
	if (erase_addr & (sz - 1)) {
		dev_err(dev,
			"Erase addr 0x%08x is not %u-byte aligned (type %u)\n",
			erase_addr, sz, erase_type);
		return -EINVAL;
	}

	req_buf[0] = CMD_ERASE_FSPI;
	req_buf[1] = 0x00; /* request rsp_type field (unused for request) */
	req_buf[2] = (0x00 << 6) |
		     (u8)ERASE_PAYLOAD_B; /* seq=0, payload_len=5 */

	put_unaligned_le32(erase_addr, &req_buf[HEAD_BYTES]); /* addr[0..3] */
	req_buf[HEAD_BYTES + REG_BYTES] = erase_type; /* type[4]    */

	ret = iomatrix_send_and_recv(context, DELAY_US_ERASE_FSPI, req_buf,
				     sizeof(req_buf), CMD_ERASE_FSPI, &rsp_type,
				     &payload);
	if (ret < 0)
		return ret;

	if (rsp_type == RSP_ACK)
		return 0;

	if (rsp_type == RSP_ERR) {
		dev_err(dev,
			"FSPI erase failed: err=0x%08x, addr=0x%08x, type=%u\n",
			payload, erase_addr, erase_type);
		return -EPROTO;
	}

	dev_err(dev, "Unexpected rsp_type: 0x%02x for CMD_ERASE_FSPI\n",
		rsp_type);
	return -EPROTO;
}

int iomatrix_regmap_fspi_erase_protected(struct regmap *map, u32 erase_addr,
					 u8 erase_type)
{
	int ret;

	if (!map)
		return -EINVAL;

	map->lock(map->lock_arg);
	ret = iomatrix_fspi_erase(map->bus_context, erase_addr, erase_type);
	map->unlock(map->lock_arg);

	return ret;
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_fspi_erase_protected);

/*
 * PECI OOB transaction.
 *
 * Sends a PECI command to the EC via the I2C slave protocol (CMD_PECI_OOB)
 * and reads the response.  The EC processes it over eSPI OOB.
 *
 * iomatrix_regmap_peci_oob() holds the regmap lock across this call to
 * prevent other I2C transactions from corrupting the EC's TX FIFO during
 * the ~60ms wait.
 *
 * Request payload:
 *   [0]        PECI client address
 *   [1]        Write length (WrLen)
 *   [2]        Read length  (RdLen)
 *   [3..]      Write data (WrLen bytes)
 *
 * Response payload (on success):
 *   [0]        PECI Completion Code
 *   [1..]      Read data (RdLen bytes)
 */
static int iomatrix_peci_oob_exec(void *context, const u8 *req_buf, u32 req_len,
				  u8 *rsp_buf, u32 *rsp_len)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	int ret, tries;
	u8 tx_buf[HEAD_BYTES + PAYLOAD_MAX_BY_PROTO];
	u8 rsp_head[HEAD_BYTES];
	u8 rsp_type;
	u32 payload_len;

	if (req_len > PAYLOAD_MAX_BY_PROTO) {
		dev_err(dev, "PECI OOB payload too long: %u\n", req_len);
		return -EINVAL;
	}

	tx_buf[0] = CMD_PECI_OOB;
	tx_buf[1] = 0x00;
	tx_buf[2] = req_len & 0x3F;
	memcpy(tx_buf + HEAD_BYTES, req_buf, req_len);

	/* Send request */
	ret = i2c_master_send(i2c, tx_buf, HEAD_BYTES + req_len);
	if (ret < 0) {
		dev_err(dev, "PECI OOB I2C send failed: %d\n", ret);
		return ret;
	}
	if (ret != HEAD_BYTES + req_len) {
		dev_err(dev, "PECI OOB I2C short write: %d\n", ret);
		return -EIO;
	}

	/* Give the EC a short head start, then poll for the response header. */
	mdelay(PECI_OOB_INIT_DELAY_MS);

	for (tries = 0; tries < PECI_OOB_TIMEOUT_US / PECI_OOB_POLL_US;
	     tries++) {
		ret = i2c_master_recv(i2c, rsp_head, 1);
		if (ret < 0) {
			dev_err(dev, "PECI OOB I2C recv cmd failed: %d\n", ret);
			return ret;
		}
		if (ret != 1) {
			dev_err(dev, "PECI OOB short cmd read: %d\n", ret);
			return -EIO;
		}

		if (rsp_head[0] == CMD_PECI_OOB)
			break;

		if (rsp_head[0] == RSP_CMD_RETRY_TOKEN) {
			udelay(PECI_OOB_POLL_US);
			continue;
		}

		dev_err(dev, "PECI OOB unexpected cmd echo: 0x%02x\n",
			rsp_head[0]);
		return -EPROTO;
	}

	if (tries == PECI_OOB_TIMEOUT_US / PECI_OOB_POLL_US) {
		dev_err(dev, "PECI OOB timeout waiting for response\n");
		return -ETIMEDOUT;
	}

	/* Read remaining response header bytes. */
	ret = i2c_master_recv(i2c, rsp_head + 1, HEAD_BYTES - 1);
	if (ret < 0) {
		dev_err(dev, "PECI OOB I2C recv header failed: %d\n", ret);
		return ret;
	}
	if (ret != HEAD_BYTES - 1) {
		dev_err(dev, "PECI OOB short header: %d\n", ret);
		return -EIO;
	}

	rsp_type = rsp_head[1];
	payload_len = rsp_head[2] & 0x3F;

	if (rsp_type == RSP_ACK) {
		*rsp_len = 0;
		return 0;
	}

	if (payload_len > *rsp_len)
		payload_len = *rsp_len;

	if (rsp_type == RSP_DAT || rsp_type == RSP_ERR) {
		u8 rsp_payload[64];
		u32 read_len;

		read_len = min_t(u32, payload_len, sizeof(rsp_payload));
		ret = i2c_master_recv(i2c, rsp_payload, read_len);
		if (ret < 0)
			return ret;
		if (ret != (int)read_len) {
			dev_err(dev, "PECI OOB short payload: %d\n", ret);
			return -EIO;
		}
		memcpy(rsp_buf, rsp_payload, read_len);
		*rsp_len = read_len;

		if (rsp_type == RSP_DAT)
			return 0;

		dev_err(dev, "PECI OOB device error\n");
		return -EPROTO;
	}

	dev_err(dev, "PECI OOB unexpected rsp_type: 0x%02x\n", rsp_type);
	return -EPROTO;
}

int iomatrix_regmap_peci_oob(struct regmap *map, const u8 *cmd_buf, u32 cmd_len,
			     u8 *resp_buf, u32 *resp_len)
{
	int ret;

	if (!map || !cmd_buf || !resp_buf || !resp_len)
		return -EINVAL;

	map->lock(map->lock_arg);
	ret = iomatrix_peci_oob_exec(map->bus_context, cmd_buf, cmd_len,
				     resp_buf, resp_len);
	map->unlock(map->lock_arg);

	return ret;
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_peci_oob);

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
