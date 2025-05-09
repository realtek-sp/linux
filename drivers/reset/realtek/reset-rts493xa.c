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
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <asm/io.h>

#include <dt-bindings/reset/realtek,rts493xa-reset.h>

#define FORCE_BUS_SD1_RESET    BIT(9)
#define FORCE_BUS_SD0_RESET    BIT(7)
#define FORCE_BUS_U2DEV_RESET  BIT(5)
#define FORCE_BUS_U2HOST_RESET BIT(4)
#define FORCE_BUS_RESET	       BIT(3)
#define FORCE_XB2_RESET	       BIT(2)
#define FORCE_CPU_RESET	       BIT(1)
#define FORCE_DRAM_RESET       BIT(0)

#define FORCE_BUS_SHA256_RESET	BIT(12)
#define FORCE_DRAM_RSA_RESET	BIT(11)
#define FORCE_FEPHY_RESET	BIT(5)
#define FORCE_RTC32K_RESET	BIT(3)
#define FORCE_U2DEV_UTMI_RESET	BIT(2)
#define FORCE_U2HOST_UTMI_RESET BIT(1)

#define FORCE_UART2_CLK_ASYNC_RESET    BIT(23)
#define FORCE_UART1_CLK_ASYNC_RESET    BIT(22)
#define FORCE_OTP_CLK_ASYNC_RESET      BIT(17)
#define FORCE_TRNG_CLK_ASYNC_RESET     BIT(16)
#define FORCE_SD1_CLK_ASYNC_RESET      BIT(15)
#define FORCE_I2C1_CLK_ASYNC_RESET     BIT(9)
#define FORCE_I2C0_CLK_ASYNC_RESET     BIT(8)
#define FORCE_UART0_CLK_ASYNC_RESET    BIT(7)
#define FORCE_ETHERNET_CLK_ASYNC_RESET BIT(6)
#define FORCE_SD0_CLK_ASYNC_RESET      BIT(5)
#define FORCE_CIPHER_CLK_ASYNC_RESET   BIT(4)

struct rts_force_reset_regs {
	u32 force_reg_reset;
	u32 force_reg_reset_fwc;
	u32 force_reg_async_reset;
};

struct rts_reset_data {
	struct mutex lock;
	struct rts_force_reset_regs __iomem *regs;
	struct reset_controller_dev rcdev;
};

#define ALL_MASK 0xFFFFFFFF

#define RTS_FRR_SET(addr, mask)      \
	do {                         \
		u32 val;             \
		val = readl((addr)); \
		val |= (mask);       \
		writel(val, (addr)); \
	} while (0)

#define RTS_FRR_CLR(addr, mask)      \
	do {                         \
		u32 val;             \
		val = readl((addr)); \
		val &= ~(mask);      \
		writel(val, (addr)); \
	} while (0)

#define RTS_FORCE_RESET_AUTO(addr, mask) RTS_FRR_SET(addr, mask)

#define RTS_FORCE_RESET(addr, mask)      \
	do {                             \
		RTS_FRR_SET(addr, mask); \
		RTS_FRR_CLR(addr, mask); \
	} while (0)

static int rts_sys_force_reset(struct reset_controller_dev *rcdev,
			       unsigned long id)
{
	struct rts_reset_data *rdata =
		container_of(rcdev, struct rts_reset_data, rcdev);

	struct rts_force_reset_regs *regs = rdata->regs;

	mutex_lock(&rdata->lock);

	switch (id) {
	case FORCE_RESET_CIPHER:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_CIPHER_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_SDIO0:
		RTS_FORCE_RESET_AUTO(&regs->force_reg_reset,
				     FORCE_BUS_SD0_RESET);
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_SD0_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_SDIO1:
		RTS_FORCE_RESET_AUTO(&regs->force_reg_reset,
				     FORCE_BUS_SD1_RESET);
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_SD1_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_U2DEV:
		RTS_FORCE_RESET_AUTO(&regs->force_reg_reset,
				     FORCE_BUS_U2DEV_RESET);
		RTS_FORCE_RESET(&regs->force_reg_reset_fwc,
				FORCE_U2DEV_UTMI_RESET);
		break;

	case FORCE_RESET_U2HOST:
		RTS_FORCE_RESET_AUTO(&regs->force_reg_reset,
				     FORCE_BUS_U2HOST_RESET);
		RTS_FORCE_RESET(&regs->force_reg_reset_fwc,
				FORCE_U2HOST_UTMI_RESET);
		break;

	case FORCE_RESET_ETHERNET:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_ETHERNET_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_RSA:
		RTS_FORCE_RESET(&regs->force_reg_reset_fwc,
				FORCE_DRAM_RSA_RESET);
		break;

	case FORCE_RESET_SHA256:
		RTS_FORCE_RESET(&regs->force_reg_reset_fwc,
				FORCE_BUS_SHA256_RESET);
		break;

	case FORCE_RESET_TRNG:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_TRNG_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_FEPHY:
		RTS_FORCE_RESET(&regs->force_reg_reset_fwc, FORCE_FEPHY_RESET);
		break;

	case FORCE_RESET_OTP:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_OTP_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_UART0:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_UART0_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_UART1:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_UART1_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_UART2:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_UART2_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_I2C0:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_I2C0_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_I2C1:
		RTS_FORCE_RESET(&regs->force_reg_async_reset,
				FORCE_I2C1_CLK_ASYNC_RESET);
		break;

	default:
		pr_info("ERROR: invalid reset model %ld\n", id);
		break;
	}

	mutex_unlock(&rdata->lock);

	return 0;
}

static int rts_sys_reset_deassert(struct reset_controller_dev *rcdev,
				  unsigned long id)
{
	struct rts_reset_data *rdata =
		container_of(rcdev, struct rts_reset_data, rcdev);

	struct rts_force_reset_regs *regs = rdata->regs;

	mutex_lock(&rdata->lock);

	switch (id) {
	case FORCE_RESET_CIPHER:
		RTS_FRR_CLR(&regs->force_reg_async_reset,
			    FORCE_CIPHER_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_ETHERNET:
		RTS_FRR_CLR(&regs->force_reg_async_reset,
			    FORCE_ETHERNET_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_RSA:
		RTS_FRR_CLR(&regs->force_reg_reset_fwc, FORCE_DRAM_RSA_RESET);
		break;

	case FORCE_RESET_SHA256:
		RTS_FRR_CLR(&regs->force_reg_reset_fwc, FORCE_BUS_SHA256_RESET);
		break;

	case FORCE_RESET_FEPHY:
		RTS_FRR_CLR(&regs->force_reg_reset_fwc, FORCE_FEPHY_RESET);
		break;

	default:
		pr_info("ERROR: invalid deassert model %ld\n", id);
		break;
	}

	mutex_unlock(&rdata->lock);

	return 0;
}

static int rts_sys_reset_assert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	struct rts_reset_data *rdata =
		container_of(rcdev, struct rts_reset_data, rcdev);

	struct rts_force_reset_regs *regs = rdata->regs;

	mutex_lock(&rdata->lock);

	switch (id) {
	case FORCE_RESET_CIPHER:
		RTS_FRR_SET(&regs->force_reg_async_reset,
			    FORCE_CIPHER_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_ETHERNET:
		RTS_FRR_SET(&regs->force_reg_async_reset,
			    FORCE_ETHERNET_CLK_ASYNC_RESET);
		break;

	case FORCE_RESET_RSA:
		RTS_FRR_SET(&regs->force_reg_reset_fwc, FORCE_DRAM_RSA_RESET);
		break;

	case FORCE_RESET_SHA256:
		RTS_FRR_SET(&regs->force_reg_reset_fwc, FORCE_BUS_SHA256_RESET);
		break;

	case FORCE_RESET_FEPHY:
		RTS_FRR_SET(&regs->force_reg_reset_fwc, FORCE_FEPHY_RESET);
		break;

	default:
		pr_info("ERROR: invalid assert model %ld\n", id);
		break;
	}

	mutex_unlock(&rdata->lock);

	return 0;
}

static const struct reset_control_ops rlx_reset_ops = {
	.reset = rts_sys_force_reset,
	.assert = rts_sys_reset_assert,
	.deassert = rts_sys_reset_deassert,
};

static void rts_force_reset_hw_init(struct rts_force_reset_regs *regs)
{
	RTS_FRR_SET(&regs->force_reg_reset_fwc,
		    FORCE_FEPHY_RESET | FORCE_U2DEV_UTMI_RESET |
			    FORCE_U2HOST_UTMI_RESET);
}

static int rts_reset_probe(struct platform_device *pdev)
{
	struct rts_reset_data *rdata;
	struct resource *res;

	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata), GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	mutex_init(&rdata->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rdata->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rdata->regs))
		return PTR_ERR(rdata->regs);

	rdata->rcdev.owner = THIS_MODULE;
	rdata->rcdev.nr_resets = FORCE_RESET_MAX;
	rdata->rcdev.ops = &rlx_reset_ops;
	rdata->rcdev.of_node = pdev->dev.of_node;

	rts_force_reset_hw_init(rdata->regs);

	return devm_reset_controller_register(&pdev->dev, &rdata->rcdev);
}

static int rts_reset_remove(struct platform_device *pdev)
{
	struct rts_reset_data *rdata = platform_get_drvdata(pdev);

	reset_controller_unregister(&rdata->rcdev);

	return 0;
}

static const struct of_device_id rts_reset_dt_ids[] = {
	{
		.compatible = "realtek,rts493xa-reset",
	},
	{ /* sentinel */ },
};

static struct platform_driver rts_reset_driver = {
	.probe	= rts_reset_probe,
	.remove	= rts_reset_remove,
	.driver = {
		.name		= "rts-reset",
		.of_match_table	= of_match_ptr(rts_reset_dt_ids),
	},
};

static int __init rts_reset_init(void)
{
	return platform_driver_register(&rts_reset_driver);
}
postcore_initcall(rts_reset_init);

static void __exit rts_reset_exit(void)
{
	platform_driver_unregister(&rts_reset_driver);
}
module_exit(rts_reset_exit);
