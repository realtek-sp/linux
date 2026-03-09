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

#include <linux/errno.h>
#include <linux/bitfield.h>
#include <linux/gpio/consumer.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/pwm.h>
#include <linux/mfd/iomatrix.h>

/*PWM Duty Register*/
#define RTS591X_PWM_DUTY(x)   (0x00 + x * 0x0c)
#define RTS591X_PWM_DUTY_MASK GENMASK(31, 0)

/*PWM divider Register*/
#define RTS591X_PWM_DIV(x)	(0x04 + x * 12)
#define RTS591X_PWM_DIV_MASK	GENMASK(31, 0)
/*PWM Control Register*/
#define RTS591X_PWM_CTRL(x)	(0x08 + x * 12)
#define RTS591X_PWM_CTRL_EN	BIT(31)
#define RTS591X_PWM_CTRL_RST	BIT(30)
#define RTS591X_PWM_CTRL_INVT	BIT(29)
#define RTS591X_PWM_CTRL_CLKSRC BIT(28)

/*FAN Tacho Control Register*/
#define RTS591X_FAN_TACH_CTRL(x)  (0x00 + x * 0x40)
#define RTS591X_FAN_TACH_CTRL_CNT GENMASK(31, 16)
#define RTS591X_FAN_TACH_READMODE BIT(4)
#define RTS591X_FAN_TACH_SELEDGE  GENMASK(3, 2)
#define RTS591X_FAN_TACH_FILTEREN BIT(1)
#define RTS591X_FAN_TACH_EN	  BIT(0)

/*FAN Tacho STATUS Register*/
#define RTS591X_FAN_TACH_STS(x)	    (0x04 + x * 0x40)
#define RTS591X_FAN_TACH_STS_CNTRDY BIT(3)
#define RTS591X_FAN_TACH_STS_CHG    BIT(2)
#define RTS591X_FAN_TACH_STS_PIN    BIT(1)
#define RTS591X_FAN_TACH_STS_LIMIT  BIT(0)

/*FAN Tacho LIMIT Register*/
#define RTS591X_FAN_TACH_LIMITH(x) (0x08 + x * 0x40)
#define RTS591X_FAN_TACH_LIMITL(x) (0x0C + x * 0x40)
#define RTS591X_FAN_TACH_MASK	   GENMASK(15, 0)

/*FAN Tacho INTEN  Register*/
#define RTS591X_FAN_TACH_INTEN(x)     (0x10 + x * 0x40)
#define RTS591X_FAN_TACH_INTEN_CHG    BIT(2)
#define RTS591X_FAN_TACH_INTEN_CNTRDY BIT(1)
#define RTS591X_FAN_TACH_INTEN_LIMIT  BIT(0)

#define RTS591X_FAN_TACH_MAX_CAHNNEL	  4
#define RTS591X_PWM_MAX_CAHNNEL		  8
#define RTS591X_PWM_DEFAULT_DUTY_PERCENT  (1 / 2)
#define RTS591X_FAN_PULSES_RESOLUTION	  2
#define RTS591X_WAIT_FAN_READY_TIMEOUT_US (1000000) /* us */
#define RTS591X_POLL_SLEEP		  (2000) /* us */
#define RTS591X_TACH_MODE_HZ		  100000
#define RTS591X_POLLING_TIMER_PERIOD	  msecs_to_jiffies(10) /*ms*/
#define RTS591X_SYS_CLOCK_1		  (32768)
#define RTS591X_SYS_CLOCK_2		  (50000000)
#define MAX_PERIOD			  1
#define RTS591X_DEFAULT_DIVIDER		  (0x800)
enum RTS591X_tacho_edge_sel {
	HALF_TACH_PERIOD,
	ONE_TACH_PERIOD,
	TWO_TACH_PERIOD,
	FOUR_TACH_PERIOD,
};

struct rts591x_fan_tacho_dev {
	int irq;
	u32 counter;
	u32 rpm;
	long min;
	long max;
	u8 pulses_per_revolution;
};

struct rts591x_pwm_fan_data {
	struct device *dev;
	u32 pwm_base;
	u32 fan_base;
	u8 pwm_clock_src;
	u8 fan_read_mode;
	u8 fan_edge_sel;
	bool sample_work;
	struct regmap *regmap;
	struct pwm_chip chip;
	bool fan_present[RTS591X_FAN_TACH_MAX_CAHNNEL];
	struct rts591x_fan_tacho_dev fan_dev[RTS591X_FAN_TACH_MAX_CAHNNEL];
	struct mutex pwm_lock;
	struct delayed_work poll_work;
	struct mutex fan_lock[RTS591X_FAN_TACH_MAX_CAHNNEL];
};

static umode_t rts591x_is_visible(const void *priv,
				  enum hwmon_sensor_types type, u32 attr,
				  int channel)
{
	switch (type) {
	case hwmon_fan:
		return 0444;
	default:
		return 0;
	}
}

static int calulate_rpm(struct rts591x_pwm_fan_data *priv, int index)
{
	u32 counter = priv->fan_dev[index].counter;
	switch (priv->fan_edge_sel & 0x03) {
	case HALF_TACH_PERIOD:
		return DIV_ROUND_CLOSEST(60 * RTS591X_TACH_MODE_HZ,
					 counter * 4);
	case ONE_TACH_PERIOD:
		return DIV_ROUND_CLOSEST(60 * RTS591X_TACH_MODE_HZ,
					 counter * 2);
	case TWO_TACH_PERIOD:
		return DIV_ROUND_CLOSEST(60 * RTS591X_TACH_MODE_HZ, counter);
	case FOUR_TACH_PERIOD:
		return DIV_ROUND_CLOSEST(120 * RTS591X_TACH_MODE_HZ, counter);
	default:;
	}
	return 0;
}

static int rts591x_read_fan(struct device *dev, u32 attr, int channel,
			    long *val)
{
	struct rts591x_pwm_fan_data *priv = dev_get_drvdata(dev);
	int ret = 0;
	mutex_lock(&priv->fan_lock[channel]);
	switch (attr) {
	case hwmon_fan_input:
		*val = priv->fan_dev[channel].rpm;
		break;
	case hwmon_fan_max:
		*val = priv->fan_dev[channel].max;
		break;
	case hwmon_fan_min:
		*val = priv->fan_dev[channel].min;
		break;
	case hwmon_fan_pulses:
		*val = RTS591X_FAN_PULSES_RESOLUTION;
		break;
	default:
		dev_err(priv->dev, "Read fan%d failed\n", channel);
		ret = EOPNOTSUPP;
	}
	mutex_unlock(&priv->fan_lock[channel]);
	return ret;
}

static int rts591x_read(struct device *dev, enum hwmon_sensor_types type,
			u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_fan:
		return rts591x_read_fan(dev, attr, channel, val);
	default:
		return -EOPNOTSUPP;
	}
}

static const u32 rts591x_fan_config[] = {
	HWMON_F_INPUT | HWMON_F_MIN | HWMON_F_MAX | HWMON_F_PULSES,
	HWMON_F_INPUT | HWMON_F_MIN | HWMON_F_MAX | HWMON_F_PULSES,
	HWMON_F_INPUT | HWMON_F_MIN | HWMON_F_MAX | HWMON_F_PULSES,
	HWMON_F_INPUT | HWMON_F_MIN | HWMON_F_MAX | HWMON_F_PULSES, 0
};

static const struct hwmon_channel_info rts591x_fan = {
	.type = hwmon_fan,
	.config = rts591x_fan_config,
};

static const struct hwmon_channel_info *rts591x_info[] = { &rts591x_fan, NULL };

static const struct hwmon_ops rts591x_hwmon_ops = {
	.is_visible = rts591x_is_visible,
	.read = rts591x_read,
};

static const struct hwmon_chip_info rts591x_chip_info = {
	.ops = &rts591x_hwmon_ops,
	.info = rts591x_info,
};

static void rts591x_sample_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rts591x_pwm_fan_data *priv =
		container_of(dwork, struct rts591x_pwm_fan_data, poll_work);
	int i, ret;

	for (i = 0; i < RTS591X_FAN_TACH_MAX_CAHNNEL; i++) {
		if (!priv->fan_present[i])
			continue;

		u32 counter, val;

		ret = regmap_read_poll_timeout(
			priv->regmap, priv->fan_base + RTS591X_FAN_TACH_STS(i),
			val, (val & RTS591X_FAN_TACH_STS_CNTRDY),
			RTS591X_POLL_SLEEP, RTS591X_WAIT_FAN_READY_TIMEOUT_US);
		if (ret) {
			dev_dbg(priv->dev,
				"Timeout waiting for read fan counter ready\n");
			priv->fan_dev[i].counter = 0;
			priv->fan_dev[i].rpm = 0;
			continue;
		}
		regmap_read(priv->regmap,
			    priv->fan_base + RTS591X_FAN_TACH_CTRL(i),
			    &counter);
		priv->fan_dev[i].counter =
			FIELD_GET(RTS591X_FAN_TACH_CTRL_CNT, counter);

		priv->fan_dev[i].rpm = calulate_rpm(priv, i);

		regmap_write(priv->regmap,
			     priv->fan_base + RTS591X_FAN_TACH_STS(i),
			     RTS591X_FAN_TACH_STS_CNTRDY);
	}

	schedule_delayed_work(&priv->poll_work, RTS591X_POLLING_TIMER_PERIOD);
}

static int rts591x_en_pwm_port(struct rts591x_pwm_fan_data *priv, u32 pwm_port)
{
	mutex_lock(&priv->pwm_lock);
	/*Reset PWM register before configuring it*/
	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_RST, RTS591X_PWM_CTRL_RST);

	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_CLKSRC,
			   priv->pwm_clock_src ? RTS591X_PWM_CTRL_CLKSRC : 0);

	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_INVT, 0);

	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_DIV(pwm_port),
			   RTS591X_PWM_DIV_MASK, RTS591X_DEFAULT_DIVIDER);

	/*Default 50% duty cycle*/
	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_DUTY(pwm_port),
			   RTS591X_PWM_DUTY_MASK, RTS591X_DEFAULT_DIVIDER >> 1);

	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_EN, RTS591X_PWM_CTRL_EN);

	mutex_unlock(&priv->pwm_lock);

	return 0;
}

static int rts591x_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			     const struct pwm_state *state)
{
	struct rts591x_pwm_fan_data *priv =
		container_of(chip, struct rts591x_pwm_fan_data, chip);
	u32 hwpwm = pwm->hwpwm, config_divisor, config_duty_trans;
	u64 min_period, config_period, config_duty_cycle;
	u32 sys_clock = priv->pwm_clock_src ? RTS591X_SYS_CLOCK_1 :
					      RTS591X_SYS_CLOCK_2;
	u64 dividend;
	enum pwm_polarity config_polarity = state->polarity;

	min_period = (u64)DIV_ROUND_CLOSEST(NSEC_PER_SEC, sys_clock);

	config_period = max(state->period, min_period);
	config_divisor = DIV_U64_ROUND_CLOSEST((u64)sys_clock * config_period,
					       NSEC_PER_SEC);

	if (config_period != pwm->state.period) {
		regmap_update_bits(priv->regmap,
				   priv->pwm_base + RTS591X_PWM_CTRL(hwpwm),
				   RTS591X_PWM_CTRL_EN, 0);

		regmap_update_bits(priv->regmap,
				   priv->pwm_base + RTS591X_PWM_DIV(hwpwm),
				   RTS591X_PWM_DIV_MASK, config_divisor);
		pwm->state.enabled = false;
	}

	config_duty_cycle = min(state->duty_cycle, config_period);
	dividend = config_duty_cycle * (u64)config_divisor;
	config_duty_trans = DIV_U64_ROUND_CLOSEST(dividend, config_period);
	if (config_duty_cycle != pwm->state.duty_cycle) {
		regmap_update_bits(priv->regmap,
				   priv->pwm_base + RTS591X_PWM_DUTY(hwpwm),
				   RTS591X_PWM_DUTY_MASK, config_duty_trans);
	}

	if (config_polarity != pwm->state.polarity) {
		regmap_update_bits(priv->regmap,
				   priv->pwm_base + RTS591X_PWM_CTRL(hwpwm),
				   RTS591X_PWM_CTRL_EN, 0);

		regmap_update_bits(priv->regmap,
				   priv->pwm_base + RTS591X_PWM_CTRL(hwpwm),
				   RTS591X_PWM_CTRL_INVT,
				   state->polarity ? RTS591X_PWM_CTRL_INVT : 0);
		pwm->state.enabled = false;
	}

	if (state->enabled != pwm->state.enabled) {
		regmap_update_bits(priv->regmap,
				   priv->pwm_base + RTS591X_PWM_CTRL(hwpwm),
				   RTS591X_PWM_CTRL_EN,
				   state->enabled ? RTS591X_PWM_CTRL_EN : 0);
	}

	return 0;
}

static int rts591x_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				 struct pwm_state *state)
{
	struct rts591x_pwm_fan_data *priv =
		container_of(chip, struct rts591x_pwm_fan_data, chip);
	u32 divisor, hwpwm = pwm->hwpwm, duty_cycle, val;
	u64 period;
	enum pwm_polarity polarity;
	bool enabled;

	u32 sys_clock = priv->pwm_clock_src ? RTS591X_SYS_CLOCK_1 :
					      RTS591X_SYS_CLOCK_2;
	regmap_read(priv->regmap, priv->pwm_base + RTS591X_PWM_DUTY(hwpwm),
		    &duty_cycle);

	regmap_read(priv->regmap, priv->pwm_base + RTS591X_PWM_DIV(hwpwm),
		    &divisor);
	regmap_read(priv->regmap, priv->pwm_base + RTS591X_PWM_CTRL(hwpwm),
		    &val);
	period = DIV_U64_ROUND_CLOSEST((u64)divisor, sys_clock) * NSEC_PER_SEC;
	enabled = FIELD_GET(RTS591X_PWM_CTRL_EN, val);
	polarity = FIELD_GET(RTS591X_PWM_CTRL_INVT, val);

	state->period = period;
	state->duty_cycle = duty_cycle;
	state->polarity = polarity;
	state->enabled = enabled;
	return 0;
}

static const struct pwm_ops rts591x_pwm_ops = {
	.apply = rts591x_pwm_apply,
	.get_state = rts591x_pwm_get_state,
};
static struct pwm_device *rts591x_pwm_xlate(struct pwm_chip *chip,
					    const struct of_phandle_args *args)
{
	struct pwm_device *pwm;

	if (chip->of_pwm_n_cells < 2)
		return ERR_PTR(-EINVAL);

	if (args->args_count < 2)
		return ERR_PTR(-EINVAL);

	if (args->args[0] >= chip->npwm)
		return ERR_PTR(-EINVAL);

	pwm = pwm_request_from_chip(chip, args->args[0], NULL);
	if (IS_ERR(pwm))
		return pwm;

	pwm->args.period = args->args[1];
	pwm->args.polarity = PWM_POLARITY_NORMAL;

	if (chip->of_pwm_n_cells >= 2) {
		if (args->args_count > 2 &&
		    args->args[2] & PWM_POLARITY_INVERSED)
			pwm->args.polarity = PWM_POLARITY_INVERSED;
	}

	return pwm;
}

static int rts591x_enable_tacho(struct rts591x_pwm_fan_data *priv)
{
	int index, ret;
	for (index = 0; index < RTS591X_FAN_TACH_MAX_CAHNNEL; index++) {
		if (!priv->fan_present[index])
			continue;

		regmap_update_bits(
			priv->regmap,
			priv->fan_base + RTS591X_FAN_TACH_CTRL(index),
			RTS591X_FAN_TACH_SELEDGE | RTS591X_FAN_TACH_READMODE |
				RTS591X_FAN_TACH_FILTEREN,
			((priv->fan_edge_sel << 2) & RTS591X_FAN_TACH_SELEDGE) |
				RTS591X_FAN_TACH_READMODE |
				RTS591X_FAN_TACH_FILTEREN);

		regmap_update_bits(
			priv->regmap,
			priv->fan_base + RTS591X_FAN_TACH_LIMITH(index),
			RTS591X_FAN_TACH_MASK, RTS591X_FAN_TACH_MASK);

		regmap_update_bits(priv->regmap,
				   priv->fan_base +
					   RTS591X_FAN_TACH_LIMITL(index),
				   RTS591X_FAN_TACH_MASK, 0);
		/*Enable fan channel*/
		ret = regmap_update_bits(
			priv->regmap,
			priv->fan_base + RTS591X_FAN_TACH_CTRL(index),
			RTS591X_FAN_TACH_EN, RTS591X_FAN_TACH_EN);
		if (ret) {
			dev_err(priv->dev, "Enable fan channel %d failed",
				index);
			return ret;
		}
	}

	schedule_delayed_work(&priv->poll_work, RTS591X_POLLING_TIMER_PERIOD);

	return ret;
}

static int rts591x_enable_pwm_fan(struct device *dev, struct device_node *child,
				  struct rts591x_pwm_fan_data *priv)
{
	u8 fan_ch;
	u32 pwm_port;
	int ret;

	ret = of_property_read_u32(child, "reg", &pwm_port);
	if (ret)
		return ret;

	rts591x_en_pwm_port(priv, pwm_port);
	ret = of_property_read_u8(child, "fan-tach-ch", &fan_ch);
	if (ret) {
		dev_err(dev, "Get tacho chan failed\n");
		return ret;
	}
	priv->fan_present[fan_ch] = true;
	priv->fan_dev[fan_ch].pulses_per_revolution =
		RTS591X_FAN_PULSES_RESOLUTION;
	priv->fan_dev[fan_ch].min = 0;
	priv->fan_dev[fan_ch].max = 0x7530; // Max rpm = 30000

	return 0;
}

static int rts591x_dt_pwm_init(struct rts591x_pwm_fan_data *priv)
{
	struct device *dev = priv->dev;
	u32 clock_src;
	int ret;

	ret = of_property_read_u32(dev->of_node, "reg", &priv->pwm_base);
	if (ret) {
		dev_err(dev, "Failed to get base address for pwm device");
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "clock-source", &clock_src);
	if (ret) {
		dev_warn(
			dev,
			"'clock-source' attribute not exist, use the default[Clock_SRC: PLL/2]");
		priv->pwm_clock_src = 0;
	} else {
		priv->pwm_clock_src = clock_src;
	}

	return 0;
};

static int rts591x_fan_Init(struct rts591x_pwm_fan_data *priv)
{
	struct device *dev = priv->dev;
	struct device_node *fans_node, *child;
	u8 edge_selection;
	int ret;

	fans_node = of_get_child_by_name(dev->of_node, "fans");
	if (!fans_node) {
		dev_warn(dev, "Failed to get fans node");
		return -ENODEV;
	}

	ret = of_property_read_u32(fans_node, "reg", &priv->fan_base);
	if (ret) {
		dev_err(dev, "Failed to get base address for fan device");
		return -ENODEV;
	}

	ret = of_property_read_u8(fans_node, "edge-selection", &edge_selection);
	if (ret) {
		dev_warn(dev, "edge_selection not exist, use the default");
		priv->fan_edge_sel = 0;
	} else {
		priv->fan_edge_sel = edge_selection;
	}

	for_each_child_of_node(fans_node, child) {
		ret = rts591x_enable_pwm_fan(dev, child, priv);
		if (ret) {
			of_node_put(child);
			dev_err(dev, "Enable fan tacho failed\n");
			return ret;
		}
	}
	of_node_put(fans_node);
	return 0;
};

static int rts591x_pwm_fan_probe(struct platform_device *pdev)
{
	int ret, index;
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	struct device *hwmon;
	struct rts591x_mfd_dev *mfd_dev;
	struct rts591x_pwm_fan_data *priv;
	const char *name;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mfd_dev = dev_get_drvdata(parent);
	if (!mfd_dev) {
		dev_err(dev, "Failed to get mfd_dev data\n");
		return -EINVAL;
	}

	priv->dev = dev;
	priv->regmap = dev_get_regmap(parent, NULL);
	if (!priv->regmap) {
		dev_err(dev, "Failed to get regmap");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, priv);

	mutex_init(&priv->pwm_lock);
	for (index = 0; index < RTS591X_FAN_TACH_MAX_CAHNNEL; index++) {
		mutex_init(&priv->fan_lock[index]);
	}

	ret = rts591x_dt_pwm_init(priv);
	if (ret) {
		dev_err(dev, "Init pwm failed:%d", ret);
		return ret;
	}

	ret = rts591x_fan_Init(priv);
	if (ret) {
		dev_warn(dev, "Init fan failed or no fan devices exist");
		goto out;
	}

	INIT_DELAYED_WORK(&priv->poll_work, rts591x_sample_handler);
	priv->sample_work = true;

	ret = rts591x_enable_tacho(priv);
	if (ret) {
		dev_err(dev, "RTS591X PWM-FAN Driver probe failed");
		return ret;
	}
	name = devm_kasprintf(dev, GFP_KERNEL, "%s", dev->of_node->name);
	hwmon = devm_hwmon_device_register_with_info(dev, name, priv,
						     &rts591x_chip_info, NULL);
	if (IS_ERR(hwmon)) {
		dev_err(dev,
			"unable to register rts591x_pwm_fan hwmon device\n");
		return PTR_ERR(hwmon);
	}
out:
	priv->chip.dev = dev;
	priv->chip.npwm = RTS591X_PWM_MAX_CAHNNEL;
	priv->chip.of_pwm_n_cells = 3;
	priv->chip.of_xlate = rts591x_pwm_xlate;
	priv->chip.ops = &rts591x_pwm_ops;

	ret = devm_pwmchip_add(dev, &priv->chip);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add PWM chip\n");

	pr_info("RTS591X PWM-FAN Driver probed");

	return 0;
}

static void rts591x_pwn_fan_shutdown(struct platform_device *pdev)
{
	struct rts591x_pwm_fan_data *priv = platform_get_drvdata(pdev);
	if (priv->sample_work) {
		cancel_delayed_work_sync(&priv->poll_work);
	}
}

static const struct of_device_id of_pwm_fan_match_table[] = {
	{
		.compatible = "realtek,rts591x-pwm-tacho",
	},
};

static struct  platform_driver rts591x_pwm_fan_driver = {
    .probe		= rts591x_pwm_fan_probe,
	.shutdown = rts591x_pwn_fan_shutdown,
	.driver		= {
		.name	= "rts591x_pwm_fan",
		.of_match_table = of_pwm_fan_match_table,
	},

};

module_platform_driver(rts591x_pwm_fan_driver);

MODULE_DESCRIPTION("PWM and Fan Tacho driver for RTS591x");
MODULE_LICENSE("GPL");