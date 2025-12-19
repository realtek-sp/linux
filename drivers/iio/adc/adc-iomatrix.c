#include <linux/bitfield.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/regmap.h>
#include <linux/mfd/iomatrix.h>

/*Control Register*/
#define RTS591X_ADC_CTRL	    (0x00)
#define RTS591X_ADC_CTRL_RPTDNINTEN BIT(5)
#define RTS591X_ADC_CTRL_SGLDNINTEN BIT(4)
#define RTS591X_ADC_CTRL_MDSEL	    BIT(3)
#define RTS591X_ADC_CTRL_RST	    BIT(2)
#define RTS591X_ADC_CTRL_START	    BIT(1)
#define RTS591X_ADC_CTRL_EN	    BIT(0)

/*Channel Control Register*/
#define RTS591X_ADC_CHCTRL	    (0x04)
#define RTS591X_ADC_CHCTRL_CALBP    BIT(24)
#define RTS591X_ADC_CHCTRL_LPFBP(x) (BIT(x) << 12)
#define RTS591X_ADC_CHCTRL_CHEN(x)  BIT(x)

/*Status Register*/
#define RTS591X_ADC_STS		(0x08)
#define RTS591X_ADC_STS_LPFSTB	BIT(17)
#define RTS591X_ADC_STS_RDY	BIT(16)
#define RTS591X_ADC_STS_RPTDN	BIT(13)
#define RTS591X_ADC_STS_SGLDN	BIT(12)
#define RTS591X_ADC_STS_CHDN(x) BIT(x)

/*Channel Data Register*/
#define RTS591X_ADC_CH0DATA	    (0x0c)
#define RTS591X_ADC_CH1DATA	    (0x10)
#define RTS591X_ADC_CH2DATA	    (0x14)
#define RTS591X_ADC_CH3DATA	    (0x18)
#define RTS591X_ADC_CH4DATA	    (0x1c)
#define RTS591X_ADC_CH5DATA	    (0x20)
#define RTS591X_ADC_CH6DATA	    (0x24)
#define RTS591X_ADC_CH7DATA	    (0x28)
#define RTS591X_ADC_CHxDATA_ADDR(x) (0x0c + 4 * x)
#define RTS591X_ADC_CHDATA_MASK	    GENMASK(11, 0)

#define SINGLE_MODE			  (0)
#define REPEAT_MODE			  (1)
#define CHANNEL_MAX_NUM			  8
#define RTS591X_WAIT_ADC_READY_TIMEOUT_US (100000) /* us */
#define RTS591X_POLL_SLEEP		  (2000) /* us */
#define RTS591X_ADC_DATA_VREF		  3300 /*mV*/
#define RTS591X_ADC_DATA_BITS_NUM	  12

#define RTS591X_CHAN(_idx, _data_reg_addr)                            \
	{                                                             \
		.type = IIO_VOLTAGE,                                  \
		.indexed = 1,                                         \
		.channel = (_idx),                                    \
		.address = (_data_reg_addr),                          \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |        \
				      BIT(IIO_CHAN_INFO_PROCESSED),   \
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	}

static const struct iio_chan_spec rts591x_adc_iio_channels[] = {
	RTS591X_CHAN(0, 0x0c), RTS591X_CHAN(1, 0x10), RTS591X_CHAN(2, 0x14),
	RTS591X_CHAN(3, 0x18), RTS591X_CHAN(4, 0x1c), RTS591X_CHAN(5, 0x20),
	RTS591X_CHAN(6, 0x24), RTS591X_CHAN(7, 0x28),
};

static struct iio_chan_spec *enabled_iio_channels;

struct rts591x_channel_model_data {
	bool channel_en;
	bool low_pass_filter;
};

struct rts591x_adc_data {
	struct device *dev;
	u32 base;
	bool linear_Calibration;
	u8 mode;
	u8 num_channels;
	struct rts591x_channel_model_data channel_model_data[CHANNEL_MAX_NUM];
	struct regmap *regmap;
};

static int rts591x_adc_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan, int *val,
				int *val2, long mask)
{
	struct rts591x_adc_data *priv = iio_priv(indio_dev);
	int ret, status;

	regmap_update_bits(priv->regmap, priv->base + RTS591X_ADC_CTRL,
			   RTS591X_ADC_CTRL_START, RTS591X_ADC_CTRL_START);

	ret = regmap_read_poll_timeout(
		priv->regmap, priv->base + RTS591X_ADC_STS, status,
		(status &
		 (priv->mode ? RTS591X_ADC_STS_RPTDN : RTS591X_ADC_STS_SGLDN)),
		RTS591X_POLL_SLEEP, RTS591X_WAIT_ADC_READY_TIMEOUT_US);
	if (ret) {
		dev_err(priv->dev,
			"Timeout waiting for  conversion completed\n");
		*val = 0xffff;
		return ret;
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = regmap_read_poll_timeout(
			priv->regmap, priv->base + RTS591X_ADC_STS, status,
			(status & RTS591X_ADC_STS_CHDN(chan->channel)),
			RTS591X_POLL_SLEEP, RTS591X_WAIT_ADC_READY_TIMEOUT_US);
		if (ret) {
			dev_err(priv->dev,
				"Timeout waiting for channel conversion completed\n");
			*val = 0xffff;
			return ret;
		}
		/*Read and clear the channel conversion status*/
		regmap_read(priv->regmap, priv->base + chan->address, val);
		regmap_update_bits(priv->regmap, priv->base + RTS591X_ADC_STS,
				   RTS591X_ADC_STS_CHDN(chan->channel),
				   RTS591X_ADC_STS_CHDN(chan->channel));
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = RTS591X_ADC_DATA_VREF;
		*val2 = RTS591X_ADC_DATA_BITS_NUM;

		return IIO_VAL_FRACTIONAL_LOG2;

	case IIO_CHAN_INFO_PROCESSED:
		ret = regmap_read_poll_timeout(
			priv->regmap, priv->base + RTS591X_ADC_STS, status,
			(status & RTS591X_ADC_STS_CHDN(chan->channel)),
			RTS591X_POLL_SLEEP, RTS591X_WAIT_ADC_READY_TIMEOUT_US);
		if (ret) {
			dev_err(priv->dev,
				"Timeout waiting for channel conversion completed\n");
			*val = 0xffff;
			return ret;
		}
		/*Turn the raw value to actual voltage value(uV)*/
		regmap_read(priv->regmap, priv->base + chan->address, val);
		*val = *val * (RTS591X_ADC_DATA_VREF * 1000 /
			       (1 << RTS591X_ADC_DATA_BITS_NUM));
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int rts591x_adc_of_parse(struct rts591x_adc_data *priv)
{
	struct device *dev = priv->dev;
	struct device_node *np = dev->of_node, *ch;
	const char *mode;
	int count = 0, ret;

	priv->linear_Calibration =
		of_property_read_bool(dev->of_node, "linear-Calibration");
	ret = of_property_read_string(dev->of_node, "mode", &mode);
	if (ret || !strcmp(mode, "single")) {
		dev_info(dev, "Use default single conversion mode");
		priv->mode = SINGLE_MODE;
	} else if (!strcmp(mode, "repeat")) {
		dev_info(dev, "Use repeat conversion mode");
		priv->mode = REPEAT_MODE;
	} else {
		dev_info(
			dev,
			"Invalid mode config, use the default single conversion mode");
	}

	enabled_iio_channels = (struct iio_chan_spec *)devm_kmalloc_array(
		dev, CHANNEL_MAX_NUM, sizeof(struct iio_chan_spec), GFP_KERNEL);
	if (!enabled_iio_channels)
		return -ENOMEM;

	for_each_child_of_node(np, ch) {
		if (!of_node_name_eq(ch, "channel"))
			continue;

		u32 id;
		long tmp;
		const char *at = strchr(ch->full_name, '@');
		if (!at || kstrtol(at + 1, 0, &tmp)) {
			dev_warn(dev, "%pOF: cannot determine channel id\n",
				 ch);
			continue;
		}
		id = (u32)tmp;

		if (id >= CHANNEL_MAX_NUM) {
			dev_warn(
				dev,
				"Invalid channel number in DT or it has exits: %i\n",
				id);
			continue;
		}

		const char *en;
		if (!of_property_read_string(ch, "channel-en", &en)) {
			if (!strcmp(en, "enabled")) {
				priv->channel_model_data[id].channel_en = true;
				priv->channel_model_data[id].low_pass_filter =
					(of_property_read_bool(
						 ch, "low-pass-filter") ?
						 0 :
						 1);
				enabled_iio_channels[count] =
					(struct iio_chan_spec)RTS591X_CHAN(
						id,
						RTS591X_ADC_CHxDATA_ADDR(id));
				count++;
			}
		}
	}

	priv->num_channels = count;
	return count ? count : -ENODEV;
}

static int rts591x_adc_enable(struct rts591x_adc_data *priv)
{
	int ret, i;
	u32 status;

	/*Reset and re-enable ADC controller*/
	regmap_update_bits(priv->regmap, priv->base + RTS591X_ADC_CTRL,
			   RTS591X_ADC_CTRL_RST | RTS591X_ADC_CTRL_EN,
			   RTS591X_ADC_CTRL_RST | RTS591X_ADC_CTRL_EN);

	/*Wait for the ADC controller ready*/
	ret = regmap_read_poll_timeout(priv->regmap,
				       priv->base + RTS591X_ADC_STS, status,
				       (status & RTS591X_ADC_STS_RDY),
				       RTS591X_POLL_SLEEP,
				       RTS591X_WAIT_ADC_READY_TIMEOUT_US);
	if (ret) {
		dev_err(priv->dev, "Timeout waiting for adc ready\n");
		return ret;
	}

	/*Set the operate mode*/
	if (priv->mode == REPEAT_MODE)
		regmap_update_bits(priv->regmap, priv->base + RTS591X_ADC_CTRL,
				   RTS591X_ADC_CTRL_MDSEL,
				   RTS591X_ADC_CTRL_MDSEL);

	if (!priv->linear_Calibration)
		regmap_update_bits(priv->regmap,
				   priv->base + RTS591X_ADC_CHCTRL,
				   RTS591X_ADC_CHCTRL_CALBP,
				   RTS591X_ADC_CHCTRL_CALBP);
	/*Enable the ADC channels which want to convert*/
	for (i = 0; i < CHANNEL_MAX_NUM; i++) {
		if (priv->channel_model_data[i].channel_en) {
			regmap_update_bits(priv->regmap,
					   priv->base + RTS591X_ADC_CHCTRL,
					   RTS591X_ADC_CHCTRL_CHEN(i),
					   RTS591X_ADC_CHCTRL_CHEN(i));

			if (!priv->channel_model_data[i].low_pass_filter)
				regmap_update_bits(priv->regmap,
						   priv->base +
							   RTS591X_ADC_CHCTRL,
						   RTS591X_ADC_CHCTRL_LPFBP(i),
						   RTS591X_ADC_CHCTRL_LPFBP(i));
		}
	}

	return 0;
}

static int rts591x_adc_disable(struct rts591x_adc_data *priv)
{
	regmap_update_bits(priv->regmap, priv->base + RTS591X_ADC_CTRL,
			   RTS591X_ADC_CTRL_EN | RTS591X_ADC_CTRL_RST, 0);

	regmap_update_bits(priv->regmap, priv->base + RTS591X_ADC_CTRL,
			   RTS591X_ADC_CTRL_RST, RTS591X_ADC_CTRL_RST);

	return 0;
}

static const struct iio_info rts591x_adc_iio_info = {
	.read_raw = rts591x_adc_read_raw,
};

static int rts591x_adc_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	struct iio_dev *indio_dev;
	struct rts591x_mfd_dev *mfd_dev;
	struct rts591x_adc_data *priv;

	mfd_dev = dev_get_drvdata(parent);
	if (!mfd_dev) {
		dev_err(dev, "Failed to get mfd_dev data\n");
		return -EINVAL;
	}

	indio_dev = devm_iio_device_alloc(dev, sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	priv = iio_priv(indio_dev);
	priv->dev = dev;
	platform_set_drvdata(pdev, indio_dev);

	ret = of_property_read_u32(dev->of_node, "reg", &priv->base);
	if (ret) {
		dev_err(dev, "Failed to get base addr\n");
		return ret;
	}

	priv->regmap = mfd_dev->regmap;
	if (!(priv->regmap)) {
		dev_err(dev, "Failed to get regmap\n");
		return -EINVAL;
	}

	ret = rts591x_adc_of_parse(priv);
	if (ret <= 0) {
		dev_err(priv->dev, "No Channel enabled\n");
		return ret;
	}
	/*Init ADC*/
	ret = rts591x_adc_enable(priv);
	if (ret) {
		dev_err(priv->dev, "Enable ADC Failed\n");
		return ret;
	}
	/*Register Device*/
	indio_dev->name = "rts591x_adc";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &rts591x_adc_iio_info;
	indio_dev->num_channels = priv->num_channels;
	indio_dev->channels = devm_krealloc_array(dev, enabled_iio_channels,
						  priv->num_channels,
						  sizeof(struct iio_chan_spec),
						  GFP_KERNEL);

	if (!indio_dev->channels)
		indio_dev->channels = rts591x_adc_iio_channels;

	ret = iio_device_register(indio_dev);
	return ret;
}

static int rts591x_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	rts591x_adc_disable(indio_dev->priv);
	iio_device_unregister(indio_dev);

	return 0;
}

static const struct of_device_id rts591x_adc_of_match[] = {
	{
		.compatible = "realtek,rts591x-adc",
	},
};

static struct platform_driver rts591x_adc_driver = {
    .probe = rts591x_adc_probe,
    .remove = rts591x_adc_remove,
	.driver		= {
		.name	= "rts591x-adc",
		.of_match_table = rts591x_adc_of_match,
	},
};

module_platform_driver(rts591x_adc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ADC sub-driver for RTS591x using MFD");
