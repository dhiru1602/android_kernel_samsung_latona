/*
 * Copyright (C) 2009 Texas Instruments Inc.
 * Mikkel Christensen <mlc@ti.com>
 *
 * Modified from mach-omap2/board-zoom-debugboard.c
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
#include <linux/gpio.h>
#include <linux/serial_8250.h>
#include <linux/smsc911x.h>
#include <linux/interrupt.h>

#include <plat/gpmc.h>
#include <plat/gpmc-smsc911x.h>

#include <mach/board-latona.h>

#define LATONA_SMSC911X_CS	7
#define LATONA_SMSC911X_GPIO	158
#define LATONA_QUADUART_CS	3
#define LATONA_QUADUART_GPIO	102
#define QUART_CLK		1843200
#define DEBUG_BASE		0x08000000
#define LATONA_ETHR_START	DEBUG_BASE

static struct omap_smsc911x_platform_data latona_smsc911x_cfg = {
	.cs             = LATONA_SMSC911X_CS,
	.gpio_irq       = LATONA_SMSC911X_GPIO,
	.gpio_reset     = -EINVAL,
	.flags		= SMSC911X_USE_32BIT,
};

static inline void __init latona_init_smsc911x(void)
{
	gpmc_smsc911x_init(&latona_smsc911x_cfg);
}

static struct plat_serial8250_port serial_platform_data[] = {
	{
		.mapbase	= OMAP3_UART1_BASE,
		.irq		= OMAP_GPIO_IRQ(102),
		.flags		= UPF_BOOT_AUTOCONF|UPF_IOREMAP|UPF_SHARE_IRQ,
		.irqflags	= IRQF_SHARED | IRQF_TRIGGER_RISING,
		.iotype		= UPIO_MEM,
		.regshift	= 1,
		.uartclk	= QUART_CLK,
	}, {
		.flags		= 0
	}
};

static struct platform_device latona_debugboard_serial_device = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev			= {
		.platform_data	= serial_platform_data,
	},
};

static inline void __init latona_init_quaduart(void)
{
	int quart_cs;
	unsigned long cs_mem_base;
	int quart_gpio = 0;

	quart_cs = LATONA_QUADUART_CS;

	if (gpmc_cs_request(quart_cs, SZ_1M, &cs_mem_base) < 0) {
		printk(KERN_ERR "Failed to request GPMC mem"
				"for Quad UART(TL16CP754C)\n");
		return;
	}

	quart_gpio = LATONA_QUADUART_GPIO;

	if (gpio_request_one(quart_gpio, GPIOF_IN, "TL16CP754C GPIO") < 0)
		printk(KERN_ERR "Failed to request GPIO%d for TL16CP754C\n",
								quart_gpio);
}

static inline int latona_debugboard_detect(void)
{
	int debug_board_detect = 0;
	int ret = 1;

	debug_board_detect = LATONA_SMSC911X_GPIO;

	if (gpio_request_one(debug_board_detect, GPIOF_IN,
			     "Latona debug board detect") < 0) {
		printk(KERN_ERR "Failed to request GPIO%d for Zoom debug"
		"board detect\n", debug_board_detect);
		return 0;
	}

	if (!gpio_get_value(debug_board_detect)) {
		ret = 0;
	}
	gpio_free(debug_board_detect);
	return ret;
}

static struct platform_device *latona_devices[] __initdata = {
	&latona_debugboard_serial_device,
};

int __init latona_debugboard_init(void)
{
	if (!latona_debugboard_detect())
		return 0;

	latona_init_smsc911x();
	latona_init_quaduart();
	return platform_add_devices(latona_devices, ARRAY_SIZE(latona_devices));
}
