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

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/mfd/iomatrix.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "kcs_bmc_device.h"

#define KCS_STATUS_REGISTER 0x00

#define RESERVED_MASK GENMASK(31, 8)

#define STS4_MASK    BIT(7)
#define STS4_DISABLE 0
#define STS4_ENABLE  BIT(7)

#define STS3_MASK    BIT(6)
#define STS3_DISABLE 0
#define STS3_ENABLE  BIT(6)

#define STS2_MASK    BIT(5)
#define STS2_DISABLE 0
#define STS2_ENABLE  BIT(5)

#define STS1_MASK    BIT(4)
#define STS1_DISABLE 0
#define STS1_ENABLE  BIT(4)

#define CMDSEL_MASK BIT(3)
#define CMDSEL_68   0
#define CMDSEL_6C   BIT(3)

#define STS0_MASK    BIT(2)
#define STS0_DISABLE 0
#define STS0_ENABLE  BIT(2)

#define IBF_MASK     BIT(1)
#define IBF_NOT_FULL 0
#define IBF_FULL     BIT(1)

#define OBF_MASK     BIT(0)
#define OBF_NOT_FULL 0
#define OBF_FULL     BIT(0)

#define KCS_INPUT_BUFFER_REGISTER 0x04

#define IN_RESERVED_MASK GENMASK(31, 9)

#define IBCLR_MASK  BIT(8)
#define IBCLR_CLEAR BIT(8)

#define IBDAT_MASK   GENMASK(7, 0)
#define IBDAT_OFFSET 0

#define KCS_OUTPUT_BUFFER_REGISTER 0x08

#define OUT_RESERVED_MASK GENMASK(31, 9)

#define OBCLR_MASK  BIT(8)
#define OBCLR_CLEAR BIT(8)

#define OBDAT_MASK   GENMASK(7, 0)
#define OBDAT_OFFSET 0

#define KCS_ADDRESS_REGISTER 0x0C

#define ADDR_RESERVED_MASK GENMASK(31, 15)

#define CMDOFS_MASK   GENMASK(14, 12)
#define CMDOFS_OFFSET 12

#define DATAADDR_MASK	GENMASK(11, 0)
#define DATAADDR_OFFSET 0

#define KCS_INTERRUPT_ENABLE_REGISTER 0x10

#define INT_RESERVED_MASK GENMASK(31, 2)

#define IBFINTEN_MASK	 BIT(1)
#define IBFINTEN_DISABLE 0
#define IBFINTEN_ENABLE	 BIT(1)

#define OBFINTEN_MASK	 BIT(0)
#define OBFINTEN_DISABLE 0
#define OBFINTEN_ENABLE	 BIT(0)

#define KCS_VWCTRL0_REGISTER 0x14

#define VW_RESERVED_MASK GENMASK(31, 2)

#define TGLV_MASK  BIT(1)
#define TGLV_LEVEL BIT(1)

#define IRQEN_MASK    BIT(0)
#define IRQEN_DISABLE 0
#define IRQEN_ENABLE  BIT(0)

#define KCS_VWCTRL1_REGISTER 0x18

#define VW1_RESERVED_MASK GENMASK(31, 9)

#define ACTSEL_MASK    BIT(8)
#define ACTSEL_DISABLE 0
#define ACTSEL_ENABLE  BIT(8)

#define IRQNUM_MASK   GENMASK(7, 0)
#define IRQNUM_OFFSET 0

#define DEVICE_NAME	"rts591x-kcs-bmc"
#define KCS_CHANNEL_MAX 4

#define OBE_POLL_PERIOD (HZ)

struct rts591x_kcs_bmc {
	struct kcs_bmc_device kcs_bmc;

	struct regmap *regmap;
	phys_addr_t base;
	u32 chan;

	struct {
		spinlock_t lock;
		bool enabled;
		bool remove;
		struct delayed_work work;
	} obe;
};

static inline struct rts591x_kcs_bmc *
to_rts591x_kcs_bmc(struct kcs_bmc_device *kcs_bmc)
{
	return container_of(kcs_bmc, struct rts591x_kcs_bmc, kcs_bmc);
}

static u8 rts591x_kcs_inb(struct kcs_bmc_device *kcs_bmc, u32 reg)
{
	struct rts591x_kcs_bmc *priv = to_rts591x_kcs_bmc(kcs_bmc);
	u32 val = 0;
	int rc;

	rc = regmap_read(priv->regmap, reg, &val);
	WARN(rc != 0, "regmap_read() failed: %d\n", rc);

	return rc == 0 ? (u8)val : 0;
}

static void rts591x_kcs_outb(struct kcs_bmc_device *kcs_bmc, u32 reg, u8 data)
{
	struct rts591x_kcs_bmc *priv = to_rts591x_kcs_bmc(kcs_bmc);
	int rc;

	rc = regmap_write(priv->regmap, reg, data);
	WARN(rc != 0, "regmap_write() failed: %d\n", rc);
}

static void rts591x_kcs_updateb(struct kcs_bmc_device *kcs_bmc, u32 reg,
				u8 mask, u8 val)
{
	struct rts591x_kcs_bmc *priv = to_rts591x_kcs_bmc(kcs_bmc);
	int rc;

	rc = regmap_update_bits(priv->regmap, reg, mask, val);
	WARN(rc != 0, "regmap_update_bits() failed: %d\n", rc);
}

static void rts591x_kcs_enable_channel(struct kcs_bmc_device *kcs_bmc,
				       bool enable)
{
	struct rts591x_kcs_bmc *priv = to_rts591x_kcs_bmc(kcs_bmc);

	regmap_update_bits(priv->regmap, priv->base + KCS_VWCTRL1_REGISTER,
			   ACTSEL_MASK, enable * ACTSEL_MASK);
}

static void rts591x_kcs_check_obe(struct work_struct *work)
{
	struct rts591x_kcs_bmc *priv = container_of(
		to_delayed_work(work), struct rts591x_kcs_bmc, obe.work);
	unsigned long flags;
	u8 str;

	spin_lock_irqsave(&priv->obe.lock, flags);
	if (!priv->obe.enabled || priv->obe.remove) {
		spin_unlock_irqrestore(&priv->obe.lock, flags);
		return;
	}
	spin_unlock_irqrestore(&priv->obe.lock, flags);

	str = rts591x_kcs_inb(&priv->kcs_bmc, priv->kcs_bmc.ioreg.str);
	if (str & KCS_BMC_STR_OBF) {
		spin_lock_irqsave(&priv->obe.lock, flags);
		if (priv->obe.enabled && !priv->obe.remove)
			mod_delayed_work(system_wq, &priv->obe.work,
					 OBE_POLL_PERIOD);
		spin_unlock_irqrestore(&priv->obe.lock, flags);
		return;
	}

	kcs_bmc_handle_event(&priv->kcs_bmc);
}

static void rts591x_kcs_irq_mask_update(struct kcs_bmc_device *kcs_bmc, u8 mask,
					u8 state)
{
	struct rts591x_kcs_bmc *priv = to_rts591x_kcs_bmc(kcs_bmc);
	unsigned long flags;

	if (mask & KCS_BMC_EVENT_TYPE_OBE) {
		bool enable = state & KCS_BMC_EVENT_TYPE_OBE;

		spin_lock_irqsave(&priv->obe.lock, flags);
		priv->obe.enabled = enable;
		spin_unlock_irqrestore(&priv->obe.lock, flags);

		if (enable)
			mod_delayed_work(system_wq, &priv->obe.work, 0);
		else
			cancel_delayed_work(&priv->obe.work);
	}

	if (mask & KCS_BMC_EVENT_TYPE_IBF)
		regmap_update_bits(priv->regmap,
				   priv->base + KCS_INTERRUPT_ENABLE_REGISTER,
				   IBFINTEN_MASK,
				   !!(state & KCS_BMC_EVENT_TYPE_IBF) *
					   IBFINTEN_ENABLE);
}

static const struct kcs_bmc_device_ops rts591x_kcs_ops = {
	.irq_mask_update = rts591x_kcs_irq_mask_update,
	.io_inputb = rts591x_kcs_inb,
	.io_outputb = rts591x_kcs_outb,
	.io_updateb = rts591x_kcs_updateb,
};

static irqreturn_t rts591x_kcs_irq(int irq, void *arg)
{
	struct kcs_bmc_device *kcs_bmc = arg;

	return kcs_bmc_handle_event(kcs_bmc);
}

static int rts591x_kcs_probe(struct platform_device *pdev)
{
	struct kcs_bmc_device *kcs_bmc;
	struct rts591x_kcs_bmc *priv;
	struct rts591x_mfd_dev *mfd_dev;
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	int rc, irq;

	mfd_dev = dev_get_drvdata(parent);
	if (!mfd_dev) {
		dev_err(dev, "cannot get parent MFD device data\n");
		return -ENODEV;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->regmap = dev_get_regmap(parent, NULL);
	if (!priv->regmap)
		return -ENODEV;

	rc = of_property_read_u32(dev->of_node, "kcs_chan", &priv->chan);
	if (rc != 0 || priv->chan == 0 || priv->chan > KCS_CHANNEL_MAX) {
		dev_err(dev, "no valid 'kcs_chan' configured\n");
		return -ENODEV;
	}

	rc = device_property_read_u32(dev, "reg", &priv->base);
	if (rc) {
		dev_err(dev, "failed to get kcs %u base address\n", priv->chan);
		return -ENODEV;
	}

	kcs_bmc = &priv->kcs_bmc;
	kcs_bmc->dev = &pdev->dev;
	kcs_bmc->channel = priv->chan;
	kcs_bmc->ioreg.idr = priv->base + KCS_INPUT_BUFFER_REGISTER;
	kcs_bmc->ioreg.odr = priv->base + KCS_OUTPUT_BUFFER_REGISTER;
	kcs_bmc->ioreg.str = priv->base + KCS_STATUS_REGISTER;
	kcs_bmc->ops = &rts591x_kcs_ops;
	kcs_bmc->io_can_sleep = true;

	spin_lock_init(&priv->obe.lock);
	priv->obe.enabled = false;
	priv->obe.remove = false;
	INIT_DELAYED_WORK(&priv->obe.work, rts591x_kcs_check_obe);

	if (!mfd_dev->irq_data) {
		dev_err(dev, "parent MFD has no IRQ domain\n");
		return -ENODEV;
	}

	irq = regmap_irq_get_virq(mfd_dev->irq_data, RTS591X_KCS_IBF_INT);
	if (irq < 0) {
		dev_err(dev, "Failed to get IRQ: %d\n", irq);
		return irq;
	}

	/* Host to BMC IRQ */
	rc = devm_request_threaded_irq(dev, irq, NULL, rts591x_kcs_irq,
				       IRQF_ONESHOT, dev_name(dev), kcs_bmc);
	if (rc) {
		dev_err(dev, "Failed to request IRQ: %d\n", rc);
		return rc;
	}

	platform_set_drvdata(pdev, priv);

	rts591x_kcs_irq_mask_update(
		kcs_bmc, (KCS_BMC_EVENT_TYPE_IBF | KCS_BMC_EVENT_TYPE_OBE), 0);
	rts591x_kcs_enable_channel(kcs_bmc, true);

	rc = kcs_bmc_add_device(&priv->kcs_bmc);
	if (rc) {
		dev_err(dev, "Failed to register channel %d: %d\n",
			kcs_bmc->channel, rc);
		rts591x_kcs_enable_channel(kcs_bmc, false);
		rts591x_kcs_irq_mask_update(
			kcs_bmc,
			KCS_BMC_EVENT_TYPE_IBF | KCS_BMC_EVENT_TYPE_OBE, 0);
		spin_lock_irq(&priv->obe.lock);
		priv->obe.enabled = false;
		priv->obe.remove = true;
		spin_unlock_irq(&priv->obe.lock);
		cancel_delayed_work_sync(&priv->obe.work);
		return rc;
	}

	dev_info(dev, "Initialised channel %d at 0x%x\n", kcs_bmc->channel,
		 priv->base);

	return 0;
}

static int rts591x_kcs_remove(struct platform_device *pdev)
{
	struct rts591x_kcs_bmc *priv = platform_get_drvdata(pdev);
	struct kcs_bmc_device *kcs_bmc = &priv->kcs_bmc;

	kcs_bmc_remove_device(kcs_bmc);

	rts591x_kcs_enable_channel(kcs_bmc, false);
	rts591x_kcs_irq_mask_update(
		kcs_bmc, (KCS_BMC_EVENT_TYPE_IBF | KCS_BMC_EVENT_TYPE_OBE), 0);

	/* Make sure it's proper dead */
	spin_lock_irq(&priv->obe.lock);
	priv->obe.enabled = false;
	priv->obe.remove = true;
	spin_unlock_irq(&priv->obe.lock);
	cancel_delayed_work_sync(&priv->obe.work);

	return 0;
}

static const struct of_device_id rts591x_kcs_bmc_match[] = {
	{ .compatible = "realtek,rts591x-kcs-bmc" },
	{}
};
MODULE_DEVICE_TABLE(of, rts591x_kcs_bmc_match);

static struct platform_driver rts591x_kcs_bmc_driver = {
	.driver = {
		.name           = DEVICE_NAME,
		.of_match_table = rts591x_kcs_bmc_match,
	},
	.probe  = rts591x_kcs_probe,
	.remove = rts591x_kcs_remove,
};
module_platform_driver(rts591x_kcs_bmc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTS591x KCS Sub-Driver Using MFD");
