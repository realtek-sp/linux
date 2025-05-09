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

#include <linux/module.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/in.h>
#include <linux/ethtool.h>
#include <asm/uaccess.h>
#include "r8168.h"
#include "rtl_eeprom.h"
#include "rtltool.h"

int rtl8168_tool_ioctl(struct rtl8168_private *tp, struct ifreq *ifr)
{
	struct rtltool_cmd my_cmd;
	unsigned long flags;
	int ret;

	if (copy_from_user(&my_cmd, ifr->ifr_data, sizeof(my_cmd)))
		return -EFAULT;

	ret = 0;
	switch (my_cmd.cmd) {
	case RTLTOOL_READ_MAC:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (my_cmd.len == 1)
			my_cmd.data = readb(tp->mmio_addr + my_cmd.offset);
		else if (my_cmd.len == 2)
			my_cmd.data =
				readw(tp->mmio_addr + (my_cmd.offset & ~1));
		else if (my_cmd.len == 4)
			my_cmd.data =
				readl(tp->mmio_addr + (my_cmd.offset & ~3));
		else {
			ret = -EOPNOTSUPP;
			break;
		}

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}
		break;

	case RTLTOOL_WRITE_MAC:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (my_cmd.len == 1)
			writeb(my_cmd.data, tp->mmio_addr + my_cmd.offset);
		else if (my_cmd.len == 2)
			writew(my_cmd.data,
			       tp->mmio_addr + (my_cmd.offset & ~1));
		else if (my_cmd.len == 4)
			writel(my_cmd.data,
			       tp->mmio_addr + (my_cmd.offset & ~3));
		else {
			ret = -EOPNOTSUPP;
			break;
		}

		break;

	case RTLTOOL_READ_PHY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		my_cmd.data = rtl8168_mdio_read(tp, my_cmd.offset);
		spin_unlock_irqrestore(&tp->lock, flags);

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}

		break;

	case RTLTOOL_WRITE_PHY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_mdio_prot_write(tp, my_cmd.offset, my_cmd.data);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;

	case RTLTOOL_READ_EPHY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		my_cmd.data = rtl8168_ephy_read(tp, my_cmd.offset);
		spin_unlock_irqrestore(&tp->lock, flags);

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}

		break;

	case RTLTOOL_WRITE_EPHY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_ephy_write(tp, my_cmd.offset, my_cmd.data);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;

	case RTLTOOL_READ_ERI:
		my_cmd.data = 0;
		if (my_cmd.len == 1 || my_cmd.len == 2 || my_cmd.len == 4) {
			spin_lock_irqsave(&tp->lock, flags);
			my_cmd.data = rtl8168_eri_read(
				tp, my_cmd.offset, my_cmd.len, ERIAR_ExGMAC);
			spin_unlock_irqrestore(&tp->lock, flags);
		} else {
			ret = -EOPNOTSUPP;
			break;
		}

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}

		break;

	case RTLTOOL_WRITE_ERI:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (my_cmd.len == 1 || my_cmd.len == 2 || my_cmd.len == 4) {
			spin_lock_irqsave(&tp->lock, flags);
			rtl8168_eri_write(tp, my_cmd.offset, my_cmd.len,
					  my_cmd.data, ERIAR_ExGMAC);
			spin_unlock_irqrestore(&tp->lock, flags);
		} else {
			ret = -EOPNOTSUPP;
			break;
		}
		break;

	case RTLTOOL_READ_EEPROM:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		my_cmd.data = rtl8168_eeprom_read_sc(tp, my_cmd.offset);
		spin_unlock_irqrestore(&tp->lock, flags);

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}

		break;

	case RTLTOOL_WRITE_EEPROM:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_eeprom_write_sc(tp, my_cmd.offset, my_cmd.data);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;

	case RTL_READ_OOB_MAC:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_oob_mutex_lock(tp);
		my_cmd.data = rtl8168_ocp_read(tp, my_cmd.offset, 4);
		rtl8168_oob_mutex_unlock(tp);
		spin_unlock_irqrestore(&tp->lock, flags);

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}
		break;

	case RTL_WRITE_OOB_MAC:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (my_cmd.len == 0 || my_cmd.len > 4)
			return -EOPNOTSUPP;

		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_oob_mutex_lock(tp);
		rtl8168_ocp_write(tp, my_cmd.offset, my_cmd.len, my_cmd.data);
		rtl8168_oob_mutex_unlock(tp);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;

	case RTL_ENABLE_PCI_DIAG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		tp->rtk_enable_diag = 1;
		spin_unlock_irqrestore(&tp->lock, flags);

		dprintk("enable rtk diag\n");
		break;

	case RTL_DISABLE_PCI_DIAG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		tp->rtk_enable_diag = 0;
		spin_unlock_irqrestore(&tp->lock, flags);

		dprintk("disable rtk diag\n");
		break;

	case RTL_READ_MAC_OCP:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (my_cmd.offset % 2)
			return -EOPNOTSUPP;

		spin_lock_irqsave(&tp->lock, flags);
		my_cmd.data = rtl8168_mac_ocp_read(tp, my_cmd.offset);
		spin_unlock_irqrestore(&tp->lock, flags);

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}
		break;

	case RTL_WRITE_MAC_OCP:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if ((my_cmd.offset % 2) || (my_cmd.len != 2))
			return -EOPNOTSUPP;

		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_mac_ocp_write(tp, my_cmd.offset, (u16)my_cmd.data);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;

	case RTL_DIRECT_READ_PHY_OCP:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		my_cmd.data = rtl8168_mdio_prot_direct_read_phy_ocp(
			tp, my_cmd.offset);
		spin_unlock_irqrestore(&tp->lock, flags);

		if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
			ret = -EFAULT;
			break;
		}

		break;

	case RTL_DIRECT_WRITE_PHY_OCP:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_irqsave(&tp->lock, flags);
		rtl8168_mdio_prot_direct_write_phy_ocp(tp, my_cmd.offset,
						       my_cmd.data);
		spin_unlock_irqrestore(&tp->lock, flags);
		break;

	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
