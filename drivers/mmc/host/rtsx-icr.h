/* SPDX-License-Identifier: GPL-2.0-or-later */
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

#ifndef _RTSX_ICR_H_
#define _RTSX_ICR_H_

#include <linux/clk.h>
#include <linux/version.h>
#include <linux/gpio/consumer.h>

#define RTSX_ICR_DRV_NAME "sd-platform"

#define RTSX_ICR_CMD_TIMEOUT_MS 300
#define RTSX_ICR_DAT_TIMEOUT_MS 10000

#define RTSX_ICR_SSC_ENABLE 1

struct rtsx_icr_debugfs;
struct rtsx_icr;
void rtsx_icr_add_debugfs(struct rtsx_icr *icr);
static void rtsx_icr_remove_debugfs(struct rtsx_icr *icr);

struct rtsx_icr {
	struct device *dev;

	int irq;

	unsigned long addr;
	unsigned long addr_len;
	void __iomem *remap_addr;

	dma_addr_t resv_addr;
	void *resv_buf;

	u32 cmd_idx;
	dma_addr_t cmd_addr;
	void *cmd_ptr;

	u32 sg_tbl_idx;
	dma_addr_t sg_tbl_addr;
	void *sg_tbl_ptr;

	u32 bier;
	u32 trans_status;
	u32 trans_wait;
	u32 card_status;
	bool asic_mode;

	struct delayed_work detect_work;
	struct delayed_work idle_work;
	int data_errors;

	spinlock_t __lock;
	unsigned long __lock_flags;
	struct mutex mutex;

	struct completion done_cmd;
	struct completion *done;

	bool removed;
	bool short_dma_mode;

	const u32 *sd_pull_ctl_enable_tbl;
	const u32 *sd_pull_ctl_disable_tbl;

	u8 state;
#define RTSX_ICR_STATE_IDLE 0
#define RTSX_ICR_STATE_RUN  1

	struct mmc_host *mmc;
	struct mmc_request *mrq;

	u8 power_state;
#define RTSX_SD_POWER_OFF 0
#define RTSX_SD_POWER_ON  1
	bool initial_mode;
	unsigned int clock;
	unsigned int max_clock;
	unsigned int min_clock;
	u8 sd_mode;
#define RTSX_SD_20_MODE	 0
#define RTSX_SD_30_MODE	 1
#define RTSX_SD_DDR_MODE 2
	int need_tuning;
	bool double_clk;

	int cmd_timeout_ms;
	int data_timeout_ms;

	struct rtsx_icr_debugfs *debugfs;

	u8 trigger_gpio;
	u8 trigger_cmd;
	u8 trigger_count;
	u32 cd_level;
	struct gpio_desc *sd_pwr_ctrl_c2;
	struct gpio_desc *sd_votage_switch_ctrl_c2;
	struct gpio_desc *sd_pull_ctrl_c2;
	u32 caps;
	int dbg_flag;
	struct clk *sd_crc_ck;
	struct clk *sd_sample_ck;
	struct clk *sd_push_ck;
	struct {
		struct pinctrl *p;
		struct pinctrl_state *default_state;
		struct pinctrl_state *poweron_state;
		struct pinctrl_state *poweroff_state;
	} pins;
	int device_type;
};

#define icr_dev(icr) ((icr)->dev)
#if IS_ENABLED(CONFIG_MMC_REALTEK_RTS493X_DEBUG)
#define icr_dbg(icr, fmt, arg...) \
	dev_dbg(icr_dev(icr), "%s: " fmt, __func__, ##arg)
#define icr_err(icr, fmt, arg...) \
	dev_err(icr_dev(icr), "%s error: " fmt, __func__, ##arg)
#else /* DEBUG */
#define icr_dbg(icr, fmt, arg...)
#define icr_err(icr, fmt, arg...) \
	dev_dbg(icr_dev(icr), "%s error: " fmt, __func__, ##arg)
#endif /* DEBUG */

#define rtsx_icr_control_table(addr, val) (((u32)(addr) << 16) | (u8)(val))

#define rtsx_icr_readb(icr, reg) ioread8((icr)->remap_addr + reg)
#define rtsx_icr_readw(icr, reg) ioread16((icr)->remap_addr + reg)
#define rtsx_icr_readl(icr, reg) ioread32((icr)->remap_addr + reg)

#define rtsx_icr_writeb(icr, reg, value) \
	iowrite8(value, (icr)->remap_addr + reg)
#define rtsx_icr_writew(icr, reg, value) \
	iowrite16(value, (icr)->remap_addr + reg)
#define rtsx_icr_writel(icr, reg, value) \
	iowrite32(value, (icr)->remap_addr + reg)

static inline void rtsx_icr_lock(struct rtsx_icr *icr)
{
	spin_lock(&icr->__lock);
}
static inline void rtsx_icr_unlock(struct rtsx_icr *icr)
{
	spin_unlock(&icr->__lock);
}
static inline void rtsx_icr_lock_irqsave(struct rtsx_icr *icr)
{
	spin_lock_irqsave(&icr->__lock, icr->__lock_flags);
}
static inline void rtsx_icr_unlock_irqrestore(struct rtsx_icr *icr)
{
	spin_unlock_irqrestore(&icr->__lock, icr->__lock_flags);
}

static inline void rtsx_icr_init_cmd(struct rtsx_icr *icr)
	__acquires(&icr->__lock)
{
	rtsx_icr_lock_irqsave(icr);
	icr->cmd_idx = 0;
}
void __rtsx_icr_add_cmd(struct rtsx_icr *icr, u8 cmd_type, u16 reg_addr,
			u8 mask, u8 data);
static inline void rtsx_icr_read(struct rtsx_icr *icr, u16 reg_addr)
{
	__rtsx_icr_add_cmd(icr, 0, reg_addr, 0, 0);
}
static inline void rtsx_icr_write(struct rtsx_icr *icr, u16 reg_addr, u8 mask,
				  u8 data)
{
	__rtsx_icr_add_cmd(icr, 1, reg_addr, mask, data);
}
static inline void rtsx_icr_check(struct rtsx_icr *icr, u16 reg_addr, u8 mask,
				  u8 data)
{
	__rtsx_icr_add_cmd(icr, 2, reg_addr, mask, data);
}
static inline void rtsx_icr_write_be32(struct rtsx_icr *icr, u16 reg_addr,
				       u32 data)
{
	rtsx_icr_write(icr, reg_addr, 0xFF, data >> 24);
	rtsx_icr_write(icr, reg_addr + 1, 0xFF, data >> 16);
	rtsx_icr_write(icr, reg_addr + 2, 0xFF, data >> 8);
	rtsx_icr_write(icr, reg_addr + 3, 0xFF, data);
}

void rtsx_icr_send_cmd(struct rtsx_icr *icr) __releases(&icr->__lock);
int rtsx_icr_wait_cmd(struct rtsx_icr *icr, int timeout);
static inline u8 *rtsx_icr_get_data(struct rtsx_icr *icr)
{
	return icr->cmd_ptr;
}
static inline void rtsx_icr_stop_cmd(struct rtsx_icr *icr)
{
	rtsx_icr_writel(icr, HCBCTLR, CMD_STOP);
	rtsx_icr_writel(icr, HDBCTLR, DATA_STOP);
}

int rtsx_icr_reg_read(struct rtsx_icr *icr, u16 addr, u8 *data);
int rtsx_icr_reg_write(struct rtsx_icr *icr, u16 addr, u8 mask, u8 data);
int rtsx_icr_pp_read(struct rtsx_icr *icr, u8 *buf, int len);
int rtsx_icr_pp_write(struct rtsx_icr *icr, u8 *buf, int len);
static inline int rtsx_icr_transfer_cmd_timeout(struct rtsx_icr *icr,
						int timeout)
{
	rtsx_icr_send_cmd(icr);
	return rtsx_icr_wait_cmd(icr, timeout);
}
static inline int rtsx_icr_transfer_cmd(struct rtsx_icr *icr)
{
	return rtsx_icr_transfer_cmd_timeout(icr, icr->cmd_timeout_ms);
}
int rtsx_icr_transfer_data(struct rtsx_icr *icr, struct mmc_data *data);
int rtsx_icr_transfer_stop(struct rtsx_icr *icr);

int rtsx_icr_set_power_mode(struct rtsx_icr *icr, unsigned char power_mode);
int rtsx_icr_set_bus_width(struct rtsx_icr *icr, unsigned char bus_width);
int rtsx_icr_set_timing(struct rtsx_icr *icr, unsigned char timing);
int rtsx_icr_set_clock(struct rtsx_icr *icr, unsigned int clock);
static int rtsx_icr_init_hw(struct rtsx_icr *icr);

#if IS_ENABLED(CONFIG_PM)
static int rtsx_icr_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct rtsx_icr *icr = platform_get_drvdata(pdev);
	int err = 0;

	icr_dbg(icr, "\n");

	cancel_delayed_work(&icr->detect_work);
	cancel_delayed_work(&icr->idle_work);

	mutex_lock(&icr->mutex);

	rtsx_icr_writel(icr, BIER, 0);
	icr->bier = 0;

	mutex_unlock(&icr->mutex);
	return err;
}

static int rtsx_icr_resume(struct platform_device *pdev)
{
	struct rtsx_icr *icr = platform_get_drvdata(pdev);
	int err = 0;

	icr_dbg(icr, "\n");

	mutex_lock(&icr->mutex);

	err = rtsx_icr_init_hw(icr);
	if (err)
		goto out;

	schedule_delayed_work(&icr->idle_work, msecs_to_jiffies(200));

out:
	mutex_unlock(&icr->mutex);

	return err;
}

#else /* CONFIG_PM */

#define rtsx_icr_suspend NULL
#define rtsx_icr_resume	 NULL

#endif /* CONFIG_PM */

#endif /* _RTSX_ICR_H_ */
