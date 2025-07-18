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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/hw_random.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/io.h>

#include "rts_rng.h"

#ifdef TRNG_DEBUG
#define dump_regs(s, n)                                        \
	do {                                                   \
		int i;                                         \
		pr_info("dump regs:\n");                       \
		for (i = (s) * 4; i < ((s) + (n)) * 4; i += 4) \
			pr_info(" 0xb88a01%02x= 0x%08x\n", i,  \
				readl(0xb88a0100 + i));        \
	} while (0)
#else
#define dump_regs(s, n)
#endif

#define SRC_DIGITAL	       0
#define SRC_ANALOG	       1
#define SRC_DIGITAL_XOR_ANALOG 2

struct rts_trng_data {
	struct platform_device *pdev;
	void __iomem *addr;
	struct clk *trng_clk;
	struct reset_control *rst;
	struct hwrng trng;
};

static inline unsigned int rts_trng_read(struct rts_trng_data *tdata,
					 unsigned int reg)
{
	return readl(tdata->addr + reg);
}

static inline void rts_trng_write(struct rts_trng_data *tdata, unsigned int reg,
				  unsigned int value)
{
	writel(value, tdata->addr + reg);
}

/* regs setting */
static inline int rts_trng_done(struct rts_trng_data *tdata)
{
	return rts_trng_read(tdata, RLX_REG_TRNG_START) ? 0 : 1;
}

static inline void rts_trng_start(struct rts_trng_data *tdata)
{
	rts_trng_write(tdata, RLX_REG_TRNG_START, 1);
}

static inline void rts_trng_set_ctl(struct rts_trng_data *tdata,
				    unsigned int value)
{
	rts_trng_write(tdata, RLX_REG_TRNG_CTL, value);
}

static inline void rts_trng_set_source(struct rts_trng_data *tdata,
				       unsigned int value)
{
	rts_trng_write(tdata, RLX_REG_TRNG_SRC_SEL, value);
}

static inline void rts_trng_enable_neu(struct rts_trng_data *tdata,
				       unsigned int enable)
{
	rts_trng_write(tdata, RLX_REG_TRNG_NEU_EN, enable);
}

static int rts_trng_get_result(struct rts_trng_data *tdata, u32 *buf,
			       unsigned int len)
{
	int ret = 0;

	if (!buf)
		return -1;

	if (len >= 4) {
		get_random_bytes(&buf[0], 4);
		buf[0] ^=
			be32_to_cpu(rts_trng_read(tdata, RLX_REG_TRNG_RESUTL0));
		ret = 4;
	}

	if (len >= 8) {
		get_random_bytes(&buf[1], 4);
		buf[1] ^=
			be32_to_cpu(rts_trng_read(tdata, RLX_REG_TRNG_RESUTL1));
		ret = 8;
	}

	dev_dbg(&tdata->pdev->dev, "%08x%08x\n", buf[0], buf[1]);

	return ret;
}

static inline void rts_trng_init(struct rts_trng_data *tdata)
{
	int ret;

	ret = clk_prepare_enable(tdata->trng_clk);
	if (ret) {
		dev_err(&tdata->pdev->dev, "clock prepare failed.\n");
		return;
	}

	/* disable irq en */
	rts_trng_write(tdata, RLX_REG_TRNG_IRQ, RLX_TRNG_GEN_DONE_INT);
	rts_trng_write(tdata, RLX_REG_TRNG_IRQ_EN, 0);

	/* trng source */
	rts_trng_set_source(tdata, SRC_ANALOG);
	/* ring oscillator enable for length */
	rts_trng_set_ctl(tdata, RLX_TRNG_CFG_EN7 | RLX_TRNG_CFG_EN13 |
					RLX_TRNG_CFG_EN17 | RLX_TRNG_CFG_EN23);
	/* von neumann */
	rts_trng_enable_neu(tdata, 1);

	dump_regs(0, 10);
}

static inline void rts_trng_uninit(struct rts_trng_data *tdata)
{
	clk_disable_unprepare(tdata->trng_clk);
}

static int rts_hwrng_init(struct hwrng *rng)
{
	struct rts_trng_data *tdata = (struct rts_trng_data *)rng->priv;

	dev_dbg(&tdata->pdev->dev, "rts hwrng init\n");
	rts_trng_init(tdata);

	return 0;
}
static void rts_hwrng_cleanup(struct hwrng *rng)
{
	struct rts_trng_data *tdata = (struct rts_trng_data *)rng->priv;

	dev_dbg(&tdata->pdev->dev, "rts hwrng cleanup\n");
	rts_trng_uninit(tdata);
}

static int rts_hwrng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	int t = 1000; //1ms
	struct rts_trng_data *tdata = (struct rts_trng_data *)rng->priv;

	dev_dbg(&tdata->pdev->dev, "rts hwrng read, max=%d, wait=%d\n", max,
		wait);

	rts_trng_start(tdata);

	while (!rts_trng_done(tdata) && t--) {
		/* O_NONBLOCK or O_NDELAY*/
		if (!wait) {
			return 0;
		}

		udelay(1);
	}

	if (!t) {
		dev_err(&tdata->pdev->dev, "device busy, timed out!\n");
		return -ETIMEDOUT;
	}

	return rts_trng_get_result(tdata, buf, max);
}

static int rts_trng_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct rts_trng_data *tdata;

	tdata = devm_kzalloc(&pdev->dev, sizeof(*tdata), GFP_KERNEL);
	if (tdata == NULL)
		return -ENOMEM;

	tdata->pdev = pdev;

	platform_set_drvdata(pdev, tdata);

	/* resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&tdata->pdev->dev, "unable to get trng address\n");
		return -ENXIO;
	}

	/* base addr */
	tdata->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tdata->addr)) {
		dev_err(&tdata->pdev->dev, "unable to ioremap\n");
		return -ENXIO;
	}

	/* clk */
	tdata->trng_clk = devm_clk_get(&pdev->dev, "trng_ck");
	if (IS_ERR(tdata->trng_clk)) {
		dev_err(&pdev->dev, "clock initialization failed.\n");
		return PTR_ERR(tdata->trng_clk);
	}

	/* retst */
	tdata->rst = devm_reset_control_get(&pdev->dev, "rst");
	if (IS_ERR(tdata->rst)) {
		dev_err(&pdev->dev, "no top level reset found.\n");
		return PTR_ERR(tdata->rst);
	}

	/* reset trng */
	reset_control_reset(tdata->rst);

	/* register */
	tdata->trng.priv = (unsigned long)tdata;
	tdata->trng.name = pdev->name;
	tdata->trng.init = rts_hwrng_init;
	tdata->trng.read = rts_hwrng_read;
	tdata->trng.cleanup = rts_hwrng_cleanup;

	ret = devm_hwrng_register(&pdev->dev, &tdata->trng);

	dev_info(&pdev->dev, "Realtek RLX trng driver initialized\n");

	return ret;
}

static int rts_trng_remove(struct platform_device *pdev)
{
	struct rts_trng_data *tdata;
	struct resource *res;

	tdata = platform_get_drvdata(pdev);

	/* unregister algs */
	devm_hwrng_unregister(&pdev->dev, &tdata->trng);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));

	devm_kfree(&pdev->dev, tdata);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static const struct of_device_id rts_trng_dt_ids[] = {
	{ .compatible = "realtek,rts493xa-trng" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rts_trng_dt_ids);

static struct platform_driver rts_trng_driver = {
	.probe = rts_trng_probe,
	.remove = rts_trng_remove,
	.driver = {
		.name = "rts-trng",
		.of_match_table = of_match_ptr(rts_trng_dt_ids),
	},
};
module_platform_driver(rts_trng_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek Trng Driver");
