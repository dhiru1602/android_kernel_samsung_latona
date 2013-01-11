/*
 * Copyright (C) 2009 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-zoom-peripherals.c
 *
 * Aditya Patange "Adi_Pat" <adithemagnificent@gmail.com>
 *
 * Mark "Hill Beast" Kennard <komcomputers@gmail.com>
 * crackerizer <github.com/crackerizer>
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
#include <linux/i2c/twl.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/wl12xx.h>
#include <linux/mmc/host.h>
#include <linux/leds.h>


#include <media/v4l2-int-device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/common.h>
#include <plat/usb.h>
#include <linux/switch.h>
#include <mach/board-latona.h>
#include <linux/irq.h>
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

/* Atmel Touchscreen */
#define OMAP_GPIO_TSP_INT 142

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

/* Samsung LEDs support */

static struct led_info sec_keyled_list[] = {
	{
	 .name = "button-backlight",
	 },
};

static struct led_platform_data sec_keyled_data = {
	.num_leds = ARRAY_SIZE(sec_keyled_list),
	.leds = sec_keyled_list,
};

static struct platform_device samsung_led_device = {
	.name = "secLedDriver",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &sec_keyled_data,
		},
};

/* End: Samsung LED's support */ 

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
static struct __initdata twl4030_power_data latona_t2scripts_data;

static struct regulator_consumer_supply latona_vmmc1_supply = {
	.supply		= "vmmc",
};

static struct regulator_consumer_supply latona_vsim_supply = {
	.supply		= "vmmc_aux",
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

/* VSIM for OMAP VDD_MMC1A (i/o for DAT4..DAT7) */
static struct regulator_init_data latona_vsim = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &latona_vsim_supply,
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
	&samsung_led_device,         /* SAMSUNG LEDs */ 
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
	latona_vsim_supply.dev = mmc[1].dev;
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

static struct i2c_board_info __initdata latona_i2c_bus2_info[] = {
#ifdef CONFIG_FSA9480_MICROUSB
	{
		I2C_BOARD_INFO("fsa9480", 0x25),
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_JACK_NINT),
	},
#endif
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
#ifdef CONFIG_SAMSUNG_BATTERY
	{
		I2C_BOARD_INFO("secFuelgaugeDev", 0x36),
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_FUEL_INT_N),
	},
#endif
#ifdef CONFIG_INPUT_YAS529
	{
		I2C_BOARD_INFO("geomagnetic", 0x2E),
	},
#endif
	{
		I2C_BOARD_INFO("Si4709_driver", 0x10),			
	},
};

static struct i2c_board_info __initdata latona_i2c_bus3_info[] = {
	{
		I2C_BOARD_INFO("qt602240_ts", 0x4A),
	},
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
	return 0;
}

static void atmel_dev_init(void)
{
	/* Set the ts_gpio pin mux */
	if (gpio_request(OMAP_GPIO_TSP_INT, "touch_atmel") < 0) {
		printk(KERN_ERR "can't get synaptics pen down GPIO\n");
		return;
	}
	gpio_direction_input(OMAP_GPIO_TSP_INT);
	
}

static void enable_board_wakeup_source(void)
{
	/* T2 interrupt line (keypad) */
	omap_mux_init_signal("sys_nirq",
		OMAP_WAKEUP_EN | OMAP_PIN_INPUT_PULLUP);
}

void __init latona_peripherals_init(void)
{
	platform_add_devices(latona_board_devices,
		ARRAY_SIZE(latona_board_devices));
	twl4030_get_scripts(&latona_t2scripts_data);
	board_onenand_init();
	omap_i2c_init();
	atmel_dev_init();
	platform_device_register(&omap_vwlan_device);
	usb_musb_init(&latona_musb_board_data);
#ifdef CONFIG_SAMSUNG_BATTERY
	latona_battery_init();
#endif 
#ifdef CONFIG_SAMSUNG_PHONE_SVNET
	latona_phone_svnet_init(); /* Initialize Phone SVNET Drivers */ 
#endif
	enable_board_wakeup_source();
	omap_serial_init();
	board_init_zeus_key(); /* ZEUS Key */ 
}
