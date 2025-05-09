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

#ifndef _LINUX_RTLTOOL_H
#define _LINUX_RTLTOOL_H

#define SIOCRTLTOOL SIOCDEVPRIVATE + 1

enum rtl_cmd {
	RTLTOOL_READ_MAC = 0,
	RTLTOOL_WRITE_MAC,
	RTLTOOL_READ_PHY,
	RTLTOOL_WRITE_PHY,
	RTLTOOL_READ_EPHY,
	RTLTOOL_WRITE_EPHY,
	RTLTOOL_READ_ERI,
	RTLTOOL_WRITE_ERI,
	RTLTOOL_READ_PCI,
	RTLTOOL_WRITE_PCI,
	RTLTOOL_READ_EEPROM,
	RTLTOOL_WRITE_EEPROM,

	RTL_READ_OOB_MAC,
	RTL_WRITE_OOB_MAC,

	RTL_ENABLE_PCI_DIAG,
	RTL_DISABLE_PCI_DIAG,

	RTL_READ_MAC_OCP,
	RTL_WRITE_MAC_OCP,

	RTL_DIRECT_READ_PHY_OCP,
	RTL_DIRECT_WRITE_PHY_OCP,

	RTLTOOL_INVALID
};

struct rtltool_cmd {
	__u32 cmd;
	__u32 offset;
	__u32 len;
	__u32 data;
};

enum mode_access { MODE_NONE = 0, MODE_READ, MODE_WRITE };

#ifdef __KERNEL__
int rtl8168_tool_ioctl(struct rtl8168_private *tp, struct ifreq *ifr);
#endif

#endif /* _LINUX_RTLTOOL_H */
