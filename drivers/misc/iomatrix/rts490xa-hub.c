// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2021 - 2023 Intel Corporation.*/
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
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/bitfield.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/i2c.h>

#include <linux/gpio/driver.h>

#define RTS490XA_HUB_TP_MAX_COUNT 0x08

#define RTS490XA_HUB_LOGICAL_BUS_MAX_COUNT 0x08

#define GPIO_BANK_SZ  0x02
#define GPIO_MAX_BANK RTS490XA_HUB_TP_MAX_COUNT

/* RTS490XA HUB REGISTERS */

/*
 * In this driver Controller - Target convention is used. All the abbreviations are
 * based on this convention. For instance: CP - Controller Port, TP - Target Port.
 */

/* Device Information Registers */
#define RTS490XA_HUB_DEV_INFO_0 0x00
#define RTS490XA_HUB_DEV_INFO_1 0x01
#define RTS490XA_HUB_PID_5	0x02
#define RTS490XA_HUB_PID_4	0x03
#define RTS490XA_HUB_PID_3	0x04
#define RTS490XA_HUB_PID_2	0x05
#define RTS490XA_HUB_PID_1	0x06
#define RTS490XA_HUB_PID_0	0x07
#define RTS490XA_HUB_BCR	0x08
#define RTS490XA_HUB_DCR	0x09
#define RTS490XA_HUB_DEV_CAPAB	0x0A
#define RTS490XA_HUB_DEV_REV	0x0B

/* Device Configuration Registers */
#define RTS490XA_HUB_PROTECTION_CODE 0x10
#define REGISTERS_LOCK_CODE	     0x00
#define REGISTERS_UNLOCK_CODE	     0x69
#define CP1_REGISTERS_UNLOCK_CODE    0x6A

#define RTS490XA_HUB_CP_CONF   0x11
#define RTS490XA_HUB_TP_ENABLE 0x12
#define TPn_ENABLE(n)	       BIT(n)

#define RTS490XA_HUB_DEV_CONF	 0x13
#define TARGET_PORT_VCCIO_PWRGD_CTRL_MASK BIT(3)

#define RTS490XA_DPAD_POWER_BUFFER_PAGE 0x7C
#define RTS490XA_DPAD_POWER_DISABLE	0x80

#define RTS490XA_HUB_IO_STRENGTH 0x14
#define TP0145_IO_STRENGTH_MASK	 GENMASK(1, 0)
#define TP0145_IO_STRENGTH(x)	 (((x) << 0) & TP0145_IO_STRENGTH_MASK)
#define TP2367_IO_STRENGTH_MASK	 GENMASK(3, 2)
#define TP2367_IO_STRENGTH(x)	 (((x) << 2) & TP2367_IO_STRENGTH_MASK)
#define CP0_IO_STRENGTH_MASK	 GENMASK(5, 4)
#define CP0_IO_STRENGTH(x)	 (((x) << 4) & CP0_IO_STRENGTH_MASK)
#define CP1_IO_STRENGTH_MASK	 GENMASK(7, 6)
#define CP1_IO_STRENGTH(x)	 (((x) << 6) & CP1_IO_STRENGTH_MASK)
#define IO_STRENGTH_20_OHM	 0x00
#define IO_STRENGTH_30_OHM	 0x01
#define IO_STRENGTH_40_OHM	 0x02
#define IO_STRENGTH_50_OHM	 0x03

#define RTS490XA_HUB_NET_OPER_MODE_CONF 0x15
#define RTS490XA_HUB_LDO_CONF		0x16
#define CP0_LDO_VOLTAGE_MASK		GENMASK(1, 0)
#define CP0_LDO_VOLTAGE(x)		(((x) << 0) & CP0_LDO_VOLTAGE_MASK)
#define CP1_LDO_VOLTAGE_MASK		GENMASK(3, 2)
#define CP1_LDO_VOLTAGE(x)		(((x) << 2) & CP1_LDO_VOLTAGE_MASK)
#define TP0145_LDO_VOLTAGE_MASK		GENMASK(5, 4)
#define TP0145_LDO_VOLTAGE(x)		(((x) << 4) & TP0145_LDO_VOLTAGE_MASK)
#define TP2367_LDO_VOLTAGE_MASK		GENMASK(7, 6)
#define TP2367_LDO_VOLTAGE(x)		(((x) << 6) & TP2367_LDO_VOLTAGE_MASK)
#define LDO_VOLTAGE_1_0V		0x00
#define LDO_VOLTAGE_1_1V		0x01
#define LDO_VOLTAGE_1_2V		0x02
#define LDO_VOLTAGE_1_8V		0x03

#define RTS490XA_HUB_TP_IO_MODE_CONF  0x17
#define RTS490XA_HUB_TP_SMBUS_AGNT_EN 0x18
#define TPn_SMBUS_MODE_EN(n)	      BIT(n)

#define RTS490XA_HUB_LDO_AND_PULLUP_CONF 0x19
#define LDO_ENABLE_DISABLE_MASK		 GENMASK(3, 0)
#define CP0_LDO_EN			 BIT(0)
#define CP1_LDO_EN			 BIT(1)
/*
 * RTS490XA HUB does not provide a way to control LDO or pull-up for individual ports. It is possible
 * for group of ports TP0/TP1/TP4/TP5 and TP2/TP3/TP6/TP7.
 */
#define TP0145_LDO_EN			 BIT(2)
#define TP2367_LDO_EN			 BIT(3)
#define TP0145_PULLUP_CONF_MASK		 GENMASK(7, 6)
#define TP0145_PULLUP_CONF(x)		 (((x) << 6) & TP0145_PULLUP_CONF_MASK)
#define TP2367_PULLUP_CONF_MASK		 GENMASK(5, 4)
#define TP2367_PULLUP_CONF(x)		 (((x) << 4) & TP2367_PULLUP_CONF_MASK)
#define PULLUP_250R			 0x00
#define PULLUP_500R			 0x01
#define PULLUP_1K			 0x02
#define PULLUP_2K			 0x03

#define RTS490XA_HUB_CP_IBI_CONF      0x1A
#define RTS490XA_HUB_TP_IBI_CONF      0x1B
#define RTS490XA_HUB_IBI_MDB_CUSTOM   0x1C
#define RTS490XA_HUB_JEDEC_CONTEXT_ID 0x1D
#define RTS490XA_HUB_TP_GPIO_MODE_EN  0x1E
#define TPn_GPIO_MODE_EN(n)	      BIT(n)

/* Controller Port Control/Status Registers */
#define RTS490XA_HUB_CP_MUX_SET		      0x38
#define CONTROLLER_PORT_MUX_REQ		      BIT(0)
#define RTS490XA_HUB_CP_MUX_STS		      0x39
#define CONTROLLER_PORT_MUX_CONNECTION_STATUS BIT(0)

/* Target Ports Control Registers */
#define RTS490XA_HUB_TP_SMBUS_AGNT_TRANS_START 0x50
#define RTS490XA_HUB_TP_NET_CON_CONF	       0x51
#define TPn_NET_CON(n)			       BIT(n)

#define RTS490XA_HUB_TP_PULLUP_EN 0x53
#define TPn_PULLUP_EN(n)	  BIT(n)

#define RTS490XA_HUB_TP_SCL_OUT_EN    0x54
#define RTS490XA_HUB_TP_SDA_OUT_EN    0x55
#define RTS490XA_HUB_TP_SCL_OUT_LEVEL 0x56
#define RTS490XA_HUB_TP_SDA_OUT_LEVEL 0x57

#define RTS490XA_HUB_TP_IN_DETECT_MODE_CONF 0x58
#define SCL0145_IO_IN_DET_CFG_MASK	    GENMASK(1, 0)
#define SCL0145_IO_IN_DET_CFG(x)	    (((x) << 0) & SCL0145_IO_IN_DET_CFG_MASK)
#define SDA0145_IO_IN_DET_CFG_MASK	    GENMASK(3, 2)
#define SDA0145_IO_IN_DET_CFG(x)	    (((x) << 2) & SDA0145_IO_IN_DET_CFG_MASK)
#define SCL2367_IO_IN_DET_CFG_MASK	    GENMASK(5, 4)
#define SCL2367_IO_IN_DET_CFG(x)	    (((x) << 4) & SCL2367_IO_IN_DET_CFG_MASK)
#define SDA2367_IO_IN_DET_CFG_MASK	    GENMASK(7, 6)
#define SDA2367_IO_IN_DET_CFG(x)	    (((x) << 6) & SDA2367_IO_IN_DET_CFG_MASK)

/* Target Ports Status Registers */
#define RTS490XA_HUB_TP_SCL_IN_LEVEL_STS  0x60
#define RTS490XA_HUB_TP_SDA_IN_LEVEL_STS  0x61
#define RTS490XA_HUB_TP_SCL_IN_DETECT_FLG 0x62
#define RTS490XA_HUB_TP_SDA_IN_DETECT_FLG 0x63

/* SMBus Agent Configuration and Status Registers */
#define RTS490XA_HUB_TP0_SMBUS_AGNT_STS		   0x64
#define RTS490XA_HUB_TP1_SMBUS_AGNT_STS		   0x65
#define RTS490XA_HUB_TP2_SMBUS_AGNT_STS		   0x66
#define RTS490XA_HUB_TP3_SMBUS_AGNT_STS		   0x67
#define RTS490XA_HUB_TP4_SMBUS_AGNT_STS		   0x68
#define RTS490XA_HUB_TP5_SMBUS_AGNT_STS		   0x69
#define RTS490XA_HUB_TP6_SMBUS_AGNT_STS		   0x6A
#define RTS490XA_HUB_TP7_SMBUS_AGNT_STS		   0x6B
#define RTS490XA_HUB_ONCHIP_TD_AND_SMBUS_AGNT_CONF 0x6C

/* Transaction status checking mask */
#define RTS490XA_HUB_CONTROLLER_AGENT_STATUS_MASK   (0xF0 | BIT(0))
#define RTS490XA_HUB_CONTROLLER_AGENT_RET_CODE_MASK 0xF0
#define RTS490XA_HUB_CONTROLLER_AGENT_FINISH_FLAG   BIT(0)
#define RTS490XA_HUB_CONTROLLER_AGENT_ADDRESS_NACK  BIT(4)

#define RTS490XA_HUB_TARGET_BUF_STATUS_MASK GENMASK(3, 1)
#define RTS490XA_HUB_TARGET_BUF_0_RECEIVE   BIT(1)
#define RTS490XA_HUB_TARGET_BUF_1_RECEIVE   BIT(2)
#define RTS490XA_HUB_TARGET_BUF_OVRFL	    BIT(3)

/* Special Function Registers */
#define RTS490XA_HUB_LDO_AND_CPSEL_STS 0x79
#define CP_SDA1_LEVEL		       BIT(7)
#define CP_SCL1_LEVEL		       BIT(6)
#define CP_SEL_PIN_INPUT_CODE_MASK     GENMASK(5, 4)
#define CP_SEL_PIN_INPUT_CODE_GET(x)   (((x) & CP_SEL_PIN_INPUT_CODE_MASK) >> 4)
#define CP_SDA1_SCL1_PINS_CODE_MASK    GENMASK(7, 6)
#define CP_SDA1_SCL1_PINS_CODE_GET(x)  (((x) & CP_SDA1_SCL1_PINS_CODE_MASK) >> 6)
#define VCCIO1_PWR_GOOD		       BIT(3)
#define VCCIO0_PWR_GOOD		       BIT(2)
#define CP1_VCCIO_PWR_GOOD	       BIT(1)
#define CP0_VCCIO_PWR_GOOD	       BIT(0)

#define RTS490XA_HUB_BUS_RESET_SCL_TIMEOUT   0x7A
#define RTS490XA_HUB_ONCHIP_TD_PROTO_ERR_FLG 0x7B
#define RTS490XA_HUB_DEV_CMD		     0x7C
#define RTS490XA_HUB_ONCHIP_TD_STS	     0x7D
#define RTS490XA_HUB_ONCHIP_TD_ADDR_CONF     0x7E
#define RTS490XA_HUB_PAGE_PTR		     0x7F

/* LDO Disable/Enable DT settings */
#define RTS490XA_HUB_DT_LDO_DISABLED	0x00
#define RTS490XA_HUB_DT_LDO_ENABLED	0x01
#define RTS490XA_HUB_DT_LDO_NOT_DEFINED 0xFF

/* LDO Voltage DT settings */
#define RTS490XA_HUB_DT_LDO_VOLT_1_0V	     0x00
#define RTS490XA_HUB_DT_LDO_VOLT_1_1V	     0x01
#define RTS490XA_HUB_DT_LDO_VOLT_1_2V	     0x02
#define RTS490XA_HUB_DT_LDO_VOLT_1_8V	     0x03
#define RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED 0xFF

/* Paged Transaction Registers */
#define RTS490XA_HUB_CONTROLLER_BUFFER_PAGE	0x10
#define RTS490XA_HUB_CONTROLLER_AGENT_BUFF	0x80
#define RTS490XA_HUB_CONTROLLER_AGENT_BUFF_DATA 0x84
#define RTS490XA_HUB_TARGET_BUFF_LENGTH		0x80
#define RTS490XA_HUB_TARGET_BUFF_ADDRESS	0x81
#define RTS490XA_HUB_TARGET_BUFF_DATA		0x82

/* Pull-up DT settings */
#define RTS490XA_HUB_DT_PULLUP_DISABLED	   0x00
#define RTS490XA_HUB_DT_PULLUP_250R	   0x01
#define RTS490XA_HUB_DT_PULLUP_500R	   0x02
#define RTS490XA_HUB_DT_PULLUP_1K	   0x03
#define RTS490XA_HUB_DT_PULLUP_2K	   0x04
#define RTS490XA_HUB_DT_PULLUP_NOT_DEFINED 0xFF

/* TP DT setting */
#define RTS490XA_HUB_DT_TP_MODE_DISABLED    0x00
#define RTS490XA_HUB_DT_TP_MODE_I2C	    0x01
#define RTS490XA_HUB_DT_TP_MODE_SMBUS	    0x02
#define RTS490XA_HUB_DT_TP_MODE_GPIO	    0x03
#define RTS490XA_HUB_DT_TP_MODE_NOT_DEFINED 0xFF

/* TP pull-up status */
#define RTS490XA_HUB_DT_TP_PULLUP_DISABLED    0x00
#define RTS490XA_HUB_DT_TP_PULLUP_ENABLED     0x01
#define RTS490XA_HUB_DT_TP_PULLUP_NOT_DEFINED 0xFF

/* CP/TP IO strength */
#define RTS490XA_HUB_DT_IO_STRENGTH_20_OHM	0x00
#define RTS490XA_HUB_DT_IO_STRENGTH_30_OHM	0x01
#define RTS490XA_HUB_DT_IO_STRENGTH_40_OHM	0x02
#define RTS490XA_HUB_DT_IO_STRENGTH_50_OHM	0x03
#define RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED 0xFF
/* SMBus polling */
#define RTS490XA_HUB_POLLING_ROLL_PERIOD_MS	1

/* SMBus transaction types fields */
#define RTS490XA_HUB_SMBUS_400kHz BIT(2)

/* Hub buffer size */
#define RTS490XA_HUB_CONTROLLER_BUFFER_SIZE 88
#define RTS490XA_HUB_TARGET_BUFFER_SIZE	    80
#define RTS490XA_HUB_SMBUS_DESCRIPTOR_SIZE  4
#define RTS490XA_HUB_SMBUS_PAYLOAD_SIZE        \
	(RTS490XA_HUB_CONTROLLER_BUFFER_SIZE - \
	 RTS490XA_HUB_SMBUS_DESCRIPTOR_SIZE)
#define RTS490XA_HUB_SMBUS_TARGET_PAYLOAD_SIZE \
	(RTS490XA_HUB_TARGET_BUFFER_SIZE - 2)

/* Hub SMBus timeout time period in nanoseconds */
#define RTS490XA_HUB_SMBUS_400kHz_TIMEOUT 2e8

/* ID Extraction */
#define RTS490XA_HUB_ID_CP_SDA_SCL 0x00
#define RTS490XA_HUB_ID_CP_SEL	   0x01

struct tp_setting {
	u8 mode;
	u8 pullup_en;
	bool always_enable;
};

struct dt_settings {
	u8 cp0_ldo_en;
	u8 cp1_ldo_en;
	u8 cp0_ldo_volt;
	u8 cp1_ldo_volt;
	u8 tp0145_ldo_en;
	u8 tp2367_ldo_en;
	u8 tp0145_ldo_volt;
	u8 tp2367_ldo_volt;
	u8 tp0145_pullup;
	u8 tp2367_pullup;
	u8 cp0_io_strength;
	u8 cp1_io_strength;
	u8 tp0145_io_strength;
	u8 tp2367_io_strength;
	struct tp_setting tp[RTS490XA_HUB_TP_MAX_COUNT];
};
struct smbus_backend {
	struct i2c_client *client;
	const char *compatible;
	int addr;
	struct list_head list;
};

struct i2c_adapter_group {
	u8 tp_mask;
	u8 tp_port;
	u8 used;

	struct delayed_work delayed_work_polling;
	struct list_head backend_entry;
	u8 last_processed_buf;
};

struct logical_bus {
	struct i2c_adapter adapter;
	struct i2c_adapter_group smbus_port_adapter;
	struct device_node *of_node;
	struct rts490xa_hub *priv;
};

struct hub_gpio {
	struct gpio_chip chip;
	int tp[GPIO_MAX_BANK];
	int nums;
};

struct rts490xa_hub {
	struct i2c_client *client;
	struct regmap *regmap;
	struct dt_settings settings;
	struct delayed_work delayed_work;
	int hub_pin_sel_id;
	int hub_pin_cp1_id;
	int hub_dt_sel_id;
	int hub_dt_cp1_id;

	struct logical_bus logical_bus[RTS490XA_HUB_LOGICAL_BUS_MAX_COUNT];
	struct mutex page_mutex;

	/* Offset for reading HUB's register. */
	u8 reg_addr;
	struct dentry *debug_dir;
	struct hub_gpio gpio;
};

struct hub_setting {
	const char *const name;
	const u8 value;
};

static const struct hub_setting ldo_en_settings[] = {
	{ "disabled", RTS490XA_HUB_DT_LDO_DISABLED },
	{ "enabled", RTS490XA_HUB_DT_LDO_ENABLED },
};

static const struct hub_setting ldo_volt_settings[] = {
	{ "1.0V", RTS490XA_HUB_DT_LDO_VOLT_1_0V },
	{ "1.1V", RTS490XA_HUB_DT_LDO_VOLT_1_1V },
	{ "1.2V", RTS490XA_HUB_DT_LDO_VOLT_1_2V },
	{ "1.8V", RTS490XA_HUB_DT_LDO_VOLT_1_8V },
};

static const struct hub_setting pullup_settings[] = {
	{ "disabled", RTS490XA_HUB_DT_PULLUP_DISABLED },
	{ "250R", RTS490XA_HUB_DT_PULLUP_250R },
	{ "500R", RTS490XA_HUB_DT_PULLUP_500R },
	{ "1k", RTS490XA_HUB_DT_PULLUP_1K },
	{ "2k", RTS490XA_HUB_DT_PULLUP_2K },
};

static const struct hub_setting tp_mode_settings[] = {
	{ "disabled", RTS490XA_HUB_DT_TP_MODE_DISABLED },
	{ "i2c", RTS490XA_HUB_DT_TP_MODE_I2C },
	{ "smbus", RTS490XA_HUB_DT_TP_MODE_SMBUS },
	{ "gpio", RTS490XA_HUB_DT_TP_MODE_GPIO },
};

static const struct hub_setting tp_pullup_settings[] = {
	{ "disabled", RTS490XA_HUB_DT_TP_PULLUP_DISABLED },
	{ "enabled", RTS490XA_HUB_DT_TP_PULLUP_ENABLED },
};

static const struct hub_setting io_strength_settings[] = {
	{ "20Ohms", RTS490XA_HUB_DT_IO_STRENGTH_20_OHM },
	{ "30Ohms", RTS490XA_HUB_DT_IO_STRENGTH_30_OHM },
	{ "40Ohms", RTS490XA_HUB_DT_IO_STRENGTH_40_OHM },
	{ "50Ohms", RTS490XA_HUB_DT_IO_STRENGTH_50_OHM },
};

static u8 rts490xa_hub_ldo_dt_to_reg(u8 dt_value)
{
	switch (dt_value) {
	case RTS490XA_HUB_DT_LDO_VOLT_1_1V:
		return LDO_VOLTAGE_1_1V;
	case RTS490XA_HUB_DT_LDO_VOLT_1_2V:
		return LDO_VOLTAGE_1_2V;
	case RTS490XA_HUB_DT_LDO_VOLT_1_8V:
		return LDO_VOLTAGE_1_8V;
	default:
		return LDO_VOLTAGE_1_0V;
	}
}

static u8 rts490xa_hub_pullup_dt_to_reg(u8 dt_value)
{
	switch (dt_value) {
	case RTS490XA_HUB_DT_PULLUP_250R:
		return PULLUP_250R;
	case RTS490XA_HUB_DT_PULLUP_500R:
		return PULLUP_500R;
	case RTS490XA_HUB_DT_PULLUP_1K:
		return PULLUP_1K;
	default:
		return PULLUP_2K;
	}
}

static u8 rts490xa_hub_io_strength_dt_to_reg(u8 dt_value)
{
	switch (dt_value) {
	case RTS490XA_HUB_DT_IO_STRENGTH_50_OHM:
		return IO_STRENGTH_50_OHM;
	case RTS490XA_HUB_DT_IO_STRENGTH_40_OHM:
		return IO_STRENGTH_40_OHM;
	case RTS490XA_HUB_DT_IO_STRENGTH_30_OHM:
		return IO_STRENGTH_30_OHM;
	default:
		return IO_STRENGTH_20_OHM;
	}
}

static void rts490xa_hub_of_get_setting(struct device *dev,
					const struct device_node *node,
					const char *setting_name,
					const struct hub_setting settings[],
					const u8 settings_count,
					u8 *setting_value)
{
	const char *sval;
	int ret;
	int i;

	ret = of_property_read_string(node, setting_name, &sval);
	if (ret) {
		/* Lack of property is not considered as a problem. */
		if (ret != -EINVAL)
			dev_warn(
				dev,
				"No setting or invalid setting for %s, err=%i\n",
				setting_name, ret);
		return;
	}

	for (i = 0; i < settings_count; ++i) {
		const struct hub_setting *const setting = &settings[i];

		if (!strcmp(setting->name, sval)) {
			*setting_value = setting->value;
			return;
		}
	}
	dev_warn(dev, "Unknown setting for %s: '%s'\n", setting_name, sval);
}

static void rts490xa_hub_tp_of_get_setting(struct device *dev,
					   const struct device_node *node,
					   struct tp_setting tp_setting[])
{
	struct device_node *tp_node;
	u32 id;

	for_each_available_child_of_node(node, tp_node) {
		if (!tp_node->name || of_node_cmp(tp_node->name, "target-port"))
			continue;

		if (!tp_node->full_name ||
		    (sscanf(tp_node->full_name, "target-port@%u", &id) != 1)) {
			dev_warn(dev,
				 "Invalid target port node found in DT: %s\n",
				 tp_node->full_name);
			continue;
		}

		if (id >= RTS490XA_HUB_TP_MAX_COUNT) {
			dev_warn(dev,
				 "Invalid target port index found in DT: %i\n",
				 id);
			continue;
		}
		rts490xa_hub_of_get_setting(dev, tp_node, "mode",
					    tp_mode_settings,
					    ARRAY_SIZE(tp_mode_settings),
					    &tp_setting[id].mode);
		rts490xa_hub_of_get_setting(dev, tp_node, "pullup",
					    tp_pullup_settings,
					    ARRAY_SIZE(tp_pullup_settings),
					    &tp_setting[id].pullup_en);
		tp_setting[id].always_enable =
			of_property_read_bool(tp_node, "always-enable");
	}
}

static void rts490xa_hub_of_get_conf_static(struct device *dev,
					    const struct device_node *node)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);

	rts490xa_hub_of_get_setting(dev, node, "cp0-ldo-en", ldo_en_settings,
				    ARRAY_SIZE(ldo_en_settings),
				    &priv->settings.cp0_ldo_en);
	rts490xa_hub_of_get_setting(dev, node, "cp1-ldo-en", ldo_en_settings,
				    ARRAY_SIZE(ldo_en_settings),
				    &priv->settings.cp1_ldo_en);
	rts490xa_hub_of_get_setting(dev, node, "cp0-ldo-volt",
				    ldo_volt_settings,
				    ARRAY_SIZE(ldo_volt_settings),
				    &priv->settings.cp0_ldo_volt);
	rts490xa_hub_of_get_setting(dev, node, "cp1-ldo-volt",
				    ldo_volt_settings,
				    ARRAY_SIZE(ldo_volt_settings),
				    &priv->settings.cp1_ldo_volt);
	rts490xa_hub_of_get_setting(dev, node, "tp0145-ldo-en", ldo_en_settings,
				    ARRAY_SIZE(ldo_en_settings),
				    &priv->settings.tp0145_ldo_en);
	rts490xa_hub_of_get_setting(dev, node, "tp2367-ldo-en", ldo_en_settings,
				    ARRAY_SIZE(ldo_en_settings),
				    &priv->settings.tp2367_ldo_en);
	rts490xa_hub_of_get_setting(dev, node, "tp0145-ldo-volt",
				    ldo_volt_settings,
				    ARRAY_SIZE(ldo_volt_settings),
				    &priv->settings.tp0145_ldo_volt);
	rts490xa_hub_of_get_setting(dev, node, "tp2367-ldo-volt",
				    ldo_volt_settings,
				    ARRAY_SIZE(ldo_volt_settings),
				    &priv->settings.tp2367_ldo_volt);
	rts490xa_hub_of_get_setting(dev, node, "tp0145-pullup", pullup_settings,
				    ARRAY_SIZE(pullup_settings),
				    &priv->settings.tp0145_pullup);
	rts490xa_hub_of_get_setting(dev, node, "tp2367-pullup", pullup_settings,
				    ARRAY_SIZE(pullup_settings),
				    &priv->settings.tp2367_pullup);
	rts490xa_hub_of_get_setting(dev, node, "cp0-io-strength",
				    io_strength_settings,
				    ARRAY_SIZE(io_strength_settings),
				    &priv->settings.cp0_io_strength);
	rts490xa_hub_of_get_setting(dev, node, "cp1-io-strength",
				    io_strength_settings,
				    ARRAY_SIZE(io_strength_settings),
				    &priv->settings.cp1_io_strength);
	rts490xa_hub_of_get_setting(dev, node, "tp0145-io-strength",
				    io_strength_settings,
				    ARRAY_SIZE(io_strength_settings),
				    &priv->settings.tp0145_io_strength);
	rts490xa_hub_of_get_setting(dev, node, "tp2367-io-strength",
				    io_strength_settings,
				    ARRAY_SIZE(io_strength_settings),
				    &priv->settings.tp2367_io_strength);

	rts490xa_hub_tp_of_get_setting(dev, node, priv->settings.tp);
}

static void rts490xa_hub_of_default_configuration(struct device *dev)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);
	int id;

	priv->settings.cp0_ldo_en = RTS490XA_HUB_DT_LDO_NOT_DEFINED;
	priv->settings.cp1_ldo_en = RTS490XA_HUB_DT_LDO_NOT_DEFINED;
	priv->settings.cp0_ldo_volt = RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED;
	priv->settings.cp1_ldo_volt = RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED;
	priv->settings.tp0145_ldo_en = RTS490XA_HUB_DT_LDO_NOT_DEFINED;
	priv->settings.tp2367_ldo_en = RTS490XA_HUB_DT_LDO_NOT_DEFINED;
	priv->settings.tp0145_ldo_volt = RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED;
	priv->settings.tp2367_ldo_volt = RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED;
	priv->settings.tp0145_pullup = RTS490XA_HUB_DT_PULLUP_NOT_DEFINED;
	priv->settings.tp2367_pullup = RTS490XA_HUB_DT_PULLUP_NOT_DEFINED;
	priv->settings.cp0_io_strength =
		RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED;
	priv->settings.cp1_io_strength =
		RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED;
	priv->settings.tp0145_io_strength =
		RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED;
	priv->settings.tp2367_io_strength =
		RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED;

	for (id = 0; id < RTS490XA_HUB_TP_MAX_COUNT; ++id) {
		priv->settings.tp[id].mode =
			RTS490XA_HUB_DT_TP_MODE_NOT_DEFINED;
		priv->settings.tp[id].pullup_en =
			RTS490XA_HUB_DT_TP_PULLUP_NOT_DEFINED;
	}
}

static int rts490xa_hub_hw_configure_pullup(struct device *dev)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);
	u8 mask = 0, value = 0;

	if (priv->settings.tp0145_pullup !=
	    RTS490XA_HUB_DT_PULLUP_NOT_DEFINED) {
		mask |= TP0145_PULLUP_CONF_MASK;
		value |= TP0145_PULLUP_CONF(rts490xa_hub_pullup_dt_to_reg(
			priv->settings.tp0145_pullup));
	}

	if (priv->settings.tp2367_pullup !=
	    RTS490XA_HUB_DT_PULLUP_NOT_DEFINED) {
		mask |= TP2367_PULLUP_CONF_MASK;
		value |= TP2367_PULLUP_CONF(rts490xa_hub_pullup_dt_to_reg(
			priv->settings.tp2367_pullup));
	}

	return regmap_update_bits(
		priv->regmap, RTS490XA_HUB_LDO_AND_PULLUP_CONF, mask, value);
}

static int rts490xa_hub_hw_configure_ldo(struct device *dev)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);
	u8 ldo_config_mask = 0, ldo_config_val = 0;
	u8 ldo_disable_mask = 0, ldo_en_val = 0;
	u32 reg_val;
	int ret;
	u8 val;

	/* Enable or Disable LDO's. If there is no DT entry - disable LDO for safety reasons */
	if (priv->settings.cp0_ldo_en == RTS490XA_HUB_DT_LDO_ENABLED)
		ldo_en_val |= CP0_LDO_EN;
	if (priv->settings.cp1_ldo_en == RTS490XA_HUB_DT_LDO_ENABLED)
		ldo_en_val |= CP1_LDO_EN;
	if (priv->settings.tp0145_ldo_en == RTS490XA_HUB_DT_LDO_ENABLED)
		ldo_en_val |= TP0145_LDO_EN;
	if (priv->settings.tp2367_ldo_en == RTS490XA_HUB_DT_LDO_ENABLED)
		ldo_en_val |= TP2367_LDO_EN;

	/* Get current LDOs configuration */
	ret = regmap_read(priv->regmap, RTS490XA_HUB_LDO_CONF, &reg_val);
	if (ret)
		return ret;

	/* LDOs Voltage level (Skip if not defined in the DT)
	 * Set the mask only if there is a change from current value
	 */
	if (priv->settings.cp0_ldo_volt !=
	    RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED) {
		val = CP0_LDO_VOLTAGE(rts490xa_hub_ldo_dt_to_reg(
			priv->settings.cp0_ldo_volt));
		if ((reg_val & CP0_LDO_VOLTAGE_MASK) != val) {
			ldo_config_mask |= CP0_LDO_VOLTAGE_MASK;
			ldo_disable_mask |= CP0_LDO_EN;
			ldo_config_val |= val;
		}
	}
	if (priv->settings.cp1_ldo_volt !=
	    RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED) {
		val = CP1_LDO_VOLTAGE(rts490xa_hub_ldo_dt_to_reg(
			priv->settings.cp1_ldo_volt));
		if ((reg_val & CP1_LDO_VOLTAGE_MASK) != val) {
			ldo_config_mask |= CP1_LDO_VOLTAGE_MASK;
			ldo_disable_mask |= CP1_LDO_EN;
			ldo_config_val |= val;
		}
	}
	if (priv->settings.tp0145_ldo_volt !=
	    RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED) {
		val = TP0145_LDO_VOLTAGE(rts490xa_hub_ldo_dt_to_reg(
			priv->settings.tp0145_ldo_volt));
		if ((reg_val & TP0145_LDO_VOLTAGE_MASK) != val) {
			ldo_config_mask |= TP0145_LDO_VOLTAGE_MASK;
			ldo_disable_mask |= TP0145_LDO_EN;
			ldo_config_val |= val;
		}
	}
	if (priv->settings.tp2367_ldo_volt !=
	    RTS490XA_HUB_DT_LDO_VOLT_NOT_DEFINED) {
		val = TP2367_LDO_VOLTAGE(rts490xa_hub_ldo_dt_to_reg(
			priv->settings.tp2367_ldo_volt));
		if ((reg_val & TP2367_LDO_VOLTAGE_MASK) != val) {
			ldo_config_mask |= TP2367_LDO_VOLTAGE_MASK;
			ldo_disable_mask |= TP2367_LDO_EN;
			ldo_config_val |= val;
		}
	}

	/*
	 * Update LDO voltage configuration only if value is changed from already existing register
	 * value. It is a good practice to disable the LDO's before making any voltage changes.
	 * Presence of config mask indicates voltage change to be applied.
	 */
	if (ldo_config_mask) {
		/* Disable LDO's before making voltage changes */
		ret = regmap_update_bits(priv->regmap,
					 RTS490XA_HUB_LDO_AND_PULLUP_CONF,
					 ldo_disable_mask, 0);
		if (ret)
			return ret;

		/* Update the LDOs configuration */
		ret = regmap_update_bits(priv->regmap, RTS490XA_HUB_LDO_CONF,
					 ldo_config_mask, ldo_config_val);
		if (ret)
			return ret;
	}

	/* Update the LDOs Enable/disable register. This will enable only LDOs enabled in DT */
	return regmap_update_bits(priv->regmap,
				  RTS490XA_HUB_LDO_AND_PULLUP_CONF,
				  LDO_ENABLE_DISABLE_MASK, ldo_en_val);
}

static int rts490xa_hub_hw_configure_io_strength(struct device *dev)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);
	u8 mask_all = 0, val_all = 0;
	u32 reg_val;
	u8 val;
	struct dt_settings tmp;
	int ret;

	/* Get IO strength configuration to figure out what needs to be changed */
	ret = regmap_read(priv->regmap, RTS490XA_HUB_IO_STRENGTH, &reg_val);
	if (ret)
		return ret;

	tmp = priv->settings;
	if (tmp.cp0_io_strength != RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED) {
		val = CP0_IO_STRENGTH(rts490xa_hub_io_strength_dt_to_reg(
			tmp.cp0_io_strength));
		mask_all |= CP0_IO_STRENGTH_MASK;
		val_all |= val;
	}
	if (tmp.cp1_io_strength != RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED) {
		val = CP1_IO_STRENGTH(rts490xa_hub_io_strength_dt_to_reg(
			tmp.cp1_io_strength));
		mask_all |= CP1_IO_STRENGTH_MASK;
		val_all |= val;
	}
	if (tmp.tp0145_io_strength != RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED) {
		val = TP0145_IO_STRENGTH(rts490xa_hub_io_strength_dt_to_reg(
			tmp.tp0145_io_strength));
		mask_all |= TP0145_IO_STRENGTH_MASK;
		val_all |= val;
	}
	if (tmp.tp2367_io_strength != RTS490XA_HUB_DT_IO_STRENGTH_NOT_DEFINED) {
		val = TP2367_IO_STRENGTH(rts490xa_hub_io_strength_dt_to_reg(
			tmp.tp2367_io_strength));
		mask_all |= TP2367_IO_STRENGTH_MASK;
		val_all |= val;
	}

	/* Set IO strength if required */
	return regmap_update_bits(priv->regmap, RTS490XA_HUB_IO_STRENGTH,
				  mask_all, val_all);
}

static int disable_target_port_vccio_pwrgd_ctrl(struct device *dev)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);
	int ret;

	ret = regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR,
			   RTS490XA_DPAD_POWER_BUFFER_PAGE);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, RTS490XA_DPAD_POWER_DISABLE, 1);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR, 0x00);
	if (ret)
		return ret;

	ret = regmap_update_bits(priv->regmap, RTS490XA_HUB_DEV_CONF,
				 TARGET_PORT_VCCIO_PWRGD_CTRL_MASK, 0);
	return ret;
}

static int rts490xa_hub_hw_configure_tp(struct device *dev)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);
	u8 pullup_mask = 0, pullup_val = 0;
	u8 smbus_mask = 0, smbus_val = 0;
	u8 gpio_mask = 0, gpio_val = 0;
	u8 i2c_mask = 0, i2c_val = 0;
	int ret;
	int i, index;

	/* TBD: Read type of HUB from register RTS490XA_HUB_DEV_INFO_0 to learn target ports count. */
	for (i = 0; i < RTS490XA_HUB_TP_MAX_COUNT; ++i) {
		if (priv->settings.tp[i].mode !=
		    RTS490XA_HUB_DT_TP_MODE_NOT_DEFINED) {
			i2c_mask |= TPn_NET_CON(i);
			smbus_mask |= TPn_SMBUS_MODE_EN(i);
			gpio_mask |= TPn_GPIO_MODE_EN(i);

			if (priv->settings.tp[i].mode ==
			    RTS490XA_HUB_DT_TP_MODE_I2C) {
				i2c_val |= TPn_NET_CON(i);
			} else if (priv->settings.tp[i].mode ==
				   RTS490XA_HUB_DT_TP_MODE_SMBUS) {
				smbus_val |= TPn_SMBUS_MODE_EN(i);
			} else if (priv->settings.tp[i].mode ==
				   RTS490XA_HUB_DT_TP_MODE_GPIO) {
				gpio_val |= TPn_GPIO_MODE_EN(i);
				priv->gpio.nums += GPIO_BANK_SZ;
				index = priv->gpio.nums / GPIO_BANK_SZ - 1;
				priv->gpio.tp[index] = i;
			}
		}
		if (priv->settings.tp[i].pullup_en !=
		    RTS490XA_HUB_DT_TP_PULLUP_NOT_DEFINED) {
			pullup_mask |= TPn_PULLUP_EN(i);
			if (priv->settings.tp[i].pullup_en ==
			    RTS490XA_HUB_DT_TP_PULLUP_ENABLED)
				pullup_val |= TPn_PULLUP_EN(i);
		}
	}

	ret = regmap_update_bits(priv->regmap, RTS490XA_HUB_TP_IO_MODE_CONF,
				 smbus_mask, smbus_val);
	if (ret)
		return ret;

	if (smbus_val) {
		ret = disable_target_port_vccio_pwrgd_ctrl(dev);
		if (ret)
			return ret;
	}

	ret = regmap_update_bits(priv->regmap, RTS490XA_HUB_TP_PULLUP_EN,
				 pullup_mask, pullup_val);
	if (ret)
		return ret;

	ret = regmap_update_bits(priv->regmap, RTS490XA_HUB_TP_SMBUS_AGNT_EN,
				 smbus_mask, smbus_val);
	if (ret)
		return ret;

	ret = regmap_update_bits(priv->regmap, RTS490XA_HUB_TP_GPIO_MODE_EN,
				 gpio_mask, gpio_val);
	if (ret)
		return ret;

	/* Request for HUB Network connection in case any TP is configured in I2C mode */
	if (i2c_val) {
		ret = regmap_write(priv->regmap, RTS490XA_HUB_CP_MUX_SET,
				   CONTROLLER_PORT_MUX_REQ);
		if (ret)
			return ret;
		/* TODO: verify if connection is done */
	}

	/* Enable TP here in case TP was configured */
	ret = regmap_update_bits(priv->regmap, RTS490XA_HUB_TP_ENABLE,
				 i2c_mask | smbus_mask | gpio_mask,
				 i2c_val | smbus_val | gpio_val);
	if (ret)
		return ret;

	return regmap_update_bits(priv->regmap, RTS490XA_HUB_TP_NET_CON_CONF,
				  i2c_mask, i2c_val);
}

static int rts490xa_hub_configure_hw(struct device *dev)
{
	int ret;

	ret = rts490xa_hub_hw_configure_ldo(dev);
	if (ret)
		return ret;

	ret = rts490xa_hub_hw_configure_io_strength(dev);
	if (ret)
		return ret;

	ret = rts490xa_hub_hw_configure_pullup(dev);
	if (ret)
		return ret;

	return rts490xa_hub_hw_configure_tp(dev);
}

static int rts490xa_hub_read_id(struct device *dev)
{
	struct rts490xa_hub *priv = dev_get_drvdata(dev);
	u32 reg_val;
	int ret;

	ret = regmap_read(priv->regmap, RTS490XA_HUB_LDO_AND_CPSEL_STS,
			  &reg_val);
	if (ret) {
		dev_err(dev, "Failed to read status register\n");
		return -1;
	}

	priv->hub_pin_sel_id = CP_SEL_PIN_INPUT_CODE_GET(reg_val);
	priv->hub_pin_cp1_id = CP_SDA1_SCL1_PINS_CODE_GET(reg_val);
	return 0;
}

static struct device_node *
rts490xa_hub_get_dt_hub_node(struct device_node *node,
			     struct rts490xa_hub *priv)
{
	struct device_node *hub_node_no_id = NULL;
	struct device_node *hub_node;
	u32 hub_id;
	u32 id_mask;
	u32 dt_id;
	u32 pin_id;
	int found_id = 0;

	for_each_available_child_of_node(node, hub_node) {
		id_mask = 0;
		if (strstr(hub_node->name, "hub")) {
			if (!of_property_read_u32(hub_node, "id", &hub_id)) {
				id_mask |= 0x0f;
				priv->hub_dt_sel_id = hub_id;
			}

			if (!of_property_read_u32(hub_node, "id-cp1",
						  &hub_id)) {
				id_mask |= 0xf0;
				priv->hub_dt_cp1_id = hub_id;
			}

			dt_id = (u32)priv->hub_dt_cp1_id << 4 |
				(u32)priv->hub_dt_sel_id;
			pin_id = (u32)priv->hub_pin_cp1_id << 4 |
				 (u32)priv->hub_pin_sel_id;

			if (id_mask != 0 &&
			    (dt_id & id_mask) == (pin_id & id_mask))
				found_id = 1;

			if (!found_id) {
				/*
				 * Just keep reference to first HUB node with no ID in case no ID
				 * matching
				 */
				if (!hub_node_no_id &&
				    priv->hub_dt_sel_id == -1 &&
				    priv->hub_dt_cp1_id == -1)
					hub_node_no_id = hub_node;
			} else {
				return hub_node;
			}
		}
	}

	return hub_node_no_id;
}

static int fops_access_reg_get(void *ctx, u64 *val)
{
	struct rts490xa_hub *priv = ctx;
	u32 reg_val;
	int ret;

	ret = regmap_read(priv->regmap, priv->reg_addr, &reg_val);
	if (ret)
		return ret;

	*val = reg_val & 0xFF;
	return 0;
}

static int fops_access_reg_set(void *ctx, u64 val)
{
	struct rts490xa_hub *priv = ctx;

	return regmap_write(priv->regmap, priv->reg_addr, val & 0xFF);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_access_reg, fops_access_reg_get,
			 fops_access_reg_set, "0x%llX\n");

static int rts490xa_hub_debugfs_init(struct rts490xa_hub *priv,
				     const char *hub_id)
{
	struct dentry *entry, *dt_conf_dir, *reg_dir;
	struct dt_settings *settings = NULL;
	int i;

	entry = debugfs_create_dir(hub_id, NULL);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	priv->debug_dir = entry;

	entry = debugfs_create_dir("dt-conf", priv->debug_dir);
	if (IS_ERR(entry))
		goto err_remove;

	dt_conf_dir = entry;

	settings = &priv->settings;
	debugfs_create_u8("cp0-ldo-en", 0400, dt_conf_dir,
			  &settings->cp0_ldo_en);
	debugfs_create_u8("cp1-ldo-en", 0400, dt_conf_dir,
			  &settings->cp1_ldo_en);
	debugfs_create_u8("cp0-ldo-volt", 0400, dt_conf_dir,
			  &settings->cp0_ldo_volt);
	debugfs_create_u8("cp1-ldo-volt", 0400, dt_conf_dir,
			  &settings->cp1_ldo_volt);
	debugfs_create_u8("tp0145-ldo-en", 0400, dt_conf_dir,
			  &settings->tp0145_ldo_en);
	debugfs_create_u8("tp2367-ldo-en", 0400, dt_conf_dir,
			  &settings->tp2367_ldo_en);
	debugfs_create_u8("tp0145-ldo-volt", 0400, dt_conf_dir,
			  &settings->tp0145_ldo_volt);
	debugfs_create_u8("tp2367-ldo-volt", 0400, dt_conf_dir,
			  &settings->tp2367_ldo_volt);
	debugfs_create_u8("tp0145-pullup", 0400, dt_conf_dir,
			  &settings->tp0145_pullup);
	debugfs_create_u8("tp2367-pullup", 0400, dt_conf_dir,
			  &settings->tp2367_pullup);

	for (i = 0; i < RTS490XA_HUB_TP_MAX_COUNT; ++i) {
		char file_name[32];

		sprintf(file_name, "tp%i.mode", i);
		debugfs_create_u8(file_name, 0400, dt_conf_dir,
				  &settings->tp[i].mode);
		sprintf(file_name, "tp%i.pullup_en", i);
		debugfs_create_u8(file_name, 0400, dt_conf_dir,
				  &settings->tp[i].pullup_en);
	}

	entry = debugfs_create_dir("reg", priv->debug_dir);
	if (IS_ERR(entry))
		goto err_remove;

	reg_dir = entry;

	entry = debugfs_create_file_unsafe("access", 0600, reg_dir, priv,
					   &fops_access_reg);
	if (IS_ERR(entry))
		goto err_remove;

	debugfs_create_u8("offset", 0600, reg_dir, &priv->reg_addr);

	return 0;

err_remove:
	debugfs_remove_recursive(priv->debug_dir);
	return PTR_ERR(entry);
}

static int rts490xa_hub_read_transaction_status(struct rts490xa_hub *priv,
						u8 target_port_status,
						u8 *status)
{
	unsigned long time_to_timeout = 0;
	unsigned int status_read;
	ktime_t start, end;
	int ret;

	start = ktime_get_real();

	msleep(10);
	while (time_to_timeout < (long)RTS490XA_HUB_SMBUS_400kHz_TIMEOUT) {
		ret = regmap_read(priv->regmap, target_port_status,
				  &status_read);
		if (ret)
			return ret;

		*status = (u8)status_read &
			  RTS490XA_HUB_CONTROLLER_AGENT_STATUS_MASK;

		if (*status & RTS490XA_HUB_CONTROLLER_AGENT_FINISH_FLAG) {
			if ((*status &
			     RTS490XA_HUB_CONTROLLER_AGENT_RET_CODE_MASK) &&
			    !(*status &
			      RTS490XA_HUB_CONTROLLER_AGENT_ADDRESS_NACK)) {
				dev_err(&priv->client->dev,
					"Invalid transfer status returned: 0x%02x\n",
					*status);
				return -EAGAIN;
			}
			return 0;
		}

		end = ktime_get_real();
		time_to_timeout = end - start;
	}
	dev_err(&priv->client->dev, "Status read timeout reached\n");
	return 0;
}

/*
 * rts490xa_hub_smbus_msg() - This starts a smbus write transaction by writing a descriptor
 * and a message to the hub registers. Controller buffer page is determined by multiplying the
 * target port index by four and adding the base page number to it.
 * @priv: a pointer to the rts490xa hub main structure
 * @ssport: a number of the port where the transaction will happen
 * @xfers: i2c_msg struct received from the master_xfers callback
 * @nxfers_i: the number of the current message
 * @rw: number informing if the message is of read or write type (0 for write, 1 for read)
 * @return_status: number passed by reference where the return status code is saved
 *
 * Return: on success function returns zero. Otherwise the regmap read or write error code
 * is returned
 */
static int rts490xa_hub_smbus_msg(struct rts490xa_hub *priv,
				  struct i2c_msg *xfers, u8 target_port,
				  u8 nxfers_i, u8 rw, u8 *return_status)
{
	u8 transaction_type = RTS490XA_HUB_SMBUS_400kHz;
	u8 controller_buffer_page =
		RTS490XA_HUB_CONTROLLER_BUFFER_PAGE + 4 * target_port;
	int write_length = xfers[nxfers_i].len;
	int read_length = xfers[nxfers_i].len;
	u8 target_port_status = RTS490XA_HUB_TP0_SMBUS_AGNT_STS + target_port;
	u8 addr = xfers[nxfers_i].addr;
	u8 target_port_code = BIT(target_port);
	u8 rw_address = 2 * addr;
	u8 desc[RTS490XA_HUB_SMBUS_DESCRIPTOR_SIZE] = { 0 };
	u8 status;
	int ret = 0;

	if (rw)
		rw_address |= BIT(0);
	else
		read_length = 0;

	desc[0] = rw_address;
	desc[1] = transaction_type;
	desc[2] = write_length;
	desc[3] = read_length;

	ret = regmap_write(priv->regmap, target_port_status,
			   RTS490XA_HUB_CONTROLLER_AGENT_FINISH_FLAG);
	if (ret)
		return ret;

	mutex_lock(&priv->page_mutex);
	ret = regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR,
			   controller_buffer_page);
	if (ret)
		goto unlock;

	ret = regmap_bulk_write(priv->regmap,
				RTS490XA_HUB_CONTROLLER_AGENT_BUFF, desc,
				RTS490XA_HUB_SMBUS_DESCRIPTOR_SIZE);
	if (ret)
		goto unlock;

	if (!rw && write_length) {
		ret = regmap_bulk_write(priv->regmap,
					RTS490XA_HUB_CONTROLLER_AGENT_BUFF_DATA,
					xfers[nxfers_i].buf,
					xfers[nxfers_i].len);
		if (ret)
			goto unlock;
	}

	ret = regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR, 0x00);
	mutex_unlock(&priv->page_mutex);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, RTS490XA_HUB_TP_SMBUS_AGNT_TRANS_START,
			   target_port_code);
	if (ret)
		return ret;

	ret = rts490xa_hub_read_transaction_status(priv, target_port_status,
						   &status);
	if (ret)
		return ret;

	*return_status = status;

	if (rw) {
		mutex_lock(&priv->page_mutex);
		ret = regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR,
				   controller_buffer_page);
		if (ret)
			goto unlock;

		ret = regmap_bulk_read(priv->regmap,
				       RTS490XA_HUB_CONTROLLER_AGENT_BUFF_DATA,
				       xfers[nxfers_i].buf,
				       xfers[nxfers_i].len);
		if (ret)
			goto unlock;

		ret = regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR, 0x00);
		mutex_unlock(&priv->page_mutex);
	}

	return ret;
unlock:
	regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR, 0x00);
	mutex_unlock(&priv->page_mutex);
	return ret;
}

/**
 * i2c_controller_smbus_port_adapter_xfer() - rts490xa hub smbus transfer logic
 * @adap: i2c_adapter corresponding with single port in the rts490xa hub
 * @xfers: all messages descriptors and data
 * @nxfers: amount of single messages in a transfer
 *
 * Return: function returns the sum of correctly sent messages (only those with hub return
 * status 0x01)
 */
static int i2c_controller_smbus_port_adapter_xfer(struct i2c_adapter *adap,
						  struct i2c_msg *xfers,
						  int nxfers)
{
	struct logical_bus *bus =
		container_of(adap, struct logical_bus, adapter);
	struct rts490xa_hub *priv = bus->priv;
	int ret_sum = 0;
	int ret;
	u8 return_status;
	u8 nxfers_i;
	u8 rw;

	for (nxfers_i = 0; nxfers_i < nxfers; nxfers_i++) {
		if (xfers[nxfers_i].len > RTS490XA_HUB_SMBUS_PAYLOAD_SIZE) {
			dev_err(&adap->dev,
				"Message nr. %d not sent - length over %d bytes.\n",
				nxfers_i, RTS490XA_HUB_SMBUS_PAYLOAD_SIZE);
			continue;
		}

		rw = xfers[nxfers_i].flags % 2;

		ret = rts490xa_hub_smbus_msg(priv, xfers,
					     bus->smbus_port_adapter.tp_port,
					     nxfers_i, rw, &return_status);
		if (ret)
			return ret;
		if (return_status == RTS490XA_HUB_CONTROLLER_AGENT_FINISH_FLAG)
			ret_sum++;
	}
	return ret_sum;
}

static u32 i2c_controller_smbus_funcs(struct i2c_adapter *adapter)
{
	return (I2C_FUNC_SMBUS_EMUL | I2C_FUNC_I2C) & ~I2C_FUNC_SMBUS_QUICK;
}

static int reg_i2c_target(struct i2c_client *client)
{
	return 0;
}

static int unreg_i2c_target(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_algorithm i2c_controller_smbus_algo = {
	.master_xfer = i2c_controller_smbus_port_adapter_xfer,
	.functionality = i2c_controller_smbus_funcs,
	.reg_slave = reg_i2c_target,
	.unreg_slave = unreg_i2c_target,
};

static void rts490xa_hub_delayed_work(struct work_struct *work)
{
	struct rts490xa_hub *priv =
		container_of(work, typeof(*priv), delayed_work.work);
	struct device *dev = &priv->client->dev;
	struct i2c_board_info host_notify_board_info = { 0 };
	struct smbus_backend *backend = NULL;
	struct logical_bus *bus;
	int ret;
	int i;

	for (i = 0; i < RTS490XA_HUB_TP_MAX_COUNT; i++) {
		bus = &priv->logical_bus[i];
		if (!bus->smbus_port_adapter.used)
			continue;

		list_for_each_entry(
			backend, &bus->smbus_port_adapter.backend_entry, list) {
			host_notify_board_info.addr = backend->addr;
			host_notify_board_info.flags = I2C_CLIENT_SLAVE;
			snprintf(host_notify_board_info.type, I2C_NAME_SIZE,
				 backend->compatible);

			backend->client = i2c_new_client_device(
				&bus->adapter, &host_notify_board_info);
			if (IS_ERR(backend->client)) {
				dev_warn(dev,
					 "Error while registering backend\n");
				return;
			}
		}

		schedule_delayed_work(
			&bus->smbus_port_adapter.delayed_work_polling,
			msecs_to_jiffies(RTS490XA_HUB_POLLING_ROLL_PERIOD_MS));
	}
}

static int rts490xa_hub_register_smbus_adapter(struct rts490xa_hub *priv, int i)
{
	struct device *dev = &priv->client->dev;
	int ret;

	dev->of_node = priv->logical_bus[i].of_node;
	ret = i2c_add_adapter(&priv->logical_bus[i].adapter);
	if (ret) {
		dev_warn(dev, "Failed to register i2c adapter\n");
		return ret;
	}

	return 0;
}

/* return true when backend is empty */
static bool backend_is_empty(struct i2c_adapter_group *g_adap,
			     struct i2c_adapter *adap)
{
	struct i2c_client *client, *next;

	if (!list_empty(&g_adap->backend_entry))
		return false;

	list_for_each_entry_safe(client, next, &adap->userspace_clients,
				 detected) {
		if (!strcmp(client->name, "slave-mqueue") ||
		    !strcmp(client->name, "mctp-i2c-controller"))
			return false;
	}

	return true;
}

static int send_to_backend(struct i2c_client *client, u8 address, u8 *val,
			   u8 len)
{
	int i, ret;
	u8 tmp;

	ret = i2c_slave_event(client, I2C_SLAVE_WRITE_REQUESTED, &address);
	if (ret)
		return ret;

	for (i = 0; i < len; i++) {
		ret = i2c_slave_event(client, I2C_SLAVE_WRITE_RECEIVED,
				      &val[i]);
		if (ret)
			return ret;
	}

	return i2c_slave_event(client, I2C_SLAVE_STOP, &tmp);
}

static int send_smbus_target_data_to_backend(struct rts490xa_hub *priv,
					     struct i2c_adapter_group *g_adap,
					     u8 address, u8 *local_buffer,
					     u8 len)
{
	struct smbus_backend *backend;
	struct i2c_client *client, *next;
	struct i2c_adapter *adap;
	bool found_backend = false;
	int ret;

	list_for_each_entry(backend, &g_adap->backend_entry, list) {
		if (address >> 1 == backend->addr) {
			ret = send_to_backend(backend->client, address,
					      local_buffer, len);
			if (ret) {
				dev_err(&priv->client->dev,
					"Failed to send to backend: %d\n", ret);
				return ret;
			}
			found_backend = true;
			break;
		}
	}

	if (!found_backend) {
		adap = &priv->logical_bus[g_adap->tp_port].adapter;
		list_for_each_entry_safe(client, next, &adap->userspace_clients,
					 detected) {
			if (client->addr == address >> 1 &&
			    (!strcmp(client->name, "slave-mqueue") ||
			     !strcmp(client->name, "mctp-i2c-controller"))) {
				ret = send_to_backend(client, address,
						      local_buffer, len);
				if (ret) {
					dev_err(&priv->client->dev,
						"Failed to send to userspace client: %d\n",
						ret);
					return ret;
				}
				break;
			}
		}
	}

	return 0;
}

static int read_smbus_target_buffer_page(struct rts490xa_hub *priv,
					 u8 target_buffer_page, u8 *address,
					 u8 *local_buffer, u8 *len)
{
	struct device *dev = &priv->client->dev;
	u32 status;
	int ret;

	mutex_lock(&priv->page_mutex);
	regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR, target_buffer_page);

	ret = regmap_read(priv->regmap, RTS490XA_HUB_TARGET_BUFF_LENGTH,
			  &status);
	if (ret)
		goto error;

	*len = status - 1;
	if (!*len)
		goto error;

	if (*len > RTS490XA_HUB_SMBUS_TARGET_PAYLOAD_SIZE) {
		dev_err(dev, "Received message too big for hub buffer\n");
		ret = -EMSGSIZE;
		goto error;
	}

	ret = regmap_read(priv->regmap, RTS490XA_HUB_TARGET_BUFF_ADDRESS,
			  &status);
	if (ret)
		goto error;

	*address = status;

	ret = regmap_bulk_read(priv->regmap, RTS490XA_HUB_TARGET_BUFF_DATA,
			       local_buffer, *len);

error:
	regmap_write(priv->regmap, RTS490XA_HUB_PAGE_PTR, 0x00);
	mutex_unlock(&priv->page_mutex);
	return ret;
}

/**
 * rts490xa_hub_delayed_work_polling() - This delayed work is a polling mechanism to
 * find if any transaction happened. After a transaction was found it is saved with
 * the slave-mqueue backend and can be read from the fs. Controller buffer page is
 * determined by adding the first buffer page number to port index multiplied by four.
 * The two target buffer page numbers are determined the same way but they are offset
 * by 2 and 3 from the controller page.
 */
static void rts490xa_hub_delayed_work_polling(struct work_struct *work)
{
	struct i2c_adapter_group *g_adap =
		container_of(work, typeof(*g_adap), delayed_work_polling.work);
	struct logical_bus *bus =
		container_of(g_adap, struct logical_bus, smbus_port_adapter);
	u8 controller_buffer_page =
		RTS490XA_HUB_CONTROLLER_BUFFER_PAGE + 4 * g_adap->tp_port;
	u8 target_port_status =
		RTS490XA_HUB_TP0_SMBUS_AGNT_STS + g_adap->tp_port;
	u8 local_buffer[RTS490XA_HUB_SMBUS_TARGET_PAYLOAD_SIZE] = { 0 };
	u8 target_buffer_page, address, len, flag;
	struct rts490xa_hub *priv = bus->priv;
	struct device *dev = &priv->client->dev;
	u32 status;
	int ret;

	if (backend_is_empty(g_adap,
			     &priv->logical_bus[g_adap->tp_port].adapter)) {
		schedule_delayed_work(
			&g_adap->delayed_work_polling,
			msecs_to_jiffies(RTS490XA_HUB_POLLING_ROLL_PERIOD_MS));
		return;
	}

	ret = regmap_read(priv->regmap, target_port_status, &status);
	if (ret) {
		dev_err(dev, "Failed to read target port status\n");
		return;
	}
	status &= RTS490XA_HUB_TARGET_BUF_STATUS_MASK;

	while (status) {
		if (g_adap->last_processed_buf)
			status &= ~g_adap->last_processed_buf;

		if (status & RTS490XA_HUB_TARGET_BUF_0_RECEIVE) {
			target_buffer_page = controller_buffer_page + 2;
			flag = RTS490XA_HUB_TARGET_BUF_0_RECEIVE;
		} else if (status & RTS490XA_HUB_TARGET_BUF_1_RECEIVE) {
			target_buffer_page = controller_buffer_page + 3;
			flag = RTS490XA_HUB_TARGET_BUF_1_RECEIVE;
		} else {
			break;
		}

		ret = read_smbus_target_buffer_page(
			priv, target_buffer_page, &address, local_buffer, &len);
		if (ret && ret != -EMSGSIZE) {
			dev_err(dev, "Failed to read target buffer page: %d\n",
				ret);
			break;
		}

		g_adap->last_processed_buf = flag;

		if (status & RTS490XA_HUB_TARGET_BUF_OVRFL)
			flag |= RTS490XA_HUB_TARGET_BUF_OVRFL;

		ret = regmap_write(priv->regmap, target_port_status, flag);
		if (ret) {
			dev_err(dev, "Failed to clear target port status\n");
			break;
		}

		if (len) {
			ret = send_smbus_target_data_to_backend(
				priv, g_adap, address, local_buffer, len);
			if (ret) {
				dev_err(dev,
					"Failed to send data to backend: %d\n",
					ret);
				break;
			}
		}

		ret = regmap_read(priv->regmap, target_port_status, &status);
		if (ret) {
			dev_err(dev, "Failed to read target port status\n");
			break;
		}
		status &= RTS490XA_HUB_TARGET_BUF_STATUS_MASK;
	}

	schedule_delayed_work(
		&g_adap->delayed_work_polling,
		msecs_to_jiffies(RTS490XA_HUB_POLLING_ROLL_PERIOD_MS));
}

static int rts490xa_hub_smbus_tp_algo(struct rts490xa_hub *priv, int i)
{
	struct device *dev = &priv->client->dev;
	int ret;

	if (priv->hub_dt_cp1_id != -1 &&
	    priv->hub_dt_cp1_id != priv->hub_pin_cp1_id) {
		dev_warn(dev, "hub_dt_cp1_id not equal to hub_pin_cp1_id!\n");
		return 1;
	}

	priv->logical_bus[i].priv = priv;
	priv->logical_bus[i].smbus_port_adapter.tp_port = i;
	priv->logical_bus[i].smbus_port_adapter.tp_mask = BIT(i);

	INIT_DELAYED_WORK(
		&priv->logical_bus[i].smbus_port_adapter.delayed_work_polling,
		rts490xa_hub_delayed_work_polling);

	priv->logical_bus[i].adapter.dev.parent = dev;
	priv->logical_bus[i].adapter.owner = dev->driver->owner;
	priv->logical_bus[i].adapter.algo = &i2c_controller_smbus_algo;

	sprintf(priv->logical_bus[i].adapter.name, "hub0x%X.port%d",
		priv->hub_dt_cp1_id, i);

	priv->logical_bus[i].adapter.timeout = 1000;
	priv->logical_bus[i].adapter.retries = 3;

	/* Register adapter for target port */
	ret = rts490xa_hub_register_smbus_adapter(priv, i);
	if (ret)
		return ret;

	priv->logical_bus[i].smbus_port_adapter.used = 1;

	return 0;
}

/* return true when backend node exist */
static bool backend_node_is_exist(int port, struct rts490xa_hub *priv, u32 addr)
{
	struct smbus_backend *backend = NULL;

	list_for_each_entry(
		backend,
		&priv->logical_bus[port].smbus_port_adapter.backend_entry,
		list) {
		if (backend->addr == addr)
			return true;
	}

	return false;
}

static int
read_backend_from_rts490xa_hub_dts(struct device_node *i2c_node_target,
				   struct rts490xa_hub *priv)
{
	struct device_node *i2c_node_tp;
	const char *compatible;
	int tp_port, ret;
	u32 addr_dts;
	struct smbus_backend *backend;

	if (sscanf(i2c_node_target->full_name, "target-port@%d", &tp_port) == 0)
		return -EINVAL;

	if (tp_port > RTS490XA_HUB_TP_MAX_COUNT)
		return -ERANGE;

	if (tp_port < 0)
		return -EINVAL;

	INIT_LIST_HEAD(
		&priv->logical_bus[tp_port].smbus_port_adapter.backend_entry);
	for_each_available_child_of_node(i2c_node_target, i2c_node_tp) {
		if (strcmp(i2c_node_tp->name, "backend"))
			continue;

		ret = of_property_read_u32(i2c_node_tp, "target-reg",
					   &addr_dts);
		if (ret)
			return ret;

		if (backend_node_is_exist(tp_port, priv, addr_dts))
			continue;

		ret = of_property_read_string(i2c_node_tp, "compatible",
					      &compatible);
		if (ret)
			return ret;

		if (strcmp("slave-mqueue", compatible) &&
		    strcmp("mctp-i2c-controller", compatible))
			return -EINVAL;

		backend = kzalloc(sizeof(*backend), GFP_KERNEL);
		if (!backend)
			return -ENOMEM;

		backend->addr = addr_dts;
		backend->compatible = compatible;
		priv->logical_bus[tp_port].of_node = i2c_node_target;
		list_add(&backend->list,
			 &priv->logical_bus[tp_port]
				  .smbus_port_adapter.backend_entry);
	}

	return 0;
}

/**
 * This function saves information about the rts490xa_hub's ports
 * working in slave mode. It takes its data from the DTs
 * (aspeed-bmc-intel-avc.dts) and saves the parameters
 * into the coresponding target port i2c_adapter_group structure
 * in the rts490xa_hub
 *
 * @dev: device used by rts490xa_hub
 * @i2c_node_hub: device node pointing to the hub
 * @priv: pointer to the rts490xa_hub structure
 */
static void rts490xa_hub_parse_dt_tp(struct device *dev,
				     const struct device_node *i2c_node_hub,
				     struct rts490xa_hub *priv)
{
	struct device_node *i2c_node_target;
	int ret;

	for_each_available_child_of_node(i2c_node_hub, i2c_node_target) {
		if (!strcmp(i2c_node_target->name, "target-port")) {
			ret = read_backend_from_rts490xa_hub_dts(
				i2c_node_target, priv);
			if (ret)
				dev_err(dev, "DTS entry invalid - error %d",
					ret);
		}
	}
}

static int rts490xa_hub_gpio_direction_input(struct gpio_chip *gc, unsigned off)
{
	struct rts490xa_hub *hub = gpiochip_get_data(gc);
	struct hub_gpio *gpio = &hub->gpio;
	int ret = 0;
	u8 reg, mask = 0;

	dev_dbg(&hub->client->dev, "%s: off=%u\n", __func__, off);

	reg = off % GPIO_BANK_SZ ? RTS490XA_HUB_TP_SDA_OUT_EN :
				   RTS490XA_HUB_TP_SCL_OUT_EN;
	mask = BIT(gpio->tp[off / GPIO_BANK_SZ]);

	dev_dbg(&hub->client->dev, "%s: reg=0x%02x, mask=0x%02x\n", __func__,
		reg, mask);

	ret = regmap_update_bits(hub->regmap, reg, mask, 0);
	return ret;
}

static int rts490xa_hub_gpio_direction_output(struct gpio_chip *gc,
					      unsigned off, int val)
{
	struct rts490xa_hub *hub = gpiochip_get_data(gc);
	struct hub_gpio *gpio = &hub->gpio;
	int ret = 0;
	u8 reg, mask = 0;

	dev_dbg(&hub->client->dev, "%s: off=%u, val=%d\n", __func__, off, val);

	reg = off % GPIO_BANK_SZ ? RTS490XA_HUB_TP_SDA_OUT_EN :
				   RTS490XA_HUB_TP_SCL_OUT_EN;
	mask = BIT(gpio->tp[off / GPIO_BANK_SZ]);

	dev_dbg(&hub->client->dev, "%s: reg=0x%02x, mask=0x%02x\n", __func__,
		reg, mask);

	ret = regmap_update_bits(hub->regmap, reg, mask, mask);
	if (ret)
		return ret;

	ret = regmap_update_bits(hub->regmap, reg + 2, mask, val ? mask : 0);
	return ret;
}

static int rts490xa_hub_gpio_get_value(struct gpio_chip *gc, unsigned off)
{
	struct rts490xa_hub *hub = gpiochip_get_data(gc);
	struct hub_gpio *gpio = &hub->gpio;
	int ret = 0, val = 0, dir;
	u8 reg, shift = 0;

	dev_dbg(&hub->client->dev, "%s: off=%u\n", __func__, off);

	dir = gc->get_direction(gc, off);
	if (dir)
		reg = off % GPIO_BANK_SZ ? RTS490XA_HUB_TP_SDA_IN_LEVEL_STS :
					   RTS490XA_HUB_TP_SCL_IN_LEVEL_STS;
	else
		reg = off % GPIO_BANK_SZ ? RTS490XA_HUB_TP_SDA_OUT_LEVEL :
					   RTS490XA_HUB_TP_SCL_OUT_LEVEL;

	shift = gpio->tp[off / GPIO_BANK_SZ];

	dev_dbg(&hub->client->dev, "%s: reg=0x%02x, shift=0x%02x\n", __func__,
		reg, shift);

	ret = regmap_read(hub->regmap, reg, &val);
	if (ret)
		return ret;

	dev_dbg(&hub->client->dev, "%s: val=%d\n", __func__, val);
	ret = (val >> shift) & 0x01;
	return ret;
}

static void rts490xa_hub_gpio_set_value(struct gpio_chip *gc, unsigned off,
					int val)
{
	struct rts490xa_hub *hub = gpiochip_get_data(gc);
	struct hub_gpio *gpio = &hub->gpio;
	u8 reg, mask = 0;

	dev_dbg(&hub->client->dev, "%s: off=%u, val=%d\n", __func__, off, val);

	reg = off % GPIO_BANK_SZ ? RTS490XA_HUB_TP_SDA_OUT_LEVEL :
				   RTS490XA_HUB_TP_SCL_OUT_LEVEL;
	mask = BIT(gpio->tp[off / GPIO_BANK_SZ]);

	dev_dbg(&hub->client->dev, "%s: reg=0x%02x, mask=0x%02x\n", __func__,
		reg, mask);

	regmap_update_bits(hub->regmap, reg, mask, val ? mask : 0);
}

static int rts490xa_hub_gpio_get_direction(struct gpio_chip *gc, unsigned off)
{
	struct rts490xa_hub *hub = gpiochip_get_data(gc);
	struct hub_gpio *gpio = &hub->gpio;
	int ret = 0, dir = 0;
	u8 reg, shift = 0;

	dev_dbg(&hub->client->dev, "%s: off=%u\n", __func__, off);

	reg = off % GPIO_BANK_SZ ? RTS490XA_HUB_TP_SDA_OUT_EN :
				   RTS490XA_HUB_TP_SCL_OUT_EN;
	shift = gpio->tp[off / GPIO_BANK_SZ];

	dev_dbg(&hub->client->dev, "%s: reg=0x%02x, shift=0x%02x\n", __func__,
		reg, shift);

	ret = regmap_read(hub->regmap, reg, &dir);
	if (ret)
		return ret;

	dev_dbg(&hub->client->dev, "%s: dir=%d\n", __func__, dir);
	ret = ~(dir >> shift) & 0x01;
	return ret;
}

static void rts490xa_hub_setup_gpio(struct rts490xa_hub *hub)
{
	struct hub_gpio *gpio = &hub->gpio;
	struct gpio_chip *gc = &gpio->chip;

	dev_dbg(&hub->client->dev, "%s\n", __func__);

	gc->direction_input = rts490xa_hub_gpio_direction_input;
	gc->direction_output = rts490xa_hub_gpio_direction_output;
	gc->get = rts490xa_hub_gpio_get_value;
	gc->set = rts490xa_hub_gpio_set_value;
	gc->get_direction = rts490xa_hub_gpio_get_direction;
	gc->can_sleep = true;

	gc->base = -1;
	gc->ngpio = gpio->nums;
	gc->label = dev_name(&hub->client->dev);
	gc->parent = &hub->client->dev;
	gc->owner = THIS_MODULE;
}

static int rts490xa_hub_probe(struct i2c_client *client)
{
	struct regmap_config rts490xa_hub_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8,
	};
	struct device *dev = &client->dev;
	struct device_node *node = NULL;
	struct regmap *regmap;
	struct rts490xa_hub *priv;
	char hub_id[32];
	int ret;
	int i;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client = client;
	mutex_init(&priv->page_mutex);
	i2c_set_clientdata(client, priv);
	INIT_DELAYED_WORK(&priv->delayed_work, rts490xa_hub_delayed_work);
	snprintf(hub_id, sizeof(hub_id), "rts490xa-hub-%d-%x",
		 client->adapter->nr, client->addr);
	ret = rts490xa_hub_debugfs_init(priv, hub_id);
	if (ret)
		dev_dbg(dev, "Failed to initialize DebugFS: %d\n", ret);

	rts490xa_hub_of_default_configuration(dev);

	regmap = devm_regmap_init_i2c(client, &rts490xa_hub_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to register RTS490XA HUB regmap\n");
		goto error;
	}
	priv->regmap = regmap;

	ret = rts490xa_hub_read_id(dev);
	if (ret)
		goto error;

	priv->hub_dt_sel_id = -1;
	priv->hub_dt_cp1_id = -1;
	if (priv->hub_pin_cp1_id >= 0 && priv->hub_pin_sel_id >= 0)
		/* Find hub node in DT matching HW ID or just first without ID provided in DT */
		node = rts490xa_hub_get_dt_hub_node(dev->parent->of_node, priv);

	if (!node) {
		dev_info(dev,
			 "No DT entry - running with hardware defaults.\n");
	} else {
		of_node_get(node);
		rts490xa_hub_of_get_conf_static(dev, node);
		of_node_put(node);

		/* Parse DTS to find data on the SMBus target mode */
		rts490xa_hub_parse_dt_tp(dev, node, priv);
	}

	if (node) {
		if (dev->of_node != node) {
			if (dev->of_node) {
				sysfs_remove_link(&dev->kobj, "of_node");
				of_node_put(dev->of_node);
			}

			dev->of_node = of_node_get(node);
			if (dev->of_node) {
				ret = sysfs_create_link(
					&dev->kobj, of_node_kobj(dev->of_node),
					"of_node");
				if (ret)
					dev_warn(
						dev,
						"Error %d creating of_node link\n",
						ret);
			}
		}
	}

	/* Unlock access to protected registers */
	ret = regmap_write(priv->regmap, RTS490XA_HUB_PROTECTION_CODE,
			   REGISTERS_UNLOCK_CODE);
	if (ret) {
		dev_err(dev, "Failed to unlock HUB's protected registers\n");
		goto error;
	}

	/* Register logic for native smbus ports */
	for (i = 0; i < RTS490XA_HUB_TP_MAX_COUNT; i++) {
		priv->logical_bus[i].smbus_port_adapter.used = 0;
		if (priv->settings.tp[i].mode == RTS490XA_HUB_DT_TP_MODE_SMBUS)
			ret = rts490xa_hub_smbus_tp_algo(priv, i);
	}

	ret = rts490xa_hub_configure_hw(dev);
	if (ret) {
		dev_err(dev, "Failed to configure the HUB\n");
		goto error;
	}

	/* Lock access to protected registers */
	ret = regmap_write(priv->regmap, RTS490XA_HUB_PROTECTION_CODE,
			   REGISTERS_LOCK_CODE);
	if (ret) {
		dev_err(dev, "Failed to lock HUB's protected registers\n");
		goto error;
	}

	/* TBD: Apply special/security lock here using DEV_CMD register */

	if (priv->gpio.nums > 0) {
		rts490xa_hub_setup_gpio(priv);

		ret = devm_gpiochip_add_data(dev, &priv->gpio.chip, priv);
		if (ret) {
			dev_err(dev, "gpiochip add data fail!\n");
			goto error;
		}
	}

	schedule_delayed_work(&priv->delayed_work, msecs_to_jiffies(100));

	return 0;

error:
	debugfs_remove_recursive(priv->debug_dir);
	return ret;
}

static void rts490xa_hub_remove(struct i2c_client *client)
{
	struct rts490xa_hub *priv = i2c_get_clientdata(client);
	struct i2c_adapter_group *g_adap;
	struct smbus_backend *backend = NULL;
	int i;

	for (i = 0; i < RTS490XA_HUB_TP_MAX_COUNT; i++) {
		if (priv->logical_bus[i].smbus_port_adapter.used) {
			g_adap = &priv->logical_bus[i].smbus_port_adapter;
			cancel_delayed_work_sync(&g_adap->delayed_work_polling);
			list_for_each_entry(backend, &g_adap->backend_entry,
					    list) {
				i2c_unregister_device(backend->client);
				kfree(backend);
			}
		}

		if (priv->logical_bus[i].smbus_port_adapter.used)
			i2c_del_adapter(&priv->logical_bus[i].adapter);
	}

	cancel_delayed_work_sync(&priv->delayed_work);
	debugfs_remove_recursive(priv->debug_dir);
}

const struct of_device_id rts490xa_hub_of_match[] = {
	{ .compatible = "realtek,rts490xa-hub" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, rts490xa_hub_of_match);

struct i2c_driver i2c_hub = {
	.driver = {
		.name = "i2c-hub",
		.of_match_table = rts490xa_hub_of_match,
	},
	.probe = rts490xa_hub_probe,
	.remove = rts490xa_hub_remove,
};

module_i2c_driver(i2c_hub);

MODULE_DESCRIPTION("RTS490xA HUB Driver");
MODULE_LICENSE("GPL");
