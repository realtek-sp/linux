// SPDX-License-Identifier: GPL-2.0-or-later
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

#include <linux/bits.h>
#include <linux/dma-mapping.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/mfd/iomatrix.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define GPIO_DIR_MASK BIT(0)
#define GPIO_DIR_IN   0
#define GPIO_DIR_OUT  BIT(0)

#define GPIO_INDETEN_MASK    BIT(1)
#define GPIO_INDETEN_DISABLE 0
#define GPIO_INDETEN_ENABLE  BIT(1)

#define GPIO_INVOLMD_MASK   BIT(2)
#define GPIO_INVOLMD_NORMAL 0
#define GPIO_INVOLMD_INVERT BIT(2)

#define GPIO_PINSTS_MASK BIT(3)

#define GPIO_MFCTRL_MASK  GENMASK(10, 8)
#define GPIO_MFCTRL_SHIFT 8

#define GPIO_OUTDRV_MASK   BIT(11)
#define GPIO_OUTDRV_NORMAL 0
#define GPIO_OUTDRV_HIGH   BIT(11)

#define GPIO_SLEWRATE_MASK BIT(12)
#define GPIO_SLEWRATE_FAST 0
#define GPIO_SLEWRATE_SLOW BIT(12)

#define GPIO_PULLDWEN_MASK    BIT(13)
#define GPIO_PULLDWEN_DISABLE 0
#define GPIO_PULLDWEN_ENABLE  BIT(13)

#define GPIO_PULLUPEN_MASK    BIT(14)
#define GPIO_PULLUPEN_DISABLE 0
#define GPIO_PULLUPEN_ENABLE  BIT(14)

#define GPIO_SCHEN_MASK	   BIT(15)
#define GPIO_SCHEN_DISABLE 0
#define GPIO_SCHEN_ENABLE  BIT(15)

#define GPIO_OUTMD_MASK	     BIT(16)
#define GPIO_OUTMD_PUSHPULL  0
#define GPIO_OUTMD_OPENDRAIN BIT(16)

#define GPIO_OUTCTRL_MASK BIT(17)
#define GPIO_OUTCTRL_LOW  0
#define GPIO_OUTCTRL_HIGH BIT(17)

#define GPIO_INTCTRL_MASK      GENMASK(26, 24)
#define GPIO_INTCTRL_SHIFT     24
#define GPIO_INT_POSITIVE_EDGE (0 << GPIO_INTCTRL_SHIFT)
#define GPIO_INT_NEGATIVE_EDGE (1 << GPIO_INTCTRL_SHIFT)
#define GPIO_INT_DUAL_EDGE     (2 << GPIO_INTCTRL_SHIFT)
#define GPIO_INT_LOW_LEVEL     (3 << GPIO_INTCTRL_SHIFT)
#define GPIO_INT_HIGH_LEVEL    (4 << GPIO_INTCTRL_SHIFT)

#define GPIO_INTEN_MASK	   BIT(28)
#define GPIO_INTEN_DISABLE 0
#define GPIO_INTEN_ENABLE  BIT(28)

#define GPIO_INTSTS_MASK BIT(31)

#define GPIO_GET_BIT(reg, mask) (((reg) & (mask)) ? 1 : 0)

#define DRIVER_NAME "rts591x-gpio"

struct rts591x_gpio_chip {
	struct regmap *regmap;
	phys_addr_t base;
	struct gpio_chip gc;
	enum rts591x_model model;
};

static const unsigned int gpio_pins[] = { 13,  16,  40,	 87,  88, 89,
					  102, 104, 105, 112, 117 };

static const unsigned int gpio_pins_hpm[] = {
	0,   1,	  2,   3,   4,	 9,   13,  14,	15,  16,  17,  18,
	19,  20,  21,  30,  40,	 84,  86,  94,	95,  96,  97,  99,
	100, 101, 102, 103, 104, 105, 106, 107, 109, 111, 112, 113,
	114, 115, 117, 118, 119, 122, 123, 124, 125, 126, 127, 131
};

static int rts591x_gpio_request(struct gpio_chip *gc, unsigned int offset)
{
	if (!test_bit(offset, gc->valid_mask)) {
		dev_err(gc->parent, "GPIO %u is not valid\n", offset);
		return -EINVAL;
	}

	dev_dbg(gc->parent, "GPIO %u requested\n", offset);

	return 0;
}

static int rts591x_gpio_init_valid_mask(struct gpio_chip *gc,
					unsigned long *valid_mask,
					unsigned int ngpios)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	const unsigned int *pins;
	size_t nr_pins;
	size_t i;

	if (chip->model == MODEL_ESCM) {
		pins = gpio_pins;
		nr_pins = ARRAY_SIZE(gpio_pins);
	} else if (chip->model == MODEL_HPM) {
		pins = gpio_pins_hpm;
		nr_pins = ARRAY_SIZE(gpio_pins_hpm);
	} else {
		bitmap_zero(valid_mask, ngpios);
		return 0;
	}

	bitmap_zero(valid_mask, ngpios);

	for (i = 0; i < nr_pins; i++) {
		unsigned int pin = pins[i];

		if (pin < ngpios)
			bitmap_set(valid_mask, pin, 1);
	}

	return 0;
}

static int rts591x_gpio_direction_input(struct gpio_chip *gc,
					unsigned int offset)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);

	return regmap_update_bits(chip->regmap, chip->base + offset * 4,
				  GPIO_DIR_MASK, GPIO_DIR_IN);
}

static int rts591x_gpio_direction_output(struct gpio_chip *gc,
					 unsigned int offset, int value)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	unsigned int mask, regval, current_mode;
	int rv;

	rv = regmap_read(chip->regmap, chip->base + offset * 4, &current_mode);
	if (rv)
		return rv;

	mask = GPIO_DIR_MASK | GPIO_OUTCTRL_MASK;

	if (current_mode & GPIO_OUTMD_MASK) {
		if (!!value) {
			regval = GPIO_OUTCTRL_HIGH & ~GPIO_DIR_OUT;
		} else {
			return 0;
		}
	} else {
		regval = (!!value ? GPIO_OUTCTRL_HIGH : GPIO_OUTCTRL_LOW) |
			 GPIO_DIR_OUT;
	}

	rv = regmap_update_bits(chip->regmap, chip->base + offset * 4, mask,
				regval);

	return rv;
}

static void rts591x_gpio_set_value(struct gpio_chip *gc, unsigned int offset,
				   int value)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	int rv, regval;

	regval = value ? GPIO_OUTCTRL_HIGH : GPIO_OUTCTRL_LOW;

	rv = regmap_update_bits(chip->regmap, chip->base + offset * 4,
				GPIO_OUTCTRL_MASK, regval);
	if (rv)
		dev_err(gc->parent, "cannot set GPIO value: %d\n", rv);
}

static int rts591x_gpio_get_value(struct gpio_chip *gc, unsigned int offset)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	unsigned int val;
	int rv;

	rv = regmap_read(chip->regmap, chip->base + offset * 4, &val);
	if (rv)
		return rv;

	return GPIO_GET_BIT(val, GPIO_PINSTS_MASK);
}

static int rts591x_gpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	unsigned int val;
	int rv;

	rv = regmap_read(chip->regmap, chip->base + offset * 4, &val);
	if (rv)
		return rv;

	return GPIO_GET_BIT(val, GPIO_DIR_MASK);
}

static int rts591x_gpio_set_config(struct gpio_chip *gc, unsigned int offset,
				   unsigned long config)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);

	enum pin_config_param param = pinconf_to_config_param(config);
	u16 arg = pinconf_to_config_argument(config);

	switch (param) {
	case PIN_CONFIG_BIAS_PULL_DOWN:
		return regmap_update_bits(chip->regmap, chip->base + offset * 4,
					  GPIO_PULLDWEN_MASK |
						  GPIO_PULLUPEN_MASK,
					  GPIO_PULLDWEN_ENABLE);
	case PIN_CONFIG_BIAS_PULL_UP:
		return regmap_update_bits(chip->regmap, chip->base + offset * 4,
					  GPIO_PULLDWEN_MASK |
						  GPIO_PULLUPEN_MASK,
					  GPIO_PULLUPEN_ENABLE);
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		return regmap_update_bits(chip->regmap, chip->base + offset * 4,
					  GPIO_OUTMD_MASK,
					  GPIO_OUTMD_OPENDRAIN);
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		return regmap_update_bits(chip->regmap, chip->base + offset * 4,
					  GPIO_OUTMD_MASK, GPIO_OUTMD_PUSHPULL);
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		return regmap_update_bits(chip->regmap, chip->base + offset * 4,
					  GPIO_SCHEN_MASK, GPIO_SCHEN_ENABLE);
	case PIN_CONFIG_POWER_SOURCE:
		if (arg != 1800 && arg != 3300)
			return -EINVAL;
		return regmap_update_bits(chip->regmap, chip->base + offset * 4,
					  GPIO_INVOLMD_MASK,
					  (arg == 1800) ? GPIO_INVOLMD_NORMAL :
							  GPIO_INVOLMD_INVERT);
	case PIN_CONFIG_SLEW_RATE:
		return regmap_update_bits(chip->regmap, chip->base + offset * 4,
					  GPIO_SLEWRATE_MASK, !!arg);
	default:
		return -ENOTSUPP;
	}
}

static int rts591x_gpio_probe(struct platform_device *pdev)
{
	struct rts591x_gpio_chip *chip;
	struct rts591x_model_pdata *pdata;
	struct device *dev, *parent;
	int ret;

	dev = &pdev->dev;
	parent = dev->parent;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	pdata = dev_get_platdata(&pdev->dev);
	if (pdata) {
		chip->model = pdata->model;
	} else {
		dev_warn(&pdev->dev, "No pdata, defaulting to ESCM\n");
		chip->model = MODEL_ESCM;
	}

	chip->regmap = dev_get_regmap(parent, NULL);
	if (!chip->regmap)
		return -ENODEV;

	ret = device_property_read_u32(dev, "reg", &chip->base);
	if (ret) {
		dev_err(dev, "failed to get gpio base address\n");
		return ret;
	}

	chip->gc.base = -1;
	chip->gc.ngpio = 132;
	chip->gc.label = dev_name(dev);
	chip->gc.parent = dev;
	chip->gc.owner = THIS_MODULE;

	chip->gc.request = rts591x_gpio_request;
	chip->gc.direction_input = rts591x_gpio_direction_input;
	chip->gc.direction_output = rts591x_gpio_direction_output;
	chip->gc.set = rts591x_gpio_set_value;
	chip->gc.get = rts591x_gpio_get_value;
	chip->gc.get_direction = rts591x_gpio_get_direction;
	chip->gc.set_config = rts591x_gpio_set_config;
	chip->gc.init_valid_mask = rts591x_gpio_init_valid_mask;

	ret = devm_gpiochip_add_data(dev, &chip->gc, chip);

	return ret;
}

static struct platform_driver rts591x_gpio_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = rts591x_gpio_probe,
};
module_platform_driver(rts591x_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTS591x GPIO Sub-Driver Using MFD");
