/*
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-zoom-display.c
 *
 * Mark "Hill Beast" Kennard <komcomputers@gmail.com>
 * Phinitnan "Crackerizer" Chanasabaeng <phinitnan_c@xtony.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/spi/spi.h>
#include <plat/mcspi.h>
#include <video/omapdss.h>
#include <plat/omap-pm.h>
#include "mux.h"

#define LATONA_PANEL_RESET_GPIO	170

static struct omap_dss_device latona_lcd_device = {
	.name			= "lcd",
	.driver_name		= "latona_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.clocks = {
		.dispc  = {
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
		},
	},
	.phy.dpi.data_lines	= 24,
	.reset_gpio = LATONA_PANEL_RESET_GPIO
};

static struct omap_dss_device *latona_dss_devices[] = {
	&latona_lcd_device
};

static struct omap_dss_board_info latona_dss_data = {
	.num_devices		= ARRAY_SIZE(latona_dss_devices),
	.devices		= latona_dss_devices,
	.default_device		= &latona_lcd_device,
};

static struct omap2_mcspi_device_config dss_lcd_mcspi_config = {
	.turbo_mode		= 1,		/* Crackerizer: Turbo is disabled 
																				in original config, testing it */
	.single_channel	= 1,  /* 0: slave, 1: master */
};

static struct spi_board_info latona_panel_spi_board_info[] __initdata = {
	[0] = {
		.modalias		= "latona_panel_spi",
		.bus_num		= 1,
		.chip_select		= 0,
		.max_speed_hz		= 375000,
		.controller_data	= &dss_lcd_mcspi_config,
	},
};

void __init latona_display_init(void)
{
	omap_display_init(&latona_dss_data);
	spi_register_board_info(latona_panel_spi_board_info,
				ARRAY_SIZE(latona_panel_spi_board_info));
}

