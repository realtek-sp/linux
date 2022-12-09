// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Realtek Semiconductor Corp.
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <asm/ioctl.h>
#include <linux/signal.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/nvmem-provider.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/vmalloc.h>
#include <linux/of.h>

//#define OTP_DEBUG

#ifdef OTP_DEBUG
#define dump_regs(s, n)                                                  \
	do {                                                             \
		int i;                                                   \
		pr_info("dump regs:\n");                                 \
		for (i = (s) * 4; i < ((s) + (n)) * 4; i += 4)           \
			pr_info(" 0x%06x%02x= 0x%08x\n", (otp)->base, i, \
				readl((otp)->base + i));                 \
	} while (0)
#else
#define dump_regs(s, n)
#endif

#define MASK(s, n) (((1UL << (n)) - 1) << (s))

#define REG_SF_CTRL_0	       0x00
#define REG_SF_MODE_CTRL       0x04
#define REG_SF_HW_CTRL	       0x08
#define REG_ADDR_CTRL	       0x10
#define REG_BURST_CTRL	       0x14
#define REG_OTP_PROGRAM_DATA   0x18
#define REG_HW_START_REG       0x30
#define REG_ECC_CTRL_AUTO      0x34
#define REG_ECC_CTRL1	       0x3C
#define REG_ECC_CTRL2	       0x40
#define REG_ECC_STATUS_REG0    0x44
#define REG_OTP_READ_DATA_0    0x48
#define REG_OTP_CONTROL_DATA_0 0x70
#define REG_AES_KEY_SEL	       0x88
#define REG_AES_KEY_GROUP      0x90
#define REG_LOAD_KEY	       0x94

//REG_SF_CTRL_0		0x00
#define RG_PENVDD2_VDD2_SW	0
//REG_SF_MODE_CTRL	0x04
#define RG_PTM_SW_MASK		MASK(0, 3)
#define N_READ			0x0
#define N_PGM			0x2
#define DOUBLE_BIT_EN		8
//REG_ADDR_CTRL		0x10
#define OTP_ADDR_PGM_SEL_MASK	MASK(0, 3)
#define OTP_ADDR_INTERNAL	3
#define OTP_ADDR_INTERNAL_MASK	MASK(3, 6)
#define OTP_ADDR_GROUP_SEL	12
#define OTP_ADDR_GROUP_SEL_MASK MASK(12, 6)
#define OTP_ADDR_MASK                                     \
	(OTP_ADDR_PGM_SEL_MASK | OTP_ADDR_INTERNAL_MASK | \
	 OTP_ADDR_GROUP_SEL_MASK)
//REG_ECC_STATUS_REG0		0x44
#define RG_ECC_ERR(i)		 ((i) * 8)
#define RG_ECC_LOC(i)		 (2 + (i) * 8)
#define RG_ECC_ERR_MASK(i)	 MASK((i) * 8, 2)
#define RG_ECC_LOC_MASK(i)	 MASK(2 + (i) * 8, 4)
#define ECC_NO_ERROR		 0x00
#define ECC_ERROR_IN_PARITY_BYTE 0x01
#define ECC_CORRECTED		 0x02
#define ECC_UNCORRECTED		 0x03

//REG_AES_KEY_SEL		0x88
#define KEY_SEL(i)	((i) * 8)
#define KEY_SEL_MASK(i) MASK((i) * 8, 3)
#define KEY(i, k)	(((i) % 2) + (k) * 2)
//REG_AES_KEY_GROUP	0x90
//REG_LOAD_KEY		0x94

//#define TEST_MODE
#ifdef TEST_MODE
#define EN_DOUBLEBIT (0)
#define GROUP0_BYTE  (32)
#else
#define EN_DOUBLEBIT (1)
#define GROUP0_BYTE  (16)
#endif

#define GROUP_NUMS (57)
#define GROUP_BYTE (36)
#define ALL_BYTES  (GROUP0_BYTE + GROUP_BYTE * (GROUP_NUMS - 1))

#define KEY_NUMS       (4)
#define KEY_GROUP_NUMS (2)

#define read_reg(reg)	    readl((otp)->base + (reg))
#define write_reg(reg, val) writel((val), (otp)->base + (reg))
#define change_reg(reg, val, mask) \
	write_reg((reg), (read_reg((reg)) & (~(mask))) | (val))

struct rts_otp {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct reset_control *rst;
	struct nvmem_device *nvmem;
	int key;
	uint8_t data[GROUP_BYTE];
};

static struct rts_otp *g_otp;

static unsigned int walk_group_init(unsigned int *offset, size_t bytes,
				    size_t *len)
{
	unsigned int group;
	size_t tmp;

	if (*offset < GROUP0_BYTE) {
		group = 0;
		tmp = GROUP0_BYTE - *offset;
	} else {
		group = (*offset - GROUP0_BYTE) / GROUP_BYTE + 1;
		*offset = (*offset - GROUP0_BYTE) % GROUP_BYTE;
		tmp = GROUP_BYTE - *offset;
	}

	if (len)
		*len = bytes < tmp ? bytes : tmp;

	return group;
}

static void walk_group_next(unsigned int *group, unsigned int *offset,
			    size_t *bytes, size_t *len)
{
	*bytes -= *len;
	if (!*bytes)
		return;

	*len = *bytes < GROUP_BYTE ? *bytes : GROUP_BYTE;
	(*group)++;
	*offset = 0;
}

static void enable_ecc(struct rts_otp *otp, unsigned int group)
{
	if (group <= 32)
		write_reg(REG_ECC_CTRL1, 1 << (group - 1));
	else
		write_reg(REG_ECC_CTRL2, 1 << (group - 32 - 1));

	write_reg(REG_ECC_CTRL_AUTO, 1);
}

static void check_ecc_status(struct rts_otp *otp, unsigned int group)
{
	u32 tmp, val = 0;
	int i = 0, start = 0, end = 2;

	/* check group g-0 */
	for (i = 0; i < 4; i++) {
		if (*((uint32_t *)otp->data + i) != 0xFFFFFFFF)
			break;
	}

	if (i == 4 && *((uint16_t *)otp->data + 16) == 0xFFFF)
		start++;

	/* check group g-1 */
	for (i = 4; i < 8; i++) {
		if (*((uint32_t *)otp->data + i) != 0xFFFFFFFF)
			break;
	}

	if (i == 8 && *((uint16_t *)otp->data + 17) == 0xFFFF)
		end--;

	tmp = read_reg(REG_ECC_STATUS_REG0);

	for (i = start; i < end; i++) {
		val = (tmp & RG_ECC_ERR_MASK(i)) >> RG_ECC_ERR(i);

		switch (val) {
		case ECC_NO_ERROR:
			dev_dbg(otp->dev, "group%d-%d: ecc no error\n", group,
				i);
			break;
		case ECC_ERROR_IN_PARITY_BYTE:
			dev_warn(otp->dev,
				 "group%d-%d: ecc error in parity byte\n",
				 group, i);
			break;
		case ECC_CORRECTED:
			dev_warn(otp->dev, "group%d-%d: ecc corrected\n", group,
				 i);
			val = (tmp & RG_ECC_LOC_MASK(i)) >> RG_ECC_LOC(i);
			dev_warn(otp->dev,
				 "group%d-%d: ecc error byte number: %d\n",
				 group, i, val);
			break;
		case ECC_UNCORRECTED:
			dev_warn(otp->dev, "group%d-%d: ecc uncorrected\n",
				 group, i);
			break;
		default:
			dev_warn(otp->dev, "group%d-%d: ecc unknown error\n",
				 group, i);
		}
	}
}

static void read_data(struct rts_otp *otp, unsigned int group,
		      unsigned int offset, u8 *val, size_t bytes)
{
	u32 tmp;
	int reg;
	unsigned int waddr, baddr;
	size_t wbytes;

	dev_dbg(otp->dev, "%s: group=%d, offset=%d, bytes=%d\n", __func__,
		group, offset, bytes);
	waddr = offset / 4;
	baddr = offset % 4;

	if (group == 0)
		reg = REG_OTP_CONTROL_DATA_0;
	else
		reg = REG_OTP_READ_DATA_0;

	dev_dbg(otp->dev, "waddr=%d, baddr=%d, reg=0x%08x\n", waddr, baddr,
		reg);
	if (baddr) {
		tmp = read_reg(reg + waddr++ * 4);
		bytes -= 4 - baddr;
		dev_dbg(otp->dev, "tmp=0x%08x\n", tmp);
		while (baddr < 4)
			*val++ = tmp >> baddr++ * 8 & 0xff;
		baddr = bytes % 4;
	}

	wbytes = ALIGN(bytes, 4) / 4;
	while (wbytes--) {
		tmp = read_reg(reg + waddr++ * 4);
		dev_dbg(otp->dev, "tmp=0x%08x\n", tmp);
		if (!wbytes && baddr) {
			while (baddr < 4)
				*val++ = tmp >> baddr++ * 8 & 0xff;
			break;
		}

		*(u32 *)val = tmp;
		val += 4;
	}
}

static int otp_read(struct rts_otp *otp, unsigned int group,
		    unsigned int offset, u8 *val, size_t bytes)
{
	u32 tmp;
	unsigned int timeout = 200;

	/* addr */
	tmp = read_reg(REG_ADDR_CTRL);
	tmp &= ~OTP_ADDR_MASK;
	if (!group && offset >= 16) {
		/* for group0 double bit test */
		tmp |= group << OTP_ADDR_GROUP_SEL | 16 << OTP_ADDR_INTERNAL;
		offset -= 16;
	} else {
		/* normal flow */
		tmp |= group << OTP_ADDR_GROUP_SEL;
	}
	write_reg(REG_ADDR_CTRL, tmp);
	/* len */
	if (!group)
		/* group 0 */
		write_reg(REG_BURST_CTRL, 15);
	else
		write_reg(REG_BURST_CTRL, GROUP_BYTE - 1);
	/* read */
	change_reg(REG_SF_MODE_CTRL, N_READ, RG_PTM_SW_MASK);

	if (!group)
		/* enable double bit */
		EN_DOUBLEBIT ?
			change_reg(REG_SF_MODE_CTRL, BIT(DOUBLE_BIT_EN),
				   BIT(DOUBLE_BIT_EN)) :
			change_reg(REG_SF_MODE_CTRL, 0, BIT(DOUBLE_BIT_EN));
	else
		/* enable ecc */
		enable_ecc(otp, group);

	/* start read */
	dump_regs(0, 40);
	write_reg(REG_HW_START_REG, 1);
	while (timeout--) {
		tmp = read_reg(REG_HW_START_REG);
		if (!tmp) {
			dev_dbg(otp->dev, "read done!\n");
			break;
		}
		msleep(20);
	}

	if (!timeout) {
		dev_err(otp->dev, "read timeout\n");
		return -ETIMEDOUT;
	}

	read_data(otp, group, 0, otp->data, GROUP_BYTE);

	/* check ecc status */
	if (group)
		check_ecc_status(otp, group);

	/* read data reg */
	if (val && bytes)
		memcpy(val, otp->data, bytes);

	return 0;
}

static int otp_write(struct rts_otp *otp, unsigned int group,
		     unsigned int offset, u8 val)
{
	u32 tmp;
	unsigned int timeout = 50;

	dev_dbg(otp->dev, "val=0x%02x\n", val);

	/* addr */
	tmp = read_reg(REG_ADDR_CTRL);
	tmp &= ~OTP_ADDR_MASK;
	tmp |= group << OTP_ADDR_GROUP_SEL | offset << OTP_ADDR_INTERNAL;
	write_reg(REG_ADDR_CTRL, tmp);
	/* len */
	write_reg(REG_BURST_CTRL, 7);
	/* pgm */
	change_reg(REG_SF_MODE_CTRL, N_PGM, RG_PTM_SW_MASK);
	/* double bit */
	if (!group)
		EN_DOUBLEBIT ?
			change_reg(REG_SF_MODE_CTRL, BIT(DOUBLE_BIT_EN),
				   BIT(DOUBLE_BIT_EN)) :
			change_reg(REG_SF_MODE_CTRL, 0, BIT(DOUBLE_BIT_EN));
	/* data */
	write_reg(REG_OTP_PROGRAM_DATA, val);

	/* start pgm */
	dump_regs(0, 40);
	write_reg(REG_HW_START_REG, 1);
	while (timeout--) {
		tmp = read_reg(REG_HW_START_REG);
		if (!tmp) {
			dev_dbg(otp->dev, "pgm done!\n");
			break;
		}
		msleep(20);
	}

	if (!timeout) {
		dev_err(otp->dev, "pgm timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int rts_otp_read(void *context, unsigned int offset, void *val,
			size_t bytes)
{
	unsigned int group;
	size_t len;
	int ret;
	struct rts_otp *otp = context;

	/* check input */
	if (!val || offset >= ALL_BYTES || offset + bytes > ALL_BYTES)
		return -EINVAL;

	/* enable power up protect */
	change_reg(REG_SF_CTRL_0, 1, BIT(RG_PENVDD2_VDD2_SW));

	/* read */
	group = walk_group_init(&offset, bytes, &len);
	dev_dbg(otp->dev, "group=%d, offset=%d, len=%d\n", group, offset, len);
	while (bytes) {
		ret = otp_read(otp, group, offset, val, len);
		if (ret)
			return ret;
		val = (u8 *)val + len;
		walk_group_next(&group, &offset, &bytes, &len);
		dev_dbg(otp->dev, "group=%d, offset=%d, bytes=%d, len=%d\n",
			group, offset, bytes, len);
	}

	/* disable power up protect */
	change_reg(REG_SF_CTRL_0, 0, BIT(RG_PENVDD2_VDD2_SW));

	return 0;
}

static int rts_otp_write(void *context, unsigned int offset, void *val,
			 size_t bytes)
{
	unsigned int group;
	int ret, i;
	size_t len;
	struct rts_otp *otp = context;

	dev_dbg(otp->dev, "offset=%d, bytes=%d, val=0x%08x\n", offset, bytes,
		(unsigned int)val);
	/* check input */
	if (!val || offset >= ALL_BYTES || offset + bytes > ALL_BYTES)
		return -EINVAL;

	/* enable power up protect */
	change_reg(REG_SF_CTRL_0, 1, BIT(RG_PENVDD2_VDD2_SW));

	/* pgm */
	group = walk_group_init(&offset, bytes, &len);
	dev_dbg(otp->dev, "group=%d, offset=%d, len=%d\n", group, offset, len);
	while (bytes) {
		for (i = 0; i < len; i++) {
			ret = otp_write(otp, group, offset++, *(u8 *)val++);
			if (ret)
				return ret;
		}
		walk_group_next(&group, &offset, &bytes, &len);
		dev_dbg(otp->dev, "group=%d, offset=%d, bytes=%d, len=%d\n",
			group, offset, bytes, len);
	}

	/* disable power up protect */
	change_reg(REG_SF_CTRL_0, 0, BIT(RG_PENVDD2_VDD2_SW));

	return 0;
}

int rts_otp_load_aes_key(unsigned int k)
{
	int ret, g, i;
	struct rts_otp *otp = g_otp;

	dev_dbg(otp->dev, "%s,%d: k=%d\n", __func__, __LINE__, k);

	if (k > KEY_NUMS - 1)
		return -EINVAL;

	/* key index 0-1 put into key group-0, 2-3 put into key group-1 */
	g = k % KEY_GROUP_NUMS;
	write_reg(REG_AES_KEY_GROUP, g);

	if (1 << k & otp->key)
		return 0;

	/* enable power up protect */
	change_reg(REG_SF_CTRL_0, 1, BIT(RG_PENVDD2_VDD2_SW));

	/* binding key group */
	for (i = 0; i < KEY_NUMS; i++) {
		dev_dbg(otp->dev, "%d, %d, %ld\n", KEY(i, k), KEY_SEL(i),
			KEY_SEL_MASK(i));
		change_reg(REG_AES_KEY_SEL, KEY(i, k) << KEY_SEL(i),
			   KEY_SEL_MASK(i));
	}
	/* enable load key */
	write_reg(REG_LOAD_KEY, 1);

	ret = otp_read(otp, k + 1, 0, NULL, 0);
	if (ret)
		goto err;

	otp->key &= ~(11 << g);
	otp->key |= 1 << k;

err:
	/* disable load key */
	write_reg(REG_LOAD_KEY, 0);
	/* disable power up protect */
	change_reg(REG_SF_CTRL_0, 0, BIT(RG_PENVDD2_VDD2_SW));

	return ret;
}
EXPORT_SYMBOL_GPL(rts_otp_load_aes_key);

static struct nvmem_config econfig = {
	.name = "rts-otp",
	.owner = THIS_MODULE,
	.stride = 1,
	.word_size = 1,
	.size = ALL_BYTES,
};

static int rts_otp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct rts_otp *rts;
	int ret = 0;

	dev_info(&pdev->dev, "rts otp probe\n");

	rts = devm_kzalloc(&pdev->dev, sizeof(*rts), GFP_KERNEL);
	if (!rts)
		return -ENOMEM;

	g_otp = rts;
	rts->dev = dev;
	rts->key = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resource provided");
		return -ENXIO;
	}

	rts->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(rts->base))
		return PTR_ERR(rts->base);

	rts->clk = devm_clk_get(dev, "otp_ck");
	if (IS_ERR(rts->clk)) {
		dev_err(dev, "clock initialization failed.\n");
		return PTR_ERR(rts->clk);
	}

	ret = clk_prepare_enable(rts->clk);
	if (ret) {
		dev_err(dev, "clock prepare failed.\n");
		return ret;
	}
#if 0
	/* reset otp */
	rts->rst = devm_reset_control_get(dev, "rst");
	if (IS_ERR(rts->rst)) {
		dev_err(&pdev->dev, "no reset found.\n");
		return PTR_ERR(rts->rst);
	}

	ret = reset_control_reset(rts->rst);
	if (ret)
		dev_warn(dev, "reset failed\n");
#endif
	econfig.dev = dev;
	econfig.reg_read = rts_otp_read;
	econfig.reg_write = rts_otp_write;
	econfig.priv = rts;

	rts->nvmem = nvmem_register(&econfig);
	if (IS_ERR(rts->nvmem))
		return PTR_ERR(rts->nvmem);

	platform_set_drvdata(pdev, rts);

	return 0;
}

static int rts_otp_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct rts_otp *rts = platform_get_drvdata(pdev);

	nvmem_unregister(rts->nvmem);
	clk_disable_unprepare(rts->clk);

	return ret;
}

static const struct of_device_id rts_otp_dt_ids[] = {
	{
		.compatible = "realtek,rts-otp",
	},
	{ /* sentinel */ },
};

static struct platform_driver rts_otp_driver = {
	.probe		= rts_otp_probe,
	.remove		= rts_otp_remove,
	.driver		= {
		.name = "rts-otp",
		.of_match_table = of_match_ptr(rts_otp_dt_ids),
	},
};

module_platform_driver(rts_otp_driver);
MODULE_AUTHOR("Zain Zhou <zain_zhou@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek otp driver");
MODULE_LICENSE("GPL");
