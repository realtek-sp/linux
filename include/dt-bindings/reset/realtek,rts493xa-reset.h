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

#ifndef DT_BINDINGS_RESET_RTS493XA_H
#define DT_BINDINGS_RESET_RTS493XA_H

#define FORCE_RESET_CIPHER   0
#define FORCE_RESET_SDIO0    1
#define FORCE_RESET_SDIO1    2
#define FORCE_RESET_U2DEV    3
#define FORCE_RESET_U2HOST   4
#define FORCE_RESET_ETHERNET 5
#define FORCE_RESET_RSA	     6
#define FORCE_RESET_SHA256   7
#define FORCE_RESET_TRNG     8
#define FORCE_RESET_FEPHY    9
#define FORCE_RESET_OTP	     10
#define FORCE_RESET_UART0    11
#define FORCE_RESET_UART1    12
#define FORCE_RESET_UART2    13
#define FORCE_RESET_I2C0     14
#define FORCE_RESET_I2C1     15
#define FORCE_RESET_MAX	     (FORCE_RESET_I2C1 + 1)

#endif
