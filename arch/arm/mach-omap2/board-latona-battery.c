/**
 *
 *  Copyright (C) 2012 - Aditya Patange
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  SAMSUNG BATTERY DRIVER Platform code for Board-LATONA.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <mach/board-latona.h>

#define B_DEBUG 0

static struct resource samsung_charger_resources[] = {
	[0] = {
	       // USB IRQ
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE,
	       },
	[1] = {
	       // TA IRQ
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	       },
	[2] = {
	       // CHG_ING_N
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	       },
	[3] = {
	       // CHG_EN
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ,
	       },
};

static int samsung_charger_config_data[] = {
	false,
	1,
	true,
};

static int samsung_battery_config_data[] = {
	false,
	4,
	true,
	0,
};

static struct platform_device samsung_charger_device = {
	.name = "secChargerDev",
	.id = -1,
	.num_resources = ARRAY_SIZE(samsung_charger_resources),
	.resource = samsung_charger_resources,

	.dev = {
		.platform_data = &samsung_charger_config_data,
		}
};

static struct platform_device samsung_battery_device = {
	.name = "secBattMonitor",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &samsung_battery_config_data,
		}
};


static struct platform_device *battery_devices[] __initdata = {
	&samsung_battery_device,
	&samsung_charger_device,
};

void __init latona_battery_init(void)
{
#if B_DEBUG
	printk(KERN_DEBUG "[%s\n]",__func__);
#endif
	platform_add_devices(battery_devices,ARRAY_SIZE(battery_devices));

	samsung_charger_resources[0].start = 0;	    
	if (gpio_request(OMAP_GPIO_TA_NCONNECTED, "ta_nconnected irq") < 0) 
	{
		printk(KERN_ERR "Failed to request GPIO%d for ta_nconnected IRQ\n",OMAP_GPIO_TA_NCONNECTED);
		samsung_charger_resources[1].start = -1;
	} else 
	{
		samsung_charger_resources[1].start =
		gpio_to_irq(OMAP_GPIO_TA_NCONNECTED);
	}

	if (gpio_request(OMAP_GPIO_CHG_ING_N, "charge full irq") < 0) 
	{
		printk(KERN_ERR "Failed to request GPIO%d for charge full IRQ\n", OMAP_GPIO_CHG_ING_N);
		samsung_charger_resources[2].start = -1;
	} else 
	{
		samsung_charger_resources[2].start =
		gpio_to_irq(OMAP_GPIO_CHG_ING_N);
	}

	if (gpio_request(OMAP_GPIO_CHG_EN, "Charge enable gpio") < 0) 
	{
		printk(KERN_ERR "Failed to request GPIO%d for charge enable gpio\n",OMAP_GPIO_CHG_EN);
		samsung_charger_resources[3].start = -1;
	} else 
	{
		samsung_charger_resources[3].start =
		gpio_to_irq(OMAP_GPIO_CHG_EN);
	}

	samsung_charger_resources[3].start = gpio_to_irq(OMAP_GPIO_CHG_EN);
}
