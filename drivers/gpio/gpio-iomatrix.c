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
	int irq;
};

static const char *rts591x_gpio_names[] = {
	"EC_GPIO000", "EC_GPIO001", "EC_GPIO002", "EC_GPIO003", "EC_GPIO004",
	"EC_GPIO005", "EC_GPIO006", "EC_GPIO007", "EC_GPIO008", "EC_GPIO009",
	"EC_GPIO010", "EC_GPIO011", "EC_GPIO012", "EC_GPIO013", "EC_GPIO014",
	"EC_GPIO015", "EC_GPIO016", "EC_GPIO017", "EC_GPIO018", "EC_GPIO019",
	"EC_GPIO020", "EC_GPIO021", "EC_GPIO022", "EC_GPIO023", "EC_GPIO024",
	"EC_GPIO025", "EC_GPIO026", "EC_GPIO027", "EC_GPIO028", "EC_GPIO029",
	"EC_GPIO030", "EC_GPIO031", "EC_GPIO032", "EC_GPIO033", "EC_GPIO034",
	"EC_GPIO035", "EC_GPIO036", "EC_GPIO037", "EC_GPIO038", "EC_GPIO039",
	"EC_GPIO040", "EC_GPIO041", "EC_GPIO042", "EC_GPIO043", "EC_GPIO044",
	"EC_GPIO045", "EC_GPIO046", "EC_GPIO047", "EC_GPIO048", "EC_GPIO049",
	"EC_GPIO050", "EC_GPIO051", "EC_GPIO052", "EC_GPIO053", "EC_GPIO054",
	"EC_GPIO055", "EC_GPIO056", "EC_GPIO057", "EC_GPIO058", "EC_GPIO059",
	"EC_GPIO060", "EC_GPIO061", "EC_GPIO062", "EC_GPIO063", "EC_GPIO064",
	"EC_GPIO065", "EC_GPIO066", "EC_GPIO067", "EC_GPIO068", "EC_GPIO069",
	"EC_GPIO070", "EC_GPIO071", "EC_GPIO072", "EC_GPIO073", "EC_GPIO074",
	"EC_GPIO075", "EC_GPIO076", "EC_GPIO077", "EC_GPIO078", "EC_GPIO079",
	"EC_GPIO080", "EC_GPIO081", "EC_GPIO082", "EC_GPIO083", "EC_GPIO084",
	"EC_GPIO085", "EC_GPIO086", "EC_GPIO087", "EC_GPIO088", "EC_GPIO089",
	"EC_GPIO090", "EC_GPIO091", "EC_GPIO092", "EC_GPIO093", "EC_GPIO094",
	"EC_GPIO095", "EC_GPIO096", "EC_GPIO097", "EC_GPIO098", "EC_GPIO099",
	"EC_GPIO100", "EC_GPIO101", "EC_GPIO102", "EC_GPIO103", "EC_GPIO104",
	"EC_GPIO105", "EC_GPIO106", "EC_GPIO107", "EC_GPIO108", "EC_GPIO109",
	"EC_GPIO110", "EC_GPIO111", "EC_GPIO112", "EC_GPIO113", "EC_GPIO114",
	"EC_GPIO115", "EC_GPIO116", "EC_GPIO117", "EC_GPIO118", "EC_GPIO119",
	"EC_GPIO120", "EC_GPIO121", "EC_GPIO122", "EC_GPIO123", "EC_GPIO124",
	"EC_GPIO125", "EC_GPIO126", "EC_GPIO127", "EC_GPIO128", "EC_GPIO129",
	"EC_GPIO130", "EC_GPIO131"
};

static const unsigned int gpio_pins[] = { 30,  31,  87,	 88,  89,  94,
					  102, 105, 112, 117, 123, 127 };

static int rts591x_gpio_init_valid_mask(struct gpio_chip *gc,
					unsigned long *valid_mask,
					unsigned int ngpios)
{
	bitmap_zero(valid_mask, ngpios);

	for (size_t i = 0; i < ARRAY_SIZE(gpio_pins); i++)
		bitmap_set(valid_mask, gpio_pins[i], 1);

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
	struct device *dev, *parent;
	int ret;

	dev = &pdev->dev;
	parent = dev->parent;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

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
	chip->gc.names = rts591x_gpio_names;

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
