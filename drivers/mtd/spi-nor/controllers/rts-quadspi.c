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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/spi-nor.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/pinctrl/consumer.h>
#include "../core.h"
#include "rts-quadspi.h"

static inline u32 rts_readl(struct rts_qspi *rqspi, u32 reg)
{
	return readl(rqspi->regs + reg);
}

static inline u16 rts_readw(struct rts_qspi *rqspi, u32 reg)
{
	return readw(rqspi->regs + reg);
}

static inline u8 rts_readb(struct rts_qspi *rqspi, u32 reg)
{
	return readb(rqspi->regs + reg);
}

static inline void rts_writel(struct rts_qspi *rqspi, u32 reg, u32 val)
{
	writel(val, rqspi->regs + reg);
}

static inline void rts_writew(struct rts_qspi *rqspi, u32 reg, u16 val)
{
	writew(val, rqspi->regs + reg);
}

static inline void rts_writeb(struct rts_qspi *rqspi, u32 reg, u8 val)
{
	writeb(val, rqspi->regs + reg);
}

static void addr2cmd(int addr_nbytes, u32 addr, u8 *cmd)
{
	cmd[1] = addr >> (addr_nbytes * 8 - 8);
	cmd[2] = addr >> (addr_nbytes * 8 - 16);
	cmd[3] = addr >> (addr_nbytes * 8 - 24);
	cmd[4] = addr >> (addr_nbytes * 8 - 32);
}

static int rts_qspi_controller_ready(struct rts_qspi *rqspi)
{
	u32 cnt;
	u32 reg;

	for (cnt = 0; cnt < 1000; cnt++) {
		reg = rts_readl(rqspi, SSIENR);
		if (!(reg & BIT(SPIC_EN)))
			return 0;
		udelay(1);
	}
	return -EBUSY;
}

static inline int rts_qspi_set_dummy(struct rts_qspi *rqspi, u32 cycle)
{
	u32 baud;
	u32 dummy;
	u32 reg;
	u32 internal_dummy;

	internal_dummy = 0;

	if (cycle == 0) {
		dummy = 0;
	} else {
		baud = rts_readl(rqspi, BAUDR);
		dummy = baud * cycle * 2 + internal_dummy;
	}

	if (dummy > USER_RD_DUMMY_LENGTH_MASK)
		return -EINVAL;

	reg = rts_readl(rqspi, USER_LENGTH);
	reg = (reg & ~USER_RD_DUMMY_LENGTH_MASK) | dummy;
	rts_writel(rqspi, USER_LENGTH, reg);

	return 0;
}

static int rts_qspi_read_xfer(struct spi_nor *nor, struct spi_nor_xfer_cfg *cfg,
			      u8 *buf, size_t len)
{
	struct rts_qspi *rqspi = nor->priv;
	int ret, cnt;
	u32 reg;

	ret = rts_qspi_set_dummy(rqspi, cfg->dummy_cycles);
	if (ret)
		goto FAIL;

	reg = rts_readl(rqspi, CTRLR0);
	reg &= ~((TMOD_MASK << TMOD_OFFSET) | (DDR_EN_MASK << DDR_EN_OFFSET) |
		 (ADDR_CH_MASK << ADDR_CH_OFFSET) |
		 (DATA_CH_MASK << DATA_CH_OFFSET) |
		 (CMD_CH_MASK << CMD_CH_OFFSET));
	reg |= (((u32)(cfg->mode) << TMOD_OFFSET) |
		((u32)(cfg->ddr_en) << DDR_EN_OFFSET) |
		((u32)(cfg->addr_nbits >> 1) << ADDR_CH_OFFSET) |
		((u32)(cfg->data_nbits >> 1) << DATA_CH_OFFSET) |
		((u32)(cfg->cmd_nbits >> 1) << CMD_CH_OFFSET));

	reg |= BIT(USER_MODE);
	rts_writel(rqspi, CTRLR0, reg);
	if (cfg->addr_nbytes) {
		u8 cmd[5], cmd_len;
		int cnt;

		cmd[0] = cfg->cmd;
		addr2cmd(cfg->addr_nbytes, cfg->addr, cmd);
		cmd_len = cfg->addr_nbytes + 1;

		for (cnt = 0; cnt < cmd_len; cnt++)
			rts_writeb(rqspi, DR, cmd[cnt]);

		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH) |
			 (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (cfg->addr_nbytes << USER_ADDR_LENGTH);
		reg |= (1 << USER_CMD_LENGTH);
		rts_writel(rqspi, USER_LENGTH, reg);
	} else {
		rts_writeb(rqspi, DR, cfg->cmd);
		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH) |
			 (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (1 << USER_CMD_LENGTH);
		rts_writel(rqspi, USER_LENGTH, reg);
	}

	rts_writel(rqspi, TX_NDF, 0);
	rts_writel(rqspi, RX_NDF, len);
	rts_writel(rqspi, SSIENR, 1);

	ret = rts_qspi_controller_ready(rqspi);
	if (ret) {
		dev_err(nor->dev, "controller busy\n");
		goto FAIL;
	}

	/* Optimize for 4 Byte FIFO read */
	for (cnt = 0; cnt < len / 4; cnt++) {
		u32 *buf32 = (u32 *)buf;

		buf32[cnt] = rts_readl(rqspi, DR);
	}
	for (cnt = len - len % 4; cnt < len; cnt++)
		buf[cnt] = rts_readb(rqspi, DR);

	return 0;
FAIL:
	dev_err(nor->dev, "%s() failed, ret = %d\n", __func__, ret);
	return ret;
}

static int rts_qspi_write_xfer(struct spi_nor *nor,
			       struct spi_nor_xfer_cfg *cfg, u8 *buf,
			       size_t len)
{
	struct rts_qspi *rqspi = nor->priv;
	int ret, cnt;
	u32 reg;

	ret = rts_qspi_set_dummy(rqspi, cfg->dummy_cycles);
	if (ret)
		goto FAIL;

	reg = rts_readl(rqspi, CTRLR0);
	reg &= ~((TMOD_MASK << TMOD_OFFSET) | (ADDR_CH_MASK << ADDR_CH_OFFSET) |
		 (DATA_CH_MASK << DATA_CH_OFFSET) |
		 (CMD_CH_MASK << CMD_CH_OFFSET));
	reg |= (((u32)(cfg->mode) << TMOD_OFFSET) |
		((u32)(cfg->addr_nbits >> 1) << ADDR_CH_OFFSET) |
		((u32)(cfg->data_nbits >> 1) << DATA_CH_OFFSET) |
		((u32)(cfg->cmd_nbits >> 1) << CMD_CH_OFFSET));
	reg |= BIT(USER_MODE);
	rts_writel(rqspi, CTRLR0, reg);

	/* when transmited bytes are not greater than 4, use ADDR_LENGTH
	 * to indicate non-cmd bytes. When len equals zero, we don't
	 * push data into FIFO, just ignore it.
	 */
	if (cfg->addr_nbytes == 0) {
		/* nor->write_reg */
		rts_writeb(rqspi, DR, cfg->cmd);
		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH) |
			 (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (1 << USER_CMD_LENGTH);

		rts_writel(rqspi, USER_LENGTH, reg);

		for (cnt = 0; cnt < len; cnt++)
			rts_writeb(rqspi, DR, buf[cnt]);
	} else {
		/* nor->write */
		u8 cmd[5];
		u8 cmd_len;

		cmd[0] = cfg->cmd;
		addr2cmd(cfg->addr_nbytes, cfg->addr, cmd);
		cmd_len = cfg->addr_nbytes + 1;

		for (cnt = 0; cnt < cmd_len; cnt++)
			rts_writeb(rqspi, DR, cmd[cnt]);

		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH) |
			 (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (cfg->addr_nbytes << USER_ADDR_LENGTH);
		reg |= (1 << USER_CMD_LENGTH);
		rts_writel(rqspi, USER_LENGTH, reg);

		/* Optimize for 4 Byte FIFO write */
		for (cnt = 0; cnt < len / 4; cnt++) {
			u32 *buf32 = (u32 *)buf;

			rts_writel(rqspi, DR, buf32[cnt]);
		}

		for (cnt = len - len % 4; cnt < len; cnt++)
			rts_writeb(rqspi, DR, buf[cnt]);
	}
	rts_writel(rqspi, RX_NDF, 0);
	rts_writel(rqspi, TX_NDF, len);
	rts_writel(rqspi, SSIENR, 1);

	ret = rts_qspi_controller_ready(rqspi);
	if (ret) {
		dev_err(nor->dev, "controller busy\n");
		goto FAIL;
	}

	return 0;

FAIL:
	dev_err(nor->dev, "%s() failed, errno = %d\n", __func__, ret);
	return ret;
}

static void cfg_transfer(struct spi_nor_xfer_cfg *cfg, u32 opcode, u32 proto,
			 loff_t dir, u8 addr_nbytes, u8 mode, u8 dummy_cycles)
{
	cfg->wren = 0;
	cfg->cmd = opcode;
	cfg->cmd_nbits = spi_nor_get_protocol_inst_nbits(proto);
	cfg->addr = dir;
	cfg->addr_nbytes = addr_nbytes;
	cfg->addr_nbits = spi_nor_get_protocol_addr_nbits(proto);
	cfg->mode = mode;
	cfg->data_nbits = spi_nor_get_protocol_data_nbits(proto);
	cfg->mode_cycles = 0;
	cfg->dummy_cycles = dummy_cycles;
}

static int rts_qspi_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf,
			     size_t len)
{
	struct spi_nor_xfer_cfg cfg = { 0 };

	cfg_transfer(&cfg, opcode, SNOR_PROTO_1_1_1, 0, 0, RECEIVE_MODE, 0);

	return rts_qspi_read_xfer(nor, &cfg, buf, len);
}

static int rts_qspi_write_reg(struct spi_nor *nor, u8 opcode, const u8 *buf,
			      size_t len)
{
	struct spi_nor_xfer_cfg cfg = { 0 };

	cfg_transfer(&cfg, opcode, SNOR_PROTO_1_1_1, 0, 0, TRANSMIT_MODE, 0);

	return rts_qspi_write_xfer(nor, &cfg, buf, len);
}

static int _rts_qspi_read(struct spi_nor *nor, loff_t from, size_t len,
			  size_t *retlen, u_char *buf)
{
	struct spi_nor_xfer_cfg cfg = { 0 };
	struct rts_qspi *rqspi = nor->priv;
	int ret;
	u32 reg;

	cfg_transfer(&cfg, nor->read_opcode, nor->read_proto, from,
		     nor->addr_nbytes, RECEIVE_MODE, nor->read_dummy);

	if (spi_nor_protocol_is_dtr(nor->read_proto)) {
		cfg.ddr_en = DDR_EN;
		reg = readl(rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);
		reg |= SPIC_DDR_EN;
		writel(reg, rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);

		reg = rts_readl(rqspi, AUTO_LENGTH);
		reg &= ~(AUTO_IN_PHYSICAL_CYC_MASK << AUTO_IN_PHYSICAL_CYC);
		reg |= (6 << AUTO_IN_PHYSICAL_CYC);
		rts_writel(rqspi, AUTO_LENGTH, reg);
	} else {
		cfg.ddr_en = 0;
		reg = rts_readl(rqspi, AUTO_LENGTH);
		reg &= ~(AUTO_IN_PHYSICAL_CYC_MASK << AUTO_IN_PHYSICAL_CYC);
		reg |= (2 << AUTO_IN_PHYSICAL_CYC);
		rts_writel(rqspi, AUTO_LENGTH, reg);
	}

	ret = rts_qspi_read_xfer(nor, &cfg, buf, len);

	if (spi_nor_protocol_is_dtr(nor->read_proto)) {
		reg = rts_readl(rqspi, CTRLR0);
		reg &= ~(DDR_EN_MASK << DDR_EN_OFFSET);
		rts_writel(rqspi, CTRLR0, reg);

		reg = readl(rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);
		reg &= ~(SPIC_DDR_EN);
		writel(reg, rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);

		reg = rts_readl(rqspi, AUTO_LENGTH);
		reg &= ~(AUTO_IN_PHYSICAL_CYC_MASK << AUTO_IN_PHYSICAL_CYC);
		reg |= (2 << AUTO_IN_PHYSICAL_CYC);
		rts_writel(rqspi, AUTO_LENGTH, reg);
	}

	if (ret)
		return ret;

	*retlen += len;

	return ret;
}

static ssize_t rts_qspi_read(struct spi_nor *nor, loff_t from, size_t len,
			     u_char *buf)
{
	struct rts_qspi *rqspi = nor->priv;
	int fifo = rqspi->fifo_size;
	int err;
	ssize_t retlen = 0;

	while (len) {
		size_t unit = fifo, rlen = 0;

		if (len < unit)
			unit = len;

		err = _rts_qspi_read(nor, from, unit, &rlen, buf);
		if (err)
			return err;

		retlen += rlen;
		len -= unit;
		from += unit;
		buf += unit;
	}

	return retlen;
}

static int _rts_qspi_write(struct spi_nor *nor, loff_t to, size_t len,
			   size_t *retlen, const u_char *buf)
{
	struct spi_nor_xfer_cfg cfg = { 0 };
	int ret;

	cfg_transfer(&cfg, nor->program_opcode, nor->write_proto, to,
		     nor->addr_nbytes, TRANSMIT_MODE, 0);

	ret = rts_qspi_write_xfer(nor, &cfg, (u_char *)buf, len);
	if (ret)
		return ret;

	*retlen += len;

	return ret;
}

/*
 * Set write enable latch with Write Enable command.
 * Returns negative if error occurred.
 */
static inline int write_enable(struct spi_nor *nor)
{
	return nor->controller_ops->write_reg(nor, SPINOR_OP_WREN, NULL, 0);
}

static u8 rts_qspi_get_sr_bp_mask(struct spi_nor *nor)
{
	u8 mask = SR_BP2 | SR_BP1 | SR_BP0;

	if (nor->flags & SNOR_F_HAS_SR_BP3_BIT6)
		return mask | SR_BP3_BIT6;

	if (nor->flags & SNOR_F_HAS_4BIT_BP)
		return mask | SR_BP3;

	return mask;
}

static int common_swp_set_state(struct spi_nor *nor, enum swp_state state)
{
	int ret;
	u8 val, mask;

	ret = spi_nor_read_sr(nor, &val);
	if (ret < 0) {
		dev_err(nor->dev, "error %d reading SR\n", ret);
		return ret;
	}

	mask = rts_qspi_get_sr_bp_mask(nor);
	ret = write_enable(nor);
	if (ret < 0) {
		dev_err(nor->dev, "error %d enabling write\n", ret);
		return ret;
	}

	if (state == SWP_ENABLE) {
		val |= mask;
	} else {
		val &= ~mask;
	}

	ret = spi_nor_write_sr(nor, &val, 1);
	if (ret < 0) {
		dev_err(nor->dev, "error %d writing SR\n", ret);
		return ret;
	}

	ret = spi_nor_wait_till_ready(nor);
	if (ret < 0) {
		dev_err(nor->dev, "error %d waiting till ready\n", ret);
		return ret;
	}

	return 0;
}

static int enable_swp_thread(void *data)
{
	struct spi_nor *nor = (struct spi_nor *)data;

	while (1) {
		mutex_lock(&nor->lock);

		if (!spi_nor_swp.swp_flag &&
		    time_after_eq(jiffies, spi_nor_swp.time + HZ)) {
			if (!common_swp_set_state(nor, SWP_ENABLE))
				spi_nor_swp.swp_flag = true;
			else
				dev_err(nor->dev, "enable swp failed\n");
		}
		mutex_unlock(&nor->lock);
		msleep(1000);
	}

	return 0;
}

static void spi_nor_init_swp(struct spi_nor *nor)
{
	spi_nor_swp.time = jiffies;

	if (common_swp_set_state(nor, SWP_ENABLE))
		spi_nor_swp.swp_flag = false;
	else
		spi_nor_swp.swp_flag = true;

	spi_nor_swp.enable_swp_task = kthread_create(
		enable_swp_thread, (void *)nor, "enable_swp_task");
	if (IS_ERR(spi_nor_swp.enable_swp_task)) {
		dev_err(nor->dev, "Unable to start kernel thread.\n");
		spi_nor_swp.enable_swp_task = NULL;
	} else {
		wake_up_process(spi_nor_swp.enable_swp_task);
	}
}

static void rts_qspi_swp_disable(struct spi_nor *nor)
{
	if (spi_nor_swp.swp_flag) {
		if (!common_swp_set_state(nor, SWP_DISABLE)) {
			spi_nor_swp.swp_flag = false;
		} else {
			dev_err(nor->dev, "disable swp failed\n");
			return;
		}
		spi_nor_swp.time = jiffies;
	}
}

static ssize_t rts_qspi_write_polling(struct spi_nor *nor, loff_t to,
				      size_t len, const u_char *buf)
{
	struct rts_qspi *rqspi = nor->priv;
	int fifo = rqspi->fifo_size;
	int ret;
	ssize_t retlen = 0;

	rts_qspi_swp_disable(nor);

	while (len) {
		size_t unit = fifo, rlen = 0;

		if (len < unit)
			unit = len;

		write_enable(nor);

		ret = _rts_qspi_write(nor, to, unit, &rlen, buf);
		if (ret)
			goto FAIL;

		ret = spi_nor_wait_till_ready(nor);
		if (ret)
			goto FAIL;

		retlen += rlen;
		len -= unit;
		to += unit;
		buf += unit;
	}

	return retlen;
FAIL:
	dev_err(nor->dev, "%s() failed, ret = %d\n", __func__, ret);
	return ret;
}

static ssize_t rts_qspi_write_irq(struct spi_nor *nor, loff_t to, size_t len,
				  const u_char *buf)
{
	struct rts_qspi *rqspi = nor->priv;
	int fifo = rqspi->fifo_size;
	struct completion done_data;
	long timeleft;
	int ret;
	int timeout = rqspi->timeout_ms;
	ssize_t retlen = 0;

	rts_qspi_swp_disable(nor);

	while (len) {
		size_t unit = fifo, rlen = 0;

		if (len < unit)
			unit = len;

		spin_lock_irqsave(&rqspi->__lock, rqspi->__lock_flags);
		rqspi->done = &done_data;
		init_completion(&done_data);

		write_enable(nor);

		ret = _rts_qspi_write(nor, to, unit, &rlen, buf);
		if (ret)
			goto FAIL;

		spin_unlock_irqrestore(&rqspi->__lock, rqspi->__lock_flags);
		rts_writel(rqspi, TX_NDF, 0);
		rts_writel(rqspi, USER_LENGTH, 0);
		rts_writel(rqspi, SSIENR, 3);
		timeleft = wait_for_completion_timeout(
			rqspi->done, msecs_to_jiffies(timeout));
		if (timeleft <= 0) {
			ret = -ETIMEDOUT;
			goto FAIL;
		}
		if (rqspi->status) {
			ret = spi_nor_wait_till_ready(nor);
			if (ret)
				goto FAIL;
		}

		retlen += rlen;
		len -= unit;
		to += unit;
		buf += unit;
	}

	return retlen;
FAIL:
	dev_err(nor->dev, "%s() failed, ret = %d\n", __func__, ret);
	return ret;
}

static int rts_qspi_erase_polling(struct spi_nor *nor, loff_t offs)
{
	int ret;
	u8 cmd_buf[8];

	rts_qspi_swp_disable(nor);

	ret = write_enable(nor);

	cmd_buf[0] = offs >> (nor->addr_nbytes * 8 - 8);
	cmd_buf[1] = offs >> (nor->addr_nbytes * 8 - 16);
	cmd_buf[2] = offs >> (nor->addr_nbytes * 8 - 24);
	cmd_buf[3] = offs >> (nor->addr_nbytes * 8 - 32);

	ret = nor->controller_ops->write_reg(nor, nor->erase_opcode, cmd_buf,
					     nor->addr_nbytes);
	if (ret)
		return ret;

	return 0;
}

static int rts_qspi_erase_irq(struct spi_nor *nor, loff_t offs)
{
	struct completion done_data;
	long timeleft;
	struct rts_qspi *rqspi = nor->priv;
	int timeout = rqspi->timeout_ms;
	u32 baudr;
	int ret;
	u8 cmd_buf[8];

	rts_qspi_swp_disable(nor);

	ret = write_enable(nor);

	spin_lock_irqsave(&rqspi->__lock, rqspi->__lock_flags);
	rqspi->done = &done_data;
	init_completion(&done_data);

	cmd_buf[0] = offs >> (nor->addr_nbytes * 8 - 8);
	cmd_buf[1] = offs >> (nor->addr_nbytes * 8 - 16);
	cmd_buf[2] = offs >> (nor->addr_nbytes * 8 - 24);
	cmd_buf[3] = offs >> (nor->addr_nbytes * 8 - 32);

	ret = nor->controller_ops->write_reg(nor, nor->erase_opcode, cmd_buf,
					     nor->addr_nbytes);
	if (ret)
		return ret;

	spin_unlock_irqrestore(&rqspi->__lock, rqspi->__lock_flags);
	rts_writel(rqspi, TX_NDF, 0);
	rts_writel(rqspi, USER_LENGTH, 0);
	baudr = rts_readl(rqspi, BAUDR);
	/* make auto check timeout longer */
	rts_writel(rqspi, BAUDR, baudr * 20);
	rts_writel(rqspi, SSIENR, 3);
	timeleft = wait_for_completion_timeout(rqspi->done,
					       msecs_to_jiffies(timeout));
	rts_writel(rqspi, SSIENR, 0);
	rts_writel(rqspi, BAUDR, baudr);
	if (timeleft <= 0) {
		ret = -ETIMEDOUT;
		return ret;
	}

	return 0;
}

static const struct spi_nor_controller_ops rts_qspi_ops = {
	.read_reg = rts_qspi_read_reg,
	.write_reg = rts_qspi_write_reg,
	.read = rts_qspi_read,
	.write = rts_qspi_write,
	.erase = rts_qspi_erase,
};

static int rts_qspi_setup(struct rts_qspi *rqspi, struct device_node *np)
{
	struct platform_device *pdev = rqspi->pdev;
	const char *spi_transfer_mode;
	struct spi_nor_hwcaps hwcaps = {};
	u32 baudr, speed_hz;
	u32 reg = 0;
	int ret;

	/* User can't program some control register if SSIENR
	 * is enabled. So disable it before init registers
	 */
	rts_writel(rqspi, SSIENR, 0); /* Disable SPIC */

	/* spi mode */
	if (of_property_read_bool(np, "spi-cpha"))
		reg |= SPI_CPHA;
	if (of_property_read_bool(np, "spi-cpol"))
		reg |= SPI_CPOL;

	if (IS_ENABLED(CONFIG_SPI_RTS_QUADSPI_IRQ))
		reg |= (0x1f << 23);

	reg |= BIT(USER_MODE);
	rts_writel(rqspi, CTRLR0, reg);

	ret = of_property_read_u32(np, "spi-frequency", &speed_hz);
	if (ret) {
		dev_err(&pdev->dev, "There's no spi-frequency propert.\n");
		return -EINVAL;
	}
	/* Set clock ratio
	 * F(spi_sclk) = F(bus) / (2 * baudr)
	 */
	if ((speed_hz == 0) || (speed_hz > rqspi->max_speed_hz)) {
		dev_warn(&pdev->dev, "request %d Hz, force to set %d Hz\n",
			 speed_hz, rqspi->max_speed_hz);
		speed_hz = rqspi->max_speed_hz;
	}

	if (speed_hz < rqspi->min_speed_hz) {
		dev_err(&pdev->dev, "requested speed too low %d Hz\n",
			speed_hz);
		return -EINVAL;
	}

	if (rqspi->devtype & TYPE_FPGA)
		baudr = 8;
	else
		baudr = DIV_ROUND_UP(rqspi->spiclk_hz, speed_hz) / 2;
	if (baudr > SCKDV_MASK) {
		dev_err(&pdev->dev, "invalid baud reg: %08X\n", baudr);
		return -EINVAL;
	}
	rts_writel(rqspi, BAUDR, baudr);

	/* pin route & FIFO depth 2^fifo_entry Byte */
	reg = rts_readl(rqspi, CTRLR2);
	reg &= ~(BIT(SO_DNUM) | BIT(WPN_DNUM) | FIFO_ENTRY_MASK);
	reg |= BIT(SO_DNUM) | ((rqspi->fifo_entry) << FIFO_ENTRY);

	WARN_ON(rqspi->fifo_entry == 0);

	rts_writel(rqspi, CTRLR2, reg);

	if (IS_ENABLED(CONFIG_SPI_RTS_QUADSPI_IRQ)) {
		/* enable ACSIM interrupt */
		rts_writel(rqspi, IMR, 0x900);
		rqspi->timeout_ms = 5000;
	} else if (IS_ENABLED(CONFIG_SPI_RTS_QUADSPI_POLLING))
		/* Disable all interrupt */
		rts_writel(rqspi, IMR, 0);

	rts_writel(rqspi, SER, 1); /* cs actived */

	rts_writel(rqspi, WRITE_SINGLE, 0xBB);

	rqspi->nor.dev = &pdev->dev;
	rqspi->nor.controller_ops = &rts_qspi_ops;
	rqspi->nor.priv = rqspi;
	// rqspi->nor.flags |= SNOR_F_SWP_IS_VOLATILE | SNOR_F_HAS_LOCK;

	ret = of_property_read_string(np, "spi-transfer-channel",
				      &spi_transfer_mode);
	if (ret) {
		dev_err(&pdev->dev,
			"There's no spi-transfer-channel propert.\n");
		return -EINVAL;
	}

	hwcaps.mask = (SNOR_HWCAPS_READ | SNOR_HWCAPS_PP);

	if (!strcmp(spi_transfer_mode, "normal")) {
	} else if (!strcmp(spi_transfer_mode, "fast"))
		hwcaps.mask |= SNOR_HWCAPS_READ_FAST;
	else if (!strcmp(spi_transfer_mode, "dual"))
		hwcaps.mask |= SNOR_HWCAPS_READ_1_1_2;
	else if (!strcmp(spi_transfer_mode, "quad")) {
		hwcaps.mask |= SNOR_HWCAPS_READ_1_1_4;
		hwcaps.mask |= SNOR_HWCAPS_PP_1_1_4 | SNOR_HWCAPS_PP_1_4_4;
	} else if (!strcmp(spi_transfer_mode, "dtr")) {
		hwcaps.mask |= SNOR_HWCAPS_READ_1_4_4_DTR;
		hwcaps.mask |= SNOR_HWCAPS_PP_1_4_4;
	} else if (!strcmp(spi_transfer_mode, "qpi")) {
		hwcaps.mask |= SNOR_HWCAPS_READ_1_4_4 | SNOR_HWCAPS_READ_1_4_4;
		hwcaps.mask |= SNOR_HWCAPS_PP_1_4_4;
	} else {
		dev_err(&pdev->dev, "The %s mode is not supported.\n",
			spi_transfer_mode);
		return -EINVAL;
	}

	spi_nor_set_flash_node(&rqspi->nor, np->child);
	if ((!strcmp(spi_transfer_mode, "quad")) ||
	    (!strcmp(spi_transfer_mode, "dtr")) ||
	    (!strcmp(spi_transfer_mode, "qpi"))) {
		rqspi->pins.p = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(rqspi->pins.p)) {
			ret = PTR_ERR(rqspi->pins.p);
			return ret;
		}
		rqspi->pins.default_state =
			pinctrl_lookup_state(rqspi->pins.p, "default");
		if (IS_ERR(rqspi->pins.default_state)) {
			dev_err(&pdev->dev, "get default state fail\n");
			devm_pinctrl_put(rqspi->pins.p);
			ret = PTR_ERR(rqspi->pins.default_state);
			return ret;
		}
		pinctrl_select_state(rqspi->pins.p, rqspi->pins.default_state);
	}

	ret = spi_nor_scan(&rqspi->nor, NULL, &hwcaps);
	if (ret)
		return ret;

	spi_nor_debugfs_register(&rqspi->nor);

	return 0;
}

static void rts_qspi_init_reset_rom(struct rts_qspi *rqspi)
{
	struct spi_nor *nor = &rqspi->nor;
	u16 ctrl_reg[3];
	u16 cmd_ch;

	rts_writel(rqspi, FLUSH_FIFO, 0xffffffff);
	switch (nor->info->id[0]) {
	case SNOR_MFR_MACRONIX:
	case SNOR_MFR_GIGADEVICE:
	case SNOR_MFR_WINBOND:
	case SNOR_MFR_XMC:
	case SNOR_MFR_EON:
	case SNOR_MFR_BOYAMICRO:
	case SNOR_MFR_XTX:
	case SNOR_MFR_FM:
	case SNOR_MFR_PUYA:
	case SNOR_MFR_ZBIT:
		if (nor->read_proto == SNOR_PROTO_4_4_4)
			cmd_ch = QPI_CH;
		else
			cmd_ch = SPI_CH;

		ctrl_reg[0] = (COUNT_2EXP16 << PGMRST_COUNT) |
			      (STATE_NEXT << PGMRST_STATE) |
			      (cmd_ch << PGMRST_CMD_CH) | (SPINOR_OP_SRSTEN);
		ctrl_reg[1] = (COUNT_2EXP23 << PGMRST_COUNT) |
			      (STATE_NEXT << PGMRST_STATE) |
			      (cmd_ch << PGMRST_CMD_CH) | (SPINOR_OP_SRST);
		ctrl_reg[2] = (COUNT_2EXP16 << PGMRST_COUNT) |
			      (STATE_END << PGMRST_STATE) |
			      (SPI_CH << PGMRST_CMD_CH) | (0x00);

		writel((ctrl_reg[1] << 16) | ctrl_reg[0],
		       rqspi->spic_cfg_regs + SPIC_PGM_FIFO_INIT0);
		writel(ctrl_reg[2], rqspi->spic_cfg_regs + SPIC_PGM_FIFO_INIT1);
		break;
	default:
		break;
	}
}

static void rts_qspi_reset(struct rts_qspi *rqspi)
{
	struct spi_nor *nor = &rqspi->nor;

	nor->controller_ops->write_reg(nor, SPINOR_OP_SRSTEN, NULL, 0);
	nor->controller_ops->write_reg(nor, SPINOR_OP_SRST, NULL, 0);
}

static irqreturn_t rts_qspi_isr(int irq, void *dev_id)
{
	struct rts_qspi *rqspi = dev_id;
	u32 int_reg;

	if (!rqspi)
		return IRQ_NONE;

	spin_lock(&(rqspi->__lock));

	int_reg = rts_readl(rqspi, ISR);
	if (!int_reg) {
		spin_unlock(&(rqspi->__lock));
		return IRQ_NONE;
	}
	rts_writel(rqspi, ICR, 0); /* clear ICR */

	dev_dbg(&(rqspi->pdev->dev), "----- IRQ: 0x%08x -----\n", int_reg);

	if (int_reg & 0x900) {
		if (int_reg & 0x100)
			rqspi->status = CHECK_TIMEOUT;
		else
			rqspi->status = 0;
		if (rqspi->done)
			complete(rqspi->done);
	}

	spin_unlock(&(rqspi->__lock));

	return IRQ_HANDLED;
}

static int rts_qspi_acquire_irq(struct rts_qspi *rqspi)
{
	int err = 0;

	spin_lock_init(&rqspi->__lock);
	err = request_irq(rqspi->irq, rts_qspi_isr, IRQF_SHARED,
			  RTS_QSPI_DRV_NAME, rqspi);
	if (err)
		dev_err(&(rqspi->pdev->dev), "request IRQ %d failed\n",
			rqspi->irq);

	return err;
}

static const struct of_device_id rts_qspi_dt_ids[] = {
	{
		.compatible = "realtek,rts493xa-quadspi",
		.data = (void *)&rts493xa_data,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rts_qspi_dt_ids);

static int rts_qspi_probe(struct platform_device *pdev)
{
	struct resource *res, *spic_cfg_res;
	struct rts_qspi *rqspi;
	struct rts_qspi_devtype_data *qspi_devtype;
	const struct flash_platform_data *data;
	const struct of_device_id *of_id;
	int ret;

	of_id = of_match_device(rts_qspi_dt_ids, &pdev->dev);

	qspi_devtype = (void *)of_id->data;

	/* fpga board */
	if (of_machine_is_compatible("realtek,rts_fpga"))
		qspi_devtype->devtype = TYPE_FPGA;

	rqspi = devm_kzalloc(&pdev->dev, sizeof(*rqspi), GFP_KERNEL);
	if (!rqspi)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spic_cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!res || !spic_cfg_res) {
		dev_err(&pdev->dev, "get resource failed!\n");
		return -EINVAL;
	}

	res = devm_request_mem_region(&pdev->dev, res->start,
				      resource_size(res), pdev->name);
	spic_cfg_res = devm_request_mem_region(&pdev->dev, spic_cfg_res->start,
					       resource_size(spic_cfg_res),
					       pdev->name);
	if (!res || !spic_cfg_res) {
		dev_err(&pdev->dev, "request memory region failed!\n");
		return -ENOMEM;
	}

	rqspi->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	rqspi->spic_cfg_regs = devm_ioremap(&pdev->dev, spic_cfg_res->start,
					    resource_size(spic_cfg_res));
	if (!rqspi->regs || !rqspi->spic_cfg_regs) {
		dev_err(&pdev->dev, "ioremap failed, phy address:%x, %x\n",
			res->start, spic_cfg_res->start);
		return -ENOMEM;
	}

	if (qspi_devtype->devtype == TYPE_ASIC) {
		rqspi->clk = devm_clk_get(&pdev->dev, "spi_ck");
		if (IS_ERR(rqspi->clk)) {
			dev_err(&pdev->dev, "get clock failed!\n");
			return PTR_ERR(rqspi->clk);
		}

		rqspi->spiclk_hz = clk_get_rate(rqspi->clk);
		if (!rqspi->spiclk_hz) {
			dev_err(&pdev->dev, "get invalid clock rate\n");
			return -EINVAL;
		}
	} else
		rqspi->spiclk_hz = 25000000;

	/* work around for spi_nor_scan check */
	pdev->dev.platform_data = (struct flash_platform_data *)data;

	platform_set_drvdata(pdev, rqspi);
	rqspi->phybase = res->start;
	rqspi->use_dma = false;
	rqspi->fifo_size = qspi_devtype->fifo_size;
	rqspi->fifo_entry = qspi_devtype->fifo_entry;
	rqspi->max_speed_hz = DIV_ROUND_UP(rqspi->spiclk_hz, 1) / 2;
	rqspi->min_speed_hz =
		DIV_ROUND_UP(rqspi->spiclk_hz, ((1 << SCKDV_WIDTH) - 2)) / 2;
	rqspi->pdev = pdev;
	rqspi->devtype = qspi_devtype->devtype;

	if (IS_ENABLED(CONFIG_SPI_RTS_QUADSPI_IRQ)) {
		rqspi->irq = platform_get_irq(pdev, 0);
		if (rqspi->irq < 0) {
			dev_err(&pdev->dev, "get irq failed!\n");
			return -ENXIO;
		}
		ret = rts_qspi_acquire_irq(rqspi);
		if (ret < 0)
			return ret;
		dev_info(&pdev->dev,
			 "Realtek QSPI Controller at 0x%08lx (irq %d)\n",
			 (unsigned long)res->start, rqspi->irq);
		synchronize_irq(rqspi->irq);
	}

	/* Initialize the hardware */
	ret = rts_qspi_setup(rqspi, pdev->dev.of_node);
	if (ret)
		return ret;

	ret = mtd_device_register(&rqspi->nor.mtd, NULL, 0);
	if (ret)
		return ret;

	rts_qspi_init_reset_rom(rqspi);

	spi_nor_init_swp(&rqspi->nor);

	return 0;
}

static int rts_qspi_remove(struct platform_device *pdev)
{
	struct rts_qspi *rqspi = platform_get_drvdata(pdev);

	rts_qspi_reset(rqspi);

	mtd_device_unregister(&rqspi->nor.mtd);

	return 0;
}

static const struct platform_device_id rts_qspi_id_table[] = {
	{
		.name = "rts493xa-qspi",
		.driver_data = (kernel_ulong_t)&rts493xa_data,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, rts_qspi_id_table);

static struct platform_driver rts_qspi_driver = {
	.driver = {
		.name	= "rts-quadspi",
		.owner	= THIS_MODULE,
		.of_match_table = rts_qspi_dt_ids,
	},
	.id_table	= rts_qspi_id_table,
	.probe		= rts_qspi_probe,
	.remove		= rts_qspi_remove,
};
module_platform_driver(rts_qspi_driver);

MODULE_ALIAS("platform:rts-quadspi");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("QuadSPI nor flash controller driver for realtek soc");
