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

/************************************************************************************
 *	This product is covered by one or more of the following patents:
 *	US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

/*
 * This driver is modified from r8169.c in Linux kernel 2.6.18
 */

/* In Linux 5.4 asm_inline was introduced, but it's not supported by clang.
 * Redefine it to just asm to enable successful compilation.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ip6_checksum.h>
//#include <net/core/dev.h>
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/completion.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
#include <linux/pci-aspm.h>
#endif
#endif
#include <linux/prefetch.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#define dev_printk(A, B, fmt, args...) printk(A fmt, ##args)
#else
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>
#endif

#include <linux/mdio.h>

#include <asm/io.h>
#include <asm/irq.h>

#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <asm/uaccess.h>
#include <linux/clk.h>
#include <linux/reset.h>
//#include <asm/fw/fw.h>

#include "r8168.h"
#include "r8168_asf.h"
#include "rtl_eeprom.h"
#include "rtltool.h"

#ifdef ENABLE_R8168_PROCFS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC. */
static const int multicast_filter_limit = 32;

#define _R(NAME, MAC, RCR, MASK, JumFrameSz) \
	{ .name = NAME,                      \
	  .mcfg = MAC,                       \
	  .RCR_Cfg = RCR,                    \
	  .RxConfigMask = MASK,              \
	  .jumbo_frame_sz = JumFrameSz }

static const struct {
	const char *name;
	u8 mcfg;
	u32 RCR_Cfg;
	u32 RxConfigMask; /* Clears the bits supported by this chip */
	u32 jumbo_frame_sz;
} rtl_chip_info[] = {
	_R("RTL8168B/8111B", CFG_METHOD_1,
	   (Reserved2_data << Reserved2_shift) |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_4k),

	_R("RTL8168B/8111B", CFG_METHOD_2,
	   (Reserved2_data << Reserved2_shift) |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_4k),

	_R("RTL8168B/8111B", CFG_METHOD_3,
	   (Reserved2_data << Reserved2_shift) |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_4k),

	_R("RTL8168C/8111C", CFG_METHOD_4,
	   RxCfg_128_int_en | RxCfg_fet_multi_en |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_6k),

	_R("RTL8168C/8111C", CFG_METHOD_5,
	   RxCfg_128_int_en | RxCfg_fet_multi_en |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_6k),

	_R("RTL8168C/8111C", CFG_METHOD_6,
	   RxCfg_128_int_en | RxCfg_fet_multi_en |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_6k),

	_R("RTL8168CP/8111CP", CFG_METHOD_7,
	   RxCfg_128_int_en | RxCfg_fet_multi_en |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_6k),

	_R("RTL8168CP/8111CP", CFG_METHOD_8,
	   RxCfg_128_int_en | RxCfg_fet_multi_en |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_6k),

	_R("RTL8168D/8111D", CFG_METHOD_9,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168D/8111D", CFG_METHOD_10,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168DP/8111DP", CFG_METHOD_11,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168DP/8111DP", CFG_METHOD_12,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168DP/8111DP", CFG_METHOD_13,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168E/8111E", CFG_METHOD_14,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168E/8111E", CFG_METHOD_15,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168E-VL/8111E-VL", CFG_METHOD_16,
	   RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e0080, Jumbo_Frame_9k),

	_R("RTL8168E-VL/8111E-VL", CFG_METHOD_17,
	   RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_9k),

	_R("RTL8168F/8111F", CFG_METHOD_18,
	   RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_9k),

	_R("RTL8168F/8111F", CFG_METHOD_19,
	   RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e1880, Jumbo_Frame_9k),

	_R("RTL8411", CFG_METHOD_20,
	   RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift), 0xff7e1880,
	   Jumbo_Frame_9k),

	_R("RTL8168G/8111G", CFG_METHOD_21,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168G/8111G", CFG_METHOD_22,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168EP/8111EP", CFG_METHOD_23,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168GU/8111GU", CFG_METHOD_24,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168GU/8111GU", CFG_METHOD_25,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("8411B", CFG_METHOD_26,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168EP/8111EP", CFG_METHOD_27,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168EP/8111EP", CFG_METHOD_28,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168H/8111H", CFG_METHOD_29,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168H/8111H", CFG_METHOD_30,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168FP/8111FP", CFG_METHOD_31,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168FP/8111FP", CFG_METHOD_32,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("RTL8168FP/8111FP", CFG_METHOD_33,
	   RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 |
		   (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_9k),

	_R("Unknown", CFG_METHOD_DEFAULT, (RX_DMA_BURST << RxCfgDMAShift),
	   0xff7e5880, Jumbo_Frame_1k)
};
#undef _R

MODULE_DEVICE_TABLE(pci, rtl8168_pci_tbl);

static int rx_copybreak = 0;
static int use_dac = 1;
static int timer_count = 0x2600;

static struct {
	u32 msg_enable;
} debug = { -1 };

static unsigned int speed_mode = SPEED_100;
static unsigned int duplex_mode = DUPLEX_FULL;
static unsigned int autoneg_mode = AUTONEG_ENABLE;
static unsigned int advertising_mode =
	ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
	ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
	ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full;
#ifdef CONFIG_ASPM
static int aspm = 1;
#else
static int aspm = 0;
#endif
#ifdef ENABLE_S5WOL
static int s5wol = 1;
#else
static int s5wol = 0;
#endif
#ifdef ENABLE_S5_KEEP_CURR_MAC
static int s5_keep_curr_mac = 1;
#else
static int s5_keep_curr_mac = 0;
#endif
#ifdef ENABLE_EEE
static int eee_enable = 1;
#else
static int eee_enable = 0;
#endif
#ifdef CONFIG_SOC_LAN
static ulong hwoptimize = HW_PATCH_SOC_LAN;
#else
static ulong hwoptimize = 0;
#endif
#ifdef ENABLE_S0_MAGIC_PACKET
static int s0_magic_packet = 1;
#else
static int s0_magic_packet = 0;
#endif

MODULE_AUTHOR("Realtek and the Linux r8168 crew <netdev@vger.kernel.org>");
MODULE_DESCRIPTION("RealTek RTL-8168 Gigabit Ethernet driver");

module_param(speed_mode, uint, 0);
MODULE_PARM_DESC(speed_mode, "force phy operation. Deprecated by ethtool (8).");

module_param(duplex_mode, uint, 0);
MODULE_PARM_DESC(duplex_mode,
		 "force phy operation. Deprecated by ethtool (8).");

module_param(autoneg_mode, uint, 0);
MODULE_PARM_DESC(autoneg_mode,
		 "force phy operation. Deprecated by ethtool (8).");

module_param(advertising_mode, uint, 0);
MODULE_PARM_DESC(advertising_mode,
		 "force phy operation. Deprecated by ethtool (8).");

module_param(aspm, int, 0);
MODULE_PARM_DESC(aspm, "Enable ASPM.");

module_param(s5wol, int, 0);
MODULE_PARM_DESC(s5wol, "Enable Shutdown Wake On Lan.");

module_param(s5_keep_curr_mac, int, 0);
MODULE_PARM_DESC(s5_keep_curr_mac, "Enable Shutdown Keep Current MAC Address.");

module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");

module_param(use_dac, int, 0);
MODULE_PARM_DESC(use_dac, "Enable PCI DAC. Unsafe on 32 bit PCI slot.");

module_param(timer_count, int, 0);
MODULE_PARM_DESC(timer_count, "Timer Interrupt Interval.");

module_param(eee_enable, int, 0);
MODULE_PARM_DESC(eee_enable, "Enable Energy Efficient Ethernet.");

module_param(hwoptimize, ulong, 0);
MODULE_PARM_DESC(hwoptimize, "Enable HW optimization function.");

module_param(s0_magic_packet, int, 0);
MODULE_PARM_DESC(s0_magic_packet, "Enable S0 Magic Packet.");

module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");

MODULE_LICENSE("GPL");

MODULE_VERSION(RTL8168_VERSION);

static void rtl8168_sleep_rx_enable(struct net_device *dev);
static void rtl8168_dsm(struct net_device *dev, int dev_state);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static void rtl8168_link_timer(unsigned long __opaque);
#else
static void rtl8168_link_timer(struct timer_list *t);
#endif
static void rtl8168_tx_clear(struct rtl8168_private *tp);
static void rtl8168_rx_clear(struct rtl8168_private *tp);

static int rtl8168_open(struct net_device *dev);
static int rtl8168_start_xmit(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t rtl8168_interrupt(int irq, void *dev_instance);
static void rtl8168_rx_desc_offset0_init(struct rtl8168_private *, int);
static int rtl8168_init_ring(struct net_device *dev);
static void rtl8168_hw_config(struct net_device *dev);
static void rtl8168_hw_start(struct net_device *dev);
static int rtl8168_close(struct net_device *dev);
static void rtl8168_set_rx_mode(struct net_device *dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static void rtl8168_tx_timeout(struct net_device *dev, unsigned int txqueue);
#else
static void rtl8168_tx_timeout(struct net_device *dev);
#endif
static struct net_device_stats *rtl8168_get_stats(struct net_device *dev);
static int rtl8168_rx_interrupt(struct net_device *, struct rtl8168_private *,
				napi_budget);
static int rtl8168_change_mtu(struct net_device *dev, int new_mtu);
static void rtl8168_down(struct net_device *dev);

static int rtl8168_set_mac_address(struct net_device *dev, void *p);
void rtl8168_rar_set(struct rtl8168_private *tp, uint8_t *addr);
static void rtl8168_desc_addr_fill(struct rtl8168_private *);
static void rtl8168_tx_desc_init(struct rtl8168_private *tp);
static void rtl8168_rx_desc_init(struct rtl8168_private *tp);

static void rtl8168_hw_reset(struct net_device *dev);

static void rtl8168_phy_power_up(struct net_device *dev);
static void rtl8168_phy_power_down(struct net_device *dev);
static int rtl8168_set_speed(struct net_device *dev, u8 autoneg, u32 speed,
			     u8 duplex, u32 adv);

static int rtl8168_set_phy_mcu_patch_request(struct rtl8168_private *tp);
static int rtl8168_clear_phy_mcu_patch_request(struct rtl8168_private *tp);

#ifdef CONFIG_R8168_NAPI
static int rtl8168_poll(napi_ptr napi, napi_budget budget);
#endif

#ifndef SET_ETHTOOL_OPS
#define SET_ETHTOOL_OPS(netdev, ops) ((netdev)->ethtool_ops = (ops))
#endif //SET_ETHTOOL_OPS

//#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)
#ifndef netif_msg_init
#define netif_msg_init _kc_netif_msg_init
/* copied from linux kernel 2.6.20 include/linux/netdevice.h */
static inline u32 netif_msg_init(int debug_value, int default_msg_enable_bits)
{
	/* use default */
	if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
		return default_msg_enable_bits;
	if (debug_value == 0) /* no output */
		return 0;
	/* set low N bits */
	return (1 << debug_value) - 1;
}

#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)

static inline void eth_copy_and_sum(struct sk_buff *dest,
				    const unsigned char *src, int len, int base)
{
	memcpy(dest->data, src, len);
}

struct rtl8168_counters {
	u64 tx_packets;
	u64 rx_packets;
	u64 tx_errors;
	u32 rx_errors;
	u16 rx_missed;
	u16 align_errors;
	u32 tx_one_collision;
	u32 tx_multi_collision;
	u64 rx_unicast;
	u64 rx_broadcast;
	u32 rx_multicast;
	u16 tx_aborted;
	u16 tx_underun;
};

#ifdef ENABLE_R8168_PROCFS
/****************************************************************************
*	-----------------------------PROCFS STUFF-------------------------
*****************************************************************************
*/

static struct proc_dir_entry *rtl8168_proc;
static int proc_init_num = 0;

static int proc_get_driver_variable(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	seq_puts(m, "\nDump Driver Variable\n");

	spin_lock_irqsave(&tp->lock, flags);
	seq_puts(m, "Variable\tValue\n----------\t-----\n");
	seq_printf(m, "MODULENAME\t%s\n", MODULENAME);
	seq_printf(m, "driver version\t%s\n", RTL8168_VERSION);
	seq_printf(m, "chipset\t%d\n", tp->chipset);
	seq_printf(m, "chipset_name\t%s\n", rtl_chip_info[tp->chipset].name);
	seq_printf(m, "mtu\t%d\n", dev->mtu);
	seq_printf(m, "NUM_RX_DESC\t0x%x\n", NUM_RX_DESC);
	seq_printf(m, "cur_rx\t0x%x\n", tp->cur_rx);
	seq_printf(m, "dirty_rx\t0x%x\n", tp->dirty_rx);
	seq_printf(m, "NUM_TX_DESC\t0x%x\n", NUM_TX_DESC);
	seq_printf(m, "cur_tx\t0x%x\n", tp->cur_tx);
	seq_printf(m, "dirty_tx\t0x%x\n", tp->dirty_tx);
	seq_printf(m, "rx_buf_sz\t0x%x\n", tp->rx_buf_sz);
	seq_printf(m, "esd_flag\t0x%x\n", tp->esd_flag);
	seq_printf(m, "pci_cfg_is_read\t0x%x\n", tp->pci_cfg_is_read);
	seq_printf(m, "rtl8168_rx_config\t0x%x\n", tp->rtl8168_rx_config);
	seq_printf(m, "cp_cmd\t0x%x\n", tp->cp_cmd);
	seq_printf(m, "intr_mask\t0x%x\n", tp->intr_mask);
	seq_printf(m, "timer_intr_mask\t0x%x\n", tp->timer_intr_mask);
	seq_printf(m, "wol_enabled\t0x%x\n", tp->wol_enabled);
	seq_printf(m, "wol_opts\t0x%x\n", tp->wol_opts);
	seq_printf(m, "efuse_ver\t0x%x\n", tp->efuse_ver);
	seq_printf(m, "eeprom_type\t0x%x\n", tp->eeprom_type);
	seq_printf(m, "autoneg\t0x%x\n", tp->autoneg);
	seq_printf(m, "duplex\t0x%x\n", tp->duplex);
	seq_printf(m, "speed\t%d\n", tp->speed);
	seq_printf(m, "advertising\t0x%x\n", tp->advertising);
	seq_printf(m, "eeprom_len\t0x%x\n", tp->eeprom_len);
	seq_printf(m, "cur_page\t0x%x\n", tp->cur_page);
	seq_printf(m, "bios_setting\t0x%x\n", tp->bios_setting);
	seq_printf(m, "features\t0x%x\n", tp->features);
	seq_printf(m, "org_pci_offset_99\t0x%x\n", tp->org_pci_offset_99);
	seq_printf(m, "org_pci_offset_180\t0x%x\n", tp->org_pci_offset_180);
	seq_printf(m, "issue_offset_99_event\t0x%x\n",
		   tp->issue_offset_99_event);
	seq_printf(m, "org_pci_offset_80\t0x%x\n", tp->org_pci_offset_80);
	seq_printf(m, "org_pci_offset_81\t0x%x\n", tp->org_pci_offset_81);
	seq_printf(m, "use_timer_interrrupt\t0x%x\n", tp->use_timer_interrrupt);
	seq_printf(m, "HwIcVerUnknown\t0x%x\n", tp->HwIcVerUnknown);
	seq_printf(m, "NotWrRamCodeToMicroP\t0x%x\n", tp->NotWrRamCodeToMicroP);
	seq_printf(m, "NotWrMcuPatchCode\t0x%x\n", tp->NotWrMcuPatchCode);
	seq_printf(m, "HwHasWrRamCodeToMicroP\t0x%x\n",
		   tp->HwHasWrRamCodeToMicroP);
	seq_printf(m, "sw_ram_code_ver\t0x%x\n", tp->sw_ram_code_ver);
	seq_printf(m, "hw_ram_code_ver\t0x%x\n", tp->hw_ram_code_ver);
	seq_printf(m, "rtk_enable_diag\t0x%x\n", tp->rtk_enable_diag);
	seq_printf(m, "ShortPacketSwChecksum\t0x%x\n",
		   tp->ShortPacketSwChecksum);
	seq_printf(m, "UseSwPaddingShortPkt\t0x%x\n", tp->UseSwPaddingShortPkt);
	seq_printf(m, "RequireAdcBiasPatch\t0x%x\n", tp->RequireAdcBiasPatch);
	seq_printf(m, "AdcBiasPatchIoffset\t0x%x\n", tp->AdcBiasPatchIoffset);
	seq_printf(m, "RequireAdjustUpsTxLinkPulseTiming\t0x%x\n",
		   tp->RequireAdjustUpsTxLinkPulseTiming);
	seq_printf(m, "SwrCnt1msIni\t0x%x\n", tp->SwrCnt1msIni);
	seq_printf(m, "HwSuppNowIsOobVer\t0x%x\n", tp->HwSuppNowIsOobVer);
	seq_printf(m, "HwFiberModeVer\t0x%x\n", tp->HwFiberModeVer);
	seq_printf(m, "HwFiberStat\t0x%x\n", tp->HwFiberStat);
	seq_printf(m, "HwSwitchMdiToFiber\t0x%x\n", tp->HwSwitchMdiToFiber);
	seq_printf(m, "HwSuppSerDesPhyVer\t0x%x\n", tp->HwSuppSerDesPhyVer);
	seq_printf(m, "NicCustLedValue\t0x%x\n", tp->NicCustLedValue);
	seq_printf(m, "RequiredSecLanDonglePatch\t0x%x\n",
		   tp->RequiredSecLanDonglePatch);
	seq_printf(m, "HwSuppDashVer\t0x%x\n", tp->HwSuppDashVer);
	seq_printf(m, "DASH\t0x%x\n", tp->DASH);
	seq_printf(m, "dash_printer_enabled\t0x%x\n", tp->dash_printer_enabled);
	seq_printf(m, "HwSuppKCPOffloadVer\t0x%x\n", tp->HwSuppKCPOffloadVer);
	seq_printf(m, "speed_mode\t0x%x\n", speed_mode);
	seq_printf(m, "duplex_mode\t0x%x\n", duplex_mode);
	seq_printf(m, "autoneg_mode\t0x%x\n", autoneg_mode);
	seq_printf(m, "advertising_mode\t0x%x\n", advertising_mode);
	seq_printf(m, "aspm\t0x%x\n", aspm);
	seq_printf(m, "s5wol\t0x%x\n", s5wol);
	seq_printf(m, "s5_keep_curr_mac\t0x%x\n", s5_keep_curr_mac);
	seq_printf(m, "eee_enable\t0x%x\n", tp->eee_enabled);
	seq_printf(m, "hwoptimize\t0x%lx\n", hwoptimize);
	seq_printf(m, "proc_init_num\t0x%x\n", proc_init_num);
	seq_printf(m, "s0_magic_packet\t0x%x\n", s0_magic_packet);
	seq_printf(m, "HwSuppMagicPktVer\t0x%x\n", tp->HwSuppMagicPktVer);
	seq_printf(m, "HwSuppCheckPhyDisableModeVer\t0x%x\n",
		   tp->HwSuppCheckPhyDisableModeVer);
	seq_printf(m, "HwPkgDet\t0x%x\n", tp->HwPkgDet);
	seq_printf(m, "HwSuppGigaForceMode\t0x%x\n", tp->HwSuppGigaForceMode);
	seq_printf(m, "random_mac\t0x%x\n", tp->random_mac);
	seq_printf(m, "org_mac_addr\t%pM\n", tp->org_mac_addr);
	seq_printf(m, "perm_addr\t%pM\n", dev->perm_addr);
	seq_printf(m, "dev_addr\t%pM\n", dev->dev_addr);
	spin_unlock_irqrestore(&tp->lock, flags);

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_tally_counter(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	struct rtl8168_private *tp = netdev_priv(dev);
	struct rtl8168_counters *counters;
	dma_addr_t paddr;
	u32 cmd;
	u32 WaitCnt;
	unsigned long flags;

	seq_puts(m, "\nDump Tally Counter\n");

	counters = tp->tally_vaddr;
	paddr = tp->tally_paddr;
	if (!counters) {
		seq_puts(m, "\nDump Tally Counter Fail\n");
		return 0;
	}

	spin_lock_irqsave(&tp->lock, flags);
	RTL_W32(tp, CounterAddrHigh, (u64)paddr >> 32);
	cmd = (u64)paddr & DMA_BIT_MASK(32);
	RTL_W32(tp, CounterAddrLow, cmd);
	RTL_W32(tp, CounterAddrLow, cmd | CounterDump);

	WaitCnt = 0;
	while (RTL_R32(tp, CounterAddrLow) & CounterDump) {
		udelay(10);

		WaitCnt++;
		if (WaitCnt > 20)
			break;
	}
	spin_unlock_irqrestore(&tp->lock, flags);

	seq_puts(m, "Statistics\tValue\n----------\t-----\n");
	seq_printf(m, "tx_packets\t%lld\n", le64_to_cpu(counters->tx_packets));
	seq_printf(m, "rx_packets\t%lld\n", le64_to_cpu(counters->rx_packets));
	seq_printf(m, "tx_errors\t%lld\n", le64_to_cpu(counters->tx_errors));
	seq_printf(m, "rx_missed\t%lld\n", le64_to_cpu(counters->rx_missed));
	seq_printf(m, "align_errors\t%lld\n",
		   le64_to_cpu(counters->align_errors));
	seq_printf(m, "tx_one_collision\t%lld\n",
		   le64_to_cpu(counters->tx_one_collision));
	seq_printf(m, "tx_multi_collision\t%lld\n",
		   le64_to_cpu(counters->tx_multi_collision));
	seq_printf(m, "rx_unicast\t%lld\n", le64_to_cpu(counters->rx_unicast));
	seq_printf(m, "rx_broadcast\t%lld\n",
		   le64_to_cpu(counters->rx_broadcast));
	seq_printf(m, "rx_multicast\t%lld\n",
		   le64_to_cpu(counters->rx_multicast));
	seq_printf(m, "tx_aborted\t%lld\n", le64_to_cpu(counters->tx_aborted));
	seq_printf(m, "tx_underun\t%lld\n", le64_to_cpu(counters->tx_underun));

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_registers(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	int i, n, max = R8168_MAC_REGS_SIZE;
	u8 byte_rd;
	struct rtl8168_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;

	seq_puts(m, "\nDump MAC Registers\n");
	seq_puts(m, "Offset\tValue\n------\t-----\n");

	spin_lock_irqsave(&tp->lock, flags);
	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 16 && n < max; i++, n++) {
			byte_rd = readb(ioaddr + n);
			seq_printf(m, "%02x ", byte_rd);
		}
	}
	spin_unlock_irqrestore(&tp->lock, flags);

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_pcie_phy(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	int i, n, max = R8168_EPHY_REGS_SIZE / 2;
	u16 word_rd;
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	seq_puts(m, "\nDump PCIE PHY\n");
	seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

	spin_lock_irqsave(&tp->lock, flags);
	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 8 && n < max; i++, n++) {
			word_rd = rtl8168_ephy_read(tp, n);
			seq_printf(m, "%04x ", word_rd);
		}
	}
	spin_unlock_irqrestore(&tp->lock, flags);

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_eth_phy(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	int i, n, max = R8168_PHY_REGS_SIZE / 2;
	u16 word_rd;
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	seq_puts(m, "\nDump Ethernet PHY\n");
	seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

	spin_lock_irqsave(&tp->lock, flags);
	seq_puts(m, "\n####################page 0##################\n ");
	rtl8168_mdio_write(tp, 0x1f, 0x0000);
	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 8 && n < max; i++, n++) {
			word_rd = rtl8168_mdio_read(tp, n);
			seq_printf(m, "%04x ", word_rd);
		}
	}
	spin_unlock_irqrestore(&tp->lock, flags);

	seq_putc(m, '\n');
	return 0;
}

static int proc_get_extended_registers(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	int i, n, max = R8168_ERI_REGS_SIZE;
	u32 dword_rd;
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	switch (tp->mcfg) {
	case CFG_METHOD_1:
	case CFG_METHOD_2:
	case CFG_METHOD_3:
		/* RTL8168B does not support Extend GMAC */
		seq_puts(m, "\nNot Support Dump Extended Registers\n");
		return 0;
	}

	seq_puts(m, "\nDump Extended Registers\n");
	seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

	spin_lock_irqsave(&tp->lock, flags);
	for (n = 0; n < max;) {
		seq_printf(m, "\n0x%02x:\t", n);

		for (i = 0; i < 4 && n < max; i++, n += 4) {
			dword_rd = rtl8168_eri_read(tp, n, 4, ERIAR_ExGMAC);
			seq_printf(m, "%08x ", dword_rd);
		}
	}
	spin_unlock_irqrestore(&tp->lock, flags);

	seq_putc(m, '\n');
	return 0;
}

static void rtl8168_proc_module_init(void)
{
	//create /proc/net/r8168
	rtl8168_proc = proc_mkdir(MODULENAME, init_net.proc_net);
	if (!rtl8168_proc)
		dprintk("cannot create %s proc entry \n", MODULENAME);
}

/*
 * seq_file wrappers for procfile show routines.
 */
static int rtl8168_proc_open(struct inode *inode, struct file *file)
{
	struct net_device *dev = proc_get_parent_data(inode);
	//	int (*show)(struct seq_file *, void *) = PDE_DATA(inode);
	int (*show)(struct seq_file *, void *) = pde_data(inode);

	return single_open(file, show, dev);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops rtl8168_proc_fops = {
	.proc_open = rtl8168_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations rtl8168_proc_fops = {
	.open = rtl8168_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

/*
 * Table of proc files we need to create.
 */
struct rtl8168_proc_file {
	char name[12];
	int (*show)(struct seq_file *, void *);
};

static const struct rtl8168_proc_file rtl8168_proc_files[] = {
	{ "driver_var", &proc_get_driver_variable },
	{ "tally", &proc_get_tally_counter },
	{ "registers", &proc_get_registers },
	{ "pcie_phy", &proc_get_pcie_phy },
	{ "eth_phy", &proc_get_eth_phy },
	{ "ext_regs", &proc_get_extended_registers },
	{ "" }
};

static void rtl8168_proc_init(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	const struct rtl8168_proc_file *f;
	struct proc_dir_entry *dir;

	if (rtl8168_proc && !tp->proc_dir) {
		dir = proc_mkdir_data(dev->name, 0, rtl8168_proc, dev);
		if (!dir) {
			printk("Unable to initialize /proc/net/%s/%s\n",
			       MODULENAME, dev->name);
			return;
		}

		tp->proc_dir = dir;
		proc_init_num++;

		for (f = rtl8168_proc_files; f->name[0]; f++) {
			if (!proc_create_data(f->name, S_IFREG | S_IRUGO, dir,
					      &rtl8168_proc_fops, f->show)) {
				printk("Unable to initialize "
				       "/proc/net/%s/%s/%s\n",
				       MODULENAME, dev->name, f->name);
				return;
			}
		}
	}
}

static void rtl8168_proc_remove(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	if (tp->proc_dir) {
		remove_proc_subtree(dev->name, rtl8168_proc);
		proc_init_num--;

		tp->proc_dir = NULL;
	}
}

#endif //ENABLE_R8168_PROCFS

static inline u16 map_phy_ocp_addr(u16 PageNum, u8 RegNum)
{
	u16 OcpPageNum = 0;
	u8 OcpRegNum = 0;
	u16 OcpPhyAddress = 0;

	if (PageNum == 0) {
		OcpPageNum = OCP_STD_PHY_BASE_PAGE + (RegNum / 8);
		OcpRegNum = 0x10 + (RegNum % 8);
	} else {
		OcpPageNum = PageNum;
		OcpRegNum = RegNum;
	}

	OcpPageNum <<= 4;

	if (OcpRegNum < 16) {
		OcpPhyAddress = 0;
	} else {
		OcpRegNum -= 16;
		OcpRegNum <<= 1;
		OcpPhyAddress = OcpPageNum + OcpRegNum;
	}

	return OcpPhyAddress;
}

static void mdio_real_direct_write_phy_ocp(struct rtl8168_private *tp,
					   u32 RegAddr, u32 value)
{
	u32 data32;
	int i;

	if (tp->HwSuppPhyOcpVer == 0)
		goto out;
	WARN_ON_ONCE(RegAddr % 2);
	data32 = RegAddr / 2;
	data32 <<= OCPR_Addr_Reg_shift;
	data32 |= OCPR_Write | value;

	RTL_W32(tp, PHYOCP, data32);
	for (i = 0; i < 100; i++) {
		udelay(1);

		if (!(RTL_R32(tp, PHYOCP) & OCPR_Flag))
			break;
	}
out:
	return;
}

static void mdio_real_write(struct rtl8168_private *tp, u32 RegAddr, u32 value)
{
	int i;

	if (RegAddr == 0x1F)
		tp->cur_page = value;

	if (tp->mcfg == CFG_METHOD_21) {
		u32 data32;
		u16 ocp_addr;

		if (RegAddr == 0x1F)
			return;

		ocp_addr = map_phy_ocp_addr(tp->cur_page, RegAddr);

		WARN_ON_ONCE(ocp_addr % 2);
		data32 = ocp_addr / 2;
		data32 <<= OCPR_Addr_Reg_shift;
		data32 |= OCPR_Write | value;

		RTL_W32(tp, PHYOCP, data32);
		for (i = 0; i < 100; i++) {
			udelay(1);

			if (!(RTL_R32(tp, PHYOCP) & OCPR_Flag))
				break;
		}
	} else {
		RTL_W32(tp, PHYAR,
			PHYAR_Write |
				(RegAddr & PHYAR_Reg_Mask) << PHYAR_Reg_shift |
				(value & PHYAR_Data_Mask));

		for (i = 0; i < 10; i++) {
			udelay(100);

			/* Check if the RTL8168 has completed writing to the specified MII register */
			if (!(RTL_R32(tp, PHYAR) & PHYAR_Flag)) {
				udelay(20);
				break;
			}
		}
	}
}

void rtl8168_mdio_write(struct rtl8168_private *tp, u32 RegAddr, u32 value)
{
	if (tp->rtk_enable_diag)
		return;

	mdio_real_write(tp, RegAddr, value);
}

void rtl8168_mdio_prot_write(struct rtl8168_private *tp, u32 RegAddr, u32 value)
{
	mdio_real_write(tp, RegAddr, value);
}

void rtl8168_mdio_prot_direct_write_phy_ocp(struct rtl8168_private *tp,
					    u32 RegAddr, u32 value)
{
	mdio_real_direct_write_phy_ocp(tp, RegAddr, value);
}

static u32 mdio_real_direct_read_phy_ocp(struct rtl8168_private *tp,
					 u32 RegAddr)
{
	u32 data32;
	int i, value = 0;

	if (tp->HwSuppPhyOcpVer == 0)
		goto out;

	WARN_ON_ONCE(RegAddr % 2);
	data32 = RegAddr / 2;
	data32 <<= OCPR_Addr_Reg_shift;

	RTL_W32(tp, PHYOCP, data32);
	for (i = 0; i < 100; i++) {
		udelay(1);

		if (RTL_R32(tp, PHYOCP) & OCPR_Flag)
			break;
	}
	value = RTL_R32(tp, PHYOCP) & OCPDR_Data_Mask;

out:
	return value;
}

static u32 rtl8168_mdio_real_read_phy_ocp(struct rtl8168_private *tp,
					  u16 PageNum, u32 RegAddr)
{
	u16 ocp_addr;

	ocp_addr = map_phy_ocp_addr(PageNum, RegAddr);

	return mdio_real_direct_read_phy_ocp(tp, ocp_addr);
}

u32 mdio_real_read(struct rtl8168_private *tp, u32 RegAddr)
{
	int i, value = 0;

	if (tp->mcfg == CFG_METHOD_11) {
		RTL_W32(tp, OCPDR,
			OCPDR_Read | (RegAddr & OCPDR_Reg_Mask)
					     << OCPDR_GPHY_Reg_shift);
		RTL_W32(tp, OCPAR, OCPAR_GPHY_Write);
		RTL_W32(tp, EPHY_RXER_NUM, 0);

		for (i = 0; i < 100; i++) {
			mdelay(1);
			if (!(RTL_R32(tp, OCPAR) & OCPAR_Flag))
				break;
		}

		mdelay(1);
		RTL_W32(tp, OCPAR, OCPAR_GPHY_Read);
		RTL_W32(tp, EPHY_RXER_NUM, 0);

		for (i = 0; i < 100; i++) {
			mdelay(1);
			if (RTL_R32(tp, OCPAR) & OCPAR_Flag)
				break;
		}

		value = RTL_R32(tp, OCPDR) & OCPDR_Data_Mask;
	} else {
		if (tp->HwSuppPhyOcpVer > 0) {
			value = rtl8168_mdio_real_read_phy_ocp(tp, tp->cur_page,
							       RegAddr);
		} else {
			if (tp->mcfg == CFG_METHOD_12 ||
			    tp->mcfg == CFG_METHOD_13)
				RTL_W32(tp, 0xD0,
					RTL_R32(tp, 0xD0) & ~0x00020000);

			RTL_W32(tp, PHYAR,
				PHYAR_Read | (RegAddr & PHYAR_Reg_Mask)
						     << PHYAR_Reg_shift);

			for (i = 0; i < 10; i++) {
				udelay(100);

				/* Check if the RTL8168 has completed retrieving data from the specified MII register */
				if (RTL_R32(tp, PHYAR) & PHYAR_Flag) {
					value = RTL_R32(tp, PHYAR) &
						PHYAR_Data_Mask;
					udelay(20);
					break;
				}
			}

			if (tp->mcfg == CFG_METHOD_12 ||
			    tp->mcfg == CFG_METHOD_13)
				RTL_W32(tp, 0xD0,
					RTL_R32(tp, 0xD0) | 0x00020000);
		}
	}

	return value;
}

u32 rtl8168_mdio_read(struct rtl8168_private *tp, u32 RegAddr)
{
	int i, value = 0;

	if (tp->mcfg == CFG_METHOD_21) {
		u32 data32;
		u16 ocp_addr;

		ocp_addr = map_phy_ocp_addr(tp->cur_page, RegAddr);

		WARN_ON_ONCE(ocp_addr % 2);

		data32 = ocp_addr / 2;
		data32 <<= OCPR_Addr_Reg_shift;

		RTL_W32(tp, PHYOCP, data32);
		for (i = 0; i < 100; i++) {
			udelay(1);
			if (RTL_R32(tp, PHYOCP) & OCPR_Flag)
				break;
		}
		value = RTL_R32(tp, PHYOCP) & OCPDR_Data_Mask;
	} else {
		RTL_W32(tp, PHYAR,
			PHYAR_Read | (RegAddr & PHYAR_Reg_Mask)
					     << PHYAR_Reg_shift);

		for (i = 0; i < 10; i++) {
			udelay(100);
			/* Check if the RTL8168 has completed retrieving data from the specified MII register */
			if (RTL_R32(tp, PHYAR) & PHYAR_Flag) {
				value = RTL_R32(tp, PHYAR) & PHYAR_Data_Mask;
				udelay(20);
				break;
			}
		}
	}

	return value;
}

u32 rtl8168_mdio_prot_read(struct rtl8168_private *tp, u32 RegAddr)
{
	return mdio_real_read(tp, RegAddr);
}

u32 rtl8168_mdio_prot_direct_read_phy_ocp(struct rtl8168_private *tp,
					  u32 RegAddr)
{
	return mdio_real_direct_read_phy_ocp(tp, RegAddr);
}

static void ClearAndSetEthPhyBit(struct rtl8168_private *tp, u8 addr,
				 u16 clearmask, u16 setmask)
{
	u16 PhyRegValue;

	PhyRegValue = rtl8168_mdio_read(tp, addr);
	PhyRegValue &= ~clearmask;
	PhyRegValue |= setmask;
	rtl8168_mdio_write(tp, addr, PhyRegValue);
}

void rtl8168_clear_eth_phy_bit(struct rtl8168_private *tp, u8 addr, u16 mask)
{
	ClearAndSetEthPhyBit(tp, addr, mask, 0);
}

void rtl8168_set_eth_phy_bit(struct rtl8168_private *tp, u8 addr, u16 mask)
{
	ClearAndSetEthPhyBit(tp, addr, 0, mask);
}

void rtl8168_mac_ocp_write(struct rtl8168_private *tp, u16 reg_addr, u16 value)
{
	u32 data32;

	WARN_ON_ONCE(reg_addr % 2);

	data32 = reg_addr / 2;
	data32 <<= OCPR_Addr_Reg_shift;
	data32 += value;
	data32 |= OCPR_Write;

	RTL_W32(tp, MACOCP, data32);
}

u16 rtl8168_mac_ocp_read(struct rtl8168_private *tp, u16 reg_addr)
{
	u32 data32;
	u16 data16 = 0;

	WARN_ON_ONCE(reg_addr % 2);

	data32 = reg_addr / 2;
	data32 <<= OCPR_Addr_Reg_shift;

	RTL_W32(tp, MACOCP, data32);
	data16 = (u16)RTL_R32(tp, MACOCP);

	return data16;
}

static void rtl8168_clear_and_set_mcu_ocp_bit(struct rtl8168_private *tp,
					      u16 addr, u16 clearmask,
					      u16 setmask)
{
	u16 RegValue;

	RegValue = rtl8168_mac_ocp_read(tp, addr);
	RegValue &= ~clearmask;
	RegValue |= setmask;
	rtl8168_mac_ocp_write(tp, addr, RegValue);
}

static void rtl8168_set_mcu_ocp_bit(struct rtl8168_private *tp, u16 addr,
				    u16 mask)
{
	rtl8168_clear_and_set_mcu_ocp_bit(tp, addr, 0, mask);
}

static u32 real_ocp_read(struct rtl8168_private *tp, u16 addr, u8 len)
{
	int i, val_shift, shift = 0;
	u32 value1 = 0, value2 = 0, mask;

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % 4;
		addr = addr & ~0x3;

		RTL_W32(tp, OCPAR, (0x0F << 12) | (addr & 0xFFF));

		for (i = 0; i < 20; i++) {
			udelay(100);
			if (RTL_R32(tp, OCPAR) & OCPAR_Flag)
				break;
		}

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = RTL_R32(tp, OCPDR) & mask;
		value2 |= (value1 >> val_shift * 8) << shift * 8;

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	udelay(20);

	return value2;
}

u32 rtl8168_ocp_read_with_oob_base_address(struct rtl8168_private *tp, u16 addr,
					   u8 len, const u32 base_address)
{
	return rtl8168_eri_read_with_oob_base_address(tp, addr, len, ERIAR_OOB,
						      base_address);
}

u32 rtl8168_ocp_read(struct rtl8168_private *tp, u16 addr, u8 len)
{
	u32 value = 0;

	if (HW_DASH_SUPPORT_TYPE_2(tp))
		value = rtl8168_ocp_read_with_oob_base_address(tp, addr, len,
							       NO_BASE_ADDRESS);
	else if (HW_DASH_SUPPORT_TYPE_3(tp))
		value = rtl8168_ocp_read_with_oob_base_address(
			tp, addr, len, RTL8168FP_OOBMAC_BASE);
	else
		value = real_ocp_read(tp, addr, len);

	return value;
}

static int real_ocp_write(struct rtl8168_private *tp, u16 addr, u8 len,
			  u32 value)
{
	int i, val_shift, shift = 0;
	u32 value1 = 0, mask;

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % 4;
		addr = addr & ~0x3;

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = rtl8168_ocp_read(tp, addr, 4) & ~mask;
		value1 |= ((value << val_shift * 8) >> shift * 8);

		RTL_W32(tp, OCPDR, value1);
		RTL_W32(tp, OCPAR, OCPAR_Flag | (0x0F << 12) | (addr & 0xFFF));

		for (i = 0; i < 10; i++) {
			udelay(100);

			/* Check if the RTL8168 has completed ERI write */
			if (!(RTL_R32(tp, OCPAR) & OCPAR_Flag))
				break;
		}

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	udelay(20);

	return 0;
}

u32 rtl8168_ocp_write_with_oob_base_address(struct rtl8168_private *tp,
					    u16 addr, u8 len, u32 value,
					    const u32 base_address)
{
	return rtl8168_eri_write_with_oob_base_address(tp, addr, len, value,
						       ERIAR_OOB, base_address);
}

void rtl8168_ocp_write(struct rtl8168_private *tp, u16 addr, u8 len, u32 value)
{
	if (HW_DASH_SUPPORT_TYPE_2(tp))
		rtl8168_ocp_write_with_oob_base_address(tp, addr, len, value,
							NO_BASE_ADDRESS);
	else if (HW_DASH_SUPPORT_TYPE_3(tp))
		rtl8168_ocp_write_with_oob_base_address(tp, addr, len, value,
							RTL8168FP_OOBMAC_BASE);
	else
		real_ocp_write(tp, addr, len, value);
}

void rtl8168_oob_mutex_lock(struct rtl8168_private *tp)
{
	u8 reg_16, reg_a0;
	u32 wait_cnt_0, wait_Cnt_1;
	u16 ocp_reg_mutex_ib;
	u16 ocp_reg_mutex_oob;
	u16 ocp_reg_mutex_prio;

	if (!tp->DASH)
		return;

	switch (tp->mcfg) {
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	default:
		ocp_reg_mutex_oob = 0x110;
		ocp_reg_mutex_ib = 0x114;
		ocp_reg_mutex_prio = 0x11C;
		break;
	}

	rtl8168_ocp_write(tp, ocp_reg_mutex_ib, 1, BIT_0);
	reg_16 = rtl8168_ocp_read(tp, ocp_reg_mutex_oob, 1);
	wait_cnt_0 = 0;
	while (reg_16) {
		reg_a0 = rtl8168_ocp_read(tp, ocp_reg_mutex_prio, 1);
		if (reg_a0) {
			rtl8168_ocp_write(tp, ocp_reg_mutex_ib, 1, 0x00);
			reg_a0 = rtl8168_ocp_read(tp, ocp_reg_mutex_prio, 1);
			wait_Cnt_1 = 0;
			while (reg_a0) {
				reg_a0 = rtl8168_ocp_read(
					tp, ocp_reg_mutex_prio, 1);
				wait_Cnt_1++;
				if (wait_Cnt_1 > 2000)
					break;
			};
			rtl8168_ocp_write(tp, ocp_reg_mutex_ib, 1, BIT_0);
		}
		reg_16 = rtl8168_ocp_read(tp, ocp_reg_mutex_oob, 1);

		wait_cnt_0++;
		if (wait_cnt_0 > 2000)
			break;
	}
}

void rtl8168_oob_mutex_unlock(struct rtl8168_private *tp)
{
	u16 ocp_reg_mutex_ib;
	u16 ocp_reg_mutex_oob;
	u16 ocp_reg_mutex_prio;

	if (!tp->DASH)
		return;

	switch (tp->mcfg) {
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	default:
		ocp_reg_mutex_oob = 0x110;
		ocp_reg_mutex_ib = 0x114;
		ocp_reg_mutex_prio = 0x11C;
		break;
	}

	rtl8168_ocp_write(tp, ocp_reg_mutex_prio, 1, BIT_0);
	rtl8168_ocp_write(tp, ocp_reg_mutex_ib, 1, 0x00);
}

void rtl8168_oob_notify(struct rtl8168_private *tp, u8 cmd)
{
	rtl8168_eri_write(tp, 0xE8, 1, cmd, ERIAR_ExGMAC);

	rtl8168_ocp_write(tp, 0x30, 1, 0x01);
}

static int rtl8168_check_dash(struct rtl8168_private *tp)
{
	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
		if (rtl8168_ocp_read(tp, 0x128, 1) & BIT_0)
			return 1;
		else
			return 0;
	} else {
		u32 reg;

		if (tp->mcfg == CFG_METHOD_13)
			reg = 0xb8;
		else
			reg = 0x10;

		if (rtl8168_ocp_read(tp, reg, 2) & 0x00008000)
			return 1;
		else
			return 0;
	}
}

void rtl8168_dash2_disable_tx(struct rtl8168_private *tp)
{
	if (!tp->DASH)
		return;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
		u16 WaitCnt;
		u8 TmpUchar;

		//Disable oob Tx
		RTL_CMAC_W8(tp, CMAC_IBCR2,
			    RTL_CMAC_R8(tp, CMAC_IBCR2) & ~(BIT_0));
		WaitCnt = 0;

		//wait oob tx disable
		do {
			TmpUchar = RTL_CMAC_R8(tp, CMAC_IBISR0);

			if (TmpUchar & ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE)
				break;

			udelay(50);
			WaitCnt++;
		} while (WaitCnt < 2000);

		//Clear ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE
		RTL_CMAC_W8(tp, CMAC_IBISR0,
			    RTL_CMAC_R8(tp, CMAC_IBISR0) |
				    ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE);
	}
}

void rtl8168_dash2_enable_tx(struct rtl8168_private *tp)
{
	if (!tp->DASH)
		return;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp))
		RTL_CMAC_W8(tp, CMAC_IBCR2,
			    RTL_CMAC_R8(tp, CMAC_IBCR2) | BIT_0);
}

void rtl8168_dash2_disable_rx(struct rtl8168_private *tp)
{
	if (!tp->DASH)
		return;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp))
		RTL_CMAC_W8(tp, CMAC_IBCR0,
			    RTL_CMAC_R8(tp, CMAC_IBCR0) & ~(BIT_0));
}

void rtl8168_dash2_enable_rx(struct rtl8168_private *tp)
{
	if (!tp->DASH)
		return;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp))
		RTL_CMAC_W8(tp, CMAC_IBCR0,
			    RTL_CMAC_R8(tp, CMAC_IBCR0) | BIT_0);
}

static void rtl8168_dash2_disable_txrx(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
		rtl8168_dash2_disable_tx(tp);
		rtl8168_dash2_disable_rx(tp);
	}
}

void rtl8168_ephy_write(struct rtl8168_private *tp, int RegAddr, int value)
{
	int i;

	RTL_W32(tp, EPHYAR,
		EPHYAR_Write | (RegAddr & EPHYAR_Reg_Mask) << EPHYAR_Reg_shift |
			(value & EPHYAR_Data_Mask));

	for (i = 0; i < 10; i++) {
		udelay(100);

		/* Check if the RTL8168 has completed EPHY write */
		if (!(RTL_R32(tp, EPHYAR) & EPHYAR_Flag))
			break;
	}

	udelay(20);
}

u16 rtl8168_ephy_read(struct rtl8168_private *tp, int RegAddr)
{
	int i;
	u16 value = 0xffff;

	RTL_W32(tp, EPHYAR,
		EPHYAR_Read | (RegAddr & EPHYAR_Reg_Mask) << EPHYAR_Reg_shift);

	for (i = 0; i < 10; i++) {
		udelay(100);

		/* Check if the RTL8168 has completed EPHY read */
		if (RTL_R32(tp, EPHYAR) & EPHYAR_Flag) {
			value = (u16)(RTL_R32(tp, EPHYAR) & EPHYAR_Data_Mask);
			break;
		}
	}

	udelay(20);

	return value;
}

u32 rtl8168_eri_read_with_oob_base_address(struct rtl8168_private *tp, int addr,
					   int len, int type,
					   const u32 base_address)
{
	int i, val_shift, shift = 0;
	u32 value1 = 0, value2 = 0, mask;
	u32 eri_cmd;
	const u32 transformed_base_address =
		((base_address & 0x00FFF000) << 6) | (base_address & 0x000FFF);

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % ERIAR_Addr_Align;
		addr = addr & ~0x3;

		eri_cmd = ERIAR_Read | transformed_base_address |
			  type << ERIAR_Type_shift |
			  ERIAR_ByteEn << ERIAR_ByteEn_shift | (addr & 0x0FFF);
		if (addr & 0xF000) {
			u32 tmp;

			tmp = addr & 0xF000;
			tmp >>= 12;
			eri_cmd |= (tmp << 20) & 0x00F00000;
		}

		RTL_W32(tp, ERIAR, eri_cmd);

		for (i = 0; i < 10; i++) {
			udelay(100);

			/* Check if the RTL8168 has completed ERI read */
			if (RTL_R32(tp, ERIAR) & ERIAR_Flag)
				break;
		}

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = RTL_R32(tp, ERIDR) & mask;
		value2 |= (value1 >> val_shift * 8) << shift * 8;

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	udelay(20);

	return value2;
}

u32 rtl8168_eri_read(struct rtl8168_private *tp, int addr, int len, int type)
{
	return rtl8168_eri_read_with_oob_base_address(tp, addr, len, type, 0);
}

int rtl8168_eri_write_with_oob_base_address(struct rtl8168_private *tp,
					    int addr, int len, u32 value,
					    int type, const u32 base_address)
{
	int i, val_shift, shift = 0;
	u32 value1 = 0, mask;
	u32 eri_cmd;
	const u32 transformed_base_address =
		((base_address & 0x00FFF000) << 6) | (base_address & 0x000FFF);

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % ERIAR_Addr_Align;
		addr = addr & ~0x3;

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = rtl8168_eri_read_with_oob_base_address(
				 tp, addr, 4, type, base_address) &
			 ~mask;
		value1 |= ((value << val_shift * 8) >> shift * 8);

		RTL_W32(tp, ERIDR, value1);

		eri_cmd = ERIAR_Write | transformed_base_address |
			  type << ERIAR_Type_shift |
			  ERIAR_ByteEn << ERIAR_ByteEn_shift | (addr & 0x0FFF);
		if (addr & 0xF000) {
			u32 tmp;

			tmp = addr & 0xF000;
			tmp >>= 12;
			eri_cmd |= (tmp << 20) & 0x00F00000;
		}

		RTL_W32(tp, ERIAR, eri_cmd);
		for (i = 0; i < 10; i++) {
			udelay(100);
			/* Check if the RTL8168 has completed ERI write */
			if (!(RTL_R32(tp, ERIAR) & ERIAR_Flag))
				break;
		}

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	udelay(20);

	return 0;
}

int rtl8168_eri_write(struct rtl8168_private *tp, int addr, int len, u32 value,
		      int type)
{
	return rtl8168_eri_write_with_oob_base_address(tp, addr, len, value,
						       type, NO_BASE_ADDRESS);
}

static void rtl8168_enable_rxdvgate(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W8(tp, 0xF2, RTL_R8(tp, 0xF2) | BIT_3);
		mdelay(2);
		break;
	}
}

static void rtl8168_disable_rxdvgate(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W8(tp, 0xF2, RTL_R8(tp, 0xF2) & ~BIT_3);
		mdelay(2);
		break;
	}
}

static u8 rtl8168_is_gpio_low(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u8 gpio_low = FALSE;

	switch (tp->HwSuppCheckPhyDisableModeVer) {
	case 1:
	case 2:
		if (!(rtl8168_mac_ocp_read(tp, 0xDC04) & BIT_9))
			gpio_low = TRUE;
		break;
	case 3:
		if (!(rtl8168_mac_ocp_read(tp, 0xDC04) & BIT_13))
			gpio_low = TRUE;
		break;
	}

	if (gpio_low)
		dprintk("gpio is low.\n");

	return gpio_low;
}

static u8 rtl8168_is_phy_disable_mode_enabled(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u8 phy_disable_mode_enabled = FALSE;

	switch (tp->HwSuppCheckPhyDisableModeVer) {
	case 1:
		if (rtl8168_mac_ocp_read(tp, 0xDC20) & BIT_1)
			phy_disable_mode_enabled = TRUE;
		break;
	case 2:
	case 3:
		if (RTL_R8(tp, 0xF2) & BIT_5)
			phy_disable_mode_enabled = TRUE;
		break;
	}

	if (phy_disable_mode_enabled)
		dprintk("phy disable mode enabled.\n");

	return phy_disable_mode_enabled;
}

static u8 rtl8168_is_in_phy_disable_mode(struct net_device *dev)
{
	u8 in_phy_disable_mode = FALSE;

	if (rtl8168_is_phy_disable_mode_enabled(dev) &&
	    rtl8168_is_gpio_low(dev))
		in_phy_disable_mode = TRUE;

	if (in_phy_disable_mode)
		dprintk("Hardware is in phy disable mode.\n");

	return in_phy_disable_mode;
}

void rtl8168_wait_txrx_fifo_empty(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int i;

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		for (i = 0; i < 10; i++) {
			udelay(100);
			if (RTL_R32(tp, TxConfig) & BIT_11)
				break;
		}
		for (i = 0; i < 10; i++) {
			udelay(100);
			if ((RTL_R8(tp, MCUCmd_reg) &
			     (Txfifo_empty | Rxfifo_empty)) ==
			    (Txfifo_empty | Rxfifo_empty))
				break;
		}
		break;
	}
}

static void rtl8168_driver_start(struct rtl8168_private *tp)
{
	//change other device state to D0.
	if (!tp->DASH)
		return;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
		int timeout;
		u32 tmp_value;

		rtl8168_ocp_write(tp, 0x180, 1, OOB_CMD_DRIVER_START);
		tmp_value = rtl8168_ocp_read(tp, 0x30, 1);
		tmp_value |= BIT_0;
		rtl8168_ocp_write(tp, 0x30, 1, tmp_value);

		for (timeout = 0; timeout < 10; timeout++) {
			mdelay(10);
			if (rtl8168_ocp_read(tp, 0x124, 1) & BIT_0)
				break;
		}
	} else {
		int timeout;
		u32 reg;

		if (tp->mcfg == CFG_METHOD_13) {
			RTL_W8(tp, TwiCmdReg, RTL_R8(tp, TwiCmdReg) | (BIT_7));
		}

		rtl8168_oob_notify(tp, OOB_CMD_DRIVER_START);

		if (tp->mcfg == CFG_METHOD_13)
			reg = 0xB8;
		else
			reg = 0x10;

		for (timeout = 0; timeout < 10; timeout++) {
			mdelay(10);
			if (rtl8168_ocp_read(tp, reg, 2) & BIT_11)
				break;
		}
	}
}

static void rtl8168_driver_stop(struct rtl8168_private *tp)
{
	if (!tp->DASH)
		goto update_device_state;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
		struct net_device *dev = tp->dev;
		int timeout;
		u32 tmp_value;

		rtl8168_dash2_disable_txrx(dev);

		rtl8168_ocp_write(tp, 0x180, 1, OOB_CMD_DRIVER_STOP);
		tmp_value = rtl8168_ocp_read(tp, 0x30, 1);
		tmp_value |= BIT_0;
		rtl8168_ocp_write(tp, 0x30, 1, tmp_value);

		for (timeout = 0; timeout < 10; timeout++) {
			mdelay(10);
			if (!(rtl8168_ocp_read(tp, 0x124, 1) & BIT_0))
				break;
		}
	} else {
		int timeout;
		u32 reg;

		rtl8168_oob_notify(tp, OOB_CMD_DRIVER_STOP);

		if (tp->mcfg == CFG_METHOD_13)
			reg = 0xB8;
		else
			reg = 0x10;

		for (timeout = 0; timeout < 10; timeout++) {
			mdelay(10);
			if ((rtl8168_ocp_read(tp, reg, 2) & BIT_11) == 0)
				break;
		}

		if (tp->mcfg == CFG_METHOD_13) {
			RTL_W8(tp, TwiCmdReg, RTL_R8(tp, TwiCmdReg) & ~(BIT_7));
		}
	}

update_device_state:
	//change other device state to D3.
	switch (tp->mcfg) {
	case CFG_METHOD_33:
		break;
	}
}

#ifdef ENABLE_DASH_SUPPORT
inline void rtl8168_enable_dash2_interrupt(struct rtl8168_private *tp)
{
	if (!tp->DASH)
		return;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp))
		RTL_CMAC_W8(tp, CMAC_IBIMR0,
			    (ISRIMR_DASH_TYPE2_ROK | ISRIMR_DASH_TYPE2_TOK |
			     ISRIMR_DASH_TYPE2_TDU | ISRIMR_DASH_TYPE2_RDU |
			     ISRIMR_DASH_TYPE2_RX_DISABLE_IDLE));
}

static inline void rtl8168_disable_dash2_interrupt(struct rtl8168_private *tp)
{
	if (!tp->DASH)
		return;

	if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
		RTL_CMAC_W8(tp, CMAC_IBIMR0, 0);
	}
}
#endif

static inline void rtl8168_enable_hw_interrupt(struct rtl8168_private *tp)
{
	RTL_W16(tp, IntrMask, tp->intr_mask);

#ifdef ENABLE_DASH_SUPPORT
	if (tp->DASH)
		rtl8168_enable_dash2_interrupt(tp);
#endif
}

static inline void rtl8168_disable_hw_interrupt(struct rtl8168_private *tp)
{
	RTL_W16(tp, IntrMask, 0x0000);

#ifdef ENABLE_DASH_SUPPORT
	if (tp->DASH)
		rtl8168_disable_dash2_interrupt(tp);
#endif
}

static inline void rtl8168_switch_to_hw_interrupt(struct rtl8168_private *tp)
{
	RTL_W32(tp, TimeInt0, 0x0000);

	rtl8168_enable_hw_interrupt(tp);
}

static inline void rtl8168_switch_to_timer_interrupt(struct rtl8168_private *tp)
{
	if (tp->use_timer_interrrupt) {
		RTL_W32(tp, TimeInt0, timer_count);
		RTL_W32(tp, TCTR, timer_count);
		RTL_W16(tp, IntrMask, tp->timer_intr_mask);

#ifdef ENABLE_DASH_SUPPORT
		if (tp->DASH)
			rtl8168_enable_dash2_interrupt(tp);
#endif
	} else {
		rtl8168_switch_to_hw_interrupt(tp);
	}
}

static void rtl8168_irq_mask_and_ack(struct rtl8168_private *tp)
{
	rtl8168_disable_hw_interrupt(tp);
#ifdef ENABLE_DASH_SUPPORT
	if (tp->DASH) {
		if (tp->dash_printer_enabled) {
			RTL_W16(tp, IntrStatus,
				RTL_R16(tp, IntrStatus) &
					~(ISRIMR_DASH_INTR_EN |
					  ISRIMR_DASH_INTR_CMAC_RESET));
		} else {
			if (HW_DASH_SUPPORT_TYPE_2(tp) ||
			    HW_DASH_SUPPORT_TYPE_3(tp))
				RTL_CMAC_W8(tp, CMAC_IBISR0,
					    RTL_CMAC_R8(tp, CMAC_IBISR0));
		}
	} else {
		RTL_W16(tp, IntrStatus, RTL_R16(tp, IntrStatus));
	}
#else
	RTL_W16(tp, IntrStatus, RTL_R16(tp, IntrStatus));
#endif
}

static void rtl8168_nic_reset(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int i;

	RTL_W32(tp, RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

	rtl8168_enable_rxdvgate(dev);

	rtl8168_wait_txrx_fifo_empty(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		mdelay(2);
		break;
	default:
		mdelay(10);
		break;
	}

	/* Soft reset the chip. */
	RTL_W8(tp, ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 100; i > 0; i--) {
		udelay(100);
		if ((RTL_R8(tp, ChipCmd) & CmdReset) == 0)
			break;
	}
}

static void rtl8168_hw_clear_timer_int(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	RTL_W32(tp, TimeInt0, 0x0000);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W32(tp, TimeInt1, 0x0000);
		RTL_W32(tp, TimeInt2, 0x0000);
		RTL_W32(tp, TimeInt3, 0x0000);
		break;
	}
}

static void rtl8168_hw_reset(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	/* Disable interrupts */
	rtl8168_irq_mask_and_ack(tp);

	rtl8168_hw_clear_timer_int(dev);

	rtl8168_nic_reset(dev);
}

static void rtl8168_calibration_setting(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	/*r-calibration setting*/
	rtl8168_mdio_write(tp, 0x1f, 0xBC0);
	rtl8168_mdio_write(tp, 0x14, 0x14);
	/* idac_fine, driver will modify it based on OTP value */
	rtl8168_mdio_write(tp, 0x1f, 0xBC0);
	rtl8168_mdio_write(tp, 0x17, 0xbb);
}

static void rtl8168_mac_loopback_test(struct rtl8168_private *tp)
{
	struct net_device *dev = tp->dev;
	struct sk_buff *skb, *rx_skb;
	dma_addr_t mapping;
	struct TxDesc *txd;
	struct RxDesc *rxd;
	void *tmpAddr;
	u32 len, rx_len, rx_cmd = 0;
	u16 type;
	u8 pattern;
	int i;

	if (tp->DASH)
		return;

	pattern = 0x5A;
	len = 60;
	type = htons(ETH_P_IP);
	txd = tp->TxDescArray;
	rxd = tp->RxDescArray;
	rx_skb = tp->Rx_skbuff[0];
	RTL_W32(tp, TxConfig,
		(RTL_R32(tp, TxConfig) & ~0x00060000) | 0x00020000);

	do {
		skb = alloc_skb(len + RTK_RX_ALIGN + NET_SKB_PAD, GFP_ATOMIC);
		if (unlikely(!skb))
			dev_printk(KERN_NOTICE, &tp->platform_dev->dev,
				   "-ENOMEM;\n");
	} while (unlikely(skb == NULL));
	skb_reserve(skb, RTK_RX_ALIGN);

	memcpy(skb_put(skb, dev->addr_len), dev->dev_addr, dev->addr_len);
	memcpy(skb_put(skb, dev->addr_len), dev->dev_addr, dev->addr_len);
	memcpy(skb_put(skb, sizeof(type)), &type, sizeof(type));
	tmpAddr = skb_put(skb, len - 14);

	mapping = dma_map_single(&tp->platform_dev->dev, skb->data, len,
				 DMA_TO_DEVICE);
	dma_sync_single_for_cpu(&tp->platform_dev->dev, le64_to_cpu(mapping),
				len, DMA_TO_DEVICE);
	txd->addr = cpu_to_le64(mapping);
	txd->opts2 = 0;
	while (1) {
		memset(tmpAddr, pattern++, len - 14);
		dma_sync_single_for_device(&tp->platform_dev->dev,
					   le64_to_cpu(mapping), len,
					   DMA_TO_DEVICE);
		txd->opts1 = cpu_to_le32(DescOwn | FirstFrag | LastFrag | len);

		RTL_W32(tp, RxConfig, RTL_R32(tp, RxConfig) | AcceptMyPhys);

		smp_wmb();
		RTL_W8(tp, TxPoll, NPQ); /* set polling bit */

		for (i = 0; i < 50; i++) {
			udelay(200);
			rx_cmd = le32_to_cpu(rxd->opts1);
			if ((rx_cmd & DescOwn) == 0)
				break;
		}

		RTL_W32(tp, RxConfig,
			RTL_R32(tp, RxConfig) &
				~(AcceptErr | AcceptRunt | AcceptBroadcast |
				  AcceptMulticast | AcceptMyPhys |
				  AcceptAllPhys));

		rx_len = rx_cmd & 0x3FFF;
		rx_len -= 4;
		rxd->opts1 = cpu_to_le32(DescOwn | tp->rx_buf_sz);

		dma_sync_single_for_cpu(&tp->platform_dev->dev,
					le64_to_cpu(mapping), len,
					DMA_TO_DEVICE);

		if (rx_len == len) {
			dma_sync_single_for_cpu(&tp->platform_dev->dev,
						le64_to_cpu(rxd->addr),
						tp->rx_buf_sz, DMA_FROM_DEVICE);
			i = memcmp(skb->data, rx_skb->data, rx_len);
			dma_sync_single_for_device(&tp->platform_dev->dev,
						   le64_to_cpu(rxd->addr),
						   tp->rx_buf_sz,
						   DMA_FROM_DEVICE);
			if (i == 0) {
				break;
			}
		}

		rtl8168_hw_reset(dev);
		rtl8168_disable_rxdvgate(dev);
		RTL_W8(tp, ChipCmd, CmdTxEnb | CmdRxEnb);
	}
	tp->dirty_tx++;
	tp->dirty_rx++;
	tp->cur_tx++;
	tp->cur_rx++;
	dma_unmap_single(&tp->platform_dev->dev, le64_to_cpu(mapping), len,
			 DMA_TO_DEVICE);
	RTL_W32(tp, TxConfig, RTL_R32(tp, TxConfig) & ~0x00060000);
	dev_kfree_skb_any(skb);
	RTL_W16(tp, IntrStatus, 0xFFBF);
}

static unsigned int rtl8168_xmii_reset_pending(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned int retval;

	rtl8168_mdio_write(tp, 0x1f, 0x0000);
	retval = rtl8168_mdio_read(tp, MII_BMCR) & BMCR_RESET;

	return retval;
}

static unsigned int rtl8168_xmii_link_ok(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned int retval;

	retval = (RTL_R8(tp, PHYstatus) & LinkStatus) ? 1 : 0;

	return retval;
}

static void rtl8168_xmii_reset_enable(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int i, val = 0;

	if (rtl8168_is_in_phy_disable_mode(dev))
		return;

	rtl8168_mdio_write(tp, 0x1f, 0x0000);
	rtl8168_mdio_write(tp, MII_ADVERTISE,
			   rtl8168_mdio_read(tp, MII_ADVERTISE) &
				   ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
				     ADVERTISE_100HALF | ADVERTISE_100FULL));
	rtl8168_mdio_write(tp, MII_CTRL1000,
			   rtl8168_mdio_read(tp, MII_CTRL1000) &
				   ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL));
	rtl8168_mdio_write(tp, MII_BMCR, BMCR_RESET | BMCR_ANENABLE);

	for (i = 0; i < 2500; i++) {
		val = rtl8168_mdio_read(tp, MII_BMCR) & BMCR_RESET;

		if (!val) {
			return;
		}

		mdelay(1);
	}

	if (netif_msg_link(tp))
		printk(KERN_ERR "%s: PHY reset failed.\n", dev->name);
}

static void rtl8168dp_10mbps_gphy_para(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u8 status = RTL_R8(tp, PHYstatus);

	if ((status & LinkStatus) && (status & _10bps)) {
		rtl8168_mdio_write(tp, 0x1f, 0x0000);
		rtl8168_mdio_write(tp, 0x10, 0x04EE);
	} else {
		rtl8168_mdio_write(tp, 0x1f, 0x0000);
		rtl8168_mdio_write(tp, 0x10, 0x01EE);
	}
}

void rtl8168_init_ring_indexes(struct rtl8168_private *tp)
{
	tp->dirty_tx = 0;
	tp->dirty_rx = 0;
	tp->cur_tx = 0;
	tp->cur_rx = 0;
}

static void rtl8168_issue_offset_99_event(struct rtl8168_private *tp)
{
	u32 csi_tmp;

	switch (tp->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		if (tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25 ||
		    tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28) {
			rtl8168_eri_write(tp, 0x3FC, 4, 0x00000000,
					  ERIAR_ExGMAC);
		} else {
			rtl8168_eri_write(tp, 0x3FC, 4, 0x083C083C,
					  ERIAR_ExGMAC);
		}
		csi_tmp = rtl8168_eri_read(tp, 0x3F8, 1, ERIAR_ExGMAC);
		csi_tmp |= BIT_0;
		rtl8168_eri_write(tp, 0x3F8, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
		csi_tmp = rtl8168_eri_read(tp, 0x1EA, 1, ERIAR_ExGMAC);
		csi_tmp |= BIT_0;
		rtl8168_eri_write(tp, 0x1EA, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	}
}

#ifdef ENABLE_DASH_SUPPORT
static void NICChkTypeEnableDashInterrupt(struct rtl8168_private *tp)
{
	if (tp->DASH) {
		if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
			rtl8168_enable_dash2_interrupt(tp);
			RTL_W16(tp, IntrMask,
				(ISRIMR_DASH_INTR_EN |
				 ISRIMR_DASH_INTR_CMAC_RESET));
		} else {
			RTL_W16(tp, IntrMask,
				(ISRIMR_DP_DASH_OK | ISRIMR_DP_HOST_OK |
				 ISRIMR_DP_REQSYS_OK));
		}
	}
}
#endif

static void rtl8168_check_link_status(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int link_status_on;

#ifdef ENABLE_FIBER_SUPPORT
	rtl8168_check_fiber_link_status(dev);
#endif //ENABLE_FIBER_SUPPORT

	link_status_on = tp->link_ok(dev);

	if (tp->mcfg == CFG_METHOD_11)
		rtl8168dp_10mbps_gphy_para(dev);

	if (netif_carrier_ok(dev) != link_status_on) {
		if (link_status_on) {
			rtl8168_hw_config(dev);

			if ((tp->mcfg == CFG_METHOD_24 ||
			     tp->mcfg == CFG_METHOD_25 ||
			     tp->mcfg == CFG_METHOD_33) &&
			    netif_running(dev)) {
				if (RTL_R8(tp, PHYstatus) & FullDup)
					RTL_W32(tp, TxConfig,
						(RTL_R32(tp, TxConfig) |
						 (BIT_24 | BIT_25)) &
							~BIT_19);
				else
					RTL_W32(tp, TxConfig,
						(RTL_R32(tp, TxConfig) |
						 BIT_25) &
							~(BIT_19 | BIT_24));
			}

			rtl8168_hw_start(dev);

			netif_carrier_on(dev);

			netif_wake_queue(dev);

			rtl8168_mdio_write(tp, 0x1F, 0x0000);
			tp->phy_reg_anlpar = rtl8168_mdio_read(tp, MII_LPA);

			if (netif_msg_ifup(tp))
				printk(KERN_INFO PFX "%s: link up\n",
				       dev->name);
		} else {
			if (netif_msg_ifdown(tp))
				printk(KERN_INFO PFX "%s: link down\n",
				       dev->name);

			tp->phy_reg_anlpar = 0;

			netif_stop_queue(dev);

			netif_carrier_off(dev);

			rtl8168_hw_reset(dev);

			rtl8168_tx_clear(tp);

			rtl8168_rx_clear(tp);

			rtl8168_init_ring(dev);

			rtl8168_set_speed(dev, tp->autoneg, tp->speed,
					  tp->duplex, tp->advertising);

			switch (tp->mcfg) {
			case CFG_METHOD_24:
			case CFG_METHOD_25:
				if (tp->org_pci_offset_99 & BIT_2)
					tp->issue_offset_99_event = TRUE;
				break;
			}

#ifdef ENABLE_DASH_SUPPORT
			if (tp->DASH)
				NICChkTypeEnableDashInterrupt(tp);
#endif
		}
	}

	if (!link_status_on) {
		switch (tp->mcfg) {
		case CFG_METHOD_24:
		case CFG_METHOD_25:
			if (tp->issue_offset_99_event) {
				if (!(RTL_R8(tp, PHYstatus) &
				      PowerSaveStatus)) {
					tp->issue_offset_99_event = FALSE;
					rtl8168_issue_offset_99_event(tp);
				}
			}
			break;
		}
	}
}

static void rtl8168_link_option(u8 *aut, u32 *spd, u8 *dup, u32 *adv)
{
	if ((*spd != SPEED_1000) && (*spd != SPEED_100) && (*spd != SPEED_10))
		*spd = SPEED_100;

	if ((*dup != DUPLEX_FULL) && (*dup != DUPLEX_HALF))
		*dup = DUPLEX_FULL;

	if ((*aut != AUTONEG_ENABLE) && (*aut != AUTONEG_DISABLE))
		*aut = AUTONEG_ENABLE;

	*adv &= (ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
		 ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
		 ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full);
	if (*adv == 0)
		*adv = (ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
			ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
			ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full);
}

void rtl8168_wait_ll_share_fifo_ready(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int i;

	for (i = 0; i < 10; i++) {
		udelay(100);
		if (RTL_R16(tp, 0xD2) & BIT_9)
			break;
	}
}

static void rtl8168_enable_cfg9346_write(struct rtl8168_private *tp)
{
	RTL_W8(tp, Cfg9346, RTL_R8(tp, Cfg9346) | Cfg9346_Unlock);
}

static void rtl8168_disable_cfg9346_write(struct rtl8168_private *tp)
{
	RTL_W8(tp, Cfg9346, RTL_R8(tp, Cfg9346) & ~Cfg9346_Unlock);
}

static void rtl8168_hw_d3_para(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	RTL_W16(tp, RxMaxSize, RX_BUF_SIZE);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W8(tp, 0xF1, RTL_R8(tp, 0xF1) & ~BIT_7);
		rtl8168_enable_cfg9346_write(tp);
		RTL_W8(tp, Config2, RTL_R8(tp, Config2) & ~BIT_7);
		RTL_W8(tp, Config5, RTL_R8(tp, Config5) & ~BIT_0);
		rtl8168_disable_cfg9346_write(tp);
		break;
	}

#ifdef ENABLE_REALWOW_SUPPORT
	rtl8168_set_realwow_d3_para(dev);
#endif

	if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19 ||
	    tp->mcfg == CFG_METHOD_20) {
		rtl8168_eri_write(tp, 0x1bc, 4, 0x0000001f, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0x1dc, 4, 0x0000002d, ERIAR_ExGMAC);
	} else if (tp->mcfg == CFG_METHOD_16) {
		rtl8168_eri_write(tp, 0x1bc, 4, 0x0000001f, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0x1dc, 4, 0x0000003f, ERIAR_ExGMAC);
	}

	if (tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_33)
		rtl8168_eri_write(tp, 0x2F8, 2, 0x0064, ERIAR_ExGMAC);

	if (tp->bios_setting & BIT_28) {
		if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19 ||
		    tp->mcfg == CFG_METHOD_20) {
			u32 gphy_val;

			rtl8168_mdio_write(tp, 0x1F, 0x0000);
			rtl8168_mdio_write(tp, 0x04, 0x0061);
			rtl8168_mdio_write(tp, 0x09, 0x0000);
			rtl8168_mdio_write(tp, 0x00, 0x9200);
			rtl8168_mdio_write(tp, 0x1F, 0x0005);
			rtl8168_mdio_write(tp, 0x05, 0x8B80);
			gphy_val = rtl8168_mdio_read(tp, 0x06);
			gphy_val &= ~BIT_7;
			rtl8168_mdio_write(tp, 0x06, gphy_val);
			mdelay(1);
			rtl8168_mdio_write(tp, 0x1F, 0x0007);
			rtl8168_mdio_write(tp, 0x1E, 0x002C);
			gphy_val = rtl8168_mdio_read(tp, 0x16);
			gphy_val &= ~BIT_10;
			rtl8168_mdio_write(tp, 0x16, gphy_val);
			rtl8168_mdio_write(tp, 0x1F, 0x0000);
		}
	}

	rtl8168_disable_rxdvgate(dev);
}

static void rtl8168_enable_magic_packet(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u32 csi_tmp;

	switch (tp->HwSuppMagicPktVer) {
	case WAKEUP_MAGIC_PACKET_V1:
		rtl8168_enable_cfg9346_write(tp);
		RTL_W8(tp, Config3, RTL_R8(tp, Config3) | MagicPacket);
		rtl8168_disable_cfg9346_write(tp);
		break;
	case WAKEUP_MAGIC_PACKET_V2:
		csi_tmp = rtl8168_eri_read(tp, 0xDE, 1, ERIAR_ExGMAC);
		csi_tmp |= BIT_0;
		rtl8168_eri_write(tp, 0xDE, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	}
}
static void rtl8168_disable_magic_packet(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u32 csi_tmp;

	switch (tp->HwSuppMagicPktVer) {
	case WAKEUP_MAGIC_PACKET_V1:
		rtl8168_enable_cfg9346_write(tp);
		RTL_W8(tp, Config3, RTL_R8(tp, Config3) & ~MagicPacket);
		rtl8168_disable_cfg9346_write(tp);
		break;
	case WAKEUP_MAGIC_PACKET_V2:
		csi_tmp = rtl8168_eri_read(tp, 0xDE, 1, ERIAR_ExGMAC);
		csi_tmp &= ~BIT_0;
		rtl8168_eri_write(tp, 0xDE, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	}
}

#define WAKE_ANY (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST)
static void rtl8168_get_hw_wol(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u8 options;
	u32 csi_tmp;
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);

	tp->wol_opts = 0;
	options = RTL_R8(tp, Config1);
	if (!(options & PMEnable))
		goto out_unlock;

	options = RTL_R8(tp, Config3);
	if (options & LinkUp)
		tp->wol_opts |= WAKE_PHY;

	switch (tp->HwSuppMagicPktVer) {
	case WAKEUP_MAGIC_PACKET_V2:
		csi_tmp = rtl8168_eri_read(tp, 0xDE, 1, ERIAR_ExGMAC);
		if (csi_tmp & BIT_0)
			tp->wol_opts |= WAKE_MAGIC;
		break;
	default:
		if (options & MagicPacket)
			tp->wol_opts |= WAKE_MAGIC;
		break;
	}

	options = RTL_R8(tp, Config5);
	if (options & UWF)
		tp->wol_opts |= WAKE_UCAST;
	if (options & BWF)
		tp->wol_opts |= WAKE_BCAST;
	if (options & MWF)
		tp->wol_opts |= WAKE_MCAST;

out_unlock:
	tp->wol_enabled = (tp->wol_opts || tp->dash_printer_enabled) ?
				  WOL_ENABLED :
				  WOL_DISABLED;

	spin_unlock_irqrestore(&tp->lock, flags);
}

static void rtl8168_set_hw_wol(struct net_device *dev, u32 wolopts)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int i, tmp;

	static struct {
		u32 opt;
		u16 reg;
		u8 mask;
	} cfg[] = {
		{ WAKE_PHY, Config3, LinkUp },
		{ WAKE_UCAST, Config5, UWF },
		{ WAKE_BCAST, Config5, BWF },
		{ WAKE_MCAST, Config5, MWF },
		{ WAKE_ANY, Config5, LanWake },
		{ WAKE_MAGIC, Config3, MagicPacket },
	};

	switch (tp->HwSuppMagicPktVer) {
	case WAKEUP_MAGIC_PACKET_V2:
		tmp = ARRAY_SIZE(cfg) - 1;

		if (wolopts & WAKE_MAGIC)
			rtl8168_enable_magic_packet(dev);
		else
			rtl8168_disable_magic_packet(dev);
		break;
	default:
		tmp = ARRAY_SIZE(cfg);
		break;
	}

	rtl8168_enable_cfg9346_write(tp);

	for (i = 0; i < tmp; i++) {
		u8 options = RTL_R8(tp, cfg[i].reg) & ~cfg[i].mask;

		if (wolopts & cfg[i].opt)
			options |= cfg[i].mask;
		RTL_W8(tp, cfg[i].reg, options);
	}

	if (tp->dash_printer_enabled)
		RTL_W8(tp, Config5, RTL_R8(tp, Config5) | LanWake);

	rtl8168_disable_cfg9346_write(tp);
}

static void rtl8168_phy_restart_nway(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	if (rtl8168_is_in_phy_disable_mode(dev))
		return;

	rtl8168_mdio_write(tp, 0x1F, 0x0000);
	rtl8168_mdio_write(tp, MII_BMCR,
			   BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART);
}

static void rtl8168_phy_setup_force_mode(struct net_device *dev, u32 speed,
					 u8 duplex)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u16 bmcr_true_force = 0;

	if (rtl8168_is_in_phy_disable_mode(dev))
		return;

	if ((speed == SPEED_10) && (duplex == DUPLEX_HALF)) {
		bmcr_true_force = BMCR_SPEED10;
	} else if ((speed == SPEED_10) && (duplex == DUPLEX_FULL)) {
		bmcr_true_force = BMCR_SPEED10 | BMCR_FULLDPLX;
	} else if ((speed == SPEED_100) && (duplex == DUPLEX_HALF)) {
		bmcr_true_force = BMCR_SPEED100;
	} else if ((speed == SPEED_100) && (duplex == DUPLEX_FULL)) {
		bmcr_true_force = BMCR_SPEED100 | BMCR_FULLDPLX;
	} else if ((speed == SPEED_1000) && (duplex == DUPLEX_FULL) &&
		   tp->HwSuppGigaForceMode) {
		bmcr_true_force = BMCR_SPEED1000 | BMCR_FULLDPLX;
	} else {
		netif_err(tp, drv, dev, "Failed to set phy force mode!\n");
		return;
	}

	rtl8168_mdio_write(tp, 0x1F, 0x0000);
	rtl8168_mdio_write(tp, MII_BMCR, bmcr_true_force);
}

static void rtl8168_powerdown_pll(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

#ifdef ENABLE_FIBER_SUPPORT
	if (HW_FIBER_MODE_ENABLED(tp))
		return;
#endif //ENABLE_FIBER_SUPPORT

	if (tp->wol_enabled == WOL_ENABLED || tp->DASH ||
	    tp->EnableKCPOffload) {
		int auto_nego;
		int giga_ctrl;
		u16 anlpar;

		rtl8168_set_hw_wol(dev, tp->wol_opts);

		if (tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_25) {
			rtl8168_enable_cfg9346_write(tp);
			RTL_W8(tp, Config2, RTL_R8(tp, Config2) | PMSTS_En);
			rtl8168_disable_cfg9346_write(tp);
		}

		if (HW_SUPP_SERDES_PHY(tp))
			return;

		rtl8168_mdio_write(tp, 0x1F, 0x0000);
		auto_nego = rtl8168_mdio_read(tp, MII_ADVERTISE);
		auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
			       ADVERTISE_100HALF | ADVERTISE_100FULL);

		if (netif_running(dev))
			anlpar = tp->phy_reg_anlpar;
		else
			anlpar = rtl8168_mdio_read(tp, MII_LPA);

#ifdef CONFIG_DOWN_SPEED_100
		auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF |
			      ADVERTISE_10HALF | ADVERTISE_10FULL);
#else
		if (anlpar & (LPA_10HALF | LPA_10FULL))
			auto_nego |= (ADVERTISE_10HALF | ADVERTISE_10FULL);
		else
			auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF |
				      ADVERTISE_10HALF | ADVERTISE_10FULL);
#endif

		if (tp->DASH)
			auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF |
				      ADVERTISE_10HALF | ADVERTISE_10FULL);

		if (((tp->mcfg == CFG_METHOD_7) ||
		     (tp->mcfg == CFG_METHOD_8)) &&
		    (RTL_R16(tp, CPlusCmd) & ASF))
			auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF |
				      ADVERTISE_10HALF | ADVERTISE_10FULL);

		giga_ctrl = rtl8168_mdio_read(tp, MII_CTRL1000) &
			    ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
		rtl8168_mdio_write(tp, MII_ADVERTISE, auto_nego);
		rtl8168_mdio_write(tp, MII_CTRL1000, giga_ctrl);
		rtl8168_phy_restart_nway(dev);

		RTL_W32(tp, RxConfig,
			RTL_R32(tp, RxConfig) | AcceptBroadcast |
				AcceptMulticast | AcceptMyPhys);

		return;
	}

	if (tp->DASH)
		return;

	if (((tp->mcfg == CFG_METHOD_7) || (tp->mcfg == CFG_METHOD_8)) &&
	    (RTL_R16(tp, CPlusCmd) & ASF))
		return;

	rtl8168_phy_power_down(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W8(tp, PMCH, RTL_R8(tp, PMCH) & ~BIT_7);
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_14 ... CFG_METHOD_15:
		RTL_W8(tp, 0xD0, RTL_R8(tp, 0xD0) & ~BIT_6);
		break;
	case CFG_METHOD_16 ... CFG_METHOD_33:
		RTL_W8(tp, 0xD0, RTL_R8(tp, 0xD0) & ~BIT_6);
		RTL_W8(tp, 0xF2, RTL_R8(tp, 0xF2) & ~BIT_6);
		break;
	}
}

static void rtl8168_powerup_pll(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W8(tp, PMCH, RTL_R8(tp, PMCH) | BIT_7 | BIT_6);
		break;
	}

	rtl8168_phy_power_up(dev);
}

static void rtl8168_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u8 options;
	unsigned long flags;

	wol->wolopts = 0;

	if (tp->mcfg == CFG_METHOD_DEFAULT) {
		wol->supported = 0;
		return;
	} else {
		wol->supported = WAKE_ANY;
	}

	spin_lock_irqsave(&tp->lock, flags);

	options = RTL_R8(tp, Config1);
	if (!(options & PMEnable))
		goto out_unlock;

	wol->wolopts = tp->wol_opts;

out_unlock:
	spin_unlock_irqrestore(&tp->lock, flags);
}

static int rtl8168_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	if (tp->mcfg == CFG_METHOD_DEFAULT)
		return -EOPNOTSUPP;

	spin_lock_irqsave(&tp->lock, flags);

	tp->wol_opts = wol->wolopts;

	tp->wol_enabled = (tp->wol_opts || tp->dash_printer_enabled) ?
				  WOL_ENABLED :
				  WOL_DISABLED;

	spin_unlock_irqrestore(&tp->lock, flags);

	device_set_wakeup_enable(&tp->platform_dev->dev, wol->wolopts);

	return 0;
}

static void rtl8168_get_drvinfo(struct net_device *dev,
				struct ethtool_drvinfo *info)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	strcpy(info->driver, MODULENAME);
	strcpy(info->version, RTL8168_VERSION);
	strcpy(info->bus_info, "platform");
	info->regdump_len = R8168_REGS_DUMP_SIZE;
	info->eedump_len = tp->eeprom_len;
}

static int rtl8168_get_regs_len(struct net_device *dev)
{
	return R8168_REGS_DUMP_SIZE;
}

static int rtl8168_set_speed_xmii(struct net_device *dev, u8 autoneg, u32 speed,
				  u8 duplex, u32 adv)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int auto_nego = 0;
	int giga_ctrl = 0;
	int rc = -EINVAL;

	if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30 ||
	    tp->mcfg == CFG_METHOD_31 || tp->mcfg == CFG_METHOD_32 ||
	    tp->mcfg == CFG_METHOD_33) {
		//Disable Giga Lite
		rtl8168_mdio_write(tp, 0x1F, 0x0A42);
		rtl8168_clear_eth_phy_bit(tp, 0x14, BIT_9);
		if (tp->mcfg == CFG_METHOD_31 || tp->mcfg == CFG_METHOD_32 ||
		    tp->mcfg == CFG_METHOD_33)
			rtl8168_clear_eth_phy_bit(tp, 0x14, BIT_7);
		rtl8168_mdio_write(tp, 0x1F, 0x0A40);
		rtl8168_mdio_write(tp, 0x1F, 0x0000);
	}

	if ((speed != SPEED_1000) && (speed != SPEED_100) &&
	    (speed != SPEED_10)) {
		speed = SPEED_100;
		duplex = DUPLEX_FULL;
	}

	giga_ctrl = rtl8168_mdio_read(tp, MII_CTRL1000);
	giga_ctrl &= ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);

	if (autoneg == AUTONEG_ENABLE) {
		/*n-way force*/
		auto_nego = rtl8168_mdio_read(tp, MII_ADVERTISE);
		auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
			       ADVERTISE_100HALF | ADVERTISE_100FULL |
			       ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

		if (adv & ADVERTISED_10baseT_Half)
			auto_nego |= ADVERTISE_10HALF;
		if (adv & ADVERTISED_10baseT_Full)
			auto_nego |= ADVERTISE_10FULL;
		if (adv & ADVERTISED_100baseT_Half)
			auto_nego |= ADVERTISE_100HALF;
		if (adv & ADVERTISED_100baseT_Full)
			auto_nego |= ADVERTISE_100FULL;
		if (adv & ADVERTISED_1000baseT_Half)
			giga_ctrl |= ADVERTISE_1000HALF;
		if (adv & ADVERTISED_1000baseT_Full)
			giga_ctrl |= ADVERTISE_1000FULL;

		//flow control
		if (dev->mtu <= ETH_DATA_LEN)
			auto_nego |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

		tp->phy_auto_nego_reg = auto_nego;
		tp->phy_1000_ctrl_reg = giga_ctrl;

		rtl8168_mdio_write(tp, 0x1f, 0x0000);
		rtl8168_mdio_write(tp, MII_ADVERTISE, auto_nego);
		rtl8168_mdio_write(tp, MII_CTRL1000, giga_ctrl);
		rtl8168_phy_restart_nway(dev);
		mdelay(20);
	} else {
		/*true force*/
		if (speed == SPEED_10 || speed == SPEED_100 ||
		    (speed == SPEED_1000 && duplex == DUPLEX_FULL &&
		     tp->HwSuppGigaForceMode)) {
			rtl8168_phy_setup_force_mode(dev, speed, duplex);
		} else
			goto out;
	}

	tp->autoneg = autoneg;
	tp->speed = speed;
	tp->duplex = duplex;
	tp->advertising = adv;

	if (tp->mcfg == CFG_METHOD_11)
		rtl8168dp_10mbps_gphy_para(dev);

	rc = 0;
out:
	return rc;
}

static int rtl8168_set_speed(struct net_device *dev, u8 autoneg, u32 speed,
			     u8 duplex, u32 adv)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int ret;

	ret = tp->set_speed(dev, autoneg, speed, duplex, adv);

	return ret;
}

static int rtl8168_set_settings(struct net_device *dev,
				const struct ethtool_link_ksettings *cmd)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int ret;
	unsigned long flags;
	u8 autoneg;
	u32 speed;
	u8 duplex;
	u32 supported, advertising;
	const struct ethtool_link_settings *base = &cmd->base;

	autoneg = base->autoneg;
	speed = base->speed;
	duplex = base->duplex;
	ethtool_convert_link_mode_to_legacy_u32(&supported,
						cmd->link_modes.supported);
	ethtool_convert_link_mode_to_legacy_u32(&advertising,
						cmd->link_modes.advertising);
	if (advertising & ~supported)
		return -EINVAL;

	spin_lock_irqsave(&tp->lock, flags);
	ret = rtl8168_set_speed(dev, autoneg, speed, duplex, advertising);
	spin_unlock_irqrestore(&tp->lock, flags);

	return ret;
}

#ifdef CONFIG_R8168_VLAN

static inline u32 rtl8168_tx_vlan_tag(struct rtl8168_private *tp,
				      struct sk_buff *skb)
{
	u32 tag;

	tag = (skb_vlan_tag_present(skb)) ?
		      TxVlanTag | swab16(skb_vlan_tag_get(skb)) :
		      0x00;

	return tag;
}

static int rtl8168_rx_vlan_skb(struct rtl8168_private *tp, struct RxDesc *desc,
			       struct sk_buff *skb)
{
	u32 opts2 = le32_to_cpu(desc->opts2);
	int ret = -1;

	if (opts2 & RxVlanTag)
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
				       swab16(opts2 & 0xffff));

	desc->opts2 = 0;
	return ret;
}

#else /* !CONFIG_R8168_VLAN */

static inline u32 rtl8168_tx_vlan_tag(struct rtl8168_private *tp,
				      struct sk_buff *skb)
{
	return 0;
}

static int rtl8168_rx_vlan_skb(struct rtl8168_private *tp, struct RxDesc *desc,
			       struct sk_buff *skb)
{
	return -1;
}

#endif

static netdev_features_t rtl8168_fix_features(struct net_device *dev,
					      netdev_features_t features)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	if (dev->mtu > MSS_MAX)
		features &= ~NETIF_F_ALL_TSO;
	if (dev->mtu > ETH_DATA_LEN) {
		features &= ~NETIF_F_ALL_TSO;
		features &= ~NETIF_F_ALL_CSUM;
	}
	spin_unlock_irqrestore(&tp->lock, flags);

	return features;
}

static int rtl8168_hw_set_features(struct net_device *dev,
				   netdev_features_t features)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u32 rx_config;

	rx_config = RTL_R32(tp, RxConfig);
	if (features & NETIF_F_RXALL)
		rx_config |= (AcceptErr | AcceptRunt);
	else
		rx_config &= ~(AcceptErr | AcceptRunt);

	RTL_W32(tp, RxConfig, rx_config);

	if (features & NETIF_F_RXCSUM)
		tp->cp_cmd |= RxChkSum;
	else
		tp->cp_cmd &= ~RxChkSum;

	if (dev->features & NETIF_F_HW_VLAN_RX)
		tp->cp_cmd |= RxVlan;
	else
		tp->cp_cmd &= ~RxVlan;

	RTL_W16(tp, CPlusCmd, tp->cp_cmd);
	RTL_R16(tp, CPlusCmd);

	return 0;
}

static int rtl8168_set_features(struct net_device *dev,
				netdev_features_t features)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	features &= NETIF_F_RXALL | NETIF_F_RXCSUM | NETIF_F_HW_VLAN_RX;

	spin_lock_irqsave(&tp->lock, flags);
	if (features ^ dev->features)
		rtl8168_hw_set_features(dev, features);
	spin_unlock_irqrestore(&tp->lock, flags);

	return 0;
}

static void rtl8168_gset_xmii(struct net_device *dev,
			      struct ethtool_link_ksettings *cmd)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u8 status;
	u8 autoneg, duplex;
	u32 speed = 0;
	u16 bmcr, bmsr, anlpar, ctrl1000 = 0, stat1000 = 0;
	u32 supported, advertising, lp_advertising;
	unsigned long flags;

	supported = SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full |
		    SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full |
		    SUPPORTED_1000baseT_Full | SUPPORTED_Autoneg |
		    SUPPORTED_TP | SUPPORTED_Pause | SUPPORTED_Asym_Pause;

	advertising = ADVERTISED_TP;

	spin_lock_irqsave(&tp->lock, flags);
	rtl8168_mdio_write(tp, 0x1F, 0x0000);
	bmcr = rtl8168_mdio_read(tp, MII_BMCR);
	bmsr = rtl8168_mdio_read(tp, MII_BMSR);
	anlpar = rtl8168_mdio_read(tp, MII_LPA);
	ctrl1000 = rtl8168_mdio_read(tp, MII_CTRL1000);
	stat1000 = rtl8168_mdio_read(tp, MII_STAT1000);
	spin_unlock_irqrestore(&tp->lock, flags);

	if (bmcr & BMCR_ANENABLE) {
		advertising |= ADVERTISED_Autoneg;
		autoneg = AUTONEG_ENABLE;

		if (bmsr & BMSR_ANEGCOMPLETE) {
			lp_advertising = mii_lpa_to_ethtool_lpa_t(anlpar);
			lp_advertising |=
				mii_stat1000_to_ethtool_lpa_t(stat1000);
		} else {
			lp_advertising = 0;
		}

		if (tp->phy_auto_nego_reg & ADVERTISE_10HALF)
			advertising |= ADVERTISED_10baseT_Half;
		if (tp->phy_auto_nego_reg & ADVERTISE_10FULL)
			advertising |= ADVERTISED_10baseT_Full;
		if (tp->phy_auto_nego_reg & ADVERTISE_100HALF)
			advertising |= ADVERTISED_100baseT_Half;
		if (tp->phy_auto_nego_reg & ADVERTISE_100FULL)
			advertising |= ADVERTISED_100baseT_Full;
		if (tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL)
			advertising |= ADVERTISED_1000baseT_Full;
	} else {
		autoneg = AUTONEG_DISABLE;
		lp_advertising = 0;
	}

	status = RTL_R8(tp, PHYstatus);

	if (status & LinkStatus) {
		/*link on*/
		if (status & _1000bpsF)
			speed = SPEED_1000;
		else if (status & _100bps)
			speed = SPEED_100;
		else if (status & _10bps)
			speed = SPEED_10;

		if (status & TxFlowCtrl)
			advertising |= ADVERTISED_Asym_Pause;

		if (status & RxFlowCtrl)
			advertising |= ADVERTISED_Pause;

		duplex = ((status & _1000bpsF) || (status & FullDup)) ?
				 DUPLEX_FULL :
				 DUPLEX_HALF;
	} else {
		/*link down*/
		speed = SPEED_UNKNOWN;
		duplex = DUPLEX_UNKNOWN;
	}

	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.supported,
						supported);
	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.advertising,
						advertising);
	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.lp_advertising,
						lp_advertising);
	cmd->base.autoneg = autoneg;
	cmd->base.speed = speed;
	cmd->base.duplex = duplex;
	cmd->base.port = PORT_TP;
}

static int rtl8168_get_settings(struct net_device *dev,
				struct ethtool_link_ksettings *cmd)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	tp->get_settings(dev, cmd);

	return 0;
}

static void rtl8168_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			     void *p)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int i;
	u8 *data = p;
	unsigned long flags;

	if (regs->len < R8168_REGS_DUMP_SIZE)
		return /* -EINVAL */;

	memset(p, 0, regs->len);

	spin_lock_irqsave(&tp->lock, flags);
	for (i = 0; i < R8168_MAC_REGS_SIZE; i++)
		*data++ = readb(ioaddr + i);
	data = (u8 *)p + 256;

	rtl8168_mdio_write(tp, 0x1F, 0x0000);
	for (i = 0; i < R8168_PHY_REGS_SIZE / 2; i++) {
		*(u16 *)data = rtl8168_mdio_read(tp, i);
		data += 2;
	}
	data = (u8 *)p + 256 * 2;

	for (i = 0; i < R8168_EPHY_REGS_SIZE / 2; i++) {
		*(u16 *)data = rtl8168_ephy_read(tp, i);
		data += 2;
	}
	data = (u8 *)p + 256 * 3;

	switch (tp->mcfg) {
	case CFG_METHOD_1:
	case CFG_METHOD_2:
	case CFG_METHOD_3:
		/* RTL8168B does not support Extend GMAC */
		break;
	default:
		for (i = 0; i < R8168_ERI_REGS_SIZE; i += 4) {
			*(u32 *)data = rtl8168_eri_read(tp, i, 4, ERIAR_ExGMAC);
			data += 4;
		}
		break;
	}
	spin_unlock_irqrestore(&tp->lock, flags);
}

static u32 rtl8168_get_msglevel(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	return tp->msg_enable;
}

static void rtl8168_set_msglevel(struct net_device *dev, u32 value)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	tp->msg_enable = value;
}

static const char rtl8168_gstrings[][ETH_GSTRING_LEN] = {
	"tx_packets",
	"rx_packets",
	"tx_errors",
	"rx_errors",
	"rx_missed",
	"align_errors",
	"tx_single_collisions",
	"tx_multi_collisions",
	"unicast",
	"broadcast",
	"multicast",
	"tx_aborted",
	"tx_underrun",
};

static int rtl8168_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(rtl8168_gstrings);
	default:
		return -EOPNOTSUPP;
	}
}

static void rtl8168_get_ethtool_stats(struct net_device *dev,
				      struct ethtool_stats *stats, u64 *data)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	struct rtl8168_counters *counters;
	dma_addr_t paddr;
	u32 cmd;
	u32 WaitCnt;
	unsigned long flags;

	ASSERT_RTNL();

	counters = tp->tally_vaddr;
	paddr = tp->tally_paddr;
	if (!counters)
		return;

	spin_lock_irqsave(&tp->lock, flags);
	RTL_W32(tp, CounterAddrHigh, (u64)paddr >> 32);
	cmd = (u64)paddr & DMA_BIT_MASK(32);
	RTL_W32(tp, CounterAddrLow, cmd);
	RTL_W32(tp, CounterAddrLow, cmd | CounterDump);

	WaitCnt = 0;
	while (RTL_R32(tp, CounterAddrLow) & CounterDump) {
		udelay(10);

		WaitCnt++;
		if (WaitCnt > 20)
			break;
	}
	spin_unlock_irqrestore(&tp->lock, flags);

	data[0] = le64_to_cpu(counters->tx_packets);
	data[1] = le64_to_cpu(counters->rx_packets);
	data[2] = le64_to_cpu(counters->tx_errors);
	data[3] = le32_to_cpu(counters->rx_errors);
	data[4] = le16_to_cpu(counters->rx_missed);
	data[5] = le16_to_cpu(counters->align_errors);
	data[6] = le32_to_cpu(counters->tx_one_collision);
	data[7] = le32_to_cpu(counters->tx_multi_collision);
	data[8] = le64_to_cpu(counters->rx_unicast);
	data[9] = le64_to_cpu(counters->rx_broadcast);
	data[10] = le32_to_cpu(counters->rx_multicast);
	data[11] = le16_to_cpu(counters->tx_aborted);
	data[12] = le16_to_cpu(counters->tx_underun);
}

static void rtl8168_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(data, *rtl8168_gstrings, sizeof(rtl8168_gstrings));
		break;
	}
}

static int rtl_get_eeprom_len(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	return tp->eeprom_len;
}

#undef ethtool_op_get_link
#define ethtool_op_get_link _kc_ethtool_op_get_link
static u32 _kc_ethtool_op_get_link(struct net_device *dev)
{
	return netif_carrier_ok(dev) ? 1 : 0;
}

u16 mmd_read(struct rtl8168_private *tp, u16 devad, u16 addr)
{
	u16 date, reg13dat;

	reg13dat = rtl8168_mdio_read(tp, 0x0D);
	reg13dat &= 0x3FFF;
	reg13dat = (reg13dat & 0xFFE0) | devad;
	rtl8168_mdio_write(tp, 0x0D, reg13dat);
	rtl8168_mdio_write(tp, 0x0E, addr);
	reg13dat = (reg13dat & 0x3FFF) | 0x4000;
	rtl8168_mdio_write(tp, 0x0D, reg13dat);
	date = rtl8168_mdio_read(tp, 0x0E);

	return date;
}

void mmd_write(struct rtl8168_private *tp, u16 devad, u16 addr, u16 date)
{
	u16 reg13dat;

	reg13dat = rtl8168_mdio_read(tp, 0x0D);
	reg13dat &= 0x3FFF;
	reg13dat = (reg13dat & 0xFFE0) | devad;
	rtl8168_mdio_write(tp, 0x0D, reg13dat);
	rtl8168_mdio_write(tp, 0x0E, addr);
	reg13dat = (reg13dat & 0x3FFF) | 0x4000;
	rtl8168_mdio_write(tp, 0x0D, reg13dat);
	rtl8168_mdio_write(tp, 0x0E, date);
}

/*
static int rtl8168_enable_external_EEE(struct rtl8168_private *tp)
{
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	rtl8168_mdio_write(tp, 0x1f, 0x0A5D);
	rtl8168_mdio_write(tp, 0x10, rtl8168_mdio_read(tp, 0x10) | 0x2);
	rtl8168_mdio_write(tp, 0x1f, 0x0A43);
	rtl8168_mdio_write(tp, 0x19, rtl8168_mdio_read(tp, 0x19) | 0x10);
	rtl8168_mdio_write(tp, 0x1f, 0x0);
	rtl8168_mdio_write(tp, 0x0, rtl8168_mdio_read(tp, 0x0) | 0x8000);
	spin_unlock_irqrestore(&tp->lock, flags);
	return 0;
}
*/

static int rtl8168_enable_EEE(struct rtl8168_private *tp)
{
	int ret;
	u16 data;

	ret = 0;
	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		/****
		csi_tmp = rtl8168_eri_read(tp, 0x1B0, 4, ERIAR_ExGMAC);
		csi_tmp |= BIT_1 | BIT_0;
		rtl8168_eri_write(tp, 0x1B0, 4, csi_tmp, ERIAR_ExGMAC);
		rtl8168_mdio_write(tp, 0x1F, 0x0A43);
		data = rtl8168_mdio_read(tp, 0x11);
		rtl8168_mdio_write(tp, 0x11, data | BIT_4);
		rtl8168_mdio_write(tp, 0x1F, 0x0A5D);
		rtl8168_mdio_write(tp, 0x10, tp->eee_adv_t);
		rtl8168_mdio_write(tp, 0x1F, 0x0000);
		****/
		data = rtl8168_eri_read(tp, 0x1B0, 4, ERIAR_ExGMAC);
		data |= BIT_1 | BIT_0;
		rtl8168_eri_write(tp, 0x1B0, 4, data, ERIAR_ExGMAC);
		data = mmd_read(tp, 7, 60);
		data |= BIT_1;
		mmd_write(tp, 7, 60, data);
		break;

	default:
		ret = -EOPNOTSUPP;
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_set_phy_mcu_patch_request(tp);
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_25:
		rtl8168_eri_write(tp, 0x1EA, 1, 0xFA, ERIAR_ExGMAC);

		rtl8168_mdio_write(tp, 0x1F, 0x0A43);
		data = rtl8168_mdio_read(tp, 0x10);
		if (data & BIT_10) {
			rtl8168_mdio_write(tp, 0x1F, 0x0A42);
			data = rtl8168_mdio_read(tp, 0x16);
			data &= ~(BIT_1);
			rtl8168_mdio_write(tp, 0x16, data);
		} else {
			rtl8168_mdio_write(tp, 0x1F, 0x0A42);
			data = rtl8168_mdio_read(tp, 0x16);
			data |= BIT_1;
			rtl8168_mdio_write(tp, 0x16, data);
		}
		rtl8168_mdio_write(tp, 0x1F, 0x0000);
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_clear_phy_mcu_patch_request(tp);
		break;
	}

	return ret;
}
/*
static int rtl8168_disable_external_EEE(struct rtl8168_private *tp)
{
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	rtl8168_mdio_write(tp, 0x1f, 0x0A5D);
	rtl8168_mdio_write(tp, 0x10, rtl8168_mdio_read(tp, 0x10) & ~0x2);
	rtl8168_mdio_write(tp, 0x1f, 0x0A43);
	rtl8168_mdio_write(tp, 0x19, rtl8168_mdio_read(tp, 0x19) & ~0x10);
	rtl8168_mdio_write(tp, 0x1f, 0x0);
	rtl8168_mdio_write(tp, 0x0, rtl8168_mdio_read(tp, 0x0) | 0x8000);
	spin_unlock_irqrestore(&tp->lock, flags);
	return 0;
}
*/

static int rtl8168_disable_EEE(struct rtl8168_private *tp)
{
	int ret;
	u16 data;

	ret = 0;
	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		/****
		csi_tmp = rtl8168_eri_read(tp, 0x1B0, 4, ERIAR_ExGMAC);
		csi_tmp &= ~(BIT_1 | BIT_0);
		rtl8168_eri_write(tp, 0x1B0, 4, csi_tmp, ERIAR_ExGMAC);
		rtl8168_mdio_write(tp, 0x1F, 0x0A43);
		data = rtl8168_mdio_read(tp, 0x11);
		rtl8168_mdio_write(tp, 0x11, data & ~BIT_4);
		rtl8168_mdio_write(tp, 0x1F, 0x0A5D);
		rtl8168_mdio_write(tp, 0x10, 0x0000);
		rtl8168_mdio_write(tp, 0x1F, 0x0000);
		****/
		data = rtl8168_eri_read(tp, 0x1B0, 4, ERIAR_ExGMAC);
		data &= ~(BIT_1 | BIT_0);
		rtl8168_eri_write(tp, 0x1B0, 4, data, ERIAR_ExGMAC);
		data = mmd_read(tp, 7, 60);
		data &= ~BIT_1;
		mmd_write(tp, 7, 60, data);
		break;

	default:
		ret = -EOPNOTSUPP;
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_set_phy_mcu_patch_request(tp);
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_25:
		rtl8168_eri_write(tp, 0x1EA, 1, 0x00, ERIAR_ExGMAC);

		rtl8168_mdio_write(tp, 0x1F, 0x0A42);
		data = rtl8168_mdio_read(tp, 0x16);
		data &= ~(BIT_1);
		rtl8168_mdio_write(tp, 0x16, data);
		rtl8168_mdio_write(tp, 0x1F, 0x0000);
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_clear_phy_mcu_patch_request(tp);
		break;
	}

	return ret;
}

static int rtl_nway_reset(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;
	int ret, bmcr;

	spin_lock_irqsave(&tp->lock, flags);

	if (unlikely(tp->rtk_enable_diag)) {
		spin_unlock_irqrestore(&tp->lock, flags);
		return -EBUSY;
	}

	/* if autoneg is off, it's an error */
	rtl8168_mdio_write(tp, 0x1F, 0x0000);
	bmcr = rtl8168_mdio_read(tp, MII_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		bmcr |= BMCR_ANRESTART;
		rtl8168_mdio_write(tp, MII_BMCR, bmcr);
		ret = 0;
	} else {
		ret = -EINVAL;
	}

	spin_unlock_irqrestore(&tp->lock, flags);

	return ret;
}

static int rtl_ethtool_get_eee(struct net_device *net, struct ethtool_eee *eee)
{
	struct rtl8168_private *tp = netdev_priv(net);
	u32 lp, adv, supported = 0;
	unsigned long flags;
	u16 val;

	switch (tp->mcfg) {
	case CFG_METHOD_21 ... CFG_METHOD_33:
		break;
	default:
		return -EOPNOTSUPP;
	}

	spin_lock_irqsave(&tp->lock, flags);

	if (unlikely(tp->rtk_enable_diag)) {
		spin_unlock_irqrestore(&tp->lock, flags);
		return -EBUSY;
	}

	rtl8168_mdio_write(tp, 0x1F, 0x0A5C);
	val = rtl8168_mdio_read(tp, 0x12);
	supported = mmd_eee_cap_to_ethtool_sup_t(val);

	rtl8168_mdio_write(tp, 0x1F, 0x0A5D);
	val = rtl8168_mdio_read(tp, 0x10);
	adv = mmd_eee_adv_to_ethtool_adv_t(val);

	val = rtl8168_mdio_read(tp, 0x11);
	lp = mmd_eee_adv_to_ethtool_adv_t(val);

	val = rtl8168_eri_read(tp, 0x1B0, 2, ERIAR_ExGMAC);
	val &= BIT_1 | BIT_0;

	rtl8168_mdio_write(tp, 0x1F, 0x0000);

	spin_unlock_irqrestore(&tp->lock, flags);

	eee->eee_enabled = !!val;
	eee->eee_active = !!(supported & adv & lp);
	eee->supported = supported;
	eee->advertised = adv;
	eee->lp_advertised = lp;

	return 0;
}

static int rtl_ethtool_set_eee(struct net_device *net, struct ethtool_eee *eee)
{
	struct rtl8168_private *tp = netdev_priv(net);
	unsigned long flags;

	switch (tp->mcfg) {
	case CFG_METHOD_21 ... CFG_METHOD_33:
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (HW_SUPP_SERDES_PHY(tp) || !HW_HAS_WRITE_PHY_MCU_RAM_CODE(tp))
		return -EOPNOTSUPP;

	spin_lock_irqsave(&tp->lock, flags);

	if (unlikely(tp->rtk_enable_diag)) {
		spin_unlock_irqrestore(&tp->lock, flags);
		return -EBUSY;
	}

	tp->eee_enabled = eee->eee_enabled;
	tp->eee_adv_t = ethtool_adv_to_mmd_eee_adv_t(eee->advertised);

	if (tp->eee_enabled)
		rtl8168_enable_EEE(tp);
	else
		rtl8168_disable_EEE(tp);

	spin_unlock_irqrestore(&tp->lock, flags);

	rtl_nway_reset(net);

	return 0;
}

static const struct ethtool_ops rtl8168_ethtool_ops = {
	.get_drvinfo = rtl8168_get_drvinfo,
	.get_regs_len = rtl8168_get_regs_len,
	.get_link = ethtool_op_get_link,
	.get_link_ksettings = rtl8168_get_settings,
	.set_link_ksettings = rtl8168_set_settings,
	.get_msglevel = rtl8168_get_msglevel,
	.set_msglevel = rtl8168_set_msglevel,
	.get_regs = rtl8168_get_regs,
	.get_wol = rtl8168_get_wol,
	.set_wol = rtl8168_set_wol,
	.get_strings = rtl8168_get_strings,
	.get_sset_count = rtl8168_get_sset_count,
	.get_ethtool_stats = rtl8168_get_ethtool_stats,
	.get_eeprom_len = rtl_get_eeprom_len,
	.get_ts_info = ethtool_op_get_ts_info,
	.get_eee = rtl_ethtool_get_eee,
	.set_eee = rtl_ethtool_set_eee,
	.nway_reset = rtl_nway_reset,
};

static void rtl8168_get_mac_version(struct rtl8168_private *tp)
{
	u32 reg, val32;
	u32 ICVerID;

	val32 = RTL_R32(tp, TxConfig);
	reg = val32 & 0x7c800000;
	ICVerID = val32 & 0x00700000;

	switch (reg) {
	case 0x50800000:
		if (ICVerID == 0x00000000) {
			tp->mcfg = CFG_METHOD_24;
		} else if (ICVerID == 0x00100000) {
			tp->mcfg = CFG_METHOD_25;
		} else {
			tp->mcfg = CFG_METHOD_25;
			tp->HwIcVerUnknown = TRUE;
		}
		tp->efuse_ver = EFUSE_SUPPORT_V3;
		break;
	default:
		printk("unknown chip version (%x)\n", reg);
		tp->mcfg = CFG_METHOD_DEFAULT;
		printk(KERN_INFO "++mac version method default+\n");
		tp->HwIcVerUnknown = TRUE;
		tp->efuse_ver = EFUSE_NOT_SUPPORT;
		break;
	}
}

static void rtl8168_print_mac_version(struct rtl8168_private *tp)
{
	int i;

	for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
		if (tp->mcfg == rtl_chip_info[i].mcfg) {
			dprintk("Realtek PCIe GbE Family Controller mcfg = %04d\n",
				rtl_chip_info[i].mcfg);
			return;
		}
	}

	dprintk("mac_version == Unknown\n");
}

static void rtl8168_tally_counter_addr_fill(struct rtl8168_private *tp)
{
	if (!tp->tally_paddr)
		return;

	RTL_W32(tp, CounterAddrHigh, (u64)tp->tally_paddr >> 32);
	RTL_W32(tp, CounterAddrLow, (u64)tp->tally_paddr & (DMA_BIT_MASK(32)));
}

static void rtl8168_tally_counter_clear(struct rtl8168_private *tp)
{
	if (tp->mcfg == CFG_METHOD_1 || tp->mcfg == CFG_METHOD_2 ||
	    tp->mcfg == CFG_METHOD_3)
		return;

	if (!tp->tally_paddr)
		return;

	RTL_W32(tp, CounterAddrHigh, (u64)tp->tally_paddr >> 32);
	RTL_W32(tp, CounterAddrLow,
		((u64)tp->tally_paddr & (DMA_BIT_MASK(32))) | CounterReset);
}

void rtl8168_enable_now_is_oob(struct rtl8168_private *tp)
{
	if (tp->HwSuppNowIsOobVer == 1)
		RTL_W8(tp, MCUCmd_reg, RTL_R8(tp, MCUCmd_reg) | Now_is_oob);
}

void rtl8168_disable_now_is_oob(struct rtl8168_private *tp)
{
	if (tp->HwSuppNowIsOobVer == 1)
		RTL_W8(tp, MCUCmd_reg, RTL_R8(tp, MCUCmd_reg) & ~Now_is_oob);
}

static void rtl8168_switch_to_sgmii_mode(struct rtl8168_private *tp)
{
	if (HW_SUPP_SERDES_PHY(tp) == FALSE)
		return;

	switch (tp->HwSuppSerDesPhyVer) {
	case 1:
		rtl8168_mac_ocp_write(tp, 0xEB00, 0x2);
		rtl8168_set_mcu_ocp_bit(tp, 0xEB16, BIT_1);
		break;
	}
}

static void rtl8168_exit_oob(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u16 data16;

	RTL_W32(tp, RxConfig,
		RTL_R32(tp, RxConfig) &
			~(AcceptErr | AcceptRunt | AcceptBroadcast |
			  AcceptMulticast | AcceptMyPhys | AcceptAllPhys));

	if (HW_SUPP_SERDES_PHY(tp))
		if (tp->HwSuppSerDesPhyVer == 1)
			rtl8168_switch_to_sgmii_mode(tp);

	if (HW_DASH_SUPPORT_DASH(tp)) {
		rtl8168_driver_start(tp);
#ifdef ENABLE_DASH_SUPPORT
		DashHwInit(dev);
#endif
	}

#ifdef ENABLE_REALWOW_SUPPORT
	rtl8168_realwow_hw_init(dev);
#else
	switch (tp->mcfg) {
	case CFG_METHOD_25:
		rtl8168_eri_write(tp, 0x174, 2, 0x00FF, ERIAR_ExGMAC);
		rtl8168_mac_ocp_write(tp, 0xE428, 0x0010);
		break;
	}
#endif //ENABLE_REALWOW_SUPPORT

	rtl8168_nic_reset(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_disable_now_is_oob(tp);

		data16 = rtl8168_mac_ocp_read(tp, 0xE8DE) & ~BIT_14;
		rtl8168_mac_ocp_write(tp, 0xE8DE, data16);
		rtl8168_wait_ll_share_fifo_ready(dev);

		data16 = rtl8168_mac_ocp_read(tp, 0xE8DE) | BIT_15;
		rtl8168_mac_ocp_write(tp, 0xE8DE, data16);

		rtl8168_wait_ll_share_fifo_ready(dev);
		break;
	}

	//wait ups resume (phy state 2)
#ifdef ENABLE_FIBER_SUPPORT
	if (HW_FIBER_MODE_ENABLED(tp))
		rtl8168_hw_init_fiber_nic(dev);
#endif //ENABLE_FIBER_SUPPORT

	tp->phy_reg_anlpar = 0;
}

void rtl8168_hw_disable_mac_mcu_bps(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_enable_cfg9346_write(tp);
		RTL_W8(tp, Config5, RTL_R8(tp, Config5) & ~BIT_0);
		RTL_W8(tp, Config2, RTL_R8(tp, Config2) & ~BIT_7);
		rtl8168_disable_cfg9346_write(tp);
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_mac_ocp_write(tp, 0xFC28, 0x0000);
		rtl8168_mac_ocp_write(tp, 0xFC2A, 0x0000);
		rtl8168_mac_ocp_write(tp, 0xFC2C, 0x0000);
		rtl8168_mac_ocp_write(tp, 0xFC2E, 0x0000);
		rtl8168_mac_ocp_write(tp, 0xFC30, 0x0000);
		rtl8168_mac_ocp_write(tp, 0xFC32, 0x0000);
		rtl8168_mac_ocp_write(tp, 0xFC34, 0x0000);
		rtl8168_mac_ocp_write(tp, 0xFC36, 0x0000);
		mdelay(3);
		rtl8168_mac_ocp_write(tp, 0xFC26, 0x0000);
		break;
	}
}

static void rtl8168_hw_init(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		rtl8168_enable_cfg9346_write(tp);
		RTL_W8(tp, Config5, RTL_R8(tp, Config5) & ~BIT_0);
		RTL_W8(tp, Config2, RTL_R8(tp, Config2) & ~BIT_7);
		rtl8168_disable_cfg9346_write(tp);
		RTL_W8(tp, 0xF1, RTL_R8(tp, 0xF1) & ~BIT_7);
		break;
	}

	if (s0_magic_packet == 1)
		rtl8168_enable_magic_packet(dev);
}

static void rtl8168_hw_ephy_config(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u16 ephy_data;

	switch (tp->mcfg) {
	case CFG_METHOD_25:
		ephy_data = rtl8168_ephy_read(tp, 0x00);
		ephy_data &= ~BIT_3;
		rtl8168_ephy_write(tp, 0x00, ephy_data);
		ephy_data = rtl8168_ephy_read(tp, 0x0C);
		ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_9 |
			       BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
		ephy_data |= (BIT_5 | BIT_11);
		rtl8168_ephy_write(tp, 0x0C, ephy_data);

		rtl8168_ephy_write(tp, 0x19, 0x7C00);
		rtl8168_ephy_write(tp, 0x1E, 0x20EB);
		rtl8168_ephy_write(tp, 0x0D, 0x1666);
		rtl8168_ephy_write(tp, 0x00, 0x10A3);
		rtl8168_ephy_write(tp, 0x06, 0xF050);
		break;
	}
}

static int rtl8168_set_phy_mcu_patch_request(struct rtl8168_private *tp)
{
	u16 PhyRegValue;
	u32 WaitCnt;
	int retval = TRUE;

	switch (tp->mcfg) {
	case CFG_METHOD_21 ... CFG_METHOD_33:
		rtl8168_mdio_write(tp, 0x1f, 0x0B82);
		rtl8168_set_eth_phy_bit(tp, 0x10, BIT_4);

		rtl8168_mdio_write(tp, 0x1f, 0x0B80);
		WaitCnt = 0;
		do {
			PhyRegValue = rtl8168_mdio_read(tp, 0x10);
			PhyRegValue &= 0x0040;
			udelay(100);
			WaitCnt++;
		} while (PhyRegValue != 0x0040 && WaitCnt < 1000);

		if (WaitCnt == 1000)
			retval = FALSE;

		rtl8168_mdio_write(tp, 0x1f, 0x0000);
		break;
	}

	return retval;
}

static int rtl8168_clear_phy_mcu_patch_request(struct rtl8168_private *tp)
{
	u16 PhyRegValue;
	u32 WaitCnt;
	int retval = TRUE;

	switch (tp->mcfg) {
	case CFG_METHOD_21 ... CFG_METHOD_33:
		rtl8168_mdio_write(tp, 0x1f, 0x0B82);
		rtl8168_clear_eth_phy_bit(tp, 0x10, BIT_4);

		rtl8168_mdio_write(tp, 0x1f, 0x0A22);
		WaitCnt = 0;
		do {
			PhyRegValue = rtl8168_mdio_read(tp, 0x12);
			PhyRegValue &= 0x0010;
			udelay(100);
			WaitCnt++;
		} while (PhyRegValue != 0x0010 && WaitCnt < 1000);

		if (WaitCnt == 1000)
			retval = FALSE;

		rtl8168_mdio_write(tp, 0x1f, 0x0000);
		break;
	}

	return retval;
}

static void rtl8168_enable_phy_aldps(struct rtl8168_private *tp)
{
	//enable aldps
	//GPHY OCP 0xA430 bit[2] = 0x1 (en_aldps)
	rtl8168_mdio_write(tp, 0x1F, 0x0A43);
	rtl8168_mdio_write(tp, 0x10, rtl8168_mdio_read(tp, 0x10) | BIT_2);
}

static void rtl8168_hw_phy_config(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	tp->phy_reset_enable(dev);

	if (HW_DASH_SUPPORT_TYPE_3(tp) && tp->HwPkgDet == 0x06)
		return;

	tp->HwHasWrRamCodeToMicroP = TRUE;

	if (dev && dev->dev.parent) {
		if (of_device_is_compatible(dev->dev.parent->of_node,
					    "realtek,rts493xa-r8168")) {
			rtl8168_mdio_write(tp, 0x1F, 0xa42);
			rtl8168_mdio_write(
				tp, 0x12, (rtl8168_mdio_read(tp, 0x12) | 0x10));
		}
	}

	if (aspm) {
		if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(tp))
			rtl8168_enable_phy_aldps(tp);
	}

	rtl8168_mdio_write(tp, 0x1F, 0x0000);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(tp)) {
		if (tp->eee_enabled)
			rtl8168_enable_EEE(tp);
		else
			rtl8168_disable_EEE(tp);
	}
}

static inline void rtl8168_delete_link_timer(struct net_device *dev,
					     struct timer_list *timer)
{
	del_timer_sync(timer);
}

static inline void rtl8168_request_link_timer(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->link_timer;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	setup_timer(timer, rtl8168_link_timer, (unsigned long)dev);
#else
	timer_setup(timer, rtl8168_link_timer, 0);
#endif
	mod_timer(timer, jiffies + RTL8168_LINK_TIMEOUT);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void rtl8168_netpoll(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	disable_irq(dev->irq);
	rtl8168_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

static void rtl8168_init_software_variable(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	if (HW_SUPP_SERDES_PHY(tp))
		eee_enable = 0;

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		tp->HwSuppNowIsOobVer = 1;
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		tp->HwSuppPhyOcpVer = 1;
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
		tp->HwPcieSNOffset = 0x16C;
		break;
	case CFG_METHOD_DEFAULT:
		tp->HwPcieSNOffset = 0;
		break;
	default:
		tp->HwPcieSNOffset = 0x164;
		break;
	}

#ifdef ENABLE_REALWOW_SUPPORT
	rtl8168_get_realwow_hw_version(dev);
#endif //ENABLE_REALWOW_SUPPORT

	if (HW_DASH_SUPPORT_DASH(tp) && rtl8168_check_dash(tp))
		tp->DASH = 1;
	else
		tp->DASH = 0;

	if (tp->DASH) {
		if (HW_DASH_SUPPORT_TYPE_3(tp)) {
			void __iomem *cmac_ioaddr = NULL;

			if (cmac_ioaddr == NULL)
				tp->DASH = 0;
			else
				tp->mapped_cmac_ioaddr = cmac_ioaddr;
		}
	}

#ifdef ENABLE_DASH_SUPPORT
#ifdef ENABLE_DASH_PRINTER_SUPPORT
	if (tp->DASH) {
		if (HW_DASH_SUPPORT_TYPE_3(tp) && tp->HwPkgDet == 0x0F)
			tp->dash_printer_enabled = 1;
		else if (HW_DASH_SUPPORT_TYPE_2(tp))
			tp->dash_printer_enabled = 1;
	}
#endif //ENABLE_DASH_PRINTER_SUPPORT
#endif //ENABLE_DASH_SUPPORT

	if (HW_DASH_SUPPORT_TYPE_2(tp))
		tp->cmac_ioaddr = tp->mmio_addr;
	else if (HW_DASH_SUPPORT_TYPE_3(tp))
		tp->cmac_ioaddr = tp->mapped_cmac_ioaddr;

	switch (tp->mcfg) {
	case CFG_METHOD_1:
		tp->intr_mask = RxDescUnavail | TxDescUnavail | TxOK | RxOK |
				SWInt;
		tp->timer_intr_mask = PCSTimeout;
		break;
	default:
		tp->intr_mask = RxDescUnavail | TxOK | RxOK | SWInt;
		tp->timer_intr_mask = PCSTimeout;
		break;
	}

#ifdef ENABLE_DASH_SUPPORT
	if (tp->DASH) {
		if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
			tp->timer_intr_mask |= (ISRIMR_DASH_INTR_EN |
						ISRIMR_DASH_INTR_CMAC_RESET);
			tp->intr_mask |= (ISRIMR_DASH_INTR_EN |
					  ISRIMR_DASH_INTR_CMAC_RESET);
		} else {
			tp->timer_intr_mask |=
				(ISRIMR_DP_DASH_OK | ISRIMR_DP_HOST_OK |
				 ISRIMR_DP_REQSYS_OK);
			tp->intr_mask |=
				(ISRIMR_DP_DASH_OK | ISRIMR_DP_HOST_OK |
				 ISRIMR_DP_REQSYS_OK);
		}
	}
#endif

	if (timer_count == 0 || tp->mcfg == CFG_METHOD_DEFAULT)
		tp->use_timer_interrrupt = FALSE;

#ifdef ENABLE_FIBER_SUPPORT
	rtl8168_check_hw_fiber_mode_support(dev);
#endif //ENABLE_FIBER_SUPPORT

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		tp->HwSuppMagicPktVer = WAKEUP_MAGIC_PACKET_V2;
		break;
	case CFG_METHOD_DEFAULT:
		tp->HwSuppMagicPktVer = WAKEUP_MAGIC_PACKET_NOT_SUPPORT;
		break;
	default:
		tp->HwSuppMagicPktVer = WAKEUP_MAGIC_PACKET_V1;
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		tp->HwSuppCheckPhyDisableModeVer = 2;
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		tp->HwSuppGigaForceMode = TRUE;
		break;
	}

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_24;
		break;
	}

	if (tp->HwIcVerUnknown) {
		tp->NotWrRamCodeToMicroP = TRUE;
		tp->NotWrMcuPatchCode = TRUE;
	}

	tp->NicCustLedValue = RTL_R16(tp, CustomLED);

	rtl8168_get_hw_wol(dev);

	rtl8168_link_option((u8 *)&autoneg_mode, (u32 *)&speed_mode,
			    (u8 *)&duplex_mode, (u32 *)&advertising_mode);

	tp->autoneg = autoneg_mode;
	tp->speed = speed_mode;
	tp->duplex = duplex_mode;
	tp->advertising = advertising_mode;

	tp->max_jumbo_frame_size = rtl_chip_info[tp->chipset].jumbo_frame_sz;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	/* MTU range: 60 - hw-specific max */
	dev->min_mtu = ETH_ZLEN;
	dev->max_mtu = tp->max_jumbo_frame_size;
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
	tp->eee_enabled = eee_enable;
	tp->eee_adv_t = MDIO_EEE_1000T | MDIO_EEE_100TX;

#ifdef ENABLE_FIBER_SUPPORT
	if (HW_FIBER_MODE_ENABLED(tp))
		rtl8168_set_fiber_mode_software_variable(dev);
#endif //ENABLE_FIBER_SUPPORT
}

static void rtl8168_release_board(struct platform_device *pdev,
				  struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	rtl8168_rar_set(tp, tp->org_mac_addr);
	tp->wol_enabled = WOL_DISABLED;

	if (!tp->DASH)
		rtl8168_phy_power_down(dev);

#ifdef ENABLE_DASH_SUPPORT
	if (tp->DASH)
		FreeAllocatedDashShareMemory(dev);
#endif

	if (tp->mapped_cmac_ioaddr != NULL)
		iounmap(tp->mapped_cmac_ioaddr);

	iounmap(ioaddr);
	release_mem_region(tp->addr_res->start, resource_size(tp->addr_res));
	free_netdev(dev);
}
/*
static int
rtl8168_get_mac_address(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int i;
	u8 mac_addr[MAC_ADDR_LEN];

	for (i = 0; i < MAC_ADDR_LEN; i++)
		mac_addr[i] = RTL_R8(tp, MAC0 + i);

	if (!is_valid_ether_addr(mac_addr)) {
		netif_err(tp, probe, dev, "Invalid ether addr %pM\n",
				  mac_addr);
		eth_hw_addr_random(dev);
		ether_addr_copy(mac_addr, dev->dev_addr);
		netif_info(tp, probe, dev, "Random ether addr %pM\n",
				   mac_addr);
		tp->random_mac = 1;
	}

	rtl8168_rar_set(tp, mac_addr);

	for (i = 0; i < MAC_ADDR_LEN; i++) {
		dev->dev_addr[i] = RTL_R8(tp, MAC0 + i);
		tp->org_mac_addr[i] = dev->dev_addr[i];
	}
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	return 0;
}
*/

/**
 * rtl8168_set_mac_address - Change the Ethernet Address of the NIC
 * @dev: network interface device structure
 * @p:	 pointer to an address structure
 *
 * Return 0 on success, negative on failure
 **/
static int rtl8168_set_mac_address(struct net_device *dev, void *p)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	struct sockaddr *addr = p;
	unsigned long flags;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	spin_lock_irqsave(&tp->lock, flags);

	eth_hw_addr_set(dev, addr->sa_data);

	rtl8168_rar_set(tp, dev->dev_addr);

	spin_unlock_irqrestore(&tp->lock, flags);

	return 0;
}

/******************************************************************************
 * rtl8168_rar_set - Puts an ethernet address into a receive address register.
 *
 * tp - The private data structure for driver
 * addr - Address to put into receive address register
 *****************************************************************************/
void rtl8168_rar_set(struct rtl8168_private *tp, uint8_t *addr)
{
	uint32_t rar_low = 0;
	uint32_t rar_high = 0;

	rar_low = ((uint32_t)addr[0] | ((uint32_t)addr[1] << 8) |
		   ((uint32_t)addr[2] << 16) | ((uint32_t)addr[3] << 24));

	rar_high = ((uint32_t)addr[4] | ((uint32_t)addr[5] << 8));

	rtl8168_enable_cfg9346_write(tp);
	RTL_W32(tp, MAC0, rar_low);
	RTL_W32(tp, MAC4, rar_high);

	switch (tp->mcfg) {
	case CFG_METHOD_14:
	case CFG_METHOD_15:
		RTL_W32(tp, SecMAC0, rar_low);
		RTL_W16(tp, SecMAC4, (uint16_t)rar_high);
		break;
	}

	if (tp->mcfg == CFG_METHOD_17) {
		rtl8168_eri_write(tp, 0xf0, 4, rar_low << 16, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xf4, 4, rar_low >> 16 | rar_high << 16,
				  ERIAR_ExGMAC);
	}

	rtl8168_disable_cfg9346_write(tp);
}

#ifdef ETHTOOL_OPS_COMPAT
static int ethtool_get_settings(struct net_device *dev, void *useraddr)
{
	struct ethtool_cmd cmd = { ETHTOOL_GSET };
	int err;

	if (!ethtool_ops->get_settings)
		return -EOPNOTSUPP;

	err = ethtool_ops->get_settings(dev, &cmd);
	if (err < 0)
		return err;

	if (copy_to_user(useraddr, &cmd, sizeof(cmd)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_settings(struct net_device *dev, void *useraddr)
{
	struct ethtool_cmd cmd;

	if (!ethtool_ops->set_settings)
		return -EOPNOTSUPP;

	if (copy_from_user(&cmd, useraddr, sizeof(cmd)))
		return -EFAULT;

	return ethtool_ops->set_settings(dev, &cmd);
}

static int ethtool_get_drvinfo(struct net_device *dev, void *useraddr)
{
	struct ethtool_drvinfo info;
	struct ethtool_ops *ops = ethtool_ops;

	if (!ops->get_drvinfo)
		return -EOPNOTSUPP;

	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
	ops->get_drvinfo(dev, &info);

	if (ops->self_test_count)
		info.testinfo_len = ops->self_test_count(dev);
	if (ops->get_stats_count)
		info.n_stats = ops->get_stats_count(dev);
	if (ops->get_regs_len)
		info.regdump_len = ops->get_regs_len(dev);
	if (ops->get_eeprom_len)
		info.eedump_len = ops->get_eeprom_len(dev);

	if (copy_to_user(useraddr, &info, sizeof(info)))
		return -EFAULT;
	return 0;
}

static int ethtool_get_regs(struct net_device *dev, char *useraddr)
{
	struct ethtool_regs regs;
	struct ethtool_ops *ops = ethtool_ops;
	void *regbuf;
	int reglen, ret;

	if (!ops->get_regs || !ops->get_regs_len)
		return -EOPNOTSUPP;

	if (copy_from_user(&regs, useraddr, sizeof(regs)))
		return -EFAULT;

	reglen = ops->get_regs_len(dev);
	if (regs.len > reglen)
		regs.len = reglen;

	regbuf = kmalloc(reglen, GFP_USER);
	if (!regbuf)
		return -ENOMEM;

	ops->get_regs(dev, &regs, regbuf);

	ret = -EFAULT;
	if (copy_to_user(useraddr, &regs, sizeof(regs)))
		goto out;
	useraddr += offsetof(struct ethtool_regs, data);
	if (copy_to_user(useraddr, regbuf, reglen))
		goto out;
	ret = 0;

out:
	kfree(regbuf);
	return ret;
}

static int ethtool_get_wol(struct net_device *dev, char *useraddr)
{
	struct ethtool_wolinfo wol = { ETHTOOL_GWOL };

	if (!ethtool_ops->get_wol)
		return -EOPNOTSUPP;

	ethtool_ops->get_wol(dev, &wol);

	if (copy_to_user(useraddr, &wol, sizeof(wol)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_wol(struct net_device *dev, char *useraddr)
{
	struct ethtool_wolinfo wol;

	if (!ethtool_ops->set_wol)
		return -EOPNOTSUPP;

	if (copy_from_user(&wol, useraddr, sizeof(wol)))
		return -EFAULT;

	return ethtool_ops->set_wol(dev, &wol);
}

static int ethtool_get_msglevel(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GMSGLVL };

	if (!ethtool_ops->get_msglevel)
		return -EOPNOTSUPP;

	edata.data = ethtool_ops->get_msglevel(dev);

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_msglevel(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata;

	if (!ethtool_ops->set_msglevel)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	ethtool_ops->set_msglevel(dev, edata.data);
	return 0;
}

static int ethtool_nway_reset(struct net_device *dev)
{
	if (!ethtool_ops->nway_reset)
		return -EOPNOTSUPP;

	return ethtool_ops->nway_reset(dev);
}

static int ethtool_get_link(struct net_device *dev, void *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GLINK };

	if (!ethtool_ops->get_link)
		return -EOPNOTSUPP;

	edata.data = ethtool_ops->get_link(dev);

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_get_eeprom(struct net_device *dev, void *useraddr)
{
	struct ethtool_eeprom eeprom;
	struct ethtool_ops *ops = ethtool_ops;
	u8 *data;
	int ret;

	if (!ops->get_eeprom || !ops->get_eeprom_len)
		return -EOPNOTSUPP;

	if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
		return -EFAULT;

	/* Check for wrap and zero */
	if (eeprom.offset + eeprom.len <= eeprom.offset)
		return -EINVAL;

	/* Check for exceeding total eeprom len */
	if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
		return -EINVAL;

	data = kmalloc(eeprom.len, GFP_USER);
	if (!data)
		return -ENOMEM;

	ret = -EFAULT;
	if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
		goto out;

	ret = ops->get_eeprom(dev, &eeprom, data);
	if (ret)
		goto out;

	ret = -EFAULT;
	if (copy_to_user(useraddr, &eeprom, sizeof(eeprom)))
		goto out;
	if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
		goto out;
	ret = 0;

out:
	kfree(data);
	return ret;
}

static int ethtool_set_eeprom(struct net_device *dev, void *useraddr)
{
	struct ethtool_eeprom eeprom;
	struct ethtool_ops *ops = ethtool_ops;
	u8 *data;
	int ret;

	if (!ops->set_eeprom || !ops->get_eeprom_len)
		return -EOPNOTSUPP;

	if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
		return -EFAULT;

	/* Check for wrap and zero */
	if (eeprom.offset + eeprom.len <= eeprom.offset)
		return -EINVAL;

	/* Check for exceeding total eeprom len */
	if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
		return -EINVAL;

	data = kmalloc(eeprom.len, GFP_USER);
	if (!data)
		return -ENOMEM;

	ret = -EFAULT;
	if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
		goto out;

	ret = ops->set_eeprom(dev, &eeprom, data);
	if (ret)
		goto out;

	if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
		ret = -EFAULT;

out:
	kfree(data);
	return ret;
}

static int ethtool_get_coalesce(struct net_device *dev, void *useraddr)
{
	struct ethtool_coalesce coalesce = { ETHTOOL_GCOALESCE };

	if (!ethtool_ops->get_coalesce)
		return -EOPNOTSUPP;

	ethtool_ops->get_coalesce(dev, &coalesce);

	if (copy_to_user(useraddr, &coalesce, sizeof(coalesce)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_coalesce(struct net_device *dev, void *useraddr)
{
	struct ethtool_coalesce coalesce;

	if (!ethtool_ops->get_coalesce)
		return -EOPNOTSUPP;

	if (copy_from_user(&coalesce, useraddr, sizeof(coalesce)))
		return -EFAULT;

	return ethtool_ops->set_coalesce(dev, &coalesce);
}

static int ethtool_get_ringparam(struct net_device *dev, void *useraddr)
{
	struct ethtool_ringparam ringparam = { ETHTOOL_GRINGPARAM };

	if (!ethtool_ops->get_ringparam)
		return -EOPNOTSUPP;

	ethtool_ops->get_ringparam(dev, &ringparam);

	if (copy_to_user(useraddr, &ringparam, sizeof(ringparam)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_ringparam(struct net_device *dev, void *useraddr)
{
	struct ethtool_ringparam ringparam;

	if (!ethtool_ops->get_ringparam)
		return -EOPNOTSUPP;

	if (copy_from_user(&ringparam, useraddr, sizeof(ringparam)))
		return -EFAULT;

	return ethtool_ops->set_ringparam(dev, &ringparam);
}

static int ethtool_get_pauseparam(struct net_device *dev, void *useraddr)
{
	struct ethtool_pauseparam pauseparam = { ETHTOOL_GPAUSEPARAM };

	if (!ethtool_ops->get_pauseparam)
		return -EOPNOTSUPP;

	ethtool_ops->get_pauseparam(dev, &pauseparam);

	if (copy_to_user(useraddr, &pauseparam, sizeof(pauseparam)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_pauseparam(struct net_device *dev, void *useraddr)
{
	struct ethtool_pauseparam pauseparam;

	if (!ethtool_ops->get_pauseparam)
		return -EOPNOTSUPP;

	if (copy_from_user(&pauseparam, useraddr, sizeof(pauseparam)))
		return -EFAULT;

	return ethtool_ops->set_pauseparam(dev, &pauseparam);
}

static int ethtool_get_rx_csum(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GRXCSUM };

	if (!ethtool_ops->get_rx_csum)
		return -EOPNOTSUPP;

	edata.data = ethtool_ops->get_rx_csum(dev);

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_rx_csum(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata;

	if (!ethtool_ops->set_rx_csum)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	ethtool_ops->set_rx_csum(dev, edata.data);
	return 0;
}

static int ethtool_get_tx_csum(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GTXCSUM };

	if (!ethtool_ops->get_tx_csum)
		return -EOPNOTSUPP;

	edata.data = ethtool_ops->get_tx_csum(dev);

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_tx_csum(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata;

	if (!ethtool_ops->set_tx_csum)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	return ethtool_ops->set_tx_csum(dev, edata.data);
}

static int ethtool_get_sg(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GSG };

	if (!ethtool_ops->get_sg)
		return -EOPNOTSUPP;

	edata.data = ethtool_ops->get_sg(dev);

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_sg(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata;

	if (!ethtool_ops->set_sg)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	return ethtool_ops->set_sg(dev, edata.data);
}

static int ethtool_get_tso(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata = { ETHTOOL_GTSO };

	if (!ethtool_ops->get_tso)
		return -EOPNOTSUPP;

	edata.data = ethtool_ops->get_tso(dev);

	if (copy_to_user(useraddr, &edata, sizeof(edata)))
		return -EFAULT;
	return 0;
}

static int ethtool_set_tso(struct net_device *dev, char *useraddr)
{
	struct ethtool_value edata;

	if (!ethtool_ops->set_tso)
		return -EOPNOTSUPP;

	if (copy_from_user(&edata, useraddr, sizeof(edata)))
		return -EFAULT;

	return ethtool_ops->set_tso(dev, edata.data);
}

static int ethtool_self_test(struct net_device *dev, char *useraddr)
{
	struct ethtool_test test;
	struct ethtool_ops *ops = ethtool_ops;
	u64 *data;
	int ret;

	if (!ops->self_test || !ops->self_test_count)
		return -EOPNOTSUPP;

	if (copy_from_user(&test, useraddr, sizeof(test)))
		return -EFAULT;

	test.len = ops->self_test_count(dev);
	data = kmalloc(test.len * sizeof(u64), GFP_USER);
	if (!data)
		return -ENOMEM;

	ops->self_test(dev, &test, data);

	ret = -EFAULT;
	if (copy_to_user(useraddr, &test, sizeof(test)))
		goto out;
	useraddr += sizeof(test);
	if (copy_to_user(useraddr, data, test.len * sizeof(u64)))
		goto out;
	ret = 0;
out:
	kfree(data);
	return ret;
}

static int ethtool_get_strings(struct net_device *dev, void *useraddr)
{
	struct ethtool_gstrings gstrings;
	struct ethtool_ops *ops = ethtool_ops;
	u8 *data;
	int ret;

	if (!ops->get_strings)
		return -EOPNOTSUPP;

	if (copy_from_user(&gstrings, useraddr, sizeof(gstrings)))
		return -EFAULT;

	switch (gstrings.string_set) {
	case ETH_SS_TEST:
		if (!ops->self_test_count)
			return -EOPNOTSUPP;
		gstrings.len = ops->self_test_count(dev);
		break;
	case ETH_SS_STATS:
		if (!ops->get_stats_count)
			return -EOPNOTSUPP;
		gstrings.len = ops->get_stats_count(dev);
		break;
	default:
		return -EINVAL;
	}

	data = kmalloc(gstrings.len * ETH_GSTRING_LEN, GFP_USER);
	if (!data)
		return -ENOMEM;

	ops->get_strings(dev, gstrings.string_set, data);

	ret = -EFAULT;
	if (copy_to_user(useraddr, &gstrings, sizeof(gstrings)))
		goto out;
	useraddr += sizeof(gstrings);
	if (copy_to_user(useraddr, data, gstrings.len * ETH_GSTRING_LEN))
		goto out;
	ret = 0;
out:
	kfree(data);
	return ret;
}

static int ethtool_phys_id(struct net_device *dev, void *useraddr)
{
	struct ethtool_value id;

	if (!ethtool_ops->phys_id)
		return -EOPNOTSUPP;

	if (copy_from_user(&id, useraddr, sizeof(id)))
		return -EFAULT;

	return ethtool_ops->phys_id(dev, id.data);
}

static int ethtool_get_stats(struct net_device *dev, void *useraddr)
{
	struct ethtool_stats stats;
	struct ethtool_ops *ops = ethtool_ops;
	u64 *data;
	int ret;

	if (!ops->get_ethtool_stats || !ops->get_stats_count)
		return -EOPNOTSUPP;

	if (copy_from_user(&stats, useraddr, sizeof(stats)))
		return -EFAULT;

	stats.n_stats = ops->get_stats_count(dev);
	data = kmalloc(stats.n_stats * sizeof(u64), GFP_USER);
	if (!data)
		return -ENOMEM;

	ops->get_ethtool_stats(dev, &stats, data);

	ret = -EFAULT;
	if (copy_to_user(useraddr, &stats, sizeof(stats)))
		goto out;
	useraddr += sizeof(stats);
	if (copy_to_user(useraddr, data, stats.n_stats * sizeof(u64)))
		goto out;
	ret = 0;
out:
	kfree(data);
	return ret;
}

static int ethtool_ioctl(struct ifreq *ifr)
{
	struct net_device *dev = __dev_get_by_name(ifr->ifr_name);
	void *useraddr = (void *)ifr->ifr_data;
	u32 ethcmd;

	/*
	 * XXX: This can be pushed down into the ethtool_* handlers that
	 * need it.  Keep existing behaviour for the moment.
	 */
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!dev || !netif_device_present(dev))
		return -ENODEV;

	if (copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)))
		return -EFAULT;

	switch (ethcmd) {
	case ETHTOOL_GSET:
		return ethtool_get_settings(dev, useraddr);
	case ETHTOOL_SSET:
		return ethtool_set_settings(dev, useraddr);
	case ETHTOOL_GDRVINFO:
		return ethtool_get_drvinfo(dev, useraddr);
	case ETHTOOL_GREGS:
		return ethtool_get_regs(dev, useraddr);
	case ETHTOOL_GWOL:
		return ethtool_get_wol(dev, useraddr);
	case ETHTOOL_SWOL:
		return ethtool_set_wol(dev, useraddr);
	case ETHTOOL_GMSGLVL:
		return ethtool_get_msglevel(dev, useraddr);
	case ETHTOOL_SMSGLVL:
		return ethtool_set_msglevel(dev, useraddr);
	case ETHTOOL_NWAY_RST:
		return ethtool_nway_reset(dev);
	case ETHTOOL_GLINK:
		return ethtool_get_link(dev, useraddr);
	case ETHTOOL_GEEPROM:
		return ethtool_get_eeprom(dev, useraddr);
	case ETHTOOL_SEEPROM:
		return ethtool_set_eeprom(dev, useraddr);
	case ETHTOOL_GCOALESCE:
		return ethtool_get_coalesce(dev, useraddr);
	case ETHTOOL_SCOALESCE:
		return ethtool_set_coalesce(dev, useraddr);
	case ETHTOOL_GRINGPARAM:
		return ethtool_get_ringparam(dev, useraddr);
	case ETHTOOL_SRINGPARAM:
		return ethtool_set_ringparam(dev, useraddr);
	case ETHTOOL_GPAUSEPARAM:
		return ethtool_get_pauseparam(dev, useraddr);
	case ETHTOOL_SPAUSEPARAM:
		return ethtool_set_pauseparam(dev, useraddr);
	case ETHTOOL_GRXCSUM:
		return ethtool_get_rx_csum(dev, useraddr);
	case ETHTOOL_SRXCSUM:
		return ethtool_set_rx_csum(dev, useraddr);
	case ETHTOOL_GTXCSUM:
		return ethtool_get_tx_csum(dev, useraddr);
	case ETHTOOL_STXCSUM:
		return ethtool_set_tx_csum(dev, useraddr);
	case ETHTOOL_GSG:
		return ethtool_get_sg(dev, useraddr);
	case ETHTOOL_SSG:
		return ethtool_set_sg(dev, useraddr);
	case ETHTOOL_GTSO:
		return ethtool_get_tso(dev, useraddr);
	case ETHTOOL_STSO:
		return ethtool_set_tso(dev, useraddr);
	case ETHTOOL_TEST:
		return ethtool_self_test(dev, useraddr);
	case ETHTOOL_GSTRINGS:
		return ethtool_get_strings(dev, useraddr);
	case ETHTOOL_PHYS_ID:
		return ethtool_phys_id(dev, useraddr);
	case ETHTOOL_GSTATS:
		return ethtool_get_stats(dev, useraddr);
	default:
		return -EOPNOTSUPP;
	}

	return -EOPNOTSUPP;
}
#endif //ETHTOOL_OPS_COMPAT

static int rtl8168_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	struct mii_ioctl_data *data = if_mii(ifr);
	int ret;
	unsigned long flags;

	ret = 0;
	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = 32; /* Internal PHY */
		break;
	case SIOCGMIIREG:
		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_mdio_write(tp, 0x1F, 0x0000);
		data->val_out = rtl8168_mdio_read(tp, data->reg_num);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;
	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_mdio_write(tp, 0x1F, 0x0000);
		rtl8168_mdio_write(tp, data->reg_num, data->val_in);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;
#ifdef ETHTOOL_OPS_COMPAT
	case SIOCETHTOOL:
		ret = ethtool_ioctl(ifr);
		break;
#endif
	case SIOCDEVPRIVATE_RTLASF:
		if (!netif_running(dev)) {
			ret = -ENODEV;
			break;
		}

		ret = rtl8168_asf_ioctl(dev, ifr);
		break;
#ifdef ENABLE_DASH_SUPPORT
	case SIOCDEVPRIVATE_RTLDASH:
		if (!netif_running(dev)) {
			ret = -ENODEV;
			break;
		}
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}

		ret = rtl8168_dash_ioctl(dev, ifr);
		break;
#endif

#ifdef ENABLE_REALWOW_SUPPORT
	case SIOCDEVPRIVATE_RTLREALWOW:
		if (!netif_running(dev)) {
			ret = -ENODEV;
			break;
		}

		ret = rtl8168_realwow_ioctl(dev, ifr);
		break;
#endif

	case SIOCRTLTOOL:
		ret = rtl8168_tool_ioctl(tp, ifr);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}

static void rtl8168_phy_power_up(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	if (rtl8168_is_in_phy_disable_mode(dev))
		return;

	rtl8168_mdio_write(tp, 0x1F, 0x0000);
	rtl8168_mdio_write(tp, MII_BMCR, BMCR_ANENABLE);
}

static void rtl8168_phy_power_down(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_24:
		rtl8168_mdio_write(tp, MII_BMCR, BMCR_ANENABLE | BMCR_PDOWN);
		break;
	default:
		rtl8168_mdio_write(tp, MII_BMCR, BMCR_PDOWN);
		break;
	}
}

#define rtd_outl(addr, val) ((*(volatile u32 *)(addr)) = (u32)val)
static int __devinit rtl8168_init_board(struct platform_device *pdev,
					struct net_device **dev_out,
					void __iomem **ioaddr_out)
{
	void __iomem *ioaddr = NULL;
	void __iomem *eth_misc_addr = NULL;
	struct net_device *dev;
	struct rtl8168_private *tp;
	int rc = -ENOMEM, i;
	struct clk *clk;
	int irq_res = 0;
	struct resource *addr_res = NULL, *eth_misc_res = NULL;
	struct reset_control *eth_reset, *fephy_reset, *eth_mem_up;

	assert(ioaddr_out != NULL);

	/* dev zeroed in alloc_etherdev */
	dev = alloc_etherdev(sizeof(*tp));
	if (dev == NULL) {
		if (netif_msg_drv(&debug))
			dev_err(&pdev->dev, "unable to alloc new ethernet\n");
		goto err_out;
	}

	SET_MODULE_OWNER(dev);
	SET_NETDEV_DEV(dev, &pdev->dev);
	tp = netdev_priv(dev);
	tp->dev = dev;
	tp->msg_enable = netif_msg_init(debug.msg_enable, R8168_MSG_DEFAULT);

	if (of_device_is_compatible(dev->dev.parent->of_node,
				    "realtek,rts493xa-r8168")) {
		eth_misc_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (eth_misc_res == NULL) {
			dev_err(&pdev->dev, "insufficient resources\n");
			dev_err(&pdev->dev, "eth_misc_reses\n");
			rc = -2;
			goto err_out_free;
		}
		if (resource_size(eth_misc_res) < R8168_REGS_SIZE) {
			dev_err(&pdev->dev, "MMIO Resource too small\n");
			rc = -3;
			goto err_out_free;
		}
		if (!request_mem_region(eth_misc_res->start,
					resource_size(eth_misc_res),
					MODULENAME)) {
			dev_err(&pdev->dev, "cannot claim address reg area\n");
			rc = -6;
			goto err_out_free;
		}

		eth_misc_addr =
			ioremap(eth_misc_res->start, R8168_PHY_REGS_SIZE);
		if (eth_misc_addr == NULL) {
			if (netif_msg_probe(tp))
				dev_err(&pdev->dev,
					"cannot remap MMIO, aborting\n");
			rc = -7;
			goto err_out_free;
		}
	}

	addr_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	//	irq_res = platform_get_irq(pdev, IORESOURCE_IRQ, 0);
	irq_res = platform_get_irq_optional(pdev, 0);
	if (addr_res == NULL) {
		dev_err(&pdev->dev, "insufficient resources\n");
		dev_err(&pdev->dev, "addr _misc_reses\n");
		rc = -2;
		goto err_out_free;
	}

	if (irq_res == 0) {
		dev_err(&pdev->dev, "insufficient resources\n");
		dev_err(&pdev->dev, "irq_misc_reses\n");
		rc = -2;
		goto err_out_free;
	}

	if (resource_size(addr_res) < R8168_REGS_SIZE) {
		dev_err(&pdev->dev, "MMIO Resource too small\n");
		rc = -3;
		goto err_out_free;
	}

	if (!request_mem_region(addr_res->start, resource_size(addr_res),
				MODULENAME)) {
		dev_err(&pdev->dev, "cannot claim address reg area\n");
		rc = -4;
		goto err_out_free;
	}

	tp->addr_res = addr_res;
	tp->irq = irq_res;

	/* ioremap MMIO region */
	ioaddr = ioremap(addr_res->start, R8168_REGS_SIZE);
	if (ioaddr == NULL) {
		if (netif_msg_probe(tp))
			dev_err(&pdev->dev, "cannot remap MMIO, aborting\n");
		rc = -5;
		goto err_out_res;
	}
	tp->mmio_addr = ioaddr;
	eth_mem_up = devm_reset_control_get(&pdev->dev, "eth-mem-up");
	if (IS_ERR(eth_mem_up)) {
		if (PTR_ERR(eth_mem_up) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "No eth-mem-up found\n");
		rc = -61;
		goto err_out_res;
	}

	eth_reset = devm_reset_control_get(&pdev->dev, "eth-reset");
	if (IS_ERR(eth_reset)) {
		if (PTR_ERR(eth_reset) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "No eth-reset found\n");
		rc = -62;
		goto err_out_res;
	}

	/* Deassert eth-mem-up */
	reset_control_deassert(eth_mem_up);
	mdelay(5);

	/* Assert eth-reset reset mac hold */
	rc = reset_control_assert(eth_reset);
	if (rc) {
		dev_err(&pdev->dev, "assert eth-reset fail\n");
		rc = -63;
		goto err_out_res;
	}
	msleep(30);

	if (of_device_is_compatible(dev->dev.parent->of_node,
				    "realtek,rts493xa-r8168")) {
		/* get fephy reset control */
		fephy_reset = devm_reset_control_get(&pdev->dev, "fephy-reset");
		if (IS_ERR(fephy_reset)) {
			if (PTR_ERR(fephy_reset) != -EPROBE_DEFER)
				dev_err(&pdev->dev, "No fephy-reset found\n");
			rc = -6;
			goto err_out_res;
		}

		/* enable ldo fephy */
		ETH_MISC_W32(LDO_FEPHY_CTRL, LDO_FEPHY_REG);
		ETH_MISC_W32(LDO_FEPHY_CFG, LDO_FEPHY_POW);
		ETH_MISC_W32(ETH_CLK_CFG, POWER_STATE);
		ETH_MISC_W32(FEPHY_CFG, POR_CEN_L);

		/* Assert fephy-reset reset fephy */
		rc = reset_control_assert(fephy_reset);
		if (rc) {
			dev_err(&pdev->dev, "assert fephy_reset fail\n");
			rc = -7;
			goto err_out_res;
		}
		msleep(30);

		/* Deassert fephy-reset release fephy */
		rc = reset_control_deassert(fephy_reset);
		if (rc) {
			dev_err(&pdev->dev, "deassert fephy_reset fail\n");
			rc = -7;
			goto err_out_res;
		}
		msleep(30);
	}

	tp->eth_reset = eth_reset;
	tp->fephy_reset = fephy_reset;
	tp->eth_mem_up = eth_mem_up;

	/* open tx phase on FPGA */
	if (of_machine_is_compatible("realtek,rts_fpga")) {
		u32 tmp1;

		tmp1 = ETH_MISC_R32(ETH_MISC_CFG2) & 0xFFFF;
		ETH_MISC_W32(ETH_MISC_CFG2, tmp1 | TX_PHASE_SEL);
	}

	/* get eth clock */
	clk = clk_get(NULL, "ethernet_ck");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "ethernet_ck get failed!\n");
		rc = -8;
		goto err_out_res;
	}

	if (clk_prepare(clk)) {
		dev_err(&pdev->dev, "failed to prepare clock!\n");
		rc = -8;
		goto err_out_res;
	}
	msleep(30);

	/* enable mac clock */
	if (clk_enable(clk)) {
		dev_err(&pdev->dev, "failed to enable clock!\n");
		rc = -8;
		goto err_out_res;
	}
	msleep(30);

	/* Deassert eth-reset release mac */
	rc = reset_control_deassert(eth_reset);
	if (rc) {
		dev_err(&pdev->dev, "deassert eth_reset fail\n");
		rc = -7;
		goto err_out_res;
	}
	msleep(30);

	/* Identify chip attached to board */
	rtl8168_get_mac_version(tp);
	rtl8168_print_mac_version(tp);

	for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
		if (tp->mcfg == rtl_chip_info[i].mcfg)
			break;
	}

	if (i < 0) {
		/* Unknown chip: assume array element #0, original RTL-8168 */
		if (netif_msg_probe(tp))
			dev_printk(KERN_DEBUG, &pdev->dev,
				   "unknown chip version, assuming %s\n",
				   rtl_chip_info[0].name);
		i++;
	}

	tp->chipset = i;
	tp->irq = irq_res;

	*ioaddr_out = ioaddr;
	*dev_out = dev;
out:
	if (of_device_is_compatible(dev->dev.parent->of_node,
				    "realtek,rts493xa-r8168")) {
		if (eth_misc_addr)
			iounmap(eth_misc_addr);
		if (eth_misc_res)
			release_mem_region(eth_misc_res->start,
					   resource_size(eth_misc_res));
	}

	return rc;

err_out_res:
	if (ioaddr)
		iounmap(ioaddr);
	if (addr_res)
		release_mem_region(addr_res->start, resource_size(addr_res));
err_out_free:
	free_netdev(dev);
err_out:
	*ioaddr_out = NULL;
	*dev_out = NULL;
	goto out;
}

static void
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
rtl8168_link_timer(unsigned long __opaque)
#else
rtl8168_link_timer(struct timer_list *t)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	struct net_device *dev = (struct net_device *)__opaque;
	struct rtl8168_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->link_timer;
#else
	struct rtl8168_private *tp = from_timer(tp, t, link_timer);
	struct net_device *dev = tp->dev;
	struct timer_list *timer = t;
#endif
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	rtl8168_check_link_status(dev);
	spin_unlock_irqrestore(&tp->lock, flags);

	mod_timer(timer, jiffies + RTL8168_LINK_TIMEOUT);
}

static const struct net_device_ops rtl8168_netdev_ops = {
	.ndo_open = rtl8168_open,
	.ndo_stop = rtl8168_close,
	.ndo_get_stats = rtl8168_get_stats,
	.ndo_start_xmit = rtl8168_start_xmit,
	.ndo_tx_timeout = rtl8168_tx_timeout,
	.ndo_change_mtu = rtl8168_change_mtu,
	.ndo_set_mac_address = rtl8168_set_mac_address,
	.ndo_do_ioctl = rtl8168_do_ioctl,
	.ndo_set_rx_mode = rtl8168_set_rx_mode,
	.ndo_fix_features = rtl8168_fix_features,
	.ndo_set_features = rtl8168_set_features,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = rtl8168_netpoll,
#endif
};
/*
static int rtl8168_init_mac_address(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;
	int addr[ETH_ALEN];
	uint8_t addrt[ETH_ALEN];
	char *rtl8168_mac_str;
	int i;

	rtl8168_mac_str = fw_getenv("ethaddr");
	if (!rtl8168_mac_str)
		return -EINVAL;

	if (sscanf(rtl8168_mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
		&addr[0], &addr[1], &addr[2],
		&addr[3], &addr[4], &addr[5]) != ETH_ALEN)
		return -EINVAL;

	for (i = 0; i < ETH_ALEN; i++)
		addrt[i] = addr[i];

	if (!is_valid_ether_addr(addrt))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addrt, ETH_ALEN);

	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_rar_set(tp, dev->dev_addr);

	spin_unlock_irqrestore(&tp->lock, flags);

	return 0;
}
*/

static int __devinit rtl8168_init_one(struct platform_device *pdev)
{
	struct net_device *dev = NULL;
	struct rtl8168_private *tp;
	void __iomem *ioaddr = NULL;
	static int board_idx = -1;
	int irq_res = NULL;
	static struct pinctrl *eth_pinctrl;
	struct pinctrl_state *default_state = NULL;

	int rc;

	assert(pdev != NULL);
	assert(ent != NULL);

	board_idx++;

	if (netif_msg_drv(&debug))
		pr_info(KERN_INFO "%s Gigabit Ethernet driver %s loaded\n",
			MODULENAME, RTL8168_VERSION);

	rc = rtl8168_init_board(pdev, &dev, &ioaddr);
	if (rc) {
		dev_err(&pdev->dev, "init board failed, rc=%d\n", rc);
		return rc;
	}

	tp = netdev_priv(dev);
	assert(ioaddr != NULL);

	tp->set_speed = rtl8168_set_speed_xmii;
	tp->get_settings = rtl8168_gset_xmii;
	tp->phy_reset_enable = rtl8168_xmii_reset_enable;
	tp->phy_reset_pending = rtl8168_xmii_reset_pending;
	tp->link_ok = rtl8168_xmii_link_ok;

	RTL_NET_DEVICE_OPS(rtl8168_netdev_ops);

	SET_ETHTOOL_OPS(dev, &rtl8168_ethtool_ops);

	dev->watchdog_timeo = RTL8168_TX_TIMEOUT;
	dev->base_addr = (unsigned long)ioaddr;
	//dev->irq = irq_res->start;
	dev->irq = tp->irq;

#ifdef CONFIG_R8168_NAPI
	//	RTL_NAPI_CONFIG(dev, tp, rtl8168_poll, R8168_NAPI_WEIGHT);
	RTL_NAPI_CONFIG(dev, tp, rtl8168_poll);
#endif

#ifdef CONFIG_R8168_VLAN
	if (tp->mcfg != CFG_METHOD_DEFAULT)
		dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
#endif

	tp->cp_cmd |= RTL_R16(tp, CPlusCmd);
	if (tp->mcfg != CFG_METHOD_DEFAULT) {
		dev->features |= NETIF_F_IP_CSUM;
		dev->features |= NETIF_F_RXCSUM | NETIF_F_SG;
		dev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM |
				   NETIF_F_RXCSUM | NETIF_F_HW_VLAN_TX |
				   NETIF_F_HW_VLAN_RX;
		dev->vlan_features = NETIF_F_SG | NETIF_F_IP_CSUM |
				     NETIF_F_HIGHDMA;
		if ((tp->mcfg != CFG_METHOD_16) &&
		    (tp->mcfg != CFG_METHOD_17)) {
			dev->features |= NETIF_F_TSO;
			dev->hw_features |= NETIF_F_TSO;
			dev->vlan_features |= NETIF_F_TSO;
		}
		dev->priv_flags |= IFF_LIVE_ADDR_CHANGE;
		dev->hw_features |= NETIF_F_RXALL;
		dev->hw_features |= NETIF_F_RXFCS;
		if ((tp->mcfg == CFG_METHOD_1) || (tp->mcfg == CFG_METHOD_2) ||
		    (tp->mcfg == CFG_METHOD_3)) {
			dev->hw_features &= ~NETIF_F_IPV6_CSUM;
			netif_set_tso_max_size(dev, LSO_32K);
			dev->gso_max_segs = NIC_MAX_PHYS_BUF_COUNT_LSO_64K;
		} else {
			dev->hw_features |= NETIF_F_IPV6_CSUM;
			dev->features |= NETIF_F_IPV6_CSUM;
			if ((tp->mcfg != CFG_METHOD_16) &&
			    (tp->mcfg != CFG_METHOD_17)) {
				dev->hw_features |= NETIF_F_TSO6;
				dev->features |= NETIF_F_TSO6;
			}
			netif_set_tso_max_size(dev, LSO_64K);
			dev->gso_max_segs = NIC_MAX_PHYS_BUF_COUNT_LSO2;
		}
	}

	tp->platform_dev = pdev;

	spin_lock_init(&tp->lock);

	rtl8168_init_software_variable(dev);

#ifdef ENABLE_DASH_SUPPORT
	if (tp->DASH)
		AllocateDashShareMemory(dev);
#endif

	rtl8168_exit_oob(dev);

	rtl8168_hw_init(dev);

	rtl8168_hw_reset(dev);

	rtl8168_calibration_setting(dev);

	/* Get production from EEPROM */
	if (((tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
	      tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_29 ||
	      tp->mcfg == CFG_METHOD_30) &&
	     (rtl8168_mac_ocp_read(tp, 0xDC00) & BIT_3)) ||
	    ((tp->mcfg == CFG_METHOD_26) &&
	     (rtl8168_mac_ocp_read(tp, 0xDC00) & BIT_4)))
		tp->eeprom_type = EEPROM_TYPE_NONE;
	else
		rtl8168_eeprom_type(tp);

	if (tp->eeprom_type == EEPROM_TYPE_93C46 ||
	    tp->eeprom_type == EEPROM_TYPE_93C56)
		rtl8168_set_eeprom_sel_low(tp);

		//rtl8168_init_mac_address(dev);
		//rtl8168_get_mac_address(dev);

#if defined(ENABLE_DASH_PRINTER_SUPPORT)
	init_completion(&tp->fw_host_ok);
	init_completion(&tp->fw_ack);
	init_completion(&tp->fw_req);
#endif

	tp->tally_vaddr = dma_alloc_coherent(&pdev->dev,
					     sizeof(*tp->tally_vaddr),
					     &tp->tally_paddr, GFP_KERNEL);
	if (!tp->tally_vaddr) {
		rc = -ENOMEM;
		goto err_out;
	}

	rtl8168_tally_counter_clear(tp);

	platform_set_drvdata(pdev, dev);
	if (netif_msg_probe(tp)) {
		pr_info(KERN_INFO "%s: 0x%lx, "
				  "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
				  "IRQ %d\n",
			dev->name, dev->base_addr, dev->dev_addr[0],
			dev->dev_addr[1], dev->dev_addr[2], dev->dev_addr[3],
			dev->dev_addr[4], dev->dev_addr[5], dev->irq);
	}

	rc = register_netdev(dev);
	if (rc)
		goto err_out;

	rtl8168_disable_rxdvgate(dev);

	eth_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(eth_pinctrl)) {
		dev_warn(
			&pdev->dev,
			"eth_pinctrl is not enabled in dts. You can refer to the doc to enable it if needed.\n");
	} else {
		default_state = pinctrl_lookup_state(eth_pinctrl,
						     PINCTRL_STATE_DEFAULT);
		if (IS_ERR(default_state)) {
			dev_warn(&pdev->dev,
				 "have not set default pinctrl state\n");
		} else {
			rc = pinctrl_select_state(eth_pinctrl, default_state);
			if (rc)
				dev_warn(&pdev->dev,
					 "could not set default pins\n");
		}
	}

	netif_carrier_off(dev);

	printk("%s", GPL_CLAIM);

out:
	return rc;

err_out:
	if (tp->tally_vaddr != NULL) {
		dma_free_coherent(&pdev->dev, sizeof(*tp->tally_vaddr),
				  tp->tally_vaddr, tp->tally_paddr);

		tp->tally_vaddr = NULL;
	}
#ifdef CONFIG_R8168_NAPI
	RTL_NAPI_DEL(tp);
#endif
	rtl8168_release_board(pdev, dev);

	goto out;
}

static int __devexit rtl8168_remove_one(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct rtl8168_private *tp = netdev_priv(dev);

	assert(dev != NULL);
	assert(tp != NULL);

#ifdef CONFIG_R8168_NAPI
	RTL_NAPI_DEL(tp);
#endif
	if (HW_DASH_SUPPORT_DASH(tp))
		rtl8168_driver_stop(tp);

	unregister_netdev(dev);
#ifdef ENABLE_R8168_PROCFS
	rtl8168_proc_remove(dev);
#endif
	if (tp->tally_vaddr != NULL) {
		dma_free_coherent(&pdev->dev, sizeof(*tp->tally_vaddr),
				  tp->tally_vaddr, tp->tally_paddr);
		tp->tally_vaddr = NULL;
	}

	rtl8168_release_board(pdev, dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void rtl8168_set_rxbufsize(struct rtl8168_private *tp,
				  struct net_device *dev)
{
	unsigned int mtu = dev->mtu;

	tp->rx_buf_sz = (mtu > ETH_DATA_LEN) ? mtu + ETH_HLEN + 8 + 1 :
					       RX_BUF_SIZE;
}

static int rtl8168_open(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;
	int retval;

	retval = -ENOMEM;

#ifdef ENABLE_R8168_PROCFS
	rtl8168_proc_init(dev);
#endif
	rtl8168_set_rxbufsize(tp, dev);
	/*
	 * Rx and Tx descriptors needs 256 bytes alignment.
	 * pci_alloc_consistent provides more.
	 */
	tp->TxDescArray = dma_alloc_coherent(&tp->platform_dev->dev,
					     R8168_TX_RING_BYTES,
					     &tp->TxPhyAddr, GFP_KERNEL);
	if (!tp->TxDescArray)
		goto err_free_all_allocated_mem;

	tp->RxDescArray = dma_alloc_coherent(&tp->platform_dev->dev,
					     R8168_RX_RING_BYTES,
					     &tp->RxPhyAddr, GFP_KERNEL);
	if (!tp->RxDescArray)
		goto err_free_all_allocated_mem;

	if (tp->UseSwPaddingShortPkt) {
		tp->ShortPacketEmptyBuffer = dma_alloc_coherent(
			&tp->platform_dev->dev, SHORT_PACKET_PADDING_BUF_SIZE,
			&tp->ShortPacketEmptyBufferPhy, GFP_KERNEL);
		if (!tp->ShortPacketEmptyBuffer)
			goto err_free_all_allocated_mem;

		memset(tp->ShortPacketEmptyBuffer, 0x0,
		       SHORT_PACKET_PADDING_BUF_SIZE);
	}

	retval = rtl8168_init_ring(dev);
	if (retval < 0)
		goto err_free_all_allocated_mem;

	if (netif_msg_probe(tp)) {
		printk(KERN_INFO "%s: 0x%lx, "
				 "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
				 "IRQ %d\n",
		       dev->name, dev->base_addr, dev->dev_addr[0],
		       dev->dev_addr[1], dev->dev_addr[2], dev->dev_addr[3],
		       dev->dev_addr[4], dev->dev_addr[5], dev->irq);
	}

	INIT_DELAYED_WORK(&tp->task, NULL);

#ifdef CONFIG_R8168_NAPI
	RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_exit_oob(dev);

	rtl8168_hw_init(dev);

	rtl8168_hw_reset(dev);

	rtl8168_powerup_pll(dev);

	rtl8168_hw_ephy_config(dev);

	rtl8168_hw_phy_config(dev);

	rtl8168_hw_config(dev);

	rtl8168_dsm(dev, DSM_IF_UP);

	rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex,
			  tp->advertising);

	spin_unlock_irqrestore(&tp->lock, flags);

	retval = request_irq(dev->irq, rtl8168_interrupt,
			     (tp->features & RTL_FEATURE_MSI) ? 0 : SA_SHIRQ,
			     dev->name, dev);
	if (retval < 0)
		goto err_free_all_allocated_mem;

	rtl8168_request_link_timer(dev);
out:
	return retval;
err_free_all_allocated_mem:
	if (tp->RxDescArray != NULL) {
		dma_free_coherent(&tp->platform_dev->dev, R8168_RX_RING_BYTES,
				  tp->RxDescArray, tp->RxPhyAddr);
		tp->RxDescArray = NULL;
	}

	if (tp->TxDescArray != NULL) {
		dma_free_coherent(&tp->platform_dev->dev, R8168_TX_RING_BYTES,
				  tp->TxDescArray, tp->TxPhyAddr);
		tp->TxDescArray = NULL;
	}

	if (tp->ShortPacketEmptyBuffer != NULL) {
		dma_free_coherent(&tp->platform_dev->dev, ETH_ZLEN,
				  tp->ShortPacketEmptyBuffer,
				  tp->ShortPacketEmptyBufferPhy);
		tp->ShortPacketEmptyBuffer = NULL;
	}

	goto out;
}

static void rtl8168_dsm(struct net_device *dev, int dev_state)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	switch (dev_state) {
	case DSM_MAC_INIT:
		if ((tp->mcfg == CFG_METHOD_5) || (tp->mcfg == CFG_METHOD_6)) {
			if (RTL_R8(tp, MACDBG) & 0x80)
				RTL_W8(tp, GPIO, RTL_R8(tp, GPIO) | GPIO_en);
			else
				RTL_W8(tp, GPIO, RTL_R8(tp, GPIO) & ~GPIO_en);
		}

		break;
	case DSM_NIC_GOTO_D3:
	case DSM_IF_DOWN:
		if ((tp->mcfg == CFG_METHOD_5) || (tp->mcfg == CFG_METHOD_6)) {
			if (RTL_R8(tp, MACDBG) & 0x80)
				RTL_W8(tp, GPIO, RTL_R8(tp, GPIO) & ~GPIO_en);
		}
		break;

	case DSM_NIC_RESUME_D3:
	case DSM_IF_UP:
		if ((tp->mcfg == CFG_METHOD_5) || (tp->mcfg == CFG_METHOD_6)) {
			if (RTL_R8(tp, MACDBG) & 0x80)
				RTL_W8(tp, GPIO, RTL_R8(tp, GPIO) | GPIO_en);
		}

		break;
	}
}

static void rtl8168_hw_set_rx_packet_filter(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u32 mc_filter[2]; /* Multicast hash filter */
	int rx_mode;
	u32 tmp = 0;

	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		if (netif_msg_link(tp))
			printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n",
			       dev->name);

		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
			  AcceptAllPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else if ((netdev_mc_count(dev) > multicast_filter_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else {
		struct netdev_hw_addr *ha;

		rx_mode = AcceptBroadcast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0;
		netdev_for_each_mc_addr(ha, dev) {
			int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;

			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
			rx_mode |= AcceptMulticast;
		}
	}

	if (dev->features & NETIF_F_RXALL)
		rx_mode |= (AcceptErr | AcceptRunt);

	tmp = mc_filter[0];
	mc_filter[0] = swab32(mc_filter[1]);
	mc_filter[1] = swab32(tmp);

	tp->rtl8168_rx_config = rtl_chip_info[tp->chipset].RCR_Cfg;
	tmp = tp->rtl8168_rx_config | rx_mode |
	      (RTL_R32(tp, RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);

	RTL_W32(tp, RxConfig, tmp);
	RTL_W32(tp, MAR0 + 0, mc_filter[0]);
	RTL_W32(tp, MAR0 + 4, mc_filter[1]);
}

static void rtl8168_set_rx_mode(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_hw_set_rx_packet_filter(dev);

	spin_unlock_irqrestore(&tp->lock, flags);
}

static void rtl8168_hw_config(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u32 csi_tmp;

	RTL_W32(tp, RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

	rtl8168_hw_reset(dev);

	rtl8168_enable_cfg9346_write(tp);
	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W8(tp, 0xF1, RTL_R8(tp, 0xF1) & ~BIT_7);
		RTL_W8(tp, Config2, RTL_R8(tp, Config2) & ~BIT_7);
		RTL_W8(tp, Config5, RTL_R8(tp, Config5) & ~BIT_0);
		break;
	}

	//clear io_rdy_l23
	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		RTL_W8(tp, Config3, RTL_R8(tp, Config3) & ~BIT_1);
		break;
	}

	RTL_W8(tp, MTPS, Reserved1_data);

	tp->cp_cmd |= INTT_1;
	if (tp->use_timer_interrrupt)
		tp->cp_cmd |= PktCntrDisable;
	else
		tp->cp_cmd &= ~PktCntrDisable;

	RTL_W16(tp, IntrMitigate, 0x5f51);

	rtl8168_tally_counter_addr_fill(tp);

	rtl8168_desc_addr_fill(tp);

	/* Set DMA burst size and Interframe Gap Time */
	if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19) {
		rtl8168_eri_write(tp, 0xC8, 4, 0x00100002, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);
		RTL_W32(tp, TxConfig, RTL_R32(tp, TxConfig) | BIT_7);
		rtl8168_eri_write(tp, 0x1d0, 1, csi_tmp, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xCC, 4, 0x00000050, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xd0, 4, 0x00000060, ERIAR_ExGMAC);
	} else if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
		   tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25 ||
		   tp->mcfg == CFG_METHOD_30) {
		/* config phy */
		rtl8168_mdio_write(tp, 0x1f, 0);
		rtl8168_mdio_read(tp, 1);

		csi_tmp = rtl8168_mdio_read(tp, 1);

		RTL_W8(tp, Config0, 0);
		RTL_W8(tp, Config1, 0);
		RTL_W8(tp, Config2, 0x3d);
		RTL_W8(tp, Config3, 0x26);
		RTL_W8(tp, Config4, 0);
		RTL_W8(tp, Config5, 0x02);

		RTL_W8(tp, TDFNR, 0x4);

		tp->cp_cmd = 0x0c61;

		/* end config phy */
		rtl8168_eri_write(tp, 0xC8, 4, 0x00080002, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xCC, 1, 0x38, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xD0, 1, 0x48, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);

		RTL_W32(tp, TxConfig, RTL_R32(tp, TxConfig) | BIT_7);

		csi_tmp = rtl8168_eri_read(tp, 0xDC, 1, ERIAR_ExGMAC);
		csi_tmp &= ~BIT_0;
		rtl8168_eri_write(tp, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
		csi_tmp |= BIT_0;
		rtl8168_eri_write(tp, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

		RTL_W8(tp, Config3, RTL_R8(tp, Config3) & ~Beacon_en);

		RTL_W8(tp, 0x1B, RTL_R8(tp, 0x1B) & ~0x07);

		RTL_W8(tp, TDFNR, 0x4);

		RTL_W8(tp, Config2, RTL_R8(tp, Config2) & ~PMSTS_En);

		if (aspm)
			RTL_W8(tp, 0xF1, RTL_R8(tp, 0xF1) | BIT_7);

		if (dev->mtu > ETH_DATA_LEN)
			RTL_W8(tp, MTPS, 0x27);

		RTL_W8(tp, 0xD0, RTL_R8(tp, 0xD0) | BIT_6);
		RTL_W8(tp, 0xF2, RTL_R8(tp, 0xF2) | BIT_6);

		RTL_W8(tp, 0xD0, RTL_R8(tp, 0xD0) | BIT_7);

		rtl8168_eri_write(tp, 0xC0, 2, 0x0000, ERIAR_ExGMAC);
		rtl8168_eri_write(tp, 0xB8, 4, 0x00000000, ERIAR_ExGMAC);

		rtl8168_eri_write(tp, 0x5F0, 2, 0x4F87, ERIAR_ExGMAC);

		if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
			csi_tmp = rtl8168_eri_read(tp, 0xD4, 4, ERIAR_ExGMAC);
			csi_tmp |= (BIT_8 | BIT_9 | BIT_10 | BIT_11 | BIT_12);
			rtl8168_eri_write(tp, 0xD4, 4, csi_tmp, ERIAR_ExGMAC);

			csi_tmp = rtl8168_eri_read(tp, 0xDC, 4, ERIAR_ExGMAC);
			csi_tmp |= (BIT_2 | BIT_3 | BIT_4);
			rtl8168_eri_write(tp, 0xDC, 4, csi_tmp, ERIAR_ExGMAC);
		} else {
			csi_tmp = rtl8168_eri_read(tp, 0xD4, 4, ERIAR_ExGMAC);
			csi_tmp |= (BIT_7 | BIT_8 | BIT_9 | BIT_10 | BIT_11 |
				    BIT_12);
			rtl8168_eri_write(tp, 0xD4, 4, csi_tmp, ERIAR_ExGMAC);
		}

		if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
		    tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25) {
			rtl8168_mac_ocp_write(tp, 0xC140, 0xFFFF);
		} else if (tp->mcfg == CFG_METHOD_29 ||
			   tp->mcfg == CFG_METHOD_30) {
			rtl8168_mac_ocp_write(tp, 0xC140, 0xFFFF);
			rtl8168_mac_ocp_write(tp, 0xC142, 0xFFFF);
		}

		csi_tmp = rtl8168_eri_read(tp, 0x1B0, 4, ERIAR_ExGMAC);
		csi_tmp &= ~BIT_12;
		rtl8168_eri_write(tp, 0x1B0, 4, csi_tmp, ERIAR_ExGMAC);

		if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
			csi_tmp = rtl8168_eri_read(tp, 0x2FC, 1, ERIAR_ExGMAC);
			csi_tmp &= ~(BIT_2);
			rtl8168_eri_write(tp, 0x2FC, 1, csi_tmp, ERIAR_ExGMAC);
		} else {
			csi_tmp = rtl8168_eri_read(tp, 0x2FC, 1, ERIAR_ExGMAC);
			csi_tmp &= ~(BIT_0 | BIT_1 | BIT_2);
			csi_tmp |= BIT_0;
			rtl8168_eri_write(tp, 0x2FC, 1, csi_tmp, ERIAR_ExGMAC);
		}

		csi_tmp = rtl8168_eri_read(tp, 0x1D0, 1, ERIAR_ExGMAC);
		csi_tmp |= BIT_1;
		rtl8168_eri_write(tp, 0x1D0, 1, csi_tmp, ERIAR_ExGMAC);
	}

	if ((tp->mcfg == CFG_METHOD_1) || (tp->mcfg == CFG_METHOD_2) ||
	    (tp->mcfg == CFG_METHOD_3)) {
		/* csum offload command for RTL8168B/8111B */
		tp->tx_tcp_csum_cmd = TxTCPCS;
		tp->tx_udp_csum_cmd = TxUDPCS;
		tp->tx_ip_csum_cmd = TxIPCS;
		tp->tx_ipv6_csum_cmd = 0;
	} else {
		/* csum offload command for RTL8168C/8111C and RTL8168CP/8111CP */
		tp->tx_tcp_csum_cmd = TxTCPCS_C;
		tp->tx_udp_csum_cmd = TxUDPCS_C;
		tp->tx_ip_csum_cmd = TxIPCS_C;
		tp->tx_ipv6_csum_cmd = TxIPV6F_C;
	}

	//other hw parameters
	if (tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26)
		rtl8168_eri_write(tp, 0x2F8, 2, 0x1D8F, ERIAR_ExGMAC);

	if (tp->bios_setting & BIT_28) {
		if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19 ||
		    tp->mcfg == CFG_METHOD_20) {
			u32 gphy_val;

			rtl8168_mdio_write(tp, 0x1F, 0x0007);
			rtl8168_mdio_write(tp, 0x1E, 0x002C);
			gphy_val = rtl8168_mdio_read(tp, 0x16);
			gphy_val |= BIT_10;
			rtl8168_mdio_write(tp, 0x16, gphy_val);
			rtl8168_mdio_write(tp, 0x1F, 0x0005);
			rtl8168_mdio_write(tp, 0x05, 0x8B80);
			gphy_val = rtl8168_mdio_read(tp, 0x06);
			gphy_val |= BIT_7;
			rtl8168_mdio_write(tp, 0x06, gphy_val);
			rtl8168_mdio_write(tp, 0x1F, 0x0000);
		}
	}

	rtl8168_hw_clear_timer_int(dev);

	switch (tp->mcfg) {
	case CFG_METHOD_25:
		rtl8168_mac_ocp_write(tp, 0xD3C0, 0x0B00);
		rtl8168_mac_ocp_write(tp, 0xD3C2, 0x0000);
		break;
	}

	tp->cp_cmd &=
		~(EnableBist | Macdbgo_oe | Force_halfdup | Force_rxflow_en |
		  Force_txflow_en | Cxpl_dbg_sel | ASF | Macdbgo_sel);

	rtl8168_hw_set_features(dev, dev->features);

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_33: {
		int timeout;

		for (timeout = 0; timeout < 10; timeout++) {
			if ((rtl8168_eri_read(tp, 0x1AE, 2, ERIAR_ExGMAC) &
			     BIT_13) == 0)
				break;
			mdelay(1);
		}
	} break;
	}

	RTL_W16(tp, RxMaxSize, tp->rx_buf_sz);

	rtl8168_disable_rxdvgate(dev);

	if (tp->mcfg == CFG_METHOD_11 || tp->mcfg == CFG_METHOD_12)
		rtl8168_mac_loopback_test(tp);

	rtl8168_dsm(dev, DSM_MAC_INIT);

	/* Set Rx packet filter */
	rtl8168_hw_set_rx_packet_filter(dev);

#ifdef ENABLE_DASH_SUPPORT
	if (tp->DASH && !tp->dash_printer_enabled)
		NICChkTypeEnableDashInterrupt(tp);
#endif

	switch (tp->mcfg) {
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_33:
		if (aspm) {
			RTL_W8(tp, Config5, RTL_R8(tp, Config5) | BIT_0);
			RTL_W8(tp, Config2, RTL_R8(tp, Config2) | BIT_7);
		} else {
			RTL_W8(tp, Config2, RTL_R8(tp, Config2) & ~BIT_7);
			RTL_W8(tp, Config5, RTL_R8(tp, Config5) & ~BIT_0);
		}
		break;
	}

	rtl8168_disable_cfg9346_write(tp);

	udelay(10);
}

static void rtl8168_hw_start(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	RTL_W8(tp, ChipCmd, CmdTxEnb | CmdRxEnb);

	rtl8168_enable_hw_interrupt(tp);
}

static int rtl8168_change_mtu(struct net_device *dev, int new_mtu)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	int ret = 0;
	unsigned long flags;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	if (new_mtu < ETH_ZLEN)
		return -EINVAL;
	else if (new_mtu > tp->max_jumbo_frame_size)
		new_mtu = tp->max_jumbo_frame_size;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)

	spin_lock_irqsave(&tp->lock, flags);
	dev->mtu = new_mtu;
	spin_unlock_irqrestore(&tp->lock, flags);

	if (!netif_running(dev))
		goto out;

	rtl8168_down(dev);

	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_set_rxbufsize(tp, dev);

	ret = rtl8168_init_ring(dev);

	if (ret < 0) {
		spin_unlock_irqrestore(&tp->lock, flags);
		goto err_out;
	}

#ifdef CONFIG_R8168_NAPI
	RTL_NAPI_ENABLE(dev, &tp->napi);
#endif //CONFIG_R8168_NAPI

	netif_stop_queue(dev);
	netif_carrier_off(dev);
	rtl8168_hw_config(dev);
	spin_unlock_irqrestore(&tp->lock, flags);

	rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex,
			  tp->advertising);

	mod_timer(&tp->link_timer, jiffies + RTL8168_LINK_TIMEOUT);
out:
	netdev_update_features(dev);

err_out:
	return ret;
}

static inline void rtl8168_make_unusable_by_asic(struct RxDesc *desc)
{
	desc->addr = 0x0badbadbadbadbadull;
	desc->opts1 &= ~cpu_to_le32(DescOwn | RsvdMask);
}

static void rtl8168_free_rx_skb(struct rtl8168_private *tp,
				struct sk_buff **sk_buff, struct RxDesc *desc)
{
	dma_unmap_single(&tp->platform_dev->dev, le64_to_cpu(desc->addr),
			 tp->rx_buf_sz, DMA_FROM_DEVICE);
	dev_kfree_skb(*sk_buff);
	*sk_buff = NULL;
	rtl8168_make_unusable_by_asic(desc);
}

static inline void rtl8168_mark_to_asic(struct RxDesc *desc, u32 rx_buf_sz)
{
	u32 eor = le32_to_cpu(desc->opts1) & RingEnd;

	desc->opts1 = cpu_to_le32(DescOwn | eor | rx_buf_sz);
}

static inline void rtl8168_map_to_asic(struct RxDesc *desc, dma_addr_t mapping,
				       u32 rx_buf_sz)
{
	desc->addr = cpu_to_le64(mapping);
	wmb();
	rtl8168_mark_to_asic(desc, rx_buf_sz);
}

static int rtl8168_alloc_rx_skb(struct rtl8168_private *tp,
				struct sk_buff **sk_buff, struct RxDesc *desc,
				int rx_buf_sz, u8 in_intr)
{
	struct sk_buff *skb;
	dma_addr_t mapping;
	int ret = 0;

	if (in_intr) {
		//		skb = RTL_ALLOC_SKB_INTR(tp, rx_buf_sz + RTK_RX_ALIGN);
		skb = alloc_skb(rx_buf_sz + RTK_RX_ALIGN + NET_SKB_PAD,
				GFP_ATOMIC);
	} else
		skb = dev_alloc_skb(rx_buf_sz + RTK_RX_ALIGN);

	if (unlikely(!skb))
		goto err_out;

	skb_reserve(skb, RTK_RX_ALIGN);

	mapping = dma_map_single(&tp->platform_dev->dev, skb->data, rx_buf_sz,
				 DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(&tp->platform_dev->dev, mapping))) {
		if (unlikely(net_ratelimit()))
			netif_err(tp, drv, tp->dev, "Failed to map RX DMA!\n");
		goto err_out;
	}

	*sk_buff = skb;
	rtl8168_map_to_asic(desc, mapping, rx_buf_sz);
out:
	return ret;

err_out:
	if (skb)
		dev_kfree_skb(skb);
	ret = -ENOMEM;
	rtl8168_make_unusable_by_asic(desc);
	goto out;
}

static void rtl8168_rx_clear(struct rtl8168_private *tp)
{
	int i;

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (tp->Rx_skbuff[i])
			rtl8168_free_rx_skb(tp, tp->Rx_skbuff + i,
					    tp->RxDescArray + i);
	}
}

static u32 rtl8168_rx_fill(struct rtl8168_private *tp, struct net_device *dev,
			   u32 start, u32 end, u8 in_intr)
{
	u32 cur;

	for (cur = start; end - cur > 0; cur++) {
		int ret, i = cur % NUM_RX_DESC;

		if (tp->Rx_skbuff[i])
			continue;

		ret = rtl8168_alloc_rx_skb(tp, tp->Rx_skbuff + i,
					   tp->RxDescArray + i, tp->rx_buf_sz,
					   in_intr);
		if (ret < 0)
			break;
	}
	return cur - start;
}

static inline void rtl8168_mark_as_last_descriptor(struct RxDesc *desc)
{
	desc->opts1 |= cpu_to_le32(RingEnd);
}

static void rtl8168_desc_addr_fill(struct rtl8168_private *tp)
{
	if (!tp->TxPhyAddr || !tp->RxPhyAddr)
		return;

	RTL_W32(tp, TxDescStartAddrLow,
		((u64)tp->TxPhyAddr & DMA_BIT_MASK(32)));
	RTL_W32(tp, TxDescStartAddrHigh, ((u64)tp->TxPhyAddr >> 32));
	RTL_W32(tp, RxDescAddrLow, ((u64)tp->RxPhyAddr & DMA_BIT_MASK(32)));
	RTL_W32(tp, RxDescAddrHigh, ((u64)tp->RxPhyAddr >> 32));
}

static void rtl8168_tx_desc_init(struct rtl8168_private *tp)
{
	int i = 0;

	memset(tp->TxDescArray, 0x0, NUM_TX_DESC * sizeof(struct TxDesc));

	for (i = 0; i < NUM_TX_DESC; i++) {
		if (i == (NUM_TX_DESC - 1))
			tp->TxDescArray[i].opts1 = cpu_to_le32(RingEnd);
	}
}

static void rtl8168_rx_desc_offset0_init(struct rtl8168_private *tp, int own)
{
	int i = 0;
	int ownbit = 0;

	if (own)
		ownbit = DescOwn;

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (i == (NUM_RX_DESC - 1))
			tp->RxDescArray[i].opts1 =
				cpu_to_le32((ownbit | RingEnd) |
					    (unsigned long)tp->rx_buf_sz);
		else
			tp->RxDescArray[i].opts1 = cpu_to_le32(
				ownbit | (unsigned long)tp->rx_buf_sz);
	}
}

static void rtl8168_rx_desc_init(struct rtl8168_private *tp)
{
	memset(tp->RxDescArray, 0x0, NUM_RX_DESC * sizeof(struct RxDesc));
}

static int rtl8168_init_ring(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	rtl8168_init_ring_indexes(tp);

	memset(tp->tx_skb, 0x0, NUM_TX_DESC * sizeof(struct ring_info));
	memset(tp->Rx_skbuff, 0x0, NUM_RX_DESC * sizeof(struct sk_buff *));

	rtl8168_tx_desc_init(tp);
	rtl8168_rx_desc_init(tp);

	if (rtl8168_rx_fill(tp, dev, 0, NUM_RX_DESC, 0) != NUM_RX_DESC)
		goto err_out;

	rtl8168_mark_as_last_descriptor(tp->RxDescArray + NUM_RX_DESC - 1);

	return 0;

err_out:
	rtl8168_rx_clear(tp);
	return -ENOMEM;
}

static void rtl8168_unmap_tx_skb(struct platform_device *pdev,
				 struct ring_info *tx_skb, struct TxDesc *desc)
{
	unsigned int len = tx_skb->len;

	dma_unmap_single(&pdev->dev, le64_to_cpu(desc->addr), len,
			 DMA_TO_DEVICE);

	desc->opts1 = cpu_to_le32(RTK_MAGIC_DEBUG_VALUE);
	desc->opts2 = 0x00;
	desc->addr = 0x00;
	tx_skb->len = 0;
}

static void rtl8168_tx_clear_range(struct rtl8168_private *tp, u32 start,
				   unsigned int n)
{
	unsigned int i;
	struct net_device *dev = tp->dev;

	for (i = 0; i < n; i++) {
		unsigned int entry = (start + i) % NUM_TX_DESC;
		struct ring_info *tx_skb = tp->tx_skb + entry;
		unsigned int len = tx_skb->len;

		if (len) {
			struct sk_buff *skb = tx_skb->skb;

			rtl8168_unmap_tx_skb(tp->platform_dev, tx_skb,
					     tp->TxDescArray + entry);
			if (skb) {
				RTLDEV->stats.tx_dropped++;
				dev_kfree_skb_any(skb);
				tx_skb->skb = NULL;
			}
		}
	}
}

static void rtl8168_tx_clear(struct rtl8168_private *tp)
{
	rtl8168_tx_clear_range(tp, tp->dirty_tx, NUM_TX_DESC);
	tp->cur_tx = tp->dirty_tx = 0;
}

static void rtl8168_schedule_work(struct net_device *dev, work_func_t task)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	INIT_DELAYED_WORK(&tp->task, task);
	schedule_delayed_work(&tp->task, 4);
}

static void rtl8168_cancel_schedule_work(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	struct work_struct *work = &tp->task.work;

	if (!work->func)
		return;

	cancel_delayed_work_sync(&tp->task);
}

static void rtl8168_wait_for_quiescence(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	synchronize_irq(dev->irq);

	/* Wait for any pending NAPI task to complete */
#ifdef CONFIG_R8168_NAPI
	RTL_NAPI_DISABLE(dev, &tp->napi);
#endif //CONFIG_R8168_NAPI

	rtl8168_irq_mask_and_ack(tp);

#ifdef CONFIG_R8168_NAPI
	RTL_NAPI_ENABLE(dev, &tp->napi);
#endif //CONFIG_R8168_NAPI
}

static void rtl8168_reset_task(struct work_struct *work)
{
	struct rtl8168_private *tp =
		container_of(work, struct rtl8168_private, task.work);
	struct net_device *dev = tp->dev;
	u32 budget = ~(u32)0;
	unsigned long flags;

	if (!netif_running(dev))
		return;

	rtl8168_wait_for_quiescence(dev);

	rtl8168_rx_interrupt(dev, tp, budget);

	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_tx_clear(tp);

	if (tp->dirty_rx == tp->cur_rx) {
		rtl8168_rx_clear(tp);
		rtl8168_init_ring(dev);
		rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex,
				  tp->advertising);
		spin_unlock_irqrestore(&tp->lock, flags);
	} else {
		spin_unlock_irqrestore(&tp->lock, flags);
		if (unlikely(net_ratelimit())) {
			struct rtl8168_private *tp = netdev_priv(dev);

			if (netif_msg_intr(tp)) {
				printk(PFX KERN_EMERG
				       "%s: Rx buffers shortage\n",
				       dev->name);
			}
		}
		rtl8168_schedule_work(dev, rtl8168_reset_task);
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static void rtl8168_tx_timeout(struct net_device *dev, unsigned int txqueue)
#else
static void rtl8168_tx_timeout(struct net_device *dev)
#endif
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);
	netif_stop_queue(dev);
	netif_carrier_off(dev);
	rtl8168_hw_reset(dev);
	spin_unlock_irqrestore(&tp->lock, flags);

	/* Let's wait a bit while any (async) irq lands on */
	rtl8168_schedule_work(dev, rtl8168_reset_task);
}

static u32 rtl8168_get_txd_opts1(u32 opts1, u32 len, unsigned int entry)
{
	u32 status = opts1 | len;

	if (entry == NUM_TX_DESC - 1)
		status |= RingEnd;

	return status;
}

static int rtl8168_xmit_frags(struct rtl8168_private *tp, struct sk_buff *skb,
			      u32 opts1, u32 opts2)
{
	struct skb_shared_info *info = skb_shinfo(skb);
	unsigned int cur_frag, entry;
	struct TxDesc *txd = NULL;
	const unsigned char nr_frags = info->nr_frags;

	entry = tp->cur_tx;
	for (cur_frag = 0; cur_frag < nr_frags; cur_frag++) {
		skb_frag_t *frag = info->frags + cur_frag;
		dma_addr_t mapping;
		u32 status, len;
		void *addr;

		entry = (entry + 1) % NUM_TX_DESC;

		txd = tp->TxDescArray + entry;
		len = skb_frag_size(frag);
		addr = skb_frag_address(frag);
		mapping = dma_map_single(&tp->platform_dev->dev, addr, len,
					 DMA_TO_DEVICE);

		if (unlikely(dma_mapping_error(&tp->platform_dev->dev,
					       mapping))) {
			if (unlikely(net_ratelimit()))
				netif_err(tp, drv, tp->dev,
					  "Failed to map TX fragments DMA!\n");
			goto err_out;
		}

		/* anti gcc 2.95.3 bugware (sic) */
		status = rtl8168_get_txd_opts1(opts1, len, entry);
		if (cur_frag == (nr_frags - 1)) {
			tp->tx_skb[entry].skb = skb;
			status |= LastFrag;
		}

		txd->addr = cpu_to_le64(mapping);

		tp->tx_skb[entry].len = len;

		txd->opts2 = cpu_to_le32(opts2);
		wmb();
		txd->opts1 = cpu_to_le32(status);
	}

	return cur_frag;

err_out:
	rtl8168_tx_clear_range(tp, tp->cur_tx + 1, cur_frag);
	return -EIO;
}

static inline __be16 get_protocol(struct sk_buff *skb)
{
	__be16 protocol;

	if (skb->protocol == htons(ETH_P_8021Q))
		protocol = vlan_eth_hdr(skb)->h_vlan_encapsulated_proto;
	else
		protocol = skb->protocol;

	return protocol;
}

static inline u32 rtl8168_tx_csum(struct sk_buff *skb, struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	u32 csum_cmd = 0;
	u8 sw_calc_csum = FALSE;

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		u8 ip_protocol = IPPROTO_RAW;

		switch (get_protocol(skb)) {
		case __constant_htons(ETH_P_IP):
			if (dev->features & NETIF_F_IP_CSUM) {
				ip_protocol = ip_hdr(skb)->protocol;
				csum_cmd = tp->tx_ip_csum_cmd;
			}
			break;
		case __constant_htons(ETH_P_IPV6):
			if (dev->features & NETIF_F_IPV6_CSUM) {
				u32 transport_offset =
					(u32)skb_transport_offset(skb);
				if (transport_offset > 0 &&
				    transport_offset <= TCPHO_MAX) {
					ip_protocol = ipv6_hdr(skb)->nexthdr;
					csum_cmd = tp->tx_ipv6_csum_cmd;
					csum_cmd |= transport_offset
						    << TCPHO_SHIFT;
				}
			}
			break;
		default:
			if (unlikely(net_ratelimit()))
				dprintk("checksum_partial proto=%x!\n",
					skb->protocol);
			break;
		}

		if (ip_protocol == IPPROTO_TCP)
			csum_cmd |= tp->tx_tcp_csum_cmd;
		else if (ip_protocol == IPPROTO_UDP)
			csum_cmd |= tp->tx_udp_csum_cmd;
		if (csum_cmd == 0) {
			sw_calc_csum = TRUE;
			WARN_ON(1); /* we need a WARN() */
		}
	}

	if (tp->ShortPacketSwChecksum && skb->len < 60 && csum_cmd != 0)
		sw_calc_csum = TRUE;

	if (sw_calc_csum) {
		skb_checksum_help(skb);
		csum_cmd = 0;
	}

	return csum_cmd;
}

static int rtl8168_sw_padding_short_pkt(struct rtl8168_private *tp,
					struct sk_buff *skb, u32 opts1,
					u32 opts2)
{
	unsigned int entry;
	dma_addr_t mapping;
	u32 status, len;
	void *addr;
	struct TxDesc *txd = NULL;
	int ret = 0;

	if (skb->len >= ETH_ZLEN)
		goto out;

	entry = tp->cur_tx;

	entry = (entry + 1) % NUM_TX_DESC;

	txd = tp->TxDescArray + entry;
	len = ETH_ZLEN - skb->len;
	addr = tp->ShortPacketEmptyBuffer;
	mapping = dma_map_single(&tp->platform_dev->dev, addr, len,
				 DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(&tp->platform_dev->dev, mapping))) {
		if (unlikely(net_ratelimit()))
			netif_err(tp, drv, tp->dev,
				  "Failed to map Short Packet Buffer DMA!\n");
		ret = -ENOMEM;
		goto out;
	}
	status = rtl8168_get_txd_opts1(opts1, len, entry);
	status |= LastFrag;

	txd->addr = cpu_to_le64(mapping);

	txd->opts2 = cpu_to_le32(opts2);
	wmb();
	txd->opts1 = cpu_to_le32(status);
out:
	return ret;
}

/* r8169_csum_workaround()
  * The hw limites the value the transport offset. When the offset is out of the
  * range, calculate the checksum by sw.
  */
static void r8168_csum_workaround(struct rtl8168_private *tp,
				  struct sk_buff *skb)
{
	if (skb_shinfo(skb)->gso_size) {
		netdev_features_t features = tp->dev->features;
		struct sk_buff *segs, *nskb;

		features &= ~(NETIF_F_SG | NETIF_F_IPV6_CSUM | NETIF_F_TSO6);
		segs = skb_segment(skb, features);
		if (IS_ERR(segs) || !segs)
			goto drop;

		do {
			nskb = segs;
			segs = segs->next;
			nskb->next = NULL;
			rtl8168_start_xmit(nskb, tp->dev);
		} while (segs);

		dev_consume_skb_any(skb);
	} else if (skb->ip_summed == CHECKSUM_PARTIAL) {
		if (skb_checksum_help(skb) < 0)
			goto drop;

		rtl8168_start_xmit(skb, tp->dev);
	} else {
		struct net_device_stats *stats;

drop:
		stats = &tp->dev->stats;
		stats->tx_dropped++;
		dev_kfree_skb_any(skb);
	}
}

/* msdn_giant_send_check()
 * According to the document of microsoft, the TCP Pseudo Header excludes the
 * packet length for IPv6 TCP large packets.
 */
static int msdn_giant_send_check(struct sk_buff *skb)
{
	const struct ipv6hdr *ipv6h;
	struct tcphdr *th;
	int ret;

	ret = skb_cow_head(skb, 0);
	if (ret)
		return ret;

	ipv6h = ipv6_hdr(skb);
	th = tcp_hdr(skb);

	th->check = 0;
	th->check = ~tcp_v6_check(0, &ipv6h->saddr, &ipv6h->daddr, 0);

	return ret;
}

static bool rtl8168_tx_slots_avail(struct rtl8168_private *tp,
				   unsigned int nr_frags)
{
	unsigned int slots_avail = tp->dirty_tx + NUM_TX_DESC - tp->cur_tx;

	/* A skbuff with nr_frags needs nr_frags+1 entries in the tx queue */
	return slots_avail > nr_frags;
}

static int rtl8168_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned int entry;
	struct TxDesc *txd;
	dma_addr_t mapping;
	u32 len;
	u32 opts1;
	u32 opts2;
	int ret = NETDEV_TX_OK;
	unsigned long flags, large_send;
	int frags;

	spin_lock_irqsave(&tp->lock, flags);

	if (unlikely(!rtl8168_tx_slots_avail(tp, skb_shinfo(skb)->nr_frags))) {
		if (netif_msg_drv(tp)) {
			printk(KERN_ERR
			       "%s: BUG! Tx Ring full when queue awake!\n",
			       dev->name);
		}
		goto err_stop;
	}

	entry = tp->cur_tx % NUM_TX_DESC;
	txd = tp->TxDescArray + entry;

	if (unlikely(le32_to_cpu(txd->opts1) & DescOwn)) {
		if (netif_msg_drv(tp)) {
			printk(KERN_ERR
			       "%s: BUG! Tx Desc is own by hardware!\n",
			       dev->name);
		}
		goto err_stop;
	}

	opts1 = DescOwn;
	opts2 = rtl8168_tx_vlan_tag(tp, skb);

	large_send = 0;
	if (dev->features & (NETIF_F_TSO | NETIF_F_TSO6)) {
		u32 mss = skb_shinfo(skb)->gso_size;

		/* TCP Segmentation Offload (or TCP Large Send) */
		if (mss) {
			if ((tp->mcfg == CFG_METHOD_1) ||
			    (tp->mcfg == CFG_METHOD_2) ||
			    (tp->mcfg == CFG_METHOD_3)) {
				opts1 |= LargeSend | (min(mss, MSS_MAX) << 16);
				large_send = 1;
			} else {
				u32 transport_offset =
					(u32)skb_transport_offset(skb);

				switch (get_protocol(skb)) {
				case __constant_htons(ETH_P_IP):
					if (transport_offset <= 128) {
						opts1 |= GiantSendv4;
						opts1 |= transport_offset
							 << GTTCPHO_SHIFT;
						opts2 |= min(mss, MSS_MAX)
							 << 18;
						large_send = 1;
					}
					break;
				case __constant_htons(ETH_P_IPV6):
					if (msdn_giant_send_check(skb)) {
						spin_unlock_irqrestore(
							&tp->lock, flags);
						r8168_csum_workaround(tp, skb);
						goto out;
					}
					if (transport_offset <= 128) {
						opts1 |= GiantSendv6;
						opts1 |= transport_offset
							 << GTTCPHO_SHIFT;
						opts2 |= min(mss, MSS_MAX)
							 << 18;
						large_send = 1;
					}
					break;
				default:
					if (unlikely(net_ratelimit()))
						dprintk("tso proto=%x!\n",
							skb->protocol);
					break;
				}
			}

			if (large_send == 0)
				goto err_dma_0;
		}
	}

	if (large_send == 0) {
		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			if ((tp->mcfg == CFG_METHOD_1) ||
			    (tp->mcfg == CFG_METHOD_2) ||
			    (tp->mcfg == CFG_METHOD_3))
				opts1 |= rtl8168_tx_csum(skb, dev);
			else
				opts2 |= rtl8168_tx_csum(skb, dev);
		}
	}

	frags = rtl8168_xmit_frags(tp, skb, opts1, opts2);
	if (unlikely(frags < 0))
		goto err_dma_0;
	if (frags) {
		len = skb_headlen(skb);
		opts1 |= FirstFrag;
	} else {
		len = skb->len;
		tp->tx_skb[entry].skb = skb;

		if (tp->UseSwPaddingShortPkt && len < 60) {
			if (unlikely(rtl8168_sw_padding_short_pkt(
				    tp, skb, opts1, opts2)))
				goto err_dma_1;
			opts1 |= FirstFrag;
			frags++;
		} else {
			opts1 |= FirstFrag | LastFrag;
		}
	}

	opts1 = rtl8168_get_txd_opts1(opts1, len, entry);
	mapping = dma_map_single(&tp->platform_dev->dev, skb->data, len,
				 DMA_TO_DEVICE);
	tp->tx_skb[entry].len = len;
	txd->addr = cpu_to_le64(mapping);
	txd->opts2 = cpu_to_le32(opts2);
	wmb();
	txd->opts1 = cpu_to_le32(opts1);

	skb_tx_timestamp(skb);

	tp->cur_tx += frags + 1;

	wmb();

	RTL_W8(tp, TxPoll, NPQ); /* set polling bit */

	if (!rtl8168_tx_slots_avail(tp, MAX_SKB_FRAGS)) {
		netif_stop_queue(dev);
		smp_rmb();
		if (rtl8168_tx_slots_avail(tp, MAX_SKB_FRAGS))
			netif_wake_queue(dev);
	}

	spin_unlock_irqrestore(&tp->lock, flags);
out:
	return ret;
err_dma_1:
	tp->tx_skb[entry].skb = NULL;
	rtl8168_tx_clear_range(tp, tp->cur_tx + 1, frags);
err_dma_0:
	RTLDEV->stats.tx_dropped++;
	spin_unlock_irqrestore(&tp->lock, flags);
	dev_kfree_skb_any(skb);
	ret = NETDEV_TX_OK;
	goto out;
err_stop:
	netif_stop_queue(dev);
	ret = NETDEV_TX_BUSY;
	RTLDEV->stats.tx_dropped++;

	spin_unlock_irqrestore(&tp->lock, flags);
	goto out;
}

static void rtl8168_tx_interrupt(struct net_device *dev,
				 struct rtl8168_private *tp)
{
	unsigned int dirty_tx, tx_left;

	assert(dev != NULL);
	assert(tp != NULL);

	dirty_tx = tp->dirty_tx;
	smp_rmb();
	tx_left = tp->cur_tx - dirty_tx;

	while (tx_left > 0) {
		unsigned int entry = dirty_tx % NUM_TX_DESC;
		struct ring_info *tx_skb = tp->tx_skb + entry;
		u32 len = tx_skb->len;
		u32 status;

		rmb();
		status = le32_to_cpu(tp->TxDescArray[entry].opts1);
		if (status & DescOwn)
			break;

		RTLDEV->stats.tx_bytes += len;
		RTLDEV->stats.tx_packets++;

		rtl8168_unmap_tx_skb(tp->platform_dev, tx_skb,
				     tp->TxDescArray + entry);

		if (tx_skb->skb != NULL) {
			dev_consume_skb_any(tx_skb->skb);
			tx_skb->skb = NULL;
		}
		dirty_tx++;
		tx_left--;
	}

	if (tp->dirty_tx != dirty_tx) {
		tp->dirty_tx = dirty_tx;
		smp_wmb();
		if (netif_queue_stopped(dev) &&
		    (rtl8168_tx_slots_avail(tp, MAX_SKB_FRAGS))) {
			netif_wake_queue(dev);
		}
		smp_rmb();
		if (tp->cur_tx != dirty_tx)
			RTL_W8(tp, TxPoll, NPQ);
	}
}

static inline int rtl8168_fragmented_frame(u32 status)
{
	return (status & (FirstFrag | LastFrag)) != (FirstFrag | LastFrag);
}

static inline void rtl8168_rx_csum(struct rtl8168_private *tp,
				   struct sk_buff *skb, struct RxDesc *desc)
{
	u32 opts1 = le32_to_cpu(desc->opts1);
	u32 opts2 = le32_to_cpu(desc->opts2);

	if ((tp->mcfg == CFG_METHOD_1) || (tp->mcfg == CFG_METHOD_2) ||
	    (tp->mcfg == CFG_METHOD_3)) {
		u32 status = opts1 & RxProtoMask;

		/* rx csum offload for RTL8168B/8111B */
		if (((status == RxProtoTCP) && !(opts1 & (RxTCPF | RxIPF))) ||
		    ((status == RxProtoUDP) && !(opts1 & (RxUDPF | RxIPF))))
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb->ip_summed = CHECKSUM_NONE;
	} else {
		/* rx csum offload for RTL8168C/8111C and RTL8168CP/8111CP */
		if (((opts2 & RxV4F) && !(opts1 & RxIPF)) || (opts2 & RxV6F)) {
			if (((opts1 & RxTCPT) && !(opts1 & RxTCPF)) ||
			    ((opts1 & RxUDPT) && !(opts1 & RxUDPF)))
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			else
				skb->ip_summed = CHECKSUM_NONE;
		} else
			skb->ip_summed = CHECKSUM_NONE;
	}
}

static inline int rtl8168_try_rx_copy(struct rtl8168_private *tp,
				      struct sk_buff **sk_buff, int pkt_size,
				      struct RxDesc *desc, int rx_buf_sz)
{
	int ret = -1;
	/*
	if (pkt_size < rx_copybreak) {
		struct sk_buff *skb;

		skb = RTL_ALLOC_SKB_INTR(tp, pkt_size + RTK_RX_ALIGN);
		if (skb) {
			u8 *data;

			data = sk_buff[0]->data;
			skb_reserve(skb, RTK_RX_ALIGN);
			prefetch(data - RTK_RX_ALIGN);
			eth_copy_and_sum(skb, data, pkt_size, 0);
			*sk_buff = skb;
			rtl8168_mark_to_asic(desc, rx_buf_sz);
			ret = 0;
		}
	}
*/
	return ret;
}

static inline void rtl8168_rx_skb(struct rtl8168_private *tp,
				  struct sk_buff *skb)
{
#ifdef CONFIG_R8168_NAPI
	napi_gro_receive(&tp->napi, skb);
#else
	netif_rx(skb);
#endif
}

static int rtl8168_rx_interrupt(struct net_device *dev,
				struct rtl8168_private *tp, napi_budget budget)
{
	unsigned int cur_rx, rx_left;
	unsigned int delta, count = 0;
	unsigned int entry;
	struct RxDesc *desc;
	u32 status;
	u32 rx_quota;

	assert(dev != NULL);
	assert(tp != NULL);

	if ((tp->RxDescArray == NULL) || (tp->Rx_skbuff == NULL))
		goto rx_out;

	rx_quota = RTL_RX_QUOTA(budget);
	cur_rx = tp->cur_rx;
	entry = cur_rx % NUM_RX_DESC;
	desc = tp->RxDescArray + entry;
	rx_left = NUM_RX_DESC + tp->dirty_rx - cur_rx;
	rx_left = rtl8168_rx_quota(rx_left, (u32)rx_quota);

	for (; rx_left > 0; rx_left--) {
		rmb();
		status = le32_to_cpu(desc->opts1);
		if (status & DescOwn)
			break;
		if (unlikely(status & RxRES)) {
			if (netif_msg_rx_err(tp)) {
				printk(KERN_INFO
				       "%s: Rx ERROR. status = %08x\n",
				       dev->name, status);
			}

			RTLDEV->stats.rx_errors++;

			if (status & (RxRWT | RxRUNT))
				RTLDEV->stats.rx_length_errors++;
			if (status & RxCRC)
				RTLDEV->stats.rx_crc_errors++;
			if (dev->features & NETIF_F_RXALL)
				goto process_pkt;

			rtl8168_mark_to_asic(desc, tp->rx_buf_sz);
		} else {
			struct sk_buff *skb;
			int pkt_size;

process_pkt:
			if (likely(!(dev->features & NETIF_F_RXFCS)))
				pkt_size = (status & 0x00003fff) - 4;
			else
				pkt_size = status & 0x00003fff;

			/*
			 * The driver does not support incoming fragmented
			 * frames. They are seen as a symptom of over-mtu
			 * sized frames.
			 */
			if (unlikely(rtl8168_fragmented_frame(status))) {
				RTLDEV->stats.rx_dropped++;
				RTLDEV->stats.rx_length_errors++;
				rtl8168_mark_to_asic(desc, tp->rx_buf_sz);
				continue;
			}

			skb = tp->Rx_skbuff[entry];

			dma_sync_single_for_cpu(&tp->platform_dev->dev,
						le64_to_cpu(desc->addr),
						tp->rx_buf_sz, DMA_FROM_DEVICE);

			if (rtl8168_try_rx_copy(tp, &skb, pkt_size, desc,
						tp->rx_buf_sz)) {
				tp->Rx_skbuff[entry] = NULL;
				dma_unmap_single(&tp->platform_dev->dev,
						 le64_to_cpu(desc->addr),
						 tp->rx_buf_sz,
						 DMA_FROM_DEVICE);
			} else {
				dma_sync_single_for_device(
					&tp->platform_dev->dev,
					le64_to_cpu(desc->addr), tp->rx_buf_sz,
					DMA_FROM_DEVICE);
			}

			if (tp->cp_cmd & RxChkSum)
				rtl8168_rx_csum(tp, skb, desc);

			skb->dev = dev;
			skb_put(skb, pkt_size);
#ifdef CONFIG_RTL8168_DMA_TEST
			rtl8168_start_xmit(skb, dev);
#else
			skb->protocol = eth_type_trans(skb, dev);

			if (skb->pkt_type == PACKET_MULTICAST)
				RTLDEV->stats.multicast++;

			if (rtl8168_rx_vlan_skb(tp, desc, skb) < 0)
				rtl8168_rx_skb(tp, skb);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
			dev->last_rx = jiffies;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
			RTLDEV->stats.rx_bytes += pkt_size;
			RTLDEV->stats.rx_packets++;
		}

		cur_rx++;
		entry = cur_rx % NUM_RX_DESC;
		desc = tp->RxDescArray + entry;
		prefetch(desc);
	}

	count = cur_rx - tp->cur_rx;
	tp->cur_rx = cur_rx;

	delta = rtl8168_rx_fill(tp, dev, tp->dirty_rx, tp->cur_rx, 1);
	if (!delta && count && netif_msg_intr(tp))
		printk(KERN_INFO "%s: no Rx buffer allocated\n", dev->name);
	tp->dirty_rx += delta;

	/*
	 * FIXME: until there is periodic timer to try and refill the ring,
	 * a temporary shortage may definitely kill the Rx process.
	 * - disable the asic to try and avoid an overflow and kick it again
	 *	 after refill ?
	 * - how do others driver handle this condition (Uh oh...).
	 */
	if ((tp->dirty_rx + NUM_RX_DESC == tp->cur_rx) && netif_msg_intr(tp))
		printk(KERN_EMERG "%s: Rx buffers exhausted\n", dev->name);

rx_out:
	return count;
}

/*
 *The interrupt handler does all of the Rx thread work and cleans up after
 *the Tx thread.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static irqreturn_t rtl8168_interrupt(int irq, void *dev_instance,
				     struct pt_regs *regs)
#else
static irqreturn_t rtl8168_interrupt(int irq, void *dev_instance)
#endif
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct rtl8168_private *tp = netdev_priv(dev);
	int status;
	int handled = 0;

	do {
		status = RTL_R16(tp, IntrStatus);

		if (!(tp->features & RTL_FEATURE_MSI)) {
			/* hotplug/major error/no more work/shared irq */
			if ((status == 0xFFFF) || !status)
				break;

			if (!(status & (tp->intr_mask | tp->timer_intr_mask)))
				break;
		}

		handled = 1;

		rtl8168_disable_hw_interrupt(tp);

		switch (tp->mcfg) {
		case CFG_METHOD_24:
		case CFG_METHOD_25:
		case CFG_METHOD_33:
			/* RX_OVERFLOW RE-START mechanism now HW handles it automatically*/
			RTL_W16(tp, IntrStatus, status & ~RxFIFOOver);
			break;
		default:
			RTL_W16(tp, IntrStatus, status);
			break;
		}

		//Work around for rx fifo overflow
		if (unlikely(status & RxFIFOOver)) {
			if (tp->mcfg == CFG_METHOD_1) {
				netif_stop_queue(dev);
				udelay(300);
				rtl8168_hw_reset(dev);
				rtl8168_tx_clear(tp);
				rtl8168_rx_clear(tp);
				rtl8168_init_ring(dev);
				rtl8168_hw_config(dev);
				rtl8168_hw_start(dev);
				netif_wake_queue(dev);
			}
		}

#ifdef ENABLE_DASH_SUPPORT
		if (tp->DASH) {
			if (HW_DASH_SUPPORT_TYPE_2(tp) ||
			    HW_DASH_SUPPORT_TYPE_3(tp)) {
				u8 DashIntType2Status;

				if (status & ISRIMR_DASH_INTR_CMAC_RESET)
					tp->CmacResetIntr = TRUE;

				DashIntType2Status =
					RTL_CMAC_R8(tp, CMAC_IBISR0);
				if (DashIntType2Status & ISRIMR_DASH_TYPE2_ROK)
					tp->RcvFwDashOkEvt = TRUE;

				if (DashIntType2Status & ISRIMR_DASH_TYPE2_TOK)
					tp->SendFwHostOkEvt = TRUE;

				if (DashIntType2Status &
				    ISRIMR_DASH_TYPE2_RX_DISABLE_IDLE)
					tp->DashFwDisableRx = TRUE;

				RTL_CMAC_W8(tp, CMAC_IBISR0,
					    DashIntType2Status);
			} else {
				if (status & ISRIMR_DP_REQSYS_OK)
					tp->RcvFwReqSysOkEvt = TRUE;

				if (status & ISRIMR_DP_DASH_OK)
					tp->RcvFwDashOkEvt = TRUE;

				if (status & ISRIMR_DP_HOST_OK)
					tp->SendFwHostOkEvt = TRUE;
			}
		}
#endif

#ifdef CONFIG_R8168_NAPI
		if (status & tp->intr_mask || tp->keep_intr_cnt-- > 0) {
			if (status & tp->intr_mask)
				tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;

			if (likely(RTL_NETIF_RX_SCHEDULE_PREP(dev, &tp->napi)))
				__RTL_NETIF_RX_SCHEDULE(dev, &tp->napi);
			else if (netif_msg_intr(tp))
				printk(KERN_INFO "%s: interrupt %04x in poll\n",
				       dev->name, status);
		} else {
			tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
			rtl8168_switch_to_hw_interrupt(tp);
		}
#else
		if (status & tp->intr_mask || tp->keep_intr_cnt-- > 0) {
			u32 budget = ~(u32)0;

			if (status & tp->intr_mask)
				tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
			rtl8168_rx_interrupt(dev, tp, budget);
			rtl8168_tx_interrupt(dev, tp);

#ifdef ENABLE_DASH_SUPPORT
			if (tp->DASH) {
				struct net_device *dev = tp->dev;

				HandleDashInterrupt(dev);
			}
#endif

			rtl8168_switch_to_timer_interrupt(tp);
		} else {
			tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
			rtl8168_switch_to_hw_interrupt(tp);
		}
#endif

	} while (false);

	return IRQ_RETVAL(handled);
}

#ifdef CONFIG_R8168_NAPI
static int rtl8168_poll(napi_ptr napi, napi_budget budget)
{
	struct rtl8168_private *tp = RTL_GET_PRIV(napi, struct rtl8168_private);
	RTL_GET_NETDEV(tp)
	unsigned int work_to_do = RTL_NAPI_QUOTA(budget, dev);
	unsigned int work_done;
	unsigned long flags;

	work_done = rtl8168_rx_interrupt(dev, tp, budget);

	spin_lock_irqsave(&tp->lock, flags);
	rtl8168_tx_interrupt(dev, tp);
	spin_unlock_irqrestore(&tp->lock, flags);

	RTL_NAPI_QUOTA_UPDATE(dev, work_done, budget);

	if (work_done < work_to_do) {
#ifdef ENABLE_DASH_SUPPORT
		if (tp->DASH) {
			struct net_device *dev = tp->dev;

			spin_lock_irqsave(&tp->lock, flags);
			HandleDashInterrupt(dev);
			spin_unlock_irqrestore(&tp->lock, flags);
		}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
		if (RTL_NETIF_RX_COMPLETE(dev, napi, work_done) == FALSE)
			return RTL_NAPI_RETURN_VALUE;
#else
		RTL_NETIF_RX_COMPLETE(dev, napi, work_done);
#endif
		/*
		 * 20040426: the barrier is not strictly required but the
		 * behavior of the irq handler could be less predictable
		 * without it. Btw, the lack of flush for the posted pci
		 * write is safe - FR
		 */
		smp_wmb();

		rtl8168_switch_to_timer_interrupt(tp);
	}

	return RTL_NAPI_RETURN_VALUE;
}
#endif //CONFIG_R8168_NAPI

static void rtl8168_sleep_rx_enable(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);

	if ((tp->mcfg == CFG_METHOD_1) || (tp->mcfg == CFG_METHOD_2)) {
		RTL_W8(tp, ChipCmd, CmdReset);
		rtl8168_rx_desc_offset0_init(tp, 0);
		RTL_W8(tp, ChipCmd, CmdRxEnb);
	} else if (tp->mcfg == CFG_METHOD_14 || tp->mcfg == CFG_METHOD_15) {
		rtl8168_ephy_write(tp, 0x19, 0xFF64);
		RTL_W32(tp, RxConfig,
			RTL_R32(tp, RxConfig) | AcceptBroadcast |
				AcceptMulticast | AcceptMyPhys);
	}
}

static void rtl8168_down(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	rtl8168_delete_link_timer(dev, &tp->link_timer);

#ifdef CONFIG_R8168_NAPI
	RTL_NAPI_DISABLE(dev, &tp->napi);
#endif //CONFIG_R8168_NAPI

	netif_stop_queue(dev);

	/* Give a racing hard_start_xmit a few cycles to complete. */
	synchronize_rcu(); /* FIXME: should this be synchronize_irq()? */

	spin_lock_irqsave(&tp->lock, flags);

	netif_carrier_off(dev);

	rtl8168_dsm(dev, DSM_IF_DOWN);

	rtl8168_hw_reset(dev);

	spin_unlock_irqrestore(&tp->lock, flags);

	synchronize_irq(dev->irq);

	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_tx_clear(tp);

	rtl8168_rx_clear(tp);

	rtl8168_sleep_rx_enable(dev);

	spin_unlock_irqrestore(&tp->lock, flags);
}

static int rtl8168_close(struct net_device *dev)
{
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	if (tp->TxDescArray != NULL && tp->RxDescArray != NULL) {
		rtl8168_cancel_schedule_work(dev);

		rtl8168_down(dev);

		spin_lock_irqsave(&tp->lock, flags);

		rtl8168_hw_d3_para(dev);

		rtl8168_powerdown_pll(dev);

		spin_unlock_irqrestore(&tp->lock, flags);

		free_irq(dev->irq, dev);

		dma_free_coherent(&tp->platform_dev->dev, R8168_RX_RING_BYTES,
				  tp->RxDescArray, tp->RxPhyAddr);
		dma_free_coherent(&tp->platform_dev->dev, R8168_TX_RING_BYTES,
				  tp->TxDescArray, tp->TxPhyAddr);
		tp->TxDescArray = NULL;
		tp->RxDescArray = NULL;

		if (tp->ShortPacketEmptyBuffer != NULL) {
			dma_free_coherent(&tp->platform_dev->dev,
					  SHORT_PACKET_PADDING_BUF_SIZE,
					  tp->ShortPacketEmptyBuffer,
					  tp->ShortPacketEmptyBufferPhy);
			tp->ShortPacketEmptyBuffer = NULL;
		}
	} else {
		spin_lock_irqsave(&tp->lock, flags);

		rtl8168_hw_d3_para(dev);

		rtl8168_powerdown_pll(dev);

		spin_unlock_irqrestore(&tp->lock, flags);
	}

	return 0;
}

static void rtl8168_shutdown(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct rtl8168_private *tp = netdev_priv(dev);

	if (HW_DASH_SUPPORT_DASH(tp))
		rtl8168_driver_stop(tp);

	if (s5_keep_curr_mac == 0 && tp->random_mac == 0)
		rtl8168_rar_set(tp, tp->org_mac_addr);

#ifdef ENABLE_FIBER_SUPPORT
	rtl8168_hw_fiber_nic_d3_para(dev);
#endif //ENABLE_FIBER_SUPPORT

	if (s5wol == 0)
		tp->wol_enabled = WOL_DISABLED;

	rtl8168_close(dev);
}

/**
 *	rtl8168_get_stats - Get rtl8168 read/write statistics
 *	@dev: The Ethernet Device to get statistics for
 *
 *	Get TX/RX statistics for rtl8168
 */
static struct net_device_stats *rtl8168_get_stats(struct net_device *dev)
{
	return &RTLDEV->stats;
}

#ifdef CONFIG_PM

static int rtl8168_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	if (!netif_running(dev))
		goto out;

	rtl8168_cancel_schedule_work(dev);

	rtl8168_delete_link_timer(dev, &tp->link_timer);

	netif_stop_queue(dev);

	netif_carrier_off(dev);

	netif_device_detach(dev);

	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_dsm(dev, DSM_NIC_GOTO_D3);

	rtl8168_hw_reset(dev);

	rtl8168_sleep_rx_enable(dev);

	rtl8168_hw_d3_para(dev);

#ifdef ENABLE_FIBER_SUPPORT
	rtl8168_hw_fiber_nic_d3_para(dev);
#endif //ENABLE_FIBER_SUPPORT

	rtl8168_powerdown_pll(dev);

	spin_unlock_irqrestore(&tp->lock, flags);

out:
	if (HW_DASH_SUPPORT_DASH(tp)) {
		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_driver_stop(tp);
		spin_unlock_irqrestore(&tp->lock, flags);
	}

	return 0;
}

static int rtl8168_resume(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct rtl8168_private *tp = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&tp->lock, flags);

	/* restore last modified mac address */
	rtl8168_rar_set(tp, dev->dev_addr);

	spin_unlock_irqrestore(&tp->lock, flags);

	if (!netif_running(dev))
		goto out;

	spin_lock_irqsave(&tp->lock, flags);

	rtl8168_exit_oob(dev);

	rtl8168_dsm(dev, DSM_NIC_RESUME_D3);

	rtl8168_hw_init(dev);

	rtl8168_powerup_pll(dev);

	rtl8168_hw_ephy_config(dev);

	rtl8168_hw_phy_config(dev);

	spin_unlock_irqrestore(&tp->lock, flags);

	rtl8168_schedule_work(dev, rtl8168_reset_task);

	netif_device_attach(dev);

	mod_timer(&tp->link_timer, jiffies + RTL8168_LINK_TIMEOUT);
out:
	return 0;
}

#endif /* CONFIG_PM */
static const struct of_device_id rtl8168_dt_ids[] = {
	{
		.compatible = "realtek,rts493xa-r8168",
	},
	{ /* sentinel */ },
};

static struct platform_driver rtl8168_platform_driver = {
	.driver		= {
			.name	= MODULENAME,
			.owner	= THIS_MODULE,
			.of_match_table	= of_match_ptr(rtl8168_dt_ids),
	},
	.probe		= rtl8168_init_one,
	.remove		= __devexit_p(rtl8168_remove_one),
	.shutdown	= rtl8168_shutdown,
#ifdef CONFIG_PM
	.suspend	= rtl8168_suspend,
	.resume		= rtl8168_resume,
#endif
};

static int __init rtl8168_init_module(void)
{
#ifdef ENABLE_R8168_PROCFS
	rtl8168_proc_module_init();
#endif
	return platform_driver_register(&rtl8168_platform_driver);
}

static void __exit rtl8168_cleanup_module(void)
{
	platform_driver_unregister(&rtl8168_platform_driver);
#ifdef ENABLE_R8168_PROCFS
	if (rtl8168_proc) {
		remove_proc_subtree(MODULENAME, init_net.proc_net);
		rtl8168_proc = NULL;
	}
#endif
}

module_init(rtl8168_init_module);
module_exit(rtl8168_cleanup_module);
