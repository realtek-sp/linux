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

#ifndef _DT_BINDINGS_IRQ_RTS493XA_H
#define _DT_BINDINGS_IRQ_RTS493XA_H

#define BSP_IRQ_TIMER0	 (0) /* XB2 timer0 */
#define BSP_IRQ_TIMER1	 (1) /* XB2 timer1 */
#define BSP_IRQ_ETH	 (15) /* Ethernet */
#define BSP_IRQ_EHCI	 (16) /* EHCI Controller */
#define BSP_IRQ_OHCI	 (17) /* OHCI Controller */
#define BSP_IRQ_USB_DEV	 (18) /* USB Device */
#define BSP_IRQ_USB_PHY	 (19) /* USB PHY */
#define BSP_IRQ_SDIO0	 (20) /* SDIO1 */
#define BSP_IRQ_SDIO1	 (21) /* SDIO2 */
#define BSP_IRQ_DMA	 (22) /* DMA Controller */
#define BSP_IRQ_AES	 (23) /* AES */
#define BSP_IRQ_RSA	 (24) /* RSA */
#define BSP_IRQ_SHA	 (25) /* SHA */
#define BSP_IRQ_TRNG	 (26) /* TRNG */
#define BSP_IRQ_SM0	 (27) /* SM2 */
#define BSP_IRQ_SM1	 (28) /* SM3 */
#define BSP_IRQ_SM2	 (29) /* SM4 */
#define BSP_IRQ_RXI	 (30) /* RXI */
#define BSP_IRQ_WATCHDOG (31) /* Watchdog */
#define BSP_IRQ_SPI_NOR	 (32) /* SPI NOR */
#define BSP_IRQ_SPI_NAND (33) /* SPI NAND */
#define BSP_IRQ_SSI	 (34) /* SSI */
#define BSP_IRQ_I2C0	 (35) /* I2CO */
#define BSP_IRQ_I2C1	 (36) /* I2C1 */
#define BSP_IRQ_UART0	 (37) /* UART0 */
#define BSP_IRQ_UART1	 (38) /* UART1 */
#define BSP_IRQ_UART2	 (39) /* UART2 */
#define BSP_IRQ_GPIO	 (40) /* GPIO */
#define BSP_IRQ_THERMAL	 (41) /* THERMAL */
#define BSP_IRQ_OTHER	 (42) /* OTHER */

#endif