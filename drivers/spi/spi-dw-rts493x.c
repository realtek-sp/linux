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
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/property.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>

#include "spi-dw-rts493x.h"

static int rts_spi_dma_init(struct device *dev, struct dw_spi *dws)
{
	init_completion(&dws->dma_completion);

	return 0;
}

static void rts_spi_dma_exit(struct dw_spi *dws)
{
}

static irqreturn_t rts_spi_dma_transfer_handler(struct dw_spi *dws)
{
	struct dw_spi_rts *dwsrts = container_of(dws, struct dw_spi_rts, dws);

	dev_err(dwsrts->dev, "dma interrupt should go to rts_spi_dma_irq\n");

	return IRQ_NONE;
}

static int rts_spi_dma_setup(struct dw_spi *dws, struct spi_transfer *xfer)
{
	reinit_completion(&dws->dma_completion);

	dws->transfer_handler = rts_spi_dma_transfer_handler;

	return 0;
}

static bool rts_spi_can_dma(struct spi_controller *host, struct spi_device *spi,
			    struct spi_transfer *xfer)
{
	struct dw_spi *dws = spi_controller_get_devdata(host);

	return xfer->len > dws->fifo_len;
}

static int rts_spi_dma_wait(struct dw_spi *dws, unsigned int len, u32 speed)
{
	unsigned long long ms;

	ms = (u64)len * MSEC_PER_SEC * BITS_PER_BYTE;
	do_div(ms, speed);
	ms += ms + 200;

	if (ms > UINT_MAX)
		ms = UINT_MAX;

	ms = wait_for_completion_timeout(&dws->dma_completion,
					 msecs_to_jiffies(ms));

	if (ms == 0) {
		dev_err(&dws->host->cur_msg->spi->dev,
			"DMA transaction timed out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int rts_spi_dma_transfer(struct dw_spi *dws, struct spi_transfer *xfer)
{
	unsigned int base, len;
	unsigned int tx_len = 0, rx_len = 0;
	unsigned int tx_dma = 0, rx_dma = 0;
	struct scatterlist *tx_sg = NULL, *rx_sg = NULL;
	int ret;

	for (base = 0, len = 0; base < xfer->len; base += len) {
		/* Fetch next Tx DMA data chunk */
		if (!tx_len && xfer->tx_buf) {
			tx_sg = !tx_sg ? &xfer->tx_sg.sgl[0] : sg_next(tx_sg);
			tx_dma = sg_dma_address(tx_sg);
			tx_len = sg_dma_len(tx_sg);
		}

		/* Fetch next Rx DMA data chunk */
		if (!rx_len && xfer->rx_buf) {
			rx_sg = !rx_sg ? &xfer->rx_sg.sgl[0] : sg_next(rx_sg);
			rx_dma = sg_dma_address(rx_sg);
			rx_len = sg_dma_len(rx_sg);
		}

		if (xfer->tx_buf && xfer->rx_buf)
			len = min(tx_len, rx_len);
		else if (xfer->tx_buf)
			len = tx_len;
		else if (xfer->rx_buf)
			len = rx_len;

		dw_writel(dws, SSI_RD_ADDR, tx_dma);
		dw_writel(dws, SSI_WR_ADDR, rx_dma);

		dw_writel(dws, SSI_DATA_LEN, len);
		/* Set the interrupt mask */
		dw_writel(dws, SSI_IRQ_STATUS, SSI_DONE_INT);
		dw_writel(dws, SSI_IRQ_ENABLE, DONE_INT_EN);
		/* start */
		dw_writel(dws, SSI_START, SPI_SSI_START);

		ret = rts_spi_dma_wait(dws, len, xfer->effective_speed_hz);
		if (ret)
			break;

		dw_writel(dws, SSI_STOP, SPI_SSI_STOP);

		reinit_completion(&dws->dma_completion);

		tx_dma += xfer->tx_buf ? len : 0;
		rx_dma += xfer->rx_buf ? len : 0;
		tx_len -= xfer->tx_buf ? len : 0;
		rx_len -= xfer->rx_buf ? len : 0;
	}

	return 0;
}

static void rts_spi_dma_stop(struct dw_spi *dws)
{
	/* stop */
	dw_writel(dws, SSI_IRQ_ENABLE, DONE_INT_DIS);
	dw_writel(dws, SSI_STOP, SPI_SSI_STOP);
}

static const struct dw_spi_dma_ops rts_dma_ops = {
	.dma_init = rts_spi_dma_init,
	.dma_exit = rts_spi_dma_exit,
	.dma_setup = rts_spi_dma_setup,
	.can_dma = rts_spi_can_dma,
	.dma_transfer = rts_spi_dma_transfer,
	.dma_stop = rts_spi_dma_stop,
};

static irqreturn_t rts_spi_dma_irq(int irq, void *data)
{
	struct dw_spi *dws = data;
	struct spi_controller *host = dws->host;
	u16 irq_dma_status = dw_readl(dws, SSI_IRQ_STATUS) & SSI_DONE_INT;

	if (!irq_dma_status)
		return IRQ_NONE;

	if (!host->cur_msg) {
		dw_writel(dws, SSI_IRQ_ENABLE, DONE_INT_DIS);
		return IRQ_HANDLED;
	}

	dw_writel(dws, SSI_IRQ_STATUS, SSI_DONE_INT);

	complete(&dws->dma_completion);

	return IRQ_HANDLED;
}

void rts_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct dw_spi *dws = spi_controller_get_devdata(spi->controller);
	struct dw_spi_rts *dwsrts = container_of(dws, struct dw_spi_rts, dws);
	bool cs_high = !!(spi->mode & SPI_CS_HIGH);

	if (cs_high == enable) {
		dw_writel(dws, DW_SPI_SER, BIT(0));
		rts_spi_writel_pad(dwsrts, DE_SSI_CS_SEL,
				   spi_get_chipselect(spi, 0));
	} else {
		dw_writel(dws, DW_SPI_SER, 0);
	}
}

static int dw_spi_rts_probe(struct platform_device *pdev)
{
	struct dw_spi_rts *dwsrts;
	struct dw_spi *dws;
	struct resource *mem;
	int ret, num_cs;
	struct device *dev = &pdev->dev;

	dwsrts = devm_kzalloc(dev, sizeof(struct dw_spi_rts), GFP_KERNEL);
	if (!dwsrts)
		return -ENOMEM;

	dwsrts->dev = dev;
	dws = &dwsrts->dws;

	dws->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &mem);
	if (IS_ERR(dws->regs))
		return PTR_ERR(dws->regs);

	dws->paddr = mem->start;

	dwsrts->reg_pad = devm_platform_get_and_ioremap_resource(pdev, 1, NULL);
	if (IS_ERR(dwsrts->reg_pad))
		return PTR_ERR(dwsrts->reg_pad);

	dws->irq = platform_get_irq(pdev, 0);
	if (dws->irq < 0) {
		dev_err(dev, "no irq resource?\n");
		return dws->irq; /* -ENXIO */
	}

	ret = devm_request_irq(&pdev->dev, dws->irq, rts_spi_dma_irq,
			       IRQF_SHARED, dev_name(&pdev->dev), dws);
	if (ret) {
		dev_err(&pdev->dev, "can not get IRQ\n");
		return ret;
	}

	dwsrts->clk = devm_clk_get(dev, "ssi_ck");
	if (IS_ERR(dwsrts->clk))
		return PTR_ERR(dwsrts->clk);
	ret = clk_prepare_enable(dwsrts->clk);
	if (ret)
		return ret;

	dws->bus_num = pdev->id;

	dws->max_freq = clk_get_rate(dwsrts->clk);

	if (device_property_read_u32(dev, "reg-io-width", &dws->reg_io_width))
		dws->reg_io_width = 4;

	if (device_property_read_u32(dev, "num-cs", &num_cs)) {
		dev_err(dev, "could not find num-cs\n");
		num_cs = 4;
	}

	dws->num_cs = num_cs;
	dws->set_cs = rts_spi_set_cs;
	dws->dma_ops = &rts_dma_ops;

	pm_runtime_enable(&pdev->dev);

	/*enable ssi pad select*/
	rts_spi_writel_pad(dwsrts, DW_SSI_PAD_SEL, 1);

	ret = dw_spi_add_host(dev, dws);
	if (ret)
		goto out;

	platform_set_drvdata(pdev, dwsrts);

	dev_info(dev, "Realtek SSI Controller at 0x%08lx (irq %d)\n",
		 (unsigned long)mem->start, dws->irq);

	return 0;

out:
	clk_disable_unprepare(dwsrts->clk);
	return ret;
}

static void dw_spi_rts_remove(struct platform_device *pdev)
{
	struct dw_spi_rts *dwsrts = platform_get_drvdata(pdev);

	clk_disable_unprepare(dwsrts->clk);
	dw_spi_remove_host(&dwsrts->dws);
}

static const struct of_device_id dw_spi_rts_of_match[] = {
	{
		.compatible = "realtek,dw-apb-ssi",
	},
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, dw_spi_rts_of_match);

static struct platform_driver dw_spi_rts_driver = {
	.probe		= dw_spi_rts_probe,
	.remove_new	= dw_spi_rts_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table = dw_spi_rts_of_match,
	},
};
module_platform_driver(dw_spi_rts_driver);

MODULE_DESCRIPTION("RTS493xA SSI Driver");
MODULE_LICENSE("GPL");
