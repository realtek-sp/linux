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

#ifndef DT_BINDINGS_SYSMEM_RTS493XA_H
#define DT_BINDINGS_SYSMEM_RTS493XA_H

#define SYS_MEM_SD_NAND_SPIC 0
#define SYS_MEM_SD_ETH	     1
#define SYS_MEM_SD_CIPHER    2
#define SYS_MEM_SD_U2DEV     3
#define SYS_MEM_SD_SDIO0     4
#define SYS_MEM_SD_SDIO1     5
#define SYS_MEM_SD_RSA	     6
#define SYS_MEM_MAX	     (SYS_MEM_SD_RSA + 1)

#endif
