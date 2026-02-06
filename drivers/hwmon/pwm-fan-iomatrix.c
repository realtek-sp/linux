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
#define RTS591X_WAIT_FAN_READY_TIMEOUT_US (100000) /* us */
#define RTS591X_POLL_SLEEP		  (2000) /* us */
#define RTS591X_TACH_MODE_HZ		  100000
#define RTS591X_POLLING_TIMER_PERIOD	  msecs_to_jiffies(10) /*ms*/
#define RTS591X_PWM_MIN_DIV		  (10)
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

struct rts591x_cooling_device {
	char name[THERMAL_NAME_LENGTH];
	struct rts591x_pwm_fan_data *priv;
	struct thermal_cooling_device *tcdev;
	int pwm_port;
	u32 *cooling_levels;
	u8 max_state;
	u8 cur_state;
};

struct rts591x_pwm_fan_data {
	struct device *dev;
	u32 pwm_base;
	u32 fan_base;
	u32 pwm_clock_div;
	u8 pwm_clock_src;
	u8 fan_read_mode;
	u8 fan_edge_sel;
	bool fan_cnt_ready_int;
	struct regmap *regmap;
	bool fan_present[RTS591X_FAN_TACH_MAX_CAHNNEL];
	struct rts591x_fan_tacho_dev fan_dev[RTS591X_FAN_TACH_MAX_CAHNNEL];
	struct rts591x_cooling_device *cdev[RTS591X_PWM_MAX_CAHNNEL];
	struct mutex pwm_lock;
	struct delayed_work poll_work;
	ktime_t sample_start;
	struct mutex fan_lock[RTS591X_FAN_TACH_MAX_CAHNNEL];
};

static umode_t rts591x_is_visible(const void *priv,
				  enum hwmon_sensor_types type, u32 attr,
				  int channel)
{
	switch (type) {
	case hwmon_pwm:
		return 0644;
	case hwmon_fan:
		return 0444;
	default:
		return 0;
	}
}
static int rts591x_read_pwm(struct device *dev, u32 attr, int channel,
			    long *val)
{
	struct rts591x_pwm_fan_data *priv = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&priv->pwm_lock);
	switch (attr) {
	case hwmon_pwm_input:
		ret = regmap_read(priv->regmap,
				  priv->pwm_base + RTS591X_PWM_DUTY(channel),
				  (u32)val);
		break;
	default:
		dev_err(priv->dev, "Read pwm%d failed\n", channel);
		ret = EOPNOTSUPP;
	}
	mutex_unlock(&priv->pwm_lock);
	return ret;
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

static int set_pwm(struct rts591x_pwm_fan_data *priv, u32 pwm_port, long val)
{
	int ret;
	ret = regmap_update_bits(priv->regmap,
				 priv->pwm_base + RTS591X_PWM_DUTY(pwm_port),
				 RTS591X_PWM_DUTY_MASK, (u32)(val));
	if (ret) {
		dev_err(priv->dev, "Update pwm duty failed:%d", ret);
		return ret;
	}
	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_EN, RTS591X_PWM_CTRL_EN);
	return 0;
}

static int rts591x_write_pwm(struct device *dev, u32 attr, int channel,
			     long val)
{
	struct rts591x_pwm_fan_data *priv = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&priv->pwm_lock);
	switch (attr) {
	case hwmon_pwm_input:
		if (val < 0 || val > priv->pwm_clock_div) {
			ret = -EINVAL;
			goto out;
		} else if (val < priv->pwm_clock_div / RTS591X_PWM_MIN_DIV) {
			val = priv->pwm_clock_div / RTS591X_PWM_MIN_DIV;
		}
		ret = set_pwm(priv, channel, val);
		if (ret) {
			goto out;
		}
		break;
	default:
		ret = EOPNOTSUPP;
	}
out:
	mutex_unlock(&priv->pwm_lock);
	return ret;
}

static int rts591x_read(struct device *dev, enum hwmon_sensor_types type,
			u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_pwm:
		return rts591x_read_pwm(dev, attr, channel, val);
	case hwmon_fan:
		return rts591x_read_fan(dev, attr, channel, val);
	default:
		return -EOPNOTSUPP;
	}
}

static int rts591x_write(struct device *dev, enum hwmon_sensor_types type,
			 u32 attr, int channel, long val)
{
	switch (type) {
	case hwmon_pwm:
		return rts591x_write_pwm(dev, attr, channel, val);
	default:
		return -EOPNOTSUPP;
	}
}
static const u32 rts591x_pwm_config[] = {
	HWMON_PWM_INPUT, HWMON_PWM_INPUT, HWMON_PWM_INPUT,
	HWMON_PWM_INPUT, HWMON_PWM_INPUT, HWMON_PWM_INPUT,
	HWMON_PWM_INPUT, HWMON_PWM_INPUT, 0
};

static const struct hwmon_channel_info rts591x_pwm = {
	.type = hwmon_pwm,
	.config = rts591x_pwm_config,
};

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

static const struct hwmon_channel_info *rts591x_info[] = { &rts591x_pwm,
							   &rts591x_fan, NULL };

static const struct hwmon_ops rts591x_hwmon_ops = {
	.is_visible = rts591x_is_visible,
	.read = rts591x_read,
	.write = rts591x_write,
};

static const struct hwmon_chip_info rts591x_chip_info = {
	.ops = &rts591x_hwmon_ops,
	.info = rts591x_info,
};

static ssize_t divider_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct rts591x_pwm_fan_data *priv = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", priv->pwm_clock_div);
}

static SENSOR_DEVICE_ATTR_RO(pwm_divider1, divider, 0);
static SENSOR_DEVICE_ATTR_RO(pwm_divider2, divider, 1);
static SENSOR_DEVICE_ATTR_RO(pwm_divider3, divider, 2);
static SENSOR_DEVICE_ATTR_RO(pwm_divider4, divider, 3);
static SENSOR_DEVICE_ATTR_RO(pwm_divider5, divider, 4);
static SENSOR_DEVICE_ATTR_RO(pwm_divider6, divider, 5);
static SENSOR_DEVICE_ATTR_RO(pwm_divider7, divider, 6);
static SENSOR_DEVICE_ATTR_RO(pwm_divider8, divider, 7);

static struct attribute *divider_attrs[] = {
	&sensor_dev_attr_pwm_divider1.dev_attr.attr,
	&sensor_dev_attr_pwm_divider2.dev_attr.attr,
	&sensor_dev_attr_pwm_divider3.dev_attr.attr,
	&sensor_dev_attr_pwm_divider4.dev_attr.attr,
	&sensor_dev_attr_pwm_divider5.dev_attr.attr,
	&sensor_dev_attr_pwm_divider6.dev_attr.attr,
	&sensor_dev_attr_pwm_divider7.dev_attr.attr,
	&sensor_dev_attr_pwm_divider8.dev_attr.attr,
	NULL,
};

static const struct attribute_group pwm_divider_group = {
	.attrs = divider_attrs,
};

static const struct attribute_group *rts591x_costum_groups[] = {
	&pwm_divider_group, NULL
};

static void rts591x_sample_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rts591x_pwm_fan_data *priv =
		container_of(dwork, struct rts591x_pwm_fan_data, poll_work);

	unsigned int delta = ktime_ms_delta(ktime_get(), priv->sample_start);
	int i;

	if (delta) {
		for (i = 0; i < RTS591X_FAN_TACH_MAX_CAHNNEL; i++) {
			if (!priv->fan_present[i])
				continue;

			u32 counter;
			regmap_read(priv->regmap,
				    priv->fan_base + RTS591X_FAN_TACH_CTRL(i),
				    &counter);
			counter = FIELD_GET(RTS591X_FAN_TACH_CTRL_CNT, counter);
			if (counter >= priv->fan_dev[i].counter) {
				priv->fan_dev[i].rpm = DIV_ROUND_CLOSEST(
					(counter - priv->fan_dev[i].counter) *
						1000 * 60,
					delta * RTS591X_FAN_PULSES_RESOLUTION);
			} else {
				priv->fan_dev[i].rpm = DIV_ROUND_CLOSEST(
					(0xFFFF - priv->fan_dev[i].counter +
					 counter) *
						1000 * 60,
					delta * RTS591X_FAN_PULSES_RESOLUTION);
			}
			priv->fan_dev[i].counter = counter;
		}
	}
	priv->sample_start = ktime_get();
	schedule_delayed_work(&priv->poll_work, RTS591X_POLLING_TIMER_PERIOD);
}

static irqreturn_t rts591x_fan_isr(int irq, void *dev_id)
{
	struct rts591x_pwm_fan_data *priv = dev_id;
	u32 val, counter;
	int ret;
	int index = irq - priv->fan_dev[0].irq;
	if (index < 0 || index > RTS591X_TACHO3_INT) {
		dev_err(priv->dev, "Invalid channel:%d", index);
		return IRQ_HANDLED;
	}

	mutex_lock(&priv->fan_lock[index]);
	ret = regmap_read_poll_timeout(
		priv->regmap, priv->fan_base + RTS591X_FAN_TACH_STS(index), val,
		(val & RTS591X_FAN_TACH_STS_CNTRDY), RTS591X_POLL_SLEEP,
		RTS591X_WAIT_FAN_READY_TIMEOUT_US);
	if (ret) {
		dev_err(priv->dev,
			"Timeout waiting for read interrupt for fan counter ready\n");
		priv->fan_dev[index].rpm = 0;
		priv->fan_dev[index].counter = 0;
		goto out;
	}

	regmap_read(priv->regmap, priv->fan_base + RTS591X_FAN_TACH_CTRL(index),
		    &counter);
	priv->fan_dev[index].counter =
		FIELD_GET(RTS591X_FAN_TACH_CTRL_CNT, counter);
	priv->fan_dev[index].rpm = calulate_rpm(priv, index);
out:
	/*clear interrupt bits*/
	regmap_write(priv->regmap, priv->fan_base + RTS591X_FAN_TACH_STS(index),
		     BIT(3));
	mutex_unlock(&priv->fan_lock[index]);

	return IRQ_HANDLED;
}

static int rts591x_en_pwm_port(struct rts591x_pwm_fan_data *priv, u32 pwm_port)
{
	mutex_lock(&priv->pwm_lock);

	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_CLKSRC,
			   priv->pwm_clock_src ? RTS591X_PWM_CTRL_CLKSRC : 0);
	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_INVT, 0);
	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_DIV(pwm_port),
			   RTS591X_PWM_DIV_MASK, priv->pwm_clock_div);

	/*Default 50% duty cycle*/
	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_DUTY(pwm_port),
			   RTS591X_PWM_DUTY_MASK, priv->pwm_clock_div >> 1);

	regmap_update_bits(priv->regmap,
			   priv->pwm_base + RTS591X_PWM_CTRL(pwm_port),
			   RTS591X_PWM_CTRL_EN, RTS591X_PWM_CTRL_EN);

	mutex_unlock(&priv->pwm_lock);

	return 0;
}

static int rts591x_pwm_cz_get_max_state(struct thermal_cooling_device *tcdev,
					unsigned long *state)
{
	struct rts591x_cooling_device *cdev = tcdev->devdata;

	*state = cdev->max_state;

	return 0;
}

static int rts591x_pwm_cz_get_cur_state(struct thermal_cooling_device *tcdev,
					unsigned long *state)
{
	struct rts591x_cooling_device *cdev = tcdev->devdata;

	*state = cdev->cur_state;

	return 0;
}

static int rts591x_pwm_cz_set_cur_state(struct thermal_cooling_device *tcdev,
					unsigned long state)
{
	int ret;
	struct rts591x_cooling_device *cdev = tcdev->devdata;

	if (state > cdev->max_state)
		return -EINVAL;
	cdev->cur_state = state;
	mutex_lock(&cdev->priv->pwm_lock);
	ret = set_pwm(cdev->priv, cdev->pwm_port,
		      cdev->cooling_levels[cdev->cur_state]);
	mutex_unlock(&cdev->priv->pwm_lock);
	return ret;
}

static const struct thermal_cooling_device_ops rts591x_pwm_cool_ops = {
	.get_max_state = rts591x_pwm_cz_get_max_state,
	.get_cur_state = rts591x_pwm_cz_get_cur_state,
	.set_cur_state = rts591x_pwm_cz_set_cur_state,
};

static int rts591x_create_pwm_cooling(struct device *dev,
				      struct device_node *child,
				      struct rts591x_pwm_fan_data *priv,
				      u32 pwm_port, u8 num_levels)
{
	int ret;
	struct rts591x_cooling_device *cdev;

	cdev = devm_kzalloc(dev, sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return -ENOMEM;

	cdev->cooling_levels = devm_kzalloc(dev, num_levels, GFP_KERNEL);
	if (!cdev->cooling_levels)
		return -ENOMEM;

	cdev->max_state = num_levels - 1;
	ret = of_property_read_u32_array(child, "cooling-levels",
					 cdev->cooling_levels, num_levels);
	if (ret) {
		dev_err(dev, "Property 'cooling-levels' cannot be read.\n");
		return ret;
	}
	snprintf(cdev->name, THERMAL_NAME_LENGTH, "%s%d", child->name,
		 pwm_port);

	cdev->tcdev = thermal_of_cooling_device_register(
		child, cdev->name, cdev, &rts591x_pwm_cool_ops);
	if (IS_ERR(cdev->tcdev))
		return PTR_ERR(cdev->tcdev);

	cdev->priv = priv;
	cdev->pwm_port = pwm_port;

	priv->cdev[pwm_port] = cdev;

	return 0;
}

static int rts591x_enable_tacho(struct rts591x_pwm_fan_data *priv)
{
	int index, ret;
	for (index = 0; index < RTS591X_FAN_TACH_MAX_CAHNNEL; index++) {
		if (!priv->fan_present[index])
			continue;

		if (priv->fan_cnt_ready_int) {
			/*Default use 100KHZ read mode, low-filter-pass*/
			regmap_update_bits(priv->regmap,
					   priv->fan_base +
						   RTS591X_FAN_TACH_CTRL(index),
					   RTS591X_FAN_TACH_SELEDGE |
						   RTS591X_FAN_TACH_READMODE |
						   RTS591X_FAN_TACH_FILTEREN,
					   ((priv->fan_edge_sel << 2) &
					    RTS591X_FAN_TACH_SELEDGE) |
						   RTS591X_FAN_TACH_READMODE |
						   RTS591X_FAN_TACH_FILTEREN);
		} else {
			regmap_update_bits(priv->regmap,
					   priv->fan_base +
						   RTS591X_FAN_TACH_CTRL(index),
					   RTS591X_FAN_TACH_SELEDGE |
						   RTS591X_FAN_TACH_READMODE |
						   RTS591X_FAN_TACH_FILTEREN,
					   ((priv->fan_edge_sel << 2) &
					    RTS591X_FAN_TACH_SELEDGE) |
						   RTS591X_FAN_TACH_FILTEREN);
		}

		regmap_update_bits(priv->regmap,
				   priv->fan_base +
					   RTS591X_FAN_TACH_INTEN(index),
				   RTS591X_FAN_TACH_INTEN_CNTRDY,
				   priv->fan_cnt_ready_int ?
					   RTS591X_FAN_TACH_INTEN_CNTRDY :
					   0);
		/*Set the fan tacho limit 0x0000 <= rpm <= 0xFFFF*/
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
		}
	}

	if (!priv->fan_cnt_ready_int) {
		priv->sample_start = ktime_get();
		schedule_delayed_work(&priv->poll_work,
				      RTS591X_POLLING_TIMER_PERIOD);
	}

	return ret;
}

static int rts591x_enable_pwm_fan(struct device *dev, struct device_node *child,
				  struct rts591x_pwm_fan_data *priv)
{
	u8 *fan_ch;
	u32 pwm_port;
	int ret, fan_count;
	u8 index, ch;

	ret = of_property_read_u32(child, "reg", &pwm_port);
	if (ret)
		return ret;

	rts591x_en_pwm_port(priv, pwm_port);

	ret = of_property_count_u32_elems(child, "cooling-levels");
	if (ret > 0) {
		ret = rts591x_create_pwm_cooling(dev, child, priv, pwm_port,
						 ret);
		if (ret)
			return ret;
	}

	fan_count = of_property_count_u8_elems(child, "fan-tach-ch");
	if (fan_count < 1)
		return -EINVAL;

	fan_ch = devm_kcalloc(dev, fan_count, sizeof(*fan_ch), GFP_KERNEL);
	if (!fan_ch)
		return -ENOMEM;
	ret = of_property_read_u8_array(child, "fan-tach-ch", fan_ch,
					fan_count);
	if (ret)
		return ret;

	for (ch = 0; ch < fan_count; ch++) {
		index = fan_ch[ch];
		priv->fan_present[index] = true;
		priv->fan_dev[index].pulses_per_revolution =
			RTS591X_FAN_PULSES_RESOLUTION;
		priv->fan_dev[index].min = 0;
		priv->fan_dev[index].max = 0x7530;
	}

	return 0;
}

static int rts591x_pwm_init(struct rts591x_pwm_fan_data *priv)
{
	struct device *dev = priv->dev;
	struct device_node *pwms_node;
	u32 clock_config[2];
	int ret;

	pwms_node = of_get_child_by_name(dev->of_node, "pwms");
	if (!pwms_node) {
		dev_err(dev, "Failed to get pwms node");
		return -ENODEV;
	}

	ret = of_property_read_u32(pwms_node, "reg", &priv->pwm_base);
	if (ret) {
		dev_err(dev, "Failed to get base address for pwm device");
		return -ENODEV;
	}

	ret = of_property_read_u32_array(pwms_node, "clock-config",
					 clock_config,
					 ARRAY_SIZE(clock_config));
	if (ret) {
		dev_warn(dev, "clock-config not exist, use the default");
		priv->pwm_clock_div = 0x800;
		priv->pwm_clock_src = 0;
	} else {
		priv->pwm_clock_div = clock_config[0];
		priv->pwm_clock_src = clock_config[1];
	}

	of_node_put(pwms_node);
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
		dev_err(dev, "Failed to get fans node");
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

	priv->fan_cnt_ready_int =
		of_property_read_bool(fans_node, "cnt-ready-interrupt-en");

	for_each_child_of_node(fans_node, child) {
		ret = rts591x_enable_pwm_fan(dev, child, priv);
		if (ret) {
			dev_err(dev, "enable pwm and fan failed\n");
			of_node_put(child);
			return ret;
		}
	}
	of_node_put(fans_node);
	return 0;
};

static int rts591x_pwm_fan_probe(struct platform_device *pdev)
{
	int ret, virq, index;
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	struct device *hwmon;
	struct rts591x_mfd_dev *mfd_dev;
	struct rts591x_pwm_fan_data *priv;
	const char *irq_name, *name;

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

	ret = rts591x_pwm_init(priv);
	if (ret) {
		dev_err(dev, "Init pwm failed:%d", ret);
		return ret;
	}

	ret = rts591x_fan_Init(priv);
	if (ret) {
		dev_err(dev, "Init fan failed:%d", ret);
		return ret;
	}

	if (priv->fan_cnt_ready_int) {
		for (index = 0; index < RTS591X_FAN_TACH_MAX_CAHNNEL; index++) {
			if (!priv->fan_present[index])
				continue;

			virq = regmap_irq_get_virq(mfd_dev->irq_data,
						   RTS591X_TACHO0_INT + index);
			if (virq < 0) {
				dev_err(dev, "Failed to get IRQ: %d\n", virq);
				return virq;
			}
			priv->fan_dev[index].irq = virq;
			irq_name = devm_kasprintf(dev, GFP_KERNEL, "%s_%d",
						  dev->of_node->name, index);
			ret = devm_request_threaded_irq(dev, virq, NULL,
							rts591x_fan_isr,
							IRQF_ONESHOT, irq_name,
							priv);
			if (ret) {
				dev_err(dev, "register IRQ fan%d failed\n",
					ret);
				return ret;
			}
		}
	} else {
		INIT_DELAYED_WORK(&priv->poll_work, rts591x_sample_handler);
	}

	ret = rts591x_enable_tacho(priv);
	if (ret) {
		dev_err(dev, "RTS591X PWM-FAN Driver probe failed");
		return ret;
	}
	name = devm_kasprintf(dev, GFP_KERNEL, "%s", dev->of_node->name);
	strreplace((char *)name, '-', '_');
	hwmon = devm_hwmon_device_register_with_info(
		dev, name, priv, &rts591x_chip_info, rts591x_costum_groups);

	if (IS_ERR(hwmon)) {
		dev_err(dev,
			"unable to register rts591x_pwm_fan hwmon device\n");
		return PTR_ERR(hwmon);
	}

	pr_info("RTS591X PWM-FAN Driver probed");

	return 0;
}

static void rts591x_pwn_fan_shutdown(struct platform_device *pdev)
{
	struct rts591x_pwm_fan_data *priv = platform_get_drvdata(pdev);
	int index;
	if (!priv->fan_cnt_ready_int)
		cancel_delayed_work_sync(&priv->poll_work);
	else {
		for (index = 0; index < RTS591X_FAN_TACH_MAX_CAHNNEL; index++) {
			mutex_lock(&priv->fan_lock[index]);
			regmap_update_bits(priv->regmap,
					   priv->fan_base +
						   RTS591X_FAN_TACH_CTRL(index),
					   RTS591X_FAN_TACH_EN, 0);
			mutex_unlock(&priv->fan_lock[index]);
			disable_irq(priv->fan_dev[index].irq);
		}
	}
}

static const struct of_device_id of_pwm_fan_match_table[] = {
	{
		.compatible = "realtek,rts591x-pwm-fan",
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