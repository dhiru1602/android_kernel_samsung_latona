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
#define HIGH_RECOVER_TEMP_LATONA	550
#define LOW_BLOCK_TEMP_LATONA		(-30)
#define LOW_RECOVER_TEMP_LATONA		10

static int NCP15WB473_batt_table[] =
{
    /* -15C ~ 85C */

    /*0[-15] ~  14[-1]*/
    360850,342567,322720,304162,287000,
    271697,255331,241075,227712,215182,
    206463,192394,182034,172302,163156,
    /*15[0] ~ 34[19]*/
    158214,146469,138859,131694,124947,
    122259,118590,112599,106949,101621,
    95227, 91845, 87363, 83128, 79126,
    74730, 71764, 68379, 65175, 62141,
    /*35[20] ~ 74[59]*/
    59065, 56546, 53966, 51520, 49201,
    47000, 44912, 42929, 41046, 39257,
    37643, 35942, 34406, 32945, 31555,
    30334, 28972, 27773, 26630, 25542,
    24591, 23515, 22571, 21672, 20813,
    20048, 19211, 18463, 17750, 17068,
    16433, 15793, 15197, 14627, 14082,
    13539, 13060, 12582, 12124, 11685,
    /*75[60] ~ 100[85]*/
    11209, 10862, 10476, 10106,  9751,
    9328,  9083,  8770,  8469,  8180,
    7798,  7635,  7379,  7133,  6896,
    6544,  6450,  6240,  6038,  5843,
    5518,  5475,  5302,  5134,  4973,
    4674
};

static DEFINE_SPINLOCK(charge_en_lock);
static int charger_state;
static bool is_charging_mode;

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
	int ret;

	gpio_set_value(OMAP_GPIO_EN_TEMP_VDD, 1);
	ret = twl4030_get_adc_data(TEMP_ADC_CHANNEL);
	gpio_set_value(OMAP_GPIO_EN_TEMP_VDD, 0);

	return ret;
}

static bool check_charge_full(void)
{
	// HIGH : BATT_FULL || LOW : CHARGING / DISCHARGING
	return gpio_get_value(charger_gpios[GPIO_CHG_ING_N].gpio);
}

static int get_bat_temp_by_adc(int *batt_temp)
{
	int temp_adc = temp_adc_value();
	int mvolt, r;
	int temp;

	mvolt = temp_adc * 1500 / 1023;
	r = ( 100000 * mvolt ) / ( 3000 - 2 * mvolt );

	/*calculating temperature*/
	for( temp = 100; temp >= 0; temp-- )
		if( ( NCP15WB473_batt_table[temp] - r ) >= 0 )
			break;

	temp -= 15;
	temp=temp-1;

	pr_debug("%s: temp adc : %d, temp : %d\n", __func__, temp_adc, temp);
	*batt_temp = temp * 10;
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

// Latona specific battery percentage calibration
static void latona_adjust_soc(int *soc_value)
{
	int value = *soc_value;

	if(value == 100)
		value = 100;
	else if(value < 30)
		value = ((value*100*4/3) + 50)/115;
	else if(value < 76)
		value = value + 5;
	else
		value = ((value*100-7600)*8/10+50)/80+81;

	if(value > 100)
		value = 100;

	*soc_value = value;
}

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
	.adjust_soc = latona_adjust_soc,
	.get_bat_temp = get_bat_temp_by_adc,
	.high_block_temp = HIGH_BLOCK_TEMP_LATONA,
	.high_recover_temp = HIGH_RECOVER_TEMP_LATONA,
	.low_block_temp = LOW_BLOCK_TEMP_LATONA,
	.low_recover_temp = LOW_RECOVER_TEMP_LATONA,
	.fully_charged_vol = 4180000,
	.recharge_vol = 4100000,
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
