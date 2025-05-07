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

#ifndef __DT_BINDINGS_CLOCK_RTS493XA_H
#define __DT_BINDINGS_CLOCK_RTS493XA_H

#define RLX_CLK_DUMMY		  0
#define RLX_CLK_SYS_OSC		  1
#define RLX_CLK_USB_PLL		  2
#define RLX_CLK_USB_PLL_2	  3
#define RLX_CLK_USB_PLL_3	  4
#define RLX_CLK_USB_PLL_5	  5
#define RLX_CLK_USB_PLL_7	  6
#define RLX_CLK_SYS_PLL0	  7
#define RLX_CLK_SYS_PLL1	  8
#define RLX_CLK_SYS_PLL2	  9
#define RLX_CLK_SYS_PLL3	  10
#define RLX_CLK_SYS_PLL0_2	  11
#define RLX_CLK_SYS_PLL0_3	  12
#define RLX_CLK_SYS_PLL0_5	  13
#define RLX_CLK_SYS_PLL0_7	  14
#define RLX_CLK_SYS_PLL1_2	  15
#define RLX_CLK_SYS_PLL1_3	  16
#define RLX_CLK_SYS_PLL1_5	  17
#define RLX_CLK_SYS_PLL1_7	  18
#define RLX_CLK_SYS_PLL2_2	  19
#define RLX_CLK_SYS_PLL2_3	  20
#define RLX_CLK_SYS_PLL2_5	  21
#define RLX_CLK_SYS_PLL2_7	  22
#define RLX_CLK_SYS_PLL3_2	  23
#define RLX_CLK_SYS_PLL3_3	  24
#define RLX_CLK_SYS_PLL3_5	  25
#define RLX_CLK_SYS_PLL3_7	  26
#define RLX_CLK_DMA_CK		  27
#define RLX_CLK_USBPHY_HOST_CK	  28
#define RLX_CLK_USBPHY_DEV_CK	  29
#define RLX_CLK_ETHERNET_CK	  30
#define RLX_CLK_CPU_CK_DIV	  31
#define RLX_CLK_CPU_CK_DEC	  32
#define RLX_CLK_CPU_CK		  33
#define RLX_CLK_BUS_CK_DIV	  34
#define RLX_CLK_BUS_CK_DEC	  35
#define RLX_CLK_BUS_CK		  36
#define RLX_CLK_DRAM_CK_DIV	  37
#define RLX_CLK_DRAM_CK_DEC	  38
#define RLX_CLK_DRAM_CK		  39
#define RLX_CLK_I2C_CK_DIV	  40
#define RLX_CLK_I2C_CK		  41
#define RLX_CLK_XB2_CK_DIV	  42
#define RLX_CLK_XB2_CK		  43
#define RLX_CLK_UART_CK_DIV	  44
#define RLX_CLK_UART_CK		  45
#define RLX_CLK_CIPHER_CK	  46
#define RLX_CLK_RSA		  47
#define RLX_CLK_SHA		  48
#define RLX_CLK_TRNG		  49
#define RLX_CLK_OTP		  50
#define RLX_CLK_MACBYPASS_CK_DIV  51
#define RLX_CLK_MACBYPASS_CK	  52
#define RLX_CLK_SD0_CRC_CK_DIV	  53
#define RLX_CLK_SD0_CRC_CK	  54
#define RLX_CLK_SD0_SAMPLE_CK_DIV 55
#define RLX_CLK_SD0_SAMPLE_CK	  56
#define RLX_CLK_SD0_PUSH_CK_DIV	  57
#define RLX_CLK_SD0_PUSH_CK	  58
#define RLX_CLK_SD0_DDR_CK	  59
#define RLX_CLK_SD1_CRC_CK_DIV	  60
#define RLX_CLK_SD1_CRC_CK	  61
#define RLX_CLK_SD1_SAMPLE_CK_DIV 62
#define RLX_CLK_SD1_SAMPLE_CK	  63
#define RLX_CLK_SD1_PUSH_CK_DIV	  64
#define RLX_CLK_SD1_PUSH_CK	  65
#define RLX_CLK_SD1_DDR_CK	  66
#define RLX_CLK_P1BUS_CK_DIV	  67
#define RLX_CLK_P1BUS_CK_DEC	  68
#define RLX_CLK_P1BUS_CK	  69
#define RLX_CLK_UART1_CK_DIV	  70
#define RLX_CLK_UART1_CK	  71
#define RLX_CLK_UART2_CK_DIV	  72
#define RLX_CLK_UART2_CK	  73
#define RLX_CLK_I2C1_CK_DIV	  74
#define RLX_CLK_I2C1_CK		  75
#define RLX_CLK_SSI_CK_DIV	  76
#define RLX_CLK_SSI_CK		  77

#define RLX_CLK_NUM_SIZE (RLX_CLK_SSI_CK + 1)

#endif