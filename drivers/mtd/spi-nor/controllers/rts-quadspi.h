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

/* SPI register offsets */
#define CTRLR0			    0x0000
#define RX_NDF			    0x0004
#define SSIENR			    0x0008
#define MWCR			    0x000c
#define SER			    0x0010
#define BAUDR			    0x0014
#define TXFTLR			    0x0018
#define RXFTLR			    0x001c
#define TXFLR			    0x0020
#define RXFLR			    0x0024
#define SR			    0x0028
#define IMR			    0x002c
#define ISR			    0x0030
#define RISR			    0x0034
#define TXOICR			    0x0038
#define RXOICR			    0x003c
#define RXUICR			    0x0040
#define MSTICR			    0x0044
#define ICR			    0x0048
#define DMACR			    0x004c
#define DMATDLR			    0x0050
#define DMARDLR			    0x0054
#define IDR			    0x0058
#define SPIC_VERSION		    0x005c
#define DR			    0x0060
#define READ_FAST_SINGLE	    0x00e0
#define READ_DUAL_DATA		    0x00e4
#define READ_DUAL_ADDR_DATA	    0x00e8
#define READ_QUAD_DATA		    0x00ec
#define READ_QUAD_ADDR_DATA	    0x00f0
#define WRITE_SINGLE		    0x00f4
#define WRITE_DUAL_DATA		    0x00f8
#define WRITE_DUAL_ADDR_DATA	    0x00fc
#define WRITE_QUAD_DATA		    0x0100
#define WRITE_QUAD_ADDR_DATA	    0x0104
#define WRITE_ENABLE		    0x0108
#define READ_STATUS		    0x010c
#define CTRLR2			    0x0110
#define FBAUDR			    0x0114
#define USER_LENGTH		    0x0118
#define AUTO_LENGTH		    0x011c
#define VALID_CMD		    0x0120
#define FLASH_SIZE		    0x0124
#define FLUSH_FIFO		    0x0128
#define TX_NDF			    0x0130
#define PGM_RST_FIFO		    0x0140

/* SPIC CFG register offsets */
#define SPIC_PGM_FIFO_INIT0	    0x0000
#define SPIC_PGM_FIFO_INIT1	    0x0004
#define SPIC_PGM_FIFO_INIT2	    0x0008
#define SPIC_PGM_FIFO_INIT3	    0x000c
#define SPIC_PGM_FIFO_INIT4	    0x0010
#define SPIC_PGM_FIFO_INIT5	    0x0014
#define SPIC_PGM_FIFO_INIT6	    0x0018
#define SPIC_PGM_FIFO_INIT7	    0x001c
#define SPIC_PGM_FIFO_WPTR	    0x0020
#define SPIC_NOR_DDR_CFG	    0x0024

/* Bit fields in CTRLR0 */
#define SCPH			    6
#define SCPOL			    7
#define TMOD_OFFSET		    8
#define TMOD_MASK		    3
#define TRANSMIT_MODE		    0
#define RECEIVE_MODE		    3
#define DDR_EN_MASK		    7
#define DDR_EN			    3
#define DDR_EN_OFFSET		    13
#define ADDR_CH_OFFSET		    16
#define ADDR_CH_MASK		    3
#define DATA_CH_OFFSET		    18
#define DATA_CH_MASK		    3
#define CMD_CH_OFFSET		    20
#define CMD_CH_MASK		    3
#define USER_MODE		    31

/* Bit fields in SR */
#define BUSY			    0
/* Bit fields in SSIENR */
#define SPIC_EN			    0
#define ATCK_CMD		    1
#define PGM_RST_TEST_EN		    4

/* Bit fields in BAUDR */
#define SCKDV			    0
#define SCKDV_WIDTH		    16
#define SCKDV_MASK		    ((1 << SCKDV_WIDTH) - 1)

/* Bit fields in USER_LENGTH */
#define USER_RD_DUMMY_LENGTH	    0
#define USER_RD_DUMMY_LENGTH_MASK   ((1 << 12) - 1)
#define USER_CMD_LENGTH		    12
#define USER_CMD_LENGTH_MASK	    3
#define USER_ADDR_LENGTH	    16
#define USER_ADDR_LENGTH_MASK	    0xF

/* Bit fields in AUTO_LENGTH */
#define AUTO_RD_DUMMY_LENGTH	    0
#define AUTO_RD_DUMMY_LENGTH_MASK   ((1 << 12) - 1)
#define AUTO_IN_PHYSICAL_CYC	    12
#define AUTO_IN_PHYSICAL_CYC_MASK   0xf
#define AUTO_ADDR_LENGTH	    16
#define AUTO_ADDR_LENGTH_MASK	    0xf
#define AUTO_RDSR_DUMMY_LENGTH	    20
#define AUTO_RDSR_DUMMY_LENGTH_MASK 0xff

/* Bit fields in VALID_CMD */
#define FRD_SINGLE		    0
#define RD_DUAL_I		    1
#define RD_DUAL_IO		    2
#define RD_QUAD_O		    3
#define RD_QUAD_IO		    4
#define WR_DUAL_I		    5
#define WR_DUAL_II		    6
#define WR_QUAD_I		    7
#define WR_QUAD_II		    8
#define WR_BLOCKING		    9

/* Bit fields in CTRLR2 */
#define SO_DNUM			    0
#define WPN_SET			    1
#define WPN_DNUM		    2
#define SEQ_EN			    3
#define FIFO_ENTRY		    4
#define FIFO_ENTRY_MASK		    (0xf << 4)
#define RX_FIFO_ENTRY		    8

/* Bit fileds in PGM_RST ctrl reg */
#define PGMRST_CMD_VAL		    0
#define PGMRST_CMD_CH		    8
#define PGMRST_STATE		    10
#define PGMRST_COUNT		    12

/* Bit fileds in SPIC_NOR_DDR_CFG reg */
#define SPIC_DDR_EN		    0x100

#define SPI_CH			    0
#define QSPI_CH			    1
#define QPI_CH			    2
#define OPI_CH			    3

#define STATE_END		    0
#define STATE_NEXT		    1
#define STATE_KEEP		    2
#define STATE_NOP		    3

#define COUNT_2EXP4		    2
#define COUNT_2EXP16		    4
#define COUNT_2EXP23		    8

#define RTS_QSPI_DRV_NAME	    "rts-spi-nor"

#define SNOR_MFR_ATMEL		    0x1F
#define SNOR_MFR_GIGADEVICE	    0xc8
#define SNOR_MFR_INTEL		    0x89
#define SNOR_MFR_MICRON		    0x20 /* ST Micro <--> Micron */
#define SNOR_MFR_MACRONIX	    0xC2
#define SNOR_MFR_SPANSION	    0x01
#define SNOR_MFR_SST		    0xBF
#define SNOR_MFR_WINBOND	    0xef /* Also used by some Spansion */
#define SNOR_MFR_BOYAMICRO	    0x68
#define SNOR_MFR_EON		    0x1c
#define SNOR_MFR_XTX		    0x0b
#define SNOR_MFR_FM		    0xa1
#define SNOR_MFR_XMC		    0x20
#define SNOR_MFR_XMC_MT_A	    0x70
#define SNOR_MFR_XMC_MT_B	    0x60
#define SNOR_MFR_PUYA		    0x85
#define SNOR_MFR_XD		    0xd8 /* manufacturer is Gigadevice */
#define SNOR_MFR_ZBIT		    0x5e

/**
 * struct spi_nor_xfer_cfg - Structure for defining a Serial Flash transfer
 * @wren:               command for "Write Enable", or 0x00 for not required
 * @cmd:                command for operation
 * @cmd_nbits:           number of pins to send @cmd (1, 2, 4)
 * @addr:               address for operation
 * @addr_nbits:          number of pins to send @addr (1, 2, 4)
 * @addr_nbytes:        number of address bytes
 *                      (3,4, or 0 for address not required)
 * @mode:               mode data
 * @data_nbits:          number of pins to send @mode (1, 2, 4)
 * @mode_cycles:        number of mode cycles (0 for mode not required)
 * @dummy_cycles:       number of dummy cycles (0 for dummy not required)
 */
struct spi_nor_xfer_cfg {
	u8 wren;
	u8 cmd;
	u8 cmd_nbits;
	u32 addr;
	u8 addr_nbits;
	u8 addr_nbytes;
	u8 ddr_en;
	u8 mode;
	u8 data_nbits;
	u8 mode_cycles;
	u8 dummy_cycles;
};

enum rts_qspi_devtype {
	TYPE_FPGA = (1 << 0),
	TYPE_ASIC = (1 << 1),
};

#if IS_ENABLED(CONFIG_SPI_RTS_QUADSPI_IRQ)
#define rts_qspi_write rts_qspi_write_irq
#define rts_qspi_erase rts_qspi_erase_irq
#elif IS_ENABLED(CONFIG_SPI_RTS_QUADSPI_POLLING)
#define rts_qspi_write rts_qspi_write_polling
#define rts_qspi_erase rts_qspi_erase_polling
#endif

#define CHECK_TIMEOUT BIT(0)
struct rts_qspi {
	struct spi_nor nor;
	struct platform_device *pdev;
	int irq;
	void __iomem *regs;
	void __iomem *spic_cfg_regs;
	phys_addr_t phybase;

	struct clk *clk;
	u32 spiclk_hz;
	u32 max_speed_hz;
	u32 min_speed_hz;

	bool use_dma;
	int fifo_size;
	int fifo_entry;

	struct completion *done;
	u8 status;
	int timeout_ms;
	spinlock_t __lock;
	unsigned long __lock_flags;

	enum rts_qspi_devtype devtype;

	struct {
		struct pinctrl *p;
		struct pinctrl_state *quad_state;
	} pins;
};

struct rts_qspi_devtype_data {
	enum rts_qspi_devtype devtype;
	int fifo_entry;
	int fifo_size;
};

static struct rts_qspi_devtype_data rts493xa_data = {
	.devtype = TYPE_ASIC,
	.fifo_entry = 7,
	.fifo_size = 128,
};

static struct spi_swp {
	unsigned long time;
	bool swp_flag;
	struct task_struct *enable_swp_task;
} spi_nor_swp;

enum swp_state { SWP_ENABLE, SWP_DISABLE };
