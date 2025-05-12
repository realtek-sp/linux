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

#ifndef _RTSX_ICR_REGS_H_
#define _RTSX_ICR_REGS_H_

#define b_0_(bit) 0
#define b_1_(bit) (0x01 << (bit))

#define b_00_(bit) b_0_(bit)
#define b_01_(bit) b_1_(bit)
#define b_10_(bit) (0x02 << (bit))
#define b_11_(bit) (0x03 << (bit))

#define b_000_(bit) b_00_(bit)
#define b_001_(bit) b_01_(bit)
#define b_010_(bit) b_10_(bit)
#define b_011_(bit) b_11_(bit)
#define b_100_(bit) (0x04 << (bit))
#define b_101_(bit) (0x05 << (bit))
#define b_110_(bit) (0x06 << (bit))
#define b_111_(bit) (0x07 << (bit))

#define REG_RW_RETRY_CNT 3

#define TUNING_PHASE_MAX 32
#define TUNING_RETRY_CNT 5

#define VOLTAGE_OUTPUT_3V3 0
#define VOLTAGE_OUTPUT_1V8 1

#define CLK_1MHz	   1000000
#define DATA_MIN_CLK	   25000000 /* default mode = 25MHz */
#define DATA_MAX_CLK	   208000000 /* SDR104 mode = 208MHz */
#define DATA_MAX_CLK_SDR50 100000000 /* SDR50 mode = 100MHz */
#define FPGA_SRC_CLK	   20000000
#define FPGA_MAX_CLK	   20000000

#define HCBAR	     0x00
#define RESV_BUF_LEN 4096
#define CMD_BUF_LEN  1024
#define CMD_NUM_MAX  256
#define HCBCTLR	     0x04
#define CMD_START    b_1_(31)
#define CMD_STOP     b_1_(28)
#define CMD_LEN_MASK 0x00FFFFFF

#define HDBAR	      0x08
#define DATA_BUF_LEN  (RESV_BUF_LEN - CMD_BUF_LEN)
#define SG_NO_OP      b_00_(4)
#define SG_TRANS_DATA b_10_(4)
#define SG_LINK_DESC  b_11_(4)
#define SG_INT	      b_1_(2)
#define SG_VALID      b_1_(0)
#define SG_LEN_MAX    0x10000
#define HDBCTLR	      0x0C
#define DATA_START    b_1_(31)
#define DATA_READ     b_1_(29)
#define DATA_WRITE    b_0_(29)
#define DATA_STOP     b_1_(28)

#define HAIMR		  0x10
#define HAIMR_START	  b_1_(31)
#define HAIMR_READ	  b_0_(30)
#define HAIMR_WRITE	  b_1_(30)
#define HAIMR_START_READ  (HAIMR_START | HAIMR_READ)
#define HAIMR_START_WRITE (HAIMR_START | HAIMR_WRITE)

#define BIPR		  0x14
#define BIER		  0x18
#define CMD_DONE_INT	  b_1_(31)
#define DATA_DONE_INT	  b_1_(30)
#define TRANS_STATUS_MASK b_11_(28)
#define TRANS_READY	  b_00_(28)
#define TRANS_FAIL	  b_01_(28)
#define TRANS_OK	  b_10_(28)
#define TRANS_INVAL	  b_11_(28)
#define SDHOST_END_INT	  b_1_(27)
#define SD_INT		  b_1_(25)
#define SD_WRITE_PROTECT  b_1_(19)
#define SD_EXIST	  b_1_(16)
#define CARD_ERR_INT	  b_1_(7)

#define OCP_WRAPPER_EN	   0x30
#define OCP_WRAPPER_ENABLE b_1_(0)

#define SDMAR	 0x3C
#define SDMACTLR 0x40

#define PINGPONG_BUF	 0xF800
#define PINGPONG_BUF_LEN 512

#define REG_MIN_ADDR 0xFD50
#define REG_MAX_ADDR 0xFF60

#define CARD_STOP      0xFD54
#define SD_CLEAR_ERROR b_1_(6)
#define SD_STOP	       b_1_(2)
#define CARD_OE	       0xFD55
#define SD_OUTPUT_EN   b_1_(2)

#define CARD_DATA_SOURCE 0xFD5B
#define SRC_RING_BUF	 b_0_(0)
#define SRC_PINGPONG_BUF b_1_(0)

#define CARD_DMA_CTL	 0xFD5C
#define DMA_BUS_WIDTH_16 b_0_(0)
#define DMA_BUS_WIDTH_32 b_1_(0)

#define SDCMD_PULL_CTL 0xFD5D
#define SDCMD_DRV_SEL  0xFD5E
#define SD_CLK_DRV_SEL b_1_(0)
#define SD_CMD_DRV_SEL b_1_(1)

#define SDCMD_SR_CTL	 0xFD5F
#define SDDAT_L_PULL_CTL 0xFD60 /* DAT[3:0] */
#define SDDAT_L_DRV_SEL	 0xFD61
#define SD_DAT0_DRV_SEL	 b_1_(0)
#define SD_DAT1_DRV_SEL	 b_1_(1)
#define SD_DAT2_DRV_SEL	 b_1_(2)
#define SD_DAT3_DRV_SEL	 b_1_(3)

#define SDDAT_L_SR_CTL	 0xFD62
#define SDDAT_H_PULL_CTL 0xFD63 /* DAT[7:4] */
#define SDDAT_H_DRV_SEL	 0xFD64
#define SDDAT_H_SR_CTL	 0xFD65

#define CARD_CLK_EN 0xFD69
#define SD_CLK_EN   b_1_(2)

#define SD_CFG1		      0xFF00
#define SD_CLK_DIVIDE_MASK    b_11_(6)
#define SD_CLK_DIVIDE_0	      b_00_(6)
#define SD_CLK_DIVIDE_128     b_10_(6)
#define SD_CLK_DIVIDE_256     b_11_(6)
#define SD_ASYNC_FIFO_CTL     b_1_(4)
#define SD_ASYNC_FIFO_RST     b_0_(4)
#define SD_ASYNC_FIFO_NOT_RST b_1_(4)
#define SD_MODE_SEL_MASK      b_11_(2)
#define SD_30_MODE	      b_10_(2)
#define SD_DDR_MODE	      b_01_(2)
#define SD_20_MODE	      b_00_(2)
#define SD_BUS_WIDTH_1BIT     b_00_(0)
#define SD_BUS_WIDTH_4BIT     b_01_(0)
#define SD_BUS_WIDTH_8BIT     b_10_(0)

#define SD_CFG2			0xFF01
#define SD_CALCULATE_CRC7	b_0_(7)
#define SD_NO_CALCULATE_CRC7	b_1_(7)
#define SD_CHECK_CRC16		b_0_(6)
#define SD_NO_CHECK_CRC16	b_1_(6)
#define SD_CHECK_CRC_TIMEOUT	b_0_(5)
#define SD_NO_CHECK_CRC_TIMEOUT b_1_(5)
#define SD_CHECK_CRC_STAT	b_0_(4)
#define SD_NO_CHECK_CRC_STAT	b_1_(4)
#define SD_WAIT_BUSY_END	b_1_(3)
#define SD_NO_WAIT_BUSY_END	b_0_(3)
#define SD_CHECK_CRC7		b_0_(2)
#define SD_NO_CHECK_CRC7	b_1_(2)
#define SD_RSP_LEN_MASK		b_11_(0)
#define SD_RSP_LEN_0		b_00_(0)
#define SD_RSP_LEN_6		b_01_(0)
#define SD_RSP_LEN_17		b_10_(0)
#define SD_RSP_TYPE_R0		(SD_RSP_LEN_0 | SD_NO_CHECK_CRC7)
#define SD_RSP_TYPE_R1		SD_RSP_LEN_6
#define SD_RSP_TYPE_R1_DATA	(SD_RSP_TYPE_R0 | SD_NO_CALCULATE_CRC7)
#define SD_RSP_TYPE_R1b		(SD_RSP_LEN_6 | SD_WAIT_BUSY_END)
#define SD_RSP_TYPE_R2		SD_RSP_LEN_17
#define SD_RSP_TYPE_R3		(SD_RSP_LEN_6 | SD_NO_CHECK_CRC7)
#define SD_RSP_TYPE_R4		(SD_RSP_LEN_6 | SD_NO_CHECK_CRC7)
#define SD_RSP_TYPE_R5		SD_RSP_LEN_6
#define SD_RSP_TYPE_R6		SD_RSP_LEN_6
#define SD_RSP_TYPE_R7		SD_RSP_LEN_6

#define SD_CFG3			  0xFF02
#define SEND_STOP_NO_WAIT	  b_1_(7)
#define SEND_STOP_WAIT		  b_0_(7)
#define SEND_CMD_NO_WAIT	  b_1_(6)
#define SEND_CMD_WAIT		  b_0_(6)
#define DATA_PHASE_WAIT		  b_1_(5)
#define DATA_PHASE_NO_WAIT	  b_0_(5)
#define SD30_CLK_STOP_AFTER_DT	  b_1_(4)
#define SD30_CLK_NO_STOP_AFTER_DT b_0_(4)
#define SD20_CLK_STOP_AFTER_DT	  b_1_(3)
#define SD20_CLK_NO_STOP_AFTER_DT b_0_(3)
#define SD_RESP_CHECK		  b_1_(2)
#define SD_RESP_NO_CHECK	  b_0_(2)
#define ADDR_BYTE_MODE		  b_1_(1)
#define ADDR_SECTOR_MODE	  b_0_(1)
#define SD_RESP_CHECK_TIMEOUT	  b_1_(0)
#define SD_RESP_NO_CHECK_TIMEOUT  b_0_(0)

#define SD_STAT1		  0xFF03
#define SD_ERR_CRC7		  b_1_(7)
#define SD_ERR_CRC16		  b_1_(6)
#define SD_ERR_WRITE_CRC	  b_1_(5)
#define SD_STATUS_CRC_ERR	  b_101_(2)
#define SD_STATUS_CRC_NO_ERR	  b_010_(2)
#define SD_STATUS_PRG_ERR	  b_111_(2)
#define SD_ERR_GET_CRC_TIMEOUT	  b_1_(1)
#define SD_ERR_CMP_TUNING_PATTERN b_1_(0)

#define SD_STAT2	0xFF04
#define SD_RESP_INVALID b_1_(1)
#define SD_RESP_TIMEOUT b_1_(0)

#define SD_BUS_STAT	   0xFF05
#define SD_CLK_TOGGLE_EN   b_1_(7)
#define SD_CLK_TOGGLE_STOP b_1_(6)
#define SD_DAT3_STATUS	   b_1_(4)
#define SD_DAT2_STATUS	   b_1_(3)
#define SD_DAT1_STATUS	   b_1_(2)
#define SD_DAT0_STATUS	   b_1_(1)
#define SD_CMD_STATUS	   b_1_(0)
#define SD_CMD_DATA_STATUS_MASK                                             \
	(SD_CMD_STATUS | SD_DAT0_STATUS | SD_DAT1_STATUS | SD_DAT2_STATUS | \
	 SD_DAT3_STATUS)

#define SD_CMD_CODE 0xFF06

#define SD_SAMPLE_POINT_CTL	 0xFF07
#define RX_DDR_DAT_TYPE_MASK	 b_1_(7)
#define RX_DDR_DAT_TYPE_FIX	 b_0_(7)
#define RX_DDR_DAT_TYPE_VAR	 b_1_(7)
#define RX_DDR_DAT_SEL_RISING	 b_0_(6)
#define RX_DDR_DAT_SEL_DELAY_1_4 b_1_(6)
#define RX_DDR_CMD_TYPE_MASK	 b_1_(5)
#define RX_DDR_CMD_TYPE_FIX	 b_0_(5)
#define RX_DDR_CMD_TYPE_VAR	 b_1_(5)
#define RX_DDR_CMD_SEL_RISING	 b_0_(4)
#define RX_DDR_CMD_SEL_DELAY_1_4 b_1_(4)
#define RX_SD20_SEL_MASK	 b_1_(3)
#define RX_SD20_SEL_RISING	 b_0_(3)
#define RX_SD20_SEL_DELAY_1_4	 b_1_(3)

#define SD_PUSH_POINT_CTL	 0xFF08
#define TX_DDR_CMD_DAT_TYPE_MASK b_1_(7)
#define TX_DDR_CMD_DAT_TYPE_FIX	 b_0_(7)
#define TX_DDR_CMD_DAT_TYPE_VAR	 b_1_(7)
#define TX_SD20_SEL_MASK	 b_1_(4)
#define TX_SD20_SEL_FALLING	 b_0_(4)
#define TX_SD20_SEL_AHEAD_1_4	 b_1_(4)

#define SD_CMD0		   0xFF09
#define SD_CMD1		   0xFF0A
#define SD_CMD2		   0xFF0B
#define SD_CMD3		   0xFF0C
#define SD_CMD4		   0xFF0D
#define SD_CMD5		   0xFF0E
#define SD_BYTE_CNT_L	   0xFF0F
#define SD_BYTE_CNT_H	   0xFF10
#define SD_BLOCK_CNT_L	   0xFF11
#define SD_BLOCK_CNT_H	   0xFF12
#define SD_TRANSFER	   0xFF13
#define SD_TRANSFER_START  b_1_(7)
#define SD_TRANSFER_END	   b_1_(6)
#define SD_STAT_IDLE	   b_1_(5)
#define SD_TRANSFER_ERR	   b_1_(4)
#define SD_TM_NORMAL_WRITE (b_0_(3) | b_000_(0))
#define SD_TM_AUTO_WRITE_3 (b_0_(3) | b_001_(0))
#define SD_TM_AUTO_WRITE_4 (b_0_(3) | b_010_(0))
#define SD_TM_AUTO_READ_3  (b_0_(3) | b_101_(0))
#define SD_TM_AUTO_READ_4  (b_0_(3) | b_110_(0))
#define SD_TM_CMD_RSP	   (b_1_(3) | b_000_(0))
#define SD_TM_AUTO_WRITE_1 (b_1_(3) | b_001_(0))
#define SD_TM_AUTO_WRITE_2 (b_1_(3) | b_010_(0))
#define SD_TM_NORMAL_READ  (b_1_(3) | b_100_(0))
#define SD_TM_AUTO_READ_1  (b_1_(3) | b_101_(0))
#define SD_TM_AUTO_READ_2  (b_1_(3) | b_110_(0))
#define SD_TM_AUTO_TUNING  (b_1_(3) | b_111_(0))

#define SD_CMD_STATE  0xFF15
#define SD_CMD_IDLE   b_1_(7)
#define SD_DATA_STATE 0xFF16
#define SD_DATA_IDLE  b_1_(7)

#define SD_AUTO_RESET_FIFO 0xFF19
#define AUTO_RESET_FIFO_EN b_1_(0)
#define SD_DAT_PAD	   0xFF1A

#define SD_DUMMY_4	   0xFF1B
#define SD_DUMMY_5	   0xFF1C
#define SD_DUTY_CTL	   0xFF1D
#define SD30_DATO_DUTY_EN  b_1_(7)
#define SD30_DATO_DELAY_EN b_000_(4)
#define SD30_CMDO_DUTY_EN  b_1_(3)
#define SD30_CMDO_DELAY_EN b_000_(0)

#define SD_RW_STOP_CTL	   0xFF1E
#define SD_INIFINITE_MODE  b_1_(2)
#define SD_CONTROLLER_BUSY b_1_(1)
#define SD_CONTROLLER_STOP b_1_(0)

#define SD_DUMMY_3		 0xFF1F
#define RA_SD_CMD_PENDING	 b_0_(1)
#define RA_SD_CMD_NO_PENDING	 b_1_(1)
#define RA_SD_CLEAR_BUSY_WAIT	 b_1_(0)
#define RA_SD_CLEAR_BUSY_NO_WAIT b_1_(0)

#define SD_ADDR_L	0xFF20
#define SD_ADDR_H	0xFF21
#define SD_START_ADDR_0 0xFF22
#define SD_START_ADDR_1 0xFF23
#define SD_START_ADDR_2 0xFF24
#define SD_START_ADDR_3 0xFF25
#define SD_RESP_MASK_1	0xFF26
#define SD_RESP_MASK_2	0xFF27
#define SD_RESP_MASK_3	0xFF28
#define SD_RESP_MASK_4	0xFF29
#define SD_RESP_DATA_1	0xFF2A
#define SD_RESP_DATA_2	0xFF2B
#define SD_RESP_DATA_3	0xFF2C
#define SD_RESP_DATA_4	0xFF2D
#define SD_WRITE_DELAY	0xFF2E
#define SD_READ_DELAY	0xFF2F

#endif /* _RTSX_ICR_REGS_H_ */
