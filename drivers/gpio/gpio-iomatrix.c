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

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mfd/iomatrix.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>

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
#define GPIO_INT_POSITIVE_EDGE FIELD_PREP(GPIO_INTCTRL_MASK, 0)
#define GPIO_INT_NEGATIVE_EDGE FIELD_PREP(GPIO_INTCTRL_MASK, 1)
#define GPIO_INT_DUAL_EDGE     FIELD_PREP(GPIO_INTCTRL_MASK, 2)
#define GPIO_INT_LOW_LEVEL     FIELD_PREP(GPIO_INTCTRL_MASK, 3)
#define GPIO_INT_HIGH_LEVEL    FIELD_PREP(GPIO_INTCTRL_MASK, 4)

#define GPIO_INTEN_MASK	   BIT(28)
#define GPIO_INTEN_DISABLE 0
#define GPIO_INTEN_ENABLE  BIT(28)

#define GPIO_INTSTS_MASK BIT(31)

#define GPIO_GET_BIT(reg, mask) (((reg) & (mask)) ? 1 : 0)

#define DRIVER_NAME	       "rts591x-gpio"
#define RTS591X_GPIO_IRQ_COUNT 3

struct rts591x_gpio_irq_entry {
	unsigned int pin;
	unsigned int irq;
};

struct rts591x_gpio_chip {
	struct regmap *regmap;
	phys_addr_t base;
	struct gpio_chip gc;
	enum rts591x_model model;
	const struct rts591x_gpio_irq_entry *irq_entries;
	unsigned int num_irq_entries;
	int virq[RTS591X_GPIO_IRQ_COUNT];
	unsigned int irq_type[RTS591X_GPIO_IRQ_COUNT];
	bool irq_configured;
	bool irq_enabled[RTS591X_GPIO_IRQ_COUNT];
	bool parent_enabled[RTS591X_GPIO_IRQ_COUNT];
	bool irq_update[RTS591X_GPIO_IRQ_COUNT];
	struct mutex irq_lock;
};

static const unsigned int gpio_pins[] = {
	13, 16, 21, 40, 41, 42, 87, 88, 89, 102, 104, 105, 112, 117, 126, 127
};

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

#ifdef CONFIG_GPIOLIB_IRQCHIP
static const struct rts591x_gpio_irq_entry rts591x_gpio_irq_entries[] = {
	{ 21, RTS591X_GPIO21_INT },
	{ 41, RTS591X_GPIO41_INT },
	{ 42, RTS591X_GPIO42_INT },
};

static int rts591x_gpio_irq_index(struct rts591x_gpio_chip *chip,
				  unsigned int offset)
{
	int i;

	for (i = 0; i < chip->num_irq_entries; i++) {
		if (chip->irq_entries[i].pin == offset)
			return i;
	}

	return -EINVAL;
}

static void rts591x_gpio_irq_init_valid_mask(struct gpio_chip *gc,
					     unsigned long *valid_mask,
					     unsigned int ngpios)
{
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	int i;

	bitmap_zero(valid_mask, ngpios);
	for (i = 0; i < chip->num_irq_entries; i++) {
		unsigned int pin = chip->irq_entries[i].pin;

		if (pin < ngpios)
			set_bit(pin, valid_mask);
	}
}

static void rts591x_gpio_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	int index;

	index = rts591x_gpio_irq_index(chip, hwirq);
	if (index < 0)
		return;

	chip->irq_enabled[index] = false;
	chip->irq_update[index] = true;
	gpiochip_disable_irq(gc, hwirq);
}

static void rts591x_gpio_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	int index;

	index = rts591x_gpio_irq_index(chip, hwirq);
	if (index < 0)
		return;

	gpiochip_enable_irq(gc, hwirq);
	chip->irq_enabled[index] = true;
	chip->irq_update[index] = true;
}

static void rts591x_gpio_irq_ack(struct irq_data *d)
{
}

static void rts591x_gpio_irq_bus_lock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);

	mutex_lock(&chip->irq_lock);
}

static void rts591x_gpio_irq_bus_sync_unlock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	int ret;
	int i;

	for (i = 0; i < chip->num_irq_entries; i++) {
		unsigned int pin = chip->irq_entries[i].pin;
		unsigned int type = chip->irq_type[i];
		unsigned int val;

		if (!chip->irq_update[i])
			continue;

		switch (type & IRQ_TYPE_SENSE_MASK) {
		case IRQ_TYPE_EDGE_FALLING:
			val = GPIO_INT_NEGATIVE_EDGE;
			break;
		case IRQ_TYPE_EDGE_BOTH:
			val = GPIO_INT_DUAL_EDGE;
			break;
		case IRQ_TYPE_LEVEL_LOW:
			val = GPIO_INT_LOW_LEVEL;
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			val = GPIO_INT_HIGH_LEVEL;
			break;
		case IRQ_TYPE_EDGE_RISING:
		default:
			val = GPIO_INT_POSITIVE_EDGE;
			break;
		}

		val |= GPIO_INTSTS_MASK;
		if (chip->irq_enabled[i])
			val |= GPIO_INTEN_ENABLE;

		if (!chip->irq_enabled[i] && chip->parent_enabled[i]) {
			disable_irq(chip->virq[i]);
			chip->parent_enabled[i] = false;
		}

		ret = regmap_update_bits(chip->regmap, chip->base + pin * 4,
					 GPIO_DIR_MASK | GPIO_INTCTRL_MASK |
						 GPIO_INTSTS_MASK |
						 GPIO_INTEN_MASK,
					 GPIO_DIR_IN | val);
		if (ret) {
			dev_err(gc->parent, "failed to update GPIO%u IRQ: %d\n",
				pin, ret);
			continue;
		}

		if (chip->irq_enabled[i] && !chip->parent_enabled[i]) {
			enable_irq(chip->virq[i]);
			chip->parent_enabled[i] = true;
		}

		chip->irq_update[i] = false;
	}

	mutex_unlock(&chip->irq_lock);
}

static int rts591x_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rts591x_gpio_chip *chip = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	int index;

	index = rts591x_gpio_irq_index(chip, hwirq);
	if (index < 0)
		return index;

	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_RISING:
	case IRQ_TYPE_EDGE_FALLING:
	case IRQ_TYPE_EDGE_BOTH:
	case IRQ_TYPE_LEVEL_LOW:
	case IRQ_TYPE_LEVEL_HIGH:
		chip->irq_type[index] = type;
		chip->irq_update[index] = true;
		return 0;
	default:
		return -EINVAL;
	}
}

static void rts591x_gpio_irq_print_chip(struct irq_data *data,
					struct seq_file *p)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(data);

	seq_puts(p, dev_name(gc->parent));
}

static const struct irq_chip rts591x_gpio_irq_chip = {
	.irq_ack = rts591x_gpio_irq_ack,
	.irq_mask = rts591x_gpio_irq_mask,
	.irq_unmask = rts591x_gpio_irq_unmask,
	.irq_set_type = rts591x_gpio_irq_set_type,
	.irq_bus_lock = rts591x_gpio_irq_bus_lock,
	.irq_bus_sync_unlock = rts591x_gpio_irq_bus_sync_unlock,
	.irq_print_chip = rts591x_gpio_irq_print_chip,
	.flags = IRQCHIP_IMMUTABLE,
	GPIOCHIP_IRQ_RESOURCE_HELPERS,
};

static irqreturn_t rts591x_gpio_irq_handler(int irq, void *data)
{
	struct rts591x_gpio_chip *chip = data;
	int i;

	for (i = 0; i < chip->num_irq_entries; i++) {
		if (chip->virq[i] == irq) {
			unsigned int pin = chip->irq_entries[i].pin;
			int nested_irq;

			if (!chip->parent_enabled[i])
				return IRQ_HANDLED;

			nested_irq = irq_find_mapping(chip->gc.irq.domain, pin);
			if (nested_irq <= 0)
				return IRQ_NONE;

			handle_nested_irq(nested_irq);
			return IRQ_HANDLED;
		}
	}

	return IRQ_NONE;
}

static int rts591x_gpio_irq_setup(struct platform_device *pdev,
				  struct rts591x_gpio_chip *chip)
{
	struct gpio_irq_chip *girq = &chip->gc.irq;
	int i;

	if (chip->model != MODEL_ESCM)
		return 0;

	chip->irq_configured = true;
	mutex_init(&chip->irq_lock);
	chip->irq_entries = rts591x_gpio_irq_entries;
	chip->num_irq_entries = ARRAY_SIZE(rts591x_gpio_irq_entries);

	for (i = 0; i < chip->num_irq_entries; i++) {
		chip->virq[i] = platform_get_irq(pdev, i);
		if (chip->virq[i] < 0)
			return chip->virq[i];
		irq_set_status_flags(chip->virq[i], IRQ_NOAUTOEN);
	}

	gpio_irq_chip_set_chip(girq, &rts591x_gpio_irq_chip);
	girq->parent_handler = NULL;
	girq->num_parents = 0;
	girq->parents = NULL;
	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_simple_irq;
	girq->init_valid_mask = rts591x_gpio_irq_init_valid_mask;
	girq->threaded = true;

	return 0;
}

static int rts591x_gpio_irq_request_parents(struct platform_device *pdev,
					    struct rts591x_gpio_chip *chip)
{
	struct device *dev = &pdev->dev;
	int ret;
	int i;

	if (chip->model != MODEL_ESCM || !chip->irq_configured)
		return 0;

	for (i = 0; i < chip->num_irq_entries; i++) {
		ret = devm_request_threaded_irq(dev, chip->virq[i], NULL,
						rts591x_gpio_irq_handler,
						IRQF_ONESHOT, dev_name(dev),
						chip);
		if (ret)
			return dev_err_probe(
				dev, ret,
				"failed to request GPIO%u parent IRQ\n",
				chip->irq_entries[i].pin);
	}

	return 0;
}

#else
static int rts591x_gpio_irq_setup(struct platform_device *pdev,
				  struct rts591x_gpio_chip *chip)
{
	return 0;
}

static int rts591x_gpio_irq_request_parents(struct platform_device *pdev,
					    struct rts591x_gpio_chip *chip)
{
	return 0;
}
#endif /* CONFIG_GPIOLIB_IRQCHIP */

static int rts591x_gpio_probe(struct platform_device *pdev)
{
	struct rts591x_gpio_chip *chip;
	struct rts591x_mfd_dev *mfd_dev;
	struct device *dev, *parent;
	int ret;

	dev = &pdev->dev;
	parent = dev->parent;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mfd_dev = dev_get_drvdata(parent);
	if (!mfd_dev)
		return -ENODEV;

	chip->model = mfd_dev->model;

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
	chip->gc.can_sleep = true;

	chip->gc.request = rts591x_gpio_request;
	chip->gc.direction_input = rts591x_gpio_direction_input;
	chip->gc.direction_output = rts591x_gpio_direction_output;
	chip->gc.set = rts591x_gpio_set_value;
	chip->gc.get = rts591x_gpio_get_value;
	chip->gc.get_direction = rts591x_gpio_get_direction;
	chip->gc.set_config = rts591x_gpio_set_config;
	chip->gc.init_valid_mask = rts591x_gpio_init_valid_mask;

	ret = rts591x_gpio_irq_setup(pdev, chip);
	if (ret)
		return ret;

	ret = devm_gpiochip_add_data(dev, &chip->gc, chip);
	if (ret)
		return ret;

	return rts591x_gpio_irq_request_parents(pdev, chip);
}

static const struct of_device_id rts591x_gpio_of_match[] = {
	{ .compatible = "realtek,rts591x-gpio" },
	{}
};
MODULE_DEVICE_TABLE(of, rts591x_gpio_of_match);

static struct platform_driver rts591x_gpio_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = rts591x_gpio_of_match,
	},
	.probe = rts591x_gpio_probe,
};
module_platform_driver(rts591x_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTS591x GPIO Sub-Driver Using MFD");
