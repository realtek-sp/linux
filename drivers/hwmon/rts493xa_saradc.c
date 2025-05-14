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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#define DRVNAME "rts493xa_saradc"

#define SYS_SAR_CFG	   0
#define SYS_SAR_ADC_STABLE 4
#define SYS_SAR_DAT0	   8
#define SYS_SAR_DAT1	   0xc
#define SYS_SAR_DAT2	   0x10
#define SYS_SAR_DAT3	   0x14
#define SYS_SAR_DIV_CNT	   0x18

#define CFG_SAR_EN     BIT(21)
#define CFG_SAR_CH0_EN BIT(11)
#define CFG_SAR_CH1_EN BIT(10)
#define CFG_SAR_CH2_EN BIT(9)
#define CFG_SAR_CH3_EN BIT(8)

struct saradc {
	struct device *hwmon_dev;
	struct mutex lock;
	u32 channels;
	void __iomem *mmio_base;
};

static void __iomem *adc_mapped_addr;

/* sysfs hook function */
static ssize_t saradc_read(struct device *dev, struct device_attribute *devattr,
			   char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct saradc *adc = platform_get_drvdata(pdev);
	u32 value, status;
	int i = attr->index;

	value = readl(adc->mmio_base + SYS_SAR_ADC_STABLE);
	if (value == 0)
		return -ERESTARTSYS;

	if (mutex_lock_interruptible(&adc->lock))
		return -ERESTARTSYS;

	if (i < 3)
		i++;
	else if (i == 3)
		i = 0;
	value = readl(adc->mmio_base + SYS_SAR_DAT0 + (i << 2));
	dev_dbg(dev, "raw value = 0x%x\n", value);
	value &= 0x3fc;
	value = value * 3 + (value * 57 + 128) / 256;
	status = sprintf(buf, "%d\n", value);

	mutex_unlock(&adc->lock);
	return status;
}

u32 saradc_read_in(int i)
{
	u32 value = 0;

	if (adc_mapped_addr) {
		if (i < 3)
			i++;
		else if (i == 3)
			i = 0;
		value = readl(adc_mapped_addr + SYS_SAR_DAT0 + (i << 2));
		value &= 0x3fc;
		value = value * 3 + (value * 57 + 128) / 256;
	}

	return value;
}
EXPORT_SYMBOL_GPL(saradc_read_in);

static ssize_t saradc_show_name(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	return sprintf(buf, "%s\n", to_platform_device(dev)->name);
}

static struct sensor_device_attribute ad_input[] = {
	SENSOR_ATTR(name, 0444, saradc_show_name, NULL, 0),
	SENSOR_ATTR(in0_input, 0444, saradc_read, NULL, 0),
	SENSOR_ATTR(in1_input, 0444, saradc_read, NULL, 1),
	SENSOR_ATTR(in2_input, 0444, saradc_read, NULL, 2),
	SENSOR_ATTR(in3_input, 0444, saradc_read, NULL, 3),
};

/*----------------------------------------------------------------------*/

static int saradc_probe(struct platform_device *pdev)
{
	int channels = 4;
	struct saradc *adc;
	int status;
	int i;
	struct resource *r;
	u32 value;
	struct clk *pclk;
	u32 xb2rate;
	u32 clkin;
	u32 chansel;

	adc = devm_kzalloc(&pdev->dev, sizeof(*adc), GFP_KERNEL);
	if (!adc)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adc->mmio_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(adc->mmio_base))
		return PTR_ERR(adc->mmio_base);

	adc_mapped_addr = adc->mmio_base;

	platform_set_drvdata(pdev, adc);

	/* set a default value for the reference */
	adc->channels = channels;

	pclk = clk_get(&pdev->dev, "xb2_ck");
	if (IS_ERR(pclk)) {
		dev_dbg(&pdev->dev, "no peripheral clock\n");
		return PTR_ERR(pclk);
	}

	xb2rate = clk_get_rate(pclk);
	clk_put(pclk);

	clkin = readl(adc->mmio_base + SYS_SAR_DIV_CNT);
	clkin &= 0xff;
	clkin = (12500000 + (clkin / 2)) / clkin;
	chansel = (xb2rate * 5 + clkin / 2) / clkin;
	chansel &= 0xff;

	mutex_init(&adc->lock);

	mutex_lock(&adc->lock);

	for (i = 0; i < adc->channels + 1; i++) {
		status = device_create_file(&pdev->dev, &ad_input[i].dev_attr);
		if (status) {
			dev_err(&pdev->dev, "device_create_file failed.\n");
			goto out_err;
		}
	}

	adc->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(adc->hwmon_dev)) {
		dev_err(&pdev->dev, "hwmon_device_register failed.\n");
		status = PTR_ERR(adc->hwmon_dev);
		goto out_err;
	}

	value = readl(adc->mmio_base + SYS_SAR_CFG) & ~0xfff;
	value |= (CFG_SAR_CH0_EN | CFG_SAR_CH1_EN | CFG_SAR_CH2_EN |
		  CFG_SAR_CH3_EN | chansel | CFG_SAR_EN);

	writel(value, adc->mmio_base + SYS_SAR_CFG);

	mutex_unlock(&adc->lock);
	return 0;

out_err:
	for (i--; i >= 0; i--)
		device_remove_file(&pdev->dev, &ad_input[i].dev_attr);

	platform_set_drvdata(pdev, NULL);
	mutex_unlock(&adc->lock);
	return status;
}

static int saradc_remove(struct platform_device *pdev)
{
	struct saradc *adc = platform_get_drvdata(pdev);
	int i;

	mutex_lock(&adc->lock);
	hwmon_device_unregister(adc->hwmon_dev);
	for (i = 0; i < 3 + adc->channels; i++)
		device_remove_file(&pdev->dev, &ad_input[i].dev_attr);

	platform_set_drvdata(pdev, NULL);
	mutex_unlock(&adc->lock);

	return 0;
}

static const struct of_device_id rts493xa_saradc_match[] = {
	{
		.compatible = "realtek,rts493xa-saradc",
	},
	{}
};
MODULE_DEVICE_TABLE(of, rts493xa_saradc_match);

static struct platform_driver saradc_driver = {
	.driver = {
		.name	= "rts493xa_saradc",
		.of_match_table = rts493xa_saradc_match,
	},
	.probe	= saradc_probe,
	.remove	= saradc_remove,
};

module_platform_driver(saradc_driver);
