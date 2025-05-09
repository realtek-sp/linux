
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
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/realtek,rts493xa.h>

enum {
	TYPE_RTS493XA = 1,

	TYPE_FPGA = (1 << 16),
};
/* USBPHY_CLK_CFG_REG */
#define USBPHY_HOST_CLK_EN (1 << 1)
#define USBPHY_DEV_CLK_EN  (1 << 0)
#define CLK_ENABLE	   0x1000000

#define CLK_CHANGE_R	 (0)
#define UART_CLK_LP_EN_R (0x04)
#define P1BUS_CLK_CFG_R	 (0x08)
#define DRAM_CLK_CFG_R	 (0x0c)
#define CPU_CLK_CFG_R	 (0x10)
#define XB2_CLK_CFG_R	 (0x14)
#define BUS_CLK_CFG_R	 (0x18)
#define CIPHER_CLK_CFG_R (0x20)
#define UART_CLK_CFG_R	 (0x28)
#define I2C_CLK_CFG_R	 (0x2c)

#define RTC32K_DIV_CFG0_R (0x3c)
#define RTC32K_DIV_CFG1_R (0x40)
#define RTC32K_DIV_CFG2_R (0x44)
#define RTC_CLK_CFG_R	  (0x48)
#define USBPHY_CLK_CFG_R  (0x4c)

#define H265_ACLK_CFG_R (0x80)
#define H265_BCLK_CFG_R (0x84)
#define H265_CCLK_CFG_R (0x88)
#define SSOR_HCLK_CFG_R (0x8C)
#define SSOR_CLK_OE_R	(0x90)
#define RSA_CLK_EN_R	(0xA0)
#define SHA_CLK_EN_R	(0xA4)

#define TRNG_CLK_CFG_R (0xBC)

#define ETHERNET_CLK_CFG_R     (0xC4)
#define EFUSE_CLK_CFG_R	       (0xC8)
#define MAC_BYPASS_CLK_CFG_R   (0xCC)
#define DMA_CLK_CFG_R	       (0xD8)
#define SD0_CRC_CLK_CFG_REG    (0xE0)
#define SD0_SAMPLE_CLK_CFG_REG (0xE4)
#define SD0_PUSH_CLK_CFG_REG   (0xE8)
#define SD0_DDR_CLK_CFG_REG    (0xEC)
#define SD1_CRC_CLK_CFG_REG    (0xF0)
#define SD1_SAMPLE_CLK_CFG_REG (0xF4)
#define SD1_PUSH_CLK_CFG_REG   (0xF8)
#define SD1_DDR_CLK_CFG_REG    (0xFC)

#define UART1_CLK_CFG_R	    (0x104)
#define UART2_CLK_CFG_R	    (0x108)
#define I2C1_CLK_CFG_R	    (0x10C)
#define CPU_TRC_CLK_CFG_REG (0x114)
#define SSI_CLK_CFG_REG	    (0x118)

#define BSP_CLK_GPLL0_BASE_R (0x000)
#define BSP_CLK_GPLL1_BASE_R (0x100)
#define BSP_CLK_GPLL2_BASE_R (0x200)
#define BSP_CLK_GPLL3_BASE_R (0x300)

#define GPLL_CTRL      0x00
#define GPLL_CFG       0x04
#define GPLL_SCCG_CFG0 0x08
#define GPLL_SCCG_CFG1 0x0C
#define GPLL_STATUS    0x10
#define GPLL_SCCG_CFG2 0x14
#define GPLL_SCCG_CFG3 0x18

#define PPOW_LDO      (1 << 1)
#define GPLL_EN	      (1 << 0)
#define SYSPLL_CK_RDY (1 << 0)
#define REG_EN_SSC    (1 << 31)
#define CMU_SSC_RSTB  (1 << 0)
#define CMU_SSC_EN    (1 << 0)

#define REG_PIF_EN_LV_LDO (1 << 8)
#define REG_PIF_H_CMU_POW (1 << 0)
#define REG_POW_PIF	  (1 << 5)

#define SD1_CK_CHANGE	0x9
#define SD0_CK_CHANGE	0x8
#define BUS_CK_CHANGE	0x7
#define XB2_CK_CHANGE	0x6
#define CPU_CK_CHANGE	0x5
#define DRAM_CK_CHANGE	0x4
#define P1BUS_CK_CHANGE 0x3
#define CK_CHANGE_NULL	0

#define BIG_SHORT_NUM 2
#define BIG_LONG_NUM  4

#define PLL0_1G	  1000000000
#define PLL0_800M 800000000

static DEFINE_SPINLOCK(clk_spinlock);

static struct clk *clks[RLX_CLK_NUM_SIZE];
static struct clk_onecell_data clk_data;

static u8 div_array_short[] = { 1, 2, 4, 6 };
static u8 div_array[] = { 1, 2, 4, 6, 8, 10, 12, 14 };

static u8 div_array_long[] = { 1,  2,  4,  6,  8,  10, 12, 14,
			       16, 18, 20, 22, 24, 26, 28, 30 };

static u32 clk_reg_v[RLX_CLK_NUM_SIZE];
static unsigned int clk_platform_type;
static u32 pll0_rate;

struct clk_rlx {
	struct clk_hw hw;
	const char *name;
	const struct clk_ops *ops;
	const char *const *parent_names;
	u8 num_parents;
	u32 clkreg;
	u32 clk_change;
	u32 rate;
	u32 *reg_v;
	u32 reg_i;
	struct notifier_block clk_nb;
};

#define DEFINE_CLK_RLX(_name, _parent_names, _ops, _clk_reg, _clk_change) \
	static struct clk_rlx _name = {                                   \
		.name = #_name,                                           \
		.parent_names = _parent_names,                            \
		.num_parents = ARRAY_SIZE(_parent_names),                 \
		.ops = &_ops,                                             \
		.clkreg = _clk_reg,                                       \
		.clk_change = _clk_change,                                \
		.reg_v = clk_reg_v,                                       \
		.reg_i = (((u32)_clk_reg & 0x1ff) >> 2),                  \
	}

static void __iomem *clk_mapped_addr;
static void __iomem *pll_mapped_addr;

#define to_clk_rlx(_hw) container_of(_hw, struct clk_rlx, hw)

static const char *const rlx_root_parent_names[] = {
	"clk25mhz",
};

static const char *const rlx_names_p1bus_div[] = { "usb_pll_2", "gpll0_3",
						   "gpll1_3", "gpll2_3" };

static const char *const rlx_names_p1bus_dec[] = { "p1bus_ck_div" };

static const char *const rlx_names_bus_div[] = { "usb_pll_2", "gpll0_2",
						 "gpll1_3", "gpll2_2" };

static const char *const rlx_names_bus_dec[] = { "bus_ck_div" };

static const char *const rlx_names_cpu_div[] = {
	"usb_pll",
	"gpll0",
	"gpll1",
	"gpll2",
};

static const char *const rlx_names_cpu_dec[] = { "cpu_ck_div" };

static const char *const rlx_names_dram_div[] = { "usb_pll_2", "gpll0_3",
						  "gpll1_3", "gpll2_3" };

static const char *const rlx_names_dram_dec[] = { "dram_ck_div" };

static const char *const rlx_names_i2c_div[] = { "usb_pll_5", "gpll2_2",
						 "gpll2_5", "usb_pll_3" };

static const char *const rlx_names_uart_div[] = { "usb_pll_5", "gpll0_2",
						  "gpll0_3", "usb_pll_2" };

static const char *const rlx_names_xb2_div[] = { "usb_pll_2", "gpll0_2",
						 "gpll0_3" };

static const char *const rlx_names_macbypass_div[] = { "gpll0_2", "gpll1",
						       "gll2_2", "gpll3_2" };

static const char *const rlx_names_sd_crc_clk_div[] = { "usb_pll_5", "gpll0_3",
							"gpll0_5",
							"usb_pll_3" };

static const char *const rlx_names_sd_sam_clk_div[] = { "usb_pll_5", "gpll0_3",
							"gpll0_5",
							"usb_pll_3" };

static const char *const rlx_names_sd_pu_clk_div[] = { "usb_pll_5", "gpll0_3",
						       "gpll0_5", "usb_pll_3" };

static const char *const rlx_names_i2c1_div[] = { "usb_pll_5", "gpll2_2",
						  "gpll2_5", "usb_pll_3" };

static const char *const rlx_names_ssi_div[] = { "usb_pll_5", "gpll0_2",
						 "gpll0_3", "gpll0_5" };

static const char *const rlx_names_v[] = { "dummy" };

static inline u32 rts_clk_readl(u32 offset)
{
	return readl(clk_mapped_addr + offset);
}

static inline void rts_clk_writel(unsigned int val, u32 offset)
{
	writel(val, clk_mapped_addr + offset);
}

static inline u32 rts_pll_readl(u32 offset)
{
	return readl(pll_mapped_addr + offset);
}

static inline void rts_pll_writel(unsigned int val, u32 offset)
{
	writel(val, pll_mapped_addr + offset);
}

static void setchgbit(int nr)
{
	u32 val;

	if (nr == CK_CHANGE_NULL)
		return;
	val = rts_clk_readl(CLK_CHANGE_R);
	val |= (1 << nr);
	rts_clk_writel(val, CLK_CHANGE_R);
}

static void clrchgbit(int nr)
{
	u32 val;

	if (nr == CK_CHANGE_NULL)
		return;
	val = rts_clk_readl(CLK_CHANGE_R);
	val &= ~(1 << nr);
	rts_clk_writel(val, CLK_CHANGE_R);
}

static short bignumcmp(unsigned short *a, unsigned short *b)
{
	short i;

	for (i = BIG_SHORT_NUM - 1; i >= 0; i--) {
		if (a[i] > b[i])
			return 1;
		else if (a[i] < b[i])
			return -1;
	}

	return 0;
}

static short bignumsub(unsigned short *a, unsigned short *b)
{
	short i, sub = 0;

	for (i = 0; i < BIG_SHORT_NUM; i++) {
		if (a[i] < b[i]) {
			a[i] -= b[i] + sub;
			sub = 1;
		} else {
			a[i] -= b[i];
			if (!a[i] && sub) {
				a[i] = 0xffff;
				sub = 1;
			} else {
				a[i] -= sub;
				sub = 0;
			}
		}
	}

	return sub;
}

static short bignumsubs(unsigned short *a, unsigned short *b)
{
	short i, sub = 0;

	for (i = 0; i < BIG_SHORT_NUM + 1; i++) {
		if (a[i] < b[i]) {
			a[i] -= b[i];
			a[i] -= sub;
			sub = 1;
		} else {
			a[i] -= b[i];
			if (!a[i] && sub) {
				a[i] = 0xffff;
				sub = 1;
			} else {
				a[i] -= sub;
				sub = 0;
			}
		}
	}

	return sub;
}

static void bignummuls(unsigned short *c, unsigned short *a, unsigned short b)
{
	short i, k;
	unsigned short add;
	unsigned long m;

	for (i = 0; i < BIG_SHORT_NUM + 1; i++)
		c[i] = 0;

	for (i = 0; i < BIG_SHORT_NUM; i++) {
		m = (unsigned long)a[i] * (unsigned long)b;
		c[i] += m & 0xffff;

		if (c[i] < (m & 0xffff))
			add = (m >> 16) + 1;
		else
			add = m >> 16;
		k = i + 1;

		for (; c[k] += add, c[k] < add; add = 1, k++)
			;
	}
}

static void bignummul(unsigned short *c, unsigned short *a, unsigned short *b)
{
	short i, j, k;
	unsigned short add;
	unsigned long m;

	memset((void *)c, 0, BIG_LONG_NUM * 2);

	for (i = 0; i < BIG_SHORT_NUM; i++) {
		for (j = 0; j < BIG_SHORT_NUM; j++) {
			m = (unsigned long)a[i] * (unsigned long)b[j];

			c[i + j] += m & 0xffff;
			if (c[i + j] < (m & 0xffff))
				add = (m >> 16) + 1;
			else
				add = m >> 16;
			k = i + j + 1;

			for (; c[k] += add, c[k] < add; add = 1, k++)
				;
		}
	}
}

static void bignumdiv(unsigned short *a, unsigned short *c, unsigned short *b)
{
	short i, h;
	unsigned long m, n;
	unsigned short *d, e[BIG_SHORT_NUM + 1];

	for (i = 0; i < BIG_SHORT_NUM; i++)
		a[i] = 0;

	d = (unsigned short *)&c[BIG_SHORT_NUM];

	for (i = BIG_SHORT_NUM - 1; i >= 0; i--) {
		for (; h = bignumcmp(d, b), h >= 0; bignumsub(d, b), a[i + 1]++)
			;

		d = (unsigned short *)&c[i];

		do {
			m = ((unsigned long)c[i + BIG_SHORT_NUM] << 16) +
			    (unsigned long)c[i + BIG_SHORT_NUM - 1];
			n = m / ((unsigned long)b[BIG_SHORT_NUM - 1] + 1);
			if (n)
				a[i] += n;
			else {
				if (m > b[BIG_SHORT_NUM - 1]) {
					d[BIG_SHORT_NUM - 1] =
						1 - bignumsub(d, b);
					a[i]++;
				}
				break;
			}

			memset((void *)e, 0, (BIG_SHORT_NUM + 1) << 1);

			bignummuls(e, b, (unsigned short)n);
			bignumsubs(d, e);

		} while (1);
	}

	for (; h = bignumcmp(d, b), h >= 0; bignumsub(d, b), a[0]++)
		;
}

static long rlx_gpll_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	u32 parent_rate = *prate;
	int n = rate / parent_rate;
	unsigned long round_rate;
	u64 t = rate;

	t <<= 12;
	t += (parent_rate >> 1);
	bignumdiv((unsigned short *)&n, (unsigned short *)&t,
		  (unsigned short *)&parent_rate);
	if ((n & 0xfff) != 0)
		n++;
	t = 0;
	bignummul((unsigned short *)&t, (unsigned short *)&parent_rate,
		  (unsigned short *)&n);

	round_rate = t >> 12;

	pr_debug("%s round:%lu\n", clk_hw_get_name(hw), round_rate);

	return round_rate;
}

static int rlx_gpll_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	u64 t = rate;
	u32 n, f, reg;

	struct clk_rlx *clk = to_clk_rlx(hw);

	t <<= 12;
	t += (parent_rate >> 1);
	bignumdiv((unsigned short *)&n, (unsigned short *)&t,
		  (unsigned short *)&parent_rate);

	if ((n & 0xfff) != 0)
		n++;
	f = n & 0xfff;
	n >>= 12;

	reg = (f << 9) + (n - 2);
	rts_pll_writel(reg, clk->clkreg + GPLL_SCCG_CFG1);

	return 0;
}

static int rlx_gpll_is_enabled(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 val;

	val = rts_pll_readl(clk->clkreg + GPLL_CTRL);

	return (val & GPLL_EN);
}

static int rlx_gpll_enable_clk(struct clk_hw *hw)
{
	u32 reg;
	u32 time = 5000;
	struct clk_rlx *clk = to_clk_rlx(hw);

	reg = rts_pll_readl(clk->clkreg + GPLL_SCCG_CFG2);
	reg |= CMU_SSC_RSTB;
	rts_pll_writel(reg, clk->clkreg + GPLL_SCCG_CFG2);

	reg = rts_pll_readl(clk->clkreg + GPLL_SCCG_CFG3);
	reg |= CMU_SSC_EN;
	rts_pll_writel(reg, clk->clkreg + GPLL_SCCG_CFG3);

	reg = rts_pll_readl(clk->clkreg + GPLL_CTRL);
	reg |= PPOW_LDO;
	rts_pll_writel(reg, clk->clkreg + GPLL_CTRL);
	udelay(5);
	reg |= GPLL_EN;
	rts_pll_writel(reg, clk->clkreg + GPLL_CTRL);

	while (--time) {
		reg = rts_pll_readl(clk->clkreg + GPLL_STATUS);
		if (reg & SYSPLL_CK_RDY)
			break;
		udelay(1);
	}

	if (time == 0) {
		pr_err("%s enable failed\n", clk_hw_get_name(hw));
		return -ETIMEDOUT;
	}

	//xb2flush();
	return 0;
}

static void rlx_gpll_disable_clk(struct clk_hw *hw)
{
	u32 reg;
	struct clk_rlx *clk = to_clk_rlx(hw);

	reg = rts_pll_readl(clk->clkreg + GPLL_CTRL);
	reg &= ~GPLL_EN;
	rts_pll_writel(reg, clk->clkreg + GPLL_CTRL);
	reg &= ~PPOW_LDO;
	rts_pll_writel(reg, clk->clkreg + GPLL_CTRL);

	reg = rts_pll_readl(clk->clkreg + GPLL_SCCG_CFG0);
	reg &= ~REG_EN_SSC;
	rts_pll_writel(reg, clk->clkreg + GPLL_SCCG_CFG0);

	//xb2flush();
}

static unsigned long rlx_gpll_recalc(struct clk_hw *hw,
				     unsigned long parent_rate)
{
	u32 reg, n, f, rate;
	u64 t;
	struct clk_rlx *clk = to_clk_rlx(hw);

	reg = rts_pll_readl(clk->clkreg + GPLL_SCCG_CFG1);
	reg &= 0x1fffff;
	n = (reg & 0x1ff) + 2;
	f = reg >> 9;

	n <<= 12;
	n += f;

	bignummul((unsigned short *)&t, (unsigned short *)&parent_rate,
		  (unsigned short *)&n);

	rate = (t >> 12);

	pr_debug("%s prate: %u\n", clk_hw_get_name(hw), rate);

	return rate;
}

static const struct clk_ops rlx_gpll_ops = {
	.is_enabled = rlx_gpll_is_enabled,
	.enable = rlx_gpll_enable_clk,
	.disable = rlx_gpll_disable_clk,
	.round_rate = rlx_gpll_round_rate,
	.set_rate = rlx_gpll_set_rate,
	.recalc_rate = rlx_gpll_recalc,
};

static const struct clk_ops rlx_gpll0_ops = {
	.is_enabled = rlx_gpll_is_enabled,
	.enable = rlx_gpll_enable_clk,
	.disable = rlx_gpll_disable_clk,
	.round_rate = rlx_gpll_round_rate,
	.recalc_rate = rlx_gpll_recalc,
};

int rts_gpll_ssc_config(struct clk *pll, u32 ppm, u32 freq)
{
	u32 tbase, rate, step, reg;
	u64 tl;
	u32 n1, f1, n2, t1, t2;

	struct clk_rlx *clk = to_clk_rlx(__clk_get_hw(pll));

	tbase = 25000000 / freq;
	tbase &= ~1;

	rate = clk_get_rate(pll);

	n1 = rts_pll_readl(clk->clkreg + GPLL_SCCG_CFG1);
	f1 = n1 >> 9;
	n1 &= 0x1ff;
	n1 += 2;

	n1 <<= 12;
	n1 += f1;

	t1 = 1000000 - ppm;
	bignummul((unsigned short *)&tl, (unsigned short *)&rate,
		  (unsigned short *)&t1);

	t1 = 1000000;
	bignumdiv((unsigned short *)&t2, (unsigned short *)&tl,
		  (unsigned short *)&t1);

	t1 = 25000000;
	tl = t2;
	tl <<= 12;
	tl += (t1 >> 1);
	bignumdiv((unsigned short *)&n2, (unsigned short *)&tl,
		  (unsigned short *)&t1);

	n1 -= n2;

	step = (n1 << 4) / tbase;

	reg = rts_pll_readl(clk->clkreg + GPLL_SCCG_CFG0);
	reg &= 0xfff000;
	reg |= (step << 12);
	reg |= REG_EN_SSC;

	rts_pll_writel(reg, clk->clkreg + GPLL_SCCG_CFG0);

	return 0;
}
EXPORT_SYMBOL_GPL(rts_gpll_ssc_config);

static int rlx_dma_enable_clk(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg;

	reg = rts_clk_readl(clk->clkreg) & ~0x3;
	rts_clk_writel(reg, clk->clkreg);
	//xb2flush();

	return 0;
}

static void rlx_dma_disable_clk(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg;

	reg = rts_clk_readl(clk->clkreg) & ~0x3;
	reg |= 0x2;
	rts_clk_writel(reg, clk->clkreg);
	//xb2flush();
}

static const struct clk_ops rlx_dma_clk_ops = {
	.enable = rlx_dma_enable_clk,
	.disable = rlx_dma_disable_clk,
};

static int usbphy_enable_clk(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg;

	reg = rts_clk_readl(clk->clkreg);
	if (!strcmp(clk_hw_get_name(hw), "usbphy_host_ck"))
		reg |= USBPHY_HOST_CLK_EN;
	else if (!strcmp(clk_hw_get_name(hw), "usbphy_dev_ck"))
		reg |= USBPHY_DEV_CLK_EN;
	rts_clk_writel(reg, clk->clkreg);
	//xb2flush();

	return 0;
}

static void usbphy_disable_clk(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg;

	reg = rts_clk_readl(clk->clkreg);
	if (!strcmp(clk_hw_get_name(hw), "usbphy_host_ck"))
		reg &= ~USBPHY_HOST_CLK_EN;
	else if (!strcmp(clk_hw_get_name(hw), "usbphy_dev_ck"))
		reg &= ~USBPHY_DEV_CLK_EN;
	rts_clk_writel(reg, clk->clkreg);
	//xb2flush();
}

static const struct clk_ops usbphy_divider_ops = {
	.enable = usbphy_enable_clk,
	.disable = usbphy_disable_clk,
};

static int rlx_decset_rate(struct clk_hw *hw, unsigned long rate,
			   unsigned long parent_rate)
{
	u64 p = parent_rate;
	unsigned long divisor;
	unsigned long f, n;
	u32 divreg, reg, t;
	int i;
	unsigned long flags;

	struct clk_rlx *clk = to_clk_rlx(hw);

	pr_debug("setrate:%s p:%ld r:%ld\n", clk_hw_get_name(hw), parent_rate,
		 rate);

	p <<= 6;
	p += (rate >> 1);

	bignumdiv((unsigned short *)&divisor, (unsigned short *)&p,
		  (unsigned short *)&rate);

	for (i = 7; i >= 0; i--) {
		t = ((i + 1) << 6);
		if (t <= divisor)
			break;
	}

	if (i < 0) {
		n = 0;
		f = 0;
	} else {
		n = i;
		f = 64 - (((n + 1) << 12) + (divisor >> 1)) / divisor;
	}

	divreg = (f << 16) | (i << 8);

	clk->rate = rate;

	clk->reg_v[clk->reg_i] &= ~0x3fff00;
	clk->reg_v[clk->reg_i] |= divreg;

	spin_lock_irqsave(&clk_spinlock, flags);

	reg = rts_clk_readl(clk->clkreg) & ~0x3fffff;

	setchgbit(clk->clk_change);

	reg |= clk->reg_v[clk->reg_i];
	rts_clk_writel(reg, clk->clkreg);

	clrchgbit(clk->clk_change);
	//xb2flush();

	spin_unlock_irqrestore(&clk_spinlock, flags);

	pr_debug("setrate: %s reg:%x\n", clk_hw_get_name(hw), reg);

	return 0;
}

static unsigned long rlx_decrecalc(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 reg, n, f, rate;
	struct clk_rlx *clk = to_clk_rlx(hw);
	u64 tm;
	u32 t;

	reg = rts_clk_readl(clk->clkreg) & 0xffff00;

	n = (reg & 0x700) >> 8;
	f = (reg & 0x3f0000) >> 16;

	pr_debug("recalc:%s %x %x %x %ld\n", clk_hw_get_name(hw), reg, n, f,
		 parent_rate);

	tm = 0;
	t = 64 - f;
	bignummul((unsigned short *)&tm, (unsigned short *)&parent_rate,
		  (unsigned short *)&t);

	tm += ((n + 1) << 5);
	t = (n + 1) << 6;
	rate = 0;
	bignumdiv((unsigned short *)&rate, (unsigned short *)&tm,
		  (unsigned short *)&t);

	pr_debug("recalc:%s r:%u, %x\n", clk_hw_get_name(hw), rate, n);

	return rate + 1;
}

static long rlx_decround_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long *prate)
{
	unsigned long divisor;
	unsigned long f = 0, n = 1, r = 0;
	int i;
	u32 p = *prate;
	u32 t;
	u64 tm;

	tm = p;
	tm <<= 6;
	// tm += (rate>>1);

	bignumdiv((unsigned short *)&divisor, (unsigned short *)&tm,
		  (unsigned short *)&rate);

	for (i = 7; i >= 0; i--) {
		t = ((i + 1) << 6);
		if (t <= divisor)
			break;
	}

	if (i < 0) {
		n = 0;
		f = 0;
	} else {
		n = i;
		f = 64 - (((n + 1) << 12) + (divisor >> 1)) / divisor;
	}

	pr_debug("round:n %ld f %ld\n", n, f);

	tm = 0;
	t = 64 - f;
	bignummul((unsigned short *)&tm, (unsigned short *)&p,
		  (unsigned short *)&t);

	tm += ((n + 1) << 5);
	t = (n + 1) << 6;
	r = 0;
	bignumdiv((unsigned short *)&r, (unsigned short *)&tm,
		  (unsigned short *)&t);

	pr_debug("%s round:%ld\n", clk_hw_get_name(hw), r);

	return r;
}

static int rlx_clk_is_enabled(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 val;

	val = rts_clk_readl(clk->clkreg);

	return (val & CLK_ENABLE);
}

static int rlx_enable_clk(struct clk_hw *hw)
{
	u32 time = 5000;
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg = rts_clk_readl(clk->clkreg);

	reg |= CLK_ENABLE;
	rts_clk_writel(reg, clk->clkreg);

	while (--time) {
		if (rts_clk_readl(clk->clkreg) & CLK_ENABLE)
			break;
		udelay(1);
	}

	if (time == 0) {
		pr_err("%s enable failed\n", clk_hw_get_name(hw));
		return -ETIMEDOUT;
	}

	//xb2flush();
	return 0;
}

static void rlx_disable_clk(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg;

	reg = rts_clk_readl(clk->clkreg);
	reg &= ~CLK_ENABLE;
	rts_clk_writel(reg, clk->clkreg);
	//xb2flush();
}

static const struct clk_ops rlx_decdivider_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_decround_rate,
	.set_rate = rlx_decset_rate,
	.recalc_rate = rlx_decrecalc,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_parent(struct clk_hw *hw, u8 field_val)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg;

	reg = clk->reg_v[clk->reg_i] & ~0x3;
	clk->reg_v[clk->reg_i] = (reg | field_val);

	return 0;
}

static u8 rlx_get_parent(struct clk_hw *hw)
{
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 reg;

	reg = clk->reg_v[clk->reg_i];
	reg &= 3;

	return reg;
}

static int rlx_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	unsigned long parent_rate = req->best_parent_rate;
	unsigned long rate = req->rate;
	unsigned long divisor = (parent_rate + rate / 2) / rate;
	int i;

	for (i = 0; i < ARRAY_SIZE(div_array); i++)
		if (divisor <= div_array[i])
			break;

	if (i == ARRAY_SIZE(div_array))
		i--;

	divisor = div_array[i];

	pr_debug("round: %s %lu\n", clk_hw_get_name(hw), divisor);

	req->rate = parent_rate / divisor;

	return 0;
}

static int rlx_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	u32 divreg;
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 div = parent_rate / rate;
	u32 i;

	pr_debug("setrate:%s div: %u p:%lu r:%lu\n", clk_hw_get_name(hw), div,
		 parent_rate, rate);

	clk->rate = rate;

	for (i = 0; i < ARRAY_SIZE(div_array_short); i++)
		if (div <= div_array_short[i])
			break;

	if (i == ARRAY_SIZE(div_array_short))
		i--;

	divreg = i << 2;

	pr_debug("setrate:%s div:%u reg:%x\n", clk_hw_get_name(hw),
		 div_array[i], divreg);

	clk->reg_v[clk->reg_i] &= ~0x0c;
	clk->reg_v[clk->reg_i] |= divreg;

	return 0;
}

static unsigned long rlx_recalc(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 div;
	struct clk_rlx *clk = to_clk_rlx(hw);

	div = clk->reg_v[clk->reg_i] & 0x1c;
	div >>= 2;
	div = div_array[div];

	pr_debug("recalc:%s p: %lu, div: %u\n", clk_hw_get_name(hw),
		 parent_rate, div);

	return parent_rate / div;
}

static const struct clk_ops rlx_divider_ops = {
	.determine_rate = rlx_determine_rate,
	.set_rate = rlx_set_rate,
	.recalc_rate = rlx_recalc,
	.set_parent = rlx_set_parent,
	.get_parent = rlx_get_parent,
};

static int rlx_set_rate_s(struct clk_hw *hw, unsigned long rate,
			  unsigned long parent_rate)
{
	u32 divreg;
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 div = parent_rate / rate;
	u32 i;
	u32 reg;
	unsigned long flags;

	pr_debug("setrate:%s div: %u p:%lu r:%lu\n", clk_hw_get_name(hw), div,
		 parent_rate, rate);

	clk->rate = rate;

	for (i = 0; i < ARRAY_SIZE(div_array); i++)
		if (div <= div_array[i])
			break;

	if (i == ARRAY_SIZE(div_array))
		i--;

	divreg = i << 2;

	pr_debug("setrate:%s div:%u reg:%x\n", clk_hw_get_name(hw),
		 div_array[i], divreg);

	clk->reg_v[clk->reg_i] &= ~0x1c;
	clk->reg_v[clk->reg_i] |= divreg;

	reg = rts_clk_readl(clk->clkreg) & ~0x1f;

	spin_lock_irqsave(&clk_spinlock, flags);
	setchgbit(clk->clk_change);

	reg |= clk->reg_v[clk->reg_i];
	rts_clk_writel(reg, clk->clkreg);
	clrchgbit(clk->clk_change);
	spin_unlock_irqrestore(&clk_spinlock, flags);

	return 0;
}

static unsigned long rlx_recalc_s(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 div;
	struct clk_rlx *clk = to_clk_rlx(hw);

	div = rts_clk_readl(clk->clkreg) & 0x1c;
	div >>= 2;
	div = div_array[div];

	pr_debug("recalc:%s p: %lu, div: %u\n", clk_hw_get_name(hw),
		 parent_rate, div);

	return parent_rate / div + 1;
}

static const struct clk_ops rlx_divider_ops_s = {
	.determine_rate = rlx_determine_rate,
	.set_rate = rlx_set_rate_s,
	.recalc_rate = rlx_recalc_s,
	.set_parent = rlx_set_parent,
	.get_parent = rlx_get_parent,
};

static int rlx_determine_rate_l(struct clk_hw *hw, struct clk_rate_request *req)
{
	unsigned long parent_rate = req->best_parent_rate;
	unsigned long rate = req->rate;
	unsigned long divisor = (parent_rate + rate / 2) / rate;
	int i;

	for (i = 0; i < ARRAY_SIZE(div_array_long); i++)
		if (divisor <= div_array_long[i])
			break;

	if (i == ARRAY_SIZE(div_array_long))
		i--;

	divisor = div_array_long[i];

	pr_debug("round: %s %lu\n", clk_hw_get_name(hw), divisor);

	req->rate = parent_rate / divisor;

	return 0;
}

static int rlx_determine_rate_c(struct clk_hw *hw, struct clk_rate_request *req)
{
	unsigned long parent_rate = req->best_parent_rate;
	unsigned long rate = req->rate;
	unsigned long divisor = (parent_rate + rate / 2) / rate;

	if (divisor > 254)
		divisor = 254;
	else if (divisor == 0)
		divisor = 1;

	if (divisor != 1) {
		divisor >>= 1;
		divisor <<= 1;
	}

	pr_debug("round: %s %lu\n", clk_hw_get_name(hw), divisor);

	req->rate = parent_rate / divisor;

	return 0;
}

static int rlx_set_rate_c(struct clk_hw *hw, unsigned long rate,
			  unsigned long parent_rate)
{
	u32 reg;
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 div = (parent_rate + (rate >> 1)) / rate;
	unsigned long flags;

	pr_debug("setrate:%s div: %u p:%lu r:%lu\n", clk_hw_get_name(hw), div,
		 parent_rate, rate);

	if (div > 30)
		div = 30;
	else if (div == 0)
		div = 1;

	clk->rate = rate;

	if (div != 1)
		div >>= 1;
	else
		div = 0;

	clk->reg_v[clk->reg_i] &= ~0x1fc;
	clk->reg_v[clk->reg_i] |= (div << 2);

	spin_lock_irqsave(&clk_spinlock, flags);
	reg = rts_clk_readl(clk->clkreg) & ~0x1ff;

	setchgbit(clk->clk_change);

	reg |= clk->reg_v[clk->reg_i];
	rts_clk_writel(reg, clk->clkreg);
	clrchgbit(clk->clk_change);

	//xb2flush();
	spin_unlock_irqrestore(&clk_spinlock, flags);

	return 0;
}

static unsigned long rlx_recalc_c(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 div;
	struct clk_rlx *clk = to_clk_rlx(hw);

	div = rts_clk_readl(clk->clkreg) & 0x1fc;
	div >>= 2;
	div <<= 1;

	if (div == 0)
		div++;

	pr_debug("recalc:%s p: %lu, div: %u\n", clk_hw_get_name(hw),
		 parent_rate, div);

	return (parent_rate + (div >> 1)) / div + 1;
}

static const struct clk_ops rlx_divider_ops_c = {
	.is_enabled = rlx_clk_is_enabled,
	.determine_rate = rlx_determine_rate_c,
	.set_rate = rlx_set_rate_c,
	.set_parent = rlx_set_parent,
	.get_parent = rlx_get_parent,
	.recalc_rate = rlx_recalc_c,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_l(struct clk_hw *hw, unsigned long rate,
			  unsigned long parent_rate)
{
	u32 divreg;
	struct clk_rlx *clk = to_clk_rlx(hw);
	u32 div = parent_rate / rate;
	u32 i;
	u32 reg;
	unsigned long flags;

	pr_debug("setrate:%s div: %u p:%lu r:%lu\n", clk_hw_get_name(hw), div,
		 parent_rate, rate);

	clk->rate = rate;

	for (i = 0; i < ARRAY_SIZE(div_array_long); i++)
		if (div <= div_array_long[i])
			break;

	if (i == ARRAY_SIZE(div_array_long))
		i--;

	divreg = i << 2;

	pr_debug("setrate:%s div:%u reg:%x\n", clk_hw_get_name(hw),
		 div_array_long[i], divreg);

	clk->reg_v[clk->reg_i] &= ~0x3c;
	clk->reg_v[clk->reg_i] |= divreg;

	reg = rts_clk_readl(clk->clkreg) & ~0x3f;

	spin_lock_irqsave(&clk_spinlock, flags);
	setchgbit(clk->clk_change);

	reg |= clk->reg_v[clk->reg_i];
	rts_clk_writel(reg, clk->clkreg);
	clrchgbit(clk->clk_change);
	spin_unlock_irqrestore(&clk_spinlock, flags);

	return 0;
}

static unsigned long rlx_recalc_l(struct clk_hw *hw, unsigned long parent_rate)
{
	u32 div;
	struct clk_rlx *clk = to_clk_rlx(hw);

	div = rts_clk_readl(clk->clkreg) & 0x3c;
	div >>= 2;
	div = div_array_long[div];

	pr_debug("recalc:%s p: %lu, div: %u\n", clk_hw_get_name(hw),
		 parent_rate, div);

	return parent_rate / div + 1;
}

static const struct clk_ops rlx_divider_ops_l = {
	.determine_rate = rlx_determine_rate_l,
	.set_rate = rlx_set_rate_l,
	.recalc_rate = rlx_recalc_l,
	.set_parent = rlx_set_parent,
	.get_parent = rlx_get_parent,
};

static int rlx_set_rate_cpu(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk *clk0, *clk1, *clk2;
	int ret = 0;

	clk1 = clk_get(NULL, "cpu_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	clk2 = clk_get(NULL, "cpu_ck_dec");
	if (IS_ERR(clk2)) {
		clk_put(clk1);
		return PTR_ERR(clk2);
	}

	clk_set_parent(clk2, clk1);

	switch (rate) {
	case 1000000000:
		if (pll0_rate == PLL0_800M) {
			ret = -EINVAL;
			goto out;
		}
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 1000000000);
		clk_set_rate(clk2, 1000000000);
		break;
	case 800000000:
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		if (pll0_rate == PLL0_800M)
			clk_set_rate(clk1, 800000000);
		else
			clk_set_rate(clk1, 1000000000);
		clk_set_rate(clk2, 800000000);
		break;
	case 600000000:
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		if (pll0_rate == PLL0_800M)
			clk_set_rate(clk1, 800000000);
		else
			clk_set_rate(clk1, 1000000000);
		clk_set_rate(clk2, 600000000);
		break;
	case 500000000:
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		if (pll0_rate == PLL0_800M)
			clk_set_rate(clk1, 800000000);
		else
			clk_set_rate(clk1, 500000000);
		clk_set_rate(clk2, 500000000);
		break;
	case 400000000:
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		if (pll0_rate == PLL0_800M)
			clk_set_rate(clk1, 400000000);
		else
			clk_set_rate(clk1, 500000000);
		clk_set_rate(clk2, 400000000);
		break;
	case 240000000:
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		if (pll0_rate == PLL0_800M)
			clk_set_rate(clk1, 400000000);
		else
			clk_set_rate(clk1, 250000000);
		clk_set_rate(clk2, 240000000);
		break;
	case 200000000:
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		if (pll0_rate == PLL0_800M)
			clk_set_rate(clk1, 200000000);
		else
			clk_set_rate(clk1, 250000000);
		clk_set_rate(clk2, 200000000);
		break;
	case 160000000:
		clk0 = clk_get(NULL, "gpll0");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		if (pll0_rate == PLL0_800M)
			clk_set_rate(clk1, 200000000);
		else
			clk_set_rate(clk1, 250000000);
		clk_set_rate(clk2, 160000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		ret = -EINVAL;
		goto out;
	}

	clk_put(clk0);
out:
	clk_put(clk1);
	clk_put(clk2);

	return ret;
}

static unsigned long rlx_recalc_cpu(struct clk_hw *hw,
				    unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate = 0;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "cpu_ck_dec");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);
	rate = clk_get_rate(clk0) - 1;
	clk_put(clk0);

	return rate;
}

static long rlx_round_rate_v(struct clk_hw *hw, unsigned long rate,
			     unsigned long *prate)
{
	return rate;
}

static const struct clk_ops rlx_clk_cpu_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_cpu,
	.recalc_rate = rlx_recalc_cpu,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_p1bus(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk *clk0, *clk1, *clk2;
	int ret = 0;

	clk1 = clk_get(NULL, "p1bus_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	clk2 = clk_get(NULL, "p1bus_ck_dec");
	if (IS_ERR(clk2)) {
		clk_put(clk1);
		return PTR_ERR(clk2);
	}

	clk_set_parent(clk2, clk1);

	switch (rate) {
	case 396000000:
		clk0 = clk_get(NULL, "gpll1_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 396000000);
		clk_set_rate(clk2, 396000000);
		break;
	case 360000000:
		clk0 = clk_get(NULL, "gpll1_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 360000000);
		clk_set_rate(clk2, 360000000);
		break;
	case 333333334:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 333333334);
		clk_set_rate(clk2, 333333334);
		break;
	case 266666667:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 266666667);
		clk_set_rate(clk2, 266666667);
		break;
	case 240000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 240000000);
		clk_set_rate(clk2, 240000000);
		break;
	case 120000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 120000000);
		clk_set_rate(clk2, 120000000);
		break;
	case 60000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 60000000);
		clk_set_rate(clk2, 60000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		ret = -EINVAL;
		goto out;
	}

	clk_put(clk0);
out:
	clk_put(clk1);
	clk_put(clk2);

	return ret;
}

static unsigned long rlx_recalc_p1bus(struct clk_hw *hw,
				      unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 25000000;

	clk0 = clk_get(NULL, "p1bus_ck_dec");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_p1bus_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_p1bus,
	.recalc_rate = rlx_recalc_p1bus,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_bus(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk *clk0, *clk1, *clk2;
	int ret = 0;

	clk1 = clk_get(NULL, "bus_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	clk2 = clk_get(NULL, "bus_ck_dec");
	if (IS_ERR(clk2)) {
		clk_put(clk1);
		return PTR_ERR(clk2);
	}

	clk_set_parent(clk2, clk1);

	switch (rate) {
	case 2140000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 17142857);
		clk_set_rate(clk2, 2140000);
		break;
	case 60000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 60000000);
		clk_set_rate(clk2, 60000000);
		break;
	case 100000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 120000000);
		clk_set_rate(clk2, 100000000);
		break;
	case 120000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 120000000);
		clk_set_rate(clk2, 120000000);
		break;
	case 125000000:
		clk0 = clk_get(NULL, "gpll2_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 125000000);
		clk_set_rate(clk2, 125000000);
		break;
	case 90000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 120000000);
		clk_set_rate(clk2, 90000000);
		break;

	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		ret = -EINVAL;
		goto out;
	}

	clk_put(clk0);
out:
	clk_put(clk1);
	clk_put(clk2);

	return ret;
}

static unsigned long rlx_recalc_bus(struct clk_hw *hw,
				    unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 30000000;

	clk0 = clk_get(NULL, "bus_ck_dec");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_bus_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_bus,
	.recalc_rate = rlx_recalc_bus,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_dram(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct clk *clk0, *clk1, *clk2;
	int ret = 0;

	clk1 = clk_get(NULL, "dram_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	clk2 = clk_get(NULL, "dram_ck_dec");
	if (IS_ERR(clk2)) {
		clk_put(clk1);
		return PTR_ERR(clk2);
	}

	clk_set_parent(clk2, clk1);

	switch (rate) {
	case 396000000:
		clk0 = clk_get(NULL, "gpll1_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 396000000);
		clk_set_rate(clk2, 396000000);
		break;
	case 360000000:
		clk0 = clk_get(NULL, "gpll1_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 360000000);
		clk_set_rate(clk2, 360000000);
		break;
	case 333333334:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 333333334);
		clk_set_rate(clk2, 333333334);
		break;
	case 266666667:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 266666667);
		clk_set_rate(clk2, 266666667);
		break;
	case 240000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			ret = PTR_ERR(clk0);
			goto out;
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 240000000);
		clk_set_rate(clk2, 240000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		ret = -EINVAL;
		goto out;
	}

	clk_put(clk0);
out:
	clk_put(clk1);
	clk_put(clk2);

	return ret;
}

static unsigned long rlx_recalc_dram(struct clk_hw *hw,
				     unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 25000000;

	clk0 = clk_get(NULL, "dram_ck_dec");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_dram_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_dram,
	.recalc_rate = rlx_recalc_dram,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_i2c(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "i2c_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 96000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 96000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "usb_pll_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "usb_pll_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_i2c(struct clk_hw *hw,
				    unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "i2c_ck_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_i2c_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_i2c,
	.recalc_rate = rlx_recalc_i2c,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_xb2(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "xb2_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 60000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 60000000);
		break;
	case 30000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 30000000);
		break;
	case 50000000:
		clk0 = clk_get(NULL, "gpll0_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 50000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_xb2(struct clk_hw *hw,
				    unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 30000000;

	clk0 = clk_get(NULL, "xb2_ck_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_xb2_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_xb2,
	.recalc_rate = rlx_recalc_xb2,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_uart(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "uart_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 120000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 120000000);
		break;
	case 96000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 96000000);
		break;
	case 24000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 24000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 40000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_uart(struct clk_hw *hw,
				     unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 24000000;

	clk0 = clk_get(NULL, "uart_ck_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_uart_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_uart,
	.recalc_rate = rlx_recalc_uart,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_uart1(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "uart1_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 120000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 120000000);
		break;
	case 96000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 96000000);
		break;
	case 24000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 24000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 40000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_uart1(struct clk_hw *hw,
				      unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 24000000;

	clk0 = clk_get(NULL, "uart1_ck_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_uart1_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_uart1,
	.recalc_rate = rlx_recalc_uart1,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_uart2(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "uart2_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 120000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 120000000);
		break;
	case 96000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 96000000);
		break;
	case 24000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 24000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "usb_pll_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 40000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_uart2(struct clk_hw *hw,
				      unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 24000000;

	clk0 = clk_get(NULL, "uart2_ck_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_uart2_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_uart2,
	.recalc_rate = rlx_recalc_uart2,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static const struct clk_ops rlx_clk_gate_ops = {
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
	.is_enabled = rlx_clk_is_enabled,
};

static int rlx_set_rate_macbypass(struct clk_hw *hw, unsigned long rate,
				  unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "macbypass_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 83000000:
		clk0 = clk_get(NULL, "gpll0_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 83333333);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_macbypass(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	clk0 = clk_get(NULL, "macbypass_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0);

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_macbypass_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_macbypass,
	.recalc_rate = rlx_recalc_macbypass,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static long rlx_round_rate_sd(struct clk_hw *hw, unsigned long rate,
			      unsigned long *prate)
{
	unsigned long round_rate = rate;

	if (pll0_rate == PLL0_800M) {
		if (rate == 50000000 || rate == 52000000)
			round_rate = 40000000;
		else if (rate == 100000000 || rate == 104000000)
			round_rate = 80000000;
		else if (rate == 25000000)
			round_rate = 33333333;
	} else if (pll0_rate == PLL0_1G) {
		if (rate == 52000000)
			round_rate = 50000000;
		else if (rate == 104000000)
			round_rate = 100000000;
		else if (rate == 25000000)
			round_rate = 33333333;
	}
	return round_rate;
}

static int rlx_set_rate_sd0_crc(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "sd0_crc_clk_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 100000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 100000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 50000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 50000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	case 33333333:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 33333333);
		break;
	default:
		pr_err("%s %ld not supported yet\n", clk_hw_get_name(hw), rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_sd0_crc(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "sd0_crc_clk_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_sd0_crc_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_sd,
	.set_rate = rlx_set_rate_sd0_crc,
	.recalc_rate = rlx_recalc_sd0_crc,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_sd1_crc(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "sd1_crc_clk_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 100000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 100000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 50000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 50000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	case 33333333:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 33333333);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_sd1_crc(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "sd1_crc_clk_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_sd1_crc_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_sd,
	.set_rate = rlx_set_rate_sd1_crc,
	.recalc_rate = rlx_recalc_sd1_crc,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_sd0_sample(struct clk_hw *hw, unsigned long rate,
				   unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "sd0_sam_clk_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 100000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 100000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 50000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 50000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	case 33333333:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 33333333);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_sd0_sample(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "sd0_sam_clk_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_sd0_sample_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_sd,
	.set_rate = rlx_set_rate_sd0_sample,
	.recalc_rate = rlx_recalc_sd0_sample,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_sd1_sample(struct clk_hw *hw, unsigned long rate,
				   unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "sd1_sam_clk_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 100000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 100000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 50000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 50000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	case 33333333:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 33333333);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_sd1_sample(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "sd1_sam_clk_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_sd1_sample_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_sd,
	.set_rate = rlx_set_rate_sd1_sample,
	.recalc_rate = rlx_recalc_sd1_sample,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_sd0_push(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "sd0_pu_clk_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 100000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 100000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 50000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 50000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	case 33333333:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 33333333);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_sd0_push(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "sd0_pu_clk_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_sd0_push_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_sd,
	.set_rate = rlx_set_rate_sd0_push,
	.recalc_rate = rlx_recalc_sd0_push,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_sd1_push(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "sd1_pu_clk_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 100000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 100000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 50000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 50000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "gpll0_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	case 33333333:
		clk0 = clk_get(NULL, "gpll0_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 33333333);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_sd1_push(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "sd1_pu_clk_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_sd1_push_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_sd,
	.set_rate = rlx_set_rate_sd1_push,
	.recalc_rate = rlx_recalc_sd1_push,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_i2c1(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "i2c1_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 96000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 96000000);
		break;
	case 80000000:
		clk0 = clk_get(NULL, "usb_pll_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 80000000);
		break;
	case 40000000:
		clk0 = clk_get(NULL, "usb_pll_3");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);

		clk_set_rate(clk1, 40000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_i2c1(struct clk_hw *hw,
				     unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "i2c1_ck_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_i2c1_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_i2c1,
	.recalc_rate = rlx_recalc_i2c1,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

static int rlx_set_rate_ssi(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk *clk0, *clk1;
	int ret = 0;

	clk1 = clk_get(NULL, "ssi_ck_div");
	if (IS_ERR(clk1))
		return PTR_ERR(clk1);

	switch (rate) {
	case 100000000:
		if (pll0_rate == PLL0_800M) {
			clk0 = clk_get(NULL, "gpll0_2");
		} else if (pll0_rate == PLL0_1G) {
			clk0 = clk_get(NULL, "gpll0_5");
		} else {
			ret = -EINVAL;
			goto out;
		}
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 100000000);
		break;
	case 96000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 96000000);
		break;
	case 24000000:
		clk0 = clk_get(NULL, "usb_pll_5");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 24000000);
		break;
	case 25000000:
		clk0 = clk_get(NULL, "gpll0_2");
		if (IS_ERR(clk0)) {
			clk_put(clk1);
			return PTR_ERR(clk0);
		}

		clk_set_parent(clk1, clk0);
		clk_set_rate(clk1, 25000000);
		break;
	default:
		pr_debug("%s %ld not supported yet\n", clk_hw_get_name(hw),
			 rate);
		clk_put(clk1);
		return -EINVAL;
	}

	clk_put(clk0);
out:
	clk_put(clk1);

	return ret;
}

static unsigned long rlx_recalc_ssi(struct clk_hw *hw,
				    unsigned long parent_rate)
{
	struct clk *clk0;
	u32 rate;

	if (clk_platform_type & TYPE_FPGA)
		return 50000000;

	clk0 = clk_get(NULL, "ssi_ck_div");
	if (IS_ERR(clk0))
		return PTR_ERR(clk0);

	rate = clk_get_rate(clk0) - 1;

	clk_put(clk0);

	return rate;
}

static const struct clk_ops rlx_clk_ssi_ops = {
	.is_enabled = rlx_clk_is_enabled,
	.round_rate = rlx_round_rate_v,
	.set_rate = rlx_set_rate_ssi,
	.recalc_rate = rlx_recalc_ssi,
	.enable = rlx_enable_clk,
	.disable = rlx_disable_clk,
};

DEFINE_CLK_RLX(gpll0, rlx_root_parent_names, rlx_gpll0_ops,
	       BSP_CLK_GPLL0_BASE_R, 0);
DEFINE_CLK_RLX(gpll1, rlx_root_parent_names, rlx_gpll_ops, BSP_CLK_GPLL1_BASE_R,
	       0);
DEFINE_CLK_RLX(gpll2, rlx_root_parent_names, rlx_gpll_ops, BSP_CLK_GPLL2_BASE_R,
	       0);
DEFINE_CLK_RLX(gpll3, rlx_root_parent_names, rlx_gpll_ops, BSP_CLK_GPLL3_BASE_R,
	       0);

DEFINE_CLK_RLX(dma_ck, rlx_root_parent_names, rlx_dma_clk_ops, DMA_CLK_CFG_R,
	       CK_CHANGE_NULL);
DEFINE_CLK_RLX(usbphy_host_ck, rlx_root_parent_names, usbphy_divider_ops,
	       USBPHY_CLK_CFG_R, CK_CHANGE_NULL);
DEFINE_CLK_RLX(usbphy_dev_ck, rlx_root_parent_names, usbphy_divider_ops,
	       USBPHY_CLK_CFG_R, CK_CHANGE_NULL);
DEFINE_CLK_RLX(ethernet_ck, rlx_root_parent_names, rlx_clk_gate_ops,
	       ETHERNET_CLK_CFG_R, CK_CHANGE_NULL);

DEFINE_CLK_RLX(p1bus_ck_div, rlx_names_p1bus_div, rlx_divider_ops,
	       P1BUS_CLK_CFG_R, P1BUS_CK_CHANGE);
DEFINE_CLK_RLX(p1bus_ck_dec, rlx_names_p1bus_dec, rlx_decdivider_ops,
	       P1BUS_CLK_CFG_R, P1BUS_CK_CHANGE);
DEFINE_CLK_RLX(p1bus_ck, rlx_names_v, rlx_clk_p1bus_ops, P1BUS_CLK_CFG_R,
	       P1BUS_CK_CHANGE);

DEFINE_CLK_RLX(cpu_ck_div, rlx_names_cpu_div, rlx_divider_ops, CPU_CLK_CFG_R,
	       CPU_CK_CHANGE);
DEFINE_CLK_RLX(cpu_ck_dec, rlx_names_cpu_dec, rlx_decdivider_ops, CPU_CLK_CFG_R,
	       CPU_CK_CHANGE);
DEFINE_CLK_RLX(cpu_ck, rlx_names_v, rlx_clk_cpu_ops, CPU_CLK_CFG_R,
	       CPU_CK_CHANGE);

DEFINE_CLK_RLX(bus_ck_div, rlx_names_bus_div, rlx_divider_ops_s, BUS_CLK_CFG_R,
	       BUS_CK_CHANGE);
DEFINE_CLK_RLX(bus_ck_dec, rlx_names_bus_dec, rlx_decdivider_ops, BUS_CLK_CFG_R,
	       BUS_CK_CHANGE);
DEFINE_CLK_RLX(bus_ck, rlx_names_v, rlx_clk_bus_ops, BUS_CLK_CFG_R,
	       BUS_CK_CHANGE);

DEFINE_CLK_RLX(dram_ck_div, rlx_names_dram_div, rlx_divider_ops, DRAM_CLK_CFG_R,
	       DRAM_CK_CHANGE);
DEFINE_CLK_RLX(dram_ck_dec, rlx_names_dram_dec, rlx_decdivider_ops,
	       DRAM_CLK_CFG_R, DRAM_CK_CHANGE);
DEFINE_CLK_RLX(dram_ck, rlx_names_v, rlx_clk_dram_ops, DRAM_CLK_CFG_R,
	       DRAM_CK_CHANGE);

DEFINE_CLK_RLX(i2c_ck_div, rlx_names_i2c_div, rlx_divider_ops_s, I2C_CLK_CFG_R,
	       CK_CHANGE_NULL);
DEFINE_CLK_RLX(i2c_ck, rlx_names_v, rlx_clk_i2c_ops, I2C_CLK_CFG_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(xb2_ck_div, rlx_names_xb2_div, rlx_divider_ops_s, XB2_CLK_CFG_R,
	       XB2_CK_CHANGE);
DEFINE_CLK_RLX(xb2_ck, rlx_names_v, rlx_clk_xb2_ops, XB2_CLK_CFG_R,
	       XB2_CK_CHANGE);

DEFINE_CLK_RLX(uart_ck_div, rlx_names_uart_div, rlx_divider_ops_s,
	       UART_CLK_CFG_R, CK_CHANGE_NULL);
DEFINE_CLK_RLX(uart_ck, rlx_names_v, rlx_clk_uart_ops, UART_CLK_CFG_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(ecc_ck, rlx_names_v, rlx_clk_gate_ops, RSA_CLK_EN_R,
	       CK_CHANGE_NULL);
DEFINE_CLK_RLX(sha_ck, rlx_names_v, rlx_clk_gate_ops, SHA_CLK_EN_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(trng_ck, rlx_names_v, rlx_clk_gate_ops, TRNG_CLK_CFG_R,
	       CK_CHANGE_NULL);
DEFINE_CLK_RLX(efuse_ck, rlx_names_v, rlx_clk_gate_ops, EFUSE_CLK_CFG_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(macbypass_div, rlx_names_macbypass_div, rlx_divider_ops,
	       MAC_BYPASS_CLK_CFG_R, CK_CHANGE_NULL);
DEFINE_CLK_RLX(macbypass_ck, rlx_names_v, rlx_clk_macbypass_ops,
	       MAC_BYPASS_CLK_CFG_R, CK_CHANGE_NULL);

DEFINE_CLK_RLX(cipher_ck, rlx_names_v, rlx_clk_gate_ops, CIPHER_CLK_CFG_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(sd0_crc_clk_div, rlx_names_sd_crc_clk_div, rlx_divider_ops_s,
	       SD0_CRC_CLK_CFG_REG, SD0_CK_CHANGE);
DEFINE_CLK_RLX(sd0_crc_clk, rlx_names_v, rlx_clk_sd0_crc_ops,
	       SD0_CRC_CLK_CFG_REG, SD0_CK_CHANGE);

DEFINE_CLK_RLX(sd0_sam_clk_div, rlx_names_sd_sam_clk_div, rlx_divider_ops_s,
	       SD0_SAMPLE_CLK_CFG_REG, SD0_CK_CHANGE);
DEFINE_CLK_RLX(sd0_sample_clk, rlx_names_v, rlx_clk_sd0_sample_ops,
	       SD0_SAMPLE_CLK_CFG_REG, SD0_CK_CHANGE);

DEFINE_CLK_RLX(sd0_pu_clk_div, rlx_names_sd_pu_clk_div, rlx_divider_ops_s,
	       SD0_PUSH_CLK_CFG_REG, SD0_CK_CHANGE);
DEFINE_CLK_RLX(sd0_push_clk, rlx_names_v, rlx_clk_sd0_push_ops,
	       SD0_PUSH_CLK_CFG_REG, SD0_CK_CHANGE);

DEFINE_CLK_RLX(sd1_crc_clk_div, rlx_names_sd_crc_clk_div, rlx_divider_ops_s,
	       SD1_CRC_CLK_CFG_REG, SD1_CK_CHANGE);
DEFINE_CLK_RLX(sd1_crc_clk, rlx_names_v, rlx_clk_sd1_crc_ops,
	       SD1_CRC_CLK_CFG_REG, SD1_CK_CHANGE);

DEFINE_CLK_RLX(sd1_sam_clk_div, rlx_names_sd_sam_clk_div, rlx_divider_ops_s,
	       SD1_SAMPLE_CLK_CFG_REG, SD1_CK_CHANGE);
DEFINE_CLK_RLX(sd1_sample_clk, rlx_names_v, rlx_clk_sd1_sample_ops,
	       SD1_SAMPLE_CLK_CFG_REG, SD1_CK_CHANGE);

DEFINE_CLK_RLX(sd1_pu_clk_div, rlx_names_sd_pu_clk_div, rlx_divider_ops_s,
	       SD1_PUSH_CLK_CFG_REG, SD1_CK_CHANGE);
DEFINE_CLK_RLX(sd1_push_clk, rlx_names_v, rlx_clk_sd1_push_ops,
	       SD1_PUSH_CLK_CFG_REG, SD1_CK_CHANGE);

DEFINE_CLK_RLX(uart1_ck_div, rlx_names_uart_div, rlx_divider_ops_s,
	       UART1_CLK_CFG_R, CK_CHANGE_NULL);
DEFINE_CLK_RLX(uart1_ck, rlx_names_v, rlx_clk_uart1_ops, UART1_CLK_CFG_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(uart2_ck_div, rlx_names_uart_div, rlx_divider_ops_s,
	       UART2_CLK_CFG_R, CK_CHANGE_NULL);
DEFINE_CLK_RLX(uart2_ck, rlx_names_v, rlx_clk_uart2_ops, UART2_CLK_CFG_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(i2c1_ck_div, rlx_names_i2c1_div, rlx_divider_ops_s,
	       I2C1_CLK_CFG_R, CK_CHANGE_NULL);
DEFINE_CLK_RLX(i2c1_ck, rlx_names_v, rlx_clk_i2c1_ops, I2C1_CLK_CFG_R,
	       CK_CHANGE_NULL);

DEFINE_CLK_RLX(ssi_ck_div, rlx_names_ssi_div, rlx_divider_ops_s,
	       SSI_CLK_CFG_REG, CK_CHANGE_NULL);
DEFINE_CLK_RLX(ssi_ck, rlx_names_v, rlx_clk_ssi_ops, SSI_CLK_CFG_REG,
	       CK_CHANGE_NULL);

static void rlx_check_clocks(struct clk *clks[], unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++)
		if (IS_ERR(clks[i]))
			pr_err("rlx clk %u: register failed with %ld\n", i,
			       PTR_ERR(clks[i]));
}

static struct clk *__init rlx_obtain_fixed_clock_from_dt(const char *name)
{
	struct of_phandle_args phandle;
	struct clk *clk = ERR_PTR(-ENODEV);
	char *path;

	path = kasprintf(GFP_KERNEL, "/clocks/%s", name);
	if (!path)
		return ERR_PTR(-ENOMEM);
	phandle.np = of_find_node_by_path(path);
	kfree(path);

	if (phandle.np) {
		clk = of_clk_get_from_provider(&phandle);
		of_node_put(phandle.np);
	}
	return clk;
}

static struct clk *rlx_obtain_fixed_clock(const char *name, unsigned long rate)
{
	struct clk *clk;

	clk = rlx_obtain_fixed_clock_from_dt(name);

	if (IS_ERR(clk))
		clk = clk_register_fixed_rate(NULL, name, NULL, 0, rate);

	return clk;
}

static int rts_pbus_clk_change(unsigned long event, const char *id)
{
	struct clk *clk;
	u32 pll1_rate;
	int ret = 0;

	if (event == PRE_RATE_CHANGE) {
		clk = clk_get(NULL, id);
		if (pll0_rate == PLL0_800M)
			ret = clk_set_rate(clk, 266666667);
		else if (pll0_rate == PLL0_1G)
			ret = clk_set_rate(clk, 333333334);
		clk_put(clk);
	} else if (event == POST_RATE_CHANGE) {
		clk = clk_get(NULL, id);
		pll1_rate = clk_get_rate(clk_get(NULL, "gpll1"));
		if (pll1_rate <= 1188000000 + 100000 &&
		    pll1_rate >= 1188000000 - 100000) {
			ret = clk_set_rate(clk, 396000000);
		} else if (pll1_rate <= 1080000000 + 100000 &&
			   pll1_rate >= 1080000000 - 100000) {
			ret = clk_set_rate(clk, 360000000);
		}
		clk_put(clk);
	}
	return ret;
}

static int rts_hclk_notifier_cb(struct notifier_block *nb, unsigned long event,
				void *data)
{
	struct clk_notifier_data *ndata = data;
	struct clk *clk;
	u32 rate;
	int ret = 0;

	pr_debug("%s: event %lu, old_rate %lu, new_rate: %lu\n", __func__,
		 event, ndata->old_rate, ndata->new_rate);

	ret = rts_pbus_clk_change(event, "p1bus_ck");
	if (ret)
		goto err;
	ret = rts_pbus_clk_change(event, "dram_ck");
	if (ret)
		goto err;

	if (event == POST_RATE_CHANGE) {
		clk = clk_get(NULL, "isp_ck");
		rate = clk_get_rate(clk);
		if (rate > 180000000 - 10000 && pll0_rate == PLL0_800M)
			clk_set_rate(clk, rate);
		clk_put(clk);

		clk = clk_get(NULL, "isp_zoom_ck");
		rate = clk_get_rate(clk);
		if (rate > 360000000 - 10000)
			clk_set_rate(clk, rate);
		clk_put(clk);
	}
err:
	return notifier_from_errno(ret);
}

static struct clk *rlx_register_clk(struct clk_rlx *rlxclk, int flags)
{
	struct clk *clk;
	struct clk_init_data init;
	int ret = 0;

	init.name = rlxclk->name;
	init.ops = rlxclk->ops;
	init.flags = flags;
	init.parent_names = rlxclk->parent_names;
	init.num_parents = rlxclk->num_parents;

	rlxclk->hw.init = &init;

	clk = clk_register(NULL, &rlxclk->hw);

	clk_register_clkdev(clk, rlxclk->name, NULL);

	if (!strcmp(rlxclk->name, "ssor_hclk_ck")) {
		rlxclk->clk_nb.notifier_call = rts_hclk_notifier_cb;
		ret = clk_notifier_register(clk, &rlxclk->clk_nb);
		if (ret)
			pr_err("%s: failed to register clock notifier for %s\n",
			       __func__, rlxclk->name);
	}
	return clk;
}

static struct clk *rlx_register_fixed_rate(struct device *dev, const char *name,
					   const char *parent_name,
					   unsigned long flags,
					   unsigned long fixed_rate)
{
	struct clk *clk;

	clk = clk_register_fixed_rate(dev, name, parent_name, flags,
				      fixed_rate);

	clk_register_clkdev(clk, name, NULL);

	return clk;
}

static struct clk *
rlx_register_fixed_factor(struct device *dev, const char *name,
			  const char *parent_name, unsigned long flags,
			  unsigned int mult, unsigned int div)
{
	struct clk *clk;

	clk = clk_register_fixed_factor(dev, name, parent_name, flags, mult,
					div);

	clk_register_clkdev(clk, name, NULL);

	return clk;
}

static void rlx_clock_hw_init(void)
{
	u32 reg;

	/* Disable usbphy */
	reg = rts_clk_readl(USBPHY_CLK_CFG_R);
	reg &= ~(USBPHY_HOST_CLK_EN | USBPHY_DEV_CLK_EN);
	rts_clk_writel(reg, USBPHY_CLK_CFG_R);

	/* Disable ephy */
	reg = rts_clk_readl(ETHERNET_CLK_CFG_R);
	reg &= ~CLK_ENABLE;
	rts_clk_writel(reg, ETHERNET_CLK_CFG_R);

	reg = rts_clk_readl(SSOR_CLK_OE_R);
	reg |= 1;
	rts_clk_writel(reg, SSOR_CLK_OE_R);
}

static void rlx_clocks_init(struct device_node *node)
{
	int i;
	int clksize;

	rlx_clock_hw_init();
	for (i = (u32)UART_CLK_LP_EN_R; i <= (u32)SSI_CLK_CFG_REG; i += 4)
		clk_reg_v[(i & 0x1ff) >> 2] = rts_clk_readl(i);

	clks[RLX_CLK_DUMMY] =
		rlx_register_fixed_rate(NULL, "dummy", NULL, 0, 100000000);
	clks[RLX_CLK_SYS_OSC] = rlx_obtain_fixed_clock("oscillator", 25000000);
	clks[RLX_CLK_USB_PLL] =
		rlx_register_fixed_rate(NULL, "usb_pll", NULL, 0, 480000000);
	clks[RLX_CLK_USB_PLL_2] = rlx_register_fixed_factor(
		NULL, "usb_pll_2", "usb_pll", CLK_SET_RATE_PARENT, 1, 2);
	clks[RLX_CLK_USB_PLL_3] = rlx_register_fixed_factor(
		NULL, "usb_pll_3", "usb_pll", CLK_SET_RATE_PARENT, 1, 3);
	clks[RLX_CLK_USB_PLL_5] = rlx_register_fixed_factor(
		NULL, "usb_pll_5", "usb_pll", CLK_SET_RATE_PARENT, 1, 5);
	clks[RLX_CLK_USB_PLL_7] = rlx_register_fixed_factor(
		NULL, "usb_pll_7", "usb_pll", CLK_SET_RATE_PARENT, 1, 7);

	clks[RLX_CLK_SYS_PLL0] = rlx_register_clk(
		&gpll0, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SYS_PLL1] = rlx_register_clk(
		&gpll1, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SYS_PLL2] = rlx_register_clk(
		&gpll2, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SYS_PLL3] = rlx_register_clk(
		&gpll3, CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SYS_PLL0_2] = rlx_register_fixed_factor(
		NULL, "gpll0_2", "gpll0", CLK_SET_RATE_PARENT, 1, 2);
	clks[RLX_CLK_SYS_PLL0_3] = rlx_register_fixed_factor(
		NULL, "gpll0_3", "gpll0", CLK_SET_RATE_PARENT, 1, 3);
	clks[RLX_CLK_SYS_PLL0_5] = rlx_register_fixed_factor(
		NULL, "gpll0_5", "gpll0", CLK_SET_RATE_PARENT, 1, 5);
	clks[RLX_CLK_SYS_PLL0_7] = rlx_register_fixed_factor(
		NULL, "gpll0_7", "gpll0", CLK_SET_RATE_PARENT, 1, 7);

	clks[RLX_CLK_SYS_PLL1_2] = rlx_register_fixed_factor(
		NULL, "gpll1_2", "gpll1", CLK_SET_RATE_PARENT, 1, 2);
	clks[RLX_CLK_SYS_PLL1_3] = rlx_register_fixed_factor(
		NULL, "gpll1_3", "gpll1", CLK_SET_RATE_PARENT, 1, 3);
	clks[RLX_CLK_SYS_PLL1_5] = rlx_register_fixed_factor(
		NULL, "gpll1_5", "gpll1", CLK_SET_RATE_PARENT, 1, 5);
	clks[RLX_CLK_SYS_PLL1_7] = rlx_register_fixed_factor(
		NULL, "gpll1_7", "gpll1", CLK_SET_RATE_PARENT, 1, 7);

	clks[RLX_CLK_SYS_PLL2_2] = rlx_register_fixed_factor(
		NULL, "gpll2_2", "gpll2", CLK_SET_RATE_PARENT, 1, 2);
	clks[RLX_CLK_SYS_PLL2_3] = rlx_register_fixed_factor(
		NULL, "gpll2_3", "gpll2", CLK_SET_RATE_PARENT, 1, 3);
	clks[RLX_CLK_SYS_PLL2_5] = rlx_register_fixed_factor(
		NULL, "gpll2_5", "gpll2", CLK_SET_RATE_PARENT, 1, 5);
	clks[RLX_CLK_SYS_PLL2_7] = rlx_register_fixed_factor(
		NULL, "gpll2_7", "gpll2", CLK_SET_RATE_PARENT, 1, 7);

	clks[RLX_CLK_SYS_PLL3_2] = rlx_register_fixed_factor(
		NULL, "gpll3_2", "gpll3", CLK_SET_RATE_PARENT, 1, 2);
	clks[RLX_CLK_SYS_PLL3_3] = rlx_register_fixed_factor(
		NULL, "gpll3_3", "gpll3", CLK_SET_RATE_PARENT, 1, 3);
	clks[RLX_CLK_SYS_PLL3_5] = rlx_register_fixed_factor(
		NULL, "gpll3_5", "gpll3", CLK_SET_RATE_PARENT, 1, 5);
	clks[RLX_CLK_SYS_PLL3_7] = rlx_register_fixed_factor(
		NULL, "gpll3_7", "gpll3", CLK_SET_RATE_PARENT, 1, 7);

	clks[RLX_CLK_DMA_CK] = rlx_register_clk(&dma_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_USBPHY_HOST_CK] =
		rlx_register_clk(&usbphy_host_ck, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_USBPHY_DEV_CK] =
		rlx_register_clk(&usbphy_dev_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_ETHERNET_CK] =
		rlx_register_clk(&ethernet_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_CPU_CK_DIV] =
		rlx_register_clk(&cpu_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_CPU_CK_DEC] =
		rlx_register_clk(&cpu_ck_dec, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_CPU_CK] = rlx_register_clk(&cpu_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_BUS_CK_DIV] =
		rlx_register_clk(&bus_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_BUS_CK_DEC] =
		rlx_register_clk(&bus_ck_dec, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_BUS_CK] = rlx_register_clk(&bus_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_DRAM_CK_DIV] =
		rlx_register_clk(&dram_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_DRAM_CK_DEC] =
		rlx_register_clk(&dram_ck_dec, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_DRAM_CK] = rlx_register_clk(&dram_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_I2C_CK_DIV] =
		rlx_register_clk(&i2c_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_I2C_CK] = rlx_register_clk(&i2c_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_XB2_CK_DIV] =
		rlx_register_clk(&xb2_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_XB2_CK] = rlx_register_clk(&xb2_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_UART_CK_DIV] =
		rlx_register_clk(&uart_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_UART_CK] = rlx_register_clk(&uart_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_CIPHER_CK] =
		rlx_register_clk(&cipher_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_RSA] = rlx_register_clk(&ecc_ck, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SHA] = rlx_register_clk(&sha_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_TRNG] = rlx_register_clk(&trng_ck, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_OTP] = rlx_register_clk(&efuse_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_MACBYPASS_CK_DIV] =
		rlx_register_clk(&macbypass_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_MACBYPASS_CK] =
		rlx_register_clk(&macbypass_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_SD0_CRC_CK_DIV] =
		rlx_register_clk(&sd0_crc_clk_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SD0_CRC_CK] =
		rlx_register_clk(&sd0_crc_clk, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_SD0_SAMPLE_CK_DIV] =
		rlx_register_clk(&sd0_sam_clk_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SD0_SAMPLE_CK] =
		rlx_register_clk(&sd0_sample_clk, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_SD0_PUSH_CK_DIV] =
		rlx_register_clk(&sd0_pu_clk_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SD0_PUSH_CK] =
		rlx_register_clk(&sd0_push_clk, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_SD1_CRC_CK_DIV] =
		rlx_register_clk(&sd1_crc_clk_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SD1_CRC_CK] =
		rlx_register_clk(&sd1_crc_clk, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_SD1_SAMPLE_CK_DIV] =
		rlx_register_clk(&sd1_sam_clk_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SD1_SAMPLE_CK] =
		rlx_register_clk(&sd1_sample_clk, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_SD1_PUSH_CK_DIV] =
		rlx_register_clk(&sd1_pu_clk_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SD1_PUSH_CK] =
		rlx_register_clk(&sd1_push_clk, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_P1BUS_CK_DIV] =
		rlx_register_clk(&p1bus_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_P1BUS_CK_DEC] =
		rlx_register_clk(&p1bus_ck_dec, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_P1BUS_CK] = rlx_register_clk(&p1bus_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_UART1_CK_DIV] =
		rlx_register_clk(&uart1_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_UART1_CK] = rlx_register_clk(&uart1_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_UART2_CK_DIV] =
		rlx_register_clk(&uart2_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_UART2_CK] = rlx_register_clk(&uart2_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_I2C1_CK_DIV] =
		rlx_register_clk(&i2c1_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_I2C1_CK] = rlx_register_clk(&i2c1_ck, CLK_IGNORE_UNUSED);

	clks[RLX_CLK_SSI_CK_DIV] =
		rlx_register_clk(&ssi_ck_div, CLK_IGNORE_UNUSED);
	clks[RLX_CLK_SSI_CK] = rlx_register_clk(&ssi_ck, CLK_IGNORE_UNUSED);

	clksize = RLX_CLK_NUM_SIZE;

	rlx_check_clocks(clks, clksize);

	clk_data.clks = clks;
	clk_data.clk_num = clksize;
	of_clk_add_provider(node, of_clk_src_onecell_get, &clk_data);

	pll0_rate = clk_get_rate(clk_get(NULL, "gpll0"));
}

static const struct of_device_id rlx_clk_match[] = {
	{
		.compatible = "realtek,rts493xa-clocks",
		.data = (void *)(TYPE_RTS493XA),
	},
	{}
};
MODULE_DEVICE_TABLE(of, rlx_clk_match);

static void __init rlx_clk_init(struct device_node *np)
{
	const struct of_device_id *of_id;

	of_id = of_match_node(rlx_clk_match, np);
	if (!of_id)
		return;
	clk_platform_type = (int)(of_id->data);

	/* fpga board */
	if (of_machine_is_compatible("realtek,rts_fpga"))
		clk_platform_type |= TYPE_FPGA;

	clk_mapped_addr = of_io_request_and_map(np, 0, of_node_full_name(np));
	if (!clk_mapped_addr) {
		pr_err("Can't map clk registers\n");
		return;
	}

	pll_mapped_addr = of_io_request_and_map(np, 1, of_node_full_name(np));
	if (!pll_mapped_addr) {
		pr_err("Can't map pll registers\n");
		return;
	}

	rlx_clocks_init(np);
}

CLK_OF_DECLARE(rts493xa_clocks, "realtek,rts493xa-clocks", rlx_clk_init);
