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

#ifndef _RLX_CRYPTO_H_
#define _RLX_CRYPTO_H_

#define RLX_REG_KEY_DATA0	     0x00
#define RLX_REG_KEY_DATA1	     0x04
#define RLX_REG_KEY_DATA2	     0x08
#define RLX_REG_KEY_DATA3	     0x0C
#define RLX_REG_KEY_DATA4	     0x10
#define RLX_REG_KEY_DATA5	     0x14
#define RLX_REG_KEY_DATA6	     0x18
#define RLX_REG_KEY_DATA7	     0x1C
#define RLX_REG_IV_IN_DATA0	     0x20
#define RLX_REG_IV_IN_DATA1	     0x24
#define RLX_REG_IV_IN_DATA2	     0x28
#define RLX_REG_IV_IN_DATA3	     0x2C
#define RLX_REG_IV_OUT_DATA0	     0x30
#define RLX_REG_IV_OUT_DATA1	     0x34
#define RLX_REG_IV_OUT_DATA2	     0x38
#define RLX_REG_IV_OUT_DATA3	     0x3C
#define RLX_REG_IN_TABLE_ADDR	     0x40
#define RLX_REG_IN_BUF_NUM	     0x44
#define RLX_REG_OUT_TABLE_ADDR	     0x48
#define RLX_REG_OUT_BUF_NUM	     0x4C
#define RLX_REG_CIPHER_CTL	     0x50
#define RLX_REG_CIPHER_INT_EN	     0x54
#define RLX_REG_CIPHER_INT_FLAG	     0x58
#define RLX_REG_CIPHER_STS	     0x5C
#define RLX_REG_FINISH_BLOCK_NUM     0x60
#define RLX_REG_DATA_IN_LENGTH	     0x64
#define RLX_REG_CCM_NA_GCM_A_LENGTH  0x70
#define RLX_REG_CCM_N_GCM_IV_LENGTH  0x74
#define RLX_REG_CCM_CTR_FLAG	     0x78
#define RLX_REG_MAC_OUT_DATA0	     0x80
#define RLX_REG_MAC_OUT_DATA1	     0x84
#define RLX_REG_MAC_OUT_DATA2	     0x88
#define RLX_REG_MAC_OUT_DATA3	     0x8C
#define RLX_REG_CCM_NA_GCM_A_BUF_NUM 0xA4
#define RLX_REG_GCM_IV_BUF_NUM	     0xA8

#define RLX_REG_CIPHER_KEY 0x200

/* RLX_REG_CIPHER_CTL */
#define RLX_AES_MODE_SEL	10
#define RLX_ALGORITHM_SEL	8
#define RLX_OPERATION_MODE	5
#define RLX_PADDING_SEL		4
#define RLX_ENCTYPT_DECTYPT_SEL 3
#define RLX_BUSY_FLAG		2
#define RLX_STOP_CIPHER		1
#define RLX_START_CIPHER	0

#define AES_BLOCK_SIZE 16
#define DES_BLOCK_SIZE 8

#endif
