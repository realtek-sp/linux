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

#ifndef __DRIVERS_USB_REALTEK_MC_REGS_H__
#define __DRIVERS_USB_REALTEK_MC_REGS_H__

#define R_EP0_MC_BUF_CTL       0x0000
#define R_EP0_MC_BUF_BC	       0x0004
#define R_MC_SID	       0x0008
#define R_MC_DUMMY0	       0x000C
#define R_MC_DEV_CFG	       0x0010
// #define R_EP0_MC_BUF_IRQ_EN 0x0014
// #define R_EP0_MC_BUF_IRQ_STATUS 0x0018
#define MC_FIFO0_CTRL	       0x0100
#define MC_FIFO0_BC	       0x0104
#define MC_FIFO0_STAT	       0x0108
#define MC_FIFO0_MODE	       0x010C
#define MC_FIFO0_RD_PTR	       0x0110
#define MC_FIFO0_WR_PTR	       0x0114
#define MC_FIFO0_DMA_CTRL      0x0118
#define MC_FIFO0_DMA_LENGTH    0x011C
#define MC_FIFO0_DMA_ADDR      0x0120
#define MC_FIFO0_IRQ	       0x0128
#define MC_FIFO0_IRQ_EN	       0x012C
#define MC_FIFO0_DMA_STOP      0x0130
#define MC_FIFO0_DMAOUT_LENGTH 0x0134

#define EP0_BASE 0x1000

/* R_EP0_MC_BUF_CTL 0x0000 */
#define U_BUF0_EP0_RX_EN_OFFSET	       0
#define U_BUF0_EP0_RX_EN_BITS	       1
#define U_BUF0_EP0_RX_EN_MASK	       (((1 << 1) - 1) << 0)
#define U_BUF0_EP0_TX_EN_OFFSET	       1
#define U_BUF0_EP0_TX_EN_BITS	       1
#define U_BUF0_EP0_TX_EN_MASK	       (((1 << 1) - 1) << 1)
/* R_EP0_MC_BUF_BC 0x0004 */
#define U_BUF0_TX_BC_OFFSET	       0
#define U_BUF0_TX_BC_BITS	       16
#define U_BUF0_TX_BC_MASK	       (((1 << 16) - 1) << 0)
#define U_BUF0_RX_BC_OFFSET	       16
#define U_BUF0_RX_BC_BITS	       16
#define U_BUF0_RX_BC_MASK	       (((1 << 16) - 1) << 16)
/* R_MC_SID 0x0008 */
#define U_BUF0_SID_OFFSET	       0
#define U_BUF0_SID_BITS		       16
#define U_BUF0_SID_MASK		       (((1 << 16) - 1) << 0)
#define U_FIFO_SID_OFFSET	       16
#define U_FIFO_SID_BITS		       16
#define U_FIFO_SID_MASK		       (((1 << 16) - 1) << 16)
/* R_MC_DUMMY0 0x000C */
/* R_MC_DEV_CFG 0x0010 */
#define CFG_U2DEV_SEL_OFFSET	       0
#define CFG_U2DEV_SEL_BITS	       1
#define CFG_U2DEV_SEL_MASK	       (((1 << 1) - 1) << 0)
// /* R_EP0_MC_BUF_IRQ_EN 0x0014 */
// #define IE_EP0OUT_OFFSET 0
// #define IE_EP0OUT_BITS 1
// #define IE_EP0OUT_MASK (((1 << 1) - 1) << 0)
// #define IE_EP0IN_OFFSET 1
// #define IE_EP0IN_BITS 1
// #define IE_EP0IN_MASK (((1 << 1) - 1) << 1)
// /* R_EP0_MC_BUF_IRQ_STATUS 0x0018 */
// #define I_EP0OUT_OFFSET 0
// #define I_EP0OUT_BITS 1
// #define I_EP0OUT_MASK (((1 << 1) - 1) << 0)
// #define I_EP0IN_OFFSET 1
// #define I_EP0IN_BITS 1
// #define I_EP0IN_MASK (((1 << 1) - 1) << 1)
/* MC_FIFO0_CTRL 0x0100 */
#define U_FIFO_FLUSH_OFFSET	       0
#define U_FIFO_FLUSH_BITS	       1
#define U_FIFO_FLUSH_MASK	       (((1 << 1) - 1) << 0)
#define U_FIFO_VALID_OFFSET	       1
#define U_FIFO_VALID_BITS	       1
#define U_FIFO_VALID_MASK	       (((1 << 1) - 1) << 1)
/* MC_FIFO0_BC 0x0104 */
#define U_FIFO_BC_OFFSET	       0
#define U_FIFO_BC_BITS		       16
#define U_FIFO_BC_MASK		       (((1 << 16) - 1) << 0)
/* MC_FIFO0_STAT 0x0108 */
#define U_FIFO_NOT_EMPTY_OFFSET	       0
#define U_FIFO_NOT_EMPTY_BITS	       1
#define U_FIFO_NOT_EMPTY_MASK	       (((1 << 1) - 1) << 0)
#define U_FIFO_EMPTY_OFFSET	       1
#define U_FIFO_EMPTY_BITS	       1
#define U_FIFO_EMPTY_MASK	       (((1 << 1) - 1) << 1)
#define U_FIFO_FULL_OFFSET	       2
#define U_FIFO_FULL_BITS	       1
#define U_FIFO_FULL_MASK	       (((1 << 1) - 1) << 2)
#define U_FIFO_UNDERFLOW_OFFSET	       4
#define U_FIFO_UNDERFLOW_BITS	       1
#define U_FIFO_UNDERFLOW_MASK	       (((1 << 1) - 1) << 4)
#define U_FIFO_OVERFLOW_OFFSET	       5
#define U_FIFO_OVERFLOW_BITS	       1
#define U_FIFO_OVERFLOW_MASK	       (((1 << 1) - 1) << 5)
/* MC_FIFO0_MODE 0x010C */
#define U_FIFO_RX_EN_OFFSET	       0
#define U_FIFO_RX_EN_BITS	       1
#define U_FIFO_RX_EN_MASK	       (((1 << 1) - 1) << 0)
#define U_FIFO_TX_EN_OFFSET	       1
#define U_FIFO_TX_EN_BITS	       1
#define U_FIFO_TX_EN_MASK	       (((1 << 1) - 1) << 1)
/* MC_FIFO0_RD_PTR 0x0110 */
#define U_FIFO_RD_PTR_OFFSET	       0
#define U_FIFO_RD_PTR_BITS	       16
#define U_FIFO_RD_PTR_MASK	       (((1 << 16) - 1) << 0)
/* MC_FIFO0_WR_PTR 0x0114 */
#define U_FIFO_WR_PTR_OFFSET	       0
#define U_FIFO_WR_PTR_BITS	       16
#define U_FIFO_WR_PTR_MASK	       (((1 << 16) - 1) << 0)
/* MC_FIFO0_DMA_CTRL 0x0118 */
#define U_PE_TRANS_EN_OFFSET	       0
#define U_PE_TRANS_EN_BITS	       1
#define U_PE_TRANS_EN_MASK	       (((1 << 1) - 1) << 0)
#define U_PE_TRANS_DIR_OFFSET	       1
#define U_PE_TRANS_DIR_BITS	       1
#define U_PE_TRANS_DIR_MASK	       (((1 << 1) - 1) << 1)
#define U_PE_TRANS_TRIG_THD_OFFSET     2
#define U_PE_TRANS_TRIG_THD_BITS       5
#define U_PE_TRANS_TRIG_THD_MASK       (((1 << 5) - 1) << 2)
/* MC_FIFO0_DMA_LENGTH 0x011C */
/* MC_FIFO0_DMA_ADDR 0x0120 */
/* MC_FIFO0_IRQ 0x0128 */
#define INT_DMA_DONE_OFFSET	       0
#define INT_DMA_DONE_BITS	       1
#define INT_DMA_DONE_MASK	       (((1 << 1) - 1) << 0)
#define INT_LASTPKT_START_OFFSET       1
#define INT_LASTPKT_START_BITS	       1
#define INT_LASTPKT_START_MASK	       (((1 << 1) - 1) << 1)
#define INT_LASTPKT_DONE_OFFSET	       2
#define INT_LASTPKT_DONE_BITS	       1
#define INT_LASTPKT_DONE_MASK	       (((1 << 1) - 1) << 2)
#define INT_FIFO_UNDERFLOW_OFFSET      3
#define INT_FIFO_UNDERFLOW_BITS	       1
#define INT_FIFO_UNDERFLOW_MASK	       (((1 << 1) - 1) << 3)
#define INT_FIFO_OVERFLOW_OFFSET       4
#define INT_FIFO_OVERFLOW_BITS	       1
#define INT_FIFO_OVERFLOW_MASK	       (((1 << 1) - 1) << 4)
#define INT_FIFO_RD_CONFLICT_OFFSET    5
#define INT_FIFO_RD_CONFLICT_BITS      1
#define INT_FIFO_RD_CONFLICT_MASK      (((1 << 1) - 1) << 5)
#define INT_FIFO_WR_CONFLICT_OFFSET    6
#define INT_FIFO_WR_CONFLICT_BITS      1
#define INT_FIFO_WR_CONFLICT_MASK      (((1 << 1) - 1) << 6)
#define INT_FIFO_NOEMPTY_OFFSET	       7
#define INT_FIFO_NOEMPTY_BITS	       1
#define INT_FIFO_NOEMPTY_MASK	       (((1 << 1) - 1) << 7)
#define INT_FIFO_EMPTY_OFFSET	       8
#define INT_FIFO_EMPTY_BITS	       1
#define INT_FIFO_EMPTY_MASK	       (((1 << 1) - 1) << 8)
#define INT_FIFO_FULL_OFFSET	       9
#define INT_FIFO_FULL_BITS	       1
#define INT_FIFO_FULL_MASK	       (((1 << 1) - 1) << 9)
/* MC_FIFO0_IRQ_EN 0x012C */
#define INT_DMA_DONE_EN_OFFSET	       0
#define INT_DMA_DONE_EN_BITS	       1
#define INT_DMA_DONE_EN_MASK	       (((1 << 1) - 1) << 0)
#define INT_LASTPKT_START_EN_OFFSET    1
#define INT_LASTPKT_START_EN_BITS      1
#define INT_LASTPKT_START_EN_MASK      (((1 << 1) - 1) << 1)
#define INT_LASTPKT_DONE_EN_OFFSET     2
#define INT_LASTPKT_DONE_EN_BITS       1
#define INT_LASTPKT_DONE_EN_MASK       (((1 << 1) - 1) << 2)
#define INT_FIFO_UNDERFLOW_EN_OFFSET   3
#define INT_FIFO_UNDERFLOW_EN_BITS     1
#define INT_FIFO_UNDERFLOW_EN_MASK     (((1 << 1) - 1) << 3)
#define INT_FIFO_OVERFLOW_EN_OFFSET    4
#define INT_FIFO_OVERFLOW_EN_BITS      1
#define INT_FIFO_OVERFLOW_EN_MASK      (((1 << 1) - 1) << 4)
#define INT_FIFO_RD_CONFLICT_EN_OFFSET 5
#define INT_FIFO_RD_CONFLICT_EN_BITS   1
#define INT_FIFO_RD_CONFLICT_EN_MASK   (((1 << 1) - 1) << 5)
#define INT_FIFO_WR_CONFLICT_EN_OFFSET 6
#define INT_FIFO_WR_CONFLICT_EN_BITS   1
#define INT_FIFO_WR_CONFLICT_EN_MASK   (((1 << 1) - 1) << 6)
#define INT_FIFO_NOEMPTY_EN_OFFSET     7
#define INT_FIFO_NOEMPTY_EN_BITS       1
#define INT_FIFO_NOEMPTY_EN_MASK       (((1 << 1) - 1) << 7)
#define INT_FIFO_EMPTY_EN_OFFSET       8
#define INT_FIFO_EMPTY_EN_BITS	       1
#define INT_FIFO_EMPTY_EN_MASK	       (((1 << 1) - 1) << 8)
#define INT_FIFO_FULL_EN_OFFSET	       9
#define INT_FIFO_FULL_EN_BITS	       1
#define INT_FIFO_FULL_EN_MASK	       (((1 << 1) - 1) << 9)
/* MC_FIFO0_DMA_STOP 0x0130 */
#define MCM_STOP_TRANS_OFFSET	       0
#define MCM_STOP_TRANS_BITS	       1
#define MCM_STOP_TRANS_MASK	       (((1 << 1) - 1) << 0)
/* MC_FIFO0_DMAOUT_LENGTH 0x0134 */

#endif /* __DRIVERS_USB_REALTEK_MC_REGS_H__ */
