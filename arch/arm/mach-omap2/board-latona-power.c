/* arch/arm/mach-omap2/board-latona-power.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 * Copyright (C) 2013 - Dheeraj CVR (cvr.dheeraj@gmail.com)
 *
 * Based on mach-omap2/board-tuna-power.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/twl4030-madc.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/max17040_battery.h>
#include <linux/moduleparam.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <plat/cpu.h>
#include <plat/omap-pm.h>

#include <mach/board-latona.h>
#include "pm.h"

#define ADC_NUM_SAMPLES			5
#define ADC_LIMIT_ERR_COUNT		5
#define TEMP_ADC_CHANNEL		0

#define HIGH_BLOCK_TEMP_LATONA		650
#define HIGH_RECOVER_TEMP_LATONA	430
#define LOW_BLOCK_TEMP_LATONA		(-30)
#define LOW_RECOVER_TEMP_LATONA		0

/**
** temp_adc_table_data
** @adc_value : thermistor adc value
** @temperature : temperature(C) * 10
**/
struct temp_adc_table_data {
	int adc_value;
	int temperature;
};

static DEFINE_SPINLOCK(charge_en_lock);
static int charger_state;
static bool is_charging_mode;

static struct temp_adc_table_data temper_table_latona[] = {
	/* ADC, Temperature (C/10) */
	{69, 700},
	{70, 690},
	{72, 680},
	{75, 670},
	{78, 660},
	{81, 650},
	{83, 640},
	{86, 630},
	{89, 620},
	{92, 610},
	{95, 600},
	{99, 590},
	{103, 580},
	{107, 570},
	{111, 560},
	{115, 550},
	{119, 540},
	{123, 530},
	{127, 520},
	{131, 510},
	{135, 500},
	{139, 490},
	{144, 480},
	{149, 470},
	{154, 460},
	{159, 450},
	{164, 440},
	{169, 430},
	{176, 420},
	{182, 410},
	{189, 400},
	{196, 390},
	{204, 380},
	{211, 370},
	{218, 360},
	{225, 350},
	{233, 340},
	{240, 330},
	{247, 320},
	{254, 310},
	{262, 300},
	{271, 290},
	{280, 280},
	{290, 270},
	{299, 260},
	{309, 250},
	{318, 240},
	{327, 230},
	{337, 220},
	{346, 210},
	{356, 200},
	{367, 190},
	{379, 180},
	{390, 170},
	{402, 160},
	{414, 150},
	{425, 140},
	{437, 130},
	{449, 120},
	{460, 110},
	{472, 100},
	{484, 90},
	{496, 80},
	{509, 70},
	{521, 60},
	{533, 50},
	{545, 40},
	{557, 30},
	{570, 20},
	{582, 10},
	{594, 0},
	{600, (-10)},
	{607, (-20)},
	{613, (-30)},
	{621, (-40)},
	{629, (-50)},
	{637, (-60)},
	{646, (-70)},
	{654, (-80)},
	{662, (-90)},
	{670, (-100)},
};

static struct temp_adc_table_data *temper_table = temper_table_latona;
static int temper_table_size = ARRAY_SIZE(temper_table_latona);

static bool enable_sr = true;
module_param(enable_sr, bool, S_IRUSR | S_IRGRP | S_IROTH);

enum {
	GPIO_CHG_ING_N = 0,
	GPIO_TA_NCONNECTED,
	GPIO_CHG_EN
};

static struct gpio charger_gpios[] = {
	[GPIO_CHG_ING_N] = {
		.flags = GPIOF_IN,
		.label = "CHG_ING_N",
		.gpio = OMAP_GPIO_CHG_ING_N
	},
	[GPIO_TA_NCONNECTED] = {
		.flags = GPIOF_IN,
		.label = "TA_nCONNECTED",
		.gpio = OMAP_GPIO_TA_NCONNECTED
	},
	[GPIO_CHG_EN] = {
		.flags = GPIOF_OUT_INIT_HIGH,
		.label = "CHG_EN",
		.gpio = OMAP_GPIO_CHG_EN
	},
};

static int twl4030_get_adc_data(int ch)
{
	int adc_data;
	int adc_max = -1;
	int adc_min = 1 << 11;
	int adc_total = 0;
	int i, j;

	for (i = 0; i < ADC_NUM_SAMPLES; i++) {
		adc_data = twl4030_get_madc_conversion(ch);
		if (adc_data == -EAGAIN) {
			for (j = 0; j < ADC_LIMIT_ERR_COUNT; j++) {
				msleep(20);
				adc_data = twl4030_get_madc_conversion(ch);
				if (adc_data > 0)
					break;
			}
			if (j >= ADC_LIMIT_ERR_COUNT) {
				pr_err("%s: Retry count exceeded[ch:%d]\n",
					__func__, ch);
				return adc_data;
			}
		} else if (adc_data < 0) {
			pr_err("%s: Failed read adc value : %d [ch:%d]\n",
				__func__, adc_data, ch);
			return adc_data;
		}

		if (adc_data > adc_max)
			adc_max = adc_data;
		if (adc_data < adc_min)
			adc_min = adc_data;

		adc_total += adc_data;
	}
	return (adc_total - adc_max - adc_min) / (ADC_NUM_SAMPLES - 2);
}

static int temp_adc_value(void)
{
	return twl4030_get_adc_data(TEMP_ADC_CHANNEL);
}

static bool check_charge_full(void)
{
	// HIGH : BATT_FULL || LOW : CHARGING / DISCHARGING
	return gpio_get_value(charger_gpios[GPIO_CHG_ING_N].gpio);
}


static int get_bat_temp_by_adc(int *batt_temp)
{
	int array_size = temper_table_size;
	int temp_adc = temp_adc_value();
	int mid;
	int left_side = 0;
	int right_side = array_size - 1;
	int temp = 0;

	if (temp_adc < 0) {
		pr_err("%s : Invalid temperature adc value [%d]\n",
			__func__, temp_adc);
		return temp_adc;
	}

	while (left_side <= right_side) {
		mid = (left_side + right_side) / 2;
		if (mid == 0 || mid == array_size - 1 ||
				(temper_table[mid].adc_value <= temp_adc &&
				 temper_table[mid+1].adc_value > temp_adc)) {
			temp = temper_table[mid].temperature;
			break;
		} else if (temp_adc - temper_table[mid].adc_value > 0) {
			left_side = mid + 1;
		} else {
			right_side = mid - 1;
		}
	}

	pr_debug("%s: temp adc : %d, temp : %d\n", __func__, temp_adc, temp);
	*batt_temp = temp;
	return 0;
}

static int charger_init(struct device *dev)
{
	return gpio_request_array(charger_gpios, ARRAY_SIZE(charger_gpios));
}

static void charger_exit(struct device *dev)
{
	gpio_free_array(charger_gpios, ARRAY_SIZE(charger_gpios));
}

static void set_charge_en(int state)
{
	gpio_set_value(charger_gpios[GPIO_CHG_EN].gpio, !state);
}

static void charger_set_charge(int state)
{
	unsigned long flags;

	spin_lock_irqsave(&charge_en_lock, flags);
	charger_state = state;
	set_charge_en(state);
	spin_unlock_irqrestore(&charge_en_lock, flags);
}

static void charger_set_only_charge(int state)
{
	unsigned long flags;

	spin_lock_irqsave(&charge_en_lock, flags);
	if (charger_state)
		set_charge_en(state);
	spin_unlock_irqrestore(&charge_en_lock, flags);
	/* CHG_ING_N level changed after set charge_en and 150ms */
	msleep(150);
}

static int charger_is_online(void)
{
	return !gpio_get_value(charger_gpios[GPIO_TA_NCONNECTED].gpio);
}

static int charger_is_charging(void)
{
	return !gpio_get_value(charger_gpios[GPIO_CHG_EN].gpio);
}

static char *latona_charger_supplied_to[] = {
	"battery",
};

static const __initdata struct pda_power_pdata charger_pdata = {
	.init = charger_init,
	.exit = charger_exit,
	.set_charge = charger_set_charge,
	.wait_for_status = 500,
	.wait_for_charger = 500,
	.supplied_to = latona_charger_supplied_to,
	.num_supplicants = ARRAY_SIZE(latona_charger_supplied_to),
	.use_otg_notifier = true,
};

static struct max17040_platform_data max17040_pdata = {
	.charger_online = charger_is_online,
	.charger_enable = charger_is_charging,
	.allow_charging = charger_set_only_charge,
	.skip_reset = true,
	.min_capacity = 3,
	.is_full_charge = check_charge_full,
	.get_bat_temp = get_bat_temp_by_adc,
	.high_block_temp = HIGH_BLOCK_TEMP_LATONA,
	.high_recover_temp = HIGH_RECOVER_TEMP_LATONA,
	.low_block_temp = LOW_BLOCK_TEMP_LATONA,
	.low_recover_temp = LOW_RECOVER_TEMP_LATONA,
	.fully_charged_vol = 419000,
	.recharge_vol = 418000,
	.limit_charging_time = 21600,  /* 6 hours */
	.limit_recharging_time = 5400, /* 90 min */
};

static int __init latona_charger_mode_setup(char *str)
{
	if (!str)		/* No mode string */
		return 0;

	is_charging_mode = !strcmp(str, "charger");

	pr_debug("Charge mode string = \"%s\" charger mode = %d\n", str,
		 is_charging_mode);

	return 1;
}
__setup("androidboot.mode=", latona_charger_mode_setup);

static const __initdata struct i2c_board_info latona_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("max17040", 0x36),
		.platform_data = &max17040_pdata,
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_FUEL_INT_N),
	}
};

void __init latona_power_init(void)
{
	struct platform_device *pdev;

	/* Update temperature data from board type */
	temper_table = temper_table_latona;
	temper_table_size = ARRAY_SIZE(temper_table_latona);

	/* Update oscillator information */
	omap_pm_set_osc_lp_time(15000, 1);

	pdev = platform_device_register_resndata(NULL, "pda-power", -1,
						 NULL, 0, &charger_pdata,
						 sizeof(charger_pdata));
	if (IS_ERR_OR_NULL(pdev))
		pr_err("cannot register pda-power\n");

	max17040_pdata.use_fuel_alert = !is_charging_mode;
	i2c_register_board_info(4, latona_i2c4_boardinfo, ARRAY_SIZE(latona_i2c4_boardinfo));

	if (enable_sr)
		omap_enable_smartreflex_on_init();

	omap_pm_enable_off_mode();
}
