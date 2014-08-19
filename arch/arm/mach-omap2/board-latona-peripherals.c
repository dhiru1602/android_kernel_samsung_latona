/*
 * Copyright (C) 2009 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-zoom-peripherals.c
 *
 * Dheeraj CVR "dhiru1602" <cvr.dheeraj@gmail.com>
 * Aditya Patange "Adi_Pat" <adithemagnificent@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/gpio.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/wl12xx.h>
#include <linux/mmc/host.h>
#include <linux/leds.h>
#include <media/v4l2-int-device.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/omap-serial.h>
#include <linux/switch.h>
#include <mach/board-latona.h>
#include <linux/i2c/twl4030-madc.h>
#include <linux/irq.h>
#include <linux/gp2a.h>
#include <linux/yas529.h>
#include "mux.h"
#include "hsmmc.h"
#include "common-board-devices.h"
#include "twl4030.h"
#include "control.h"

#include "../../../drivers/media/video/cam_pmic.h"
#include "../../../drivers/media/video/ce147.h"
#include "../../../drivers/media/video/s5ka3dfx.h"

struct ce147_platform_data omap_board_ce147_platform_data;
struct s5ka3dfx_platform_data omap_board_s5ka3dfx_platform_data;

#define GP2A_LIGHT_ADC_CHANNEL	4

/* ZEUS Key and Headset Switch */

static struct gpio_switch_platform_data headset_switch_data = {
	.name = "h2w",
	.gpio = OMAP_GPIO_DET_3_5,
};

static struct platform_device headset_switch_device = {
	.name = "switch-gpio",
	.dev = {
		.platform_data = &headset_switch_data,
		}
};

#ifdef CONFIG_SWITCH_SIO
struct platform_device latona_sio_switch = {
	.name = "switch-sio",
	.id = -1,
};
#endif

static struct resource board_zeus_key_resources[] = {
	[0] = {
	       .start = 0,                                             /* Power Button */ 
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	       },
	[1] = {
	       .start = 0,                                             /* Home Button */ 
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	       },
	[2] = {
	       .start = 0,                                             /* Headset Button */ 
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	       }
};

static struct platform_device board_zeus_key_device = {
	.name = "zeuskey_device",
	.id = -1,
	.num_resources = ARRAY_SIZE(board_zeus_key_resources),
	.resource = board_zeus_key_resources,
};

static inline void __init board_init_zeus_key(void)
{
	board_zeus_key_resources[0].start = gpio_to_irq(OMAP_GPIO_KEY_PWRON);
	if (gpio_request(OMAP_GPIO_KEY_PWRON, "power_key_irq") < 0) {
		printk(KERN_ERR
		       "\n FAILED TO REQUEST GPIO %d for POWER KEY IRQ \n",
		       OMAP_GPIO_KEY_PWRON);
		return;
	}
	board_zeus_key_resources[1].start = gpio_to_irq(OMAP_GPIO_KEY_HOME);
	if (gpio_request(OMAP_GPIO_KEY_HOME, "home_key_irq") < 0) {
		printk(KERN_ERR
		       "\n FAILED TO REQUEST GPIO %d for VOLDN KEY IRQ \n",
		       OMAP_GPIO_KEY_HOME);
		return;
	}
	board_zeus_key_resources[2].start = gpio_to_irq(OMAP_GPIO_EAR_SEND_END);
	if (gpio_request(OMAP_GPIO_EAR_SEND_END, "ear_key_irq") < 0) {
		printk(KERN_ERR
		       "\n FAILED TO REQUEST GPIO %d for EAR KEY IRQ \n",
		       OMAP_GPIO_EAR_SEND_END);
		return;
	}
	gpio_direction_input(OMAP_GPIO_EAR_SEND_END);
	gpio_direction_input(OMAP_GPIO_KEY_PWRON);
	gpio_direction_input(OMAP_GPIO_KEY_HOME);
}
/* End: ZEUS Key and Headset Switch */

/* Latona LEDs support */
static struct platform_device latona_led_device = {
	.name = "LatonaLedDriver",
	.id = -1,
};

/* LATONA has only Volume UP/DOWN */
static uint32_t board_keymap[] = {
	KEY(2, 1, KEY_VOLUMEUP),
	KEY(1, 1, KEY_VOLUMEDOWN),
	0
};

static struct matrix_keymap_data board_map_data = {
	.keymap			= board_keymap,
	.keymap_size		= ARRAY_SIZE(board_keymap),
};

static struct twl4030_keypad_data latona_kp_twl4030_data = {
	.keymap_data	= &board_map_data,
	.rows		= 5,
	.cols		= 6,
	.rep		= 0,
};

static struct twl4030_ins sleep_on_seq[] = {
	{MSG_BROADCAST(DEV_GRP_ALL, RES_GRP_ALL, RES_TYPE_ALL, RES_TYPE2_R0,
							RES_STATE_SLEEP), 2},
	{MSG_BROADCAST(DEV_GRP_ALL, RES_GRP_ALL, RES_TYPE_ALL, RES_TYPE2_R1,
							RES_STATE_SLEEP), 2},
	{MSG_BROADCAST(DEV_GRP_ALL, RES_GRP_ALL, RES_TYPE_ALL, RES_TYPE2_R2,
							RES_STATE_SLEEP), 2},
};

static struct twl4030_ins wakeup_p12_seq[] = {
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_ALL, RES_TYPE_ALL, RES_TYPE2_R0,
							RES_STATE_ACTIVE), 2},
};

static struct twl4030_ins wakeup_p3_seq[] = {
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_ALL, RES_TYPE_R0, RES_TYPE2_R2,
							RES_STATE_ACTIVE), 2},
};

static struct twl4030_ins wrst_seq[] = {
	{MSG_SINGULAR(DEV_GRP_NULL, RES_RESET, RES_STATE_OFF), 2},
	{MSG_SINGULAR(DEV_GRP_NULL, RES_MAIN_REF, RES_STATE_WRST), 2},
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_ALL, RES_TYPE_R0, RES_TYPE2_R2,
							RES_STATE_WRST), 2},
	{MSG_SINGULAR(DEV_GRP_NULL, RES_VUSB_3V1, RES_STATE_WRST), 2},
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_ALL, RES_TYPE_R0, RES_TYPE2_R1,
							RES_STATE_WRST), 2},
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_RC, RES_TYPE_ALL, RES_TYPE2_R0,
							RES_STATE_WRST), 2},
	{MSG_SINGULAR(DEV_GRP_NULL, RES_RESET, RES_STATE_ACTIVE), 2},
};

static struct twl4030_resconfig twl4030_rconfig[] = {
	{ .resource = RES_VAUX1, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VAUX2, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VAUX3, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VAUX4, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VMMC1, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VMMC2, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VPLL1, .devgroup = DEV_GRP_P1,
		.type = 3, .type2 = 1, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VPLL2, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VSIM, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_SLEEP },

	{ .resource = RES_VINTANA1, .devgroup = DEV_GRP_P1,
		.type = 1, .type2 = 2, .remap_sleep = RES_STATE_SLEEP },
	{ .resource = RES_VINTANA2, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 2, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VINTDIG, .devgroup = DEV_GRP_P1,
		.type = 1, .type2 = 2, .remap_sleep = RES_STATE_SLEEP },
	{ .resource = RES_VIO, .devgroup = DEV_GRP_ALL,
		.type = 2, .type2 = 2, .remap_sleep = RES_STATE_SLEEP },
	{ .resource = RES_VDD1, .devgroup = DEV_GRP_P1,
		.type = 4, .type2 = 1, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VDD2, .devgroup = DEV_GRP_P1,
		.type = 3, .type2 = 1, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VUSB_1V5, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VUSB_1V8, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_VUSB_3V1, .devgroup = DEV_GRP_P1,
		.type = 0, .type2 = 0, .remap_sleep = RES_STATE_OFF },
	{ .resource = RES_REGEN, .devgroup = DEV_GRP_ALL,
		.type = 2, .type2 = 1, .remap_sleep = RES_STATE_OFF },

	{ .resource = RES_NRES_PWRON, .devgroup = DEV_GRP_ALL,
		.type = 0, .type2 = 1, .remap_sleep = RES_STATE_SLEEP },
	{ .resource = RES_CLKEN, .devgroup = DEV_GRP_P3,
		.type = 3, .type2 = 2, .remap_sleep = RES_STATE_SLEEP },
	{ .resource = RES_SYSEN, .devgroup = DEV_GRP_ALL,
		.type = 6, .type2 = 1, .remap_sleep = RES_STATE_SLEEP },
	{ .resource = RES_HFCLKOUT, .devgroup = DEV_GRP_P3,
		.type = 0, .type2 = 2, .remap_sleep = RES_STATE_SLEEP },
	{ 0, 0},
};

static struct twl4030_script sleep_on_script = {
	.script = sleep_on_seq,
	.size = ARRAY_SIZE(sleep_on_seq),
	.flags = TWL4030_SLEEP_SCRIPT,
};

static struct twl4030_script wakeup_p12_script = {
	.script = wakeup_p12_seq,
	.size = ARRAY_SIZE(wakeup_p12_seq),
	.flags = TWL4030_WAKEUP12_SCRIPT,
};

static struct twl4030_script wakeup_p3_script = {
	.script = wakeup_p3_seq,
	.size = ARRAY_SIZE(wakeup_p3_seq),
	.flags = TWL4030_WAKEUP3_SCRIPT,
};

static struct twl4030_script wrst_script = {
	.script = wrst_seq,
	.size = ARRAY_SIZE(wrst_seq),
	.flags = TWL4030_WRST_SCRIPT,
};

static struct twl4030_script *twl4030_scripts[] = {
	&wakeup_p12_script,
	&sleep_on_script,
	&wakeup_p3_script,
	&wrst_script,
};

static struct twl4030_power_data latona_t2scripts_data = {
	.scripts = twl4030_scripts,
	.num = ARRAY_SIZE(twl4030_scripts),
	.resource_config = twl4030_rconfig,
};

static struct regulator_consumer_supply latona_vmmc1_supply = {
	.supply		= "vmmc",
};

static struct regulator_consumer_supply latona_vmmc2_supply = {
	.supply		= "vmmc",
};

static struct regulator_consumer_supply latona_vmmc3_supply = {
	.supply		= "vmmc",
	.dev_name	= "omap_hsmmc.2",
};

static struct regulator_consumer_supply latona_vaux3_supply = {
	.supply = "vaux3",
};

static struct regulator_consumer_supply latona_vaux4_supply = {
	.supply = "vaux4",
};


/* VMMC1 for OMAP VDD_MMC1 (i/o) and MMC1 card */
static struct regulator_init_data latona_vmmc1 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &latona_vmmc1_supply,
};

/* VMMC2 for MMC2 card */
static struct regulator_init_data latona_vmmc2 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 1850000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &latona_vmmc2_supply,
};

static struct regulator_init_data latona_vmmc3 = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies = &latona_vmmc3_supply,
};

static struct fixed_voltage_config latona_vwlan = {
	.supply_name		= "vwl1271",
	.microvolts		= 1800000, /* 1.8V */
	.gpio			= LATONA_WIFI_PMENA_GPIO,
	.startup_delay		= 70000, /* 70msec */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &latona_vmmc3,
};

static struct platform_device *latona_board_devices[] __initdata = {
	&headset_switch_device,
	&board_zeus_key_device,     /* ZEUS KEY */ 
	&latona_led_device,         /* SAMSUNG LEDs */ 
#ifdef CONFIG_SWITCH_SIO
	&latona_sio_switch,
#endif
};

static int adc_vs_lux_table[][2] = {
	{ 2, 8 },
	{ 5, 10 },
	{ 8, 45 },
	{ 10, 69 },
	{ 12, 79 },
	{ 22, 102 },
	{ 30, 159 },
	{ 50, 196 },
	{ 75, 200 },
	{ 110, 245 },
	{ 150, 295 },
	{ 170, 307 },
	{ 300, 326 },
	{ 460, 378 },
	{ 1000, 463 },
	{ 1500, 500 },
	{ 1700, 515 },
	{ 2000, 526 },
	{ 2850, 561 },
	{ 3450, 570 },
	{ 6000, 603 },
	{ 11000, 648 },    /* lux, adc value */
	{ 0, 0, }
};

static int gp2a_light_adc_to_lux(int adc_val)
{
	int i;
	int lux = 11000;

	for (i = 0; adc_vs_lux_table[i+1][0] > 0; i++) {
		// in case the adc value is smaller than 30 lux
		if (adc_val < adc_vs_lux_table[i][1]) {
			lux = adc_vs_lux_table[i][0] * adc_val / adc_vs_lux_table[i][1];
			break;
		}

		if (adc_val >= adc_vs_lux_table[i][1] && adc_val < adc_vs_lux_table[i+1][1]) {
			lux = (adc_vs_lux_table[i+1][0] - adc_vs_lux_table[i][0]) /
			      (adc_vs_lux_table[i+1][1] - adc_vs_lux_table[i][1]) * 
			      (adc_val - adc_vs_lux_table[i][1]) +
			      adc_vs_lux_table[i][0];
			break;
		}
	}
	return lux;
}

static int gp2a_light_adc_value(void)
{
	return gp2a_light_adc_to_lux(twl4030_get_madc_conversion(GP2A_LIGHT_ADC_CHANNEL));
}

static void gp2a_power(bool on)
{
	struct regulator *usb3v1;

	usb3v1 = regulator_get(NULL, "usb3v1");

	if (on)
		regulator_enable(usb3v1);
	else
		regulator_disable(usb3v1);

	/* this controls the power supply rail to the gp2a IC */
	gpio_set_value(OMAP_GPIO_ALS_EN, on);
}

static struct gp2a_platform_data gp2a_pdata = {
	.power = gp2a_power,
	.p_out = OMAP_GPIO_PS_VOUT,
	.light_adc_value = gp2a_light_adc_value,
};

static struct platform_device omap_vwlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data	= &latona_vwlan,
	},
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.name		= "internal",
		.mmc		= 2,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.nonremovable	= true,
		.power_saving	= true,
	},
	{
		.name		= "external",
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA,
		.gpio_wp	= -EINVAL,
		.power_saving	= true,
	},
	{
		.name		= "wl1271",
		.mmc		= 3,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.gpio_wp	= -EINVAL,
		.gpio_cd	= -EINVAL,
		.nonremovable	= true,
	},
	{}      /* Terminator */
};

static struct regulator_consumer_supply latona_vpll2_supplies[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

/* VAUX3 for LCD */
static struct regulator_init_data latona_aux3 = {
	.constraints = {
			.min_uV = 1800000,
			.max_uV = 1800000,
			.boot_on = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &latona_vaux3_supply,
};

/* VAUX4 for LCD */
static struct regulator_init_data latona_aux4 = {
	.constraints = {
			.min_uV = 2800000,
			.max_uV = 2800000,
			.boot_on = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &latona_vaux4_supply,
};

static struct regulator_consumer_supply latona_vdda_dac_supply =
	REGULATOR_SUPPLY("vdda_dac", "omapdss_venc");

static struct regulator_init_data latona_vpll2 = {
	.constraints = {
		.min_uV                 = 1800000,
		.max_uV                 = 1800000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask         = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies		= ARRAY_SIZE(latona_vpll2_supplies),
	.consumer_supplies		= latona_vpll2_supplies,
};

static struct regulator_init_data latona_vdac = {
	.constraints = {
		.min_uV                 = 1800000,
		.max_uV                 = 1800000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask         = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies		= 1,
	.consumer_supplies		= &latona_vdda_dac_supply,
};

static int latona_twl_gpio_setup(struct device *dev,
		unsigned gpio, unsigned ngpio)
{
	/* gpio + 0 is "mmc0_cd" (input/IRQ) */
	//mmc[0].gpio_cd = gpio + 0;

	mmc[1].gpio_cd = 23;

	omap2_hsmmc_init(mmc);

	/* link regulators to MMC adapters ... we "know" the
	 * regulators will be set up only *after* we return.
	*/
	latona_vmmc1_supply.dev = mmc[1].dev;
	latona_vmmc2_supply.dev = mmc[0].dev;

	return 0;
}

static int latona_batt_table[] = {
/* 0 C*/
30800, 29500, 28300, 27100,
26000, 24900, 23900, 22900, 22000, 21100, 20300, 19400, 18700, 17900,
17200, 16500, 15900, 15300, 14700, 14100, 13600, 13100, 12600, 12100,
11600, 11200, 10800, 10400, 10000, 9630,  9280,  8950,  8620,  8310,
8020,  7730,  7460,  7200,  6950,  6710,  6470,  6250,  6040,  5830,
5640,  5450,  5260,  5090,  4920,  4760,  4600,  4450,  4310,  4170,
4040,  3910,  3790,  3670,  3550
};

static struct twl4030_bci_platform_data latona_bci_data = {
	.battery_tmp_tbl	= latona_batt_table,
	.tblsize		= ARRAY_SIZE(latona_batt_table),
};

static struct twl4030_usb_data latona_usb_data = {
	.usb_mode	= T2_USB_MODE_ULPI,
};

static struct omap_musb_board_data latona_musb_board_data = {
       .interface_type         = MUSB_INTERFACE_ULPI,
       .mode                   = MUSB_PERIPHERAL,
       .power                  = 100,
};


static struct twl4030_gpio_platform_data latona_gpio_data = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= TWL4030_GPIO_IRQ_BASE,
	.irq_end	= TWL4030_GPIO_IRQ_END,
	.setup		= latona_twl_gpio_setup,
	.debounce	= 0x04,
};

static struct twl4030_madc_platform_data latona_madc_data = {
	.irq_line	= 1,
};

static struct twl4030_codec_audio_data latona_audio_data;

static struct twl4030_codec_data latona_codec_data = {
	.audio_mclk = 26000000,
	.audio = &latona_audio_data,
};

static struct twl4030_platform_data latona_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.bci		= &latona_bci_data,
	.madc		= &latona_madc_data,
	.usb		= &latona_usb_data,
	.gpio		= &latona_gpio_data,
	.keypad		= &latona_kp_twl4030_data,
	.power		= &latona_t2scripts_data,
	.codec		= &latona_codec_data,
	.vmmc1          = &latona_vmmc1,
	.vmmc2          = &latona_vmmc2,
	.vaux3          = &latona_aux3,
	.vaux4          = &latona_aux4,
	.vpll2		= &latona_vpll2,
	.vdac		= &latona_vdac,
};

static struct omap_onenand_platform_data board_onenand_data = {
	.cs		= 0,
	.gpio_irq	= 73,
	.dma_channel	= -1,
	.parts		= onenand_partitions,
	.nr_parts	= ARRAY_SIZE(onenand_partitions),
	.flags		= ONENAND_SYNC_READWRITE,
};

static void __init board_onenand_init(void)
{
	gpmc_onenand_init(&board_onenand_data);
}

static int latona_mxt_key_codes[MXT_KEYARRAY_MAX_KEYS] = {
	[0] = KEY_MENU,
	[1] = KEY_BACK,
};

/* Latona specific TSP configuration */
static u8 mxt_init_vals[] = {
	/* MXT_GEN_COMMAND(6) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_GEN_POWER(7) */
	0x40, 0xff, 0x32,
	/* MXT_GEN_ACQUIRE(8) */
	0x0a, 0x00, 0x05, 0x01, 0x00, 0x00, 0x09, 0x1b,
	/* MXT_TOUCH_MULTI(9) */
	0x8f, 0x00, 0x00, 0x12, 0x0b, 0x01, 0x10, 0x20, 0x02, 0x01,
	0x00, 0x03, 0x01, 0x2e, 0x05, 0x05, 0x28, 0x0a, 0x1f, 0x03,
	0xdf, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x12,
	/* MXT_TOUCH_KEYARRAY(15) */
	0x83, 0x10, 0x0b, 0x02, 0x01, 0x01, 0x00, 0x23, 0x04, 0x00,
	0x00,
	/* MXT_SPT_GPIOPWM(19) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_GRIPFACE(20) */
	0x13, 0x00, 0x00, 0x05, 0x05, 0x00, 0x00, 0x1e, 0x14, 0x04,
	0x0f, 0x0a,
	/* MXT_PROCG_NOISE(22) */
	0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x1b, 0x00,
	0x00, 0x1d, 0x22, 0x27, 0x31, 0x3a, 0x03,
	/* MXT_TOUCH_PROXIMITY(23) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_ONETOUCH(24) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_SELFTEST(25) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_TWOTOUCH(27) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_CTECONFIG(28) */
	0x00, 0x00, 0x02, 0x10, 0x3f, 0x3c,
};

static struct mxt_platform_data latona_mxt_platform_data = {
	.config			= mxt_init_vals,
	.config_length		= ARRAY_SIZE(mxt_init_vals),

	.x_line         = 18,
	.y_line         = 11,
	.x_size         = 800,
	.y_size         = 480,
	.blen           = 0x10,
	.threshold      = 0x20,
	.voltage        = 2800000,              /* 2.8V */
	.orient         = MXT_DIAGONAL,
	.irqflags       = IRQF_TRIGGER_FALLING,
	.key_codes	= latona_mxt_key_codes,
	.tsp_en_gpio	= OMAP_GPIO_TOUCH_EN,
};

static struct yas529_platform_data yas529_pdata = {
	.reset_line = OMAP_GPIO_MSENSE_NRST,
	.reset_asserted = 0,
};

static struct i2c_board_info __initdata latona_i2c_bus2_info[] = {
#if defined(CONFIG_SND_SOC_MAX97000)
	{
		I2C_BOARD_INFO("max97000", 0x4d),
	},
#endif
	{
		I2C_BOARD_INFO(CE147_DRIVER_NAME, CE147_I2C_ADDR),
		.platform_data = &omap_board_ce147_platform_data,
	},
	{
		I2C_BOARD_INFO(S5KA3DFX_DRIVER_NAME, S5KA3DFX_I2C_ADDR),
		.platform_data = &omap_board_s5ka3dfx_platform_data,
	},
	{
		I2C_BOARD_INFO("cam_pmic", CAM_PMIC_I2C_ADDR),
	},
	{
		I2C_BOARD_INFO("Yas529Geomag", 0x2E),
		.platform_data = &yas529_pdata,
	},
	{
		I2C_BOARD_INFO("gp2a", 0x44),
		.platform_data = &gp2a_pdata,
	},
};

static struct i2c_board_info __initdata latona_i2c_bus3_info[] = {
	{
		I2C_BOARD_INFO("atmel_mxt_ts", 0x4A),
		.platform_data = &latona_mxt_platform_data,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_TOUCH_INT),
	},
};

static struct i2c_gpio_platform_data latona_gpio_i2c4_pdata = {
	.sda_pin = OMAP_GPIO_FUEL_SDA,
	.scl_pin = OMAP_GPIO_FUEL_SCL,
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device latona_gpio_i2c4_device = {
	.name	= "i2c-gpio",
	.id	= 4,
	.dev = {
		.platform_data = &latona_gpio_i2c4_pdata,
	},
};

static __initdata struct i2c_board_info latona_i2c5_boardinfo[] = {
	{
		I2C_BOARD_INFO("YamahaBMA222", 0x08),
	},
};

static struct i2c_gpio_platform_data latona_gpio_i2c5_pdata = {
	.sda_pin = OMAP_GPIO_SENSOR_SDA,
	.scl_pin = OMAP_GPIO_SENSOR_SCL,
	.udelay = 5,
	.timeout = 0,
};

static struct platform_device latona_gpio_i2c5_device = {
	.name	= "i2c-gpio",
	.id	= 5,
	.dev = {
		.platform_data = &latona_gpio_i2c5_pdata,
	},
};

static __initdata struct i2c_board_info latona_i2c6_boardinfo[] = {
	{
		I2C_BOARD_INFO("Si4709_driver", 0x10),
	},
};

static struct i2c_gpio_platform_data latona_gpio_i2c6_pdata = {
	.sda_pin = OMAP_GPIO_FM_SDA,
	.scl_pin = OMAP_GPIO_FM_SCL,
	.udelay = 5,
	.timeout = 0,
};

static struct platform_device latona_gpio_i2c6_device = {
	.name	= "i2c-gpio",
	.id	= 6,
	.dev = {
		.platform_data = &latona_gpio_i2c6_pdata,
	},
};

static struct platform_device *latona_i2c_gpio_devices[] __initdata = {
	&latona_gpio_i2c4_device, // Fuel Guage MAX17040
	&latona_gpio_i2c5_device, // Yamaha BMA222
	&latona_gpio_i2c6_device, // Si4709 FM Radio
};

static int __init omap_i2c_init(void)
{
         /* Disable OMAP 3630 internal pull-ups for I2Ci */
	if (cpu_is_omap3630()) {

		u32 prog_io;

		prog_io = omap_ctrl_readl(OMAP343X_CONTROL_PROG_IO1);
		/* Program (bit 19)=1 to disable internal pull-up on I2C1 */
		prog_io |= OMAP3630_PRG_I2C1_PULLUPRESX;
		/* Program (bit 0)=1 to disable internal pull-up on I2C2 */
		prog_io |= OMAP3630_PRG_I2C2_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP343X_CONTROL_PROG_IO1);

		prog_io = omap_ctrl_readl(OMAP36XX_CONTROL_PROG_IO2);
		/* Program (bit 7)=1 to disable internal pull-up on I2C3 */
		prog_io |= OMAP3630_PRG_I2C3_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP36XX_CONTROL_PROG_IO2);

		prog_io = omap_ctrl_readl(OMAP36XX_CONTROL_PROG_IO_WKUP1);
		/* Program (bit 5)=1 to disable internal pull-up on I2C4(SR) */
		prog_io |= OMAP3630_PRG_SR_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP36XX_CONTROL_PROG_IO_WKUP1);
	}

	omap_register_i2c_bus(2, 400, latona_i2c_bus2_info,
			ARRAY_SIZE(latona_i2c_bus2_info));
	omap_pmic_init(1, 400, "twl5030", INT_34XX_SYS_NIRQ, &latona_twldata);	
	omap_register_i2c_bus(3, 400, latona_i2c_bus3_info,
			ARRAY_SIZE(latona_i2c_bus3_info));

	i2c_register_board_info(5, latona_i2c5_boardinfo, ARRAY_SIZE(latona_i2c5_boardinfo));
	i2c_register_board_info(6, latona_i2c6_boardinfo, ARRAY_SIZE(latona_i2c6_boardinfo));

	return 0;
}

static void atmel_dev_init(void)
{
	/* Set the ts_gpio pin mux */
	if (gpio_request(OMAP_GPIO_TOUCH_INT, "touch_atmel") < 0) {
		printk(KERN_ERR "can't get synaptics pen down GPIO\n");
		return;
	}
	gpio_direction_input(OMAP_GPIO_TOUCH_INT);
	
}

static void enable_board_wakeup_source(void)
{
	/* T2 interrupt line (keypad) */
	omap_mux_init_signal("sys_nirq",
		OMAP_WAKEUP_EN | OMAP_PIN_INPUT_PULLUP);
}

static struct omap_device_pad latona_uart3_pads[] __initdata = {
	{
		.name	= "uart3_tx_irtx.uart3_tx_irtx",
		.enable	= OMAP_PIN_OUTPUT | OMAP_MUX_MODE0,
	},
	{
		.name	= "uart3_rx_irrx.uart3_rx_irrx",
		.flags	= OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle	= OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static inline void __init latona_serial_init(void)
{
	omap_serial_init_port_pads(0, NULL, 0, NULL);
	omap_serial_init_port_pads(1, NULL, 0, NULL);
	omap_serial_init_port_pads(2, latona_uart3_pads, ARRAY_SIZE(latona_uart3_pads), NULL);
}

void __init latona_peripherals_init(void)
{
	platform_add_devices(latona_board_devices,
		ARRAY_SIZE(latona_board_devices));
	board_onenand_init();
	latona_power_init();
	latona_connector_init();
	platform_add_devices(latona_i2c_gpio_devices,
		ARRAY_SIZE(latona_i2c_gpio_devices));
	atmel_dev_init();
	omap_i2c_init();
	platform_device_register(&omap_vwlan_device);
	usb_musb_init(&latona_musb_board_data);
#ifdef CONFIG_SAMSUNG_PHONE_SVNET
	latona_phone_svnet_init(); /* Initialize Phone SVNET Drivers */ 
#endif
	enable_board_wakeup_source();
	latona_serial_init();
	board_init_zeus_key(); /* ZEUS Key */ 
}
