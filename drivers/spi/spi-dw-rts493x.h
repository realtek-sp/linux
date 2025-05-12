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

#ifndef RTS_SPI_HEADER_H
#define RTS_SPI_HEADER_H

#include "spi-dw.h"
#include <linux/gpio.h>

/*Register for ssi spi*/
#define SSI_START      0x8000
#define SSI_STOP       0x8004
#define SSI_RD_ADDR    0x8008
#define SSI_WR_ADDR    0x800c
#define SSI_DATA_LEN   0x8010
#define SSI_CONTROL    0x8014
#define SSI_IRQ_ENABLE 0x8018
#define SSI_IRQ_STATUS 0x801c

//SPI_SSI_START
#define SPI_SSI_START 0x1

//SPI_SSI_STOP
#define SPI_SSI_STOP 0x1

//SPI_SSI_IRQ
#define DONE_INT_EN  0x1
#define DONE_INT_DIS 0x0

//SPI_SSI_IRQ_STATUS
#define SSI_DONE_INT 0x1

/* Bit fields in CTRLR0 */
#define SPI_DFS_OFFSET 0

#define SPI_FRF_OFFSET	  4
#define SPI_FRF_SPI	  0x0
#define SPI_FRF_SSP	  0x1
#define SPI_FRF_MICROWIRE 0x2
#define SPI_FRF_RESV	  0x3

#define SPI_MODE_OFFSET 6
#define SPI_SCPH_OFFSET 6
#define SPI_SCOL_OFFSET 7

#define SPI_TMOD_OFFSET	   8
#define SPI_TMOD_MASK	   (0x3 << SPI_TMOD_OFFSET)
#define SPI_TMOD_TR	   0x0 /* xmit & recv */
#define SPI_TMOD_TO	   0x1 /* xmit only */
#define SPI_TMOD_RO	   0x2 /* recv only */
#define SPI_TMOD_EPROMREAD 0x3 /* eeprom read mode */

#define SPI_SLVOE_OFFSET 10
#define SPI_SRL_OFFSET	 11
#define SPI_CFS_OFFSET	 12

/*Register for ssi pad offsets*/
/*0x1887_0070 ~ 0x1887_0088*/
#define DW_SSI_PAD_SEL 0x0
#define DW_SSI_PAD_PU  0x4
#define DW_SSI_PAD_PD  0x8
#define DW_SSI_PAD_SR  0xc
#define DW_SSI_PAD_OE2 0x10
#define DW_SSI_CS_FW   0x14
#define DE_SSI_CS_SEL  0x18

/* TX RX interrupt level threshold, max can be 256 */
#define SPI_INT_THRESHOLD 32

enum dw_ssi_type {
	SSI_MOTO_SPI = 0,
	SSI_TI_SSP,
	SSI_NS_MICROWIRE,
};

#define DRIVER_NAME "dw_spi_rts"

struct dw_spi_rts {
	struct dw_spi dws;
	void __iomem *reg_pad;
	struct clk *clk;
	struct device *dev;
};

static inline u32 rts_spi_readl_pad(struct dw_spi_rts *dwsrts, u32 offset)
{
	return readl(dwsrts->reg_pad + offset);
}

static inline void rts_spi_writel_pad(struct dw_spi_rts *dwsrts, u32 offset,
				      u32 val)
{
	writel(val, dwsrts->reg_pad + offset);
}

#endif /* RTS_SPI_HEADER_H */
