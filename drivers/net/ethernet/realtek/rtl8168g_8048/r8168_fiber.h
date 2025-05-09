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

/************************************************************************************
 *	This product is covered by one or more of the following patents:
 *	US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

#ifndef _LINUX_R8168_FIBER_H
#define _LINUX_R8168_FIBER_H

enum {
	FIBER_MODE_NIC_ONLY = 0,
	FIBER_MODE_RTL8168H_RTL8211FS,
	FIBER_MODE_RTL8168H_MDI_SWITCH_RTL8211FS,
	FIBER_MODE_MAX
};

enum {
	FIBER_STAT_NOT_CHECKED = 0,
	FIBER_STAT_CONNECT,
	FIBER_STAT_DISCONNECT,
	FIBER_STAT_MAX
};

#define HW_FIBER_MODE_ENABLED(_M) ((_M)->HwFiberModeVer > 0)

void rtl8168_hw_init_fiber_nic(struct net_device *dev);
void rtl8168_hw_fiber_nic_d3_para(struct net_device *dev);
void rtl8168_hw_fiber_phy_config(struct net_device *dev);
void rtl8168_hw_switch_mdi_to_fiber(struct net_device *dev);
void rtl8168_hw_switch_mdi_to_nic(struct net_device *dev);
unsigned int rtl8168_hw_fiber_link_ok(struct net_device *dev);
void rtl8168_check_fiber_link_status(struct net_device *dev);
void rtl8168_check_hw_fiber_mode_support(struct net_device *dev);
void rtl8168_set_fiber_mode_software_variable(struct net_device *dev);

#endif /* _LINUX_R8168_FIBER_H */
