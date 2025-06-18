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

#ifndef _RLX_SHA_H_
#define _RLX_SHA_H_

#include <linux/bitops.h>

#define RLX_REG_SHA_CTL		0x00
#define RLX_REG_SHA_IRQ_EN	0x04
#define RLX_REG_SHA_IRQ_FLAG	0x08
#define RLX_REG_SHA_DATA_LEN	0x0C
#define RLX_REG_SHA_DATA_ADDR	0x10
#define RLX_REG_HMAC_IPAD	0x14
#define RLX_REG_HMAC_OPAD	0x18
#define RLX_REG_SHA_TOTAL_LEN	0x20
#define RLX_REG_SHA_PADDING_CTL 0x24
#define RLX_REG_SHA_SET_IV	0x28
#define RLX_REG_HMAC_MODE	0x2C
#define RLX_REG_HMAC_KEY	0x60
#define RLX_REG_SHA_IV_N0	0xB0
#define RLX_REG_SHA_IV_N1	0xB4
#define RLX_REG_SHA_IV_N2	0xB8
#define RLX_REG_SHA_IV_N3	0xBC
#define RLX_REG_SHA_IV_N4	0xC0
#define RLX_REG_SHA_IV_N5	0xC4
#define RLX_REG_SHA_IV_N6	0xC8
#define RLX_REG_SHA_IV_N7	0xCC
#define RLX_REG_SHA_RESULT_N0	0xE0
#define RLX_REG_SHA_RESULT_N1	0xE4
#define RLX_REG_SHA_RESULT_N2	0xE8
#define RLX_REG_SHA_RESULT_N3	0xEC
#define RLX_REG_SHA_RESULT_N4	0xF0
#define RLX_REG_SHA_RESULT_N5	0xF4
#define RLX_REG_SHA_RESULT_N6	0xF8
#define RLX_REG_SHA_RESULT_N7	0xFC

/* RLX_REG_SHA_CTL */
#define RLX_SHA256_DONE	  BIT(2)
#define RLX_REG_WPTR_RET  BIT(1)
#define RLX_CFG_SHA_START BIT(0)

/* RLX_REG_SHA_IRQ_EN */
#define RLX_SHA256_DONE_INT_EN BIT(0)

/* RLX_REG_SHA_IRQ_FLAG	*/
#define RLX_SHA256_DONE_INT BIT(0)

/* RLX_REG_SHA_PADDING_CTL */
#define RLX_SHA_PADDIND_EN BIT(0)

/*  RLX_REG_SHA_SET_IV */
#define RLX_SHA_SET_IV BIT(0)

#endif
