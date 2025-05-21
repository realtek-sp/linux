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
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <asm/ioctl.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/notifier.h>

enum {
	TYPE_RTS493XA = 1,

	TYPE_FPGA = (1 << 16),
};

struct rts_usb_phy_regs {
	u32 phy_mdio;

	u32 aphy_cfg1;
#define UPHY_HST_SEN_HST_SHIFT 27
#define UPHY_HST_SEN_HST_MASK  0x0F
#define UPHY_HST_SEN_SHIFT     23
#define UPHY_HST_SEN_MASK      0x0F
#define UPHY_HST_SEN_OBJ       8
#define UPHY_HST_SEN_OBJ_SHIFT 4

	u32 aphy_cfg2;
#define UPHY_HST_SENS_CHIRP	  0xF
#define UPHY_HST_SENS_CHIRP_SHIFT 13
#define UPHY_HST_HS_CKSEL	  0x08
#define UPHY_HST_CAL		  (1 << 3)
#define UPHY_HST_SRC_MASK	  0x07

	u32 aphy_cfg3;
#define UPHY_DEV_SEN_SHIFT	  23
#define UPHY_DEV_SEN_MASK	  0x0F
#define UPHY_DEV_SEN_OBJ	  8
#define UPHY_DEV_SEN_OBJ_SHIFT	  0
#define UPHY_DEV_SEN_NORMAL	  8
#define UPHY_DEV_SEN_NORMAL_SHIFT 8

	u32 aphy_cfg4;
#define UPHY_DEV_CAL	  (1 << 3)
#define UPHY_DEV_SRC_MASK 0x07

	u32 aphy_cfg5;
#define UPHY_ANA_AUTO_K	   (1 << 16)
#define UPHY_Z0_CODE_MASK  0x0F
#define UPHY_Z0_CODE_SHIFT 8

	u32 aphy_cfg6;
#define UPHY_U2_APHY_RST_N (1 << 20)

	u32 dphy_cfg1;

	u32 dphy_cfg2;
#define UPHY_U2_DPHY_CHECK_CHIRPK 0x100

	u32 host_cfg;
#define UPHY_PORT_OC_EN		      0x01
#define UPHY_HST_FORCE_PORT_PWR_EN    0x02
#define UPHY_HST_AUTO_PPD_ON_OC	      0x08
#define UPHY_PORT_PWR_POLARITY_SEL_HI 0x20000
#define UPHY_PORT_OC_POLARITY_SEL_HI  0x10000
};

#define RTS_Z0_CODE 9
#define RTS_DEV_SEN 2
#define RTS_HST_SEN 2
#define RTS_SRC_VAL 4

struct rts_usb_phy {
	struct usb_phy phy;
	struct device *dev;
	struct rts_usb_phy_regs __iomem *regs;

	struct mutex mutex;
	int device_type;
	unsigned int irq;
	int host_sens_obj;
	int host_sens_chirp;
	int dev_sens_obj;
	int dev_sens_nor;
#ifdef CONFIG_USB_RTS_PHY_DEBUG
	struct cdev cdev;
#endif
};

#define DEV_PORT 0
#define HST_PORT 1

#define PORT_CFG0(port) ((port) << 4)
#define PORT_CFG1(port) (((port) << 4) | 0x01)
#define PORT_CFG2(port) (((port) << 4) | 0x02)
#define PORT_CFG3(port) (((port) << 4) | 0x03)

#define USB2_CLK_CFG0 0x100
#define USB2_CLK_CFG1 0x101
#define USB2_CLK_CFG2 0x102
#define USB2_CLK_CFG3 0x103

#define APHY_RPDEN 0x40

#define APHY_CALEN_DEV	      0x10
#define APHY_NSQ_DEV	      0x20
#define APHY_CALEN_HST	      0x400
#define APHY_NSQ_HST	      0x800
#define APHY_SD_CAL_DEV_SHIFT 0
#define APHY_SD_CAL_DEV_MASK  0x0F
#define APHY_SD_CAL_DEV_INIT  (0x08 << APHY_SD_CAL_DEV_SHIFT)
#define APHY_SD_CAL_HST_SHIFT 6
#define APHY_SD_CAL_HST_MASK  0x3C0
#define APHY_SD_CAL_HST_INIT  (0x08 << APHY_SD_CAL_HST_SHIFT)
#define APHY_CALEN_INIT	      (APHY_SD_CAL_HST_INIT | APHY_SD_CAL_DEV_INIT)

/*
 *    Bit 3:  MDO (MISO)
 *    Bit 2:  MDI OE (MOSI OE)
 *    Bit 1:  MDI (MOSI)
 *    Bit 0:  MDC
 */

#define MDI_H 0x02
#define MDI_L 0x00
#define MDO_H 0x08
#define MDO_L 0x00
#define MDC_H 0x01
#define MDC_L 0x00
#define MD_OE 0x04

#define MDC_MASK 0x01
#define MDI_MASK 0x02
#define MDO_MASK 0x08

/* Push data at falling edge */
#define MDI_CLK_W(rts, init, bit)                     \
	do {                                          \
		(init) |= MD_OE;                      \
		(init) |= MDC_H;                      \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(1);                            \
		(init) &= ~(MDC_MASK | MDI_MASK);     \
		(init) |= (bit);                      \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(5);                            \
	} while (0)

/* Push data at rising edge */
#define MDI_CLK_W_RISING(rts, init, bit)              \
	do {                                          \
		(init) |= MD_OE;                      \
		(init) |= MDC_H;                      \
		writel(init, &(rts)->regs->phy_mdio); \
		(init) &= ~MDI_MASK;                  \
		(init) |= (bit);                      \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(1);                            \
		(init) &= ~MDC_MASK;                  \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(5);                            \
	} while (0)

/* Sample data at falling edge */
#define MDO_CLK_R(rts, init, bit)                         \
	do {                                              \
		u32 _tmp_val;                             \
		(init) &= ~MD_OE;                         \
		(init) |= MDC_H;                          \
		writel(init, &(rts)->regs->phy_mdio);     \
		udelay(1);                                \
		(init) &= ~(MDC_MASK | MDI_MASK);         \
		writel(init, &(rts)->regs->phy_mdio);     \
		_tmp_val = readl(&(rts)->regs->phy_mdio); \
		(bit) = (_tmp_val & MDO_H) ? 1 : 0;       \
		udelay(5);                                \
	} while (0)

static u16 mdio_read(struct rts_usb_phy *rts, u16 addr)
{
	u32 init = readl(&rts->regs->phy_mdio);
	u16 data = 0;
	int i;

	/* PRE */
	for (i = 0; i < 40; i++)
		MDI_CLK_W(rts, init, MDI_H);

	/* ST */
	MDI_CLK_W(rts, init, MDI_L);
	MDI_CLK_W(rts, init, MDI_H);

	/* OP */
	MDI_CLK_W(rts, init, MDI_H);
	MDI_CLK_W(rts, init, MDI_L);

	/* Address */
	for (i = 9; i >= 0; i--) {
		int hi = (addr >> i) & 0x01;

		if (hi)
			MDI_CLK_W(rts, init, MDI_H);
		else
			MDI_CLK_W(rts, init, MDI_L);
	}

	/* TA */
	MDI_CLK_W(rts, init, MDI_H);
	MDI_CLK_W(rts, init, MDI_L);

	/* Data */
	for (i = 15; i >= 0; i--) {
		u8 bit;

		MDO_CLK_R(rts, init, bit);
		data |= bit << i;
	}

	/* Idle */
	for (i = 0; i < 10; i++)
		MDI_CLK_W_RISING(rts, init, MDI_H);

	return data;
}

static void mdio_write(struct rts_usb_phy *rts, u16 addr, u16 val)
{
	u32 init = readl(&rts->regs->phy_mdio);
	int i;

	/* PRE */
	for (i = 0; i < 40; i++)
		MDI_CLK_W(rts, init, MDI_H);

	/* ST */
	MDI_CLK_W(rts, init, MDI_L);
	MDI_CLK_W(rts, init, MDI_H);

	/* OP */
	MDI_CLK_W(rts, init, MDI_L);
	MDI_CLK_W(rts, init, MDI_H);

	/* Address */
	for (i = 9; i >= 0; i--) {
		int hi = (addr >> i) & 0x01;

		if (hi)
			MDI_CLK_W(rts, init, MDI_H);
		else
			MDI_CLK_W(rts, init, MDI_L);
	}

	/* TA */
	MDI_CLK_W(rts, init, MDI_H);
	MDI_CLK_W(rts, init, MDI_L);

	/* Data */
	for (i = 15; i >= 0; i--) {
		int hi = (val >> i) & 0x01;

		if (hi)
			MDI_CLK_W(rts, init, MDI_H);
		else
			MDI_CLK_W(rts, init, MDI_L);
	}

	/* Idle */
	for (i = 0; i < 10; i++)
		MDI_CLK_W(rts, init, MDI_H);
}

/*
 * We will calibration from bit 3 to bit 0
 * idx stands for the bit index
 */
static void rts_usb_phy_calen_dev(struct rts_usb_phy *rts, int port, int idx)
{
	u32 val;
	u16 mask = 0x01 << idx;
	u8 nsq_dev;

	val = mdio_read(rts, PORT_CFG3(port));
	nsq_dev = !!(val & APHY_NSQ_DEV);
	val &= ~mask;
	val |= ((nsq_dev << idx) | (mask >> 1));
	mdio_write(rts, PORT_CFG3(port), val);
}

static void rts_usb_phy_calen_hst(struct rts_usb_phy *rts, int port, int idx)
{
	u32 val;
	u16 mask = 0x40 << idx;
	u8 nsq_hst;

	val = mdio_read(rts, PORT_CFG3(port));
	nsq_hst = !!(val & APHY_NSQ_HST);
	val &= ~mask;
	val |= ((nsq_hst << (idx + APHY_SD_CAL_HST_SHIFT)) |
		((mask >> 1) & APHY_SD_CAL_HST_MASK));
	mdio_write(rts, PORT_CFG3(port), val);
}

static void rts_usb_reset_aphy(struct rts_usb_phy *rts)
{
	u32 cfg, mask;

	cfg = readl(&rts->regs->aphy_cfg6);
	if (rts->device_type == TYPE_RTS493XA)
		mask = UPHY_U2_APHY_RST_N;

	cfg &= ~mask;
	writel(cfg, &rts->regs->aphy_cfg6);
	mdelay(1);
	cfg |= mask;
	writel(cfg, &rts->regs->aphy_cfg6);
}

static void rts_usb_pd15k(struct rts_usb_phy *rts, int port)
{
	u16 val;

	val = mdio_read(rts, PORT_CFG0(port));
	val |= APHY_RPDEN;
	mdio_write(rts, PORT_CFG0(port), val);
	mdio_read(rts, PORT_CFG0(port));
}

static void rts_usb_disable_cali(struct rts_usb_phy *rts, int port, u16 calen)
{
	u16 val;

	val = mdio_read(rts, PORT_CFG3(port));
	val &= ~calen;
	mdio_write(rts, PORT_CFG3(port), val);
	mdio_read(rts, PORT_CFG3(port));
}

static int rts_init_fpga_phy(struct usb_phy *phy)
{
	struct rts_usb_phy *rts = container_of(phy, struct rts_usb_phy, phy);
	int i, port;
	u16 val;

	mutex_lock(&rts->mutex);

	/* Reset APHY */
	rts_usb_reset_aphy(rts);
	msleep(100);

	/* Device calibration */
	for (port = 0; port < 2; port++) {
		mdio_write(rts, PORT_CFG3(port), APHY_CALEN_INIT);
		mdio_write(rts, PORT_CFG3(port),
			   APHY_CALEN_INIT | APHY_CALEN_DEV);

		for (i = 3; i >= 0; i--)
			rts_usb_phy_calen_dev(rts, port, i);

		rts_usb_disable_cali(rts, port, APHY_CALEN_DEV);
	}

	/* Pull down 15K resistor for downstream ports */
	rts_usb_pd15k(rts, HST_PORT);

	/* Host calibration */
	mdio_write(rts, PORT_CFG3(HST_PORT), APHY_CALEN_INIT);
	mdio_write(rts, PORT_CFG3(HST_PORT), APHY_CALEN_INIT | APHY_CALEN_HST);

	for (i = 3; i >= 0; i--)
		rts_usb_phy_calen_hst(rts, HST_PORT, i);

	rts_usb_disable_cali(rts, HST_PORT, APHY_CALEN_HST);

	/* fixed RLE0485 Z0 value 0b'1010 without Auto-Calibration */
	mdio_write(rts, USB2_CLK_CFG2, 0x128);

	/* Improve the RX sensitivity for downstream port */
	val = mdio_read(rts, PORT_CFG3(HST_PORT));
	val = ((val & 0x0F) < 0x0E) ? (val + 2) : (val | 0x0F);
	mdio_write(rts, PORT_CFG3(HST_PORT), val);

	mutex_unlock(&rts->mutex);
	return 0;
}

static int rts_init_usb_phy_rts493x(struct usb_phy *phy)
{
	struct rts_usb_phy *rts = container_of(phy, struct rts_usb_phy, phy);
	u32 cfg;

	mutex_lock(&rts->mutex);

	/* Disable Z0 autoK */
	cfg = readl(&rts->regs->aphy_cfg5);
	cfg &= ~(UPHY_ANA_AUTO_K | (UPHY_Z0_CODE_MASK << UPHY_Z0_CODE_SHIFT));
	cfg |= (RTS_Z0_CODE << UPHY_Z0_CODE_SHIFT);
	writel(cfg, &rts->regs->aphy_cfg5);

	/* Host sensitivity */
	cfg = readl(&rts->regs->aphy_cfg1);
	cfg &= ~(UPHY_HST_SEN_OBJ << UPHY_HST_SEN_OBJ_SHIFT);
	cfg |= (rts->host_sens_obj << UPHY_HST_SEN_OBJ_SHIFT);
	writel(cfg, &rts->regs->aphy_cfg1);

	/* Disable REG_CAL_1 & Enable REG_CAL_1 */
	cfg = readl(&rts->regs->aphy_cfg2);
	cfg &= ~UPHY_HST_CAL;
	writel(cfg, &rts->regs->aphy_cfg2);
	cfg |= UPHY_HST_CAL;
	writel(cfg, &rts->regs->aphy_cfg2);

	/* modify HST_SENS_CHIRP */
	cfg = readl(&rts->regs->aphy_cfg2);
	cfg &= ~(UPHY_HST_SENS_CHIRP << UPHY_HST_SENS_CHIRP_SHIFT);
	cfg |= (rts->host_sens_chirp << UPHY_HST_SENS_CHIRP_SHIFT);
	writel(cfg, &rts->regs->aphy_cfg2);

	/* Dev sensitivity */
	cfg = readl(&rts->regs->aphy_cfg3);
	cfg &= ~(UPHY_DEV_SEN_OBJ << UPHY_DEV_SEN_OBJ_SHIFT);
	cfg &= ~(UPHY_DEV_SEN_NORMAL << UPHY_DEV_SEN_NORMAL_SHIFT);
	cfg |= (rts->dev_sens_obj << UPHY_DEV_SEN_OBJ_SHIFT);
	cfg |= (rts->dev_sens_nor << UPHY_DEV_SEN_NORMAL_SHIFT);
	writel(cfg, &rts->regs->aphy_cfg3);

	/* Disable REG_CAL_1 & Enable REG_CAL_1 */
	cfg = readl(&rts->regs->aphy_cfg4);
	cfg &= ~UPHY_DEV_CAL;
	writel(cfg, &rts->regs->aphy_cfg4);
	cfg |= UPHY_DEV_CAL;
	writel(cfg, &rts->regs->aphy_cfg4);

	mutex_unlock(&rts->mutex);

	return 0;
}

#ifdef CONFIG_USB_RTS_PHY_DEBUG
static int usb_phy_mdio_open(struct inode *inode, struct file *file)
{
	struct rts_usb_phy *rts =
		container_of(inode->i_cdev, struct rts_usb_phy, cdev);

	file->private_data = rts;
	return 0;
}

static int usb_phy_mdio_release(struct inode *inode, struct file *file)
{
	return 0;
}

#define USB_PHY_MDIO_IOC_MAGIC 0x74

#define USB_PHY_MDIO_IOC_READ  _IOWR(USB_PHY_MDIO_IOC_MAGIC, 0xA2, int)
#define USB_PHY_MDIO_IOC_WRITE _IOWR(USB_PHY_MDIO_IOC_MAGIC, 0xA3, int)

static long usb_phy_mdio_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	struct rts_usb_phy *rts = file->private_data;
	int retval = 0;

	mutex_lock(&rts->mutex);

	switch (cmd) {
	case USB_PHY_MDIO_IOC_READ: {
		u16 addr, val;
		u8 buf[2];

		if (copy_from_user(buf, (char *)arg, 2)) {
			retval = -EFAULT;
			break;
		} else {
			retval = 0;
		}

		addr = ((u16)buf[0] << 8) | buf[1];

		val = mdio_read(rts, addr);
		buf[0] = (u8)(val >> 8);
		buf[1] = (u8)val;

		if (copy_to_user((char *)arg, buf, 2))
			retval = -EFAULT;
		else
			retval = 0;
		break;
	}

	case USB_PHY_MDIO_IOC_WRITE: {
		u16 addr, val;
		u8 buf[4];

		if (copy_from_user(buf, (char *)arg, 4)) {
			retval = -EFAULT;
			break;
		} else {
			retval = 0;
		}

		addr = ((u16)buf[0] << 8) | buf[1];
		val = ((u16)buf[2] << 8) | buf[3];

		mdio_write(rts, addr, val);
		break;
	}

	default:
		return -EINVAL;
	}

	mutex_unlock(&rts->mutex);
	return retval;
}

static const struct file_operations usb_phy_mdio_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = usb_phy_mdio_ioctl,
	.open = usb_phy_mdio_open,
	.release = usb_phy_mdio_release,
};

dev_t usb_phy_mdio_devno = MKDEV(121, 0);
struct cdev usb_phy_mdio_cdev;
#endif

static const struct of_device_id rts_usbphy_dt_ids[] = {
	{
		.compatible = "realtek,rts493xa-usbphy",
		.data = (void *)(TYPE_RTS493XA),
	},
	{ /* sentinel */ },
};

static int rts_usb_phy_probe(struct platform_device *pdev)
{
	struct rts_usb_phy *rts;
	struct resource *res;
	int ret = 0;
	const struct of_device_id *of_id;

	of_id = of_match_device(rts_usbphy_dt_ids, &pdev->dev);

	rts = devm_kzalloc(&pdev->dev, sizeof(*rts), GFP_KERNEL);
	if (!rts)
		return -ENOMEM;

	mutex_init(&rts->mutex);

	rts->dev = &pdev->dev;

	rts->phy.dev = rts->dev;
	rts->phy.label = "rtsuphy";
	rts->phy.type = USB_PHY_TYPE_USB2;
	rts->device_type = (struct rts_usbphy_type *)of_id->data;
	if (of_machine_is_compatible("realtek,rts_fpga")) {
		rts->phy.init = rts_init_fpga_phy;
	} else {
		if (rts->device_type == TYPE_RTS493XA) {
			rts->phy.init = rts_init_usb_phy_rts493x;
			of_property_read_u32(rts->dev->of_node, "host-sens-obj",
					     &rts->host_sens_obj);
			of_property_read_u32(rts->dev->of_node,
					     "host-sens-chirp",
					     &rts->host_sens_chirp);
			of_property_read_u32(rts->dev->of_node, "dev-sens-obj",
					     &rts->dev_sens_obj);
			of_property_read_u32(rts->dev->of_node,
					     "dev-sens-normal",
					     &rts->dev_sens_nor);
		}
	}

	usb_add_phy_dev(&rts->phy);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resource provided\n");
		return -ENXIO;
	}

	rts->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rts->regs))
		return PTR_ERR(rts->regs);

	platform_set_drvdata(pdev, rts);

	usb_phy_init(&rts->phy);

#ifdef CONFIG_USB_RTS_PHY_DEBUG
	ret = register_chrdev_region(usb_phy_mdio_devno, 1, "usb_phy_mdio");
	if (ret) {
		dev_err(&pdev->dev, "register_chrdev_region fail\n");
		return -ENODEV;
	}

	cdev_init(&rts->cdev, &usb_phy_mdio_fops);
	usb_phy_mdio_cdev.owner = THIS_MODULE;

	ret = cdev_add(&rts->cdev, usb_phy_mdio_devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "cdev_add fail\n");
		unregister_chrdev_region(usb_phy_mdio_devno, 1);
		return -ENODEV;
	}
#endif

	dev_info(&pdev->dev, "Initialized Realtek IPCam USB Phy module\n");
	return 0;
}

static int rts_usb_phy_remove(struct platform_device *pdev)
{
#ifdef CONFIG_USB_RTS_PHY_DEBUG
	struct rts_usb_phy *rts = platform_get_drvdata(pdev);

	cdev_del(&rts->cdev);
	unregister_chrdev_region(usb_phy_mdio_devno, 1);
#endif

	return 0;
}

static struct platform_driver rts_usb_phy_driver = {
	.probe		= rts_usb_phy_probe,
	.remove		= rts_usb_phy_remove,
	.driver		= {
		.name	= "usbphy-platform",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(rts_usbphy_dt_ids),
	},
};

static int __init rts_usb_phy_init(void)
{
	return platform_driver_register(&rts_usb_phy_driver);
}
subsys_initcall(rts_usb_phy_init);

static void __exit rts_usb_phy_exit(void)
{
	platform_driver_unregister(&rts_usb_phy_driver);
}
module_exit(rts_usb_phy_exit);
