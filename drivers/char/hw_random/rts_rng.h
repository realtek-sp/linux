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

#ifndef _RLX_RNG_H_
#define _RLX_RNG_H_

#include <linux/bitops.h>

#define RLX_REG_TRNG_START   0x00
#define RLX_REG_TRNG_CTL     0x08
#define RLX_REG_TRNG_IRQ_EN  0x0C
#define RLX_REG_TRNG_IRQ     0x10
#define RLX_REG_TRNG_SRC_SEL 0x14
#define RLX_REG_TRNG_RESUTL0 0x18
#define RLX_REG_TRNG_RESUTL1 0x1C
#define RLX_REG_TRNG_NEU_EN  0x20

/* RLX_REG_TRNG_IRQ_EN */
#define RLX_TRNG_GEN_DONE_INT_EN BIT(0)

/* RLX_REG_TRNG_IRQ */
#define RLX_TRNG_GEN_DONE_INT BIT(0)

/* RLX_REG_TRNG_CTL */
#define RLX_TRNG_CFG_EN7  BIT(0)
#define RLX_TRNG_CFG_EN13 BIT(1)
#define RLX_TRNG_CFG_EN17 BIT(2)
#define RLX_TRNG_CFG_EN23 BIT(3)

#endif
