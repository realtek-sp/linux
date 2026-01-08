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

#include <linux/regmap.h>
#include <linux/spinlock.h>

enum {
	RTS591X_I2CSLV_PENDING_READ_INT,
	RTS591X_KCS_IBF_INT,
	RTS591X_I2C0_INT,
	RTS591X_I2C1_INT,
	RTS591X_I2C2_INT,
	RTS591X_I2C3_INT,
	RTS591X_I2C4_INT,
	RTS591X_I2C5_INT,
	RTS591X_I2C6_INT,
	RTS591X_I2C7_INT,
	RTS591X_ADC_INT,
	RTS591X_TACHO0_INT,
	RTS591X_TACHO1_INT,
	RTS591X_TACHO2_INT,
	RTS591X_TACHO3_INT,
	RTS591X_NUM_IRQS
};

#define RTS591X_I2CSLV_PENDING_READ_INT_MASK \
	BIT(RTS591X_I2CSLV_PENDING_READ_INT)
#define RTS591X_KCS_IBF_INT_MASK BIT(RTS591X_KCS_IBF_INT)
#define RTS591X_I2C0_INT_MASK	 BIT(RTS591X_I2C0_INT)
#define RTS591X_I2C1_INT_MASK	 BIT(RTS591X_I2C1_INT)
#define RTS591X_I2C2_INT_MASK	 BIT(RTS591X_I2C2_INT)
#define RTS591X_I2C3_INT_MASK	 BIT(RTS591X_I2C3_INT)
#define RTS591X_I2C4_INT_MASK	 BIT(RTS591X_I2C4_INT)
#define RTS591X_I2C5_INT_MASK	 BIT(RTS591X_I2C5_INT)
#define RTS591X_I2C6_INT_MASK	 BIT(RTS591X_I2C6_INT)
#define RTS591X_I2C7_INT_MASK	 BIT(RTS591X_I2C7_INT)
#define RTS591X_ADC_INT_MASK	 BIT(RTS591X_ADC_INT)
#define RTS591X_TACHO0_INT_MASK	 BIT(RTS591X_TACHO0_INT)
#define RTS591X_TACHO1_INT_MASK	 BIT(RTS591X_TACHO1_INT)
#define RTS591X_TACHO2_INT_MASK	 BIT(RTS591X_TACHO2_INT)
#define RTS591X_TACHO3_INT_MASK	 BIT(RTS591X_TACHO3_INT)

#define RTS591X_IRQ_STAT_BASE 0x2005C000

struct rts591x_mfd_dev {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *irq_gpio;
	struct regmap_irq_chip_data *irq_data;
};

enum rts591x_model { MODEL_ESCM = 0, MODEL_HPM };

struct rts591x_model_pdata {
	enum rts591x_model model;
};

int iomatrix_regmap_block_write_protected(struct regmap *map,
					  unsigned int base_reg,
					  const void *buf, size_t len);

int iomatrix_regmap_fspi_erase_protected(struct regmap *map, u32 erase_addr,
					 u8 erase_type);

#endif /* __IOMATRIX_H */
