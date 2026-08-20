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

#ifndef __IOMATRIX_H
#define __IOMATRIX_H

#include <dt-bindings/interrupt-controller/realtek,rts591x-iomatrix.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>

enum {
	RTS591X_I2CSLV_PENDING_READ_INT =
		RTS591X_IOMATRIX_IRQ_I2CSLV_PENDING_READ,
	RTS591X_KCS_IBF_INT = RTS591X_IOMATRIX_IRQ_KCS_IBF,
	RTS591X_I2C0_INT = RTS591X_IOMATRIX_IRQ_I2C0,
	RTS591X_I2C1_INT = RTS591X_IOMATRIX_IRQ_I2C1,
	RTS591X_I2C2_INT = RTS591X_IOMATRIX_IRQ_I2C2,
	RTS591X_I2C3_INT = RTS591X_IOMATRIX_IRQ_I2C3,
	RTS591X_I2C4_INT = RTS591X_IOMATRIX_IRQ_I2C4,
	RTS591X_I2C5_INT = RTS591X_IOMATRIX_IRQ_I2C5,
	RTS591X_I2C6_INT = RTS591X_IOMATRIX_IRQ_I2C6,
	RTS591X_I2C7_INT = RTS591X_IOMATRIX_IRQ_I2C7,
	RTS591X_I2C10_INT = RTS591X_IOMATRIX_IRQ_I2C10,
	RTS591X_I2C11_INT = RTS591X_IOMATRIX_IRQ_I2C11,
	RTS591X_ADC_INT = RTS591X_IOMATRIX_IRQ_ADC,
	RTS591X_TACHO0_INT = RTS591X_IOMATRIX_IRQ_TACHO0,
	RTS591X_TACHO1_INT = RTS591X_IOMATRIX_IRQ_TACHO1,
	RTS591X_TACHO2_INT = RTS591X_IOMATRIX_IRQ_TACHO2,
	RTS591X_TACHO3_INT = RTS591X_IOMATRIX_IRQ_TACHO3,
	RTS591X_PORT80_INT = RTS591X_IOMATRIX_IRQ_PORT80,
	RTS591X_PORT80_1_INT = RTS591X_IOMATRIX_IRQ_PORT80_1,
	RTS591X_GPIO21_INT = RTS591X_IOMATRIX_IRQ_GPIO21,
	RTS591X_GPIO41_INT = RTS591X_IOMATRIX_IRQ_GPIO41,
	RTS591X_GPIO42_INT = RTS591X_IOMATRIX_IRQ_GPIO42,
	RTS591X_NUM_IRQS
};

#define RTS591X_I2CSLV_PENDING_READ_INT_MASK \
	BIT(RTS591X_I2CSLV_PENDING_READ_INT)
#define RTS591X_KCS_IBF_INT_MASK  BIT(RTS591X_KCS_IBF_INT)
#define RTS591X_I2C0_INT_MASK	  BIT(RTS591X_I2C0_INT)
#define RTS591X_I2C1_INT_MASK	  BIT(RTS591X_I2C1_INT)
#define RTS591X_I2C2_INT_MASK	  BIT(RTS591X_I2C2_INT)
#define RTS591X_I2C3_INT_MASK	  BIT(RTS591X_I2C3_INT)
#define RTS591X_I2C4_INT_MASK	  BIT(RTS591X_I2C4_INT)
#define RTS591X_I2C5_INT_MASK	  BIT(RTS591X_I2C5_INT)
#define RTS591X_I2C6_INT_MASK	  BIT(RTS591X_I2C6_INT)
#define RTS591X_I2C7_INT_MASK	  BIT(RTS591X_I2C7_INT)
#define RTS591X_I2C10_INT_MASK	  BIT(RTS591X_I2C10_INT)
#define RTS591X_I2C11_INT_MASK	  BIT(RTS591X_I2C11_INT)
#define RTS591X_ADC_INT_MASK	  BIT(RTS591X_ADC_INT)
#define RTS591X_TACHO0_INT_MASK	  BIT(RTS591X_TACHO0_INT)
#define RTS591X_TACHO1_INT_MASK	  BIT(RTS591X_TACHO1_INT)
#define RTS591X_TACHO2_INT_MASK	  BIT(RTS591X_TACHO2_INT)
#define RTS591X_TACHO3_INT_MASK	  BIT(RTS591X_TACHO3_INT)
#define RTS591X_PORT80_INT_MASK	  BIT(RTS591X_PORT80_INT)
#define RTS591X_PORT80_1_INT_MASK BIT(RTS591X_PORT80_1_INT)
#define RTS591X_GPIO21_INT_MASK	  BIT(RTS591X_GPIO21_INT)
#define RTS591X_GPIO41_INT_MASK	  BIT(RTS591X_GPIO41_INT)
#define RTS591X_GPIO42_INT_MASK	  BIT(RTS591X_GPIO42_INT)

#define RTS591X_IRQ_STAT_BASE 0x20075000

enum rts591x_model { MODEL_ESCM = 0, MODEL_HPM };

struct rts591x_mfd_dev {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *irq_gpio;
	struct regmap_irq_chip_data *irq_data;
	enum rts591x_model model;
};

/*
 * SPIC write packetization sequence, carried in the request attr byte bit[7:6].
 * A sector is streamed as FIRST -> CONT x n -> LAST, or SINGLE when it fits in
 * one frame.  Every frame is acknowledged; FIRST resets the EC sector buffer,
 * CONT appends, and LAST/SINGLE finalize the sector (compare + erase + program)
 * and return the final ACK/ERROR.
 */
enum iomatrix_spic_seq {
	IOMATRIX_SPIC_SEQ_SINGLE = 0,
	IOMATRIX_SPIC_SEQ_FIRST = 1,
	IOMATRIX_SPIC_SEQ_CONT = 2,
	IOMATRIX_SPIC_SEQ_LAST = 3,
};

int iomatrix_regmap_update_lock(struct regmap *map);
void iomatrix_regmap_update_unlock(struct regmap *map);
int iomatrix_regmap_spic_erase(struct regmap *map, u32 addr);
int iomatrix_regmap_spic_write(struct regmap *map, u32 addr, const u8 *buf,
			       u32 len, u8 seq);
int iomatrix_regmap_spic_read(struct regmap *map, u32 addr, u8 *buf, u32 len);
int iomatrix_regmap_spic_update(struct regmap *map);
int iomatrix_regmap_spic_reboot(struct regmap *map);

int iomatrix_regmap_peci_oob(struct regmap *map, const u8 *cmd_buf, u32 cmd_len,
			     u8 *resp_buf, u32 *resp_len);

#endif /* __IOMATRIX_H */
