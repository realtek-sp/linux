
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

#include <linux/bitmap.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinconf-generic.h>
#include "../pinctrl-utils.h"

#define REALTEK_MC_PINCTRL_NAME "realtek-mc-pinctrl"

#define GPIO_OE		       0x0000
#define GPIO_VALUE	       0x0004
#define GPIO_PULLCTRL	       0x0008
#define GPIO_INT_EN	       0x000c
#define GPIO_INT	       0x0010
#define GPIO_DRV_SEL	       0x0014
#define GPIO_SR_CTRL	       0x0018
#define GPIO_0_15_PAD_CFG      0x001c
#define UART0_GPIO_OE	       0x0020
#define UART0_GPIO_VALUE       0x0024
#define UART0_PULLCTRL	       0x0028
#define UART0_GPIO_INT_EN      0x002c
#define UART0_GPIO_INT	       0x0030
#define UART0_DRV_SEL	       0x0034
#define UART0_SR_CTRL	       0x0038
#define UART0_PAD_CFG	       0x003C
#define UART1_GPIO_OE	       0x0040
#define UART1_GPIO_VALUE       0x0044
#define UART1_PULLCTRL	       0x0048
#define UART1_GPIO_INT_EN      0x004c
#define UART1_GPIO_INT	       0x0050
#define UART1_DRV_SEL	       0x0054
#define UART1_SR_CTRL	       0x0058
#define UART1_PAD_CFG	       0x005C
#define UART2_GPIO_OE	       0x0060
#define UART2_GPIO_VALUE       0x0064
#define UART2_PULLCTRL	       0x0068
#define UART2_GPIO_INT_EN      0x006c
#define UART2_GPIO_INT	       0x0070
#define UART2_DRV_SEL	       0x0074
#define UART2_SR_CTRL	       0x0078
#define UART2_PAD_CFG	       0x007C
#define PWM_GPIO_OE	       0x0080
#define PWM_GPIO_VALUE	       0x0084
#define PWM_PULLCTRL	       0x0088
#define PWM_GPIO_INT_EN	       0x008C
#define PWM_GPIO_INT	       0x0090
#define PWM_DRV_SEL	       0x0094
#define PWM_SR_CTRL	       0x0098
#define PWM_PAD_CFG	       0x009C
#define XB2_I2C_GPIO_OE	       0x00a0
#define XB2_I2C_GPIO_VALUE     0x00a4
#define XB2_I2C_PULLCTRL       0x00a8
#define XB2_I2C_GPIO_INT_EN    0x00ac
#define XB2_I2C_GPIO_INT       0x00b0
#define XB2_I2C_GPIO_DRV_SEL   0x00b4
#define XB2_I2C_GPIO_SR_CTRL   0x00b8
#define XB2_I2C_PAD_CFG	       0x00bC
#define SD0_GPIO_OE	       0x00c0
#define SD0_GPIO_VALUE	       0x00c4
#define SD0_PULLCTRL	       0x00c8
#define SD0_GPIO_INT_EN	       0x00cc
#define SD0_GPIO_INT	       0x00d0
#define SD0_GPIO_DRV_SEL       0x00d4
#define SD0_GPIO_SR_CTRL       0x00d8
#define SD0_PAD_CFG	       0x00dC
#define SD1_GPIO_OE	       0x00e0
#define SD1_GPIO_VALUE	       0x00e4
#define SD1_PULLCTRL	       0x00e8
#define SD1_GPIO_INT_EN	       0x00ec
#define SD1_GPIO_INT	       0x00f0
#define SD1_GPIO_DRV_SEL       0x00f4
#define SD1_GPIO_SR_CTRL       0x00f8
#define SD1_PAD_CFG	       0x00fC
#define VIDEO_GPIO_OE	       0x0100
#define VIDEO_GPIO_VALUE       0x0104
#define VIDEO_PULLCTRL	       0x0108
#define VIDEO_GPIO_INT_EN      0x010c
#define VIDEO_GPIO_INT	       0x0110
#define VIDEO_DRV_SEL	       0x0114
#define VIDEO_SR_CTRL	       0x0118
#define VIDEO_PAD_CFG	       0x011c
#define DMIC_GPIO_OE	       0x0130
#define DMIC_GPIO_VALUE	       0x0134
#define DMIC_PULLCTRL	       0x0138
#define DMIC_GPIO_INT_EN       0x013c
#define DMIC_GPIO_INT	       0x0140
#define DMIC_DRV_SEL	       0x0144
#define DMIC_SR_CTRL	       0x0148
#define DMIC_PAD_CFG	       0x014C
#define AUDIO_ADDA_GPIO_OE     0x0150
#define AUDIO_ADDA_GPIO_VALUE  0x0154
#define AUDIO_ADDA_PULLCTRL    0x0158
#define AUDIO_ADDA_GPIO_INT_EN 0x015c
#define AUDIO_ADDA_GPIO_INT    0x0160
#define AUDIO_ADDA_DRV_SEL     0x0164
#define AUDIO_ADDA_SR_CTRL     0x0168
#define AUDIO_ADDA_PAD_CFG     0x016C
#define I2S_GPIO_OE	       0x0170
#define I2S_GPIO_VALUE	       0x0174
#define I2S_PULLCTRL	       0x0178
#define I2S_GPIO_INT_EN	       0x017C
#define I2S_GPIO_INT	       0x0180
#define I2S_DRV_SEL	       0x0184
#define I2S_SR_CTRL	       0x0188
#define I2S_PAD_CFG	       0x018C
#define SARADC_GPIO_OE	       0x0190
#define SARADC_GPIO_VALUE      0x0194
#define SARADC_PULLCTRL	       0x0198
#define SARADC_GPIO_INT_EN     0x019c
#define SARADC_GPIO_INT	       0x01a0
#define SARADC_DRV_SEL	       0x01a4
#define SARADC_SR_CTRL	       0x01a8
#define SARADC_PAD_CFG	       0x01aC
#define USB0_GPIO_OE	       0x01f0
#define USB0_GPIO_VALUE	       0x01f4
#define USB0_PULLCTRL	       0x01f8
#define USB0_GPIO_INT_EN       0x01fc
#define USB0_GPIO_INT	       0x0200
#define USB0_DRV_SEL	       0x0204
#define USB0_SR_CTRL	       0x0208
#define USB0_PAD_CFG	       0x020c
#define USB1_GPIO_OE	       0x0210
#define USB1_GPIO_VALUE	       0x0214
#define USB1_PULLCTRL	       0x0218
#define USB1_GPIO_INT_EN       0x021c
#define USB1_GPIO_INT	       0x0220
#define USB1_DRV_SEL	       0x0224
#define USB1_SR_CTRL	       0x0228
#define USB1_PAD_CFG	       0x022C
#define USB2_GPIO_OE	       0x0230
#define USB2_GPIO_VALUE	       0x0234
#define USB2_PULLCTRL	       0x0238
#define USB2_GPIO_INT_EN       0x023c
#define USB2_GPIO_INT	       0x0240
#define USB2_DRV_SEL	       0x0244
#define USB2_SR_CTRL	       0x0248
#define USB2_PAD_CFG	       0x024C
#define SPI_GPIO_OE	       0x0250
#define SPI_GPIO_VALUE	       0x0254
#define SPI_PULLCTRL	       0x0258
#define SPI_GPIO_INT_EN	       0x025c
#define SPI_GPIO_INT	       0x0260
#define SPI_DRV_SEL	       0x0264
#define SPI_SR_CTRL	       0x0268
#define SPI_PAD_CFG	       0x026C
#define SSOR_I2C_GPIO_OE       0x0270
#define SSOR_I2C_GPIO_VALUE    0x0274
#define SSOR_I2C_PULLCTRL      0x0278
#define SSOR_I2C_GPIO_INT_EN   0x027c
#define SSOR_I2C_GPIO_INT      0x0280
#define SSOR_I2C_DRV_SEL       0x0284
#define SSOR_I2C_SR_CTRL       0x0288
#define SSOR_I2C_PAD_CFG       0x028c
#define ASIC_DBG_EN	       0x0290
#define ASIC_DBG_SEL	       0x0294
#define PAD_V18_EN	       0x0298
#define PWM_LED_SEL	       0x02a0

enum {
	GPIO_FUNC_SELECT,
	SDIO0_FUNC_SELECT,
	SDIO1_FUNC_SELECT,
	UART_FUNC_SELECT,
	PWM_FUNC_SELECT,
	USBD_FUNC_SELECT,
	ETNLED_FUNC_SELECT,
	USBHST_FUNC_SELECT,
	SARADC_FUNC_SELECT,
	SSI_FUNC_SELECT,
	I2C_FUNC_SELECT,
	SPI_FUNC_SELECT,
};

#define RTS_PINRANGE(a, b, c) { .gpio_base = a, .pin_base = b, .pins = c }

#define RTS_GETFIELD(val, width, offset) ((val >> offset) & ((1 << width) - 1))
#define RTS_SETFIELD(reg, field, width, offset)      \
	((reg & (~(((1 << width) - 1) << offset))) | \
	 ((field & ((1 << width) - 1)) << offset))

#define RTS_SOC_HW_ID(type)	((int)(type) & 0xff)
#define RTS_MAX_NGPIO		89

enum {
	TYPE_RTS493XA = 1,

	TYPE_FPGA = (1 << 16),
};

enum rts_pinconf_pull {
	RTS_PINCONFIG_DUMMY_OP0,
	RTS_PINCONFIG_PULL_NONE,
	RTS_PINCONFIG_DUMMY_OP1,
	RTS_PINCONFIG_PULL_DOWN,
	RTS_PINCONFIG_DUMMY_OP2,
	RTS_PINCONFIG_PULL_UP,
	RTS_PIN_CONFIG_DRIVE_STRENGTH = 9,
	RTS_PIN_CONFIG_SLEW_RATE = 17,
	RTS_PINCONFIG_INPUT,
	RTS_PINCONFIG_OUTPUT,
};

struct rts_pinctrl {
	struct gpio_chip *gpio_chip;
	struct device *dev;
	unsigned int *irq_type;
	struct pinctrl_dev *rtspctl_dev;
	struct irq_domain *irq_domain;
	spinlock_t irq_lock;
	void __iomem *addr;
	void __iomem *addr_sdio0;
	void __iomem *addr_sdio1;
	void __iomem *addr_i2s;
	struct platform_device *pdev;
	int irq;
	u64 pinsmask[2];
	int devt;
	u8 audio_adda_gpio_value;
	u8 usb0_gpio_value;
	u8 usb1_gpio_value;
};

struct rts_pin_group {
	const char *name;
	const unsigned int *pins;
	const unsigned int num_pins;
};

struct rts_func {
	const char *name;
	const char *const *groups;
	const unsigned int num_groups;
};

struct rts_pinpair {
	unsigned int gpio_base;
	unsigned int pin_base;
	unsigned int pins;
};

struct sharepin_cfg_addr {
	int pinl;
	int pinh;
	int pint;
	int pinaddr;
};

struct pinregs {
	int gpio_oe;
	int gpio_value;
	int pullctrl;
	int gpio_int_en;
	int gpio_int;
	int drv_sel;
	int sr_ctrl;
	int pad_cfg;
};

enum {
	GPIO_TYPE_GENERIC,
	GPIO_TYPE_UART0,
	GPIO_TYPE_UART1,
	GPIO_TYPE_UART2,
	GPIO_TYPE_PWM,
	GPIO_TYPE_I2C,
	GPIO_TYPE_SDIO0,
	GPIO_TYPE_SDIO1,
	GPIO_TYPE_SSOR,
	GPIO_TYPE_DMIC,
	GPIO_TYPE_ADDA,
	GPIO_TYPE_I2S,
	GPIO_TYPE_SARADC,
	GPIO_TYPE_USBH,
	GPIO_TYPE_USBD,
	GPIO_TYPE_USB3,
	GPIO_TYPE_SSORI2C,
	GPIO_TYPE_SPI,
};

static struct lock_class_key gpio_lock_class;
static struct lock_class_key gpio_request_class;

static struct pinctrl_pin_desc rts_gpio_pins[] = {
	PINCTRL_PIN(0, "GPIO0"),
	PINCTRL_PIN(1, "GPIO1"),
	PINCTRL_PIN(2, "GPIO2"),
	PINCTRL_PIN(3, "GPIO3"),
	PINCTRL_PIN(4, "GPIO4"),
	PINCTRL_PIN(5, "GPIO5"),
	PINCTRL_PIN(6, "GPIO6"),
	PINCTRL_PIN(7, "GPIO7"),
	PINCTRL_PIN(8, "GPIO8"),
	PINCTRL_PIN(9, "GPIO9"),
	PINCTRL_PIN(10, "GPIO10"),
	PINCTRL_PIN(11, "GPIO11"),
	PINCTRL_PIN(12, "GPIO12"),
	PINCTRL_PIN(13, "GPIO13"),
	PINCTRL_PIN(14, "GPIO14"),
	PINCTRL_PIN(15, "GPIO15"),
	PINCTRL_PIN(16, "UART0RXD"),
	PINCTRL_PIN(17, "UART0TXD"),
	PINCTRL_PIN(18, "UART0RTS"),
	PINCTRL_PIN(19, "UART0CTS"),
	PINCTRL_PIN(20, "UART1RXD"),
	PINCTRL_PIN(21, "UART1TXD"),
	PINCTRL_PIN(22, "UART2RXD"),
	PINCTRL_PIN(23, "UART2TXD"),
	PINCTRL_PIN(24, "UART2RTS"),
	PINCTRL_PIN(25, "UART2CTS"),
	PINCTRL_PIN(26, "PWMGPIO0"),
	PINCTRL_PIN(27, "PWMGPIO1"),
	PINCTRL_PIN(28, "PWMGPIO2"),
	PINCTRL_PIN(29, "PWMGPIO3"),
	PINCTRL_PIN(30, "I2CSCL"),
	PINCTRL_PIN(31, "I2CSDA"),
	PINCTRL_PIN(32, "SDIO0_CMD"),
	PINCTRL_PIN(33, "SDIO0_DATA0"),
	PINCTRL_PIN(34, "SDIO0_DATA1"),
	PINCTRL_PIN(35, "SDIO0_DATA2"),
	PINCTRL_PIN(36, "SDIO0_DATA3"),
	PINCTRL_PIN(37, "SDIO0_CLK"),
	PINCTRL_PIN(38, "SDIO0_WP"),
	PINCTRL_PIN(39, "SDIO0_CD"),
	PINCTRL_PIN(40, "SDIO1_CMD"),
	PINCTRL_PIN(41, "SDIO1_DATA0"),
	PINCTRL_PIN(42, "SDIO1_DATA1"),
	PINCTRL_PIN(43, "SDIO1_DATA2"),
	PINCTRL_PIN(44, "SDIO1_DATA3"),
	PINCTRL_PIN(45, "SDIO1_CLK"),
	PINCTRL_PIN(46, "SDIO1_WP"),
	PINCTRL_PIN(47, "SDIO1_CD"),
	PINCTRL_PIN(48, "PAD_SSOR_PIXDIN_EXT_0"),
	PINCTRL_PIN(49, "PAD_SSOR_PIXDIN_EXT_1"),
	PINCTRL_PIN(50, "PAD_SSOR_PIXDIN_0"),
	PINCTRL_PIN(51, "PAD_SSOR_PIXDIN_1"),
	PINCTRL_PIN(52, "PAD_SSOR_PIXDIN_2"),
	PINCTRL_PIN(53, "PAD_SSOR_PIXDIN_3"),
	PINCTRL_PIN(54, "PAD_SSOR_PIXDIN_6"),
	PINCTRL_PIN(55, "PAD_SSOR_PIXDIN_7"),
	PINCTRL_PIN(56, "PAD_SSOR_PIXDIN_8"),
	PINCTRL_PIN(57, "PAD_SSOR_PIXDIN_9"),
	PINCTRL_PIN(58, "PAD_SSOR_HSYNC"),
	PINCTRL_PIN(59, "PAD_SSOR_VSYNC"),
	PINCTRL_PIN(60, "PAD_SSOR_PIXCLK"),
	PINCTRL_PIN(61, "PAD_CLK_DMIC1"),
	PINCTRL_PIN(62, "PAD_DMIC1_IN"),
	PINCTRL_PIN(63, "PAD_CLK_DMIC2"),
	PINCTRL_PIN(64, "PAD_DMIC2_IN"),
	PINCTRL_PIN(65, "LINE_OUTL"),
	PINCTRL_PIN(66, "LINE_OUTR"),
	PINCTRL_PIN(67, "MIC_INL"),
	PINCTRL_PIN(68, "MIC_INR"),
	PINCTRL_PIN(69, "PAD_I2S_MCLK"),
	PINCTRL_PIN(70, "PAD_I2S_SCK"),
	PINCTRL_PIN(71, "PAD_I2S_WS"),
	PINCTRL_PIN(72, "PAD_I2S_SD_IN"),
	PINCTRL_PIN(73, "PAD_I2S_SD_OUT"),
	PINCTRL_PIN(74, "CH_SAR_PAD0"),
	PINCTRL_PIN(75, "CH_SAR_PAD1"),
	PINCTRL_PIN(76, "CH_SAR_PAD2"),
	PINCTRL_PIN(77, "CH_SAR_PAD3"),
	PINCTRL_PIN(78, "USB_HOST_DP"),
	PINCTRL_PIN(79, "USB_HOST_DM"),
	PINCTRL_PIN(80, "USB_DEV_DP"),
	PINCTRL_PIN(81, "USB_DEV_DM"),
	PINCTRL_PIN(82, "USB_HOST_PWREN"),
	PINCTRL_PIN(83, "USB_HOST_OCD"),
	PINCTRL_PIN(84, "USB_DEV_VBUS"),
	PINCTRL_PIN(85, "SSOR_I2C_SCL"),
	PINCTRL_PIN(86, "SSOR_I2C_SDA"),
	PINCTRL_PIN(87, "SPI_HOLD"),
	PINCTRL_PIN(88, "SPI_WP"),
	PINCTRL_PIN(89, "SPI_SO"), //pin is not gpio
	PINCTRL_PIN(90, "SPI_SI"), //pin is not gpio
	PINCTRL_PIN(91, "SPI_SCK"), //pin is not gpio
	PINCTRL_PIN(92, "SPI_CSN"), //pin is not gpio
};

static struct sharepin_cfg_addr pincfgaddr[] = {
	{ .pinl = 0, .pinh = 15, .pint = GPIO_TYPE_GENERIC, .pinaddr = GPIO_OE },
	{ .pinl = 16,
	  .pinh = 19,
	  .pint = GPIO_TYPE_UART0,
	  .pinaddr = UART0_GPIO_OE },
	{ .pinl = 20,
	  .pinh = 21,
	  .pint = GPIO_TYPE_UART1,
	  .pinaddr = UART1_GPIO_OE },
	{ .pinl = 22,
	  .pinh = 25,
	  .pint = GPIO_TYPE_UART2,
	  .pinaddr = UART2_GPIO_OE },
	{ .pinl = 26,
	  .pinh = 29,
	  .pint = GPIO_TYPE_PWM,
	  .pinaddr = PWM_GPIO_OE },
	{ .pinl = 30,
	  .pinh = 31,
	  .pint = GPIO_TYPE_I2C,
	  .pinaddr = XB2_I2C_GPIO_OE },
	{ .pinl = 32,
	  .pinh = 39,
	  .pint = GPIO_TYPE_SDIO0,
	  .pinaddr = SD0_GPIO_OE },
	{ .pinl = 40,
	  .pinh = 47,
	  .pint = GPIO_TYPE_SDIO1,
	  .pinaddr = SD1_GPIO_OE },
	{ .pinl = 48,
	  .pinh = 60,
	  .pint = GPIO_TYPE_SSOR,
	  .pinaddr = VIDEO_GPIO_OE },
	{ .pinl = 61,
	  .pinh = 64,
	  .pint = GPIO_TYPE_DMIC,
	  .pinaddr = DMIC_GPIO_OE },
	{ .pinl = 65,
	  .pinh = 68,
	  .pint = GPIO_TYPE_ADDA,
	  .pinaddr = AUDIO_ADDA_GPIO_OE },
	{ .pinl = 69,
	  .pinh = 73,
	  .pint = GPIO_TYPE_I2S,
	  .pinaddr = I2S_GPIO_OE },
	{ .pinl = 74,
	  .pinh = 77,
	  .pint = GPIO_TYPE_SARADC,
	  .pinaddr = SARADC_GPIO_OE },
	{ .pinl = 78,
	  .pinh = 79,
	  .pint = GPIO_TYPE_USBH,
	  .pinaddr = USB0_GPIO_OE },
	{ .pinl = 80,
	  .pinh = 81,
	  .pint = GPIO_TYPE_USBD,
	  .pinaddr = USB1_GPIO_OE },
	{ .pinl = 82,
	  .pinh = 84,
	  .pint = GPIO_TYPE_USB3,
	  .pinaddr = USB2_GPIO_OE },
	{ .pinl = 85,
	  .pinh = 86,
	  .pint = GPIO_TYPE_SSORI2C,
	  .pinaddr = SSOR_I2C_GPIO_OE },
	{ .pinl = 87,
	  .pinh = 88,
	  .pint = GPIO_TYPE_SPI,
	  .pinaddr = SPI_GPIO_OE },
};

static const unsigned int gpio_pins[] = {
	0,  1,	2,  3,	4,  5,	6,  7,	8,  9,	10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
	54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
	72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88
};

static const unsigned int uart0_pins[] = { 16, 17, 18, 19 };

static const unsigned int ssi_pins[] = { 16, 17, 18, 19 };

static const unsigned int uart1_pins[] = { 20, 21 };

static const unsigned int uart2_pins[] = { 22, 23, 24, 25 };

static const unsigned int pwm0_pins[] = { 26 };

static const unsigned int pwm1_pins[] = { 27 };

static const unsigned int pwm2_pins[] = { 28 };

static const unsigned int pwm3_pins[] = { 29 };

static const unsigned int i2c0_pins[] = { 30, 31 };

static const unsigned int i2cpwm_pins[] = { 26, 27 };

static const unsigned int sdio0_pins[] = { 32, 33, 34, 35, 36, 37 };

static const unsigned int sdio0_wp_pins[] = { 38 };

static const unsigned int sdio0_cd_pins[] = { 39 };

static const unsigned int sdio1_pins[] = { 40, 41, 42, 43, 44, 45 };

static const unsigned int sdio1_video_pins[] = {
	8, 48, 49, 58, 59, 60,
};

static const unsigned int sdio1_wp_pins[] = { 46 };

static const unsigned int sdio1_cd_pins[] = { 47 };

static const unsigned int saradc0_pins[] = { 74 };

static const unsigned int saradc1_pins[] = { 75 };

static const unsigned int saradc2_pins[] = { 76 };

static const unsigned int saradc3_pins[] = { 77 };

static const unsigned int usbh_pins[] = { 78, 79, 82, 83 };

static const unsigned int usbd_pins[] = { 80, 81, 84 };

static const unsigned int etnled0_pins[] = { 26 };

static const unsigned int etnled1_pins[] = { 27 };

static const unsigned int etnled2_pins[] = { 28 };

static const unsigned int usbd_uart2_pins[] = { 80, 81 };

static const unsigned int i2c1_pins[] = { 85, 86 };

static const unsigned int spi_pins[] = { 87, 88, 89, 90, 91, 92 };

static const unsigned int pwmsd_pins[] = { 41, 42, 43, 44 };

static const unsigned int default_pins[] = {};

enum {
	GPIO_GROUP_SELECT,
	UART0_GROUP_SELECT,
	UART1_GROUP_SELECT,
	UART2_GROUP_SELECT,
	PWM0_GROUP_SELECT,
	PWM1_GROUP_SELECT,
	PWM2_GROUP_SELECT,
	PWM3_GROUP_SELECT,
	USBD_GROUP_SELECT,
	ETNLED0_GROUP_SELECT,
	ETNLED1_GROUP_SELECT,
	ETNLED2_GROUP_SELECT,
	USBH_GROUP_SELECT,
	SARADC0_GROUP_SELECT,
	SARADC1_GROUP_SELECT,
	SARADC2_GROUP_SELECT,
	SARADC3_GROUP_SELECT,
	SSI_GROUP_SELECT,
	SDIO0_GROUP_SELECT,
	SDIO0WP_GROUP_SELECT,
	SDIO0CD_GROUP_SELECT,
	SDIO1_GROUP_SELECT,
	SDIO1VIDEO_GROUP_SELECT,
	SDIO1WP_GROUP_SELECT,
	SDIO1CD_GROUP_SELECT,
	I2C0_GROUP_SELECT,
	I2CPWM_GROUP_SELECT,
	UART2_USB_GROUP_SELECT,
	I2C1_GROUP_SELECT,
	SPI_GROUP_SELECT,
	PWMSD_GROUP_SELECT,
	DEFAULT_GROUP_SELECT,
};

static const struct rts_pin_group rts_pin_groups[] = {
	{
		.name = "gpiogrp",
		.pins = gpio_pins,
		.num_pins = ARRAY_SIZE(gpio_pins),
	},
	{
		.name = "uart0grp",
		.pins = uart0_pins,
		.num_pins = ARRAY_SIZE(uart0_pins),
	},
	{
		.name = "uart1grp",
		.pins = uart1_pins,
		.num_pins = ARRAY_SIZE(uart1_pins),
	},
	{
		.name = "uart2grp",
		.pins = uart2_pins,
		.num_pins = ARRAY_SIZE(uart2_pins),
	},
	{
		.name = "pwm0grp",
		.pins = pwm0_pins,
		.num_pins = ARRAY_SIZE(pwm0_pins),
	},
	{
		.name = "pwm1grp",
		.pins = pwm1_pins,
		.num_pins = ARRAY_SIZE(pwm1_pins),
	},
	{
		.name = "pwm2grp",
		.pins = pwm2_pins,
		.num_pins = ARRAY_SIZE(pwm2_pins),
	},
	{
		.name = "pwm3grp",
		.pins = pwm3_pins,
		.num_pins = ARRAY_SIZE(pwm3_pins),
	},
	{
		.name = "usbdgrp",
		.pins = usbd_pins,
		.num_pins = ARRAY_SIZE(usbd_pins),
	},
	{
		.name = "etnled0grp",
		.pins = etnled0_pins,
		.num_pins = ARRAY_SIZE(etnled0_pins),
	},
	{
		.name = "etnled1grp",
		.pins = etnled1_pins,
		.num_pins = ARRAY_SIZE(etnled1_pins),
	},
	{
		.name = "etnled2grp",
		.pins = etnled2_pins,
		.num_pins = ARRAY_SIZE(etnled2_pins),
	},
	{
		.name = "usbhgrp",
		.pins = usbh_pins,
		.num_pins = ARRAY_SIZE(usbh_pins),
	},
	{
		.name = "saradc0grp",
		.pins = saradc0_pins,
		.num_pins = ARRAY_SIZE(saradc0_pins),
	},
	{
		.name = "saradc1grp",
		.pins = saradc1_pins,
		.num_pins = ARRAY_SIZE(saradc1_pins),
	},
	{
		.name = "saradc2grp",
		.pins = saradc2_pins,
		.num_pins = ARRAY_SIZE(saradc2_pins),
	},
	{
		.name = "saradc3grp",
		.pins = saradc3_pins,
		.num_pins = ARRAY_SIZE(saradc3_pins),
	},
	{
		.name = "ssigrp",
		.pins = ssi_pins,
		.num_pins = ARRAY_SIZE(ssi_pins),
	},
	{
		.name = "sdio0grp",
		.pins = sdio0_pins,
		.num_pins = ARRAY_SIZE(sdio0_pins),
	},
	{
		.name = "sdio0wpgrp",
		.pins = sdio0_wp_pins,
		.num_pins = ARRAY_SIZE(sdio0_wp_pins),
	},
	{
		.name = "sdio0cdgrp",
		.pins = sdio0_cd_pins,
		.num_pins = ARRAY_SIZE(sdio0_cd_pins),
	},
	{
		.name = "sdio1grp",
		.pins = sdio1_pins,
		.num_pins = ARRAY_SIZE(sdio1_pins),
	},
	{
		.name = "sdio1videogrp",
		.pins = sdio1_video_pins,
		.num_pins = ARRAY_SIZE(sdio1_video_pins),
	},
	{
		.name = "sdio1wpgrp",
		.pins = sdio1_wp_pins,
		.num_pins = ARRAY_SIZE(sdio1_wp_pins),
	},
	{
		.name = "sdio1cdgrp",
		.pins = sdio1_cd_pins,
		.num_pins = ARRAY_SIZE(sdio1_cd_pins),
	},
	{
		.name = "i2c0grp",
		.pins = i2c0_pins,
		.num_pins = ARRAY_SIZE(i2c0_pins),
	},
	{
		.name = "i2cpwmgrp",
		.pins = i2cpwm_pins,
		.num_pins = ARRAY_SIZE(i2cpwm_pins),
	},
	{
		.name = "uart2usbdgrp",
		.pins = usbd_uart2_pins,
		.num_pins = ARRAY_SIZE(usbd_uart2_pins),
	},
	{
		.name = "i2c1grp",
		.pins = i2c1_pins,
		.num_pins = ARRAY_SIZE(i2c1_pins),
	},
	{
		.name = "spigrp",
		.pins = spi_pins,
		.num_pins = ARRAY_SIZE(spi_pins),
	},
	{
		.name = "pwmsdgrp",
		.pins = pwmsd_pins,
		.num_pins = ARRAY_SIZE(pwmsd_pins),
	},
	{
		.name = "default",
		.pins = default_pins,
		.num_pins = ARRAY_SIZE(default_pins),
	},
};

static const char *const gpiogrps[] = { "gpiogrp" };

static const char *const uartgrps[] = { "uart0grp", "uart1grp", "uart2grp",
					"uart2usbdgrp" };

static const char *const pwmgrps[] = { "pwm0grp", "pwm1grp", "pwm2grp",
				       "pwm3grp", "pwmsdgrp" };

static const char *const usbdgrps[] = { "usbdgrp", "default" };

static const char *const etnledgrps[] = { "etnled0grp", "etnled1grp",
					  "etnled2grp" };

static const char *const saradcgrps[] = {
	"saradc0grp",
	"saradc1grp",
	"saradc2grp",
	"saradc3grp",
};

static const char *const usbhgrps[] = { "usbhgrp" };

static const char *const ssigrps[] = { "ssigrp" };

static const char *const sdio0grps[] = { "sdio0grp", "sdio0wpgrp",
					 "sdio0cdgrp" };

static const char *const sdio1grps[] = { "sdio1grp", "sdio1wpgrp", "sdio1cdgrp",
					 "sdio1videogrp" };

static const char *const i2cgrps[] = { "i2c0grp", "i2cpwmgrp", "i2c1grp" };

static const char *const spigrps[] = { "spigrp", "default" };

static const struct rts_func rts_functions[] = {
	{
		.name = "gpiofunc",
		.groups = gpiogrps,
		.num_groups = ARRAY_SIZE(gpiogrps),
	},
	{
		.name = "sdio0func",
		.groups = sdio0grps,
		.num_groups = ARRAY_SIZE(sdio0grps),
	},
	{
		.name = "sdio1func",
		.groups = sdio1grps,
		.num_groups = ARRAY_SIZE(sdio1grps),
	},
	{
		.name = "uartfunc",
		.groups = uartgrps,
		.num_groups = ARRAY_SIZE(uartgrps),
	},
	{
		.name = "pwmfunc",
		.groups = pwmgrps,
		.num_groups = ARRAY_SIZE(pwmgrps),
	},
	{
		.name = "usbdfunc",
		.groups = usbdgrps,
		.num_groups = ARRAY_SIZE(usbdgrps),
	},
	{
		.name = "etnledfunc",
		.groups = etnledgrps,
		.num_groups = ARRAY_SIZE(etnledgrps),
	},
	{
		.name = "usbhfunc",
		.groups = usbhgrps,
		.num_groups = ARRAY_SIZE(usbhgrps),
	},
	{
		.name = "saradcfunc",
		.groups = saradcgrps,
		.num_groups = ARRAY_SIZE(saradcgrps),
	},
	{
		.name = "ssifunc",
		.groups = ssigrps,
		.num_groups = ARRAY_SIZE(ssigrps),
	},
	{
		.name = "i2cfunc",
		.groups = i2cgrps,
		.num_groups = ARRAY_SIZE(i2cgrps),
	},
	{
		.name = "spifunc",
		.groups = spigrps,
		.num_groups = ARRAY_SIZE(spigrps),
	},
};

static struct rts_pinpair rts_pintable[] = {
	RTS_PINRANGE(0, 0, RTS_MAX_NGPIO),
};

#define HAIMR		  0x10
#define HAIMR_START	  BIT(31)
#define HAIMR_WRITE	  BIT(30)
#define HAIMR_START_WRITE (HAIMR_START | HAIMR_WRITE)

#define H_SD_LDO_CFG0	   0xFF52
#define SD_LDO_TUNE33	   0x71
#define H_SD_LDO_CTRL	   0xFF54
#define SD_LDO_LDO3318_POW 0x01

#define REG_RW_RETRY_CNT 3

#define rts_pinctrl_readl_sdio(addr, reg) ioread32((addr) + (reg))
#define rts_pinctrl_writel_sdio(addr, reg, value) \
	iowrite32(value, (addr) + (reg))

static int rts_pinctrl_sdio_reg_write(struct rts_pinctrl *rtspc, u16 addr,
				      u8 mask, u8 data, int index)
{
	int i;
	u32 val = HAIMR_START_WRITE |
		  ((u32)(addr & 0x3FFF) << 16 | (u32)mask << 8 | (u32)data);

	if (index == 0)
		rts_pinctrl_writel_sdio(rtspc->addr_sdio0, HAIMR, val);
	else
		rts_pinctrl_writel_sdio(rtspc->addr_sdio1, HAIMR, val);

	for (i = 0; i < REG_RW_RETRY_CNT; i++) {
		if (index == 0)
			val = rts_pinctrl_readl_sdio(rtspc->addr_sdio0, HAIMR);
		else
			val = rts_pinctrl_readl_sdio(rtspc->addr_sdio1, HAIMR);
		if (!(val & HAIMR_START)) {
			if (data != (u8)val)
				return -EIO;
			return 0;
		}
	}
	return -ETIMEDOUT;
}

static int rts_pinctrl_map_resourse(struct rts_pinctrl *rtspc, int index)
{
	int err = 0;
	struct resource *res;
	struct platform_device *pdev = rtspc->pdev;

	if (index == 0) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (res) {
			if (!request_mem_region(res->start,
						res->end - res->start + 1,
						NULL)) {
				return -ENOMEM;
			}
			rtspc->addr_sdio0 =
				ioremap(res->start, res->end - res->start + 1);
			if (IS_ERR(rtspc->addr_sdio0)) {
				err = PTR_ERR(rtspc->addr_sdio0);
				goto free_mem;
			}
		} else {
			err = -ENOMEM;
		}
	} else if (index == 1) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (res) {
			if (!request_mem_region(res->start,
						res->end - res->start + 1,
						NULL)) {
				return -ENOMEM;
			}
			rtspc->addr_sdio1 =
				ioremap(res->start, res->end - res->start + 1);
			if (IS_ERR(rtspc->addr_sdio1)) {
				err = PTR_ERR(rtspc->addr_sdio1);
				goto free_mem;
			}
		} else {
			err = -ENOMEM;
		}
	} else if (index == 2) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
		if (res) {
			if (!request_mem_region(res->start,
						res->end - res->start + 1,
						NULL)) {
				return -ENOMEM;
			}
			rtspc->addr_i2s =
				ioremap(res->start, res->end - res->start + 1);
			if (IS_ERR(rtspc->addr_i2s)) {
				err = PTR_ERR(rtspc->addr_i2s);
				goto free_mem;
			}
		} else {
			err = -ENOMEM;
		}
	}
	return err;
free_mem:
	release_mem_region(res->start, res->end - res->start + 1);
	return err;
}

static int rts_pinctrl_power_on_sdio_ldo(struct rts_pinctrl *rtspc, int index)
{
	int err;

	err = rts_pinctrl_map_resourse(rtspc, index);
	if (err < 0)
		return err;

	err = rts_pinctrl_sdio_reg_write(rtspc, H_SD_LDO_CFG0, 0xFF,
					 SD_LDO_TUNE33, index);
	if (err < 0)
		return err;
	err = rts_pinctrl_sdio_reg_write(rtspc, H_SD_LDO_CTRL, 0xFF,
					 SD_LDO_LDO3318_POW, index);

	return err;
}

#define RLX_REG_I2S_OCP_CTL (0)
#define RLX_LDOI2S_POW	    (0)

static int rts_pinctrl_power_on_i2s_ldo(struct rts_pinctrl *rtspc)
{
	int err;
	u32 value;

	err = rts_pinctrl_map_resourse(rtspc, 2);
	if (err < 0)
		return err;

	value = readl(rtspc->addr_i2s + RLX_REG_I2S_OCP_CTL);
	writel(value | ((u32)0x1 << RLX_LDOI2S_POW),
	       rtspc->addr_i2s + RLX_REG_I2S_OCP_CTL);

	return err;
}

static inline void rts_clr_reg_bit(int bit, void *offset)
{
	u32 v;

	v = ioread32(offset) & ~(1 << bit);
	iowrite32(v, offset);
}

static inline void rts_set_reg_bit(int bit, void *offset)
{
	u32 v;

	v = ioread32(offset) | (1 << bit);
	iowrite32(v, offset);
}
static struct sharepin_cfg_addr *rts_get_pinaddr(int pin)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pincfgaddr); i++)
		if (pincfgaddr[i].pinl <= pin && pincfgaddr[i].pinh >= pin)
			return &pincfgaddr[i];

	return 0;
}

static void rts_gpio_set_field(void __iomem *reg, unsigned int field,
			       unsigned int width, unsigned int offset)
{
	unsigned int val = readl(reg);

	val = RTS_SETFIELD(val, field, width, offset);
	writel(val, reg);
}

static unsigned int rts_gpio_get_field(void __iomem *reg, unsigned int width,
				       unsigned int offset)
{
	unsigned int val = readl(reg);

	return RTS_GETFIELD(val, width, offset);
}

static int rts_gpio_enable(struct gpio_chip *chip, unsigned int gpio)
{
	struct rts_pinctrl *rtspc = gpiochip_get_data(chip);
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int bf;
	static int sd0_en = 1, sd1_en = 1, i2s_en = 1;

	sc = rts_get_pinaddr(gpio);
	regs = (struct pinregs *)sc->pinaddr;
	bf = gpio - sc->pinl;

	switch (sc->pint) {
	case GPIO_TYPE_GENERIC:
		if (gpio == 0)
			rts_gpio_set_field(rtspc->addr + GPIO_0_15_PAD_CFG, 0,
					   4, 0);
		else if (gpio == 1)
			rts_gpio_set_field(rtspc->addr + GPIO_0_15_PAD_CFG, 0,
					   4, 4);
		else if (gpio == 8)
			rts_gpio_set_field(rtspc->addr + GPIO_0_15_PAD_CFG, 0,
					   4, 8);
		break;
	case GPIO_TYPE_UART0:
	case GPIO_TYPE_UART1:
	case GPIO_TYPE_UART2:
		bf = 3 - bf;
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 1, 4,
				   bf << 2);
		break;
	case GPIO_TYPE_I2S:
		if (i2s_en) {
			i2s_en = 0;
			rts_pinctrl_power_on_i2s_ldo(rtspc);
		}
		rts_gpio_set_field(rtspc->addr + (int)&regs->pad_cfg, 1, 4,
				   bf << 2);
		break;
	case GPIO_TYPE_PWM:
	case GPIO_TYPE_I2C:
	case GPIO_TYPE_USB3:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 1, 4,
				   bf << 2);
		break;
	case GPIO_TYPE_SDIO0:
		if (sd0_en) {
			sd0_en = 0;
			rts_pinctrl_power_on_sdio_ldo(rtspc, 0);
		}
		if (gpio == 38)
			rts_gpio_set_field(rtspc->addr + (int)&regs->pad_cfg, 1,
					   2, 4);
		else if (gpio == 39)
			rts_gpio_set_field(rtspc->addr + (int)&regs->pad_cfg, 1,
					   2, 2);
		else
			rts_gpio_set_field(rtspc->addr + (int)&regs->pad_cfg, 1,
					   2, 0);
		break;
	case GPIO_TYPE_SDIO1:
		if (sd1_en) {
			sd1_en = 0;
			rts_pinctrl_power_on_sdio_ldo(rtspc, 1);
		}
		if (gpio == 46)
			rts_gpio_set_field(rtspc->addr + (int)&regs->pad_cfg, 1,
					   2, 4);
		else if (gpio == 47)
			rts_gpio_set_field(rtspc->addr + (int)&regs->pad_cfg, 1,
					   2, 2);
		else
			rts_gpio_set_field(rtspc->addr + (int)&regs->pad_cfg, 1,
					   2, 0);
		break;
	case GPIO_TYPE_SARADC:
	case GPIO_TYPE_USBD:
	case GPIO_TYPE_USBH:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 2, 4,
				   bf << 2);
		break;
	case GPIO_TYPE_SSOR:
		if (bf >= 12)
			bf = 12;
		bf /= 2;
		if (bf >= 3)
			bf++;
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 1, 4,
				   bf << 2);
		break;
	case GPIO_TYPE_DMIC:
		bf /= 2;
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 1, 4,
				   bf << 2);
		break;
	case GPIO_TYPE_ADDA:
		bf /= 2;
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 2, 4,
				   bf << 2);
		break;
	case GPIO_TYPE_SSORI2C:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 1, 2,
				   0);
		break;
	case GPIO_TYPE_SPI:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pad_cfg), 1, 2,
				   0);
		break;
	default:
		dev_err(rtspc->dev, "not known function selector\n");
		break;
	}
	return 0;
}

static int rts_gpio_request(struct gpio_chip *chip, unsigned int offset)
{
	int res = pinctrl_gpio_request(chip->base + offset);

	if (!res)
		rts_gpio_enable(chip, chip->base + offset);

	return res;
}

static void rts_gpio_free(struct gpio_chip *chip, unsigned int offset)
{
	pinctrl_gpio_free(chip->base + offset);
}

static int rts_gpio_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	struct rts_pinctrl *rtspc = gpiochip_get_data(chip);
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int bf;

	sc = rts_get_pinaddr(chip->base + offset);
	regs = (struct pinregs *)sc->pinaddr;
	bf = chip->base + offset - sc->pinl;

	return !rts_gpio_get_field(rtspc->addr + (int)&(regs->gpio_oe), 1, bf);
}

static int rts_gpio_direction_input(struct gpio_chip *chip, unsigned int offset)
{
	return pinctrl_gpio_direction_input(chip->base + offset);
}

static int rts_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct rts_pinctrl *rtspc = gpiochip_get_data(chip);
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int bf;

	sc = rts_get_pinaddr(chip->base + offset);
	regs = (struct pinregs *)sc->pinaddr;
	bf = chip->base + offset - sc->pinl;

	if (sc->pinl == 65 &&
	    rts_gpio_get_field(rtspc->addr + (int)&(regs->gpio_oe), 1, bf))
		return (rtspc->audio_adda_gpio_value >> bf) & 0x1;
	if (sc->pinl == 78 &&
	    rts_gpio_get_field(rtspc->addr + (int)&(regs->gpio_oe), 1, bf))
		return (rtspc->usb0_gpio_value >> bf) & 0x1;
	if (sc->pinl == 80 &&
	    rts_gpio_get_field(rtspc->addr + (int)&(regs->gpio_oe), 1, bf))
		return (rtspc->usb1_gpio_value >> bf) & 0x1;
	return rts_gpio_get_field(rtspc->addr + (int)&(regs->gpio_value), 1,
				  bf);
}

static void rts_gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct rts_pinctrl *rtspc = gpiochip_get_data(chip);
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int bf;

	sc = rts_get_pinaddr(chip->base + offset);
	regs = (struct pinregs *)sc->pinaddr;
	bf = chip->base + offset - sc->pinl;

	if (sc->pinl == 65) {
		if (value) {
			rtspc->audio_adda_gpio_value |= (1 << bf);
			writel(rtspc->audio_adda_gpio_value,
			       rtspc->addr + (int)&(regs->gpio_value));
		} else {
			rtspc->audio_adda_gpio_value &= (~(1 << bf));
			writel(rtspc->audio_adda_gpio_value,
			       rtspc->addr + (int)&(regs->gpio_value));
		}
	} else if (sc->pinl == 78) {
		if (value) {
			rtspc->usb0_gpio_value |= (1 << bf);
			writel(rtspc->usb0_gpio_value,
			       rtspc->addr + (int)&(regs->gpio_value));
		} else {
			rtspc->usb0_gpio_value &= (~(1 << bf));
			writel(rtspc->usb0_gpio_value,
			       rtspc->addr + (int)&(regs->gpio_value));
		}
	} else if (sc->pinl == 80) {
		if (value) {
			rtspc->usb1_gpio_value |= (1 << bf);
			writel(rtspc->usb1_gpio_value,
			       rtspc->addr + (int)&(regs->gpio_value));
		} else {
			rtspc->usb1_gpio_value &= (~(1 << bf));
			writel(rtspc->usb1_gpio_value,
			       rtspc->addr + (int)&(regs->gpio_value));
		}
	} else {
		if (value)
			rts_set_reg_bit(bf,
					rtspc->addr + (int)&(regs->gpio_value));
		else
			rts_clr_reg_bit(bf,
					rtspc->addr + (int)&(regs->gpio_value));
	}
}

static int rts_gpio_direction_output(struct gpio_chip *chip,
				     unsigned int offset, int value)
{
	rts_gpio_set(chip, offset, value);
	return pinctrl_gpio_direction_output(chip->base + offset);
}

static int rts_gpio_to_irq(struct gpio_chip *chip, unsigned int offset)
{
	struct rts_pinctrl *rtspc = gpiochip_get_data(chip);

	return irq_linear_revmap(rtspc->irq_domain, offset);
}

static struct gpio_chip rts_gpio_chip = {
	.label = REALTEK_MC_PINCTRL_NAME,
	.owner = THIS_MODULE,
	.request = rts_gpio_request,
	.free = rts_gpio_free,
	.get_direction = rts_gpio_get_direction,
	.direction_input = rts_gpio_direction_input,
	.direction_output = rts_gpio_direction_output,
	.get = rts_gpio_get,
	.set = rts_gpio_set,
	.to_irq = rts_gpio_to_irq,
	.base = 0,
	.ngpio = RTS_MAX_NGPIO,
	.can_sleep = 0,
};

static int rts_rtspctl_get_groups_count(struct pinctrl_dev *rtspctldev)
{
	return ARRAY_SIZE(rts_pin_groups);
}

static const char *rts_rtspctl_get_group_name(struct pinctrl_dev *rtspctldev,
					      unsigned int selector)
{
	return rts_pin_groups[selector].name;
}

static int rts_rtspctl_get_group_pins(struct pinctrl_dev *rtspctldev,
				      unsigned int selector,
				      const unsigned int **pins,
				      unsigned int *num_pins)
{
	*pins = rts_pin_groups[selector].pins;
	*num_pins = rts_pin_groups[selector].num_pins;

	return 0;
}

static void rts_rtspctl_pin_dbg_show(struct pinctrl_dev *rtspctldev,
				     struct seq_file *s, unsigned int offset)
{
}

static const struct pinctrl_ops rts_rtspctl_ops = {
	.get_groups_count = rts_rtspctl_get_groups_count,
	.get_group_name = rts_rtspctl_get_group_name,
	.get_group_pins = rts_rtspctl_get_group_pins,
	.pin_dbg_show = rts_rtspctl_pin_dbg_show,
#ifdef CONFIG_OF
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinctrl_utils_free_map,
#endif

};

static int rts_pmx_get_functions_count(struct pinctrl_dev *rtspctldev)
{
	return ARRAY_SIZE(rts_functions);
}

static const char *rts_pmx_get_function_name(struct pinctrl_dev *rtspctldev,
					     unsigned int selector)
{
	return rts_functions[selector].name;
}

static int rts_pmx_get_function_groups(struct pinctrl_dev *rtspctldev,
				       unsigned int selector,
				       const char *const **groups,
				       unsigned int *const num_groups)
{
	/* every pin can do every function */
	*groups = rts_functions[selector].groups;
	*num_groups = rts_functions[selector].num_groups;

	return 0;
}

static int rts_pmx_enable(struct pinctrl_dev *rtspctldev,
			  unsigned int func_selector,
			  unsigned int group_selector)
{
	struct rts_pinctrl *rtspc = pinctrl_dev_get_drvdata(rtspctldev);

	switch (func_selector) {
	case I2C_FUNC_SELECT:
		if (group_selector == I2C0_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + XB2_I2C_PAD_CFG, 2, 2,
					   0);
			rts_gpio_set_field(rtspc->addr + XB2_I2C_PAD_CFG, 2, 2,
					   4);
		} else if (group_selector == I2CPWM_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 8, 4, 0);
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 8, 4, 4);
		} else if (group_selector == I2C1_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + SSOR_I2C_PAD_CFG, 2, 2,
					   0);
		}
		break;
	case PWM_FUNC_SELECT:
		if (group_selector == PWM0_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 2, 4, 0);
		else if (group_selector == PWM1_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 2, 4, 4);
		else if (group_selector == PWM2_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 2, 4, 8);
		else if (group_selector == PWM3_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 2, 4, 12);
		else if (group_selector == PWMSD_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SD1_PAD_CFG, 4, 4, 0);
		break;
	case UART_FUNC_SELECT:
		if (group_selector == UART0_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 2, 4,
					   0);
			rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 2, 4,
					   4);
			rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 2, 4,
					   8);
			rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 2, 4,
					   12);
		} else if (group_selector == UART1_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + UART1_PAD_CFG, 2, 4,
					   8);
			rts_gpio_set_field(rtspc->addr + UART1_PAD_CFG, 2, 4,
					   12);
		} else if (group_selector == UART2_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + UART2_PAD_CFG, 2, 4,
					   0);
			rts_gpio_set_field(rtspc->addr + UART2_PAD_CFG, 2, 4,
					   4);
			rts_gpio_set_field(rtspc->addr + UART2_PAD_CFG, 2, 4,
					   8);
			rts_gpio_set_field(rtspc->addr + UART2_PAD_CFG, 2, 4,
					   12);
		} else if (group_selector == UART2_USB_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + USB1_PAD_CFG, 4, 4, 0);
			rts_gpio_set_field(rtspc->addr + USB1_PAD_CFG, 4, 4, 4);
		}
		break;
	case USBD_FUNC_SELECT:
		rts_gpio_set_field(rtspc->addr + USB1_PAD_CFG, 1, 3, 4);
		rts_gpio_set_field(rtspc->addr + USB1_PAD_CFG, 1, 3, 0);
		rts_gpio_set_field(rtspc->addr + USB2_PAD_CFG, 2, 2, 8);
		break;
	case ETNLED_FUNC_SELECT:
		if (group_selector == ETNLED0_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 4, 4, 0);
		else if (group_selector == ETNLED1_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 4, 4, 4);
		else if (group_selector == ETNLED2_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + PWM_PAD_CFG, 4, 3, 8);
		break;
	case USBHST_FUNC_SELECT:
		rts_gpio_set_field(rtspc->addr + USB0_PAD_CFG, 1, 2, 4);
		rts_gpio_set_field(rtspc->addr + USB0_PAD_CFG, 1, 2, 0);
		rts_gpio_set_field(rtspc->addr + USB2_PAD_CFG, 2, 2, 4);
		rts_gpio_set_field(rtspc->addr + USB2_PAD_CFG, 2, 2, 0);
		break;
	case SARADC_FUNC_SELECT:
		if (group_selector == SARADC0_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SARADC_PAD_CFG, 1, 2,
					   0);
		else if (group_selector == SARADC1_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SARADC_PAD_CFG, 1, 2,
					   4);
		else if (group_selector == SARADC2_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SARADC_PAD_CFG, 1, 2,
					   8);
		else
			rts_gpio_set_field(rtspc->addr + SARADC_PAD_CFG, 1, 2,
					   12);
		break;
	case SSI_FUNC_SELECT:
		rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 4, 3, 0);
		rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 4, 3, 4);
		rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 4, 3, 8);
		rts_gpio_set_field(rtspc->addr + UART0_PAD_CFG, 4, 3, 12);
		break;
	case SDIO0_FUNC_SELECT:
		rts_gpio_set_field(rtspc->addr + SD0_PAD_CFG, 2, 2, 0);
		if (group_selector == SDIO0WP_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SD0_PAD_CFG, 2, 2, 4);
		else if (group_selector == SDIO0CD_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SD0_PAD_CFG, 2, 2, 2);
		break;
	case SDIO1_FUNC_SELECT:
		if (group_selector == SDIO1_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + SD1_PAD_CFG, 2, 4, 0);
			rts_gpio_set_field(rtspc->addr + SD1_PAD_CFG, 0, 1, 12);
		} else if (group_selector == SDIO1WP_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SD1_PAD_CFG, 2, 4, 8);
		else if (group_selector == SDIO1CD_GROUP_SELECT)
			rts_gpio_set_field(rtspc->addr + SD1_PAD_CFG, 2, 4, 4);
		else if (group_selector == SDIO1VIDEO_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + GPIO_0_15_PAD_CFG, 4,
					   4, 8);
			rts_gpio_set_field(rtspc->addr + SD1_PAD_CFG, 1, 1, 12);
			rts_gpio_set_field(rtspc->addr + VIDEO_PAD_CFG, 4, 4,
					   0);
			rts_gpio_set_field(rtspc->addr + VIDEO_PAD_CFG, 4, 4,
					   24);
			rts_gpio_set_field(rtspc->addr + VIDEO_PAD_CFG, 4, 4,
					   28);
			rts_gpio_set_field(rtspc->addr + PAD_V18_EN, 3, 2, 0);
		}
		break;
	case SPI_FUNC_SELECT:
		if (group_selector == DEFAULT_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + SPI_PAD_CFG, 1, 4, 0);
			rts_gpio_set_field(rtspc->addr + SPI_PAD_CFG, 1, 4, 4);
		} else if (group_selector == SPI_GROUP_SELECT) {
			rts_gpio_set_field(rtspc->addr + SPI_PAD_CFG, 2, 4, 0);
			rts_gpio_set_field(rtspc->addr + SPI_PAD_CFG, 2, 4, 4);
		}
		break;
	default:
		dev_err(rtspc->dev, "not known function selector %d\n",
			func_selector);
		break;
	}

	return 0;
}

static int rts_gpio_config_set(struct rts_pinctrl *rtspc, unsigned int pin,
			       unsigned int config, unsigned int value)
{
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int bf;

	sc = rts_get_pinaddr(pin);
	regs = (struct pinregs *)sc->pinaddr;
	bf = pin - sc->pinl;

	switch (config) {
	case RTS_PINCONFIG_PULL_NONE:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pullctrl), 0, 2,
				   bf << 1);
		break;
	case RTS_PINCONFIG_PULL_DOWN:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pullctrl), 1, 2,
				   bf << 1);
		break;
	case RTS_PINCONFIG_PULL_UP:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->pullctrl), 2, 2,
				   bf << 1);
		break;
	case RTS_PIN_CONFIG_DRIVE_STRENGTH:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->drv_sel),
				   (value >> 2) - 1, 1, bf);
		break;
	case RTS_PIN_CONFIG_SLEW_RATE:
		rts_gpio_set_field(rtspc->addr + (int)&(regs->sr_ctrl), value,
				   1, bf);
		break;
	case RTS_PINCONFIG_INPUT:
		if (pin < RTS_MAX_NGPIO) {
			rts_gpio_set_field(rtspc->addr + (int)&(regs->gpio_oe),
					   0, 1, bf);
		}
		break;
	case RTS_PINCONFIG_OUTPUT:
		if (pin < RTS_MAX_NGPIO) {
			rts_gpio_set_field(rtspc->addr + (int)&(regs->gpio_oe),
					   1, 1, bf);
		}
		break;
	default:
		dev_err(rtspc->dev, "illegal configuration requested\n");
		return -EINVAL;
	}

	return 0;
}

static int rts_pmx_gpio_set_direction(struct pinctrl_dev *rtspctldev,
				      struct pinctrl_gpio_range *range,
				      unsigned int offset, bool input)
{
	int ret;
	struct rts_pinctrl *rtspc = pinctrl_dev_get_drvdata(rtspctldev);

	int config = input ? RTS_PINCONFIG_INPUT : RTS_PINCONFIG_OUTPUT;

	ret = rts_gpio_config_set(rtspc, offset, config, 0);

	return 0;
}

static int rts_pmx_request(struct pinctrl_dev *rtspctldev, unsigned int offset)
{
	int i, j;
	struct rts_pinctrl *rtspc = pinctrl_dev_get_drvdata(rtspctldev);
	u64 bs;

	if (offset > 119)
		return -EINVAL;

	i = offset > 63;
	j = i ? (offset - 64) : offset;
	bs = 1;
	bs <<= j;

	if (rtspc->pinsmask[i] & bs)
		return -EBUSY;

	rtspc->pinsmask[i] |= bs;
	return 0;
}

static int rts_pmx_free(struct pinctrl_dev *rtspctldev, unsigned int offset)
{
	int i, j;
	struct rts_pinctrl *rtspc = pinctrl_dev_get_drvdata(rtspctldev);
	u64 bs;

	if (offset > 119)
		return -EINVAL;

	i = offset > 63;
	j = i ? (offset - 64) : offset;
	bs = 1;
	bs <<= j;

	if (rtspc->pinsmask[i] & bs) {
		rtspc->pinsmask[i] &= ~bs;
		return 0;
	} else {
		return -EINVAL;
	}
}

static const struct pinmux_ops rts_pmx_ops = {
	.request = rts_pmx_request,
	.free = rts_pmx_free,
	.get_functions_count = rts_pmx_get_functions_count,
	.get_function_name = rts_pmx_get_function_name,
	.get_function_groups = rts_pmx_get_function_groups,
	.set_mux = rts_pmx_enable,
	.gpio_set_direction = rts_pmx_gpio_set_direction,
};

static int rts_pin_config_set(struct rts_pinctrl *rtspc, unsigned int pin,
			      unsigned int param)
{
	int ret;
	unsigned int config = pinconf_to_config_param(param);
	unsigned int value = pinconf_to_config_argument(param);

	ret = rts_gpio_config_set(rtspc, pin, config, value);

	return ret;
}

static int rts_pinconf_get(struct pinctrl_dev *rtspctldev, unsigned int pin,
			   unsigned long *config)
{
	return -ENOTSUPP;
}

static int rts_pinconf_set(struct pinctrl_dev *rtspctldev, unsigned int pin,
			   unsigned long *configs, unsigned int num_configs)
{
	int ret, i;
	struct rts_pinctrl *rtspc;

	rtspc = pinctrl_dev_get_drvdata(rtspctldev);

	for (i = 0; i < num_configs; i++) {
		ret = rts_pin_config_set(rtspc, pin, configs[i]);
		if (ret)
			return ret;
	}

	return 0;
}

/* set the pin config settings for a specified pin group */
static int rts_pinconf_group_set(struct pinctrl_dev *pctldev,
				 unsigned int group, unsigned long *configs,
				 unsigned int num_configs)
{
	const unsigned int *pins;
	unsigned int cnt;

	pins = rts_pin_groups[group].pins;
	for (cnt = 0; cnt < rts_pin_groups[group].num_pins; cnt++)
		rts_pinconf_set(pctldev, pins[cnt], configs, num_configs);

	return 0;
}

static const struct pinconf_ops rts_pinconf_ops = {
	.pin_config_get = rts_pinconf_get,
	.pin_config_set = rts_pinconf_set,
	.pin_config_group_set = rts_pinconf_group_set,
};

static struct pinctrl_desc rts_pinctrl_desc = {
	.name = REALTEK_MC_PINCTRL_NAME,
	.pins = rts_gpio_pins,
	.npins = ARRAY_SIZE(rts_gpio_pins),
	.pctlops = &rts_rtspctl_ops,
	.pmxops = &rts_pmx_ops,
	.confops = &rts_pinconf_ops,
	.owner = THIS_MODULE,
};

static void rts_gpio_irq_enable(struct irq_data *data)
{
	struct rts_pinctrl *rtspc = irq_data_get_irq_chip_data(data);
	unsigned char gpio = (unsigned char)irqd_to_hwirq(data);
	unsigned long flags;
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int bf, bs;

	sc = rts_get_pinaddr(gpio);
	regs = (struct pinregs *)sc->pinaddr;
	bf = gpio - sc->pinl;

	bs = sc->pinh - sc->pinl + 1;
	if (bs > 8)
		bs = 16;
	else if (bs > 4)
		bs = 8;
	else
		bs = 4;

	spin_lock_irqsave(&rtspc->irq_lock, flags);
	if (gpio < RTS_MAX_NGPIO) {
		if (rtspc->irq_type[gpio] == IRQ_TYPE_EDGE_RISING ||
		    rtspc->irq_type[gpio] == IRQ_TYPE_EDGE_BOTH)
			rts_set_reg_bit(bf + bs,
					rtspc->addr +
						(int)&(regs->gpio_int_en));

		if (rtspc->irq_type[gpio] == IRQ_TYPE_EDGE_FALLING ||
		    rtspc->irq_type[gpio] == IRQ_TYPE_EDGE_BOTH)
			rts_set_reg_bit(bf, rtspc->addr +
						    (int)&(regs->gpio_int_en));
	}
	spin_unlock_irqrestore(&rtspc->irq_lock, flags);
}

static void rts_gpio_irq_disable(struct irq_data *data)
{
	struct rts_pinctrl *rtspc = irq_data_get_irq_chip_data(data);
	unsigned char gpio = (unsigned char)irqd_to_hwirq(data);
	unsigned long flags;
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int bf, bs;

	sc = rts_get_pinaddr(gpio);

	bs = sc->pinh - sc->pinl + 1;
	if (bs > 8)
		bs = 16;
	else if (bs > 4)
		bs = 8;
	else
		bs = 4;

	regs = (struct pinregs *)sc->pinaddr;
	bf = gpio - sc->pinl;

	spin_lock_irqsave(&rtspc->irq_lock, flags);
	if (gpio < RTS_MAX_NGPIO) {
		rts_clr_reg_bit(gpio, rtspc->addr + (int)&(regs->gpio_int_en));
		rts_clr_reg_bit(gpio + bs,
				rtspc->addr + (int)&(regs->gpio_int_en));
	}
	spin_unlock_irqrestore(&rtspc->irq_lock, flags);
}

static int rts_gpio_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct rts_pinctrl *rtspc = irq_data_get_irq_chip_data(data);
	unsigned char gpio = (unsigned char)irqd_to_hwirq(data);

	switch (type) {
	case IRQ_TYPE_NONE:
	case IRQ_TYPE_EDGE_RISING:
	case IRQ_TYPE_EDGE_FALLING:
	case IRQ_TYPE_EDGE_BOTH:
		rtspc->irq_type[gpio] = type;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int rts_gpio_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct rts_pinctrl *rtspc = irq_data_get_irq_chip_data(data);
	unsigned long flags;

	spin_lock_irqsave(&rtspc->irq_lock, flags);

	if (on)
		rts_gpio_irq_enable(data);
	else
		rts_gpio_irq_disable(data);

	spin_unlock_irqrestore(&rtspc->irq_lock, flags);

	return 0;
}

#else /* CONFIG_PM_SLEEP */

#define rts_gpio_irq_set_wake NULL

#endif /* CONFIG_PM_SLEEP */

static struct irq_chip rts_gpio_irq_chip = {
	.name = REALTEK_MC_PINCTRL_NAME,
	.irq_enable = rts_gpio_irq_enable,
	.irq_disable = rts_gpio_irq_disable,
	.irq_set_type = rts_gpio_irq_set_type,
	.irq_set_wake = rts_gpio_irq_set_wake,
};

static irqreturn_t rts_irq_handler(int irq, void *pc)
{
	unsigned long offset;
	struct rts_pinctrl *rtspc = (struct rts_pinctrl *)pc;
	unsigned long val1 = 0, val2 = 0;
	int irqno = 0;
	int i;
	int mask;
	int bs;
	struct pinregs *regs;
	struct sharepin_cfg_addr *sc;
	int handled = 0;

	for (i = 0; i < ARRAY_SIZE(pincfgaddr); i++) {
		sc = &pincfgaddr[i];
		sc = rts_get_pinaddr(sc->pinl);
		regs = (struct pinregs *)sc->pinaddr;

		val1 = readl(rtspc->addr + (int)&(regs->gpio_int_en));
		val2 = readl(rtspc->addr + (int)&(regs->gpio_int));

		bs = sc->pinh - sc->pinl + 1;
		if (bs > 8)
			bs = 16;
		else if (bs > 4)
			bs = 8;
		else
			bs = 4;

		mask = GENMASK(sc->pinh - sc->pinl, 0);
		mask = (mask << bs) | mask;

		val1 &= mask;
		val2 &= mask;

		writel(val2, rtspc->addr + (int)&(regs->gpio_int));

		val2 &= val1;

		for_each_set_bit(offset, (const unsigned long *)&val2, 32) {
			irqno = offset;
			if (irqno >= bs)
				irqno -= bs;
			irqno += sc->pinl;
			irqno = irq_linear_revmap(rtspc->irq_domain, irqno);
			if (irqno) {
				handled = 1;
				generic_handle_irq(irqno);
			}
		}
	}

	if (handled == 1)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static const struct of_device_id rts_pinctrl_match[] = {
	{
		.compatible = "realtek,rts493xa-pinctrl",
		.data = (void *)(TYPE_RTS493XA),
	},
	{}
};

MODULE_DEVICE_TABLE(of, rts_pinctrl_match);

static int rts_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rts_pinctrl *rtspc;
	struct resource *res;
	int err, i, len;
	struct rts_pinpair *pp;
	u32 ngpios;
	u32 mapping[4], map;

	const struct of_device_id *of_id;

	of_id = of_match_device(rts_pinctrl_match, &pdev->dev);

	rtspc = devm_kzalloc(dev, sizeof(*rtspc), GFP_KERNEL);
	if (!rtspc)
		return -ENOMEM;
	rtspc->pdev = pdev;

	err = of_property_read_u32(dev->of_node, "ngpios", &ngpios);
	if (err < 0) {
		dev_warn(dev, "failed to get gpio numbers:%d\n", err);
		ngpios = RTS_MAX_NGPIO;
	} else {
		if (ngpios > RTS_MAX_NGPIO)
			ngpios = RTS_MAX_NGPIO;
	}

	len = ngpios * sizeof(int);
	rtspc->irq_type = kzalloc(len, GFP_KERNEL);
	if (rtspc->irq_type == NULL) {
		err = -ENOMEM;
		goto free_rtspc;
	}

	rtspc->devt = RTS_SOC_HW_ID(of_id->data);
	platform_set_drvdata(pdev, rtspc);
	rtspc->dev = dev;

	spin_lock_init(&rtspc->irq_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOENT;
		goto free_irqtype;
	}

	rtspc->addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(rtspc->addr)) {
		err = PTR_ERR(rtspc->addr);
		goto free_irqtype;
	}

	err = of_property_read_u32_array(pdev->dev.of_node,
					 "pwm-etnled-mapping", mapping,
					 ARRAY_SIZE(mapping));
	if (err == 0) {
		map = mapping[0] | (mapping[1] << 2) | (mapping[2] << 4) |
		      (mapping[3] << 6);
		writel(map, rtspc->addr + PWM_LED_SEL);
	}

	rtspc->irq = platform_get_irq(pdev, 0);
	if (rtspc->irq < 0) {
		dev_err(dev, "irqs not supported\n");
		err = rtspc->irq;
		goto unmap_res;
	}

	rtspc->irq_domain = irq_domain_add_linear(NULL, ngpios,
						  &irq_domain_simple_ops, NULL);
	if (!rtspc->irq_domain) {
		dev_err(dev, "could not create IRQ domain\n");
		err = -ENOMEM;
		goto unmap_res;
	}

	for (i = 0; i < ngpios; i++) {
		int gpioirq = irq_create_mapping(rtspc->irq_domain, i);

		irq_set_lockdep_class(gpioirq, &gpio_lock_class,
				      &gpio_request_class);
		irq_set_chip_and_handler(gpioirq, &rts_gpio_irq_chip,
					 handle_simple_irq);
		irq_set_chip_data(gpioirq, rtspc);
	}

	err = request_irq(rtspc->irq, rts_irq_handler, IRQF_SHARED, "gpio",
			  (void *)rtspc);
	if (err) {
		dev_err(dev, "failure requesting irq %i\n", rtspc->irq);
		goto unmap_res;
	}
	dev_info(&pdev->dev, "rtspc registered with IRQs\n");

	rts_gpio_chip.ngpio = ngpios;
	rtspc->gpio_chip = &rts_gpio_chip;
	rtspc->gpio_chip->label = dev_name(dev);
	rtspc->gpio_chip->parent = dev;

	err = gpiochip_add_data(rtspc->gpio_chip, rtspc);
	if (err) {
		dev_err(dev, "failed to add gpiochip\n");
		goto unmap_res;
	}

	rtspc->rtspctl_dev = pinctrl_register(&rts_pinctrl_desc, dev, rtspc);
	if (!rtspc->rtspctl_dev)
		goto remove_gpiochip;

	pp = rts_pintable;
	len = ARRAY_SIZE(rts_pintable);

	for (i = 0; i < len; i++) {
		err = gpiochip_add_pin_range(rtspc->gpio_chip, pdev->name,
					     pp[i].gpio_base, pp[i].pin_base,
					     pp[i].pins);
		if (err)
			goto remove_gpiochip;
	}

	device_init_wakeup(dev, 1);

	return 0;

remove_gpiochip:
	dev_info(&pdev->dev, "Remove gpiochip\n");
	gpiochip_remove(rtspc->gpio_chip);
unmap_res:
	devm_iounmap(dev, rtspc->addr);
free_irqtype:
	kfree(rtspc->irq_type);
free_rtspc:
	devm_kfree(dev, rtspc);

	return err;
}

static int rts_pinctrl_remove(struct platform_device *pdev)
{
	struct rts_pinctrl *rtspc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	device_init_wakeup(dev, 0);
	free_irq(rtspc->irq, rtspc);

	pinctrl_unregister(rtspc->rtspctl_dev);

	gpiochip_remove(rtspc->gpio_chip);
	devm_iounmap(dev, rtspc->addr);
	kfree(rtspc->irq_type);
	devm_kfree(dev, rtspc);

	return 0;
}

#ifdef CONFIG_PM

static int rts_pinctrl_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct rts_pinctrl *rtspc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	if (device_may_wakeup(dev))
		enable_irq_wake(rtspc->irq);

	return 0;
}

static int rts_pinctrl_resume(struct platform_device *pdev)
{
	return 0;
}

#else /* CONFIG_PM */

#define rts_pinctrl_suspend NULL
#define rts_pinctrl_resume  NULL

#endif /* CONFIG_PM */

static struct platform_driver rts_pinctrl_driver = {
	.probe = rts_pinctrl_probe,
	.remove = rts_pinctrl_remove,
	.suspend = rts_pinctrl_suspend,
	.resume = rts_pinctrl_resume,
	.driver = {
		   .name = REALTEK_MC_PINCTRL_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = rts_pinctrl_match,
		   },
};

static int __init rts_pinctrl_init(void)
{
	return platform_driver_register(&rts_pinctrl_driver);
}

postcore_initcall(rts_pinctrl_init);

static void __exit rts_pinctrl_exit(void)
{
	platform_driver_unregister(&rts_pinctrl_driver);
}

module_exit(rts_pinctrl_exit);
