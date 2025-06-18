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

#ifndef __RTS_RSA_H__
#define __RTS_RSA_H__

#include <linux/bitops.h>

#define RLX_REG_RSA_MODE       0x00
#define RLX_REG_RSA_MODE_SHIFT 0x04
#define RLX_REG_RSA_CTRL       0x08
#define RLX_REG_RSA_NP_INV     0x0C
#define RLX_REG_RSA_INT_CTRL   0x10
#define RLX_REG_RSA_INT_STATUS 0x14
#define RLX_REG_RSA_MEM0       0x1000

#define RLX_RSA_RAM_PCTEXT_BASE_ADDR 0x1000
#define RLX_RSA_RAM_RRMODN_BASE_ADDR 0x1180
#define RLX_RSA_RAM_NKEY_BASE_ADDR   0x1480
#define RLX_RSA_RAM_DEKEY_BASE_ADDR  0x1600

/* RSA MODE */
#define RLX_RSA_MODE_NBITS_MASK (BIT(0) | BIT(1))
#define RLX_RSA_MODE_NBITS_3072 0x00
#define RLX_RSA_MODE_NBITS_2048 0x01
#define RLX_RSA_MODE_NBITS_1024 0x02
#define RLX_RSA_MODE_NBITS_512	0x03

#define RLX_RSA_MODE_SEL_MASK	   (BIT(2) | BIT(3))
#define RLX_RSA_MODE_SEL_IDLE	   (0x00 << 2)
#define RLX_RSA_MODE_SEL_WRITE_KEY (0x01 << 2)
#define RLX_RSA_MODE_SEL_READ_MSG  (0x02 << 2)
#define RLX_RSA_MODE_SEL_CALC	   (0x03 << 2)

#define RLX_RSA_MODE_MODN_MASK	      BIT(4)
#define RLX_RSA_MODE_MODN_ORIG_FUNC   0x00
#define RLX_RSA_MODE_MODN_CALC_RRMODN BIT(4)

/* RSA CTRL */
#define RLX_RSA_CTRL_READ_RRMODN BIT(2)
#define RLX_RSA_CTRL_DONE	 BIT(1)
#define RLX_RSA_CTRL_START	 BIT(0)

/* RSA INT CTL */
#define RLX_RSA_INT_CTL_MASK BIT(0)
#define RLX_RSA_INT_CTL_EN   BIT(0)

/* RSA INT STATUS */
#define RLX_RSA_INT_STATUS_DONE BIT(0)

#endif
