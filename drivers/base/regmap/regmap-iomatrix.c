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
#include <linux/mfd/iomatrix.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/minmax.h>

#include "internal.h"

#define HEAD_BYTES (3)
#define REG_BYTES  (4)
#define VAL_BYTES  (4)
#define LEN_BYTES  (4)

#define ERASE_PAYLOAD_B (REG_BYTES)

/*
 * Protocol: 1-byte cmd + 1-byte resp_type + 1-byte payload_len (0..255).
 * payload_len occupies the whole attr byte (seq framing is currently unused).
 * PAYLOAD_MAX_BY_PROTO bounds response payloads; a request write frame may carry
 * up to SPIC_REQ_MAX_PAYLOAD bytes (4-byte address + 128-byte data).
 */
#define PAYLOAD_MAX_BY_PROTO (0x3F)
#define SPIC_WRITE_MAX_BYTES (128)
#define SPIC_REQ_MAX_PAYLOAD (REG_BYTES + SPIC_WRITE_MAX_BYTES)

/* Command definitions */
#define CMD_WRITE_MEM	(0x03) /* single 32-bit write */
#define CMD_READ_MEM	(0x04) /* single 32-bit read */
#define CMD_PECI_OOB	(0x0A) /* PECI over eSPI OOB transaction */
#define CMD_WRITE_SPIC	(0x0B) /* SPI flash write (fill EC sector buffer) */
#define CMD_READ_SPIC	(0x0C) /* SPI flash read (reserved) */
#define CMD_UPDATE_SPIC (0x0D) /* commit EC sector buffer */
#define CMD_ERASE_SPIC	(0x0E) /* SPI flash sector erase (reserved) */
#define CMD_REBOOT_EC	(0x0F) /* reboot EC after update (no response) */

#define DELAY_US_RDWR_MEM (50)
#define SPIC_POLL_US	  (1000)
#define SPIC_TIMEOUT_US	  (2000000)

/* EC reboot: settle past the WDT reset, then poll the slave address until it
 * acknowledges again (the EC is running the new image).
 */
#define REBOOT_SETTLE_MS (1500)
#define REBOOT_PROBE_MS	 (20)
#define REBOOT_ONLINE_MS (5000)

#define PECI_OOB_INIT_DELAY_MS (6)
#define PECI_OOB_TIMEOUT_US    (60000)
#define PECI_OOB_POLL_US       (500)

/* Response codes (from device) */
#define RSP_ACK		    (0x00)
#define RSP_ERR		    (0x01)
#define RSP_DAT		    (0x02)
#define RSP_CMD_RETRY_TOKEN (0xFF)
#define RSP_CMD_RETRY_MAX   (100)

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
	req_buf[2] = (u8)payload_len;

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

static int iomatrix_spic_recv(void *context, u8 expected_cmd, u8 *rsp_type,
			      u8 *rsp_buf, u32 *rsp_len)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	unsigned long timeout = jiffies + usecs_to_jiffies(SPIC_TIMEOUT_US);
	u8 rsp_head[HEAD_BYTES];
	u32 payload_len;
	int ret;

	do {
		ret = i2c_master_recv(i2c, rsp_head, 1);
		if (ret < 0)
			return ret;
		if (ret != 1)
			return -EIO;
		if (rsp_head[0] == expected_cmd)
			break;
		if (rsp_head[0] != RSP_CMD_RETRY_TOKEN) {
			dev_err(dev, "SPIC unexpected cmd echo: 0x%02x\n",
				rsp_head[0]);
			return -EPROTO;
		}
		udelay(DELAY_US_RDWR_MEM);
	} while (!time_after(jiffies, timeout));

	if (rsp_head[0] != expected_cmd)
		return -ETIMEDOUT;

	ret = i2c_master_recv(i2c, rsp_head + 1, HEAD_BYTES - 1);
	if (ret < 0)
		return ret;
	if (ret != HEAD_BYTES - 1)
		return -EIO;

	*rsp_type = rsp_head[1];
	payload_len = rsp_head[2] & PAYLOAD_MAX_BY_PROTO;
	if (payload_len > *rsp_len) {
		u8 discard[PAYLOAD_MAX_BY_PROTO];

		ret = i2c_master_recv(i2c, discard, payload_len);
		if (ret < 0)
			return ret;
		if (ret != payload_len)
			return -EIO;
		return -EMSGSIZE;
	}

	if (payload_len) {
		ret = i2c_master_recv(i2c, rsp_buf, payload_len);
		if (ret < 0)
			return ret;
		if (ret != payload_len)
			return -EIO;
	}
	*rsp_len = payload_len;

	return 0;
}

/*
 * Issue a SPIC command whose success response is a bare ACK (erase, write).
 * The frame (header + payload) is built here and the transfer is handed to
 * iomatrix_send_and_recv; an ERR response is decoded from its 4-byte code.
 */
static int iomatrix_spic_ack_cmd(void *context, u8 cmd, const u8 *payload,
				 u32 payload_len)
{
	struct device *dev = context;
	u8 req_buf[HEAD_BYTES + SPIC_REQ_MAX_PAYLOAD];
	u8 rsp_type;
	u32 error = 0;
	int ret;

	if (payload_len > SPIC_REQ_MAX_PAYLOAD)
		return -EMSGSIZE;

	req_buf[0] = cmd;
	req_buf[1] = 0;
	req_buf[2] = (u8)payload_len;
	memcpy(req_buf + HEAD_BYTES, payload, payload_len);

	ret = iomatrix_send_and_recv(context, DELAY_US_RDWR_MEM, req_buf,
				     HEAD_BYTES + payload_len, cmd, &rsp_type,
				     &error);
	if (ret)
		return ret;
	if (rsp_type == RSP_ACK)
		return 0;
	if (rsp_type == RSP_ERR) {
		dev_err(dev, "SPIC command 0x%02x failed: 0x%08x\n", cmd,
			error);
		return error == 0x00000002 ? -ERANGE : -EIO;
	}

	dev_err(dev, "SPIC command 0x%02x unexpected rsp_type 0x%02x\n", cmd,
		rsp_type);
	return -EPROTO;
}

int iomatrix_regmap_update_lock(struct regmap *map)
{
	if (!map)
		return -EINVAL;

	map->lock(map->lock_arg);
	return 0;
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_update_lock);

void iomatrix_regmap_update_unlock(struct regmap *map)
{
	if (map)
		map->unlock(map->lock_arg);
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_update_unlock);

/*
 * Erase the whole sector that contains @addr (reserved; the update path lets
 * CMD_UPDATE_SPIC erase on demand).  The payload is the 4-byte address only; the
 * EC derives the sector from it, so there is no erase-type byte.
 */
int iomatrix_regmap_spic_erase(struct regmap *map, u32 addr)
{
	u8 payload[ERASE_PAYLOAD_B];

	if (!map)
		return -EINVAL;

	put_unaligned_le32(addr, payload);

	return iomatrix_spic_ack_cmd(map->bus_context, CMD_ERASE_SPIC, payload,
				     sizeof(payload));
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_spic_erase);

/*
 * Stream one write frame of a sector.
 *
 * Each frame carries up to 128 data bytes at absolute Flash offset @addr and
 * waits for an ACK, so the EC is paced one frame at a time.  @seq is retained
 * for future packetization but is currently not encoded on the wire.
 */
int iomatrix_regmap_spic_write(struct regmap *map, u32 addr, const u8 *buf,
			       u32 len, u8 seq)
{
	u8 payload[SPIC_REQ_MAX_PAYLOAD];

	(void)seq;

	if (!map || !buf || !len || len > SPIC_WRITE_MAX_BYTES)
		return -EINVAL;

	put_unaligned_le32(addr, payload);
	memcpy(payload + REG_BYTES, buf, len);

	return iomatrix_spic_ack_cmd(map->bus_context, CMD_WRITE_SPIC, payload,
				     REG_BYTES + len);
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_spic_write);

/*
 * SPIC read returns a variable-length DATA payload, which iomatrix_send_and_recv
 * (fixed 4-byte payload) cannot receive, so the request is sent directly and the
 * bulk response is drained by iomatrix_spic_recv.
 */
int iomatrix_regmap_spic_read(struct regmap *map, u32 addr, u8 *buf, u32 len)
{
	struct device *dev;
	struct i2c_client *i2c;
	u8 req_buf[HEAD_BYTES + REG_BYTES + 1];
	u8 rsp_type;
	u32 rsp_len = len;
	int ret;

	if (!map || !buf || !len || len > PAYLOAD_MAX_BY_PROTO)
		return -EINVAL;

	dev = map->bus_context;
	i2c = to_i2c_client(dev);

	req_buf[0] = CMD_READ_SPIC;
	req_buf[1] = 0;
	req_buf[2] = REG_BYTES + 1;
	put_unaligned_le32(addr, &req_buf[HEAD_BYTES]);
	req_buf[HEAD_BYTES + REG_BYTES] = (u8)len;

	ret = i2c_master_send(i2c, req_buf, sizeof(req_buf));
	if (ret < 0)
		return ret;
	if (ret != sizeof(req_buf))
		return -EIO;

	ret = iomatrix_spic_recv(dev, CMD_READ_SPIC, &rsp_type, buf, &rsp_len);
	if (ret)
		return ret;
	if (rsp_type != RSP_DAT || rsp_len != len)
		return -EPROTO;

	return 0;
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_spic_read);

/*
 * Commit the EC sector buffer filled by CMD_WRITE_SPIC (no payload).  The EC
 * compares the buffer against Flash and erases + programs as needed, so the
 * response may take up to the SPIC timeout.  This uses the sleeping poll of
 * iomatrix_spic_recv rather than iomatrix_send_and_recv (which busy-waits and is
 * tuned for fast register responses) to wait out the erase + program.
 */
int iomatrix_regmap_spic_update(struct regmap *map)
{
	struct device *dev;
	struct i2c_client *i2c;
	u8 req_buf[HEAD_BYTES];
	u8 rsp_buf[VAL_BYTES];
	u32 rsp_len = sizeof(rsp_buf);
	u8 rsp_type;
	int ret;

	if (!map)
		return -EINVAL;

	dev = map->bus_context;
	i2c = to_i2c_client(dev);

	req_buf[0] = CMD_UPDATE_SPIC;
	req_buf[1] = 0;
	req_buf[2] = 0;

	ret = i2c_master_send(i2c, req_buf, sizeof(req_buf));
	if (ret < 0)
		return ret;
	if (ret != sizeof(req_buf))
		return -EIO;

	ret = iomatrix_spic_recv(dev, CMD_UPDATE_SPIC, &rsp_type, rsp_buf,
				 &rsp_len);
	if (ret)
		return ret;
	if (rsp_type == RSP_ACK)
		return 0;
	if (rsp_type == RSP_ERR && rsp_len == sizeof(u32)) {
		u32 error = get_unaligned_le32(rsp_buf);

		dev_err(dev, "SPIC update failed: 0x%08x\n", error);
		return error == 0x00000002 ? -ERANGE : -EIO;
	}

	dev_err(dev, "SPIC update unexpected rsp_type 0x%02x\n", rsp_type);
	return -EPROTO;
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_spic_update);

/*
 * Wait for the EC to come back on the bus after a reboot.  During the WDT reset
 * and re-boot the slave NAKs its address, so a 1-byte read fails; once the new
 * image answers, the read succeeds.  A quiet loop is used (no per-attempt
 * dev_err) because failures are the expected state until the EC returns.
 */
static int iomatrix_spic_wait_online(void *context)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	unsigned long timeout;
	u8 probe;
	int ret;

	/* Let the EC take the WDT reset and start booting the new image before
	 * probing, so a still-running old image cannot answer as "online".
	 */
	msleep(REBOOT_SETTLE_MS);

	timeout = jiffies + msecs_to_jiffies(REBOOT_ONLINE_MS);
	do {
		ret = i2c_master_recv(i2c, &probe, 1);
		if (ret >= 0)
			return 0;
		msleep(REBOOT_PROBE_MS);
	} while (time_before(jiffies, timeout));

	dev_err(dev, "EC did not come back online %d ms after reboot\n",
		REBOOT_SETTLE_MS + REBOOT_ONLINE_MS);
	return -ETIMEDOUT;
}

/*
 * Reboot the EC once every sector has been committed (no payload).  The EC
 * resets via its watchdog and does not answer the reboot command itself, so the
 * request is sent fire-and-forget ([0F][00][00] with a STOP).  Reboot is only
 * reported successful once the EC acknowledges its address again, i.e. it has
 * booted the new image; a caller holding the regmap lock keeps other I2C traffic
 * off the bus across the reset window.
 */
int iomatrix_regmap_spic_reboot(struct regmap *map)
{
	struct device *dev;
	struct i2c_client *i2c;
	u8 req_buf[HEAD_BYTES];
	int ret;

	if (!map)
		return -EINVAL;

	dev = map->bus_context;
	i2c = to_i2c_client(dev);

	req_buf[0] = CMD_REBOOT_EC;
	req_buf[1] = 0;
	req_buf[2] = 0;

	ret = i2c_master_send(i2c, req_buf, sizeof(req_buf));
	if (ret < 0)
		return ret;
	if (ret != sizeof(req_buf))
		return -EIO;

	return iomatrix_spic_wait_online(dev);
}
EXPORT_SYMBOL_GPL(iomatrix_regmap_spic_reboot);

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
